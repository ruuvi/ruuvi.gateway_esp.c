# Test group 5.7-2: Response and Communication Restriction on Detected Unauthorized Software Changes

Provision 5.7-2 — Status: **R F (s)**. Related IXIT: `IXIT 20-SecBoot`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition 19 (s) Requirement**: *"The device can alert an entity (e.g. user, administrator) upon
  detection of an unauthorized change in device software."*
* **DUT Capabilities Assessment**: As declared in `IXIT 20-SecBoot`, upon detecting an unauthorized
  software change or cryptographic signature verification failure, the DUT alerts the local
  user/administrator via deterministic local state changes (persistent infinite reboot loop, total
  absence of the setup hotspot SSID `Configure Ruuvi Gateway XXXX`, and complete loss of Web-UI
  reachability).
* **Condition Result**: Condition 19 evaluates to **TRUE**. Provision 5.7-2 is evaluated as *
  *Recommendation (R)**.

---

## Test case 5.7-2-1 (conceptual)

**Purpose**: To conceptually assess whether the method of user/administrator notification upon
detecting unauthorized software changes is sufficient (`a`), and whether all notification
functionalities in `IXIT 20-SecBoot` strictly restrict communication capabilities to those necessary
for the alert state (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of User Notification

* **Requirement**: Assess whether the method described in "User Notification" in `IXIT 20-SecBoot`
  is sufficient to inform the user or administrator of an unauthorized software modification.
* **Evaluation**:
  * Upon detecting signature failure across available slots, the DUT halts standard boot, prevents
    network stack initialization, and executes `gateway_restart()`, forcing a continuous reboot
    loop.
  * The operator is unambiguously alerted to the system failure through physical observation:
    continuous power-cycling behavior, complete disappearance of the Wi-Fi configuration SSID (
    `Configure Ruuvi Gateway XXXX`), and complete unreachability of the local Web-UI interface over
    Ethernet/Wi-Fi.
  * This physical and operational failure state effectively alerts the administrator to a critical
    software integrity fault requiring maintenance recovery or hardware reflashing.
* **Unit A Verdict**: **PASS**

#### Test Unit B: Necessity of Notification Functionality & Communication Restriction

* **Requirement**: Assess whether the "Notification Functionality" in `IXIT 20-SecBoot` restricts
  network communication strictly to necessary capabilities upon detecting unauthorized software
  changes.
* **Evaluation**:

| Secure Boot Failure Scenario (`IXIT 20-SecBoot`)        | Declared Notification Functionality & State                                         | Communication Restriction Assessment                                                                                                                                          | Unit Verdict |
|:--------------------------------------------------------|:------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Primary Slot Signature Mismatch** (`ota_0` / `ota_1`) | Triggers automated slot rollback to backup partition.                               | Network interfaces remain offline during signature evaluation; execution switches strictly to the cryptographically verified backup slot.                                     |   **PASS**   |
| **Dual-Slot Signature Failure** (Complete Compromise)   | System lockout state: network initialization halted, infinite hardware reboot loop. | **Total Communication Lockdown.** Network interfaces (Wi-Fi, Ethernet) are blocked prior to TCP/IP stack or driver initialization. Zero network sockets or frames are opened. |   **PASS**   |

* **Conceptual Assessment Justification**: `IXIT 20-SecBoot` defines a fail-secure architecture.
  Upon detecting unauthorized software modifications, the DUT restricts all wider network
  communications to zero (complete network stack suppression), neutralizing remote exploitation
  risks while alerting the local operator via a deterministic physical failure state.

* **Unit B Verdict**: **PASS**

---

## Test case 5.7-2-2 (functional)

**Purpose**: To functionally verify on the DUT that alerting takes place as documented in
`IXIT 20-SecBoot` (`a`), and that network communication to wider networks is restricted strictly to
the documented necessary scope upon detecting unauthorized software changes (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Alerting and Communication Restriction Inspection

**Testing Methodology**: The test laboratory injected invalid signature blocks into both OTA flash
slots (`ota_0` and `ota_1`) via serial flashing tools (`esptool.py`), applied power to the DUT,
observed physical system behavior, and monitored network interfaces using a passive protocol
analyzer (Wireshark).

| Functional Test Scenario                | Target Failure Injection                                       | Observed Physical DUT Behavior & Alerting (Unit a)                                                                                                              | Captured Network Traffic & Communication Scope (Unit b)                                                                                                         | Unit Verdict |
|:----------------------------------------|:---------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Dual-Slot Signature Corruption Test** | Both `ota_0` and `ota_1` loaded with invalid signature blocks. | DUT halts initialization and enters an infinite reboot loop (~3s reset interval). The setup hotspot SSID (`Configure Ruuvi Gateway XXXX`) is completely absent. | **Zero Network Traffic Captured.** Wireshark inspection over Ethernet and Wi-Fi channels confirms zero ARP requests, DHCP discovers, or IP packets are emitted. |   **PASS**   |

**Assessment Justification**: Functional network testing confirms that after detecting unauthorized
software changes, the DUT issues a clear local physical alert (infinite reboot loop + missing AP
SSID) and enforces complete network communication restriction. No unapproved network packets or
background socket connections are initiated.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.7-2-1 & 5.7-2-2

| Test Case          | Purpose / Focus                      | Assessment Summary                                                                                                           | Unit Verdict |
|:-------------------|:-------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.7-2-1 Unit a** | User Notification Assessment         | Local failure state (reboot loop, missing SSID, Web-UI offline) unambiguously alerts operator to software integrity failure. |   **PASS**   |
| **5.7-2-1 Unit b** | Communication Restriction Assessment | Network stack initialization is completely suppressed prior to driver bring-up, enforcing zero network exposure.             |   **PASS**   |
| **5.7-2-2 Unit a** | Functional Alerting Verification     | Injection of corrupted signatures triggers continuous hardware reboot loop and suppresses wireless AP setup.                 |   **PASS**   |
| **5.7-2-2 Unit b** | Functional Communication Monitoring  | Protocol analysis (Wireshark) confirms zero network frames or socket connections are emitted after signature failure.        |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.7-2 of `ETSI EN 303 645`. Upon
detecting unauthorized software modifications or signature verification failures across available
firmware slots (`IXIT 20-SecBoot`), the DUT alerts the operator via deterministic local failure
indicators (infinite reboot loop, loss of setup SSID, and total Web-UI unreachability). All network
interfaces (Ethernet, Wi-Fi) are blocked prior to TCP/IP stack initialization, ensuring zero
outbound communication vectors during the alert state. Functional testing and protocol analysis
confirm that no network traffic is emitted following a software integrity failure.

**Group Verdict**: **PASS**
