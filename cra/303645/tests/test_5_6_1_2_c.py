"""Live ETSI 5.6-1-2 Unit c initialized LAN logical-interface scan."""

from __future__ import annotations

import os
import re
import secrets
import select
import shutil
import socket
import subprocess
import sys
import time
from dataclasses import dataclass, replace
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Protocol, TextIO, cast

import requests
from Crypto.PublicKey import ECC

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import GatewayAuthenticationModeError, InvalidSetup
from lib.evidence import AssertionEvidence, EvidenceLog, format_utc, utc_now
from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult
from lib.netscan import (
    DatagramSocket,
    MdnsAnswer,
    NmapInfo,
    PortObservation,
    ResolvedHost,
    Resolver,
    ScanError,
    ScanResult,
    check_nmap,
    find_nmap,
    new_datagram_socket,
    probe_mdns,
    resolve_host,
    run_tcp_connect_scan,
    run_udp_scan,
    system_resolver,
)

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.6-1-2, Test Unit c"
PORTS: str = "1-65535"
NMAP_MIN_VERSION: tuple[int, int] = (7, 40)
TCP_HOST_TIMEOUT: int = 600
UDP_HOST_TIMEOUT: int = 1800
MAX_RETRIES: int = 1
TARGETED_RETRIES: int = 2
FULL_SCAN_DELAY_MS: int = 5
MDNS_TIMEOUT: float = 3.0
TCPDUMP_START_TIMEOUT: float = 10.0
NTP_REQUEST_TIMEOUT: float = 25.0
NTP_CAPTURE_FILTER: str = "udp dst port 123 and udp[4:2] = 56 and (udp[8] & 7) = 3"
TOTAL_STEPS: int = 12
DEFAULT_FIELDS: tuple[str, ...] = AUTHENTICATION_DEFAULT_FIELDS + (
    "use_http", "http_url", "http_data_format", "http_auth", "use_mqtt", "mqtt_data_format", "remote_cfg_use",
    "ntp_use", "ntp_use_dhcp", "ntp_server1", "ntp_server2", "ntp_server3", "ntp_server4",
    "scan_default", "scan_coded_phy", "scan_1mbit_phy", "scan_2mbit_phy",
    "scan_channel_37", "scan_channel_38", "scan_channel_39",
)
HOTSPOT_PORTS: frozenset[int] = frozenset({53, 67, 68})
CHECKS: tuple[str, ...] = (
    "factory-default preflight", "IXIT declaration prerequisite", "documented HTTP listener", "TCP undocumented listeners",
    "TCP coverage", "UDP undocumented listeners", "hotspot servers disabled",
    "custom telemetry disabled", "NTP source identification", "documented mDNS responder",
    "UDP coverage", "NTP configuration restoration",
)
RAW_PRIVILEGE_HELP: str = (
    "The full UDP scan requires nmap with root/CAP_NET_RAW. "
    "Run sudo .venv/bin/python test_5_6_1_2_c.py."
)


@dataclass(frozen=True)
class ExpectedInterface:
    ixit_28: str
    protocol: str
    ports: tuple[int, ...]
    status: str
    role: str


