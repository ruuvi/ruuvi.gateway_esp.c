# Functional Test Automation Feasibility Assessment (ETSI EN 303 645 / TS 103 701)

## Purpose

This is a **preliminary automation-feasibility assessment** of every **test unit** declared inside
every **functional** test case across the `Self_Assessment_Test_Group_*.md` documents for the Ruuvi
Gateway (DUT) under ETSI EN 303 645 and the ETSI TS 103 701 test specification. It classifies
whether each individual functional **test unit** could, in principle, be exercised and evaluated by a
**Python test script** — it does **not** assert that every classified unit has an implementation, nor
does it re-judge the pass/fail verdicts recorded in the source documents.

The goal is to support a later stage in which **one independent coding-agent task is created per
automatable functional test unit**. Units classified `AUTO` (and the scriptable scope of `HYBRID`
units) are the primary candidates for that follow-up work; `MANUAL`, `N/A`, and `BLOCKED` units
document why full automation is not currently in scope. Conceptual test cases and conceptual test
units are intentionally excluded — only the test units defined inside test-case headings explicitly
marked `(functional)` in the source are represented, **one row per test unit**. This document is
branch-independent guidance for creating or updating tests, not an inventory of implemented scripts.
Test IDs identify requirements, not prerequisites on particular test files being present or merged.

## Common automation execution contract

Future live-DUT Python tests use the following repository-wide conventions:

- Tests take no command-line parameters. They load `gw_id`, `gw_mac`, and `gw_hostname` from `.env`
  in the current working directory and write evidence beneath that directory's `logs/` folder.
- Test runner classes and execution functions include the test ID in their symbol names, using
  `FunctionalTest_<test_id>` and `execute_test_<test_id>`, so IDE call hierarchies identify the
  owning test script.
- `gw_id` is the gateway's default password; the fixed administrative username is `Admin`.
- `.env` `gw_mac` is the nRF52 identity MAC exposed by authenticated `GET /ruuvi.json`. The ESP32
  Ethernet and Wi-Fi interfaces have different MACs that are not stored in `.env`; a future ESP32
  Bluetooth interface must be identified separately. For link-layer capture, resolve the verified
  DUT IP to its active interface MAC using an on-link ARP/neighbor observation or equivalent trusted
  context. A routed or ambiguous mapping is a setup ERROR.
- For 5.6-2-2-A, DHCP inspection needs a visible DUT exchange. The runner starts `tcpdump`, waits
  until it is listening, then resets the dedicated DUT through its CH340 USB serial bridge to
  observe startup DHCP. A switched host port may still lack visibility; use a router-side capture,
  switch mirror, or TAP when needed. Posting `eth_dhcp=false` with a static IP stops Ethernet DHCP;
  there is no JSON field selecting a DHCP server, and Wi-Fi station static-IP settings are not parsed from
  `/ruuvi.json`. Starting a DHCP server on the ordinary Ubuntu host cannot direct the DUT to it
  and may compete with the LAN's server. Preserve the factory-default DUT and treat missing capture
  evidence as ERROR under the task's verdict rule.
- Before executing test-specific probes or mutations, every live-DUT test authenticates, reads
  `GET /ruuvi.json`, loads
  [`gw_cfg_default_gen_ui.json`](../../gw_cfg_default/gw_cfg_default_gen_ui.json), and
  compares every configuration field relevant to that test with its factory default. Authentication
  tests always compare `lan_auth_type`, `lan_auth_user`, `lan_auth_api_key_use`, and
  `lan_auth_api_key_rw_use`. A missing, wrongly typed, or non-default value is invalid setup: print the
  shared physical factory-reset instruction and finish without attempting to manufacture the baseline.
- A non-200 default `Admin`/`gw_id` login must mark the reset requirement before raising setup
  ERROR. When configuration is available, validate its exposed MAC against `.env` before checking
  default fields. A mismatched or malformed identity must not produce reset advice; if no MAC is
  exposed, a non-default baseline remains ERROR without reset advice for an unverified device.
- `gw_cfg_default_gen_ui.json` is generated with `gw_cfg_json_generate_for_ui_client()` and therefore
  already matches the public `/ruuvi.json` representation. Tests compare it directly and must not
  derive a public view from the raw saved configuration or handle hidden password/key values.
- Every run writes a local evidence log named with the test-script prefix, a UTC date/time, and
  `.log` extension. Evidence logs may contain credentials, cookies, authorization headers, hashes,
  ECDH material, and complete HTTP request/response data and therefore must remain outside version
  control.
- Console output prints the evidence-log path once as `Open log file: <path>`, then reports each
  long-running phase as `[Step N out of M] <description>`. Detailed protocol data and secrets remain
  in the evidence log rather than being repeated on the console. Each formatted progress line is
  also written to the evidence log with its standard UTC timestamp prefix so phase boundaries are
  easy to locate.
- Print concise reasons as well as the overall verdict: identify the failing component and its
  expected/observed values, or the affected check and exception reason for ERROR. Preserve earlier
  FAIL explanations if a later error stops execution, and identify skipped checks as NOT RUN.
  Result explanations are evidence too, but do not increment numbered progress phases.
- Progress phases should be small enough to identify the currently executing authentication mode.
  In particular, unauthenticated, Basic, Digest, and bearer route matrices are separate steps rather
  than one combined long-running step.
- A physical gateway is still required for a functional-test verdict. Mock-based host tests only
  validate the Python implementation.
- The implementation-derived [firmware HTTP API reference](../../docs/http_api.md) is the canonical
  route and authorization inventory for HTTP-based tests.
- Keep route-specific interface restrictions and authentication precedence in status expectations.
  For 5.1-1-2-B, keep `GET /info.json` in the 26-route inventory. Missing/Basic/Digest credentials
  without an authorized cookie return 302 in default auth mode before dispatch; disabled bearer
  probes require 401. The hotspot-only handler returns 404 on LAN only after authorization
  succeeds, so 404 must fail these negative probes.
- Every rejected security assertion must set the associated mechanism to FAIL in both
  `RunResult.outcomes` and final evidence, including inventory length/uniqueness/set/coverage checks
  and final configuration/status verification. Setup, transport, and protocol exceptions remain
  overall ERROR. An overall verdict alone is insufficient evidence of individual outcomes.
- Track mechanism ownership explicitly through multi-phase tests. Complete a mechanism only after
  its own checks finish; retain its PASS if a later mechanism fails, and replace PASS with FAIL if
  its own later control fails. Provisioning plus read-back and required login verification form one
  setup phase: any exception marks that setup outcome ERROR before mandatory restoration.
- Complete safer probes across all schemes before potentially mutating probes. For 5.1-2A-2-B this
  includes negative password GETs and positive RO/RW GETs before password POSTs, followed by RO POST
  denial and RW no-op POST. Configuration hash failures after these writes are RW failures.
- Require independent fake-server validation of complete login bodies, per-session cookies and
  outstanding challenges. Include a full run through the real client, exact ordered HTTP matrices
  with bodies, and late failure/abort/recovery regressions. Basic/Digest setup fixtures must return
  their real challenge schemes so exception-based recovery is exercised.
- Carry recovery advice into evidence and terminal output. Validate available identity before
  advising a reset for non-default configuration. Save candidate credentials before a potentially
  applied change and retain previous candidates until verification. Derive reset instructions from
  both restart and boot-time erasure: the 200 ms red on/off signal confirms completion, while the
  approximately eleven-second duration is only an observation.
- Inspect the target branch before relying on a referenced module, task, or predecessor test.
  Reuse compatible infrastructure where available; a test missing from that branch is not a reason
  to substitute another unit's requirements. Keep documentation links relative to each document.

### Shared Python test infrastructure

New functional tests should reuse compatible modules under [`tests/lib/`](tests/lib/README.md)
instead of copying setup, inventory, progress, or evidence-log logic. The following responsibility
map describes the shared architecture; verify availability and API compatibility on the target
branch, and implement any missing reusable capability there without depending on an unmerged test:

| Module | Reusable responsibility |
|:-------|:------------------------|
| [`http_api.py`](tests/lib/http_api.py) | HTTP protocol descriptors (`HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`), the ordered 26-route `API_INVENTORY`, its canonical set assertion, and the immutable `ApiRoute` model. |
| [`models.py`](tests/lib/models.py) | `DutConfig`, `RunResult` (including an optional operator-recovery message), and `ProgressReporter`; each test supplies its own total number of progress steps. |
| [`config.py`](tests/lib/config.py) | Strict, non-executing `.env` parsing; gateway ID, nRF52 identity MAC, and hostname validation; and loading/selecting public factory defaults directly from `gw_cfg_default/gw_cfg_default_gen_ui.json`. |
| [`evidence.py`](tests/lib/evidence.py) | Exclusive timestamped log creation, structured records, wire request/response blocks, exceptions, and final timing. Callers supply the test-specific filename prefix. |
| [`errors.py`](tests/lib/errors.py) | Common invalid-setup, gateway-connection, malformed-protocol, and unexpected-authentication-mode exceptions so infrastructure failures consistently produce ERROR rather than a security verdict. |
| [`gateway.py`](tests/lib/gateway.py) | `GatewayApi`, `AuthMech`, and gateway-configuration descriptors plus `GatewayClient` for logged HTTP requests, typed JSON decoding, interactive Ruuvi authentication with ECDH/challenge-response, live-syntax Digest challenge parsing, fresh sessions, and shared Basic/Digest Authorization builders. |
| [`serial_dut.py`](tests/lib/serial_dut.py) | Import/PATH serial-tool preflight, deterministic CH340 discovery, bounded esptool reset, and UART capture with injected subprocess/serial boundaries; version interpretation and verdicts remain in the runner. |

