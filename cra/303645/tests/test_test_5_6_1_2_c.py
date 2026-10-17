"""Offline runner checks for 5.6-1-2 c; these do not establish DUT compliance."""

from __future__ import annotations

import hashlib
import json
import socket
import subprocess
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping
from urllib.parse import urlsplit

import requests

import test_5_6_1_2_c as target
from lib.config import FACTORY_RESET_MESSAGE, default_config_values
from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import HttpMethod
from lib.models import DutConfig, RunResult
from lib.netscan import DatagramSocket
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 23, 5, 0, tzinfo=timezone.utc)


class ScanGateway(DefaultAuthGateway):
    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.fields: dict[str, Any] = json.loads(json.dumps(default_config_values(target.DEFAULT_FIELDS)))
        self.mac: Any = CONFIG.gw_mac
        self.include_mac: bool = True
        self.login_status: int | None = None
        self.fail_restore: bool = False
        self.side_effect: bool = False
        self.lose_change_response: bool = False

    def _config_response(self) -> FakeResponse:
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
        value: str | None = body.get("ntp_server1")
        if value is not None and not (self.fail_restore and value == "time.google.com"):
            self.fields["ntp_server1"] = value
        if self.side_effect and value == "192.0.2.20":
            self.fields["use_mqtt"] = True

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        if self.lose_change_response and call.method == HttpMethod.POST and call.body == {
            "ntp_server1": "192.0.2.20",
        }:
            self.lose_change_response = False
            self._apply_config(call.body)
            raise requests.Timeout("response lost after change applied")
        return super()._response_override(session, call)


class FakeNmap:
    def __init__(self) -> None:
        self.calls: list[tuple[str, ...]] = []
        self.tcp_open: set[int] = {80}
        self.udp_open: set[int] = {5353}
        self.udp_ambiguous: dict[int, str] = {}
        self.targeted_states: dict[int, str] = {}
        self.tcp_filtered: bool = False
        self.fail_udp: bool = False
        self.incomplete_udp: bool = False
        self.version: str = "Nmap version 7.95 ( https://nmap.org )"
        self.http_service: str = "http"
        self.unstable_tcp: bool = False

    def __call__(self, command: list[str], **kwargs: Any) -> subprocess.CompletedProcess[str]:
        self.calls.append(tuple(command))
        if command[-1] == "--version":
            return subprocess.CompletedProcess(command, 0, stdout=self.version, stderr="")
        if "-oX" in command and command[command.index("-oX") + 1] == "-":
            return subprocess.CompletedProcess(command, 0, stdout="<nmaprun/>", stderr="")
        protocol: str = "tcp" if "-sT" in command else "udp"
        if protocol == "udp" and self.fail_udp:
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])
        specification: str = command[command.index("-p") + 1]
        full: bool = specification == target.PORTS
        ports: set[int] = set(range(1, 65536)) if full else {int(specification)}
        open_ports: set[int] = (self.tcp_open if protocol == "tcp" else self.udp_open) & ports
        if protocol == "tcp" and not full and self.unstable_tcp:
            open_ports = set()
        ambiguous: dict[int, str] = (
            {port: state for port, state in self.udp_ambiguous.items() if port in ports and port not in open_ports}
            if protocol == "udp" and full else {}
        )
        if protocol == "udp" and not full:
            port: int = int(specification)
            state: str = self.targeted_states.get(port, "open" if port in open_ports else "closed")
            open_ports = {port} if state == "open" else set()
            ambiguous = {port: state} if state in {"open|filtered", "closed|filtered", "filtered"} else {}
        remaining: set[int] = ports - open_ports - ambiguous.keys()
        explicit: str = "".join(
            f'<port protocol="{protocol}" portid="{port}"><state state="open" reason="response"/>'
            f'<service name="{self.http_service if protocol == "tcp" and port == 80 else "unknown"}"/></port>'
            for port in sorted(open_ports)
        )
        explicit += "".join(
            f'<port protocol="udp" portid="{port}"><state state="{state}" reason="no-response"/></port>'
            for port, state in sorted(ambiguous.items())
        )
        extras: str = f'<extraports state="closed" count="{len(remaining)}"/>' if remaining else ""
        if protocol == "tcp" and full and self.tcp_filtered:
            extras = f'<extraports state="closed" count="{len(remaining) - 1}"/><extraports state="filtered" count="1"/>'
        xml: str = (
            f'<nmaprun><scaninfo protocol="{protocol}" services="{specification}" '
            f'numservices="{len(ports)}"/><host><status state="up"/><ports>{extras}{explicit}'
            f'</ports></host><runstats><finished exit="{"error" if protocol == "udp" and self.incomplete_udp else "success"}"/>'
            '</runstats></nmaprun>'
        )
        Path(command[command.index("-oX") + 1]).write_text(xml, encoding="utf-8")
        return subprocess.CompletedProcess(command, 0, stdout="", stderr="")


