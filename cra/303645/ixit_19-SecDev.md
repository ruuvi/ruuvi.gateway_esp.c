# IXIT 19-SecDev: Secure Development Processes

The following declarations detail the engineering standards, static verification utilities, testing
strategies, and peer-review workflows used throughout the software lifecycle to ensure the security,
stability, and integrity of the Ruuvi Gateway codebase.

## Table C.19: IXIT 19-SecDev (Secure Development Processes)

### **ID**: SecDev-Automated-Static-Analysis

#### Description

The development lifecycle enforces automated static code analysis checks across all software modules
prior to production compilation. Codebases are systematically scanned using the SonarCloud analysis
platform alongside strict compiler validation flags (`-Wall -Wextra`). These tools parse the
codebase to identify security vulnerabilities, potential buffer overflows, memory leaks, unreachable
logic blocks, and unused functions. Any identified violations fail the automated build pipeline and
must be resolved by development engineers before the code can be prepared for release.

---

### **ID**: SecDev-Peer-Review-Governance

#### Description

Ruuvi implements a mandatory peer-review process for all firmware and software alterations. Source
code modifications targeting the core repositories—including the main ESP32 application stack (
`https://github.com/ruuvi/ruuvi.gateway_esp.c/`), the nRF52 co-processor firmware (
`https://github.com/ruuvi/ruuvi.gateway_nrf.c/`), and the Web-UI frontend application (
`https://github.com/ruuvi/ruuvi.gwui.html/`)—must be submitted via GitHub Pull Requests. Each
submission undergoes a manual code review by at least one additional senior systems engineer to
verify logical correctness, structural efficiency, and adherence to safe programming principles
before it can be merged into production branches.

---

### **ID**: SecDev-Defensive-Coding-Standards

#### Description

Software development follows defensive isolation architectures tailored to memory-constrained
embedded environments. The co-processor logic utilizes the Nordic Semiconductor nRF5 SDK v15.3.0
driver framework to maintain an isolated, receive-only execution state. The main application uses
the ESP-IDF framework, which isolates network protocols, cryptographic operations, and user
configuration routines into distinct FreeRTOS tasks with restricted memory boundaries and priority
gating. This architecture ensures that standard buffer failures or runtime faults cannot bypass
authentication layers or corrupt critical system components.

---

### **ID**: SecDev-Regression-Testing

#### Description

The testing strategy relies on automated regression sweeps combined with functional security
evaluations. Ongoing test sweeps execute software builds against simulated targets and physical
hardware units within the continuous integration (CI) infrastructure to verify performance
benchmarks and cryptographic protocols. The validation framework includes dedicated negative testing
parameters, intentionally injecting malformed JSON inputs and out-of-bounds BLE payloads to verify
that the parsing layers reject anomalies safely. Any structural defect, parsing error, or security
regression identified during these testing sweeps is logged as an issue within the repository's
native GitHub Issues layout and must be explicitly tracked and resolved prior to compiling official
production release artifacts.

---

### **ID**: SecDev-Supply-Chain-Component-Management

#### Description

Third-party libraries, driver frameworks, and core SDK dependencies are pinned to specific, verified
production versions (such as Nordic nRF5 SDK v15.3.0 and optimized ESP-IDF release tags) to prevent
untrusted or unverified upstream changes from introducing vulnerabilities. Security-sensitive data
interfaces—including Web-UI HTML/JavaScript bundles—are compiled, compressed, and minified using
production Webpack optimization engines (`webpack.prod.js`) to strip out development metadata and
comments before packaging into signing-verified binary images allocated to the `fatfs_gwui`
partition.

---

## Summary Matrix for the Technical File

| Process ID                                   | Target Domain           | Primary Enforcement Mechanism                         | Implementation Lifecycle Phase     |
|:---------------------------------------------|:------------------------|:------------------------------------------------------|:-----------------------------------|
| **SecDev-Automated-Static-Analysis**         | Source Code Quality     | SonarCloud CI Sweeps / GCC Warning Flags              | Continuous Integration (Pre-Merge) |
| **SecDev-Peer-Review-Governance**            | Code Verification       | Dual-Engineer GitHub Pull Request Approval            | Pre-Merge Development Phase        |
| **SecDev-Defensive-Coding-Standards**        | Firmware Architecture   | FreeRTOS Task Separation / Rx-Only Co-Processor       | Initial Design & Implementation    |
| **SecDev-Regression-Testing**                | Security & Stability    | Automated Sweeps / Negative Boundary Testing Logs     | Pre-Release Verification Phase     |
| **SecDev-Supply-Chain-Component-Management** | Dependency Verification | Component Version Pinning / Webpack Production Minify | Build & Post-Build Packaging       |
