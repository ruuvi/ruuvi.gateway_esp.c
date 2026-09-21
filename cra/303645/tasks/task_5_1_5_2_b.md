# Task: Automate ETSI Test Case 5.1-5-2, Unit B

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

Maintain a Python 3.8 functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case
5.1-5-2, Test Unit B: Functional Brute-Force Attempt**. The test runs against a physical Ruuvi
Gateway, measures authentication timing, verifies that guessed credentials are rejected, and restores
the gateway configuration after every outcome.

The implementation is:

- [`tests/test_5_1_5_2_b.py`](../tests/test_5_1_5_2_b.py) — live-DUT test;
- [`tests/test_test_5_1_5_2_b.py`](../tests/test_test_5_1_5_2_b.py) — deterministic implementation
  tests, not ETSI evidence;
- [`tests/lib/`](../tests/lib/README.md) — shared configuration, HTTP, authentication, evidence, error, and
  result helpers.

Do not modify firmware behavior or self-assessment source documents.

## Required shared test infrastructure

Reuse the modules under [`tests/lib/`](../tests/lib/README.md) rather than implementing infrastructure in the
test script:

- [`gateway.py`](../tests/lib/gateway.py): use `GatewayClient`, `GatewayApi`, `AuthMech`,
  `GatewayCfgDesc`, and `GatewayCfgLanAuthType`;
- [`http_api.py`](../tests/lib/http_api.py): use `HttpMethod`, `HttpStatus`, `HttpHeader`, and
  `HttpAuthScheme`;
- [`config.py`](../tests/lib/config.py), [`evidence.py`](../tests/lib/evidence.py),
  [`models.py`](../tests/lib/models.py), and [`errors.py`](../tests/lib/errors.py): use the shared
  configuration, logging, model, progress, result, and error behavior.

`GatewayClient` is the single implementation of gateway authentication. Use
`authenticate_interactive()` for complete control/restoration logins. Timing-sensitive interactive
campaigns must use its staged `prepare_interactive_challenge_request()`,
`send_interactive_challenge_request()`, `parse_interactive_challenge_response()`,
`prepare_interactive_login_request()`, and `send_interactive_login_request()` APIs so local
cryptographic preparation and response parsing remain outside wire timing. Use
`interactive_login_challenge_from_response()` to chain failed POST challenges,
`calculate_digest_ha1()` for stored HA1 values, `parse_digest_challenge()` and
`authorization_header_digest()` for Digest, and `authorization_header_basic()` for Basic.

Do not duplicate client-side ECDH, challenge/cookie parsing, password-response calculation,
Authorization-header construction, request logging, checked JSON decoding, session creation, or
transport-error translation in the live test or host-test client path. The fake gateway should still
model the server independently so it can validate the client's wire output. Extend `tests/lib/` only
when new behavior is reusable by multiple functional tests.

## Current test matrix

Run ten failed-authentication attempts and ten successful controls where applicable:

| IXIT mechanism                       | Expected status and timing                                                                                          |
| :----------------------------------- | :------------------------------------------------------------------------------------------------------------------ |
| `AuthMech-LAN-WebUI-Default`         | Measure GET and POST separately. Every GET and failed POST exceeds 1 second; successful POST averages below 250 ms. |
| `AuthMech-LAN-WebUI-User-Defined`    | Same requirements as default interactive authentication.                                                            |
| `AuthMech-M2M-API-Bearer-RO`         | Valid control averages below 250 ms. Every independent 256-bit guess returns 401 after more than 1 second.          |
| `AuthMech-M2M-API-Bearer-RW`         | Valid control averages below 250 ms. Every independent 256-bit guess returns 401 after more than 1 second.          |
| `AuthMech-LAN-WebUI-Basic`           | Valid controls average below 250 ms. Every invalid credential returns 401 after more than 1 second.                 |
| `AuthMech-LAN-WebUI-Digest`          | Valid authenticated responses average below 250 ms. Every invalid credential returns 401 after more than 1 second.  |
| `AuthMech-LAN-WebUI-Unauthenticated` | Every request returns 200. Timing is recorded but does not affect the verdict.                                      |
| `AuthMech-LAN-WebUI-Disabled`        | Every request returns 403 after more than 1 second. A 403 is correct because no credential can grant access.        |

Production constants:

