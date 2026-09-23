"""Regression checks for shared offline fixtures, not live-DUT compliance evidence."""

from __future__ import annotations

import hashlib
import unittest
from typing import Any

from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType
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
            second_session, HttpMethod.GET, GatewayApi.CONFIG, {}, None,
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
        body: dict[str, str]
        for body in ({GatewayCfgDesc.LAN_AUTH_API_KEY: "new-ro"}, {}):
            response: FakeResponse = gateway.response_for(session, HttpMethod.POST, GatewayApi.CONFIG, {}, body)
            self.assertEqual(HttpStatus.C_200_OK, response.status_code)
            self.assertEqual(("new-ro", "existing-rw"), (gateway.ro_key, gateway.rw_key))
            self.assertEqual(GatewayCfgLanAuthType.DEFAULT, gateway.mode)
            self.assertTrue(session.authorized)
        response = gateway.response_for(
            session, HttpMethod.POST, GatewayApi.CONFIG, {},
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


if __name__ == "__main__":
    unittest.main()
