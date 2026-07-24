# IXIT 14-SecMgmt: Secure Management Processes

The following declarations detail the complete secure management processes governing the
lifecycle—including generation, provisioning, storage, updates, decommissioning, and compromise
handling—of all critical security parameters maintained by the device.

---

## Table C.14: IXIT 14-SecMgmt (Secure Management Processes)

### **ID**: SecMgmt-Hardware-Silicon-Root

#### Description

Governs the lifecycle management of the 64-bit hardware-unique identifier (
`SecParam-Hardware-DeviceID`).

* **Generation & Provisioning:** Permanently burned into read-only Factory Information Configuration
  Registers (FICR) on the nRF52811 co-processor by the silicon vendor during chip fabrication. As
  specified in `SecParam-Hardware-DeviceID`, it is a hard-coded unique per-device identity that
  cannot be altered, spoofed, or re-provisioned during the operational life of the DUT.
* **Storage:** Maintained in read-only registers on the co-processor hardware layout. During early
  system initialization, the running firmware on the nRF52811 reads this value from FICR and passes
  it over the internal UART bus to the ESP32. The ESP32 application tasks cache this value
  dynamically in RAM (static application task memory). This identifier serves two primary functions:
  1. It acts as the default symmetric secret key layer for calculating message authenticity headers
     (`SecComMech-HMAC-Signing`) on outbound telemetry data packets sent to the Ruuvi Cloud.
  2. It functions as the static seed for the hardware-unique factory-default administrative
     credential string (`SecComMech-WebUI-Session`) used to gate access to the local Web-UI.
* **Updates & Archival:** Statically immutable in hardware silicon; no update or archival mechanism
  exists for the underlying register value.
* **Decommissioning & Destruction:** Relies on physical destruction of the underlying co-processor
  silicon package, as the register cells are non-volatile and physically permanent.
* **Compromise & Risk Management:**
  * *Default Password Compliance:* The identifier is printed on a physical sticker label attached to
    the underside of the gateway enclosure and printed to the local diagnostic console trace
    (`LogIntf-USB-UART-Log-Stream`) to facilitate out-of-the-box deployment. Because the credential
    is unique per device and 64 bits in length (high entropy), it satisfies
    `ETSI EN 303 645 Provision 5.1-2` natively as a unique pre-installed password. Updating the
    administrative credentials during or after onboarding is fully supported via the Web-UI but is
    not forced.
  * *Sensitive Data Exposure Protection:* Sensitive security parameters (such as user-defined
    passwords, Wi-Fi passkeys, and API tokens) are logically filtered and never exposed or returned
    in cleartext over the Web-UI configuration interface or diagnostic log streams.
  * *HMAC Key Exposure & Dynamic Rotation:* An attacker possessing physical access to the device
    casing can read the `DEVICEID` sticker to attempt telemetry spoofing for that specific gateway.
    Because `DEVICEID` is globally unique, this physical attack vector is strictly localized and
    cannot scale to other fleet units. Furthermore, the Ruuvi Cloud backend mitigates this risk by
    dynamically rotating the active `HMAC-SHA256` key at runtime via the `Ruuvi-HMAC-KEY` inbound
    HTTPS response header, decoupling the runtime signing secret from the physical label.

#### Cross-Reference

* Directly tracks and manages the physical parameters declared under `SecParam-Hardware-DeviceID`.

---

### **ID**: SecMgmt-Local-Credentials

#### Description

Governs the lifecycle management of local authentication barriers, specifically covering the Web-UI
login credentials (`SecParam-LAN-WebUI-Credentials`) and the device network access parameters (
`SecParam-WiFi-STA-Credentials`).

* **Generation & Provisioning:** The default factory administrative password is derived from the
  hexadecimal representation of the hardware `DEVICEID` (the nRF52 FICR identifier mapped under
  `SecParam-Hardware-DeviceID`) printed on the external casing sticker label. Custom administrative
  credentials and network SSIDs/passkeys are generated manually by the operator during the
  multi-step onboarding wizard. When the administrator provisions a custom access secret, the
  frontend Web-UI concatenates the user parameters (
  `username + ':' + gatewayName + ':' + unencrypted_password`) and runs an MD5 hashing block to
  translate it into a secure signature hash array.
* **Storage:** Persistently written within the main application configuration file (`ruuvi.json`)
  located inside the internal non-volatile flash partition (`nvs`). As defined in
  `SecParam-LAN-WebUI-Credentials`, the password string itself is stored exclusively as a
  pre-computed cryptographic hash digest format (`lan_auth_pass`). Network credentials (
  `wifi_sta_config.password`) are retained in cleartext within `ruuvi.json` since flash encryption
  is not enabled on the microcontroller layout. Protection against unauthorized data disclosure is
  handled logically: the firmware's runtime engine explicitly strips or masks these sensitive
  credential variables out of the `ruuvi.json` payload data stream whenever configuration states are
  read via the network-facing Web-UI. Furthermore, these credential fields are completely omitted
  from the diagnostic console trace maps streaming across `LogIntf-USB-UART-Log-Stream`.
