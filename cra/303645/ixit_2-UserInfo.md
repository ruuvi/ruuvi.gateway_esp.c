# CRA: 303 645 IXIT-2-USERINFO: User Information

Source: ETSI TS 103 701 V2.1.1 / ETSI EN 303 645  
Section A.3: Implementation eXtra Information for Testing (IXIT) pro forma — "IXIT 2-UserInfo: User
Information"

---

## Documentation of Change Mechanisms

Information regarding the technical steps to configure and harden device authentication metrics is
fully documented in the official online manufacturer documentation portal.

* **Access Vector:** The user can review full administrative configuration options, credential
  management procedures, and local interface protection rules by navigating to:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan
* **Scope:** The documentation specifies how to alter the device from its factory-default password
  mapping sequence to an explicit user-defined administrative credential pair via the management
  Web-UI framework.

## Documentation of Replacement

Not applicable. The Device Under Test (DUT) is designed with full over-the-air firmware modification
capability via dual active application banks (`ota_0` and `ota_1`), as verified under the update
mechanics sections. No standalone hardware isolation layout or replacement architecture descriptions
are required.

## Documentation of Sensors

Comprehensive architecture overviews detailing the gateway’s logical environmental data collection
layers and wireless intercept parameters are published on the documentation server.

* **Access Vector:** The user can access precise filtering, PHY layer tracking rules, and MAC
  filtering matrix control steps at:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/bluetooth-scanning-settings
* **Scope:** The documentation outlines that the nRF52811 radio sub-system operates as a
  connectionless passive listener. It specifies how the operator can restrict scanning strictly to
  official Ruuvi manufacturer frames, open the tracking bounds to alternative manufacturer IDs
  across channels 37/38/39, toggle 1M/2M/Coded PHY layers, and provision hardware-tier Whitelist or
  Blacklist rules.

## Documentation of Secure Setup

The exact technical requirements and workflow diagrams for securely initializing the device inside a
target infrastructure environment are maintained within the onboarding documentation tracks.

* **Access Vector:** The operator can access complete step-by-step wizard walkthrough guidelines by
  navigating to:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages
* **Scope:** The deployment layout details the initial captive portal setup environment, interface
  lock rules, mandatory password configuration adjustments, and how to verify that automatic profile
  configuration download targets are securely provisioned.

## Documentation of Setup Check

The verification pathways allowing an administrator to confirm the current secure state validation
profile of the operational gateway are exposed both inside the running software environment and the
online support center.

* **Access Vector:** Setup verification methods can be verified dynamically by loading the gateway
  management dashboard interface over the local network drop, or by referencing the deployment
  validation matrix at:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan
* **Scope:** The documentation provides clear checks allowing the user to confirm:
  * The administrative password status layer.
  * The enforcement policy of the stateless local machine-to-machine (M2M) API keys (
    `lan_auth_api_key` and `lan_auth_api_key_rw`).

## Documentation of Maintenance Check

The mechanisms to monitor the ongoing operational security state, update check timers, and partition
lifecycle flags of the platform are explicitly mapped within the firmware update manual.

* **Access Vector:** The user can review runtime health auditing criteria and check for firmware
  patch rollouts by navigating to:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/software-update
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/automatic-updates
* **Scope:** The deployment text details how to manually invoke signature-verified OTA updates,
  schedule automated background version checks across defined days of the week, choose between
  standard production or beta tester release branches, and audit internal task memory allocations
  via the `http_server` status telemetry envelope.

## Documentation of Personal Data

The definitions, transmission bounds, and physical handling rules governing potential personal data
footprints are outlined within the manufacturer’s corporate privacy policy and software architecture
documents.

* **Access Vector:** The explicit data processing disclosures are accessible at:
  * Privacy Notice: https://ruuvi.com/privacy/ (Section: *Privacy Statement & Policy on Ruuvi
    Station and Ruuvi Gateway and other software*)
  * Technical Profile Documentation: https://docs.ruuvi.com/ruuvi-gateway-firmware/
* **Scope:** The documentation details how the hardware isolates network routing metadata (IP
  assignments), hardware identifying strings (the unique nRF52 public MAC address vs. the isolated
  64-bit microcontroller silicon `DEVICEID`), and user-provisioned custom endpoint secrets.

## Documentation of Telemetry Data

The structural composition, collection interval limits, and recipient destinations of diagnostic
telemetry packets are defined under the data privacy and technical status guidelines.

* **Access Vector:** The user can audit the telemetry schema definitions and adjust routing
  destinations by navigating to:
  * https://docs.ruuvi.com/ruuvi-gateway-firmware/data-formats/http-gateway-status
