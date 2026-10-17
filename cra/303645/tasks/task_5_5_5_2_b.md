# Task: Automate ETSI Test Case 5.5-5-2, Unit B

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

## Authoritative mDNS documentation prerequisite

The source IXIT documents in this branch do not yet declare `LogIntf-mDNS-Responder` or
`SoftServ-Local-mDNS-Discovery`. Updating those authoritative documents is a separate task.
Before mutation or scanning, require actual ID declaration headings in `ixit_28-LogIntf.md` and
`ixit_13-SoftServ.md`, respectively. Missing declarations are a documentation-gap setup ERROR;
the expected model alone must never allowlist UDP/5353. A live PASS requires those updates first.
Offline success fixtures supply independently authored declarations to exercise the later checks.

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
advice. For an authenticated Wi-Fi STA baseline, report unsuitable settings as setup ERROR
without factory-reset advice that would erase its configured connection; correct the relevant
settings manually while preserving connectivity.

Validate transport separately: `use_eth` must be a boolean; `false` requires a configured nonempty
`wifi_sta_config.ssid`. Do not require empty Wi-Fi settings, `use_eth=true`, or factory-default
Ethernet addressing. Preserve all transport fields across the NTP-only writes and compare the
complete saved configuration after recovery. Omitted Ethernet/Wi-Fi fields remain unchanged in
`gw_cfg_json_parse_eth()` and `gw_cfg_json_parse_wifi_sta_config()`.

Use the shared reset instruction: back up needed settings; local configuration, credentials, tokens,
and uploaded certificates/private keys are erased and connectivity is interrupted, while cloud
accounts and forwarded data remain. Hold CONFIGURE through the initial restart until repeating red
200 ms on/200 ms off signals boot-time erase completion, then release. Release triggers another
restart, after which the configuration hotspot opens. About eleven seconds is observed elapsed time,
not a fixed threshold; the five-second restart timer alone does not prove erasure.

## Safe partial NTP configuration writes

Obtain two identical, type-sensitive full authenticated configuration reads before mutation.
Save that complete baseline in local evidence. Require a verified nRF52 MAC and the default
`mqtt_data_format`, scan-default/PHY/channel fields, plus device-specific `mqtt_prefix` equal to
`ruuvi/<nRF52 MAC>/` and `mqtt_client_id` equal to the nRF52 MAC (firmware `gw_cfg_default.c`).
The partial NTP POST resets omitted MQTT fields through `gw_cfg_json_parse_mqtt()`; reject
customized or missing values before starting capture or writing, rather than broadening this task
into an unrelated configuration restore. Model these resets independently in offline gateway fixtures.

Restore `ntp_server1` even after a lost write response or scan failure, then compare the entire
configuration by type and value with the saved baseline. Any drift, including unrelated fields,
is restoration ERROR with recovery advice to restore drifted settings from local baseline evidence
and confirm the full configuration. Matching only the NTP server cannot establish restoration.

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

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.5-5-2, Test Unit
B: Completeness of Logical Interface Documentation (Network Interface Scanning)**.

The test must run against a physical Ruuvi Gateway and prove, by active network discovery, that the
DUT exposes **only the network-reachable listening sockets declared in
[`IXIT 28-LogIntf`](../ixit_28-LogIntf.md)** in the initialized operational state, and that there is
**no undocumented administrative, configuration, diagnostic, or control listener** through which the
security-relevant configuration functions of provision 5.5-5 could be reached outside the documented
interfaces. Every open socket found by the scan must map to an enabled, justified IXIT 28 entry
(and, for management/configuration services, an [`IXIT 13-SoftServ`](../ixit_13-SoftServ.md)
service); every port that is not documented must be reported as a finding.

Follow the structure, execution contract, and user experience of the existing tasks
[`task_5_1_5_2_b.md`](task_5_1_5_2_b.md) and [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md) and reuse the
shared modules in [`cra/303645/tests/lib`](../tests/lib/README.md) rather than duplicating
configuration, logging, or model code.

