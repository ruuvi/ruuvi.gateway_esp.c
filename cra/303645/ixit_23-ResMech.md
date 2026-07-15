# IXIT 23-ResMech: Resilience Mechanisms

The following declarations detail the hardware and firmware mitigation logic utilized by the Ruuvi
Gateway to ensure data integrity, system stability, and automated recovery during unexpected network
disruptions or abrupt power terminations.

---

## Table C.23: IXIT 23-ResMech (Resilience Mechanisms)

### **ID**: ResMech-Power-NVS-Wear-Leveling

#### Description

The device lacks an internal battery backup, meaning a power drop results in an immediate hard
shutdown. To defend against persistent partition corruption during active write states,
configuration metrics and security parameters are isolated into dedicated Non-Volatile Storage (NVS)
blocks.

* **Storage Allocation:** Basic operational configuration elements (`ruuvi.json`) are written to the
  standard `nvs` partition, while high-volume assets—including user-provisioned SSL certificates,
  keys, and advanced orchestration profiles—are isolated on an independent, expanded flash layout
  partition named `gw_cfg_def`.
* **Mechanism:** Rather than overwriting a single, static memory address when parameters change, the
  underlying ESP-IDF NVS architecture enforces an append-only transaction entry log distributed
  across sequential flash pages. A fresh write cycle writes to an unallocated sector before the
  obsolete entry metadata tag is updated or flagged as dead.

#### Type

Power outage

#### Security Guarantees

Ensures baseline **Data Integrity** and cold-boot reliability. A hard power-cut during an active
setting modification loop cannot result in a corrupted or half-written configuration state; upon
reboot, the system safely rolls back to the last complete, valid parameter block version, preventing
bricking vectors or unbootable loops.

---

### **ID**: ResMech-Firmware-Redundancy-And-Rollback

#### Description

The system provides structural resilience against unbootable states, code corruption, or signature
validation failures by implementing an exhaustive dual-slot application-layer redundant
configuration layout.

* **Component Redundancy Matrix:** Every logical software layer of the platform is completely
  duplicated into a matching primary and secondary partition slot pair:
  * **Main Application Firmware:** Alternates between active and inactive slots via standard ESP-IDF
    OTA partition controls (`ota_0` and `ota_1`).
  * **Web-UI Assets File System:** Implemented via two distinct, read-only raw FAT file system
    partitions (`fatfs_gwui` and `fatfs_gwui_2`).
  * **nRF52 Co-Processor Firmware:** Maintained within two separate, read-only raw FAT file system
    partitions (`fatfs_nrf52` and `fatfs_nrf52_2`).
* **Recovery Mechanism:** During the initialization verification sequence (both post-download and
  post-reboot), if any active software components fail their mandatory cryptographic validation
  checks (`esp_image_verify` or embedded RSA signature block parsing), the system invokes standard
  ESP-IDF OTA framework fallback mechanisms. The verification engine invalidates the bad boot slot
  and automatically flags the alternate partition stack to trigger an immediate, automated rollback
  recovery.

#### Type

Power outage

#### Security Guarantees

Ensures system **Availability** and runtime stability. If a power disruption corrupts a flash write
sequence during an active firmware update, or if a post-reboot validation check fails for the
secondary filesystems, the device self-heals by instantly executing a fallback to the prior
known-good, cryptographically verified operational version.

---

### **ID**: ResMech-Net-Link-Layer-Auto-Recovery

#### Description

The gateway architecture locks its physical interface type (Ethernet or Wi-Fi) based entirely on the
initial provisioning walkthrough choice, executing zero dynamic background medium switching.
Resilience is handled through targeted local link-state auto-recovery monitors.

* **Wi-Fi Connectivity:** If configured for wireless operation and the radio connection to the
  target access point is dropped, the internal network driver executes an automated, infinite retry
  loop to continuously negotiate re-association.
* **Ethernet Connectivity:** If configured for wired operation and the physical link layer drops (
  e.g., cable disconnection), the interface driver stalls. Upon detection of physical pin
  re-engagement (the cable is plugged back in), the system instantly re-initializes the link layer
  and re-requests its DHCP lease configuration parameters without requiring a system reboot.
* *Note on Scope:* Altering the network connection type post-setup requires a manual configuration
  shift initiated through physical activation of the configuration hotspot.

#### Type

Network connectivity

#### Security Guarantees

Protects system **Availability**. Guarantees that temporary localized network hardware disruptions,
router reboots, or physical cable maintenance will not result in a permanent offline lockout state,
allowing the device to independently rejoin the network as soon as the physical medium becomes
functional again.

---

### **ID**: ResMech-Net-Telemetry-Protocol-Reconnection

#### Description

Outbound data client layers incorporate connection handling specific to their protocol schemas to
manage transit failures safely.

* **Stateful Connections (MQTT/MQTTS/WS/WSS):** For long-lived telemetry streams, the client loops
  actively monitor server-side heartbeats. Upon socket termination or connection dropouts, the
  driver flushes the broken interface context and automatically re-initiates connection and
  handshake sequences until the stateful stream is fully restored.
* **Stateless Connections (HTTP/HTTPS):** Because HTTP telemetry relays are designed as stateless
  periodic JSON posts, individual transit failure events do not disrupt system state. The task
  engine drops failed frames safely and proceeds to package the next incoming chunk of sensor
  advertisements for the subsequent scheduled post interval, mitigating data-clogging
  vulnerabilities.

#### Type

Network connectivity

#### Security Guarantees

Protects system **Availability** and architectural stability. Isolates stateful task threads from
locking up during a cloud backend outage, ensuring immediate service re-alignment upon destination
availability.

---

## Summary Matrix for the Technical File

| Resilience ID                                   | Mitigation Objective          | Primary Technical Mechanism                                                       | Target Vulnerability / Threat                                             |
|:------------------------------------------------|:------------------------------|:----------------------------------------------------------------------------------|:--------------------------------------------------------------------------|
| **ResMech-Power-NVS-Wear-Leveling**             | Storage Stability             | Append-Only NVS Transaction Split Logs (`nvs` / `gw_cfg_def`)                     | Flash sector corruption and partial-write states from hard power cuts.    |
| **ResMech-Firmware-Redundancy-And-Rollback**    | Code Integrity / Self-Healing | Dual Partition Layout Arrays with Native ESP-IDF OTA Rollback                     | Bricked systems from failed updates or malicious partition modifications. |
| **ResMech-Net-Link-Layer-Auto-Recovery**        | Link-Layer Persistence        | Automated Wi-Fi Association Retry Loops & Physical Ethernet Cable Hot-Plug Detect | Temporary wireless dropouts or physical cabling disruptions.              |
| **ResMech-Net-Telemetry-Protocol-Reconnection** | Telemetry Stream Resilience   | Stateful Socket Re-Establishment & Stateless Frame-Drop Isolation                 | Remote cloud server outages, session drops, and routing freezes.          |
