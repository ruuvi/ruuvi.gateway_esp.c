# AGENTS.md — CRA Python Functional Tests

## Scope

These instructions apply to `cra/303645/tests/` and every directory below it, including `lib/`.
One file is intentionally used for the whole subtree so test-runner and shared-library rules remain
consistent. Add a deeper `AGENTS.md` only if a future subdirectory needs materially different
instructions.

Read these references before changing this subtree:

- [`README.md`](README.md) for setup, test execution, live-DUT behavior, coverage, and CI.
- [`lib/README.md`](lib/README.md) for the shared library architecture and public API.
- The matching task specification under `cra/303645/tasks/` when creating or changing a CRA test.

## Purpose and structure

This is a Python 3.8 project for automating selected ETSI EN 303 645 / ETSI TS 103 701 tests against
a dedicated Ruuvi Gateway.

- `test_<id>.py` is a live-DUT functional-test executable.
- `test_test_<id>.py` is the deterministic host-side implementation test for that executable.
- `test_lib.py` is the only direct unit-test module for `lib/`.
- `lib/` contains reusable runtime infrastructure shared by live tests.
- `.env` contains local DUT configuration.
- `logs/` contains generated functional-test evidence.

Do not confuse this project with the C/C++ firmware unit tests in the repository-root `tests/`
directory. It has separate dependencies, commands, and CI.

## Environment

Use Python 3.8. Set up the environment from this directory:

```bash
python3.8 -m venv .venv
.venv/bin/python -m pip install --requirement requirements.txt
```

Declare required third-party packages in `requirements.txt`. Prefer the standard library and
existing dependencies. Keep new dependencies compatible with Python 3.8.

## Shared-library rules

- Put behavior in `lib/` only when it is reusable across functional tests. Keep case-specific
  assertions, outcome policy, sequencing, and recovery in the relevant `test_<id>.py`.
- Treat existing names and behavior in `lib/` as a public API for this test project. Preserve
  backward compatibility unless all callers are deliberately migrated.
- Put direct tests of `lib/` only in `test_lib.py`. Do not duplicate tests for configuration
  parsing, evidence serialization, HTTP construction, challenge parsing, authentication helpers,
  models, errors, or API inventory in `test_test_<id>.py`.
- A `test_test_<id>.py` module may use library classes and constants as infrastructure and may test
  how its functional runner responds to library results or exceptions. Assertions about the
  library itself belong in `test_lib.py`.
- Keep `test_lib.py` outside `lib/`. Runtime modules must not include test-only helpers or fixtures.
- Inject external boundaries such as HTTP sessions, random bytes, clocks, key generation, serial
  access, DNS, and subprocess execution. Library unit tests must be deterministic and offline.
- Update `lib/README.md` when the architecture, public API, dependencies, behavior, or examples
  change.

## Functional-test rules

- Live scripts must run from `cra/303645/tests` with no command-line arguments unless the governing
  task specification explicitly says otherwise.
- Read DUT identity and address through `lib.config.load_dut_config`; do not parse `.env` again in a
  test script.
- Use `GatewayClient` for gateway HTTP and authentication instead of duplicating protocol code.
- Use `EvidenceLog` and structured evidence dataclasses. Log the HTTP request before transmission
  and the response before interpreting it.
- Preserve the result convention: `0` for PASS, `1` for a test assertion FAIL, and `2` for setup,
  transport, protocol, or recovery ERROR.
- Verify restoration in any test that changes DUT state. Attempt restoration after failures and
  exceptions, and never report PASS when restoration cannot be verified.
- Report factory-reset requirements with `FACTORY_RESET_MESSAGE` where the initial or recovered
  authentication state is unsuitable.
- Do not run live-DUT scripts during normal automated validation. They require an explicitly
  configured gateway and can mutate its configuration.
- Do not alter or commit local `.env` values or generated `logs/`. Evidence may contain credentials,
  authentication material, configuration values, and HTTP bodies.

## Host-side implementation tests

- Use `unittest`, matching the existing modules.
- Fake all network and hardware boundaries. A host-side test must never contact a real DUT.
- Test the automation's behavior: exact request matrices and bodies, status handling, ordering,
  timing policy, recovery, restoration, evidence integration, progress, and exit-code aggregation.
- Make fixture expectations independent of the production helper where practical. For example,
  calculate an expected digest independently instead of calling the same method under test.
- Keep fixtures small enough to make the tested behavior visible. Reuse a local fake only when it
  remains clear which state transitions and failures it models.
- Host-side implementation tests are not compliance evidence. Do not describe them as proving ETSI
  conformance.

## Required validation

After changing `lib/` or `test_lib.py`, run the dedicated library suite with its branch-coverage
gate:

```bash
.venv/bin/python -m coverage erase
.venv/bin/python -m coverage run --branch --source=lib -m unittest --verbose test_lib.py
.venv/bin/python -m coverage report --show-missing --fail-under=95
```

After changing any functional runner, implementation test, or shared library, run every host-side
automation test:

```bash
.venv/bin/python -m unittest discover \
  --verbose \
  --start-directory . \
  --pattern "test_test_*.py"
```

Also run `git diff --check`. Ensure new Python syntax is valid on Python 3.8. Do not satisfy the
coverage threshold with assertions-free calls or tests that only mirror implementation details;
cover meaningful success, rejection, and failure behavior.

The workflow `.github/workflows/cra-python-tests.yml` runs these two suites on Ubuntu 22.04. Keep
local commands and workflow commands aligned. The library coverage measurement must use
`test_lib.py` alone so incidental execution by functional-runner tests cannot inflate it.

## Change checklist

When adding a CRA test case:

1. Read its specification under `cra/303645/tasks/` and relevant assessment/IXIT sources.
2. Add `test_<id>.py` for live-DUT execution.
3. Add `test_test_<id>.py` for deterministic implementation verification.
4. Extend `lib/` only for generally reusable behavior and add its direct tests to `test_lib.py`.
5. Update `requirements.txt` only when necessary.
6. Update the applicable README when commands, structure, public API, or operational expectations
   change.
7. Run library coverage and the complete `test_test_*.py` suite.
