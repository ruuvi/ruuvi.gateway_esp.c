"""Live ETSI 5.1-5-2 Unit B brute-force functional test."""

from __future__ import annotations

import base64
import hashlib
import json
import secrets
import statistics
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, cast

import requests
from Crypto.PublicKey import ECC

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import GatewayAuthenticationModeError, GatewayConnectionError, InvalidSetup
from lib.evidence import AssertionEvidence, EvidenceLog, MechanismResultEvidence, format_utc, utc_now
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthChallenge,
    InteractiveAuthResult,
    InteractiveChallengeRequest,
    InteractiveLoginChallenge,
    InteractiveLoginRequest,
)
from lib.http_api import (
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-5-2, Test Unit B"
ADMIN_USERNAME: str = "Admin"
ATTEMPTS_PER_TARGET: int = 10
MIN_FAILED_ATTEMPT_SECONDS: float = 1.00
MAX_SUCCESS_AVERAGE_SECONDS: float = 0.250
MAX_INTERACTIVE_ATTEMPTS_PER_SECOND: float = 1.12
CONNECT_TIMEOUT_SECONDS: int = 5
READ_TIMEOUT_SECONDS: int = 15
PREPARED_STATE_READ_ATTEMPTS: int = 3
BEARER_REQUEST_ATTEMPTS: int = 3
DIGEST_REQUEST_ATTEMPTS: int = 3
HTTP_TIMEOUT: tuple[int, int] = (CONNECT_TIMEOUT_SECONDS, READ_TIMEOUT_SECONDS)
USER_AGENT: str = "ruuvi-etsi-test-5.1-5-2-b"
TOTAL_STEPS: int = 18
MECHANISMS: tuple[str, ...] = (
    AuthMech.LAN_WEBUI_DEFAULT,
    AuthMech.LAN_WEBUI_USER_DEFINED,
    AuthMech.M2M_API_BEARER_RO,
    AuthMech.M2M_API_BEARER_RW,
    AuthMech.LAN_WEBUI_BASIC,
    AuthMech.LAN_WEBUI_DIGEST,
    AuthMech.LAN_WEBUI_UNAUTHENTICATED,
    AuthMech.LAN_WEBUI_DISABLED,
    "temporary-state setup",
    "final restoration",
)
AUTH_MECHANISMS: tuple[str, ...] = MECHANISMS[:8]


class SecurityFailure(Exception):
    """A security assertion failed."""


class UnexpectedAuthorization(SecurityFailure):
    """A negative probe succeeded; stop all further probes and restore."""


@dataclass(frozen=True)
class CampaignSummaryEvidence:
    campaign: str
    attempted: int
    completed: int
    denied: int
    authorized: int
    errored: int
    elapsed_seconds: float
    attempts_per_second: float | None
    minimum_latency_seconds: float | None
    maximum_latency_seconds: float | None
    mean_latency_seconds: float | None
    median_latency_seconds: float | None
    p05_latency_seconds: float | None
    p50_latency_seconds: float | None
    p95_latency_seconds: float | None
    p99_latency_seconds: float | None


@dataclass(frozen=True)
class PreparedStateReadRetryEvidence:
    attempt: int
    maximum_attempts: int
    error: str


@dataclass(frozen=True)
class SuccessfulRequestAttemptEvidence:
    mechanism: str
    attempt: int
    monotonic_start_ns: int
    monotonic_end_ns: int
    elapsed_ns: int
    elapsed_seconds: float
    status: int


@dataclass(frozen=True)
class InteractiveAuthStageAttemptEvidence:
    mechanism: str
    successful: bool
    method: str
    path: str
    attempt: int
    monotonic_start_ns: int
    monotonic_end_ns: int
    elapsed_ns: int
    elapsed_seconds: float
    status: int


@dataclass(frozen=True)
class InteractiveAuthStageSummaryEvidence:
    mechanism: str
    successful: bool
    method: str
    path: str
    attempts: int
    evaluated_attempts: int
    ignored_slowest_response_seconds: float | None
    average_response_seconds: float
    minimum_response_seconds: float


@dataclass(frozen=True)
class SuccessfulAuthSummaryEvidence:
    mechanism: str
    attempts: int
    evaluated_attempts: int
    ignored_slowest_response_seconds: float
    average_response_seconds: float


@dataclass(frozen=True)
class AuthModeRequestRetryEvidence:
    mechanism: str
    attempt: int
    request_attempt: int
    maximum_request_attempts: int
    elapsed_ns: int | None
    error: str


@dataclass(frozen=True)
class AuthModeAttemptEvidence:
    mechanism: str
    successful: bool
    attempt: int
    status: int
    elapsed_ns: int
    elapsed_seconds: float


@dataclass(frozen=True)
class BearerRequestRetryEvidence:
    campaign: str
    attempt: int
    request_attempt: int
    maximum_request_attempts: int
    bearer_token: str
    monotonic_start_ns: int
    monotonic_end_ns: int
    elapsed_ns: int
    error: str


@dataclass(frozen=True)
class TemporaryCredentialsEvidence:
    username: str
    plaintext_password: str
    realm: str
    stored_password: str
    ro_key: str
    rw_key: str


class RestorationAttemptEvidence:
    pass


@dataclass(frozen=True)
class RestorationStatusAttemptEvidence(RestorationAttemptEvidence):
    method: str
    status: int


@dataclass(frozen=True)
class RestorationErrorAttemptEvidence(RestorationAttemptEvidence):
    method: str
    error: str
    message: str


def canonical_json_hash(value: Any) -> str:
    encoded: bytes = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def percentile(values: list[int], fraction: float) -> float:
    """Return a linearly interpolated percentile for integer nanosecond samples."""
    if not values:
        raise ValueError("percentile requires at least one value")
    if not 0.0 <= fraction <= 1.0:
        raise ValueError("percentile fraction must be between zero and one")
    ordered: list[int] = sorted(values)
    position: float = (len(ordered) - 1) * fraction
    lower: int = int(position)
    upper: int = min(lower + 1, len(ordered) - 1)
    weight: float = position - lower
    return ordered[lower] + ((ordered[upper] - ordered[lower]) * weight)


def without_single_slowest(values: list[int]) -> list[int]:
    ordered: list[int] = sorted(values)
    return ordered[:-1] if len(ordered) > 1 else ordered


@dataclass
class CampaignStats:
    name: str
    attempted: int = 0
    completed: int = 0
    denied: int = 0
    authorized: int = 0
    errored: int = 0
    durations_ns: list[int] = field(default_factory=list)
    first_start_ns: int | None = None
    last_end_ns: int | None = None

    def as_evidence(self) -> CampaignSummaryEvidence:
        elapsed_ns: int = 0
        if self.first_start_ns is not None and self.last_end_ns is not None:
            elapsed_ns = self.last_end_ns - self.first_start_ns
        elapsed_seconds: float = elapsed_ns / 1_000_000_000
        minimum_latency_seconds: float | None = None
        maximum_latency_seconds: float | None = None
        mean_latency_seconds: float | None = None
        median_latency_seconds: float | None = None
        p05_latency_seconds: float | None = None
        p50_latency_seconds: float | None = None
        p95_latency_seconds: float | None = None
        p99_latency_seconds: float | None = None
        if self.durations_ns:
            seconds: list[float] = [value / 1_000_000_000 for value in self.durations_ns]
            minimum_latency_seconds = min(seconds)
            maximum_latency_seconds = max(seconds)
            mean_latency_seconds = statistics.mean(seconds)
            median_latency_seconds = statistics.median(seconds)
            p05_latency_seconds = percentile(self.durations_ns, 0.05) / 1_000_000_000
            p50_latency_seconds = percentile(self.durations_ns, 0.50) / 1_000_000_000
            p95_latency_seconds = percentile(self.durations_ns, 0.95) / 1_000_000_000
            p99_latency_seconds = percentile(self.durations_ns, 0.99) / 1_000_000_000
        return CampaignSummaryEvidence(
            campaign=self.name,
            attempted=self.attempted,
            completed=self.completed,
            denied=self.denied,
            authorized=self.authorized,
            errored=self.errored,
            elapsed_seconds=elapsed_seconds,
            attempts_per_second=(self.completed / elapsed_seconds if elapsed_seconds > 0 else None),
            minimum_latency_seconds=minimum_latency_seconds,
            maximum_latency_seconds=maximum_latency_seconds,
            mean_latency_seconds=mean_latency_seconds,
            median_latency_seconds=median_latency_seconds,
            p05_latency_seconds=p05_latency_seconds,
            p50_latency_seconds=p50_latency_seconds,
            p95_latency_seconds=p95_latency_seconds,
            p99_latency_seconds=p99_latency_seconds,
        )


@dataclass(frozen=True)
class BearerAccessCheck:
    name: str
    key: str | None
    path: str


class AttemptCredential:
    pass


@dataclass(frozen=True)
class InteractiveAttemptCredential(AttemptCredential):
    username: str
    password: str


@dataclass(frozen=True)
class BearerAttemptCredential(AttemptCredential):
    bearer_token: str


@dataclass(frozen=True)
class CampaignRunningCountsEvidence:
    attempted: int
    completed: int
    denied: int
    authorized: int
    errored: int


@dataclass(frozen=True)
class CampaignAttemptEvidence:
    campaign: str
    attempt: int
    credential: AttemptCredential
    method: str
    path: str
    monotonic_start_ns: int
    monotonic_end_ns: int
    elapsed_ns: int
    elapsed_seconds: float
    outcome: str
    running_counts: CampaignRunningCountsEvidence


@dataclass
class InteractiveTimingStats:
    successful_get_durations_ns: list[int] = field(default_factory=list)
    successful_post_durations_ns: list[int] = field(default_factory=list)
    failed_get_durations_ns: list[int] = field(default_factory=list)
    failed_post_durations_ns: list[int] = field(default_factory=list)


class FunctionalTest_5_1_5_2_b:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        monotonic_ns: Callable[[], int] = time.monotonic_ns,
        success_monotonic_ns: Callable[[], int] | None = None,
        attempt_count: int = ATTEMPTS_PER_TARGET,
        progress: Callable[[str], None] | None = None,
        campaign_progress: Callable[[str], None] | None = None,
    ) -> None:
        if attempt_count <= 0:
            raise ValueError("attempt_count must be positive")
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config,
            evidence,
            session_factory=session_factory,
            random_bytes=random_bytes,
            ecc_generate=ecc_generate,
            timeout=HTTP_TIMEOUT,
            user_agent=USER_AGENT,
        )
        self.random_bytes: Callable[[int], bytes] = random_bytes
        self.monotonic_ns: Callable[[], int] = monotonic_ns
        self.success_monotonic_ns: Callable[[], int] = (
            success_monotonic_ns if success_monotonic_ns is not None else monotonic_ns
        )
        self.attempt_count: int = attempt_count
        self.progress: Callable[[str], None] = progress if progress is not None else lambda description: None
        self.campaign_progress: Callable[[str], None] = (
            campaign_progress if campaign_progress is not None else lambda description: None
        )
        self.outcomes: dict[str, str] = {mechanism: "NOT RUN" for mechanism in MECHANISMS}
        self.campaigns: dict[str, CampaignStats] = {}
        self.success_durations_ns: dict[str, list[int]] = {}
        self.interactive_timings: dict[str, InteractiveTimingStats] = {}
        self.baseline: dict[str, Any] | None = None
        self.baseline_hash: str | None = None
        self.temporary_username: str | None = None
        self.temporary_password: str | None = None
        self.temporary_ro_key: str | None = None
        self.temporary_rw_key: str | None = None
        self.mutation_possible: bool = False
        self.factory_reset_required: bool = False

    def _set_outcome(self, mechanism: str, outcome: str) -> None:
        priority: dict[str, int] = {"NOT RUN": 0, "PASS": 1, "FAIL": 2, "ERROR": 3}
        if priority[outcome] >= priority[self.outcomes[mechanism]]:
            self.outcomes[mechanism] = outcome

    def _record_assertion(self, description: str, passed: bool, actual: Any = "") -> None:
        self.evidence.write(
            "ASSERTION",
            AssertionEvidence(
                description=description,
                result="PASS" if passed else "FAIL",
                actual=actual,
            ),
        )

    def _require_setup(self, condition: bool, description: str, actual: Any = "") -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            raise InvalidSetup(f"{description} (actual: {actual!r})")

    def _require_security(self, condition: bool, description: str, actual: Any = "") -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            raise SecurityFailure(f"{description} (actual: {actual!r})")

    @staticmethod
    def _normalize_mac(value: str) -> str:
        return value.replace(":", "").upper()

    def _random_secret(self, size: int = 32) -> str:
        return base64.b64encode(self.random_bytes(size)).decode("ascii")

    def _random_distinct_secret(self, *excluded: str, size: int = 32) -> str:
        _: int
        for _ in range(100):
            candidate: str = self._random_secret(size)
            if candidate not in excluded:
                return candidate
        raise InvalidSetup("random generator did not produce a distinct secret")

    def _random_default_password(self) -> str:
        _: int
        for _ in range(100):
            candidate: str = self.random_bytes(8).hex(":").upper()
            if candidate.upper() != self.config.gw_id.upper():
                return candidate
        raise InvalidSetup("random generator did not produce a non-default 64-bit password")

    def _distinct_username(self, *excluded: str) -> str:
        _: int
        for _ in range(100):
            candidate: str = f"test-{self.gateway.random_text(12)}"
            if candidate not in excluded:
                return candidate
        raise InvalidSetup("random generator did not produce a distinct username")

    def _validate_challenge(
        self,
        result: InteractiveAuthChallenge | InteractiveAuthResult,
        expected_mode: str | None,
        context: str,
    ) -> None:
        self._require_setup(
            result.challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            f"{context} challenge returns 401",
            result.challenge_response.status_code,
        )
        self._require_setup(
            result.auth_header.lower().startswith("x-ruuvi-interactive"),
            f"{context} advertises x-ruuvi-interactive",
            result.auth_header,
        )
        if expected_mode is not None:
            self._require_setup(
                result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) == expected_mode,
                f"{context} challenge reports {expected_mode}",
                result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE),
            )

    def _successful_login(
        self,
        username: str,
        password: str,
        expected_mode: str,
        setup_check: bool = True,
        *,
        check_mode: bool = True,
    ) -> InteractiveAuthResult:
        try:
            result: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
        except GatewayAuthenticationModeError:
            if expected_mode == GatewayCfgLanAuthType.DEFAULT:
                self.factory_reset_required = True
            raise
        if expected_mode == GatewayCfgLanAuthType.DEFAULT and (
            result.login_response.status_code != HttpStatus.C_200_OK
        ):
            self.factory_reset_required = True
        self._validate_challenge(result, expected_mode if check_mode else None, "successful login control")
        if setup_check:
            self._require_setup(
                result.login_response.status_code == HttpStatus.C_200_OK,
                "configured interactive credential remains operational",
                result.login_response.status_code,
            )
        else:
            self._require_security(
                result.login_response.status_code == HttpStatus.C_200_OK,
                "configured interactive credential remains operational",
                result.login_response.status_code,
            )
        return result

    def _read_config(self, session: requests.Session, context: str) -> dict[str, Any]:
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._require_setup(
            response.status_code == HttpStatus.C_200_OK,
            f"{context} succeeds",
            response.status_code,
        )
        return self.gateway.response_json(response, context, dict)

    def _read_prepared_config(self, session: requests.Session) -> dict[str, Any]:
        attempt: int
        for attempt in range(1, PREPARED_STATE_READ_ATTEMPTS + 1):
            try:
                return self._read_config(session, "prepared-state GET /ruuvi.json")
            except GatewayConnectionError as error:
                error: Exception
                self.evidence.write(
                    "PREPARED STATE READ RETRY",
                    PreparedStateReadRetryEvidence(
                        attempt=attempt,
                        maximum_attempts=PREPARED_STATE_READ_ATTEMPTS,
                        error=str(error),
                    ),
                )
                if attempt == PREPARED_STATE_READ_ATTEMPTS:
                    raise
        raise AssertionError("unreachable")

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any] | None = None) -> None:
        if GatewayCfgDesc.GW_MAC in payload:
            actual_mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require_setup(isinstance(actual_mac, str), "gw_mac is a string", actual_mac)
            self._require_setup(
                OCTETS_6_RE.fullmatch(actual_mac) is not None,
                "gw_mac is a six-octet MAC",
                actual_mac,
            )
            self._require_setup(
                self._normalize_mac(actual_mac) == self._normalize_mac(self.config.gw_mac),
                "DUT gw_mac matches .env",
                actual_mac,
            )
        if auth_payload is not None:
            auth_type: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
            if auth_type != GatewayCfgLanAuthType.DEFAULT:
                self.factory_reset_required = GatewayCfgDesc.GW_MAC in payload
            self._require_setup(
                auth_type == GatewayCfgLanAuthType.DEFAULT,
                "baseline GET /auth reports factory-default mode",
                auth_type,
            )
        expected: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        value: Any
        field_name: str
        for field_name, value in expected.items():
            actual: Any = payload.get(field_name)
            is_default: bool = type(actual) is type(value) and actual == value
            if not is_default:
                self.factory_reset_required = GatewayCfgDesc.GW_MAC in payload
            self._require_setup(
                is_default,
                f"{field_name} has its factory-default value",
                actual,
            )
        identity: dict[str, Any] = {
            key: payload[key]
            for key in (
                GatewayCfgDesc.GW_MAC,
                GatewayCfgDesc.FW_VER,
                GatewayCfgDesc.NRF52_FW_VER,
            )
            if key in payload
        }
        self.evidence.write("DUT VERSION AND IDENTITY", identity)

    def _log_attempt(
        self,
        stats: CampaignStats,
        attempt: int,
        start_ns: int,
        end_ns: int,
        outcome: str,
        credential: AttemptCredential,
    ) -> None:
        self.evidence.write(
            "CAMPAIGN ATTEMPT",
            CampaignAttemptEvidence(
                campaign=stats.name,
                attempt=attempt,
                credential=credential,
                method=HttpMethod.POST,
                path=GatewayApi.AUTH,
                monotonic_start_ns=start_ns,
                monotonic_end_ns=end_ns,
                elapsed_ns=end_ns - start_ns,
                elapsed_seconds=(end_ns - start_ns) / 1_000_000_000,
                outcome=outcome,
                running_counts=CampaignRunningCountsEvidence(
                    attempted=stats.attempted,
                    completed=stats.completed,
                    denied=stats.denied,
                    authorized=stats.authorized,
                    errored=stats.errored,
                ),
            ),
        )
        self.campaign_progress(
            f"{stats.name}: attempt {attempt} out of {self.attempt_count} completed in "
            f"{(end_ns - start_ns) / 1_000_000_000:.9f} seconds ({outcome})"
        )

    def _measure_success(
        self,
        mechanism: str,
        session: requests.Session,
        headers: dict[str, str] | None = None,
    ) -> None:
        durations_ns: list[int] = []
        self.success_durations_ns[mechanism] = durations_ns
        try:
            attempt: int
            for attempt in range(1, self.attempt_count + 1):
                start_ns: int = self.success_monotonic_ns()
                response: requests.Response = self.gateway.request(
                    session,
                    HttpMethod.GET,
                    GatewayApi.STATUS,
                    headers=headers,
                )
                end_ns: int = self.success_monotonic_ns()
                duration_ns: int = end_ns - start_ns
                durations_ns.append(duration_ns)
                self.evidence.write(
                    "SUCCESSFUL AUTH ATTEMPT",
                    SuccessfulRequestAttemptEvidence(
                        mechanism=mechanism,
                        attempt=attempt,
                        monotonic_start_ns=start_ns,
                        monotonic_end_ns=end_ns,
                        elapsed_ns=duration_ns,
                        elapsed_seconds=duration_ns / 1_000_000_000,
                        status=response.status_code,
                    ),
                )
                self.campaign_progress(
                    f"{mechanism}: successful attempt {attempt} out of "
                    f"{self.attempt_count} completed in "
                    f"{duration_ns / 1_000_000_000:.9f} seconds"
                )
                self._require_setup(
                    response.status_code == HttpStatus.C_200_OK,
                    f"{mechanism} successful probe returns 200",
                    response.status_code,
                )
            evaluated_durations_ns: list[int] = without_single_slowest(durations_ns)
            average_seconds: float = statistics.mean(evaluated_durations_ns) / 1_000_000_000
            self.evidence.write(
                "SUCCESSFUL AUTH SUMMARY",
                SuccessfulAuthSummaryEvidence(
                    mechanism=mechanism,
                    attempts=len(durations_ns),
                    evaluated_attempts=len(evaluated_durations_ns),
                    ignored_slowest_response_seconds=max(durations_ns) / 1_000_000_000,
                    average_response_seconds=average_seconds,
                ),
            )
            self._require_security(
                average_seconds < MAX_SUCCESS_AVERAGE_SECONDS,
                f"{mechanism} successful average response time is below 250 ms after ignoring one spike",
                average_seconds,
            )
        except SecurityFailure:
            self._set_outcome(mechanism, "FAIL")
            raise
        except Exception:
            self._set_outcome(mechanism, "ERROR")
            raise

    def _measure_interactive_login_success(
        self,
        mechanism: str,
        username: str,
        password: str,
        expected_mode: str,
    ) -> None:
        timing: InteractiveTimingStats = self.interactive_timings.setdefault(mechanism, InteractiveTimingStats())
        get_durations_ns: list[int] = timing.successful_get_durations_ns
        post_durations_ns: list[int] = timing.successful_post_durations_ns
        self.success_durations_ns[mechanism] = post_durations_ns
        try:
            attempt: int
            for attempt in range(1, self.attempt_count + 1):
                challenge_request: InteractiveChallengeRequest = self.gateway.prepare_interactive_challenge_request()
                get_start_ns: int = self.monotonic_ns()
                challenge_response: requests.Response = self.gateway.send_interactive_challenge_request(
                    challenge_request
                )
                get_end_ns: int = self.monotonic_ns()
                challenge: InteractiveAuthChallenge = self.gateway.parse_interactive_challenge_response(
                    challenge_request,
                    challenge_response,
                )
                get_duration_ns: int = get_end_ns - get_start_ns
                get_durations_ns.append(get_duration_ns)
                self._validate_challenge(challenge, expected_mode, mechanism)
                self.evidence.write(
                    "INTERACTIVE AUTH STAGE ATTEMPT",
                    InteractiveAuthStageAttemptEvidence(
                        mechanism=mechanism,
                        successful=True,
                        method=HttpMethod.GET,
                        path=GatewayApi.AUTH,
                        attempt=attempt,
                        monotonic_start_ns=get_start_ns,
                        monotonic_end_ns=get_end_ns,
                        elapsed_ns=get_duration_ns,
                        elapsed_seconds=get_duration_ns / 1_000_000_000,
                        status=challenge.challenge_response.status_code,
                    ),
                )

                login_request: InteractiveLoginRequest = self.gateway.prepare_interactive_login_request(
                    challenge,
                    username,
                    password,
                )
                post_start_ns: int = self.success_monotonic_ns()
                login_response: requests.Response = self.gateway.send_interactive_login_request(login_request)
                post_end_ns: int = self.success_monotonic_ns()
                post_duration_ns: int = post_end_ns - post_start_ns
                post_durations_ns.append(post_duration_ns)
                self._require_setup(
                    login_response.status_code == HttpStatus.C_200_OK,
                    f"{mechanism} successful login returns 200",
                    login_response.status_code,
                )
                self.evidence.write(
                    "INTERACTIVE AUTH STAGE ATTEMPT",
                    InteractiveAuthStageAttemptEvidence(
                        mechanism=mechanism,
                        successful=True,
                        method=HttpMethod.POST,
                        path=GatewayApi.AUTH,
                        attempt=attempt,
                        monotonic_start_ns=post_start_ns,
                        monotonic_end_ns=post_end_ns,
                        elapsed_ns=post_duration_ns,
                        elapsed_seconds=post_duration_ns / 1_000_000_000,
                        status=login_response.status_code,
                    ),
                )
                self.campaign_progress(
                    f"{mechanism}: successful attempt {attempt} out of {self.attempt_count}: "
                    f"GET /auth completed in {get_duration_ns / 1_000_000_000:.9f} seconds, "
                    f"POST /auth completed in {post_duration_ns / 1_000_000_000:.9f} seconds"
                )
            minimum_get_seconds: float = min(get_durations_ns) / 1_000_000_000
            self._require_security(
                min(get_durations_ns) > int(MIN_FAILED_ATTEMPT_SECONDS * 1_000_000_000),
                f"{mechanism} successful GET /auth responses all exceed one second",
                minimum_get_seconds,
            )
            evaluated_get_durations_ns: list[int] = without_single_slowest(get_durations_ns)
            evaluated_post_durations_ns: list[int] = without_single_slowest(post_durations_ns)
            get_average_seconds: float = statistics.mean(evaluated_get_durations_ns) / 1_000_000_000
            post_average_seconds: float = statistics.mean(evaluated_post_durations_ns) / 1_000_000_000
            durations: list[int]
            evaluated: list[int]
            average_seconds: float
            method: str
            for method, durations, evaluated, average_seconds in (
                (
                    HttpMethod.GET,
                    get_durations_ns,
                    evaluated_get_durations_ns,
                    get_average_seconds,
                ),
                (
                    HttpMethod.POST,
                    post_durations_ns,
                    evaluated_post_durations_ns,
                    post_average_seconds,
                ),
            ):
                self.evidence.write(
                    "INTERACTIVE AUTH STAGE SUMMARY",
                    InteractiveAuthStageSummaryEvidence(
                        mechanism=mechanism,
                        successful=True,
                        method=method,
                        path=GatewayApi.AUTH,
                        attempts=len(durations),
                        evaluated_attempts=len(evaluated),
                        ignored_slowest_response_seconds=max(durations) / 1_000_000_000,
                        average_response_seconds=average_seconds,
                        minimum_response_seconds=min(durations) / 1_000_000_000,
                    ),
                )
            self._require_security(
                post_average_seconds < MAX_SUCCESS_AVERAGE_SECONDS,
                f"{mechanism} successful POST /auth average is below 250 ms after ignoring one spike",
                post_average_seconds,
            )
        except SecurityFailure:
            self._set_outcome(mechanism, "FAIL")
            raise
        except Exception:
            self._set_outcome(mechanism, "ERROR")
            raise

    def _configure_auth_mode(
        self,
        auth_type: str,
        username: str = "",
        stored_password: str = "",
    ) -> None:
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: auth_type,
            GatewayCfgDesc.LAN_AUTH_API_KEY: self.temporary_ro_key or "",
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: self.temporary_rw_key or "",
        }
        if auth_type in {
            GatewayCfgLanAuthType.RUUVI,
            GatewayCfgLanAuthType.BASIC,
            GatewayCfgLanAuthType.DIGEST,
        }:
            body[GatewayCfgDesc.LAN_AUTH_USER] = username
            body[GatewayCfgDesc.LAN_AUTH_PASS] = stored_password
        self.evidence.write("CONFIGURATION TRANSITION", body)
        mechanism_by_mode: dict[str, str] = {
            GatewayCfgLanAuthType.BASIC: AuthMech.LAN_WEBUI_BASIC,
            GatewayCfgLanAuthType.DIGEST: AuthMech.LAN_WEBUI_DIGEST,
            GatewayCfgLanAuthType.ALLOW: AuthMech.LAN_WEBUI_UNAUTHENTICATED,
            GatewayCfgLanAuthType.DENY: AuthMech.LAN_WEBUI_DISABLED,
            GatewayCfgLanAuthType.RUUVI: AuthMech.LAN_WEBUI_USER_DEFINED,
        }
        try:
            response: requests.Response = self._post_config(
                self.gateway.new_session(),
                body,
                self.temporary_rw_key,
            )
            self._require_setup(
                response.status_code == HttpStatus.C_200_OK,
                f"transition to {auth_type} succeeds",
                response.status_code,
            )
        except Exception:
            self._set_outcome(mechanism_by_mode[auth_type], "ERROR")
            raise

    def _prepare_digest_request(
        self,
        username: str,
        password: str,
        path: str = GatewayApi.STATUS,
    ) -> Callable[[], requests.Response]:
        session: requests.Session = self.gateway.new_session()
        challenge_response: requests.Response = self.gateway.request(session, HttpMethod.GET, path)
        self._require_setup(
            challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            "Digest challenge returns 401",
            challenge_response.status_code,
        )
        auth_header: str = challenge_response.headers.get(HttpHeader.WWW_AUTHENTICATE, "")
        self._require_setup(
            auth_header.lower().startswith("digest "),
            "Digest challenge advertises Digest",
            auth_header,
        )
        parameters: dict[str, str] = self.gateway.parse_digest_challenge(auth_header)
        required: set[str] = {"realm", "qop", "nonce", "opaque"}
        self._require_setup(
            required.issubset(parameters),
            "Digest challenge contains required parameters",
            parameters,
        )
        authorization: str = self.gateway.authorization_header_digest(
            username,
            password,
            HttpMethod.GET,
            path,
            parameters,
        )
        return lambda: self.gateway.request(
            session,
            HttpMethod.GET,
            path,
            headers={HttpHeader.AUTHORIZATION: authorization},
        )

    def _measure_http_auth_campaign(
        self,
        mechanism: str,
        successful: bool,
        expected_status: int,
        operation: Callable[[], requests.Response] | Callable[[], Callable[[], requests.Response]],
        finalize_mechanism: bool,
        enforce_timing: bool = True,
        prepare_before_timing: bool = False,
        request_attempts: int = 1,
    ) -> None:
        durations_ns: list[int] = []
        stats: CampaignStats = CampaignStats(mechanism)
        if successful:
            self.success_durations_ns[mechanism] = durations_ns
            clock: Callable[[], int] = self.success_monotonic_ns
        else:
            self.campaigns[mechanism] = stats
            clock = self.monotonic_ns
        try:
            attempt: int
            for attempt in range(1, self.attempt_count + 1):
                response: requests.Response | None = None
                successful_start_ns: int | None = None
                successful_end_ns: int | None = None
                request_attempt: int
                for request_attempt in range(1, request_attempts + 1):
                    request_start_ns: int | None = None
                    try:
                        # The flag selects a request factory (Digest challenge first)
                        # or a directly timed HTTP operation; neither returns arbitrary data.
                        timed_operation: Callable[[], requests.Response] = (
                            cast(Callable[[], Callable[[], requests.Response]], operation)()
                            if prepare_before_timing
                            else cast(Callable[[], requests.Response], operation)
                        )
                        request_start_ns = clock()
                        response = timed_operation()
                        successful_start_ns = request_start_ns
                        successful_end_ns = clock()
                        break
                    except GatewayConnectionError as error:
                        error: Exception
                        request_end_ns: int | None = clock() if request_start_ns is not None else None
                        self.evidence.write(
                            "AUTH MODE REQUEST RETRY",
                            AuthModeRequestRetryEvidence(
                                mechanism=mechanism,
                                attempt=attempt,
                                request_attempt=request_attempt,
                                maximum_request_attempts=request_attempts,
                                elapsed_ns=(
                                    request_end_ns - request_start_ns
                                    if request_end_ns is not None and request_start_ns is not None
                                    else None
                                ),
                                error=str(error),
                            ),
                        )
                        if request_attempt == request_attempts:
                            raise
                if successful_start_ns is None or successful_end_ns is None or response is None:
                    raise AssertionError("authentication request retry loop returned no response")
                duration_ns: int = successful_end_ns - successful_start_ns
                durations_ns.append(duration_ns)
                stats.attempted += 1
                stats.completed += 1
                if HttpStatus.C_200_OK <= response.status_code < HttpStatus.C_300_MULTIPLE_CHOICES:
                    stats.authorized += 1
                elif response.status_code == expected_status:
                    stats.denied += 1
                else:
                    stats.errored += 1
                self.evidence.write(
                    "AUTH MODE ATTEMPT",
                    AuthModeAttemptEvidence(
                        mechanism=mechanism,
                        successful=successful,
                        attempt=attempt,
                        status=response.status_code,
                        elapsed_ns=duration_ns,
                        elapsed_seconds=duration_ns / 1_000_000_000,
                    ),
                )
                self.campaign_progress(
                    f"{mechanism}: {'successful' if successful else 'failed'} attempt "
                    f"{attempt} out of {self.attempt_count} completed in "
                    f"{duration_ns / 1_000_000_000:.9f} seconds"
                )
                if not successful and HttpStatus.C_200_OK <= response.status_code < HttpStatus.C_300_MULTIPLE_CHOICES:
                    raise UnexpectedAuthorization(f"{mechanism} accepted a negative probe")
                self._require_security(
                    response.status_code == expected_status,
                    f"{mechanism} {'successful' if successful else 'failed'} attempt returns {expected_status}",
                    response.status_code,
                )
            if successful and enforce_timing:
                evaluated_durations_ns: list[int] = without_single_slowest(durations_ns)
                average_seconds: float = statistics.mean(evaluated_durations_ns) / 1_000_000_000
                self._require_security(
                    average_seconds < MAX_SUCCESS_AVERAGE_SECONDS,
                    f"{mechanism} successful average response time is below 250 ms after ignoring one spike",
                    average_seconds,
                )
            elif not successful and enforce_timing:
                stats.durations_ns = durations_ns
                minimum_ns: int = min(durations_ns)
                self._require_security(
                    minimum_ns > int(MIN_FAILED_ATTEMPT_SECONDS * 1_000_000_000),
                    f"{mechanism} failed attempts all exceed one second",
                    minimum_ns / 1_000_000_000,
                )
            if finalize_mechanism:
                self._set_outcome(mechanism, "PASS")
        except SecurityFailure:
            self._set_outcome(mechanism, "FAIL")
            raise
        except Exception:
            self._set_outcome(mechanism, "ERROR")
            raise

    def _test_additional_auth_modes(self, realm: str) -> None:
        username: str = self.temporary_username or ""
        password: str = self.temporary_password or ""
        failures: list[SecurityFailure] = []

        def observe(operation: Callable[[], None]) -> None:
            try:
                operation()
            except SecurityFailure as error:
                error: SecurityFailure
                if isinstance(error, UnexpectedAuthorization):
                    raise
                failures.append(error)
                self.evidence.exception(error)

        basic_password: str = self._random_secret(16)
        basic_authorization: str = self.gateway.authorization_header_basic(username, basic_password)
        basic_token: str = basic_authorization[len(f"{HttpAuthScheme.BASIC} ") :]
        self.progress("Testing Basic authentication")
        self._configure_auth_mode(GatewayCfgLanAuthType.BASIC, username, basic_token)
        basic_session: requests.Session = self.gateway.new_session()
        basic_headers: dict[str, str] = {HttpHeader.AUTHORIZATION: basic_authorization}
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_BASIC,
                True,
                HttpStatus.C_200_OK,
                lambda: self.gateway.request(
                    basic_session,
                    HttpMethod.GET,
                    GatewayApi.STATUS,
                    headers=basic_headers,
                ),
                False,
            ),
        )
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_BASIC,
                False,
                HttpStatus.C_401_UNAUTHORIZED,
                lambda: self.gateway.request(
                    self.gateway.new_session(),
                    HttpMethod.GET,
                    GatewayApi.STATUS,
                    headers={
                        HttpHeader.AUTHORIZATION: self.gateway.authorization_header_basic(
                            self._distinct_username(username),
                            self._random_secret(),
                        )
                    },
                ),
                True,
            ),
        )

        digest_ha1: str = self.gateway.calculate_digest_ha1(username, realm, password)
        self.progress("Testing Digest authentication")
        self._configure_auth_mode(GatewayCfgLanAuthType.DIGEST, username, digest_ha1)
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_DIGEST,
                True,
                HttpStatus.C_200_OK,
                lambda: self._prepare_digest_request(username, password),
                False,
                prepare_before_timing=True,
                request_attempts=DIGEST_REQUEST_ATTEMPTS,
            )
        )
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_DIGEST,
                False,
                HttpStatus.C_401_UNAUTHORIZED,
                lambda: self._prepare_digest_request(
                    self._distinct_username(username),
                    self._random_secret(),
                ),
                True,
                prepare_before_timing=True,
                request_attempts=DIGEST_REQUEST_ATTEMPTS,
            ),
        )

        self.progress("Testing unauthenticated access mode")
        self._configure_auth_mode(GatewayCfgLanAuthType.ALLOW)
        allow_session: requests.Session = self.gateway.new_session()
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_UNAUTHENTICATED,
                True,
                HttpStatus.C_200_OK,
                lambda: self.gateway.request(
                    allow_session,
                    HttpMethod.GET,
                    GatewayApi.STATUS,
                ),
                True,
                enforce_timing=False,
            )
        )

        self.progress("Testing disabled access mode")
        self._configure_auth_mode(GatewayCfgLanAuthType.DENY)
        observe(
            lambda: self._measure_http_auth_campaign(
                AuthMech.LAN_WEBUI_DISABLED,
                False,
                HttpStatus.C_403_FORBIDDEN,
                lambda: self.gateway.request(
                    self.gateway.new_session(),
                    HttpMethod.GET,
                    GatewayApi.STATUS,
                ),
                True,
            ),
        )
        self._configure_auth_mode(GatewayCfgLanAuthType.RUUVI, username, digest_ha1)
        if failures:
            raise failures[0]

    def _attack_interactive(self, mechanism: str, expected_mode: str) -> None:
        stats: CampaignStats = CampaignStats(mechanism)
        self.campaigns[mechanism] = stats
        timing: InteractiveTimingStats = self.interactive_timings.setdefault(mechanism, InteractiveTimingStats())
        failed_get_durations_ns: list[int] = timing.failed_get_durations_ns
        failed_post_durations_ns: list[int] = timing.failed_post_durations_ns
        try:
            challenge_request: InteractiveChallengeRequest = self.gateway.prepare_interactive_challenge_request()
            get_start_ns: int = self.monotonic_ns()
            challenge_response: requests.Response = self.gateway.send_interactive_challenge_request(challenge_request)
            get_end_ns: int = self.monotonic_ns()
            challenge: InteractiveAuthChallenge = self.gateway.parse_interactive_challenge_response(
                challenge_request,
                challenge_response,
            )
            get_duration_ns: int = get_end_ns - get_start_ns
            failed_get_durations_ns.append(get_duration_ns)
            self._validate_challenge(challenge, expected_mode, mechanism)
            self.evidence.write(
                "INTERACTIVE AUTH STAGE ATTEMPT",
                InteractiveAuthStageAttemptEvidence(
                    mechanism=mechanism,
                    successful=False,
                    method=HttpMethod.GET,
                    path=GatewayApi.AUTH,
                    attempt=1,
                    monotonic_start_ns=get_start_ns,
                    monotonic_end_ns=get_end_ns,
                    elapsed_ns=get_duration_ns,
                    elapsed_seconds=get_duration_ns / 1_000_000_000,
                    status=challenge.challenge_response.status_code,
                ),
            )
            self.campaign_progress(
                f"{mechanism}: failed campaign GET /auth completed in {get_duration_ns / 1_000_000_000:.9f} seconds"
            )
            self._require_security(
                get_duration_ns > int(MIN_FAILED_ATTEMPT_SECONDS * 1_000_000_000),
                f"{mechanism} failed-campaign GET /auth exceeds one second",
                get_duration_ns / 1_000_000_000,
            )
            login_challenge: InteractiveLoginChallenge = challenge

            attempt: int
            for attempt in range(1, self.attempt_count + 1):
                if mechanism == AuthMech.LAN_WEBUI_DEFAULT:
                    username: str = ADMIN_USERNAME
                    password: str = self._random_default_password()
                else:
                    username = self.temporary_username or ""
                    password = self._random_distinct_secret(self.temporary_password or "")
                credential: InteractiveAttemptCredential = InteractiveAttemptCredential(username, password)
                login_request: InteractiveLoginRequest = self.gateway.prepare_interactive_login_request(
                    login_challenge,
                    username,
                    password,
                )
                stats.attempted += 1
                start_ns: int = self.monotonic_ns()
                if stats.first_start_ns is None:
                    stats.first_start_ns = start_ns
                authorized: bool = False
                end_ns: int | None = None
                try:
                    login_response: requests.Response = self.gateway.send_interactive_login_request(login_request)
                    end_ns = self.monotonic_ns()
                    status: int = login_response.status_code
                    if HttpStatus.C_200_OK <= status < HttpStatus.C_300_MULTIPLE_CHOICES:
                        authorized = True
                        stats.authorized += 1
                        raise UnexpectedAuthorization(f"{mechanism} accepted guessed credentials on attempt {attempt}")
                    self._require_security(
                        status == HttpStatus.C_401_UNAUTHORIZED,
                        f"{mechanism} login attempt is denied with 401",
                        status,
                    )
                    auth_payload: dict[str, Any] = self.gateway.response_json(
                        login_response,
                        "failed POST /auth",
                        dict,
                    )
                    self._require_security(
                        auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) == expected_mode,
                        f"{mechanism} failed POST /auth reports {expected_mode}",
                        auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE),
                    )
                    login_challenge = self.gateway.interactive_login_challenge_from_response(
                        login_challenge.session,
                        login_response,
                        "failed POST /auth",
                    )
                    stats.denied += 1
                    stats.completed += 1
                except Exception:
                    if not authorized:
                        stats.errored += 1
                    raise
                finally:
                    attempt_end_ns: int = self.monotonic_ns() if end_ns is None else end_ns
                    stats.last_end_ns = attempt_end_ns
                    duration_ns: int = attempt_end_ns - start_ns
                    stats.durations_ns.append(duration_ns)
                    failed_post_durations_ns.append(duration_ns)
                    outcome: str = (
                        "AUTHORIZED" if stats.authorized else ("DENIED" if stats.completed == attempt else "ERROR")
                    )
                    self._log_attempt(stats, attempt, start_ns, attempt_end_ns, outcome, credential)

            evidence: CampaignSummaryEvidence = stats.as_evidence()
            self._require_security(
                stats.completed == self.attempt_count and stats.denied == self.attempt_count,
                f"{mechanism} completed and denied every attempt",
                evidence,
            )
            minimum_ns: int = int(MIN_FAILED_ATTEMPT_SECONDS * 1_000_000_000)
            self._require_security(
                min(stats.durations_ns) > minimum_ns,
                f"{mechanism} failed POST /auth attempts all exceed one second",
                evidence.minimum_latency_seconds,
            )
            aggregate_durations_ns: list[int] = without_single_slowest(stats.durations_ns)
            aggregate_seconds: float = sum(aggregate_durations_ns) / 1_000_000_000
            aggregate_attempts_per_second: float | None = (
                len(aggregate_durations_ns) / aggregate_seconds if aggregate_seconds > 0 else None
            )
            self._require_security(
                aggregate_seconds > len(aggregate_durations_ns) * MIN_FAILED_ATTEMPT_SECONDS,
                f"{mechanism} POST /auth aggregate elapsed time meets the threshold after ignoring one spike",
                aggregate_seconds,
            )
            self._require_security(
                aggregate_attempts_per_second is not None
                and aggregate_attempts_per_second <= MAX_INTERACTIVE_ATTEMPTS_PER_SECOND,
                f"{mechanism} throughput is throttled after ignoring one spike",
                aggregate_attempts_per_second,
            )
            self.evidence.write(
                "INTERACTIVE AUTH STAGE SUMMARY",
                InteractiveAuthStageSummaryEvidence(
                    mechanism=mechanism,
                    successful=False,
                    method=HttpMethod.GET,
                    path=GatewayApi.AUTH,
                    attempts=1,
                    evaluated_attempts=1,
                    ignored_slowest_response_seconds=None,
                    average_response_seconds=get_duration_ns / 1_000_000_000,
                    minimum_response_seconds=get_duration_ns / 1_000_000_000,
                ),
            )
            evaluated_post_durations_ns: list[int] = without_single_slowest(failed_post_durations_ns)
            self.evidence.write(
                "INTERACTIVE AUTH STAGE SUMMARY",
                InteractiveAuthStageSummaryEvidence(
                    mechanism=mechanism,
                    successful=False,
                    method=HttpMethod.POST,
                    path=GatewayApi.AUTH,
                    attempts=len(failed_post_durations_ns),
                    evaluated_attempts=len(evaluated_post_durations_ns),
                    ignored_slowest_response_seconds=(max(failed_post_durations_ns) / 1_000_000_000),
                    average_response_seconds=(statistics.mean(evaluated_post_durations_ns) / 1_000_000_000),
                    minimum_response_seconds=min(failed_post_durations_ns) / 1_000_000_000,
                ),
            )
            self._set_outcome(mechanism, "PASS")
        except SecurityFailure:
            self._set_outcome(mechanism, "FAIL")
            raise
        except Exception:
            self._set_outcome(mechanism, "ERROR")
            raise
        finally:
            self.evidence.write("CAMPAIGN SUMMARY", stats.as_evidence())
            self.evidence.write(
                "PER-MECHANISM RESULT",
                MechanismResultEvidence(
                    mechanism=mechanism,
                    result=self.outcomes[mechanism],
                ),
            )

    def _attack_bearer(self, mechanism: str, path: str, configured_keys: set[str]) -> None:
        stats: CampaignStats = CampaignStats(mechanism)
        self.campaigns[mechanism] = stats
        method: str = HttpMethod.POST if path == GatewayApi.CONFIG else HttpMethod.GET
        try:
            attempt: int
            for attempt in range(1, self.attempt_count + 1):
                session: requests.Session = self.gateway.new_session()
                token: str = self._random_secret()
                self._require_setup(
                    token not in configured_keys,
                    "bearer guess differs from configured temporary keys",
                )
                stats.attempted += 1
                start_ns: int = self.monotonic_ns()
                if stats.first_start_ns is None:
                    stats.first_start_ns = start_ns
                response: requests.Response | None = None
                response_start_ns: int = start_ns
                response_end_ns: int | None = None
                try:
                    request_attempt: int
                    for request_attempt in range(1, BEARER_REQUEST_ATTEMPTS + 1):
                        request_start_ns: int = self.monotonic_ns()
                        try:
                            response = self.gateway.request(
                                session,
                                method,
                                path,
                                headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"},
                                json_body={} if method == HttpMethod.POST else None,
                            )
                            response_start_ns = request_start_ns
                            response_end_ns = self.monotonic_ns()
                            break
                        except GatewayConnectionError as error:
                            error: Exception
                            request_end_ns: int = self.monotonic_ns()
                            self.evidence.write(
                                "BEARER REQUEST RETRY",
                                BearerRequestRetryEvidence(
                                    campaign=mechanism,
                                    attempt=attempt,
                                    request_attempt=request_attempt,
                                    maximum_request_attempts=BEARER_REQUEST_ATTEMPTS,
                                    bearer_token=token,
                                    monotonic_start_ns=request_start_ns,
                                    monotonic_end_ns=request_end_ns,
                                    elapsed_ns=request_end_ns - request_start_ns,
                                    error=str(error),
                                ),
                            )
                            if request_attempt == BEARER_REQUEST_ATTEMPTS:
                                raise
                            session = self.gateway.new_session()
                    if response is None:
                        raise AssertionError("bearer request retry loop returned no response")
                    if HttpStatus.C_200_OK <= response.status_code < HttpStatus.C_300_MULTIPLE_CHOICES:
                        stats.authorized += 1
                        raise UnexpectedAuthorization(f"{mechanism} accepted guessed bearer on attempt {attempt}")
                    self._require_security(
                        response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                        f"{mechanism} guess is denied with 401",
                        response.status_code,
                    )
                    stats.denied += 1
                    stats.completed += 1
                except Exception:
                    if response is None or not (
                        HttpStatus.C_200_OK <= response.status_code < HttpStatus.C_300_MULTIPLE_CHOICES
                    ):
                        stats.errored += 1
                    raise
                finally:
                    end_ns: int = response_end_ns if response_end_ns is not None else self.monotonic_ns()
                    stats.last_end_ns = end_ns
                    stats.durations_ns.append(end_ns - response_start_ns)
                    outcome: str = (
                        "AUTHORIZED" if stats.authorized else ("DENIED" if stats.completed == attempt else "ERROR")
                    )
                    self._log_attempt(
                        stats,
                        attempt,
                        response_start_ns,
                        end_ns,
                        outcome,
                        BearerAttemptCredential(token),
                    )
            self._require_security(
                stats.completed == self.attempt_count and stats.denied == self.attempt_count and stats.authorized == 0,
                f"{mechanism} rejected every independent 256-bit guess",
                stats.as_evidence(),
            )
            self._require_security(
                min(stats.durations_ns) > int(MIN_FAILED_ATTEMPT_SECONDS * 1_000_000_000),
                f"{mechanism} failed attempts all exceed one second",
                stats.as_evidence().minimum_latency_seconds,
            )
            self._set_outcome(mechanism, "PASS")
        except SecurityFailure:
            self._set_outcome(mechanism, "FAIL")
            raise
        except Exception:
            self._set_outcome(mechanism, "ERROR")
            raise
        finally:
            self.evidence.write("CAMPAIGN SUMMARY", stats.as_evidence())
            self.evidence.write(
                "PER-MECHANISM RESULT",
                MechanismResultEvidence(
                    mechanism=mechanism,
                    result=self.outcomes[mechanism],
                ),
            )

    def _print_summary(self) -> None:
        self.campaign_progress("Authentication mechanism summary:")
        mechanism: str
        for mechanism in AUTH_MECHANISMS:
            interactive_timing: InteractiveTimingStats | None = self.interactive_timings.get(mechanism)
            if interactive_timing is not None:
                successful_get_average: str = (
                    f"{statistics.mean(without_single_slowest(interactive_timing.successful_get_durations_ns)) / 1_000_000_000:.9f} seconds"
                    if interactive_timing.successful_get_durations_ns
                    else "N/A"
                )
                successful_post_average: str = (
                    f"{statistics.mean(without_single_slowest(interactive_timing.successful_post_durations_ns)) / 1_000_000_000:.9f} seconds"
                    if interactive_timing.successful_post_durations_ns
                    else "N/A"
                )
                failed_get_average: str = (
                    f"{statistics.mean(interactive_timing.failed_get_durations_ns) / 1_000_000_000:.9f} seconds"
                    if interactive_timing.failed_get_durations_ns
                    else "N/A"
                )
                failed_post_average: str = (
                    f"{statistics.mean(interactive_timing.failed_post_durations_ns) / 1_000_000_000:.9f} seconds"
                    if interactive_timing.failed_post_durations_ns
                    else "N/A"
                )
                failed_post_minimum: str = (
                    f"{min(interactive_timing.failed_post_durations_ns) / 1_000_000_000:.9f} seconds"
                    if interactive_timing.failed_post_durations_ns
                    else "N/A"
                )
                self.campaign_progress(
                    f"{mechanism}: result={self.outcomes[mechanism]}, "
                    f"successful GET /auth average={successful_get_average}, "
                    f"successful POST /auth average={successful_post_average}, "
                    f"failed GET /auth average={failed_get_average}, "
                    f"failed POST /auth average={failed_post_average}, "
                    f"failed POST /auth minimum={failed_post_minimum}"
                )
                continue
            stats: CampaignStats | None = self.campaigns.get(mechanism)
            if stats is None or not stats.durations_ns:
                failed_average: str = "N/A"
                failed_minimum: str = "N/A"
            else:
                failed_average = f"{statistics.mean(stats.durations_ns) / 1_000_000_000:.9f} seconds"
                failed_minimum = f"{min(stats.durations_ns) / 1_000_000_000:.9f} seconds"
            success_durations: list[int] = self.success_durations_ns.get(mechanism, [])
            success_average: str = (
                f"{statistics.mean(without_single_slowest(success_durations)) / 1_000_000_000:.9f} seconds"
                if success_durations
                else "N/A"
            )
            self.campaign_progress(
                f"{mechanism}: result={self.outcomes[mechanism]}, "
                f"successful average response time={success_average}, "
                f"failed average response time={failed_average}, "
                f"failed minimum response time={failed_minimum}"
            )

    def _post_config(
        self,
        session: requests.Session,
        body: dict[str, str],
        bearer_key: str | None = None,
    ) -> requests.Response:
        headers: dict[str, str] | None = (
            {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {bearer_key}"} if bearer_key is not None else None
        )
        return self.gateway.request(
            session,
            HttpMethod.POST,
            GatewayApi.CONFIG,
            headers=headers,
            json_body=body,
        )

    def _provision_temporary_state(
        self,
        realm: str,
    ) -> None:
        temporary_username: str = self._distinct_username(ADMIN_USERNAME)
        temporary_password: str = self._random_distinct_secret(self.config.gw_id)
        temporary_ro_key: str = self._random_distinct_secret(self.config.gw_id, temporary_password)
        temporary_rw_key: str = self._random_distinct_secret(self.config.gw_id, temporary_password, temporary_ro_key)
        self.temporary_username = temporary_username
        self.temporary_password = temporary_password
        self.temporary_ro_key = temporary_ro_key
        self.temporary_rw_key = temporary_rw_key
        stored_password: str = self.gateway.calculate_digest_ha1(
            temporary_username,
            realm,
            temporary_password,
        )
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
            GatewayCfgDesc.LAN_AUTH_USER: temporary_username,
            GatewayCfgDesc.LAN_AUTH_PASS: stored_password,
            GatewayCfgDesc.LAN_AUTH_API_KEY: temporary_ro_key,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: temporary_rw_key,
        }
        self.evidence.write(
            "TEMPORARY CREDENTIALS",
            TemporaryCredentialsEvidence(
                username=temporary_username,
                plaintext_password=temporary_password,
                realm=realm,
                stored_password=stored_password,
                ro_key=temporary_ro_key,
                rw_key=temporary_rw_key,
            ),
        )
        self.evidence.write("CONFIGURATION TRANSITION", body)
        default_session: requests.Session = self._successful_login(
            ADMIN_USERNAME,
            self.config.gw_id,
            GatewayCfgLanAuthType.DEFAULT,
        ).session
        self.mutation_possible = True
        response: requests.Response = self._post_config(default_session, body)
        self._require_setup(
            response.status_code == HttpStatus.C_200_OK,
            "temporary credential configuration succeeds",
            response.status_code,
        )

        custom: InteractiveAuthResult = self._successful_login(
            temporary_username,
            temporary_password,
            GatewayCfgLanAuthType.RUUVI,
        )
        ro_history: requests.Response = self.gateway.request(
            self.gateway.new_session(),
            HttpMethod.GET,
            GatewayApi.HISTORY,
            headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {temporary_ro_key}"},
        )
        self._require_setup(
            ro_history.status_code == HttpStatus.C_200_OK,
            "temporary RO key reads /history",
            ro_history.status_code,
        )
        prepared: dict[str, Any] = self._read_prepared_config(custom.session)
        self._require_setup(
            prepared.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI,
            f"prepared state uses {GatewayCfgLanAuthType.RUUVI}",
            prepared.get(GatewayCfgDesc.LAN_AUTH_TYPE),
        )
        self._require_setup(
            prepared.get(GatewayCfgDesc.LAN_AUTH_USER) == temporary_username,
            "prepared state uses the temporary username",
            prepared.get(GatewayCfgDesc.LAN_AUTH_USER),
        )
        self._require_setup(
            prepared.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE) is True,
            "prepared state enables the RO API key",
            prepared.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE),
        )
        self._require_setup(
            prepared.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE) is True,
            "prepared state enables the RW API key",
            prepared.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE),
        )
        self.outcomes["temporary-state setup"] = "PASS"

    def _verify_rw_noop(self) -> None:
        session: requests.Session = self.gateway.new_session()
        session.headers[HttpHeader.AUTHORIZATION] = f"{HttpAuthScheme.BEARER} {self.temporary_rw_key or ''}"
        before: dict[str, Any] = self._read_config(session, "configuration before RW no-op")
        response: requests.Response = self._post_config(self.gateway.new_session(), {}, self.temporary_rw_key)
        self._require_security(
            response.status_code == HttpStatus.C_200_OK,
            "real RW key authorizes no-op POST /ruuvi.json",
            response.status_code,
        )
        after: dict[str, Any] = self._read_config(session, "configuration after RW no-op")
        self._require_security(
            canonical_json_hash(before) == canonical_json_hash(after),
            "RW no-op preserves the configuration hash",
            canonical_json_hash(after),
        )

    def _restore(self) -> bool:
        if not self.mutation_possible:
            return True
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_API_KEY: "",
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
        }
        self.evidence.write("CONFIGURATION RESTORATION", body)
        restored: bool = False
        attempts: list[RestorationAttemptEvidence] = []

        def try_post(name: str, session: requests.Session, bearer_key: str | None = None) -> bool:
            try:
                post_response: requests.Response = self._post_config(session, body, bearer_key)
                attempts.append(
                    RestorationStatusAttemptEvidence(
                        method=name,
                        status=post_response.status_code,
                    )
                )
                return post_response.status_code == HttpStatus.C_200_OK
            except Exception as post_error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                post_error: Exception
                attempts.append(
                    RestorationErrorAttemptEvidence(
                        method=name,
                        error=type(post_error).__name__,
                        message=str(post_error),
                    )
                )
                self.evidence.exception(post_error)
                return False

        if self.temporary_rw_key is not None:
            restored = try_post("temporary RW bearer", self.gateway.new_session(), self.temporary_rw_key)
        if not restored and self.temporary_username is not None and self.temporary_password is not None:
            try:
                custom: InteractiveAuthResult = self.gateway.authenticate_interactive(
                    self.temporary_username,
                    self.temporary_password,
                )
                if custom.login_response.status_code == HttpStatus.C_200_OK:
                    restored = try_post("temporary custom interactive", custom.session)
                else:
                    attempts.append(
                        RestorationStatusAttemptEvidence(
                            method="temporary custom interactive",
                            status=custom.login_response.status_code,
                        )
                    )
            except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                error: Exception
                attempts.append(
                    RestorationErrorAttemptEvidence(
                        method="temporary custom interactive",
                        error=type(error).__name__,
                        message=str(error),
                    )
                )
                self.evidence.exception(error)
        if not restored:
            try:
                default: InteractiveAuthResult = self.gateway.authenticate_interactive(
                    ADMIN_USERNAME, self.config.gw_id
                )
                if default.login_response.status_code == HttpStatus.C_200_OK:
                    restored = try_post("default interactive", default.session)
                else:
                    attempts.append(
                        RestorationStatusAttemptEvidence(
                            method="default interactive",
                            status=default.login_response.status_code,
                        )
                    )
            except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                attempts.append(
                    RestorationErrorAttemptEvidence(
                        method="default interactive",
                        error=type(error).__name__,
                        message=str(error),
                    )
                )
                self.evidence.exception(error)
        self.evidence.write("RESTORATION ATTEMPTS", attempts)
        if not restored:
            self.outcomes["final restoration"] = "ERROR"
            return False

        try:
            default = self._successful_login(
                ADMIN_USERNAME,
                self.config.gw_id,
                GatewayCfgLanAuthType.DEFAULT,
            )
            restored_config: dict[str, Any] = self._read_config(default.session, "restored GET /ruuvi.json")
            self._require_setup(
                restored_config.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT,
                f"restored mode is {GatewayCfgLanAuthType.DEFAULT}",
                restored_config.get(GatewayCfgDesc.LAN_AUTH_TYPE),
            )
            self._require_setup(
                restored_config.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE) is False,
                "restored RO key is disabled",
                restored_config.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE),
            )
            self._require_setup(
                restored_config.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE) is False,
                "restored RW key is disabled",
                restored_config.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE),
            )
            self._require_setup(
                canonical_json_hash(restored_config) == self.baseline_hash,
                "restored configuration matches the stable baseline",
                canonical_json_hash(restored_config),
            )
            custom = self.gateway.authenticate_interactive(
                self.temporary_username or "",
                self.temporary_password or "",
            )
            self._require_setup(
                custom.login_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                "temporary custom credential no longer authorizes",
                custom.login_response.status_code,
            )
            bearer: BearerAccessCheck
            for bearer in (
                BearerAccessCheck("RO", self.temporary_ro_key, GatewayApi.HISTORY),
                BearerAccessCheck("RW", self.temporary_rw_key, GatewayApi.CONFIG),
            ):
                response: requests.Response = self.gateway.request(
                    self.gateway.new_session(),
                    HttpMethod.GET,
                    bearer.path,
                    headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {bearer.key or ''}"},
                )
                self._require_setup(
                    response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    f"temporary {bearer.name} bearer no longer authorizes",
                    response.status_code,
                )
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            self.evidence.exception(error)
            self.outcomes["final restoration"] = "ERROR"
            return False
        self.outcomes["final restoration"] = "PASS"
        return True

    def run(self) -> RunResult:
        self.evidence.write("TEST CASE AND UNIT", TEST_ID)
        self.evidence.write("UTC START", format_utc(self.evidence.started_at))
        self.evidence.write(
            "DUT CONFIGURATION",
            self.config,
        )
        verdict: str = "ERROR"
        exit_code: int = 2
        failure: BaseException | None = None
        security_failures: list[SecurityFailure] = []
        active_mechanism: str | None = None

        def observe_security(operation: Callable[[], None]) -> None:
            try:
                operation()
            except SecurityFailure as security_error:
                security_error: SecurityFailure
                if isinstance(security_error, UnexpectedAuthorization):
                    raise
                security_failures.append(security_error)
                self.evidence.exception(security_error)

        try:
            self.progress("Authenticating with the default administrative credential")
            try:
                initial: InteractiveAuthResult = self._successful_login(
                    ADMIN_USERNAME,
                    self.config.gw_id,
                    GatewayCfgLanAuthType.DEFAULT,
                    check_mode=False,
                )
            except GatewayAuthenticationModeError:
                self.factory_reset_required = True
                raise
            self.progress("Reading and validating the initial gateway configuration")
            first_baseline: dict[str, Any] = self._read_config(initial.session, "first baseline GET /ruuvi.json")
            self._validate_baseline(first_baseline, initial.auth_payload)
            self.progress("Confirming the baseline configuration is stable")
            second_baseline: dict[str, Any] = self._read_config(initial.session, "second baseline GET /ruuvi.json")
            first_hash: str = canonical_json_hash(first_baseline)
            second_hash: str = canonical_json_hash(second_baseline)
            self._require_setup(first_hash == second_hash, "two baseline configuration reads are stable")
            self.baseline = second_baseline
            self.baseline_hash = second_hash
            self.evidence.write("BASELINE CONFIGURATION SHA256", self.baseline_hash)
            self.progress("Performing the successful default-login control")
            control: InteractiveAuthResult = self._successful_login(
                ADMIN_USERNAME,
                self.config.gw_id,
                GatewayCfgLanAuthType.DEFAULT,
            )
            realm: str = control.challenge["realm"]
            self.evidence.write("LIVE AUTHENTICATION REALM", realm)
            observe_security(
                lambda: self._measure_interactive_login_success(
                    AuthMech.LAN_WEBUI_DEFAULT,
                    ADMIN_USERNAME,
                    self.config.gw_id,
                    GatewayCfgLanAuthType.DEFAULT,
                )
            )

            self.progress("Attacking default interactive authentication")
            observe_security(
                lambda: self._attack_interactive(
                    AuthMech.LAN_WEBUI_DEFAULT,
                    GatewayCfgLanAuthType.DEFAULT,
                )
            )
            self.progress("Provisioning temporary interactive and bearer credentials")
            try:
                self._provision_temporary_state(realm)
                self.progress("Verifying the prepared temporary state")
                self._successful_login(
                    self.temporary_username or "",
                    self.temporary_password or "",
                    GatewayCfgLanAuthType.RUUVI,
                )
                self.evidence.write("PREPARED STATE RESULT", "PASS")
            except Exception:
                self.outcomes["temporary-state setup"] = "ERROR"
                raise
            observe_security(
                lambda: self._measure_interactive_login_success(
                    AuthMech.LAN_WEBUI_USER_DEFINED,
                    self.temporary_username or "",
                    self.temporary_password or "",
                    GatewayCfgLanAuthType.RUUVI,
                )
            )
            observe_security(
                lambda: self._measure_success(
                    AuthMech.M2M_API_BEARER_RO,
                    self.gateway.new_session(),
                    headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {self.temporary_ro_key or ''}"},
                )
            )
            observe_security(
                lambda: self._measure_success(
                    AuthMech.M2M_API_BEARER_RW,
                    self.gateway.new_session(),
                    headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {self.temporary_rw_key or ''}"},
                )
            )
            self.progress("Attacking user-defined interactive authentication")
            observe_security(
                lambda: self._attack_interactive(
                    AuthMech.LAN_WEBUI_USER_DEFINED,
                    GatewayCfgLanAuthType.RUUVI,
                )
            )
            self.progress("Performing the successful custom-login control")
            active_mechanism = AuthMech.LAN_WEBUI_USER_DEFINED
            self._successful_login(
                self.temporary_username or "",
                self.temporary_password or "",
                GatewayCfgLanAuthType.RUUVI,
                setup_check=False,
            )
            configured_keys: set[str] = {self.temporary_ro_key or "", self.temporary_rw_key or ""}
            active_mechanism = None
            self.progress("Attacking read-only bearer authentication")
            observe_security(
                lambda: self._attack_bearer(
                    AuthMech.M2M_API_BEARER_RO,
                    GatewayApi.HISTORY,
                    configured_keys,
                )
            )
            active_mechanism = AuthMech.M2M_API_BEARER_RO
            ro_control: requests.Response = self.gateway.request(
                self.gateway.new_session(),
                HttpMethod.GET,
                GatewayApi.HISTORY,
                headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {self.temporary_ro_key or ''}"},
            )
            self._require_security(
                ro_control.status_code == HttpStatus.C_200_OK,
                "real RO key remains operational",
                ro_control.status_code,
            )
            # All prepared-state read probes precede the potentially mutating
            # configuration probes. A 2xx denial bypass aborts to restoration.
            ro_post: requests.Response = self._post_config(self.gateway.new_session(), {}, self.temporary_ro_key)
            if HttpStatus.C_200_OK <= ro_post.status_code < HttpStatus.C_300_MULTIPLE_CHOICES:
                raise UnexpectedAuthorization("RO key authorized POST /ruuvi.json")
            self._require_security(
                ro_post.status_code == HttpStatus.C_401_UNAUTHORIZED,
                "real RO key cannot POST /ruuvi.json",
                ro_post.status_code,
            )
            active_mechanism = AuthMech.M2M_API_BEARER_RW
            self._verify_rw_noop()
            self.progress("Attacking read/write bearer authentication")
            active_mechanism = None
            observe_security(
                lambda: self._attack_bearer(
                    AuthMech.M2M_API_BEARER_RW,
                    GatewayApi.CONFIG,
                    configured_keys,
                )
            )
            active_mechanism = AuthMech.M2M_API_BEARER_RW
            self._verify_rw_noop()
            active_mechanism = None
            observe_security(lambda: self._test_additional_auth_modes(realm))
            if security_failures:
                raise security_failures[0]
        except SecurityFailure as error:
            error: Exception
            if active_mechanism is not None:
                self._set_outcome(active_mechanism, "FAIL")
            failure = error
            self.evidence.exception(error)
            verdict = "FAIL"
            exit_code = 1
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            if active_mechanism is not None:
                self._set_outcome(active_mechanism, "ERROR")
            failure = error
            self.evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        finally:
            self.progress("Restoring the original gateway authentication configuration")
            restoration_ok: bool = self._restore()
            self.progress("Verifying restoration and aggregating the verdict")
            if not restoration_ok:
                self.factory_reset_required = True
                verdict = "ERROR"
                exit_code = 2
            elif failure is None:
                if all(self.outcomes[mechanism] == "PASS" for mechanism in MECHANISMS):
                    verdict, exit_code = "PASS", 0
                else:
                    self.evidence.write("INCOMPLETE RESULTS", "Not every required mechanism completed with PASS")
                    verdict, exit_code = "ERROR", 2
        outcome: str
        mechanism: str
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write(
                "FINAL RESULT",
                MechanismResultEvidence(mechanism=mechanism, result=outcome),
            )
        self._print_summary()
        self.evidence.write("OVERALL RESULT", verdict)
        recovery_message: str | None = FACTORY_RESET_MESSAGE if self.factory_reset_required else None
        return RunResult(exit_code, verdict, dict(self.outcomes), set(), recovery_message)