EXPECTED_INTERFACES: tuple[ExpectedInterface, ...] = (
    ExpectedInterface("LogIntf-HTTP-Server", "tcp", (80,), "Enabled", "inbound"),
    ExpectedInterface("LogIntf-mDNS-Responder", "udp", (5353,), "Enabled", "inbound"),
    ExpectedInterface("LogIntf-Cloud-HTTPS-Telemetry", "tcp", (443,), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-Cloud-HTTPS-Status", "tcp", (443,), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-FW-Update-Client", "tcp", (443,), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-Network-DNS-Client", "udp", (53,), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-Network-DHCP-Client", "udp", (67, 68), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-Time-NTP-Client", "udp", (123,), "Enabled", "outbound"),
    ExpectedInterface("LogIntf-Hotspot-DNS-Server", "udp", (53,), "Transient", "hotspot-only"),
    ExpectedInterface("LogIntf-Hotspot-DHCP-Server", "udp", (67, 68), "Transient", "hotspot-only"),
    ExpectedInterface("LogIntf-WiFi-WPS-Client", "802.11", (), "Transient", "management"),
    ExpectedInterface("LogIntf-Custom-HTTP-Telemetry", "tcp", (), "Disabled", "outbound"),
    ExpectedInterface("LogIntf-Custom-Stream-Telemetry", "tcp", (), "Disabled", "outbound"),
    ExpectedInterface("LogIntf-USB-UART-Log-Stream", "serial", (), "Enabled", "physical"),
    ExpectedInterface("LogIntf-USB-Boot-Flasher", "rom", (), "Enabled", "physical"),
    ExpectedInterface("LogIntf-Internal-SWD-Bus", "swd", (), "Enabled", "internal"),
)


def validate_interface_declarations(directory: Path) -> None:
    """Reject the pending mDNS model until the authoritative IXIT declares it."""
    text: str = (directory / "ixit_28-LogIntf.md").read_text(encoding="utf-8")
    if re.search(r"^### \*\*ID\*\*: LogIntf-mDNS-Responder\s*$", text, re.MULTILINE) is None:
        raise InvalidSetup("IXIT documentation gap: ixit_28-LogIntf.md must declare LogIntf-mDNS-Responder")


@dataclass(frozen=True)
class Finding:
    protocol: str
    port: int
    detail: str
    initial: str
    confirmation: str


@dataclass(frozen=True)
class NtpCaptureResult:
    source_port: int
    stdout: str
    stderr: str


class NtpCapture(Protocol):
    def start(self, dut_ip: str, host_ip: str) -> tuple[str, ...]: ...
    def collect(self, timeout: float) -> NtpCaptureResult: ...
    def stop(self) -> None: ...


def local_ipv4_for(dut_ip: str) -> str:
    route_socket: socket.socket
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route_socket:
        route_socket.connect((dut_ip, 123))
        return str(route_socket.getsockname()[0])


def parse_ntp_source_port(output: str, dut_ip: str, host_ip: str) -> int:
    pattern: str = rf"\b{re.escape(dut_ip)}\.(\d+) > {re.escape(host_ip)}\.123:"
    match: re.Match[str] | None = re.search(pattern, output)
    if match is None:
        raise ScanError(f"tcpdump did not report a gateway NTP request: {output.strip()}")
    port: int = int(match.group(1))
    if not 49152 <= port <= 65535:
        raise ScanError(f"tcpdump reported unexpected NTP source port {port}")
    return port


class TcpdumpNtpCapture:
    def __init__(self) -> None:
        self.process: subprocess.Popen[str] | None = None
        self.dut_ip: str = ""
        self.host_ip: str = ""
        self.startup_stderr: str = ""

    def start(self, dut_ip: str, host_ip: str) -> tuple[str, ...]:
        path: str | None = shutil.which("tcpdump")
        if path is None:
            raise ScanError("tcpdump is required for NTP source identification")
        self.dut_ip = dut_ip
        self.host_ip = host_ip
        capture_filter: str = f"src host {dut_ip} and dst host {host_ip} and {NTP_CAPTURE_FILTER}"
        command: list[str] = [path, "-i", "any", "-nn", "-l", "-c", "1", "-s", "0", capture_filter]
        process: subprocess.Popen[str] = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        self.process = process
        try:
            if process.stderr is None:
                raise ScanError("tcpdump stderr is unavailable")
            stderr: TextIO = cast(TextIO, process.stderr)
            deadline: float = time.monotonic() + TCPDUMP_START_TIMEOUT
            startup: bytes = b""
            while True:
                remaining: float = deadline - time.monotonic()
                if remaining <= 0 or not select.select([stderr], [], [], remaining)[0]:
                    raise ScanError("tcpdump did not become ready before the NTP configuration change")
                chunk: bytes = os.read(stderr.fileno(), 4096)
                startup += chunk
                if b"listening on" in startup:
                    self.startup_stderr = startup.decode("utf-8", errors="replace")
                    return tuple(command)
                if not chunk:
                    raise ScanError(f"tcpdump exited before listening: {startup.decode('utf-8', errors='replace')}")
        except Exception:
            self.stop()
            raise

    def collect(self, timeout: float) -> NtpCaptureResult:
        process: subprocess.Popen[str] | None = self.process
        if process is None:
            raise ScanError("tcpdump was not started")
        try:
            stdout: str
            stderr: str
            stdout, stderr = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            error: subprocess.TimeoutExpired
            raise ScanError("no NTP client request reached the test host before timeout") from error
        if process.returncode != 0:
            raise ScanError(f"tcpdump exited {process.returncode}: {stderr.strip()}")
        port: int = parse_ntp_source_port(stdout, self.dut_ip, self.host_ip)
        return NtpCaptureResult(port, stdout, self.startup_stderr + stderr)

    def stop(self) -> None:
        process: subprocess.Popen[str] | None = self.process
        self.process = None
        if process is not None:
            if process.poll() is None:
                process.terminate()
            try:
                process.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


def same_public_value(actual: Any, expected: Any) -> bool:
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return actual.keys() == expected.keys() and all(
            same_public_value(actual[key], value) for key, value in expected.items()
        )
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(
            same_public_value(actual_item, expected_item) for actual_item, expected_item in zip(actual, expected)
        )
    return actual == expected


class RecoveryEvidence(EvidenceLog):
    """Abort ordinary acquisition on reporting errors; never let them block recovery."""

    def __init__(self, log: EvidenceLog) -> None:
        self.log: EvidenceLog = log
        super().__init__(log.path, log._stream, log.started_at)
        self.recovering: bool = False
        self.failed: bool = False

    def attempt(self, operation: Callable[[], None]) -> None:
        try:
            operation()
        except Exception:  # Reporting failures are re-raised until recovery begins.
            self.failed = True
            if not self.recovering:
                raise

    def write(self, label: str, value: Any = "") -> None:
        self.attempt(lambda: self.log.write(label, value))

    def write_line(self, message: str) -> None:
        self.attempt(lambda: self.log.write_line(message))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.attempt(lambda: self.log.write_http_request(request))

    def write_http_response(self, response: requests.Response) -> None:
        self.attempt(lambda: self.log.write_http_response(response))

    def exception(self, error: BaseException) -> None:
        self.attempt(lambda: self.log.exception(error))

    def finish(self, verdict: str, now: Callable[[], datetime] = utc_now) -> None:
        try:
            self.attempt(lambda: self.log.finish(verdict, now))
        finally:
            self.attempt(self._stream.close)


class FunctionalTest_5_6_1_2_c:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog,
        *, session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
        resolver: Resolver = system_resolver,
        socket_factory: Callable[..., DatagramSocket] = new_datagram_socket,
        find_executable: Callable[[str], str | None] = find_nmap,
        local_ip: Callable[[str], str] = local_ipv4_for,
        capture_factory: Callable[[], NtpCapture] = TcpdumpNtpCapture,
        privilege: Callable[[], bool] | None = None,
        clock: Callable[[], float] | None = None,
        ixit_directory: Path | None = None,
        progress: Callable[[str], None] | None = None,
        output: Callable[[str], None] | None = None,
    ) -> None:
        self.ixit_directory: Path = Path(__file__).resolve().parent.parent if ixit_directory is None else ixit_directory
        self.config: DutConfig = config
        self.evidence: RecoveryEvidence = evidence if isinstance(evidence, RecoveryEvidence) else RecoveryEvidence(evidence)
        self.gateway: GatewayClient
        self.gateway_factory: Callable[[DutConfig], GatewayClient] = lambda endpoint: GatewayClient(
            endpoint, self.evidence, session_factory=session_factory,
            random_bytes=random_bytes, ecc_generate=ecc_generate,
        )
        self.runner: Callable[..., subprocess.CompletedProcess[str]] = runner
        self.resolver: Resolver = resolver
        self.socket_factory: Callable[..., DatagramSocket] = socket_factory
        self.find_executable: Callable[[str], str | None] = find_executable
        self.local_ip: Callable[[str], str] = local_ip
        self.capture_factory: Callable[[], NtpCapture] = capture_factory
        self.privilege: Callable[[], bool] | None = privilege
        self.clock: Callable[[], float] | None = clock
        self.progress: Callable[[str], None] = progress if progress is not None else lambda message: None
        self.output: Callable[[str], None] = output if output is not None else lambda message: None
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.findings: list[Finding] = []
        self.factory_reset_required: bool = False
        self.mdns_confirmed: bool = False
        self.baseline_config: dict[str, Any] | None = None
        self.original_ntp_server1: str | None = None
        self.auth_session: requests.Session | None = None

    def _say(self, message: str) -> None:
        self.evidence.attempt(lambda: self.output(message))
        self.evidence.write_line(message)

    def _assert_setup(self, description: str, passed: bool, actual: Any) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if passed else "ERROR", actual))
        if not passed:
            raise InvalidSetup(description)

    def _preflight_gateway(self) -> None:
        self.outcomes["factory-default preflight"] = "ERROR"
        try:
            login: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            self.factory_reset_required = True
            raise
        if login.login_response.status_code != HttpStatus.C_200_OK:
            self.factory_reset_required = True
        self._assert_setup("default Admin/gw_id login returns 200",
                           login.login_response.status_code == HttpStatus.C_200_OK,
                           login.login_response.status_code)
        auth_type: Any = login.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        response: requests.Response = self.gateway.request(login.session, HttpMethod.GET, GatewayApi.CONFIG)
        self._assert_setup("authenticated GET /ruuvi.json returns 200",
                           response.status_code == HttpStatus.C_200_OK, response.status_code)
        payload: dict[str, Any] = self.gateway.response_json(response, GatewayApi.CONFIG, dict)
        verified_identity: bool = False
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._assert_setup("gw_mac is a six-octet MAC",
                               isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None, mac)
            self._assert_setup("DUT gw_mac matches .env",
                               mac.replace(":", "").upper() == self.config.gw_mac.replace(":", "").upper(), mac)
            verified_identity = True
        if auth_type != GatewayCfgLanAuthType.DEFAULT and verified_identity:
            self.factory_reset_required = True
        self._assert_setup("interactive challenge reports default authentication",
                           auth_type == GatewayCfgLanAuthType.DEFAULT, auth_type)
        defaults: dict[str, Any] = default_config_values(DEFAULT_FIELDS)
        field: str
        expected: Any
        for field, expected in defaults.items():
            actual: Any = payload.get(field)
            matches: bool = field in payload and same_public_value(actual, expected)
            if not matches and verified_identity:
                self.factory_reset_required = True
            self._assert_setup(f"{field} has its public factory-default value", matches, actual)
        # Partial POSTs reset omitted MQTT fields, even when MQTT is disabled.
        self._assert_setup("verified MAC is required before partial configuration writes", verified_identity, "identity presence")
        verified_mac: str = cast(str, payload[GatewayCfgDesc.GW_MAC]).upper()
        mqtt_defaults: dict[str, str] = {
            "mqtt_prefix": f"ruuvi/{verified_mac}/",
            "mqtt_client_id": verified_mac,
        }
        for field, expected in mqtt_defaults.items():
            matches = field in payload and same_public_value(payload[field], expected)
            if not matches:
                self.factory_reset_required = True
            self._assert_setup(f"{field} matches the device-specific default before a partial POST",
                               matches, "default match" if matches else "unsafe baseline")
        second: dict[str, Any] = self._read_config(login.session)
        self._assert_setup("two stable authenticated configuration reads",
                           same_public_value(second, payload), "equal" if same_public_value(second, payload) else "different")
        self.baseline_config = payload
        self.original_ntp_server1 = payload["ntp_server1"]
        self.auth_session = login.session
        self.evidence.write("FULL CONFIGURATION BASELINE", payload)
        self.outcomes["factory-default preflight"] = "PASS"

    def _read_config(self, session: requests.Session) -> dict[str, Any]:
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        if response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"authenticated GET /ruuvi.json returned {response.status_code}")
        return self.gateway.response_json(response, GatewayApi.CONFIG, dict)

    def _set_ntp_server(self, session: requests.Session, value: str) -> None:
        response: requests.Response = self.gateway.request(
            session, HttpMethod.POST, GatewayApi.CONFIG, json_body={"ntp_server1": value},
        )
        if response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"POST /ruuvi.json ntp_server1 returned {response.status_code}")

    def _verify_changed_config(self, session: requests.Session, host_ip: str) -> None:
        baseline: dict[str, Any] | None = self.baseline_config
        if baseline is None:
            raise ScanError("baseline configuration was not saved")
        actual: dict[str, Any] = self._read_config(session)
        expected: dict[str, Any] = dict(baseline)
        expected["ntp_server1"] = host_ip
        if not same_public_value(actual, expected):
            raise ScanError("NTP change read-back differs from the expected single-field configuration")
        self.evidence.write("NTP CHANGE VERIFIED", {"ntp_server1": host_ip, "other_fields_unchanged": True})

    def _restore_ntp_server(self) -> None:
        baseline: dict[str, Any] | None = self.baseline_config
        original: str | None = self.original_ntp_server1
        if baseline is None or original is None:
            raise ScanError("baseline NTP configuration was not saved")
        login: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        if login.login_response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"restoration login returned {login.login_response.status_code}")
        post_error: Exception | None = None
        try:
            self._set_ntp_server(login.session, original)
        except Exception as error:  # noqa: BLE001 - Read back even if POST applied but its response was lost.
            error: Exception
            post_error = error
            self.evidence.exception(error)
        actual: dict[str, Any] = self._read_config(login.session)
        if not same_public_value(actual, baseline):
            raise ScanError("NTP restoration failed: final /ruuvi.json differs from the full baseline") from post_error
        if post_error is not None:
            self.evidence.write("NTP RESTORATION", "POST reported an error; full GET confirms baseline")
        self.evidence.write("NTP RESTORATION", {"ntp_server1": original, "full_baseline_verified": True})

    def _logged_runner(self, command: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        self.evidence.write("NMAP COMMAND", command)
        try:
            result: subprocess.CompletedProcess[str] = self.runner(command, **kwargs)
        except Exception as error:
            error: Exception
            self._log_raw_xml(command)
            self.evidence.exception(error)
            raise
        self.evidence.write("NMAP PROCESS", {
            "returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr,
        })
        self._log_raw_xml(command)
        return result

    def _log_raw_xml(self, command: list[str]) -> None:
        if "-oX" not in command:
            return
        path: Path = Path(command[command.index("-oX") + 1])
        try:
            content: str = path.read_text(encoding="utf-8", errors="backslashreplace")
        except OSError:
            return
        self.evidence.write("NMAP RAW XML BEFORE PARSING", content)

    def _record_scan(self, label: str, result: ScanResult) -> None:
        self.evidence.write(f"{label} RAW NMAP XML", result.raw_xml)
        self.evidence.write(f"{label} SCAN RESULT", {
            "open": sorted(result.open_ports), "closed_count": result.closed_count,
            "filtered_count": result.filtered_count, "open_filtered_count": result.open_filtered_count,
            "closed_filtered_count": result.closed_filtered_count,
            "closed": sorted(result.closed_ports) if len(result.closed_ports) <= 256 else "see raw XML",
            "filtered": sorted(result.filtered_ports) if len(result.filtered_ports) <= 256 else "see raw XML",
            "open_filtered": sorted(result.open_filtered) if len(result.open_filtered) <= 256 else "see raw XML",
            "closed_filtered": sorted(result.closed_filtered) if len(result.closed_filtered) <= 256 else "see raw XML",
            "observations": result.observations, "elapsed_seconds": result.elapsed,
        })

    def _confirmation(self, ip: str, port: int, info: NmapInfo, protocol: str) -> ScanResult:
        kwargs: dict[str, Any] = {
            "nmap_path": info.path, "runner": self._logged_runner,
            "host_timeout": 60, "max_retries": TARGETED_RETRIES,
        }
        if self.clock is not None:
            kwargs["clock"] = self.clock
        if protocol == "tcp":
            result: ScanResult = run_tcp_connect_scan(ip, f"{port:d}", **kwargs)
        else:
            result = run_udp_scan(ip, f"{port:d}", has_raw_privilege=info.has_raw_privilege, **kwargs)
        self._record_scan(f"{protocol.upper()} CONFIRMATION {port}", result)
        return result

    def _compare_tcp(self, ip: str, info: NmapInfo, result: ScanResult) -> None:
        declared: frozenset[int] = frozenset(
            port for entry in EXPECTED_INTERFACES if entry.protocol == "tcp"
            and entry.status == "Enabled" and entry.role == "inbound" for port in entry.ports
        )
        http: PortObservation | None = next(
            (item for item in result.observations if item.port == 80 and item.state == "open"), None,
        )
        http_ok: bool = 80 in declared and http is not None and http.service == "http"
        self.outcomes["documented HTTP listener"] = (
            "PASS" if http_ok else "FAIL" if http is not None or 80 in result.closed_ports else "ERROR"
        )
        self.evidence.write("ASSERTION", AssertionEvidence("80/tcp is open and HTTP-fingerprinted",
                            self.outcomes["documented HTTP listener"], http))
        extra: list[int] = sorted(result.open_ports - declared)
        self.outcomes["TCP undocumented listeners"] = "ERROR"
        port: int
        for port in extra:
            confirmation: ScanResult = self._confirmation(ip, port, info, "tcp")
            if port not in confirmation.open_ports:
                raise ScanError(f"unstable TCP finding on {port}: independent probe did not confirm open")
            self.findings.append(Finding("tcp", port, "no enabled IXIT 28 inbound entry", "open", "open"))
            self.outcomes["TCP undocumented listeners"] = "FAIL"
            self.outcomes["custom telemetry disabled"] = "FAIL"
        self.outcomes["TCP undocumented listeners"] = "FAIL" if extra else "PASS"
        self.outcomes["TCP coverage"] = "ERROR" if result.filtered_count else "PASS"
        self.outcomes["custom telemetry disabled"] = "FAIL" if extra else "PASS"
        self.evidence.write("ASSERTION", AssertionEvidence("no custom telemetry or other undeclared TCP listener",
                            self.outcomes["custom telemetry disabled"], extra))

    def _compare_udp(self, ip: str, info: NmapInfo, result: ScanResult, ntp_source_port: int) -> None:
        declared: frozenset[int] = frozenset(
            port for entry in EXPECTED_INTERFACES if entry.protocol == "udp"
            and entry.status == "Enabled" and entry.role == "inbound" for port in entry.ports
        )
        self.outcomes["UDP undocumented listeners"] = "ERROR"
        self.outcomes["hotspot servers disabled"] = "ERROR"
        extra: list[int] = sorted(result.open_ports - declared)
        port: int
        for port in extra:
            confirmation: ScanResult = self._confirmation(ip, port, info, "udp")
            if port not in confirmation.open_ports:
                raise ScanError(f"unstable UDP finding on {port}: independent probe did not confirm open")
            detail: str = "transient hotspot server active" if port in HOTSPOT_PORTS else "no enabled IXIT 28 server entry"
            self.findings.append(Finding("udp", port, detail, "open", "open"))
            self.outcomes["UDP undocumented listeners"] = "FAIL"
            if port in HOTSPOT_PORTS:
                self.outcomes["hotspot servers disabled"] = "FAIL"
        self.outcomes["UDP undocumented listeners"] = "FAIL" if extra else "PASS"
        hotspot_states: list[tuple[int, str]] = []
        independently_resolved: set[int] = {5353} if self.mdns_confirmed else set()
        for port in sorted(HOTSPOT_PORTS):
            if port in result.open_ports:
                hotspot_states.append((port, "open"))
                continue
            if port in result.closed_ports:
                hotspot_states.append((port, "closed"))
                continue
            targeted: ScanResult = self._confirmation(ip, port, info, "udp")
            if port in targeted.open_ports:
                independently_resolved.add(port)
                self.findings.append(Finding("udp", port, "transient hotspot server active", "indeterminate", "open"))
                hotspot_states.append((port, "open"))
                self.outcomes["UDP undocumented listeners"] = "FAIL"
                self.outcomes["hotspot servers disabled"] = "FAIL"
            elif port in targeted.closed_ports:
                independently_resolved.add(port)
                hotspot_states.append((port, "closed"))
            else:
                hotspot_states.append((port, "indeterminate"))
        states: tuple[str, ...] = tuple(state for _, state in hotspot_states)
        self.outcomes["hotspot servers disabled"] = (
            "FAIL" if "open" in states else "ERROR" if "indeterminate" in states else "PASS"
        )
        self.evidence.write("ASSERTION", AssertionEvidence("hotspot DNS/DHCP servers on UDP 53/67/68 inactive",
                            self.outcomes["hotspot servers disabled"], hotspot_states))
        unresolved_open_filtered: frozenset[int] = result.open_filtered - independently_resolved
        unresolved_filtered_count: int = result.filtered_count - len(result.filtered_ports & independently_resolved)
        exact_sntp_only: bool = (
            result.closed_filtered == frozenset({ntp_source_port})
            and result.closed_filtered_count == 1
            and not unresolved_open_filtered
            and unresolved_filtered_count == 0
        )
        self.outcomes["UDP coverage"] = "PASS" if exact_sntp_only else "ERROR"
        comparison: dict[str, Any] = {
            "captured_sntp_source_port": ntp_source_port,
            "closed_filtered_ports": sorted(result.closed_filtered),
            "closed_filtered_count": result.closed_filtered_count,
            "open_filtered_ports": sorted(result.open_filtered),
            "open_filtered_count": result.open_filtered_count,
            "filtered_count": result.filtered_count,
            "independently_resolved": sorted(independently_resolved),
            "unresolved_open_filtered": sorted(unresolved_open_filtered),
            "unresolved_filtered_count": unresolved_filtered_count,
            "only_sntp_source_is_closed_filtered": exact_sntp_only,
        }
        self.evidence.write("SNTP UDP COVERAGE COMPARISON", comparison)
        self.evidence.write("ASSERTION", AssertionEvidence(
            "captured SNTP source is the sole closed|filtered UDP port", self.outcomes["UDP coverage"], comparison,
        ))
        if not exact_sntp_only:
            self._say(
                f"UDP coverage ERROR: expected only captured SNTP source {ntp_source_port}/udp "
                f"as closed|filtered; observed {result.closed_filtered_count} closed|filtered, "
                f"{len(unresolved_open_filtered)} unresolved open|filtered, "
                f"{unresolved_filtered_count} unresolved filtered"
            )

    def _compare_mdns(self, ip: str, udp: ScanResult) -> None:
        self.outcomes["documented mDNS responder"] = "ERROR"
        self.evidence.write("MDNS REQUEST", {"ip": ip, "port": 5353,
                                              "query": "PTR _services._dns-sd._udp.local", "timeout": MDNS_TIMEOUT})
        answer: MdnsAnswer | None = probe_mdns(ip, timeout=MDNS_TIMEOUT, socket_factory=self.socket_factory)
        self.evidence.write("MDNS RESPONSE", answer if answer is not None else "no answer")
        if answer is not None:
            if 5353 not in udp.closed_ports:
                self.outcomes["documented mDNS responder"] = "PASS"
                self.mdns_confirmed = True
        elif 5353 in udp.closed_ports:
            self.outcomes["documented mDNS responder"] = "FAIL"
        self.evidence.write("ASSERTION", AssertionEvidence("LogIntf-mDNS-Responder answers _http._tcp",
                            self.outcomes["documented mDNS responder"], answer))

    def run(self) -> RunResult:
        verdict: str = "PASS"
        recovery: str | None = None
        capture: NtpCapture | None = None
        mutation_possible: bool = False
        try:
            self.progress("Resolving DUT LAN IPv4 address")
            resolved: ResolvedHost = resolve_host(self.config.gw_hostname, resolver=self.resolver)
            self.evidence.write("RESOLVED HOST", resolved)
            # Keep HTTP identity, writes and recovery on the exact scanned IPv4 endpoint.
            endpoint: DutConfig = replace(self.config, gw_hostname=resolved.ip)
            self.gateway = self.gateway_factory(endpoint)
            self.evidence.write("PINNED HTTP ENDPOINT", {
                "configured_hostname": self.config.gw_hostname, "base_url": endpoint.base_url,
            })
            self.progress("Checking nmap version and UDP privilege")
            info: NmapInfo = check_nmap(
                NMAP_MIN_VERSION, runner=self._logged_runner, find_executable=self.find_executable,
                **({"privilege": self.privilege} if self.privilege is not None else {}),
            )
            self.evidence.write("NMAP PREFLIGHT", info)
            if not info.has_raw_privilege:
                self.outcomes["UDP coverage"] = "ERROR"
                raise ScanError(f"{RAW_PRIVILEGE_HELP} {info.privilege_error}")
            self.progress("Authenticating and checking factory-default identity and configuration")
            self._preflight_gateway()
            self.outcomes["IXIT declaration prerequisite"] = "ERROR"
            validate_interface_declarations(self.ixit_directory)
            self.outcomes["IXIT declaration prerequisite"] = "PASS"
            self.evidence.write("EXPECTED IXIT 28 / IXIT 15 INTERFACE MODEL", EXPECTED_INTERFACES)
            self.evidence.write("PHYSICAL CONTEXT", "IXIT 15: Ethernet or Wi-Fi STA LAN; hotspot inactive")
            self.progress("Scanning full TCP range and confirming unexpected listeners")
            self.outcomes["TCP coverage"] = "ERROR"
            tcp: ScanResult = run_tcp_connect_scan(
                resolved.ip, PORTS, nmap_path=info.path, runner=self._logged_runner,
                host_timeout=TCP_HOST_TIMEOUT, max_retries=MAX_RETRIES, scan_delay_ms=FULL_SCAN_DELAY_MS,
                **({"clock": self.clock} if self.clock is not None else {}),
            )
            self._record_scan("TCP", tcp)
            self._compare_tcp(resolved.ip, info, tcp)
            self.outcomes["NTP source identification"] = "ERROR"
            self.progress("Starting tcpdump for the gateway NTP client request")
            host_ip: str = self.local_ip(resolved.ip)
            self.evidence.write("LOCAL NTP CAPTURE", {
                "dut_ip": resolved.ip, "host_ip": host_ip, "filter": NTP_CAPTURE_FILTER,
            })
            active_capture: NtpCapture = self.capture_factory()
            capture = active_capture
            capture_command: tuple[str, ...] = active_capture.start(resolved.ip, host_ip)
            self.evidence.write("TCPDUMP COMMAND", capture_command)
            self.progress("Temporarily setting ntp_server1 and capturing its UDP source port")
            session: requests.Session | None = self.auth_session
            if session is None:
                raise ScanError("preflight did not retain an authenticated session")
            mutation_possible = True  # A failed or timed-out POST may still apply.
            self._set_ntp_server(session, host_ip)
            self._verify_changed_config(session, host_ip)
            ntp_capture: NtpCaptureResult = active_capture.collect(NTP_REQUEST_TIMEOUT)
            ntp_source_port: int = ntp_capture.source_port
            if not 49152 <= ntp_source_port <= 65535:
                raise ScanError(f"captured NTP source port {ntp_source_port} is outside the dynamic range")
            self.evidence.write("CAPTURED NTP CLIENT REQUEST", ntp_capture)
            self._say(f"NTP client source port: {ntp_source_port}/udp (request to {host_ip}:123)")
            self.outcomes["NTP source identification"] = "PASS"
            active_capture.stop()
            capture = None
            self.progress("Scanning full privileged UDP range")
            self.outcomes["UDP coverage"] = "ERROR"
            udp: ScanResult = run_udp_scan(
                resolved.ip, PORTS, nmap_path=info.path, runner=self._logged_runner,
                host_timeout=UDP_HOST_TIMEOUT, max_retries=MAX_RETRIES, scan_delay_ms=FULL_SCAN_DELAY_MS,
                has_raw_privilege=info.has_raw_privilege, enumerate_ports=True,
                **({"clock": self.clock} if self.clock is not None else {}),
            )
            self._record_scan("UDP", udp)
            self.progress("Probing documented mDNS responder")
            self._compare_mdns(resolved.ip, udp)
            self.progress("Confirming UDP listeners and hotspot-server disablement")
            self._compare_udp(resolved.ip, info, udp, ntp_source_port)
            self.progress("Comparing confirmed sockets with enabled IXIT 28 entries")
        except Exception as error:  # noqa: BLE001 - Preserve completed check outcomes on any acquisition error.
            error: Exception
            self.evidence.recovering = True
            self.evidence.exception(error)
            self._say(f"scan ERROR: {type(error).__name__}: {error}")
            verdict = "ERROR"
        finally:
            self.evidence.recovering = True
            if capture is not None:
                try:
                    capture.stop()
                except Exception as error:  # noqa: BLE001 - Restoration must run after capture cleanup fails.
                    error: Exception
                    self.evidence.exception(error)
                    self._say(f"tcpdump cleanup ERROR: {type(error).__name__}: {error}")
                    verdict = "ERROR"
            if mutation_possible:
                self.evidence.attempt(lambda: self.progress("Restoring and verifying the original NTP configuration"))
                self.outcomes["NTP configuration restoration"] = "ERROR"
                try:
                    self._restore_ntp_server()
                except Exception as error:  # noqa: BLE001 - Operator recovery is needed if full state differs.
                    error: Exception
                    self.evidence.exception(error)
                    self._say(f"NTP restoration ERROR: {type(error).__name__}: {error}")
                    original: str | None = self.original_ntp_server1
                    recovery = (
                        f"USER ACTION REQUIRED: Restore ntp_server1 to {original} on the verified gateway "
                        "and confirm the full GET /ruuvi.json configuration matches its pretest baseline."
                    )
                    verdict = "ERROR"
                else:
                    self.outcomes["NTP configuration restoration"] = "PASS"
        if self.evidence.failed:
            verdict = "ERROR"
        if verdict != "ERROR":
            values: tuple[str, ...] = tuple(self.outcomes.values())
            verdict = "ERROR" if "ERROR" in values or "NOT RUN" in values else (
                "FAIL" if "FAIL" in values else "PASS"
            )
        if self.factory_reset_required:
            recovery = FACTORY_RESET_MESSAGE
        finding: Finding
        for finding in self.findings:
            self.evidence.write("FINDING", finding)
            self._say(f"undocumented listener FAIL: {finding.port}/{finding.protocol} ({finding.detail})")
        check: str
        outcome: str
        for check, outcome in self.outcomes.items():
            self.evidence.write("FINAL CHECK RESULT", {"check": check, "result": outcome})
            if outcome in {"FAIL", "ERROR"}:
                self._say(f"{check} {outcome}")
        self.evidence.write("FINDING TABLE", self.findings)
        self.evidence.write("OVERALL RESULT", verdict)
        if self.evidence.failed:
            verdict = "ERROR"
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(), recovery)


