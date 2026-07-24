# IXIT 27-UserIntf: User Interfaces

The following declarations detail all physical and logical user interfaces present on or exposed by
the Device Under Test (DUT) that enable administrative input, configuration provisioning, or state
modification from the operator.

---

## Table C.27: IXIT 27-UserIntf (User Interfaces)

### **ID**: UserIntf-Physical-Configure-Button

#### Description

A physical, mechanical push-button labeled `CONFIGURE` located structurally on the gateway's
exterior plastic profile casing. It allows the operator to interact directly with the device's
running state machine without relying on active network access.

* **Short Press (under 2 seconds):** Signals the background GPIO interrupt loop to force the device
  to spawn its transient local Wi-Fi provisioning hotspot (Captive Portal) to allow configuration
  changes over the air.
* **Long Press (7 seconds or longer):** Triggers a hardware-level factory reset, formatting all
  non-volatile parameters blocks (`nvs` and `gw_cfg_def`) and completely erasing stored credentials,
  keys, or custom security certificates.

#### Type

Physical interface, cross-referenced to `PhyIntf-Configure-Button`

---

### **ID**: UserIntf-Local-Hotspot-Captive-Portal

#### Description

The temporary, browser-driven onboarding setup wizard presented automatically to the user when
bridging a smartphone or laptop directly to the gateway's unencrypted local wireless setup
network (`Configure Ruuvi Gateway XXXX`).

* **Access Vector:** The user connects their wireless provisioning client to the wireless medium,
  triggering native Captive Portal redirection routines over the network. Under the hood, this
  configuration experience is driven by three tightly bounded logical sub-services:
  **`LogIntf-Hotspot-DHCP-Server`** (to lease the client an IP block),
  **`LogIntf-Hotspot-DNS-Server`** (to capture and force-redirect web queries),
  and **`LogIntf-HTTP-Server`** (serving the actual HTML layout inputs on Port 80).

#### Type

Logical interface, cross-referenced to `LogIntf-HTTP-Server`

---

### **ID**: UserIntf-LAN-Management-WebUI

#### Description

The browser-based management dashboard served directly by the embedded HTTP server running on the
ESP32 platform, accessible once the gateway successfully joins the customer's
local area network (LAN/WLAN).

* **Access Vector:** The user navigates to the device's locally assigned IP address over HTTP Port
  80 via a standard web browser interface (**`LogIntf-HTTP-Server`**).
* **Security & Input Constraints:** Access is gated by a challenge-response form requiring the
  unique factory default password printed on the casing label (derived from `DEVICEID`) or an
  explicit user-defined administrator password. It allows partial parameter changes—such as custom
  server routing updates, M2M token creation, custom certificate uploads, and manual firmware update
  triggers.
* *Note on Isolation:* To safeguard network stability and prevent accidental lockouts, core network
  connection medium choices (switching between Wi-Fi and Ethernet) are completely locked down and
  are **not** permitted to be changed via this LAN interface; they can only be altered through the
  captive portal environment.

#### Type

Logical interface, cross-referenced to `LogIntf-HTTP-Server`

---

## Summary Matrix for the Technical File

| User Interface ID                         | Interface Media Type            | Access / Navigation Method                                  | Permitted Input Capabilities                                                                                    | Cross-Referenced Registry ID                                                               |
|:------------------------------------------|:--------------------------------|:------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------|
| **UserIntf-Physical-Configure-Button**    | Physical Hardware Input         | Tactile push-button on enclosure exterior                   | Hotspot toggling; Low-level NVS flash memory partition formatting.                                              | **PhyIntf-Configure-Button**                                                               |
| **UserIntf-Local-Hotspot-Captive-Portal** | Logical Local Radio Interface   | Bridging to `Configure Ruuvi Gateway XXXX` unencrypted SSID | Full initial onboarding wizard path execution (SSID, passkeys, network settings, cloud targets).                | **LogIntf-HTTP-Server** / **LogIntf-Hotspot-DHCP-Server** / **LogIntf-Hotspot-DNS-Server** |
| **UserIntf-LAN-Management-WebUI**         | Logical Local Network Interface | Navigating local station IP via HTTP Port 80                | Maintenance, token creation, SSL certificate storage, and manual OTA updates. (Network medium switches locked). | **LogIntf-HTTP-Server**                                                                    |
