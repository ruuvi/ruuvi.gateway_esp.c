# IXIT 8-UpdProc: Update Procedures

The following table details the lifecycle of a security update at Ruuvi, from development to global
deployment.

## Table C.8: IXIT 8-UpdProc (Update Procedures)

### Ruuvi-UpdProc-Sec: Security Update Lifecycle

#### Description

The management of security updates is a coordinated effort between the **Security Incident Team 
(SIT)** and the **Software Development Department (SDD)**.

1. **Initiation**: Once a vulnerability is confirmed via the process in **IXIT 5-VulnMon**, a 
high-priority ticket is created in Ruuvi's internal issue tracking system (GitHub Issues).

2. **Development**: The SDD is responsible for developing the patch. This may involve updating
   Node.js dependencies, backporting ESP-IDF fixes, or modifying custom C/C++ firmware logic for the
   ESP32 or nRF52.

3. **Verification**: The QA team within the SDD performs regression testing, including "negative
   tests" (attempting to exploit the patched vulnerability) and "positive tests" (ensuring existing
   gateway features, like MQTT relaying and BLE scanning, remain functional).

4. **Signing**: The final firmware binary is signed using the manufacturer's private RSA-2048 key 
   in a secure build environment.

5. **Deployment**: The SDD uploads the signed binary and corresponding metadata to the official 
   update server (fwupdate.ruuvi.com).

6. **Communication**: A security advisory and a detailed changelog are published on the Ruuvi 
   Release Page and the product website to inform users of the update's importance.

#### Time Frame

The targeted time frame for the completion of this procedure is aligned with the maximum deadlines
established in **IXIT 3-VulnTypes**:
- **Standard Security Updates**: Maximum 90 days from initial disclosure to deployment.
- **Critical Cryptographic/Network Fixes**: Maximum 90 days for expedited rollout.

#### Procedural Safeguards

- **Ticket Traceability**: Every security-related change must be linked to a specific vulnerability
  ID (CVE or internal ID) to ensure no reported issues are overlooked during the release cycle.

- **Automated CI/CD**: Ruuvi employs automated build pipelines that perform static analysis and
  dependency scanning on every commit, reducing the time required to integrate and verify security
  patches.

- **Staged Rollout**: For major security updates, Ruuvi may utilize the "Beta" channel (as
  defined in **IXIT 7-UpdMech**) to verify the fix in a real-world environment before pushing it to
  the global "Release" channel.

#### Summary of Responsibilities

| Entity                       | Primary Responsibility                                                                            |
|------------------------------|---------------------------------------------------------------------------------------------------|
| Security Incident Team (SIT) | Vulnerability intake, risk assessment, and coordination with upstream vendors (Espressif/Nordic). |
| Software Development (SDD)   | Patch engineering, firmware signing, and maintenance of the update server.                        |
| QA / Testing Team            | Verification of fixes through exploit simulation and regression testing.                          |
