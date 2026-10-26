"""Implementation tests only; these are not ETSI compliance evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Mapping, cast
from unittest import mock
from urllib.parse import urlsplit

import requests

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
from test_support.fake_gateway import (
    DefaultAuthGateway,
    FakeResponse,
    FakeSession,
    RecordedRequest,
)

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


class BearerGateway(DefaultAuthGateway):
    """Fault scenarios for bearer provisioning, scope checks, and restoration."""

    def __init__(self, config: target.DutConfig) -> None:
        super().__init__(config)
        self.config_reads: int = 0
        self.config_overrides: dict[int, dict[str, Any]] = {}
        self.restore_admin_failures: int = 0
        self.restore_rw_failure: bool = False
        self.override: dict[tuple[str, str, str], FakeResponse | BaseException] = {}
        self.malformed_config: bool = False
        self.mutate_after_positive: bool = False
        self.ntp_use: bool = True

    def response_for(
        self,
        session: FakeSession,
        method: str,
        path: str,
        headers: Mapping[str, str],
        body: Any,
        allow_redirects: bool = False,
    ) -> FakeResponse:
        response: FakeResponse = super().response_for(session, method, path, headers, body, allow_redirects)
        if (
            path in (GatewayApi.HISTORY, GatewayApi.CONFIG)
            and response.status_code == HttpStatus.C_401_UNAUTHORIZED
            and response.text == '{"error": "unauthorized"}'
        ):
            # Firmware http_server_fill_auth_json() emits public metadata for
            # invalid/revoked bearers, RO-on-RW denial, and password-only POSTs.
            return FakeResponse(401, {
                "gateway_name": "RuuviGatewayEEFF",
                "lan_auth_type": "lan_auth_bearer" if self._bearer(headers) is not None else "lan_auth_default",
                "lan": True,
            })
        return response

    def config(self) -> dict[str, Any]:
        return {
            GatewayCfgDesc.LAN_AUTH_TYPE: self.mode,
            GatewayCfgDesc.LAN_AUTH_USER: self.custom_username,
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: bool(self.ro_key),
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: bool(self.rw_key),
            GatewayCfgDesc.GW_MAC: self.dut_config.gw_mac,
            GatewayCfgDesc.FW_VER: "test-fw",
            GatewayCfgDesc.NRF52_FW_VER: "test-nrf",
            "stable": "mutated" if self.mutate_after_positive else "value",
            "ntp_use": self.ntp_use,
            "extension": [{"baseline_nested_field": "private-value"}],
        }

    def _read_response(self, path: str) -> FakeResponse:
        if path == GatewayApi.HISTORY:
            data: dict[str, Any] = {
                "coordinates": "",
                "gw_mac": self.dut_config.gw_mac,
                "tags": {},
            }
            if self.ntp_use:
                data["timestamp"] = 1720000000
            return FakeResponse(HttpStatus.C_200_OK, {"data": data})
        return super()._read_response(path)

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        override: FakeResponse | BaseException | None = self.override.get(
            (call.authorization, call.method, call.path),
            self.override.get((call.scheme, call.method, call.path)),
        )
        if isinstance(override, BaseException):
            raise override
        if override is not None:
            return override
        restoration: bool = (
            call.method == HttpMethod.POST
            and call.path == GatewayApi.CONFIG
            and call.body == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "",
                GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
            }
        )
        if restoration and session.authorized and self.restore_admin_failures > 0:
            self.restore_admin_failures -= 1
            return FakeResponse(HttpStatus.C_500_INTERNAL_SERVER_ERROR, {"error": "injected"})
        if restoration and call.bearer_token == self.rw_key and self.restore_rw_failure:
            return FakeResponse(HttpStatus.C_500_INTERNAL_SERVER_ERROR, {"error": "injected"})
        return None

    def _config_response(self) -> FakeResponse:
        self.config_reads += 1
        if self.malformed_config:
            return FakeResponse(HttpStatus.C_200_OK, malformed_json=True)
        return FakeResponse(HttpStatus.C_200_OK, self.config_overrides.get(self.config_reads, self.config()))


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        deterministic_random.calls = 0
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root: Path = Path(self.temp_dir.name)
        self.gateway: BearerGateway = BearerGateway(CONFIG)
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
            ("none", "GET", "/ruuvi.json", None),
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
        probes: list[RecordedRequest] = calls[3:17] + [calls[18], calls[20]]
        self.assertEqual(len(probes), len({call.session_number for call in probes}))
        self.assertTrue(all(not call.allow_redirects for call in calls))

    def test_last_read_failures_prevent_all_probe_writes(self) -> None:
        authorization: str
        for authorization in (f"Bearer {CONFIG.gw_id}", f"Bearer {RW_KEY}"):
            with self.subTest(authorization=authorization):
                self.gateway = BearerGateway(CONFIG)
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
                self.gateway = BearerGateway(CONFIG)

                class CorruptLoginSession(FakeSession):
                    def send(
                        self,
                        request: requests.PreparedRequest,
                        *,
                        injected_fault: str = fault,
                        **kwargs: Any,
                    ) -> FakeResponse:
                        if request.method == "POST" and urlsplit(request.url or "").path == "/auth":
                            if injected_fault == "missing-cookie":
                                request.headers.pop(HttpHeader.COOKIE, None)
                            elif injected_fault == "cookie":
                                request.headers[HttpHeader.COOKIE] = "RUUVISESSION=another-session"
                            elif injected_fault == "challenge":
                                self.challenge = ""
                            else:
                                assert isinstance(request.body, (str, bytes))
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
            # Omit the cookie on the wire even when the challenge populated the jar.
            session.cookies.clear()
            response: requests.Response = client.request(
                session,
                HttpMethod.POST,
                GatewayApi.AUTH,
                headers={HttpHeader.COOKIE: submitted_cookie} if submitted_cookie is not None else {},
                json_body=submitted_body,
            )
            self.assertEqual(submitted_cookie, self.gateway.calls[-1].cookie)
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
                self.gateway = BearerGateway(CONFIG)
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

    def test_successful_non_default_login_checks_identity_before_reset_advice(self) -> None:
        mac: Any
        config_mode: str
        for config_mode in (GatewayCfgLanAuthType.DEFAULT, GatewayCfgLanAuthType.RUUVI):
            for mac in (None, 123, "invalid", "11:22:33:44:55:66", CONFIG.gw_mac):
                with self.subTest(config_mode=config_mode, mac=mac):
                    self.gateway = BearerGateway(CONFIG)
                    self.gateway.mode = GatewayCfgLanAuthType.RUUVI
                    self.gateway.custom_ha1 = hashlib.md5(
                        f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()
                    ).hexdigest()
                    payload: dict[str, Any] = self.gateway.config()
                    payload[GatewayCfgDesc.LAN_AUTH_TYPE] = config_mode
                    if mac is None:
                        del payload[GatewayCfgDesc.GW_MAC]
                    else:
                        payload[GatewayCfgDesc.GW_MAC] = mac
                    self.gateway.config_overrides[1] = payload
                    result: RunResult = self.runner().run()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                    self.assertEqual(
                        target.FACTORY_RESET_MESSAGE if mac == CONFIG.gw_mac else None,
                        result.recovery_message,
                    )
                    self.assertEqual(
                        [("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json")],
                        [(call.method, call.path) for call in self.gateway.calls],
                    )
                    self.assertEqual(self.gateway.calls[1].session_number, self.gateway.calls[2].session_number)
                    self.assertEqual("NOT RUN", result.outcomes["temporary-state setup"])
                    self.assertEqual("", self.gateway.ro_key)
                    self.assertEqual("", self.gateway.rw_key)

    def test_non_default_login_with_unreadable_identity_does_not_recommend_reset(self) -> None:
        fault: FakeResponse | requests.RequestException
        faults: tuple[FakeResponse | requests.RequestException, ...] = (
            FakeResponse(503, {}),
            FakeResponse(200, malformed_json=True),
            requests.Timeout("baseline read timed out"),
        )
        for fault in faults:
            with self.subTest(fault=fault):
                self.gateway = BearerGateway(CONFIG)
                self.gateway.mode = GatewayCfgLanAuthType.RUUVI
                self.gateway.custom_ha1 = hashlib.md5(
                    f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()
                ).hexdigest()
                self.gateway.override[("", "GET", "/ruuvi.json")] = fault
                result: RunResult = self.runner().run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertIsNone(result.recovery_message)
                self.assertEqual(
                    [("GET", "/auth"), ("POST", "/auth"), ("GET", "/ruuvi.json")],
                    [(call.method, call.path) for call in self.gateway.calls],
                )

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
                gateway: BearerGateway = BearerGateway(CONFIG)
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

    def test_negative_get_protected_data_is_fail_with_expected_denial_status(self) -> None:
        scheme: str
        path: str
        payload: Any
        cases: tuple[Any, ...] = (
            {GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT},
            {"ssid": "private-network"},
            {"channel": 6},
            {"stable": "authenticated-baseline-only"},
            {"baseline_nested_field": "authenticated-baseline-only"},
            {"http_user": "private-user"},
            {"mqtt_pass": "private-password"},
            {"http_bearer_token": "private-token"},
            {"http_api_key": "private-key"},
            {"http_stat_pass": "private-password"},
            {"remote_cfg_auth_bearer_token": "private-token"},
            {"wrapper": [{"wifi_sta_config": {"ssid": "private-network"}}]},
            {"data": {"tags": {"01:02:03:04:05:06": {"rssi": -60}}}},
            {"data": {"coordinates": "private-location", "tags": {}}},
        )
        for scheme in (HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST, HttpAuthScheme.BEARER):
            for path in (GatewayApi.HISTORY, GatewayApi.CONFIG):
                for payload in cases:
                    with self.subTest(scheme=scheme, path=path, payload=payload):
                        self.gateway = BearerGateway(CONFIG)
                        deterministic_random.calls = 0
                        status: int = 401 if scheme == HttpAuthScheme.BEARER else 302
                        authorization: str = f"Bearer {CONFIG.gw_id}" if scheme == HttpAuthScheme.BEARER else scheme
                        self.gateway.override[(authorization, HttpMethod.GET, path)] = FakeResponse(status, payload)
                        result: RunResult = self.runner().run()
                        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
                        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                        self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))
                        cleanup_index: int = next(
                            index for index, call in enumerate(self.gateway.calls)
                            if call.body == {GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""}
                        )
                        self.assertFalse(any(
                            call.authorization == f"Bearer {RO_KEY}" for call in self.gateway.calls[:cleanup_index]
                        ))

    def test_hidden_remote_credentials_in_denial_are_security_failures(self) -> None:
        field: str
        method: str
        for field in ("remote_cfg_auth_token", "remote_cfg_auth_apikey"):
            for method in (HttpMethod.GET, HttpMethod.POST):
                with self.subTest(field=field, method=method):
                    self.gateway = BearerGateway(CONFIG)
                    deterministic_random.calls = 0
                    self.gateway.override[("Basic", method, GatewayApi.CONFIG)] = FakeResponse(
                        302 if method == HttpMethod.GET else 401, {"nested": [{field: "hidden-secret"}]},
                    )
                    result: RunResult = self.runner().run()
                    self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                    mechanism: str = AuthMech.M2M_API_BEARER_RO if method == HttpMethod.GET else AuthMech.M2M_API_BEARER_RW
                    self.assertEqual("FAIL", result.outcomes[mechanism])
                    self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])

    def test_ro_denial_mutation_is_ro_fail_before_rw_post(self) -> None:
        original: Callable[[FakeSession, RecordedRequest], FakeResponse | None] = self.gateway._response_override

        def mutate(session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            if call.authorization == f"Bearer {RO_KEY}" and call.method == HttpMethod.POST:
                self.gateway.mutate_after_positive = True
                return FakeResponse(401, {})
            if call.body == {"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}:
                self.gateway.mutate_after_positive = False
            return original(session, call)

        self.gateway._response_override = mutate
        runner: target.FunctionalTest_5_1_2a_2_b = self.runner()
        result: RunResult = runner.run()
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.M2M_API_BEARER_RW])
        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
        rw_posts: list[RecordedRequest] = [call for call in self.gateway.calls
            if call.authorization == f"Bearer {RW_KEY}" and call.method == HttpMethod.POST]
        self.assertEqual(1, len(rw_posts))  # Only mandatory revoked-key verification, after restoration.
        self.assertIn("denied RO POST did not change configuration", " ".join(runner.diagnostics))

    def test_revoked_key_success_marks_its_own_mechanism_fail(self) -> None:
        key: str
        method: str
        path: str
        mechanism: str
        for key, method, path, mechanism in (
            (RO_KEY, "GET", "/history", AuthMech.M2M_API_BEARER_RO),
            (RW_KEY, "POST", "/ruuvi.json", AuthMech.M2M_API_BEARER_RW),
        ):
            with self.subTest(mechanism=mechanism):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

                def inject(description: str, token: str = key, verb: str = method, route: str = path) -> None:
                    if description.startswith("Restoring"):
                        self.gateway.override[(f"Bearer {token}", verb, route)] = FakeResponse(200, {})

                runner.progress = inject
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[mechanism])
                self.assertEqual("ERROR", result.outcomes["final restoration and non-mutation"])
                self.assertIn("expected HTTP 401", " ".join(runner.diagnostics))
                self.assertIn("observed: 200", " ".join(runner.diagnostics))
                self.assertIn('"result": "FAIL"', self.log.path.read_text())

    def test_negative_get_embedded_or_truncated_protected_json_is_fail(self) -> None:
        text: str
        for text in ('<html>{"lan_auth_type":"lan_auth_default"}</html>', '{"data":{"tags":',
                     '<html>{"http_bearer_token":"private-token"}</html>', '{"ssid":'):
            with self.subTest(text=text):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                response: FakeResponse = FakeResponse(302)
                response._content = text.encode("utf-8")
                self.gateway.override[(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.HISTORY)] = response
                result: RunResult = self.runner().run()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
                self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])

    def test_post_denials_reject_protected_data_and_abort_before_rw_write(self) -> None:
        authorization: str
        payload: Any
        for authorization in (HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST,
                              f"Bearer {CONFIG.gw_id}", f"Bearer {RO_KEY}"):
            for payload in ({"ssid": "private-network"}, {"mqtt_pass": "secret"},
                            {"http_user": "private-user"}, {"storage_ready": True}):
                with self.subTest(authorization=authorization, payload=payload):
                    self.gateway = BearerGateway(CONFIG)
                    deterministic_random.calls = 0
                    self.gateway.override[(authorization, HttpMethod.POST, GatewayApi.CONFIG)] = FakeResponse(401, payload)
                    result: RunResult = self.runner().run()
                    mechanism: str = (AuthMech.M2M_API_BEARER_RO if authorization == f"Bearer {RO_KEY}"
                                      else AuthMech.M2M_API_BEARER_RW)
                    self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                    self.assertEqual("FAIL", result.outcomes[mechanism])
                    self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                    self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))
                    cleanup_index: int = next(
                        index for index, call in enumerate(self.gateway.calls)
                        if call.body == {GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""}
                    )
                    self.assertFalse(any(
                        call.authorization == f"Bearer {RW_KEY}" and call.method == HttpMethod.POST
                        for call in self.gateway.calls[:cleanup_index]
                    ))

    def test_post_denials_allow_auth_errors_and_public_metadata(self) -> None:
        authorization: str
        payload: Any
        for authorization in (HttpAuthScheme.BASIC, f"Bearer {RO_KEY}"):
            for payload in (None, {}, {"error": "unauthorized"},
                            {"gw_mac": CONFIG.gw_mac, "fw_ver": "public-fw", "nrf52_fw_ver": "public-nrf"}):
                with self.subTest(authorization=authorization, payload=payload):
                    self.gateway = BearerGateway(CONFIG)
                    deterministic_random.calls = 0
                    self.gateway.override[(authorization, HttpMethod.POST, GatewayApi.CONFIG)] = FakeResponse(401, payload)
                    self.assertEqual(0, self.runner().run().exit_code)

    def test_revoked_bearer_denials_must_not_disclose_protected_data(self) -> None:
        key: str
        method: str
        path: str
        for key, method, path in ((RO_KEY, HttpMethod.GET, GatewayApi.HISTORY),
                                  (RW_KEY, HttpMethod.POST, GatewayApi.CONFIG)):
            with self.subTest(method=method):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

                def inject_revoked_leak(description: str, token: str = key,
                                        verb: str = method, route: str = path) -> None:
                    if description == "Restoring the original API-key configuration":
                        self.gateway.override[(f"Bearer {token}", verb, route)] = FakeResponse(401, {"ssid": "private"})

                runner.progress = inject_revoked_leak
                result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes["final restoration and non-mutation"])
                self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

    def test_negative_get_allows_empty_auth_errors_and_public_metadata(self) -> None:
        payload: Any
        payloads: tuple[Any, ...] = (
            None, {}, {"error": "unauthorized"},
            {"gw_mac": CONFIG.gw_mac, "fw_ver": "public-fw", "nrf52_fw_ver": "public-nrf"},
            {"status": 401, "auth_type": "x-ruuvi-interactive", "realm": "Ruuvi Gateway"},
        )
        for payload in payloads:
            with self.subTest(payload=payload):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                self.gateway.override[(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.CONFIG)] = FakeResponse(302, payload)
                result: RunResult = self.runner().run()
                self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.gateway = BearerGateway(CONFIG)
        deterministic_random.calls = 0
        response: FakeResponse = FakeResponse(302)
        response._content = b"<html>Please authenticate</html>"
        self.gateway.override[(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.HISTORY)] = response
        self.assertEqual(0, self.runner().run().exit_code)

    def test_firmware_auth_denials_allow_optional_public_versions(self) -> None:
        versions: dict[str, str]
        for versions in ({}, {"fw_ver": "public-fw", "nrf52_fw_ver": "public-nrf"}):
            with self.subTest(versions=versions):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                payload: dict[str, Any] = {
                    "gateway_name": "RuuviGatewayEEFF", "lan_auth_type": "lan_auth_bearer", "lan": True,
                    **versions,
                }
                self.gateway.override[(f"Bearer {CONFIG.gw_id}", "GET", "/history")] = FakeResponse(401, payload)
                result: RunResult = self.runner().run()
                self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
                self.assertTrue(all(outcome == "PASS" for outcome in result.outcomes.values()))
                self.assertIsNone(result.recovery_message)
                self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))
                self.assertEqual(
                    [("GET", "/history"), ("POST", "/ruuvi.json")],
                    [(call.method, call.path) for call in self.gateway.calls[-2:]],
                )

    def test_public_auth_envelope_does_not_hide_protected_data(self) -> None:
        envelope: dict[str, Any] = {
            "gateway_name": "RuuviGatewayEEFF", "lan_auth_type": "lan_auth_bearer", "lan": True,
        }
        payload: dict[str, Any]
        cases: tuple[dict[str, Any], ...] = (
            {**envelope, "ssid": "private-network"},
            {**envelope, "nested": [{"mqtt_pass": "secret"}]},
            {**envelope, "data": {"tags": {}}},
            {**envelope, "nested": {"lan_auth_type": "lan_auth_default"}},
            {**envelope, "gateway_name": {"ssid": "private-network"}},
            {**envelope, "lan_auth_type": "lan_auth_ruuvi"},
            {**envelope, "lan": 1},
            {**envelope, "fw_ver": {"mqtt_pass": "secret"}},
            {"lan_auth_type": "lan_auth_bearer", "lan": True},
            {"nested": envelope},
        )
        for payload in cases:
            with self.subTest(payload=payload):
                self.gateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                self.gateway.override[(f"Bearer {CONFIG.gw_id}", "GET", "/history")] = FakeResponse(401, payload)
                result: RunResult = self.runner().run()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
                self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

    def test_positive_bearer_responses_require_json_objects(self) -> None:
        cases: tuple[tuple[str, str, str], ...] = (
            (RO_KEY, HttpMethod.GET, GatewayApi.HISTORY),
            (RO_KEY, HttpMethod.GET, GatewayApi.CONFIG),
            (RW_KEY, HttpMethod.GET, GatewayApi.HISTORY),
            (RW_KEY, HttpMethod.GET, GatewayApi.CONFIG),
            (RW_KEY, HttpMethod.POST, GatewayApi.CONFIG),
        )
        key: str
        method: str
        path: str
        fault: str
        for key, method, path in cases:
            for fault in ("malformed", "wrong-type", "nonempty"):
                if fault == "nonempty" and method != HttpMethod.POST:
                    continue
                with self.subTest(key=key, method=method, path=path, fault=fault):
                    self.gateway = BearerGateway(CONFIG)
                    deterministic_random.calls = 0
                    response: FakeResponse = FakeResponse(
                        200, [] if fault == "wrong-type" else {"unexpected": True},
                        malformed_json=fault == "malformed",
                    )
                    self.gateway.override[(f"Bearer {key}", method, path)] = response
                    runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

                    def end_positive_fault(description: str) -> None:
                        if description.startswith("Restoring"):
                            # Fault only the positive reply, not subsequent revoked-token controls.
                            self.gateway.override.clear()

                    runner.progress = end_positive_fault
                    result: RunResult = runner.run()
                    self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                    self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                    self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))
                    self.assertEqual("ERROR", result.outcomes[
                        AuthMech.M2M_API_BEARER_RO if key == RO_KEY else AuthMech.M2M_API_BEARER_RW
                    ])

    def test_positive_reads_reject_empty_error_only_and_incomplete_data(self) -> None:
        key: str
        path: str
        payload: dict[str, Any]
        for key in (RO_KEY, RW_KEY):
            for path in (GatewayApi.CONFIG, GatewayApi.HISTORY):
                for payload in ({}, {"error": "unauthorized"}, {"data": {"tags": {}}}):
                    with self.subTest(key=key, path=path, payload=payload):
                        self.gateway = BearerGateway(CONFIG)
                        deterministic_random.calls = 0
                        self.gateway.override[(f"Bearer {key}", HttpMethod.GET, path)] = FakeResponse(200, payload)
                        runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

                        def stop_fault_for_recovery(description: str) -> None:
                            if description.startswith("Restoring"):
                                self.gateway.override.clear()

                        runner.progress = stop_fault_for_recovery
                        result: RunResult = runner.run()
                        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                        self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))
                        restore_index: int = next(
                            index for index, call in enumerate(self.gateway.calls)
                            if call.body == {"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}
                        )
                        self.assertFalse(any(
                            call.method == HttpMethod.POST and call.body == {}
                            for call in self.gateway.calls[:restore_index]
                        ))

    def test_positive_config_must_match_prepared_state(self) -> None:
        stale: dict[str, Any] = self.gateway.config()
        self.gateway.override[(f"Bearer {RO_KEY}", HttpMethod.GET, GatewayApi.CONFIG)] = FakeResponse(200, stale)
        result: RunResult = self.runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
        self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

    def test_history_structure_and_sensor_reports(self) -> None:
        fault: str
        ntp_use: bool
        for ntp_use in (False, True):
            for fault in ("none", "wrong-mac", "coordinates", "tags", "timestamp", "report", "missing-phy", "invalid-phy"):
                with self.subTest(ntp_use=ntp_use, fault=fault):
                    self.gateway = BearerGateway(CONFIG)
                    self.gateway.ntp_use = ntp_use
                    deterministic_random.calls = 0
                    report: dict[str, Any] = {
                        "rssi": -60, "ble_chan": 37, "ble_phy": "1M", "data": "020106",
                        "timestamp" if ntp_use else "counter": 123,
                    }
                    data: dict[str, Any] = {
                        "gw_mac": CONFIG.gw_mac, "coordinates": "",
                        "tags": {"01:02:03:04:05:06": report},
                    }
                    if ntp_use:
                        data["timestamp"] = 1720000000
                    if fault == "wrong-mac":
                        data["gw_mac"] = "01:02:03:04:05:06"
                    elif fault == "coordinates":
                        data["coordinates"] = None
                    elif fault == "tags":
                        data["tags"] = []
                    elif fault == "timestamp":
                        data["timestamp"] = True
                    elif fault == "report":
                        report["rssi"] = "-60"
                    if fault == "missing-phy":
                        del report["ble_phy"]
                    elif fault == "invalid-phy":
                        report["ble_phy"] = "invalid"
                    self.gateway.override[(f"Bearer {RO_KEY}", HttpMethod.GET, GatewayApi.HISTORY)] = FakeResponse(
                        200, {"data": data}
                    )
                    runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

                    def stop_history_fault_for_recovery(description: str) -> None:
                        if description.startswith("Restoring"):
                            self.gateway.override.clear()

                    runner.progress = stop_history_fault_for_recovery
                    result: RunResult = runner.run()
                    self.assertEqual(
                        (0, "PASS") if fault == "none" else (2, "ERROR"),
                        (result.exit_code, result.verdict),
                    )
                    self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                    self.assertEqual("PASS" if fault == "none" else "ERROR",
                                     result.outcomes[AuthMech.M2M_API_BEARER_RO])
                    self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

    def test_empty_history_passes_with_ntp_disabled(self) -> None:
        self.gateway.ntp_use = False
        result: RunResult = self.runner().run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))

    def test_evidence_failure_after_provisioning_cannot_block_recovery(self) -> None:
        unverified: bool
        for unverified in (False, True):
            with self.subTest(unverified=unverified):
                self.gateway = BearerGateway(CONFIG)
                if unverified:
                    self.gateway.restore_admin_failures = 100
                    self.gateway.restore_rw_failure = True
                runner: target.FunctionalTest_5_1_2a_2_b = self.runner()
                original_write: Callable[[str, Any], None] = self.log.write
                broken: bool = False

                def fail_after_provisioning(
                    label: str, value: Any = "", write: Callable[[str, Any], None] = original_write
                ) -> None:
                    nonlocal broken
                    broken = broken or bool(self.gateway.ro_key)
                    if broken:
                        raise OSError("evidence disk unavailable")
                    write(label, value)

                with mock.patch.object(self.log, "write", side_effect=fail_after_provisioning):
                    result: RunResult = runner.run()
                self.assertTrue(broken)
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                cleanup: list[RecordedRequest] = [
                    call for call in self.gateway.calls
                    if call.body == {GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""}
                ]
                self.assertEqual(3 if unverified else 1, len(cleanup))
                self.assertEqual("ERROR" if unverified else "PASS", result.outcomes["final restoration and non-mutation"])
                self.assertEqual(target.FACTORY_RESET_MESSAGE if unverified else None, result.recovery_message)
                if not unverified:
                    self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

    def test_runner_preamble_failure_is_error_before_dut_access(self) -> None:
        runner: target.FunctionalTest_5_1_2a_2_b = self.runner()
        with mock.patch.object(self.log, "write", side_effect=OSError("evidence unavailable")):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual([], self.gateway.calls)
        self.assertIn("initial evidence preamble: OSError", " ".join(runner.diagnostics))

    def test_progress_failure_during_cleanup_preserves_verified_recovery(self) -> None:
        runner: target.FunctionalTest_5_1_2a_2_b = self.runner()

        def fail_progress(description: str) -> None:
            if description.startswith(("Restoring", "Verifying restoration")):
                raise OSError("progress output unavailable")

        runner.progress = fail_progress
        result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
        self.assertEqual(("", ""), (self.gateway.ro_key, self.gateway.rw_key))

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

        gateway: BearerGateway = BearerGateway(CONFIG)
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
            allow_redirects: bool = False,
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
                gateway: BearerGateway = BearerGateway(CONFIG)
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
        gateway: BearerGateway = BearerGateway(CONFIG)
        gateway.ro_key = "already-enabled"
        runner: target.FunctionalTest_5_1_2a_2_b = target.FunctionalTest_5_1_2a_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(gateway),
        )
        result: RunResult = runner.run()
        self.assertEqual(2, result.exit_code)
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)

        gateway = BearerGateway(CONFIG)
        gateway.custom_username = "NotAdmin"
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
            gateway: BearerGateway = BearerGateway(CONFIG)
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

    def test_execute_reports_error_and_manual_recovery_when_evidence_fails(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root)
            gateway: BearerGateway = BearerGateway(CONFIG)
            gateway.restore_admin_failures = 100
            gateway.restore_rw_failure = True
            messages: list[str] = []
            original_write: Callable[[target.EvidenceLog, str, Any], None] = cast(
                Callable[[target.EvidenceLog, str, Any], None], target.EvidenceLog.write
            )
            broken: bool = False

            def fail_write(log: target.EvidenceLog, label: str, value: Any = "") -> None:
                nonlocal broken
                broken = broken or bool(gateway.ro_key)
                if broken:
                    raise OSError("evidence unavailable")
                original_write(log, label, value)

            def fail_progress(_message: str) -> None:
                if broken:
                    raise OSError("progress evidence unavailable")

            with mock.patch.object(target.EvidenceLog, "write", new=fail_write), mock.patch.object(
                target.EvidenceLog, "write_line", side_effect=fail_progress
            ):
                result: RunResult = target.execute_test_5_1_2a_2_b(
                    root, session_factory=lambda: FakeSession(gateway),
                    random_bytes=deterministic_random, output=messages.append,
                )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
            self.assertEqual(["Overall verdict: ERROR", target.FACTORY_RESET_MESSAGE], messages[-2:])
            self.assertEqual(3, sum(call.body == {
                GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""
            } for call in gateway.calls))

    def test_complete_entrypoint_lifecycle_failures_return_error_without_escaping(self) -> None:
        phase: str
        for phase in ("create", "open-output", "preamble", "finish", "terminal-output"):
            directory: str
            with self.subTest(phase=phase), tempfile.TemporaryDirectory() as directory:
                root: Path = Path(directory)
                self.write_env(root)
                gateway: BearerGateway = BearerGateway(CONFIG)
                deterministic_random.calls = 0
                messages: list[str] = []
                failed: bool = False
                original_write: Callable[[target.EvidenceLog, str, Any], None] = cast(
                    Callable[[target.EvidenceLog, str, Any], None], target.EvidenceLog.write,
                )

                def output(message: str, selected: str = phase, sink: Callable[[str], None] = messages.append) -> None:
                    nonlocal failed
                    if not failed and ((selected == "open-output" and message.startswith("Open log"))
                                       or (selected == "terminal-output" and message.startswith("Overall verdict"))):
                        failed = True
                        raise OSError("secret-in-output-error")
                    sink(message)

                def write(log: target.EvidenceLog, label: str, value: Any = "", selected: str = phase,
                          emit: Callable[[target.EvidenceLog, str, Any], None] = original_write) -> None:
                    nonlocal failed
                    if selected == "preamble" and not failed:
                        failed = True
                        raise OSError("secret-in-preamble-error")
                    emit(log, label, value)

                with mock.patch.object(target.EvidenceLog, "write", new=write):
                    if phase == "create":
                        with mock.patch.object(target.EvidenceLog, "create", side_effect=PermissionError("secret-path")):
                            result: RunResult = target.execute_test_5_1_2a_2_b(root, output=output)
                    elif phase == "finish":
                        with mock.patch.object(target.EvidenceLog, "finish", side_effect=OSError("secret-path")):
                            result = target.execute_test_5_1_2a_2_b(
                                root, session_factory=lambda fixture=gateway: FakeSession(fixture), output=output,
                                random_bytes=deterministic_random,
                            )
                    else:
                        result = target.execute_test_5_1_2a_2_b(
                            root, session_factory=lambda fixture=gateway: FakeSession(fixture), output=output,
                            random_bytes=deterministic_random,
                        )
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertIn("Overall verdict: ERROR", messages)
                self.assertFalse(any("secret-" in message for message in messages))
                if phase in ("create", "open-output", "preamble"):
                    self.assertEqual([], gateway.calls)
                else:
                    self.assertEqual("PASS", result.outcomes["final restoration and non-mutation"])
                    self.assertEqual("", gateway.ro_key)
                    self.assertEqual("", gateway.rw_key)
                if phase == "terminal-output":
                    evidence: str = next((root / "logs").iterdir()).read_text()
                    self.assertIn("OVERALL VERDICT: ERROR", evidence)

    def test_terminal_fail_reason_survives_later_restoration_error(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root)
            gateway: BearerGateway = BearerGateway(CONFIG)
            gateway.override[("Basic", "GET", "/history")] = FakeResponse(200, {"secret": CONFIG.gw_id})
            gateway.restore_admin_failures = 100
            gateway.restore_rw_failure = True
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_2a_2_b(
                root, session_factory=lambda: FakeSession(gateway), output=messages.append,
                random_bytes=deterministic_random,
            )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            text: str = "\n".join(messages)
            self.assertIn("Basic gains no access to GET /history", text)
            self.assertIn("expected: assertion satisfied; observed: 200", text)
            self.assertIn("Restoration ERROR", text)
            self.assertEqual(target.FACTORY_RESET_MESSAGE, messages[-1])
            self.assertNotIn(CONFIG.gw_id, text)
            self.assertNotIn(RO_KEY, text)
            self.assertNotIn(RW_KEY, text)
            evidence: str = next((root / "logs").iterdir()).read_text()
            self.assertIn("TERMINAL DIAGNOSTIC", evidence)
            self.assertIn("Basic gains no access", evidence)

    def test_transport_error_terminal_reason_is_secret_free(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            self.write_env(root)
            gateway: BearerGateway = BearerGateway(CONFIG)
            gateway.override[("Basic", "GET", "/history")] = requests.ConnectionError(f"Bearer {CONFIG.gw_id}")
            messages: list[str] = []
            result: RunResult = target.execute_test_5_1_2a_2_b(
                root, session_factory=lambda: FakeSession(gateway), output=messages.append,
                random_bytes=deterministic_random,
            )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            text: str = "\n".join(messages)
            self.assertIn("Testing rejection of password schemes by M2M read routes", text)
            self.assertIn("GatewayConnectionError", text)
            self.assertNotIn(CONFIG.gw_id, text)

    def test_basic_and_digest_setup_errors_report_recovery_and_stop(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            directory: str
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                root: Path = Path(directory)
                self.write_env(root)
                gateway: BearerGateway = BearerGateway(CONFIG)
                gateway.mode = mode
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
