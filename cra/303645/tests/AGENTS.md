# AGENTS.md — CRA Python Functional Tests

## Scope

These instructions apply to `cra/303645/tests/` and every directory below it, including `lib/`.
One file is intentionally used for the whole subtree so test-runner and shared-library rules remain
consistent. Add a deeper `AGENTS.md` only if a future subdirectory needs materially different
instructions.

Read these references before changing this subtree:

- [`README.md`](README.md) for setup, test execution, live-DUT behavior, coverage, and CI.
- [`lib/README.md`](lib/README.md) for the shared library architecture and public API.
- The matching task specification under `cra/303645/tasks/` when creating or changing a CRA test.

Task specifications use `task_<id>.md`, with the same lowercase, underscore-separated `<id>` used
by the corresponding `test_<id>.py` and `test_test_<id>.py` modules. Locate the specification by
test case and unit, then read its referenced assessment/IXIT sources and HTTP API documentation
where available. If the task specification itself is absent, report that gap; do not invent
requirements or assume a different unit has the same procedure.

## Purpose and structure

This is a Python 3.8 project for automating selected ETSI EN 303 645 / ETSI TS 103 701 tests against
a dedicated Ruuvi Gateway.

- `test_<id>.py` is a live-DUT functional-test executable.
- `test_test_<id>.py` is the deterministic host-side implementation test for that executable.
- Name the runner `FunctionalTest_<id>` and its execution function `execute_test_<id>`.
- `test_lib.py` is the only direct unit-test module for `lib/`.
- `lib/` contains reusable runtime infrastructure shared by live tests.
- `.env` contains local DUT configuration.
- `logs/` contains generated functional-test evidence.

Do not confuse this project with the C/C++ firmware unit tests in the repository-root `tests/`
directory. It has separate dependencies, commands, and CI.

## Environment

Use Python 3.8. Set up the environment from this directory:

```bash
python3.8 -m venv .venv
.venv/bin/python -m pip install --requirement requirements.txt
```

Declare required third-party packages in `requirements.txt`. Prefer the standard library and
existing dependencies. Keep new dependencies compatible with Python 3.8.

Verify the interpreter with `.venv/bin/python --version`. An existing Python 3.8 environment with
the required dependencies may be used instead; report the actual interpreter used. Do not silently
validate with a newer system Python. Configure the IDE to use the intended interpreter; automatic
environment setup may select another Python version or create a repository-root `.venv`. Check its
selection before using it and avoid unrelated environment or IDE configuration changes.

## Strict explicit typing

All Python variables in this subtree must have explicit, accurate type annotations. This applies
to live runners, host-side tests, fixtures, and `lib/`, including module constants, class and instance
attributes, and local variables. Inference from an initializer or a function's return annotation
does not satisfy this requirement: write `result: RunResult = self.make_runner().run()`, not
`result = self.make_runner().run()`.

- Annotate a variable at its first definition in its scope; subsequent assignments may reuse that
  declaration. Annotate instance attributes where they are initialized and function parameters and
  return values at the signature. Use concrete result, response, credential, and fixture types.
- Specify container element/key/value types and callable argument/return types. Include `None`
  only when it is a possible value. Do not use bare containers, `Any`, or `object` merely to avoid
  determining the correct type. Reserve dynamic types for genuinely dynamic protocol/JSON values
  and intentional malformed-input boundaries, keeping the surrounding interfaces typed.
- For unpacking, `for`, and `with ... as` bindings, declare the names with annotations before the
  statement. Annotate caught exception names in their handler or before the `try` statement, using
  a type that covers every handler sharing that name. Existing parameter/attribute declarations
  also cover reassignment through these constructs.
- Comprehension and generator-expression targets live in their own scope and cannot carry Python
  annotations. Their input iterable and resulting collection must have explicit element types;
  do not add ineffective declarations in an enclosing scope. For lambdas, use a typed callable
  context, or a named function with annotated parameters and return type when the context is unclear.