```text
ATTEMPTS_PER_TARGET = 10
MIN_FAILED_ATTEMPT_SECONDS = 1.00
MAX_SUCCESS_AVERAGE_SECONDS = 0.250
MAX_INTERACTIVE_ATTEMPTS_PER_SECOND = 1.12
```

Threshold comparisons are strict: an exact 1.00- or 0.250-second boundary does not pass.

The source assessment describes 1,000 attempts and entropy-only defenses for bearer/legacy modes;
this runner uses the ten-attempt matrix above, not that historical campaign. Current firmware applies
the shared delay to HTTP `401` and `403` in
[`http_server_netconn_serve_handle_req()`](../../../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L197),
including Basic, Digest, and bearer denials. Do not interpret this reduced run as proof of keyspace
exhaustion or silently substitute the source's timing claims for the implemented matrix.

## Timing interpretation

`GatewayClient.authenticate_interactive()` performs two HTTP requests:

1. `GET /auth` obtains a challenge and normally returns 401 after approximately one second.
2. `POST /auth` returns immediately on success and is delayed approximately one second on failure.

Measure GET and POST independently, excluding client-side ECC/hash preparation and response parsing.
For successful controls, obtain a new challenge before each POST because a successful POST consumes
the login session. Require every successful GET to exceed one second and the successful POST average
to remain below 250 ms after ignoring one slowest response.

For a failed brute-force campaign, send exactly one initial GET. Each failed POST returns 401 together
with a fresh `x-ruuvi-interactive` challenge and `RUUVISESSION` cookie; use those values for the next
POST instead of sending another GET. Require the initial GET and every failed POST to exceed one
second. Compute aggregate interactive throughput from POST timings only, ignoring one slowest POST.

Basic and bearer authentication use one request per measured attempt.

Digest uses two requests per logical attempt:

1. an unauthenticated GET obtains the Digest challenge;
2. a second GET carries the `Authorization: Digest ...` header.

Acquire the challenge before starting the success/failure response timer. Measure only the second
authenticated response. The first request correctly has no Authorization header.

The gateway also sends data to cloud services and can occasionally delay an HTTP response. Such work
can make a response slower but cannot make the firmware's authentication delay faster:

- preserve every raw sample in evidence;
- for upper-bound successful-response averages, discard exactly one slowest sample;
- for aggregate interactive throughput, discard exactly one slowest sample;
- for every minimum-delay requirement, always use the raw minimum and discard nothing.

Do not add client-side sleeps or run attempts in parallel.

## Credentials and protocol details

### Default interactive authentication

Always use username `Admin`. Generate each wrong password from an independent random 64-bit value and
format it as `XX:XX:XX:XX:XX:XX:XX:XX`. Regenerate if it equals `.env` `gw_id`.

### Temporary user-defined authentication

Generate a distinct username and a 256-bit random plaintext password. Capture the live authentication
realm from the gateway challenge and store:

```text
MD5(username + ":" + realm + ":" + plaintext_password)
```

Do not derive the realm from `gw_hostname`.
Failed attempts use this configured username and independent wrong 256-bit passwords, so the campaign
exercises password guessing rather than rejection of unknown usernames.

### Bearer authentication

Generate temporary independent 256-bit RO and RW keys. Each failed guess must also be an independent
256-bit value and must differ from both configured keys.

Use `/history` for RO checks. Prove RW authorization with `POST /ruuvi.json` and exactly `{}` as its
body; compare the canonical configuration hash before and after to prove that no field changed. Use
`/status.json` for timed valid bearer controls so configuration handling does not affect the
sub-250-ms success benchmark.
Do not depend on `/ap.json` or Wi-Fi scanning. The RO guess campaign uses `GET /history`; the RW
guess campaign uses `POST /ruuvi.json` with `{}`. Use fresh cookie-free sessions for guesses and
scope checks. RO on the POST route must return HTTP `401`; valid RW returns `200`. Revoked explicit
RO/RW bearer tokens return `401`, not an interactive `302` redirect.

Within the prepared custom state, complete the interactive and bearer read controls, custom
password campaign, and RO guess campaign before configuration probes. Then check RO denial on POST,
verify an RW no-op and its unchanged hash, run the RW guess campaign, and repeat the RW no-op/hash
control. Basic/Digest/ALLOW/DENY campaigns follow in their own explicitly configured states.
Timing-only failures may be collected while testing continues, but **any unexpected `2xx` from a
negative probe aborts all later probes and transitions**. Run mandatory restoration after that abort.

### Basic authentication

