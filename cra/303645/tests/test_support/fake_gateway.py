"""Offline gateway transport fixtures shared by independent implementation tests."""

from __future__ import annotations

import base64
import hashlib
import json
from dataclasses import dataclass
from typing import Any, Mapping
from urllib.parse import urlsplit

import requests
from Crypto.PublicKey import ECC
from requests.structures import CaseInsensitiveDict

from lib.gateway import GatewayApi, GatewayCfgDesc, GatewayCfgLanAuthType
from lib.http_api import HttpAuthScheme, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig


def env_text(config: DutConfig) -> str:
    return (
        f"{GatewayCfgDesc.GW_ID}={config.gw_id}\n"
        f"{GatewayCfgDesc.GW_MAC}={config.gw_mac}\n"
        f"{GatewayCfgDesc.GW_HOSTNAME}={config.gw_hostname}\n"
    )


class FakeResponse(requests.Response):
    def __init__(
        self,
        status: int = HttpStatus.C_200_OK,
        payload: Any = None,
        headers: Mapping[str, str] | None = None,
        cookies: Mapping[str, str] | None = None,
        malformed_json: bool = False,
        *,
        json_error: BaseException | None = None,
    ) -> None:
        super().__init__()
        self.status_code: int = status
        self.headers.update(headers or {})
        self.cookies.update(dict(cookies or {}))
        self._payload: Any = payload
        self._json_error: BaseException | None = json_error
        self._malformed_json: bool = malformed_json
        text: str = "{broken" if malformed_json else json.dumps(payload) if payload is not None else ""
        self._content: bytes = text.encode("utf-8")
        self.encoding: str = "utf-8"

    def json(self, **kwargs: Any) -> Any:
        if self._json_error is not None:
            raise self._json_error
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
    method: str
    path: str
    bearer_token: str | None
    body: Any
    session_number: int
    authorization: str
    cookie: str | None
    allow_redirects: bool = False

    @property
    def scheme(self) -> str:
        return self.authorization.split(" ", 1)[0] if self.authorization else "none"

    @property
    def key(self) -> FakeRequestKey:
        return FakeRequestKey(self.scheme, self.method, self.path)


