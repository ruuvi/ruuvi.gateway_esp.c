"""Offline implementation regressions; no hardware/network or compliance evidence."""

from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from functools import partial
from pathlib import Path
from typing import Any, Callable
from unittest import mock

import requests
from Crypto.PublicKey import ECC

import test_5_5_2_2_a as target
from lib.config import FACTORY_RESET_MESSAGE
from lib.errors import InvalidSetup
from lib.evidence import EvidenceLog
from lib.gateway import GatewayCfgLanAuthType
from lib.models import DutConfig, RunResult
from lib.serial_dut import CH340_VID, UART_BAUD, SerialPort, SerialTransport, SerialVersions
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 22, 10, 20, 30, tzinfo=timezone.utc)
BANNER: str = (
    "ets Jul 29 2019 12:21:46\r\n"
    "I (42) boot: ESP-IDF v4.0.3 2nd stage bootloader\r\n"
    "I (939) cpu_start: Application information:\r\n"
    "I (950) cpu_start: App version:      v1.15.0\r\n"
    "I (961) cpu_start: ESP-IDF:          v4.2.5\r\n"
    "\x1b[0;32mI (5234) nRF52Fw: [main/1] ### Firmware on nRF52: v2.0.0\x1b[0m\r\n"
)
METRICS: str = 'ruuvigw_info{mac="AA:BB:CC:DD:EE:FF",esp_fw="v1.15.0",nrf_fw="v2.0.0"} 1\n'
BASELINE_JSON: str = ('{"lan_auth_type":"lan_auth_default","lan_auth_user":"Admin",'
                      '"lan_auth_api_key_use":false,"lan_auth_api_key_rw_use":false,'
                      '"gw_mac":"AA:BB:CC:DD:EE:FF","fw_ver":"v1.15.0","nrf52_fw_ver":"v2.0.0"}')


class StatusGateway(DefaultAuthGateway):
    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.baseline: dict[str, Any] = json.loads(BASELINE_JSON)
        # Actual /status.json is network status, not the outbound status-report schema.
        self.status: dict[str, Any] = {
            "ssid": None, "ip": "192.0.2.127", "netmask": "255.255.255.0",
            "gw": "192.0.2.1", "dhcp": "192.0.2.1", "urc": 0,
        }
        self.overrides: dict[tuple[str, str], FakeResponse] = {}
        self.malformed: bool = False
        self.metrics: str = METRICS

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        override: FakeResponse | None = self.overrides.get((call.method, call.path))
        if override is not None:
            return override
        if call.path in ("/ruuvi.json", "/status.json", "/metrics") and call.cookie != f"RUUVISESSION={session.cookie}":
            return FakeResponse(302)
        if call.path == "/metrics" and session.authorized:
            response: FakeResponse = FakeResponse(200, headers={"Content-Type": "text/plain; version=0.0.4"})
            response._content = self.metrics.encode("utf-8")
            return response
        if call.path == "/status.json" and session.authorized:
            return FakeResponse(200, self.status)
        return None

    def _config_response(self) -> FakeResponse:
        return FakeResponse(200, self.baseline, malformed_json=self.malformed)


