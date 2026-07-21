# IXIT 25-DelFunc: Deletion Functionalities

The following declarations detail the local hardware and remote service-level deletion
functionalities implemented to permanently eradicate user configuration profiles, network
credentials, private keys, personal account records, and cloud telemetry datasets.

---

## Table C.25: IXIT 25-DelFunc (Deletion Functionalities)

### **ID**: DelFunc-Hardware-Factory-Reset

#### Description

Executes a complete structural formatting sweep over the device's persistent flash memory blocks,
permanently purging all user-provisioned configuration parameters, Wi-Fi station credentials,
Machine-to-Machine (M2M) API tokens, custom SSL certificates, and private cryptographic keys.

* **NVS Flash Sector Security Note:** Incremental parameter modifications or text-field updates
  performed through the Web-UI dashboard do not execute an immediate physical sector overwrite on
  the underlying flash storage cells due to the append-only transaction architecture of the ESP-IDF
  Non-Volatile Storage (NVS) wear-leveling algorithm. To guarantee complete, physical data
  destruction across raw flash pages, the operator must execute this hardware factory reset
  procedure.

#### Target Type

User data on the device

#### Initiation and Interaction

The user must manually press and hold the physical `CONFIGURE` button located on the gateway
enclosure for **7 seconds or longer**. This physical action bypasses logical application constraints
and forces the underlying ESP-IDF framework to issue low-level flash sector formatting and erase
commands across both the `nvs` partition (housing `ruuvi.json` and credentials) and the expanded
`gw_cfg_def` partition (housing custom SSL certificates and private keys).

#### Confirmation

The gateway provides physical feedback by executing an immediate hardware restart sequence
(`gateway_restart()`). Upon rebooting, the device returns to its factory baseline state: it drops
all station connections, purges stored credentials, and activates its local configuration hotspot.
The user confirms successful deletion when the active Wi-Fi station link drops, the former local
Web-UI becomes unreachable, and accessing the captive portal requires the unique, hardware-derived
factory default password to enter Step 1 of the onboarding configuration wizard.

---

### **ID**: DelFunc-Service-Account-Deletion

#### Description

Removes the user's online profile and associated account records from the official Ruuvi Cloud
backend infrastructure, completely purging personal identifiers and queuing telemetry logs for
permanent deletion.

#### Target Type

Personal data on associated services

#### Initiation and Interaction

The user signs into their account on the official Ruuvi Cloud web portal or Ruuvi Station
application and submits an account deletion request. To prevent accidental or unauthorized data
loss, the cloud system sends a confirmation link to the user's registered email address. The user
must click the confirmation link to authorize final account eradication.

#### Confirmation

Upon user confirmation via the emailed link, the user is automatically logged out of active app and
browser sessions, and the web interface displays an account termination confirmation message. All
personally identifiable information (PII) is removed immediately, and associated data records (such
as custom background images, gateway assignment mappings, and historical sensor readings) are queued
for automated background deletion from cloud databases.

---

## Summary Matrix for the Technical File

| Deletion ID                        | Targeted Storage Space                            | Scope of Data Purged                                                                                         | Verification Evidence                                                                                                            |
|:-----------------------------------|:--------------------------------------------------|:-------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------|
| `DelFunc-Hardware-Factory-Reset`   | On-Device Flash Partitions (`nvs` / `gw_cfg_def`) | Low-level sector erase of passwords, network credentials, M2M tokens, custom certificates, and private keys. | Hotspot activates; former station connections drop; system reverts to Step 1 of onboarding wizard requiring default credentials. |
| `DelFunc-Service-Account-Deletion` | Associated Service (Ruuvi Cloud Infrastructure)   | Permanent erasure of user profile, PII, gateway associations, and background queuing of sensor history logs. | Automated session logout; confirmation message rendered; account access revoked and user data permanently purged.                |
