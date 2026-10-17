"""Offline implementation tests for Unit c; these are not ETSI or live-DUT evidence."""

from __future__ import annotations

import base64
import hashlib
import io
import json
import tempfile
import unittest
from collections.abc import Mapping
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, cast
from unittest import mock
from urllib.parse import urlsplit

import requests
from Crypto.Cipher import AES
from Crypto.PublicKey import ECC
from Crypto.Util.Padding import unpad

import test_5_5_4_2_c as runner
from lib.config import AUTHENTICATION_DEFAULT_FIELDS, FACTORY_RESET_MESSAGE, default_config_values
from lib.evidence import EvidenceLog
from lib.gateway import GatewayCfgLanAuthType
from lib.http_api import GatewayApi, HttpHeader, HttpMethod
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 1, 2, tzinfo=timezone.utc)
# Reference vectors from Web-UI elliptic/CryptoJS: client scalar 1, server scalar 2.
CLIENT_PUBLIC: str = "BGsX0fLhLEJH+Lzm5WOkQPJ3A32BLeszoPShOUXYmMKWT+NC4v4af5uO5+tKfA+eFivOM1drMV7Oy7ZAaDe/UfU="
SERVER_PUBLIC: str = "BHzyexiNA09+ilI4AwS1GsPAiWnid/IbNaYLSPxHZpl4B3dVENuO0EApPZrGn3Qw27p9reY86YIpngS3nSJ4c9E="
AES_KEY: bytes = bytes.fromhex("23775201799b2234a18e8071e409cec80d42632fe77534180afdc533c9b76f81")
MUTATION: dict[str, str] = {
    "encrypted": "PRnzzoNy9bgU/sjvC8xhZb3w0NTm20f1HsE8OdAI749mD5bhF57KgwZwL3+IoaY+",
    "iv": "AAECAwQFBgcICQoLDA0ODw==", "hash": "p+j1hr/ITqOWX2pqocvA3AMAcmawVCgT+B5Un7AMPoI=",
}
RESTORATION: dict[str, str] = {
    "encrypted": "PRnzzoNy9bgU/sjvC8xhZRlLbnwVHN6sBmm3NuE19HQ=",
    "iv": "AAECAwQFBgcICQoLDA0ODw==", "hash": "v/f7mhEBpnMWX07O8bvVwki0QGDHITfbUTjCZgabhXg=",
}


