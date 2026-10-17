"""Live ETSI 5.5-4-2 Unit a denial-boundary test; requires a dedicated LAN gateway."""

from __future__ import annotations

import hashlib
import json
import secrets
import sys
from dataclasses import dataclass
from datetime import datetime
from functools import partial
from pathlib import Path
from typing import Any, Callable
from urllib.parse import SplitResult, urlsplit

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
from lib.evidence import AssertionEvidence, EvidenceLog, HashComparisonEvidence, MechanismResultEvidence, utc_now
from lib.gateway import (
    AuthMech,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthChallenge,
    InteractiveAuthResult,
    InteractiveChallengeRequest,
)
from lib.http_api import API_INVENTORY, ApiRoute, GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-4-2, Test Unit a"
SETUP: str = "factory-default setup"
UNAUTHENTICATED: str = AuthMech.LAN_WEBUI_UNAUTHENTICATED
INVALID: str = "invalid-credential denial"
PREPARED: str = "temporary-key setup"
INSUFFICIENT: str = AuthMech.M2M_API_BEARER_RO
LAN_FORBIDDEN: str = "LAN-forbidden 403 distinction"
FINAL: str = "final restoration and non-mutation"
CHECKS: tuple[str, ...] = (SETUP, UNAUTHENTICATED, INVALID, PREPARED, INSUFFICIENT, LAN_FORBIDDEN, FINAL)
READ_PATHS: tuple[str, ...] = (GatewayApi.CONFIG, GatewayApi.STATUS, GatewayApi.HISTORY)
TOTAL_STEPS: int = 12


class SecurityFailure(Exception):
    """An observed denial or non-mutation assertion failed."""


@dataclass(frozen=True)
class ProbeResult:
    check: str
    route: ApiRoute
    scheme: str
    expected_status: int
    observed_status: int
    result: str


def config_hash(payload: dict[str, Any]) -> str:
    encoded: bytes = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    return hashlib.sha256(encoded).hexdigest()


class RecoveryEvidence(EvidenceLog):
    """Forward recovery logging best effort without changing protocol or assertion behavior."""

    def __init__(self, evidence: EvidenceLog, report: Callable[[Callable[[], None]], None]) -> None:
        super().__init__(evidence.path, evidence._stream, evidence.started_at)
        self.original: EvidenceLog = evidence
        self.report: Callable[[Callable[[], None]], None] = report

    def write(self, label: str, value: Any = "") -> None:
        self.report(partial(self.original.write, label, value))

    def write_line(self, message: str) -> None:
        self.report(partial(self.original.write_line, message))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.report(partial(self.original.write_http_request, request))

    def write_http_response(self, response: requests.Response) -> None:
        self.report(partial(self.original.write_http_response, response))

    def exception(self, error: BaseException) -> None:
        self.report(partial(self.original.exception, error))

    def finish(self, verdict: str, now: Callable[[], datetime] = utc_now) -> None:
        self.report(partial(self.original.finish, verdict, now))


