# Task: Automate ETSI Test Case 5.6-2-2, Unit A

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
malformed identity is ERROR without reset advice. Reject an all-zero nRF52 MAC even if it matches
`.env`: firmware treats it as invalid and uses the `RuuviGatewayXXXX` fallback; it cannot verify
identity or authorize DHCP capture/reset. If MAC is absent, a non-default baseline is ERROR
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

Create a Python functional test for **ETSI EN 303 645 / ETSI TS 103 701 test case 5.6-2-2, Test Unit
A: Functional Assessment of Unauthenticated Disclosed Information**.

The test must run against a physical Ruuvi Gateway and prove, by unauthenticated HTTP probing in the
**initialized operational state**, that no **unannounced or security-relevant information**
(internal memory pointers, cleartext credentials, password hashes, API keys / bearer tokens, Wi-Fi
PSKs, private keys, exact component patch levels / build hashes, or personal data such as sensor
history) is observable from network-accessible interfaces **without authentication**, while
confirming that the information that _is_ disclosed is limited to the **deliberately public device
metadata** declared in [`IXIT 28-LogIntf`](../ixit_28-LogIntf.md) (generic `Server: Ruuvi Gateway`
banner; 302 redirects to the auth entry; 401 auth challenge; hostname).

The assessment must be **objective and fully scripted** — no subjective manual review. Follow the
execution contract and user experience of [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md) and
[`task_5_1_5_2_b.md`](task_5_1_5_2_b.md) and reuse the shared modules in
[`cra/303645/tests/lib`](../tests/lib/README.md).

## Deliverables

1. Create `cra/303645/tests/test_5_6_2_2_a.py`, Python 3.8-compatible, **no command-line
   arguments**.
2. Create `cra/303645/tests/test_test_5_6_2_2_a.py` with deterministic host-side implementation
   tests (mock-based; **not** ETSI compliance evidence).
3. Extend `cra/303645/tests/lib/` only where a generally reusable, backward-compatible helper is
   required (e.g. a reusable "sensitive pattern" scanner). Do not duplicate existing helpers; reuse
   `GatewayClient`, `EvidenceLog`, `http_api`, models, and errors.
4. Update `cra/303645/tests/requirements.txt` only if a new dependency is unavoidable. The core HTTP
   assessment needs only the existing `requests` dependency and the standard library. Any packet
   capture uses a system tool / optional dependency (see _Optional L2 packet-capture sub-check_).

Do not modify firmware behavior or any self-assessment/IXIT source document.

Disclosure scanning includes nonempty credential input values identified by a known field's
`name` or `id`, including hidden inputs, as well as literal assignments in rendered HTML and
`text/plain` responses. Scan decoded rendered text for quoted configuration dumps and runtime
diagnostics at every HTTP status, excluding script/style text from the rendered scan. Decode JSON
where possible, including recursively serialized strings inside containers, and preserve quote/escape boundaries in quoted credential and embedded Wi-Fi
configuration literals. Check DeviceID seeds in raw and decoded JSON text, including the equivalent
contiguous hexadecimal representation; do not match a substring of a longer hexadecimal value.
Nonempty headers named for known credentials also fail the scan, independent of their value syntax.
Accept both `:` and `=` for bare credential assignments in rendered/plain text and headers.
Inspect decoded JSON credential fields structurally: numbers (including zero), booleans, and
populated arrays/objects are values even when they are not quoted strings. Null, empty strings,
empty containers, and string-type schema descriptors remain public controls. Preserve the public
static asset's default `Admin` username exception. Exempt only the exact UI phrase
`Bearer authentication` without an Authorization label; short alphabetic bearer values such as
`Bearer abc` are disclosures in both raw and decoded text.
Empty controls and ordinary JavaScript expressions are not credentials.
The known-field inventory includes all remote-config token/API-key variants and Basic usernames,
HTTP statistics usernames, and all four client private-key storage names (`http_cli_key`,
`stat_cli_key`, `mqtt_cli_key`, `rcfg_cli_key`). A nonempty value fails even without PEM syntax;
public certificate field names, empty defaults, and schema declarations remain permitted.
For every captured DHCP packet, inspect BOOTP's fixed `sname` and `file` fields as well as DHCP
options. These client fields must be zero-filled; any nonempty payload is undeclared information
and fails the DHCP check, including ASCII or binary DeviceID password seeds in an earlier packet.

