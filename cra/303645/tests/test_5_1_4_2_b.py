"""Live ETSI 5.1-4-2 Unit B: immediate invalidation and independent key rotation."""

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
from lib.gateway import AuthMech, GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import API_INVENTORY, ApiRoute, GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-4-2, Test Unit B"
OUT_OF_SCOPE: str = (
    "Basic/Digest (pending documentation); Hotspot-Provisioning; Unauthenticated; Disabled; "
    "Unit A's pure commit/active-value and regeneration obligations."
)
SETUP: str = "setup"
RESTORATION: str = "final restoration"
MECHANISMS: tuple[str, ...] = (
    AuthMech.LAN_WEBUI_DEFAULT,
    AuthMech.LAN_WEBUI_USER_DEFINED,
    AuthMech.M2M_API_BEARER_RO,
    AuthMech.M2M_API_BEARER_RW,
)
TOTAL_STEPS: int = 8
RESTORE_BODY: dict[str, str] = {
    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
    GatewayCfgDesc.LAN_AUTH_API_KEY: "",
    GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
}


@dataclass(frozen=True)
class Credential:
    username: str
    plaintext: str
    ha1: str
    realm: str


@dataclass(frozen=True)
class BearerKeys:
    ro: str
    rw: str


class SecurityFailure(Exception):
    """An observed security assertion failed; restoration is still mandatory."""


class RecoveryEvidence(EvidenceLog):
    """Best-effort evidence only during mandatory recovery and final reporting."""

    def __init__(self, source: EvidenceLog) -> None:
        super().__init__(source.path, source._stream, source.started_at)
        self.source: EvidenceLog = source
        self.failed: bool = False

    def attempt(self, operation: Callable[[], None]) -> None:
        try:
            operation()
        except Exception:  # noqa: BLE001 - Evidence/output failures must never block recovery.
            self.failed = True

    def write(self, label: str, value: Any = "") -> None:
        self.attempt(lambda: self.source.write(label, value))

    def exception(self, error: BaseException) -> None:
        self.attempt(lambda: self.source.exception(error))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.attempt(lambda: self.source.write_http_request(request))

    def write_http_response(self, response: requests.Response) -> None:
        self.attempt(lambda: self.source.write_http_response(response))


def configuration_hash(payload: dict[str, Any]) -> str:
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    ).hexdigest()


