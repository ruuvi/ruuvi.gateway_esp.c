"""Offline Unit b implementation tests; these do not provide ETSI compliance evidence."""

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
from unittest.mock import patch

import requests
from Crypto.PublicKey import ECC

import test_5_5_4_2_b as runner
from lib.config import AUTHENTICATION_DEFAULT_FIELDS, FACTORY_RESET_MESSAGE
from lib.gateway import GatewayCfgLanAuthType
from lib.http_api import GatewayApi, HttpHeader, HttpMethod
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import FakeGateway, FakeRequestKey, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
USERNAME: str = f"test-{base64.urlsafe_b64encode(b'x' * 9).decode()}"
PASSWORD: str = base64.urlsafe_b64encode(b"x" * 24).decode()
RO_KEY: str = base64.urlsafe_b64encode(b"r" * 32).decode().rstrip("=")
RW_KEY: str = base64.urlsafe_b64encode(b"w" * 32).decode().rstrip("=")
CUSTOM_HA1: str = hashlib.md5(f"{USERNAME}:Ruuvi Gateway:{PASSWORD}".encode()).hexdigest()
PROVISION: dict[str, str] = {
    "lan_auth_type": "lan_auth_ruuvi", "lan_auth_user": USERNAME, "lan_auth_pass": CUSTOM_HA1,
    "lan_auth_api_key": RO_KEY, "lan_auth_api_key_rw": RW_KEY,
}
RESTORE: dict[str, str] = {"lan_auth_type": "lan_auth_default", "lan_auth_api_key": "", "lan_auth_api_key_rw": ""}


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


class PositiveGateway(FakeGateway):
    def __init__(self) -> None:
        with patch("test_support.fake_gateway.ECC.generate", side_effect=fixed_ecc):
            super().__init__(CONFIG)
        self.faults: list[Fault] = []
        self.config_reads: int = 0
        self.ntp_use: bool = True
        self.config_edit: Callable[[dict[str, Any], int], None] | None = None
        self.auth_edit: Callable[[dict[str, Any], int], None] | None = None
        self.corrupt_login: Callable[[requests.PreparedRequest], None] | None = None
        self.sessions: list[BoundarySession] = []

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
        return None

    def _config_response(self) -> FakeResponse:
        self.config_reads += 1
        response: FakeResponse = super()._config_response()
        payload: dict[str, Any] = response.json()
        payload["ntp_use"] = self.ntp_use
        payload["mqtt"] = {"enabled": False, "port": 1883, "topics": ["sensor"]}
        if self.config_edit is not None:
            self.config_edit(payload, self.config_reads)
        return FakeResponse(response.status_code, json.loads(json.dumps(payload)))

    def _auth_response(
        self, session: FakeSession, method: str, headers: Mapping[str, str], body: Any,
    ) -> FakeResponse:
        response: FakeResponse = super()._auth_response(session, method, headers, body)
        if method == HttpMethod.GET and self.auth_edit is not None:
            payload: dict[str, Any] = response.json()
            self.auth_edit(payload, session.number)
            return FakeResponse(response.status_code, json.loads(json.dumps(payload)),
                                response.headers, response.cookies.get_dict())
        return response

    def _read_response(self, path: str) -> FakeResponse:
        payload: dict[str, Any] = {"status": "ok"}
        if path == GatewayApi.HISTORY:
            payload = {"data": {"coordinates": "", "timestamp": 1760000000, "gw_mac": CONFIG.gw_mac,
                                "tags": {"AA:11:22:33:44:55": {"rssi": -33, "timestamp": 1759999999,
                                                            "ble_chan": 37, "ble_phy": "1M", "data": "AABB"}}}}
            if not self.ntp_use:
                payload["data"].pop("timestamp")
                tag: dict[str, Any]
                for tag in payload["data"]["tags"].values():
                    tag["counter"] = tag.pop("timestamp")
        return FakeResponse(200, json.loads(json.dumps(payload)))

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        return FakeResponse(302 if method == "GET" and self._bearer(headers) is None else 401, {})


