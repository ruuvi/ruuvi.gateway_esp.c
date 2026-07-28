# IXIT 21-PersData: Personal Data

The following declarations list all categories of metadata, environmental telemetry, and
network-layer routing footprints processed by the Device Under Test (DUT) that can be linked back to
an identifiable user, specifying their processing environments, lifecycle bounds, and consent
withdrawal metrics.

---

## Table C.21: IXIT 21-PersData (Personal Data)

### **ID**: PersData-Network-IP-Footprints

#### Description

Network-layer identifiers assigned to the gateway interfaces. This includes public egress WAN IP
addresses contained within outbound transport layer wrappers, as well as local network station IP
addresses (`wifi_sta_config.ip` / `eth_config.ip`) assigned automatically via DHCP or provisioned
manually by an administrator as a static IP configuration inside the Web-UI layout.

#### Purpose

* Facilitates local network routing, packet switching, and socket session validation for the
  management Web-UI framework.
* Required for establishing secure outbound TLS connections to transfer sensor metadata arrays to
  remote telemetry targets.

#### Aggregation

No

#### Authorized Parties

* The local network administrator.
* Upstream internet service providers (ISPs).
* Ruuvi Innovations Oy (only when utilizing the official manufacturer cloud backend services).
* Authorized operators of third-party custom HTTP/MQTT ingestion servers configured by the user.

#### Lifecycle

Maintained dynamically within temporary system runtime memory blocks (RAM) while the interface is
active. Entries are automatically flushed from memory upon a link disconnect, DHCP lease expiration,
system crash, or manual hardware power cycle. Manually configured static IP profiles are retained
persistently inside the `ruuvi.json` configuration manifest within the `nvs` flash partition block
until deleted or rewritten.

#### Processing Activities

The IP footprint is either dynamically assigned by local network infrastructure or entered manually
as a static configuration by the system administrator via the Web-UI. This metadata is parsed in
system memory and attached to every inbound or outbound TCP/IP packet header traversing the network
interfaces.

#### Secure Communication Mechanisms

`SecComMech-TLS`, `SecComMech-WebUI-Session`.
The communication partner is an associated service when utilizing `https://network.ruuvi.com/`, and
is a non-associated service when utilizing custom destination nodes.

#### Sensitive (Yes/No)

No (Classified as standard network operational metadata under ETSI EN 303 645 definitions).

#### Obtaining Consent

Implicitly established when the user connects the device to an active Ethernet loop, permits
automatic DHCP leasing, or explicitly configures manual station network credentials within the
configuration wizard.

#### Withdrawing Consent

The user can completely withdraw consent and terminate IP processing loops by disconnecting the
physical network media or erasing station credentials via a 7-second hold of the physical
`CONFIGURE` button (`DelFunc-Hardware-Factory-Reset`), which forces a complete low-level formatting
block-erasure across the `nvs` flash partition.

#### Storing Consent

N/A (Derived dynamically from active physical connectivity and explicit configuration states).

#### Anonymization

No.

---

### **ID**: PersData-Gateway-LAN-MAC

#### Description

The physical Media Access Control (MAC) address assigned to the ESP32 network interface (Wi-Fi
Station/Ethernet).

#### Purpose

Used strictly at the local link layer (OSI Layer 2) to facilitate standard Ethernet frame switching,
packet routing, and local DHCP IP address lease reservations on the client's local area network.

#### Aggregation

No

#### Authorized Parties

* The local network administrator.
* Local network infrastructure equipment (routers, managed switches).
* *Note on Isolation:* Unlike the nRF52 radio MAC, this address is used purely for local network
  topology routing and is never packaged or transmitted to Ruuvi Cloud or custom remote telemetry
  endpoints.

#### Lifecycle

Persistent on the ESP32 Wi-Fi/Ethernet controller hardware registry cells. It is read dynamically
during interface initialization and remains visible on the local network segment as long as the
device is physically connected.

#### Processing Activities

Broadcasted locally across the physical network layer to announce the gateway's presence to local
routers, handle ARP resolution queries, and establish local socket connections for the management
Web-UI framework.

#### Secure Communication Mechanisms