- Preserve Python 3.8 support with postponed annotations. Annotation syntax must not introduce
  incompatible runtime type expressions or change test, authentication, recovery, or evidence behavior.
- Review annotations as part of code review; a clean Ruff run alone does not enforce explicit
  local-variable annotations.

## Shared-library rules

- Put behavior in `lib/` only when it is reusable across functional tests. Keep case-specific
  assertions, outcome policy, sequencing, and recovery in the relevant `test_<id>.py`.
- Treat existing names and behavior in `lib/` as a public API for this test project. Preserve
  backward compatibility unless all callers are deliberately migrated.
- Put direct tests of `lib/` only in `test_lib.py`. Do not duplicate tests for configuration
  parsing, evidence serialization, HTTP construction, challenge parsing, authentication helpers,
  models, errors, or API inventory in `test_test_<id>.py`.
- A `test_test_<id>.py` module may use library classes and constants as infrastructure and may test
  how its functional runner responds to library results or exceptions. Assertions about the
  library itself belong in `test_lib.py`.
- Keep `test_lib.py` outside `lib/`. Runtime modules must not include test-only helpers or fixtures.
- Inject external boundaries such as HTTP sessions, random bytes, clocks, key generation, serial
  access, DNS, and subprocess execution. Library unit tests must be deterministic and offline.
- Update `lib/README.md` when the architecture, public API, dependencies, behavior, or examples
  change.

### Shared protocol contracts

- Reuse `GatewayApi`, `GatewayCfgDesc`, `GatewayCfgLanAuthType`, `AuthMech`, `HttpMethod`,
  `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` instead of scattering protocol literals through
  runners. Use `ApiRoute` and the shared inventories where the task requires API coverage.
- Use `GatewayClient.new_session()` for sessions. Preserve `trust_env=False`: host `.netrc`
  credentials must neither add authentication nor replace an explicit Authorization header.
  Environment proxies are also disabled by this setting.
- Preserve fixed HTTP timeouts and disabled redirects for negative authentication probes. Use
  fresh unauthenticated sessions where required so authorized cookies cannot affect the result.
- Reuse the shared interactive login, Basic/Digest header, checked JSON, and ECDH helpers. Their
  contracts include case-insensitive scheme matching with a space/tab token boundary, required
  challenge parameters, cookie validation, and rejection of invalid P-256 keys including infinity
  before deriving shared key material. Do not recreate these mechanisms in individual runners.
- Keep regression tests for these contracts in `test_lib.py`, including malformed scheme prefixes,
  matching `.netrc` entries, invalid ECDH keys, and successful authentication/key agreement.

## Functional-test rules

- Live scripts must run from `cra/303645/tests` with no command-line arguments unless the governing
  task specification explicitly says otherwise.
- Read DUT identity and address through `lib.config.load_dut_config`; do not parse `.env` again in a
  test script.
- Use `GatewayClient` for gateway HTTP and authentication instead of duplicating protocol code.
- Use `EvidenceLog` and structured evidence dataclasses. Log the HTTP request before transmission
  and the response before interpreting it.
- Preserve the result convention: `0` for PASS, `1` for a test assertion FAIL, and `2` for setup,
  transport, protocol, or recovery ERROR.
- Make terminal verdicts explainable: print the component and expected/observed values for an
  assertion failure, or the affected check and exception type/reason for ERROR. Identify skipped
  checks as NOT RUN, keep earlier FAIL explanations visible after later errors, and label
  informational observations as unverified rather than PASS. Mirror explanations in evidence,
  without dumping protocol bodies/secrets or counting result lines as additional progress steps.
  Preserve the final `Overall verdict: <PASS|FAIL|ERROR>` line for 5.5-2-2-A.
- Verify restoration in any test that changes DUT state. Attempt restoration after failures and
  exceptions, and never report PASS when restoration cannot be verified.
- Report factory-reset requirements with `FACTORY_RESET_MESSAGE` where the initial or recovered
  authentication state is unsuitable.
