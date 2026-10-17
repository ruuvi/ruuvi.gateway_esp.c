"""Offline implementation checks, not live ETSI compliance evidence."""

from __future__ import annotations

import importlib
import tempfile
import unittest
from dataclasses import replace
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest import mock

import requests

import test_5_3_13_2_b as runner
from lib.errors import WebResourceConnectionError, WebResourceProtocolError, WebResourceRedirectError
from lib.evidence import EvidenceLog
from lib.models import RunResult
from lib.webresource import PublicResourceResult, RedirectHop, fetch_public_resource
from test_support.fake_gateway import FakeResponse

NOW: datetime = datetime(2026, 9, 21, 1, 2, 3, tzinfo=timezone.utc)
PUBLIC_PAGE: PublicResourceResult = PublicResourceResult(
    200, "https://ruuvi.com/terms/lifecycle-promises/", "https", "ruuvi.com",
    (("Content-Type", "text/html; charset=UTF-8"),), (), "text/html; charset=UTF-8", 10, (), False,
)


class SupportPeriodAccessibilityTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.directory: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root: Path = Path(self.directory.name)
        self.output: list[str] = []
        self.session: requests.Session = requests.Session()
        self.addCleanup(self.session.close)
        self.fetch: mock.Mock = mock.Mock(return_value=PUBLIC_PAGE)

    def execute(self) -> RunResult:
        clock: mock.Mock = mock.Mock(side_effect=(NOW, NOW + timedelta(seconds=2)))
        with mock.patch.object(runner.webresource, "fetch_public_resource", self.fetch):
            return runner.execute_test_5_3_13_2_b(
                self.root, session_factory=lambda: self.session, now=clock, output=self.output.append,
            )

    def transcript(self) -> str:
        messages: list[str] = [line for line in self.output if line.startswith("Open log file: ")]
        log_path: Path = Path(messages[-1].partition(": ")[2])
        return log_path.read_text(encoding="utf-8")

    def test_exact_policy_delegation_and_pass_evidence(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((0, "PASS"), (result.exit_code, result.verdict))
        self.assertEqual({"SupportPeriodPublication-UnrestrictedPublicAccess": "PASS"}, result.outcomes)
        self.assertEqual("https://ruuvi.com/terms/lifecycle-promises/", runner.TARGET_URL)
        self.assertEqual({"ruuvi.com", "www.ruuvi.com"}, runner.ALLOWED_HOSTS)
        self.assertEqual({200}, runner.ALLOWED_FINAL_STATUS)
        self.assertEqual(1, runner.MIN_BODY_BYTES)
        self.fetch.assert_called_once()
        self.assertEqual(runner.TARGET_URL, self.fetch.call_args.args[0])
        self.assertIsInstance(self.fetch.call_args.args[1], EvidenceLog)
        self.assertEqual(runner.ALLOWED_HOSTS, self.fetch.call_args.kwargs["allowed_hosts"])
        self.assertEqual(5, self.fetch.call_args.kwargs["connect_timeout"])
        self.assertEqual(20, self.fetch.call_args.kwargs["read_timeout"])
        self.assertEqual(5, self.fetch.call_args.kwargs["max_redirects"])
        self.assertEqual("ruuvi-etsi-test-5.3-13-2-b", self.fetch.call_args.kwargs["user_agent"])
        self.assertIs(self.session, self.fetch.call_args.kwargs["session_factory"]())
        self.assertEqual(6, len(self.output))
        self.assertTrue(self.output[0].startswith("Open log file: "))
        self.assertEqual("Overall verdict: PASS", self.output[-1])
        index: int
        message: str
        for index, message in enumerate(self.output[1:-1], 1):
            self.assertTrue(message.startswith(f"[Step {index} out of 4]"))
            self.assertRegex(self.transcript(), rf"\[\d{{4}}-\d{{2}}-\d{{2}}T[^\]]+Z\] \[Step {index} out of 4\]")
            self.assertIn(message, self.transcript())
        self.assertIn(runner.TEST_ID, self.transcript())
        self.assertIn("UTC START: 2026-09-21T01:02:03Z", self.transcript())
        self.assertIn("UTC END: 2026-09-21T01:02:05Z", self.transcript())
        self.assertIn("DURATION SECONDS: 2.000", self.transcript())
        self.assertEqual(9, self.transcript().count("ASSERTION:"))
        self.assertNotIn('"result": "FAIL"', self.transcript())
        self.assertIn("OVERALL VERDICT: PASS", self.transcript())

    def test_accessibility_failure_matrix(self) -> None:
        statuses: tuple[int, ...] = (201, 204, 300, 304, 400, 401, 403, 404, 410)
        cases: tuple[PublicResourceResult, ...] = (
            *(replace(PUBLIC_PAGE, status=status) for status in statuses),
            replace(PUBLIC_PAGE, authentication_seen=True),
            replace(PUBLIC_PAGE, final_scheme="http"),
            replace(PUBLIC_PAGE, final_host="login.example"),
            replace(PUBLIC_PAGE, content_type="application/pdf"),
            replace(PUBLIC_PAGE, content_type=""),
            replace(PUBLIC_PAGE, body_length=0),
            replace(PUBLIC_PAGE, redirects=(RedirectHop(302, PUBLIC_PAGE.final_url, "http://ruuvi.com/page"),)),
            replace(PUBLIC_PAGE, redirects=(RedirectHop(302, PUBLIC_PAGE.final_url, "https://paywall.example/"),)),
            replace(PUBLIC_PAGE, stopped_at_redirect=True),
            replace(PUBLIC_PAGE, tls_verified=False),
        )
        observation: PublicResourceResult
        for observation in cases:
            with self.subTest(observation=observation):
                self.fetch.return_value = observation
                result: RunResult = self.execute()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[runner.MECHANISM])
                self.assertIn('"result": "FAIL"', self.transcript())
                self.assertIn("OVERALL VERDICT: FAIL", self.transcript())

    def test_allowed_www_redirect_and_minimum_html_body(self) -> None:
        self.fetch.return_value = replace(
            PUBLIC_PAGE, final_url="https://www.ruuvi.com/lifecycle", final_host="www.ruuvi.com",
            content_type="TEXT/HTML", body_length=1,
            redirects=(RedirectHop(301, PUBLIC_PAGE.final_url, "https://www.ruuvi.com/lifecycle"),),
        )
        self.assertEqual(0, self.execute().exit_code)

    def test_every_server_error_and_rate_limit_are_error(self) -> None:
        statuses: tuple[int, ...] = (429, *range(500, 600))
        status: int
        for status in statuses:
            with self.subTest(status=status):
                # Transient error pages may have no HTML or body; these are not compliance failures.
                self.fetch.return_value = replace(PUBLIC_PAGE, status=status, content_type="", body_length=0)
                result: RunResult = self.execute()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes[runner.MECHANISM])
                self.assertIn("EXCEPTION TYPE: InvalidSetup", self.transcript())
                self.assertNotIn('"result": "FAIL"', self.transcript())

    def test_infrastructure_and_unexpected_errors_create_evidence(self) -> None:
        errors: tuple[Exception, ...] = (
            WebResourceConnectionError("DNS failed"), WebResourceConnectionError("TLS failed"),
            WebResourceConnectionError("refused"), WebResourceConnectionError("connect timeout"),
            WebResourceConnectionError("read timeout"), WebResourceRedirectError("loop"),
            WebResourceRedirectError("overflow"), WebResourceProtocolError("missing Location"),
            RuntimeError("unexpected"),
        )
        error: Exception
        for error in errors:
            with self.subTest(error=error):
                self.fetch.side_effect = error
                result: RunResult = self.execute()
                self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
                self.assertEqual("ERROR", result.outcomes[runner.MECHANISM])
                self.assertIn(f"EXCEPTION TYPE: {type(error).__name__}", self.transcript())
                self.assertIn("Traceback", self.transcript())
                self.assertIn("OVERALL VERDICT: ERROR", self.transcript())

    def test_prior_challenge_failure_survives_later_error(self) -> None:
        self.fetch.side_effect = WebResourceConnectionError(
            "second hop timed out", replace(PUBLIC_PAGE, status=302, authentication_seen=True),
        )
        result: RunResult = self.execute()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[runner.MECHANISM])
        self.assertIn('"description": "no WWW-Authenticate challenge anywhere", "result": "FAIL"', self.transcript())
        self.assertIn('"mechanism": "SupportPeriodPublication-UnrestrictedPublicAccess", "result": "FAIL"', self.transcript())

    def test_transient_with_challenge_keeps_failure(self) -> None:
        self.fetch.return_value = replace(PUBLIC_PAGE, status=503, authentication_seen=True)
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[runner.MECHANISM])

    def test_full_run_delegates_http_evidence_and_never_loads_dut_configuration(self) -> None:
        responses: list[FakeResponse] = [
            FakeResponse(302, headers={"Location": "https://www.ruuvi.com/lifecycle"}),
            FakeResponse(payload="unrelated public content", headers={"Content-Type": "text/html"},
                         cookies={"visitor": "secret-cookie"}),
        ]
        send: mock.Mock
        self.fetch = mock.Mock(wraps=fetch_public_resource)
        with mock.patch.object(self.session, "send", side_effect=responses) as send, \
                mock.patch("lib.config.load_dut_config", side_effect=AssertionError("DUT config accessed")), \
                mock.patch("lib.gateway.GatewayClient", side_effect=AssertionError("DUT client created")):
            result: RunResult = self.execute()
        self.assertEqual(0, result.exit_code)
        self.fetch.assert_called_once()
        calls: list[mock._Call] = send.call_args_list
        actual_requests: list[tuple[str, str, str | bytes | None]] = [
            (call.args[0].method, call.args[0].url, call.args[0].body) for call in calls
        ]
        self.assertEqual(
            [("GET", "https://ruuvi.com/terms/lifecycle-promises/", None),
             ("GET", "https://www.ruuvi.com/lifecycle", None)], actual_requests,
        )
        verification_options: list[bool] = [call.kwargs["verify"] for call in calls]
        redirect_options: list[bool] = [call.kwargs["allow_redirects"] for call in calls]
        timeout_options: list[tuple[float, float]] = [call.kwargs["timeout"] for call in calls]
        self.assertEqual([True, True], verification_options)
        self.assertEqual([False, False], redirect_options)
        self.assertEqual([(5, 20), (5, 20)], timeout_options)
        self.assertFalse(self.session.trust_env)
        self.assertIn("REDIRECT HOP:", self.transcript())
        self.assertIn("secret-cookie", self.transcript())
        self.assertNotIn("secret-cookie", "\n".join(self.output))
        self.assertEqual(2, self.transcript().count("HTTP REQUEST BEGIN:"))
        self.assertEqual(2, self.transcript().count("HTTP RESPONSE BEGIN:"))

    def test_product_name_timeframe_and_placeholder_do_not_affect_verdict(self) -> None:
        pages: tuple[str, ...] = (
            "<html>Unrelated content</html>",
            "<html>Ruuvi Gateway: ? years</html>",
            "<html>Ruuvi Gateway: 5 years</html>",
        )
        page: str
        self.fetch = mock.Mock(wraps=fetch_public_resource)
        for page in pages:
            with self.subTest(page=page), mock.patch.object(
                self.session, "send", return_value=FakeResponse(payload=page, headers={"Content-Type": "text/html"}),
            ):
                self.assertEqual(0, self.execute().exit_code)

    def test_real_fetch_restrictions_produce_case_fail(self) -> None:
        responses: tuple[FakeResponse, ...] = (
            FakeResponse(302, headers={"Location": "https://login.example/"}),
            FakeResponse(302, headers={"Location": "https://registration.example/"}),
            FakeResponse(302, headers={"Location": "https://paywall.example/"}),
            FakeResponse(302, headers={"Location": "http://ruuvi.com/lifecycle"}),
            FakeResponse(headers={"Content-Type": "text/html", "www-authenticate": ""}, payload="page"),
        )
        response: FakeResponse
        send: mock.Mock
        self.fetch = mock.Mock(wraps=fetch_public_resource)
        for response in responses:
            with self.subTest(headers=response.headers), mock.patch.object(
                self.session, "send", return_value=response,
            ) as send:
                result: RunResult = self.execute()
                self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                self.assertEqual("FAIL", result.outcomes[runner.MECHANISM])
                send.assert_called_once()

    def test_malformed_redirect_after_challenge_preserves_failure(self) -> None:
        self.fetch = mock.Mock(wraps=fetch_public_resource)
        response: FakeResponse = FakeResponse(
            302, headers={"Location": "https://[broken", "WWW-Authenticate": "Basic realm=private"},
        )
        with mock.patch.object(self.session, "send", return_value=response):
            result: RunResult = self.execute()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual("FAIL", result.outcomes[runner.MECHANISM])
        self.assertIn("HTTP STATUS 302", self.transcript())
        self.assertIn("OVERALL VERDICT: ERROR", self.transcript())

    def test_session_factory_failure_still_finishes_log(self) -> None:
        factory: mock.Mock = mock.Mock(side_effect=RuntimeError("session setup failed"))
        result: RunResult = runner.execute_test_5_3_13_2_b(
            self.root, session_factory=factory, now=lambda: NOW, output=self.output.append,
        )
        self.assertEqual(2, result.exit_code)
        self.assertIn("session setup failed", self.transcript())
        self.assertIn("OVERALL VERDICT: ERROR", self.transcript())
        self.assertEqual(1, sum(line.startswith("Open log file: ") for line in self.output))

    def test_exclusive_log_names_and_main_exit_codes(self) -> None:
        self.execute()
        self.execute()
        self.assertEqual(2, len(list((self.root / "logs").glob("test_5_3_13_2_b_20260921T010203.000000Z*.log"))))
        code: int
        verdict: str
        cases: tuple[tuple[int, str], ...] = ((0, "PASS"), (1, "FAIL"), (2, "ERROR"))
        for code, verdict in cases:
            with self.subTest(verdict=verdict), mock.patch.object(
                runner, "execute_test_5_3_13_2_b", return_value=RunResult(code, verdict, {}, set()),
            ):
                self.assertEqual(code, runner.main())

    def test_import_has_no_network_or_execution_side_effects(self) -> None:
        with mock.patch("requests.Session", side_effect=AssertionError("session created on import")), \
                mock.patch("lib.evidence.EvidenceLog.create", side_effect=AssertionError("log created on import")), \
                mock.patch("lib.config.load_dut_config", side_effect=AssertionError("DUT config accessed")):
            importlib.reload(runner)
        # Restore defaults captured during the guarded import.
        importlib.reload(runner)
        self.assertFalse((self.root / "logs").exists())


if __name__ == "__main__":
    unittest.main()