The firmware compares the Base64 token following `Basic ` directly with `lan_auth_pass`.
`lan_auth_pass` can store at most 64 characters. Generate a dedicated 128-bit Basic password so the
complete Base64 encoding of `username:password` fits that limit. Do not reuse the longer temporary
Ruuvi password.

### Digest authentication

Store HA1 using `GatewayClient.calculate_digest_ha1()`:

```text
MD5(username + ":" + realm + ":" + plaintext_password)
```

Build authenticated requests with `GatewayClient.authorization_header_digest()` rather than
reimplementing the response calculation from HA1, nonce, nc, cnonce, qop, and HA2.
The live `WWW-Authenticate` header separates quoted fields with spaces rather than RFC-style commas:

```text
Digest realm="..." qop="auth" nonce="..." opaque="..."
```

Use `GatewayClient.parse_digest_challenge()`, which supports this syntax. Host fixtures must reproduce
the live space-separated header; `requests.utils.parse_dict_header()` cannot parse it correctly.

## Transport retries

A gateway busy with other work can exceed the 15-second HTTP read timeout. A timeout is not
authentication evidence and must not be counted as a completed attempt.

- Bearer guesses use up to three transport attempts with the same logical token and a fresh session.
- Digest uses up to three transport attempts. Each retry obtains a new challenge on a fresh session
  before sending a new Authorization header.
- Only the response from the completed transport attempt contributes to latency evaluation.
- Log every retry. Exhaustion remains ERROR.

Do not use retry delays to hide a fast authentication response.

## Configuration transitions

Before mutation, authenticate using `Admin`/`gw_id`, require `lan_auth_default`, require disabled RO
and RW keys, and compare two independently decoded `/ruuvi.json` responses for stability.

Provision the temporary state with only:

```json
{
  "lan_auth_type": "lan_auth_ruuvi",
  "lan_auth_user": "<username>",
  "lan_auth_pass": "<HA1>",
  "lan_auth_api_key": "<RO-key>",
  "lan_auth_api_key_rw": "<RW-key>"
}
```

Authentication-only transitions must not send or preserve `mqtt_data_format`, `mqtt_prefix`, or
`mqtt_client_id`. The final full-configuration hash comparison will reveal whether omission changes
gateway state on the tested firmware.

Authentication changes clear interactive sessions, and the firmware has a small authorization-session
pool. Obtain a fresh valid login immediately before a configuration POST instead of assuming an older
session remains authorized.

## Restoration and operator recovery

Restoration is mandatory after mutation:

```json
{
  "lan_auth_type": "lan_auth_default",
  "lan_auth_api_key": "",
  "lan_auth_api_key_rw": ""
}
```

Try the temporary RW bearer first, then the temporary interactive credential, then `Admin`/`gw_id`.
Verify:

- default login succeeds;
- `lan_auth_type` is `lan_auth_default`;
- both API keys are disabled;
- the restored canonical configuration hash equals the stable baseline;
- temporary interactive and bearer credentials no longer authorize.

Use read-only `GET /history` and `GET /ruuvi.json` to verify revocation of the temporary RO and RW
keys respectively; do not issue another potentially mutating negative probe after restoration.

The test cannot PASS without verified restoration. If a run starts in a non-default authentication
mode, or if restoration cannot be verified, end with this explicit operator instruction:

```text
USER ACTION REQUIRED: Factory reset erases saved local configuration, credentials, tokens, and 
uploaded certificates/private keys and interrupts connectivity. Back up needed settings first. 
It does not delete cloud accounts or already-forwarded data. Hold CONFIGURE through the LED turning 
off and the Gateway restarting until the red LED repeatedly turns on for 200 ms and off for 200 ms 
(normally about 11 seconds after the initial press). Release only after this completion signal;
the Gateway restarts again and opens its configuration hotspot. If the signal never appears,
do not assume erasure succeeded.
```

