# Test group 5.3-9: The DUT Itself Verifies Authenticity and Integrity of Updates Prior to Installation

Provision 5.3-9 — Status: **R F (g)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-9-1 (conceptual)

**Purpose**: To conceptually assess whether every update mechanism defined in `IXIT 7-UpdMech`
suitably verifies the authenticity (`a`) and integrity (`b`) of software updates, and to confirm
that both authenticity (`c`) and integrity (`d`) verifications are performed directly by the DUT
itself prior to installation.

---

### Test Units Conceptual Assessment Matrix

| Mechanism ID    | Delivery Medium       | Unit a: Source/Target Authenticity Verification                                                                                                             | Unit b: Payload Integrity Verification                                                                                                                      |                Unit c: Authenticity Verified by DUT Itself?                |                   Unit d: Integrity Verified by DUT Itself?                    | Case Verdict |
|:----------------|:----------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------:|:------------------------------------------------------------------------------:|:------------:|
| `UpdMech-WebUI` | Network (HTTPS)       | Enforces RSA-3072-PSS digital signature verification against the embedded public key (`SecParam-FW-Verification-Key`). Rejects forged or untrusted sources. | Computes SHA-256 hash over downloaded binaries (`ruuvi_gateway_esp.bin`, `fatfs_gwui.bin`, `fatfs_nrf52.bin`) and compares against signed signature blocks. | **Yes** (Executed locally by application task prior to staging completion) | **Yes** (Executed locally via `esp_image_verify` and partition RSA-PSS checks) |   **PASS**   |
| `UpdMech-Auto`  | Network (HTTPS)       | Evaluates identical RSA-3072-PSS signature blocks embedded in the main firmware header during background updates.                                           | Validates main image SHA-256 digest and cross-verifies auxiliary filesystem signatures embedded in main firmware manifest at boot.                          |          **Yes** (Executed locally during background OTA staging)          |           **Yes** (Executed locally on-device before reboot handoff)           |   **PASS**   |
| `UpdMech-USB`   | Local Port (USB-UART) | Post-reset application layer parses RSA-3072-PSS signature block before executing runtime handoff or mounting partition tables.                             | Flasher tool executes MD5 transfer checks. Main application executes RSA signature checks on boot prior to initializing services.                           |            **Yes** (Bootloader/App validates signature on DUT)             |                   **Yes** (Executed on-device post-flashing)                   |   **PASS**   |

---

### Detailed Test Unit Assessments

#### Unit a: Authenticity Verification

* **Assessment**: For all update mechanisms, authenticity is established via an **RSA-3072 digital
  signature with RSA-PSS padding over a SHA-256 digest**. The public verification key (
  `SecParam-FW-Verification-Key`) is embedded directly inside the main application text segment.
  Because an attacker cannot forge an RSA-3072-PSS signature without access to the manufacturer's
  private key (stored in secure `GitHub Secrets`), the DUT reliably verifies that the update
  originates from the legitimate manufacturer.
* **Verdict**: **PASS**

#### Unit b: Integrity Verification

* **Assessment**: Integrity is verified by calculating the SHA-256 digest over the downloaded binary
  payload blocks and comparing the result against the decrypted signature hash. Any payload
  modification (such as bit flips during network transit or malicious code injection) produces a
  hash mismatch, causing immediate rejection of the binary package.
* **Verdict**: **PASS**

#### Units c & d: Performing Entity (Verification by DUT Itself)

* **Assessment**: Verification is performed entirely on-device by the DUT itself without relying on
  external third-party proxies:
  1. **Main Application:** Upon completing network downloads (`UpdMech-WebUI` / `UpdMech-Auto`), the
     main application invokes `esp_image_verify` from the native ESP-IDF `bootloader_support`
     library to parse the RSA-3072-PSS signature appended to `ruuvi_gateway_esp.bin`.
  2. **Auxiliary Partitions:** Post-reboot, the main firmware executes
     `esp_secure_boot_verify_rsa_signature_block` over `fatfs_gwui.bin` and `fatfs_nrf52.bin` using
     signature manifests embedded within `ruuvi_gateway_esp.bin`.
  3. **Co-Processor Flash:** The ESP32 host halts the nRF52 co-processor, injects
     `nrf52swd_calc_sha256_digest_on_nrf52` into nRF52 RAM via SWD, reads back the calculated live
     flash digest, and compares it against `fatfs_nrf52.bin`.
* **Verdict**: **PASS**

---

## Test case 5.3-9-2 (functional)

**Purpose**: To functionally verify that the DUT itself detects and rejects software updates that
fail authenticity or integrity checks prior to committing partition slots for execution.

### Test Units A & B: Functional Verification of On-Device Authenticity and Integrity Checks

**Testing Methodology**: The test laboratory introduced corrupted, bit-flipped, and improperly
signed binaries across all update mechanisms to verify that the DUT autonomously rejects invalid
updates.

| Test Scenario / Injected Failure        | Execution Pathway                                              | Observed On-Device System Behavior                                                                                                                                                                       | Case Verdict |
|:----------------------------------------|:---------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Injected Bit-Flip / Corrupted Image** | Network update via `UpdMech-WebUI` or `UpdMech-Auto`           | `esp_image_verify` detected SHA-256 digest mismatch post-download. The DUT invalidated the staged inactive slot (`ota_0`/`ota_1`), aborted system restart, and maintained active application continuity. |   **PASS**   |
| **Untrusted / Forged RSA Signature**    | Signed binary using an arbitrary 3072-bit RSA private key      | `esp_image_verify` evaluated the signature against the embedded `SecParam-FW-Verification-Key`. Signature validation failed; the DUT dropped the payload and refused partition handoff.                  |   **PASS**   |
| **Auxiliary Manifest Mismatch**         | Uploaded valid main binary with tampered `fatfs_gwui.bin`      | Main application booted, executed `esp_secure_boot_verify_rsa_signature_block` checks, detected the auxiliary manifest hash mismatch, and executed an immediate rollback to the previous partition set.  |   **PASS**   |
| **Co-Processor SWD Flash Tampering**    | Flashed modified code directly to nRF52 via external SWD probe | Post-reboot SWD RAM check (`nrf52swd_calc_sha256_digest_on_nrf52`) detected a digest mismatch against `fatfs_nrf52.bin` and automatically re-flashed the nRF52 chip to restore factory integrity.        |   **PASS**   |

**Assessment Justification**: Functional testing demonstrates that the DUT autonomously validates
payload authenticity and integrity. Corrupted, tampered, or improperly signed binaries are reliably
detected and rejected on-device prior to committing execution partition slots.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.3-9 of `ETSI EN 303 645`. All software update
mechanisms (`UpdMech-WebUI`, `UpdMech-Auto`, `UpdMech-USB`) rely on on-device cryptographic
verification performed directly by the DUT itself prior to installation. RSA-3072-PSS signature
validation, SHA-256 hash checks, and inter-chip SWD RAM audits ensure that untrusted, corrupted, or
tampered updates are rejected without impacting operational continuity.

**Group Verdict**: **PASS**
