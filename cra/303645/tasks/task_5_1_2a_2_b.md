# Task: Automate ETSI Test Case 5.1-2A-2, Unit B

## Mandatory factory-default precondition

Before test-specific probes or configuration changes, authenticate and fetch `GET /ruuvi.json`, then
use the shared `lib.config` helper to compare all fields relevant to this test directly with
`gw_cfg_default/gw_cfg_default_gen_ui.json`. Authentication tests must compare `lan_auth_type`,
`lan_auth_user`, `lan_auth_api_key_use`, and `lan_auth_api_key_rw_use`. The generated file already
matches the public UI/API representation; do not derive it from the raw saved configuration or handle
hidden passwords or keys. Any missing, wrongly typed, or non-default value is setup ERROR: print the
shared CONFIGURE-button factory-reset instruction and stop without changing the DUT to manufacture a
baseline.

## Review regression requirements

Follow the [CRA test rules](../tests/AGENTS.md), especially functional-test rules and host-side
implementation tests. These requirements qualify the factory-default precondition above:

- Catch Basic/Digest `GatewayAuthenticationModeError` and mark failed default login as setup ERROR
  with the shared recovery message in evidence and terminal output. Once configuration is available,
  validate its MAC before default fields; wrong, malformed, or absent identity must not trigger reset
  advice for a non-default baseline. Do not manufacture defaults by changing the DUT.
  A successful interactive login must reach the authenticated configuration read even if `/auth`
  reports a non-default mode. Reject that mode as setup ERROR only after checking identity;
  recommend reset only for a matching MAC, never when the read fails or identity is unavailable.
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
  Provisioning, read-back, and required login controls must all classify setup exceptions as ERROR.
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
5.1-2A-2, Test Unit B: Password Rejection for M2M**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface and prove that the
machine-to-machine (M2M) REST endpoints — which are protected by high-entropy bearer keys — **reject
every password-based authentication scheme** and **accept only the configured bearer keys**. It must
positively demonstrate that a configured read-only (RO) key and read/write (RW) key each authorize
exactly the scope they are meant to, and nothing more.

This is a live DUT test. Mock-based tests of the Python implementation are useful for development but
do not replace the physical-gateway run or constitute compliance evidence.

## Deliverables

1. Create `cra/303645/tests/test_5_1_2a_2_b.py`, compatible with Python 3.8.
   - The filename normalizes the test-case id `5.1-2A-2` and unit `B` to lower case with dots and
     hyphens replaced by underscores: `test_5_1_2a_2_b.py`.
2. Create `cra/303645/tests/test_test_5_1_2a_2_b.py` with deterministic host-side implementation
   tests. These mock-based tests are **not** ETSI compliance evidence.
3. Reuse the shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md). Extend `lib/` only where a
   generally reusable, backward-compatible helper is required; do not duplicate an existing helper.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. Prefer the
   standard library and the existing `requests` / `pycryptodome` dependencies.

Do not modify firmware behavior, the self-assessment source documents, the other task files, or the
shared assessment documents. Changes to existing `tests/lib/*.py`, `tests/test_lib.py`, affected
callers, and test guidance are permitted only for the shared helpers and narrowly scoped
compatibility, explicit-typing, lint, or review-regression fixes needed by this automation.
Preserve existing authentication, evidence, and recovery contracts unless a reviewed bug requires
a documented correction. Cover shared behavior in `test_lib.py` and run all host-side automation
tests to verify caller compatibility; unrelated existing-test rewrites remain out of scope.

## Normative source and interpretation

The source is
[`Self_Assessment_Test_Group_5_1-2A.md`](../Self_Assessment_Test_Group_5_1-2A.md), **lines 42-58**
(Test case 5.1-2A-2, Test Unit B). Its per-mechanism claims are:

| Documented IXIT Entry ID                    | Targeted Interface | Functional activity                                                 | Claimed result                     |
|:--------------------------------------------|:-------------------|:--------------------------------------------------------------------|:-----------------------------------|
| `AuthMech-LAN-WebUI-*` (Default..Disabled)  | Administrative Web-UI | N/A — validated as interactive user-to-machine interface          | N/A                                |
| `AuthMech-M2M-API-Bearer-RO`                | Local API `/history` | Inject `Authorization: Basic` credentials and password JSON params | Rejected; server returns `401`     |
| `AuthMech-M2M-API-Bearer-RW`                | Config node `/ruuvi.json` | Inject `Authorization: Basic` credentials and password JSON params | Rejected; server returns `401`     |

Relevant IXIT declarations are in [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md):

- `AuthMech-M2M-API-Bearer-RO`: lines 321-365 (declared token `lan_auth_api_key`, cleartext
  `Authorization: Bearer`, fast string comparison against the stored key, high-entropy token defense).
- `AuthMech-M2M-API-Bearer-RW`: lines 369-409 (declared token `lan_auth_api_key_rw`, independently
  configurable and isolated from the RO key).
- Interactive password schemes that must **not** unlock M2M access: `AuthMech-LAN-WebUI-Default`
  (57-115), `AuthMech-LAN-WebUI-User-Defined` (119-160), `AuthMech-LAN-WebUI-Basic` (164-201),
  `AuthMech-LAN-WebUI-Digest` (205-240).

### Scope boundary (do not conflate adjacent units)

- **In scope:** the two token-protected M2M mechanisms (`AuthMech-M2M-API-Bearer-RO` and
  `AuthMech-M2M-API-Bearer-RW`) and the proof that password schemes cannot reach them.
- **Out of scope, state explicitly in the report:**
  - the interactive Web-UI mechanisms themselves as *access-control* targets — Unit B treats them
    only as the *password schemes* being injected against M2M endpoints, never as endpoints to be
    positively authenticated;
  - unconfigured-secret boundary testing (that is test case 5.1-1-2, Unit B);
  - brute-force / rate-limiting behavior (test case 5.1-5-2);
  - the exhaustive port/interface discovery of 5.1-2A-2 **Unit A**.

### Source-vs-implementation conflict (document, do not invent)

The source claims a flat `401 Unauthorized` for injected password schemes on `/history`. The firmware
does **not** always return `401` for GET routes: when a request carries **no recognized bearer token**
and the active `auth_type` is `RUUVI`/`DEFAULT`, an unauthenticated GET of a `.json`/extensionless
resource is answered with a **`302` auth redirect**, not `401`
(`components/esp32-wifi-manager/src/http_server_handle_req.c`, GET handler lines 150-172). A `401` is
returned for GET only when an `Authorization: Bearer` header is present but the token is not a valid
key (`HTTP_SERVER_AUTH_API_KEY_PROHIBITED`). POST routes always return `401` for password schemes.

Therefore the **observable compliance criterion** is:

> No password-based scheme (`Basic`, `Digest`, or an interactive Ruuvi password presented as a token
> or in the request body) ever yields a `2xx` response or returns protected M2M data from an M2M
> endpoint. Only a correct configured bearer key yields `200` from a route within its scope.

Record the exact status code observed for every probe, assert against the firmware-derived matrix
below, and never assert the idealized `401` where the firmware demonstrably returns `302`.

## Firmware-derived route and status matrix

Authoritative reference: [`docs/http_api.md`](../../../docs/http_api.md), sections *Authentication
model* (113-169), *Bearer keys (RO vs RW)* (131-152), *Per-method authorization summary* (188-199),
and the endpoint entries for `/history` (289-299) and `/ruuvi.json` (240-244, 399-412).

Decisive implementation:

- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`:
  - `http_server_handle_req_check_auth_bearer()` lines 49-81 — a bearer token is compared first
    against `auth_api_key_rw` (always accepted when present and matching), then against `auth_api_key`
    (accepted only when `flag_check_rw_access == false`); returns `NOT_USED` when no `Bearer ` header
    is present, `ALLOWED`, or `PROHIBITED`.
  - `http_server_handle_req_get_or_check_auth()` lines ~270-330 — the bearer check runs **before** the
    `auth_type` scheme switch. A `Basic`/`Digest` header therefore yields `NOT_USED` and falls through
    to the interactive scheme, which for `RUUVI`/`DEFAULT` produces the `401`/redirect flow.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`:
  - GET redirect for unauthenticated `RUUVI`/`DEFAULT` (`flag_access_by_bearer_token == false`) at
    lines 150-172.
  - POST handler `http_server_handle_req_post()` lines 507-552 runs `check_auth` with
    `flag_check_rw_access_with_bearer_token = true` **before** any body dispatch, so a failed auth
    returns before the endpoint processes the body.

