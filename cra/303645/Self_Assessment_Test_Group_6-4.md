# Test group 6-4: Necessity of Processing Personal Data in Telemetry Data Streams

Provision 6-4 — Status: **M**. Related IXIT: `IXIT 21-PersData`, `IXIT 24-TelData`.

---

## Test case 6-4-1 (conceptual)

**Purpose**: To conceptually assess whether the personal data categories in `IXIT 21-PersData`
referenced under "Personal Data" in `IXIT 24-TelData` are necessary for the intended functionality
and achieving the defined processing purposes of the telemetry data streams (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Conceptual Assessment of Personal Data Processing Necessity

| Telemetry Schema ID (`IXIT 24-TelData`)     | Mapped Personal Data Categories (`IXIT 21-PersData`)                  | Intended Telemetry Functionality & Purpose (`IXIT 24-TelData`)                                                                                                                                                          | Necessity Audit Assessment against Functionality & Purpose                                                                                                                                                                                                                                                                                | Unit Verdict |
|:--------------------------------------------|:----------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`TelData-System-Status-Heartbeat`**       | `PersData-Network-IP-Footprints`<br>`PersData-Gateway-MAC-Identifier` | Transmits FreeRTOS task stack metrics, internal memory allocations, and connection loss counts to monitor device reliability baselines, debug runtime memory leaks, and detect Denial-of-Service (DoS) buffer flooding. | **Strictly Necessary.**<br>1. `PersData-Network-IP-Footprints` is technically required for socket packet encapsulation to reach `https://network.ruuvi.com/status`.<br>2. `PersData-Gateway-MAC-Identifier` (`gw_mac`) is strictly required to attribute system diagnostic health metrics to a specific physical unit in the cloud fleet. |   **PASS**   |
| **`TelData-Crash-Panic-Diagnostics`**       | `PersData-Network-IP-Footprints`<br>`PersData-Gateway-MAC-Identifier` | Transmits CPU registers, execution backtrace arrays, panic strings (`RESET_INFO`), and ELF hashes post-reboot to diagnose firmware bugs, task deadlocks, and memory corruption/buffer overflow exploits.                | **Strictly Necessary.**<br>1. `PersData-Network-IP-Footprints` is required for TCP/IP network transport.<br>2. `PersData-Gateway-MAC-Identifier` is necessary for engineering teams to correlate post-reboot crash reports with hardware revisions and deployed firmware builds.                                                          |   **PASS**   |
| **`TelData-Environmental-Sensor-Payloads`** | `PersData-Network-IP-Footprints`<br>`PersData-Gateway-MAC-Identifier` | Ships accumulated environmental BLE beacon advertisement metrics (temperature, humidity, air pressure, motion) to remote cloud backends or custom user ingestion endpoints.                                             | **Strictly Necessary.**<br>1. `PersData-Network-IP-Footprints` provides transport layer routing.<br>2. `PersData-Gateway-MAC-Identifier` is necessary as an origin header to map incoming environmental metrics to the user's registered gateway profile.                                                                                 |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Strict Data Minimization:** Every personal data element processed within telemetry streams is
     strictly limited to standard network-layer routing metadata (`PersData-Network-IP-Footprints`)
     and physical origin identifiers (`PersData-Gateway-MAC-Identifier`).
  2. **Functional Necessity:** IP addresses are required by standard TCP/IP network protocols for
     packet transport. The gateway MAC address (`gw_mac`) is necessary to perform essential device
     attribution, enabling cloud platforms to assign environmental metrics to correct user
     dashboards and allowing engineering teams to correlate diagnostic health/crash logs with
     specific hardware instances. No extraneous personal data (such as user credentials, location
     GPS coordinates, or unneeded hardware identifiers) is included in telemetry envelopes.

* **Unit A Verdict**: **PASS**

---

## Summary Matrix for Test Case 6-4-1

| Test Case        | Purpose / Focus                         | Assessment Summary                                                                                                                                                                                       | Unit Verdict |
|:-----------------|:----------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **6-4-1 Unit a** | Necessity of Personal Data in Telemetry | All personal data elements referenced in `IXIT 24-TelData` (`PersData-Network-IP-Footprints` and `PersData-Gateway-MAC-Identifier`) are strictly necessary for network transport and device attribution. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 6-4 of `ETSI EN 303 645`. Personal data
processed within telemetry data streams (`IXIT 24-TelData`) is strictly minimized and necessary for
achieving the defined processing purposes. Network IP footprints (`PersData-Network-IP-Footprints`)
are required for TCP/IP packet transport and TLS socket establishment, while the Bluetooth radio MAC
address (`PersData-Gateway-MAC-Identifier`) is necessary to map incoming environmental metrics to
user accounts and attribute diagnostic health/crash logs to specific fleet hardware. Telemetry
payloads contain zero unnecessary or excessive personal data.

**Group Verdict**: **PASS**
