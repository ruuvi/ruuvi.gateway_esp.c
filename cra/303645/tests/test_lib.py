"""Deterministic unit tests for the shared CRA functional-test library."""

import base64
import hashlib
import io
import json
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Tuple
from unittest import mock

import requests
from Crypto.PublicKey import ECC
from Crypto.PublicKey.ECC import EccKey, EccPoint
from requests.cookies import RequestsCookieJar, cookiejar_from_dict

from lib.config import (
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
    GatewayApi as HttpGatewayApi,
    HttpHeader,
    HttpMethod,
)
from lib.models import DutConfig, ProgressReporter


NOW: datetime = datetime(2025, 1, 2, 3, 4, 5, 678901, tzinfo=timezone.utc)
CONFIG: DutConfig = DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)


class RecordingEvidence(EvidenceLog):
    def __init__(self) -> None:
        super().__init__(Path("<memory>"), io.StringIO(), NOW)
        self.entries: List[Tuple[str, Any]] = []
        self.requests: List[requests.PreparedRequest] = []
        self.responses: List[requests.Response] = []

    def write(self, label: str, value: Any = "") -> None:
        self.entries.append((label, value))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.requests.append(request)

    def write_http_response(self, response: requests.Response) -> None:
        self.responses.append(response)


class FakeResponse(requests.Response):
    def __init__(
        self,
        payload: Any = None,
        headers: Optional[Mapping[str, str]] = None,
        cookies: Optional[Mapping[str, str]] = None,
    ) -> None:
        super().__init__()
        self._payload: Any = payload
        self.headers.update(headers or {})
        self.cookies: RequestsCookieJar = cookiejar_from_dict(dict(cookies or {}))

    def json(self, **kwargs: Any) -> Any:
        if isinstance(self._payload, BaseException):
            raise self._payload
        return self._payload


class FakeSession(requests.Session):
    def __init__(
        self,
        response: Any = None,
        error: Optional[requests.RequestException] = None,
    ) -> None:
        super().__init__()
        self.response: Any = response
        self.error: Optional[requests.RequestException] = error
        self.sent: List[Tuple[requests.PreparedRequest, Dict[str, Any]]] = []

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        prepared: requests.PreparedRequest = request.prepare()
        return prepared

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> Any:
        self.sent.append((request, kwargs))
        if self.error is not None:
            raise self.error
        return self.response


