# IXIT 26-UserDec: User Decisions

The following declarations detail all security-relevant choices, parameter selections, and
administrative permissions presented to the operator during the initial step-by-step onboarding
wizard or subsequent device maintenance lifecycles.

---

## Table C.26: IXIT 26-UserDec (User Decisions)

### **ID**: UserDec-1-Network-Medium-Selection

#### Description

The user selects the physical network interface used for primary outbound internet backhaul. This is
the first action presented in the onboarding sequence.

#### Options

* `Ethernet`
* `Wi-Fi`
* **Default:** The gateway initializes its local configuration hotspot mode. After 60 seconds, it
  triggers a link-state check on the physical interface; if a live cable connection is detected on
  the RJ-45 port, `Ethernet` is applied as the default selected medium.

#### Triggered By

Triggered automatically on initial out-of-the-box boot or following a hardware factory reset
execution. Post-installation, this setting cannot be altered via the LAN management dashboard; it
can only be re-triggered by the user physically pressing the `CONFIGURE` button to reactivate the
local provisioning hotspot.

---

### **ID**: UserDec-2-Interface-Configuration

#### Description

Positioned as the second step in the setup wizard, the user defines the addressing rules and network
parameters for the interface selected in Step 1.

#### Options

* **For Ethernet:** `Use DHCP` (Default) *OR* `Manual IP settings` (Static IP, Netmask, Gateway, and
  DNS strings).
* **For Wi-Fi:** `Select from visible network list` *OR* `Enter SSID/Password manually` *OR*
  `Use WPS to connect`.

#### Triggered By

Triggered automatically by the onboarding state machine following interface selection.
Post-installation modification is restricted to the captive portal environment.

---

### **ID**: UserDec-3-Onboarding-Firmware-Update

#### Description

The third screen of the onboarding wizard automatically triggers an online firmware integrity check.
The user decides whether to flash available software updates before completing device deployment.

#### Options

* `Install Update` (Available only if a newer version string is identified on the remote update
  index) *OR* `Skip / Keep Current`.
* **Default:** User interaction required; no automated update is performed on this screen.

#### Triggered By

Automated background checking upon entering the "Software Update" screen framework.

---

### **ID**: UserDec-4-Automatic-Configuration-Download

#### Description

The fourth step allows the user to decide if the device configuration profile should be fetched
automatically from a centralized remote management provisioning endpoint.

#### Options

* `Enabled` (Requires entering a valid destination server profile URL) *OR* `Disabled` (Default).

#### Triggered By

Wizard loop progression to the "Automatic Configuration Download" view. This setting can also be
triggered and modified voluntarily by the user post-initialization via the Web-UI maintenance menu.

---

### **ID**: UserDec-5-Automatic-Updates

#### Description

The fifth configuration panel manages long-term operational firmware update checking behaviors,
release channel targets, and automated patch delivery schedules.

#### Options

* **Update Policy:** `Auto update` (Default) *OR* `Auto update (for beta testers)` *OR*
  `Manual updates only`.
* **Schedule Filters:** Selection checkboxes for specific days of the week and text fields defining
  permissible time-of-day execution intervals.

#### Triggered By

Wizard loop progression to the "Automatic Updates" screen. This configuration can be triggered and
altered by an authenticated user at any stage during the device maintenance lifecycle via the
Web-UI.

---

### **ID**: UserDec-6-Remote-Access-Settings

#### Description

The sixth step handles administrative entry boundaries for the local management Web-UI alongside
stateless machine-to-machine (M2M) API token permissions.

#### Options

* **Authentication Profile:**
  * `Password protected with the default password` (Default; utilizes the unique `DEVICEID` string
    printed on the physical enclosure label).
  * `Protected with a custom password` (Requires definition of a unique administrator username and
    password sequence).
  * `Not configurable via remote connection` (Completely disables the network HTTP listening task on
    Port 80 post-setup).
  * `Remote configurable without a password` (Unsafe configuration state; permits open
    modification).
* **Local M2M API Keys (Independent Toggles):**
  * `Enable read-only access to "/history" using API key` (`lan_auth_api_key`).
  * `Enable full (read/write) access to the Ruuvi Gateway router using API key` (
    `lan_auth_api_key_rw`).

#### Triggered By

Wizard loop progression to the "Remote Access Settings" dashboard. Can be triggered dynamically
on-demand by the administrator via the Web-UI parameters panel.

---

### **ID**: UserDec-7-Cloud-Options

#### Description

The seventh junction points the gateway toward out-of-the-box data channeling or advanced
multi-target telemetry routing routing controls.

#### Options

* `Ruuvi Cloud (recommended)` (Default; channels JSON metrics strings directly to official platform
  endpoints and terminates the setup wizard).
* `Use Ruuvi Cloud and/or a custom server and configure other advanced settings` (Advances the
  wizard track to the "Custom Server" entry panel).

#### Triggered By

