"""Reusable HTTP and interactive-authentication client for Ruuvi Gateway tests."""

from __future__ import annotations

import base64
import hashlib
import re
import secrets
from dataclasses import dataclass
from typing import Any, Callable, cast

import requests
from Crypto.PublicKey import ECC

from .errors import GatewayAuthenticationModeError, GatewayConnectionError, GatewayProtocolError
from .evidence import EvidenceLog
from .http_api import GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod
from .models import DutConfig

AUTH_PARAMETERS_RE: re.Pattern[str] = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)="([^"]*)"')


class AuthMech:
    HOTSPOT_PROVISIONING: str = "AuthMech-Hotspot-Provisioning"
    LAN_WEBUI_DEFAULT: str = "AuthMech-LAN-WebUI-Default"
    LAN_WEBUI_USER_DEFINED: str = "AuthMech-LAN-WebUI-User-Defined"
    LAN_WEBUI_BASIC: str = "AuthMech-LAN-WebUI-Basic"
    LAN_WEBUI_DIGEST: str = "AuthMech-LAN-WebUI-Digest"
    LAN_WEBUI_UNAUTHENTICATED: str = "AuthMech-LAN-WebUI-Unauthenticated"
    LAN_WEBUI_DISABLED: str = "AuthMech-LAN-WebUI-Disabled"
    M2M_API_BEARER_RO: str = "AuthMech-M2M-API-Bearer-RO"
    M2M_API_BEARER_RW: str = "AuthMech-M2M-API-Bearer-RW"


class GatewayCfgDesc:
    GW_ID: str = "gw_id"
    GW_HOSTNAME: str = "gw_hostname"
    LAN_AUTH_TYPE: str = "lan_auth_type"
    LAN_AUTH_USER: str = "lan_auth_user"
    LAN_AUTH_PASS: str = "lan_auth_pass"
    LAN_AUTH_API_KEY: str = "lan_auth_api_key"
    LAN_AUTH_API_KEY_RW: str = "lan_auth_api_key_rw"
    LAN_AUTH_API_KEY_USE: str = "lan_auth_api_key_use"
    LAN_AUTH_API_KEY_RW_USE: str = "lan_auth_api_key_rw_use"
    GW_MAC: str = "gw_mac"
    FW_VER: str = "fw_ver"
    NRF52_FW_VER: str = "nrf52_fw_ver"


class GatewayCfgLanAuthType:
    ALLOW: str = "lan_auth_allow"
    BASIC: str = "lan_auth_basic"
    DIGEST: str = "lan_auth_digest"
    RUUVI: str = "lan_auth_ruuvi"
    DENY: str = "lan_auth_deny"
    DEFAULT: str = "lan_auth_default"
    BEARER: str = "lan_auth_bearer"


@dataclass(frozen=True)
class AuthCalculationEvidence:
    username: str
    password: str
    ha1_input: str
    ha1: str
    response: str


@dataclass
class InteractiveChallengeRequest:
    session: Any
    private_key: ECC.EccKey
    public_key_b64: str


@dataclass
class InteractiveLoginChallenge:
    session: Any
    challenge: dict[str, str]
    auth_header: str
    cookie: str


@dataclass
class InteractiveAuthChallenge(InteractiveLoginChallenge):
    challenge_response: Any
    auth_payload: dict[str, Any]
    gateway_public_key_raw: bytes
    aes_key: bytes


@dataclass
class InteractiveLoginRequest:
    login_challenge: InteractiveLoginChallenge
    username: str
    password_response: str


@dataclass
class InteractiveAuthResult:
    session: Any
    challenge_response: Any
    auth_payload: dict[str, Any]
    challenge: dict[str, str]
    auth_header: str
    cookie: str
    gateway_public_key_raw: bytes
    aes_key: bytes
    login_response: Any


