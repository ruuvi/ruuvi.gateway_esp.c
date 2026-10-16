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

| Personal Data Category ID (`IXIT 21-PersData`) | Consent Withdrawal Mechanism ("Withdrawing Consent")                                                                                                                                                                                 | Technical Method & Interface Configuration                                                                                                                                                                                                                                                                              | Assessment of Withdrawal Capability                                                                                                   | Unit Verdict |
|:-----------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Disconnect physical network media OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                                              | Disconnecting Ethernet/Wi-Fi links halts active IP frame routing immediately. Executing a hardware factory reset via the physical `CONFIGURE` button formats the `nvs` partition, purging stored static IP parameters.                                                                                                  | **Adequate.** Instantly terminates active network IP tracking and wipes persistent station configuration.                             |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Disconnect physical network media OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                                              | Physical link disconnection terminates Layer 2 Ethernet frame switching and ARP resolution. A hardware factory reset purges saved Wi-Fi station network profiles, disconnecting the interface from the local network.                                                                                                   | **Adequate.** Ceases local Layer 2 MAC exposure on the client network.                                                                |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Toggle off outbound telemetry targets OR disable diagnostics reporting in Web-UI Step 8 (`UserDec-8`).                                                                                                                               | Web-UI toggle deactivates HTTP status POST tasks, terminating runtime memory access to the silicon `DEVICEID` for outbound HMAC payload signing.                                                                                                                                                                        | **Adequate.** Completely halts payload signing loops and cloud telemetry transmission.                                                |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Disable Ruuvi Cloud / custom telemetry relays in Web-UI Step 8 (`UserDec-8`) OR disconnect network links.                                                                                                                            | Toggling telemetry targets to `Disabled` updates `ruuvi.json` in NVS, halting the construction and transmission of outbound JSON telemetry frames containing `gw_mac`.                                                                                                                                                  | **Adequate.** Immediately terminates transmission of gateway MAC identifiers to remote server destinations.                           |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Clear credential form fields and private keys in the Web-UI OR execute hardware factory reset (`DelFunc-Hardware-Factory-Reset`).                                                                                                    | Clearing and saving Web-UI fields removes the current credential values from the active configuration. Previous values may remain in append-only NVS records; this does not format either partition. Hardware factory reset formats `nvs` and `gw_cfg_def` to erase stored credentials, certificates, and private keys. | **Adequate.** Clearing active values stops their use; hardware factory reset is required to erase previous values retained in flash.  |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | **On-Device:** Disable telemetry relays in Web-UI (`UserDec-8`) OR execute factory reset (`DelFunc-Hardware-Factory-Reset`).<br>**Associated Services:** Submit cloud account deletion request (`DelFunc-Service-Account-Deletion`). | Web-UI toggle halts outbound telemetry POST loops immediately. A cloud account deletion request terminates active authorization sessions and queues historical sensor telemetry records for permanent database eradication.                                                                                             | **Adequate.** Halts real-time telemetry forwarding locally and permanently purges stored sensor history on associated cloud services. |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Device-Level Withdrawal:** Consumers can withdraw consent for on-device personal data
     processing at any time by configuring settings via the local Web-UI (e.g., toggling off cloud
     relays, clearing custom endpoint credentials, or disabling diagnostic status reporting) or by
     executing a physical hardware factory reset (`DelFunc-Hardware-Factory-Reset`) using the
     `CONFIGURE` button.
  2. **Service-Level Withdrawal:** Consumers can withdraw consent for associated cloud service data
     processing at any time by initiating account deletion (`DelFunc-Service-Account-Deletion`)
     via the Ruuvi Station application or web portal.
  3. **Documentation Accuracy:** The "Withdrawing Consent" declarations in `IXIT 21-PersData`
     accurately and comprehensively describe appropriate device and service configuration workflows
     for every cataloged category of personal data.

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
telemetry controls, disconnecting physical network media, executing hardware factory resets, and
submitting cloud account deletion requests), and audited network packet traffic, local memory
states, and cloud backend database records.