- Mark the reset requirement before raising on a non-200 default `Admin`/`gw_id` login. This
  pre-login failure must remain setup ERROR and retain the recovery message in terminal output
  and evidence. Once configuration is available, validate any exposed MAC against `.env` before
  checking default fields; never recommend resetting a mismatched or malformed identity. If the
  MAC is absent, a non-default baseline is still ERROR but must not prescribe a reset without
  identity confirmation.
- Attribute security assertion failures to the mechanism that ran, including inventory validation
  (length, duplicates, canonical set, and completed coverage) and final non-mutation verification.
  `RunResult.outcomes` and final evidence must record FAIL, not NOT RUN, for a rejected assertion.
  Setup, transport, and malformed-protocol exceptions remain overall ERROR.
- Track the active mechanism explicitly; never infer it from the first NOT RUN entry. A shared
  credential-write failure may affect both keys, but an RO/RW probe or its non-mutation assertion
  belongs to that specific mechanism. Preserve a completed mechanism's PASS when a later mechanism
  fails, and never leave PASS after one of its later controls fails. Prepared-state setup includes
  provisioning, configuration read-back, and required login controls; any exception in these phases
  must mark the setup outcome ERROR and still attempt restoration.
- Save candidate recovery credentials before sending a change that might apply, retaining prior
  candidates until the transition is verified. A timeout or failed follow-up login must not lose
  the credential needed to recover a partially applied change.
- Use the shared reset message and verify changes against the boot-time erase handler and LED
  pattern as well as the restart timer. The five-second timer requests restart; completion is the
  red LED repeating 200 ms on/200 ms off. About eleven seconds is observed elapsed time, not a
  firmware threshold. Never replace this completion condition with a fixed hold duration.
  Release after completion triggers another restart, then the configuration hotspot opens:
  `handle_reset_button_is_pressed_during_boot()` waits for release before calling
  `gateway_restart_immediate_no_cleanup()`. Keep this second restart distinct from the initial
  timer-triggered restart in recovery messages, documentation, and the message regression test.
- When factory-default state is a task precondition, compare the required fields and their types
  with `default_config_values()` from the checked-in `gw_cfg_default/gw_cfg_default_gen_ui.json`.
  Do not manufacture the baseline by changing the DUT or hard-code a second set of default values.
- `authenticate_interactive()` can raise `GatewayAuthenticationModeError` before returning a
  result, notably for Basic or Digest challenges. Where default interactive authentication is
  required, catch that error, mark the factory-reset requirement, and propagate it to setup ERROR
  classification. Checking only `result.auth_payload` misses this path. Preserve both the error
  evidence and `RunResult.recovery_message`, including its terminal reporting.
- Follow the task's request ordering and abort policy. Where required, send reads before dangerous
  writes and stop immediately on unexpected authorization success. Preserve non-mutation checks
  and restoration on partial setup, failure, and exception paths during refactoring.
- Enforce dangerous-last ordering across the entire negative matrix, not separately within each
  authentication scheme. Partition routes once using method/path and handler effects, and share
  that partition between interactive and bearer probes. Finish all safer probes across schemes
  before the first potentially mutating request. Empty POST bodies do not make destructive
  handlers safe if authentication is bypassed. For 5.1-1-2-B, reads and fresh-session `/auth`
  writes precede every non-`/auth` POST/DELETE; final authenticated verification follows the matrix.
  Update progress totals when adding phases, and defer mechanism PASS until all its phases finish.
- Keep case-specific status expectations and recovery policy in the runner. A successful helper
  return alone does not establish a passing security assertion or successful restoration.
- Check route expectations against `docs/http_api.md` and the relevant firmware handler, including
  LAN/hotspot restrictions and authentication precedence. For 5.1-1-2-B, keep `GET /info.json` in
  the 26-route inventory. In default auth mode, missing/Basic/Digest credentials without an
  authorized cookie return 302 before dispatch; disabled bearer probes require 401. The LAN-only
  404 occurs after authorization succeeds and must not be accepted by these negative probes.
