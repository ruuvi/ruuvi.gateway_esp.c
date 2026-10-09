"""Injectable esptool flash access and strict ESP32 partition metadata decoding."""

from __future__ import annotations

import csv
import hashlib
import subprocess
import sys
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, TextIO

from .errors import InvalidSetup
from .evidence import EvidenceLog
from .serial_dut import ESPTOOL_BAUD, UART_BAUD, SerialPort, SerialVersions

SECTOR_SIZE: int = 0x1000
PARTITION_TABLE_OFFSET: int = 0x8000
PARTITION_TABLE_SIZE: int = 0xC00
PARTITION_ENTRY_SIZE: int = 32
OTADATA_OFFSET: int = 0xD000
OTADATA_SIZE: int = 0x2000
FLASH_TIMEOUT: float = 180.0
FLASH_BAUD: int = ESPTOOL_BAUD
FLASH_READ_ATTEMPTS: int = 3
FLASH_READ_CHUNK_SIZE: int = 0x10000
FLASH_READ_TIMEOUT: float = 30.0


@dataclass(frozen=True)
class Partition:
    name: str
    type_id: int
    subtype: int
    offset: int
    size: int
    flags: int = 0


@dataclass(frozen=True)
class OtaEntry:
    index: int
    sequence: int
    state: int
    crc: int
    valid: bool


@dataclass(frozen=True)
class OtaSelection:
    entries: tuple[OtaEntry, OtaEntry]
    active_slot: str


@dataclass(frozen=True)
class FlashCommandResult:
    arguments: tuple[str, ...]
    return_code: int | None
    stdout: str
    stderr: str


def parse_partition_table(data: bytes) -> tuple[Partition, ...]:
    if len(data) != PARTITION_TABLE_SIZE:
        raise InvalidSetup(f"partition table length is {len(data)}, expected {PARTITION_TABLE_SIZE}")
    parts: list[Partition] = []
    md5_seen: bool = False
    index: int
    for index in range(0, len(data), PARTITION_ENTRY_SIZE):
        entry: bytes = data[index:index + PARTITION_ENTRY_SIZE]
        if entry[:2] == b"\xff\xff":
            if entry[2:4] != b"\xff\xff":
                raise InvalidSetup(f"invalid partition table terminator at 0x{index:x}")
            break
        if entry[:2] == b"\xeb\xeb":
            if md5_seen:
                raise InvalidSetup("partition table has more than one MD5 record")
            if entry[2:16] != b"\xff" * 14 or entry[16:32] != hashlib.md5(data[:index]).digest():
                raise InvalidSetup(f"invalid partition table MD5 record at 0x{index:x}")
            md5_seen = True
            continue
        if entry[:2] != b"\xaa\x50":
            raise InvalidSetup(f"invalid partition entry magic at 0x{index:x}")
        try:
            name: str = entry[12:28].split(b"\x00", 1)[0].decode("ascii")
        except UnicodeDecodeError as error:
            error: UnicodeDecodeError
            raise InvalidSetup(f"non-ASCII partition name at 0x{index:x}") from error
        if not name or any(part.name == name for part in parts):
            raise InvalidSetup(f"missing or duplicate partition name {name}")
        parts.append(Partition(name, entry[2], entry[3],
                               int.from_bytes(entry[4:8], "little"),
                               int.from_bytes(entry[8:12], "little"),
                               int.from_bytes(entry[28:32], "little")))
    else:
        raise InvalidSetup("partition table has no terminating entry")
    if not parts:
        raise InvalidSetup("partition table has no entries")
    return tuple(parts)


def _number(value: str) -> int:
    clean: str = value.strip().upper()
    if clean.endswith("M"):
        return int(clean[:-1], 0) * 1024 * 1024
    if clean.endswith("K"):
        return int(clean[:-1], 0) * 1024
    return int(clean, 0)


def parse_partition_csv(path: Path) -> tuple[Partition, ...]:
    subtype_ids: dict[str, int] = {"nvs": 2, "ota": 0, "phy": 1, "fat": 0x81,
                                    "ota_0": 0x10, "ota_1": 0x11}
    type_ids: dict[str, int] = {"app": 0, "data": 1}
    flag_masks: dict[str, int] = {"encrypted": 1}
    parts: list[Partition] = []
    stream: TextIO
    try:
        with path.open(newline="", encoding="utf-8") as stream:
            row: list[str]
            for row in csv.reader(line for line in stream if not line.lstrip().startswith("#")):
                if not row or not row[0].strip():
                    continue
                name: str = row[0].strip()
                flags: int = 0
                flag: str
                for flag in row[5].strip().split(":") if len(row) > 5 else ():
                    if flag:
                        flags |= flag_masks[flag]
                offset: int = _number(row[3])
                size: int = _number(row[4])
                if offset < 0 or size <= 0:
                    raise ValueError(f"partition {name} requires a non-negative offset and positive size")
                parts.append(Partition(name, type_ids[row[1].strip()], subtype_ids[row[2].strip()],
                                       offset, size, flags))
    except (OSError, UnicodeError, ValueError, KeyError, IndexError) as error:
        error: OSError | UnicodeError | ValueError | KeyError | IndexError
        raise InvalidSetup(f"cannot parse partition CSV {path}: {error}") from error
    return tuple(parts)