def execute_test_5_1_5_2_b(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now,
    monotonic_ns: Callable[[], int] = time.monotonic_ns,
    success_monotonic_ns: Callable[[], int] | None = None,
    attempt_count: int = ATTEMPTS_PER_TARGET,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    output: Callable[[str], None] | None = None,
) -> RunResult:
    if work_dir is None:
        work_dir = Path.cwd()
    if output is None:
        output = print
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_1_5_2_b", now)
    output(f"Open log file: {log.path}")

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {mechanism: "NOT RUN" for mechanism in MECHANISMS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    try:
        try:
            progress.step("Loading and validating .env")
            config: DutConfig = load_dut_config(work_dir / ".env")
            log.write(
                "DUT CONFIGURATION",
                config,
            )
            result = FunctionalTest_5_1_5_2_b(
                config,
                log,
                session_factory=session_factory,
                random_bytes=random_bytes,
                monotonic_ns=monotonic_ns,
                success_monotonic_ns=success_monotonic_ns,
                attempt_count=attempt_count,
                progress=progress.step,
                campaign_progress=output_progress,
            ).run()
        except Exception as error:
            error: Exception
            log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
            log.write("OVERALL RESULT", "ERROR")
    finally:
        if result.recovery_message is not None:
            log.write("USER ACTION REQUIRED", result.recovery_message)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    if result.recovery_message is not None:
        output(result.recovery_message)
    return result


def main() -> int:
    return execute_test_5_1_5_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
