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
from typing import Any, Callable, Mapping, cast
from unittest.mock import MagicMock, patch
from urllib.parse import urlsplit

import requests

import test_5_6_1_2_c as target
from lib.config import FACTORY_RESET_MESSAGE, default_config_values
from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient
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
        self.fields.update({"mqtt_prefix": f"ruuvi/{CONFIG.gw_mac}/", "mqtt_client_id": CONFIG.gw_mac})
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
        # gw_cfg_json_parse_mqtt resets these on every partial POST.
        self.fields.update({
            "mqtt_data_format": body.get("mqtt_data_format", "ruuvi_raw"),
            "mqtt_prefix": body.get("mqtt_prefix") or f"ruuvi/{CONFIG.gw_mac}/",
            "mqtt_client_id": body.get("mqtt_client_id") or CONFIG.gw_mac,
        })
        if self.fields["scan_default"]:
            self.fields.update(default_config_values((
                "scan_default", "scan_coded_phy", "scan_1mbit_phy", "scan_2mbit_phy",
                "scan_channel_37", "scan_channel_38", "scan_channel_39",
            )))
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
        self.http_filtered: bool = False
        self.fail_udp: bool = False
        self.incomplete_udp: bool = False
        self.version: str = "Nmap version 7.95 ( https://nmap.org )"
        self.http_service: str = "http"
        self.unstable_tcp: bool = False
        self.failed_confirmation: tuple[str, int] | None = None

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
        if not full and self.failed_confirmation == (protocol, int(specification)):
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])
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
        if protocol == "tcp" and self.http_filtered:
            open_ports.discard(80)
            ambiguous[80] = "filtered"
        remaining: set[int] = ports - open_ports - ambiguous.keys()
        explicit: str = "".join(
            f'<port protocol="{protocol}" portid="{port}"><state state="open" reason="response"/>'
            f'<service name="{self.http_service if protocol == "tcp" and port == 80 else "unknown"}"/></port>'
            for port in sorted(open_ports)
        )
        explicit += "".join(
            f'<port protocol="{protocol}" portid="{port}"><state state="{state}" reason="no-response"/></port>'
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
    def __init__(self, gateway: ScanGateway, wire_calls: list[WireRequest], wire_hosts: list[str]) -> None:
        super().__init__(gateway)
        self.wire_calls: list[WireRequest] = wire_calls
        self.wire_hosts: list[str] = wire_hosts

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        url: str = request.url or ""
        self.wire_hosts.append(urlsplit(url).hostname or "")
        body: Any = json.loads(request.body) if request.body else None
        self.wire_calls.append(WireRequest(
            urlsplit(url).scheme, request.method or "", urlsplit(url).path, body,
            self.number, bool(kwargs["allow_redirects"]), request.headers.get("Cookie"),
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
        self.ixit_directory: Path = self.directory / "ixit"
        self.ixit_directory.mkdir()
        # Preserve all actual declarations; only this offline fixture adds the pending mDNS entry.
        self.ixit_text: str = Path(__file__).resolve().parents[1].joinpath("ixit_28-LogIntf.md").read_text(encoding="utf-8")
        self.ixit_text = self.ixit_text.replace(
            "## Summary Matrix for the Technical File",
            "### **ID**: LogIntf-mDNS-Responder\n\n"
            "#### Description\n\nInbound mDNS responder, UDP 5353, on LAN/WLAN.\n\n"
            "#### Access\n\n* `PhyIntf-Ethernet` (LAN)\n* `PhyIntf-WiFi` (WLAN)\n\n"
            "#### Status\n\nEnabled.\n\n## Summary Matrix for the Technical File",
        )
        self.ixit_text += "\n| `LogIntf-mDNS-Responder` | Server / 5353 | mDNS / UDP | Yes (LAN / WLAN) | Enabled | None |\n"
        (self.ixit_directory / "ixit_28-LogIntf.md").write_text(self.ixit_text, encoding="utf-8")
        self.gateway: ScanGateway = ScanGateway()
        self.nmap: FakeNmap = FakeNmap()
        self.nmap.udp_ambiguous[62922] = "closed|filtered"
        self.mdns: FakeMdnsSocket = FakeMdnsSocket()
        self.ntp_capture: FakeNtpCapture = FakeNtpCapture(self.gateway)
        self.lines: list[str] = []
        self.wire_calls: list[WireRequest] = []
        self.wire_hosts: list[str] = []

    def execute(self, *, privilege: bool = True, executable: bool = True,
                resolvable: bool = True, addresses: tuple[str, ...] = ("192.0.2.10",)) -> RunResult:
        def resolve(_host: str, _port: int | None, **_kwargs: Any) -> list[tuple[int, int, int, str, tuple[str, int]]]:
            if not resolvable:
                raise OSError("name not found")
            return [(socket.AF_INET, socket.SOCK_STREAM, 6, "", (address, 0)) for address in addresses]

        return target.execute_test_5_6_1_2_c(
            self.directory, session_factory=lambda: ScanSession(self.gateway, self.wire_calls, self.wire_hosts),
            runner=self.nmap, resolver=resolve, socket_factory=lambda family, kind: self.mdns,
            find_executable=lambda name: "/usr/bin/nmap" if executable else None,
            local_ip=lambda dut_ip: "192.0.2.20", capture_factory=lambda: self.ntp_capture,
            privilege=lambda: privilege, clock=lambda: 1.0, ixit_directory=self.ixit_directory,
            now=lambda: NOW, output=self.lines.append,
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
        before: dict[str, Any] = dict(self.gateway.fields)
        result: RunResult = self.execute()
        self.assertEqual(before, self.gateway.fields)
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
        self.assertIn("COVERAGE LIMITATION", evidence)
        self.assertIn("LogIntf-WiFi-WPS-Client", evidence)
        self.assertIn("NOT OBSERVED / OUT OF SCOPE", evidence)
        self.assertIn("WPS inactivity is unverified", evidence)
        self.assertIn("FINAL CHECK RESULT", evidence)
        self.assertIn('"only_sntp_source_is_closed_filtered": true', evidence)
        self.assertIn('"full_baseline_verified": true', evidence)
        self.assertIn("OVERALL VERDICT: PASS", evidence)
        self.assertEqual(12, sum(line.startswith("[Step ") for line in self.lines))
        self.assertIn("Overall verdict: PASS", self.lines)

    def test_env_mac_case_does_not_change_device_derived_mqtt_defaults(self) -> None:
        spelling: str
        for spelling in (CONFIG.gw_mac.lower(), "Aa:bB:Cc:dD:eE:Ff"):
            with self.subTest(spelling=spelling):
                self.setUp()
                self.directory.joinpath(".env").write_text(
                    f"gw_id={CONFIG.gw_id}\ngw_mac={spelling}\ngw_hostname={CONFIG.gw_hostname}\n",
                    encoding="utf-8",
                )
                before: dict[str, Any] = dict(self.gateway.fields)
                result: RunResult = self.execute()
                self.assertEqual("PASS", result.verdict)
                self.assertIsNone(result.recovery_message)
                self.assertEqual(before, self.gateway.fields)

    def test_multiple_dns_addresses_keep_http_and_scans_on_verified_target(self) -> None:
        # An independently resolved hostname could select the second, unverified device.
        # The HTTP transport must receive the selected literal and never resolve that name.
        client_factory: MagicMock
        with patch.object(target.socket, "getaddrinfo", side_effect=AssertionError("unexpected DNS lookup")), \
                patch("test_5_6_1_2_c.GatewayClient", wraps=GatewayClient) as client_factory:
            result: RunResult = self.execute(addresses=("192.0.2.10", "192.0.2.99"))
        client_factory.assert_called_once()
        self.assertEqual("http://192.0.2.10", client_factory.call_args[0][0].base_url)
        self.assertEqual(CONFIG.gw_mac, client_factory.call_args[0][0].gw_mac)
        self.assertEqual("PASS", result.verdict)
        self.assertEqual(["192.0.2.10"] * 10, self.wire_hosts)
        commands: list[tuple[str, ...]] = [call for call in self.nmap.calls if "-p" in call]
        self.assertEqual(2, len(commands))
        self.assertEqual(["192.0.2.10"] * 2, [call[-1] for call in commands])
        self.assertEqual("192.0.2.10", self.ntp_capture.dut_ip)
        self.assertEqual(("192.0.2.10", 5353), self.mdns.destination)
        self.assertIn('"configured_hostname": "gateway.local"', self.evidence())
        self.assertIn('"base_url": "http://192.0.2.10"', self.evidence())
        self.assertIn("192.0.2.99", self.evidence())

    def test_missing_ixit_declaration_aborts_before_scan_or_mutation(self) -> None:
        (self.ixit_directory / "ixit_28-LogIntf.md").write_text("15 declared interfaces\n", encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertEqual("ERROR", result.outcomes["IXIT declaration prerequisite"])
        self.assertFalse(any("-sT" in call for call in self.nmap.calls))
        self.assertFalse(self.gateway.config_bodies)
        self.assertFalse(self.ntp_capture.started)
        self.assertIn("IXIT documentation gap", self.evidence())

    def test_every_ixit_summary_declaration_is_required_unchanged(self) -> None:
        lines: list[str] = self.ixit_text.splitlines()
        row: str
        variant: str
        for row in lines:
            if not row.startswith("| `LogIntf-"):
                continue
            for variant in ("missing", "renamed", "port", "protocol", "access", "status", "duplicate"):
                with self.subTest(interface=row.split("|")[1], variant=variant):
                    self.setUp()
                    cells: list[str] = row.split("|")
                    if variant == "missing":
                        replacement: str = ""
                    elif variant == "duplicate":
                        replacement = f"{row}\n{row}"
                    else:
                        index: int = {"renamed": 1, "port": 2, "protocol": 3, "access": 4, "status": 5}[variant]
                        cells[index] = " changed "
                        replacement = "|".join(cells)
                    changed: str = self.ixit_text.replace(row, replacement)
                    (self.ixit_directory / "ixit_28-LogIntf.md").write_text(changed, encoding="utf-8")
                    result: RunResult = self.execute()
                    self.assertEqual("ERROR", result.verdict)
                    self.assertEqual("ERROR", result.outcomes["IXIT declaration prerequisite"])
                    self.assertFalse(any("-sT" in call for call in self.nmap.calls))
                    self.assertFalse(self.gateway.config_bodies)
                    self.assertFalse(self.ntp_capture.started)

    def test_ixit_prose_cannot_contradict_valid_summary(self) -> None:
        original: str
        changed: str
        for original, changed in (
            ("UDP 5353", "UDP 5354"), ("Enabled.", "Disabled."),
            ("Inbound mDNS responder", "Outbound mDNS client"),
        ):
            with self.subTest(original=original):
                self.setUp()
                self.assertIn(original, self.ixit_text)
                path: Path = self.ixit_directory / "ixit_28-LogIntf.md"
                path.write_text(self.ixit_text.replace(original, changed), encoding="utf-8")
                result: RunResult = self.execute()
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual("ERROR", result.outcomes["IXIT declaration prerequisite"])
                self.assertFalse(self.gateway.config_bodies)
                self.assertFalse(any("-sT" in call for call in self.nmap.calls))

    def test_ixit_changed_heading_is_rejected_even_with_valid_summary(self) -> None:
        path: Path = self.ixit_directory / "ixit_28-LogIntf.md"
        path.write_text(self.ixit_text.replace(
            "### **ID**: LogIntf-HTTP-Server", "### **ID**: LogIntf-Renamed-Server",
        ), encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertFalse(self.gateway.config_bodies)
        self.assertFalse(any("-sT" in call for call in self.nmap.calls))

    def test_ixit_heading_alone_cannot_enable_mdns_allowlist(self) -> None:
        path: Path = self.ixit_directory / "ixit_28-LogIntf.md"
        path.write_text("### **ID**: LogIntf-mDNS-Responder\n", encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertFalse(self.gateway.config_bodies)
        self.assertFalse(any("-sT" in call for call in self.nmap.calls))

    def test_closed_filtered_hotspot_ports_resolved_closed_allow_complete_udp_coverage(self) -> None:
        self.nmap.udp_ambiguous.update({53: "closed|filtered", 67: "closed|filtered", 68: "closed|filtered"})
        result: RunResult = self.execute()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["UDP coverage"])
        self.assertEqual("PASS", result.outcomes["hotspot servers disabled"])
        evidence: str = self.evidence()
        self.assertIn('"unresolved_closed_filtered_count": 1', evidence)
        self.assertIn('"closed_filtered_count": 4', evidence)
        self.assertIn('"independently_resolved": [', evidence)

    def test_unresolved_closed_filtered_hotspot_port_still_prevents_pass(self) -> None:
        self.nmap.udp_ambiguous[53] = "closed|filtered"
        self.nmap.targeted_states[53] = "closed|filtered"
        result: RunResult = self.execute()
        self.assertEqual("ERROR", result.verdict)
        self.assertEqual("ERROR", result.outcomes["UDP coverage"])
        self.assertEqual("ERROR", result.outcomes["hotspot servers disabled"])

    def test_unsafe_mqtt_baselines_are_rejected_before_mutation(self) -> None:
        field: str
        variant: str
        for field in ("mqtt_data_format", "mqtt_prefix", "mqtt_client_id"):
            for variant in ("missing", "custom", "wrong_type"):
                with self.subTest(field=field, variant=variant):
                    self.setUp()
                    if variant == "missing":
                        del self.gateway.fields[field]
                    else:
                        self.gateway.fields[field] = "custom" if variant == "custom" else True
                    before: dict[str, Any] = dict(self.gateway.fields)
                    result: RunResult = self.execute()
                    self.assertEqual("ERROR", result.verdict)
                    self.assertEqual(before, self.gateway.fields)
                    self.assertFalse(self.gateway.config_bodies)
                    self.assertFalse(self.ntp_capture.started)
                    self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")

    def test_confirmed_failures_survive_later_confirmation_errors(self) -> None:
        scenario: str
        for scenario in ("tcp", "udp", "targeted_hotspot"):
            with self.subTest(scenario=scenario):
                self.setUp()
                checks: tuple[str, str]
                if scenario == "tcp":
                    self.nmap.tcp_open.update({22, 23})
                    self.nmap.failed_confirmation = ("tcp", 23)
                    checks = ("TCP undocumented listeners", "custom telemetry disabled")
                elif scenario == "udp":
                    self.nmap.udp_open.update({53, 9999})
                    self.nmap.failed_confirmation = ("udp", 9999)
                    checks = ("UDP undocumented listeners", "hotspot servers disabled")
                else:
                    self.nmap.udp_ambiguous.update({53: "open|filtered", 67: "open|filtered"})
                    self.nmap.targeted_states[53] = "open"
                    self.nmap.failed_confirmation = ("udp", 67)
                    checks = ("UDP undocumented listeners", "hotspot servers disabled")
                result: RunResult = self.execute()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                check: str
                for check in checks:
                    self.assertEqual("FAIL", result.outcomes[check])
                    self.assertIn(f"{check} FAIL", self.lines)
                self.assertIn('"result": "FAIL"', self.evidence())
                if scenario != "tcp":
                    self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])

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

    def test_custom_outbound_http_aborts_before_scan_capture_or_mutation(self) -> None:
        field: str
        value: str
        for field, value in (("http_data_format", "ruuvi_decoded"), ("http_auth", "basic")):
            with self.subTest(field=field):
                self.setUp()
                self.gateway.fields[field] = value
                before: dict[str, Any] = dict(self.gateway.fields)
                self.assertIs(True, self.gateway.fields["use_http"])
                self.assertEqual("https://network.ruuvi.com/record", self.gateway.fields["http_url"])
                result: RunResult = self.execute()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("ERROR", result.outcomes["factory-default preflight"])
                self.assertEqual("NOT RUN", result.outcomes["custom telemetry disabled"])
                self.assertFalse(any("-sT" in call or "-sU" in call for call in self.nmap.calls))
                self.assertFalse(self.ntp_capture.started)
                self.assertEqual([
                    ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
                ], [(call.method, call.path) for call in self.wire_calls])
                self.assertEqual(before, self.gateway.fields)
                self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")

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

    def test_secret_bearing_http_url_rejection_only_records_value_in_evidence(self) -> None:
        secret_url: str = "https://secret-user:secret-password@example.com/record?token=secret-token"
        self.gateway.fields["http_url"] = secret_url
        before: dict[str, Any] = dict(self.gateway.fields)
        result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        console: str = "\n".join(self.lines)
        secret: str
        for secret in (secret_url, "secret-user", "secret-password", "secret-token"):
            self.assertNotIn(secret, console)
        self.assertIn("http_url has its public factory-default value", console)
        self.assertIn(secret_url, self.evidence())
        self.assertEqual(before, self.gateway.fields)
        self.assertFalse(self.ntp_capture.started)
        self.assertFalse(any("-sT" in call or "-sU" in call for call in self.nmap.calls))
        self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)

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

    def test_filtered_http_is_error_without_missing_listener_failure(self) -> None:
        self.nmap.http_filtered = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["documented HTTP listener"], "ERROR")
        self.assertEqual(result.outcomes["TCP coverage"], "ERROR")
        self.assertNotIn('"result": "FAIL"', self.evidence())

    def assert_restored_after_reporting_error(self, result: RunResult, baseline: dict[str, Any]) -> None:
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["NTP configuration restoration"], "PASS")
        self.assertEqual(self.gateway.fields, baseline)
        self.assertEqual([(call.method, call.path, call.body) for call in self.wire_calls[-2:]], [
            ("POST", "/ruuvi.json", {"ntp_server1": baseline["ntp_server1"]}),
            ("GET", "/ruuvi.json", None),
        ])

    def test_failed_final_progress_still_restores_and_verifies_full_baseline(self) -> None:
        baseline: dict[str, Any] = dict(self.gateway.fields)
        original: Callable[[target.ProgressReporter, str], None] = cast(
            Callable[[target.ProgressReporter, str], None], target.ProgressReporter.step,
        )

        def step(reporter: target.ProgressReporter, message: str) -> None:
            if message.startswith("Restoring and verifying"):
                raise BrokenPipeError("terminal unavailable")
            original(reporter, message)

        with patch.object(target.ProgressReporter, "step", step):
            result: RunResult = self.execute()
        self.assert_restored_after_reporting_error(result, baseline)
        self.assertIn("OVERALL VERDICT: ERROR", self.evidence())

    def test_persistent_evidence_failure_after_mutation_does_not_block_recovery_http(self) -> None:
        baseline: dict[str, Any] = dict(self.gateway.fields)
        original: Callable[[target.EvidenceLog, str, Any], None] = cast(
            Callable[[target.EvidenceLog, str, Any], None], target.EvidenceLog.write,
        )
        failed: bool = False

        def write(log: target.EvidenceLog, label: str, value: Any = "") -> None:
            nonlocal failed
            failed = failed or self.gateway.fields["ntp_server1"] != baseline["ntp_server1"]
            if failed:
                raise OSError("evidence unavailable")
            original(log, label, value)

        with patch.object(target.EvidenceLog, "write", write):
            result: RunResult = self.execute()
        self.assert_restored_after_reporting_error(result, baseline)
        self.assertTrue(failed)

    def test_failed_evidence_creation_or_preamble_never_contacts_dut(self) -> None:
        operation: str
        for operation in ("create", "write"):
            with self.subTest(operation=operation), patch.object(
                target.EvidenceLog, operation, side_effect=OSError("secret path must not appear"),
            ):
                result: RunResult = self.execute()
            self.assertEqual(result.exit_code, 2)
            self.assertFalse(self.wire_calls)
            self.assertFalse(self.nmap.calls)
            self.assertNotIn("secret path", "\n".join(self.lines))

    def test_failed_finalization_returns_error_and_preserves_check_outcomes(self) -> None:
        with patch.object(target.EvidenceLog, "finish", side_effect=OSError("disk error")):
            result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(result.outcomes["NTP configuration restoration"], "PASS")
        self.assertEqual("Overall verdict: ERROR", self.lines[-1])

    def test_failed_finalization_preserves_earlier_listener_failure(self) -> None:
        self.nmap.tcp_open.add(23)
        with patch.object(target.EvidenceLog, "finish", side_effect=OSError("disk error")):
            result: RunResult = self.execute()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["TCP undocumented listeners"])
        self.assertEqual("PASS", result.outcomes["NTP configuration restoration"])
        self.assertIn("undocumented listener FAIL: 23/tcp", "\n".join(self.lines))
        self.assertEqual("Overall verdict: ERROR", self.lines[-1])

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


