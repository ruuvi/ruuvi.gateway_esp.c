# Test group 6-2: Valid Consent for Personal Data Processing

Provision 6-2 — Status: **M**. Related IXIT: `IXIT 21-PersData`, `IXIT 26-UserDec`.

---

## Test case 6-2-1 (conceptual)

**Purpose**: To conceptually assess whether the opt-in choice for processing personal data in
`IXIT 21-PersData` is given freely, obviously, and explicitly according to the description of "
Obtaining Consent" (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Conceptual Assessment of Consent Criteria (Free, Obvious, Explicit)

| Personal Data Category ID (`IXIT 21-PersData`) | Consent Mechanism & Trigger Vector                                                               | Free Choice Evaluation                                                                            | Obviousness Evaluation                                                                          | Explicit Opt-In Evaluation                                                                                         | Unit Verdict |
|:-----------------------------------------------|:-------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Physical network attachment or manual static IP configuration in Step 2 of wizard (`UserDec-2`). | **Free.** User can choose Ethernet, Wi-Fi, or operate offline without WAN access.                 | **Obvious.** Clearly presented on interface setup wizard screens and settings panels.           | **Explicit.** User explicitly connects cabling or submits station credentials in the setup wizard.                 |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Physical Ethernet connection or Wi-Fi station setup (`UserDec-1` / `UserDec-2`).                 | **Free.** User is not forced to bridge the device onto a local network.                           | **Obvious.** Local L2 network frame attachment is standard for network bridging.                | **Explicit.** User explicitly provisions network credentials to bring the interface online.                        |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | On-device root password usage & HMAC cloud registration (`UserDec-6` / `UserDec-7`).             | **Free.** User can alter Web-UI password and disable outbound cloud telemetry.                    | **Obvious.** Printed on physical casing label and referenced in setup wizard Step 6.            | **Explicit.** User explicitly submits setup wizard configuration and registers cloud targets.                      |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Outbound telemetry endpoint configuration in Step 7 (`UserDec-7` / `UserDec-8`).                 | **Free.** User can disable Ruuvi Cloud relay and route telemetry strictly to local API endpoints. | **Obvious.** Displayed in Web-UI settings as `gw_mac` origin identifier.                        | **Explicit.** User explicitly selects telemetry destinations during onboarding Step 7.                             |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Manual credential entry in custom server settings (`UserDec-8`).                                 | **Free.** Entering custom target passwords, API keys, or mTLS certificates is entirely voluntary. | **Obvious.** Dedicated form fields in Web-UI custom server dashboard.                           | **Explicit.** User manually inputs/scripts credential submission into NVS storage partitions (`nvs`/`gw_cfg_def`). |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Cloud options selection in Step 7 (`UserDec-7`) & scanning rules in Step 10 (`UserDec-10`).      | **Free.** User can disable Ruuvi Cloud, disable custom targets, or restrict radio scanning rules. | **Obvious.** Step 7 explicitly asks user whether to enable Ruuvi Cloud or custom server relays. | **Explicit.** User explicitly selects `Ruuvi Cloud (recommended)` or configures custom HTTP/MQTT server URLs.      |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Freely Given:** Consumers are never forced to accept telemetry tracking or cloud streaming to
     operate the gateway locally. The setup wizard (`IXIT 26-UserDec`) allows users to run
     local-only REST API processing, disable statistics reporting, or restrict radio scanning rules.
  2. **Obviously Presented:** Every personal data processing pathway is prominently displayed during
     the step-by-step onboarding wizard screens (Steps 2, 6, 7, 8, and 10) rather than hidden in
     obscure sub-menus.
  3. **Explicitly Actioned:** Outbound personal data transmission requires active user configuration
     steps (e.g. submitting Wi-Fi credentials, selecting cloud options, or entering custom HTTP/MQTT
     target URLs). No pre-checked default hidden background transmission occurs without explicit
     user network provisioning.

* **Unit A Verdict**: **PASS**

---

## Test case 6-2-2 (functional)

