"""Live ETSI 5.5-5-2 Unit B network listening-surface test."""

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
from dataclasses import dataclass
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

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-5-2, Test Unit B"
PORTS: str = "1-65535"
NMAP_MIN_VERSION: tuple[int, int] = (7, 40)
TCP_HOST_TIMEOUT: int = 600
UDP_HOST_TIMEOUT: int = 1800
MAX_RETRIES: int = 1
FULL_SCAN_DELAY_MS: int = 5
MDNS_TIMEOUT: float = 3.0
TCPDUMP_START_TIMEOUT: float = 10.0
NTP_REQUEST_TIMEOUT: float = 25.0
NTP_CAPTURE_FILTER: str = "udp dst port 123 and udp[4:2] = 56 and (udp[8] & 7) = 3"
RAW_PRIVILEGE_HELP: str = (
    "Nmap cannot run the required UDP scan. Run sudo .venv/bin/python test_5_5_5_2_b.py. "
    "CAP_NET_RAW on Python alone does not grant it to the separate nmap process. "
    "If Python also lacks CAP_NET_RAW, these commands enable it for Python only:\n"
    "sudo apt install libcap2-bin\n"
    "sudo setcap cap_net_raw+ep /usr/bin/python3.8"
)
TOTAL_STEPS: int = 8
DEFAULT_FIELDS: tuple[str, ...] = AUTHENTICATION_DEFAULT_FIELDS + (
    "wifi_sta_config", "wifi_ap_config", "use_eth", "eth_dhcp", "ntp_use", "ntp_use_dhcp",
    "ntp_server1", "ntp_server2", "ntp_server3", "ntp_server4",
)
CHECKS: tuple[str, ...] = (
    "factory-default preflight", "NTP source identification", "documented HTTP listener",
    "TCP undocumented listeners", "TCP coverage", "UDP undocumented listeners",
    "documented mDNS responder", "UDP coverage", "NTP configuration restoration",
)


@dataclass(frozen=True)
class ExpectedInterface:
    protocol: str
    ports: tuple[int, ...]
    ixit_28: str | None
    ixit_13: tuple[str, ...]
    role: str
    enabled_on_lan: bool


EXPECTED_INTERFACES: tuple[ExpectedInterface, ...] = (
    ExpectedInterface(
        "tcp", (80,), "LogIntf-HTTP-Server",
        ("SoftServ-Local-Management-WebUI", "SoftServ-Local-Programmatic-API"),
        "inbound", True,
    ),
    ExpectedInterface("udp", (53,), "LogIntf-Network-DNS-Client", (), "outbound", False),
    ExpectedInterface("udp", (67, 68), "LogIntf-Network-DHCP-Client", (), "outbound", False),
    ExpectedInterface("udp", (123,), "LogIntf-Time-NTP-Client", (), "outbound", False),
    ExpectedInterface("udp", (67, 68), "LogIntf-Hotspot-DHCP-Server",
                     ("SoftServ-Hotspot-Orchestration",), "hotspot-only", False),
    ExpectedInterface("udp", (53,), "LogIntf-Hotspot-DNS-Server",
                     ("SoftServ-Hotspot-Orchestration",), "hotspot-only", False),
    ExpectedInterface("udp", (5353,), "LogIntf-mDNS-Responder",
                      ("SoftServ-Local-mDNS-Discovery",), "inbound", True),
)


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
    """Ask the host routing table which IPv4 source address reaches the DUT."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route_socket:
        route_socket.connect((dut_ip, 123))
        return str(route_socket.getsockname()[0])


def parse_ntp_source_port(output: str, dut_ip: str, host_ip: str) -> int:
    """Read the source port from a numeric tcpdump line for the selected peers."""
    pattern: str = rf"\b{re.escape(dut_ip)}\.(\d+) > {re.escape(host_ip)}\.123:"
    match: re.Match[str] | None = re.search(pattern, output)
    if match is None:
        raise ScanError(f"tcpdump did not report the gateway NTP source port: {output.strip()}")
    port: int = int(match.group(1))
    if not 1 <= port <= 65535:
        raise ScanError(f"tcpdump reported invalid NTP source port {port}")
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
    """Compare JSON values without Python's bool/int equality aliasing, including nested values."""
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


