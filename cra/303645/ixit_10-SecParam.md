# IXIT 10-SecParam: Security Parameters

The following table lists all critical and public security parameters persistently or transiently
stored on the Ruuvi Gateway (DUT) during intended usage, specifying their protection frameworks,
provisioning contexts, and generation mechanisms.

## Table C.10: IXIT 10-SecParam (Security Parameters)

### **ID**: SecParam-FW-Verification-Key

#### Description

The RSA-3072 public signature key embedded directly within the application image text segment. It
serves as the static cryptographic root of trust utilized by the application-layer firmware
validation pipeline (`main/fw_update.c`) to verify the integrity and authenticity of downloaded OTA
applications and all data partition images (`fatfs_gwui.bin` / `fatfs_nrf52.bin`). It is hard-coded
in the device software binary code.

#### Type

public

#### Security Guarantees

Ensures **Integrity**. The parameter must remain immutable to prevent an attacker from substituting
a compromised public key to bypass firmware origin verification.

#### Protection Scheme

The key resides in the application code segment flashed into the active factory/OTA partition. It is
protected by physical enclosure bounds and application-layer access restrictions. Any logical
modification attempt requires rewriting the firmware block via `esptool.py` (which requires physical
pin strapping) or executing an authorized firmware update block signed by the matching private key.

#### Provisioning Mechanism

N/A

#### Secure Communication Mechanisms

N/A (The parameter is statically compiled into the firmware image and never transmitted over any
interface).

#### Generation Mechanism

N/A

---

### **ID**: SecParam-Main-Firmware-Signature

#### Description

The RSA-3072 signature block generated over the main application image and appended directly to the
end of the compiled binary image file. It is verified at the application layer during the OTA
validation sequence.

#### Type

public

#### Security Guarantees

Ensures **Integrity** and **Authenticity** of the main application partition image, preventing
execution of corrupted, truncated, or unauthenticated firmware binaries.

#### Protection Scheme

Appended to the trailing edge of the active and inactive OTA app slots. Prior to triggering a
partition swap or completing an update cycle, the verification engine evaluates the block using
`esp_ota_end_patched` against the computed SHA-256 binary hash.

#### Provisioning Mechanism

N/A

#### Secure Communication Mechanisms

`SecComMech-Firmware-Signature-Verification`.
The parameter is transmitted remotely over the network during an OTA firmware update session. During
normal runtime, it is checked strictly locally by the verification engine and is not remotely
accessible.

#### Generation Mechanism

N/A

---

### **ID**: SecParam-WebUI-Partition-Signature

#### Description

The 4096-byte RSA-3072 signature block representing the valid production state tag for the Web-UI
data partition (`fatfs_gwui.bin`). This signature structure is extracted at compile-time and
embedded as a static binary blob directly within the main firmware ELF executable text segment.

#### Type

public

#### Security Guarantees

Ensures **Integrity** of the local user interface files, protecting against cold-boot file
manipulation vectors targeting the unencrypted data file system partition.

#### Protection Scheme

The parameter inherits the structural text protections of the main application partition. Early
during the boot cycle initialization phase, the `fw_update_read_flash_info_and_check_signatures()`
routine computes a dynamic SHA-256 digest over the Web-UI partition, validating it against this
embedded block via `esp_secure_boot_verify_rsa_signature_block`. Mismatches abort initialization and
trigger an automated slot rollback sequence.

#### Provisioning Mechanism

N/A

#### Secure Communication Mechanisms

`SecComMech-Firmware-Signature-Verification`.
Transmitted remotely as an embedded component of the main firmware binary during an active OTA
download. At boot runtime, it is evaluated strictly locally via internal flash memory mapping and is
not accessible over any remote network interface.

#### Generation Mechanism

N/A

---

### **ID**: SecParam-nRF52-Partition-Signature

#### Description

The 4096-byte RSA-3072 signature block representing the valid production state tag for the nRF52
co-processor code partition (`fatfs_nrf52.bin`). This signature structure is extracted at
compile-time and embedded as a static binary blob directly within the main firmware ELF executable
text segment.

#### Type

public

#### Security Guarantees

Ensures **Integrity** and **Authenticity** of the co-processor firmware. Protects against
unauthorized modifications of the BLE scanning layer.

#### Protection Scheme

Maintained within the main application partition text array.

