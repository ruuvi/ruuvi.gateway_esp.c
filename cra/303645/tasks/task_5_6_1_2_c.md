# Task: Automate ETSI Test Case 5.6-1-2, Unit c

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

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.6-1-2, Test Unit
c: Undocumented Logical Interfaces (Network Scanning)**.

The test must run against a physical Ruuvi Gateway and prove, by active network discovery, that in
the **initialized operational state** the DUT exposes **only the logical network interfaces declared
as enabled in [`IXIT 28-LogIntf`](../ixit_28-LogIntf.md)** — the Port 80 HTTP server and,
after the required separate IXIT declaration update, the Port 5353 mDNS responder — and that **every interface declared _Disabled_ or _Transient_ is genuinely not
listening**. Every open socket found by the scan must map to an _enabled_ IXIT 28 entry; every open
socket without such an entry, and any transient/disabled interface that is unexpectedly active, must
be reported as a finding.

Use [`cra/303645/tests/lib`](../tests/lib/README.md) as the primary implementation reference. Follow
the structure, execution contract, and user experience of the existing tasks
[`task_5_1_5_2_b.md`](task_5_1_5_2_b.md) and [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md) and reuse the
shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md) rather than duplicating
configuration, logging, model, result, or error-classification code. Treat `nmap`, firmware, and
IXIT sources as behavioral references rather than implementation templates.

## Relationship to test 5.5-5-2 Unit B (shared scan helper, distinct normative units)

This unit and **5.5-5-2 Unit B** ([`task_5_5_5_2_b.md`](task_5_5_5_2_b.md)) both rely on full-range
network port scanning and comparison against `IXIT 28-LogIntf`, and both are permitted to share one
scanning helper. **They are distinct normative units and must remain two separate test scripts with
their own verdicts and evidence logs.** Do not merge them.

- **This unit (5.6-1-2 c)** belongs to provision **5.6-1** (_Unused Network and Logical Interfaces
  Are Disabled_). Its focus is **minimization and disablement**: the scan confirms that no
  undocumented logical interface exists and that interfaces IXIT 28 marks _Disabled_
  (`LogIntf-Custom-HTTP-Telemetry`, `LogIntf-Custom-Stream-Telemetry`) or _Transient/On-demand_
  (`LogIntf-Hotspot-DHCP-Server`, `LogIntf-Hotspot-DNS-Server`, `LogIntf-WiFi-WPS-Client`) are **not
  listening** in the initialized, non-hotspot state. It is paired with 5.6-1-2 Unit a (interface
  status matching, HYBRID) and Unit b (undocumented _physical_ interfaces, MANUAL) — those are out
  of scope here. This unit's comparison set is the full **enabled** IXIT 28 set, with
  `IXIT 15-PhyIntf` as physical-interface context. Source:
  [`Self_Assessment_Test_Group_5_6-1.md`](../Self_Assessment_Test_Group_5_6-1.md), test case 5.6-1-2
  (lines 57-79), Unit c row (line 72), summary line 89.
- **5.5-5-2 Unit B** belongs to provision **5.5-5** (_Auth/Authz for security-relevant configuration
  functions_). Its focus is that **no undocumented listener can carry a security-relevant
  configuration/management function**, cross-referencing `IXIT 13-SoftServ`. Same scanning
  mechanism, different normative question and comparison model.

Implement a shared, backward-compatible helper (see _Shared scanning helper_). Whichever task is
implemented first creates `lib/netscan.py`; the second reuses it without changing its behavior. Each
test keeps its own comparison model, assertions, verdict aggregation, and host-side tests. Do not
let one unit's expectations leak into the other's runner.

## Deliverables

1. Create `cra/303645/tests/test_5_6_1_2_c.py`, compatible with Python 3.8, taking **no command-line
   arguments**.
2. Create `cra/303645/tests/test_test_5_6_1_2_c.py` with deterministic host-side implementation
   tests. These mock-based tests are **not** ETSI compliance evidence.
