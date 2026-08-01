# Test group 5.7-1: Software Integrity Verification

Provision 5.7-1 — Status: **R**. Related IXIT: `IXIT 20-SecBoot`.

---

## Test case 5.7-1-1 (conceptual)

**Purpose**: To conceptually assess whether the secure boot and software integrity verification
mechanisms in `IXIT 20-SecBoot` provide the security guarantees of **Integrity** and
**Authenticity** (`a`), and whether the described detection mechanisms are suitable to fulfill these
guarantees (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Security Guarantees (Integrity and Authenticity)

* **Requirement**: Assess whether every software verification mechanism in `IXIT 20-SecBoot`
  explicitly specifies the mandatory security guarantees of **Integrity** and **Authenticity**.
* **Evaluation**: `IXIT 20-SecBoot` explicitly defines **Integrity** and **Authenticity** as the
  primary security guarantees across all three software tiers:
  1. Main ESP32 application binary image (`ota_0` / `ota_1`).
  2. Web-UI asset data partition (`fatfs_gwui.bin`).
  3. Co-processor BLE radio firmware partition (`fatfs_nrf52.bin`).
* **Unit A Verdict**: **PASS**

#### Test Unit B: Suitability of Detection and Verification Mechanisms

* **Requirement**: Assess whether the cryptographic algorithms and detection routines in
  `IXIT 20-SecBoot` are suitable to provide the claimed security guarantees under the baseline
  attacker model (Clause D.2).
* **Evaluation**:

| Target Software Component (`IXIT 20-SecBoot`)  | Cryptographic Verification Primitive                              | Detection Routine & Rollback Strategy                                                                              | Suitability & Attacker Model Assessment                                                                                    | Unit Verdict |
|:-----------------------------------------------|:------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Main Application Image** (`ota_0` / `ota_1`) | RSA-3072 PSS over SHA-256 (`SecParam-Main-Firmware-Signature`).   | `esp_image_verify` evaluates the active app slot against the embedded public key (`SecParam-FW-Verification-Key`). | **Suitable.** Asymmetric RSA-3072 signature verification guarantees that unauthenticated or modified binaries cannot boot. |   **PASS**   |
| **Web-UI File System** (`fatfs_gwui`)          | RSA-3072 PSS over SHA-256 (`SecParam-WebUI-Partition-Signature`). | `esp_secure_boot_verify_rsa_signature_block` computes SHA-256 partition digest and validates signature.            | **Suitable.** Prevents cold-boot manipulation of Web-UI asset files. Signature failure triggers automated slot rollback.   |   **PASS**   |
| **nRF52 Co-Processor Code** (`fatfs_nrf52`)    | RSA-3072 PSS over SHA-256 (`SecParam-nRF52-Partition-Signature`). | `esp_secure_boot_verify_rsa_signature_block` validates co-processor binary before SWD RAM stub injection.          | **Suitable.** Guarantees radio stack integrity. Mismatches block SWD loading and trigger slot rollback.                    |   **PASS**   |

* **Architectural Note on Root of Trust**: Production units utilize a precompiled secondary
  bootloader (`components/binaries/bootloader.bin`, v1.9.2) without burning hardware secure boot
  eFuses. Cryptographic verification is enforced programmatically at the application layer upon
  initialization. Under the ETSI EN 303 645 Level Basic Attacker Model (Clause D.2),
  application-layer RSA-3072 signature verification provides adequate software integrity protection
  against remote network and logical manipulation vectors.

* **Unit B Verdict**: **PASS**

---

## Test case 5.7-1-2 (functional)

**Purpose**: To functionally verify on the DUT that software integrity verification is implemented
according to `IXIT 20-SecBoot` and that corrupted or unauthenticated binary images are detected and
rejected (`a`).

---

### Test Unit A: Functional Software Integrity Verification Testing

**Testing Methodology**: The test laboratory performed bit-level manipulation (flipping binary
bytes) within the active application image (`ota_0`) and data partitions (`fatfs_gwui`,
`fatfs_nrf52`) via local serial flashing tools (`esptool.py`), monitoring boot diagnostic logs over
`LogIntf-USB-UART-Log-Stream`.

| Functional Test Scenario            | Target Component & Corruption Executed                                                              | Observed Functional DUT Behavior                                                                                                                              | Unit Verdict |
|:------------------------------------|:----------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Corrupted App Image Boot Test**   | Main Application Partition (`ota_0`). Single-byte corruption injected into app binary text segment. | `esp_image_verify` fails SHA-256 digest validation. The application marks `ota_0` corrupted, halts network setup, and executes automated rollback to `ota_1`. |   **PASS**   |
| **Corrupted Web-UI Partition Test** | Web-UI Data Partition (`fatfs_gwui.bin`). Signature block modified in flash.                        | `esp_secure_boot_verify_rsa_signature_block` fails verification. System aborts initialization and initiates automated partition rollback.                     |   **PASS**   |
| **Corrupted nRF52 Firmware Test**   | Co-Processor Partition (`fatfs_nrf52.bin`). Binary payload header modified.                         | Boot verification fails; SWD RAM stub injection is short-circuited. System triggers automated rollback to restore the signed co-processor image.              |   **PASS**   |
| **Dual-Slot Failure Lockout Test**  | Both `ota_0` and `ota_1` partition slots corrupted.                                                 | Signature checks fail for both slots. System halts application bring-up, disables all network interfaces, and enters an infinite reboot loop.                 |   **PASS**   |

**Assessment Justification**: Functional testing confirms that the software verification mechanisms
documented in `IXIT 20-SecBoot` are enforced. Bit-level modifications to application or data
partitions are detected during initialization, preventing corrupted or tampered code execution and
triggering automated slot rollback or system lockout.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.7-1-1 & 5.7-1-2

| Test Case          | Purpose / Focus                   | Assessment Summary                                                                                                   | Unit Verdict |
|:-------------------|:----------------------------------|:---------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.7-1-1 Unit a** | Security Guarantees Assessment    | Verification mechanisms explicitly guarantee Integrity and Authenticity across all software components.              |   **PASS**   |
| **5.7-1-1 Unit b** | Suitability of Detection Routines | RSA-3072 PSS signature validation over SHA-256 digests provides suitable application-layer software verification.    |   **PASS**   |
| **5.7-1-2 Unit a** | Functional Integrity Verification | Bit manipulation testing confirms the DUT detects corrupted binaries, aborts execution, and initiates slot rollback. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.7-1 of `ETSI EN 303 645`. The
device implements multi-stage software integrity verification (`IXIT 20-SecBoot`) across the main
ESP32 application, Web-UI asset partition (`fatfs_gwui`), and nRF52 co-processor code partition (
`fatfs_nrf52`). Cryptographic signatures are validated during early initialization using RSA-3072
with RSA-PSS padding over SHA-256 digests. Functional testing confirms that tampered or corrupted
software binaries are detected, preventing unauthorized code execution and triggering automated slot
rollback recovery.

**Group Verdict**: **PASS**