With temporary RO and RW keys configured while `lan_auth_type` stays `lan_auth_default`, the required
matrix is:

**Negative — password schemes must never gain M2M access:**

| Probe (header / body)                          | GET `/history` (RO) | GET `/ruuvi.json` (RO) | POST `/ruuvi.json` (RW) |
|:-----------------------------------------------|:-------------------:|:----------------------:|:-----------------------:|
| `Authorization: Basic base64(Admin:gw_id)`     | `302`               | `302`                  | `401`                   |
| `Authorization: Digest …` (random, well-formed)| `302`               | `302`                  | `401`                   |
| `Authorization: Bearer <gw_id>` (password as token) | `401`          | `401`                  | `401`                   |
| Password JSON body, no bearer (POST only)      | n/a                 | n/a                    | `401`                   |

For every negative probe the mandatory assertion is `status_code not in {200..299}`; additionally
assert the exact code from the matrix and classify a `2xx` as an immediate FAIL. The password JSON
body for the POST probe must contain keys such as `{"lan_auth_type":"lan_auth_ruuvi",
"lan_auth_user":"Admin","lan_auth_pass":"<md5>","password":"<gw_id>"}` **with no valid bearer header**,
so the request is rejected by `check_auth` before the body is parsed and cannot change the
configuration.

**Positive — configured bearer keys authorize exactly their scope:**

| Probe                              | GET `/history` (RO) | GET `/ruuvi.json` (RO) | POST `/ruuvi.json` with `{}` (RW) |
|:-----------------------------------|:-------------------:|:----------------------:|:---------------------------------:|
| `Authorization: Bearer <RO key>`   | `200`               | `200`                  | `401`                             |
| `Authorization: Bearer <RW key>`   | `200`               | `200`                  | `200`                             |

Use exactly `{}` as the body of the positive `POST /ruuvi.json` probe. The firmware copies the current
configuration before parsing the partial body, so an empty object changes no configuration fields.
Verify this by comparing the canonical authenticated `/ruuvi.json` hash before and after the probe.
Do not use `/ap.json` as the RW proof: Wi-Fi scanning is irrelevant when the gateway operates through
Ethernet and availability of that endpoint should not be a prerequisite for this test.

## Execution and configuration

The script takes **no command-line arguments**:

```text
cd cra/303645/tests
.venv/bin/python test_5_1_2a_2_b.py
```

Load the DUT configuration only from `.env` in the current working directory via
`lib.config.load_dut_config(Path(".env"))`. A missing/unparseable/malformed `.env` is an
invalid-setup ERROR.

Configuration semantics (identical to the existing tests):

- The administrative username is fixed as `Admin` and is not configurable.
- `gw_id` is the gateway's default administrative password.
- `gw_mac` is the expected six-octet MAC; verify it against the authenticated `/ruuvi.json` when the
  field is present.
- `gw_hostname` is the LAN hostname or IP without a scheme; the base URL is `http://<gw_hostname>`.

Use `GatewayClient` as the mandatory HTTP and authentication boundary, with the shared finite
connect/read timeout and user-agent policy. Do not construct or log `requests` traffic directly.

## Mandatory shared architecture

- Use `lib.config` for `.env` loading/validation, `lib.evidence` for all evidence and wire logging,
  `lib.models` for progress/results, and `lib.errors` for setup, transport, and protocol failures.
  Do not recreate any of these concerns in the test.
- Use `lib.gateway.GatewayClient` for fresh sessions, logged requests, checked JSON, complete
  interactive authentication, and the staged challenge/login operations needed for rejected-login
  assertions. Use its Basic and Digest authorization builders and `calculate_digest_ha1`; do not
  hand-build Authorization values, parse challenges/cookies, or duplicate HA1/auth-response logic.