class FunctionalTest_5_5_5_2_b:
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
        progress: Callable[[str], None] | None = None,
        output: Callable[[str], None] | None = None,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config, evidence, session_factory=session_factory,
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
        self.original_ntp_server1: str | None = None
        self.auth_session: requests.Session | None = None

    def _say(self, message: str) -> None:
        self.output(message)
        self.evidence.write_line(message)

    def _assert_setup(self, description: str, passed: bool, actual: Any) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if passed else "ERROR", actual))
        if not passed:
            raise InvalidSetup(f"{description}: {actual!r}")

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
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._assert_setup("gw_mac is a six-octet MAC",
                               isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None, mac)
            self._assert_setup("DUT gw_mac matches .env",
                               mac.replace(":", "").upper() == self.config.gw_mac.replace(":", "").upper(), mac)
        if auth_type != GatewayCfgLanAuthType.DEFAULT and GatewayCfgDesc.GW_MAC in payload:
            self.factory_reset_required = True
        self._assert_setup("interactive challenge reports default authentication",
                           auth_type == GatewayCfgLanAuthType.DEFAULT, auth_type)
        defaults: dict[str, Any] = default_config_values(DEFAULT_FIELDS)
        expected: Any
        field: str
        for field, expected in defaults.items():
            actual: Any = payload.get(field)
            matches: bool = field in payload and same_public_value(actual, expected)
            if not matches and GatewayCfgDesc.GW_MAC in payload:
                self.factory_reset_required = True
            self._assert_setup(f"{field} has its public factory-default value", matches, actual)
        self.original_ntp_server1 = payload["ntp_server1"]
        self.auth_session = login.session
        self.outcomes["factory-default preflight"] = "PASS"

    def _set_ntp_server(self, session: requests.Session, value: str) -> None:
        response: requests.Response = self.gateway.request(
            session, HttpMethod.POST, GatewayApi.CONFIG, json_body={"ntp_server1": value},
        )
        if response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"POST /ruuvi.json ntp_server1 returned {response.status_code}")

    def _read_ntp_server(self, session: requests.Session) -> str:
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        if response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"GET /ruuvi.json after NTP change returned {response.status_code}")
        payload: dict[str, Any] = self.gateway.response_json(response, GatewayApi.CONFIG, dict)
        value: Any = payload.get("ntp_server1")
        if not isinstance(value, str):
            raise ScanError(f"GET /ruuvi.json ntp_server1 has invalid value {value!r}")
        return value

    def _restore_ntp_server(self) -> None:
        original: str | None = self.original_ntp_server1
        if original is None:
            raise ScanError("original ntp_server1 was not saved")
        login: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        if login.login_response.status_code != HttpStatus.C_200_OK:
            raise ScanError(f"restoration login returned {login.login_response.status_code}")
        post_error: Exception | None = None
        try:
            self._set_ntp_server(login.session, original)
        except Exception as error:  # noqa: BLE001 - Read back even after a lost POST response.
            post_error = error
            self.evidence.exception(error)
        actual: str = self._read_ntp_server(login.session)
        if actual != original:
            raise ScanError(f"ntp_server1 restoration failed: expected {original}, got {actual}") from post_error
        if post_error is not None:
            self.evidence.write("NTP RESTORATION", "POST reported an error, but GET confirms original value")

    def _logged_runner(self, command: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        self.evidence.write("NMAP COMMAND", command)
        try:
            result: subprocess.CompletedProcess[str] = self.runner(command, **kwargs)
        except Exception as error:
            error: Exception
            self._log_raw_xml(command)
            self.evidence.exception(error)
            raise
        stdout: str = result.stdout
        if "-d3" in command and len(stdout) > 8192:
            stdout = f"{stdout[:4096]}\n...[{len(stdout) - 8192} debug characters omitted]...\n{stdout[-4096:]}"
        self.evidence.write("NMAP PROCESS", {"returncode": result.returncode,
                                             "stdout": stdout, "stderr": result.stderr})
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

    @staticmethod
    def _port_ids_for_evidence(ports: frozenset[int]) -> list[int] | str:
        return sorted(ports) if len(ports) <= 256 else f"{len(ports)} port IDs; see raw Nmap XML"

    def _record_scan(self, label: str, result: ScanResult) -> None:
        self.evidence.write(f"{label} RAW NMAP XML", result.raw_xml)
        self.evidence.write(f"{label} SCAN RESULT", {
            "open": sorted(result.open_ports), "closed_count": result.closed_count,
            "closed": self._port_ids_for_evidence(result.closed_ports),
            "filtered_count": result.filtered_count,
            "filtered": self._port_ids_for_evidence(result.filtered_ports),
            "open_filtered_count": result.open_filtered_count,
            "open_filtered": self._port_ids_for_evidence(result.open_filtered),
            "closed_filtered_count": result.closed_filtered_count,
            "closed_filtered": self._port_ids_for_evidence(result.closed_filtered),
            "elapsed_seconds": result.elapsed,
            "observations": result.observations,
            "extraports": result.extraports,
        })
        if len(result.extraports) > 1:
            self.evidence.write(f"{label} COMPRESSED PORT ID LIMITATION", (
                "Nmap reported multiple extraports states without individual port IDs. "
                "State counts are complete; closed/filtered/open|filtered/closed|filtered sets "
                "contain only explicit ports."
            ))

    def _tcp_confirmation(self, ip: str, port: int, info: NmapInfo) -> ScanResult:
        result: ScanResult = run_tcp_connect_scan(
            ip, f"{port:d}", nmap_path=info.path, runner=self._logged_runner,
            host_timeout=60, max_retries=MAX_RETRIES,
            **({"clock": self.clock} if self.clock is not None else {}),
        )
        self._record_scan(f"TCP CONFIRMATION {port}", result)
        return result

    def _udp_confirmation(self, ip: str, port: int, info: NmapInfo) -> ScanResult:
        result: ScanResult = run_udp_scan(
            ip, f"{port:d}", nmap_path=info.path, runner=self._logged_runner,
            host_timeout=60, max_retries=MAX_RETRIES, has_raw_privilege=info.has_raw_privilege,
            **({"clock": self.clock} if self.clock is not None else {}),
        )
        self._record_scan(f"UDP CONFIRMATION {port}", result)
        return result

    def _compare_tcp(self, ip: str, info: NmapInfo, result: ScanResult) -> None:
        declared: frozenset[int] = frozenset(
            port for entry in EXPECTED_INTERFACES if entry.protocol == "tcp" and entry.enabled_on_lan
            and entry.role == "inbound" for port in entry.ports
        )
        http: PortObservation | None = next(
            (item for item in result.observations if item.port == 80 and item.state == "open"), None,
        )
        http_ok: bool = declared.issubset(result.open_ports) and http is not None and http.service == "http"
        self.outcomes["documented HTTP listener"] = "PASS" if http_ok else "FAIL"
        self.evidence.write("ASSERTION", AssertionEvidence("80/tcp is open and fingerprints as HTTP",
                                                            self.outcomes["documented HTTP listener"], http))
        if not http_ok:
            self._say(f"documented HTTP listener FAIL: 80/tcp missing or fingerprint is {http!r}")
        extra: list[int] = sorted(result.open_ports - declared)
        self.outcomes["TCP undocumented listeners"] = "ERROR"
        port: int
        for port in extra:
            confirmation: ScanResult = self._tcp_confirmation(ip, port, info)
            if port in confirmation.open_ports:
                self.findings.append(Finding("tcp", port, "no enabled IXIT 28 entry", "open", "open"))
            else:
                raise ScanError(f"unstable TCP finding on {port}: confirmation did not reproduce open state")
        self.outcomes["TCP undocumented listeners"] = "FAIL" if extra else "PASS"
        self.outcomes["TCP coverage"] = "ERROR" if result.filtered_count else "PASS"
        if result.filtered_count:
            port_word: str = "port" if result.filtered_count == 1 else "ports"
            self._say(
                f"TCP coverage ERROR: {result.filtered_count} filtered {port_word} gave no definitive "
                "response; a complete listener comparison is not possible"
            )

    def _compare_udp(self, ip: str, info: NmapInfo, result: ScanResult,
                     mdns_answer: MdnsAnswer | None, ntp_source_port: int) -> None:
        declared: frozenset[int] = frozenset(
            port for entry in EXPECTED_INTERFACES if entry.protocol == "udp" and entry.enabled_on_lan
            and entry.role == "inbound" for port in entry.ports
        )
        self.outcomes["UDP undocumented listeners"] = "ERROR"
        unresolved: set[int] = set(result.open_filtered | result.closed_filtered)
        unresolved.update(result.filtered_ports)
        sntp_resolved: bool = ntp_source_port in unresolved
        unresolved.discard(ntp_source_port)
        mdns_resolved: bool = mdns_answer is not None and 5353 in unresolved
        if mdns_answer is not None:
            unresolved.discard(5353)
        port: int
        for port in sorted(result.open_ports - declared):
            confirmation: ScanResult = self._udp_confirmation(ip, port, info)
            if port in confirmation.open_ports:
                self.findings.append(Finding("udp", port, "no enabled IXIT 28 server entry", "open", "open"))
            else:
                raise ScanError(f"unstable UDP finding on {port}: confirmation did not reproduce open state")
        self.outcomes["UDP undocumented listeners"] = "FAIL" if result.open_ports - declared else "PASS"
        filtered_without_ids: int = result.filtered_count - len(result.filtered_ports)
        indeterminate_count: int = len(unresolved) + filtered_without_ids
        self.evidence.write("UDP COVERAGE LIMITATION", (
            f"Full {PORTS}/udp scanned with host timeout {UDP_HOST_TIMEOUT}s and max retries {MAX_RETRIES}; "
            f"{result.open_filtered_count} open|filtered and {result.closed_filtered_count} "
            f"closed|filtered ports in the full scan; "
            f"{int(sntp_resolved)} identified as the outbound SNTP client by captured request; "
            f"{int(mdns_resolved)} resolved by direct mDNS response; "
            f"{indeterminate_count} ports remain indeterminate "
            f"including {result.filtered_count} filtered. "
            "A non-response cannot prove absence. "
            "Only positively responding services and the direct mDNS probe support listener findings."
        ))
        self.outcomes["UDP coverage"] = "ERROR" if indeterminate_count else "PASS"
        if indeterminate_count:
            port_word: str = "port" if indeterminate_count == 1 else "ports"
            unresolved_ports: list[int] = sorted(unresolved)
            sample_ports: frozenset[int] = frozenset(unresolved_ports[:10])
            details: list[str] = [f"{port}/udp" for port in unresolved_ports[:10]]
            if len(unresolved_ports) > 10:
                details.append(f"and {len(unresolved_ports) - 10} more")
            if filtered_without_ids:
                details.append(f"{filtered_without_ids} filtered with IDs unavailable")
            sample_observations: list[PortObservation] = [
                item for item in result.observations if item.port in sample_ports
            ]
            self.evidence.write("UDP UNRESOLVED PORTS", {
                "explicit_ports": self._port_ids_for_evidence(frozenset(unresolved)),
                "filtered_without_ids": filtered_without_ids,
                "sample_observations": sample_observations,
            })
            self._say(
                f"UDP coverage ERROR: {indeterminate_count} {port_word} did not give a definitive "
                f"response in the full scan: {', '.join(details)}"
            )

    def run(self) -> RunResult:
        verdict: str = "PASS"
        recovery: str | None = None
        capture: NtpCapture | None = None
        mutation_possible: bool = False
        try:
            self.progress("Resolving DUT LAN IPv4 address")
            resolved: ResolvedHost = resolve_host(self.config.gw_hostname, resolver=self.resolver)
            self.evidence.write("RESOLVED HOST", resolved)
            self.progress("Checking nmap version and UDP raw-socket privilege")
            info: NmapInfo = check_nmap(
                NMAP_MIN_VERSION, runner=self._logged_runner, find_executable=self.find_executable,
                **({"privilege": self.privilege} if self.privilege is not None else {}),
            )
            self.evidence.write("NMAP PREFLIGHT", info)
            if not info.has_raw_privilege:
                self.outcomes["UDP coverage"] = "ERROR"
                raise ScanError(RAW_PRIVILEGE_HELP + (
                    f"\n{info.privilege_error}" if info.privilege_error else ""
                ))
            self.progress("Authenticating and verifying factory-default DUT identity and configuration")
            self._preflight_gateway()
            self.evidence.write("EXPECTED IXIT 28 / IXIT 13 INTERFACE MODEL", EXPECTED_INTERFACES)
            self.outcomes["NTP source identification"] = "ERROR"
            host_ip: str = self.local_ip(resolved.ip)
            self.evidence.write("LOCAL NTP CAPTURE", {
                "dut_ip": resolved.ip, "host_ip": host_ip, "filter": NTP_CAPTURE_FILTER,
            })
            active_capture: NtpCapture = self.capture_factory()
            capture = active_capture
            capture_command: tuple[str, ...] = active_capture.start(resolved.ip, host_ip)
            self.evidence.write("TCPDUMP COMMAND", capture_command)
            session: requests.Session | None = self.auth_session
            if session is None:
                raise ScanError("preflight did not retain an authenticated session")
            mutation_possible = True  # A failed or timed-out POST may still apply on the DUT.
            self._set_ntp_server(session, host_ip)
            observed_server: str = self._read_ntp_server(session)
            if observed_server != host_ip:
                raise ScanError(f"ntp_server1 change did not apply: expected {host_ip}, got {observed_server}")
            ntp_capture: NtpCaptureResult = active_capture.collect(NTP_REQUEST_TIMEOUT)
            ntp_source_port: int = ntp_capture.source_port
            self.evidence.write("CAPTURED NTP CLIENT REQUEST", ntp_capture)
            self._say(f"NTP client source port: {ntp_source_port}/udp (captured request to {host_ip}:123)")
            self.outcomes["NTP source identification"] = "PASS"
            active_capture.stop()
            capture = None
            self.progress("Scanning full TCP range and confirming unexpected listeners")
            self.outcomes["TCP coverage"] = "ERROR"
            tcp: ScanResult = run_tcp_connect_scan(
                resolved.ip, PORTS, nmap_path=info.path, runner=self._logged_runner,
                host_timeout=TCP_HOST_TIMEOUT, max_retries=MAX_RETRIES,
                scan_delay_ms=FULL_SCAN_DELAY_MS,
                **({"clock": self.clock} if self.clock is not None else {}),
            )
            self._record_scan("TCP", tcp)
            self._compare_tcp(resolved.ip, info, tcp)
            self.progress("Scanning full privileged UDP range and confirming open ports")
            self.outcomes["UDP coverage"] = "ERROR"
            udp: ScanResult = run_udp_scan(
                resolved.ip, PORTS, nmap_path=info.path, runner=self._logged_runner,
                host_timeout=UDP_HOST_TIMEOUT, max_retries=MAX_RETRIES,
                has_raw_privilege=info.has_raw_privilege, enumerate_ports=True,
                scan_delay_ms=FULL_SCAN_DELAY_MS,
                **({"clock": self.clock} if self.clock is not None else {}),
            )
            self._record_scan("UDP", udp)
            self.progress("Sending direct DNS-SD query to UDP 5353")
            self.outcomes["documented mDNS responder"] = "ERROR"
            self.evidence.write("MDNS REQUEST", {"ip": resolved.ip, "port": 5353,
                                                  "query": "PTR _services._dns-sd._udp.local", "timeout": MDNS_TIMEOUT})
            answer: MdnsAnswer | None = probe_mdns(
                resolved.ip, timeout=MDNS_TIMEOUT, socket_factory=self.socket_factory,
            )
            self.evidence.write("MDNS RESPONSE", answer if answer is not None else "no answer")
            if answer is not None:
                self.outcomes["documented mDNS responder"] = "PASS"
            else:
                mdns_closed: bool = 5353 in udp.closed_ports
                self.outcomes["documented mDNS responder"] = "FAIL" if mdns_closed else "ERROR"
                if mdns_closed:
                    self._say("documented mDNS responder FAIL: no direct DNS-SD answer and 5353/udp closed")
                else:
                    self._say("documented mDNS responder ERROR: no direct DNS-SD answer; UDP state is unverified")
            self.progress("Comparing discovered sockets with IXIT 28 and IXIT 13")
            self._compare_udp(resolved.ip, info, udp, answer, ntp_source_port)
        except Exception as error:  # noqa: BLE001 - Preserve earlier findings after any acquisition error.
            error: Exception
            self.evidence.exception(error)
            self._say(f"scan ERROR: {type(error).__name__}: {error}")
            verdict = "ERROR"
        finally:
            if capture is not None:
                try:
                    capture.stop()
                except Exception as error:  # noqa: BLE001 - Restoration must still run after capture cleanup fails.
                    self.evidence.exception(error)
                    self._say(f"tcpdump cleanup ERROR: {type(error).__name__}: {error}")
                    verdict = "ERROR"
            if mutation_possible:
                self.outcomes["NTP configuration restoration"] = "ERROR"
                try:
                    self._restore_ntp_server()
                except Exception as error:  # noqa: BLE001 - Preserve the original value for manual recovery.
                    self.evidence.exception(error)
                    self._say(f"ntp_server1 restoration ERROR: {type(error).__name__}: {error}")
                    original: str | None = self.original_ntp_server1
                    recovery = f"Restore ntp_server1 to {original} on the verified gateway and confirm GET /ruuvi.json."
                    verdict = "ERROR"
                else:
                    self.outcomes["NTP configuration restoration"] = "PASS"
                    self.evidence.write("NTP RESTORATION", {"ntp_server1": self.original_ntp_server1,
                                                            "verified": True})
        if verdict != "ERROR":
            values: tuple[str, ...] = tuple(self.outcomes.values())
            verdict = "ERROR" if "ERROR" in values or "NOT RUN" in values else (
                "FAIL" if "FAIL" in values else "PASS"
            )
        if self.factory_reset_required:
            recovery = f"{FACTORY_RESET_MESSAGE}\n{recovery}" if recovery else FACTORY_RESET_MESSAGE
        finding: Finding
        for finding in self.findings:
            self.evidence.write("FINDING", finding)
            self._say(f"undocumented listener FAIL: {finding.port}/{finding.protocol} ({finding.detail})")
        check: str
        outcome: str
        for check, outcome in self.outcomes.items():
            self.evidence.write("FINAL CHECK RESULT", {"check": check, "result": outcome})
            if outcome == "FAIL":
                self._say(f"{check} FAIL")
        self.evidence.write("FINDING TABLE", self.findings)
        self.evidence.write("OVERALL RESULT", verdict)
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(), recovery)


