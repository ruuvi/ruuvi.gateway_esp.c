"""Live functional identification observations for ETSI 5.5-2-2 Unit A."""

from __future__ import annotations

import re
import secrets
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Callable
from urllib.parse import quote

import requests
from Crypto.PublicKey import ECC

from lib.config import AUTHENTICATION_DEFAULT_FIELDS, FACTORY_RESET_MESSAGE, default_config_values, load_dut_config
from lib.errors import GatewayAuthenticationModeError, GatewayConnectionError, GatewayProtocolError, InvalidSetup
from lib.evidence import AssertionEvidence, EvidenceLog, MechanismResultEvidence, format_utc, utc_now
from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import GatewayApi, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult
from lib.serial_dut import SerialPort, SerialTransport

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-2-2, Test Unit A"
EXPECTED_IDF_VERSION: str = "v4.2.5"
REPOSITORY_CMAKE: Path = Path(__file__).resolve().parents[3] / "CMakeLists.txt"
ESP_REPO: str = "https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c"
NRF_REPO: str = "https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c"
USER_AGENT: str = "ruuvi-etsi-test-5.5-2-2-a"
HTTP_TIMEOUT: tuple[int, int] = (5, 15)
UART_CAPTURE_SECONDS: float = 8.0
TOTAL_STEPS: int = 10
SETUP: str = "DUT gw_mac/.env consistency and factory-default setup"
FRAMEWORK: str = "NetSecImpl-Framework-Stack"
APPLICATION: str = "ruuvi.gateway_esp.c"
COPROCESSOR: str = "NetSecImpl-CoProcessor-Firmware"
CRYPTO: str = "NetSecImpl-Crypto-Library"
WEB_UI: str = "NetSecImpl-Web-Application-Core"
SDK: str = "nRF5 SDK"
ESP_CONSISTENCY: str = "ESP32 firmware version consistency"
NRF_CONSISTENCY: str = "nRF52 firmware version consistency"
CHECKS: tuple[str, ...] = (
    SETUP, FRAMEWORK, ESP_CONSISTENCY, NRF_CONSISTENCY, APPLICATION, COPROCESSOR, CRYPTO, WEB_UI, SDK,
)
VERSION_PATTERN: str = r"[vV]?(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
VERSION_RE: re.Pattern[str] = re.compile(VERSION_PATTERN)
ANSI_ESCAPE_RE: re.Pattern[str] = re.compile(r"\x1b\[[0-9;]*m")
BANNER_RE: re.Pattern[str] = re.compile(
    rf"cpu_start:[ \t]+ESP-IDF:[ \t]+({VERSION_PATTERN})[ \t]*\r?$", re.MULTILINE,
)
MAC_RE: re.Pattern[str] = re.compile(r"[0-9a-fA-F]{12}|[0-9a-fA-F]{2}(?:[:-][0-9a-fA-F]{2}){5}")
APP_VERSION_RE: re.Pattern[str] = re.compile(
    r"^I \(\d+\) cpu_start:[ \t]+App version:[ \t]*([^\r\n]*)\r?\n", re.MULTILINE,
)
NRF_VERSION_RE: re.Pattern[str] = re.compile(
    # The stable message prefix is independent of logger tags (nRF52Fw vs nrf52fw).
    # Retain support for the unprefixed message emitted by other firmware revisions.
    r"^(?:[^\r\n]*?###[ \t]+|I \(\d+\) (?i:nrf52fw):[ \t]+(?:\[[^]\r\n]+][ \t]+)?)"
    r"Firmware on nRF52:[ \t]*([^\r\n]*)\r?\n", re.MULTILINE,
)
METRIC_LABEL_RE: re.Pattern[str] = re.compile(
    r'[ \t]*([a-zA-Z_][a-zA-Z0-9_]*)[ \t]*=[ \t]*"((?:[^"\\\r\n]|\\["\\n])*)"[ \t]*(,|$)',
)


@dataclass(frozen=True)
class DeviceVersions:
    fw_ver: str
    nrf52_fw_ver: str
    gw_mac: str


@dataclass(frozen=True)
class HttpVersions:
    ruuvi_json: DeviceVersions
    metrics: DeviceVersions


