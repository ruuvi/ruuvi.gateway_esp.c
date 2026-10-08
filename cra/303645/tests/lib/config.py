"""Strict local DUT configuration loading for functional tests."""

from __future__ import annotations

import ipaddress
import json
import re
from pathlib import Path
from typing import Any, Iterable

from .errors import InvalidSetup
from .gateway import GatewayCfgDesc
from .models import DutConfig

OCTETS_8_RE: re.Pattern[str] = re.compile(r"^(?:[0-9A-Fa-f]{2}:){7}[0-9A-Fa-f]{2}$")
OCTETS_6_RE: re.Pattern[str] = re.compile(r"^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
HOSTNAME_RE: re.Pattern[str] = re.compile(
    r"^(?=.{1,253}\.?$)(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)*"
    r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.?$"
)
FIRMWARE_VERSION_RE: re.Pattern[str] = re.compile(r"v\d+\.\d+\.\d+-(?:prod|dev)")
DEFAULT_GATEWAY_UI_CONFIG_PATH: Path = (
    Path(__file__).resolve().parent.parent.parent.parent.parent / "gw_cfg_default" / "gw_cfg_default_gen_ui.json"
)
PORTABLE_GATEWAY_UI_CONFIG_PATH: Path = Path(__file__).resolve().parent.parent / "gw_cfg_default_gen_ui.json"
FACTORY_RESET_MESSAGE: str = (
    "USER ACTION REQUIRED: The gateway is not in the required factory-default state. "
    "Factory reset erases saved local configuration, credentials, tokens, and uploaded "
    "certificates/private keys and interrupts connectivity. Back up needed settings first. "
    "It does not delete cloud accounts or already-forwarded data. Hold CONFIGURE through the "
    "LED turning off and the Gateway restarting until the red LED repeatedly turns on for "
    "200 ms and off for 200 ms (normally about 11 seconds after the initial press). "
    "Release only after this completion signal; the Gateway restarts again and opens its "
    "configuration hotspot. If the signal never appears, do not assume erasure succeeded. "
    "Restore LAN connectivity and verify the DUT identity before running the test again."
)
AUTHENTICATION_DEFAULT_FIELDS: tuple[str, ...] = (
    GatewayCfgDesc.LAN_AUTH_TYPE,
    GatewayCfgDesc.LAN_AUTH_USER,
    GatewayCfgDesc.LAN_AUTH_API_KEY_USE,
    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE,
)


class InvalidConfig(InvalidSetup):
    """The local DUT configuration is missing or malformed."""


def load_ui_default_config(
    config_path: Path | None = None,
) -> dict[str, Any]:
    """Load generated defaults as exposed by authenticated GET /ruuvi.json."""
    if config_path is None:
        if DEFAULT_GATEWAY_UI_CONFIG_PATH.exists():
            config_path = DEFAULT_GATEWAY_UI_CONFIG_PATH
        elif PORTABLE_GATEWAY_UI_CONFIG_PATH.exists():
            config_path = PORTABLE_GATEWAY_UI_CONFIG_PATH
        else:
            raise InvalidConfig(
                "Missing factory-default reference file. Copy gw_cfg_default/gw_cfg_default_gen_ui.json "
                f"from the matching firmware checkout to {PORTABLE_GATEWAY_UI_CONFIG_PATH} "
                "(beside the test scripts), or run from the full repository. "
                f"Repository path checked: {DEFAULT_GATEWAY_UI_CONFIG_PATH}"
            )
    try:
        payload: Any = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        error: Exception
        raise InvalidConfig(f"cannot read default gateway configuration {config_path}: {error}") from error
    if not isinstance(payload, dict):
        raise InvalidConfig(f"default gateway configuration {config_path} must contain an object")
    return payload


def default_config_values(
    fields: Iterable[str],
    config_path: Path | None = None,
) -> dict[str, Any]:
    defaults: dict[str, Any] = load_ui_default_config(config_path)
    requested: tuple[str, ...] = tuple(fields)
    missing: list[str] = [field for field in requested if field not in defaults]
    if missing:
        raise InvalidConfig(f"default gateway configuration does not expose field(s): {', '.join(sorted(missing))}")
    return {field: defaults[field] for field in requested}


def validate_hostname(value: str) -> None:
    if not value or value != value.strip():
        raise InvalidConfig(f"{GatewayCfgDesc.GW_HOSTNAME} must be non-empty without surrounding whitespace")
    if any(char in value for char in "/?#@") or "://" in value:
        raise InvalidConfig(
            f"{GatewayCfgDesc.GW_HOSTNAME} must not contain a scheme, path, query, fragment, or credentials"
        )
    if any(char.isspace() for char in value):
        raise InvalidConfig(f"{GatewayCfgDesc.GW_HOSTNAME} must not contain whitespace")
    try:
        ipaddress.ip_address(value)
        return
    except ValueError:
        pass
    if ":" in value:
        raise InvalidConfig(f"{GatewayCfgDesc.GW_HOSTNAME} must not contain a port")
    if not HOSTNAME_RE.fullmatch(value):
        raise InvalidConfig(f"{GatewayCfgDesc.GW_HOSTNAME} is not a valid hostname or IP address")


def load_dut_config(env_path: Path) -> DutConfig:
    required: set[str] = {
        GatewayCfgDesc.GW_ID,
        GatewayCfgDesc.GW_MAC,
        GatewayCfgDesc.GW_HOSTNAME,
    }
    allowed: set[str] = required | {"gw_fw", "wifi_ssid", "wifi_password"}
    values: dict[str, str] = {}
    try:
        lines: list[str] = env_path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        error: Exception
        raise InvalidConfig(f"cannot read {env_path}: {error}") from error

    raw_line: str
    line_number: int
    for line_number, raw_line in enumerate(lines, 1):
        line: str = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise InvalidConfig(f"malformed .env line {line_number}")
        value: str
        key: str
        key, value = (part.strip() for part in line.split("=", 1))
        if key in ("wifi_ssid", "wifi_password"):
            # Credentials are literal: retain spaces, # and =, without shell expansion.
            value = raw_line.split("=", 1)[1]
        if key not in allowed:
            raise InvalidConfig(f"unknown .env key {key!r} on line {line_number}")
        if key in values:
            raise InvalidConfig(f"duplicate .env key {key!r}")
        values[key] = value

    missing: set[str] = required.difference(values)
    if missing:
        raise InvalidConfig(f"missing .env key(s): {', '.join(sorted(missing))}")
    if not OCTETS_8_RE.fullmatch(values[GatewayCfgDesc.GW_ID]):
        raise InvalidConfig(f"{GatewayCfgDesc.GW_ID} must contain eight colon-separated hexadecimal octets")
    if not OCTETS_6_RE.fullmatch(values[GatewayCfgDesc.GW_MAC]):
        raise InvalidConfig(f"{GatewayCfgDesc.GW_MAC} must contain six colon-separated hexadecimal octets")
    validate_hostname(values[GatewayCfgDesc.GW_HOSTNAME])
    firmware_version: str | None = values.get("gw_fw") or None
    if firmware_version is not None and FIRMWARE_VERSION_RE.fullmatch(firmware_version) is None:
        raise InvalidConfig("gw_fw must look like v1.17.5-dev or v1.17.5-prod")
    wifi_ssid: str | None = values.get("wifi_ssid")
    wifi_password: str | None = values.get("wifi_password")
    if (wifi_ssid is None) != (wifi_password is None):
        raise InvalidConfig("wifi_ssid and wifi_password must be supplied together")
    if wifi_ssid is not None and not 1 <= len(wifi_ssid.encode("utf-8")) <= 32:
        raise InvalidConfig("wifi_ssid must contain 1 to 32 UTF-8 bytes")
    if wifi_password is not None and not 8 <= len(wifi_password.encode("utf-8")) <= 63:
        raise InvalidConfig("wifi_password must contain 8 to 63 UTF-8 bytes for a password-protected test network")
    if wifi_ssid is not None and wifi_password is not None and ("\0" in wifi_ssid or "\0" in wifi_password):
        raise InvalidConfig("Wi-Fi credentials must not contain NUL characters")
    return DutConfig(values[GatewayCfgDesc.GW_ID], values[GatewayCfgDesc.GW_MAC],
                     values[GatewayCfgDesc.GW_HOSTNAME], firmware_version, wifi_ssid, wifi_password)
