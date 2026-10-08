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
| `flash_dut.py`   | Injectable esptool flash reads/writes and strict ESP32 partition-table and OTA metadata decoding                 |

`GatewayClient` is the main runtime boundary. It prepares requests, writes request and response
evidence, translates `requests` transport failures into library exceptions, and implements the
gateway authentication flows. Functional-test scripts build their test-specific assertions and
state transitions on top of this client.

This README describes reusable API contracts and helper behavior. Concrete procedures, capture
deadlines, expected observations, verdict rules, and live-run findings belong in the matching
`cra/303645/tasks/task_<id>.md` specification in the [task directory](../../tasks).

## Public API

### Configuration

- `load_dut_config(path)` requires `gw_id`, `gw_mac`, and `gw_hostname` in a local `.env` file and
  accepts optional `gw_fw` as a full `vX.Y.Z-dev` or `vX.Y.Z-prod` release tag. Empty `gw_fw`
  becomes `None`. `gw_mac` is the nRF52 identity MAC, not an ESP32 network-interface MAC.
  Optional `wifi_ssid` and `wifi_password` must be supplied together: 1–32 and 8–63 UTF-8
  bytes respectively, without NUL characters. Their values after `=` are literal, preserving
  whitespace, `#`, and `=` without quote handling or expansion. Absent fields become `None`;
  runners decide whether the pair is required. Unknown, duplicate, missing required, and
  malformed values raise `InvalidConfig`. Errors do not echo credential values.
- `validate_hostname(value)` accepts DNS names, IPv4 addresses, and IPv6 addresses without a URL
  scheme, port, path, credentials, or surrounding whitespace.
- `load_ui_default_config(path=...)` reads the generated gateway UI defaults as a JSON object.
  Omit the path (or pass `None`) to prefer the repository-root
  `gw_cfg_default/gw_cfg_default_gen_ui.json`, then fall back to
  `gw_cfg_default_gen_ui.json` beside the test scripts, one directory above `lib/`.
  This supports standalone/Windows copies without depending on the current working
  directory. Copy the portable reference from the matching firmware checkout;
  defaults are never inferred from live DUT state. An explicit path never falls
  back, and an existing malformed repository reference is an error even if a
  portable copy exists. Missing automatic candidates report where to copy the file.
- `default_config_values(fields, path=...)` selects required fields from those defaults and rejects
  missing fields, using the same path-selection rules.
- `FACTORY_RESET_MESSAGE` and `AUTHENTICATION_DEFAULT_FIELDS` define shared functional-test policy.
  The recovery message requires the red LED completion signal (200 ms on/200 ms off) after boot-time
  erasure, warns about lost local settings, and treats about 11 seconds as typical elapsed time.
  Releasing CONFIGURE after this signal triggers another restart, after which the configuration
  hotspot opens.
  Runners decide when reset advice is appropriate and include it in evidence and terminal output.

### Models

- `DutConfig` stores the gateway ID, nRF52 MAC, and hostname. Its `base_url` property formats DNS,
  IPv4, and bracketed IPv6 URLs. Optional `gw_fw`, `wifi_ssid`, and `wifi_password` follow
  the original three fields; `wifi_password` is excluded from the dataclass representation.
- `RunResult` carries the process exit code, verdict, per-check outcomes, route coverage, and an
  optional recovery message.
- `ProgressReporter` emits numbered progress messages through an injected output callback.
  It counts phases, not result explanations. Runners should use a separate output callback for
  concise PASS/FAIL/ERROR reasons and mirror those lines with `EvidenceLog.write_line()`; neither
  `RunResult` nor `EvidenceLog.exception()` automatically prints a diagnostic to the terminal.

