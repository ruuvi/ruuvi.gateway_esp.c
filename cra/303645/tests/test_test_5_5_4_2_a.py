"""Offline implementation tests for Unit a. These are not ETSI compliance evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import tempfile
import unittest
from collections.abc import Mapping
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, cast
from unittest.mock import MagicMock, patch

import requests
from Crypto.PublicKey import ECC

import test_5_5_4_2_a as runner
from lib.config import AUTHENTICATION_DEFAULT_FIELDS, FACTORY_RESET_MESSAGE
from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import GatewayApi, HttpHeader, HttpMethod
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import (
    DefaultAuthGateway,
    FakeRequestKey,
    FakeResponse,
    FakeSession,
    RecordedRequest,
    env_text,
)

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
RO: str = base64.urlsafe_b64encode(b"r" * 32).decode().rstrip("=")
RW: str = base64.urlsafe_b64encode(b"w" * 32).decode().rstrip("=")
KEY_BODY: dict[str, str] = {GatewayCfgDesc.LAN_AUTH_API_KEY: RO, GatewayCfgDesc.LAN_AUTH_API_KEY_RW: RW}
CLEAR_BODY: dict[str, str] = {GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""}


def fixed_ecc(**_kwargs: str) -> ECC.EccKey:
    return ECC.construct(curve="P-256", d=1)


def fixed_now() -> datetime:
    return datetime(2026, 1, 2, 3, 4, 5, tzinfo=timezone.utc)


class FixedRandom:
    def __init__(self) -> None:
        self.key_count: int = 0

    def __call__(self, size: int) -> bytes:
        if size == 32:
            self.key_count += 1
            return (b"r" if self.key_count == 1 else b"w") * size
        return b"x" * size


@dataclass(frozen=True)
class Fault:
    key: FakeRequestKey
    occurrence: int = 1
    response: FakeResponse | None = None
    error: Exception | None = None
    apply_body: bool = False


class DenialGateway(DefaultAuthGateway):
    def __init__(self) -> None:
        with patch("test_support.fake_gateway.ECC.generate", side_effect=fixed_ecc):
            super().__init__(CONFIG)
        self.faults: list[Fault] = []
        self.config_reads: int = 0
        self.config_edit: Callable[[dict[str, Any], int], None] | None = None
        self.auth_edit: Callable[[dict[str, Any], int], None] | None = None
        self.corrupt_login: Callable[[requests.PreparedRequest], None] | None = None
        self.sessions: list[BoundarySession] = []
        self.accept_any_admin_password: bool = False
        self.rotate_failed_username: str | None = None
        self.authorize_failed_original: bool = False
        self.failed_original_cookies: set[str] = set()

    def session(self) -> BoundarySession:
        session: BoundarySession = BoundarySession(self)
        self.sessions.append(session)
        return session

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        fault: Fault
        for fault in self.faults:
            matches: list[RecordedRequest] = [item for item in self.calls if item.key == fault.key]
            if call.key == fault.key and len(matches) == fault.occurrence:
                if fault.apply_body:
                    self._apply_config(call.body)
                if fault.error is not None:
                    raise fault.error
                return fault.response
        if call.path == GatewayApi.INFO and call.method == HttpMethod.GET:
            authorized: bool = self._is_real_bearer(call.bearer_token, call.path)
            if call.bearer_token is None:
                active: FakeSession
                for active in self.authorized_sessions:
                    authorized |= self._has_session_cookie(active, {HttpHeader.COOKIE: call.cookie or ""})
            if authorized:
                return FakeResponse(404)
        if call.path == GatewayApi.CONNECT_WPS and call.method == HttpMethod.POST:
            return FakeResponse(403 if call.bearer_token == self.rw_key and self.rw_key else 401)
        return None

    def _config_response(self) -> FakeResponse:
        self.config_reads += 1
        response: FakeResponse = super()._config_response()
        payload: dict[str, Any] = response.json()
        # Include unrelated nested settings to detect partial-parser side effects without masking them.
        payload["mqtt"] = {"enabled": False, "port": 1883, "topics": ["sensor"]}
        if self.config_edit is not None:
            self.config_edit(payload, self.config_reads)
        return FakeResponse(response.status_code, json.loads(json.dumps(payload)))

    def _auth_response(
        self, session: FakeSession, method: str, headers: Mapping[str, str], body: Any,
    ) -> FakeResponse:
        cookie: str = headers.get(HttpHeader.COOKIE, "")
        if (method == HttpMethod.GET and self.authorize_failed_original
                and cookie in self.failed_original_cookies):
            return FakeResponse(200, {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode})
        response: FakeResponse = super()._auth_response(session, method, headers, body)
        if method == HttpMethod.POST and response.status_code == 401 and isinstance(body, dict):
            if (self.accept_any_admin_password and body.get("login") == "Admin"
                    and session.challenge and self._has_session_cookie(session, headers)):
                session.authorized = True
                session.challenge = ""
                self.authorized_sessions.append(session)
                return FakeResponse(200, {"authenticated": True})
            username: str = body.get("login", "")
            if (self.rotate_failed_username == username
                    or self.rotate_failed_username == "invalid" and username.startswith("invalid-")):
                self.failed_original_cookies.add(cookie)
                return FakeResponse(401, {"authenticated": False},
                                    cookies={"RUUVISESSION": f"replacement-{session.number}"})
        if method == HttpMethod.GET and self.auth_edit is not None:
            payload: dict[str, Any] = response.json()
            self.auth_edit(payload, session.number)
            return FakeResponse(response.status_code, json.loads(json.dumps(payload)),
                                response.headers, response.cookies.get_dict())
        return response

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        response: FakeResponse = super()._unauthorized_response(method, path, headers)
        if response.status_code == 302:
            response.headers[HttpHeader.LOCATION] = "http://gateway.local/#auth"
            if path != GatewayApi.STATUS:
                response.cookies.set("RUUVI_PREV_URL", path)
        return response


class BoundarySession(FakeSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if self.trust_env or kwargs.get("timeout") != (5, 15) or kwargs.get("allow_redirects") is not False:
            raise AssertionError("unsafe transport settings")
        if (isinstance(self.gateway, DenialGateway) and self.gateway.corrupt_login is not None
                and request.method == HttpMethod.POST and request.url == f"{CONFIG.base_url}{GatewayApi.AUTH}"):
            self.gateway.corrupt_login(request)
        return super().send(request, **kwargs)


@dataclass(frozen=True)
class ExpectedRequest:
    scheme: str
    method: str
    path: str
    session: int
    body: Any = None


def login_body(session: int, *, invalid: bool = False, wrong_password: bool = False) -> dict[str, str]:
    username: str = f"invalid-{base64.urlsafe_b64encode(b'x' * 9).decode()}" if invalid else "Admin"
    password: str = base64.urlsafe_b64encode(b"x" * 24).decode() if invalid or wrong_password else CONFIG.gw_id
    ha1: str = hashlib.md5(f"{username}:Ruuvi Gateway:{password}".encode()).hexdigest()
    response: str = hashlib.sha256(f"challenge-{session}-1:{ha1}".encode()).hexdigest()
    return {"login": username, "password": response}


class DenialRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work: Path = Path(self.temp.name)
        (self.work / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.gateway: DenialGateway = DenialGateway()
        self.output: list[str] = []
        self.log: str = ""

    def execute(self) -> RunResult:
        before: set[Path] = set((self.work / "logs").glob("*.log"))
        result: RunResult = runner.execute_test_5_5_4_2_a(
            self.work, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, self.output.append,
        )
        logs: list[Path] = list(set((self.work / "logs").glob("*.log")) - before)
        self.log = logs[-1].read_text(encoding="utf-8")
        return result

    def fault(self, scheme: str, method: str, path: str, status: int, occurrence: int = 1) -> None:
        self.gateway.faults.append(Fault(FakeRequestKey(scheme, method, path), occurrence, FakeResponse(status)))

    def assert_keys_cleared(self) -> None:
        self.assertEqual((self.gateway.ro_key, self.gateway.rw_key), ("", ""))
        self.assertIn(CLEAR_BODY, self.gateway.config_bodies)
        self.assertEqual(
            [(item.bearer_token, item.method, item.path) for item in self.gateway.calls[-2:]],
            [(RO, "GET", "/history"), (RW, "GET", "/history")],
        )

    def test_complete_prepared_request_sequence_and_evidence(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((result.verdict, result.exit_code), ("PASS", 0), self.log[-1500:])
        expected: list[ExpectedRequest] = [
            ExpectedRequest("none", "GET", "/auth", 1),
            ExpectedRequest("none", "POST", "/auth", 1, login_body(1)),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("none", "GET", "/ruuvi.json", 2),
            ExpectedRequest("none", "GET", "/history", 3),
            ExpectedRequest("none", "GET", "/status.json", 4),
            ExpectedRequest("none", "GET", "/info.json", 5),
            ExpectedRequest("none", "GET", "/auth", 6),
            ExpectedRequest("none", "DELETE", "/auth", 7),
            ExpectedRequest("none", "GET", "/auth", 8),
            ExpectedRequest("none", "POST", "/auth", 8, login_body(8, invalid=True)),
            ExpectedRequest("none", "GET", "/auth", 8),
            ExpectedRequest("none", "GET", "/auth", 9),
            ExpectedRequest("none", "POST", "/auth", 9, login_body(9, wrong_password=True)),
            ExpectedRequest("none", "GET", "/auth", 9),
            ExpectedRequest("Basic", "GET", "/ruuvi.json", 10),
            ExpectedRequest("Basic", "GET", "/status.json", 11),
            ExpectedRequest("Basic", "GET", "/history", 12),
            ExpectedRequest("Basic", "GET", "/info.json", 13),
            ExpectedRequest("Digest", "GET", "/ruuvi.json", 14),
            ExpectedRequest("Digest", "GET", "/status.json", 15),
            ExpectedRequest("Digest", "GET", "/history", 16),
            ExpectedRequest("Digest", "GET", "/info.json", 17),
            ExpectedRequest("Bearer", "GET", "/info.json", 18),
            ExpectedRequest("none", "POST", "/ruuvi.json", 19, {}),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("Basic", "POST", "/ruuvi.json", 20, {}),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("Digest", "POST", "/ruuvi.json", 21, {}),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("none", "POST", "/ruuvi.json", 1, KEY_BODY),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("Bearer", "GET", "/history", 22),
            ExpectedRequest("Bearer", "GET", "/history", 23),
            ExpectedRequest("Bearer", "POST", "/ruuvi.json", 24, {}),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("Bearer", "POST", "/connect_wps", 25),
            ExpectedRequest("none", "GET", "/ruuvi.json", 1),
            ExpectedRequest("none", "GET", "/auth", 26),
            ExpectedRequest("none", "POST", "/auth", 26, login_body(26)),
            ExpectedRequest("none", "POST", "/ruuvi.json", 26, CLEAR_BODY),
            ExpectedRequest("none", "GET", "/auth", 27),
            ExpectedRequest("none", "POST", "/auth", 27, login_body(27)),
            ExpectedRequest("none", "GET", "/ruuvi.json", 27),
            ExpectedRequest("Bearer", "GET", "/history", 28),
            ExpectedRequest("Bearer", "GET", "/history", 29),
        ]
        observed: list[ExpectedRequest] = [
            ExpectedRequest(call.scheme, call.method, call.path, call.session_number, call.body)
            for call in self.gateway.calls
        ]
        self.assertEqual(observed, expected)
        call: RecordedRequest
        for call in self.gateway.calls:
            self.assertFalse(call.allow_redirects)
            if call.session_number in (1, 26, 27) and call.method != "GET" or (
                call.path == "/ruuvi.json" and call.session_number in (1, 27)
            ):
                self.assertEqual(call.cookie, f"RUUVISESSION=session-{call.session_number}")
            if call.scheme != "none":
                self.assertIsNone(call.cookie)
            if call.scheme == "Basic":
                credential: bytes = f"invalid-{'eHh4' * 3}:{'eHh4' * 8}".encode()
                self.assertEqual(call.authorization, f"Basic {base64.b64encode(credential).decode()}")
            if call.scheme == "Digest":
                parameters: dict[str, str] = requests.utils.parse_dict_header(call.authorization[7:])
                ha1: str = hashlib.md5(f"invalid-{'eHh4' * 3}:Ruuvi Gateway:{'eHh4' * 8}".encode()).hexdigest()
                ha2: str = hashlib.md5(f"{call.method}:{call.path}".encode()).hexdigest()
                digest: str = hashlib.md5(f"{ha1}:{'eHh4' * 4}:00000001:{'eHh4' * 4}:auth:{ha2}".encode()).hexdigest()
                self.assertEqual(parameters["response"], digest)
                self.assertEqual(parameters["uri"], call.path)
        number: int
        for number in (8, 9):
            self.assertEqual([
                (call.method, call.path, call.cookie) for call in self.gateway.calls
                if call.session_number == number
            ], [
                ("GET", "/auth", None),
                ("POST", "/auth", f"RUUVISESSION=session-{number}"),
                ("GET", "/auth", f"RUUVISESSION=session-{number}"),
            ])
        bearers: list[str | None] = [call.bearer_token for call in self.gateway.calls if call.scheme == "Bearer"]
        self.assertEqual(bearers, ["eHh4" * 8, RO, RW, RO, RW, RO, RW])
        self.assertEqual(set(result.outcomes.values()), {"PASS"})
        self.assertEqual(result.outcomes["AuthMech-LAN-WebUI-Default: unauthenticated-denial check"], "PASS")
        self.assertNotIn("AuthMech-LAN-WebUI-Unauthenticated", result.outcomes)
        self.assertIn("AuthMech-LAN-WebUI-Default: unauthenticated-denial check", self.log)
        self.assert_keys_cleared()
        self.assertIn("Self-assessment claims 403 for RO-on-write; firmware expects 401; observed 401", self.log)
        status: int
        for status in (302, 401, 403):
            self.assertIn(f'"expected_status": {status}', self.log)
        steps: list[str] = [line for line in self.output if line.startswith("[Step")]
        self.assertEqual(len(steps), 13)
        step: str
        for number, step in enumerate(steps, 1):
            self.assertTrue(step.startswith(f"[Step {number} out of 13]"))
            self.assertIn(step, self.log)
        self.assertEqual(sum(line.startswith("Open log file: ") for line in self.output), 1)
        self.assertEqual(self.output[-1], "Overall verdict: PASS")
        secret: str
        for secret in (CONFIG.gw_id, RO, RW, "RUUVISESSION=", "ECDH", login_body(1)["password"]):
            self.assertNotIn(secret, "\n".join(self.output))
        self.assertIn("DURATION SECONDS", self.log)
        self.assertIn("HTTP REQUEST BEGIN", self.log)
        self.assertIn("HTTP RESPONSE BEGIN", self.log)

    def test_missing_env_still_creates_error_evidence(self) -> None:
        (self.work / ".env").unlink()
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.SETUP], "ERROR")
        self.assertEqual(self.gateway.calls, [])
        self.assertIn("EXCEPTION TYPE", self.log)
        self.assertEqual(self.output[-1], "Overall verdict: ERROR")

    def test_real_basic_and_digest_setup_challenges_require_reset(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.gateway = DenialGateway()
                self.gateway.mode = mode
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                self.assertEqual(len(self.gateway.calls), 1)
                self.assertIn("GatewayAuthenticationModeError", self.log)
                self.assertIn(FACTORY_RESET_MESSAGE, self.output)

    def test_failed_default_login_requires_reset_and_stops(self) -> None:
        self.fault("none", "POST", "/auth", 401)
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
        self.assertEqual(len(self.gateway.calls), 2)

    def test_corrupt_default_password_response_is_rejected_by_transport(self) -> None:
        def corrupt(request: requests.PreparedRequest) -> None:
            request.body = json.dumps({"login": "Admin", "password": "bad"})
        self.gateway.corrupt_login = corrupt
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
        self.assertEqual(self.gateway.config_reads, 0)

    def test_missing_login_cookie_is_rejected_by_transport(self) -> None:
        def corrupt(request: requests.PreparedRequest) -> None:
            request.headers.pop(HttpHeader.COOKIE, None)
        self.gateway.corrupt_login = corrupt
        self.assertEqual(self.execute().exit_code, 2)
        self.assertEqual(self.gateway.config_reads, 0)

    def test_baseline_fields_reject_unequal_missing_and_wrong_types(self) -> None:
        field: str
        variant: str
        for field in AUTHENTICATION_DEFAULT_FIELDS:
            for variant in ("unequal", "missing", "wrong_type"):
                with self.subTest(field=field, variant=variant):
                    self.gateway = DenialGateway()
                    def edit(
                        payload: dict[str, Any], _number: int,
                        selected_variant: str = variant, selected_field: str = field,
                    ) -> None:
                        if selected_variant == "missing":
                            payload.pop(selected_field)
                        elif selected_variant == "unequal":
                            payload[selected_field] = (
                                not payload[selected_field] if type(payload[selected_field]) is bool else "different"
                            )
                        else:
                            payload[selected_field] = 0 if type(payload[selected_field]) is bool else False
                    self.gateway.config_edit = edit
                    result: RunResult = self.execute()
                    self.assertEqual(result.exit_code, 2)
                    self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                    self.assertEqual(len(self.gateway.calls), 3)

    def test_bad_or_missing_identity_precedes_nondefault_reset_advice(self) -> None:
        mac: Any
        for mac in (None, 7, "invalid", "00:00:00:00:00:00"):
            with self.subTest(mac=mac):
                self.gateway = DenialGateway()
                def edit(payload: dict[str, Any], _number: int, selected_mac: Any = mac) -> None:
                    payload[GatewayCfgDesc.LAN_AUTH_USER] = "changed"
                    if selected_mac is None:
                        payload.pop(GatewayCfgDesc.GW_MAC)
                    else:
                        payload[GatewayCfgDesc.GW_MAC] = selected_mac
                self.gateway.config_edit = edit
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertIsNone(result.recovery_message)
                self.assertEqual(len(self.gateway.calls), 3)

    def test_absent_mac_with_default_fields_is_allowed(self) -> None:
        def edit(payload: dict[str, Any], _number: int) -> None:
            payload.pop(GatewayCfgDesc.GW_MAC)
        self.gateway.config_edit = edit
        self.assertEqual(self.execute().exit_code, 0)

    def test_unstable_baseline_stops_before_probes(self) -> None:
        def edit(payload: dict[str, Any], number: int) -> None:
            if number == 2:
                payload["mqtt"]["port"] = 1884
        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.SETUP], "ERROR")
        self.assertEqual(result.outcomes[runner.FINAL], "PASS")
        self.assertEqual(self.gateway.config_bodies, [])
        self.assertEqual(self.gateway.config_reads, 3)

    def test_each_late_read_failure_aborts_before_configuration_posts(self) -> None:
        scheme: str
        for scheme in ("Basic", "Digest"):
            with self.subTest(scheme=scheme):
                self.gateway = DenialGateway()
                self.fault(scheme, "GET", "/history", 200)
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 1)
                self.assertEqual(result.outcomes[runner.INVALID], "FAIL")
                self.assertEqual(result.outcomes[runner.FINAL], "PASS")
                writes: list[RecordedRequest] = [
                    call for call in self.gateway.calls if call.path == "/ruuvi.json" and call.method == "POST"
                ]
                self.assertEqual(writes, [])

    def test_info_denials_require_scheme_specific_status_before_any_write(self) -> None:
        scheme: str
        status: int
        for scheme in ("none", "Basic", "Digest", "Bearer"):
            for status in (200, 302, 401, 403, 404):
                if status == (401 if scheme == "Bearer" else 302):
                    continue
                with self.subTest(scheme=scheme, status=status):
                    self.gateway = DenialGateway()
                    self.fault(scheme, "GET", "/info.json", status)
                    result: RunResult = self.execute()
                    owner: str = runner.UNAUTHENTICATED if scheme == "none" else runner.INVALID
                    self.assertEqual((result.exit_code, result.outcomes[owner]), (1, "FAIL"))
                    self.assertEqual(result.outcomes[runner.FINAL], "PASS")
                    self.assertEqual(self.gateway.config_bodies, [])
                    self.assertFalse(any(call.method == "POST" and call.path != "/auth"
                                         for call in self.gateway.calls))
                    self.assertIn(f'"observed_status": {status}', self.log)

    def test_info_lan_dispatch_404_requires_authorized_cookie_or_valid_bearer(self) -> None:
        self.assertEqual(self.execute().exit_code, 0)
        session: BoundarySession = self.gateway.session()
        session.trust_env = False
        session.cookies.set("RUUVISESSION", self.gateway.sessions[0].cookie)
        request: requests.PreparedRequest = session.prepare_request(
            requests.Request("GET", f"{CONFIG.base_url}/info.json"),
        )
        self.assertEqual(session.send(request, timeout=(5, 15), allow_redirects=False).status_code, 404)
        session.cookies.clear()
        self.gateway.ro_key = RO
        request = session.prepare_request(requests.Request(
            "GET", f"{CONFIG.base_url}/info.json", headers={"Authorization": f"Bearer {RO}"},
        ))
        self.assertEqual(session.send(request, timeout=(5, 15), allow_redirects=False).status_code, 404)

    def test_each_wrong_negative_status_fails_its_owner_and_stops(self) -> None:
        cases: tuple[tuple[str, str, str, int, str], ...] = (
            ("none", "GET", "/ruuvi.json", 3, runner.UNAUTHENTICATED),
            ("none", "GET", "/history", 1, runner.UNAUTHENTICATED),
            ("none", "GET", "/status.json", 1, runner.UNAUTHENTICATED),
            ("none", "GET", "/info.json", 1, runner.UNAUTHENTICATED),
            ("none", "GET", "/auth", 2, runner.UNAUTHENTICATED),
            ("none", "DELETE", "/auth", 1, runner.UNAUTHENTICATED),
            ("none", "POST", "/auth", 2, runner.INVALID),
            ("none", "POST", "/auth", 3, runner.INVALID),
            ("none", "GET", "/auth", 5, runner.INVALID),
            ("none", "GET", "/auth", 6, runner.INVALID),
            ("none", "GET", "/auth", 3, runner.INVALID),
            ("Basic", "GET", "/ruuvi.json", 1, runner.INVALID),
            ("Basic", "GET", "/status.json", 1, runner.INVALID),
            ("Basic", "GET", "/history", 1, runner.INVALID),
            ("Basic", "GET", "/info.json", 1, runner.INVALID),
            ("Digest", "GET", "/ruuvi.json", 1, runner.INVALID),
            ("Digest", "GET", "/status.json", 1, runner.INVALID),
            ("Digest", "GET", "/history", 1, runner.INVALID),
            ("Digest", "GET", "/info.json", 1, runner.INVALID),
            ("Bearer", "GET", "/info.json", 1, runner.INVALID),
            ("none", "POST", "/ruuvi.json", 1, runner.UNAUTHENTICATED),
            ("Basic", "POST", "/ruuvi.json", 1, runner.INVALID),
            ("Digest", "POST", "/ruuvi.json", 1, runner.INVALID),
            ("Bearer", "POST", "/ruuvi.json", 1, runner.INSUFFICIENT),
            ("Bearer", "POST", "/connect_wps", 1, runner.LAN_FORBIDDEN),
        )
        method: str
        path: str
        occurrence: int
        owner: str
        scheme: str
        status: int
        for scheme, method, path, occurrence, owner in cases:
            for status in (204, 404):
                with self.subTest(scheme=scheme, method=method, path=path, status=status):
                    self.gateway = DenialGateway()
                    self.fault(scheme, method, path, status, occurrence)
                    result: RunResult = self.execute()
                    self.assertEqual((result.exit_code, result.outcomes[owner]), (1, "FAIL"), self.log[-1500:])
                    self.assertEqual(result.outcomes[runner.FINAL], "PASS")
                    key: FakeRequestKey = FakeRequestKey(scheme, method, path)
                    matching: list[int] = [index for index, call in enumerate(self.gateway.calls) if call.key == key]
                    after: list[RecordedRequest] = self.gateway.calls[matching[occurrence - 1] + 1:]
                    self.assertFalse(any(call.scheme in ("Basic", "Digest") for call in after))
                    self.assertFalse(any(call.method == "POST" and call.body == {} for call in after))
                    if owner in (runner.INSUFFICIENT, runner.LAN_FORBIDDEN):
                        self.assert_keys_cleared()
                    self.assertIn(f'"mechanism": "{owner}", "result": "FAIL"', self.log)

    def test_admin_wrong_password_bypass_fails_before_configuration_writes(self) -> None:
        self.gateway.accept_any_admin_password = True
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.INVALID]), (1, "FAIL"))
        logins: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.method == "POST" and call.path == "/auth"
        ]
        self.assertEqual([call.body for call in logins], [
            login_body(1), login_body(8, invalid=True), login_body(9, wrong_password=True),
            login_body(10),
        ])
        self.assertEqual(self.gateway.config_bodies, [])
        self.assertIn('"mechanism": "invalid-credential denial", "result": "FAIL"', self.log)

    def test_failed_login_rotated_cookie_checks_both_distinct_candidates(self) -> None:
        username: str
        for username in ("invalid", "Admin"):
            with self.subTest(username=username):
                self.gateway = DenialGateway()
                self.gateway.rotate_failed_username = username
                self.assertEqual(self.execute().exit_code, 0)
                number: int = 8 if username == "invalid" else 9
                calls: list[RecordedRequest] = [
                    call for call in self.gateway.calls if call.session_number == number
                ]
                self.assertEqual([(call.method, call.path, call.cookie) for call in calls], [
                    ("GET", "/auth", None),
                    ("POST", "/auth", f"RUUVISESSION=session-{number}"),
                    ("GET", "/auth", f"RUUVISESSION=session-{number}"),
                    ("GET", "/auth", f"RUUVISESSION=replacement-{number}"),
                ])

    def test_failed_login_rotated_cookie_cannot_hide_original_authorization(self) -> None:
        username: str
        for username in ("invalid", "Admin"):
            with self.subTest(username=username):
                self.gateway = DenialGateway()
                self.gateway.rotate_failed_username = username
                self.gateway.authorize_failed_original = True
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[runner.INVALID]), (1, "FAIL"))
                self.assertEqual(self.gateway.config_bodies, [])
                number: int = 8 if username == "invalid" else 9
                calls: list[RecordedRequest] = [
                    call for call in self.gateway.calls if call.session_number == number
                ]
                self.assertEqual([(call.method, call.cookie) for call in calls], [
                    ("GET", None), ("POST", f"RUUVISESSION=session-{number}"),
                    ("GET", f"RUUVISESSION=session-{number}"),
                ])
                self.assertEqual(result.outcomes[runner.FINAL], "PASS")

    def test_invalid_login_must_not_authorize_followup_session(self) -> None:
        self.fault("none", "GET", "/auth", 200, 4)
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.INVALID]), (1, "FAIL"))
        self.assertEqual(self.gateway.config_bodies, [])

    def test_auth_json_requires_default_mode_type_and_value(self) -> None:
        session_number: int
        variant: str
        for session_number in (1, 6):
            for variant in ("equal", "unequal", "missing", "wrong_type"):
                with self.subTest(session=session_number, variant=variant):
                    self.gateway = DenialGateway()

                    def edit(
                        payload: dict[str, Any], number: int,
                        selected_session: int = session_number, selected_variant: str = variant,
                    ) -> None:
                        if number != selected_session:
                            return
                        if selected_variant == "missing":
                            payload.pop(GatewayCfgDesc.LAN_AUTH_TYPE)
                        elif selected_variant != "equal":
                            payload[GatewayCfgDesc.LAN_AUTH_TYPE] = (
                                "lan_auth_allow" if selected_variant == "unequal" else False
                            )

                    self.gateway.auth_edit = edit
                    result: RunResult = self.execute()
                    if variant == "equal":
                        self.assertEqual(result.exit_code, 0)
                    elif session_number == 1:
                        self.assertEqual(result.exit_code, 2)
                        self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                        self.assertEqual(self.gateway.config_bodies, [])
                    else:
                        self.assertEqual(result.exit_code, 1)
                        self.assertEqual(result.outcomes[runner.UNAUTHENTICATED], "FAIL")
                        self.assertEqual(result.outcomes[runner.FINAL], "PASS")

    def test_malformed_negative_auth_challenge_is_error_and_aborts(self) -> None:
        self.gateway.faults = [Fault(
            FakeRequestKey("none", "GET", "/auth"), 2,
            FakeResponse(401, {GatewayCfgDesc.LAN_AUTH_TYPE: "lan_auth_default"},
                         {HttpHeader.WWW_AUTHENTICATE: 'Basic realm="Ruuvi Gateway"'}),
        )]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.UNAUTHENTICATED], "ERROR")
        self.assertEqual(result.outcomes[runner.FINAL], "PASS")
        self.assertEqual(self.gateway.config_bodies, [])

    def test_redirect_target_and_status_previous_cookie(self) -> None:
        key: FakeRequestKey
        response: FakeResponse
        cases: tuple[tuple[FakeRequestKey, int, FakeResponse], ...] = (
            (FakeRequestKey("none", "GET", "/ruuvi.json"), 3, FakeResponse(302, headers={"Location": "/login"})),
            (FakeRequestKey("none", "GET", "/status.json"), 1,
             FakeResponse(302, cookies={"RUUVI_PREV_URL": "/status.json"})),
        )
        occurrence: int
        for key, occurrence, response in cases:
            with self.subTest(key=key):
                self.gateway = DenialGateway()
                self.gateway.faults = [Fault(key, occurrence, response)]
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[runner.UNAUTHENTICATED]), (1, "FAIL"))
                self.assertEqual(self.gateway.config_bodies, [])

    def test_auth_redirect_authority_is_bound_to_verified_gateway(self) -> None:
        location: str
        accepted: bool
        for location, accepted in (
            ("/#auth", True), ("http://gateway.local:80/#auth", True),
            ("//gateway.local/#auth", True), ("http://RuuviGatewayEEFF.local/#auth", True),
            ("http://ruuvigatewayeeff/#auth", True),
            ("http://unrelated.example/#auth", False), ("//unrelated.example/#auth", False),
            ("http://gateway.local:81/#auth", False), ("http://gateway.local:0/#auth", False),
            ("http://other@gateway.local/#auth", False), ("http://gateway.local:bad/#auth", False),
            ("http://ruuvigateway0000.local/#auth", False), ("http:/#auth", False),
        ):
            with self.subTest(location=location):
                self.gateway = DenialGateway()
                self.gateway.faults = [Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 3,
                                            FakeResponse(302, headers={"Location": location}))]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 0 if accepted else 1)
                if not accepted:
                    self.assertEqual(self.gateway.config_bodies, [])

    def test_ip_target_accepts_only_its_mac_derived_canonical_alias(self) -> None:
        config: DutConfig = DutConfig(CONFIG.gw_id, CONFIG.gw_mac, "192.0.2.7")
        (self.work / ".env").write_text(env_text(config), encoding="utf-8")
        location: str
        accepted: bool
        for location, accepted in (("http://192.0.2.7/#auth", True),
                                   ("http://RuuviGatewayEEFF.local/#auth", True),
                                   ("http://192.0.2.8/#auth", False)):
            with self.subTest(location=location):
                self.gateway = DenialGateway()
                self.gateway.faults = [Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 3,
                                            FakeResponse(302, headers={"Location": location}))]
                self.assertEqual(self.execute().exit_code, 0 if accepted else 1)

    def test_reporting_failures_do_not_skip_key_removal_or_retry(self) -> None:
        checkpoint: str
        for checkpoint in ("progress", "attempt", "exception", "say"):
            with self.subTest(checkpoint=checkpoint):
                self.check_reporting_failure(checkpoint)

    def test_cleanup_reporting_error_preserves_prior_security_failure(self) -> None:
        self.check_reporting_failure("attempt", security_failure=True)

    def check_reporting_failure(self, checkpoint: str, *, security_failure: bool = False) -> None:
        self.gateway = DenialGateway()
        original_write: Callable[[runner.EvidenceLog, str, Any], None] = cast(
            "Callable[[runner.EvidenceLog, str, Any], None]", runner.EvidenceLog.write,
        )
        original_exception: Callable[[runner.EvidenceLog, BaseException], None] = cast(
            "Callable[[runner.EvidenceLog, BaseException], None]", runner.EvidenceLog.exception,
        )
        original_say: Callable[[runner.FunctionalTest_5_5_4_2_a, str], None] = cast(
            "Callable[[runner.FunctionalTest_5_5_4_2_a, str], None]", runner.FunctionalTest_5_5_4_2_a._say,
        )
        original_progress: Callable[[runner.ProgressReporter, str], None] = cast(
            "Callable[[runner.ProgressReporter, str], None]", runner.ProgressReporter.step,
        )
        failures: list[str] = []

        def fail_once(label: str) -> None:
            if label == checkpoint and not failures:
                failures.append(label)
                raise OSError("injected reporting failure")

        def write(log: runner.EvidenceLog, label: str, data: Any = "") -> None:
            if label == "KEY RESTORATION ATTEMPT":
                fail_once("attempt")
            original_write(log, label, data)

        def report_exception(log: runner.EvidenceLog, error: BaseException) -> None:
            fail_once("exception")
            original_exception(log, error)

        def say(test: runner.FunctionalTest_5_5_4_2_a, message: str) -> None:
            if message.startswith("Restoration attempt"):
                fail_once("say")
            original_say(test, message)

        def progress(reporter: runner.ProgressReporter, message: str) -> None:
            if message.startswith("Restoring keys"):
                fail_once("progress")
            original_progress(reporter, message)

        self.fault("none", "POST", "/ruuvi.json", 401, 3)
        if security_failure:
            self.fault("Bearer", "POST", "/ruuvi.json", 200)
        with patch.object(runner.EvidenceLog, "write", write), patch.object(
            runner.EvidenceLog, "exception", report_exception,
        ), patch.object(runner.FunctionalTest_5_5_4_2_a, "_say", say), patch.object(
            runner.ProgressReporter, "step", progress,
        ):
            result: RunResult = self.execute()
        self.assertEqual(failures, [checkpoint])
        self.assertEqual((result.exit_code, result.outcomes[runner.FINAL]), (2, "ERROR"))
        self.assertEqual(len([call for call in self.gateway.calls if call.body == CLEAR_BODY]), 2)
        self.assert_keys_cleared()
        if security_failure:
            self.assertEqual(result.outcomes[runner.INSUFFICIENT], "FAIL")

    def test_evidence_creation_failure_returns_setup_error_without_dut_access(self) -> None:
        persistent: bool
        for persistent in (False, True):
            with self.subTest(persistent_output_failure=persistent):
                self.output.clear()

                def output(message: str, fail: bool = persistent) -> None:
                    if fail:
                        raise OSError("secret terminal failure")
                    self.output.append(message)

                finish: MagicMock
                with patch.object(runner.EvidenceLog, "create", side_effect=OSError("secret log path")), patch.object(
                    runner.EvidenceLog, "finish",
                ) as finish:
                    result: RunResult = runner.execute_test_5_5_4_2_a(
                        self.work, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, output,
                    )
                self.assertEqual((result.exit_code, result.verdict, result.outcomes[runner.SETUP]), (2, "ERROR", "ERROR"))
                finish.assert_not_called()
                self.assertEqual(self.gateway.calls, [])
                self.assertEqual(set(result.outcomes.values()), {"ERROR", "NOT RUN"})
                if not persistent:
                    self.assertIn("Setup ERROR — OSError; evidence log could not be created", self.output)
                    self.assertEqual(self.output[-1], "Overall verdict: ERROR")
                    self.assertNotIn("secret log path", "\n".join(self.output))

    def test_terminal_failure_returns_error_and_preserves_error_evidence(self) -> None:
        checkpoint: str
        for checkpoint in ("Open log file:", "Overall verdict:"):
            with self.subTest(checkpoint=checkpoint):
                self.gateway = DenialGateway()
                failed: bool = False
                self.output.clear()

                def output(message: str, prefix: str = checkpoint) -> None:
                    nonlocal failed
                    if message.startswith(prefix) and not failed:
                        failed = True
                        raise OSError("broken output")
                    self.output.append(message)

                result: RunResult = runner.execute_test_5_5_4_2_a(
                    self.work, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, output,
                )
                self.assertEqual((result.exit_code, result.verdict), (2, "ERROR"))
                self.assertEqual(self.output[-1], "Overall verdict: ERROR")
                logs: list[Path] = list((self.work / "logs").glob("*.log"))
                self.assertTrue(logs)
                self.assertTrue(all("OVERALL VERDICT: ERROR" in path.read_text(encoding="utf-8") for path in logs))
                if checkpoint == "Open log file:":
                    self.assertEqual(self.gateway.calls, [])
                else:
                    self.assert_keys_cleared()

    def test_initial_evidence_write_failure_returns_error_and_attempts_finalization(self) -> None:
        label: str
        persistent: bool
        for label in ("TEST CASE AND UNIT", "UTC START"):
            for persistent in (False, True):
                with self.subTest(label=label, persistent=persistent):
                    self.check_initial_evidence_write_failure(label, persistent=persistent)

    def check_initial_evidence_write_failure(self, label: str, *, persistent: bool) -> None:
        original_write: Callable[[runner.EvidenceLog, str, Any], None] = cast(
            "Callable[[runner.EvidenceLog, str, Any], None]", runner.EvidenceLog.write,
        )
        failures: list[str] = []

        def write(evidence_log: runner.EvidenceLog, heading: str, value: Any = "") -> None:
            if (heading == label and not failures) or (persistent and failures):
                failures.append(heading)
                raise OSError("full evidence disk")
            original_write(evidence_log, heading, value)

        log: runner.EvidenceLog = runner.EvidenceLog.create(self.work / "logs", "initial", fixed_now)
        finished: list[str] = []
        original_finish: Callable[[str, Callable[[], datetime]], None] = log.finish

        def finish(verdict: str, now: Callable[[], datetime]) -> None:
            finished.append(verdict)
            if persistent:
                raise OSError("full evidence disk during finalization")
            original_finish(verdict, now)

        try:
            with patch.object(runner.EvidenceLog, "create", return_value=log), patch.object(
                runner.EvidenceLog, "write", write,
            ), patch.object(log, "finish", side_effect=finish):
                result: RunResult = runner.execute_test_5_5_4_2_a(
                    self.work, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, self.output.append,
                )
        finally:
            if persistent:
                log.finish("ERROR", fixed_now)
        self.assertEqual((result.exit_code, result.verdict, result.outcomes[runner.SETUP]), (2, "ERROR", "ERROR"))
        self.assertEqual(finished, ["ERROR"])
        self.assertEqual(self.gateway.calls, [])
        self.assertTrue(any("Setup ERROR — OSError" in line for line in self.output))
        self.assertEqual(self.output[-1], "Overall verdict: ERROR")
        self.assertEqual(failures[0], label)
        if persistent:
            self.assertIn("EXCEPTION TYPE", failures)
            self.assertIn("FINAL RESULTS", failures)
        else:
            self.assertIn("OVERALL VERDICT: ERROR", log.path.read_text(encoding="utf-8"))

    def test_persistent_logging_failure_still_clears_keys_and_verifies_revocation(self) -> None:
        scenario: str
        for scenario in ("clear", "lost-clear-response", "denied-login", "unrevoked-key"):
            with self.subTest(scenario=scenario):
                self.check_persistent_logging_failure(scenario)

    def check_persistent_logging_failure(self, scenario: str) -> None:
        self.gateway = DenialGateway()
        if scenario == "lost-clear-response":
            self.gateway.faults.append(Fault(FakeRequestKey("none", "POST", "/ruuvi.json"), 3,
                                            error=requests.Timeout("clearing response lost"), apply_body=True))
        elif scenario == "denied-login":
            self.fault("none", "POST", "/auth", 401, 4)
            self.fault("none", "POST", "/auth", 401, 5)
        elif scenario == "unrevoked-key":
            self.fault("Bearer", "GET", "/history", 200, 1)

        original_write: Callable[[runner.EvidenceLog, str, Any], None] = cast(
            "Callable[[runner.EvidenceLog, str, Any], None]", runner.EvidenceLog.write,
        )
        original_line: Callable[[runner.EvidenceLog, str], None] = cast(
            "Callable[[runner.EvidenceLog, str], None]", runner.EvidenceLog.write_line,
        )
        broken: list[bool] = [False]
        failures: list[str] = []

        def check_disk(label: str) -> None:
            broken[0] = broken[0] or bool(self.gateway.ro_key or self.gateway.rw_key)
            if broken[0]:
                failures.append(label)
                raise OSError("persistent full disk after provisioning")

        def write(evidence_log: runner.EvidenceLog, label: str, value: Any = "") -> None:
            check_disk(label)
            original_write(evidence_log, label, value)

        def write_line(evidence_log: runner.EvidenceLog, message: str) -> None:
            check_disk("line")
            original_line(evidence_log, message)

        def finish(_log: runner.EvidenceLog, _verdict: str, _now: Callable[[], datetime]) -> None:
            check_disk("finish")

        log: runner.EvidenceLog = runner.EvidenceLog.create(self.work / "logs", "persistent", fixed_now)
        try:
            with patch.object(runner.EvidenceLog, "create", return_value=log), patch.object(
                runner.EvidenceLog, "write", write,
            ), patch.object(runner.EvidenceLog, "write_line", write_line), patch.object(
                runner.EvidenceLog, "finish", finish,
            ):
                result: RunResult = runner.execute_test_5_5_4_2_a(
                    self.work, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, self.output.append,
                )
        finally:
            log.finish("ERROR", fixed_now)
        self.assertTrue(broken[0])
        self.assertGreater(failures.count("HTTP REQUEST BEGIN"), 4)
        self.assertIn("ASSERTION", failures)
        self.assertIn("finish", failures)
        self.assertEqual((result.exit_code, result.verdict), (2, "ERROR"))
        self.assertIsNotNone(result.recovery_message)
        if scenario == "denied-login":
            self.assertNotIn(CLEAR_BODY, self.gateway.config_bodies)
            self.assertEqual((self.gateway.ro_key, self.gateway.rw_key), (RO, RW))
            self.assertEqual(len([call for call in self.gateway.calls
                                  if call.method == "POST" and call.path == "/auth"]), 6)
        else:
            self.assert_keys_cleared()
            self.assertEqual(len([call for call in self.gateway.calls if call.body == CLEAR_BODY]),
                             2 if scenario == "lost-clear-response" else 1)
        if scenario == "unrevoked-key":
            self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        final_reads: list[RecordedRequest] = [call for call in self.gateway.calls
                                             if call.method == "GET" and call.path == "/ruuvi.json"]
        self.assertIn(final_reads[-1].session_number,
                      [session.number for session in self.gateway.authorized_sessions])

    def test_timeout_connection_and_malformed_baseline_are_errors(self) -> None:
        fault: Fault
        cases: tuple[Fault, ...] = (
            Fault(FakeRequestKey("none", "GET", "/auth"), error=requests.Timeout("offline timeout")),
            Fault(FakeRequestKey("none", "GET", "/auth"), error=requests.ConnectionError("offline connection")),
            Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), response=FakeResponse(200, malformed_json=True)),
            Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), response=FakeResponse(200, [])),
        )
        for fault in cases:
            with self.subTest(fault=fault):
                self.gateway = DenialGateway()
                self.gateway.faults = [fault]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.SETUP], "ERROR")
                self.assertEqual(self.gateway.config_bodies, [])
                self.assertIn("EXCEPTION TYPE", self.log)

    def test_early_configuration_probe_abort_still_restores_without_candidate_keys(self) -> None:
        scheme: str
        status: int | None
        for scheme in ("none", "Basic", "Digest"):
            for status in (200, 204, None):
                with self.subTest(scheme=scheme, status=status):
                    self.gateway = DenialGateway()
                    self.gateway.faults = [Fault(
                        FakeRequestKey(scheme, "POST", "/ruuvi.json"), 1,
                        FakeResponse(status) if status is not None else None,
                        requests.Timeout("lost early write response") if status is None else None,
                    )]
                    original: Callable[[FakeSession, RecordedRequest], FakeResponse | None] = (
                        self.gateway._response_override
                    )

                    def side_effect(
                        session: FakeSession, call: RecordedRequest, selected_scheme: str = scheme,
                        original_override: Callable[[FakeSession, RecordedRequest], FakeResponse | None] = original,
                    ) -> FakeResponse | None:
                        if call.key == FakeRequestKey(selected_scheme, "POST", "/ruuvi.json") and call.body == {}:
                            # Model a bypass that activates unrelated keys before replying or timing out.
                            self.gateway.ro_key = "unexpected-ro-key"
                            self.gateway.rw_key = "unexpected-rw-key"
                        return original_override(session, call)

                    with patch.object(self.gateway, "_response_override", side_effect=side_effect):
                        result: RunResult = self.execute()
                    owner: str = runner.UNAUTHENTICATED if scheme == "none" else runner.INVALID
                    self.assertEqual((result.exit_code, result.outcomes[owner]),
                                     (2, "ERROR") if status is None else (1, "FAIL"))
                    self.assertEqual(result.outcomes[runner.FINAL], "PASS", self.log[-1500:])
                    self.assertEqual(result.outcomes[runner.PREPARED], "NOT RUN")
                    self.assertEqual((self.gateway.ro_key, self.gateway.rw_key), ("", ""))
                    self.assertEqual(self.gateway.config_bodies, [CLEAR_BODY])
                    writes: list[RecordedRequest] = [
                        call for call in self.gateway.calls if call.method == "POST" and call.path == "/ruuvi.json"
                    ]
                    self.assertEqual(writes[-1].body, CLEAR_BODY)
                    self.assertEqual(writes[-2].key, FakeRequestKey(scheme, "POST", "/ruuvi.json"))
                    self.assertFalse(any(call.scheme == "Bearer" and not call.bearer_token
                                         for call in self.gateway.calls))
                    self.assertIsNone(result.recovery_message)

    def test_early_bypass_with_unrepairable_side_effect_requires_operator_recovery(self) -> None:
        self.fault("none", "POST", "/ruuvi.json", 200)

        def edit(payload: dict[str, Any], _number: int) -> None:
            if any(call.method == "POST" and call.path == "/ruuvi.json" and call.body == {}
                   for call in self.gateway.calls):
                payload["mqtt"]["port"] = 1884

        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.UNAUTHENTICATED]), (2, "FAIL"))
        self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        self.assertEqual(self.gateway.config_bodies, [CLEAR_BODY])
        self.assertIsNotNone(result.recovery_message)
        self.assertIn("Restoration could not be verified", result.recovery_message or "")
        self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
        self.assertIn('"mqtt"', self.log)
        self.assertEqual(self.output[-1], "Overall verdict: ERROR")

    def test_partial_provisioning_timeout_or_denial_always_restores(self) -> None:
        failure: Exception | None
        for failure in (requests.Timeout("lost provisioning response"), RuntimeError("unexpected"), None):
            with self.subTest(failure=failure):
                self.gateway = DenialGateway()
                self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/ruuvi.json"), 2,
                                            FakeResponse(503), failure, apply_body=True)]
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[runner.PREPARED]), (2, "ERROR"))
                self.assertEqual(result.outcomes[runner.INSUFFICIENT], "NOT RUN")
                self.assert_keys_cleared()
                self.assertIn(RO, self.log)
                self.assertIn(RW, self.log)

    def test_prepared_configuration_rejects_side_effects_missing_and_wrong_types(self) -> None:
        variant: str
        for variant in ("unrelated", "missing", "integer", "false", "mode"):
            with self.subTest(variant=variant):
                self.gateway = DenialGateway()
                def edit(payload: dict[str, Any], number: int, selected_variant: str = variant) -> None:
                    if number != 6:
                        return
                    if selected_variant == "unrelated":
                        payload["mqtt"]["port"] = 1884
                    elif selected_variant == "missing":
                        payload.pop(GatewayCfgDesc.LAN_AUTH_API_KEY_USE)
                    elif selected_variant == "mode":
                        payload[GatewayCfgDesc.LAN_AUTH_TYPE] = GatewayCfgLanAuthType.ALLOW
                    else:
                        payload[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = 1 if selected_variant == "integer" else False
                self.gateway.config_edit = edit
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[runner.PREPARED]), (2, "ERROR"))
                self.assert_keys_cleared()

    def test_positive_bearer_controls_are_setup_and_precede_negative_writes(self) -> None:
        occurrence: int
        for occurrence in (1, 2):
            with self.subTest(occurrence=occurrence):
                self.gateway = DenialGateway()
                self.fault("Bearer", "GET", "/history", 401, occurrence)
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[runner.PREPARED]), (2, "ERROR"))
                self.assertFalse(any(call.scheme == "Bearer" and call.method == "POST" for call in self.gateway.calls))
                self.assert_keys_cleared()

    def test_prepared_read_transport_error_restores(self) -> None:
        self.gateway.faults = [Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 7,
                                    error=requests.Timeout("readback lost"))]
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.PREPARED]), (2, "ERROR"))
        self.assert_keys_cleared()

    def test_ro_on_write_403_is_a_failure(self) -> None:
        self.fault("Bearer", "POST", "/ruuvi.json", 403)
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.INSUFFICIENT]), (1, "FAIL"))
        self.assertEqual(result.outcomes[runner.LAN_FORBIDDEN], "NOT RUN")
        self.assert_keys_cleared()

    def test_wps_401_is_a_failure_after_verified_rw_control(self) -> None:
        self.fault("Bearer", "POST", "/connect_wps", 401)
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes[runner.LAN_FORBIDDEN]), (1, "FAIL"))
        self.assertEqual(result.outcomes[runner.INSUFFICIENT], "PASS")
        self.assert_keys_cleared()

    def test_nonmutation_failure_is_attributed_to_responsible_probe(self) -> None:
        number: int
        owner: str
        for number, owner in ((3, runner.UNAUTHENTICATED), (4, runner.INVALID),
                              (5, runner.INVALID), (7, runner.INSUFFICIENT), (8, runner.LAN_FORBIDDEN)):
            with self.subTest(number=number):
                self.gateway = DenialGateway()
                def edit(payload: dict[str, Any], count: int, selected_number: int = number) -> None:
                    if count == selected_number:
                        payload["mqtt"]["enabled"] = 0
                self.gateway.config_edit = edit
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes[owner]), (1, "FAIL"), self.log[-1500:])
                self.assertIn('CHANGED CONFIGURATION FIELDS: ["mqtt"]', self.log)
                if owner == runner.LAN_FORBIDDEN:
                    self.assertEqual(result.outcomes[runner.INSUFFICIENT], "PASS")

    def test_restore_reauthenticates_and_retries_denied_or_lost_response(self) -> None:
        error: Exception | None
        for error in (None, requests.Timeout("restoration response lost")):
            with self.subTest(error=error):
                self.gateway = DenialGateway()
                self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/ruuvi.json"), 3,
                                            FakeResponse(401), error, apply_body=error is not None)]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 0, self.log[-1500:])
                restores: list[RecordedRequest] = [call for call in self.gateway.calls if call.body == CLEAR_BODY]
                self.assertEqual(len(restores), 2)
                self.assertNotEqual(restores[0].session_number, restores[1].session_number)
                self.assert_keys_cleared()
                self.assertIn("KEY RESTORATION ATTEMPT: 2", self.log)

    def test_failed_restoration_keeps_prior_security_failure(self) -> None:
        self.fault("Bearer", "POST", "/ruuvi.json", 200)
        self.fault("none", "POST", "/ruuvi.json", 401, 3)
        self.fault("none", "POST", "/ruuvi.json", 401, 4)
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.INSUFFICIENT], "FAIL")
        self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        self.assertIn("Restoration could not be verified", result.recovery_message or "")
        self.assertIn(FACTORY_RESET_MESSAGE, self.log)
        self.assertIn(result.recovery_message, self.output)
        self.assertEqual(len([call for call in self.gateway.calls if call.body == CLEAR_BODY]), 2)

    def test_security_failure_survives_later_final_transport_error(self) -> None:
        self.fault("Bearer", "POST", "/ruuvi.json", 200)
        self.gateway.faults.append(Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 8,
                                        error=requests.Timeout("final read unavailable")))
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.INSUFFICIENT], "FAIL")
        self.assertEqual(result.outcomes[runner.FINAL], "ERROR")
        self.assert_keys_cleared()
        self.assertIn('"mechanism": "AuthMech-M2M-API-Bearer-RO", "result": "FAIL"', self.log)

    def test_restoration_login_failure_retries_with_fresh_credentials(self) -> None:
        self.fault("none", "POST", "/auth", 401, 4)
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assertIn("KEY RESTORATION ATTEMPT: 2", self.log)
        self.assert_keys_cleared()

    def test_final_mismatch_and_unrevoked_keys_prevent_pass(self) -> None:
        variant: str
        for variant in ("mismatch", "ro", "rw"):
            with self.subTest(variant=variant):
                self.gateway = DenialGateway()
                if variant == "mismatch":
                    def edit(payload: dict[str, Any], number: int) -> None:
                        if number == 9:
                            payload["mqtt"]["enabled"] = 0
                    self.gateway.config_edit = edit
                else:
                    self.fault("Bearer", "GET", "/history", 200, 3 if variant == "ro" else 4)
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
                self.assertIsNotNone(result.recovery_message)

    def test_final_transport_failure_preserves_completed_security_checks(self) -> None:
        self.gateway.faults = [Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 10,
                                    error=requests.ConnectionError("final read unavailable"))]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.FINAL], "ERROR")
        self.assertEqual(result.outcomes[runner.INSUFFICIENT], "PASS")
        self.assertEqual(result.outcomes[runner.LAN_FORBIDDEN], "PASS")
        self.assert_keys_cleared()

    def test_main_returns_execution_exit_code(self) -> None:
        code: int
        for code in (0, 1, 2):
            with self.subTest(code=code), patch.object(
                runner, "execute_test_5_5_4_2_a", return_value=RunResult(code, "test", {}, set()),
            ):
                self.assertEqual(runner.main(), code)


if __name__ == "__main__":
    unittest.main()
