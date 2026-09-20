"""Live ETSI 5.1-1-2 Unit B functional test for a dedicated Ruuvi Gateway."""

from __future__ import annotations

import hashlib
import json
import re
import secrets
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Callable

import requests
from Crypto.PublicKey import ECC

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
    EXPECTED_API_INVENTORY,
    ApiRoute,
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-1-2, Test Unit B"
ADMIN_USERNAME: str = "Admin"
CONNECT_TIMEOUT_SECONDS: int = 5
READ_TIMEOUT_SECONDS: int = 15
HTTP_TIMEOUT: tuple[int, int] = (CONNECT_TIMEOUT_SECONDS, READ_TIMEOUT_SECONDS)
USER_AGENT: str = "ruuvi-etsi-test-5.1-1-2-b"
TOTAL_STEPS: int = 15
SAFE_WRITE: str = "SAFE_WRITE"
DANGEROUS_WRITE: str = "DANGEROUS_WRITE"
FINAL_VERIFICATION_RECOVERY_MESSAGE: str = (
    "Final non-mutation verification failed or could not be completed. "
    "Inspect the evidence log and the physical DUT; recover the saved lab configuration "
    "if needed before running further tests. No automatic configuration restore was attempted."
)
MECHANISMS: tuple[str, ...] = (
    AuthMech.LAN_WEBUI_USER_DEFINED,
    AuthMech.LAN_WEBUI_BASIC,
    AuthMech.LAN_WEBUI_DIGEST,
    AuthMech.M2M_API_BEARER_RO,
    AuthMech.M2M_API_BEARER_RW,
    "complete HTTP API inventory coverage",
    "final non-mutation verification",
)


class SecurityFailure(Exception):
    """A security assertion failed."""


def normalize_mac(value: str) -> str:
    return value.replace(":", "").upper()


def canonical_json_hash(value: Any) -> str:
    encoded: bytes = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


