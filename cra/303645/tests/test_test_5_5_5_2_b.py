"""Offline implementation tests for 5.5-5-2 B; these are not DUT evidence."""

from __future__ import annotations

import hashlib
import json
import socket
import struct
import subprocess
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping
from urllib.parse import urlsplit

import requests

import probe_sntp_client_port as sntp_probe
import test_5_5_5_2_b as target
from lib.config import FACTORY_RESET_MESSAGE, default_config_values
from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import HttpMethod
from lib.models import DutConfig, RunResult
from lib.netscan import DatagramSocket
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 23, 5, 0, tzinfo=timezone.utc)


class SntpProbeTests(unittest.TestCase):
    def test_request_is_client_mode_with_48_byte_header_and_transmit_time(self) -> None:
        now_ns: int = 1_700_000_000_500_000_000
        packet: bytes = sntp_probe.build_ntp_client_request(now_ns)
        seconds: int
        fraction: int
        seconds, fraction = struct.unpack_from("!II", packet, 40)
        self.assertEqual(48, len(packet))
        self.assertEqual(4, (packet[0] >> 3) & 7)
        self.assertEqual(3, packet[0] & 7)
        self.assertEqual((0, 6, 0xEC), (packet[1], packet[2], packet[3]))
        self.assertEqual((3_908_988_800, 1 << 31), (seconds, fraction))

    def test_tcpdump_source_parser_requires_exact_peers_and_valid_port(self) -> None:
        line: str = "12:00:00 IP 192.0.2.10.64053 > 192.0.2.20.123: NTPv4, Client"
        self.assertEqual(64053, target.parse_ntp_source_port(line, "192.0.2.10", "192.0.2.20"))
        with self.assertRaises(target.ScanError):
            target.parse_ntp_source_port(line, "192.0.2.11", "192.0.2.20")
        with self.assertRaises(target.ScanError):
            target.parse_ntp_source_port("IP 192.0.2.10.0 > 192.0.2.20.123:", "192.0.2.10", "192.0.2.20")


class ScanGateway(DefaultAuthGateway):
    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.fields: dict[str, Any] = default_config_values(target.DEFAULT_FIELDS)
        self.fields.update({"mqtt_prefix": f"ruuvi/{CONFIG.gw_mac}/", "mqtt_client_id": CONFIG.gw_mac})
        self.read_count: int = 0
        self.drift_on_read: int = 0
        self.mac: Any = CONFIG.gw_mac
        self.include_mac: bool = True
        self.login_status: int | None = None
        self.fail_ntp_restore: bool = False
        self.lose_initial_ntp_response: bool = False

    def _config_response(self) -> FakeResponse:
        self.read_count += 1
        if self.read_count == self.drift_on_read:
            self.fields["unrelated_setting"] = "changed"
        payload: dict[str, Any] = dict(self.fields)
        if self.include_mac:
            payload[GatewayCfgDesc.GW_MAC] = self.mac
        return FakeResponse(200, payload)

    def _auth_response(self, session: FakeSession, method: str,
                       headers: Mapping[str, str], body: Any) -> FakeResponse:
        response: FakeResponse = super()._auth_response(session, method, headers, body)
        if self.login_status is not None and method == HttpMethod.POST:
            response.status_code = self.login_status
        return response

    def _apply_config(self, body: dict[str, str]) -> None:
        super()._apply_config(body)
        # Independent model of the unconditional omitted-field resets in the firmware parser.
        self.fields["mqtt_data_format"] = body.get("mqtt_data_format", "ruuvi_raw")
        self.fields["mqtt_prefix"] = body.get("mqtt_prefix") or f"ruuvi/{CONFIG.gw_mac}/"
        self.fields["mqtt_client_id"] = body.get("mqtt_client_id") or CONFIG.gw_mac
        if "ntp_server1" in body and not (self.fail_ntp_restore and body["ntp_server1"] == "time.google.com"):
            self.fields["ntp_server1"] = body["ntp_server1"]

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        if (self.lose_initial_ntp_response and call.method == HttpMethod.POST
                and call.path == GatewayApi.CONFIG and call.body == {"ntp_server1": "192.0.2.20"}):
            self.lose_initial_ntp_response = False
            self._apply_config(call.body)
            raise requests.Timeout("response lost after NTP change")
        return super()._response_override(session, call)