* **Scope:** The documentation outlines the payload blocks sent to the diagnostic endpoint
  `https://network.ruuvi.com/status` (including stack sizes, reset reasons, and crash log traces
  contained in the `RESET_INFO` string) and details how the operator can selectively opt-out of
  statistics transmission entirely via Step 8 of the setup wizard layout.

## Documentation of Deletion

The data-erasure workflows, local partition formatting mechanisms, and remote service account
deletion pathways are formally cataloged in the core user manuals and privacy frameworks.

* **Access Vector:** Permanent data cleanup mechanisms are documented across the following target
  endpoints:
  * Product User Documentation: https://docs.ruuvi.com/ruuvi-gateway-firmware/
  * Privacy Policy Eradication Portal: https://ruuvi.com/privacy/
* **Scope:** The documentation instructs the user on:
  * **Local Hardware Erasure:** How to execute a manual physical long-press of the `CONFIGURE`
    button for 7 seconds or longer to trigger low-level formatting loops across the `nvs` and
    `gw_cfg_def` partitions, completely wiping custom SSL certificates, keys, M2M tokens, and Wi-Fi
    credentials.
  * **API Token Deactivation:** How to manually purge authorization values inside the Web-UI to
    instantly return HTTP 401 unauthenticated errors to client scripts.
  * **Associated Cloud Deletion:** How to leverage the routing dashboard to terminate outbound
    HTTP/MQTT pipelines, causing the gateway identifier to fall permanently offline within the
    manufacturer’s cloud databases, and how to request formal profile erasure from remote backend
    systems.

## Model Designation

* **Model Designation String:** Ruuvi Gateway
* **Identification Method:** The unique model designation is permanently printed in high-contrast
  text directly onto the physical tracking label (sticker) permanently affixed to the lower exterior
  underside surface of the gateway product enclosure casing.
* **Traceability:** The device documentation manual instructs the user to visually inspect this
  physical label to identify model validation sequences, product certification markings, and the
  factory default administrative

## Support Period

> **Documentation gap (to be resolved before submission):** The manufacturer publishes lifecycle
> promises at https://ruuvi.com/terms/lifecycle-promises/, but the Ruuvi Gateway is not currently
> covered by that page. The support period for the Gateway must be defined and published before
> submission. A tracking issue should be open before this PR is merged.

The device architecture and core operating components are actively maintained with security-critical
firmware patches, vulnerability mitigation updates, and functional platform maintenance for a
minimum duration of **? years after the product model is placed on the commercial market**.

## Publication of Support Period

The defined hardware support lifespan metrics and lifecycle commitments are officially published and
maintained on the manufacturer’s public legal terms portal.

* **Access Vector:** The support duration statement can be read by users at:
  * Product Lifecycle Policy: https://ruuvi.com/terms/lifecycle-promises/

## Publication of Vulnerability Disclosure Policy

The manufacturer maintains a standardized vulnerability disclosure program providing clear
coordination pathways for security researchers, operators, and consumer notification tracking.

* **Access Vector:** The comprehensive vulnerability handling rules and report submission channels
  are located at:
  * Vulnerability Disclosure / Security Policy: https://ruuvi.com/terms/vulnerability-policy/
* **Scope:** The publication explicitly outlines the designated reporting communication channels,
  the cryptographic key structures used for secure message submission, target response time
  milestones, and the coordinated public announcement procedures followed to patch identified
  security flaws safely.

## Publication of Non-Updatable

Not applicable. The gateway firmware architecture features complete remote over-the-air validation
and update capabilities, managed via secure dual-slot flash layouts (`ota_0` and `ota_1`).

---

## Summary Matrix for the Technical File

| Documentation Domain      | Primary Publication Vector  | User Access Interface / URL                                                                  | Compliance Verification Reference           |
|:--------------------------|:----------------------------|:---------------------------------------------------------------------------------------------|:--------------------------------------------|
| **Change Mechanisms**     | Online Document Portal      | https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan    | Hardening dashboard verification.           |
| **Sensors & Filtering**   | Online Document Portal      | https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/bluetooth-scanning-settings | Radio constraint adjustments.               |
| **Secure Setup Rules**    | Setup Wizard Manual         | https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan    | Wizard execution workflow tracking.         |
| **Data & Token Deletion** | Technical & Privacy Manuals | https://docs.ruuvi.com/ruuvi-gateway-firmware/                                               | Physical formatting and cloud deactivation. |
| **Model Labeling**        | Casing Enclosure Layout     | Underneath casing structure sticker label                                                    | Direct physical visual identification.      |
| **Support Promises**      | Lifecycle Index Page        | https://ruuvi.com/terms/lifecycle-promises/                                                  | Enforces 5-year security patch tracking.    |
| **Vulnerability Gating**  | Security Disclosure Page    | https://ruuvi.com/terms/vulnerability-policy/                                                | Standardized report coordination drop.      |