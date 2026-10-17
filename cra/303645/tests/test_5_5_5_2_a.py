"""Live ETSI 5.5-5-2 Unit A configuration authorization test (dedicated LAN DUT)."""

from __future__ import annotations

import hashlib
import json
import secrets
import sys
from dataclasses import dataclass, replace
from datetime import datetime
from pathlib import Path
from typing import Any, Callable
from urllib.parse import SplitResult, urljoin, urlsplit

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
from lib.evidence import AssertionEvidence, EvidenceLog, MechanismResultEvidence, format_utc, utc_now
from lib.gateway import AuthMech, GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import API_INVENTORY, ApiRoute, GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 5.5-5-2, Test Unit A"
SETUP: str = "temporary-state setup"
RESTORE: str = "final restoration and non-mutation"
CHECKS: tuple[str, ...] = (
    AuthMech.LAN_WEBUI_DEFAULT, AuthMech.M2M_API_BEARER_RO, AuthMech.M2M_API_BEARER_RW, SETUP, RESTORE,
)
TOTAL_STEPS: int = 11
ROUTE_SELECTION: tuple[ApiRoute, ...] = (
    ApiRoute(HttpMethod.GET, GatewayApi.CONFIG), ApiRoute(HttpMethod.POST, GatewayApi.CONFIG),
    ApiRoute(HttpMethod.GET, GatewayApi.AUTH), ApiRoute(HttpMethod.POST, GatewayApi.AUTH),
)


class SecurityFailure(Exception):
    """An observed authorization or state assertion failed."""


@dataclass(frozen=True)
class CheckResult:
    name: str
    verdict: str = "NOT RUN"
    reason: str = "Required controls have not completed"


@dataclass(frozen=True)
class TemporaryState:
    ro_key: str
    rw_key: str
    unauthorized_marker: str
    ro_marker: str
    rw_marker: str


@dataclass(frozen=True)
class ConfigurationComparison:
    context: str
    expected_hash: str
    actual_hash: str
    changed_fields: tuple[str, ...]


def config_hash(payload: dict[str, Any]) -> str:
    encoded: bytes = json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()
    return hashlib.sha256(encoded).hexdigest()


