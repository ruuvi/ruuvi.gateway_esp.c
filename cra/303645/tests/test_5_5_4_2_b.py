"""Live ETSI 5.5-4-2 Unit b positive authorization test for a dedicated LAN gateway."""

from __future__ import annotations

import hashlib
import json
import re
import secrets
import sys
from dataclasses import dataclass
from datetime import datetime
from functools import partial
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
from lib.errors import GatewayAuthenticationModeError, GatewayProtocolError, InvalidSetup
from lib.evidence import AssertionEvidence, EvidenceLog, HashComparisonEvidence, MechanismResultEvidence, utc_now
from lib.gateway import AuthMech, GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import ApiRoute, GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-4-2, Test Unit b"
SETUP: str = "factory-default setup"
DEFAULT: str = AuthMech.LAN_WEBUI_DEFAULT
CUSTOM: str = AuthMech.LAN_WEBUI_USER_DEFINED
RO: str = AuthMech.M2M_API_BEARER_RO
RW: str = AuthMech.M2M_API_BEARER_RW
PREPARED: str = "temporary-state setup"
FINAL: str = "final restoration and non-mutation"
CHECKS: tuple[str, ...] = (SETUP, DEFAULT, PREPARED, CUSTOM, RO, RW, FINAL)
TOTAL_STEPS: int = 8


class SecurityFailure(Exception):
    """A positive authorization or configuration-preservation assertion failed."""


@dataclass(frozen=True)
class Credentials:
    username: str
    password: str
    ro_key: str
    rw_key: str


def config_hash(payload: dict[str, Any]) -> str:
    encoded: bytes = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    return hashlib.sha256(encoded).hexdigest()