- Keep imports free of DUT access and test execution. Put the executable entry point behind
  `if __name__ == "__main__"`; allow execution wrappers to inject work directories, clocks, and
  output callbacks for offline tests.
- Do not run live-DUT scripts during normal automated validation. They require an explicitly
  configured gateway and can mutate its configuration.
- Do not alter or commit local `.env` values or generated `logs/`. Evidence may contain credentials,
  authentication material, configuration values, and HTTP bodies.

### Runtime identification and serial acquisition (5.5-2-2-A)

- Reuse authenticated `GET /ruuvi.json` for `fw_ver`, `nrf52_fw_ver`, and `gw_mac`. `/status.json`
  reports network status; `ruuvi_gw_status.schema.json` describes outbound statistics, not that GET
  response. Do not restore the incorrect `ESP_FW`/`NRF_FW`/`DEVICE_ADDR` assumptions. Verify the
  default-authentication baseline and device identity before serial discovery/reset.
- Then request `GatewayApi.METRICS` with the same authenticated session. Parse one `ruuvigw_info`
  sample (value 1), independently of label order, requiring `mac`, `esp_fw`, and `nrf_fw` and
  checking the MAC against `.env`. Missing/duplicate samples or labels and malformed versions are
  ERROR. Compare JSON `fw_ver`/`nrf52_fw_ver`, metrics `esp_fw`/`nrf_fw`, and installed UART versions
  separately for each component. Well-formed disagreements are FAIL; release-existence PASS must
  never erase them. Preserve HTTP-source FAILs if UART acquisition or later release checks error.
  Log and print source-labeled version values, and include the metrics phase and comparison phase
  in progress totals (ten steps for this runner).
- Use `lib.serial_dut` for hardware access. Keep imports/preflight free of device access and
  discover exactly one CH340 VID `0x1A86`; zero/multiple matches are ERROR, never a guessed port.
  Do not add serial settings to the three-key `.env` for this case.
- Preserve imported-esptool and PATH `esptool`/`esptool.py` support. Fall back only when the
  top-level module is absent, not when an installed package has a broken dependency. Pyserial
  must import in the current interpreter. Record tool versions/source, but never treat them as
  runtime DUT framework evidence.
- Reset with the preflight-selected tool and discovered port using
  `--before default_reset --after hard_reset read_mac`. Use an injected subprocess without a shell,
  a bounded timeout, and evidence of command/output/status (including partial timeout output).
  Do not flash or erase. Do not reintroduce the hand-written DTR/RTS sequence that stranded the
  tested gateway in ROM download mode. Open pyserial only after esptool releases the port, with
  inactive DTR/RTS, and read immediately without another reset, sleep, or input flush.
- Capture up to eight seconds but stop as soon as complete application ESP-IDF, ESP32 firmware, and
  installed-nRF52 firmware lines arrive. Use the shared optional `stop_when` callback; require
  newline-terminated lines so a split serial chunk cannot truncate a version suffix. The nRF52
  message can arrive after two seconds. The 20-second reset subprocess timeout is separate.
  Use `cpu_start: App version:` for ESP32 firmware and the stable `### Firmware on nRF52:` prefix
  for the co-processor, independently of logger-tag spelling (`nRF52Fw` versus `nrf52fw`). Preserve
  support for legacy unprefixed messages under either tag spelling. Never use `Firmware on FatFS`
  or an update-image manifest. Include the mixed-case, ANSI-colored 5234-ms startup line in
  regressions and verify early exit; do not restore a fixed 30-second wait. For the
  framework, parse only `cpu_start: ESP-IDF:`, allowing whitespace and ANSI colors.
  A retained older `2nd stage bootloader` banner must neither fail nor satisfy the framework check.
  Missing application output or `DOWNLOAD_BOOT` / `waiting for download` alone is ERROR; an observed
  application `v4.2.2` against required `v4.2.5` is FAIL. No manual-reset fallback is part of this AUTO test.
