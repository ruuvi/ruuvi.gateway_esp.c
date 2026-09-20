"""Implementation tests only; these are not ETSI functional-test evidence."""

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

import test_5_1_1_2_b as target
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthChallenge,
)
from lib.http_api import (
    ApiRoute,
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)
from lib.models import RunResult

FIXED_NOW: datetime = datetime(2026, 8, 30, 9, 36, 1, 123456, tzinfo=timezone.utc)
CONFIG: target.DutConfig = target.DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)


def env_line(field: str, value: str) -> str:
    return f"{field}={value}\n"


def env_text(config: target.DutConfig = CONFIG) -> str:
    return (
        f"{env_line(GatewayCfgDesc.GW_ID, config.gw_id)}"
        f"{env_line(GatewayCfgDesc.GW_MAC, config.gw_mac)}"
        f"{env_line(GatewayCfgDesc.GW_HOSTNAME, config.gw_hostname)}"
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
class FakeRequestKey:
    scheme: str
    method: str
    path: str


@dataclass(frozen=True)
class RecordedRequest:
    scheme: str
    method: str
    path: str
    body: Any
    allow_redirects: bool

    @property
    def key(self) -> FakeRequestKey:
        return FakeRequestKey(self.scheme, self.method, self.path)


class FakeGateway:
    def __init__(self) -> None:
        self.calls: list[RecordedRequest] = []
        self.override: dict[FakeRequestKey, FakeResponse | BaseException] = {}
        self.final_override: dict[str, FakeResponse | BaseException] = {}
        self.final_auth_fields: dict[str, Any] = {}
        self.auth_mode: str = GatewayCfgLanAuthType.DEFAULT
        self.auth_user: str = target.ADMIN_USERNAME
        self.ro_enabled: bool = False
        self.rw_enabled: bool = False
        self.final_mutation: bool = False
        self.config_reads: int = 0
        self.challenge_headers: bool = True
        self.connection_error: BaseException | None = None
        self.server_key: ECC.EccKey = ECC.generate(curve="secp256r1")
        server_public: ECC.EccKey = self.server_key.public_key()
        raw: bytes = (
            b"\x04" + int(server_public.pointQ.x).to_bytes(32, "big") + int(server_public.pointQ.y).to_bytes(32, "big")
        )
        self.server_public_b64: str = base64.b64encode(raw).decode("ascii")

    @staticmethod
    def _scheme(headers: dict[str, str]) -> str:
        authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
        return authorization.split(" ", 1)[0] if authorization else "none"

    def response_for(
        self,
        session: FakeSession,
        method: str,
        path: str,
        headers: dict[str, str],
        body: Any,
        allow_redirects: bool,
    ) -> FakeResponse:
        scheme: str = self._scheme(headers)
        self.calls.append(RecordedRequest(scheme, method, path, body, allow_redirects))
        if self.connection_error is not None:
            error: BaseException = self.connection_error
            self.connection_error = None
            raise error
        request_key: FakeRequestKey = FakeRequestKey(scheme, method, path)
        if request_key in self.override:
            override: FakeResponse | BaseException = self.override[request_key]
            if isinstance(override, BaseException):
                raise override
            return override

        if scheme == HttpAuthScheme.BEARER:
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})
        if path == GatewayApi.AUTH and method == HttpMethod.GET:
            if session.authorized:
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                )
            session_id: str = f"session-{session.number}"
            if self.auth_mode == GatewayCfgLanAuthType.BASIC:
                auth_header: str = 'Basic realm="Ruuvi Gateway"'
            elif self.auth_mode == GatewayCfgLanAuthType.DIGEST:
                auth_header = 'Digest realm="Ruuvi Gateway", qop="auth", nonce="nonce", opaque="opaque"'
            else:
                session.challenge_number += 1
                session.challenge = f"challenge-{session.number}-{session.challenge_number}"
                session.cookie = session_id
                auth_header = (
                    f'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="{session.challenge}" '
                    f'session_cookie="RUUVISESSION" session_id="{session_id}"'
                )
            headers_out: dict[str, str] = {HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.server_public_b64}
            if self.challenge_headers:
                headers_out[HttpHeader.WWW_AUTHENTICATE] = auth_header
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                headers=headers_out,
                cookies={"RUUVISESSION": session_id},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.POST:
            default_ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
            expected_body: dict[str, str] = {
                "login": "Admin",
                "password": hashlib.sha256(f"{session.challenge}:{default_ha1}".encode()).hexdigest(),
            }
            if (
                session.challenge
                and body == expected_body
                and headers.get(HttpHeader.COOKIE) == f"RUUVISESSION={session.cookie}"
            ):
                session.authorized = True
                session.challenge = ""
                return FakeResponse(HttpStatus.C_200_OK, {"authenticated": True})
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {"authenticated": False},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.DELETE:
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})
        if session.authorized:
            if self.config_reads and path in self.final_override:
                override = self.final_override[path]
                if isinstance(override, BaseException):
                    raise override
                return override
            if path == GatewayApi.CONFIG:
                self.config_reads += 1
                payload: dict[str, str | bool] = {
                    GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode,
                    GatewayCfgDesc.LAN_AUTH_USER: self.auth_user,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: self.ro_enabled,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: self.rw_enabled,
                    GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
                    GatewayCfgDesc.FW_VER: "test",
                    "stable": "changed" if self.final_mutation and self.config_reads > 1 else "value",
                }
                if self.config_reads > 1:
                    payload.update(self.final_auth_fields)
                return FakeResponse(HttpStatus.C_200_OK, payload)
            if path == GatewayApi.STATUS:
                return FakeResponse(HttpStatus.C_200_OK, {"status": "ok"})
        if method == HttpMethod.GET and path == GatewayApi.INFO:
            return FakeResponse(HttpStatus.C_404_NOT_FOUND)
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
        method: str | None = request.method
        url: str | None = request.url
        if method is None or url is None:
            raise ValueError("prepared request must contain a method and URL")
        body: Any = json.loads(request.body) if request.body else None
        return self.gateway.response_for(
            self,
            method,
            urlsplit(url).path,
            dict(request.headers),
            body,
            kwargs["allow_redirects"],
        )


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        FakeSession.next_number = 1
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root: Path = Path(self.temp_dir.name)
        self.gateway: FakeGateway = FakeGateway()
        self.log: target.EvidenceLog = target.EvidenceLog.create(
            self.root / "logs",
            "test_5_1_1_2_b",
            lambda: FIXED_NOW,
        )
        self.addCleanup(self._close_log)

    def _close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST")

    def make_runner(self) -> target.FunctionalTest_5_1_1_2_b:
        return target.FunctionalTest_5_1_1_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(self.gateway),
            random_bytes=lambda size: bytes(index % 251 for index in range(size)),
        )

    def test_complete_mocked_sequence_passes_and_disables_redirects(self) -> None:
        result: RunResult = self.make_runner().run()
        self.assertEqual(0, result.exit_code)
        self.assertEqual("PASS", result.verdict)
        self.assertEqual(target.EXPECTED_API_INVENTORY, result.coverage)
        self.assertEqual(set(target.MECHANISMS), set(result.outcomes))
        self.assertTrue(all(value == "PASS" for value in result.outcomes.values()))
        content: str = self.log.path.read_text(encoding="utf-8")
        user_defined_pass: str = json.dumps({"mechanism": AuthMech.LAN_WEBUI_USER_DEFINED, "result": "PASS"})
        self.assertIn("PER-MECHANISM RESULT: " + user_defined_pass, content)
        self.assertIn("FINAL RESULT: " + user_defined_pass, content)
        self.assertTrue(self.gateway.calls)
        self.assertTrue(all(call.allow_redirects is False for call in self.gateway.calls))
        first_write: int = next(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.method in {HttpMethod.POST, HttpMethod.DELETE} and call.path != GatewayApi.AUTH
        )
        last_interactive_get: int = max(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.method == HttpMethod.GET and call.scheme in {HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST}
        )
        last_bearer_get: int = max(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.method == HttpMethod.GET and call.scheme == HttpAuthScheme.BEARER
        )
        self.assertLess(last_interactive_get, first_write)
        self.assertLess(last_bearer_get, first_write)

    def test_overall_pass_requires_every_mechanism_result_to_pass(self) -> None:
        mechanism: str
        outcome: str | None
        for mechanism in target.MECHANISMS[:-1]:
            for outcome in (None, "NOT RUN", "FAIL", "ERROR"):
                with self.subTest(mechanism=mechanism, outcome=outcome):
                    self.gateway = FakeGateway()
                    runner: target.FunctionalTest_5_1_1_2_b = self.make_runner()
                    original: Callable[[str], None] = runner._probe_bearer_group

                    def lose_outcome(
                        phase: str,
                        *,
                        original_probe: Callable[[str], None] = original,
                        current_runner: target.FunctionalTest_5_1_1_2_b = runner,
                        tested_mechanism: str = mechanism,
                        injected_outcome: str | None = outcome,
                    ) -> None:
                        original_probe(phase)
                        if phase == target.DANGEROUS_WRITE:
                            if injected_outcome is None:
                                current_runner.outcomes.pop(tested_mechanism)
                            else:
                                current_runner.outcomes[tested_mechanism] = injected_outcome

                    runner._probe_bearer_group = lose_outcome
                    result: RunResult = runner.run()
                    self.assertEqual(
                        (1, "FAIL") if outcome == "FAIL" else (2, "ERROR"),
                        (result.exit_code, result.verdict),
                    )
                    self.assertEqual(outcome, result.outcomes.get(mechanism))
                    self.assertEqual("PASS", result.outcomes["final non-mutation verification"])
                    self.assert_final_verification_requests(self.gateway.calls[-4:])
        self.assertIn(
            '"description": "all required mechanism results are PASS", "result": "FAIL"',
            self.log.path.read_text(encoding="utf-8"),
        )

    def assert_failed_mechanism(self, result: target.RunResult, mechanism: str) -> None:
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn(
            "FINAL RESULT: " + json.dumps({"mechanism": mechanism, "result": "FAIL"}),
            content,
        )

    def assert_final_verification_requests(self, calls: list[RecordedRequest]) -> None:
        self.assertEqual(
            [("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"), ("GET", "/status.json")],
            [(call.method, call.path) for call in calls],
        )
        self.assertTrue(all(call.scheme == "none" for call in calls))
        self.assertEqual("Admin", calls[1].body["login"])
        self.assertTrue(all(call.body is None for call in (calls[0], calls[2], calls[3])))

    def test_rejected_default_login_requires_reset_and_aborts(self) -> None:
        status: int
        for status in (HttpStatus.C_401_UNAUTHORIZED, HttpStatus.C_403_FORBIDDEN):
            with self.subTest(status=status):
                self.gateway = FakeGateway()
                self.gateway.override[FakeRequestKey("none", HttpMethod.POST, GatewayApi.AUTH)] = FakeResponse(status)
                result: RunResult = self.make_runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(
                    [(HttpMethod.GET, GatewayApi.AUTH), (HttpMethod.POST, GatewayApi.AUTH)],
                    [(call.method, call.path) for call in self.gateway.calls],
                )

    def test_incorrect_default_password_response_does_not_authorize(self) -> None:
        wrong_password: str
        for wrong_password in ("", "00:11:22:33:44:55:66:88"):
            with self.subTest(password=wrong_password):
                self.gateway = FakeGateway()
                runner: target.FunctionalTest_5_1_1_2_b = self.make_runner()
                runner.config = target.DutConfig(wrong_password, CONFIG.gw_mac, CONFIG.gw_hostname)
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(0, self.gateway.config_reads)
                self.assertEqual(2, len(self.gateway.calls))

    def test_corrupt_default_password_calculation_does_not_authorize(self) -> None:
        runner: target.FunctionalTest_5_1_1_2_b = self.make_runner()
        with mock.patch.object(GatewayClient, "calculate_digest_ha1", return_value="invalid"):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(0, self.gateway.config_reads)

    def test_login_cookie_and_body_corruption_stop_the_runner(self) -> None:
        fault: str
        for fault in ("missing-cookie", "wrong-cookie", "extra-body-field"):
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
                        if request.method == HttpMethod.POST and urlsplit(request.url).path == GatewayApi.AUTH:
                            if injected_fault == "missing-cookie":
                                request.headers.pop(HttpHeader.COOKIE, None)
                            elif injected_fault == "wrong-cookie":
                                request.headers[HttpHeader.COOKIE] = "RUUVISESSION=another-session"
                            else:
                                body: dict[str, str] = json.loads(request.body)
                                body["extra"] = "unexpected"
                                request.body = json.dumps(body)
                        return super().send(request, **kwargs)

                runner: target.FunctionalTest_5_1_1_2_b = target.FunctionalTest_5_1_1_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda session_type=CorruptLoginSession: session_type(self.gateway),
                )
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual([GatewayApi.AUTH, GatewayApi.AUTH], [call.path for call in self.gateway.calls])
                self.assertEqual(0, self.gateway.config_reads)

    def test_fake_login_requires_current_one_time_session_challenge(self) -> None:
        client: target.GatewayClient = self.make_runner().gateway
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

    def test_inventory_rejections_are_reported_as_failed_coverage(self) -> None:
        inventory: tuple[ApiRoute, ...] = target.API_INVENTORY
        invalid_inventories: tuple[tuple[ApiRoute, ...], ...] = (
            inventory[:-1],
            inventory[:-1] + (inventory[0],),
            inventory[:-1] + (target.ApiRoute(HttpMethod.DELETE, GatewayApi.INFO),),
        )
        invalid: tuple[ApiRoute, ...]
        for invalid in invalid_inventories:
            with self.subTest(inventory=invalid):
                self.gateway = FakeGateway()
                with mock.patch.dict(vars(target), API_INVENTORY=invalid):
                    result: RunResult = self.make_runner().run()
                self.assert_failed_mechanism(result, "complete HTTP API inventory coverage")
                self.assertEqual(7, len(self.gateway.calls))
                self.assert_final_verification_requests(self.gateway.calls[3:])
                self.assertEqual("PASS", result.outcomes["final non-mutation verification"])

    def test_missing_bearer_coverage_is_reported_as_failed_coverage(self) -> None:
        runner: target.FunctionalTest_5_1_1_2_b = self.make_runner()
        runner._probe_routes[target.DANGEROUS_WRITE].pop()
        result: RunResult = runner.run()
        self.assert_failed_mechanism(result, "complete HTTP API inventory coverage")

    def test_baseline_identity_is_checked_before_reset_recommendation(self) -> None:
        mac: str | int | None
        for mac in ("11:22:33:44:55:66", "invalid", 123, None):
            field: str
            for field in target.AUTHENTICATION_DEFAULT_FIELDS:
                with self.subTest(mac=mac, field=field):
                    self.gateway = FakeGateway()
                    payload: dict[str, Any] = target.default_config_values(target.AUTHENTICATION_DEFAULT_FIELDS)
                    payload[field] = "non-default"
                    if mac is not None:
                        payload[GatewayCfgDesc.GW_MAC] = mac
                    self.gateway.override[FakeRequestKey("none", HttpMethod.GET, GatewayApi.CONFIG)] = FakeResponse(
                        HttpStatus.C_200_OK, payload
                    )
                    result: RunResult = self.make_runner().run()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                    self.assertIsNone(result.recovery_message)
                    self.assertEqual(3, len(self.gateway.calls))

    def test_lan_info_404_is_not_allowed_for_other_routes_or_bearer(self) -> None:
        path: str
        scheme: str
        for scheme, path in (
            ("none", GatewayApi.AP),
            (HttpAuthScheme.BASIC, GatewayApi.AP),
            (HttpAuthScheme.DIGEST, GatewayApi.AP),
            (HttpAuthScheme.BEARER, GatewayApi.INFO),
        ):
            with self.subTest(scheme=scheme, path=path):
                self.gateway = FakeGateway()
                key: FakeRequestKey = FakeRequestKey(scheme, HttpMethod.GET, path)
                self.gateway.override[key] = FakeResponse(HttpStatus.C_404_NOT_FOUND)
                result: RunResult = self.make_runner().run()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual(key, self.gateway.calls[-5].key)
                self.assert_final_verification_requests(self.gateway.calls[-4:])

    def test_each_negative_scheme_accepts_only_expected_route_statuses(self) -> None:
        scheme: None | str
        for scheme in (None, HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            with self.subTest(scheme=scheme):
                gateway: FakeGateway = FakeGateway()
                runner: target.FunctionalTest_5_1_1_2_b = target.FunctionalTest_5_1_1_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                    random_bytes=lambda size: b"x" * size,
                )
                runner._probe_interactive_group(HttpMethod.GET, scheme)
                runner._probe_interactive_group(target.SAFE_WRITE, scheme)
                runner._probe_interactive_group(target.DANGEROUS_WRITE, scheme)
                expected_scheme: str = scheme if scheme is not None else "none"
                expected_calls: list[tuple[str, str, str, dict[str, Any] | None]] = [
                    (expected_scheme, route.method, route.path, None)
                    for route in target.API_INVENTORY
                    if route.method == HttpMethod.GET and route.path != GatewayApi.AUTH
                ]
                if scheme is None:
                    expected_calls.append(("none", HttpMethod.DELETE, GatewayApi.AUTH, None))
                expected_calls.extend(
                    (expected_scheme, route.method, route.path, {} if route.method == HttpMethod.POST else None)
                    for route in target.API_INVENTORY
                    if route.method != HttpMethod.GET and route.path != GatewayApi.AUTH
                )
                self.assertEqual(
                    expected_calls,
                    [(call.scheme, call.method, call.path, call.body) for call in gateway.calls],
                )

    def test_bearer_matrix_uses_safe_order_and_least_operative_bodies(self) -> None:
        runner: target.FunctionalTest_5_1_1_2_b = self.make_runner()
        runner._probe_bearer_group(HttpMethod.GET)
        runner._probe_bearer_group(target.SAFE_WRITE)
        self.assertEqual("NOT RUN", runner.outcomes[AuthMech.M2M_API_BEARER_RW])
        runner._probe_bearer_group(target.DANGEROUS_WRITE)
        reads: list[ApiRoute] = [route for route in target.API_INVENTORY if route.method == HttpMethod.GET]
        session_writes: list[ApiRoute] = [
            route for route in target.API_INVENTORY if route.method != HttpMethod.GET and route.path == GatewayApi.AUTH
        ]
        mutating_routes: list[ApiRoute] = [
            route for route in target.API_INVENTORY if route.method != HttpMethod.GET and route.path != GatewayApi.AUTH
        ]
        self.assertEqual(
            [
                (HttpAuthScheme.BEARER, route.method, route.path, {} if route.method == HttpMethod.POST else None)
                for route in reads + session_writes + mutating_routes
            ],
            [(call.scheme, call.method, call.path, call.body) for call in self.gateway.calls],
        )

    def test_all_safe_probes_finish_before_any_potentially_mutating_probe(self) -> None:
        result: RunResult = self.make_runner().run()
        self.assertEqual("PASS", result.verdict)
        first_mutating: int = next(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.method != HttpMethod.GET and call.path != GatewayApi.AUTH
        )
        first_probe: int = next(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.key == FakeRequestKey("none", HttpMethod.GET, GatewayApi.AP)
        )
        expected_safe: list[tuple[str, str, str, dict[str, Any] | None]] = [
            (scheme, route.method, route.path, None)
            for scheme in ("none", HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST, HttpAuthScheme.BEARER)
            for route in target.API_INVENTORY
            if route.method == HttpMethod.GET and (route.path != GatewayApi.AUTH or scheme == HttpAuthScheme.BEARER)
        ]
        expected_safe.extend(
            [
                ("none", HttpMethod.DELETE, GatewayApi.AUTH, None),
                (HttpAuthScheme.BEARER, HttpMethod.POST, GatewayApi.AUTH, {}),
                (HttpAuthScheme.BEARER, HttpMethod.DELETE, GatewayApi.AUTH, None),
            ]
        )
        self.assertEqual(
            expected_safe,
            [
                (call.scheme, call.method, call.path, call.body)
                for call in self.gateway.calls[first_probe:first_mutating]
            ],
        )

    def test_session_write_failure_aborts_before_mutating_probes(self) -> None:
        method: str
        scheme: str
        for scheme, method in (
            ("none", HttpMethod.DELETE),
            (HttpAuthScheme.BEARER, HttpMethod.POST),
            (HttpAuthScheme.BEARER, HttpMethod.DELETE),
        ):
            with self.subTest(scheme=scheme, method=method):
                self.gateway = FakeGateway()
                failing_key: FakeRequestKey = FakeRequestKey(scheme, method, GatewayApi.AUTH)
                self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
                result: RunResult = self.make_runner().run()
                self.assertEqual("FAIL", result.verdict)
                self.assertEqual(failing_key, self.gateway.calls[-5].key)
                self.assert_final_verification_requests(self.gateway.calls[-4:])
                self.assertEqual("PASS", result.outcomes["final non-mutation verification"])
                self.assertEqual(
                    [],
                    [
                        call
                        for call in self.gateway.calls
                        if call.method != HttpMethod.GET and call.path != GatewayApi.AUTH
                    ],
                )

    def test_mutating_probe_success_aborts_immediately(self) -> None:
        scheme: str
        for scheme in ("none", HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST, HttpAuthScheme.BEARER):
            path: str
            for path in (GatewayApi.CONFIG, GatewayApi.FW_UPDATE_RESET, GatewayApi.INIT_STORAGE):
                with self.subTest(scheme=scheme, path=path):
                    self.gateway = FakeGateway()
                    failing_key: FakeRequestKey = FakeRequestKey(scheme, HttpMethod.POST, path)
                    self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
                    result: RunResult = self.make_runner().run()
                    self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                    self.assertEqual(failing_key, self.gateway.calls[-5].key)
                    self.assert_final_verification_requests(self.gateway.calls[-4:])
                    self.assertEqual(2, self.gateway.config_reads)
                    self.assertEqual("PASS", result.outcomes["final non-mutation verification"])

    def test_unexpected_success_aborts_immediately(self) -> None:
        self.gateway.override[FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.AP)] = FakeResponse(
            HttpStatus.C_200_OK,
            {},
        )
        result: RunResult = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.verdict)
        failing_index: int = next(
            index
            for index, call in enumerate(self.gateway.calls)
            if call.key == FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.AP)
        )
        self.assert_final_verification_requests(self.gateway.calls[failing_index + 1 :])
        self.assertEqual("PASS", result.outcomes["final non-mutation verification"])

    def test_mutation_after_failed_probe_is_reported_without_more_probes(self) -> None:
        failing_key: FakeRequestKey = FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG)
        self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
        self.gateway.final_mutation = True
        result: RunResult = self.make_runner().run()
        self.assert_failed_mechanism(result, AuthMech.LAN_WEBUI_BASIC)
        self.assert_failed_mechanism(result, "final non-mutation verification")
        self.assertEqual(failing_key, self.gateway.calls[-5].key)
        self.assert_final_verification_requests(self.gateway.calls[-4:])
        self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message)
        self.assertIn("full canonical configuration hash is unchanged", self.log.path.read_text(encoding="utf-8"))

    def test_final_error_retains_probe_failure_and_attempts_both_final_reads(self) -> None:
        failing_key: FakeRequestKey = FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG)
        self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
        self.gateway.final_override[GatewayApi.CONFIG] = requests.Timeout("final config timeout")
        self.gateway.final_override[GatewayApi.STATUS] = FakeResponse(HttpStatus.C_403_FORBIDDEN)
        result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_BASIC])
        self.assertEqual("ERROR", result.outcomes["final non-mutation verification"])
        self.assertEqual(failing_key, self.gateway.calls[-5].key)
        self.assert_final_verification_requests(self.gateway.calls[-4:])
        self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message)
        content: str = self.log.path.read_text(encoding="utf-8")
        self.assertIn("final config timeout", content)
        self.assertIn("gateway still answers authenticated GET /status.json", content)
        self.assertIn(json.dumps({"mechanism": AuthMech.LAN_WEBUI_BASIC, "result": "FAIL"}), content)

    def test_probe_transport_error_still_runs_final_verification(self) -> None:
        failing_key: FakeRequestKey = FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG)
        self.gateway.override[failing_key] = requests.Timeout("probe may have reached the DUT")
        self.gateway.final_mutation = True
        result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes["final non-mutation verification"])
        self.assertEqual(failing_key, self.gateway.calls[-5].key)
        self.assert_final_verification_requests(self.gateway.calls[-4:])
        self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message)

    def test_failed_final_login_preserves_probe_failure_and_reports_unverified_state(self) -> None:
        failing_key: FakeRequestKey = FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG)
        self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
        original: Callable[[FakeSession, str, str, dict[str, str], Any, bool], FakeResponse] = self.gateway.response_for

        def fail_final_login(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any, allow_redirects: bool
        ) -> FakeResponse:
            response: FakeResponse = original(session, method, path, headers, body, allow_redirects)
            if self.gateway.calls[-1].key == failing_key:
                self.gateway.override[FakeRequestKey("none", HttpMethod.POST, GatewayApi.AUTH)] = FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED
                )
            return response

        self.gateway.response_for = fail_final_login
        result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_BASIC])
        self.assertEqual("ERROR", result.outcomes["final non-mutation verification"])
        self.assertEqual(failing_key, self.gateway.calls[-3].key)
        self.assertEqual(
            [("GET", "/auth"), ("POST", "/auth")],
            [(call.method, call.path) for call in self.gateway.calls[-2:]],
        )
        self.assertEqual(1, self.gateway.config_reads)
        self.assertIn(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message or "")

    def test_wrong_bearer_status_fails_and_aborts(self) -> None:
        self.gateway.override[FakeRequestKey(HttpAuthScheme.BEARER, HttpMethod.GET, GatewayApi.STATUS)] = FakeResponse(
            HttpStatus.C_403_FORBIDDEN, {}
        )
        result: RunResult = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual(
            FakeRequestKey(HttpAuthScheme.BEARER, HttpMethod.GET, GatewayApi.STATUS),
            self.gateway.calls[-5].key,
        )
        self.assert_final_verification_requests(self.gateway.calls[-4:])

    def test_non_default_modes_are_setup_errors_requiring_factory_reset(self) -> None:
        auth_mode: str
        for auth_mode in (
            GatewayCfgLanAuthType.BASIC,
            GatewayCfgLanAuthType.DIGEST,
        ):
            with self.subTest(auth_mode=auth_mode):
                gateway: FakeGateway = FakeGateway()
                gateway.auth_mode = auth_mode
                runner: target.FunctionalTest_5_1_1_2_b = target.FunctionalTest_5_1_1_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda fixture=gateway: FakeSession(fixture),
                )
                result: RunResult = runner.run()
                self.assertEqual(2, result.exit_code)
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual(
                    target.FACTORY_RESET_MESSAGE,
                    result.recovery_message,
                )

    def test_enabled_api_key_flags_are_setup_errors(self) -> None:
        attribute: str
        for attribute in ("ro_enabled", "rw_enabled"):
            with self.subTest(attribute=attribute):
                gateway: FakeGateway = FakeGateway()
                setattr(gateway, attribute, True)
                runner: target.FunctionalTest_5_1_1_2_b = target.FunctionalTest_5_1_1_2_b(
                    CONFIG, self.log, session_factory=lambda fixture=gateway: FakeSession(fixture)
                )
                result: RunResult = runner.run()
                self.assertEqual(2, result.exit_code)

    def test_non_default_auth_user_requires_factory_reset(self) -> None:
        self.gateway.auth_user = "NotAdmin"
        result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)

    def test_timeout_and_connection_failure_are_errors(self) -> None:
        error: BaseException
        for error in (
            requests.Timeout("timeout"),
            requests.ConnectionError("connection"),
        ):
            with self.subTest(error=type(error).__name__):
                gateway: FakeGateway = FakeGateway()
                gateway.connection_error = error
                runner: target.FunctionalTest_5_1_1_2_b = target.FunctionalTest_5_1_1_2_b(
                    CONFIG, self.log, session_factory=lambda fixture=gateway: FakeSession(fixture)
                )
                self.assertEqual(2, runner.run().exit_code)

    def test_malformed_json_is_error(self) -> None:
        self.gateway.override[FakeRequestKey("none", HttpMethod.GET, GatewayApi.AUTH)] = FakeResponse(
            HttpStatus.C_401_UNAUTHORIZED,
            malformed_json=True,
            headers={
                HttpHeader.WWW_AUTHENTICATE: (
                    'x-ruuvi-interactive realm="r", challenge="c", session_cookie="RUUVISESSION", session_id="s"'
                ),
                HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.gateway.server_public_b64,
            },
            cookies={"RUUVISESSION": "s"},
        )
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_missing_auth_header_is_error(self) -> None:
        self.gateway.challenge_headers = False
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_appended_basic_challenge_is_error(self) -> None:
        response: FakeResponse = self.gateway.response_for(
            FakeSession(self.gateway),
            HttpMethod.GET,
            GatewayApi.AUTH,
            {HttpHeader.RUUVI_ECDH_PUBLIC_KEY: "key"},
            None,
            False,
        )
        response.headers[HttpHeader.WWW_AUTHENTICATE] = (
            f'{response.headers[HttpHeader.WWW_AUTHENTICATE]}, {HttpAuthScheme.BASIC} realm="unexpected"'
        )
        self.gateway.override[FakeRequestKey("none", HttpMethod.GET, GatewayApi.AUTH)] = response
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_delete_auth_failure_is_attributed_to_user_defined_mechanism(self) -> None:
        self.gateway.override[FakeRequestKey("none", HttpMethod.DELETE, GatewayApi.AUTH)] = FakeResponse(
            HttpStatus.C_200_OK, {}
        )
        result: RunResult = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_USER_DEFINED])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.LAN_WEBUI_DIGEST])

    def test_final_state_mismatch_fails(self) -> None:
        self.gateway.final_mutation = True
        result: RunResult = self.make_runner().run()
        self.assert_failed_mechanism(result, "final non-mutation verification")

    def test_final_authentication_field_mismatch_is_reported_as_fail(self) -> None:
        self.gateway.final_auth_fields[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = True
        result: RunResult = self.make_runner().run()
        self.assert_failed_mechanism(result, "final non-mutation verification")

    def test_final_http_status_failures_are_reported_as_fail(self) -> None:
        path: str
        for path in (GatewayApi.CONFIG, GatewayApi.STATUS):
            with self.subTest(path=path):
                self.gateway = FakeGateway()
                self.gateway.final_override[path] = FakeResponse(HttpStatus.C_403_FORBIDDEN)
                result: RunResult = self.make_runner().run()
                self.assert_failed_mechanism(result, "final non-mutation verification")
                self.assert_final_verification_requests(self.gateway.calls[-4:])
                self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message)

    def test_final_transport_and_protocol_errors_remain_errors(self) -> None:
        error: BaseException
        for error in (
            requests.Timeout("final timeout"),
            requests.ConnectionError("final connection"),
            FakeResponse(HttpStatus.C_200_OK, malformed_json=True),
        ):
            with self.subTest(error=error):
                self.gateway = FakeGateway()
                self.gateway.final_override[GatewayApi.CONFIG] = error
                result: RunResult = self.make_runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes["final non-mutation verification"])
                self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, result.recovery_message)
                self.assert_final_verification_requests(self.gateway.calls[-4:])