class FunctionalTest_5_5_4_2_b:
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
            ecc_generate=ecc_generate, timeout=(5, 15), user_agent="ruuvi-etsi-test-5.5-4-2-b",
        )
        self.progress: Callable[[str], None] = progress
        self.output: Callable[[str], None] = output
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.active: str = SETUP
        self.coverage: set[ApiRoute] = set()
        self.baseline: dict[str, Any] | None = None
        self.credentials: Credentials | None = None
        self.mutation_possible: bool = False
        self.recovery_message: str | None = None
        self.reporting_failed: bool = False

    def _report(self, callback: Callable[[], None]) -> None:
        # Reporting must never stop credential recovery, including fallback attempts.
        try:
            callback()
        except Exception:  # noqa: BLE001 - Preserve ERROR while allowing independent recovery work.
            self.reporting_failed = True

    def _log_error(self, error: Exception) -> None:
        self._report(lambda: self.evidence.exception(error))

    def _say(self, message: str) -> None:
        self._report(lambda: self.output(message))
        self._report(lambda: self.evidence.write_line(message))

    def _require(self, condition: bool, description: str, actual: Any = "", *, setup: bool = False) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", actual))
        if not condition:
            self.outcomes[self.active] = "ERROR" if setup else "FAIL"
            self._say(f"{self.active}: {'ERROR' if setup else 'FAIL'} — {description}")
            if setup:
                raise InvalidSetup(description)
            raise SecurityFailure(description)

    def _login(self, username: str, password: str, *, baseline: bool = False) -> InteractiveAuthResult:
        try:
            auth: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
        except GatewayAuthenticationModeError:
            if baseline:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        status: int = auth.login_response.status_code
        if baseline and status != HttpStatus.C_200_OK:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(status == HttpStatus.C_200_OK,
                      f"interactive login: expected HTTP 200, observed {status}", setup=baseline)
        self.gateway.response_json(auth.login_response, "POST /auth", dict)
        auth.session.cookies.clear()
        auth.session.cookies.set("RUUVISESSION", auth.cookie)
        return auth

    def _request(
        self, method: str, path: str, *, session: requests.Session | None = None,
        token: str = "", body: dict[str, str] | None = None, expected: int = HttpStatus.C_200_OK,
        setup: bool = False,
    ) -> requests.Response:
        response: requests.Response = self.gateway.request(
            session if session is not None else self.gateway.new_session(), method, path,
            headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"} if token else None,
            json_body=body,
        )
        self._require(response.status_code == expected,
                      f"{method} {path}: expected HTTP {expected}, observed {response.status_code}", setup=setup)
        self.coverage.add(ApiRoute(method, path))
        return response

    def _read(
        self, path: str, *, session: requests.Session | None = None, token: str = "", setup: bool = False,
    ) -> dict[str, Any]:
        response: requests.Response = self._request(HttpMethod.GET, path, session=session, token=token, setup=setup)
        return self.gateway.response_json(response, f"GET {path}", dict)

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any]) -> None:
        verified: bool = False
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require(isinstance(mac, str) and OCTETS_6_RE.fullmatch(mac) is not None
                          and mac.upper() == self.config.gw_mac.upper(),
                          "configuration MAC must match .env", mac, setup=True)
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
        self._require(type(payload.get("ntp_use")) is bool, "ntp_use must be a boolean", setup=True)
        mode: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        valid = type(mode) is str and mode == GatewayCfgLanAuthType.DEFAULT
        if not valid and verified:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(valid, "login challenge must report default authentication", mode, setup=True)

    def _compare_config(self, actual: dict[str, Any], expected: dict[str, Any], *, setup: bool = False) -> None:
        # No exclusions: even unrelated omitted-field parser side effects must remain visible.
        self.evidence.write("CONFIGURATION SHA256", HashComparisonEvidence(config_hash(expected), config_hash(actual)))
        fields: set[str] = set(actual) | set(expected)
        changed: list[str] = [
            field for field in sorted(fields)
            if field not in actual or field not in expected
            or config_hash({field: actual[field]}) != config_hash({field: expected[field]})
        ]
        self.evidence.write("CHANGED CONFIGURATION FIELDS", changed)
        self._require(not changed, "full configuration must match the retained snapshot", changed, setup=setup)

    def _prepare(self, auth: InteractiveAuthResult, baseline: dict[str, Any]) -> dict[str, Any]:
        self.active = PREPARED
        self.progress("Provisioning temporary custom credentials and RO/RW keys")
        credentials: Credentials = Credentials(
            f"test-{self.gateway.random_text(9)}", self.gateway.random_text(24),
            self.gateway.random_text(32), self.gateway.random_text(32),
        )
        self._require(credentials.username != "Admin" and credentials.password != self.config.gw_id
                      and bool(credentials.ro_key) and bool(credentials.rw_key)
                      and credentials.ro_key != credentials.rw_key,
                      "temporary credentials must be distinct and nonempty", setup=True)
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
            GatewayCfgDesc.LAN_AUTH_USER: credentials.username,
            GatewayCfgDesc.LAN_AUTH_PASS: self.gateway.calculate_digest_ha1(
                credentials.username, auth.challenge["realm"], credentials.password,
            ),
            GatewayCfgDesc.LAN_AUTH_API_KEY: credentials.ro_key,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: credentials.rw_key,
        }
        # Retain both these candidates and the original Admin/gw_id before a write may apply.
        self.credentials = credentials
        self.evidence.write("TEMPORARY RECOVERY CREDENTIALS", credentials)
        self.mutation_possible = True
        self._request(HttpMethod.POST, GatewayApi.CONFIG, session=auth.session, body=body, setup=True)
        prepared: dict[str, Any] = self._read(GatewayApi.CONFIG, token=credentials.rw_key, setup=True)
        expected: dict[str, Any] = dict(baseline)
        expected.update({
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
            GatewayCfgDesc.LAN_AUTH_USER: credentials.username,
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: True,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: True,
        })
        self._compare_config(prepared, expected, setup=True)
        return prepared

    @staticmethod
    def _validate_history(payload: dict[str, Any], ntp_use: bool) -> None:
        # Validate the required history envelope; sensor values may vary throughout the run.
        data: Any = payload.get("data")
        if not isinstance(data, dict):
            raise GatewayProtocolError("GET /history must contain a data object")
        if (type(data.get("coordinates")) is not str
                or (ntp_use and type(data.get("timestamp")) is not int)
                or (not ntp_use and "timestamp" in data)
                or type(data.get("gw_mac")) is not str or OCTETS_6_RE.fullmatch(data["gw_mac"]) is None
                or type(data.get("tags")) is not dict):
            raise GatewayProtocolError("GET /history has missing or mistyped required data fields")
        record_clock: str = "timestamp" if ntp_use else "counter"
        tags: dict[str, Any] = data["tags"]
        mac: str
        tag: Any
        for mac, tag in tags.items():
            if (OCTETS_6_RE.fullmatch(mac) is None or not isinstance(tag, dict)
                    or type(tag.get("rssi")) is not int or type(tag.get(record_clock)) is not int
                    or type(tag.get("ble_chan")) is not int or tag.get("ble_phy") not in ("1M", "2M", "Coded")
                    or type(tag.get("data")) is not str or not tag["data"]
                    or re.fullmatch(r"(?:[0-9A-Fa-f]{2})+", tag["data"]) is None):
                raise GatewayProtocolError("GET /history has a malformed sensor record")

    def _positive_checks(self, prepared: dict[str, Any]) -> None:
        if self.credentials is None:
            raise InvalidSetup("temporary credentials unavailable")
        credentials: Credentials = self.credentials
        self.active = CUSTOM
        self.progress("Checking custom interactive login and configuration access")
        auth: InteractiveAuthResult = self._login(credentials.username, credentials.password)
        self._compare_config(self._read(GatewayApi.CONFIG, session=auth.session), prepared)
        self.outcomes[PREPARED] = "PASS"
        self.outcomes[CUSTOM] = "PASS"
        self.active = RO
        self.progress("Checking RO bearer history, status, and configuration reads")
        history: dict[str, Any] = self._read(GatewayApi.HISTORY, token=credentials.ro_key)
        self._validate_history(history, prepared["ntp_use"])
        self._read(GatewayApi.STATUS, token=credentials.ro_key)
        self._compare_config(self._read(GatewayApi.CONFIG, token=credentials.ro_key), prepared)
        self.outcomes[RO] = "PASS"
        self.active = RW
        self.progress("Checking RW bearer no-op POST and full configuration hash")
        before: dict[str, Any] = self._read(GatewayApi.CONFIG, token=credentials.rw_key)
        self._compare_config(before, prepared)
        self._request(HttpMethod.POST, GatewayApi.CONFIG, token=credentials.rw_key, body={})
        self._compare_config(self._read(GatewayApi.CONFIG, token=credentials.rw_key), before)
        self.outcomes[RW] = "PASS"

    def _restore(self, credentials: Credentials) -> None:
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_API_KEY: "", GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
        }
        candidate: str
        for candidate in ("RW bearer", "custom interactive", "default interactive"):
            self._report(partial(self.evidence.write, "RESTORATION ATTEMPT", candidate))
            try:
                session: requests.Session
                headers: dict[str, str] | None = None
                if candidate == "RW bearer":
                    session = self.gateway.new_session()
                    headers = {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {credentials.rw_key}"}
                else:
                    username: str = credentials.username if candidate == "custom interactive" else "Admin"
                    password: str = credentials.password if candidate == "custom interactive" else self.config.gw_id
                    # A denied recovery candidate is an attempted fallback, not a security assertion.
                    auth: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
                    if auth.login_response.status_code != HttpStatus.C_200_OK:
                        raise InvalidSetup(f"{candidate} recovery login denied")
                    session = auth.session
                    session.cookies.clear()
                    session.cookies.set("RUUVISESSION", auth.cookie)
                response: requests.Response = self.gateway.request(
                    session, HttpMethod.POST, GatewayApi.CONFIG, headers=headers, json_body=body,
                )
                if response.status_code != HttpStatus.C_200_OK:
                    raise InvalidSetup(f"{candidate} restoration returned HTTP {response.status_code}")
                return
            except Exception as error:  # noqa: BLE001 - Recovery must try every retained candidate.
                error: Exception
                self._log_error(error)
                self._say(f"{candidate} restoration: {type(error).__name__}; trying retained recovery credentials")
        raise InvalidSetup("all restoration credentials failed")

    def _finalize(self, baseline: dict[str, Any]) -> bool:
        self.active = FINAL
        self._report(lambda: self.progress(
            "Restoring default authentication and verifying full state and credential revocation",
        ))
        failed: bool = False
        credentials: Credentials | None = self.credentials
        error: Exception
        if self.mutation_possible and credentials is not None:
            try:
                self._restore(credentials)
            except Exception as error:  # noqa: BLE001 - Still collect independent final-state evidence.
                self._log_error(error)
                failed = True
        try:
            auth: InteractiveAuthResult = self._login("Admin", self.config.gw_id)
            self._compare_config(self._read(GatewayApi.CONFIG, session=auth.session), baseline)
        except Exception as error:  # noqa: BLE001 - Keep a proven FAIL and continue revocation checks.
            self._log_error(error)
            failed = True
        if self.mutation_possible and credentials is not None:
            try:
                revoked: InteractiveAuthResult = self.gateway.authenticate_interactive(
                    credentials.username, credentials.password,
                )
                status: int = revoked.login_response.status_code
                self._require(status == HttpStatus.C_401_UNAUTHORIZED,
                              f"removed custom login: expected HTTP 401, observed {status}")
                revoked.session.cookies.clear()
                revoked.session.cookies.set("RUUVISESSION",
                                            revoked.login_response.cookies.get("RUUVISESSION") or revoked.cookie)
                self._request(HttpMethod.GET, GatewayApi.CONFIG, session=revoked.session,
                              expected=HttpStatus.C_302_FOUND)
            except Exception as error:  # noqa: BLE001 - Each removed credential requires independent verification.
                self._log_error(error)
                failed = True
            key: str
            for key in (credentials.ro_key, credentials.rw_key):
                try:
                    self._request(HttpMethod.GET, GatewayApi.CONFIG, token=key,
                                  expected=HttpStatus.C_401_UNAUTHORIZED)
                except Exception as error:  # noqa: BLE001 - Continue checking the other removed credential.
                    self._log_error(error)
                    failed = True
        failed = failed or self.reporting_failed
        if failed:
            if self.outcomes[FINAL] != "FAIL":
                self.outcomes[FINAL] = "ERROR"
            self.recovery_message = (
                "USER ACTION REQUIRED: Restoration or cleanup reporting could not be verified. Temporary credentials or changed "
                "settings may remain; use the recovery credentials in the local evidence and your backup "
                "to restore this dedicated gateway. " + FACTORY_RESET_MESSAGE
            )
        else:
            self.outcomes[FINAL] = "PASS"
        return not failed

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        self.evidence.write("DUT IDENTITY", self.config)
        try:
            self.progress("Authenticating Admin and verifying the factory-default baseline")
            auth: InteractiveAuthResult = self._login("Admin", self.config.gw_id, baseline=True)
            baseline: dict[str, Any] = self._read(GatewayApi.CONFIG, session=auth.session, setup=True)
            self._validate_baseline(baseline, auth.auth_payload)
            self._compare_config(self._read(GatewayApi.CONFIG, session=auth.session, setup=True), baseline, setup=True)
            self.baseline = baseline
            self.outcomes[SETUP] = "PASS"
            self.active = DEFAULT
            self.progress("Checking default interactive configuration and status access")
            self._compare_config(self._read(GatewayApi.CONFIG, session=auth.session), baseline)
            self._read(GatewayApi.STATUS, session=auth.session)
            self.outcomes[DEFAULT] = "PASS"
            prepared: dict[str, Any] = self._prepare(auth, baseline)
            self._positive_checks(prepared)
            verdict = "PASS"
        except SecurityFailure as error:
            error: Exception
            self._log_error(error)
            if self.active == CUSTOM:
                self.outcomes[PREPARED] = "ERROR"
            verdict = "FAIL"
        except Exception as error:  # noqa: BLE001 - Unexpected exceptions remain ERROR and always trigger recovery.
            self._log_error(error)
            if self.active == CUSTOM and self.outcomes[PREPARED] != "PASS":
                self.outcomes[PREPARED] = "ERROR"
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            self._say(f"{self.active}: ERROR — {type(error).__name__}; see evidence for details")
        finally:
            if self.baseline is not None and not self._finalize(self.baseline):
                verdict = "ERROR"
        if verdict == "PASS" and any(value != "PASS" for value in self.outcomes.values()):
            verdict = "ERROR"
        check: str
        outcome: str
        for check, outcome in self.outcomes.items():
            self._report(partial(self.evidence.write, "FINAL RESULT", MechanismResultEvidence(check, outcome)))
            self._say(f"{check}: {outcome}")
        if self.reporting_failed:
            verdict = "ERROR"
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(self.coverage), self.recovery_message)


