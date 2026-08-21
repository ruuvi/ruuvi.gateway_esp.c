# Test group 5.12-1: Installation and Maintenance Involve Minimal and Secure Decisions

Provision 5.12-1 — Status: **R**. Related IXIT: `IXIT 26-UserDec`.

---

## Test case 5.12-1-1 (conceptual)

**Purpose**: To conceptually assess whether every installation and maintenance decision taken by the
user in `IXIT 26-UserDec` is necessary for operation (`a`), and whether every default value follows
security best practice (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Assessment of Decision Necessity and Security Best Practice Defaults

| User Decision ID (`IXIT 26-UserDec`)       | Operational Decision Focus                   | Operational Necessity Assessment (Unit a)                                                  | Default Option & Security Best Practice Compliance (Unit b)                                                                    | Unit Verdict |
|:-------------------------------------------|:---------------------------------------------|:-------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`UserDec-1-Network-Medium-Selection`**   | Step 1: Interface Medium (Ethernet vs Wi-Fi) | **Necessary.** Connects device to physical operational network environment.                | **Ethernet (Auto-detect).** Prefers wired interface when cable is present, minimizing wireless attack surface.                 |   **PASS**   |
| **`UserDec-2-Interface-Configuration`**    | Step 2: Addressing Rules (DHCP vs Static IP) | **Necessary.** Establishes local IP routing parameters.                                    | **Use DHCP.** Standard dynamic network onboarding following standard router infrastructure rules.                              |   **PASS**   |
| **`UserDec-3-Onboarding-Firmware-Update`** | Step 3: Out-of-the-Box Firmware Check        | **Necessary.** Evaluates software version prior to operational deployment.                 | **User Intent Required.** Prominently alerts user if a newer signed firmware image is available before setup completion.       |   **PASS**   |
| **`UserDec-4-Automatic-Config-Download`**  | Step 4: Remote Provisioning Manifest         | **Necessary.** Enables fleet management profile fetching for enterprise deployments.       | **Disabled.** Prevents unauthorized or untrusted remote profile injection on unconfigured units.                               |   **PASS**   |
| **`UserDec-5-Automatic-Updates`**          | Step 5: Patch Delivery & Schedule            | **Necessary.** Configures long-term firmware maintenance behavior.                         | **Auto update.** Enforces automated background security patch checks and updates (ETSI EN 303 645 Provision 5.3-13).           |   **PASS**   |
| **`UserDec-6-Remote-Access-Settings`**     | Step 6: Web-UI & M2M API Credentials         | **Necessary.** Sets administrative access boundaries and M2M API key permissions.          | **Unique Hardware Password.** Uses unique `DEVICEID` per unit (Clause 5.1-1 compliance); M2M API keys are disabled by default. |   **PASS**   |
| **`UserDec-7-Cloud-Options`**              | Step 7: Telemetry Channel Selection          | **Necessary.** Selects between standard out-of-the-box cloud or custom routing.            | **Ruuvi Cloud (recommended).** Directs data to official secure HTTPS endpoints (`SecComMech-TLS`).                             |   **PASS**   |
| **`UserDec-8-Custom-Server-Routing`**      | Step 8: Multi-Target Endpoint Routing        | **Necessary.** Configures custom HTTP/MQTT targets for advanced deployments.               | **Cloud Active / Stats On.** Enforces HTTPS for official telemetry; custom third-party targets default to disabled.            |   **PASS**   |
| **`UserDec-9-Time-Sync-Options`**          | Step 9: NTP Clock Synchronization            | **Necessary.** Synchronizes system wall-clock critical for TLS validation and log digests. | **Default NTP Servers.** Targets public NTP pools (`time.google.com`, `cloudflare.com`, `ntp.org`).                            |   **PASS**   |
| **`UserDec-10-Bluetooth-Scanning`**        | Step 10: Radio Filter Rules                  | **Necessary.** Defines scanning scope and hardware MAC filtering rules.                    | **Listen to Ruuvi sensors only.** Filters out non-Ruuvi beacons, minimizing ambient radio tracking and data noise.             |   **PASS**   |

**Assessment Justification**: All 10 user decisions in `IXIT 26-UserDec` are operationally necessary
to deploy and maintain the gateway within diverse network environments. Every default configuration
value follows security best practices: Web-UI access defaults to a unique per-device password (
`UserDec-6`), automatic security updates are enabled by default (`UserDec-5`), unauthenticated
auto-configuration is disabled by default (`UserDec-4`), and radio scanning defaults to restricted
manufacturer frames (`UserDec-10`).

**Verdict**: **PASS**

---

## Test case 5.12-1-2 (functional)

**Purpose**: To functionally verify on the DUT that all decisions in `IXIT 26-UserDec` are triggered
as documented (`a`), prominently requested during installation/maintenance (`b`), understandable for
non-technical users under Clause D.3 (`c`), and conformant to the options declared in IXIT (`d`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C & D: Functional Decision Triggering, Prominence, Understandability, and Conformance

**Testing Methodology**: The test laboratory executed a complete out-of-the-box onboarding sequence
via the captive portal hotspot and inspected post-installation maintenance views in the LAN Web-UI,
auditing screen prominence, non-technical phrasing (Clause D.3), and option conformance against
`IXIT 26-UserDec`.

| User Decision ID (`IXIT 26-UserDec`) | Trigger & Prominence Audit (Units a & b)                                                                 | Understandability Evaluation (Unit c - Clause D.3)                                                            | IXIT Option Conformance Audit (Unit d)                                                                            | Unit Verdict |
|:-------------------------------------|:---------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------|:------------:|
| **`UserDec-1` to `UserDec-3`**       | Triggered sequentially in onboarding wizard Steps 1–3. Prominently presented on dedicated setup screens. | Uses clear, non-technical language (*"Ethernet"*, *"Wi-Fi"*, *"Install Update"*).                             | Options match IXIT: Ethernet autodetect, DHCP/Static IP choices, and update triggers.                             |   **PASS**   |
| **`UserDec-4` & `UserDec-5`**        | Triggered in wizard Steps 4–5 and accessible via LAN Web-UI Maintenance Menu.                            | Prominently explains auto-config download and patch schedules in plain terms (*"Auto update (recommended)"*). | Options match IXIT: Auto/Manual update toggles, release channels (production/beta), and day/time filters.         |   **PASS**   |
| **`UserDec-6`**                      | Triggered in wizard Step 6 and Web-UI Account Settings.                                                  | Prominently highlights administrative password protection and M2M API key read/write toggles.                 | Options match IXIT: Default unique password, custom password, disable remote config, and M2M API keys.            |   **PASS**   |
| **`UserDec-7` & `UserDec-8`**        | Triggered in wizard Steps 7–8 and Web-UI Data Routing Panel.                                             | Prominently presents standard vs custom routing (*"Ruuvi Cloud (recommended)"* vs custom HTTP/MQTT).          | Options match IXIT: Ruuvi Cloud relay toggle, custom HTTP/HTTPS, custom MQTT/MQTTS/WS/WSS, stats toggle.          |   **PASS**   |
| **`UserDec-9` & `UserDec-10`**       | Triggered in wizard Steps 9–10 and Web-UI Advanced Settings.                                             | Prominently explains time sync and radio filtering (*"Listen to Ruuvi sensors only"*).                        | Options match IXIT: Default/Custom NTP pools, PHY selectors (1M/2M/Coded), and Whitelist/Blacklist address masks. |   **PASS**   |

**Assessment Justification**: Functional audit of the onboarding wizard and LAN Web-UI confirms that
all user decisions are prominently displayed step-by-step without hidden or obscure security
sub-menus. All options use plain, non-technical language easily understandable by non-technical
users (Clause D.3), and the functional UI implementation matches `IXIT 26-UserDec` in every detail.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.12-1-1 & 5.12-1-2

| Test Case           | Purpose / Focus                 | Assessment Summary                                                                                                   | Unit Verdict |
|:--------------------|:--------------------------------|:---------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.12-1-1 Unit a** | Necessity of User Decisions     | All 10 user decisions in `IXIT 26-UserDec` are operationally necessary for network setup, security, or data routing. |   **PASS**   |
| **5.12-1-1 Unit b** | Security Best Practice Defaults | Default values follow security best practice (unique passwords, auto-updates enabled, radio scanning filtered).      |   **PASS**   |
| **5.12-1-2 Unit a** | Triggering Assessment           | All onboarding and maintenance decisions trigger correctly as declared in `IXIT 26-UserDec`.                         |   **PASS**   |
| **5.12-1-2 Unit b** | Prominence Assessment           | Decisions are prominently presented step-by-step during the setup wizard and Web-UI maintenance flows.               |   **PASS**   |
| **5.12-1-2 Unit c** | Understandability (Clause D.3)  | Descriptions and options use plain, clear language suitable for users with limited technical knowledge.              |   **PASS**   |
| **5.12-1-2 Unit d** | IXIT Option Conformance         | Functional UI implementation matches the options, defaults, and triggers in `IXIT 26-UserDec` precisely.             |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.12-1 of `ETSI EN 303 645`. The
installation and maintenance workflows (`IXIT 26-UserDec`) involve minimal, necessary user decisions
presented step-by-step within an intuitive onboarding wizard. Every configuration parameter defaults
to a secure-by-default state—including unique per-device Web-UI passwords, automated security patch
delivery, disabled remote auto-configuration downloads, encrypted cloud telemetry relays, and
restricted Bluetooth radio scanning. Functional testing confirms that all decision steps are
prominently displayed, easy to understand for non-technical users (Clause D.3), and completely
conformant to technical documentation.

**Group Verdict**: **PASS**