def decode_otadata(data: bytes) -> OtaSelection:
    if len(data) != OTADATA_SIZE:
        raise InvalidSetup(f"otadata length is {len(data)}, expected {OTADATA_SIZE}")
    entries: list[OtaEntry] = []
    index: int
    for index in range(2):
        raw: bytes = data[index * SECTOR_SIZE:index * SECTOR_SIZE + 32]
        sequence: int = int.from_bytes(raw[:4], "little")
        state: int = int.from_bytes(raw[24:28], "little")
        crc: int = int.from_bytes(raw[28:32], "little")
        # ESP-IDF crc32_le(UINT32_MAX, sequence, 4) matches zlib's seeded CRC.
        expected_crc: int = zlib.crc32(raw[:4], 0xFFFFFFFF)
        valid: bool = sequence != 0xFFFFFFFF and state not in (3, 4) and crc == expected_crc
        entries.append(OtaEntry(index, sequence, state, crc, valid))
    valid_entries: list[OtaEntry] = [entry for entry in entries if entry.valid]
    if not valid_entries:
        raise InvalidSetup("otadata has no valid OTA slot")
    # max() retains the first entry on a tie, matching the pinned bootloader.
    chosen: OtaEntry = max(valid_entries, key=lambda entry: entry.sequence)
    raw_sequence: int = (chosen.sequence - 1) & 0xFFFFFFFF
    return OtaSelection((entries[0], entries[1]), f"ota_{raw_sequence % 2}")