| Personal Data Category ID (`IXIT 21-PersData`) | Executed Consent Withdrawal Action                                                                                | Observed Device / Service Behavior & Post-Withdrawal State                                                                                                                                                                                                                                                                                            | Functional Verification Assessment                                                                  | Unit Verdict |
|:-----------------------------------------------|:------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Disconnect Ethernet cable; execute hardware factory reset via `CONFIGURE` button.                                 | Ethernet link drops immediately. Pressing the button for ~5–6 s triggers a restart; maintaining the hold through early boot until the status LED blinks red 200 ms on / 200 ms off (~11 s total) executes partition erasure. Releasing the button triggers another restart into factory defaults. Flash dump confirms static IP profiles are cleared. | **Matches IXIT.** IP packet processing halts; persistent IP configuration is completely erased.     |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Disconnect Ethernet cable and Wi-Fi station connection.                                                           | Interface PHY enters idle state; traffic monitoring confirms zero Layer 2 ARP/DHCP frame emissions on the LAN.                                                                                                                                                                                                                                        | **Matches IXIT.** Local MAC exposure ceases instantly upon physical link disconnection.             |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Toggle off diagnostics reporting in Web-UI (`UserDec-8`).                                                         | HTTP POST tasks targeting `https://network.ruuvi.com/status` terminate; local HMAC generation tasks halt.                                                                                                                                                                                                                                             | **Matches IXIT.** Outbound status tracking and payload signing cease immediately.                   |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Toggle Ruuvi Cloud telemetry relay `Disabled` in Web-UI Step 8.                                                   | Asynchronous HTTP communication tasks halt; packet analysis confirms zero JSON envelopes containing `gw_mac` leave the network interface.                                                                                                                                                                                                             | **Matches IXIT.** Remote transmission of gateway MAC identifier halts immediately.                  |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Clear custom HTTP API keys in Web-UI and trigger hardware factory reset.                                          | Web-UI updates `ruuvi.json`; hardware factory reset formats `gw_cfg_def` and `nvs`. Flash memory inspection confirms mTLS private keys and Bearer tokens are erased.                                                                                                                                                                                  | **Matches IXIT.** Custom credentials are fully purged and client-side mTLS handshakes terminate.    |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Disable cloud relays in Web-UI; authorize cloud account deletion email link (`DelFunc-Service-Account-Deletion`). | Outbound telemetry POST transmissions stop immediately. Cloud service terminates active sessions; backend database queries confirm historical sensor telemetry is inaccessible and permanently purged.                                                                                                                                                | **Matches IXIT.** Local telemetry forwarding halts and cloud-stored history is permanently deleted. |   **PASS**   |

* **Functional Assessment Justification**:
  Functional testing confirms that consumer consent for processing personal data can be withdrawn at
  any time across all categories declared in `IXIT 21-PersData`. Adjusting device functionality via
  Web-UI settings, disconnecting physical media, or executing the hardware factory reset
  (`DelFunc-Hardware-Factory-Reset`) terminates on-device data processing and clears persistent
  credentials. Executing cloud account deletion permanently purges personal telemetry history from
  associated cloud services.

* **Verdict**: **PASS**

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
diagnostics), disconnecting network media, or executing a low-level physical flash memory partition
erasure (`DelFunc-Hardware-Factory-Reset`) initiated by holding the `CONFIGURE` button (~5–6 seconds
until restart, held into early boot until the red 200 ms on / 200 ms off status LED pattern signals
completion at ~11 seconds total elapsed time, then released to trigger another restart into factory
default state). On associated services, consumers can withdraw consent by requesting account
termination (`DelFunc-Service-Account-Deletion`), which terminates active sessions and permanently
purges cloud-stored telemetry records.

**Group Verdict**: **PASS**
