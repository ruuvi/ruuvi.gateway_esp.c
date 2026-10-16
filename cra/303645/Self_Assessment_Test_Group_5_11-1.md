# Test group 5.11-1: Simple Erasure of User Data from the Device

Provision 5.11-1 — Status: **M**. Related IXIT: `IXIT 10-SecParam`, `IXIT 21-PersData`,
`IXIT 25-DelFunc`.

## Evidence status — 2026-09-28 automation follow-up

The PASS verdicts and laboratory/dump statements below are the prior recorded assessment;
the new standalone Unit A runner does not independently revalidate them. Its latest supplied
terminal excerpt reported overall **ERROR**, including a remote-config fixture rejection and
an RW write that changed `mqtt_prefix`. The fixture correction and explicit MQTT preservation
workaround were subsequently verified offline; a corrected live PASS remains pending.
See
the [current task and live-run follow-up](tasks/task_5_11_1_2_a.md#reported-firmware-issue-and-live-run-follow-up).

The supported runner is HYBRID: an operator opens the hotspot, changes the host network, and
confirms the LED-completed factory reset. A real protected Wi-Fi LAN and authenticated target
services supply credential-use evidence. Plain HTTP/MQTT checks and certificate storage flags
do not establish HTTPS/mTLS handshake correctness. Verified default configuration and revoked
credentials after recovery are functional observations, not a new partition-dump or forensic
irrecoverability result. The recovered Unit A manifest is historical; Unit C needs a fresh
verified seed. Do not treat this follow-up as new evidence for the group-wide PASS below.

---

## Test case 5.11-1-1 (conceptual)

**Purpose**: To conceptually assess whether the user data erasure functionality in `IXIT 25-DelFunc`
can be performed by a non-technical user (Clause D.3) (`a`), is adequate to permanently erase
targeted user data from device flash memory (`b`), and covers all personal data, user
configurations, and user cryptographic material (`c`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A, B & C: Conceptual Assessment of User Data Erasure Functionality

| Deletion Functionality ID (`IXIT 25-DelFunc`) | Initiation & User Interaction (Unit a)                                                                                                                                                                                                                                                                                                                                                                          | Technical Erasure Method & Flash Adequacy (Unit b)                                                                                                                                                                                                                                                                                                                                    | Scope of Covered Personal Data, Config & Keys (Unit c)                                                                                                                                                                                                                                                                                                                                                                                                                   | Unit Verdict |
|:----------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`DelFunc-Hardware-Factory-Reset`**          | **Simple Physical Action.** The user presses and holds the physical `CONFIGURE` button through the initial reboot (~5–6 s) until the status LED blinks red 200 ms on / 200 ms off (~11 s total hold). After observing this feedback, release the button to trigger another restart, after which the configuration hotspot opens. Bypasses complex menus; easily executable by non-technical users (Clause D.3). | **Low-Level Partition Formatting.** During boot, `handle_reset_button_is_pressed_during_boot()` calls `ruuvi_nvs_erase()` and `gw_cfg_storage_deinit_erase_init()`, executing structural sector erase commands across the `nvs` and `gw_cfg_def` partitions. Bypasses append-only transaction logs to format flash sectors, resetting device parameters to baseline factory defaults. | **Complete On-Device User Data Coverage.** Permanently wipes all user Wi-Fi credentials (`PersData-Network-IP-Footprints`), administrator Web-UI credentials (`SecParam-LAN-WebUI-Credentials`), M2M Bearer tokens (`SecParam-LAN-Bearer-Tokens`), custom HTTP/MQTT target URLs, and custom SSL/TLS certificates and private keys stored in `gw_cfg_def`. Silicon hardware root seeds (`DEVICEID`) and factory MAC addresses are retained as immutable system constants. |   **PASS**   |

**Assessment Justification**: The technical declarations in `IXIT 25-DelFunc` fulfill all conceptual
criteria under Provision 5.11-1:

1. **User Accessibility (Unit a):** Executing a physical button hold until receiving unambiguous
   visual LED feedback is simple, deterministic, and requires no specialized technical knowledge.
2. **Erasure Adequacy (Unit b):** Direct formatting of both the `nvs` and `gw_cfg_def` partition
   blocks structurally purges the append-only NVS transaction tables and cryptographic storage
   sectors, preventing logical extraction or recovery of stored user settings.
3. **Data Scope Coverage (Unit c):** All personal data (`IXIT 21-PersData`), user settings, M2M
   tokens, and user-provisioned cryptographic keys (`IXIT 10-SecParam`) stored in on-device flash
   are completely purged.

**Verdict**: **PASS**

---

## Test case 5.11-1-2 (functional)

**Purpose**: To functionally verify on the DUT that typical user data is created (`a`), that user
data erasure initiation and interaction operate as documented (`b`), that target user data is
completely erased from device storage (`c`), and that multi-user privilege boundaries are assessed
if applicable (`d`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B, C & D: Functional User Data Creation, Erasure, and Inspection

**Testing Methodology**: The test laboratory injected non-default user data (custom Wi-Fi station
SSID/password, custom Web-UI admin password, M2M Bearer API keys, custom HTTPS telemetry targets,
and user SSL certificates), executed the long-press `CONFIGURE` button sequence (~11 seconds),
observed hardware and LED feedback, and inspected flash memory via local analysis tools
(`esptool.py`) and the Web-UI onboarding endpoints.

| Functional Test Stage                   | Test Action Executed on DUT                                                                                                                   | Observed Functional DUT Behavior & Verification                                                                                                                                                                                                                                                                                                                                                                                                                                 | Unit Verdict |
|:----------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Data Creation (Unit a)**              | Provision non-default Wi-Fi credentials, custom Web-UI admin password, M2M Bearer tokens, custom HTTPS targets, and private TLS certificates. | Settings successfully commit to `ruuvi.json` on the `nvs` partition; custom certificates and private keys are written to the `gw_cfg_def` partition. The DUT enters a personalized operational state.                                                                                                                                                                                                                                                                           |   **PASS**   |
| **Initiation Verification (Unit b)**    | Press and hold the physical `CONFIGURE` button for about 11 seconds until the status LED blinks red repeatedly (200 ms on / 200 ms off).      | At ~5-6 seconds, the status LED turns off and the device executes a restart. The button is held through the boot sequence. At ~11 seconds total elapsed time, `main_task_init()` executes partition formatting and signals completion via `leds_notify_cfg_erased()`, causing the status LED to blink red repeatedly (200 ms on / 200 ms off). Button release triggers another restart, after which check availability of the default hotspot (`Configure Ruuvi Gateway XXXX`). |   **PASS**   |
| **Post-Erasure Inspection (Unit c)**    | Re-connect to the DUT via the configuration hotspot (`Configure Ruuvi Gateway XXXX`) and inspect flash partition contents via `esptool.py`.   | **Target User Data Completely Cleared.** The management interface reverts to requiring the factory-default `DEVICEID` password. Stored station SSIDs, custom passwords, M2M tokens, target endpoints, and uploaded mTLS keys are completely absent. Partition dumps confirm that `nvs` and `gw_cfg_def` user allocation tables have been formatted and re-initialized to factory defaults.                                                                                      |   **PASS**   |
| **Multi-User Isolation Check (Unit d)** | Evaluate multi-user role capabilities.                                                                                                        | The DUT operates strictly as a single-tenant embedded gateway with a single administrator access context. Executing a hardware factory reset resets total device state to factory defaults, which is fully appropriate for a single-tenant hardware appliance.                                                                                                                                                                                                                  |   **PASS**   |

**Assessment Justification**: Functional verification confirms that `DelFunc-Hardware-Factory-Reset`
operates exactly as specified in `IXIT 25-DelFunc`. Holding the physical `CONFIGURE` button through
the 5–6 second restart until the 200 ms red on/off LED confirmation sequence (~11 seconds total)
triggers `ruuvi_nvs_erase()` and `gw_cfg_storage_deinit_erase_init()`. Post-reset memory inspection
verifies that all user configurations, network credentials, API tokens, and uploaded cryptographic
material are purged.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.11-1-1 & 5.11-1-2

| Test Case           | Purpose / Focus                    | Assessment Summary                                                                                                      | Unit Verdict |
|:--------------------|:-----------------------------------|:------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.11-1-1 Unit a** | Simplicity Assessment (Clause D.3) | Physical button hold until visual LED feedback (~11 seconds) is easily accessible to non-technical users.               |   **PASS**   |
| **5.11-1-1 Unit b** | Erasure Method Adequacy            | Low-level formatting across `nvs` and `gw_cfg_def` cleanly eradicates persistent storage tables.                        |   **PASS**   |
| **5.11-1-1 Unit c** | User Data Scope Coverage           | Covers all personal data (`IXIT 21-PersData`), user configurations, M2M tokens, and uploaded keys (`IXIT 10-SecParam`). |   **PASS**   |
| **5.11-1-2 Unit a** | Typical User Data Creation         | Custom configurations, credentials, M2M tokens, and TLS keys successfully provisioned.                                  |   **PASS**   |
| **5.11-1-2 Unit b** | Initiation & Interaction Check     | Long press triggers reboot at ~5–6 s and completes partition erase with red 200 ms on/off LED signal at ~11 s.          |   **PASS**   |
| **5.11-1-2 Unit c** | Post-Erasure Data Absence Check    | Memory dumps and captive portal inspection confirm complete absence of user data and credentials.                       |   **PASS**   |
| **5.11-1-2 Unit d** | Multi-User Boundary Check          | Single-tenant device architecture cleanly resets total system state upon hardware factory reset.                        |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.11-1 of `ETSI EN 303 645`. The device
provides a simple, physical user data erasure mechanism (`DelFunc-Hardware-Factory-Reset`) initiated
by a physical long press on the `CONFIGURE` button. The button is held through an initial reboot at
5–6 seconds until the firmware executes partition erasure across the `nvs` and `gw_cfg_def`
partitions, signaling completion via a distinct red 200 ms on / 200 ms off status LED pattern
(approximately 11 seconds total hold duration). Conceptual evaluation and functional testing confirm
that all user-provisioned network credentials, administrative passwords, M2M Bearer tokens,
telemetry URLs, and private SSL/TLS keys are permanently eradicated from the device.

**Group Verdict**: **PASS**
