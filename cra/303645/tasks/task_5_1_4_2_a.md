# Task: Automate ETSI Test Case 5.1-4-2, Unit A

## Mandatory factory-default precondition

Before test-specific activity, authenticate and compare every gateway configuration field relevant to
this test directly with `gw_cfg_default/gw_cfg_default_gen_ui.json` through the shared `lib.config`
helper. This generated file already matches the public UI/API representation; do not derive it from
the raw saved configuration or handle hidden secrets. A missing, wrongly typed, or non-default value
is setup ERROR: print the shared CONFIGURE-button factory-reset instruction and stop without changing
the DUT to manufacture a baseline.

## Review regression requirements

Follow the [CRA test rules](../tests/AGENTS.md), especially functional-test rules and host-side
implementation tests. These requirements qualify the factory-default precondition above:

- Catch Basic/Digest `GatewayAuthenticationModeError` and mark failed default login as setup ERROR
  with the shared recovery message in evidence and terminal output. Once configuration is available,
  validate its MAC before default fields; wrong, malformed, or absent identity must not trigger reset
  advice for a non-default baseline. Do not manufacture defaults by changing the DUT.
  A successful login must reach the authenticated identity read even if `/auth` reports a
  non-default mode; only a verified matching MAC permits reset advice for that mode.
- Complete safer probes across all schemes before potentially mutating probes within the same
  prepared state; include positive reads where applicable. Abort on unexpected negative success,
  while preserving mandatory restoration. Assert exact ordered method/path/scheme/body/session
  records at the fake HTTP boundary, including the distinction between probes and recovery.
- Independently validate complete login bodies, session cookies, and outstanding challenges in the
  fake server. Model Basic/Digest challenges faithfully and include a full run through the real
  client. Cover wrong passwords, corrupt responses, late safer-phase failures, and dangerous-phase
  aborts wherever this task exercises authentication.
- Attribute each rejected security assertion and final hash check to its actual mechanism. Preserve
  completed mechanisms when a later one fails; never leave a failed mechanism PASS or NOT RUN.
  Baseline/setup, transport, and protocol exceptions are ERROR. Once the baseline is validated,
  the credential changes, their read-backs, and new-value logins are this unit's functional
  assertions: a rejected change or mismatched commit is FAIL for the active mechanism.
  Retain new and previous recovery credentials across partially applied changes.
- Use `FACTORY_RESET_MESSAGE`; the reset completion condition is red LED 200 ms on/200 ms off
  after boot-time erasure. Approximately eleven seconds is observed elapsed time, not a threshold.
  Verify recovery advice against both restart and erasure paths.
- Check referenced documents and predecessor scripts exist before changing links. Shared-library
  tests belong only in `test_lib.py`; case tests cover integration. Run Python 3.8 host discovery,
  required library coverage for shared changes, whole-subtree Ruff before/after, and the mandatory
  IDE inspection. Offline tests are not compliance evidence.

## Objective

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case
5.1-4-2, Test Unit A: Functional Execution of Authentication-Value Changes**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface and prove that an
administrator can **successfully change every in-scope authentication value**, that each change is
**committed to persistent configuration**, and that each **new value becomes actively usable**. Unit A
is the *execution and success* view: the change happens, it is stored, and the new credential/token
works. The complementary *invalidation and session* view is covered by Unit B
(`task_5_1_4_2_b.md`) — do not duplicate its emphasis here.

This is a live DUT test. Mock-based tests of the Python implementation are useful for development but
do not replace the physical-gateway run or constitute compliance evidence.

## Deliverables

1. Create `cra/303645/tests/test_5_1_4_2_a.py`, compatible with Python 3.8. The filename normalizes
   test-case id `5.1-4-2` and unit `A` to `test_5_1_4_2_a.py`.
2. Create `cra/303645/tests/test_test_5_1_4_2_a.py` with deterministic host-side implementation tests.
   These are **not** ETSI compliance evidence.
3. Reuse the shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md). Extend `lib/` only where a
   generally reusable, backward-compatible helper is required; do not duplicate an existing helper.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable.