`GatewayClient.request()` supports query parameters and either JSON or raw request bodies, prepares
and logs the exact request before transmission, disables redirects unless a caller explicitly enables
them, logs the response before returning it, and converts Requests transport failures into
`GatewayConnectionError`. `response_json()` handles malformed JSON and can optionally enforce the
expected top-level Python type. `authenticate_interactive()` performs the reusable `/auth` session,
P-256 ECDH, and password-response protocol, then returns the session, parsed challenge/auth data,
login response, and derived AES key in an `InteractiveAuthResult`; staged preparation, transmission,
and parsing helpers let timing-sensitive tests measure GET and POST wire operations without including
local cryptographic work. It does not decide whether the login status is correct for a particular
ETSI case. `authorization_header_basic()` and
`authorization_header_digest()` are the single implementations of those Authorization formats.
The Digest builder accepts the parsed challenge, request method, and request path so both positive
and negative probes use the same challenge-aware calculation.

Test-specific runners retain requirements, security assertions, route ordering, and PASS/FAIL/ERROR
aggregation. This separation prevents a shared communication helper from silently imposing one test
unit's expected auth mode or status codes on future tests. Definitions used by only one case belong
inside that test's runner rather than under `tests/lib/`.

### Coding and test-architecture conventions

AI agents must always run `ruff check cra/303645/tests` from the repository root (or `ruff check .`
from the test directory) before changing CRA automation or its guidance and again before handoff,
including documentation-only tasks. Check the whole subtree, fix all findings using the repository
configuration (including pre-existing findings), and require a final clean run. Report the initial
finding count and final result. Do not defer cleanup unless the user explicitly requests it.
Preserve Python 3.8 compatibility and review fixes for behavioral changes. Do not disable valid
rules or apply unsafe fixes blindly. For the current review, do not run `ruff format` or reformat
whole files; lint corrections and behavioral fixes should remain focused. Explicitly report any
lint backlog that the user defers. Follow [`tests/AGENTS.md`](tests/AGENTS.md) for the host-side
regression suites and library coverage gate; lint success is not behavioral verification.
Postponed annotations permit modern annotation syntax on Python 3.8, but runtime expressions must
remain compatible. Keep documented, line-specific exceptions for intentional recovery boundaries
and false positives involving `EvidenceLog.exception(error)`; never remove error classification,
restoration, or traceback evidence solely to silence a lint rule.

The Python tests prioritize IDE navigation and human auditability. Protocol and configuration
vocabularies must have one declaration point and expose related choices as class attributes so typing
`.` in an IDE shows the valid values:

- `GatewayApi` declares every public gateway HTTP path.
- `AuthMech` declares every IXIT authentication-mechanism identifier.
- `GatewayCfgDesc` declares gateway configuration field names, while value classes such as
  `GatewayCfgLanAuthType` declare all supported string values.
- `HttpMethod`, `HttpHeader`, and `HttpAuthScheme` declare shared HTTP strings.
- `HttpStatus` members include both the numeric code and description, for example
  `HttpStatus.C_200_OK` and `HttpStatus.C_401_UNAUTHORIZED`.

Call sites must use these members rather than repeat raw field names, enum values, paths, methods,
headers, schemes, or status integers. Free-form values such as generated credentials and test-specific
evidence text remain ordinary strings. Dynamic strings use f-strings; do not use `str.format()` or
concatenate string literals with variables.

Stable multi-field concepts use named immutable models instead of anonymous tuples or loosely typed
dictionaries. `ApiRoute` exposes `.method` and `.path`; campaign credentials use separate interactive
and bearer classes; bearer access checks, host-fixture request records, assertions, campaign samples,
retry records, and result records also expose named fields. Nested evidence such as campaign running
counts is represented by nested dataclasses rather than nested dictionaries. This avoids numeric tuple
indexing and raw field-name keys, makes constructor mistakes visible, and lets IDE completion show the
available fields.

Do not pass an ad hoc dictionary to an internal method when its keys define a stable record schema.
Pass a dedicated typed model instead. Dictionaries remain appropriate only at genuine mapping
boundaries such as gateway JSON objects, HTTP headers, and parsed configuration, where keys must use
the central descriptor classes. `EvidenceLog` is the serialization boundary: it accepts typed
dataclass instances, including nested records and lists of records, and converts them to JSON without
requiring callers to build dictionaries.

Each live-DUT script has a paired `test_test_<test_id>.py` implementation suite with deterministic
fake sessions and gateway responses. The host suite validates branching, retries, timing boundaries,
protocol syntax, restoration, and evidence generation, but cannot produce an ETSI verdict. The paired
fixtures must validate the complete default-login body with an independently calculated password
response before granting session access. Regression cases must include failed default login recovery,
wrong-device identity combined with non-default fields, each inventory/final verification assertion,
transport/protocol errors, and route-specific LAN responses. Check mechanism outcomes and logged
evidence as well as the overall verdict. Discover the offline implementation suites available on
the target branch rather than assuming that specific predecessor files exist:

```bash
cd cra/303645/tests
.venv/bin/python -m unittest discover \
  --verbose --start-directory . --pattern 'test_test_*.py'
```

Require nonzero discovery and confirm that it includes the suite for the test being created or
updated. Do not broaden the pattern to `test_*.py`, which also matches live-DUT scripts.

### Live-DUT authentication timing and recovery rules

Embedded-gateway HTTP latency is not stationary. Periodic cloud uploads and other firmware tasks can
delay an isolated response, including beyond the client's read timeout. They can make authentication
slower but cannot make a firmware-enforced delay faster. Timing verdicts must therefore preserve all
raw samples while applying outlier handling only in the adverse direction:

- upper-bound response averages may discard exactly one slowest sample;
- aggregate throughput may discard exactly one slowest sample;
- minimum-delay assertions must always use the raw minimum and discard no sample;
- retries after transport errors do not count as completed logical attempts, and retry delays must
  never be included in a way that can mask a fast authentication response.

Multi-request authentication flows require explicit timing boundaries. Ruuvi interactive
authentication measures `GET /auth` and `POST /auth` separately, outside local ECC/hash preparation
and response parsing. Successful controls obtain a challenge for each POST. Failed Default and
user-defined campaigns issue one initial GET, then chain the fresh challenge and cookie returned by
each failed POST into the next password guess; failed throughput therefore uses POST timings only.
Digest obtains its unauthenticated challenge before timing and measures the subsequent request
carrying `Authorization: Digest ...`. The first Digest request correctly has no Authorization header.

Host fixtures must reproduce live wire syntax and storage limits rather than idealized protocol
syntax. In particular, the firmware emits space-separated quoted Digest challenge parameters, and
the stored Basic token is limited to 64 characters. Generic comma-oriented header parsing and
overlong fake credentials can pass host tests while failing on the DUT.