class EncryptedGateway(DefaultAuthGateway):
    """Case-local encrypted-write and recovery fault model; shared fake owns login validation."""

    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.server_public_b64 = SERVER_PUBLIC
        self.state: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        self.state.update({"gw_mac": CONFIG.gw_mac, "coordinates": "original", "unrelated": {"flag": False},
                           "mqtt_data_format": "ruuvi_raw", "mqtt_prefix": f"ruuvi/{CONFIG.gw_mac}/",
                           "mqtt_client_id": CONFIG.gw_mac})
        self.original: dict[str, Any] = json.loads(json.dumps(self.state))
        self.prepared: list[requests.PreparedRequest] = []
        self.sessions: list[EncryptedSession] = []
        self.plain_bodies: list[dict[str, Any]] = []
        self.pending_body: dict[str, Any] | None = None
        self.posts: int = 0
        self.reads: int = 0
        self.reject_mutation: bool = False
        self.ignore_mutation: bool = False
        self.after_write_error: Exception | None = None
        self.read_error_at: int = 0
        self.malformed_at: int = 0
        self.side_effect: bool = False
        self.restore_from_attempt: int = 1
        self.restore_attempts: int = 0
        self.fail_fresh_login: bool = False
        self.corrupt_cookie: bool = False
        self.login_denied: bool = False
        self.leak_at: int = 0
        self.leak_location: str = "header"
        self.baseline_unstable: bool = False
        self.corrupt_login: bool = False
        self.restore_error: bool = False
        self.restore_reply_lost: bool = False
        self.mutation_status: int = 200

    def session(self) -> requests.Session:
        session: EncryptedSession = EncryptedSession(self)
        self.sessions.append(session)
        return session

    def response_for(
        self, session: FakeSession, method: str, path: str, headers: Mapping[str, str],
        body: Any, allow_redirects: bool = False,
    ) -> FakeResponse:
        if (path == GatewayApi.AUTH and method == HttpMethod.GET
                and headers.get(HttpHeader.RUUVI_ECDH_PUBLIC_KEY) != CLIENT_PUBLIC):
            raise AssertionError("client public key differs from independent P-256 vector")
        self.pending_body = None
        if path == GatewayApi.CONFIG and method == HttpMethod.POST:
            if headers.get(HttpHeader.RUUVI_ECDH_ENCRYPTED) == "true":
                if set(body) != {"encrypted", "iv", "hash"}:
                    raise AssertionError("incorrect encrypted envelope fields")
                plaintext: bytes = unpad(AES.new(AES_KEY, AES.MODE_CBC, base64.b64decode(body["iv"])).decrypt(
                    base64.b64decode(body["encrypted"])), 16)
                if base64.b64encode(hashlib.sha256(plaintext).digest()).decode() != body["hash"]:
                    raise AssertionError("invalid plaintext SHA-256")
                self.pending_body = json.loads(plaintext)
            else:
                self.pending_body = body
            if self.pending_body is None or set(self.pending_body) != {"coordinates"}:
                raise AssertionError("write escaped coordinates scope")
        return super().response_for(session, method, path, headers, body, allow_redirects)

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        if (call.path == GatewayApi.AUTH and call.method == HttpMethod.POST
                and (self.login_denied or (self.fail_fresh_login and session.number > 1))):
            return FakeResponse(401, {})
        if call.path == GatewayApi.CONFIG and call.method == HttpMethod.POST:
            self.posts += 1
            if not self._is_session_authorized(session, {HttpHeader.COOKIE: call.cookie or ""}):
                return FakeResponse(401, {})
            if self.pending_body is None:
                raise AssertionError("missing decoded coordinates")
            self.plain_bodies.append(self.pending_body)
            if self.posts == 1:
                if self.reject_mutation:
                    return FakeResponse(400, {})
                if self.mutation_status != 200:
                    return FakeResponse(self.mutation_status, {})
                if not self.ignore_mutation:
                    self.state.update(self.pending_body)
                if self.side_effect:
                    self.state["unrelated"] = {"flag": 0}
                if self.after_write_error is not None:
                    raise self.after_write_error
            else:
                self.restore_attempts += 1
                if self.restore_error:
                    raise requests.ConnectionError("restore unavailable")
                if self.restore_attempts < self.restore_from_attempt:
                    return FakeResponse(400, {})
                self.state.update(self.pending_body)
                if self.restore_reply_lost:
                    raise requests.Timeout("restoration applied, response lost")
            # The real partial parser resets these omitted fields for every accepted POST.
            self.state.update({"mqtt_data_format": "ruuvi_raw", "mqtt_prefix": f"ruuvi/{CONFIG.gw_mac}/",
                               "mqtt_client_id": CONFIG.gw_mac})
            return FakeResponse(200, {})
        return None

    def _interactive_challenge_response(self, session: FakeSession, include_ecdh_public_key: bool) -> FakeResponse:
        response: FakeResponse = super()._interactive_challenge_response(session, include_ecdh_public_key)
        if self.corrupt_cookie:
            response.cookies.set("RUUVISESSION", "wrong")
        return response

    def _config_response(self) -> FakeResponse:
        self.reads += 1
        if self.reads == self.read_error_at:
            raise requests.Timeout("read failed")
        if self.reads == self.malformed_at:
            return FakeResponse(200, malformed_json=True)
        payload: dict[str, Any] = json.loads(json.dumps(self.state))
        if self.baseline_unstable and self.reads == 2:
            payload["unrelated"] = {"flag": 0}
        return FakeResponse(200, payload)