- Keep GitHub checks scoped to the installed release tags, without credentials or a latest-release
  requirement. Confirm repository availability/identity before treating a tag 404 as FAIL;
  transport/rate-limit/malformed-response failures remain ERROR. Preserve earlier component FAILs.
  mbedTLS, Web-UI, and nRF5 SDK provenance are informational, not runtime-verified versions.
- Do not assume the proposed `/ruuvi.json` field `espidf_ver` exists: it was not implemented.
  Do not replace UART observations with repository pins or firmware-version inference. Distinguish
  user-confirmed live results from offline implementation checks when documenting validation.

## Host-side implementation tests

- Use `unittest`, matching the existing modules.
- Fake all network and hardware boundaries. A host-side test must never contact a real DUT.
- Test the automation's behavior: exact request matrices and bodies, status handling, ordering,
  timing policy, recovery, restoration, evidence integration, progress, and exit-code aggregation.
- Make fixture expectations independent of the production helper where practical. For example,
  calculate an expected digest independently instead of calling the same method under test.
- A successful-login fake must validate the complete submitted credential body, including the
  challenge-derived password response for `Admin`/`config.gw_id`, before authorizing its session.
  Username-only acceptance cannot verify the valid-login precondition. Cover incorrect passwords
  and corrupt responses, as well as a successful complete run.
  Validate the cookie and outstanding challenge for that session too; consume successful challenges
  so missing, stale, replayed, or cross-session login requests cannot authorize. A client-level fake
  may isolate orchestration, but include a full run through the real client and a fake HTTP boundary
  for authentication-sensitive runners. Do not make failed configuration writes alter fake credentials.
- Assert the ordered requests actually recorded by the fake HTTP boundary, including scheme,
  method, path, and relevant body/session/redirect details. Do not rely solely on the absence of
  exceptions, `assertTrue(True)`, an aggregate PASS, or the runner's own coverage set to prove that
  a request matrix ran.
  Guard against vacuous checks such as `all(...)` over an empty list.
- Build expected matrices from the task and canonical inventory, not from the runner's filtering
  helper or its recorded requests. Preserve read/write ordering and explicitly include special
  routes such as unauthenticated `DELETE /auth` when required by the task. Assert the exact sequence
  so skipped, duplicated, reordered, and extra requests are detected. Do not copy a route count
  from another test: the inventory contains multiple methods for the same path.
- Include request bodies in matrix assertions. Where the least-operative contract applies, expect
  `{}` for POST and `None` for GET/DELETE, including bearer and special `/auth` probes. Inspect
  prepared requests at the fake transport boundary so omitted, extra, or operative bodies fail
  tests even when the fake's status response depends only on authentication.
- Add a full-run ordering assertion across schemes, independent of the runner's partition helper.
  A per-scheme sequence test cannot detect an early dangerous request in another scheme. Cover
  failures in the last safer phases and assert that no mutating probes follow the failure; preserve
  immediate abort on unexpected success during the dangerous phase as well.
  Include positive bearer reads before negative writes when the prepared state supports both. Keep
  provisioning and mandatory recovery explicitly identifiable in the recorded sequence; restoration
  and revoked-key checks must still run after a probe abort. For 5.1-2A-2-B, require all password and
  RO/RW bearer GETs before password POSTs, then RO denial and the RW no-op POST plus hash check.
- Make fake responses consistent with the state being modeled. A Basic or Digest auth mode must
  advertise the corresponding challenge, not always `x-ruuvi-interactive`; otherwise tests can
  bypass the real shared-library error path. Test both modes and assert ERROR, the reset message,
  and absence of subsequent probes or mutation when setup is rejected.
- Cover applicable failure paths: unexpected success/status, immediate abort, malformed or missing
  response data, timeout/connection errors, partial mutation, failed restoration, and final-state
  mismatch. Verify final verdicts, recovery messages, and required evidence/output behavior.