class FakeNmap:
    def __init__(self) -> None:
        self.calls: list[tuple[str, ...]] = []
        self.tcp_open: set[int] = {80}
        self.udp_open: set[int] = {5353}
        self.udp_ambiguous: dict[int, str] = {}
        self.tcp_ambiguous: dict[int, str] = {}
        self.tcp_filtered: bool = False
        self.udp_filtered: bool = False
        self.fail_udp: bool = False
        self.deny_udp_probe: bool = False
        self.version: str = "Nmap version 7.95 ( https://nmap.org )"
        self.tcp_http_service: str = "http"
        self.timeouts: list[int] = []
        self.unstable_tcp: bool = False
        self.fail_confirmation_port: int = 0
        self.incomplete_udp: bool = False
        self.reject_mdns_nmap_confirmation: bool = False

    def __call__(self, command: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        self.calls.append(tuple(command))
        self.timeouts.append(kwargs["timeout"])
        if command[-1] == "--version":
            return subprocess.CompletedProcess(command, 0, stdout=self.version, stderr="")
        if "-oX" in command and command[command.index("-oX") + 1] == "-":
            if self.deny_udp_probe:
                return subprocess.CompletedProcess(command, 1, stdout="", stderr="requires root privileges")
            return subprocess.CompletedProcess(command, 0, stdout="<nmaprun/>", stderr="")
        protocol: str = "tcp" if "-sT" in command else "udp"
        if protocol == "udp" and self.fail_udp:
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])
        specification: str = command[command.index("-p") + 1]
        if protocol == "udp" and specification == "5353" and self.reject_mdns_nmap_confirmation:
            raise AssertionError("5353/udp must be confirmed by direct DNS-SD query")
        if specification == str(self.fail_confirmation_port):
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])
        full_scan: bool = specification == "1-65535"
        ports: set[int] = set(range(1, 65536)) if full_scan else {int(token) for token in specification.split(",")}
        open_set: set[int] = (
            self.tcp_open if protocol == "tcp" else self.udp_open
        ) & ports
        if protocol == "tcp" and not full_scan and self.unstable_tcp:
            open_set.clear()
        ambiguous: dict[int, str]
        source_states: dict[int, str] = self.udp_ambiguous if protocol == "udp" else self.tcp_ambiguous
        ambiguous = {port: state for port, state in source_states.items()
                     if port in ports and port not in open_set}
        other: set[int] = ports - open_set - ambiguous.keys()
        other_state: str = "filtered" if protocol == "udp" and self.udp_filtered else "closed"
        explicit: str = "".join(
            f'<port protocol="{protocol}" portid="{port}"><state state="open" reason="syn-ack"/>'
            f'<service name="{self.tcp_http_service if protocol == "tcp" and port == 80 else "unknown"}"/>'
            "</port>"
            for port in sorted(open_set)
        )
        explicit += "".join(
            f'<port protocol="{protocol}" portid="{port}"><state state="{state}" reason="no-response"/></port>'
            for port, state in sorted(ambiguous.items())
        )
        if protocol == "tcp" and full_scan and self.tcp_filtered:
            extra: str = (
                f'<extraports state="closed" count="{len(other) - 1}"/>'
                '<extraports state="filtered" count="1"/>'
            )
        else:
            extra = f'<extraports state="{other_state}" count="{len(other)}"/>' if other else ""
        xml: str = (
            f'<nmaprun><scaninfo protocol="{protocol}" services="{specification}" '
            f'numservices="{len(ports)}"/><host><status state="up"/><ports>{extra}{explicit}'
            f'</ports></host><runstats><finished exit="{ "error" if protocol == "udp" and self.incomplete_udp else "success" }"/>'
            '</runstats></nmaprun>'
        )
        Path(command[command.index("-oX") + 1]).write_text(xml, encoding="utf-8")
        return subprocess.CompletedProcess(command, 0, stdout="", stderr="")


class FakeMdnsSocket(DatagramSocket):
    def __init__(self, answer: bool) -> None:
        self.answer: bool = answer
        self.request: bytes = b""
        self.destination: tuple[str, int] | None = None
        self.closed: bool = False
        self.timeout: float = 0.0

    def settimeout(self, timeout: float) -> None:
        self.timeout = timeout

    def sendto(self, data: bytes, address: tuple[str, int]) -> int:
        self.request = data
        self.destination = address
        return len(data)

    def recvfrom(self, size: int) -> tuple[bytes, tuple[str, int]]:
        if size < 4096:
            raise AssertionError("mDNS receive buffer is smaller than the test fixture")
        if not self.answer:
            raise socket.timeout()
        response: bytes = (
            b"\x00\x00\x84\x00\x00\x00\x00\x01\x00\x00\x00\x00"
            b"\x09_services\x07_dns-sd\x04_udp\x05local\x00\x00\x0c\x00\x01"
            b"\x00\x00\x00\x01\x00\x12\x05_http\x04_tcp\x05local\x00"
        )
        return response, ("192.0.2.10", 5353)

    def close(self) -> None:
        self.closed = True


