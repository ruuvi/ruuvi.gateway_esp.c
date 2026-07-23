# IXIT 17-PrivlCtrl: Privilege Control

The following declarations detail the structural boundaries, application-layer execution scopes, and
token validation mechanisms utilized by the Ruuvi Gateway to restrict software components to the
minimum privileges required for functional operational states.

## Table C.17: IXIT 17-PrivlCtrl (Privilege Control)

### **ID**: PrivlCtrl-Hardware-Asymmetric-Architecture

#### Description

Privilege control is inherently enforced by the physical separation of core system duties between
two distinct microcontrollers, establishing a hardware-level execution boundary.

* **Co-Processor Isolation:** The nRF52811 co-processor is assigned the sole responsibility of
  managing the low-level Bluetooth Low Energy (BLE) radio scan engine. It operates in a strict,
  unprivileged receive-only (Rx-Only) state, unable to make inbound network routing mutations or
  write directly to the main system's flash partition buses (IBUS/DBUS).
* **Main Application Separation:** The main ESP32 MCU handles the high-level network connectivity,
  TLS handshakes, and configuration updates. It interacts with the co-processor exclusively over an
  isolated local serial bus (UART), ensuring a vulnerability in one chip layout cannot
  cross-propagate arbitrary control execution vectors to the alternate memory matrix.

---

### **ID**: PrivlCtrl-FreeRTOS-Task-Prioritization

#### Description

The main controller application code executes within a deterministic, non-root embedded runtime
environment managed by FreeRTOS scheduler loops inside the ESP-IDF framework. Instead of a highly
privileged, single-threaded execution state, tasks are restricted using the principle of least
privilege via scheduling constraints.

* **Separation of Duties:** Background driver tasks — such as the unprivileged Wi-Fi or Ethernet
  network interface layers — are isolated from high-priority data routing tasks and configuration
  mutation tasks.
* **Resource Access Boundaries:** Memory allocations and execution priorities are assigned per task
  handler loop, ensuring that a deadlock or buffer failure in a low-priority metrics extraction task
  cannot stall network transport or allow an actor to alter security-sensitive configuration blocks.
* *ROM Bootloader Separation:* The hardware ROM bootloader operates entirely outside of the FreeRTOS
  environment. It has no software hooks or task privileges within the operational firmware image; it
  can only be activated externally via direct serial interface control lines.

---

### **ID**: PrivlCtrl-Local-Network-Gating

#### Description

Privilege control at the local network boundary is restricted based on the device's current runtime
operating state machine, preventing exposure of core configuration parameters outside the dedicated
setup window.

* **Hotspot Mode Restriction:** The capability to alter underlying hardware medium types (switching
  between Wi-Fi or Ethernet tracking modes) or update the target Wi-Fi station credentials (
  SSID/Password) is strictly restricted to the local Captive Portal configuration hotspot state.
* **LAN Mode Isolation:** Once the gateway transitions out of provisioning and joins the standard
  station network, the network configuration interface options are omitted entirely from the
  standard Web-UI application view. This enforces an explicit state-based privilege boundary that
  blocks unauthorized network parameter modifications over the local area network (LAN/WLAN).

---

### **ID**: PrivlCtrl-M2M-Token-Privilege-Separation

#### Description

For automated machine-to-machine (M2M) interaction over the local network interface (HTTP Port 80),
the firmware enforces role-based privilege control to isolate telemetry collection from device
orchestration tasks.

* **Read-Only Privilege (`AuthMech-M2M-API-Bearer-RO`):** Restricts the presenting machine account
  to data harvesting only. Presenting a valid read-only token (`lan_auth_api_key`) strictly limits
  execution to the `/history` endpoint, preventing the client from accessing or altering
  configuration records.
* **Read/Write Privilege (`AuthMech-M2M-API-Bearer-RW`):** Grants full administrative authority.
  Only a client presenting the distinct write-access token (`lan_auth_api_key_rw`) is permitted to
  submit schema modifications (via `POST /ruuvi.json`) to mutate gateway settings parameters. If an
  API endpoint's token field is left unconfigured, its corresponding execution privilege is
  completely revoked at the application layer.

---

## Summary Matrix for the Technical File

| Privilege Control ID                           | Target Boundary          | Primary Enforcement Mechanism                    | Operation Restricted                                                                                          |
|:-----------------------------------------------|:-------------------------|:-------------------------------------------------|:--------------------------------------------------------------------------------------------------------------|
| **PrivlCtrl-Hardware-Asymmetric-Architecture** | MCU Inter-Communication  | Asymmetric dual-chip layout (ESP32 + nRF52 UART) | Isolates radio processing loops from network state machine modifications and flash bus mutations.             |
| **PrivlCtrl-FreeRTOS-Task-Prioritization**     | Internal Runtime Logic   | FreeRTOS Scheduler / Task Priority Gating        | Prevents unprivileged driver tasks from altering system configuration memory or capturing ROM flasher states. |
| **PrivlCtrl-Local-Network-Gating**             | Local Provisioning State | State Machine / Hotspot View Omission            | Restricts network medium selection and Wi-Fi credential updates to the Captive Portal setup mode.             |
| **PrivlCtrl-M2M-Token-Privilege-Separation**   | Local Programmatic API   | Role-Based Token Validation (`RO` vs `RW` Keys)  | Isolates data query tasks from administrative configuration payload manipulation.                             |