class FunctionalTest_5_1_4_2_b:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        progress: Callable[[str], None] | None = None,
        output: Callable[[str], None] | None = None,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config, evidence, session_factory, random_bytes, ecc_generate, (5, 15), "ruuvi-etsi-test-5.1-4-2-b"
        )
        self.progress: Callable[[str], None] = progress or (lambda message: None)
        self.output: Callable[[str], None] = output or (lambda message: None)
        self.diagnostics: list[str] = []
        self.last_check: str = "initial setup"
        self.outcomes: dict[str, str] = {name: "NOT RUN" for name in (SETUP, *MECHANISMS, RESTORATION)}
        self.active: str = SETUP
        self.last_mutator: str = SETUP
        self.coverage: set[ApiRoute] = set()
        self.baseline: dict[str, Any] | None = None
        self.expected: dict[str, Any] = {}
        self.last_login: InteractiveAuthResult | None = None
        self.credentials: list[Credential] = []
        self.keys: list[BearerKeys] = []
        self.mutation_possible: bool = False
        self.recovery_message: str | None = None

    def _check(self, condition: bool, description: str, actual: Any = "", *, setup: bool = False) -> None:
        self.last_check = description
        if not condition:
            self.outcomes[SETUP if setup else self.active] = "ERROR" if setup else "FAIL"
            observed: str = str(actual) if isinstance(actual, (int, bool)) else "value mismatch (details in log)"
            self.diagnostics.append(f"{self.active}: expected {description}; observed {observed}")
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", actual))
        if not condition:
            if setup:
                raise InvalidSetup(description)
            raise SecurityFailure(description)

    def _request(
        self, session: requests.Session, method: str, path: str,
        *, headers: dict[str, str] | None = None, body: dict[str, str] | None = None,
    ) -> requests.Response:
        self.last_check = f"{method} {path}"
        route: ApiRoute = ApiRoute(method, path)
        if route not in API_INVENTORY:
            raise InvalidSetup(f"non-canonical route: {method} {path}")
        response: requests.Response = self.gateway.request(
            session, method, path, headers=headers, json_body=body, allow_redirects=False
        )
        self.coverage.add(route)
        return response

    def _login(self, username: str, password: str, *, setup: bool = False) -> InteractiveAuthResult:
        self.last_check = "interactive login"
        try:
            result: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
        except Exception:  # Setup includes required positive login controls; the run boundary logs the error.
            if setup:
                self.outcomes[SETUP] = "ERROR"
            raise
        self.coverage.update((ApiRoute(HttpMethod.GET, GatewayApi.AUTH), ApiRoute(HttpMethod.POST, GatewayApi.AUTH)))
        self.evidence.write("INTERACTIVE SESSION COOKIE", result.cookie)
        return result

    def _read_config(self, login: InteractiveAuthResult, *, setup: bool = False) -> dict[str, Any]:
        try:
            response: requests.Response = self._request(login.session, HttpMethod.GET, GatewayApi.CONFIG)
            self._check(response.status_code == HttpStatus.C_200_OK, "authorized configuration read",
                        response.status_code, setup=setup)
            return self.gateway.response_json(response, "GET /ruuvi.json", dict)
        except Exception:  # Prepared-state read-back belongs to setup even for an unexpected exception.
            if setup:
                self.outcomes[SETUP] = "ERROR"
            raise

    def _compare_config(self, actual: dict[str, Any], expected: dict[str, Any], *, setup: bool = False) -> None:
        # Canonical JSON retains JSON types, including the bool/int equality traps, recursively.
        expected_hash: str = configuration_hash(expected)
        actual_hash: str = configuration_hash(actual)
        self._check(actual_hash == expected_hash, "complete configuration matches expected state",
                    HashComparisonEvidence(expected_hash, actual_hash), setup=setup)

    def _validate_baseline(self, payload: dict[str, Any]) -> None:
        verified: bool = False
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._check(isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None,
                        "exposed MAC is well formed", mac, setup=True)
            self._check(mac.upper() == self.config.gw_mac.upper(), "DUT MAC matches .env", mac, setup=True)
            verified = True
        field: str
        expected: Any
        for field, expected in default_config_values(AUTHENTICATION_DEFAULT_FIELDS).items():
            actual: Any = payload.get(field)
            valid: bool = type(actual) is type(expected) and actual == expected
            if not valid and verified:
                self.recovery_message = FACTORY_RESET_MESSAGE
            self._check(valid, f"factory-default {field}", actual, setup=True)

    def _baseline(self) -> InteractiveAuthResult:
        self.progress("Validating factory-default authentication and two stable configuration reads")
        try:
            login: InteractiveAuthResult = self._login("Admin", self.config.gw_id, setup=True)
        except GatewayAuthenticationModeError:
            self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        if login.login_response.status_code != HttpStatus.C_200_OK:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._check(login.login_response.status_code == HttpStatus.C_200_OK, "default login accepted",
                    login.login_response.status_code, setup=True)
        payload: dict[str, Any] = self._read_config(login, setup=True)
        self._validate_baseline(payload)
        mode: Any = login.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if mode != GatewayCfgLanAuthType.DEFAULT and GatewayCfgDesc.GW_MAC in payload:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._check(type(mode) is str and mode == GatewayCfgLanAuthType.DEFAULT,
                    "default interactive challenge mode", mode, setup=True)
        self._compare_config(self._read_config(login, setup=True), payload, setup=True)
        self.baseline = payload
        self.expected = dict(payload)
        self.last_login = login
        self.evidence.write("BASELINE CONFIGURATION", payload)
        self.evidence.write("BASELINE SHA256", configuration_hash(payload))
        self.outcomes[SETUP] = "PASS"
        return login

    def _secret(self, excluded: list[str], size: int = 32) -> str:
        attempt: int
        for attempt in range(100):
            candidate: str = self.gateway.random_text(size)
            if candidate not in excluded:
                return candidate
        raise InvalidSetup("random source could not generate a distinct value")

    def _credential(self, realm: str) -> Credential:
        users: list[str] = ["Admin", *[item.username for item in self.credentials]]
        passwords: list[str] = [self.config.gw_id, *[item.plaintext for item in self.credentials]]
        username: str = self._secret(users, 12)
        password: str = self._secret(passwords)
        credential: Credential = Credential(
            username, password, self.gateway.calculate_digest_ha1(username, realm, password), realm
        )
        self.evidence.write("GENERATED INTERACTIVE CREDENTIAL", credential)
        return credential

    def _change(self, login: InteractiveAuthResult, body: dict[str, str]) -> None:
        self.evidence.write("CONFIGURATION TRANSITION", body)
        self.mutation_possible = True
        self.last_mutator = self.active
        try:
            response: requests.Response = self._request(login.session, HttpMethod.POST, GatewayApi.CONFIG, body=body)
        except Exception:  # A possibly applied provisioning request requires setup ERROR and recovery.
            self.outcomes[SETUP] = "ERROR"
            raise
        self._check(response.status_code == HttpStatus.C_200_OK, "configuration provisioning accepted",
                    response.status_code, setup=True)

    def _replay(self, login: InteractiveAuthResult, *, dropped: bool) -> None:
        self.evidence.write("REPLAY CAPTURED COOKIE", login.cookie)
        response: requests.Response = self._request(
            login.session, HttpMethod.GET, GatewayApi.CONFIG,
            headers={HttpHeader.COOKIE: f"RUUVISESSION={login.cookie}"},
        )
        allowed: tuple[int, ...] = (
            (HttpStatus.C_401_UNAUTHORIZED, HttpStatus.C_302_FOUND) if dropped else (HttpStatus.C_200_OK,)
        )
        self._check(response.status_code in allowed,
                    "session dropped" if dropped else "interactive session survives API-key-only change",
                    response.status_code)

    def _interactive_change(
        self, old_login: InteractiveAuthResult, old_user: str, old_password: str, mechanism: str,
    ) -> InteractiveAuthResult:
        self.active = mechanism
        self.progress(f"Checking immediate invalidation and session drop: {mechanism}")
        credential: Credential = self._credential(old_login.challenge["realm"])
        # Retain both old and candidate credentials even if the POST applies but its response is lost.
        self.credentials.append(credential)
        self._change(old_login, {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
            GatewayCfgDesc.LAN_AUTH_USER: credential.username,
            GatewayCfgDesc.LAN_AUTH_PASS: credential.ha1,
        })
        self._replay(old_login, dropped=True)
        old: InteractiveAuthResult = self._login(old_user, old_password)
        self._check(old.login_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    "old interactive value rejected", old.login_response.status_code)
        new: InteractiveAuthResult = self._login(credential.username, credential.plaintext, setup=True)
        self._check(new.login_response.status_code == HttpStatus.C_200_OK,
                    "new interactive value accepted", new.login_response.status_code)
        self.last_login = new
        self.expected.update({GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
                              GatewayCfgDesc.LAN_AUTH_USER: credential.username})
        self._compare_config(self._read_config(new, setup=True), self.expected)
        self.outcomes[mechanism] = "PASS"
        return new

    def _bearer_read(self, token: str, expected_status: int, description: str) -> None:
        response: requests.Response = self._request(
            self.gateway.new_session(), HttpMethod.GET, GatewayApi.HISTORY,
            headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"},
        )
        self._check(response.status_code == expected_status, description, response.status_code)

    def _bearer_write(self, login: InteractiveAuthResult, token: str, status: int, description: str) -> None:
        # Every authorization-only POST, even rejection or a lost response, gets a full hash check.
        self.last_mutator = self.active
        pending: Exception | None = None
        try:
            response: requests.Response = self._request(
                self.gateway.new_session(), HttpMethod.POST, GatewayApi.CONFIG,
                headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"}, body={},
            )
            self._check(response.status_code == status, description, response.status_code)
        except Exception as error:  # Preserve a lost POST reply while still measuring possible side effects.
            error: Exception
            pending = error
            raise
        finally:
            try:
                self._compare_config(self._read_config(login, setup=True), self.expected)
            except SecurityFailure:
                if pending is not None and not isinstance(pending, SecurityFailure):
                    raise pending
                raise

    def _key_change(self, login: InteractiveAuthResult, body: dict[str, str]) -> None:
        self._change(login, body)
        self._replay(login, dropped=False)
        self._compare_config(self._read_config(login, setup=True), self.expected)

    def _bearers(self, login: InteractiveAuthResult) -> None:
        self.active = AuthMech.M2M_API_BEARER_RO
        self.progress("Provisioning RO and RW keys and checking the surviving interactive session")
        ro1: str = self._secret([self.config.gw_id])
        rw1: str = self._secret([self.config.gw_id, ro1])
        first: BearerKeys = BearerKeys(ro1, rw1)
        self.keys.append(first)
        self.evidence.write("INITIAL BEARER KEYS", first)
        self.expected.update({GatewayCfgDesc.LAN_AUTH_API_KEY_USE: True, GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: True})
        self._key_change(login, {GatewayCfgDesc.LAN_AUTH_API_KEY: ro1, GatewayCfgDesc.LAN_AUTH_API_KEY_RW: rw1})
        self._bearer_read(ro1, HttpStatus.C_200_OK, "initial RO key accepted")
        self._bearer_read(rw1, HttpStatus.C_200_OK, "initial RW key read accepted")
        self._bearer_write(login, rw1, HttpStatus.C_200_OK, "initial RW key accepted")

        self.progress("Checking independent RO rotation: old rejected, new accepted, RW unaffected")
        ro2: str = self._secret([self.config.gw_id, ro1, rw1])
        self.keys.append(BearerKeys(ro2, rw1))
        self.evidence.write("RO ROTATION KEYS", self.keys[-1])
        self._key_change(login, {GatewayCfgDesc.LAN_AUTH_API_KEY: ro2})
        self._bearer_read(ro1, HttpStatus.C_401_UNAUTHORIZED, "old RO key rejected")
        self._bearer_read(ro2, HttpStatus.C_200_OK, "new RO key accepted")
        self._bearer_read(rw1, HttpStatus.C_200_OK, "other RW key still reads")
        self._bearer_write(login, rw1, HttpStatus.C_200_OK, "other RW key unaffected by RO rotation")
        self.outcomes[AuthMech.M2M_API_BEARER_RO] = "PASS"

        self.active = AuthMech.M2M_API_BEARER_RW
        self.progress("Checking independent RW rotation: old rejected, new accepted, RO scope preserved")
        rw2: str = self._secret([self.config.gw_id, ro1, rw1, ro2])
        self.keys.append(BearerKeys(ro2, rw2))
        self.evidence.write("RW ROTATION KEYS", self.keys[-1])
        self._key_change(login, {GatewayCfgDesc.LAN_AUTH_API_KEY_RW: rw2})
        self._bearer_read(ro2, HttpStatus.C_200_OK, "other RO key unaffected by RW rotation")
        self._bearer_read(rw2, HttpStatus.C_200_OK, "new RW key read accepted")
        self._bearer_read(rw1, HttpStatus.C_401_UNAUTHORIZED, "old RW key read rejected")
        self._bearer_write(login, rw1, HttpStatus.C_401_UNAUTHORIZED, "old RW key rejected")
        self._bearer_write(login, rw2, HttpStatus.C_200_OK, "new RW key accepted")
        self._bearer_write(login, ro2, HttpStatus.C_401_UNAUTHORIZED, "other RO key remains scope-confined")
        self.outcomes[AuthMech.M2M_API_BEARER_RW] = "PASS"

    def _restore_post(self, login: InteractiveAuthResult) -> bool:
        response: requests.Response = self._request(
            login.session, HttpMethod.POST, GatewayApi.CONFIG, body=RESTORE_BODY
        )
        self.evidence.write("RESTORATION POST STATUS", response.status_code)
        return response.status_code == HttpStatus.C_200_OK

    def _restore(self) -> None:
        if not self.mutation_possible:
            return
        self.evidence.write("RESTORATION BODY", RESTORE_BODY)
        restored: bool = False
        error: Exception
        if self.last_login is not None:
            try:
                self.evidence.write("RESTORATION ATTEMPT", "most recently authorized session")
                restored = self._restore_post(self.last_login)
            except Exception as error:  # noqa: BLE001 - Recovery must continue after any failed attempt.
                self.evidence.exception(error)
        candidate: Credential
        candidates: list[Credential] = list(reversed(self.credentials))
        candidates.append(Credential("Admin", self.config.gw_id, "", ""))
        for candidate in candidates:
            if restored:
                break
            try:
                self.evidence.write("RESTORATION CREDENTIAL ATTEMPT", candidate)
                login: InteractiveAuthResult = self._login(candidate.username, candidate.plaintext)
                if login.login_response.status_code == HttpStatus.C_200_OK:
                    restored = self._restore_post(login)
            except Exception as error:  # noqa: BLE001 - Try every retained credential before declaring recovery ERROR.
                self.evidence.exception(error)
        self._check(restored, "restoration POST accepted", setup=True)
        login = self._login("Admin", self.config.gw_id)
        self._check(login.login_response.status_code == HttpStatus.C_200_OK,
                    "restored default login accepted", login.login_response.status_code, setup=True)
        self.last_login = login
        if self.baseline is None:
            raise InvalidSetup("no baseline available for restoration verification")
        self.expected = dict(self.baseline)
        self.active = self.last_mutator
        self._compare_config(self._read_config(login, setup=True), self.baseline)
        # All possibly applied values are revoked, including those from a partially completed run.
        for candidate in self.credentials:
            self.active = (
                AuthMech.LAN_WEBUI_DEFAULT if candidate == self.credentials[0] else AuthMech.LAN_WEBUI_USER_DEFINED
            )
            old: InteractiveAuthResult = self._login(candidate.username, candidate.plaintext)
            self._check(old.login_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                        "temporary interactive value revoked", old.login_response.status_code)
        ro_keys: list[str] = list(dict.fromkeys(pair.ro for pair in self.keys))
        rw_keys: list[str] = list(dict.fromkeys(pair.rw for pair in self.keys))
        token: str
        for token in ro_keys:
            self.active = AuthMech.M2M_API_BEARER_RO
            self._bearer_read(token, HttpStatus.C_401_UNAUTHORIZED, "temporary RO key revoked")
        for token in rw_keys:
            self.active = AuthMech.M2M_API_BEARER_RW
            self._bearer_read(token, HttpStatus.C_401_UNAUTHORIZED, "temporary RW key read revoked")
        for token in rw_keys:
            self.active = AuthMech.M2M_API_BEARER_RW
            self._bearer_write(login, token, HttpStatus.C_401_UNAUTHORIZED, "temporary RW key revoked")
        self.outcomes[RESTORATION] = "PASS"
        self.active = RESTORATION

    def run(self) -> RunResult:
        recovery_evidence: RecoveryEvidence = RecoveryEvidence(self.evidence)
        verdict: str = "ERROR"
        exit_code: int = 2
        self.evidence.write("TEST CASE AND UNIT", TEST_ID)
        self.evidence.write("OUT OF SCOPE", OUT_OF_SCOPE)
        self.evidence.write("SESSION SEMANTICS", "credential changes clear sessions; API-key-only changes preserve sessions")
        error: Exception
        try:
            default: InteractiveAuthResult = self._baseline()
            first: InteractiveAuthResult = self._interactive_change(
                default, "Admin", self.config.gw_id, AuthMech.LAN_WEBUI_DEFAULT
            )
            credential: Credential = self.credentials[-1]
            second: InteractiveAuthResult = self._interactive_change(
                first, credential.username, credential.plaintext, AuthMech.LAN_WEBUI_USER_DEFINED
            )
            self._bearers(second)
            verdict, exit_code = "PASS", 0
        except SecurityFailure as error:
            recovery_evidence.exception(error)
            verdict, exit_code = "FAIL", 1
        except Exception as error:  # noqa: BLE001 - Unexpected failures are evidence-bearing ERRORs with recovery.
            self.diagnostics.append(
                f"{self.active}: ERROR during {self.last_check}: {type(error).__name__} (details in log)"
            )
            recovery_evidence.exception(error)
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            if isinstance(error, InvalidSetup):
                self.outcomes[SETUP] = "ERROR"
        finally:
            self.evidence = recovery_evidence
            self.gateway.evidence = recovery_evidence
            recovery_evidence.attempt(
                lambda: self.progress("Restoring defaults and verifying the full baseline and temporary-value revocation")
            )
            self.active = RESTORATION
            try:
                self._restore()
            except Exception as error:  # noqa: BLE001 - Unverified restoration always overrides the overall verdict.
                recovery_evidence.exception(error)
                self.diagnostics.append(
                    f"final restoration: ERROR during {self.last_check}: {type(error).__name__} (details in log)"
                )
                self.outcomes[RESTORATION] = "ERROR"
                self.recovery_message = f"Restoration is unverified; inspect {self.evidence.path}. {FACTORY_RESET_MESSAGE}"
                verdict, exit_code = "ERROR", 2
        if verdict == "PASS" and any(value != "PASS" for value in self.outcomes.values()):
            verdict, exit_code = "ERROR", 2
        name: str
        outcome: str
        for name, outcome in self.outcomes.items():
            self.evidence.write("FINAL RESULT", MechanismResultEvidence(name, outcome))
        diagnostic: str
        for diagnostic in self.diagnostics:
            self.evidence.write("RESULT EXPLANATION", diagnostic)
            recovery_evidence.attempt(lambda message=diagnostic: self.output(message))
        for name, outcome in self.outcomes.items():
            if outcome == "NOT RUN":
                self.evidence.write("RESULT EXPLANATION", f"{name}: NOT RUN")
                recovery_evidence.attempt(lambda component=name: self.output(f"{component}: NOT RUN"))
        if recovery_evidence.failed:
            verdict, exit_code = "ERROR", 2
            self.evidence.write("RESULT EXPLANATION", "Reporting: ERROR; output/evidence sink failed")
            recovery_evidence.attempt(lambda: self.output("Reporting: ERROR; output/evidence sink failed"))
        if self.recovery_message:
            self.evidence.write("USER ACTION REQUIRED", self.recovery_message)
        return RunResult(exit_code, verdict, dict(self.outcomes), set(self.coverage), self.recovery_message)


