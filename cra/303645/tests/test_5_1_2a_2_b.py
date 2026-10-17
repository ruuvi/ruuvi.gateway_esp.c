"""Live ETSI 5.1-2A-2 Unit B password-rejection test for a Ruuvi Gateway."""

from __future__ import annotations

import hashlib
import json
import re
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
    load_ui_default_config,
)
from lib.errors import GatewayAuthenticationModeError, GatewayProtocolError, InvalidSetup
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


# These hidden wire fields are emitted only when password hiding is disabled.
# Source: main/gw_cfg_json_generate_remote.c, token/apikey serializers.
HIDDEN_REMOTE_CREDENTIAL_FIELDS: tuple[str, ...] = ("remote_cfg_auth_token", "remote_cfg_auth_apikey")


def safe_error_reason(error: BaseException) -> str:
    """Never echo arbitrary protocol bodies, URLs, credentials, or OS filenames."""
    if isinstance(error, requests.RequestException):
        return "gateway transport request failed"
    if isinstance(error, OSError):
        return "evidence/output or local configuration unavailable"
    if isinstance(error, GatewayAuthenticationModeError):
        return "default interactive authentication is required"
    if isinstance(error, GatewayProtocolError):
        return "gateway response violates the required protocol or endpoint schema"
    return "required check could not be completed; see evidence for details"


