# Task: Automate ETSI Test Case 5.5-5-2, Unit A

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

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-5-2, Test Unit
A: Functional Access and Authorization Testing for Configuration Changes**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface (port 80, plain HTTP)
and functionally verify that security-relevant **configuration changes** to the two configurable
network services declared in [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md)
(`SoftServ-Local-Management-WebUI`, `Allows Configuration: Yes`, lines 73-101;
`SoftServ-Local-Programmatic-API`, `Allows Configuration: Yes`, lines 103-131) are:

1. **rejected** when the request is unauthenticated;
2. **rejected** when the caller presents insufficient privilege (a read-only bearer key on a write);
3. **accepted and committed** when the caller presents a valid read/write bearer key.

It must use a harmless, reversible configuration field selected from the firmware, perform exact
pre/post reads to prove which writes did and did not take effect, exercise the RO-vs-RW boundary,
and **mandatorily restore** the original value.

Test case 5.5-5-2 **Unit B** (logical-interface / port scanning) is **out of scope** for this task.

This is a live-DUT test. Mock-based tests validate the Python implementation only; the physical
gateway is authoritative for the verdict.

## Deliverables

1. Create `cra/303645/tests/test_5_5_5_2_a.py`, compatible with Python 3.8.
2. Create `cra/303645/tests/test_test_5_5_5_2_a.py` containing deterministic host-side `unittest`
   implementation tests that mock HTTP and are clearly labelled as implementation tests, not ETSI
   evidence.
3. Extend `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is
   required. Do not duplicate an existing helper.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable.

Do not modify firmware behavior, source assessments, or unrelated tasks/tests. Make the related
shared-library, direct-test, fixture, and guidance updates required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-5.md`](../Self_Assessment_Test_Group_5_5-5.md), test
case 5.5-5-2 (lines 44-76), specifically **Test Unit A: Functional Access and Authorization
Testing** (methodology line 57 and the functional matrix, lines 61-65). It also relates to the
conceptual matrix of 5.5-5-1 (lines 18-40), which states that write access via `POST /ruuvi.json`
requires the RW key (`lan_auth_api_key_rw`) and that read-only tokens are rejected for writes.

Unit A declares these DUT scenarios and expected behaviors:

| Scenario                         | Target           | Action on DUT                                                                      | Self-assessment expected behavior                                                   |
| :------------------------------- | :--------------- | :--------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------- |
| Factory-default-state protection | WebUI            | Unauthenticated `POST /ruuvi.json` or `GET /ruuvi.json` (no default-password auth) | `GET /ruuvi.json` -> `302` to `/#auth`; `GET /auth` -> `401`; config change blocked |
| M2M role-boundary enforcement    | Programmatic API | `POST /ruuvi.json` with a Read-Only key (`lan_auth_api_key`)                       | Mutation rejected (self-assessment says `403`); flash untouched                     |
| Authorized configuration change  | Programmatic API | `POST /ruuvi.json` with a valid Read/Write key (`lan_auth_api_key_rw`)             | Token validated; parameters updated; changes committed to NVS                       |

### Mandatory firmware-vs-assessment reconciliation

The self-assessment states **HTTP 403 Forbidden** for the read-only-key-on-write scenario, but the
firmware returns **HTTP 401** for a prohibited bearer token (RO key on an RW-scoped route). See
[`http_server_handle_req_auth_bearer()`](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257)
(returns `http_server_resp_401_json` when not allowed, line 269) and
[`http_server_handle_req_get_or_check_auth()`](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L272)
(`HTTP_SERVER_AUTH_API_KEY_PROHIBITED` -> `flag_allowed = false`, lines 285-296), corroborated by
[`docs/http_api.md`](../../../docs/http_api.md) lines 150-152.

The test **must** expect **`401`** for the RO-key-on-write scenario, record that the self-assessment
`403` claim is a documented discrepancy (observed status `401`), and **not** FAIL the DUT for
returning `401`. The critical functional evidence is that the write is **rejected** and the
**running configuration is preserved** (verified by exact pre/post reads), not the specific
401-vs-403 code. Flash contents remain unverified by these HTTP reads; the persistence limitation
below still applies.

### Operational-state note

