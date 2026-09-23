# CRA Functional-Test Library

This package contains shared infrastructure for the Python functional tests in
[`cra/303645/tests`](../README.md). Its purpose is to keep configuration parsing, evidence logging,
HTTP protocol handling, authentication, and common data models consistent across live-DUT tests.

The package supports Python 3.8. It is internal to this repository rather than a separately
published Python package. It is governed by the subtree instructions in
[`../AGENTS.md`](../AGENTS.md); a second `AGENTS.md` is intentionally unnecessary here.

## Architecture

The package is split by responsibility:

| Module           | Responsibility                                                                                                   |
|------------------|------------------------------------------------------------------------------------------------------------------|
| `config.py`      | Strict `.env` and generated default-configuration loading and validation                                         |
| `errors.py`      | Shared setup, connection, protocol, and authentication-mode exceptions                                           |
| `evidence.py`    | Exclusive timestamped evidence logs, structured records, HTTP transcripts, exceptions, and final verdicts        |
| `gateway.py`     | Gateway HTTP client, authentication modes, Basic and Digest authorization, and interactive ECDH authentication   |
| `http_api.py`    | Gateway endpoint names, canonical HTTP methods, statuses, headers, schemes, and the firmware API route inventory |
| `models.py`      | DUT configuration, test results, and console progress reporting                                                  |
| `webresource.py` | Unauthenticated public HTTPS fetching, bounded redirects, transport errors, and immutable observations           |
| `serial_dut.py`  | CH340 discovery, serial-tool preflight, and bounded reset/console capture with injectable hardware and clocks    |

`GatewayClient` is the main runtime boundary. It prepares requests, writes request and response
evidence, translates `requests` transport failures into library exceptions, and implements the
gateway authentication flows. Functional-test scripts build their test-specific assertions and
state transitions on top of this client.

This README describes reusable API contracts and helper behavior. Concrete procedures, capture
deadlines, expected observations, verdict rules, and live-run findings belong in the matching
`task_<id>.md` specification in the [task directory](../../tasks).

## Public API

### Configuration

- `load_dut_config(path)` reads exactly `gw_id`, `gw_mac`, and `gw_hostname` from a local `.env`
  file. Unknown, duplicate, missing, and malformed values raise `InvalidConfig`.
- `validate_hostname(value)` accepts DNS names, IPv4 addresses, and IPv6 addresses without a URL
  scheme, port, path, credentials, or surrounding whitespace.
- `load_ui_default_config(path=...)` reads the generated gateway UI defaults as a JSON object.
- `default_config_values(fields, path=...)` selects required fields from those defaults and rejects
  missing fields.
- `FACTORY_RESET_MESSAGE` and `AUTHENTICATION_DEFAULT_FIELDS` define shared functional-test policy.
  The recovery message requires the red LED completion signal (200 ms on/200 ms off) after boot-time
  erasure, warns about lost local settings, and treats about 11 seconds as typical elapsed time.
  Releasing CONFIGURE after this signal triggers another restart, after which the configuration
  hotspot opens.
  Runners decide when reset advice is appropriate and include it in evidence and terminal output.

### Models

- `DutConfig` stores the gateway ID, MAC address, and hostname. Its `base_url` property formats DNS,
  IPv4, and bracketed IPv6 URLs.
- `RunResult` carries the process exit code, verdict, per-check outcomes, route coverage, and an
  optional recovery message.
- `ProgressReporter` emits numbered progress messages through an injected output callback.
  It counts phases, not result explanations. Runners should use a separate output callback for
  concise PASS/FAIL/ERROR reasons and mirror those lines with `EvidenceLog.write_line()`; neither
  `RunResult` nor `EvidenceLog.exception()` automatically prints a diagnostic to the terminal.

### Evidence

- `EvidenceLog.create(log_dir, filename_prefix, now=...)` creates a timestamped file using exclusive
  creation. A numeric suffix prevents collisions.
- `write()` and `write_line()` record structured or plain evidence and flush immediately.
- `write_http_request()` and `write_http_response()` record complete HTTP exchanges.
- `exception()` records an exception and traceback.
- `finish()` uses one injected end timestamp for all terminal record prefixes and values, records
  duration and overall verdict, flushes, and closes the log.
- `AssertionEvidence`, `MechanismResultEvidence`, `RouteResultEvidence`, and
  `HashComparisonEvidence` are structured evidence records.

Evidence logs can contain credentials, authentication material, configuration values, and HTTP
bodies. Store and share them with the same care as DUT credentials.