@dataclass(frozen=True)
class VersionComparison:
    ruuvi_json: str
    metrics: str
    uart: str | None


@dataclass(frozen=True)
class BootBanner:
    raw_version: str
    normalized_version: str


@dataclass(frozen=True)
class VersionNormalization:
    source: str
    raw: str
    normalized: str


@dataclass(frozen=True)
class ReleaseLookup:
    url: str
    status: int
    tag: str | None


def normalize_version(value: str) -> str:
    token: str = value.strip()
    if VERSION_RE.fullmatch(token) is None:
        raise GatewayProtocolError(f"invalid semantic version: {value!r}")
    return f"v{token.lstrip('vV').lower()}"


def extract_boot_banner(console: str) -> BootBanner:
    # The second-stage bootloader can survive OTA updates and use an older IDF.
    # cpu_start prints the running application's esp_app_desc_t.idf_ver instead.
    matches: list[str] = BANNER_RE.findall(ANSI_ESCAPE_RE.sub("", console))
    if not matches:
        if "DOWNLOAD_BOOT" in console or "waiting for download" in console:
            raise InvalidSetup(
                "UART shows ESP32 ROM download mode; the reset did not start the application "
                "and no application ESP-IDF version was observed"
            )
        raise InvalidSetup("no application cpu_start: ESP-IDF: line observed in the bounded UART window")
    versions: set[str] = {normalize_version(version) for version in matches}
    if len(versions) != 1:
        raise GatewayProtocolError(f"conflicting application ESP-IDF startup lines: {matches!r}")
    return BootBanner(matches[0], versions.pop())


def normalize_mac(value: Any) -> str:
    if not isinstance(value, str) or MAC_RE.fullmatch(value) is None:
        raise InvalidSetup(f"malformed DUT MAC address: {value!r}")
    return value.replace(":", "").replace("-", "").lower()


def _unescape_metric_label(match: re.Match[str]) -> str:
    return "\n" if match.group(1) == "n" else match.group(1)


def extract_metrics_versions(body: str) -> DeviceVersions:
    """Parse the single gateway-info sample, independently of label order."""
    lines: list[str] = [line.strip() for line in body.splitlines()
                        if re.match(r"ruuvigw_info(?:\{|\s|$)", line.strip())]
    if len(lines) != 1:
        raise GatewayProtocolError("/metrics requires exactly one ruuvigw_info sample")
    sample: re.Match[str] | None = re.fullmatch(r"ruuvigw_info\{(.*)}[ \t]+([-+0-9.eE]+)[ \t]*", lines[0])
    if sample is None:
        raise GatewayProtocolError("malformed ruuvigw_info sample")
    try:
        value: float = float(sample.group(2))
    except ValueError as error:
        error: ValueError
        raise GatewayProtocolError("invalid ruuvigw_info sample value") from error
    if value != 1.0:
        raise GatewayProtocolError("ruuvigw_info sample value must be 1")
    labels: dict[str, str] = {}
    raw_labels: str = sample.group(1)
    position: int = 0
    while position < len(raw_labels):
        label: re.Match[str] | None = METRIC_LABEL_RE.match(raw_labels, position)
        if label is None or label.group(1) in labels:
            raise GatewayProtocolError("malformed or duplicate ruuvigw_info label")
        labels[label.group(1)] = re.sub(r'\\(["\\n])', _unescape_metric_label, label.group(2))
        position = label.end()
    if not all(key in labels for key in ("mac", "esp_fw", "nrf_fw")):
        raise GatewayProtocolError("ruuvigw_info requires mac, esp_fw, and nrf_fw labels")
    normalize_version(labels["esp_fw"])
    normalize_version(labels["nrf_fw"])
    normalize_mac(labels["mac"])
    return DeviceVersions(labels["esp_fw"], labels["nrf_fw"], labels["mac"])


def extract_uart_version(console: str, pattern: re.Pattern[str], component: str) -> str:
    matches: list[str] = pattern.findall(ANSI_ESCAPE_RE.sub("", console))
    if not matches:
        raise InvalidSetup(f"no {component} firmware version line observed in the bounded UART window")
    versions: set[str] = {normalize_version(version) for version in matches}
    if len(versions) != 1:
        raise GatewayProtocolError(f"conflicting {component} firmware versions in UART boot log")
    return matches[0]


