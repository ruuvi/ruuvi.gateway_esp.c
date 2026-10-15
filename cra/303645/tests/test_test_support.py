"""Regression checks for shared offline fixtures, not live-DUT compliance evidence."""

from __future__ import annotations

import hashlib
import io
import json
import unittest
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from unittest import mock

import requests

from lib.errors import GatewayAuthenticationModeError
from lib.evidence import EvidenceLog
from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig
from test_support.fake_gateway import DefaultAuthGateway, FakeGateway, FakeResponse, FakeSession

CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")


def login_body(config: DutConfig, challenge: str) -> dict[str, str]:
    ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{config.gw_id}".encode()).hexdigest()
    return {"login": "Admin", "password": hashlib.sha256(f"{challenge}:{ha1}".encode()).hexdigest()}


class ResponseTestCase(unittest.TestCase):
    def test_falsey_payloads_headers_and_cookies_are_preserved(self) -> None:
        payload: Any
        for payload in (None, False, 0, "", [], {}):
            with self.subTest(payload=payload):
                response: FakeResponse = FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED,
                    payload,
                    {HttpHeader.WWW_AUTHENTICATE: "challenge"},
                    {"RUUVISESSION": "cookie"},
                )
                self.assertIs(payload, response.json())
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, response.status_code)
                self.assertEqual("challenge", response.headers[HttpHeader.WWW_AUTHENTICATE])
                self.assertEqual("cookie", response.cookies["RUUVISESSION"])

    def test_json_errors_preserve_injected_exception_and_malformed_wire_body(self) -> None:
        error: ValueError = ValueError("injected JSON failure")
        response: FakeResponse = FakeResponse(json_error=error)
        raised_error: ValueError
        try:
            response.json()
        except ValueError as raised_error:
            self.assertIs(error, raised_error)
        else:
            self.fail("the injected JSON error was not raised")
        malformed: FakeResponse = FakeResponse(malformed_json=True)
        self.assertEqual("{broken", malformed.text)
        with self.assertRaisesRegex(ValueError, "malformed"):
            malformed.json()