Do not modify firmware behavior or self-assessment source documents. Maintain this runner, its
implementation tests, and related guidance; preserve unrelated scripts and user changes.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_1-4.md`](../Self_Assessment_Test_Group_5_1-4.md),
**lines 60-85** (Test case 5.1-4-2). Note that the source **merges Units A and B** into one section
titled "Test Unit A & B: Functional Execution and Success Verification". This task implements the
**Unit A** slice of that combined section: *the change executes and the new value is committed and
active*. The in-scope target entries from the source table (lines 73-78) are:

| Target Entry ID                   | Functional mutation executed                | Unit A obligation (this task)                           |
| :-------------------------------- | :------------------------------------------ | :------------------------------------------------------ |
| `AuthMech-LAN-WebUI-Default`      | Migrate from default to user-defined        | New user/password committed and authenticates           |
| `AuthMech-LAN-WebUI-User-Defined` | Modify the custom administrative parameters | Re-modified user/password committed and authenticates   |
| `AuthMech-M2M-API-Bearer-RO`      | Re-trigger the client token generation loop | New RO key committed and authorizes a read endpoint     |
| `AuthMech-M2M-API-Bearer-RW`      | Re-trigger the client token generation loop | New RW key committed and authorizes an RW-only endpoint |

Relevant IXIT declarations in [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md):

- `AuthMech-LAN-WebUI-Default`: lines 57-115 (default `x-ruuvi-interactive` credential derived from
  the hardware `DEVICEID`, i.e. `gw_id`).
- `AuthMech-LAN-WebUI-User-Defined`: lines 119-160 (user-defined username/password replacing the
  default, same challenge-response pipeline).
- `AuthMech-M2M-API-Bearer-RO` / `-RW`: lines 321-365 / 369-409 (independently configurable
  high-entropy keys `lan_auth_api_key` / `lan_auth_api_key_rw`).

### Scope boundary (do not conflate adjacent units or mechanisms)

- **In scope:** the four target entries above.
- **Out of scope — state explicitly in the report and do not exercise as change targets:**
  - `AuthMech-LAN-WebUI-Basic` and `AuthMech-LAN-WebUI-Digest` — they appear in the 5.1-4-*conceptual*
    table with a *pending documentation* note and are **absent from the 5.1-4-2 functional table**;
  - `AuthMech-Hotspot-Provisioning`, `-Unauthenticated`, `-Disabled` — not credential-bearing here;
  - the immediate-invalidation, prior-value-rejection, session-drop, and independent RO/RW rotation
    emphases — those are Unit B's obligation.

Where the source narration ("prior password rejected") overlaps invalidation, Unit A only needs to
confirm the *new* value works and is committed; the rigorous old-value invalidation proof belongs to
Unit B. Keep a light confirmation that the old value no longer authenticates so "migrated" is real,
but do not build the session-invalidation machinery here.

## Firmware behavior that makes the change observable

- `POST /ruuvi.json` copies the current configuration, applies the partial JSON body, persists it, and
  calls `ruuvi_cb_on_change_cfg()` → `settings_save_to_flash()` + `http_server_set_auth()`
  (`main/http_server_cb_on_post.c` lines 44-96; `main/ruuvi_gateway_main.c` lines 424-442). It
  responds `200 {}`.
- The interactive stored password is `MD5(username + ":" + realm + ":" + plaintext)`; the login
  response is `SHA256(challenge + ":" + HA1)` (`ixit_1-AuthMech.md` lines 96-111; the shared
  `GatewayClient.authenticate_interactive()` performs this). Capture `realm` from the live
  `challenge["realm"]` of a successful default login — the URL hostname may be an IP/alias and need
  not equal the advertised realm.
- Selecting `lan_auth_type = "lan_auth_ruuvi"` with a username/password that differ from the default
  yields a genuine user-defined state; if the submitted user/pass equal the default, the firmware
  downgrades the type back to `DEFAULT` (`main/gw_cfg_json_parse_lan_auth.c` lines 87-92) — so always
  use distinct custom values.
- The read-back `/ruuvi.json` hides plaintext and instead exposes `lan_auth_type`, `lan_auth_user`,
  `lan_auth_api_key_use`, and `lan_auth_api_key_rw_use` (`main/gw_cfg_json_generate.c` lines 519-580).
- API keys are compared in `http_server_handle_req_check_auth_bearer()`
  (`components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c` lines 49-81): the RW key is
  accepted on any route; the RO key only on non-RW routes. `GET /history` accepts RO, while
  `POST /ruuvi.json` requires RW. Use exactly `{}` as the POST body so no configuration field changes,
  and verify the canonical configuration hash remains unchanged.

Authoritative endpoint reference: [Authentication model](../../../docs/http_api.md#authentication-model),
[GET /auth](../../../docs/http_api.md#get-auth), [POST /auth](../../../docs/http_api.md#post-auth),
[GET /ruuvi.json](../../../docs/http_api.md#get-ruuvijson),
[POST /ruuvi.json](../../../docs/http_api.md#post-ruuvijson), and
[GET /history](../../../docs/http_api.md#get-history).
Read-back and successful use establish the active committed values; this test does not reboot or
power-cycle the DUT and does not independently prove persistence across power loss.

## Execution and configuration

No command-line arguments:

```text
cd cra/303645/tests
.venv/bin/python test_5_1_4_2_a.py
```

Load `.env` from the current directory via `lib.config.load_dut_config(Path(".env"))`. `Admin` is the
fixed username; `gw_id` is the default password; `gw_mac`/`gw_hostname` carry the usual semantics
(verify `gw_mac` against authenticated `/ruuvi.json` when present; base URL `http://<gw_hostname>`).

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for setup validation, evidence/wire
  logging, results/progress, and error classification; do not reimplement them.