### Gateway HTTP and authentication

- `GatewayCfgDesc`, `GatewayCfgLanAuthType`, and `AuthMech` contain shared gateway protocol names.
  `gateway.py` re-exports `GatewayApi` for backward compatibility.
- `GatewayClient.new_session()` disables Requests' environment settings (`trust_env=False`),
  including automatic `.netrc` authentication and environment proxies, so host settings cannot
  silently change the authentication under test.
- The session factory is `Callable[[], requests.Session]`; `new_session()` and the
  authentication models carry concrete `requests.Session` objects. Request, challenge,
  and login responses are `requests.Response` objects. Host-side transport fixtures
  subclass these Requests types and override sending so they never contact a DUT.
  Dynamic JSON bodies and decoded payloads remain typed as `Any` at the protocol boundary.
- `GatewayClient.request()` sends a request with fixed timeouts and redirects disabled by default.
  It accepts either `json_body` or `data`, but never both.
- `response_json()` validates JSON decoding and, optionally, the top-level Python type.
- `authorization_header_basic()` and `authorization_header_digest()` construct authorization
  headers. `calculate_digest_ha1()` exposes the shared HA1 calculation.
- `parse_interactive_challenge()` and `parse_digest_challenge()` validate authentication challenge
  parameters and require a space or tab after the case-insensitive authentication scheme token.
- `request_interactive_challenge()`, `submit_interactive_authentication()`, and
  `authenticate_interactive()` implement the gateway interactive authentication sequence and ECDH
  key derivation. Gateway ECDH public keys at the point at infinity are rejected with
  `GatewayProtocolError` before deriving shared key material.

For runtime device identification, authenticated `GatewayApi.CONFIG` (`GET /ruuvi.json`) exposes
`GatewayCfgDesc.FW_VER`, `NRF52_FW_VER`, and `GW_MAC`. Callers can reuse an authenticated configuration
response that already contains these fields. `/status.json` is network status, not the outbound
HTTP statistics report: its schema must not be inferred from `ruuvi_gw_status.schema.json`.

`GatewayApi.METRICS` (`GET /metrics`) returns Prometheus text rather than JSON. Use
`GatewayClient.request()` with the existing authenticated session so requests/responses retain
normal evidence, timeout, and redirect handling; do not use the unauthenticated public-resource
helper in `webresource.py`. The `ruuvigw_info` sample exposes `mac`, `esp_fw`, and `nrf_fw` labels.
Callers select the required observations, validate device identity, parse metrics, and compare
values across sources according to their task specification. The HTTP transport records and
returns responses without assigning a compliance verdict.

### Public web resources

`fetch_public_resource(url, evidence, *, allowed_hosts, connect_timeout, read_timeout,
max_redirects, user_agent, session_factory=requests.Session)` uses a fresh caller-supplied session
and closes it after the fetch. It disables environment authentication/proxies, clears initial
credentials/cookies/parameters, and always sends `verify=True` with finite positive timeouts.
It uses `EvidenceLog` to record requests before transmission and responses before interpretation.
No DUT configuration is loaded. Case-specific URLs, host sets, and acceptable statuses stay in runners.

The immutable `PublicResourceResult` records the last response's status, URL, scheme, host,
headers, cookies, content type, byte length, TLS verification, any authentication challenge, and
the complete tuple of immutable `RedirectHop(status, from_url, to_url)` records. It contains no
compliance verdict. Redirects 301/302/303/307/308 are resolved explicitly; a missing/malformed
Location is a protocol error. Relative destinations are resolved against the current URL.

An off-host or non-HTTPS redirect is recorded but not followed: `stopped_at_redirect=True`,
`final_url` still identifies the last response actually received, and the attempted destination
appears in the final redirect hop. Callers must inspect both the response and redirect observations.
`WebResourceConnectionError`, `WebResourceProtocolError`, and `WebResourceRedirectError` derive
from `WebResourceError` / `InvalidSetup`. Their optional `observation` retains the last complete
response and known redirects when later transport/protocol/loop/overflow failures prevent completion.
This lets runners preserve earlier failed checks while reporting an overall ERROR. Transient HTTP
status classification and all accessibility assertions remain the runner's responsibility.

### Serial discovery and capture