N/A (Operates at the physical and link layers below the TLS/cryptographic stack).

#### Sensitive (Yes/No)

No (Standard network layer hardware interface identifier).

#### Obtaining Consent

Implicitly established when the user plugs in an Ethernet cable or provisions Wi-Fi station
credentials to bridge the gateway onto their local network.

#### Withdrawing Consent

The user can cease local MAC exposure instantly by disconnecting the physical network media or
wiping stored network credentials via the hardware factory reset routine (
`DelFunc-Hardware-Factory-Reset`).

#### Storing Consent

N/A (Derived dynamically from active local hardware link states).

#### Anonymization

No.

---

### **ID**: PersData-Hardware-DeviceID

#### Description

The unique 64-bit microcontroller hardware identifier (`SecParam-Hardware-DeviceID`) extracted
directly out of the internal Factory Information Configuration Registers (FICR) inside the nRF52811
silicon chip structure.

#### Purpose

Acts directly as the factory-default Web-UI credential password string for initial system
authentication, and functions as the baseline cryptographic root seed for calculating outbound
symmetric HMAC telemetry payload signatures.

#### Aggregation

No

#### Authorized Parties

* Ruuvi Innovations Oy (maintains corresponding cryptographic identity pairs in secure manufacturing
  registration databases).
* *Note on Isolation:* Third-party target cloud destinations, external API consumers, and local area
  network scanning utilities do not have access to, nor visibility of, this raw 64-bit hardware
  register identifier.

#### Lifecycle

Permanently burned into non-volatile, read-only factory silicon cells (FICR) during chip
fabrication. It is structurally immutable and persists for the physical lifespan of the hardware
package.

#### Processing Activities

Extracted locally from hardware registers during early system boot execution steps. It is printed
directly onto a physical serial sticker label affixed to the external gateway housing and routed to
the local diagnostic `LogIntf-USB-UART-Log-Stream` for out-of-the-box installation tracking. At
runtime, internal tasks consume it locally within isolated memory blocks to calculate cryptographic
HMAC signatures over outbound metrics streams.

#### Secure Communication Mechanisms

`SecComMech-HMAC-Signing`. The parameter itself is used to sign payloads but is never transmitted
raw across network interfaces.

#### Sensitive (Yes/No)

Yes (Acts as a factory root credential and symmetric signing seed).

#### Obtaining Consent

Implicitly given when the user deploys the gateway hardware and registers the device with signed
telemetry cloud streams.

#### Withdrawing Consent

The user can completely stop payload signing loops and cloud transmission by deactivating outbound
telemetry streams or turning off diagnostics reporting inside the settings dashboard.

#### Storing Consent

Stored locally within non-volatile parameter configurations (`ruuvi.json` on the `nvs` partition) by
preserving the user's explicit target active state selections.

#### Anonymization

No.

---

### **ID**: PersData-Gateway-MAC-Identifier

#### Description

The physical nRF52 Bluetooth Media Access Control (MAC) address assigned to the wireless radio
controller interface.

#### Purpose

Serves as the explicit, static primary system identity string used across network communications to
uniquely map and route raw environmental telemetry payloads back to a specific physical gateway
unit.

#### Aggregation

No

#### Authorized Parties

* Ruuvi Innovations Oy.
* Authorized administrators of third-party target HTTP/MQTT ingestion servers configured by the
  user.

#### Lifecycle

Persistent on the radio controller chip. It is retrieved during initialization, written dynamically
into system memory configuration blocks, and persists as a visible configuration field over the
operational lifetime of the device setup.

#### Processing Activities

During the initialization phase, the firmware reads the address from the nRF52 radio chip and writes
it directly to the active configuration profile matrix as a read-only parameter (visible when
reading configuration parameters as `"gw_mac": "XX:XX:XX:XX:XX:XX"`). This unique string is
subsequently wrapped inside outbound JSON telemetry envelopes, acting as the explicit origin header
to map incoming metrics records at the server tier.

#### Secure Communication Mechanisms

`SecComMech-TLS`. The communication partner is an associated service when utilizing the official
manufacturer backend.

#### Sensitive (Yes/No)

No (Standard network interface hardware identifier).

#### Obtaining Consent

