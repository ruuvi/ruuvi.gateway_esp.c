"""Logged, unauthenticated HTTPS observations; callers decide accessibility verdicts."""

from __future__ import annotations

import math
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
        if not parsed.hostname or parsed.username is not None or parsed.password is not None:
            raise ValueError("URL must have a host and no credentials")
        # Accessing port also validates its range and syntax.
        if parsed.port is not None and parsed.port == 0:
            raise ValueError("URL port must be positive")
        if any(character.isspace() for character in url):
            raise ValueError("URL must not contain whitespace")
        return parsed
    except ValueError as error:
        error: ValueError
        raise WebResourceProtocolError(f"invalid public-resource URL: {url}") from error


def _fetch_session(
    session: requests.Session,
    url: str,
    evidence: EvidenceLog,
    allowed_hosts: frozenset[str],
    timeout: tuple[float, float],
    max_redirects: int,
) -> PublicResourceResult:
    observation: PublicResourceResult | None = None
    redirects: tuple[RedirectHop, ...] = ()
    visited: set[str] = set()
    authentication_seen: bool = False
    try:
        while True:
            prepared: requests.PreparedRequest = session.prepare_request(requests.Request(HttpMethod.GET, url))
            current_url: str = prepared.url or url
            visited.add(current_url)
            evidence.write_http_request(prepared)
            response: requests.Response = session.send(
                prepared, timeout=timeout, allow_redirects=False, verify=True, proxies={}, stream=False,
            )
            evidence.write_http_response(response)
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
    except ValueError as url_error:
        url_error: ValueError
        raise WebResourceProtocolError(f"malformed public-resource redirect: {url_error}", observation) from url_error


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
) -> PublicResourceResult:
    """Fetch within HTTPS/host bounds, returning observations without a compliance verdict.

    The last response remains final_url when a forbidden redirect is not followed; its
    destination is retained in redirects. Errors retain the last complete observation.
    The injected factory must supply a fresh session, whose lifetime belongs to this call.
    """
    parsed: SplitResult = _parse_url(url)
    if parsed.scheme != "https" or parsed.hostname not in allowed_hosts:
        raise InvalidSetup("initial public URL must use HTTPS and an allowed host")
    if (not math.isfinite(connect_timeout) or connect_timeout <= 0
            or not math.isfinite(read_timeout) or read_timeout <= 0 or max_redirects < 0):
        raise InvalidSetup("public-resource timeouts must be finite and positive; redirect limit nonnegative")
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
        )
    finally:
        session.close()