class FlashTool:
    def __init__(
        self, port: SerialPort, versions: SerialVersions, evidence: EvidenceLog,
        run_command: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
        python_executable: str = sys.executable,
    ) -> None:
        self.port: SerialPort = port
        self.versions: SerialVersions = versions
        self.evidence: EvidenceLog = evidence
        self.run_command: Callable[..., subprocess.CompletedProcess[str]] = run_command
        self.python_executable: str = python_executable

    def command(
        self, *args: str, after: str = "hard_reset", timeout: float = FLASH_TIMEOUT,
        baud: int = FLASH_BAUD,
    ) -> FlashCommandResult:
        prefix: tuple[str, ...] = ((self.python_executable, "-m", "esptool")
                                   if self.versions.esptool_source == "Python module: esptool"
                                   else (self.versions.esptool_source,))
        command: tuple[str, ...] = prefix + ("--chip", "esp32", "--port", self.port.device,
                                            "--baud", str(baud),
                                            "--before", "default_reset", "--after", after) + args
        self.evidence.write("ESPTOOL COMMAND", command)
        try:
            completed: subprocess.CompletedProcess[str] = self.run_command(
                list(command), capture_output=True, text=True, timeout=timeout, check=False,
            )
        except (OSError, subprocess.SubprocessError) as error:
            error: OSError | subprocess.SubprocessError
            stdout: str | bytes | None = getattr(error, "stdout", None)
            stderr: str | bytes | None = getattr(error, "stderr", None)
            self.evidence.write("ESPTOOL RESULT", FlashCommandResult(
                args, getattr(error, "returncode", None),
                stdout.decode("utf-8", errors="backslashreplace") if isinstance(stdout, bytes) else stdout or "",
                stderr.decode("utf-8", errors="backslashreplace") if isinstance(stderr, bytes) else stderr or "",
            ))
            self.evidence.write("ESPTOOL FAILURE", f"{type(error).__name__}: {error}")
            raise InvalidSetup(f"esptool {' '.join(args)} failed: {error}") from error
        result: FlashCommandResult = FlashCommandResult(args, completed.returncode,
                                                        completed.stdout, completed.stderr)
        self.evidence.write("ESPTOOL RESULT", result)
        if result.return_code != 0:
            lines: list[str] = (result.stderr + "\n" + result.stdout).splitlines()
            diagnostic: str = lines[-1] if lines else "no diagnostic output"
            raise InvalidSetup(f"esptool {' '.join(args)} returned {result.return_code}: {diagnostic}")
        return result

    def identify(self) -> tuple[str, str]:
        chip: FlashCommandResult = self.command("chip_id")
        flash: FlashCommandResult = self.command("flash_id")
        if "Chip ID" not in chip.stdout or "Detected flash size" not in flash.stdout:
            raise InvalidSetup("esptool did not report chip ID and flash size")
        return chip.stdout, flash.stdout

    def read(self, offset: int, size: int, path: Path) -> bytes:
        if offset < 0 or size <= 0:
            raise InvalidSetup("flash reads require a non-negative offset and positive size")
        path.parent.mkdir(parents=True, exist_ok=True)
        # Keep the existing backup until the complete read has been validated.
        chunk_path: Path = path.with_name(f"{path.name}.chunk")
        buffer: bytearray = bytearray(size)
        frontier: int = 0
        cursor: int = 0
        failures: int = 0
        read_baud: int = FLASH_BAUD
        last_error: InvalidSetup | OSError | None = None
        while frontier < size:
            chunk_size: int = min(FLASH_READ_CHUNK_SIZE, size - cursor)
            chunk: bytes = b""
            try:
                chunk_path.unlink(missing_ok=True)
                self.command("read_flash", hex(offset + cursor), hex(chunk_size), str(chunk_path),
                             timeout=min(FLASH_READ_TIMEOUT, self._transfer_timeout(chunk_size)),
                             baud=read_baud)
                chunk = chunk_path.read_bytes()
                if len(chunk) != chunk_size:
                    raise InvalidSetup(f"short flash read at 0x{offset + cursor:x}: {len(chunk)} of {chunk_size}")
            except (InvalidSetup, OSError) as error:
                error: InvalidSetup | OSError
                last_error = error
                failures += 1
                retry_from: int = max(0, frontier - SECTOR_SIZE)
                if read_baud == FLASH_BAUD and (
                    "Corrupt data" in str(error) or "short flash read" in str(error)
                    or isinstance(error.__cause__, subprocess.TimeoutExpired)
                ):
                    read_baud = UART_BAUD
                    self.evidence.write("FLASH READ BAUD FALLBACK", {"baud": read_baud,
                                                                     "reason": str(error)})
                self.evidence.write("FLASH READ ATTEMPT FAILED", {
                    "offset": hex(offset + cursor), "size": chunk_size, "attempt": failures,
                    "committed_bytes": frontier, "retry_from": hex(offset + retry_from),
                    "next_baud": read_baud,
                    "reason": f"{type(error).__name__}: {error}",
                })
                if failures >= FLASH_READ_ATTEMPTS:
                    break
                cursor = retry_from
                continue
            finally:
                chunk_path.unlink(missing_ok=True)
            end: int = cursor + chunk_size
            overlap: int = min(frontier, end) - cursor
            if overlap > 0 and buffer[cursor:cursor + overlap] != chunk[:overlap]:
                raise InvalidSetup(f"flash contents changed during chunked read at 0x{offset + cursor:x}")
            buffer[cursor:end] = chunk
            if end > frontier:
                frontier = end
                failures = 0
            cursor = end
            self.evidence.write("FLASH READ PROGRESS", {"offset": hex(offset), "committed_bytes": frontier,
                                                        "total_bytes": size})
        if frontier == size:
            data: bytes = bytes(buffer)
            # Keep the previous backup until a complete replacement is on the same filesystem.
            directory: str
            with tempfile.TemporaryDirectory(prefix=f".{path.name}.", dir=path.parent) as directory:
                replacement: Path = Path(directory) / path.name
                replacement.write_bytes(data)
                replacement.replace(path)
            return data
        try:
            self.command("read_mac")
            self.evidence.write("FLASH READ RECOVERY RESET", "hard reset completed")
        except InvalidSetup as reset_error:
            reset_error: InvalidSetup
            self.evidence.write("FLASH READ RECOVERY RESET FAILURE", str(reset_error))
            raise InvalidSetup(
                f"flash read at 0x{offset + cursor:x} failed after {FLASH_READ_ATTEMPTS} attempts; "
                f"last error: {last_error}; gateway reset also failed"
            ) from reset_error
        raise InvalidSetup(
            f"flash read at 0x{offset + cursor:x} failed after {FLASH_READ_ATTEMPTS} attempts: "
            f"{last_error}"
        ) from last_error

    def write(self, offset: int, data: bytes, path: Path) -> None:
        if offset < 0:
            raise InvalidSetup("flash writes require a non-negative offset")
        if offset % SECTOR_SIZE or len(data) % SECTOR_SIZE or not data:
            raise InvalidSetup("flash writes must cover aligned complete sectors")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        # The runner opens UART after its own explicit reset. Do not boot between
        # a corruption write and that capture, or rollback evidence can be missed.
        self.command("write_flash", hex(offset), str(path), after="no_reset",
                     timeout=self._transfer_timeout(len(data)))

    @staticmethod
    def _transfer_timeout(size: int) -> float:
        # Each byte takes at least ten wire bits. Allow for
        # framing, erase work, and host scheduling without an unbounded wait.
        return max(FLASH_TIMEOUT, 60.0 + size * 15.0 / FLASH_BAUD)