class TcpdumpCaptureTests(unittest.TestCase):
    """Exercise the real observer without spawning tcpdump or accessing host pipes."""

    def setUp(self) -> None:
        self.capture: target.TcpdumpNtpCapture = target.TcpdumpNtpCapture()
        self.process: MagicMock = MagicMock(spec=subprocess.Popen)
        self.process.stderr = MagicMock()
        self.process.stderr.fileno.return_value = 17
        self.process.returncode = 0
        self.process.poll.return_value = None
        self.process.communicate.return_value = (
            "IP 192.0.2.10.62922 > 192.0.2.20.123: NTPv4, Client", "1 packet captured\n",
        )

    def start(self, *, chunks: tuple[bytes, ...] = (b"tcpdump: listening on any\n",),
              ready: bool = True, times: tuple[float, ...] = (0.0, 1.0, 2.0)) -> tuple[str, ...]:
        spawn: MagicMock
        with patch.object(target.shutil, "which", return_value="/usr/bin/tcpdump"), \
                patch.object(target.subprocess, "Popen", return_value=self.process) as spawn, \
                patch.object(target.select, "select", return_value=([self.process.stderr] if ready else [], [], [])), \
                patch.object(target.os, "read", side_effect=chunks), \
                patch.object(target.time, "monotonic", side_effect=times):
            command: tuple[str, ...] = self.capture.start("192.0.2.10", "192.0.2.20")
            spawn.assert_called_once_with(list(command), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            return command

    def test_split_readiness_and_successful_capture(self) -> None:
        command: tuple[str, ...] = self.start(chunks=(b"tcpdump: listen", b"ing on any\n"))
        self.assertEqual(("/usr/bin/tcpdump", "-i", "any", "-nn", "-l", "-c", "1", "-s", "0"), command[:-1])
        self.assertEqual(
            "src host 192.0.2.10 and dst host 192.0.2.20 and udp dst port 123 "
            "and udp[4:2] = 56 and (udp[8] & 7) = 3", command[-1],
        )
        result: target.NtpCaptureResult = self.capture.collect(25.0)
        self.assertEqual(62922, result.source_port)
        self.assertIn("listening on any", result.stderr)
        self.assertIn("1 packet captured", result.stderr)
        self.process.communicate.assert_called_once_with(timeout=25.0)
        self.process.poll.return_value = 0
        self.capture.stop()
        self.process.terminate.assert_not_called()
        self.process.kill.assert_not_called()
        self.assertIsNone(self.capture.process)

    def test_startup_early_exit_and_both_timeout_paths_cleanup(self) -> None:
        scenario: str
        for scenario in ("early_exit", "select_timeout", "deadline"):
            with self.subTest(scenario=scenario):
                self.setUp()
                with self.assertRaises(target.ScanError):
                    self.start(chunks=(b"permission denied\n", b""), ready=scenario != "select_timeout",
                               times=(0.0, 11.0) if scenario == "deadline" else (0.0, 1.0, 2.0))
                self.process.terminate.assert_called_once_with()
                self.process.communicate.assert_called_once_with(timeout=3)
                self.assertIsNone(self.capture.process)

    def test_missing_tool_and_missing_stderr(self) -> None:
        spawn: MagicMock
        with patch.object(target.shutil, "which", return_value=None), \
                patch.object(target.subprocess, "Popen") as spawn:
            with self.assertRaisesRegex(target.ScanError, "tcpdump is required"):
                self.capture.start("192.0.2.10", "192.0.2.20")
            spawn.assert_not_called()
        self.process.stderr = None
        with self.assertRaisesRegex(target.ScanError, "stderr is unavailable"):
            self.start()
        self.process.terminate.assert_called_once_with()
        self.assertIsNone(self.capture.process)

    def test_capture_timeout_then_kill_cleanup(self) -> None:
        self.start()
        self.process.communicate.side_effect = [
            subprocess.TimeoutExpired("tcpdump", 25), subprocess.TimeoutExpired("tcpdump", 3), ("", ""),
        ]
        with self.assertRaisesRegex(target.ScanError, "before timeout"):
            self.capture.collect(25)
        self.capture.stop()
        self.process.terminate.assert_called_once_with()
        self.process.kill.assert_called_once_with()
        self.assertEqual(3, self.process.communicate.call_count)
        self.assertIsNone(self.capture.process)
        self.capture.stop()
        self.assertEqual(3, self.process.communicate.call_count)

    def test_capture_rejects_bad_process_status_and_packet_output(self) -> None:
        scenario: str
        for scenario in ("status", "malformed", "wrong_dut", "wrong_host", "bad_port"):
            with self.subTest(scenario=scenario):
                self.setUp()
                self.start()
                output: str = "IP 192.0.2.10.62922 > 192.0.2.20.123: NTPv4, Client"
                if scenario == "status":
                    self.process.returncode = 1
                elif scenario == "malformed":
                    output = "no packets captured"
                elif scenario == "wrong_dut":
                    output = output.replace("192.0.2.10", "192.0.2.11")
                elif scenario == "wrong_host":
                    output = output.replace("192.0.2.20", "192.0.2.21")
                else:
                    output = output.replace("62922", "123")
                self.process.communicate.return_value = (output, "capture diagnostic")
                with self.assertRaises(target.ScanError):
                    self.capture.collect(25)
                self.capture.stop()
                self.process.terminate.assert_called_once_with()

    def test_collect_requires_started_capture(self) -> None:
        with self.assertRaisesRegex(target.ScanError, "not started"):
            self.capture.collect(25)


if __name__ == "__main__":
    unittest.main()