**Purpose**: To functionally assess on the DUT and associated services whether consumer consent for
processing personal data is obtained freely, obviously, and explicitly as described in
`IXIT 21-PersData` (`a`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Functional Consent Verification

**Testing Methodology**: The test laboratory executed the complete onboarding wizard sequence (
`IXIT 26-UserDec`), inspecting Web-UI consent prompts, default selection states, and network packet
emissions across Ethernet/Wi-Fi drops to verify that personal data is transmitted only following
explicit, obvious, and free user opt-in actions.

| Personal Data Category ID (`IXIT 21-PersData`) | Tested Onboarding / Web-UI Interaction                                            | Observed Device Behavior & Consent Enforcement                                                                                                     | Functional Consent Verification Assessment                                                        | Unit Verdict |
|:-----------------------------------------------|:----------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Unboxed DUT boot without network connection vs. active setup wizard provisioning. | Zero WAN IP packets are emitted until the user explicitly plugs in an Ethernet cable or provisions Wi-Fi station credentials.                      | **Matches IXIT.** IP metadata processing occurs only upon explicit user network attachment.       |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Ethernet cable disconnection test.                                                | Unplugging the network cable immediately terminates L2 MAC broadcasting and ARP resolution on the LAN.                                             | **Matches IXIT.** MAC exposure is bound cleanly to physical interface activation states.          |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Web-UI Step 6 authentication setup & HMAC signing toggle.                         | The raw 64-bit FICR `DEVICEID` is consumed locally for HMAC tag calculation only when outbound telemetry targets are active.                       | **Matches IXIT.** Raw identifier remains isolated locally and is never transmitted over networks. |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Onboarding Step 7 (`UserDec-7` Cloud Options).                                    | Outbound JSON telemetry frames containing `gw_mac` are transmitted strictly after the user selects `Ruuvi Cloud` or configures custom server URLs. | **Matches IXIT.** Radio MAC identity mapping requires explicit destination setup.                 |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Leave custom HTTP/MQTT server fields blank vs. inputting API keys/mTLS certs.     | Blank fields result in zero credential processing or mTLS handshakes. Credentials are encrypted in NVS only upon explicit form submission.         | **Matches IXIT.** Secret processing occurs strictly upon explicit user definition.                |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Toggle cloud telemetry relay `Disabled` in Web-UI Step 8.                         | Disabling cloud targets immediately halts outbound HTTPS POST requests to `https://network.ruuvi.com/record`. Radio scanning remains local-only.   | **Matches IXIT.** Telemetry transmission strictly obeys user opt-in/opt-out selections.           |   **PASS**   |

**Assessment Justification**: Functional testing confirms that consumer consent for processing
personal data is obtained strictly as declared in `IXIT 21-PersData`. Outbound personal data
transmission to associated cloud services or custom targets requires explicit, obvious opt-in
actions during the setup wizard or Web-UI maintenance menus. Toggling telemetry targets off or
executing a hardware factory reset instantly terminates personal data processing and removes stored
credentials.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-2-1 & 6-2-2

| Test Case        | Purpose / Focus                   | Assessment Summary                                                                                                                                 | Unit Verdict |
|:-----------------|:----------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-2-1 Unit a** | Conceptual Consent Criteria Audit | Consent for all 6 personal data categories is freely given, obviously presented, and explicitly actioned in `IXIT 26-UserDec`.                     |   **PASS**   |
| **6-2-2 Unit a** | Functional Consent Verification   | Functional tests confirm data processing occurs strictly following explicit user opt-in actions; disabling targets halts transmission immediately. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-2 of `ETSI EN 303 645`. Consumer consent
for processing personal data (`IXIT 21-PersData`) is obtained in a valid, explicit, obvious, and
freely given manner. During the onboarding wizard (`IXIT 26-UserDec`), users are presented with
clear choices regarding network connection, administrative authentication, cloud telemetry relay,
custom destination setup, and Bluetooth scanning filters. Personal data transmission to associated
cloud services requires explicit user opt-in configuration, and users retain complete control to
disable telemetry relay or withdraw consent at any time via the Web-UI or a physical hardware
factory reset (`DelFunc-Hardware-Factory-Reset`).

**Group Verdict**: **PASS**