`SerialPort(device, vid, pid)` and `SerialVersions(esptool, pyserial, esptool_source=...)` are
immutable records. The optional source field preserves two-argument construction.
`preflight_serial(importer=..., find_executable=..., run_command=...)` first tries importing
esptool in the running Python environment. If that module is absent, it searches `PATH` for
`esptool`, then `esptool.py`, and runs the resolved executable with only `version`, a 10-second
timeout, captured output, and no shell. This supports ESP-IDF's standalone script even when its
directory is not on Python's import path. It records the last output line as the version and the
executable path as its source; imported packages are labeled `Python module: esptool`.
Missing dependencies inside an installed esptool are not hidden by this fallback. Failed, timed-out,
or empty-output version commands remain setup errors. Pyserial must still import in the running
Python environment because discovery and capture use its API directly. Module imports, executable
lookup, and subprocess execution are injectable. Preflight never enumerates or resets hardware.
Imports are deferred until use, so importing the helper never touches a DUT.
`discover_serial_port(enumerate_fn=...)` selects exactly one bridge with `CH340_VID = 0x1A86`;
zero or multiple matches raise `InvalidSetup` listing the matches and all enumerated ports.
The default enumerator uses `serial.tools.list_ports.comports()`.

`SerialTransport` injects enumeration, serial opening, preflight, monotonic time, subprocess execution,
and the Python executable. The legacy `sleep` argument is retained but no longer used.
Its `preflight()` caches the tool selection and `discover()` exposes port discovery.
`capture(port, evidence, duration=30.0, *, stop_when=None)` first invokes the preflight-selected tool with
`--port <discovered-port> --before default_reset --after hard_reset read_mac`.
An imported package runs as `[sys.executable, "-m", "esptool", ...]`; a PATH tool runs by its resolved
path, including ESP-IDF's `esptool.py`. The subprocess has a 20-second timeout and no shell.
Evidence records the command before execution and `SerialCommandResult` (return code, stdout,
stderr) afterward, including partial output on timeout. Execution failure prevents UART capture.
This command enters the downloader to read the MAC, then hard-resets into the application; it
does not flash, erase, or write partitions. Only after the tool exits successfully does capture
open UART at `UART_BAUD = 115200` and read immediately, without an extra reset, sleep, or input flush.
The optional `stop_when: Callable[[str], bool]` receives all decoded text accumulated after each
read; returning true stops capture immediately, with no fixed wait afterward. Without a callback,
capture reads until the duration expires. Callers choose the duration and completion predicate for
their required observations. A predicate that parses lines should require complete lines so a
partial serial chunk cannot truncate a value.
The prepared bench must support esptool reset wiring. The window must be finite and positive; each
read timeout is at most 250 ms and is capped to the remaining window. The raw captured text is recorded even after
a partial read failure, and the port closes on success or failure. Exceptions propagate to the
runner for ERROR classification. Banner interpretation and all compliance assertions stay in runners.

The capture duration bounds UART reads, not the entire reset-and-capture operation. A successful
esptool process is only an acquisition prerequisite: callers still need to validate the captured
output against their required observations. Increasing the read window does not resolve a chip
waiting for a download command. Keep reset and port handoff behavior covered by offline tests.

`SerialCommandResult(return_code, stdout, stderr)` is immutable. On launch/timeout errors the
return code can be `None`; partial byte output is decoded with `backslashreplace`. The esptool
command may report uploading a RAM stub, which is not a partition flash operation. Its `read_mac`
output describes the ESP32 MAC; it must not replace the HTTP `gw_mac` identity gate, whose value
comes from the nRF52 address in `gw_cfg_json_add_items_device_info()`.

`SerialTransport` returns captured text without interpreting banners or comparing versions.
Runners implement the parsing and verdict rules defined by their task specifications. Direct
early-stop, cumulative-buffer, deadline, callback-error, and port-cleanup tests belong in
`test_lib.py`; observation parsing and comparison regressions belong in the paired runner tests.

`open_serial(port, baud, timeout)` initializes inactive DTR/RTS before opening.
Both default port enumeration and serial opening accept an optional injected module importer.
`PortInfo` and `SerialConnection` describe the narrow injectable port and serial interfaces.
Direct helper tests are in `test_lib.py`; no live hardware is needed for library validation.

### HTTP inventory and errors

- `GatewayApi`, `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` centralize endpoint
  paths and HTTP vocabulary in `http_api.py`.
  `HttpStatus.C_404_NOT_FOUND` represents unavailable routes; runners decide where it is expected
  (for example, hotspot-only `/info.json` on LAN after authorization succeeds). Authentication
  runs before dispatch: in default mode, missing/Basic/Digest credentials without an authorized
  cookie produce 302; a rejected bearer produces 401. Neither denial reaches the route
  availability check, so 404 is not a substitute for an authentication-denial status.
