"""Strict local DUT configuration loading for functional tests."""

import ipaddress
import json
import re
from pathlib import Path
from typing import Any, Dict, Iterable

from .errors import InvalidSetup
from .gateway import GatewayCfgDesc
from .models import DutConfig


OCTETS_8_RE = re.compile(r"^(?:[0-9A-Fa-f]{2}:){7}[0-9A-Fa-f]{2}$")
OCTETS_6_RE = re.compile(r"^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
HOSTNAME_RE = re.compile(
    r"^(?=.{1,253}\.?$)(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)*"
    r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.?$"
)
DEFAULT_GATEWAY_UI_CONFIG_PATH = (
    Path(__file__).resolve().parents[4]
    / "gw_cfg_default"
    / "gw_cfg_default_gen_ui.json"
)
FACTORY_RESET_MESSAGE = (
    "USER ACTION REQUIRED: The gateway is not in the required factory-default state. "
    "Perform a factory reset by pressing and holding the CONFIGURE button for longer "
    "than 7 seconds, then run the test again."
)
AUTHENTICATION_DEFAULT_FIELDS = (
    GatewayCfgDesc.LAN_AUTH_TYPE,
    GatewayCfgDesc.LAN_AUTH_USER,
    GatewayCfgDesc.LAN_AUTH_API_KEY_USE,
    GatewayCfgDesc.LAN_AUTH_API_KEY_RW_USE,
)
class InvalidConfig(InvalidSetup):
    """The local DUT configuration is missing or malformed."""


def load_ui_default_config(
    config_path: Path = DEFAULT_GATEWAY_UI_CONFIG_PATH,
) -> Dict[str, Any]:
    """Load generated defaults as exposed by authenticated GET /ruuvi.json."""
    try:
        payload = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise InvalidConfig(f"cannot read default gateway configuration {config_path}: {error}") from error
    if not isinstance(payload, dict):
        raise InvalidConfig(f"default gateway configuration {config_path} must contain an object")
    return payload


def default_config_values(
    fields: Iterable[str],
    config_path: Path = DEFAULT_GATEWAY_UI_CONFIG_PATH,
) -> Dict[str, Any]:
    defaults = load_ui_default_config(config_path)
    requested = tuple(fields)
    missing = [field for field in requested if field not in defaults]
    if missing:
        raise InvalidConfig(
            "default gateway configuration does not expose field(s): "
            f"{', '.join(sorted(missing))}"
        )
    return {field: defaults[field] for field in requested}


def validate_hostname(value: str) -> None:
    if not value or value != value.strip():
        raise InvalidConfig(
            f"{GatewayCfgDesc.GW_HOSTNAME} must be non-empty without surrounding whitespace"
        )
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
        raise InvalidConfig(
            f"{GatewayCfgDesc.GW_HOSTNAME} is not a valid hostname or IP address"
        )


def load_dut_config(env_path: Path) -> DutConfig:
    expected = {
        GatewayCfgDesc.GW_ID,
        GatewayCfgDesc.GW_MAC,
        GatewayCfgDesc.GW_HOSTNAME,
    }
    values: Dict[str, str] = {}
    try:
        lines = env_path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        raise InvalidConfig(f"cannot read {env_path}: {error}") from error

    for line_number, raw_line in enumerate(lines, 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise InvalidConfig(f"malformed .env line {line_number}")
        key, value = (part.strip() for part in line.split("=", 1))
        if key not in expected:
            raise InvalidConfig(f"unknown .env key {key!r} on line {line_number}")
        if key in values:
            raise InvalidConfig(f"duplicate .env key {key!r}")
        values[key] = value

    missing = expected.difference(values)
    if missing:
        raise InvalidConfig(f"missing .env key(s): {', '.join(sorted(missing))}")
    if not OCTETS_8_RE.fullmatch(values[GatewayCfgDesc.GW_ID]):
        raise InvalidConfig(
            f"{GatewayCfgDesc.GW_ID} must contain eight colon-separated hexadecimal octets"
        )
    if not OCTETS_6_RE.fullmatch(values[GatewayCfgDesc.GW_MAC]):
        raise InvalidConfig(
            f"{GatewayCfgDesc.GW_MAC} must contain six colon-separated hexadecimal octets"
        )
    validate_hostname(values[GatewayCfgDesc.GW_HOSTNAME])
    return DutConfig(**values)
