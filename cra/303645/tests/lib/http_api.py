"""Canonical firmware HTTP API inventory used by functional tests."""

from __future__ import annotations

from dataclasses import dataclass


class GatewayApi:
    AUTH: str = "/auth"
    AP: str = "/ap.json"
    STATUS: str = "/status.json"
    CONFIG: str = "/ruuvi.json"
    FIRMWARE_UPDATE: str = "/firmware_update.json"
    INFO: str = "/info.json"
    METRICS: str = "/metrics"
    HISTORY: str = "/history"
    VALIDATE_URL: str = "/validate_url"
    EXTRA_CFG: str = "/extra_cfg"
    CONNECT: str = "/connect.json"
    CONNECT_WPS: str = "/connect_wps"
    BLUETOOTH_SCANNING: str = "/bluetooth_scanning.json"
    FW_UPDATE: str = "/fw_update.json"
    FW_UPDATE_URL: str = "/fw_update_url.json"
    FW_UPDATE_RESET: str = "/fw_update_reset"
    GW_CFG_DOWNLOAD: str = "/gw_cfg_download"
    SSL_CERT: str = "/ssl_cert"
    INIT_STORAGE: str = "/init_storage"


class HttpMethod:
    GET: str = "GET"
    POST: str = "POST"
    DELETE: str = "DELETE"


class HttpStatus:
    C_200_OK: int = 200
    C_300_MULTIPLE_CHOICES: int = 300
    C_302_FOUND: int = 302
    C_401_UNAUTHORIZED: int = 401
    C_403_FORBIDDEN: int = 403
    C_404_NOT_FOUND: int = 404
    C_500_INTERNAL_SERVER_ERROR: int = 500


class HttpHeader:
    AUTHORIZATION: str = "Authorization"
    WWW_AUTHENTICATE: str = "WWW-Authenticate"
    USER_AGENT: str = "User-Agent"
    COOKIE: str = "Cookie"
    RUUVI_ECDH_PUBLIC_KEY: str = "Ruuvi-Ecdh-Pub-Key"


class HttpAuthScheme:
    BASIC: str = "Basic"
    DIGEST: str = "Digest"
    BEARER: str = "Bearer"


@dataclass(frozen=True)
class ApiRoute:
    method: str
    path: str


API_INVENTORY: tuple[ApiRoute, ...] = (
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

EXPECTED_API_INVENTORY: set[ApiRoute] = {
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
