# Task: Automate ETSI Test Case 5.7-1-2, Unit A

**MAC identity for this task:** `.env` `gw_mac` is the nRF52 identity MAC checked against
authenticated `/ruuvi.json`; it is not the ESP32 Ethernet or Wi-Fi MAC, nor any future ESP32
Bluetooth MAC. For link-layer observations, obtain the active interface MAC from the verified DUT IP
and an on-link ARP/neighbor entry or equivalent trusted context; do not filter frames on `.env`
`gw_mac`. See [DUT configuration](../tests/README.md#dut-configuration).

## Current implementation contract

Read the [automation assessment](../Functional_Test_Automation_Assessment.md),
[test README](../tests/README.md), [CRA test rules](../tests/AGENTS.md), and
[shared-library API](../tests/lib/README.md) before implementing this task. The assessment
classifies this unit as AUTO; that is feasibility, not an implemented test or a recorded live
verdict. The runner and paired implementation suite named below are future deliverables.

- Use Python 3.8 and explicit, accurate annotations for every variable, parameter, return value,
  constant, attribute, fixture, and local binding. Follow the rules for loop, unpacking, context,
  exception, and comprehension bindings in the CRA test rules. Use postponed annotations while
  keeping runtime type expressions compatible with Python 3.8; do not use broad types to evade
  typing.
- Use shared protocol/configuration descriptors and immutable dataclasses for stable records;
  dictionaries belong at real JSON/header/XML mapping boundaries. Use f-strings for dynamic text.
- Keep imports free of network/hardware access; guard `main()` with `if __name__ == "__main__"`.
  Inject sessions, randomness, clocks, output, work directories, and applicable hardware/subprocess
  boundaries.
- Reuse `EvidenceLog`, `ProgressReporter`, and `RunResult`. Log requests before sending and
  responses before interpreting. Print `Open log file: <path>` once and mirror each
  `[Step N out of M]` line into evidence with its UTC timestamp; keep secrets off the console.
  Preserve per-check outcomes in final evidence, not only the overall verdict.
- Put direct shared-library tests only in `test_lib.py`; case suites test their runner's sequencing,
  assertions, recovery, evidence, and exit codes. Import only the matching runner, never another
  case's runner or implementation tests. Reuse applicable offline fixtures from
  [`test_support/fake_gateway.py`](../tests/test_support/fake_gateway.py); shared fixture contract
  tests belong in `test_test_support.py`, and fixtures must stay outside `lib/`.
- Extend reusable APIs backward-compatibly and update `lib/README.md` for changes. Related
  `test_lib.py`, shared-fixture tests, and guidance updates are part of the deliverable when needed;
  preserve unrelated tests, firmware, assessment/IXIT documents, and user changes.
- Return 0 for PASS, 1 for an observed assertion FAIL, and 2 for setup, transport, protocol, or
  recovery ERROR. Incomplete required checks cannot yield PASS. Preserve earlier per-check FAIL
  evidence when a later ERROR prevents completion. Offline tests verify implementation, not ETSI
  compliance; only execution against the specified real target supplies functional evidence.

## Mandatory factory-default precondition and recovery

Before the initial firmware installation, load `.env` through `load_dut_config(Path(".env"))`. After
installing and verifying the selected firmware, and before any corruption case, authenticate as
`Admin`/`gw_id` through `GatewayClient` and read `GET /ruuvi.json`. Select every configuration field
relevant to this task with `default_config_values()` from
[`gw_cfg_default_gen_ui.json`](../../../gw_cfg_default/gw_cfg_default_gen_ui.json). Always include
`lan_auth_type`, `lan_auth_user`, `lan_auth_api_key_use`, and `lan_auth_api_key_rw_use` for
authentication checks. Compare the public representation directly by type and value; do not derive
defaults from saved secrets, duplicate default constants, or change the DUT to manufacture its
baseline.

A non-200 default login must mark the shared `FACTORY_RESET_MESSAGE` before raising setup ERROR.
Catch Basic/Digest `GatewayAuthenticationModeError` even when interactive login raises before
returning. Retain recovery advice in both `RunResult.recovery_message` and terminal/evidence output.
Once configuration is available, validate any exposed MAC before default fields: a wrong or
malformed identity is ERROR without reset advice. If MAC is absent, a non-default baseline is ERROR
without reset advice for an unverified device. Missing, wrongly typed, or non-default required
fields stop the run before probes or mutations; only a verified identity permits baseline reset
advice.

Use the shared reset instruction: back up needed settings; local configuration, credentials, tokens,
and uploaded certificates/private keys are erased and connectivity is interrupted, while cloud
accounts and forwarded data remain. Hold CONFIGURE through the initial restart until repeating red
200 ms on/200 ms off signals boot-time erase completion, then release. Release triggers another
restart, after which the configuration hotspot opens. About eleven seconds is observed elapsed time,
not a fixed threshold; the five-second restart timer alone does not prove erasure.

## DUT regression and state-preservation requirements

- Use `GatewayClient.new_session()` with `trust_env=False`, finite timeouts, and disabled redirects
  for negative probes. Keep baseline/authorized sessions separate from unauthenticated and bearer
  probes. Reuse shared challenge, Basic/Digest, ECDH, and checked-JSON helpers.
- Compare decoded values by type and equality, never string/object identity. Each configuration
  validator needs independently JSON-decoded equal, unequal, missing, and wrong-type fixtures,
  including boolean/integer equality traps.
- Complete safer probes across all schemes in a prepared state before potentially mutating probes,
  including positive reads where applicable. Abort negative probes on unexpected authorization
  success; still run mandatory final verification/restoration. Keep progress phases distinct by
  authentication mode. Empty bodies do not make destructive handlers safe.
- Verify route-specific expectations against [the HTTP API](../../../docs/http_api.md) and handlers.
  Use only the task's required routes; do not inherit a predecessor's matrix count. If an inventory
  check is required, cover length, uniqueness, canonical membership, and completed coverage. For LAN
  `GET /info.json`, authentication precedes dispatch: missing/Basic/Digest credentials without an
  authorized cookie return 302 in default mode, and rejected bearer tokens return `401`. The
  hotspot-only 404 occurs only after authorization succeeds; reject it in negative probes.
- Track the active mechanism/check explicitly. Defer PASS until all its controls finish; a later
  failure replaces that mechanism's PASS without erasing another mechanism's completed PASS.
  Provisioning, read-back, and required login controls form setup; exceptions there are ERROR.
  Attribute rejected security assertions and final hash failures to the responsible check.
- Before mutation, obtain two stable authenticated configuration reads and retain the full baseline.
  Send only fields within this task's mutation scope. Partial JSON parsing can rewrite omitted
  fields; copying the current configuration does not guarantee preservation. Authentication-only
  changes must omit unrelated MQTT fields. Detect side effects with complete configuration
  comparisons; never normalize them away or add unrelated fields merely to hide a mismatch. An RW
  authorization-only probe uses exactly `POST /ruuvi.json` with `{}` and hash verification.
- Save candidate recovery credentials before each possibly applied change and retain prior
  candidates until verified. Restore and verify after partial setup, failures, and exceptions. A
  changed or unverifiable final state prevents PASS; unverified restoration is overall ERROR with
  operator recovery advice, while any proven security FAIL remains in its own outcome.
- Host tests must validate complete login bodies independently, including password response, session
  cookie, and outstanding challenge; consume successful challenges and reject missing, stale,
  replayed, or cross-session submissions. Include a full run through the real client and fake HTTP
  transport. Model Basic/Digest setup challenges faithfully.
- Assert exact prepared request order, scheme, method, path, body, session, and redirect settings
  against independent expectations. Cover late safer-phase failures, immediate aborts, partial
  mutation, recovery candidates, failed restoration, final mismatches, and evidence/outcomes. Direct
  parsing, crypto, and evidence-serialization tests remain in `test_lib.py`.

## Objective

Create an automated (AUTO) **destructive** functional live-DUT test for **ETSI EN 303 645 / ETSI TS
103 701 test case 5.7-1-2, Test Unit A: Functional Software Integrity Verification Testing**.

The test must run against a physical Ruuvi Gateway and prove that the application-layer secure-boot
verification declared in `IXIT 20-SecBoot` actually detects tampered binary images: it deliberately
corrupts bytes in the active application slot and data partitions, resets the device, and confirms
from the UART boot console that the firmware detects the corruption and performs the documented
rollback/lockout behavior.

This destructive test does not read flash or back up partitions. The script asks for a full firmware
version (for example `v1.17.5-dev`), downloads that release, installs it in both slots, and runs 12
distinct cases. It writes validated OTA selection metadata and restores only the regions changed by
previous cases. `--case N` runs one case from a fresh installation. After the case sequence,
including an early failure, the script reflashes both slots, selects slot 0, and verifies signed
boot and LAN functionality. If automatic recovery cannot be verified, it reports ERROR and gives the
manual flash command.

Use [`cra/303645/tests/lib`](../tests/lib/README.md) as the primary implementation reference and
[`test_5_1_1_2_b.py`](../tests/test_5_1_1_2_b.py) as the runner/user-experience example. The
firmware, partition table, IXIT, serial, and `esptool` sources below are behavioral references; do
not copy their generic parsing, logging, result, or error-handling patterns into the test.

## Deliverables

1. Create `cra/303645/tests/test_5_7_1_2_a.py`, compatible with Python 3.8. The live-DUT script
   loads `.env` from the current working directory and accepts optional `--case N` (`1` through
   `12`); without it, run the complete matrix.
2. Create `cra/303645/tests/test_test_5_7_1_2_a.py` with deterministic host-side `unittest` tests
   using fakes for the flash tool, serial transport, and clock. They must never write real flash or
   open a real port. These are implementation regression tests, **not** compliance evidence.
3. Extend `cra/303645/tests/lib/` only with a generally reusable, backward-compatible helper (the
   shared serial/USB discovery helper — reuse it if another task already added `lib/serial_dut.py`;
   otherwise add it). Keep `test_5_1_1_2_b.py` and its host tests passing unchanged.
4. Update `cra/303645/tests/requirements.txt` to add `esptool` (the repository's documented flashing
   tool; it also provides `pyserial`). Prefer the standard library otherwise.

Do not modify firmware behavior, source assessments, or the signing key. Related shared-library,
direct-test, fixture, and guidance updates are allowed as required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_7-1.md`](../Self_Assessment_Test_Group_5_7-1.md), test
case 5.7-1-2, Test Unit A (lines 53-81). The declared mechanism is
[`ixit_20-SecBoot.md`](../ixit_20-SecBoot.md), `SecBoot-Application-Chain-Of-Trust` (lines 11-91).

Key declared behavior to verify functionally:

- On early init the firmware runs `fw_update_read_flash_info_and_check_signatures()`, calling
  `esp_image_verify` on the active app slot and `esp_secure_boot_verify_rsa_signature_block` on the
  Web-UI (`fatfs_gwui`) and nRF52 (`fatfs_nrf52`) partitions, all RSA-3072 PSS over SHA-256
  (`ixit_20-SecBoot.md` lines 40-59).
- The signed-on-boot ESP-IDF bootloader can reject app checksum and RSA signature failures before
  application start and load the alternate slot. Application FATFS signature failures initiate OTA
  rollback to the alternate slot.
- If both slots fail (or the alternate is unpopulated), the device halts application bring-up,
  blocks all network interfaces, and enters a persistent reboot loop via `gateway_restart()` (lines
  68-74).
- Both paths occur before networking, so the evidence is on the **UART boot console**.

The 12-case matrix covers app checksum failure and checksum-valid app signature failure, FATFS
signature failure for Web-UI and nRF52, each from active slot 0 and slot 1, plus the corresponding
four both-slot reboot-loop cases. Each case starts with clean images. The test records a separate
verdict for every case.

Interpretation rules:

- Corrupt **only** the byte ranges specified below. **Never** modify the second-stage bootloader
  (`0x1000`), the partition table (`0x8000`), `nvs` (`0x9000`), `phy_init` (`0xf000`), or the
  default-config partition `gw_cfg_def` (`0xB00000`). The test may write only a validated two-entry
  OTA selection image to the `otadata` partition (`0xd000`, `0x2000`).
- **Never** request, read, generate, or use the secure-boot signing key. This test proves detection
  of corruption; it does not sign anything. Signature _validity_ is not the goal — signature/digest
  _rejection_ is.
- The evidence for pass/fail is the boot-console log content, matched against expected verification
  and rollback/lockout messages.

## Partition layout, artifacts, and tools

Partition map from [`partitions.csv`](../../../partitions.csv):

| Name            | Type/SubType | Offset     | Size      |
| :-------------- | :----------- | :--------- | :-------- |
| `nvs`           | data/nvs     | `0x9000`   | `0x4000`  |
| `otadata`       | data/ota     | `0xd000`   | `0x2000`  |
| `phy_init`      | data/phy     | `0xf000`   | `0x1000`  |
| `ota_0`         | app/ota_0    | `0x100000` | `4M`      |
| `fatfs_gwui`    | data/fat     | `0x500000` | `0xC0000` |
| `fatfs_nrf52`   | data/fat     | `0x5C0000` | `0x40000` |
| `ota_1`         | app/ota_1    | `0x600000` | `4M`      |
| `fatfs_gwui_2`  | data/fat     | `0xA00000` | `0xC0000` |
| `fatfs_nrf52_2` | data/fat     | `0xAC0000` | `0x40000` |
| `gw_cfg_def`    | data/nvs     | `0xB00000` | `0x40000` |

Do not hard-code offsets blindly: parse the partition table printed in the UART boot log and compare
every row with `partitions.csv`. Identify the **currently running** app slot from the UART
`Loaded app` and `### Currently running partition:` lines. This is the source of truth even when
`Currently running partition state` says `UNDEFINED` after bootloader fallback. Require the
running-partition observations and the table to agree before each corruption write. The downloaded
partition-table binary must match `partitions.csv` before the initial firmware installation. Do not
invoke `read_flash`, including for `otadata`. The initial UART capture stops only after the app and
both paired FATFS signature-success lines and the subsequent
`ruuvi_gateway: [main/1] ### Init gw_cfg:` line are complete; the bounded timeout remains for
missing or incomplete boot evidence.

Tools (all documented for this repository in [`README.md`](../../../README.md) lines 250-297):

- `ruuvi_gw_flash.py` for the initial installation, and `esptool.py write_flash <offset> <file>` for
  selected partition writes, patched sectors, and validated `otadata`.
- `esptool.py --before default_reset --after hard_reset ...` and/or DTR/RTS toggling to reset.

## Serial/USB device discovery and reset

Load the unchanged `.env` through `lib.config.load_dut_config(Path(".env"))`; do not parse or
validate it locally. It holds only `gw_id`, `gw_mac`, `gw_hostname` and must not be extended. Serial
discovery is independent of `gw_hostname`. Discover the port deterministically:

- The Ruuvi Gateway uses a **CH340** bridge (**USB VID `0x1A86`**) at **115200 baud**
  ([`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), `LogIntf-USB-UART-Log-Stream`;
  [`README.md`](../../../README.md) lines 11-17).
- Enumerate with `serial.tools.list_ports.comports()` and select ports whose USB VID is `0x1A86`.
  Exactly one -> use it. Zero or multiple -> `ERROR` (exit `2`); do not guess, prompt, or add a
  flag.
- Put discovery in the shared `lib/serial_dut.py` helper and inject enumeration/open functions for
  the host tests.

Reset/power-cycle automation for each boot after corruption:

- Reset via `esptool` `--before default_reset --after hard_reset` (DTR/RTS), matching
  [`README.md`](../../../README.md) line 263. A relay/PSU is **not** available in `.env`; DTR/RTS is
  the automation path.
- If the prepared bench does not allow DTR/RTS reset or download-mode strapping (see
  [`ixit_15-PhyIntf.md`](../ixit_15-PhyIntf.md) `PhyIntf-USB`, lines 30-37), the required
  prepared-environment condition is a Gateway whose CH340 DTR/RTS auto-reset and `esptool`
  download-mode entry work. If reset or `esptool` connection fails, emit `ERROR`; **do not** print
  manual instructions to press a button or replug — this is an AUTO test.

## Preflight and abort preconditions

The selected release must be installed before LAN login and baseline checks so a gateway left in a
reboot loop by a previous run can be recovered. Before initial firmware installation:

1. Verify `esptool` is runnable and log its version; verify pyserial import and version.
2. Discover exactly one CH340 serial port.
3. Connect with `esptool` and read the chip id / flash size to confirm the DUT is reachable in the
   bootloader; log it.
4. Download the selected release and validate its layout and app checksum.

After installation, write validated OTA metadata, reboot, and capture UART. Parse the printed
partition table and running slot; confirm the table matches `partitions.csv` and the signed active
app/FATFS pair booted successfully. Then authenticate, validate factory-default identity, and read
the stable configuration baseline before running any mutation case.

If any preflight step fails, emit `ERROR` and do not corrupt anything. If installation has started,
still attempt automatic recovery. Before preflight, print the destructive-test notice and ask for
the full firmware version.

## Required shared functional-test architecture

Keep the runner focused on destructive safety ordering, all 12 distinct corruption scenarios, boot
evidence, and verdict aggregation:

- [`config.py`](../tests/lib/config.py): use its strict `.env`/`DutConfig` setup contract without
  local dotenv, hostname, or identity parsing.
- [`evidence.py`](../tests/lib/evidence.py): use `EvidenceLog.create(...)` as the exclusive
  structured log and use its exception/finalization behavior. Do not add a second logger,
  timestamp/naming helper, generic record serializer, or separate console transcript log; raw UART
  and tool output are structured values in this evidence log.
- [`models.py`](../tests/lib/models.py) and [`errors.py`](../tests/lib/errors.py): reuse
  `DutConfig`, `ProgressReporter`, `RunResult`, and shared setup/error classification. Define stable
  partition, corruption, console-match, scenario, and tool-result records as typed immutable
  dataclasses. Use mappings only at genuine external boundaries such as decoded tool output, and
  convert them promptly to typed records.
- A reusable `serial_dut.py`/flash-tool helper owns injected port enumeration, safe subprocess
  invocation, bounded capture, tool-output parsing, and transport failures. Do not repeat generic
  serial/esptool parsing in the case runner. Preserve all task-specific forbidden ranges, partition
  checks, reset/manual-recovery requirements, and verdict distinctions in this document.
- Use `GatewayClient` for mandatory authenticated identity/default preflight after the selected
  firmware boots and before any corruption case. Keep serial, UART, DTR/RTS, flash, partition, and
  `esptool` operations in their own injected helpers; a gateway HTTP client does not implement those
  transports.

Use the established dependency-injected `FunctionalTest_5_7_1_2_a`,
`execute_test_5_7_1_2_a(...) -> RunResult`, shared progress reporting, and `main() -> int`; do not
define parallel generic result/progress/error recipes.

## Operator execution guide

Run from `cra/303645/tests` after meeting this task's USB, firmware-download and recovery
preconditions. This test is destructive: it installs firmware and mutates flash on the dedicated
DUT. Run one numbered case from a fresh installation, for example:

```text
.venv/bin/python test_5_7_1_2_a.py --case 2
```

Omit `--case` to run all 12 cases. The script prompts with `.env` `gw_fw` as the default release (or
`v1.17.5-prod` when absent); Enter accepts it, while a typed tag overrides it. It installs the
release in both slots before checking the Gateway over LAN and reflashes both slots after the run or
an error. Recovery requires slot 0 signed boot and the authenticated LAN baseline; unverified
recovery is ERROR with a manual flash command. Each failed case gets a targeted retry command.

Each case prints `Step N begin` with the selected active slot, target, mutation and expected UART
result, followed by `Step N finished: OK`, `FAIL` or `ERROR` with observed UART signals. These case
messages are separate from the numbered setup/recovery progress. A single-slot rollback confirms
startup only after the opposite slot verifies its app and paired FATFS signatures and prints
`### Init gw_cfg:`. The detailed signal rules and recovery procedure follow below.
Initialization by a damaged slot is FAIL even if a later signed fallback boots successfully.
Both-slot cases 9-12 observe the entire bounded 30-second capture window before declaring PASS,
so initialization after an initially sufficient rejection prefix remains visible.

## Firmware installation and scenario sequence

Prompt for a full version such as `v1.17.5-dev`, showing optional `.env` `gw_fw` as the default.
Pressing Enter selects that default; when `gw_fw` is absent or empty, use `v1.17.5-prod`. From the
repository root, run `./ruuvi_gw_flash.py --download_only v1.17.5-dev`, validate the downloaded
images and partition table, then install the baseline using
`./ruuvi_gw_flash.py --port <CH340-port> --flash_both_slots v1.17.5-dev`.

The 12 distinct cases are grouped as follows:

| Cases | Starting state and mutation                                                                                   | Expected result |
| :---- | :------------------------------------------------------------------------------------------------------------ | :-------------- |
| 1-4   | slot 0: app sector damage, Web-UI FATFS damage, nRF52 FATFS damage, checksum-valid app with bad RSA signature | boot slot 1     |
| 5-8   | slot 1: same four mutations                                                                                   | boot slot 0     |
| 9-12  | both slots: same four mutations                                                                               | reboot loop     |

The release's `ota_data_initial.bin` is an erased two-sector image. Build two ESP-IDF OTA entries:
sequence numbers `(3, 2)` select slot 0 and `(1, 2)` select slot 1. Each entry has `VALID` state at
byte 24 and ESP-IDF `crc32_le(UINT32_MAX, sequence, 4)` at byte 28. Validate both entries and the
selected slot before writing the 8 KiB image. Write it after initial installation and after
restoring the preceding case's changed regions. Reboot and confirm the intended running slot on
UART. Never infer the running slot from `otadata` or its printed state. No `read_flash` operation,
partition backup, or HTTP firmware update is performed.

App damage flips one bit in an interior 4 KiB image-body sector while preserving every other byte,
causing bootloader checksum failure. For bad-signature app cases, change an image-body byte,
recompute the ESP checksum and appended simple SHA-256, and leave the RSA signature block intact.
FATFS partitions have no ESP image checksum; flip one bit in unused padding at the end of the second
FAT12 allocation table. The signed partition changes without changing the filesystem's active FAT,
directory, or file data. Verify the FAT12 geometry and padding before writing the patched 4 KiB
sector. Never read or use the signing key.

Capture UART before every mutation to prove that the selected app and paired FATFS partitions verify
successfully and gateway initialization reaches `### Init gw_cfg:`. A single-slot case requires
rejection of the target and a subsequent loaded, running, signed opposite slot followed by that
slot's `### Init gw_cfg:` line. The `### Currently running partition:` line identifies that slot;
the printed OTA state is informational. Both-slot cases require rejection of both targets, multiple
reset banners, and no normal boot. Incomplete UART evidence is ERROR. A normal boot despite mutation
is FAIL.

For app checksum damage, the bootloader reports the target OTA slot as not bootable. A
checksum-valid app with a bad RSA signature can pass bootloader loading and fail in the running
firmware's self-check instead. In that case, require the active-slot running line, signature
failure, `image valid, signature bad`, `Firmware rollback is possible, so try to do it`, and a
signed boot of the opposite slot for single-slot cases. Do not require a bootloader
`OTA app partition slot ... is not bootable` line for this case. Record
`Self check of running partition signature failed` as supplementary evidence; its absence does not
invalidate the explicit RSA rejection and complete rollback evidence. For case 12, the rollback API
checks the alternate app without running it. Require two separate boots of the selected slot
(matching its name and address), each with an RSA verification failure, then
`image valid, signature bad`, then
`Rollback is not possible, do not have any suitable apps in slots`. A checksum/hash failure in
either counted boot invalidates that cycle. No `### Init gw_cfg:` may appear. Do not require the
alternate slot to run: both invalid signatures cause repeated boots of the selected slot. Report the
number of signature rejection / rollback unavailable cycles in the case result. Match only complete
UART lines. A later partial line must not invalidate evidence already completed, while an incomplete
required line cannot satisfy a check. Print `Step N begin` with the active slot, mutation, and
expected UART result, followed by `Step N finished: OK`, `FAIL`, or `ERROR` with the observed
evidence.

After the final case or a premature failure, reflash the selected release in both slots, write
validated OTA metadata selecting slot 0, and verify signed UART boot plus authenticated LAN
baseline. Recovery failure or incomplete verification makes the overall verdict ERROR and prints
manual flashing instructions. After a case FAIL or ERROR, print `python test_5_7_1_2_a.py --case N`
for each failed case.
Retain the original stable full-configuration digest and require the recovered configuration to
match it; stable changes to non-authentication settings also make recovery ERROR. Preserve any
factory-reset advice when appending installation or recovery diagnostics. Firmware-helper child
processes prepend the running interpreter's directory to PATH, supporting an unactivated virtualenv.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_7_1_2_a_<UTC-date-time>.log
```

Use `EvidenceLog` as the exclusive structured evidence sink. The log must include:

- test case/unit, UTC start/end/duration;
- discovered serial port and matched USB VID/PID; `esptool`/pyserial versions; chip id/flash size;
- UART partition table, raw initial boot, and chosen active slot;
- selected scenario, exact overwritten sector offsets/lengths, generated payload digest, reset
  method, raw captured console window, matched patterns, and scenario verdict;
- automatic recovery attempt and verification result; manual recovery command if unverified;
- per-scenario and overall verdicts; exception types and tracebacks.

Authentication evidence may contain secrets; keep all artifacts local and ignored. The signing key
is never accessed. Keep terminal output concise and free of secrets.

## DUT preconditions and safety

Run only against a **dedicated, sacrificial** test gateway:

- physically connected over USB with a working CH340 port and DTR/RTS reset;
- reachable in the ESP32 ROM bootloader via `esptool` (download mode entry works);
- not a production device — flash is rewritten;
- with the operator prepared to reflash both slots manually if automatic recovery fails.

The test intentionally corrupts flash and reboots the device repeatedly. Never run it against a
production gateway.

## Verdict and exit codes

Record a verdict for all 12 cases. PASS requires every case to pass; a security assertion FAIL
persists even when later cases pass. Incomplete evidence or setup/transport failure is ERROR.
Verified automatic recovery is required after any write; recovery failure is ERROR.

| Exit code | Meaning                                                    |
| :-------- | :--------------------------------------------------------- |
| `0`       | All 12 cases PASS                                          |
| `1`       | At least one case proves an integrity-verification failure |
| `2`       | Setup, transport, or incomplete UART evidence              |

Host-side tests must exercise all 12 cases without live hardware, verify app checksum/hash repair
and unchanged RSA signature, validate both generated OTA selections and one-bit FAT12 mutations, and
cover `--case N` plus FAIL/ERROR handling. Run the complete offline discovery command in **Required
implementation validation** below.

## Acceptance criteria

- The no-argument live script loads `.env` and asks for a full release version.
- It downloads one release, flashes both slots initially, and generates valid two-slot OTA metadata
  to select the starting slot. It restores only previously changed regions and performs all 12
  distinct cases, or one selected case with `--case N`.
- It identifies the active slot and partition table from UART, never reads flash, and never touches
  the secure-boot signing key.
- It records separate failure-mechanism and rollback/reboot-loop evidence for each case.
- It reflashes both slots and verifies signed boot and LAN functionality on both successful and
  unsuccessful runs. If recovery cannot be verified, it prints manual recovery advice.
- Python 3.8 host tests and Ruff pass.

## Ambiguities to record (do not silently resolve)

- Production units enforce signature checks programmatically at the application layer without burned
  secure-boot eFuses ([`ixit_20-SecBoot.md`](../ixit_20-SecBoot.md) lines 18-25 and 42-47); the
  observable evidence is therefore the application boot log, not a ROM-bootloader secure-boot abort.
  Record this so the boot-log expectations are interpreted correctly.
- The app image-body sector is checked against segment addresses in the initial UART boot log. If no
  segment proves the chosen sector belongs to the running image, abort before writing.

## Implementation references

- [`Self_Assessment_Test_Group_5_7-1.md`](../Self_Assessment_Test_Group_5_7-1.md), lines 53-81.
- [`ixit_20-SecBoot.md`](../ixit_20-SecBoot.md), lines 11-91.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), `LogIntf-USB-UART-Log-Stream` and
  `LogIntf-USB-Boot-Flasher`.
- [`ixit_15-PhyIntf.md`](../ixit_15-PhyIntf.md), lines 9-37 (`PhyIntf-USB`).
- [`partitions.csv`](../../../partitions.csv).
- [`README.md`](../../../README.md), lines 11-17 (CH340) and 250-297 (esptool read/write/reset).
- [`main/http_server_cb_on_get.c`](../../../main/http_server_cb_on_get.c) (`GET /status.json` after
  recovery).
- [`docs/http_api.md`](../../../docs/http_api.md), lines 228-238 (`GET /status.json`).
- [`cra/303645/tests/test_5_1_1_2_b.py`](../tests/test_5_1_1_2_b.py).
- [`cra/303645/tests/test_test_5_1_1_2_b.py`](../tests/test_test_5_1_1_2_b.py).
- [`cra/303645/tests/lib/config.py`](../tests/lib/config.py).
- [`cra/303645/tests/lib/evidence.py`](../tests/lib/evidence.py).
- [`cra/303645/tests/lib/gateway.py`](../tests/lib/gateway.py).
- [`cra/303645/tests/lib/http_api.py`](../tests/lib/http_api.py).
- [`cra/303645/tests/lib/models.py`](../tests/lib/models.py).
- [`cra/303645/tests/lib/errors.py`](../tests/lib/errors.py).

## Runner naming

Use `FunctionalTest_5_7_1_2_a` and `execute_test_5_7_1_2_a`; retain `main() -> int` as the
executable entry point with optional `--case N`.

## Required implementation validation

Run Ruff from the repository root before changes and again before handoff, including guidance-only
changes; fix findings and report the initial count and final result:

```bash
cra/303645/tests/.venv/bin/ruff check cra/303645/tests
```

This discovers `tests/pyproject.toml` under the CRA project. For explicit selection from the
repository root use `(cd cra/303645/tests && .venv/bin/ruff check --config pyproject.toml .)`. Do
not run `ruff format`, apply unsafe fixes blindly, or weaken rules. Review explicit annotations
separately; Ruff does not enforce all local-variable annotations.

For implementation changes, use the actual Python 3.8 interpreter and run all offline runner tests:

```bash
cd cra/303645/tests
.venv/bin/python --version
.venv/bin/python -m unittest discover --verbose --start-directory . --pattern "test_test_*.py"
```

Confirm nonzero discovery and report counts. Never run live scripts or broad `test_*.py` discovery
as ordinary validation. When changing shared code or `test_lib.py`, also run from that directory:

```bash
.venv/bin/python -m coverage erase
.venv/bin/python -m coverage run --branch --source=lib -m unittest test_lib.py
.venv/bin/python -m coverage report --show-missing --fail-under=95
```

Coverage must come from `test_lib.py` alone. Inspect each existing file before editing and each
changed file after its final edit with MCP `get_file_problems`, repository-relative `filePath`, and
`errorsOnly: false`; for shared changes also inspect affected callers and their tests. Resolve
actionable findings and recheck; report unavailable or timed-out inspections as unverified. Finish
with `git diff --check` and a clean whole-subtree Ruff run. Guidance-only changes require Ruff, IDE
inspection, and path/command checks, not rerunning unchanged Python suites. Keep local `.env`, logs,
backups, manifests, and coverage output out of commits.
