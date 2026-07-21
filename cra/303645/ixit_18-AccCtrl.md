# IXIT 18-AccCtrl: Access Control

The following declarations detail the hardware-level access control mechanisms, peripheral
protections, and partition barriers enforced by the device architecture to control and isolate
memory space access.

## Table C.18: IXIT 18-AccCtrl (Access Control)

### **ID**: AccCtrl-Hardware-Flash-Partition-Isolation

#### Description

The ESP32 platform controls memory boundaries by physically mapping external SPI flash memory into
distinct, hardware-isolated partition slots managed via the structural partition table layout.

* **Hardware Mechanism:** The microcontroller leverages dedicated internal Memory Management Unit (
  MMU) cache boundaries to divide flash access at the hardware bus tier. The running software
  application code is strictly restricted to code execution slots linked via the Instruction Bus (
  IBUS). Non-executable storage blocks—including the NVS configuration partitions (`nvs` and
  `gw_cfg_def`) and the read-only asset partitions (`fatfs_gwui` and `fatfs_nrf52`)—are mapped
  strictly via the Data Bus (DBUS).
* **Operating System Support:** The underlying ESP-IDF runtime framework utilizes this hardware
  layout to abstract low-level flash operations. The execution engine enforces rigid instruction
  space restrictions: any active application thread running code within an active execution slot (
  `ota_0` or `ota_1`) is physically prevented from executing instructions or jumping to vectors
  mapped to the data-only DBUS partition ranges.

---

### **ID**: AccCtrl-CoProcessor-Bus-Separation

#### Description

Hardware-level access control is enforced at the silicon bus tier through absolute physical circuit
isolation between the primary network system and the Bluetooth scanning sub-system.

* **Hardware Mechanism:** The nRF52811 chip houses its own internal flash, RAM, and internal
  peripheral buses completely disconnected from the ESP32's internal memory matrix. Inter-processor
  communication is strictly restricted to a dedicated physical hardware UART serial interface
  channel.
* **Operating System Support:** The nRF5 SDK v15.3.0 application running on the co-processor
  encapsulates the BLE radio stack within its own standalone physical boundaries. Because the two
  microcontrollers do not share dual-port RAM structures or common Direct Memory Access (DMA)
  vectors, a complete compromise of the network-facing ESP32 execution threads cannot read or mutate
  the internal memory registers, RAM sectors, or flash space of the nRF52811.

---

### **ID**: AccCtrl-Hardware-Debug-Interface-Protection

#### Description

Hardware-level access control over the device's physical low-level debugging vectors and ROM-level
bootloaders is enforced to mitigate physical memory injection or unauthorized firmware modification.

* **Hardware Mechanism:** 1. *Physical Debug Locks:* The low-level JTAG and SWD interface pins on
  production mainboard PCBs are unpopulated, disconnected, or completely isolated inside the sealed
  physical enclosure shell. Hardware debugging or direct runtime memory monitoring via these
  interfaces is physically blocked.
  2. *ROM Bootloader Access:* The hardware-level ROM bootloader communication interface can be
     activated programmatically over the Type-C USB interface using development utilities (e.g.,
     `esptool.py`), which toggle the USB-to-UART bridge control lines (DTR/RTS) to reset the chip
     and force entry into the bootloader state.
* **Operating System Support:** The production firmware binary strips out all active runtime
  debugging instrumentation, interactive shell terminals, or configuration-parsing commands. Because
  the hardware ROM bootloader is completely isolated from the network stack and operating framework,
  it is structurally impossible to invoke this flasher state or manipulate partition memory maps
  remotely over network connections; access is entirely gated by localized, physical connection to
  the USB-C port.

---

## Summary Matrix for the Technical File

| Access Control ID                               | Hardware Mechanism                                  | Target Memory Space Protected                                        | Operating System / Framework Role                                                                        |
|:------------------------------------------------|:----------------------------------------------------|:---------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------|
| **AccCtrl-Hardware-Flash-Partition-Isolation**  | Bus-Level MMU Mappings & IBUS/DBUS Division         | Active Application Code (`ota_0`/`ota_1`) vs. Non-Executable Storage | ESP-IDF partition driver mapping rules and bus-level execution blocks.                                   |
| **AccCtrl-CoProcessor-Bus-Separation**          | Dual-MCU Circuit Isolation (UART Interface Channel) | nRF52811 Silicon Registers, Internal Flash, and RAM Space            | Standalone nRF5 SDK stack encapsulation with zero shared DMA or RAM regions.                             |
| **AccCtrl-Hardware-Debug-Interface-Protection** | ROM Bootloader USB Lockout & Isolated JTAG          | Raw Processor Flash Blocks and Live Runtime Memory Contexts          | Strips debug commands; network boundaries completely isolate the USB flasher state from remote exploits. |
