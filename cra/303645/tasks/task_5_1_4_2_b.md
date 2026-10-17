# Task: Automate ETSI Test Case 5.1-4-2, Unit B

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
verdict. The implementation is [`test_5_1_4_2_b.py`](../tests/test_5_1_4_2_b.py), with paired
offline checks in [`test_test_5_1_4_2_b.py`](../tests/test_test_5_1_4_2_b.py).

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

## Objective

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-4-2, Test Unit
B: Immediate Invalidation of Prior Authentication Values**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface and prove that when
an authentication value is changed, the **previous value is immediately invalidated**, the **new
value is immediately accepted**, and — for interactive credentials — **existing authorized sessions
are immediately dropped**. It must also prove that the read-only (RO) and read/write (RW) M2M keys
can be **rotated independently** without disturbing one another. Unit B is the _invalidation and
session_ view that complements Unit A (`task_5_1_4_2_a.md`), which owns the
_execution/commit/active_ view. Do not re-implement Unit A's obligations here beyond what
invalidation proof requires.

This is a live DUT test. Mock-based tests of the Python implementation are useful for development
but do not replace the physical-gateway run or constitute compliance evidence.

## Deliverables

1. Create `cra/303645/tests/test_5_1_4_2_b.py`, compatible with Python 3.8. The filename normalizes
   test-case id `5.1-4-2` and unit `B` to `test_5_1_4_2_b.py`.
2. Create `cra/303645/tests/test_test_5_1_4_2_b.py` with deterministic host-side implementation
   tests. These are **not** ETSI compliance evidence.
3. Reuse the shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md). Extend `lib/` only
   where a generally reusable, backward-compatible helper is required; do not duplicate an existing
   helper.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable.

