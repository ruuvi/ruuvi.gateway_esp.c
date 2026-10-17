"""Offline implementation regressions; these are not live compliance evidence."""

from __future__ import annotations

import base64
import hashlib
import io
import json
import tempfile
import unittest
from dataclasses import FrozenInstanceError, dataclass, replace
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler
from pathlib import Path
from typing import Any, Callable
from unittest.mock import Mock, patch
from urllib.parse import parse_qs, urlsplit

import requests
from Crypto.IO import PEM
from Crypto.PublicKey import ECC
from Crypto.Util.asn1 import DerSequence

import fixture_5_11_1_2_a as fixture_module
import test_5_11_1_2_a as runner
from fixture_5_11_1_2_a import CredentialWitness, HttpRoute
from lib import config as config_module
from lib.config import FACTORY_RESET_MESSAGE, load_ui_default_config
from lib.gateway import GatewayCfgLanAuthType
from lib.models import DutConfig, RunResult
from test_support.fake_gateway import FakeGateway, FakeResponse, FakeSession, RecordedRequest, env_text

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
NOW: datetime = datetime(2026, 9, 27, 12, 0, 0, tzinfo=timezone.utc)
FILES: tuple[str, ...] = (
    "http_cli_cert", "http_cli_key", "http_srv_cert", "mqtt_cli_cert", "mqtt_cli_key",
    "mqtt_srv_cert", "stat_cli_cert", "stat_cli_key", "rcfg_cli_cert", "rcfg_cli_key",
)
CATEGORIES: tuple[str, ...] = (
    "Wi-Fi STA credentials", "Web-UI admin password", "M2M API keys RO + RW",
    "Custom HTTP telemetry target + secret", "Custom MQTT target + secret",
    "Remote-config URL + secret", "User SSL/mTLS certificates + private keys",
)


class DeterministicBytes:
    def __init__(self) -> None:
        self.counter: int = 0

    def __call__(self, size: int) -> bytes:
        result: bytes = b""
        while len(result) < size:
            self.counter += 1
            result += hashlib.sha256(f"unit-a-fixture-{self.counter}".encode()).digest()
        return result[:size]


@dataclass(frozen=True)
class WireRequest:
    host: str
    method: str
    path: str
    filename: str | None
    body: Any
    session: int
    scheme: str
    redirects: bool
    trust_env: bool
    timeout: tuple[int, int]
    query: dict[str, list[str]]


class CreationSession(FakeSession):
    def __init__(self, gateway: CreationGateway) -> None:
        super().__init__(gateway)
        self.creation_gateway: CreationGateway = gateway

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        url: str = request.url or ""
        path: str = urlsplit(url).path
        body: Any = request.body
        if isinstance(body, bytes):
            body = body.decode()
        if body and path != "/ssl_cert":
            body = json.loads(body)
        query: dict[str, list[str]] = parse_qs(urlsplit(url).query)
        filename: str | None = query["file"][0] if "file" in query else None
        authorization: str = request.headers.get("Authorization", "")
        wire: WireRequest = WireRequest(
            urlsplit(url).hostname or "", request.method or "", path, filename, body,
            self.number, authorization.split(" ")[0] if authorization else "none",
            kwargs["allow_redirects"], self.trust_env, kwargs["timeout"], query,
        )
        self.creation_gateway.wire.append(wire)
        self.creation_gateway.current = wire
        if (wire.host == "10.10.0.1") != (self.creation_gateway.host_network == "hotspot"):
            raise requests.ConnectionError("Host is on the wrong network")
        if wire.host != "10.10.0.1" and self.creation_gateway.ap_active:
            return FakeResponse(503, {})
        if wire.host != "10.10.0.1" and path == "/auth" and self.creation_gateway.lan_failures:
            self.creation_gateway.lan_failures -= 1
            raise requests.ConnectionError("Ethernet not ready yet")
        if wire.host == "10.10.0.1" and not self.creation_gateway.hotspot_available:
            raise requests.ConnectionError("Offline hotspot")
        response: FakeResponse = self.gateway.response_for(
            self, wire.method, path, dict(request.headers), body, wire.redirects,
        )
        self.cookies.update(response.cookies)
        return response


class FakeHttpFixture:
    def __init__(self, routes: tuple[HttpRoute, ...]) -> None:
        self.base_url: str = "http://192.0.2.20:18080"
        self.witness: CredentialWitness = CredentialWitness(routes)
        self.closed: bool = False

    def counts(self, path: str) -> tuple[int, int]:
        return self.witness.counts(path)

    def close(self) -> None:
        self.closed = True