def execute_test_5_5_5_2_b(
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
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    directory: Path = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(directory / "logs", "test_5_5_5_2_b", now)
    output(f"Open log file: {log.path}")

    def report(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    log.write("PRECONDITIONS", (
        "Dedicated initialized gateway, SoftAP inactive, same LAN segment; no update or remote config; "
        "nmap >= 7.40 and tcpdump, capture privilege for NTP and raw-socket privilege for UDP, "
        "host mDNS resolution or IPv4 gw_hostname"
    ))
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(directory / ".env")
        log.write("DUT CONFIGURATION", config)
        result = FunctionalTest_5_5_5_2_b(
            config, log, session_factory=session_factory, random_bytes=random_bytes,
            ecc_generate=ecc_generate, runner=runner, resolver=resolver,
            socket_factory=socket_factory, find_executable=find_executable,
            local_ip=local_ip, capture_factory=capture_factory,
            privilege=privilege, clock=clock, progress=progress.step, output=output,
        ).run()
    except Exception as error:
        error: Exception
        log.exception(error)  # noqa: TRY401 - EvidenceLog needs the exception object.
        report(f"setup ERROR: {type(error).__name__}: {error}")
    finally:
        if result.recovery_message is not None:
            log.write("USER ACTION REQUIRED", result.recovery_message)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    if result.recovery_message is not None:
        output(result.recovery_message)
    return result


def main() -> int:
    return execute_test_5_5_5_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
