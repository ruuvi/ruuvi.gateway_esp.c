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

| Mechanism ID    | Delivery Medium       | Unit a: Source/Target Authenticity Verification                                                                                                                                                                                                                                | Unit b: Payload Integrity Verification                                                                                                                                                                                                                                     |                              Unit c: Authenticity Verified by DUT Itself?                               |                                                Unit d: Integrity Verified by DUT Itself?                                                 |          Case Verdict          |
|:----------------|:----------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------:|:----------------------------------------------------------------------------------------------------------------------------------------:|:------------------------------:|
| `UpdMech-WebUI` | Network (HTTPS)       | The DUT validates the downloaded main application using RSA-3072-PSS and requires the signer public-key digest to match that of the running application.                                                                                                                       | The main candidate image is validated before the boot target is changed. The Web-UI and nRF52 images are written only to inactive partitions, then their SHA-256 digests are verified against signatures embedded in the new main application during early initialization. |               **Yes** (the DUT validates the main candidate before selecting it to boot)                | **Yes** (the DUT validates the main candidate before boot selection and the complete selected partition set during early initialization) |            **PASS**            |
| `UpdMech-Auto`  | Network (HTTPS)       | Same on-device RSA-3072-PSS and signer-key continuity checks as `UpdMech-WebUI`.                                                                                                                                                                                               | Same staged-partition and early-initialization verification sequence as `UpdMech-WebUI`.                                                                                                                                                                                   |                                                 **Yes**                                                 |                                                                 **Yes**                                                                  |            **PASS**            |
| `UpdMech-USB`   | Local Port (USB-UART) | v1.17.x ROM-loader flashing requires sustained physical access and a boot-strap action. It is a recovery/service path, not a remotely accessible operational update flow. An officially signed Ruuvi Gateway image performs the normal application self-check after it starts. | The host flasher's MD5 check confirms transfer integrity only. Because Secure Boot eFuses are not enabled in v1.17.x, the ROM loader does not authenticate a complete physical reflash.                                                                                    | **Qualified** (application self-check applies only after a signed Ruuvi Gateway application is running) |                                                     **Qualified** (same limitation)                                                      | **PASS (ETSI baseline scope)** |

---

### Detailed Test Unit Assessments

#### Unit a: Authenticity Verification

* **Assessment**: For `UpdMech-WebUI` and `UpdMech-Auto`, authenticity is established by an
  **RSA-3072 digital signature with RSA-PSS padding over a SHA-256 digest**. The DUT verifies the
  downloaded main application and requires its public-key digest to match the running application
  before it changes the boot target. The verification key is a product security parameter; the
  technical file does not depend on the storage location of the manufacturer's private signing key.
  The auxiliary partition signatures are embedded in the new main application, so they are checked
  by that application during the subsequent early-initialization self-check.
* **Verdict**: **PASS**

#### Unit b: Integrity Verification

* **Assessment**: The main application candidate is verified after download and before the boot
  target changes; a signature or signer-key mismatch fails the update and the inactive partitions
  are invalidated. The Web-UI and nRF52 images are staged in inactive partitions. After reboot, the
  new main application calculates their SHA-256 digests and verifies them against the signatures it
  carries. This sequencing is necessary because the authoritative signatures for those partitions
  are part of the new main application. A failure aborts initialization and invokes the OTA rollback
  path, which reboots from the previously working slot.
* **Verdict**: **PASS**

#### Units c & d: Performing Entity (Verification by DUT Itself)

* **Assessment**: Verification is performed by the DUT itself without relying on an external proxy:
  1. **Main application candidate:** `esp_ota_end_patched` verifies the downloaded main image and
     `fw_update_check_is_valid_pub_key` compares its signer-key digest with the running image before
     `esp_ota_set_boot_partition_patched` selects the candidate for reboot.
  2. **Selected partition set:** During early initialization, the new application calls
     `esp_image_verify` for itself and `esp_secure_boot_verify_rsa_signature_block` for
     `fatfs_gwui` and `fatfs_nrf52`. On failure, the normal OTA rollback state is retained and the
     device reboots to the previously working OTA slot.
  3. **Co-processor flash:** After the selected partition set passes signature validation, the ESP32
     checks the nRF52 flash with `nrf52swd_calc_sha256_digest_on_nrf52` and restores it from the
     signed selected `fatfs_nrf52` image when required.

#### USB Recovery / Service Interface Scope

`UpdMech-USB` is retained in the IXIT because it is a documented local recovery and factory-service
path. It requires physical connection, boot-strap entry into the ESP32 ROM loader, and sustained
control of the device. ETSI EN 303 645 addresses baseline threats and does not aim to protect
against attacks requiring sustained physical access. A complete custom reflash replaces the Ruuvi
Gateway software and is therefore outside the operational Ruuvi Gateway update flow evaluated here.

This is a release limitation, not an authentication claim for the v1.17.x ROM loader: its transfer
MD5 check is not an authenticity control, and application-level verification only applies once a
Ruuvi Gateway application is running. The planned v1.18.x Secure Boot eFuse and flash-encryption
implementation is the control that will cryptographically restrict physical reflashing; it must be
verified and documented when released.

* **Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway's network update mechanisms (`UpdMech-WebUI` and `UpdMech-Auto`) perform
on-device verification before selecting a new main application for boot. The selected application
then verifies itself and the Web-UI and nRF52 partitions before normal service initialization; a
failure invokes rollback to the previously working OTA slot. `UpdMech-USB` is a sustained-physical-
access recovery/service path with the v1.17.x limitation recorded above. Functional adversarial
testing is assessed under TS 103 701 test cases 5.3-2-2 and 5.7-1-2, not this conceptual test case.

**Group Verdict**: **PASS**