Ethernet and Wi-Fi use their own ESP32 MACs, which are absent from `.env`. A packet-capture caller
must identify the active link MAC from the verified DUT IP and an on-link ARP/neighbor observation
or equivalent trusted capture context; it must not substitute `DutConfig.gw_mac`. A future ESP32
Bluetooth interface must be identified separately as well. See the
[DUT configuration guidance](../README.md#dut-configuration) for the DHCP capture precondition.

### Evidence

- `EvidenceLog.create(log_dir, filename_prefix, now=...)` creates a timestamped file using exclusive
  creation. A numeric suffix prevents collisions.
- `write()` and `write_line()` record structured or plain evidence and flush immediately.
  A top-level `DutConfig` passed to `write()` has its Wi-Fi password redacted in metadata.
  HTTP transcripts and runner-specific manifests retain their existing complete-evidence contract;
  they can contain provisioned Wi-Fi credentials.
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
  `GatewayCfgDesc` also names Wi-Fi credential fields, the storage-presence mapping, and
  HTTP/MQTT/remote-config target and authentication fields. `GatewayStorageFile` names the
  HTTP/MQTT/statistics/remote-config client certificate/key files and HTTP/MQTT server
  certificates used by data-creation tests. These are protocol descriptors, not a route
  inventory or a promise that stored contents can be read: current TLS storage uses NVS
  strings, for which `GET /extra_cfg` returns 403; callers can inspect `storage.<filename>`.
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

- `build_encrypted_json(aes_key, payload, iv_generate=secrets.token_bytes)` returns an immutable
  `EncryptedJsonEnvelope` containing the exact UTF-8 plaintext and the Base64 `encrypted`, `iv`,
  and `hash` fields. It uses compact JSON, AES-256-CBC/PKCS7 and SHA-256 of the plaintext, matching
  Web-UI CryptoJS. The key must have 32 bytes and the injected IV generator must return 16 bytes.
  Nonfinite JSON numbers are rejected. `json_body()` returns only the three wire-envelope fields.
- `post_encrypted_json(session, path, envelope)` logs the envelope and sends it with
  `Ruuvi-Ecdh-Encrypted: true`, using normal request evidence, timeouts and disabled redirects.
  Callers own endpoint selection, authorization, mutation scope, acceptance checks and restoration.
- `calculate_interactive_response(username, password, challenge)` returns immutable
  `AuthCalculationEvidence` with HA1 input/output and the challenge-bound response. The login builder
  uses this calculation. Complete login results expose it as optional `calculation` for compatibility
  with existing manually constructed results.
- Parsed challenges and complete login results expose optional immutable `EcdhEvidence` through
  `ecdh`: both uncompressed P-256 public keys, the 32-byte shared X coordinate, and its SHA-256 AES
  key. Existing constructors remain valid. Live client results include these observations.
- `captured_requests` retains immutable `CapturedRequest` snapshots of method, URL, header pairs
  and body bytes before sending, including attempts whose response is lost. Mutable `bytearray`
  bodies are copied into immutable bytes. A body of `b""` means
  no request body; `None` marks an unsupported streaming body which is left unconsumed to preserve
  existing transport behavior. Evidence-dependent callers must reject incomplete captures.
  Capture includes all requests through that client, including recovery; it does not inspect a
  browser or make any TLS claim. These records contain secrets and belong only in local evidence.

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

### LAN scan acquisition

`lib.netscan` provides verdict-free network acquisition for full-range interface tests.
`resolve_host()` selects the first IPv4 address and retains all resolved addresses; failures are
`ScanError` setup errors. `check_nmap()` checks executable presence and a parseable minimum version,
then probes Nmap's own UDP privilege with a bounded one-port loopback scan. A capability on Python
alone does not transfer to the Nmap child process. Its executable lookup, subprocess, and privilege
check are injectable.

`run_tcp_connect_scan()` uses nmap `-sT -sV`; `run_udp_scan()` uses `--privileged -sU -sV` with
`--defeat-icmp-ratelimit`. Both accept an optional per-host `scan_delay_ms` for Nmap
`--scan-delay`. Host timeout and scan delay must be positive integers; retries must be a
non-negative integer. Invalid limits, including non-finite numbers, raise `ScanError` before
creating temporary files or launching Nmap. Both scans use an outer subprocess timeout,
preserve a nonzero Nmap exit diagnostic even when XML is
absent, and parse completed nmap XML. `ScanResult` is immutable and
retains the exact command, raw XML, elapsed time, confirmed-open ports, state counts, explicit port
observations, and typed `ExtraportsGroup` records. Nmap can compress multiple closed, filtered,
`open|filtered`, or `closed|filtered` groups without listing their individual port IDs. In that case, the corresponding
port sets contain only explicitly listed ports; counts still cover the full requested range.
The optional `enumerate_ports=True` mode adds Nmap `-d3` so indeterminate port IDs remain available
for coverage evidence; it rejects a result that still compresses an indeterminate group. Optional
`defeat_icmp_ratelimit` and `service_detection` settings let callers choose scan behavior without
changing the default scan command.
With `--defeat-icmp-ratelimit`, Nmap labels nonresponsive UDP ports `closed|filtered`; this is as
indeterminate for listener absence as `open|filtered` and never a confirmed closed port. Incomplete
port coverage, timeout, malformed XML, and missing UDP
privilege raise `ScanError`. The subprocess and monotonic clock are injectable.

`probe_mdns()` sends a direct DNS-SD PTR query for `_services._dns-sd._udp.local` to UDP 5353. It
returns a typed answer only when the requested IP replies from UDP 5353 and advertises `_http._tcp`.
Timeout returns `None`, which callers must interpret against their scan coverage. Non-finite or
nonpositive timeouts raise `ScanError` before opening a socket; malformed non-ASCII DNS labels
also raise `ScanError`. Its socket factory is injectable.
The library does not map findings to IXIT entries or decide a case verdict. Direct helper contracts
are tested in `test_lib.py` without launching nmap or contacting the network.

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
`--port <discovered-port> --baud 460800 --before default_reset --after hard_reset read_mac`.
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

`SerialTransport.reset(port, evidence)` exposes the same bounded esptool reset as a standalone
operation for callers that need another observer, such as `tcpdump`, listening before reboot. It
records the command and result without opening UART or capturing console output. Discover the
single CH340 port and verify DUT identity before using it; its `read_mac` output is an ESP32 MAC,
not `.env` `gw_mac`.

The capture duration bounds UART reads, not the entire reset-and-capture operation. A successful
esptool process is only an acquisition prerequisite: callers still need to validate the captured
output against their required observations. Increasing the read window does not resolve a chip
waiting for a download command. Keep reset and port handoff behavior covered by offline tests.

`SerialCommandResult(return_code, stdout, stderr)` is immutable. On launch/timeout errors the
return code can be `None`; partial byte output is decoded with `backslashreplace`. The esptool
command may report uploading a RAM stub, which is not a partition flash operation. Its `read_mac`
output describes the ESP32 MAC; it must not replace the HTTP `gw_mac` identity gate, whose value
comes from the nRF52 address in `gw_cfg_json_add_items_device_info()`.

`SerialTransport.observe(port, evidence, duration=30.0, *, stop_when=None)` opens UART immediately
after a caller-controlled reset, reads for a finite duration or until `stop_when` accepts the
cumulative text, records the raw console, and closes the port. `capture()` delegates to this method
after resetting. Callers can use `observe()` while a separate packet capture remains active
to correlate a task-defined startup marker with network traffic; the task owns the marker,
capture deadline and interpretation.

`SerialTransport` returns captured text without interpreting banners or comparing versions.
Runners implement the parsing and verdict rules defined by their task specifications. Direct
early-stop, cumulative-buffer, deadline, callback-error, and port-cleanup tests belong in
`test_lib.py`; observation parsing and comparison regressions belong in the paired runner tests.

### ESP32 flash access

`flash_dut.FlashTool` uses the serial preflight's selected esptool source and CH340 port. Its
subprocess runner is injectable. Every `chip_id`, `flash_id`, `read_flash`, and `write_flash`
command explicitly uses `--baud 460800`. UART boot capture remains at 115200 baud. Reads use `--before default_reset --after hard_reset`;
writes use `--after no_reset` so
the caller can attach UART capture before an explicit hard reset. Every command has a finite
timeout and evidence of the exact command and result. Each flash read command has a 30-second
timeout; write timeouts scale with transfer size. `read()` assembles a partition from complete
64 KiB esptool reads. On corrupt data, a short read, or a read timeout, it retries at 115200 baud.
After a failed or short read, it resumes one 4 KiB sector before the last saved boundary and checks
overlapping bytes. It permits three failures without new progress, clears that count when new bytes
are saved, and writes the final backup only after the requested byte count is complete;
`write()` requires complete 4 KiB aligned sectors because
esptool erases flash by sector. Both methods reject negative offsets, and `read()` requires a
positive size. Invalid ranges raise `InvalidSetup` before touching files or invoking esptool.
Policy about permitted partitions remains with the caller.
After all read attempts fail, `read()` makes a best-effort `read_mac` hard reset so a fatal
esptool exit does not leave the gateway in download mode. Reset failure is reported separately.

`parse_partition_table()` validates the ESP-IDF MD5 record when present and converts the
on-device binary table to immutable `Partition` records, rejecting non-ASCII names as `InvalidSetup`;
`parse_partition_csv()` does the same for the repository layout. Callers compare the full tuples
before writing. `decode_otadata()` validates both OTA selection entries' sequence, state, and
ESP-IDF seeded CRC (`zlib.crc32(sequence_bytes, 0xFFFFFFFF)`),
then returns the unambiguous active slot or raises `InvalidSetup`. Direct parser and tool tests
live in `test_lib.py`.

`open_serial(port, baud, timeout)` initializes inactive DTR/RTS before opening.
Both default port enumeration and serial opening accept an optional injected module importer.
`PortInfo` and `SerialConnection` describe the narrow injectable port and serial interfaces.
Direct helper tests are in `test_lib.py`; no live hardware is needed for library validation.

### HTTP inventory and errors

- `GatewayApi`, `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` centralize endpoint
  paths and HTTP vocabulary in `http_api.py`.
  Named statuses cover all common firmware responses documented in `docs/http_api.md`, including
  400, 409, 502, 503, and 504.
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