class ConfigurationAndLoggingTestCase(unittest.TestCase):
    @staticmethod
    def write_env(root: Path, text: str) -> Path:
        path: Path = root / ".env"
        path.write_text(text, encoding="utf-8")
        return path

    def test_execute_reports_failed_default_login_recovery_in_output_and_log(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root, env_text())
            gateway: FakeGateway = FakeGateway()
            gateway.override[FakeRequestKey("none", HttpMethod.POST, GatewayApi.AUTH)] = FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED
            )
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_1_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                output=messages.append,
            )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
            self.assertEqual(target.FACTORY_RESET_MESSAGE, messages[-1])
            content: str = next((root / "logs").iterdir()).read_text(encoding="utf-8")
            self.assertIn("USER ACTION REQUIRED", content)
            self.assertIn(target.FACTORY_RESET_MESSAGE, content)
            self.assertIn("OVERALL VERDICT: ERROR", content)

    def test_execute_maps_configuration_error_to_exit_code_2_and_logs_it(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            logs: Path = root / "logs"
            with mock.patch("builtins.print"):
                result: RunResult = target.execute_test_5_1_1_2_b(root, now=lambda: FIXED_NOW)
            self.assertEqual(2, result.exit_code)
            content: str = next(logs.iterdir()).read_text(encoding="utf-8")
            self.assertIn("InvalidConfig", content)
            self.assertIn("OVERALL VERDICT: ERROR", content)

    def test_execute_reports_probe_failure_and_failed_final_verification(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root, env_text())
            gateway: FakeGateway = FakeGateway()
            gateway.override[FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.POST, GatewayApi.CONFIG)] = FakeResponse(
                HttpStatus.C_200_OK, {}
            )
            gateway.final_mutation = True
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_1_2_b(
                root, session_factory=lambda: FakeSession(gateway), now=lambda: FIXED_NOW, output=messages.append
            )
            self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
            self.assertEqual(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, messages[-1])
            content: str = next((root / "logs").iterdir()).read_text(encoding="utf-8")
            self.assertIn("USER ACTION REQUIRED", content)
            self.assertIn(target.FINAL_VERIFICATION_RECOVERY_MESSAGE, content)
            self.assertIn(json.dumps({"mechanism": AuthMech.LAN_WEBUI_BASIC, "result": "FAIL"}), content)
            self.assertIn(json.dumps({"mechanism": "final non-mutation verification", "result": "FAIL"}), content)
            self.assertIn("OVERALL VERDICT: FAIL", content)

    def test_execute_uses_current_directory_by_default(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            with mock.patch.object(target.Path, "cwd", return_value=root), mock.patch("builtins.print"):
                result: RunResult = target.execute_test_5_1_1_2_b(now=lambda: FIXED_NOW)
            self.assertEqual(2, result.exit_code)
            self.assertTrue((root / "logs").is_dir())

    def test_execute_prints_log_path_once_and_reports_all_progress_steps(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(
                root,
                env_text(),
            )
            gateway: FakeGateway = FakeGateway()
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_1_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                output=messages.append,
            )
            self.assertEqual(0, result.exit_code)
            log_path: str = str(next((root / "logs").iterdir()))
            self.assertEqual(1, sum(log_path in message for message in messages))
            progress: list[str] = [message for message in messages if message.startswith("[Step ")]
            self.assertEqual(target.TOTAL_STEPS, len(progress))
            log_lines: list[str] = Path(log_path).read_text(encoding="utf-8").splitlines()
            message: str
            index: int
            for index, message in enumerate(progress, 1):
                self.assertTrue(message.startswith(f"[Step {index} out of {target.TOTAL_STEPS}] "))
                matching_log_lines: list[str] = [line for line in log_lines if line.endswith(message)]
                self.assertEqual(1, len(matching_log_lines))
                self.assertRegex(
                    matching_log_lines[0],
                    r"^\[\d{4}-\d{2}-\d{2}T.*Z\] \[Step ",
                )
            self.assertEqual("Overall verdict: PASS", messages[-1])


if __name__ == "__main__":
    unittest.main()
