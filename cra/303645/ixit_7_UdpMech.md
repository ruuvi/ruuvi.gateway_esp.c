# IXIT 7-UpdMech: Update Mechanisms

The following table describes the update mechanisms supported by the Ruuvi Gateway (DUT) to ensure
the continued security and functionality of all software components defined in **IXIT 6-SoftComp**.

## Table C.7: IXIT 7-UpdMech (Update Mechanisms)

### UpdMech-1: User-Initiated Network Update (Web-UI)

#### Description

User-initiated firmware update performed over the local network via the Gateway’s Web-UI
configuration wizard. The process is network-based, where the DUT fetches update metadata and
binaries from the official Ruuvi update server.

**Steps**:
1. User opens the Web-UI configuration wizard.
2. DUT queries the version index at `https://network.ruuvi.com/firmwareupdate`, which returns a JSON
   descriptor with a `latest` (release) and `beta` entry, each carrying a `version` and a base `url`.
3. If a newer version is found, the user is prompted to initiate the update.
4. DUT downloads the individual signed binary images (`ruuvi_gateway_esp.bin`, `fatfs_gwui.bin`,
   `fatfs_nrf52.bin`) from the base `url` — for releases this is `https://fwupdate.ruuvi.com/<version>`,
   and for beta builds `https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/<version>/`.
   The images are always written to the **inactive** partitions (the inactive OTA application slot and
   the inactive `fatfs_gwui` / `fatfs_nrf52` data partitions); the currently running slot is left
   untouched.
5. DUT verifies the authenticity and integrity of the downloaded main application image (RSA-3072-PSS
   against the public key embedded in the running application), sets the inactive slot as the next
   boot partition, and restarts.
6. On boot, the new firmware validates itself, then validates the Web-UI partition (`fatfs_gwui`) and
   the nRF52 firmware partition (`fatfs_nrf52`); if the nRF52 firmware version differs it is
   (re)flashed to the co-processor over SWD. Once all checks succeed the new firmware is marked valid;
   if any check fails the device rolls back to the previously working slot.

#### Security Guarantees

The mechanism ensures **Integrity** and **Authenticity** of the firmware. 
Verification is performed by the DUT itself before the installation begins.
This protects against unauthorized firmware execution and "Man-in-the-Middle" (MitM) attacks during
delivery.

#### Cryptographic Details

Authenticity and integrity are realized by a firmware image signed with the ESP32 Secure Boot v2
signature format: **RSA-3072 with RSA-PSS padding over a SHA-256 digest**. The signing is performed
with the Ruuvi manufacturer private key. The corresponding public key is embedded in the application
binary and its digest anchors the root of trust. Verification is performed by the main application at
the application layer (the production units use a legacy non-secure 2nd-stage bootloader and hardware
secure-boot eFuses are not burned — see IXIT 20-SecBoot): the main application image is verified via
`esp_image_verify`/`esp_ota_end_patched`, and the Web-UI and nRF52 data partitions are verified via
RSA-PSS signature blocks. Note: a rollback-on-failure mechanism (fall back to the previously working
slot) is enabled, but anti-rollback/downgrade prevention via secure-version eFuses is **not** enabled,
so downgrade to an older signed release is not cryptographically blocked.

#### Initiation and Interaction

Initiated by the user by navigating to the Web-UI configuration wizard. The user must manually click
a "Update" button to start the download and installation process.

#### Configuration

The user can select the auto-update cycle via the Web-UI settings — "Regular" (stable),
"Beta tester" (pre-release), or "Manual" — and can override the firmware update URL. The
manual/Web-UI update itself does not depend on the cycle setting.

#### Update Checking

The check is performed by the DUT itself every time the Web-UI configuration wizard is accessed by a
user.

#### User Notification

The user is notified via the Web-UI if a new version is available. During the update, a progress bar
and status messages are displayed. Once finished, a "Success" message is shown before the device
reboots.

---

### UpdMech-2: Automatic Background Update (Auto-Update)

#### Description

A network-based automatic update mechanism that ensures the device remains up-to-date without user
intervention.

#### Security Guarantees

See UpdMech-1.

#### Cryptographic Details

See UpdMech-1.

#### Initiation and Interaction

This is an automatic update mechanism. It requires no user interaction to initiate or apply. The
system performs the update in the background and reboots during periods of low activity.

#### Configuration

The user can enable or disable the Auto-Update feature via the Web-UI by selecting the auto-update
cycle. The default configuration is the "Regular" (stable) cycle, enabled by default, restricted to
a configurable schedule (weekdays bitmask and daily time window, with a timezone offset).

#### Update Checking

The DUT independently checks https://network.ruuvi.com/firmwareupdate for new releases. The check
is performed by the DUT itself: approximately 2 hours after each reboot, then roughly every 12 hours
after a successful check (retrying about every 40 minutes on failure), and only within the
user-configured weekday/time-window schedule.

#### User Notification

No notification is provided for automatic updates to ensure a seamless "Set and Forget" experience.

---

### UpdMech-3: Local Manual Update (USB)

#### Description

A local, non-network update mechanism using a serial (UART) connection over the on-board CH340
USB-to-serial converter. Images are written to flash with `esptool.py` (or the Ruuvi helper script
`ruuvi_gw_flash.py`). This is primarily intended for initial provisioning, recovery, or offline
environments.

#### Security Guarantees

The mechanism requires physical access to the device's USB port. `esptool.py` verifies each flash
write, and at boot the application re-verifies the image signatures (see UpdMech-1). Signed
Ruuvi release images — including older versions — can be flashed, since anti-rollback via
secure-version eFuses is not enabled.

#### Cryptographic Details

`esptool.py` performs an MD5 flash-write verification of the transferred data. Image authenticity is
enforced by the same RSA-3072-PSS / SHA-256 signature verification described in UpdMech-1, applied at
boot.

#### Initiation and Interaction

The user must physically connect the Gateway to a host computer via USB and run the flashing tool.

#### Configuration

No specific configuration options; this is a purely manual "one-time" action.

#### Update Checking

No automatic checking. The user manually provides the firmware file.

#### User Notification

Not applicable.

----------------------------------------------------------------------------------------------------

## Summary of Update Mechanisms

| ID        | Delivery        | Initiation      | Verification         | Downgrade blocked?        |
|-----------|-----------------|-----------------|----------------------|---------------------------|
| UpdMech-1 | Network (HTTPS) | User (Web-UI)   | RSA-3072-PSS/SHA-256 | No (not eFuse-enforced)    |
| UpdMech-2 | Network (HTTPS) | Automatic       | RSA-3072-PSS/SHA-256 | No (not eFuse-enforced)    |
| UpdMech-3 | Local (USB-UART)| User (Physical) | Boot RSA-3072-PSS + esptool MD5 | No |

