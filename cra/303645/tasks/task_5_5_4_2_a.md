# Task: Automate ETSI Test Case 5.5-4-2, Unit a

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
a: Deny Unauthenticated, Invalid, and Insufficient-Privilege Access**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface (port 80, plain HTTP)
and functionally verify that the two network-exposed services declared in
[`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md) (`SoftServ-Local-Management-WebUI`, lines 73-101,
and `SoftServ-Local-Programmatic-API`, lines 103-131) deny access to:

1. **unauthenticated** subjects;
2. subjects presenting **invalid** credentials;
3. subjects with **insufficient privilege** (a read-only bearer key used on a read/write operation).

It must exercise the source-required interfaces and distinguish the three documented denial statuses
`302`, `401`, and `403` produced by the firmware, mapping each probe to the exact status the
firmware returns.

This is a live-DUT test. Mock-based tests of the Python implementation are useful for development
but do not replace the physical-gateway run or constitute compliance evidence. The physical gateway
is authoritative for the verdict; mock tests validate the implementation only.

## Deliverables

1. Create `cra/303645/tests/test_5_5_4_2_a.py`, compatible with Python 3.8.
2. Create `cra/303645/tests/test_test_5_5_4_2_a.py` containing deterministic host-side
   implementation tests using `unittest`. They must mock the HTTP boundary and be clearly described
   as implementation tests, not ETSI functional-test evidence.
3. Extend `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is
   required. Do not duplicate an existing helper in the new test.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. Prefer the
   standard library and the existing `requests`/`pycryptodome` dependencies.

Do not modify firmware behavior, source assessments, or unrelated tasks/tests. Make the related
shared-library, direct-test, fixture, and guidance updates required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), test
case 5.5-4-2 (lines 79-102), specifically the **Unit a** row of the functional matrix (line 92) and
the assessment justification (lines 96-102).

Unit a declares these DUT actions and expected behaviors:

| Probe | Action on DUT                                                   | Self-assessment expected behavior                                    |
| :---- | :-------------------------------------------------------------- | :------------------------------------------------------------------- |
| 1     | Unauthenticated `GET /ruuvi.json`                               | `302 Found`, `Location: http://<hostname>.local/#auth`               |
| 2     | Unauthenticated `GET /auth`                                     | `401 Unauthorized`                                                   |
| 3     | Invalid login payload `POST /auth`                              | Password verification fails; `401 Unauthorized` after ~1 s delay     |
| 4     | `POST /ruuvi.json` using a Read-Only token (`lan_auth_api_key`) | Configuration update rejected (self-assessment says `403 Forbidden`) |

### Mandatory firmware-vs-assessment reconciliation (read carefully)

The self-assessment prose for probe 4 states **HTTP 403 Forbidden**, but the firmware actually
returns **HTTP 401** for a _prohibited bearer token_ (a read-only key on a read/write-scoped route).
See
[`http_server_handle_req_auth_bearer()`](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257),
lines 257-270 (returns `http_server_resp_401_json` when not allowed) and
[`http_server_handle_req_get_or_check_auth()`](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L272),
lines 285-296 (`HTTP_SERVER_AUTH_API_KEY_PROHIBITED` -> `flag_allowed = false`), corroborated by
[`docs/http_api.md`](../../../docs/http_api.md) lines 150-152.

The test **must**:

- treat the physical DUT and firmware source as authoritative and expect **`401`** for the
  read-only-key-on-write probe;
- record in the evidence log that the self-assessment's `403` claim is a documented discrepancy and
  that the observed firmware status is `401`;
- **not** FAIL the DUT merely because it returns `401` instead of the `403` written in the prose.

`403` is genuinely produced elsewhere in the firmware — for the `DENY` auth type and for
LAN-forbidden routes (`POST /connect.json`, `POST /connect_wps`, `DELETE /connect.json`), which
return `403` from the LAN even for an otherwise-authorized caller (see
[`docs/http_api.md`](../../../docs/http_api.md) lines 384-395 and 569-570). Unit a demonstrates the
`403` distinction through one such LAN-forbidden route, as described in the procedure.

Successful positive access (Unit b) and the full configuration-change commit path (test case 5.5-5-2
A) are out of scope here. This unit tests only the denial boundary.