def startup_complete(console: str) -> bool:
    """Require complete lines so a serial chunk cannot truncate a version suffix."""
    complete: str = ANSI_ESCAPE_RE.sub("", console).rsplit("\n", 1)[0] + "\n" if "\n" in console else ""
    return bool(BANNER_RE.search(complete) and APP_VERSION_RE.search(complete) and NRF_VERSION_RE.search(complete))


def lookup_release(
    repository: str,
    tag: str,
    evidence: EvidenceLog,
    session_factory: Callable[[], requests.Session] = requests.Session,
) -> ReleaseLookup:
    """Public tag lookup; a 404 needs an independent successful repository lookup."""
    if repository not in (ESP_REPO, NRF_REPO):
        raise InvalidSetup(f"unexpected GitHub repository: {repository}")
    normalized: str = normalize_version(tag)
    evidence.write("VERSION NORMALIZATION", VersionNormalization("GitHub requested tag", tag, normalized))
    url: str = f"{repository}/releases/tags/{quote(normalized, safe='')}"
    session: requests.Session = session_factory()
    session.trust_env = False
    session.auth = None
    session.headers.clear()
    session.cookies.clear()
    session.params = {}

    def get(target: str) -> requests.Response:
        request: requests.PreparedRequest = session.prepare_request(requests.Request(
            HttpMethod.GET, target,
            headers={HttpHeader.USER_AGENT: USER_AGENT, "Accept": "application/vnd.github+json"},
        ))
        evidence.write_http_request(request)
        try:
            received: requests.Response = session.send(
                request, timeout=HTTP_TIMEOUT, allow_redirects=False, verify=True,
            )
        except requests.RequestException as error:
            error: requests.RequestException
            raise GatewayConnectionError(f"public GitHub lookup failed: {target}: {error}") from error
        evidence.write_http_response(received)
        return received

    try:
        response: requests.Response = get(url)
        if response.status_code == HttpStatus.C_404_NOT_FOUND:
            reachable: requests.Response = get(repository)
            if reachable.status_code != HttpStatus.C_200_OK:
                raise InvalidSetup(f"cannot confirm GitHub repository availability: HTTP {reachable.status_code}")
            metadata: dict[str, Any] = GatewayClient.response_json(reachable, "GitHub repository", dict)
            if metadata.get("full_name") != repository[len("https://api.github.com/repos/"):]:
                raise GatewayProtocolError("GitHub repository identity mismatch")
            result: ReleaseLookup = ReleaseLookup(url, response.status_code, None)
        elif response.status_code == HttpStatus.C_200_OK:
            payload: dict[str, Any] = GatewayClient.response_json(response, "GitHub release", dict)
            resolved: Any = payload.get("tag_name")
            if not isinstance(resolved, str):
                raise GatewayProtocolError("GitHub release has no string tag_name")
            canonical: str = normalize_version(resolved)
            evidence.write("VERSION NORMALIZATION", VersionNormalization("GitHub resolved tag", resolved, canonical))
            published: Any = payload.get("published_at")
            if (canonical != normalized or payload.get("draft") is not False
                    or not isinstance(published, str) or not published.strip()):
                raise GatewayProtocolError("GitHub response does not identify the requested published release")
            result = ReleaseLookup(url, response.status_code, canonical)
        else:
            raise InvalidSetup(f"GitHub unavailable or rate-limited: HTTP {response.status_code}")
        evidence.write("RELEASE LOOKUP", result)
        return result
    finally:
        session.close()


