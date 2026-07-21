# IXIT 13-SoftServ: Software Services

The following declarations map the complete software service profile of the Ruuvi Gateway (DUT),
specifying their activation status, core functional necessity, network interface accessibility
parameters, configuration privileges, and associated inbound authentication mechanisms.

---

## Table C.13: IXIT 13-SoftServ (Software Services)

### **ID**: SoftServ-Hotspot-Orchestration

#### Description

A composite local daemon grouping an internal HTTP Server, a local DNS redirection server, and a
local DHCP server. It runs strictly over the isolated wireless access point interface to host the
transient setup wizard configuration panel. The service is accessible via a network interface but is
**not** accessible in the standard initialized operational state.

#### Status

Active on-demand (Transient Initial State).

#### Justification

Necessary to allow wireless provisioning clients to temporarily connect to the unconfigured gateway,
automatically obtain a dynamic IP address lease, receive captive portal DNS redirects, and complete
the setup sequence.

#### Allows Configuration (Yes/No)

Yes. The user can commit initial operating states including station network credentials, custom API
token parameters, and data routing profiles written straight to `ruuvi.json` on the `nvs` flash
partition.

#### Authentication Mechanism

`AuthMech-Hotspot-Provisioning` (Surfaced via the localized open wireless hotspot medium).

---

### **ID**: SoftServ-WiFi-WPS-Onboarding

#### Description

An on-demand Wi-Fi Protected Setup (WPS) client helper sub-service embedded within the internal
wireless stack. The service is accessible via a network interface but is **not** accessible in the
standard initialized operational state, requiring explicit user initiation within the setup wizard
context.

#### Status

Active on-demand (Transient task restricted entirely to the hotspot configuration lifecycle).

#### Justification

Provides an alternative, human-error-resistant onboarding option allowing deployment teams to
quickly pair the gateway with local infrastructure routers without manually inputting complex
credentials.

#### Allows Configuration (Yes/No)

Yes. Upon a successful push-button pairing cycle, the service automatically captures and writes the
target station network credentials to the device parameters file.

#### Authentication Mechanism

None. Enforced via out-of-band physical proximity access constraints on the target wireless Access
Point.

---

### **ID**: SoftServ-Local-Management-WebUI

#### Description

Internal HTTP server running on standard network Port 80, providing local area network
administrative access to the device management dashboard, system diagnostics views, and
configuration mutation endpoints. The service is accessible via a network interface and is active in
the initialized state.

#### Status

Enabled

#### Justification

Necessary to provide the device administrator with continuous local subnet access to monitor
operational metrics, adjust cloud targets, manage M2M bearer tokens, or upload custom SSL
certificates.

#### Allows Configuration (Yes/No)

Yes. Enables full administrative changes to critical parameters, including Web-UI credentials, LAN
Bearer API tokens, SSL certificates, and scanning layer PHY criteria blocks.

#### Authentication Mechanism

Cross-referenced to the validation matrices managed under `SecComMech-WebUI-Session`.

---

### **ID**: SoftServ-Local-Programmatic-API

#### Description

Stateless REST API endpoints handled by the internal web engine over the local network layer (Port
80). This includes the `/history` endpoint returning a dynamic JSON array containing buffered BLE
advertisement metrics, and the `/ruuvi.json` endpoint mapping configuration data blocks. The service
is accessible via a network interface and is active in the initialized state.

#### Status

Enabled

#### Justification

Enforces machine-to-machine (M2M) connectivity capabilities, enabling home automation hubs or local
industrial controllers to programmatically query a consolidated snapshot of environmental metrics or
deploy automated parameter profiles.

#### Allows Configuration (Yes/No)

Yes. Write-access endpoints accept schema updates to modify runtime parameters inside `ruuvi.json`,
provided a valid high-privilege bearer token payload is supplied.

#### Authentication Mechanism

Cross-referenced to the privilege parameters managed under `SecComMech-LAN-Bearer-Authentication`.

---

### **ID**: SoftServ-CoProcessor-Verification-Loop

#### Description

An internal background multi-chip monitoring and verification service loop that handles early-stage
hardware integrity tracking. It runs strictly point-to-point via the isolated link-layer interface
and cannot be queried, intercepted, or addressed by inbound network sockets.

#### Status

Active (Triggered automatically during every boot execution lifecycle and post-OTA flashing
sequences).

#### Justification

Essential anti-tamper security mechanism required to cryptographically verify the co-processor's
memory blocks before functional tasks are brought up. This structural check prevents persistent
side-loading vectors or out-of-band code injection attacks on the Bluetooth radio layer.

#### Allows Configuration (Yes/No)

No. (The validation rules, registers, curve allocations, and signature arrays are immutable
properties handled within the core image code segment).

#### Authentication Mechanism

