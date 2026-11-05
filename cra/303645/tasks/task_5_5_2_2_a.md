# Task: Automate ETSI Test Case 5.5-2-2, Unit A

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
verdict. The implementation is [`test_5_5_2_2_a.py`](../tests/test_5_5_2_2_a.py), with paired
offline checks in [`test_test_5_5_2_2_a.py`](../tests/test_test_5_5_2_2_a.py).

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

Before test-specific probes, scans, resets, or mutations, load `.env` through
`load_dut_config(Path(".env"))`, authenticate as `Admin`/`gw_id` through `GatewayClient`, and read
`GET /ruuvi.json`. Select every configuration field relevant to this task with
`default_config_values()` from
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

## Corrected runtime observation source

A live run on firmware v1.16.3 showed that `GET /status.json` returns network connection fields
(`ssid`, `ip`, `netmask`, `gw`, `dhcp`, `urc`), not `ESP_FW`, `NRF_FW`, and `DEVICE_ADDR`. The
handler in `components/esp32-wifi-manager/src/json_network_info.c` confirms that distinction. The
earlier task and HTTP API reference incorrectly associated this endpoint with
`schemas/ruuvi_gw_status.schema.json`, which describes outbound HTTP statistics.

Use the already-required authenticated `GET /ruuvi.json` response as the objective runtime source:
`fw_ver` is the ESP32 version, `nrf52_fw_ver` is the co-processor version, and `gw_mac` is the DUT
identity. These are emitted from firmware runtime device information, not inferred from `.env` or
configuration defaults. Record the endpoint and original field names explicitly in evidence. The
exact gateway request sequence is `GET /auth`, `POST /auth`, `GET /ruuvi.json`, `GET /metrics`;
neither `/status.json` nor hotspot-only `/info.json` is needed. Source assessments remain unchanged.

Use the same authenticated session for `/metrics` after the JSON identity/default gate. Require
exactly one `ruuvigw_info{mac="...",esp_fw="...",nrf_fw="..."} 1` sample, with label order
independent, and validate its MAC against `.env`. Compare JSON `fw_ver`, metrics `esp_fw`, and UART
`cpu_start: App version:` for ESP32; compare JSON `nrf52_fw_ver`, metrics `nrf_fw`, and UART
`### Firmware on nRF52:` for the co-processor, independently of logger-tag spelling (`nRF52Fw`
versus `nrf52fw`; legacy unprefixed messages under either tag are also accepted). Never substitute
`Firmware on FatFS`, which describes a stored update image. Well-formed disagreements are FAIL;
missing/duplicate/malformed metrics or missing/conflicting UART versions are ERROR. Keep separate
ESP32/nRF52 consistency outcomes so later release-existence PASS cannot mask a mismatch. Compare the
HTTP values before reset to preserve a proven HTTP mismatch if UART acquisition later errors.

The UART framework observation must also come from the **running application**, specifically
`cpu_start: ESP-IDF:          v4.2.5`. The `ESP-IDF ... 2nd stage bootloader` line describes the
bootloader, which can remain unchanged across OTA updates and use an older framework. Ignore that
line for the framework comparison. Bootloader v4.0.3 with application v4.2.2 is a FAIL against the
v4.2.5 expectation; the same bootloader with application v4.2.5 passes this check. Capture until
complete ESP-IDF, ESP32-app, and installed-nRF52 version lines arrive, with a eight-second maximum
and immediate early exit. Two seconds can capture ESP-IDF but miss nRF52 startup. ROM
`DOWNLOAD_BOOT` / `waiting for download` without an application version means the automatic reset
did not start the application: report ERROR, not FAIL.

## Objective

Create an automated (AUTO) functional live-DUT test for **ETSI EN 303 645 / ETSI TS 103 701 test
case 5.5-2-2, Test Unit A: Functional Identification Verification**.

The test must run against a physical Ruuvi Gateway and **objectively identify** the name and version
of each declared network and security implementation on the DUT, then compare those observations to
the identifications declared in `IXIT 12-NetSecImpl` and to public GitHub release evidence.

