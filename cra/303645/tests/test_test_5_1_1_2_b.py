"""Implementation tests only; these are not ETSI functional-test evidence."""

import base64
import json
import sys
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union
from unittest import mock
from urllib.parse import urlsplit

import requests
from Crypto.PublicKey import ECC

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_5_1_1_2_b as target  # noqa: E402
from lib.gateway import (  # noqa: E402
    AuthMech,
    GatewayApi,
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
)
from lib.http_api import (  # noqa: E402
    HttpAuthScheme,
    HttpHeader,
    HttpMethod,
    HttpStatus,
)

FIXED_NOW = datetime(2026, 8, 30, 9, 36, 1, 123456, tzinfo=timezone.utc)
CONFIG = target.DutConfig(
    gw_id="00:11:22:33:44:55:66:77",
    gw_mac="AA:BB:CC:DD:EE:FF",
    gw_hostname="gateway.local",
)


def env_line(field: str, value: str) -> str:
    return f"{field}={value}\n"


def env_text(config: target.DutConfig = CONFIG) -> str:
    return (
        f"{env_line(GatewayCfgDesc.GW_ID, config.gw_id)}"
        f"{env_line(GatewayCfgDesc.GW_MAC, config.gw_mac)}"
        f"{env_line(GatewayCfgDesc.GW_HOSTNAME, config.gw_hostname)}"
    )


class FakeCookies:
    def __init__(self, values: Optional[Dict[str, str]] = None) -> None:
        self.values = values or {}

    def get(self, key: str) -> Optional[str]:
        return self.values.get(key)

    def get_dict(self) -> Dict[str, str]:
        return dict(self.values)


class FakeResponse:
    def __init__(
            self,
            status: int,
            payload: Any = None,
            headers: Optional[Dict[str, str]] = None,
            cookies: Optional[Dict[str, str]] = None,
            malformed_json: bool = False,
    ) -> None:
        self.status_code = status
        self.headers = headers or {}
        self.cookies = FakeCookies(cookies)
        self._payload = payload
        self._malformed_json = malformed_json
        self.text = "{broken" if malformed_json else json.dumps(payload) if payload is not None else ""

    def json(self) -> Any:
        if self._malformed_json:
            raise ValueError("malformed")
        return self._payload


@dataclass(frozen=True)
class FakeRequestKey:
    scheme: str
    method: str
    path: str


@dataclass(frozen=True)
class RecordedRequest:
    scheme: str
    method: str
    path: str
    body: Any
    allow_redirects: bool

    @property
    def key(self) -> FakeRequestKey:
        return FakeRequestKey(self.scheme, self.method, self.path)


