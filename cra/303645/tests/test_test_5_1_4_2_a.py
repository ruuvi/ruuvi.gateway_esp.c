"""Implementation tests only; these are not ETSI functional-test evidence."""

from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable
from unittest import mock

import requests

from lib.models import RunResult

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_5_1_4_2_a as target
import test_test_5_1_5_2_b as wire
from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType, InteractiveAuthResult
from lib.http_api import HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus

CONFIG: target.DutConfig = target.DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 6, tzinfo=timezone.utc)


class WireGateway(wire.FakeGateway):
    """Use the independent HTTP login fixture with partial credential updates."""

    def _apply_config(self, body: dict[str, str]) -> None:
        old_auth: tuple[str, str, str] = (self.mode, self.custom_username, self.custom_ha1)
        self.mode = body.get(GatewayCfgDesc.LAN_AUTH_TYPE, self.mode)
        self.custom_username = body.get(GatewayCfgDesc.LAN_AUTH_USER, self.custom_username)
        self.custom_ha1 = body.get(GatewayCfgDesc.LAN_AUTH_PASS, self.custom_ha1)
        self.ro_key = body.get(GatewayCfgDesc.LAN_AUTH_API_KEY, self.ro_key)
        self.rw_key = body.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW, self.rw_key)
        if old_auth != (self.mode, self.custom_username, self.custom_ha1):
            session: wire.FakeSession
            for session in self.authorized_sessions:
                session.authorized = False
            self.authorized_sessions.clear()


class Response(requests.Response):
    def __init__(self, status: int, payload: dict[str, Any] | None = None) -> None:
        super().__init__()
        self.status_code: int = status
        self.payload: dict[str, Any] = payload or {}

    def json(self, **kwargs: Any) -> dict[str, Any]:
        return self.payload


class FakeSession(requests.Session):
    def __init__(self, username: str | None = None) -> None:
        super().__init__()
        self.username: str | None = username

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> requests.Response:
        raise AssertionError("orchestration fixture must not send HTTP requests")


