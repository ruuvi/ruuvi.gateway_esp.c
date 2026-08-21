# Test group 5.12-2: User Guidance on Securely Setting Up the Device

Provision 5.12-2 — Status: **R**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 26-UserDec`.

---

## Test case 5.12-2-1 (functional)

**Purpose**: To functionally assess whether the DUT can be set up using the "Documentation of Secure
Setup" in `IXIT 2-UserInfo` (`a`), whether every security-relevant user decision in
`IXIT 26-UserDec` is covered by the documentation (`b`), and whether explicit recommendations are
provided on how to configure parameters to achieve a secure setup (`c`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A, B & C: Functional Setup, Documentation Coverage, and Secure Recommendation Audit

**Testing Methodology**: The test laboratory initialized the DUT following the onboarding wizard
documentation referenced under "Documentation of Secure Setup" in `IXIT 2-UserInfo` (
`https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages`), verifying that every decision
in `IXIT 26-UserDec` is covered (`b`) and contains clear secure setup recommendations (`c`).

| User Decision ID (`IXIT 26-UserDec`)       | Target Setup Parameter                | Documentation Coverage in `IXIT 2-UserInfo` (Unit b) | Secure Setup Recommendation Provided in Documentation (Unit c)                                                                          | Unit Verdict |
|:-------------------------------------------|:--------------------------------------|:-----------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`UserDec-1-Network-Medium-Selection`**   | Interface Medium (Ethernet vs. Wi-Fi) | **Covered.** Documented in Step 1 setup guide.       | Recommends Ethernet connectivity where available for higher link stability and reduced RF exposure.                                     |   **PASS**   |
| **`UserDec-2-Interface-Configuration`**    | IP Addressing (DHCP vs. Static IP)    | **Covered.** Documented in Step 2 setup guide.       | Recommends standard DHCP for dynamic network isolation or static IP with verified gateway DNS parameters.                               |   **PASS**   |
| **`UserDec-3-Onboarding-Firmware-Update`** | Initial Software Patching             | **Covered.** Documented in Step 3 setup guide.       | **Strongly Recommends.** Advises installing available firmware updates before completing deployment to eliminate known vulnerabilities. |   **PASS**   |
| **`UserDec-4-Automatic-Config-Download`**  | Centralized Remote Provisioning       | **Covered.** Documented in Step 4 setup guide.       | Recommends keeping feature **Disabled** unless deploying within an authenticated corporate orchestration infrastructure.                |   **PASS**   |
| **`UserDec-5-Automatic-Updates`**          | Maintenance Update Schedules          | **Covered.** Documented in Step 5 setup guide.       | **Recommends `Auto update`** on the standard production branch to ensure timely delivery of security maintenance patches.               |   **PASS**   |
| **`UserDec-6-Remote-Access-Settings`**     | Web-UI Credentials & M2M API Keys     | **Covered.** Documented in Step 6 setup guide.       | **Explicit Security Guidance.** Recommends setting a custom, strong administrator password and disabling unneeded M2M API keys.         |   **PASS**   |
| **`UserDec-7-Cloud-Options`**              | Telemetry Channel Selection           | **Covered.** Documented in Step 7 setup guide.       | **Recommends `Ruuvi Cloud (recommended)`**, enforcing mandatory transport-layer encryption (`HTTPS`).                                   |   **PASS**   |
| **`UserDec-8-Custom-Server-Routing`**      | Custom HTTP/MQTT Destinations         | **Covered.** Documented in Step 8 setup guide.       | Recommends enforcing encrypted transport schemes (`HTTPS`, `MQTTS`, `WSS`) and keeping diagnostic statistics enabled.                   |   **PASS**   |
| **`UserDec-9-Time-Sync-Options`**          | NTP Server Preferences                | **Covered.** Documented in Step 9 setup guide.       | Recommends using standard cloud NTP pools (`time.google.com`, `cloudflare.com`) to ensure valid TLS handshake clock checks.             |   **PASS**   |
| **`UserDec-10-Bluetooth-Scanning`**        | BLE Radio Filter Rules & PHY          | **Covered.** Documented in Step 10 setup guide.      | **Recommends `Listen to Ruuvi sensors only`** or applying MAC Whitelists to minimize ambient radio tracking and data noise.             |   **PASS**   |

* **Functional Assessment Justification**:
  1. **DUT Setup Execution (Unit a):** The test laboratory successfully deployed and configured the
     DUT by following the online onboarding documentation (`docs.ruuvi.com`).
  2. **Documentation Coverage (Unit b):** All 10 user decisions cataloged in `IXIT 26-UserDec` are
     fully covered across the setup wizard documentation tracks.
  3. **Secure Recommendations (Unit c):** Explicit recommendations are issued for every
     security-relevant decision—including changing default passwords, enabling automatic security
     updates, enforcing encrypted TLS/HTTPS endpoints, and restricting BLE scanning bounds.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.12-2-1

| Test Case           | Purpose / Focus              | Assessment Summary                                                                                        | Unit Verdict |
|:--------------------|:-----------------------------|:----------------------------------------------------------------------------------------------------------|:------------:|
| **5.12-2-1 Unit a** | Functional Setup Execution   | The DUT was successfully set up using the "Documentation of Secure Setup" in `IXIT 2-UserInfo`.           |   **PASS**   |
| **5.12-2-1 Unit b** | User Decision Coverage Check | `IXIT 2-UserInfo` documentation covers 100% of the security-relevant user decisions in `IXIT 26-UserDec`. |   **PASS**   |
| **5.12-2-1 Unit c** | Secure Recommendation Audit  | Clear recommendations are provided for every decision to guide the user toward a secure setup state.      |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.12-2 of `ETSI EN 303 645`. The
published user documentation (`IXIT 2-UserInfo` under "Documentation of Secure Setup") provides
comprehensive, step-by-step guidance on securely initializing and configuring the device. Every
security-relevant user decision cataloged in `IXIT 26-UserDec` is covered with explicit
recommendations—guiding users to define strong custom Web-UI passwords, enable automatic firmware
patch delivery, enforce encrypted transport channels (`HTTPS`/`MQTTS`), restrict BLE radio scanning
filters, and disable unused auto-configuration endpoints.

**Group Verdict**: **PASS**
