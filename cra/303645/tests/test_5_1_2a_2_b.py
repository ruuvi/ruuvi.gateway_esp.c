"""Live ETSI 5.1-2A-2 Unit B password-rejection test for a Ruuvi Gateway."""

from __future__ import annotations

import hashlib
import json
import secrets
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Callable

import requests

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import GatewayAuthenticationModeError, InvalidSetup
from lib.evidence import (
    AssertionEvidence,
    EvidenceLog,
    HashComparisonEvidence,
    MechanismResultEvidence,
    RouteResultEvidence,
    format_utc,
    utc_now,
)
from lib.gateway import (
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthResult,
)
from lib.http_api import (
    API_INVENTORY,
    ApiRoute,
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-2A-2, Test Unit B"
ADMIN_USERNAME: str = "Admin"
HTTP_TIMEOUT: tuple[int, int] = (5, 15)
USER_AGENT: str = "ruuvi-etsi-test-5.1-2a-2-b"
TOTAL_STEPS: int = 11
MECHANISMS: tuple[str, ...] = (
    AuthMech.M2M_API_BEARER_RO,
    AuthMech.M2M_API_BEARER_RW,
    "temporary-state setup",
    "final restoration and non-mutation",
)


class SecurityFailure(Exception):
    """A security assertion failed."""


@dataclass(frozen=True)
class NegativeProbe:
    name: str
    method: str
    path: str
    expected_status: int
    mechanism: str
    authorization: str | None
    body: dict[str, Any] | None


@dataclass(frozen=True)
class PositiveProbe:
    name: str
    method: str
    path: str
    expected_status: int
    mechanism: str
    key: str
    body: dict[str, Any] | None


@dataclass(frozen=True)
class RestorationAttempt:
    method: str
    status: int | None
    error: str | None


def normalize_mac(value: str) -> str:
    return value.replace(":", "").upper()


def canonical_json_hash(value: Any) -> str:
    encoded: bytes = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def api_route(method: str, path: str) -> ApiRoute:
    route: ApiRoute = ApiRoute(method, path)
    if route not in API_INVENTORY:
        raise InvalidSetup(f"required route is absent from API_INVENTORY: {method} {path}")
    return route


class FunctionalTest_5_1_2a_2_b:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        progress: Callable[[str], None] | None = None,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config,
            evidence,
            session_factory=session_factory,
            random_bytes=random_bytes,
            timeout=HTTP_TIMEOUT,
            user_agent=USER_AGENT,
        )
        self.progress: Callable[[str], None] = progress if progress is not None else lambda description: None
        self.outcomes: dict[str, str] = {mechanism: "NOT RUN" for mechanism in MECHANISMS}
        self.coverage: set[ApiRoute] = set()
        self.admin_session: requests.Session | None = None
        self.baseline_hash: str | None = None
        self.prepared_hash: str | None = None
        self.ro_key: str | None = None
        self.rw_key: str | None = None
        self.mutation_possible: bool = False
        self.factory_reset_required: bool = False

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

    def _require_security(
        self, condition: bool, description: str, actual: Any = "", mechanism: str | None = None
    ) -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            if mechanism is not None:
                self.outcomes[mechanism] = "FAIL"
            raise SecurityFailure(f"{description} (actual: {actual!r})")

    def _authenticate_admin(self) -> InteractiveAuthResult:
        try:
            result: InteractiveAuthResult = self.gateway.authenticate_interactive(ADMIN_USERNAME, self.config.gw_id)
        except GatewayAuthenticationModeError:
            self.factory_reset_required = True
            raise
        self._require_setup(
            result.challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            "GET /auth returns the interactive challenge",
            result.challenge_response.status_code,
        )
        if result.login_response.status_code != HttpStatus.C_200_OK:
            self.factory_reset_required = True
        self._require_setup(
            result.login_response.status_code == HttpStatus.C_200_OK,
            "default administrative credentials authenticate",
            result.login_response.status_code,
        )
        return result

    def _read_config(self, session: requests.Session | None, context: str) -> dict[str, Any]:
        if session is None:
            raise InvalidSetup("administrative session is not initialized")
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._require_setup(
            response.status_code == HttpStatus.C_200_OK,
            f"{context} succeeds",
            response.status_code,
        )
        return self.gateway.response_json(response, context, dict)

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any]) -> None:
        if GatewayCfgDesc.GW_MAC in payload:
            actual_mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require_setup(isinstance(actual_mac, str), "gw_mac is a string")
            self._require_setup(
                OCTETS_6_RE.fullmatch(actual_mac) is not None,
                "gw_mac is a six-octet MAC",
                actual_mac,
            )
            self._require_setup(
                normalize_mac(actual_mac) == normalize_mac(self.config.gw_mac),
                "DUT gw_mac matches .env",
                actual_mac,
            )
        # A successful login permits an authenticated identity check before any
        # non-default auth state can trigger destructive recovery advice.
        auth_type: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if auth_type != GatewayCfgLanAuthType.DEFAULT:
            self.factory_reset_required = GatewayCfgDesc.GW_MAC in payload
        self._require_setup(
            auth_type == GatewayCfgLanAuthType.DEFAULT,
            f"interactive authentication reports {GatewayCfgLanAuthType.DEFAULT}",
            auth_type,
        )
        expected: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        value: Any
        field: str
        for field, value in expected.items():
            actual: Any = payload.get(field)
            is_default: bool = type(actual) is type(value) and actual == value
            if not is_default:
                self.factory_reset_required = GatewayCfgDesc.GW_MAC in payload
            self._require_setup(is_default, f"{field} has its factory-default value", actual)
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

    def _provision(self) -> None:
        if self.admin_session is None:
            raise InvalidSetup("administrative session is not initialized")
        self.ro_key = self.gateway.random_text(32)
        self.rw_key = self.gateway.random_text(32)
        self._require_setup(self.ro_key != self.rw_key, "temporary RO and RW keys are distinct")
        self._require_setup(
            self.ro_key != self.config.gw_id and self.rw_key != self.config.gw_id,
            "temporary keys differ from the administrative password",
        )
        self.evidence.write("TEMPORARY RO KEY", self.ro_key)
        self.evidence.write("TEMPORARY RW KEY", self.rw_key)
        body: dict[str, str | None] = {
            GatewayCfgDesc.LAN_AUTH_API_KEY: self.ro_key,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: self.rw_key,
        }
        self.evidence.write("CONFIGURATION TRANSITION", body)
        self.mutation_possible = True
        response: requests.Response = self.gateway.request(
            self.admin_session,
            HttpMethod.POST,
            GatewayApi.CONFIG,
            json_body=body,
        )
        self._require_setup(
            response.status_code == HttpStatus.C_200_OK,
            "temporary API-key provisioning returns 200",
            response.status_code,
        )
        self._require_setup(
            self.gateway.response_json(response, "provisioning POST /ruuvi.json", dict) == {},
            "temporary API-key provisioning returns an empty object",
        )

    def _verify_prepared(self) -> None:
        prepared: dict[str, Any] = self._read_config(self.admin_session, "prepared GET /ruuvi.json")
        expected: dict[str, str | bool] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: True,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: True,
        }
        value: str | bool
        field: str
        for field, value in expected.items():
            self._require_setup(
                prepared.get(field) is value if isinstance(value, bool) else prepared.get(field) == value,
                f"prepared {field} has the required value",
                prepared.get(field),
            )
        self.prepared_hash = canonical_json_hash(prepared)
        self.evidence.write("PREPARED CONFIGURATION SHA256", self.prepared_hash)
        self.outcomes["temporary-state setup"] = "PASS"

    def _negative_matrix(self, realm: str) -> list[NegativeProbe]:
        basic: str = self.gateway.authorization_header_basic(ADMIN_USERNAME, self.config.gw_id)

        def digest(request_method: str, request_path: str) -> str:
            return self.gateway.authorization_header_digest(
                ADMIN_USERNAME,
                self.config.gw_id,
                request_method,
                request_path,
                {
                    "realm": realm,
                    "nonce": self.gateway.random_text(18),
                    "qop": "auth",
                    "opaque": self.gateway.random_text(18),
                },
            )

        probes: list[NegativeProbe] = []
        routes: tuple[tuple[str, str, int, str], ...] = (
            (HttpMethod.GET, GatewayApi.HISTORY, HttpStatus.C_302_FOUND, AuthMech.M2M_API_BEARER_RO),
            (HttpMethod.GET, GatewayApi.CONFIG, HttpStatus.C_302_FOUND, AuthMech.M2M_API_BEARER_RO),
            (
                HttpMethod.POST,
                GatewayApi.CONFIG,
                HttpStatus.C_401_UNAUTHORIZED,
                AuthMech.M2M_API_BEARER_RW,
            ),
        )
        scheme: str
        for scheme in (HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            mechanism: str
            expected: int
            path: str
            method: str
            for method, path, expected, mechanism in routes:
                authorization: str = basic if scheme == HttpAuthScheme.BASIC else digest(method, path)
                probes.append(
                    NegativeProbe(
                        scheme,
                        method,
                        path,
                        expected,
                        mechanism,
                        authorization,
                        {} if method == HttpMethod.POST else None,
                    )
                )
        password_bearer: str = f"{HttpAuthScheme.BEARER} {self.config.gw_id}"
        _: int
        for method, path, _, mechanism in routes:
            probes.append(
                NegativeProbe(
                    "interactive password as bearer token",
                    method,
                    path,
                    HttpStatus.C_401_UNAUTHORIZED,
                    mechanism,
                    password_bearer,
                    {} if method == HttpMethod.POST else None,
                )
            )
        password_hash: str = self.gateway.calculate_digest_ha1(
            ADMIN_USERNAME,
            realm,
            self.config.gw_id,
        )
        probes.append(
            NegativeProbe(
                "interactive password JSON body",
                HttpMethod.POST,
                GatewayApi.CONFIG,
                HttpStatus.C_401_UNAUTHORIZED,
                AuthMech.M2M_API_BEARER_RW,
                None,
                {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
                    GatewayCfgDesc.LAN_AUTH_USER: ADMIN_USERNAME,
                    GatewayCfgDesc.LAN_AUTH_PASS: password_hash,
                    "password": self.config.gw_id,
                },
            )
        )
        return probes

    def _run_negative_matrix(self, realm: str, method: str) -> None:
        self._require_security(
            self.config.gw_id not in {self.ro_key, self.rw_key},
            "password-as-token probe differs from both configured keys",
        )
        probe: NegativeProbe
        for probe in self._negative_matrix(realm):
            if probe.method != method:
                continue
            response: requests.Response = self.gateway.request(
                self.gateway.new_session(),
                probe.method,
                probe.path,
                headers=({HttpHeader.AUTHORIZATION: probe.authorization} if probe.authorization is not None else None),
                json_body=probe.body,
            )
            self.coverage.add(api_route(probe.method, probe.path))
            try:
                self._require_security(
                    not 200 <= response.status_code <= 299,
                    f"{probe.name} gains no access to {probe.method} {probe.path}",
                    response.status_code,
                )
                self._require_security(
                    response.status_code == probe.expected_status,
                    f"{probe.name} returns the exact status for {probe.method} {probe.path}",
                    response.status_code,
                )
            except SecurityFailure:
                self.outcomes[probe.mechanism] = "FAIL"
                raise
            self.evidence.write(
                "PER-PROBE RESULT",
                RouteResultEvidence(
                    mechanism=probe.mechanism,
                    method=probe.method,
                    path=probe.path,
                    result=f"PASS ({response.status_code})",
                ),
            )
        if method == HttpMethod.GET:
            return
        current: dict[str, Any] = self._read_config(self.admin_session, "post-negative GET /ruuvi.json")
        current_hash: str = canonical_json_hash(current)
        self._require_security(
            current_hash == self.prepared_hash,
            "negative POST probes did not change configuration",
            HashComparisonEvidence(baseline=self.prepared_hash or "", final=current_hash),
            mechanism=AuthMech.M2M_API_BEARER_RW,
        )

    def _positive_matrix(self) -> list[PositiveProbe]:
        if self.ro_key is None or self.rw_key is None:
            raise InvalidSetup("temporary bearer keys were not generated")
        return [
            PositiveProbe(
                "RO bearer",
                HttpMethod.GET,
                GatewayApi.HISTORY,
                HttpStatus.C_200_OK,
                AuthMech.M2M_API_BEARER_RO,
                self.ro_key,
                None,
            ),
            PositiveProbe(
                "RO bearer",
                HttpMethod.GET,
                GatewayApi.CONFIG,
                HttpStatus.C_200_OK,
                AuthMech.M2M_API_BEARER_RO,
                self.ro_key,
                None,
            ),
            PositiveProbe(
                "RO bearer",
                HttpMethod.POST,
                GatewayApi.CONFIG,
                HttpStatus.C_401_UNAUTHORIZED,
                AuthMech.M2M_API_BEARER_RO,
                self.ro_key,
                {},
            ),
            PositiveProbe(
                "RW bearer",
                HttpMethod.GET,
                GatewayApi.HISTORY,
                HttpStatus.C_200_OK,
                AuthMech.M2M_API_BEARER_RW,
                self.rw_key,
                None,
            ),
            PositiveProbe(
                "RW bearer",
                HttpMethod.GET,
                GatewayApi.CONFIG,
                HttpStatus.C_200_OK,
                AuthMech.M2M_API_BEARER_RW,
                self.rw_key,
                None,
            ),
            PositiveProbe(
                "RW bearer",
                HttpMethod.POST,
                GatewayApi.CONFIG,
                HttpStatus.C_200_OK,
                AuthMech.M2M_API_BEARER_RW,
                self.rw_key,
                {},
            ),
        ]

    def _run_positive_matrix(self, method: str) -> None:
        probe: PositiveProbe
        for probe in self._positive_matrix():
            if probe.method != method:
                continue
            response: requests.Response = self.gateway.request(
                self.gateway.new_session(),
                probe.method,
                probe.path,
                headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {probe.key}"},
                json_body=probe.body,
            )
            self.coverage.add(api_route(probe.method, probe.path))
            try:
                self._require_security(
                    response.status_code == probe.expected_status,
                    f"{probe.name} has the required scope for {probe.method} {probe.path}",
                    response.status_code,
                )
            except SecurityFailure:
                self.outcomes[probe.mechanism] = "FAIL"
                raise
            self.evidence.write(
                "PER-PROBE RESULT",
                RouteResultEvidence(
                    mechanism=probe.mechanism,
                    method=probe.method,
                    path=probe.path,
                    result=f"PASS ({response.status_code})",
                ),
            )
            if method == HttpMethod.POST and probe.mechanism == AuthMech.M2M_API_BEARER_RO:
                self.outcomes[AuthMech.M2M_API_BEARER_RO] = "PASS"
        if method == HttpMethod.GET:
            return
        current: dict[str, Any] = self._read_config(self.admin_session, "post-positive GET /ruuvi.json")
        current_hash: str = canonical_json_hash(current)
        self._require_security(
            current_hash == self.prepared_hash,
            "successful RW empty-object POST did not change configuration",
            HashComparisonEvidence(baseline=self.prepared_hash or "", final=current_hash),
            mechanism=AuthMech.M2M_API_BEARER_RW,
        )
        self.outcomes[AuthMech.M2M_API_BEARER_RW] = "PASS"

    def _restore(self) -> bool:
        if not self.mutation_possible:
            return True
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_API_KEY: "",
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
        }
        self.evidence.write("CONFIGURATION RESTORATION", body)
        attempts: list[RestorationAttempt] = []

        def attempt(attempt_name: str, session: requests.Session, bearer_key: str | None = None) -> bool:
            headers: dict[str, str] | None = (
                {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {bearer_key}"} if bearer_key is not None else None
            )
            try:
                post_response: requests.Response = self.gateway.request(
                    session,
                    HttpMethod.POST,
                    GatewayApi.CONFIG,
                    headers=headers,
                    json_body=body,
                )
                attempts.append(RestorationAttempt(attempt_name, post_response.status_code, None))
                return post_response.status_code == HttpStatus.C_200_OK
            except Exception as post_error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                post_error: Exception
                attempts.append(RestorationAttempt(attempt_name, None, f"{type(post_error).__name__}: {post_error}"))
                self.evidence.exception(post_error)
                return False

        restored: bool = False
        if self.admin_session is not None:
            restored = attempt("authorized Admin session", self.admin_session)
        if not restored and self.rw_key is not None:
            restored = attempt("temporary RW bearer", self.gateway.new_session(), self.rw_key)
        if not restored:
            try:
                login: InteractiveAuthResult = self.gateway.authenticate_interactive(ADMIN_USERNAME, self.config.gw_id)
                if login.login_response.status_code == HttpStatus.C_200_OK:
                    restored = attempt("re-authenticated Admin session", login.session)
                else:
                    attempts.append(
                        RestorationAttempt(
                            "re-authenticated Admin session",
                            login.login_response.status_code,
                            None,
                        )
                    )
            except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                error: Exception
                attempts.append(
                    RestorationAttempt(
                        "re-authenticated Admin session",
                        None,
                        f"{type(error).__name__}: {error}",
                    )
                )
                self.evidence.exception(error)
        self.evidence.write("RESTORATION ATTEMPTS", attempts)
        if not restored:
            self.outcomes["final restoration and non-mutation"] = "ERROR"
            return False

        try:
            login = self._authenticate_admin()
            restored_config: dict[str, Any] = self._read_config(login.session, "restored GET /ruuvi.json")
            self._validate_baseline(restored_config, login.auth_payload)
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
            restored_hash: str = canonical_json_hash(restored_config)
            self._require_setup(
                restored_hash == self.baseline_hash,
                "restored configuration equals the baseline",
                HashComparisonEvidence(baseline=self.baseline_hash or "", final=restored_hash),
            )
            rejected: tuple[tuple[str, str | None, str, str], ...] = (
                (
                    "RO",
                    self.ro_key,
                    HttpMethod.GET,
                    GatewayApi.HISTORY,
                ),
                (
                    "RW",
                    self.rw_key,
                    HttpMethod.POST,
                    GatewayApi.CONFIG,
                ),
            )
            path: str
            method: str
            key: str | None
            name: str
            for name, key, method, path in rejected:
                response: requests.Response = self.gateway.request(
                    self.gateway.new_session(),
                    method,
                    path,
                    headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {key or ''}"},
                    json_body={} if method == HttpMethod.POST else None,
                )
                self._require_setup(
                    response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    f"temporary {name} key no longer authorizes",
                    response.status_code,
                )
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            self.evidence.exception(error)
            self.outcomes["final restoration and non-mutation"] = "ERROR"
            return False
        self.outcomes["final restoration and non-mutation"] = "PASS"
        return True

    def run(self) -> RunResult:
        self.evidence.write("TEST CASE AND UNIT", TEST_ID)
        self.evidence.write("UTC START", format_utc(self.evidence.started_at))
        self.evidence.write("DUT CONFIGURATION", self.config)
        self.evidence.write(
            "SCOPE BOUNDARY",
            "Interactive Web-UI access control, unconfigured-secret boundaries, rate limiting, "
            "and Unit A interface discovery are out of scope.",
        )
        verdict: str = "ERROR"
        exit_code: int = 2
        failure: BaseException | None = None
        try:
            self.progress("Authenticating with the default administrative credentials")
            login: InteractiveAuthResult = self._authenticate_admin()
            self.admin_session = login.session
            self.progress("Reading and validating the baseline gateway configuration")
            baseline: dict[str, Any] = self._read_config(self.admin_session, "baseline GET /ruuvi.json")
            self._validate_baseline(baseline, login.auth_payload)
            self.baseline_hash = canonical_json_hash(baseline)
            self.evidence.write("BASELINE CONFIGURATION SHA256", self.baseline_hash)
            self.progress("Provisioning temporary RO and RW bearer keys")
            try:
                self._provision()
                self.progress("Verifying the prepared temporary state")
                self._verify_prepared()
            except Exception:
                self.outcomes["temporary-state setup"] = "ERROR"
                raise
            self.progress("Testing rejection of password schemes by M2M read routes")
            self._run_negative_matrix(login.challenge["realm"], HttpMethod.GET)
            self.progress("Testing positive RO and RW bearer read scope")
            self._run_positive_matrix(HttpMethod.GET)
            self.progress("Testing rejection of password schemes by M2M write routes")
            self._run_negative_matrix(login.challenge["realm"], HttpMethod.POST)
            self.progress("Testing positive RO and RW bearer write scope")
            self._run_positive_matrix(HttpMethod.POST)
        except SecurityFailure as error:
            error: Exception
            failure = error
            self.evidence.exception(error)
            verdict = "FAIL"
            exit_code = 1
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            failure = error
            self.evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        finally:
            self.progress("Restoring the original API-key configuration")
            restoration_ok: bool = self._restore()
            self.progress("Verifying restoration and aggregating the verdict")
            if not restoration_ok:
                self.factory_reset_required = True
                verdict = "ERROR"
                exit_code = 2
            elif failure is None:
                verdict = "PASS"
                exit_code = 0
        outcome: str
        mechanism: str
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write(
                "FINAL RESULT",
                MechanismResultEvidence(mechanism=mechanism, result=outcome),
            )
        self.evidence.write("OVERALL RESULT", verdict)
        recovery_message: str | None = FACTORY_RESET_MESSAGE if self.factory_reset_required else None
        return RunResult(exit_code, verdict, dict(self.outcomes), set(self.coverage), recovery_message)


def execute_test_5_1_2a_2_b(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    output: Callable[[str], None] | None = None,
) -> RunResult:
    if work_dir is None:
        work_dir = Path.cwd()
    if output is None:
        output = print
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_1_2a_2_b", now)
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
            log.write("DUT CONFIGURATION", config)
            result = FunctionalTest_5_1_2a_2_b(
                config,
                log,
                session_factory=session_factory,
                random_bytes=random_bytes,
                progress=progress.step,
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
    return execute_test_5_1_2a_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