class FakeGateway:
    def __init__(self) -> None:
        self.calls: List[RecordedRequest] = []
        self.override: Dict[FakeRequestKey, Union[FakeResponse, BaseException]] = {}
        self.auth_mode = GatewayCfgLanAuthType.DEFAULT
        self.auth_user = target.ADMIN_USERNAME
        self.ro_enabled = False
        self.rw_enabled = False
        self.final_mutation = False
        self.config_reads = 0
        self.challenge_headers = True
        self.connection_error: Optional[BaseException] = None
        self.server_key = ECC.generate(curve="secp256r1")
        server_public = self.server_key.public_key()
        raw = (
                b"\x04"
                + int(server_public.pointQ.x).to_bytes(32, "big")
                + int(server_public.pointQ.y).to_bytes(32, "big")
        )
        self.server_public_b64 = base64.b64encode(raw).decode("ascii")

    @staticmethod
    def _scheme(headers: Dict[str, str]) -> str:
        authorization = headers.get(HttpHeader.AUTHORIZATION, "")
        return authorization.split(" ", 1)[0] if authorization else "none"

    def response_for(
            self,
            session: "FakeSession",
            method: str,
            path: str,
            headers: Dict[str, str],
            body: Any,
            allow_redirects: bool,
    ) -> FakeResponse:
        scheme = self._scheme(headers)
        self.calls.append(
            RecordedRequest(scheme, method, path, body, allow_redirects)
        )
        if self.connection_error is not None:
            error = self.connection_error
            self.connection_error = None
            raise error
        request_key = FakeRequestKey(scheme, method, path)
        if request_key in self.override:
            override = self.override[request_key]
            if isinstance(override, BaseException):
                raise override
            return override

        if scheme == HttpAuthScheme.BEARER:
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})
        if path == GatewayApi.AUTH and method == HttpMethod.GET:
            if session.authorized:
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                )
            session_id = f"session-{session.number}"
            if self.auth_mode == GatewayCfgLanAuthType.BASIC:
                auth_header = 'Basic realm="Ruuvi Gateway"'
            elif self.auth_mode == GatewayCfgLanAuthType.DIGEST:
                auth_header = (
                    'Digest realm="Ruuvi Gateway", qop="auth", nonce="nonce", '
                    'opaque="opaque"'
                )
            else:
                auth_header = (
                    'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="challenge" '
                    f'session_cookie="RUUVISESSION" session_id="{session_id}"'
                )
            headers_out = {HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.server_public_b64}
            if self.challenge_headers:
                headers_out[HttpHeader.WWW_AUTHENTICATE] = auth_header
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode},
                headers=headers_out,
                cookies={"RUUVISESSION": session_id},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.POST:
            if isinstance(body, dict) and body.get("login") == target.ADMIN_USERNAME:
                session.authorized = True
                return FakeResponse(HttpStatus.C_200_OK, {"authenticated": True})
            return FakeResponse(
                HttpStatus.C_401_UNAUTHORIZED,
                {"authenticated": False},
            )
        if path == GatewayApi.AUTH and method == HttpMethod.DELETE:
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})
        if session.authorized:
            if path == GatewayApi.CONFIG:
                self.config_reads += 1
                payload = {
                    GatewayCfgDesc.LAN_AUTH_TYPE: self.auth_mode,
                    GatewayCfgDesc.LAN_AUTH_USER: self.auth_user,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_USE: self.ro_enabled,
                    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: self.rw_enabled,
                    GatewayCfgDesc.GW_MAC: CONFIG.gw_mac,
                    GatewayCfgDesc.FW_VER: "test",
                    "stable": "changed" if self.final_mutation and self.config_reads > 1 else "value",
                }
                return FakeResponse(HttpStatus.C_200_OK, payload)
            if path == GatewayApi.STATUS:
                return FakeResponse(HttpStatus.C_200_OK, {"status": "ok"})
        return FakeResponse(
            HttpStatus.C_302_FOUND
            if method == HttpMethod.GET
            else HttpStatus.C_401_UNAUTHORIZED,
            {"error": "unauthorized"},
        )


class FakeSession:
    next_number = 1

    def __init__(self, gateway: FakeGateway) -> None:
        self.gateway = gateway
        self.authorized = False
        self.number = FakeSession.next_number
        FakeSession.next_number += 1

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        return request.prepare()

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        method = request.method
        url = request.url
        if method is None or url is None:
            raise ValueError("prepared request must contain a method and URL")
        body = json.loads(request.body) if request.body else None
        return self.gateway.response_for(
            self,
            method,
            urlsplit(url).path,
            dict(request.headers),
            body,
            kwargs["allow_redirects"],
        )


