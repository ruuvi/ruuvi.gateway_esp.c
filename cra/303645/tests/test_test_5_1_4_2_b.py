"""Offline implementation verification for Unit B; never live compliance evidence."""

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

import requests
from Crypto.PublicKey import ECC

import test_5_1_4_2_b as target
from lib.gateway import AuthMech, GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import GatewayApi, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import FakeGateway, FakeResponse, FakeSession, RecordedRequest, UniqueRandom, env_text

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 21, tzinfo=timezone.utc)
REALM: str = "Unit B live fixture realm"


def generated(number: int, size: int = 32) -> str:
    return base64.urlsafe_b64encode(bytes(range(number, number + size))).decode("ascii").rstrip("=")


@dataclass(frozen=True)
class ExpectedCall:
    label: str
    request: RecordedRequest


@dataclass(frozen=True)
class Fault:
    request_number: int
    response: FakeResponse | None = None
    error: requests.RequestException | None = None
    apply: bool = False


def expected_calls() -> list[ExpectedCall]:
    """Independent procedure specification, including wire bodies and session allocation."""
    calls: list[ExpectedCall] = []

    def add(
        label: str, method: str, path: str, session: int,
        body: Any = None, token: str | None = None, cookie: bool = True,
    ) -> None:
        calls.append(ExpectedCall(label, RecordedRequest(
            method, path, token, body, session,
            f"Bearer {token}" if token else "",
            f"RUUVISESSION=session-{session}-1" if cookie else None, False,
        )))

    def login(label: str, session: int, username: str, password: str) -> None:
        ha1: str = hashlib.md5(f"{username}:{REALM}:{password}".encode()).hexdigest()
        response: str = hashlib.sha256(f"challenge-{session}-1:{ha1}".encode()).hexdigest()
        add(f"{label} challenge", HttpMethod.GET, GatewayApi.AUTH, session, cookie=False)
        add(f"{label} login", HttpMethod.POST, GatewayApi.AUTH, session, {"login": username, "password": response})

    def config(label: str, session: int) -> None:
        add(label, HttpMethod.GET, GatewayApi.CONFIG, session)

    def bearer(label: str, session: int, token: str, *, write: bool = False, admin: int = 5) -> None:
        add(label, HttpMethod.POST if write else HttpMethod.GET,
            GatewayApi.CONFIG if write else GatewayApi.HISTORY, session,
            {} if write else None, token, cookie=False)
        if write:
            config(f"{label} hash", admin)

    user1: str = generated(1, 12)
    password1: str = generated(2)
    user2: str = generated(3, 12)
    password2: str = generated(4)
    login("default", 1, "Admin", CONFIG.gw_id)
    config("baseline", 1)
    config("stable baseline", 1)
    add("default change", HttpMethod.POST, GatewayApi.CONFIG, 1, {
        "lan_auth_type": "lan_auth_ruuvi", "lan_auth_user": user1,
        "lan_auth_pass": hashlib.md5(f"{user1}:{REALM}:{password1}".encode()).hexdigest(),
    })
    config("default stale session", 1)
    login("old default", 2, "Admin", CONFIG.gw_id)
    login("new custom1", 3, user1, password1)
    config("custom1 config", 3)
    add("custom change", HttpMethod.POST, GatewayApi.CONFIG, 3, {
        "lan_auth_type": "lan_auth_ruuvi", "lan_auth_user": user2,
        "lan_auth_pass": hashlib.md5(f"{user2}:{REALM}:{password2}".encode()).hexdigest(),
    })
    config("custom stale session", 3)
    login("old custom1", 4, user1, password1)
    login("new custom2", 5, user2, password2)
    config("custom2 config", 5)
    add("provision keys", HttpMethod.POST, GatewayApi.CONFIG, 5,
        {"lan_auth_api_key": generated(5), "lan_auth_api_key_rw": generated(6)})
    config("initial surviving session", 5)
    config("initial key config", 5)
    bearer("initial RO read", 6, generated(5))
    bearer("initial RW read", 7, generated(6))
    bearer("initial RW write", 8, generated(6), write=True)
    add("rotate RO", HttpMethod.POST, GatewayApi.CONFIG, 5, {"lan_auth_api_key": generated(7)})
    config("RO surviving session", 5)
    config("RO config", 5)
    bearer("old RO", 9, generated(5))
    bearer("new RO", 10, generated(7))
    bearer("unaffected RW read", 11, generated(6))
    bearer("unaffected RW write", 12, generated(6), write=True)
    add("rotate RW", HttpMethod.POST, GatewayApi.CONFIG, 5, {"lan_auth_api_key_rw": generated(8)})
    config("RW surviving session", 5)
    config("RW config", 5)
    bearer("unaffected RO read", 13, generated(7))
    bearer("new RW read", 14, generated(8))
    bearer("old RW write", 15, generated(6), write=True)
    bearer("new RW write", 16, generated(8), write=True)
    bearer("RO scope", 17, generated(7), write=True)
    add("restore", HttpMethod.POST, GatewayApi.CONFIG, 5,
        {"lan_auth_type": "lan_auth_default", "lan_auth_api_key": "", "lan_auth_api_key_rw": ""})
    login("restored default", 18, "Admin", CONFIG.gw_id)
    config("restored config", 18)
    login("revoked custom1", 19, user1, password1)
    login("revoked custom2", 20, user2, password2)
    bearer("revoked RO1", 21, generated(5))
    bearer("revoked RO2", 22, generated(7))
    bearer("revoked RW1", 23, generated(6), write=True, admin=18)
    bearer("revoked RW2", 24, generated(8), write=True, admin=18)
    return calls


