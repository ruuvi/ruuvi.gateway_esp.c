# IXIT 27-UserIntf: User Interfaces

The following declarations detail all physical and logical user interfaces present on or exposed by
the Device Under Test (DUT) that enable administrative input, configuration provisioning, or state
modification by the operator.

---

## Table C.27: IXIT 27-UserIntf (User Interfaces)

### **ID**: UserIntf-Physical-Configure-Button

#### Description

A physical, mechanical tactile push-button labeled `CONFIGURE` located externally on the gateway's
plastic profile casing. It allows the operator to interact directly with the device's running state
machine without relying on active network access.

Physical press and release events are registered via GPIO interrupts and handled by the background
firmware task `reset_task_handle_sig_configure_button_event()`. The physical interface supports
three distinct operational modes depending on press duration and system reboot state:

* **Short Press (press and release within 2 seconds):** This is conservative operator guidance
  for activating the hotspot without requesting reboot; two seconds is not a firmware branch
  threshold. Release cancels the five-second restart timer and signals the runtime engine to
  terminate active station connections and spawn the unencrypted local Wi-Fi provisioning hotspot
  (`Configure Ruuvi Gateway XXXX`) with its captive onboarding portal
  (`UserIntf-Local-Hotspot-Captive-Portal`).
  Existing configuration parameters in NVS flash are preserved. No timed debounce filter is
  implemented. Ignoring presses released within 50 ms is planned
  under [issue #1658](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1658), milestone
  `v1.17.6`.
* **Reboot Request (~5 seconds hold):** Holding the button for 5 seconds triggers a
  software-initiated system restart (`gateway_restart()`). The status LED extinguishes to indicate
  the pending reboot. Releasing the button immediately upon the LED turning off restarts the device
  cleanly without modifying stored parameters.
* **Hardware Factory Reset (boot-held, ~11 seconds total hold):** If the user continues to hold the
  button through the reboot sequence until the boot-time button-state sample, factory recovery
  starts. For restart-only operation, use a **3 seconds window** after restart to release the
  button and keep it released. The current sample is reported at approximately 3600 ms; enforcing
  a minimum of 3000 ms is pending
  in [issue #1657](https://github.com/ruuvi/ruuvi.gateway_esp.c/issues/1657).
  Release after the sample selects recovery does not cancel erasure. The boot handler
  `handle_reset_button_is_pressed_during_boot()` (invoked in `main_task_init()`) executes partition
  formatting (`ruuvi_nvs_erase()` and `gw_cfg_storage_deinit_erase_init()`) across `nvs` and
  `gw_cfg_def`. Recovery takes approximately 500 ms. Following
  successful formatting, the firmware invokes `leds_notify_cfg_erased()`, causing the status LED to
  blink red repeatedly in a 200 ms on / 200 ms off cadence (`RR--RR--`). Release the button upon
  observing this feedback; release triggers another restart, after which the setup hotspot opens
  with factory credentials. See the release-triggered restart description in
  [IXIT 25](ixit_25-DelFunc.md#initiation-and-interaction).

#### Type

Physical interface, cross-referenced to `PhyIntf-Configure-Button`. Operational erasure and reset
mechanisms are cross-referenced to `DelFunc-Hardware-Factory-Reset`.

---

### **ID**: UserIntf-Local-Hotspot-Captive-Portal

#### Description

The temporary, browser-driven onboarding setup wizard presented automatically to the user when
bridging a smartphone or laptop directly to the gateway's unencrypted local wireless setup
network (`Configure Ruuvi Gateway XXXX`).

* **Access Vector:** The user connects their wireless provisioning client to the wireless medium,
  triggering native Captive Portal redirection routines over the network. Under the hood, this
  configuration experience is driven by three tightly bounded logical sub-services:
  `LogIntf-Hotspot-DHCP-Server` (to lease the client an IP block), `LogIntf-Hotspot-DNS-Server` (to
  capture and force-redirect web queries), and `LogIntf-HTTP-Server` (serving the actual HTML layout
  inputs on Port 80).

#### Type

Logical interface, cross-referenced to `LogIntf-HTTP-Server`

---

### **ID**: UserIntf-LAN-Management-WebUI

#### Description

The browser-based management dashboard served directly by the embedded HTTP server running on the
ESP32 platform, accessible once the gateway successfully joins the customer's
local area network (LAN/WLAN).

* **Access Vector:** The user navigates to the device's locally assigned IP address over HTTP Port
  80 via a standard web browser interface (`LogIntf-HTTP-Server`).
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

| User Interface ID                         | Interface Media Type            | Access / Navigation Method                                  | Permitted Input Capabilities                                                                                                                             | Cross-Referenced Registry ID                                                               |
|:------------------------------------------|:--------------------------------|:------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------|
| **UserIntf-Physical-Configure-Button**    | Physical Hardware Input         | Tactile push-button on enclosure exterior                   | Hotspot activation (<2 s); system restart (~5 s); boot-held partition formatting & factory reset (~11 s total hold, confirmed by 200 ms red on/off LED). | **PhyIntf-Configure-Button**<br>`DelFunc-Hardware-Factory-Reset`                           |
| **UserIntf-Local-Hotspot-Captive-Portal** | Logical Local Radio Interface   | Bridging to `Configure Ruuvi Gateway XXXX` unencrypted SSID | Full initial onboarding wizard path execution (SSID, passkeys, network settings, cloud targets).                                                         | **LogIntf-HTTP-Server** / **LogIntf-Hotspot-DHCP-Server** / **LogIntf-Hotspot-DNS-Server** |
| **UserIntf-LAN-Management-WebUI**         | Logical Local Network Interface | Navigating local station IP via HTTP Port 80                | Maintenance, token creation, SSL certificate storage, and manual OTA updates. (Network medium switches locked).                                          | **LogIntf-HTTP-Server**                                                                    |