class CreationGateway(FakeGateway):
    """Use shared challenge/cookie validation; model this case's storage operations."""

    def __init__(self) -> None:
        super().__init__(CONFIG)
        self.server_key = ECC.construct(curve="P-256", d=7)
        # Keep the public key paired with the shared fixture's server key.
        raw: bytes = b"\x04" + int(self.server_key.pointQ.x).to_bytes(32, "big") + int(self.server_key.pointQ.y).to_bytes(32, "big")
        self.server_public_b64 = base64.b64encode(raw).decode()
        self.public: dict[str, Any] = load_ui_default_config()
        self.public.update(gw_mac=CONFIG.gw_mac, fw_ver="test", nrf52_fw_ver="test", mqtt_prefix="ruuvi/AA:BB:CC:DD:EE:FF/", mqtt_client_id=CONFIG.gw_mac)
        self.initial: dict[str, Any] = json.loads(json.dumps(self.public))
        self.secrets: dict[str, str] = {}
        self.files: dict[str, str] = {}
        self.wire: list[WireRequest] = []
        self.current: WireRequest | None = None
        self.http_fixture: FakeHttpFixture | None = None
        self.validation_override: int | bool | None = None
        self.omit_fixture_witness: bool = False
        self.reset_completes: bool = True
        self.reset_mac: str | None = CONFIG.gw_mac
        self.initial_auth_response: FakeResponse | None = None
        self.hotspot_available: bool = True
        self.host_network: str = "lan"
        self.ap_active: bool = False
        self.finish_failures: int = 0
        self.lose_finish_reply: bool = False
        self.lan_failures: int = 0
        self.lose_wifi_reply: bool = False
        self.lose_eth_reply: bool = False
        self.delayed_wifi_reads: int = 0
        self.wifi_association_succeeds: bool = True
        self.wifi_status_override: dict[str, Any] | None = None
        self.wifi_status_http: int = 200
        self.retain_saved_wifi_on_clear: bool = False
        self.wrong_wifi_ssid: str | None = None
        self.lan_identity: str | None = None
        self.lose_clear_reply: bool = False
        self.hotspot_mac: str | None = CONFIG.gw_mac
        self.expose_secrets: bool = True
        self.expose_blobs: bool = True
        self.provision_status: int = 200
        self.lose_provision_response: bool = False
        self.fail_file: str | None = None
        self.file_status: int = 500
        self.lose_upload_response: bool = False
        self.omit_unconfirmed_storage: bool = False
        self.corrupt_file: str | None = None
        self.delete_failure: str | None = None
        self.lose_delete_response: bool = False
        self.missing_flag: str | None = None
        self.noop_mutates: bool = False
        self.noop_mutation_field: str = "mqtt_prefix"
        self.side_effect: bool = False
        self.omit_fields: set[str] = set()
        self.alter_fields: dict[str, Any] = {}
        self.final_mismatch: bool = False
        self.survive_admin: bool = False
        self.survive_token: bool = False
        self.reject_custom_login: bool = False
        self.restoring: bool = False
        self.restore_status: int = 200
        self.default_login_status: int | None = None
        self.config_reads: int = 0
        self.unstable_baseline: bool = False
        self.malformed_read: bool = False
        self.fail_keys_read: bool = False
        self.read_failure: bool = False
        self.secret_clear_status: int = 200
        self.lose_restore_response: bool = False

    def _config_response(self) -> FakeResponse:
        self.config_reads += 1
        if self.read_failure and self.mode == GatewayCfgLanAuthType.RUUVI and not self.restoring:
            self.read_failure = False
            raise requests.Timeout("readback lost")
        result: dict[str, Any] = json.loads(json.dumps(self.public))
        if self.lan_identity is not None and self.current is not None and self.current.host != "10.10.0.1":
            result["gw_mac"] = self.lan_identity
        result.update(lan_auth_type=self.mode, lan_auth_user=self.custom_username if self.mode != "lan_auth_default" else "Admin", lan_auth_api_key_use=bool(self.ro_key), lan_auth_api_key_rw_use=bool(self.rw_key))
        if self.expose_secrets:
            result.update({k: v for k, v in self.secrets.items() if v and k != "wifi_password"})
            if self.secrets.get("wifi_password"):
                result["wifi_sta_config"]["password"] = self.secrets["wifi_password"]
        if self.mode == "lan_auth_ruuvi" and not self.restoring:
            field: str
            for field in self.omit_fields:
                result.pop(field, None)
            result.update(self.alter_fields)
            if self.omit_unconfirmed_storage and self.fail_file is not None:
                result["storage"].pop(self.fail_file, None)
        if self.unstable_baseline and self.config_reads == 2:
            result["coordinates"] = "unstable"
        if self.restoring and self.final_mismatch:
            result["coordinates"] = "surviving change"
        return FakeResponse(200, result, malformed_json=self.malformed_read)

    def _apply_config(self, body: dict[str, Any]) -> None:
        super()._apply_config(body)
        name: str
        value: Any
        if self.current is not None and self.current.scheme == "Bearer":
            # Model issue #1650: omitted MQTT fields reset even for an authorized partial write.
            for name in ("mqtt_prefix", "mqtt_client_id", "mqtt_data_format"):
                self.public[name] = body.get(name, self.initial[name])
        for name, value in body.items():
            if name in ("lan_auth_type", "lan_auth_user", "lan_auth_pass", "lan_auth_api_key", "lan_auth_api_key_rw"):
                continue
            if name.endswith("_pass"):
                self.secrets[name] = value
            else:
                self.public[name] = value
        if self.current is not None and self.current.scheme == "Bearer" and self.noop_mutates:
            self.public[self.noop_mutation_field] = "unexpected-reset/"
        if body.get("http_auth") == "none":
            self.public.pop("http_user", None)
            # Faithful parser: switching HTTP to none does NOT clear saved Basic secret.
        if body.get("remote_cfg_auth_type") == "none":
            self.public.pop("remote_cfg_auth_basic_user", None)
            self.secrets.pop("remote_cfg_auth_basic_pass", None)
        if body.get("lan_auth_type") == "lan_auth_default":
            self.restoring = True
        if body.get("lan_auth_type") == "lan_auth_ruuvi" and self.side_effect:
            self.public["coordinates"] = "unexpected"
        if self.lose_provision_response and body.get("lan_auth_type") == "lan_auth_ruuvi":
            self.lose_provision_response = False
            raise requests.Timeout("applied write, lost reply")
        if self.lose_restore_response and body.get("lan_auth_type") == "lan_auth_default":
            self.lose_restore_response = False
            raise requests.Timeout("applied restoration, lost reply")

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        assert self.current is not None
        wire: WireRequest = self.current
        if call.path == "/auth" and call.method == "GET" and self.initial_auth_response is not None:
            return self.initial_auth_response
        if call.path == "/validate_url":
            params: dict[str, str] = {key: values[0] for key, values in wire.query.items()}
            status: int | bool
            if params["validate_type"] == "check_mqtt":
                status = 200 if params["auth_type"] == "none" or (params["user"] == "rw" and self.secrets.get("mqtt_pass") == "readwrite") else 401
            else:
                assert self.http_fixture is not None
                secret: str = "http_pass" if params["validate_type"] == "check_post_advs" else "remote_cfg_auth_basic_pass"
                authorization: str = "Basic " + base64.b64encode(f"{params['user']}:{self.secrets.get(secret, '')}".encode()).decode()
                method: str = "POST" if secret == "http_pass" else "GET"
                if self.omit_fixture_witness:
                    status = 401 if params["user"].startswith("invalid-") else 200
                else:
                    body: bytes
                    status, body = self.http_fixture.witness.respond(method, urlsplit(params["url"]).path, authorization)
                    if method == "GET" and status == 200:
                        # The firmware parses the downloaded configuration after HTTP authentication.
                        # Its diagnostic names use_remote_cfg, but the JSON parser reads remote_cfg_use.
                        document: Any = json.loads(body)
                        if not isinstance(document, dict) or document.get("remote_cfg_use") is not True:
                            status = 502
            return FakeResponse(200, {"status": status if self.validation_override is None else self.validation_override})
        if wire.host == "10.10.0.1":
            if call.path == "/status.json" and call.method == "GET":
                status: dict[str, Any] = self.wifi_status_override if self.wifi_status_override is not None else {
                    "ssid": self.public["wifi_sta_config"]["ssid"], "urc": 0, "ip": "192.0.2.10",
                }
                return FakeResponse(self.wifi_status_http, status)
            if call.path == "/ruuvi.json" and call.method == "POST" and "use_eth" in call.body:
                self.public.update(call.body)
                return FakeResponse(200, {})
            if call.path == "/ruuvi.json" and call.method == "POST":
                if self.finish_failures:
                    self.finish_failures -= 1
                    return FakeResponse(500, {})
                self.public.update(call.body)
                # The application POST schedules AP shutdown; /connect.json does not.
                self.public["mqtt_prefix"] = call.body.get("mqtt_prefix", self.initial["mqtt_prefix"])
                self.public["mqtt_client_id"] = call.body.get("mqtt_client_id", self.initial["mqtt_client_id"])
                self.public["mqtt_data_format"] = call.body.get("mqtt_data_format", "ruuvi_raw")
                self.ap_active = False
                if self.lose_finish_reply:
                    self.lose_finish_reply = False
                    raise requests.Timeout("Finish applied but reply lost")
                return FakeResponse(200, {})
            if call.path == "/auth" and call.method == "GET":
                return FakeResponse(200, {"lan_auth_type": "lan_auth_allow", "lan": False})
            if call.path == "/ruuvi.json" and call.method == "GET":
                response: FakeResponse = self._config_response()
                payload: dict[str, Any] = response.json()
                if self.hotspot_mac is None:
                    payload.pop("gw_mac", None)
                else:
                    payload["gw_mac"] = self.hotspot_mac
                if self.secrets.get("wifi_password") and self.delayed_wifi_reads:
                    self.delayed_wifi_reads -= 1
                    payload["wifi_sta_config"] = {"ssid": ""}
                return FakeResponse(200, payload)
            if call.path == "/connect.json":
                # Current firmware clears runtime credentials but does not save an empty STA config.
                if call.body["ssid"] == "" and self.retain_saved_wifi_on_clear:
                    return FakeResponse(200, {})
                if call.body["ssid"] and not self.wifi_association_succeeds:
                    return FakeResponse(200, {})
                if call.body["ssid"] is not None:
                    self.public["wifi_sta_config"] = {"ssid": call.body["ssid"]}
                    if call.body["ssid"] and self.wrong_wifi_ssid is not None:
                        self.public["wifi_sta_config"]["ssid"] = self.wrong_wifi_ssid
                    self.secrets["wifi_password"] = call.body["password"]
                    if call.body["ssid"]:
                        self.public["use_eth"] = False
                        if self.lose_wifi_reply:
                            self.lose_wifi_reply = False
                            raise requests.Timeout("Wi-Fi applied but response lost")
                    elif self.lose_clear_reply:
                        self.lose_clear_reply = False
                        raise requests.Timeout("Wi-Fi cleared but response lost")
                elif self.lose_eth_reply:
                    self.lose_eth_reply = False
                    raise requests.Timeout("Ethernet applied but response lost")
                return FakeResponse(200, {})
        if call.path == "/connect.json":
            return FakeResponse(403, {})
        if call.path == "/auth" and call.method == "POST":
            if self.default_login_status is not None and not self.config_bodies:
                return FakeResponse(self.default_login_status, {})
            if self.reject_custom_login and self.mode == "lan_auth_ruuvi":
                return FakeResponse(401, {})
            if self.survive_admin and self.restoring and call.body.get("login") != "Admin":
                return FakeResponse(200, {})
        if self.survive_token and self.restoring and call.bearer_token:
            return FakeResponse(200, {})
        if self.fail_keys_read and call.method == "GET" and call.bearer_token:
            return FakeResponse(401, {})
        if call.path == "/ruuvi.json" and call.method == "POST":
            if call.body.get("lan_auth_type") == "lan_auth_ruuvi" and self.provision_status != 200:
                return FakeResponse(self.provision_status, {})
            if call.body.get("lan_auth_type") == "lan_auth_default" and self.restore_status != 200:
                return FakeResponse(self.restore_status, {})
            if call.body.get("http_pass") == "" and self.secret_clear_status != 200:
                return FakeResponse(self.secret_clear_status, {})
        if call.path in ("/ssl_cert", "/extra_cfg") and session.authorized:
            if wire.filename is None:
                raise AssertionError("Storage requests must name a file")
            filename: str = wire.filename
            if call.method == "POST":
                if filename == self.fail_file and not self.lose_upload_response:
                    return FakeResponse(self.file_status, {})
                self.files[filename] = call.body
                self.public["storage"][filename] = True
                if filename == self.fail_file and self.lose_upload_response:
                    raise requests.Timeout("upload applied, reply lost")
                return FakeResponse(200, {})
            if call.method == "DELETE":
                if filename == self.delete_failure:
                    return FakeResponse(500, {})
                self.files.pop(filename, None)
                self.public["storage"][filename] = False
                if self.lose_delete_response:
                    self.lose_delete_response = False
                    raise requests.Timeout("delete reply lost")
                return FakeResponse(200, {})
            if not self.expose_blobs:
                if self.missing_flag == filename:
                    self.public["storage"].pop(filename, None)
                return FakeResponse(403, {})
            if filename not in self.files:
                return FakeResponse(404, {})
            response = FakeResponse(200)
            response._content = ("corrupt" if filename == self.corrupt_file else self.files[filename]).encode()
            return response
        return None