- Use `lib.gateway.GatewayClient` for sessions, logged HTTP, checked JSON, complete and staged
  interactive authentication, challenge/cookie handling, and `calculate_digest_ha1`. The task must not
  construct interactive requests or calculate stored HA1 values independently.
- Use `GatewayApi`, the four applicable `AuthMech` constants, `GatewayCfgDesc`, and
  `GatewayCfgLanAuthType`, plus `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` from
  `lib.http_api`. Use `API_INVENTORY` when selecting or validating canonical routes.
- Stable internal schemas must use typed immutable models. Keep dictionaries only at genuine protocol
  mapping boundaries, using `GatewayCfgDesc` descriptor keys for gateway configuration JSON.

Treat the shared modules as the primary implementation references; firmware/Web-UI sources define
observable behavior only and are not implementation recipes.

Follow the existing test pattern: a `FunctionalTest` class with injected sessions/random/clock; an
`execute(...)` entry point; `main() -> int` ending in `sys.exit(main())`; `[Step N out of M]` progress;
`Open log file: <path>` printed once; a final `Overall verdict: <PASS|FAIL|ERROR>` line. Obtain the
stored password value from `GatewayClient.calculate_digest_ha1(user, realm, plaintext)`.

## Evidence log

Every invocation, including setup failures, must create `logs/test_5_1_4_2_a_<UTC-date-time>.log` via
`EvidenceLog` (exclusive creation). Record: test case/unit; UTC start/end/duration; DUT identity; every
temporary custom username, plaintext, MD5 value, RO key, and RW key; every HTTP request/response; each
per-mechanism "change committed" and "new value active" assertion; every configuration transition and
restoration attempt; and exception types/tracebacks. Full secrets and bodies may appear in the local
log; terminal output stays concise and prints no secrets.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over LAN HTTP port 80;
- initially `lan_auth_default` with both API keys empty/disabled;
- SoftAP inactive; no firmware update / remote-config in progress; no concurrent config changes;
- a configuration backup available in case automatic restoration fails.

A failed precondition is ERROR, not PASS/FAIL/SKIP. The live run intentionally changes the LAN
administrative credential and API keys temporarily; never run against a production gateway.

**Lock-out avoidance:** keep the currently-active credential and RW key in memory at every step; only
ever transition to a value the test itself generated (and can therefore reproduce) or back to
`lan_auth_default` (which restores the firmware-generated `Admin`/`gw_id`). Because restoration returns
to default, retain `Admin`/`gw_id` as a final recovery candidate. It only works if the DUT is already
in default state (including a restoration whose response was lost); it cannot override an active
custom credential.

## Procedure

### 1. Baseline

Authenticate `Admin`/`gw_id`; fetch `GET /ruuvi.json`; require `lan_auth_type == "lan_auth_default"`,
both `*_use` flags `False`, and `gw_mac` match when present. Snapshot the canonical sorted-JSON
SHA-256 hash for the final restoration comparison. Capture `realm` from the login challenge.

### 2. Change `AuthMech-LAN-WebUI-Default` → user-defined and verify commit + active