## Relevant IXIT declarations

- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md): `AuthMech-LAN-WebUI-Default` (lines 57-115),
  `AuthMech-M2M-API-Bearer-RO` (lines 321-365), `AuthMech-M2M-API-Bearer-RW` (lines 369-409).
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md): `SecComMech-WebUI-Session` (lines 82-114) and
  `SecComMech-LAN-Bearer-Authentication` (lines 163-200), which declares that an omitted or invalid
  token is rejected with `401` before execution reaches internal subsystems (lines 198-200).
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md): the two in-scope network services (lines 73-131).
- [`ixit_18-AccCtrl.md`](../ixit_18-AccCtrl.md): hardware/partition access-control declarations;
  informational only — it concerns silicon/flash isolation, not the network authentication boundary
  under test, and must not be used as network-auth evidence.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md): `LogIntf-HTTP-Server` (lines 9-40) confirms the
  port-80 HTTP server hosts `/auth`, `/ruuvi.json`, and `/history` and that unauthenticated access
  to restricted endpoints is blocked with a `302` redirect to `/#auth` and `/auth` returns `401`.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for validated setup, full HTTP
  evidence, result/progress models, and ERROR classification. No local substitutes are permitted.
- Use `GatewayClient` for sessions, logged requests, checked JSON, complete authentication, and the
  staged challenge/login flow required to inspect a rejected login. Use its Basic/Digest
  authorization builders, HA1 helper, and challenge/cookie parsers; do not hand-build requests or
  auth values.
- Use the applicable `GatewayApi`, `AuthMech`, `GatewayCfgDesc`, and `GatewayCfgLanAuthType`
  constants. Use `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`, and select probe
  routes from `API_INVENTORY`; do not reproduce the API inventory.
- Stable probe/result schemas must be typed immutable models. Dictionaries remain only at genuine
  protocol mappings and use descriptor constants for configuration keys.

The shared modules are the primary implementation references; firmware sources are behavioral
references only.

Preserve compatibility with `test_5_1_1_2_b.py`, `test_5_1_5_2_b.py`, and their host-side tests. If
a shared API must change, keep its existing behavior and existing tests passing.

Follow the existing test pattern:

- define a `FunctionalTest_5_5_4_2_a` class with dependency injection for sessions, random
  generation, and any clocks;
- expose `execute_test_5_5_4_2_a(...) -> RunResult` for host-side tests and `main() -> int`;
- call `sys.exit(main())`;
- report progress as `[Step N out of M] <description>`;
- print the log path once as `Open log file: <path>`;
- print the final line as `Overall verdict: <PASS|FAIL|ERROR>`.

## Execution and configuration

The test takes no command-line arguments. Run the commands below from `cra/303645/tests` after the
initial `cd` from the repository root.

[`test_5_5_4_2_a.py`](../tests/test_5_5_4_2_a.py) implements the Unit a procedure. On a dedicated
gateway with factory-default authentication, inactive configuration hotspot, and disabled API keys,
run:

```bash
cd cra/303645/tests
.venv/bin/python test_5_5_4_2_a.py
```

The runner completes unauthenticated and invalid-credential reads before configuration POST probes,
then temporarily provisions RO/RW keys for privilege checks. RO-on-write must return `401`,
correcting the self-assessment's documented `403` discrepancy. A verified RW key on LAN
`POST /connect_wps` must return `403`: the source checks authentication before the LAN restriction.

Two stable initial configuration reads and complete configuration comparisons expose partial-parser
side effects. Only the two API-key fields are written; no unrelated settings are added or excluded
from comparisons. Cleanup re-authenticates, clears both keys (retrying once if needed), verifies the
full baseline, and checks both revoked keys. Failed or unverifiable restoration returns ERROR with
operator recovery advice while retaining any observed security FAIL in its own result.

The paired [`test_test_5_5_4_2_a.py`](../tests/test_test_5_5_4_2_a.py) suite uses the real client
and offline HTTP fixtures. Run it with
`.venv/bin/python -m unittest --verbose test_test_5_5_4_2_a.py`. These implementation tests do not
provide a live-DUT verdict. Unit a does not measure login timing.

Load `.env` from the current working directory using `lib.config.load_dut_config`. `.env` format:

```dotenv
gw_id=XX:XX:XX:XX:XX:XX:XX:XX
gw_mac=XX:XX:XX:XX:9C:2C
gw_hostname=ruuvigateway9c2c.local
```

Semantics:

- `gw_id` is the gateway's default `Admin` password (fixed administrative username `Admin`).
- `gw_mac` is the expected six-octet nRF52 identity MAC; verify it against the authenticated
  configuration response when that field is present, and treat a mismatch as invalid setup.
- `gw_hostname` is the LAN hostname/IP without a scheme; the base URL is `http://<gw_hostname>`.
- A missing key, malformed value, unknown key, or duplicate key is an invalid-setup ERROR (exit 2).

The `.env`, `logs/`, and `.venv/` paths are already git-ignored; keep them ignored.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_5_4_2_a_<UTC-date-time>.log
```

Use `EvidenceLog` exactly as the existing tests do (filesystem-safe UTC stamp, exclusive creation).
The log must contain:

- test case and unit; UTC start/end timestamps and duration;
- parsed DUT identity (`gw_hostname`, `gw_mac`, `gw_id`);
- each request method/URL, request headers and body, and response status, headers, cookies, and
  body;
- any interactive-auth calculations and ECDH material generated;
- the temporary RO/RW keys, if provisioned;
- each assertion, the per-probe result, the per-status (`302`/`401`/`403`) classification, and the
  overall verdict;
- the explicit note that the self-assessment `403` claim for the read-only-on-write probe was
  observed as firmware `401`;
- exception type and traceback on error.

Secrets, cookies, Authorization headers, hashes, ECDH material, and full response bodies **may** be
written to this local evidence log. Do not print those to the terminal. Keep terminal output
concise.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable from the test host over its LAN interface on HTTP port 80;
- in `lan_auth_default` mode (default `Admin`/`gw_id` credential derived from `DEVICEID`);
- with both M2M API keys disabled/empty;
- with the configuration SoftAP inactive, so LAN requests are not blocked;
- not concurrently modified by another process;
- with no firmware update or remote-configuration operation in progress;
- with a backup available in case automatic restoration fails.

Do not reset or reconfigure the DUT to manufacture the baseline. A failed precondition is ERROR, not
PASS/FAIL/SKIP.

**Dangerous-route avoidance.** Correct firmware rejects each negative probe before its endpoint
handler runs. Nonetheless, an authentication regression could let a nominally rejected request reach
a destructive handler. Therefore:

- use empty or deliberately invalid bodies where authentication/authorization is evaluated first;
- for the read-only-on-write probe, target `POST /ruuvi.json` with exactly `{}` and verify the
  complete configuration remains unchanged; even an empty body can expose parser side effects;
- never probe firmware-update, storage-init, reset, or certificate routes with a body that could
  take effect;
- stop immediately after any unexpected authorization success (`2xx`);
- provision temporary RO/RW keys **only** for the insufficient-privilege probes, and remove them in
  a mandatory `finally` restoration path.

## Firmware behavior reference

The authoritative HTTP API reference is [`docs/http_api.md`](../../../docs/http_api.md). Key facts:

- Under `RUUVI`/`DEFAULT`, an unauthenticated LAN `GET` of a `.json`/extensionless resource is
  redirected with `302` to the auth entry point; `/status.json` returns `302` but omits the
  previous-URL cookie
  ([`http_server_handle_req.c` L135-173, L153](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L135),
  docs lines 105-106, 221-233, 591-594).
- `GET /auth` unauthenticated returns `401` with a fresh session id and `x-ruuvi-interactive`
  challenge
  ([`http_server_handle_req_get_auth.c` L347](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L347),
  docs lines 208-215).
- `POST /auth` with a wrong login/password returns `401` after an approximately one-second
  server-side delay
  ([`http_server_handle_req_post_auth.c` L248](../../../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c#L248),
  [`http_server_handle_req.c` L608-676](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L608),
  docs lines 363-375).
- A prohibited bearer token (RO key where RW is required) returns `401`
  ([`http_server_handle_req_get_auth.c` L257-296](../../../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257),
  docs lines 131-152).
- All `POST` and `DELETE` requests require an RW bearer key for API-key auth; most `GET`s accept an
  RO key (docs lines 141-148, 190-195, 556-583).
- LAN-forbidden routes `POST /connect.json`, `POST /connect_wps`, and `DELETE /connect.json` return
  `403` from the LAN (docs lines 384-395, 569-570, 581).

## Required procedure

### 1. Initialize and validate the baseline

1. Create the evidence log before reading `.env`.
2. Load `.env`; construct `GatewayClient` with a finite `(connect, read)` timeout and the test
   user-agent, matching the existing tests.
3. Authenticate as `Admin` with `gw_id` via `authenticate_interactive`; require the login
   `POST /auth` to return `200`.
4. Fetch `/ruuvi.json` with the authorized session and require:
   - `lan_auth_type == "lan_auth_default"`;
   - `lan_auth_api_key_use is False`;
   - `lan_auth_api_key_rw_use is False`;
   - normalized `gw_mac` matches `.env` when the field is present.
5. Snapshot the required default fields and a canonical sorted-JSON SHA-256 hash of the full
   configuration for the final non-mutation check.

Any missing/mistyped field, failed default authentication, or incorrect baseline is ERROR.

### 2. Unauthenticated-access probes (source-required interfaces)

Use fresh sessions **without** the authorized baseline cookie and with redirects disabled:

1. `GET /ruuvi.json` -> require `302`; `Location` must point at the auth entry (`/#auth`). Classify
   as `302`. (SoftServ-Local-Programmatic-API / SoftServ-Local-Management-WebUI.)
