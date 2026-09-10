"""Shared data models and console progress reporting."""

import ipaddress
from dataclasses import dataclass
from typing import TYPE_CHECKING, Callable, Dict, Optional, Set

if TYPE_CHECKING:
    from .http_api import ApiRoute


@dataclass(frozen=True)
class DutConfig:
    gw_id: str
    gw_mac: str
    gw_hostname: str

    @property
    def base_url(self) -> str:
        try:
            address = ipaddress.ip_address(self.gw_hostname)
        except ValueError:
            return f"http://{self.gw_hostname}"
        if address.version == 6:
            return f"http://[{self.gw_hostname}]"
        return f"http://{self.gw_hostname}"


@dataclass
class RunResult:
    exit_code: int
    verdict: str
    outcomes: Dict[str, str]
    coverage: Set["ApiRoute"]
    recovery_message: Optional[str] = None


class ProgressReporter:
    def __init__(self, output: Callable[[str], None], total: int) -> None:
        self.output = output
        self.total = total
        self.current = 0

    def step(self, description: str) -> None:
        self.current += 1
        self.output(f"[Step {self.current} out of {self.total}] {description}")