- Use `GatewayApi`, `AuthMech`, `GatewayCfgDesc`, and `GatewayCfgLanAuthType` for the routes,
  mechanism identifiers, configuration descriptor keys, and auth-state values used by this task.
  Use `lib.http_api.HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`; select the tested
  routes from `API_INVENTORY` rather than maintaining a second route inventory.
- Stable internal schemas must be typed immutable models. Dictionaries are permitted only at genuine
  HTTP/JSON/header mapping boundaries and must use `GatewayCfgDesc` keys where applicable.

The shared modules are the primary implementation references. Firmware and Web-UI sources are
behavioral references only and must not be copied into the test.

Follow the existing test pattern: a `FunctionalTest` class with dependency injection for sessions,
random generation and clock; an `execute(...)` entry point used by host-side tests; a `main() -> int`
ending in `sys.exit(main())`; progress reported as `[Step N out of M] <description>`; the log path
printed once as `Open log file: <path>`; and a final `Overall verdict: <PASS|FAIL|ERROR>` line.
Preserve compatibility with `test_5_1_1_2_b.py` and its host tests.

## Evidence log

Every invocation, including setup failures, must create
`logs/test_5_1_2a_2_b_<UTC-date-time>.log` via `EvidenceLog`, using exclusive creation so an existing
file is never overwritten. The log must record: test case/unit; UTC start/end/duration; parsed DUT
identity; the temporary RO and RW keys; every HTTP request and response (method, URL, headers, body,
status, cookies, response body); each per-probe assertion with the observed status; per-mechanism and
overall verdicts; every configuration transition and restoration attempt; and exception
types/tracebacks. Full secrets, Authorization headers, and complete bodies **may** appear in this
local log. Terminal output stays concise (progress steps, the log path, the final verdict) and must
not print secrets.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable over its LAN HTTP interface on port 80;
- initially in the factory-default authentication state (`lan_auth_type == "lan_auth_default"`,
  `lan_auth_user == "Admin"`, `lan_auth_api_key_use == False`, and
  `lan_auth_api_key_rw_use == False`);
- with the configuration SoftAP inactive;
- with no firmware update or remote-configuration operation in progress and no other process changing
  configuration;
- with a configuration backup available in case automatic restoration fails.

A failed precondition is ERROR, not PASS/FAIL/SKIP. Do not reset or reconfigure the DUT to manufacture
the baseline. Never run against a production gateway.

The test temporarily configures API keys but **never changes the interactive administrative password**,
so `Admin`/`gw_id` remains a valid recovery credential throughout and the DUT cannot be locked out.

## Procedure

### 1. Initialize and validate the baseline

1. Create the evidence log before reading `.env`; then load and validate `.env`.
2. Authenticate as `Admin` with `gw_id` (default interactive flow) and fetch `GET /ruuvi.json`.
3. Require `lan_auth_type == "lan_auth_default"`, `lan_auth_user == "Admin"`,
   `lan_auth_api_key_use is False`, `lan_auth_api_key_rw_use is False`, and normalized `gw_mac`
   matches `.env` when present.
4. Snapshot the canonical sorted-JSON SHA-256 hash of the baseline `/ruuvi.json` for the final
   non-mutation check. A wrong baseline is ERROR.

### 2. Provision temporary RO and RW keys safely

1. Generate two distinct high-entropy keys through the shared client's injected randomness facility.
2. Using the still-authorized default `Admin` session, send `POST /ruuvi.json` with the partial body
   `{"lan_auth_api_key":"<RO>","lan_auth_api_key_rw":"<RW>"}`. Because the firmware copies the current
   configuration before parsing a partial body, `lan_auth_type`, `lan_auth_user`, and `lan_auth_pass`
   stay unchanged, and changing only the API keys does **not** clear the interactive session
   (`http_server_auth.c` `http_server_set_auth` lines 67-100). Require `200 {}`.
3. Verify the prepared state: fetch `GET /ruuvi.json` (same `Admin` session), require
   `lan_auth_type == "lan_auth_default"`, `lan_auth_api_key_use is True`, and
   `lan_auth_api_key_rw_use is True`, then snapshot its canonical sorted-JSON SHA-256 hash. Any
   failure here is ERROR followed by mandatory restoration.