- `ApiRoute`, `API_INVENTORY`, and `EXPECTED_API_INVENTORY` define the canonical 26-route firmware
  API matrix.
- `InvalidSetup` is the common setup-error base class. `InvalidConfig`,
  `GatewayConnectionError`, `GatewayProtocolError`, and `GatewayAuthenticationModeError` provide
  more specific failure categories.

## Usage

Run examples from `cra/303645/tests` so the `lib` package is importable.

Load the DUT configuration:

```python
from pathlib import Path

from lib.config import load_dut_config

config = load_dut_config(Path(".env"))
print(config.base_url)
```

Create and finish an evidence log:

```python
from pathlib import Path

from lib.evidence import AssertionEvidence, EvidenceLog

log = EvidenceLog.create(Path("logs"), "test_example")
try:
    log.write("ASSERTION", AssertionEvidence("gateway responded", "PASS", 200))
finally:
    log.finish("PASS")
```

Use the gateway client with an interactive login:

```python
from pathlib import Path

from lib.config import load_dut_config
from lib.evidence import EvidenceLog
from lib.gateway import GatewayClient

config = load_dut_config(Path(".env"))
log = EvidenceLog.create(Path("logs"), "interactive_login")
verdict = "ERROR"
try:
    client = GatewayClient(config, log)
    result = client.authenticate_interactive("Admin", config.gw_id)
    verdict = "PASS" if result.login_response.status_code == 200 else "FAIL"
finally:
    log.finish(verdict)
```

## Unit tests

Always run `cra/303645/tests/.venv/bin/ruff check cra/303645/tests` from the repository root before
changes and before handoff, even for library documentation changes. Fix all findings and require a
clean final check; see [`../AGENTS.md`](../AGENTS.md) for the annotation and exception-handling
conventions. Ruff does not replace the dedicated unit tests and coverage gate below.

The root command automatically discovers `cra/303645/tests/pyproject.toml`. To select
it explicitly from the repository root while preserving relative settings, use
`(cd cra/303645/tests && .venv/bin/ruff check --config pyproject.toml .)`.
See [Required linting](../README.md#required-linting) for working-directory semantics
and configuration-discovery verification.

The dedicated library test module is [`../test_lib.py`](../test_lib.py). Locating it outside `lib/`
is intentional and follows normal Python project structure: `lib/` contains reusable runtime code,
while its tests live in the enclosing test project. This also prevents test helpers from becoming
part of the library API.

Set up the Python 3.8 environment and run the library tests:

```bash
cd cra/303645/tests
python3.8 -m venv .venv
.venv/bin/python -m pip install --requirement requirements.txt
.venv/bin/python -m unittest --verbose test_lib.py
```

The unit tests are deterministic and do not contact a DUT or the network. HTTP sessions, random
bytes, clocks, and protocol responses are injected or faked.

## Code coverage

Measure statement and branch coverage for `lib/` using only its dedicated unit tests:

```bash
cd cra/303645/tests
.venv/bin/python -m coverage erase
.venv/bin/python -m coverage run --branch --source=lib -m unittest test_lib.py
.venv/bin/python -m coverage report --show-missing
```

Generate a browsable HTML report when investigating missed lines or branches:

```bash
.venv/bin/python -m coverage html
```

Open `htmlcov/index.html` locally. The report artifacts `.coverage` and `htmlcov/` should not be
committed.

The GitHub Actions workflow [`../../../../.github/workflows/cra-python-tests.yml`](../../../../.github/workflows/cra-python-tests.yml)
runs branch coverage and applies `coverage report --fail-under=95`. `coverage.py` exits with a
nonzero status if total coverage is below 95%, so the workflow job and its required status check
fail. The current dedicated library suite reports 97% total branch-aware coverage.

The threshold is a guardrail rather than a substitute for useful assertions. New tests should
exercise behavior and failure handling rather than adding calls solely to execute lines.

## Extending the library

Keep additions reusable across more than one functional test. Preserve backward compatibility for
existing scripts, use dependency injection for network, randomness, clocks, or subprocesses, and
add direct library tests only to `test_lib.py`. Scenario-specific orchestration and compliance
assertions belong in the corresponding `test_<id>.py` and `test_test_<id>.py` pair.
