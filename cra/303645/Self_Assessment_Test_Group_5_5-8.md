# Test group 5.5-8: Secure Management Processes for Critical Security Parameters

Provision 5.5-8 — Status: **M C (16)**. Related IXIT: `IXIT 4-Conf`, `IXIT 10-SecParam`,
`IXIT 14-SecMgmt`, `IXIT 25-DelFunc`.

---

## Condition Evaluation (ETSI EN 303 645 Annex B)

* **Condition 16 Requirement**: *"Critical security parameters are used."*
* **DUT Capabilities Assessment**: As declared in `IXIT 10-SecParam`, the DUT uses multiple Critical
  Security Parameters (CSPs), including administrative credentials (
  `SecParam-LAN-WebUI-Credentials`), Wi-Fi passphrases (`SecParam-WiFi-STA-Credentials`), M2M API
  Bearer tokens (`SecParam-LAN-Bearer-Tokens`), HMAC root secrets (
  `SecParam-HMAC-Symmetric-Secrets`), and TLS private keys.
* **Condition Result**: Condition 16 evaluates to **TRUE**. Provision 5.5-8 is **Mandatory (M)**.

---

## Test case 5.5-8-1 (conceptual)

**Purpose**: To conceptually assess whether the secure management processes in `IXIT 14-SecMgmt`
cover the entire lifecycle of all Critical Security Parameters (`a`), and to verify that explicit
corporate confirmation for secure management is provided in `IXIT 4-Conf` (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Full Parameter Lifecycle Coverage

* **Requirement**: Assess whether `IXIT 14-SecMgmt` defines complete processes governing all six
  lifecycle phases (*generation*, *provisioning*, *storage*, *updates*,
  *decommissioning/archival/destruction*, and *compromise handling*) across all CSPs.

| Governed CSP Group (`IXIT 14-SecMgmt`)                                                                 | Generation & Provisioning                                                                    | Storage & Protection                                                                                               | Updates & Rotation                                                                                  | Decommissioning, Archival & Destruction                                                                                                                                                                                                                                    | Expiration & Compromise Handling                                                                                                          | Unit Verdict |
|:-------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`SecMgmt-Hardware-Silicon-Root`**<br>(`SecParam-Hardware-DeviceID`)                                  | Permanently burned by silicon vendor in nRF52811 FICR registers. Immutable.                  | Read-only silicon registers; dynamic RAM cache. Printed on physical sticker & UART boot log.                       | N/A (Hardware Immutable).                                                                           | Physical chip destruction. Non-volatile silicon cells.                                                                                                                                                                                                                     | Static unique $2^{64}$ entropy pool prevents class attacks. HMAC keys dynamically rotatable via cloud headers.                            |   **PASS**   |
| **`SecMgmt-Local-Credentials`**<br>(`SecParam-LAN-WebUI-Credentials`, `SecParam-WiFi-STA-Credentials`) | Default derived from $DEVICEID$. Custom passkeys generated during setup wizard (MD5 hashed). | Stored in `ruuvi.json` on `nvs` partition. Password saved as MD5 hash. Scrubbed from Web-UI queries & serial logs. | Modified on-demand via authenticated LAN Web-UI session.                                            | **Hardware Factory Reset.** Initiated by a long press on the `CONFIGURE` button, triggering a reboot after 5–6 s followed by partition erasure (~11 s total), with LED confirmation upon NVS erase completion (`ruuvi_nvs_erase()`, `gw_cfg_storage_deinit_erase_init()`). | Factory reset via physical button restores factory defaults and clears user credentials; Web-UI credential updates replace stored hashes. |   **PASS**   |
| **`SecMgmt-Programmatic-M2M-Tokens`**<br>(`SecParam-LAN-Bearer-Tokens`)                                | Client-side 256-bit Web Crypto PRNG (`crypto.lib.WordArray.random(32)`).                     | Saved in `ruuvi.json` on `nvs`. Scrubbed from Web-UI configuration exports & UART logs.                            | Replaced completely via Web-UI configuration saves or cleared.                                      | Clearing and saving Web-UI fields removes active token values, but previous values may remain in append-only NVS records. Hardware factory reset (`DelFunc-Hardware-Factory-Reset`) formats the partitions to erase retained values.                                       | Immediate token regeneration or field clearing in Web-UI revokes compromised tokens instantly.                                            |   **PASS**   |
| **`SecMgmt-Outbound-Assets-And-Secrets`**<br>(Telemetry Credentials, TLS Private Keys, HMAC Keys)      | HMAC default seeded from $DEVICEID$. Remote endpoints/keys entered manually during setup.    | Task RAM context memory & `ruuvi.json` / `gw_cfg_def` NVS space. Scrubbed from logs.                               | Web-UI updates. HMAC keys support dynamic in-flight rotation via `Ruuvi-HMAC-KEY` response headers. | Cleared during hardware factory reset (`DelFunc-Hardware-Factory-Reset`) via low-level partition formatting across `nvs` and `gw_cfg_def`.                                                                                                                                 | Updating compromised target credentials in Web-UI or executing a hardware factory reset revokes outgoing mbedTLS connection states.       |   **PASS**   |

* **Unit A Assessment Justification**: `IXIT 14-SecMgmt` comprehensively addresses every lifecycle
  stage across all CSP categories—from high-entropy generation and hardware-anchored provisioning to
  runtime masking, dynamic cloud-driven key rotation, and low-level NVS flash partition formatting
  via the physical `CONFIGURE` button factory reset mechanism (`DelFunc-Hardware-Factory-Reset`).

* **Unit A Verdict**: **PASS**

#### Test Unit B: Check for Confirmation of Secure Management

* **Requirement**: Check whether "Confirmation of Secure Management" in `IXIT 4-Conf` states an
  explicit positive confirmation.
* **Evaluation**: `IXIT 4-Conf` explicitly states:
  * **`Confirmation of Secure Management: Yes`**
  * The declaration confirms that corporate infrastructure is active, operational staff are briefed,
    and secure management processes—including low-level NVS formatting routines across `nvs` and
    `gw_cfg_def` partitions via the `CONFIGURE` button factory reset procedure—are formally
    established.

* **Unit B Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-8-1

| Test Unit          | Purpose / Focus                | Assessment Summary                                                                                                                                                                  | Unit Verdict |
|:-------------------|:-------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.5-8-1 Unit a** | Parameter Lifecycle Management | `IXIT 14-SecMgmt` covers generation, provisioning, storage, updates, destruction, and compromise handling across all CSPs, utilizing the verified hardware factory reset mechanism. |   **PASS**   |
| **5.5-8-1 Unit b** | Implementation Confirmation    | `IXIT 4-Conf` explicitly confirms that secure management processes are active and operational staff are briefed.                                                                    |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.5-8 of `ETSI EN 303 645`. The technical
documentation (`IXIT 14-SecMgmt`) establishes comprehensive secure management processes covering the
complete lifecycle of all Critical Security Parameters—including hardware silicon roots, local
Web-UI/Wi-Fi credentials, high-entropy M2M bearer tokens, and TLS/HMAC telemetry secrets.
Decommissioning and credential eradication are achieved via low-level partition formatting
(`ruuvi_nvs_erase()` and `gw_cfg_storage_deinit_erase_init()`) initiated by long pressing the
physical `CONFIGURE` button, which triggers a reboot after 5–6 seconds and executes flash erasure
after an additional 5–6 seconds (approximately 11 seconds in total elapsed time), with LED feedback
signaling completion once erasure finishes. `IXIT 4-Conf` provides formal corporate confirmation of
operational deployment.

**Group Verdict**: **PASS**