## Relationship to test 5.6-1-2 Unit c (shared scan helper, distinct normative units)

This unit and **5.6-1-2 Unit c** ([`task_5_6_1_2_c.md`](task_5_6_1_2_c.md)) both rely on full-range
network port scanning and comparison against `IXIT 28-LogIntf`, and both are permitted to share one
scanning helper. **They are distinct normative units and must remain two separate test scripts with
their own verdicts and evidence logs.** Do not merge them.

- **This unit (5.5-5-2 B)** belongs to provision **5.5-5** (_Authentication and Authorization for
  Security-Relevant Network Configuration Functions_). Its focus is that **no undocumented listener
  can carry a security-relevant configuration/management function**. Its comparison set is IXIT 28
  **cross-referenced with the configurable/management services in `IXIT 13-SoftServ`**
  (`SoftServ-Local-Management-WebUI`, `SoftServ-Local-Programmatic-API`,
  `SoftServ-Hotspot-Orchestration`): the only network-reachable configuration surface in the
  initialized state must be the documented HTTP server on port 80. Source:
  [`Self_Assessment_Test_Group_5_5-5.md`](../Self_Assessment_Test_Group_5_5-5.md), lines 67-76 and
  the summary row on line 86.
- **5.6-1-2 Unit c** belongs to provision **5.6-1** (_Unused Network and Logical Interfaces Are
  Disabled_). Its focus is **minimization/disablement**: every open socket maps to an _enabled_ IXIT
  28 entry, and interfaces marked _Disabled_ or _Transient_ (custom telemetry clients, hotspot
  DHCP/DNS servers, WPS) are genuinely **not listening** in the initialized, non-hotspot state.

Implement a shared, backward-compatible helper (see _Shared scanning helper_ below). Whichever of
the two tasks is implemented first creates the helper; the second reuses it without modifying its
behavior. Each test keeps its own comparison model, assertions, verdict aggregation, and host-side
tests. Do not let one unit's expectations leak into the other's runner.

## Deliverables

1. Create `cra/303645/tests/test_5_5_5_2_b.py`, compatible with Python 3.8, taking **no command-line
   arguments**.
2. Create `cra/303645/tests/test_test_5_5_5_2_b.py` containing deterministic host-side
   implementation tests. These mock-based tests are **not** ETSI compliance evidence; they validate
   the Python implementation only.
3. Add the shared scanning helper `cra/303645/tests/lib/netscan.py` (if 5.6-1-2 c has not already
   added it) with an injectable subprocess/resolver/clock so host tests never invoke real `nmap` or
   real DNS/mDNS. Extend `cra/303645/tests/lib/` only where a generally reusable,
   backward-compatible helper is required; do not duplicate existing helpers.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. Prefer the
   standard library and the existing `requests`/`pycryptodome` dependencies. `nmap` is an external
   system binary, **not** a pip package (see _Tooling preconditions_).

