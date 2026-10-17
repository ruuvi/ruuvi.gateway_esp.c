"""Offline checks for the destructive 5.7-1-2 Unit A runner."""

from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable
from unittest import mock

import test_5_7_1_2_a as target
from lib.config import FACTORY_RESET_MESSAGE
from lib.errors import InvalidSetup
from lib.evidence import EvidenceLog
from lib.flash_dut import OTADATA_OFFSET, OTADATA_SIZE, OtaSelection, Partition, decode_otadata
from lib.gateway import GatewayCfgLanAuthType
from lib.models import DutConfig, RunResult
from lib.serial_dut import SerialPort, SerialVersions
from test_support.fake_gateway import DefaultAuthGateway, FakeResponse, FakeSession, env_text

NOW: datetime = datetime(2026, 9, 26, 7, 0, tzinfo=timezone.utc)
CONFIG: DutConfig = DutConfig("00:11:22:33:44:55:66:77", "AA:BB:CC:DD:EE:FF", "gateway.local")
LAYOUT: tuple[tuple[str, int, int, int, int], ...] = (
    ("nvs", 1, 2, 0x9000, 0x4000), ("otadata", 1, 0, 0xD000, 0x2000),
    ("phy_init", 1, 1, 0xF000, 0x1000), ("ota_0", 0, 0x10, 0x100000, 0x400000),
    ("fatfs_gwui", 1, 0x81, 0x500000, 0xC0000),
    ("fatfs_nrf52", 1, 0x81, 0x5C0000, 0x40000),
    ("ota_1", 0, 0x11, 0x600000, 0x400000),
    ("fatfs_gwui_2", 1, 0x81, 0xA00000, 0xC0000),
    ("fatfs_nrf52_2", 1, 0x81, 0xAC0000, 0x40000),
    ("gw_cfg_def", 1, 2, 0xB00000, 0x40000),
)
BLANK_OTADATA: bytes = b"\xff" * OTADATA_SIZE


def signed_image() -> bytes:
    header: bytearray = bytearray(24)
    header[0] = 0xE9
    header[1] = 1
    header[23] = 1
    segment: bytes = b"\x00" * 4 + (0x41000).to_bytes(4, "little")
    body: bytes = bytes(header) + segment + b"\x55" * 0x41000
    checksum_offset: int = (len(body) + 16) // 16 * 16 - 1
    image: bytearray = bytearray(body + b"\x00" * (checksum_offset - len(body)) + b"\xef")
    image.extend(hashlib.sha256(image).digest())
    image.extend(b"\xff" * (-len(image) % target.SECTOR_SIZE))
    image.extend(b"\xe7" + b"\xff" * (target.SECTOR_SIZE - 1))
    return bytes(image)


