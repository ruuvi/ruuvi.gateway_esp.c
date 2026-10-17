"""Standalone ETSI 5.11-1-2 Unit A: data creation and operator-confirmed reset recovery.

No CLI arguments; load .env in the working directory. PASS requires every required
item committed AND verified factory-reset recovery. This is not physical-erasure
proof and never leaves the device seeded for Unit C.

The adjacent *_manifest_<UTC>.json has schema_version=1, test_case, unit,
utc_created, gw_mac, gw_hostname, baseline={sha256,payload}, items (category,
location, field_or_file, value, sha256, committed, verification, reason,
readback_sha256), outcomes and restored={attempted,verified,sha256,reason}.
currently_seeded is false after verified restoration, otherwise null (unknown).
Unit C must independently establish its seed.
An initial checkpoint precedes mutations; the final atomic replacement includes
restoration. An interrupted checkpoint has restored.verified=false.

Firmware limitations are evidence, never implicit success: LAN /connect.json is
forbidden. Prompt for the configuration hotspot at 10.10.0.1 and verify its nRF52
identity before writing. A fresh successful station status plus saved-SSID readback
provides indirect Wi-Fi password proof on the configured protected test network.
Hidden target passwords are checked with use_saved_password=true: local HTTP
credential witnesses and the authenticated public Mosquitto test listener.
RO/RW bearer proof applies only to those API keys. Certificates are NVS strings in current firmware: /extra_cfg may
return 403, so use storage.<filename> (not a TLS enable switch) as presence proof.
Only readable blobs get readback hashes. The generated default snapshot's MQTT
prefix/client ID contain a placeholder MAC; substitute the verified DUT MAC when
checking these defaults and retain their stable values in the complete baseline.
Print bench requirements before DUT access: start on LAN, with the hotspot closed.
The operator confirms CONFIGURE and host network changes at explicit prompts.
The runner performs API transitions and verifies identity after reconnecting.
Hotspot-only access cannot verify LAN authentication.
"""

from __future__ import annotations

import hashlib
import ipaddress
import json
import secrets
import sys
import time
from dataclasses import asdict, dataclass, fields, replace
from datetime import datetime, timedelta
from functools import partial
from pathlib import Path
from typing import Any, Callable, TextIO
from urllib.parse import urlsplit

import requests
from Crypto.Hash import SHA256
from Crypto.IO import PEM
from Crypto.PublicKey import ECC
from Crypto.Signature import DSS
from Crypto.Util.asn1 import DerBitString, DerObjectId, DerSequence, DerSetOf

from fixture_5_11_1_2_a import HttpFixture, HttpRoute, LocalHttpFixture
from lib.config import (
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    InvalidConfig,
    default_config_values,
    load_dut_config,
    load_ui_default_config,
)
from lib.errors import GatewayAuthenticationModeError, InvalidSetup
from lib.evidence import EvidenceLog, format_utc, utc_now
from lib.gateway import (
    GatewayCfgDesc as Cfg,
)
from lib.gateway import (
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthChallenge,
    InteractiveAuthResult,
    InteractiveChallengeRequest,
)
from lib.gateway import (
    GatewayStorageFile as StorageFile,
)
from lib.http_api import GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult

TEST_CASE: str = "5.11-1-2"
PREFIX: str = "test_5_11_1_2_a"
WIFI: str = "Wi-Fi STA credentials"
ADMIN: str = "Web-UI admin password"
KEYS: str = "M2M API keys RO + RW"
HTTP: str = "Custom HTTP telemetry target + secret"
MQTT: str = "Custom MQTT target + secret"
REMOTE: str = "Remote-config URL + secret"
CERTS: str = "User SSL/mTLS certificates + private keys"
CATEGORIES: tuple[str, ...] = (WIFI, ADMIN, KEYS, HTTP, MQTT, REMOTE, CERTS)
CHECKS: tuple[str, ...] = ("baseline", *CATEGORIES, "state preservation", "factory reset", "manifest")
CERTIFICATE_FILES: tuple[str, ...] = (
    StorageFile.HTTP_CLI_CERT, StorageFile.HTTP_CLI_KEY, StorageFile.HTTP_SRV_CERT,
    StorageFile.MQTT_CLI_CERT, StorageFile.MQTT_CLI_KEY, StorageFile.MQTT_SRV_CERT,
    StorageFile.STAT_CLI_CERT, StorageFile.STAT_CLI_KEY, StorageFile.RCFG_CLI_CERT, StorageFile.RCFG_CLI_KEY,
)
# Device metadata and MAC-derived MQTT defaults differ from the generator's
# placeholder device. Check MAC-derived defaults separately after identity verification.
DEVICE_DERIVED_FIELDS: tuple[str, ...] = (
    Cfg.FW_VER, Cfg.NRF52_FW_VER, Cfg.GW_MAC, Cfg.MQTT_PREFIX, Cfg.MQTT_CLIENT_ID,
)
REDACTED_FIELDS: tuple[str, ...] = (
    Cfg.LAN_AUTH_PASS, Cfg.LAN_AUTH_API_KEY, Cfg.LAN_AUTH_API_KEY_RW,
    Cfg.HTTP_PASS, Cfg.MQTT_PASS, Cfg.REMOTE_CFG_AUTH_BASIC_PASS,
)


class CommitmentFailure(Exception):
    """An accepted write has a complete, inconsistent observation."""


@dataclass(frozen=True)
class ScalarFixture:
    category: str
    field: str
    value: str | int | bool


@dataclass(frozen=True)
class PemFixture:
    filename: str
    pem: str


@dataclass(frozen=True)
class SyntheticData:
    username: str
    password: str
    ssid: str
    wifi_password: str
    ro_key: str
    rw_key: str
    scalars: tuple[ScalarFixture, ...]
    certificates: tuple[PemFixture, ...]


@dataclass(frozen=True)
class ManifestItem:
    category: str
    location: str
    field_or_file: str
    value: str | int | bool
    sha256: str
    committed: bool = False
    verification: str = "unverified"
    reason: str = "Not yet verified"
    readback_sha256: str | None = None


@dataclass(frozen=True)
class Baseline:
    sha256: str
    # An immutable snapshot; decoded only at JSON request/serialization boundaries.
    payload_json: str


@dataclass(frozen=True)
class Restoration:
    attempted: bool = False
    verified: bool = False
    sha256: str | None = None
    reason: str = "Not yet verified"


@dataclass(frozen=True)
class CategoryResult:
    category: str
    verdict: str


@dataclass(frozen=True)
class Manifest:
    utc_created: str
    gw_mac: str
    gw_hostname: str
    baseline: Baseline | None
    items: tuple[ManifestItem, ...]
    restored: Restoration
    outcomes: tuple[CategoryResult, ...]


