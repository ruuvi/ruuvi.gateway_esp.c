# Test group 5.11-1: Simple Erasure of User Data from the Device

Provision 5.11-1 — Status: **M**. Related IXIT: `IXIT 10-SecParam`, `IXIT 21-PersData`,
`IXIT 25-DelFunc`.

---

## Test case 5.11-1-1 (conceptual)

**Purpose**: To conceptually assess whether the user data erasure functionality in `IXIT 25-DelFunc`
can be performed by a non-technical user (Clause D.3) (`a`), is adequate to permanently erase
targeted user data from device flash memory (`b`), and covers all personal data, user
configurations, and user cryptographic material (`c`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A, B & C: Conceptual Assessment of User Data Erasure Functionality

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Initiation & User Interaction (Unit a)                                                                                                                                                                       | Technical Erasure Method & Flash Adequacy (Unit b)                                                                                                                                                                               | Scope of Covered Personal Data, Config & Keys (Unit c)                                                                                                                                                                                                                                                                  | Unit Verdict |
|:----------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Hardware-Factory-Reset`**          | **Simple Physical Action.** User presses and holds the physical `CONFIGURE` button on the gateway enclosure for **7 seconds or longer**. Bypasses software menus; requires no technical skills (Clause D.3). | **Low-Level Sector Formatting.** Triggers low-level flash sector erase commands across `nvs` and `gw_cfg_def` partitions. Overcomes NVS append-only wear-leveling by physically clearing all wear-leveled flash pages to `0xFF`. | **100% Coverage of User Data.** Purges all Wi-Fi credentials (`PersData-Network-IP-Footprints`), Web-UI admin passwords (`SecParam-WebUI-User-Defined-Password`), M2M Bearer tokens (`lan_auth_api_key`/`_rw`), `ruuvi.json` configs, custom HTTP/MQTT target URLs, and custom SSL private keys stored in `gw_cfg_def`. |   **PASS**   |

**Assessment Justification**: The technical declarations in `IXIT 25-DelFunc` fulfill all conceptual
criteria under Provision 5.11-1:

1. **User Accessibility (Unit a):** Holding a physical button for 7 seconds is simple and easily
   performed by a user with limited technical knowledge.
2. **Erasure Adequacy (Unit b):** Executing full sector block erasure across both `nvs` and
   `gw_cfg_def` partitions ensures append-only wear-leveled flash pages are wiped, preventing raw
   flash extraction.
3. **Data Scope Coverage (Unit c):** All personal data (`IXIT 21-PersData`), user settings, and
   cryptographic keys (`IXIT 10-SecParam`) stored on the device are completely erased.

**Verdict**: **PASS**

---

## Test case 5.11-1-2 (functional)

**Purpose**: To functionally verify on the DUT that typical user data is created (`a`), that user
data erasure initiation and interaction operate as documented (`b`), that target user data is
completely erased from device storage (`c`), and that multi-user privilege boundaries are assessed
if applicable (`d`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C & D: Functional User Data Creation, Erasure, and Inspection

**Testing Methodology**: The test laboratory injected non-default user data (custom Wi-Fi station
SSID/password, custom Web-UI admin password, M2M Bearer API keys, custom HTTPS telemetry targets,
and user SSL certificates), executed the 7-second physical button hold, observed hardware feedback,
and inspected flash memory via local tools (`esptool.py`) and Web-UI onboarding wizard endpoints.

| Functional Test Stage                   | Test Action Executed on DUT                                                                                             | Observed Functional DUT Behavior & Verification                                                                                                                                                                                 | Unit Verdict |
|:----------------------------------------|:------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Data Creation (Unit a)**              | Provision non-default Wi-Fi credentials, custom Web-UI admin password, M2M Bearer tokens, and custom HTTPS target URLs. | Settings successfully commit to `ruuvi.json` (`nvs` partition) and custom SSL certificates write to `gw_cfg_def`. DUT operates in custom user state.                                                                            |   **PASS**   |
| **Initiation Verification (Unit b)**    | Press and hold physical `CONFIGURE` button for 7 seconds.                                                               | At $t \ge 7\text{ s}$, the DUT halts application execution and invokes `gateway_restart()`. Physical Wi-Fi station link drops immediately.                                                                                      |   **PASS**   |
| **Post-Erasure Inspection (Unit c)**    | Re-connect to DUT via captive portal SSID (`Configure Ruuvi Gateway XXXX`) and perform flash dump via `esptool.py`.     | **Zero User Data Retained.** Captive portal requires factory default $DEVICEID$ password. Step 1 of onboarding wizard displays blank fields. Flash dump confirms `nvs` and `gw_cfg_def` sectors are cleanly formatted (`0xFF`). |   **PASS**   |
| **Multi-User Isolation Check (Unit d)** | Evaluate multi-user role capabilities.                                                                                  | The DUT operates as a single-tenant embedded gateway (single administrator context). Executing a factory reset wipes all on-device data, which is appropriate for a single-tenant hardware appliance.                           |   **PASS**   |

**Assessment Justification**: Functional testing confirms that executing
`DelFunc-Hardware-Factory-Reset` cleanly purges all user-created configurations, credentials, API
tokens, and private SSL keys from physical flash memory. The initiation behavior matches
`IXIT 25-DelFunc` precisely, and post-reset flash dumps verify zero residual user data remains on
the device.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-1-1 & 5.11-1-2

| Test Case           | Purpose / Focus                    | Assessment Summary                                                                                                              | Unit Verdict |
|:--------------------|:-----------------------------------|:--------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-1-1 Unit a** | Simplicity Assessment (Clause D.3) | Physical 7-second press of the `CONFIGURE` button is easily accessible to non-technical users.                                  |   **PASS**   |
| **5.11-1-1 Unit b** | Erasure Method Adequacy            | Full partition-level sector block erasure overwrites append-only NVS wear-leveled flash sectors cleanly.                        |   **PASS**   |
| **5.11-1-1 Unit c** | User Data Scope Coverage           | Covers all personal data (`IXIT 21-PersData`), user settings, M2M tokens, passwords, and private SSL keys (`IXIT 10-SecParam`). |   **PASS**   |
| **5.11-1-2 Unit a** | Typical User Data Creation         | Non-default user configurations, passwords, API tokens, and SSL certificates successfully provisioned.                          |   **PASS**   |
| **5.11-1-2 Unit b** | Initiation & Interaction Check     | 7-second button hold triggers immediate system restart and Wi-Fi station disconnection as documented.                           |   **PASS**   |
| **5.11-1-2 Unit c** | Post-Erasure Data Absence Check    | Flash memory inspection and captive portal checks verify 100% erasure of all on-device user data.                               |   **PASS**   |
| **5.11-1-2 Unit d** | Multi-User Boundary Check          | Single-tenant device architecture cleanly clears total system state upon factory reset.                                         |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.11-1 of `ETSI EN 303 645`. The device
provides a simple, physical user data erasure mechanism (`DelFunc-Hardware-Factory-Reset`) initiated
by holding the physical `CONFIGURE` button for 7 seconds or longer. This action executes a low-level
sector block erase across both the `nvs` and `gw_cfg_def` flash partitions, physically purging all
user-provisioned Wi-Fi credentials, Web-UI passwords, M2M Bearer tokens, telemetry URLs, and private
SSL certificates. Functional testing and post-reset flash memory dumps verify that all user data is
permanently erased from the device.

**Group Verdict**: **PASS**