class FakeNtpCapture:
    def __init__(self, gateway: ScanGateway) -> None:
        self.gateway: ScanGateway = gateway
        self.started: bool = False
        self.stopped: bool = False
        self.fail_start: bool = False
        self.fail_collect: bool = False
        self.dut_ip: str = ""
        self.host_ip: str = ""
        self.start_call_count: int = 0

    def start(self, dut_ip: str, host_ip: str) -> tuple[str, ...]:
        if self.fail_start:
            raise RuntimeError("tcpdump could not start")
        self.started = True
        self.dut_ip = dut_ip
        self.host_ip = host_ip
        self.start_call_count = len(self.gateway.calls)
        if self.gateway.calls[-1].method != HttpMethod.GET or self.gateway.calls[-1].path != GatewayApi.CONFIG:
            raise AssertionError("tcpdump did not start immediately after preflight")
        return "/usr/bin/tcpdump", "-i", "any", "-nn", "-c", "1"

    def collect(self, timeout: float) -> target.NtpCaptureResult:
        if self.fail_collect:
            raise target.ScanError("no NTP request captured")
        if timeout != target.NTP_REQUEST_TIMEOUT or self.gateway.fields["ntp_server1"] != self.host_ip:
            raise AssertionError("NTP capture ran before the gateway used the local server")
        if not any(call.method == HttpMethod.POST and call.body == {"ntp_server1": self.host_ip}
                   for call in self.gateway.calls[self.start_call_count:]):
            raise AssertionError("NTP capture ran before the configuration POST")
        return target.NtpCaptureResult(64053, f"IP {self.dut_ip}.64053 > {self.host_ip}.123: NTPv4, Client", "")

    def stop(self) -> None:
        self.stopped = True


@dataclass(frozen=True)
class WireRequest:
    scheme: str
    method: str
    path: str
    body: Any
    session: int
    allow_redirects: bool
    cookie: str | None


class ScanSession(FakeSession):
    def __init__(self, gateway: ScanGateway, wire_calls: list[WireRequest]) -> None:
        super().__init__(gateway)
        self.wire_calls: list[WireRequest] = wire_calls

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        url: str = request.url or ""
        method: str = request.method or ""
        body: Any = json.loads(request.body) if request.body else None
        self.wire_calls.append(WireRequest(
            urlsplit(url).scheme, method, urlsplit(url).path, body, self.number,
            bool(kwargs.get("allow_redirects")), request.headers.get("Cookie"),
        ))
        return super().send(request, **kwargs)


class NetworkSurfaceRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory: Path = Path(self.temp.name)
        self.directory.joinpath(".env").write_text(
            f"gw_id={CONFIG.gw_id}\ngw_mac={CONFIG.gw_mac}\ngw_hostname={CONFIG.gw_hostname}\n",
            encoding="utf-8",
        )
        self.ixit_directory: Path = self.directory / "ixit"
        self.ixit_directory.mkdir()
        self.ixit_directory.joinpath("ixit_28-LogIntf.md").write_text(
            "### **ID**: LogIntf-mDNS-Responder\n\nUDP 5353 inbound responder.\n", encoding="utf-8",
        )
        self.ixit_directory.joinpath("ixit_13-SoftServ.md").write_text(
            "### **ID**: SoftServ-Local-mDNS-Discovery\n\nLAN DNS-SD discovery service.\n", encoding="utf-8",
        )
        self.gateway: ScanGateway = ScanGateway()
        self.nmap: FakeNmap = FakeNmap()
        self.nmap.udp_ambiguous[64053] = "closed|filtered"
        self.mdns: FakeMdnsSocket = FakeMdnsSocket(True)
        self.ntp_capture: FakeNtpCapture = FakeNtpCapture(self.gateway)
        self.lines: list[str] = []
        self.wire_calls: list[WireRequest] = []

    def execute(self, *, privilege: bool | None = True,
                find_executable: bool = True, resolvable: bool = True) -> RunResult:
        selected_privilege: bool = bool(privilege)
        def resolve(_host: str, _port: int | None, **_kwargs: Any) -> list[tuple[int, int, int, str, tuple[str, int]]]:
            if not resolvable:
                raise OSError("name not found")
            return [(socket.AF_INET, socket.SOCK_STREAM, 6, "", ("192.0.2.10", 0))]
        return target.execute_test_5_5_5_2_b(
            self.directory, ixit_directory=self.ixit_directory, session_factory=lambda: ScanSession(self.gateway, self.wire_calls), runner=self.nmap,
            resolver=resolve,
            socket_factory=lambda family, kind: self.mdns,
            find_executable=lambda name: "/usr/bin/nmap" if find_executable else None,
            local_ip=lambda dut_ip: "192.0.2.20",
            capture_factory=lambda: self.ntp_capture,
            privilege=(lambda: selected_privilege) if privilege is not None else None,
            clock=lambda: 1.0, now=lambda: NOW, output=self.lines.append,
        )

    def evidence(self) -> str:
        logs: list[Path] = list(self.directory.joinpath("logs").glob("test_5_5_5_2_b_*.log"))
        self.assertEqual(1, len(logs))
        return logs[0].read_text(encoding="utf-8")

    def test_constants_and_full_successful_request_and_scan_order(self) -> None:
        self.assertEqual("1-65535", target.PORTS)
        self.assertEqual((7, 40), target.NMAP_MIN_VERSION)
        self.assertGreater(target.UDP_HOST_TIMEOUT, target.TCP_HOST_TIMEOUT)
        self.assertEqual(1, target.MAX_RETRIES)
        self.assertEqual(5, target.FULL_SCAN_DELAY_MS)
        result: RunResult = self.execute()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        # The captured SNTP source port is the sole indeterminate UDP port.
        self.assertEqual("PASS", result.outcomes["documented HTTP listener"])
        self.assertEqual("PASS", result.outcomes["documented mDNS responder"])
        self.assertEqual(
            [(HttpMethod.GET, GatewayApi.AUTH), (HttpMethod.POST, GatewayApi.AUTH),
             (HttpMethod.GET, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.CONFIG),
             (HttpMethod.POST, GatewayApi.CONFIG),
             (HttpMethod.GET, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.AUTH),
             (HttpMethod.POST, GatewayApi.AUTH), (HttpMethod.POST, GatewayApi.CONFIG),
             (HttpMethod.GET, GatewayApi.CONFIG)],
            [(call.method, call.path) for call in self.gateway.calls],
        )
        self.assertEqual([None, "Admin", None, None, None, None, None, "Admin", None, None], [
            None if call.body is None else call.body.get("login")
            for call in self.gateway.calls
        ])
        self.assertEqual([{"ntp_server1": "192.0.2.20"}, {"ntp_server1": "time.google.com"}],
                         [call.body for call in self.gateway.calls if call.method == HttpMethod.POST
                          and call.path == GatewayApi.CONFIG])
        login: Any = self.gateway.calls[1].body
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        expected_response: str = hashlib.sha256(f"challenge-1-1:{ha1}".encode()).hexdigest()
        restore_response: str = hashlib.sha256(f"challenge-2-1:{ha1}".encode()).hexdigest()
        self.assertEqual({"login": "Admin", "password": expected_response}, login)
        self.assertEqual("RUUVISESSION=session-1", self.gateway.calls[1].cookie)
        self.assertEqual([1, 1, 1, 1, 1, 1, 2, 2, 2, 2],
                         [call.session_number for call in self.gateway.calls])
        self.assertEqual([
            WireRequest("http", "GET", "/auth", None, 1, False, None),
            WireRequest("http", "POST", "/auth", login, 1, False, "RUUVISESSION=session-1"),
            WireRequest("http", "GET", "/ruuvi.json", None, 1, False, "RUUVISESSION=session-1"),
            WireRequest("http", "GET", "/ruuvi.json", None, 1, False, "RUUVISESSION=session-1"),
            WireRequest("http", "POST", "/ruuvi.json", {"ntp_server1": "192.0.2.20"},
                        1, False, "RUUVISESSION=session-1"),
            WireRequest("http", "GET", "/ruuvi.json", None, 1, False, "RUUVISESSION=session-1"),
            WireRequest("http", "GET", "/auth", None, 2, False, None),
            WireRequest("http", "POST", "/auth", {"login": "Admin", "password": restore_response},
                        2, False, "RUUVISESSION=session-2"),
            WireRequest("http", "POST", "/ruuvi.json", {"ntp_server1": "time.google.com"},
                        2, False, "RUUVISESSION=session-2"),
            WireRequest("http", "GET", "/ruuvi.json", None, 2, False, "RUUVISESSION=session-2"),
        ], self.wire_calls)
        self.assertTrue(all(not call.allow_redirects for call in self.gateway.calls))
        self.assertEqual(["--version", "192.0.2.10", "192.0.2.10"],
                         [command[-1] for command in self.nmap.calls])
        self.assertIn("-sT", self.nmap.calls[1])
        self.assertIn("-sU", self.nmap.calls[2])
        self.assertEqual("5ms", self.nmap.calls[1][self.nmap.calls[1].index("--scan-delay") + 1])
        self.assertEqual("5ms", self.nmap.calls[2][self.nmap.calls[2].index("--scan-delay") + 1])
        self.assertIn("--defeat-icmp-ratelimit", self.nmap.calls[2])
        self.assertIn("-d3", self.nmap.calls[2])
        self.assertEqual([10, target.TCP_HOST_TIMEOUT + 30, target.UDP_HOST_TIMEOUT + 30],
                         self.nmap.timeouts)
        self.assertEqual(("192.0.2.10", 5353), self.mdns.destination)
        self.assertTrue(self.mdns.closed)
        log: str = self.evidence()
        self.assertIn("EXPECTED IXIT 28 / IXIT 13 INTERFACE MODEL", log)
        self.assertIn("RAW NMAP XML", log)
        self.assertEqual(1, sum(line.startswith("Open log file:") for line in self.lines))
        self.assertEqual("Overall verdict: PASS", self.lines[-1])
        self.assertEqual(8, sum(line.startswith("[Step ") for line in self.lines))
        self.assertIn("[Step 8 out of 8]", self.lines[-2])

    def test_missing_authoritative_mdns_declaration_stops_before_mutation(self) -> None:
        self.ixit_directory.joinpath("ixit_28-LogIntf.md").write_text(
            "Summary mentions LogIntf-mDNS-Responder but has no declaration.\n", encoding="utf-8",
        )
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["IXIT declaration prerequisite"])
        self.assertFalse(self.ntp_capture.started)
        self.assertEqual(1, len(self.nmap.calls))
        self.assertFalse(any(call.method == "POST" and call.path == "/ruuvi.json"
                             for call in self.gateway.calls))
        self.assertIn("IXIT documentation gap", self.evidence())

    def test_unstable_full_baseline_rejected_before_mutation(self) -> None:
        self.gateway.drift_on_read = 2
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertFalse(self.ntp_capture.started)
        self.assertFalse(any(call.method == "POST" and call.path == "/ruuvi.json"
                             for call in self.gateway.calls))
        self.assertIn("two full configuration reads are stable", self.evidence())

    def test_custom_mqtt_baseline_rejected_before_parser_can_reset_it(self) -> None:
        field: str
        for field in ("mqtt_data_format", "mqtt_prefix", "mqtt_client_id"):
            with self.subTest(field=field):
                self.gateway.fields = ScanGateway().fields
                self.gateway.fields[field] = "custom"
                self.gateway.calls.clear()
                result: RunResult = self.execute()
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual("custom", self.gateway.fields[field])
                self.assertFalse(any(call.method == "POST" and call.path == "/ruuvi.json"
                                     for call in self.gateway.calls))

    def test_unrelated_restoration_drift_is_error_with_full_recovery_advice(self) -> None:
        self.gateway.drift_on_read = 4
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertIn("all drifted settings", result.recovery_message or "")
        self.assertIn("full configuration differs", self.evidence())

    def test_unknown_http_state_is_error_not_absence_failure(self) -> None:
        compressed: bool
        for compressed in (False, True):
            with self.subTest(compressed=compressed):
                self.nmap.tcp_open.clear()
                self.nmap.tcp_filtered = compressed
                self.nmap.tcp_ambiguous = {} if compressed else {80: "filtered"}
                result: RunResult = self.execute()
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual("ERROR", result.outcomes["documented HTTP listener"])

    def test_confirmed_listener_failure_survives_later_confirmation_error(self) -> None:
        protocol: str
        for protocol in ("tcp", "udp"):
            with self.subTest(protocol=protocol):
                self.nmap.tcp_open = {80}
                self.nmap.udp_open = {5353}
                if protocol == "tcp":
                    self.nmap.tcp_open.update({2323, 2324})
                else:
                    self.nmap.udp_open.update({2323, 2324})
                self.nmap.fail_confirmation_port = 2324
                result: RunResult = self.execute()
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual("FAIL", result.outcomes[f"{protocol.upper()} undocumented listeners"])
                self.assertIn(f"2323/{protocol}", "\n".join(self.lines))
                self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])

    def test_lowercase_and_mixed_case_env_mac_preserve_default_gateway(self) -> None:
        spelling: str
        for spelling in ("aa:bb:cc:dd:ee:ff", "Aa:bB:Cc:dD:Ee:fF"):
            with self.subTest(spelling=spelling):
                self.directory.joinpath(".env").write_text(
                    f"gw_id={CONFIG.gw_id}\ngw_mac={spelling}\ngw_hostname={CONFIG.gw_hostname}\n",
                    encoding="utf-8",
                )
                baseline: dict[str, Any] = dict(self.gateway.fields)
                self.gateway.calls.clear()
                result: RunResult = self.execute()
                self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
                self.assertEqual("PASS", result.outcomes["factory-default preflight"])
                self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
                self.assertIsNone(result.recovery_message)
                self.assertEqual(baseline, self.gateway.fields)
                self.assertEqual([{"ntp_server1": "192.0.2.20"}, {"ntp_server1": "time.google.com"}],
                                 [call.body for call in self.gateway.calls if call.method == "POST"
                                  and call.path == "/ruuvi.json"])
                self.assertEqual(["GET", "POST", "GET", "GET", "POST"],
                                 [call.method for call in self.gateway.calls[:5]])

    def test_confirmed_mdns_matches_documented_interface(self) -> None:
        self.nmap.udp_open.clear()
        result: RunResult = self.execute()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["documented mDNS responder"])
        self.assertEqual("PASS", result.outcomes["UDP undocumented listeners"])
        self.assertIn("LogIntf-mDNS-Responder", self.evidence())

    def test_capture_failure_before_mutation_leaves_configuration_untouched(self) -> None:
        self.ntp_capture.fail_start = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertFalse(any(call.method == HttpMethod.POST and call.path == GatewayApi.CONFIG
                             for call in self.gateway.calls))
        self.assertEqual("NOT RUN", result.outcomes["NTP configuration restoration"])
        self.assertEqual(1, len(self.nmap.calls))

    def test_capture_failure_after_mutation_restores_default(self) -> None:
        self.ntp_capture.fail_collect = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertTrue(self.ntp_capture.stopped)
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertEqual([{"ntp_server1": "192.0.2.20"}, {"ntp_server1": "time.google.com"}],
                         [call.body for call in self.gateway.calls if call.method == HttpMethod.POST
                          and call.path == GatewayApi.CONFIG])

    def test_lost_ntp_change_response_still_restores_applied_change(self) -> None:
        self.gateway.lose_initial_ntp_response = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertTrue(self.ntp_capture.stopped)

    def test_failed_restoration_is_error_with_recovery_value(self) -> None:
        self.gateway.fail_ntp_restore = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["NTP configuration restoration"])
        self.assertEqual("192.0.2.20", self.gateway.fields["ntp_server1"])
        self.assertIn("Restore ntp_server1 to time.google.com", result.recovery_message or "")
        self.assertIn("ntp_server1 restoration ERROR", "\n".join(self.lines))

    def test_missing_documented_mdns_is_failure(self) -> None:
        self.nmap.udp_open.clear()
        self.mdns = FakeMdnsSocket(False)
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["documented mDNS responder"])
        self.assertIn("no direct DNS-SD answer and 5353/udp closed", "\n".join(self.lines))

    def test_extra_tcp_listener_is_confirmed_and_retained_after_udp_error(self) -> None:
        self.nmap.tcp_open.add(2323)
        self.nmap.fail_udp = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["TCP undocumented listeners"])
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertIn("2323/tcp", self.evidence())
        self.assertEqual("2323", self.nmap.calls[2][self.nmap.calls[2].index("-p") + 1])

    def test_unstable_tcp_open_is_error_without_confirmed_finding(self) -> None:
        self.nmap.tcp_open.add(2323)
        self.nmap.unstable_tcp = True
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertEqual("ERROR", result.outcomes["TCP undocumented listeners"])
        self.assertNotIn("undocumented listener FAIL: 2323/tcp", self.evidence())

    def test_udp_open_is_confirmed(self) -> None:
        self.nmap.udp_open.add(9999)
        self.mdns = FakeMdnsSocket(True)
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["UDP undocumented listeners"])
        self.assertIn("9999/udp", self.evidence())

    def test_missing_http_is_failure_even_with_mdns_answer(self) -> None:
        self.nmap.tcp_open.clear()
        self.mdns = FakeMdnsSocket(True)
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["documented HTTP listener"])

    def test_open_filtered_udp_port_prevents_complete_coverage(self) -> None:
        self.nmap.udp_ambiguous[10000] = "open|filtered"
        self.mdns = FakeMdnsSocket(True)
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertEqual("PASS", result.outcomes["documented mDNS responder"])
        self.assertIn("1 open|filtered and 1 closed|filtered ports in the full scan", self.evidence())
        self.assertEqual(3, len(self.nmap.calls))
        self.assertIn("10000/udp", "\n".join(self.lines))

    def test_closed_filtered_udp_ports_prevent_complete_coverage(self) -> None:
        self.nmap.udp_ambiguous[10000] = "closed|filtered"
        self.nmap.udp_ambiguous[10001] = "closed|filtered"
        self.nmap.udp_open.add(5353)
        self.nmap.reject_mdns_nmap_confirmation = True
        self.mdns = FakeMdnsSocket(True)
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["UDP undocumented listeners"])
        self.assertEqual("PASS", result.outcomes["documented mDNS responder"])
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertNotIn("undocumented listener FAIL: 5353/udp", self.evidence())
        self.assertIn("3 closed|filtered ports in the full scan", self.evidence())
        self.assertEqual(3, len(self.nmap.calls))
        self.assertIn(b"_services\x07_dns-sd", self.mdns.request)

    def test_undeclared_udp_response_is_retained_with_incomplete_coverage(self) -> None:
        self.nmap.udp_open.add(9999)
        self.nmap.udp_ambiguous[10000] = "closed|filtered"
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["UDP undocumented listeners"])
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertIn("9999/udp", self.evidence())

    def test_unresolved_full_scan_is_error(self) -> None:
        self.nmap.udp_ambiguous[9999] = "closed|filtered"
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertIn("1 ports remain indeterminate", self.evidence())
        self.assertIn("UDP coverage ERROR: 1 port", "\n".join(self.lines))
        self.assertIn("9999/udp", "\n".join(self.lines))
        self.assertIn("UDP UNRESOLVED PORTS", self.evidence())
        self.assertIn('"state": "closed|filtered"', self.evidence())

    def test_direct_mdns_answer_resolves_indeterminate_5353(self) -> None:
        self.nmap.udp_open.clear()
        self.nmap.udp_ambiguous[5353] = "closed|filtered"
        result: RunResult = self.execute()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["documented mDNS responder"])
        self.assertEqual("PASS", result.outcomes["UDP coverage"])
        self.assertIn("1 resolved by direct mDNS response", self.evidence())

    def test_broad_scan_open_5353_without_direct_answer_remains_unverified(self) -> None:
        self.nmap.udp_open.add(5353)
        self.nmap.reject_mdns_nmap_confirmation = True
        self.mdns = FakeMdnsSocket(False)
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["UDP undocumented listeners"])
        self.assertEqual("ERROR", result.outcomes["documented mDNS responder"])
        self.assertEqual(3, len(self.nmap.calls))
        self.assertIn("UDP state is unverified", "\n".join(self.lines))
        self.assertNotIn("undocumented listener FAIL: 5353/udp", self.evidence())

    def test_compressed_tcp_filtered_group_prevents_complete_comparison(self) -> None:
        self.nmap.tcp_filtered = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["TCP coverage"])
        self.assertEqual(3, len(self.nmap.calls))
        self.assertIn("TCP COMPRESSED PORT ID LIMITATION", self.evidence())
        self.assertIn("TCP coverage ERROR: 1 filtered port", "\n".join(self.lines))

    def test_tcp_filtered_group_does_not_mask_udp_coverage_limitation(self) -> None:
        self.nmap.tcp_filtered = True
        self.nmap.udp_filtered = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["TCP coverage"])
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])

    def test_udp_filtered_group_alone_prevents_pass(self) -> None:
        self.nmap.udp_filtered = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["TCP coverage"])
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])

    def test_extra_open_tcp_with_compressed_filtered_group_is_confirmed(self) -> None:
        self.nmap.tcp_filtered = True
        self.nmap.tcp_open.add(2323)
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["TCP undocumented listeners"])
        self.assertEqual("ERROR", result.outcomes["TCP coverage"])
        self.assertIn("2323/tcp", self.evidence())
        self.assertEqual("2323", self.nmap.calls[2][self.nmap.calls[2].index("-p") + 1])

    def test_missing_and_old_nmap_or_unresolvable_hostname_stop_before_scan(self) -> None:
        for condition in ("missing", "old", "resolve"):
            with self.subTest(condition=condition):
                self.nmap.calls.clear()
                if condition == "old":
                    self.nmap.version = "Nmap version 7.39"
                result: RunResult = self.execute(
                    find_executable=condition != "missing", resolvable=condition != "resolve",
                )
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertLessEqual(len(self.nmap.calls), 1)
                self.nmap.version = "Nmap version 7.95"

    def test_each_default_field_uses_type_and_value_and_rejects_missing(self) -> None:
        defaults: dict[str, Any] = ScanGateway().fields
        field: str
        value: Any
        for field, value in defaults.items():
            unequal: Any = (
                {**value, "unexpected": True} if isinstance(value, dict)
                else not value if isinstance(value, bool) else "different"
            )
            alternatives: tuple[Any, Any] = (unequal, 0 if isinstance(value, bool) else None)
            alternative: Any
            for alternative in alternatives:
                with self.subTest(field=field, alternative=alternative):
                    self.gateway.fields = dict(defaults)
                    self.gateway.fields[field] = alternative
                    self.nmap.calls.clear()
                    result: RunResult = self.execute()
                    self.assertEqual("ERROR", result.verdict)
                    self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
                    self.assertEqual("NOT RUN", result.outcomes["documented HTTP listener"])
                    self.assertEqual(1, len(self.nmap.calls))
            self.gateway.fields = dict(defaults)
            del self.gateway.fields[field]
            self.nmap.calls.clear()
            result = self.execute()
            self.assertEqual("ERROR", result.verdict)
            self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
            self.assertEqual(1, len(self.nmap.calls))
            self.gateway.fields = dict(defaults)
            self.nmap.calls.clear()
            result = self.execute()
            self.assertEqual("PASS", result.verdict)
        self.gateway.fields = dict(defaults)

    def test_nested_bool_integer_trap_and_absent_identity(self) -> None:
        self.gateway.fields["wifi_ap_config"] = {"channel": True}
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
        self.gateway.include_mac = False
        result = self.execute()
        self.assertIsNone(result.recovery_message)

    def test_http_fingerprint_rejection(self) -> None:
        self.nmap.tcp_http_service = "unknown"
        result: RunResult = self.execute()
        self.assertEqual("FAIL", result.verdict)
        self.assertEqual("FAIL", result.outcomes["documented HTTP listener"])

    def test_bad_baseline_and_identity_stop_before_scan(self) -> None:
        self.gateway.fields[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = 0
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertEqual(1, len(self.nmap.calls))
        self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
        self.gateway.mac = "11:22:33:44:55:66"
        result = self.execute()
        self.assertIsNone(result.recovery_message)

    def test_nondefault_auth_mode_and_login_denial_retain_recovery(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            self.gateway.mode = mode
            self.nmap.calls.clear()
            result: RunResult = self.execute()
            self.assertEqual("ERROR", result.verdict)
            self.assertEqual(1, len(self.nmap.calls))
            self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
        self.gateway.mode = GatewayCfgLanAuthType.DEFAULT
        self.gateway.login_status = 401
        result = self.execute()
        self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")

    def test_malformed_identity_blocks_reset_advice_before_default_checks(self) -> None:
        self.gateway.mac = "not-a-mac"
        self.gateway.fields[GatewayCfgDesc.LAN_AUTH_USER] = "changed"
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertIsNone(result.recovery_message)
        self.assertEqual(1, len(self.nmap.calls))
        self.gateway.mode = GatewayCfgLanAuthType.RUUVI
        self.nmap.calls.clear()
        result = self.execute()
        self.assertIsNone(result.recovery_message)
        self.assertEqual(1, len(self.nmap.calls))

    def test_udp_timeout_and_incomplete_xml_are_errors(self) -> None:
        for failure in ("timeout", "incomplete"):
            with self.subTest(failure=failure):
                self.nmap.fail_udp = failure == "timeout"
                self.nmap.incomplete_udp = failure == "incomplete"
                self.nmap.calls.clear()
                result: RunResult = self.execute()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("PASS", result.outcomes["documented HTTP listener"])
                self.assertEqual("ERROR", result.outcomes["UDP coverage"])
                self.assertEqual(3, len(self.nmap.calls))
                if failure == "incomplete":
                    logs: list[Path] = sorted(self.directory.joinpath("logs").glob("test_5_5_5_2_b_*.log"))
                    latest: str = logs[-1].read_text(encoding="utf-8")
                    self.assertIn("NMAP RAW XML BEFORE PARSING", latest)
                    self.assertIn('finished exit="error"', latest)

    def test_missing_privilege_is_error_before_udp_command(self) -> None:
        result: RunResult = self.execute(privilege=False)
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(1, len(self.nmap.calls))
        self.assertEqual([], self.gateway.calls)
        self.assertIn("CAP_NET_RAW", self.evidence())
        self.assertIn("sudo .venv/bin/python test_5_5_5_2_b.py", self.lines[-2])
        self.assertIn("sudo apt install libcap2-bin", self.lines[-2])
        self.assertIn("sudo setcap cap_net_raw+ep /usr/bin/python3.8", self.lines[-2])

    def test_nmap_udp_privilege_probe_fails_before_dut_requests(self) -> None:
        self.nmap.deny_udp_probe = True
        result: RunResult = self.execute(privilege=None)
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(2, len(self.nmap.calls))
        self.assertIn("--privileged", self.nmap.calls[1])
        self.assertEqual("127.0.0.1", self.nmap.calls[1][-1])
        self.assertEqual([], self.gateway.calls)
        self.assertIn("requires root privileges", self.lines[-2])
        self.assertIn("Nmap UDP privilege probe exited 1", self.evidence())


if __name__ == "__main__":
    unittest.main()