class FakeGatewayClient:
    """Protocol-boundary fixture with independently defined credentials and state."""

    def __init__(self) -> None:
        self.user: str = target.ADMIN_USERNAME
        self.password: str = CONFIG.gw_id
        self.mode: str = GatewayCfgLanAuthType.DEFAULT
        self.ro_key: str = ""
        self.rw_key: str = ""
        self.calls: list[dict[str, Any]] = []
        self.counter: int = 0
        self.fail_post: bool = False
        self.fail_restore: bool = False

    def random_text(self, size: int = 18) -> str:
        self.counter += 1
        return f"value{self.counter:02d}".ljust(size, "x")

    @staticmethod
    def calculate_digest_ha1(username: str, realm: str, password: str) -> str:
        return hashlib.md5(f"{username}:{realm}:{password}".encode()).hexdigest()

    @staticmethod
    def response_json(response: Response, context: str, expected_type: Any) -> dict[str, Any]:
        if not isinstance(response.payload, expected_type):
            raise target.InvalidSetup(context)
        return response.json()

    @staticmethod
    def new_session() -> FakeSession:
        return FakeSession()

    def authenticate_interactive(self, username: str, password: str) -> InteractiveAuthResult:
        if self.mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            raise target.GatewayAuthenticationModeError("fixture uses Basic/Digest")
        expected_password: str = (
            self.calculate_digest_ha1(username, "captured live realm", password)
            if self.mode == GatewayCfgLanAuthType.RUUVI
            else password
        )
        success: bool = username == self.user and expected_password == self.password
        return InteractiveAuthResult(
            session=FakeSession(username if success else None),
            challenge_response=Response(HttpStatus.C_401_UNAUTHORIZED),
            auth_payload={GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
            challenge={"realm": "captured live realm"},
            auth_header="",
            cookie="",
            gateway_public_key_raw=b"",
            aes_key=b"",
            login_response=Response(HttpStatus.C_200_OK if success else HttpStatus.C_401_UNAUTHORIZED),
        )

    def request(
        self,
        session: requests.Session,
        method: str,
        path: str,
        headers: dict[str, str] | None = None,
        json_body: dict[str, str] | None = None,
    ) -> Response:
        body: dict[str, str] | None = json_body if json_body is not None else None
        self.calls.append({"session": session, "method": method, "path": path, "headers": headers, "body": body})
        authorization: str = (headers or {}).get(HttpHeader.AUTHORIZATION, "")
        prefix: str = HttpAuthScheme.BEARER + " "
        bearer: str = authorization[len(prefix) :] if authorization.startswith(prefix) else ""
        authorized: bool = (isinstance(session, FakeSession) and session.username == self.user) or (
            bool(bearer) and bearer == self.rw_key
        )
        if path == GatewayApi.HISTORY:
            return Response(HttpStatus.C_200_OK if bearer == self.ro_key else HttpStatus.C_401_UNAUTHORIZED)
        if path != GatewayApi.CONFIG or not authorized:
            return Response(HttpStatus.C_401_UNAUTHORIZED)
        if method == HttpMethod.GET:
            return Response(
                HttpStatus.C_200_OK,
                {
                    GatewayCfgDesc.LAN_AUTH_TYPE: self.mode,
                    GatewayCfgDesc.LAN_AUTH_USER: self.user,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: bool(self.ro_key),
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: bool(self.rw_key),
                    GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
                    GatewayCfgDesc.FW_VER: "fixture",
                },
            )
        if self.fail_post:
            self.fail_post = False
            return Response(500)
        if self.fail_restore and body and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT:
            return Response(500)
        if body is not None:
            if body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT:
                self.mode, self.user, self.password, self.ro_key, self.rw_key = (
                    GatewayCfgLanAuthType.DEFAULT,
                    target.ADMIN_USERNAME,
                    CONFIG.gw_id,
                    "",
                    "",
                )
            elif GatewayCfgDesc.LAN_AUTH_USER in body:
                self.mode = GatewayCfgLanAuthType.RUUVI
                self.user = body[GatewayCfgDesc.LAN_AUTH_USER]
                self.password = body[GatewayCfgDesc.LAN_AUTH_PASS]
            if GatewayCfgDesc.LAN_AUTH_API_KEY in body:
                self.ro_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY]
                self.rw_key = body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW]
        return Response(HttpStatus.C_200_OK)


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.log: target.EvidenceLog = target.EvidenceLog.create(Path(self.temp_dir.name) / "logs", "test", lambda: NOW)
        self.addCleanup(self._close_log)
        self.fake: FakeGatewayClient = FakeGatewayClient()
        self.runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(CONFIG, self.log)
        gateway_patch: mock._patch = mock.patch.object(self.runner, "gateway", self.fake)
        gateway_patch.start()
        self.addCleanup(gateway_patch.stop)

    def _close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST", lambda: NOW)

    def test_full_sequence_uses_exact_mutations_and_restores(self) -> None:
        result: RunResult = self.runner.run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        bodies: list[dict[str, Any]] = [call["body"] for call in self.fake.calls if call["method"] == HttpMethod.POST]
        self.assertEqual(GatewayCfgLanAuthType.RUUVI, bodies[0][GatewayCfgDesc.LAN_AUTH_TYPE])
        self.assertEqual(GatewayCfgLanAuthType.RUUVI, bodies[1][GatewayCfgDesc.LAN_AUTH_TYPE])
        self.assertEqual({GatewayCfgDesc.LAN_AUTH_API_KEY, GatewayCfgDesc.LAN_AUTH_API_KEY_RW}, set(bodies[2]))
        self.assertEqual({GatewayCfgDesc.LAN_AUTH_API_KEY, GatewayCfgDesc.LAN_AUTH_API_KEY_RW}, set(bodies[4]))
        self.assertEqual({}, bodies[3])
        self.assertEqual({}, bodies[5])
        self.assertEqual(
            {
                GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            },
            bodies[-1],
        )
        self.assertTrue(all(value == "PASS" for value in result.outcomes.values()))

    def test_failed_change_is_fail_and_still_restores(self) -> None:
        self.fake.fail_post = True
        result: RunResult = self.runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_failed_restoration_prevents_pass(self) -> None:
        self.fake.fail_restore = True
        result: RunResult = self.runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes["final restoration"])
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)

    def test_full_run_uses_real_client_and_independent_wire_login_validation(self) -> None:
        gateway: WireGateway = WireGateway()
        runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(
            CONFIG,
            self.log,
            session_factory=lambda: wire.FakeSession(gateway),
            random_bytes=wire.UniqueRandom(),
        )
        result: RunResult = runner.run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual(7, len(gateway.config_bodies))
        self.assertEqual([{}, {}], [body for body in gateway.config_bodies if not body])
        self.assertEqual(
            [
                ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
                ("POST", "/ruuvi.json"),
                ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
                ("GET", "/auth"), ("POST", "/auth"),
                ("POST", "/ruuvi.json"),
                ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
                ("POST", "/ruuvi.json"), ("GET", "/ruuvi.json"),
                ("GET", "/history"), ("POST", "/ruuvi.json"), ("GET", "/ruuvi.json"),
                ("POST", "/ruuvi.json"), ("GET", "/ruuvi.json"),
                ("GET", "/history"), ("POST", "/ruuvi.json"), ("GET", "/ruuvi.json"),
                ("POST", "/ruuvi.json"),
                ("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json"),
            ],
            [(call.method, call.path) for call in gateway.calls],
        )
        bearer_calls: list[wire.RecordedRequest] = [call for call in gateway.calls if call.bearer_token]
        self.assertEqual([None, {}, None, {}], [call.body for call in bearer_calls])
        self.assertEqual(4, len({call.session_number for call in bearer_calls}))
        self.assertTrue(all(call.cookie is None for call in bearer_calls))

    def test_successful_login_checks_identity_before_nondefault_auth_mode(self) -> None:
        mac: Any
        for mac in (None, 42, "bad", "11:22:33:44:55:66", CONFIG.gw_mac):
            with self.subTest(mac=mac):
                gateway: WireGateway = WireGateway()
                original: Callable[..., wire.FakeResponse] = gateway.response_for

                def response(
                    session: wire.FakeSession, method: str, path: str, headers: dict[str, str], body: Any,
                    original_response: Callable[..., wire.FakeResponse] = original, identity_mac: Any = mac,
                ) -> wire.FakeResponse:
                    reply: wire.FakeResponse = original_response(session, method, path, headers, body)
                    if method == HttpMethod.GET and path == GatewayApi.AUTH:
                        reply._payload[GatewayCfgDesc.LAN_AUTH_TYPE] = GatewayCfgLanAuthType.RUUVI
                    if method == HttpMethod.GET and path == GatewayApi.CONFIG:
                        reply._payload.pop(GatewayCfgDesc.GW_MAC, None)
                        if identity_mac is not None:
                            reply._payload[GatewayCfgDesc.GW_MAC] = identity_mac
                    return reply

                runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(
                    CONFIG, self.log, session_factory=lambda fixture=gateway: wire.FakeSession(fixture)
                )
                with mock.patch.object(wire.FakeGateway, "response_for", side_effect=response):
                    result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE if mac == CONFIG.gw_mac else None, result.recovery_message)
                self.assertEqual([GatewayApi.AUTH, GatewayApi.AUTH, GatewayApi.CONFIG], [c.path for c in gateway.calls])
                self.assertEqual([], gateway.config_bodies)

    def test_readback_mode_must_match_each_custom_transition(self) -> None:
        stage: int
        for stage in (1, 2):
            with self.subTest(stage=stage):
                fake: FakeGatewayClient = FakeGatewayClient()
                original: Callable[..., Response] = fake.request
                reads: int = 0

                def request(
                    *args: Any, original_request: Callable[..., Response] = original,
                    fixture: FakeGatewayClient = fake, transition: int = stage, **kwargs: Any,
                ) -> Response:
                    nonlocal reads
                    reply: Response = original_request(*args, **kwargs)
                    if args[1:3] == (HttpMethod.GET, GatewayApi.CONFIG) and fixture.mode == GatewayCfgLanAuthType.RUUVI:
                        reads += 1
                        if reads == transition:
                            reply.payload[GatewayCfgDesc.LAN_AUTH_TYPE] = GatewayCfgLanAuthType.DEFAULT
                    return reply

                runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(CONFIG, self.log)
                with mock.patch.object(runner, "gateway", fake), mock.patch.object(fake, "request", side_effect=request):
                    result: RunResult = runner.run()
                mechanism: str = target.MECHANISMS[stage - 1]
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[mechanism])
                self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_missing_mechanism_pass_cannot_produce_overall_pass(self) -> None:
        verify: Callable[..., None] = self.runner._verify_bearer

        def omit_ro_result(*args: Any, **kwargs: Any) -> None:
            verify(*args, **kwargs)
            self.runner.outcomes[target.AuthMech.M2M_API_BEARER_RO] = "NOT RUN"

        with mock.patch.object(self.runner, "_verify_bearer", side_effect=omit_ro_result):
            result: RunResult = self.runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_corrupt_default_login_stops_before_mutation(self) -> None:
        gateway: WireGateway = WireGateway()
        runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(
            CONFIG,
            self.log,
            session_factory=lambda: wire.FakeSession(gateway),
        )
        with mock.patch.object(target.GatewayClient, "calculate_digest_ha1", return_value="corrupt"):
            result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual([], gateway.config_bodies)

    def test_basic_and_digest_setup_errors_preserve_evidence_and_output(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                root: Path = Path(self.temp_dir.name)
                (root / ".env").write_text(wire.env_text(), encoding="utf-8")
                gateway: WireGateway = WireGateway()
                gateway.mode = mode
                messages: list[str] = []
                result: RunResult = target.execute_test_5_1_4_2_a(
                    root,
                    session_factory=lambda fixture=gateway: wire.FakeSession(fixture),
                    output=messages.append,
                )
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(target.FACTORY_RESET_MESSAGE, messages[-1])
                self.assertEqual([GatewayApi.AUTH], [call.path for call in gateway.calls])
                path: Path = Path(messages[0][len("Open log file: ") :])
                self.assertIn("GatewayAuthenticationModeError", path.read_text(encoding="utf-8"))
                self.assertIn(target.FACTORY_RESET_MESSAGE, path.read_text(encoding="utf-8"))

    def test_rw_failure_does_not_get_attributed_to_ro(self) -> None:
        original: Callable[..., Response] = self.fake.request

        def request(*args: Any, **kwargs: Any) -> Response:
            if args[1] == HttpMethod.POST and kwargs.get("json_body") == {}:
                return Response(401)
            return original(*args, **kwargs)

        with mock.patch.object(self.fake, "request", side_effect=request):
            result: RunResult = self.runner.run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("NOT RUN", result.outcomes[target.AuthMech.M2M_API_BEARER_RO])
        self.assertIn(
            '"mechanism": "AuthMech-M2M-API-Bearer-RW", "result": "FAIL"',
            self.log.path.read_text(encoding="utf-8"),
        )

    def test_completed_ro_survives_final_rw_hash_failure(self) -> None:
        original: Callable[..., Response] = self.fake.request
        noop_count: int = 0

        def request(*args: Any, **kwargs: Any) -> Response:
            nonlocal noop_count
            response: Response = original(*args, **kwargs)
            if args[1] == HttpMethod.POST and kwargs.get("json_body") == {}:
                noop_count += 1
            if args[1] == HttpMethod.GET and args[2] == GatewayApi.CONFIG and noop_count == 2:
                response.payload["unexpected"] = True
                noop_count += 1
            return response

        with mock.patch.object(self.fake, "request", side_effect=request):
            result: RunResult = self.runner.run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("PASS", result.outcomes[target.AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("FAIL", result.outcomes[target.AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_partial_credential_transition_retains_recovery_candidates(self) -> None:
        stage: int
        for stage in (1, 2):
            applied: bool
            for applied in (False, True):
                with self.subTest(stage=stage, applied=applied):
                    self.check_partial_credential_transition(stage, applied)

    def check_partial_credential_transition(self, stage: int, applied: bool) -> None:
        fake: FakeGatewayClient = FakeGatewayClient()
        runner: target.FunctionalTest_5_1_4_2_a = target.FunctionalTest_5_1_4_2_a(CONFIG, self.log)
        original: Callable[..., Response] = fake.request
        changes: list[dict[str, str]] = []

        def request(*args: Any, **kwargs: Any) -> Response:
            body: dict[str, str] | None = kwargs.get("json_body")
            if body and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI:
                changes.append(body)
                if len(changes) == stage:
                    if applied:
                        original(*args, **kwargs)
                    raise wire.requests.Timeout("credential write response lost")
            return original(*args, **kwargs)

        with mock.patch.object(runner, "gateway", fake), mock.patch.object(fake, "request", side_effect=request):
            result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, fake.mode)

    def test_unverified_identity_never_gets_reset_advice(self) -> None:
        mac: Any
        for mac in (None, 42, "bad", "11:22:33:44:55:66", CONFIG.gw_mac):
            with self.subTest(mac=mac):
                self.runner.factory_reset_required = False
                payload: dict[str, Any] = {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
                    GatewayCfgDesc.LAN_AUTH_USER: "NotAdmin",
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
                }
                if mac is not None:
                    payload[GatewayCfgDesc.GW_MAC] = mac
                with self.assertRaises(target.InvalidSetup):
                    self.runner._validate_baseline(payload)
                self.assertEqual(mac == CONFIG.gw_mac, self.runner.factory_reset_required)


if __name__ == "__main__":
    unittest.main()