Do not modify firmware behavior, source assessments, or unrelated tasks/tests. Related
shared-library, direct-test, fixture, and guidance changes are allowed as required by the
implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_1-4.md`](../Self_Assessment_Test_Group_5_1-4.md),
**lines 60-85** (Test case 5.1-4-2). The source **merges Units A and B** into one section. This task
implements the **Unit B** slice: _the prior value is immediately blocked and the new one accepted_.
The in-scope target entries (source table lines 73-78) and their Unit B obligations:

| Target Entry ID                   | Source runtime behavior                                               | Unit B obligation (this task)                                                                   |
| :-------------------------------- | :-------------------------------------------------------------------- | :---------------------------------------------------------------------------------------------- |
| `AuthMech-LAN-WebUI-Default`      | "Prior hardware-tied password rejected; new parameters accepted"      | Old default `Admin`/`gw_id` login rejected; new custom login accepted; existing session dropped |
| `AuthMech-LAN-WebUI-User-Defined` | "Stale password structure rejected; newly updated credentials active" | Old custom login rejected; new custom login accepted; existing session dropped                  |
| `AuthMech-M2M-API-Bearer-RO`      | "Old token immediately returns `401`; new token maps"                 | Old RO key `401`; new RO key `200`; RW key unaffected by RO rotation                            |
| `AuthMech-M2M-API-Bearer-RW`      | "Old token immediately returns `401`; new token maps"                 | Old RW key `401`; new RW key `200`; RO key unaffected by RW rotation                            |

Relevant IXIT declarations in [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md): `-Default` (57-115),
`-User-Defined` (119-160), `-M2M-API-Bearer-RO` (321-365), `-M2M-API-Bearer-RW` (369-409, which
states the RW key is "independently configurable and uniquely isolated from the read-only key
variable array").

### Scope boundary (do not conflate adjacent units or mechanisms)

- **In scope:** immediate old-value invalidation, immediate new-value acceptance, interactive
  session-drop semantics, and independent RO/RW rotation for the four target entries above.
- **Out of scope — state explicitly in the report and do not exercise:**
  - `AuthMech-LAN-WebUI-Basic` / `-Digest` — absent from the 5.1-4-2 functional table (they carry a
    _pending documentation_ note in the conceptual table);
  - `-Hotspot-Provisioning`, `-Unauthenticated`, `-Disabled`;
  - the pure commit/active-value verification and re-generation success emphasis owned by Unit A.

### Session-invalidation semantics (firmware truth — document, do not invent)

The decisive implementation is `http_server_set_auth()` in
`components/esp32-wifi-manager/src/http_server_auth.c` (lines 55-100), invoked from
`ruuvi_cb_on_change_cfg()` (`main/ruuvi_gateway_main.c` lines 424-442) after every
`POST /ruuvi.json`:

- Authorized interactive sessions are cleared (`http_server_auth_clear_authorized_sessions()`, lines
  16-19) **only when `auth_type`, `auth_user`, or `auth_pass` changes** (lines 67-86).
- Changing **only the API keys does NOT clear interactive sessions** (the keys are copied
  unconditionally at lines 88-99, with no session clear). This is the observable criterion for
  independent key rotation: an authorized interactive session survives an API-key-only change, while
  the old key value is nonetheless immediately replaced.

Consequences the test must encode:

- After an interactive credential change, a request replaying the **previous** `RUUVISESSION` cookie
  must return `401` (session dropped), and the previous plaintext must no longer authenticate.
- After an RO-only or RW-only key change, the old key immediately returns `401` and the new key
  immediately maps, while the untouched key keeps working and any still-authorized interactive
  session remains valid.

Authoritative endpoint reference: [`docs/http_api.md`](../../../docs/http_api.md), sections 113-169,
131-152; endpoints `/auth` (208-213, 363-375, DELETE 491-494), `/ruuvi.json` (240-244, 399-412),
`/history` (289-299). Bearer comparison logic:
`components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c` lines 49-81 (RW accepted on
any route; RO only on non-RW routes; `GET /history` accepts RO, while `POST /ruuvi.json` requires
RW).

## Execution and configuration

No command-line arguments:

Live execution on a separately authorized, dedicated gateway meeting the preconditions below:

```text
cd cra/303645/tests
.venv/bin/python test_5_1_4_2_b.py
```

Load `.env` from the current directory via `lib.config.load_dut_config(Path(".env"))`. `Admin` is
the fixed username; `gw_id` is the default password; `gw_mac`/`gw_hostname` carry the usual
semantics.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for setup, complete wire
  evidence, result/progress models, and error classification. Do not duplicate those facilities.
- Use `GatewayClient` for sessions, logged HTTP, checked JSON, complete and staged interactive
  authentication, challenge/cookie handling, and `calculate_digest_ha1`. Stale-session probes may
  pass the previously captured cookie through the shared request API, but must not parse or manage
  the authentication exchange independently.
- Use `GatewayApi`, the four applicable `AuthMech` constants, `GatewayCfgDesc`, and
  `GatewayCfgLanAuthType`, together with `HttpMethod`, `HttpStatus`, `HttpHeader`, and
  `HttpAuthScheme`. Use `API_INVENTORY` for canonical route selection/coverage.
- Stable internal schemas must be typed immutable models. Dictionaries remain only at real protocol
  mapping boundaries and use `GatewayCfgDesc` keys for configuration JSON.

The shared modules are the primary implementation references. Firmware sources remain behavioral
references only.

Follow the existing test pattern (`FunctionalTest_5_1_4_2_b` class with injected
sessions/random/clock; `execute_test_5_1_4_2_b(...) -> RunResult`; `main() -> int` →
`sys.exit(main())`; `[Step N out of M]` progress; `Open log file: <path>`; final
`Overall verdict: <PASS|FAIL|ERROR>`). Compute stored password values only through
`GatewayClient.calculate_digest_ha1` using the live realm.

## Evidence log

Every invocation, including setup failures, must create `logs/test_5_1_4_2_b_<UTC-date-time>.log`
via `EvidenceLog` (exclusive creation). Record: test case/unit; UTC start/end/duration; DUT
identity; every custom username/plaintext/MD5, RO key, and RW key; the old and new `RUUVISESSION`
cookies; every HTTP request/response; each per-mechanism "old rejected", "new accepted", "session
dropped", and "other key unaffected" assertion; every configuration transition and restoration
attempt; and exception types/tracebacks. Full secrets, cookies, and bodies may appear in the local
log; terminal output stays concise and prints no secrets.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over LAN HTTP port 80;
- initially `lan_auth_default` with both API keys empty/disabled;
- SoftAP inactive; no firmware update / remote-config in progress; no concurrent config changes;
- a configuration backup available in case automatic restoration fails.

A failed precondition is ERROR, not PASS/FAIL/SKIP. The live run intentionally changes the LAN
administrative credential and API keys temporarily; never run against a production gateway.

**Recovery path that cannot lock out the DUT:** always hold the _currently active_ credential
(custom username + plaintext) and the current RW key in memory before applying the next change, and
only transition to values the test generated itself or back to `lan_auth_default`. Restoration to
default re-enables the firmware-generated `Admin`/`gw_id`, so that credential is always the
last-resort recovery. Never set an interactive password the test cannot reproduce.

## Procedure

### 1. Baseline

Authenticate `Admin`/`gw_id`; fetch `GET /ruuvi.json`; require
`lan_auth_type == "lan_auth_default"`, both `*_use` flags `False`, `gw_mac` match when present.
Snapshot the canonical sorted-JSON SHA-256 hash for the final restoration comparison. Capture the
live `realm`.

### 2. `AuthMech-LAN-WebUI-Default`: immediate invalidation + session drop

1. Authenticate `Admin`/`gw_id`; keep this authorized session **S0** and its `RUUVISESSION` cookie.
2. Using S0, `POST /ruuvi.json` with
   `{"lan_auth_type":"lan_auth_ruuvi","lan_auth_user":"<user1>","lan_auth_pass":"<md5_1>"}`
   (distinct custom values); require `200`.
3. **Session drop:** replay S0's cookie — `GET /ruuvi.json` with `Cookie: RUUVISESSION=<S0>` must
   return `401` (or the `302` auth redirect for a `.json` GET in `RUUVI`/`DEFAULT`; assert
   `status not in 2xx` and record the exact code). The prior session no longer authorizes.
4. **Prior value rejected:** an interactive login with the old default `Admin`/`gw_id` must return
   `401`.
5. **New value accepted:** an interactive login with `user1`/`plaintext1` must return `200`.

### 3. `AuthMech-LAN-WebUI-User-Defined`: immediate invalidation + session drop

1. Authenticate as `user1`/`plaintext1`; keep session **S1** and cookie.
2. Using S1, `POST /ruuvi.json` with a new custom `user2`/`md5_2` (type `lan_auth_ruuvi`); require
   `200`.
3. **Session drop:** replaying S1's cookie returns a non-`2xx` (session dropped).
4. **Prior value rejected:** logging in with `user1`/`plaintext1` now returns `401`.
5. **New value accepted:** logging in with `user2`/`plaintext2` returns `200`; keep session **S2**.

### 4. `AuthMech-M2M-API-Bearer-RO`: immediate invalidation via independent rotation

1. Using S2, `POST /ruuvi.json` with `{"lan_auth_api_key":"<RO1>","lan_auth_api_key_rw":"<RW1>"}`;
   require `200`. Fetch `/ruuvi.json` and snapshot its canonical hash. Confirm `Bearer <RO1>` →
   `200` on `/history` and `Bearer <RW1>` → `POST /ruuvi.json` with `{}` returns `200` without
   changing that hash.
2. **Session-not-dropped observable:** because only API keys changed, replaying S2 must still return
   `200` from `/ruuvi.json` (interactive session survives an API-key-only change). Record this as
   the documented independent-rotation semantic.
3. **Rotate RO only:** `POST /ruuvi.json` with `{"lan_auth_api_key":"<RO2>"}` (leave RW unset in the
   body); require `200`. Verify only the intended key transition and capture a fresh authenticated
   configuration hash after rotation for the following RW no-op comparison.
4. **Old rejected:** `Bearer <RO1>` → `401` on `/history`.
5. **New accepted:** `Bearer <RO2>` → `200` on `/history`.
6. **Other key unaffected:** `Bearer <RW1>` → `POST /ruuvi.json` with `{}` returns `200` without
   changing the configuration hash (RW untouched by RO rotation).

### 5. `AuthMech-M2M-API-Bearer-RW`: immediate invalidation via independent rotation

1. **Rotate RW only:** `POST /ruuvi.json` with `{"lan_auth_api_key_rw":"<RW2>"}` (leave RO unset);
   require `200`; fetch `/ruuvi.json` and snapshot its canonical hash.
2. **Other key still reads:** `Bearer <RO2>` → `200` on `/history`, before write probes.
3. **Old rejected:** `Bearer <RW1>` → `POST /ruuvi.json` with `{}` returns `401`.
4. **New accepted:** `Bearer <RW2>` → `POST /ruuvi.json` with `{}` returns `200` without changing
   the configuration hash.
5. **Other key remains scope-confined:** `Bearer <RO2>` → `POST /ruuvi.json` with `{}` returns
   `401`.

Use exactly `{}` for every RW authorization probe. Treat it as an intended no-op whose lack of side
effects must be measured: parser-specific defaults can change omitted fields. Compare the canonical
configuration hash before and after every such POST, including rejected old/RO keys. Do not depend
on `/ap.json`: Wi-Fi scanning is irrelevant in Ethernet-only operation and may not remain available
there.

### 6. Restore default and verify (mandatory `finally`)

In a `finally` path, once any credential may have changed:

1. `POST /ruuvi.json` with
   `{"lan_auth_type":"lan_auth_default","lan_auth_api_key":"","lan_auth_api_key_rw":""}` using the
   most recently authorized custom session. Fallbacks in order, logging each: re-authenticate with
   the last known custom credential and resend; then authenticate `Admin`/`gw_id` and resend. Never
   silently suppress cleanup errors.
2. **Verify restoration:** authenticate `Admin`/`gw_id`; fetch `/ruuvi.json`; require
   `lan_auth_type == "lan_auth_default"`, both `*_use` flags `False`, the canonical hash equal to
   the step-1 baseline, and that the surviving temporary credentials/keys no longer authorize
   (custom login `401`; `Bearer <RO2>` non-`2xx` on `/history`; `Bearer <RW2>` → `401` on
   `POST /ruuvi.json` with `{}`).

Restoration must reject every possibly applied temporary credential and key, including values from
partial setup or failed transitions, and verify the original full configuration. Preserve earlier
per-mechanism FAIL results when unverified restoration makes the overall verdict ERROR.

The test cannot PASS unless restoration is verified. If security assertions passed but restoration
failed, return ERROR with a concise operator warning pointing at the evidence log.

## Verdict and exit codes

Emit separate results for `AuthMech-LAN-WebUI-Default`, `AuthMech-LAN-WebUI-User-Defined`,
`AuthMech-M2M-API-Bearer-RO`, `AuthMech-M2M-API-Bearer-RW`, and final restoration. Overall PASS
requires every old value to be immediately invalidated, every new value immediately accepted,
interactive sessions dropped on credential change, independent RO/RW rotation proven, and
restoration verified.

| Exit code | Meaning                                                                                                                                          |
| :-------- | :----------------------------------------------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                                                                     |
| `1`       | A security assertion failed (old value still valid, new value not accepted, session not dropped, key rotation not isolated, or DUT left mutated) |
| `2`       | Invalid setup, infrastructure/protocol error, or unverified restoration                                                                          |

No silent errors; never convert an exception into a PASS-shaped default.

## Host-side implementation tests

Run the deterministic implementation suite from the repository root with:

```bash
cd cra/303645/tests
.venv/bin/python -m unittest --verbose test_test_5_1_4_2_b.py
```

These offline checks verify the implementation and supply no live-DUT compliance verdict.

Follow `test_test_5_1_1_2_b.py`: `unittest`, fake sessions/responses, temporary directories,
injected dependencies; no live gateway. Fake-gateway hash, challenge, cookie, and wire expectations
must be independent fixtures/reference vectors; never call client code under test to create expected
values. Cover at least:

- the stale-session replay returning non-`2xx` after a credential change, and the surviving session
  after an API-key-only change;
- old interactive credential `401` and new credential `200` for both interactive transitions;
- independent RO rotation (old RO `401`, new RO `200`, RW unaffected) and RW rotation (old RW `401`,
  new RW `200`, RO unaffected and still scope-confined);
- the exact partial bodies for each transition and the exact restoration body;
- runner use of the captured live realm and resulting stored-password payload; direct HA1
  calculation tests stay in `test_lib.py`;
- a still-valid old value, an unaccepted new value, a non-dropped session, or a cross-affected key
  each → FAIL;
- restoration attempted after PASS, FAIL, and raised exceptions, with each fallback exercised;
- restoration failure or surviving temporary credentials prevents PASS (returns ERROR);
- runner setup/evidence integration and terminal progress (direct parsing and collision tests belong
  only in `test_lib.py`);
- timeout/connection failure/malformed JSON → ERROR;
- PASS/FAIL/ERROR exit-code aggregation.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- Each prior authentication value is proven immediately invalid; each new value immediately valid;
  interactive sessions are dropped on credential change; RO and RW keys rotate independently;
  out-of-scope mechanisms are not exercised.
- The DUT is restored to `lan_auth_default` with both keys disabled, verified against the baseline,
  and surviving temporary credentials no longer authorize.
- Setup/infra failures and unverified restoration never produce PASS.
- Implementation and host-side tests run under Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_1-4.md`](../Self_Assessment_Test_Group_5_1-4.md), lines 60-85.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-160 and 321-409.
- [`docs/http_api.md`](../../../docs/http_api.md), sections 113-169, 131-152, and endpoints `/auth`
  (208-213, 363-375, 491-494), `/ruuvi.json` (240-244, 399-412), and `/history` (289-299).
- `components/esp32-wifi-manager/src/http_server_auth.c`, `http_server_set_auth` lines 55-100 and
  `http_server_auth_clear_authorized_sessions` lines 16-19.
- `main/ruuvi_gateway_main.c`, `ruuvi_cb_on_change_cfg` lines 424-442.
- `main/http_server_cb_on_post.c`, `http_server_cb_on_post_ruuvi` lines 44-96.
- `main/gw_cfg_json_parse_lan_auth.c`, lines 55-112 (downgrade-to-default 87-92; API keys 95-111).
- `main/gw_cfg_json_generate.c`, lines 519-580 (`lan_auth_api_key_use` flags 551-566).
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 49-81 and 270-336.
- `cra/303645/tests/test_5_1_1_2_b.py`, `cra/303645/tests/test_5_1_5_2_b.py`, and their host tests.
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`.

## Runner naming

Use `FunctionalTest_5_1_4_2_b` and `execute_test_5_1_4_2_b`; retain `main() -> int` as the
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
