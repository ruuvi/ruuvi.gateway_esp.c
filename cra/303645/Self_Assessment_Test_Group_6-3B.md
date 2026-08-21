# Test group 6-3B: Storing Information on Consumers' Consent for Personal Data Processing

Provision 6-3B — Status: **M**. Related IXIT: `IXIT 21-PersData`, `IXIT 26-UserDec`.

---

## Test case 6-3B-1 (conceptual)

**Purpose**: To conceptually assess whether the DUT or its associated services provide a means of
persistently storing information on consumers' consent for processing each category of personal data
in `IXIT 21-PersData` according to "Storing Consent" (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Conceptual Assessment of Storing Consent Mechanisms

| Personal Data Category ID (`IXIT 21-PersData`) | Consent Basis & Origin                                                                                                      | Storage Location & Technical Format ("Storing Consent")                                                                                                                                                                                                    | Persistent Storage Assessment                                                                                                                | Unit Verdict |
|:-----------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Physical interface connection / Static IP configuration in Step 2 (`UserDec-2`).                                            | **Implicit / Flash Partition.** Static IP profiles and active interface flags are stored persistently in `ruuvi.json` on the `nvs` partition. Dynamic DHCP leases are derived directly from physical link state.                                           | **Sufficient.** Explicit user IP setup choices are stored in non-volatile flash memory; dynamic IP loops persist for physical link duration. |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Hardware eFuse cells (`esp_read_mac`) & network cable attachment (`UserDec-1` / `UserDec-2`).                               | **Hardware eFuse / Link State.** Permanently burned into ESP32 eFuse silicon cells; exposure is derived dynamically from physical link state and saved station credentials in `ruuvi.json` (`nvs` partition).                                              | **Sufficient.** Consent state corresponds directly to active physical connectivity and saved network credentials.                            |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Read from nRF52811 FICR into RAM on boot; deployment setup & HMAC cloud telemetry registration (`UserDec-6` / `UserDec-7`). | **Flash Partition Target Flags.** Stored locally in non-volatile flash configurations (`ruuvi.json` on `nvs`) as active target selection flags (`use_ruuvi_cloud: true/false`). Hardware ID is read into RAM dynamically.                                  | **Sufficient.** Active target selection flags persist in `nvs` flash across reboots; raw ID resides in volatile RAM.                         |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Read from nRF52811 radio into RAM on boot; outbound telemetry endpoint setup in Step 7 (`UserDec-7` / `UserDec-8`).         | **Flash Partition Target Flags.** Stored locally in non-volatile flash partitions (`ruuvi.json` on `nvs`) by saving active target selection flags (`use_ruuvi_cloud: true/false`, `use_custom_server: true/false`). `gw_mac` is read into RAM dynamically. | **Sufficient.** Active target flags saved in `nvs` flash maintain the explicit routing consent state; raw MAC resides in volatile RAM.       |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Manual credential submission for third-party servers (`UserDec-8`).                                                         | **Flash Partitions.** Alphanumeric API keys/passwords saved in `ruuvi.json` (`nvs`); x509 PEM certificates/keys stored in `gw_cfg_def`. Blank fields denote zero consent.                                                                                  | **Sufficient.** Preserving or clearing credential strings in non-volatile flash acts as the persistent consent state.                        |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Cloud option selection in Step 7 (`UserDec-7`) & radio scanning setup in Step 10 (`UserDec-10`).                            | **On-Device:** Stored locally in `nvs` flash (`ruuvi.json`) as active telemetry target flags.<br>**Associated Services:** Stored in Ruuvi Cloud user account database as active gateway associations (`gw_mac` claims).                                    | **Sufficient.** Active target flags persist in NVS flash locally; cloud user database preserves gateway binding state persistently.          |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Persistent On-Device Consent Storage:** Active consent choices for processing on-device and
     outbound personal data are stored persistently in non-volatile flash memory (`ruuvi.json`
     manifest on the `nvs` partition and custom assets on the `gw_cfg_def` partition). These
     configuration flags survive hard power cuts and system reboots, ensuring consent states remain
     intact. The underlying hardware identifiers (`PersData-Gateway-LAN-MAC`,
     `PersData-Hardware-DeviceID`, `PersData-Gateway-MAC-Identifier`) are read directly from
     hardware eFuses or co-processor registers into volatile RAM on boot, but their outbound
     transmission consent state is governed by the persistent target selection flags saved in NVS.
  2. **Associated Service Consent Storage:** For personal data processed on official associated
     services (`https://network.ruuvi.com/`), consent information is persistently stored within the
     Ruuvi Cloud database through active user account registrations, claimed gateway MAC
     associations (`gw_mac`), and authenticated subscription sessions.
  3. **Documentation Accuracy:** The declarations under "Storing Consent" in `IXIT 21-PersData`
     accurately describe the technical storage mechanisms for every personal data category.

* **Unit A Verdict**: **PASS**

---

## Test case 6-3B-2 (functional)

**Purpose**: To functionally check on the DUT and associated services that information about the
storage of consumers' consent for processing personal data matches the description in "Storing
Consent" in `IXIT 21-PersData` (`a`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Functional Consent Storage Verification

**Testing Methodology**: The test laboratory configured active processing states for all personal
data categories across onboarding wizard steps (`IXIT 26-UserDec`), performed system power cycles
and reboots, inspected `nvs` and `gw_cfg_def` flash partition contents via `esptool.py`, and
verified user account gateway claim states on the Ruuvi Cloud portal.

| Personal Data Category ID (`IXIT 21-PersData`) | Tested Onboarding / Web-UI State                                              | Executed Cold Boot / Memory Audit                                          | Observed Storage Behavior & Inspection Results                                                                                                        | Unit Verdict |
|:-----------------------------------------------|:------------------------------------------------------------------------------|:---------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PersData-Network-IP-Footprints`**           | Configure static IP settings in Web-UI Step 2.                                | Perform hard USB power cycle; dump `nvs` flash partition via `esptool.py`. | **Matches IXIT.** Static IP configuration parameters remain intact in `ruuvi.json` on `nvs` flash post-reboot.                                        |   **PASS**   |
| **`PersData-Gateway-LAN-MAC`**                 | Provision Wi-Fi station SSID/password.                                        | Perform system reboot; check interface re-association behavior.            | **Matches IXIT.** Station credentials persist in `nvs`; ESP32 reads hardware eFuse via `esp_read_mac` on boot and re-exposes LAN MAC.                 |   **PASS**   |
| **`PersData-Hardware-DeviceID`**               | Enable Ruuvi Cloud telemetry in Step 7.                                       | Perform cold boot; monitor outbound POST headers via packet sniffer.       | **Matches IXIT.** Device reloads `use_ruuvi_cloud: true` from `nvs` flash, extracts `DEVICEID` from nRF52811 FICR into RAM, and resumes HMAC signing. |   **PASS**   |
| **`PersData-Gateway-MAC-Identifier`**          | Enable custom server telemetry in Step 8.                                     | Inspect `ruuvi.json` flash dump post-reboot.                               | **Matches IXIT.** Target active flag (`use_custom_server: true`) persists in `nvs` flash; `gw_mac` is read from nRF52811 into RAM on boot.            |   **PASS**   |
| **`PersData-Custom-Target-Access-Secrets`**    | Upload user x509 PEM SSL client certificate in Web-UI.                        | Perform power cycle; inspect `gw_cfg_def` partition sectors.               | **Matches IXIT.** SSL certificate blocks persist in dedicated `gw_cfg_def` flash partition across reboots.                                            |   **PASS**   |
| **`PersData-BLE-Sensor-Telemetry`**            | Claim gateway MAC on Ruuvi Cloud web portal and enable cloud POSTs in Step 7. | Perform reboot; audit Ruuvi Cloud web portal account dashboard.            | **Matches IXIT.** Local `nvs` flash preserves active cloud post flag; Ruuvi Cloud database preserves user gateway claim state.                        |   **PASS**   |

**Assessment Justification**: Functional testing and physical flash memory audits confirm that
information about storing consumers' consent matches the descriptions in `IXIT 21-PersData`
precisely. On the device, active consent states (routing flags, Wi-Fi credentials, SSL keys) are
stored persistently in non-volatile flash partitions (`nvs` and `gw_cfg_def`), surviving power
interruptions and system restarts, while hardware MACs and DeviceIDs are dynamically restored to RAM
from eFuses/FICR registers. On associated services, consent information is persistently maintained
in the Ruuvi Cloud database as active user account bindings until explicitly deleted.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-3B-1 & 6-3B-2

| Test Case         | Purpose / Focus                       | Assessment Summary                                                                                                                                          | Unit Verdict |
|:------------------|:--------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-3B-1 Unit a** | Conceptual Consent Storage Audit      | "Storing Consent" in `IXIT 21-PersData` accurately describes persistent NVS flash storage and cloud database mechanisms for all 6 personal data categories. |   **PASS**   |
| **6-3B-2 Unit a** | Functional Consent Storage Inspection | Flash dumps (`esptool.py`) and cloud portal audits confirm consent target flags persist across cold reboots and match IXIT descriptions exactly.            |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-3B of `ETSI EN 303 645`. Means of
persistently storing information on consumers' consent for personal data processing (
`IXIT 21-PersData`) are provided by the DUT and its associated cloud service. User consent choices
made during onboarding (`IXIT 26-UserDec`) or via Web-UI configuration menus—such as enabling Ruuvi
Cloud telemetry, defining custom server endpoints, uploading SSL certificates, or saving static IP
rules — are stored locally in non-volatile flash memory partitions (`ruuvi.json` on `nvs` and custom
certificates on `gw_cfg_def`). Hardware MAC addresses (read from ESP32 eFuses via `esp_read_mac`)
and `DEVICEID` / `gw_mac` values (read from the nRF52811 co-processor) are loaded into volatile RAM
on boot, but their transmission consent state is governed by the persistent NVS target flags. For
associated services, consent information is persistently maintained within the Ruuvi Cloud database
as active user account gateway associations. Functional testing confirms that stored consent states
persist reliably across system power cycles and reboots.

**Group Verdict**: **PASS**