The Self_Assessment methodology for this unit inspects "build logs, system diagnostic status
telemetry (`/status`), UART boot console outputs, and public GitHub repository release tags to
cross-check component names and version strings." The automated test reproduces exactly the parts of
that methodology that are observable from a running device and a public network:

1. read the DUT firmware and co-processor versions from both authenticated HTTP sources;
2. capture application ESP-IDF and both installed firmware versions from UART, then require
   three-source agreement separately for ESP32 and nRF52;
3. verify each observed version against `IXIT 12-NetSecImpl` and against published GitHub release
   tags.

Follow the structure and user experience of [`test_5_1_1_2_b.py`](../tests/test_5_1_1_2_b.py). Reuse
the shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md) rather than duplicating
configuration, logging, HTTP, or authentication code.

## Deliverables

1. Create `cra/303645/tests/test_5_5_2_2_a.py`, compatible with Python 3.8. This is the live-DUT
   script. It must take **no command-line arguments** and load `.env` from the current working
   directory.
2. Create `cra/303645/tests/test_test_5_5_2_2_a.py` containing deterministic host-side `unittest`
   implementation tests that use mocks/fakes and never touch real hardware or the network. These
   tests are implementation regression tests, **not** ETSI compliance evidence.
3. Extend `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is
   required (see "Serial/USB device discovery"). Do not duplicate an existing helper. Keep
   `test_5_1_1_2_b.py` and its host tests passing unchanged.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. This test
   needs serial-port access; add `esptool` (the tool already documented for this repository, which
   pulls in `pyserial`) and prefer the standard library plus the existing `requests` dependency for
   everything else.

Do not modify firmware behavior or source assessments. Related shared-library tests, fixtures, and
guidance may be updated as required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-2.md`](../Self_Assessment_Test_Group_5_5-2.md), test
case 5.5-2-2, Test Unit A (lines 50-75). The declared component identifications are in
[`ixit_12-NetSecImpl.md`](../ixit_12-NetSecImpl.md) (summary matrix at lines 156-164).

The four declared implementations and how each is observable on a running DUT:

| Implementation ID (`IXIT 12-NetSecImpl`)                        | Declared name & version                          | Objective on-DUT observation                                        | Classification of result                                                                                                                                                                                                                         |
| :-------------------------------------------------------------- | :----------------------------------------------- | :------------------------------------------------------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `NetSecImpl-Framework-Stack`                                    | ESP-IDF **v4.2.5**                               | Application UART startup line (`cpu_start: ESP-IDF: v4.2.5`).       | **Hard match.** Must equal the IXIT-declared and repository-pinned `v4.2.5`. Mismatch is a FAIL.                                                                                                                                                 |
| `NetSecImpl-CoProcessor-Firmware`                               | nRF5 SDK v15.3.0 / `ruuvi.gateway_nrf.c` release | `nrf52_fw_ver`, metrics `nrf_fw`, and installed nRF52 UART version. | **Consistency and existence match.** `nrf52_fw_ver` must be a published release tag of `ruuvi/ruuvi.gateway_nrf.c`. The nRF5 SDK version itself is build provenance, not observable at runtime — record it as informational (see "Ambiguities"). |
| `NetSecImpl-Framework-Stack` app layer                          | `ruuvi.gateway_esp.c` release                    | `fw_ver`, metrics `esp_fw`, and UART `App version:`.                | **Consistency and existence match.** `fw_ver` must be a published release tag of `ruuvi/ruuvi.gateway_esp.c`.                                                                                                                                    |
| `NetSecImpl-Crypto-Library` / `NetSecImpl-Web-Application-Core` | mbedTLS (custom backport) / Web-UI bundle        | Not exposed as a runtime version string over HTTP.                  | **Informational.** Record that no objective on-DUT version string exists; do not FAIL on it.                                                                                                                                                     |