#### Provisioning Mechanism

N/A

#### Secure Communication Mechanisms

`SecComMech-Firmware-Signature-Verification`
Transmitted remotely as an embedded component of the main firmware binary during an active OTA
download. At boot runtime, it is evaluated strictly locally via internal flash memory mapping and is
not accessible over any remote network interface.

#### Generation Mechanism

N/A

---

### **ID**: SecParam-CoProcessor-Verification-Stub

#### Description

The bare-metal compiled binary utility array (`sha256_stub_bin`) embedded within the main
application text segment. During early boot sequence validation, the master ESP32 host
programmatically streams this payload across the internal SWD bus into the nRF52811 RAM allocation
table to orchestrate the hardware SHA-256 flash hash sweep.

#### Type

public

#### Security Guarantees

Ensures **Integrity** and **Access Control**. The verification tool must remain authentic to prevent
an adversary from tampering with the validation metrics or generating dummy execution completion
flags over the SWD link.

#### Protection Scheme

Compiled directly into the master software image, residing inside the active verified app slots (
`ota_0` or `ota_1`). Any attempt to alter its payload structural logic requires rewriting the
primary application storage flash.

#### Provisioning Mechanism

N/A

#### Secure Communication Mechanisms

`SecComMech-CoProcessor-SWD-Validation`. Transferred strictly locally over the isolated internal
physical track infrastructure of `LogIntf-Internal-SWD-Bus`. It is inaccessible to external network
adapters.

#### Generation Mechanism

N/A

---

### **ID**: SecParam-Hardware-DeviceID

#### Description

The 64-bit hardware-unique identifier ($DEVICEID$) extracted out of the internal Factory Information
Configuration Registers (FICR) inside the nRF52811 silicon chip structure. Formatted as uppercase
pairs separated by colons (e.g., `AA:BB:CC:DD:EE:FF:00:11`), it acts directly as the factory-default
Web-UI credential string and functions as the baseline symmetric secret key layer for outbound
payload message signatures. It is hard-coded in hardware silicon registers.

#### Type

critical

#### Security Guarantees

Ensures **Confidentiality** and **Integrity**. Protects against remote spoofing of local interface
management access and fabrication of telemetry signatures.

#### Protection Scheme

Stored in read-only factory silicon cells on the nRF52811 layout. This parameter is printed
transparently onto the local `LogIntf-USB-UART-Log-Stream` diagnostics console during active boot
execution steps for local installation validation. However, no logical network API or remote socket
command structures expose the raw register values onto unauthenticated network channels.

#### Provisioning Mechanism

Permanently burned at the chip manufacturing tier by the silicon vendor. It cannot be altered during
the operational lifecycle of the DUT.

#### Secure Communication Mechanisms

`SecComMech-HMAC-Signing`, `SecComMech-WebUI-Session`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-LAN-WebUI-Credentials

#### Description

The active username and password sequence utilized by the `x-ruuvi-interactive` challenge-response
scheme to validate administrative access onto the local network configuration server. Represents the
JSON keys `lan_auth_user` and `lan_auth_pass`. It is not hard-coded in the source code.

#### Type

critical

#### Security Guarantees

Ensures **Confidentiality** and **Integrity**. Protects against unauthorized local modification of
gateway operation routing rules and structural data target updates.

#### Protection Scheme

Stored inside the configuration file (`ruuvi.json`) within the device non-volatile flash partition (
`nvs`). The password sequence is protected by **only saving a pre-computed MD5 cryptographic hash
value** (`lan_auth_pass`) rather than a cleartext string. Because ESP32 flash encryption is not
enabled, the value is not encrypted at rest on the raw silicon tiers. Beyond the security provided
by hashing, logical network protections apply: the firmware's runtime engine explicitly filters and
removes the `lan_auth_pass` string block completely from outbound JSON payloads whenever settings
are queried via the Web-UI. Additionally, the field is completely withheld from the local
`LogIntf-USB-UART-Log-Stream` serial outputs. Changes are allowed only for users holding an
authenticated session context established via ECDH key negotiation.

#### Provisioning Mechanism