class FunctionalTest_5_5_4_2_a:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        progress: Callable[[str], None] = lambda message: None,
        output: Callable[[str], None] = print,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config, evidence, session_factory=session_factory, random_bytes=random_bytes,
            ecc_generate=ecc_generate, timeout=(5, 15), user_agent="ruuvi-etsi-test-5.5-4-2-a",
        )
        self.progress: Callable[[str], None] = progress
        self.output: Callable[[str], None] = output
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.active: str = SETUP
        self.coverage: set[ApiRoute] = set()
        self.baseline: dict[str, Any] | None = None
        self.mutation_possible: bool = False
        self.ro_key: str = ""
        self.rw_key: str = ""
        self.recovery_message: str | None = None
        self.reporting_failed: bool = False

    def _say(self, message: str) -> None:
        self.output(message)
        self.evidence.write_line(message)

    def _require(self, condition: bool, description: str, actual: Any = "", *, setup: bool = False) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", actual))
        if not condition:
            self.outcomes[self.active] = "ERROR" if setup else "FAIL"
            self._say(f"{self.active}: {'ERROR' if setup else 'FAIL'} — {description}")
            if setup:
                raise InvalidSetup(description)
            raise SecurityFailure(description)

    def _login(self, *, baseline: bool = False) -> InteractiveAuthResult:
        try:
            auth: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            if baseline:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        if baseline and auth.login_response.status_code != HttpStatus.C_200_OK:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(
            auth.login_response.status_code == HttpStatus.C_200_OK,
            "default Admin login must return 200", auth.login_response.status_code, setup=True,
        )
        # GatewayClient sends an explicit cookie for login; retain it for all subsequent requests.
        auth.session.cookies.clear()
        auth.session.cookies.set("RUUVISESSION", auth.cookie)
        return auth

    def _read_config(self, session: requests.Session) -> dict[str, Any]:
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._require(response.status_code == HttpStatus.C_200_OK,
                      "authenticated configuration read must return 200", response.status_code, setup=True)
        return self.gateway.response_json(response, "authenticated GET /ruuvi.json", dict)

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any]) -> None:
        verified: bool = False
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require(
                isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None
                and mac.upper() == self.config.gw_mac.upper(),
                "configuration MAC must match .env", mac, setup=True,
            )
            verified = True
        expected: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        field: str
        value: Any
        for field, value in expected.items():
            actual: Any = payload.get(field)
            valid: bool = type(actual) is type(value) and actual == value
            if not valid and verified:
                self.recovery_message = FACTORY_RESET_MESSAGE
            self._require(valid, f"{field} must have its factory-default type and value", actual, setup=True)
        mode: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if mode != GatewayCfgLanAuthType.DEFAULT and verified:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(type(mode) is str and mode == GatewayCfgLanAuthType.DEFAULT,
                      "login challenge must report default authentication", mode, setup=True)

    def _compare_config(self, actual: dict[str, Any], expected: dict[str, Any], *, setup: bool = False) -> None:
        # Canonical JSON also distinguishes bool/int and nested value types. Exclude no fields.
        self.evidence.write("CONFIGURATION SHA256", HashComparisonEvidence(config_hash(expected), config_hash(actual)))
        fields: set[str] = set(actual) | set(expected)
        changed: list[str] = [
            field for field in sorted(fields)
            if field not in actual or field not in expected
            or config_hash({field: actual[field]}) != config_hash({field: expected[field]})
        ]
        self.evidence.write("CHANGED CONFIGURATION FIELDS", changed)
        self._require(not changed, "full configuration must match the retained snapshot", changed, setup=setup)

    def _probe(
        self, method: str, path: str, expected: int, authorization: str = "",
        *, session: requests.Session | None = None, setup: bool = False,
    ) -> requests.Response:
        route: ApiRoute = next((item for item in API_INVENTORY if item.method == method and item.path == path),
                               ApiRoute(method, path))
        self._require(route in API_INVENTORY, f"required route {method} {path} must exist", setup=True)
        response: requests.Response = self.gateway.request(
            session if session is not None else self.gateway.new_session(), method, path,
            headers={HttpHeader.AUTHORIZATION: authorization} if authorization else None,
            json_body={} if method == HttpMethod.POST and path == GatewayApi.CONFIG else None,
        )
        passed: bool = response.status_code == expected
        self.evidence.write("PER-PROBE STATUS CLASSIFICATION", ProbeResult(
            self.active, route, authorization.split(" ", 1)[0] if authorization else "missing",
            expected, response.status_code, "PASS" if passed else "ERROR" if setup else "FAIL",
        ))
        self._require(passed, f"{method} {path}: expected HTTP {expected}, observed {response.status_code}",
                      setup=setup)
        self.coverage.add(route)
        return response

    def _auth_denial(self, session: requests.Session | None = None) -> None:
        probe_session: requests.Session = session if session is not None else self.gateway.new_session()
        response: requests.Response = self._probe(
            HttpMethod.GET, GatewayApi.AUTH, HttpStatus.C_401_UNAUTHORIZED, session=probe_session,
        )
        self.gateway.interactive_login_challenge_from_response(probe_session, response, "denied GET /auth")
        payload: dict[str, Any] = self.gateway.response_json(response, "denied GET /auth", dict)
        mode: Any = payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        self._require(type(mode) is str and mode == GatewayCfgLanAuthType.DEFAULT,
                      "denied GET /auth must report default interactive authentication", mode)

    def _auth_location_valid(self, value: str) -> bool:
        try:
            location: SplitResult = urlsplit(value)
            target: SplitResult = urlsplit(self.config.base_url)
            # generate_hostname() derives this alias from the verified nRF52 identity MAC.
            canonical: str = f"ruuvigateway{self.config.gw_mac.replace(':', '')[-4:]}".lower()
            trusted: set[str | None] = {target.hostname, canonical, f"{canonical}.local"}
            authority_valid: bool = not location.netloc or (
                location.hostname in trusted and (80 if location.port is None else location.port) == (target.port or 80)
                and location.username is None and location.password is None
            )
            return (location.path == "/" and location.fragment == "auth" and not location.query
                    and location.scheme in ("", "http") and authority_valid
                    and (not location.scheme or bool(location.netloc)))
        except ValueError:
            return False

    def _report(self, callback: Callable[[], None]) -> None:
        """Reporting must never interrupt recovery; incomplete evidence remains an ERROR."""
        try:
            callback()
        except Exception:  # noqa: BLE001 - Reporting failure must not prevent key removal or its retry.
            self.reporting_failed = True

    def _missing_reads(self) -> None:
        self.active = UNAUTHENTICATED
        self.progress("Checking unauthenticated reads and session denial")
        path: str
        for path in (GatewayApi.CONFIG, GatewayApi.HISTORY, GatewayApi.STATUS):
            response: requests.Response = self._probe(HttpMethod.GET, path, HttpStatus.C_302_FOUND)
            if path == GatewayApi.CONFIG:
                self._require(self._auth_location_valid(response.headers.get(HttpHeader.LOCATION, "")),
                              "302 Location must point to the gateway /#auth entry point")
            if path == GatewayApi.STATUS:
                self._require("RUUVI_PREV_URL" not in response.cookies
                              and "RUUVI_PREV_URL=" not in response.headers.get("Set-Cookie", ""),
                              "status redirect must omit the previous-URL cookie")
        self._auth_denial()
        self._probe(HttpMethod.DELETE, GatewayApi.AUTH, HttpStatus.C_401_UNAUTHORIZED)

    def _invalid_login(self) -> None:
        self.active = INVALID
        self.progress("Checking invalid interactive credentials")
        request: InteractiveChallengeRequest = self.gateway.prepare_interactive_challenge_request()
        challenge_response: requests.Response = self.gateway.send_interactive_challenge_request(request)
        self._require(challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                      f"invalid-login challenge: expected HTTP 401, observed {challenge_response.status_code}")
        challenge: InteractiveAuthChallenge = self.gateway.parse_interactive_challenge_response(
            request, challenge_response,
        )
        username: str = f"invalid-{self.gateway.random_text(9)}"
        password: str = self.gateway.random_text(24)
        self._require(password != self.config.gw_id, "invalid password must differ from default", setup=True)
        response: requests.Response = self.gateway.submit_interactive_authentication(challenge, username, password)
        self.evidence.write("PER-PROBE STATUS CLASSIFICATION", ProbeResult(
            INVALID, ApiRoute(HttpMethod.POST, GatewayApi.AUTH), "interactive",
            HttpStatus.C_401_UNAUTHORIZED, response.status_code,
            "PASS" if response.status_code == HttpStatus.C_401_UNAUTHORIZED else "FAIL",
        ))
        self._require(response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                      f"invalid login: expected HTTP 401, observed {response.status_code}")
        challenge.session.cookies.clear()
        challenge.session.cookies.set("RUUVISESSION", response.cookies.get("RUUVISESSION") or challenge.cookie)
        self._auth_denial(challenge.session)

    def _invalid_header(self, scheme: str, method: str, path: str) -> str:
        username: str = f"invalid-{self.gateway.random_text(9)}"
        password: str = self.gateway.random_text(24)
        if scheme == HttpAuthScheme.BASIC:
            return self.gateway.authorization_header_basic(username, password)
        return self.gateway.authorization_header_digest(username, password, method, path, {
            "realm": "Ruuvi Gateway", "nonce": self.gateway.random_text(12),
            "qop": "auth", "opaque": self.gateway.random_text(12),
        })

    def _baseline_probes(self, session: requests.Session, baseline: dict[str, Any]) -> None:
        self._missing_reads()
        self._invalid_login()
        scheme: str
        path: str
        for scheme in (HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            self.progress(f"Checking invalid {scheme} reads")
            for path in READ_PATHS:
                self._probe(HttpMethod.GET, path, HttpStatus.C_302_FOUND,
                            self._invalid_header(scheme, HttpMethod.GET, path))
        # Every safer phase across every baseline scheme completes before these writes.
        for scheme in ("", HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            self.active = INVALID if scheme else UNAUTHENTICATED
            self.progress(f"Checking {scheme or 'missing'} credentials on configuration POST")
            header: str = self._invalid_header(scheme, HttpMethod.POST, GatewayApi.CONFIG) if scheme else ""
            self._probe(HttpMethod.POST, GatewayApi.CONFIG, HttpStatus.C_401_UNAUTHORIZED, header)
            self._compare_config(self._read_config(session), baseline)
            if not scheme:
                self.outcomes[UNAUTHENTICATED] = "PASS"
        self.outcomes[INVALID] = "PASS"

    def _prepare_keys(self, session: requests.Session, baseline: dict[str, Any]) -> dict[str, Any]:
        self.active = PREPARED
        self.progress("Provisioning temporary RO/RW keys and checking both read controls")
        self.ro_key = self.gateway.random_text(32)
        self.rw_key = self.gateway.random_text(32)
        self._require(bool(self.ro_key) and bool(self.rw_key) and self.ro_key != self.rw_key,
                      "temporary keys must be nonempty and distinct", setup=True)
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_API_KEY: self.ro_key, GatewayCfgDesc.LAN_AUTH_API_KEY_RW: self.rw_key,
        }
        self.evidence.write("TEMPORARY KEY RECOVERY CANDIDATES", body)
        self.mutation_possible = True
        response: requests.Response = self.gateway.request(session, HttpMethod.POST, GatewayApi.CONFIG, json_body=body)
        self._require(response.status_code == HttpStatus.C_200_OK, "key provisioning must return 200",
                      response.status_code, setup=True)
        prepared: dict[str, Any] = self._read_config(session)
        expected: dict[str, Any] = dict(baseline)
        expected[GatewayCfgDesc.LAN_AUTH_API_KEY_USE] = True
        expected[GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE] = True
        self._compare_config(prepared, expected, setup=True)
        key: str
        for key in (self.ro_key, self.rw_key):
            self._probe(HttpMethod.GET, GatewayApi.HISTORY, HttpStatus.C_200_OK,
                        f"{HttpAuthScheme.BEARER} {key}", setup=True)
        self.outcomes[PREPARED] = "PASS"
        return prepared

    def _bearer_probes(self, session: requests.Session, prepared: dict[str, Any]) -> None:
        self.active = INSUFFICIENT
        self.progress("Checking RO-on-write denial and the RW LAN-forbidden distinction")
        self.evidence.write("ASSESSMENT DISCREPANCY",
                            "Self-assessment claims RO-on-write 403; authoritative firmware requires 401.")
        response: requests.Response = self._probe(HttpMethod.POST, GatewayApi.CONFIG, HttpStatus.C_401_UNAUTHORIZED,
                                                 f"{HttpAuthScheme.BEARER} {self.ro_key}")
        self.evidence.write(
            "ASSESSMENT DISCREPANCY",
            f"Self-assessment claims 403 for RO-on-write; firmware expects 401; observed {response.status_code}.",
        )
        self._compare_config(self._read_config(session), prepared)
        self.outcomes[INSUFFICIENT] = "PASS"
        self.active = LAN_FORBIDDEN
        # handle_req_post checks RW authentication before the LAN restriction. A verified RW key
        # must reach the 403 branch; 401 here is a failure, not an informational substitute.
        self._probe(HttpMethod.POST, GatewayApi.CONNECT_WPS, HttpStatus.C_403_FORBIDDEN,
                    f"{HttpAuthScheme.BEARER} {self.rw_key}")
        self._compare_config(self._read_config(session), prepared)
        self.outcomes[LAN_FORBIDDEN] = "PASS"

    def _restore_keys(self) -> None:
        attempt: int
        for attempt in (1, 2):
            self._report(partial(self.evidence.write, "KEY RESTORATION ATTEMPT", attempt))
            try:
                auth: InteractiveAuthResult = self._login()
                response: requests.Response = self.gateway.request(
                    auth.session, HttpMethod.POST, GatewayApi.CONFIG,
                    json_body={GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""},
                )
                if response.status_code != HttpStatus.C_200_OK:
                    raise InvalidSetup("key restoration did not return 200")
                return
            except Exception as error:  # Retry recovery even after a partially applied write.
                error: Exception
                self._report(partial(self.evidence.exception, error))
                self._report(partial(self._say, f"Restoration attempt {attempt}: {type(error).__name__}; see evidence"))
                if attempt == 2:
                    raise

    def _finalize(self, baseline: dict[str, Any]) -> bool:
        original: EvidenceLog = self.evidence
        client_original: EvidenceLog = self.gateway.evidence
        recovery: RecoveryEvidence = RecoveryEvidence(original, self._report)
        self.evidence = recovery
        self.gateway.evidence = recovery
        try:
            return self._finalize_with_recovery_evidence(baseline)
        finally:
            self.evidence = original
            self.gateway.evidence = client_original

    def _finalize_with_recovery_evidence(self, baseline: dict[str, Any]) -> bool:
        self.active = FINAL
        self._report(lambda: self.progress("Restoring keys and verifying complete configuration and token revocation"))
        failed: bool = False
        error: Exception
        if self.mutation_possible:
            try:
                self._restore_keys()
            except Exception as error:  # noqa: BLE001 - Still try independent final reads and revocation checks.
                self._report(partial(self.evidence.exception, error))
                failed = True
        try:
            auth: InteractiveAuthResult = self._login()
            self._compare_config(self._read_config(auth.session), baseline)
        except Exception as error:  # noqa: BLE001 - Preserve proven FAIL and continue independent revocation checks.
            self._report(partial(self.evidence.exception, error))
            failed = True
        if self.mutation_possible:
            key: str
            for key in (self.ro_key, self.rw_key):
                try:
                    self._probe(HttpMethod.GET, GatewayApi.HISTORY, HttpStatus.C_401_UNAUTHORIZED,
                                f"{HttpAuthScheme.BEARER} {key}")
                except Exception as error:  # noqa: BLE001 - Verify both saved candidates even when one fails.
                    self._report(partial(self.evidence.exception, error))
                    failed = True
        if failed or self.reporting_failed:
            if self.outcomes[FINAL] != "FAIL":
                self.outcomes[FINAL] = "ERROR"
            self.recovery_message = (
                f"Restoration could not be verified. Inspect {self.evidence.path} and recover the dedicated gateway. "
                f"{FACTORY_RESET_MESSAGE}"
            )
        else:
            self.outcomes[FINAL] = "PASS"
        return not (failed or self.reporting_failed)

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        self.evidence.write("DUT CONFIGURATION", self.config)
        try:
            self.progress("Authenticating and obtaining two stable factory-default configuration reads")
            auth: InteractiveAuthResult = self._login(baseline=True)
            baseline: dict[str, Any] = self._read_config(auth.session)
            self._validate_baseline(baseline, auth.auth_payload)
            self.baseline = baseline
            self._compare_config(self._read_config(auth.session), baseline, setup=True)
            self.outcomes[SETUP] = "PASS"
            self._baseline_probes(auth.session, baseline)
            prepared: dict[str, Any] = self._prepare_keys(auth.session, baseline)
            self._bearer_probes(auth.session, prepared)
            verdict = "PASS"
        except SecurityFailure as error:
            error: Exception
            self._report(partial(self.evidence.exception, error))
            verdict = "FAIL"
        except Exception as error:  # noqa: BLE001 - Convert unexpected failures to ERROR and always recover.
            self._report(partial(self.evidence.exception, error))
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            self._report(partial(self._say, f"{self.active}: ERROR — {type(error).__name__}; see evidence for details"))
        finally:
            if self.baseline is not None and not self._finalize(self.baseline):
                verdict = "ERROR"
        if verdict == "PASS" and any(value != "PASS" for value in self.outcomes.values()):
            verdict = "ERROR"
        check: str
        outcome: str
        for check, outcome in self.outcomes.items():
            self._report(partial(self.evidence.write, "FINAL RESULT", MechanismResultEvidence(check, outcome)))
            self._report(partial(self._say, f"{check}: {outcome}"))
        if self.reporting_failed:
            verdict = "ERROR"
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(self.coverage), self.recovery_message)


