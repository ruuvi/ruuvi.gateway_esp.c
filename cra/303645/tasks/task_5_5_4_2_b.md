# Task: Automate ETSI Test Case 5.5-4-2, Unit b

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
verdict. The runner and paired implementation suite are present in this checkout; see
[Execution and configuration](#execution-and-configuration) for usage. Their presence does not
establish a live-DUT verdict.

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

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-4-2, Test Unit
b: Grant Valid Authorized Access**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface (port 80, plain HTTP)
and functionally verify that authenticated subjects with proper rights are **granted** access to the
two network-exposed services declared in [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md)
(`SoftServ-Local-Management-WebUI`, lines 73-101, and `SoftServ-Local-Programmatic-API`, lines
103-131). It must confirm positive authorization for:

1. the **default** Ruuvi interactive Web-UI login (`Admin`/`gw_id`);
2. a **custom** (user-defined) Ruuvi interactive Web-UI login;
3. the **read-only (RO)** M2M bearer scope on read endpoints;
4. the **read/write (RW)** M2M bearer scope on an RW-scoped endpoint.

It must use safe, representative endpoints and prove positive authorization **without unsafe or
persistent configuration changes**. The configuration-change commit path (writing and persisting a
parameter) is verified separately by test case 5.5-5-2 A and is **out of scope** here to avoid
duplication.

This is a live-DUT test. Mock-based tests validate the Python implementation only; the physical
gateway is authoritative for the verdict.

## Deliverables

1. Create `cra/303645/tests/test_5_5_4_2_b.py`, compatible with Python 3.8.
2. Create `cra/303645/tests/test_test_5_5_4_2_b.py` containing deterministic host-side `unittest`
   implementation tests that mock HTTP and are clearly labelled as implementation tests, not ETSI
   evidence.
3. Extend `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is
   required. Do not duplicate an existing helper.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable.

Do not modify firmware behavior, source assessments, or unrelated tasks/tests. Make the related
shared-library, direct-test, fixture, and guidance updates required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), test
case 5.5-4-2 (lines 79-102), specifically the **Unit b** row of the functional matrix (line 93) and
the assessment justification (lines 96-102).

Unit b declares these DUT actions and expected behaviors:

| Step | Action on DUT                                             | Self-assessment expected behavior                                          |
| :--- | :-------------------------------------------------------- | :------------------------------------------------------------------------- |
| 1    | Authenticate via Web-UI with a valid `POST /auth` payload | Authentication succeeds; ECDH session established; resource access granted |
| 2    | `GET /history` using a valid Read-Only token              | `200` JSON sensor-metrics array                                            |
| 3    | `POST /ruuvi.json` using a valid Read/Write token         | Configuration update processed and saved to NVS                            |

### Scope refinement (unit-boundary rule)

Unit b proves that valid credentials are **accepted**. To avoid overlapping with test case 5.5-5-2 A
(which owns the mutate-and-persist commit path and the RO-vs-RW write boundary), this test
demonstrates RW-scope positive authorization through an **RW-authorized no-op write** that returns
`200` without changing configuration, rather than through a persisting configuration update:

- The firmware requires an **RW** bearer key for `POST /ruuvi.json`. Use `{}` as the intended no-op
  and prove configuration preservation by full-state comparison; partial parsers may rewrite omitted
  fields. See [`docs/http_api.md`](../../../docs/http_api.md) lines 141-148 and 399-412.

Therefore the RW-scope positive check targets `POST /ruuvi.json` with the RW key and exactly `{}` as
the body, followed by a canonical configuration-hash comparison. A persisting field change belongs
to 5.5-5-2 A. Do **not** repeat negative/insufficient-privilege probes here (those belong to Unit
a); include only the minimal authenticated setup needed to provision credentials.

## Relevant IXIT declarations

- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md): `AuthMech-LAN-WebUI-Default` (lines 57-115),
  `AuthMech-LAN-WebUI-User-Defined` (lines 119-160), `AuthMech-M2M-API-Bearer-RO` (lines 321-365),
  `AuthMech-M2M-API-Bearer-RW` (lines 369-409).
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md): `SecComMech-WebUI-Session` (lines 82-114),
  `SecComMech-LAN-Bearer-Authentication` (lines 163-200, including the RO-reads / RW-writes
  privilege separation).
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md): the two in-scope services (lines 73-131).
- [`ixit_18-AccCtrl.md`](../ixit_18-AccCtrl.md): informational only (hardware/partition isolation,
  not network auth).
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md): `LogIntf-HTTP-Server` (lines 9-40).

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for setup, evidence/wire logging,
  common results/progress, and failure classification.
- Use `GatewayClient` for sessions, logged HTTP, checked JSON, complete interactive authentication
  for both logins, staged auth where response inspection is needed, challenge/cookie parsing, and
  `calculate_digest_ha1`. Do not duplicate any authentication or request construction.
- Use the four applicable `AuthMech` constants plus `GatewayApi`, `GatewayCfgDesc`, and
  `GatewayCfgLanAuthType`. Use `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` from
  `lib.http_api`; use `API_INVENTORY` only when validating that selected routes are canonical.
- Stable schemas must be typed immutable models. Dictionaries are restricted to real protocol
  mapping boundaries and use `GatewayCfgDesc` keys where applicable.

The shared modules are the primary implementation references. Firmware/Web-UI sources remain
behavioral references only.

Preserve compatibility with the existing tests and their host-side suites. If a shared API must
change, keep its behavior and tests passing.

Follow the existing test pattern: a `FunctionalTest_5_5_4_2_b` class with dependency injection;
`execute_test_5_5_4_2_b(...) -> RunResult`; `main() -> int`; `sys.exit(main())`;
`[Step N out of M] <description>` progress; a single `Open log file: <path>` line; a final
`Overall verdict: <PASS|FAIL|ERROR>` line.

## Execution and configuration

The test takes no command-line arguments. Run the commands below from `cra/303645/tests` after the
initial `cd` from the repository root.

[`test_5_5_4_2_b.py`](../tests/test_5_5_4_2_b.py) implements the Unit b procedure. Use a dedicated
LAN gateway with factory-default authentication, disabled API keys, inactive configuration hotspot,
no firmware update or remote configuration operation, and no concurrent configuration changes. Keep
a backup available. Run:

```bash
cd cra/303645/tests
.venv/bin/python test_5_5_4_2_b.py
```

The runner verifies two stable configuration reads, checks default interactive access, and
provisions temporary custom credentials plus RO/RW bearer keys using the DUT's advertised realm. It
checks custom interactive access and RO history/status/configuration reads before an RW
`POST /ruuvi.json` with exactly `{}`. Full configuration comparisons detect side effects without
excluding unrelated fields. The configuration commit-and-persist test belongs to 5.5-5-2 A.

Cleanup runs even after a partially applied write. It tries the retained RW key, custom login, then
default login, and verifies the full baseline and revocation of all temporary credentials.
Unverified restoration returns ERROR while retaining earlier per-mechanism FAIL evidence. Secrets
are recorded only in the local evidence log. No firmware-update, reset, storage, certificate, or
connection routes are called.

Run the paired offline suite with `.venv/bin/python -m unittest --verbose test_test_5_5_4_2_b.py`.
These implementation tests use the real client with fake HTTP transport; they are not live-DUT
evidence.

Load `.env` from the current working directory (same format and semantics as the other tests): fixed
`Admin` username, `gw_id` as its default password, `gw_mac` verified against the authenticated
configuration when present, base URL `http://<gw_hostname>`. A missing/malformed/unknown/duplicate
key is an invalid-setup ERROR (exit 2). `.env`, `logs/`, and `.venv/` remain git-ignored.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_5_4_2_b_<UTC-date-time>.log
```

Use `EvidenceLog`. The log must contain: test case/unit; UTC start/end/duration; DUT identity; the
temporary custom username/password, RO key, and RW key; every HTTP request/response;
interactive-auth and ECDH material; each assertion; per-scope positive-authorization results
(default interactive, custom interactive, RO reads, RW route); the final restoration and
non-mutation verification; and exception type/traceback on error. Full secrets and bodies may be
written to this local log only. Keep terminal output concise.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over its LAN interface on HTTP port 80;
- initially in `lan_auth_default` mode with both M2M keys disabled/empty;
- configuration SoftAP inactive; no firmware-update or remote-config operation in progress;
- not concurrently modified by another process; a backup available if restoration fails.

Do not reset/reconfigure to manufacture the baseline. A failed precondition is ERROR.

**Safety / configuration preservation.** This test intentionally provisions a temporary custom
interactive credential and temporary RO/RW keys, then removes them. To keep positive checks from
changing configuration:

- prove RO scope only through read endpoints (`GET /history`, `GET /status.json`,
  `GET /ruuvi.json`);
- prove RW scope through `POST /ruuvi.json` with exactly `{}` and verify the canonical configuration
  hash remains unchanged;
- never call firmware-update, storage-init, reset, certificate, or `connect*` routes;
- restore the DUT (default auth, keys cleared) in a mandatory `finally` path and verify restoration.

## Firmware behavior reference

Authoritative: [`docs/http_api.md`](../../../docs/http_api.md). Key facts:

- `GET /auth` starts the interactive session; a valid `POST /auth` challenge-response returns `200`
  with an authorized session
  ([`http_server_handle_req_post_auth.c` L248](../../../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c#L248),
  docs lines 363-375).
- Setting `lan_auth_type = "lan_auth_ruuvi"` with `lan_auth_user`/`lan_auth_pass` activates a
  user-defined interactive credential that replaces the default; the stored password value is
  `MD5(username + ":" + realm + ":" + plaintext_password)`
  ([`gw_cfg_json_parse_lan_auth.c` L35-111](../../../main/gw_cfg_json_parse_lan_auth.c#L35),
  [`page_lan_auth.mjs` L146-196](../../../ruuvi.gwui.html/src/page_lan_auth.mjs#L146)).
- An accepted bearer key returns an auth JSON with `200`; an RO key authorizes reads; an RW key
  authorizes reads, all POSTs, and all DELETEs
  ([`http_server_handle_req_get_auth.c` L257-296](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257),
  docs lines 131-152, 190-195, 556-583).
- The realm advertised by the DUT (`challenge["realm"]`) may differ from `gw_hostname`; always use
  the live captured realm when computing/verifying custom credentials.
- `http_server_cb_on_post_ruuvi()` starts from the current configuration, but each parser determines
  whether omitted fields are preserved; full-state comparisons must detect unintended changes
  ([`http_server_cb_on_post.c` L44-58](../../../main/http_server_cb_on_post.c#L44)).

## Required procedure

### 1. Initialize and validate the baseline

1. Create the evidence log before reading `.env`.
2. Load `.env`; construct `GatewayClient` with finite timeouts and the test user-agent.
3. Default interactive login: `authenticate_interactive("Admin", gw_id)` -> require `200`. Capture
   the live `challenge["realm"]` from the `InteractiveAuthResult`.
4. Fetch `/ruuvi.json` with the authorized session; require `lan_auth_type == "lan_auth_default"`,
   both API-key-use flags `False`, and `gw_mac` match when present.
5. Snapshot the required default fields and a canonical sorted-JSON SHA-256 hash for the final
   non-mutation check.

Missing/mistyped fields, failed default login, or incorrect baseline is ERROR.

### 2. Confirm default interactive positive authorization

Using the authorized default session, confirm resource access is granted: `GET /ruuvi.json` and
`GET /status.json` return `200` with well-formed JSON. Record the "default interactive" positive
result. (This directly satisfies Unit b step 1 for the default credential.)

### 3. Provision a temporary custom interactive credential and RO/RW keys

Generate a custom plaintext password, RO key, RW key, and distinct username through the shared
client's injected randomness. Compute the stored value with `GatewayClient.calculate_digest_ha1`
using the **live captured realm**.

Using the authorized default session, send a partial `POST /ruuvi.json`:

```json
{
  "lan_auth_type": "lan_auth_ruuvi",
  "lan_auth_user": "<custom-username>",
  "lan_auth_pass": "<MD5-value>",
  "lan_auth_api_key": "<temporary-RO-key>",
  "lan_auth_api_key_rw": "<temporary-RW-key>"
}
```

The authentication change clears existing interactive sessions. Retain the temporary RW key as the
primary cleanup credential. Confirm the prepared state: an authenticated `/ruuvi.json` reports
`lan_auth_ruuvi` and both API-key-use flags `True`. A failure here is ERROR / invalid prepared
state, followed by mandatory restoration.

### 4. Confirm custom interactive positive authorization

Perform `authenticate_interactive(custom_username, custom_plaintext_password)` using the live realm
-> require the login `POST /auth` to return `200` and the session to be authorized (a follow-up
`GET /ruuvi.json` returns `200`). Record the "custom interactive" positive result. (Satisfies Unit b
step 1 for the user-defined credential; covers `AuthMech-LAN-WebUI-User-Defined`.)

### 5. Confirm RO bearer positive authorization

Using a session with no interactive cookie and `Authorization: Bearer <temporary-RO-key>`:

- `GET /history` -> require `200` and the JSON object defined by
  [the history schema](../../../schemas/ruuvi_history.schema.json), including its `data` object; do
  not encode the source's informal "array" wording as a top-level array requirement;
- `GET /status.json` -> require `200`;
- `GET /ruuvi.json` -> require `200`.

Record the "RO bearer" positive result (`AuthMech-M2M-API-Bearer-RO`).

### 6. Confirm RW bearer positive authorization (no configuration change)

Using `Authorization: Bearer <temporary-RW-key>`:

- snapshot the canonical authenticated `/ruuvi.json` hash;
- `POST /ruuvi.json` with exactly `{}` -> require `200`;
- fetch authenticated `/ruuvi.json` again and require the canonical hash to remain unchanged;
- optionally `GET /history` -> require `200` (an RW key is also valid for reads).

Record the "RW bearer" positive result (`AuthMech-M2M-API-Bearer-RW`). Do not include any
configuration fields in this POST; the write-commit proof belongs to 5.5-5-2 A.

### 7. Restore the DUT in a `finally` path

Restoration is mandatory once temporary credentials may have been applied.

Primary restoration: `POST /ruuvi.json` with `Authorization: Bearer <temporary-RW-key>` and body:

```json
{"lan_auth_type": "lan_auth_default", "lan_auth_api_key": "", "lan_auth_api_key_rw": ""}
```

`lan_auth_default` restores the firmware-generated default `Admin` credential; empty key strings
disable both bearer mechanisms. Fallbacks, in order, if primary restoration is denied or its
response is lost:

1. authenticate with the temporary custom username/password and resend the restoration body;
2. authenticate with `Admin`/`gw_id` and resend it (covers the case where the transition never took
   effect).

Log every attempt; never silently suppress cleanup errors.

Verify restoration by authenticating as `Admin`/`gw_id`, fetching `/ruuvi.json`, and requiring
`lan_auth_default`, both API-key-use flags `False`, the canonical hash matching the baseline, and
that the temporary custom credential, RO key, and RW key no longer authorize access. The test cannot
PASS unless restoration is verified; if security assertions passed but restoration failed, return
ERROR with a concise operator warning.

### 8. Non-mutation proof

The final baseline comparison in step 7 is the non-mutation proof: after restoration, the canonical
full-configuration hash must match the initial baseline (excluding only fields proven volatile). Any
persistent difference is FAIL and must be listed in the evidence log.

## Verdict and exit codes

Emit separate results for:

- `AuthMech-LAN-WebUI-Default` (default interactive positive);
- `AuthMech-LAN-WebUI-User-Defined` (custom interactive positive);
- `AuthMech-M2M-API-Bearer-RO` (RO reads positive);
- `AuthMech-M2M-API-Bearer-RW` (RW route positive);
- temporary-state setup;
- final restoration and non-mutation verification.

Overall PASS requires every positive-authorization result to pass and restoration to be verified.

| Exit code | Meaning                                                                                       |
| :-------- | :-------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                  |
| `1`       | A required authorization was denied when it should have succeeded, or the DUT was mutated     |
| `2`       | Invalid setup, connectivity/protocol error, invalid prepared state, or unverified restoration |

Never convert an exception into a PASS-shaped default. A denied valid credential is FAIL; a broken
setup is ERROR.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py`. Fake-gateway challenges, cookies, hashes, and wire expectations
must be independent fixed/reference fixtures and must not call client code under test. Cover at
least:

- runner integration with invalid-setup errors and evidence/progress (direct config parsing and log
  collision tests belong only in `test_lib.py`);
- the complete PASS path with fakes: default `200`, custom-credential provisioning body exact,
  custom interactive `200`, RO reads `200`, RW `POST /ruuvi.json` with `{}` `200`, and unchanged
  hash;
- realm captured from the live challenge (not from `.env`) for the custom credential;
- a denied valid credential (e.g., RO read returns `401`, or RW no-op POST returns non-`200`)
  produces FAIL;
- restoration body exactness; restoration attempted after PASS, FAIL, ERROR, and raised exceptions;
- restoration failure or final-state mismatch prevents PASS;
- the RW authorization POST body is exactly `{}` and causes no configuration-hash change;
- timeout, connection failure, malformed JSON, and missing baseline fields produce ERROR;
- PASS/FAIL/ERROR exit-code aggregation.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- The default and custom interactive logins both succeed; RO and RW bearer scopes both grant access
  on their respective safe endpoints.
- No positive-authorization check changes configuration; RW proof uses `POST /ruuvi.json` with `{}`
  and verifies the configuration hash.
- Temporary credentials are provisioned securely, recorded in evidence, and removed and verified in
  a mandatory `finally` path; the baseline is restored.
- A compliant DUT produces PASS; a wrongly denied valid credential produces FAIL;
  setup/infrastructure failures produce ERROR, never PASS.
- The implementation and host-side tests run on Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), lines 79-102.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-160 and 321-409.
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md), lines 82-114 and 163-200.
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md), lines 73-131.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), lines 9-40.
- [`docs/http_api.md`](../../../docs/http_api.md), especially lines 131-152, 190-195, 208-233,
  363-375, and 556-583.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, lines 135-173 and 519-583.
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 257-296 and 347-358.
- `components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c`, lines 248 onward.
- `main/gw_cfg_json_parse_lan_auth.c`, lines 35-111.
- `main/http_server_cb_on_post.c`, lines 44-96.
- `ruuvi.gwui.html/src/page_lan_auth.mjs`, lines 146-196.
- `cra/303645/tests/test_5_1_1_2_b.py`, `cra/303645/tests/test_5_1_5_2_b.py`, and their host-side
  tests.
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`.

## Runner naming

Use `FunctionalTest_5_5_4_2_b` and `execute_test_5_5_4_2_b`; retain `main() -> int` as the
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
