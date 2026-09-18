#!/usr/bin/env python3
"""Live ETSI 5.1-2A-2 Unit B password-rejection test for a Ruuvi Gateway."""

import hashlib
import json
import secrets
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set

import requests

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import InvalidSetup
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

TEST_ID = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-2A-2, Test Unit B"
ADMIN_USERNAME = "Admin"
HTTP_TIMEOUT = (5, 15)
USER_AGENT = "ruuvi-etsi-test-5.1-2a-2-b"
TOTAL_STEPS = 9
MECHANISMS = (
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
    authorization: Optional[str]
    body: Optional[Dict[str, Any]]


@dataclass(frozen=True)
class PositiveProbe:
    name: str
    method: str
    path: str
    expected_status: int
    mechanism: str
    key: str
    body: Optional[Dict[str, Any]]


@dataclass(frozen=True)
class RestorationAttempt:
    method: str
    status: Optional[int]
    error: Optional[str]


def normalize_mac(value: str) -> str:
    return value.replace(":", "").upper()


def canonical_json_hash(value: Any) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode(
        "utf-8"
    )
    return hashlib.sha256(encoded).hexdigest()


def api_route(method: str, path: str) -> ApiRoute:
    route = ApiRoute(method, path)
    if route not in API_INVENTORY:
        raise InvalidSetup(f"required route is absent from API_INVENTORY: {method} {path}")
    return route


class FunctionalTest_5_1_2a_2_b:
    def __init__(
            self,
            config: DutConfig,
            evidence: EvidenceLog,
            session_factory: Callable[[], Any] = requests.Session,
            random_bytes: Callable[[int], bytes] = secrets.token_bytes,
            progress: Optional[Callable[[str], None]] = None,
    ) -> None:
        self.config = config
        self.evidence = evidence
        self.gateway = GatewayClient(
            config,
            evidence,
            session_factory=session_factory,
            random_bytes=random_bytes,
            timeout=HTTP_TIMEOUT,
            user_agent=USER_AGENT,
        )
        self.progress = progress if progress is not None else lambda description: None
        self.outcomes = {mechanism: "NOT RUN" for mechanism in MECHANISMS}
        self.coverage: Set[ApiRoute] = set()
        self.admin_session: Optional[Any] = None
        self.baseline_hash: Optional[str] = None
        self.prepared_hash: Optional[str] = None
        self.ro_key: Optional[str] = None
        self.rw_key: Optional[str] = None
        self.mutation_possible = False
        self.factory_reset_required = False

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

    def _authenticate_admin(self) -> Any:
        result = self.gateway.authenticate_interactive(ADMIN_USERNAME, self.config.gw_id)
        self._require_setup(
            result.challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            "GET /auth returns the interactive challenge",
            result.challenge_response.status_code,
        )
        if (
                result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
                != GatewayCfgLanAuthType.DEFAULT
        ):
            self.factory_reset_required = True
        self._require_setup(
            result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
            == GatewayCfgLanAuthType.DEFAULT,
            f"interactive authentication reports {GatewayCfgLanAuthType.DEFAULT}",
            result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE),
        )
        self._require_setup(
            result.login_response.status_code == HttpStatus.C_200_OK,
            "default administrative credentials authenticate",
            result.login_response.status_code,
        )
        return result

    def _read_config(self, session: Any, context: str) -> Dict[str, Any]:
        response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._require_setup(
            response.status_code == HttpStatus.C_200_OK,
            f"{context} succeeds",
            response.status_code,
        )
        return self.gateway.response_json(response, context, dict)

    def _validate_baseline(self, payload: Dict[str, Any]) -> None:
        expected = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        for field, value in expected.items():
            actual = payload.get(field)
            is_default = type(actual) is type(value) and actual == value
            if not is_default:
                self.factory_reset_required = True
            self._require_setup(is_default, f"{field} has its factory-default value", actual)
        if GatewayCfgDesc.GW_MAC in payload:
            actual_mac = payload[GatewayCfgDesc.GW_MAC]
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
        identity = {
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
        self.ro_key = self.gateway.random_text(32)
        self.rw_key = self.gateway.random_text(32)
        self._require_setup(self.ro_key != self.rw_key, "temporary RO and RW keys are distinct")
        self._require_setup(
            self.ro_key != self.config.gw_id and self.rw_key != self.config.gw_id,
            "temporary keys differ from the administrative password",
        )
        self.evidence.write("TEMPORARY RO KEY", self.ro_key)
        self.evidence.write("TEMPORARY RW KEY", self.rw_key)
        body = {
            GatewayCfgDesc.LAN_AUTH_API_KEY: self.ro_key,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: self.rw_key,
        }
        self.evidence.write("CONFIGURATION TRANSITION", body)
        self.mutation_possible = True
        response = self.gateway.request(
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
        prepared = self._read_config(self.admin_session, "prepared GET /ruuvi.json")
        expected = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: True,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: True,
        }
        for field, value in expected.items():
            self._require_setup(
                prepared.get(field) is value if isinstance(value, bool) else prepared.get(field) == value,
                f"prepared {field} has the required value",
                prepared.get(field),
            )
        self.prepared_hash = canonical_json_hash(prepared)
        self.evidence.write("PREPARED CONFIGURATION SHA256", self.prepared_hash)
        self.outcomes["temporary-state setup"] = "PASS"

    def _negative_matrix(self, realm: str) -> List[NegativeProbe]:
        basic = self.gateway.authorization_header_basic(ADMIN_USERNAME, self.config.gw_id)

        def digest(method: str, path: str) -> str:
            return self.gateway.authorization_header_digest(
                ADMIN_USERNAME,
                self.config.gw_id,
                method,
                path,
                {
                    "realm": realm,
                    "nonce": self.gateway.random_text(18),
                    "qop": "auth",
                    "opaque": self.gateway.random_text(18),
                },
            )

        probes: List[NegativeProbe] = []
        routes = (
            (HttpMethod.GET, GatewayApi.HISTORY, HttpStatus.C_302_FOUND, AuthMech.M2M_API_BEARER_RO),
            (HttpMethod.GET, GatewayApi.CONFIG, HttpStatus.C_302_FOUND, AuthMech.M2M_API_BEARER_RO),
            (
                HttpMethod.POST,
                GatewayApi.CONFIG,
                HttpStatus.C_401_UNAUTHORIZED,
                AuthMech.M2M_API_BEARER_RW,
            ),
        )
        for scheme in (HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            for method, path, expected, mechanism in routes:
                authorization = basic if scheme == HttpAuthScheme.BASIC else digest(method, path)
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
        password_bearer = f"{HttpAuthScheme.BEARER} {self.config.gw_id}"
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
        password_hash = self.gateway.calculate_digest_ha1(
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

    def _run_negative_matrix(self, realm: str) -> None:
        self._require_security(
            self.config.gw_id not in {self.ro_key, self.rw_key},
            "password-as-token probe differs from both configured keys",
        )
        for probe in self._negative_matrix(realm):
            response = self.gateway.request(
                self.gateway.new_session(),
                probe.method,
                probe.path,
                headers=(
                    {HttpHeader.AUTHORIZATION: probe.authorization}
                    if probe.authorization is not None
                    else None
                ),
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
        current = self._read_config(self.admin_session, "post-negative GET /ruuvi.json")
        current_hash = canonical_json_hash(current)
        self._require_security(
            current_hash == self.prepared_hash,
            "negative POST probes did not change configuration",
            HashComparisonEvidence(baseline=self.prepared_hash or "", final=current_hash),
        )

    def _positive_matrix(self) -> List[PositiveProbe]:
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

    def _run_positive_matrix(self) -> None:
        for probe in self._positive_matrix():
            response = self.gateway.request(
                self.gateway.new_session(),
                probe.method,
                probe.path,
                headers={
                    HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {probe.key}"
                },
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
        current = self._read_config(self.admin_session, "post-positive GET /ruuvi.json")
        current_hash = canonical_json_hash(current)
        self._require_security(
            current_hash == self.prepared_hash,
            "successful RW empty-object POST did not change configuration",
            HashComparisonEvidence(baseline=self.prepared_hash or "", final=current_hash),
        )
        self.outcomes[AuthMech.M2M_API_BEARER_RO] = "PASS"
        self.outcomes[AuthMech.M2M_API_BEARER_RW] = "PASS"

    def _restore(self) -> bool:
        if not self.mutation_possible:
            return True
        body = {
            GatewayCfgDesc.LAN_AUTH_API_KEY: "",
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
        }
        self.evidence.write("CONFIGURATION RESTORATION", body)
        attempts: List[RestorationAttempt] = []

        def attempt(name: str, session: Any, key: Optional[str] = None) -> bool:
            headers = (
                {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {key}"}
                if key is not None
                else None
            )
            try:
                response = self.gateway.request(
                    session,
                    HttpMethod.POST,
                    GatewayApi.CONFIG,
                    headers=headers,
                    json_body=body,
                )
                attempts.append(RestorationAttempt(name, response.status_code, None))
                return response.status_code == HttpStatus.C_200_OK
            except Exception as error:
                attempts.append(RestorationAttempt(name, None, f"{type(error).__name__}: {error}"))
                self.evidence.exception(error)
                return False

        restored = False
        if self.admin_session is not None:
            restored = attempt("authorized Admin session", self.admin_session)
        if not restored and self.rw_key is not None:
            restored = attempt("temporary RW bearer", self.gateway.new_session(), self.rw_key)
        if not restored:
            try:
                login = self.gateway.authenticate_interactive(ADMIN_USERNAME, self.config.gw_id)
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
            except Exception as error:
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
            restored_config = self._read_config(login.session, "restored GET /ruuvi.json")
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
            restored_hash = canonical_json_hash(restored_config)
            self._require_setup(
                restored_hash == self.baseline_hash,
                "restored configuration equals the baseline",
                HashComparisonEvidence(baseline=self.baseline_hash or "", final=restored_hash),
            )
            rejected = (
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
            for name, key, method, path in rejected:
                response = self.gateway.request(
                    self.gateway.new_session(),
                    method,
                    path,
                    headers={
                        HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {key or ''}"
                    },
                    json_body={} if method == HttpMethod.POST else None,
                )
                self._require_setup(
                    response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    f"temporary {name} key no longer authorizes",
                    response.status_code,
                )
        except Exception as error:
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
        verdict = "ERROR"
        exit_code = 2
        failure: Optional[BaseException] = None
        try:
            self.progress("Authenticating with the default administrative credentials")
            login = self._authenticate_admin()
            self.admin_session = login.session
            self.progress("Reading and validating the baseline gateway configuration")
            baseline = self._read_config(self.admin_session, "baseline GET /ruuvi.json")
            self._validate_baseline(baseline)
            self.baseline_hash = canonical_json_hash(baseline)
            self.evidence.write("BASELINE CONFIGURATION SHA256", self.baseline_hash)
            self.progress("Provisioning temporary RO and RW bearer keys")
            try:
                self._provision()
            except Exception:
                self.outcomes["temporary-state setup"] = "ERROR"
                raise
            self.progress("Verifying the prepared temporary state")
            self._verify_prepared()
            self.progress("Testing rejection of password schemes by M2M routes")
            self._run_negative_matrix(login.challenge["realm"])
            self.progress("Testing positive RO and RW bearer scope")
            self._run_positive_matrix()
        except SecurityFailure as error:
            failure = error
            self.evidence.exception(error)
            verdict = "FAIL"
            exit_code = 1
        except Exception as error:
            failure = error
            self.evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        finally:
            self.progress("Restoring the original API-key configuration")
            restoration_ok = self._restore()
            self.progress("Verifying restoration and aggregating the verdict")
            if not restoration_ok:
                self.factory_reset_required = True
                verdict = "ERROR"
                exit_code = 2
            elif failure is None:
                verdict = "PASS"
                exit_code = 0
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write(
                "FINAL RESULT",
                MechanismResultEvidence(mechanism=mechanism, result=outcome),
            )
        self.evidence.write("OVERALL RESULT", verdict)
        recovery_message = FACTORY_RESET_MESSAGE if self.factory_reset_required else None
        return RunResult(exit_code, verdict, dict(self.outcomes), set(self.coverage), recovery_message)


def execute_test_5_1_2a_2_b(
        work_dir: Optional[Path] = None,
        session_factory: Callable[[], Any] = requests.Session,
        now: Callable[[], datetime] = utc_now,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        output: Optional[Callable[[str], None]] = None,
) -> RunResult:
    if work_dir is None:
        work_dir = Path.cwd()
    if output is None:
        output = print
    log = EvidenceLog.create(work_dir / "logs", "test_5_1_2a_2_b", now)
    output(f"Open log file: {log.path}")

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress = ProgressReporter(output_progress, TOTAL_STEPS)
    result = RunResult(2, "ERROR", {mechanism: "NOT RUN" for mechanism in MECHANISMS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    try:
        try:
            progress.step("Loading and validating .env")
            config = load_dut_config(work_dir / ".env")
            log.write("DUT CONFIGURATION", config)
            result = FunctionalTest_5_1_2a_2_b(
                config,
                log,
                session_factory=session_factory,
                random_bytes=random_bytes,
                progress=progress.step,
            ).run()
        except Exception as error:
            log.exception(error)
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