class FunctionalTest_5_5_2_2_a:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        serial_transport: SerialTransport | None = None,
        release_lookup: Callable[[str, str, EvidenceLog], ReleaseLookup] = lookup_release,
        progress: Callable[[str], None] = print,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        cmake_path: Path = REPOSITORY_CMAKE,
        *,
        output: Callable[[str], None] = print,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(
            config, evidence, session_factory, random_bytes, ecc_generate, HTTP_TIMEOUT, USER_AGENT,
        )
        self.serial: SerialTransport = serial_transport if serial_transport is not None else SerialTransport()
        self.release_lookup: Callable[[str, str, EvidenceLog], ReleaseLookup] = release_lookup
        self.progress: Callable[[str], None] = progress
        self.output: Callable[[str], None] = output
        self.cmake_path: Path = cmake_path
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.recovery_message: str | None = None
        self.active: str = SETUP

    def _report(self, message: str) -> None:
        self.evidence.write_line(message)
        self.output(message)

    def _normalized(self, source: str, raw: str) -> str:
        normalized: str = normalize_version(raw)
        self.evidence.write("VERSION NORMALIZATION", VersionNormalization(source, raw, normalized))
        return normalized

    def _json(self, session: requests.Session, path: str) -> dict[str, Any]:
        response: requests.Response = self.gateway.request(session, HttpMethod.GET, path)
        if response.status_code != HttpStatus.C_200_OK:
            raise InvalidSetup(f"GET {path} requires 200, observed {response.status_code}")
        return self.gateway.response_json(response, path, dict)

    def _identity(self, value: Any) -> None:
        if normalize_mac(value) != normalize_mac(self.config.gw_mac):
            raise InvalidSetup("DUT identity differs from .env gw_mac")

    def _baseline(self) -> HttpVersions:
        try:
            auth: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        try:
            if auth.login_response.status_code != HttpStatus.C_200_OK:
                self.recovery_message = FACTORY_RESET_MESSAGE
                raise InvalidSetup(f"POST {GatewayApi.AUTH}: default login rejected")
            baseline: dict[str, Any] = self._json(auth.session, GatewayApi.CONFIG)
            verified: bool = GatewayCfgDesc.GW_MAC in baseline
            if verified:
                self._identity(baseline[GatewayCfgDesc.GW_MAC])
            defaults: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
            key: str
            expected: Any
            for key, expected in defaults.items():
                observed: Any = baseline.get(key)
                equal: bool = type(observed) is type(expected) and observed == expected
                self.evidence.write("DEFAULT FIELD", AssertionEvidence(key, "PASS" if equal else "ERROR", observed))
                if not equal:
                    self.recovery_message = FACTORY_RESET_MESSAGE if verified else None
                    raise InvalidSetup(f"required factory-default field differs or is absent: {key}")
            if auth.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) != GatewayCfgLanAuthType.DEFAULT:
                self.recovery_message = FACTORY_RESET_MESSAGE if verified else None
                raise InvalidSetup(f"GET {GatewayApi.AUTH} did not report default interactive authentication")
            self.progress("Validating runtime device identity and versions from authenticated /ruuvi.json")
            if not verified:
                raise GatewayProtocolError(f"GET {GatewayApi.CONFIG} requires {GatewayCfgDesc.GW_MAC}")
            for key in (GatewayCfgDesc.FW_VER, GatewayCfgDesc.NRF52_FW_VER):
                if not isinstance(baseline.get(key), str):
                    raise GatewayProtocolError(f"GET {GatewayApi.CONFIG} requires string {key}")
                # Reject malformed observations before any USB discovery/reset.
                self._normalized(f"GET {GatewayApi.CONFIG} {key}", baseline[key])
            versions: DeviceVersions = DeviceVersions(
                baseline[GatewayCfgDesc.FW_VER], baseline[GatewayCfgDesc.NRF52_FW_VER], baseline[GatewayCfgDesc.GW_MAC],
            )
            self.evidence.write("DEVICE VERSION SOURCE", f"GET {GatewayApi.CONFIG}")
            self.evidence.write("DEVICE VERSIONS", versions)
            self.progress("Reading firmware versions from authenticated /metrics")
            response: requests.Response = self.gateway.request(auth.session, HttpMethod.GET, GatewayApi.METRICS)
            if response.status_code != HttpStatus.C_200_OK:
                raise InvalidSetup(f"GET {GatewayApi.METRICS} requires 200, observed {response.status_code}")
            metrics: DeviceVersions = extract_metrics_versions(response.text)
            self._identity(metrics.gw_mac)
            self.evidence.write("METRICS DEVICE VERSIONS", metrics)
            return HttpVersions(versions, metrics)
        finally:
            auth.session.close()

    def _framework(self, port: SerialPort) -> str:
        self.active = FRAMEWORK
        self.progress("Resetting through CH340 and capturing ESP-IDF, ESP32, and nRF52 startup versions")
        self.evidence.write("UART CAPTURE SECONDS", UART_CAPTURE_SECONDS)
        console: str = self.serial.capture(
            port, self.evidence, duration=UART_CAPTURE_SECONDS, stop_when=startup_complete,
        )
        banner: BootBanner = extract_boot_banner(console)
        self.evidence.write("APPLICATION ESP-IDF STARTUP", banner)
        observed: str = self._normalized("UART cpu_start application ESP-IDF", banner.raw_version)
        expected: str = self._normalized("IXIT 12-NetSecImpl ESP-IDF", EXPECTED_IDF_VERSION)
        self.outcomes[FRAMEWORK] = "PASS" if observed == expected else "FAIL"
        self.evidence.write("FRAMEWORK COMPARISON", AssertionEvidence(
            f"UART application ESP-IDF equals IXIT {expected}", self.outcomes[FRAMEWORK], observed,
        ))
        self._report(
            f"{self.outcomes[FRAMEWORK]}: Application ESP-IDF: observed {observed}, expected {expected} "
            "(IXIT; bootloader version is not checked)"
        )
        source: str = self.cmake_path.read_text(encoding="utf-8")
        pin: re.Match[str] | None = re.search(r'set\(EXPECTED_IDF_VERSION\s+"?([^\s")]+)"?\s*\)', source)
        if pin is None:
            raise InvalidSetup("CMakeLists.txt has no EXPECTED_IDF_VERSION pin")
        pinned: str = self._normalized("CMake EXPECTED_IDF_VERSION", pin.group(1))
        if pinned != expected:
            raise InvalidSetup("repository ESP-IDF pin disagrees with the authoritative IXIT expectation")
        return console

    def _consistency(self, check: str, ruuvi: str, metrics: str, uart: str | None = None) -> None:
        self.active = check
        values: VersionComparison = VersionComparison(
            self._normalized(f"{check} /ruuvi.json", ruuvi),
            self._normalized(f"{check} /metrics", metrics),
            self._normalized(f"{check} UART", uart) if uart is not None else None,
        )
        equal: bool = values.ruuvi_json == values.metrics and (values.uart is None or values.uart == values.ruuvi_json)
        outcome: str = "FAIL" if not equal else ("PASS" if uart is not None else "PENDING")
        self.evidence.write("VERSION SOURCE COMPARISON", AssertionEvidence(check, outcome, values))
        if outcome != "PENDING":
            self.outcomes[check] = outcome
            self._report(
                f"{outcome}: {check}: /ruuvi.json={values.ruuvi_json}, /metrics={values.metrics}, "
                f"UART={values.uart if values.uart is not None else 'not yet observed'}"
            )

    def _release(self, check: str, repo: str, raw: str) -> None:
        self.active = check
        self.progress(f"Checking the published {check} release")
        tag: str = self._normalized(check, raw)
        result: ReleaseLookup = self.release_lookup(repo, tag, self.evidence)
        self.evidence.write("COMPONENT RELEASE COMPARISON", result)
        if result.status == HttpStatus.C_404_NOT_FOUND:
            self.outcomes[check] = "FAIL"
            self._report(f"FAIL: {check}: installed {tag} has no published GitHub release (confirmed HTTP 404)")
        elif result.status == HttpStatus.C_200_OK and result.tag is not None:
            if self._normalized(f"{check} resolved release", result.tag) != tag:
                raise GatewayProtocolError("release lookup returned a different tag")
            self.outcomes[check] = "PASS"
            self._report(f"PASS: {check}: installed {tag} is a published GitHub release")
        else:
            raise InvalidSetup(f"release lookup incomplete: HTTP {result.status}")

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        code: int = 2
        check: str
        for check in (CRYPTO, WEB_UI, SDK):
            self.outcomes[check] = "INFORMATIONAL"
        self.evidence.write("IDENTIFICATION LIMITATIONS", (
            "mbedTLS Custom Backport and Web-UI Production Bundle expose no runtime version string; "
            "nRF5 SDK v15.3.0 is build provenance, not runtime telemetry. Published release existence "
            "does not establish the exact audited build or cryptographic security."
        ))
        try:
            self.progress("Authenticating and checking factory-default configuration")
            versions: HttpVersions = self._baseline()
            self.outcomes[SETUP] = "PASS"
            self._report("PASS: DUT identity and factory-default configuration")
            self._consistency(ESP_CONSISTENCY, versions.ruuvi_json.fw_ver, versions.metrics.fw_ver)
            self._consistency(NRF_CONSISTENCY, versions.ruuvi_json.nrf52_fw_ver, versions.metrics.nrf52_fw_ver)
            # Hardware discovery follows the mandatory baseline gate; no scans/reset before it.
            self.active = FRAMEWORK
            self.progress("Discovering exactly one CH340 USB serial port")
            port: SerialPort = self.serial.discover()
            self.evidence.write("SERIAL PORT", port)
            console: str = self._framework(port)
            self.progress("Comparing ESP32 and nRF52 firmware versions across JSON, metrics, and UART")
            self.active = ESP_CONSISTENCY
            self._consistency(ESP_CONSISTENCY, versions.ruuvi_json.fw_ver, versions.metrics.fw_ver,
                              extract_uart_version(console, APP_VERSION_RE, "ESP32"))
            self.active = NRF_CONSISTENCY
            self._consistency(NRF_CONSISTENCY, versions.ruuvi_json.nrf52_fw_ver, versions.metrics.nrf52_fw_ver,
                              extract_uart_version(console, NRF_VERSION_RE, "nRF52"))
            self._release(APPLICATION, ESP_REPO, versions.ruuvi_json.fw_ver)
            self._release(COPROCESSOR, NRF_REPO, versions.ruuvi_json.nrf52_fw_ver)
            verdict = "FAIL" if "FAIL" in self.outcomes.values() else "PASS"
            code = 1 if verdict == "FAIL" else 0
        except Exception as error:  # noqa: BLE001 - Run boundary records unexpected hardware/setup failures.
            error: Exception
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            self.evidence.exception(error)
            self._report(f"ERROR: {self.active}: {type(error).__name__}: {error}")
            skipped: list[str] = [check for check in CHECKS if self.outcomes[check] == "NOT RUN"]
            if skipped:
                self._report(f"NOT RUN: {', '.join(skipped)} (execution stopped)")
        self._report("INFORMATIONAL: mbedTLS, Web-UI, and nRF5 SDK versions are not verified at runtime")
        for check in CHECKS:
            self.evidence.write("COMPONENT RESULT", MechanismResultEvidence(check, self.outcomes[check]))
        return RunResult(code, verdict, dict(self.outcomes), set(), self.recovery_message)