class RotationGateway(FakeGateway):
    """Reuse server-side challenge validation, with case-specific failure injection."""

    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.faults: list[Fault] = []
        self.restore_failures: int = 0
        self.apply_failed_restore: bool = False
        self.restore_attempt_count: int = 0
        self.restore_session_numbers: list[int] = []
        self.noop_side_effect: bool = False
        self.side_effect: bool = False
        self.session_drop_bug: bool = False
        self.key_session_bug: bool = False
        self.cross_key_bug: str | None = None
        self.config_override: dict[str, Any] | None = None
        self.bad_baseline_password: bool = False

    def _default_credentials(self) -> tuple[str, str]:
        password: str = "wrong-password" if self.bad_baseline_password else CONFIG.gw_id
        return "Admin", hashlib.md5(f"Admin:{REALM}:{password}".encode()).hexdigest()

    def _interactive_challenge_response(self, session: FakeSession, include_ecdh_public_key: bool) -> FakeResponse:
        response: FakeResponse = super()._interactive_challenge_response(session, include_ecdh_public_key)
        if HttpHeader.WWW_AUTHENTICATE in response.headers:
            response.headers[HttpHeader.WWW_AUTHENTICATE] = response.headers[HttpHeader.WWW_AUTHENTICATE].replace(
                'realm="Ruuvi Gateway"', f'realm="{REALM}"'
            )
        return response

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        if session.authorized and not call.authorization and call.cookie != f"RUUVISESSION={session.cookie}":
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {})
        fault: Fault
        for fault in self.faults:
            if fault.request_number == len(self.calls):
                if fault.apply:
                    self._apply_config(call.body)
                if fault.error is not None:
                    raise fault.error
                return fault.response
        if call.method == HttpMethod.POST and call.path == GatewayApi.CONFIG:
            if call.body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT:
                self.restore_attempt_count += 1
                self.restore_session_numbers.append(session.number)
                if self.restore_failures:
                    self.restore_failures -= 1
                    if self.apply_failed_restore:
                        self._apply_config(call.body)
                    return FakeResponse(HttpStatus.C_500_INTERNAL_SERVER_ERROR, {})
            elif call.body == {} and self.noop_side_effect:
                self.side_effect = True
        return None

    def _config_response(self) -> FakeResponse:
        response: FakeResponse = super()._config_response()
        payload: dict[str, Any] = response.json()
        if self.config_override is not None and not self.config_bodies:
            payload = self.config_override
        if self.side_effect:
            payload["mqtt_disable_retained_messages"] = True
        # Exercise a real JSON boundary instead of literal interning or Python bool/int equality.
        return FakeResponse(response.status_code, json.loads(json.dumps(payload)))

    def _apply_config(self, body: dict[str, str]) -> None:
        previous: list[FakeSession] = list(self.authorized_sessions)
        super()._apply_config(body)
        if self.session_drop_bug and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI:
            self.authorized_sessions = previous
            session: FakeSession
            for session in previous:
                session.authorized = True
        if self.key_session_bug and GatewayCfgDesc.LAN_AUTH_API_KEY in body and body[GatewayCfgDesc.LAN_AUTH_API_KEY]:
            for session in self.authorized_sessions:
                session.authorized = False
            self.authorized_sessions.clear()
        if set(body) == {GatewayCfgDesc.LAN_AUTH_API_KEY} and self.cross_key_bug == "RO":
            self.rw_key = "cross-affected"
        if set(body) == {GatewayCfgDesc.LAN_AUTH_API_KEY_RW} and self.cross_key_bug == "RW":
            self.ro_key = "cross-affected"


