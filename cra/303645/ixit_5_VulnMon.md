# IXIT 5-VulnMon: Vulnerability Monitoring

The following table describes the procedures used to monitor for vulnerabilities in the Ruuvi
Gateway hardware, firmware, and third-party dependencies.

## Table C.5: IXIT 5-VulnMon (Vulnerability Monitoring)

### VulnMon-1

**Core Framework & Hardware Monitoring**: Ruuvi monitors the official security channels of the 
primary hardware and framework providers. This includes:

- **Espressif (ESP32/ESP-IDF)**: Monitoring
  the [Espressif Security Advisories](https://www.espressif.com/en/support/documents/advisories) for
  SoC-level vulnerabilities and framework patches.

- **Nordic Semiconductor (nRF52)**: Monitoring
  the [Nordic Security Advisories](https://docs.nordicsemi.com/bundle/struct_sa/page/struct/sa.html)
  for issues affecting the nRF52811 and the nRF5 SDK v15.3.0.

- **mbedTLS**: Monitoring [TrustedFirmware.org](https://www.trustedfirmware.org/projects/mbed-tls/)
  for vulnerabilities in the TLS stack used for secure communications.

### VulnMon-2

**Software Dependency Auditing (Web-UI)**: The Node.js-based Web-UI and its dependencies (including 
jQuery, CryptoJS, Elliptic, and Winston) are monitored using automated tools.

- **GitHub Advisory Database**: Ruuvi utilizes automated GitHub Security Alerts for the Gateway 
  repository, which scans all package.json dependencies against known CVEs.

- **npm audit**: During the CI/CD pipeline and local development, npm audit is executed to identify 
  and assess vulnerabilities in the JS dependency tree.

### VulnMon-3

**Global CVE & Threat Intelligence Monitoring**: The Security Incident Team (SIT) performs weekly
reviews of the [NIST National Vulnerability Database (NVD)](https://nvd.nist.gov/vuln) and
[MITRE CVE](https://www.cve.org/) lists using keywords related to the Ruuvi Gateway’s hardware
stack (ESP32, nRF52, mbedTLS). If a vulnerability is discovered, it is logged; 
if it is found to be non-applicable to the DUT (e.g., affecting a feature not used), it
is documented as "n/a" to provide an audit trail of the investigation.

### VulnMon-4

**Leaked Information & Public Disclosure Monitoring**: Ruuvi monitors public repositories, security
forums, and developer communities for leaked credentials, unauthorized firmware forks, or "Zero Day"
reports specifically referencing "Ruuvi Gateway." This includes monitoring
the [Ruuvi Community Forum](https://f.ruuvi.com/) and specialized hardware security news
aggregators.

### VulnMon-5

**Rectification Workflow**: Once a potential vulnerability is identified via VulnMon-1 through 
VulnMon-4, the SIT initiates the assessment process defined in **IXIT 3-VulnTypes**.
Rectification involves:
1. **Risk Analysis**: Determining if the vulnerability can be triggered in the Gateway’s specific 
   operating environment.
2. **Patch Development**: Applying updates (e.g., updating ESP-IDF versions or JS dependency 
   versions).
3. **Validation**: Functional testing to ensure the patch does not break existing Gateway features.

---

## Internal Monitoring Details (SIT Briefing)

To support the above procedures, Ruuvi maintains an internal Vulnerability Tracking Ledger. For
every Node.js dependency listed (such as webpack, babel-loader, or elliptic), the monitoring system
is configured to flag any "Critical" or "High" severity vulnerabilities immediately.

For the nRF5 SDK v15.3.0, which is a legacy SDK, special attention is paid to backported security
fixes provided by Nordic Semiconductor to ensure the BLE communication remains robust against known
protocol attacks.
