"""Destructive ETSI 5.7-1-2 Unit A test for a dedicated sacrificial gateway."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import zlib
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Protocol, Sequence

import requests

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    FIRMWARE_VERSION_RE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import GatewayAuthenticationModeError, GatewayConnectionError, InvalidSetup
from lib.evidence import EvidenceLog, format_utc, utc_now
from lib.flash_dut import (
    OTADATA_OFFSET,
    OTADATA_SIZE,
    SECTOR_SIZE,
    FlashTool,
    OtaSelection,
    Partition,
    decode_otadata,
    parse_partition_csv,
    parse_partition_table,
)
from lib.gateway import GatewayCfgDesc, GatewayCfgLanAuthType, GatewayClient, InteractiveAuthResult
from lib.http_api import GatewayApi, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult
from lib.serial_dut import SerialPort, SerialTransport, SerialVersions

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.7-1-2, Test Unit A"
PREFIX: str = "test_5_7_1_2_a"
ADMIN_USERNAME: str = "Admin"
CAPTURE_SECONDS: float = 30.0
BASE_STEPS: int = 15
DEFAULT_FW_VERSION: str = "v1.17.5-prod"
PREPARATION_NOTICE: str = (
    "This destructive test installs the selected release in both slots, runs the integrity cases, "
    "then reflashes both slots and verifies the recovered gateway. It does not back up flash."
)
RECOVERY_NOTICE: str = (
    "Automatic recovery could not be verified. From the repository root, run "
    "./ruuvi_gw_flash.py <fw_version_tag> --flash_both_slots."
)
SAFE_PARTITIONS: frozenset[str] = frozenset({"ota_0", "ota_1", "fatfs_gwui", "fatfs_gwui_2",
                                              "fatfs_nrf52", "fatfs_nrf52_2"})
FORBIDDEN: tuple[tuple[int, int], ...] = ((0x1000, 0x8000), (0x8000, 0x9000),
                                          (0x9000, 0xD000), (0xD000, 0xF000),
                                          (0xF000, 0x10000), (0xB00000, 0xB40000))
APP_CHECKSUM_FAILURE: re.Pattern[str] = re.compile(r"Checksum failed|Image hash failed", re.IGNORECASE)
APP_SIGNATURE_FAILURE: re.Pattern[str] = re.compile(
    r"Secure boot signature verification failed", re.IGNORECASE,
)
APP_SIGNATURE_BAD: re.Pattern[str] = re.compile(r"image valid, signature bad", re.IGNORECASE)
APP_SELF_CHECK_FAILURE: str = "Self check of running partition signature failed"
ROLLBACK_UNAVAILABLE: str = "Rollback is not possible, do not have any suitable apps in slots"
APP_UNBOOTABLE: re.Pattern[str] = re.compile(r"OTA app partition slot ([01]) is not bootable", re.IGNORECASE)
GATEWAY_INIT: re.Pattern[str] = re.compile(r"ruuvi_gateway:\s*\[main/1]\s*### Init gw_cfg:")
BOOT_BANNER: re.Pattern[str] = re.compile(r"rst:0x[0-9a-f]+", re.IGNORECASE)
LOADED_APP: re.Pattern[str] = re.compile(r"Loaded app from partition at offset 0x([0-9a-f]+)", re.IGNORECASE)
RUNNING_APP: re.Pattern[str] = re.compile(
    r"### Currently running partition: (ota_[01]): address 0x([0-9a-f]+)", re.IGNORECASE,
)
SIGNED_NRF: re.Pattern[str] = re.compile(
    r"Successfully verified fatfs partition 'fatfs_nrf52(?:_2)?' using embedded signature",
)
APP_SEGMENT: re.Pattern[str] = re.compile(
    r"esp_image: segment \d+: paddr=0x([0-9a-f]+)\s+vaddr=0x[0-9a-f]+\s+size=0x([0-9a-f]+)",
    re.IGNORECASE,
)
ANSI_COLOR: re.Pattern[str] = re.compile(r"\x1b\[[0-9;]*m")
BOOT_PARTITION_ROW: re.Pattern[str] = re.compile(
    r"boot:\s*(\d+)\s+(\S+)\s+.+?\s+([0-9a-f]{2})\s+([0-9a-f]{2})\s+"
    r"([0-9a-f]{8})\s+([0-9a-f]{8})\s*$", re.IGNORECASE,
)


class PreparationDeclined(InvalidSetup):
    """The operator did not supply a valid firmware release."""


@dataclass(frozen=True)
class Scenario:
    number: int
    slot: int
    component: str
    method: str
    both: bool = False

    @property
    def name(self) -> str:
        slots: str = "both slots" if self.both else f"slot {self.slot}"
        return f"{self.number:02d} {slots} {self.component} {self.method}"


SCENARIOS: tuple[Scenario, ...] = (
    Scenario(1, 0, "app", "damage"),
    Scenario(2, 0, "Web-UI", "damage"),
    Scenario(3, 0, "nRF52", "damage"),
    Scenario(4, 0, "app", "bad signature"),
    Scenario(5, 1, "app", "damage"),
    Scenario(6, 1, "Web-UI", "damage"),
    Scenario(7, 1, "nRF52", "damage"),
    Scenario(8, 1, "app", "bad signature"),
    Scenario(9, 0, "app", "damage", True),
    Scenario(10, 0, "Web-UI", "damage", True),
    Scenario(11, 0, "nRF52", "damage", True),
    Scenario(12, 0, "app", "bad signature", True),
)


def scenario_plan(case: Scenario) -> str:
    selected: int = 0 if case.both else case.slot
    targets: str = "both slots" if case.both else f"ota_{selected}" if case.component == "app" else (
        f"{case.component} FATFS paired with ota_{selected}"
    )
    if case.component == "app" and case.method == "damage":
        change: str = "flip one app-body bit to trigger a bootloader checksum failure"
    elif case.component == "app":
        change = "change the app body, repair its checksum/hash, and leave an invalid RSA signature"
    else:
        change = f"flip one unused {case.component} FAT12 bit to invalidate its signature"
    expected: str = ("both images rejected and a reboot loop" if case.both else
                     f"rejection on UART followed by signed boot from ota_{1 - selected}")
    return f"active slot ota_{selected}; target {targets}; damage: {change}; expect {expected}"


def scenario_observation(case: Scenario, match: ConsoleMatch) -> str:
    if case.both:
        lockout: str = (f"signature rejection / rollback unavailable cycles={match.signature_lockout_cycles}, "
                        if case.component == "app" and case.method == "bad signature" else "")
        return (f"UART rejection={'yes' if match.detection else 'no'}, "
                f"{lockout}"
                f"reset banners={match.boot_banners}, gw_cfg init observed={'yes' if match.normal_boot else 'no'}")
    return (f"UART rejection={'yes' if match.detection else 'no'}, "
            f"ota_{1 - case.slot} loaded={'yes' if match.alternate_loaded else 'no'}, "
            f"ota_{1 - case.slot} running={'yes' if match.alternate_running else 'no'}, "
            f"signed rollback evidence={'yes' if match.rollback_or_lockout else 'no'}, "
            f"gw_cfg init observed={'yes' if match.normal_boot else 'no'}")


class FlashBoundary(Protocol):
    def identify(self) -> tuple[str, str]: ...
    def write(self, offset: int, data: bytes, path: Path) -> None: ...


class SerialBoundary(Protocol):
    def preflight(self) -> SerialVersions: ...
    def discover(self) -> SerialPort: ...
    def capture(
        self, port: SerialPort, evidence: EvidenceLog, duration: float,
        *, stop_when: Callable[[str], bool] | None = None,
    ) -> str: ...
    def observe(
        self, port: SerialPort, evidence: EvidenceLog, duration: float,
        *, stop_when: Callable[[str], bool] | None = None,
    ) -> str: ...


class FirmwareInstaller:
    def __init__(
        self, repository: Path, evidence: EvidenceLog,
        run_command: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
    ) -> None:
        self.repository: Path = repository
        self.evidence: EvidenceLog = evidence
        self.run_command: Callable[..., subprocess.CompletedProcess[str]] = run_command

    def _command(self, version: str, *options: str, timeout: float) -> None:
        command: tuple[str, ...] = (sys.executable, str(self.repository / "ruuvi_gw_flash.py"),
                                     *options, version)
        self.evidence.write("FIRMWARE INSTALL COMMAND", command)
        environment: dict[str, str] = os.environ.copy()
        environment["PATH"] = str(Path(sys.executable).parent) + os.pathsep + environment.get("PATH", "")
        try:
            completed: subprocess.CompletedProcess[str] = self.run_command(
                list(command), cwd=str(self.repository), capture_output=True, text=True,
                timeout=timeout, check=False, env=environment,
            )
        except (OSError, subprocess.SubprocessError) as error:
            error: OSError | subprocess.SubprocessError
            self.evidence.write("FIRMWARE INSTALL ERROR", f"{type(error).__name__}: {error}")
            raise InvalidSetup(f"firmware command failed: {error}") from error
        self.evidence.write("FIRMWARE INSTALL RESULT", {
            "return_code": completed.returncode, "stdout": completed.stdout, "stderr": completed.stderr,
        })
        if completed.returncode:
            lines: list[str] = (completed.stderr or completed.stdout).strip().splitlines()
            diagnostic: str = lines[-1] if lines else "no diagnostic"
            raise InvalidSetup(f"firmware command returned {completed.returncode}: {diagnostic}")

    def download(self, version: str) -> ReleaseImages:
        self._command(version, "--download_only", timeout=600.0)
        return load_release_images(self.repository, version)

    def flash_both(self, version: str, port: SerialPort) -> None:
        self._command(version, "--port", port.device, "--flash_both_slots", timeout=900.0)


class InstallerBoundary(Protocol):
    def download(self, version: str) -> ReleaseImages: ...
    def flash_both(self, version: str, port: SerialPort) -> None: ...


@dataclass(frozen=True)
class Corruption:
    partition: str
    sector_offset: int
    sector_length: int
    byte_offset: int
    original_byte: int
    changed_byte: int
    written_sha256: str


@dataclass(frozen=True)
class RestoreRegion:
    partition: str
    offset: int
    original: bytes


@dataclass(frozen=True)
class ConsoleMatch:
    scenario: str
    active_slot: str
    detection: bool
    rollback_or_lockout: bool
    normal_boot: bool
    boot_banners: int
    alternate_slot: str
    alternate_loaded: bool
    alternate_running: bool
    self_check_failure_observed: bool
    signature_lockout_cycles: int
    result: str


@dataclass(frozen=True)
class ReleaseImages:
    app: bytes
    web: bytes
    nrf: bytes
    ota_initial: bytes


def bad_signature_app(image: bytes) -> bytes:
    """Change an image segment, then repair its ESP checksum and simple SHA-256."""
    if len(image) < 0x2000 or image[0] != 0xE9 or image[23] != 1:
        raise InvalidSetup("selected app has no signed ESP image with an appended SHA-256")
    cursor: int = 24
    checksum: int = 0xEF
    mutation: int = 0x40000
    inside_segment: bool = False
    segment: int
    for segment in range(image[1]):
        if cursor + 8 > len(image):
            raise InvalidSetup("truncated ESP segment header")
        length: int = int.from_bytes(image[cursor + 4:cursor + 8], "little")
        start: int = cursor + 8
        end: int = start + length
        if end > len(image):
            raise InvalidSetup("truncated ESP segment data")
        if start <= mutation < end:
            inside_segment = True
        byte: int
        for byte in image[start:end]:
            checksum ^= byte
        cursor = end
    checksum_offset: int = (cursor + 16) // 16 * 16 - 1
    hash_offset: int = checksum_offset + 1
    signature_offset: int = (hash_offset + 32 + SECTOR_SIZE - 1) // SECTOR_SIZE * SECTOR_SIZE
    if not inside_segment or signature_offset + SECTOR_SIZE > len(image):
        raise InvalidSetup("selected app has no complete body or RSA signature block")
    if image[checksum_offset] != checksum or hashlib.sha256(image[:hash_offset]).digest() != image[hash_offset:hash_offset + 32]:
        raise InvalidSetup("selected app checksum or appended SHA-256 is invalid")
    if image[signature_offset] != 0xE7:
        raise InvalidSetup("selected app has no ESP secure-boot-v2 signature block")
    changed: bytearray = bytearray(image)
    changed[mutation] ^= 1
    changed[checksum_offset] ^= 1
    changed[hash_offset:hash_offset + 32] = hashlib.sha256(changed[:hash_offset]).digest()
    if changed[signature_offset:] != image[signature_offset:]:
        raise InvalidSetup("app signature block was unexpectedly modified")
    return bytes(changed)


def build_otadata(active_slot: int, initial: bytes) -> bytes:
    """Build two confirmed OTA entries; the higher sequence selects the active slot."""
    if active_slot not in (0, 1) or initial != b"\xff" * OTADATA_SIZE:
        raise InvalidSetup("release otadata must be a blank two-sector OTA partition")
    result: bytearray = bytearray(initial)
    sequences: tuple[int, int] = (3, 2) if active_slot == 0 else (1, 2)
    index: int
    sequence: int
    for index, sequence in enumerate(sequences):
        entry_offset: int = index * SECTOR_SIZE
        sequence_bytes: bytes = sequence.to_bytes(4, "little")
        result[entry_offset:entry_offset + 4] = sequence_bytes
        result[entry_offset + 24:entry_offset + 28] = (2).to_bytes(4, "little")
        result[entry_offset + 28:entry_offset + 32] = zlib.crc32(
            sequence_bytes, 0xFFFFFFFF,
        ).to_bytes(4, "little")
    generated: bytes = bytes(result)
    decoded: OtaSelection = decode_otadata(generated)
    if decoded.active_slot != f"ota_{active_slot}" or not all(
        entry.valid and entry.state == 2 for entry in decoded.entries
    ):
        raise InvalidSetup("generated otadata is not a valid two-slot selection")
    return generated


def fatfs_padding_offset(image: bytes) -> int:
    """Choose a byte outside the FAT12 cluster map in the second FAT copy."""
    if len(image) < SECTOR_SIZE or image[510:512] != b"\x55\xaa" or image[54:62] != b"FAT12   ":
        raise InvalidSetup("downloaded FATFS image has no FAT12 boot sector")
    sector_bytes: int = int.from_bytes(image[11:13], "little")
    cluster_sectors: int = image[13]
    reserved: int = int.from_bytes(image[14:16], "little")
    copies: int = image[16]
    root_entries: int = int.from_bytes(image[17:19], "little")
    total_sectors: int = int.from_bytes(image[19:21], "little") or int.from_bytes(image[32:36], "little")
    fat_sectors: int = int.from_bytes(image[22:24], "little")
    if (sector_bytes != 512 or cluster_sectors == 0 or reserved == 0 or copies != 2
            or fat_sectors == 0 or total_sectors * sector_bytes != len(image)):
        raise InvalidSetup("unsupported FAT12 image geometry")
    fat_bytes: int = fat_sectors * sector_bytes
    first: int = reserved * sector_bytes
    second: int = first + fat_bytes
    root_sectors: int = (root_entries * 32 + sector_bytes - 1) // sector_bytes
    data_sectors: int = total_sectors - reserved - copies * fat_sectors - root_sectors
    if data_sectors <= 0 or image[first:first + fat_bytes] != image[second:second + fat_bytes]:
        raise InvalidSetup("FAT12 image has inconsistent allocation tables")
    clusters: int = data_sectors // cluster_sectors
    used_fat_bytes: int = ((clusters + 2) * 12 + 7) // 8
    if clusters >= 4085 or fat_bytes - used_fat_bytes < 16:
        raise InvalidSetup("FAT12 image has no unused second-FAT padding")
    offset: int = second + fat_bytes - 1
    if image[offset] != 0 or any(image[second + used_fat_bytes:second + fat_bytes]):
        raise InvalidSetup("unused FAT12 padding is not empty")
    return offset


def load_release_images(repository: Path, version: str) -> ReleaseImages:
    def read(filename: str) -> bytes:
        path: Path = repository / ".releases" / version / filename
        try:
            return path.read_bytes()
        except OSError as error:
            error: OSError
            raise InvalidSetup(f"missing downloaded firmware image {path}: {error}") from error

    partitions: tuple[Partition, ...] = parse_partition_table(read("partition-table.bin"))
    if partitions != parse_partition_csv(repository / "partitions.csv"):
        raise InvalidSetup("downloaded partition table differs from partitions.csv")
    images: ReleaseImages = ReleaseImages(
        read("ruuvi_gateway_esp.bin"), read("fatfs_gwui.bin"), read("fatfs_nrf52.bin"),
        read("ota_data_initial.bin"),
    )
    sizes: dict[str, int] = {part.name: part.size for part in partitions}
    if (len(images.app) > sizes["ota_0"] or len(images.app) % SECTOR_SIZE
            or len(images.web) != sizes["fatfs_gwui"]
            or len(images.nrf) != sizes["fatfs_nrf52"]
            or len(images.ota_initial) != sizes["otadata"]):
        raise InvalidSetup("downloaded firmware image sizes do not match the partition table")
    bad_signature_app(images.app)
    fatfs_padding_offset(images.web)
    fatfs_padding_offset(images.nrf)
    build_otadata(0, images.ota_initial)
    return images


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def config_digest(payload: dict[str, Any]) -> str:
    encoded: bytes = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    return digest(encoded)


def initial_boot_captured(console: str) -> bool:
    clean: str = ANSI_COLOR.sub("", console)
    signature: re.Match[str] | None = SIGNED_NRF.search(clean)
    initialized: re.Match[str] | None = GATEWAY_INIT.search(clean, signature.end()) if signature else None
    return (initialized is not None and "\n" in clean[initialized.end():]
            and LOADED_APP.search(clean) is not None and RUNNING_APP.search(clean) is not None)


def parse_boot_partition_table(console: str) -> tuple[Partition, ...]:
    parts: list[Partition] = []
    started: bool = False
    finished: bool = False
    raw_line: str
    for raw_line in console.splitlines():
        line: str = ANSI_COLOR.sub("", raw_line)
        if "boot: Partition Table:" in line:
            if started:
                raise InvalidSetup("UART boot log contains multiple partition tables")
            started = True
            continue
        if not started:
            continue
        if "boot: End of partition table" in line:
            finished = True
            break
        row: re.Match[str] | None = BOOT_PARTITION_ROW.search(line)
        if row is None:
            if re.search(r"boot:\s*\d+\s+", line):
                raise InvalidSetup(f"malformed UART partition row: {line}")
            continue
        index: int = int(row.group(1))
        if index != len(parts):
            raise InvalidSetup("UART partition table has missing or duplicate row numbers")
        parts.append(Partition(row.group(2), int(row.group(3), 16), int(row.group(4), 16),
                               int(row.group(5), 16), int(row.group(6), 16)))
    if not started or not finished or not parts:
        raise InvalidSetup("UART boot log has no complete partition table")
    return tuple(parts)


def permitted_corruption(partition: Partition, offset: int, length: int) -> None:
    if partition.name not in SAFE_PARTITIONS or length <= 0:
        raise InvalidSetup(f"corruption is not permitted in {partition.name}")
    if offset < partition.offset or offset + length > partition.offset + partition.size:
        raise InvalidSetup(f"corruption falls outside {partition.name}")
    if any(offset < end and offset + length > start for start, end in FORBIDDEN):
        raise InvalidSetup(f"corruption overlaps forbidden flash range at 0x{offset:x}")
    if partition.name.startswith("ota_") and not (partition.offset + 0x1000 <= offset < partition.offset + partition.size - 0x1000):
        raise InvalidSetup("app corruption must stay in the image body")


def app_corruption_sector(partition: Partition, console: str) -> int:
    preferred: int = partition.offset + 0x40000
    match: re.Match[str]
    for match in APP_SEGMENT.finditer(ANSI_COLOR.sub("", console)):
        start: int = int(match.group(1), 16)
        end: int = start + int(match.group(2), 16)
        if start <= preferred and preferred + SECTOR_SIZE <= end:
            permitted_corruption(partition, preferred, SECTOR_SIZE)
            return preferred
    raise InvalidSetup(f"UART does not prove that {partition.name}+0x40000 is inside an app segment")


def app_signature_lockout_cycles(console: str, active: Partition) -> int:
    """Count separate boots rejecting the running app and every rollback candidate."""
    cycles: int = 0
    boots: list[str] = BOOT_BANNER.split(console)[1:]
    boot: str
    for boot in boots:
        running: re.Match[str] | None = RUNNING_APP.search(boot)
        if (running is None or running.group(1) != active.name
                or int(running.group(2), 16) != active.offset):
            continue
        failure: re.Match[str] | None = APP_SIGNATURE_FAILURE.search(boot, running.end())
        bad_signature: re.Match[str] | None = (APP_SIGNATURE_BAD.search(boot, failure.end())
                                               if failure is not None else None)
        if (bad_signature is not None and ROLLBACK_UNAVAILABLE in boot[bad_signature.end():]
                and APP_CHECKSUM_FAILURE.search(boot) is None):
            cycles += 1
    return cycles


def match_console(scenario: Scenario, console: str, active: Partition, alternate: Partition) -> ConsoleMatch:
    clean: str = ANSI_COLOR.sub("", console)
    # Serial chunks can end halfway through an unrelated line after all required evidence.
    # Only complete lines contribute evidence; a trailing fragment does not revoke it.
    clean = clean[:clean.rfind("\n") + 1]
    complete: bool = bool(clean)
    normal: bool = GATEWAY_INIT.search(clean) is not None
    unsafe_initialization: bool = False
    loaded_offset: int | None = None
    line: str
    for line in clean.splitlines():
        if BOOT_BANNER.search(line) is not None:
            loaded_offset = None
        loaded: re.Match[str] | None = LOADED_APP.search(line)
        running: re.Match[str] | None = RUNNING_APP.search(line)
        if loaded is not None:
            loaded_offset = int(loaded.group(1), 16)
        if running is not None:
            loaded_offset = int(running.group(2), 16)
            if running.group(1) != alternate.name:
                loaded_offset = active.offset
        if GATEWAY_INIT.search(line) is not None and (scenario.both or loaded_offset != alternate.offset):
            unsafe_initialization = True
    banners: int = len(BOOT_BANNER.findall(clean))
    if scenario.component == "app":
        pattern: re.Pattern[str] = (APP_CHECKSUM_FAILURE if scenario.method == "damage"
                                    else APP_SIGNATURE_FAILURE)
    else:
        label: str = "fatfs_gwui" if scenario.component == "Web-UI" else "fatfs_nrf52"
        suffix: str = "(?:_2)?" if scenario.both else ("" if scenario.slot == 0 else "_2")
        pattern = re.compile(
            rf"Failed to verify fatfs partition '{label}{suffix}' using embedded signature",
            re.IGNORECASE,
        )
    failures: list[re.Match[str]] = list(pattern.finditer(clean))
    detected: bool = bool(failures)
    lockout_cycles: int = 0
    if scenario.component == "app" and scenario.method == "bad signature":
        running_apps: list[re.Match[str]] = list(RUNNING_APP.finditer(clean))
        signature_failed_slots: set[str] = set()
        index: int
        running_app: re.Match[str]
        for index, running_app in enumerate(running_apps):
            end: int = running_apps[index + 1].start() if index + 1 < len(running_apps) else len(clean)
            boot_window: str = clean[running_app.end():end]
            if (APP_SIGNATURE_FAILURE.search(boot_window) is not None
                    and APP_SIGNATURE_BAD.search(boot_window) is not None
                    and APP_CHECKSUM_FAILURE.search(boot_window) is None):
                signature_failed_slots.add(running_app.group(1))
        expected_slots: set[str] = {active.name}
        detected = signature_failed_slots == expected_slots
        if scenario.both:
            # The rollback API verifies the alternate image without running it. With
            # both signatures invalid, the selected slot repeats its failed boot.
            lockout_cycles = app_signature_lockout_cycles(clean, active)
            detected = lockout_cycles > 0
    if scenario.component == "app" and scenario.method == "damage" and not scenario.both:
        unbootable: set[str] = {match.group(1) for match in APP_UNBOOTABLE.finditer(clean)}
        detected = detected and f"{scenario.slot}" in unbootable
    if scenario.both:
        if scenario.component == "app" and scenario.method == "damage":
            unbootable: set[str] = {match.group(1) for match in APP_UNBOOTABLE.finditer(clean)}
            detected = detected and unbootable == {"0", "1"}
        elif scenario.component != "app":
            suffixes: set[str] = {"_2" if "_2'" in line else "" for line in clean.splitlines()
                                  if pattern.search(line) is not None}
            detected = detected and suffixes == {"", "_2"}
        control: bool = detected and banners >= 2 and not normal
        if scenario.component == "app" and scenario.method == "bad signature":
            control = control and lockout_cycles >= 2
        alternate_loaded: bool = False
        alternate_running: bool = False
    else:
        failure: re.Match[str] | None = failures[0] if failures else None
        tail: str = clean[failure.end():] if failure is not None else ""
        alternate_loaded = any(int(match.group(1), 16) == alternate.offset
                               for match in LOADED_APP.finditer(tail))
        running_matches: list[re.Match[str]] = [
            match for match in RUNNING_APP.finditer(tail)
            if match.group(1) == alternate.name and int(match.group(2), 16) == alternate.offset
        ]
        alternate_running = bool(running_matches)
        next_reset: re.Match[str] | None = (BOOT_BANNER.search(tail, running_matches[0].end())
                                            if running_matches else None)
        verified_tail: str = (tail[running_matches[0].end():next_reset.start() if next_reset else len(tail)]
                              if running_matches else "")
        alternate_suffix: str = "" if alternate.name == "ota_0" else "_2"
        signatures: tuple[str, ...] = (
            "Signature of the running partition is valid.",
            f"Successfully verified fatfs partition 'fatfs_gwui{alternate_suffix}' using embedded signature",
            f"Successfully verified fatfs partition 'fatfs_nrf52{alternate_suffix}' using embedded signature",
        )
        cursor: int = 0
        line: str
        alternate_verified: bool = True
        for line in signatures:
            position: int = verified_tail.find(line, cursor)
            if position < 0:
                alternate_verified = False
                break
            cursor = position + len(line)
        alternate_verified = (alternate_verified and GATEWAY_INIT.search(verified_tail, cursor) is not None)
        control = detected and alternate_loaded and alternate_running and alternate_verified
        if scenario.component == "app" and scenario.method == "bad signature":
            control = (control and "Firmware rollback is possible, so try to do it" in tail
                       and "Rollback to previously worked partition. Restart." in tail)
        elif scenario.component != "app" and failure is not None:
            control = (control and "Firmware rollback is possible, so try to do it" in tail
                       and "Signature of the running partition is valid." in clean[:failure.start()])
    if complete and unsafe_initialization:
        verdict: str = "FAIL"
    elif complete and detected and control:
        verdict = "PASS"
    elif complete and normal and (scenario.both or not alternate_running):
        verdict = "FAIL"
    else:
        verdict = "ERROR"
    return ConsoleMatch(scenario.name, active.name, detected, control, normal, banners,
                        alternate.name, alternate_loaded, alternate_running,
                        APP_SELF_CHECK_FAILURE in clean, lockout_cycles, verdict)


class FunctionalTest_5_7_1_2_a:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog, artifact_dir: Path,
        serial: SerialBoundary,
        flash_factory: Callable[[SerialPort, SerialVersions, EvidenceLog], FlashBoundary] = FlashTool,
        session_factory: Callable[[], requests.Session] = requests.Session,
        progress: Callable[[str], None] | None = None,
        installer: InstallerBoundary | None = None,
        scenarios: tuple[Scenario, ...] = SCENARIOS,
        report: Callable[[str], None] | None = None,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.artifact_dir: Path = artifact_dir
        self.serial: SerialBoundary = serial
        self.flash_factory: Callable[[SerialPort, SerialVersions, EvidenceLog], FlashBoundary] = flash_factory
        self.gateway: GatewayClient = GatewayClient(config, evidence, session_factory=session_factory)
        self.progress: Callable[[str], None] = progress if progress is not None else lambda _: None
        self.report: Callable[[str], None] = report if report is not None else evidence.write_line
        self.scenarios: tuple[Scenario, ...] = scenarios
        self.outcomes: dict[str, str] = {case.name: "NOT RUN" for case in scenarios}
        self.failure_details: list[str] = []
        self.failed_numbers: list[int] = []
        self.active_case: Scenario | None = None
        self.reset_advice: bool = False
        self.flash: FlashBoundary | None = None
        self.port: SerialPort | None = None
        self.touched: bool = False
        self.session: requests.Session | None = None
        self.recovery_notice: str = RECOVERY_NOTICE
        self.repository: Path = Path(__file__).resolve().parents[3]
        self.installer: InstallerBoundary = installer if installer is not None else FirmwareInstaller(
            self.repository, evidence,
        )

    def _baseline(self) -> str:
        self.progress("Authenticating and validating factory-default identity")
        self.session = None
        auth: InteractiveAuthResult | None = None
        try:
            for attempt in range(30):
                try:
                    auth = self.gateway.authenticate_interactive(
                        ADMIN_USERNAME, self.config.gw_id,
                    )
                    break
                except GatewayConnectionError:
                    if attempt == 29:
                        raise
                    time.sleep(2)
        except GatewayAuthenticationModeError:
            self.reset_advice = True
            raise
        if auth is None:
            raise InvalidSetup("gateway authentication did not complete")
        if auth.challenge_response.status_code != HttpStatus.C_401_UNAUTHORIZED:
            raise InvalidSetup("GET /auth did not issue a challenge")
        if auth.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) != GatewayCfgLanAuthType.DEFAULT:
            self.reset_advice = True
            raise InvalidSetup("gateway authentication mode is not factory default")
        if auth.login_response.status_code != HttpStatus.C_200_OK:
            self.reset_advice = True
            raise InvalidSetup(f"default Admin login returned {auth.login_response.status_code}")
        self.session = auth.session
        first: dict[str, Any] = self._configuration()
        if GatewayCfgDesc.GW_MAC in first:
            mac: Any = first[GatewayCfgDesc.GW_MAC]
            if not isinstance(mac, str) or OCTETS_6_RE.fullmatch(mac) is None or mac.replace(":", "").upper() != self.config.gw_mac.replace(":", "").upper():
                raise InvalidSetup("authenticated gateway MAC does not match .env")
        expected: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        field: str
        value: Any
        for field, value in expected.items():
            actual: Any = first.get(field)
            if type(actual) is not type(value) or actual != value:
                self.reset_advice = GatewayCfgDesc.GW_MAC in first
                raise InvalidSetup(f"{field} is not factory default")
        self.progress("Confirming a stable full configuration baseline")
        second: dict[str, Any] = self._configuration()
        if config_digest(first) != config_digest(second):
            raise InvalidSetup("two authenticated configuration reads are unstable")
        self.evidence.write("BASELINE CONFIG SHA256", config_digest(first))
        return config_digest(first)

    def _configuration(self) -> dict[str, Any]:
        if self.session is None:
            raise InvalidSetup("no authenticated gateway session")
        response: requests.Response = self.gateway.request(self.session, HttpMethod.GET, GatewayApi.CONFIG)
        if response.status_code != HttpStatus.C_200_OK:
            raise InvalidSetup(f"authenticated GET /ruuvi.json returned {response.status_code}")
        return self.gateway.response_json(response, "GET /ruuvi.json", dict)

    def _preflight(self, partition_csv: Path) -> tuple[dict[str, Partition], Partition, Partition, str]:
        if self.port is None or self.flash is None:
            self._serial_setup()
        assert self.port is not None
        assert self.flash is not None
        self.progress("Rebooting gateway and capturing the partition table and active slot on UART")
        boot_console: str = self.serial.capture(self.port, self.evidence, CAPTURE_SECONDS,
                                                stop_when=initial_boot_captured)
        actual: tuple[Partition, ...] = parse_boot_partition_table(boot_console)
        expected: tuple[Partition, ...] = parse_partition_csv(partition_csv)
        self.evidence.write("UART PARTITION TABLE", actual)
        if actual != expected:
            raise InvalidSetup("UART partition table differs from partitions.csv")
        parts: dict[str, Partition] = {part.name: part for part in actual}
        running: set[tuple[str, int]] = {
            (match.group(1), int(match.group(2), 16)) for match in RUNNING_APP.finditer(boot_console)
        }
        if len(running) != 1:
            raise InvalidSetup("UART boot log does not identify one unambiguous running OTA slot")
        running_name: str
        running_offset: int
        running_name, running_offset = running.pop()
        self.evidence.write("UART ACTIVE APP SLOT", {"name": running_name, "offset": hex(running_offset)})
        if running_name not in ("ota_0", "ota_1") or running_offset != parts[running_name].offset:
            raise InvalidSetup("UART running OTA slot disagrees with the partition table")
        active: Partition = parts[running_name]
        alternate: Partition = parts["ota_1" if running_name == "ota_0" else "ota_0"]
        self._verify_initial_boot(active, boot_console)
        return parts, active, alternate, boot_console

    def _serial_setup(self) -> None:
        self.progress("Checking serial tools and discovering the CH340 port")
        versions: SerialVersions = self.serial.preflight()
        port: SerialPort = self.serial.discover()
        self.port = port
        self.evidence.write("SERIAL VERSIONS", versions)
        self.evidence.write("SERIAL PORT", port)
        self.flash = self.flash_factory(port, versions, self.evidence)
        self.progress("Identifying the ESP32 and flash size")
        if self.flash is None:
            raise InvalidSetup("flash tool was not initialized")
        flash: FlashBoundary = self.flash
        identity: tuple[str, str] = flash.identify()
        self.evidence.write("CHIP ID AND FLASH SIZE", identity)

    def _patch_sector(self, partition: Partition, image: bytes, byte_in_image: int) -> RestoreRegion:
        if self.flash is None:
            raise InvalidSetup("flash tool was not initialized")
        if byte_in_image < 0 or byte_in_image >= len(image):
            raise InvalidSetup(f"patch byte is outside {partition.name} image")
        sector_offset: int = partition.offset + byte_in_image // SECTOR_SIZE * SECTOR_SIZE
        permitted_corruption(partition, sector_offset, SECTOR_SIZE)
        original: bytes = image[sector_offset - partition.offset:sector_offset - partition.offset + SECTOR_SIZE]
        if len(original) != SECTOR_SIZE:
            raise InvalidSetup(f"incomplete patch sector for {partition.name}")
        changed: bytearray = bytearray(original)
        index: int = byte_in_image % SECTOR_SIZE
        changed[index] ^= 1
        byte_offset: int = partition.offset + byte_in_image
        record: Corruption = Corruption(partition.name, sector_offset, SECTOR_SIZE,
                                        byte_offset, original[index], changed[index], digest(changed))
        self.evidence.write("FLASH CORRUPTION", record)
        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self.touched = True  # A failed write can still have modified the target sector.
        self.flash.write(sector_offset, bytes(changed), self.artifact_dir / f"patch-{partition.name}.bin")
        return RestoreRegion(partition.name, sector_offset, original)

    def _write_otadata(self, partition: Partition, slot: int, initial: bytes) -> None:
        if self.flash is None:
            raise InvalidSetup("flash tool was not initialized")
        if (partition.name != "otadata" or partition.type_id != 1 or partition.subtype != 0
                or partition.offset != OTADATA_OFFSET or partition.size != OTADATA_SIZE):
            raise InvalidSetup("unexpected otadata partition layout")
        data: bytes = build_otadata(slot, initial)
        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self.evidence.write("OTA SELECTION WRITE", {
            "active_slot": f"ota_{slot}", "offset": hex(partition.offset), "sha256": digest(data),
        })
        self.touched = True
        self.flash.write(partition.offset, data, self.artifact_dir / f"otadata-ota_{slot}.bin")

    def _restore_regions(self, regions: tuple[RestoreRegion, ...]) -> None:
        if self.flash is None:
            raise InvalidSetup("flash tool was not initialized")
        region: RestoreRegion
        for region in regions:
            self.evidence.write("FLASH IMAGE RESTORE", {
                "partition": region.partition, "offset": hex(region.offset),
                "size": len(region.original), "sha256": digest(region.original),
            })
            self.touched = True
            self.flash.write(region.offset, region.original,
                             self.artifact_dir / f"restore-{region.partition}.bin")

    def _verify_initial_boot(self, active: Partition, console: str) -> None:
        if self.port is None:
            raise InvalidSetup("serial port was not discovered")
        self.progress("Confirming the selected app and signed data partitions on UART")
        suffix: str = "" if active.name == "ota_0" else "_2"
        expected: tuple[str, ...] = (
            f"### Currently running partition: {active.name}: address 0x{active.offset:08x}",
            "Signature of the running partition is valid.",
            f"Successfully verified fatfs partition 'fatfs_gwui{suffix}' using embedded signature",
            f"Successfully verified fatfs partition 'fatfs_nrf52{suffix}' using embedded signature",
            "### Init gw_cfg:",
        )
        loaded: bool = any(int(match.group(1), 16) == active.offset for match in LOADED_APP.finditer(console))
        passed: bool = initial_boot_captured(console) and loaded and all(text in console for text in expected)
        self.evidence.write("INITIAL BOOT CHECK", {
            "active_slot": active.name, "loaded": loaded, "required_lines": expected, "result": passed,
        })
        if not passed:
            raise InvalidSetup("initial UART boot does not prove the selected signed partition set is running")

    def _boot(self, scenario: Scenario, active: Partition, alternate: Partition) -> ConsoleMatch:
        if self.port is None:
            raise InvalidSetup("serial port was not discovered")
        self.evidence.write("RESET METHOD", "esptool default_reset / hard_reset, CH340 DTR/RTS")
        console: str = self.serial.capture(
            self.port, self.evidence, CAPTURE_SECONDS,
            stop_when=(None if scenario.both else
                       lambda captured: match_console(scenario, captured, active, alternate).result == "PASS"),
        )
        match: ConsoleMatch = match_console(scenario, console, active, alternate)
        self.evidence.write("BOOT MATCH", match)
        return match

    def _write_image(self, partition: Partition, data: bytes, purpose: str) -> None:
        if self.flash is None:
            raise InvalidSetup("flash tool was not initialized")
        if partition.name not in SAFE_PARTITIONS or not data or len(data) % SECTOR_SIZE or len(data) > partition.size:
            raise InvalidSetup(f"firmware image does not fit partition {partition.name}")
        if partition.name.startswith("fatfs_") and len(data) != partition.size:
            raise InvalidSetup(f"FATFS image size does not match partition {partition.name}")
        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self.evidence.write("FLASH IMAGE WRITE", {
            "purpose": purpose, "partition": partition.name, "size": len(data), "sha256": digest(data),
        })
        self.touched = True
        self.flash.write(partition.offset, data, self.artifact_dir / f"{purpose}-{partition.name}.bin")

    @staticmethod
    def _partition_for(parts: dict[str, Partition], component: str, slot: int) -> Partition:
        base: str = {"app": "ota", "Web-UI": "fatfs_gwui", "nRF52": "fatfs_nrf52"}[component]
        name: str = f"ota_{slot}" if component == "app" else base + ("" if slot == 0 else "_2")
        return parts[name]

    def _prepare_case(self, case: Scenario, parts: dict[str, Partition], images: ReleaseImages,
                      restores: tuple[RestoreRegion, ...]) -> str:
        self.progress(f"Preparing {case.name}")
        if self.port is None:
            raise InvalidSetup("serial port was not discovered")
        selected: int = 0 if case.both else case.slot
        self._restore_regions(restores)
        self._write_otadata(parts["otadata"], selected, images.ota_initial)
        console: str = self.serial.capture(self.port, self.evidence, CAPTURE_SECONDS,
                                           stop_when=initial_boot_captured)
        self._verify_initial_boot(parts[f"ota_{selected}"], console)
        return console

    def _inject_case(self, case: Scenario, parts: dict[str, Partition], images: ReleaseImages,
                     console: str) -> tuple[RestoreRegion, ...]:
        slots: tuple[int, ...] = (0, 1) if case.both else (case.slot,)
        restores: list[RestoreRegion] = []
        slot: int
        for slot in slots:
            target: Partition = self._partition_for(parts, case.component, slot)
            if case.method == "bad signature":
                if case.component != "app":
                    raise InvalidSetup("bad-signature image replacement applies only to OTA apps")
                payload: bytes = bad_signature_app(images.app)
                self._write_image(target, payload, f"bad-signature-{case.number}")
                restores.append(RestoreRegion(target.name, target.offset, images.app))
            else:
                if case.component == "app":
                    sector: int = app_corruption_sector(target, console) if slot == case.slot else (
                        target.offset + 0x40000
                    )
                    image: bytes = images.app
                    byte_in_image: int = sector - target.offset
                else:
                    image = images.web if case.component == "Web-UI" else images.nrf
                    byte_in_image = fatfs_padding_offset(image)
                restores.append(self._patch_sector(target, image, byte_in_image))
        return tuple(restores)

    def run(self, partition_csv: Path, version: str) -> RunResult:
        verdict: str = "ERROR"
        error: BaseException | None = None
        recovery_error: BaseException | None = None
        recovered: bool = False
        images: ReleaseImages | None = None
        baseline_digest: str | None = None
        try:
            if FIRMWARE_VERSION_RE.fullmatch(version) is None:
                raise InvalidSetup("firmware version must look like v1.17.5-dev")
            self.recovery_notice = RECOVERY_NOTICE.replace("<fw_version_tag>", version)
            self._serial_setup()
            self.progress(f"Downloading firmware images for {version}")
            downloaded: ReleaseImages = self.installer.download(version)
            images = downloaded
            assert self.port is not None
            self.progress(f"Installing {version} in both OTA slots")
            self.touched = True
            self.installer.flash_both(version, self.port)
            selected: int = 0 if self.scenarios[0].both else self.scenarios[0].slot
            self.progress(f"Writing two-slot OTA metadata with slot {selected} selected")
            csv_parts: dict[str, Partition] = {part.name: part for part in parse_partition_csv(partition_csv)}
            self._write_otadata(csv_parts["otadata"], selected, downloaded.ota_initial)
            parts: dict[str, Partition]
            active: Partition
            _alternate: Partition
            _boot_console: str
            parts, active, _alternate, _boot_console = self._preflight(partition_csv)
            if active.name != f"ota_{selected}":
                raise InvalidSetup(f"prepared OTA metadata did not boot slot {selected}")
            baseline_digest = self._baseline()
            restores: tuple[RestoreRegion, ...] = ()
            case: Scenario
            for case in self.scenarios:
                self.active_case = case
                self.report(f"Step {case.number:02d} begin: {scenario_plan(case)}")
                console: str = self._prepare_case(case, parts, downloaded, restores)
                self.progress(f"Injecting {case.name} and observing boot")
                restores = self._inject_case(case, parts, downloaded, console)
                selected: int = 0 if case.both else case.slot
                observation: ConsoleMatch = self._boot(
                    case, parts[f"ota_{selected}"], parts[f"ota_{1 - selected}"],
                )
                self.outcomes[case.name] = observation.result
                observed: str = scenario_observation(case, observation)
                if observation.result == "FAIL":
                    verdict = "FAIL"
                    self.failed_numbers.append(case.number)
                    detail: str = f"{case.name}: expected {scenario_plan(case)}; observed {observed}."
                    self.failure_details.append(detail)
                    self.evidence.write("SECURITY ASSERTION FAILURE", detail)
                    self.report(f"Step {case.number:02d} finished: FAIL — {observed}")
                elif observation.result != "PASS":
                    raise InvalidSetup(f"{case.name}: expected {scenario_plan(case)}; "
                                       f"UART boot evidence is incomplete; observed {observed}")
                else:
                    self.report(f"Step {case.number:02d} finished: OK — {observed}")
                self.active_case = None
            if verdict != "FAIL":
                verdict = "PASS"
        except Exception as caught_error:  # noqa: BLE001 - Record any failure after irreversible flash writes.
            caught_error: Exception
            error = caught_error
            verdict = "ERROR"
            self.evidence.exception(caught_error)
            if self.active_case is not None:
                self.outcomes[self.active_case.name] = "ERROR"
                self.report(f"Step {self.active_case.number:02d} finished: ERROR — {caught_error}")
        except KeyboardInterrupt as interrupted:
            interrupted: KeyboardInterrupt
            error = interrupted
            verdict = "ERROR"
            self.evidence.exception(interrupted)
            if self.active_case is not None:
                self.outcomes[self.active_case.name] = "ERROR"
                self.report(f"Step {self.active_case.number:02d} finished: ERROR — interrupted")
        finally:
            if self.touched and self.port is not None and images is not None:
                try:
                    self.progress(f"Recovering {version} in both OTA slots")
                    self.installer.flash_both(version, self.port)
                    csv_parts = {part.name: part for part in parse_partition_csv(partition_csv)}
                    self._write_otadata(csv_parts["otadata"], 0, images.ota_initial)
                    recovered_parts, recovered_active, _alternate, _console = self._preflight(partition_csv)
                    if recovered_active != recovered_parts["ota_0"]:
                        raise InvalidSetup("recovered gateway did not boot slot 0")
                    recovered_digest: str = self._baseline()
                    if baseline_digest is not None and recovered_digest != baseline_digest:
                        raise InvalidSetup("recovered full configuration differs from the pre-corruption baseline")
                    recovered = True
                    self.evidence.write("AUTOMATIC RECOVERY", "both slots reflashed; slot 0 signed boot and LAN baseline verified")
                except (Exception, KeyboardInterrupt) as caught_recovery_error:  # noqa: BLE001 - Final recovery must preserve evidence and report ERROR.
                    recovery_error = caught_recovery_error
                    verdict = "ERROR"
                    self.evidence.exception(caught_recovery_error)
                    self.evidence.write("AUTOMATIC RECOVERY", f"unverified: {caught_recovery_error}")
        recovery: str | None = FACTORY_RESET_MESSAGE if self.reset_advice else None
        if recovery is None and isinstance(error, InvalidSetup) and not self.touched:
            recovery = f"Setup error: {error}. See {self.evidence.path} for details."
        if self.touched:
            if error is not None:
                diagnostic: str = f"{type(error).__name__}: {error}. See {self.evidence.path} for details."
                recovery = f"{recovery}\n{diagnostic}" if recovery else diagnostic
            if self.failure_details:
                failures: str = "\n".join(self.failure_details)
                recovery = f"{failures}\n{recovery}" if recovery else failures
            if recovered:
                restored: str = "Gateway recovery verified: both firmware slots reflashed; slot 0 signed boot and LAN baseline passed."
                recovery = f"{recovery}\n{restored}" if recovery else restored
            else:
                if recovery_error is not None:
                    failure: str = f"Automatic recovery failed: {type(recovery_error).__name__}: {recovery_error}."
                    recovery = f"{recovery}\n{failure}" if recovery else failure
                recovery = f"{recovery}\n{self.recovery_notice}" if recovery else self.recovery_notice
                self.evidence.write("MANUAL RECOVERY REQUIRED", self.recovery_notice)
        retry_numbers: set[int] = set(self.failed_numbers)
        if error is not None and self.active_case is not None:
            retry_numbers.add(self.active_case.number)
        if error is not None and len(self.scenarios) == 1:
            retry_numbers.add(self.scenarios[0].number)
        if retry_numbers:
            commands: str = "\n".join(
                f"Retry case {number}: python test_5_7_1_2_a.py --case {number}"
                for number in sorted(retry_numbers)
            )
            recovery = f"{recovery}\n{commands}" if recovery else commands
        if error is not None:
            self.evidence.write("TERMINAL REASON", f"{type(error).__name__}: {error}")
        if recovery_error is not None:
            self.evidence.write("TERMINAL RECOVERY REASON", f"{type(recovery_error).__name__}: {recovery_error}")
        name: str
        result: str
        for name, result in self.outcomes.items():
            self.evidence.write("FINAL RESULT", {"scenario": name, "result": result})
        self.evidence.write("OVERALL RESULT", verdict)
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(), recovery)


def execute_test_5_7_1_2_a(
    work_dir: Path | None = None,
    session_factory: Callable[[], requests.Session] = requests.Session,
    serial: SerialBoundary | None = None,
    flash_factory: Callable[[SerialPort, SerialVersions, EvidenceLog], FlashBoundary] = FlashTool,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] | None = None,
    input_fn: Callable[[str], str] = input,
    installer: InstallerBoundary | None = None,
    case_number: int | None = None,
) -> RunResult:
    scenarios: tuple[Scenario, ...] = (
        SCENARIOS if case_number is None else tuple(case for case in SCENARIOS if case.number == case_number)
    )
    if not scenarios:
        raise ValueError("case number must be from 1 to 12")
    if work_dir is None:
        work_dir = Path.cwd()
    if output is None:
        output = print
    evidence: EvidenceLog = EvidenceLog.create(work_dir / "logs", PREFIX, now)
    output(f"Open log file: {evidence.path}")

    def report(message: str) -> None:
        output(message)
        evidence.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, BASE_STEPS + len(scenarios) * 3)
    result: RunResult = RunResult(2, "ERROR", {case.name: "NOT RUN" for case in scenarios}, set())
    evidence.write("TEST CASE AND UNIT", TEST_ID)
    evidence.write("UTC START", format_utc(evidence.started_at))
    evidence.write("SIGNATURE VERIFICATION PATHS", (
        "ESP-IDF checks signed apps on boot and in the application; firmware verifies FATFS "
        "against embedded RSA signatures. ROM secure-boot eFuses are not burned."
    ))
    artifact_dir: Path = work_dir / "logs" / "corruptions" / evidence.path.stem
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(work_dir / ".env")
        default_version: str = config.gw_fw or DEFAULT_FW_VERSION
        output(PREPARATION_NOTICE)
        evidence.write_line(PREPARATION_NOTICE)
        try:
            answer: str = input_fn(f"Firmware version [{default_version}] (Enter for default): ").strip()
        except EOFError as cause:
            cause: EOFError
            raise PreparationDeclined("no firmware version was supplied") from cause
        version: str = answer or default_version
        if FIRMWARE_VERSION_RE.fullmatch(version) is None:
            raise PreparationDeclined("firmware version must look like v1.17.5-dev")
        evidence.write("SELECTED FIRMWARE VERSION", version)
        runner: FunctionalTest_5_7_1_2_a = FunctionalTest_5_7_1_2_a(
            config, evidence, artifact_dir, serial or SerialTransport(), flash_factory,
            session_factory, progress.step, installer, scenarios, report,
        )
        result = runner.run(Path(__file__).resolve().parents[3] / "partitions.csv", version)
    except PreparationDeclined as error:
        error: PreparationDeclined
        evidence.exception(error)
        result = RunResult(2, "ERROR", {case.name: "NOT RUN" for case in scenarios}, set(),
                           f"Test not started: {error}.")
        evidence.write("OVERALL RESULT", "ERROR")
    except Exception as initialization_error:  # noqa: BLE001 - Initialization errors still produce finalized evidence.
        initialization_error: Exception
        evidence.exception(initialization_error)
        result = RunResult(2, "ERROR", {case.name: "NOT RUN" for case in scenarios}, set(),
                           f"Setup error: {initialization_error}. See {evidence.path} for details.")
        evidence.write("OVERALL RESULT", "ERROR")
    finally:
        if result.recovery_message is not None:
            evidence.write("TERMINAL DETAILS", result.recovery_message)
        evidence.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    if result.recovery_message is not None:
        output(result.recovery_message)
    return result


def main(argv: Sequence[str] | None = None) -> int:
    damage_descriptions: dict[tuple[str, str], str] = {
        ("app", "damage"): "app checksum failure (one-bit damage)",
        ("app", "bad signature"): "app signature failure (checksum/hash repaired)",
        ("Web-UI", "damage"): "Web-UI FATFS signature failure (one-bit damage)",
        ("nRF52", "damage"): "nRF52 FATFS signature failure (one-bit damage)",
    }
    case_descriptions: str = "\n".join(
        f"  {case.number:2d}: {'Both slots' if case.both else f'Active slot {case.slot}'}: "
        f"{damage_descriptions[(case.component, case.method)]}\n"
        "      Expected: " + ("reboot loop; no gateway initialization" if case.both else
                              f"signed boot and gateway initialization from slot {1 - case.slot}")
        for case in SCENARIOS
    )
    parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description=TEST_ID,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            f"Test cases (omit --case to run all {len(SCENARIOS)}):\n{case_descriptions}\n\n"
            "FATFS damage changes an unused FAT12 bit to invalidate the signature.\n"
            "Both-slot cases start with slot 0 selected and damage both paired images.\n\n"
            "The test prompts for a firmware release, installs it in both slots,\n"
            "then reflashes and verifies recovery after testing, including failures.\n\n"
            "Examples:\n"
            "  python test_5_7_1_2_a.py --case 10  # Both Web-UI partitions invalid\n"
            "  python test_5_7_1_2_a.py --case 11  # Both nRF52 partitions invalid"
        ),
    )
    parser.add_argument("--case", type=int, choices=range(1, len(SCENARIOS) + 1),
                        help="run only this integrity case (default: run all cases)")
    arguments: argparse.Namespace = parser.parse_args(argv)
    return execute_test_5_7_1_2_a(case_number=arguments.case).exit_code


if __name__ == "__main__":
    sys.exit(main())