class FakeMdnsSocket(DatagramSocket):
    def __init__(self) -> None:
        self.answer: bool = True
        self.destination: tuple[str, int] | None = None
        self.closed: bool = False

    def settimeout(self, timeout: float) -> None:
        if timeout != target.MDNS_TIMEOUT:
            raise AssertionError("wrong mDNS timeout")

    def sendto(self, data: bytes, address: tuple[str, int]) -> int:
        self.destination = address
        return len(data)

    def recvfrom(self, size: int) -> tuple[bytes, tuple[str, int]]:
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
        self.source_port: int = 62922
        self.started: bool = False
        self.stopped: bool = False
        self.fail_start: bool = False
        self.fail_collect: bool = False
        self.dut_ip: str = ""
        self.host_ip: str = ""

    def start(self, dut_ip: str, host_ip: str) -> tuple[str, ...]:
        if self.fail_start:
            raise target.ScanError("tcpdump did not start")
        if self.gateway.fields["ntp_server1"] != "time.google.com":
            raise AssertionError("capture must start before changing ntp_server1")
        self.started = True
        self.dut_ip = dut_ip
        self.host_ip = host_ip
        return "/usr/bin/tcpdump", "-i", "any", "-nn", "-c", "1"

    def collect(self, timeout: float) -> target.NtpCaptureResult:
        if timeout != target.NTP_REQUEST_TIMEOUT:
            raise AssertionError("wrong NTP capture timeout")
        if self.gateway.fields["ntp_server1"] != self.host_ip:
            raise AssertionError("capture must follow the NTP configuration change")
        if self.fail_collect:
            raise target.ScanError("no NTP request captured")
        return target.NtpCaptureResult(
            self.source_port,
            f"IP {self.dut_ip}.{self.source_port} > {self.host_ip}.123: NTPv4, Client", "",
        )

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
        body: Any = json.loads(request.body) if request.body else None
        self.wire_calls.append(WireRequest(
            urlsplit(url).scheme, request.method or "", urlsplit(url).path, body,
            self.number, kwargs.get("allow_redirects"), request.headers.get("Cookie"),
        ))
        return super().send(request, **kwargs)


class NetworkInterfaceRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory: Path = Path(self.temp.name)
        self.directory.joinpath(".env").write_text(
            f"gw_id={CONFIG.gw_id}\ngw_mac={CONFIG.gw_mac}\ngw_hostname={CONFIG.gw_hostname}\n",
            encoding="utf-8",
        )
        self.gateway: ScanGateway = ScanGateway()
        self.nmap: FakeNmap = FakeNmap()
        self.nmap.udp_ambiguous[62922] = "closed|filtered"
        self.mdns: FakeMdnsSocket = FakeMdnsSocket()
        self.ntp_capture: FakeNtpCapture = FakeNtpCapture(self.gateway)
        self.lines: list[str] = []
        self.wire_calls: list[WireRequest] = []

    def execute(self, *, privilege: bool = True, executable: bool = True,
                resolvable: bool = True) -> RunResult:
        def resolve(_host: str, _port: int | None, **_kwargs: Any) -> list[tuple[int, int, int, str, tuple[str, int]]]:
            if not resolvable:
                raise OSError("name not found")
            return [(socket.AF_INET, socket.SOCK_STREAM, 6, "", ("192.0.2.10", 0))]

        return target.execute_test_5_6_1_2_c(
            self.directory, session_factory=lambda: ScanSession(self.gateway, self.wire_calls),
            runner=self.nmap, resolver=resolve, socket_factory=lambda family, kind: self.mdns,
            find_executable=lambda name: "/usr/bin/nmap" if executable else None,
            local_ip=lambda dut_ip: "192.0.2.20", capture_factory=lambda: self.ntp_capture,
            privilege=lambda: privilege, clock=lambda: 1.0, now=lambda: NOW, output=self.lines.append,
        )

    def evidence(self) -> str:
        logs: list[Path] = list(self.directory.joinpath("logs").glob("test_5_6_1_2_c_*.log"))
        self.assertEqual(1, len(logs))
        return logs[0].read_text(encoding="utf-8")

    def test_clean_full_scan_and_exact_authenticated_order(self) -> None:
        self.assertEqual("1-65535", target.PORTS)
        self.assertEqual((7, 40), target.NMAP_MIN_VERSION)
        self.assertGreater(target.UDP_HOST_TIMEOUT, target.TCP_HOST_TIMEOUT)
        self.assertEqual(1, target.MAX_RETRIES)
        self.assertEqual(16, len(target.EXPECTED_INTERFACES))
        self.assertEqual(16, len({entry.ixit_28 for entry in target.EXPECTED_INTERFACES}))
        result: RunResult = self.execute()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.assertEqual({"PASS"}, set(result.outcomes.values()))
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        password_response: str = hashlib.sha256(f"challenge-1-1:{ha1}".encode()).hexdigest()
        restore_response: str = hashlib.sha256(f"challenge-2-1:{ha1}".encode()).hexdigest()
        self.assertEqual([
            WireRequest("http", "GET", "/auth", None, 1, False, None),
            WireRequest("http", "POST", "/auth", {"login": "Admin", "password": password_response},
                        1, False, "RUUVISESSION=session-1"),
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
        self.assertEqual([("tcp", target.PORTS), ("udp", target.PORTS)], [
            ("tcp" if "-sT" in call else "udp", call[call.index("-p") + 1])
            for call in self.nmap.calls if "-p" in call
        ])
        tcp_command: tuple[str, ...] = self.nmap.calls[1]
        udp_command: tuple[str, ...] = self.nmap.calls[2]
        self.assertTrue({"-sT", "-Pn", "-n", "--reason", "-sV", "-oX"}.issubset(tcp_command))
        self.assertTrue({"-sU", "-Pn", "-n", "--reason", "-sV", "--defeat-icmp-ratelimit",
                         "-oX"}.issubset(udp_command))
        self.assertEqual(f"{target.UDP_HOST_TIMEOUT}s", udp_command[udp_command.index("--host-timeout") + 1])
        self.assertEqual(f"{target.MAX_RETRIES:d}", udp_command[udp_command.index("--max-retries") + 1])
        self.assertEqual(("192.0.2.10", 5353), self.mdns.destination)
        self.assertTrue(self.mdns.closed)
        self.assertTrue(self.ntp_capture.started)
        self.assertTrue(self.ntp_capture.stopped)
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        evidence: str = self.evidence()
        self.assertIn("EXPECTED IXIT 28 / IXIT 15 INTERFACE MODEL", evidence)
        self.assertIn("hotspot DNS/DHCP servers on UDP 53/67/68 inactive", evidence)
        self.assertIn("FINAL CHECK RESULT", evidence)
        self.assertIn('"only_sntp_source_is_closed_filtered": true', evidence)
        self.assertIn('"full_baseline_verified": true', evidence)
        self.assertIn("OVERALL VERDICT: PASS", evidence)
        self.assertEqual(12, sum(line.startswith("[Step ") for line in self.lines))
        self.assertIn("Overall verdict: PASS", self.lines)

    def test_unexpected_tcp_listener_requires_confirmation(self) -> None:
        self.nmap.tcp_open.add(22)
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["TCP undocumented listeners"])
        self.assertEqual("FAIL", result.outcomes["custom telemetry disabled"])
        self.assertIn("undocumented listener FAIL: 22/tcp", "\n".join(self.lines))
        self.assertIn("-p", self.nmap.calls[2])
        self.assertEqual("22", self.nmap.calls[2][self.nmap.calls[2].index("-p") + 1])
        self.assertIn("TCP CONFIRMATION 22", self.evidence())

    def test_udp_listener_and_hotspot_violation(self) -> None:
        self.nmap.udp_open.update({53, 9999})
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["UDP undocumented listeners"])
        self.assertEqual("FAIL", result.outcomes["hotspot servers disabled"])
        self.assertIn("transient hotspot server active", self.evidence())
        self.assertIn("9999/udp", "\n".join(self.lines))

    def test_required_listener_failures_and_mdns_ambiguity(self) -> None:
        self.nmap.tcp_open.clear()
        self.nmap.udp_open.clear()
        self.mdns.answer = False
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["documented HTTP listener"])
        self.assertEqual("FAIL", result.outcomes["documented mDNS responder"])

    def test_mdns_nonanswer_with_ambiguous_port_is_error(self) -> None:
        self.nmap.udp_open.clear()
        self.nmap.udp_ambiguous[5353] = "open|filtered"
        self.mdns.answer = False
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["documented mDNS responder"])
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])

    def test_mdns_answer_contradicting_closed_scan_is_error(self) -> None:
        self.nmap.udp_open.clear()
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["documented mDNS responder"])

    def test_additional_udp_ambiguity_and_hotspot_ambiguity_are_errors(self) -> None:
        self.nmap.udp_ambiguous[64053] = "closed|filtered"
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertIn("64053", self.evidence())
        self.nmap.udp_ambiguous[53] = "open|filtered"
        self.nmap.targeted_states[53] = "open|filtered"
        result = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["hotspot servers disabled"])

    def test_ambiguous_hotspot_port_confirmed_open_is_finding(self) -> None:
        self.nmap.udp_ambiguous[67] = "open|filtered"
        self.nmap.targeted_states[67] = "open"
        result: RunResult = self.execute()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["hotspot servers disabled"])
        self.assertEqual("FAIL", result.outcomes["UDP undocumented listeners"])

    def test_preconditions_abort_before_scan(self) -> None:
        scenario: str
        for scenario in ("missing", "old", "privilege", "resolve", "bad_default", "bad_mac",
                         "malformed_mac", "login_denied", "basic", "digest"):
            with self.subTest(scenario=scenario):
                self.setUp()
                if scenario == "old":
                    self.nmap.version = "Nmap version 7.39"
                if scenario == "bad_default":
                    self.gateway.fields["use_mqtt"] = True
                if scenario == "bad_mac":
                    self.gateway.mac = "01:02:03:04:05:06"
                if scenario == "malformed_mac":
                    self.gateway.mac = "invalid"
                if scenario == "login_denied":
                    self.gateway.login_status = 401
                if scenario in {"basic", "digest"}:
                    self.gateway.mode = (
                        GatewayCfgLanAuthType.BASIC if scenario == "basic" else GatewayCfgLanAuthType.DIGEST
                    )
                result: RunResult = self.execute(
                    executable=scenario != "missing", privilege=scenario != "privilege",
                    resolvable=scenario != "resolve",
                )
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertFalse(any("-sT" in call for call in self.nmap.calls))
                if scenario in {"bad_default", "basic", "digest", "login_denied"}:
                    self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
                    self.assertIn("USER ACTION REQUIRED", self.evidence())
                if scenario in {"bad_mac", "malformed_mac"}:
                    self.assertIsNone(result.recovery_message)

    def test_unverified_mac_does_not_request_factory_reset(self) -> None:
        self.gateway.include_mac = False
        self.gateway.fields["use_mqtt"] = True
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertIsNone(result.recovery_message)

    def test_each_required_default_rejects_missing_and_wrong_typed_values(self) -> None:
        defaults: dict[str, Any] = default_config_values(target.DEFAULT_FIELDS)
        field: str
        expected: Any
        variant: str
        for field, expected in defaults.items():
            for variant in ("missing", "unequal", "wrong_type"):
                with self.subTest(field=field, variant=variant):
                    self.setUp()
                    if variant == "missing":
                        del self.gateway.fields[field]
                    elif variant == "wrong_type":
                        self.gateway.fields[field] = int(expected) if isinstance(expected, bool) else True
                    else:
                        self.gateway.fields[field] = not expected if isinstance(expected, bool) else f"{expected}-other"
                    result: RunResult = self.execute()
                    self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                    self.assertFalse(any("-sT" in call for call in self.nmap.calls))
                    self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")

    def test_earlier_finding_survives_later_scan_error(self) -> None:
        self.nmap.tcp_open.add(23)
        self.nmap.fail_udp = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["TCP undocumented listeners"])
        self.assertIn("23/tcp", "\n".join(self.lines))
        self.assertIn('"result": "FAIL"', self.evidence())
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])

    def test_capture_failure_still_restores_applied_change(self) -> None:
        self.ntp_capture.fail_collect = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertTrue(self.ntp_capture.stopped)
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertFalse(any("-sU" in call and target.PORTS in call for call in self.nmap.calls))

    def test_capture_start_failure_never_changes_configuration(self) -> None:
        self.ntp_capture.fail_start = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("NOT RUN", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])
        self.assertFalse(self.gateway.config_bodies)

    def test_lost_change_response_restores_possible_mutation(self) -> None:
        self.gateway.lose_change_response = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertEqual("time.google.com", self.gateway.fields["ntp_server1"])

    def test_restoration_failure_reports_operator_recovery(self) -> None:
        self.gateway.fail_restore = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["NTP configuration restoration"])
        self.assertIn("Restore ntp_server1", result.recovery_message or "")
        self.assertIn("USER ACTION REQUIRED", self.evidence())
        self.assertIn("USER ACTION REQUIRED", "\n".join(self.lines))

    def test_change_side_effect_is_detected_and_unrestored_state_is_error(self) -> None:
        self.gateway.side_effect = True
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["NTP configuration restoration"])
        self.assertIn("full GET /ruuvi.json", result.recovery_message or "")

    def test_captured_port_must_match_sole_closed_filtered_port(self) -> None:
        self.ntp_capture.source_port = 62923
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertIn('"captured_sntp_source_port": 62923', self.evidence())

    def test_timeout_incomplete_and_unstable_findings_are_errors(self) -> None:
        scenario: str
        for scenario in ("timeout", "incomplete", "unstable"):
            with self.subTest(scenario=scenario):
                self.setUp()
                if scenario == "timeout":
                    self.nmap.fail_udp = True
                elif scenario == "incomplete":
                    self.nmap.incomplete_udp = True
                else:
                    self.nmap.tcp_open.add(23)
                    self.nmap.unstable_tcp = True
                result: RunResult = self.execute()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertIn("EXCEPTION TYPE", self.evidence())


if __name__ == "__main__":
    unittest.main()
