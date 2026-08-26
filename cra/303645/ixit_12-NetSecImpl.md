# IXIT 12-NetSecImpl: Network and Security Implementations

The following declarations map the specific hardware and software component implementations
providing network and security functionalities across the Ruuvi Gateway (DUT), detailing the review
scopes, evaluation frameworks, and tracking records validating the codebase.

## Table C.12: IXIT 12-NetSecImpl (Network and Security Implementations)

### **ID**: NetSecImpl-Framework-Stack

#### Description

The core network stack and operating environment abstraction layer are implemented via the Espressif
IoT Development Framework (ESP-IDF v4.2.5). This framework encapsulates the Lightweight IP (lwIP)
stack and FreeRTOS kernel to manage thread execution pools, low-level Wi-Fi/Ethernet socket mapping
drivers, and background UDP/TCP network routines.

#### Review/Evaluation Method

The main firmware layer built on top of the ESP-IDF framework undergoes a strict, multi-tiered
verification and gatekeeping process before any code modification is permitted into the stable
release line:

1. **Automated Static & Language Analysis:**

* **SonarCloud:** Conducts automated static application security testing (SAST) on every repository
  pull request to track code quality metrics, catch logic flaws, and detect security regressions.
* **Clang-Format:** Enforces uniform formatting constraints across C source boundaries to preserve
  clean, peer-reviewable code structures.
* **GitHub Copilot:** Integrated into the engineering pipeline to provide real-time code checking,
  context-aware bug detection, and automated programmatic code verification during development.

2. **Continuous Integration (CI) Unit-Testing:**
   A dedicated test runner executes via GitHub Actions on every code submission, sequentially
   evaluating a comprehensive matrix of foundational firmware component modules, subsystem drivers,
   and logic tasks covering:

* MQTT Stream Telemetry Engine
* HTTP/HTTPS Post & Telemetry Pipelines
* Gateway Configuration, Storage & Authorization Management
* Core Utilities & Cryptographic Wrappers
* Hardware & Co-Processor Integration Controllers

3. **Mandatory Human Gatekeeping:**
   Branch protection rules are strictly enforced on the remote repository. Independent of automated
   green statuses from the CI test runner, **at least one human senior developer must manually
   review and explicitly approve** a pull request before it can be merged into the `master` branch.

#### Report

Development compliance, automated coverage scores, and human review approvals are tracked
dynamically within the public GitHub pull request and issue tracking architecture for the project
repository (`https://github.com/ruuvi/ruuvi.gateway_esp.c`).

---

### **ID**: NetSecImpl-Crypto-Library

#### Description

The cryptographic functionalities—including Transport Layer Security (TLS 1.2/1.3) client routines,
secure boot block validation utilities, hashing utilities, and HMAC payload calculations—are driven
by the integrated `mbedtls` library distributed within ESP-IDF v4.2.5.

**Maintenance Modification Hooks:** To ensure compatibility with modern cryptographic rules and
resource constraints on the hardware platform, the standard library layer was modified as follows:

1. Backported critical stability and security bugfixes related to TLS 1.3 operations from newer
   upstream mbedTLS versions.
2. Custom-patched the library to integrate support for fixed-size pre-allocated buffers, reducing
   dynamic memory fragmentation issues under constrained runtime conditions.

#### Review/Evaluation Method

The integration layer, backported components, and custom pre-allocated buffer engine are subjected
to rigorous continuous regression checking. An automated GitHub Actions pipeline executing on an
`ubuntu-22.04` environment triggers an isolated testing workflow (`test-mbedtls`) upon every
repository push and pull request.

The verification engine compiles the codebase using dynamic diagnostic flags, specifically applying
the **AddressSanitizer (`-fsanitize=address -g`)** option across compiler parameters (
`CMAKE_C_FLAGS` / `CMAKE_CXX_FLAGS`) to catch memory corruption, out-of-bounds pointer leaks, or
allocation overflows. The test suite evaluates custom memory configurations against the native core
suite (`make test`) and three specific target configurations located at
`components/mbedtls/mbedtls/tests`:

1. `run-test_suite_ssl-sanitize.sh`: Evaluates standard SSL processing states against the
   `test_suite_ssl` and the customized `test_suite_ssl_pre_allocated_buffers` targets.