def execute_test_5_5_4_2_b(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    work_dir = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_5_4_2_b", now)
    output(f"Open log file: {log.path}")

    def progress_output(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(progress_output, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    def report(message: str, *, record: bool = True) -> None:
        try:
            output(message)
        except Exception:  # noqa: BLE001 - A closed terminal must not discard the run result.
            result.exit_code = 2
            result.verdict = "ERROR"
        try:
            if record:
                log.write_line(message)
        except Exception:  # noqa: BLE001 - Failed evidence must not discard the run result.
            result.exit_code = 2
            result.verdict = "ERROR"

    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", log.started_at.isoformat())
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        result = FunctionalTest_5_5_4_2_b(
            config, log, session_factory, random_bytes, ecc_generate, progress.step, output,
        ).run()
    except Exception as error:  # Setup errors must still create and finish evidence.
        error: Exception
        log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
        result.outcomes[SETUP] = "ERROR"
        log.write("FINAL RESULTS", result.outcomes)
        report(f"Setup ERROR — {type(error).__name__}; see evidence for details")
    finally:
        if result.recovery_message is not None:
            report(result.recovery_message)
        log.finish(result.verdict, now)
    report(f"Overall verdict: {result.verdict}", record=False)
    return result


def main() -> int:
    return execute_test_5_5_4_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
