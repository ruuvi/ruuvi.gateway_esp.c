"""Canonical firmware HTTP API inventory used by functional tests."""

from dataclasses import dataclass
from typing import Set, Tuple

class HttpMethod:
    GET = "GET"
    POST = "POST"
    DELETE = "DELETE"


class HttpStatus:
    C_200_OK = 200
    C_300_MULTIPLE_CHOICES = 300
    C_302_FOUND = 302
    C_401_UNAUTHORIZED = 401
    C_403_FORBIDDEN = 403
    C_500_INTERNAL_SERVER_ERROR = 500


class HttpHeader:
    AUTHORIZATION = "Authorization"
    WWW_AUTHENTICATE = "WWW-Authenticate"
    USER_AGENT = "User-Agent"
    COOKIE = "Cookie"
    RUUVI_ECDH_PUBLIC_KEY = "Ruuvi-Ecdh-Pub-Key"


class HttpAuthScheme:
    BASIC = "Basic"
    DIGEST = "Digest"
    BEARER = "Bearer"

from .gateway import GatewayApi


@dataclass(frozen=True)
class ApiRoute:
    method: str
    path: str

API_INVENTORY: Tuple[ApiRoute, ...] = (
    ApiRoute(HttpMethod.GET, GatewayApi.AUTH),
    ApiRoute(HttpMethod.GET, GatewayApi.AP),
    ApiRoute(HttpMethod.GET, GatewayApi.STATUS),
    ApiRoute(HttpMethod.GET, GatewayApi.CONFIG),
    ApiRoute(HttpMethod.GET, GatewayApi.FIRMWARE_UPDATE),
    ApiRoute(HttpMethod.GET, GatewayApi.INFO),
    ApiRoute(HttpMethod.GET, GatewayApi.METRICS),
    ApiRoute(HttpMethod.GET, GatewayApi.HISTORY),
    ApiRoute(HttpMethod.GET, GatewayApi.VALIDATE_URL),
    ApiRoute(HttpMethod.GET, GatewayApi.EXTRA_CFG),
    ApiRoute(HttpMethod.POST, GatewayApi.AUTH),
    ApiRoute(HttpMethod.POST, GatewayApi.CONNECT),
    ApiRoute(HttpMethod.POST, GatewayApi.CONNECT_WPS),
    ApiRoute(HttpMethod.POST, GatewayApi.CONFIG),
    ApiRoute(HttpMethod.POST, GatewayApi.BLUETOOTH_SCANNING),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE_URL),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE_RESET),
    ApiRoute(HttpMethod.POST, GatewayApi.GW_CFG_DOWNLOAD),
    ApiRoute(HttpMethod.POST, GatewayApi.SSL_CERT),
    ApiRoute(HttpMethod.POST, GatewayApi.EXTRA_CFG),
    ApiRoute(HttpMethod.POST, GatewayApi.INIT_STORAGE),
    ApiRoute(HttpMethod.DELETE, GatewayApi.AUTH),
    ApiRoute(HttpMethod.DELETE, GatewayApi.CONNECT),
    ApiRoute(HttpMethod.DELETE, GatewayApi.SSL_CERT),
    ApiRoute(HttpMethod.DELETE, GatewayApi.EXTRA_CFG),
)

EXPECTED_API_INVENTORY: Set[ApiRoute] = {
    ApiRoute(HttpMethod.GET, GatewayApi.AUTH),
    ApiRoute(HttpMethod.GET, GatewayApi.AP),
    ApiRoute(HttpMethod.GET, GatewayApi.STATUS),
    ApiRoute(HttpMethod.GET, GatewayApi.CONFIG),
    ApiRoute(HttpMethod.GET, GatewayApi.FIRMWARE_UPDATE),
    ApiRoute(HttpMethod.GET, GatewayApi.INFO),
    ApiRoute(HttpMethod.GET, GatewayApi.METRICS),
    ApiRoute(HttpMethod.GET, GatewayApi.HISTORY),
    ApiRoute(HttpMethod.GET, GatewayApi.VALIDATE_URL),
    ApiRoute(HttpMethod.GET, GatewayApi.EXTRA_CFG),
    ApiRoute(HttpMethod.POST, GatewayApi.AUTH),
    ApiRoute(HttpMethod.POST, GatewayApi.CONNECT),
    ApiRoute(HttpMethod.POST, GatewayApi.CONNECT_WPS),
    ApiRoute(HttpMethod.POST, GatewayApi.CONFIG),
    ApiRoute(HttpMethod.POST, GatewayApi.BLUETOOTH_SCANNING),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE_URL),
    ApiRoute(HttpMethod.POST, GatewayApi.FW_UPDATE_RESET),
    ApiRoute(HttpMethod.POST, GatewayApi.GW_CFG_DOWNLOAD),
    ApiRoute(HttpMethod.POST, GatewayApi.SSL_CERT),
    ApiRoute(HttpMethod.POST, GatewayApi.EXTRA_CFG),
    ApiRoute(HttpMethod.POST, GatewayApi.INIT_STORAGE),
    ApiRoute(HttpMethod.DELETE, GatewayApi.AUTH),
    ApiRoute(HttpMethod.DELETE, GatewayApi.CONNECT),
    ApiRoute(HttpMethod.DELETE, GatewayApi.SSL_CERT),
    ApiRoute(HttpMethod.DELETE, GatewayApi.EXTRA_CFG),
}
