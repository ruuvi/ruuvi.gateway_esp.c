# IXIT 7-UpdMech: Update Mechanisms

The following declarations detail the update mechanisms supported by the Ruuvi Gateway (DUT) to
ensure the continued security, stability, and integrity of all software components defined in the
technical documentation.

---

## Table C.7: IXIT 7-UpdMech (Update Mechanisms)

### **ID**: UpdMech-WebUI: User-Initiated Network Update (Web-UI)

#### Description

User-initiated firmware update performed over the local area network via the Gateway’s Web-UI
configuration dashboard interface. The process is network-based, where the DUT fetches update
indices and signed binary assets from the official production update servers.

* **Steps**:
  1. The user authenticates into the local LAN Web-UI configuration dashboard.
  2. The DUT queries the version index at `https://network.ruuvi.com/firmwareupdate`, which returns
     a structured JSON descriptor carrying a `latest` (release) and `beta` entry, each specifying a
     string `version` and a base `url`.
  3. If a newer version string is identified on the server, the interface prompts the operator to
     initiate the update process.
  4. The DUT downloads the individual binary files (`ruuvi_gateway_esp.bin`, `fatfs_gwui.bin`,
     `fatfs_nrf52.bin`) from the base `url` (resolving to `https://fwupdate.ruuvi.com/<version>` for
     production releases or the designated GitHub repository releases path for beta tracks).
  5. The images are streamed exclusively into the **inactive** hardware storage slots (alternating
     between flash slots `ota_0` and `ota_1`, alongside the alternate `fatfs_gwui_2` and
     `fatfs_nrf52_2` partitions); the active running application space remains untouched during
     transit.
  6. The DUT completes the main-image update through `esp_ota_end_patched`, which verifies the
     downloaded RSA-3072-PSS signature block. It then verifies that the candidate signer public-key
     digest matches the currently running Ruuvi Gateway application. A failure invalidates all next
     partitions; only a passing candidate is selected as the next boot target.
  7. Post-reboot, the selected new main firmware validates itself and its auxiliary file system
     partitions via `esp_image_verify` and `esp_secure_boot_verify_rsa_signature_block`. The
     auxiliary signatures are embedded in the new main firmware, so their verification necessarily
     occurs after the new main firmware starts but before normal service initialization and before
     the OTA image is marked valid. A failure retains the rollback state and reboots to the
     previously working OTA slot. The firmware then invokes the co-processor verification track
     (`nrf52fw_update_fw_step3`). Rather than relying on a loose alphanumeric
     version string comparison, the ESP32 host halts the nRF52811 chip, injects a specialized
     SHA-256 calculation binary stub into the co-processor's RAM via the hardware Serial Wire
     Debug (SWD) bus (`nrf52swd_calc_sha256_digest_on_nrf52`), and manipulates the execution pointer
     register. The host reads back the calculated cryptographic digest of the target's active flash
     segments and compares it against the signed reference firmware array stored inside the freshly
     staged `fatfs_nrf52` partition. The host bypasses update operations only if both the version
     structure and the active flash SHA-256 digest match the reference block precisely. If a hash
     collision mismatch or version delta is found, the host immediately executes
     `nrf52fw_update_fw_step4` to programmatically flash and restore the co-processor. If all
     initialization checks succeed, the system configuration slot is marked valid.

#### Security Guarantees

The mechanism ensures **Integrity** and **Authenticity** of the firmware images. The DUT verifies
the downloaded main application before selecting it as the boot target. The selected application
then verifies the complete selected partition set before normal service initialization and only
marks the OTA image valid after successful initialization. This protects the platform against
unauthorized binary execution and Man-in-the-Middle (MitM) payload manipulation during network
transit.

#### Cryptographic Details

Authenticity and integrity are enforced by a firmware image signed with the ESP32 Secure Boot v2
signature layout: **RSA-3072 with RSA-PSS padding over a SHA-256 digest**, compiled using the
manufacturer private key. The public key for verification (`SecParam-FW-Verification-Key`) is
integrated within the main application text segment. Because production units running v1.17.x
implement a secondary bootloader block where hardware secure boot eFuses are not burned, signature
validation is processed programmatically at the application layer. Automated
rollback-on-failure is driven by the ESP-IDF partition table mapping: if post-reboot asset
validation fails, the system reverts execution to the prior known-good partition slot. Downgrade
prevention via hardware secure-version eFuses is not enabled; block verification isolates malicious
binaries, but older signed release images are not cryptographically blocked from installation.

#### Initiation and Interaction

Initiated manually by the operator navigating to the software update panel in the configuration
wizard dashboard. The user must explicitly click the "Update" button control to trigger the network
download loop.

#### Configuration

The user configures the update channel target within Step 5 of the onboarding wizard or subsequent
maintenance screens, selecting between `Auto update` (Regular release channel),
`Auto update (for beta testers)`, or `Manual updates only`. The operator can also manually override
the default update host target URL string.

#### Update Checking

The query check is initiated and performed by the DUT itself every time an authenticated session
loads the software update dashboard interface.

#### User Notification