def execute_test_5_5_4_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    work_dir = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_5_4_2_a", now)

    def progress_output(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(progress_output, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    try:
        output(f"Open log file: {log.path}")
        log.write("TEST CASE AND UNIT", TEST_ID)
        log.write("UTC START", log.started_at.isoformat())
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        result = FunctionalTest_5_5_4_2_a(
            config, log, session_factory, random_bytes, ecc_generate, progress.step, output,
        ).run()
    except Exception as error:  # noqa: BLE001 - Setup failures return ERROR and still finalize evidence.
        error: Exception
        result.outcomes[SETUP] = "ERROR"
        setup_message: str = f"Setup ERROR — {type(error).__name__}; see evidence for details"
        report: Callable[[], None]
        for report in (
            partial(log.exception, error), partial(log.write, "FINAL RESULTS", result.outcomes),
            partial(output, setup_message), partial(log.write_line, setup_message),
        ):
            try:
                report()
            except Exception:  # noqa: BLE001 - Failed reporting must preserve ERROR and log finalization.
                result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
    finally:
        try:
            if result.recovery_message is not None:
                progress_output(result.recovery_message)
        except Exception:  # noqa: BLE001 - Failed terminal evidence cannot discard the recovery result.
            result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
        try:
            log.finish(result.verdict, now)
        except Exception:  # noqa: BLE001 - Full-disk/flush failures must return ERROR after recovery.
            result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_5_4_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