def fat12_image(size: int, fat_sectors: int, root_entries: int) -> bytes:
    result: bytearray = bytearray(b"\x00" * size)
    result[:3] = b"\xeb\x3c\x90"
    result[11:13] = (512).to_bytes(2, "little")
    result[13] = 1
    result[14:16] = (1).to_bytes(2, "little")
    result[16] = 2
    result[17:19] = root_entries.to_bytes(2, "little")
    result[19:21] = (size // 512).to_bytes(2, "little")
    result[22:24] = fat_sectors.to_bytes(2, "little")
    result[54:62] = b"FAT12   "
    result[510:512] = b"\x55\xaa"
    first: int = 512
    second: int = first + fat_sectors * 512
    result[first:first + 3] = b"\xf8\xff\xff"
    result[second:second + 3] = b"\xf8\xff\xff"
    return bytes(result)


APP: bytes = signed_image()
WEB: bytes = fat12_image(0xC0000, 5, 224)
NRF: bytes = fat12_image(0x40000, 2, 112)
IMAGES: target.ReleaseImages = target.ReleaseImages(APP, WEB, NRF, BLANK_OTADATA)


def baseline(slot: int, state: str = "VALID") -> str:
    lines: list[str] = ["I (59) boot: Partition Table:\n"]
    index: int
    name: str
    type_id: int
    subtype: int
    offset: int
    size: int
    for index, (name, type_id, subtype, offset, size) in enumerate(LAYOUT):
        lines.append(f"I (70) boot:  {index} {name:<16} Unknown data "
                     f"{type_id:02x} {subtype:02x} {offset:08x} {size:08x}\n")
    lines.append("I (145) boot: End of partition table\n")
    app_offset: int = 0x100000 if slot == 0 else 0x600000
    suffix: str = "" if slot == 0 else "_2"
    lines.extend((
        (f"I (149) esp_image: segment 0: paddr=0x{app_offset + 0x20:08x} "
         "vaddr=0x3f400020 size=0x8bd50 (572752) map\n"),
        f"I (973) boot: Loaded app from partition at offset 0x{app_offset:x}\n",
        (f"I (1117) fw_update: ### Currently running partition: ota_{slot}: "
         f"address 0x{app_offset:08x}, size 0x400000\n"),
        f"I (1195) fw_update: Currently running partition state: {state}\n",
        "I (2676) fw_update: Signature of the running partition is valid.\n",
        (f"I (3333) fw_update: Successfully verified fatfs partition 'fatfs_gwui{suffix}' "
         "using embedded signature\n"),
        (f"I (3665) fw_update: Successfully verified fatfs partition 'fatfs_nrf52{suffix}' "
         "using embedded signature\n"),
        "I (3927) ruuvi_gateway: [main/1] ### Init gw_cfg: device_id=00:00:00:00:00:00:00:00\n",
        "Start mDNS: Hostname: RuuviGatewayEEFF\n",
    ))
    return "".join(lines)


def observation(case: target.Scenario) -> str:
    if case.component == "app" and case.method == "bad signature":
        def signature_error(slot: int) -> str:
            app_address: int = 0x100000 if slot == 0 else 0x600000
            return ("rst:0x1 (POWERON_RESET)\n"
                    + f"I (973) boot: Loaded app from partition at offset 0x{app_address:x}\n"
                    + f"I (1117) fw_update: ### Currently running partition: ota_{slot}: "
                    f"address 0x{app_address:08x}, size 0x400000\n"
                    + "E (2674) esp_image: Secure boot signature verification failed\n"
                    + "W (3886) esp_image: image valid, signature bad\n"
                    + "E (3896) fw_update: Self check of running partition signature failed\n"
                    + "Firmware rollback is possible, so try to do it\n"
                    + "Rollback to previously worked partition. Restart.\n")
        if case.both:
            # The alternate app is checked by the rollback API without being run.
            cycle: str = signature_error(0).replace(
                "Firmware rollback is possible, so try to do it\n"
                "Rollback to previously worked partition. Restart.\n",
                "E (5329) esp_image: Secure boot signature verification failed\n"
                "W (6542) esp_image: image valid, signature bad\n"
                "E (6543) ruuvi_gateway: Firmware rollback is not possible\n"
                "E (9160) esp_ota_ops: Rollback is not possible, do not have any suitable apps in slots\n"
                "I (9178) ruuvi_gateway: Restarting system in 5 seconds...\n",
            )
            return cycle + cycle.replace("rst:0x1 (POWERON_RESET)", "rst:0xc (SW_CPU_RESET)")
        errors: str = signature_error(case.slot)
    elif case.component == "app":
        failure: str = "E (957) esp_image: Checksum failed. Calculated 0x29 read 0x92\n"
        def app_error(slot: int) -> str:
            return failure + f"E (957) boot: OTA app partition slot {slot} is not bootable\n"
        if case.both:
            return ("rst:0x1 (POWERON_RESET)\n" + app_error(0) + app_error(1)
                    + "rst:0xc (SW_CPU_RESET)\n" + app_error(0) + app_error(1))
        errors: str = app_error(case.slot)
    else:
        label: str = "fatfs_gwui" if case.component == "Web-UI" else "fatfs_nrf52"
        def fatfs_error(slot: int) -> str:
            fatfs_suffix: str = "" if slot == 0 else "_2"
            return (f"E (3333) fw_update: Failed to verify fatfs partition '{label}{fatfs_suffix}' "
                    "using embedded signature\n")
        if case.both:
            return ("rst:0x1 (POWERON_RESET)\n" + fatfs_error(0)
                    + "rst:0xc (SW_CPU_RESET)\n" + fatfs_error(1))
        errors = ("Signature of the running partition is valid.\n" + fatfs_error(case.slot)
                  + "Firmware rollback is possible, so try to do it\n")
    other: int = 1 - case.slot
    address: int = 0x100000 if other == 0 else 0x600000
    suffix: str = "" if other == 0 else "_2"
    return ("rst:0x1 (POWERON_RESET)\n" + errors + "rst:0xc (SW_CPU_RESET)\n"
            + f"I (973) boot: Loaded app from partition at offset 0x{address:x}\n"
            + f"I (1117) fw_update: ### Currently running partition: ota_{other}: "
            f"address 0x{address:08x}, size 0x400000\n"
            + "Currently running partition state: UNDEFINED\n"
            + "Signature of the running partition is valid.\n"
            + f"Successfully verified fatfs partition 'fatfs_gwui{suffix}' using embedded signature\n"
            + f"Successfully verified fatfs partition 'fatfs_nrf52{suffix}' using embedded signature\n"
            + "I (3927) ruuvi_gateway: [main/1] ### Init gw_cfg: device_id=00:00:00:00:00:00:00:00\n"
            + "Start mDNS: Hostname: RuuviGatewayEEFF\n")


class FakeInstaller:
    def __init__(self) -> None:
        self.calls: list[tuple[str, str]] = []
        self.fail_on_flash_call: int | None = None

    def download(self, version: str) -> target.ReleaseImages:
        self.calls.append(("download", version))
        return IMAGES

    def flash_both(self, version: str, port: SerialPort) -> None:
        self.calls.append(("flash_both", version))
        if port.device != "/dev/ttyUSB0":
            raise AssertionError("wrong serial port")
        if self.fail_on_flash_call == sum(action == "flash_both" for action, _version in self.calls):
            raise InvalidSetup("injected recovery flash failure")


class FakeFlash:
    def __init__(self) -> None:
        self.writes: list[tuple[int, bytes]] = []
        self.fail_at: int | None = None

    def identify(self) -> tuple[str, str]:
        return "Chip ID: 1234", "Detected flash size: 16MB"

    def write(self, offset: int, data: bytes, path: Path) -> None:
        if self.fail_at == offset:
            raise InvalidSetup("injected flash write failure")
        self.writes.append((offset, data))


class FakeSerial:
    def __init__(self, captures: tuple[str, ...]) -> None:
        self.captures: list[str | tuple[str, ...]] = list(captures)
        self.calls: int = 0

    def preflight(self) -> SerialVersions:
        return SerialVersions("4.8.1", "3.5")

    def discover(self) -> SerialPort:
        return SerialPort("/dev/ttyUSB0", 0x1A86, 0x7523)

    def capture(
        self, port: SerialPort, evidence: EvidenceLog, duration: float,
        *, stop_when: Callable[[str], bool] | None = None,
    ) -> str:
        self.calls += 1
        if port.device != "/dev/ttyUSB0" or duration != target.CAPTURE_SECONDS:
            raise AssertionError("wrong capture parameters")
        if not self.captures:
            raise InvalidSetup("no more fake UART captures")
        capture: str | tuple[str, ...] = self.captures.pop(0)
        chunks: tuple[str, ...] = (capture,) if isinstance(capture, str) else capture
        result: str = ""
        chunk: str
        for chunk in chunks:
            result += chunk
            if stop_when is not None and stop_when(result):
                break
        evidence.write("RAW BOOT CONSOLE", result)
        return result

    def observe(
        self, port: SerialPort, evidence: EvidenceLog, duration: float,
        *, stop_when: Callable[[str], bool] | None = None,
    ) -> str:
        raise AssertionError("HTTP update and passive UART observation are not used")


class RunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work_dir: Path = Path(self.temporary.name)
        (self.work_dir / ".env").write_text(env_text(CONFIG), encoding="utf-8")
        self.gateway: DefaultAuthGateway = DefaultAuthGateway(CONFIG)
        self.flash: FakeFlash = FakeFlash()
        self.installer: FakeInstaller = FakeInstaller()
        captures: list[str] = [baseline(0)]
        case: target.Scenario
        for case in target.SCENARIOS:
            captures.append(baseline(0 if case.both else case.slot))
            captures.append(observation(case))
        captures.append(baseline(0))
        self.serial: FakeSerial = FakeSerial(tuple(captures))
        self.output: list[str] = []
        self.prompts: list[str] = []

    def execute(self, answer: str = "v1.17.5-dev", case_number: int | None = None) -> RunResult:
        factory: Callable[[SerialPort, SerialVersions, EvidenceLog], FakeFlash] = (
            lambda _port, _versions, _evidence: self.flash
        )
        def answer_prompt(prompt: str) -> str:
            self.prompts.append(prompt)
            return answer
        return target.execute_test_5_7_1_2_a(
            work_dir=self.work_dir, session_factory=lambda: FakeSession(self.gateway),
            serial=self.serial, flash_factory=factory, now=lambda: NOW,
            output=self.output.append, input_fn=answer_prompt,
            installer=self.installer, case_number=case_number,
        )

    def test_enter_uses_firmware_version_from_env(self) -> None:
        env_path: Path = self.work_dir / ".env"
        env_path.write_text(env_path.read_text(encoding="utf-8") + "gw_fw=v1.17.5-dev\n",
                            encoding="utf-8")
        result: RunResult = self.execute("", case_number=1)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual(["Firmware version [v1.17.5-dev] (Enter for default): "], self.prompts)
        self.assertEqual(("download", "v1.17.5-dev"), self.installer.calls[0])

    def test_enter_uses_hardcoded_default_without_env_version(self) -> None:
        result: RunResult = self.execute("", case_number=1)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual(["Firmware version [v1.17.5-prod] (Enter for default): "], self.prompts)
        self.assertEqual(("download", "v1.17.5-prod"), self.installer.calls[0])

    def test_enter_uses_hardcoded_default_for_blank_env_version(self) -> None:
        env_path: Path = self.work_dir / ".env"
        env_path.write_text(env_path.read_text(encoding="utf-8") + "gw_fw=\n", encoding="utf-8")
        result: RunResult = self.execute("", case_number=1)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual(["Firmware version [v1.17.5-prod] (Enter for default): "], self.prompts)
        self.assertEqual(("download", "v1.17.5-prod"), self.installer.calls[0])

    def test_explicit_version_overrides_env_default(self) -> None:
        env_path: Path = self.work_dir / ".env"
        env_path.write_text(env_path.read_text(encoding="utf-8") + "gw_fw=v1.17.5-prod\n",
                            encoding="utf-8")
        result: RunResult = self.execute("v1.17.5-dev", case_number=1)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual(["Firmware version [v1.17.5-prod] (Enter for default): "], self.prompts)
        self.assertEqual(("download", "v1.17.5-dev"), self.installer.calls[0])

    def test_all_twelve_cases_restore_exact_regions_and_write_otadata(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual(12, len(result.outcomes))
        self.assertEqual({"PASS"}, set(result.outcomes.values()))
        self.assertEqual([("download", "v1.17.5-dev"), ("flash_both", "v1.17.5-dev"),
                          ("flash_both", "v1.17.5-dev")],
                         self.installer.calls)
        self.assertEqual([0xD000, 0xD000, 0x140000, 0x140000, 0xD000, 0x501000],
                         [offset for offset, _data in self.flash.writes[:6]])
        ota_writes: list[bytes] = [data for offset, data in self.flash.writes if offset == OTADATA_OFFSET]
        self.assertEqual(14, len(ota_writes))
        self.assertEqual([0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0],
                         [int(decode_otadata(data).active_slot[-1]) for data in ota_writes])
        self.assertTrue(any(offset == 0x600000 and data == target.bad_signature_app(APP)
                            for offset, data in self.flash.writes))
        self.assertIn("Gateway recovery verified", result.recovery_message or "")
        self.assertTrue(any(line.startswith("[Step 51 out of 51]") for line in self.output))
        beginnings: list[str] = [line for line in self.output if line.startswith("Step ")
                                 and " begin:" in line]
        endings: list[str] = [line for line in self.output if line.startswith("Step ")
                              and " finished:" in line]
        self.assertEqual(12, len(beginnings))
        self.assertEqual(12, len(endings))
        self.assertTrue(any(line.startswith("Step 08 begin: active slot ota_1;")
                            and "invalid RSA signature" in line for line in beginnings))
        self.assertTrue(any(line.startswith("Step 12 begin: active slot ota_0; target both slots;")
                            for line in beginnings))
        self.assertTrue(all("finished: OK" in line for line in endings))

    def test_initial_flash_precedes_lan_baseline(self) -> None:
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        install_step: int = next(index for index, line in enumerate(self.output)
                                 if "Installing v1.17.5-dev in both OTA slots" in line)
        baseline_step: int = next(index for index, line in enumerate(self.output)
                                  if "Authenticating and validating factory-default identity" in line)
        self.assertLess(install_step, baseline_step)

    def test_otadata_sequences_crc_and_unchanged_padding(self) -> None:
        slot: int
        for slot in (0, 1):
            generated: bytes = target.build_otadata(slot, BLANK_OTADATA)
            selected: OtaSelection = decode_otadata(generated)
            self.assertEqual(f"ota_{slot}", selected.active_slot)
            self.assertEqual((3, 2) if slot == 0 else (1, 2),
                             tuple(entry.sequence for entry in selected.entries))
            self.assertEqual((2, 2), tuple(entry.state for entry in selected.entries))
            self.assertTrue(all(entry.valid for entry in selected.entries))
            self.assertEqual((0xED4A5011, 0x55F63774) if slot == 0
                             else (0x4743989A, 0x55F63774),
                             tuple(entry.crc for entry in selected.entries))
            self.assertEqual(b"\xff" * (target.SECTOR_SIZE - 32), generated[32:target.SECTOR_SIZE])
        with self.assertRaisesRegex(InvalidSetup, "blank"):
            target.build_otadata(1, b"\x00" * OTADATA_SIZE)

    def test_damage_changes_exactly_one_bit_and_restores_original_sector(self) -> None:
        result: RunResult = self.execute()
        self.assertEqual(0, result.exit_code, result.recovery_message)
        app_sector: bytes = APP[0x40000:0x41000]
        writes: list[bytes] = [data for offset, data in self.flash.writes if offset == 0x140000]
        self.assertGreaterEqual(len(writes), 2)
        self.assertEqual(1, sum(bin(left ^ right).count("1")
                                for left, right in zip(app_sector, writes[0])))
        self.assertEqual(app_sector, writes[1])
        web_byte: int = target.fatfs_padding_offset(WEB)
        web_sector_offset: int = 0x500000 + web_byte // target.SECTOR_SIZE * target.SECTOR_SIZE
        web_original: bytes = WEB[web_byte // target.SECTOR_SIZE * target.SECTOR_SIZE:
                                  (web_byte // target.SECTOR_SIZE + 1) * target.SECTOR_SIZE]
        web_writes: list[bytes] = [data for offset, data in self.flash.writes if offset == web_sector_offset]
        self.assertEqual(1, sum(bin(left ^ right).count("1")
                                for left, right in zip(web_original, web_writes[0])))
        self.assertEqual(web_original, web_writes[1])

    def test_fat12_mutation_is_in_unused_second_fat_padding(self) -> None:
        image: bytes
        for image in (WEB, NRF):
            offset: int = target.fatfs_padding_offset(image)
            fat_size: int = int.from_bytes(image[22:24], "little") * 512
            self.assertGreaterEqual(offset, 512 + fat_size)
            self.assertLess(offset, 512 + 2 * fat_size)
            self.assertEqual(0, image[offset])
        broken: bytearray = bytearray(WEB)
        broken[510] = 0
        with self.assertRaisesRegex(InvalidSetup, "FAT12 boot sector"):
            target.fatfs_padding_offset(bytes(broken))

    def test_bad_signature_app_repairs_checksum_and_hash(self) -> None:
        changed: bytes = target.bad_signature_app(APP)
        self.assertEqual(APP[-target.SECTOR_SIZE:], changed[-target.SECTOR_SIZE:])
        self.assertEqual(0xEE, changed[0x4102F])
        self.assertEqual(hashlib.sha256(changed[:0x41030]).digest(), changed[0x41030:0x41050])

    def test_running_partition_is_authoritative_when_ota_state_is_undefined(self) -> None:
        case: target.Scenario = target.SCENARIOS[0]
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        match: target.ConsoleMatch = target.match_console(case, observation(case), active, alternate)
        self.assertEqual("PASS", match.result)
        self.assertEqual("ota_1", match.alternate_slot)

    def test_app_signature_rejection_after_boot_counts_as_detection(self) -> None:
        case: target.Scenario
        for case in (target.SCENARIOS[3], target.SCENARIOS[7], target.SCENARIOS[11]):
            console: str = observation(case)
            self.assertNotIn("is not bootable", console)
            active: Partition = Partition(f"ota_{case.slot}", 0, 0x10 + case.slot,
                                          0x100000 if case.slot == 0 else 0x600000, 0x400000)
            alternate: Partition = Partition(f"ota_{1 - case.slot}", 0, 0x11 - case.slot,
                                             0x600000 if case.slot == 0 else 0x100000, 0x400000)
            self.assertEqual("PASS", target.match_console(case, console, active, alternate).result)

    def test_signature_case_requires_rejection_rollback_signatures_and_gateway_init(self) -> None:
        case: target.Scenario = target.SCENARIOS[3]
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        console: str = observation(case)
        self.assertEqual("PASS", target.match_console(case, console, active, alternate).result)
        required: str
        for required in ("Secure boot signature verification failed", "image valid, signature bad",
                         "Firmware rollback is possible, so try to do it",
                         "Rollback to previously worked partition. Restart.",
                         "Signature of the running partition is valid.",
                         "Successfully verified fatfs partition 'fatfs_gwui_2' using embedded signature",
                         "Successfully verified fatfs partition 'fatfs_nrf52_2' using embedded signature",
                         "### Init gw_cfg:"):
            with self.subTest(missing=required):
                changed: str = console.replace(required, "missing marker")
                self.assertNotEqual("PASS", target.match_console(case, changed, active, alternate).result)

    def test_both_bad_signatures_repeat_selected_slot_when_rollback_is_unavailable(self) -> None:
        case: target.Scenario = target.SCENARIOS[11]
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        console: str = observation(case)
        self.assertNotIn("Currently running partition: ota_1", console)
        # Preserve ANSI and an unfinished later chunk, as in the reported capture.
        console = "\x1b[0;31m" + console.replace("\n", "\x1b[0m\r\n")
        match: target.ConsoleMatch = target.match_console(case, console + "I (1113) fw_up", active, alternate)
        self.assertEqual("PASS", match.result)
        self.assertEqual(2, match.signature_lockout_cycles)
        self.assertTrue(match.detection)
        self.assertTrue(match.rollback_or_lockout)
        self.assertFalse(match.normal_boot)
        self.assertIn("rollback unavailable cycles=2", target.scenario_observation(case, match))
        denial_end: int = console.rindex(target.ROLLBACK_UNAVAILABLE) + len(target.ROLLBACK_UNAVAILABLE)
        self.assertEqual("ERROR", target.match_console(case, console[:denial_end], active, alternate).result)
        line_end: int = console.index("\n", denial_end) + 1
        self.assertEqual("PASS", target.match_console(case, console[:line_end], active, alternate).result)

    def test_both_bad_signatures_require_two_ordered_lockouts_in_separate_boots(self) -> None:
        case: target.Scenario = target.SCENARIOS[11]
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        console: str = observation(case)
        first_cycle: str = console.split("rst:0xc", 1)[0]
        variants: dict[str, str] = {
            "only one rejection cycle": first_cycle + "rst:0xc (SW_CPU_RESET)\n",
            "no reset between cycles": console.replace("rst:0xc (SW_CPU_RESET)\n", ""),
            "wrong slot": console.replace("partition: ota_0", "partition: ota_1"),
            "wrong address": console.replace("address 0x00100000", "address 0x00600000"),
            "checksum failure": console + "E (9161) esp_image: Checksum failed\n",
            "denial before rejection": console.replace(target.ROLLBACK_UNAVAILABLE, "missing")
                .replace("E (2674)", target.ROLLBACK_UNAVAILABLE + "\nE (2674)"),
            "evidence straddles reset": first_cycle.replace(target.ROLLBACK_UNAVAILABLE, "missing")
                + console.replace("Secure boot signature verification failed", "missing"),
        }
        required: str
        for required in ("Secure boot signature verification failed", "image valid, signature bad",
                         target.ROLLBACK_UNAVAILABLE):
            variants[f"missing {required}"] = console.replace(required, "missing")
        name: str
        changed: str
        for name, changed in variants.items():
            with self.subTest(name=name):
                self.assertEqual("ERROR", target.match_console(case, changed, active, alternate).result)
        normal: str = "I (3927) ruuvi_gateway: [main/1] ### Init gw_cfg: device_id=test\n"
        self.assertEqual("FAIL", target.match_console(case, console + normal, active, alternate).result)

    def test_missing_redundant_self_check_and_trailing_fragment_do_not_hide_rollback(self) -> None:
        case: target.Scenario = target.SCENARIOS[3]
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        # Reproduce the gap in the 20260926T101314 UART capture, retaining the direct RSA errors.
        console: str = observation(case).replace(
            "E (3896) fw_update: Self check of running partition signature failed\n",
            "\x1b[0;31mE (3888) fw_update: [main/1] .\x1b[0;32mI (5219) esp_image: Verifying image signature...\x1b[0m\r\n",
        )
        init_end: int = console.index("\n", console.index("### Init gw_cfg:")) + 1
        self.assertNotEqual("PASS", target.match_console(case, console[:init_end - 1], active, alternate).result)
        match: target.ConsoleMatch = target.match_console(case, console[:init_end], active, alternate)
        self.assertEqual("PASS", match.result)
        self.assertTrue(match.detection)
        self.assertTrue(match.rollback_or_lockout)
        self.assertFalse(match.self_check_failure_observed)
        trailing_fragment: str = "\x1b[0;32mI (23218) http: [ad"
        self.assertEqual("PASS", target.match_console(case, console + trailing_fragment, active, alternate).result)

    def test_initial_boot_waits_for_gateway_init_after_signatures(self) -> None:
        console: str = baseline(0)
        self.assertTrue(target.initial_boot_captured(console))
        before_init: str = console.split("I (3927) ruuvi_gateway:", 1)[0]
        self.assertFalse(target.initial_boot_captured(before_init))

    def test_step_messages_name_slot_damage_and_uart_result(self) -> None:
        case: target.Scenario = target.SCENARIOS[3]
        self.serial = FakeSerial((baseline(0), baseline(0), observation(case), baseline(0)))
        result: RunResult = self.execute(case_number=4)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertTrue(any(line.startswith("Step 04 begin: active slot ota_0;")
                            and "invalid RSA signature" in line for line in self.output))
        self.assertTrue(any(line.startswith("Step 04 finished: OK — UART rejection=yes")
                            and "ota_1 running=yes" in line and "gw_cfg init observed=yes" in line
                            for line in self.output))

    def test_one_case_starts_from_its_own_slot_and_has_short_progress(self) -> None:
        case: target.Scenario = target.SCENARIOS[4]
        self.serial = FakeSerial((baseline(1, "UNDEFINED"), baseline(1, "UNDEFINED"),
                                  observation(case), baseline(0)))
        result: RunResult = self.execute(case_number=5)
        self.assertEqual(0, result.exit_code, result.recovery_message)
        self.assertEqual([case.name], list(result.outcomes))
        ota_writes: list[bytes] = [data for offset, data in self.flash.writes if offset == OTADATA_OFFSET]
        self.assertEqual(3, len(ota_writes))
        self.assertEqual(["ota_1", "ota_1", "ota_0"],
                         [decode_otadata(data).active_slot for data in ota_writes])
        self.assertTrue(any(line.startswith("[Step 18 out of 18]") for line in self.output))

    def test_incomplete_case_reports_targeted_retry_command(self) -> None:
        self.serial.captures[4] = "rst:0x1 (POWERON_RESET)\n"
        self.serial.captures[5] = baseline(0)
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertEqual("ERROR", result.outcomes[target.SCENARIOS[1].name])
        self.assertIn("python test_5_7_1_2_a.py --case 2", result.recovery_message or "")
        self.assertTrue(any(line.startswith("Step 02 finished: ERROR —") for line in self.output))

    def test_clean_boot_after_damage_is_security_failure(self) -> None:
        self.serial.captures[2] = baseline(0)
        result: RunResult = self.execute()
        self.assertEqual(1, result.exit_code)
        self.assertEqual("FAIL", result.outcomes[target.SCENARIOS[0].name])
        self.assertIn("UART rejection=no", result.recovery_message or "")
        self.assertIn("python test_5_7_1_2_a.py --case 1", result.recovery_message or "")
        self.assertTrue(any(line.startswith("Step 01 finished: FAIL —") for line in self.output))

    def test_flash_error_still_recovers_and_offers_retry(self) -> None:
        self.flash.fail_at = 0x140000
        self.serial.captures[2] = baseline(0)
        result: RunResult = self.execute()
        self.assertEqual(2, result.exit_code)
        self.assertIn("Gateway recovery verified", result.recovery_message or "")
        self.assertIn("python test_5_7_1_2_a.py --case 1", result.recovery_message or "")

    def test_recovery_flash_failure_reports_manual_command(self) -> None:
        self.installer.fail_on_flash_call = 2
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(2, result.exit_code)
        self.assertIn("Automatic recovery failed", result.recovery_message or "")
        self.assertIn("v1.17.5-dev --flash_both_slots", result.recovery_message or "")

    def test_initial_flash_failure_is_retried_for_recovery(self) -> None:
        self.installer.fail_on_flash_call = 1
        self.serial = FakeSerial((baseline(0),))
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(2, result.exit_code)
        self.assertEqual(2, sum(action == "flash_both" for action, _version in self.installer.calls))
        self.assertIn("Gateway recovery verified", result.recovery_message or "")

    def test_unverified_recovery_requires_manual_action(self) -> None:
        self.serial = FakeSerial((baseline(0), baseline(0), observation(target.SCENARIOS[0]),
                                  "rst:0x1 (POWERON_RESET)\n"))
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(2, result.exit_code)
        self.assertIn("Automatic recovery failed", result.recovery_message or "")
        self.assertIn("v1.17.5-dev --flash_both_slots", result.recovery_message or "")

    def test_full_release_name_is_required(self) -> None:
        result: RunResult = self.execute("v1.17.5")
        self.assertEqual(2, result.exit_code)
        self.assertEqual([], self.installer.calls)
        self.assertEqual(0, self.serial.calls)

    def test_installer_downloads_and_flashes_once(self) -> None:
        commands: list[list[str]] = []
        def run(command: list[str], **_kwargs: Any) -> subprocess.CompletedProcess[str]:
            commands.append(command)
            return subprocess.CompletedProcess(command, 0, "downloaded", "")
        evidence: EvidenceLog = EvidenceLog.create(self.work_dir / "logs", "installer", lambda: NOW)
        installer: target.FirmwareInstaller = target.FirmwareInstaller(self.work_dir, evidence, run)
        with mock.patch.object(target, "load_release_images", return_value=IMAGES):
            installer.download("v1.17.5-dev")
        installer.flash_both("v1.17.5-dev", self.serial.discover())
        evidence.finish("PASS", lambda: NOW)
        self.assertEqual(["--download_only", "v1.17.5-dev"], commands[0][-2:])
        self.assertEqual(["--port", "/dev/ttyUSB0", "--flash_both_slots", "v1.17.5-dev"],
                         commands[1][-4:])

    def test_command_line_selects_one_case(self) -> None:
        expected: RunResult = RunResult(0, "PASS", {}, set())
        with mock.patch.object(target, "execute_test_5_7_1_2_a", return_value=expected) as execute:
            self.assertEqual(0, target.main(["--case", "5"]))
        execute.assert_called_once_with(case_number=5)

    def test_installer_finds_tool_in_unactivated_virtualenv(self) -> None:
        scripts: Path = self.work_dir / "venv" / "bin"
        scripts.mkdir(parents=True)
        tool: Path = scripts / "esptool.py"
        tool.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        tool.chmod(0o755)
        environments: list[dict[str, str]] = []
        def run(command: list[str], *, env: dict[str, str], **_kwargs: Any) -> subprocess.CompletedProcess[str]:
            environments.append(env)
            self.assertEqual(str(tool), shutil.which("esptool.py", path=env["PATH"]))
            return subprocess.CompletedProcess(command, 0, "flashed", "")
        evidence: EvidenceLog = EvidenceLog.create(self.work_dir / "logs", "path", lambda: NOW)
        installer: target.FirmwareInstaller = target.FirmwareInstaller(self.work_dir, evidence, run)
        with mock.patch.object(target.sys, "executable", str(scripts / "python")), mock.patch.dict(
            os.environ, {"PATH": "/missing-tools", "PRESERVED_ENV": "present"}, clear=True,
        ):
            installer.flash_both("v1.17.5-dev", self.serial.discover())
            self.assertEqual("/missing-tools", os.environ["PATH"])
        evidence.finish("PASS", lambda: NOW)
        self.assertEqual("present", environments[0]["PRESERVED_ENV"])

    def test_damaged_slot_init_fails_despite_later_signed_rollback(self) -> None:
        case: target.Scenario
        for case in (target.SCENARIOS[3], target.SCENARIOS[7]):
            console: str = observation(case).replace(
                "E (2674)", "I (2500) ruuvi_gateway: [main/1] ### Init gw_cfg: unsafe\nE (2674)", 1,
            )
            self.serial = FakeSerial((baseline(case.slot), baseline(case.slot), console, baseline(0)))
            result: RunResult = self.execute(case_number=case.number)
            self.assertEqual(1, result.exit_code, result.recovery_message)
            self.assertEqual("FAIL", result.outcomes[case.name])

    def test_both_slot_capture_observes_late_initialization_after_passing_prefix(self) -> None:
        case: target.Scenario
        for case in target.SCENARIOS[8:]:
            self.serial = FakeSerial((baseline(0), baseline(0), observation(case), baseline(0)))
            self.serial.captures[2] = (
                observation(case), "I (25000) ruuvi_gateway: [main/1] ### Init gw_cfg: unsafe\n",
            )
            result: RunResult = self.execute(case_number=case.number)
            self.assertEqual(1, result.exit_code, (case.number, result.recovery_message))
            self.assertEqual("FAIL", result.outcomes[case.name])

    def test_recovery_rejects_stable_non_authentication_configuration_drift(self) -> None:
        class DriftGateway(DefaultAuthGateway):
            def __init__(self) -> None:
                super().__init__(CONFIG)
                self.reads: int = 0

            def _config_response(self) -> FakeResponse:
                self.reads += 1
                payload: dict[str, Any] = dict(super()._config_response().json())
                payload["mqtt_use"] = self.reads > 2
                return FakeResponse(200, payload)

        gateway: DriftGateway = DriftGateway()
        self.gateway = gateway
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(2, result.exit_code)
        self.assertEqual(4, gateway.reads)
        self.assertIn("differs from the pre-corruption baseline", result.recovery_message or "")
        self.assertNotIn("Gateway recovery verified", result.recovery_message or "")

    def test_baseline_failure_after_installation_keeps_factory_reset_advice(self) -> None:
        self.gateway.mode = GatewayCfgLanAuthType.BASIC
        self.serial = FakeSerial((baseline(0), baseline(0)))
        result: RunResult = self.execute(case_number=1)
        self.assertEqual(2, result.exit_code)
        self.assertEqual("NOT RUN", result.outcomes[target.SCENARIOS[0].name])
        self.assertIn(FACTORY_RESET_MESSAGE, result.recovery_message or "")
        self.assertTrue(any(FACTORY_RESET_MESSAGE in line for line in self.output))
        self.assertEqual(2, sum(action == "flash_both" for action, _version in self.installer.calls))

    def test_wrong_app_failure_mechanism_cannot_pass_signature_case(self) -> None:
        active: Partition = Partition("ota_0", 0, 0x10, 0x100000, 0x400000)
        alternate: Partition = Partition("ota_1", 0, 0x11, 0x600000, 0x400000)
        result: target.ConsoleMatch = target.match_console(target.SCENARIOS[3],
                                                           observation(target.SCENARIOS[0]),
                                                           active, alternate)
        self.assertNotEqual("PASS", result.result)


if __name__ == "__main__":
    unittest.main()
