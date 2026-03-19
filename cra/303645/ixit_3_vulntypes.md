# CRA: 303 645 IXIT-2-VULNTYPES: Relevant Vulnerabilities

The completed IXIT lists all types of vulnerabilities that are relevant for the DUT.
- **ID**: Unique per IXIT identifier that may be assigned using a sequential numbering scheme
  or some other labelling scheme.
- **Description**: Brief description of the kind of vulnerability that is relevant for the DUT.
- **Action**: Description of the way of acting on this kind of vulnerability in case of a
  vulnerability disclosure including all entities and responsibilities.
- **Time Frame**: Targeted time frame in which the given steps of the action in case of a
  vulnerability are scheduled.

---

## ID: RuuviGwVulnType-WebUI

### Description
Vulnerabilities in the Gateway Web UI used for configuration and polling, including 
the HTTP server integration, user interface assets, and the underlying Node.js runtime environment.

### Action
When a notification about a potential vulnerability is received according to the Vulnerability 
Disclosure Policy, it is forwarded to the Security Incident Team (SIT). 
If confirmed, the SIT proposes a fix to the Software Development Department (SDD). 
The SDD applies the necessary patches to the Web UI source code or updates the affected Node.js 
components. Once tested, a new firmware image containing the updated Web UI is rolled out, 
and a changelog is published.

### Time Frame
- 7 days for initial response.
- 30 days for SIT to investigate and propose a fix.
- 30 days for SDD to integrate the fix.
- By no later than 90 days after receiving the vulnerability, the fix will be released.

---

## ID: RuuviGwVulnType-FWESP32

### Description
Vulnerabilities in the system firmware running on the ESP32, specifically related to 
the ESP-IDF system components, RTOS task management, and core system drivers (excluding specific 
Wi-Fi and Crypto subsystems).

### Action
Notifications are investigated by the SIT. If the vulnerability affects an upstream ESP-IDF 
component, the SIT will contact Espressif Systems to report the issue or obtain a patch. 
The SDD will backport the upstream fix or implement a custom workaround in the firmware. 
After verification, an updated firmware OTA release is published alongside a security advisory.

### Time Frame
- 7 days for initial response.
- By no later than 90 days after receiving the vulnerability, a fix will be released (dependent on 
- upstream vendor response times, but mitigated within the deadline where possible).

---

## ID: RuuviGwVulnType-FWNRF

### Description
Vulnerabilities in the nRF52 co-processor firmware, which is responsible for Bluetooth 
Low Energy (BLE) communication, based on the nRF5_SDK (v15.3.0).

### Action
The SIT evaluates the report regarding BLE parsing, filtering, or the nRF5_SDK stack. 
If the flaw is within Ruuvi's custom logic, the SDD fixes it directly. If it is an SDK flaw, 
the SIT contacts Nordic Semiconductor for guidance or patches. The SDD will bundle the updated 
nRF52 firmware into the main ESP32 firmware payload so the ESP32 can flash the co-processor 
during the next system update.

### Time Frame
- 7 days for initial response.
- By no later than 90 days after receiving the vulnerability, a fix will be released.

---

## ID: RuuviGwVulnType-WIFI

### Description
Vulnerabilities related to the Wi-Fi subsystem, including the 802.11 MAC layer, WPS implementation, 
WPA/WPA2 supplicant, and AP/STA mode handling provided by the ESP32 Wi-Fi stack.

### Action
Since Wi-Fi drivers are heavily tied to the closed-source / specific ESP-IDF implementations, 
the SIT will verify the vulnerability and immediately collaborate with Espressif Systems. 
The SDD will integrate the patched Wi-Fi libraries provided by the vendor, test for regressions 
in network stability, and deploy the fix via a firmware update.

### Time Frame
- 7 days for initial response.
- Up to 90 days for coordination with the vendor and releasing the firmware update.

---

## ID: RuuviGwVulnType-Crypt

### Description
Vulnerabilities related to cryptographic operations and secure communication channels, including 
the patched versions of `mbedtls`, `esp-tls`, `tcp_transport`, `esp_http_client`, and `mqtt` used 
for HTTPS and MQTTS cloud telemetry relays.

### Action
Given the critical nature of cryptography, the SIT immediately assesses the severity. 
If the vulnerability is in the upstream `mbedtls` or ESP-IDF network clients, upstream patches are 
retrieved. If the flaw resides in the custom patches applied by Ruuvi, the SDD corrects the logic. 
A hotfix firmware release is prioritized for critical cryptographic bypasses or severe 
data-in-transit exposures.

### Time Frame
- 7 days for initial response.
- 14 days for SIT/SDD to backport crypto patches.
- By no later than 60 days (expedited for critical CVEs) after receiving the vulnerability, 
  a fix will be released.

---

## ID: RuuviGwVulnType-3rdParty

### Description
Vulnerabilities concerning commercially licensed or open-source third-party libraries not covered 
above.

### Action
The SIT confirms the vulnerability and tracks the upstream repository of the affected library. 
If an updated, secure version of the library is available, the SDD bumps the dependency version. 
If no fix is available upstream, the SDD will attempt to patch the library locally, 
remove the vulnerable feature, or switch to a secure alternative.

### Time Frame
- 7 days for initial response.
- By no later than 90 days after receiving the vulnerability, a fix will be released, 
  or an advisory will be published if deprecating a feature is required.

---

## ID: RuuviGwVulnType-HW

### Description
Vulnerabilities concerning the physical hardware, including the ESP32 MCU, nRF52 co-processor, 
mainboard layout, or physical interfaces (e.g., SWD/JTAG debug ports).

### Action
The SIT evaluates the hardware vulnerability. If the vulnerability allows physical tampering or 
side-channel attacks, the SIT works with the SDD to determine if software mitigations (e.g., 
disabling debug interfaces, adding software rate-limiting, or enforcing flash encryption) 
can resolve or reduce the risk. If the vulnerability is unpatchable via software, a warning is 
published on the manufacturer's security advisory page. 
Hardware revisions may be planned for future manufacturing batches.

### Time Frame
- 7 days for initial response.
- Usually, 90 days after receiving the vulnerability, a software mitigation is released, 
  or a public warning advisory is published.
 