N/A (Managed via direct bare-metal host control loops executing across `LogIntf-Internal-SWD-Bus`).

---

### **ID**: SoftServ-Centralized-Remote-Config

#### Description

An outbound client synchronization loop that periodically polls a designated remote corporate
management server to check for and fetch automated gateway operational configuration manifests. The
service is **not** accessible via an inbound network interface.

#### Status

Disabled by default (Activated explicitly by user selection).

#### Justification

Enables centralized enterprise orchestration, allowing large-scale fleet deployments to pull uniform
operating behaviors from a secure remote endpoint.

#### Allows Configuration (Yes/No)

Yes. Ingests and processes remote configuration JSON manifests from the corporate server to modify
device parameters in `ruuvi.json`. Mutual TLS (mTLS) credentials (`SecParam-Remote-Config-Assets`)
are configured locally via the Web-UI or API.

#### Authentication Mechanism

N/A (Outbound client task loop only).

---

### **ID**: SoftServ-Telemetry-Relay-RuuviCloud

#### Description

A dedicated outbound HTTPS data client packaging accumulated BLE sensor payloads into compressed
JSON arrays and streaming them to the official manufacturer cloud backend framework (
`https://network.ruuvi.com/record`). The service is **not** accessible via an inbound network
interface.

#### Status

Enabled (Active by default in the initialized state).

#### Justification

Fulfills the primary out-of-the-box functionality of the gateway, allowing immediate data
visualization and alert tracking via the official Ruuvi Cloud ecosystem without requiring custom
database setups. Underlying transport loops employ stateless non-blocking asynchronous timers with a
cloud-controlled 60-second cadence via `X-Ruuvi-Gateway-Rate` during successful operation and a
fixed 67-second retry period during link degradation, ensuring task isolation as declared under
`ResMech-Net-Telemetry-Protocol-Reconnection`. A one-hour network watchdog provides last-resort
recovery if all configured telemetry paths remain unable to refresh successful network activity, as
declared under `ResMech-Net-Watchdog-Recovery`.

#### Allows Configuration (Yes/No)

No. Outbound client data pipeline only. Receiver response headers (such as `X-Ruuvi-Gateway-Rate`)
dynamically throttle runtime memory posting intervals (1–3600s), but do not modify persistent
security configurations or device settings.

#### Authentication Mechanism

N/A (Outbound client service loop; access protection on the receiving cloud server side is handled
exclusively by the default dynamic `Ruuvi-HMAC-SHA256` token arrays calculated via
`SecComMech-HMAC-Signing`).

---

### **ID**: SoftServ-Telemetry-Relay-CustomHTTP

#### Description

An independent outbound telemetry data client loop packaging accumulated BLE advertisement sensor
payloads into standardized JSON matrices and posting them to an arbitrary user-specified remote or
local HTTP/HTTPS destination URL. The service is **not** accessible via an inbound network
interface.

#### Status

Disabled by default (Activated explicitly by the administrator via the Web-UI panel).

#### Justification

Provides flexibility for corporate or private deployments, allowing sensor metrics to be natively
integrated directly into custom data warehouses or third-party analytical platforms. Underlying
transport loops employ stateless non-blocking asynchronous timers with a fixed 67-second backoff
retry period during link degradation, ensuring task isolation as declared under
`ResMech-Net-Telemetry-Protocol-Reconnection`. The same one-hour network watchdog provides
last-resort recovery if all configured telemetry paths remain unable to refresh successful network
activity, as declared under `ResMech-Net-Watchdog-Recovery`.

#### Allows Configuration (Yes/No)

No. Outbound client data pipeline only. Receiver response headers (such as `X-Ruuvi-Gateway-Rate`)
dynamically throttle runtime memory posting intervals (1–3600s), but do not modify persistent
security configurations or device settings.

#### Authentication Mechanism

N/A (Outbound client service loop; access protection on the receiving server side is handled by
user-configured basic auth secrets, bearer tokens, or verified SSL client certificates).

---

### **ID**: SoftServ-Telemetry-Relay-MQTT

#### Description

An outbound streaming message client designed to establish long-lived connections over MQTT, MQTTS,
WS, or WSS to publish real-time BLE scanning payloads directly to user brokers. The service is **not
** accessible via an inbound network interface.

#### Status

Disabled by default (Activated explicitly by the administrator via the Web-UI panel).

#### Justification

Provides real-time event-driven data streaming for industrial automation setups where polling
latencies are unacceptable. Utilizes stateful socket monitor loops that implement an automated
10-second background reconnect cadence (`disable_auto_reconnect = false`) as detailed under
`ResMech-Net-Telemetry-Protocol-Reconnection`, with the one-hour network watchdog acting as a
last-resort recovery path if all configured telemetry paths remain unreachable.

#### Allows Configuration (Yes/No)