Initialized at factory setup directly to the 16-character hexadecimal mapping of the unique
hardware $DEVICEID$ string layout (matching `lan_auth_default`). Custom provisioning updates undergo
client-side generation inside the browser interface: the user input parameters are concatenated (
`username + ':' + gatewayName + ':' + unencrypted_password`) and computed through a standard MD5
hashing block into a 32-character hexadecimal string block saved to the `lan_auth_pass` parameter.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-WiFi-STA-Credentials

#### Description

The operational network SSID string and associated plaintext password block utilized by the internal
wireless stack to join target customer local area access points. Represents the JSON keys
`wifi_sta_config.ssid` and `wifi_sta_config.password`. It is not hard-coded in the source code.

#### Type

critical

#### Security Guarantees

Ensures **Confidentiality** and **Integrity**. Protects local network security paths from threat
actors scanning raw flash storage layout dumps.

#### Protection Scheme

Stored inside the configuration file (`ruuvi.json`) within the device non-volatile flash partition (
`nvs`). ESP32 flash encryption is not enabled, so the cleartext password resides unencrypted at rest
within the storage block. It is logically inaccessible via unauthenticated HTTP endpoint routes; the
firmware configuration engine scrubs the password sub-elements completely from configuration reads
and Web-UI network payloads to prevent credential leaks, and the string is never written to
`LogIntf-USB-UART-Log-Stream`.

#### Provisioning Mechanism

Assigned by the user during the initial captive-portal setup sequence, via WPS (
`esp_wifi_wps_enable`), or modified via an authenticated local Web-UI session.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-Remote-Config-Assets

#### Description

The configuration credentials and cryptographic certificate files used explicitly by the out-of-band
client configuration thread to fetch automated gateway settings parameters. Maps to
`remote_cfg_url`, `remote_cfg_auth_bearer_token`, `remote_cfg_auth_basic_user`,
`remote_cfg_auth_basic_pass`, and the dedicated storage status files `rcfg_cli_key`,
`rcfg_cli_cert`, and `rcfg_srv_cert`. It is not hard-coded in the source code.

#### Type

critical (Credentials/Private Key) / public (Certificates/URL)

#### Security Guarantees

Ensures **Confidentiality** and **Integrity** of the automation loop, preventing unauthorized
configuration interception or arbitrary configuration injection attacks.

#### Protection Scheme

Symmetric parameters are stored inside the configuration file (`ruuvi.json`) within the device
non-volatile flash partition (`nvs`). TLS PEM certificates and keys are written to distinct
allocation tables within a dedicated NVS namespace (`gw_cfg_storage`). Flash encryption is not
enabled. Raw private key and credential values are omitted from network JSON configuration queries
and are suppressed from diagnostic log stream output.

#### Provisioning Mechanism

Defined manually by the systems deployment team through the remote synchronization configuration
menus in the Web-UI panel.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`, `SecComMech-TLS`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-Custom-HTTP-Telemetry-Assets

#### Description

The access credentials and cryptographic certificate assets used to authenticate and encrypt
outbound data pushes to custom HTTP/HTTPS endpoints. Contains the basic auth credentials (
`http_user`, `http_pass`), bearer/API keys (`http_bearer_token`, `http_api_key`), client private
key (`http_cli_key`), client X.509 certificate (`http_cli_cert`), and target server root CA cert (
`http_srv_cert`). It is not hard-coded in the source code.

#### Type

critical (Credentials/Private Key) / public (Certificates)

#### Security Guarantees

Ensures **Confidentiality** and **Integrity** of HTTP POST payload routing blocks, preventing data
snooping and target platform endpoint spoofing.

#### Protection Scheme

Stored inside `ruuvi.json` on the `nvs` partition, with heavy certificates utilizing the
`gw_cfg_storage` namespace. Passwords and keys are scrubbed during Web-UI query calls and are
withheld from the UART serial terminal logs. Client keys are loaded into runtime task memory solely
within the execution scope of the HTTP telemetry thread handler loop.

#### Provisioning Mechanism

Manually populated by the administrator using the target HTTP configuration fields within the local
setup dashboard.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`, `SecComMech-TLS`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-Custom-Stream-Telemetry-Assets

#### Description

The access credentials and cryptographic certificate assets used to authenticate and secure
long-lived stream connections over MQTT, MQTTS, WS, or WSS. Contains target broker parameters (
`mqtt_user`, `mqtt_pass`), client private key (`mqtt_cli_key`), client X.509 certificate (
`mqtt_cli_cert`), and broker server root CA cert (`mqtt_srv_cert`). It is not hard-coded in the
source code.

