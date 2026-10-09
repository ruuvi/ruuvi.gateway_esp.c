"""Logged, unauthenticated HTTPS observations; callers decide accessibility verdicts."""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, replace
from typing import Callable
from urllib.parse import SplitResult, urldefrag, urljoin, urlsplit

import requests

from .errors import (
    InvalidSetup,
    WebResourceConnectionError,
    WebResourceError,
    WebResourceProtocolError,
    WebResourceRedirectError,
)
from .evidence import EvidenceLog
from .http_api import HttpHeader, HttpMethod, HttpStatus

REDIRECT_STATUSES: frozenset[int] = frozenset((
    HttpStatus.C_301_MOVED_PERMANENTLY, HttpStatus.C_302_FOUND, HttpStatus.C_303_SEE_OTHER,
    HttpStatus.C_307_TEMPORARY_REDIRECT, HttpStatus.C_308_PERMANENT_REDIRECT,
))
MAX_RESPONSE_BYTES: int = 4 * 1024 * 1024
TOTAL_TIMEOUT: float = 60.0


def _read_response(
    response: requests.Response, max_response_bytes: int, deadline: float, monotonic: Callable[[], float],
) -> None:
    """Cache only a complete bounded body for the existing response evidence writer."""
    body: bytearray = bytearray()
    chunk: bytes
    # A one-byte read exposes slow trickles instead of waiting for a large chunk to fill.
    for chunk in response.iter_content(chunk_size=1):
        if monotonic() >= deadline:
            raise WebResourceConnectionError("incomplete public-resource fetch: total transfer deadline exceeded")
        if len(body) + len(chunk) > max_response_bytes:
            raise WebResourceConnectionError("incomplete public-resource fetch: response size limit exceeded")
        body.extend(chunk)
    if monotonic() >= deadline:
        raise WebResourceConnectionError("incomplete public-resource fetch: total transfer deadline exceeded")
    response._content = bytes(body)


@dataclass(frozen=True)
class RedirectHop:
    status: int
    from_url: str
    to_url: str


@dataclass(frozen=True)
class PublicResourceResult:
    status: int
    final_url: str
    final_scheme: str
    final_host: str
    headers: tuple[tuple[str, str], ...]
    cookies: tuple[tuple[str, str], ...]
    content_type: str
    body_length: int
    redirects: tuple[RedirectHop, ...]
    authentication_seen: bool
    tls_verified: bool = True
    stopped_at_redirect: bool = False


def _parse_url(url: str) -> SplitResult:
    try:
        parsed: SplitResult = urlsplit(url)
    except ValueError:
        # urllib errors can echo the authority, including credentials, in tracebacks.
        raise WebResourceProtocolError("invalid public-resource URL: malformed authority or host") from None
    if parsed.username is not None or parsed.password is not None:
        raise WebResourceProtocolError("invalid public-resource URL: userinfo is not allowed")
    if not parsed.hostname:
        raise WebResourceProtocolError("invalid public-resource URL: host is required")
    try:
        port: int | None = parsed.port
    except ValueError:
        raise WebResourceProtocolError("invalid public-resource URL: invalid port") from None
    if port == 0:
        raise WebResourceProtocolError("invalid public-resource URL: port must be positive")
    if any(character.isspace() for character in url):
        raise WebResourceProtocolError("invalid public-resource URL: whitespace is not allowed")
    return parsed