- For identification tests, retain realistic mixed bootloader/application UART fixtures and
  separate missing-banner, ROM-download, version-mismatch, missing-release, and network-error cases.
  Cover each source disagreeing for each firmware component; keep JSON, metrics, and UART fixtures
  independent. Cover malformed/duplicate metrics, wrong metrics MAC, authenticated request order,
  delayed/partial UART messages, FatFS-image rejection, early stop, timeout, and cleanup on errors.
  Assert informative terminal reasons as well as final verdicts, including a prior FAIL followed
  by ERROR, initialization failures, unchanged progress numbering, and the final verdict line.
  Keep direct tool-selection/reset-order/timeout/partial-output/port-cleanup tests in `test_lib.py`;
  inject subprocess execution as well as serial I/O so no host test can reset a real gateway.
- Exercise each inventory rejection and final assertion independently, and inspect both the
  per-mechanism result and final evidence. Combine wrong/missing/malformed identity with non-default
  authentication fields to verify that recovery advice does not target an unverified device.
  Model unauthenticated default-mode LAN `GET /info.json` returning 302 for missing/Basic/Digest
  credentials. Verify that 404 fails these negative probes and disabled-bearer probes, with the
  responsible mechanism marked FAIL and mandatory final verification still attempted.
- Keep fixtures small enough to make the tested behavior visible. Reuse a local fake only when it
  remains clear which state transitions and failures it models.
- Record prepared HTTP requests in a typed structure (for example, a dataclass), and use a typed
  request key for response overrides. Keep response payloads flexible only where malformed-input
  testing needs it; do not make the entire fake interface `Any`.
- Host-side implementation tests are not compliance evidence. Do not describe them as proving ETSI
  conformance.

## Required validation

Always run Ruff on the entire subtree before making changes and again before handoff for every
task affecting these tests or their guidance, including documentation-only tasks. From the
repository root:

```bash
cra/303645/tests/.venv/bin/ruff check cra/303645/tests
```

This command automatically discovers `cra/303645/tests/pyproject.toml` for this subtree.
For explicit configuration selection from the repository root, use:

```bash
(cd cra/303645/tests && .venv/bin/ruff check --config pyproject.toml .)
```

