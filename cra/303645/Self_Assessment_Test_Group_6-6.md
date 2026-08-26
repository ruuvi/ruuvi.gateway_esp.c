# Test group 6-6: Personal Data Minimization, Retention Duration, and Disposal

Provision 6-6 — Status: **R**. Related IXIT: `IXIT 21-PersData`, `IXIT 25-DelFunc`.

---

## Test case 6-6-1 (conceptual)

**Purpose**: To conceptually assess whether the processing of each personal data category in
`IXIT 21-PersData` is necessary for the defined purpose (`a`), whether the processing duration in "
Lifecycle" is reasonable (`b`), and whether the disposal process described in "Lifecycle" is
suitable to delete personal data from the DUT or associated services (`c`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A, B & C: Conceptual Assessment of Data Necessity, Processing Duration, and Disposal Method

| Personal Data Category ID (`IXIT 21-PersData`) | Defined Processing Purpose & Activities                                                       | Processing Necessity Assessment (Unit a)                                                                    | Retention Duration & Reasonable Lifecycle (Unit b)                                                                             | Disposal Process Suitability (`IXIT 25-DelFunc`) (Unit c)                                                                                                               | Unit Verdict |
|:-----------------------------------------------|:----------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Network packet routing, Web-UI session validation, and WAN TLS socket connection setup.       | **Necessary.** Essential for L3/L4 TCP/IP socket transport and local network management.                    | **Reasonable.** Held in volatile RAM during active link; static IP saved in `ruuvi.json` (`nvs`) until updated or reset.       | **Suitable.** Dynamic IPs flush on link drop/reboot; static IPs format cleanly via 7s physical button hold (`DelFunc-Hardware-Factory-Reset`).                          |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Layer 2 Ethernet frame switching, ARP query resolution, and local DHCP IP lease reservation.  | **Necessary.** Required at Layer 2 to anchor physical network links on the local network.                   | **Reasonable.** Stored permanently in read-only hardware eFuses (`esp_read_mac`); visible on LAN only while media is plugged.  | **Suitable.** Disconnecting network cable instantly ceases L2 exposure; clearing Wi-Fi credentials in `nvs` disables radio bridging.                                    |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Default Web-UI password seed and root seed for HMAC telemetry payload signing.                | **Necessary.** Required to establish factory baseline authentication and sign outbound status telemetry.    | **Reasonable.** Stored in nRF52811 FICR silicon; loaded into volatile RAM on boot. Never stored in NVS flash.                  | **Suitable.** Toggling telemetry off halts RAM consumption for HMAC signing; hardware factory reset reverts Web-UI to default state.                                    |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Origin header in outbound JSON envelopes to map metrics back to a specific physical gateway.  | **Necessary.** Required by backend servers to attribute telemetry data to the correct user gateway profile. | **Reasonable.** Stored in nRF52811 radio registers; loaded into RAM (`gw_mac`) on boot. Not stored in NVS.                     | **Suitable.** Disabling cloud targets in `ruuvi.json` halts JSON header generation; cloud account deletion (`DelFunc-Service-Account-Deletion`) purges server mappings. |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Client-side machine authentication and mTLS encryption when pushing data to custom endpoints. | **Necessary.** Essential to secure machine-to-machine telemetry pathways and protect private servers.       | **Reasonable.** Stored in `ruuvi.json` (`nvs`) and `gw_cfg_def` partitions until cleared or reset by user.                     | **Suitable.** Web-UI form clearing or 7s hardware button hold executes a complete flash sector erase across `nvs` and `gw_cfg_def`.                                     |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Real-time environmental monitoring, trend graphing, and alerting for the user.                | **Necessary.** Fulfills core product functionality of shipping sensor payloads to cloud/custom endpoints.   | **Reasonable.** On-device RAM ring buffers hold transient data; cloud history retained for active user subscription lifecycle. | **Suitable.** On-device RAM queues wipe on reboot; associated service history purges on user account deletion (`DelFunc-Service-Account-Deletion`).                     |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Data Necessity (Unit a):** Every personal data category processed in `IXIT 21-PersData` is
     directly necessary for local network operations, cryptographic authentication, or routing BLE
     telemetry data. No excessive or extraneous personal data is collected.
  2. **Reasonable Duration (Unit b):** On-device telemetry arrays and IP routing footprints reside
     in volatile RAM and flush dynamically. Hardware identifiers reside in read-only silicon
     registers. User configuration secrets and telemetry target settings reside in non-volatile
     flash memory strictly for the operational lifetime defined by the user.
  3. **Disposal Process Suitability (Unit c):** Disposal processes are complete and effective.
     On-device secrets and configuration profiles are permanently wiped via a 7-second physical
     button hold (`DelFunc-Hardware-Factory-Reset`), while associated service data is permanently
     purged via email-verified cloud account deletion (`DelFunc-Service-Account-Deletion`).

* **Verdict**: **PASS**

---

## Test case 6-6-2 (functional)

**Purpose**: To functionally verify on the DUT and associated services that typical personal data is
created (`a`), that uncreated/unnecessary personal data is not collected (`b`–`c`), and that
personal data is deleted when it is no longer required (`d`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C & D: Functional Personal Data Collection, Minimization, and Disposal Verification

**Testing Methodology**: The test laboratory provisioned typical user data on the DUT and Ruuvi
Cloud (`a`), audited background network traffic and system memory to verify that extraneous
uncreated personal data is not collected (`b`–`c`), and executed deletion functionalities (
`DelFunc-Hardware-Factory-Reset` and `DelFunc-Service-Account-Deletion`) to verify complete data
disposal (`d`).

| Personal Data Category ID (`IXIT 21-PersData`) | Tested Data Creation & Network Traffic Audit                                 | Extraneous Data Minimization Assessment (Units b & c)                                                                                | Functional Disposal & Deletion Verification (Unit d)                                                                                                    | Unit Verdict |
|:-----------------------------------------------|:-----------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Provision static IP settings; monitor network egress traffic.                | Packet inspection confirms WAN packets contain only standard egress IP headers required for TCP/TLS routing.                         | Disconnecting network cable halts IP processing. Factory reset formats `nvs` flash, completely erasing saved static IP profiles.                        |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Monitor L2 Ethernet and Wi-Fi frame broadcasts.                              | Wireshark captures confirm ESP32 LAN MAC is used solely for L2 ARP/DHCP switching and is **never** sent over WAN to Ruuvi Cloud.     | Unplugging network media terminates L2 frame exposure. Factory reset clears Wi-Fi credentials, keeping station radio idle.                              |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Audit outbound TLS payloads and diagnostic HTTP status packets.              | Network sniffer confirms raw 64-bit FICR `DEVICEID` is never transmitted raw over network interfaces; used locally for HMAC signing. | Disabling diagnostics in Web-UI halts HMAC payload signing loops immediately.                                                                           |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Monitor JSON telemetry envelopes sent to `https://network.ruuvi.com/record`. | Wire captures confirm `gw_mac` is included strictly as the payload origin header for cloud sensor mapping.                           | Toggling telemetry targets `Disabled` in Web-UI halts outbound JSON posting. Cloud account deletion purges `gw_mac` database links.                     |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Provision custom HTTP API keys and upload x509 mTLS certificates.            | Memory audit confirms secrets reside strictly in `nvs` and `gw_cfg_def` partitions and are attached only to configured endpoints.    | Executing a 7-second hold of the `CONFIGURE` button triggers a low-level sector erase, wiping `nvs` and `gw_cfg_def` flash sectors completely (`0xFF`). |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Stream live BLE sensor telemetry to Ruuvi Cloud; request account deletion.   | Volatile RAM queues handle transient sensor packets. No unneeded sensor measurements or user location data are appended.             | Disabling cloud targets halts real-time posting. Authorizing cloud account deletion email link purges historical telemetry from cloud databases.        |   **PASS**   |

**Assessment Justification**: Functional testing and network traffic analysis confirm that the DUT
and its associated services collect only personal data strictly necessary for defined operational
purposes. No extraneous personal data (such as user contact lists, precise GPS locations, or
unneeded hardware identifiers) is collected or transmitted. When personal data is no longer
required, executing a physical factory reset (`DelFunc-Hardware-Factory-Reset`) permanently erases
on-device flash memory sectors, and requesting cloud account deletion (
`DelFunc-Service-Account-Deletion`) permanently purges historical telemetry records from associated
cloud services.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-6-1 & 6-6-2

| Test Case             | Purpose / Focus                       | Assessment Summary                                                                                                                              | Unit Verdict |
|:----------------------|:--------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-6-1 Unit a**      | Processing Necessity Check            | Conceptual audit confirms processing of all 6 personal data categories is strictly necessary for defined operational purposes.                  |   **PASS**   |
| **6-6-1 Unit b**      | Processing Duration Check             | Processing lifecycles are reasonable (RAM-only for transient data, hardware silicon for IDs, NVS flash for active configurations).              |   **PASS**   |
| **6-6-1 Unit c**      | Disposal Method Check                 | Disposal processes (7-second hardware reset and email-verified cloud account deletion) suitably delete personal data.                           |   **PASS**   |
| **6-6-2 Unit a**      | Personal Data Creation                | Typical user configurations, station credentials, API secrets, and telemetry streams created on DUT and Ruuvi Cloud.                            |   **PASS**   |
| **6-6-2 Units b & c** | Minimization & Unnecessary Data Check | Packet inspection confirms zero unnecessary or extraneous personal data is collected or transmitted by the DUT or cloud service.                |   **PASS**   |
| **6-6-2 Unit d**      | Functional Deletion Verification      | Functional tests confirm that executing factory resets wipes on-device flash sectors (`0xFF`) and cloud account deletion purges server history. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 6-6 of `ETSI EN 303 645`. Personal
data processed by the device and its associated services (`IXIT 21-PersData`) is strictly minimized
and necessary for defined operational purposes. Retention durations are bounded reasonably according
to data lifecycles—transient telemetry and IP footprints reside in volatile RAM, hardware IDs reside
in read-only silicon registers, and configuration credentials reside in non-volatile flash memory
strictly while active. Suitable disposal processes are provided: executing a 7-second hold of the
physical `CONFIGURE` button (`DelFunc-Hardware-Factory-Reset`) executes a low-level sector erase
across on-device flash memory (`nvs` and `gw_cfg_def`), while requesting cloud account deletion (
`DelFunc-Service-Account-Deletion`) permanently purges personal data from associated cloud services.

**Group Verdict**: **PASS**