### 3. Prove password schemes cannot gain M2M access (negative matrix)

The negative and positive tables are executed as four phases in one prepared state: all negative
GETs across Basic, Digest, and password-as-bearer; all positive RO/RW GETs; all negative POSTs;
then positive-table POSTs (RO denial followed by RW no-op). Neither table's per-scheme layout is
execution order. Provisioning precedes these phases and mandatory restoration follows any abort.

Run every applicable negative probe in the matrix above against `/history`, `GET /ruuvi.json`, and
`POST /ruuvi.json`. Use `{}` as the POST body except for the password-JSON-body probe. Use fresh
sessions with no authorized cookie. For the `Bearer <gw_id>` probe, assert the token differs from both
configured keys. Require the matrix status for each probe, assert no `2xx`, and stop immediately with
FAIL on any authorization success. Aggregate `/history` and `GET /ruuvi.json` results under
`AuthMech-M2M-API-Bearer-RO`, and `POST /ruuvi.json` results under
`AuthMech-M2M-API-Bearer-RW`. Confirm the POST negative probes returned before changing the
configuration (the subsequent authenticated `/ruuvi.json` hash must still equal the
post-provisioning state).
Attribute a failed post-negative hash comparison to `AuthMech-M2M-API-Bearer-RW` in both the
returned outcomes and final evidence. Prepared read-back failures instead mark temporary-state
setup ERROR and still require restoration.

### 4. Prove positive RO/RW bearer scope

Run the positive matrix: `Bearer <RO>` → `200` on `/history` and `GET /ruuvi.json`, `401` on
`POST /ruuvi.json` with `{}`; `Bearer <RW>` → `200` on `/history`, `GET /ruuvi.json`, and
`POST /ruuvi.json` with `{}`. A missing `200` where required, or a `200` where the RO key must be
denied, is FAIL for the corresponding mechanism. Fetch `/ruuvi.json` through the authorized `Admin`
session after the successful RW POST and require its canonical hash to equal the post-provisioning
state.
Finalize RO only after its reads and POST denial have passed. Preserve that RO PASS if the later RW
POST or its non-mutation check fails; mark RW FAIL for either assertion. Do not mark RW PASS before
the hash check completes.

### 5. Restore and verify non-mutation (mandatory `finally`)

Restoration is mandatory once the API keys may have been applied. In a `finally` path:

1. Send `POST /ruuvi.json` with `{"lan_auth_api_key":"","lan_auth_api_key_rw":""}` using the
   authorized `Admin` session (fallback: `Bearer <RW>`; final fallback: re-authenticate `Admin`/`gw_id`
   — always valid because the interactive password was never changed). Log every attempt; never
   silently suppress cleanup errors.
2. Verify restoration: authenticate `Admin`/`gw_id`, fetch `/ruuvi.json`, require
   `lan_auth_api_key_use is False`, `lan_auth_api_key_rw_use is False`, the canonical hash equal to the
   step-1 baseline, and both temporary keys now rejected (`Bearer <RO>` → `401` and `Bearer <RW>` →
   `401` on their previously-authorized routes, confirming the keys no longer authorize).
   A revoked explicit bearer token returns `401`, including on GET `/history`; `302` is the
   default interactive redirect for requests without a recognized bearer header, not revoked keys.

The test cannot PASS unless restoration is verified. If the security assertions passed but restoration
failed, return ERROR with a concise operator warning pointing at the evidence log and manual recovery.

## Verdict and exit codes

Emit separate results for:

- `AuthMech-M2M-API-Bearer-RO` (password-scheme rejection + positive RO scope + RO-on-RW-route denial);
- `AuthMech-M2M-API-Bearer-RW` (password-scheme rejection + positive RW scope);
- temporary-state setup;
- final restoration and non-mutation.

Overall PASS requires every result to pass and restoration to be verified.

