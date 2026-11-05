"""Public VDP accessibility only; policy wording and completeness are outside Unit A."""

from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path
from typing import Callable
from urllib.parse import urlsplit

import requests

from lib import webresource
from lib.errors import InvalidSetup, WebResourceError
from lib.evidence import AssertionEvidence, EvidenceLog, MechanismResultEvidence, format_utc, utc_now
from lib.http_api import HttpStatus
from lib.models import ProgressReporter, RunResult
from lib.webresource import PublicResourceResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.2-1-2, Test Unit A"
TARGET_URL: str = "https://ruuvi.com/terms/vulnerability-policy/"
ALLOWED_HOSTS: frozenset[str] = frozenset({"ruuvi.com", "www.ruuvi.com"})
ALLOWED_FINAL_STATUS: frozenset[int] = frozenset({HttpStatus.C_200_OK})
CONNECT_TIMEOUT_SECONDS: int = 5
READ_TIMEOUT_SECONDS: int = 20
MAX_REDIRECTS: int = 5
MIN_BODY_BYTES: int = 1
USER_AGENT: str = "ruuvi-etsi-test-5.2-1-2-a"
MECHANISM: str = "VulnerabilityDisclosurePolicy-PublicAccessibility"
TOTAL_STEPS: int = 4


class FunctionalTest_5_2_1_2_a:
    def __init__(
        self,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        progress: Callable[[str], None] = print,
    ) -> None:
        self.evidence: EvidenceLog = evidence
        self.session_factory: Callable[[], requests.Session] = session_factory
        self.progress: Callable[[str], None] = progress
        self.failed: bool = False

    def _assert(self, description: str, passed: bool, actual: str | int | bool) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if passed else "FAIL", actual))
        self.failed = self.failed or not passed

    def _check_authentication(self, result: PublicResourceResult) -> None:
        self._assert("no WWW-Authenticate challenge anywhere", not result.authentication_seen,
                     result.authentication_seen)

    def _check_route(self, result: PublicResourceResult) -> None:
        self._check_authentication(result)
        self._assert("final scheme and every redirect use HTTPS",
                     result.final_scheme == "https"
                     and all(urlsplit(hop.to_url).scheme == "https" for hop in result.redirects),
                     result.final_scheme)
        self._assert("final host and every redirect host are allowed",
                     result.final_host in ALLOWED_HOSTS
                     and all(urlsplit(hop.to_url).hostname in ALLOWED_HOSTS for hop in result.redirects),
                     result.final_host)

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        exit_code: int = 2
        try:
            self.progress("Fetching the public vulnerability disclosure policy")
            result: PublicResourceResult = webresource.fetch_public_resource(
                TARGET_URL, self.evidence, allowed_hosts=ALLOWED_HOSTS,
                connect_timeout=CONNECT_TIMEOUT_SECONDS, read_timeout=READ_TIMEOUT_SECONDS,
                max_redirects=MAX_REDIRECTS, user_agent=USER_AGENT, session_factory=self.session_factory,
            )
            self.progress("Evaluating public accessibility")
            self._check_route(result)
            if (HttpStatus.C_500_INTERNAL_SERVER_ERROR <= result.status < 600
                    or result.status == HttpStatus.C_429_TOO_MANY_REQUESTS):
                raise InvalidSetup(f"transient public-resource response: HTTP {result.status}")
            self._assert("final status is allowed", result.status in ALLOWED_FINAL_STATUS, result.status)
            self._assert("final status is not an access denial",
                         result.status not in (HttpStatus.C_401_UNAUTHORIZED, HttpStatus.C_403_FORBIDDEN),
                         result.status)
            self._assert("redirect traversal completed", not result.stopped_at_redirect, result.stopped_at_redirect)
            self._assert("final content type begins with text/html",
                         result.content_type.lower().startswith("text/html"), result.content_type)
            self._assert("final body is non-empty", result.body_length >= MIN_BODY_BYTES, result.body_length)
            verdict = "FAIL" if self.failed else "PASS"
            exit_code = 1 if self.failed else 0
        except Exception as error:  # noqa: BLE001 - Run boundary records all unexpected failures as ERROR.
            error: Exception
            if isinstance(error, WebResourceError) and error.observation is not None:
                # An incomplete chain may contain a malformed URL; only judge known challenges.
                self._check_authentication(error.observation)
            self.evidence.exception(error)
        outcome: str = "FAIL" if self.failed else verdict
        self.evidence.write("FINAL RESULT", MechanismResultEvidence(MECHANISM, outcome))
        self.evidence.write("OVERALL RESULT", verdict)
        return RunResult(exit_code, verdict, {MECHANISM: outcome}, set())


def execute_test_5_2_1_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    root: Path = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(root / "logs", "test_5_2_1_2_a", now)
    output(f"Open log file: {log.path}")

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {MECHANISM: "ERROR"}, set())
    try:
        progress.step("Opening evidence and recording the accessibility policy")
        log.write("TEST CASE AND UNIT", TEST_ID)
        log.write("UTC START", format_utc(log.started_at))
        log.write("TARGET URL", TARGET_URL)
        log.write("ALLOWED HOSTS", sorted(ALLOWED_HOSTS))
        log.write("ALLOWED FINAL STATUS", sorted(ALLOWED_FINAL_STATUS))
        log.write("CONNECT TIMEOUT SECONDS", CONNECT_TIMEOUT_SECONDS)
        log.write("READ TIMEOUT SECONDS", READ_TIMEOUT_SECONDS)
        log.write("MAX REDIRECTS", MAX_REDIRECTS)
        log.write("MIN BODY BYTES", MIN_BODY_BYTES)
        result = FunctionalTest_5_2_1_2_a(log, session_factory, progress.step).run()
        progress.step("Recording the final verdict")
    finally:
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_2_1_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
