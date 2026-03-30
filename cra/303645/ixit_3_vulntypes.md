# IXIT 3-VulnTypes: Relevant Vulnerabilities

This document categorizes the types of vulnerabilities relevant to the Ruuvi Gateway (DUT) and
defines the mandatory procedures and maximum timeframes for their resolution.

---

## ID: RuuviGw-Vuln1: Web-UI & Node.js

### Description
Vulnerabilities in the local configuration interface, including Node.js dependencies (e.g., jQuery,
elliptic, crypto-js) and Web-UI assets.

### Action
The Security Incident Team (SIT) acknowledges the report.
The Software Development Department (SDD) audits the dependency tree.
Affected components are updated or patched.
New firmware is built and validated.

### Time Frame
90 Days (Acknowledgment within 7 days)

---

## ID: RuuviGw-Vuln2: ESP32 Core FW

### Description
Vulnerabilities in the ESP-IDF framework, RTOS kernel, or system-level drivers managing memory and
tasks.

### Action
Notifications are investigated by the SIT. If the vulnerability affects an upstream ESP-IDF 
component, the SIT will contact Espressif Systems to report the issue or obtain a patch. 
The SDD will backport the upstream fix or implement a custom workaround in the firmware. 
After verification, an updated firmware OTA release is published alongside a security advisory.

### Time Frame
90 Days (Acknowledgment within 7 days)

---

## ID: RuuviGw-Vuln3: nRF52 BLE Stack

### Description
Vulnerabilities in the nRF52 co-processor firmware or the underlying nRF5 SDK (v15.3.0).

### Action

SIT assesses BLE protocol risks. 
SDD updates the co-processor binary.
The new binary is bundled into the main ESP32 OTA payload for automatic flashing.

### Time Frame
90 Days (Acknowledgment within 7 days)

---

## ID: RuuviGw-Vuln4: Network (Wi-Fi/IP)

### Description
Vulnerabilities in the 802.11 stack, WPA3/WPA2 supplicants, or the TCP/IP stack (LwIP) in AP or STA
modes.

### Action
SIT collaborates with Espressif Systems.
SDD integrates updated Wi-Fi/Network libraries.
Regressions in connectivity are tested prior to release.

### Time Frame
90 Days (Acknowledgment within 7 days)

---

## ID: RuuviGw-Vuln5: Cryptography

### Description
Vulnerabilities in `mbedtls`, `esp-tls`, or custom Ruuvi patches for HTTPS/MQTTS secure tunnels.

### Action
**High Priority**.
SIT immediately assesses data exposure risk.
SDD backports security patches. 
Expedited CI/CD pipeline for hotfix release.

### Time Frame
90 Days (Acknowledgment within 7 days)

---

## ID: RuuviGw-Vuln6: Hardware & Physical

### Description
Vulnerabilities in the ESP32/nRF52 silicon or physical PCB layout allowing side-channel or JTAG/SWD
attacks.

### Action
SIT evaluates physical access risk. 
SDD implements software-side mitigations (e.g., permanent eFuse blowing for debug lockout).

### Time Frame
90 Days (Or Advisory within 30 days)

----------------------------------------------------------------------------------------------------

## Detailed Action Descriptions

1. Receipt and Acknowledgment
  All vulnerability reports received via the official Vulnerability Disclosure Policy (VDP) channel (
  e.g., security@ruuvi.com) are logged. The Security Incident Team (SIT) provides an initial
  response to the reporter within the timeframe specified in the table above to confirm receipt and
  request further technical details if necessary.
2. Investigation and Confirmation
  The SIT investigates the report to determine:
  - **Applicability**: Does the flaw affect the current firmware version of the Ruuvi Gateway?
  - **Severity**: Calculated using CVSS v3.1 or higher.
  - **Scope**: Does it affect a custom Ruuvi component or a third-party framework (Espressif/Nordic)?
3. Remediation (The "Action")
   Once confirmed, the **Software Development Department (SDD)** is tasked with remediation:
   - **Custom Code**: Direct code fix and unit testing.
   - **Third-Party Libraries**: Bumping versions (e.g., updating a Node.js package or mbedtls 
     version).
   - **Vendor Frameworks**: Coordinating with Espressif or Nordic. If a vendor patch is delayed, SDD
     implements a temporary "sanitization" layer to mitigate the risk until an official patch is
     available.
4. Release and Disclosure
   Final fixes are deployed via an Over-the-Air (OTA) update. Ruuvi publishes a security advisory on
   the official website and includes details in the firmware changelog. Users are notified of the
   update's availability through the Ruuvi Gateway's status dashboard.
