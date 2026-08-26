# Test group 5.3-15A: Isolation Capabilities of the DUT

Provision 5.3-15A — Status: **R**. Related IXIT: `IXIT 9-ReplSup`.

---

## Test case 5.3-15A-1 (conceptual)

**Purpose**: To conceptually assess whether the isolation procedures declared in `IXIT 9-ReplSup`
effectively allow the DUT to be removed from the network (or placed in a self-contained environment)
without functionality loss beyond that network connectivity.

---

### Test Unit A: Suitability Assessment of the Isolation Method

**Testing Methodology**: The test laboratory evaluated the three-step isolation procedure declared
in `IXIT 9-ReplSup` against the conceptual isolation criteria in ETSI TS 103 701 Section 5.3.15A.1.

| Declared Isolation Step (`IXIT 9-ReplSup`)       | Mechanism & Action Executed                                                                    | Isolation Effectiveness Assessment                                                                                                   | Unit Verdict |
|:-------------------------------------------------|:-----------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **1. Physical / Wireless Network Disconnection** | Disconnect RJ-45 Ethernet cable or revoke Wi-Fi AP access / clear Wi-Fi credentials in Web-UI. | **Completely sever backhaul connectivity.** Disables network interface data transmission paths (HTTP/MQTT telemetry outbound drops). |   **PASS**   |
| **2. Physical Power Isolation**                  | Unplug Type-C USB power cable.                                                                 | Completely powers down the device, neutralizing all physical and radio operations.                                                   |   **PASS**   |
| **3. Logical / Broker Removal**                  | Purge Gateway `DEVICEID` / MAC address from remote MQTT broker or cloud dashboard.             | Prevents unauthorized data injection or stale connection handshakes if network access is restored.                                   |   **PASS**   |

**Assessment Justification**:

* The method described in `IXIT 9-ReplSup` completely isolates the DUT from both local area
  networks (LAN) and wide area networks (WAN).
* Severing network connectivity halts network data relaying (`LwIP`, HTTP, MQTT tasks), but does not
  disrupt or damage the core passive BLE radio scanning engine or local NVS configuration
  parameters. Any functionality loss is strictly confined to network connectivity.

**Verdict**: **PASS**

---

## Test case 5.3-15A-2 (functional)

**Purpose**: To functionally test that performing the isolation procedure in `IXIT 9-ReplSup`
successfully removes the DUT from the network without causing unhandled system failures or affecting
non-network core functions.

---

### Test Units Functional Assessment Matrix

| Test Unit / Phase                         | Action Executed                                                                                      | Observed Functional DUT Behavior                                                                                                                                                                                                                                                                                               | Unit Verdict |
|:------------------------------------------|:-----------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Setup**                         | Place DUT in operational environment with active Ethernet/Wi-Fi connection and passive BLE scanning. | DUT successfully scans BLE advertisements and streams MQTT telemetry to cloud endpoints.                                                                                                                                                                                                                                       |   **PASS**   |
| **Unit b: Isolation Execution**           | Disconnect Ethernet cable and revoke Wi-Fi credentials per `IXIT 9-ReplSup`.                         | All network sockets (HTTP/MQTT) close gracefully. Network interface state transitions to disconnected.                                                                                                                                                                                                                         |   **PASS**   |
| **Unit c: Functionality Loss Assessment** | Audit local hardware status, console logs, and BLE scanning engine post-isolation.                   | **Connectivity Loss Isolated.** Outbound network transmission halts as expected. The low-level passive BLE scanning task (`SoftComp-nRF52FW`) and core OS scheduler continue running cleanly without crashes, memory leaks, or unhandled exceptions. Re-connecting network interfaces restores telemetry streaming seamlessly. |   **PASS**   |

**Assessment Justification**: Functional testing verifies that isolating the Ruuvi Gateway by
disconnecting network media disables all network communication without causing instability or
corrupting non-network platform functions.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.3-15A of `ETSI EN 303 645`. The
isolation procedures declared in `IXIT 9-ReplSup` (physical cable removal, Wi-Fi credential
revocation, and logical broker purges) effectively isolate the DUT from connected networks.
Functional testing confirms that isolating the device halts outbound network communication while
preserving system stability and local passive scanning capabilities without unhandled side effects.

**Group Verdict**: **PASS**
