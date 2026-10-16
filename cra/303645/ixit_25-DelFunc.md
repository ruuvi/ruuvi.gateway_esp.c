# IXIT 25-DelFunc: Deletion Functionalities

The following declarations detail the local hardware and remote service-level deletion
functionalities implemented to permanently eradicate user configuration profiles, network
credentials, private keys, personal account records, and cloud telemetry datasets.

---

## Table C.25: IXIT 25-DelFunc (Deletion Functionalities)

### **ID**: DelFunc-Hardware-Factory-Reset

#### Description

Executes a structural formatting sweep across the device's persistent configuration partitions,
erasing saved local Gateway and network configurations, administrator credentials,
Machine-to-Machine (M2M) API tokens, custom target URLs, and uploaded SSL/TLS certificates and
private keys.

For restart-only operation, release the physical `CONFIGURE` button immediately when the status LED
turns off for the initial restart and keep it released. The three-second release window after
restart is operator guidance, not an enforced firmware timeout or a guaranteed minimum delay.
The currently reported button-state check occurs approximately 3600 ms after restart. This is a
single boot-time sample after initialization, not a dedicated cancellation timer: an explicit
minimum sampling time of 3000 ms is pending in
[issue #1657](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1657) for milestone `v1.17.6`.\
If the button is held at that sample, the factory recovery procedure starts. The boot handler
`handle_reset_button_is_pressed_during_boot()` invokes low-level partition formatting routines
(`ruuvi_nvs_erase()` and
`gw_cfg_storage_deinit_erase_init()`) across both the `nvs` partition (housing `ruuvi.json` and
credentials) and the
expanded `gw_cfg_def` partition (housing custom SSL/TLS certificates and private keys). This resets
the device to its
factory baseline state. It does not delete remote cloud accounts or telemetry data previously
transmitted to external
endpoints.

* **NVS Flash Sector Security & Architecture Note:** Incremental parameter modifications or
  text-field updates performed through the Web-UI dashboard do not execute an immediate physical
  sector overwrite on the underlying flash storage cells due to the append-only transaction
  architecture of the ESP-IDF Non-Volatile Storage (NVS) wear-leveling algorithm. Executing this
  hardware factory reset performs a structural partition format. Following partition formatting,
  default factory records and the factory-assigned hardware MAC address are initialized in
  accordance with baseline operational rules.

#### Target Type

User data on the device

#### Initiation and Interaction

**Destructive action:** Before initiating, the operator must back up any required local settings.
Factory reset erases all stored local configurations, credentials, API keys, custom server targets,
and uploaded certificates/keys, immediately interrupting network connectivity. Cloud accounts and
previously forwarded telemetry data are not deleted.

The user manually presses and holds the physical `CONFIGURE` button located on the gateway enclosure
through the status LED extinguishing and the device executing an initial restart (requested after
~5 seconds). The user maintains the press through the boot stage until the red status LED repeatedly
turns on for 200 ms and off for 200 ms (`RR--RR--` pattern), typically occurring approximately
11 seconds after the initial press. Upon observing this cadence, the user releases the button.
Release triggers another restart, after which the unconfigured configuration hotspot opens
(`Configure Ruuvi Gateway XXXX`).

**Release-triggered restart:** The `main/ruuvi_gateway_main.c` boot-time erase handler waits for
release and then calls `gateway_restart_immediate_no_cleanup()`, which invokes `esp_restart()` in
`main/reset_task.c`.

*Note on Timing:* The ~11-second duration is the typical user-observed elapsed time required for the
initial five-second restart request, restart and startup processing up to the boot-time button-state
sample, and the factory recovery procedure. Releasing the button before that sample and keeping it
released preserves settings and allows normal boot. Once the sample detects a held button and
selects recovery, releasing the button does not cancel partition erasure, even if the red 200 ms
on/off completion pattern has not yet appeared. Recovery completes first; the gateway then restarts
once the button is released. For a factory reset, keep holding until the completion pattern appears,
then release.

#### Confirmation

The firmware provides explicit, deterministic confirmation of successful data erasure prior to
button release, followed by operational network state feedback:

1. **Firmware Erasure Notification:** Following the completion of `ruuvi_nvs_erase()` and
   `gw_cfg_storage_deinit_erase_init()`, the boot handler calls `leds_notify_cfg_erased()`, driving
   the status LED into the repeating `LEDS_BLINKING_ON_CFG_ERASED` pattern (200 ms red on / 200 ms
   off). This serves as the unequivocal signal that persistent parameters have been erased and the
   button can be released.
2. **Operational Confirmation:** Button release triggers another restart. After that restart,
   check that the device returns to its factory baseline state: former Wi-Fi station connections
   drop, the previous Web-UI session becomes unreachable, and the Gateway activates its local
   captive configuration hotspot (`Configure Ruuvi Gateway XXXX`). Accessing the administrative
   interface requires logging in with the unique, hardware-derived factory default password
   (`DEVICEID`) to begin
   initial onboarding.

---

### **ID**: DelFunc-Service-Account-Deletion

#### Description

Removes the user's online profile and associated account records from the official Ruuvi Cloud
backend infrastructure, completely purging personal identifiers and queuing telemetry logs for
permanent deletion.

#### Target Type

Personal data on associated services

#### Initiation and Interaction

The user signs into their account on the official Ruuvi Cloud web portal (https://station.ruuvi.com)
or Ruuvi Station mobile application and submits an account deletion request. To prevent accidental
or unauthorized data loss, the cloud system sends a confirmation link with an authorization token to
the user's registered email address. The user must click the confirmation link to authorize final
account eradication.

#### Confirmation

Upon user confirmation via the emailed link:

* The user is automatically logged out of active app and browser sessions, and the web interface
  displays an account termination confirmation message.
* All personally identifiable information (PII)—including account name, email address, custom sensor
  tag names, and gateway association mappings—is removed immediately from active operational
  databases.
* Historical sensor readings associated with the account are queued for automated background
  deletion from cloud databases in accordance with corporate data retention schedules.

---

## Summary Matrix for the Technical File

| Deletion ID                        | Targeted Storage Space                            | Scope of Data Purged                                                                                                         | Verification Evidence                                                                                                                                                                                                                         |
|:-----------------------------------|:--------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `DelFunc-Hardware-Factory-Reset`   | On-Device Flash Partitions (`nvs` / `gw_cfg_def`) | Low-level partition erase of passwords, network credentials, M2M tokens, custom server URLs, and uploaded certificates/keys. | LED blinking red 200 ms on / 200 ms off (`LEDS_BLINKING_ON_CFG_ERASED`) prior to release; release triggers another restart, after which check availability of configuration hotspot (`Configure Ruuvi Gateway XXXX`) and default credentials. |
| `DelFunc-Service-Account-Deletion` | Associated Service (Ruuvi Cloud Infrastructure)   | Permanent erasure of user profile, PII, gateway associations, and background queuing of sensor history logs.                 | Automated session logout; confirmation message rendered; account access revoked and user data permanently purged.                                                                                                                             |
