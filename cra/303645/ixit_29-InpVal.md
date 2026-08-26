# IXIT 29-InpVal: Data Input Validation

The following declarations detail the input validation mechanisms, structure checking, and payload
parsing filters implemented by the Device Under Test (DUT) across all physical, network, and
application boundaries to prevent memory corruption, injection exploits, or invalid state changes.

---

## Table C.29: IXIT 29-InpVal (Data Input Validation)

### **ID**: InpVal-JSON-Schema-Validation

#### Source

* `UserIntf-Local-Hotspot-Captive-Portal`
* `UserIntf-LAN-Management-WebUI`
* `LogIntf-HTTP-Server`

#### Description

Every administrative configuration payload submitted via the browser-based interfaces targeting the
gateway’s local HTTP configuration endpoint (`POST /ruuvi.json`) is subjected to a strict
programmatic schema validation layer before mutations are committed to non-volatile flash
partitions (`nvs` and `gw_cfg_def`).

* **Format and Structure Rules:** Incoming HTTP request bodies are handled by a dedicated JSON
  stream parser framework. The parameters must map to strict predefined value definitions (such as
  alphanumeric strings for SSIDs, valid IP address formats for manual configurations, or boolean
  flags for relay selectors).
* **Handling Unexpected Data:** Any deviation from the structural layout—such as passing malformed
  JSON syntax, unmapped key properties, invalid parameter data types (e.g., passing string objects
  into integer targets), or array configurations exceeding maximum buffer restrictions—triggers an
  immediate rejection. The firmware discards the corrupt data segment safely, outputs an error trace
  block to `LogIntf-USB-UART-Log-Stream`, logs an internal error flag, and responds to the network
  client with an explicit HTTP 400 Bad Request status code, leaving the existing active settings
  untouched.

---

### **ID**: InpVal-BLE-Advertisement-Filtering

#### Source

* `ExtSens-Logical-BLE-Radio-Scanning`
* `PhyIntf-BLE-nRF52`

#### Description

The radio interface application running on the nRF52811 co-processor filters raw incoming air frames
at the link layer to process only well-formed wireless structures before serial UART handoffs to the
ESP32.

* **Format and Structure Rules:** Incoming BLE packet frames are parsed against active physical
  constraint settings defined by the user in Step 10 of the onboarding wizard (
  `UserDec-10-Bluetooth-Scanning`). Packets must pass structural length parameters, match selected
  manufacturer IDs (unless promiscuous mode is enabled), conform to selected active Bluetooth PHY
  rules (`1M`, `2M`, or `Coded`), and comply with local whitelists or blacklists governed by
  explicit 48-bit hardware MAC address filters.
* **Handling Unexpected Data:** Random radio noise, overlapping packet collisions, frames
  originating from blacklisted hardware addresses, or packets carrying malformed structures that
  fail internal BLE cyclic redundancy checks (CRC) are systematically dropped at the link layer by
  the nRF52811 hardware transceiver. If a frame passes wireless validation but carries an unmapped
  payload structure, it is safely dropped during the internal UART streaming serialization task,
  preventing buffer overflow vectors from reaching the main ESP32 task pipelines.

---

### **ID**: InpVal-Firmware-Binary-Signature-Verification

#### Source

* `UserDec-3-Onboarding-Firmware-Update`
* `UserDec-5-Automatic-Updates`
* `LogIntf-FW-Update-Client`

#### Description

The system executes a deferred, multi-stage validation check over incoming firmware files to verify
authenticity and integrity before full platform validation is acknowledged.

* **Main Firmware Image Checks:** The system first downloads the update payloads and streams them
  directly into the inactive flash partition slot (`ota_0` or `ota_1`). Once writing completes, the
  background maintenance client executes `esp_image_verify` from the native ESP-IDF
  `bootloader_support` library. This verifies the binary image parameters and authenticates the
  block's cryptographic hash against the production keys.
* **Dependent Asset Verification Loop:** If the main firmware image signature is certified valid,
  the gateway continues by downloading the companion filesystems containing the updated Web-UI
  layout (`fatfs_gwui`) and the co-processor code (`fatfs_nrf52`). The reference cryptographic
  signatures for these auxiliary assets are structurally embedded directly within the main
  application binary text segment. Consequently, their verification occurs strictly post-reboot
  during early system initialization when the new firmware becomes active.
* **Handling Unexpected Data:** If the core application image verification fails post-download, the
  update sequence aborts and the inactive partition is not flagged for boot. If a signature mismatch
  or structural file corruption is detected for the Web-UI or co-processor files during the
  subsequent boot verification step, initialization is instantly flagged as unsafe, and the platform
  triggers a firmware rollback procedure to fall back to the prior known-good partition stack.

---

### **ID**: InpVal-M2M-Token-Validation

#### Source

* `UserDec-6-Remote-Access-Settings`
* `LogIntf-HTTP-Server`

#### Description

Programmatic machine-to-machine interactions accessing the local network endpoints are subjected to
strict token validation filters.

* **Format and Structure Rules:** Incoming requests must pass exact alphanumeric bearer token
  parameters matching the saved variables (`lan_auth_api_key` for read-only access to `/history` or
  `lan_auth_api_key_rw` for administrative path control overrides).
* **Handling Unexpected Data:** If the token header contains an invalid string format, incorrect
  length attributes, or character structures that do not match the target configuration, the query
  is immediately blocked. The HTTP task layer terminates the socket link and returns an explicit
  HTTP 401 Unauthorized status frame to isolate the logic.

---

## Summary Matrix for the Technical File

| Validation ID                                     | Data Input Medium               | Evaluation Verification Method                                            | Action on Unexpected Input                                                                                                    |
|:--------------------------------------------------|:--------------------------------|:--------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------|
| **InpVal-JSON-Schema-Validation**                 | Local Web-UI Configuration      | Type-matching and key alignment schema checks                             | Drop input, retain parameters, and return HTTP 400.                                                                           |
| **InpVal-BLE-Advertisement-Filtering**            | Wireless 2.4 GHz Air Interfaces | Transceiver Link-Layer CRC, PHY, and MAC Mask filters                     | Immediate packet drop by the nRF52811 radio sub-system.                                                                       |
| **InpVal-Firmware-Binary-Signature-Verification** | Outbound Update Network Link    | Post-download `esp_image_verify` / Pre-initialization Embedded RSA checks | Terminate update sequence on download failure; trigger an automated partition rollback loop if post-reboot asset checks fail. |
| **InpVal-M2M-Token-Validation**                   | Programmatic Local HTTP Queries | Direct alphanumeric string matching                                       | Terminate connection and return HTTP 401 Unauthorized.                                                                        |
