"""Implementation tests only; these are not ETSI functional-test evidence."""

from __future__ import annotations

import base64
import hashlib
import json
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterator
from unittest import mock

import requests

import test_5_1_5_2_b as target
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
)
from lib.http_api import (
    HttpMethod,
    HttpStatus,
)
from lib.models import RunResult
from test_support.fake_gateway import FakeGateway, FakeResponse, FakeSession, RecordedRequest, UniqueRandom, env_text

FIXED_NOW: datetime = datetime(2026, 8, 31, 9, 0, 0, tzinfo=timezone.utc)
CONFIG: target.DutConfig = target.DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
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


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root: Path = Path(self.temp_dir.name)
        self.gateway: FakeGateway = FakeGateway(CONFIG)
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

    def test_legacy_password_bypass_is_detected_for_configured_username(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                self.check_legacy_password_bypass(mode)

    def check_legacy_password_bypass(self, mode: str) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        original: Callable[..., FakeResponse] = gateway._credentials_response
        guessed_passwords: list[str] = []

        def bypass(
            headers: Any, method: str, path: str,
        ) -> FakeResponse:
            response: FakeResponse = original(headers, method, path)
            authorization: str = headers.get("Authorization", "")
            if gateway.mode == mode and response.status_code == 401 and authorization:
                if mode == GatewayCfgLanAuthType.BASIC:
                    username: str
                    password: str
                    username, password = base64.b64decode(authorization.split(" ", 1)[1]).decode().split(":", 1)
                    self.assertEqual(gateway.custom_username, username)
                    configured_password: str = base64.b64decode(gateway.custom_ha1).decode().split(":", 1)[1]
                    self.assertEqual(len(configured_password), len(password))
                    self.assertNotEqual(configured_password, password)
                    guessed_passwords.append(password)
                else:
                    parameters: dict[str, str | None] = requests.utils.parse_dict_header(authorization[7:])
                    self.assertEqual(gateway.custom_username, parameters["username"])
                    guessed_passwords.append(parameters["response"] or "")
                return FakeResponse(200, {"authenticated": True})
            return response

        runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
            CONFIG, self.log, session_factory=lambda: FakeSession(gateway),
            random_bytes=UniqueRandom(), monotonic_ns=AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000), attempt_count=1,
        )
        with mock.patch.object(gateway, "_credentials_response", side_effect=bypass):
            result: RunResult = runner.run()
        mechanism: str = AuthMech.LAN_WEBUI_BASIC if mode == GatewayCfgLanAuthType.BASIC else AuthMech.LAN_WEBUI_DIGEST
        self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(1, len(guessed_passwords))

    def test_cookie_free_challenge_success_is_security_failure(self) -> None:
        mechanism: str
        for mechanism in (AuthMech.LAN_WEBUI_DEFAULT, AuthMech.LAN_WEBUI_DIGEST):
            with self.subTest(mechanism=mechanism):
                runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
                with mock.patch.object(runner.gateway, "request", return_value=FakeResponse(200, {})):
                    if mechanism == AuthMech.LAN_WEBUI_DIGEST:
                        with self.assertRaises(target.UnexpectedAuthorization):
                            runner._measure_http_auth_campaign(
                                mechanism, False, 401, lambda active_runner=runner: active_runner._prepare_digest_request("user", "wrong"),
                                True, prepare_before_timing=True,
                            )
                    else:
                        # This boundary supplies transport observations normally produced by request().
                        runner.gateway.last_transport_timing = target.TransportTiming(0, 2_000_000_000)
                        with self.assertRaises(target.UnexpectedAuthorization):
                            runner._attack_interactive(mechanism, GatewayCfgLanAuthType.DEFAULT)
                self.assertEqual("FAIL", runner.outcomes[mechanism])

    def test_accepted_attempt_reporting_error_preserves_security_failure(self) -> None:
        mechanism: str
        boundary: str
        for mechanism in (AuthMech.LAN_WEBUI_USER_DEFINED, AuthMech.M2M_API_BEARER_RO):
            for boundary in ("evidence", "terminal"):
                with self.subTest(mechanism=mechanism, boundary=boundary):
                    self.check_accepted_attempt_reporting_error(mechanism, boundary)

    def check_accepted_attempt_reporting_error(self, mechanism: str, boundary: str) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        if mechanism == AuthMech.M2M_API_BEARER_RO:
            gateway.bearer_guess_status = 200
        runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
            CONFIG, self.log, session_factory=lambda: FakeSession(gateway),
            random_bytes=UniqueRandom(), monotonic_ns=AdvancingClock(2_100_000_000),
            success_monotonic_ns=AdvancingClock(100_000_000), attempt_count=1,
        )
        original_write: Callable[..., None] = self.log.write
        original_override: Callable[..., FakeResponse | None] = gateway._response_override
        bypassed: list[bool] = [False]

        def override(
            session: FakeSession, call: RecordedRequest,
        ) -> FakeResponse | None:
            if (not bypassed[0] and mechanism == AuthMech.LAN_WEBUI_USER_DEFINED and call.method == HttpMethod.POST
                    and call.path == GatewayApi.AUTH and runner.campaigns.get(mechanism) is not None):
                bypassed[0] = True
                return FakeResponse(200, {})
            return original_override(session, call)

        def write(
            label: str, value: Any = "",
        ) -> None:
            if label == "CAMPAIGN ATTEMPT" and value.campaign == mechanism and value.outcome == "AUTHORIZED":
                raise OSError("accepted attempt evidence unavailable")
            original_write(label, value)

        def progress(
            _message: str,
        ) -> None:
            if runner.campaigns.get(mechanism) is not None and runner.campaigns[mechanism].authorized:
                raise BrokenPipeError("accepted attempt terminal unavailable")

        runner.campaign_progress = progress if boundary == "terminal" else lambda message: None
        with mock.patch.object(self.log, "write", side_effect=write if boundary == "evidence" else original_write), mock.patch.object(
            gateway, "_response_override", side_effect=override,
        ):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_slow_evidence_does_not_change_transport_timing_verdict(self) -> None:
        successful: bool
        for successful in (True, False):
            with self.subTest(successful=successful):
                self.check_slow_evidence_timing(successful)

    def check_slow_evidence_timing(self, successful: bool) -> None:
        elapsed: list[int] = [0]
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        runner.monotonic_ns = lambda: elapsed[0]
        runner.success_monotonic_ns = lambda: elapsed[0]
        self.gateway.mode = GatewayCfgLanAuthType.ALLOW if successful else GatewayCfgLanAuthType.DENY
        class TimedSession(FakeSession):
            def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
                elapsed[0] += 100_000_000
                return super().send(request, **kwargs)

        runner.gateway.session_factory = lambda: TimedSession(self.gateway)

        def slow_write(_value: Any) -> None:
            elapsed[0] += 2_000_000_000

        with mock.patch.object(
            self.log, "write_http_request", side_effect=slow_write,
        ), mock.patch.object(self.log, "write_http_response", side_effect=slow_write):
            operation: Callable[[], requests.Response] = lambda: runner.gateway.request(
                runner.gateway.new_session(), HttpMethod.GET, GatewayApi.STATUS,
            )
            if successful:
                runner._measure_http_auth_campaign(
                    AuthMech.LAN_WEBUI_BASIC, True, 200, operation, True,
                )
                self.assertEqual("PASS", runner.outcomes[AuthMech.LAN_WEBUI_BASIC])
                self.assertEqual([100_000_000], runner.success_durations_ns[AuthMech.LAN_WEBUI_BASIC])
            else:
                with self.assertRaises(target.SecurityFailure):
                    runner._measure_http_auth_campaign(
                        AuthMech.LAN_WEBUI_DISABLED, False, 403, operation, True,
                    )
                self.assertEqual("FAIL", runner.outcomes[AuthMech.LAN_WEBUI_DISABLED])
                self.assertEqual([100_000_000], runner.campaigns[AuthMech.LAN_WEBUI_DISABLED].durations_ns)

    def test_complete_interactive_timing_excludes_slow_evidence(self) -> None:
        self.check_complete_slow_evidence(fast_denials=False)
        self.check_complete_slow_evidence(fast_denials=True)

    def check_complete_slow_evidence(self, fast_denials: bool) -> None:
        elapsed: list[int] = [0]
        gateway: FakeGateway = FakeGateway(CONFIG)

        class TimedSession(FakeSession):
            def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
                response: FakeResponse = super().send(request, **kwargs)
                elapsed[0] += (
                    2_100_000_000 if response.status_code in (401, 403) and not fast_denials else 100_000_000
                )
                return response

        runner: target.FunctionalTest_5_1_5_2_b = target.FunctionalTest_5_1_5_2_b(
            CONFIG, self.log, session_factory=lambda: TimedSession(gateway), random_bytes=UniqueRandom(),
            monotonic_ns=lambda: elapsed[0], success_monotonic_ns=lambda: elapsed[0], attempt_count=2,
        )

        def slow_write(_value: Any) -> None:
            elapsed[0] += 5_000_000_000

        with mock.patch.object(self.log, "write_http_request", side_effect=slow_write), mock.patch.object(
            self.log, "write_http_response", side_effect=slow_write,
        ):
            result: RunResult = runner.run()
        self.assertEqual((1, "FAIL") if fast_denials else (0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])
        mechanism: str
        for mechanism in (AuthMech.LAN_WEBUI_DEFAULT, AuthMech.LAN_WEBUI_USER_DEFINED):
            timing: target.InteractiveTimingStats = runner.interactive_timings[mechanism]
            self.assertEqual([100_000_000] * 2, timing.successful_post_durations_ns)
            if not fast_denials:
                self.assertEqual([2_100_000_000], timing.failed_get_durations_ns)
                self.assertEqual([2_100_000_000] * 2, timing.failed_post_durations_ns)
            else:
                self.assertEqual("FAIL", result.outcomes[mechanism])

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
                self.gateway = FakeGateway(CONFIG)
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
        self.gateway = FakeGateway(CONFIG)
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
                    GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
                    GatewayCfgDesc.LAN_AUTH_USER: target.ADMIN_USERNAME,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: False,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: False,
                }
            )
        )
        self.make_runner()._validate_baseline(payload)

    def test_missing_authenticated_identity_stops_before_second_read_and_mutation(self) -> None:
        original: Callable[..., FakeResponse] = self.gateway._config_response

        def config_response() -> FakeResponse:
            response: FakeResponse = original()
            response._payload.pop(GatewayCfgDesc.GW_MAC, None)
            return response

        with mock.patch.object(self.gateway, "_config_response", side_effect=config_response):
            result: RunResult = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertIsNone(result.recovery_message)
        self.assertEqual([], self.gateway.config_bodies)
        self.assertEqual(
            [("GET", GatewayApi.AUTH), ("POST", GatewayApi.AUTH), ("GET", GatewayApi.CONFIG)],
            [(call.method, call.path) for call in self.gateway.calls],
        )

    def test_deny_retains_explicit_rw_recovery_and_checks_exact_noop(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        result: RunResult = runner.run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        transition: int = next(
            index for index, call in enumerate(self.gateway.calls)
            if isinstance(call.body, dict) and call.body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DENY
        )
        calls: list[RecordedRequest] = self.gateway.calls[transition:transition + 7]
        rw: str = runner.temporary_rw_key or ""
        self.assertTrue(rw)
        self.assertEqual(rw, calls[0].body[GatewayCfgDesc.LAN_AUTH_API_KEY_RW])
        self.assertEqual(
            [("POST", GatewayApi.CONFIG, rw), ("GET", GatewayApi.STATUS, None),
             ("GET", GatewayApi.STATUS, None), ("GET", GatewayApi.CONFIG, rw),
             ("POST", GatewayApi.CONFIG, rw), ("GET", GatewayApi.CONFIG, rw),
             ("POST", GatewayApi.CONFIG, rw)],
            [(call.method, call.path, call.bearer_token) for call in calls],
        )
        self.assertTrue(all(call.cookie is None for call in calls))
        self.assertEqual({}, calls[4].body)
        self.assertEqual(GatewayCfgLanAuthType.RUUVI, calls[6].body[GatewayCfgDesc.LAN_AUTH_TYPE])
        self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_lost_deny_transition_reply_restores_with_retained_rw_key(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        original: Callable[..., requests.Response] = runner._post_config

        def post(session: requests.Session, body: dict[str, Any], bearer_key: str | None = None) -> requests.Response:
            response: requests.Response = original(session, body, bearer_key)
            if body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DENY:
                self.assertEqual(GatewayCfgLanAuthType.DENY, self.gateway.mode)
                raise target.GatewayConnectionError("DENY reply lost after application")
            return response

        with mock.patch.object(runner, "_post_config", side_effect=post):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes[AuthMech.LAN_WEBUI_DISABLED])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertIsNone(result.recovery_message)
        self.assertEqual(["bearer"], self.gateway.restoration_attempts)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)
        writes: list[RecordedRequest] = [
            call for call in self.gateway.calls if call.method == "POST" and call.path == GatewayApi.CONFIG
        ]
        self.assertEqual(GatewayCfgLanAuthType.DENY, writes[-2].body[GatewayCfgDesc.LAN_AUTH_TYPE])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, writes[-1].body[GatewayCfgDesc.LAN_AUTH_TYPE])
        self.assertEqual(runner.temporary_rw_key, writes[-1].bearer_token)

    def test_blocked_rw_in_deny_is_error_with_unverified_restoration(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)

        def block(_session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            if self.gateway.mode == GatewayCfgLanAuthType.DENY and call.bearer_token is not None:
                return FakeResponse(403, {})
            return None

        with mock.patch.object(self.gateway, "_response_override", side_effect=block):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("ERROR", result.outcomes[AuthMech.LAN_WEBUI_DISABLED])
        self.assertEqual("ERROR", result.outcomes["final restoration"])
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)
        self.assertEqual(GatewayCfgLanAuthType.DENY, self.gateway.mode)

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
        session: requests.Session = runner.gateway.authenticate_interactive("Admin", CONFIG.gw_id).session
        with self.assertRaises(target.SecurityFailure):
            runner._measure_success(
                AuthMech.LAN_WEBUI_DEFAULT,
                session,
            )
        self.assertEqual("FAIL", runner.outcomes[AuthMech.LAN_WEBUI_DEFAULT])

    def test_unauthenticated_mode_does_not_enforce_response_timing(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        runner.success_monotonic_ns = AdvancingClock(600_000_000)
        self.gateway.mode = GatewayCfgLanAuthType.ALLOW
        runner._measure_http_auth_campaign(
            AuthMech.LAN_WEBUI_UNAUTHENTICATED,
            True,
            HttpStatus.C_200_OK,
            lambda: runner.gateway.request(
                FakeSession(self.gateway), HttpMethod.GET, GatewayApi.STATUS,
            ),
            True,
            enforce_timing=False,
        )
        self.assertEqual(
            "PASS",
            runner.outcomes[AuthMech.LAN_WEBUI_UNAUTHENTICATED],
        )

    def test_digest_challenge_is_prepared_before_response_timing(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        self.gateway.mode = GatewayCfgLanAuthType.DIGEST
        self.gateway.custom_username = "configured"
        self.gateway.custom_ha1 = runner.gateway.calculate_digest_ha1("configured", "Ruuvi Gateway", "password")
        runner._measure_http_auth_campaign(
            AuthMech.LAN_WEBUI_DIGEST, True, HttpStatus.C_200_OK,
            lambda: runner._prepare_digest_request("configured", "password"),
            True, prepare_before_timing=True,
        )
        self.assertEqual([GatewayApi.STATUS, GatewayApi.STATUS], [call.path for call in self.gateway.calls])
        self.assertEqual("", self.gateway.calls[0].authorization)
        self.assertTrue(self.gateway.calls[1].authorization.startswith("Digest "))
        self.assertEqual([100_000_000], runner.success_durations_ns[AuthMech.LAN_WEBUI_DIGEST])

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
                gateway: FakeGateway = FakeGateway(CONFIG)
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

    def test_campaign_evidence_records_actual_request_routes(self) -> None:
        self.assertEqual(0, self.make_runner(attempt_count=2).run().exit_code)
        lines: list[str] = self.log.path.read_text(encoding="utf-8").splitlines()
        attempts: list[dict[str, Any]] = [
            json.loads(line.split("CAMPAIGN ATTEMPT: ", 1)[1])
            for line in lines if "CAMPAIGN ATTEMPT: " in line
        ]
        expected: dict[str, tuple[str, str]] = {
            AuthMech.LAN_WEBUI_DEFAULT: (HttpMethod.POST, GatewayApi.AUTH),
            AuthMech.LAN_WEBUI_USER_DEFINED: (HttpMethod.POST, GatewayApi.AUTH),
            AuthMech.M2M_API_BEARER_RO: (HttpMethod.GET, GatewayApi.HISTORY),
            AuthMech.M2M_API_BEARER_RW: (HttpMethod.POST, GatewayApi.CONFIG),
        }
        self.assertEqual(8, len(attempts))
        entry: dict[str, Any]
        for entry in attempts:
            self.assertEqual(expected[entry["campaign"]], (entry["method"], entry["path"]))
        campaign: str
        for campaign in expected:
            self.assertEqual([1, 2], [entry["attempt"] for entry in attempts if entry["campaign"] == campaign])

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
                gateway: FakeGateway = FakeGateway(CONFIG)
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

        gateway: FakeGateway = FakeGateway(CONFIG)
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

        gateway = FakeGateway(CONFIG)
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

    def test_restoration_evidence_retains_error_and_successful_fallback(self) -> None:
        original: Callable[..., FakeResponse] = self.gateway.response_for

        def response(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any
        ) -> FakeResponse:
            if (
                method == HttpMethod.POST and path == GatewayApi.CONFIG
                and isinstance(body, dict)
                and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT
                and headers.get("Authorization", "").startswith("Bearer ")
            ):
                raise requests.Timeout("restoration reply lost")
            return original(session, method, path, headers, body)

        with mock.patch.object(self.gateway, "response_for", side_effect=response):
            result: RunResult = self.make_runner(attempt_count=1).run()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])
        lines: list[str] = self.log.path.read_text(encoding="utf-8").splitlines()
        records: list[list[dict[str, Any]]] = [
            json.loads(line.split("RESTORATION ATTEMPTS: ", 1)[1])
            for line in lines if "RESTORATION ATTEMPTS: " in line
        ]
        self.assertEqual(1, len(records))
        self.assertEqual(2, len(records[0]))
        self.assertEqual("temporary RW bearer", records[0][0]["method"])
        self.assertEqual("GatewayConnectionError", records[0][0]["error"])
        self.assertIn("restoration reply lost", records[0][0]["message"])
        self.assertEqual({"method": "temporary custom interactive", "status": 200}, records[0][1])

    def test_legacy_abort_recovers_after_primary_restore_failure(self) -> None:
        mode: str
        for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
            with self.subTest(mode=mode):
                gateway: FakeGateway = FakeGateway(CONFIG)
                gateway.primary_restore_status = HttpStatus.C_500_INTERNAL_SERVER_ERROR
                original: Callable[..., FakeResponse] = gateway.response_for

                def response(
                    session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any,
                    fixture: FakeGateway = gateway, active_mode: str = mode,
                    transport: Callable[..., FakeResponse] = original,
                ) -> FakeResponse:
                    reply: FakeResponse = transport(session, method, path, headers, body)
                    if fixture.mode == active_mode and method == HttpMethod.GET and path == GatewayApi.STATUS:
                        raise requests.Timeout("legacy campaign aborted")
                    return reply

                runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
                runner.gateway.session_factory = lambda fixture=gateway: FakeSession(fixture)
                with mock.patch.object(gateway, "response_for", side_effect=response):
                    result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("PASS", result.outcomes["final restoration"])
                self.assertEqual("ERROR", result.outcomes[
                    AuthMech.LAN_WEBUI_BASIC if mode == GatewayCfgLanAuthType.BASIC else AuthMech.LAN_WEBUI_DIGEST
                ])
                self.assertIsNone(result.recovery_message)
                self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
                restored_calls: list[RecordedRequest] = [
                    call for call in gateway.calls if call.method == HttpMethod.POST
                    and call.path == GatewayApi.CONFIG and isinstance(call.body, dict)
                    and call.body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT
                ]
                self.assertEqual(2, len(restored_calls))
                self.assertTrue(restored_calls[0].authorization.startswith("Bearer "))
                self.assertTrue(restored_calls[1].authorization.startswith(
                    "Basic " if mode == GatewayCfgLanAuthType.BASIC else "Digest "
                ))

    def test_failed_digest_transition_keeps_previous_basic_recovery(self) -> None:
        self.check_lost_digest_transition(applied=False)

    def test_applied_digest_transition_with_lost_reply_keeps_new_recovery(self) -> None:
        self.check_lost_digest_transition(applied=True)

    def check_lost_digest_transition(self, applied: bool) -> None:
        self.gateway.primary_restore_status = HttpStatus.C_500_INTERNAL_SERVER_ERROR
        original: Callable[..., FakeResponse] = self.gateway.response_for

        def response(
            session: FakeSession, method: str, path: str, headers: dict[str, str], body: Any
        ) -> FakeResponse:
            if (method == HttpMethod.POST and path == GatewayApi.CONFIG and isinstance(body, dict)
                    and body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DIGEST):
                if applied:
                    original(session, method, path, headers, body)
                raise requests.Timeout("Digest transition not applied")
            return original(session, method, path, headers, body)

        with mock.patch.object(self.gateway, "response_for", side_effect=response):
            result: RunResult = self.make_runner(attempt_count=1).run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual("PASS", result.outcomes[AuthMech.LAN_WEBUI_BASIC])
        self.assertEqual("ERROR", result.outcomes[AuthMech.LAN_WEBUI_DIGEST])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_progress_and_persistent_evidence_failures_do_not_skip_recovery(self) -> None:
        boundary: str
        for boundary in ("progress", "evidence"):
            with self.subTest(boundary=boundary):
                gateway: FakeGateway = FakeGateway(CONFIG)
                runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
                runner.gateway.session_factory = lambda fixture=gateway: FakeSession(fixture)
                original_write: Callable[[str, Any], None] = self.log.write

                def write(
                    label: str, value: Any = "", active_runner: target.FunctionalTest_5_1_5_2_b = runner,
                    active_boundary: str = boundary, delegate: Callable[[str, Any], None] = original_write,
                ) -> None:
                    if active_runner.mutation_possible and active_boundary == "evidence":
                        raise OSError("evidence sink closed")
                    delegate(label, value)

                def progress(
                    _message: str, active_runner: target.FunctionalTest_5_1_5_2_b = runner,
                    active_boundary: str = boundary,
                ) -> None:
                    if active_runner.mutation_possible and active_boundary == "progress":
                        raise BrokenPipeError("stdout closed")

                runner.progress = progress
                with mock.patch.object(self.log, "write", side_effect=write):
                    result: RunResult = runner.run()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("PASS", result.outcomes["final restoration"])
                self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
                self.assertEqual("", gateway.rw_key)
                self.assertIsNone(result.recovery_message)

    def test_response_logging_failure_preserves_control_probe_bypasses(self) -> None:
        boundary: str
        for boundary in ("Digest challenge", "RO scope"):
            with self.subTest(boundary=boundary):
                self.check_control_probe_logging_failure(boundary)

    def check_control_probe_logging_failure(self, boundary: str) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        runner.gateway.session_factory = lambda fixture=gateway: FakeSession(fixture)
        accepted: list[requests.Response] = []
        original: Callable[..., FakeResponse | None] = gateway._response_override
        original_log: Callable[[requests.Response], None] = self.log.write_http_response

        def override(
            session: FakeSession, call: RecordedRequest,
            phase: str = boundary, active: target.FunctionalTest_5_1_5_2_b = runner,
            delegate: Callable[..., FakeResponse | None] = original,
        ) -> FakeResponse | None:
            digest: bool = (
                phase == "Digest challenge" and gateway.mode == GatewayCfgLanAuthType.DIGEST
                and call.path == GatewayApi.STATUS and not call.authorization
                and AuthMech.LAN_WEBUI_DIGEST not in active.campaigns
            )
            ro_scope: bool = (
                phase == "RO scope" and call.method == HttpMethod.POST
                and call.path == GatewayApi.CONFIG and call.bearer_token == active.temporary_ro_key
                and bool(active.temporary_ro_key)
            )
            if not accepted and (digest or ro_scope):
                response: FakeResponse = FakeResponse(200, {})
                accepted.append(response)
                return response
            return delegate(session, call)

        def report(
            response: requests.Response, delegate: Callable[[requests.Response], None] = original_log,
        ) -> None:
            if accepted and response is accepted[0]:
                raise OSError("control response could not be logged")
            delegate(response)

        with mock.patch.object(gateway, "_response_override", side_effect=override), mock.patch.object(
            self.log, "write_http_response", side_effect=report,
        ):
            result: RunResult = runner.run()
        mechanism: str = AuthMech.LAN_WEBUI_DIGEST if boundary == "Digest challenge" else AuthMech.M2M_API_BEARER_RO
        self.assertEqual(1, len(accepted))
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
        self.assertNotIn(AuthMech.LAN_WEBUI_DIGEST, runner.campaigns)
        if boundary == "RO scope":
            self.assertNotIn(AuthMech.M2M_API_BEARER_RW, runner.campaigns)

    def test_http_mode_aborted_campaign_keeps_completed_latencies_in_summary(self) -> None:
        mechanism: str
        fault: str
        for mechanism in (AuthMech.LAN_WEBUI_BASIC, AuthMech.LAN_WEBUI_DIGEST, AuthMech.LAN_WEBUI_DISABLED):
            for fault in ("timeout", "accepted"):
                with self.subTest(mechanism=mechanism, fault=fault):
                    self.check_aborted_campaign_summary(mechanism, fault)

    def check_aborted_campaign_summary(self, mechanism: str, fault: str) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=3)
        responses: list[FakeResponse | Exception] = [
            FakeResponse(401, {}),
            target.GatewayConnectionError("timeout") if fault == "timeout" else FakeResponse(200, {}),
        ]
        messages: list[str] = []
        runner.campaign_progress = messages.append

        def operation() -> requests.Response:
            response: FakeResponse | Exception = responses.pop(0)
            if isinstance(response, Exception):
                raise response
            runner.gateway.last_transport_timing = target.TransportTiming(0, 2_100_000_000)
            runner.gateway.last_response = response
            return response

        with self.assertRaises(target.GatewayConnectionError if fault == "timeout" else target.UnexpectedAuthorization):
            runner._measure_http_auth_campaign(mechanism, False, 401, operation, True)
        stats: target.CampaignStats = runner.campaigns[mechanism]
        self.assertEqual([2_100_000_000] * (1 if fault == "timeout" else 2), stats.durations_ns)
        runner._print_summary()
        summary: str = next(message for message in messages if message.startswith(f"{mechanism}: result="))
        self.assertIn("failed average response time=2.100000000 seconds", summary)
        self.assertIn("failed minimum response time=2.100000000 seconds", summary)

    def test_response_reporting_failure_preserves_bypass_in_every_negative_campaign(self) -> None:
        mechanism: str
        for mechanism in target.AUTH_MECHANISMS:
            if mechanism == AuthMech.LAN_WEBUI_UNAUTHENTICATED:
                continue
            with self.subTest(mechanism=mechanism):
                self.check_response_reporting_failure(mechanism)

    def test_login_response_reporting_failure_preserves_bypass_after_challenge(self) -> None:
        mechanism: str
        for mechanism in (AuthMech.LAN_WEBUI_DEFAULT, AuthMech.LAN_WEBUI_USER_DEFINED, AuthMech.LAN_WEBUI_DIGEST):
            with self.subTest(mechanism=mechanism):
                self.check_response_reporting_failure(mechanism, skip_challenge=True)

    def check_response_reporting_failure(self, mechanism: str, skip_challenge: bool = False) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        runner.gateway.session_factory = lambda fixture=gateway: FakeSession(fixture)
        original: Callable[..., FakeResponse | None] = gateway._response_override
        accepted: list[requests.Response] = []

        def override(
            session: FakeSession, call: RecordedRequest,
            active: str = mechanism, active_runner: target.FunctionalTest_5_1_5_2_b = runner,
            delegate: Callable[..., FakeResponse | None] = original,
        ) -> FakeResponse | None:
            campaign: target.CampaignStats | None = active_runner.campaigns.get(active)
            negative_stage: bool = not skip_challenge or (
                call.method == HttpMethod.POST if active != AuthMech.LAN_WEBUI_DIGEST else bool(call.authorization)
            )
            if campaign is not None and not accepted and negative_stage:
                response: FakeResponse = FakeResponse(200, {})
                accepted.append(response)
                return response
            return delegate(session, call)

        original_log: Callable[[requests.Response], None] = self.log.write_http_response

        def report_response(
            response: requests.Response,
            delegate: Callable[[requests.Response], None] = original_log,
        ) -> None:
            if accepted and response is accepted[0]:
                raise OSError("received bypass cannot be logged")
            delegate(response)

        with mock.patch.object(gateway, "_response_override", side_effect=override), mock.patch.object(
            self.log, "write_http_response", side_effect=report_response,
        ):
            result: RunResult = runner.run()
        self.assertEqual(1, len(accepted))
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[mechanism])
        self.assertEqual(1, runner.campaigns[mechanism].authorized)
        self.assertEqual("NOT RUN" if mechanism == AuthMech.LAN_WEBUI_DEFAULT else "PASS", result.outcomes["final restoration"])
        self.assertLessEqual(runner.campaigns[mechanism].attempted, 1)
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)

    def test_completed_security_fail_is_sticky_after_later_transport_error(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)
        runner.success_monotonic_ns = AdvancingClock(300_000_000)
        with mock.patch.object(runner, "_attack_interactive", side_effect=target.GatewayConnectionError("later timeout")):
            result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
        self.assertEqual("NOT RUN", result.outcomes["final restoration"])

    def test_exhausted_retries_keep_real_bounds_without_incomplete_latency_samples(self) -> None:
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=2)
        original: Callable[..., FakeResponse | None] = self.gateway._response_override
        count: int = 0

        def override(session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
            nonlocal count
            if call.bearer_token is not None:
                count += 1
                if count > 1:
                    raise requests.Timeout("retry exhaustion")
            return original(session, call)

        with mock.patch.object(self.gateway, "_response_override", side_effect=override), self.assertRaises(target.GatewayConnectionError):
            runner._attack_bearer(AuthMech.M2M_API_BEARER_RO, GatewayApi.HISTORY, set())
        stats: target.CampaignStats = runner.campaigns[AuthMech.M2M_API_BEARER_RO]
        self.assertEqual(1, len(stats.durations_ns))
        self.assertEqual(2_100_000_000, stats.durations_ns[0])
        self.assertGreater(stats.as_evidence().elapsed_seconds, 0)
        transport: target.TransportTiming | None = runner.gateway.last_transport_timing
        if transport is None:
            self.fail("missing failed transport bounds")
        self.assertEqual(transport.end_ns, stats.last_end_ns)

    def test_reporting_error_preserves_earlier_security_failure(self) -> None:
        self.gateway.bearer_guess_status = HttpStatus.C_200_OK
        runner: target.FunctionalTest_5_1_5_2_b = self.make_runner(attempt_count=1)

        def progress(message: str) -> None:
            if "Restoring the original" in message:
                raise BrokenPipeError("stdout closed during cleanup")

        runner.progress = progress
        result: RunResult = runner.run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[AuthMech.M2M_API_BEARER_RO])
        self.assertEqual("PASS", result.outcomes[AuthMech.LAN_WEBUI_DEFAULT])
        self.assertEqual("PASS", result.outcomes["final restoration"])
        self.assertEqual(GatewayCfgLanAuthType.DEFAULT, self.gateway.mode)

    def test_restoration_failure_or_verification_mismatch_prevents_pass(self) -> None:
        attribute: str
        for attribute in ("restored_config_mismatch", "temporary_bearers_survive"):
            with self.subTest(attribute=attribute):
                gateway: FakeGateway = FakeGateway(CONFIG)
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

        gateway = FakeGateway(CONFIG)
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
    def test_evidence_lifecycle_setup_failures_return_error_without_dut_access(self) -> None:
        boundary: str
        for boundary in ("create", "preamble", "initial output"):
            with self.subTest(boundary=boundary):
                self.check_evidence_lifecycle_failure(boundary)

    def check_evidence_lifecycle_failure(self, boundary: str) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            messages: list[str] = []
            sessions: mock.Mock = mock.Mock(side_effect=AssertionError("DUT must not be contacted"))
            original_write: Callable[[target.EvidenceLog, str, Any], None] = target.EvidenceLog.write

            def write(log: target.EvidenceLog, label: str, value: Any = "") -> None:
                if label == "TEST CASE AND UNIT" and boundary == "preamble":
                    raise OSError("secret detail")
                original_write(log, label, value)

            def output(message: str) -> None:
                if message.startswith("Open log file:") and boundary == "initial output":
                    raise ValueError("secret detail")
                messages.append(message)

            with mock.patch.object(target.EvidenceLog, "write", autospec=True, side_effect=write):
                if boundary == "create":
                    with mock.patch.object(target.EvidenceLog, "create", side_effect=OSError("secret detail")):
                        result: RunResult = target.execute_test_5_1_5_2_b(root, session_factory=sessions, output=output)
                else:
                    result = target.execute_test_5_1_5_2_b(root, session_factory=sessions, output=output)
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertTrue(all(value == "NOT RUN" for value in result.outcomes.values()))
            sessions.assert_not_called()
            self.assertEqual("Overall verdict: ERROR", messages[-1])
            self.assertNotIn("secret detail", "\n".join(messages))

    def test_finish_failure_corrects_terminal_verdict_after_passing_run(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            (root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
            gateway: FakeGateway = FakeGateway(CONFIG)
            messages: list[str] = []
            def finish(log: target.EvidenceLog, _verdict: str, _now: Callable[[], datetime]) -> None:
                log._stream.close()
                raise OSError("disk full")

            with mock.patch.object(target.EvidenceLog, "finish", autospec=True, side_effect=finish):
                result: RunResult = target.execute_test_5_1_5_2_b(
                    root, session_factory=lambda: FakeSession(gateway), now=lambda: FIXED_NOW,
                    monotonic_ns=AdvancingClock(2_100_000_000),
                    success_monotonic_ns=AdvancingClock(100_000_000), attempt_count=1,
                    random_bytes=UniqueRandom(), output=messages.append,
                )
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertIn("Overall verdict: PASS", messages)
            self.assertEqual("Overall verdict: ERROR", messages[-1])
            self.assertEqual("PASS", result.outcomes["final restoration"])

    def test_output_failures_preserve_restoration_and_return_result(self) -> None:
        trigger: str
        for trigger in ("Restoring the original", "result=", "Overall verdict:"):
            with self.subTest(trigger=trigger):
                self.check_output_failure(trigger)

    def check_output_failure(self, trigger: str) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            (root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
            gateway: FakeGateway = FakeGateway(CONFIG)
            broken: bool = False

            def output(message: str) -> None:
                nonlocal broken
                broken = broken or trigger in message
                if broken:
                    raise BrokenPipeError("terminal consumer closed")

            result: RunResult = target.execute_test_5_1_5_2_b(
                root, session_factory=lambda: FakeSession(gateway), now=lambda: FIXED_NOW,
                monotonic_ns=AdvancingClock(2_100_000_000),
                success_monotonic_ns=AdvancingClock(100_000_000), attempt_count=1,
                random_bytes=UniqueRandom(), output=output,
            )
            self.assertTrue(broken)
            self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
            self.assertEqual("PASS", result.outcomes["final restoration"])
            self.assertTrue(all(value == "PASS" for value in result.outcomes.values()))
            self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
            self.assertEqual("", gateway.rw_key)
            self.assertIsNone(result.recovery_message)

    def test_execute_log_and_terminal_progress_match_shared_pattern(self) -> None:
        directory: str
        with tempfile.TemporaryDirectory() as directory:
            root: Path = Path(directory)
            (root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
            gateway: FakeGateway = FakeGateway(CONFIG)
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
            (root / ".env").write_text(env_text(CONFIG), encoding="utf-8")
            gateway: FakeGateway = FakeGateway(CONFIG)
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
