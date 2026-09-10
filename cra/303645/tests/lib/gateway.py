"""Reusable HTTP and interactive-authentication client for Ruuvi Gateway tests."""

import base64
import hashlib
import re
import secrets
from dataclasses import dataclass
from typing import Any, Callable, Dict, Optional, Tuple, Type

import requests
from Crypto.PublicKey import ECC

from .errors import GatewayAuthenticationModeError, GatewayConnectionError, GatewayProtocolError
from .evidence import EvidenceLog
from .http_api import GatewayApi, HttpAuthScheme, HttpHeader, HttpMethod
from .models import DutConfig


AUTH_PARAMETERS_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)="([^"]*)"')


class AuthMech:
    HOTSPOT_PROVISIONING = "AuthMech-Hotspot-Provisioning"
    LAN_WEBUI_DEFAULT = "AuthMech-LAN-WebUI-Default"
    LAN_WEBUI_USER_DEFINED = "AuthMech-LAN-WebUI-User-Defined"
    LAN_WEBUI_BASIC = "AuthMech-LAN-WebUI-Basic"
    LAN_WEBUI_DIGEST = "AuthMech-LAN-WebUI-Digest"
    LAN_WEBUI_UNAUTHENTICATED = "AuthMech-LAN-WebUI-Unauthenticated"
    LAN_WEBUI_DISABLED = "AuthMech-LAN-WebUI-Disabled"
    M2M_API_BEARER_RO = "AuthMech-M2M-API-Bearer-RO"
    M2M_API_BEARER_RW = "AuthMech-M2M-API-Bearer-RW"


class GatewayCfgDesc:
    GW_ID = "gw_id"
    GW_HOSTNAME = "gw_hostname"
    LAN_AUTH_TYPE = "lan_auth_type"
    LAN_AUTH_USER = "lan_auth_user"
    LAN_AUTH_PASS = "lan_auth_pass"
    LAN_AUTH_API_KEY = "lan_auth_api_key"
    LAN_AUTH_API_KEY_RW = "lan_auth_api_key_rw"
    LAN_AUTH_API_KEY_USE = "lan_auth_api_key_use"
    LAN_AUTH_API_KEY_RW_USE = "lan_auth_api_key_rw_use"
    GW_MAC = "gw_mac"
    FW_VER = "fw_ver"
    NRF52_FW_VER = "nrf52_fw_ver"


class GatewayCfgLanAuthType:
    ALLOW = "lan_auth_allow"
    BASIC = "lan_auth_basic"
    DIGEST = "lan_auth_digest"
    RUUVI = "lan_auth_ruuvi"
    DENY = "lan_auth_deny"
    DEFAULT = "lan_auth_default"
    BEARER = "lan_auth_bearer"


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
    private_key: Any
    public_key_b64: str


@dataclass
class InteractiveLoginChallenge:
    session: Any
    challenge: Dict[str, str]
    auth_header: str
    cookie: str