## Normative source and interpretation

The source is [`Self_Assessment_Test_Group_5_6-2.md`](../Self_Assessment_Test_Group_5_6-2.md), test
case 5.6-2-2, **Test Unit A** (lines 59-78), with conceptual context in 5.6-2-1 (lines 18-46). The
declared methodology is unauthenticated HTTP requests (`GET /`, `GET /ruuvi.json`, `GET /history`,
`HEAD /`), packet-capture inspection of LAN broadcasts, and inspection of header banners and
authentication workflows. The declared expected behavior:

- Generic `Server: Ruuvi Gateway` header; **no** software version numbers or build hashes.
- Unauthenticated `GET /ruuvi.json` and `GET /history` return **HTTP 302** with
  `Location: http://<hostname>.local/#auth`; zero configuration, credentials, or sensor arrays.
- `GET /auth` returns **HTTP 401 Unauthorized**.
- LAN DHCP frames contain only standard L2 MAC address and hostname strings.

The [automation-feasibility assessment](../Functional_Test_Automation_Assessment.md) classifies this
unit **AUTO**: "Unauthenticated HTTP requests plus local capture analysis can be scripted with
objective assertions on banners/redirects/disclosure." Implement the **HTTP-disclosure assessment as
the mandatory, fully-AUTO core**; treat the L2 DHCP capture as a **precondition-gated sub-check**
(see below), because it is only automatable under documented host conditions.

This runner assesses HTTP responses and DHCP broadcasts, matching the source Unit A table.
The UART `### Start mDNS` line is used only to correlate the configured hostname after reset;
it is not an mDNS packet observation. UDP 5353 traffic and mDNS disclosure assessment are outside
this task's implemented coverage and are not implied by a PASS here.

### Firmware reality vs source wording (must be handled, not copied blindly)

Verify behavior against the implementation-derived [HTTP API reference](../../../docs/http_api.md),
not the prose of the self-assessment:

- **`HEAD /` returns HTTP `400`, not a banner.** The firmware dispatch handles only `GET`/`DELETE`/
  `POST`; any other method returns `400`
  ([`http_server_handle_req()`](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L693)).
  The source's "`HEAD /` returns `Server: Ruuvi Gateway`" is inaccurate. The test must **send a
  `HEAD /` probe and assert `400`** (documenting the discrepancy in evidence) and must obtain the
  `Server` banner via **`GET`** instead. Do not fail the test merely because `HEAD` is not a banner
  source; treat `HEAD -> 400` as expected.