class FakeGateway:
    def __init__(self, config: DutConfig) -> None:
        self.dut_config: DutConfig = config
        self.next_session_number: int = 1
        self.session_limit: int | None = 4
        self.connection_error: BaseException | None = None
        self.challenge_headers: bool = True
        self.mode: str = GatewayCfgLanAuthType.DEFAULT
        self.custom_username: str = ""
        self.custom_ha1: str = ""
        self.ro_key: str = ""
        self.rw_key: str = ""
        self.calls: list[RecordedRequest] = []
        self.config_bodies: list[dict[str, Any]] = []
        self.wrong_login_success: bool = False
        self.bearer_guess_status: int | None = None
        self.malformed_challenge: bool = False
        self.digest_authenticated_timeouts: int = 0
        self.timeout_next: bool = False
        self.prepared_config_timeouts: int = 0
        self.guess_exception: BaseException | None = None
        self.guess_exceptions: list[BaseException] = []
        self.primary_restore_status: int = HttpStatus.C_200_OK
        self.primary_restore_applies: bool = False
        self.custom_fallback_enabled: bool = True
        self.restored_config_mismatch: bool = False
        self.temporary_bearers_survive: bool = False
        self.restoration_attempts: list[str] = []
        self.authorized_sessions: list[FakeSession] = []
        self.server_key: ECC.EccKey = ECC.generate(curve="secp256r1")
        public: ECC.EccKey = self.server_key.public_key()
        raw: bytes = (
            b"\x04"
            + int(public.pointQ.x).to_bytes(32, byteorder="big")
            + int(public.pointQ.y).to_bytes(32, byteorder="big")
        )
        self.server_public_b64: str = base64.b64encode(raw).decode("ascii")

    @staticmethod
    def _bearer(headers: Mapping[str, str]) -> str | None:
        authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
        bearer_prefix: str = f"{HttpAuthScheme.BEARER} "
        return authorization[len(bearer_prefix) :] if authorization.startswith(bearer_prefix) else None

    def _is_real_bearer(self, token: str | None, path: str) -> bool:
        if not token:
            return False
        if token == self.rw_key:
            return True
        return token == self.ro_key and path != GatewayApi.AP

    @staticmethod
    def _has_session_cookie(session: FakeSession, headers: Mapping[str, str]) -> bool:
        cookie_header: str = headers.get(HttpHeader.COOKIE, "")
        pairs: list[list[str]] = [pair.strip().split("=", 1) for pair in cookie_header.split(";")]
        values: list[str] = [pair[1] for pair in pairs if len(pair) == 2 and pair[0] == "RUUVISESSION"]
        return bool(session.cookie) and values == [session.cookie]

    def _is_session_authorized(self, session: FakeSession, headers: Mapping[str, str]) -> bool:
        return (self.mode in (GatewayCfgLanAuthType.DEFAULT, GatewayCfgLanAuthType.RUUVI)
                and session.authorized and self._has_session_cookie(session, headers))

    def _interactive_challenge_response(
        self,
        session: FakeSession,
        include_ecdh_public_key: bool,
    ) -> FakeResponse:
        session.authorized = False
        self.authorized_sessions = [active for active in self.authorized_sessions if active is not session]
        session.challenge_number += 1
        session.challenge, session.cookie = self._challenge_values(session)
        auth_header: str = (
            f'x-ruuvi-interactive realm="Ruuvi Gateway" challenge="{session.challenge}" '
            f'session_cookie="RUUVISESSION" session_id="{session.cookie}"'
        )
        if self.malformed_challenge:
            auth_header = 'x-ruuvi-interactive realm="Ruuvi Gateway"'
        response_headers: dict[str, str] = {}
        if self.challenge_headers:
            response_headers[HttpHeader.WWW_AUTHENTICATE] = auth_header
        if include_ecdh_public_key:
            response_headers[HttpHeader.RUUVI_ECDH_PUBLIC_KEY] = self.server_public_b64
        return FakeResponse(
            HttpStatus.C_401_UNAUTHORIZED,
            {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
            headers=response_headers,
            cookies={"RUUVISESSION": session.cookie},
        )

    def response_for(
        self,
        session: FakeSession,
        method: str,
        path: str,
        headers: Mapping[str, str],
        body: Any,
        allow_redirects: bool = False,
    ) -> FakeResponse:
        headers = CaseInsensitiveDict(headers)
        token: str | None = self._bearer(headers)
        self.calls.append(
            RecordedRequest(
                method, path, token, body, session.number,
                headers.get(HttpHeader.AUTHORIZATION, ""), headers.get(HttpHeader.COOKIE), allow_redirects,
            )
        )
        if self.connection_error is not None:
            error: BaseException = self.connection_error
            self.connection_error = None
            raise error
        if self.timeout_next:
            self.timeout_next = False
            raise requests.Timeout("timeout")

        override: FakeResponse | None = self._response_override(session, self.calls[-1])
        if override is not None:
            return override
        if path == GatewayApi.AUTH:
            return self._auth_response(session, method, headers, body)

        if self.mode == GatewayCfgLanAuthType.DENY and token is None:
            return FakeResponse(HttpStatus.C_403_FORBIDDEN, {"error": "forbidden"})
        session_authorized: bool = (self.mode == GatewayCfgLanAuthType.ALLOW
                                    or self._is_session_authorized(session, headers))
        if method == HttpMethod.POST and path == GatewayApi.CONFIG:
            authorized: bool = bool(token) and token == self.rw_key if token is not None else session_authorized
            if not authorized:
                if token and token not in {self.ro_key, self.rw_key}:
                    if self.guess_exceptions:
                        raise self.guess_exceptions.pop(0)
                    if self.guess_exception is not None:
                        error: BaseException = self.guess_exception
                        self.guess_exception = None
                        raise error
                    if self.bearer_guess_status is not None:
                        status: int = self.bearer_guess_status
                        self.bearer_guess_status = None
                        return FakeResponse(status, {})
                return FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED,
                    {"error": "unauthorized"},
                )
            if not isinstance(body, dict):
                return FakeResponse(HttpStatus.C_503_SERVICE_UNAVAILABLE)
            self.config_bodies.append(body)
            restoring: bool = body.get(GatewayCfgDesc.LAN_AUTH_TYPE) == GatewayCfgLanAuthType.DEFAULT
            if restoring:
                method_name: str = (
                    "bearer" if token else ("custom" if self.mode == GatewayCfgLanAuthType.RUUVI else "default")
                )
                self.restoration_attempts.append(method_name)
                if token and self.primary_restore_status != HttpStatus.C_200_OK:
                    if self.primary_restore_applies:
                        self._apply_config(body)
                    return FakeResponse(self.primary_restore_status, {})
            self._apply_config(body)
            return FakeResponse(HttpStatus.C_200_OK, {})

        if method == HttpMethod.GET and path == GatewayApi.CONFIG and (
            self._is_real_bearer(token, path) if token is not None else session_authorized
        ):
            return self._config_response()

        if method == HttpMethod.GET and path == GatewayApi.STATUS and token is None and session_authorized:
            return FakeResponse(HttpStatus.C_200_OK, {"status": "ok"})

        if (
            method == HttpMethod.GET
            and path
            in {
                GatewayApi.HISTORY,
                GatewayApi.AP,
                GatewayApi.STATUS,
                GatewayApi.CONFIG,
            }
            and (token is not None or session_authorized)
        ):
            if self.temporary_bearers_survive and self.mode == GatewayCfgLanAuthType.DEFAULT:
                return FakeResponse(HttpStatus.C_200_OK, {})
            if self._is_real_bearer(token, path) if token is not None else session_authorized:
                return self._read_response(path)
            if token in {self.ro_key, self.rw_key}:
                return FakeResponse(
                    HttpStatus.C_401_UNAUTHORIZED,
                    {"error": "unauthorized"},
                )
            if self.guess_exceptions:
                raise self.guess_exceptions.pop(0)
            if self.guess_exception is not None:
                error = self.guess_exception
                self.guess_exception = None
                raise error
            if self.bearer_guess_status is not None:
                status = self.bearer_guess_status
                self.bearer_guess_status = None
                return FakeResponse(status, {})
            return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})

        if (method == HttpMethod.GET and path == GatewayApi.STATUS
                and self.mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST)):
            return self._credentials_response(headers, path)
        return self._unauthorized_response(method, path, headers)

    def _credentials_response(self, headers: Mapping[str, str], path: str) -> FakeResponse:
        """Validate the configured Basic/Digest scheme for status and GET /auth."""
        authorization: str = headers.get(HttpHeader.AUTHORIZATION, "")
        valid: bool = False
        challenge: str
        if self.mode == GatewayCfgLanAuthType.BASIC:
            # In Basic mode, the stored lan_auth_pass is already Base64(username:password).
            valid = authorization == f"{HttpAuthScheme.BASIC} {self.custom_ha1}"
            challenge = 'Basic realm="Ruuvi Gateway"'
        else:
            challenge_parameters: dict[str, str] = {
                "realm": "Ruuvi Gateway", "qop": "auth", "nonce": "nonce", "opaque": "opaque",
            }
            challenge = "Digest " + ", ".join(f'{name}="{value}"' for name, value in challenge_parameters.items())
            if authorization.startswith(f"{HttpAuthScheme.DIGEST} "):
                parameters: dict[str, str | None] = requests.utils.parse_dict_header(
                    authorization[len(f"{HttpAuthScheme.DIGEST} ") :]
                )
                required: tuple[str, ...] = ("uri", "nonce", "nc", "cnonce", "qop", "username", "response")
                if (all(parameters.get(name) for name in required)
                        and parameters["uri"] == path
                        and all(parameters.get(name) == value for name, value in challenge_parameters.items())):
                    if self.digest_authenticated_timeouts > 0:
                        self.digest_authenticated_timeouts -= 1
                        raise requests.Timeout("Digest authenticated request timeout")
                    ha2: str = hashlib.md5(f"{HttpMethod.GET}:{path}".encode()).hexdigest()
                    expected: str = hashlib.md5(
                        f"{self.custom_ha1}:{parameters['nonce']}:{parameters['nc']}:"
                        f"{parameters['cnonce']}:{parameters['qop']}:{ha2}".encode()
                    ).hexdigest()
                    valid = parameters["username"] == self.custom_username and parameters["response"] == expected
        return FakeResponse(
            HttpStatus.C_200_OK if valid else HttpStatus.C_401_UNAUTHORIZED,
            {"authenticated": valid},
            headers={} if valid else {HttpHeader.WWW_AUTHENTICATE: challenge},
        )

    def _response_override(self, session: FakeSession, call: RecordedRequest) -> FakeResponse | None:
        """Inject case-specific responses after recording the request."""
        return None

    def _auth_response(
        self, session: FakeSession, method: str, headers: Mapping[str, str], body: Any,
    ) -> FakeResponse:
        if method == HttpMethod.GET:
            if self.mode == GatewayCfgLanAuthType.DENY:
                return FakeResponse(HttpStatus.C_403_FORBIDDEN,
                                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode, "authenticated": False})
            if self.mode == GatewayCfgLanAuthType.ALLOW:
                cookies: Mapping[str, str] = {}
                if not session.authorized or not self._has_session_cookie(session, headers):
                    self.authorized_sessions = [active for active in self.authorized_sessions if active is not session]
                    session.challenge_number += 1
                    session.challenge = ""
                    session.cookie = self._challenge_values(session)[1]
                    cookies = {"RUUVISESSION": session.cookie}
                    session.authorized = True
                    self.authorized_sessions.append(session)
                    if self.session_limit is not None and len(self.authorized_sessions) > self.session_limit:
                        self.authorized_sessions.pop(0).authorized = False
                return FakeResponse(HttpStatus.C_200_OK,
                                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode, "authenticated": True},
                                    cookies=cookies)
            if self.mode in (GatewayCfgLanAuthType.BASIC, GatewayCfgLanAuthType.DIGEST):
                credential_response: FakeResponse = self._credentials_response(headers, GatewayApi.AUTH)
                return FakeResponse(
                    credential_response.status_code,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode,
                     "authenticated": credential_response.status_code == HttpStatus.C_200_OK},
                    headers=credential_response.headers,
                )
            if self._is_session_authorized(session, headers):
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {GatewayCfgDesc.LAN_AUTH_TYPE: self.mode},
                )
            return self._interactive_challenge_response(session, True)

        if method == HttpMethod.POST:
            if self.mode not in (GatewayCfgLanAuthType.DEFAULT, GatewayCfgLanAuthType.RUUVI):
                return FakeResponse(HttpStatus.C_503_SERVICE_UNAVAILABLE)
            credentials: tuple[str, str] | None = self._login_credentials()
            valid: bool = False
            if credentials is not None:
                username: str
                ha1: str
                username, ha1 = credentials
                expected_response: str = hashlib.sha256(f"{session.challenge}:{ha1}".encode()).hexdigest()
                valid = (
                    bool(session.challenge)
                    and self._has_session_cookie(session, headers)
                    and body == {"login": username, "password": expected_response}
                )
            if self.wrong_login_success and not valid:
                valid = True
                self.wrong_login_success = False
            session.authorized = valid
            if valid:
                session.challenge = ""
                self.authorized_sessions.append(session)
                if self.session_limit is not None and len(self.authorized_sessions) > self.session_limit:
                    self.authorized_sessions.pop(0).authorized = False
                return FakeResponse(
                    HttpStatus.C_200_OK,
                    {"authenticated": True},
                )
            return self._failed_login_response(session)
        return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})

    def _default_credentials(self) -> tuple[str, str]:
        ha1: str = hashlib.md5(f"Admin:Ruuvi Gateway:{self.dut_config.gw_id}".encode()).hexdigest()
        return "Admin", ha1

    def _login_credentials(self) -> tuple[str, str] | None:
        if self.mode == GatewayCfgLanAuthType.DEFAULT:
            return self._default_credentials()
        if self.mode == GatewayCfgLanAuthType.RUUVI and self.custom_fallback_enabled:
            return self.custom_username, self.custom_ha1
        return None

    def _challenge_values(self, session: FakeSession) -> tuple[str, str]:
        return (
            f"challenge-{session.number}-{session.challenge_number}",
            f"session-{session.number}-{session.challenge_number}",
        )

    def _failed_login_response(self, session: FakeSession) -> FakeResponse:
        return self._interactive_challenge_response(session, False)

    def _config_response(self) -> FakeResponse:
        if self.mode == GatewayCfgLanAuthType.RUUVI and self.prepared_config_timeouts > 0:
            self.prepared_config_timeouts -= 1
            raise requests.Timeout("prepared-state timeout")
        payload: dict[str, str | bool] = {
            GatewayCfgDesc.LAN_AUTH_TYPE: self.mode,
            GatewayCfgDesc.LAN_AUTH_USER: (
                self.custom_username if self.mode != GatewayCfgLanAuthType.DEFAULT else "Admin"
            ),
            GatewayCfgDesc.LAN_AUTH_API_KEY_USE: bool(self.ro_key),
            GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE: bool(self.rw_key),
            GatewayCfgDesc.GW_MAC: self.dut_config.gw_mac,
            GatewayCfgDesc.FW_VER: "test",
        }
        if self.restored_config_mismatch and self.mode == GatewayCfgLanAuthType.DEFAULT and self.config_bodies:
            payload["unexpected"] = True
        return FakeResponse(HttpStatus.C_200_OK, payload)

    def _read_response(self, path: str) -> FakeResponse:
        return FakeResponse(HttpStatus.C_200_OK, {})

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"error": "unauthorized"})

    def _apply_config(self, body: dict[str, str]) -> None:
        if not body:
            return
        old_auth: tuple[str, str, str] = (self.mode, self.custom_username, self.custom_ha1)
        self.mode = body.get(GatewayCfgDesc.LAN_AUTH_TYPE, self.mode)
        self.custom_username = body.get(GatewayCfgDesc.LAN_AUTH_USER, self.custom_username)
        self.custom_ha1 = body.get(GatewayCfgDesc.LAN_AUTH_PASS, self.custom_ha1)
        if self.mode != GatewayCfgLanAuthType.DEFAULT or not self.temporary_bearers_survive:
            self.ro_key = body.get(GatewayCfgDesc.LAN_AUTH_API_KEY, self.ro_key)
            self.rw_key = body.get(GatewayCfgDesc.LAN_AUTH_API_KEY_RW, self.rw_key)
        if old_auth != (self.mode, self.custom_username, self.custom_ha1):
            session: FakeSession
            for session in self.authorized_sessions:
                session.authorized = False
            self.authorized_sessions.clear()


