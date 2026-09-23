"""Injectable USB serial discovery and bounded ESP32 reset/console capture."""

from __future__ import annotations

import math
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from importlib import import_module
from types import ModuleType
from typing import Callable, Iterable, Protocol

from .errors import InvalidSetup
from .evidence import EvidenceLog

CH340_VID: int = 0x1A86
UART_BAUD: int = 115200
TOOL_VERSION_TIMEOUT: float = 10.0
RESET_TIMEOUT_SECONDS: float = 20.0


@dataclass(frozen=True)
class SerialPort:
    device: str
    vid: int | None
    pid: int | None


@dataclass(frozen=True)
class SerialVersions:
    esptool: str
    pyserial: str
    esptool_source: str = "Python module: esptool"


@dataclass(frozen=True)
class SerialCommandResult:
    return_code: int | None
    stdout: str
    stderr: str


def _command_output(value: str | bytes | None) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="backslashreplace")
    return "" if value is None else value


class PortInfo(Protocol):
    @property
    def device(self) -> str: ...

    @property
    def vid(self) -> int | None: ...

    @property
    def pid(self) -> int | None: ...


class SerialConnection(Protocol):
    dtr: bool
    rts: bool
    timeout: float | None
    port: str | None

    def open(self) -> None: ...
    def reset_input_buffer(self) -> None: ...
    def read(self, size: int) -> bytes: ...
    def close(self) -> None: ...