def execute_test_5_6_1_2_c(
    work_dir: Path | None = None,
    *, session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
    resolver: Resolver = system_resolver,
    socket_factory: Callable[..., DatagramSocket] = new_datagram_socket,
    find_executable: Callable[[str], str | None] = find_nmap,
    local_ip: Callable[[str], str] = local_ipv4_for,
    capture_factory: Callable[[], NtpCapture] = TcpdumpNtpCapture,
    privilege: Callable[[], bool] | None = None,
    clock: Callable[[], float] | None = None,
    ixit_directory: Path | None = None,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    directory: Path = Path.cwd() if work_dir is None else work_dir
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    try:
        log: RecoveryEvidence = RecoveryEvidence(EvidenceLog.create(directory / "logs", "test_5_6_1_2_c", now))
    except Exception:  # noqa: BLE001 - No DUT activity is permitted without an evidence file.
        try:
            output("setup ERROR: could not create evidence log")
        except Exception:  # noqa: BLE001, S110 - Both reporting channels are unavailable; return setup ERROR.
            pass
        return result

    def report(message: str) -> None:
        log.attempt(lambda: output(message))
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, TOTAL_STEPS)
    try:
        log.attempt(lambda: output(f"Open log file: {log.path}"))
        log.write("TEST CASE AND UNIT", TEST_ID)
        log.write("UTC START", format_utc(log.started_at))
        log.write("PRECONDITIONS", (
            "Dedicated initialized gateway on same LAN segment, SoftAP inactive; nmap >= 7.40, "
            "root/CAP_NET_RAW for UDP, tcpdump for outbound NTP capture, "
            "host mDNS resolution or IPv4 gw_hostname; ntp_server1 is temporarily changed and restored"
        ))
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(directory / ".env")
        log.write("DUT CONFIGURATION", config)
        result = FunctionalTest_5_6_1_2_c(
            config, log, session_factory=session_factory, random_bytes=random_bytes,
            ecc_generate=ecc_generate, runner=runner, resolver=resolver,
            socket_factory=socket_factory, find_executable=find_executable,
            local_ip=local_ip, capture_factory=capture_factory,
            privilege=privilege, clock=clock, ixit_directory=ixit_directory, progress=progress.step, output=output,
        ).run()
    except Exception as error:
        error: Exception
        log.recovering = True
        log.exception(error)  # noqa: TRY401 - EvidenceLog needs the exception object.
        report(f"setup ERROR: {type(error).__name__}")
    finally:
        log.recovering = True
        if result.recovery_message is not None:
            log.write("USER ACTION REQUIRED", result.recovery_message)
            log.attempt(lambda: output(result.recovery_message or ""))
        if log.failed:
            result = replace(result, verdict="ERROR", exit_code=2)
        log.attempt(lambda: output(f"Overall verdict: {result.verdict}"))
        if log.failed:
            result = replace(result, verdict="ERROR", exit_code=2)
        log.finish(result.verdict, now)
        if log.failed:
            result = replace(result, verdict="ERROR", exit_code=2)
            log.attempt(lambda: output(f"Overall verdict: {result.verdict}"))
    return result


def main() -> int:
    return execute_test_5_6_1_2_c().exit_code


if __name__ == "__main__":
    sys.exit(main())
