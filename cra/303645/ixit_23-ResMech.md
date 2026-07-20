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
manage transport failures safely.

* **Stateful Connections (MQTT/MQTTS/WS/WSS):** For long-lived telemetry streams, the client loops
  actively monitor server-side heartbeats. Upon socket termination, connection dropout, or backend
  unavailability, the ESP-MQTT client clears the connected status and uses automatic reconnect (
  `disable_auto_reconnect = false`) with the default reconnect interval (`MQTT_RECON_DEFAULT_MS`, 10
  seconds) to re-enter the connection and handshake sequence until the stateful stream is restored.
  The gateway publishes an online status after `MQTT_EVENT_CONNECTED`; while disconnected,
  advertisement publishing is gated by `gw_status_is_mqtt_connected()` so the application task does
  not block on a dead backend.
* **Stateless Connections (HTTP/HTTPS):** Because HTTP telemetry relays are designed as stateless
  periodic JSON posts, individual transit failure events do not disrupt system state. The async
  communication loop polls the in-flight request at bounded 50 ms intervals and, on non-2xx status,
  transport error, or unreachable backend, cleans up the HTTP client, releases the HTTP server
  mutex, reports send failure, and relaunches the affected periodic advertisement timer with an
  increased period. The normal HTTP advertisement period starts at
  `ADV_POST_DEFAULT_INTERVAL_SECONDS` (10 seconds) and may be updated by the backend
  `X-Ruuvi-Gateway-Rate` response header, bounded to 1-3600 seconds; for the Ruuvi Cloud target, the
  cloud service sets this header to 60 seconds so successful cloud uploads settle to a 60-second
  polling cadence. On send failure, `adv1_post_timer_relaunch_with_increased_period()` or
  `adv2_post_timer_relaunch_with_increased_period()` changes the polling period to the fixed retry
  delay `ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS` (67 seconds). On the next successful
  post, `adv*_post_timer_relaunch_with_default_period()` restores the configured/default period. The
  failed HTTP batch is not retried indefinitely; it is safely dropped and the next incoming
  advertisement chunk is packaged for the subsequent scheduled post interval, mitigating
  data-clogging vulnerabilities.
* **MQTT Advertisement Delivery Modes:** Periodic MQTT delivery follows the same non-blocking async
  task model and only starts when MQTT is connected. Instant-mode MQTT delivery is event-driven: if
  MQTT is disconnected, advertisements remain in the retransmission list; if the MQTT publish buffer
  is full, sending is postponed via a 50 ms retry timer. This provides graceful recovery from
  backend downtime without a gateway reboot, subject to normal in-memory advertisement table
  capacity.
* **Fleet Reconnection-Storm Control:** The HTTP recovery path does not implement exponential
  backoff. Instead, the Ruuvi Cloud path uses the cloud-provided 60-second `X-Ruuvi-Gateway-Rate`
  cadence during normal operation and switches to the fixed 67-second retry delay after failed
  posts. Therefore, if Ruuvi Cloud recovers without immediately returning a valid
  `X-Ruuvi-Gateway-Rate` header, gateways continue retrying at 67-second intervals rather than
  falling back to the 10-second factory default.
* **Implementation Status:** Implemented and evidenced in
  `main/adv_post_async_comm.c::adv_post_do_async_comm`,
  `main/adv_post_async_comm.c::adv_post_do_async_comm_in_progress`,
  `main/adv_post_timers.c::adv1_post_timer_relaunch_with_default_period`,
  `main/adv_post_timers.c::adv1_post_timer_relaunch_with_increased_period`,
  `main/adv_post_timers.c::adv2_post_timer_relaunch_with_default_period`,
  `main/adv_post_timers.c::adv2_post_timer_relaunch_with_increased_period`,
  `main/http.c::http_async_poll`, `main/mqtt.c::mqtt_generate_client_config`,
  `main/mqtt.c::mqtt_event_handler`, and `main/adv_mqtt_signals.c::adv_mqtt_handle_sig_recv_adv`.

#### Type

Network connectivity

#### Security Guarantees

Protects system **Availability** and architectural stability. Isolates stateful task threads and
prevents heap memory starvation or lockups during prolonged cloud backend outages by discarding
dropped stateless packages. This ensures immediate service alignment upon destination availability.

