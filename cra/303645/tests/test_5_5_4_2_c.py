"""Live ETSI 5.5-4-2 Unit c: client-side protection evidence over plain LAN HTTP."""

from __future__ import annotations

import base64
import hashlib
import json
import secrets
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Callable
from urllib.parse import unquote

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
from lib.gateway import (
    AuthMech,
    CapturedRequest,
    EcdhEvidence,
    EncryptedJsonEnvelope,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthResult,
)
from lib.http_api import GatewayApi, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_ID: str = "ETSI 5.5-4-2, Unit c — SecComMech-WebUI-Session"
SETUP: str = "factory-default setup"
CHALLENGE: str = "challenge and cookie binding"
ECDH: str = "P-256 ECDH and SHA-256 derivation"
LOGIN: str = "MD5/SHA-256 challenge response"
ENVELOPE: str = "AES-CBC envelope acceptance and effect"
PLAINTEXT: str = "plaintext-password absence"
RESTORE: str = "restoration and full configuration verification"
CHECKS: tuple[str, ...] = (SETUP, CHALLENGE, ECDH, LOGIN, ENVELOPE, PLAINTEXT, RESTORE)
TOTAL_STEPS: int = 8
TLS_STATEMENT: str = "TLS not used / not claimed — plain HTTP port 80. TLS not claimed; application-layer ECDH/AES only."
RECOVERY_MESSAGE: str = (
    "USER ACTION REQUIRED: Restoration could not be verified. Inspect the physical DUT and local evidence; "
    "restore the original coordinates and any changed settings from the retained baseline/backup before rerunning."
)


class ProtectionFailure(Exception):
    """An observed protection assertion failed."""


def config_hash(payload: dict[str, Any]) -> str:
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=True, allow_nan=False).encode("utf-8")).hexdigest()


class RecoveryEvidence(EvidenceLog):
    """Keep recovery HTTP usable after evidence or terminal output fails."""

    def __init__(self, delegate: EvidenceLog) -> None:
        # This proxy delegates every EvidenceLog operation; it does not own a second stream.
        self.delegate: EvidenceLog = delegate
        self.path: Path = delegate.path
        self.started_at: datetime = delegate.started_at
        self.recovering: bool = False
        self.failed: bool = False

    def report(self, action: Callable[[], None]) -> None:
        try:
            action()
        except Exception:  # Reporting must not obstruct mandatory DUT recovery.
            self.failed = True
            if not self.recovering:
                raise

    def write(self, label: str, value: Any = "") -> None:
        self.report(lambda: self.delegate.write(label, value))

    def write_line(self, message: str) -> None:
        self.report(lambda: self.delegate.write_line(message))

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        self.report(lambda: self.delegate.write_http_request(request))

    def write_http_response(self, response: requests.Response) -> None:
        self.report(lambda: self.delegate.write_http_response(response))

    def exception(self, error: BaseException) -> None:
        self.report(lambda: self.delegate.exception(error))

    def finish(self, verdict: str, now: Callable[[], datetime] = utc_now) -> None:
        self.report(lambda: self.delegate.finish(verdict, now))


