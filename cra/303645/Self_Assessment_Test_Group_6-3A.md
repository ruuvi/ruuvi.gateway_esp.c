# Test group 6-3A: Withdrawing Consumer Consent for Personal Data Processing

Provision 6-3A — Status: **M**. Related IXIT: `IXIT 21-PersData`, `IXIT 25-DelFunc`,
`IXIT 26-UserDec`.

---

## Test case 6-3A-1 (conceptual)

**Purpose**: To conceptually assess whether the documentation under "Withdrawing Consent" in
`IXIT 21-PersData` accurately describes how consumers can withdraw consent to the processing of
personal data at any time by configuring IoT device and service functionality appropriately (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Conceptual Assessment of Consent Withdrawal Declarations

| Personal Data Category ID (`IXIT 21-PersData`) | Consent Withdrawal Mechanism ("Withdrawing Consent")                                                                                                                                                                                 | Technical Method & Interface Configuration                                                                                                                                       | Assessment of Withdrawal Capability                                                                                      | Unit Verdict |
|:-----------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Disconnect physical network media OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                                              | Unplugging Ethernet/Wi-Fi link halts active packet processing; factory reset executes a low-level sector erase across `nvs` partition, purging static IP configurations.         | **Adequate.** Instantly terminates active network IP tracking and erases persistent station settings.                    |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Disconnect physical network media OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                                              | Physical disconnection instantly terminates L2 Ethernet frame switching and ARP resolution; factory reset clears Wi-Fi station association parameters.                           | **Adequate.** Instantly ceases local Layer 2 MAC exposure on the client's network.                                       |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Toggle off outbound telemetry targets OR disable diagnostics reporting in Web-UI Step 8 (`UserDec-8`).                                                                                                                               | Web-UI toggle deactivates status POST loops, stopping local memory consumption of `DEVICEID` for outbound HMAC payload signing.                                                  | **Adequate.** Completely halts payload signing loops and cloud transmission.                                             |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Disable Ruuvi Cloud / custom telemetry relays in Web-UI Step 8 (`UserDec-8`) OR disconnect network links.                                                                                                                            | Toggling telemetry targets to `Disabled` updates `ruuvi.json` configuration, halting outbound JSON envelope generation containing `gw_mac`.                                      | **Adequate.** Immediately terminates transmission of gateway MAC identifiers to remote servers.                          |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Clear credential text fields / private keys in Web-UI form OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                     | Web-UI form clearing or 7-second button hold formats `nvs` and `gw_cfg_def` partitions, wiping API tokens, passwords, and private mTLS keys.                                     | **Adequate.** Completely eradicates custom authentication credentials and terminates private mTLS handshakes.            |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | **On-Device:** Disable telemetry relays in Web-UI (`UserDec-8`) OR execute factory reset (`DelFunc-Hardware-Factory-Reset`).<br>**Associated Services:** Submit cloud account deletion request (`DelFunc-Service-Account-Deletion`). | Web-UI toggles immediately halt telemetry POST loops; cloud account deletion invalidates user tokens and queues historical telemetry records for permanent database eradication. | **Adequate.** Halts real-time telemetry forwarding locally and permanently purges stored history on associated services. |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Device-Level Withdrawal:** Consumers can withdraw consent for on-device personal data
     processing at any time by re-configuring settings via the LAN Web-UI (e.g. toggling off cloud
     relays, clearing custom target secrets, or disabling statistics reporting) or by executing a
     physical 7-second press of the `CONFIGURE` button (`DelFunc-Hardware-Factory-Reset`).
  2. **Service-Level Withdrawal:** Consumers can withdraw consent for associated cloud service data
     processing at any time by requesting account termination (`DelFunc-Service-Account-Deletion`)
     via the Ruuvi Station app or web portal.
  3. **Documentation Accuracy:** The "Withdrawing Consent" declarations in `IXIT 21-PersData`
     accurately describe appropriate device and service configuration workflows for every category
     of personal data.

* **Unit A Verdict**: **PASS**

---

## Test case 6-3A-2 (functional)

**Purpose**: To functionally verify on the DUT and associated services that consumer consent for
processing personal data can be withdrawn at any time strictly as described in "Withdrawing Consent"
in `IXIT 21-PersData` (`a`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Functional Verification of Consent Withdrawal Execution

**Testing Methodology**: The test laboratory established active processing states for all personal
data categories, executed the documented consent withdrawal procedures (toggling off Web-UI
telemetry controls, disconnecting network media, executing hardware factory resets, and submitting
cloud account deletion requests), and audited network packet traffic, local memory states, and cloud
backend databases.

| Personal Data Category ID (`IXIT 21-PersData`) | Executed Consent Withdrawal Action                                                                                | Observed Device / Service Behavior & Post-Withdrawal State                                                                                               | Functional Verification Assessment                                                                  | Unit Verdict |
|:-----------------------------------------------|:------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Disconnect Ethernet cable; execute 7-second hold of `CONFIGURE` button.                                           | Ethernet link drops immediately. Factory reset formats `nvs` flash; post-reboot flash dump confirms static IP profiles are cleared.                      | **Matches IXIT.** IP packet processing halts; persistent IP configuration is erased.                |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Disconnect Ethernet cable and Wi-Fi station connection.                                                           | Interface PHY enters idle state; traffic sniffer confirms zero L2 ARP/DHCP frame emissions.                                                              | **Matches IXIT.** Local MAC exposure ceases instantly upon link disconnection.                      |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Toggle off diagnostics reporting in Web-UI (`UserDec-8`).                                                         | HTTP POST tasks targeting `https://network.ruuvi.com/status` terminate; local HMAC generation tasks halt.                                                | **Matches IXIT.** Outbound status tracking and payload signing cease immediately.                   |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Toggle Ruuvi Cloud telemetry relay `Disabled` in Web-UI Step 8.                                                   | Async HTTP communication tasks halt; packet sniffer confirms zero JSON envelopes containing `gw_mac` leave the interface.                                | **Matches IXIT.** Remote transmission of gateway MAC identifier halts immediately.                  |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Clear custom HTTP API keys in Web-UI and trigger factory reset.                                                   | Web-UI updates `ruuvi.json`; factory reset formats `gw_cfg_def`. Flash dump confirms mTLS private keys and Bearer tokens are erased.                     | **Matches IXIT.** Custom credentials are fully purged and client-side mTLS handshakes terminate.    |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Disable cloud relays in Web-UI; authorize cloud account deletion email link (`DelFunc-Service-Account-Deletion`). | Outbound telemetry POSTs stop immediately. Cloud portal invalidates session tokens; API queries confirm historical telemetry is unaccessible and purged. | **Matches IXIT.** Local telemetry forwarding halts and cloud-stored history is permanently deleted. |   **PASS**   |

**Assessment Justification**: Functional testing confirms that consumer consent for processing
personal data can be withdrawn at any time across all categories in `IXIT 21-PersData`. Configuring
device functionality via Web-UI controls, disconnecting physical media, or executing a 7-second
hardware reset instantly terminates on-device data processing and erases persistent credentials.
Executing cloud account deletion permanently purges personal telemetry history from associated
services.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-3A-1 & 6-3A-2

| Test Case         | Purpose / Focus                            | Assessment Summary                                                                                                                                                                               | Unit Verdict |
|:------------------|:-------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-3A-1 Unit a** | Conceptual Withdrawal Description Audit    | "Withdrawing Consent" in `IXIT 21-PersData` accurately describes device and service configuration workflows for withdrawing consent at any time across all 6 personal data categories.           |   **PASS**   |
| **6-3A-2 Unit a** | Functional Consent Withdrawal Verification | Functional tests confirm that configuring device settings, disconnecting media, or submitting cloud account deletion requests immediately halts data processing and erases stored personal data. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-3A of `ETSI EN 303 645`. Consumers can
withdraw consent for personal data processing (`IXIT 21-PersData`) at any time by configuring device
and service functionality appropriately. On the device, consumers can withdraw consent by adjusting
Web-UI settings (disabling telemetry relays, clearing custom server secrets, or toggling off
diagnostics), disconnecting network media, or executing a low-level physical flash memory formatting
loop (`DelFunc-Hardware-Factory-Reset`). On associated services, consumers can withdraw consent by
requesting account termination (`DelFunc-Service-Account-Deletion`), which revokes session tokens
and permanently purges cloud-stored telemetry records.

**Group Verdict**: **PASS**