#### Type

critical (Credentials/Private Key) / public (Certificates)

#### Security Guarantees

Ensures **Confidentiality** and **Integrity** of raw sensor streaming blocks, blocking rogue
injection vulnerabilities or passive line wiretapping.

#### Protection Scheme

Persistent configuration parameters reside inside `ruuvi.json` on the `nvs` partition (not encrypted
at rest, as flash encryption is not enabled). Private keys are stripped from administrative Web-UI
exports and diagnostic log lines, and are accessed solely by the MQTT task loop to build connection
handshakes via mbedTLS context handles.

#### Provisioning Mechanism

Uploaded and saved by the administrator when moving from local HTTP relays to specific third-party
message broker hubs.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`, `SecComMech-TLS`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-System-Statistics-Assets

#### Description

The parameters and authentication assets required to process and secure the device heartbeat and
background diagnostic tracking stream. Contains target URL identifiers (`http_stat_url`), basic auth
metrics (`http_stat_user`, `http_stat_pass`), local client private key (`stat_cli_key`), public
client certificate (`stat_cli_cert`), and the receiving diagnostics node validation root CA cert (
`stat_srv_cert`). It is not hard-coded in the source code.

#### Type

critical (Credentials/Private Key) / public (Certificates/URL)

#### Security Guarantees

Ensures **Confidentiality** and **Integrity** of core performance tracking information, preventing
firmware tracking manipulation or server endpoint hijacking.

#### Protection Scheme

Maintained inside `ruuvi.json` on the `nvs` partition (not encrypted at rest). Secret values are
stripped by application filters from network request routing pipelines and are entirely omitted from
console debug outputs. The background diagnostic logging task manages the isolated runtime memory
lifecycle of the private key.

#### Provisioning Mechanism

Initialized to factory default endpoints targeting manufacturer cloud backends (
`https://network.ruuvi.com/status`), but dynamically adjustable by administrators via the advanced
data routing dashboard.

#### Secure Communication Mechanisms

`SecComMech-WebUI-Session`, `SecComMech-TLS`

#### Generation Mechanism

N/A

---

### **ID**: SecParam-HMAC-Symmetric-Secrets

#### Description

The dynamic symmetric secrets used by the outbound JSON payload generation components to calculate
message authenticity headers via `g_hmac_sha256_key_ruuvi`, `g_hmac_sha256_key_custom`, and
`g_hmac_sha256_key_stats`. They are not hard-coded identities, but their default initialization
vectors match the hardware $DEVICEID$.

#### Type

critical

#### Security Guarantees

Ensures **Confidentiality** and **Integrity**. Protects against telemetry spoofing and data
falsification attacks targeted at cloud endpoints.

#### Protection Scheme

Maintained inside static key structures within the application task context memory space. These
structures are inaccessible via standard configuration reads and are completely absent from text
logging logs.

#### Provisioning Mechanism

Initialized by default using the unique 64-bit hardware `DEVICEID` seed declared in
`SecParam-Hardware-DeviceID`. The keys can be dynamically rotated at runtime by a verified receiving
cloud infrastructure via the `Ruuvi-HMAC-KEY` response header field.

#### Secure Communication Mechanisms

`SecComMech-HMAC-Signing`, `SecComMech-TLS`

#### Generation Mechanism

Initial default generated from the hardware registers. Remote rotated variants are generated by the
cloud server infrastructure using high-entropy random number generators.

---

### **ID**: SecParam-LAN-Bearer-Tokens

#### Description

Automatically or manually generated high-entropy Bearer tokens utilized for Machine-to-Machine (M2M)
interaction over the local network interface, separating access into read-only (`lan_auth_api_key`)
or full read/write (`lan_auth_api_key_rw`) privileges. They are not hard-coded in the source code.

#### Type

critical

#### Security Guarantees

Ensures **Access Control**. Prevents unauthorized automated system modifications or data scraping by
restricting API execution to clients presenting a structurally valid token signature vector.

#### Protection Scheme