2. `GET /history` -> require `302` (corroborating read endpoint).
3. `GET /status.json` -> require `302` (previous-URL cookie omitted).
4. `GET /info.json` -> require `302`; LAN `404` requires authorization and must fail this denial check.
5. `GET /auth` -> require `401`; the `WWW-Authenticate` header must begin with `x-ruuvi-interactive`
   and the JSON must report `lan_auth_default`. Classify as `401`.
6. `DELETE /auth` without an authorized session -> require `401` (fresh-session auth-only probe).
7. Defer the missing-credential `POST /ruuvi.json` with exactly `{}` until all baseline-state
   invalid-credential reads in the next phase have finished; require `401`.

### 3. Invalid-credential probes

1. Use the shared staged interactive flow to submit a random non-default username/password, then
   separately submit `Admin` with a password guaranteed to differ from `gw_id`. Obtain a fresh
   challenge for each attempt and require each login response to be `401`. Retain the original
   submitted `RUUVISESSION` cookie and any distinct replacement returned by the failed login;
   separately send each cookie on a follow-up `GET /auth` and require `401` with default interactive
   authentication. Neither cookie may authorize access. Keep both attempts within the existing
   invalid-interactive-credentials progress phase. Classify as `401`.
2. Complete invalid Basic and Digest GETs on `/ruuvi.json`, `/status.json`, `/history`, and `/info.json` using
   the shared authorization builders before any configuration POST probe. Require the route-specific
   `302`/`401` denial expectations; `GET /auth` must not advertise Basic or Digest.
3. With API keys still disabled, send a fresh rejected bearer token on `GET /info.json` and
   require `401`. A `404` is not authentication denial. Complete this separate safer progress phase
   before any configuration POST probe; the runner has thirteen progress steps.
4. Then send the deferred missing-credential POST and invalid Basic/Digest `POST /ruuvi.json`
   probes, each with exactly `{}` and expected `401`. Verify the full configuration hash afterward.
   Keep the cross-scheme read-before-write sequence explicit in fake-transport assertions.

Any `2xx` from a negative probe is FAIL and stops the test immediately.

If recording the failed-login delay, use the shared staged authentication API to time wire GET and
POST operations separately, excluding local ECDH/hash preparation and parsing. Preserve every raw
sample; a minimum-delay assertion uses the raw minimum without discarding samples. Transport retries
are not completed attempts and retry sleeps must not inflate authentication timings. Follow the
assessment's timing rules rather than introducing a second timing policy.

### 4. Insufficient-privilege probes (temporary RO/RW keys)

Provision temporary keys only because this probe requires an active read-only key:

1. Generate distinct temporary RO and RW keys through the shared client's injected randomness.
2. Using the still-authorized default session, send a partial `POST /ruuvi.json`:

   ```json
   {"lan_auth_api_key": "<temporary-RO-key>", "lan_auth_api_key_rw": "<temporary-RW-key>"}
   ```

   Verify that only the intended key flags changed and `lan_auth_type` stayed `lan_auth_default`.
   Partial parsers can rewrite omitted settings even though the handler starts from a copy
   ([`http_server_cb_on_post.c` L44-58](../../../main/http_server_cb_on_post.c#L44)).

3. Confirm the prepared state: authenticated `/ruuvi.json` now reports
   `lan_auth_api_key_use is True` and `lan_auth_api_key_rw_use is True`. A failure here is ERROR /
   invalid prepared state, followed by mandatory restoration.
4. Verify positive RO/RW `GET /history` controls before the write probes; these prove the temporary
   keys were provisioned successfully and are setup controls, not Unit b's authorization matrix.
5. **RO-on-write probe (primary insufficient-privilege case):** send `POST /ruuvi.json` with
   `Authorization: Bearer <temporary-RO-key>` and exactly `{}` as the body. Require the firmware
   status to be **`401`** (prohibited bearer). Record the self-assessment `403` discrepancy. Because
   the auth layer short-circuits before the handler, verify (via a fresh authenticated read) that
   the configuration is unchanged, proving the destructive handler was not reached. Classify as
   `401`.
6. **Genuine `403` distinction (LAN-forbidden route):** send `POST /connect_wps` with
   `Authorization: Bearer <temporary-RW-key>` and no body -> require `403` (LAN-forbidden). This
   demonstrates the `403` status without side effects, because the LAN-forbidden check returns `403`
   before any WPS action runs
   ([`http_server_handle_req.c` L563-569](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L563)).
   Authentication is evaluated first: `http_server_handle_req_post()` requires RW authorization
   and returns an authentication failure before dispatching to the LAN-forbidden check. After the
   positive RW control above has verified the key, this probe must reach the LAN restriction and
   return `403`. Classify as `403`; `401` or any other status is a failed probe, not an
   informational substitute. Record the expected and observed statuses in the evidence log and
   still perform mandatory restoration.

Test read-only/less-dangerous probes before the RW/LAN-forbidden probe. Stop immediately on any
unexpected `2xx`.

### 5. Restore the DUT in a `finally` path

Establish the mandatory final-restoration obligation before sending any configuration POST,
including denied probes with `{}`: an authorization bypass or lost reply may hide parser side effects before temporary keys
exist. Mark that obligation before transmission. Clear both key fields even after an early abort,
then compare the complete retained baseline. Check revocation only for nonempty saved key
candidates; no temporary-key check is claimed when provisioning never ran. Changes outside the
two key fields are not repaired by sending the baseline back: preserve their evidence and return
ERROR with operator recovery advice when restoration cannot be verified.

Primary restoration: authenticate as `Admin`/`gw_id`, then `POST /ruuvi.json`:

```json
{"lan_auth_api_key": "", "lan_auth_api_key_rw": ""}
```

Empty strings disable both bearer mechanisms. If primary restoration is denied or its response is
lost, retry after re-authenticating as `Admin`/`gw_id`. Log every attempt; never silently suppress
cleanup errors.

Verify restoration by authenticating as `Admin`/`gw_id`, fetching `/ruuvi.json`, and requiring:

- `lan_auth_type == "lan_auth_default"`;
- both API-key-use flags `False`;
- the canonical configuration hash matches the initial baseline;
- the temporary RO and RW keys no longer authorize access.

The test cannot PASS unless restoration is verified. If security assertions passed but restoration
failed, return ERROR with a concise operator warning pointing to the evidence log.

### 6. Non-mutation proof

The final authenticated `/ruuvi.json` comparison (step 5) is the non-mutation proof: the required
authentication fields must be unchanged and the canonical full-configuration hash must match the
baseline, excluding only fields proven volatile. Any persistent difference caused by the test is
FAIL and must be listed in the evidence log.

## Verdict and exit codes

Emit separate results for:

- unauthenticated denial (`302`/`401` as documented);
- invalid-credential denial (`401`);
- insufficient-privilege denial (RO-on-write `401`);
- `403` distinction (LAN-forbidden route);
- final restoration and non-mutation verification.

Overall PASS requires every result to pass and restoration to be verified.

| Exit code | Meaning                                                                                       |
| :-------- | :-------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                  |
| `1`       | At least one security assertion FAIL (unexpected `2xx`, wrong denial status, or DUT mutation) |
| `2`       | Invalid setup, connectivity/protocol error, incomplete matrix, or unverified restoration      |

Never convert an exception into a PASS-shaped default.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py`: `unittest`, fake sessions/responses, temporary directories, and
dependency injection. Fake gateways must use independent fixed/reference hash, challenge, cookie,
and wire fixtures; they must not call the `GatewayClient` implementation under test to validate
requests. Cover at least:

- runner setup/evidence integration and invalid-setup ERROR; direct parsing and log-collision tests
  belong only in `test_lib.py`;
- the complete PASS response sequence with reduced fakes;
- per-probe expected statuses for unauthenticated (`302`/`401`), invalid Basic/Digest, invalid
  interactive login, RO-on-write (`401`), and the LAN-forbidden `403` case;
- the firmware `401` expectation for the RO-on-write probe and the recorded `403` discrepancy note;
- unexpected `2xx` produces FAIL and aborts immediately;
- redirects remain disabled on negative probes;
- temporary-key provisioning body is exact and restoration is attempted after PASS, FAIL, ERROR, and
  raised exceptions;
- restoration failure or final-state mismatch prevents PASS;
- timeout, connection failure, malformed JSON, and missing baseline fields produce ERROR;
- PASS/FAIL/ERROR exit-code aggregation.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- The live test authenticates with `Admin`/`gw_id` and verifies the default baseline.
- Every unauthenticated, invalid, and insufficient-privilege probe receives the documented denial
  status, with `302`/`401`/`403` explicitly distinguished and the RO-on-write firmware `401`
  expected.
- Temporary RO/RW keys are provisioned only for the insufficient-privilege probes and removed and
  verified in a mandatory `finally` path.
- A compliant DUT produces PASS; any unauthorized success or DUT mutation produces FAIL; setup or
  infrastructure failures produce ERROR, never PASS.
- The implementation and host-side tests run on Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), lines 79-102.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-115 and 321-409.
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md), lines 82-114 and 163-200.
- [`ixit_13-SoftServ.md`](../ixit_13-SoftServ.md), lines 73-131.
- [`ixit_18-AccCtrl.md`](../ixit_18-AccCtrl.md) (informational, hardware isolation).
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), lines 9-40.
- [`docs/http_api.md`](../../../docs/http_api.md), especially lines 105-106, 131-152, 190-199,
  208-233, 363-395, and 556-583.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, lines 135-173, 519-583, and 608-676.
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 49-82, 251-296, and
  347-358.
- `components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c`, lines 248 onward.
- `main/http_server_cb_on_post.c`, lines 44-96.
- `cra/303645/tests/test_5_1_1_2_b.py` and `cra/303645/tests/test_test_5_1_1_2_b.py`.
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`, `http_api.py`.

## Runner naming

Use `FunctionalTest_5_5_4_2_a` and `execute_test_5_5_4_2_a`; retain `main() -> int` as the
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

### Review safeguards

The auth-entry redirect accepts relative `/#auth` or HTTP absolute/scheme-relative URLs only
for the configured DUT host or its identity-derived `RuuviGateway<last-four-nRF52-MAC-hex>`
hostname (with optional `.local`), on the configured HTTP port. This permits IP-addressed
runs to recognize the canonical alias without trusting arbitrary hostnames or other gateways.
User information, other ports, query strings and other destinations fail the assertion.

Progress, restoration-attempt evidence and recovery-error reporting are best effort during
cleanup: a reporting exception cannot skip key removal, its retry, or independent final checks.
Any such reporting failure prevents PASS and produces overall ERROR, retaining earlier FAILs.

During recovery, best-effort evidence handling also covers the HTTP client's request/response
transcripts, authentication calculations and runner assertions. A persistently unwritable log
must not prevent authentication, clearing requests or final state/revocation checks. Authentication,
transport and assertion failures still propagate normally; only reporting failures are contained.
Final evidence flush/close failures return ERROR after cleanup instead of losing the run result.
