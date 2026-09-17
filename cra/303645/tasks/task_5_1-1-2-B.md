# Task: Automate ETSI Test Case 5.1-1-2, Unit B

## Mandatory factory-default precondition

Before test-specific probes, authenticate and fetch `GET /ruuvi.json`, then use the shared
`lib.config` helper to compare `lan_auth_type`, `lan_auth_user`, `lan_auth_api_key_use`, and
`lan_auth_api_key_rw_use` directly with `gw_cfg_default/gw_cfg_default_gen_ui.json`. The generated file
already matches the public UI/API representation; do not derive it from the raw saved configuration or
handle hidden passwords or keys. Any missing, wrongly typed, or non-default value is setup ERROR:
print the shared CONFIGURE-button factory-reset instruction and stop without changing the DUT to
manufacture a baseline.

## Objective

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.1-1-2, 
Test Unit B: Requirement to Set User-Defined Passwords**.

The test must run against a physical Ruuvi Gateway over its LAN HTTP interface and verify that all
user-defined authentication mechanisms remain unusable until the corresponding secret is configured.
It must exercise every HTTP API method/path listed in the firmware-derived
[HTTP API reference](../../../docs/http_api.md), not only representative read and write endpoints.

This is a live DUT test. Mock-based tests of the Python implementation are useful for development,
but they do not replace the physical-gateway run or constitute compliance evidence.

## Deliverables

1. Create `cra/303645/tests/test_5_1_1_2_b.py`, compatible with Python 3.8.
2. Reuse the existing modules under `cra/303645/tests/lib/`. Extend them only when functionality is
   genuinely reusable by multiple functional tests; do not copy shared behavior into the test script.
3. Add dependency declarations or brief usage documentation under `cra/303645/tests/` only when
   required. Reuse `requests~=2.33.0` and `pycryptodome~=3.21.0` from
   `ruuvi.gwui.html/scripts/requirements.txt` where applicable.
4. Add deterministic host-side unit tests for the Python implementation. They must mock HTTP and must
   be clearly described as implementation tests, not ETSI functional-test evidence.

Do not modify firmware behavior or the self-assessment source documents.

## Required shared test infrastructure

The test script must contain only case-specific procedure, assertions, safety ordering, and verdict
aggregation. Use the shared modules under [`tests/lib/`](../tests/lib/) for infrastructure:

- [`gateway.py`](../tests/lib/gateway.py): `GatewayClient`, `GatewayApi`, `AuthMech`,
  `GatewayCfgDesc`, and `GatewayCfgLanAuthType`;
- [`http_api.py`](../tests/lib/http_api.py): `API_INVENTORY`, `EXPECTED_API_INVENTORY`,
  `ApiRoute`, `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`;
- [`config.py`](../tests/lib/config.py): strict `.env` loading and validation;
- [`evidence.py`](../tests/lib/evidence.py): exclusive evidence-log creation, structured records,
  wire request/response logging, exceptions, and final timestamps;
- [`models.py`](../tests/lib/models.py) and [`errors.py`](../tests/lib/errors.py): DUT/result/progress
  models and shared error classification.

Authentication code must use `GatewayClient`. In particular, use `authenticate_interactive()` for a
complete Ruuvi login, `response_json()` for checked JSON decoding, `authorization_header_basic()` and
`authorization_header_digest()` for Authorization values, and `new_session()`/`request()` for logged
HTTP operations. Do not reimplement ECDH exchange, challenge parsing, cookie handling, HA1/password
response calculation, Basic/Digest formatting, request logging, or transport-error translation in the
test script.

## Normative scope

The source is
[`Self_Assessment_Test_Group_5_1-1.md`](../Self_Assessment_Test_Group_5_1-1.md), lines 51-68.
Unit B covers these IXIT mechanisms:

