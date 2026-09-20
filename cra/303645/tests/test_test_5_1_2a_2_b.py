"""Implementation tests only; these are not ETSI compliance evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable
from unittest import mock
from urllib.parse import urlsplit

import requests
from Crypto.PublicKey import ECC

import test_5_1_2a_2_b as target
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    InteractiveAuthChallenge,
)
from lib.http_api import HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import RunResult

FIXED_NOW: datetime = datetime(2026, 9, 3, 4, 20, 4, 888000, tzinfo=timezone.utc)
CONFIG: target.DutConfig = target.DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)
RO_KEY: str = base64.urlsafe_b64encode(bytes(range(32))).decode("ascii").rstrip("=")
RW_KEY: str = base64.urlsafe_b64encode(bytes(range(32, 64))).decode("ascii").rstrip("=")


class DeterministicRandom:
    def __init__(self) -> None:
        self.calls: int = 0

    def __call__(self, size: int) -> bytes:
        self.calls += 1
        start: int = 0 if self.calls % 2 == 1 else 32
        return bytes((start + index) % 256 for index in range(size))


deterministic_random: DeterministicRandom = DeterministicRandom()


def env_text() -> str:
    return (
        f"{GatewayCfgDesc.GW_ID}={CONFIG.gw_id}\n"
        f"{GatewayCfgDesc.GW_MAC}={CONFIG.gw_mac}\n"
        f"{GatewayCfgDesc.GW_HOSTNAME}={CONFIG.gw_hostname}\n"
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
    session_number: int
    method: str
    path: str
    authorization: str
    body: Any
    allow_redirects: bool

    @property
    def scheme(self) -> str:
        return self.authorization.split(" ", 1)[0] if self.authorization else "none"


class FakeGateway:
    def __init__(self) -> None:
        self.calls: list[RecordedRequest] = []
        self.ro_key: str = ""
        self.rw_key: str = ""
        self.auth_user: str = target.ADMIN_USERNAME
        self.auth_mode: str = GatewayCfgLanAuthType.DEFAULT
        self.config_reads: int = 0
        self.config_overrides: dict[int, dict[str, Any]] = {}
        self.restore_admin_failures: int = 0
        self.restore_rw_failure: bool = False
        self.override: dict[tuple[str, str, str], FakeResponse | BaseException] = {}
        self.connection_error: BaseException | None = None
        self.malformed_config: bool = False
        self.mutate_after_positive: bool = False
        self.server_key: ECC.EccKey = ECC.generate(curve="secp256r1")
        public: ECC.EccKey = self.server_key.public_key()
        raw: bytes = b"\x04" + int(public.pointQ.x).to_bytes(32, "big") + int(public.pointQ.y).to_bytes(32, "big")
        self.server_public_b64: str = base64.b64encode(raw).decode("ascii")

    def config(self) -> dict[str, Any]:
        return {
            GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode,
            GatewayCfgDesc.LAN_AUTH_USER: self.auth_user,
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: bool(self.ro_key),
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: bool(self.rw_key),
            GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
            GatewayCfgDesc.FW_VER: "test-fw",
            GatewayCfgDesc.NRF52_FW_VER: "test-nrf",
            "stable": "mutated" if self.mutate_after_positive else "value",
        }

    @staticmethod
    def token(authorization: str) -> str | None:
        prefix: str = f"{HttpAuthScheme.BEARER} "
        return authorization[len(prefix) :] if authorization.startswith(prefix) else None

    def response_for(
        self,
        session: FakeSession,
        method: str,
        path: str,
        headers: dict[str, str],
        body: Any,
        allow_redirects: bool,
    ) -> FakeResponse:
        authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
        call: RecordedRequest = RecordedRequest(
            session.number,
            method,
            path,
            authorization,
            body,
            allow_redirects,
        )
        self.calls.append(call)
        if self.connection_error is not None:
            error: BaseException = self.connection_error
            self.connection_error = None
            raise error
        override: FakeResponse | BaseException | None = self.override.get(
            (authorization, method, path),
            self.override.get((call.scheme, method, path)),
        )
        if isinstance(override, BaseException):
            raise override
        if override is not None:
            return override

        if path == GatewayApi.AUTH and method == HttpMethod.GET:
            if self.auth_mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
                scheme: str = (
                    HttpAuthScheme.BASIC if self.auth_mode == GatewayCfgLanAuthType.BASIC else HttpAuthScheme.DIGEST
                )
                return FakeResponse(
                    401,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                    {HttpHeader.WWW_AUTHENTICATE: f'{scheme} realm="Ruuvi Gateway"'},
                )
            session_id: str = f"session-{session.number}"
            session.challenge_number += 1
            session.challenge = f"challenge-{session.number}-{session.challenge_number}"
            session.cookie = session_id
            header: str = (
                f'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="{session.challenge}" '
                f'session_cookie="RUUVISESSION" session_id="{session_id}"'
            )
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                headers={
                    HttpHeader.WWW_AUTHENTICATE: header,
                    HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.server_public_b64,
                },
                cookies={"RUUVISESSION": session_id},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.POST:
            ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
            expected_body: dict[str, str] = {
                "login": "Admin",
                "password": hashlib.sha256(f"{session.challenge}:{ha1}".encode()).hexdigest(),
            }
            if (
                session.challenge
                and body == expected_body
                and headers.get(HttpHeader.COOKIE) == f"RUUVISESSION={session.cookie}"
            ):
                session.authorized = True
                session.challenge = ""
                return FakeResponse(HttpStatus.C_200_OK, {"authenticated": True})
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"authenticated": False})

        bearer: str | None = self.token(authorization)
        read_allowed: bool = bearer in {self.ro_key, self.rw_key} and bearer != ""
        write_allowed: bool = bearer == self.rw_key and bearer != ""
        authorized_read: bool = session.authorized or read_allowed
        authorized_write: bool = session.authorized or write_allowed

        if path == GatewayApi.CONFIG and method == HttpMethod.GET and authorized_read:
            self.config_reads += 1
            if self.malformed_config:
                return FakeResponse(HttpStatus.C_200_OK, malformed_json=True)
            return FakeResponse(HttpStatus.C_200_OK, self.config_overrides.get(self.config_reads, self.config()))
        if path == GatewayApi.HISTORY and method == HttpMethod.GET and authorized_read:
            return FakeResponse(HttpStatus.C_200_OK, {"data": []})
        if path == GatewayApi.CONFIG and method == HttpMethod.POST and authorized_write:
            restoration: bool = body == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            }
            if restoration and session.authorized and self.restore_admin_failures > 0:
                self.restore_admin_failures -= 1
                return FakeResponse(HttpStatus.C_500_INTERNAL_SERVER_ERROR, {"error": "injected"})
            if restoration and bearer == self.rw_key and self.restore_rw_failure:
                return FakeResponse(HttpStatus.C_500_INTERNAL_SERVER_ERROR, {"error": "injected"})
            if isinstance(body, dict):
                if GatewayCfgDesc.LAN_AUTH_API_KEY in body:
                    self.ro_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY]
                if GatewayCfgDesc.LAN_AUTH_API_KEY_RW in body:
                    self.rw_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
            return FakeResponse(HttpStatus.C_200_OK, {})

        if bearer is not None:
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})
        return FakeResponse(
            HttpStatus.C_302_FOUND if method == HttpMethod.GET else HttpStatus.C_401_UNAUTHORIZED,
            {"error": "unauthorized"},
        )


class FakeSession(requests.Session):
    next_number: int = 1

    def __init__(self, gateway: FakeGateway) -> None:
        super().__init__()
        self.gateway: FakeGateway = gateway
        self.authorized: bool = False
        self.challenge_number: int = 0
        self.challenge: str = ""
        self.cookie: str = ""
        self.number: int = FakeSession.next_number
        FakeSession.next_number += 1

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        return request.prepare()

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        body: Any = json.loads(request.body) if request.body else None
        return self.gateway.response_for(
            self,
            request.method,
            urlsplit(request.url).path,
            dict(request.headers),
            body,
            kwargs["allow_redirects"],
        )


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        FakeSession.next_number = 1
        deterministic_random.calls = 0
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root: Path = Path(self.temp_dir.name)
        self.gateway: FakeGateway = FakeGateway()
        self.log: target.EvidenceLog = target.EvidenceLog.create(
            self.root / "logs",
            "test_5_1_2a_2_b",
            lambda: FIXED_NOW,
        )
        self.addCleanup(self.close_log)

    def close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST")

    def runner(self) -> target.FunctionalTest_5_1_2a_2_b:
        return target.FunctionalTest_5_1_2a_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(self.gateway),
            random_bytes=deterministic_random,
        )

    def test_full_sequence_passes_with_exact_matrices_and_bodies(self) -> None:
        result: RunResult = self.runner().run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertTrue(all(outcome == "PASS" for outcome in result.outcomes.values()))

        probes: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.path in {GatewayApi.HISTORY, GatewayApi.CONFIG}
        ]
        expected_negative: dict[tuple[str, ...], int] = {
            (HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.HISTORY): 302,
            (HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.CONFIG): 302,
            (HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG): 401,
            (HttpAuthScheme.DIGEST, HttpMethod.GET, GatewayApi.HISTORY): 302,
            (HttpAuthScheme.DIGEST, HttpMethod.GET, GatewayApi.CONFIG): 302,
            (HttpAuthScheme.DIGEST, HttpMethod.POST, GatewayApi.CONFIG): 401,
        }
        key: tuple[str, ...]
        for key in expected_negative:
            self.assertTrue(
                any((call.scheme, call.method, call.path) == key for call in probes),
                key,
            )
        password_bearer: str = f"{HttpAuthScheme.BEARER} {CONFIG.gw_id}"
        self.assertEqual(
            3,
            sum(call.authorization == password_bearer for call in self.gateway.calls),
        )
        password_body: dict[str, str] = next(
            call.body
            for call in self.gateway.calls
            if isinstance(call.body, dict)
            and call.body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI
        )
        self.assertEqual(CONFIG.gw_id, password_body["password"])
        self.assertNotIn(HttpHeader.AUTHORIZATION, password_body)

        writes: list[Any] = [
            call.body
            for call in self.gateway.calls
            if call.method == HttpMethod.POST and call.path == GatewayApi.CONFIG
        ]
        self.assertIn(
            {
                GatewayCfgDesc.LAN_AUTH_API_KEY: RO_KEY,
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: RW_KEY,
            },
            writes,
        )
        self.assertIn(
            {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            },
            writes,
        )
        self.assertTrue(all(not call.allow_redirects for call in self.gateway.calls))

    def test_negative_success_is_fail_and_aborts_before_later_negative_probes(self) -> None:
        self.gateway.override[(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.HISTORY)] = FakeResponse(
            HttpStatus.C_200_OK, {"protected": True}
        )
        result: RunResult = self.runner().run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertFalse(any(call.scheme == HttpAuthScheme.DIGEST for call in self.gateway.calls))
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("", self.gateway.rw_key)

    def test_full_probe_order_and_bodies_at_transport_boundary(self) -> None:
        result: RunResult = self.runner().run()
        self.assertEqual(0, result.exit_code)
        calls: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.path in (GatewayApi.HISTORY, GatewayApi.CONFIG)
        ]
        password_hash: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        password_body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
            GatewayCfgDesc.LAN_AUTH_USER: "Admin",
            GatewayCfgDesc.LAN_AUTH_PASS: password_hash,
            "password": CONFIG.gw_id,
        }
        expected: list[tuple[str, str, str, Any]] = [
            ("none", "GET", "/ruuvi.json", None),
            ("none", "POST", "/ruuvi.json", {"lan_auth_api_key": RO_KEY, "lan_auth_api_key_rw": RW_KEY}),
            ("none", "GET", "/ruuvi.json", None),
            ("Basic", "GET", "/history", None),
            ("Basic", "GET", "/ruuvi.json", None),
            ("Digest", "GET", "/history", None),
            ("Digest", "GET", "/ruuvi.json", None),
            (f"Bearer {CONFIG.gw_id}", "GET", "/history", None),
            (f"Bearer {CONFIG.gw_id}", "GET", "/ruuvi.json", None),
            (f"Bearer {RO_KEY}", "GET", "/history", None),
            (f"Bearer {RO_KEY}", "GET", "/ruuvi.json", None),
            (f"Bearer {RW_KEY}", "GET", "/history", None),
            (f"Bearer {RW_KEY}", "GET", "/ruuvi.json", None),
            ("Basic", "POST", "/ruuvi.json", {}),
            ("Digest", "POST", "/ruuvi.json", {}),
            (f"Bearer {CONFIG.gw_id}", "POST", "/ruuvi.json", {}),
            ("none", "POST", "/ruuvi.json", password_body),
            ("none", "GET", "/ruuvi.json", None),
            (f"Bearer {RO_KEY}", "POST", "/ruuvi.json", {}),
            (f"Bearer {RW_KEY}", "POST", "/ruuvi.json", {}),
            ("none", "GET", "/ruuvi.json", None),
            ("none", "POST", "/ruuvi.json", {"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}),
            ("none", "GET", "/ruuvi.json", None),
            (f"Bearer {RO_KEY}", "GET", "/history", None),
            (f"Bearer {RW_KEY}", "POST", "/ruuvi.json", {}),
        ]
        self.assertEqual(
            expected,
            [
                (call.authorization if call.scheme == "Bearer" else call.scheme, call.method, call.path, call.body)
                for call in calls
            ],
        )
        probes: list[RecordedRequest] = calls[3:17] + calls[18:20]
        self.assertEqual(len(probes), len({call.session_number for call in probes}))
        self.assertTrue(all(not call.allow_redirects for call in calls))

    def test_last_read_failures_prevent_all_probe_writes(self) -> None:
        authorization: str
        for authorization in (f"Bearer {CONFIG.gw_id}", f"Bearer {RW_KEY}"):
            with self.subTest(authorization=authorization):
                self.gateway = FakeGateway()
                deterministic_random.calls = 0
                self.gateway.override[(authorization, "GET", "/ruuvi.json")] = FakeResponse(500, {})
                result: RunResult = self.runner().run()
                self.assertEqual(1, result.exit_code)
                writes: list[RecordedRequest] = [
                    call for call in self.gateway.calls if call.method == "POST" and call.path == "/ruuvi.json"
                ]
                self.assertEqual(3, len(writes))  # Provisioning, restoration, revoked-key verification.
                self.assertTrue(all("lan_auth_api_key" in call.body for call in writes[:2]))
                self.assertEqual((f"Bearer {RW_KEY}", {}), (writes[-1].authorization, writes[-1].body))

    def test_dangerous_success_aborts_remaining_probes_and_restores(self) -> None:
        self.gateway.override[("Basic", "POST", "/ruuvi.json")] = FakeResponse(200, {})
        result: RunResult = self.runner().run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertFalse(any(call.method == "POST" and call.scheme == "Digest" for call in self.gateway.calls))
        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])

    def test_corrupt_login_response_and_wrong_password_cannot_pass_setup(self) -> None:
        with mock.patch.object(target.GatewayClient, "calculate_digest_ha1", return_value="corrupt"):
            result: RunResult = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual(["/auth", "/auth"], [call.path for call in self.gateway.calls])
        runner: target.FunctionalTest_5_1_2a_2_b = self.runner()
        login: target.InteractiveAuthResult = runner.gateway.authenticate_interactive("Admin", "wrong-password")
        self.assertEqual(401, login.login_response.status_code)
        assert isinstance(login.session, FakeSession)
        self.assertFalse(login.session.authorized)

    def test_post_negative_hash_failure_is_rw_fail_in_final_evidence(self) -> None:
        prepared: dict[str, Any] = self.gateway.config()
        prepared.update(lan_auth_api_key_use=True, lan_auth_api_key_rw_use=True, stable="mutated")
        self.gateway.config_overrides[5] = prepared  # After baseline, prepared, and two bearer reads.
        result: RunResult = self.runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertIn(
            json.dumps({"mechanism": AuthMech.M2M_API_BEARER_RW, "result": "FAIL"}),
            self.log.path.read_text(encoding="utf-8"),
        )

    def test_login_without_matching_cookie_or_outstanding_challenge_is_rejected(self) -> None:
        fault: str
        for fault in ("missing-cookie", "cookie", "challenge", "extra-body-field"):
            with self.subTest(fault=fault):
                self.gateway = FakeGateway()

                class CorruptLoginSession(FakeSession):
                    def send(
                        self,
                        request: requests.PreparedRequest,
                        *,
                        injected_fault: str = fault,
                        **kwargs: Any,
                    ) -> FakeResponse:
                        if request.method == "POST" and urlsplit(request.url).path == "/auth":
                            if injected_fault == "missing-cookie":
                                request.headers.pop(HttpHeader.COOKIE, None)
                            elif injected_fault == "cookie":
                                request.headers[HttpHeader.COOKIE] = "RUUVISESSION=another-session"
                            elif injected_fault == "challenge":
                                self.challenge = ""
                            else:
                                body: dict[str, str] = json.loads(request.body)
                                body["extra"] = "unexpected"
                                request.body = json.dumps(body)
                        return super().send(request, **kwargs)

                runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda session_type=CorruptLoginSession: session_type(self.gateway),
                )
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(["/auth", "/auth"], [call.path for call in self.gateway.calls])

    def test_fake_login_requires_current_one_time_session_challenge(self) -> None:
        client: target.GatewayClient = self.runner().gateway
        first: InteractiveAuthChallenge = client.request_interactive_challenge()
        second: InteractiveAuthChallenge = client.request_interactive_challenge()
        assert isinstance(first.session, FakeSession)
        assert isinstance(second.session, FakeSession)
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()

        def login_body(challenge: str) -> dict[str, str]:
            return {
                "login": "Admin",
                "password": hashlib.sha256(f"{challenge}:{ha1}".encode()).hexdigest(),
            }

        def submit(session: requests.Session, submitted_body: dict[str, str], submitted_cookie: str | None) -> int:
            response: requests.Response = client.request(
                session,
                HttpMethod.POST,
                GatewayApi.AUTH,
                headers={HttpHeader.COOKIE: submitted_cookie} if submitted_cookie is not None else {},
                json_body=submitted_body,
            )
            return response.status_code

        body: dict[str, str] = login_body(first.challenge["challenge"])
        cookie: str = f"RUUVISESSION={first.cookie}"
        fresh: FakeSession = FakeSession(self.gateway)
        self.assertEqual(401, submit(fresh, body, cookie))
        self.assertFalse(fresh.authorized)
        self.assertEqual(401, submit(first.session, body, None))
        self.assertEqual(401, submit(first.session, body, f"RUUVISESSION={second.cookie}"))
        self.assertFalse(first.session.authorized)
        self.assertEqual(401, submit(second.session, body, f"RUUVISESSION={second.cookie}"))
        self.assertFalse(second.session.authorized)

        # Refresh the same session: the previously issued response must become stale.
        refreshed: requests.Response = client.request(first.session, HttpMethod.GET, GatewayApi.AUTH)
        self.assertEqual(401, refreshed.status_code)
        self.assertEqual(401, submit(first.session, body, cookie))
        self.assertFalse(first.session.authorized)
        current_body: dict[str, str] = login_body(first.session.challenge)
        self.assertEqual(200, submit(first.session, current_body, cookie))
        self.assertTrue(first.session.authorized)
        self.assertEqual("", first.session.challenge)
        self.assertEqual(401, submit(first.session, current_body, cookie))

        self.assertEqual(
            200,
            submit(second.session, login_body(second.challenge["challenge"]), f"RUUVISESSION={second.cookie}"),
        )
        self.assertTrue(second.session.authorized)
        self.assertEqual("", second.session.challenge)

    def test_ro_completion_survives_later_rw_write_failure(self) -> None:
        self.gateway.override[(f"Bearer {RW_KEY}", "POST", "/ruuvi.json")] = FakeResponse(401, {})
        result: RunResult = self.runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])

    def test_prepared_state_failure_marks_setup_error_and_restores(self) -> None:
        self.gateway.config_overrides[2] = self.gateway.config()
        result: RunResult = self.runner().run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes["temporary-state setup"])
        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
        self.assertFalse(any(call.scheme == "Basic" for call in self.gateway.calls))

    def test_identity_is_checked_before_reset_advice(self) -> None:
        mac: Any
        for mac in (None, 123, "invalid", "11:22:33:44:55:66", CONFIG.gw_mac):
            with self.subTest(mac=mac):
                self.gateway = FakeGateway()
                payload: dict[str, Any] = self.gateway.config()
                payload[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = True
                if mac is None:
                    del payload[GatewayCfgDesc.GW_MAC]
                else:
                    payload[GatewayCfgDesc.GW_MAC] = mac
                self.gateway.config_overrides[1] = payload
                result: RunResult = self.runner().run()
                self.assertEqual(2, result.exit_code)
                self.assertEqual(
                    target.FACTORY_RESET_MESSAGE if mac == CONFIG.gw_mac else None, result.recovery_message
                )
                self.assertFalse(any(call.path != "/auth" and call.method == "POST" for call in self.gateway.calls))

    def test_wrong_positive_status_is_fail_for_each_bearer_mechanism(self) -> None:
        cases: tuple[tuple[str, str, str, int], ...] = (
            (RO_KEY, HttpMethod.POST, GatewayApi.CONFIG, HttpStatus.C_200_OK),
            (RW_KEY, HttpMethod.GET, GatewayApi.HISTORY, HttpStatus.C_401_UNAUTHORIZED),
        )
        status: int
        path: str
        method: str
        key: str
        for key, method, path, status in cases:
            with self.subTest(key=key, method=method, path=path):
                gateway: FakeGateway = FakeGateway()
                gateway.override[(f"{HttpAuthScheme.BEARER} {key}", method, path)] = FakeResponse(status, {})
                runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    random_bytes=deterministic_random,
                )
                result: RunResult = runner.run()
                self.assertEqual(1, result.exit_code)
                self.assertEqual("", gateway.ro_key)
                self.assertEqual("", gateway.rw_key)

    def test_restoration_runs_after_exception(self) -> None:
        self.gateway.override[(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.HISTORY)] = requests.ConnectionError(
            "injected"
        )
        result: RunResult = self.runner().run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("", self.gateway.rw_key)
        self.assertEqual(
            "PASS",
            result.outcomes["final restoration and non-mutation"],
        )

    def test_each_restoration_fallback_is_exercised(self) -> None:
        self.gateway.restore_admin_failures = 1
        result: RunResult = self.runner().run()
        self.assertEqual(0, result.exit_code)
        restore_calls: list[RecordedRequest] = [
            call
            for call in self.gateway.calls
            if call.body
            == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            }
        ]
        self.assertEqual(2, len(restore_calls))
        self.assertEqual("", restore_calls[0].authorization)
        self.assertEqual(f"{HttpAuthScheme.BEARER} {RW_KEY}", restore_calls[1].authorization)

        gateway: FakeGateway = FakeGateway()
        gateway.restore_admin_failures = 1
        gateway.restore_rw_failure = True
        deterministic_random.calls = 0
        runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
            random_bytes=deterministic_random,
        )
        result = runner.run()
        self.assertEqual(0, result.exit_code)
        restore_calls = [
            call
            for call in gateway.calls
            if call.body
            == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            }
        ]
        self.assertEqual(3, len(restore_calls))
        self.assertEqual("", restore_calls[-1].authorization)

    def test_unverified_restoration_forces_error_and_recovery_warning(self) -> None:
        self.gateway.restore_admin_failures = 100
        self.gateway.restore_rw_failure = True
        result: RunResult = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual("ERROR", result.outcomes["final restoration and non-mutation"])

    def test_configuration_mutation_is_fail_but_restoration_still_runs(self) -> None:
        original: Callable[[FakeSession, str, str, dict[str, str], Any, bool], FakeResponse] = self.gateway.response_for

        def mutate_after_rw(
            session: FakeSession,
            method: str,
            path: str,
            headers: dict[str, str],
            body: Any,
            allow_redirects: bool,
        ) -> FakeResponse:
            response: FakeResponse = original(session, method, path, headers, body, allow_redirects)
            if (
                method == HttpMethod.POST
                and path == GatewayApi.CONFIG
                and headers.get(HttpHeader.AUTHORIZATION) == f"{HttpAuthScheme.BEARER} {RW_KEY}"
                and body == {}
            ):
                self.gateway.mutate_after_positive = True
            if body == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            }:
                self.gateway.mutate_after_positive = False
            return response

        self.gateway.response_for = mutate_after_rw
        result: RunResult = self.runner().run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])

    def test_setup_transport_and_malformed_json_are_errors(self) -> None:
        mode: str
        for mode in ("timeout", "connection", "json"):
            with self.subTest(mode=mode):
                gateway: FakeGateway = FakeGateway()
                if mode == "timeout":
                    gateway.connection_error = requests.Timeout("timeout")
                elif mode == "connection":
                    gateway.connection_error = requests.ConnectionError("connection")
                else:
                    gateway.malformed_config = True
                runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                )
                self.assertEqual(2, runner.run().exit_code)

    def test_non_default_or_enabled_key_baseline_is_error(self) -> None:
        gateway: FakeGateway = FakeGateway()
        gateway.ro_key = "already-enabled"
        runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
        )
        result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)

        gateway = FakeGateway()
        gateway.auth_user = "NotAdmin"
        runner = target.FunctionalTest_5_1_2a_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
        )
        result = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)


class ConfigurationAndLoggingTestCase(unittest.TestCase):
    @staticmethod
    def write_env(root: Path) -> None:
        (root / ".env").write_text(env_text(), encoding="utf-8")

    def test_missing_env_is_error_and_always_creates_evidence(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_2a_2_b(
                root,
                now=lambda: FIXED_NOW,
                output=messages.append,
            )
            self.assertEqual(2, result.exit_code)
            log: Path = next((root / "logs").iterdir())
            self.assertRegex(log.name, r"^test_5_1_2a_2_b_\d{8}T\d{6}\.\d{6}Z\.log$")
            self.assertIn("InvalidConfig", log.read_text(encoding="utf-8"))
            self.assertEqual("Overall verdict: ERROR", messages[-1])

    def test_execute_progress_and_terminal_output_are_concise(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root)
            gateway: FakeGateway = FakeGateway()
            messages: list[str] = []
            deterministic_random.calls = 0
            result: RunResult = target.execute_test_5_1_2a_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                random_bytes=deterministic_random,
                output=messages.append,
            )
            self.assertEqual(0, result.exit_code)
            progress: list[str] = [line for line in messages if line.startswith("[Step ")]
            self.assertEqual(target.TOTAL_STEPS, len(progress))
            line: str
            index: int
            for index, line in enumerate(progress, 1):
                self.assertTrue(line.startswith(f"[Step {index} out of {target.TOTAL_STEPS}] "))
            self.assertEqual(1, sum(line.startswith("Open log file: ") for line in messages))
            self.assertEqual("Overall verdict: PASS", messages[-1])
            self.assertFalse(any(CONFIG.gw_id in line for line in messages))
            log_text: str = next((root / "logs").iterdir()).read_text(encoding="utf-8")
            self.assertIn("TEMPORARY RO KEY", log_text)
            self.assertIn("OVERALL VERDICT: PASS", log_text)

    def test_basic_and_digest_setup_errors_report_recovery_and_stop(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            directory: str
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root: Path = Path(directory)
                self.write_env(root)
                gateway: FakeGateway = FakeGateway()
                gateway.auth_mode = mode
                messages: list[str] = []
                result: RunResult = target.execute_test_5_1_2a_2_b(
                    root,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    output=messages.append,
                )
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(target.FACTORY_RESET_MESSAGE, messages[-1])
                self.assertEqual(["/auth"], [call.path for call in gateway.calls])
                evidence: str = next((root / "logs").iterdir()).read_text(encoding="utf-8")
                self.assertIn("GatewayAuthenticationModeError", evidence)
                self.assertIn(target.FACTORY_RESET_MESSAGE, evidence)


if __name__ == "__main__":
    unittest.main()