1. Generate a distinct custom username and high-entropy plaintext password through the client's
   injected randomness, then use its HA1 helper with the captured realm.
2. Using the authorized default session, `POST /ruuvi.json` with
   `{"lan_auth_type":"lan_auth_ruuvi","lan_auth_user":"<user1>","lan_auth_pass":"<md5_1>"}`; require
   `200`.
3. **Commit check:** re-authenticate with the new custom credential (the change cleared the old
   session), fetch `/ruuvi.json`, and require `lan_auth_type == "lan_auth_ruuvi"` and
   `lan_auth_user == "<user1>"`.
4. **Active check:** the custom login returned `200` (new value works). Lightly confirm the migration
   is real by requiring the old default `Admin`/`gw_id` interactive login now returns `401` (leave the
   exhaustive invalidation proof to Unit B).

### 3. Modify `AuthMech-LAN-WebUI-User-Defined` again and verify commit + active

Generate `user2`/`plaintext2`/`md5_2`. Using the `user1` authorized session, `POST /ruuvi.json` with
`{"lan_auth_type":"lan_auth_ruuvi","lan_auth_user":"<user2>","lan_auth_pass":"<md5_2>"}`; require `200`.
Commit check: re-authenticate as `user2`, fetch `/ruuvi.json`, require
`lan_auth_type == "lan_auth_ruuvi"` and `lan_auth_user == "<user2>"`.
Active check: the `user2` login succeeded.

### 4. Change `AuthMech-M2M-API-Bearer-RO` and `-RW` (token generation loop) and verify

1. Generate distinct RO and RW keys through the shared client's injected randomness, distinct from
   `gw_id`.
2. Using the `user2` session, `POST /ruuvi.json` with
   `{"lan_auth_api_key":"<RO1>","lan_auth_api_key_rw":"<RW1>"}`; require `200`.
3. **Commit check:** fetch `/ruuvi.json`; require `lan_auth_api_key_use is True` and
   `lan_auth_api_key_rw_use is True`; snapshot its canonical hash.
4. **Active check:** `Bearer <RO1>` → `200` on `/history`; `Bearer <RW1>` →
   `POST /ruuvi.json` with `{}` returns `200`; require the configuration hash to remain unchanged.