class SecurityFailure(Exception):
    """A security assertion failed."""


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
        self.prepared_config: dict[str, Any] = {}
        self.ro_key: str | None = None
        self.rw_key: str | None = None
        self.mutation_possible: bool = False
        self.factory_reset_required: bool = False
        self.protected_response_fields: set[str] = set()
        self.diagnostics: list[str] = []
        self.active_check: str = "initial evidence preamble"

    def _step(self, description: str) -> None:
        self.active_check = description
        self.progress(description)

    @staticmethod
    def _assertion_diagnostic(description: str, actual: Any) -> str:
        # Only numeric/status and hash observations are safe for terminal output.
        observed: str = "redacted"
        if (type(actual) in (int, bool) or isinstance(actual, HashComparisonEvidence)
                or actual in ("protected data detected", "no protected data detected")):
            observed = str(actual)
        return f"{description}; expected: assertion satisfied; observed: {observed}"

    def _error_diagnostic(self, error: BaseException) -> None:
        self.diagnostics.append(f"{self.active_check}: {type(error).__name__}: {safe_error_reason(error)}")

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
            self.diagnostics.append(self._assertion_diagnostic(description, actual))
            raise InvalidSetup(f"{description} (actual: {actual!r})")

    def _require_security(
        self, condition: bool, description: str, actual: Any = "", mechanism: str | None = None
    ) -> None:
        self._record_assertion(description, condition, actual)
        if not condition:
            if mechanism is not None:
                self.outcomes[mechanism] = "FAIL"
            self.diagnostics.append(self._assertion_diagnostic(description, actual))
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
        # Identity/version fields can occur in public authentication metadata.
        # Other configuration fields and the history schema describe protected data.
        def field_names(node: Any) -> set[str]:
            fields: set[str] = set()
            child: Any
            if isinstance(node, dict):
                fields.update(node)
                for child in node.values():
                    fields.update(field_names(child))
            elif isinstance(node, list):
                for child in node:
                    fields.update(field_names(child))
            return fields

        self.protected_response_fields = (field_names(load_ui_default_config()) | field_names(payload)) - {
            GatewayCfgDesc.GW_MAC, GatewayCfgDesc.FW_VER, GatewayCfgDesc.NRF52_FW_VER,
        }
        self.protected_response_fields.update({
            "tags", "coordinates", "rssi", "ble_phy", "ble_chan",
            GatewayCfgDesc.LAN_AUTH_PASS, GatewayCfgDesc.LAN_AUTH_API_KEY,
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW,
            GatewayCfgDesc.PASSWORD, GatewayCfgDesc.HTTP_USER, GatewayCfgDesc.HTTP_PASS,
            GatewayCfgDesc.MQTT_PASS, GatewayCfgDesc.REMOTE_CFG_AUTH_BASIC_USER,
            GatewayCfgDesc.REMOTE_CFG_AUTH_BASIC_PASS, "http_bearer_token", "http_api_key",
            "http_stat_pass", "remote_cfg_auth_bearer_token",
            *HIDDEN_REMOTE_CREDENTIAL_FIELDS,
        })

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
        self.prepared_config = prepared
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

    def _require_no_protected_data(self, response: requests.Response, probe: NegativeProbe | PositiveProbe) -> None:
        def contains_protected_fields(node: Any) -> bool:
            if isinstance(node, dict):
                if self.protected_response_fields.intersection(node):
                    return True
                return any(contains_protected_fields(value) for value in node.values())
            if isinstance(node, list):
                return any(contains_protected_fields(value) for value in node)
            return False

        leaked: bool = False
        try:
            payload: Any = json.loads(response.text)
        except ValueError:
            # Denials may be empty, plain text, or HTML. Recognizable protected
            # JSON fields in an HTML wrapper or truncated reply still reveal data.
            field: str
            for field in self.protected_response_fields:
                if re.search(r'"' + re.escape(field) + r'"\s*:', response.text):
                    leaked = True
                    break
        else:
            leaked = contains_protected_fields(payload)
        self._require_security(
            not leaked,
            f"{probe.name} denial contains no protected data for {probe.method} {probe.path}",
            "protected data detected" if leaked else "no protected data detected",
            mechanism=probe.mechanism,
        )

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
                    f"{probe.name} returns expected HTTP {probe.expected_status} for {probe.method} {probe.path}",
                    response.status_code,
                )
                self._require_no_protected_data(response, probe)
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

    def _validate_positive_read(self, payload: dict[str, Any], probe: PositiveProbe) -> None:
        if probe.path == GatewayApi.CONFIG:
            if canonical_json_hash(payload) != self.prepared_hash:
                raise GatewayProtocolError("bearer GET /ruuvi.json does not match the prepared configuration")
            return
        data: Any = payload.get("data")
        if not isinstance(data, dict):
            raise GatewayProtocolError("bearer GET /history requires a data object")
        mac: Any = data.get(GatewayCfgDesc.GW_MAC)
        if (
            not isinstance(mac, str)
            or OCTETS_6_RE.fullmatch(mac) is None
            or normalize_mac(mac) != normalize_mac(self.config.gw_mac)
            or not isinstance(data.get("coordinates"), str)
            or not isinstance(data.get("tags"), dict)
        ):
            raise GatewayProtocolError("bearer GET /history has invalid identity, coordinates, or tags")
        # Firmware omits the envelope timestamp when NTP timestamps are disabled.
        if self.prepared_config.get("ntp_use", True) and type(data.get("timestamp")) is not int:
            raise GatewayProtocolError("bearer GET /history requires an integer timestamp")
        if "timestamp" in data and type(data["timestamp"]) is not int:
            raise GatewayProtocolError("bearer GET /history timestamp must be an integer")
        tags: dict[str, Any] = data["tags"]
        sensor_mac: str
        report: Any
        for sensor_mac, report in tags.items():
            if (
                OCTETS_6_RE.fullmatch(sensor_mac) is None
                or not isinstance(report, dict)
                or type(report.get("rssi")) is not int
                or type(report.get("ble_chan")) is not int
                or ("ble_phy" in report and report["ble_phy"] not in ("1M", "2M", "Coded"))
                or not isinstance(report.get("data"), str)
                or re.fullmatch(r"(?:[0-9A-Fa-f]{2})+", report["data"]) is None
                or type(report.get("timestamp" if self.prepared_config.get("ntp_use", True) else "counter")) is not int
            ):
                raise GatewayProtocolError("bearer GET /history contains a malformed sensor report")

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
                    f"{probe.name} has the required scope (expected HTTP {probe.expected_status}) for {probe.method} {probe.path}",
                    response.status_code,
                )
                if probe.expected_status == HttpStatus.C_401_UNAUTHORIZED:
                    self._require_no_protected_data(response, probe)
            except SecurityFailure:
                self.outcomes[probe.mechanism] = "FAIL"
                raise
            if response.status_code == HttpStatus.C_200_OK:
                payload: dict[str, Any] = self.gateway.response_json(
                    response, f"{probe.name} {probe.method} {probe.path}", dict
                )
                if probe.method == HttpMethod.POST and payload != {}:
                    raise GatewayProtocolError("successful RW POST /ruuvi.json must return an empty JSON object")
                if probe.method == HttpMethod.GET:
                    self._validate_positive_read(payload, probe)
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
                current_ro: dict[str, Any] = self._read_config(self.admin_session, "post-RO-denial GET /ruuvi.json")
                current_ro_hash: str = canonical_json_hash(current_ro)
                self._require_security(
                    current_ro_hash == self.prepared_hash,
                    "denied RO POST did not change configuration before the RW write",
                    HashComparisonEvidence(baseline=self.prepared_hash or "", final=current_ro_hash),
                    mechanism=AuthMech.M2M_API_BEARER_RO,
                )
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
                self._require_security(
                    not 200 <= response.status_code <= 299,
                    f"revoked {name} bearer is denied (expected HTTP 401)",
                    response.status_code,
                    mechanism=AuthMech.M2M_API_BEARER_RO if name == "RO" else AuthMech.M2M_API_BEARER_RW,
                )
                self._require_setup(
                    response.status_code == HttpStatus.C_401_UNAUTHORIZED,
                    f"temporary {name} key no longer authorizes",
                    response.status_code,
                )
                self._require_no_protected_data(response, NegativeProbe(
                    f"revoked {name} bearer", method, path, HttpStatus.C_401_UNAUTHORIZED,
                    AuthMech.M2M_API_BEARER_RO if name == "RO" else AuthMech.M2M_API_BEARER_RW,
                    None, None,
                ))
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            self._error_diagnostic(error)
            self.evidence.exception(error)
            self.outcomes["final restoration and non-mutation"] = "ERROR"
            return False
        self.outcomes["final restoration and non-mutation"] = "PASS"
        return True

    def run(self) -> RunResult:
        verdict: str = "ERROR"
        exit_code: int = 2
        failure: BaseException | None = None
        recovery_evidence: RecoveryEvidence = RecoveryEvidence(self.evidence)
        try:
            self.evidence.write("TEST CASE AND UNIT", TEST_ID)
            self.evidence.write("UTC START", format_utc(self.evidence.started_at))
            self.evidence.write("DUT CONFIGURATION", self.config)
            self.evidence.write(
                "SCOPE BOUNDARY",
                "Interactive Web-UI access control, unconfigured-secret boundaries, rate limiting, "
                "and Unit A interface discovery are out of scope.",
            )
            self._step("Authenticating with the default administrative credentials")
            login: InteractiveAuthResult = self._authenticate_admin()
            self.admin_session = login.session
            self._step("Reading and validating the baseline gateway configuration")
            baseline: dict[str, Any] = self._read_config(self.admin_session, "baseline GET /ruuvi.json")
            self._validate_baseline(baseline, login.auth_payload)
            self.baseline_hash = canonical_json_hash(baseline)
            self.evidence.write("BASELINE CONFIGURATION SHA256", self.baseline_hash)
            self._step("Provisioning temporary RO and RW bearer keys")
            try:
                self._provision()
                self._step("Verifying the prepared temporary state")
                self._verify_prepared()
            except Exception:
                self.outcomes["temporary-state setup"] = "ERROR"
                raise
            self._step("Testing rejection of password schemes by M2M read routes")
            self._run_negative_matrix(login.challenge["realm"], HttpMethod.GET)
            self._step("Testing positive RO and RW bearer read scope")
            self._run_positive_matrix(HttpMethod.GET)
            self._step("Testing rejection of password schemes by M2M write routes")
            self._run_negative_matrix(login.challenge["realm"], HttpMethod.POST)
            self._step("Testing positive RO and RW bearer write scope")
            self._run_positive_matrix(HttpMethod.POST)
        except SecurityFailure as error:
            error: Exception
            failure = error
            recovery_evidence.exception(error)
            verdict = "FAIL"
            exit_code = 1
        except Exception as error:  # noqa: BLE001 - Log unexpected failures and preserve ERROR/recovery behavior.
            failure = error
            self._error_diagnostic(error)
            recovery_evidence.exception(error)
            verdict = "ERROR"
            exit_code = 2
        finally:
            # GatewayClient also logs before transmission. Replace both evidence
            # references so a broken sink cannot prevent cleanup requests.
            self.evidence = recovery_evidence
            self.gateway.evidence = recovery_evidence
            recovery_evidence.attempt(lambda: self._step("Restoring the original API-key configuration"))
            try:
                restoration_ok: bool = self._restore()
            except Exception as restoration_error:  # noqa: BLE001 - Retain manual recovery on unexpected failures.
                restoration_error: Exception
                self._error_diagnostic(restoration_error)
                recovery_evidence.exception(restoration_error)
                self.outcomes["final restoration and non-mutation"] = "ERROR"
                restoration_ok = False
            recovery_evidence.attempt(lambda: self._step("Verifying restoration and aggregating the verdict"))
            if not restoration_ok:
                self.factory_reset_required = True
                self.diagnostics.append("Restoration ERROR: original state and credential revocation could not be verified")
                verdict = "ERROR"
                exit_code = 2
            elif failure is None:
                verdict = "PASS"
                exit_code = 0
        if recovery_evidence.failed:
            verdict = "ERROR"
            exit_code = 2
        if recovery_evidence.failed:
            self.diagnostics.append("Recovery evidence/output ERROR: one or more observations could not be recorded")
        diagnostic: str
        for diagnostic in self.diagnostics:
            self.evidence.write("TERMINAL DIAGNOSTIC", diagnostic)
        outcome: str
        mechanism: str
        for mechanism, outcome in self.outcomes.items():
            self.evidence.write(
                "FINAL RESULT",
                MechanismResultEvidence(mechanism=mechanism, result=outcome),
            )
        self.evidence.write("OVERALL RESULT", verdict)
        if recovery_evidence.failed:
            verdict = "ERROR"
            exit_code = 2
            if not any("evidence/output ERROR" in diagnostic for diagnostic in self.diagnostics):
                self.diagnostics.append("Final evidence/output ERROR: result observations could not be recorded")
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
    log: EvidenceLog
    final_evidence: RecoveryEvidence | None = None
    runner: FunctionalTest_5_1_2a_2_b | None = None
    diagnostics: list[str] = []
    result: RunResult = RunResult(2, "ERROR", {mechanism: "NOT RUN" for mechanism in MECHANISMS}, set())

    def terminal(message: str) -> bool:
        nonlocal result
        try:
            output(message)
            return True
        except Exception as output_error:  # noqa: BLE001 - An unavailable output callback must remain infrastructure ERROR.
            output_error: Exception
            result.exit_code = 2
            result.verdict = "ERROR"
            reason: str = f"Terminal output ERROR: {type(output_error).__name__}: output callback unavailable"
            if final_evidence is not None:
                final_evidence.write("TERMINAL OUTPUT ERROR", reason)
            try:
                output(reason)
            except Exception:  # noqa: BLE001 - A permanently broken output sink cannot be repaired here.
                return False
            return False

    try:
        log = EvidenceLog.create(work_dir / "logs", "test_5_1_2a_2_b", now)
        final_evidence = RecoveryEvidence(log)
        output(f"Open log file: {log.path}")

        def output_progress(message: str) -> None:
            output(message)
            log.write_line(message)

        progress: ProgressReporter = ProgressReporter(output_progress, TOTAL_STEPS)
        log.write("TEST CASE AND UNIT", TEST_ID)
        log.write("UTC START", format_utc(log.started_at))
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        log.write("DUT CONFIGURATION", config)
        active_runner: FunctionalTest_5_1_2a_2_b = FunctionalTest_5_1_2a_2_b(
            config, log, session_factory=session_factory, random_bytes=random_bytes, progress=progress.step,
        )
        runner = active_runner
        result = active_runner.run()
    except Exception as error:  # noqa: BLE001 - The entire entrypoint, including log creation, returns ERROR/2.
        error: Exception
        diagnostics.append(f"Execution setup ERROR: {type(error).__name__}: {safe_error_reason(error)}")
        if final_evidence is not None:
            final_evidence.exception(error)
    finally:
        if runner is not None:
            diagnostics = runner.diagnostics + diagnostics
        if result.verdict != "PASS":
            mechanism: str
            outcome: str
            for mechanism, outcome in result.outcomes.items():
                if outcome == "NOT RUN":
                    diagnostics.append(f"{mechanism}: NOT RUN")
        diagnostic: str
        for diagnostic in diagnostics:
            if final_evidence is not None:
                final_evidence.write("TERMINAL DIAGNOSTIC", diagnostic)
            terminal(diagnostic)
        if final_evidence is not None:
            if result.recovery_message is not None:
                final_evidence.write("USER ACTION REQUIRED", result.recovery_message)
            if final_evidence.failed:
                result.exit_code = 2
                result.verdict = "ERROR"
            # Deliver the verdict before finishing so terminal-output failure is
            # reflected in the final evidence verdict and returned result.
            if not terminal(f"Overall verdict: {result.verdict}"):
                terminal("Overall verdict: ERROR")
            if result.recovery_message is not None:
                terminal(result.recovery_message)
            final_log: EvidenceLog = final_evidence.source
            final_evidence.attempt(lambda: final_log.finish(result.verdict, now))
            if final_evidence.failed:
                result.exit_code = 2
                result.verdict = "ERROR"
                final_evidence.write("FINALIZATION ERROR", "evidence/output could not be completed")
                final_evidence.write("OVERALL VERDICT", "ERROR")
                terminal("Evidence finalization ERROR: evidence/output could not be completed")
                terminal("Overall verdict: ERROR")
                if result.recovery_message is not None:
                    terminal(result.recovery_message)
            if not final_log._stream.closed:
                close_failed_before: bool = final_evidence.failed
                final_evidence.attempt(final_log._stream.close)
                if final_evidence.failed and not close_failed_before:
                    result.exit_code = 2
                    result.verdict = "ERROR"
                    terminal("Evidence close ERROR: evidence stream could not be closed")
                    terminal("Overall verdict: ERROR")
                    if result.recovery_message is not None:
                        terminal(result.recovery_message)
        else:
            terminal("Overall verdict: ERROR")
    return result


def main() -> int:
    return execute_test_5_1_2a_2_b().exit_code


if __name__ == "__main__":
    sys.exit(main())
