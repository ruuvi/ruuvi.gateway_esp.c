"""Offline runner checks; these are not ETSI compliance evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import struct
import subprocess
import tempfile
import unittest
from collections.abc import Mapping
from dataclasses import replace
from http.cookies import SimpleCookie
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
    Observation,
    Probe,
    _wait_for_tcpdump_ready,
    capture_dhcp,
    capture_dhcp_after_reset,
    decode_dhcp_pcap,
    execute_test_5_6_2_2_a,
    scan_observation,
)
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("AA:BB:CC:DD:EE:FF:00:11", "AA:BB:CC:DD:EE:FF", "gateway.local")
ETHERNET_MAC: str = "02:11:22:33:44:55"
DHCP_HOSTNAME: str = "RuuviGatewayEEFF"
GET_PATHS: tuple[str, ...] = (GatewayApi.AUTH, GatewayApi.AP, GatewayApi.STATUS, GatewayApi.CONFIG,
    GatewayApi.FIRMWARE_UPDATE, GatewayApi.INFO, GatewayApi.METRICS, GatewayApi.HISTORY,
    GatewayApi.VALIDATE_URL, GatewayApi.EXTRA_CFG)


def _dhcp_record(frame: bytes, ethernet_padding: bytes = b"") -> bytes:
    """Give authored wire fixtures real datagram lengths before adding Ethernet padding."""
    packet: bytearray = bytearray(frame)
    ip_header_length: int = (packet[14] & 0x0F) * 4
    struct.pack_into("!H", packet, 16, len(packet) - 14)
    struct.pack_into("!H", packet, 14 + ip_header_length + 4, len(packet) - 14 - ip_header_length)
    packet.extend(ethernet_padding)
    return struct.pack("<IIII", 0, 0, len(packet), len(packet)) + packet


class DisclosureGateway(DefaultAuthGateway):
    def __init__(self, config: DutConfig) -> None:
        super().__init__(config)
        self.changed_path: str = ""
        self.changed_status: int | None = None
        self.changed_body: str | None = None
        self.changed_header: tuple[str, str] | None = None
        self.config_override: dict[str, Any] | None = None
        self.append_header: tuple[str, str] | None = None

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
                if self.changed_header[0] == "Set-Cookie":
                    parsed_cookie: SimpleCookie = SimpleCookie()
                    parsed_cookie.load(self.changed_header[1])
                    response.cookies.clear()
                    cookie_name: str
                    for cookie_name in parsed_cookie:
                        response.cookies.set(cookie_name, parsed_cookie[cookie_name].value)
            if self.append_header is not None:
                response.headers[self.append_header[0]] += self.append_header[1]
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
            source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME,
            hostname=DHCP_HOSTNAME, option_values=((12, DHCP_HOSTNAME.encode().hex()),),
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

    def test_successful_rendered_diagnostics_are_detected_without_script_literals(self) -> None:
        content_type: str
        text: str
        expected: str
        for content_type, text, expected in (
            ("text/html", '<pre>{"stack":"0x3ffb5678"}</pre>', "FAIL"),
            ("text/html", '<pre>{&quot;stack&quot;:&quot;0x3ffb5678&quot;}</pre>', "FAIL"),
            ("text/html", "<pre>abort() called</pre>", "FAIL"),
            ("text/html", "<pre>0x400d1234</pre>", "FAIL"),
            ("text/html", "<pre>0x400d&#49;234</pre>", "FAIL"),
            ("text/plain", "0x400d1234", "FAIL"),
            ("text/html", "<script>const mask=0xffffffff;</script>", "PASS"),
            ("text/html", '<style>p:after{content:"0x400d1234"}</style>', "PASS"),
            ("text/html", "<pre>abort() was called at PC 0x400d1234 on core 0</pre>", "FAIL"),
            ("text/plain", "abort() called", "FAIL"),
            ("text/plain", '{"stack":"0x3ffb5678"}', "FAIL"),
            ("text/html", '<script>const label="abort() called";</script>', "PASS"),
            ("text/html", '<script>const label="Backtrace: 0x400d1234:0x3ffb5678";</script>', "PASS"),
            ("text/html", '<style>p:after{content:"panic"}</style>', "PASS"),
            ("text/html", "<p>The gateway is ready.</p>", "PASS"),
        ):
            with self.subTest(content_type=content_type, text=text):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_header = ("Content-Type", content_type)
                if content_type == "text/plain":
                    self.fake.changed_path = "/app.js"
                self.fake.changed_body = ('<script src="/app.js"></script>' if content_type == "text/html" else "") + text
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_successful_non_script_diagnostics_and_executable_controls(self) -> None:
        content_type: str
        body: str
        expected: str
        for content_type, body, expected in (
            ("application/json", '{"error":"panic"}', "FAIL"),
            ("application/json", '{"error":"abort()"}', "FAIL"),
            ("application/octet-stream", "abort() called", "FAIL"),
            ("application/json", '{"status":"ready"}', "PASS"),
            ("application/javascript", 'const label="panic abort()";', "PASS"),
            ("text/css", 'p:after{content:"panic abort()"}', "PASS"),
        ):
            with self.subTest(content_type=content_type, body=body):
                self.setUp()
                self.fake.changed_path = "/app.js"
                self.fake.changed_header = ("Content-Type", content_type)
                self.fake.changed_body = body
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_escaped_json_disclosures_and_public_defaults(self) -> None:
        body: str
        expected: str
        for body, expected in (
            (r'{"m\u0071tt_pass":"leaked"}', "FAIL"),
            (r'{"nested":{"lan_auth_pa\u0073s":"leaked"}}', "FAIL"),
            (r'{"lan_auth_api_ke\u0079":"qrstuvwxyz012345"}', "FAIL"),
            (r'{"remote_cfg_auth_bearer_to\u006ben":"leaked"}', "FAIL"),
            (r'{"hist\u006fry":[{"temperature":25}]}', "FAIL"),
            (r'{"sensor_da\u0074a":["telemetry"]}', "FAIL"),
            (r'{"ble_m\u0061c":{"address":"AA:BB:CC:DD:EE:FF"}}', "FAIL"),
            (r'{"ta\u0067s":{"AA:BB:CC:DD:EE:FF":{"data":"telemetry"}}}', "FAIL"),
            (r'{"coordina\u0074es":"office"}', "FAIL"),
            (r'{"wifi_sta_co\u006efig":{"pass\u0077ord":"secret"}}', "FAIL"),
            (r'{"m\u0071tt_pass":"","lan_auth_us\u0065r":"Admin","hist\u006fry":[]}', "PASS"),
            (r'{"properties":{"m\u0071tt_pass":{"type":"string"},"ta\u0067s":{"type":"object"}}}', "PASS"),
            (r'{"label":"Bearer authentication"}', "PASS"),
            ('{"wifi_ap_config":{"password":""}}', "PASS"),
            ('{"wifi_ap_config.password":""}', "PASS"),
        ):
            with self.subTest(body=body):
                self.setUp()
                self.fake.changed_path = "/app.js"
                # An array and an embedded object exercise nested decoded JSON in public assets.
                self.fake.changed_body = "window.fixture = [" + body + "];"
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_nested_serialized_json_disclosures_and_public_controls(self) -> None:
        payload: Any
        expected: str
        for payload, expected in (
            ('{"mqtt_pass":"leaked"}', "FAIL"),
            ({"dump": '{"mqtt_pass":"leaked"}'}, "FAIL"),
            ([{"dump": ['{"wifi_sta_config":{"password":"secret"}}']}], "FAIL"),
            ('{"nested":{"remote_cfg_auth_bearer_token":"leaked"}}', "FAIL"),
            ('{"mqtt_pass":"","lan_auth_user":"Admin"}', "PASS"),
            ({"dump": '{"properties":{"mqtt_pass":{"type":"string"}}}'}, "PASS"),
            (["Bearer authentication", "mqtt_pass"], "PASS"),
        ):
            depth: int
            for depth in (1, 2, 4):
                with self.subTest(payload=payload, depth=depth):
                    self.setUp()
                    encoded: str = json.dumps(payload)
                    for _ in range(depth - 1):
                        encoded = json.dumps(encoded)
                    self.fake.changed_path = "/app.js"
                    self.fake.changed_body = "window.fixture = " + encoded + ";"
                    result: RunResult = self._run()
                    self.assertEqual(expected, result.verdict, self.messages)
                    self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_canonical_remote_and_file_credentials_fail_without_pem_markers(self) -> None:
        field: str
        for field in ("remote_cfg_auth_token", "remote_cfg_auth_apikey", "http_cli_key",
                      "rcfg_cli_key", "remote_cfg_auth_basic_user", "http_stat_user"):
            surface: str
            for surface in ("JSON", "header", "HTML input"):
                with self.subTest(field=field, surface=surface):
                    self.setUp()
                    self.fake.changed_path = "/app.js"
                    if surface == "JSON":
                        self.fake.changed_body = json.dumps({field: "opaque-value"})
                    elif surface == "header":
                        self.fake.changed_header = (field, "opaque-value")
                    else:
                        self.fake.changed_path = "/"
                        self.fake.changed_body = (
                            '<script src="/app.js"></script>'
                            f'<input type="hidden" name="{field}" value="opaque-value">'
                        )
                    result: RunResult = self._run()
                    self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                    self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
                    self.assertNotIn("opaque-value", "\n".join(self.messages))

    def test_canonical_credential_empty_schema_and_public_certificate_controls(self) -> None:
        field: str
        for field in ("remote_cfg_auth_token", "remote_cfg_auth_apikey", "http_cli_key",
                      "rcfg_cli_key", "remote_cfg_auth_basic_user", "http_stat_user"):
            with self.subTest(field=field):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_body = (
                    '<script src="/app.js"></script>'
                    f'<input type="hidden" name="{field}" value="">'
                )
                self.fake.changed_header = (field, "")
                result: RunResult = self._run()
                self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)
                self.setUp()
                self.fake.changed_path = "/app.js"
                self.fake.changed_body = json.dumps({
                    field: "", "properties": {field: {"type": "string"}},
                    "http_cli_cert": "public-certificate", "rcfg_cli_cert": "public-certificate",
                    "mqtt_cli_cert": "public-certificate", "stat_cli_cert": "public-certificate",
                })
                result = self._run()
                self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)

    def test_escaped_credential_quotes_preserve_complete_finding(self) -> None:
        assignment: str
        for assignment in (r'"mqtt_pass":"abc\"def"', r"'mqtt_pass':'abc\'def'",
                           r'"mqtt_pass":"\""', r'"mqtt_pass":"abc\\"'):
            with self.subTest(assignment=assignment):
                self.setUp()
                self.fake.changed_path = "/app.js"
                body: str = "{" + assignment + "}"
                self.fake.changed_body = body
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                observation: Observation = Observation(
                    Probe(HttpMethod.GET, "/app.js", 200), (), body, 200,
                )
                tokens: list[str] = [finding.token for finding in scan_observation(observation, CONFIG.gw_id, "Admin")
                                     if finding.category == "credential value"]
                self.assertIn(assignment, tokens)

    def test_decoded_and_contiguous_device_id_seeds_are_detected(self) -> None:
        text: str
        in_header: bool
        expected: str
        for text, in_header, expected in (
            (r'{"nested":{"value":"AA\u003ABB\u003ACC\u003ADD\u003AEE\u003AFF\u003A00\u003A11"}}', False, "FAIL"),
            (r'["AA\u003ABB\u003ACC\u003ADD\u003AEE\u003AFF\u003A00\u003A11"]', False, "FAIL"),
            ('{"value":"aabbccddeeff0011"}', False, "FAIL"),
            ("seed=AABBCCDDEEFF0011", True, "FAIL"),
            (CONFIG.gw_id.lower(), True, "FAIL"),
            ('{"value":"0aabbccddeeff0011f"}', False, "PASS"),
            ('{"value":"aabbccddeeff0012"}', False, "PASS"),
        ):
            with self.subTest(text=text, in_header=in_header):
                self.setUp()
                self.fake.changed_path = "/app.js"
                if in_header:
                    self.fake.changed_header = ("X-Public-Value", text)
                else:
                    self.fake.changed_body = "window.fixture = [" + text + "];"
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_public_challenge_requires_exact_declared_realm(self) -> None:
        original_response: Callable[..., FakeResponse] = self.fake.response_for

        def wrong_realm(session: FakeSession, method: str, path: str,
                        headers: Mapping[str, str], body: Any,
                        allow_redirects: bool = False) -> FakeResponse:
            response: FakeResponse = original_response(session, method, path, headers, body, allow_redirects)
            if path == GatewayApi.AUTH and len(self.fake.calls) > 3:
                response.headers["WWW-Authenticate"] = response.headers["WWW-Authenticate"].replace(
                    'realm="Ruuvi Gateway"', 'realm="attacker-realm"',
                )
            return response

        with patch.object(self.fake, "response_for", side_effect=wrong_realm):
            result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["redirects and challenge"])
        self.assertIn("declared Ruuvi Gateway realm", self._log())

    def test_credential_header_names_are_scanned_with_public_header_controls(self) -> None:
        name: str
        value: str
        expected: str
        for name, value, expected in (
            ("mqtt_pass", "leaked-password", "FAIL"),
            ("wifi_ap_config.password", "softap-secret", "FAIL"),
            ("WIFI-AP-CONFIG.PASSWORD", "softap-secret", "FAIL"),
            ("wifi_ap_config.password", "", "PASS"),
            ("lan_auth_api_key", "qrstuvwxyz012345", "FAIL"),
            ("MQTT-PASS", "leaked-password", "FAIL"),
            ("mqtt_pass", "", "PASS"),
            ("X-Public-Nonce", "public-handshake-nonce", "PASS"),
            ("Set-Cookie", "public_session=example; Path=/", "PASS"),
        ):
            with self.subTest(name=name, value=value):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_header = (name, value)
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_wifi_and_html_escaped_configuration_literals_are_detected(self) -> None:
        path: str
        text: str
        for path, text in (
            ("/app.js", '{"wifi_sta_config":{"password":"\'secretpass"}}'),
            ("/app.js", '{"wifi_ap_config":{"password":"softap-secret"}}'),
            ("/app.js", "window.config = {'wifi_ap_config':{'password':'softap-secret'}};"),
            ("/app.js", r'{"wifi_ap_co\u006efig":{"pass\u0077ord":"softap-secret"}}'),
            ("/app.js", '{"wifi_ap_config.password":"softap-secret"}'),
            ("/", '<pre>{&quot;wifi_ap_config&quot;:{&quot;password&quot;:&quot;softap-secret&quot;}}</pre>'),
            ("/app.js", '{"wifi_sta_config":{"ssid":"office}net","password":"ordinarysecret"}}'),
            ("/app.js", r'{"wifi_sta_config":{"ssid":"office\"}net","password":"\"secret"}}'),
            ("/app.js", r'{"wifi_sta_config":{"pass\u0077ord":"secret"}}'),
            ("/app.js", "window.config = {'wifi_sta_config':{'ssid':'office}net','password':'\\\'secret'}};"),
            ("/", '<pre>{&quot;mqtt_pass&quot;:&quot;leaked-password&quot;}</pre>'),
            ("/", '<pre>{&quot;wifi_sta_config&quot;:{&quot;password&quot;:&quot;secret&quot;}}</pre>'),
        ):
            with self.subTest(path=path, text=text):
                self.setUp()
                self.fake.changed_path = path
                self.fake.changed_body = ('<script src="/app.js"></script>' if path == "/" else "") + text
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
        self.fake.append_header = ("WWW-Authenticate", ', mqtt_pass="leaked-password"')
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

    def test_sensitive_findings_keep_tokens_in_local_evidence_only(self) -> None:
        secret: str = "private-canary-credential-9"
        body: str
        header: tuple[str, str] | None
        for body, header in (
            (json.dumps({"mqtt_pass": secret}), None),
            (f'<input name="http_pass" value="{secret}">', ("Content-Type", "text/html")),
            ("", ("Authorization", f"Bearer {secret}")),
        ):
            with self.subTest(body=body, header=header):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_body = body + '<script src="/app.js"></script>'
                self.fake.changed_header = header
                result: RunResult = self._run()
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
                self.assertNotIn(secret, "\n".join(self.messages))
                self.assertIn(secret, self._log())
                self.assertIn("sensitive disclosure FAIL:", "\n".join(self.messages))

    def test_dhcp_requires_post_reset_uart_hostname_marker(self) -> None:
        marker: str | None
        for marker in (None, "", " \t"):
            with self.subTest(marker=marker):
                self.setUp()
                self.capture = replace(self.capture, uart_hostname=marker)
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("ERROR", result.outcomes["DHCP capture"])
                self.assertTrue(all(state == "PASS" for check, state in result.outcomes.items()
                                    if check != "DHCP capture"))
                self.assertIn("UART mDNS hostname", "\n".join(self.messages))

    def test_pcap_length_mismatch_cannot_be_hidden_by_later_complete_packet(self) -> None:
        bootp: bytearray = bytearray(240)
        bootp[236:240] = b"\x63\x82\x53\x63"
        ip_header: bytearray = bytearray(20)
        ip_header[0] = 0x45
        ip_header[9] = 17
        hostname: bytes = DHCP_HOSTNAME.encode()
        frame: bytes = (bytes.fromhex("ffffffffffff0211223344550800") + ip_header + bytes(8)
                        + bootp + bytes((12, len(hostname))) + hostname + b"\xff")
        complete: bytes = _dhcp_record(frame)
        delta: int
        for delta in (-1, 1):
            with self.subTest(original_length_delta=delta):
                self.setUp()
                record: bytearray = bytearray(complete)
                struct.pack_into("<I", record, 12, len(frame) + delta)
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                self.capture = replace(decode_dhcp_pcap(pcap + record + complete, "eth0"),
                                       ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("ERROR", result.outcomes["DHCP capture"])
                self.assertIn("captured/original frame lengths differ", "\n".join(self.messages))

    def test_dhcp_device_id_seed_in_allowed_option_fails(self) -> None:
        option: str
        for option in ("01" + ETHERNET_MAC.replace(":", ""), CONFIG.gw_mac.replace(":", ""),
                       CONFIG.gw_id.encode().hex(), CONFIG.gw_id.replace(":", "").encode().hex(),
                       CONFIG.gw_id.replace(":", "").lower().encode().hex(), CONFIG.gw_id.replace(":", ""),
                       "01" + CONFIG.gw_id.replace(":", "")):
            with self.subTest(option=option):
                self.setUp()
                self.capture = replace(self.capture, hostname=DHCP_HOSTNAME,
                    option_values=((12, DHCP_HOSTNAME.encode().hex()), (61, option)))
                result: RunResult = self._run()
                expected: str = "PASS" if option in {
                    "01" + ETHERNET_MAC.replace(":", ""), CONFIG.gw_mac.replace(":", ""),
                } else "FAIL"
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["DHCP capture"])

    def test_unquoted_cookie_secret_is_detected(self) -> None:
        self.fake.changed_path = "/"
        self.fake.changed_header = ("Set-Cookie", "mqtt_pass=leaked-password; Path=/")
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_unquoted_html_credentials_fail_without_scanning_javascript_expressions(self) -> None:
        markup: str
        expected: str
        for markup, expected in (
            ("<pre>mqtt_pass=abc</pre>", "FAIL"),
            ("<pre>http_pass=a&amp;b</pre>", "FAIL"),
            ("<script>mqtt_pass=config.mqtt_pass;</script>", "PASS"),
            ("<script>document.form.mqtt_pass=config.mqtt_pass;</script>", "PASS"),
            ("<style>.mqtt_pass=example</style>", "PASS"),
        ):
            with self.subTest(markup=markup):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_body = '<script src="/app.js"></script>' + markup
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_credential_input_attributes_are_scanned_without_empty_control_false_positives(self) -> None:
        markup: str
        expected: str
        for markup, expected in (
            ('<input type="password" name="mqtt_pass" value="leaked-password">', "FAIL"),
            ('<input type="hidden" id="http_pass" value="abc">', "FAIL"),
            ('<input type="password" name="mqtt_pass" value="a &amp; b">', "FAIL"),
            ('<input type="password" name="mqtt_pass" value="">', "PASS"),
            ('<input type="password" name="mqtt_pass">', "PASS"),
            ('<input name="hostname" value="gateway">', "PASS"),
            ('<script>const form = \'<input name="mqtt_pass" value="example">\';</script>', "PASS"),
        ):
            with self.subTest(markup=markup):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_body = '<script src="/app.js"></script>' + markup
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_plain_text_credentials_fail_without_scanning_javascript_expressions(self) -> None:
        path: str
        content_type: str
        body: str
        expected: str
        for path, content_type, body, expected in (
            (GatewayApi.AUTH, "text/plain; charset=utf-8", "mqtt_pass=abc", "FAIL"),
            (GatewayApi.AUTH, "text/plain", "http_pass=a&b", "FAIL"),
            (GatewayApi.AUTH, "text/plain", "mqtt_pass=", "PASS"),
            ("/app.js", "application/javascript", "mqtt_pass=config.mqtt_pass;", "PASS"),
            ("/app.js", "text/javascript", "mqtt_pass=config.mqtt_pass;", "PASS"),
        ):
            with self.subTest(path=path, content_type=content_type, body=body):
                self.setUp()
                self.fake.changed_path = path
                self.fake.changed_header = ("Content-Type", content_type)
                self.fake.changed_body = body
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_bootp_fixed_fields_cannot_hide_disclosures_in_any_packet(self) -> None:
        field_offset: int
        payload: bytes
        early: bool
        for field_offset in (44, 108):
            for payload in (b"mqtt_pass=abc", CONFIG.gw_id.encode(), bytes.fromhex(CONFIG.gw_id.replace(":", "")), b"\x00\x01", b""):
                for early in (False, True):
                    with self.subTest(field_offset=field_offset, payload=payload, early=early):
                        self.setUp()
                        # IPv4 options make the BOOTP offset differ from the common 20-byte header.
                        ip_header: bytearray = bytearray(24)
                        ip_header[0] = 0x46
                        ip_header[9] = 17
                        bootp: bytearray = bytearray(240)
                        bootp[236:240] = b"\x63\x82\x53\x63"
                        bootp[field_offset:field_offset + len(payload)] = payload
                        hostname: bytes = DHCP_HOSTNAME.encode()
                        options: bytes = bytes((12, len(hostname))) + hostname + bytes((255,))
                        ethernet: bytes = bytes.fromhex("ffffffffffff0211223344550800")
                        frame: bytes = ethernet + ip_header + bytes(8) + bootp + options
                        pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                        pcap += _dhcp_record(frame)
                        if early:
                            bootp[44:236] = bytes(192)
                            frame = ethernet + ip_header + bytes(8) + bootp + options
                            pcap += _dhcp_record(frame)
                        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                               uart_hostname=DHCP_HOSTNAME)
                        result: RunResult = self._run()
                        expected: str = "FAIL" if payload else "PASS"
                        self.assertEqual(expected, result.verdict, self.messages)
                        self.assertEqual(expected, result.outcomes["DHCP capture"])

    def test_standalone_abort_diagnostics_fail(self) -> None:
        diagnostic: str
        for diagnostic in ("abort()", "abort() called", "panic", "backtrace"):
            with self.subTest(diagnostic=diagnostic):
                self.setUp()
                self.fake.changed_path = GatewayApi.AUTH
                self.fake.changed_header = ("X-Diagnostic", diagnostic)
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_only_complete_standard_numeric_headers_are_public(self) -> None:
        header: str
        value: str
        expected: str
        for header, value, expected in (
            ("Content-Length", "1234567", "PASS"),
            ("Cache-Control", "max-age=31536000", "PASS"),
            ("Cache-Control", "public, max-age=31536000, immutable", "PASS"),
            ("Cache-Control", "max-age=31536000; build=v1.2.3", "FAIL"),
            ("Cache-Control", "max-age=31536000, build=deadbeef", "FAIL"),
            ("Content-Length", "1234567; build=deadbeef", "FAIL"),
            ("X-Build", "31536000", "FAIL"),
            ("Server", "Ruuvi Gateway 31536000", "FAIL"),
        ):
            with self.subTest(header=header, value=value):
                self.setUp()
                self.fake.changed_path = "/"
                self.fake.changed_header = (header, value)
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["sensitive disclosure scan"])

    def test_previous_url_cookie_requires_exact_name_and_requested_path(self) -> None:
        cookie: str
        expected: str
        for cookie, expected in (
            ("NOT_RUUVI_PREV_URL=/ruuvi.json", "FAIL"),
            ("RUUVI_PREV_URL=", "FAIL"),
            ("RUUVI_PREV_URL=/history", "FAIL"),
            ("ruuvi_prev_url=/ruuvi.json", "FAIL"),
            ("RUUVI_PREV_URL=/ruuvi.json; Path=/", "PASS"),
        ):
            with self.subTest(cookie=cookie):
                self.setUp()
                self.fake.changed_path = GatewayApi.CONFIG
                self.fake.changed_header = ("Set-Cookie", cookie)
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["redirects and challenge"])

    def test_malformed_optional_ecdh_keys_are_protocol_errors(self) -> None:
        public_key: str
        for public_key in ("", "!invalid!", "AAAA",
                           base64.b64encode(bytes(65)).decode(),
                           base64.b64encode(b"\x04" + bytes(64)).decode(),
                           base64.b64encode(b"\x04" + b"\x01" * 64).decode()):
            with self.subTest(public_key=public_key):
                self.setUp()
                self.fake.changed_path = GatewayApi.AUTH
                self.fake.changed_header = ("Ruuvi-Ecdh-Pub-Key", public_key)
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("ERROR", result.outcomes["public metadata"])
                self.assertIn("invalid gateway ECDH public key", self._log())

    def test_actual_history_fixture_in_public_asset_is_detected(self) -> None:
        fixture: Path = Path(__file__).resolve().parents[3] / "schemas/examples/history.json"
        self.fake.changed_path = "/app.js"
        self.fake.changed_body = fixture.read_text(encoding="utf-8")
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertIn("sensor tags", self._log())
        self.setUp()
        self.fake.changed_path = "/app.js"
        self.fake.changed_body = '{"data":{"tags":{}},"properties":{"tags":{"type":"object"}}}'
        result = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code))

    def test_handshake_headers_allow_only_public_values(self) -> None:
        path: str
        header: str
        suffix: str
        for path, header, suffix in (
            (GatewayApi.CONFIG, "Set-Cookie", "; build=v1.2.3"),
            (GatewayApi.AUTH, "Set-Cookie", "; build=v1.2.3"),
            (GatewayApi.AUTH, "WWW-Authenticate", ' build="v1.2.3"'),
            (GatewayApi.AUTH, "WWW-Authenticate", ' diagnostic="panic"'),
        ):
            with self.subTest(path=path, header=header, suffix=suffix):
                self.setUp()
                self.fake.changed_path = path
                self.fake.append_header = (header, suffix)
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("PASS", result.outcomes["redirects and challenge"])
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])

    def test_invalid_challenge_cookie_cannot_pass(self) -> None:
        header: tuple[str, str]
        for header in (("Set-Cookie", "RUUVISESSION="), ("Set-Cookie", "RUUVISESSION=other"),
                       ("WWW-Authenticate", ('x-ruuvi-interactive realm="Ruuvi Gateway" '
                         'challenge="nonce" session_id="session-4" session_cookie="WRONG"'))):
            with self.subTest(header=header):
                self.setUp()
                self.fake.changed_path = GatewayApi.AUTH
                self.fake.changed_header = header
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("ERROR", result.outcomes["redirects and challenge"])
                self.assertEqual("PASS", result.outcomes["HTTP disclosure matrix"])
                self.assertEqual("NOT RUN", result.outcomes["sensitive disclosure scan"])

    def test_validated_nonce_and_ecdh_key_are_public_but_extra_key_metadata_is_not(self) -> None:
        original_response: Callable[..., FakeResponse] = self.fake.response_for

        def public_handshake(session: FakeSession, method: str, path: str,
                             headers: Mapping[str, str], body: Any,
                             allow_redirects: bool = False) -> FakeResponse:
            response: FakeResponse = original_response(session, method, path, headers, body, allow_redirects)
            if path == GatewayApi.AUTH and len(self.fake.calls) > 3:
                response.headers["WWW-Authenticate"] = response.headers["WWW-Authenticate"].replace(
                    f'challenge="{session.challenge}"', 'challenge="0123456789abcdef0123456789abcdef"',
                )
                response.headers["Ruuvi-Ecdh-Pub-Key"] = self.fake.server_public_b64
            return response

        with patch.object(self.fake, "response_for", side_effect=public_handshake):
            result: RunResult = self._run()
        self.assertEqual(("PASS", 0), (result.verdict, result.exit_code), self.messages)
        self.setUp()
        self.fake.changed_path = GatewayApi.AUTH
        self.fake.changed_header = ("Ruuvi-Ecdh-Pub-Key", self.fake.server_public_b64 + "; build=v1.2.3")
        result = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
        self.assertEqual("ERROR", result.outcomes["public metadata"])

    def test_allowed_dhcp_options_still_scan_sensitive_contents(self) -> None:
        value: str
        for value in ('{"mqtt_pass":"abc"}', 'mqtt_pass=abc',
                      '{"wifi_sta_config":{"password":"abc"}}',
                      '{"coordinates":"60,24"}',
                      '{"data":{"tags":{"F4:1F:0C:28:CB:D6":{"rssi":-34}}}}'):
            with self.subTest(value=value):
                self.setUp()
                self.capture = replace(self.capture, hostname=DHCP_HOSTNAME,
                    option_values=((12, DHCP_HOSTNAME.encode().hex()), (61, value.encode().hex())))
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("FAIL", result.outcomes["DHCP capture"])

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

    def test_short_bearer_credentials_fail_in_headers_and_bodies(self) -> None:
        path: str
        body: str | None
        header: tuple[str, str] | None
        scenarios: tuple[tuple[str, str | None, tuple[str, str] | None], ...] = (
            (GatewayApi.STATUS, None, ("Authorization", "Bearer abc")),
            (GatewayApi.STATUS, None, ("X-Token", "Bearer a")),
            ("/", '<script src="/app.js"></script><span>Authorization: Bearer abc</span>', None),
            ("/", '<script src="/app.js"></script><span>Authorization: Bearer a</span>', None),
            ("/", '<script src="/app.js"></script><span>Authorization: Bearer a1</span>', None),
            ("/app.js", r'{"Authorization":"Bear\u0065r abc"}', None),
        )
        for path, body, header in scenarios:
            with self.subTest(path=path, body=body, header=header):
                self.setUp()
                self.fake.changed_path = path
                self.fake.changed_body = body
                self.fake.changed_header = header
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
                self.assertIn("bearer token", self._log())

    def test_padded_bearer_credentials_fail_and_retain_complete_evidence(self) -> None:
        path: str
        body: str | None
        header: tuple[str, str] | None
        token: str
        scenarios: tuple[tuple[str, str | None, tuple[str, str] | None, str], ...] = (
            (GatewayApi.STATUS, None, ("Authorization", "Bearer abc="), "Bearer abc="),
            ("/", '<script src="/app.js"></script><span>Bearer abc==</span>', None, "Bearer abc=="),
            ("/app.js", r'{"value":"Bear\u0065r abc="}', None, "Bearer abc="),
        )
        for path, body, header, token in scenarios:
            with self.subTest(path=path, token=token):
                self.setUp()
                self.fake.changed_path = path
                self.fake.changed_body = body
                self.fake.changed_header = header
                result: RunResult = self._run()
                self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
                self.assertEqual("FAIL", result.outcomes["sensitive disclosure scan"])
                self.assertIn("bearer token", self._log())
                self.assertIn(token, self._log())
                self.assertNotIn(token, "\n".join(self.messages))

    def test_padded_bearer_credentials_fail_in_dhcp_options(self) -> None:
        self.capture = replace(self.capture, option_values=(
            (12, DHCP_HOSTNAME.encode().hex()), (61, b"Bearer abc==".hex()),
        ))
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["DHCP capture"])

    def test_short_bearer_credentials_fail_in_dhcp_options(self) -> None:
        self.capture = replace(self.capture, option_values=(
            (12, DHCP_HOSTNAME.encode().hex()), (61, b"Authorization: Bearer a".hex()),
        ))
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("FAIL", result.outcomes["DHCP capture"])

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
            pcap += _dhcp_record(frame)
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
            source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME,
            hostname=DHCP_HOSTNAME, option_values=((12, DHCP_HOSTNAME.encode().hex()),),
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

    def test_default_baseline_without_identity_cannot_start_dhcp_capture(self) -> None:
        from lib.config import AUTHENTICATION_DEFAULT_FIELDS, default_config_values

        earlier_failure: bool
        for earlier_failure in (False, True):
            with self.subTest(earlier_failure=earlier_failure):
                self.setUp()
                self.fake.config_override = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
                if earlier_failure:
                    self.fake.changed_path = "/"
                    self.fake.changed_header = ("Server", "Ruuvi Gateway v1.2.3")
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("PASS", result.outcomes["factory-default preflight"])
                self.assertEqual("FAIL" if earlier_failure else "PASS", result.outcomes["generic banner"])
                self.assertEqual("ERROR", result.outcomes["DHCP capture"])
                self.assertEqual([], self.capture_calls)
                self.assertIsNone(result.recovery_message)
                self.assertIn("authenticated DUT MAC identity is unavailable", "\n".join(self.messages))
                self.assertIn("authenticated DUT MAC identity is unavailable", self._log())

    def test_dhcp_rejects_invalid_ipv4_version_and_header_length(self) -> None:
        version_ihl: int
        for version_ihl in (0x65, 0x05, 0x40, 0x41, 0x44, 0x45, 0x46):
            with self.subTest(version_ihl=version_ihl):
                self.setUp()
                ip_header: bytearray = bytearray((version_ihl & 0x0F) * 4)
                if len(ip_header) < 10:
                    ip_header.extend(bytes(10 - len(ip_header)))
                ip_header[0] = version_ihl
                ip_header[9] = 17
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                hostname: bytes = DHCP_HOSTNAME.encode("ascii")
                frame: bytes = (bytes.fromhex("ffffffffffff0211223344550800") + ip_header + bytes(8)
                                + bootp + bytes((12, len(hostname))) + hostname + b"\xff")
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += _dhcp_record(frame)
                complete: bytes = (bytes.fromhex("ffffffffffff021122334455080045000000000000000011")
                                   + bytes(10) + bytes(8) + bootp
                                   + bytes((12, len(hostname))) + hostname + b"\xff")
                # A later valid packet cannot erase a malformed earlier observation.
                pcap += _dhcp_record(complete)
                self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                       uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                expected: str = "PASS" if version_ihl in {0x45, 0x46} else "ERROR"
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["DHCP capture"])
                if expected == "ERROR":
                    self.assertIn("IPv4", self.capture.problem or "")

    def test_dhcp_option_end_marker_is_required_in_every_packet(self) -> None:
        suffix: bytes
        valid: bool
        for suffix, valid in ((b"", False), (bytes(8), False), (b"\x3c\x01\xff", False),
                              (b"\xff" + bytes(8), True)):
            with self.subTest(suffix=suffix):
                self.setUp()
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                ip_header: bytearray = bytearray(20)
                ip_header[0] = 0x45
                ip_header[9] = 17
                hostname: bytes = DHCP_HOSTNAME.encode("ascii")
                prefix: bytes = (
                    bytes.fromhex("ffffffffffff0211223344550800") + ip_header + bytes(8)
                    + bootp + bytes((12, len(hostname))) + hostname
                )
                frame: bytes = prefix + suffix
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += _dhcp_record(frame)
                # A later complete record must not hide an earlier incomplete packet.
                complete: bytes = prefix + b"\xff"
                pcap += _dhcp_record(complete)
                self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                       uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                self.assertEqual("PASS" if valid else "ERROR", result.verdict)
                self.assertEqual("PASS" if valid else "ERROR", result.outcomes["DHCP capture"])
                if not valid:
                    self.assertIsNone(self.capture.frame)
                    self.assertIn("missing the end marker", "\n".join(self.messages))

    def test_dhcp_end_marker_rejects_non_padding_payload_in_every_packet(self) -> None:
        suffix: bytes
        padding: bytes
        expected: str
        for suffix, padding, expected in (
            (b"", b"", "PASS"),
            (bytes(32), b"", "PASS"),
            (bytes(8), b"\x0c\x01\xff", "PASS"),
            (b'lan_auth_pass="secret"', b"", "ERROR"),
            (bytes(8) + b"Authorization: Bearer a", b"", "ERROR"),
            (bytes.fromhex(CONFIG.gw_id.replace(":", "")), b"", "ERROR"),
        ):
            with self.subTest(suffix=suffix, padding=padding):
                self.setUp()
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                hostname: bytes = DHCP_HOSTNAME.encode("ascii")
                prefix: bytes = (bytes.fromhex("ffffffffffff021122334455080045000000000000000011")
                                 + bytes(10) + bytes(8) + bootp
                                 + bytes((12, len(hostname))) + hostname + b"\xff")
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += _dhcp_record(prefix + suffix, padding)
                # Final valid packet cannot erase an earlier unassessed payload.
                pcap += _dhcp_record(prefix)
                self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                       uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                self.assertEqual(expected, result.verdict, self.messages)
                self.assertEqual(expected, result.outcomes["DHCP capture"])
                if expected == "ERROR":
                    self.assertIn("non-padding bytes after the end marker", "\n".join(self.messages))
                    self.assertNotIn("secret", "\n".join(self.messages))

    def test_dhcp_datagram_lengths_reject_incomplete_or_invalid_payloads(self) -> None:
        field_offset: int
        declared_length: int
        for field_offset, declared_length in (
            (16, 0), (16, 27), (16, 65535),
            (38, 0), (38, 247), (38, 248), (38, 65535),
        ):
            with self.subTest(field_offset=field_offset, declared_length=declared_length):
                self.setUp()
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                hostname: bytes = DHCP_HOSTNAME.encode("ascii")
                frame: bytes = (bytes.fromhex("ffffffffffff021122334455080045000000000000000011")
                                + bytes(10) + bytes(8) + bootp
                                + bytes((12, len(hostname))) + hostname + b"\xff")
                record: bytearray = bytearray(_dhcp_record(frame))
                struct.pack_into("!H", record, 16 + field_offset, declared_length)
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += record + _dhcp_record(frame)
                self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                       uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("ERROR", result.outcomes["DHCP capture"])
                self.assertIn("datagram length", self.capture.problem or "")

    def test_dhcp_does_not_borrow_option_bytes_from_ethernet_trailer(self) -> None:
        suffix: bytes
        expected_problem: str
        for suffix, expected_problem in (
            (b"", "missing the end marker"),
            (b"\x3c", "truncated DHCP option length"),
            (b"\x3c\x02a", "truncated DHCP option value"),
        ):
            with self.subTest(suffix=suffix):
                self.setUp()
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                hostname: bytes = DHCP_HOSTNAME.encode("ascii")
                frame: bytes = (bytes.fromhex("ffffffffffff021122334455080045000000000000000011")
                                + bytes(10) + bytes(8) + bootp
                                + bytes((12, len(hostname))) + hostname + suffix)
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += _dhcp_record(frame, b"\x01a\xff")
                self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                                       uart_hostname=DHCP_HOSTNAME)
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertEqual("ERROR", result.outcomes["DHCP capture"])
                self.assertIn(expected_problem, self.capture.problem or "")

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
        pcap += _dhcp_record(frame)
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                               uart_hostname=DHCP_HOSTNAME)
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
        pcap += _dhcp_record(frame)
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                               uart_hostname=DHCP_HOSTNAME)
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertIn("DHCP disclosure FAIL", "\n".join(self.messages))
        self.setUp()
        extra: bytes = bytes((43, 4)) + b"leak"
        frame = ethernet + ip_header + bytes(8) + bootp + options[:-1] + extra + bytes((255,))
        pcap = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
        pcap += _dhcp_record(frame)
        self.capture = replace(decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                               uart_hostname=DHCP_HOSTNAME)
        result: RunResult = self._run()
        self.assertEqual(("FAIL", 1), (result.verdict, result.exit_code))
        self.assertEqual("FAIL", result.outcomes["DHCP capture"])

    def test_last_option_12_controls_hostname_even_when_empty_or_malformed(self) -> None:
        cases: tuple[tuple[tuple[bytes, ...], str, str], ...] = (
            ((b"espressif", DHCP_HOSTNAME.encode("ascii")), DHCP_HOSTNAME, "PASS"),
            ((DHCP_HOSTNAME.encode("ascii"), b"espressif"), "espressif", "ERROR"),
            ((DHCP_HOSTNAME.encode("ascii"), b""), "", "ERROR"),
            ((DHCP_HOSTNAME.encode("ascii"), b"\xff"), "\ufffd", "ERROR"),
            ((), "", "ERROR"),
        )
        hostnames: tuple[bytes, ...]
        expected_hostname: str
        expected_verdict: str
        for hostnames, expected_hostname, expected_verdict in cases:
            with self.subTest(hostnames=hostnames):
                self.setUp()
                bootp: bytearray = bytearray(240)
                bootp[236:240] = b"\x63\x82\x53\x63"
                ip_header: bytearray = bytearray(20)
                ip_header[0] = 0x45
                ip_header[9] = 17
                options: bytes = b"".join(bytes((12, len(value))) + value for value in hostnames)
                frame: bytes = (
                    bytes.fromhex("ffffffffffff0211223344550800") + ip_header + bytes(8)
                    + bootp + options + bytes((255,))
                )
                pcap: bytes = struct.pack("<IHHIIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1)
                pcap += _dhcp_record(frame)
                self.capture = replace(
                    decode_dhcp_pcap(pcap, "eth0"), ethernet_mac=ETHERNET_MAC,
                    uart_hostname=DHCP_HOSTNAME,
                )
                self.assertEqual(expected_hostname, self.capture.hostname)
                self.assertEqual(tuple((12, value.hex()) for value in hostnames), self.capture.option_values)
                result: RunResult = self._run()
                self.assertEqual(expected_verdict, result.verdict)
                self.assertEqual(expected_verdict, result.outcomes["DHCP capture"])
                if expected_verdict == "ERROR":
                    self.assertIn("last Option 12 hostname", "\n".join(self.messages))

    def test_dhcp_option_12_requires_firmware_short_hostname(self) -> None:
        observed: str
        for observed in ("OtherGateway", ""):
            with self.subTest(observed=observed):
                self.setUp()
                self.capture = CaptureResult(
                    f"{ETHERNET_MAC} DHCP Hostname {observed}", "eth0",
                    source_mac=ETHERNET_MAC, ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME,
                    hostname=observed, option_values=((12, observed.encode().hex()),) if observed else (),
                )
                result: RunResult = self._run()
                self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code))
                self.assertIn("Option 12 hostname", "\n".join(self.messages))

    def test_dhcp_summary_hostname_cannot_replace_missing_option_12(self) -> None:
        self.capture = replace(self.capture, hostname=None, option_values=((53, "01"),))
        result: RunResult = self._run()
        self.assertEqual(("ERROR", 2), (result.verdict, result.exit_code), self.messages)
        self.assertEqual("ERROR", result.outcomes["DHCP capture"])
        self.assertEqual("PASS", result.outcomes["sensitive disclosure scan"])
        self.assertIn(f"DHCP Hostname {DHCP_HOSTNAME}", self.capture.frame or "")
        self.assertIn("last Option 12 hostname <missing>", "\n".join(self.messages))
        self.assertIn("OVERALL VERDICT: ERROR", self._log())

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
            pcap += _dhcp_record(frame)
        capture: CaptureResult = decode_dhcp_pcap(pcap, "eth0")
        self.assertEqual(DHCP_HOSTNAME, capture.hostname)
        self.assertEqual(("espressif",), tuple(frame.hostname for frame in capture.prior_frames))
        self.capture = replace(capture, ethernet_mac=ETHERNET_MAC, uart_hostname=DHCP_HOSTNAME)
        self.assertEqual("PASS", self._run().verdict)


if __name__ == "__main__":
    unittest.main()
