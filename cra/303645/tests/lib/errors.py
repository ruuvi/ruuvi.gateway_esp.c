"""Shared functional-test setup and gateway communication errors."""


class InvalidSetup(Exception):
    """The local setup or DUT protocol state prevents a valid test run."""


class GatewayConnectionError(InvalidSetup):
    """An HTTP request to the gateway could not be completed."""


class GatewayProtocolError(InvalidSetup):
    """A gateway response does not satisfy the interactive protocol contract."""


class GatewayAuthenticationModeError(GatewayProtocolError):
    """The gateway uses an authentication mode incompatible with the requested flow."""

    def __init__(self, auth_type: str) -> None:
        self.auth_type = auth_type
        super().__init__(f"gateway authentication mode is {auth_type!r}")