Wizard loop progression to the "Cloud Options" view.

---

### **ID**: UserDec-8-Custom-Server-Routing

#### Description

Displayed if the advanced track is chosen in Step 7, this dashboard manages individual destination
endpoints and diagnostic feedback channels.

#### Options

* **Ruuvi Cloud Target Relay:** `Enabled` (Default) *OR* `Disabled`.
* **Custom HTTP Target:** `Enabled` *OR* `Disabled` (Requires destination HTTP/HTTPS URL path
  parameter string).
* **Custom MQTT / WS Target:** `Enabled` *OR* `Disabled` (Supports MQTT, MQTTS, WS, or WSS transport
  strings).
* **Statistics Reporting:** `Send to Ruuvi Cloud` (Default) *OR* `Send to a custom server` *OR*
  `Do not send statistics`.

#### Triggered By

Triggered automatically if advanced routing settings were requested in Step 7. Can be accessed and
updated at any point post-onboarding via the Web-UI data routing configurations.

---

### **ID**: UserDec-9-Time-Synchronization-Options

#### Description

The ninth step configures network time query preferences used to calculate valid local telemetry
timestamps and validate TLS connection states.

#### Options

* `Use default set of time servers (NTP servers)` (Default; targets standard cloud pools).
* `Define time servers to be used for time synchronisation` (Requires specific IP/Domain target
  strings).
* `Use time servers offered by your DHCP server`.
* `Don't use time synchronisation` (Operates on unsynced internal clocks).

#### Triggered By

Wizard loop progression to the time parameters panel. Available for continuous user modifications
via the Web-UI dashboard.

---

### **ID**: UserDec-10-Bluetooth-Scanning

#### Description

The final onboarding screen dictates the behavioral filters, channel masks, and parsing rules
applied to the nRF52 radio scanning engine.

#### Options

* **Filtering Mode:**
  * `Listen to Ruuvi sensors only` (Default; drops non-Ruuvi manufacturer hex structures).
  * `Filter Bluetooth messages by manufacturer ID` (Permits tracking specific third-party beacons
    via numeric company identifier definitions).
  * `Listen to all Bluetooth beacon messages` (Disables manufacturer filtering restrictions
    entirely).
* **Radio Constraints (Available for Custom/All filtering modes):**
  * Active PHY Selection: Checkbox selectors for `1M`, `2M`, and `Coded` layers.
  * Active Channel Overrides: Bitmask selectors for BLE wireless channels `37`, `38`, and/or `39`.
* **Hardware Address List Controls:** Toggle controls to enforce `Whitelist` or `Blacklist` MAC
  address matching array tables.

#### Triggered By

Wizard loop progression to the final "Bluetooth Scanning" configuration view. Fully adjustable
post-installation via the authenticated Web-UI settings menu.

---

## Summary Matrix for the Technical File

| Onboarding Step | Decision / Parameter Name    | Default State           | Security & Privacy Controls Enforced                                               | Post-Setup User Trigger Vector     |
|:----------------|:-----------------------------|:------------------------|:-----------------------------------------------------------------------------------|:-----------------------------------|
| **Step 1**      | Network Medium Selection     | Ethernet (Autodetect)   | Controls the active physical interface and closes unneeded paths.                  | Physical Button Setup Hotspot Only |
| **Step 2**      | Interface Configuration      | DHCP Enabled            | Isolates network routing variables or enforces static IP rules.                    | Physical Button Setup Hotspot Only |
| **Step 3**      | Onboarding Firmware Update   | User Intent Required    | Prevents deployment with out-of-date components or known vulnerabilities.          | Automated on-entry wizard check    |
| **Step 4**      | Auto Configuration Download  | Disabled                | Prevents unauthorized external profile injection if unconfigured.                  | LAN Web-UI Maintenance Menu        |
| **Step 5**      | Automatic Updates            | Auto Update             | Balances automated patch management with network maintenance schedules.            | LAN Web-UI Maintenance Menu        |
| **Step 6**      | Remote Access Settings       | Unique Case Password    | Hardens Web-UI entry points and gates programmatic local M2M API tokens.           | LAN Web-UI Account Settings        |
| **Step 7**      | Cloud Options                | Ruuvi Cloud Enabled     | Routes data to standard official endpoints or opens advanced custom targets.       | Junction choice in setup flow      |
| **Step 8**      | Custom Server Routing        | Cloud Active / Stats On | Isolates, drops, or duplicates telemetry streams (HTTP/MQTT) and health stats.     | LAN Web-UI Data Routing Panel      |
| **Step 9**      | Time Synchronization Options | Default NTP Servers     | Establishes uniform time to validate TLS certificates and logs.                    | LAN Web-UI Settings Menu           |
| **Step 10**     | Bluetooth Scanning           | Ruuvi Sensors Only      | Limits radio capture scope and applies Whitelist/Blacklist hardware address masks. | LAN Web-UI Advanced Radio Panel    |