| Exit code | Meaning |
|:---------:|:--------|
| `0` | Overall PASS |
| `1` | A security assertion failed (a password scheme gained access, wrong scope, or configuration mutation) |
| `2` | Invalid setup, infrastructure/protocol error, or unverified restoration |

Never convert an exception into a PASS-shaped default; there must be no silent errors.

## Host-side implementation tests

Use `unittest`, fake sessions/responses, temporary directories, and injected dependencies in
`test_test_5_1_2a_2_b.py`; no live gateway. Fake-server expected Authorization values, hashes, cookies,
and wire responses must be independent fixed fixtures or independently calculated reference vectors;
the fake must never call the `GatewayClient` code under test to manufacture its expectations. Cover
at least:

- the exact negative and positive route/status matrices, including the `302`-not-`401` GET behavior
  and the `401` POST behavior;
- a full PASS path with fake responses;
- unexpected `2xx` from any negative probe → FAIL with immediate abort;
- a wrong positive status (e.g. RO key `200` on `POST /ruuvi.json`, or RW key not `200`) → FAIL;
- the exact provisioning body, the exact restoration body, and that the POST negative probe carries no
  valid bearer;
- restoration attempted after PASS, FAIL, and raised exceptions, with each fallback exercised;
- restoration failure prevents PASS (returns ERROR);
- setup/evidence integration and terminal progress format (direct parsing/collision tests belong in
  `test_lib.py`);
- timeout, connection failure, and malformed JSON → ERROR;
- PASS/FAIL/ERROR exit-code aggregation.

Run this task's self-contained implementation-test module:

```text
cd cra/303645/tests
.venv/bin/python -m unittest --verbose test_test_5_1_2a_2_b.py
```

Also run all implementation tests available in the checkout to detect regressions, without
requiring a specific predecessor module:

```text
.venv/bin/python -m unittest discover --verbose --start-directory . --pattern "test_test_*.py"
```

## Acceptance criteria

- The live script runs with no arguments and loads `.env` from the current directory.
- Every run creates the required timestamped evidence log.
- It authenticates with `Admin`/`gw_id`, provisions temporary RO/RW keys, and restores/verifies them.
- Password schemes (`Basic`, `Digest`, interactive Ruuvi password as token and as body params) never
  obtain a `2xx` from any M2M endpoint.
- The configured RO and RW keys authorize exactly their firmware-defined scope.
- A compliant DUT yields PASS; any unauthorized success yields FAIL; setup/infra failures yield ERROR.
- The final authenticated comparison shows the DUT configuration is unchanged.
- Implementation and host-side tests run under Python 3.8.

## Implementation references

- [`Self_Assessment_Test_Group_5_1-2A.md`](../Self_Assessment_Test_Group_5_1-2A.md), lines 42-58.
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 321-365 and 369-409 (M2M keys); 57-240
  (password schemes).
- [`docs/http_api.md`](../../../docs/http_api.md), sections 113-169, 131-152, 188-199, and endpoints
  `/history` (289-299) and `/ruuvi.json` (240-244, 399-412).
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 49-81, 257, and 270-336.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, GET handler lines 83-210 (redirect
  150-172), POST handler 507-552.
- `components/esp32-wifi-manager/src/http_server_auth.c`, `http_server_set_auth` lines 55-100.
- `main/gw_cfg_json_parse_lan_auth.c`, lines 55-112 (API-key parsing 95-111).
- `main/gw_cfg_json_generate.c`, lines 519-580 (`lan_auth_api_key_use` flags 551-566).
- `main/http_server_cb_on_post.c`, `http_server_cb_on_post_ruuvi` lines 44-96.
- `main/ruuvi_gateway_main.c`, `ruuvi_cb_on_change_cfg` lines 424-442.
- `cra/303645/tests/test_5_1_2a_2_b.py` and `cra/303645/tests/test_test_5_1_2a_2_b.py`
  (this task's runner and implementation tests).
- `cra/303645/tests/lib/config.py`, `evidence.py`, `gateway.py`, `models.py`, `http_api.py`.

## Runner naming

Name the live-test runner and execution function `FunctionalTest_<test_id>` and
`execute_test_<test_id>`, respectively, where `<test_id>` is the normalized test-script identifier.