def execute_test_5_5_2_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    serial_transport: SerialTransport | None = None,
    release_lookup: Callable[[str, str, EvidenceLog], ReleaseLookup] = lookup_release,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    root: Path = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(root / "logs", "test_5_5_2_2_a", now)
    output(f"Open log file: {log.path}")

    def output_progress(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
    serial: SerialTransport = serial_transport if serial_transport is not None else SerialTransport()
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    try:
        progress.step("Preflighting serial tools and loading .env")
        log.write("SERIAL TOOL VERSIONS", serial.preflight())
        config: DutConfig = load_dut_config(root / ".env")
        result = FunctionalTest_5_5_2_2_a(
            config, log, session_factory, serial, release_lookup, progress.step,
            output=output,
        ).run()
    except Exception as error:  # Execution boundary must log initialization failures too.
        error: Exception
        result.outcomes[SETUP] = "ERROR"
        log.exception(error)  # noqa: TRY401 - EvidenceLog requires the exception object.
        output_progress(f"ERROR: Initialization: {type(error).__name__}: {error}")
        output_progress("NOT RUN: Component checks (initialization failed)")
    finally:
        progress.step("Recording final component outcomes")
        check: str
        outcome: str
        for check, outcome in result.outcomes.items():
            log.write("FINAL RESULT", MechanismResultEvidence(check, outcome))
        if result.recovery_message is not None:
            log.write("USER ACTION REQUIRED", result.recovery_message)
            output(result.recovery_message)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_5_2_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
