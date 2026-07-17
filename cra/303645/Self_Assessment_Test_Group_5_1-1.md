# Test group 5.1-1: No universal default passwords

## Test case 5.1-1-1 (conceptual)

**Purpose**: To conceptually assess whether all password-based and machine-to-machine authentication
mechanisms utilize pre-installed secrets that are unique per device, or are properly defined by the
user.

| IXIT Entry ID                        | Description / Context              | Authenticator Category  | User-Defined? |   Unique Per Device?    | Case Verdict |
|:-------------------------------------|:-----------------------------------|:-----------------------:|:-------------:|:-----------------------:|:------------:|
| `AuthMech-Hotspot-Provisioning`      | Wi-Fi Onboarding Hotspot           |  None (Open Interface)  |      N/A      |           N/A           |   **PASS**   |
| `AuthMech-LAN-WebUI-Default`         | LAN Configuration Web-UI           |     Password-Based      |      No       | Yes (`DEVICEID` Mapped) |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined`    | Custom Administrative Login        |     Password-Based      |      Yes      |           N/A           |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic`           | Legacy Basic Auth Fallback         |     Password-Based      |      Yes      |           N/A           |   **PASS**   |
| `AuthMech-LAN-WebUI-Digest`          | Legacy Digest Auth Interface       |     Password-Based      |      Yes      |           N/A           |   **PASS**   |
| `AuthMech-LAN-WebUI-Unauthenticated` | Open LAN Management State          |  None (Open Interface)  |      N/A      |           N/A           |   **PASS**   |
| `AuthMech-LAN-WebUI-Disabled`        | Restricted Local Network Access    | None (Interface Closed) |      N/A      |           N/A           |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`         | Programmatic REST API (`/history`) |     Token-Based M2M     |      Yes      |  N/A (User-Committed)   |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`         | Programmatic Configuration Node    |     Token-Based M2M     |      Yes      |  N/A (User-Committed)   |   **PASS**   |

**Assessment**: Every credential validation mechanism deployed on the DUT in states other than the
pure factory default layout is either completely user-defined/user-committed, or relies on a
hardware-tied generation pipeline ensuring absolute uniqueness per device architecture.

**Verdict**: **PASS**

---

## Test case 5.1-1-2 (functional)

### Test Unit A: Discovery of Undocumented Mechanisms

**Purpose**: Functional validation of network-exposed interfaces using diagnostic discovery tools to
verify the completeness of the tracking documentation.
**Testing Infrastructure & Method**: Deployed full-range TCP/UDP `nmap` stealth scans across the
local Ethernet subnet loop and the active wireless onboarding access point envelope, cross-checking
against local Bluetooth Low Energy passive scanning capture setups.

| Physical Interface                 | Protocol / Target Node | Discovery Verification State | Associated Index Reference                      |
|:-----------------------------------|:-----------------------|:-----------------------------|:------------------------------------------------|
| **LAN Interface (Ethernet / STA)** | HTTP (Port 80)         | Fully Documented Profile     | `AuthMech-LAN-WebUI-Default` through `Disabled` |
| **Wireless AP (Hotspot)**          | HTTP (Port 80)         | Fully Documented Profile     | `AuthMech-Hotspot-Provisioning`                 |
| **Bluetooth LE Radio**             | Custom BLE Advertising | Receive-Only Passive Scanner | N/A (Exposes no inbound auth sockets)           |

**Assessment**: Functional network mapping confirms that zero undocumented password-based or
machine-to-machine authentication paths are exposed via the device network interfaces or listed
within operational references.

**Verdict**: **PASS**

### Test Unit B: Requirement to Set User-Defined Passwords

**Purpose**: Functional verification that the DUT explicitly prevents access to user-defined
authentication interfaces until the operator configures a non-default secret value.

| Documented IXIT Entry ID          | Credential Control Attribute | Verification Constraint Enforced by DUT                     | Test Unit Verdict |
|:----------------------------------|:-----------------------------|:------------------------------------------------------------|:-----------------:|
| `AuthMech-LAN-WebUI-User-Defined` | User-Customized Password     | Access denied until custom string is set via Web-UI         |     **PASS**      |
| `AuthMech-LAN-WebUI-Basic`        | User-Customized Password     | Endpoint disabled until explicitly activated and configured |     **PASS**      |
| `AuthMech-LAN-WebUI-Digest`       | User-Customized Password     | Endpoint disabled until explicitly activated and configured |     **PASS**      |
| `AuthMech-M2M-API-Bearer-RO`      | High-Entropy M2M Token       | Empty by default; API returns `401 Unauthorized` until set  |     **PASS**      |
| `AuthMech-M2M-API-Bearer-RW`      | High-Entropy M2M Token       | Empty by default; API returns `401 Unauthorized` until set  |     **PASS**      |

**Assessment**: For all operational authentication mechanisms categorized as user-defined or
token-based, the DUT strictly restricts programmatic or interactive access validation paths until an
active configuration entry is committed by the administrator.

**Verdict**: **PASS**

### Test Unit C: Verification of Generation Mechanisms

**Purpose**: Functional validation to confirm that pre-installed default passwords match the
documented generation rules when evaluated outside the factory default state.
**Execution Context**: Evaluated across multiple hardware production samples operating in
alternative operational environments (post-onboarding configurations, active network connection
tracks, and post-OTA firmware updating sequences).

| Target Entry ID              | Checked Operational State                  | Observed Materialization Vector                                         | Alignment Verdict |
|:-----------------------------|:-------------------------------------------|:------------------------------------------------------------------------|:-----------------:|
| `AuthMech-LAN-WebUI-Default` | Operational States (excl. Factory Default) | 16-char uppercase colon-separated string matching nRF52 FICR `DEVICEID` |     **PASS**      |

**Assessment**: Functional analysis proves that for all test scenarios, the pre-installed default
credentials match the hardware-unique `DEVICEID` registers format as declared under
`SecParam-Hardware-DeviceID`. There is zero variance between the active device parameter state and
the documented execution rules.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.1-1 of ETSI EN 303 645. The out-of-the-box primary
access barrier (`AuthMech-LAN-WebUI-Default`) uses a hardware-bound, non-universal cryptographic
string layout. All alternative machine-to-machine interfaces (`AuthMech-M2M-API-Bearer-RO` / `RW`)
and advanced custom configurations remain entirely unpopulated, disabled, or locked out by default
until explicitly configured by an authenticated system administrator.

**Group Verdict**: **PASS**