Obtained when the user configures target endpoint records and connects the gateway to forward radio
metrics.

#### Withdrawing Consent

The consumer can cease remote MAC transmission at any time by toggling off telemetry target
destinations or disconnecting network links.

#### Storing Consent

Stored locally within non-volatile parameter partitions (`ruuvi.json` on the `nvs` partition) by
saving the user's connectivity choice.

#### Anonymization

No (The MAC must remain fixed and explicit to perform intended sensor-to-server data mapping).

---

### **ID**: PersData-Custom-Target-Access-Secrets

#### Description

User-provisioned authorization keys, basic authentication parameters, bearer tokens, and custom
private SSL cryptographic certificates utilized to facilitate high-privilege machine identity
verification and data encryption (`SecParam-Custom-HTTP-Telemetry-Assets` /
`SecParam-Custom-Stream-Telemetry-Assets` / `SecParam-Remote-Config-Assets`).

#### Purpose

Allows the gateway to securely authenticate against and push sensor telemetry packets (or pull
automated settings parameters) to a user-controlled, third-party storage node, database array,
private MQTT broker, or remote corporate orchestration server.

#### Aggregation

No

#### Authorized Parties

* The authenticated system administrator.
* The designated third-party destination server validation handler.

#### Lifecycle

Alphanumeric parameters (passwords/bearer keys) reside within the standard non-volatile
configurations file (`ruuvi.json` on the `nvs` partition). High-volume cryptographic assets, such as
user x509 PEM certificates and companion private keys, are stored as separate allocation blocks on
the expanded size NVS storage partition named `gw_cfg_def`. These items are held persistently until
cleared via the Web-UI or erased completely during a hardware factory reset loop (
`DelFunc-Hardware-Factory-Reset`).

#### Processing Activities

Entered manually by the administrator or uploaded via setup automation scripts. The text arrays and
certificates are saved inside their respective non-volatile flash partitions, loaded into secure
task memory at runtime, and attached to outbound HTTP connections or MQTT mbedTLS handshakes to
execute secure client-side cryptographic authentication checks (supporting mutual TLS/mTLS
verification rules).

#### Secure Communication Mechanisms

`SecComMech-TLS`. The communication partner is a non-associated third-party service.

#### Sensitive (Yes/No)

Yes (Classified as critical security credentials protecting private network pathways).

#### Obtaining Consent

Explicitly provided by the user when they manually enter or script the submission of the credentials
into the device configuration profile.

#### Withdrawing Consent

The consumer can withdraw consent instantly by clearing the credential fields inside the Web-UI form
or executing a physical hardware factory reset (`DelFunc-Hardware-Factory-Reset`) to format the NVS
storage partition blocks.

#### Storing Consent

Stored implicitly by keeping the credential strings saved inside the non-volatile `ruuvi.json`
partition code layer. Leaving fields blank signifies zero consent for transmission.

#### Anonymization

No.

---

### **ID**: PersData-BLE-Sensor-Telemetry

#### Description

Environmental measurements and status metrics collected passively by the gateway from nearby BLE
sensor tags (e.g., temperature, relative humidity, atmospheric pressure, acceleration, battery
voltage, movement counters, RSSI, and BLE tag MAC addresses).

#### Purpose

* Enables continuous real-time environmental monitoring, historical trends, and alerting for the
  user via the Ruuvi Cloud ecosystem or user-defined custom endpoints.