The self-assessment references both factory-default and initialized states. This automated test runs
against the DUT in its prepared `lan_auth_default` state (the default `Admin`/`gw_id` credential is
derived from `DEVICEID`, which the assessment treats as the factory default). The M2M RO/RW boundary
is tested by temporarily provisioning RO/RW keys. The test must **not** reset the device or force a
factory state to manufacture preconditions; it validates the prepared state.

## Relevant IXIT declarations

- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md): `AuthMech-LAN-WebUI-Default` (lines 57-115),
  `AuthMech-M2M-API-Bearer-RW` (lines 369-409), `AuthMech-M2M-API-Bearer-RO` (lines 321-365).
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md): `SecComMech-LAN-Bearer-Authentication` (lines
  163-200), declaring RO reads / RW writes privilege separation and `401` on missing/invalid token;
  `SecComMech-WebUI-Session` (lines 82-114).
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md): the two configurable services (lines 73-131).
- [`ixit_18-AccCtrl.md`](../ixit_18-AccCtrl.md): informational only (hardware/partition isolation).
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md): `LogIntf-HTTP-Server` (lines 9-40), documenting the
  `302` redirect to `/#auth` and `401` at `/auth` for unauthenticated restricted access.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for setup, wire evidence,
  result/progress models, and error classification; no local replacements.
- Use `GatewayClient` for sessions, logged requests, checked JSON, and complete/staged interactive
  authentication, including challenge/cookie handling. Do not manually construct requests or auth.
- Use the applicable `GatewayApi`, `AuthMech`, `GatewayCfgDesc`, and `GatewayCfgLanAuthType`
  constants, plus `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`. Validate selected
  routes against `API_INVENTORY` rather than duplicating route definitions.
- Stable scenario/result schemas must be typed immutable models. Dictionaries remain only at genuine
  protocol mapping boundaries, with `GatewayCfgDesc` keys for configuration payloads.

The shared modules are the primary implementation references; firmware sources define behavioral
expectations only.

Preserve compatibility with the existing tests and their host-side suites.

Follow the existing test pattern: a `FunctionalTest_5_5_5_2_a` class with dependency injection;
`execute_test_5_5_5_2_a(...) -> RunResult`; `main() -> int`; `sys.exit(main())`;
`[Step N out of M] <description>` progress; a single `Open log file: <path>` line; a final
`Overall verdict: <PASS|FAIL|ERROR>` line.

## Execution and configuration

No command-line arguments:

Live execution after implementing this task (not host-side validation):

```text
cd cra/303645/tests
.venv/bin/python test_5_5_5_2_a.py
```