class CreationTests(unittest.TestCase):
    fixtures: runner.SyntheticData

    @classmethod
    def setUpClass(cls) -> None:
        cls.fixtures = runner.generate_fixtures("Ruuvi Gateway", DeterministicBytes(), NOW)

    def setUp(self) -> None:
        self.temporary: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory: Path = Path(self.temporary.name)
        (self.directory / ".env").write_text(env_text(CONFIG) + self.wifi_env(), encoding="utf-8")
        self.gateway: CreationGateway = CreationGateway()
        self.output: list[str] = []
        self.result: RunResult | None = None
        self.confirmations: list[tuple[str, int]] = []
        self.input_prompts: list[str] = []
        self.answers: list[str | BaseException] = []
        self.elapsed: float = 0.0

    def wifi_env(self) -> str:
        return f"wifi_ssid={self.fixtures.ssid}\nwifi_password={self.fixtures.wifi_password}\n"

    def test_missing_wifi_configuration_stops_before_dut_access(self) -> None:
        (self.directory / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["baseline"], "ERROR")
        self.assertEqual(self.gateway.wire, [])
        self.assertEqual(self.confirmations, [])
        self.assertIn("Set wifi_ssid and wifi_password in .env", "\n".join(self.output))

    def test_configured_wifi_credentials_replace_generated_values_on_wire_and_manifest(self) -> None:
        (self.directory / ".env").write_text(
            env_text(CONFIG) + "wifi_ssid=MY_WIFI_123\nwifi_password=12345678\n", encoding="utf-8",
        )
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assertEqual(self.gateway.wire[4].body, {"ssid": "MY_WIFI_123", "password": "12345678"})
        self.assertEqual(self.gateway.wire[6].body, self.gateway.wire[4].body)
        items: dict[str, Any] = {item["field_or_file"]: item for item in self.manifest()["items"]}
        self.assertEqual(items["wifi_sta_config.ssid"]["value"], "MY_WIFI_123")
        self.assertEqual(items["wifi_sta_config.password"]["value"], "12345678")
        self.assertNotIn("12345678", "\n".join(self.output))
        self.assert_restored()

    def test_incomplete_wifi_pair_is_reported_before_any_network_access(self) -> None:
        (self.directory / ".env").write_text(env_text(CONFIG) + "wifi_ssid=Test\n", encoding="utf-8")
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(self.gateway.wire, [])
        self.assertEqual(self.confirmations, [])
        self.assertIn("wifi_ssid and wifi_password must be supplied together", "\n".join(self.output))

    def test_factory_reset_clears_wifi_that_api_cleanup_cannot_clear(self) -> None:
        self.gateway.retain_saved_wifi_on_clear = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assert_restored()
        self.assertTrue(any("factory reset verified Gateway" in instruction for instruction, _ in self.confirmations))
        self.assertFalse([w for w in self.gateway.wire if w.path == "/connect.json" and w.body == {"ssid": "", "password": ""}])

    def make_http_fixture(self, hostname: str, routes: tuple[HttpRoute, ...]) -> FakeHttpFixture:
        self.assertEqual(hostname, CONFIG.gw_hostname)
        fixture: FakeHttpFixture = FakeHttpFixture(routes)
        self.gateway.http_fixture = fixture
        return fixture

    def confirm(self, prompt: str) -> str:
        self.input_prompts.append(prompt)
        instruction: str = next(line for line in reversed(self.output) if line.startswith("OPERATOR ACTION"))
        self.confirmations.append((instruction, len(self.gateway.wire)))
        answer: str | BaseException = self.answers.pop(0) if self.answers else ""
        if isinstance(answer, BaseException):
            raise answer
        if answer in ("", "yes"):
            if "factory reset verified Gateway" in instruction:
                self.gateway.restoring = True
                self.gateway.hotspot_mac = self.gateway.reset_mac
                if self.gateway.reset_completes:
                    self.gateway.public = json.loads(json.dumps(self.gateway.initial))
                    self.gateway.secrets.clear()
                    self.gateway.files.clear()
                    self.gateway.mode = GatewayCfgLanAuthType.DEFAULT
                    self.gateway.custom_username = "Admin"
                    if not self.gateway.survive_token:
                        self.gateway.ro_key = ""
                        self.gateway.rw_key = ""
            self.gateway.host_network = "lan" if "return to LAN" in instruction else "hotspot"
            if self.gateway.host_network == "hotspot":
                self.gateway.ap_active = True
        return answer

    def sleep(self, seconds: float) -> None:
        self.elapsed += seconds

    def fixture_factory(self, realm: str, _random_bytes: Callable[[int], bytes], now: datetime) -> runner.SyntheticData:
        self.assertEqual(realm, "Ruuvi Gateway")
        self.assertEqual(now, NOW)
        return self.fixtures

    def execute(self, writer: Callable[[Path, runner.Manifest], None] = runner.write_manifest) -> RunResult:
        result: RunResult = runner.execute_test_5_11_1_2_a(
            self.directory, lambda: CreationSession(self.gateway), lambda: NOW,
            self.output.append, DeterministicBytes(), self.fixture_factory, writer,
            input_fn=self.confirm, monotonic=lambda: self.elapsed, sleep=self.sleep,
            http_fixture_factory=self.make_http_fixture,
        )
        self.result = result
        return result

    def manifest(self) -> dict[str, Any]:
        paths: list[Path] = list((self.directory / "logs").glob("*_manifest_*.json"))
        self.assertEqual(len(paths), 1)
        return json.loads(paths[0].read_text())

    def log(self) -> str:
        paths: list[Path] = list((self.directory / "logs").glob("*.log"))
        self.assertEqual(len(paths), 1)
        return paths[0].read_text()

    def assert_restored(self) -> None:
        self.assertEqual(self.gateway.public, self.gateway.initial)
        self.assertFalse(self.gateway.files)
        self.assertTrue(self.manifest()["restored"]["verified"])
        self.assertFalse(any(self.gateway.secrets.values()))

    def test_complete_run_request_order_bodies_sessions_and_manifest(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.verdict), (0, "PASS"), self.log())
        self.assert_restored()
        manifest: dict[str, Any] = self.manifest()
        self.assertEqual((manifest["test_case"], manifest["unit"], manifest["schema_version"]), ("5.11-1-2", "A", 1))
        self.assertEqual({item["category"] for item in manifest["items"]}, set(CATEGORIES))
        self.assertEqual(tuple(item["field_or_file"] for item in manifest["items"] if item["location"] == "gw_cfg_def"), FILES)
        self.assertFalse(manifest["currently_seeded"])
        self.assertTrue(all(item["committed"] for item in manifest["items"]))
        expected_hash: str = hashlib.sha256(json.dumps(self.gateway.initial, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        self.assertEqual(manifest["baseline"], {"sha256": expected_hash, "payload": self.gateway.initial})
        self.assertEqual({item["category"]: item["verdict"] for item in manifest["outcomes"]}, result.outcomes)
        expected: list[tuple[str, str, str | None, str]] = [
            ("GET", "/auth", None, "none"), ("POST", "/auth", None, "none"),
            ("GET", "/ruuvi.json", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("POST", "/connect.json", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("POST", "/connect.json", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("GET", "/status.json", None, "none"),
            ("GET", "/ruuvi.json", None, "none"), ("POST", "/ruuvi.json", None, "none"),
            ("POST", "/connect.json", None, "none"), ("POST", "/ruuvi.json", None, "none"),
            ("GET", "/auth", None, "none"),
            ("POST", "/auth", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("POST", "/ruuvi.json", None, "none"), ("GET", "/auth", None, "none"),
            ("POST", "/auth", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            *[("GET", "/validate_url", None, "none")] * 7,
            ("GET", "/history", None, "Bearer"), ("GET", "/ruuvi.json", None, "Bearer"),
        ]
        filename: str
        for filename in FILES:
            expected.extend((("POST", "/ssl_cert", filename, "none"), ("GET", "/extra_cfg", filename, "none")))
        expected.extend([
            ("GET", "/ruuvi.json", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("POST", "/ruuvi.json", None, "Bearer"), ("GET", "/ruuvi.json", None, "none"),
            ("GET", "/ruuvi.json", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("POST", "/ruuvi.json", None, "none"), ("POST", "/connect.json", None, "none"),
            ("POST", "/ruuvi.json", None, "none"), ("GET", "/auth", None, "none"),
            ("POST", "/auth", None, "none"), ("GET", "/ruuvi.json", None, "none"),
            ("GET", "/ruuvi.json", None, "none"), ("GET", "/auth", None, "none"),
            ("POST", "/auth", None, "none"), ("GET", "/history", None, "Bearer"),
            ("GET", "/history", None, "Bearer"),
        ])
        self.assertEqual([(w.method, w.path, w.filename, w.scheme) for w in self.gateway.wire], expected)
        sessions: list[int] = (
            [1] * 5 + [2] * 8 + [3] * 4 + [4] * 10 + [5, 6] + [4] * 20
            + [4, 4, 7, 4] + [8] * 5 + [9] * 4 + [10] * 2 + [11, 12]
        )
        self.assertEqual([w.session for w in self.gateway.wire], sessions)
        self.assertEqual([count for _, count in self.confirmations], [5, 13, 53, 58])
        self.assertEqual([w.host for w in self.gateway.wire],
                         ["gateway.local"] * 5 + ["10.10.0.1"] * 8
                         + ["gateway.local"] * 40 + ["10.10.0.1"] * 5 + ["gateway.local"] * 8)
        self.assertEqual(self.gateway.wire[11].body, {"ssid": None, "password": None})
        self.assertFalse([w for w in self.gateway.wire if w.method == "DELETE"])
        self.assertEqual(self.gateway.wire[56].body, {"ssid": None, "password": None})
        network: dict[str, Any] = {"use_eth": True, "eth_dhcp": True, "eth_static_ip": "", "eth_netmask": "", "eth_gw": "", "eth_dns1": "", "eth_dns2": ""}
        self.assertEqual(self.gateway.wire[10].body, network)
        self.assertEqual(self.gateway.wire[55].body, network)
        self.assertEqual(self.gateway.wire[12].body, {
            "mqtt_prefix": "ruuvi/AA:BB:CC:DD:EE:FF/", "mqtt_client_id": CONFIG.gw_mac,
            "mqtt_data_format": "ruuvi_raw",
        })
        self.assertEqual(self.gateway.wire[57].body, {
            "mqtt_prefix": "ruuvi/AA:BB:CC:DD:EE:FF/", "mqtt_client_id": CONFIG.gw_mac,
            "mqtt_data_format": "ruuvi_raw",
        })
        self.assertEqual(self.elapsed, 0.0)
        self.assertFalse(self.gateway.ap_active)
        self.assertEqual(self.input_prompts, ["Press Enter when ready, or type q then Enter to stop: "] * 4)
        self.assertIn("Wi-Fi network 'Configure Ruuvi Gateway EEFF'", self.confirmations[0][0])
        self.assertIn("DHCP to assign the host an IP address", self.confirmations[0][0])
        self.assertIn("Gateway API at http://10.10.0.1", self.confirmations[0][0])
        default_ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        self.assertEqual(self.gateway.wire[1].body, {
            "login": "Admin", "password": hashlib.sha256(f"challenge-1-1:{default_ha1}".encode()).hexdigest(),
        })
        wire: WireRequest
        for wire in self.gateway.wire:
            self.assertFalse(wire.redirects)
            self.assertFalse(wire.trust_env)
            self.assertEqual(wire.timeout, (5, 15))
            if wire.method in ("GET", "DELETE"):
                self.assertIsNone(wire.body)
            if wire.method == "POST" and wire.scheme == "Bearer":
                self.assertEqual(wire.body, {
                    "mqtt_prefix": "cra/224a0dd91f13f268d34c57ae/",
                    "mqtt_client_id": CONFIG.gw_mac, "mqtt_data_format": "ruuvi_raw",
                })
            if wire.path == "/ssl_cert" and wire.method == "POST":
                self.assertTrue(wire.body.startswith("-----BEGIN "))
        self.assertEqual(self.gateway.wire[4].body, {"ssid": self.fixtures.ssid, "password": self.fixtures.wifi_password})
        self.assertEqual(self.gateway.wire[6].body, self.gateway.wire[4].body)
        self.assertEqual(self.gateway.wire[6].host, "10.10.0.1")
        self.assertEqual(self.gateway.wire[4].host, "gateway.local")
        self.assertNotEqual(self.gateway.wire[27].session, self.gateway.wire[28].session)
        self.assertNotEqual(self.gateway.wire[27].session, self.gateway.wire[19].session)
        expected_provision: dict[str, Any] = {
            "lan_auth_type": "lan_auth_ruuvi", "lan_auth_user": self.fixtures.username,
            "lan_auth_pass": hashlib.md5(f"{self.fixtures.username}:Ruuvi Gateway:{self.fixtures.password}".encode()).hexdigest(),
            "lan_auth_api_key": self.fixtures.ro_key, "lan_auth_api_key_rw": self.fixtures.rw_key,
            "use_http": True,
            "http_url": "http://192.0.2.20:18080/ea5ad4bd4cbb99e79877b71d/telemetry",
            "http_auth": "basic", "http_user": "http-954be3cccea9", "http_pass": "ee780c695fc221c0467153f9",
            "use_mqtt": False, "mqtt_server": "test.mosquitto.org", "mqtt_port": 1883,
            "mqtt_user": "rw", "mqtt_pass": "readwrite",
            "mqtt_prefix": "cra/224a0dd91f13f268d34c57ae/", "mqtt_client_id": CONFIG.gw_mac,
            "mqtt_data_format": "ruuvi_raw", "remote_cfg_use": False,
            "remote_cfg_url": "http://192.0.2.20:18080/ce29ebdbdb064347ab84ec05/config.json",
            "remote_cfg_auth_type": "basic", "remote_cfg_auth_basic_user": "rcfg-7f1ede685ff2",
            "remote_cfg_auth_basic_pass": "2b8d0a740fd3fa6c06aaff57",
        }
        provision: dict[str, Any] = self.gateway.wire[16].body
        self.assertEqual(provision, expected_provision)
        # Exact field scope is independent of the runner's payload construction.
        self.assertEqual(set(provision), {
            "lan_auth_type", "lan_auth_user", "lan_auth_pass", "lan_auth_api_key", "lan_auth_api_key_rw",
            "use_http", "http_url", "http_auth", "http_user", "http_pass", "use_mqtt", "mqtt_server",
            "mqtt_port", "mqtt_user", "mqtt_pass", "mqtt_prefix", "mqtt_client_id", "mqtt_data_format",
            "remote_cfg_use", "remote_cfg_url", "remote_cfg_auth_type", "remote_cfg_auth_basic_user", "remote_cfg_auth_basic_pass",
        })
        field: str
        for field in ("lan_auth_type", "lan_auth_user", "lan_auth_pass", "lan_auth_api_key", "lan_auth_api_key_rw", "use_http"):
            self.assertEqual(provision[field], expected_provision[field])
        self.assertEqual(provision["mqtt_client_id"], CONFIG.gw_mac)
        self.assertEqual(provision["mqtt_server"], "test.mosquitto.org")
        self.assertEqual(provision["mqtt_port"], 1883)
        self.assertEqual(provision["http_auth"], "basic")
        self.assertEqual(provision["remote_cfg_auth_type"], "basic")
        self.assertFalse(provision["remote_cfg_use"])
        self.assertTrue(self.gateway.http_fixture is not None and self.gateway.http_fixture.closed)
        self.assertEqual(len([line for line in self.output if line.startswith("Open log file:")]), 1)
        progress: list[str] = [line for line in self.output if line.startswith("[Step")]
        self.assertEqual(len(progress), 9)
        index: int
        for index, line in enumerate(progress, 1):
            self.assertTrue(line.startswith(f"[Step {index} out of 9]"))
            self.assertIn(line, self.log())
        self.assertNotIn(self.fixtures.password, "\n".join(self.output))
        self.assertIn(self.fixtures.password, self.log())
        self.assertLess(self.log().index("BENCH PREPARATION:"), self.log().index("HTTP REQUEST BEGIN"))
        preparation: list[str] = [line for line in self.output if line.startswith("BENCH PREPARATION:")]
        self.assertEqual(len(preparation), 1)

    def test_preconfigured_station_wifi_gets_guidance_before_baseline_rejection(self) -> None:
        self.gateway.public["wifi_sta_config"] = {"ssid": "synthetic-already-configured"}
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["baseline"], "ERROR")
        self.assertFalse([w for w in self.gateway.wire if w.path == "/connect.json"])
        text: str = "\n".join(self.output)
        fragment: str
        for fragment in (
            "factory-default Ethernet settings and no saved Wi-Fi station credentials",
            "Start on LAN with the configuration hotspot closed",
            "Opening it with CONFIGURE disconnects the Gateway's active LAN interface",
            "hotspot requests bypass authentication",
            "Changing gw_hostname to 10.10.0.1 will not fix that",
            "The runner pauses for you to press CONFIGURE and switch the host network",
            "Preconfiguring station Wi-Fi violates the empty-credentials baseline",
            "Confirm each network-change prompt to continue",
        ):
            self.assertIn(fragment, text)
            self.assertIn(fragment, self.log())
        self.assertLess(self.log().index("BENCH PREPARATION:"), self.log().index("HTTP REQUEST BEGIN"))

    def test_missing_default_reference_is_explained_before_network_or_prompts(self) -> None:
        portable: Path = self.directory / "gw_cfg_default_gen_ui.json"
        with patch.object(config_module, "DEFAULT_GATEWAY_UI_CONFIG_PATH", self.directory / "missing.json"), \
                patch.object(config_module, "PORTABLE_GATEWAY_UI_CONFIG_PATH", portable):
            result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes["baseline"]), (2, "ERROR"))
        self.assertFalse(self.gateway.wire)
        self.assertFalse(self.confirmations)
        self.assertIsNone(result.recovery_message)
        self.assertIn(f"to {portable}", "\n".join(self.output))
        self.assertIn("Missing factory-default reference file", self.log())
        self.assertFalse(self.manifest()["restored"]["attempted"])
        self.assertIsNone(self.manifest()["baseline"])

    def test_portable_defaults_allow_complete_run_and_hotspot_prompts(self) -> None:
        portable: Path = self.directory / "gw_cfg_default_gen_ui.json"
        portable.write_text(json.dumps(load_ui_default_config()), encoding="utf-8")
        with patch.object(config_module, "DEFAULT_GATEWAY_UI_CONFIG_PATH", self.directory / "missing.json"), \
                patch.object(config_module, "PORTABLE_GATEWAY_UI_CONFIG_PATH", portable):
            result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0, self.log())
        self.assertEqual(len(self.confirmations), 4)
        self.assert_restored()

    def test_unavailable_hotspot_stops_normal_checks_and_reports_unverified_recovery(self) -> None:
        self.gateway.hotspot_available = False
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
        self.assertEqual(result.outcomes[runner.CERTS], "NOT RUN")
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])
        self.assertFalse(any(w.method == "POST" and w.host == "10.10.0.1" for w in self.gateway.wire))
        self.assertIn("LAN transition incomplete", "\n".join(self.output))
        self.assertIsNotNone(result.recovery_message)

    def test_confirmation_rejects_unknown_input_and_recovers_after_cancellation(self) -> None:
        self.answers = ["later", "q"]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual([count for _, count in self.confirmations[:2]], [5, 5])
        self.assertIn("Press Enter to continue", "\n".join(self.output))
        self.assertIn("Operator cancelled", "\n".join(self.output))
        self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
        self.assert_restored()

    def test_prompt_uses_gateway_mac_suffix_for_network_name(self) -> None:
        mac: str = "C8:25:2D:8E:9C:2C"
        (self.directory / ".env").write_text(env_text(DutConfig(CONFIG.gw_id, mac, CONFIG.gw_hostname)) + self.wifi_env(), encoding="utf-8")
        self.gateway.public["gw_mac"] = mac
        self.gateway.initial["gw_mac"] = mac
        self.gateway.public.update(mqtt_prefix=f"ruuvi/{mac}/", mqtt_client_id=mac)
        self.gateway.initial.update(mqtt_prefix=f"ruuvi/{mac}/", mqtt_client_id=mac)
        self.gateway.reset_mac = mac
        self.gateway.hotspot_mac = mac
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0, self.log())
        self.assertIn("Wi-Fi network 'Configure Ruuvi Gateway 9C2C'", self.confirmations[0][0])
        self.assertIn("Wi-Fi network 'Configure Ruuvi Gateway 9C2C'", self.confirmations[2][0])
        self.assert_restored()

    def test_failed_finish_request_stops_normal_checks_and_recovers(self) -> None:
        self.gateway.finish_failures = 1
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
        self.assertEqual(self.elapsed, 90.0)
        self.assertIn("HTTP STATUS 500", self.log())
        self.assert_restored()

    def test_lost_finish_reply_still_verifies_lan_and_restores(self) -> None:
        self.gateway.lose_finish_reply = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
        self.assertEqual(result.outcomes[runner.ADMIN], "PASS")
        self.assertEqual(self.elapsed, 0.0)
        self.assert_restored()

    def test_eof_or_interrupt_does_not_silently_confirm_or_mutate_hotspot(self) -> None:
        interruption: BaseException
        for interruption in (EOFError(), KeyboardInterrupt()):
            with self.subTest(interruption=type(interruption).__name__):
                self.gateway = CreationGateway()
                self.answers = [interruption]
                self.confirmations = []
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(len(self.confirmations), 1)
                self.assertFalse(any(w.host == "10.10.0.1" for w in self.gateway.wire))
                self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
                self.assertEqual(result.outcomes["factory reset"], "ERROR")
                self.assertIsNotNone(result.recovery_message)

    def test_lost_wifi_or_ethernet_reply_still_returns_to_lan_and_restores(self) -> None:
        field: str
        for field in ("lose_wifi_reply", "lose_eth_reply"):
            with self.subTest(field=field):
                self.setUp()
                setattr(self.gateway, field, True)
                self.confirmations = []
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
                self.assertEqual(result.outcomes[runner.ADMIN], "PASS")
                self.assertEqual(len(self.confirmations), 4)
                self.assert_restored()

    def test_delayed_wifi_readback_is_polled_before_returning_to_ethernet(self) -> None:
        self.gateway.delayed_wifi_reads = 2
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0, self.log())
        self.assertEqual(self.elapsed, 4.0)
        self.assert_restored()

    def test_cancelled_lan_confirmation_after_wifi_write_still_cleans_up(self) -> None:
        self.answers = ["yes", "q"]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
        self.assertEqual(len(self.confirmations), 4)
        self.assert_restored()

    def test_declined_factory_reset_leaves_recovery_unverified(self) -> None:
        self.answers = ["", "", "q"]
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])
        self.assertTrue(self.gateway.files)
        self.assertEqual(len(self.confirmations), 3)

    def test_wrong_lan_identity_after_transition_stops_before_custom_provisioning(self) -> None:
        original: Callable[[str], str] = self.confirm

        def confirm(prompt: str) -> str:
            answer: str = original(prompt)
            self.gateway.lan_identity = "00:11:22:33:44:55" if len(self.confirmations) == 2 else None
            return answer

        self.confirm = confirm
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
        self.assertEqual(self.elapsed, 0.0)
        self.assertFalse(any(w.path == "/ruuvi.json" and w.method == "POST" and w.host == "gateway.local" for w in self.gateway.wire))
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(any("factory reset verified Gateway" in text for text, _ in self.confirmations))

    def test_return_to_lan_waits_for_readiness_before_other_mutations(self) -> None:
        original: Callable[[str], str] = self.confirm

        def confirm(prompt: str) -> str:
            answer: str = original(prompt)
            if len(self.confirmations) == 2:
                self.gateway.lan_failures = 2
            return answer

        self.confirm = confirm
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0, self.log())
        self.assertEqual(self.elapsed, 4.0)
        self.assertEqual([(w.method, w.path) for w in self.gateway.wire[13:17]],
                         [("GET", "/auth"), ("GET", "/auth"), ("GET", "/auth"), ("POST", "/auth")])
        self.assert_restored()

    def test_lan_timeout_stops_normal_checks_but_allows_recovery(self) -> None:
        original: Callable[[str], str] = self.confirm

        def confirm(prompt: str) -> str:
            answer: str = original(prompt)
            self.gateway.lan_failures = 100 if len(self.confirmations) == 2 else 0
            return answer

        self.confirm = confirm
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(self.elapsed, 90.0)
        self.assertEqual(result.outcomes[runner.ADMIN], "NOT RUN")
        self.assertIn("LAN did not recover", "\n".join(self.output))
        self.assert_restored()

    def test_hidden_target_secrets_are_proven_by_saved_secret_checks(self) -> None:
        self.gateway.expose_secrets = False
        self.gateway.expose_blobs = False
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        self.assertEqual(result.outcomes[runner.KEYS], "PASS")
        self.assertEqual(result.outcomes[runner.CERTS], "PASS")
        self.assertEqual(result.outcomes[runner.WIFI], "PASS")
        password: dict[str, Any] = next(i for i in self.manifest()["items"] if i["field_or_file"] == "wifi_sta_config.password")
        self.assertTrue(password["committed"])
        self.assertEqual(password["verification"], "indirect")
        self.assertIsNone(password["readback_sha256"])
        status_reads: list[WireRequest] = [w for w in self.gateway.wire if w.path == "/status.json"]
        self.assertEqual([(w.method, w.host, w.body) for w in status_reads], [("GET", "10.10.0.1", None)])
        category: str
        for category in (runner.HTTP, runner.MQTT, runner.REMOTE):
            self.assertEqual(result.outcomes[category], "PASS")
        items: list[dict[str, Any]] = [i for i in self.manifest()["items"] if i["category"] == runner.CERTS]
        self.assertTrue(all(i["verification"] == "indirect" and i["readback_sha256"] is None for i in items))
        self.assert_restored()

    def test_wifi_status_does_not_accept_ethernet_failure_or_unspecified_address(self) -> None:
        status: dict[str, Any]
        for status in (
            {"ssid": None, "urc": 0, "ip": "192.0.2.10"},
            {"ssid": "Other", "urc": 0, "ip": "192.0.2.10"},
            {"ssid": self.fixtures.ssid, "urc": 1, "ip": "192.0.2.10"},
            {"ssid": self.fixtures.ssid, "urc": 3, "ip": "192.0.2.10"},
            {"ssid": self.fixtures.ssid, "urc": 0, "ip": "0.0.0.0"},
            {"ssid": self.fixtures.ssid, "urc": 0, "ip": "127.0.0.1"},
        ):
            with self.subTest(status=status):
                self.setUp()
                self.gateway.expose_secrets = False
                self.gateway.wifi_status_override = status
                result: RunResult = self.execute()
                self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
                self.assertEqual(self.elapsed, 30.0)
                password: dict[str, Any] = next(i for i in self.manifest()["items"] if i["field_or_file"] == "wifi_sta_config.password")
                self.assertFalse(password["committed"])
                self.assert_restored()

    def test_malformed_or_rejected_wifi_status_never_proves_password(self) -> None:
        status: dict[str, Any]
        for status in ({}, {"ssid": self.fixtures.ssid, "urc": False, "ip": "192.0.2.10"},
                       {"ssid": self.fixtures.ssid, "urc": 0, "ip": "invalid"}):
            with self.subTest(status=status):
                self.setUp()
                self.gateway.expose_secrets = False
                self.gateway.wifi_status_override = status
                result: RunResult = self.execute()
                self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
                self.assert_restored()
        self.setUp()
        self.gateway.wifi_status_http = 503
        result = self.execute()
        self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
        self.assert_restored()

    def test_mixed_bundle_reports_actionable_setup_error_before_dut_access(self) -> None:
        @dataclass(frozen=True)
        class LegacyDutConfig:
            gw_id: str
            gw_mac: str
            gw_hostname: str

        with patch.dict(vars(runner), DutConfig=LegacyDutConfig):
            result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(self.gateway.wire, [])
        self.assertIn("Do not mix runner/library versions", "\n".join(self.output))

    def test_latest_live_scenario_keeps_firmware_and_restoration_failures(self) -> None:
        self.gateway.expose_secrets = False
        self.gateway.expose_blobs = False
        self.gateway.noop_mutates = True
        self.gateway.retain_saved_wifi_on_clear = True
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes[runner.WIFI], "PASS")
        self.assertEqual(result.outcomes[runner.KEYS], "FAIL")
        self.assertEqual(result.outcomes["factory reset"], "PASS")
        self.assertIn("RW bearer preserving POST changed configuration fields: mqtt_prefix", "\n".join(self.output))
        self.assertTrue(self.manifest()["restored"]["verified"])

    def test_target_proof_queries_use_saved_secrets_and_separate_broker_ports(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        calls: list[WireRequest] = [w for w in self.gateway.wire if w.path == "/validate_url"]
        self.assertEqual(len(calls), 7)
        self.assertEqual([w.query["validate_type"][0] for w in calls],
                         ["check_post_advs"] * 2 + ["check_mqtt"] * 3 + ["check_remote_cfg"] * 2)
        self.assertEqual([w.query["url"][0] for w in calls[2:5]],
                         ["mqtt://test.mosquitto.org:1883", "mqtt://test.mosquitto.org:1884", "mqtt://test.mosquitto.org:1884"])
        self.assertEqual([w.query["auth_type"][0] for w in calls[2:5]], ["none", "basic", "basic"])
        self.assertEqual(calls[4].query["user"], ["rw"])
        call: WireRequest
        for call in calls:
            self.assertEqual(call.query["use_saved_password"], ["true"])
            self.assertEqual(call.query["use_ssl_client_cert"], ["false"])
            self.assertNotIn("password", call.query)
            self.assertNotIn("encrypted_password", call.query)
            self.assertNotIn("readwrite", json.dumps(call.query))
            self.assertNotIn(self.fixtures.ro_key, json.dumps(call.query))
            self.assertNotIn(self.fixtures.rw_key, json.dumps(call.query))
        self.assertTrue(calls[0].query["user"][0].startswith("invalid-"))
        self.assertTrue(calls[3].query["user"][0].startswith("invalid-"))
        self.assertTrue(calls[5].query["user"][0].startswith("invalid-"))
        self.assertTrue(calls[6].query["url"][0].endswith("/config.json"))
        self.assert_restored()

    def test_validation_application_error_or_boolean_status_is_not_success(self) -> None:
        status: int | bool
        for status in (200, 401, 502, True):
            with self.subTest(status=status):
                self.setUp()
                self.gateway.validation_override = status
                result: RunResult = self.execute()
                category: str
                for category in (runner.HTTP, runner.MQTT, runner.REMOTE):
                    self.assertEqual(result.outcomes[category], "ERROR")
                self.assert_restored()

    def test_remote_config_internal_field_name_is_rejected_after_http_success(self) -> None:
        original_factory: Callable[[str, tuple[HttpRoute, ...]], FakeHttpFixture] = self.make_http_fixture

        def invalid_fixture(hostname: str, routes: tuple[HttpRoute, ...]) -> FakeHttpFixture:
            invalid_routes: tuple[HttpRoute, ...] = tuple(
                replace(route, response=b'{"use_remote_cfg":true}') if route.method == "GET" else route
                for route in routes
            )
            return original_factory(hostname, invalid_routes)

        with patch.object(self, "make_http_fixture", side_effect=invalid_fixture):
            result: RunResult = self.execute()
        self.assertEqual(result.outcomes[runner.REMOTE], "ERROR")
        self.assertEqual(result.outcomes[runner.HTTP], "PASS")
        self.assertEqual(result.outcomes[runner.MQTT], "PASS")
        self.assertIn("expected application status 200, received 502", "\n".join(self.output))
        self.assert_restored()

    def test_fixture_start_failure_occurs_before_mutation_and_reset(self) -> None:
        with patch.object(self, "make_http_fixture", side_effect=runner.InvalidSetup("fixture bind failed")):
            result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(self.confirmations, [])
        self.assertFalse([w for w in self.gateway.wire if w.path != "/auth" and w.method == "POST"])
        self.assertIn("fixture bind failed", "\n".join(self.output))

    def test_complete_observed_mismatch_is_fail_and_survives_recovery_error(self) -> None:
        self.gateway.alter_fields["http_url"] = "https://127.0.0.1:18443/wrong"
        self.gateway.final_mismatch = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.HTTP], "FAIL")
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])
        self.assertIn("manifest", result.recovery_message or "")
        self.assertIn('"Custom HTTP telemetry target + secret": "FAIL"', self.log())

    def test_accepted_bad_blob_is_fail_with_successful_restoration(self) -> None:
        self.gateway.corrupt_file = "rcfg_cli_key"
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 1)
        self.assertEqual(result.outcomes[runner.CERTS], "FAIL")
        self.assert_restored()

    def test_partial_upload_is_error_and_every_attempted_file_is_deleted(self) -> None:
        self.gateway.fail_file = "mqtt_cli_cert"
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.CERTS], "ERROR")
        self.assertEqual(result.outcomes["state preservation"], "PASS")
        self.assertEqual(result.outcomes[runner.KEYS], "PASS")
        self.assertTrue([w for w in self.gateway.wire if w.method == "POST" and w.scheme == "Bearer"])
        self.assertFalse([w for w in self.gateway.wire if w.method == "DELETE"])
        self.assertFalse(self.gateway.files)
        self.assert_restored()

    def test_applied_upload_with_lost_reply_uses_observed_storage_for_preservation(self) -> None:
        self.gateway.fail_file = "mqtt_cli_cert"
        self.gateway.lose_upload_response = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.CERTS], "ERROR")
        self.assertEqual(result.outcomes["state preservation"], "PASS")
        self.assertEqual(result.outcomes[runner.KEYS], "PASS")
        self.assertIn("upload applied, reply lost", self.log())
        self.assert_restored()

    def test_unobservable_rejected_upload_is_preservation_error_without_rw_probe(self) -> None:
        self.gateway.fail_file = "mqtt_cli_cert"
        self.gateway.omit_unconfirmed_storage = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.CERTS], "ERROR")
        self.assertEqual(result.outcomes["state preservation"], "ERROR")
        self.assertEqual(result.outcomes[runner.KEYS], "ERROR")
        self.assertFalse([w for w in self.gateway.wire if w.method == "POST" and w.scheme == "Bearer"])
        self.assertIn("Unresolved upload storage outcome", "\n".join(self.output))
        self.assert_restored()

    def test_applied_write_with_lost_response_recovers_custom_candidate(self) -> None:
        self.gateway.lose_provision_response = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.ADMIN], "ERROR")
        self.assert_restored()
        self.assertIn("applied write, lost reply", self.log())

    def test_rejected_write_recovers_default_candidate(self) -> None:
        self.gateway.provision_status = 500
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored()
        self.assertEqual(self.gateway.mode, "lan_auth_default")

    def test_readback_exception_still_restores(self) -> None:
        self.gateway.read_failure = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored()

    def test_rw_workaround_still_rejects_mqtt_and_unrelated_mutations(self) -> None:
        field: str
        for field in ("mqtt_prefix", "coordinates"):
            with self.subTest(field=field):
                self.setUp()
                self.gateway.noop_mutates = True
                self.gateway.noop_mutation_field = field
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 1)
                self.assertEqual(result.outcomes[runner.KEYS], "FAIL")
                self.assertEqual(result.outcomes[runner.ADMIN], "PASS")
                self.assertIn(f"RW bearer preserving POST changed configuration fields: {field}", "\n".join(self.output))
                self.assert_restored()

    def test_rw_workaround_uses_observed_client_id_and_records_workaround(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 0)
        bodies: list[dict[str, Any]] = [w.body for w in self.gateway.wire if w.method == "POST" and w.scheme == "Bearer"]
        self.assertEqual(bodies, [{
            "mqtt_prefix": "cra/224a0dd91f13f268d34c57ae/",
            "mqtt_client_id": CONFIG.gw_mac, "mqtt_data_format": "ruuvi_raw",
        }])
        self.assertIn("workaround for firmware issue #1650", "\n".join(self.output))
        rw_item: dict[str, Any] = next(i for i in self.manifest()["items"] if i["field_or_file"] == "lan_auth_api_key_rw")
        self.assertTrue(rw_item["committed"])
        self.assertIn("workaround for firmware issue #1650", rw_item["reason"])
        self.assert_restored()

    def test_late_safer_failure_prevents_rw_write(self) -> None:
        self.gateway.side_effect = True
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes["state preservation"], "FAIL")
        self.assertFalse([w for w in self.gateway.wire if w.scheme == "Bearer" and w.method == "POST"])
        self.assertEqual(result.outcomes["factory reset"], "PASS")
        self.assertEqual(result.outcomes[runner.KEYS], "ERROR")
        self.assertIn("RO/RW reads completed; RW no-op POST not run", "\n".join(self.output))

    def test_no_station_connection_is_setup_error_and_does_not_block_rw_check(self) -> None:
        self.gateway.wifi_association_succeeds = False
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
        self.assertEqual(result.outcomes["state preservation"], "PASS")
        self.assertEqual(result.outcomes[runner.KEYS], "PASS")
        self.assertEqual(self.elapsed, 30.0)
        self.assertIn("A matching test access point is required", "\n".join(self.output))
        self.assertEqual([w.body for w in self.gateway.wire if w.method == "POST" and w.scheme == "Bearer"], [{
            "mqtt_prefix": "cra/224a0dd91f13f268d34c57ae/",
            "mqtt_client_id": CONFIG.gw_mac, "mqtt_data_format": "ruuvi_raw",
        }])
        items: list[dict[str, Any]] = [i for i in self.manifest()["items"] if i["category"] == runner.WIFI]
        self.assertEqual(len(items), 2)
        self.assertTrue(all(not i["committed"] and i["verification"] == "unverified" for i in items))
        self.assert_restored()

    def test_reported_live_scenario_preserves_hidden_errors_and_exposes_rw_failure(self) -> None:
        self.gateway.wifi_association_succeeds = False
        self.gateway.expose_secrets = False
        self.gateway.expose_blobs = False
        self.gateway.noop_mutates = True
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes, {
            "baseline": "PASS", runner.WIFI: "ERROR", runner.ADMIN: "PASS", runner.KEYS: "FAIL",
            runner.HTTP: "PASS", runner.MQTT: "PASS", runner.REMOTE: "PASS", runner.CERTS: "PASS",
            "state preservation": "PASS", "factory reset": "PASS", "manifest": "PASS",
        })
        self.assertEqual(result.exit_code, 2)
        self.assertNotIn("http_pass is redacted", "\n".join(self.output))
        self.assertNotIn(self.fixtures.wifi_password, "\n".join(self.output))
        self.assert_restored()

    def test_uncommitted_wifi_does_not_hide_an_unrelated_configuration_change(self) -> None:
        self.gateway.wifi_association_succeeds = False
        self.gateway.side_effect = True
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes[runner.WIFI], "ERROR")
        self.assertEqual(result.outcomes["state preservation"], "FAIL")
        self.assertEqual(result.outcomes[runner.KEYS], "ERROR")
        self.assertIn("Created configuration differs in field(s): coordinates", "\n".join(self.output))
        self.assertFalse(any(w.method == "POST" and w.scheme == "Bearer" for w in self.gateway.wire))

    def test_unexpected_saved_ssid_remains_failure(self) -> None:
        self.gateway.wrong_wifi_ssid = "unexpected-test-network"
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes[runner.WIFI], "FAIL")
        self.assertEqual(result.outcomes["state preservation"], "FAIL")
        self.assertIn("Created configuration differs in field(s): wifi_sta_config", "\n".join(self.output))
        self.assert_restored()

    def test_surviving_credentials_and_keys_prevent_pass(self) -> None:
        attribute: str
        for attribute in ("survive_admin", "survive_token", "final_mismatch"):
            with self.subTest(attribute=attribute):
                self.gateway = CreationGateway()
                setattr(self.gateway, attribute, True)
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.outcomes["factory reset"], "ERROR")
                self.assertIsNotNone(result.recovery_message)
                # Separate invocations produce collision-safe evidence; remove only test artifacts.
                path: Path
                for path in (self.directory / "logs").iterdir():
                    path.unlink()

    def test_reset_without_erasure_is_not_accepted(self) -> None:
        self.gateway.reset_completes = False
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])
        self.assertIn("Factory reset did not restore the complete baseline", "\n".join(self.output))

    def test_manifest_failure_after_creation_still_restores_and_keeps_checkpoint(self) -> None:
        calls: list[runner.Manifest] = []

        def writer(path: Path, manifest: runner.Manifest) -> None:
            calls.append(manifest)
            if len(calls) == 2:
                raise OSError("manifest full")
            runner.write_manifest(path, manifest)

        result: RunResult = self.execute(writer)
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes["factory reset"], "PASS")
        self.assertEqual(result.outcomes["manifest"], "ERROR")
        self.assertTrue(self.manifest()["restored"]["verified"])
        self.assertEqual(self.gateway.public, self.gateway.initial)

    def test_checkpoint_failure_prevents_mutation(self) -> None:
        def writer(_path: Path, _manifest: runner.Manifest) -> None:
            raise OSError("manifest not writable")

        result: RunResult = self.execute(writer)
        self.assertEqual(result.exit_code, 2)
        self.assertFalse([w for w in self.gateway.wire if w.method == "POST" and w.path != "/auth"])
        self.assertEqual(self.gateway.public, self.gateway.initial)

    def assert_setup_rejected_without_login(self, reason: str) -> None:
        result: RunResult = self.execute()
        self.assertEqual((result.exit_code, result.outcomes["baseline"]), (2, "ERROR"))
        self.assertIsNone(result.recovery_message)
        self.assertIn(reason, "\n".join(self.output))
        self.assertIn(reason, self.log())
        self.assertEqual([(w.method, w.path, w.redirects) for w in self.gateway.wire], [("GET", "/auth", False)])
        self.assertEqual(self.gateway.public, self.gateway.initial)
        self.assertFalse(self.manifest()["restored"]["attempted"])
        self.assertFalse(self.manifest()["items"])
        self.assertNotIn(FACTORY_RESET_MESSAGE, "\n".join(self.output))
        self.assertTrue(all(result.outcomes[c] == "NOT RUN" for c in CATEGORIES))

    def test_hostname_on_hotspot_reports_captive_portal_without_following_redirect(self) -> None:
        self.gateway.initial_auth_response = FakeResponse(
            302, headers={"Location": "http://10.10.0.1/"}, malformed_json=True,
        )
        self.assert_setup_rejected_without_login("GET /auth returned HTTP 302 to the configuration hotspot")
        self.assertNotIn("malformed JSON", self.log())

    def test_hotspot_ip_reports_authentication_bypass_without_reset_advice(self) -> None:
        self.gateway.host_network = "hotspot"
        (self.directory / ".env").write_text(
            env_text(DutConfig(CONFIG.gw_id, CONFIG.gw_mac, "10.10.0.1")) + self.wifi_env(), encoding="utf-8",
        )
        self.assert_setup_rejected_without_login("GET /auth identifies configuration-hotspot access (lan=false)")
        self.assertEqual(self.gateway.wire[0].host, "10.10.0.1")

    def test_unknown_redirect_is_not_followed_or_mislabeled_as_hotspot(self) -> None:
        self.gateway.initial_auth_response = FakeResponse(302, headers={"Location": "http://other.invalid/"})
        self.assert_setup_rejected_without_login("GET /auth returned HTTP 302; verify the LAN endpoint")
        self.assertNotIn("captive portal", "\n".join(self.output))

    def test_lan_unavailable_while_hotspot_active_reports_http_status(self) -> None:
        self.gateway.initial_auth_response = FakeResponse(503, malformed_json=True)
        self.assert_setup_rejected_without_login("GET /auth returned HTTP 503; verify LAN access with the hotspot closed")

    def test_setup_auth_modes_and_default_login_failure_keep_reset_advice(self) -> None:
        mode: str
        for mode in ("lan_auth_basic", "lan_auth_digest", "lan_auth_default"):
            with self.subTest(mode=mode):
                self.gateway = CreationGateway()
                self.gateway.mode = mode
                self.gateway.default_login_status = 401
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                self.assertIn(FACTORY_RESET_MESSAGE, "\n".join(self.output))
                self.assertFalse([w for w in self.gateway.wire if w.path != "/auth"])

    def test_wrong_missing_malformed_identity_never_recommends_reset(self) -> None:
        mac: Any
        for mac in (None, "00:11:22:33:44:55", "bad", 42):
            with self.subTest(mac=mac):
                self.gateway = CreationGateway()
                if mac is None:
                    self.gateway.public.pop("gw_mac")
                else:
                    self.gateway.public["gw_mac"] = mac
                self.gateway.public["use_http"] = False
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertIsNone(result.recovery_message)
                self.assertFalse([w for w in self.gateway.wire if w.path == "/connect.json"])

    def test_hotspot_wrong_identity_never_receives_mutations(self) -> None:
        self.gateway.hotspot_mac = "00:11:22:33:44:55"
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertFalse([w for w in self.gateway.wire if w.host == "10.10.0.1" and w.method == "POST"])
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])

    def test_default_fields_missing_unequal_and_wrong_type_are_setup_errors(self) -> None:
        # Drive the actual baseline gate, independently decoded for each scenario.
        defaults: dict[str, Any] = load_ui_default_config()
        field: str
        for field in ("use_http", "mqtt_port", "http_url", "storage", "wifi_sta_config"):
            replacement: Any
            for replacement in (None, [], 1 if isinstance(defaults[field], bool) else True, "nondefault"):
                with self.subTest(field=field, value=replacement):
                    self.gateway = CreationGateway()
                    if replacement is None:
                        self.gateway.public.pop(field)
                    else:
                        self.gateway.public[field] = json.loads(json.dumps(replacement))
                    result: RunResult = self.execute()
                    self.assertEqual(result.exit_code, 2)
                    self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                    self.assertFalse([w for w in self.gateway.wire if w.path == "/connect.json"])

    def test_unstable_and_malformed_baseline_prevent_writes(self) -> None:
        attribute: str
        for attribute in ("unstable_baseline", "malformed_read"):
            with self.subTest(attribute=attribute):
                self.gateway = CreationGateway()
                setattr(self.gateway, attribute, True)
                result: RunResult = self.execute()
                self.assertEqual(result.exit_code, 2)
                self.assertFalse([w for w in self.gateway.wire if w.path == "/connect.json"])

    def test_custom_resettable_settings_stop_before_mutation_or_reset_prompt(self) -> None:
        field: str
        value: Any
        for field, value in (
            ("eth_dhcp", False), ("eth_static_ip", "192.0.2.20"),
            ("eth_netmask", "255.255.255.0"), ("eth_gw", "192.0.2.1"),
            ("eth_dns1", "192.0.2.53"), ("eth_dns2", "192.0.2.54"),
            ("coordinates", "60.0,24.0"), ("wifi_ap_config", {"channel": 6}),
            ("mqtt_prefix", "custom/topic/"), ("mqtt_client_id", "custom-client"),
            ("ntp_server1", "custom.invalid"), ("auto_update_cycle", "manual"),
            ("scan_filter_list", ["AA:BB:CC:DD:EE:FF"]),
        ):
            with self.subTest(field=field):
                self.gateway = CreationGateway()
                self.gateway.public[field] = value
                self.confirmations.clear()
                result: RunResult = self.execute()
                self.assertEqual((result.exit_code, result.outcomes["baseline"]), (2, "ERROR"))
                self.assertEqual(result.recovery_message, FACTORY_RESET_MESSAGE)
                self.assertEqual(self.confirmations, [])
                self.assertFalse([w for w in self.gateway.wire if w.method == "POST" and w.path != "/auth"])

    def test_readback_type_traps_missing_and_unequal_values(self) -> None:
        field: str
        value: Any
        for field, value in (("use_http", 1), ("mqtt_port", True), ("mqtt_port", 7), ("http_user", [])):
            with self.subTest(field=field, value=value):
                self.gateway = CreationGateway()
                self.gateway.alter_fields[field] = value
                result: RunResult = self.execute()
                self.assertEqual(result.outcomes[runner.HTTP if field.startswith("http") or field == "use_http" else runner.MQTT], "FAIL")
        self.gateway = CreationGateway()
        self.gateway.omit_fields.add("http_url")
        result = self.execute()
        self.assertEqual(result.outcomes[runner.HTTP], "ERROR")

    def test_fixture_generation_uses_live_realm_and_matching_valid_pem(self) -> None:
        generated: runner.SyntheticData = runner.generate_fixtures("different realm", DeterministicBytes(), NOW)
        ha1: str = hashlib.md5(f"{generated.username}:different realm:{generated.password}".encode()).hexdigest()
        self.assertEqual(next(f.value for f in generated.scalars if f.field == "lan_auth_pass"), ha1)
        self.assertNotEqual(ha1, hashlib.md5(f"{generated.username}:{CONFIG.gw_hostname}:{generated.password}".encode()).hexdigest())
        self.assertNotEqual(generated.ro_key, generated.rw_key)
        self.assertLessEqual(len(generated.ssid), 32)
        self.assertGreaterEqual(len(generated.wifi_password), 8)
        self.assertEqual(tuple(p.filename for p in generated.certificates), FILES)
        cert_der: bytes = PEM.decode(generated.certificates[0].pem)[0]
        certificate: DerSequence = DerSequence().decode(cert_der)
        tbs: DerSequence = DerSequence().decode(certificate[0])
        public: ECC.EccKey = ECC.import_key(tbs[5])
        private: ECC.EccKey = ECC.import_key(generated.certificates[1].pem)
        self.assertEqual(public.pointQ, private.pointQ)
        with self.assertRaises(FrozenInstanceError):
            setattr(generated, "username", "mutated")  # noqa: B010 - Exercise frozen assignment without a static read-only error.
        # The default uses secrets.token_bytes; no network or subprocess is needed.
        random_source: Mock
        with patch.object(runner.secrets, "token_bytes", side_effect=DeterministicBytes()) as random_source:
            runner.generate_fixtures("realm", runner.secrets.token_bytes, NOW)
            self.assertGreater(random_source.call_count, 15)

    def test_wrong_identity_after_reset_gets_no_network_writes(self) -> None:
        self.gateway.reset_mac = "00:11:22:33:44:55"
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes["factory reset"], "ERROR")
        self.assertFalse(self.manifest()["restored"]["verified"])
        reset_index: int = self.confirmations[2][1]
        self.assertEqual([(w.method, w.path) for w in self.gateway.wire[reset_index:]], [("GET", "/ruuvi.json")])

    def test_interactive_recovery_failure_uses_saved_rw_candidate(self) -> None:
        self.gateway.reject_custom_login = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assert_restored()
        self.assertFalse([w for w in self.gateway.wire if w.method == "POST" and w.path == "/ruuvi.json" and w.scheme == "Bearer"])

    def test_saved_secret_checks_need_server_witness_not_only_validation_success(self) -> None:
        self.gateway.omit_fixture_witness = True
        self.gateway.expose_secrets = False
        result: RunResult = self.execute()
        self.assertEqual(result.outcomes[runner.HTTP], "ERROR")
        self.assertEqual(result.outcomes[runner.REMOTE], "ERROR")
        self.assertIn("no matching local authenticated/rejected request evidence", "\n".join(self.output))
        self.assert_restored()

    def test_missing_storage_presence_flag_is_error_not_success(self) -> None:
        self.gateway.expose_blobs = False
        self.gateway.missing_flag = "http_cli_cert"
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.CERTS], "ERROR")
        self.assert_restored()

    def test_rw_write_is_skipped_after_failed_positive_read(self) -> None:
        self.gateway.fail_keys_read = True
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.outcomes[runner.KEYS], "ERROR")
        self.assertFalse([w for w in self.gateway.wire if w.scheme == "Bearer" and w.method == "POST"])
        self.assert_restored()

    def test_setup_failure_still_creates_evidence(self) -> None:
        (self.directory / ".env").unlink()
        result: RunResult = self.execute()
        self.assertEqual(result.exit_code, 2)
        self.assertFalse(self.gateway.wire)
        self.assertIn("OVERALL VERDICT: ERROR", self.log())