class StatusSession(FakeSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if kwargs.get("timeout") != (5, 15) or self.trust_env:
            raise AssertionError("gateway requests need bounded timeouts and trust_env=False")
        response: FakeResponse = super().send(request, **kwargs)
        # requests.Session.send normally stores response cookies; the fake transport bypasses it.
        self.cookies.update(response.cookies)
        return response


@dataclass(frozen=True)
class SentPublicRequest:
    request: requests.PreparedRequest
    timeout: tuple[int, int]
    redirects: bool
    verify: bool
    trust_env: bool


class PublicSession(requests.Session):
    def __init__(self, responses: list[FakeResponse | requests.RequestException], evidence: EvidenceLog) -> None:
        super().__init__()
        self.responses: list[FakeResponse | requests.RequestException] = responses
        self.evidence: EvidenceLog = evidence
        self.sent: list[SentPublicRequest] = []
        self.closed: bool = False

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        # The prepared request must already be recorded before any transmission.
        content: str = self.evidence.path.read_text(encoding="utf-8")
        if f"GET {request.url}" not in content:
            raise AssertionError("request was not logged before sending")
        self.sent.append(SentPublicRequest(
            request, kwargs["timeout"], kwargs["allow_redirects"], kwargs["verify"], self.trust_env,
        ))
        response: FakeResponse | requests.RequestException = self.responses.pop(0)
        if isinstance(response, requests.RequestException):
            raise response
        return response

    def close(self) -> None:
        self.closed = True
        super().close()


class IdentificationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root: Path = Path(self.directory.name)
        self.log: EvidenceLog = EvidenceLog.create(self.root / "logs", "case", lambda: NOW)
        self.addCleanup(self.close_log)
        self.gateway: StatusGateway = StatusGateway()
        self.events: list[str] = []
        self.output: list[str] = []
        self.serial: mock.Mock = mock.create_autospec(SerialTransport, instance=True)
        self.serial.preflight.return_value = SerialVersions("4.8.1", "3.5")
        self.serial.discover.side_effect = self.discover
        self.serial.capture.side_effect = self.capture
        self.release_calls: list[tuple[str, str]] = []
        self.release_results: list[int | Exception] = [200, 200]

    def close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST", lambda: NOW)

    def discover(self) -> SerialPort:
        self.assertEqual(["/auth", "/auth", "/ruuvi.json", "/metrics"],
                         [call.path for call in self.gateway.calls])
        self.events.append("discover")
        return SerialPort("/dev/ttyUSB0", 0x1A86, 0x7523)

    def capture(
        self, port: SerialPort, evidence: EvidenceLog, duration: float, *, stop_when: Callable[[str], bool],
    ) -> str:
        self.assertEqual(SerialPort("/dev/ttyUSB0", 0x1A86, 0x7523), port)
        self.assertEqual(8.0, duration)
        self.assertTrue(stop_when(BANNER))
        self.events.append("reset/capture")
        evidence.write("RAW BOOT CONSOLE", BANNER)
        return BANNER

    def lookup(self, repo: str, tag: str, evidence: EvidenceLog) -> target.ReleaseLookup:
        evidence.write("FAKE RELEASE LOOKUP", tag)
        self.release_calls.append((repo, tag))
        self.events.append(tag)
        result: int | Exception = self.release_results.pop(0)
        if isinstance(result, Exception):
            raise result
        return target.ReleaseLookup(f"{repo}/releases/tags/{tag}", result, tag if result == 200 else None)

    def runner(self) -> target.FunctionalTest_5_5_2_2_a:
        return target.FunctionalTest_5_5_2_2_a(
            CONFIG, self.log, lambda: StatusSession(self.gateway), self.serial, self.lookup,
            progress=lambda message: None, random_bytes=lambda size: bytes(range(size)),
            ecc_generate=lambda **kwargs: ECC.construct(curve="P-256", d=7),
            output=self.output.append,
        )

    def execute(self) -> RunResult:
        (self.root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        return target.execute_test_5_5_2_2_a(
            self.root, lambda: StatusSession(self.gateway), self.serial, self.lookup,
            now=lambda: NOW, output=self.output.append,
        )

    def test_constants_and_complete_prepared_sequence(self) -> None:
        self.assertEqual("v4.2.5", target.EXPECTED_IDF_VERSION)
        self.assertEqual(0x1A86, CH340_VID)
        self.assertEqual(115200, UART_BAUD)
        self.assertEqual(8.0, target.UART_CAPTURE_SECONDS)
        result: RunResult = self.runner().run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        expected_password: str = hashlib.sha256(
            f"challenge-1-1:{hashlib.md5(b'Admin:Ruuvi Gateway:00:11:22:33:44:55:66:77').hexdigest()}".encode()
        ).hexdigest()
        expected: list[RecordedRequest] = [
            RecordedRequest("GET", "/auth", None, None, 1, "", None),
            RecordedRequest("POST", "/auth", None, {"login": "Admin", "password": expected_password},
                            1, "", "RUUVISESSION=session-1"),
            RecordedRequest("GET", "/ruuvi.json", None, None, 1, "", "RUUVISESSION=session-1"),
            RecordedRequest("GET", "/metrics", None, None, 1, "", "RUUVISESSION=session-1"),
        ]
        self.assertEqual(expected, self.gateway.calls)
        self.assertEqual(["discover", "reset/capture", "v1.15.0", "v2.0.0"], self.events)
        self.assertEqual([
            ("https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c", "v1.15.0"),
            ("https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c", "v2.0.0"),
        ], self.release_calls)
        self.assertEqual([], self.gateway.config_bodies)
        self.assertEqual("", self.gateway.authorized_sessions[0].challenge)
        check: str
        for check in (target.CRYPTO, target.WEB_UI, target.SDK):
            self.assertEqual("INFORMATIONAL", result.outcomes[check])

    def test_normalization_and_boot_banner(self) -> None:
        raw: str
        for raw in ("v4.2.5", "4.2.5", " V4.2.5 "):
            self.assertEqual("v4.2.5", target.normalize_version(raw))
        self.assertEqual("v1.2.3-rc.1", target.normalize_version(" V1.2.3-RC.1 "))
        self.assertEqual("v4.2.5", target.extract_boot_banner(BANNER).normalized_version)
        for raw in ("unknown", "v4.2", "v4.2.5junk", "v04.2.5", "v4.2.5/path", "v4.2.5+bad"):
            with self.subTest(raw=raw), self.assertRaises(target.GatewayProtocolError):
                target.normalize_version(raw)
        self.assertEqual("v4.2.5", target.extract_boot_banner(
            "\x1b[0;32mI (961) cpu_start: ESP-IDF:          V4.2.5\x1b[0m\r\n",
        ).normalized_version)
        for raw in ("noise", "application ESP-IDF v4.2.5", "ESP-IDF v4.2.5junk 2nd stage bootloader",
                    "I (42) boot: ESP-IDF v4.2.5 2nd stage bootloader\n"):
            with self.subTest(raw=raw), self.assertRaises(InvalidSetup):
                target.extract_boot_banner(raw)
        with self.assertRaises(target.GatewayProtocolError):
            target.extract_boot_banner(f"{BANNER}\nI (962) cpu_start: ESP-IDF: v4.2.4\n")

    def test_complete_malformed_framework_record_is_not_discarded(self) -> None:
        raw: str
        console: str
        for raw in ("v4.2.5junk", "", "v4.2", "v4.2.5/invalid"):
            malformed: str = f"I (962) cpu_start: ESP-IDF: {raw}\n"
            for console in (malformed, BANNER + malformed, malformed + BANNER):
                with self.subTest(raw=raw, console=console):
                    with self.assertRaisesRegex(target.GatewayProtocolError, "invalid semantic version"):
                        target.extract_boot_banner(console)
                    self.gateway = StatusGateway()
                    self.serial.capture.side_effect = None
                    self.serial.capture.return_value = console
                    result: RunResult = self.runner().run()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                    self.assertEqual("ERROR", result.outcomes[target.FRAMEWORK])
                    self.assertEqual([], self.release_calls)

    def test_unterminated_framework_record_is_acquisition_error(self) -> None:
        suffix: str
        for suffix in ("v4.2.5", "v4.2.50", "v4.2.2\r"):
            with self.subTest(suffix=suffix):
                console: str = BANNER.split("I (961) cpu_start:")[0] + f"I (961) cpu_start: ESP-IDF: {suffix}"
                with self.assertRaisesRegex(InvalidSetup, "no application cpu_start: ESP-IDF:"):
                    target.extract_boot_banner(console)
                self.gateway = StatusGateway()
                self.serial.capture.side_effect = None
                self.serial.capture.return_value = console
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes[target.FRAMEWORK])
                self.assertEqual([], self.release_calls)
                self.assertFalse(target.startup_complete(console))

    def test_each_source_and_component_mismatch_fails_without_release_pass_masking(self) -> None:
        source: str
        field: str
        old: str
        new: str
        check: str
        for source in ("json", "metrics", "uart"):
            for field, old, new, check in (
                ("fw_ver", "v1.15.0", "v1.16.3", target.ESP_CONSISTENCY),
                ("nrf52_fw_ver", "v2.0.0", "v2.0.1", target.NRF_CONSISTENCY),
            ):
                with self.subTest(source=source, check=check):
                    self.gateway = StatusGateway()
                    self.output.clear()
                    self.release_results = [200, 200]
                    self.serial.capture.side_effect = None
                    self.serial.capture.return_value = BANNER
                    if source == "json":
                        self.gateway.baseline[field] = new
                    elif source == "metrics":
                        self.gateway.metrics = METRICS.replace(old, new)
                    else:
                        self.serial.capture.return_value = BANNER.replace(old, new)
                    result: RunResult = self.runner().run()
                    self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                    self.assertEqual("FAIL", result.outcomes[check])
                    self.assertEqual("PASS", result.outcomes[target.APPLICATION])
                    self.assertEqual("PASS", result.outcomes[target.COPROCESSOR])
                    lines: list[str] = [line for line in self.output if line.startswith(f"FAIL: {check}:")]
                    self.assertTrue(lines)
                    self.assertIn(new, lines[-1])
                    self.assertIn(old, lines[-1])
                    self.assertIn("UART=", lines[-1])
                    self.assertNotIn("not yet observed", lines[-1])

    def test_metrics_parser_order_comments_escapes_and_normalized_three_source_pass(self) -> None:
        self.gateway.metrics = (
            '# HELP ruuvigw_info Firmware information\nruuvigw_uptime_us 42\n'
            'ruuvigw_info{nrf_fw=" 2.0.0 ",note="comma,quote\\\"slash\\\\newline\\n",'
            'esp_fw=" V1.15.0 ",mac="aa-bb-cc-dd-ee-ff"} 1.0\n'
        )
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = f"\x1b[0;32m{BANNER.replace('v1.15.0', 'V1.15.0')}\x1b[0m"
        result: RunResult = self.runner().run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes[target.ESP_CONSISTENCY])
        self.assertEqual("PASS", result.outcomes[target.NRF_CONSISTENCY])
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("GET http://gateway.local/metrics", content)
        self.assertIn("VERSION SOURCE COMPARISON", content)
        self.assertIn('"uart": "v1.15.0"', content)

    def test_bad_metrics_and_wrong_identity_stop_before_reset(self) -> None:
        body: str
        for body in (
            "", "# ruuvigw_info commented out\n", METRICS + METRICS,
            METRICS.replace('esp_fw="v1.15.0",', ""), METRICS.replace('nrf_fw="v2.0.0"', 'nrf_fw="bad"'),
            METRICS.replace('esp_fw="v1.15.0"', 'esp_fw="v1.15.0",esp_fw="v1.15.0"'),
            METRICS.replace('esp_fw="v1.15.0"', 'esp_fw=v1.15.0'),
            METRICS.replace("} 1", "} 0"), METRICS.replace("} 1", "} 1e"),
            METRICS.replace("} 1", "} NaN"), METRICS.replace("ruuvigw_info", "ruuvigw_info_extra"),
            METRICS.replace("AA:BB:CC:DD:EE:FF", "11:22:33:44:55:66"),
            METRICS.replace("AA:BB:CC:DD:EE:FF", "invalid"),
            METRICS.replace("v1.15.0", r"v1.15.0\t"),
        ):
            with self.subTest(body=body):
                self.gateway = StatusGateway()
                self.gateway.metrics = body
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertIsNone(result.recovery_message)
                self.serial.discover.assert_not_called()
                self.serial.capture.assert_not_called()
        self.assertEqual([], self.release_calls)

    def test_metrics_http_and_transport_errors_stop_before_reset(self) -> None:
        status: int
        for status in (302, 401, 503):
            self.gateway = StatusGateway()
            self.gateway.overrides[("GET", "/metrics")] = FakeResponse(status)
            self.assertEqual(2, self.runner().run().exit_code)
            self.serial.discover.assert_not_called()
        runner: target.FunctionalTest_5_5_2_2_a = self.runner()
        original: Callable[..., requests.Response] = runner.gateway.request

        def request(session: requests.Session, method: str, path: str) -> requests.Response:
            if path == "/metrics":
                raise requests.Timeout("metrics timed out")
            return original(session, method, path)

        runner.gateway.request = request
        self.assertEqual(2, runner.run().exit_code)
        self.serial.capture.assert_not_called()

    def test_uart_requires_installed_versions_not_fatfs_and_waits_for_complete_lines(self) -> None:
        before_nrf: str = BANNER.split("\x1b[0;32mI (5234)")[0]
        self.assertFalse(target.startup_complete(before_nrf))
        self.assertFalse(target.startup_complete(BANNER.rstrip("\r\n")))
        self.assertTrue(target.startup_complete(BANNER))
        self.assertFalse(target.startup_complete("no newline"))
        self.assertFalse(target.startup_complete(BANNER.replace("Firmware on nRF52", "Firmware on FatFS")))
        self.serial.capture.side_effect = None
        console: str
        check: str
        for console, check in (
            (BANNER.replace("App version:", "Other version:"), target.ESP_CONSISTENCY),
            (BANNER.replace("Firmware on nRF52", "Firmware on FatFS"), target.NRF_CONSISTENCY),
            (BANNER.replace("v2.0.0", "bad"), target.NRF_CONSISTENCY),
            (BANNER + "I (3000) nrf52fw: [main/1] Firmware on nRF52: v2.0.1\n", target.NRF_CONSISTENCY),
        ):
            with self.subTest(check=check, console=console):
                self.gateway = StatusGateway()
                self.serial.capture.return_value = console
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes[check])
                self.assertEqual([], self.release_calls)

    def test_http_mismatch_survives_later_missing_uart_and_release_error(self) -> None:
        self.gateway.metrics = METRICS.replace("v2.0.0", "v2.0.1")
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = BANNER.replace("Firmware on nRF52", "Firmware on FatFS")
        result: RunResult = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[target.NRF_CONSISTENCY])
        self.gateway = StatusGateway()
        self.gateway.metrics = METRICS.replace("v1.15.0", "v1.16.3")
        self.serial.capture.return_value = BANNER
        self.release_results = [requests.Timeout("release unavailable")]
        result = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[target.ESP_CONSISTENCY])
        self.assertEqual("ERROR", result.outcomes[target.APPLICATION])

    def test_logged_mixed_case_nrf_tag_completes_and_preserves_expected_idf_failure(self) -> None:
        console: str = (
            "\x1b[0;32mI (939) cpu_start: Application information:\x1b[0m\r\n"
            "\x1b[0;32mI (950) cpu_start: App version:      v1.16.3\x1b[0m\r\n"
            "\x1b[0;32mI (961) cpu_start: ESP-IDF:          v4.2.2\x1b[0m\r\n"
            "\x1b[0;32mI (5216) nRF52Fw: [main/1] Firmware on FatFS: v2.0.0\x1b[0m\r\n"
            "\x1b[0;32mI (5234) nRF52Fw: [main/1] ### Firmware on nRF52: v2.0.0\x1b[0m\r\n"
        )
        before_installed: str = console.split("\x1b[0;32mI (5234)")[0]
        self.assertFalse(target.startup_complete(before_installed))
        self.assertFalse(target.startup_complete(console.rstrip("\r\n")))
        self.assertTrue(target.startup_complete(console))
        self.assertEqual("v2.0.0", target.extract_uart_version(console, target.NRF_VERSION_RE, "nRF52"))
        self.gateway.baseline["fw_ver"] = "v1.16.3"
        self.gateway.metrics = METRICS.replace("v1.15.0", "v1.16.3")
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = console
        result: RunResult = self.execute()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[target.FRAMEWORK])
        self.assertEqual("PASS", result.outcomes[target.ESP_CONSISTENCY])
        self.assertEqual("PASS", result.outcomes[target.NRF_CONSISTENCY])
        self.assertEqual("PASS", result.outcomes[target.APPLICATION])
        self.assertEqual("PASS", result.outcomes[target.COPROCESSOR])
        self.assertEqual(8.0, self.serial.capture.call_args.kwargs["duration"])
        self.assertFalse(any(line.startswith("ERROR:") for line in self.output))

    def test_nrf_marker_is_tag_independent_with_legacy_unprefixed_support(self) -> None:
        line: str
        for line in (
            "I (5234) nRF52Fw: [main/1] ### Firmware on nRF52: v2.0.0\n",
            "I (5234) nrf52fw: [main/1] ### Firmware on nRF52: v2.0.0\n",
            "I (5234) another_tag: [main/1] ### Firmware on nRF52: v2.0.0\n",
            "### Firmware on nRF52: v2.0.0\n",
            "I (5234) nRF52Fw: [main/1] Firmware on nRF52: v2.0.0\n",
            "I (5234) nrf52fw: [main/1] Firmware on nRF52: v2.0.0\n",
        ):
            with self.subTest(line=line):
                self.assertEqual("v2.0.0", target.extract_uart_version(line, target.NRF_VERSION_RE, "nRF52"))
        with self.assertRaises(InvalidSetup):
            target.extract_uart_version("### Firmware on FatFS: v2.0.0\n", target.NRF_VERSION_RE, "nRF52")

    def test_framework_failure_and_later_error_preserve_failure(self) -> None:
        self.gateway.baseline["fw_ver"] = "v1.16.3"
        self.gateway.metrics = METRICS.replace("v1.15.0", "v1.16.3")
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = BANNER.replace("4.2.5", "4.2.2").replace("v1.15.0", "v1.16.3")
        result: RunResult = self.runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.FRAMEWORK])
        self.assertEqual("PASS", result.outcomes[target.COPROCESSOR])
        self.assertIn('"actual": "v4.2.2"', self.log.path.read_text(encoding="utf-8"))
        self.gateway = StatusGateway()
        self.release_results = [requests.ConnectionError("offline")]
        result = self.runner().run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.FRAMEWORK])
        self.assertEqual("ERROR", result.outcomes[target.APPLICATION])

    def test_terminal_explains_framework_failure_and_successful_releases(self) -> None:
        self.gateway.baseline["fw_ver"] = "v1.16.3"
        self.gateway.metrics = METRICS.replace("v1.15.0", "v1.16.3")
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = BANNER.replace("4.2.5", "4.2.2").replace("v1.15.0", "v1.16.3")
        result: RunResult = self.execute()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        expected: list[str] = [
            "PASS: DUT identity and factory-default configuration",
            "FAIL: Application ESP-IDF: observed v4.2.2, expected v4.2.5 (IXIT; bootloader version is not checked)",
            "PASS: ESP32 firmware version consistency: /ruuvi.json=v1.16.3, /metrics=v1.16.3, UART=v1.16.3",
            "PASS: nRF52 firmware version consistency: /ruuvi.json=v2.0.0, /metrics=v2.0.0, UART=v2.0.0",
            "PASS: ruuvi.gateway_esp.c: installed v1.16.3 is a published GitHub release",
            "PASS: NetSecImpl-CoProcessor-Firmware: installed v2.0.0 is a published GitHub release",
            "INFORMATIONAL: mbedTLS, Web-UI, and nRF5 SDK versions are not verified at runtime",
        ]
        self.assertEqual(expected, [line for line in self.output if line.startswith(("PASS:", "FAIL:", "INFORMATIONAL:"))])
        self.assertEqual("Overall verdict: FAIL", self.output[-1])
        self.assertEqual(10, sum(line.startswith("[Step") for line in self.output))
        paths: list[Path] = list((self.root / "logs").glob("test_5_5_2_2_a_*.log"))
        content: str = paths[0].read_text(encoding="utf-8")
        line: str
        for line in expected:
            self.assertIn(line, content)

    def test_terminal_preserves_failure_reason_when_later_lookup_errors(self) -> None:
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = BANNER.replace("4.2.5", "4.2.2")
        self.release_results = [requests.Timeout("GitHub timed out")]
        result: RunResult = self.execute()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertIn(
            "FAIL: Application ESP-IDF: observed v4.2.2, expected v4.2.5 (IXIT; bootloader version is not checked)",
            self.output,
        )
        self.assertIn("ERROR: ruuvi.gateway_esp.c: Timeout: GitHub timed out", self.output)
        self.assertIn("NOT RUN: NetSecImpl-CoProcessor-Firmware (execution stopped)", self.output)
        self.assertEqual("Overall verdict: ERROR", self.output[-1])

    def test_terminal_names_each_missing_release_and_its_installed_tag(self) -> None:
        self.release_results = [404, 404]
        result: RunResult = self.execute()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertIn(
            "FAIL: ruuvi.gateway_esp.c: installed v1.15.0 has no published GitHub release (confirmed HTTP 404)",
            self.output,
        )
        self.assertIn(
            "FAIL: NetSecImpl-CoProcessor-Firmware: installed v2.0.0 has no published GitHub release (confirmed HTTP 404)",
            self.output,
        )
        self.assertFalse(any(line.startswith("ERROR:") for line in self.output))

    def test_terminal_explains_serial_error_without_claiming_version_failure(self) -> None:
        self.serial.capture.side_effect = OSError("port unavailable")
        result: RunResult = self.execute()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertIn("ERROR: NetSecImpl-Framework-Stack: OSError: port unavailable", self.output)
        self.assertIn(
            "NOT RUN: ESP32 firmware version consistency, nRF52 firmware version consistency, "
            "ruuvi.gateway_esp.c, NetSecImpl-CoProcessor-Firmware (execution stopped)", self.output,
        )
        self.assertFalse(any(line.startswith("FAIL:") for line in self.output))
        self.assertEqual([], self.release_calls)

    def test_rom_download_mode_is_setup_error_not_version_failure(self) -> None:
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = (
            "ets Jul 29 2019 12:21:46\r\n"
            "rst:0x1 (POWERON_RESET),boot:0x7 (DOWNLOAD_BOOT(UART0/UART1/SDIO_REI_REO_V2))\r\n"
            "waiting for download\r\n"
        )
        result: RunResult = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes[target.FRAMEWORK])
        self.assertIn("ROM download mode", self.log.path.read_text(encoding="utf-8"))
        self.assertEqual([], self.release_calls)
        self.serial.capture.assert_called_once_with(
            SerialPort("/dev/ttyUSB0", 0x1A86, 0x7523), self.log, duration=8.0, stop_when=target.startup_complete,
        )

    def test_each_missing_release_fails_and_later_error_keeps_prior_failure(self) -> None:
        statuses: list[int | Exception]
        check: str
        for statuses, check in (([404, 200], target.APPLICATION), ([200, 404], target.COPROCESSOR),
                                ([404, requests.Timeout("offline")], target.APPLICATION)):
            with self.subTest(check=check, statuses=statuses):
                self.gateway = StatusGateway()
                self.release_results = list(statuses)
                result: RunResult = self.runner().run()
                self.assertEqual(2 if isinstance(statuses[-1], Exception) else 1, result.exit_code)
                self.assertEqual("FAIL", result.outcomes[check])

    def test_release_infrastructure_errors(self) -> None:
        status: int | Exception
        for status in (403, 429, 500, requests.ConnectionError("DNS"), requests.Timeout("read")):
            with self.subTest(status=status):
                self.gateway = StatusGateway()
                self.release_results = [200, status]
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("PASS", result.outcomes[target.APPLICATION])
                self.assertEqual("ERROR", result.outcomes[target.COPROCESSOR])

    def test_serial_errors(self) -> None:
        ports: list[SerialPort]
        for ports in ([], [SerialPort("one", 0x1A86, 1), SerialPort("two", 0x1A86, 2)]):
            self.gateway = StatusGateway()
            self.serial.discover.side_effect = SerialTransport(enumerate_fn=partial(tuple, ports)).discover
            result: RunResult = self.runner().run()
            self.assertEqual(2, result.exit_code)
            self.serial.capture.assert_not_called()
        self.serial.discover.side_effect = self.discover
        problem: OSError | str
        for problem in (OSError("reset line failed"), "noise without boot banner"):
            self.gateway = StatusGateway()
            self.serial.capture.side_effect = problem if isinstance(problem, OSError) else None
            self.serial.capture.return_value = problem
            result = self.runner().run()
            self.assertEqual(2, result.exit_code)
            self.assertEqual("ERROR", result.outcomes[target.FRAMEWORK])
        self.assertEqual([], self.release_calls)

    def test_bad_or_missing_device_observations_before_reset(self) -> None:
        key: str
        value: Any
        for key in ("fw_ver", "nrf52_fw_ver", "gw_mac"):
            for value in (None, False, 0, [], {}, "", "invalid"):
                with self.subTest(key=key, value=value):
                    self.gateway = StatusGateway()
                    if value is None:
                        del self.gateway.baseline[key]
                    else:
                        self.gateway.baseline[key] = value
                    result: RunResult = self.runner().run()
                    self.assertEqual(2, result.exit_code)
                    self.assertIsNone(result.recovery_message)
                    self.serial.discover.assert_not_called()
        self.gateway = StatusGateway()
        self.gateway.baseline["gw_mac"] = "11:22:33:44:55:66"
        self.assertEqual(2, self.runner().run().exit_code)
        self.serial.discover.assert_not_called()
        self.gateway = StatusGateway()
        self.gateway.malformed = True
        self.assertEqual(2, self.runner().run().exit_code)

    def test_logged_firmware_device_info_passes_without_requesting_network_status(self) -> None:
        self.gateway.baseline["fw_ver"] = "v1.16.3"
        self.gateway.metrics = METRICS.replace("v1.15.0", "v1.16.3")
        self.serial.capture.side_effect = None
        self.serial.capture.return_value = BANNER.replace("v1.15.0", "v1.16.3")
        self.gateway.baseline["nrf52_fw_ver"] = "v2.0.0"
        result: RunResult = self.runner().run()
        self.assertEqual(0, result.exit_code)
        self.assertEqual([
            ("https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c", "v1.16.3"),
            ("https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c", "v2.0.0"),
        ], self.release_calls)
        self.assertEqual(["/auth", "/auth", "/ruuvi.json", "/metrics"], [call.path for call in self.gateway.calls])
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("DEVICE VERSION SOURCE: GET /ruuvi.json", content)
        self.assertIn('"fw_ver": "v1.16.3"', content)
        self.assertIn('"nrf52_fw_ver": "v2.0.0"', content)
        self.assertNotIn("GET http://gateway.local/status.json", content)

    def test_device_info_requires_a_json_object(self) -> None:
        payload: Any
        for payload in ([], None, 123, "text"):
            with self.subTest(payload=payload):
                self.gateway = StatusGateway()
                self.gateway.overrides[("GET", "/ruuvi.json")] = FakeResponse(200, payload)
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertIsNone(result.recovery_message)
                self.serial.discover.assert_not_called()

    def test_independently_decoded_defaults_type_and_value_checks(self) -> None:
        field: str
        replacement: Any
        cases: tuple[tuple[str, Any], ...] = (
            ("lan_auth_type", "lan_auth_ruuvi"), ("lan_auth_type", False),
            ("lan_auth_user", "admin"), ("lan_auth_user", 1),
            ("lan_auth_api_key_use", True), ("lan_auth_api_key_use", 0),
            ("lan_auth_api_key_rw_use", True), ("lan_auth_api_key_rw_use", 0),
        )
        for field, replacement in cases:
            with self.subTest(field=field, replacement=replacement):
                self.gateway = StatusGateway()
                self.gateway.baseline = json.loads(BASELINE_JSON)
                self.gateway.baseline[field] = json.loads(json.dumps(replacement))
                result: RunResult = self.runner().run()
                self.assertEqual(2, result.exit_code)
                self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
                self.serial.discover.assert_not_called()
        for field in ("lan_auth_type", "lan_auth_user", "lan_auth_api_key_use", "lan_auth_api_key_rw_use"):
            self.gateway = StatusGateway()
            del self.gateway.baseline[field]
            self.assertEqual(2, self.runner().run().exit_code)
        self.gateway = StatusGateway()
        self.gateway.baseline["gw_mac"] = "aa-bb-cc-dd-ee-ff"
        self.assertEqual(0, self.runner().run().exit_code)

    def test_unverified_identity_never_recommends_reset(self) -> None:
        mac: Any
        for mac in (None, "bad", "00:11:22:33:44:55", False):
            self.gateway = StatusGateway()
            self.gateway.baseline["lan_auth_api_key_use"] = True
            if mac is None:
                del self.gateway.baseline["gw_mac"]
            else:
                self.gateway.baseline["gw_mac"] = mac
            result: RunResult = self.runner().run()
            self.assertEqual(2, result.exit_code)
            self.assertIsNone(result.recovery_message)
            self.serial.discover.assert_not_called()

    def test_default_login_rejections_and_basic_digest_recovery(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST, GatewayCfgLanAuthType.DEFAULT):
            self.gateway = StatusGateway()
            self.gateway.mode = mode
            self.gateway.overrides[("POST", "/auth")] = FakeResponse(401, {})
            result: RunResult = self.execute()
            self.assertEqual(2, result.exit_code)
            self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
            self.serial.discover.assert_not_called()
            self.assertFalse(any(call.path == "/ruuvi.json" for call in self.gateway.calls))
            self.assertIn(FACTORY_RESET_MESSAGE, self.output)
            self.assertEqual("Overall verdict: ERROR", self.output[-1])
        path: Path
        for path in (self.root / "logs").glob("test_5_5_2_2_a_*.log"):
            content: str = path.read_text(encoding="utf-8")
            self.assertIn(FACTORY_RESET_MESSAGE, content)
            self.assertIn(json.dumps({"mechanism": target.SETUP, "result": "ERROR"}, sort_keys=True), content)

    def test_bad_password_is_rejected(self) -> None:
        runner: target.FunctionalTest_5_5_2_2_a = self.runner()
        runner.config = DutConfig("77:66:55:44:33:22:11:00", CONFIG.gw_mac, CONFIG.gw_hostname)
        result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual([], self.gateway.authorized_sessions)

    def test_corrupt_login_cookie_body_or_outstanding_challenge_aborts(self) -> None:
        fault: str
        for fault in ("missing-cookie", "wrong-cookie", "password", "extra-field", "stale", "consumed"):
            with self.subTest(fault=fault):
                self.gateway = StatusGateway()

                class CorruptSession(StatusSession):
                    def send(
                        self, request: requests.PreparedRequest, *, injected_fault: str = fault, **kwargs: Any,
                    ) -> FakeResponse:
                        if request.method == "POST" and request.url == "http://gateway.local/auth":
                            if injected_fault == "missing-cookie":
                                request.headers.pop("Cookie", None)
                            elif injected_fault == "wrong-cookie":
                                request.headers["Cookie"] = "RUUVISESSION=cross-session"
                            elif injected_fault in ("password", "extra-field"):
                                if not isinstance(request.body, (str, bytes)):
                                    raise AssertionError("authentication request requires a JSON body")
                                body: dict[str, str] = json.loads(request.body)
                                body["password" if injected_fault == "password" else "extra"] = "bad"
                                request.body = json.dumps(body)
                            else:
                                self.challenge = "stale-challenge" if injected_fault == "stale" else ""
                        return super().send(request, **kwargs)

                runner: target.FunctionalTest_5_5_2_2_a = self.runner()
                runner.gateway.session_factory = partial(CorruptSession, self.gateway)
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual([], self.gateway.authorized_sessions)
                self.assertEqual(["/auth", "/auth"], [call.path for call in self.gateway.calls])
                self.serial.discover.assert_not_called()

    def test_gateway_status_and_transport_errors_prevent_serial_access(self) -> None:
        status: int
        for status in (302, 401, 503):
            self.gateway = StatusGateway()
            self.gateway.overrides[("GET", "/ruuvi.json")] = FakeResponse(status)
            self.assertEqual(2, self.runner().run().exit_code)
            self.serial.discover.assert_not_called()
        self.gateway = StatusGateway()
        self.gateway.connection_error = requests.Timeout("DUT offline")
        self.assertEqual(2, self.runner().run().exit_code)
        self.serial.discover.assert_not_called()

    def test_full_real_clients_with_fake_gateway_and_github_transports(self) -> None:
        sessions: list[PublicSession] = []

        def public_session() -> PublicSession:
            tag: str = "v1.15.0" if not sessions else "v2.0.0"
            session: PublicSession = PublicSession([
                FakeResponse(200, {"tag_name": tag, "draft": False, "published_at": "2024-01-01T00:00:00Z"}),
            ], self.log)
            sessions.append(session)
            return session

        runner: target.FunctionalTest_5_5_2_2_a = self.runner()
        runner.release_lookup = partial(target.lookup_release, session_factory=public_session)
        result: RunResult = runner.run()
        self.assertEqual(0, result.exit_code)
        self.assertEqual(2, len(sessions))
        self.assertEqual(4, len(self.gateway.calls))
        self.assertTrue(all(session.closed for session in sessions))
        self.assertEqual([
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/tags/v1.15.0",
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c/releases/tags/v2.0.0",
        ], [session.sent[0].request.url for session in sessions])

    def test_evidence_progress_and_main(self) -> None:
        (self.root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        output: list[str] = []
        result: RunResult = target.execute_test_5_5_2_2_a(
            self.root, lambda: StatusSession(self.gateway), self.serial, self.lookup,
            now=lambda: NOW, output=output.append,
        )
        paths: list[Path] = list((self.root / "logs").glob("test_5_5_2_2_a_*.log"))
        self.assertEqual(1, len(paths))
        self.assertEqual("test_5_5_2_2_a_20260922T102030.000000Z.log", paths[0].name)
        content: str = paths[0].read_text(encoding="utf-8")
        self.assertEqual(1, sum(line.startswith("Open log file: ") for line in output))
        steps: list[str] = [line for line in output if line.startswith("[Step")]
        self.assertEqual(10, len(steps))
        index: int
        line: str
        for index, line in enumerate(steps, 1):
            self.assertTrue(line.startswith(f"[Step {index} out of 10]"))
            self.assertIn(f"] {line}", content)
        for line in ("UTC START", "UTC END", "DURATION SECONDS", "HTTP REQUEST", "HTTP RESPONSE",
                     "RAW BOOT CONSOLE", "VERSION NORMALIZATION", "FINAL RESULT", "4.8.1", "3.5",
                     "mbedTLS", "Web-UI", "v15.3.0", "exact audited build"):
            self.assertIn(line, content)
        self.assertEqual("Overall verdict: PASS", output[-1])
        self.assertIn(
            "PASS: Application ESP-IDF: observed v4.2.5, expected v4.2.5 (IXIT; bootloader version is not checked)",
            output,
        )
        self.assertFalse(any(line.startswith(("FAIL:", "ERROR:", "NOT RUN:")) for line in output))
        self.assertNotIn(CONFIG.gw_id, "\n".join(output))
        with mock.patch.object(target, "execute_test_5_5_2_2_a", return_value=result):
            self.assertEqual(0, target.main())

    def test_initialization_failures_still_log_error(self) -> None:
        output: list[str] = []
        self.serial.preflight.side_effect = ImportError("esptool missing")
        result: RunResult = target.execute_test_5_5_2_2_a(self.root, serial_transport=self.serial, output=output.append)
        self.assertEqual(2, result.exit_code)
        self.serial.discover.assert_not_called()
        paths: list[Path] = list((self.root / "logs").glob("test_5_5_2_2_a_*.log"))
        self.assertIn("ImportError", paths[0].read_text(encoding="utf-8"))
        self.assertEqual("Overall verdict: ERROR", output[-1])
        self.assertIn("ERROR: Initialization: ImportError: esptool missing", output)
        self.assertIn("NOT RUN: Component checks (initialization failed)", output)
        self.serial.preflight.side_effect = None
        result = target.execute_test_5_5_2_2_a(self.root, serial_transport=self.serial, output=output.append)
        self.assertEqual(2, result.exit_code)

    def test_repository_pin_cross_check(self) -> None:
        runner: target.FunctionalTest_5_5_2_2_a = self.runner()
        runner.cmake_path = self.root / "CMakeLists.txt"
        runner.cmake_path.write_text("set(EXPECTED_IDF_VERSION v4.2.4)", encoding="utf-8")
        self.assertEqual(2, runner.run().exit_code)
        self.assertEqual([], self.release_calls)


class ReleaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.log: EvidenceLog = EvidenceLog.create(Path(self.directory.name), "release", lambda: NOW)
        self.addCleanup(lambda: self.log.finish("TEST", lambda: NOW))

    def test_public_request_normalizes_and_strips_credentials(self) -> None:
        session: PublicSession = PublicSession([
            FakeResponse(200, {"tag_name": " V1.15.0 ", "draft": False, "published_at": "2024-01-01T00:00:00Z"})
        ], self.log)
        session.auth = ("user", "secret")
        session.headers["Authorization"] = "Bearer secret"
        session.cookies.set("secret", "cookie")
        session.params = {"token": "secret"}
        with mock.patch.dict("os.environ", {"GITHUB_TOKEN": "secret", "HTTPS_PROXY": "http://invalid"}):
            result: target.ReleaseLookup = target.lookup_release(target.ESP_REPO, " 1.15.0 ", self.log, lambda: session)
        self.assertEqual(target.ReleaseLookup(
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/tags/v1.15.0", 200, "v1.15.0"), result)
        self.assertEqual(1, len(session.sent))
        call: SentPublicRequest = session.sent[0]
        self.assertEqual((5, 15), call.timeout)
        self.assertFalse(call.redirects)
        self.assertFalse(call.trust_env)
        self.assertTrue(call.verify)
        self.assertIsNone(call.request.body)
        self.assertNotIn("Authorization", call.request.headers)
        self.assertNotIn("Cookie", call.request.headers)
        self.assertEqual("ruuvi-etsi-test-5.5-2-2-a", call.request.headers["User-Agent"])
        self.assertTrue(session.closed)
        self.assertNotIn("secret", self.log.path.read_text(encoding="utf-8"))

    def test_definitive_404_requires_successful_repository_identity(self) -> None:
        session: PublicSession = PublicSession([
            FakeResponse(404, {"message": "Not Found"}),
            FakeResponse(200, {"full_name": "ruuvi/ruuvi.gateway_nrf.c"}),
        ], self.log)
        result: target.ReleaseLookup = target.lookup_release(target.NRF_REPO, "v2.0.0", self.log, lambda: session)
        self.assertEqual(404, result.status)
        self.assertEqual([
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c/releases/tags/v2.0.0",
            "https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c",
        ], [call.request.url for call in session.sent])

    def test_transport_status_and_malformed_release_errors(self) -> None:
        responses: list[FakeResponse | requests.RequestException]
        cases: tuple[list[FakeResponse | requests.RequestException], ...] = (
            [requests.ConnectionError("DNS")], [requests.Timeout("timeout")],
            [FakeResponse(403, {}, {"X-RateLimit-Remaining": "0"})], [FakeResponse(429)], [FakeResponse(500)],
            [FakeResponse(302, {}, {"Location": "https://elsewhere.invalid"})],
            [FakeResponse(200, [], malformed_json=True)], [FakeResponse(200, [])], [FakeResponse(200, {})],
            [FakeResponse(200, {"tag_name": "bad"})],
            [FakeResponse(200, {"tag_name": "v9.9.9", "draft": False, "published_at": "today"})],
            [FakeResponse(200, {"tag_name": "v2.0.0", "draft": True, "published_at": "today"})],
            [FakeResponse(200, {"tag_name": "v2.0.0", "draft": False, "published_at": None})],
            [FakeResponse(404), FakeResponse(404)], [FakeResponse(404), FakeResponse(403)],
            [FakeResponse(404), requests.Timeout("offline")],
            [FakeResponse(404), FakeResponse(200, {"full_name": "other/repo"})],
        )
        for responses in cases:
            with self.subTest(responses=responses):
                session: PublicSession = PublicSession(list(responses), self.log)
                with self.assertRaises(InvalidSetup):
                    target.lookup_release(target.NRF_REPO, "v2.0.0", self.log, mock.Mock(return_value=session))
                self.assertTrue(session.closed)
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("X-RateLimit-Remaining: 0", content)


if __name__ == "__main__":
    unittest.main()