| IXIT mechanism | Required unconfigured-state behavior |
|:---------------|:-------------------------------------|
| `AuthMech-LAN-WebUI-User-Defined` | Custom Ruuvi interactive authentication is not active until a custom username/password is committed. |
| `AuthMech-LAN-WebUI-Basic` | HTTP Basic authentication is not active until explicitly selected and configured. |
| `AuthMech-LAN-WebUI-Digest` | HTTP Digest authentication is not active until explicitly selected and configured. |
| `AuthMech-M2M-API-Bearer-RO` | The read-only API key is empty/disabled and cannot authorize any read API. |
| `AuthMech-M2M-API-Bearer-RW` | The read/write API key is empty/disabled and cannot authorize any read/write API. |

See [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md), lines 119-240 and 321-409.

Testing successful access after configuring new secrets belongs to other access-control tests. This
unit tests the security boundary **before** user-defined secrets exist.

## Test configuration

The script takes **no command-line arguments**:

```text
cd cra/303645/tests
.venv/bin/python test_5_1_1_2_b.py
```

Load the DUT configuration with `load_dut_config()` from `.env` in the current working directory. Any
missing, malformed, duplicate, or unknown field is an invalid-setup ERROR.

`.env` format:

```dotenv
gw_id=XX:XX:XX:XX:XX:XX:XX:XX
gw_mac=XX:XX:XX:XX:XX:XX
gw_hostname=ruuvigateway9c2c.local
```

Configuration semantics:

- `gw_id` is the gateway's default password.
- `gw_mac` is the expected six-octet gateway MAC address.
- `gw_hostname` is the LAN hostname or IP address, without a URL scheme.
- The administrative username is always `Admin` and is not configurable.

The configuration files contain credentials and must not be committed.

## Evidence log

Create the log through `EvidenceLog.create()`. Every invocation, including configuration or connection
failures, must create:

```text
logs/test_5_1_1_2_b_<UTC-date-time>.log
```

The log must contain:

- test case and unit;
- UTC start/end timestamps and duration;
- parsed DUT identity (`gw_hostname`, `gw_mac`, and `gw_id`);
- each request method and URL;
- request headers and body;
- response status, headers, cookies, and body;
- authentication calculations and ECDH material when generated;
- each assertion, per-route result, per-mechanism result, and overall verdict;
- exception type and traceback for errors.

Secrets, cookies, authorization headers, hashes, ECDH material, and full response bodies **may be
included in this local evidence log**. Do not print those details to the terminal. Print the log path
once as `Open log file: <path>`, report long-running phases as `[Step N out of M] <description>`, and
keep terminal results concise. The log directory must remain ignored by Git.

## DUT preconditions and safety

Run only on a dedicated test gateway:

- reachable from the test host over its LAN interface on HTTP port 80;
- in `lan_auth_default` mode;
- with both M2M API keys disabled/empty;
- with the configuration SoftAP inactive, so LAN requests are not blocked while the AP is active;
- not concurrently modified by another process;
- with no firmware update or remote configuration operation in progress.

The complete API matrix includes update, storage, reset, connectivity, and configuration endpoints.
Correct firmware rejects every negative probe before its endpoint handler runs. An authentication
regression could allow a nominally rejected request to reach a destructive handler. Therefore:

- use a disposable lab configuration and preserve any required backup before running;
- issue dangerous POST/DELETE routes last;
- stop immediately after any unexpected authorization success;
- use empty or deliberately invalid bodies/parameters where authentication is evaluated first;
- after the matrix, authenticate normally and verify the baseline configuration is unchanged;
- never run this test against a production gateway.

Do not reset or reconfigure the DUT to manufacture the required baseline. A failed precondition is
ERROR, not PASS, FAIL, or SKIP.

## Authentication protocol

Use `GatewayClient.authenticate_interactive()` for the valid default login and invalid random
interactive login. Assert the returned challenge response, parsed authentication payload, and login
response; do not reproduce the underlying P-256 ECDH, `x-ruuvi-interactive`, cookie, HA1, or password
response implementation.

Use `GatewayClient.authorization_header_basic()` and
`GatewayClient.authorization_header_digest()` for negative Basic and Digest probes. Use the shared
gateway/configuration/HTTP descriptor classes at every call site instead of raw paths, field names,
authentication mechanism IDs, methods, headers, schemes, or status codes.

