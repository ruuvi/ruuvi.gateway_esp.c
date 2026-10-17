"""Offline runner checks; these are not ETSI compliance evidence."""

from __future__ import annotations

import hashlib
import os
import struct
import subprocess
import tempfile
import unittest
from collections.abc import Mapping
from dataclasses import replace
from pathlib import Path
from typing import Any, BinaryIO, Callable
from unittest.mock import Mock, patch

import requests

from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import GatewayApi, HttpMethod
from lib.models import DutConfig, RunResult
from test_5_6_2_2_a import (
    CAPTURE_PERMISSION_INSTRUCTION,
    CAPTURE_VISIBILITY_INSTRUCTION,
    CAPTURE_WINDOW_SECONDS,
    CaptureResult,
    _wait_for_tcpdump_ready,
    capture_dhcp,
    capture_dhcp_after_reset,
    decode_dhcp_pcap,
    execute_test_5_6_2_2_a,
)
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("AA:BB:CC:DD:EE:FF:00:11", "AA:BB:CC:DD:EE:FF", "gateway.local")
ETHERNET_MAC: str = "02:11:22:33:44:55"
DHCP_HOSTNAME: str = "RuuviGatewayEEFF"
GET_PATHS: tuple[str, ...] = (GatewayApi.AUTH, GatewayApi.AP, GatewayApi.STATUS, GatewayApi.CONFIG,
    GatewayApi.FIRMWARE_UPDATE, GatewayApi.INFO, GatewayApi.METRICS, GatewayApi.HISTORY,
    GatewayApi.VALIDATE_URL, GatewayApi.EXTRA_CFG)


class DisclosureGateway(DefaultAuthGateway):
    def __init__(self, config: DutConfig) -> None:
        super().__init__(config)
        self.changed_path: str = ""
        self.changed_status: int | None = None
        self.changed_body: str | None = None
        self.changed_header: tuple[str, str] | None = None
        self.config_override: dict[str, Any] | None = None

    def _config_response(self) -> FakeResponse:
        return FakeResponse(200, self.config_override) if self.config_override is not None else super()._config_response()

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        if method == "HEAD":
            return FakeResponse(400)
        if path == "/":
            response: FakeResponse = FakeResponse(200, headers={"Content-type": "text/html; charset=utf-8"})
            response._content = b'<script src="/app.js"></script>'
            return response
        if path == "/app.js":
            response = FakeResponse(200)
            response._content = b'class C{setDefaultUser(){this.lan_auth_user="Admin"}}; const version = "1.2.3";'
            return response
        response_headers: dict[str, str] = {"Location": f"{self.dut_config.base_url}/#auth"}
        if path not in {GatewayApi.AP, GatewayApi.STATUS}:
            response_headers["Set-Cookie"] = f"RUUVI_PREV_URL={path}"
        return FakeResponse(302, headers=response_headers)

    def response_for(self, session: FakeSession, method: str, path: str, headers: Mapping[str, str],
                     body: Any, allow_redirects: bool = False) -> FakeResponse:
        response: FakeResponse = super().response_for(session, method, path, headers, body, allow_redirects)
        response.headers["Server"] = "Ruuvi Gateway"
        if method == HttpMethod.GET and path == GatewayApi.AUTH and not session.authorized:
            cookie: str | None = response.cookies.get("RUUVISESSION")
            if cookie:
                response.headers["Set-Cookie"] = f"RUUVISESSION={cookie}"
        if path == self.changed_path and len(self.calls) > 3 and (path != "/" or method == HttpMethod.GET):
            if self.changed_status is not None:
                response.status_code = self.changed_status
            if self.changed_body is not None:
                response._content = self.changed_body.encode()
            if self.changed_header is not None:
                response.headers[self.changed_header[0]] = self.changed_header[1]
        return response


class RunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.work_dir: Path = Path(temporary.name)
        (self.work_dir / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.fake: DisclosureGateway = DisclosureGateway(CONFIG)
        self.messages: list[str] = []
        self.capture: CaptureResult = CaptureResult(
            f"02:11:22:33:44:55 > ff:ff:ff:ff:ff:ff DHCP Hostname {DHCP_HOSTNAME}", "eth0",
            source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC,
        )
        self.capture_calls: list[tuple[str, int]] = []

    def _capture(self, hostname: str, duration: int) -> CaptureResult:
        self.capture_calls.append((hostname, duration))
        return self.capture

    def _run(self) -> RunResult:
        return execute_test_5_6_2_2_a(self.work_dir, session_factory=lambda: FakeSession(self.fake),
                                        capturer=self._capture, output=self.messages.append)

    def _log(self) -> str:
        paths: list[Path] = list((self.work_dir / "logs").glob("test_5_6_2_2_a_*.log"))
        self.assertEqual(1, len(paths))
        return paths[0].read_text(encoding="utf-8")

    def test_pass_and_ordered_isolated_wire_matrix(self) -> None:
        result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)
        calls: list[RecordedRequest] = self.fake.calls
        self.assertEqual([(HttpMethod.GET, GatewayApi.AUTH), (HttpMethod.POST, GatewayApi.AUTH),
                          (HttpMethod.GET, GatewayApi.CONFIG)],
                         [(call.method, call.path) for call in calls[:3]])
        self.assertEqual([(HttpMethod.GET, "/"), (HttpMethod.GET, "/app.js")] +
                         [(HttpMethod.GET, path) for path in GET_PATHS] +
                         [(HttpMethod.GET, calls[-2].path), ("HEAD", "/")],
                         [(call.method, call.path) for call in calls[3:]])
        self.assertRegex(calls[-2].path, r"^/cra-[0-9a-f]{16}\.json$")
        self.assertEqual([None] * 14, [call.body for call in calls[3:]])
        self.assertEqual(["none"] * 14, [call.scheme for call in calls[3:]])
        self.assertEqual([None] * 14, [call.cookie for call in calls[3:]])
        self.assertEqual(14, len({call.session_number for call in calls[3:]}))
        self.assertEqual("Admin", calls[1].body["login"])
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        expected_response: str = hashlib.sha256(f"challenge-1-1:{ha1}".encode()).hexdigest()
        self.assertEqual(expected_response, calls[1].body["password"])
        self.assertEqual("RUUVISESSION=session-1", calls[1].cookie)
        self.assertEqual([(CONFIG.gw_hostname, CAPTURE_WINDOW_SECONDS)], self.capture_calls)
        steps: list[str] = [message for message in self.messages if message.startswith("[Step")]
        self.assertEqual(8, len(steps))
        self.assertTrue(all(message.startswith(f"[Step {index} out of 8]")
                            for index, message in enumerate(steps, 1)))
        self.assertEqual(1, sum(message.startswith("Open log file:") for message in self.messages))
        self.assertEqual("Overall verdict: PASS", self.messages[-1])
        evidence: str = self._log()
        self.assertIn("HTTP STATUS LINE", evidence)
        self.assertIn("SENSITIVE SCAN RESULT", evidence)
        self.assertIn("OVERALL VERDICT: PASS", evidence)

    def test_disclosures_and_status_fail(self) -> None:
        scenarios: tuple[tuple[str, int | None, str | None, tuple[str, str] | None], ...] = (
            (GatewayApi.CONFIG, 200, '{"lan_auth_pass":"1234567890abcdef"}', None),
            (GatewayApi.STATUS, None, None, ("X-Token", "Bearer abcdefgh12345678")),
            ("/app.js", None, '-----BEGIN PRIVATE KEY-----', None),
            (GatewayApi.HISTORY, 200, '{"history":[{"mac":"AA:BB"}]}', None),
            ("/", None, None, ("Server", "Ruuvi Gateway v1.2.3")),
            (GatewayApi.INFO, 404, None, None),
        )
        path: str
        status: int | None
        body: str | None
        header: tuple[str, str] | None
        for path, status, body, header in scenarios:
            with self.subTest(path=path, body=body):
                self.setUp()
                self.fake.changed_path = path
                self.fake.changed_status = status
                self.fake.changed_body = body
                self.fake.changed_header = header
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("PASS", result.outcomes["DHCP capture"])

    def test_cookie_and_challenge_protocol_are_enforced(self) -> None:
        self.fake.changed_path = GatewayApi.STATUS
        self.fake.changed_header = ("Set-Cookie", "RUUVI_PREV_URL=/status.json")
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["redirects and challenge"])
        self.setUp()
        self.fake.changed_path = GatewayApi.CONFIG
        self.fake.changed_header = ("Set-Cookie", "")
        result = self._run()
        self.assertEqual("FAIL", result.outcomes["redirects and challenge"])
        self.setUp()
        self.fake.changed_path = GatewayApi.AUTH
        self.fake.changed_header = ("WWW-Authenticate", "invalid")
        result = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["redirects and challenge"])
        self.assertEqual("PASS", result.outcomes["generic banner"])
        self.assertEqual("NOT RUN", result.outcomes["sensitive disclosure scan"])
        self.assertIn("GatewayProtocolError", self._log())

    def test_short_whitespace_credentials_and_coordinates_are_disclosures(self) -> None:
        body: str
        for body in ('{"http_pass":"abc"}', '{"mqtt_pass":"a b c"}',
                     '{"mqtt_pass":"false"}', '{"wifi_sta_config":{"password":"x"}}',
                     '{"coordinates":"60.1699,24.9384"}', '{"coordinates":"office,top floor"}'):
            with self.subTest(body=body):
                self.setUp()
                self.fake.changed_path = "/app.js"
                self.fake.changed_body = body
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_empty_defaults_and_coordinate_schema_are_public(self) -> None:
        self.fake.changed_path = "/app.js"
        self.fake.changed_body = (
            '{"http_pass":"","mqtt_pass":"","coordinates":"",'
            '"properties":{"coordinates":{"type":"string","description":"Coordinates"}}}'
        )
        result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)

    def test_runtime_backtrace_in_successful_html_fails_but_static_marker_is_public(self) -> None:
        self.fake.changed_path = "/"
        self.fake.changed_body = (
            '<script src="/app.js"></script><pre>Backtrace: 0x400d1234:0x3ffb5678</pre>'
        )
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertIn("runtime backtrace", self._log())
        self.setUp()
        self.fake.changed_path = "/app.js"
        self.fake.changed_body = 'const label="Backtrace:"; const mask=0xffffffff;'
        result = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)

    def test_cookie_and_valid_challenge_cannot_hide_secret_fields(self) -> None:
        self.fake.changed_path = "/"
        self.fake.changed_header = ("Set-Cookie", 'mqtt_pass="leaked-password"')
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
        self.setUp()
        self.fake.changed_path = GatewayApi.AUTH
        self.fake.changed_header = (
            "WWW-Authenticate",
            ('x-ruuvi-interactive realm="Ruuvi Gateway", challenge="nonce", '
             'session_id="session-3", session_cookie="RUUVISESSION", mqtt_pass="leaked-password"'),
        )
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("PASS", result.outcomes["redirects and challenge"])
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_ipv4_redirect_is_public_but_appended_build_is_not(self) -> None:
        config: DutConfig = replace(CONFIG, gw_hostname="192.0.2.10")
        (self.work_dir / ".env").write_text(env_text(config), encoding="utf-8")
        self.fake = DisclosureGateway(config)
        result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)
        self.setUp()
        (self.work_dir / ".env").write_text(env_text(config), encoding="utf-8")
        self.fake = DisclosureGateway(config)
        self.fake.changed_path = GatewayApi.STATUS
        self.fake.changed_header = ("Location", f"{config.base_url}/#auth-v1.2.3")
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
        self.assertIn("build identifier", self._log())

    def test_matrix_failure_then_timeout_keeps_failure_and_unrun_checks(self) -> None:
        self.fake.changed_path = "/app.js"
        self.fake.changed_status = 404
        original_response: Callable[..., FakeResponse] = self.fake.response_for

        def response_with_timeout(session: FakeSession, method: str, path: str,
                                  headers: Mapping[str, str], body: Any,
                                  allow_redirects: bool = False) -> FakeResponse:
            if path == GatewayApi.AP:
                raise requests.Timeout("injected matrix timeout")
            return original_response(session, method, path, headers, body, allow_redirects)

        with patch.object(self.fake, "response_for", side_effect=response_with_timeout):
            result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("PASS", result.outcomes["factory-default preflight"])
        self.assertEqual("FAIL", result.outcomes["HTTP disclosure matrix"])
        self.assertTrue(all(state == "NOT RUN" for check, state in result.outcomes.items()
                            if check not in {"factory-default preflight", "HTTP disclosure matrix"}))
        self.assertIn("HTTP disclosure matrix ERROR", "\n".join(self.messages))
        self.assertIn("HTTP disclosure matrix FAIL", "\n".join(self.messages))
        self.assertIn("INCOMPLETE CHECK", self._log())
        self.assertEqual([], self.capture_calls)

    def test_dhcp_device_id_seed_in_allowed_option_fails(self) -> None:
        option: str
        for option in ("01" + ETHERNET_MAC.replace(":", ""), CONFIG.gw_id.encode().hex()):
            with self.subTest(option=option):
                self.setUp()
                self.capture = replace(self.capture, hostname=DHCP_HOSTNAME,
                    option_values=((12, DHCP_HOSTNAME.encode().hex()), (61, option)))
                result: RunResult = self._run()
                expected: str = "FAIL" if option == CONFIG.gw_id.encode().hex() else "PASS"
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["DHCP capture"])

    def test_public_bearer_prose_is_not_a_token(self) -> None:
        self.fake.changed_path = "/"
        self.fake.changed_body = '<script src="/app.js"></script><span>Bearer authentication</span><script>const mask=0xffffffff;</script>'
        result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.setUp()
        self.fake.changed_path = "/"
        self.fake.changed_body = '<script src="/app.js"></script><span>Authorization: Bearer abcdefghijklmnop</span>'
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.setUp()
        self.fake.changed_path = "/"
        self.fake.changed_body = '<script src="/app.js"></script><span>stack:0xdeadbeef</span>'
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))

    def test_static_default_username_is_public_but_custom_value_fails(self) -> None:
        result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))
        self.setUp()
        self.fake.changed_path = "/app.js"
        self.fake.changed_body = 'class C{setDefaultUser(){this.lan_auth_user="changed-user"}}'
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_dhcp_route_lookup_uses_resolved_ipv4_before_tcpdump(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get", "192.0.2.10"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        packet: subprocess.CompletedProcess[bytes] = subprocess.CompletedProcess(
            ["tcpdump"], 1, b"", b"permission denied",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "-4", "neigh", "show"], 0,
            f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10") as resolve, \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor, packet)) as run_command:
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 2)
        resolve.assert_called_once_with(CONFIG.gw_hostname)
        self.assertEqual(["/usr/bin/ip", "route", "get", "192.0.2.10"], run_command.call_args_list[0].args[0])
        self.assertEqual(["/usr/bin/ip", "-4", "neigh", "show", "to", "192.0.2.10", "dev", "eth0"],
                         run_command.call_args_list[1].args[0])
        self.assertEqual("/usr/bin/tcpdump", run_command.call_args_list[2].args[0][0])
        self.assertEqual(ETHERNET_MAC, run_command.call_args_list[2].args[0][-1])
        self.assertEqual("eth0", result.interface)
        self.assertEqual(ETHERNET_MAC, result.ethernet_mac)
        self.assertIn("permission denied", result.problem or "")

    def test_boot_capture_waits_until_tcpdump_is_ready_before_reset(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "neigh"], 0, f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        process: Mock = Mock()
        process.poll.return_value = None
        process.returncode = 0
        process.communicate.return_value = (b"", b"")
        order: list[str] = []

        def ready(_process: Mock) -> None:
            self.assertIs(process, _process)
            order.append("ready")

        def reboot() -> str:
            self.assertEqual(["ready"], order)
            process.communicate.assert_not_called()
            order.append("reboot")
            return ""

        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor)), \
                patch("test_5_6_2_2_a.subprocess.Popen", return_value=process) as popen, \
                patch("test_5_6_2_2_a._wait_for_tcpdump_ready", side_effect=ready):
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 30, reboot)
        self.assertEqual(["ready", "reboot"], order)
        self.assertEqual(1, process.communicate.call_count)
        process.kill.assert_called_once_with()
        self.assertEqual(ETHERNET_MAC, popen.call_args.args[0][-1])
        self.assertIn("UART mDNS hostname marker not observed", result.problem or "")

    def test_boot_capture_stays_active_through_uart_marker_and_selects_last_packet(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "neigh"], 0, f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        bootp: bytearray = bytearray(240)
        bootp[236:240] = b"\x63\x82\x53\x63"
        ip_header: bytearray = bytearray(20)
        ip_header[0] = 0x45
        ip_header[9] = 17
        ethernet: bytes = bytes.fromhex("ffffffffffff0211223344550800")
        pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        name: str
        for name in ("espressif", DHCP_HOSTNAME):
            hostname: bytes = name.encode("ascii")
            frame: bytes = (ethernet + ip_header + bytes(8) + bootp
                            + bytes((12, len(hostname))) + hostname + bytes((255,)))
            pcap += struct.pack("<IIII", 0, 0, len(frame), len(frame)) + frame
        process: Mock = Mock()
        process.returncode = -15
        process.poll.side_effect = [None, -15]
        process.communicate.return_value = (b"", b"")
        order: list[str] = []

        def launch(_command: list[str], *, stdout: BinaryIO, stderr: int) -> Mock:
            self.assertEqual(subprocess.PIPE, stderr)
            stdout.write(pcap)
            order.append("launch")
            return process

        def ready(_process: Mock) -> None:
            self.assertIs(process, _process)
            order.append("ready")

        def reboot() -> str:
            order.append("reboot-and-observe")
            return DHCP_HOSTNAME

        def tail(seconds: float) -> None:
            self.assertEqual(3.0, seconds)
            order.append("tail")

        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor)), \
                patch("test_5_6_2_2_a.subprocess.Popen", side_effect=launch), \
                patch("test_5_6_2_2_a._wait_for_tcpdump_ready", side_effect=ready), \
                patch("test_5_6_2_2_a.time.sleep", side_effect=tail):
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 30, reboot)
        self.assertEqual(["launch", "ready", "reboot-and-observe", "tail"], order)
        process.terminate.assert_called_once_with()
        self.assertEqual(DHCP_HOSTNAME, result.hostname)
        self.assertEqual(DHCP_HOSTNAME, result.uart_hostname)
        self.assertEqual(("espressif",), tuple(frame.hostname for frame in result.prior_frames))

    def test_boot_capture_does_not_reset_until_ready_and_cleans_up_on_failure(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "neigh"], 0, f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        process: Mock = Mock()
        process.poll.return_value = None
        process.communicate.return_value = (b"", b"")
        reboot: Mock = Mock()
        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor)), \
                patch("test_5_6_2_2_a.subprocess.Popen", return_value=process), \
                patch("test_5_6_2_2_a._wait_for_tcpdump_ready", return_value="permission denied"):
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 30, reboot)
        reboot.assert_not_called()
        process.kill.assert_called_once_with()
        self.assertIn("permission denied", result.problem or "")

    def test_tcpdump_readiness_uses_actual_pipe_output(self) -> None:
        read_fd: int
        write_fd: int
        read_fd, write_fd = os.pipe()
        process: Mock = Mock()
        process.poll.return_value = None
        try:
            os.write(write_fd, b"tcpdump: listening on eth0, link-type EN10MB\n")
            with os.fdopen(read_fd, "rb", buffering=0) as stderr:
                process.stderr = stderr
                self.assertIsNone(_wait_for_tcpdump_ready(process))
        finally:
            os.close(write_fd)

    def test_boot_capture_reset_failure_terminates_tcpdump(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "neigh"], 0, f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        process: Mock = Mock()
        process.poll.return_value = None
        process.communicate.return_value = (b"", b"")

        def broken_reboot() -> str:
            raise RuntimeError("USB reset failed")

        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor)), \
                patch("test_5_6_2_2_a.subprocess.Popen", return_value=process), \
                patch("test_5_6_2_2_a._wait_for_tcpdump_ready", return_value=None):
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 30, broken_reboot)
        self.assertIn("gateway reboot or UART observation failed: RuntimeError: USB reset failed", result.problem or "")
        process.kill.assert_called_once_with()
        process.communicate.assert_called_once_with()

    def test_boot_capture_missing_uart_marker_terminates_tcpdump(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get"], 0, "192.0.2.10 dev eth0 src 192.0.2.20\n", "",
        )
        neighbor: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "neigh"], 0, f"192.0.2.10 dev eth0 lladdr {ETHERNET_MAC} REACHABLE\n", "",
        )
        process: Mock = Mock()
        process.poll.return_value = None
        process.communicate.return_value = (b"", b"")
        reboot: Mock = Mock(return_value="")
        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", side_effect=(route, neighbor)), \
                patch("test_5_6_2_2_a.subprocess.Popen", return_value=process), \
                patch("test_5_6_2_2_a._wait_for_tcpdump_ready", return_value=None):
            result: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 30, reboot)
        reboot.assert_called_once_with()
        process.kill.assert_called_once_with()
        self.assertIn("UART mDNS hostname marker not observed", result.problem or "")

    def test_default_runner_uses_serial_reboot_capturer(self) -> None:
        with patch("test_5_6_2_2_a.capture_dhcp_after_reset", return_value=self.capture) as boot_capture:
            result: RunResult = execute_test_5_6_2_2_a(
                self.work_dir, session_factory=lambda: FakeSession(self.fake), output=self.messages.append,
            )
        self.assertEqual("PASS", result.verdict)
        self.assertEqual((CONFIG.gw_hostname, CAPTURE_WINDOW_SECONDS), boot_capture.call_args.args[:2])

    def test_boot_capturer_reuses_public_serial_reset(self) -> None:
        transport: Mock = Mock()
        transport.discover.return_value = "/dev/ttyUSB0"
        early_uart: str = 'I (1000) ruuvi_gateway: ### Start mDNS: Hostname: "espressif"\n'
        configured_uart: str = f'I (14319) ruuvi_gateway: ### Start mDNS: Hostname: "{DHCP_HOSTNAME}"\n'
        transport.observe.return_value = early_uart + configured_uart
        observed: list[str] = []

        def capture(_hostname: str, _duration: int, reboot: Callable[[], str]) -> CaptureResult:
            observed.append("capture-ready")
            self.assertEqual(DHCP_HOSTNAME, reboot())
            return self.capture

        with patch("test_5_6_2_2_a.SerialTransport", return_value=transport), \
                patch("test_5_6_2_2_a.capture_dhcp", side_effect=capture):
            result: CaptureResult = capture_dhcp_after_reset(CONFIG.gw_hostname, 30, Mock())
        self.assertEqual(["capture-ready"], observed)
        transport.preflight.assert_called_once_with()
        transport.discover.assert_called_once_with()
        transport.reset.assert_called_once()
        transport.observe.assert_called_once()
        stop_when: Callable[[str], bool] = transport.observe.call_args.kwargs["stop_when"]
        self.assertFalse(stop_when(early_uart))
        self.assertTrue(stop_when(early_uart + configured_uart))
        self.assertEqual(self.capture, result)

    def test_routed_path_and_unverified_ethernet_source_cannot_pass(self) -> None:
        route: subprocess.CompletedProcess[str] = subprocess.CompletedProcess(
            ["ip", "route", "get", "192.0.2.10"], 0,
            "192.0.2.10 via 192.0.2.1 dev eth0 src 192.0.2.20\n", "",
        )
        with patch("test_5_6_2_2_a.shutil.which", side_effect=lambda tool: f"/usr/bin/{tool}"), \
                patch("test_5_6_2_2_a.socket.gethostbyname", return_value="192.0.2.10"), \
                patch("test_5_6_2_2_a.subprocess.run", return_value=route) as run_command:
            capture: CaptureResult = capture_dhcp(CONFIG.gw_hostname, 2)
        self.assertIn("not on this L2 segment", capture.problem or "")
        self.assertEqual(1, run_command.call_count)
        self.setUp()
        self.capture = CaptureResult(
            f"aa:bb:cc:dd:ee:ff DHCP Hostname {DHCP_HOSTNAME}", "eth0",
            source_mac=CONFIG.gw_mac, ethernet_mac=ETHERNET_MAC,
        )
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["DHCP capture"])

    def test_setup_and_capture_errors(self) -> None:
        self.fake.config_override = {GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_USER: "Admin", GatewayCfgDesc.LAN_AUTH_API_KEY_USE: 0,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False}
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(3, len(self.fake.calls))
        self.assertIsNotNone(result.recovery_message)
        self.setUp()
        self.fake.config_override = {GatewayCfgDesc.GW_MAC: "00:00:00:00:00:00"}
        result = self._run()
        self.assertIsNone(result.recovery_message)
        self.setUp()
        self.fake.mode = GatewayCfgLanAuthType.BASIC
        result = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(1, len(self.fake.calls))
        self.setUp()
        self.capture = CaptureResult(None, None, "no frame within window")
        result = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertIn("HTTP disclosure core passed", "\n".join(self.messages))
        self.setUp()
        self.capture = CaptureResult(
            f"02:11:22:33:44:55 DHCP Hostname {DHCP_HOSTNAME} vendor-specific token=secret", "eth0",
            source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC,
        )
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))

    def test_capture_permission_error_prints_sudo_retry_instruction(self) -> None:
        self.capture = CaptureResult(
            None, "ens33",
            "tcpdump capture failed (exit 1): tcpdump: ens33: You don't have permission to capture "
            "on that device (socket: Operation not permitted)",
        )
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual("ERROR", result.outcomes["DHCP capture"])
        self.assertIn(CAPTURE_PERMISSION_INSTRUCTION, self.messages)
        self.assertIn("DHCP CAPTURE RETRY INSTRUCTION", self._log())
        self.assertIn(CAPTURE_PERMISSION_INSTRUCTION, self._log())
        self.setUp()
        self.capture = CaptureResult(None, "ens33", "no DUT DHCP frame within 30s")
        self._run()
        self.assertNotIn(CAPTURE_PERMISSION_INSTRUCTION, self.messages)
        self.assertIn(CAPTURE_VISIBILITY_INSTRUCTION, self.messages)

    def test_wrong_password_and_negative_probe_abort(self) -> None:
        self.fake.dut_config = DutConfig("00:00:00:00:00:00:00:00", CONFIG.gw_mac, CONFIG.gw_hostname)
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertEqual(2, len(self.fake.calls))
        self.assertIsNotNone(result.recovery_message)
        self.setUp()
        self.fake.changed_path = GatewayApi.CONFIG
        self.fake.changed_status = 200
        self.fake.changed_body = '{"lan_auth_pass":"disclosed_hash"}'
        result = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual(GatewayApi.CONFIG, self.fake.calls[-1].path)
        self.assertEqual("FAIL", result.outcomes["HTTP disclosure matrix"])
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
        self.assertEqual("PASS", result.outcomes["DHCP capture"])

    def test_each_default_field_rejects_missing_unequal_and_wrong_type(self) -> None:
        defaults: dict[str, Any] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_USER: "Admin",
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
        }
        field: str
        value: Any
        for field, value in defaults.items():
            for case in ("missing", "unequal", "wrong type"):
                with self.subTest(field=field, case=case):
                    self.setUp()
                    payload: dict[str, Any] = {GatewayCfgDesc.GW_MAC: CONFIG.gw_mac, **defaults}
                    if case == "missing":
                        payload.pop(field)
                    elif case == "unequal":
                        payload[field] = True if isinstance(value, bool) else "changed"
                    else:
                        payload[field] = 0 if isinstance(value, bool) else 1
                    self.fake.config_override = payload
                    result: RunResult = self._run()
                    self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                    self.assertEqual(3, len(self.fake.calls))
                    self.assertIsNotNone(result.recovery_message)

    def test_missing_identity_cannot_receive_reset_advice(self) -> None:
        self.fake.config_override = {
            GatewayCfgDesc.LAN_AUTH_TYPE: "changed", GatewayCfgDesc.LAN_AUTH_USER: "Admin",
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False, GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
        }
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
        self.assertIsNone(result.recovery_message)

    def test_decoded_dhcp_options_are_checked(self) -> None:
        bootp: bytearray = bytearray(240)
        bootp[236:240] = b"\x63\x82\x53\x63"
        hostname: bytes = DHCP_HOSTNAME.encode("ascii")
        options: bytes = (
            bytes((53, 1, 1, 12, len(hostname))) + hostname
            + bytes((60, len(hostname))) + hostname + bytes((255,))
        )
        ip_header: bytearray = bytearray(20)
        ip_header[0] = 0x45
        ip_header[9] = 17
        ethernet: bytes = bytes.fromhex("ffffffffffff0211223344550800")
        frame: bytes = ethernet + ip_header + bytes(8) + bootp + options
        pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        pcap += struct.pack("<IIII", 0, 0, len(frame), len(frame)) + frame
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC)
        self.assertEqual((53, 12, 60), tuple(code for code, _ in self.capture.option_values or ()))
        self.assertEqual("PASS", self._run().verdict)
        self.setUp()
        changed_vendor: bytes = b"unexpected-vendor"
        frame = (
            ethernet + ip_header + bytes(8) + bootp
            + bytes((53, 1, 1, 12, len(hostname))) + hostname
            + bytes((60, len(changed_vendor))) + changed_vendor + bytes((255,))
        )
        pcap = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        pcap += struct.pack("<IIII", 0, 0, len(frame), len(frame)) + frame
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC)
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertIn("DHCP disclosure FAIL", "\n".join(self.messages))
        self.setUp()
        extra: bytes = bytes((43, 4)) + b"leak"
        frame = ethernet + ip_header + bytes(8) + bootp + options[:-1] + extra + bytes((255,))
        pcap = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        pcap += struct.pack("<IIII", 0, 0, len(frame), len(frame)) + frame
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC)
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["DHCP capture"])

    def test_dhcp_option_12_requires_firmware_short_hostname(self) -> None:
        observed: str
        for observed in ("OtherGateway", ""):
            with self.subTest(observed=observed):
                self.setUp()
                self.capture = CaptureResult(
                    f"{ETHERNET_MAC} DHCP Hostname {observed}", "eth0",
                    source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC,
                    hostname=observed, option_values=((12, observed.encode().hex()),) if observed else (),
                )
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertIn("Option 12 hostname", "\n".join(self.messages))

    def test_early_boot_espressif_never_satisfies_final_hostname_check(self) -> None:
        early_hostname: str = "espressif"
        early_capture: CaptureResult = CaptureResult(
            f"{ETHERNET_MAC} DHCP Hostname {early_hostname}; option codes 53, 57, 12, 61, 60, 55",
            "eth0", source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC,
            hostname=early_hostname,
            option_values=((53, "01"), (57, "05dc"), (12, early_hostname.encode().hex()),
                           (61, "01" + ETHERNET_MAC.replace(":", "")),
                           (60, early_hostname.encode().hex()), (55, "01031c2b062a")),
        )
        self.capture = replace(early_capture, uart_hostname=DHCP_HOSTNAME)
        self.assertEqual("ERROR", self._run().verdict)
        self.setUp()
        final_capture: CaptureResult = CaptureResult(
            f"{ETHERNET_MAC} DHCP Hostname {DHCP_HOSTNAME}", "eth0",
            source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC,
            hostname=DHCP_HOSTNAME, uart_hostname=DHCP_HOSTNAME,
            option_values=((12, DHCP_HOSTNAME.encode().hex()), (60, DHCP_HOSTNAME.encode().hex())),
            prior_frames=(early_capture,),
        )
        self.capture = final_capture
        self.assertEqual("PASS", self._run().verdict)
        self.setUp()
        self.capture = replace(final_capture, prior_frames=(replace(
            early_capture,
            option_values=((12, early_hostname.encode().hex()), (60, DHCP_HOSTNAME.encode().hex())),
        ),))
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))

    def test_multiple_pcap_records_select_last_dhcp_packet(self) -> None:
        bootp: bytearray = bytearray(240)
        bootp[236:240] = b"\x63\x82\x53\x63"
        ip_header: bytearray = bytearray(20)
        ip_header[0] = 0x45
        ip_header[9] = 17
        ethernet: bytes = bytes.fromhex("ffffffffffff0211223344550800")
        pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        name: str
        for name in ("espressif", DHCP_HOSTNAME):
            hostname: bytes = name.encode("ascii")
            options: bytes = bytes((53, 1, 1, 12, len(hostname))) + hostname + bytes((255,))
            frame: bytes = ethernet + ip_header + bytes(8) + bootp + options
            pcap += struct.pack("<IIII", 0, 0, len(frame), len(frame)) + frame
        capture: CaptureResult = decode_dhcp_pcap(pcap, "eth0")
        self.assertEqual(DHCP_HOSTNAME, capture.hostname)
        self.assertEqual(("espressif",), tuple(frame.hostname for frame in capture.prior_frames))
        self.capture = replace(capture, ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME)
        self.assertEqual("PASS", self._run().verdict)


if __name__ == "__main__":
    unittest.main()