class EncryptedSession(FakeSession):
    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        gateway: EncryptedGateway = cast(EncryptedGateway, self.gateway)
        if len(gateway.prepared) + 1 == gateway.leak_at:
            if gateway.leak_location == "url":
                request.params = {"leaked": CONFIG.gw_id}
            elif gateway.leak_location == "body":
                request.json = {"leaked": CONFIG.gw_id}
            else:
                request.headers["X-Leaked-Password"] = CONFIG.gw_id
        if gateway.corrupt_login and request.method == "POST" and request.url.endswith("/auth"):
            request.json = {"login": "Admin", "password": "0" * 64}
        prepared: requests.PreparedRequest = super().prepare_request(request)
        return prepared

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        gateway: EncryptedGateway = cast(EncryptedGateway, self.gateway)
        gateway.prepared.append(request)
        if self.trust_env or kwargs != {"timeout": (5, 15), "allow_redirects": False}:
            raise AssertionError("unexpected transport options")
        return super().send(request, **kwargs)


class UnitCImplementationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work_dir: Path = Path(self.directory.name)
        (self.work_dir / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.gateway: EncryptedGateway = EncryptedGateway()
        self.output: list[str] = []
        self.log: str = ""

    def execute(self) -> RunResult:
        result: RunResult = runner.execute_test_5_5_4_2_c(
            self.work_dir, self.gateway.session, lambda size: bytes(range(size)),
            lambda **kwargs: ECC.construct(curve="P-256", d=1), lambda size: bytes(range(size)),
            lambda: NOW, self.output.append,
        )
        paths: list[Path] = sorted((self.work_dir / "logs").glob("*.log"))
        self.log = paths[-1].read_text(encoding="utf-8")
        return result

    def test_full_run_exact_prepared_order_bodies_sessions_and_evidence(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual({check: "PASS" for check in runner.CHECKS}, result.outcomes)
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        response: str = hashlib.sha256(f"challenge-1-1:{ha1}".encode()).hexdigest()
        expected: list[tuple[str, str, Any]] = [
            ("GET", "/auth", None), ("POST", "/auth", {"login": "Admin", "password": response}),
            ("GET", "/ruuvi.json", None), ("GET", "/ruuvi.json", None),
            ("POST", "/ruuvi.json", MUTATION), ("GET", "/ruuvi.json", None),
            ("POST", "/ruuvi.json", RESTORATION), ("GET", "/ruuvi.json", None),
        ]
        self.assertEqual(expected, [(call.method, call.path, call.body) for call in self.gateway.calls])
        self.assertEqual([("none", 1, False)] * 8,
                         [(call.scheme, call.session_number, call.allow_redirects) for call in self.gateway.calls])
        self.assertEqual([None] + ["RUUVISESSION=session-1"] * 7, [call.cookie for call in self.gateway.calls])
        self.assertEqual(["http"] * 8, [urlsplit(request.url or "").scheme for request in self.gateway.prepared])
        self.assertEqual(CLIENT_PUBLIC, self.gateway.prepared[0].headers[HttpHeader.RUUVI_ECDH_PUBLIC_KEY])
        self.assertEqual("true", self.gateway.prepared[4].headers[HttpHeader.RUUVI_ECDH_ENCRYPTED])
        self.assertEqual(self.gateway.original, self.gateway.state)
        self.assertEqual([{"coordinates": "cra-AAECAwQFBgcI"}, {"coordinates": "original"}], self.gateway.plain_bodies)
        self.assertEqual(1, sum(line.startswith("Open log file: ") for line in self.output))
        progress: list[str] = [line for line in self.output if line.startswith("[Step")]
        self.assertEqual(8, len(progress))
        index: int
        line: str
        for index, line in enumerate(progress, 1):
            self.assertTrue(line.startswith(f"[Step {index} out of 8]"))
            self.assertIn(line, self.log)
        self.assertIn("TLS not claimed", self.log)
        self.assertIn('"response":', self.log)
        self.assertIn("ECDH SHARED SECRET", self.log)
        self.assertIn("FULL CONFIGURATION SHA256", self.log)
        self.assertNotIn(CONFIG.gw_id, "\n".join(self.output))
        self.assertNotIn(AES_KEY.hex(), "\n".join(self.output))
        self.assertEqual("Overall verdict: PASS", self.output[-1])

    def test_invalid_env_has_evidence_without_network(self) -> None:
        (self.work_dir / ".env").unlink()
        result: RunResult = self.execute()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.outcomes[runner.SETUP]))
        self.assertFalse(self.gateway.calls)
        self.assertIn("TLS not claimed", self.log)
        self.assertIn("EXCEPTION TYPE", self.log)
        self.assertEqual("Overall verdict: ERROR", self.output[-1])

    def test_parser_sensitive_mqtt_baselines_abort_without_post(self) -> None:
        field: str
        for field in ("mqtt_data_format", "mqtt_prefix", "mqtt_client_id"):
            variant: Any
            for variant in ("custom", None, False):
                with self.subTest(field=field, value=variant):
                    self.gateway = EncryptedGateway()
                    self.gateway.state[field] = variant
                    result: RunResult = self.execute()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.outcomes[runner.SETUP]))
                    self.assertEqual(0, self.gateway.posts)
                    self.assertEqual(0, self.gateway.restore_attempts)

    def test_device_mqtt_defaults_use_canonical_verified_mac(self) -> None:
        config: DutConfig = DutConfig(CONFIG.gw_id, CONFIG.gw_mac.lower(), CONFIG.gw_hostname)
        (self.work_dir / ".env").write_text(env_text(config), encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual(0, result.exit_code)
        self.assertEqual(self.gateway.original, self.gateway.state)

    def test_reporting_failure_after_mutation_cannot_block_cleanup(self) -> None:
        boundary: str
        failure: bool
        for boundary in ("evidence", "terminal", "progress"):
            for failure in (False, True):
                with self.subTest(boundary=boundary, restoration_fails=failure):
                    self.gateway = EncryptedGateway()
                    if failure:
                        self.gateway.restore_from_attempt = 100
                    evidence: EvidenceLog = EvidenceLog(Path("memory"), io.StringIO(), NOW)
                    write: Callable[[str, Any], None] = evidence.write

                    def reporting_write(
                        label: str, value: Any = "", selected: str = boundary,
                        delegate: Callable[[str, Any], None] = write,
                    ) -> None:
                        if selected == "evidence" and self.gateway.posts:
                            raise OSError("evidence disk full")
                        delegate(label, value)

                    def reporting_output(_message: str, selected: str = boundary) -> None:
                        if selected == "terminal" and self.gateway.posts:
                            raise BrokenPipeError("closed terminal")

                    def reporting_progress(_message: str, selected: str = boundary) -> None:
                        if selected == "progress" and self.gateway.posts:
                            raise BrokenPipeError("closed progress output")

                    with mock.patch.object(evidence, "write", side_effect=reporting_write):
                        test: runner.FunctionalTest_5_5_4_2_c = runner.FunctionalTest_5_5_4_2_c(
                            CONFIG, evidence, self.gateway.session, lambda size: bytes(range(size)),
                            lambda **kwargs: ECC.construct(curve="P-256", d=1), lambda size: bytes(range(size)),
                            reporting_progress, reporting_output,
                        )
                        result: RunResult = test.run()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                    self.assertGreater(self.gateway.restore_attempts, 0)
                    self.assertEqual(("GET", "/ruuvi.json"),
                                     (self.gateway.calls[-1].method, self.gateway.calls[-1].path))
                    if failure:
                        self.assertEqual(runner.RECOVERY_MESSAGE, result.recovery_message)
                    else:
                        self.assertEqual("PASS", result.outcomes[runner.RESTORE])
                        self.assertEqual(self.gateway.original, self.gateway.state)

    def test_execute_persistent_reporting_failure_retains_result_and_recovery_advice(self) -> None:
        boundary: str
        for boundary in ("evidence", "terminal"):
            with self.subTest(boundary=boundary):
                self.gateway = EncryptedGateway()
                self.gateway.restore_from_attempt = 100
                write: Callable[[EvidenceLog, str, Any], None] = EvidenceLog.write

                def reporting_write(
                    log: EvidenceLog, label: str, value: Any = "", selected: str = boundary,
                    delegate: Callable[[EvidenceLog, str, Any], None] = write,
                ) -> None:
                    if selected == "evidence" and self.gateway.posts:
                        raise OSError("persistent disk failure")
                    delegate(log, label, value)

                def reporting_output(message: str, selected: str = boundary) -> None:
                    if selected == "terminal" and self.gateway.posts:
                        raise BrokenPipeError("persistent closed terminal")
                    self.output.append(message)

                with mock.patch.object(EvidenceLog, "write", reporting_write):
                    result: RunResult = runner.execute_test_5_5_4_2_c(
                        self.work_dir, self.gateway.session, lambda size: bytes(range(size)),
                        lambda **kwargs: ECC.construct(curve="P-256", d=1), lambda size: bytes(range(size)),
                        lambda: NOW, reporting_output,
                    )
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual(runner.RECOVERY_MESSAGE, result.recovery_message)
                self.assertEqual(3, self.gateway.restore_attempts)
                self.assertEqual(("GET", "/ruuvi.json"),
                                 (self.gateway.calls[-1].method, self.gateway.calls[-1].path))

    def test_config_writes_reject_missing_and_cross_session_cookies(self) -> None:
        encrypted: bool
        cookie_kind: str
        for encrypted in (True, False):
            for cookie_kind in ("missing", "cross-session"):
                with self.subTest(encrypted=encrypted, cookie=cookie_kind):
                    self.gateway = EncryptedGateway()
                    test: runner.FunctionalTest_5_5_4_2_c = runner.FunctionalTest_5_5_4_2_c(
                        CONFIG, EvidenceLog(Path("memory"), io.StringIO(), NOW), self.gateway.session,
                        lambda size: bytes(range(size)), lambda **kwargs: ECC.construct(curve="P-256", d=1),
                        output=lambda message: None,
                    )
                    auth: runner.InteractiveAuthResult = test._login()
                    other: runner.InteractiveAuthResult = test._login()
                    auth.session.cookies.clear()
                    headers: dict[str, str] = {HttpHeader.RUUVI_ECDH_ENCRYPTED: "true"} if encrypted else {}
                    cookie: str | None = None
                    if cookie_kind == "cross-session":
                        headers[HttpHeader.COOKIE] = f"RUUVISESSION={other.cookie}"
                        cookie = headers[HttpHeader.COOKIE]
                    response: requests.Response = test.gateway.request(
                        auth.session, HttpMethod.POST, GatewayApi.CONFIG, headers=headers,
                        json_body=MUTATION if encrypted else {"coordinates": "changed"},
                    )
                    self.assertEqual(401, response.status_code)
                    self.assertEqual(cookie, self.gateway.calls[-1].cookie)
                    self.assertTrue(cast(FakeSession, auth.session).authorized)
                    self.assertEqual(self.gateway.original, self.gateway.state)
                    self.assertEqual([], self.gateway.plain_bodies)
                    self.assertEqual(0, self.gateway.restore_attempts)

    def test_baseline_validator_json_value_and_type_matrix(self) -> None:
        defaults: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        field: str
        expected: Any
        for field, expected in defaults.items():
            variant: str
            for variant in ("equal", "unequal", "missing", "wrong-type"):
                with self.subTest(field=field, variant=variant):
                    payload: dict[str, Any] = json.loads(json.dumps(self.gateway.original))
                    if variant == "missing":
                        del payload[field]
                    elif variant == "unequal":
                        payload[field] = not expected if type(expected) is bool else "different"
                    elif variant == "wrong-type":
                        payload[field] = 0 if type(expected) is bool else False
                    stream: io.StringIO = io.StringIO()
                    evidence: EvidenceLog = EvidenceLog(Path("memory"), stream, NOW)
                    test: runner.FunctionalTest_5_5_4_2_c = runner.FunctionalTest_5_5_4_2_c(
                        CONFIG, evidence, output=lambda message: None,
                    )
                    if variant == "equal":
                        test._validate_baseline(payload, {"lan_auth_type": json.loads('"lan_auth_default"')})
                    else:
                        with self.assertRaises(runner.InvalidSetup):
                            test._validate_baseline(payload, {"lan_auth_type": "lan_auth_default"})
                        self.assertEqual(FACTORY_RESET_MESSAGE, test.recovery_message)

    def test_identity_precedes_nondefault_reset_advice(self) -> None:
        mac: Any
        for mac in (None, "11:22:33:44:55:66", "invalid", False):
            with self.subTest(mac=mac):
                self.gateway = EncryptedGateway()
                if mac is None:
                    del self.gateway.state["gw_mac"]
                else:
                    self.gateway.state["gw_mac"] = mac
                self.gateway.state["lan_auth_api_key_use"] = True
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertIsNone(result.recovery_message)
                self.assertEqual(0, self.gateway.posts)

    def test_basic_digest_and_rejected_login_request_factory_reset(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST, GatewayCfgLanAuthType.DEFAULT):
            with self.subTest(mode=mode):
                self.gateway = EncryptedGateway()
                self.gateway.mode = mode
                self.gateway.login_denied = True
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
                self.assertIn(FACTORY_RESET_MESSAGE, self.log)
                self.assertIn(FACTORY_RESET_MESSAGE, self.output)
                self.assertEqual(0, self.gateway.reads)
                self.assertEqual(0, self.gateway.posts)

    def test_protocol_and_transport_setup_errors_prevent_mutations(self) -> None:
        fault: str
        for fault in ("challenge", "cookie", "key", "json", "timeout", "connection", "unstable", "coordinates"):
            with self.subTest(fault=fault):
                self.gateway = EncryptedGateway()
                if fault == "challenge":
                    self.gateway.malformed_challenge = True
                elif fault == "cookie":
                    self.gateway.corrupt_cookie = True
                elif fault == "key":
                    self.gateway.server_public_b64 = "bad"
                elif fault == "json":
                    self.gateway.malformed_at = 1
                elif fault == "timeout":
                    self.gateway.connection_error = requests.Timeout("timeout")
                elif fault == "connection":
                    self.gateway.connection_error = requests.ConnectionError("offline")
                elif fault == "unstable":
                    self.gateway.baseline_unstable = True
                else:
                    self.gateway.state["coordinates"] = False
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual("ERROR", result.outcomes[runner.SETUP])
                self.assertEqual("NOT RUN", result.outcomes[runner.ENVELOPE])
                self.assertEqual(0, self.gateway.posts)
                self.assertIn("EXCEPTION TYPE", self.log)

    def test_rejected_or_ineffective_envelope_fails_and_restores(self) -> None:
        reject: bool
        for reject in (True, False):
            with self.subTest(reject=reject):
                self.gateway = EncryptedGateway()
                self.gateway.reject_mutation = reject
                self.gateway.ignore_mutation = not reject
                result: RunResult = self.execute()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.outcomes[runner.ENVELOPE]))
                self.assertEqual("PASS", result.outcomes[runner.RESTORE])
                self.assertEqual("PASS", result.outcomes[runner.LOGIN])
                self.assertEqual(RESTORATION, self.gateway.calls[-2].body)
                self.assertEqual(self.gateway.original, self.gateway.state)
                self.assertIn('"result": "FAIL"', self.log)

    def test_partial_mutation_and_read_errors_always_restore(self) -> None:
        fault: str
        for fault in ("timeout", "unexpected", "read", "json"):
            with self.subTest(fault=fault):
                self.gateway = EncryptedGateway()
                if fault == "timeout":
                    self.gateway.after_write_error = requests.Timeout("applied, response lost")
                elif fault == "unexpected":
                    self.gateway.after_write_error = RuntimeError("unexpected boundary error")
                elif fault == "read":
                    self.gateway.read_error_at = 3
                else:
                    self.gateway.malformed_at = 3
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual("ERROR", result.outcomes[runner.ENVELOPE])
                self.assertEqual("PASS", result.outcomes[runner.RESTORE])
                self.assertEqual(RESTORATION, self.gateway.calls[-2].body)
                self.assertEqual(self.gateway.original, self.gateway.state)

    def test_restore_reauth_and_cleartext_fallback_exact_scope(self) -> None:
        self.gateway.restore_from_attempt = 3
        result: RunResult = self.execute()
        self.assertEqual(0, result.exit_code)
        self.assertEqual([1, 1, 2, 2], [call.session_number for call in self.gateway.calls if call.method == "POST"
                                      and call.path == "/ruuvi.json"])
        self.assertEqual([MUTATION, RESTORATION, RESTORATION, {"coordinates": "original"}],
                         [call.body for call in self.gateway.calls if call.method == "POST" and call.path == "/ruuvi.json"])
        self.assertEqual(2, len(self.gateway.sessions))
        self.assertEqual(self.gateway.original, self.gateway.state)

    def test_mixed_case_hostname_uses_prepared_url_for_login(self) -> None:
        config: DutConfig = DutConfig(CONFIG.gw_id, CONFIG.gw_mac, "Gateway.LOCAL")
        (self.work_dir / ".env").write_text(env_text(config), encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual((0, "PASS"), (result.exit_code, result.outcomes[runner.LOGIN]))
        self.assertEqual("PASS", result.outcomes[runner.ENVELOPE])
        self.assertEqual("http://gateway.local/auth", self.gateway.prepared[1].url)

    def test_applied_restoration_lost_reply_is_verified_before_retry(self) -> None:
        attempt: int
        for attempt in (1, 2, 3):
            with self.subTest(attempt=attempt):
                self.gateway = EncryptedGateway()
                self.gateway.restore_from_attempt = attempt
                self.gateway.restore_reply_lost = True
                result: RunResult = self.execute()
                self.assertEqual((0, "PASS"), (result.exit_code, result.outcomes[runner.RESTORE]))
                self.assertEqual(attempt, self.gateway.restore_attempts)
                self.assertEqual(self.gateway.original, self.gateway.state)
                self.assertEqual(("GET", "/ruuvi.json"),
                                 (self.gateway.calls[-1].method, self.gateway.calls[-1].path))
                self.assertEqual(1 if attempt == 1 else 2, len(self.gateway.sessions))
                self.assertIsNone(result.recovery_message)
                self.assertIn("restoration applied, response lost", self.log)

    def test_failed_restore_and_unrelated_side_effect_preserve_earlier_fail(self) -> None:
        side_effect: bool
        for side_effect in (True, False):
            with self.subTest(side_effect=side_effect):
                self.gateway = EncryptedGateway()
                self.gateway.side_effect = side_effect
                if not side_effect:
                    self.gateway.restore_from_attempt = 100
                    self.gateway.read_error_at = 3
                result: RunResult = self.execute()
                self.assertEqual(2, result.exit_code)
                self.assertEqual("FAIL", result.outcomes[runner.ENVELOPE] if side_effect else result.outcomes[runner.RESTORE])
                self.assertEqual("FAIL", result.outcomes[runner.RESTORE])
                self.assertEqual(runner.RECOVERY_MESSAGE, result.recovery_message)
                self.assertIn(runner.RECOVERY_MESSAGE, self.output)
                self.assertIn(runner.RECOVERY_MESSAGE, self.log)

    def test_plaintext_in_url_header_or_body_fails_even_on_final_read(self) -> None:
        location: str
        for location in ("url", "header", "body"):
            with self.subTest(location=location):
                self.gateway = EncryptedGateway()
                self.gateway.leak_at = 8
                self.gateway.leak_location = location
                result: RunResult = self.execute()
                self.assertEqual(1, result.exit_code)
                self.assertEqual("FAIL", result.outcomes[runner.PLAINTEXT])
                self.assertEqual("PASS", result.outcomes[runner.RESTORE])
                self.assertNotIn(CONFIG.gw_id, "\n".join(self.output))
                if location == "url":
                    self.assertNotIn(CONFIG.gw_id, self.gateway.prepared[-1].url or "")
                    self.assertIn("00%3A11%3A22%3A33%3A44%3A55%3A66%3A77", self.gateway.prepared[-1].url or "")

    def test_corrupt_login_response_is_rejected_by_independent_fake(self) -> None:
        self.gateway.corrupt_login = True
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual([("GET", "/auth"), ("POST", "/auth")],
                         [(call.method, call.path) for call in self.gateway.calls])
        self.assertFalse(self.gateway.sessions[0].authorized)

    def test_prior_envelope_fail_survives_recovery_write_error_with_baseline_verified(self) -> None:
        self.gateway.reject_mutation = True
        self.gateway.restore_error = True
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[runner.ENVELOPE])
        self.assertEqual("PASS", result.outcomes[runner.RESTORE])
        self.assertEqual("PASS", result.outcomes[runner.LOGIN])
        self.assertEqual(1, self.gateway.restore_attempts)
        self.assertIn('"mechanism": "AES-CBC envelope acceptance and effect", "result": "FAIL"', self.log)
        self.assertIn("EXCEPTION TYPE: GatewayConnectionError", self.log)

    def test_server_error_is_error_and_still_restores(self) -> None:
        self.gateway.mutation_status = 500
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[runner.ENVELOPE])
        self.assertEqual("PASS", result.outcomes[runner.RESTORE])
        self.assertEqual(RESTORATION, self.gateway.calls[-2].body)

    def test_auth_mode_and_coordinates_validator_json_type_matrix(self) -> None:
        field: str
        for field in ("coordinates", "lan_auth_type"):
            variant: str
            for variant in ("equal", "unequal", "missing", "wrong-type"):
                with self.subTest(field=field, variant=variant):
                    payload: dict[str, Any] = json.loads(json.dumps(self.gateway.original))
                    auth_payload: dict[str, Any] = json.loads('{"lan_auth_type":"lan_auth_default"}')
                    target: dict[str, Any] = payload if field == "coordinates" else auth_payload
                    if variant == "unequal":
                        target[field] = "different"
                    elif variant == "missing":
                        del target[field]
                    elif variant == "wrong-type":
                        target[field] = 0
                    test: runner.FunctionalTest_5_5_4_2_c = runner.FunctionalTest_5_5_4_2_c(
                        CONFIG, EvidenceLog(Path("memory"), io.StringIO(), NOW), output=lambda message: None,
                    )
                    if variant == "equal" or (field == "coordinates" and variant == "unequal"):
                        # Coordinates is restored to the observed original, not a hard-coded default.
                        test._validate_baseline(payload, auth_payload)
                    else:
                        with self.assertRaises(runner.InvalidSetup):
                            test._validate_baseline(payload, auth_payload)

    def test_configuration_comparison_rejects_boolean_integer_and_nested_changes(self) -> None:
        expected: dict[str, Any] = json.loads('{"coordinates":"original","nested":{"flag":false}}')
        altered: str
        for altered in ('{"coordinates":"original","nested":{"flag":false}}',
                        '{"coordinates":"original","nested":{"flag":0}}',
                        '{"coordinates":"different","nested":{"flag":false}}',
                        '{"coordinates":"original"}'):
            with self.subTest(altered=altered):
                test: runner.FunctionalTest_5_5_4_2_c = runner.FunctionalTest_5_5_4_2_c(
                    CONFIG, EvidenceLog(Path("memory"), io.StringIO(), NOW), output=lambda message: None,
                )
                if altered == '{"coordinates":"original","nested":{"flag":false}}':
                    test._compare(json.loads(altered), expected)
                else:
                    with self.assertRaises(runner.ProtectionFailure):
                        test._compare(json.loads(altered), expected)

    def test_login_password_leak_aborts_before_coordinates_mutation(self) -> None:
        self.gateway.leak_at = 2
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[runner.PLAINTEXT])
        self.assertEqual("PASS", result.outcomes[runner.LOGIN])
        self.assertEqual("NOT RUN", result.outcomes[runner.ENVELOPE])
        self.assertEqual(0, self.gateway.posts)
        self.assertEqual("PASS", result.outcomes[runner.RESTORE])

    def test_encrypted_request_header_leak_fails_and_still_restores(self) -> None:
        self.gateway.leak_at = 5
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[runner.PLAINTEXT])
        self.assertEqual("PASS", result.outcomes[runner.RESTORE])
        self.assertEqual(RESTORATION, self.gateway.calls[-2].body)
        self.assertEqual(self.gateway.original, self.gateway.state)

    def test_main_returns_execution_exit_code(self) -> None:
        code: int
        verdict: str
        for code, verdict in ((0, "PASS"), (1, "FAIL"), (2, "ERROR")):
            with self.subTest(code=code), mock.patch.object(
                runner, "execute_test_5_5_4_2_c", return_value=RunResult(code, verdict, {}, set()),
            ):
                self.assertEqual(code, runner.main())


if __name__ == "__main__":
    unittest.main()