Destructive tests must communicate unrecovered state to the operator. If the initial authentication
mode is not the required default, or final restoration cannot be verified, the last console message
must warn that factory reset erases saved local configuration, credentials, tokens, and uploaded
certificates/private keys and interrupts connectivity; back up needed settings first. It does not
delete cloud accounts or already-forwarded data. The corrected recovery instruction is to hold
CONFIGURE through LED off and restart until repeating red 200 ms on/off completion feedback,
normally about 11 seconds after the initial press, then release the button. Release triggers
another restart, after which the configuration hotspot opens. If completion feedback never
appears, do not assume erasure. Hotspot, lost LAN access, LED off or solid red alone are
insufficient; release after completion cannot undo the erase. Eleven seconds is user-reported
approximate elapsed time, not a timer or safe boundary for shorter holds: a distinct five-second
timer requests restart and the boot-held check selects erasure. See
the [warned local procedure](ixit_25-DelFunc.md#initiation-and-interaction). Transport timeouts
during restoration are ERROR, never PASS.

Partial configuration behavior must be treated as parser-specific, not inferred from a top-level
"copy current configuration" design. A test should send only fields within its intended mutation
scope, then compare the complete restored configuration against two stable pre-mutation reads.
Authentication-only tests should omit unrelated MQTT fields; if firmware defaults or rewrites an
omitted value, the full-state hash must expose that side effect instead of silently normalizing it.

### Reliability rules for decoded protocol data

Values returned by `.env`, HTTP, or JSON parsing cross a serialization boundary and must be compared
by **type and value**, not by Python object identity. Use `type(actual) is type(expected)` when the
wire type is normative, followed by `actual == expected`. This is especially important for mixed
expectation tables containing both strings and booleans: `is` is appropriate for `None` and may be
used deliberately for the `True`/`False` singletons, but it must not be generalized to strings,
numbers, lists, or dictionaries. Conversely, value comparison alone is insufficient where the JSON
type matters because Python considers `False == 0` and `True == 1`.

Host-side protocol fixtures must exercise the same serialization boundary as the live DUT. Do not
construct all fake response payloads exclusively from the same string literals used by assertions;
CPython may intern those literals, causing an incorrect identity comparison to pass by accident.
At least one test for every decoded configuration validator should round-trip its payload through
`json.dumps()`/`json.loads()` (or decode representative response text), then verify:

- an equal, independently decoded value is accepted;
- a different value is rejected;
- a value with the wrong JSON type is rejected even when Python considers it equal.

Code review for live-DUT tests must search validators for `is`/`is not` and require an explicit
singleton rationale for every occurrence. Before beginning a destructive or long-running campaign,
the implementation tests must cover all preflight validators using decoded payloads, because a mock
that shares literal objects with production expectations is not an adequate substitute for the
actual HTTP/JSON boundary.

### Runtime component identification and serial acquisition (5.5-2-2-A)

Use these requirements when creating or updating functional component-identification automation,
regardless of which tests are present on the target branch. The AUTO classification assumes a
dedicated default-configured LAN gateway, exactly one connected CH340 bridge (USB VID `0x1A86`),
working esptool reset wiring, and public GitHub access. Reset interrupts service; the identification
procedure must not write configuration or flash.

- **Correct HTTP source:** authenticate and reuse `GET /ruuvi.json` for `fw_ver`, `nrf52_fw_ver`,
  and `gw_mac`, validating identity/defaults before serial discovery or reset. `/status.json` is
  network status, not the outbound statistics payload described by `ruuvi_gw_status.schema.json`;
  it does not supply `ESP_FW`, `NRF_FW`, or `DEVICE_ADDR`. `/info.json` is hotspot-only.
- **Three-source consistency:** after the default/identity gate, use the same authenticated
  session for `GET /metrics`. Require exactly one `ruuvigw_info` sample with value 1 and labels
  `mac`, `esp_fw`, and `nrf_fw`; validate `mac` against the DUT identity. Compare ESP32 `fw_ver`
  from JSON with metrics `esp_fw` and UART `cpu_start: App version:`. Compare JSON `nrf52_fw_ver`
  with metrics `nrf_fw` and UART `### Firmware on nRF52:`, independently of logger-tag spelling
  (`nRF52Fw` and `nrf52fw` differ across firmware revisions).
  Normalize all versions consistently. Label order/comments must not affect parsing, but duplicate
  samples/labels, missing fields, malformed versions, or missing UART observations are ERROR.
  A well-formed version disagreement is FAIL, independently for each component. A published-release
  PASS must not overwrite it. Log and print the values from all three sources.
- **Host tool availability:** an ESP-IDF installation can expose `esptool.py` on PATH while
  `import esptool` fails in the test interpreter. Preflight must accept the imported module or fall
  back to `esptool`/`esptool.py`, record its version/source, and still require importable pyserial.
  Fall back only when the top-level module is absent, not to hide a broken internal dependency.
  A host tool's version is not evidence of the framework version running on the DUT.
- **Reset and capture:** use the discovered port with
  `--before default_reset --after hard_reset read_mac`; wait for esptool to release the port,
  then immediately capture UART at 115200 baud until complete ESP-IDF, ESP32-app, and installed-nRF52
  version lines arrive, with an eight-second maximum rather than a fixed wait. The nRF52 observation can
  arrive later than the early ESP-IDF line; a two-second capture is not sufficient for all three.
  Do not use `Firmware on FatFS`, which identifies a stored update image, as installed nRF52 evidence.
  Give the reset subprocess a separate
  20-second timeout and log its command, output, and status. Open UART with inactive DTR/RTS and
  no additional reset, sleep, or input flush. Hand-written control-line sequences can leave the
  device in `DOWNLOAD_BOOT` / `waiting for download`; without an application startup line this is
  acquisition ERROR, not a version FAIL, and a longer capture does not make the application start.
  A successful reset command alone is not proof of application startup. No manual-button fallback
  is part of this AUTO procedure.
- **Application versus bootloader:** parse `cpu_start: ESP-IDF:`, allowing whitespace and ANSI
  colors, and compare against IXIT `v4.2.5`, cross-checked with `EXPECTED_IDF_VERSION` in
  `CMakeLists.txt`. A retained bootloader's `ESP-IDF v4.0.3 2nd stage bootloader` banner is not the
  application framework version and must neither fail nor satisfy this check.
- **Release-check scope:** the installed ESP32 and nRF52 tags must identify published releases
  in their respective repositories; this is not a latest-release comparison. A tag 404 is FAIL
  only after repository availability/identity is confirmed. Network, rate-limit, malformed-response,
  and serial failures are ERROR. Earlier component FAILs survive a later overall ERROR. mbedTLS,
  Web-UI, and nRF5 SDK provenance remain informational, not runtime-verified versions; published
  release existence does not establish the exact audited build or cryptographic security.

Regression fixtures must distinguish bootloader and application versions. For example, a bootloader
using `v4.0.3` plus an application reporting `v4.2.2` must fail an application expectation of
`v4.2.5`; the same bootloader plus application `v4.2.5` must pass that comparison. Published ESP32
and nRF52 releases do not override an application-IDF mismatch. Missing application output must
produce ERROR, not be replaced by a version inferred from the host SDK, repository pin, or firmware
release number. Include a prior version FAIL followed by a release-lookup ERROR, preserving both
explanations in terminal output and evidence.
Also cover each of the three sources disagreeing for either firmware component, partial serial
chunks, delayed nRF52 output, and HTTP-source mismatches preserved after a later UART error.

Do not assume an `espidf_ver` field exists in `/ruuvi.json`. An API-based alternative would require
an explicitly implemented and documented runtime field on the firmware under test, plus a defined
compatibility policy for older firmware. UART acquisition remains necessary when that field is
unavailable. Keep live-run logs and implementation-status records separate from these reusable
requirements; offline fixtures establish automation behavior, not a live compliance verdict.

## Status vocabulary and legend

Column **Automation class** uses the following short codes, applied **per test unit** (a single
functional test case may split across several classes):

- **AUTO** — Can be fully exercised and objectively evaluated by a Python script with no human
  interaction, once the documented test environment (powered DUT, network access, wired
  serial/flash harness as applicable) is available.
- **HYBRID** — A script can perform/measure substantial portions, but a manual setup, physical
  action, visual judgment, account/provisioning step, or external lab dependency remains.
- **MANUAL** — The unit primarily requires human action/judgment, physical inspection,
  organizational/documentation evidence, or a workflow that is not meaningfully automatable by a
  Python test script.
- **N/A** — The unit/source is declared not applicable to the DUT by the source document.
- **BLOCKED** — The unit could theoretically be automated but cannot currently be executed because
  a required interface, fixture, credential, external service, hardware capability, or observable
  outcome is unavailable/undefined. Not used for ordinary manual setup (that is `HYBRID`).

Feasibility is classified against the DUT reality: an embedded ESP32 + nRF52 device with
Wi-Fi/Ethernet/BLE, a web configuration UI and REST API, TLS, OTA/USB firmware update with
secure-boot signature verification, physical power/reset and a factory-reset button, NVS/flash
storage, external Ruuvi Cloud services, and accompanying IXIT/organizational documentation.

The **Unit** column preserves the source designator (`A`/`B`/`C`… or `a`/`b`/`c`… exactly as the
source labels it). The **Test unit** column preserves the exact or faithfully normalized source
test-unit title; where a source combines units under one heading (e.g. "Test Units A & B"), each
enumerated unit is still given its own row using the lettered sub-goals declared in that test case's
purpose.

## Assessment summary

- Functional test cases listed: **57** (no conceptual cases).
- Explicit functional test units enumerated: **117** (one row each).
- Functional test cases with no explicit test units (fallback `—` rows): **0**.
- Final table data rows: **117**.
- Class counts: **AUTO 18**, **HYBRID 66**, **MANUAL 22**, **N/A 9**, **BLOCKED 2**.

These totals count the current table, correcting the prior stale 58-case/119-unit summary.
The 5.5-2-2-A follow-up changes no row classifications and does not constitute a new completeness
audit against all source test units. The 5.11-1-2-A review moves one unit from AUTO to HYBRID
because its supported workflow requires hotspot and reset interactions; this changes feasibility
counts, not the source document's recorded compliance verdicts.

## Functional test automation feasibility

For interface-independent RW authorization checks, use `POST /ruuvi.json` with exactly `{}` and verify
the canonical configuration hash before and after the request where the case requires the empty-body
probe. Unit 5.11-1-2 A has a user-authorized exception: explicitly preserve the three observed MQTT
fields to work around #1650 and retain the complete comparison. This exception does not change
other cases' required probes. Do not use `GET /ap.json` as a generic
RW probe: Wi-Fi scanning is not relevant to Ethernet-only operation and that endpoint may be
unavailable when Wi-Fi is inactive. `/ap.json` remains in scope only when the endpoint inventory or
its endpoint-specific disclosure behavior is itself under test.

| Test case | Unit | Test unit | Source | Short description | Automation class | Rationale / manual dependency |
|:----------|:-----|:----------|:-------|:------------------|:-----------------|:------------------------------|
| 5.1-1-2 | A | Discovery of Undocumented Mechanisms | [5.1-1](Self_Assessment_Test_Group_5_1-1.md#test-case-51-1-2-functional) | Scan network/BLE interfaces for undocumented auth paths vs documented list | HYBRID | nmap TCP/UDP sweeps are scriptable, but BLE passive-radio capture needs a lab adapter and cross-checking discovered interfaces against the documented authenticator list is a human completeness judgment. |
| 5.1-1-2 | B | Requirement to Set User-Defined Passwords | [5.1-1](Self_Assessment_Test_Group_5_1-1.md#test-case-51-1-2-functional) | All documented HTTP APIs reject unconfigured user-defined authentication | AUTO | A zero-argument script can load DUT data from the invocation directory's `.env`, verify the default auth state, and negatively probe every method/path in the [HTTP API inventory](../../docs/http_api.md#endpoint-index) with unconfigured Ruuvi, Basic, Digest, RO bearer, and RW bearer credentials. |
| 5.1-1-2 | C | Verification of Generation Mechanisms | [5.1-1](Self_Assessment_Test_Group_5_1-1.md#test-case-51-1-2-functional) | Default password matches per-device FICR DEVICEID rule | HYBRID | Format comparison is scriptable, but extracting the nRF52 FICR DEVICEID from several physical units over serial/SWD needs multiple hardware samples and console access. |
| 5.1-2-2 | A | Password generation-rule conformity on physical units | [5.1-2](Self_Assessment_Test_Group_5_1-2.md#test-case-51-2-2-functional) | Read default passwords from physical units and match the documented syntax | HYBRID | Dictionary/format comparison is scriptable, but reading the DEVICEID-derived password from multiple factory units over the debug console requires physical samples and serial access. |
| 5.1-2A-2 | A | Verification of Exhaustive Documentation | [5.1-2A](Self_Assessment_Test_Group_5_1-2A.md#test-case-51-2a-2-functional) | Confirm no undocumented M2M password interfaces are exposed | HYBRID | Port-scan mapping is scriptable, but judging completeness of discovered M2M interfaces against the documentation is a human assessment. |
| 5.1-2A-2 | B | Password Rejection for M2M | [5.1-2A](Self_Assessment_Test_Group_5_1-2A.md#test-case-51-2a-2-functional) | M2M REST endpoints reject passwords, accept only bearer tokens | AUTO | Automation can validate the secret-safe factory-default authentication state, inject Basic, Digest, interactive-password bearer/body probes, positively verify RO/RW bearer scope, and restore the original configuration with a full-state hash check. |
| 5.1-3-2 | A | Verification of Cryptographic Implementation | [5.1-3](Self_Assessment_Test_Group_5_1-3.md#test-case-51-3-2-functional) | Captured onboarding/config traffic uses documented crypto primitives | HYBRID | Packet capture and header parsing are scriptable, but Wi-Fi onboarding association and expert judgement of TLS/ECDH/AES handshake conformance remain manual. |
| 5.1-4-2 | A | Functional execution of credential change | [5.1-4](Self_Assessment_Test_Group_5_1-4.md#test-case-51-4-2-functional) | Change Web-UI/API credentials via the authenticated config API | AUTO | The credential-change flow can be driven end-to-end through the authenticated config API; new RW keys are activated with a `{}` configuration POST and an unchanged-hash assertion. |
| 5.1-4-2 | B | Success verification / prior-value invalidation | [5.1-4](Self_Assessment_Test_Group_5_1-4.md#test-case-51-4-2-functional) | Old secret rejected and new secret accepted immediately after change | AUTO | Re-authentication can assert old-secret HTTP 401 and new-secret success; RW-key rotation is checked with hash-verified no-op configuration POSTs. |
| 5.1-5-2 | A | Discovery of Undocumented Interfaces | [5.1-5](Self_Assessment_Test_Group_5_1-5.md#test-case-51-5-2-functional) | Confirm no undocumented auth sockets bypass rate limiting | HYBRID | Port/BLE sweeps are scriptable, but completeness judgement of discovered auth entry points versus documentation is manual. |
| 5.1-5-2 | B | Functional Brute-Force Attempt | [5.1-5](Self_Assessment_Test_Group_5_1-5.md#test-case-51-5-2-functional) | Fixed delay throttles interactive guesses; 256-bit bearer guesses fail | AUTO | Eight 10-attempt live-DUT campaigns cover default, user-defined, Basic, Digest, RO/RW bearer, unauthenticated, and disabled modes. Default/Ruuvi GET and POST stages are timed separately: successful controls obtain one challenge per POST, while failed campaigns issue one initial GET and chain each failed POST response's fresh challenge and cookie into the next password guess. Every interactive GET and failed POST must exceed 1.00 second; successful interactive POST, Basic, Digest, and bearer requests must average below 250 ms. Upper-bound averages and aggregate POST throughput ignore one slow outlier caused by periodic cloud activity; minimum-delay checks always use the raw minimum. Digest and bearer transport failures receive bounded fresh-session retries. Unauthenticated requests are required to succeed but have no brute-force timing requirement. Authentication payloads omit unrelated MQTT fields, restoration is verified against a stable full-state hash, and unrecovered state produces an explicit factory-reset instruction. |
| 5.2-1-2 | A | Public accessibility of disclosure policy | [5.2-1](Self_Assessment_Test_Group_5_2-1.md#test-case-52-1-2-functional) | Disclosure policy URL reachable over unauthenticated HTTP | AUTO | A script can fetch the public policy URL and assert it responds without authentication. |
| 5.2-1-2 | B | Presence of required policy elements | [5.2-1](Self_Assessment_Test_Group_5_2-1.md#test-case-52-1-2-functional) | Policy states contact, acknowledgement and status-update timelines | HYBRID | A script can grep the page for a contact address and timeline keywords, but semantic completeness of the policy against the standard is a human judgement on external content. |
| 5.3-1-2 | A | Functional Assessment of Update Mechanisms | [5.3-1](Self_Assessment_Test_Group_5_3-1.md#test-case-53-1-2-functional) | Each update mechanism rejects unsigned/corrupted images (anti-misuse) | HYBRID | HTTPS/signature rejection paths are scriptable, but exercising the physical USB serial-flash path and multi-channel selection needs physical media and a manual choice. |
| 5.3-2-2 | A | Devised attack simulations against update mechanisms | [5.3-2](Self_Assessment_Test_Group_5_3-2.md#test-case-53-2-2-functional) | Craft MitM, mix-and-match, power-cut and SWD tamper attacks | HYBRID | Network bit-flip/substitution attacks are scriptable, but interrupted-power and direct nRF52 SWD tampering need a physical rig and external debug probe. |
| 5.3-2-2 | B | Resilience verification (rejection of tampered updates) | [5.3-2](Self_Assessment_Test_Group_5_3-2.md#test-case-53-2-2-functional) | DUT detects and rolls back all tampered updates | HYBRID | Signature/rollback rejection over the network is observable by script, but the SWD and power-interruption outcomes require sustained physical observation. |
| 5.3-4A-2 | A | Functional Assessment of Automated Update Execution | [5.3-4A](Self_Assessment_Test_Group_5_3-4A.md#test-case-53-4a-2-functional) | Autonomous check→download→verify→install of a newer release | BLOCKED | Producing the "installed newer signed release" outcome requires a controllable channel serving a newer validly-signed image, but the secure-boot signing fixture is unavailable. |
| 5.3-4B-2 | A | Consent Prompt Verification | [5.3-4B](Self_Assessment_Test_Group_5_3-4B.md#test-case-53-4b-2-functional) | Onboarding Step 5 renders auto-update consent controls | HYBRID | The wizard page is web-inspectable, but reaching a factory-default state requires the physical reset/button and captive-portal association. |
| 5.3-4B-2 | B | Granted Consent Verification | [5.3-4B](Self_Assessment_Test_Group_5_3-4B.md#test-case-53-4b-2-functional) | Granting consent triggers autonomous signed update install | BLOCKED | Confirming the update is actually applied needs a mock channel plus a newer validly-signed image; the signing fixture is unavailable. |
| 5.3-4B-2 | C | Denied Consent Verification | [5.3-4B](Self_Assessment_Test_Group_5_3-4B.md#test-case-53-4b-2-functional) | Denying consent keeps the background update task inactive | HYBRID | A mock version channel plus packet capture can confirm zero background downloads, but requires the mock-channel lab setup and a fresh onboarding. |
| 5.3-6A-2 | A | Functional Assessment of Automatic Update Controls | [5.3-6A](Self_Assessment_Test_Group_5_3-6A.md#test-case-53-6a-2-functional) | Enable/disable/postpone controls for automatic updates | HYBRID | Enable/disable toggles and polling captures are scriptable, but verifying schedule-window deferral needs a mock update channel and time-shifted observation. |
| 5.3-6B-2 | A | Functional Assessment of Notification Enable/Disable Controls | [5.3-6B](Self_Assessment_Test_Group_5_3-6B.md#test-case-53-6b-2-functional) | Enable/disable controls for update notifications | HYBRID | Toggle state and rendered notifications are inspectable, but notification content depends on a mocked external version index. |
| 5.3-13-2 | a | Information Provided as Described | [5.3-13](Self_Assessment_Test_Group_5_3-13.md#test-case-53-13-2-functional) | Lifecycle page must list the gateway support period — source FAIL | HYBRID | A fetch can search the page for the product entry, but source records **FAIL** (page omits the gateway) and confirming "as described" is a documentation judgement. |
| 5.3-13-2 | b | Unrestricted Public Access | [5.3-13](Self_Assessment_Test_Group_5_3-13.md#test-case-53-13-2-functional) | Lifecycle page publicly reachable without restriction — source PASS | AUTO | A script can fetch the lifecycle URL and assert unauthenticated HTTP access with no paywall. |
| 5.3-13-2 | c | Support Period Defined for Software Components | [5.3-13](Self_Assessment_Test_Group_5_3-13.md#test-case-53-13-2-functional) | Published binding support timeframe — source FAIL ("? years" placeholder) | MANUAL | Source records **FAIL**; verifying a binding timeframe commitment is a subjective review of organizational documentation, not a device test. |
| 5.3-14-2 | a | User information access provided as described | [5.3-14](Self_Assessment_Test_Group_5_3-14.md#test-case-53-14-2-functional) | Rationale-for-absence-of-updates resource | N/A | Source declares N/A: the DUT is fully OTA-updatable, so no absence-of-updates resource is required. |
| 5.3-14-2 | b | Resource accessible without restrictions | [5.3-14](Self_Assessment_Test_Group_5_3-14.md#test-case-53-14-2-functional) | Public access to absence-of-updates rationale | N/A | Source declares N/A: the provision condition is not met because the DUT is updatable. |
| 5.3-14-2 | c | Rationale contains reason for absence of updates | [5.3-14](Self_Assessment_Test_Group_5_3-14.md#test-case-53-14-2-functional) | Published reason for non-updatability | N/A | Source declares N/A: firmware update lifecycle management is actively supported. |
| 5.3-14-2 | d | Hardware replacement plan details period and method | [5.3-14](Self_Assessment_Test_Group_5_3-14.md#test-case-53-14-2-functional) | Published hardware-replacement schedule | N/A | Source declares N/A: OTA maintainability removes the non-updatable hardware-replacement requirement. |
| 5.3-15A-2 | a | Setup | [5.3-15A](Self_Assessment_Test_Group_5_3-15A.md#test-case-53-15a-2-functional) | Establish active network + BLE scanning baseline | HYBRID | Telemetry/scan checks are scriptable, but the baseline needs a physical BLE sensor and a cabled/Wi-Fi environment. |
| 5.3-15A-2 | b | Isolation Execution | [5.3-15A](Self_Assessment_Test_Group_5_3-15A.md#test-case-53-15a-2-functional) | Disconnect network media and revoke Wi-Fi per IXIT 9 | HYBRID | Socket-close observation is scriptable, but physically unplugging Ethernet and revoking Wi-Fi credentials are manual. |
| 5.3-15A-2 | c | Functionality Loss Assessment | [5.3-15A](Self_Assessment_Test_Group_5_3-15A.md#test-case-53-15a-2-functional) | BLE/core functions survive isolation without crashes | HYBRID | Post-isolation BLE/log checks are scriptable, but depend on the manual physical isolation step. |
| 5.3-15B-2 | a | Hardware Replacement Execution | [5.3-15B](Self_Assessment_Test_Group_5_3-15B.md#test-case-53-15b-2-functional) | Deploy replacement unit, apply backup, re-enter Wi-Fi | HYBRID | Backup application is scriptable, but procuring/swapping a physical replacement unit and re-entering Wi-Fi via Web-UI are manual. |
| 5.3-15B-2 | b | Functional & Security Parity Regained | [5.3-15B](Self_Assessment_Test_Group_5_3-15B.md#test-case-53-15b-2-functional) | Replacement restores telemetry/BLE/TLS under its own identity | HYBRID | Telemetry/TLS re-verification is scriptable, but depends on the physical unit swap and a BLE sensor source. |
| 5.3-16-2 | a | Extraction via Described Method | [5.3-16](Self_Assessment_Test_Group_5_3-16.md#test-case-53-16-2-functional) | Model designation read from physical label without tools | MANUAL | Pure visual inspection of the enclosure label; inherently human. |
| 5.3-16-2 | b | Simple Text & IXIT Matching | [5.3-16](Self_Assessment_Test_Group_5_3-16.md#test-case-53-16-2-functional) | Label text equals "Ruuvi Gateway" and matches IXIT | MANUAL | Reading and comparing the printed label string is a visual/human task. |
| 5.4-1-2 | A | Functional Assessment of Implemented Protection Schemes | [5.4-1](Self_Assessment_Test_Group_5_4-1.md#test-case-54-1-2-functional) | Sensitive parameters protected from remote/logical exposure | HYBRID | Remote HTTP probes and redirect/401 checks are scriptable, but confirming no secrets leak in UART logs and Web-UI exports needs serial access and human review. |
| 5.4-2-2 | A | Functional Assessment of Tamper-Resistant Implementation | [5.4-2](Self_Assessment_Test_Group_5_4-2.md#test-case-54-2-2-functional) | Hard-coded DeviceID immutable across erase/OTA | HYBRID | NVS-erase/OTA cycles are scriptable, but reading FICR registers to confirm immutability requires serial/SWD hardware access. |
| 5.4-3-2 | A | Functional Assessment of Dynamic Provisioning & Parameter Uniqueness | [5.4-3](Self_Assessment_Test_Group_5_4-3.md#test-case-54-3-2-functional) | No hard-coded secrets; provisioned parameters unique per unit | HYBRID | Firmware-binary analysis is scriptable, but per-device uniqueness verification needs multiple physical DUTs. |
| 5.5-1-2 | A | Functional Verification of Protocol Implementation | [5.5-1](Self_Assessment_Test_Group_5_5-1.md#test-case-55-1-2-functional) | Runtime cipher suites/protocols match IXIT 11 declarations | HYBRID | TLS capture/analysis is scriptable, but oscilloscope/logic-analyzer probing of internal SWD lines needs specialized bench hardware. |
| 5.5-2-2 | A | Functional Identification Verification | [5.5-2](Self_Assessment_Test_Group_5_5-2.md#test-case-55-2-2-functional) | Component names/versions match IXIT 12 and reports | AUTO | Authenticated `/ruuvi.json`, `/metrics`, and UART boot output must agree on ESP32/nRF52 firmware versions. Esptool reset and early-exit UART capture (eight-second maximum) also supply application ESP-IDF for comparison with IXIT v4.2.5. Public GitHub lookups verify installed release tags. Unobservable component versions remain informational; see the runtime component identification and serial acquisition requirements above. |
| 5.5-4-2 | a | Deny Unauthenticated / Invalid Access | [5.5-4](Self_Assessment_Test_Group_5_5-4.md#test-case-55-4-2-functional) | Unauth/invalid/over-privileged requests rejected (302/401/403) | AUTO | HTTP requests with missing/invalid credentials and an RO token on `POST /ruuvi.json` can be scripted with objective status-code and unchanged-hash assertions. |
| 5.5-4-2 | b | Grant Authorized Access | [5.5-4](Self_Assessment_Test_Group_5_5-4.md#test-case-55-4-2-functional) | Valid credentials/tokens granted appropriate access | AUTO | Authenticated RO requests and an RW `POST /ruuvi.json` with `{}` are scriptable with objective success and configuration-non-mutation verdicts. |
| 5.5-4-2 | c | Verify Protection Conformance | [5.5-4](Self_Assessment_Test_Group_5_5-4.md#test-case-55-4-2-functional) | Handshake uses challenge/ECDH/encrypted payloads, no cleartext | AUTO | Local traffic capture can confirm challenge headers, ECDH exchange and absence of cleartext passwords objectively. |
| 5.5-5-2 | A | Functional Access and Authorization Testing | [5.5-5](Self_Assessment_Test_Group_5_5-5.md#test-case-55-5-2-functional) | Unauthorized config rejected; authorized config committed | AUTO | Unauthorized/invalid/token-escalation config requests are exercisable over HTTP with objective 302/401/403/commit verdicts. |
| 5.5-5-2 | B | Completeness of Logical Interface Documentation | [5.5-5](Self_Assessment_Test_Group_5_5-5.md#test-case-55-5-2-functional) | Only documented listening sockets exposed (port scan) | AUTO | Full-range nmap scanning is scriptable. The SNTP client uses a boot-randomized UDP source port (49152-65535), so the test correlates a captured outbound request to UDP/123 with any indeterminate port, then restores the temporary NTP configuration. |
| 5.5-6-2 | A | Functional Network Sniffing & Cryptographic Verification | [5.5-6](Self_Assessment_Test_Group_5_5-6.md#test-case-55-6-2-functional) | Local critical parameters encrypted in transit; no cleartext | HYBRID | Cleartext scanning of captures is scriptable, but Wi-Fi provisioning capture setup is manual; the LAN bearer-token row is declared N/A (opt-in, disabled by default). |
| 5.5-7-2 | A | Functional Network Sniffing & Cryptographic Verification | [5.5-7](Self_Assessment_Test_Group_5_5-7.md#test-case-55-7-2-functional) | Remote-interface critical parameters encrypted in transit | HYBRID | Capture-and-scan is scriptable, but monitor-mode capture across interfaces needs lab setup; the LAN bearer-token row is declared N/A. |
| 5.6-1-2 | a | Interface Status Matching | [5.6-1](Self_Assessment_Test_Group_5_6-1.md#test-case-56-1-2-functional) | Interface enabled/disabled states match IXIT 15/28 | HYBRID | State checks are partly scriptable, but confirming ESP32 BLE emits nothing and SWD is unresponsive needs a BLE sniffer and a debug probe. |
| 5.6-1-2 | b | Undocumented Physical Interfaces | [5.6-1](Self_Assessment_Test_Group_5_6-1.md#test-case-56-1-2-functional) | No undocumented physical interfaces (teardown + RF) | MANUAL | Requires hardware disassembly, PCB-trace inspection and 2.4 GHz RF spectrum sweeps. |
| 5.6-1-2 | c | Undocumented Logical Interfaces | [5.6-1](Self_Assessment_Test_Group_5_6-1.md#test-case-56-1-2-functional) | Documented TCP/80 and UDP/5353 exposed; no hidden logical interfaces | AUTO | Full-range nmap comparison to IXIT 28 is scriptable; an indeterminate UDP port can be the boot-randomized SNTP client source, so identifying it requires outbound NTP traffic correlation rather than a fixed-port assumption. |
| 5.6-2-2 | A | Functional Assessment of Unauthenticated Disclosed Information | [5.6-2](Self_Assessment_Test_Group_5_6-2.md#test-case-56-2-2-functional) | Unauthenticated endpoints expose no sensitive data | AUTO | HTTP assertions are scriptable; a full PASS also requires a boot-time DUT DHCP frame captured after a controlled CH340 reset. Missing serial or L2 visibility yields ERROR. |
| 5.6-3-2 | a | Exposed Interface Documentation Completeness | [5.6-3](Self_Assessment_Test_Group_5_6-3.md#test-case-56-3-2-functional) | Exposed physical interfaces limited to documented set | MANUAL | Requires physical teardown and visual inspection of the external enclosure. |
| 5.6-3-2 | b | Casing Protection Audit | [5.6-3](Self_Assessment_Test_Group_5_6-3.md#test-case-56-3-2-functional) | Non-exposed SWD pads protected inside sealed casing | MANUAL | Requires enclosure disassembly and visual inspection. |
| 5.6-3-2 | c | Enabled / Disabled Status Verification | [5.6-3](Self_Assessment_Test_Group_5_6-3.md#test-case-56-3-2-functional) | ESP32 BLE/SWD disabled; FEM RF muting verified | HYBRID | BLE sniffing and J-Link probe attempts are scriptable, but oscilloscope/logic-analyzer checks of the FEM LNA lines are a manual bench task. |
| 5.6-4A-2 | A | Debug interface protection | [5.6-4A](Self_Assessment_Test_Group_5_6-4A.md#test-case-56-4a-2-functional) | SWD locked post-boot; USB is non-interactive log-only | HYBRID | J-Link/serial probing is partly scriptable, but physical probe attachment to internal pads is manual. |
| 5.6-4A-2 | B | Discovery of undocumented physical debug interfaces | [5.6-4A](Self_Assessment_Test_Group_5_6-4A.md#test-case-56-4a-2-functional) | No hidden JTAG/SWD/UART on PCB test points | MANUAL | Requires probing unlabelled PCB pads with multimeter, logic analyzer and boundary-scan tools. |
| 5.6-4B-2 | A | Functional Verification of Physical Debug Protection | [5.6-4B](Self_Assessment_Test_Group_5_6-4B.md#test-case-56-4b-2-functional) | No exposed debug ports on external casing | MANUAL | Physical teardown and visual casing inspection are inherently human. |
| 5.7-1-2 | A | Functional Software Integrity Verification Testing | [5.7-1](Self_Assessment_Test_Group_5_7-1.md#test-case-57-1-2-functional) | Corrupted images rejected; rollback/lockout enforced | AUTO | esptool.py corruption, USB-reset power cycling and serial boot-log assertions can run end-to-end over a wired harness. |
| 5.7-2-2 | A | Alerting on detected unauthorized software changes | [5.7-2](Self_Assessment_Test_Group_5_7-2.md#test-case-57-2-2-functional) | Reboot-loop alert + absent hotspot SSID on dual-slot failure | HYBRID | esptool injection is scriptable and SSID absence is Wi-Fi-scannable, but confirming the sustained reboot-loop alert emphasizes physical observation. |
| 5.7-2-2 | B | Network communication restriction to necessary scope | [5.7-2](Self_Assessment_Test_Group_5_7-2.md#test-case-57-2-2-functional) | Zero network traffic emitted after tamper detection | HYBRID | Passive capture of zero traffic is scriptable, but requires the dual-slot signature-corruption fixture and capture setup. |
| 5.8-1-2 | A | Functional Cryptographic Verification for Personal Data Transmission | [5.8-1](Self_Assessment_Test_Group_5_8-1.md#test-case-58-1-2-functional) | Personal data sent only over TLS/encrypted sessions | HYBRID | TLS handshake verification from captures is scriptable, but capture-environment setup and config triggering remain manual. |
| 5.8-2-2 | A | Functional Cryptographic Verification for Sensitive Personal Data Transmission | [5.8-2](Self_Assessment_Test_Group_5_8-2.md#test-case-58-2-2-functional) | Sensitive data to cloud is TLS-encrypted and HMAC-signed | HYBRID | HMAC-header and TLS checks are scriptable, but cloud-endpoint access and capture setup are manual. |
| 5.8-3-1 | A | Accessibility of Sensor Documentation | [5.8-3](Self_Assessment_Test_Group_5_8-3.md#test-case-58-3-1-functional) | External-sensing documentation publicly reachable | AUTO | A script can fetch the documented sensor-docs URL and assert unauthenticated accessibility. |
| 5.8-3-1 | B | Understandability for Non-Technical Users (Clause D.3) | [5.8-3](Self_Assessment_Test_Group_5_8-3.md#test-case-58-3-1-functional) | Docs explain sensing in plain language | MANUAL | Judging plain-language clarity for non-technical users is a semantic human assessment. |
| 5.8-3-1 | C | Completeness of Sensing Capabilities Documentation & Physical Audit | [5.8-3](Self_Assessment_Test_Group_5_8-3.md#test-case-58-3-1-functional) | No undocumented mics/cameras/PIR sensors on board | MANUAL | Requires physical teardown and visual PCB/casing inspection. |
| 5.9-1-2 | A | Network connectivity interruption resilience | [5.9-1](Self_Assessment_Test_Group_5_9-1.md#test-case-59-1-2-functional) | Graceful recovery from link/cloud outages | HYBRID | Cloud/WAN blocking and log reads are scriptable, but unplugging Ethernet and reading logs over serial involve physical action. |
| 5.9-1-2 | B | Abrupt power termination resilience | [5.9-1](Self_Assessment_Test_Group_5_9-1.md#test-case-59-1-2-functional) | Clean boot/rollback after power cuts, no flash corruption | HYBRID | Post-recovery checks are scriptable, but pulling USB power and timing recovery are physical/manual. |
| 5.9-2-2 | A | Local functionality during network outage | [5.9-2](Self_Assessment_Test_Group_5_9-2.md#test-case-59-2-2-functional) | BLE scanning/buffering continues while offline | HYBRID | Network triggers and log reads are scriptable, but need a physical BLE sensor and a manual cable disconnect. |
| 5.9-2-2 | B | Power restoration recovery | [5.9-2](Self_Assessment_Test_Group_5_9-2.md#test-case-59-2-2-functional) | Restores exact pre-outage state after power cut | HYBRID | Physical power cuts and cloud-dashboard confirmation of restored telemetry are manual. |
| 5.9-3-2 | A | Functional Protocol Sniffing and Mass-Reconnection Inspection | [5.9-3](Self_Assessment_Test_Group_5_9-3.md#test-case-59-3-2-functional) | Orderly handshakes and 67s retry backoff avoid reconnection storms | HYBRID | Firewall shaping and capture parsing are scriptable, but multi-server failover setup and sustained retry-interval observation need lab infrastructure. |
| 5.11-1-2 | A | Data Creation | [5.11-1](Self_Assessment_Test_Group_5_11-1.md#test-case-511-1-2-functional) | Provision non-default user data/credentials/certs and recover the baseline | HYBRID | API checks are scripted, but CONFIGURE, host network changes and LED-confirmed factory reset require an operator. Require a real protected 2.4 GHz AP, reachable authenticated HTTP/remote-config fixtures and authenticated MQTT password controls. The RW write explicitly preserves current MQTT prefix/client ID/data format (issue #1650 workaround) and compares the full configuration. Storage presence is not a TLS handshake or forensic-erasure proof. A commissioned Pi controller could remove physical transitions; see the [task](tasks/task_5_11_1_2_a.md). |
| 5.11-1-2 | B | Initiation Verification | [5.11-1](Self_Assessment_Test_Group_5_11-1.md#test-case-511-1-2-functional) | Warned hold through restart to repeating red 200 ms on/off, then release; normally about 11 seconds to feedback, not a fixed threshold | HYBRID | Physical CONFIGURE hold and LED/release observation require human action; scripted restart/hotspot observations alone cannot confirm erasure. Corrected initiation is pending functional revalidation. |
| 5.11-1-2 | C | Post-Erasure Inspection | [5.11-1](Self_Assessment_Test_Group_5_11-1.md#test-case-511-1-2-functional) | Inspect absence of seeded user data after LED-confirmed local reset; no whole-partition all-0xFF requirement | HYBRID | Data inspection can be scripted, but reset requires physical action and an authorized fixture. Saved MAC/flags/default records may be written after erase. Prior complete-wipe/dump claims need separate revalidation; LED feedback is not forensic proof. |
| 5.11-1-2 | D | Multi-User Isolation Check | [5.11-1](Self_Assessment_Test_Group_5_11-1.md#test-case-511-1-2-functional) | Multi-user privilege boundaries (single-tenant device) | N/A | The DUT is a single-tenant appliance, so per-user isolation is not applicable per the source. |
| 5.11-2-2 | A | Personal data creation on associated service | [5.11-2](Self_Assessment_Test_Group_5_11-2.md#test-case-511-2-2-functional) | Register cloud account, claim gateway, stream telemetry | HYBRID | Telemetry streaming is scriptable, but provisioning a Ruuvi Cloud account and a claimed physical gateway are external/manual. |
| 5.11-2-2 | B | Deletion initiation & interaction | [5.11-2](Self_Assessment_Test_Group_5_11-2.md#test-case-511-2-2-functional) | Request account deletion and confirm via emailed link | HYBRID | The deletion request can be issued programmatically, but clicking the emailed confirmation link needs an external mailbox and human action. |
| 5.11-2-2 | C | Post-deletion data absence check | [5.11-2](Self_Assessment_Test_Group_5_11-2.md#test-case-511-2-2-functional) | Login/API confirm account and telemetry purged | HYBRID | API/login absence checks are scriptable, but depend on the external account and email-confirmation steps. |
| 5.11-3-1 | a | Documented instructions successfully delete personal data | [5.11-3](Self_Assessment_Test_Group_5_11-3.md#test-case-511-3-1-functional) | Prior automation label retained; normative unit a is typical-data creation, not reset-success evidence | HYBRID | The existing inventory label conflates unit a with unit c execution. Corrected local-reset execution remains pending under unit c; service execution is separate. No inventory-wide reclassification performed. |
| 5.11-3-1 | b | Documentation coverage of all deletion functionalities | [5.11-3](Self_Assessment_Test_Group_5_11-3.md#test-case-511-3-1-functional) | Cross-check IXIT 2 against every IXIT 25 function; local 012 is a draft supporting reference | MANUAL | All-function coverage and public availability remain pending; linking corrected local instructions is not completed functional assessment. |
| 5.11-3-1 | c | Conciseness of deletion instructions | [5.11-3](Self_Assessment_Test_Group_5_11-3.md#test-case-511-3-1-functional) | Assess conciseness and necessary steps by executing each documented deletion function | MANUAL | Human assessment includes the physical LED-confirmed reset walkthrough, not text review alone. Corrected local execution is pending; no service result was rerun. |
| 5.11-4-1 | a | Personal data creation | [5.11-4](Self_Assessment_Test_Group_5_11-4.md#test-case-511-4-1-functional) | Create typical personal data on DUT and cloud | HYBRID | Device provisioning is scriptable, but cloud data creation needs an external account and a physical gateway. |
| 5.11-4-1 | b | Execution of deletion per documentation | [5.11-4](Self_Assessment_Test_Group_5_11-4.md#test-case-511-4-1-functional) | Execute each deletion mechanism per its documentation | HYBRID | The corrected physical hold/LED/release sequence is pending revalidation; cloud account deletion remains a separate external action. |
| 5.11-4-1 | c | Clear deletion confirmation feedback | [5.11-4](Self_Assessment_Test_Group_5_11-4.md#test-case-511-4-1-functional) | Assess each function's clear completion signal; local reset uses repeating red 200 ms on/off before release | HYBRID | Hotspot reactivation can be observed by script but is not erase-complete confirmation. Local LED clarity and the separate cloud confirmation screen require human assessment; no renewed PASS follows from source review. |
| 5.12-1-2 | A | Decisions triggered as documented | [5.12-1](Self_Assessment_Test_Group_5_12-1.md#test-case-512-1-2-functional) | All IXIT 26 decisions triggered in wizard/Web-UI | HYBRID | Triggering the wizard/API flows is scriptable, but needs a fresh onboarding via the captive portal. |
| 5.12-1-2 | B | Prominence of decision requests | [5.12-1](Self_Assessment_Test_Group_5_12-1.md#test-case-512-1-2-functional) | Decisions prominently presented during setup/maintenance | MANUAL | Judging screen prominence is a human/visual assessment. |
| 5.12-1-2 | C | Understandability for non-technical users (Clause D.3) | [5.12-1](Self_Assessment_Test_Group_5_12-1.md#test-case-512-1-2-functional) | Decision prompts use plain, non-technical language | MANUAL | Understandability judgement per Clause D.3 is semantic and human. |
| 5.12-1-2 | D | Conformance to IXIT options | [5.12-1](Self_Assessment_Test_Group_5_12-1.md#test-case-512-1-2-functional) | Presented options match the IXIT 26 declarations | HYBRID | The UI option set can be enumerated by script, but mapping it to the IXIT declarations needs human interpretation. |
| 5.12-2-1 | a | Device setup using documentation | [5.12-2](Self_Assessment_Test_Group_5_12-2.md#test-case-512-2-1-functional) | DUT can be set up following the secure-setup docs | HYBRID | Executing documented setup steps is partly scriptable, but is a documentation-driven walkthrough. |
| 5.12-2-1 | b | Documentation coverage of all security decisions | [5.12-2](Self_Assessment_Test_Group_5_12-2.md#test-case-512-2-1-functional) | Docs cover every IXIT 26 security-relevant decision | MANUAL | Auditing documentation coverage against the decision catalogue is a manual review. |
| 5.12-2-1 | c | Explicit secure-setup recommendations | [5.12-2](Self_Assessment_Test_Group_5_12-2.md#test-case-512-2-1-functional) | Docs give a clear secure recommendation per decision | MANUAL | Judging guidance clarity/coverage is a manual documentation review. |
| 5.12-3-2 | a | Secure baseline provisioning | [5.12-3](Self_Assessment_Test_Group_5_12-3.md#test-case-512-3-2-functional) | Provision hardened baseline (password, keys, HTTPS, latest FW) | HYBRID | Config is scriptable, but establishing the "latest signed firmware" state uses a wired flash fixture. |
| 5.12-3-2 | b | Documented setup check (secure) | [5.12-3](Self_Assessment_Test_Group_5_12-3.md#test-case-512-3-2-functional) | Setup check reports secure state correctly | HYBRID | Web-UI state is readable by script, but judging the diagnostic indication/prominence is human. |
| 5.12-3-2 | c | Documented maintenance check (secure) | [5.12-3](Self_Assessment_Test_Group_5_12-3.md#test-case-512-3-2-functional) | Maintenance check reports up-to-date/healthy state | HYBRID | Update-panel state is readable by script, but requires the latest signed firmware and a human indication judgement. |
| 5.12-3-2 | d | Insecure reconfiguration | [5.12-3](Self_Assessment_Test_Group_5_12-3.md#test-case-512-3-2-functional) | Reconfigure to insecure state (no password, manual, old FW) | HYBRID | Config is scriptable, but flashing an outdated firmware image uses a wired flash fixture. |
| 5.12-3-2 | e | Repeated checks detect insecure state | [5.12-3](Self_Assessment_Test_Group_5_12-3.md#test-case-512-3-2-functional) | Checks prominently warn on insecure configuration | HYBRID | State reads are scriptable, but judging the prominence of the warning banners is human. |
| 5.13-1A-2 | A | User interface documentation completeness | [5.13-1A](Self_Assessment_Test_Group_5_13-1A.md#test-case-513-1a-2-functional) | All user interfaces documented in IXIT 27 | MANUAL | Requires physical mainboard inspection plus a software walkthrough audited against the documentation. |
| 5.13-1A-2 | B | Input validation fuzzing / vulnerability inspection | [5.13-1A](Self_Assessment_Test_Group_5_13-1A.md#test-case-513-1a-2-functional) | App-layer inputs resist malformed/malicious data | HYBRID | HTTP/JSON fuzzing and web-vuln scanning are scriptable, but physical button-chatter injection and serial error-trace review are manual. |
| 5.13-1B-2 | A | Network input fuzzing / injection validation | [5.13-1B](Self_Assessment_Test_Group_5_13-1B.md#test-case-513-1b-2-functional) | Network/BLE/firmware inputs resist tampering | HYBRID | LwIP/UDP fuzzing and OTA signature checks are scriptable, but over-the-air corrupted BLE-frame injection needs RF test hardware. |
| 5.13-1B-2 | B | Physical network interface documentation completeness | [5.13-1B](Self_Assessment_Test_Group_5_13-1B.md#test-case-513-1b-2-functional) | All physical network interfaces documented in IXIT 15 | MANUAL | Requires physical hardware inspection and a user-manual audit. |
| 6-1-2 | A | Clarity & transparency of published user information | [6-1](Self_Assessment_Test_Group_6-1.md#test-case-6-1-2-functional) | Published personal-data information clear and transparent | MANUAL | Judging clarity/transparency of external published statements is a human documentation audit. |
| 6-1-2 | B | Accuracy vs actual implementation | [6-1](Self_Assessment_Test_Group_6-1.md#test-case-6-1-2-functional) | Published claims match actual data flows (IXIT 21) | HYBRID | Wire captures and memory checks are scriptable, but cross-referencing them against external published statements is a manual audit. |
| 6-2-2 | A | Functional Consent Verification | [6-2](Self_Assessment_Test_Group_6-2.md#test-case-6-2-2-functional) | Personal data sent only after explicit opt-in | HYBRID | Wizard execution and packet monitoring are scriptable, but judging "free, obvious, explicit" consent is human. |
| 6-3A-2 | A | Functional Verification of Consent Withdrawal Execution | [6-3A](Self_Assessment_Test_Group_6-3A.md#test-case-6-3a-2-functional) | Consent withdrawable via UI/network/factory reset | HYBRID | UI toggles and flash checks are scriptable, but the factory-reset button, network disconnect and cloud account deletion are physical/external. |
| 6-3B-2 | A | Functional Consent Storage Verification | [6-3B](Self_Assessment_Test_Group_6-3B.md#test-case-6-3b-2-functional) | Consent state persists across power cycles/reboots | HYBRID | Flash-partition dumps and reboots are scriptable, but cloud account-dashboard confirmation needs web-portal/visual verification. |
| 6-5-2 | A | Accessibility of telemetry documentation | [6-5](Self_Assessment_Test_Group_6-5.md#test-case-6-5-2-functional) | Telemetry documentation publicly reachable | AUTO | A script can fetch the telemetry-docs URL and assert unauthenticated accessibility. |
| 6-5-2 | B | Alignment with IXIT purpose | [6-5](Self_Assessment_Test_Group_6-5.md#test-case-6-5-2-functional) | Published telemetry purposes match IXIT 24 | MANUAL | Comparing published descriptions to the IXIT purposes is a semantic documentation review. |
| 6-5-2 | C | Description of collected telemetry metrics | [6-5](Self_Assessment_Test_Group_6-5.md#test-case-6-5-2-functional) | Docs describe all collected telemetry metrics | MANUAL | Verifying completeness of the described metrics is a manual documentation review. |
| 6-5-2 | D | Description of usage, parties & purpose | [6-5](Self_Assessment_Test_Group_6-5.md#test-case-6-5-2-functional) | Docs describe usage, recipients and purposes | MANUAL | Assessing completeness of usage/recipient/purpose descriptions is a human review. |
| 6-6-2 | A | Typical personal data creation | [6-6](Self_Assessment_Test_Group_6-6.md#test-case-6-6-2-functional) | Create typical personal data on DUT and cloud | HYBRID | Device provisioning is scriptable, but cloud data creation needs an external account and a physical gateway. |
| 6-6-2 | B | Unnecessary personal data not collected | [6-6](Self_Assessment_Test_Group_6-6.md#test-case-6-6-2-functional) | No extraneous personal data collected/transmitted | HYBRID | Traffic/memory audits are scriptable, but judging "necessary/minimized" is a human assessment. |
| 6-6-2 | C | Uncreated personal-data categories not collected | [6-6](Self_Assessment_Test_Group_6-6.md#test-case-6-6-2-functional) | Categories not provisioned are not collected | HYBRID | Capture/memory audits are scriptable, but confirming absence of unneeded categories involves human judgement. |
| 6-6-2 | D | Deletion when no longer required | [6-6](Self_Assessment_Test_Group_6-6.md#test-case-6-6-2-functional) | Factory reset and cloud deletion purge data | HYBRID | Flash-erase checks are scriptable, but the factory-reset button and cloud account deletion are physical/external. |
| 6-7-2 | a | Creation of personal data foreseen for aggregation | [6-7](Self_Assessment_Test_Group_6-7.md#test-case-6-7-2-functional) | No data designated for sole aggregation | N/A | Source declares N/A: per IXIT 21 no personal data is processed solely for aggregation. |
| 6-7-2 | b | Functional verification of aggregation mechanisms | [6-7](Self_Assessment_Test_Group_6-7.md#test-case-6-7-2-functional) | No aggregation-only processing exists | N/A | Source declares N/A: no aggregation-only processing loops exist to exercise. |
| 6-8-2 | a | Creation of personal data foreseen for anonymization | [6-8](Self_Assessment_Test_Group_6-8.md#test-case-6-8-2-functional) | No data designated for anonymization | N/A | Source declares N/A: per IXIT 21 no personal data is designated for anonymization. |
| 6-8-2 | b | Functional verification of anonymization | [6-8](Self_Assessment_Test_Group_6-8.md#test-case-6-8-2-functional) | No anonymization routines exist | N/A | Source declares N/A: no anonymization routines exist to exercise. |

## Ambiguities requiring parent review

- **Combined-unit headings.** Several functional test cases group multiple units under one heading
  (e.g. "Test Units A & B", "Test Unit A, B, C & D"). Each enumerated unit is expanded into its own
  row, and per-unit titles are derived faithfully from the lettered sub-goals stated in that test
  case's purpose. If the parent intends these to remain single combined rows, the affected cases are
  5.1-4-2, 5.2-1-2, 5.3-2-2, 5.6-4A-2, 5.7-2-2, 5.9-1-2, 5.9-2-2, 5.13-1A-2, 5.13-1B-2,
  6-1-2, and the multi-unit cases 5.3-4B-2, 5.8-3-1, 5.11-1-2/2-2/3-1/4-1, 5.12-1-2/2-1/3-2, 6-5-2,
  6-6-2.
- **BLOCKED vs HYBRID on the auto-update install path.** 5.3-4A-2 A and 5.3-4B-2 B are marked
  BLOCKED because objectively confirming that a *newer, validly signed* image is installed requires
  the secure-boot signing fixture, which is unavailable. If a mock/relaxed-verification build or a
  signing fixture becomes available, these become HYBRID/AUTO.
- **5.11-1-2 D (multi-user isolation).** The source verdict is PASS but the content states the DUT is
  single-tenant, so per-user isolation is inapplicable; classified N/A on that basis.