class ConfigTestCase(unittest.TestCase):
    def test_load_ui_defaults_and_select_values(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            path: Path = Path(directory) / "defaults.json"
            path.write_text('{"beta": 2, "alpha": 1}', encoding="utf-8")
            self.assertEqual({"beta": 2, "alpha": 1}, load_ui_default_config(path))
            self.assertEqual({"alpha": 1}, default_config_values(["alpha"], path))

    def test_repository_ui_defaults_do_not_expose_authentication_secrets(self) -> None:
        defaults: Dict[str, Any] = load_ui_default_config()
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
            cases: Tuple[Tuple[Path, str], ...] = (
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
                "# local DUT\n"
                "gw_id = 00:11:22:33:44:55:66:77\n"
                "gw_mac=AA:BB:CC:DD:EE:FF\n"
                "gw_hostname = 2001:db8::1\n",
                encoding="utf-8",
            )
            config: DutConfig = load_dut_config(path)
            self.assertEqual("http://[2001:db8::1]", config.base_url)

    def test_load_dut_config_rejects_each_invalid_env_shape(self) -> None:
        valid: str = (
            "gw_id=00:11:22:33:44:55:66:77\n"
            "gw_mac=AA:BB:CC:DD:EE:FF\n"
            "gw_hostname=gateway.local\n"
        )
        cases: Dict[str, str] = {
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
        output: List[str] = []
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
        error: GatewayAuthenticationModeError = GatewayAuthenticationModeError(
            GatewayCfgLanAuthType.BASIC
        )
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
            with mock.patch("lib.evidence.utc_now", return_value=NOW):
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
            with mock.patch("lib.evidence.utc_now", return_value=NOW):
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
                    log.exception(error)
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

    def test_request_prepares_logs_and_sends_expected_http_request(self) -> None:
        response: requests.Response = requests.Response()
        session: FakeSession = FakeSession(response=response)
        result: Any = self.client.request(
            session,
            HttpMethod.POST,
            GatewayApi.CONFIG,
            headers={"X-Test": "yes"},
            json_body={"enabled": True},
            params={"mode": "test"},
            allow_redirects=True,
        )
        prepared: requests.PreparedRequest
        options: Dict[str, Any]
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
        events: List[str] = []

        class OrderedEvidence(RecordingEvidence):
            def write_http_request(self, request: requests.PreparedRequest) -> None:
                events.append("request-log")

            def write_http_response(self, response: requests.Response) -> None:
                events.append("response-log")

        class OrderedSession(FakeSession):
            def send(self, request: requests.PreparedRequest, **kwargs: Any) -> Any:
                events.append("send")
                return super().send(request, **kwargs)

        response: requests.Response = requests.Response()
        client: GatewayClient = GatewayClient(CONFIG, OrderedEvidence())
        client.request(OrderedSession(response=response), HttpMethod.GET, GatewayApi.STATUS)
        self.assertEqual(["request-log", "send", "response-log"], events)

    def test_request_rejects_two_bodies_and_translates_requests_errors(self) -> None:
        with self.assertRaisesRegex(ValueError, "mutually exclusive"):
            self.client.request(FakeSession(), "POST", "/path", json_body={}, data="body")
        session: FakeSession = FakeSession(error=requests.Timeout("timed out"))
        with self.assertRaisesRegex(GatewayConnectionError, r"GET http://gateway.local/path failed"):
            self.client.request(session, "GET", "/path")

    def test_response_json_validates_decoding_and_expected_type(self) -> None:
        self.assertEqual({"ok": True}, self.client.response_json(FakeResponse({"ok": True}), "test", dict))
        with self.assertRaisesRegex(GatewayProtocolError, "malformed JSON"):
            self.client.response_json(FakeResponse(ValueError("bad")), "test")
        with self.assertRaisesRegex(GatewayProtocolError, "must be dict"):
            self.client.response_json(FakeResponse([]), "test", dict)

    def test_challenge_parsers_are_case_insensitive_and_require_all_fields(self) -> None:
        interactive: str = (
            'X-RUUVI-INTERACTIVE realm="gateway", challenge="abc", '
            'session_cookie="RUUVISESSION", session_id="cookie"'
        )
        self.assertEqual("abc", self.client.parse_interactive_challenge(interactive)["challenge"])
        digest: str = 'DIGEST realm="gateway" qop="auth" nonce="n" opaque="o"'
        self.assertEqual("n", self.client.parse_digest_challenge(digest)["nonce"])
        parser: Callable[[Optional[str]], Dict[str, str]]
        header: Optional[str]
        for parser, header in (
            (self.client.parse_interactive_challenge, None),
            (self.client.parse_interactive_challenge, 'x-ruuvi-interactive realm="gateway"'),
            (self.client.parse_digest_challenge, "Basic realm=\"gateway\""),
            (self.client.parse_digest_challenge, 'Digest realm="gateway"'),
        ):
            with self.subTest(parser=parser.__name__, header=header), self.assertRaises(GatewayProtocolError):
                parser(header)
        with self.assertRaisesRegex(GatewayProtocolError, "nonce, opaque, qop"):
            self.client.parse_digest_challenge('Digest realm="gateway"')

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
            'x-ruuvi-interactive realm="gateway", challenge="abc", '
            'session_cookie="RUUVISESSION", session_id="cookie"'
        )
        response: FakeResponse = FakeResponse(
            headers={HttpHeader.WWW_AUTHENTICATE: header},
            cookies={"RUUVISESSION": "cookie"},
        )
        challenge: InteractiveLoginChallenge = (
            self.client.interactive_login_challenge_from_response(
                object(), response, "GET /auth"
            )
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
                    object(),
                    FakeResponse(headers={HttpHeader.WWW_AUTHENTICATE: changed_header}, cookies=cookies),
                    "GET /auth",
                )

    def test_prepare_challenge_encodes_uncompressed_p256_public_key(self) -> None:
        private_key: EccKey = ECC.construct(curve="P-256", d=1)
        session: object = object()
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
            b"\x04"
            + int(server_public.pointQ.x).to_bytes(32, "big")
            + int(server_public.pointQ.y).to_bytes(32, "big")
        )
        header: str = (
            'x-ruuvi-interactive realm="gateway", challenge="abc", '
            'session_cookie="RUUVISESSION", session_id="cookie"'
        )
        response: FakeResponse = FakeResponse(
            {GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT},
            {
                HttpHeader.WWW_AUTHENTICATE: header,
                HttpHeader.RUUVI_ECDH_PUBLIC_KEY: base64.b64encode(server_raw).decode("ascii"),
            },
            {"RUUVISESSION": "cookie"},
        )
        request: InteractiveChallengeRequest = InteractiveChallengeRequest(
            object(), client_private, "unused"
        )
        result: InteractiveAuthChallenge = (
            self.client.parse_interactive_challenge_response(request, response)
        )
        client_public_point: EccPoint = client_private.public_key().pointQ
        server_private_scalar: int = int(server_private.d)
        shared_point: EccPoint = client_public_point * server_private_scalar
        shared_x: int = int(shared_point.x)
        shared: bytes = shared_x.to_bytes(32, "big")
        self.assertEqual(hashlib.sha256(shared).digest(), result.aes_key)
        self.assertEqual(server_raw, result.gateway_public_key_raw)

    def test_parse_challenge_response_reports_auth_mode_and_invalid_key(self) -> None:
        private_key: EccKey = ECC.construct(curve="P-256", d=1)
        request: InteractiveChallengeRequest = InteractiveChallengeRequest(
            object(), private_key, "unused"
        )
        mode_response: FakeResponse = FakeResponse(
            {GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.BASIC}
        )
        with self.assertRaises(GatewayAuthenticationModeError):
            self.client.parse_interactive_challenge_response(request, mode_response)

        header: str = (
            'x-ruuvi-interactive realm="gateway", challenge="abc", '
            'session_cookie="RUUVISESSION", session_id="cookie"'
        )
        invalid_key_raw: bytes
        for invalid_key_raw in (b"short", b"\x02" + (b"\x00" * 64)):
            invalid_key_response: FakeResponse = FakeResponse(
                {},
                {
                    HttpHeader.WWW_AUTHENTICATE: header,
                    HttpHeader.RUUVI_ECDH_PUBLIC_KEY: base64.b64encode(
                        invalid_key_raw
                    ).decode("ascii"),
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

    def test_prepare_and_send_login_uses_cookie_and_digest_response(self) -> None:
        response: requests.Response = requests.Response()
        session: FakeSession = FakeSession(response=response)
        challenge: InteractiveLoginChallenge = InteractiveLoginChallenge(
            session=session,
            challenge={"realm": "gateway", "challenge": "challenge"},
            auth_header="header",
            cookie="cookie",
        )
        request: InteractiveLoginRequest = self.client.prepare_interactive_login_request(
            challenge, "user", "password"
        )
        ha1: str = hashlib.md5(b"user:gateway:password").hexdigest()
        expected: str = hashlib.sha256(
            f"challenge:{ha1}".encode("utf-8")
        ).hexdigest()
        self.assertEqual(expected, request.password_response)
        self.client.send_interactive_login_request(request)
        prepared: requests.PreparedRequest = session.sent[0][0]
        self.assertEqual("RUUVISESSION=cookie", prepared.headers[HttpHeader.COOKIE])
        self.assertEqual({"login": "user", "password": expected}, json.loads(prepared.body))

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
        submit: mock.MagicMock
        with mock.patch.object(
            self.client,
            "request_interactive_challenge",
            return_value=challenge,
        ), mock.patch.object(
            self.client,
            "submit_interactive_authentication",
            return_value=login_response,
        ) as submit:
            result: InteractiveAuthResult = self.client.authenticate_interactive(
                "user", "password"
            )

        submit.assert_called_once_with(challenge, "user", "password")
        self.assertIs(challenge.session, result.session)
        self.assertIs(challenge_response, result.challenge_response)
        self.assertIs(login_response, result.login_response)
        self.assertEqual(b"a" * 32, result.aes_key)


if __name__ == "__main__":
    unittest.main()