The repository pins ESP-IDF to `v4.2.5` in [`CMakeLists.txt`](../../../CMakeLists.txt) via
`set(EXPECTED_IDF_VERSION v4.2.5)` (line 24) and the nRF52 co-processor firmware to `v2.0.0` via
`set(RUUVI_NRF52_FW_VERSION "v2.0.0")` (line 30). Treat `EXPECTED_IDF_VERSION` from `CMakeLists.txt`
as a cross-check for the boot banner, but the authoritative expectation for the framework version is
the IXIT-declared `v4.2.5`.

Important interpretation rules:

- **Objective identification only.** Report only what the DUT and public GitHub actually return. Do
  not infer a version from the presence of an endpoint or from the `.env` file.
- **Do not assume the latest release equals the installed version.** The GitHub check verifies that
  the DUT-reported version _exists as a published release tag_, not that it is the newest one. Query
  the specific tag; never compare against `releases/latest`.
- **No credentials.** Use only the unauthenticated public GitHub REST API. Never send a token, never
  read `GITHUB_TOKEN` or any credential from the environment, and never write GitHub credentials to
  the log.
- This test verifies identification/version matching. It is not a cryptographic or penetration test
  and must not claim more than name/version correspondence.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for DUT setup, logging, common
  results/progress, and setup/communication classification. Do not reimplement these facilities.
- Use `GatewayClient` for the complete interactive login, sessions, logged HTTP, and checked JSON.
  Use `GatewayApi.AUTH`, `GatewayApi.CONFIG`, matching `GatewayCfgDesc` constants, and
  `GatewayCfgLanAuthType.DEFAULT` rather than local string constants. The raw
  `fw_ver`/`nrf52_fw_ver`/ `gw_mac` telemetry object is a genuine protocol mapping boundary.
- Use the applicable `HttpMethod`, `HttpStatus`, and `HttpHeader` constants from `lib.http_api`.
  `API_INVENTORY` is not a version catalogue and need not be traversed by this device-identification
  task.
- Stable device-info, serial-discovery, boot-banner, and release-lookup schemas must be typed
  immutable models. Dictionaries are allowed only at actual JSON/header mapping boundaries, with
  `GatewayCfgDesc` keys where applicable.

The shared modules are the primary implementation references. Firmware and repository sources are
used only to define expected observable behavior and version declarations.

Follow the existing test pattern:

- define a `FunctionalTest_5_5_2_2_a` class with dependency injection for the HTTP session factory,
  the serial transport, and the GitHub-release lookup function so the host tests can substitute
  fakes;
- expose `execute_test_5_5_2_2_a(...) -> RunResult` returning a `RunResult` for host tests;
- expose `main() -> int` and call `sys.exit(main())`;
- report progress as `[Step N out of M] <description>`;
- print the log path once as `Open log file: <path>`;
- print the final line as `Overall verdict: <PASS|FAIL|ERROR>`.

## Execution and configuration

The test takes no command-line arguments:

Live execution on a dedicated gateway meeting the preconditions below:

```text
cd cra/303645/tests
.venv/bin/python test_5_5_2_2_a.py
```

Load `.env` from the current working directory. The administrative username is `Admin` and `gw_id`
is its default password. Use `gw_hostname` for the DUT URL (via `DutConfig.base_url`) and verify the
DUT-reported `gw_mac` normalizes to the same 12 hex nibbles as `.env` `gw_mac`.

## Serial/USB device discovery (mandatory, no CLI args)

The `.env` file contains only `gw_id`, `gw_mac`, and `gw_hostname` and must not be extended. The
serial port must therefore be discovered automatically and deterministically:

- The Ruuvi Gateway exposes a **CH340** USB-to-UART bridge (**USB VID `0x1A86`**) operating at
  **115200 baud** (see [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), `LogIntf-USB-UART-Log-Stream`,
  lines 392-419, and [`README.md`](../../../README.md) lines 11-17).
- Enumerate serial ports with `serial.tools.list_ports.comports()` (pyserial, installed as an
  `esptool` dependency). Select ports whose USB vendor id equals `0x1A86`.
