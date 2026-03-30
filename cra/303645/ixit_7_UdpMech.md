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
2. DUT queries https://fwupdate.ruuvi.com for the latest available version.
3. If a newer version is found, the user is prompted to initiate the update.
4. DUT downloads the signed update package.
5. DUT verifies authenticity and integrity using a pre-installed public key.
6. Upon successful validation, the DUT flashes the new firmware and restarts.

#### Security Guarantees

The mechanism ensures **Integrity** and **Authenticity** of the firmware. 
Verification is performed by the DUT itself before the installation begins.
This protects against unauthorized firmware execution and "Man-in-the-Middle" (MitM) attacks during
delivery.

#### Cryptographic Details

Authenticity and integrity are realized by a signed firmware package based on RSA-2048 with SHA-256.
The signing is performed with the Ruuvi manufacturer private key. The corresponding public key is
integrated into the DUT during manufacturing (Hardware Root of Trust). Anti-rollback/downgrade
protection is enforced for this mechanism to prevent the installation of older, potentially
vulnerable versions.

#### Initiation and Interaction

Initiated by the user by navigating to the Web-UI configuration wizard. The user must manually click
a "Update" button to start the download and installation process.

#### Configuration

The user can configure the update "Channel" via the Web-UI settings, choosing between "Release" (
stable) or "Beta" (pre-release) branches.

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

The user can enable or disable the Auto-Update feature via the Web-UI. The default configuration
is "Enabled" for the "Release" channel.

#### Update Checking

The DUT independently queries https://fwupdate.ruuvi.com twice per day (every 12 hours). The check
is performed by the DUT itself.

#### User Notification

No notification is provided for automatic updates to ensure a seamless "Set and Forget" experience.

---

### UpdMech-3: Local Manual Update (USB)

#### Description

A local, non-network-based update mechanism using a USB flash drive. This is primarily intended for
initial provisioning, recovery, or offline environments.

#### Security Guarantees

Integrity is checked via checksums. Unlike the network-based mechanisms, this path is designed to
allow firmware downgrades and the installation of custom/arbitrary firmware for developer use. The
security risk is mitigated by the requirement for physical access to the device and the USB port.

#### Cryptographic Details

Checksum verification (e.g., CRC32 or SHA-256) is used to ensure the file was not corrupted during
the transfer to the USB drive.

#### Initiation and Interaction

The user must physically the Gateway to USB and run the flashing script.

#### Configuration

No specific configuration options; this is a purely manual "one-time" action.

#### Update Checking

No automatic checking. The user manually provides the firmware file.

#### User Notification

Not applicable.

----------------------------------------------------------------------------------------------------

## Summary of Update Mechanisms

| ID        | Delivery        | Initiation      | Verification     | Downgrade Allowed? |
|-----------|-----------------|-----------------|------------------|--------------------|
| UpdMech-1 | Network (HTTPS) | User (Web-UI)   | RSA-2048/SHA-256 | No                 |
| UpdMech-2 | Network (HTTPS) | Automatic       | RSA-2048/SHA-256 | No                 |
| UpdMech-3 | Local (USB)     | User (Physical) | Checksum         | Yes                |