class DefaultAuthGateway(FakeGateway):
    """Default-credential fixture for access-control cases, with injectable reported auth mode.

    These cases retain a session cookie and return plain denials after failed logins.
    Brute-force cases use FakeGateway's chained challenge responses instead.
    """

    def __init__(self, config: DutConfig) -> None:
        super().__init__(config)
        self.custom_username = "Admin"
        self.session_limit = None

    def _login_credentials(self) -> tuple[str, str]:
        return self._default_credentials()

    def _challenge_values(self, session: FakeSession) -> tuple[str, str]:
        return f"challenge-{session.number}-{session.challenge_number}", f"session-{session.number}"

    def _failed_login_response(self, session: FakeSession) -> FakeResponse:
        self.authorized_sessions = [active for active in self.authorized_sessions if active is not session]
        return FakeResponse(HttpStatus.C_401_UNAUTHORIZED, {"authenticated": False})

    def _read_response(self, path: str) -> FakeResponse:
        return FakeResponse(HttpStatus.C_200_OK, {"data": []} if path == GatewayApi.HISTORY else {})

    def _unauthorized_response(self, method: str, path: str, headers: Mapping[str, str]) -> FakeResponse:
        status: int = (
            HttpStatus.C_302_FOUND
            if method == HttpMethod.GET and self._bearer(headers) is None
            else HttpStatus.C_401_UNAUTHORIZED
        )
        return FakeResponse(status, {"error": "unauthorized"})