class BoundarySession(FakeSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if self.trust_env or kwargs.get("timeout") != (5, 15) or kwargs.get("allow_redirects") is not False:
            raise AssertionError("unsafe transport settings")
        if (isinstance(self.gateway, PositiveGateway) and self.gateway.corrupt_login is not None
                and request.method == "POST" and request.url == f"{CONFIG.base_url}/auth"):
            self.gateway.corrupt_login(request)
        return super().send(request, **kwargs)


@dataclass(frozen=True)
class ExpectedRequest:
    method: str
    path: str
    session: int
    body: Any = None
    token: str | None = None
    cookie: str | None = None


def login_body(session: int, username: str = "Admin", password: str = CONFIG.gw_id) -> dict[str, str]:
    ha1: str = hashlib.md5(f"{username}:Ruuvi Gateway:{password}".encode()).hexdigest()
    response: str = hashlib.sha256(f"challenge-{session}-1:{ha1}".encode()).hexdigest()
    return {"login": username, "password": response}


def login_requests(session: int, *, custom: bool = False) -> list[ExpectedRequest]:
    body: dict[str, str] = login_body(session, USERNAME, PASSWORD) if custom else login_body(session)
    return [ExpectedRequest("GET", "/auth", session),
            ExpectedRequest("POST", "/auth", session, body, cookie=f"RUUVISESSION=session-{session}-1")]


class ImplementationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work_dir: Path = Path(self.directory.name)
        (self.work_dir / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.gateway: PositiveGateway = PositiveGateway()
        self.output: list[str] = []
        self.evidence: str = ""

    def execute(self) -> RunResult:
        result: RunResult = runner.execute_test_5_5_4_2_b(
            self.work_dir, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, self.output.append,
        )
        logs: list[Path] = sorted((self.work_dir / "logs").glob("test_5_5_4_2_b_*.log"))
        self.evidence = logs[-1].read_text(encoding="utf-8")
        self.assertEqual(self.output[-1], f"Overall verdict: {result.verdict}")
        return result

    def assert_restored(self, result: RunResult) -> None:
        self.assertEqual(result.outcomes[runner.FINAL], "PASS")
        self.assertEqual(self.gateway.mode, "lan_auth_default")
        self.assertEqual(self.gateway.ro_key, "")
        self.assertEqual(self.gateway.rw_key, "")
        self.assertEqual(self.gateway.config_bodies[-1], RESTORE)

    def test_complete_pass_exact_prepared_wire_sequence(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.verdict), (0, "PASS"))
        self.assertEqual(set(result.outcomes.values()), {"PASS"})
        self.assert_restored(result)
        # Independent route/body/session matrix: no runner route list or filtering helper.
        expected: list[ExpectedRequest] = login_requests(1) + [
            ExpectedRequest("GET", "/ruuvi.json", 1, cookie="RUUVISESSION=session-1-1"),
            ExpectedRequest("GET", "/ruuvi.json", 1, cookie="RUUVISESSION=session-1-1"),
            ExpectedRequest("GET", "/ruuvi.json", 1, cookie="RUUVISESSION=session-1-1"),
            ExpectedRequest("GET", "/status.json", 1, cookie="RUUVISESSION=session-1-1"),
            ExpectedRequest("POST", "/ruuvi.json", 1, PROVISION, cookie="RUUVISESSION=session-1-1"),
            ExpectedRequest("GET", "/ruuvi.json", 2, token=RW_KEY),
        ] + login_requests(3, custom=True) + [
            ExpectedRequest("GET", "/ruuvi.json", 3, cookie="RUUVISESSION=session-3-1"),
            ExpectedRequest("GET", "/history", 4, token=RO_KEY),
            ExpectedRequest("GET", "/status.json", 5, token=RO_KEY),
            ExpectedRequest("GET", "/ruuvi.json", 6, token=RO_KEY),
            ExpectedRequest("GET", "/ruuvi.json", 7, token=RW_KEY),
            ExpectedRequest("POST", "/ruuvi.json", 8, {}, RW_KEY),
            ExpectedRequest("GET", "/ruuvi.json", 9, token=RW_KEY),
            ExpectedRequest("POST", "/ruuvi.json", 10, RESTORE, RW_KEY),
        ] + login_requests(11) + [
            ExpectedRequest("GET", "/ruuvi.json", 11, cookie="RUUVISESSION=session-11-1"),
        ] + login_requests(12, custom=True) + [
            ExpectedRequest("GET", "/ruuvi.json", 12, cookie="RUUVISESSION=session-12-2"),
            ExpectedRequest("GET", "/ruuvi.json", 13, token=RO_KEY),
            ExpectedRequest("GET", "/ruuvi.json", 14, token=RW_KEY),
        ]
        actual: list[ExpectedRequest] = [
            ExpectedRequest(call.method, call.path, call.session_number, call.body, call.bearer_token, call.cookie)
            for call in self.gateway.calls
        ]
        self.assertEqual(actual, expected)
        call: RecordedRequest
        for call in self.gateway.calls:
            self.assertEqual(call.scheme, "Bearer" if call.bearer_token else "none")
            self.assertEqual(call.authorization, f"Bearer {call.bearer_token}" if call.bearer_token else "")
            self.assertFalse(call.allow_redirects)
        self.assertEqual(self.gateway.config_bodies, [PROVISION, {}, RESTORE])
        self.assertNotEqual(CONFIG.gw_hostname, "Ruuvi Gateway")
        self.assertEqual(self.gateway.config_bodies[0]["lan_auth_pass"], CUSTOM_HA1)

    def test_evidence_progress_secrets_and_individual_results(self) -> None:
        result: RunResult = self.execute()
        progress: list[str] = [line for line in self.output if line.startswith("[Step ")]
        self.assertEqual(len(progress), 8)
        index: int
        line: str
        for index, line in enumerate(progress, 1):
            self.assertTrue(line.startswith(f"[Step {index} out of 8] "))
            self.assertIn(line, self.evidence)
        self.assertEqual(sum(line.startswith("Open log file: ") for line in self.output), 1)
        console: str = "\n".join(self.output)
        secret: str
        for secret in (PASSWORD, RO_KEY, RW_KEY, CONFIG.gw_id):
            self.assertIn(secret, self.evidence)
            self.assertNotIn(secret, console)
        check: str
        for check in result.outcomes:
            self.assertIn(check, self.evidence)
        for line in ("ECDH SHARED SECRET", "CONFIGURATION SHA256", "CHANGED CONFIGURATION FIELDS",
                     "RESTORATION ATTEMPT", "UTC START", "PASS"):
            self.assertIn(line, self.evidence)

    def test_invalid_env_creates_evidence_without_network(self) -> None:
        (self.work_dir / ".env").unlink()
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.SETUP], "ERROR")
        self.assertEqual(self.gateway.calls, [])
        self.assertIn("Traceback", self.evidence)

    def test_basic_and_digest_baselines_report_shared_reset_advice(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.gateway = PositiveGateway()
                self.gateway.mode = mode
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                self.assertIn(FACTORY_RESET_MESSAGE, self.output)
                self.assertIn(FACTORY_RESET_MESSAGE, self.evidence)
                self.assertEqual([(call.method, call.path) for call in self.gateway.calls], [("GET", "/auth")])

    def test_default_login_denied_is_setup_error_with_reset_advice(self) -> None:
        self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/auth"), response=FakeResponse(401, {}))]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
        self.assertEqual(len(self.gateway.calls), 2)

    def test_each_baseline_field_requires_type_and_value(self) -> None:
        field: str
        replacement: Any
        for field in AUTHENTICATION_DEFAULT_FIELDS:
            for replacement in (None, 0, 1, "wrong", True):
                with self.subTest(field=field, value=replacement):
                    self.gateway = PositiveGateway()

                    def edit(payload: dict[str, Any], _read: int, selected_field: str = field,
                             selected_value: Any = replacement) -> None:
                        if selected_value is None:
                            payload.pop(selected_field)
                        else:
                            payload[selected_field] = selected_value

                    self.gateway.config_edit = edit
                    result: RunResult = self.execute()
                    self.assertEqual(result.exit_code, 2)
                    self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                    self.assertEqual(self.gateway.config_bodies, [])

    def test_identity_validation_precedes_nondefault_fields_and_reset_advice(self) -> None:
        mac: Any
        for mac in (None, "11:22:33:44:55:66", "broken", 7):
            with self.subTest(mac=mac):
                self.gateway = PositiveGateway()

                def edit(payload: dict[str, Any], _read: int, selected_mac: Any = mac) -> None:
                    if selected_mac is None:
                        payload.pop("gw_mac")
                    else:
                        payload["gw_mac"] = selected_mac
                    payload["lan_auth_api_key_use"] = True

                self.gateway.config_edit = edit
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertIsNone(result.recovery_message)
                self.assertEqual(self.gateway.config_bodies, [])

    def test_matching_case_insensitive_or_absent_mac_with_valid_defaults(self) -> None:
        missing: bool
        for missing in (False, True):
            with self.subTest(missing=missing):
                self.gateway = PositiveGateway()

                def edit(payload: dict[str, Any], _read: int, remove_mac: bool = missing) -> None:
                    if remove_mac:
                        payload.pop("gw_mac")
                    else:
                        payload["gw_mac"] = CONFIG.gw_mac.lower()

                self.gateway.config_edit = edit
                self.assertEqual(self.execute().exit_code, 0)

    def test_reported_challenge_mode_requires_default_string(self) -> None:
        replacement: Any
        for replacement in (None, 0, True, "lan_auth_ruuvi"):
            with self.subTest(value=replacement):
                self.gateway = PositiveGateway()

                def edit(payload: dict[str, Any], _session: int, selected_value: Any = replacement) -> None:
                    if selected_value is None:
                        payload.pop("lan_auth_type")
                    else:
                        payload["lan_auth_type"] = selected_value

                self.gateway.auth_edit = edit
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                self.assertEqual(self.gateway.config_bodies, [])

    def test_unstable_baseline_aborts_before_mutation(self) -> None:
        def edit(payload: dict[str, Any], read: int) -> None:
            if read == 2:
                payload["mqtt"]["enabled"] = 0

        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(self.gateway.config_bodies, [])
        self.assertIn("mqtt", self.evidence)

    def test_each_prepared_field_rejects_missing_wrong_type_and_unequal_values(self) -> None:
        field: str
        replacement: Any
        for field in AUTHENTICATION_DEFAULT_FIELDS:
            for replacement in (None, 1, False, "wrong"):
                with self.subTest(field=field, value=replacement):
                    self.gateway = PositiveGateway()

                    def edit(payload: dict[str, Any], read: int, selected_field: str = field,
                             selected_value: Any = replacement) -> None:
                        if read == 4:
                            if selected_value is None:
                                payload.pop(selected_field)
                            else:
                                payload[selected_field] = selected_value

                    self.gateway.config_edit = edit
                    result: RunResult = self.execute()
                    self.assertEqual(result.exit_code, 2)
                    self.assertEqual(result.outcomes[runner.PREPARED], "ERROR")
                    self.assertEqual(result.outcomes[runner.CUSTOM], "NOT RUN")
                    self.assert_restored(result)

    def test_preparation_side_effect_is_visible_and_restoration_mismatch_is_error(self) -> None:
        def edit(payload: dict[str, Any], read: int) -> None:
            if read >= 4:
                payload["mqtt"]["port"] = 8883

        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.PREPARED], "ERROR")
        self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        self.assertIsNotNone(result.recovery_message)
        self.assertIn('"mqtt"', self.evidence)
        self.assertEqual(self.gateway.config_bodies, [PROVISION, RESTORE])

    def test_valid_authorization_denials_fail_and_abort_before_later_probes(self) -> None:
        cases: tuple[tuple[str, FakeRequestKey, int], ...] = (
            (runner.DEFAULT, FakeRequestKey("none", "GET", "/status.json"), 1),
            (runner.CUSTOM, FakeRequestKey("none", "POST", "/auth"), 2),
            (runner.CUSTOM, FakeRequestKey("none", "GET", "/ruuvi.json"), 4),
            (runner.RO, FakeRequestKey("Bearer", "GET", "/history"), 1),
            (runner.RO, FakeRequestKey("Bearer", "GET", "/status.json"), 1),
            (runner.RO, FakeRequestKey("Bearer", "GET", "/ruuvi.json"), 2),
            (runner.RW, FakeRequestKey("Bearer", "GET", "/ruuvi.json"), 3),
            (runner.RW, FakeRequestKey("Bearer", "POST", "/ruuvi.json"), 1),
        )
        check: str
        key: FakeRequestKey
        occurrence: int
        for check, key, occurrence in cases:
            with self.subTest(check=check, key=key):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(key, occurrence, FakeResponse(401, {}))]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 1)
                self.assertEqual(result.outcomes[check], "FAIL")
                self.assertIn(f"{check}: FAIL", self.output)
                self.assertEqual(result.outcomes[runner.FINAL], "PASS")
                if check != runner.DEFAULT:
                    self.assert_restored(result)
                    self.assertEqual(result.outcomes[runner.DEFAULT], "PASS")
                if check not in (runner.RW, runner.DEFAULT):
                    self.assertNotIn({}, self.gateway.config_bodies)
                if check == runner.RW:
                    self.assertEqual(result.outcomes[runner.RO], "PASS")

    def test_rw_noop_hash_failure_attributed_to_rw_and_can_recover(self) -> None:
        def edit(payload: dict[str, Any], read: int) -> None:
            if read == 8:
                payload["mqtt"]["enabled"] = 0

        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 1)
        self.assertEqual(result.outcomes[runner.RW], "FAIL")
        self.assertEqual(result.outcomes[runner.RO], "PASS")
        self.assert_restored(result)

    def test_persistent_rw_mutation_keeps_security_fail_when_recovery_errors(self) -> None:
        def edit(payload: dict[str, Any], read: int) -> None:
            if read >= 8:
                payload["mqtt"]["enabled"] = 0

        self.gateway.config_edit = edit
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.RW], "FAIL")
        self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        self.assertEqual(result.outcomes[runner.RO], "PASS")
        self.assertIn("USER ACTION REQUIRED", result.recovery_message or "")
        self.assertIn(result.recovery_message, self.output)

    def test_transport_and_unexpected_errors_always_restore(self) -> None:
        error: Exception
        for error in (requests.Timeout("timeout"), requests.ConnectionError("offline"), RuntimeError("unexpected")):
            with self.subTest(error=type(error).__name__):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/history"), error=error)]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.RO], "ERROR")
                self.assertEqual(result.outcomes[runner.CUSTOM], "PASS")
                self.assert_restored(result)
                self.assertIn(type(error).__name__, self.evidence)

    def test_malformed_response_and_history_shape_errors(self) -> None:
        payload: Any
        for payload in ([], {}, {"data": []}, {"data": {"timestamp": True}},
                        {"data": {"coordinates": "", "timestamp": 5, "gw_mac": CONFIG.gw_mac,
                                  "tags": {"bad": {}}}}):
            with self.subTest(payload=payload):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/history"),
                                            response=FakeResponse(200, payload))]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.RO], "ERROR")
                self.assert_restored(result)
        self.gateway = PositiveGateway()
        self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/status.json"),
                                    response=FakeResponse(200, malformed_json=True))]
        result = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored(result)

    def test_ntp_disabled_counter_history_completes_rw_check_and_restores(self) -> None:
        self.gateway.ntp_use = False
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assertEqual(result.outcomes[runner.RO], "PASS")
        self.assertEqual(result.outcomes[runner.RW], "PASS")
        self.assertIn({}, self.gateway.config_bodies)
        self.assert_restored(result)

    def test_counter_history_rejects_missing_and_mistyped_counter(self) -> None:
        value: Any
        for value in (None, True, "123"):
            with self.subTest(value=value):
                self.gateway = PositiveGateway()
                self.gateway.ntp_use = False
                tag: dict[str, Any] = {"rssi": -33, "ble_chan": 37, "ble_phy": "1M", "data": "AABB"}
                if value is not None:
                    tag["counter"] = value
                self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/history"),
                    response=FakeResponse(200, {"data": {"coordinates": "", "gw_mac": CONFIG.gw_mac,
                                                        "tags": {"AA:11:22:33:44:55": tag}}}))]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.RO], "ERROR")
                self.assert_restored(result)

    def test_cleanup_reporting_failures_preserve_restoration_and_fallbacks(self) -> None:
        target: str
        for target in ("progress", "output", "persistent-output", "prior-fail", "checkpoint", "exception"):
            with self.subTest(target=target):
                self.gateway = PositiveGateway()
                if target != "progress":
                    self.gateway.primary_restore_status = 401
                if target == "prior-fail":
                    self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/history"),
                                                response=FakeResponse(401, {}))]
                original_write: Callable[..., None] = runner.EvidenceLog.write
                original_exception: Callable[..., None] = runner.EvidenceLog.exception

                def write(log: runner.EvidenceLog, label: str, value: Any = "", selected: str = target,
                          original: Callable[..., None] = original_write) -> None:
                    if selected == "checkpoint" and label == "RESTORATION ATTEMPT":
                        raise OSError("evidence checkpoint failed")
                    original(log, label, value)

                def log_exception(log: runner.EvidenceLog, error: BaseException, selected: str = target,
                                  original: Callable[..., None] = original_exception) -> None:
                    if selected == "exception":
                        raise OSError("exception evidence failed")
                    original(log, error)

                def output(message: str, selected: str = target) -> None:
                    if (selected == "progress" and "Restoring default" in message
                            or selected in ("output", "prior-fail") and "restoration:" in message
                            or selected == "persistent-output" and PROVISION in self.gateway.config_bodies):
                        raise BrokenPipeError("consumer closed")
                    self.output.append(message)

                with patch.object(runner.EvidenceLog, "write", write), \
                        patch.object(runner.EvidenceLog, "exception", log_exception):
                    result: RunResult = runner.execute_test_5_5_4_2_b(
                        self.work_dir, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, output,
                    )
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.FINAL], "ERROR")
                self.assertEqual(self.gateway.mode, "lan_auth_default")
                self.assertEqual((self.gateway.ro_key, self.gateway.rw_key), ("", ""))
                self.assertEqual(self.gateway.config_bodies[-1], RESTORE)
                self.assertEqual(self.gateway.calls[-1].bearer_token, RW_KEY)
                if target == "prior-fail":
                    self.assertEqual(result.outcomes[runner.RO], "FAIL")
                if target != "progress":
                    self.assertEqual(self.gateway.restoration_attempts, ["bearer", "custom"])

    def test_persistent_transcript_failure_cannot_block_recovery_traffic(self) -> None:
        scenario: str
        for scenario in ("bearer", "custom", "lost-response", "denied", "unrevoked-key"):
            with self.subTest(scenario=scenario):
                self.check_persistent_transcript_failure(scenario)

    def check_persistent_transcript_failure(self, scenario: str) -> None:
        self.gateway = PositiveGateway()
        if scenario in ("custom", "denied"):
            self.gateway.primary_restore_status = 401
        if scenario == "lost-response":
            self.gateway.faults = [Fault(FakeRequestKey("Bearer", "POST", "/ruuvi.json"),
                error=requests.Timeout("restoration response lost"), apply_body=True)]
        elif scenario == "denied":
            self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/auth"), 2,
                                        response=FakeResponse(401, {}))]
        elif scenario == "unrevoked-key":
            self.gateway.faults = [Fault(FakeRequestKey("Bearer", "GET", "/ruuvi.json"),
                                        response=FakeResponse(200, {}))]
        original_write: Callable[[runner.EvidenceLog, str, Any], None] = cast(
            "Callable[[runner.EvidenceLog, str, Any], None]", runner.EvidenceLog.write,
        )
        original_line: Callable[[runner.EvidenceLog, str], None] = cast(
            "Callable[[runner.EvidenceLog, str], None]", runner.EvidenceLog.write_line,
        )
        broken: bool = False
        failures: list[str] = []

        def check_disk(label: str) -> None:
            nonlocal broken
            broken = broken or PROVISION in self.gateway.config_bodies
            if broken:
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

        log: runner.EvidenceLog = runner.EvidenceLog.create(self.work_dir / "logs", "persistent", fixed_now)
        try:
            with patch.object(runner.EvidenceLog, "create", return_value=log), \
                    patch.object(runner.EvidenceLog, "write", write), \
                    patch.object(runner.EvidenceLog, "write_line", write_line), \
                    patch.object(runner.EvidenceLog, "finish", finish):
                result: RunResult = runner.execute_test_5_5_4_2_b(
                    self.work_dir, self.gateway.session, FixedRandom(), fixed_ecc, fixed_now, self.output.append,
                )
        finally:
            log.finish("ERROR", fixed_now)
        self.assertTrue(broken)
        self.assertGreater(failures.count("HTTP REQUEST BEGIN"), 4)
        self.assertIn("ASSERTION", failures)
        self.assertIn("finish", failures)
        self.assertIn("INTERACTIVE CHALLENGE", failures)
        self.assertEqual((result.exit_code, result.verdict), (2, "ERROR"))
        self.assertIsNotNone(result.recovery_message)
        self.assertEqual(self.gateway.calls[-1].bearer_token, RW_KEY)
        if scenario == "denied":
            self.assertEqual(self.gateway.mode, "lan_auth_ruuvi")
            self.assertEqual(self.gateway.restoration_attempts, ["bearer"])
            self.assertEqual(len([call for call in self.gateway.calls
                                  if call.path == "/auth" and call.method == "POST"]), 5)
        else:
            self.assertEqual(self.gateway.mode, "lan_auth_default")
            self.assertEqual((self.gateway.ro_key, self.gateway.rw_key), ("", ""))
            self.assertEqual(self.gateway.config_bodies[-1], RESTORE)
            self.assertEqual(self.gateway.restoration_attempts,
                             ["bearer", "custom"] if scenario == "custom"
                             else ["default"] if scenario == "lost-response" else ["bearer"])
            final_reads: list[RecordedRequest] = [call for call in self.gateway.calls
                                                 if call.method == "GET" and call.path == "/ruuvi.json"
                                                 and call.bearer_token is None]
            self.assertIn(final_reads[-2].session_number,
                          [session.number for session in self.gateway.authorized_sessions])
        if scenario == "unrevoked-key":
            self.assertEqual(result.outcomes[runner.FINAL], "FAIL")

    def test_initial_malformed_json_and_transport_error_do_not_mutate(self) -> None:
        fault: Fault
        for fault in (
            Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), response=FakeResponse(200, malformed_json=True)),
            Fault(FakeRequestKey("none", "GET", "/auth"), error=requests.Timeout("timeout")),
        ):
            with self.subTest(fault=fault):
                self.gateway = PositiveGateway()
                self.gateway.faults = [fault]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(self.gateway.config_bodies, [])

    def test_provisioning_lost_response_or_denial_recovers_all_candidate_states(self) -> None:
        applied: bool
        for applied in (False, True):
            with self.subTest(applied=applied):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/ruuvi.json"),
                                            error=requests.Timeout("lost provisioning response"), apply_body=applied)]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.PREPARED], "ERROR")
                self.assert_restored(result)
                self.assertIn("TEMPORARY RECOVERY CREDENTIALS", self.evidence)
                self.assertEqual(self.gateway.restoration_attempts, ["bearer"] if applied else ["default"])
        self.gateway = PositiveGateway()
        self.gateway.faults = [Fault(FakeRequestKey("none", "POST", "/ruuvi.json"), response=FakeResponse(503, {}))]
        result = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored(result)

    def test_partial_provisioning_can_recover_with_custom_credentials(self) -> None:
        def apply_partial(_session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            if call.method == "POST" and call.path == "/ruuvi.json" and call.body == PROVISION:
                partial: dict[str, str] = dict(PROVISION)
                partial.pop("lan_auth_api_key_rw")
                self.gateway._apply_config(partial)
                raise requests.Timeout("partial write")
            return None

        with patch.object(self.gateway, "_response_override", side_effect=apply_partial):
            result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored(result)
        self.assertEqual(self.gateway.restoration_attempts, ["custom"])

    def test_prepared_readback_and_custom_login_exceptions_are_setup_errors(self) -> None:
        key: FakeRequestKey
        occurrence: int
        for key, occurrence in ((FakeRequestKey("Bearer", "GET", "/ruuvi.json"), 1),
                                (FakeRequestKey("none", "POST", "/auth"), 2)):
            with self.subTest(key=key):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(key, occurrence, error=requests.Timeout("setup control"))]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.PREPARED], "ERROR")
                self.assert_restored(result)

    def test_restore_fallback_order_on_denied_or_lost_primary(self) -> None:
        applied: bool
        for applied in (False, True):
            with self.subTest(applied=applied):
                self.gateway = PositiveGateway()
                self.gateway.faults = [Fault(FakeRequestKey("Bearer", "POST", "/ruuvi.json"), 2,
                                            error=requests.Timeout("lost restore response"), apply_body=applied)]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 0)
                self.assert_restored(result)
                self.assertIn("custom interactive", self.evidence)
                if applied:
                    self.assertIn("default interactive", self.evidence)
                    self.assertEqual(self.gateway.restoration_attempts, ["default"])
                else:
                    self.assertEqual(self.gateway.restoration_attempts, ["custom"])
        self.gateway = PositiveGateway()
        self.gateway.primary_restore_status = 401
        result = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assertEqual(self.gateway.restoration_attempts, ["bearer", "custom"])

    def test_all_restoration_candidates_fail_and_preserve_earlier_fail(self) -> None:
        self.gateway.primary_restore_status = 401
        self.gateway.faults = [
            Fault(FakeRequestKey("Bearer", "GET", "/history"), response=FakeResponse(401, {})),
            Fault(FakeRequestKey("none", "POST", "/ruuvi.json"), 2, FakeResponse(503, {})),
        ]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.RO], "FAIL")
        self.assertNotEqual(result.outcomes[runner.FINAL], "PASS")
        self.assertIn("all restoration credentials failed", self.evidence)
        self.assertIsNotNone(result.recovery_message)
        self.assertIn(RO_KEY, self.evidence)
        self.assertIn(RW_KEY, self.evidence)

    def test_final_configuration_mismatch_and_transport_errors_prevent_pass(self) -> None:
        self.gateway.restored_config_mismatch = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
        self.assertEqual(result.outcomes[runner.RW], "PASS")
        self.gateway = PositiveGateway()
        self.gateway.faults = [Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 5,
                                    error=requests.Timeout("final read"))]
        result = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.FINAL], "ERROR")
        self.assertEqual(self.gateway.calls[-1].bearer_token, RW_KEY)

    def test_each_revoked_credential_is_checked_even_after_another_fails(self) -> None:
        fault: Fault
        for fault in (
            Fault(FakeRequestKey("none", "POST", "/auth"), 4, FakeResponse(200, {})),
            Fault(FakeRequestKey("none", "GET", "/ruuvi.json"), 6, FakeResponse(200, {})),
            Fault(FakeRequestKey("Bearer", "GET", "/ruuvi.json"), 5, FakeResponse(200, {})),
            Fault(FakeRequestKey("Bearer", "GET", "/ruuvi.json"), 6, FakeResponse(200, {})),
        ):
            with self.subTest(fault=fault):
                self.gateway = PositiveGateway()
                self.gateway.faults = [fault]
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.FINAL], "FAIL")
                self.assertEqual(self.gateway.calls[-1].bearer_token, RW_KEY)

    def test_corrupted_login_password_and_cookie_cannot_authorize(self) -> None:
        corruption: str
        for corruption in ("password", "cookie", "missing"):
            with self.subTest(corruption=corruption):
                self.gateway = PositiveGateway()

                def corrupt(request: requests.PreparedRequest, selected_corruption: str = corruption) -> None:
                    if selected_corruption == "cookie":
                        request.headers[HttpHeader.COOKIE] = "RUUVISESSION=another-session"
                    elif selected_corruption == "missing":
                        request.headers.pop(HttpHeader.COOKIE)
                    else:
                        request.body = json.dumps({"login": "Admin", "password": "0" * 64})

                self.gateway.corrupt_login = corrupt
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(self.gateway.config_bodies, [])
                self.assertEqual(self.gateway.authorized_sessions, [])

    def test_main_returns_execution_exit_code(self) -> None:
        code: int
        for code, verdict in ((0, "PASS"), (1, "FAIL"), (2, "ERROR")):
            with patch.object(runner, "execute_test_5_5_4_2_b", return_value=RunResult(code, verdict, {}, set())):
                self.assertEqual(runner.main(), code)


if __name__ == "__main__":
    unittest.main()