Load `.env` from the current working directory: fixed `Admin` username, `gw_id` default password,
`gw_mac` verified against the authenticated config when present, base URL `http://<gw_hostname>`.
Missing/malformed/unknown/duplicate keys are invalid-setup ERROR (exit 2). `.env`, `logs/`, `.venv/`
remain git-ignored.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_5_5_2_a_<UTC-date-time>.log
```

Use `EvidenceLog`. The log must contain: test case/unit; UTC start/end/duration; DUT identity; the
selected reversible field and its original value; the temporary RO/RW keys; every HTTP request and
response; each pre/post read of the target field; the unauthorized-rejection, RO-rejection, and
RW-commit results; the recorded `403`-vs-`401` discrepancy note; the mandatory restoration attempts
and verification; and exception type/traceback on error. Full secrets and bodies may be written to
this local log only. Keep terminal output concise.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over its LAN interface on HTTP port 80;
- `.env` `gw_hostname` set to the DUT's canonical hostname used by firmware auth redirects.
  This task enforces the configured HTTP origin; an IP address or alternate alias that redirects
  to another hostname does not satisfy its redirect assertion. Configure the canonical hostname
  before running, rather than accepting an arbitrary redirect host;
- in `lan_auth_default` mode with both M2M keys disabled/empty;
- `mqtt_data_format` equals the generated default (`ruuvi_raw`), and `mqtt_prefix` and
  `mqtt_client_id` equal the device-derived defaults (`ruuvi/<nRF52-MAC>/` and `<nRF52-MAC>`,
  with uppercase MAC octets). Require these fields and their string types even if MQTT is
  disabled: partial POSTs reset omitted MQTT fields. Reject custom values before any
  configuration POST; do not overwrite settings to manufacture this precondition
  ([MQTT parser](../../../main/gw_cfg_json_parse_mqtt.c),
  [device defaults](../../../main/gw_cfg_default.c));
- configuration SoftAP inactive; no firmware-update or remote-config operation in progress;
- not concurrently modified; a backup available if restoration fails.

Do not reset/reconfigure to manufacture the baseline. A failed precondition is ERROR.

**Persistence limitation:** `gw_cfg_set()` updates RAM before calling the save callback, and
`settings_save_to_flash()` does not propagate flash-write failures to the HTTP response. HTTP 200,
configuration readback, and rejected bearer tokens therefore verify only running state. This task
forbids reset routes and does not automate a power-cycle. After any configuration POST attempt,
matching runtime cleanup is reported as restoration **ERROR: flash persistence unverified**, with
operator recovery advice; the runner cannot award overall PASS. Keep the dedicated DUT isolated
until an independent power-cycle and baseline/key-revocation verification are complete. Temporary
keys or marker values may return after reboot if cleanup was not saved. Reset/reconfigure the
verified dedicated DUT if necessary. This follow-up is operator recovery, not an automated PASS
or a claim of NVS verification. Earlier mechanism FAIL results remain recorded.

**Harmless, reversible field selection.** Use the free-text `coordinates` field of the Ruuvi
configuration. It is a plain string echoed in `/ruuvi.json`
([`gw_cfg_json_generate.c` L817](../../../main/gw_cfg_json_generate.c#L817)) and parsed from
`POST /ruuvi.json` ([`gw_cfg_json_parse.c` L62-66](../../../main/gw_cfg_json_parse.c#L62)); changing
it is the reversible marker selected for this task. Send only the intended fields and verify the
complete configuration after each transition; parser-specific handling of omitted fields can cause
collateral changes
([`http_server_cb_on_post.c` L44-58](../../../main/http_server_cb_on_post.c#L44)).

**Dangerous-route avoidance.**

- Confine marker mutations to `POST /ruuvi.json` with only `coordinates`; temporary key provisioning
  and cleanup use only their specified authentication fields.
- For unauthorized/RO-rejection probes, still send only a benign `{"coordinates": ...}` body, so
  that even if the auth layer regressed, no dangerous field is altered.
- Never touch firmware-update, storage-init, reset, certificate, or `connect*` routes.
- Stop further probes immediately on any unexpected authorization success (`2xx`), even if
  `coordinates` appears unchanged. Still perform authenticated final checks and restoration.
- Provision temporary RO/RW keys only for the boundary/commit scenarios and remove them in a
  mandatory `finally` restoration path.

## Firmware behavior reference

Authoritative: [`docs/http_api.md`](../../../docs/http_api.md). Key facts:

- Unauthenticated LAN `GET /ruuvi.json` under `RUUVI`/`DEFAULT` -> `302` to the auth entry;
  `GET /auth` unauthenticated -> `401` (docs lines 105-106, 208-215, 240-);
- Unauthenticated `POST /ruuvi.json` -> `401` (POST is not redirected)
  ([`http_server_handle_req.c` L519-583](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L519),
  docs lines 363-410);
- All POST routes require an RW bearer key for API-key auth; an RO key on a POST -> `401`
  (prohibited bearer), short-circuited before the handler
  ([`http_server_handle_req_get_auth.c` L257-296](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257),
  docs lines 141-152, 353-361, 571);
- A valid RW bearer key on `POST /ruuvi.json` returns `200 {}` and the config is updated/committed
  ([`http_server_cb_on_post_ruuvi()` L44](../../../main/http_server_cb_on_post.c#L44), docs lines
  399-410).

## Required procedure

### 1. Initialize and validate the baseline

1. Create the evidence log before reading `.env`.
2. Load `.env`; construct `GatewayClient` with finite timeouts and the test user-agent.
3. Authenticate as `Admin` with `gw_id`; require the login `POST /auth` to return `200`.
4. Fetch `/ruuvi.json`; require `lan_auth_type == "lan_auth_default"`, both API-key-use flags
   `False`, and `gw_mac` match when present.
5. Record the original `coordinates` value and a canonical sorted-JSON SHA-256 hash of the full
   config.

Missing/mistyped fields, failed login, or incorrect baseline is ERROR.

### 2. Unauthorized-rejection scenario (factory-default-state protection)

Using fresh sessions with no authorization and redirects disabled:

1. `GET /ruuvi.json` -> require `302` with `Location` resolving to `/#auth` on the configured DUT HTTP
   origin (same hostname, scheme, and effective port). Relative redirects and explicit port 80
   are valid; external hosts, HTTPS, and other ports fail before configuration writes.