class FunctionalTest_5_1_1_2_b:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        progress: Callable[[str], None] | None = None,
    ) -> None:
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
        self.progress: Callable[[str], None] = progress if progress is not None else lambda description: None
        self.outcomes: dict[str, str] = {mechanism: "NOT RUN" for mechanism in MECHANISMS}
        self.coverage: set[ApiRoute] = set()
        self.factory_reset_required: bool = False
        self._probe_routes: dict[str, list[ApiRoute]] = {
            HttpMethod.GET: [],
            SAFE_WRITE: [],
            DANGEROUS_WRITE: [],
        }
        route: ApiRoute
        for route in API_INVENTORY:
            if route.method == HttpMethod.GET:
                phase: str = HttpMethod.GET
            elif route.path == GatewayApi.AUTH:
                # These probes use fresh sessions and cannot change persistent configuration.
                phase = SAFE_WRITE
            else:
                # Empty payloads do not make state-changing handlers safe if auth is bypassed.
                phase = DANGEROUS_WRITE
            self._probe_routes[phase].append(route)

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
        self,
        condition: bool,
        description: str,
        actual: Any = "",
        mechanism: str | None = None,
    ) -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            if mechanism is not None:
                self.outcomes[mechanism] = "FAIL"
            raise SecurityFailure(f"{description} (actual: {actual!r})")

    def _assert_interactive_authentication(
        self,
        username: str,
        password: str,
        expect_success: bool,
    ) -> requests.Session:
        try:
            result: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
        except GatewayAuthenticationModeError:
            self.factory_reset_required = True
            raise
        response: requests.Response = result.challenge_response
        self._require_setup(
            response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            "GET /auth returns 401 challenge",
            response.status_code,
        )
        if result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) != GatewayCfgLanAuthType.DEFAULT:
            self.factory_reset_required = True
        self._require_setup(
            result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT,
            f"GET /auth reports {GatewayCfgLanAuthType.DEFAULT}",
            result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE),
        )
        self._require_setup(
            re.search(
                r"(?:^|,)\s*(?:Basic|Digest)(?:\s|$)",
                result.auth_header,
                re.IGNORECASE,
            )
            is None,
            "GET /auth does not advertise Basic or Digest",
            result.auth_header,
        )
        expected_status: int = HttpStatus.C_200_OK if expect_success else HttpStatus.C_401_UNAUTHORIZED
        if expect_success:
            if result.login_response.status_code != expected_status:
                self.factory_reset_required = True
            self._require_setup(
                result.login_response.status_code == expected_status,
                "default credentials authenticate",
                result.login_response.status_code,
            )
        else:
            self._require_security(
                result.login_response.status_code == expected_status,
                "random user-defined credentials are denied",
                result.login_response.status_code,
            )
            confirmation: requests.Response = self.gateway.request(
                result.session,
                HttpMethod.GET,
                GatewayApi.AUTH,
            )
            self._require_security(
                confirmation.status_code == HttpStatus.C_401_UNAUTHORIZED,
                "failed login did not authorize its session",
                confirmation.status_code,
            )
        return result.session

    def _check_inventory(self) -> None:
        inventory_set: set[ApiRoute] = set(API_INVENTORY)
        self._require_security(
            len(API_INVENTORY) == 26,
            "API inventory contains exactly 26 entries",
            len(API_INVENTORY),
            mechanism="complete HTTP API inventory coverage",
        )
        self._require_security(
            len(inventory_set) == len(API_INVENTORY),
            "API inventory contains no duplicates",
            len(inventory_set),
            mechanism="complete HTTP API inventory coverage",
        )
        self._require_security(
            inventory_set == EXPECTED_API_INVENTORY,
            "API inventory equals the canonical method/path matrix",
            mechanism="complete HTTP API inventory coverage",
        )

    def _validate_baseline(self, config_payload: dict[str, Any]) -> None:
        if GatewayCfgDesc.GW_MAC in config_payload:
            actual_mac: Any = config_payload[GatewayCfgDesc.GW_MAC]
            self._require_setup(isinstance(actual_mac, str), "gw_mac response field is a string")
            self._require_setup(
                OCTETS_6_RE.fullmatch(actual_mac) is not None,
                "gw_mac response field is a six-octet MAC",
                actual_mac,
            )
            self._require_setup(
                normalize_mac(actual_mac) == normalize_mac(self.config.gw_mac),
                "DUT gw_mac matches .env",
                actual_mac,
            )
        required: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        expected: Any
        field: str
        for field, expected in required.items():
            actual: Any = config_payload.get(field)
            is_default: bool = type(actual) is type(expected) and actual == expected
            if not is_default and GatewayCfgDesc.GW_MAC in config_payload:
                # Recommend a reset for a bad baseline only after confirming DUT identity.
                self.factory_reset_required = True
            self._require_setup(is_default, f"{field} has its factory-default value", actual)
        identity: dict[str, Any] = {
            key: config_payload[key]
            for key in (
                GatewayCfgDesc.GW_MAC,
                GatewayCfgDesc.FW_VER,
                GatewayCfgDesc.NRF52_FW_VER,
            )
            if key in config_payload
        }
        self.evidence.write("DUT VERSION AND IDENTITY", identity)

    def _probe_interactive_group(self, method_group: str, scheme: str | None) -> None:
        mechanism: str = (
            AuthMech.LAN_WEBUI_USER_DEFINED
            if scheme is None
            else (AuthMech.LAN_WEBUI_BASIC if scheme == HttpAuthScheme.BASIC else AuthMech.LAN_WEBUI_DIGEST)
        )
        try:
            route: ApiRoute
            for route in self._probe_routes[method_group]:
                method: str = route.method
                path: str = route.path
                if path == GatewayApi.AUTH and (scheme is not None or method != HttpMethod.DELETE):
                    continue
                headers: dict[str, str] = {}
                if scheme == HttpAuthScheme.BASIC:
                    headers[HttpHeader.AUTHORIZATION] = self.gateway.authorization_header_basic(
                        self.gateway.random_text(9),
                        self.gateway.random_text(18),
                    )
                elif scheme == HttpAuthScheme.DIGEST:
                    headers[HttpHeader.AUTHORIZATION] = self.gateway.authorization_header_digest(
                        self.gateway.random_text(9),
                        self.gateway.random_text(18),
                        method,
                        path,
                        {
                            "realm": self.gateway.random_text(9),
                            "nonce": self.gateway.random_text(12),
                            "qop": "auth",
                            "opaque": self.gateway.random_text(12),
                        },
                    )
                response: requests.Response = self.gateway.request(
                    self.gateway.new_session(),
                    method,
                    path,
                    headers=headers,
                    json_body={} if method == HttpMethod.POST else None,
                )
                expected: set[int] = (
                    {
                        HttpStatus.C_302_FOUND,
                        HttpStatus.C_401_UNAUTHORIZED,
                        HttpStatus.C_403_FORBIDDEN,
                    }
                    if method == HttpMethod.GET
                    else {
                        HttpStatus.C_401_UNAUTHORIZED,
                        HttpStatus.C_403_FORBIDDEN,
                    }
                )
                if path == GatewayApi.AUTH:
                    expected = {HttpStatus.C_401_UNAUTHORIZED}
                elif method == HttpMethod.GET and path == GatewayApi.INFO:
                    # The firmware serves info.json only through the hotspot interface.
                    expected.add(HttpStatus.C_404_NOT_FOUND)
                self._require_security(
                    response.status_code in expected,
                    f"{method} {path} rejects {scheme or 'missing'} credentials",
                    response.status_code,
                )
                self.evidence.write(
                    "PER-ROUTE RESULT",
                    RouteResultEvidence(
                        mechanism=mechanism,
                        method=method,
                        path=path,
                        result="PASS",
                    ),
                )
        except SecurityFailure:
            self.outcomes[mechanism] = "FAIL"
            self.evidence.write(
                "PER-MECHANISM RESULT",
                MechanismResultEvidence(mechanism=mechanism, result="FAIL"),
            )
            raise
        if method_group == DANGEROUS_WRITE:
            self.outcomes[mechanism] = "PASS"
            self.evidence.write(
                "PER-MECHANISM RESULT",
                MechanismResultEvidence(mechanism=mechanism, result="PASS"),
            )

    def _probe_bearer_group(self, method_group: str) -> None:
        route: ApiRoute
        for route in self._probe_routes[method_group]:
            method: str = route.method
            path: str = route.path
            mechanism: str = AuthMech.M2M_API_BEARER_RO if method == HttpMethod.GET else AuthMech.M2M_API_BEARER_RW
            token: str = self.gateway.random_text(32)
            self._require_security(token != self.config.gw_id, "random bearer differs from gw_id")
            response: requests.Response = self.gateway.request(
                self.gateway.new_session(),
                method,
                path,
                headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"},
                json_body={} if method == HttpMethod.POST else None,
            )
            try:
                self._require_security(
                    response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    f"{method} {path} rejects disabled bearer token with 401",
                    response.status_code,
                )
            except SecurityFailure:
                self.outcomes[mechanism] = "FAIL"
                self.evidence.write(
                    "PER-MECHANISM RESULT",
                    MechanismResultEvidence(mechanism=mechanism, result="FAIL"),
                )
                raise
            self.coverage.add(route)
            self.evidence.write(
                "PER-ROUTE RESULT",
                RouteResultEvidence(
                    mechanism=mechanism,
                    method=method,
                    path=path,
                    result="PASS",
                ),
            )
        if method_group == HttpMethod.GET:
            self.outcomes[AuthMech.M2M_API_BEARER_RO] = "PASS"
            self.evidence.write(
                "PER-MECHANISM RESULT",
                MechanismResultEvidence(
                    mechanism=AuthMech.M2M_API_BEARER_RO,
                    result="PASS",
                ),
            )
            return
        if method_group != DANGEROUS_WRITE:
            return
        self.outcomes[AuthMech.M2M_API_BEARER_RW] = "PASS"
        self.evidence.write(
            "PER-MECHANISM RESULT",
            MechanismResultEvidence(
                mechanism=AuthMech.M2M_API_BEARER_RW,
                result="PASS",
            ),
        )
        self._require_security(
            self.coverage == EXPECTED_API_INVENTORY,
            "bearer probes cover the complete API inventory",
            len(self.coverage),
            mechanism="complete HTTP API inventory coverage",
        )
        self.outcomes["complete HTTP API inventory coverage"] = "PASS"

    def _verify_final_config(
        self, final_session: requests.Session, baseline_fields: dict[str, Any], baseline_hash: str
    ) -> None:
        final_response: requests.Response = self.gateway.request(
            final_session,
            HttpMethod.GET,
            GatewayApi.CONFIG,
        )
        self._require_security(
            final_response.status_code == HttpStatus.C_200_OK,
            "final authenticated GET /ruuvi.json succeeds",
            final_response.status_code,
            mechanism="final non-mutation verification",
        )
        final_config: dict[str, Any] = self.gateway.response_json(
            final_response,
            "final GET /ruuvi.json",
            dict,
        )
        final_fields: dict[str, Any] = {key: final_config.get(key) for key in AUTHENTICATION_DEFAULT_FIELDS}
        self._require_security(
            final_fields == baseline_fields,
            "authentication baseline fields are unchanged",
            final_fields,
            mechanism="final non-mutation verification",
        )
        final_hash: str = canonical_json_hash(final_config)
        self._require_security(
            final_hash == baseline_hash,
            "full canonical configuration hash is unchanged",
            HashComparisonEvidence(baseline=baseline_hash, final=final_hash),
            mechanism="final non-mutation verification",
        )

    def _verify_final_status(self, final_session: requests.Session) -> None:
        status_response: requests.Response = self.gateway.request(
            final_session,
            HttpMethod.GET,
            GatewayApi.STATUS,
        )
        self._require_security(
            status_response.status_code == HttpStatus.C_200_OK,
            "gateway still answers authenticated GET /status.json",
            status_response.status_code,
            mechanism="final non-mutation verification",
        )

    def _verify_non_mutation(self, baseline_fields: dict[str, Any], baseline_hash: str) -> str:
        try:
            final_session: requests.Session = self._assert_interactive_authentication(
                ADMIN_USERNAME, self.config.gw_id, True
            )
        except Exception as error:  # noqa: BLE001 - Report inability to verify the DUT after probing.
            error: Exception
            self.evidence.exception(error)
            return "ERROR"

        result: str = "PASS"
        checks: tuple[Callable[[], None], ...] = (
            lambda: self._verify_final_config(final_session, baseline_fields, baseline_hash),
            lambda: self._verify_final_status(final_session),
        )
        check: Callable[[], None]
        for check in checks:
            try:
                check()
            except SecurityFailure as error:
                self.evidence.exception(error)
                if result != "ERROR":
                    result = "FAIL"
            except Exception as error:  # noqa: BLE001 - Continue independent final checks and retain both errors.
                self.evidence.exception(error)
                result = "ERROR"
        return result

    def run(self) -> RunResult:
        self.evidence.write("TEST CASE AND UNIT", TEST_ID)
        self.evidence.write("UTC START", format_utc(self.evidence.started_at))
        self.evidence.write(
            "DUT CONFIGURATION",
            self.config,
        )
        verdict: str = "ERROR"
        exit_code: int
        baseline_fields: dict[str, Any] = {}
        baseline_hash: str | None = None
        try:
            self.progress("Authenticating with the default administrative credentials")
            baseline_session: requests.Session = self._assert_interactive_authentication(
                ADMIN_USERNAME,
                self.config.gw_id,
                True,
            )
            self.progress("Reading and validating the baseline gateway configuration")
            baseline_response: requests.Response = self.gateway.request(
                baseline_session,
                HttpMethod.GET,
                GatewayApi.CONFIG,
            )
            self._require_setup(
                baseline_response.status_code == HttpStatus.C_200_OK,
                "authenticated baseline GET /ruuvi.json succeeds",
                baseline_response.status_code,
            )
            baseline: dict[str, Any] = self.gateway.response_json(
                baseline_response,
                "baseline GET /ruuvi.json",
                dict,
            )
            self._validate_baseline(baseline)
            baseline_fields = {key: baseline[key] for key in AUTHENTICATION_DEFAULT_FIELDS}
            baseline_hash = canonical_json_hash(baseline)
            self.evidence.write("VOLATILE CONFIGURATION FIELDS EXCLUDED", [])
            self.evidence.write("BASELINE AUTH FIELDS", baseline_fields)
            self.evidence.write("BASELINE CONFIGURATION SHA256", baseline_hash)
            self.progress("Validating the canonical HTTP API inventory")
            self._check_inventory()

            random_username: str = f"test-{self.gateway.random_text(9)}"
            random_password: str = self.gateway.random_text(24)
            self.progress("Testing rejection of unconfigured user-defined credentials")
            try:
                self._assert_interactive_authentication(
                    random_username,
                    random_password,
                    False,
                )
            except SecurityFailure:
                self.outcomes[AuthMech.LAN_WEBUI_USER_DEFINED] = "FAIL"
                raise
            self.progress("Testing unauthenticated access to read APIs")
            self._probe_interactive_group(HttpMethod.GET, None)
            self.progress("Testing Basic authentication against read APIs")
            self._probe_interactive_group(HttpMethod.GET, HttpAuthScheme.BASIC)
            self.progress("Testing Digest authentication against read APIs")
            self._probe_interactive_group(HttpMethod.GET, HttpAuthScheme.DIGEST)
            self.progress("Testing disabled bearer authentication against read APIs")
            self._probe_bearer_group(HttpMethod.GET)
            self.progress("Testing unauthenticated and bearer access to session write APIs")
            self._probe_interactive_group(SAFE_WRITE, None)
            self._probe_bearer_group(SAFE_WRITE)
            self.progress("Testing unauthenticated access to potentially mutating APIs")
            self._probe_interactive_group(DANGEROUS_WRITE, None)
            self.progress("Testing Basic authentication against potentially mutating APIs")
            self._probe_interactive_group(DANGEROUS_WRITE, HttpAuthScheme.BASIC)
            self.progress("Testing Digest authentication against potentially mutating APIs")
            self._probe_interactive_group(DANGEROUS_WRITE, HttpAuthScheme.DIGEST)
            self.progress("Testing disabled bearer authentication against potentially mutating APIs")
            self._probe_bearer_group(DANGEROUS_WRITE)

            verdict = "PASS"
            exit_code = 0
        except SecurityFailure as error:
            error: Exception
            self.evidence.exception(error)
            verdict = "FAIL"
            exit_code = 1
        except (InvalidSetup, OSError) as error:
            self.evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            self.evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        finally:
            # Abort additional probes, not the read-only checks of a validated baseline.
            if baseline_hash is not None:
                self.progress("Verifying the gateway configuration was not changed")
                final_result: str = self._verify_non_mutation(baseline_fields, baseline_hash)
                self.outcomes["final non-mutation verification"] = final_result
                if final_result == "ERROR":
                    verdict = "ERROR"
                    exit_code = 2
                elif final_result == "FAIL" and verdict != "ERROR":
                    verdict = "FAIL"
                    exit_code = 1
        if verdict == "PASS":
            nonpassing: dict[str, str] = {
                mechanism: self.outcomes.get(mechanism, "NOT RUN")
                for mechanism in MECHANISMS
                if self.outcomes.get(mechanism) != "PASS"
            }
            self._record_assertion("all required mechanism results are PASS", not nonpassing, nonpassing)
            if nonpassing:
                # Missing/incomplete results are ERROR, not evidence of a security failure.
                verdict = "FAIL" if all(value == "FAIL" for value in nonpassing.values()) else "ERROR"
                exit_code = 1 if verdict == "FAIL" else 2
        outcome: str
        mechanism: str
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write(
                "FINAL RESULT",
                MechanismResultEvidence(mechanism=mechanism, result=outcome),
            )
        self.evidence.write("OVERALL RESULT", verdict)
        recovery_message: str | None = FACTORY_RESET_MESSAGE if self.factory_reset_required else None
        if baseline_hash is not None and self.outcomes["final non-mutation verification"] != "PASS":
            recovery_message = (
                f"{recovery_message}\n{FINAL_VERIFICATION_RECOVERY_MESSAGE}"
                if recovery_message is not None
                else FINAL_VERIFICATION_RECOVERY_MESSAGE
            )
        return RunResult(
            exit_code,
            verdict,
            dict(self.outcomes),
            set(self.coverage),
            recovery_message,
        )


def execute_test_5_1_1_2_b(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] | None = None,
) -> RunResult:
    if work_dir is None:
        work_dir = Path.cwd()
    if output is None:
        output = print
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_1_1_2_b", now)
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
            result = FunctionalTest_5_1_1_2_b(
                config,
                log,
                session_factory=session_factory,
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
    return execute_test_5_1_1_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