Do not modify firmware behavior, the self-assessment source documents, the IXIT documents, or the
existing tests and their shared helpers in a backward-incompatible way.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_5-5.md`](../Self_Assessment_Test_Group_5_5-5.md), test
case 5.5-5-2, **Test Unit B** (lines 67-76). It declares the methodology as comprehensive port
scanning (`nmap -p 1-65535 -sV -sU`) and traffic-capture sweeps across active Ethernet and Wi-Fi
interfaces, and asserts that the initialized DUT exposes the HTTP server on TCP/80 and the mDNS
responder on UDP/5353, with transient setup services restricted to provisioning and **all exposed
listening sockets corresponding precisely to `IXIT 28-LogIntf`**.

The [automation-feasibility assessment](../Functional_Test_Automation_Assessment.md) classifies this
unit **AUTO** and calls for full-range scanning plus correlation of the SNTP client's dynamic UDP
source port. Implement it as an objective, fully scripted scan-and-compare. The subjective/manual
"traffic capture sweeps" wording in the source is **not** required for this unit; do not attempt
subjective manual review. Packet capture is out of scope here (see 5.6-2-2 A for the
disclosure-focused capture rules).

### Expected network-surface model (derived from IXIT 28 and firmware)

Build the expected model programmatically and record it in the evidence log. In the **initialized
state (not the transient provisioning hotspot)**, the reachable network surface over the LAN
(Ethernet / Wi-Fi STA) is:

| Transport | Port(s)                            | Source declaration                                                                                                                                                                                                                           | Expectation on LAN in initialized state                                                                                                                                      |
| :-------- | :--------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| TCP       | 80                                 | `LogIntf-HTTP-Server` ([IXIT 28](../ixit_28-LogIntf.md)) hosting `SoftServ-Local-Management-WebUI` and `SoftServ-Local-Programmatic-API` ([IXIT 13-SoftServ](../ixit_13-SoftServ.md))                                                        | **Open** — the documented management/configuration listener                                                                                                                  |
| TCP       | all others (1-65535 minus 80)      | none                                                                                                                                                                                                                                         | **Closed** — any other open TCP port is an undocumented listener → FAIL                                                                                                      |
| UDP       | 53 / 67 / 68                       | `LogIntf-Network-DNS-Client`, `LogIntf-Network-DHCP-Client` — **outbound clients**, not inbound servers ([IXIT 28](../ixit_28-LogIntf.md))                                                                                                   | No persistent inbound server socket; these are remote service ports.                                                                                                         |
| UDP       | dynamic source (49152-65535) → 123 | `LogIntf-Time-NTP-Client` — **outbound client**, with UDP/123 as the server destination ([IXIT 28](../ixit_28-LogIntf.md))                                                                                                                   | The local source port is boot-randomized and may appear `open\|filtered`; it is not an inbound NTP server. Identify the actual port by capture, not a fixed expected number. |
| UDP       | 53 / 67 / 68                       | `LogIntf-Hotspot-DHCP-Server`, `LogIntf-Hotspot-DNS-Server` — **transient, hotspot-only** ([IXIT 28](../ixit_28-LogIntf.md))                                                                                                                 | **Not active** in the initialized (non-hotspot) state.                                                                                                                       |
| UDP       | 5353 (mDNS)                        | `LogIntf-mDNS-Responder` ([IXIT 28](../ixit_28-LogIntf.md)) and `SoftServ-Local-mDNS-Discovery` ([IXIT 13](../ixit_13-SoftServ.md)); firmware `start_mdns()` advertises `_http._tcp` ([`main/main_loop.c`](../../../main/main_loop.c#L485)). | **Open** — documented local discovery responder. See _mDNS handling_ below.                                                                                                  |

### mDNS handling

After the authoritative declaration prerequisite passes, the `.local` hostname resolution used
throughout this test suite depends on the DUT's documented mDNS responder on **UDP 5353**. Treat it as an objective part of the comparison:

- Confirm the responder deterministically (a direct mDNS query to UDP 5353 that returns an answer is
  a definitive positive, immune to the UDP `open|filtered` ambiguity described below).
- Record it in evidence and include it in the "open sockets vs IXIT 28" comparison.
- A confirmed `_http._tcp` response matches `LogIntf-mDNS-Responder` and contributes **PASS**. If
  the scan explicitly reports 5353/udp closed and the direct probe gets no answer, report a **FAIL**
  for the missing enabled service. If the scan reports `open` but the direct query gets no answer,
  or only an indeterminate UDP state is available, report **ERROR** because the service identity or
  availability cannot be established.

The direct response is positive evidence even if the broad UDP scan misses this multicast-oriented
service; record both observations.

## Tooling preconditions and privilege model

`nmap` is required for authoritative full-range scanning and UDP service detection; a pure
`socket`-based connect scan cannot reliably fingerprint services or perform UDP detection and would
be far slower across 65535 ports. Treat `nmap` as a hard external precondition:

1. **Locate binary** — `shutil.which("nmap")`. If absent → **ERROR** with a documented precondition
   (`apt-get install nmap`).
2. **Version preflight** — run `nmap --version`, parse the version, and require **>= 7.40** (stable
   `-oX` XML, `-sV`, `--defeat-icmp-ratelimit`). If older or unparseable → **ERROR**.
3. **Privilege model** — a **TCP connect scan (`-sT`)** needs no elevated privilege and reports
   `open`, `closed`, or `filtered`. A **UDP scan (`-sU`)** needs `root`/`CAP_NET_RAW` to read ICMP
   port-unreachable messages; **without privilege an `nmap` UDP scan cannot distinguish `closed`
   from `filtered` and therefore cannot support any completeness claim**. If the UDP scan cannot run
   privileged, the UDP portion must return **ERROR** with a documented precondition (run as root or
   grant `CAP_NET_RAW`), **never a silent PASS**.

Do not weaken TCP or UDP coverage silently to avoid a privilege or timeout error; downgrade to an
explicit ERROR with a documented precondition instead.

## Scan strategy

Run all scans against the **resolved LAN IP of the DUT** (see _Hostname resolution_), with host
discovery disabled (`-Pn`) and DNS disabled (`-n`), emitting XML (`-oX`) parsed by the helper.

### TCP (full range)

- `nmap -sT -Pn -n --reason -sV --scan-delay 5ms -p 1-65535 -oX <file> <ip>` (full 1-65535 range).
  The delay reduces the request rate to the DUT.
- TCP connect scan reports `open`, `closed`, or `filtered`; there is no `open|filtered` ambiguity
  for TCP. A `filtered` result means no definitive response was received; it does not identify where
  the packet was dropped. Any filtered TCP port leaves coverage incomplete and produces **ERROR
  (2)**, even if all confirmed-open ports match IXIT 28.
- **Expected open set: exactly `{80/tcp}`.** Any additional open TCP port is an undocumented
  listener → FAIL.
- **False-positive handling:** re-confirm every unexpected open TCP port with a second independent
  probe (a fresh `nmap -sT -sV` of just that port, or a direct `socket` connect + short banner read)
  before recording it as a FAIL finding. Record both the initial and confirmation results.

### UDP (source-declared full range, privileged, honest interpretation)

- Target the **full `1-65535` UDP range** to match the source methodology, using
  `nmap -sU -sV -Pn -n --reason -d3 --defeat-icmp-ratelimit --scan-delay 5ms` with
  `-p 1-65535 -oX <file> <ip>`, a small documented `--max-retries`, and a **finite
  `--host-timeout`**. Requires privilege (above).
- A full UDP `-sV` sweep is slow. Use a named production constant for the finite `--host-timeout`
  and an outer Python subprocess timeout. **If the scan cannot complete within the budget, return
  ERROR** (an incomplete scan is not evidence of absence), and document the budget in the evidence
  log. Do not silently reduce the port range; if a reduced range is ever used it must be an
  explicitly logged, documented limitation, not a hidden default.
- **Honest interpretation (mandatory):** ordinary UDP scans can report non-responding ports as
  `open|filtered`. With the required `--defeat-icmp-ratelimit`, Nmap reports them as
  `closed|filtered` instead,
  [trading accuracy for speed](https://nmap.org/book/man-performance.html). **Neither nonresponse
  state proves that no UDP service exists.** Therefore:
  - Only a UDP port that `nmap` reports as **`open`** (state confirmed, ideally with an `-sV`
    service match) counts as a discovered listening service.
  - An undeclared **confirmed-open** UDP port → FAIL.
  - `open|filtered` and `closed|filtered` ports are **indeterminate** and must never be interpreted
    as closed. If any port is indeterminate after the full scan and cannot be attributed to the
    captured outbound SNTP client or direct mDNS response, report UDP coverage **ERROR (2)** with
    its count and available port IDs, even if the confirmed-open listener comparison finds no
    undocumented service. A direct mDNS response resolves an indeterminate UDP/5353 result. Do not
    claim that every nonresponding port is closed.
- **Deterministic positive probe for the known responder:** independently of the broad `nmap` sweep,
  send a direct DNS-SD query to UDP 5353 and record whether the DUT advertises `_http._tcp`. This is
  a definitive, privilege-independent positive confirmation and feeds the IXIT 28 comparison (see
  _mDNS handling_). Use this response as the independent confirmation for a broad-scan 5353/udp
  `open` result; a second single-port `nmap -sV` can spend its entire host budget on service probes.
- `-d3` keeps indeterminate port IDs in Nmap's XML instead of only reporting an `extraports` count.
  Log their states and IDs. The 5 ms full-scan delay makes a separate slower pass unnecessary;
  silence still leaves coverage ERROR.
- The SNTP client binds a local UDP source port from 49152-65535 when sending its first request.
  LwIP seeds its dynamic-port allocator randomly at boot; the selected port may change after a
  reboot or SNTP restart, so neither a port observed in an earlier run nor UDP/123 identifies the
  current DUT-side socket. Before scanning, determine the test host's LAN IPv4 address and start
  `tcpdump` for a 48-byte NTP client request (mode 3) from the verified DUT to host UDP/123. Once
  capture is listening, POST `{"ntp_server1": "<host-ipv4>"}` to authenticated `/ruuvi.json` and
  verify the setting by GET. Record the gateway's source port from the captured request, then close
  the capture. Only that port, if indeterminate in the later UDP sweep, is attributable to the
  documented outbound SNTP client; record this distinction in evidence. Every other unresponsive
  port remains a coverage ERROR. The test host need not serve NTP to observe the request.
- Save the original `ntp_server1` before mutation. In a cleanup path that runs after scan, timeout,
  or transport errors, reauthenticate, POST the original value, and verify it by GET. Restoration
  failure is overall ERROR with the original value in recovery advice. Starting `tcpdump` and
  validating factory-default NTP settings must precede the first mutation.

### Retries and stability

- Use small, finite `nmap` retry counts (`--max-retries`) chosen to bound runtime while limiting
  transient false negatives; document the chosen values as production constants.
- Confirm any FAIL-triggering open port with an independent re-probe (above) so a single transient
  reading cannot flip the verdict. For 5353/udp, the direct DNS-SD query is that independent probe.

## Hostname resolution

- Load `gw_hostname` from `.env` via `lib.config.load_dut_config` and resolve it to an IP with
  `socket.getaddrinfo` (prefer IPv4). Record the resolved address. Before authentication, pin the runner's `GatewayClient` to that
  selected IPv4 for every HTTP operation, including NTP mutation and restoration; retain the original
  configured hostname in evidence. Subsequent hostname changes must not redirect those requests.
- The default `gw_hostname` (e.g. `ruuvigateway9c2c.local`) is an **mDNS** name; resolving it
  requires host mDNS support (e.g. `avahi-daemon` + `libnss-mdns`, or `getaddrinfo` mDNS). If
  resolution fails → **ERROR** with a documented precondition (install/enable mDNS resolution on the
  test host, or set `gw_hostname` to a literal IP in `.env`). `lib.config` already accepts an IP
  literal for `gw_hostname`.
- If resolution returns multiple addresses, scan the primary IPv4 address and log the full set;
  treat IPv6-only resolution as out of scope for this scan (log a clear ERROR/limitation) unless an
  IPv4 address is available.
- The scan must be run from a host on the **same LAN segment** as the DUT so that the LAN listening
  surface (not a WAN/NAT view) is measured. State this in the DUT preconditions.

## Comparison and verdict

Build the expected model from IXIT 28 (and, for management services, IXIT 13) in code, then compare:

1. **TCP open set == `{80}`** and port 80 fingerprints as an HTTP server (`-sV`, service
   `name="http" method="probed"`). A table-only guess or missing probe metadata is unverified
   HTTP **ERROR**, even if the port is open; an actively probed non-HTTP service is FAIL.
2. **No undeclared confirmed-open TCP port.** Any extra open TCP port → FAIL finding.
3. **No undeclared confirmed-open UDP port** among scanned ports. Any confirmed-open undeclared UDP
   port → FAIL finding.
4. **mDNS/5353** result is recorded and compared to `LogIntf-mDNS-Responder`; a confirmed
   `_http._tcp` responder matches the documented service (see _mDNS handling_).
5. **Complete TCP and UDP coverage** requires a definitive state for every scanned port or
   independent identification of the outbound SNTP client source port or mDNS responder. Any
   remaining TCP `filtered` or UDP `filtered`, `open|filtered`, or `closed|filtered` result yields
   **ERROR (2)**, as does a timeout, incomplete scan, or unverified configuration restoration.
   Record the count and retain any confirmed undocumented-listener FAIL in its individual outcome
   even when overall completion is ERROR.

Emit a per-finding table and an overall verdict. This unit's PASS requires: TCP/80 is the only open
TCP socket, the documented UDP/5353 responder is confirmed, no undocumented open socket exists
relative to IXIT 28, no scanned port remains unresolved after the SNTP and mDNS observations, and
the complete baseline configuration is restored.

### Exit codes

| Exit code | Meaning                                                                                                                                                                                                           |
| :-------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS — documented listeners only, with the captured SNTP client port and direct mDNS response independently identified and configuration restored                                                         |
| `1`       | A security finding — an undocumented open/confirmed listening socket (TCP or UDP) was found, or a documented enabled listener was absent                                                                          |
| `2`       | Invalid setup / precondition failure / inconclusive or incomplete scan (unresolved port state, missing or too-old `nmap`, missing privilege for UDP, unresolvable hostname, scan timeout, subprocess/parse error) |

A missing privilege, incomplete/timed-out scan, or port unresolved after SNTP and mDNS
identification must be **ERROR (2)**. The direct mDNS probe still must confirm the documented
responder before PASS.

## Shared scanning helper (`lib/netscan.py`)

Provide reusable, dependency-injected primitives so both scan tests and their host tests can run
without real `nmap`, DNS, or mDNS:

- `resolve_host(hostname, *, resolver=socket.getaddrinfo) -> ResolvedHost` (ip, family; raises a
  `lib.errors.InvalidSetup` subclass on failure).
- `check_nmap(min_version, *, runner=subprocess.run) -> NmapInfo` (path, parsed version,
  `has_raw_privilege`); raises on missing/old binary.
- `run_tcp_connect_scan(ip, ports, *, runner, host_timeout, max_retries) -> ScanResult`.
- `run_udp_scan(ip, ports, *, runner, host_timeout, max_retries) -> ScanResult` (requires privilege;
  raises a documented ERROR-class exception when unprivileged).
- `probe_mdns(ip, *, timeout, socket_factory=socket.socket) -> Optional[MdnsAnswer]` — deterministic
  direct UDP 5353 query.
- `ScanResult` dataclass: `open_ports`, `closed_count`, `filtered_count`, `open_filtered` and
  `closed_filtered` sets and counts, the raw XML path/string, and elapsed time. Parse `nmap` **XML**
  (not human output) for stability.

All external effects (subprocess, DNS, mDNS socket, clock) must be injectable. Keep the helper free
of any single unit's verdict logic; the comparison-to-IXIT-28 model lives in each test runner.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for validated DUT setup,
  evidence, typed results/progress, and precondition/error classification. Do not reimplement them.
- Use the shared `lib.netscan` primitives and typed immutable scan models. New stable scan,
  expectation, and finding schemas must also be typed immutable models; dictionaries are limited to
  genuine XML/JSON/header mapping boundaries.
- Use `GatewayClient` for the mandatory authenticated identity/default preflight before scanning.
  The socket scan itself uses `lib.netscan` service fingerprints; do not substitute HTTP preflight
  success for port-scan evidence. Use shared HTTP descriptors for preflight requests.

The shared modules are the primary implementation references. Firmware sources are behavioral
references for expected listeners only.

Follow the existing runner pattern from `test_5_1_1_2_b.py`:

- a `FunctionalTest_5_5_5_2_b` class with dependency injection for the scan runner, resolver, mDNS
  probe, and clock;
- an `execute_test_5_5_5_2_b(...) -> RunResult` entry point for host-side tests;
- `main() -> int`; `sys.exit(main())`;
- progress reported as `[Step N out of M] <description>`;
- print the log path once as `Open log file: <path>`;
- print the final line as `Overall verdict: <PASS|FAIL|ERROR>`.

## Execution and configuration

Live execution after implementing this task (not host-side validation):

```text
cd cra/303645/tests
sudo .venv/bin/python test_5_5_5_2_b.py
```

- No command-line arguments. Load `.env` from the current working directory.
- Root/`CAP_NET_RAW` is required for the UDP portion; document this in the preconditions and the
  console/first-log lines. If run unprivileged, the UDP portion ERRORs with a clear message.
- All timeouts (per-scan `--host-timeout`, outer subprocess timeouts, mDNS probe timeout) are finite
  production constants; none are CLI options.

Expect a live run to take from a few minutes (TCP connect scan of 65535 ports) up to the configured
UDP `--host-timeout` budget. Progress reporting must make clear the process is active and show the
current phase (resolve, preflight, TCP scan, UDP scan, mDNS probe, compare).

## Evidence log

Every invocation, including precondition failures, must create
`logs/test_5_5_5_2_b_<UTC-date-time>.log` using `EvidenceLog`. It must include: test case/unit and
UTC start/end/duration; DUT configuration; resolved IP and resolution method; `nmap` path/version
and privilege status; exact `nmap` command lines; raw `nmap` XML; parsed open/closed/filtered port
sets; local IPv4 address, `tcpdump` command/output, captured SNTP source port, configuration
mutation and verified restoration; UDP `open|filtered`/`closed|filtered` sets and any unresolved
coverage error; mDNS probe request/response; the computed expected model from IXIT 28/IXIT 13;
per-finding comparison; re-confirmation probes; per-check and overall verdicts; and exception
types/tracebacks. Keep terminal output concise.

## DUT preconditions

- A dedicated test gateway in the **initialized state** (not the provisioning hotspot; SoftAP
  inactive), reachable on the same LAN segment as the test host over Ethernet or Wi-Fi STA.
- No firmware update or remote-configuration operation in progress; no other process generating
  significant load.
- The test host has `nmap` >= 7.40 installed and can run it with `root`/`CAP_NET_RAW`, and can
  resolve the DUT's `.local` hostname (mDNS) or `.env` provides an IP literal. It also has `tcpdump`
  and permission to capture local UDP/123 traffic.

This unit temporarily changes `ntp_server1` and restores it after the scan. Run it only on a
dedicated test gateway and verify the restoration result.

## Required procedure

1. Create the evidence log; record test id/unit and UTC start.
2. Load and validate `.env`.
3. Resolve `gw_hostname` to a LAN IP; record it (ERROR on failure, with the mDNS precondition).
4. Preflight `nmap` presence, version, and privilege (ERROR on failure). Authenticate and validate
   DUT identity and relevant factory-default fields, including NTP settings, before starting scan
   traffic. Determine the host LAN IPv4 address, start `tcpdump`, POST it as `ntp_server1`, verify
   by GET, and capture the gateway's NTP request source port.
5. Full TCP connect scan `1-65535` at 5 ms scan delay; parse XML; re-confirm any unexpected open
   port and mark unresolved filtered ports ERROR.
6. Privileged UDP scan of the source-declared range at 5 ms scan delay with a finite budget; parse
   XML and record `open|filtered`/`closed|filtered` indeterminacy (ERROR on any port not
   independently identified as the SNTP client or mDNS responder, or on incomplete/timeout).
7. Deterministic mDNS probe to UDP 5353; record the answer.
8. Build the expected model from IXIT 28 (+ IXIT 13 for management services) and compare.
9. Restore the saved `ntp_server1` even after a prior error, verify the entire saved configuration
   baseline by GET, then aggregate
   findings into PASS/FAIL/ERROR; write the verdict; print `Overall verdict: ...`.

## Host-side implementation tests

Include authenticated preflight through the real client and shared fake gateway, with identity,
non-default fields, and Basic/Digest setup-error cases; assert no scan starts after failed
preflight.

Follow `test_test_5_1_1_2_b.py` conventions: `unittest`, injected fake subprocess runner returning
independently authored canned `nmap` XML, fake resolver, fake mDNS socket, temporary directories,
and an injected clock. No real `nmap`/DNS/mDNS calls. Any fake HTTP server must use independent wire
fixtures and must not invoke client code under test to form expected requests. Cover at least:

- production constants (port ranges, `--host-timeout`, `--max-retries`, 5 ms full-scan delay,
  minimum `nmap` version);
- clean PASS path: TCP open == `{80}` with HTTP `-sV` match, direct mDNS answer on 5353, captured
  SNTP client source port, no undeclared confirmed-open UDP port, no other unresolved port states,
  and verified NTP restoration → PASS/exit 0;
- an extra open TCP port (e.g. `2323/tcp open`) → FAIL/exit 1, with re-confirmation exercised;
- a confirmed-open undeclared UDP port → FAIL/exit 1;
- an mDNS responder confirmed on 5353 (declared in IXIT 28) → PASS contribution;
- mDNS absent with 5353/udp explicitly closed → FAIL/exit 1; indeterminate or contradictory evidence
  without a direct answer → ERROR/exit 2;
- definitively closed port 80 or an open port with a non-HTTP fingerprint → FAIL/exit 1;
- filtered or unknown port 80, including ambiguous compressed port groups → HTTP ERROR/exit 2;
- table-only HTTP guesses versus actively probed HTTP identification;
- successful configured Wi-Fi STA baseline preserved across mutation/restoration, with no reset advice;
- all HTTP URLs pinned to the scanned IPv4 even if later hostname resolution changes;
- missing `nmap`, too-old version, and missing privilege for UDP → ERROR/exit 2;
- unresolvable hostname → ERROR/exit 2;
- UDP scan host-timeout/incomplete → ERROR/exit 2 (not PASS);
- remaining TCP `filtered` or UDP `filtered`/`open|filtered`/`closed|filtered` output is recorded as
  ERROR coverage unless the exact UDP port has independent SNTP or mDNS identification;
- `tcpdump` startup/capture failure stops before scans and always restores an attempted NTP change;
  a lost POST response, scan timeout, and failed restoration have distinct ERROR evidence;
- runner handling of independently authored parsed scan results; direct XML parsing and reusable
  subprocess/resolver/mDNS contracts belong only in `test_lib.py`;
- evidence-log naming/content and terminal progress match the shared pattern;
- PASS/FAIL/ERROR exit-code aggregation.

## References

- Source: [`Self_Assessment_Test_Group_5_5-5.md`](../Self_Assessment_Test_Group_5_5-5.md) — test
  case 5.5-5-2 Unit B (lines 67-76), summary line 86.
- [`IXIT 28-LogIntf`](../ixit_28-LogIntf.md) — `LogIntf-HTTP-Server`, `LogIntf-mDNS-Responder`, UDP
  clients/servers, and the summary matrix.
- [`IXIT 13-SoftServ`](../ixit_13-SoftServ.md) — configurable/management services reached over the
  network.
- [`IXIT 15-PhyIntf`](../ixit_15-PhyIntf.md) — physical interfaces carrying the logical sockets
  (context).
- Firmware: [`docs/http_api.md`](../../../docs/http_api.md) (HTTP surface on port 80);
  [`main/main_loop.c` `start_mdns()`](../../../main/main_loop.c#L485) (mDNS responder on UDP 5353).
- Automation classification:
  [`Functional_Test_Automation_Assessment.md`](../Functional_Test_Automation_Assessment.md) (5.5-5-2
  B — AUTO).
- Contract & patterns: [`task_5_1_5_2_b.md`](task_5_1_5_2_b.md),
  [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md), [`cra/303645/tests/lib`](../tests/lib/README.md).

## Runner naming

Use `FunctionalTest_5_5_5_2_b` and `execute_test_5_5_5_2_b`; retain `main() -> int` as the
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