class HttpFixtureTests(unittest.TestCase):
    def test_witness_only_accepts_exact_route_method_and_credentials(self) -> None:
        witness: CredentialWitness = CredentialWitness((HttpRoute("/nonce/config.json", "GET", "u", "p", b'{"remote_cfg_use":true}'),))
        good: str = "Basic " + base64.b64encode(b"u:p").decode()
        wrong: str = "Basic " + base64.b64encode(b"u:wrong").decode()
        self.assertEqual(witness.respond("GET", "/other", good)[0], 404)
        self.assertEqual(witness.respond("POST", "/nonce/config.json", good)[0], 405)
        self.assertEqual(witness.respond("GET", "/nonce/config.json", wrong)[0], 401)
        self.assertEqual(witness.respond("GET", "/nonce/config.json", "")[0], 401)
        self.assertEqual(witness.respond("GET", "/nonce/config.json", good), (200, b'{"remote_cfg_use":true}'))
        self.assertEqual(witness.counts("/nonce/config.json"), (1, 2))

    def test_route_selection_and_unusable_address_errors(self) -> None:
        connection: Mock = Mock()
        factory: Mock = Mock()
        factory.return_value.__enter__ = Mock(return_value=connection)
        factory.return_value.__exit__ = Mock(return_value=False)
        with patch.object(fixture_module.socket, "socket", factory):
            connection.getsockname.return_value = ("192.0.2.20", 1234)
            self.assertEqual(fixture_module.host_lan_address("gateway.local"), "192.0.2.20")
            connection.connect.assert_called_once_with(("gateway.local", 80))
            connection.send.assert_not_called()
            connection.getsockname.return_value = ("127.0.0.1", 1234)
            with self.assertRaises(runner.InvalidSetup):
                fixture_module.host_lan_address("gateway.local")
            connection.connect.side_effect = OSError("route failed")
            with self.assertRaisesRegex(runner.InvalidSetup, "Cannot select"):
                fixture_module.host_lan_address("gateway.local")

    def test_server_lifecycle_and_http_handler_without_real_sockets(self) -> None:
        server: Mock = Mock(server_port=18080)
        thread: Mock = Mock()
        server_factory: Mock
        with patch.object(fixture_module, "host_lan_address", return_value="192.0.2.20"), \
                patch.object(fixture_module.http.server, "ThreadingHTTPServer", return_value=server) as server_factory, \
                patch.object(fixture_module.threading, "Thread", return_value=thread):
            fixture: fixture_module.LocalHttpFixture = fixture_module.LocalHttpFixture(
                "gateway.local", (HttpRoute("/test", "POST", "u", "p"),),
            )
            self.assertEqual(fixture.base_url, "http://192.0.2.20:18080")
            handler_type: type[BaseHTTPRequestHandler] = server_factory.call_args[0][1]
            handler: BaseHTTPRequestHandler = handler_type.__new__(handler_type)
            handler.rfile = io.BytesIO(b"POST /test HTTP/1.1\r\nHost: fixture\r\nContent-Length: 2\r\nAuthorization: Basic dTpw\r\n\r\n{}")
            output: io.BytesIO = io.BytesIO()
            handler.wfile = output
            handler.handle_one_request()
            self.assertIn(b"200 OK", output.getvalue())
            self.assertIn(b"Content-Length: 2", output.getvalue())
            self.assertEqual(fixture.counts("/test"), (1, 0))
            handler.rfile = io.BytesIO(b"POST /test HTTP/1.1\r\nHost: fixture\r\nContent-Length: 2\r\nAuthorization: Basic dTp3cm9uZw==\r\n\r\n{}")
            handler.handle_one_request()
            self.assertIn(b"401 Unauthorized", output.getvalue())
            self.assertEqual(fixture.counts("/test"), (1, 1))
            fixture.close()
            thread.start.assert_called_once()
            server.shutdown.assert_called_once()
            server.server_close.assert_called_once()
            thread.join.assert_called_once_with(timeout=5.0)

    def test_bind_and_thread_start_failures_close_resources(self) -> None:
        with patch.object(fixture_module, "host_lan_address", return_value="192.0.2.20"), \
                patch.object(fixture_module.http.server, "ThreadingHTTPServer", side_effect=OSError("bind")), \
                self.assertRaisesRegex(runner.InvalidSetup, "Cannot bind"):
            fixture_module.LocalHttpFixture("gateway.local", ())
        server: Mock = Mock(server_port=18080)
        with patch.object(fixture_module, "host_lan_address", return_value="192.0.2.20"), \
                patch.object(fixture_module.http.server, "ThreadingHTTPServer", return_value=server), \
                patch.object(fixture_module.threading.Thread, "start", side_effect=RuntimeError("thread")), \
                self.assertRaisesRegex(RuntimeError, "thread"):
            fixture_module.LocalHttpFixture("gateway.local", ())
        server.server_close.assert_called_once()

if __name__ == "__main__":
    unittest.main()