def _fetch_session(
    session: requests.Session,
    url: str,
    evidence: EvidenceLog,
    allowed_hosts: frozenset[str],
    timeout: tuple[float, float],
    max_redirects: int,
    max_response_bytes: int,
    deadline: float,
    monotonic: Callable[[], float],
) -> PublicResourceResult:
    observation: PublicResourceResult | None = None
    redirects: tuple[RedirectHop, ...] = ()
    visited: set[str] = set()
    authentication_seen: bool = False
    try:
        while True:
            remaining: float = deadline - monotonic()
            if remaining <= 0:
                raise WebResourceConnectionError("incomplete public-resource fetch: total transfer deadline exceeded")
            prepared: requests.PreparedRequest = session.prepare_request(requests.Request(HttpMethod.GET, url))
            current_url: str = prepared.url or url
            visited.add(current_url)
            evidence.write_http_request(prepared)
            response: requests.Response = session.send(
                prepared, timeout=(min(timeout[0], remaining), min(timeout[1], remaining)),
                allow_redirects=False, verify=True, proxies={}, stream=True,
            )
            try:
                _read_response(response, max_response_bytes, deadline, monotonic)
                evidence.write_http_response(response)
            finally:
                response.close()
            authentication_seen = authentication_seen or HttpHeader.WWW_AUTHENTICATE in response.headers
            parsed: SplitResult = _parse_url(current_url)
            current: PublicResourceResult = PublicResourceResult(
                response.status_code, current_url, parsed.scheme, parsed.hostname or "",
                tuple(response.headers.items()), tuple(response.cookies.get_dict().items()),
                response.headers.get(HttpHeader.CONTENT_TYPE, ""), len(response.content),
                redirects, authentication_seen,
            )
            observation = current
            evidence.write("PUBLIC RESOURCE OBSERVATION", current)
            if response.status_code not in REDIRECT_STATUSES:
                return current
            location: str = response.headers.get(HttpHeader.LOCATION, "")
            if not location.strip():
                raise WebResourceProtocolError("redirect response has no non-empty Location")
            target: str = urldefrag(urljoin(current_url, location)).url
            hop: RedirectHop = RedirectHop(response.status_code, current_url, target)
            redirects += (hop,)
            current = replace(current, redirects=redirects, stopped_at_redirect=True)
            observation = current
            evidence.write("REDIRECT HOP", hop)
            evidence.write("PUBLIC RESOURCE OBSERVATION", observation)
            destination: SplitResult = _parse_url(target)
            # Observe policy boundaries without contacting an unapproved host or plaintext URL.
            if destination.scheme != "https" or destination.hostname not in allowed_hosts:
                return current
            if len(redirects) > max_redirects or target in visited:
                raise WebResourceRedirectError("redirect loop or maximum redirects exceeded")
            url = target
    except WebResourceError as resource_error:
        resource_error: WebResourceError
        resource_error.observation = observation
        raise
    except requests.RequestException as request_error:
        request_error: requests.RequestException
        raise WebResourceConnectionError(f"public-resource HTTP failure: {request_error}", observation) from request_error
    except ValueError:
        # urljoin/urldefrag can fail before _parse_url; do not expose their raw diagnostics.
        raise WebResourceProtocolError("malformed public-resource redirect: invalid URL syntax", observation) from None


def fetch_public_resource(
    url: str,
    evidence: EvidenceLog,
    *,
    allowed_hosts: frozenset[str],
    connect_timeout: float,
    read_timeout: float,
    max_redirects: int,
    user_agent: str,
    session_factory: Callable[[], requests.Session] = requests.Session,
    max_response_bytes: int = MAX_RESPONSE_BYTES,
    total_timeout: float = TOTAL_TIMEOUT,
    monotonic: Callable[[], float] = time.monotonic,
) -> PublicResourceResult:
    """Fetch within HTTPS/host bounds, returning observations without a compliance verdict.

    The last response remains final_url when a forbidden redirect is not followed; its
    destination is retained in redirects. Errors retain the last complete observation.
    The injected factory must supply a fresh session, whose lifetime belongs to this call.
    """
    parsed: SplitResult = _parse_url(url)
    if parsed.scheme != "https" or parsed.hostname not in allowed_hosts:
        raise InvalidSetup("initial public URL must use HTTPS and an allowed host")
    if (isinstance(connect_timeout, bool) or not isinstance(connect_timeout, (int, float))
            or not math.isfinite(connect_timeout) or connect_timeout <= 0
            or isinstance(read_timeout, bool) or not isinstance(read_timeout, (int, float))
            or not math.isfinite(read_timeout) or read_timeout <= 0
            or type(max_redirects) is not int or max_redirects < 0):
        raise InvalidSetup("public-resource timeouts must be finite and positive; redirect limit a non-negative integer")
    if type(max_response_bytes) is not int or max_response_bytes <= 0:
        raise InvalidSetup("public-resource response size limit must be a positive integer")
    if (isinstance(total_timeout, bool) or not isinstance(total_timeout, (int, float))
            or not math.isfinite(total_timeout) or total_timeout <= 0):
        raise InvalidSetup("public-resource total timeout must be finite and positive")
    deadline: float = monotonic() + total_timeout
    evidence.write("TLS VERIFICATION ENABLED", True)
    session: requests.Session = session_factory()
    try:
        session.trust_env = False
        session.auth = None
        session.cert = None
        session.headers.clear()
        session.headers[HttpHeader.USER_AGENT] = user_agent
        session.cookies.clear()
        session.params = {}
        session.proxies.clear()
        return _fetch_session(
            session, url, evidence, allowed_hosts, (connect_timeout, read_timeout), max_redirects,
            max_response_bytes, deadline, monotonic,
        )
    finally:
        session.close()