class FakeSession(requests.Session):
    def __init__(self, gateway: FakeGateway) -> None:
        super().__init__()
        self.gateway: FakeGateway = gateway
        self.authorized: bool = False
        self.number: int = gateway.next_session_number
        self.challenge_number: int = 0
        self.challenge: str = ""
        self.cookie: str = ""
        gateway.next_session_number += 1

    def prepare_request(self, request: requests.Request) -> requests.PreparedRequest:
        return super().prepare_request(request)

    def send(self, request: requests.PreparedRequest, **kwargs: Any) -> FakeResponse:
        if kwargs.get("allow_redirects") is not False:
            raise AssertionError("functional probes must disable redirects")
        method: str | None = request.method
        url: str | None = request.url
        if method is None or url is None:
            raise ValueError("prepared request must contain a method and URL")
        body: Any = json.loads(request.body) if request.body else None
        response: FakeResponse = self.gateway.response_for(
            self,
            method,
            urlsplit(url).path,
            request.headers,
            body,
        )
        self.cookies.update(response.cookies)
        return response


class UniqueRandom:
    def __init__(self) -> None:
        self.calls: list[int] = []
        self.value: int = 0

    def __call__(self, size: int) -> bytes:
        self.calls.append(size)
        self.value += 1
        return bytes((self.value + index) % 256 for index in range(size))