Eleven seconds is user-reported approximate elapsed time, not an exact threshold or a guarantee
that shorter holds are safe. A separate five-second timer requests restart; holding at the boot-time
check selects erasure. Hotspot appearance, lost LAN access, LED off, or solid red alone are not
confirmation, and release after the completion signal cannot undo erasure. See the
[warned local reset procedure](../ixit_25-DelFunc.md#initiation-and-interaction).

Non-default mode detection uses `GatewayAuthenticationModeError`; the recovery instruction is carried
in `RunResult.recovery_message` and must be the final console message.

## Evidence and verdicts

Log every HTTP request/response, credential, raw timing sample, retry, transition, restoration attempt,
exception, per-mechanism result, and overall result. Print each attempt and measured response time to
the console. End with a summary containing the evaluated successful average and raw failed minimum for
every mechanism.

Stable evidence payloads must be immutable dataclasses with named attributes, including nested
campaign counts and restoration-attempt records. Do not pass schema-like dictionaries to
`EvidenceLog.write()` or other internal helpers. `EvidenceLog` owns conversion of typed records to
JSON; dictionaries are reserved for genuine protocol mappings such as parsed gateway configuration,
request JSON, and HTTP headers, whose keys use the shared descriptor classes.

Use exit codes:

Overall PASS requires an explicit PASS for every mechanism, temporary-state setup, and final
restoration. An incomplete/skipped result is ERROR. Late control or non-mutation failures belong to
the affected mechanism and must replace an earlier PASS; preserve the other completed results.

| Exit code | Meaning                                                                                 |
| :-------: | :-------------------------------------------------------------------------------------- |
|    `0`    | PASS                                                                                    |
|    `1`    | Security/timing assertion failed                                                        |
|    `2`    | Invalid setup, protocol/transport error, incomplete campaign, or unverified restoration |

Use test-specific symbols `FunctionalTest_5_1_5_2_b` and `execute_test_5_1_5_2_b`; retain `main()`
as the zero-argument script entry point.

## Host-side implementation tests

Host tests must model:

- independently JSON-decoded values so string identity bugs cannot pass;
- exact live Basic and Digest storage/header behavior;
- a four-entry interactive-session pool;
- one initial GET followed by chained failed POST challenges for Default and user-defined campaigns;
- separate successful/failed GET and POST timing evidence for interactive authentication;
- successful and failed timing thresholds, including strict boundary checks;
- one ignored slow spike for upper-bound averages, with raw minimum checks unchanged;
- bearer and Digest transport retry success and exhaustion;
- omitted MQTT fields remaining absent from authentication payloads;
- all authentication transitions and restoration fallbacks;
- factory-reset output when initial mode is non-default or restoration fails;
- Basic and Digest setup responses with their actual challenge schemes, rejected default logins,
  and mismatched/malformed/missing MAC combined with non-default fields;
- late custom/RO/RW control failures overriding earlier mechanism PASS in results and evidence;
- prepared-state login failure marking temporary-state setup ERROR even after provisioning passed;
- IDE-clean Optional and callable typing.
- complete wire-login identity-before-mode checks, exact bearer method/path/body/session ordering,
  no Wi-Fi-scan dependency, and hash-checked RW no-op controls;
- immediate abort on unexpected interactive or bearer success, with only restoration/verification
  requests afterward, and incomplete mechanisms preventing overall PASS;
- fake config writes enforcing RW authorization and clearing sessions on auth changes, while a
  valid empty update leaves configuration and sessions unchanged.

Run:

```text
cd cra/303645/tests
.venv/bin/python -m unittest discover --verbose --start-directory . --pattern "test_test_*.py"
```

## Implementation references

- [`tests/lib/gateway.py`](../tests/lib/gateway.py) — primary authentication and gateway HTTP API;
- [`tests/lib/http_api.py`](../tests/lib/http_api.py) — shared HTTP vocabulary;
- [`tests/lib/config.py`](../tests/lib/config.py), [`tests/lib/evidence.py`](../tests/lib/evidence.py),
  [`tests/lib/errors.py`](../tests/lib/errors.py), and [`tests/lib/models.py`](../tests/lib/models.py)
  — shared test infrastructure;
- [`Self_Assessment_Test_Group_5_1-5.md`](../Self_Assessment_Test_Group_5_1-5.md)
- [`ixit_1-AuthMech.md`](../ixit_1-AuthMech.md)
- [`docs/http_api.md`](../../../docs/http_api.md)
- `components/esp32-wifi-manager/src/http_server_handle_req.c`
- `components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`
- `components/esp32-wifi-manager/src/http_server_auth_digest.c`
- `components/esp32-wifi-manager/src/http_server_resp.c`
- `main/gw_cfg_json_parse_lan_auth.c`

The firmware sources are behavioral references for the fake gateway and assertions, not code to copy
into the Python client.

## Runner naming

Name the live-test runner and execution function `FunctionalTest_<test_id>` and
`execute_test_<test_id>`, respectively, where `<test_id>` is the normalized test-script identifier.
