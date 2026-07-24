# IXIT 8-UpdProc: Update Procedures

The following table details the lifecycle of a security update at Ruuvi, tracking the process from
development to global deployment.

---

## Table C.8: IXIT 8-UpdProc (Update Procedures)

### **ID**: Ruuvi-UpdProc-Sec: Security Update Lifecycle

#### Description

The management of security updates is a coordinated effort between the **Security Incident Team (
SIT)** and the **Software Development Department (SDD)**.

1. **Initiation:** Once a vulnerability is confirmed via the evaluation process in `IXIT 5-VulnMon`,
   a high-priority tracking ticket is created in Ruuvi's internal repository management ecosystem (
   GitHub Issues).
2. **Development:** The SDD is responsible for developing the patch block. This process involves
   updating pinned Node.js dependencies, backporting stable ESP-IDF framework fixes, or modifying
   custom C/C++ firmware modules for the ESP32 application or the nRF52 radio co-processor.
3. **Verification:** The QA group within the SDD performs regression testing loops, executing
   negative testing parameters (attempting to trigger the target exploit vector) and positive
   validation checks (confirming that core gateway tasks like MQTT/HTTP telemetry streams and
   connectionless BLE scanning match functionality baselines).
4. **Signing:** The final firmware binaries are cryptographically signed using the manufacturer's
   private RSA-3072 key (enforcing ESP32 Secure Boot v2 via RSA-PSS configuration parameters).
   The private key is maintained as an encrypted pipeline secret (`GitHub Secrets`), scoped
   strictly to protected, access-controlled release branches and executed within automated,
   ephemeral build environments.
5. **Deployment:** The SDD uploads the signed binaries and version descriptor files to the official
   update infrastructure. Initial index verification flags resolve to
   `https://network.ruuvi.com/firmwareupdate`, and binary asset fetches execute via
   `https://fwupdate.ruuvi.com`.
6. **Communication:** A formal security advisory and a detailed changelog are published to the
   public Ruuvi Release Index page and product documentation servers to notify operators of the
   release context.

#### Time Frame

The targeted time frame for completing this update procedure matches the response window metrics
defined in the operational policy indices:

* **Standard Security Updates:** Maximum 90 days from initial tracking confirmation to server
  deployment.
* **Critical Vulnerability / Emergency Network Fixes:** Maximum 90 days for expedited engineering,
  verification, and automated update rollout.

#### Procedural Safeguards

* **Ticket Traceability:** Every security patch commit must reference a unique vulnerability
  tracking ID (CVE or internal identifier) to guarantee complete verification transparency during
  the release cycle.
* **Automated CI/CD Pipeline & Secret Scoping:** Ruuvi implements automated build pipelines that
  force static analysis scans and software composition analysis (SCA) dependency runs on every code
  modification. Cryptographic signing keys are restricted to protected environment scopes, ensuring
  build jobs on untrusted developer branches cannot access or extract production signing assets.
* **Staged Rollout Strategy:** Major security updates leverage the transient Beta track subsystem (
  as detailed in `IXIT 7-UpdMech`). New code images are deployed to the optional beta branch (
  `https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/`) to track stability in
  production conditions prior to migrating the image to the global Release track.

#### Summary of Responsibilities

| Entity                                    | Primary Responsibility                                                                                                       |
|:------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|
| **Security Incident Team (SIT)**          | Vulnerability intake management, risk assessment scoring, and coordination with upstream silicon vendors (Espressif/Nordic). |
| **Software Development Department (SDD)** | Patch block engineering, secure RSA firmware signing validation, and server deployment maintenance.                          |
| **QA / Testing Group**                    | Vulnerability exploitation simulation, boundary verification, and regression testing optimization.                           |
