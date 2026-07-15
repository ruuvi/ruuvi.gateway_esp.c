# IXIT 25-DelFunc: Deletion Functionalities

The following declarations detail the local hardware and remote service-level deletion
functionalities implemented to permanently eradicate user configuration profiles, network
credentials, private keys, and cached device metadata.

---

## Table C.25: IXIT 25-DelFunc (Deletion Functionalities)

### **ID**: DelFunc-Hardware-Factory-Reset

#### Description

Permanently formats and wipes all user-provisioned system parameters, station network credentials,
programmatic tokens, and security parameters from the device's persistent flash memory blocks.

#### Target Type

User data on the device

#### Initiation and Interaction

The user must manually press and hold the physical `CONFIGURE` button located on the gateway
enclosure for **7 seconds or longer**. This long-press action bypasses logical software controls and
forces the underlying ESP-IDF operating framework to issue a low-level structural formatting and
erase command across both the `nvs` partition (housing `ruuvi.json`) and the expanded `gw_cfg_def`
partition (housing custom SSL certificates and private keys).

#### Confirmation

The gateway provides immediate physical feedback by executing a complete hardware restart sequence (
`gateway_restart()`). Upon boot, the device returns to its factory baseline state: it terminates all
standard Wi-Fi or Ethernet station configurations and activates its local configuration hotspot. The
user confirms successful deletion when the active Wi-Fi station connection drops, the previous local
LAN management Web-UI becomes unreachable, and the system requires the unique, hardware-derived
factory default password to re-enter Step 1 of the onboarding configuration wizard.

---

### **ID**: DelFunc-WebUI-Token-Erasure

#### Description

Allows an administrator to selectively deactivate and permanently purge programmatic
Machine-to-Machine (M2M) authorization bearer tokens directly from the system configuration matrix.

#### Target Type

User data on the device

#### Initiation and Interaction

The user must navigate to the advanced configuration dashboard within the Web-UI panel, clear the
text fields for the Read-Only token (`lan_auth_api_key`) or the Read/Write token (
`lan_auth_api_key_rw`) so that they are completely empty, and click the "Save" button.

#### Confirmation

The Web-UI displays a success banner confirming configuration storage. The gateway updates the
active flash parameters configuration and blocks any subsequent inbound programmatic API requests
attempting to target local endpoints (such as `GET /history` or `POST /ruuvi.json`) using the
deleted keys, instantly returning unauthenticated HTTP 401 error frames.

---

### **ID**: DelFunc-Cloud-Relay-Deactivation

#### Description

Instructs the gateway to completely cease communication and data forwarding to the official cloud
platform, terminating remote tracking. The specific associated service covered by this functionality
is the **Ruuvi Cloud** backend infrastructure.

#### Target Type

Personal data on associated services

#### Initiation and Interaction

The administrator must connect to the local configuration hotspot or LAN Web-UI, navigate to the
data routing control menus, toggle the "Ruuvi Cloud" target relay switch to the "Disabled" state,
and click save. (The same action can be applied to custom HTTP/HTTPS or MQTT/MQTTS/WS/WSS endpoints,
as well as the background diagnostics statistics switch to disable
`https://network.ruuvi.com/status` transmissions).

#### Confirmation

The Web-UI shows that the target switch state is saved as disabled. Over the wire, the gateway
instantly tears down its outbound HTTPS client tasks and stateful socket connections, halting the
transmission of JSON telemetry envelopes to the manufacturer's backend. The user can log into their
official Ruuvi Cloud account dashboard to confirm that the unique gateway MAC identifier registers
an offline status marker and that no new real-time metrics are recorded.

---

## Summary Matrix for the Technical File

| Deletion ID                          | Targeted Storage Space                  | Scope of Data Purged                                                   | Verification Evidence                                                                          |
|:-------------------------------------|:----------------------------------------|:-----------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------|
| **DelFunc-Hardware-Factory-Reset**   | Flash Partitions (`nvs` / `gw_cfg_def`) | Complete erasure of passwords, SSIDs, private keys, and custom certs.  | Hotspot activates; system drops user passwords and reverts to step 1 of the onboarding wizard. |
| **DelFunc-WebUI-Token-Erasure**      | Local Config Parameter Fields           | Wipes individual M2M bearer tokens, revoking API endpoints.            | Scripted API queries instantly return HTTP 401 access errors.                                  |
| **DelFunc-Cloud-Relay-Deactivation** | Outbound Telemetry Task Memory          | Disables data streaming loops to Ruuvi Cloud or custom remote servers. | Associated cloud dashboard registers device status as permanently offline.                     |
