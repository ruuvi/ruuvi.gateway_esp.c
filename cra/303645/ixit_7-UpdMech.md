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
  6. The DUT executes `esp_image_verify` from the native ESP-IDF `bootloader_support` library over
     the downloaded main binary array. Upon successful verification against the appended production
     RSA-3072 signature block, the engine configures the inactive slot as the primary boot target
     and executes a system restart.
  7. Post-reboot, the new main firmware initializes, validates its auxiliary file system partitions
     via `esp_secure_boot_verify_rsa_signature_block` checks, and invokes the co-processor
     verification track (`nrf52fw_update_fw_step3`). Rather than relying on a loose alphanumeric
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

The mechanism ensures **Integrity** and **Authenticity** of the firmware images. Verification is
performed directly by the DUT itself at the application tier before the new partition slot is
committed for permanent execution. This protects the platform against unauthorized binary execution
and Man-in-the-Middle (MitM) payload manipulation during network transit.

#### Cryptographic Details

Authenticity and integrity are enforced by a firmware image signed with the ESP32 Secure Boot v2
signature layout: **RSA-3072 with RSA-PSS padding over a SHA-256 digest**, compiled using the
manufacturer private key. The public key for verification (`SecParam-FW-Verification-Key`) is
integrated within the main application text segment. Because production units running v1.17.x
implement a secondary bootloader block where hardware secure boot eFuses are not burned, signature
validation is processed programmatically at the application layer post-boot. Automated
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
  flash encryption are not enabled), an actor with direct physical access to the device casing can
  read flash sectors or flash custom binaries via `esptool.py`. This threat vector is strictly
  contained to actors holding physical access to the hardware enclosure (`PhyIntf-USB`).
  Enabling ESP32 eFuse Secure Boot v2 and NVS Flash Encryption is scheduled for the v1.18.x
  firmware release.

#### Security Guarantees

The mechanism requires immediate physical proximity access to the device's Type-C USB interface
port. The interface operates outside of the network stack, completely isolating the flasher state
from remote network exploits. For standard signed images, image verification signatures are
validated at boot by the application layer post-flashing.

#### Cryptographic Details

The development flash tool executes an initial `MD5` validation sweep over the transferred binary
data array during the write process. Image authenticity and structural block integrity are
subsequently enforced at boot by the exact same RSA-3072-PSS / SHA-256 signature verification loop
described under `UpdMech-WebUI`.

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

| Interface ID    | Delivery Medium              | Initiation Vector                | Cryptographic Verification Protocol                       | Automated Rollback Active? | Downgrade Blocked by Fuses? | Physical Access Required? |
|:----------------|:-----------------------------|:---------------------------------|:----------------------------------------------------------|:--------------------------:|:---------------------------:|:-------------------------:|
| `UpdMech-WebUI` | Network (HTTPS / Port 443)   | User Manual Request (Web-UI)     | RSA-3072-PSS / SHA-256 ESP Scan + SWD nRF RAM Check Block |            Yes             |             No              |            No             |
| `UpdMech-Auto`  | Network (HTTPS / Port 443)   | Automated Background Scheduler   | RSA-3072-PSS / SHA-256 ESP Scan + SWD nRF RAM Check Block |            Yes             |             No              |            No             |
| `UpdMech-USB`   | Local Port (USB-UART Bridge) | Physical USB Connection Flashing | Boot RSA-3072-PSS Scan + Flasher Tool MD5 Sweep           |   N/A (Manual Overwrite)   |             No              |          **Yes**          |
