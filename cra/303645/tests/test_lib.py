"""Deterministic unit tests for the shared CRA functional-test library."""

from __future__ import annotations

import base64
import hashlib
import io
import json
import os
import subprocess
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Callable, Mapping
from unittest import mock

import requests
from Crypto.PublicKey import ECC
from Crypto.PublicKey.ECC import EccKey, EccPoint

from lib import evidence, serial_dut
from lib.config import (
    FACTORY_RESET_MESSAGE,
    InvalidConfig,
    default_config_values,
    load_dut_config,
    load_ui_default_config,
    validate_hostname,
)
from lib.errors import (
    GatewayAuthenticationModeError,
    GatewayConnectionError,
    GatewayProtocolError,
    InvalidSetup,
    WebResourceConnectionError,
    WebResourceProtocolError,
    WebResourceRedirectError,
)
from lib.evidence import AssertionEvidence, EvidenceLog, format_utc
from lib.gateway import (
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthChallenge,
    InteractiveAuthResult,
    InteractiveChallengeRequest,
    InteractiveLoginChallenge,
    InteractiveLoginRequest,
)
from lib.http_api import (
    API_INVENTORY,
    EXPECTED_API_INVENTORY,
    ApiRoute,
    HttpHeader,
    HttpMethod,
)
from lib.http_api import (
    GatewayApi as HttpGatewayApi,
)
from lib.models import DutConfig, ProgressReporter
from lib.webresource import PublicResourceResult, fetch_public_resource
from test_support.fake_gateway import FakeResponse

NOW: datetime = datetime(2025, 1, 2, 3, 4, 5, 678901, tzinfo=timezone.utc)
CONFIG: DutConfig = DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)


class RecordingEvidence(EvidenceLog):
    def __init__(self) -> None:
        super().__init__(Path("<memory>"), io.StringIO(), NOW)
        self.entries: list[tuple[str, Any]] = []
        self.requests: list[requests.PreparedRequest] = []
        self.responses: list[requests.Response] = []

    def write(self, label: str, value: Any = "") -> None:
        self.entries.append((label, value))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.requests.append(request)

    def write_http_response(self, response: requests.Response) -> None:
        self.responses.append(response)


class FakeSession(requests.Session):
    def __init__(
        self,
        response: requests.Response | None = None,
        error: requests.RequestException | None = None,
    ) -> None:
        super().__init__()
        self.response: requests.Response | None = response
        self.error: requests.RequestException | None = error
        self.sent: list[tuple[requests.PreparedRequest, dict[str, Any]]] = []

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        prepared: requests.PreparedRequest = request.prepare()
        return prepared

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> requests.Response:
        self.sent.append((request, kwargs))
        if self.error is not None:
            raise self.error
        if self.response is None:
            raise AssertionError("fake transport requires a response or an error")
        return self.response


class WebResourceTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.stream: io.StringIO = io.StringIO()
        self.log: EvidenceLog = EvidenceLog(Path("<memory>"), self.stream, NOW)
        self.session: requests.Session = requests.Session()
        self.send: mock.Mock = mock.Mock()
        self.url: str = "https://public.example/policy"
        self.hosts: frozenset[str] = frozenset({"public.example", "www.public.example"})

    def fetch(self, max_redirects: int = 5) -> PublicResourceResult:
        with mock.patch.object(self.session, "send", self.send):
            return fetch_public_resource(
                self.url, self.log, allowed_hosts=self.hosts, connect_timeout=5, read_timeout=20,
                max_redirects=max_redirects, user_agent="offline-test", session_factory=lambda: self.session,
            )

    def test_fresh_request_and_complete_response_evidence(self) -> None:
        response: FakeResponse = FakeResponse(
            payload="public document", headers={"Content-Type": "text/html; charset=utf-8"},
            cookies={"visitor": "anonymous"},
        )

        def send(request: requests.PreparedRequest, **kwargs: Any) -> requests.Response:
            before_send: str = self.stream.getvalue()
            self.assertIn(f"GET {self.url}", before_send)
            self.assertIn("HTTP REQUEST END", before_send)
            self.assertNotIn("HTTP RESPONSE BEGIN", before_send)
            self.assertEqual({"User-Agent": "offline-test"}, dict(request.headers))
            self.assertIsNone(request.body)
            self.assertEqual((5, 20), kwargs["timeout"])
            self.assertIs(True, kwargs["verify"])
            self.assertIs(False, kwargs["allow_redirects"])
            self.assertEqual({}, kwargs["proxies"])
            return response

        self.session.auth = ("stale", "secret")
        self.session.headers["Authorization"] = "Bearer secret"
        self.session.cookies.set("session", "secret")
        self.session.params = {"token": "secret"}
        self.session.proxies["https"] = "https://proxy.example"
        self.send.side_effect = send
        with mock.patch.object(self.session, "close", wraps=self.session.close) as close:
            close: mock.Mock
            result: PublicResourceResult = self.fetch()
        close.assert_called_once_with()
        self.assertFalse(self.session.trust_env)
        self.assertEqual(self.url, result.final_url)
        self.assertEqual("https", result.final_scheme)
        self.assertEqual("public.example", result.final_host)
        self.assertEqual(len(response.content), result.body_length)
        self.assertEqual((("visitor", "anonymous"),), result.cookies)
        self.assertEqual(tuple(response.headers.items()), result.headers)
        self.assertEqual((), result.redirects)
        self.assertTrue(result.tls_verified)
        transcript: str = self.stream.getvalue()
        self.assertIn("HTTP STATUS 200", transcript)
        self.assertIn('Cookies: {"visitor": "anonymous"}', transcript)
        self.assertIn(response.text, transcript)
        self.assertIn('"authentication_seen": false', transcript)
        self.assertIn("TLS VERIFICATION ENABLED: True", transcript)
        self.assertNotIn("secret", transcript)

    def test_environment_credentials_and_proxies_are_ignored(self) -> None:
        self.send.return_value = FakeResponse(payload="page")
        with mock.patch("requests.sessions.get_netrc_auth", side_effect=AssertionError("netrc used")), \
                mock.patch.dict(os.environ, {"HTTPS_PROXY": "http://proxy.invalid", "REQUESTS_CA_BUNDLE": "/bad"}):
            self.fetch()
        self.assertNotIn("Authorization", self.send.call_args.args[0].headers)
        self.assertEqual({}, self.send.call_args.kwargs["proxies"])
        self.assertIs(True, self.send.call_args.kwargs["verify"])

    def test_all_redirect_statuses_and_exact_budget(self) -> None:
        statuses: tuple[int, ...] = (301, 302, 303, 307, 308)
        responses: list[FakeResponse] = [
            FakeResponse(status, headers={"Location": f"/hop/{index}"})
            for index, status in enumerate(statuses)
        ]
        responses.append(FakeResponse(payload="page", headers={"Content-Type": "text/html"}))
        self.send.side_effect = responses
        result: PublicResourceResult = self.fetch()
        self.assertEqual("https://public.example/hop/4", result.final_url)
        self.assertFalse(result.stopped_at_redirect)
        self.assertEqual(6, self.send.call_count)
        indexes: range = range(5)
        expected_urls: list[str] = [self.url] + [f"https://public.example/hop/{index}" for index in indexes]
        calls: list[mock._Call] = self.send.call_args_list
        actual_urls: list[str] = [call.args[0].url for call in calls]
        actual_statuses: list[int] = [hop.status for hop in result.redirects]
        self.assertEqual(list(statuses), actual_statuses)
        self.assertEqual(expected_urls, actual_urls)
        self.assertEqual(5, self.stream.getvalue().count("REDIRECT HOP:"))

    def test_protocol_relative_redirect_retains_authentication_challenge(self) -> None:
        self.send.side_effect = [
            FakeResponse(302, headers={"Location": "//www.public.example/page#section", "www-authenticate": ""}),
            FakeResponse(payload="page"),
        ]
        result: PublicResourceResult = self.fetch()
        self.assertTrue(result.authentication_seen)
        self.assertEqual("https://www.public.example/page", result.final_url)
        self.assertEqual(self.url, result.redirects[0].from_url)
        self.assertEqual(result.final_url, result.redirects[0].to_url)

    def test_forbidden_redirect_is_observed_without_following(self) -> None:
        targets: tuple[str, ...] = ("http://public.example/plain", "https://login.example/sso", "ftp://public.example/file")
        target: str
        for target in targets:
            with self.subTest(target=target):
                self.send.reset_mock()
                self.send.return_value = FakeResponse(302, headers={"Location": target})
                result: PublicResourceResult = self.fetch()
                self.assertTrue(result.stopped_at_redirect)
                self.assertEqual(self.url, result.final_url)
                self.assertEqual(target, result.redirects[0].to_url)
                self.send.assert_called_once()

    def test_loop_and_overflow_retain_observations(self) -> None:
        destinations: tuple[str, ...] = (self.url, "/another")
        destination: str
        for destination in destinations:
            with self.subTest(destination=destination):
                self.send.return_value = FakeResponse(302, headers={"Location": destination})
                caught: unittest.case._AssertRaisesContext
                with self.assertRaises(WebResourceRedirectError) as caught:
                    self.fetch(max_redirects=0 if destination == "/another" else 5)
                self.assertIsNotNone(caught.exception.observation)
                self.assertEqual(1, len(caught.exception.observation.redirects))
                self.assertIn("REDIRECT HOP:", self.stream.getvalue())

    def test_redirect_protocol_errors_log_response_first(self) -> None:
        locations: tuple[str, ...] = ("", "   ", "https://[broken", "https://user:pass@public.example/", "https://public.example:bad/")
        location: str
        for location in locations:
            with self.subTest(location=location):
                self.send.return_value = FakeResponse(302, headers={"Location": location})
                with self.assertRaises(WebResourceProtocolError):
                    self.fetch()
                self.assertIn("HTTP STATUS 302", self.stream.getvalue())

    def test_invalid_initial_urls_and_limits_never_send(self) -> None:
        urls: tuple[str, ...] = (
            "http://public.example/", "https://other.example/", "https:///no-host",
            "https://user@public.example/", "https://public.example:0/", "https://public.example/a b",
        )
        url: str
        for url in urls:
            with self.subTest(url=url):
                self.url = url
                with self.assertRaises(InvalidSetup):
                    self.fetch()
        self.url = "https://public.example/"
        factory: mock.Mock = mock.Mock(side_effect=AssertionError("invalid configuration reached transport"))
        connect: float
        read: float
        limit: int
        cases: tuple[tuple[float, float, int], ...] = ((0, 20, 5), (5, 0, 5), (float("inf"), 20, 5), (5, float("nan"), 5), (5, 20, -1))
        for connect, read, limit in cases:
            with self.subTest(connect=connect, read=read, limit=limit), self.assertRaises(InvalidSetup):
                fetch_public_resource(self.url, self.log, allowed_hosts=self.hosts, connect_timeout=connect,
                                      read_timeout=read, max_redirects=limit, user_agent="test", session_factory=factory)
        self.send.assert_not_called()
        factory.assert_not_called()

    def test_transport_failures_are_typed_and_close_session(self) -> None:
        errors: tuple[requests.RequestException, ...] = (
            requests.ConnectionError("DNS lookup failed"), requests.ConnectionError("connection refused"),
            requests.exceptions.SSLError("certificate verification failed"), requests.ConnectTimeout("connect"),
            requests.ReadTimeout("read"), requests.exceptions.ChunkedEncodingError("incomplete body"),
        )
        error: requests.RequestException
        for error in errors:
            with self.subTest(error=type(error).__name__):
                self.send.side_effect = error
                caught: unittest.case._AssertRaisesContext
                close: mock.Mock
                with mock.patch.object(self.session, "close") as close, \
                        self.assertRaises(WebResourceConnectionError) as caught:
                    self.fetch()
                close.assert_called_once_with()
                self.assertIs(error, caught.exception.__cause__)
                self.assertIsNone(caught.exception.observation)
                self.assertIn("HTTP REQUEST END", self.stream.getvalue())

    def test_transport_failure_preserves_prior_challenge_and_redirect(self) -> None:
        self.send.side_effect = [
            FakeResponse(302, headers={"Location": "/next", "WWW-Authenticate": "Basic realm=private"}),
            requests.ReadTimeout("read"),
        ]
        caught: unittest.case._AssertRaisesContext
        with self.assertRaises(WebResourceConnectionError) as caught:
            self.fetch()
        result: PublicResourceResult = caught.exception.observation
        self.assertTrue(result.authentication_seen)
        self.assertEqual("https://public.example/next", result.redirects[0].to_url)
        self.assertEqual(2, self.send.call_count)