- **Unauthenticated `GET /info.json` returns `302` on LAN in default auth mode.** Authentication
  runs before dispatch, so a request without an authorized cookie is redirected before the
  hotspot-only handler can return `404`
  ([`http_api.md` GET /info.json](../../../docs/http_api.md#get-infojson)). Assert `302`, with the
  previous-URL cookie and no metadata body. An authorized LAN request reaches dispatch and returns
  `404`; that is outside this unauthenticated matrix.
- **`.json` / extensionless data endpoints redirect with `302`** under `DEFAULT`/`RUUVI` auth;
  `/ap.json` and `/status.json` are redirected but **omit** the previous-URL cookie
  ([`http_api.md` auth model](../../../docs/http_api.md#authentication-model)). Assert the `302` and
  the differing `Set-Cookie` behavior without following redirects.

## Scope of probing (GET only; no dangerous methods)

Probe **unauthenticated** (no session cookie, no Authorization header) over the LAN in the
initialized `lan_auth_default` state. Use `GatewayClient.request(..., allow_redirects=False)` so
redirects are **captured, not followed**. Enumerate every source-relevant GET path from the
canonical [`lib.http_api`](../tests/lib/http_api.py) inventory, plus the static root, a
representative static asset, an unknown `*.json`, and a single `HEAD /` discrepancy probe:

| Probe                                                           | Expected unauthenticated status | Notes                                                                                                                                        |
| :-------------------------------------------------------------- | :------------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------- |
| `GET /`                                                         | `200`, `text/html`              | Public Web-UI (`ruuvi.html`/`index.html`), served without auth ([static fallback](../../../docs/http_api.md#static-web-assets-and-fallback)) |
| `GET /<a static asset>` (e.g. a `.js`/`.css` reached from root) | `200`                           | Public app asset — still scanned for embedded secrets                                                                                        |
| `GET /auth`                                                     | `401`                           | Interactive challenge (`WWW-Authenticate: x-ruuvi-interactive`, fresh session id, realm, challenge, optional gateway ECDH key)               |
| `GET /ruuvi.json`                                               | `302` -> `.../#auth`            | Config gated; body must contain no config; prev-url `Set-Cookie` present                                                                     |
| `GET /history`                                                  | `302` -> `.../#auth`            | Sensor history gated; no sensor arrays                                                                                                       |
| `GET /status.json`                                              | `302`                           | Cookie omitted                                                                                                                               |
| `GET /ap.json`                                                  | `302`                           | Cookie omitted; RW-gated route                                                                                                               |
| `GET /firmware_update.json`                                     | `302`                           |                                                                                                                                              |
| `GET /metrics`                                                  | `302`                           | Extensionless, auth-gated                                                                                                                    |
| `GET /validate_url`                                             | `302`                           | Extensionless, auth-gated                                                                                                                    |
| `GET /extra_cfg`                                                | `302`                           | Extensionless, auth-gated                                                                                                                    |
| `GET /info.json`                                                | `302`                           | Auth redirects before hotspot-only dispatch                                                                                                  |
| `GET /<random>.json`                                            | `302`                           | Auth redirects before unknown-route dispatch                                                                                                 |
| `HEAD /`                                                        | `400`                           | Firmware method-dispatch discrepancy (documented above)                                                                                      |

The disclosure matrix sends only GET/HEAD requests from fresh unauthenticated sessions, with no
baseline cookies or Authorization headers. The separate mandatory setup phase may use `POST /auth`
for default login and authenticated `GET /ruuvi.json` for identity/default checks; its responses are
not disclosure evidence. Do not send configuration POSTs or DELETEs, or touch `/connect.json`,
`/fw_update*`, `/ssl_cert`, `/extra_cfg` writes, or `/init_storage`.

For each probe, capture and log the exact request, the full status line, **all response headers**,
any `Set-Cookie`/`Location` values, and the complete body, then run the disclosure analysis below.

## Disclosure analysis: sensitive patterns vs deliberately public metadata

Scan **every unauthenticated probe response's headers and body** (including static assets) against a
**sensitive-pattern set** derived from the IXIT sources. Exclude authenticated preflight evidence
from this corpus. Match disclosed values in context: public JavaScript field names, schema strings,
and ephemeral challenge values alone do not prove a leaked secret. Ground the patterns in these
declarations (cite them in the evidence log):

- **Credentials / password hashes** — `lan_auth_user`, `lan_auth_pass` (the stored MD5 hash), from
  [`IXIT 10-SecParam` `SecParam-LAN-WebUI-Credentials`](../ixit_10-SecParam.md) (lines 254-295) and
  [`IXIT 1-AuthMech`](../ixit_1-AuthMech.md). The DeviceID default password seed
  (`SecParam-Hardware-DeviceID`, lines 212-253) — equal to `.env` `gw_id` — must never appear
  unauthenticated.
- **M2M bearer / API keys** — `lan_auth_api_key`, `lan_auth_api_key_rw`
  ([`SecParam-LAN-Bearer-Tokens`](../ixit_10-SecParam.md) lines 591-641) and any
  `Authorization: Bearer` value.
- **Wi-Fi credentials** — `wifi_sta_config.password` / `wifi_ap_config.password` PSKs
  ([`SecParam-WiFi-STA-Credentials`](../ixit_10-SecParam.md) lines 307-336). Scan nested JSON and
  JavaScript object literals with quoted, unquoted identifier, or mixed keys. Keep identifier
  boundaries exact, handle escaped strings, and permit empty passwords and schema descriptions.
- **Remote-config / custom-telemetry secrets** — `remote_cfg_auth_bearer_token`,
  `remote_cfg_auth_basic_pass`, `http_user`/`http_pass`, `http_bearer_token`, `http_api_key`,
  `mqtt_user`/`mqtt_pass`, `mqtt_cli_key`, `http_stat_pass`, `stat_cli_key`, and any client private
  key ([`SecParam-Remote-Config-Assets`](../ixit_10-SecParam.md) lines 355-411, `SecParam-Custom-*`
  lines 404-532; [`PersData-Custom-Target-Access-Secrets`](../ixit_21-PersData.md) lines 290-360).
- **Private-key / certificate material** — `-----BEGIN [A-Z ]*PRIVATE KEY-----` and similar PEM
  markers.
- **Personal data / sensor telemetry** — decoded sensor history arrays, BLE MAC lists, coordinates
  ([`PersData-BLE-Sensor-Telemetry`](../ixit_21-PersData.md) lines 361+); these are gated behind
  auth and must not appear in unauthenticated bodies.
- **Exact component patch levels / build identifiers** — precise firmware version strings, ESP-IDF
  build hashes, git commit hashes, and internal build metadata that IXIT 28 says are **suppressed**
  ([`LogIntf-HTTP-Server` Disclosed Information](../ixit_28-LogIntf.md) lines 27-37; 5.6-2-1 Unit B
  lines 36-46). Define objective regexes (e.g. semantic-version patterns like `v?\d+\.\d+\.\d+`,
  40/7+ hex git hashes, `esp-idf`/`v4.2` build tokens) and assert none appear in **headers**
  (especially `Server`) or error bodies.
- **Internal memory pointers / diagnostics** — hex addresses (`0x[0-9a-fA-F]{6,}`), backtrace/stack
  markers, `abort()`/panic strings.

### Deliberately public metadata (must be allowed, and positively verified)

Distinguish the following declared-public items and assert they are present/normal rather than
flagging them:

- **`Server: Ruuvi Gateway`** — the **exact generic banner**; assert it is present and equals the
  generic string with **no** appended version/build token
  ([IXIT 28 lines 27-37](../ixit_28-LogIntf.md); emitted verbatim in
  [`http_server_netconn_resp.c`](../../../components/esp32-wifi-manager/src/http_server_netconn_resp.c#L462)).
- **Hostname in the `302` `Location`** (`http://<hostname>/#auth`) and via mDNS — device hostname is
  deliberately public metadata; do not treat the hostname alone as a sensitive disclosure.
- **Web-UI static HTML/JS/CSS** — the public configurator app; assert `200` and scan for embedded
  secrets, but its ordinary presence is expected.
- **Ephemeral auth-challenge material** — `WWW-Authenticate: x-ruuvi-interactive` parameters (realm,
  challenge nonce, session id, `RUUVISESSION` cookie) and the per-request gateway **ECDH public
  key** are ephemeral, non-secret handshake values; assert their expected shape (reuse
  `GatewayClient.parse_interactive_challenge`) but do not flag them as sensitive disclosure.

The verdict must be objective: a FAIL requires a concrete sensitive-pattern match with the offending
probe, header/field, and matched token recorded; deliberately public metadata never triggers FAIL.

## Optional L2 packet-capture sub-check (precondition-gated)

The source's "LAN DHCP Broadcast Inspection" scenario asserts DHCP frames disclose only the MAC and
hostname. Under the no-manual, no-CLI-argument contract this is only automatable when the **test
host can capture and reset the dedicated DUT**. Specify it as a precondition-gated sub-check, and
**do not silently skip or silently pass it**:

- **Prerequisites (auto-detected, documented):** `root`/`CAP_NET_RAW`; `tcpdump` and `ip` tools;
  exactly one connected CH340 USB bridge with working esptool reset wiring; a capture interface on
  the DUT's LAN segment; and observation of a DHCP frame **from the DUT's active Ethernet or Wi-Fi
  MAC**, resolved from its verified IP through an on-link ARP/neighbor entry or equivalent trusted
  observation, within a finite capture window (named production constant). `.env` `gw_mac` is the
  nRF52 identity MAC and must not be used as the frame-source filter. Retain the matching MAC
  returned by authenticated `/ruuvi.json`; if it is absent, mark DHCP ERROR before serial discovery,
  capture, or reset. Never derive the expected DHCP hostname from an unverified `.env` MAC.
  After the authenticated
  baseline and HTTP probes, discover the serial port, start `tcpdump`, wait for its listening
  indication, and only then call public `SerialTransport.reset()` and observe UART until the
  complete configured `### Start mDNS: Hostname:` line, for at most 30 seconds after reset; an early
  `espressif` mDNS line does not end the wait. The reset must not flash or erase the DUT. Continue
  `tcpdump` for three seconds after the marker, then select the last complete DUT DHCP packet.
  The capture process must still be running before intentional termination; an earlier exit,
  including exit zero, is acquisition ERROR even if the partial file contains parseable packets. On a
  switched LAN, use a router-side capture, switch mirror, or TAP if the host cannot see DUT
  broadcasts.
- **Behavior:**
  - If prerequisites are present and DHCP frames from the DUT are captured: assert the _last_ Option
    12 matches both the UART mDNS hostname and the short firmware hostname, `RuuviGateway` plus the
    final two authenticated nRF52 MAC octets ([`generate_hostname()`](../../../main/ruuvi_gateway_main.c#L144)).
    Ethernet starts before setting the firmware hostname
    ([`ethernet_start()`](../../../main/ethernet.c#L382)), so an earlier frame can carry the lwIP
    default `espressif` ([`sdkconfig`](../../../sdkconfig)); that frame does not satisfy the final
    check. `.env` `gw_hostname` may be an mDNS name ending in `.local` or an IP address and is not
    the DHCP Option 12 value. Permit Option 60 only when its value repeats Option 12 in its own
    frame. Require matching pcap captured/original frame lengths for every record; a snaplen-truncated
    record or impossible length mismatch is acquisition ERROR even if a later packet is complete.
    Validate the IPv4 version and minimum 20-byte header before decoding UDP/BOOTP.
    Require UDP source/destination ports `(68, 67)` or `(67, 68)`; a DHCP-shaped payload on other
    ports is acquisition ERROR.
    Require the DHCP option end marker (`255`) in every captured packet; exhaustion of
    option bytes or zero padding without that marker is incomplete acquisition and ERROR.
    Bound option decoding by the declared UDP length within the complete IPv4 datagram;
    invalid or truncated lengths are ERROR. After END, permit only zero DHCP padding inside
    that UDP payload: any nonzero byte is unassessed data and ERROR, never PASS. Ethernet
    trailer bytes outside the declared datagram are not DHCP options. A later valid packet
    must not conceal an earlier malformed payload.
    Scan all captured frames, including colon-separated, contiguous ASCII, and binary DeviceID
    password-seed representations, and reject other unexpected or sensitive options; contribute
    PASS/FAIL to the verdict.
  - If prerequisites are **not** present (no root/capture tool/interface), the UART marker is
    absent, or no post-configuration DHCP hostname is observed: record **ERROR / undetermined** with
    the exact unmet precondition. This is **not** a PASS (absence of a captured frame is not proof
    of non-disclosure) and **not** a silent skip.
- **Effect on overall verdict:** the HTTP-disclosure core determines PASS/FAIL. Because the source
  scenario includes DHCP inspection, a full PASS covering Unit A requires the capture sub-check to
  have actually run and passed; when its precondition is unmet the overall result is **ERROR (2)**
  with a message stating the HTTP-disclosure core passed but the documented L2-capture precondition
  was not satisfied. This keeps the classification honest rather than silently weakening coverage.

Keep capture and serial tool access out of imports so the mandatory HTTP core runs without them.
Host tests must inject a fake capturer and never perform a real reset or capture.

Do not temporarily set a static IP or run a DHCP server on the ordinary test host to force this
check. `eth_dhcp=false` stops the Ethernet DHCP client; `/ruuvi.json` has no DHCP-server selector;
Wi-Fi station static-IP settings are not parsed from this JSON. A controlled network can arrange an
observable DHCP exchange separately, while preserving the test's factory-default DUT baseline.

## Mandatory shared architecture

- Use `lib.config`, `lib.evidence`, `lib.models`, and `lib.errors` for setup, full evidence logging,
  results/progress, and ERROR classification; do not duplicate those responsibilities.
- Use `GatewayClient` for fresh sessions, logged HTTP, checked JSON, redirects-disabled behavior,
  and complete interactive authentication for preflight, and staged challenge/cookie parsing for the
  unauthenticated matrix. Do not reimplement any authentication exchange or reuse preflight cookies
  in disclosure probes.
- Use applicable `GatewayApi`, `AuthMech.LAN_WEBUI_DEFAULT`, `GatewayCfgDesc`, and
  `GatewayCfgLanAuthType.DEFAULT` constants. Enumerate relevant GET routes from `API_INVENTORY` and
  use `HttpMethod`, `HttpStatus`, `HttpHeader`, and `HttpAuthScheme`; add only the explicitly
  required static/unknown/HEAD probes outside that inventory.
- Stable probe, disclosure-finding, capture, and result schemas must be typed immutable models.
  Dictionaries remain only at genuine HTTP/JSON/header mappings and use descriptor keys where
  applicable.

The shared modules are the primary implementation references; firmware sources define observable
behavior only.

Follow the `test_5_1_1_2_b.py` runner pattern: a `FunctionalTest_5_6_2_2_a` class with dependency
injection for sessions, the optional capturer, and the clock; an
`execute_test_5_6_2_2_a(...) -> RunResult` entry point; `main() -> int`; `sys.exit(main())`;
`[Step N out of M] <description>` progress (unauthenticated GET matrix, banner check, redirect
check, sensitive-pattern scan, and the DHCP sub-check as separate steps); print the log path once as
`Open log file: <path>`; print the final `Overall verdict: <PASS|FAIL|ERROR>`.

## Execution and configuration

Live execution after implementing this task (not host-side validation):

```text
cd cra/303645/tests
.venv/bin/python test_5_6_2_2_a.py
```

- No CLI arguments; load `.env` from the current working directory. Use `gw_hostname` for the DUT
  URL and `.env` `gw_mac` only for the nRF52 identity check. Identify DUT DHCP frames by the active
  Ethernet or Wi-Fi MAC obtained from the verified DUT IP.
- The mandatory HTTP core needs no elevated privilege. The DHCP capture needs `root`/`CAP_NET_RAW`,
  `tcpdump`, and an attached CH340 USB serial bridge; when absent, the capture sub-check ERRORs
  (documented precondition) rather than being skipped.
- All timeouts (HTTP request timeout via `GatewayClient`, capture window) are finite production
  constants; none are CLI options.

## Evidence log

Every invocation, including precondition failures, must create
`logs/test_5_6_2_2_a_<UTC-date-time>.log` via `EvidenceLog`, containing: test case/unit and UTC
start/end/duration; DUT config; the probe matrix with each request/response (status, all headers,
cookies, `Location`, full body); the exact `Server` banner; the sensitive-pattern set with source
citations; every pattern-scan result (including the specific probe/header/field and matched token on
any hit); the deliberately-public-metadata confirmations; the auth-challenge shape; the DHCP
sub-check prerequisites, captured frames (or the unmet precondition), and result; per-check and
overall verdicts; and exception types/tracebacks. Secrets that the test itself generates or reads
(e.g. `gw_id`) may appear in this local log; keep terminal output concise and free of secrets.

## Verdict and exit codes

| Exit code | Meaning                                                                                                                                                                                                                                    |
| :-------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `0`       | Overall PASS — no sensitive disclosure in any unauthenticated response; banner generic; redirects/401/404 as expected; DHCP capture sub-check ran and passed                                                                               |
| `1`       | A disclosure finding — a sensitive pattern appeared in an unauthenticated header/body, the `Server` banner leaked a version/build token, or a gated endpoint returned config/history/credentials instead of the expected `302`/`401`/`404` |
| `2`       | Invalid setup / precondition failure / incomplete observation — unresolvable/unreachable DUT, wrong initial auth state, malformed protocol response, or the L2-capture precondition unmet/undetermined                                     |

A gated endpoint that unexpectedly returns `200` with configuration or sensor data is a **FAIL
(1)**. An unmet DHCP-capture precondition is **ERROR (2)**, never a silent PASS. A wrong initial DUT
auth state (not `lan_auth_default`) is **ERROR (2)**, not a security FAIL.

## DUT preconditions

- Dedicated test gateway in the **initialized state** (`lan_auth_default`; API keys disabled; SoftAP
  inactive), reachable over the LAN.
- No firmware update / remote-config in progress; no other process changing configuration.
- For the DHCP sub-check: the test host is on the DUT's LAN segment with `root`/`CAP_NET_RAW`,
  `tcpdump`, and an attached CH340 USB bridge controlling this DUT; otherwise that sub-check reports
  its unmet precondition as ERROR.

The disclosure probes are unauthenticated GET/HEAD requests following an isolated authenticated
preflight. No configuration is changed; still run it on a dedicated test gateway.

## Required procedure

1. Create the evidence log; record test id/unit and UTC start.
2. Load/validate `.env`; construct `GatewayClient`.
3. Complete the mandatory default login and authenticated identity/default checks. Discard the
   authorized session for probing; an unauthenticated challenge alone cannot verify the baseline.
4. Execute the unauthenticated GET/HEAD probe matrix with redirects disabled; assert expected
   statuses (302/401/404/200/400) and the differing cookie behavior.
5. Assert the generic `Server: Ruuvi Gateway` banner (via GET) with no version/build token.
6. Run the sensitive-pattern scan over all headers and bodies; record any finding.
7. Confirm deliberately public metadata (banner, hostname, static app, ephemeral challenge/ECDH).
8. Start the precondition-gated DHCP capture, wait until `tcpdump` is listening, reset the DUT
   through the discovered CH340 port, and inspect the boot DHCP frame (or record ERROR).
9. Aggregate PASS/FAIL/ERROR; write the verdict; print `Overall verdict: ...`.

## Host-side implementation tests

Follow `test_test_5_1_1_2_b.py` conventions: `unittest`, fake sessions/responses, an injected fake
capturer, temporary directories, and an injected clock. No real network or capture. Fake-server
challenge, cookie, header, body, and wire expectations must be independently authored fixtures; the
fake server must never call client code under test to generate expected values. Cover at least:

- the sensitive-pattern set and source-grounded field names;
- clean PASS: all probes return expected statuses, generic banner, no sensitive pattern, capture
  sub-check (faked) passes → PASS/exit 0;
- a config/credential leak in a `200` body, a bearer token in a header, a PSK/private-key marker, or
  decoded sensor history each → FAIL/exit 1;
- a `Server` header carrying a version/build hash → FAIL/exit 1;
- `HEAD /` asserted as `400` (not a banner); unauthenticated default-mode LAN `GET /info.json` and
  unknown `*.json` asserted as `302` before dispatch, with `404` rejected;
- `302` responses captured (not followed) with correct `Location` and cookie-present/omitted
  behavior; a gated endpoint returning `200`+config → FAIL;
- ephemeral challenge/ECDH values, public JavaScript field names, and the hostname are **not**
  flagged as sensitive; authenticated preflight responses never enter the disclosure corpus;
- preflight errors stop the matrix, and every matrix request is cookie/Authorization-free;
- wrong initial auth state, unreachable DUT, malformed response → ERROR/exit 2;
- DHCP sub-check: passing capture (fake), a sensitive DHCP option → FAIL, and an unmet precondition
  / no-frame-observed → ERROR (never a silent PASS/skip);
- the final short firmware hostname in Option 12 must match the UART marker even when `.env` has an
  mDNS `.local` suffix; an early `espressif` frame alone is ERROR, and an unrelated Option 60 value
  in any captured frame is FAIL;
- `tcpdump` readiness precedes serial reset; readiness failure must not reset the DUT, and reset
  failure or DHCP timeout must terminate the capture process and produce ERROR;
- evidence-log naming/content and terminal progress match the shared pattern;
- PASS/FAIL/ERROR exit-code aggregation.

## References

- Source: [`Self_Assessment_Test_Group_5_6-2.md`](../Self_Assessment_Test_Group_5_6-2.md) — test
  case 5.6-2-2 Unit A (lines 59-78), 5.6-2-1 conceptual (lines 18-46), summary line 88.
- [`IXIT 28-LogIntf`](../ixit_28-LogIntf.md) — `LogIntf-HTTP-Server` disclosed info (lines 27-46).
- [`IXIT 10-SecParam`](../ixit_10-SecParam.md) — credential/key/token field names (WebUI creds
  254-295, Wi-Fi 307-336, remote-cfg 355-411, custom/stat assets 404-532, bearer tokens 591-641,
  DeviceID 212-253).
- [`IXIT 1-AuthMech`](../ixit_1-AuthMech.md) — auth hashes/tokens/ECDH (ephemeral vs secret).
- [`IXIT 21-PersData`](../ixit_21-PersData.md) — personal data (IP/MAC footprints, custom-target
  secrets 290-360, BLE sensor telemetry 361+).
- Firmware: [`docs/http_api.md`](../../../docs/http_api.md) (auth model, redirects, `/info.json`
  hotspot-only, static fallback);
  [`http_server_handle_req()` method dispatch](../../../components/esp32-wifi-manager/src/http_server_handle_req.c#L693)
  (HEAD -> 400);
  [`http_server_netconn_resp.c`](../../../components/esp32-wifi-manager/src/http_server_netconn_resp.c#L462)
  (`Server: Ruuvi Gateway`).
- Automation classification:
  [`Functional_Test_Automation_Assessment.md`](../Functional_Test_Automation_Assessment.md) (5.6-2-2
  A — AUTO).
- Contract & patterns: [`task_5_1_1_2_b.md`](task_5_1_1_2_b.md),
  [`task_5_1_5_2_b.md`](task_5_1_5_2_b.md), [`cra/303645/tests/lib`](../tests/lib/README.md).

## Runner naming

Use `FunctionalTest_5_6_2_2_a` and `execute_test_5_6_2_2_a`; retain `main() -> int` as the
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