5. **Re-trigger the generation loop once** (the source's "re-triggered client generation loop"):
   generate `RO2`/`RW2`, `POST /ruuvi.json` with the new keys, require `200`, fetch `/ruuvi.json` and
   snapshot its canonical hash, then verify `Bearer <RO2>` → `200` on `/history` and
   `Bearer <RW2>` → `POST /ruuvi.json` with `{}` returns `200` without changing that hash. (Old-key
   rejection after rotation is Unit B's rigorous obligation; here only confirm the *new* keys are
   committed and active.)

### 5. Restore default and verify (mandatory `finally`)

Once any credential may have changed, restoration is mandatory. In a `finally` path:

1. `POST /ruuvi.json` with
   `{"lan_auth_type":"lan_auth_default","lan_auth_api_key":"","lan_auth_api_key_rw":""}` using the most
   recently authorized custom session. Selecting `lan_auth_default` restores the firmware-generated
   `Admin`/`gw_id` credential and the empty strings disable both keys.
2. Fallbacks in order, logging each: re-authenticate with the last known custom credential and resend;
   retain and try previously generated candidates if a transition may not have applied, then
   authenticate `Admin`/`gw_id` and resend. Save each candidate before sending its change, even if
   the POST times out or the subsequent login fails. Never silently suppress cleanup errors.
3. **Verify restoration:** authenticate `Admin`/`gw_id`; fetch `/ruuvi.json`; require
   `lan_auth_type == "lan_auth_default"`, both `*_use` flags `False`, and the canonical hash equal to
   the step-1 baseline.

The test cannot PASS unless restoration is verified. If security assertions passed but restoration
failed, return ERROR with a concise operator warning pointing at the evidence log.

## Verdict and exit codes

Emit separate results for `AuthMech-LAN-WebUI-Default`, `AuthMech-LAN-WebUI-User-Defined`,
`AuthMech-M2M-API-Bearer-RO`, `AuthMech-M2M-API-Bearer-RW`, and final restoration. Overall PASS requires
every change to be committed and active and restoration to be verified.
Gate PASS explicitly on every required outcome being PASS; a skipped or incomplete mechanism must
not pass merely because no exception was raised. Transport/protocol failures mark the active
mechanism ERROR without erasing completed mechanisms.

| Exit code | Meaning                                                                                                                             |
| :-------: | :---------------------------------------------------------------------------------------------------------------------------------- |
|    `0`    | Overall PASS                                                                                                                        |
|    `1`    | A functional assertion failed (a change was not committed, a new value did not authenticate/authorize, or the DUT was left mutated) |
|    `2`    | Invalid setup, infrastructure/protocol error, or unverified restoration                                                             |

No silent errors; never convert an exception into a PASS-shaped default.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py`: `unittest`, fake sessions/responses, temporary directories, injected
dependencies; no live gateway. Fake-gateway hashes, cookies, and wire expectations must be independent
fixed/reference fixtures and must not call the client code under test to derive expected values. Cover
at least:

- the exact partial bodies for each change (interactive user-defined, re-modified user-defined, RO/RW
  key set, RO/RW re-generation) and the exact restoration body;
- MD5 stored-password computation using a captured realm (not the URL hostname);
- commit checks reading back `lan_auth_type`/`lan_auth_user`/`*_use` flags;
- active checks: new custom login `200`, `Bearer <RO>` `200` on `/history`, `Bearer <RW>` `200` on
  `POST /ruuvi.json` with `{}`; and unchanged configuration hashes after no-op RW POSTs;
- a failed commit or an inactive new value → FAIL;
- explicit failure ownership: RO read/commit failures belong to RO; RW POST/non-mutation failures
  belong to RW. A shared key-write failure affects both. Preserve completed RO after its second
  generation check if the later RW check fails; do not select the first NOT RUN result;
- a complete run through `GatewayClient` and a fake HTTP transport that validates independent
  challenge/password/cookie expectations, in addition to isolated orchestration fixtures;
- restoration attempted after PASS, FAIL, and raised exceptions, with each fallback exercised;
- restoration failure prevents PASS (returns ERROR);
- setup/evidence integration and terminal progress format; direct library tests stay in `test_lib.py`;
- timeout/connection failure/malformed JSON → ERROR;
- PASS/FAIL/ERROR exit-code aggregation.
- identity-before-mode checks through a complete wire login, both custom-mode read-back checks,
  and an incomplete-mechanism result that cannot produce overall PASS;
- the full ordered method/path/body/session sequence, including cookie-free bearer checks and
  restoration through the most recent session before credential fallbacks.

Run all available implementation-test modules (never the live-DUT scripts):

```text
cd cra/303645/tests
.venv/bin/python -m unittest discover --verbose --start-directory . --pattern "test_test_*.py"
```

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- Each in-scope authentication value is changed, its commit is read back, and its new value is proven
  active; Basic/Digest and other out-of-scope mechanisms are not exercised.
- The DUT is restored to `lan_auth_default` with both keys disabled, verified against the baseline.
- Setup/infra failures and unverified restoration never produce PASS.
- Implementation and host-side tests run under Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_1-4.md`](../Self_Assessment_Test_Group_5_1-4.md), lines 60-85.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-160 and 321-409.
- [`docs/http_api.md`](../../../docs/http_api.md): use the stable endpoint links in
  [Firmware behavior that makes the change observable](#firmware-behavior-that-makes-the-change-observable).
- `main/http_server_cb_on_post.c`, `http_server_cb_on_post_ruuvi` lines 44-96.
- `main/ruuvi_gateway_main.c`, `ruuvi_cb_on_change_cfg` lines 424-442.
- `main/gw_cfg_json_parse_lan_auth.c`, lines 55-112 (downgrade-to-default 87-92; API keys 95-111).
- `main/gw_cfg_json_generate.c`, lines 519-580 (`lan_auth_api_key_use` flags 551-566).
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 49-81 and 270-336.
- `components/esp32-wifi-manager/src/http_server_auth.c`, `http_server_set_auth` lines 55-100.
- `cra/303645/tests/test_5_1_1_2_b.py`, `cra/303645/tests/test_5_1_5_2_b.py`, and their host tests.
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`.

## Runner naming

Name the live-test runner and execution function `FunctionalTest_<test_id>` and
`execute_test_<test_id>`, respectively, where `<test_id>` is the normalized test-script identifier.