- If **exactly one** matching port is found, use it. If **zero** or **more than one** match, do not
  guess and do not prompt — emit `ERROR` (exit code `2`) with a precise message naming what was
  found. Do not add a CLI flag or `.env` key to disambiguate.
- Put this discovery logic in a small, backward-compatible shared helper (e.g. `lib/serial_dut.py`)
  so that other serial-based tests can reuse it. Inject the enumeration function and the serial-open
  function so host tests can supply fakes.

### Boot-log acquisition and device reset

The ESP-IDF version is only printed on the UART during boot, so the DUT must be reset to observe it:

- Reset using
  `esptool.py --port <discovered-port> --before default_reset --after hard_reset read_mac`. Use the
  tool selected during preflight: the resolved PATH executable or the imported package via the
  current Python interpreter's `-m esptool`. Invoke without a shell and with a 20-second timeout.
  Record the command, stdout, stderr, and exit status; execution failure is ERROR. The command
  enters the downloader to read the MAC, then hard-resets into application boot.
- Immediately after esptool exits successfully and releases the port, open the discovered port at
  115200 baud with inactive DTR/RTS. Do not add another reset, sleep, or input flush. Read for at
  most eight seconds, stopping as soon as complete ESP-IDF, ESP32-app, and nRF52 version lines
  arrive. Use an optional shared capture predicate on cumulative text; require complete lines so a
  partial chunk cannot truncate a version suffix. Extract the application framework from
  `cpu_start: ESP-IDF:          v4.2.5`, allowing whitespace and optional ANSI log colors. Do not
  use the 2nd-stage bootloader banner as the application framework version.
- If DTR/RTS reset is not effective on the prepared bench (some units gate download/reset strapping;
  see [`ixit_15-PhyIntf.md`](../ixit_15-PhyIntf.md) `PhyIntf-USB`, lines 30-37), the required
  prepared-environment condition is a Gateway whose CH340 DTR/RTS reset works with `esptool`. If a
  reset cannot be performed or the banner is not observed within the window, emit `ERROR` (exit code
  `2`). **Do not** print instructions asking a human to press a button or replug the device — this
  is an AUTO test.

## Preflight and version behavior

Install `requirements.txt` first: `esptool~=4.8.1` supplies the Python 3.8-compatible tool and
pyserial. Preflight also accepts ESP-IDF's standalone `esptool.py` (or `esptool`) on `PATH` when the
`esptool` module cannot be imported. It runs only the executable's `version` command with a
10-second timeout and records the version and source in evidence. A shell executable on `PATH` does
not necessarily make its module importable by Python; pyserial must be installed in the interpreter
running the test. Use `.venv/bin/python` to select the documented environment. Fall back to a PATH
executable only when the top-level `esptool` module is absent, not when an installed module fails
because of a broken dependency. Keep the helper's tool-selection contract in `lib.serial_dut`; this
runner must not replace it with a local DTR/RTS reset sequence.

- Verify `esptool` is importable/runnable before use. Log the `esptool` version. Do not require a
  specific version, but record it as evidence.
- Verify pyserial is importable. Record its version.
- The serial reset must not flash, erase, or write any partition. Use only read-MAC/reset/monitor
  operations.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_5_2_2_a_<UTC-date-time>.log