From this directory, use `.venv/bin/ruff check --config pyproject.toml .`; the shorter
`.venv/bin/ruff check .` also discovers the same file. With `--config`, relative settings
resolve against the working directory, so keep it at `cra/303645/tests` rather than
passing the nested config path from the repository root. See
[`README.md` — Required linting](README.md#required-linting) for discovery verification.
Use this configuration and fix reported lint problems; rerun until clean. Inspect each proposed fix and
preserve Python 3.8 behavior, recovery, and evidence semantics. Do not silence valid findings by
weakening rules or adding blanket exclusions. Explain any necessary narrow suppression.
Do not defer findings merely because they predate the current change. Report the initial finding
count and the final command result; completion requires a clean check unless the user explicitly
instructs otherwise.

Use postponed annotations (`from __future__ import annotations`) for Ruff's modern annotation
syntax while supporting Python 3.8. Keep runtime type expressions compatible with Python 3.8;
postponement applies only to annotations. Validate changes with the actual Python 3.8 interpreter.
Preserve broad exception catches at run/recovery boundaries when they log unexpected errors,
classify the run as ERROR, or continue restoration. A line-specific `BLE001` suppression with
that reason is appropriate. `EvidenceLog.exception(error)` requires the exception object and is
not `logging.exception`; use a documented line-specific `TRY401` suppression for this false
positive instead of deleting the argument or losing the traceback.

Linting and formatting are separate operations. During the current code-review fixes, do not run
`ruff format` or perform a whole-file reformat. Keep lint corrections focused; inspect the diff
before applying automatic fixes and do not apply `--unsafe-fixes` blindly. If the user defers
unrelated lint cleanup, report the remaining findings explicitly instead of claiming a clean lint
run. A Ruff pass does not replace the behavioral regression tests below.

IDE inspection through the MCP server is mandatory for every task in this subtree, including
documentation-only changes. Use the `get_file_problems` MCP API (exposed by CLion as
`mcp__clion__get_file_problems`) for each file you create or modify. Inspect existing files before
editing and inspect every changed file again after its final edit. For shared-library changes,
also inspect affected callers and their implementation tests.

Pass the repository-relative `filePath` and explicitly set `errorsOnly: false` so warnings are
included. For example:

```json
{"filePath": "cra/303645/tests/lib/evidence.py", "errorsOnly": false, "timeout": 20000}
```

Review every returned error, warning, and weak warning. Fix actionable findings and rerun
`get_file_problems` on the affected files until the fixes are verified. Do not substitute Ruff,
unit tests, successful compilation, or a visual editor check for this MCP inspection. Do not
disable inspections or add blanket suppressions to make the result appear clean; explain any
remaining intentional convention or confirmed false positive with its file and reason.

If the MCP API is unavailable, fails, or reports `timedOut: true`, do not treat that as a clean
inspection. Retry incomplete inspections where possible, complete other validation, and explicitly
report the unverified files and the limitation at handoff. Never silently skip this check or claim
IDE validation passed without complete MCP results. Report the inspected files and remaining
findings in the final validation summary.

After changing `lib/` or `test_lib.py`, run the dedicated library suite with its branch-coverage
gate:

```bash
.venv/bin/python -m coverage erase
.venv/bin/python -m coverage run --branch --source=lib -m unittest --verbose test_lib.py
.venv/bin/python -m coverage report --show-missing --fail-under=95
```

After changing any functional runner, implementation test, or shared library, run every host-side
automation test:

```bash
.venv/bin/python -m unittest discover \
  --verbose \
  --start-directory . \
  --pattern "test_test_*.py"
```

Confirm discovery runs the intended modules and a nonzero number of tests. `Ran 0 tests` is not
successful runner validation. Use the `test_test_*.py` pattern; broad `test_*.py` discovery also
matches live scripts. Documentation examples must refer to files present in the checkout, or be
explicitly labeled as future examples.

Also run `git diff --check`. Ensure new Python syntax is valid on Python 3.8. Do not satisfy the
coverage threshold with assertions-free calls or tests that only mirror implementation details;
cover meaningful success, rejection, and failure behavior.

Run syntax checks with Python 3.8 for changed Python files if needed, from the directory containing
them or with correct repository-relative paths. Record test counts, coverage, and any validation
limits. Documentation-only edits require Ruff, the mandatory `get_file_problems` MCP inspection,
checking paths and commands, and `git diff --check`; they do not require rerunning unchanged Python
suites.

The workflow `.github/workflows/cra-python-tests.yml` runs these two suites on Ubuntu 22.04. Keep
local commands and workflow commands aligned. The library coverage measurement must use
`test_lib.py` alone so incidental execution by functional-runner tests cannot inflate it.
Keep both workflow path filters sensitive to test inputs outside this subtree, notably
`gw_cfg_default/**`; changes to the generated default snapshot can change test expectations.

## Change checklist

When adding or refactoring a CRA test case:

1. Read its specification under `cra/303645/tasks/` and relevant assessment/IXIT sources.
2. Add or update `test_<id>.py` for live-DUT execution, preserving the task's scope and safety
   policy.
3. Add or update `test_test_<id>.py` for deterministic implementation verification, including actual
   request sequences and failure/recovery paths. Preserve user changes and existing meaningful
   tests.
4. Extend `lib/` only for generally reusable behavior and add its direct tests to `test_lib.py`.
5. Update `requirements.txt` only when necessary.
6. Update the applicable README when commands, structure, public API, or operational expectations
   change.
7. Run Ruff over the entire subtree and fix its findings, then run the applicable validation above,
   including library coverage when shared code changes and the complete `test_test_*.py` suite for
   runner changes. Always run `get_file_problems` through MCP with `errorsOnly: false`, resolve and
   recheck actionable IDE findings, and require a final clean Ruff run.
8. Report the changes and validation results. Do not describe offline tests as live-DUT evidence.