class GatewayTestCase(unittest.TestCase):
    def authenticated_session(self, gateway: FakeGateway) -> FakeSession:
        stream: io.StringIO = io.StringIO()
        self.addCleanup(stream.close)
        log: EvidenceLog = EvidenceLog(Path("<memory>"), stream, datetime(2025, 1, 1, tzinfo=timezone.utc))
        client: GatewayClient = GatewayClient(gateway.dut_config, log, session_factory=lambda: FakeSession(gateway))
        login: InteractiveAuthResult = client.authenticate_interactive("Admin", gateway.dut_config.gw_id)
        self.assertEqual(HttpStatus.C_200_OK, login.login_response.status_code)
        if not isinstance(login.session, FakeSession):
            self.fail("client did not use the injected fake session")
        self.assertEqual(login.session.cookie, login.session.cookies.get("RUUVISESSION"))
        return login.session

    def test_authenticated_requests_persist_cookie_and_accept_other_cookie_pairs(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            session: FakeSession = self.authenticated_session(gateway)
            path: str
            for path in (GatewayApi.CONFIG, GatewayApi.STATUS, GatewayApi.HISTORY, GatewayApi.AP, GatewayApi.AUTH):
                with self.subTest(gateway=gateway_type.__name__, path=path):
                    response: requests.Response = session.get(f"{CONFIG.base_url}{path}", allow_redirects=False)
                    self.assertEqual(HttpStatus.C_200_OK, response.status_code)
                    self.assertEqual(f"RUUVISESSION={session.cookie}", gateway.calls[-1].cookie)
            response = session.post(f"{CONFIG.base_url}{GatewayApi.CONFIG}", json={}, allow_redirects=False)
            self.assertEqual(HttpStatus.C_200_OK, response.status_code)
            self.assertEqual([{}], gateway.config_bodies)
            response = session.get(
                f"{CONFIG.base_url}{GatewayApi.STATUS}",
                headers={"cookie": f"other=value; RUUVISESSION={session.cookie}; another=value"},
                allow_redirects=False,
            )
            self.assertEqual(HttpStatus.C_200_OK, response.status_code)
            self.assertEqual(f"other=value; RUUVISESSION={session.cookie}; another=value", gateway.calls[-1].cookie)

    def test_ruuvi_mode_still_accepts_interactive_login_and_cookie_reads(self) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        gateway.mode = GatewayCfgLanAuthType.RUUVI
        gateway.custom_username = "Admin"
        gateway.custom_ha1 = hashlib.md5(f"Admin:Ruuvi Gateway:{CONFIG.gw_id}".encode()).hexdigest()
        session: FakeSession = self.authenticated_session(gateway)
        response: requests.Response = session.get(f"{CONFIG.base_url}{GatewayApi.CONFIG}", allow_redirects=False)
        self.assertEqual(HttpStatus.C_200_OK, response.status_code)
        self.assertEqual(GatewayCfgLanAuthType.RUUVI, response.json()[GatewayCfgDesc.LAN_AUTH_TYPE])

    def test_all_session_authorized_routes_reject_missing_wrong_and_swapped_cookies(self) -> None:
        gateway_type: type[FakeGateway]
        method: str
        path: str
        cookie_kind: str
        routes: tuple[tuple[str, str], ...] = (
            (HttpMethod.POST, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.CONFIG),
            (HttpMethod.GET, GatewayApi.STATUS), (HttpMethod.GET, GatewayApi.HISTORY),
            (HttpMethod.GET, GatewayApi.AP), (HttpMethod.GET, GatewayApi.AUTH),
        )
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            for method, path in routes:
                for cookie_kind in ("missing", "empty", "wrong", "swapped", "duplicate"):
                    with self.subTest(gateway=gateway_type.__name__, method=method, path=path, cookie=cookie_kind):
                        gateway: FakeGateway = gateway_type(CONFIG)
                        session: FakeSession = self.authenticated_session(gateway)
                        other: FakeSession = self.authenticated_session(gateway)
                        original_cookie: str = session.cookie
                        cookie: str = {
                            "empty": "", "wrong": "RUUVISESSION=wrong",
                            "swapped": f"RUUVISESSION={other.cookie}",
                            "duplicate": f"RUUVISESSION={session.cookie}; RUUVISESSION=wrong",
                            "missing": "",
                        }[cookie_kind]
                        session.cookies.clear()
                        headers: dict[str, str] = {} if cookie_kind == "missing" else {HttpHeader.COOKIE: cookie}
                        response: requests.Response = session.request(
                            method, f"{CONFIG.base_url}{path}", headers=headers,
                            json={GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "changed"} if method == HttpMethod.POST else None,
                            allow_redirects=False,
                        )
                        expected: int = (HttpStatus.C_302_FOUND
                                         if gateway_type is DefaultAuthGateway and method == HttpMethod.GET
                                         and path != GatewayApi.AUTH else HttpStatus.C_401_UNAUTHORIZED)
                        self.assertEqual(expected, response.status_code)
                        self.assertEqual(None if cookie_kind == "missing" else cookie, gateway.calls[-1].cookie)
                        self.assertEqual([], gateway.config_bodies)
                        self.assertEqual("", gateway.rw_key)
                        if path == GatewayApi.AUTH:
                            self.assertFalse(session.authorized)
                            self.assertNotIn(session, gateway.authorized_sessions)
                            self.assertEqual(session.cookie, session.cookies.get("RUUVISESSION"))
                            response = session.get(f"{CONFIG.base_url}{GatewayApi.CONFIG}", allow_redirects=False)
                            self.assertNotEqual(HttpStatus.C_200_OK, response.status_code)
                        else:
                            self.assertEqual(original_cookie, session.cookie)

    def test_bearer_only_access_does_not_require_session_cookie(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            gateway.ro_key = "read-only"
            gateway.rw_key = "read-write"
            session: FakeSession = FakeSession(gateway)
            session.trust_env = False
            token: str
            for token in (gateway.ro_key, gateway.rw_key):
                path: str
                for path in (GatewayApi.CONFIG, GatewayApi.STATUS, GatewayApi.HISTORY, GatewayApi.AP):
                    with self.subTest(gateway=gateway_type.__name__, token=token, path=path):
                        response: requests.Response = session.get(
                            f"{CONFIG.base_url}{path}",
                            headers={HttpHeader.AUTHORIZATION: f"Bearer {token}"}, allow_redirects=False,
                        )
                        expected: int = (HttpStatus.C_401_UNAUTHORIZED if token == gateway.ro_key
                                         and path == GatewayApi.AP else HttpStatus.C_200_OK)
                        self.assertEqual(expected, response.status_code)
                        self.assertIsNone(gateway.calls[-1].cookie)
                response = session.post(
                    f"{CONFIG.base_url}{GatewayApi.CONFIG}", json={},
                    headers={HttpHeader.AUTHORIZATION: f"Bearer {token}"}, allow_redirects=False,
                )
                self.assertEqual(HttpStatus.C_200_OK if token == gateway.rw_key else HttpStatus.C_401_UNAUTHORIZED,
                                 response.status_code)
                self.assertFalse(session.authorized)

    def test_bearer_and_cookie_header_names_are_case_insensitive_and_recorded(self) -> None:
        gateway_type: type[FakeGateway]
        authorization_name: str
        cookie_name: str
        token: str
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            gateway.ro_key = "read-only"
            session: FakeSession = FakeSession(gateway)
            session.trust_env = False
            self.addCleanup(session.close)
            for authorization_name, cookie_name in (("Authorization", "Cookie"), ("authorization", "cookie"),
                                                   ("aUtHoRiZaTiOn", "cOoKiE")):
                for token in ("read-only", "wrong"):
                    with self.subTest(gateway=gateway_type.__name__, header=authorization_name, token=token):
                        response: requests.Response = session.get(
                            f"{CONFIG.base_url}{GatewayApi.STATUS}",
                            headers={authorization_name: f"Bearer {token}", cookie_name: "unrelated=value"},
                            allow_redirects=False,
                        )
                        self.assertEqual(HttpStatus.C_200_OK if token == "read-only" else HttpStatus.C_401_UNAUTHORIZED,
                                         response.status_code)
                        self.assertEqual(f"Bearer {token}", gateway.calls[-1].authorization)
                        self.assertEqual(token, gateway.calls[-1].bearer_token)
                        self.assertEqual("unrelated=value", gateway.calls[-1].cookie)

    def test_failed_login_removes_default_session_without_changing_cookie_or_other_sessions(self) -> None:
        gateway: DefaultAuthGateway = DefaultAuthGateway(CONFIG)
        session: FakeSession = self.authenticated_session(gateway)
        other: FakeSession = self.authenticated_session(gateway)
        original_cookie: str = session.cookie
        original_challenge: str = session.challenge
        self.assertEqual([session, other], gateway.authorized_sessions)
        session.cookies.clear()
        response: requests.Response = session.post(
            f"{CONFIG.base_url}{GatewayApi.AUTH}", json={"login": "Admin", "password": "wrong"},
            allow_redirects=False,
        )
        self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, response.status_code)
        self.assertEqual({"authenticated": False}, response.json())
        self.assertNotIn(HttpHeader.WWW_AUTHENTICATE, response.headers)
        self.assertEqual({"RUUVISESSION": original_cookie}, response.cookies.get_dict())
        self.assertFalse(session.authorized)
        self.assertEqual([other], gateway.authorized_sessions)
        self.assertTrue(other.authorized)
        self.assertEqual(original_cookie, session.cookie)
        self.assertEqual(original_cookie, session.cookies.get("RUUVISESSION"))
        self.assertEqual(original_challenge, session.challenge)
        self.assertEqual(HttpStatus.C_302_FOUND, session.get(
            f"{CONFIG.base_url}{GatewayApi.STATUS}", allow_redirects=False,
        ).status_code)
        self.assertEqual(HttpStatus.C_200_OK, other.get(
            f"{CONFIG.base_url}{GatewayApi.STATUS}", allow_redirects=False,
        ).status_code)

    def test_allow_and_deny_modes_apply_to_lan_routes_and_auth_state(self) -> None:
        routes: tuple[tuple[str, str], ...] = (
            (HttpMethod.GET, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.STATUS),
            (HttpMethod.GET, GatewayApi.HISTORY), (HttpMethod.GET, GatewayApi.AP),
            (HttpMethod.GET, GatewayApi.AUTH), (HttpMethod.POST, GatewayApi.CONFIG),
        )
        gateway_type: type[FakeGateway]
        mode: str
        method: str
        path: str
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            for mode in (GatewayCfgLanAuthType.ALLOW, GatewayCfgLanAuthType.DENY):
                for method, path in routes:
                    with self.subTest(gateway=gateway_type.__name__, mode=mode, method=method, path=path):
                        gateway: FakeGateway = gateway_type(CONFIG)
                        gateway.mode = mode
                        session: FakeSession = FakeSession(gateway)
                        session.trust_env = False
                        self.addCleanup(session.close)
                        body: dict[str, str] | None = (
                            {GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "updated"} if method == HttpMethod.POST else None
                        )
                        response: requests.Response = session.request(
                            method, f"{CONFIG.base_url}{path}", json=body, allow_redirects=False,
                        )
                        self.assertEqual(HttpStatus.C_200_OK if mode == GatewayCfgLanAuthType.ALLOW
                                         else HttpStatus.C_403_FORBIDDEN, response.status_code)
                        self.assertEqual([(method, path)], [(call.method, call.path) for call in gateway.calls])
                        self.assertEqual(body, gateway.calls[-1].body)
                        if method == HttpMethod.POST:
                            self.assertEqual([body] if mode == GatewayCfgLanAuthType.ALLOW else [], gateway.config_bodies)
                            self.assertEqual("updated" if mode == GatewayCfgLanAuthType.ALLOW else "", gateway.rw_key)
                        if path == GatewayApi.AUTH:
                            self.assertEqual(mode, response.json()[GatewayCfgDesc.LAN_AUTH_TYPE])
                            self.assertEqual(mode == GatewayCfgLanAuthType.ALLOW, response.json()["authenticated"])
                            if mode == GatewayCfgLanAuthType.ALLOW:
                                self.assertNotIn(HttpHeader.WWW_AUTHENTICATE, response.headers)
                                self.assertNotIn(HttpHeader.RUUVI_ECDH_PUBLIC_KEY, response.headers)
                                self.assertEqual("", session.challenge)
                                self.assertEqual(session.cookie, response.cookies.get("RUUVISESSION"))
                                self.assertEqual([session], gateway.authorized_sessions)
                                cookie: str = session.cookie
                                response = session.get(f"{CONFIG.base_url}{path}", allow_redirects=False)
                                self.assertEqual(HttpStatus.C_200_OK, response.status_code)
                                self.assertEqual(cookie, session.cookie)
                                self.assertEqual({}, response.cookies.get_dict())
                                self.assertNotIn(HttpHeader.WWW_AUTHENTICATE, response.headers)
                                self.assertNotIn(HttpHeader.RUUVI_ECDH_PUBLIC_KEY, response.headers)
                            else:
                                self.assertEqual({}, response.cookies.get_dict())
                                self.assertEqual([], gateway.authorized_sessions)

    def test_allow_mode_is_rejected_by_interactive_client_without_posting_login(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            with self.subTest(gateway=gateway_type.__name__):
                gateway: FakeGateway = gateway_type(CONFIG)
                gateway.mode = GatewayCfgLanAuthType.ALLOW
                session: FakeSession = FakeSession(gateway)
                self.addCleanup(session.close)
                stream: io.StringIO = io.StringIO()
                self.addCleanup(stream.close)
                log: EvidenceLog = EvidenceLog(Path("<memory>"), stream, datetime(2025, 1, 1, tzinfo=timezone.utc))
                factory: mock.Mock = mock.Mock(return_value=session)
                client: GatewayClient = GatewayClient(CONFIG, log, session_factory=factory)
                caught: unittest.case._AssertRaisesContext
                with self.assertRaises(GatewayAuthenticationModeError) as caught:
                    client.authenticate_interactive("Admin", CONFIG.gw_id)
                self.assertEqual(GatewayCfgLanAuthType.ALLOW, caught.exception.auth_type)
                factory.assert_called_once_with()
                self.assertEqual([(HttpMethod.GET, GatewayApi.AUTH)], [(call.method, call.path) for call in gateway.calls])
                self.assertEqual(session.cookie, session.cookies.get("RUUVISESSION"))
                self.assertEqual("", session.challenge)
                self.assertTrue(session.authorized)
                self.assertEqual([session], gateway.authorized_sessions)

    def test_allow_and_deny_modes_preserve_bearer_precedence_outside_auth(self) -> None:
        mode: str
        token: str
        method: str
        path: str
        for mode in (GatewayCfgLanAuthType.ALLOW, GatewayCfgLanAuthType.DENY):
            gateway: FakeGateway = FakeGateway(CONFIG)
            gateway.mode = mode
            gateway.ro_key = "read-only"
            gateway.rw_key = "read-write"
            session: FakeSession = FakeSession(gateway)
            session.trust_env = False
            self.addCleanup(session.close)
            for token in ("read-only", "read-write", "wrong", ""):
                for method, path in ((HttpMethod.GET, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.STATUS),
                                     (HttpMethod.GET, GatewayApi.HISTORY), (HttpMethod.GET, GatewayApi.AP),
                                     (HttpMethod.POST, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.AUTH)):
                    with self.subTest(mode=mode, token=token, method=method, path=path):
                        response: requests.Response = session.request(
                            method, f"{CONFIG.base_url}{path}", headers={HttpHeader.AUTHORIZATION: f"Bearer {token}"},
                            json={} if method == HttpMethod.POST else None, allow_redirects=False,
                        )
                        allowed: bool = token == "read-write" or (
                            token == "read-only" and method == HttpMethod.GET and path != GatewayApi.AP
                        )
                        expected: int = HttpStatus.C_200_OK if allowed else HttpStatus.C_401_UNAUTHORIZED
                        if path == GatewayApi.AUTH:
                            expected = (HttpStatus.C_200_OK if mode == GatewayCfgLanAuthType.ALLOW
                                        else HttpStatus.C_403_FORBIDDEN)
                        self.assertEqual(expected, response.status_code)

    def test_get_auth_validates_basic_and_digest_and_challenges_invalid_credentials(self) -> None:
        gateway_type: type[FakeGateway]
        mode: str
        authorization: str
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
                gateway: FakeGateway = gateway_type(CONFIG)
                gateway.mode = mode
                gateway.custom_username = "user"
                gateway.rw_key = "read-write"
                if mode == GatewayCfgLanAuthType.BASIC:
                    gateway.custom_ha1 = "dXNlcjpwYXNz"
                    valid_header: str = "Basic dXNlcjpwYXNz"
                    wrong_header: str = "Basic dXNlcjp3cm9uZw=="
                    scheme: str = HttpAuthScheme.BASIC
                else:
                    gateway.custom_ha1 = hashlib.md5(b"user:Ruuvi Gateway:pass").hexdigest()
                    ha2: str = hashlib.md5(b"GET:/auth").hexdigest()
                    digest: str = hashlib.md5(f"{gateway.custom_ha1}:nonce:00000001:client:auth:{ha2}".encode()).hexdigest()
                    valid_header = ('Digest username="user", realm="Ruuvi Gateway", opaque="opaque", '
                                    'uri="/auth", nonce="nonce", nc=00000001, '
                                    f'cnonce="client", qop=auth, response="{digest}"')
                    wrong_header = valid_header.replace(digest, "wrong")
                    scheme = HttpAuthScheme.DIGEST
                session: FakeSession = FakeSession(gateway)
                session.trust_env = False
                self.addCleanup(session.close)
                for authorization in (valid_header, wrong_header, "", scheme, f'{scheme} username="user"',
                                      "Bearer read-write"):
                    with self.subTest(gateway=gateway_type.__name__, mode=mode, authorization=authorization):
                        response: requests.Response = session.get(
                            f"{CONFIG.base_url}{GatewayApi.AUTH}",
                            headers={"authorization": authorization}, allow_redirects=False,
                        )
                        valid: bool = authorization == valid_header
                        self.assertEqual(HttpStatus.C_200_OK if valid else HttpStatus.C_401_UNAUTHORIZED,
                                         response.status_code)
                        self.assertEqual({GatewayCfgDesc.LAN_AUTH_TYPE: mode, "authenticated": valid}, response.json())
                        self.assertEqual(authorization, gateway.calls[-1].authorization)
                        if valid:
                            self.assertNotIn(HttpHeader.WWW_AUTHENTICATE, response.headers)
                        else:
                            self.assertTrue(response.headers[HttpHeader.WWW_AUTHENTICATE].startswith(f"{scheme} "))
                        self.assertFalse(session.authorized)
                        self.assertEqual({}, session.cookies.get_dict())

    def test_basic_and_digest_authorize_lan_routes_without_changing_bearer_permissions(self) -> None:
        gateway_type: type[FakeGateway]
        mode: str
        method: str
        path: str
        authorization: str
        expected_status: int
        routes: tuple[tuple[str, str], ...] = (
            (HttpMethod.GET, GatewayApi.CONFIG), (HttpMethod.GET, GatewayApi.HISTORY),
            (HttpMethod.GET, GatewayApi.AP), (HttpMethod.GET, GatewayApi.STATUS),
            (HttpMethod.POST, GatewayApi.CONFIG),
        )
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
                for method, path in routes:
                    gateway: FakeGateway = gateway_type(CONFIG)
                    gateway.mode = mode
                    gateway.custom_username = "user"
                    gateway.ro_key = "read-only"
                    ha1: str = hashlib.md5(b"user:Ruuvi Gateway:pass").hexdigest()
                    ha2: str = hashlib.md5(f"{method}:{path}".encode()).hexdigest()
                    digest: str = hashlib.md5(f"{ha1}:nonce:00000001:client:auth:{ha2}".encode()).hexdigest()
                    if mode == GatewayCfgLanAuthType.BASIC:
                        gateway.custom_ha1 = "dXNlcjpwYXNz"
                        valid_header: str = "Basic dXNlcjpwYXNz"
                        wrong_header: str = "Basic dXNlcjp3cm9uZw=="
                        scheme: str = HttpAuthScheme.BASIC
                    else:
                        gateway.custom_ha1 = ha1
                        valid_header = ('Digest username="user", realm="Ruuvi Gateway", opaque="opaque", '
                                        f'uri="{path}", nonce="nonce", nc=00000001, '
                                        f'cnonce="client", qop=auth, response="{digest}"')
                        wrong_header = valid_header.replace(digest, "wrong")
                        scheme = HttpAuthScheme.DIGEST
                    cases: list[tuple[str, int]] = [
                        (valid_header, HttpStatus.C_200_OK), (wrong_header, HttpStatus.C_401_UNAUTHORIZED),
                        ("", HttpStatus.C_401_UNAUTHORIZED), ("Bearer invalid", HttpStatus.C_401_UNAUTHORIZED),
                        ("Bearer ", HttpStatus.C_401_UNAUTHORIZED), ("Bearer read-write", HttpStatus.C_200_OK),
                        ("Bearer read-only", HttpStatus.C_401_UNAUTHORIZED
                         if method == HttpMethod.POST or path == GatewayApi.AP else HttpStatus.C_200_OK),
                    ]
                    if mode == GatewayCfgLanAuthType.DIGEST:
                        wrong_method: str = HttpMethod.GET if method == HttpMethod.POST else HttpMethod.POST
                        wrong_ha2: str = hashlib.md5(f"{wrong_method}:{path}".encode()).hexdigest()
                        wrong_digest: str = hashlib.md5(
                            f"{ha1}:nonce:00000001:client:auth:{wrong_ha2}".encode(),
                        ).hexdigest()
                        cases.append((valid_header.replace(digest, wrong_digest), HttpStatus.C_401_UNAUTHORIZED))
                    session: FakeSession = FakeSession(gateway)
                    session.trust_env = False
                    self.addCleanup(session.close)
                    for authorization, expected_status in cases:
                        with self.subTest(gateway=gateway_type.__name__, mode=mode, method=method,
                                          path=path, authorization=authorization):
                            gateway.rw_key = "read-write"
                            gateway.config_bodies.clear()
                            body: dict[str, str] | None = (
                                {GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "updated"} if method == HttpMethod.POST else None
                            )
                            response: requests.Response = session.request(
                                method, f"{CONFIG.base_url}{path}", json=body,
                                headers={"authorization": authorization}, allow_redirects=False,
                            )
                            self.assertEqual(expected_status, response.status_code)
                            self.assertEqual((method, path, body, authorization), (
                                gateway.calls[-1].method, gateway.calls[-1].path,
                                gateway.calls[-1].body, gateway.calls[-1].authorization,
                            ))
                            self.assertIsNone(gateway.calls[-1].cookie)
                            self.assertFalse(session.authorized)
                            if expected_status != HttpStatus.C_200_OK:
                                self.assertEqual([], gateway.config_bodies)
                                self.assertEqual("read-write", gateway.rw_key)
                                if not authorization.startswith("Bearer "):
                                    self.assertTrue(response.headers[HttpHeader.WWW_AUTHENTICATE].startswith(scheme))
                            elif method == HttpMethod.POST:
                                self.assertEqual([body], gateway.config_bodies)
                                self.assertEqual("updated", gateway.rw_key)
                            elif path == GatewayApi.CONFIG:
                                self.assertEqual(CONFIG.gw_mac, response.json()[GatewayCfgDesc.GW_MAC])
                            elif path == GatewayApi.STATUS and authorization == valid_header:
                                self.assertEqual({"status": "ok"}, response.json())
                            else:
                                self.assertEqual({"data": []} if gateway_type is DefaultAuthGateway
                                                 and path == GatewayApi.HISTORY else {}, response.json())

    def test_non_interactive_modes_reject_pending_login_before_credentials_or_body(self) -> None:
        gateway_type: type[FakeGateway]
        mode: str
        body: Any
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            for mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST,
                         GatewayCfgLanAuthType.ALLOW, GatewayCfgLanAuthType.DENY):
                gateway: FakeGateway = gateway_type(CONFIG)
                session: FakeSession = FakeSession(gateway)
                gateway.response_for(session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
                challenge: str = session.challenge
                cookie: str = session.cookie
                valid_body: dict[str, str] = login_body(CONFIG, challenge)
                gateway.mode = mode
                gateway.wrong_login_success = True
                for body in (valid_body, None, []):
                    with self.subTest(gateway=gateway_type.__name__, mode=mode, body=body):
                        response: FakeResponse = gateway.response_for(
                            session, HttpMethod.POST, GatewayApi.AUTH,
                            {HttpHeader.COOKIE: f"RUUVISESSION={cookie}"}, body,
                        )
                        self.assertEqual(HttpStatus.C_503_SERVICE_UNAVAILABLE, response.status_code)
                        self.assertFalse(session.authorized)
                        self.assertEqual([], gateway.authorized_sessions)
                        self.assertEqual((challenge, cookie), (session.challenge, session.cookie))
                        self.assertTrue(gateway.wrong_login_success)

    def test_gateway_identity_and_session_counters_are_independent(self) -> None:
        other_config: DutConfig = DutConfig("11:22:33:44:55:66:77:88", "BB:CC:DD:EE:FF:00", "other.local")
        first: FakeGateway = FakeGateway(CONFIG)
        second: FakeGateway = FakeGateway(other_config)
        first_session: FakeSession = FakeSession(first)
        second_session: FakeSession = FakeSession(second)
        self.assertEqual((1, 1, 2), (first_session.number, second_session.number, FakeSession(first).number))
        first.ro_key = "first-only"
        self.assertEqual("", second.ro_key)
        second.response_for(second_session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
        response: FakeResponse = second.response_for(
            second_session, HttpMethod.POST, GatewayApi.AUTH,
            {HttpHeader.COOKIE: f"RUUVISESSION={second_session.cookie}"},
            login_body(other_config, second_session.challenge),
        )
        self.assertEqual(HttpStatus.C_200_OK, response.status_code)
        config_response: FakeResponse = second.response_for(
            second_session, HttpMethod.GET, GatewayApi.CONFIG,
            {HttpHeader.COOKIE: f"RUUVISESSION={second_session.cookie}"}, None,
        )
        self.assertEqual(other_config.gw_mac, config_response.json()[GatewayCfgDesc.GW_MAC])
        self.assertEqual([], first.calls)

    def test_login_rejects_cross_session_responses_and_replay(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            with self.subTest(gateway=gateway_type.__name__):
                gateway: FakeGateway = gateway_type(CONFIG)
                first: FakeSession = FakeSession(gateway)
                second: FakeSession = FakeSession(gateway)
                gateway.response_for(first, HttpMethod.GET, GatewayApi.AUTH, {}, None)
                gateway.response_for(second, HttpMethod.GET, GatewayApi.AUTH, {}, None)
                body: dict[str, str] = login_body(CONFIG, first.challenge)
                denied: FakeResponse = gateway.response_for(
                    second, HttpMethod.POST, GatewayApi.AUTH,
                    {HttpHeader.COOKIE: f"RUUVISESSION={second.cookie}"}, body,
                )
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, denied.status_code)
                self.assertFalse(second.authorized)
                headers: dict[str, str] = {HttpHeader.COOKIE: f"RUUVISESSION={first.cookie}"}
                accepted: FakeResponse = gateway.response_for(first, HttpMethod.POST, GatewayApi.AUTH, headers, body)
                self.assertEqual(HttpStatus.C_200_OK, accepted.status_code)
                self.assertEqual("", first.challenge)
                replay: FakeResponse = gateway.response_for(first, HttpMethod.POST, GatewayApi.AUTH, headers, body)
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, replay.status_code)

    def test_partial_token_updates_preserve_credentials_and_sessions(self) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        session: FakeSession = FakeSession(gateway)
        gateway.response_for(session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
        login: FakeResponse = gateway.response_for(
            session, HttpMethod.POST, GatewayApi.AUTH,
            {HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}"}, login_body(CONFIG, session.challenge),
        )
        self.assertEqual(HttpStatus.C_200_OK, login.status_code)
        gateway.rw_key = "existing-rw"
        cookie_headers: dict[str, str] = {HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}"}
        body: dict[str, str]
        for body in ({GatewayCfgDesc.LAN_AUTH_API_KEY: "new-ro"}, {}):
            response: FakeResponse = gateway.response_for(session, HttpMethod.POST, GatewayApi.CONFIG, cookie_headers, body)
            self.assertEqual(HttpStatus.C_200_OK, response.status_code)
            self.assertEqual(("new-ro", "existing-rw"), (gateway.ro_key, gateway.rw_key))
            self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
            self.assertTrue(session.authorized)
        response = gateway.response_for(
            session, HttpMethod.POST, GatewayApi.CONFIG, cookie_headers,
            {
                GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.RUUVI,
                GatewayCfgDesc.LAN_AUTH_USER: "changed-user",
                GatewayCfgDesc.LAN_AUTH_PASS: "changed-ha1",
            },
        )
        self.assertEqual(HttpStatus.C_200_OK, response.status_code)
        self.assertFalse(session.authorized)
        self.assertEqual(("new-ro", "existing-rw"), (gateway.ro_key, gateway.rw_key))
        self.assertEqual("changed-user", gateway.custom_username)
        self.assertEqual("changed-ha1", gateway.custom_ha1)

    def test_ro_and_invalid_bearer_writes_cannot_change_configuration(self) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        gateway.ro_key = "read-only"
        gateway.rw_key = "read-write"
        session: FakeSession = FakeSession(gateway)
        token: str
        for token in (gateway.ro_key, "incorrect", ""):
            with self.subTest(token=token):
                response: FakeResponse = gateway.response_for(
                    session, HttpMethod.POST, GatewayApi.CONFIG,
                    {HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"},
                    {GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "replaced"},
                )
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, response.status_code)
                self.assertEqual("read-write", gateway.rw_key)
                self.assertEqual([], gateway.config_bodies)

    def test_non_object_config_bodies_are_rejected_without_mutation(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            gateway.ro_key = "read-only"
            gateway.rw_key = "read-write"
            session: FakeSession = FakeSession(gateway)
            gateway.response_for(session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
            login: FakeResponse = gateway.response_for(
                session, HttpMethod.POST, GatewayApi.AUTH,
                {HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}"}, login_body(CONFIG, session.challenge),
            )
            self.assertEqual(HttpStatus.C_200_OK, login.status_code)
            original_auth: tuple[str, str, str] = (gateway.mode, gateway.custom_username, gateway.custom_ha1)
            token: str | None
            expected_status: int
            for token, expected_status in (
                (None, HttpStatus.C_503_SERVICE_UNAVAILABLE),
                ("read-write", HttpStatus.C_503_SERVICE_UNAVAILABLE),
                ("read-only", HttpStatus.C_401_UNAUTHORIZED),
                ("invalid", HttpStatus.C_401_UNAUTHORIZED),
            ):
                body: Any
                for body in ([], [{GatewayCfgDesc.LAN_AUTH_API_KEY_RW: "changed"}], None, "text", 7, False):
                    with self.subTest(gateway=gateway_type.__name__, token=token, body=body):
                        headers: dict[str, str] = {
                            HttpHeader.CONTENT_TYPE: "application/json",
                            HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}",
                        }
                        if token is not None:
                            headers[HttpHeader.AUTHORIZATION] = f"{HttpAuthScheme.BEARER} {token}"
                        prepared: requests.PreparedRequest = session.prepare_request(requests.Request(
                            HttpMethod.POST, f"{CONFIG.base_url}{GatewayApi.CONFIG}",
                            headers=headers, data=json.dumps(body),
                        ))
                        response: FakeResponse = session.send(prepared, allow_redirects=False)
                        self.assertEqual(expected_status, response.status_code)
                        self.assertEqual(body, gateway.calls[-1].body)
                        self.assertEqual([], gateway.config_bodies)
                        self.assertEqual([], gateway.restoration_attempts)
                        self.assertEqual(original_auth, (gateway.mode, gateway.custom_username, gateway.custom_ha1))
                        self.assertEqual(("read-only", "read-write"), (gateway.ro_key, gateway.rw_key))
                        self.assertTrue(session.authorized)
                        self.assertEqual([session], gateway.authorized_sessions)

    def test_status_bearer_takes_precedence_over_authorized_session(self) -> None:
        gateway_type: type[FakeGateway]
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            gateway.ro_key = "read-only"
            gateway.rw_key = "read-write"
            session: FakeSession = FakeSession(gateway)
            gateway.response_for(session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
            cookie_headers: dict[str, str] = {HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}"}
            login: FakeResponse = gateway.response_for(
                session, HttpMethod.POST, GatewayApi.AUTH, cookie_headers, login_body(CONFIG, session.challenge),
            )
            self.assertEqual(HttpStatus.C_200_OK, login.status_code)
            token: str | None
            expected_status: int
            for token, expected_status in (
                (None, HttpStatus.C_200_OK),
                ("invalid", HttpStatus.C_401_UNAUTHORIZED),
                ("", HttpStatus.C_401_UNAUTHORIZED),
                ("read-only", HttpStatus.C_200_OK),
                ("read-write", HttpStatus.C_200_OK),
            ):
                with self.subTest(gateway=gateway_type.__name__, token=token):
                    headers: dict[str, str] = dict(cookie_headers)
                    if token is not None:
                        headers[HttpHeader.AUTHORIZATION] = f"{HttpAuthScheme.BEARER} {token}"
                    response: FakeResponse = gateway.response_for(
                        session, HttpMethod.GET, GatewayApi.STATUS, headers, None,
                    )
                    self.assertEqual(expected_status, response.status_code)
                    self.assertTrue(session.authorized)
            gateway.ro_key = ""
            gateway.rw_key = ""
            for token in ("read-only", "read-write"):
                with self.subTest(gateway=gateway_type.__name__, revoked_token=token):
                    response = gateway.response_for(
                        session, HttpMethod.GET, GatewayApi.STATUS,
                        {**cookie_headers, HttpHeader.AUTHORIZATION: f"{HttpAuthScheme.BEARER} {token}"}, None,
                    )
                    self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, response.status_code)

    def test_digest_requires_request_uri_and_advertised_challenge(self) -> None:
        advertised: dict[str, str] = {
            "realm": "Ruuvi Gateway", "nonce": "nonce", "qop": "auth", "opaque": "opaque",
        }
        gateway_type: type[FakeGateway]
        path: str
        field: str | None
        value: str
        for gateway_type in (FakeGateway, DefaultAuthGateway):
            gateway: FakeGateway = gateway_type(CONFIG)
            gateway.mode = GatewayCfgLanAuthType.DIGEST
            gateway.custom_username = "user"
            gateway.custom_ha1 = hashlib.md5(b"user:Ruuvi Gateway:pass").hexdigest()
            session: FakeSession = FakeSession(gateway)
            session.trust_env = False
            self.addCleanup(session.close)
            for path in (GatewayApi.AUTH, GatewayApi.STATUS):
                challenge: requests.Response = session.get(f"{CONFIG.base_url}{path}", allow_redirects=False)
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, challenge.status_code)
                self.assertEqual(advertised, requests.utils.parse_dict_header(
                    challenge.headers[HttpHeader.WWW_AUTHENTICATE][len("Digest "):],
                ))
                for field, value in ((None, ""), ("uri", GatewayApi.STATUS if path == GatewayApi.AUTH else GatewayApi.AUTH),
                                     ("nonce", "other-nonce"), ("qop", "auth-int"), ("realm", "other-realm"),
                                     ("opaque", "other-opaque")):
                    with self.subTest(gateway=gateway_type.__name__, path=path, field=field):
                        parameters: dict[str, str] = {
                            **advertised, "username": "user", "uri": path, "nc": "00000001", "cnonce": "client",
                        }
                        if field is not None:
                            parameters[field] = value
                        # Hash each altered header correctly; a stale response hash must not explain rejection.
                        ha2: str = hashlib.md5(f"GET:{parameters['uri']}".encode()).hexdigest()
                        parameters["response"] = hashlib.md5(
                            f"{gateway.custom_ha1}:{parameters['nonce']}:{parameters['nc']}:"
                            f"{parameters['cnonce']}:{parameters['qop']}:{ha2}".encode()
                        ).hexdigest()
                        header: str = "Digest " + ", ".join(f'{name}="{item}"' for name, item in parameters.items())
                        response: requests.Response = session.get(
                            f"{CONFIG.base_url}{path}", headers={HttpHeader.AUTHORIZATION: header}, allow_redirects=False,
                        )
                        self.assertEqual(HttpStatus.C_200_OK if field is None else HttpStatus.C_401_UNAUTHORIZED,
                                         response.status_code)
                        if field is None and path == GatewayApi.STATUS:
                            self.assertEqual({"status": "ok"}, response.json())
                        else:
                            self.assertEqual(field is None, response.json()["authenticated"])
                        if field is not None:
                            self.assertEqual(challenge.headers[HttpHeader.WWW_AUTHENTICATE],
                                             response.headers[HttpHeader.WWW_AUTHENTICATE])

    def test_digest_auth_rejects_incomplete_parameters_without_transport_errors(self) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        gateway.mode = GatewayCfgLanAuthType.DIGEST
        gateway.custom_username = "user"
        gateway.custom_ha1 = hashlib.md5(b"user:Ruuvi Gateway:pass").hexdigest()
        ha2: str = hashlib.md5(b"GET:/status.json").hexdigest()
        digest: str = hashlib.md5(f"{gateway.custom_ha1}:nonce:00000001:client:auth:{ha2}".encode()).hexdigest()
        parameters: dict[str, str] = {
            "username": "user", "realm": "Ruuvi Gateway", "uri": "/status.json", "nonce": "nonce",
            "nc": "00000001", "cnonce": "client", "qop": "auth", "opaque": "opaque", "response": digest,
        }
        fields: dict[str, str] = {name: f'{name}="{value}"' for name, value in parameters.items()}
        valid_header: str = "Digest " + ", ".join(fields.values())
        malformed_headers: list[str] = ["Digest ", 'Digest username="user"']
        name: str
        for name in ("uri", "nonce", "nc", "cnonce", "qop", "username", "response", "realm", "opaque"):
            remaining: list[str] = [field for key, field in fields.items() if key != name]
            malformed_headers.extend((
                "Digest " + ", ".join(remaining),
                "Digest " + ", ".join(remaining + [name]),
                "Digest " + ", ".join(remaining + [f'{name}=""']),
            ))
        session: FakeSession = FakeSession(gateway)
        session.trust_env = False
        self.addCleanup(session.close)
        gateway.digest_authenticated_timeouts = 1
        authorization: str
        for authorization in malformed_headers:
            with self.subTest(authorization=authorization):
                response: requests.Response = session.get(
                    f"{CONFIG.base_url}{GatewayApi.STATUS}",
                    headers={HttpHeader.AUTHORIZATION: authorization}, allow_redirects=False,
                )
                self.assertEqual(HttpStatus.C_401_UNAUTHORIZED, response.status_code)
                self.assertEqual({"authenticated": False}, response.json())
                self.assertEqual(authorization, gateway.calls[-1].authorization)
                self.assertFalse(session.authorized)
                self.assertEqual(1, gateway.digest_authenticated_timeouts)
        with self.assertRaisesRegex(requests.Timeout, "Digest authenticated request timeout"):
            session.get(
                f"{CONFIG.base_url}{GatewayApi.STATUS}",
                headers={HttpHeader.AUTHORIZATION: valid_header}, allow_redirects=False,
            )
        self.assertEqual(0, gateway.digest_authenticated_timeouts)
        expected_status: int
        header_name: str
        for authorization, expected_status in (
            (valid_header, HttpStatus.C_200_OK),
            (valid_header.replace(digest, "wrong"), HttpStatus.C_401_UNAUTHORIZED),
        ):
            for header_name in ("Authorization", "authorization", "aUtHoRiZaTiOn"):
                with self.subTest(authorization=authorization, header=header_name):
                    response = session.get(
                        f"{CONFIG.base_url}{GatewayApi.STATUS}",
                        headers={header_name: authorization}, allow_redirects=False,
                    )
                    self.assertEqual(expected_status, response.status_code)
                    self.assertEqual({"status": "ok"} if expected_status == HttpStatus.C_200_OK
                                     else {"authenticated": False}, response.json())
                    self.assertEqual(authorization, gateway.calls[-1].authorization)

    def test_basic_auth_accepts_client_header_for_stored_encoded_credentials(self) -> None:
        gateway: FakeGateway = FakeGateway(CONFIG)
        session: FakeSession = FakeSession(gateway)
        gateway.response_for(session, HttpMethod.GET, GatewayApi.AUTH, {}, None)
        cookie_headers: dict[str, str] = {HttpHeader.COOKIE: f"RUUVISESSION={session.cookie}"}
        login: FakeResponse = gateway.response_for(
            session, HttpMethod.POST, GatewayApi.AUTH, cookie_headers, login_body(CONFIG, session.challenge),
        )
        self.assertEqual(HttpStatus.C_200_OK, login.status_code)
        # Independently authored Base64("user:pass"), as stored by the firmware in Basic mode.
        stored_credentials: str = "dXNlcjpwYXNz"
        configured: FakeResponse = gateway.response_for(
            session, HttpMethod.POST, GatewayApi.CONFIG, cookie_headers,
            {
                GatewayCfgDesc.LAN_AUTH_TYPE: GatewayCfgLanAuthType.BASIC,
                GatewayCfgDesc.LAN_AUTH_USER: "user",
                GatewayCfgDesc.LAN_AUTH_PASS: stored_credentials,
            },
        )
        self.assertEqual(HttpStatus.C_200_OK, configured.status_code)
        authorization: str
        expected_status: int
        header_name: str
        for authorization, expected_status in (
            (GatewayClient.authorization_header_basic("user", "pass"), HttpStatus.C_200_OK),
            (GatewayClient.authorization_header_basic("user", "wrong"), HttpStatus.C_401_UNAUTHORIZED),
            (GatewayClient.authorization_header_basic("wrong", "pass"), HttpStatus.C_401_UNAUTHORIZED),
            (GatewayClient.authorization_header_basic("user", stored_credentials), HttpStatus.C_401_UNAUTHORIZED),
            ("Basic pass", HttpStatus.C_401_UNAUTHORIZED),
            ("Basic !!!", HttpStatus.C_401_UNAUTHORIZED),
            ("", HttpStatus.C_401_UNAUTHORIZED),
        ):
            for header_name in ("Authorization", "authorization", "aUtHoRiZaTiOn"):
                with self.subTest(authorization=authorization, header=header_name):
                    response: FakeResponse = gateway.response_for(
                        FakeSession(gateway), HttpMethod.GET, GatewayApi.STATUS,
                        {header_name: authorization} if authorization else {}, None,
                    )
                    self.assertEqual(expected_status, response.status_code)
                    self.assertEqual(authorization, gateway.calls[-1].authorization)


if __name__ == "__main__":
    unittest.main()