```

Use `EvidenceLog` exactly as the existing test does. The log must include:

- test case/unit and UTC start/end/duration;
- discovered serial port and the USB VID/PID that matched;
- `esptool`/pyserial versions;
- the reset command, stdout, stderr, and exit status (or partial output on timeout);
- the full HTTP request/response for authentication, `GET /ruuvi.json`, and `GET /metrics`;
- the raw captured boot-console text and extracted ESP-IDF, ESP32, and nRF52 versions;
- source-labeled normalized values and consistency results for ESP32 and nRF52;
- observed `fw_ver`, `nrf52_fw_ver`, `gw_mac`;
- every GitHub API request URL, HTTP status, and the resolved/normalized tag for each lookup;
- the normalization applied to every version string;
- per-component expected-vs-observed comparison and its result;
- the final per-component and overall verdicts;
- exception types and tracebacks.

Keep terminal output concise; put full evidence only in the log. Print explanatory PASS/FAIL lines
for the identity/default gate, application ESP-IDF comparison (observed and expected versions,
explicitly excluding the bootloader), and each installed firmware tag's published-release check.
Print each component's `/ruuvi.json`, `/metrics`, and UART values with its consistency result. Use
ten progress steps, including metrics acquisition and three-source comparison. For a missing
release, identify the component, installed tag, and confirmed HTTP 404. For an ERROR, name the
affected check (or initialization), exception type, and reason; identify any skipped checks as NOT
RUN. Preserve earlier failure explanations when later errors occur. Label unverified runtime
components INFORMATIONAL. Record these messages in the evidence log without counting them as
additional progress steps, and retain `Overall verdict: <PASS|FAIL|ERROR>` as the final terminal
line.

## Normalization rules

Apply and log a single deterministic normalization to every version string before comparison:

- trim surrounding whitespace;
- compare case-insensitively;
- treat an optional leading `v`/`V` as equivalent (normalize to a single canonical form, e.g.
  lowercase with a leading `v`);
- extract the semantic-version token (`MAJOR.MINOR.PATCH`, optionally with a pre-release suffix) via
  an explicit regex and reject strings that do not match that shape as a protocol/observation error.

Define the normalization as a small pure function and test it deterministically in the host tests.

## DUT preconditions and safety

Run only against a dedicated test gateway:

- reachable over its LAN HTTP interface at `gw_hostname`;
- initially in `lan_auth_default` (so `Admin`/`gw_id` authenticates and `GET /ruuvi.json` returns
  `200`);
- physically connected over USB so the CH340 serial port enumerates and DTR/RTS reset works;
- with no firmware update or remote-configuration operation in progress.

This test is **non-destructive**: it authenticates, reads `/ruuvi.json` and `/metrics`, and resets
the device to read the boot banner. It must never write configuration, flash, or erase anything.
Resetting the DUT briefly interrupts service; do not run against a production gateway.

## Required procedure

### 1. Initialize

1. Create the evidence log before reading `.env`.
2. Preflight `esptool` and pyserial; record versions.
3. Load and validate `.env`.

### 2. Read HTTP device information

1. Construct `GatewayClient` with the same timeout/user-agent pattern as the existing test.
2. Authenticate as `Admin` with `gw_id` and complete the mandatory `GET /ruuvi.json`
   identity/default preflight before resetting the device or making version probes.
3. Reuse that authenticated `GET /ruuvi.json` response (no second read is needed); require `200` and
   a JSON object containing string `fw_ver`, `nrf52_fw_ver`, and `gw_mac`
   ([`schemas/ruuvi_gw_cfg.schema.json`](../../../schemas/ruuvi_gw_cfg.schema.json)).
4. Require returned `gw_mac` to normalize equal to `.env` `gw_mac`. A mismatch is `ERROR` (wrong
   DUT), not a compliance FAIL. Missing, wrongly typed, or malformed device information is also
   ERROR; validate both firmware version strings before serial discovery or reset.
5. Read and validate `/metrics` in the same authenticated session, requiring HTTP 200, one valid
   info sample, both version labels, and matching MAC. Malformed data/identity is ERROR before
   reset.
6. Compare the two HTTP versions per component without marking a full three-source PASS yet.
7. Discover the serial port by USB VID `0x1A86` only after this preflight; ERROR on zero/multiple.

`/info.json` is **hotspot-only** and returns `404` on LAN after authorization succeeds; do not use
it for the authenticated runtime observation. `fw_ver`, `nrf52_fw_ver`, and `gw_mac` come from the
runtime device-info object emitted by
[`gw_cfg_json_add_items_device_info()`](../../../main/gw_cfg_json_generate.c#L69) through
[`GET /ruuvi.json`](../../../docs/http_api.md#get-ruuvijson). `/status.json` instead returns network
connection status and must not be treated as the outbound HTTP statistics report.

### 3. Capture UART versions and compare all three sources

1. Reset the DUT over the discovered port with esptool
   `--before default_reset --after hard_reset read_mac`.
2. Read at 115200 baud, stopping on complete required lines or the eight-second deadline.
3. Extract the ESP-IDF version from the application's `cpu_start: ESP-IDF:` startup line.
4. Extract ESP32 `cpu_start: App version:` and nRF52 `Firmware on nRF52:`; compare each with both
   HTTP sources after normalization. Record each consistency outcome independently.
5. Missing serial port, failed reset, or missing required version output is `ERROR`.

### 4. Compare against IXIT and GitHub release evidence

For each component:

1. **ESP-IDF framework (`NetSecImpl-Framework-Stack`).** Require the boot-banner ESP-IDF version to
   normalize equal to `v4.2.5` (IXIT-declared and matching `EXPECTED_IDF_VERSION` in
   `CMakeLists.txt` line 24). Mismatch is a **FAIL**.
2. **ESP32 application firmware (`ruuvi.gateway_esp.c`).** Query
   `https://api.github.com/repos/ruuvi/ruuvi.gateway_esp.c/releases/tags/<fw_ver>` (using the exact
   observed tag, normalized). `200` -> the installed version is a real published release (**PASS**);
   `404` while GitHub is reachable -> the installed version is not a published release (**FAIL**).
