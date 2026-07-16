# IXIT 4-Conf: Confirmations

The following declarations provide formal confirmations for the establishment of operational
security processes for the Ruuvi Gateway (DUT). Each entry verifies that the required infrastructure
is in place and the responsible operators are actively briefed.

---

## Table C.4: IXIT 4-Conf (Confirmations)

### **Confirmation of Vulnerability Actions**: Yes

For every vulnerability remediation action detailed under `IXIT 3-VulnTypes`, the required corporate
infrastructure is fully deployed and operational staff are briefed to meet the target time frames.
Reports are ingested via the published Vulnerability Disclosure Policy channel, triaged
systematically by the internal Security Incident Team (SIT), and resolved by the Software
Development Department (SDD) utilizing the automated over-the-air (OTA) update pipeline defined
under `IXIT 7-UpdMech`.

### **Confirmation of Vulnerability Monitoring**: Yes

For the vulnerability monitoring, identification, and rectification protocols established under
`IXIT 5-VulnMon`, the necessary tracking infrastructure is active and operational operators are
briefed. This loop encompasses continuous monitoring of upstream vendor security
advisories—including Espressif Systems (ESP-IDF), Nordic Semiconductor (nRF5 SDK), and the mbedTLS
framework repository—along with dedicated tracking of all integrated third-party components and
software library dependencies.

### **Confirmation of Update Procedures**: Yes

For every software update procedure declared under `IXIT 8-UpdProc`, the required infrastructure is
active and operators are briefed to satisfy the designated target response time frames.
Cryptographically signed firmware images are compiled within a controlled, isolated build
environment, pushed securely to the centralized release distribution server (
`https://network.ruuvi.com/firmwareupdate`), and delivered to endpoint devices via the network-based
update tracks.

### **Confirmation of Secure Management**: Yes

The secure management processes described under `IXIT 14-SecMgmt` are fully established. This
includes strict lifecycle controls governing the storage of critical parameters within the
`ruuvi.json` manifest file inside the `nvs` flash block, logical isolation of credentials from
outbound query streams, and the execution of low-level structural formatting block-erasure routines
across both the `nvs` and `gw_cfg_def` partitions upon a 7-second hardware reset button hold.

### **Confirmation of Secure Development**: Yes

The secure development processes declared under `IXIT 19-SecDev` are fully established. Enforced
code quality standards include mandatory peer reviews for all branch merge requests, automated
static analysis sweeps (via SonarCloud and strict compiler verification flags), continuous
integration (CI) regression testing blocks, explicit pinning of external component dependency
versions, and embedded defensive programming patterns.

---

## Summary Matrix for the Technical File

| Confirmation Requirement                     | Status  | Associated Reference Index            |
|:---------------------------------------------|:-------:|:--------------------------------------|
| **Confirmation of Vulnerability Actions**    | **Yes** | `IXIT 3-VulnTypes` / `IXIT 7-UpdMech` |
| **Confirmation of Vulnerability Monitoring** | **Yes** | `IXIT 5-VulnMon`                      |
| **Confirmation of Update Procedures**        | **Yes** | `IXIT 8-UpdProc`                      |
| **Confirmation of Secure Management**        | **Yes** | `IXIT 14-SecMgmt`                     |
| **Confirmation of Secure Development**       | **Yes** | `IXIT 19-SecDev`                      |
