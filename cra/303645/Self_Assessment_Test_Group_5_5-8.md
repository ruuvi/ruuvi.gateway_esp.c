# Test group 5.5-8: Secure Management Processes for Critical Security Parameters

Provision 5.5-8 — Status: **M C (16)**. Related IXIT: `IXIT 4-Conf`, `IXIT 10-SecParam`,
`IXIT 14-SecMgmt`.

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

| Governed CSP Group (`IXIT 14-SecMgmt`)                                                                 | Generation & Provisioning                                                                    | Storage & Protection                                                                                               | Updates & Rotation                                                                                  | Decommissioning, Archival & Destruction                                                                                 | Expiration & Compromise Handling                                                                               | Unit Verdict |
|:-------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------|:------------:|
| **`SecMgmt-Hardware-Silicon-Root`**<br>(`SecParam-Hardware-DeviceID`)                                  | Permanently burned by silicon vendor in nRF52811 FICR registers. Immutable.                  | Read-only silicon registers; dynamic RAM cache. Printed on physical sticker & UART boot log.                       | N/A (Hardware Immutable).                                                                           | Physical chip destruction. Non-volatile silicon cells.                                                                  | Static unique $2^{64}$ entropy pool prevents class attacks. HMAC keys dynamically rotatable via cloud headers. |   **PASS**   |
| **`SecMgmt-Local-Credentials`**<br>(`SecParam-LAN-WebUI-Credentials`, `SecParam-WiFi-STA-Credentials`) | Default derived from $DEVICEID$. Custom passkeys generated during setup wizard (MD5 hashed). | Stored in `ruuvi.json` on `nvs` partition. Password saved as MD5 hash. Scrubbed from Web-UI queries & serial logs. | Modified on-demand via authenticated LAN Web-UI session.                                            | **7-Second Hold Hardware Erasure.** Forces low-level physical sector formatting across `nvs` & `gw_cfg_def` partitions. | Hard factory reset via physical button invalidates compromised sessions and rolls back to unique default.      |   **PASS**   |
| **`SecMgmt-Programmatic-M2M-Tokens`**<br>(`SecParam-LAN-Bearer-Tokens`)                                | Client-side 256-bit Web Crypto PRNG (`crypto.lib.WordArray.random(32)`).                     | Saved in `ruuvi.json` on `nvs`. Scrubbed from Web-UI configuration exports & UART logs.                            | Replaced completely via Web-UI configuration saves or cleared.                                      | Web-UI field wiping or 7-second hardware button formatting purges flash blocks.                                         | Immediate token regeneration or field clearing in Web-UI revokes compromised tokens instantly.                 |   **PASS**   |
| **`SecMgmt-Outbound-Assets-And-Secrets`**<br>(Telemetry Credentials, TLS Private Keys, HMAC Keys)      | HMAC default seeded from $DEVICEID$. Remote endpoints/keys entered manually during setup.    | Task RAM context memory & `ruuvi.json` / `gw_cfg_storage` NVS space. Scrubbed from logs.                           | Web-UI updates. HMAC keys support dynamic in-flight rotation via `Ruuvi-HMAC-KEY` response headers. | Cleared during 7-second hardware button NVS partition re-formatting. No persistent archival.                            | Updating compromised target credentials or triggering a factory reset revokes leaked mbedTLS sessions.         |   **PASS**   |

* **Unit A Assessment Justification**: `IXIT 14-SecMgmt` comprehensively addresses every lifecycle
  stage—from secure high-entropy generation and hardware-anchored provisioning to low-level NVS
  flash sector re-formatting during factory reset and dynamic cloud-driven key rotation during
  compromise.

* **Unit A Verdict**: **PASS**

#### Test Unit B: Check for Confirmation of Secure Management

* **Requirement**: Check whether "Confirmation of Secure Management" in `IXIT 4-Conf` states an
  explicit positive confirmation.
* **Evaluation**: `IXIT 4-Conf` explicitly states:
  * **`Confirmation of Secure Management: Yes`**
  * The declaration confirms that corporate infrastructure is active, operational staff are briefed,
    and low-level NVS formatting routines on 7-second hardware button holds are implemented and
    verified.
* **Unit B Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-8-1

| Test Unit          | Purpose / Focus                | Assessment Summary                                                                                                         | Unit Verdict |
|:-------------------|:-------------------------------|:---------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.5-8-1 Unit a** | Parameter Lifecycle Management | `IXIT 14-SecMgmt` covers generation, provisioning, storage, updates, destruction, and compromise handling across all CSPs. |   **PASS**   |
| **5.5-8-1 Unit b** | Implementation Confirmation    | `IXIT 4-Conf` explicitly confirms that secure management processes are active and operational staff are briefed.           |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.5-8 of `ETSI EN 303 645`. The technical
documentation (`IXIT 14-SecMgmt`) establishes comprehensive secure management processes covering the
complete lifecycle of all Critical Security Parameters—including hardware silicon roots, local
Web-UI/Wi-Fi credentials, high-entropy M2M bearer tokens, and TLS/HMAC telemetry secrets.
Eradication is guaranteed via low-level NVS partition re-formatting upon a 7-second physical button
hold, and `IXIT 4-Conf` provides formal corporate confirmation of operational deployment.

**Group Verdict**: **PASS**