class ConfigTestCase(unittest.TestCase):
    def test_factory_reset_guidance_uses_completion_signal_not_a_time_threshold(self) -> None:
        self.assertIn("200 ms and off for 200 ms", FACTORY_RESET_MESSAGE)
        self.assertIn("normally about 11 seconds", FACTORY_RESET_MESSAGE)
        self.assertIn("Release only after this completion signal", FACTORY_RESET_MESSAGE)
        self.assertIn("Back up needed settings first", FACTORY_RESET_MESSAGE)
        self.assertIn("do not assume erasure succeeded", FACTORY_RESET_MESSAGE)
        self.assertNotIn("longer than", FACTORY_RESET_MESSAGE)
        self.assertIn(
            "Release only after this completion signal; the Gateway restarts again and opens its "
            "configuration hotspot.",
            FACTORY_RESET_MESSAGE,
        )

    def test_load_ui_defaults_and_select_values(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            path: Path = Path(directory) / "defaults.json"
            path.write_text('{"beta": 2, "alpha": 1}', encoding="utf-8")
            self.assertEqual({"beta": 2, "alpha": 1}, load_ui_default_config(path))
            self.assertEqual({"alpha": 1}, default_config_values(["alpha"], path))

    def test_repository_ui_defaults_do_not_expose_authentication_secrets(self) -> None:
        defaults: dict[str, Any] = load_ui_default_config()
        self.assertNotIn(GatewayCfgDesc.LAN_AUTH_API_KEY, defaults)
        self.assertNotIn(GatewayCfgDesc.LAN_AUTH_API_KEY_RW, defaults)
        self.assertIs(False, defaults[GatewayCfgDesc.LAN_AUTH_API_KEY_USE])
        self.assertIs(False, defaults[GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE])
        self.assertNotIn("password", defaults["wifi_sta_config"])
        self.assertNotIn("password", defaults["wifi_ap_config"])
        self.assertNotIn("mqtt_pass", defaults)

    def test_default_config_rejects_unreadable_malformed_and_non_object_files(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            cases: tuple[tuple[Path, str], ...] = (
                (root / "missing.json", "cannot read default gateway configuration"),
                (root / "malformed.json", "cannot read default gateway configuration"),
                (root / "array.json", "must contain an object"),
            )
            cases[1][0].write_text("{", encoding="utf-8")
            cases[2][0].write_text("[]", encoding="utf-8")
            path: Path
            message: str
            for path, message in cases:
                with self.subTest(path=path.name), self.assertRaisesRegex(InvalidConfig, message):
                    load_ui_default_config(path)

    def test_default_config_reports_all_missing_fields_in_sorted_order(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            path: Path = Path(directory) / "defaults.json"
            path.write_text('{"present": true}', encoding="utf-8")
            with self.assertRaisesRegex(InvalidConfig, r"missing_a, missing_z"):
                default_config_values(["missing_z", "present", "missing_a"], path)

    def test_validate_hostname_accepts_dns_ipv4_and_ipv6(self) -> None:
        hostname: str
        for hostname in ("gateway.local", "gateway.example.", "192.0.2.10", "2001:db8::1"):
            with self.subTest(hostname=hostname):
                validate_hostname(hostname)

    def test_validate_hostname_rejects_unsafe_or_invalid_values(self) -> None:
        hostname: str
        for hostname in (
            "",
            " gateway.local",
            "gateway local",
            "http://gateway.local",
            "gateway.local/path",
            "gateway.local:8080",
            "-gateway.local",
        ):
            with self.subTest(hostname=hostname), self.assertRaises(InvalidConfig):
                validate_hostname(hostname)

    def test_load_dut_config_accepts_comments_spacing_and_ipv6(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            path: Path = Path(directory) / ".env"
            path.write_text(
                "# local DUT\ngw_id = 00:11:22:33:44:55:66:77\ngw_mac=AA:BB:CC:DD:EE:FF\ngw_hostname = 2001:db8::1\n",
                encoding="utf-8",
            )
            config: DutConfig = load_dut_config(path)
            self.assertEqual("http://[2001:db8::1]", config.base_url)

    def test_load_dut_config_rejects_each_invalid_env_shape(self) -> None:
        valid: str = "gw_id=00:11:22:33:44:55:66:77\ngw_mac=AA:BB:CC:DD:EE:FF\ngw_hostname=gateway.local\n"
        cases: dict[str, str] = {
            "missing": valid.replace("gw_mac=AA:BB:CC:DD:EE:FF\n", ""),
            "unknown": valid + "extra=value\n",
            "duplicate": valid + "gw_hostname=other.local\n",
            "malformed": valid + "no-separator\n",
            "gateway id": valid.replace("00:11:22:33:44:55:66:77", "invalid"),
            "gateway mac": valid.replace("AA:BB:CC:DD:EE:FF", "invalid"),
            "shell-shaped value": valid.replace(
                "00:11:22:33:44:55:66:77",
                "$(touch /tmp/never-run)",
            ),
        }
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            path: Path = Path(directory) / ".env"
            name: str
            content: str
            for name, content in cases.items():
                with self.subTest(case=name):
                    path.write_text(content, encoding="utf-8")
                    with self.assertRaises(InvalidConfig):
                        load_dut_config(path)


class ModelsAndApiTestCase(unittest.TestCase):
    def test_base_url_formats_dns_ipv4_and_ipv6(self) -> None:
        self.assertEqual("http://gateway.local", CONFIG.base_url)
        self.assertEqual(
            "http://192.0.2.1",
            DutConfig(CONFIG.gw_id, CONFIG.gw_mac, "192.0.2.1").base_url,
        )
        self.assertEqual(
            "http://[2001:db8::1]",
            DutConfig(CONFIG.gw_id, CONFIG.gw_mac, "2001:db8::1").base_url,
        )

    def test_progress_reporter_numbers_steps(self) -> None:
        output: list[str] = []
        reporter: ProgressReporter = ProgressReporter(output.append, total=2)
        reporter.step("prepare")
        reporter.step("verify")
        self.assertEqual(
            ["[Step 1 out of 2] prepare", "[Step 2 out of 2] verify"],
            output,
        )

    def test_api_inventory_is_unique_and_matches_expected_routes(self) -> None:
        self.assertIs(HttpGatewayApi, GatewayApi)
        self.assertEqual(26, len(API_INVENTORY))
        self.assertEqual(len(API_INVENTORY), len(set(API_INVENTORY)))
        self.assertEqual(EXPECTED_API_INVENTORY, set(API_INVENTORY))
        self.assertIn(ApiRoute(HttpMethod.GET, GatewayApi.STATUS), API_INVENTORY)
        self.assertIn(ApiRoute(HttpMethod.DELETE, GatewayApi.AUTH), API_INVENTORY)

    def test_authentication_mode_error_preserves_type_and_message(self) -> None:
        error: GatewayAuthenticationModeError = GatewayAuthenticationModeError(GatewayCfgLanAuthType.BASIC)
        self.assertIsInstance(error, GatewayProtocolError)
        self.assertIsInstance(error, InvalidSetup)
        self.assertEqual(GatewayCfgLanAuthType.BASIC, error.auth_type)
        self.assertIn("lan_auth_basic", str(error))


class EvidenceLogTestCase(unittest.TestCase):
    def test_format_utc_normalizes_an_offset(self) -> None:
        offset: timezone = timezone(timedelta(hours=7))
        self.assertEqual("2025-01-02T03:04:05.678901Z", format_utc(NOW.astimezone(offset)))

    def test_create_uses_collision_suffix_and_write_serializes_dataclass(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            with mock.patch.object(evidence, "utc_now", return_value=NOW):
                first: EvidenceLog = EvidenceLog.create(root, "evidence", lambda: NOW)
                first.write("ASSERTION", AssertionEvidence("check", "PASS", {"b": 2, "a": 1}))
                first.finish("PASS", lambda: NOW + timedelta(seconds=1.25))
                second: EvidenceLog = EvidenceLog.create(root, "evidence", lambda: NOW)
                second.finish("PASS", lambda: NOW)

            self.assertTrue(second.path.name.endswith("_1.log"))
            content: str = first.path.read_text(encoding="utf-8")
            self.assertIn(
                'ASSERTION: {"actual": {"a": 1, "b": 2}, "description": "check", "result": "PASS"}',
                content,
            )
            self.assertIn("DURATION SECONDS: 1.250", content)
            self.assertIn("OVERALL VERDICT: PASS", content)
            ended_stamp: str = "2025-01-02T03:04:06.928901Z"
            self.assertIn(f"[{ended_stamp}] UTC END: {ended_stamp}", content)
            self.assertIn(f"[{ended_stamp}] DURATION SECONDS: 1.250", content)
            self.assertIn(f"[{ended_stamp}] OVERALL VERDICT: PASS", content)

    def test_http_request_response_and_exception_are_recorded(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(evidence, "utc_now", return_value=NOW):
                log: EvidenceLog = EvidenceLog.create(Path(directory), "http", lambda: NOW)
                request: requests.PreparedRequest = requests.Request(
                    "POST",
                    "http://gateway.local/ruuvi.json",
                    headers={"X-Test": "yes"},
                    data=b"\xffbody",
                ).prepare()
                response: requests.Response = requests.Response()
                response.status_code = 200
                response.headers["Content-Type"] = "application/json"
                response.cookies.set("session", "cookie")
                response._content = b'{"ok":true}'
                response.encoding = "utf-8"
                log.write_http_request(request)
                log.write_http_response(response)
                error: RuntimeError
                try:
                    raise RuntimeError("failure detail")
                except RuntimeError as error:
                    log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
                log.finish("ERROR", lambda: NOW)

            content: str = log.path.read_text(encoding="utf-8")
            self.assertIn("POST http://gateway.local/ruuvi.json", content)
            self.assertIn(r"\xffbody", content)
            self.assertIn("HTTP STATUS 200", content)
            self.assertIn('Cookies: {"session": "cookie"}', content)
            self.assertIn("RuntimeError: failure detail", content)


class GatewayClientTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.evidence: RecordingEvidence = RecordingEvidence()
        self.client: GatewayClient = GatewayClient(
            CONFIG,
            self.evidence,
            random_bytes=lambda size: bytes(range(size)),
        )

    def test_new_session_preserves_request_authentication_with_matching_netrc(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            netrc_path: Path = Path(directory) / ".netrc"
            netrc_path.write_text(
                "machine gateway.local login netrc-user password netrc-password\n",
                encoding="utf-8",
            )
            netrc_path.chmod(0o600)
            authorization: str | None
            with mock.patch.dict(os.environ, {"NETRC": str(netrc_path)}):
                for authorization in (None, "Bearer explicit-token"):
                    session: requests.Session
                    with self.subTest(authorization=authorization), self.client.new_session() as session:
                        headers: dict[str, str] = (
                            {} if authorization is None else {HttpHeader.AUTHORIZATION: authorization}
                        )
                        send: mock.MagicMock = mock.MagicMock(return_value=FakeResponse())
                        with mock.patch.object(
                            session,
                            requests.Session.send.__name__,
                            new=send,
                        ):
                            self.client.request(session, HttpMethod.GET, GatewayApi.STATUS, headers=headers)
                        send.assert_called_once()
                        prepared: requests.PreparedRequest = send.call_args[0][0]
                        self.assertEqual(authorization, prepared.headers.get(HttpHeader.AUTHORIZATION))
                        self.assertIs(prepared, self.evidence.requests[-1])

    def test_request_prepares_logs_and_sends_expected_http_request(self) -> None:
        response: requests.Response = requests.Response()
        session: FakeSession = FakeSession(response=response)
        result: requests.Response = self.client.request(
            session,
            HttpMethod.POST,
            GatewayApi.CONFIG,
            headers={"X-Test": "yes"},
            json_body={"enabled": True},
            params={"mode": "test"},
            allow_redirects=True,
        )
        prepared: requests.PreparedRequest
        options: dict[str, Any]
        prepared, options = session.sent[0]
        self.assertIs(response, result)
        self.assertEqual("ruuvi-cra-functional-test", prepared.headers[HttpHeader.USER_AGENT])
        self.assertEqual("yes", prepared.headers["X-Test"])
        self.assertEqual(b'{"enabled": true}', prepared.body)
        self.assertEqual("mode=test", prepared.url.split("?", 1)[1])
        self.assertEqual({"timeout": (5, 15), "allow_redirects": True}, options)
        self.assertEqual([prepared], self.evidence.requests)
        self.assertEqual([response], self.evidence.responses)

    def test_request_logs_before_send_and_logs_response_after_send(self) -> None:
        events: list[str] = []

        class OrderedEvidence(RecordingEvidence):
            def write_http_request(self, request: requests.PreparedRequest) -> None:
                events.append("request-log")

            def write_http_response(self, response: requests.Response) -> None:
                events.append("response-log")

        class OrderedSession(FakeSession):
            def send(self, request: requests.PreparedRequest, **kwargs: Any) -> requests.Response:
                events.append("send")
                return super().send(request, **kwargs)

        logged_response: requests.Response = requests.Response()
        client: GatewayClient = GatewayClient(CONFIG, OrderedEvidence())
        client.request(OrderedSession(response=logged_response), HttpMethod.GET, GatewayApi.STATUS)
        self.assertEqual(["request-log", "send", "response-log"], events)

    def test_request_rejects_two_bodies_and_translates_requests_errors(self) -> None:
        with self.assertRaisesRegex(ValueError, "mutually exclusive"):
            self.client.request(FakeSession(), "POST", "/path", json_body={}, data="body")
        session: FakeSession = FakeSession(error=requests.Timeout("timed out"))
        with self.assertRaisesRegex(GatewayConnectionError, r"GET http://gateway.local/path failed"):
            self.client.request(session, "GET", "/path")

    def test_response_json_validates_decoding_and_expected_type(self) -> None:
        self.assertEqual({"ok": True}, self.client.response_json(FakeResponse(payload={"ok": True}), "test", dict))
        with self.assertRaisesRegex(GatewayProtocolError, "malformed JSON"):
            self.client.response_json(FakeResponse(json_error=ValueError("bad")), "test")
        with self.assertRaisesRegex(GatewayProtocolError, "must be dict"):
            self.client.response_json(FakeResponse(payload=[]), "test", dict)

    def test_challenge_parsers_are_case_insensitive_and_require_all_fields(self) -> None:
        interactive: str = (
            'X-RUUVI-INTERACTIVE realm="gateway", challenge="abc", session_cookie="RUUVISESSION", session_id="cookie"'
        )
        self.assertEqual("abc", self.client.parse_interactive_challenge(interactive)["challenge"])
        digest: str = 'DIGEST realm="gateway" qop="auth" nonce="n" opaque="o"'
        self.assertEqual("n", self.client.parse_digest_challenge(digest)["nonce"])
        parser: Callable[[str | None], dict[str, str]]
        header: str | None
        for parser, header in (
            (self.client.parse_interactive_challenge, None),
            (self.client.parse_interactive_challenge, 'x-ruuvi-interactive realm="gateway"'),
            (self.client.parse_digest_challenge, 'Basic realm="gateway"'),
            (self.client.parse_digest_challenge, 'Digest realm="gateway"'),
        ):
            with self.subTest(parser=parser.__name__, header=header), self.assertRaises(GatewayProtocolError):
                parser(header)
        with self.assertRaisesRegex(GatewayProtocolError, "nonce, opaque, qop"):
            self.client.parse_digest_challenge('Digest realm="gateway"')

    def test_challenge_parsers_require_scheme_token_boundary(self) -> None:
        parser: Callable[[str | None], dict[str, str]]
        scheme: str
        parameters: str
        for parser, scheme, parameters in (
            (
                self.client.parse_interactive_challenge,
                "x-ruuvi-interactive",
                'realm="gateway", challenge="abc", session_cookie="RUUVISESSION", session_id="cookie"',
            ),
            (
                self.client.parse_digest_challenge,
                "Digest",
                'realm="gateway", qop="auth", nonce="n", opaque="o"',
            ),
        ):
            separator: str
            for separator in (" ", "\t", "  ", " \t"):
                with self.subTest(scheme=scheme, separator=separator):
                    self.assertEqual(
                        "gateway",
                        parser(scheme.upper() + separator + parameters)["realm"],
                    )
            suffix: str
            for suffix in ("-evil ", "ive ", "", ",", "\r", "\n", "\u00a0"):
                with self.subTest(scheme=scheme, suffix=suffix), self.assertRaisesRegex(
                    GatewayProtocolError, "did not advertise"
                ):
                    parser(scheme + suffix + parameters)
            with self.subTest(scheme=scheme, header="scheme only"), self.assertRaisesRegex(
                GatewayProtocolError, "did not advertise"
            ):
                parser(scheme)

    def test_auth_header_helpers_are_deterministic(self) -> None:
        self.assertEqual("Basic dXNlcjpwQHNz", self.client.authorization_header_basic("user", "p@ss"))
        expected_ha1: str = hashlib.md5(b"user:realm:p@ss").hexdigest()
        self.assertEqual(expected_ha1, self.client.calculate_digest_ha1("user", "realm", "p@ss"))
        header: str = self.client.authorization_header_digest(
            "user",
            "p@ss",
            "GET",
            "/status.json",
            {"realm": "realm", "nonce": "nonce", "qop": "auth", "opaque": "opaque"},
        )
        self.assertIn('cnonce="AAECAwQFBgcICQoL"', header)
        self.assertIn('uri="/status.json"', header)
        self.assertRegex(header, r'response="[0-9a-f]{32}"')

    def test_login_challenge_checks_cookie_contract(self) -> None:
        header: str = (
            'x-ruuvi-interactive realm="gateway", challenge="abc", session_cookie="RUUVISESSION", session_id="cookie"'
        )
        response: FakeResponse = FakeResponse(
            headers={HttpHeader.WWW_AUTHENTICATE: header},
            cookies={"RUUVISESSION": "cookie"},
        )
        challenge: InteractiveLoginChallenge = self.client.interactive_login_challenge_from_response(
            FakeSession(), response, "GET /auth"
        )
        self.assertEqual("cookie", challenge.cookie)

        changed_header: str
        cookies: Mapping[str, str]
        for changed_header, cookies in (
            (header, {}),
            (header.replace('session_cookie="RUUVISESSION"', 'session_cookie="OTHER"'), {"RUUVISESSION": "cookie"}),
            (header.replace('session_id="cookie"', 'session_id="different"'), {"RUUVISESSION": "cookie"}),
        ):
            with self.subTest(header=changed_header, cookies=cookies), self.assertRaises(GatewayProtocolError):
                self.client.interactive_login_challenge_from_response(
                    FakeSession(),
                    FakeResponse(headers={HttpHeader.WWW_AUTHENTICATE: changed_header}, cookies=cookies),
                    "GET /auth",
                )

    def test_prepare_challenge_encodes_uncompressed_p256_public_key(self) -> None:
        private_key: EccKey = ECC.construct(curve="P-256", d=1)
        session: FakeSession = FakeSession()
        client: GatewayClient = GatewayClient(
            CONFIG,
            self.evidence,
            session_factory=lambda: session,
            ecc_generate=lambda **kwargs: private_key,
        )
        request: InteractiveChallengeRequest = client.prepare_interactive_challenge_request()
        raw: bytes = base64.b64decode(request.public_key_b64)
        self.assertIs(session, request.session)
        self.assertIs(private_key, request.private_key)
        self.assertEqual(65, len(raw))
        self.assertEqual(4, raw[0])

    def test_parse_challenge_response_derives_matching_ecdh_key(self) -> None:
        client_private: EccKey = ECC.construct(curve="P-256", d=1)
        server_private: EccKey = ECC.construct(curve="P-256", d=2)
        server_public: EccKey = server_private.public_key()
        server_raw: bytes = (
            b"\x04" + int(server_public.pointQ.x).to_bytes(32, "big") + int(server_public.pointQ.y).to_bytes(32, "big")
        )
        header: str = (
            'x-ruuvi-interactive realm="gateway", challenge="abc", session_cookie="RUUVISESSION", session_id="cookie"'
        )
        response: FakeResponse = FakeResponse(
            200,
            {GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT},
            {
                HttpHeader.WWW_AUTHENTICATE: header,
                HttpHeader.RUUVI_ECDH_PUBLIC_KEY: base64.b64encode(server_raw).decode("ascii"),
            },
            {"RUUVISESSION": "cookie"},
        )
        request: InteractiveChallengeRequest = InteractiveChallengeRequest(FakeSession(), client_private, "unused")
        result: InteractiveAuthChallenge = self.client.parse_interactive_challenge_response(request, response)
        client_public_point: EccPoint = client_private.public_key().pointQ
        server_private_scalar: int = int(server_private.d)
        shared_point: EccPoint = client_public_point * server_private_scalar
        shared_x: int = int(shared_point.x)
        shared: bytes = shared_x.to_bytes(32, "big")
        self.assertEqual(hashlib.sha256(shared).digest(), result.aes_key)
        self.assertEqual(server_raw, result.gateway_public_key_raw)

    def test_parse_challenge_response_reports_auth_mode_and_invalid_key(self) -> None:
        private_key: EccKey = ECC.construct(curve="P-256", d=1)
        request: InteractiveChallengeRequest = InteractiveChallengeRequest(FakeSession(), private_key, "unused")
        mode_response: FakeResponse = FakeResponse(payload={GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.BASIC})
        with self.assertRaises(GatewayAuthenticationModeError):
            self.client.parse_interactive_challenge_response(request, mode_response)

        header: str = (
            'x-ruuvi-interactive realm="gateway", challenge="abc", session_cookie="RUUVISESSION", session_id="cookie"'
        )
        invalid_key_raw: bytes
        for invalid_key_raw in (b"short", b"\x02" + (b"\x00" * 64)):
            invalid_key_response: FakeResponse = FakeResponse(
                200,
                {},
                {
                    HttpHeader.WWW_AUTHENTICATE: header,
                    HttpHeader.RUUVI_ECDH_PUBLIC_KEY: base64.b64encode(invalid_key_raw).decode("ascii"),
                },
                {"RUUVISESSION": "cookie"},
            )
            with self.subTest(key=invalid_key_raw), self.assertRaisesRegex(
                GatewayProtocolError,
                "invalid gateway ECDH public key",
            ):
                self.client.parse_interactive_challenge_response(
                    request,
                    invalid_key_response,
                )

    def test_parse_challenge_response_rejects_point_at_infinity(self) -> None:
        request: InteractiveChallengeRequest = InteractiveChallengeRequest(
            FakeSession(), ECC.construct(curve="P-256", d=1), "unused"
        )
        response: FakeResponse = FakeResponse(
            200,
            {},
            {
                HttpHeader.WWW_AUTHENTICATE: (
                    'x-ruuvi-interactive realm="gateway", challenge="abc", '
                    'session_cookie="RUUVISESSION", session_id="cookie"'
                ),
                HttpHeader.RUUVI_ECDH_PUBLIC_KEY: base64.b64encode(b"\x04" + bytes(64)).decode("ascii"),
            },
            {"RUUVISESSION": "cookie"},
        )
        with self.assertRaisesRegex(GatewayProtocolError, "invalid gateway ECDH public key"):
            self.client.parse_interactive_challenge_response(request, response)
        labels: list[str] = [label for label, _ in self.evidence.entries]
        self.assertNotIn("ECDH SHARED SECRET", labels)
        self.assertNotIn("ECDH AES KEY", labels)

    def test_prepare_and_send_login_uses_cookie_and_digest_response(self) -> None:
        response: requests.Response = requests.Response()
        session: FakeSession = FakeSession(response=response)
        challenge: InteractiveLoginChallenge = InteractiveLoginChallenge(
            session=session,
            challenge={"realm": "gateway", "challenge": "challenge"},
            auth_header="header",
            cookie="cookie",
        )
        request: InteractiveLoginRequest = self.client.prepare_interactive_login_request(challenge, "user", "password")
        ha1: str = hashlib.md5(b"user:gateway:password").hexdigest()
        expected: str = hashlib.sha256(f"challenge:{ha1}".encode()).hexdigest()
        self.assertEqual(expected, request.password_response)
        self.client.send_interactive_login_request(request)
        prepared: requests.PreparedRequest = session.sent[0][0]
        self.assertEqual("RUUVISESSION=cookie", prepared.headers[HttpHeader.COOKIE])
        body: bytes | str | None = prepared.body
        if body is None:
            self.fail("prepared interactive login request has no body")
        self.assertEqual({"login": "user", "password": expected}, json.loads(body))

    def test_authenticate_interactive_returns_challenge_and_login_results(self) -> None:
        challenge_response: requests.Response = requests.Response()
        login_response: requests.Response = requests.Response()
        session: requests.Session = requests.Session()
        challenge: InteractiveAuthChallenge = InteractiveAuthChallenge(
            session=session,
            challenge={"realm": "gateway", "challenge": "value"},
            auth_header="header",
            cookie="cookie",
            challenge_response=challenge_response,
            auth_payload={GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT},
            gateway_public_key_raw=b"public-key",
            aes_key=b"a" * 32,
        )
        submit: mock.MagicMock = mock.MagicMock(return_value=login_response)
        with mock.patch.object(
            self.client,
            "request_interactive_challenge",
            return_value=challenge,
        ), mock.patch.object(
            self.client,
            "submit_interactive_authentication",
            new=submit,
        ):
            result: InteractiveAuthResult = self.client.authenticate_interactive("user", "password")

        submit.assert_called_once_with(challenge, "user", "password")
        self.assertIs(challenge.session, result.session)
        self.assertIs(challenge_response, result.challenge_response)
        self.assertIs(login_response, result.login_response)
        self.assertEqual(b"a" * 32, result.aes_key)


class SerialDutTests(unittest.TestCase):
    def setUp(self) -> None:
        self.evidence: RecordingEvidence = RecordingEvidence()
        self.port: serial_dut.SerialPort = serial_dut.SerialPort("/dev/ttyUSB0", 0x1A86, 0x7523)

    def test_discovery_selects_only_one_matching_vid(self) -> None:
        other: serial_dut.SerialPort = serial_dut.SerialPort("/dev/ttyACM0", 0x1234, 0x9999)
        missing_usb: serial_dut.SerialPort = serial_dut.SerialPort("/dev/ttyS0", None, None)
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            enumerate_fn=lambda: (missing_usb, self.port, other),
        )
        self.assertEqual(self.port, transport.discover())
        enumeration: mock.Mock = mock.Mock(return_value=(other,))
        with self.assertRaisesRegex(InvalidSetup, "found 0 matches.*ttyACM0"):
            serial_dut.discover_serial_port(enumeration)
        enumeration.return_value = (self.port, serial_dut.SerialPort("/dev/ttyUSB1", 0x1A86, 1))
        with self.assertRaisesRegex(InvalidSetup, "found 2 matches.*ttyUSB0.*ttyUSB1"):
            serial_dut.discover_serial_port(enumeration)
        enumeration.side_effect = OSError("USB enumeration failed")
        with self.assertRaisesRegex(OSError, "USB enumeration failed"):
            serial_dut.discover_serial_port(enumeration)

    def test_preflight_versions_missing_modules_and_default_enumerator(self) -> None:
        importer: mock.Mock = mock.Mock(side_effect=[
            mock.Mock(__version__="4.8.1"), mock.Mock(__version__="3.5"),
        ])
        versions: serial_dut.SerialVersions = serial_dut.preflight_serial(importer)
        self.assertEqual(serial_dut.SerialVersions("4.8.1", "3.5"), versions)
        self.assertEqual([mock.call("esptool"), mock.call("serial")], importer.call_args_list)
        importer.side_effect = ImportError("esptool unavailable")
        with self.assertRaisesRegex(ImportError, "unavailable"):
            serial_dut.preflight_serial(importer)
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(preflight_fn=lambda: versions)
        self.assertEqual(versions, transport.preflight())
        module: mock.Mock = mock.Mock()
        module.comports.return_value = (self.port,)
        importer = mock.Mock(return_value=module)
        self.assertEqual((self.port,), serial_dut.enumerate_ports(importer))
        importer.assert_called_once_with("serial.tools.list_ports")
        module.comports.assert_called_once_with()

    def test_preflight_prefers_imported_esptool_without_a_subprocess(self) -> None:
        importer: mock.Mock = mock.Mock(side_effect=[
            mock.Mock(__version__="4.8.1"), mock.Mock(__version__="3.5"),
        ])
        finder: mock.Mock = mock.Mock()
        command: mock.Mock = mock.Mock()
        result: serial_dut.SerialVersions = serial_dut.preflight_serial(importer, finder, command)
        self.assertEqual(serial_dut.SerialVersions("4.8.1", "3.5"), result)
        finder.assert_not_called()
        command.assert_not_called()

    def test_preflight_accepts_both_path_entry_points_and_records_source(self) -> None:
        executable: str
        for executable in ("esptool", "esptool.py"):
            with self.subTest(executable=executable):
                path: str = f"/opt/esp idf/{executable}"
                importer: mock.Mock = mock.Mock(side_effect=[
                    ModuleNotFoundError("No module named 'esptool'", name="esptool"),
                    mock.Mock(__version__="3.5"),
                ])
                finder: mock.Mock = mock.Mock(side_effect=[path] if executable == "esptool" else [None, path])
                command: mock.Mock = mock.Mock(return_value=subprocess.CompletedProcess(
                    [path, "version"], 0, stdout="esptool.py v3.1-dev\n3.1-dev\n", stderr="",
                ))
                result: serial_dut.SerialVersions = serial_dut.preflight_serial(importer, finder, command)
                self.assertEqual(serial_dut.SerialVersions("3.1-dev", "3.5", path), result)
                self.assertEqual([mock.call("esptool"), mock.call("serial")], importer.call_args_list)
                expected_search: list[mock._Call] = [mock.call("esptool")]
                if executable == "esptool.py":
                    expected_search.append(mock.call("esptool.py"))
                self.assertEqual(expected_search, finder.call_args_list)
                command.assert_called_once_with(
                    [path, "version"], capture_output=True, text=True, timeout=10.0, check=True,
                )

    def test_preflight_reports_missing_module_and_path_executable(self) -> None:
        importer: mock.Mock = mock.Mock(side_effect=ModuleNotFoundError("esptool missing", name="esptool"))
        finder: mock.Mock = mock.Mock(return_value=None)
        command: mock.Mock = mock.Mock()
        with self.assertRaisesRegex(InvalidSetup, "neither esptool nor esptool.py is on PATH"):
            serial_dut.preflight_serial(importer, finder, command)
        command.assert_not_called()

    def test_preflight_does_not_mask_missing_internal_or_pyserial_dependencies(self) -> None:
        importer: mock.Mock = mock.Mock(side_effect=ModuleNotFoundError("reedsolo missing", name="reedsolo"))
        finder: mock.Mock = mock.Mock()
        command: mock.Mock = mock.Mock()
        with self.assertRaisesRegex(ModuleNotFoundError, "reedsolo missing"):
            serial_dut.preflight_serial(importer, finder, command)
        finder.assert_not_called()
        command.assert_not_called()
        importer.side_effect = [mock.Mock(__version__="4.8.1"), ModuleNotFoundError("serial missing", name="serial")]
        with self.assertRaisesRegex(ModuleNotFoundError, "serial missing"):
            serial_dut.preflight_serial(importer, finder, command)
        finder.assert_not_called()
        command.assert_not_called()
        importer.side_effect = [
            ModuleNotFoundError("esptool missing", name="esptool"),
            ModuleNotFoundError("serial missing", name="serial"),
        ]
        finder.return_value = "/opt/esptool.py"
        command.return_value = subprocess.CompletedProcess(["/opt/esptool.py", "version"], 0, stdout="3.1-dev\n")
        with self.assertRaisesRegex(ModuleNotFoundError, "serial missing"):
            serial_dut.preflight_serial(importer, finder, command)

    def test_preflight_rejects_failed_timed_out_or_empty_version_command(self) -> None:
        error: Exception
        for error in (
            OSError("cannot execute"), subprocess.TimeoutExpired(["esptool.py", "version"], 10),
            subprocess.CalledProcessError(1, ["esptool.py", "version"]),
        ):
            with self.subTest(error=error):
                importer: mock.Mock = mock.Mock(side_effect=ModuleNotFoundError("missing", name="esptool"))
                finder: mock.Mock = mock.Mock(return_value="/opt/esptool.py")
                command: mock.Mock = mock.Mock(side_effect=error)
                context: unittest.case._AssertRaisesContext
                with self.assertRaisesRegex(InvalidSetup, "esptool version preflight failed") as context:
                    serial_dut.preflight_serial(importer, finder, command)
                self.assertIs(error, context.exception.__cause__)
                importer.assert_called_once_with("esptool")
        command = mock.Mock(return_value=subprocess.CompletedProcess(["esptool.py", "version"], 0, stdout=" \n"))
        with self.assertRaisesRegex(InvalidSetup, "produced no version"):
            serial_dut.preflight_serial(importer, finder, command)

    def test_default_open_sets_inactive_lines_before_open(self) -> None:
        module: mock.Mock = mock.Mock()
        serial: mock.Mock = module.Serial.return_value

        def inspect_open() -> None:
            self.assertFalse(serial.dtr)
            self.assertFalse(serial.rts)
            self.assertEqual("/dev/ttyUSB0", serial.port)

        serial.open.side_effect = inspect_open
        importer: mock.Mock = mock.Mock(return_value=module)
        self.assertIs(serial, serial_dut.open_serial("/dev/ttyUSB0", 115200, 0.25, importer))
        importer.assert_called_once_with("serial")
        module.Serial.assert_called_once_with(port=None, baudrate=115200, timeout=0.25)
        serial.open.assert_called_once_with()

    def test_reset_sequence_bounded_reads_raw_evidence_and_close(self) -> None:
        serial: mock.Mock = mock.Mock(spec=serial_dut.SerialConnection)
        opener: mock.Mock = mock.Mock(return_value=serial)
        clock: mock.Mock = mock.Mock(side_effect=[0.0, 0.0, 0.3, 0.49, 0.5])
        sleep: mock.Mock = mock.Mock()
        command: mock.Mock = mock.Mock(return_value=subprocess.CompletedProcess(
            [], 0, stdout="MAC: 11:22:33:44:55:66\nHard resetting via RTS pin...\n", stderr="",
        ))
        preflight: mock.Mock = mock.Mock(return_value=serial_dut.SerialVersions("4.8.1", "3.5"))
        order: mock.Mock = mock.Mock()
        order.attach_mock(command, "command")
        order.attach_mock(opener, "open")

        def read(size: int) -> bytes:
            self.assertEqual(4096, size)
            self.assertGreater(serial.timeout, 0)
            self.assertLessEqual(serial.timeout, 0.25)
            self.assertIn(("ESPTOOL RESET RESULT", serial_dut.SerialCommandResult(
                0, command.return_value.stdout, "",
            )), self.evidence.entries)
            return b"boot\xff\r\n"

        serial.read.side_effect = read
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            open_fn=opener, monotonic=clock, sleep=sleep, preflight_fn=preflight,
            run_command=command, python_executable="/test/python",
        )
        self.assertEqual(serial_dut.SerialVersions("4.8.1", "3.5"), transport.preflight())
        result: str = transport.capture(self.port, self.evidence, duration=0.5)
        expected_command: list[str] = [
            "/test/python", "-m", "esptool", "--port", "/dev/ttyUSB0", "--before",
            "default_reset", "--after", "hard_reset", "read_mac",
        ]
        self.assertEqual([
            mock.call.command(expected_command, capture_output=True, text=True, timeout=20.0, check=False),
            mock.call.open("/dev/ttyUSB0", 115200, 0.25),
        ], order.mock_calls)
        preflight.assert_called_once_with()
        self.assertIn(("ESPTOOL RESET COMMAND", tuple(expected_command)), self.evidence.entries)
        opener.assert_called_once_with("/dev/ttyUSB0", 115200, 0.25)
        sleep.assert_not_called()
        self.assertEqual("boot\\xff\r\n" * 3, result)
        self.assertEqual(3, serial.read.call_count)
        serial.reset_input_buffer.assert_not_called()
        serial.close.assert_called_once_with()
        self.assertIn(("RAW BOOT CONSOLE", result), self.evidence.entries)

    def test_capture_early_stop_receives_accumulated_text_and_closes(self) -> None:
        serial: mock.Mock = mock.Mock(spec=serial_dut.SerialConnection)
        serial.read.side_effect = [b"first\nsec", b"ond\n", AssertionError("read beyond completion")]
        stop: mock.Mock = mock.Mock(side_effect=[False, True])
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            open_fn=mock.Mock(return_value=serial), monotonic=lambda: 0.0,
            preflight_fn=lambda: serial_dut.SerialVersions("4.8.1", "3.5"),
            run_command=mock.Mock(return_value=subprocess.CompletedProcess([], 0, stdout="reset", stderr="")),
        )
        self.assertEqual("first\nsecond\n", transport.capture(self.port, self.evidence, stop_when=stop))
        self.assertEqual([mock.call("first\nsec"), mock.call("first\nsecond\n")], stop.call_args_list)
        self.assertEqual(2, serial.read.call_count)
        self.assertIn(("RAW BOOT CONSOLE", "first\nsecond\n"), self.evidence.entries)
        serial.close.assert_called_once_with()

    def test_capture_incomplete_stop_times_out_and_callback_error_closes(self) -> None:
        serial: mock.Mock = mock.Mock(spec=serial_dut.SerialConnection)
        serial.read.return_value = b"partial"
        stop: mock.Mock = mock.Mock(return_value=False)
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            open_fn=mock.Mock(return_value=serial), monotonic=mock.Mock(side_effect=[0.0, 0.0, 30.0]),
            preflight_fn=lambda: serial_dut.SerialVersions("4.8.1", "3.5"),
            run_command=mock.Mock(return_value=subprocess.CompletedProcess([], 0, stdout="reset", stderr="")),
        )
        self.assertEqual("partial", transport.capture(self.port, self.evidence, stop_when=stop))
        stop.assert_called_once_with("partial")
        serial.close.assert_called_once_with()
        serial.reset_mock()
        transport.monotonic = lambda: 0.0
        stop.side_effect = ValueError("predicate failed")
        with self.assertRaisesRegex(ValueError, "predicate failed"):
            transport.capture(self.port, self.evidence, stop_when=stop)
        serial.close.assert_called_once_with()
        self.assertIn(("RAW BOOT CONSOLE", "partial"), self.evidence.entries)

    def test_path_reset_and_read_failure_preserve_partial_log_and_close(self) -> None:
        serial: mock.Mock = mock.Mock(spec=serial_dut.SerialConnection)
        serial.read.side_effect = [b"partial", OSError("read failed")]
        command: mock.Mock = mock.Mock(return_value=subprocess.CompletedProcess([], 0, stdout="reset", stderr=""))
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            open_fn=mock.Mock(return_value=serial), monotonic=lambda: 0.0,
            preflight_fn=lambda: serial_dut.SerialVersions("3.1-dev", "3.5", "/opt/esp idf/esptool.py"),
            run_command=command,
        )
        with self.assertRaisesRegex(OSError, "read failed"):
            transport.capture(self.port, self.evidence)
        command.assert_called_once_with([
            "/opt/esp idf/esptool.py", "--port", "/dev/ttyUSB0", "--before", "default_reset",
            "--after", "hard_reset", "read_mac",
        ], capture_output=True, text=True, timeout=20.0, check=False)
        serial.close.assert_called_once_with()
        self.assertIn(("RAW BOOT CONSOLE", "partial"), self.evidence.entries)

    def test_reset_errors_prevent_open_and_preserve_command_output(self) -> None:
        error: Exception
        expected: serial_dut.SerialCommandResult
        for error, expected in (
            (OSError("cannot execute"), serial_dut.SerialCommandResult(None, "", "")),
            (subprocess.TimeoutExpired("esptool", 20, output=b"partial\xff", stderr=b"timeout"),
             serial_dut.SerialCommandResult(None, "partial\\xff", "timeout")),
            (subprocess.CalledProcessError(2, "esptool", output="partial", stderr="failed"),
             serial_dut.SerialCommandResult(2, "partial", "failed")),
        ):
            opener: mock.Mock = mock.Mock()
            command: mock.Mock = mock.Mock(side_effect=error)
            transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
                open_fn=opener, run_command=command,
                preflight_fn=lambda: serial_dut.SerialVersions("4.8.1", "3.5"),
            )
            context: unittest.case._AssertRaisesContext
            with self.subTest(error=error), self.assertRaisesRegex(InvalidSetup, "read_mac/reset failed") as context:
                transport.capture(self.port, self.evidence)
            self.assertIs(error, context.exception.__cause__)
            self.assertIn(("ESPTOOL RESET RESULT", expected), self.evidence.entries)
            opener.assert_not_called()
        command.side_effect = None
        command.return_value = subprocess.CompletedProcess([], 2, stdout="download mode", stderr="connection failed")
        with self.assertRaisesRegex(InvalidSetup, "exited with status 2"):
            transport.capture(self.port, self.evidence)
        self.assertIn(("ESPTOOL RESET RESULT", serial_dut.SerialCommandResult(
            2, "download mode", "connection failed",
        )), self.evidence.entries)
        opener.assert_not_called()

    def test_bad_duration_open_failure_and_logging_failure(self) -> None:
        opener: mock.Mock = mock.Mock(side_effect=OSError("port busy"))
        command: mock.Mock = mock.Mock(return_value=subprocess.CompletedProcess([], 0, stdout="reset", stderr=""))
        transport: serial_dut.SerialTransport = serial_dut.SerialTransport(
            open_fn=opener, run_command=command,
            preflight_fn=lambda: serial_dut.SerialVersions("4.8.1", "3.5"),
        )
        duration: float
        for duration in (0.0, -1.0, float("inf"), float("nan")):
            with self.subTest(duration=duration), self.assertRaises(InvalidSetup):
                transport.capture(self.port, self.evidence, duration)
        opener.assert_not_called()
        command.assert_not_called()
        with self.assertRaisesRegex(OSError, "port busy"):
            transport.capture(self.port, self.evidence)
        command.reset_mock()
        with mock.patch.object(self.evidence, "write", side_effect=OSError("disk full")), \
                self.assertRaisesRegex(OSError, "disk full"):
            transport.capture(self.port, self.evidence)
        command.assert_not_called()
        self.assertEqual(1, opener.call_count)
        serial: mock.Mock = mock.Mock(spec=serial_dut.SerialConnection)
        transport.open_fn = mock.Mock(return_value=serial)
        transport.monotonic = mock.Mock(side_effect=[0.0, 30.0])
        with mock.patch.object(self.evidence, "write", side_effect=[None, None, None, OSError("disk full")]), \
                self.assertRaisesRegex(OSError, "disk full"):
            transport.capture(self.port, self.evidence)
        serial.close.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