## Canonical HTTP API inventory

Iterate over `API_INVENTORY` from `tests/lib/http_api.py`; do not declare a private copy in the test.
[`docs/http_api.md`](../../../docs/http_api.md) remains authoritative for behavior. Assert that the
ordered inventory contains 26 unique routes and equals `EXPECTED_API_INVENTORY`. Static asset
fallback, captive-portal redirects, unknown-route fallbacks, and simulator-only routes are not APIs.

## Procedure

### 1. Load configuration and open the evidence log

Load and validate the current-directory environment file, create the log, and record the test identity
and configuration. A parsing failure is overall ERROR with exit code 2.

### 2. Authenticate and validate the baseline

1. Perform interactive authentication as `Admin` using `gw_id`.
2. Fetch `/ruuvi.json`.
3. Require:
   - `lan_auth_type == "lan_auth_default"`;
   - `lan_auth_user == "Admin"`;
   - `lan_auth_api_key_use is False`;
   - `lan_auth_api_key_rw_use is False`.
4. Record any firmware/version and identity fields exposed by authenticated responses. If a response
   exposes a gateway MAC, compare its normalized value with `gw_mac`; a mismatch is invalid setup.
5. Snapshot the four baseline fields and a canonical hash of the full configuration response for the
   final non-mutation check.

Missing/wrongly typed fields, failed default authentication, or an incorrect baseline is ERROR.

### 3. Test the interactive mechanisms

Use fresh sessions without the authorized baseline cookie:

1. Confirm `GET /auth` advertises `x-ruuvi-interactive` and reports `lan_auth_default`, not
   `lan_auth_ruuvi`, `lan_auth_basic`, or `lan_auth_digest`.
2. Attempt Ruuvi interactive login with a random non-default username and password-derived response.
   Require `401` and no authorized session.
3. For every non-`/auth` API method/path, send:
   - no Authorization header;
   - syntactically valid Basic credentials generated randomly;
   - a syntactically valid Digest Authorization header generated randomly.
4. Expected denial behavior:
   - protected GET APIs: `302`, `401`, or `403`, but never `2xx`; in default mode the firmware
     normally returns `302`, including for `/ap.json` and `/status.json` (those two only omit the
     previous-URL cookie);
   - all protected POST and DELETE APIs: `401` or `403`, but never `2xx`.
5. `GET /auth` must not advertise Basic or Digest. Exercise `POST /auth` through the failed random
   interactive login above. Exercise `DELETE /auth` without an authorized session and require `401`.

Any `2xx` from a negative probe is FAIL and must stop the test immediately.

### 4. Test disabled RO and RW bearer authentication across all APIs

Generate a fresh cryptographically random bearer token for each request. Never use `gw_id` as a
bearer token.

1. Probe every one of the 26 method/path combinations with
   `Authorization: Bearer <random-token>`.
2. Use no query string and the least-operative request body:
   - GET and DELETE: no body;
   - POST `/auth`: `{}` with a fresh unauthenticated session;
   - every other POST: empty body or `{}` as appropriate, without valid endpoint parameters.
3. Require exactly `401` for all routes. An explicitly supplied prohibited bearer token is handled by
   the authentication layer before redirect, origin restriction, argument validation, or endpoint
   execution.
4. Test read-only routes before RW/dangerous routes. Stop immediately on the first non-`401`.

For reporting, aggregate GET bearer results under `AuthMech-M2M-API-Bearer-RO` and POST/DELETE
results under `AuthMech-M2M-API-Bearer-RW`. Because an RW token is also valid for reads when
configured, the absence of both baseline keys plus denial across the complete route inventory is the
required evidence.

### 5. Verify non-mutation

Authenticate again with `Admin`/`gw_id`, fetch `/ruuvi.json`, and compare it with the baseline:

- the four authentication fields must be unchanged;
- the canonical full-configuration hash must match, excluding only fields proven to be volatile;
- the gateway must still answer `/status.json`.

