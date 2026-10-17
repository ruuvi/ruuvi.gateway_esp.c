"""Deterministic offline implementation tests; NOT ETSI compliance evidence."""

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
from typing import Any, Callable
from unittest import mock

import requests
from Crypto.PublicKey import ECC

import test_5_5_5_2_a as target
from lib.config import FACTORY_RESET_MESSAGE
from lib.evidence import EvidenceLog
from lib.gateway import AuthMech, GatewayCfgLanAuthType
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import (
    DefaultAuthGateway,
    FakeResponse,
    FakeSession,
    RecordedRequest,
    UniqueRandom,
    env_text,
)

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 23, tzinfo=timezone.utc)
RO: str = base64.urlsafe_b64encode(bytes(range(1, 33))).decode().rstrip("=")
RW: str = base64.urlsafe_b64encode(bytes(range(2, 34))).decode().rstrip("=")
PREFIX: str = f"cra-{base64.urlsafe_b64encode(bytes(range(3, 11))).decode().rstrip('=')}"
U_BODY: dict[str, str] = {"coordinates": f"{PREFIX}-U"}
RO_BODY: dict[str, str] = {"coordinates": f"{PREFIX}-RO"}
RW_BODY: dict[str, str] = {"coordinates": f"{PREFIX}-RW"}
RESTORE_BODY: dict[str, str] = {"coordinates": ""}
KEYS_BODY: dict[str, str] = {"lan_auth_api_key": RO, "lan_auth_api_key_rw": RW}
CLEAR_BODY: dict[str, str] = {"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}


@dataclass(frozen=True)
class WriteFault:
    body: dict[str, str]
    status: int = 200
    apply: bool = False
    error: Exception | None = None
    bearer_only: bool = False
    repeat: bool = False


class ConfigGateway(DefaultAuthGateway):
    """Use the shared independent challenge/cookie verifier and model only this task's fields."""

    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.coordinates: str = ""
        self.stable: str | int = "untouched"
        self.read_count: int = 0
        self.read_overrides: dict[int, Any] = {}
        self.write_faults: list[WriteFault] = []
        self.provision_side_effect: bool = False
        self.commit_side_effect: bool = False
        self.partial_keys: bool = False
        self.bad_redirect: bool = False
        self.redirect_location: str = "http://gateway.local/#auth"
        self.mqtt_data_format: str = "ruuvi_raw"
        self.mqtt_prefix: str = "ruuvi/AA:BB:CC:DD:EE:FF/"
        self.mqtt_client_id: str = "AA:BB:CC:DD:EE:FF"
        self.bypass_get: bool = False
        self.reject_login: bool = False
        self.restore_started: bool = False
        self.restore_login_fails: bool = False
        self.sessions: list[CheckedSession] = []

    def config(self) -> dict[str, Any]:
        return json.loads(json.dumps({
            "lan_auth_type": self.mode, "lan_auth_user": "Admin",
            "lan_auth_api_key_use": bool(self.ro_key), "lan_auth_api_key_rw_use": bool(self.rw_key),
            "coordinates": self.coordinates, "gw_mac": CONFIG.gw_mac,
            "mqtt_data_format": self.mqtt_data_format, "mqtt_prefix": self.mqtt_prefix,
            "mqtt_client_id": self.mqtt_client_id,
            "fw_ver": "test-fw", "nrf52_fw_ver": "test-nrf", "stable": self.stable,
        }))

    def _default_credentials(self) -> tuple[str, str]:
        if self.reject_login:
            return "Admin", "wrong-ha1"
        return super()._default_credentials()

    def _config_response(self) -> FakeResponse:
        self.read_count += 1
        value: Any = self.read_overrides.get(self.read_count, self.config())
        if isinstance(value, Exception):
            raise value
        return FakeResponse(200, value, malformed_json=value == "malformed")

    def _apply_config(self, body: dict[str, str]) -> None:
        super()._apply_config(body)
        self.coordinates = body.get("coordinates", self.coordinates)
        # Independent model of gw_cfg_json_parse_mqtt(): missing fields reset on every POST.
        self.mqtt_data_format = body.get("mqtt_data_format", "ruuvi_raw")
        self.mqtt_prefix = body.get("mqtt_prefix") or "ruuvi/AA:BB:CC:DD:EE:FF/"
        self.mqtt_client_id = body.get("mqtt_client_id") or "AA:BB:CC:DD:EE:FF"
        if body == KEYS_BODY and self.provision_side_effect:
            self.stable = "collateral provisioning"
        if body == RW_BODY and self.commit_side_effect:
            self.stable = "collateral commit"
        if body == KEYS_BODY and self.partial_keys:
            self.rw_key = ""

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        if call.method == "POST" and call.path == "/ruuvi.json":
            if call.body == RESTORE_BODY:
                self.restore_started = True
            fault: WriteFault
            for fault in self.write_faults:
                if fault.body == call.body and (not fault.bearer_only or call.bearer_token is not None):
                    if not fault.repeat:
                        self.write_faults.remove(fault)
                    if fault.apply:
                        self._apply_config(call.body)
                    if fault.error is not None:
                        raise fault.error
                    return FakeResponse(fault.status, {})
        if self.restore_login_fails and self.restore_started and call.path == "/auth" and call.method == "POST":
            return FakeResponse(401, {"authenticated": False})
        return None

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        if method == "GET" and path == "/ruuvi.json" and not headers.get("Authorization"):
            return FakeResponse(200 if self.bypass_get else 302, {}, headers={
                "Location": "/other" if self.bad_redirect else self.redirect_location,
            })
        return super()._unauthorized_response(method, path, headers)

    def session(self) -> requests.Session:
        session: CheckedSession = CheckedSession(self)
        self.sessions.append(session)
        return session


class CheckedSession(FakeSession):
    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if self.trust_env or kwargs.get("timeout") != (5, 15) or kwargs.get("allow_redirects") is not False:
            raise AssertionError("requests must disable environment settings/redirects and use bounded timeouts")
        if (self.authorized and not request.headers.get("Authorization")
                and request.headers.get("Cookie") != f"RUUVISESSION={self.cookie}"):
            raise AssertionError("authorized request must carry the session cookie")
        response: FakeResponse = super().send(request, **kwargs)
        # Requests' real transport persists response cookies; the shared send override bypasses it.
        self.cookies.update(response.cookies)
        return response


def fixed_ecc(**_kwargs: Any) -> ECC.EccKey:
    return ECC.construct(curve="P-256", d=7)


class FunctionalImplementationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root: Path = Path(self.directory.name)
        (self.root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        with mock.patch("test_support.fake_gateway.ECC.generate", side_effect=fixed_ecc):
            self.gateway: ConfigGateway = ConfigGateway()
        self.output: list[str] = []
        self.evidence: str = ""

    def execute(self) -> RunResult:
        result: RunResult = target.execute_test_5_5_5_2_a(
            self.root, self.gateway.session, now=lambda: NOW, random_bytes=UniqueRandom(),
            ecc_generate=fixed_ecc, output=self.output.append,
        )
        logs: list[Path] = sorted((self.root / "logs").glob("*.log"))
        self.evidence = logs[-1].read_text(encoding="utf-8")
        return result

    def config_writes(self) -> list[RecordedRequest]:
        return [call for call in self.gateway.calls if call.method == "POST" and call.path == "/ruuvi.json"]

    def assert_restored(self) -> None:
        self.assertEqual("", self.gateway.coordinates)
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("", self.gateway.rw_key)
        self.assertIn(RESTORE_BODY, [call.body for call in self.config_writes()])

    @staticmethod
    def request(method: str, path: str, session: int, body: Any = None,
                token: str | None = None, cookie: bool = False) -> RecordedRequest:
        return RecordedRequest(method, path, token, body, session,
                               f"Bearer {token}" if token else "",
                               f"RUUVISESSION=session-{session}" if cookie else None, False)

    def login(self, session: int, *, read: bool = True) -> list[RecordedRequest]:
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        password: str = hashlib.sha256(f"challenge-{session}-1:{ha1}".encode()).hexdigest()
        calls: list[RecordedRequest] = [
            self.request("GET", "/auth", session),
            self.request("POST", "/auth", session, {"login": "Admin", "password": password}, cookie=True),
        ]
        if read:
            calls.append(self.request("GET", "/ruuvi.json", session, cookie=True))
        return calls

    def test_complete_pass_exact_prepared_requests_bodies_sessions_and_evidence(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict), self.evidence)
        expected: list[RecordedRequest] = (
            self.login(1) + [self.request("GET", "/ruuvi.json", 1, cookie=True),
                             self.request("GET", "/ruuvi.json", 2), self.request("GET", "/auth", 3),
                             self.request("POST", "/ruuvi.json", 4, U_BODY)]
            + self.login(5) + self.login(6, read=False)
            + [self.request("POST", "/ruuvi.json", 6, KEYS_BODY, cookie=True)]
            + self.login(7)
            + [self.request("GET", "/ruuvi.json", 8, token=RO),
               self.request("GET", "/ruuvi.json", 9, token=RW),
               self.request("POST", "/ruuvi.json", 10, RO_BODY, RO)]
            + self.login(11) + [self.request("POST", "/ruuvi.json", 12, RW_BODY, RW)]
            + self.login(13) + self.login(14)
            + [self.request("POST", "/ruuvi.json", 15, RESTORE_BODY, RW)]
            + self.login(16) + [self.request("POST", "/ruuvi.json", 17, CLEAR_BODY, RW)]
            + self.login(18) + [self.request("GET", "/ruuvi.json", 19, token=RO),
                                self.request("GET", "/ruuvi.json", 20, token=RW)]
        )
        self.assertEqual(expected, self.gateway.calls)
        self.assertEqual({"PASS"}, set(result.outcomes.values()))
        self.assert_restored()
        self.assertIsNone(result.recovery_message)
        self.assertIn("Self-assessment claims 403; firmware expects 401; observed status 401", self.evidence)
        self.assertIn("Final baseline SHA256 non-mutation proof", self.evidence)
        self.assertIn(RO, self.evidence)
        self.assertIn(RW, self.evidence)
        self.assertNotIn(RO, "\n".join(self.output))
        self.assertNotIn(RW, "\n".join(self.output))
        self.assertEqual(1, sum(line.startswith("Open log file:") for line in self.output))
        steps: list[str] = [line for line in self.output if line.startswith("[Step")]
        self.assertEqual(11, len(steps))
        step: str
        index: int
        for index, step in enumerate(steps, 1):
            self.assertTrue(step.startswith(f"[Step {index} out of 11]"))
            self.assertIn(step, self.evidence)
        self.assertEqual("Overall verdict: PASS", self.output[-1])

    def test_invalid_env_creates_evidence_without_network(self) -> None:
        (self.root / ".env").unlink()
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual([], self.gateway.calls)
        self.assertIn("InvalidConfig", self.evidence)
        self.assertIn("OVERALL VERDICT", self.evidence)
        self.assertEqual("Overall verdict: ERROR", self.output[-1])

    def test_default_login_rejected_requires_reset(self) -> None:
        self.gateway.reject_login = True
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertIn(FACTORY_RESET_MESSAGE, self.output)
        self.assertEqual([], self.config_writes())

    def test_basic_and_digest_setup_challenges_require_reset_without_probes(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.gateway.mode = mode
                self.gateway.calls.clear()
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual(1, len(self.gateway.calls))
                self.assertEqual([], self.config_writes())

    def test_baseline_missing_wrong_type_and_nondefault_fields_stop_before_probes(self) -> None:
        field: str
        value: Any
        for field, value in (
            ("lan_auth_type", "lan_auth_ruuvi"), ("lan_auth_user", "Other"),
            ("lan_auth_api_key_use", 0), ("lan_auth_api_key_rw_use", True),
            ("coordinates", 0), ("coordinates", "nondefault"), ("coordinates", None),
            ("mqtt_data_format", None), ("mqtt_prefix", None), ("mqtt_client_id", None),
            ("mqtt_data_format", 0), ("mqtt_prefix", 0), ("mqtt_client_id", 0),
        ):
            with self.subTest(field=field, value=value):
                self.gateway.calls.clear()
                self.gateway.read_count = 0
                payload: dict[str, Any] = self.gateway.config()
                if value is None:
                    del payload[field]
                else:
                    payload[field] = value
                self.gateway.read_overrides = {1: json.loads(json.dumps(payload))}
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertEqual([], self.config_writes())
                self.assertEqual(3, len(self.gateway.calls))

    def test_wrong_malformed_or_missing_identity_never_recommends_reset(self) -> None:
        mac: Any
        for mac in ("00:00:00:00:00:00", "bad", 123, None):
            with self.subTest(mac=mac):
                self.gateway.calls.clear()
                self.gateway.read_count = 0
                payload: dict[str, Any] = self.gateway.config()
                payload["lan_auth_api_key_use"] = True
                if mac is None:
                    del payload["gw_mac"]
                else:
                    payload["gw_mac"] = mac
                self.gateway.read_overrides = {1: payload}
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertIsNone(result.recovery_message)
                self.assertNotIn(FACTORY_RESET_MESSAGE, self.evidence)
                self.assertEqual([], self.config_writes())

    def test_unstable_baseline_is_setup_error(self) -> None:
        changed: dict[str, Any] = self.gateway.config()
        changed["stable"] = "changed"
        self.gateway.read_overrides[2] = changed
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual([], self.config_writes())

    def test_read_bypass_and_bad_redirect_abort_before_any_write(self) -> None:
        mode: str
        for mode in ("bypass_get", "bad_redirect"):
            with self.subTest(mode=mode):
                setattr(self.gateway, mode, True)
                result: RunResult = self.execute()
                self.assertEqual(1, result.exit_code)
                self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
                self.assertEqual("PASS", result.outcomes[target.RESTORE])
                self.assertEqual([], self.config_writes())
                setattr(self.gateway, mode, False)

    def test_custom_mqtt_baselines_stop_before_any_configuration_post(self) -> None:
        field: str
        value: str
        for field, value in (("mqtt_data_format", "ruuvi_decoded"), ("mqtt_prefix", "custom/"),
                             ("mqtt_client_id", "custom-client")):
            with self.subTest(field=field):
                self.gateway.calls.clear()
                before: dict[str, Any] = self.gateway.config()
                setattr(self.gateway, field, value)
                custom: dict[str, Any] = self.gateway.config()
                result: RunResult = self.execute()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes[target.SETUP])
                self.assertEqual([], self.config_writes())
                self.assertEqual(3, len(self.gateway.calls))
                self.assertEqual(custom, self.gateway.config())
                setattr(self.gateway, field, before[field])

    def test_partial_post_model_resets_omitted_custom_mqtt_settings(self) -> None:
        self.gateway.mqtt_data_format = "ruuvi_decoded"
        self.gateway.mqtt_prefix = "custom/"
        self.gateway.mqtt_client_id = "custom-client"
        self.gateway._apply_config(KEYS_BODY)
        self.assertEqual("ruuvi_raw", self.gateway.mqtt_data_format)
        self.assertEqual("ruuvi/AA:BB:CC:DD:EE:FF/", self.gateway.mqtt_prefix)
        self.assertEqual("AA:BB:CC:DD:EE:FF", self.gateway.mqtt_client_id)

    def test_redirect_rejects_external_origin_scheme_and_port_before_writes(self) -> None:
        location: str
        for location in ("http://another-host.example/#auth", "https://gateway.local/#auth",
                         "http://gateway.local:8080/#auth", "http://gateway.local:0/#auth",
                         "//another-host.example/#auth"):
            with self.subTest(location=location):
                self.gateway.calls.clear()
                self.gateway.redirect_location = location
                result: RunResult = self.execute()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
                self.assertEqual("PASS", result.outcomes[target.RESTORE])
                self.assertEqual([], self.config_writes())

    def test_relative_and_explicit_default_port_redirects_retain_dut_origin(self) -> None:
        location: str
        for location in ("/#auth", "http://gateway.local:80/#auth"):
            with self.subTest(location=location):
                self.gateway.redirect_location = location
                result: RunResult = self.execute()
                self.assertEqual((0, "PASS"), (result.exit_code, result.verdict), self.evidence)
                self.assert_restored()

    def test_negative_write_success_aborts_even_without_applied_mutation(self) -> None:
        body: dict[str, str]
        mechanism: str
        apply: bool
        for body, mechanism in ((U_BODY, AuthMech.LAN_WEBUI_DEFAULT), (RO_BODY, AuthMech.M2M_API_BEARER_RO)):
            for apply in (False, True):
                with self.subTest(body=body, apply=apply):
                    self.gateway.calls.clear()
                    self.gateway.write_faults = [WriteFault(body, status=204, apply=apply)]
                    result: RunResult = self.execute()
                    self.assertEqual(1, result.exit_code)
                    self.assertEqual("FAIL", result.outcomes[mechanism])
                    self.assertEqual("NOT RUN", result.outcomes[AuthMech.M2M_API_BEARER_RW])
                    bodies: list[Any] = [call.body for call in self.config_writes()]
                    self.assertNotIn(RW_BODY, bodies)
                    if body == U_BODY:
                        self.assertNotIn(KEYS_BODY, bodies)
                        self.assertNotIn(RO_BODY, bodies)
                    self.assert_restored()

    def test_401_with_mutation_is_fail_and_restored(self) -> None:
        self.gateway.write_faults = [WriteFault(RO_BODY, status=401, apply=True)]
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertNotIn(RW_BODY, [call.body for call in self.config_writes()])
        self.assert_restored()

    def test_ro_403_fails_documented_firmware_status_expectation(self) -> None:
        self.gateway.write_faults = [WriteFault(RO_BODY, status=403)]
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assert_restored()

    def test_rw_lost_commit_is_fail_and_preserves_completed_ro_pass(self) -> None:
        self.gateway.write_faults = [WriteFault(RW_BODY)]
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assert_restored()

    def test_rw_collateral_change_is_fail_with_final_difference_evidence(self) -> None:
        self.gateway.commit_side_effect = True
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("FAIL", result.outcomes[target.RESTORE])
        self.assertIn('"changed_fields": ["stable"]', self.evidence)
        self.assertIsNotNone(result.recovery_message)
        self.assert_restored()

    def test_provisioning_side_effect_or_partial_keys_is_setup_error_and_cleanup(self) -> None:
        self.gateway.provision_side_effect = True
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.SETUP])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assert_restored()

    def test_partial_provisioning_lost_response_uses_admin_fallback(self) -> None:
        self.gateway.partial_keys = True
        self.gateway.write_faults = [WriteFault(KEYS_BODY, apply=True, error=requests.Timeout("lost provision"))]
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.SETUP])
        self.assertEqual("PASS", result.outcomes[target.RESTORE])
        restores: list[RecordedRequest] = [call for call in self.config_writes() if call.body == RESTORE_BODY]
        self.assertEqual([RW, None], [call.bearer_token for call in restores])
        self.assert_restored()

    def test_late_positive_rw_read_failure_prevents_both_bearer_writes(self) -> None:
        self.gateway.read_overrides[6] = requests.ConnectionError("last safe read lost")
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.SETUP])
        self.assertNotIn(RO_BODY, [call.body for call in self.config_writes()])
        self.assertNotIn(RW_BODY, [call.body for call in self.config_writes()])
        self.assert_restored()

    def test_lost_marker_write_response_restores_after_error(self) -> None:
        self.gateway.write_faults = [WriteFault(RW_BODY, apply=True, error=requests.Timeout("lost commit"))]
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("PASS", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("ERROR", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assert_restored()

    def test_recovery_denial_or_lost_response_falls_back_in_order(self) -> None:
        body: dict[str, str]
        lost: bool
        for body in (RESTORE_BODY, CLEAR_BODY):
            for lost in (False, True):
                with self.subTest(body=body, lost=lost):
                    self.gateway.calls.clear()
                    self.gateway.write_faults = [WriteFault(body, status=401, apply=lost,
                                                          error=requests.Timeout("lost cleanup") if lost else None,
                                                          bearer_only=True)]
                    result: RunResult = self.execute()
                    self.assertEqual(0, result.exit_code, self.evidence)
                    restores: list[RecordedRequest] = [call for call in self.config_writes() if call.body == body]
                    self.assertEqual([RW, None], [call.bearer_token for call in restores])
                    self.assert_restored()

    def test_failed_marker_cleanup_still_disables_keys_and_prevents_pass(self) -> None:
        self.gateway.write_faults = [WriteFault(RESTORE_BODY, status=500, repeat=True)]
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.RESTORE])
        self.assertEqual("", self.gateway.ro_key)
        self.assertEqual("", self.gateway.rw_key)
        self.assertIsNotNone(result.recovery_message)
        self.assertIn(CLEAR_BODY, [call.body for call in self.config_writes()])

    def test_failed_key_cleanup_and_failed_final_login_are_errors(self) -> None:
        self.gateway.write_faults = [WriteFault(CLEAR_BODY, status=500, repeat=True)]
        self.gateway.restore_login_fails = True
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.RESTORE])
        self.assertIsNotNone(result.recovery_message)
        self.assertIn(CLEAR_BODY, [call.body for call in self.config_writes()])

    def test_fail_is_preserved_when_post_read_or_final_recovery_errors(self) -> None:
        self.gateway.write_faults = [WriteFault(RO_BODY, status=200)]
        self.gateway.read_overrides[7] = requests.Timeout("post denial read")
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertIn('"mechanism": "AuthMech-M2M-API-Bearer-RO", "result": "FAIL"', self.evidence)
        self.assert_restored()

    def test_malformed_json_timeout_connection_and_unexpected_exceptions_are_errors(self) -> None:
        value: Any
        for value in ("malformed", [], requests.Timeout("injected"), requests.ConnectionError("injected"),
                      RuntimeError("unexpected failure")):
            with self.subTest(value=value):
                self.gateway.calls.clear()
                self.gateway.read_count = 0
                self.gateway.read_overrides = {1: value}
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual([], self.config_writes())
                self.assertIn("Traceback", self.evidence)

    def test_revoked_keys_remaining_usable_is_fail(self) -> None:
        self.gateway.write_faults = [WriteFault(CLEAR_BODY)]
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.RESTORE])
        self.assertIsNotNone(result.recovery_message)

    def test_comparison_detects_independently_decoded_missing_unequal_and_bool_int(self) -> None:
        log: EvidenceLog = EvidenceLog.create(self.root / "direct", "comparison", lambda: NOW)
        self.addCleanup(log.finish, "PASS", lambda: NOW)
        runner: target.FunctionalTest_5_5_5_2_a = target.FunctionalTest_5_5_5_2_a(CONFIG, log, output=self.output.append)
        expected: dict[str, Any] = json.loads('{"coordinates":"", "flag":false, "nested":{"value":1}}')
        text: str
        for text in ('{"coordinates":"", "flag":0, "nested":{"value":1}}',
                     '{"coordinates":"", "flag":false, "nested":{"value":true}}',
                     '{"coordinates":"different", "flag":false, "nested":{"value":1}}',
                     '{"coordinates":"", "flag":false}',
                     '{"coordinates":0, "flag":false, "nested":{"value":1}}'):
            with self.subTest(text=text):
                self.assertFalse(runner._compare(expected, json.loads(text), "case comparison"))
        equal: dict[str, Any] = json.loads('{"nested":{"value":1}, "flag":false, "coordinates":""}')
        self.assertTrue(runner._compare(expected, equal, "case comparison"))

    def test_revocation_transport_status_is_unverified_recovery_error(self) -> None:
        # All ordinary reads and writes succeed, but the final revoked-key check cannot be interpreted.
        original: Callable[[FakeSession, RecordedRequest], FakeResponse | None] = self.gateway._response_override

        def override(session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            if self.gateway.restore_started and call.method == "GET" and call.bearer_token:
                return FakeResponse(500, {})
            return original(session, call)

        with mock.patch.object(self.gateway, "_response_override", side_effect=override):
            result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.RESTORE])
        self.assertIsNotNone(result.recovery_message)
        self.assert_restored()

    def test_recovery_post_read_failure_preserves_earlier_security_fail(self) -> None:
        self.gateway.write_faults = [WriteFault(RO_BODY, status=200)]
        self.gateway.restore_login_fails = True
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("ERROR", result.outcomes[target.RESTORE])
        self.assertIn(CLEAR_BODY, [call.body for call in self.config_writes()])

    def test_final_hash_bool_integer_difference_is_fail(self) -> None:
        # A valid, independently decoded final JSON must not equate integer zero with false.
        changed: dict[str, Any] = self.gateway.config()
        changed["lan_auth_api_key_use"] = 0
        self.gateway.read_overrides[11] = json.loads(json.dumps(changed))
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.RESTORE])
        self.assertIn('"changed_fields": ["lan_auth_api_key_use"]', self.evidence)

    def test_required_route_selection_rejects_missing_duplicate_and_noncanonical(self) -> None:
        selection: tuple[target.ApiRoute, ...]
        for selection in (
            target.ROUTE_SELECTION[:-1],
            (*target.ROUTE_SELECTION[:-1], target.ROUTE_SELECTION[0]),
            (*target.ROUTE_SELECTION[:-1], target.ApiRoute("GET", "/not-a-route")),
        ):
            with self.subTest(selection=selection), mock.patch.object(target, "ROUTE_SELECTION", selection):
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual([], self.gateway.calls)

    def test_failed_auth_safe_phase_prevents_mutation(self) -> None:
        original: Callable[[FakeSession, RecordedRequest], FakeResponse | None] = self.gateway._response_override

        def override(session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            if call.method == "GET" and call.path == "/auth" and session.number == 3:
                return FakeResponse(200, {})
            return original(session, call)

        with mock.patch.object(self.gateway, "_response_override", side_effect=override):
            result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
        self.assertEqual([], self.config_writes())
        self.assertEqual("PASS", result.outcomes[target.RESTORE])

    def test_main_returns_runner_exit_code(self) -> None:
        code: int
        for code in (0, 1, 2):
            with mock.patch.object(target, "execute_test_5_5_5_2_a", return_value=RunResult(code, "test", {}, set())):
                self.assertEqual(code, target.main())


if __name__ == "__main__":
    unittest.main()