class FunctionalTest_5_5_4_2_c:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        iv_generate: Callable[[int], bytes] = secrets.token_bytes,
        progress: Callable[[str], None] = lambda message: None,
        output: Callable[[str], None] = print,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: RecoveryEvidence = evidence if isinstance(evidence, RecoveryEvidence) else RecoveryEvidence(evidence)
        self.gateway: GatewayClient = GatewayClient(
            config, self.evidence, session_factory, random_bytes, ecc_generate,
            timeout=(5, 15), user_agent="ruuvi-etsi-test-5.5-4-2-c",
        )
        self.iv_generate: Callable[[int], bytes] = iv_generate
        self.progress: Callable[[str], None] = lambda message: self.evidence.report(lambda: progress(message))
        self.output: Callable[[str], None] = lambda message: self.evidence.report(lambda: output(message))
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.active: str = SETUP
        self.recovery_message: str | None = None
        self.baseline: dict[str, Any] | None = None
        self.auth: InteractiveAuthResult | None = None
        self.mutation_possible: bool = False

    def _say(self, message: str) -> None:
        self.output(message)
        self.evidence.write_line(message)

    def _require(self, condition: bool, description: str, actual: Any = "", *, setup: bool = False) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", actual))
        if not condition:
            self.outcomes[self.active] = "ERROR" if setup else "FAIL"
            self._say(f"{self.active}: {self.outcomes[self.active]} — {description}")
            if setup:
                raise InvalidSetup(description)
            raise ProtectionFailure(description)

    def _login(self, *, initial: bool = False) -> InteractiveAuthResult:
        try:
            auth: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            if initial:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        if auth.login_response.status_code != HttpStatus.C_200_OK:
            if initial:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise InvalidSetup(f"default login: expected HTTP 200, observed {auth.login_response.status_code}")
        self.gateway.response_json(auth.login_response, "POST /auth", dict)
        # Also works with offline transports which do not implement Requests cookie extraction.
        auth.session.cookies.clear()
        auth.session.cookies.set("RUUVISESSION", auth.cookie)
        return auth

    def _read(self, auth: InteractiveAuthResult) -> dict[str, Any]:
        response: requests.Response = self.gateway.request(auth.session, HttpMethod.GET, GatewayApi.CONFIG)
        if response.status_code != HttpStatus.C_200_OK:
            raise GatewayProtocolError(f"GET /ruuvi.json: expected HTTP 200, observed {response.status_code}")
        return self.gateway.response_json(response, "GET /ruuvi.json", dict)

    def _validate_baseline(self, payload: dict[str, Any], auth_payload: dict[str, Any]) -> None:
        verified: bool = False
        if GatewayCfgDesc.GW_MAC in payload:
            mac: Any = payload[GatewayCfgDesc.GW_MAC]
            self._require(type(mac) is str and OCTETS_6_RE.fullmatch(mac) is not None
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
        mode: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        valid = type(mode) is str and mode == GatewayCfgLanAuthType.DEFAULT
        if not valid and verified:
            self.recovery_message = FACTORY_RESET_MESSAGE
        self._require(valid, "challenge must report default authentication", mode, setup=True)
        self._require(type(payload.get(GatewayCfgDesc.COORDINATES)) is str,
                      "coordinates must be present as a string", setup=True)
        self._require(verified, "verified MAC is required to derive safe MQTT defaults", setup=True)
        mqtt_defaults: dict[str, Any] = default_config_values((GatewayCfgDesc.MQTT_DATA_FORMAT,))
        # Firmware builds these defaults from the uppercase nRF52 MAC, not the JSON template.
        mqtt_defaults[GatewayCfgDesc.MQTT_PREFIX] = f"ruuvi/{self.config.gw_mac.upper()}/"
        mqtt_defaults[GatewayCfgDesc.MQTT_CLIENT_ID] = self.config.gw_mac.upper()
        for field, value in mqtt_defaults.items():
            actual = payload.get(field)
            self._require(type(actual) is type(value) and actual == value,
                          f"{field} must survive coordinates-only partial parsing unchanged", setup=True)

    def _compare(self, actual: dict[str, Any], expected: dict[str, Any], *, setup: bool = False) -> None:
        comparison: HashComparisonEvidence = HashComparisonEvidence(config_hash(expected), config_hash(actual))
        self.evidence.write("FULL CONFIGURATION SHA256", comparison)
        self._require(comparison.baseline == comparison.final,
                      "complete configuration must match the expected snapshot", comparison, setup=setup)

    def _check_handshake(self, auth: InteractiveAuthResult) -> None:
        self.active = CHALLENGE
        self.progress("Verifying the captured challenge and session cookie binding")
        self.evidence.write("RAW WWW-AUTHENTICATE", auth.auth_header)
        self.evidence.write("PARSED CHALLENGE", auth.challenge)
        self.evidence.write("RUUVISESSION COOKIE", auth.cookie)
        parsed: dict[str, str] = self.gateway.parse_interactive_challenge(auth.auth_header)
        self._require(auth.challenge_response.status_code == HttpStatus.C_401_UNAUTHORIZED
                      and parsed == auth.challenge and parsed["session_cookie"] == "RUUVISESSION"
                      and parsed["session_id"] == auth.cookie
                      and auth.challenge_response.cookies.get("RUUVISESSION") == auth.cookie,
                      "HTTP 401 challenge with quoted fields must bind the issued session cookie")
        self.outcomes[CHALLENGE] = "PASS"
        self.active = ECDH
        self.progress("Verifying P-256 public keys and the shared-key derivation")
        material: EcdhEvidence | None = auth.ecdh
        if material is None:
            raise GatewayProtocolError("ECDH observation unavailable")
        self.evidence.write("ECDH OBSERVATION", material)
        first: CapturedRequest = self.gateway.captured_requests[0]
        self._require(dict(first.headers).get(HttpHeader.RUUVI_ECDH_PUBLIC_KEY)
                      == base64.b64encode(material.client_public_key).decode("ascii")
                      and len(material.client_public_key) == 65 and material.client_public_key[0] == 4
                      and len(material.gateway_public_key) == 65 and material.gateway_public_key[0] == 4
                      and material.gateway_public_key == auth.gateway_public_key_raw
                      and auth.challenge_response.headers.get(HttpHeader.RUUVI_ECDH_PUBLIC_KEY)
                      == base64.b64encode(material.gateway_public_key).decode("ascii")
                      and len(material.shared_secret) == 32
                      and auth.aes_key == material.aes_key == hashlib.sha256(material.shared_secret).digest(),
                      "captured P-256 exchange must derive AES key as SHA256(ECDH X)")
        self.outcomes[ECDH] = "PASS"
        self.active = LOGIN
        self.progress("Verifying the captured MD5/SHA-256 login response")
        if auth.calculation is None:
            raise GatewayProtocolError("authentication calculation unavailable")
        self.evidence.write("AUTH CALCULATION INPUTS AND OUTPUTS", auth.calculation)
        request: CapturedRequest = self.gateway.captured_requests[1]
        expected_url: str | None = requests.Request(HttpMethod.POST, f"{self.config.base_url}{GatewayApi.AUTH}").prepare().url
        self._require(request.method == HttpMethod.POST and request.url == expected_url
                      and request.body is not None and json.loads(request.body)
                      == {"login": "Admin", "password": auth.calculation.response},
                      "captured POST /auth must contain only Admin and the calculated response")
        self.outcomes[LOGIN] = "PASS"
        self._check_request_password(request)

    def _mutate(self, auth: InteractiveAuthResult, baseline: dict[str, Any]) -> None:
        self.active = ENVELOPE
        self.progress("Sending and verifying the encrypted coordinates mutation")
        original: str = baseline[GatewayCfgDesc.COORDINATES]
        marker: str = f"cra-{self.gateway.random_text(9)}"
        if marker == original:
            marker = f"{marker}-x"
        envelope: EncryptedJsonEnvelope = self.gateway.build_encrypted_json(
            auth.aes_key, {GatewayCfgDesc.COORDINATES: marker}, self.iv_generate,
        )
        self.mutation_possible = True
        response: requests.Response = self.gateway.post_encrypted_json(auth.session, GatewayApi.CONFIG, envelope)
        request: CapturedRequest = self.gateway.captured_requests[-1]
        self._check_request_password(request)
        self._require(dict(request.headers).get(HttpHeader.RUUVI_ECDH_ENCRYPTED) == "true"
                      and request.body is not None and json.loads(request.body) == envelope.json_body()
                      and marker.encode() not in request.body,
                      "configuration POST must carry the ciphertext envelope and encryption header")
        if response.status_code >= HttpStatus.C_500_INTERNAL_SERVER_ERROR:
            raise GatewayProtocolError(f"encrypted POST infrastructure error: HTTP {response.status_code}")
        self._require(response.status_code == HttpStatus.C_200_OK,
                      f"encrypted POST /ruuvi.json: expected HTTP 200, observed {response.status_code}")
        expected: dict[str, Any] = dict(baseline)
        expected[GatewayCfgDesc.COORDINATES] = marker
        self._compare(self._read(auth), expected)
        self.outcomes[ENVELOPE] = "PASS"

    def _restore(self, auth: InteractiveAuthResult, baseline: dict[str, Any]) -> bool:
        self.active = RESTORE
        self.progress("Restoring coordinates and verifying every baseline configuration field")
        candidate: int
        error: Exception
        rejected_response: bool = False
        for candidate in range(3):
            self.evidence.write("RESTORATION ATTEMPT", candidate + 1)
            try:
                if self.mutation_possible:
                    if candidate == 1:
                        auth = self._login()
                    body: dict[str, str] = {GatewayCfgDesc.COORDINATES: baseline[GatewayCfgDesc.COORDINATES]}
                    try:
                        if candidate < 2:
                            envelope: EncryptedJsonEnvelope = self.gateway.build_encrypted_json(
                                auth.aes_key, body, self.iv_generate,
                            )
                            response: requests.Response = self.gateway.post_encrypted_json(
                                auth.session, GatewayApi.CONFIG, envelope,
                            )
                        else:
                            response = self.gateway.request(auth.session, HttpMethod.POST, GatewayApi.CONFIG, json_body=body)
                        if response.status_code != HttpStatus.C_200_OK:
                            rejected_response = True
                            self._say(f"Restoration write {candidate + 1}: expected HTTP 200, observed {response.status_code}")
                        self.evidence.write("RESTORATION HTTP STATUS", response.status_code)
                    except Exception as error:  # noqa: BLE001 - A lost write reply must not skip recovery read-back.
                        self.evidence.exception(error)
                        self._say(f"Restoration write {candidate + 1}: {type(error).__name__}; verifying read-back")
                    # Always read back, including after a lost reply or explicit rejection.
                final: dict[str, Any] = self._read(auth)
                if candidate < 2 and config_hash(final) != config_hash(baseline):
                    self.evidence.write("RESTORATION MISMATCH", final)
                    raise InvalidSetup("restoration read-back differs; retrying with retained default credentials")
                self._compare(final, baseline)
                self.outcomes[RESTORE] = "ERROR" if rejected_response else "PASS"
                if rejected_response:
                    self._say("Restoration: ERROR — baseline verified, but a cleanup POST returned non-200")
                return not rejected_response
            except ProtectionFailure as error:
                self.evidence.exception(error)
                # Do not hide unrelated parser side effects by adding fields to a cleanup POST.
                break
            except Exception as error:  # noqa: BLE001 - Continue mandatory recovery after transport/protocol errors.
                self.evidence.exception(error)
                self._say(f"Restoration attempt {candidate + 1}: {type(error).__name__}; see local evidence")
        if self.outcomes[RESTORE] != "FAIL":
            self.outcomes[RESTORE] = "ERROR"
        self.recovery_message = RECOVERY_MESSAGE
        return False

    def _check_request_password(self, request: CapturedRequest) -> None:
        previous: str = self.active
        self.active = PLAINTEXT
        if request.body is None:
            raise GatewayProtocolError("a streaming request could not be captured")
        header_text: str = "\n".join(f"{name}: {value}" for name, value in request.headers)
        self._require(self.config.gw_id.encode() not in request.body
                      and self.config.gw_id not in request.url and self.config.gw_id not in unquote(request.url)
                      and self.config.gw_id not in header_text,
                      "plaintext admin password must be absent from every captured URL, header, and body")
        self.active = previous

    def _check_plaintext(self) -> None:
        self.active = PLAINTEXT
        self.progress("Checking all captured requests, including recovery, for plaintext passwords")
        captures: list[CapturedRequest] = self.gateway.captured_requests
        if not captures:
            return
        request: CapturedRequest
        for request in captures:
            self._check_request_password(request)
        self.evidence.write("PASSWORD ABSENCE CAPTURE COUNT", len(captures))
        self.outcomes[PLAINTEXT] = "PASS"

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        self.evidence.write("DUT IDENTITY", self.config)
        self.evidence.write("AUTHENTICATION MECHANISM", AuthMech.LAN_WEBUI_DEFAULT)
        error: Exception
        try:
            self.progress("Authenticating Admin and checking two stable factory-default configuration reads")
            auth: InteractiveAuthResult = self._login(initial=True)
            self.auth = auth
            baseline: dict[str, Any] = self._read(auth)
            self._validate_baseline(baseline, auth.auth_payload)
            self._compare(self._read(auth), baseline, setup=True)
            self.baseline = baseline
            self.evidence.write("FULL BASELINE", baseline)
            self.outcomes[SETUP] = "PASS"
            self._check_handshake(auth)
            self._mutate(auth, baseline)
            verdict = "PASS"
        except ProtectionFailure as error:
            self.evidence.recovering = True
            self.evidence.exception(error)
            verdict = "FAIL"
        except Exception as error:  # noqa: BLE001 - Unexpected errors must still trigger mandatory restoration.
            self.evidence.recovering = True
            self.evidence.exception(error)
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            self._say(f"{self.active}: ERROR — {type(error).__name__}; see local evidence")
        finally:
            self.evidence.recovering = True
            if self.baseline is not None and self.auth is not None and not self._restore(self.auth, self.baseline):
                verdict = "ERROR"
            try:
                self._check_plaintext()
            except ProtectionFailure as error:
                self.evidence.exception(error)
                if verdict != "ERROR":
                    verdict = "FAIL"
            except Exception as error:  # noqa: BLE001 - Incomplete request evidence cannot yield PASS.
                self.evidence.exception(error)
                self.outcomes[PLAINTEXT] = "ERROR"
                verdict = "ERROR"
        if verdict == "PASS" and any(value != "PASS" for value in self.outcomes.values()):
            verdict = "ERROR"
        check: str
        outcome: str
        for check, outcome in self.outcomes.items():
            self.evidence.write("FINAL RESULT", MechanismResultEvidence(check, outcome))
            self._say(f"{check}: {outcome}")
        if self.evidence.failed:
            verdict = "ERROR"
            self._say("Reporting ERROR — terminal or evidence output failed; recovery checks still attempted")
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(), self.recovery_message)