* **Updates:** Modified on-demand by an authenticated administrator holding an active session
  context established through the LAN Web-UI panel.
* **Decommissioning, Archival & Destruction:** Eradication of these parameters is achieved via a
  local factory reset execution loop, triggered by pressing and holding the physical `CONFIGURE`
  button for 7 seconds or longer. Because the underlying ESP-IDF NVS partition layer utilizes an
  append-only transaction log structure for wear-leveling, standard configuration updates do not
  instantly overwrite old physical flash sectors when saving changes. To guarantee total data
  destruction, the 7-second hardware button press forces a low-level structural formatting
  block-erasure sequence across both the `nvs` and `gw_cfg_def` flash partition blocks. This clears
  all raw flash allocation cells and rolls back the interface to the baseline unique per-device
  factory default state. No archival logs are kept.
* **Compromise Management:** If local credentials are compromised, an administrator can invalidate
  all active session tokens by triggering a factory reset via the physical hardware button or
  submitting a credential update payload via an authenticated session.

#### Cross-Reference

* Directly tracks and manages the parameters declared under `SecParam-LAN-WebUI-Credentials` and
  `SecParam-WiFi-STA-Credentials`.

---

### **ID**: SecMgmt-Programmatic-M2M-Tokens

#### Description

Governs the operational lifecycle of user-controlled Machine-to-Machine authentication sequences,
specifically managing the Read-Only (`lan_auth_api_key`) and Read/Write (`lan_auth_api_key_rw`) keys
encapsulated under `SecParam-LAN-Bearer-Tokens`.

* **Generation & Provisioning:** Generated inside the user's browser client layout using the
  automated Web Crypto API high-entropy cryptographic pipeline:
  `crypto.enc.Base64.stringify(crypto.SHA256(crypto.lib.WordArray.random(32)))`. The resulting token
  is provisioned directly to the device flash memory when the administrator saves the settings
  block.
* **Storage:** Saved inside the `ruuvi.json` file block within the non-volatile `nvs` partition
  configuration array. Because flash encryption is not enabled on the microcontroller architecture,
  these parameters reside not encrypted at rest within the layout blocks. Protection against readout
  over network vectors relies on the application logic stripping these tokens when configurations
  are exported via the Web-UI, and these fields are entirely withheld from the local
  `LogIntf-USB-UART-Log-Stream` serial outputs.
* **Updates:** Tokens cannot be incrementally updated or altered. Instead, modification is processed
  via complete token replacement, where a newly generated string overwrites the existing entry. If
  an entry is cleared and left empty, its corresponding API endpoint verification loop is
  deactivated.
* **Decommissioning & Destruction:** Clearing a token field inside the Web-UI dashboard panel or
  executing a hardware factory reset completely purges the string segment from non-volatile storage,
  invalidating any downstream programmatic script loop attempting to access the endpoints.
* **Compromise Management:** Because these tokens are transmitted via standard HTTP `Authorization`
  headers over Port 80, they are vulnerable to local network interception. In the event of
  network-layer exposure, the administrator can invalidate the token by navigating to the advanced
  configurations panel to regenerate or permanently wipe the exposed token fields.

#### Cross-Reference

* Directly tracks and manages the validation parameters declared under `SecParam-LAN-Bearer-Tokens`.

---

### **ID**: SecMgmt-Outbound-Assets-And-Secrets

#### Description

Governs the lifecycle of outbound connection client structures, tracking symmetric signing
constants (`SecParam-HMAC-Symmetric-Secrets`), along with target endpoints, tokens, and SSL
keys/certificates grouped under `SecParam-Remote-Config-Assets`,
`SecParam-Custom-HTTP-Telemetry-Assets`, `SecParam-Custom-Stream-Telemetry-Assets`, and
`SecParam-System-Statistics-Assets`.

* **Generation & Provisioning:** HMAC keys are initialized by default using the unique 64-bit
  hardware `DEVICEID` seed declared in `SecParam-Hardware-DeviceID`. Custom telemetry destination
  credentials, cloud target passwords, and remote orchestration endpoints/SSL assets are provisioned
  manually by the system administrator.
* **Storage:** Stored within secure application task memory contexts at runtime (
  `g_hmac_sha256_key_ruuvi`, `g_hmac_sha256_key_custom`, and `g_hmac_sha256_key_stats`) and written
  inside the `ruuvi.json` manifest within the internal `nvs` partition (or inside the auxiliary
  `gw_cfg_def` flash partition during certificates storage operations). These sensitive target
  records are stripped from standard Web-UI queries and console log outputs.
* **Updates:** Outbound targets, certificates, and cloud credentials are updated via the Web-UI. As
  detailed in `SecParam-HMAC-Symmetric-Secrets`, the keys can be dynamically rotated at runtime by a
  verified receiving cloud server infrastructure through the ingestion of a high-entropy string
  passed inside the `Ruuvi-HMAC-KEY` inbound HTTPS response header field.