def canonical_hash(payload: Any) -> str:
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def typed_equal(actual: Any, expected: Any) -> bool:
    """Compare a decoded JSON value recursively, including bool/int distinctions."""
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return actual.keys() == expected.keys() and all(typed_equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(typed_equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


def write_manifest(path: Path, manifest: Manifest) -> None:
    """Atomic checkpoints. A failed final write leaves the unverified checkpoint."""
    payload: dict[str, Any] = asdict(manifest)
    payload.update(
        schema_version=1, test_case=TEST_CASE, unit="A",
        currently_seeded=False if manifest.restored.verified else None,
    )
    if manifest.baseline is not None:
        payload["baseline"] = {
            "sha256": manifest.baseline.sha256,
            "payload": json.loads(manifest.baseline.payload_json),
        }
    temporary: Path = path.with_suffix(".json.tmp")
    stream: TextIO
    try:
        with temporary.open("w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
        temporary.replace(path)
    finally:
        if temporary.exists():
            temporary.unlink()


def self_signed_pem(random_bytes: Callable[[int], bytes], now: datetime) -> tuple[str, str]:
    """Build an X.509 v1 P-256 certificate using only PyCryptodome and stdlib."""
    key: ECC.EccKey = ECC.generate(curve="P-256", randfunc=random_bytes)
    algorithm: bytes = DerSequence([DerObjectId("1.2.840.10045.4.3.2").encode()]).encode()
    common_name: bytes = b"CRA synthetic fixture"
    utf8_name: bytes = bytes((12, len(common_name))) + common_name
    name: bytes = DerSequence([
        DerSetOf([DerSequence([DerObjectId("2.5.4.3").encode(), utf8_name]).encode()]).encode(),
    ]).encode()
    start: bytes = (now - timedelta(days=1)).strftime("%Y%m%d%H%M%SZ").encode()
    end: bytes = (now + timedelta(days=30)).strftime("%Y%m%d%H%M%SZ").encode()
    validity: bytes = DerSequence([bytes((24, len(start))) + start, bytes((24, len(end))) + end]).encode()
    serial: int = int.from_bytes(random_bytes(16), "big") or 1
    tbs: bytes = DerSequence([
        serial, algorithm, name, validity, name, key.public_key().export_key(format="DER"),
    ]).encode()
    signature: bytes = DSS.new(key, "deterministic-rfc6979", encoding="der").sign(SHA256.new(tbs))
    certificate: bytes = DerSequence([tbs, algorithm, DerBitString(signature).encode()]).encode()
    return PEM.encode(certificate, "CERTIFICATE"), key.export_key(format="PEM")


def generate_fixtures(
    realm: str, random_bytes: Callable[[int], bytes], now: datetime,
) -> SyntheticData:
    def token(size: int = 12) -> str:
        return random_bytes(size).hex()

    username: str = f"cra-{token(6)}"
    password: str = token()
    ssid: str = f"cra-{token(8)}"
    wifi_password: str = token()
    ro: str = token(16)
    rw: str = token(16)
    scalars: tuple[ScalarFixture, ...] = (
        ScalarFixture(ADMIN, Cfg.LAN_AUTH_TYPE, GatewayCfgLanAuthType.RUUVI),
        ScalarFixture(ADMIN, Cfg.LAN_AUTH_USER, username),
        ScalarFixture(ADMIN, Cfg.LAN_AUTH_PASS, GatewayClient.calculate_digest_ha1(username, realm, password)),
        ScalarFixture(KEYS, Cfg.LAN_AUTH_API_KEY, ro),
        ScalarFixture(KEYS, Cfg.LAN_AUTH_API_KEY_RW, rw),
        ScalarFixture(HTTP, Cfg.USE_HTTP, True),
        ScalarFixture(HTTP, Cfg.HTTP_URL, f"https://127.0.0.1:18443/{token()}/telemetry"),
        ScalarFixture(HTTP, Cfg.HTTP_AUTH, "basic"),
        ScalarFixture(HTTP, Cfg.HTTP_USER, f"http-{token(6)}"),
        ScalarFixture(HTTP, Cfg.HTTP_PASS, token()),
        ScalarFixture(MQTT, Cfg.USE_MQTT, True),
        ScalarFixture(MQTT, Cfg.MQTT_SERVER, "127.0.0.1"),
        ScalarFixture(MQTT, Cfg.MQTT_PORT, 18883),
        ScalarFixture(MQTT, Cfg.MQTT_USER, f"mqtt-{token(6)}"),
        ScalarFixture(MQTT, Cfg.MQTT_PASS, token()),
        ScalarFixture(MQTT, Cfg.MQTT_PREFIX, f"cra/{token()}/"),
        # Store remote assets without starting the remote configuration worker.
        ScalarFixture(REMOTE, Cfg.REMOTE_CFG_USE, False),
        ScalarFixture(REMOTE, Cfg.REMOTE_CFG_URL, f"https://127.0.0.1:18443/{token()}/config"),
        ScalarFixture(REMOTE, Cfg.REMOTE_CFG_AUTH_TYPE, "basic"),
        ScalarFixture(REMOTE, Cfg.REMOTE_CFG_AUTH_BASIC_USER, f"rcfg-{token(6)}"),
        ScalarFixture(REMOTE, Cfg.REMOTE_CFG_AUTH_BASIC_PASS, token()),
    )
    cert: str
    key: str
    cert, key = self_signed_pem(random_bytes, now)
    certificates: tuple[PemFixture, ...] = tuple(
        PemFixture(filename, key if filename.endswith("_key") else cert) for filename in CERTIFICATE_FILES
    )
    return SyntheticData(username, password, ssid, wifi_password, ro, rw, scalars, certificates)


class FunctionalTest_5_11_1_2_a:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        now: Callable[[], datetime] = utc_now,
        progress: Callable[[str], None] = lambda message: None,
        output: Callable[[str], None] = lambda message: None,
        fixture_factory: Callable[[str, Callable[[int], bytes], datetime], SyntheticData] = generate_fixtures,
        manifest_writer: Callable[[Path, Manifest], None] = write_manifest,
        hotspot_hostname: str = "10.10.0.1",
        input_fn: Callable[[str], str] = input,
        monotonic: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
        http_fixture_factory: Callable[[str, tuple[HttpRoute, ...]], HttpFixture] = LocalHttpFixture,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.session_factory: Callable[[], requests.Session] = session_factory
        self.sessions: list[requests.Session] = []
        self.had_error: bool = False
        self.gateway: GatewayClient = GatewayClient(config, evidence, self._new_transport, random_bytes, ecc_generate=self._ecc_generate)
        self.hotspot: GatewayClient = GatewayClient(
            replace(config, gw_hostname=hotspot_hostname), evidence, self._new_transport, random_bytes, ecc_generate=self._ecc_generate,
        )
        self.random_bytes: Callable[[int], bytes] = random_bytes
        self.now: Callable[[], datetime] = now
        self.progress: Callable[[str], None] = progress
        self.output: Callable[[str], None] = output
        self.fixture_factory: Callable[[str, Callable[[int], bytes], datetime], SyntheticData] = fixture_factory
        self.manifest_writer: Callable[[Path, Manifest], None] = manifest_writer
        self.manifest_path: Path = evidence.path.with_name(evidence.path.name.replace(PREFIX, f"{PREFIX}_manifest")).with_suffix(".json")
        self.baseline: Baseline | None = None
        self.fixtures: SyntheticData | None = None
        self.items: list[ManifestItem] = []
        self.outcomes: dict[str, str] = dict.fromkeys(CHECKS, "NOT RUN")
        self.restoration: Restoration = Restoration()
        self.recovery_message: str | None = None
        self.active: str = "baseline"
        self.config_attempted: bool = False
        self.wifi_attempted: bool = False
        self.wifi_unchanged_verified: bool = False
        self.wifi_connection_verified: bool = False
        self.wifi_creation_verified: bool = False
        self.attempted_files: list[str] = []
        self.accepted_files: set[str] = set()
        self.session: requests.Session | None = None
        self.hotspot_session: requests.Session | None = None
        self.input_fn: Callable[[str], str] = input_fn
        self.monotonic: Callable[[], float] = monotonic
        self.sleep: Callable[[float], None] = sleep
        self.http_fixture_factory: Callable[[str, tuple[HttpRoute, ...]], HttpFixture] = http_fixture_factory
        self.http_fixture: HttpFixture | None = None
        self.identity_conflict: bool = False
        self.lan_ready: bool = True
        self.input_closed: bool = False

    def _ecc_generate(self, *, curve: str) -> ECC.EccKey:
        return ECC.generate(curve=curve, randfunc=self.random_bytes)

    def _new_transport(self) -> requests.Session:
        session: requests.Session = self.session_factory()
        self.sessions.append(session)
        return session

    def _say(self, message: str) -> None:
        self.output(message)
        self.evidence.write_line(message)

    def _report_bench_requirements(self) -> None:
        self._say(
            "BENCH PREPARATION: This runner requires factory-default Ethernet settings "
            "and no saved Wi-Fi station credentials, with .env gw_hostname reachable over LAN."
        )
        self._say(
            "Start on LAN with the configuration hotspot closed. "
            "Opening it with CONFIGURE disconnects the Gateway's active LAN interface."
        )
        self._say(
            "Hotspot-only access cannot verify LAN passwords or API keys: hotspot requests bypass authentication. "
            f"Changing gw_hostname to {self.hotspot.config.gw_hostname} will not fix that."
        )
        self._say(
            "Full Wi-Fi creation and cleanup require coordinated LAN/hotspot transitions. "
            "The runner pauses for you to press CONFIGURE and switch the host network, then uses /connect.json."
        )
        self._say(
            "Preconfiguring station Wi-Fi violates the empty-credentials baseline. "
            "Start on Ethernet; keep its cable connected. Confirm each network-change prompt to continue."
        )
        self._say(
            "Factory-reset recovery replaces API cleanup at the end; it erases local settings and "
            "requires your LED confirmation, hotspot connection and return to LAN."
        )
        self._say(
            "Wi-Fi persistence requires a successful station connection to an available 2.4 GHz test AP. "
            "Set wifi_ssid and wifi_password in .env to that network's credentials. "
            "The runner provisions them but does not create the AP; "
            "accepted provisioning alone cannot prove they were saved."
        )

    def _confirm(self, instruction: str) -> None:
        self._say(instruction)
        if self.input_closed:
            raise InvalidSetup("Operator input is unavailable; network transition cannot be confirmed")
        while True:
            try:
                answer: str = self.input_fn("Press Enter when ready, or type q then Enter to stop: ").strip().lower()
            except (EOFError, KeyboardInterrupt) as error:
                error: BaseException
                self.input_closed = True
                raise InvalidSetup("Operator input ended; network transition was not confirmed") from error
            if answer in ("", "yes"):
                self.evidence.write_line("OPERATOR CONFIRMATION: ready")
                return
            if answer == "q":
                raise InvalidSetup("Operator cancelled the transition; required recovery will still be attempted")
            self._say("Press Enter to continue, or type q then Enter to stop and attempt recovery.")

    def _open_hotspot(self, purpose: str) -> dict[str, Any]:
        self.lan_ready = False
        ssid: str = f"Configure Ruuvi Gateway {self.config.gw_mac.replace(':', '')[-4:].upper()}"
        self._confirm(
            f"OPERATOR ACTION - {purpose}: briefly press and release CONFIGURE (well before five seconds), "
            f"then select the Wi-Fi network '{ssid}' on this host. "
            "Wait for DHCP to assign the host an IP address. "
            f"The script will contact the Gateway API at {self.hotspot.config.base_url} after you confirm. "
            "If already connected to that Wi-Fi network, stay connected. Close any Gateway browser pages."
        )
        self.hotspot_session = self.hotspot.new_session()
        assert self.hotspot_session is not None
        payload: dict[str, Any] = self._read(self.hotspot, self.hotspot_session)
        if not self._identity(payload):
            raise InvalidSetup("Hotspot identity missing")
        return payload

    def _wait_for_lan(self, default_auth: bool = False) -> None:
        self._say("Waiting for authenticated LAN access (up to 90 seconds, plus any in-flight HTTP timeout)...")
        deadline: float = self.monotonic() + 90.0
        while True:
            try:
                session: requests.Session = self._restore_session() if self.config_attempted and not default_auth else self._login("Admin", self.config.gw_id).session
                payload: dict[str, Any] = self._read(self.gateway, session)
            except Exception as error:  # Bounded reconnect retries after network changes.
                error: Exception
                self.evidence.exception(error)
                if self.identity_conflict:
                    raise
                if self.monotonic() >= deadline:
                    raise InvalidSetup("LAN did not recover; check host network, Ethernet cable and .env gw_hostname") from error
                self.sleep(2.0)
                continue
            if not self._identity(payload):
                raise InvalidSetup("LAN identity missing after network transition")
            if payload.get(Cfg.USE_ETH) is not True:
                raise InvalidSetup("Ethernet configuration was not restored after network transition")
            self.session = session
            self.lan_ready = True
            return

    def _return_to_lan(self, default_auth: bool = False) -> None:
        assert self.hotspot_session is not None and self.baseline is not None
        payload: dict[str, Any] = self._read(self.hotspot, self.hotspot_session)
        if not self._identity(payload):
            raise InvalidSetup("Hotspot identity missing before Ethernet reconnection")
        baseline: dict[str, Any] = json.loads(self.baseline.payload_json)
        network: dict[str, Any] = {k: v for k, v in baseline.items() if k == Cfg.USE_ETH or k.startswith("eth_")}
        try:
            response: requests.Response = self.hotspot.request(self.hotspot_session, HttpMethod.POST, GatewayApi.CONFIG, json_body=network)
            self._ok(response, "Restore Ethernet settings through hotspot")
            try:
                response = self.hotspot.request(self.hotspot_session, HttpMethod.POST, GatewayApi.CONNECT, json_body={Cfg.SSID: None, Cfg.PASSWORD: None})
                self._ok(response, "Reconnect Ethernet through hotspot")
            finally:
                # Finish even if the Ethernet request applied but its reply was lost.
                # An empty application POST resets MQTT fields on this firmware.
                finish: dict[str, str] = {}
                field: str
                for field in (Cfg.MQTT_PREFIX, Cfg.MQTT_CLIENT_ID, Cfg.MQTT_DATA_FORMAT):
                    value: Any = payload.get(field)
                    if not isinstance(value, str) or not value:
                        raise InvalidSetup(f"Cannot finish configuration: missing or malformed {field}")
                    finish[field] = value
                self._say("Finishing configuration through POST /ruuvi.json; the hotspot should close in about five seconds.")
                response = self.hotspot.request(self.hotspot_session, HttpMethod.POST, GatewayApi.CONFIG, json_body=finish)
                self._ok(response, "Finish configuration and deactivate hotspot")
        finally:
            # A lost reply can mean that the connection change already took effect.
            self._confirm(
                "OPERATOR ACTION - return to LAN: disconnect this host from the Gateway hotspot and "
                "connect it to the same normal LAN as the Gateway's Ethernet cable. "
                "Do not press CONFIGURE. Close Gateway browser pages while the hotspot shuts down."
            )
            self._wait_for_lan(default_auth)

    def _read_wifi_after_connect(self) -> dict[str, Any]:
        assert self.hotspot_session is not None and self.fixtures is not None
        deadline: float = self.monotonic() + 30.0
        self._say("Waiting up to 30 seconds for Wi-Fi configuration readback through the hotspot...")
        while True:
            try:
                payload: dict[str, Any] = self._read(self.hotspot, self.hotspot_session)
            except Exception as error:  # Reconnect can temporarily interrupt hotspot traffic.
                error: Exception
                self.evidence.exception(error)
                if self.identity_conflict:
                    raise
                if self.monotonic() >= deadline:
                    raise InvalidSetup("Hotspot readback did not recover after Wi-Fi provisioning") from error
            else:
                if not self._identity(payload):
                    raise InvalidSetup("Hotspot identity missing after Wi-Fi provisioning")
                wifi: Any = payload.get(Cfg.WIFI_STA_CONFIG)
                if isinstance(wifi, dict) and wifi.get(Cfg.SSID) == self.fixtures.ssid:
                    self.wifi_connection_verified = self._wifi_status_connected()
                    if self.wifi_connection_verified:
                        return payload
                if self.monotonic() >= deadline:
                    return payload
            self.sleep(2.0)

    def _wifi_status_connected(self) -> bool:
        """Check this provisioning attempt, whose POST clears the previous network status."""
        assert self.hotspot_session is not None and self.fixtures is not None
        response: requests.Response = self.hotspot.request(self.hotspot_session, HttpMethod.GET, GatewayApi.STATUS)
        self._ok(response, "Wi-Fi station status")
        status: dict[str, Any] = self.hotspot.response_json(response, "Wi-Fi station status", dict)
        if type(status.get("urc")) is not int or not isinstance(status.get("ip"), str):
            raise InvalidSetup("Wi-Fi status is missing a valid integer urc or string ip")
        # UPDATE_CONNECTION_OK=0 in wifi_manager_defs.h; Ethernet has ssid=null.
        if status["urc"] != 0 or status.get(Cfg.SSID) != self.fixtures.ssid:
            return False
        try:
            address: ipaddress.IPv4Address = ipaddress.IPv4Address(status["ip"])
        except ipaddress.AddressValueError as error:
            error: ipaddress.AddressValueError
            raise InvalidSetup("Wi-Fi status contains an invalid station IPv4 address") from error
        return not (address.is_unspecified or address.is_loopback or address.is_multicast)

    def _checkpoint(self) -> None:
        manifest: Manifest = Manifest(
            format_utc(self.evidence.started_at), self.config.gw_mac, self.config.gw_hostname,
            self.baseline, tuple(self.items), self.restoration,
            tuple(CategoryResult(k, v) for k, v in self.outcomes.items()),
        )
        self.evidence.write("MANIFEST PATH", str(self.manifest_path))
        self.manifest_writer(self.manifest_path, manifest)

    @staticmethod
    def _ok(response: requests.Response, description: str) -> None:
        if response.status_code != HttpStatus.C_200_OK:
            raise InvalidSetup(f"{description}: HTTP {response.status_code}")

    def _read(self, client: GatewayClient, session: requests.Session) -> dict[str, Any]:
        if self.identity_conflict:
            raise InvalidSetup("DUT identity is unverified or mismatched; stopping configuration access")
        response: requests.Response = client.request(session, HttpMethod.GET, GatewayApi.CONFIG)
        self._ok(response, "Configuration read")
        payload: dict[str, Any] = client.response_json(response, "configuration", dict)
        if not self._identity(payload):
            raise InvalidSetup("Missing nRF52 identity; stopping configuration access")
        return payload

    def _identity(self, payload: dict[str, Any]) -> bool:
        mac: Any = payload.get(Cfg.GW_MAC)
        if Cfg.GW_MAC not in payload:
            self.identity_conflict = True
            return False
        if not isinstance(mac, str) or not OCTETS_6_RE.fullmatch(mac) or mac.upper() != self.config.gw_mac.upper():
            self.identity_conflict = True
            raise InvalidSetup("Wrong or malformed nRF52 identity; verify DUT selection")
        return True

    def _login(self, username: str, password: str, default: bool = False) -> InteractiveAuthChallenge:
        request: InteractiveChallengeRequest = self.gateway.prepare_interactive_challenge_request()
        response: requests.Response = self.gateway.send_interactive_challenge_request(request)
        if response.status_code == HttpStatus.C_302_FOUND:
            if response.headers.get(HttpHeader.LOCATION) == f"{self.hotspot.config.base_url}/":
                raise InvalidSetup(
                    "GET /auth returned HTTP 302 to the configuration hotspot (captive portal). "
                    "Close the hotspot and restore LAN access to .env gw_hostname before starting. "
                    "Hotspot-only access cannot verify LAN authentication; changing gw_hostname to "
                    "the hotspot IP is insufficient. Start on LAN; the runner prompts for hotspot access later."
                )
            raise InvalidSetup("GET /auth returned HTTP 302; verify the LAN endpoint (redirect not followed)")
        if response.status_code not in (HttpStatus.C_200_OK, HttpStatus.C_401_UNAUTHORIZED):
            raise InvalidSetup(
                f"GET /auth returned HTTP {response.status_code}; verify LAN access with the hotspot closed"
            )
        payload: dict[str, Any] = self.gateway.response_json(response, "GET /auth", dict)
        if payload.get("lan") is False:
            raise InvalidSetup(
                "GET /auth identifies configuration-hotspot access (lan=false), which bypasses authentication. "
                "Close the hotspot and restore LAN access to .env gw_hostname before starting. "
                "Start on LAN; the runner prompts for hotspot access later."
            )
        try:
            challenge: InteractiveAuthChallenge = self.gateway.parse_interactive_challenge_response(request, response)
        except GatewayAuthenticationModeError:
            if default:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise
        login_response: requests.Response = self.gateway.submit_interactive_authentication(challenge, username, password)
        if login_response.status_code != HttpStatus.C_200_OK:
            if default:
                self.recovery_message = FACTORY_RESET_MESSAGE
            raise InvalidSetup("Required interactive login was rejected")
        return challenge

    def _baseline(self, defaults: dict[str, Any]) -> str:
        login: InteractiveAuthChallenge = self._login("Admin", self.config.gw_id, default=True)
        self.session = login.session
        assert self.session is not None
        payload: dict[str, Any] = self._read(self.gateway, self.session)
        identified: bool = self._identity(payload)
        field: str
        expected: Any
        for field, expected in defaults.items():
            if field not in payload or not typed_equal(payload[field], expected):
                if identified:
                    self.recovery_message = FACTORY_RESET_MESSAGE
                raise InvalidSetup(f"Required factory default field differs or is missing: {field}")
        if not identified:
            raise InvalidSetup("Missing nRF52 identity; cannot authorize mutations")
        device_defaults: dict[str, Any] = default_config_values((Cfg.GW_MAC, Cfg.MQTT_PREFIX, Cfg.MQTT_CLIENT_ID))
        placeholder_mac: Any = device_defaults[Cfg.GW_MAC]
        if not isinstance(placeholder_mac, str) or not placeholder_mac:
            raise InvalidConfig("Default reference has no placeholder MAC for MQTT defaults")
        for field in (Cfg.MQTT_PREFIX, Cfg.MQTT_CLIENT_ID):
            template: Any = device_defaults[field]
            if not isinstance(template, str) or placeholder_mac not in template:
                raise InvalidConfig(f"Default reference lacks MAC-derived MQTT template: {field}")
            expected = template.replace(placeholder_mac, payload[Cfg.GW_MAC])
            if field not in payload or not typed_equal(payload[field], expected):
                self.recovery_message = FACTORY_RESET_MESSAGE
                raise InvalidSetup(f"Required device-derived factory default differs or is missing: {field}")
        second: dict[str, Any] = self._read(self.gateway, self.session)
        if not typed_equal(payload, second):
            raise InvalidSetup("Two baseline reads are not stable")
        self.baseline = Baseline(canonical_hash(payload), json.dumps(payload, sort_keys=True))
        self.evidence.write("BASELINE", self.baseline)
        self.evidence.write("LIVE REALM", login.challenge["realm"])
        self.outcomes["baseline"] = "PASS"
        return login.challenge["realm"]

    def _declare_items(self) -> None:
        assert self.fixtures is not None
        fixture: ScalarFixture
        for fixture in self.fixtures.scalars:
            self.items.append(ManifestItem(fixture.category, "nvs", fixture.field, fixture.value, canonical_hash(fixture.value)))
        self.items.extend((
            ManifestItem(WIFI, "nvs", f"{Cfg.WIFI_STA_CONFIG}.{Cfg.SSID}", self.fixtures.ssid, canonical_hash(self.fixtures.ssid)),
            ManifestItem(WIFI, "nvs", f"{Cfg.WIFI_STA_CONFIG}.{Cfg.PASSWORD}", self.fixtures.wifi_password, canonical_hash(self.fixtures.wifi_password)),
        ))
        pem: PemFixture
        for pem in self.fixtures.certificates:
            self.items.append(ManifestItem(CERTS, "gw_cfg_def", pem.filename, pem.pem, hashlib.sha256(pem.pem.encode()).hexdigest()))
        self.evidence.write("SYNTHETIC FIXTURES", self.fixtures)
        self.evidence.write("DECLARED ITEMS AND HASHES", self.items)

    def _mark(self, field: str, committed: bool, method: str, reason: str, readback_hash: str | None = None) -> None:
        index: int
        item: ManifestItem
        for index, item in enumerate(self.items):
            if item.field_or_file == field:
                self.items[index] = replace(item, committed=committed, verification=method, reason=reason, readback_sha256=readback_hash)
                self.evidence.write("ITEM COMMITMENT", self.items[index])
                return
        raise ValueError(f"Undeclared item: {field}")

    def _field(self, payload: dict[str, Any], field: str, expected: Any, secret: bool = False, item_field: str | None = None) -> None:
        item_name: str = item_field or field
        if field not in payload:
            self._mark(item_name, False, "unverified", "Field is absent/redacted; no independent commitment proof")
            if secret:
                self.outcomes[self.active] = "ERROR"
                self.had_error = True
                self._say(
                    f"{self.active}: ERROR ({field} is redacted; this runner has no credential-use verification "
                    "for this field. An accepted write alone does not prove the secret was stored.)"
                )
                return
            raise InvalidSetup(f"Required readback field is missing: {field}")
        if not typed_equal(payload[field], expected):
            self._mark(item_name, False, "direct", "Accepted write differs from readback")
            raise CommitmentFailure(f"Accepted write differs from readback: {field}")
        self._mark(item_name, True, "direct", "Exact typed configuration readback", canonical_hash(payload[field]))

    def _wifi(self) -> None:
        assert self.fixtures is not None and self.session is not None
        body: dict[str, str] = {Cfg.SSID: self.fixtures.ssid, Cfg.PASSWORD: self.fixtures.wifi_password}
        # Retain the possibly applied write before sending, including a lost response.
        self.wifi_attempted = True
        response: requests.Response = self.gateway.request(self.session, HttpMethod.POST, GatewayApi.CONNECT, json_body=body)
        if response.status_code == HttpStatus.C_403_FORBIDDEN:
            self.wifi_attempted = False
            payload: dict[str, Any] = self._open_hotspot("provision configured Wi-Fi credentials")
            assert self.baseline is not None
            try:
                if not typed_equal(payload, json.loads(self.baseline.payload_json)):
                    raise InvalidSetup("Hotspot configuration differs from the verified LAN baseline")
                self.wifi_attempted = True
                assert self.hotspot_session is not None
                response = self.hotspot.request(self.hotspot_session, HttpMethod.POST, GatewayApi.CONNECT, json_body=body)
                self._ok(response, "Wi-Fi provisioning")
                observed: dict[str, Any] = self._read_wifi_after_connect()
            finally:
                self._return_to_lan()
        else:
            self._ok(response, "Wi-Fi provisioning")
            observed = self._read(self.gateway, self.session)
        wifi: Any = observed.get(Cfg.WIFI_STA_CONFIG)
        if not isinstance(wifi, dict):
            raise InvalidSetup("Wi-Fi readback missing or malformed")
        assert self.baseline is not None
        baseline_wifi: Any = json.loads(self.baseline.payload_json)[Cfg.WIFI_STA_CONFIG]
        if typed_equal(wifi, baseline_wifi):
            self.wifi_unchanged_verified = True
            reason: str = (
                "Wi-Fi credentials remain at baseline; no successful station connection and saved credentials "
                "were observed. A matching test access point is required; /connect.json acceptance is asynchronous."
            )
            field: str
            for field in (Cfg.SSID, Cfg.PASSWORD):
                self._mark(f"{Cfg.WIFI_STA_CONFIG}.{field}", False, "unverified", reason)
            raise InvalidSetup(reason)
        self._field(wifi, Cfg.SSID, self.fixtures.ssid, item_field=f"{Cfg.WIFI_STA_CONFIG}.{Cfg.SSID}")
        if Cfg.PASSWORD not in wifi and self.wifi_connection_verified:
            self._mark(
                f"{Cfg.WIFI_STA_CONFIG}.{Cfg.PASSWORD}", True, "indirect",
                "Configured protected Wi-Fi connection verified by fresh /status.json (SSID, urc=0, station IP) "
                "and saved SSID in /ruuvi.json; password is intentionally redacted",
            )
        else:
            self._field(wifi, Cfg.PASSWORD, self.fixtures.wifi_password, secret=True, item_field=f"{Cfg.WIFI_STA_CONFIG}.{Cfg.PASSWORD}")

        self.wifi_creation_verified = True

    def _provision_scalars(self) -> None:
        assert self.fixtures is not None and self.baseline is not None and self.session is not None
        baseline: dict[str, Any] = json.loads(self.baseline.payload_json)
        body: dict[str, Any] = {f.field: f.value for f in self.fixtures.scalars}
        # MQTT is in this task's mutation scope. Preserve its parser-dependent fields.
        body[Cfg.MQTT_CLIENT_ID] = baseline[Cfg.MQTT_CLIENT_ID]
        body[Cfg.MQTT_DATA_FORMAT] = baseline[Cfg.MQTT_DATA_FORMAT]
        self.config_attempted = True
        response: requests.Response = self.gateway.request(self.session, HttpMethod.POST, GatewayApi.CONFIG, json_body=body)
        self._ok(response, "Configuration provisioning")
        login: InteractiveAuthChallenge = self._login(self.fixtures.username, self.fixtures.password)
        self.session = login.session
        self._mark(Cfg.LAN_AUTH_PASS, True, "indirect", "Fresh custom-password interactive login")

    def _verify_category(self, category: str, payload: dict[str, Any]) -> None:
        assert self.fixtures is not None
        fixture: ScalarFixture
        for fixture in self.fixtures.scalars:
            if fixture.category != category or fixture.field in (Cfg.LAN_AUTH_PASS, Cfg.LAN_AUTH_API_KEY, Cfg.LAN_AUTH_API_KEY_RW):
                continue
            if fixture.field in REDACTED_FIELDS and fixture.field not in payload:
                continue  # Verified through saved-secret use below, never by an accepted write.
            self._field(payload, fixture.field, fixture.value, secret=fixture.field in REDACTED_FIELDS)
        if category in (HTTP, MQTT, REMOTE):
            self._prove_target(category)

    def _configure_targets(self) -> None:
        fixtures: SyntheticData | None = self.fixtures
        assert fixtures is not None
        values: dict[str, Any] = {item.field: item.value for item in fixtures.scalars}
        http_path: str = urlsplit(values[Cfg.HTTP_URL]).path
        remote_path: str = urlsplit(values[Cfg.REMOTE_CFG_URL]).path + ".json"
        routes: tuple[HttpRoute, ...] = (
            HttpRoute(http_path, "POST", values[Cfg.HTTP_USER], values[Cfg.HTTP_PASS]),
            HttpRoute(remote_path, "GET", values[Cfg.REMOTE_CFG_AUTH_BASIC_USER],
                      values[Cfg.REMOTE_CFG_AUTH_BASIC_PASS], json.dumps({Cfg.REMOTE_CFG_USE: True}).encode()),
        )
        http_fixture: HttpFixture = self.http_fixture_factory(self.config.gw_hostname, routes)
        self.http_fixture = http_fixture
        replacements: dict[str, str | int | bool] = {
            Cfg.HTTP_URL: http_fixture.base_url + http_path,
            Cfg.REMOTE_CFG_URL: http_fixture.base_url + remote_path,
            Cfg.USE_MQTT: False, Cfg.MQTT_SERVER: "test.mosquitto.org", Cfg.MQTT_PORT: 1883,
            Cfg.MQTT_USER: "rw", Cfg.MQTT_PASS: "readwrite",
        }
        self.fixtures = replace(fixtures, scalars=tuple(
            replace(item, value=replacements.get(item.field, item.value)) for item in fixtures.scalars
        ))
        self._say(f"Local HTTP/remote-config fixture: {http_fixture.base_url}. "
                  "Allow inbound Python traffic from the Gateway on this LAN (Windows Firewall if prompted).")
        self._say("MQTT: test.mosquitto.org:1883 connectivity; authenticated port 1884 checks the saved "
                  "public test password. Background MQTT forwarding remains disabled.")

    def _validate_target(self, params: dict[str, str], expected: int) -> None:
        assert self.session is not None
        response: requests.Response = self.gateway.request(self.session, HttpMethod.GET, GatewayApi.VALIDATE_URL, params=params)
        self._ok(response, "Target validation transport")
        payload: dict[str, Any] = self.gateway.response_json(response, "Target validation", dict)
        status: Any = payload.get("status")
        if type(status) is not int:
            raise InvalidSetup("Target validation response has no integer application status")
        if status != expected:
            raise InvalidSetup(f"Target validation expected application status {expected}, received {status}; "
                               "check fixture reachability, firewall, broker availability and stored credentials")

    def _prove_target(self, category: str) -> None:
        assert self.fixtures is not None and self.http_fixture is not None
        values: dict[str, Any] = {item.field: item.value for item in self.fixtures.scalars}
        secret_field: str
        url: str
        username: str
        check: str
        if category == MQTT:
            secret_field, url, username, check = Cfg.MQTT_PASS, "mqtt://test.mosquitto.org:1884", "rw", "check_mqtt"
        elif category == HTTP:
            secret_field, url, username, check = Cfg.HTTP_PASS, values[Cfg.HTTP_URL], values[Cfg.HTTP_USER], "check_post_advs"
        else:
            secret_field, url, username, check = (Cfg.REMOTE_CFG_AUTH_BASIC_PASS, values[Cfg.REMOTE_CFG_URL],
                                                  values[Cfg.REMOTE_CFG_AUTH_BASIC_USER], "check_remote_cfg")
        params: dict[str, str] = {
            "validate_type": check, "url": url, "auth_type": "basic", "user": username,
            "use_saved_password": "true", "use_ssl_client_cert": "false", "use_ssl_server_cert": "false",
        }
        public_nonce: str = hashlib.sha256(values[Cfg.MQTT_PREFIX].encode()).hexdigest()[:24]
        if category == MQTT:
            params.update(mqtt_topic_prefix=values[Cfg.MQTT_PREFIX], mqtt_client_id="cra-" + public_nonce,
                          mqtt_disable_retained_messages="true")
            self._validate_target(dict(params, url="mqtt://test.mosquitto.org:1883", auth_type="none", user=""), 200)
        path: str = urlsplit(url).path
        before: tuple[int, int] = (0, 0) if category == MQTT else self.http_fixture.counts(path)
        # No replacement password is sent; the Gateway must retrieve its saved secret.
        self._validate_target(dict(params, user="invalid-" + public_nonce[:12]), 401)
        self._validate_target(params, 200)
        if category != MQTT:
            after: tuple[int, int] = self.http_fixture.counts(path)
            self.evidence.write("HTTP FIXTURE WITNESS", {"category": category, "before": before, "after": after})
            if after[0] <= before[0] or after[1] <= before[1]:
                raise InvalidSetup("Gateway validation has no matching local authenticated/rejected request evidence")
        self._mark(secret_field, True, "indirect", "Saved-password target use: wrong-user rejection and authenticated success")

    def _bearer(self, token: str, method: str, path: str, body: dict[str, Any] | None = None) -> requests.Response:
        session: requests.Session = self.gateway.new_session()
        try:
            return self.gateway.request(session, method, path, headers={HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"}, json_body=body)
        finally:
            session.close()

    def _keys_read(self, payload: dict[str, Any]) -> None:
        assert self.fixtures is not None
        field: str
        for field in (Cfg.LAN_AUTH_API_KEY_USE, Cfg.LAN_AUTH_API_KEY_RW_USE):
            if field not in payload or type(payload[field]) is not bool:
                raise InvalidSetup(f"Missing or malformed key-use flag: {field}")
            if not payload[field]:
                raise CommitmentFailure(f"Accepted API key not enabled: {field}")
        self._ok(self._bearer(self.fixtures.ro_key, HttpMethod.GET, GatewayApi.HISTORY), "RO bearer read")
        self._mark(Cfg.LAN_AUTH_API_KEY, True, "indirect", "RO bearer GET /history")
        response: requests.Response = self._bearer(self.fixtures.rw_key, HttpMethod.GET, GatewayApi.CONFIG)
        self._ok(response, "RW bearer read")
        created: dict[str, Any] = self.gateway.response_json(response, "RW bearer configuration", dict)
        if not self._identity(created):
            raise InvalidSetup("Missing nRF52 identity during RW bearer read")

    def _keys_write(self) -> None:
        assert self.fixtures is not None and self.session is not None
        before: dict[str, Any] = self._read(self.gateway, self.session)
        body: dict[str, Any] = {}
        field: str
        for field in (Cfg.MQTT_PREFIX, Cfg.MQTT_CLIENT_ID, Cfg.MQTT_DATA_FORMAT):
            value: Any = before.get(field)
            if not isinstance(value, str) or not value:
                raise InvalidSetup(f"Cannot verify RW write: missing or malformed {field}")
            body[field] = value
        self._say("RW bearer check: explicitly preserving current mqtt_prefix, mqtt_client_id and "
                  "mqtt_data_format (workaround for firmware issue #1650).")
        self._ok(self._bearer(self.fixtures.rw_key, HttpMethod.POST, GatewayApi.CONFIG, body), "RW bearer preserving write")
        after: dict[str, Any] = self._read(self.gateway, self.session)
        self.evidence.write("RW NO-OP HASHES", (canonical_hash(before), canonical_hash(after)))
        if not typed_equal(before, after):
            changed: list[str] = sorted(field for field in before.keys() | after.keys()
                                       if field not in before or field not in after or not typed_equal(before[field], after[field]))
            self._mark(Cfg.LAN_AUTH_API_KEY_RW, False, "indirect", "RW authorization succeeded but the required no-op changed configuration")
            raise CommitmentFailure(f"RW bearer preserving POST changed configuration fields: {', '.join(changed)}")
        self._mark(Cfg.LAN_AUTH_API_KEY_RW, True, "indirect",
                   "RW bearer POST /ruuvi.json with current mqtt_prefix, mqtt_client_id and mqtt_data_format; "
                   "unchanged complete configuration hash (workaround for firmware issue #1650)")

    def _certificates(self) -> None:
        assert self.fixtures is not None and self.session is not None
        pem: PemFixture
        for pem in self.fixtures.certificates:
            self.attempted_files.append(pem.filename)
            response: requests.Response = self.gateway.request(self.session, HttpMethod.POST, GatewayApi.SSL_CERT, params={"file": pem.filename}, data=pem.pem)
            self._ok(response, f"Upload {pem.filename}")
            self.accepted_files.add(pem.filename)
            response = self.gateway.request(self.session, HttpMethod.GET, GatewayApi.EXTRA_CFG, params={"file": pem.filename})
            if response.status_code == HttpStatus.C_200_OK:
                if response.content != pem.pem.encode():
                    self._mark(pem.filename, False, "direct", "Accepted PEM differs from stored content", hashlib.sha256(response.content).hexdigest())
                    raise CommitmentFailure(f"Accepted certificate content differs: {pem.filename}")
                self._mark(pem.filename, True, "direct", "Exact stored PEM readback", hashlib.sha256(response.content).hexdigest())
            elif response.status_code == HttpStatus.C_403_FORBIDDEN:
                payload: dict[str, Any] = self._read(self.gateway, self.session)
                storage: Any = payload.get(Cfg.STORAGE)
                if not isinstance(storage, dict) or type(storage.get(pem.filename)) is not bool:
                    raise InvalidSetup(f"Missing or malformed storage presence flag: {pem.filename}")
                if not storage[pem.filename]:
                    self._mark(pem.filename, False, "indirect", "Accepted file has a false storage presence flag")
                    raise CommitmentFailure(f"Accepted certificate absent from storage: {pem.filename}")
                self._mark(pem.filename, True, "indirect", "storage presence flag changed from false to true; content unreadable")
            elif response.status_code == HttpStatus.C_404_NOT_FOUND:
                self._mark(pem.filename, False, "direct", "Accepted file read returned 404")
                raise CommitmentFailure(f"Accepted certificate is missing: {pem.filename}")
            else:
                self._ok(response, f"Read {pem.filename}")

    def _preservation(self) -> None:
        assert self.baseline is not None and self.fixtures is not None and self.session is not None
        expected: dict[str, Any] = json.loads(self.baseline.payload_json)
        payload: dict[str, Any] = self._read(self.gateway, self.session)
        fixture: ScalarFixture
        for fixture in self.fixtures.scalars:
            if fixture.field not in REDACTED_FIELDS or fixture.field in payload:
                expected[fixture.field] = fixture.value
        expected[Cfg.LAN_AUTH_API_KEY_USE] = True
        expected[Cfg.LAN_AUTH_API_KEY_RW_USE] = True
        if (self.wifi_attempted and not self.wifi_creation_verified and not self.wifi_connection_verified
                and typed_equal(payload.get(Cfg.WIFI_STA_CONFIG), expected[Cfg.WIFI_STA_CONFIG])):
            # Rejected or lost Wi-Fi replies leave the category ERROR. Readback may
            # independently prove that its baseline station settings survived.
            self.wifi_unchanged_verified = True
        if self.wifi_attempted and not self.wifi_unchanged_verified:
            expected[Cfg.WIFI_STA_CONFIG] = {Cfg.SSID: self.fixtures.ssid}
            if Cfg.PASSWORD in payload.get(Cfg.WIFI_STA_CONFIG, {}):
                expected[Cfg.WIFI_STA_CONFIG][Cfg.PASSWORD] = self.fixtures.wifi_password
        filename: str
        for filename in self.attempted_files:
            if filename in self.accepted_files:
                expected[Cfg.STORAGE][filename] = True
            else:
                # A rejected/lost reply does not establish whether storage changed.
                # Observe its presence independently; retain attempt tracking for reset.
                storage: Any = payload.get(Cfg.STORAGE)
                if not isinstance(storage, dict) or type(storage.get(filename)) is not bool:
                    raise InvalidSetup(f"Unresolved upload storage outcome: {filename}")
                expected[Cfg.STORAGE][filename] = storage[filename]
                self.evidence.write("UNCONFIRMED UPLOAD STORAGE PRESENCE", (filename, storage[filename]))
        if not typed_equal(payload, expected):
            self.evidence.write("EXPECTED COMPLETE CREATED CONFIGURATION", expected)
            differences: list[str] = [
                field for field in sorted(set(payload) | set(expected))
                if field not in payload or field not in expected or not typed_equal(payload[field], expected[field])
            ]
            raise CommitmentFailure(f"Created configuration differs in field(s): {', '.join(differences)}")
        if self.wifi_unchanged_verified:
            self._say("state preservation: verified with Wi-Fi still at baseline; Wi-Fi creation remains ERROR.")

    def _perform(self, category: str, action: Callable[[], None], finish: bool = True) -> bool:
        self.active = category
        try:
            action()
            if finish and self.outcomes[category] not in ("ERROR", "FAIL"):
                self.outcomes[category] = "PASS"
            return True
        except Exception as error:  # Preserve outcomes and mandatory recovery.
            error: Exception
            self.evidence.exception(error)
            verdict: str = "FAIL" if isinstance(error, CommitmentFailure) else "ERROR"
            self.had_error = self.had_error or verdict == "ERROR"
            if self.outcomes[category] != "FAIL":
                self.outcomes[category] = verdict
            reason: str = str(error) if type(error) in (InvalidSetup, CommitmentFailure) else "See evidence for protocol/transport details"
            self._say(f"{category}: {verdict} ({type(error).__name__}: {reason})")
            if self.identity_conflict and category not in ("factory reset", "manifest"):
                raise
            return False

    def _restore_session(self) -> requests.Session:
        assert self.fixtures is not None
        candidate: tuple[str, str]
        for candidate in ((self.fixtures.username, self.fixtures.password), ("Admin", self.config.gw_id)):
            try:
                return self._login(*candidate).session
            except Exception as error:  # noqa: BLE001 - Try all saved recovery candidates.
                error: Exception
                self.evidence.exception(error)
        # The saved RW candidate may work even when interactive login has failed.
        session: requests.Session = self.gateway.new_session()
        session.headers[HttpHeader.AUTHORIZATION] = f"{HttpAuthScheme.BEARER} {self.fixtures.rw_key}"
        self._read(self.gateway, session)
        return session

    def _factory_reset_recovery(self) -> None:
        assert self.baseline is not None
        if self.identity_conflict:
            raise InvalidSetup("DUT identity is unverified or mismatched; factory reset is not offered")
        baseline: dict[str, Any] = json.loads(self.baseline.payload_json)
        mutated: bool = self.wifi_attempted or self.config_attempted or bool(self.attempted_files) or not self.lan_ready
        if mutated:
            self.restoration = Restoration(True, False, reason="Awaiting operator-confirmed factory reset")
            self._perform("manifest", self._checkpoint)  # A failed evidence write must not prevent recovery.
            self.lan_ready = False
            ssid: str = f"Configure Ruuvi Gateway {self.config.gw_mac.replace(':', '')[-4:].upper()}"
            self._confirm(
                f"OPERATOR ACTION - factory reset verified Gateway {self.config.gw_mac}: "
                "this erases all local settings, credentials and uploaded certificates/private keys. "
                "Hold CONFIGURE through the initial restart until the red LED repeatedly turns on for "
                "200 ms and off for 200 ms (normally about 11 seconds). "
                "Release only after this completion signal; the Gateway restarts again. "
                f"Connect this host to Wi-Fi network '{ssid}' and wait for DHCP. "
                "Keep Ethernet connected. Confirm only after observing the LED completion signal "
                "and joining that hotspot; q leaves recovery unverified."
            )
            reset_session: requests.Session = self.hotspot.new_session()
            self.hotspot_session = reset_session
            reset_payload: dict[str, Any] = self._read(self.hotspot, reset_session)
            if not self._identity(reset_payload):
                raise InvalidSetup("Identity missing after factory reset")
            if not typed_equal(reset_payload, baseline):
                raise InvalidSetup("Factory reset did not restore the complete baseline on the hotspot")
            self._return_to_lan(default_auth=True)
        assert self.session is not None
        final: dict[str, Any] = self._read(self.gateway, self.session)
        if not self._identity(final):
            raise InvalidSetup("Identity missing during final factory-baseline verification")
        final_hash: str = canonical_hash(final)
        self.evidence.write("FACTORY RESET HASHES", (self.baseline.sha256, final_hash))
        if not typed_equal(final, baseline) or final_hash != self.baseline.sha256:
            raise InvalidSetup("Complete baseline was not recovered after factory reset")
        if self.config_attempted:
            assert self.fixtures is not None
            login: InteractiveAuthResult = self.gateway.authenticate_interactive(self.fixtures.username, self.fixtures.password)
            if login.login_response.status_code != HttpStatus.C_401_UNAUTHORIZED:
                raise InvalidSetup("Created admin credentials survived factory reset")
            token: str
            for token in (self.fixtures.ro_key, self.fixtures.rw_key):
                response: requests.Response = self._bearer(token, HttpMethod.GET, GatewayApi.HISTORY)
                if response.status_code != HttpStatus.C_401_UNAUTHORIZED:
                    raise InvalidSetup("Created bearer token survived factory reset")
        self.restoration = Restoration(True, True, final_hash,
                                       "Factory baseline and credential revocation verified" if mutated else "No mutations; baseline verified unchanged")

    def run(self) -> RunResult:
        try:
            self._report_bench_requirements()
            if self.config.wifi_ssid is None or self.config.wifi_password is None:
                raise InvalidSetup(
                    "Set wifi_ssid and wifi_password in .env for an available password-protected "
                    "2.4 GHz Wi-Fi LAN. Start the Gateway on Ethernet; the runner will provision Wi-Fi."
                )
            default_fields: tuple[str, ...] = tuple(
                field for field in load_ui_default_config() if field not in DEVICE_DERIVED_FIELDS
            )
            defaults: dict[str, Any] = default_config_values(default_fields)
            self.progress("Authenticate, verify identity and capture two stable default reads")
            realm: str = self._baseline(defaults)
            fixtures: SyntheticData = self.fixture_factory(realm, self.random_bytes, self.now())
            self.fixtures = replace(fixtures, ssid=self.config.wifi_ssid, wifi_password=self.config.wifi_password)
            self._configure_targets()
            self._declare_items()
            self.active = "manifest"
            self._checkpoint()
            self.progress("Provision Wi-Fi using operator-confirmed hotspot access")
            self._perform(WIFI, self._wifi)
            if not self.lan_ready:
                raise InvalidSetup("LAN transition incomplete; stopping normal checks and attempting recovery")
            self.progress("Provision custom authentication and local target assets")
            self.active = ADMIN
            self._provision_scalars()
            assert self.session is not None
            payload: dict[str, Any] = self._read(self.gateway, self.session)
            category: str
            for category in (ADMIN, HTTP, MQTT, REMOTE):
                self._perform(category, partial(self._verify_category, category, payload))
            self.progress("Verify RO and RW bearer reads in separate sessions")
            keys_ready: bool = self._perform(KEYS, lambda: self._keys_read(payload), finish=False)
            self.progress("Store and verify every required certificate and private key")
            self._perform(CERTS, self._certificates)
            self.progress("Check complete created configuration and RW no-op preservation")
            preserved: bool = self._perform("state preservation", self._preservation)
            if keys_ready:
                if preserved:
                    self._perform(KEYS, self._keys_write)
                else:
                    self.had_error = True
                    self.outcomes[KEYS] = "ERROR"
                    self._say(f"{KEYS}: ERROR (RO/RW reads completed; RW no-op POST not run because state preservation failed)")
        except Exception as error:  # noqa: BLE001 - Run boundary must always restore.
            error: Exception
            self.evidence.exception(error)
            self.had_error = True
            if self.outcomes[self.active] != "FAIL":
                self.outcomes[self.active] = "ERROR"
            reason: str = str(error) if type(error) in (InvalidSetup, InvalidConfig) else "See evidence for protocol/transport details"
            self._say(f"{self.active}: ERROR ({type(error).__name__}: {reason})")
        finally:
            self.progress("Offer factory reset and verify baseline and credential revocation")
            if self.baseline is not None and not self._perform("factory reset", self._factory_reset_recovery):
                self.restoration = Restoration(True, False, reason="Factory-reset recovery could not be verified; inspect evidence")
                self.recovery_message = f"Factory-reset recovery unverified. Recover the dedicated DUT using manifest {self.manifest_path} and evidence {self.evidence.path}."
            if self.http_fixture is not None:
                try:
                    self.http_fixture.close()
                except Exception as error:  # noqa: BLE001 - Final evidence must survive fixture shutdown failure.
                    error: Exception
                    self.evidence.exception(error)
                    self.had_error = True
                    self._say("Local HTTP fixture shutdown failed; see evidence")
            # A category only passes when every item has independent commitment evidence.
            for category in CATEGORIES:
                category_items: list[ManifestItem] = [i for i in self.items if i.category == category]
                if self.outcomes[category] == "PASS" and (not category_items or not all(i.committed for i in category_items)):
                    self.outcomes[category] = "ERROR"
            self.progress("Finalize historical manifest and per-category results")
            if self.outcomes["manifest"] == "NOT RUN":
                self.outcomes["manifest"] = "PASS"
            self._perform("manifest", self._checkpoint)
            self.evidence.write("FACTORY RESET RECOVERY", self.restoration)
            self.evidence.write("PER-CHECK OUTCOMES", self.outcomes)
            session: requests.Session
            for session in self.sessions:
                session.close()
        verdict: str
        if self.had_error or "ERROR" in self.outcomes.values():
            verdict = "ERROR"
        elif "FAIL" in self.outcomes.values():
            verdict = "FAIL"
        elif "NOT RUN" in self.outcomes.values():
            verdict = "ERROR"
        else:
            verdict = "PASS"
        category: str
        outcome: str
        for category, outcome in self.outcomes.items():
            self._say(f"{category}: {outcome}")
        return RunResult({"PASS": 0, "FAIL": 1, "ERROR": 2}[verdict], verdict, dict(self.outcomes), set(), self.recovery_message)


def execute_test_5_11_1_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    fixture_factory: Callable[[str, Callable[[int], bytes], datetime], SyntheticData] = generate_fixtures,
    manifest_writer: Callable[[Path, Manifest], None] = write_manifest,
    input_fn: Callable[[str], str] = input,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
    http_fixture_factory: Callable[[str, tuple[HttpRoute, ...]], HttpFixture] = LocalHttpFixture,
) -> RunResult:
    directory: Path = work_dir if work_dir is not None else Path.cwd()
    log: EvidenceLog = EvidenceLog.create(directory / "logs", PREFIX, now)
    output(f"Open log file: {log.path}")

    def report(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, 9)
    result: RunResult = RunResult(2, "ERROR", dict.fromkeys(CHECKS, "NOT RUN"), set())
    log.write("TEST CASE AND UNIT", f"{TEST_CASE} A: creation plus operator-confirmed reset recovery, not forensic erasure")
    log.write("UTC START", format_utc(log.started_at))
    try:
        progress.step("Load and validate .env")
        model_fields: set[str] = {field.name for field in fields(DutConfig)}
        if not {"gw_fw", "wifi_ssid", "wifi_password"}.issubset(model_fields):
            raise InvalidConfig(
                "Incompatible lib/models.py: extract the complete test bundle, including lib/, "
                "into a clean directory and copy your .env there. Do not mix runner/library versions."
            )
        config: DutConfig = load_dut_config(directory / ".env")
        log.write("DUT CONFIGURATION", config)
        result = FunctionalTest_5_11_1_2_a(
            config, log, session_factory, random_bytes, now, progress.step, output,
            fixture_factory, manifest_writer, input_fn=input_fn, monotonic=monotonic, sleep=sleep,
            http_fixture_factory=http_fixture_factory,
        ).run()
    except Exception as error:
        error: Exception
        log.exception(error)  # noqa: TRY401 - EvidenceLog needs the exception object.
        result.outcomes["baseline"] = "ERROR"
        reason: str = str(error) if isinstance(error, InvalidConfig) else "see evidence for details"
        report(f"Setup: ERROR ({type(error).__name__}: {reason})")
    finally:
        if result.recovery_message:
            report(result.recovery_message)
        log.write("FINAL PER-CHECK OUTCOMES", result.outcomes)
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_11_1_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