class FunctionalTestCase(unittest.TestCase):
    def setUp(self) -> None:
        FakeSession.next_number = 1
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.gateway = FakeGateway()
        self.log = target.EvidenceLog.create(
            self.root / "logs",
            "test_5_1_1_2_b",
            lambda: FIXED_NOW,
        )
        self.addCleanup(self._close_log)

    def _close_log(self) -> None:
        if not self.log._stream.closed:
            self.log.finish("TEST")

    def make_runner(self) -> target.FunctionalTest_5_1_1_2_b:
        return target.FunctionalTest_5_1_1_2_b(
            CONFIG,
            self.log,
            session_factory=lambda: FakeSession(self.gateway),
            random_bytes=lambda size: bytes((index % 251 for index in range(size))),
        )

    def test_complete_mocked_sequence_passes_and_disables_redirects(self) -> None:
        result = self.make_runner().run()
        self.assertEqual(0, result.exit_code)
        self.assertEqual("PASS", result.verdict)
        self.assertEqual(target.EXPECTED_API_INVENTORY, result.coverage)
        self.assertTrue(all(value == "PASS" for value in result.outcomes.values()))
        negative_calls = [
            call
            for call in self.gateway.calls
            if not (
                    call.path in {GatewayApi.CONFIG, GatewayApi.STATUS}
                    and call.scheme == "none"
            )
        ]
        self.assertTrue(all(call.allow_redirects is False for call in negative_calls))
        first_write = next(
            index for index, call in enumerate(self.gateway.calls)
            if call.method in {HttpMethod.POST, HttpMethod.DELETE}
            and call.path != GatewayApi.AUTH
        )
        last_interactive_get = max(
            index for index, call in enumerate(self.gateway.calls)
            if call.method == HttpMethod.GET
            and call.scheme in {HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST}
        )
        last_bearer_get = max(
            index for index, call in enumerate(self.gateway.calls)
            if call.method == HttpMethod.GET
            and call.scheme == HttpAuthScheme.BEARER
        )
        self.assertLess(last_interactive_get, first_write)
        self.assertLess(last_bearer_get, first_write)

    def test_each_negative_scheme_accepts_only_expected_route_statuses(self) -> None:
        for scheme in (None, HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST):
            with self.subTest(scheme=scheme):
                gateway = FakeGateway()
                runner = target.FunctionalTest_5_1_1_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda gateway=gateway: FakeSession(gateway),
                    random_bytes=lambda size: b"x" * size,
                )
                runner._probe_interactive_group(HttpMethod.GET, scheme)
                runner._probe_interactive_group(target.SAFE_WRITE, scheme)
                runner._probe_interactive_group(target.DANGEROUS_WRITE, scheme)
                expected_scheme = scheme if scheme is not None else "none"
                expected_calls: List[Tuple[str, str, str, Optional[Dict[str, Any]]]] = [
                    (expected_scheme, route.method, route.path, None)
                    for route in target.API_INVENTORY
                    if route.method == HttpMethod.GET and route.path != GatewayApi.AUTH
                ]
                if scheme is None:
                    expected_calls.append(
                        ("none", HttpMethod.DELETE, GatewayApi.AUTH, None)
                    )
                expected_calls.extend(
                    (expected_scheme, route.method, route.path,
                     {} if route.method == HttpMethod.POST else None)
                    for route in target.API_INVENTORY
                    if route.method != HttpMethod.GET and route.path != GatewayApi.AUTH
                )
                self.assertEqual(
                    expected_calls,
                    [
                        (call.scheme, call.method, call.path, call.body)
                        for call in gateway.calls
                    ],
                )

    def test_bearer_matrix_uses_safe_order_and_least_operative_bodies(self) -> None:
        runner = self.make_runner()
        runner._probe_bearer_group(HttpMethod.GET)
        runner._probe_bearer_group(target.SAFE_WRITE)
        self.assertEqual("NOT RUN", runner.outcomes[AuthMech.M2M_API_BEARER_RW])
        runner._probe_bearer_group(target.DANGEROUS_WRITE)
        reads = [route for route in target.API_INVENTORY if route.method == HttpMethod.GET]
        session_writes = [
            route for route in target.API_INVENTORY
            if route.method != HttpMethod.GET and route.path == GatewayApi.AUTH
        ]
        mutating_routes = [
            route for route in target.API_INVENTORY
            if route.method != HttpMethod.GET and route.path != GatewayApi.AUTH
        ]
        self.assertEqual(
            [
                (HttpAuthScheme.BEARER, route.method, route.path,
                 {} if route.method == HttpMethod.POST else None)
                for route in reads + session_writes + mutating_routes
            ],
            [(call.scheme, call.method, call.path, call.body) for call in self.gateway.calls],
        )

    def test_all_safe_probes_finish_before_any_potentially_mutating_probe(self) -> None:
        result = self.make_runner().run()
        self.assertEqual("PASS", result.verdict)
        first_mutating = next(
            index for index, call in enumerate(self.gateway.calls)
            if call.method != HttpMethod.GET and call.path != GatewayApi.AUTH
        )
        first_probe = next(
            index for index, call in enumerate(self.gateway.calls)
            if call.key == FakeRequestKey("none", HttpMethod.GET, GatewayApi.AP)
        )
        expected_safe: List[Tuple[str, str, str, Optional[Dict[str, Any]]]] = [
            (scheme, route.method, route.path, None)
            for scheme in ("none", HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST, HttpAuthScheme.BEARER)
            for route in target.API_INVENTORY
            if route.method == HttpMethod.GET
               and (route.path != GatewayApi.AUTH or scheme == HttpAuthScheme.BEARER)
        ]
        expected_safe.extend([
            ("none", HttpMethod.DELETE, GatewayApi.AUTH, None),
            (HttpAuthScheme.BEARER, HttpMethod.POST, GatewayApi.AUTH, {}),
            (HttpAuthScheme.BEARER, HttpMethod.DELETE, GatewayApi.AUTH, None),
        ])
        self.assertEqual(
            expected_safe,
            [
                (call.scheme, call.method, call.path, call.body)
                for call in self.gateway.calls[first_probe:first_mutating]
            ],
        )

    def test_session_write_failure_aborts_before_mutating_probes(self) -> None:
        for scheme, method in (
                ("none", HttpMethod.DELETE),
                (HttpAuthScheme.BEARER, HttpMethod.POST),
                (HttpAuthScheme.BEARER, HttpMethod.DELETE),
        ):
            with self.subTest(scheme=scheme, method=method):
                self.gateway = FakeGateway()
                failing_key = FakeRequestKey(scheme, method, GatewayApi.AUTH)
                self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
                result = self.make_runner().run()
                self.assertEqual("FAIL", result.verdict)
                self.assertEqual(failing_key, self.gateway.calls[-1].key)
                self.assertEqual([], [
                    call for call in self.gateway.calls
                    if call.method != HttpMethod.GET and call.path != GatewayApi.AUTH
                ])

    def test_mutating_probe_success_aborts_immediately(self) -> None:
        for scheme in ("none", HttpAuthScheme.BASIC, HttpAuthScheme.DIGEST, HttpAuthScheme.BEARER):
            for path in (GatewayApi.CONFIG, GatewayApi.FW_UPDATE_RESET, GatewayApi.INIT_STORAGE):
                with self.subTest(scheme=scheme, path=path):
                    self.gateway = FakeGateway()
                    failing_key = FakeRequestKey(scheme, HttpMethod.POST, path)
                    self.gateway.override[failing_key] = FakeResponse(HttpStatus.C_200_OK, {})
                    result = self.make_runner().run()
                    self.assertEqual((1, "FAIL"), (result.exit_code, result.verdict))
                    self.assertEqual(failing_key, self.gateway.calls[-1].key)
                    self.assertEqual(1, self.gateway.config_reads)

    def test_unexpected_success_aborts_immediately(self) -> None:
        self.gateway.override[
            FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.AP)
        ] = FakeResponse(
            HttpStatus.C_200_OK,
            {},
        )
        result = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.verdict)
        failing_index = next(
            index for index, call in enumerate(self.gateway.calls)
            if call.key
            == FakeRequestKey(HttpAuthScheme.BASIC, HttpMethod.GET, GatewayApi.AP)
        )
        self.assertEqual(failing_index, len(self.gateway.calls) - 1)

    def test_wrong_bearer_status_fails_and_aborts(self) -> None:
        self.gateway.override[
            FakeRequestKey(HttpAuthScheme.BEARER, HttpMethod.GET, GatewayApi.STATUS)
        ] = FakeResponse(HttpStatus.C_403_FORBIDDEN, {})
        result = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual(
            FakeRequestKey(HttpAuthScheme.BEARER, HttpMethod.GET, GatewayApi.STATUS),
            self.gateway.calls[-1].key,
        )

    def test_non_default_modes_are_setup_errors_requiring_factory_reset(self) -> None:
        for auth_mode in (
                GatewayCfgLanAuthType.BASIC,
                GatewayCfgLanAuthType.DIGEST,
        ):
            with self.subTest(auth_mode=auth_mode):
                gateway = FakeGateway()
                gateway.auth_mode = auth_mode
                runner = target.FunctionalTest_5_1_1_2_b(
                    CONFIG,
                    self.log,
                    session_factory=lambda gateway=gateway: FakeSession(gateway),
                )
                result = runner.run()
                self.assertEqual(2, result.exit_code)
                self.assertEqual("ERROR", result.verdict)
                self.assertEqual(
                    target.FACTORY_RESET_MESSAGE,
                    result.recovery_message,
                )

    def test_enabled_api_key_flags_are_setup_errors(self) -> None:
        for attribute in ("ro_enabled", "rw_enabled"):
            with self.subTest(attribute=attribute):
                gateway = FakeGateway()
                setattr(gateway, attribute, True)
                runner = target.FunctionalTest_5_1_1_2_b(
                    CONFIG, self.log, session_factory=lambda gateway=gateway: FakeSession(gateway)
                )
                result = runner.run()
                self.assertEqual(2, result.exit_code)

    def test_non_default_auth_user_requires_factory_reset(self) -> None:
        self.gateway.auth_user = "NotAdmin"
        result = self.make_runner().run()
        self.assertEqual((2, "ERROR"), (result.exit_code, result.verdict))
        self.assertEqual(target.FACTORY_RESET_MESSAGE, result.recovery_message)

    def test_timeout_and_connection_failure_are_errors(self) -> None:
        for error in (
                requests.Timeout("timeout"),
                requests.ConnectionError("connection"),
        ):
            with self.subTest(error=type(error).__name__):
                gateway = FakeGateway()
                gateway.connection_error = error
                runner = target.FunctionalTest_5_1_1_2_b(
                    CONFIG, self.log, session_factory=lambda gateway=gateway: FakeSession(gateway)
                )
                self.assertEqual(2, runner.run().exit_code)

    def test_malformed_json_is_error(self) -> None:
        self.gateway.override[
            FakeRequestKey("none", HttpMethod.GET, GatewayApi.AUTH)
        ] = FakeResponse(
            HttpStatus.C_401_UNAUTHORIZED,
            malformed_json=True,
            headers={
                HttpHeader.WWW_AUTHENTICATE: (
                    'x-ruuvi-interactive realm="r", challenge="c", '
                    'session_cookie="RUUVISESSION", session_id="s"'
                ),
                HttpHeader.RUUVI_ECDH_PUBLIC_KEY: self.gateway.server_public_b64,
            },
            cookies={"RUUVISESSION": "s"},
        )
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_missing_auth_header_is_error(self) -> None:
        self.gateway.challenge_headers = False
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_appended_basic_challenge_is_error(self) -> None:
        response = self.gateway.response_for(
            FakeSession(self.gateway),
            HttpMethod.GET,
            GatewayApi.AUTH,
            {HttpHeader.RUUVI_ECDH_PUBLIC_KEY: "key"},
            None,
            False,
        )
        response.headers[HttpHeader.WWW_AUTHENTICATE] = (
            f'{response.headers[HttpHeader.WWW_AUTHENTICATE]}, '
            f'{HttpAuthScheme.BASIC} realm="unexpected"'
        )
        self.gateway.override[
            FakeRequestKey("none", HttpMethod.GET, GatewayApi.AUTH)
        ] = response
        self.assertEqual(2, self.make_runner().run().exit_code)

    def test_delete_auth_failure_is_attributed_to_user_defined_mechanism(self) -> None:
        self.gateway.override[
            FakeRequestKey("none", HttpMethod.DELETE, GatewayApi.AUTH)
        ] = FakeResponse(HttpStatus.C_200_OK, {})
        result = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[AuthMech.LAN_WEBUI_USER_DEFINED])
        self.assertEqual("NOT RUN", result.outcomes[AuthMech.LAN_WEBUI_DIGEST])

    def test_final_state_mismatch_fails(self) -> None:
        self.gateway.final_mutation = True
        result = self.make_runner().run()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.verdict)


