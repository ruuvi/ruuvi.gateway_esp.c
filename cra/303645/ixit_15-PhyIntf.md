# IXIT 15-PhyIntf: Physical Interfaces

The following declarations map the complete physical and wireless interface footprint of the Ruuvi
Gateway (DUT), specifying their activation status and the physical or logical protection methods
applied to limit unauthorized exposure.

## Table C.15: IXIT 15-PhyIntf (Physical Interfaces)

### **ID**: PhyIntf-USB

#### Description

External Type-C USB port used for powering the device, reading system logs via an integrated virtual
USB-to-UART serial bridge, and local recovery flashing during production assembly or hardware
service.

#### Type

Physical port

#### Status

Enabled. The physical interface must remain connected during operation to provide power to the
Gateway and allow local debugging output.

#### Debug Interface

Yes (Passive Diagnostic Logging Only).

#### Protection

Exposed externally. The interface is protected logically by its firmware configuration: it operates
strictly as a one-way, read-only text transmission stream for system logs. It does not host an
interactive command shell, login prompt, or execution runtime interface. Unauthorized firmware
flashing via this port is blocked at the hardware level during operation; the SoC must be completely
power-cycled while a separate internal GPIO pin is physically strapped low to accept new code
images.

---

### **ID**: PhyIntf-Ethernet

#### Description

Physical RJ-45 Ethernet interface used for network-based telemetry relaying to cloud services and
accessing the local configuration Web-UI.

#### Type

Network, physical port

#### Status

Enabled. Necessary to fulfill the device's primary operational function (relaying BLE data to cloud
or local servers) and supporting wired configuration.

#### Debug Interface

No

#### Protection

Exposed externally. Physical protection is provided by the device casing, which encloses all
internal processor lines and exposes only the standard isolated network jack interface connector.
Software-tier protection is enforced at the application layer: the internal HTTP server explicitly
rejects unauthenticated configuration mutations and gates local programmatic requests via strict
access control tokens.

---

### **ID**: PhyIntf-WiFi

#### Description

Internal 2.4 GHz Wi-Fi air interface utilizing the ESP32 radio module for wireless network
connectivity and initial captive portal configuration.

#### Type

Network, air interface

#### Status

Enabled (On-Demand / Configurable). Activated during the transient provisioning phase (as a captive
portal hotspot) or permanently if configured by the user as the primary internet backhaul.

#### Debug Interface

No

#### Protection

Logical network protection applies. Standard wireless security protocols (WPA2/WPA3 Personal)
restrict interface access to authorized stations when acting as a client. When operating as an
unencrypted provisioning hotspot, the interface enforces a strict structural runtime timeout
framework (1 hour) before automatically tearing down the radio network to minimize exposure.

---

### **ID**: PhyIntf-BLE-ESP32

#### Description

The hardware Bluetooth/BLE peripheral macro inside the core ESP32 SoC chip.

#### Type

Air interface

#### Status

Disabled.

#### Debug Interface

No

#### Protection

Permanently disabled at the application initialization phase. The ESP-IDF software application
structure completely disables the radio stack on the ESP32 for Bluetooth operations to optimize
resource allocation and eliminate the wireless attack surface on this macro. No software hooks or
handlers exist in the production firmware binary to bind this stack to an active logical interface.

---

### **ID**: PhyIntf-BLE-nRF52

#### Description

Dedicated Bluetooth Low Energy (2.4 GHz) air interface managed by the nRF52811 co-processor running
nRF5 SDK v15.3.0.

#### Type

Network, air interface

#### Status

Enabled. Required to scan, receive, and decode broadcast data structures from nearby environmental
monitors (e.g., RuuviTag, Ruuvi Air).

#### Debug Interface

No

#### Protection

Rx-Only Architecture. The custom co-processor logic enforces a passive listener paradigm. The radio
firmware stack is compiled without connection or pairing handlers; it does not accept incoming BLE
connection requests, bonding requests, or pairing inputs from unknown peripherals, neutralizing
remote over-the-air injection risks.

---

### **ID**: PhyIntf-SWD-nRF52

#### Description

Internal Serial Wire Debug (SWD) connector pad array located directly on the mainboard PCB, utilized
strictly for initial factory programming of the nRF52811 co-processor binary.

#### Type

Physical port (Internal pads)

#### Status

Disabled during active firmware execution.

#### Debug Interface

Yes (Factory Provisioning Only).

#### Protection

The interface is protected through a combination of physical and logical barriers:

1. **Enclosure Deterrence:** The interface pads are internal, contained entirely within the device's
   plastic casing shell. Accessing the pads requires the physical use of tools (a screwdriver) to
   open the unit enclosure.
2. **Logical Pin Lockout:** Once the primary application firmware executes on the co-processor
   during the boot cycle, the SWD debug pins are automatically reconfigured in software as standard
   General Purpose Input/Output (GPIO) pins for alternative system status tasks. Hardware debugging
   or memory readout via these pins is rendered electrically impossible while the core firmware is
   active.

---

### **ID**: PhyIntf-Configure-Button

#### Description

A physical, mechanical push-button component soldered directly onto the mainboard PCB and exposed
externally through a dedicated cutout structural opening at the back of the device.

#### Type

Physical button / Human-Interface Component

#### Status

Enabled. Continuously monitored via an unprivileged background GPIO interrupt loop to capture user
interaction inputs.

#### Debug Interface

No (Acts strictly as a system state-machine toggle control).

#### Protection

Protected against arbitrary or accidental activation through structural engineering: the button is
recessed inside the enclosure profile, requiring a purposeful manual finger press or a tool
insertion to engage. Protection against malicious exploitation is handled via firmware-enforced time
thresholds (debouncing and timed long-press tracking counters) to separate setup requests from total
hardware formatting loops.

---

## Summary Matrix for the Technical File

| Interface ID                 | Type            | Runtime Status  | Debug Capability | Primary Protection                                                        |
|:-----------------------------|:----------------|:----------------|:----------------:|:--------------------------------------------------------------------------|
| **PhyIntf-USB**              | Physical Port   | Enabled         | Log-Output Only  | Read-only TX log output streaming; hardware boot-strap flash constraints. |
| **PhyIntf-Ethernet**         | Physical Port   | Enabled         |        No        | Casing enclosure isolation + Logical application-layer token gating.      |
| **PhyIntf-WiFi**             | Air Interface   | User-Configured |        No        | WPA2/WPA3 infrastructure rules & automated hotspot timeout windows.       |
| **PhyIntf-BLE-ESP32**        | Air Interface   | Disabled        |        No        | Compiled out of application firmware maps entirely.                       |
| **PhyIntf-BLE-nRF52**        | Air Interface   | Enabled         |        No        | Connectionless, passive Rx-only listener architecture.                    |
| **PhyIntf-SWD-nRF52**        | Internal Pads   | Disabled        |   Factory Only   | Casing tool access requirement + Software boot-time GPIO remapping.       |
| **PhyIntf-Configure-Button** | Physical Button | Enabled         |        No        | Recessed enclosure placement & firmware time-gated action filters.        |