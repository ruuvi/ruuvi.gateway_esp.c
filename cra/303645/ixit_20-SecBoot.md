# IXIT 20-SecBoot: Secure Boot Mechanisms

The following declarations detail the application-layer secure boot verification loops, signature
validation targets, automated recovery rollback strategies, and failure behaviors enforced by the
device during its initialization sequence.

---

## Table C.20: IXIT 20-SecBoot (Secure Boot Mechanisms)

### **ID**: SecBoot-Application-Chain-Of-Trust

#### Description

The Ruuvi Gateway framework implements an application-layer secure boot verification sequence
executed within the core application binary to enforce signature validation constraints.

* **Trust Assumptions:** The secondary bootloader image stored in flash is assumed to be stable.
  Although the application firmware build configuration defines `CONFIG_SECURE_BOOT_V2_ENABLED`, a
  precompiled secondary bootloader (`components/binaries/bootloader.bin`, v1.9.2) is flashed to the
  device target without burning the hardware eFuses for Secure Boot or Flash Encryption.
  Consequently, cryptographic signature enforcement is handled programmatically by the main
  application runtime engine post-boot. The static public cryptographic root of trust (
  `SecParam-FW-Verification-Key`) is compiled directly into the immutable text segment of the main
  application binary image file.
* **Protected Components:** The verification engine validates the complete software ecosystem across
  three distinct layers: the main application firmware image itself, the read-only Web-UI asset file
  system partition (`fatfs_gwui`), and the read-only nRF52 co-processor binary partition (
  `fatfs_nrf52`).

#### Security Guarantees

Ensures the **Integrity** and **Authenticity** of the complete software operational application
space prior to full system initialization. This blocks the execution of unauthorized, manipulated,
or truncated code payloads, preventing malicious local physical or remote network write
modifications from operating on either the ESP32 or the nRF52811 microcontrollers.

#### Detection Mechanisms

During the early system initialization phase, the firmware invokes the
`fw_update_read_flash_info_and_check_signatures()` tracking routine. This routine executes the
following multi-stage verification steps:

1. **Self-Validation:** Invokes the low-level API utility `esp_image_verify` from the native ESP-IDF
   `bootloader_support` library. Passing the active partition's address properties and memory
   boundaries, this routine extracts internal image metadata hashes and cryptographically certifies
   the active binary slot against the appended RSA-3072 production signature block matching
   `SecParam-Main-Firmware-Signature`.
2. **Web-UI Partition Validation:** Computes a dynamic SHA-256 digest over the raw contents of the
   active Web-UI partition block, validating it via `esp_secure_boot_verify_rsa_signature_block`
   against the embedded assembly-linked public key tag symbols matching
   `SecParam-WebUI-Partition-Signature`.
3. **Co-Processor Partition Validation:** Maps the co-processor binary data blocks and validates the
   payload structure against `SecParam-nRF52-Partition-Signature` using the
   `esp_secure_boot_verify_rsa_signature_block` validation routine.

If any signature verification block check fails or an unexpected partition structural size mismatch
is encountered, the verification framework drops the initialization sequence and flags the active
slot block as corrupted.

#### User Notification

The device enforces automated recovery loops when an unauthorized or corrupted block change is
identified. The validation engine attempts an automated slot rollback sequence to swap execution
flags and boot from the alternate, cryptographically verified firmware version stored in the
secondary OTA flash slot (`ota_0` or `ota_1`).

If a successful rollback cannot be completed (e.g., both slots fail signature validation or the
backup partition is unpopulated), the gateway halts standard application initialization, blocks the
setup of all network interfaces, and triggers a hardware reset via `gateway_restart()`. This forces
the device into a persistent, infinite **Reboot Loop**. The operator can recognize this failure
state through continuous, repetitive physical power-cycling behaviors, the complete absence of the
configuration hotspot broadcast SSID (`Configure Ruuvi Gateway XXXX`), and a total lack of local LAN
management Web-UI connectivity.

#### Notification Functionality

There are no network functionalities involved. All validation routines, automated rollback choices,
and reset executions occur strictly at the local, pre-boot application layer before the network
interface sub-systems, driver blocks, or TCP/IP client tasks are initialized.

---

## Summary Matrix for the Technical File

| Secure Boot ID                         | Protected Software Component                  | Primary Detection Mechanism                                 | Enforcement Action on Failure                                              |
|:---------------------------------------|:----------------------------------------------|:------------------------------------------------------------|:---------------------------------------------------------------------------|
| **SecBoot-Application-Chain-Of-Trust** | Main ESP32 Application Code (`ota_0`/`ota_1`) | `esp_image_verify` against RSA-3072 public key structures   | Aborts update initialization sequence; prevents slot flag execution.       |
| **SecBoot-WebUI-Validation**           | Local UI Assets File System (`fatfs_gwui`)    | `esp_secure_boot_verify_rsa_signature_block` SHA-256 checks | Marks partition slot corrupted; triggers automated slot rollback.          |
| **SecBoot-CoProcessor-Validation**     | nRF52 BLE Code Block (`fatfs_nrf52`)          | `esp_secure_boot_verify_rsa_signature_block` SHA-256 checks | Marks partition slot corrupted; triggers automated slot rollback.          |
| **SecBoot-System-Lockout**             | Complete Device Runtime Stack                 | Dual-Slot Signature Verification Failure Scan               | Halts network tasks, blocks interfaces, and triggers infinite reboot loop. |