class ConfigurationAndLoggingTestCase(unittest.TestCase):
    def write_env(self, root: Path, text: str) -> Path:
        path = root / ".env"
        path.write_text(text, encoding="utf-8")
        return path

    def test_execute_maps_configuration_error_to_exit_code_2_and_logs_it(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            logs = root / "logs"
            with mock.patch("builtins.print"):
                result = target.execute_test_5_1_1_2_b(root, now=lambda: FIXED_NOW)
            self.assertEqual(2, result.exit_code)
            content = next(logs.iterdir()).read_text(encoding="utf-8")
            self.assertIn("InvalidConfig", content)
            self.assertIn("OVERALL VERDICT: ERROR", content)

    def test_execute_uses_current_directory_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(target.Path, "cwd", return_value=root):
                with mock.patch("builtins.print"):
                    result = target.execute_test_5_1_1_2_b(now=lambda: FIXED_NOW)
            self.assertEqual(2, result.exit_code)
            self.assertTrue((root / "logs").is_dir())

    def test_execute_prints_log_path_once_and_reports_all_progress_steps(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_env(
                root,
                env_text(),
            )
            gateway = FakeGateway()
            messages = []
            result = target.execute_test_5_1_1_2_b(
                root,
                session_factory=lambda: FakeSession(gateway),
                now=lambda: FIXED_NOW,
                output=messages.append,
            )
            self.assertEqual(0, result.exit_code)
            log_path = str(next((root / "logs").iterdir()))
            self.assertEqual(1, sum(log_path in message for message in messages))
            progress = [message for message in messages if message.startswith("[Step ")]
            self.assertEqual(target.TOTAL_STEPS, len(progress))
            log_lines = Path(log_path).read_text(encoding="utf-8").splitlines()
            for index, message in enumerate(progress, 1):
                self.assertTrue(
                    message.startswith(f"[Step {index} out of {target.TOTAL_STEPS}] ")
                )
                matching_log_lines = [line for line in log_lines if line.endswith(message)]
                self.assertEqual(1, len(matching_log_lines))
                self.assertRegex(
                    matching_log_lines[0],
                    r"^\[\d{4}-\d{2}-\d{2}T.*Z\] \[Step ",
                )
            self.assertEqual("Overall verdict: PASS", messages[-1])


if __name__ == "__main__":
    unittest.main()