3. **nRF52 co-processor firmware (`ruuvi.gateway_nrf.c`).** Query
   `https://api.github.com/repos/ruuvi/ruuvi.gateway_nrf.c/releases/tags/<nrf52_fw_ver>`; classify
   exactly as in step 2.
4. Record mbedTLS and Web-UI as **informational** (no runtime version string; not a FAIL).

### 5. Offline / network error classification

- Any GitHub request that fails to connect (DNS failure, timeout, connection reset) is `ERROR` (exit
  code `2`), never `FAIL`. The DUT may be correct even when the test host is offline.
- HTTP `403`/`429` from GitHub (rate limiting) is `ERROR`, not `FAIL`. Log the `X-RateLimit-*`
  headers if present.
- Confirm a tag `404` with a successful repository-identity lookup before reporting identification
  FAIL. Unconfirmed repository availability is ERROR. Preserve earlier component failures when a
  later acquisition or release lookup errors.
- Use a bounded connect/read timeout and set a descriptive `User-Agent` on GitHub requests.

## Verdict and exit codes

Emit a per-component result for:

- `NetSecImpl-Framework-Stack` (ESP-IDF boot-banner match);
- `ruuvi.gateway_esp.c` application release existence;
- `NetSecImpl-CoProcessor-Firmware` (`ruuvi.gateway_nrf.c` release existence);
- ESP32 and nRF52 firmware version consistency across JSON, metrics, and UART;
- DUT `gw_mac` / `.env` `gw_mac` consistency (setup);
- informational components (mbedTLS, Web-UI).

Overall PASS requires both three-source comparisons, the ESP-IDF match, and both release-existence
checks to pass, with no setup or network error. Use:

| Exit code | Meaning                                                                                                                |
| :-------- | :--------------------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                                           |
| `1`       | An identification mismatch was proven (sources disagree, ESP-IDF is wrong, or a confirmed published release is absent) |
| `2`       | Invalid setup, wrong DUT, serial/reset failure, missing boot banner, or GitHub unreachable/rate-limited                |

## Live-run findings and troubleshooting

The 2026-09-22 log `test_5_5_2_2_a_20260922T132622.117207Z.log` contained the installed nRF52
version at boot timestamp 5234 ms, but the parser required lowercase `nrf52fw` and missed the actual
`nRF52Fw` tag. Match the stable `### Firmware on nRF52:` message instead of requiring a specific
tag. The corrected capture stops on complete required lines, with an eight-second maximum; the
generic serial helper's default remains 30 seconds. Legacy unprefixed messages under either tag
spelling remain supported. Never use `Firmware on FatFS` as a substitute.