* *Personal Data Note:* While individual raw physical sensor measurements (such as temperature or
  humidity) are inherently environmental readings, when bound to a specific gateway MAC identifier (
  `gw_mac`), IP address, or user account (which often incorporates the user's name and email), the
  aggregated stream can reveal daily routines, presence, and home/facility occupancy patterns.

#### Aggregation

No

#### Authorized Parties

* Ruuvi Innovations Oy (when outbound Ruuvi Cloud telemetry relay is active).
* Authorized operators of user-configured third-party target servers (HTTP/HTTPS/MQTT/MQTTS/WS/WSS
  endpoints).
* Authenticated local network API clients holding valid M2M Bearer tokens (
  `SoftServ-Local-Programmatic-API`).

#### Lifecycle

* **On-Device:** Telemetry packages are processed in volatile system RAM queues and are discarded
  dynamically as fresh radio data arrives. Transient ring buffers are completely wiped on reboot,
  power interruption, or hardware factory reset (`DelFunc-Hardware-Factory-Reset`).
* **On Associated Services:** Retained on Ruuvi Cloud infrastructure for the duration of the active
  user subscription or account lifecycle. Data is purged upon explicit account deletion (
  `DelFunc-Service-Account-Deletion`).

#### Processing Activities

The nRF52 radio co-processor passively captures connectionless BLE advertisement packets over the
air. Received advertisement frames are passed via the internal UART link to the main ESP32
application processor, buffered in runtime memory, compiled into structured JSON/MQTT payload
blocks, and transmitted over network channels to configured telemetry targets. Communication with
the official Ruuvi Cloud backend strictly enforces encrypted channels (`HTTPS`). For user-configured
custom targets, protocol selection is determined by user configuration: custom HTTP endpoints
implicitly select secure (`HTTPS`) or cleartext (`HTTP`) transport based on the URL prefix (
`http://` or `https://`), while custom stream endpoints allow explicit administrator selection among
`MQTT` (default), `MQTTS`, `WS`, and `WSS`.

#### Secure Communication Mechanisms

`SecComMech-TLS`, `SecComMech-HMAC-Signing`, `SecComMech-LAN-Bearer-Authentication`. The
communication partner is an associated service when posting to `https://network.ruuvi.com/record`,
and a non-associated service when posting to custom user destinations. Transport-layer encryption (
`SecComMech-TLS`) is enforced for official Ruuvi Cloud communications and applies to custom targets
when secure transport schemes (`HTTPS`, `MQTTS`, `WSS`) are selected. If cleartext transport
schemes (`HTTP`, `MQTT`, `WS`) are configured, transport-layer encryption is not applied.

#### Sensitive (Yes/No)

Yes (Can be processed to infer household occupancy, activity levels, or personal routines linked to
an identified user account).

#### Obtaining Consent

Implicitly or explicitly provided when the user completes network provisioning, configures sensor
scanning rules, and enables outbound cloud or custom data targets.

#### Withdrawing Consent

The user can withdraw consent at any time on the device by toggling off telemetry relays in the
Web-UI or executing a hardware factory reset (`DelFunc-Hardware-Factory-Reset`). For cloud-stored
data, consent is withdrawn by requesting account termination (`DelFunc-Service-Account-Deletion`).

#### Storing Consent

Stored locally in non-volatile flash memory (`ruuvi.json` on `nvs`) as active telemetry target
configuration flags.

#### Anonymization

No (Telemetry payloads explicitly preserve sensor and gateway MAC addresses to permit accurate data
attribution and time-series graphing).

---

## Summary Matrix for the Technical File

| Parameter ID                            | Category of Personal Data                | Solely Aggregated? | Authorized Receivers                            | Sensitive? | Anonymization Applied? |
|:----------------------------------------|:-----------------------------------------|:------------------:|:------------------------------------------------|:----------:|:----------------------:|
| `PersData-Network-IP-Footprints`        | Network Address Metadata                 |         No         | Upstream ISPs / Configured Cloud Targets        |     No     |           No           |
| `PersData-Gateway-LAN-MAC`              | Local Network Interface MAC              |         No         | Local Network Infrastructure Only               |     No     |           No           |
| `PersData-Hardware-DeviceID`            | Unique 64-bit Hardware ID Seed           |         No         | Ruuvi Innovations Oy Only                       |    Yes     |           No           |
| `PersData-Gateway-MAC-Identifier`       | nRF52 Bluetooth MAC Address              |         No         | Cloud / Configured Endpoints                    |     No     |           No           |
| `PersData-Custom-Target-Access-Secrets` | Private API Keys, Passwords & mTLS Certs |         No         | Configured Ingestion/Orchestration Targets      |    Yes     |           No           |
| `PersData-BLE-Sensor-Telemetry`         | Environmental Metrics & BLE Payloads     |         No         | Ruuvi Cloud / Custom User Endpoints / Local API |    Yes     |           No           |