---

### **ID**: ResMech-Net-Watchdog-Recovery

#### Description

The gateway includes a network watchdog as a last-resort recovery mechanism for rare conditions
where normal protocol retry logic cannot restore outbound communication, such as a deadlocked
network task, leaked memory, or a broken client state that prevents all configured telemetry targets
from being reached.

* **Trigger Condition:** The `adv_post_task` thread spawns a periodic watchdog verification loop at
  boot and initializes the last-success epoch timestamp. This variable tracking metric is
  exclusively refreshed by valid outbound network activity, including successful HTTP advertisement
  packet delivery to Ruuvi Cloud or custom HTTP(S) destinations, HTTP 429 rate-limiting responses (
  which cryptographically verify server target reachability), and successful MQTT advertisement
  publications in instant mode. If no configured outbound telemetry path can refresh the timestamp
  for `RUUVI_NETWORK_WATCHDOG_TIMEOUT_SECONDS` (60 minutes), the watchdog invokes
  `gateway_restart("Network watchdog")`.
* **Check Cadence:** The watchdog evaluation loop is executed every
  `RUUVI_NETWORK_WATCHDOG_PERIOD_SECONDS` (1 second). The reboot sequence is not utilized as a
  primary connection recovery tool; it functions strictly as a fail-safe measure when the underlying
  HTTP retry and MQTT auto-reconnect paths have failed to establish successful transaction cycles
  for an uninterrupted one-hour window.
* **Fleet Behavior:** During a prolonged Ruuvi Cloud service outage, each gateway's one-hour
  watchdog timeout window is anchored cleanly to its own last successful data upload. In standard
  operational states, uploads are naturally paced by the cloud-provided `X-Ruuvi-Gateway-Rate: 60`
  header, meaning the fleet's last-success metrics are spread across a rolling one-minute interval
  rather than synchronized to a single snapshot. If an outage exceeds the one-hour threshold,
  watchdog reboots are distributed smoothly over that same interval. Following an interface restart,
  if the target backend remains offline, the HTTP path enforces the fixed 67-second failure retry
  delay instead of the 10-second factory default.

#### Type

Network connectivity

#### Security Guarantees

Protects system **Availability** by automatically recovering from stuck processing loops or socket
lockups without generating synchronized fleet-wide reconnection events. The combination of
distributed watchdog reboot timing and the post-boot 67-second fixed retry delay ensures the fleet
cannot generate a self-inflicted Distributed Denial of Service (DDoS) packet storm against the cloud
infrastructure during recovery windows.

---

## Summary Matrix for the Technical File

| Resilience ID                                 | Mitigation Objective          | Primary Technical Mechanism                                                       | Target Vulnerability / Threat                                             |
|:----------------------------------------------|:------------------------------|:----------------------------------------------------------------------------------|:--------------------------------------------------------------------------|
| `ResMech-Power-NVS-Wear-Leveling`             | Storage Stability             | Append-Only NVS Transaction Split Logs (`nvs` / `gw_cfg_def`)                     | Flash sector corruption and partial-write states from hard power cuts.    |
| `ResMech-Firmware-Redundancy-And-Rollback`    | Code Integrity / Self-Healing | Dual Partition Layout Arrays with Native ESP-IDF OTA Rollback                     | Bricked systems from failed updates or malicious partition modifications. |
| `ResMech-Net-Link-Layer-Auto-Recovery`        | Link-Layer Persistence        | Automated Wi-Fi Association Retry Loops & Physical Ethernet Cable Hot-Plug Detect | Temporary wireless dropouts or physical cabling disruptions.              |
| `ResMech-Net-Telemetry-Protocol-Reconnection` | Telemetry Stream Resilience   | Stateful Socket Re-Establishment & Stateless Frame-Drop Isolation                 | Remote cloud server outages, session drops, and routing freezes.          |
| `ResMech-Net-Watchdog-Recovery`               | Last-Resort Network Recovery  | One-Hour Last-Success Watchdog with Distributed Reboot Timing                     | Rare deadlocks, leaks, or broken client states preventing all telemetry.  |