2. `GET /auth` -> require `401` advertising `x-ruuvi-interactive` and reporting `lan_auth_default`.
3. `POST /ruuvi.json` with no Authorization header and a benign body
   `{"coordinates": "<probe-value-U>"}` -> require `401`.
4. Prove the write did not take effect: re-authenticate as `Admin`/`gw_id`, fetch `/ruuvi.json`, and
   confirm `coordinates` still equals the **original** value (exact pre/post read).

### 3. Provision temporary RO/RW keys

Generate distinct temporary RO and RW keys through the shared client's injected randomness. Using
the authorized `Admin` session, send a partial `POST /ruuvi.json`:

```json
{"lan_auth_api_key": "<temporary-RO-key>", "lan_auth_api_key_rw": "<temporary-RW-key>"}
```

`lan_auth_type` stays `lan_auth_default`. Confirm the prepared state: authenticated `/ruuvi.json`
reports both API-key-use flags `True`. A failure here is ERROR / invalid prepared state, followed by
mandatory restoration.

### 4. M2M role-boundary scenario (RO rejected on write)

1. Read `coordinates` (pre-read) with the authorized `Admin` session or the RO key.
2. `POST /ruuvi.json` with `Authorization: Bearer <temporary-RO-key>` and body
   `{"coordinates": "<probe-value-RO>"}` -> require **`401`** (prohibited bearer). Record the
   self-assessment `403` discrepancy.
3. Post-read `coordinates` (fresh authenticated read) -> require it to be **unchanged** from the
   pre-read value, proving the exposed running field is unchanged. Flash contents remain unverified.

Any change to `coordinates` from the RO write is FAIL (authorization bypass) and stops the test.

### 5. Authorized configuration-change scenario (RW commit)

1. Choose a new marker value `"<probe-value-RW>"` distinct from the original and from the probe
   values above.
2. `POST /ruuvi.json` with `Authorization: Bearer <temporary-RW-key>` and body
   `{"coordinates": "<probe-value-RW>"}` -> require `200`.
3. Verify the commit: fetch `/ruuvi.json` (authenticated) and require
   `coordinates == "<probe-value-RW>"` (the running configuration changed; NVS persistence is unverified).
4. Confirm no other configured field changed: compare against the verified state after temporary key
   provisioning, allowing only the intended `coordinates` transition. Keep the original
   pre-provisioning baseline for final restoration; do not misclassify deliberately enabled key
   flags as collateral changes. Any other persistent difference is FAIL.

### 6. Restore the DUT in a `finally` path

Restoration is mandatory once any temporary key or the RW commit may have been applied.

Primary restoration, using `Authorization: Bearer <temporary-RW-key>`:

1. `POST /ruuvi.json` with `{"coordinates": "<original-value>"}` to restore the field;
2. `POST /ruuvi.json` with `{"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}` to disable both
   keys.

Fallbacks, in order, if a primary restoration step is denied or its response is lost: authenticate
as `Admin`/`gw_id` and resend the restoration bodies. Log every attempt; never suppress cleanup
errors.

Verify restoration by authenticating as `Admin`/`gw_id`, fetching `/ruuvi.json`, and requiring:

- `coordinates == <original-value>`;
- `lan_auth_type == "lan_auth_default"` and both API-key-use flags `False`;
- the canonical full-config hash matches the initial baseline;
- the temporary RO and RW keys no longer authorize access.

The test cannot PASS unless persistent restoration is verified. The HTTP checks above verify
running state only. When they succeed after any POST attempt, mark restoration ERROR and report
flash persistence as unverified with the isolation, power-cycle, and independent verification
advice above. This also applies when HTTP replies claim success for cleanup. Preserve observed
configuration differences or usable temporary keys as restoration FAIL; transport/recovery errors
remain ERROR. Always point to the evidence log.

### 7. Non-mutation proof

The final baseline comparison in step 6 proves only running-state non-mutation: every exposed
field, including `coordinates`, matches the initial baseline. It does not prove flash restoration
or persistence of key revocation. Any observed difference is FAIL and must be listed in evidence;
a matching comparison after writes leaves persistent restoration unverified/ERROR.

## Verdict and exit codes

Emit separate results for:

