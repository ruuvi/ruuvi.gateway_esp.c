"""Implementation tests only; these are not ETSI compliance evidence."""

from __future__ import annotations

import base64
import json
import sys
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable
from urllib.parse import urlsplit

import requests
from Crypto.PublicKey import ECC
from lib.models import RunResult

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_5_1_2a_2_b as target
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
)
from lib.http_api import HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus

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


class FakeCookies:
    def __init__(self, values: dict[str, str] | None = None) -> None:
        self.values: dict[str, str] = values or {}

    def get(self, key: str) -> str | None:
        return self.values.get(key)

    def get_dict(self) -> dict[str, str]:
        return dict(self.values)


class FakeResponse:
    def __init__(
        self,
        status: int,
        payload: Any = None,
        headers: dict[str, str] | None = None,
        cookies: dict[str, str] | None = None,
        malformed_json: bool = False,
    ) -> None:
        self.status_code: int = status
        self.headers: dict[str, str] = headers or {}
        self.cookies: FakeCookies = FakeCookies(cookies)
        self._payload: Any = payload
        self._malformed_json: bool = malformed_json
        self.text: str = "{broken" if malformed_json else json.dumps(payload) if payload is not None else ""

    def json(self) -> Any:
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
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
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
            session_id: str = f"session-{session.number}"
            header: str = (
                'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="challenge" '
                f'session_cookie="RUUVISESSION" session_id="{session_id}"'
            )
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT},
                headers={
                    HttpHeader.WWW_AUTHENTICATE: header,
                    HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.server_public_b64,
                },
                cookies={"RUUVISESSION": session_id},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.POST:
            if isinstance(body, dict) and body.get("login") == target.ADMIN_USERNAME:
                session.authorized = True
                return FakeResponse(HttpStatus.C_200_OK, {"authenticated": True})
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"authenticated": False})

        bearer: str | None = self.token(authorization)
        read_allowed: bool = bearer in {self.ro_key, self.rw_key} and bearer != ""
        write_allowed: bool = bearer == self.rw_key and bearer != ""
        authorized_read: bool = session.authorized or read_allowed
        authorized_write: bool = session.authorized or write_allowed

        if path == GatewayApi.CONFIG and method == HttpMethod.GET and authorized_read:
            if self.malformed_config:
                return FakeResponse(HttpStatus.C_200_OK, malformed_json=True)
            return FakeResponse(HttpStatus.C_200_OK, self.config())
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


class FakeSession:
    next_number: int = 1

    def __init__(self, gateway: FakeGateway) -> None:
        self.gateway: FakeGateway = gateway
        self.authorized: bool = False
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
    def write_env(self, root: Path) -> None:
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

    def test_exit_code_aggregation(self) -> None:
        self.assertEqual(0, target.RunResult(0, "PASS", {}, set()).exit_code)
        self.assertEqual(1, target.RunResult(1, "FAIL", {}, set()).exit_code)
        self.assertEqual(2, target.RunResult(2, "ERROR", {}, set()).exit_code)


if __name__ == "__main__":
    unittest.main()