class FunctionalTest_5_5_5_2_a:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog,
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
            ecc_generate=ecc_generate, timeout=(5, 15), user_agent="ruuvi-etsi-test-5.5-5-2-a",
        )
        self.progress: Callable[[str], None] = progress
        self.output: Callable[[str], None] = output
        self.results: tuple[CheckResult, ...] = tuple(CheckResult(name) for name in CHECKS)
        self.active: str = SETUP
        self.baseline: dict[str, Any] | None = None
        self.temporary: TemporaryState | None = None
        self.mutation_possible: bool = False
        self.keys_possible: bool = False
        self.recovery_message: str | None = None
        self.had_error: bool = False

    def _result(self, name: str, verdict: str, reason: str) -> None:
        self.results = tuple(
            replace(item, verdict=verdict, reason=reason) if item.name == name else item
            for item in self.results
        )
        self.evidence.write("CHECK", CheckResult(name, verdict, reason))

    def _require(self, condition: bool, description: str, *, setup: bool = False) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", condition))
        if not condition:
            if setup:
                if not any(item.name == self.active and item.verdict == "FAIL" for item in self.results):
                    self._result(self.active, "ERROR", description)
                raise InvalidSetup(description)
            self._result(self.active, "FAIL", description)
            raise SecurityFailure(description)

    def _login(self, *, baseline: bool = False) -> requests.Session:
        try:
            auth: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            if baseline:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        if auth.login_response.status_code != HttpStatus.C_200_OK and baseline:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(auth.login_response.status_code == HttpStatus.C_200_OK,
                      "Admin login must return 200", setup=True)
        # The baseline config identity is validated before interpreting its auth mode.
        if not baseline:
            self._require(auth.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT,
                          "Admin challenge must report default authentication", setup=True)
        return auth.session

    def _request(
        self, method: str, path: str, *, body: dict[str, str] | None = None,
        token: str | None = None, session: requests.Session | None = None,
    ) -> requests.Response:
        headers: dict[str, str] = {} if token is None else {
            HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}",
        }
        return self.gateway.request(
            session if session is not None else self.gateway.new_session(), method, path,
            headers=headers, json_body=body, allow_redirects=False,
        )

    def _read(self, context: str, *, session: requests.Session | None = None,
              token: str | None = None) -> dict[str, Any]:
        response: requests.Response = self._request(HttpMethod.GET, GatewayApi.CONFIG, session=session, token=token)
        self._require(response.status_code == HttpStatus.C_200_OK, f"{context}: config read must return 200", setup=True)
        payload: dict[str, Any] = self.gateway.response_json(response, context, dict)
        self.evidence.write(context, payload)
        return payload

    def _compare(self, expected: dict[str, Any], actual: dict[str, Any], context: str) -> bool:
        fields: list[str] = sorted(set(expected) | set(actual))
        changes: tuple[str, ...] = tuple(
            name for name in fields if name not in expected or name not in actual
            or json.dumps(expected[name], sort_keys=True) != json.dumps(actual[name], sort_keys=True)
        )
        comparison: ConfigurationComparison = ConfigurationComparison(
            context, config_hash(expected), config_hash(actual), changes,
        )
        self.evidence.write("FULL CONFIGURATION COMPARISON", comparison)
        return comparison.expected_hash == comparison.actual_hash

    def _validate_baseline(self, payload: dict[str, Any]) -> None:
        mac: Any = payload.get(GatewayCfgDesc.GW_MAC)
        if GatewayCfgDesc.GW_MAC in payload:
            self._require(isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None
                          and mac.upper() == self.config.gw_mac.upper(), "DUT MAC must match .env", setup=True)
        defaults: dict[str, Any] = default_config_values((
            *AUTHENTICATION_DEFAULT_FIELDS, GatewayCfgDesc.COORDINATES,
            GatewayCfgDesc.MQTT_DATA_FORMAT, GatewayCfgDesc.MQTT_PREFIX, GatewayCfgDesc.MQTT_CLIENT_ID,
        ))
        # gw_cfg_default_init() substitutes the nRF52 MAC into these generated defaults.
        # Partial POSTs reset all three omitted MQTT fields, even with use_mqtt disabled.
        name: str
        for name in (GatewayCfgDesc.MQTT_PREFIX, GatewayCfgDesc.MQTT_CLIENT_ID):
            defaults[name] = defaults[name].replace("00:00:00:00:00:00", self.config.gw_mac.upper())
        value: Any
        for name, value in defaults.items():
            actual: Any = payload.get(name)
            matches: bool = name in payload and type(actual) is type(value) and actual == value
            if not matches and mac is not None:
                self.recovery_message = FACTORY_RESET_MESSAGE
            self._require(matches, f"Baseline {name} must match the generated default by type and value", setup=True)

    def _prepare_values(self, original: str) -> TemporaryState:
        ro: str = self.gateway.random_text(32)
        rw: str = self.gateway.random_text(32)
        self._require(bool(ro) and bool(rw) and ro != rw, "Temporary keys must be distinct and nonempty", setup=True)
        prefix: str = f"cra-{self.gateway.random_text(8)}"
        state: TemporaryState = TemporaryState(ro, rw, f"{prefix}-U", f"{prefix}-RO", f"{prefix}-RW")
        self._require(original not in (state.unauthorized_marker, state.ro_marker, state.rw_marker),
                      "Markers must differ from original coordinates", setup=True)
        self.evidence.write("TEMPORARY KEYS AND MARKERS (LOCAL EVIDENCE ONLY)", state)
        return state

    def _negative_write(self, marker: str, expected: dict[str, Any], token: str | None = None) -> None:
        self.mutation_possible = True
        response: requests.Response = self._request(
            HttpMethod.POST, GatewayApi.CONFIG, body={GatewayCfgDesc.COORDINATES: marker}, token=token,
        )
        rejected: bool = response.status_code == HttpStatus.C_401_UNAUTHORIZED
        if token is not None:
            self.evidence.write("ASSESSMENT DISCREPANCY", f"Self-assessment claims 403; firmware expects 401; "
                                f"observed status {response.status_code}")
        if not rejected:
            self._result(self.active, "FAIL", f"Negative write expected 401; observed {response.status_code}")
        # This mandatory authenticated read also runs on unexpected 2xx; no further probes follow it.
        after: dict[str, Any] = self._read("Negative write post-read", session=self._login())
        unchanged: bool = self._compare(expected, after, "Rejected write preserves every field")
        self._require(unchanged, "Rejected write must preserve every configuration field")
        self._require(rejected, f"Negative write expected 401; observed {response.status_code}")
        self._result(self.active, "PASS", "Read controls and rejected write preserve the complete configuration")

    def _scenarios(self) -> None:
        self.progress("Authenticating Admin and checking two stable baseline reads")
        self._require(len(ROUTE_SELECTION) == 4 and len(set(ROUTE_SELECTION)) == 4
                      and set(ROUTE_SELECTION) <= set(API_INVENTORY), "Required routes belong to API inventory", setup=True)
        admin: requests.Session = self._login(baseline=True)
        first: dict[str, Any] = self._read("Initial baseline", session=admin)
        self._validate_baseline(first)
        second: dict[str, Any] = self._read("Stable baseline", session=admin)
        self._require(self._compare(first, second, "Baseline stability"), "Baseline must be stable", setup=True)
        self.baseline = first
        original: str = first[GatewayCfgDesc.COORDINATES]
        self.evidence.write("SELECTED REVERSIBLE FIELD", {GatewayCfgDesc.COORDINATES: original})
        temporary: TemporaryState = self._prepare_values(original)
        self.temporary = temporary

        self.active = AuthMech.LAN_WEBUI_DEFAULT
        self.progress("Checking unauthenticated WebUI reads")
        response: requests.Response = self._request(HttpMethod.GET, GatewayApi.CONFIG)
        self._require(response.status_code == HttpStatus.C_302_FOUND, "Unauthenticated config GET must return 302")
        location: str = response.headers.get(HttpHeader.LOCATION, "")
        destination: str = urljoin(self.config.base_url, location)
        target: SplitResult = urlsplit(destination)
        origin: SplitResult = urlsplit(self.config.base_url)
        target_port: int | None = target.port
        origin_port: int | None = origin.port
        if target_port is None:
            target_port = 80
        if origin_port is None:
            origin_port = 80
        self._require(bool(location) and target.path == "/" and target.fragment == "auth"
                      and target.query == "" and target.scheme == origin.scheme
                      and target.hostname == origin.hostname and target_port == origin_port
                      and target.username is None and target.password is None,
                      "Unauthenticated GET must redirect to /#auth on the configured DUT HTTP origin")
        response = self._request(HttpMethod.GET, GatewayApi.AUTH)
        self._require(response.status_code == HttpStatus.C_401_UNAUTHORIZED, "Unauthenticated GET /auth must return 401")
        self.gateway.parse_interactive_challenge(response.headers.get(HttpHeader.WWW_AUTHENTICATE, ""))
        payload: dict[str, Any] = self.gateway.response_json(response, "Unauthenticated GET /auth", dict)
        self._require(payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT,
                      "Unauthenticated challenge must report lan_auth_default")
        self.progress("Checking unauthenticated configuration write rejection")
        self._negative_write(temporary.unauthorized_marker, first)

        self.active = SETUP
        self.progress("Provisioning temporary RO/RW keys and verifying configuration")
        # Retain both credentials before sending: even a lost response may have applied them.
        self.keys_possible = True
        response = self._request(HttpMethod.POST, GatewayApi.CONFIG, session=self._login(), body={
            GatewayCfgDesc.LAN_AUTH_API_KEY: temporary.ro_key,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: temporary.rw_key,
        })
        self._require(response.status_code == HttpStatus.C_200_OK, "Temporary key provisioning must return 200", setup=True)
        prepared: dict[str, Any] = dict(first)
        prepared.update({GatewayCfgDesc.LAN_AUTH_API_KEY_USE: True, GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: True})
        actual: dict[str, Any] = self._read("Prepared state", session=self._login())
        self._require(self._compare(prepared, actual, "Key provisioning changes only key-use flags"),
                      "Invalid prepared state or provisioning side effect", setup=True)
        key: str
        role: str
        for role, key in (("RO", temporary.ro_key), ("RW", temporary.rw_key)):
            self.progress(f"Checking positive {role} bearer read before bearer writes")
            actual = self._read("Positive bearer read / pre-write state", token=key)
            self._require(self._compare(prepared, actual, "Positive bearer read matches prepared state"),
                          "Bearer read control must match the prepared configuration", setup=True)
        self._result(SETUP, "PASS", "Stable default baseline and both temporary bearer read controls verified")

        self.active = AuthMech.M2M_API_BEARER_RO
        self.progress("Checking RO bearer write rejection (firmware 401; assessment claims 403)")
        self._negative_write(temporary.ro_marker, prepared, temporary.ro_key)
        self.active = AuthMech.M2M_API_BEARER_RW
        self.progress("Checking RW bearer commit and absence of collateral changes")
        response = self._request(HttpMethod.POST, GatewayApi.CONFIG, token=temporary.rw_key,
                                 body={GatewayCfgDesc.COORDINATES: temporary.rw_marker})
        if response.status_code != HttpStatus.C_200_OK:
            self._result(self.active, "FAIL", f"RW write expected 200; observed {response.status_code}")
        actual = self._read("RW commit post-read", session=self._login())
        committed: dict[str, Any] = dict(prepared)
        committed[GatewayCfgDesc.COORDINATES] = temporary.rw_marker
        self._require(self._compare(committed, actual, "RW commit changes only coordinates"),
                      "RW write must commit only the intended coordinates change")
        self._require(response.status_code == HttpStatus.C_200_OK, "RW write must return 200")
        self._result(self.active, "PASS", "RW marker committed; every other configuration field preserved")

    def _restore_body(self, body: dict[str, str]) -> None:
        error: Exception
        if self.keys_possible and self.temporary is not None:
            try:
                self.evidence.write("RESTORATION ATTEMPT", "temporary RW bearer")
                response: requests.Response = self._request(
                    HttpMethod.POST, GatewayApi.CONFIG, body=body, token=self.temporary.rw_key,
                )
                if response.status_code == HttpStatus.C_200_OK:
                    return
                self.evidence.write("RESTORATION PRIMARY DENIED", response.status_code)
            except Exception as error:  # noqa: BLE001 - Recovery must continue after lost responses/unexpected errors.
                self.evidence.exception(error)
        self.evidence.write("RESTORATION ATTEMPT", "default Admin fallback")
        response = self._request(HttpMethod.POST, GatewayApi.CONFIG, body=body, session=self._login())
        self._require(response.status_code == HttpStatus.C_200_OK, "Admin restoration must return 200", setup=True)

    def _restore(self) -> None:
        if self.baseline is None:
            return
        self.active = RESTORE
        error: Exception
        if self.mutation_possible:
            # Observe all fields before restoring coordinates, including partially provisioned keys.
            before: dict[str, Any] | None = None
            try:
                before = self._read("Before restoration", session=self._login())
            except Exception as error:  # noqa: BLE001 - A failed read must not prevent either cleanup write.
                self.evidence.exception(error)
            try:
                self._restore_body({GatewayCfgDesc.COORDINATES: self.baseline[GatewayCfgDesc.COORDINATES]})
                after: dict[str, Any] = self._read("Coordinates restored", session=self._login())
                if before is not None:
                    before[GatewayCfgDesc.COORDINATES] = self.baseline[GatewayCfgDesc.COORDINATES]
                    if not self._compare(before, after, "Coordinate restoration has no collateral changes"):
                        self._result(RESTORE, "FAIL", "Coordinate restoration caused collateral changes")
            except Exception as error:  # noqa: BLE001 - Still disable keys and verify after coordinate cleanup failure.
                self.evidence.exception(error)
                self.had_error = True
            if self.keys_possible:
                try:
                    self._restore_body({GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: ""})
                except Exception as error:  # noqa: BLE001 - Always attempt final verification.
                    self.evidence.exception(error)
                    self.had_error = True
        final: dict[str, Any] = self._read("Final restored configuration", session=self._login())
        matches: bool = self._compare(self.baseline, final, "Final baseline SHA256 non-mutation proof")
        if not matches:
            self._result(RESTORE, "FAIL", "Final full configuration differs from baseline; see changed fields in evidence")
        revoked: bool = True
        if self.keys_possible and self.temporary is not None:
            token: str
            for token in (self.temporary.ro_key, self.temporary.rw_key):
                response: requests.Response = self._request(HttpMethod.GET, GatewayApi.CONFIG, token=token)
                if HttpStatus.C_200_OK <= response.status_code < HttpStatus.C_300_MULTIPLE_CHOICES:
                    revoked = False
                    self._result(RESTORE, "FAIL", "Temporary bearer still authorizes after cleanup")
                else:
                    self._require(response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                                  "Revoked bearer check must return 401; restoration is unverified", setup=True)
        if matches and revoked and not any(item.name == RESTORE and item.verdict == "FAIL" for item in self.results):
            self._result(RESTORE, "PASS", "Original full configuration verified; temporary keys revoked")
        else:
            self.recovery_message = f"USER ACTION REQUIRED: DUT restoration differs from baseline; inspect {self.evidence.path}"

    def run(self) -> RunResult:
        error: Exception
        try:
            self._scenarios()
        except SecurityFailure as error:
            self.evidence.exception(error)
        except Exception as error:  # noqa: BLE001 - Preserve evidence and restore on every runtime failure.
            self.evidence.exception(error)
            self.had_error = True
            if not any(item.name == self.active and item.verdict in ("FAIL", "ERROR") for item in self.results):
                self._result(self.active, "ERROR", f"{type(error).__name__}; see evidence")
        finally:
            self.progress("Restoring original coordinates and removing temporary keys")
            try:
                self._restore()
            except Exception as error:  # noqa: BLE001 - Unverified recovery is always ERROR.
                self.evidence.exception(error)
                self.had_error = True
                if not any(item.name == RESTORE and item.verdict == "FAIL" for item in self.results):
                    self._result(RESTORE, "ERROR", "Restoration could not be verified")
            if self.had_error and self.mutation_possible and not any(
                item.name == RESTORE and item.verdict == "PASS" for item in self.results
            ):
                self.recovery_message = f"USER ACTION REQUIRED: Check DUT recovery and configuration in {self.evidence.path}"
        self.progress("Recording per-check outcomes and final verdict")
        item: CheckResult
        for item in self.results:
            self.evidence.write("FINAL RESULT", MechanismResultEvidence(item.name, item.verdict))
            self.output(f"{item.name}: {item.verdict} — {item.reason}")
        verdict: str = "PASS"
        exit_code: int = 0
        if self.had_error:
            verdict, exit_code = "ERROR", 2
        elif any(item.verdict == "FAIL" for item in self.results):
            verdict, exit_code = "FAIL", 1
        elif any(item.verdict != "PASS" for item in self.results):
            verdict, exit_code = "ERROR", 2
        return RunResult(exit_code, verdict, {item.name: item.verdict for item in self.results}, set(), self.recovery_message)


def execute_test_5_5_5_2_a(
    work_dir: Path | None = None, session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now, random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate, output: Callable[[str], None] = print,
) -> RunResult:
    work_dir = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_5_5_2_a", now)
    output(f"Open log file: {log.path}")

    def report(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {name: "NOT RUN" for name in CHECKS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        log.write("DUT IDENTITY", config)
        result = FunctionalTest_5_5_5_2_a(
            config, log, session_factory, random_bytes, ecc_generate, progress.step, report,
        ).run()
    except Exception as error:  # Even initialization failures must finish the evidence log.
        error: Exception
        log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
        report(f"Setup ERROR: {type(error).__name__}; see evidence log")
        result.outcomes[SETUP] = "ERROR"
    finally:
        if result.recovery_message is not None:
            report(result.recovery_message)
        log.write("ALL CHECK OUTCOMES", result.outcomes)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_5_5_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