* **Decommissioning & Destruction:** Completely wiped from flash storage cells during local
  system-wide hardware factory reset cycles via a clean formatting of the NVS storage partition
  blocks.
* **Compromise Management:** Outbound connection structures use isolated mbedTLS context instances.
  If an endpoint credential leaks, updating the target secret inside the local gateway configuration
  invalidates the compromised token stream.

#### Cross-Reference

* Directly tracks and manages the cryptographic parameters declared under
  `SecParam-HMAC-Symmetric-Secrets`, `SecParam-Remote-Config-Assets`,
  `SecParam-Custom-HTTP-Telemetry-Assets`, `SecParam-Custom-Stream-Telemetry-Assets`, and
  `SecParam-System-Statistics-Assets`.

---

### **ID**: SecMgmt-CoProcessor-Integrity-Remediation

#### Description

Governs the boot-time secure initialization, anti-tamper tracking, and firmware verification layout
of the physical nRF52811 co-processor sub-system.

* **Hardening & Verification Mechanics:** Enforces a rigid automated code integrity check during the
  early startup execution path. The master ESP32 host halts the nRF52811 chip over the physical
  layout traces via the Serial Wire Debug (SWD) bus interface. It dynamically injects a secure
  SHA-256 binary calculation tool directly into the target's internal RAM segment and updates the
  core PC register block. The injected stub executes a bare-metal scan across the active nRF52 flash
  memory locations, computing a full cryptographic digest (`nrf52swd_calc_sha256_digest_on_nrf52`).
* **Storage & Reference Alignment:** The master ESP32 host reads the completed digest signature
  block out of RAM and evaluates it against the production reference firmware payload preserved
  inside the signature-verified `fatfs_nrf52` system flash partition. If the active memory block
  digest matches the signed partition footprint, the check passes, and asynchronous serial
  communication channels (UART link layer) are safely initialized.
* **Compromise and Tamper Remediation:** This architectural loop provides definitive proof that an
  attacker possessing specialized physical hardware emulation equipment (such as an external SWD
  hardware debugger tool) cannot permanently compromise or maintain unauthorized code alterations
  inside the co-processor's flash space. If an out-of-band firmware modification or a flash hash
  discrepancy is identified at boot, the master host short-circuits execution, immediately triggers
  an automated rollback recovery block (`nrf52fw_update_fw_step4`), and physically overwrites the
  target co-processor's memory cells with the verified factory image before passing control.

#### Cross-Reference

* Directly maps to the physical component validation criteria declared under `SoftComp-nRF52FW` and
  `SecComMech-CoProcessor-SWD-Validation`.

---

## Summary Matrix for the Technical File

| Management Process ID                 | Target Parameters Governed                                          | Primary Provisioning Method                                    | Destruction / Decommissioning Mechanism                                     | Associated Index ID                                                  |
|:--------------------------------------|:--------------------------------------------------------------------|:---------------------------------------------------------------|:----------------------------------------------------------------------------|:---------------------------------------------------------------------|
| `SecMgmt-Hardware-Silicon-Root`       | `SecParam-Hardware-DeviceID`                                        | Pre-programmed by Silicon Vendor (nRF52 FICR registers)        | Physical Component Destruction Only                                         | `SecParam-Hardware-DeviceID`                                         |
| `SecMgmt-Local-Credentials`           | `SecParam-LAN-WebUI-Credentials`<br>`SecParam-WiFi-STA-Credentials` | MD5 Hashed Onboarding Setup / Manual Input                     | Local Factory Reset (Complete physical NVS partition formatting)            | `SecParam-LAN-WebUI-Credentials`<br>`SecParam-WiFi-STA-Credentials`  |
| `SecMgmt-Programmatic-M2M-Tokens`     | `SecParam-LAN-Bearer-Tokens`                                        | Browser Web Crypto API / Direct M2M JSON Payload Configuration | UI Key Clearing / Local Factory Reset (Complete partition formatting)       | `SecParam-LAN-Bearer-Tokens`                                         |
| `SecMgmt-Outbound-Assets-And-Secrets` | `SecParam-HMAC-Symmetric-Secrets`<br>and Remote Telemetry Assets    | Manual Web Entry / Dynamic Header Session Key Rotation         | Local Factory Reset (Complete physical NVS partition formatting)            | `SecParam-HMAC-Symmetric-Secrets`<br>`SecParam-Remote-Config-Assets` |
| `SecMgmt-CoProcessor-Integrity`       | `SoftComp-nRF52FW` Firmware Stack Blocks                            | Boot-Time Automated SWD Dynamic RAM Stub Injection             | Automatic Host Overwrite / NVS Firmware Rollback Flash Partition formatting | `SoftComp-nRF52FW`<br>`SecComMech-CoProcessor-SWD-Validation`        |