def preflight_serial(
    importer: Callable[[str], ModuleType] = import_module,
    find_executable: Callable[[str], str | None] = shutil.which,
    run_command: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> SerialVersions:
    """Check an imported esptool or a PATH executable without accessing hardware."""
    source: str = "Python module: esptool"
    version: str
    try:
        esptool: ModuleType = importer("esptool")
        version = str(esptool.__version__)
    except ModuleNotFoundError as error:
        error: ModuleNotFoundError
        # Do not hide a broken installed package whose own dependency is missing.
        if error.name != "esptool":
            raise
        executable: str | None = find_executable("esptool") or find_executable("esptool.py")
        if executable is None:
            raise InvalidSetup(
                f"esptool is not importable by {sys.executable} and neither esptool nor esptool.py "
                "is on PATH; install requirements.txt in the test environment or expose ESP-IDF's esptool.py"
            ) from error
        try:
            completed: subprocess.CompletedProcess[str] = run_command(
                [executable, "version"], capture_output=True, text=True,
                timeout=TOOL_VERSION_TIMEOUT, check=True,
            )
        except (OSError, subprocess.SubprocessError) as command_error:
            command_error: OSError | subprocess.SubprocessError
            raise InvalidSetup(f"esptool version preflight failed for {executable}: {command_error}") from command_error
        lines: list[str] = completed.stdout.strip().splitlines()
        if not lines:
            raise InvalidSetup(f"esptool version command produced no version: {executable}")
        # esptool's version command prints the version on its last line; old releases
        # also print a banner first. No specific tool version is required.
        version = lines[-1].strip()
        source = executable
    serial: ModuleType = importer("serial")
    return SerialVersions(version, str(serial.__version__), source)


def enumerate_ports(importer: Callable[[str], ModuleType] = import_module) -> Iterable[PortInfo]:
    ports: ModuleType = importer("serial.tools.list_ports")
    return ports.comports()


def discover_serial_port(
    enumerate_fn: Callable[[], Iterable[PortInfo]] = enumerate_ports,
) -> SerialPort:
    observed: tuple[SerialPort, ...] = tuple(
        SerialPort(port.device, port.vid, port.pid) for port in enumerate_fn()
    )
    matches: tuple[SerialPort, ...] = tuple(port for port in observed if port.vid == CH340_VID)
    if len(matches) != 1:
        raise InvalidSetup(
            f"expected exactly one CH340 USB VID 0x{CH340_VID:04X}; "
            f"found {len(matches)} matches {matches!r}; enumerated ports: {observed!r}"
        )
    return matches[0]


def open_serial(
    port: str, baud: int, timeout: float,
    importer: Callable[[str], ModuleType] = import_module,
) -> SerialConnection:
    serial: ModuleType = importer("serial")
    # Configure inactive control lines before opening; do not enter the ROM downloader.
    connection: SerialConnection = serial.Serial(port=None, baudrate=baud, timeout=timeout)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    return connection


class SerialTransport:
    def __init__(
        self,
        enumerate_fn: Callable[[], Iterable[PortInfo]] = enumerate_ports,
        open_fn: Callable[[str, int, float], SerialConnection] = open_serial,
        preflight_fn: Callable[[], SerialVersions] = preflight_serial,
        monotonic: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
        *,
        run_command: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
        python_executable: str = sys.executable,
    ) -> None:
        self.enumerate_fn: Callable[[], Iterable[PortInfo]] = enumerate_fn
        self.open_fn: Callable[[str, int, float], SerialConnection] = open_fn
        self.preflight_fn: Callable[[], SerialVersions] = preflight_fn
        self.monotonic: Callable[[], float] = monotonic
        self.sleep: Callable[[float], None] = sleep
        self.run_command: Callable[..., subprocess.CompletedProcess[str]] = run_command
        self.python_executable: str = python_executable
        self._versions: SerialVersions | None = None

    def preflight(self) -> SerialVersions:
        versions: SerialVersions = self.preflight_fn()
        self._versions = versions
        return versions

    def discover(self) -> SerialPort:
        return discover_serial_port(self.enumerate_fn)

    def _reset(self, port: SerialPort, evidence: EvidenceLog) -> None:
        versions: SerialVersions = self._versions if self._versions is not None else self.preflight()
        prefix: tuple[str, ...] = (
            (self.python_executable, "-m", "esptool")
            if versions.esptool_source == "Python module: esptool"
            else (versions.esptool_source,)
        )
        command: tuple[str, ...] = prefix + (
            "--port", port.device, "--before", "default_reset", "--after", "hard_reset", "read_mac",
        )
        evidence.write("ESPTOOL RESET TOOL", versions)
        evidence.write("ESPTOOL RESET COMMAND", command)
        try:
            completed: subprocess.CompletedProcess[str] = self.run_command(
                list(command), capture_output=True, text=True, timeout=RESET_TIMEOUT_SECONDS, check=False,
            )
        except (OSError, subprocess.SubprocessError) as error:
            error: OSError | subprocess.SubprocessError
            evidence.write("ESPTOOL RESET RESULT", SerialCommandResult(
                getattr(error, "returncode", None),
                _command_output(getattr(error, "stdout", None)),
                _command_output(getattr(error, "stderr", None)),
            ))
            raise InvalidSetup(f"esptool read_mac/reset failed for {port.device}: {error}") from error
        evidence.write("ESPTOOL RESET RESULT", SerialCommandResult(
            completed.returncode, completed.stdout, completed.stderr,
        ))
        if completed.returncode != 0:
            raise InvalidSetup(f"esptool read_mac/reset exited with status {completed.returncode} for {port.device}")

    def capture(
        self, port: SerialPort, evidence: EvidenceLog, duration: float = 30.0,
        *, stop_when: Callable[[str], bool] | None = None,
    ) -> str:
        if not math.isfinite(duration) or duration <= 0:
            raise InvalidSetup("serial capture duration must be finite and positive")
        # esptool owns the serial port until its hard_reset and process exit complete.
        self._reset(port, evidence)
        connection: SerialConnection = self.open_fn(port.device, UART_BAUD, min(0.25, duration))
        chunks: list[bytes] = []
        try:
            # Read immediately: no additional reset, sleep, or input-buffer flush that
            # could discard the application's startup line after esptool exits.
            deadline: float = self.monotonic() + duration
            while True:
                remaining: float = deadline - self.monotonic()
                if remaining <= 0:
                    break
                connection.timeout = min(0.25, remaining)
                chunks.append(connection.read(4096))
                if stop_when is not None and stop_when(b"".join(chunks).decode("utf-8", errors="backslashreplace")):
                    break
        finally:
            try:
                evidence.write("RAW BOOT CONSOLE", b"".join(chunks).decode("utf-8", errors="backslashreplace"))
            finally:
                connection.close()
        return b"".join(chunks).decode("utf-8", errors="backslashreplace")