2. `run-test_suite_ssl-reduced_buffer_size.sh`: Compiles and executes tests using a custom header
   blueprint (`user-config-reduced-buffer-size.h`) to verify boundaries under memory constraints.
3. `run-test_suite_ssl-variable_buffer.sh`: Validates dynamic constraints using a dedicated
   parameter set (`user-config-variable-buffer-h`).

#### Report

Test run diagnostics and coverage details are parsed directly inside the automated GitHub Action job
log histories. Vulnerability monitoring maps to the official mbedTLS advisory disclosures, tracked
at `https://www.trustedfirmware.org/projects/mbed-tls/`.

---

### **ID**: NetSecImpl-Web-Application-Core

#### Description

The application-layer logical session environment, client-side handshake logic, and parsing layer
for the local user setup layout are implemented in the Web-UI application module (`src/auth.mjs` /
`crypto.mjs`). It is compiled out into a standalone FATFS binary layout (`fatfs_gwui.bin`)
provisioned with custom encryption features.

#### Review/Evaluation Method

The custom script-based authentication layers were reviewed manually by a team of two senior systems
developers. The audit focused on the end-to-end exchange pipeline for the custom ephemeral
`Ruuvi-Ecdh-Pub-Key` key agreement vectors, ensuring the session-wide AES key derivation loop does
not leak cryptographic primitives across cross-site script gaps. Automated static checking tools
were run against the UI javascript bundle to search for input handling errors, insecure storage
habits, or parsing bugs.

#### Report

The source code for the Web-UI application layer is maintained in a public GitHub repository at
`https://github.com/ruuvi/ruuvi.gwui.html/`. Any structural defects, security bugs, or
vulnerabilities identified during manual peer reviews or regression sweeps are publicly logged,
tracked, and managed to resolution via the repository's native GitHub Issues interface prior to
official production artifact builds.

---

### **ID**: NetSecImpl-CoProcessor-Firmware

#### Description

The dedicated Bluetooth Low Energy (BLE) passive monitoring sub-component binary code running on the
nRF52811 co-processor hardware chip layout. The firmware was developed utilizing the Nordic
Semiconductor nRF5 SDK v15.3.0. Its purpose is to process radio sweeps and drop non-matching BLE
frames.

#### Review/Evaluation Method

The underlying nRF5 SDK v15.3.0 framework relies on Nordic's standard silicon drivers. Ruuvi
engineers performed manual code reviews focused on the custom application logic handling the radio
configuration arrays, ensuring that the passive scan engine operates in a strict receive-only (
Rx-Only) execution state. This review guarantees that the co-processor isolates its operational
boundaries and ignores incoming connection or pairing request attempts.

#### Report

The source code for the co-processor application layer is maintained in a public GitHub repository
at `https://github.com/ruuvi/ruuvi.gateway_nrf.c/`. Any identified issues, driver bugs, or
vulnerability monitoring updates are publicly logged and tracked to resolution using the
repository's native GitHub Issues layout.

---

## Summary Matrix for the Technical File

| Implementation ID                   | Primary Component                  | Operating Version | Core Functional Scope                                                | Evaluation Method                                                               |
|:------------------------------------|:-----------------------------------|:------------------|:---------------------------------------------------------------------|:--------------------------------------------------------------------------------|
| **NetSecImpl-Framework-Stack**      | ESP-IDF (lwIP / FreeRTOS)          | v4.2.5            | L2/L4 Network Stack, Socket Handlers, Drivers                        | CI Test Suite, SonarCloud SAST, Copilot Checking, Mandatory 1-Human PR Approval |
| **NetSecImpl-Crypto-Library**       | mbedtls (Patched / Backported)     | Custom Backport   | TLS 1.2/1.3, RSA-3072, HMAC-SHA256, Fixed-Size Pre-Allocated Buffers | CI GitHub Actions, AddressSanitizer (`-fsanitize=address`) Testing Suite        |
| **NetSecImpl-Web-Application-Core** | Web-UI (`auth.mjs` / `crypto.mjs`) | Production Bundle | Ephemeral ECDH Handshake, AES-CBC UI Encryption                      | Dual-Engineer Peer Review, JS Static Analyzers                                  |
| **NetSecImpl-CoProcessor-Firmware** | nRF5 SDK                           | v15.3.0           | Passive BLE Scanning, Rx-Only Radio Controls                         | Passive Listener State Review                                                   |
