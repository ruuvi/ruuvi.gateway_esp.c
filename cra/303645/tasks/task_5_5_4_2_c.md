# Task: Automate ETSI Test Case 5.5-4-2, Unit c

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
c: Verify Authentication-Protection Conformance**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface (port 80, plain HTTP)
and functionally verify that the Web-UI session-protection mechanism (`SecComMech-WebUI-Session`,
[`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md) lines 82-114) operates exactly as documented in
the IXIT, to the extent the firmware source requires. Specifically it must, from live captured
client-side request/response evidence, confirm:

1. the exact `x-ruuvi-interactive` `WWW-Authenticate` challenge structure;
2. session cookie binding (`RUUVISESSION` cookie ↔ `session_cookie`/`session_id` challenge fields);
3. the P-256 (secp256r1) ECDH public-key exchange over `Ruuvi-Ecdh-Pub-Key`;
4. the MD5/SHA-256 challenge-response construction (`HA1 = MD5(user:realm:password)`,
   `response = SHA256(challenge:HA1)`);
5. the AES-CBC encrypted request envelope for a post-auth configuration mutation
   (`Ruuvi-Ecdh-Encrypted: true` with a `{"encrypted","iv","hash"}` body, AES-256-CBC, SHA-256
   hash), and that the DUT accepts it;
6. the **absence of any plaintext password** in the captured client-side request evidence.

**This test must not claim TLS.** Port 80 is plain HTTP; confidentiality here is provided at the
application layer by ECDH/AES, not by TLS. State this explicitly in the evidence.

This is a live-DUT test. Mock-based tests validate the Python implementation only; the physical
gateway is authoritative for the verdict.

## Deliverables

1. Create `cra/303645/tests/test_5_5_4_2_c.py`, compatible with Python 3.8.
2. Create `cra/303645/tests/test_test_5_5_4_2_c.py` containing deterministic host-side `unittest`
   implementation tests that mock HTTP and are clearly labelled as implementation tests, not ETSI
   evidence.
3. Reuse complete/staged interactive authentication from `lib.gateway`. The shared client currently
   returns the AES key but has no encrypted configuration-envelope builder; add a reusable typed
   API, its direct tests in `test_lib.py`, and `lib/README.md` documentation if still absent.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. AES-CBC and
   SHA-256 are available in the existing `pycryptodome` dependency.

Do not modify firmware behavior, source assessments, or unrelated tasks/tests. Make the related
shared-library, direct-test, fixture, and guidance updates required by the implementation contract.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), test
case 5.5-4-2 (lines 79-102), specifically the **Unit c** row of the functional matrix (line 94) and
the assessment justification (lines 96-102). Unit c requires monitoring the authentication
handshakes and API requests and confirming: `WWW-Authenticate` challenge headers,
`Ruuvi-Ecdh-Pub-Key` exchange, the encrypted JSON password-challenge payload of `POST /auth`,
AES-CBC encrypted configuration payloads, and that passwords are never sent in cleartext.

### Interpretation for an automated client-side test

The self-assessment describes network monitoring of a browser session. This automated test provides
**client-side** evidence: the Python test **is** the client, so its captured requests are
authoritative for what the client transmits (the browser transmits the same values). The test
therefore:

- uses `GatewayClient` as the single request/authentication implementation and asserts on its
  captured client-side bytes recorded in the evidence log;
- verifies the DUT's protocol responses (challenge header, gateway ECDH key, `200` on a valid
  encrypted request), which proves the DUT enforces and interoperates with the documented scheme;
- proves plaintext-password absence by asserting the plaintext admin password never appears in any
  captured request URL, header, or body — only the derived `SHA256(challenge:HA1)` response and, for
  the encrypted mutation, ciphertext.

Verify **only to the extent the firmware source requires**: assert on the fields and transforms the
firmware actually parses/validates (challenge parsing, cookie/session binding, ECDH key
length/format, the AES envelope fields and successful decryption). Do not assert firmware internals
the source does not expose.

## Protocol behavior and mandatory shared architecture

The live evidence must still demonstrate the documented challenge/cookie binding, uncompressed P-256
key exchange, `aes_key = SHA256(ECDH X)`, MD5/SHA-256 login response, and AES-256-CBC/PKCS7 envelope
with ciphertext, IV, and plaintext hash. These are behavioral assertions, not implementation
recipes.

- Use `GatewayClient` for logged HTTP, checked JSON, sessions, complete/staged interactive auth,
  challenge/cookie parsing, HA1/auth-response calculation, and ECDH. The current shared client
  returns the derived AES key but does not yet construct the encrypted configuration envelope; add
  that capability as a reusable, typed `gateway.py` API and use it from this test. Do not place AES
  envelope, cookie, challenge, or request-building code directly in the case runner.
- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors`; do not recreate setup, logging,
  result, or error machinery.
- Use `GatewayApi`, `AuthMech.LAN_WEBUI_DEFAULT`, `GatewayCfgDesc`, and
  `GatewayCfgLanAuthType.DEFAULT`, together with applicable `HttpMethod`, `HttpStatus`,
  `HttpHeader`, and `HttpAuthScheme` constants. Use `API_INVENTORY` only for canonical-route
  validation.
- Stable evidence and authentication schemas must be typed immutable models. Dictionaries remain
  only at genuine JSON/header boundaries and use descriptor keys where applicable.

The shared modules are the primary implementation references. Firmware and Web-UI sources remain
behavioral/reference-vector sources only.

Follow the execution/logging pattern of [`test_5_1_1_2_b.py`](../tests/test_5_1_1_2_b.py): a
`FunctionalTest_5_5_4_2_c` class with dependency injection (sessions, random bytes, ECC generation,
IV generation); `execute_test_5_5_4_2_c(...) -> RunResult`; `main() -> int`; `sys.exit(main())`;
`[Step N out of M]` progress; a single `Open log file: <path>` line; a final
`Overall verdict: <PASS|FAIL|ERROR>` line.

## Execution and configuration

The test takes no command-line arguments. Run the commands below from `cra/303645/tests` after the
initial `cd` from the repository root.

[`test_5_5_4_2_c.py`](../tests/test_5_5_4_2_c.py) implements the Unit c procedure. Use a dedicated
LAN gateway with factory-default Admin authentication, both API keys disabled, inactive
configuration hotspot, no update or remote configuration operation, no concurrent changes, and an
available backup. Run:

```bash
cd cra/303645/tests
.venv/bin/python test_5_5_4_2_c.py
```

The runner captures its own requests over plain HTTP port 80; TLS is not used or claimed. It
verifies the interactive challenge and cookie binding, P-256 exchange, MD5/SHA-256 login body, and
acceptance and effect of an AES-CBC coordinates write. It first requires two identical full
configuration reads. Before writing, it requires the omitted-parser fields `mqtt_data_format`
(`ruuvi_raw` from the checked-in default template), `mqtt_prefix` (`ruuvi/<MAC>/`), and
`mqtt_client_id` (`<MAC>`) to match the firmware defaults. `<MAC>` is the uppercase, verified
nRF52 identity MAC. Missing, wrongly typed, or custom values produce setup ERROR without a POST,
because coordinates-only partial parsing would overwrite them. Cleanup restores the original coordinates, retries with fresh default
authentication or cleartext when needed, and compares the entire final configuration with the
baseline. Unrelated parser side effects remain visible; cleanup never adds unrelated fields to
conceal a mismatch. Unverified restoration is ERROR while earlier protection FAIL outcomes remain
recorded. Recovery reporting is best-effort across progress, terminal output, and the client's
request/response evidence: a reporting failure cannot block cleanup HTTP or full read-back, but
retains overall ERROR for incomplete reporting. Unverified cleanup retains operator recovery advice.

The plaintext-password check includes every captured request, including recovery. Full keys,
credentials, configuration and bodies are confined to the local evidence log. The paired offline
suite uses shared gateway login fixtures and independent Web-UI CryptoJS vectors:

```bash
.venv/bin/python -m unittest --verbose test_test_5_5_4_2_c.py
```

These implementation tests do not supply a live-DUT conformance verdict.

Load `.env` from the current working directory: fixed `Admin` username, `gw_id` default password,
`gw_mac` verified against the authenticated config when present, base URL `http://<gw_hostname>`.
Missing/malformed/unknown/duplicate keys are invalid-setup ERROR (exit 2). `.env`, `logs/`, `.venv/`
remain git-ignored.

## Evidence log

Every invocation, including setup failures, must create:

```text
logs/test_5_5_4_2_c_<UTC-date-time>.log
```

Use `EvidenceLog`. The log must contain: test case/unit; UTC start/end/duration; DUT identity; the
raw `WWW-Authenticate` header and parsed challenge fields; the `RUUVISESSION` cookie and binding
check; the client and gateway ECDH public keys, shared secret, and derived AES key; the
`HA1`/`response` calculation inputs and outputs; the exact `POST /auth` body; the full
encrypted-envelope request (headers + `{"encrypted","iv","hash"}`) and the DUT response; the
plaintext-password-absence check result over every captured request; the safe-write pre/post reads
and restoration; an explicit "TLS not used / not claimed — plain HTTP port 80" statement; per-aspect
and overall verdicts; and exception type/traceback on error. Full secrets, keys, hashes, and bodies
may be written to this local log only. Keep terminal output concise and never print secrets to the
terminal.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over its LAN interface on HTTP port 80;
- in `lan_auth_default` mode with both M2M keys disabled/empty;
- configuration SoftAP inactive; no firmware-update or remote-config operation in progress;
- not concurrently modified; a backup available if restoration fails.

Do not reset/reconfigure to manufacture the baseline. A failed precondition is ERROR.

**Safe endpoint and body.** The encrypted-envelope proof must use a harmless, reversible mutation.
Use `POST /ruuvi.json` with the encrypted body `{"coordinates": "<temporary-marker>"}`, sent over
the **authorized interactive session** (using that session's `aes_key`). `coordinates` is a
free-text `ruuvi_cfg` field echoed in `/ruuvi.json`
([`gw_cfg_json_generate.c` L817](../../../main/gw_cfg_json_generate.c#L817),
[`gw_cfg_json_parse.c` L62-66](../../../main/gw_cfg_json_parse.c#L62)). Send only the target field,
then verify all other fields against the stable baseline; do not assume partial parsing preserves
omitted values ([`http_server_cb_on_post.c` L44-58](../../../main/http_server_cb_on_post.c#L44)).

- Never send an encrypted body to firmware-update, storage-init, reset, certificate, or `connect*`
  routes.
- Capture the original `coordinates` value before mutating and restore it exactly in a mandatory
  `finally` path.
- Because the mutation is only a means to prove AES-CBC decryption, keep the marker value short and
  benign, and prove non-mutation of all other fields via a full-config hash comparison after
  restoration.

To keep the unit boundary clear: this unit's write proves **crypto-envelope decryption**, whereas
test case 5.5-5-2 A owns the RO-vs-RW authorization commit on the same field via cleartext bearer
requests. The two exercise different axes (encrypted interactive session vs cleartext bearer
authorization).

## Required procedure

### 1. Initialize and validate the baseline

1. Create the evidence log before reading `.env`.
2. Load `.env`; construct `GatewayClient` with finite timeouts and the test user-agent.
3. Perform `authenticate_interactive("Admin", gw_id)` and require the login `POST /auth` to return
   `200`. Retain the `InteractiveAuthResult` (challenge, cookie, ECDH keys, `aes_key`, session).
4. Fetch `/ruuvi.json`; require `lan_auth_type == "lan_auth_default"`, both API-key-use flags
   `False`, and `gw_mac` match when present. Snapshot the current `coordinates` value and a
   canonical sorted-JSON SHA-256 hash of the full config.

Missing/mistyped fields, failed login, or incorrect baseline is ERROR. Record "TLS not claimed".

### 2. Verify the challenge and cookie binding

From the captured `GET /auth` response (in the `InteractiveAuthResult`):

- the `WWW-Authenticate` header begins with `x-ruuvi-interactive` and contains quoted `realm`,
  `challenge`, `session_cookie`, and `session_id`;
- a `RUUVISESSION` cookie was set;
- `session_cookie == "RUUVISESSION"` and `session_id == <RUUVISESSION cookie value>` (binding);
- the JSON reports `lan_auth_default`.

(`GatewayClient.parse_interactive_challenge` and `authenticate_interactive` already enforce these;
the test must assert on and log the captured values, not merely rely on the helper.)

### 3. Verify the ECDH exchange and key derivation

- the client `Ruuvi-Ecdh-Pub-Key` is an uncompressed P-256 point (`0x04 || X || Y`, 65 bytes);
- the DUT returned a 65-byte uncompressed P-256 public key in `Ruuvi-Ecdh-Pub-Key`;
- the shared secret is the ECDH X coordinate and `aes_key == SHA256(shared_secret)`.

Log the client key, gateway key, shared secret, and AES key.

### 4. Verify the MD5/SHA-256 challenge-response construction

- use the shared HA1/auth-response calculation and its typed evidence to verify
  `HA1 = MD5("Admin:" + realm + ":" + gw_id)` and `response = SHA256(challenge + ":" + HA1)` without
  duplicating the calculation;
- assert the captured `POST /auth` body equals `{"login": "Admin", "password": <response-hex>}`;
- assert the plaintext password `gw_id` does **not** appear anywhere in the `POST /auth` request
  (URL, headers, or body) — only the derived `response` hex is present.

### 5. Verify the AES-CBC encrypted request envelope (safe mutation)

1. Build the plaintext `{"coordinates": "<temporary-marker>"}`.
2. Use the shared encrypted-request support with the authorized session's negotiated key; capture
   its `{"encrypted","iv","hash"}` wire envelope.
3. Send the encrypted `POST /ruuvi.json` through `GatewayClient`. Require `200` (the DUT decrypted
   and applied it — proving AES-CBC/hash behavior). A `400` means the DUT rejected the envelope and
   is a FAIL (or, if consistently reproducible, an implementation mismatch to resolve against the
   CryptoJS reference).
4. Verify the mutation took effect: fetch `/ruuvi.json` and confirm
   `coordinates == <temporary-marker>`.
5. Assert no plaintext of any password appears in the captured encrypted request; assert the request
   body is the ciphertext envelope, not cleartext JSON.

### 6. Restore the DUT in a `finally` path

Restoration is mandatory once the safe mutation may have been applied. On the authorized session (or
a fresh `Admin`/`gw_id` session if needed), send an encrypted (or, as a fallback, cleartext)
`POST /ruuvi.json` with `{"coordinates": "<original-value>"}`. Verify by fetching `/ruuvi.json` that
`coordinates` equals the original value and that the canonical full-config hash matches the baseline
(non-mutation of every other field). Log every restoration attempt; never suppress cleanup errors.
The test cannot PASS unless restoration is verified; if aspects passed but restoration failed,
return ERROR with a concise operator warning.

## Verdict and exit codes

Emit separate results for:

- challenge structure and cookie/session binding;
- P-256 ECDH exchange and `aes_key = SHA256(shared_secret)`;
- MD5/SHA-256 challenge-response construction;
- AES-CBC encrypted-envelope acceptance and effect;
- plaintext-password absence across all captured requests;
- final restoration and non-mutation verification.

Overall PASS requires every aspect to pass and restoration to be verified.

| Exit code | Meaning                                                                                                    |
| :-------- | :--------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS                                                                                               |
| `1`       | An observed protection assertion failed (e.g., a valid envelope rejected or a plaintext password observed) |
| `2`       | Invalid setup, connectivity/protocol error, or unverified restoration                                      |

Never claim TLS. Never convert an exception into a PASS-shaped default.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py`. Fake gateways must validate hashes, ECDH vectors, cookies, and wire
envelopes against independent fixed/reference vectors; they must never call the client code under
test to generate expected values. Cover at least:

- runner integration with invalid-setup errors and evidence/progress (direct config parsing and log
  collision tests belong only in `test_lib.py`);
- deterministic ECC and IV injection so the challenge parse, cookie binding, ECDH derivation, and
  `HA1`/`response` are reproducible;
- runner submission of the independently expected encrypted wire envelope and its rejection
  handling; direct fixed-key/IV/plaintext AES-CBC/PKCS7 vectors, including
  `hash = base64(SHA256(plaintext))`, belong only in `test_lib.py`;
- malformed challenge fields, mismatched session cookies, or invalid ECDH keys rejected by the
  shared client produce protocol ERROR; an observed rejection of a valid envelope with `400`
  produces FAIL. Preserve the distinction in per-aspect outcomes and final evidence;
- a captured request containing the plaintext password produces FAIL;
- restoration body exactness; restoration attempted after PASS, FAIL, ERROR, and raised exceptions;
- restoration failure or final-config mismatch prevents PASS;
- the evidence explicitly records "TLS not claimed";
- timeout, connection failure, and malformed JSON produce ERROR;
- PASS/FAIL/ERROR exit-code aggregation.

After implementation, run the complete offline discovery command in **Required implementation
validation** below, including the paired suite created by this task.

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log and records "TLS not claimed".
- The challenge structure, cookie/session binding, P-256 ECDH exchange, MD5/SHA-256 construction,
  and AES-CBC envelope are each verified against the firmware and the CryptoJS reference client,
  only to the extent the source requires.
- The DUT accepts the encrypted envelope (`200`) and the safe mutation is verified and then
  restored.
- No plaintext password appears in any captured client-side request.
- Temporary mutation is reversed and the baseline restored and verified in a mandatory `finally`
  path.
- A conforming DUT produces PASS; non-conformance produces FAIL; setup/infrastructure failures or
  unverified restoration produce ERROR, never PASS.
- Shared `GatewayClient` behavior remains backward-compatible and does not break existing tests.
- The implementation and host-side tests run on Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_5-4.md`](../Self_Assessment_Test_Group_5_5-4.md), lines 79-102.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 57-160.
- [`ixit_11-SecComMech.md`](../ixit_11-SecComMech.md), `SecComMech-WebUI-Session`, lines 82-114.
- [`ixit_28-LogIntf.md`](../ixit_28-LogIntf.md), lines 9-40.
- [`docs/http_api.md`](../../../docs/http_api.md), especially lines 154-186, 208-215, and 363-410.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, lines 331-355 and 575-676.
- `components/esp32-wifi-manager/src/http_server_ecdh.c`, lines 23-51 and the decrypt path.
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 347-358.
- `components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c`, lines 50 and 248 onward.
- `main/http_server_cb_on_post.c`, lines 44-58.
- `main/gw_cfg_json_parse.c`, lines 62-66; `main/gw_cfg_json_generate.c`, line 817.
- `ruuvi.gwui.html/src/auth.mjs`, lines 31-36, 151-152, 244-245, and 270-285.
- `ruuvi.gwui.html/src/crypto.mjs` (AES/SHA256/MD5/ECDH reference).
- `cra/303645/tests/test_5_1_1_2_b.py` and its host-side tests.
- `cra/303645/tests/lib/gateway.py` (`authenticate_interactive`, `InteractiveAuthResult`),
  `config.py`, `evidence.py`, `models.py`.

## Runner naming

Use `FunctionalTest_5_5_4_2_c` and `execute_test_5_5_4_2_c`; retain `main() -> int` as the
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