class GatewayClient:
    def __init__(
        self,
        config: DutConfig,
        evidence: EvidenceLog,
        session_factory: Callable[[], Any] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        timeout: tuple[int, int] = (5, 15),
        user_agent: str = "ruuvi-cra-functional-test",
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.session_factory: Callable[[], Any] = session_factory
        self.random_bytes: Callable[[int], bytes] = random_bytes
        self.ecc_generate: Callable[..., ECC.EccKey] = ecc_generate
        self.timeout: tuple[int, int] = timeout
        self.user_agent: str = user_agent

    def new_session(self) -> Any:
        session: requests.Session = self.session_factory()
        # Keep host .netrc credentials from changing the authentication under test.
        session.trust_env = False
        return session

    def request(
        self,
        session: Any,
        method: str,
        path: str,
        headers: dict[str, str] | None = None,
        json_body: Any = None,
        data: Any = None,
        params: dict[str, Any] | None = None,
        allow_redirects: bool = False,
    ) -> Any:
        if json_body is not None and data is not None:
            raise ValueError("json_body and data are mutually exclusive")
        request_headers: dict[str, str] = {HttpHeader.USER_AGENT: self.user_agent}
        if headers:
            request_headers.update(headers)
        url: str = f"{self.config.base_url}{path}"
        try:
            request: requests.Request = requests.Request(
                method=method,
                url=url,
                headers=request_headers,
                params=params,
                json=json_body,
                data=data,
            )
            prepared_request: requests.PreparedRequest = session.prepare_request(request)
            self.evidence.write_http_request(prepared_request)
            response: requests.Response = session.send(
                prepared_request,
                timeout=self.timeout,
                allow_redirects=allow_redirects,
            )
        except requests.RequestException as error:
            error: Exception
            raise GatewayConnectionError(f"{method} {url} failed: {error}") from error
        self.evidence.write_http_response(response)
        return response

    @staticmethod
    def response_json(
        response: Any,
        context: str,
        expected_type: type[Any] | None = None,
    ) -> Any:
        try:
            payload: Any = response.json()
        except (TypeError, ValueError) as error:
            error: Exception
            raise GatewayProtocolError(f"{context} returned malformed JSON") from error
        if expected_type is not None and not isinstance(payload, expected_type):
            raise GatewayProtocolError(f"{context} JSON must be {expected_type.__name__}")
        return payload

    @staticmethod
    def parse_interactive_challenge(header: str | None) -> dict[str, str]:
        prefix: str = "x-ruuvi-interactive"
        if (
            header is None
            or not header.lower().startswith(prefix)
            or header[len(prefix) : len(prefix) + 1] not in (" ", "\t")
        ):
            raise GatewayProtocolError("GET /auth did not advertise x-ruuvi-interactive")
        parameters: dict[str, str] = dict(AUTH_PARAMETERS_RE.findall(header[len(prefix) :].strip()))
        required: set[str] = {"realm", "challenge", "session_cookie", "session_id"}
        missing: set[str] = required.difference(parameters)
        if missing:
            raise GatewayProtocolError(f"interactive challenge is missing: {', '.join(sorted(missing))}")
        return parameters

    @staticmethod
    def parse_digest_challenge(header: str | None) -> dict[str, str]:
        prefix: str = HttpAuthScheme.DIGEST.lower()
        if (
            header is None
            or not header.lower().startswith(prefix)
            or header[len(prefix) : len(prefix) + 1] not in (" ", "\t")
        ):
            raise GatewayProtocolError("response did not advertise Digest authentication")
        parameters: dict[str, str] = dict(AUTH_PARAMETERS_RE.findall(header[len(prefix) :].strip()))
        required: set[str] = {"realm", "qop", "nonce", "opaque"}
        missing: set[str] = required.difference(parameters)
        if missing:
            raise GatewayProtocolError(f"Digest challenge is missing: {', '.join(sorted(missing))}")
        return parameters

    def random_text(self, size: int = 18) -> str:
        return base64.urlsafe_b64encode(self.random_bytes(size)).decode("ascii").rstrip("=")

    @staticmethod
    def authorization_header_basic(username: str, password: str) -> str:
        encoded: str = base64.b64encode(f"{username}:{password}".encode()).decode("ascii")
        return f"{HttpAuthScheme.BASIC} {encoded}"

    @staticmethod
    def calculate_digest_ha1(username: str, realm: str, password: str) -> str:
        return hashlib.md5(f"{username}:{realm}:{password}".encode()).hexdigest()

    def authorization_header_digest(
        self,
        username: str,
        password: str,
        method: str,
        path: str,
        challenge: dict[str, str],
    ) -> str:
        nc: str = "00000001"
        cnonce: str = self.random_text(12)
        ha1: str = self.calculate_digest_ha1(username, challenge["realm"], password)
        ha2: str = hashlib.md5(f"{method}:{path}".encode()).hexdigest()
        response: str = hashlib.md5(
            f"{ha1}:{challenge['nonce']}:{nc}:{cnonce}:{challenge['qop']}:{ha2}".encode()
        ).hexdigest()
        return (
            f'{HttpAuthScheme.DIGEST} username="{username}", '
            f'realm="{challenge["realm"]}", nonce="{challenge["nonce"]}", '
            f'uri="{path}", qop={challenge["qop"]}, nc={nc}, cnonce="{cnonce}", '
            f'response="{response}", opaque="{challenge["opaque"]}"'
        )

    def interactive_login_challenge_from_response(
        self,
        session: Any,
        response: Any,
        context: str,
    ) -> InteractiveLoginChallenge:
        auth_header: str | None = response.headers.get(HttpHeader.WWW_AUTHENTICATE)
        challenge: dict[str, str] = self.parse_interactive_challenge(auth_header)
        self.evidence.write("INTERACTIVE CHALLENGE", challenge)

        cookie: str | None = response.cookies.get("RUUVISESSION")
        if not cookie:
            raise GatewayProtocolError(f"{context} did not supply RUUVISESSION cookie")
        if challenge["session_cookie"] != "RUUVISESSION":
            raise GatewayProtocolError("interactive challenge names an unexpected session cookie")
        if challenge["session_id"] != cookie:
            raise GatewayProtocolError("interactive challenge session_id does not match cookie")
        return InteractiveLoginChallenge(
            session=session,
            challenge=challenge,
            auth_header=cast(str, auth_header),  # Validated by parse_interactive_challenge above.
            cookie=cookie,
        )

    def prepare_interactive_challenge_request(self) -> InteractiveChallengeRequest:
        session: requests.Session = self.new_session()
        private_key: ECC.EccKey = self.ecc_generate(curve="secp256r1")
        public_key: ECC.EccKey = private_key.public_key()
        public_key_raw: bytes = (
            b"\x04"
            + int(public_key.pointQ.x).to_bytes(32, byteorder="big")
            + int(public_key.pointQ.y).to_bytes(32, byteorder="big")
        )
        public_key_b64: str = base64.b64encode(public_key_raw).decode("ascii")
        self.evidence.write("ECDH CLIENT PUBLIC KEY", public_key_b64)
        return InteractiveChallengeRequest(
            session=session,
            private_key=private_key,
            public_key_b64=public_key_b64,
        )

    def send_interactive_challenge_request(
        self,
        request: InteractiveChallengeRequest,
    ) -> Any:
        return self.request(
            request.session,
            HttpMethod.GET,
            GatewayApi.AUTH,
            headers={HttpHeader.RUUVI_ECDH_PUBLIC_KEY: request.public_key_b64},
        )

    def parse_interactive_challenge_response(
        self,
        request: InteractiveChallengeRequest,
        challenge_response: Any,
    ) -> InteractiveAuthChallenge:
        auth_payload: dict[str, Any] = self.response_json(challenge_response, "GET /auth", dict)
        auth_header: str | None = challenge_response.headers.get(HttpHeader.WWW_AUTHENTICATE)
        auth_type: Any = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if (
            isinstance(auth_type, str)
            and auth_type != GatewayCfgLanAuthType.DEFAULT
            and (auth_header is None or not auth_header.lower().startswith("x-ruuvi-interactive"))
        ):
            raise GatewayAuthenticationModeError(auth_type)
        login_challenge: InteractiveLoginChallenge = self.interactive_login_challenge_from_response(
            request.session,
            challenge_response,
            "GET /auth",
        )

        gateway_public_b64: str | None = challenge_response.headers.get(HttpHeader.RUUVI_ECDH_PUBLIC_KEY)
        if not gateway_public_b64:
            raise GatewayProtocolError("GET /auth did not supply gateway ECDH public key")
        try:
            gateway_public_raw: bytes = base64.b64decode(gateway_public_b64, validate=True)
            if len(gateway_public_raw) != 65 or gateway_public_raw[0] != 0x04:
                raise ValueError("unexpected uncompressed P-256 key encoding")
            x: int = int.from_bytes(gateway_public_raw[1:33], "big")
            y: int = int.from_bytes(gateway_public_raw[33:65], "big")
            gateway_public: ECC.EccKey = ECC.construct(
                curve="secp256r1",
                point_x=x,
                point_y=y,
            )
            if gateway_public.pointQ.is_point_at_infinity():
                raise ValueError("gateway ECDH public key is the point at infinity")
            shared_point: ECC.EccPoint = gateway_public.pointQ * int(request.private_key.d)
            shared_secret: bytes = int(shared_point.x).to_bytes(
                32,
                "big",
            )
        except (ValueError, TypeError) as error:
            error: Exception
            raise GatewayProtocolError("invalid gateway ECDH public key") from error
        self.evidence.write("ECDH GATEWAY PUBLIC KEY", gateway_public_b64)
        self.evidence.write("ECDH SHARED SECRET", shared_secret.hex())
        aes_key: bytes = hashlib.sha256(shared_secret).digest()
        self.evidence.write("ECDH AES KEY", aes_key.hex())
        return InteractiveAuthChallenge(
            session=request.session,
            challenge=login_challenge.challenge,
            auth_header=login_challenge.auth_header,
            cookie=login_challenge.cookie,
            challenge_response=challenge_response,
            auth_payload=auth_payload,
            gateway_public_key_raw=gateway_public_raw,
            aes_key=aes_key,
        )

    def request_interactive_challenge(self) -> InteractiveAuthChallenge:
        request: InteractiveChallengeRequest = self.prepare_interactive_challenge_request()
        response: requests.Response = self.send_interactive_challenge_request(request)
        return self.parse_interactive_challenge_response(request, response)

    def prepare_interactive_login_request(
        self,
        login_challenge: InteractiveLoginChallenge,
        username: str,
        password: str,
    ) -> InteractiveLoginRequest:
        challenge: dict[str, str] = login_challenge.challenge
        ha1_input: str = f"{username}:{challenge['realm']}:{password}"
        ha1: str = self.calculate_digest_ha1(username, challenge["realm"], password)
        password_response: str = hashlib.sha256(f"{challenge['challenge']}:{ha1}".encode()).hexdigest()
        self.evidence.write(
            "AUTH CALCULATION",
            AuthCalculationEvidence(
                username=username,
                password=password,
                ha1_input=ha1_input,
                ha1=ha1,
                response=password_response,
            ),
        )
        return InteractiveLoginRequest(
            login_challenge=login_challenge,
            username=username,
            password_response=password_response,
        )

    def send_interactive_login_request(self, request: InteractiveLoginRequest) -> Any:
        return self.request(
            request.login_challenge.session,
            HttpMethod.POST,
            GatewayApi.AUTH,
            headers={HttpHeader.COOKIE: f"RUUVISESSION={request.login_challenge.cookie}"},
            json_body={
                "login": request.username,
                "password": request.password_response,
            },
        )

    def submit_interactive_authentication(
        self,
        login_challenge: InteractiveLoginChallenge,
        username: str,
        password: str,
    ) -> Any:
        request: InteractiveLoginRequest = self.prepare_interactive_login_request(
            login_challenge,
            username,
            password,
        )
        return self.send_interactive_login_request(request)

    def authenticate_interactive(self, username: str, password: str) -> InteractiveAuthResult:
        challenge: InteractiveAuthChallenge = self.request_interactive_challenge()
        login_response: requests.Response = self.submit_interactive_authentication(
            challenge,
            username,
            password,
        )
        return InteractiveAuthResult(
            session=challenge.session,
            challenge_response=challenge.challenge_response,
            auth_payload=challenge.auth_payload,
            challenge=challenge.challenge,
            auth_header=challenge.auth_header,
            cookie=challenge.cookie,
            gateway_public_key_raw=challenge.gateway_public_key_raw,
            aes_key=challenge.aes_key,
            login_response=login_response,
        )