class CookieSession(FakeSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if self.trust_env or kwargs.get("timeout") != (5, 15):
            raise AssertionError("runner must disable environment authentication and set finite timeouts")
        response: FakeResponse = super().send(request, **kwargs)
        # Requests normally extracts response cookies inside Session.send; the fake transport must too.
        self.cookies.update(response.cookies)
        return response


class CorruptLoginSession(CookieSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if request.method == HttpMethod.POST and request.url == f"{CONFIG.base_url}{GatewayApi.AUTH}":
            request.body = json.dumps({"login": "Admin", "password": "corrupt-response"})
        return super().send(request, **kwargs)


class RotationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory: Path = Path(self.temp.name)
        self.log: target.EvidenceLog = target.EvidenceLog.create(self.directory, "unit_b", lambda: NOW)
        self.addCleanup(self.log.finish, "TEST", lambda: NOW)
        fixed_key: ECC.EccKey = ECC.construct(curve="secp256r1", d=7)
        with mock.patch("test_support.fake_gateway.ECC.generate", return_value=fixed_key):
            self.fake: RotationGateway = RotationGateway()
        self.runner: target.FunctionalTest_5_1_4_2_b = target.FunctionalTest_5_1_4_2_b(
            CONFIG, self.log, lambda: CookieSession(self.fake), UniqueRandom(),
            ecc_generate=lambda **kwargs: fixed_key,
        )
        self.plan: list[ExpectedCall] = expected_calls()

    def fault(self, label: str, response: FakeResponse | None = None,
              error: requests.RequestException | None = None, *, apply: bool = False) -> None:
        index: int
        item: ExpectedCall
        for index, item in enumerate(self.plan, 1):
            if item.label == label:
                self.fake.faults.append(Fault(index, response, error, apply))
                return
        raise AssertionError(f"unknown independent request label: {label}")

    def run_case(self, verdict: str, mechanism: str | None = None, outcome: str | None = None) -> RunResult:
        result: RunResult = self.runner.run()
        self.assertEqual(verdict, result.verdict, self.log.path.read_text())
        self.assertEqual({"PASS": 0, "FAIL": 1, "ERROR": 2}[verdict], result.exit_code)
        if mechanism is not None:
            self.assertEqual(outcome or verdict, result.outcomes[mechanism])
            self.assertIn(json.dumps({"mechanism": mechanism, "result": outcome or verdict}, sort_keys=True),
                          self.log.path.read_text())
        return result

    def test_complete_real_client_wire_sequence(self) -> None:
        result: RunResult = self.run_case("PASS")
        self.assertEqual(58, len(self.plan))
        self.assertEqual([item.request for item in self.plan], self.fake.calls)
        self.assertEqual({"PASS"}, set(result.outcomes.values()))
        self.assertEqual({target.ApiRoute("GET", "/auth"), target.ApiRoute("POST", "/auth"),
                          target.ApiRoute("GET", "/ruuvi.json"), target.ApiRoute("POST", "/ruuvi.json"),
                          target.ApiRoute("GET", "/history")}, result.coverage)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertEqual(("", ""), (self.fake.ro_key, self.fake.rw_key))

    def test_session_drop_bug_aborts_and_restores(self) -> None:
        self.fake.session_drop_bug = True
        self.run_case("FAIL", AuthMech.LAN_WEBUI_DEFAULT)
        self.assertEqual(self.plan[5].request, self.fake.calls[5])
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[6].body)

    def test_key_changes_must_preserve_interactive_session(self) -> None:
        self.fake.key_session_bug = True
        self.run_case("FAIL", AuthMech.M2M_API_BEARER_RO)
        self.assertGreater(self.fake.restore_attempt_count, 1)

    def test_cross_affected_rw_after_ro_rotation(self) -> None:
        self.fake.cross_key_bug = "RO"
        self.run_case("FAIL", AuthMech.M2M_API_BEARER_RO)
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[31].body)

    def test_cross_affected_ro_after_rw_rotation_preserves_ro_pass(self) -> None:
        self.fake.cross_key_bug = "RW"
        result: RunResult = self.run_case("FAIL", AuthMech.M2M_API_BEARER_RW)
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[37].body)

    def test_rejected_security_statuses_abort_at_each_control(self) -> None:
        cases: tuple[tuple[str, int, str], ...] = (
            ("default stale session", 200, AuthMech.LAN_WEBUI_DEFAULT),
            ("old default login", 200, AuthMech.LAN_WEBUI_DEFAULT),
            ("new custom1 login", 401, AuthMech.LAN_WEBUI_DEFAULT),
            ("custom stale session", 200, AuthMech.LAN_WEBUI_USER_DEFINED),
            ("old custom1 login", 200, AuthMech.LAN_WEBUI_USER_DEFINED),
            ("new custom2 login", 401, AuthMech.LAN_WEBUI_USER_DEFINED),
            ("old RO", 200, AuthMech.M2M_API_BEARER_RO),
            ("new RO", 401, AuthMech.M2M_API_BEARER_RO),
            ("unaffected RW read", 401, AuthMech.M2M_API_BEARER_RO),
            ("new RW read", 401, AuthMech.M2M_API_BEARER_RW),
            ("old RW write", 200, AuthMech.M2M_API_BEARER_RW),
            ("new RW write", 401, AuthMech.M2M_API_BEARER_RW),
            ("RO scope", 200, AuthMech.M2M_API_BEARER_RW),
        )
        label: str
        status: int
        mechanism: str
        for label, status, mechanism in cases:
            with self.subTest(label=label):
                self.fake = RotationGateway()
                self.runner = target.FunctionalTest_5_1_4_2_b(
                    CONFIG, self.log, lambda: CookieSession(self.fake), UniqueRandom()
                )
                self.fault(label, FakeResponse(status, {}))
                self.run_case("FAIL", mechanism)
                index: int = self.fake.faults[0].request_number
                # A write always gets its final hash read before restoration; reads abort immediately.
                if self.plan[index - 1].request.method == HttpMethod.POST and self.plan[index - 1].request.body == {}:
                    self.assertEqual(self.plan[index].request, self.fake.calls[index])
                    index += 1
                self.assertEqual(target.RESTORE_BODY, self.fake.calls[index].body)

    def test_stale_session_redirect_is_accepted_but_server_error_is_not(self) -> None:
        self.fault("default stale session", FakeResponse(302, {}))
        self.fault("custom stale session", FakeResponse(302, {}))
        self.run_case("PASS")

    def test_stale_session_500_does_not_prove_invalidation(self) -> None:
        self.fault("default stale session", FakeResponse(500, {}))
        self.run_case("FAIL", AuthMech.LAN_WEBUI_DEFAULT)

    def test_noop_side_effect_is_failure_and_unrestored_state_is_error(self) -> None:
        self.fake.noop_side_effect = True
        result: RunResult = self.run_case("ERROR", AuthMech.M2M_API_BEARER_RO, "FAIL")
        self.assertEqual("ERROR", result.outcomes[target.RESTORATION])
        self.assertIn(target.FACTORY_RESET_MESSAGE, result.recovery_message or "")

    def test_rejected_old_write_still_checks_hash(self) -> None:
        payload: dict[str, Any] = {
            "lan_auth_type": "lan_auth_ruuvi", "lan_auth_user": generated(3, 12),
            "lan_auth_api_key_use": True, "lan_auth_api_key_rw_use": True,
            "gw_mac": CONFIG.gw_mac, "fw_ver": "test", "unexpected": True,
        }
        self.fault("old RW write hash", FakeResponse(200, payload))
        self.run_case("FAIL", AuthMech.M2M_API_BEARER_RW)
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[40].body)

    def test_security_fail_survives_later_hash_transport_error(self) -> None:
        self.fault("old RW write", FakeResponse(200, {}))
        self.fault("old RW write hash", error=requests.Timeout("lost verification"))
        self.run_case("ERROR", AuthMech.M2M_API_BEARER_RW, "FAIL")

    def test_bearer_hash_server_error_is_error_after_verified_restoration(self) -> None:
        self.fault("initial RW write hash", FakeResponse(500, {}))
        result: RunResult = self.run_case("ERROR", AuthMech.M2M_API_BEARER_RO)
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])

    def test_post_timeout_and_hash_mismatch_preserve_error_and_mechanism_fail(self) -> None:
        payload: dict[str, Any] = {"unexpected": True}
        self.fault("initial RW write", error=requests.Timeout("secret=leak"))
        self.fault("initial RW write hash", FakeResponse(200, payload))
        messages: list[str] = []
        self.runner.output = messages.append
        result: RunResult = self.run_case("ERROR", AuthMech.M2M_API_BEARER_RO, "FAIL")
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        terminal: str = "\n".join(messages)
        self.assertIn("expected complete configuration matches expected state", terminal)
        self.assertIn("GatewayConnectionError", terminal)
        self.assertIn("NOT RUN", terminal)
        self.assertNotIn("secret=leak", terminal)
        self.assertIn("RESULT EXPLANATION", self.log.path.read_text())

    def test_final_progress_failure_still_restores_and_retains_error(self) -> None:
        def progress(message: str) -> None:
            if message.startswith("Restoring defaults"):
                raise BrokenPipeError("closed output")

        self.runner.progress = progress
        result: RunResult = self.run_case("ERROR")
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertEqual(("", ""), (self.fake.ro_key, self.fake.rw_key))

    def test_security_failure_survives_failed_assertion_evidence_write(self) -> None:
        self.fake.session_drop_bug = True
        original: Callable[[str, Any], None] = self.log.write
        failed_assertions: list[target.AssertionEvidence] = []

        def write(label: str, value: Any = "") -> None:
            if label == "ASSERTION" and isinstance(value, target.AssertionEvidence) and value.result == "FAIL":
                failed_assertions.append(value)
                raise OSError("unwritable assertion evidence")
            original(label, value)

        with mock.patch.object(self.log, "write", side_effect=write):
            result: RunResult = self.run_case("ERROR", AuthMech.LAN_WEBUI_DEFAULT, "FAIL")
        self.assertEqual(1, len(failed_assertions))
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[6].body)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertEqual(("", ""), (self.fake.ro_key, self.fake.rw_key))

    def test_failed_progress_and_persistent_evidence_failure_still_restore(self) -> None:
        original: Callable[[str, Any], None] = self.log.write
        failed: bool = False

        def write(label: str, value: Any = "") -> None:
            if failed:
                raise OSError("unwritable evidence")
            original(label, value)

        def progress(message: str) -> None:
            nonlocal failed
            if message.startswith("Restoring defaults"):
                failed = True
                raise BrokenPipeError("closed output")

        self.runner.progress = progress
        with mock.patch.object(self.log, "write", side_effect=write):
            result: RunResult = self.runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertEqual(("", ""), (self.fake.ro_key, self.fake.rw_key))

    def test_terminal_keeps_security_failure_after_recovery_error(self) -> None:
        self.fake.session_drop_bug = True
        self.fake.restore_failures = 100
        messages: list[str] = []
        self.runner.output = messages.append
        self.run_case("ERROR", AuthMech.LAN_WEBUI_DEFAULT, "FAIL")
        terminal: str = "\n".join(messages)
        self.assertIn("expected session dropped; observed 200", terminal)
        self.assertIn("final restoration: ERROR", terminal)
        self.assertIn("NOT RUN", terminal)
        self.assertNotIn(CONFIG.gw_id, terminal)
        self.assertNotIn(generated(1, 12), terminal)

    def test_wrapper_setup_error_has_secret_safe_diagnostic(self) -> None:
        messages: list[str] = []
        (self.directory / ".env").write_text("gw_id=secret=leak\n", encoding="utf-8")
        result: RunResult = target.execute_test_5_1_4_2_b(self.directory, output=messages.append)
        self.assertEqual(2, result.exit_code)
        self.assertIn("Setup/execution: ERROR: InvalidConfig", "\n".join(messages))
        self.assertIn("NOT RUN", "\n".join(messages))
        self.assertNotIn("secret=leak", "\n".join(messages))

    def test_partial_change_retains_candidate_for_recovery(self) -> None:
        self.fault("custom change", error=requests.Timeout("applied but response lost"), apply=True)
        result: RunResult = self.run_case("ERROR", AuthMech.LAN_WEBUI_USER_DEFINED)
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertGreaterEqual(self.fake.restore_attempt_count, 2)

    def test_rejected_provisioning_is_setup_error_without_applying_candidate(self) -> None:
        self.fault("custom change", FakeResponse(500, {}))
        result: RunResult = self.run_case("ERROR", AuthMech.LAN_WEBUI_USER_DEFINED)
        self.assertEqual("ERROR", result.outcomes[target.SETUP])
        self.assertEqual("PASS", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])

    def test_last_custom_credential_restoration_fallback(self) -> None:
        self.fake.restore_failures = 1
        self.run_case("PASS")
        self.assertEqual([5, 18], self.fake.restore_session_numbers)

    def test_default_restoration_fallback_after_partially_applied_restore(self) -> None:
        self.fake.restore_failures = 1
        self.fake.apply_failed_restore = True
        self.run_case("PASS")
        self.assertEqual([5, 20], self.fake.restore_session_numbers)

    def test_restoration_transport_exception_uses_custom_fallback(self) -> None:
        self.fault("restore", error=requests.Timeout("lost restoration response"))
        self.run_case("PASS")
        self.assertEqual([18], self.fake.restore_session_numbers)
        self.assertIn("GatewayConnectionError", self.log.path.read_text())

    def test_prepared_readback_unexpected_exception_marks_setup_error(self) -> None:
        original: Callable[[requests.Response, str, type[Any]], Any] = self.runner.gateway.response_json

        def decode(response: requests.Response, context: str, expected_type: type[Any]) -> Any:
            if len(self.fake.calls) == 11:
                raise RuntimeError("unexpected read-back failure")
            return original(response, context, expected_type)

        with mock.patch.object(self.runner.gateway, "response_json", side_effect=decode):
            result: RunResult = self.run_case("ERROR", AuthMech.LAN_WEBUI_DEFAULT)
        self.assertEqual("ERROR", result.outcomes[target.SETUP])
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])

    def test_prior_candidate_retained_after_unapplied_change(self) -> None:
        self.fault("custom change", error=requests.ConnectionError("request lost"))
        self.fake.restore_failures = 1
        self.run_case("ERROR", AuthMech.LAN_WEBUI_USER_DEFINED)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.fake.mode)
        self.assertGreaterEqual(self.fake.restore_attempt_count, 2)

    def test_failed_restoration_prevents_pass(self) -> None:
        self.fake.restore_failures = 100
        result: RunResult = self.run_case("ERROR", target.RESTORATION)
        self.assertIsNotNone(result.recovery_message)

    def test_final_hash_mismatch_marks_responsible_mechanism(self) -> None:
        self.fake.restored_config_mismatch = True
        self.run_case("ERROR", AuthMech.M2M_API_BEARER_RW, "FAIL")

    def test_surviving_temporary_values_prevent_pass(self) -> None:
        label: str
        mechanism: str
        for label, mechanism in (
            ("revoked custom1 login", AuthMech.LAN_WEBUI_DEFAULT),
            ("revoked custom2 login", AuthMech.LAN_WEBUI_USER_DEFINED),
            ("revoked RO1", AuthMech.M2M_API_BEARER_RO),
            ("revoked RO2", AuthMech.M2M_API_BEARER_RO),
            ("revoked RW1", AuthMech.M2M_API_BEARER_RW),
            ("revoked RW2", AuthMech.M2M_API_BEARER_RW),
        ):
            with self.subTest(label=label):
                self.fake = RotationGateway()
                self.runner = target.FunctionalTest_5_1_4_2_b(
                    CONFIG, self.log, lambda: CookieSession(self.fake), UniqueRandom()
                )
                self.fault(label, FakeResponse(200, {}))
                self.run_case("ERROR", mechanism, "FAIL")

    def test_bad_default_login_has_reset_advice_before_mutation(self) -> None:
        self.fake.bad_baseline_password = True
        result: RunResult = self.run_case("ERROR", target.SETUP)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual(2, len(self.fake.calls))
        self.assertEqual([], self.fake.config_bodies)

    def test_corrupt_password_response_cannot_pass_runner_setup(self) -> None:
        self.runner = target.FunctionalTest_5_1_4_2_b(CONFIG, self.log, lambda: CorruptLoginSession(self.fake))
        result: RunResult = self.run_case("ERROR", target.SETUP)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual(2, len(self.fake.calls))
        self.assertEqual([], self.fake.config_bodies)

    def test_basic_digest_setup_exceptions_preserve_reset_advice(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.fake = RotationGateway()
                self.fake.mode = mode
                self.runner = target.FunctionalTest_5_1_4_2_b(CONFIG, self.log, lambda: CookieSession(self.fake))
                result: RunResult = self.run_case("ERROR", target.SETUP)
                self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(1, len(self.fake.calls))

    def test_baseline_validator_decoded_missing_unequal_and_wrong_types(self) -> None:
        baseline: dict[str, Any] = self.fake._config_response().json()
        field: str
        value: Any
        for field in target.AUTHENTICATION_DEFAULT_FIELDS:
            alternatives: list[Any] = [None, 0, 1, "wrong", [], {}]
            for value in alternatives:
                with self.subTest(field=field, value=value):
                    payload: dict[str, Any] = json.loads(json.dumps(baseline))
                    if value is None:
                        del payload[field]
                    else:
                        payload[field] = value
                    self.runner.recovery_message = None
                    with self.assertRaises(target.InvalidSetup):
                        self.runner._validate_baseline(payload)
                    self.assertEqual(target.FACTORY_RESET_MESSAGE, self.runner.recovery_message)
        self.runner._validate_baseline(json.loads(json.dumps(baseline)))

    def test_identity_is_checked_before_reset_advice(self) -> None:
        mac: Any
        for mac in (None, "11:22:33:44:55:66", "invalid", 123):
            with self.subTest(mac=mac):
                self.fake = RotationGateway()
                payload: dict[str, Any] = self.fake._config_response().json()
                payload[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = True
                if mac is None:
                    del payload[GatewayCfgDesc.GW_MAC]
                else:
                    payload[GatewayCfgDesc.GW_MAC] = mac
                self.fake.config_override = payload
                self.runner = target.FunctionalTest_5_1_4_2_b(CONFIG, self.log, lambda: CookieSession(self.fake))
                result: RunResult = self.run_case("ERROR", target.SETUP)
                self.assertIsNone(result.recovery_message)
                self.assertEqual([], self.fake.config_bodies)

    def test_nondefault_verified_baseline_stops_before_mutation(self) -> None:
        self.fake.ro_key = "pre-existing"
        result: RunResult = self.run_case("ERROR", target.SETUP)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual(3, len(self.fake.calls))

    def test_two_stable_reads_required_before_mutation(self) -> None:
        payload: dict[str, Any] = self.fake._config_response().json()
        payload["unrelated"] = True
        self.fault("stable baseline", FakeResponse(200, payload))
        self.run_case("ERROR", target.SETUP)
        self.assertEqual([], self.fake.config_bodies)

    def test_full_configuration_comparison_preserves_types_and_all_fields(self) -> None:
        baseline: dict[str, Any] = json.loads('{"flag":false,"nested":{"value":1},"text":"value"}')
        self.runner._compare_config(json.loads(json.dumps(baseline)), baseline)
        candidate: dict[str, Any]
        for candidate in ({"flag": 0, "nested": {"value": 1}, "text": "value"},
                          {"flag": False, "nested": {"value": True}, "text": "value"},
                          {"flag": False, "nested": {"value": 2}, "text": "value"},
                          {"flag": False, "nested": {"value": 1}},
                          {"flag": False, "nested": {"value": 1}, "text": []}):
            with self.subTest(candidate=candidate), self.assertRaises(target.SecurityFailure):
                self.runner._compare_config(json.loads(json.dumps(candidate)), baseline)

    def test_malformed_config_is_error_and_restores(self) -> None:
        self.fault("custom1 config", FakeResponse(200, malformed_json=True))
        result: RunResult = self.run_case("ERROR", AuthMech.LAN_WEBUI_DEFAULT)
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])

    def test_transport_errors_restore_after_mutation(self) -> None:
        self.fault("new RO", error=requests.ConnectionError("offline"))
        result: RunResult = self.run_case("ERROR", AuthMech.M2M_API_BEARER_RO)
        self.assertEqual("PASS", result.outcomes[target.RESTORATION])
        self.assertEqual(target.RESTORE_BODY, self.fake.calls[30].body)

    def test_setup_error_still_creates_evidence(self) -> None:
        output: list[str] = []
        result: RunResult = target.execute_test_5_1_4_2_b(self.directory, output=output.append, now=lambda: NOW)
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        logs: list[Path] = list((self.directory / "logs").glob("test_5_1_4_2_b_*.log"))
        self.assertEqual(1, len(logs))
        self.assertIn("InvalidConfig", logs[0].read_text())
        self.assertEqual("Overall verdict: ERROR", output[-1])

    def test_recovery_advice_appears_in_terminal_and_evidence(self) -> None:
        (self.directory / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.fake.mode = GatewayCfgLanAuthType.DIGEST
        output: list[str] = []
        result: RunResult = target.execute_test_5_1_4_2_b(
            self.directory, lambda: CookieSession(self.fake), now=lambda: NOW, output=output.append
        )
        self.assertEqual("ERROR", result.verdict)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, output[-1])
        log_path: Path = next((self.directory / "logs").glob("*.log"))
        self.assertIn(target.FACTORY_RESET_MESSAGE, log_path.read_text())

    def test_execution_progress_and_secret_safe_console(self) -> None:
        # This file is an isolated test fixture, never the repository's real .env.
        (self.directory / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        output: list[str] = []
        result: RunResult = target.execute_test_5_1_4_2_b(
            self.directory, lambda: CookieSession(self.fake), UniqueRandom(), lambda: NOW, output.append
        )
        self.assertEqual("PASS", result.verdict)
        self.assertEqual(1, sum(line.startswith("Open log file:") for line in output))
        steps: list[str] = [line for line in output if line.startswith("[Step")]
        self.assertEqual(8, len(steps))
        self.assertTrue(steps[-1].startswith("[Step 8 out of 8]"))
        log_path: Path = next((self.directory / "logs").glob("*.log"))
        evidence: str = log_path.read_text()
        step: str
        for step in steps:
            self.assertIn(step, evidence)
        self.assertIn(target.OUT_OF_SCOPE, evidence)
        self.assertIn(REALM, evidence)
        self.assertIn("HTTP REQUEST", evidence)
        self.assertIn("HTTP RESPONSE", evidence)
        self.assertIn("DURATION", evidence)
        secret: str
        for secret in (CONFIG.gw_id, generated(1, 12), generated(2), generated(5), "session-5-1"):
            self.assertNotIn(secret, "\n".join(output))
            self.assertIn(secret, evidence)

    def test_main_returns_runner_exit_code(self) -> None:
        code: int
        for code in (0, 1, 2):
            with mock.patch.object(target, "execute_test_5_1_4_2_b", return_value=RunResult(code, "TEST", {}, set())):
                self.assertEqual(code, target.main())


if __name__ == "__main__":
    unittest.main()