No. Outbound streaming client pipeline only; does not accept inbound connections or allow remote
device parameter modification.

#### Authentication Mechanism

N/A (Outbound client service loop).

---

### **ID**: SoftServ-System-Diagnostics-Reporting

#### Description

An outbound client heartbeat module that periodically compiles memory statistics, connection uptime
metrics, and general diagnostic logs, transferring them via HTTPS to target collection frameworks (
`https://network.ruuvi.com/status`). The service is **not** accessible via an inbound network
interface.

#### Status

Enabled (Active by default in the initialized state).

#### Justification

Required to report operational status metrics to health monitoring dashboards for preventive
maintenance and fleet health tracking.

#### Allows Configuration (Yes/No)

No. Outbound status stream only; destination parameters are fixed and no remote configuration inputs
are parsed.

#### Authentication Mechanism

N/A (Outbound client service loop).

---

### **ID**: SoftServ-Network-Infrastructure-Clients

#### Description

A suite of standard background core network helper utilities acting as clients within the ESP-IDF
TCP/IP framework, encapsulating a DNS client resolver (UDP Port 53), a DHCP client manager (UDP
Ports 67/68), and a Simple NTP time client (UDP Port 123). The services are **not** accessible via
an inbound network interface.

#### Status

Enabled

#### Justification

Essential network infrastructure components required to automatically assign runtime IP
configurations, resolve target domain URLs into valid IP routes, and maintain reliable system
wall-clock synchronization critical for valid TLS handshake validation checks.

#### Allows Configuration (Yes/No)

No.

#### Authentication Mechanism

N/A

---

### **ID**: SoftServ-OTA-Firmware-Updater

#### Description

An outbound firmware-update client that downloads release metadata and signed firmware images over
HTTPS from the configured update server (`https://network.ruuvi.com/firmwareupdate`) and applies
them to the inactive application slots. The full mechanism is described in `IXIT 7-UpdMech` (
`UpdMech-WebUI` / `UpdMech-Auto`). The service is **not** accessible via an inbound network
interface.

#### Status

Enabled (Active by default in the initialized state).

#### Justification

Required to keep the device firmware current and to deliver security fixes.

#### Allows Configuration (Yes/No)

No. While the update policy parameters (e.g., target branch, auto-update schedule) are configured
locally by an administrator via the Web-UI or API, the updater client routine itself does not expose
an inbound interface to configure device parameters.

#### Authentication Mechanism

N/A (Outbound client service loop; downloaded images are verified by RSA-3072-PSS signatures — see
`SecComMech-Firmware-Signature-Verification`).

---

## Summary Matrix for the Technical File

| Service ID                                | Directionality | Initial Status | Accessible via Network? | Allows Security Config? | Inbound Authentication Hook Reference   |
|:------------------------------------------|:--------------:|:--------------:|:-----------------------:|:-----------------------:|:----------------------------------------|
| `SoftServ-Hotspot-Orchestration`          |    Inbound     |   Transient    | Yes (Wireless Local AP) |           Yes           | `AuthMech-Hotspot-Provisioning`         |
| `SoftServ-WiFi-WPS-Onboarding`            |    Inbound     |   Transient    | Yes (Wireless Local AP) |           Yes           | None (Out-of-Band Physical Proximity)   |
| `SoftServ-Local-Management-WebUI`         |    Inbound     |    Enabled     |    Yes (LAN Port 80)    |           Yes           | `SecComMech-WebUI-Session`              |
| `SoftServ-Local-Programmatic-API`         |    Inbound     |    Enabled     |    Yes (LAN Port 80)    |           Yes           | `SecComMech-LAN-Bearer-Authentication`  |
| `SoftServ-CoProcessor-Verification-Loop`  | Internal Loop  |    Enabled     |           No            |           No            | None (Hardware Inter-Chip Control Link) |
| `SoftServ-Centralized-Remote-Config`      |    Outbound    |    Disabled    |           No            | Yes (Remote Manifests)  | N/A                                     |
| `SoftServ-Telemetry-Relay-RuuviCloud`     |    Outbound    |    Enabled     |           No            |           No            | N/A                                     |
| `SoftServ-Telemetry-Relay-CustomHTTP`     |    Outbound    |    Disabled    |           No            |           No            | N/A                                     |
| `SoftServ-Telemetry-Relay-MQTT`           |    Outbound    |    Disabled    |           No            |           No            | N/A                                     |
| `SoftServ-System-Diagnostics-Reporting`   |    Outbound    |    Enabled     |           No            |           No            | N/A                                     |
| `SoftServ-Network-Infrastructure-Clients` |    Outbound    |    Enabled     |           No            |           No            | N/A                                     |
| `SoftServ-OTA-Firmware-Updater`           |    Outbound    |    Enabled     |           No            |           No            | N/A                                     |