- unauthorized-change rejection (`GET` `302`, `GET /auth` `401`, unauth `POST` `401`, field
  unchanged);
- RO-on-write rejection (`401`, field unchanged);
- authorized RW commit (`200`, field changed to the new value, no collateral change);
- temporary-state setup;
- final restoration and non-mutation verification.

Overall PASS requires every result to pass and persistent restoration to be verified. The current
HTTP-only procedure cannot meet that persistence requirement after writes, so even a successful
runtime campaign returns ERROR with explicit recovery advice.

| Exit code | Meaning                                                                                                                       |
| :-------- | :---------------------------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                                                  |
| `1`       | A security assertion failed (unauthorized/RO write took effect, authorized write did not commit, or the DUT was left mutated) |
| `2`       | Invalid setup, connectivity/protocol error, invalid prepared state, or unverified restoration                                 |

Never convert an exception into a PASS-shaped default.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py`. Fake-gateway challenges, cookies, authorization, and wire responses
must be independent fixtures and must not call client code under test to derive expectations. Cover
at least:

- runner integration with invalid-setup errors and evidence/progress (direct config parsing and log
  collision tests belong only in `test_lib.py`);
- the complete successful runtime campaign with fakes, ending in persistence-unverified ERROR: unauth `GET` `302`, `GET /auth` `401`, unauth `POST` `401`, RO
  `POST` `401`, RW `POST` `200`, and pre/post reads showing only the RW write changed `coordinates`;
- the firmware `401` expectation for the RO-on-write scenario and the recorded `403` discrepancy
  note;
- an unauthorized or RO `2xx` response, even without a marker change, produces FAIL and aborts
  subsequent probes while preserving final checks and restoration;
- an authorized RW write that does not commit (post-read still original) produces FAIL;
- a collateral change to a non-`coordinates` field produces FAIL;
- temporary-key and restoration bodies are exact; restoration attempted after PASS, FAIL, ERROR, and
  raised exceptions;
- restoration failure or final-config mismatch prevents PASS;
- separate fake running/persisted states model a failed cleanup save: running readback and token
  rejection succeed, persisted temporary keys return after a simulated reboot, and the runner
  reports restoration ERROR and operator recovery advice;
- timeout, connection failure, malformed JSON, and missing baseline fields produce ERROR;
- custom MQTT format/prefix/client-ID baselines stop before writes, using fixtures that model
  omitted-field resets on partial POSTs;
- off-origin redirects (external host, changed scheme or port) fail without writes; relative
  and explicit-default-port redirects remain accepted;
- PASS/FAIL/ERROR exit-code aggregation.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- Unauthenticated and read-only configuration writes are rejected and proven not to change the
  target field; the authorized read/write commit succeeds and is verified by an exact post-read.
- The RO-on-write firmware `401` is expected and the self-assessment `403` claim is recorded as a
  discrepancy.
- Only the harmless `coordinates` field and temporary authentication keys are deliberately mutated;
  original running values are restored and checked in a mandatory `finally` path. Persistent
  restoration remains explicitly unverified until separate operator recovery checks.
- A successful HTTP-only campaign records its mechanism results but returns ERROR because flash
  restoration is unverified. An authorization bypass or lost runtime commit remains a per-mechanism
  FAIL, preserved alongside any restoration ERROR. Setup/infrastructure failures remain ERROR.
  Observed final runtime mismatches or usable temporary keys remain restoration FAIL.
- The implementation and host-side tests run on Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_5-5.md`](../Self_Assessment_Test_Group_5_5-5.md), lines 18-40 and
  44-76.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-115 and 321-409.
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md), lines 82-114 and 163-200.
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md), lines 73-131.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), lines 9-40.
- [`docs/http_api.md`](../../../docs/http_api.md), especially lines 105-106, 141-152, 190-199,
  208-215, 353-361, and 399-410.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, lines 519-583.
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 257-296 and 347-358.
- `main/http_server_cb_on_post.c`, lines 44-96.
- `main/gw_cfg_json_parse.c`, lines 62-66; `main/gw_cfg_json_generate.c`, line 817.
- `cra/303645/tests/test_5_1_1_2_b.py`, `cra/303645/tests/test_5_1_5_2_b.py`, and their host-side
  tests.
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`.

## Runner naming

Use `FunctionalTest_5_5_5_2_a` and `execute_test_5_5_5_2_a`; retain `main() -> int` as the
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