The user-confirmed 2026-09-21 run completed with the expected FAIL on firmware `v1.16.3`: the
application reported ESP-IDF `v4.2.2`, while the test requires `v4.2.5`. Both published-release
checks passed (`v1.16.3` for ESP32 and `v2.0.0` for nRF52). The reset and two-second capture worked
with ESP-IDF's standalone `esptool.py` `3.1-dev`. The local evidence file is
`test_5_5_2_2_a_20260921T180906.127289Z.log`; this result validates acquisition on that bench, not
all hardware setups or a passing compliance verdict. See the assessment's reusable
[identification requirements](../Functional_Test_Automation_Assessment.md#runtime-component-identification-and-serial-acquisition-55-2-2-a)
for guidance independent of any particular test implementation. That earlier two-second capture
ended before the nRF52 version line and predates the three-source requirement. It is not live
evidence of the new consistency checks; the extended, early-exit capture is required to obtain all
observations.

The earlier hand-written DTR/RTS reset left this gateway waiting in ROM download mode. If a run
reports `DOWNLOAD_BOOT` without an application line, treat it as a reset/acquisition ERROR; waiting
30 seconds is not a fix. A successful esptool exit alone also does not prove application startup:
the required `cpu_start: ESP-IDF:` line must be captured. Preflight failure, reset failure, and an
observed version mismatch are different outcomes, not interchangeable reasons for FAIL.

The clearer terminal reporting added after that live run includes lines such as:

```text
FAIL: Application ESP-IDF: observed v4.2.2, expected v4.2.5 (IXIT; bootloader version is not checked)
PASS: ruuvi.gateway_esp.c: installed v1.16.3 is a published GitHub release
PASS: NetSecImpl-CoProcessor-Firmware: installed v2.0.0 is a published GitHub release
Overall verdict: FAIL
```

This is an illustrative excerpt of the updated output, not the earlier live transcript. No
`espidf_ver` field was added to `/ruuvi.json`; that was a proposed future improvement only. The test
continues to obtain application ESP-IDF from UART, not from the host SDK/tool version or an inferred
firmware-to-IDF mapping.

Run offline implementation checks from `cra/303645/tests` with
`.venv/bin/python -m unittest --verbose test_test_5_5_2_2_a.py`. These checks access no hardware or
network and provide no live compliance verdict.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py` and use `unittest` with fakes for the HTTP session, the serial
transport, and the GitHub lookup. Fake HTTP responses and expected wire values must be independent
fixtures and must not invoke the client code under test to generate their expectations. Cover at
least:

- exact expected constants (ESP-IDF `v4.2.5`, GitHub repo URLs, CH340 VID `0x1A86`, 115200 baud);
- runner handling of single-port discovery and zero/multiple-port setup errors; direct discovery and
  serial helper tests belong only in `test_lib.py`;
- normalization function: `v`/no-`v`, case, whitespace, and a malformed string;
- application startup-line extraction from realistic UART output and from noise; an old bootloader
  must not fail a matching application version or satisfy the check when the application line is
  absent;
- full PASS path with faked `/ruuvi.json`, faked boot banner `v4.2.5`, and faked GitHub `200`s;
- independent metrics/JSON/UART fixtures, each source disagreeing for either component, malformed or
  duplicate metrics, wrong metrics MAC, authenticated request order, delayed/chunked UART lines,
  early exit/deadline behavior, FatFS-image exclusion, and prior consistency FAIL followed by ERROR;
- the ANSI-colored, mixed-case installed-nRF52 line at 5234 ms, legacy unprefixed messages under
  either supported tag, and completion only after newline-terminated required records; retain the
  eight-second deadline and early exit instead of reintroducing a fixed 30-second wait;
- application ESP-IDF `v4.2.2` -> FAIL even with a retained bootloader using a different IDF;
- ROM download mode without an application startup line -> ERROR, with a precise reset-mode
  explanation;
- `fw_ver` tag `404` (GitHub reachable) -> FAIL; `nrf52_fw_ver` tag `404` -> FAIL;
- GitHub connection error and `403`/`429` -> ERROR, never FAIL;
- `gw_mac` mismatch vs `.env` `gw_mac` -> ERROR;
- serial reset failure / no banner -> ERROR;
- mbedTLS and Web-UI recorded informational and never cause FAIL;
- evidence-log naming/content and terminal progress match the existing pattern;
- exit-code aggregation for PASS/FAIL/ERROR.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs without command-line arguments and loads `.env` from the current directory.
- It reuses the shared library and the same execution/logging pattern as `test_5_1_1_2_b.py`.
- It discovers the CH340 serial port automatically and errors deterministically on zero/multiple.
- It cross-checks ESP32/nRF52 firmware versions across `/ruuvi.json`, `/metrics`, and installed UART
  observations, and separately reads application ESP-IDF from the boot log.
- It verifies ESP-IDF `v4.2.5` and that `fw_ver`/`nrf52_fw_ver` are published GitHub release tags,
  using only the unauthenticated public GitHub API and never assuming the latest release is
  installed.
- Offline/rate-limited GitHub and any serial/reset failure produce ERROR, never a false FAIL.
- No firmware, configuration, or flash content is modified.
- Existing and new host-side implementation tests pass under Python 3.8.

## Ambiguities to record (do not silently resolve)

- The nRF5 SDK **v15.3.0** version declared in `IXIT 12-NetSecImpl` is build provenance and is
  **not** observable from the running DUT; only the `ruuvi.gateway_nrf.c` co-processor release tag
  (`nrf52_fw_ver`, pinned to `v2.0.0` in `CMakeLists.txt` line 30) is observable. Record this and
  treat the SDK version as informational.
- mbedTLS "Custom Backport" and the Web-UI bundle expose no runtime version string over HTTP; their
  identification is a build-manifest/source concern, recorded as informational.
- The GitHub existence check confirms a version is _a_ published release, not that it is the exact
  audited build; note this limitation in the log.

## Implementation references

- [`Self_Assessment_Test_Group_5_5-2.md`](../Self_Assessment_Test_Group_5_5-2.md), lines 50-75.
- [`ixit_12-NetSecImpl.md`](../ixit_12-NetSecImpl.md), lines 156-164.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), `LogIntf-USB-UART-Log-Stream`.
- [`ixit_15-PhyIntf.md`](../ixit_15-PhyIntf.md), lines 9-37 (`PhyIntf-USB`).
- [`docs/http_api.md`](../../../docs/http_api.md#get-ruuvijson), `GET /ruuvi.json` and hotspot-only
  `GET /info.json`.
- [`schemas/ruuvi_gw_cfg.schema.json`](../../../schemas/ruuvi_gw_cfg.schema.json).
- [`main/http_server_cb_on_get.c`](../../../main/http_server_cb_on_get.c),
  `http_server_resp_json_ruuvi()`.
- [`main/gw_cfg_json_generate.c`](../../../main/gw_cfg_json_generate.c),
  `gw_cfg_json_add_items_device_info()`.
- [`CMakeLists.txt`](../../../CMakeLists.txt), lines 24 and 30.
- [`README.md`](../../../README.md), lines 11-17 (CH340) and 250-297 (esptool).
- [`cra/303645/tests/test_5_1_1_2_b.py`](../tests/test_5_1_1_2_b.py).
- [`cra/303645/tests/test_test_5_1_1_2_b.py`](../tests/test_test_5_1_1_2_b.py).
- [`cra/303645/tests/lib/config.py`](../tests/lib/config.py).
- [`cra/303645/tests/lib/evidence.py`](../tests/lib/evidence.py).
- [`cra/303645/tests/lib/gateway.py`](../tests/lib/gateway.py).
- [`cra/303645/tests/lib/models.py`](../tests/lib/models.py).

## Runner naming

Use `FunctionalTest_5_5_2_2_a` and `execute_test_5_5_2_2_a`; retain `main() -> int` as the
zero-argument executable entry point.

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
