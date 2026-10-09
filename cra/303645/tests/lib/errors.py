"""Shared functional-test setup and gateway communication errors."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .webresource import PublicResourceResult


class InvalidSetup(Exception):
    """The local setup or DUT protocol state prevents a valid test run."""


class GatewayConnectionError(InvalidSetup):
    """An HTTP request to the gateway could not be completed."""


class GatewayProtocolError(InvalidSetup):
    """A gateway response does not satisfy the interactive protocol contract."""


class GatewayAuthenticationModeError(GatewayProtocolError):
    """The gateway uses an authentication mode incompatible with the requested flow."""

    def __init__(self, auth_type: str) -> None:
        self.auth_type: str = auth_type
        super().__init__(f"gateway authentication mode is {auth_type!r}")


class WebResourceError(InvalidSetup):
    """An incomplete public fetch, retaining any earlier HTTP observation."""

    def __init__(self, message: str, observation: PublicResourceResult | None = None) -> None:
        self.observation: PublicResourceResult | None = observation
        super().__init__(message)


class WebResourceConnectionError(WebResourceError):
    """DNS, TLS, connection, timeout, or response transfer failure."""


class WebResourceProtocolError(WebResourceError):
    """Malformed URL or redirect protocol prevents completion."""


class WebResourceRedirectError(WebResourceProtocolError):
    """A redirect loop or redirect budget exhaustion prevents completion."""