@dataclass
class InteractiveAuthChallenge(InteractiveLoginChallenge):
    challenge_response: Any
    auth_payload: Dict[str, Any]
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
    auth_payload: Dict[str, Any]
    challenge: Dict[str, str]
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
        ecc_generate: Callable[..., Any] = ECC.generate,
        timeout: Tuple[int, int] = (5, 15),
        user_agent: str = "ruuvi-cra-functional-test",
    ) -> None:
        self.config = config
        self.evidence = evidence
        self.session_factory = session_factory
        self.random_bytes = random_bytes
        self.ecc_generate = ecc_generate
        self.timeout = timeout
        self.user_agent = user_agent

    def new_session(self) -> Any:
        return self.session_factory()

    def request(
        self,
        session: Any,
        method: str,
        path: str,
        headers: Optional[Dict[str, str]] = None,
        json_body: Any = None,
        data: Any = None,
        params: Optional[Dict[str, Any]] = None,
        allow_redirects: bool = False,
    ) -> Any:
        if json_body is not None and data is not None:
            raise ValueError("json_body and data are mutually exclusive")
        request_headers = {HttpHeader.USER_AGENT: self.user_agent}
        if headers:
            request_headers.update(headers)
        url = f"{self.config.base_url}{path}"
        try:
            request = requests.Request(
                method=method,
                url=url,
                headers=request_headers,
                params=params,
                json=json_body,
                data=data,
            )
            prepared_request = session.prepare_request(request)
            self.evidence.write_http_request(prepared_request)
            response = session.send(
                prepared_request,
                timeout=self.timeout,
                allow_redirects=allow_redirects,
            )
        except requests.RequestException as error:
            raise GatewayConnectionError(f"{method} {url} failed: {error}") from error
        self.evidence.write_http_response(response)
        return response

    @staticmethod
    def response_json(
        response: Any,
        context: str,
        expected_type: Optional[Type[Any]] = None,
    ) -> Any:
        try:
            payload = response.json()
        except (TypeError, ValueError) as error:
            raise GatewayProtocolError(f"{context} returned malformed JSON") from error
        if expected_type is not None and not isinstance(payload, expected_type):
            raise GatewayProtocolError(f"{context} JSON must be {expected_type.__name__}")
        return payload

    @staticmethod
    def parse_interactive_challenge(header: Optional[str]) -> Dict[str, str]:
        prefix = "x-ruuvi-interactive"
        if header is None or not header.lower().startswith(prefix):
            raise GatewayProtocolError("GET /auth did not advertise x-ruuvi-interactive")
        parameters = dict(AUTH_PARAMETERS_RE.findall(header[len(prefix):].strip()))
        required = {"realm", "challenge", "session_cookie", "session_id"}
        missing = required.difference(parameters)
        if missing:
            raise GatewayProtocolError(
                f"interactive challenge is missing: {', '.join(sorted(missing))}"
            )
        return parameters

    @staticmethod
    def parse_digest_challenge(header: Optional[str]) -> Dict[str, str]:
        prefix = HttpAuthScheme.DIGEST.lower()
        if header is None or not header.lower().startswith(prefix):
            raise GatewayProtocolError("response did not advertise Digest authentication")
        parameters = dict(AUTH_PARAMETERS_RE.findall(header[len(prefix):].strip()))
        required = {"realm", "qop", "nonce", "opaque"}
        missing = required.difference(parameters)
        if missing:
            raise GatewayProtocolError(
                f"Digest challenge is missing: {', '.join(sorted(missing))}"
            )
        return parameters

    def random_text(self, size: int = 18) -> str:
        return base64.urlsafe_b64encode(self.random_bytes(size)).decode("ascii").rstrip("=")

    @staticmethod
    def authorization_header_basic(username: str, password: str) -> str:
        encoded = base64.b64encode(f"{username}:{password}".encode("utf-8")).decode(
            "ascii"
        )
        return f"{HttpAuthScheme.BASIC} {encoded}"

    @staticmethod
    def calculate_digest_ha1(username: str, realm: str, password: str) -> str:
        return hashlib.md5(f"{username}:{realm}:{password}".encode("utf-8")).hexdigest()

    def authorization_header_digest(
        self,
        username: str,
        password: str,
        method: str,
        path: str,
        challenge: Dict[str, str],
    ) -> str:
        nc = "00000001"
        cnonce = self.random_text(12)
        ha1 = self.calculate_digest_ha1(username, challenge["realm"], password)
        ha2 = hashlib.md5(f"{method}:{path}".encode("utf-8")).hexdigest()
        response = hashlib.md5(
            f'{ha1}:{challenge["nonce"]}:{nc}:{cnonce}:{challenge["qop"]}:{ha2}'.encode(
                "utf-8"
            )
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
        auth_header = response.headers.get(HttpHeader.WWW_AUTHENTICATE)
        challenge = self.parse_interactive_challenge(auth_header)
        self.evidence.write("INTERACTIVE CHALLENGE", challenge)

        cookie = response.cookies.get("RUUVISESSION")
        if not cookie:
            raise GatewayProtocolError(f"{context} did not supply RUUVISESSION cookie")
        if challenge["session_cookie"] != "RUUVISESSION":
            raise GatewayProtocolError("interactive challenge names an unexpected session cookie")
        if challenge["session_id"] != cookie:
            raise GatewayProtocolError("interactive challenge session_id does not match cookie")
        return InteractiveLoginChallenge(
            session=session,
            challenge=challenge,
            auth_header=auth_header,
            cookie=cookie,
        )

    def prepare_interactive_challenge_request(self) -> InteractiveChallengeRequest:
        session = self.new_session()
        private_key = self.ecc_generate(curve="secp256r1")
        public_key = private_key.public_key()
        public_key_raw = (
            b"\x04"
            + int(public_key.pointQ.x).to_bytes(32, byteorder="big")
            + int(public_key.pointQ.y).to_bytes(32, byteorder="big")
        )
        public_key_b64 = base64.b64encode(public_key_raw).decode("ascii")
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
        auth_payload = self.response_json(challenge_response, "GET /auth", dict)
        auth_header = challenge_response.headers.get(HttpHeader.WWW_AUTHENTICATE)
        auth_type = auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE)
        if (
            isinstance(auth_type, str)
            and auth_type != GatewayCfgLanAuthType.DEFAULT
            and (auth_header is None or not auth_header.lower().startswith("x-ruuvi-interactive"))
        ):
            raise GatewayAuthenticationModeError(auth_type)
        login_challenge = self.interactive_login_challenge_from_response(
            request.session,
            challenge_response,
            "GET /auth",
        )

        gateway_public_b64 = challenge_response.headers.get(
            HttpHeader.RUUVI_ECDH_PUBLIC_KEY
        )
        if not gateway_public_b64:
            raise GatewayProtocolError("GET /auth did not supply gateway ECDH public key")
        try:
            gateway_public_raw = base64.b64decode(gateway_public_b64, validate=True)
            if len(gateway_public_raw) != 65 or gateway_public_raw[0] != 0x04:
                raise ValueError("unexpected uncompressed P-256 key encoding")
            x = int.from_bytes(gateway_public_raw[1:33], "big")
            y = int.from_bytes(gateway_public_raw[33:65], "big")
            gateway_public = ECC.construct(
                curve="secp256r1",
                point_x=x,
                point_y=y,
            )
            shared_point = gateway_public.pointQ * int(request.private_key.d)
            shared_secret = int(shared_point.x).to_bytes(
                32,
                "big",
            )
        except (ValueError, TypeError) as error:
            raise GatewayProtocolError("invalid gateway ECDH public key") from error
        self.evidence.write("ECDH GATEWAY PUBLIC KEY", gateway_public_b64)
        self.evidence.write("ECDH SHARED SECRET", shared_secret.hex())
        aes_key = hashlib.sha256(shared_secret).digest()
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
        request = self.prepare_interactive_challenge_request()
        response = self.send_interactive_challenge_request(request)
        return self.parse_interactive_challenge_response(request, response)

    def prepare_interactive_login_request(
        self,
        login_challenge: InteractiveLoginChallenge,
        username: str,
        password: str,
    ) -> InteractiveLoginRequest:
        challenge = login_challenge.challenge
        ha1_input = f'{username}:{challenge["realm"]}:{password}'
        ha1 = self.calculate_digest_ha1(username, challenge["realm"], password)
        password_response = hashlib.sha256(
            f'{challenge["challenge"]}:{ha1}'.encode("utf-8")
        ).hexdigest()
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
            headers={
                HttpHeader.COOKIE: f"RUUVISESSION={request.login_challenge.cookie}"
            },
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
        request = self.prepare_interactive_login_request(
            login_challenge,
            username,
            password,
        )
        return self.send_interactive_login_request(request)

    def authenticate_interactive(self, username: str, password: str) -> InteractiveAuthResult:
        challenge = self.request_interactive_challenge()
        login_response = self.submit_interactive_authentication(
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
