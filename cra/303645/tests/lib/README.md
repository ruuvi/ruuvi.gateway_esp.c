# CRA Functional-Test Library

This package contains shared infrastructure for the Python functional tests in
[`cra/303645/tests`](../). Its purpose is to keep configuration parsing, evidence logging, HTTP
protocol handling, authentication, and common data models consistent across live-DUT tests.

The package supports Python 3.8. It is internal to this repository rather than a separately
published Python package. It is governed by the subtree instructions in
[`../AGENTS.md`](../AGENTS.md); a second `AGENTS.md` is intentionally unnecessary here.

## Architecture

The package is split by responsibility:

| Module | Responsibility |
|---|---|
| `config.py` | Strict `.env` and generated default-configuration loading and validation |
| `errors.py` | Shared setup, connection, protocol, and authentication-mode exceptions |
| `evidence.py` | Exclusive timestamped evidence logs, structured records, HTTP transcripts, exceptions, and final verdicts |
| `gateway.py` | Gateway HTTP client, authentication modes, Basic and Digest authorization, and interactive ECDH authentication |
| `http_api.py` | Gateway endpoint names, canonical HTTP methods, statuses, headers, schemes, and the firmware API route inventory |
| `models.py` | DUT configuration, test results, and console progress reporting |

`GatewayClient` is the main runtime boundary. It prepares requests, writes request and response
evidence, translates `requests` transport failures into library exceptions, and implements the
gateway authentication flows. Functional-test scripts build their test-specific assertions and
state transitions on top of this client.

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

### Models

- `DutConfig` stores the gateway ID, MAC address, and hostname. Its `base_url` property formats DNS,
  IPv4, and bracketed IPv6 URLs.
- `RunResult` carries the process exit code, verdict, per-check outcomes, route coverage, and an
  optional recovery message.
- `ProgressReporter` emits numbered progress messages through an injected output callback.

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
- `GatewayClient.request()` sends a request with fixed timeouts and redirects disabled by default.
  It accepts either `json_body` or `data`, but never both.
- `response_json()` validates JSON decoding and, optionally, the top-level Python type.
- `authorization_header_basic()` and `authorization_header_digest()` construct authorization
  headers. `calculate_digest_ha1()` exposes the shared HA1 calculation.
- `parse_interactive_challenge()` and `parse_digest_challenge()` validate authentication challenge
  parameters.
- `request_interactive_challenge()`, `submit_interactive_authentication()`, and
  `authenticate_interactive()` implement the gateway interactive authentication sequence and ECDH
  key derivation.

### HTTP inventory and errors

- `GatewayApi`, `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme` centralize endpoint
  paths and HTTP vocabulary in `http_api.py`.
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
