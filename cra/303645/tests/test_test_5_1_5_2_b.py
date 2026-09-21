"""Implementation tests only; these are not ETSI functional-test evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import sys
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterator
from unittest import mock
from urllib.parse import urlsplit

import requests
from Crypto.PublicKey import ECC

from lib.models import RunResult

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_5_1_5_2_b as target
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
)
from lib.http_api import (
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)

FIXED_NOW: datetime = datetime(2026, 8, 31, 9, 0, 0, tzinfo=timezone.utc)
CONFIG: target.DutConfig = target.DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)


def env_text(config: target.DutConfig = CONFIG) -> str:
    return (
        f"{GatewayCfgDesc.GW_ID}={config.gw_id}\n"
        f"{GatewayCfgDesc.GW_MAC}={config.gw_mac}\n"
        f"{GatewayCfgDesc.GW_HOSTNAME}={config.gw_hostname}\n"
    )


class FakeResponse(requests.Response):
    def __init__(
        self,
        status: int,
        payload: Any = None,
        headers: dict[str, str] | None = None,
        cookies: dict[str, str] | None = None,
        malformed_json: bool = False,
    ) -> None:
        super().__init__()
        self.status_code: int = status
        self.headers.update(headers or {})
        self.cookies.update(cookies or {})
        self._payload: Any = payload
        self._malformed_json: bool = malformed_json
        text: str = "{broken" if malformed_json else json.dumps(payload) if payload is not None else ""
        self._content: bytes = text.encode("utf-8")
        self.encoding: str = "utf-8"

    def json(self, **kwargs: Any) -> Any:
        if self._malformed_json:
            raise ValueError("malformed")
        return self._payload


@dataclass(frozen=True)
class RecordedRequest:
    method: str
    path: str
    bearer_token: str | None
    body: Any
    session_number: int
    authorization: str | None
    cookie: str | None


class FakeGateway:
    def __init__(self) -> None:
        self.mode: str = GatewayCfgLanAuthType.DEFAULT
        self.custom_username: str = ""
        self.custom_ha1: str = ""
        self.ro_key: str = ""
        self.rw_key: str = ""
        self.calls: list[RecordedRequest] = []
        self.config_bodies: list[dict[str, Any]] = []
        self.wrong_login_success: bool = False
        self.bearer_guess_status: int | None = None
        self.malformed_challenge: bool = False
        self.digest_authenticated_timeouts: int = 0
        self.timeout_next: bool = False
        self.prepared_config_timeouts: int = 0
        self.guess_exception: BaseException | None = None
        self.guess_exceptions: list[BaseException] = []
        self.primary_restore_status: int = HttpStatus.C_200_OK
        self.primary_restore_applies: bool = False
        self.custom_fallback_enabled: bool = True
        self.restored_config_mismatch: bool = False
        self.temporary_bearers_survive: bool = False
        self.restoration_attempts: list[str] = []
        self.authorized_sessions: list[FakeSession] = []
        self.server_key: ECC.EccKey = ECC.generate(curve="secp256r1")
        public: ECC.EccKey = self.server_key.public_key()
        raw: bytes = (
            b"\x04"
            + int(public.pointQ.x).to_bytes(32, byteorder="big")
            + int(public.pointQ.y).to_bytes(32, byteorder="big")
        )
        self.server_public_b64: str = base64.b64encode(raw).decode("ascii")

    @staticmethod
    def _bearer(headers: dict[str, str]) -> str | None:
        authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
        bearer_prefix: str = f"{HttpAuthScheme.BEARER} "
        return authorization[len(bearer_prefix) :] if authorization.startswith(bearer_prefix) else None

    def _is_real_bearer(self, token: str | None, path: str) -> bool:
        if not token:
            return False
        if token == self.rw_key:
            return True
        return token == self.ro_key and path != GatewayApi.AP

    def _interactive_challenge_response(
        self,
        session: FakeSession,
        include_ecdh_public_key: bool,
    ) -> FakeResponse:
        session.challenge_number += 1
        session.challenge = f"challenge-{session.challenge_number}"
        session.cookie = f"session-{session.number}-{session.challenge_number}"
        auth_header: str = (
            f'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="{session.challenge}" '
            f'session_cookie="RUUVISESSION" session_id="{session.cookie}"'
        )
        if self.malformed_challenge:
            auth_header = 'x-ruuvi-interactive realm="Ruuvi Gateway"'
        response_headers: dict[str, str] = {HttpHeader.WWW_AUTHENTICATE: auth_header}
        if include_ecdh_public_key:
            response_headers[HttpHeader.RUUVI_ECDH_PUBLIC_KEY] = self.server_public_b64
        return FakeResponse(
            HttpStatus.C_401_UNAUTHORIZED,
            {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
            headers=response_headers,
            cookies={"RUUVISESSION": session.cookie},
        )

    def response_for(
        self,
        session: FakeSession,
        method: str,
        path: str,
        headers: dict[str, str],
        body: Any,
    ) -> FakeResponse:
        token: str | None = self._bearer(headers)
        self.calls.append(
            RecordedRequest(
                method, path, token, body, session.number,
                headers.get(HttpHeader.AUTHORIZATION), headers.get(HttpHeader.COOKIE),
            )
        )
        if self.timeout_next:
            self.timeout_next = False
            raise requests.Timeout("timeout")

        if method == HttpMethod.GET and path == GatewayApi.AUTH:
            if self.mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
                scheme: str = (
                    HttpAuthScheme.BASIC if self.mode == GatewayCfgLanAuthType.BASIC else HttpAuthScheme.DIGEST
                )
                return FakeResponse(
                    401,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
                    headers={HttpHeader.WWW_AUTHENTICATE: f'{scheme} realm="Ruuvi Gateway"'},
                )
            if session.authorized:
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
                )
            return self._interactive_challenge_response(session, True)

        if method == HttpMethod.POST and path == GatewayApi.AUTH:
            login: str = body.get("login", "") if isinstance(body, dict) else ""
            default_ha1: str = hashlib.md5(f"{target.ADMIN_USERNAME}:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
            configured_ha1: str = default_ha1 if self.mode == GatewayCfgLanAuthType.DEFAULT else self.custom_ha1
            expected_response: str = hashlib.sha256(f"{session.challenge}:{configured_ha1}".encode()).hexdigest()
            valid_username: bool = (self.mode == GatewayCfgLanAuthType.DEFAULT and login == target.ADMIN_USERNAME) or (
                self.mode == GatewayCfgLanAuthType.RUUVI
                and login == self.custom_username
                and self.custom_fallback_enabled
            )
            valid_cookie: bool = headers.get(HttpHeader.COOKIE) == f"RUUVISESSION={session.cookie}"
            valid: bool = (
                bool(session.challenge)
                and valid_username
                and valid_cookie
                and body
                == {
                    "login": login,
                    "password": expected_response,
                }
            )
            if self.wrong_login_success and not valid:
                valid = True
                self.wrong_login_success = False
            session.authorized = valid
            if valid:
                session.challenge = ""
                self.authorized_sessions.append(session)
                if len(self.authorized_sessions) > 4:
                    self.authorized_sessions.pop(0).authorized = False
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {"authenticated": True},
                )
            return self._interactive_challenge_response(session, False)

        if method == HttpMethod.POST and path == GatewayApi.CONFIG:
            authorized: bool = bool(token) and token == self.rw_key if token is not None else session.authorized
            if not authorized:
                if token and token not in {self.ro_key, self.rw_key}:
                    if self.guess_exceptions:
                        raise self.guess_exceptions.pop(0)
                    if self.guess_exception is not None:
                        error: BaseException = self.guess_exception
                        self.guess_exception = None
                        raise error
                    if self.bearer_guess_status is not None:
                        status: int = self.bearer_guess_status
                        self.bearer_guess_status = None
                        return FakeResponse(status, {})
                return FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED,
                    {"error": "unauthorized"},
                )
            self.config_bodies.append(body)
            restoring: bool = body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT
            if restoring:
                method_name: str = (
                    "bearer" if token else ("custom" if self.mode == GatewayCfgLanAuthType.RUUVI else "default")
                )
                self.restoration_attempts.append(method_name)
                if token and self.primary_restore_status != HttpStatus.C_200_OK:
                    if self.primary_restore_applies:
                        self._apply_config(body)
                    return FakeResponse(self.primary_restore_status, {})
            self._apply_config(body)
            return FakeResponse(HttpStatus.C_200_OK, {})

        if method == HttpMethod.GET and path == GatewayApi.CONFIG and (
            self._is_real_bearer(token, path) if token is not None else session.authorized
        ):
            if self.mode == GatewayCfgLanAuthType.RUUVI and self.prepared_config_timeouts > 0:
                self.prepared_config_timeouts -= 1
                raise requests.Timeout("prepared-state timeout")
            payload: dict[str, str | bool] = {
                GatewayCfgDesc.LAN_AUTH_TYPE: self.mode,
                GatewayCfgDesc.LAN_AUTH_USER: (
                    self.custom_username if self.mode != GatewayCfgLanAuthType.DEFAULT else target.ADMIN_USERNAME
                ),
                GatewayCfgDesc.LAN_AUTH_API_KEY_USE: bool(self.ro_key),
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: bool(self.rw_key),
                GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
                GatewayCfgDesc.FW_VER: "test",
            }
            if self.restored_config_mismatch and self.mode == GatewayCfgLanAuthType.DEFAULT and self.config_bodies:
                payload["unexpected"] = True
            return FakeResponse(HttpStatus.C_200_OK, payload)

        if method == HttpMethod.GET and path == GatewayApi.STATUS and session.authorized:
            return FakeResponse(HttpStatus.C_200_OK, {"status": "ok"})

        if (
            method == HttpMethod.GET
            and path
            in {
                GatewayApi.HISTORY,
                GatewayApi.AP,
                GatewayApi.STATUS,
                GatewayApi.CONFIG,
            }
            and token
        ):
            if self.temporary_bearers_survive and self.mode == GatewayCfgLanAuthType.DEFAULT:
                return FakeResponse(HttpStatus.C_200_OK, {})
            if self._is_real_bearer(token, path):
                return FakeResponse(HttpStatus.C_200_OK, {})
            if token in {self.ro_key, self.rw_key}:
                return FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED,
                    {"error": "unauthorized"},
                )
            if self.guess_exceptions:
                raise self.guess_exceptions.pop(0)
            if self.guess_exception is not None:
                error = self.guess_exception
                self.guess_exception = None
                raise error
            if self.bearer_guess_status is not None:
                status = self.bearer_guess_status
                self.bearer_guess_status = None
                return FakeResponse(status, {})
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})

        if method == HttpMethod.GET and path == GatewayApi.STATUS:
            authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
            if self.mode == GatewayCfgLanAuthType.ALLOW:
                return FakeResponse(HttpStatus.C_200_OK, {"status": "ok"})
            if self.mode == GatewayCfgLanAuthType.DENY:
                return FakeResponse(HttpStatus.C_403_FORBIDDEN, {"error": "forbidden"})
            if self.mode == GatewayCfgLanAuthType.BASIC:
                valid = authorization == f"{HttpAuthScheme.BASIC} {self.custom_ha1}"
                return FakeResponse(
                    HttpStatus.C_200_OK if valid else HttpStatus.C_401_UNAUTHORIZED,
                    {"authenticated": valid},
                )
            if self.mode == GatewayCfgLanAuthType.DIGEST:
                if not authorization.startswith(f"{HttpAuthScheme.DIGEST} "):
                    return FakeResponse(
                        HttpStatus.C_401_UNAUTHORIZED,
                        {"authenticated": False},
                        headers={
                            HttpHeader.WWW_AUTHENTICATE: (
                                'Digest realm="Ruuvi Gateway" qop="auth" nonce="nonce" opaque="opaque"'
                            )
                        },
                    )
                if self.digest_authenticated_timeouts > 0:
                    self.digest_authenticated_timeouts -= 1
                    raise requests.Timeout("Digest authenticated request timeout")
                parameters: dict[str, str] = requests.utils.parse_dict_header(
                    authorization[len(f"{HttpAuthScheme.DIGEST} ") :]
                )
                ha2: str = hashlib.md5(f"{HttpMethod.GET}:{parameters['uri']}".encode()).hexdigest()
                expected: str = hashlib.md5(
                    f"{self.custom_ha1}:{parameters['nonce']}:{parameters['nc']}:"
                    f"{parameters['cnonce']}:{parameters['qop']}:{ha2}".encode()
                ).hexdigest()
                valid = parameters["username"] == self.custom_username and parameters["response"] == expected
                return FakeResponse(
                    HttpStatus.C_200_OK if valid else HttpStatus.C_401_UNAUTHORIZED,
                    {"authenticated": valid},
                )
        return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})

    def _apply_config(self, body: dict[str, str]) -> None:
        if not body:
            return
        old_auth: tuple[str, str, str] = (self.mode, self.custom_username, self.custom_ha1)
        self.mode = body[GatewayCfgDesc.LAN_AUTH_TYPE]
        if self.mode in {
            GatewayCfgLanAuthType.RUUVI,
            GatewayCfgLanAuthType.BASIC,
            GatewayCfgLanAuthType.DIGEST,
        }:
            self.custom_username = body[GatewayCfgDesc.LAN_AUTH_USER]
            self.custom_ha1 = body[GatewayCfgDesc.LAN_AUTH_PASS]
            self.ro_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY]
            self.rw_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
        elif self.mode == GatewayCfgLanAuthType.DEFAULT:
            if not self.temporary_bearers_survive:
                self.ro_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY]
                self.rw_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
        else:
            self.ro_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY]
            self.rw_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
        if old_auth != (self.mode, self.custom_username, self.custom_ha1):
            session: FakeSession
            for session in self.authorized_sessions:
                session.authorized = False
            self.authorized_sessions.clear()


class FakeSession(requests.Session):
    next_number: int = 1

    def __init__(self, gateway: FakeGateway) -> None:
        super().__init__()
        self.gateway: FakeGateway = gateway
        self.authorized: bool = False
        self.number: int = FakeSession.next_number
        self.challenge_number: int = 0
        self.challenge: str = ""
        self.cookie: str = ""
        FakeSession.next_number += 1

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        return super().prepare_request(request)

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if kwargs.get("allow_redirects") is not False:
            raise AssertionError("functional probes must disable redirects")
        body: Any = json.loads(request.body) if request.body else None
        return self.gateway.response_for(
            self,
            request.method,
            urlsplit(request.url).path,
            dict(request.headers),
            body,
        )


class AdvancingClock:
    def __init__(self, increment_ns: int = 1_000_000_000) -> None:
        self.value: int = 0
        self.increment_ns: int = increment_ns

    def __call__(self) -> int:
        value: int = self.value
        self.value += self.increment_ns
        return value


class ContiguousAttemptClock:
    def __init__(self, duration_ns: int) -> None:
        self.value: int = 0
        self.duration_ns: int = duration_ns
        self.at_start: bool = True

    def __call__(self) -> int:
        if self.at_start:
            self.at_start = False
            return self.value
        self.value += self.duration_ns
        self.at_start = True
        return self.value


class UniqueRandom:
    def __init__(self) -> None:
        self.calls: list[int] = []
        self.value: int = 0

    def __call__(self, size: int) -> bytes:
        self.calls.append(size)
        self.value += 1
        return bytes((self.value + index) % 256 for index in range(size))


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        FakeSession.next_number = 1
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root: Path = Path(self.temp_dir.name)
        self.gateway: FakeGateway = FakeGateway()
        self.random: UniqueRandom = UniqueRandom()
        self.log: target.EvidenceLog = target.EvidenceLog.create(
            self.root / "logs",
            "test_5_1_5_2_b",
            lambda: FIXED_NOW,
        )
        self.addCleanup(self._close_log)

    def _close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST")

    def make_runner(
        self,
        attempt_count: int = 3,
        clock: AdvancingClock | None = None,
    ) -> target.FunctionalTest_5_1_5_2_b:
        return target.FunctionalTest_5_1_5_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(self.gateway),
            random_bytes=self.random,
            monotonic_ns=clock or AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000),
            attempt_count=attempt_count,
        )

    def test_production_constants_and_mechanisms_are_exact(self) -> None:
        self.assertEqual(10, target.ATTEMPTS_PER_TARGET)
        self.assertEqual(1.00, target.MIN_FAILED_ATTEMPT_SECONDS)
        self.assertEqual(0.250, target.MAX_SUCCESS_AVERAGE_SECONDS)
        self.assertEqual(1.12, target.MAX_INTERACTIVE_ATTEMPTS_PER_SECOND)
        self.assertEqual(
            {
                AuthMech.LAN_WEBUI_DEFAULT,
                AuthMech.LAN_WEBUI_USER_DEFINED,
                AuthMech.M2M_API_BEARER_RO,
                AuthMech.M2M_API_BEARER_RW,
                AuthMech.LAN_WEBUI_BASIC,
                AuthMech.LAN_WEBUI_DIGEST,
                AuthMech.LAN_WEBUI_UNAUTHENTICATED,
                AuthMech.LAN_WEBUI_DISABLED,
            },
            set(target.AUTH_MECHANISMS),
        )

    def test_rejected_default_login_requires_reset_and_stops_before_campaigns(self) -> None:
        with mock.patch.object(target.GatewayClient, "calculate_digest_ha1", return_value="corrupt"):
            result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual([GatewayApi.AUTH, GatewayApi.AUTH], [call.path for call in self.gateway.calls])
        self.assertEqual([], self.gateway.config_bodies)

    def test_identity_precedes_nondefault_baseline_reset_advice(self) -> None:
        mac: Any
        for mac in (None, 123, "invalid", "11:22:33:44:55:66", CONFIG.gw_mac):
            with self.subTest(mac=mac):
                runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
                payload: dict[str, Any] = {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
                    GatewayCfgDesc.LAN_AUTH_USER: "NotAdmin",
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
                }
                if mac is not None:
                    payload[GatewayCfgDesc.GW_MAC] = mac
                with self.assertRaises(target.InvalidSetup):
                    runner._validate_baseline(payload)
                self.assertEqual(mac == CONFIG.gw_mac, runner.factory_reset_required)

    def test_late_bearer_control_failure_overrides_prior_pass(self) -> None:
        mechanism: str
        for mechanism in (AuthMech.M2M_API_BEARER_RO, AuthMech.M2M_API_BEARER_RW):
            with self.subTest(mechanism=mechanism):
                self.check_late_bearer_control_failure(mechanism)

    def test_successful_login_checks_identity_before_nondefault_auth_mode(self) -> None:
        mac: Any
        for mac in (None, 123, "bad", "11:22:33:44:55:66", CONFIG.gw_mac):
            with self.subTest(mac=mac):
                self.gateway = FakeGateway()
                original: Callable[..., FakeResponse] = self.gateway.response_for

                def response(
                    session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any,
                    original_response: Callable[..., FakeResponse] = original, identity_mac: Any = mac,
                ) -> FakeResponse:
                    reply: FakeResponse = original_response(session, method, path, headers, body)
                    if method == HttpMethod.GET and path == GatewayApi.AUTH:
                        reply._payload[GatewayCfgDesc.LAN_AUTH_TYPE] = GatewayCfgLanAuthType.RUUVI
                    if method == HttpMethod.GET and path == GatewayApi.CONFIG:
                        reply._payload.pop(GatewayCfgDesc.GW_MAC, None)
                        if identity_mac is not None:
                            reply._payload[GatewayCfgDesc.GW_MAC] = identity_mac
                    return reply

                with mock.patch.object(self.gateway, "response_for", side_effect=response):
                    result: RunResult = self.make_runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE if mac == CONFIG.gw_mac else None, result.recovery_message)
                self.assertEqual(
                    [GatewayApi.AUTH, GatewayApi.AUTH, GatewayApi.CONFIG], [c.path for c in self.gateway.calls]
                )
                self.assertEqual([], self.gateway.config_bodies)

    def test_bearer_wire_sequence_has_reads_before_noop_posts_and_no_scan(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        self.assertEqual(0, runner.run().exit_code)
        ro: str = self.gateway.config_bodies[0][GatewayCfgDesc.LAN_AUTH_API_KEY]
        rw: str = self.gateway.config_bodies[0][GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
        # Stop at the Basic-mode transition; its independent state comes later.
        calls: list[RecordedRequest] = []
        call: RecordedRequest
        for call in self.gateway.calls:
            if isinstance(call.body, dict) and call.body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.BASIC:
                break
            if call.bearer_token:
                calls.append(call)
        self.assertEqual(
            [
                ("GET", "/history", "RO"),
                ("GET", "/status.json", "RO"), ("GET", "/status.json", "RO"),
                ("GET", "/status.json", "RW"), ("GET", "/status.json", "RW"),
                ("GET", "/history", "guess"), ("GET", "/history", "guess"),
                ("GET", "/history", "RO"), ("POST", "/ruuvi.json", "RO"),
                ("GET", "/ruuvi.json", "RW"), ("POST", "/ruuvi.json", "RW"),
                ("GET", "/ruuvi.json", "RW"),
                ("POST", "/ruuvi.json", "guess"), ("POST", "/ruuvi.json", "guess"),
                ("GET", "/ruuvi.json", "RW"), ("POST", "/ruuvi.json", "RW"),
                ("GET", "/ruuvi.json", "RW"),
            ],
            [(c.method, c.path, "RO" if c.bearer_token == ro else "RW" if c.bearer_token == rw else "guess") for c in calls],
        )
        guesses: list[RecordedRequest] = [c for c in calls if c.bearer_token not in {ro, rw}]
        self.assertEqual(4, len({c.session_number for c in guesses}))
        for call in calls:
            self.assertIsNone(call.cookie)
            self.assertEqual({} if call.method == HttpMethod.POST else None, call.body)
        self.assertNotIn(GatewayApi.AP, [c.path for c in self.gateway.calls])

    def test_unexpected_interactive_success_aborts_before_provisioning(self) -> None:
        self.gateway.wrong_login_success = True
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        result: RunResult = runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
        self.assertEqual("NOT RUN", result.outcomes["temporary-state setup"])
        self.assertEqual(1, runner.campaigns[AuthMech.LAN_WEBUI_DEFAULT].attempted)
        self.assertEqual([], self.gateway.config_bodies)

    def test_unexpected_rw_guess_success_aborts_to_restoration(self) -> None:
        original: Callable[..., FakeResponse] = self.gateway.response_for
        bypass_index: int | None = None

        def response(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any
        ) -> FakeResponse:
            nonlocal bypass_index
            token: str | None = FakeGateway._bearer(headers)
            if method == HttpMethod.POST and path == GatewayApi.CONFIG and token not in {None, self.gateway.ro_key, self.gateway.rw_key}:
                self.gateway.bearer_guess_status = 204
                bypass_index = len(self.gateway.calls)
            return original(session, method, path, headers, body)

        with mock.patch.object(self.gateway, "response_for", side_effect=response):
            result: RunResult = self.make_runner().run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.LAN_WEBUI_BASIC])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertIsNotNone(bypass_index)
        assert bypass_index is not None
        remaining: list[RecordedRequest] = self.gateway.calls[bypass_index + 1 :]
        self.assertEqual(
            [("POST", "/ruuvi.json"), ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
             ("GET", "/auth"), ("POST", "/auth"), ("GET", "/history"), ("GET", "/ruuvi.json")],
            [(c.method, c.path) for c in remaining],
        )
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, remaining[0].body[GatewayCfgDesc.LAN_AUTH_TYPE])

    def test_rw_noop_mutation_fails_its_mechanism_and_restores(self) -> None:
        original: Callable[..., FakeResponse] = self.gateway.response_for

        def response(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any
        ) -> FakeResponse:
            reply: FakeResponse = original(session, method, path, headers, body)
            if method == HttpMethod.POST and path == GatewayApi.CONFIG and body == {} and reply.status_code == 200:
                self.gateway.custom_username = "unexpected-change"
            return reply

        with mock.patch.object(self.gateway, "response_for", side_effect=response):
            result: RunResult = self.make_runner().run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_missing_mechanism_pass_cannot_produce_overall_pass(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        with mock.patch.object(runner, "_test_additional_auth_modes"):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_mode_transition_error_is_attributed_and_restored(self) -> None:
        original: Callable[..., FakeResponse] = self.gateway.response_for

        def response(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any
        ) -> FakeResponse:
            if isinstance(body, dict) and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.BASIC:
                raise requests.Timeout("mode change may have applied")
            return original(session, method, path, headers, body)

        with mock.patch.object(self.gateway, "response_for", side_effect=response):
            result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes[AuthMech.LAN_WEBUI_BASIC])
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def check_late_bearer_control_failure(self, mechanism: str) -> None:
        self.gateway = FakeGateway()
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        original: Callable[[str, str, set[str]], None] = runner._attack_bearer
        response_for: Callable[[FakeSession, str, str, dict[str, str], Any], FakeResponse] = self.gateway.response_for
        completed: list[str] = []

        def attack(
            mech: str,
            path: str,
            keys: set[str],
        ) -> None:
            original(mech, path, keys)
            completed.append(mech)

        def response(
            session: FakeSession,
            method: str,
            path: str,
            headers: dict[str, str],
            body: Any,
        ) -> FakeResponse:
            reply: FakeResponse = response_for(session, method, path, headers, body)
            token: str | None = FakeGateway._bearer(headers)
            expected_token: str = (
                self.gateway.ro_key if mechanism == AuthMech.M2M_API_BEARER_RO else self.gateway.rw_key
            )
            expected_path: str = GatewayApi.HISTORY if mechanism == AuthMech.M2M_API_BEARER_RO else GatewayApi.CONFIG
            if mechanism in completed and token == expected_token and path == expected_path and (
                mechanism == AuthMech.M2M_API_BEARER_RO or body == {}
            ):
                return FakeResponse(401, {})
            return reply

        with mock.patch.object(runner, "_attack_bearer", side_effect=attack), mock.patch.object(
            self.gateway,
            "response_for",
            side_effect=response,
        ):
            result: RunResult = runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertIn(
            json.dumps({"mechanism": mechanism, "result": "FAIL"}),
            self.log.path.read_text(encoding="utf-8"),
        )

    def test_prepared_login_failure_marks_temporary_setup_error(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        provision: Callable[[str], None] = runner._provision_temporary_state

        def provision_then_reject(realm: str) -> None:
            provision(realm)
            self.gateway.custom_fallback_enabled = False

        with mock.patch.object(runner, "_provision_temporary_state", side_effect=provision_then_reject):
            result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes["temporary-state setup"])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_success_timing_filter_ignores_exactly_one_slow_outlier(self) -> None:
        self.assertEqual(
            [100_000_000, 110_000_000],
            target.without_single_slowest([4_450_000_000, 110_000_000, 100_000_000]),
        )

    def test_default_campaign_uses_admin_and_random_64_bit_formatted_passwords(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=3)
        runner._attack_interactive(
            AuthMech.LAN_WEBUI_DEFAULT,
            GatewayCfgLanAuthType.DEFAULT,
        )
        challenge_requests: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.method == HttpMethod.GET and call.path == GatewayApi.AUTH
        ]
        login_bodies: list[Any] = [
            call.body for call in self.gateway.calls if call.method == HttpMethod.POST and call.path == GatewayApi.AUTH
        ]
        self.assertEqual(1, len(challenge_requests))
        self.assertEqual(3, len(login_bodies))
        self.assertTrue(all(body["login"] == target.ADMIN_USERNAME for body in login_bodies))
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertRegex(
            content,
            r'"password": "[0-9A-F]{2}(?::[0-9A-F]{2}){7}"',
        )
        self.assertGreaterEqual(self.random.calls.count(8), 3)

    def test_default_password_generator_retries_the_configured_password(self) -> None:
        values: Iterator[bytes] = iter(
            (
                bytes.fromhex(CONFIG.gw_id.replace(":", "")),
                bytes.fromhex("8899AABBCCDDEEFF"),
            )
        )
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        runner.random_bytes = lambda size: next(values)
        self.assertEqual("88:99:AA:BB:CC:DD:EE:FF", runner._random_default_password())

    def test_user_defined_attack_reuses_post_challenges_with_configured_username(self) -> None:
        self.gateway.mode = GatewayCfgLanAuthType.RUUVI
        self.gateway.custom_username = "configured-user"
        self.gateway.custom_ha1 = hashlib.md5(
            f"{self.gateway.custom_username}:Ruuvi Gateway:configured-password".encode()
        ).hexdigest()
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=3)
        runner.temporary_username = self.gateway.custom_username
        runner.temporary_password = "configured-password"

        runner._attack_interactive(
            AuthMech.LAN_WEBUI_USER_DEFINED,
            GatewayCfgLanAuthType.RUUVI,
        )

        challenge_requests: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.method == HttpMethod.GET and call.path == GatewayApi.AUTH
        ]
        login_bodies: list[Any] = [
            call.body for call in self.gateway.calls if call.method == HttpMethod.POST and call.path == GatewayApi.AUTH
        ]
        self.assertEqual(1, len(challenge_requests))
        self.assertEqual(3, len(login_bodies))
        self.assertTrue(all(body["login"] == self.gateway.custom_username for body in login_bodies))

    def test_baseline_accepts_equal_json_decoded_auth_mode(self) -> None:
        payload: dict[str, Any] = json.loads(
            json.dumps(
                {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
                    GatewayCfgDesc.LAN_AUTH_USER: target.ADMIN_USERNAME,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
                }
            )
        )
        self.make_runner()._validate_baseline(payload)

    def test_complete_reduced_sequence_passes_and_restores(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        result: RunResult = runner.run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertTrue(all(value == "PASS" for value in result.outcomes.values()))
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("", self.gateway.rw_key)
        mechanism: str
        for mechanism in target.MECHANISMS[:4]:
            self.assertEqual(3, runner.campaigns[mechanism].attempted)
            self.assertEqual(3, len(runner.success_durations_ns[mechanism]))
        for mechanism in (
            AuthMech.LAN_WEBUI_DEFAULT,
            AuthMech.LAN_WEBUI_USER_DEFINED,
        ):
            timing: target.InteractiveTimingStats = runner.interactive_timings[mechanism]
            self.assertEqual(3, len(timing.successful_get_durations_ns))
            self.assertEqual(3, len(timing.successful_post_durations_ns))
            self.assertEqual(1, len(timing.failed_get_durations_ns))
            self.assertEqual(3, len(timing.failed_post_durations_ns))
        for mechanism in (
            AuthMech.LAN_WEBUI_BASIC,
            AuthMech.LAN_WEBUI_DIGEST,
        ):
            self.assertEqual(3, runner.campaigns[mechanism].attempted)
            self.assertEqual(3, len(runner.success_durations_ns[mechanism]))
        self.assertEqual(
            3,
            len(runner.success_durations_ns[AuthMech.LAN_WEBUI_UNAUTHENTICATED]),
        )
        self.assertEqual(
            3,
            runner.campaigns[AuthMech.LAN_WEBUI_DISABLED].attempted,
        )

    def test_basic_stored_token_fits_firmware_password_buffer(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner()
        self.assertEqual(0, runner.run().exit_code)
        body: dict[str, Any] = next(
            body
            for body in self.gateway.config_bodies
            if body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.BASIC
        )
        token: str = body[GatewayCfgDesc.LAN_AUTH_PASS]
        decoded: str = base64.b64decode(token).decode("utf-8")
        self.assertLessEqual(len(token), 64)
        self.assertEqual(body[GatewayCfgDesc.LAN_AUTH_USER], decoded.split(":", 1)[0])

    def test_host_runner_uses_injected_attempt_count_not_production_count(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        result: RunResult = runner.run()
        self.assertEqual(0, result.exit_code)
        self.assertTrue(all(stats.attempted == 2 for stats in runner.campaigns.values()))

    def test_interactive_guesses_are_all_denied(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=4)
        self.assertEqual(0, runner.run().exit_code)
        name: str
        for name in target.MECHANISMS[:2]:
            stats: target.CampaignStats = runner.campaigns[name]
            self.assertEqual((4, 4, 0), (stats.completed, stats.denied, stats.authorized))

    def test_single_too_fast_interactive_sample_fails_and_restores(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(clock=AdvancingClock(500_000_000))
        result: RunResult = runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_failed_attempt_of_exactly_one_second_fails(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(clock=AdvancingClock(1_000_000_000))
        result: RunResult = runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))

    def test_interactive_failure_of_exactly_one_second_fails(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(
            attempt_count=1,
            clock=AdvancingClock(1_000_000_000),
        )
        with self.assertRaises(target.SecurityFailure):
            runner._attack_interactive(
                AuthMech.LAN_WEBUI_DEFAULT,
                GatewayCfgLanAuthType.DEFAULT,
            )

    def test_missing_interactive_delay_fails(self) -> None:
        result: RunResult = self.make_runner(clock=AdvancingClock(0)).run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))

    def test_successful_average_must_be_strictly_below_250_ms(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        runner.success_monotonic_ns = AdvancingClock(250_000_000)
        session: FakeSession = FakeSession(self.gateway)
        session.authorized = True
        with self.assertRaises(target.SecurityFailure):
            runner._measure_success(
                AuthMech.LAN_WEBUI_DEFAULT,
                session,
            )
        self.assertEqual("FAIL", runner.outcomes[AuthMech.LAN_WEBUI_DEFAULT])

    def test_unauthenticated_mode_does_not_enforce_response_timing(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        runner.success_monotonic_ns = AdvancingClock(600_000_000)
        runner._measure_http_auth_campaign(
            AuthMech.LAN_WEBUI_UNAUTHENTICATED,
            True,
            HttpStatus.C_200_OK,
            lambda: FakeResponse(HttpStatus.C_200_OK, {"status": "ok"}),
            True,
            enforce_timing=False,
        )
        self.assertEqual(
            "PASS",
            runner.outcomes[AuthMech.LAN_WEBUI_UNAUTHENTICATED],
        )

    def test_digest_challenge_is_prepared_before_response_timing(self) -> None:
        events: list[str] = []

        def clock() -> int:
            events.append("clock")
            return len(events) * 100_000_000

        def prepare_request() -> Callable[[], requests.Response]:
            events.append("challenge")

            def send_authenticated_request() -> FakeResponse:
                events.append("authenticated request")
                return FakeResponse(HttpStatus.C_200_OK, {"authenticated": True})

            return send_authenticated_request

        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        runner.success_monotonic_ns = clock
        runner._measure_http_auth_campaign(
            AuthMech.LAN_WEBUI_DIGEST,
            True,
            HttpStatus.C_200_OK,
            prepare_request,
            True,
            prepare_before_timing=True,
        )
        self.assertEqual(
            ["challenge", "clock", "authenticated request", "clock"],
            events,
        )

    def test_digest_authenticated_request_retries_after_transport_timeout(self) -> None:
        self.gateway.digest_authenticated_timeouts = 1
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        result: RunResult = runner.run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual(
            2,
            len(runner.success_durations_ns[AuthMech.LAN_WEBUI_DIGEST]),
        )
        self.assertIn(
            "AUTH MODE REQUEST RETRY",
            self.log.path.read_text(encoding="utf-8"),
        )

    def test_aggregate_throughput_failure_is_detected(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=3)
        runner.monotonic_ns = ContiguousAttemptClock(890_000_000)
        with mock.patch.object(target, "MIN_FAILED_ATTEMPT_SECONDS", 0.80), self.assertRaises(target.SecurityFailure):
            runner._attack_interactive(
                AuthMech.LAN_WEBUI_DEFAULT,
                GatewayCfgLanAuthType.DEFAULT,
            )
        stats: target.CampaignStats = runner.campaigns[AuthMech.LAN_WEBUI_DEFAULT]
        self.assertGreater(
            stats.as_evidence().attempts_per_second,
            target.MAX_INTERACTIVE_ATTEMPTS_PER_SECOND,
        )

    def test_unexpected_interactive_success_fails_and_restores(self) -> None:
        self.gateway.wrong_login_success = True
        result: RunResult = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_timeout_and_malformed_challenge_are_errors(self) -> None:
        attribute: str
        for attribute in ("timeout_next", "malformed_challenge"):
            with self.subTest(attribute=attribute):
                gateway: FakeGateway = FakeGateway()
                setattr(gateway, attribute, True)
                runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    random_bytes=UniqueRandom(),
                    monotonic_ns=AdvancingClock(2_100_000_000),
                    success_monotonic_ns=AdvancingClock(100_000_000),
                    attempt_count=2,
                )
                self.assertEqual(2, runner.run().exit_code)

    def test_bearer_guesses_use_32_bytes_and_correct_endpoints(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        self.assertEqual(0, runner.run().exit_code)
        guessed_calls: list[RecordedRequest] = [
            call
            for call in self.gateway.calls
            if call.bearer_token
            and call.bearer_token not in {"", self.gateway.ro_key, self.gateway.rw_key}
        ]
        paths: list[str] = [call.path for call in guessed_calls]
        self.assertGreaterEqual(paths.count(GatewayApi.HISTORY), 2)
        self.assertGreaterEqual(paths.count(GatewayApi.CONFIG), 2)
        self.assertNotIn(GatewayApi.AP, paths)
        self.assertTrue(all(size in {8, 12, 16, 32} for size in self.random.calls))
        self.assertGreaterEqual(self.random.calls.count(32), 4)

    def test_bearer_campaign_fails_when_response_is_faster_than_one_second(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        runner.monotonic_ns = AdvancingClock(1_000)
        with self.assertRaises(target.SecurityFailure):
            runner._attack_bearer(
                AuthMech.M2M_API_BEARER_RO,
                GatewayApi.HISTORY,
                {"configured"},
            )
        self.assertEqual("FAIL", runner.outcomes[AuthMech.M2M_API_BEARER_RO])

    def test_unexpected_bearer_status_fails(self) -> None:
        status: int
        for status in (HttpStatus.C_200_OK, HttpStatus.C_403_FORBIDDEN):
            with self.subTest(status=status):
                gateway: FakeGateway = FakeGateway()
                gateway.bearer_guess_status = status
                runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    random_bytes=UniqueRandom(),
                    monotonic_ns=AdvancingClock(2_100_000_000),
                    success_monotonic_ns=AdvancingClock(100_000_000),
                    attempt_count=1,
                )
                result: RunResult = runner.run()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_bearer_campaign_retries_timeout_with_same_token_and_fresh_session(self) -> None:
        self.gateway.guess_exception = requests.Timeout("timeout")
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        runner._attack_bearer(
            AuthMech.M2M_API_BEARER_RO,
            GatewayApi.HISTORY,
            {"configured"},
        )
        history_calls: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.method == HttpMethod.GET and call.path == GatewayApi.HISTORY
        ]
        self.assertEqual(2, len(history_calls))
        self.assertEqual(
            history_calls[0].bearer_token,
            history_calls[1].bearer_token,
        )
        self.assertEqual("PASS", runner.outcomes[AuthMech.M2M_API_BEARER_RO])
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("BEARER REQUEST RETRY", content)

    def test_exhausted_bearer_retries_are_error_and_restore(self) -> None:
        self.gateway.guess_exceptions = [
            requests.Timeout(f"timeout {index}") for index in range(target.BEARER_REQUEST_ATTEMPTS)
        ]
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_post_provisioning_unexpected_exception_is_error_and_restores(self) -> None:
        self.gateway.guess_exception = RuntimeError("unexpected")
        result: RunResult = self.make_runner(attempt_count=1).run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_temporary_configuration_body_is_exact(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        self.assertEqual(0, runner.run().exit_code)
        setup: dict[str, Any] = self.gateway.config_bodies[0]
        self.assertEqual(
            {
                GatewayCfgDesc.LAN_AUTH_TYPE,
                GatewayCfgDesc.LAN_AUTH_USER,
                GatewayCfgDesc.LAN_AUTH_PASS,
                GatewayCfgDesc.LAN_AUTH_API_KEY,
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW,
            },
            set(setup),
        )
        self.assertEqual(32, len(setup[GatewayCfgDesc.LAN_AUTH_PASS]))
        restoration: dict[str, Any] = self.gateway.config_bodies[-1]
        self.assertEqual(
            {
                GatewayCfgDesc.LAN_AUTH_TYPE,
                GatewayCfgDesc.LAN_AUTH_API_KEY,
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW,
            },
            set(restoration),
        )

    def test_prepared_state_read_retries_a_transient_timeout(self) -> None:
        self.gateway.prepared_config_timeouts = 1
        result: RunResult = self.make_runner(attempt_count=1).run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual(0, self.gateway.prepared_config_timeouts)
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("PREPARED STATE READ RETRY", content)

    def test_prepared_state_read_stops_after_bounded_retries_and_restores(self) -> None:
        self.gateway.prepared_config_timeouts = target.PREPARED_STATE_READ_ATTEMPTS
        result: RunResult = self.make_runner(attempt_count=1).run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_primary_and_both_restoration_fallbacks(self) -> None:
        with self.subTest(method="primary"):
            runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
            self.assertEqual(0, runner.run().exit_code)
            self.assertEqual(["bearer"], self.gateway.restoration_attempts)

        gateway: FakeGateway = FakeGateway()
        gateway.primary_restore_status = HttpStatus.C_401_UNAUTHORIZED
        runner = target.FunctionalTest_5_1_5_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
            random_bytes=UniqueRandom(),
            monotonic_ns=AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000),
            attempt_count=1,
        )
        self.assertEqual(0, runner.run().exit_code)
        self.assertEqual(["bearer", "custom"], gateway.restoration_attempts)

        gateway = FakeGateway()
        gateway.primary_restore_status = HttpStatus.C_500_INTERNAL_SERVER_ERROR
        gateway.primary_restore_applies = True
        runner = target.FunctionalTest_5_1_5_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
            random_bytes=UniqueRandom(),
            monotonic_ns=AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000),
            attempt_count=1,
        )
        self.assertEqual(0, runner.run().exit_code)
        self.assertEqual(["bearer", "default"], gateway.restoration_attempts)

    def test_restoration_failure_or_verification_mismatch_prevents_pass(self) -> None:
        attribute: str
        for attribute in ("restored_config_mismatch", "temporary_bearers_survive"):
            with self.subTest(attribute=attribute):
                gateway: FakeGateway = FakeGateway()
                setattr(gateway, attribute, True)
                runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    random_bytes=UniqueRandom(),
                    monotonic_ns=AdvancingClock(2_100_000_000),
                    success_monotonic_ns=AdvancingClock(100_000_000),
                    attempt_count=1,
                )
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))

        gateway = FakeGateway()
        gateway.primary_restore_status = HttpStatus.C_401_UNAUTHORIZED
        gateway.custom_fallback_enabled = False
        runner = target.FunctionalTest_5_1_5_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
            random_bytes=UniqueRandom(),
            monotonic_ns=AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000),
            attempt_count=1,
        )
        result = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes["final restoration"])

    def test_percentile_interpolation_is_deterministic(self) -> None:
        values: list[int] = [0, 10, 20, 30, 40]
        self.assertEqual(20, target.percentile(values, 0.50))
        self.assertEqual(2, target.percentile(values, 0.05))
        self.assertEqual(38, target.percentile(values, 0.95))


class ExecutionTestCase(unittest.TestCase):
    def test_execute_log_and_terminal_progress_match_shared_pattern(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            (root / ".env").write_text(env_text(), encoding="utf-8")
            gateway: FakeGateway = FakeGateway()
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_5_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                monotonic_ns=AdvancingClock(2_100_000_000),
                success_monotonic_ns=AdvancingClock(100_000_000),
                attempt_count=1,
                random_bytes=UniqueRandom(),
                output=messages.append,
            )
            self.assertEqual(0, result.exit_code)
            log_path: Path = next((root / "logs").iterdir())
            self.assertTrue(log_path.name.startswith("test_5_1_5_2_b_"))
            self.assertEqual(1, sum(str(log_path) in line for line in messages))
            progress: list[str] = [line for line in messages if line.startswith("[Step ")]
            self.assertEqual(target.TOTAL_STEPS, len(progress))
            attempts: list[str] = [line for line in messages if "completed in" in line]
            self.assertEqual(16, len(attempts))
            self.assertTrue(all("seconds" in line for line in attempts))
            summary: list[str] = [line for line in messages if "result=" in line]
            self.assertEqual(8, len(summary))
            self.assertEqual("Overall verdict: PASS", messages[-1])
            content: str = log_path.read_text(encoding="utf-8")
            self.assertIn("CAMPAIGN ATTEMPT", content)
            self.assertIn("INTERACTIVE AUTH STAGE ATTEMPT", content)
            self.assertIn("INTERACTIVE AUTH STAGE SUMMARY", content)
            self.assertIn(f'"method": "{HttpMethod.GET}"', content)
            self.assertIn(f'"method": "{HttpMethod.POST}"', content)
            self.assertIn("monotonic_start_ns", content)
            self.assertIn("OVERALL VERDICT: PASS", content)

    def test_execute_configuration_failure_is_error(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_5_2_b(
                Path(directory),
                now=lambda: FIXED_NOW,
                attempt_count=1,
                output=messages.append,
            )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertEqual("Overall verdict: ERROR", messages[-1])

    def test_execute_requests_factory_reset_when_gateway_is_not_in_default_mode(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.check_nondefault_recovery(mode)

    def check_nondefault_recovery(self, mode: str) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            (root / ".env").write_text(env_text(), encoding="utf-8")
            gateway: FakeGateway = FakeGateway()
            gateway.mode = mode
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_5_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                attempt_count=1,
                output=messages.append,
            )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
            self.assertEqual(target.FACTORY_RESET_MESSAGE, messages[-1])
            self.assertEqual([GatewayApi.AUTH], [call.path for call in gateway.calls])
            self.assertIn(
                "GatewayAuthenticationModeError",
                next((root / "logs").iterdir()).read_text(encoding="utf-8"),
            )
            self.assertIn(
                "USER ACTION REQUIRED",
                next((root / "logs").iterdir()).read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