def execute_test_5_5_4_2_c(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
    iv_generate: Callable[[int], bytes] = secrets.token_bytes,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    log: RecoveryEvidence | None = None
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    reporting_failed: bool = False

    def terminal(message: str) -> None:
        nonlocal reporting_failed
        try:
            output(message)
        except Exception:  # Reporting failures must retain a result and cannot block recovery.
            reporting_failed = True
            if log is not None:
                log.failed = True
            if log is not None and not log.recovering:
                raise

    def progress_output(message: str) -> None:
        terminal(message)
        if log is not None:
            log.write_line(message)

    error: Exception
    try:
        work_dir = Path.cwd() if work_dir is None else work_dir
        active_log: RecoveryEvidence = RecoveryEvidence(EvidenceLog.create(work_dir / "logs", "test_5_5_4_2_c", now))
        log = active_log
        terminal(f"Open log file: {active_log.path}")
        progress: ProgressReporter = ProgressReporter(progress_output, TOTAL_STEPS)
        active_log.write("TEST CASE AND UNIT", TEST_ID)
        active_log.write("UTC START", active_log.started_at.isoformat())
        active_log.write("TRANSPORT", TLS_STATEMENT)
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        result = FunctionalTest_5_5_4_2_c(
            config, active_log, session_factory, random_bytes, ecc_generate, iv_generate, progress.step, terminal,
        ).run()
    except Exception as error:  # Setup failures must still produce best-effort final evidence.
        if log is not None:
            log.recovering = True
            log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
        result.outcomes[SETUP] = "ERROR"
        if log is not None:
            log.write("FINAL RESULTS", result.outcomes)
        progress_output(f"Setup ERROR — {type(error).__name__}; see local evidence if available")
    finally:
        if log is not None:
            log.recovering = True
        if result.recovery_message:
            progress_output(result.recovery_message)
        if reporting_failed or (log is not None and log.failed):
            result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
        # A terminal failure must affect the verdict before healthy evidence is closed.
        reported_verdict: str = result.verdict
        terminal(f"Overall verdict: {reported_verdict}")
        if reporting_failed or (log is not None and log.failed):
            result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
        if reported_verdict != result.verdict:
            terminal(f"Overall verdict: {result.verdict}")
        if log is not None:
            try:
                log.delegate.finish(result.verdict, now)
            except Exception as error:  # Finalization failures must correct any previously printed PASS/FAIL.
                log.failed = True
                result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
                log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
                log.finish("ERROR", now)
                progress_output("Reporting ERROR — evidence finalization failed; see local evidence if available")
                if result.recovery_message:
                    progress_output(result.recovery_message)
                terminal("Overall verdict: ERROR")
        if reporting_failed or (log is not None and log.failed):
            result = RunResult(2, "ERROR", result.outcomes, result.coverage, result.recovery_message)
    return result


def main() -> int:
    return execute_test_5_5_4_2_c().exit_code


if __name__ == "__main__":
    sys.exit(main())