Stored inside the configuration file (`ruuvi.json`) within the device non-volatile flash partition (
`nvs`) not encrypted at rest. Because these tokens are passed via standard, unencrypted HTTP
`Authorization` headers over Port 80, they do not inherit the application-layer encryption used by
the interactive Web-UI. Their protection relies entirely on the isolation of the local network
architecture. Tokens are hidden from regular configuration outputs and never dumped to the local
serial log stream.

#### Provisioning Mechanism

The tokens are initialized automatically by the Web-UI platform configuration loop, but the device
administrator has full authority to modify, clear, or explicitly provision any arbitrary custom
value into the fields via the configuration dashboard. If an entry is left empty, its corresponding
M2M authentication endpoint domain remains entirely disabled.

#### Secure Communication Mechanisms

N/A (The parameter is transmitted in cleartext via standard HTTP headers over remotely accessible
local network interfaces).

#### Generation Mechanism

Automatically generated on the user's browser client via secure pseudo-random structures using the
following cryptographic pipeline:
`crypto.enc.Base64.stringify(crypto.SHA256(crypto.lib.WordArray.random(32)))`.
This generates a high-entropy 256-bit token string that is completely unique per generation event.

---

## Summary Matrix for the Technical File

| Parameter ID                              | Cryptographic Type |     Persistent Storage Type     | Access Privileges / Roles                                                                                              |
|:------------------------------------------|:------------------:|:-------------------------------:|:-----------------------------------------------------------------------------------------------------------------------|
| `SecParam-FW-Verification-Key`            |       public       |     Application Flash Text      | Read: `fw_update.c` Verification Loop<br>Modify: Physical Flash Device Only                                            |
| `SecParam-Main-Firmware-Signature`        |       public       |      tail of OTA partition      | Read: `esp_ota_end_patched`<br>Modify: Complete System OTA Update                                                      |
| `SecParam-WebUI-Partition-Signature`      |       public       |     Application Flash Blobs     | Read: Boot Verification Context<br>Modify: Complete System OTA Update                                                  |
| `SecParam-nRF52-Partition-Signature`      |       public       |     Application Flash Blobs     | Read: Boot Verification Context<br>Modify: Complete System OTA Update                                                  |
| `SecParam-CoProcessor-Verification-Stub`  |       public       |     Application Flash Blobs     | Read: SWD Boot-Time Calculation Launcher<br>Modify: Complete System OTA Update                                         |
| `SecParam-Hardware-DeviceID`              |      critical      |     Hardware Silicon (FICR)     | Read: Diagnostic Logs / Internal Tasks<br>Modify: Immutable (Direct Default Password & HMAC Root Key)                  |
| `SecParam-LAN-WebUI-Credentials`          |      critical      | `ruuvi.json` on `nvs` Partition | Read: Internal Auth Handlers Only (Masked on Web-UI reads)<br>Modify: Authenticated Administrator (Stored as MD5 hash) |
| `SecParam-WiFi-STA-Credentials`           |      critical      | `ruuvi.json` on `nvs` Partition | Read: Wi-Fi Stack Driver Initialization (Masked on Web-UI reads)<br>Modify: Provisioning Wizard / Admin Session        |
| `SecParam-Remote-Config-Assets`           |       mixed        | `ruuvi.json` / `gw_cfg_storage` | Read: Outbound Remote Sync Core Tasks<br>Modify: Authenticated Administrator                                           |
| `SecParam-Custom-HTTP-Telemetry-Assets`   |       mixed        | `ruuvi.json` / `gw_cfg_storage` | Read: HTTP Post Application Tasks<br>Modify: Authenticated Administrator                                               |
| `SecParam-Custom-Stream-Telemetry-Assets` |       mixed        | `ruuvi.json` / `gw_cfg_storage` | Read: MQTT Driver Task Lifecycle<br>Modify: Authenticated Administrator                                                |
| `SecParam-System-Statistics-Assets`       |       mixed        | `ruuvi.json` on `nvs` Partition | Read: Background Diagnostic Log Loop<br>Modify: Authenticated Administrator                                            |
| `SecParam-HMAC-Symmetric-Secrets`         |      critical      |     Static App Task Memory      | Read: `hmac_sha256.c` Hashing Contexts<br>Modify: Remote Cloud Server Rotation Header                                  |
| `SecParam-LAN-Bearer-Tokens`              |      critical      | `ruuvi.json` on `nvs` Partition | Read: Local API Validation Guards<br>Modify: Authenticated Administrator                                               |
