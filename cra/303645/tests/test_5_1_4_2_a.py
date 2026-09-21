"""Live ETSI 5.1-4-2 Unit A authentication-value change functional test."""

from __future__ import annotations

import hashlib
import json
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
from lib.http_api import API_INVENTORY, ApiRoute, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-4-2, Test Unit A"
ADMIN_USERNAME: str = "Admin"
HTTP_TIMEOUT: tuple[int, int] = (5, 15)
USER_AGENT: str = "ruuvi-etsi-test-5.1-4-2-a"
TOTAL_STEPS: int = 8
MECHANISMS: tuple[str, ...] = (
    AuthMech.LAN_WEBUI_DEFAULT,
    AuthMech.LAN_WEBUI_USER_DEFINED,
    AuthMech.M2M_API_BEARER_RO,
    AuthMech.M2M_API_BEARER_RW,
    "final restoration",
)


class SecurityFailure(Exception):
    """A functional security assertion failed."""


def canonical_json_hash(value: Any) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    ).hexdigest()


class FunctionalTest_5_1_4_2_a:
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
            config, evidence, session_factory, random_bytes, ecc_generate, HTTP_TIMEOUT, USER_AGENT
        )
        self.progress: Callable[[str], None] = progress if progress is not None else lambda description: None
        self.outcomes: dict[str, str] = {mechanism: "NOT RUN" for mechanism in MECHANISMS}
        self.coverage: set[ApiRoute] = set()
        self.baseline_hash: str | None = None
        self.mutation_possible: bool = False
        self.factory_reset_required: bool = False
        self.last_username: str | None = None
        self.last_password: str | None = None
        self.last_session: requests.Session | None = None
        self.active_mechanisms: tuple[str, ...] = ()
        self.recovery_credentials: list[tuple[str, str]] = []

    def _record_assertion(self, description: str, passed: bool, actual: Any = "") -> None:
        self.evidence.write(
            "ASSERTION",
            AssertionEvidence(description, "PASS" if passed else "FAIL", actual),
        )

    def _require_setup(self, condition: bool, description: str, actual: Any = "") -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            raise InvalidSetup(f"{description} (actual: {actual!r})")

    def _require_security(self, condition: bool, description: str, actual: Any = "") -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            raise SecurityFailure(f"{description} (actual: {actual!r})")

    def _require(self, condition: bool, description: str, actual: Any, *, setup: bool) -> None:
        if setup:
            self._require_setup(condition, description, actual)
        else:
            self._require_security(condition, description, actual)

    def _secret(self, *excluded: str) -> str:
        _: int
        for _ in range(100):
            candidate: str = self.gateway.random_text(32)
            if candidate not in excluded:
                return candidate
        raise InvalidSetup("random generator did not produce a distinct secret")

    def _username(self, *excluded: str) -> str:
        _: int
        for _ in range(100):
            candidate: str = "etsi-" + self.gateway.random_text(12)
            if candidate not in excluded and candidate != ADMIN_USERNAME:
                return candidate
        raise InvalidSetup("random generator did not produce a distinct username")

    def _login(
        self, username: str, password: str, expected_mode: str, setup: bool, *, check_mode: bool = True
    ) -> InteractiveAuthResult:
        try:
            result: InteractiveAuthResult = self.gateway.authenticate_interactive(username, password)
        except GatewayAuthenticationModeError:
            if setup and expected_mode == GatewayCfgLanAuthType.DEFAULT:
                self.factory_reset_required = True
            raise
        actual_mode: Any = result.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if (
            setup
            and expected_mode == GatewayCfgLanAuthType.DEFAULT
            and result.login_response.status_code != HttpStatus.C_200_OK
        ):
            self.factory_reset_required = True
        self._require(
            result.challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
            "GET /auth returns an interactive challenge",
            result.challenge_response.status_code,
            setup=setup,
        )
        if check_mode:
            self._require(
                actual_mode == expected_mode,
                f"GET /auth reports {expected_mode}",
                actual_mode,
                setup=setup,
            )
        self._require(
            result.login_response.status_code == HttpStatus.C_200_OK,
            f"{username} authenticates successfully",
            result.login_response.status_code,
            setup=setup,
        )
        return result

    def _read_config(self, session: requests.Session | None, context: str, setup: bool = False) -> dict[str, Any]:
        if session is None:
            raise InvalidSetup("administrative session is not initialized")
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._require(response.status_code == HttpStatus.C_200_OK, context, response.status_code, setup=setup)
        return self.gateway.response_json(response, context, dict)

    def _post_config(
        self, session: requests.Session, body: dict[str, str], bearer: str | None = None
    ) -> requests.Response:
        headers: None | dict[str, str] = (
            None if bearer is None else {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {bearer}"}
        )
        self.evidence.write("CONFIGURATION TRANSITION", body)
        response: requests.Response = self.gateway.request(
            session, HttpMethod.POST, GatewayApi.CONFIG, headers=headers, json_body=body
        )
        self.coverage.add(ApiRoute(HttpMethod.POST, GatewayApi.CONFIG))
        return response

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any] | None = None) -> None:
        route: ApiRoute
        for route in (
            ApiRoute(HttpMethod.GET, GatewayApi.HISTORY),
            ApiRoute(HttpMethod.POST, GatewayApi.CONFIG),
        ):
            self._require_setup(route in API_INVENTORY, f"{route.method} {route.path} is canonical")
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require_setup(isinstance(mac, str), "gw_mac response field is a string", mac)
            self._require_setup(OCTETS_6_RE.fullmatch(mac) is not None, "gw_mac is a MAC", mac)
            self._require_setup(
                mac.replace(":", "").upper() == self.config.gw_mac.replace(":", "").upper(),
                "DUT gw_mac matches .env",
                mac,
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
        expected: Any
        field: str
        for field, expected in default_config_values(AUTHENTICATION_DEFAULT_FIELDS).items():
            actual: Any = payload.get(field)
            if type(actual) is not type(expected) or actual != expected:
                self.factory_reset_required = GatewayCfgDesc.GW_MAC in payload
            self._require_setup(
                type(actual) is type(expected) and actual == expected,
                f"{field} has its factory-default value",
                actual,
            )
        self.evidence.write(
            "DUT IDENTITY",
            {field: payload[field] for field in (GatewayCfgDesc.GW_MAC, GatewayCfgDesc.FW_VER) if field in payload},
        )

    def _verify_bearer(self, ro_key: str, rw_key: str, expected_hash: str, final: bool = False) -> None:
        self.active_mechanisms = (AuthMech.M2M_API_BEARER_RO,)
        ro_response: requests.Response = self.gateway.request(
            self.gateway.new_session(),
            HttpMethod.GET,
            GatewayApi.HISTORY,
            headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {ro_key}"},
        )
        self.coverage.add(ApiRoute(HttpMethod.GET, GatewayApi.HISTORY))
        self._require_security(
            ro_response.status_code == HttpStatus.C_200_OK,
            "new RO key authorizes GET /history",
            ro_response.status_code,
        )
        if final:
            self.outcomes[AuthMech.M2M_API_BEARER_RO] = "PASS"
        self.active_mechanisms = (AuthMech.M2M_API_BEARER_RW,)
        rw_response: requests.Response = self._post_config(self.gateway.new_session(), {}, rw_key)
        self._require_security(
            rw_response.status_code == HttpStatus.C_200_OK,
            "new RW key authorizes no-op POST /ruuvi.json",
            rw_response.status_code,
        )
        config: dict[str, Any] = self._read_config(self.last_session, "configuration after no-op RW POST")
        actual_hash: str = canonical_json_hash(config)
        self._require_security(
            actual_hash == expected_hash,
            "no-op RW POST preserves configuration hash",
            HashComparisonEvidence(expected_hash, actual_hash),
        )

    def _restore(self) -> bool:
        if not self.mutation_possible:
            self.outcomes["final restoration"] = "PASS"
            return True
        body: dict[str, str] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.DEFAULT,
            GatewayCfgDesc.LAN_AUTH_API_KEY: "",
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "",
        }
        self.evidence.write("CONFIGURATION RESTORATION", body)
        restored: bool = False
        attempts: list[dict[str, str | int]] = []
        if self.last_session is not None:
            try:
                status: int = self._post_config(self.last_session, body).status_code
                attempts.append({"method": "last authorized session", "status": status})
                restored = status == HttpStatus.C_200_OK
            except Exception as error:  # noqa: BLE001 - Continue recovery after a failed session attempt.
                error: Exception
                attempts.append({"method": "last authorized session", "error": type(error).__name__})
                self.evidence.exception(error)
        password: str | None
        user: str | None
        name: str
        credentials: list[tuple[str, str, str]] = [
            ("candidate custom credential", username, secret)
            for username, secret in reversed(self.recovery_credentials)
        ]
        credentials.append(("default credential", ADMIN_USERNAME, self.config.gw_id))
        for name, user, password in credentials:
            if restored or user is None or password is None:
                continue
            try:
                login: InteractiveAuthResult = self.gateway.authenticate_interactive(user, password)
                status = login.login_response.status_code
                if status == HttpStatus.C_200_OK:
                    status = self._post_config(login.session, body).status_code
                attempts.append({"method": name, "status": status})
                restored = status == HttpStatus.C_200_OK
            except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
                attempts.append({"method": name, "error": type(error).__name__, "message": str(error)})
                self.evidence.exception(error)
        self.evidence.write("RESTORATION ATTEMPTS", attempts)
        if not restored:
            self.outcomes["final restoration"] = "ERROR"
            return False
        try:
            login = self._login(ADMIN_USERNAME, self.config.gw_id, GatewayCfgLanAuthType.DEFAULT, True)
            payload: dict[str, Any] = self._read_config(login.session, "restored GET /ruuvi.json", True)
            self._require_setup(
                payload.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE) is False,
                "restored RO key is disabled",
                payload.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE),
            )
            self._require_setup(
                payload.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE) is False,
                "restored RW key is disabled",
                payload.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE),
            )
            actual_hash: str = canonical_json_hash(payload)
            self._require_setup(
                actual_hash == self.baseline_hash,
                "restored configuration equals baseline",
                HashComparisonEvidence(self.baseline_hash or "", actual_hash),
            )
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            self.evidence.exception(error)
            self.outcomes["final restoration"] = "ERROR"
            return False
        self.outcomes["final restoration"] = "PASS"
        return True

    def run(self) -> RunResult:
        failure: BaseException | None
        exit_code: int
        verdict: str
        verdict, exit_code, failure = "ERROR", 2, None
        self.evidence.write("TEST CASE AND UNIT", TEST_ID)
        self.evidence.write("UTC START", format_utc(self.evidence.started_at))
        try:
            self.progress("Authenticating with the factory-default administrative credential")
            default: InteractiveAuthResult = self._login(
                ADMIN_USERNAME, self.config.gw_id, GatewayCfgLanAuthType.DEFAULT, True, check_mode=False
            )
            realm: str = default.challenge["realm"]
            self.progress("Reading and validating the factory-default configuration")
            baseline: dict[str, Any] = self._read_config(default.session, "baseline GET /ruuvi.json", True)
            self._validate_baseline(baseline, default.auth_payload)
            self.last_session = default.session
            self.baseline_hash = canonical_json_hash(baseline)
            self.evidence.write("BASELINE CONFIGURATION SHA256", self.baseline_hash)

            password1: str
            user1: str
            user1, password1 = self._username(), self._secret(self.config.gw_id)
            ha1: str = self.gateway.calculate_digest_ha1(user1, realm, password1)
            self.evidence.write(
                "CUSTOM INTERACTIVE CREDENTIAL", {"username": user1, "plaintext": password1, "ha1": ha1, "realm": realm}
            )
            self.progress("Changing the default interactive credential")
            self.active_mechanisms = (AuthMech.LAN_WEBUI_DEFAULT,)
            self.last_username, self.last_password = user1, password1
            self.recovery_credentials.append((user1, password1))
            self.mutation_possible = True
            response: requests.Response = self._post_config(
                default.session,
                {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
                    GatewayCfgDesc.LAN_AUTH_USER: user1,
                    GatewayCfgDesc.LAN_AUTH_PASS: ha1,
                },
            )
            self._require_security(
                response.status_code == HttpStatus.C_200_OK,
                "first custom credential change succeeds",
                response.status_code,
            )
            custom1: InteractiveAuthResult = self._login(user1, password1, GatewayCfgLanAuthType.RUUVI, False)
            self.last_session = custom1.session
            self.last_username, self.last_password = user1, password1
            first: dict[str, Any] = self._read_config(custom1.session, "first custom configuration")
            self._require_security(
                first.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI,
                "first custom authentication mode is committed",
                first.get(GatewayCfgDesc.LAN_AUTH_TYPE),
            )
            self._require_security(
                first.get(GatewayCfgDesc.LAN_AUTH_USER) == user1,
                "first custom username is committed",
                first.get(GatewayCfgDesc.LAN_AUTH_USER),
            )
            old: InteractiveAuthResult = self.gateway.authenticate_interactive(ADMIN_USERNAME, self.config.gw_id)
            self._require_security(
                old.login_response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                "default credential no longer authenticates",
                old.login_response.status_code,
            )
            self.outcomes[AuthMech.LAN_WEBUI_DEFAULT] = "PASS"

            self.active_mechanisms = (AuthMech.LAN_WEBUI_USER_DEFINED,)
            password2: str
            user2: str
            user2, password2 = self._username(user1), self._secret(self.config.gw_id, password1)
            ha2: str = self.gateway.calculate_digest_ha1(user2, realm, password2)
            self.evidence.write(
                "REPLACEMENT INTERACTIVE CREDENTIAL", {"username": user2, "plaintext": password2, "ha1": ha2}
            )
            self.progress("Changing the user-defined interactive credential")
            self.last_username, self.last_password = user2, password2
            self.recovery_credentials.append((user2, password2))
            response = self._post_config(
                custom1.session,
                {
                    GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
                    GatewayCfgDesc.LAN_AUTH_USER: user2,
                    GatewayCfgDesc.LAN_AUTH_PASS: ha2,
                },
            )
            self._require_security(
                response.status_code == HttpStatus.C_200_OK,
                "replacement custom credential change succeeds",
                response.status_code,
            )
            custom2: InteractiveAuthResult = self._login(user2, password2, GatewayCfgLanAuthType.RUUVI, False)
            self.last_username, self.last_password, self.last_session = user2, password2, custom2.session
            second: dict[str, Any] = self._read_config(custom2.session, "replacement custom configuration")
            self._require_security(
                second.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.RUUVI,
                "replacement custom authentication mode is committed",
                second.get(GatewayCfgDesc.LAN_AUTH_TYPE),
            )
            self._require_security(
                second.get(GatewayCfgDesc.LAN_AUTH_USER) == user2,
                "replacement custom username is committed",
                second.get(GatewayCfgDesc.LAN_AUTH_USER),
            )
            self.outcomes[AuthMech.LAN_WEBUI_USER_DEFINED] = "PASS"

            self.progress("Generating and committing RO and RW bearer values")
            self.active_mechanisms = (AuthMech.M2M_API_BEARER_RO, AuthMech.M2M_API_BEARER_RW)
            rw1: str
            ro1: str
            ro1 = self._secret(self.config.gw_id)
            rw1 = self._secret(self.config.gw_id, ro1)
            self.evidence.write("FIRST BEARER KEYS", {"ro": ro1, "rw": rw1})
            response = self._post_config(
                custom2.session, {GatewayCfgDesc.LAN_AUTH_API_KEY: ro1, GatewayCfgDesc.LAN_AUTH_API_KEY_RW: rw1}
            )
            self._require_security(
                response.status_code == HttpStatus.C_200_OK, "first bearer key change succeeds", response.status_code
            )
            bearer1: dict[str, Any] = self._read_config(custom2.session, "first bearer configuration")
            self.active_mechanisms = (AuthMech.M2M_API_BEARER_RO,)
            self._require_security(
                bearer1.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE) is True,
                "RO key is committed",
                bearer1.get(GatewayCfgDesc.LAN_AUTH_API_KEY_USE),
            )
            self.active_mechanisms = (AuthMech.M2M_API_BEARER_RW,)
            self._require_security(
                bearer1.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE) is True,
                "RW key is committed",
                bearer1.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE),
            )
            self._verify_bearer(ro1, rw1, canonical_json_hash(bearer1))
            self.progress("Regenerating and verifying RO and RW bearer values")
            self.active_mechanisms = (AuthMech.M2M_API_BEARER_RO, AuthMech.M2M_API_BEARER_RW)
            rw2: str
            ro2: str
            ro2 = self._secret(ro1, rw1, self.config.gw_id)
            rw2 = self._secret(ro1, rw1, ro2, self.config.gw_id)
            self.evidence.write("REGENERATED BEARER KEYS", {"ro": ro2, "rw": rw2})
            response = self._post_config(
                custom2.session, {GatewayCfgDesc.LAN_AUTH_API_KEY: ro2, GatewayCfgDesc.LAN_AUTH_API_KEY_RW: rw2}
            )
            self._require_security(
                response.status_code == HttpStatus.C_200_OK,
                "regenerated bearer key change succeeds",
                response.status_code,
            )
            bearer2: dict[str, Any] = self._read_config(custom2.session, "regenerated bearer configuration")
            mechanism: str
            field: str
            for mechanism, field in (
                (AuthMech.M2M_API_BEARER_RO, GatewayCfgDesc.LAN_AUTH_API_KEY_USE),
                (AuthMech.M2M_API_BEARER_RW, GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE),
            ):
                self.active_mechanisms = (mechanism,)
                self._require_security(
                    bearer2.get(field) is True,
                    f"regenerated {field} is committed",
                    bearer2.get(field),
                )
            self._verify_bearer(ro2, rw2, canonical_json_hash(bearer2), final=True)
            self.outcomes[AuthMech.M2M_API_BEARER_RW] = "PASS"
        except SecurityFailure as error:
            error: Exception
            failure, verdict, exit_code = error, "FAIL", 1
            self.evidence.exception(error)
            for mechanism in self.active_mechanisms:
                self.outcomes[mechanism] = "FAIL"
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            failure, verdict, exit_code = error, "ERROR", 2
            self.evidence.exception(error)
            for mechanism in self.active_mechanisms:
                self.outcomes[mechanism] = "ERROR"
        finally:
            self.progress("Restoring the factory-default authentication configuration")
            restored: bool = self._restore()
            self.progress("Verifying restoration and aggregating the verdict")
            if not restored:
                self.factory_reset_required = True
                verdict, exit_code = "ERROR", 2
            elif failure is None:
                if all(self.outcomes[mechanism] == "PASS" for mechanism in MECHANISMS):
                    verdict, exit_code = "PASS", 0
                else:
                    self.evidence.write("INCOMPLETE RESULTS", self.outcomes)
                    verdict, exit_code = "ERROR", 2
        outcome: str
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write("FINAL RESULT", MechanismResultEvidence(mechanism, outcome))
        return RunResult(
            exit_code,
            verdict,
            dict(self.outcomes),
            set(self.coverage),
            FACTORY_RESET_MESSAGE if self.factory_reset_required else None,
        )


def execute_test_5_1_4_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] | None = None,
) -> RunResult:
    work_dir, output = work_dir or Path.cwd(), output or print
    log: EvidenceLog = EvidenceLog.create(work_dir / "logs", "test_5_1_4_2_a", now)
    output(f"Open log file: {log.path}")

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {mechanism: "NOT RUN" for mechanism in MECHANISMS}, set())
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        log.write("DUT CONFIGURATION", config)
        result = FunctionalTest_5_1_4_2_a(
            config,
            log,
            session_factory,
            random_bytes,
            progress=progress.step,
        ).run()
    except Exception as error:
        error: Exception
        log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
    finally:
        if result.recovery_message is not None:
            log.write("USER ACTION REQUIRED", result.recovery_message)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    if result.recovery_message:
        output(result.recovery_message)
    return result


def main() -> int:
    return execute_test_5_1_4_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