def execute_test_5_1_4_2_b(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] | None = None,
) -> RunResult:
    work_dir, output = work_dir or Path.cwd(), output or print

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
    initial_outcomes: dict[str, str] = {name: "NOT RUN" for name in (*MECHANISMS, RESTORATION)}
    initial_outcomes[SETUP] = "ERROR"
    result: RunResult = RunResult(2, "ERROR", initial_outcomes, set())
    try:
        log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_1_4_2_b", now)
    except Exception as error:  # noqa: BLE001 - Log creation failure must stop before any DUT request.
        error: Exception
        try:
            output(f"Evidence creation: ERROR: {type(error).__name__} (log unavailable)")
            output("Overall verdict: ERROR")
        except Exception:  # noqa: BLE001 - A failed output sink must not escape the execution wrapper.
            return result
        return result
    final_evidence: RecoveryEvidence = RecoveryEvidence(log)
    try:
        output(f"Open log file: {log.path}")
        log.write("TEST CASE AND UNIT", TEST_ID)
        log.write("OUT OF SCOPE", OUT_OF_SCOPE)
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        log.write("DUT CONFIGURATION", config)
        result = FunctionalTest_5_1_4_2_b(
            config, log, session_factory, random_bytes, progress=progress.step, output=output
        ).run()
    except Exception as error:  # noqa: BLE001 - Setup failures remain ERROR even if reporting fails.
        error: Exception
        final_evidence.exception(error)
        summary: str = f"Setup/execution: ERROR: {type(error).__name__} (details in log)"
        final_evidence.write("RESULT EXPLANATION", summary)
        final_evidence.attempt(lambda: output(summary))
        name: str
        outcome: str
        for name, outcome in result.outcomes.items():
            final_evidence.write("FINAL RESULT", MechanismResultEvidence(name, outcome))
            if outcome == "NOT RUN":
                final_evidence.write("RESULT EXPLANATION", f"{name}: NOT RUN")
                final_evidence.attempt(lambda component=name: output(f"{component}: NOT RUN"))
    finally:
        final_evidence.attempt(lambda: log.finish(result.verdict, now))
        if not log._stream.closed:
            final_evidence.attempt(log._stream.close)
        if final_evidence.failed:
            result.exit_code, result.verdict = 2, "ERROR"
    final_evidence.attempt(lambda: output(f"Overall verdict: {result.verdict}"))
    if result.recovery_message:
        final_evidence.attempt(lambda: output(str(result.recovery_message)))
    if final_evidence.failed:
        result.exit_code, result.verdict = 2, "ERROR"
    return result


def main() -> int:
    return execute_test_5_1_4_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
