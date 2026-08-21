# Test group 5.6-7: Software Runs with Least Necessary Privilege

Provision 5.6-7 — Status: **R**. Related IXIT: `IXIT 17-PrivlCtrl`.

---

## Test case 5.6-7-1 (conceptual)

**Purpose**: To conceptually assess whether the privilege control mechanisms documented in
`IXIT 17-PrivlCtrl` collectively facilitate the principles of separation of duty, need to know, and
minimization of privilege (`a`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Privilege Control Principles

* **Requirement**: Assess whether the mechanisms described in `IXIT 17-PrivlCtrl` effectively
  enforce separation of duty, need to know, and minimization of privilege across hardware, software
  runtime, and network interface boundaries.

| Privilege Control ID (`IXIT 17-PrivlCtrl`)       | System Boundary             | Primary Technical Enforcement Mechanism                                                     | Security Principle Facilitated & Audit Assessment                                                                                                                                                                     | Unit Verdict |
|:-------------------------------------------------|:----------------------------|:--------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PrivlCtrl-Hardware-Asymmetric-Architecture`** | Dual-MCU Hardware Tier      | Physical separation between nRF52811 (BLE radio) and ESP32 (WAN/Network) via isolated UART. | **Separation of Duty & Minimization of Privilege.** Isolate passive radio scanning from WAN network routing. The nRF52 operates in an unprivileged, connectionless Rx-Only state unable to write to main flash buses. |   **PASS**   |
| **`PrivlCtrl-FreeRTOS-Task-Prioritization`**     | Embedded Runtime Logic      | FreeRTOS scheduler task prioritization and isolated memory allocations.                     | **Minimization of Privilege.** Isolates low-level network drivers from high-priority data routing and NVS configuration tasks, preventing driver overflows from compromising security states.                         |   **PASS**   |
| **`PrivlCtrl-Local-Network-Gating`**             | Operating State Machine     | State-dependent endpoint availability and Web-UI view restriction.                          | **Need to Know.** Restricts station network credential updates and medium selection to the transient setup hotspot mode, omitting configuration views during standard LAN operation.                                  |   **PASS**   |
| **`PrivlCtrl-M2M-Token-Privilege-Separation`**   | Local Programmatic REST API | Role-based Bearer token validation (`lan_auth_api_key` vs. `lan_auth_api_key_rw`).          | **Separation of Duty & Need to Know.** Separates data harvesting (`/history`) from settings mutation (`POST /ruuvi.json`). Read-only tokens receive zero configuration visibility or write access.                    |   **PASS**   |

**Assessment Justification**: The privilege control strategy in `IXIT 17-PrivlCtrl` satisfies all
three core principles:

1. **Separation of Duty** is achieved via dual-MCU hardware isolation (BLE radio vs. WAN main
   application) and role-based M2M Bearer tokens (`RO` vs. `RW`).
2. **Need to Know** is enforced by restricting configuration routes to dedicated setup states and
   limiting read-only API clients strictly to telemetry arrays.
3. **Minimization of Privilege** is maintained via connectionless Rx-Only co-processor firmware and
   deterministic FreeRTOS task scheduling constraints that isolate unprivileged network drivers.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-7-1

| Test Case          | Purpose / Focus                         | Assessment Summary                                                                                                                                                          | Unit Verdict |
|:-------------------|:----------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-7-1 Unit a** | Conceptual Privilege Control Assessment | Hardware dual-MCU separation, FreeRTOS task scheduling, state-based network gating, and M2M token roles facilitate least privilege, need-to-know, and separation of duties. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.6-7 of `ETSI EN 303 645`. The
technical declarations in `IXIT 17-PrivlCtrl` demonstrate effective privilege control mechanisms
operating across hardware, runtime software, and network layers. Hardware separation between the
ESP32 master and nRF52 co-processor, FreeRTOS task isolation, state-dependent network configuration
gating, and role-based M2M Bearer token validation collectively enforce the principles of separation
of duty, need to know, and minimization of privilege.

**Group Verdict**: **PASS**