Any persistent difference caused by the test is FAIL and must be listed in the evidence log.

### 6. Produce the verdict

Emit separate results for:

- `AuthMech-LAN-WebUI-User-Defined`;
- `AuthMech-LAN-WebUI-Basic`;
- `AuthMech-LAN-WebUI-Digest`;
- `AuthMech-M2M-API-Bearer-RO`;
- `AuthMech-M2M-API-Bearer-RW`;
- complete HTTP API inventory coverage;
- final non-mutation verification.

Overall PASS requires every result to pass and all 26 API method/path combinations to be covered.
Authorization success, an incorrect denial status, missing inventory coverage, or DUT mutation is
FAIL. Invalid setup, connectivity failures, timeouts, malformed responses, or inability to complete
the matrix is ERROR. Never convert an exception into a PASS-shaped default.

Exit codes:

| Exit code | Meaning |
|:---------:|:--------|
| `0` | Overall PASS |
| `1` | At least one security assertion FAIL |
| `2` | Test ERROR or invalid setup |

## Host-side implementation tests

Mock the HTTP boundary without contacting a gateway and cover at least:

- `.env` parsing, validation, and no shell evaluation;
- log creation and collision prevention;
- the exact 26-entry API inventory;
- complete PASS response sequences;
- per-route expected statuses for unauthenticated, Basic, Digest, and bearer probes;
- unexpected `2xx` and wrong bearer status handling;
- immediate abort after authorization success;
- non-default auth mode or enabled API-key flags;
- timeout, connection failure, malformed JSON, and missing auth headers;
- redirect following disabled on negative probes;
- final-state mismatch;
- PASS/FAIL/ERROR exit-code aggregation.

These tests validate the Python implementation only. The live physical-DUT run is mandatory for the
functional-test verdict.

## Acceptance criteria

- Running the script with no arguments loads `.env` from the current working directory.
- Every run creates the required timestamped evidence log.
- The live test authenticates with `Admin` and `gw_id`.
- All 26 documented API method/path combinations receive the required negative probes.
- The five IXIT mechanisms have separate verdicts and traceable per-route evidence.
- A compliant default-state DUT produces PASS; any unauthorized success produces FAIL.
- Setup and infrastructure failures produce ERROR, never PASS.
- The final authenticated comparison shows that the DUT configuration is unchanged.
- The implementation and host-side tests run on Python 3.8.

## Implementation references

- [`tests/lib/gateway.py`](../tests/lib/gateway.py) — primary authentication and gateway HTTP API;
- [`tests/lib/http_api.py`](../tests/lib/http_api.py) — canonical API inventory and HTTP vocabulary;
- [`tests/lib/config.py`](../tests/lib/config.py), [`tests/lib/evidence.py`](../tests/lib/evidence.py),
  [`tests/lib/errors.py`](../tests/lib/errors.py), and [`tests/lib/models.py`](../tests/lib/models.py)
  — shared test infrastructure;
- [`docs/http_api.md`](../../../docs/http_api.md) — complete firmware HTTP API and authorization
  reference.
- `cra/303645/Self_Assessment_Test_Group_5_1-1.md`, lines 29-68.
- `cra/303645/ixit_1-AuthMech.md`, lines 119-240 and 321-409.
- `main/gw_cfg_default.c`, lines 81-86.
- `main/gw_cfg_json_generate.c`, lines 520-579.
- `components/esp32-wifi-manager/src/http_server_handle_req.c`, lines 83-230 and 507-679.
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`, lines 49-81 and 258-358.
- `components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c`, lines 248 onward.
- `main/http_server_cb_on_get.c`, `main/http_server_cb_on_post.c`, and `main/http_server_cb.c`.

The firmware sources are behavioral references for the fake gateway and assertions, not alternative
client implementations.

## Runner naming

Name the live-test runner and execution function `FunctionalTest_<test_id>` and
`execute_test_<test_id>`, respectively, where `<test_id>` is the normalized test-script identifier.