The user is notified via the Web-UI interface of version availability details. During active
installation loops, a localized progress tracking bar and operational status indicators are
rendered. A success banner is displayed immediately before the gateway invokes `gateway_restart()`.

---

### **ID**: UpdMech-Auto: Automatic Background Update (Auto-Update)

#### Description

A network-based automatic update mechanism that ensures the device remains up-to-date with security
patches without requiring manual operator intervention.

#### Security Guarantees

See `UpdMech-WebUI`.

#### Cryptographic Details

See `UpdMech-WebUI`. The update validation track forces the same inter-chip SWD-injected SHA-256
flash evaluation sequence before passing or invoking the automated nRF52 co-processor code
restoration loop.

#### Initiation and Interaction

This is an automated update mechanism requiring no user interaction. The system performs the version
tracking check and image download background tasks seamlessly, deferring the system reboot execution
to low-activity periods.

#### Configuration

The feature is enabled by default when the update policy is configured to `Auto update`. The user
can restrict automated execution by establishing an active schedule mask (defining allowed weekdays
and permitted daily time windows tied to a local timezone offset parameter).

#### Update Checking

The check is performed independently by the DUT itself. The background task engine queries
`https://network.ruuvi.com/firmwareupdate` approximately 2 hours post-boot, repeating the query
check roughly every 12 hours following a successful check. If the remote server fails to respond,
the engine enforces a 40 minutes retry backoff loop, executing only within the user-configured
calendar schedule window.

#### User Notification

No physical LED indicators or network notification messages are emitted during background automatic
updates to ensure a seamless operational deployment experience.

---

### **ID**: UpdMech-USB: Local Manual Update (USB)

#### Description

A local, physical, non-network update mechanism utilizing a virtual serial connection over the
on-board USB-to-UART bridge controller interface (`PhyIntf-USB`). Images are written directly to
flash sectors using development tools (such as `esptool.py` or the manufacturer's flashing utility
script). This interface serves as the primary path for factory provisioning, recovery operations, or
offline maintenance environments.

* **Physical Access Boundary Note:** Operating over the physical USB port bypasses the network stack
  entirely. On production units running v1.17.x firmware (where hardware eFuse Secure Boot and NVS
  flash encryption are not enabled), an actor with direct physical access can read flash sectors or
  completely replace the product software via `esptool.py`. Doing so requires sustained physical
  access and ROM-loader boot-strap entry; the resulting custom software is no longer the Ruuvi
  Gateway DUT. This is a v1.17.x release limitation, not an authentication control. Secure Boot
  eFuses and flash encryption are planned for v1.18.x; the technical file must be updated with
  released-product evidence once those controls are enabled.

#### Security Guarantees

The mechanism requires immediate physical proximity access to the device's Type-C USB interface
port and ROM-loader boot-strap entry. It operates outside of the network stack, completely isolating
the flasher state from remote network exploits. When an official Ruuvi Gateway image is flashed, its
application-layer self-check validates the main image and selected auxiliary partitions during early
initialization. On v1.17.x this is not a ROM-loader pre-flash authentication control.

#### Cryptographic Details

The development flash tool executes an initial `MD5` validation sweep over the transferred binary
data array during the write process; this is a transport-integrity check, not an authenticity check.
For an official Ruuvi Gateway image, application-level RSA-3072-PSS / SHA-256 verification then runs
at boot as described under `UpdMech-WebUI`. A complete custom reflash is a physical recovery/service
scenario outside the normal Ruuvi Gateway update flow and the ETSI EN 303 645 baseline threat scope.

#### Initiation and Interaction

The operator must physically connect the gateway to a local host machine via a USB-C cable and run
the flashing utility. The tool manipulates the USB bridge control lines (DTR/RTS) programmatically
to reset the chip and force entry into the hardware ROM bootloader state to receive data.

#### Configuration

No configuration options exist; this represents a purely manual, one-time engineering action.

#### Update Checking

No automated checking is performed. The user must manually supply the compiled firmware binary
files.

#### User Notification

Not applicable.

---

## Summary Matrix for the Technical File

| Interface ID    | Delivery Medium              | Initiation Vector                  | Cryptographic Verification Protocol                                                                               | Automated Rollback Active? | Downgrade Blocked by Fuses? | Physical Access Required? |
|:----------------|:-----------------------------|:-----------------------------------|:------------------------------------------------------------------------------------------------------------------|:--------------------------:|:---------------------------:|:-------------------------:|
| `UpdMech-WebUI` | Network (HTTPS / Port 443)   | User Manual Request (Web-UI)       | Main candidate: RSA-3072-PSS / SHA-256 before boot selection; selected partition set: early-init signature checks |            Yes             |             No              |            No             |
| `UpdMech-Auto`  | Network (HTTPS / Port 443)   | Automated Background Scheduler     | Same as `UpdMech-WebUI`                                                                                           |            Yes             |             No              |            No             |
| `UpdMech-USB`   | Local Port (USB-UART Bridge) | Physical recovery/service flashing | MD5 transfer check; official Ruuvi Gateway image self-check at boot; no ROM-loader authentication in v1.17.x      |   N/A (manual overwrite)   |             No              |          **Yes**          |