3. Add or reuse the shared scanning helper `cra/303645/tests/lib/netscan.py` with injectable
   subprocess/resolver/mDNS/clock so host tests never invoke real `nmap`, DNS, or mDNS. Extend
   `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is required.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. `nmap` is a
   system binary, not a pip package.

Do not modify firmware behavior or any self-assessment/IXIT source document.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_6-1.md`](../Self_Assessment_Test_Group_5_6-1.md), test
case 5.6-1-2, **Test Unit c** (line 72) calls for comprehensive port scanning
(`nmap -p 1-65535 -sV -sU`) across Ethernet and Wi-Fi. The documented initialized listeners are
TCP/80 (`LogIntf-HTTP-Server`) and UDP/5353 (`LogIntf-mDNS-Responder`); no SSH, Telnet, or
diagnostic listener is expected.

The [automation-feasibility assessment](../Functional_Test_Automation_Assessment.md) classifies this
unit **AUTO**: full-range scanning and comparison to IXIT 28 are scriptable, with outbound traffic
correlation needed to identify the dynamic SNTP source port. Implement it as an objective
scan-and-compare with no manual/subjective step.

### Expected network-surface model (initialized, non-hotspot state)

Build the expected model programmatically and record it in the evidence log:

| Transport  | Port(s)                          | IXIT 28 declaration                                                                                                                                                                | Expectation on LAN, initialized state                                                                               |
| :--------- | :------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------ |
| TCP        | 80                               | `LogIntf-HTTP-Server` — **Enabled** ([IXIT 28](../ixit_28-LogIntf.md))                                                                                                             | **Open** — documented HTTP listener                                                                                 |
| TCP        | all others                       | none enabled                                                                                                                                                                       | **Closed** — any other open TCP port (incl. SSH/Telnet/diagnostic) → FAIL                                           |
| UDP        | 53/67/68                         | DNS/DHCP **clients** — outbound only ([IXIT 28](../ixit_28-LogIntf.md))                                                                                                            | Remote service ports; no persistent inbound server socket                                                           |
| UDP        | dynamic source 49152-65535 → 123 | SNTP **client** — outbound request to server UDP/123 ([IXIT 28](../ixit_28-LogIntf.md))                                                                                            | The local source port is boot-randomized, may change after reboot or SNTP restart, and is not an inbound NTP server |
| UDP        | 53/67/68                         | Hotspot DHCP/DNS **servers** — **Transient/On-demand** ([IXIT 28](../ixit_28-LogIntf.md))                                                                                          | **Not listening** (hotspot inactive) → active here would be a FAIL finding                                          |
| Wi-Fi mgmt | —                                | `LogIntf-WiFi-WPS-Client` — **Transient/On-demand** ([IXIT 28](../ixit_28-LogIntf.md))                                                                                             | Not active; out of nmap scope (802.11 mgmt) — note as a coverage limitation                                         |
| Client     | user-config                      | `LogIntf-Custom-HTTP-Telemetry`, `LogIntf-Custom-Stream-Telemetry` — **Disabled by default** ([IXIT 28](../ixit_28-LogIntf.md))                                                    | Outbound clients, disabled — no inbound listener                                                                    |
| UDP        | 5353 (mDNS)                      | `LogIntf-mDNS-Responder` — **Enabled** ([IXIT 28](../ixit_28-LogIntf.md)); firmware `start_mdns()` advertises `_http._tcp` ([`main/main_loop.c`](../../../main/main_loop.c#L485)). | **Open** — documented discovery responder; see _mDNS handling_                                                      |

The distinctive emphasis of **this** unit (vs 5.5-5-2 B) is the **disablement check**: PASS requires
not only "no undocumented open socket" but also positive evidence that the _Transient_/_Disabled_
interfaces are inactive in the initialized state (hotspot DHCP/DNS servers on UDP 53/67/68 not
listening; no custom-telemetry inbound listener). Assert these explicitly from the scan results.

### mDNS handling

As with 5.5-5-2 B: the firmware runs the documented mDNS responder on **UDP 5353**, advertising
`_http._tcp` (`start_mdns()`). The `.local` hostname resolution this suite depends on is served by
that responder. The test must:

- Confirm the responder deterministically via a direct mDNS query to UDP 5353 (definitive positive,
  immune to UDP `open|filtered` ambiguity).
- Record it and compare it to `LogIntf-mDNS-Responder`. A confirmed `_http._tcp` response is a
  **PASS contribution**. Explicitly closed 5353/udp with no direct answer is a **FAIL** for a
  missing enabled interface; an open scan result without a confirming answer, or indeterminate UDP
  state without an answer, is **ERROR**.

## IXIT declaration prerequisite and safe partial writes

The checked-in IXIT 28 currently lists 15 interfaces and does **not** declare
`LogIntf-mDNS-Responder`. The model and mDNS PASS contributions described above are conditional
on a separately reviewed IXIT update declaring that interface. The runner checks the authoritative
IXIT ID heading before scanning or mutation; a missing declaration is setup **ERROR**, never an
implicit allowlist or inventory-completeness PASS. Offline success fixtures explicitly supply the
future declaration; they are not evidence that the current IXIT contains it.

Before the NTP-only POST, require factory-default `mqtt_data_format` from the generated defaults,
and require `mqtt_prefix` = `ruuvi/<verified gw_mac>/` and `mqtt_client_id` = `<verified gw_mac>`.
These last two defaults are device-derived in `gw_cfg_default.c`. Missing, wrong-type or custom
values are setup **ERROR** before mutation, even with MQTT disabled: the firmware resets these
omitted fields on every partial POST. Do not add unrelated MQTT fields to the task's write scope.
Also gate `scan_default` and its PHY/channel defaults: the scan parser resets those omitted
fields when default scanning is enabled. Keep full baseline comparisons after the change and restoration. The fake gateway models these
parser resets and success tests verify preservation of the unrelated fields.

Record each confirmed TCP or UDP finding immediately as FAIL for its affected checks, including
custom telemetry or hotspot disablement. Later acquisition failures retain these FAIL outcomes
while making the overall verdict ERROR. Test both the open-listener and targeted-hotspot loops.
Test the actual tcpdump wrapper offline with mocked process, pipe, readiness and clock boundaries,
including split readiness, early exit, startup/capture timeouts, malformed output and kill cleanup.

## Tooling preconditions and privilege model

Identical to the sibling scan task. `nmap` is a hard external precondition:

1. **Locate** `nmap` (`shutil.which`); absent → **ERROR** (precondition: `apt-get install nmap`).
2. **Version** `>= 7.40` (stable `-oX`, `-sV`, `--defeat-icmp-ratelimit`); parse `nmap --version`;
   older/unparseable → **ERROR**.
3. **Privilege**: TCP connect scan (`-sT`) needs no privilege and is definitive. UDP scan (`-sU`)
   needs `root`/`CAP_NET_RAW`; **unprivileged UDP cannot distinguish `closed` from `filtered`** and
   cannot support a completeness claim. Without privilege, the UDP portion is **ERROR** with a
   documented precondition, **never a silent PASS**.

Do not silently narrow coverage to avoid a privilege or timeout condition; return an explicit ERROR
with the documented precondition instead.

## Scan strategy

Scan the **resolved LAN IP** with `-Pn -n --reason -oX`, parsing `nmap` **XML** in the helper.

### TCP (full range, definitive)

- `nmap -sT -Pn -n --reason -sV -p 1-65535 -oX <file> <ip>`.
- Expected open set: **exactly `{80/tcp}`**, fingerprinting as HTTP. Any other open TCP port (e.g.
  `22/tcp` SSH, `23/tcp` Telnet) → FAIL.
- **False-positive handling:** re-confirm each unexpected open TCP port with an independent probe
  (second `nmap -sT -sV` of that port, or a `socket` connect + short read) before recording a FAIL
  finding.

### UDP (source-declared full range, privileged, honest interpretation)

- Target the **full `1-65535` UDP range** to match the source, using
  `nmap -sU -sV -Pn -n --reason --defeat-icmp-ratelimit -p 1-65535 -oX <file> <ip>` with small,
  documented `--max-retries` and a **finite `--host-timeout`** (named production constant) plus an
  outer subprocess timeout. Requires privilege.
- **If the UDP scan cannot complete within the budget → ERROR** (an incomplete scan is not proof of
  absence). Do not silently reduce the range; any reduced range must be an explicitly logged,
  documented limitation, never a hidden default.
- **Honest interpretation (mandatory):** `nmap` marks non-responding UDP ports `open|filtered`. A
  sweep of `open|filtered` does **not** prove "no UDP service." Only a **confirmed `open`** UDP port
  (ideally with an `-sV` service match) counts as a discovered listener; an undeclared
  confirmed-open UDP port → FAIL. `open|filtered` and `closed|filtered` ports are recorded as
  **indeterminate**, never as evidence that those ports are closed. Unresolved indeterminacy is
  ERROR.
- Before the UDP scan, capture an outbound NTP client request from the verified DUT to the test host
  with `tcpdump`. Start capture before temporarily setting `ntp_server1` to the host IPv4 address;
  read back the change. The captured source port must be the sole `closed|filtered` port in the full
  UDP scan. No fixed port, earlier capture, or server UDP/123 substitutes for this same-run
  correlation. Restore `ntp_server1` and verify the full original configuration afterward, including
  after a lost POST response, capture failure, or scan error.
- **Deterministic positive probe** for the known responder: send a direct mDNS query to UDP 5353 and
  record the answer; feed it to the IXIT 28 comparison.
- **Disablement evidence:** explicitly assert that hotspot DHCP/DNS server ports (UDP 53/67/68) are
  not confirmed-open in the initialized state, and record this as part of the minimization verdict
  (subject to the same `open|filtered` honesty rule — use a targeted deterministic DHCP/DNS server
  probe if a definitive positive/negative is needed, otherwise report indeterminacy rather than
  asserting a false negative).

### Retries and stability

Use small finite `nmap` retries (documented constants); re-confirm any FAIL-triggering open port
with an independent re-probe so a single transient reading cannot flip the verdict.

## Hostname resolution

- Resolve `gw_hostname` (`socket.getaddrinfo`, prefer IPv4); record the address.
- The default `.local` name requires host mDNS (`avahi-daemon` + `libnss-mdns` or equivalent); on
  resolution failure → **ERROR** with a documented precondition (enable mDNS resolution, or set
  `gw_hostname` to an IP literal in `.env`, which `lib.config` accepts).
- On multiple addresses, scan the primary IPv4 and log the set; IPv6-only resolution is a logged
  ERROR/limitation unless an IPv4 is available.
- Run from a host on the **same LAN segment** as the DUT so the LAN listening surface is measured.

## Comparison and verdict

Build the expected model from IXIT 28 (with IXIT 15 as physical context) in code, then compare:

1. **TCP open set == `{80}`**, HTTP-fingerprinted → PASS contribution.
2. **No undeclared confirmed-open TCP port** (explicitly assert no SSH/Telnet/diagnostic) → else
   FAIL finding.
3. **No undeclared confirmed-open UDP port** among scanned ports → else FAIL finding.
4. **Transient/Disabled interfaces inactive**: hotspot DHCP/DNS server ports not confirmed-open; no
   custom-telemetry inbound listener → else FAIL finding (this is the minimization emphasis).
5. **mDNS/5353** recorded and compared to its enabled IXIT entry; confirmed `_http._tcp` responder →
   PASS contribution.
6. **UDP coverage**: the captured SNTP source must be the sole `closed|filtered` port. Any other
   unresolved UDP port is ERROR. A direct mDNS answer or a conclusive targeted hotspot probe can
   resolve `open|filtered` or `filtered` observations. Record the separate 802.11-management WPS
   scope limitation.
7. **NTP restoration**: verify the full final configuration equals the saved baseline; an unverified
   or changed final state is ERROR with operator recovery advice.

### Exit codes

- `0` — PASS: documented listeners and disablement checks passed, the captured SNTP source is the
  sole `closed|filtered` UDP port, and full configuration restoration was verified.
- `1` — FAIL: an undocumented confirmed-open socket, unexpectedly active transient or disabled
  interface, or missing enabled listener was found.
- `2` — ERROR: setup, capture, scan, protocol, or restoration could not be verified.

Missing privilege, incomplete/timed-out scan, uncaptured SNTP source, extra UDP ambiguity, or
unverified NTP restoration must be **ERROR (2)**.

## Shared scanning helper (`lib/netscan.py`)

Provide the same reusable, dependency-injected primitives described in
[`task_5_5_5_2_b.md`](task_5_5_5_2_b.md) (_Shared scanning helper_): `resolve_host`, `check_nmap`,
`run_tcp_connect_scan`, `run_udp_scan`, `probe_mdns`, and a frozen `ScanResult` dataclass parsing
`nmap` **XML**. All external effects (subprocess, DNS, mDNS socket, clock) must be injectable. Keep
the helper free of any single unit's verdict logic; each test's comparison-to-IXIT-28 model and the
disablement assertions live in the test runner. If the sibling task already created the helper,
reuse it unchanged; only add backward-compatible additions if this unit needs them (e.g. a targeted
DHCP/DNS server probe).

## Required shared functional-test architecture

Keep the test runner limited to this unit's expected IXIT model, disablement assertions, findings,
and verdict aggregation:

- [`config.py`](../tests/lib/config.py): use `load_dut_config(Path(".env"))` for strict DUT setup
  and hostname/IP input; do not duplicate `.env`, hostname, MAC, or URL parsing.
- [`evidence.py`](../tests/lib/evidence.py): use `EvidenceLog.create(...)` as the exclusive
  structured log, including exceptions and final timestamps. Do not implement another logger,
  timestamp/naming scheme, XML transcript log, or generic structured-record serializer.
- [`models.py`](../tests/lib/models.py) and [`errors.py`](../tests/lib/errors.py): reuse
  `DutConfig`, `ProgressReporter`, `RunResult`, and shared setup/error classification. Represent
  stable scan, endpoint, probe, finding, coverage, and comparison records with typed immutable
  dataclasses; use mappings only when parsing external nmap XML or other genuine external
  boundaries.
- [`http_api.py`](../tests/lib/http_api.py): it remains the centralized HTTP vocabulary if an HTTP
  method, status, header, or authentication scheme must be represented. Use `GatewayClient` for the
  mandatory authenticated identity/default preflight before scanning, including relevant custom
  telemetry enable flags. Use `netscan` for scan evidence; HTTP success alone is not a scan.
- `netscan.py` in `cra/303645/tests/lib/`: own reusable resolver, subprocess construction/safety,
  nmap version/XML parsing, timeout handling, and mDNS probing. Do not duplicate those generic
  recipes in the case runner; preserve the task-specific full ranges, finite production limits,
  re-confirmation, UDP honesty, and hardware/network preconditions stated here.

Follow the `FunctionalTest_5_6_1_2_c` dependency-injection pattern with an
`execute_test_5_6_1_2_c(...) -> RunResult` entry, shared progress reporting, and `main() -> int`.

## Execution and configuration

Live execution after implementing this task (not host-side validation):

```text
cd cra/303645/tests
sudo .venv/bin/python test_5_6_1_2_c.py
```

No CLI arguments; load `.env` from the current working directory. Root/`CAP_NET_RAW` required for
the UDP portion (documented; unprivileged → UDP ERROR). `tcpdump` and permission to capture local
UDP/123 traffic are also required. All timeouts are finite production constants. Expect a few
minutes for the TCP scan up to the UDP `--host-timeout` budget; progress must show the active phase
(resolve, preflight, TCP scan, UDP scan, mDNS probe, disablement checks, compare).

## Evidence log

Every invocation, including precondition failures, creates the exclusive structured evidence log
`logs/test_5_6_1_2_c_<UTC-date-time>.log` via `EvidenceLog`, including: test case/unit and UTC
start/end/duration; DUT config; resolved IP and method; `nmap` path/version/privilege; exact command
lines; raw XML; parsed open/closed/filtered sets; `tcpdump` command/output, captured SNTP source
port, UDP coverage comparison, NTP change/read-back and final restoration verification; mDNS and
transient-interface probe request/responses; the computed expected model from IXIT 28/15;
per-finding comparison including the disablement checks; re-confirmation probes; per-check and
overall verdicts; and exception types/tracebacks. Keep terminal output concise.

## DUT preconditions

- Dedicated test gateway in the **initialized state** (SoftAP/provisioning hotspot inactive),
  reachable on the same LAN segment over Ethernet or Wi-Fi STA.
- Custom HTTP/MQTT telemetry left at defaults (disabled) so the disablement checks reflect the
  documented baseline; no firmware update / remote-config in progress; no heavy competing load.
- Test host has `nmap` >= 7.40 runnable with `root`/`CAP_NET_RAW`, `tcpdump`, and can resolve the
  DUT `.local` hostname (or `.env` supplies an IP literal).

This unit temporarily changes only `ntp_server1`. Run it on a dedicated gateway; the original value
and complete public configuration must be restored and verified before PASS.

## Required procedure

1. Create the evidence log; record test id/unit and UTC start.
2. Load/validate `.env`.
3. Resolve `gw_hostname` to a LAN IP (ERROR on failure, mDNS precondition).
4. Preflight `nmap` presence/version/privilege (ERROR on failure). Authenticate and validate DUT
   identity and relevant factory-default fields, including disabled custom telemetry, before scans.
5. Full TCP connect scan `1-65535`; parse XML; re-confirm any unexpected open port; explicitly
   assert no SSH/Telnet/diagnostic port.
6. Start `tcpdump`, set `ntp_server1` to the test host IPv4 address, verify the single-field change,
   and capture the gateway's outbound NTP source port.
7. Privileged UDP scan of the source-declared range with a finite budget; parse XML; require that
   the captured source port is the only `closed|filtered` port.
8. Deterministic mDNS probe (UDP 5353) and transient hotspot-server disablement probes; record
   results.
9. Build the expected model from IXIT 28 (+ IXIT 15 context) and compare, including disablement of
   transient/disabled interfaces.
10. Restore the original `ntp_server1` even after failed capture or scan; verify the full final
    configuration. Aggregate PASS/FAIL/ERROR; write the verdict; print `Overall verdict: ...`.

## Host-side implementation tests

Include authenticated preflight through the real client and shared fake gateway, with identity,
non-default fields, and Basic/Digest setup-error cases; assert no scan starts after failed
preflight.

Follow `test_test_5_1_1_2_b.py` conventions: `unittest`, injected fake subprocess runner returning
canned `nmap` XML, fake resolver, fake mDNS/transient-probe sockets, temporary directories, injected
clock. No real `nmap`/DNS/mDNS calls. Cover at least:

- production constants (port ranges, `--host-timeout`, `--max-retries`, min `nmap` version);
- clean PASS: TCP open == `{80}` HTTP, direct mDNS answer on 5353, no undeclared confirmed-open UDP,
  and transient/disabled interfaces inactive → PASS/exit 0;
- an SSH/Telnet/other extra open TCP port → FAIL/exit 1 with re-confirmation exercised;
- a confirmed-open undeclared UDP port → FAIL/exit 1;
- an active hotspot DHCP/DNS server port in the initialized state → FAIL/exit 1 (disablement
  violation);
- an mDNS responder confirmed on 5353 (declared) → PASS contribution;
- mDNS absent with 5353/udp explicitly closed → FAIL/exit 1; indeterminate or contradictory evidence
  without a direct answer → ERROR/exit 2;
- absence of port 80 → FAIL/exit 1;
- missing/old `nmap`, missing UDP privilege → ERROR/exit 2;
- unresolvable hostname → ERROR/exit 2;
- UDP host-timeout/incomplete → ERROR/exit 2 (not PASS);
- captured SNTP port as sole `closed|filtered` UDP port → PASS contribution; a different or extra
  indeterminate port → ERROR/exit 2;
- lost NTP-change response, capture failure, scan failure, restoration failure, and full final
  configuration mismatch all exercise restoration and ERROR/exit 2;
- runner handling of independently authored parsed scan results; direct XML parsing and reusable
  subprocess/resolver/mDNS contracts belong only in `test_lib.py`;
- evidence-log naming/content and terminal progress match the shared pattern;
- PASS/FAIL/ERROR exit-code aggregation.

## References

- Source: [`Self_Assessment_Test_Group_5_6-1.md`](../Self_Assessment_Test_Group_5_6-1.md) — test
  case 5.6-1-2 (lines 57-79), Unit c row (line 72), summary line 89.
- [`IXIT 28-LogIntf`](../ixit_28-LogIntf.md) — `LogIntf-HTTP-Server`, `LogIntf-mDNS-Responder`,
  custom telemetry, UDP clients/servers, WPS, and the summary matrix.
- [`IXIT 15-PhyIntf`](../ixit_15-PhyIntf.md) — physical-interface context (Ethernet, Wi-Fi, disabled
  radios).
- Firmware: [`docs/http_api.md`](../../../docs/http_api.md);
  [`main/main_loop.c` `start_mdns()`](../../../main/main_loop.c#L485) (mDNS responder on UDP 5353).
- Automation classification:
  [`Functional_Test_Automation_Assessment.md`](../Functional_Test_Automation_Assessment.md) (5.6-1-2
  c — AUTO).
- Contract & patterns: [`task_5_5_5_2_b.md`](task_5_5_5_2_b.md) (shared helper),
  [`task_5_1_5_2_b.md`](task_5_1_5_2_b.md), [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md),
  [`cra/303645/tests/lib`](../tests/lib/README.md).

## Runner naming

Use `FunctionalTest_5_6_1_2_c` and `execute_test_5_6_1_2_c`; retain `main() -> int` as the
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
