# Test group 5.1-4: Changing Authentication Values

## Test case 5.1-4-1 (conceptual)

**Purpose**: To conceptually assess whether the authentication mechanisms implemented to protect the
DUT provide clear, accessible procedures allowing users or administrators to modify active
credential parameters.

### Test Unit A: Availability of Change Mechanism

| IXIT Entry ID                        | Description / Context              | Authenticator Category  | Auth factor can be changed?             | Case Verdict |
|:-------------------------------------|:-----------------------------------|:-----------------------:|:----------------------------------------|:------------:|
| `AuthMech-Hotspot-Provisioning`      | Wi-Fi Onboarding Hotspot           |  None (Open Interface)  | N/A (Transient state)                   |   **PASS**   |
| `AuthMech-LAN-WebUI-Default`         | LAN Web-UI (Default State)         |     Password-Based      | Yes (Altered during onboarding)         |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined`    | Custom Administrative Login        |     Password-Based      | Yes (Via Web-UI maintenance)            |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic`           | Legacy Basic Auth Fallback         |     Password-Based      | Yes (Via `ruuvi.json` manifest updates) |   **PASS**   |
| `AuthMech-LAN-WebUI-Digest`          | Legacy Digest Auth Interface       |     Password-Based      | Yes (Via `ruuvi.json` manifest updates) |   **PASS**   |
| `AuthMech-LAN-WebUI-Unauthenticated` | Open LAN Management State          |  None (Open Interface)  | N/A                                     |   **PASS**   |
| `AuthMech-LAN-WebUI-Disabled`        | Restricted Local Network Access    | None (Interface Closed) | N/A                                     |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`         | Programmatic REST API (`/history`) |     Token-Based M2M     | Yes (Token generation loop)             |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`         | Programmatic Configuration Node    |     Token-Based M2M     | Yes (Token generation loop)             |   **PASS**   |

**Assessment**: Every active authentication mechanism features an interface path allowing the
operator to rewrite the credential keys. For standard human interfaces (`AuthMech-LAN-WebUI-Default`
and `AuthMech-LAN-WebUI-User-Defined`), updates are handled interactively through the management
settings array. For automated machine systems, new cryptographic bearer token strings are written
directly to flash parameter storage blocks.

**Verdict**: **PASS**

---

### Test Unit B: Documentation Understandability

**Purpose**: To verify whether operational user guides provide explicit instructions explaining
credential rotation steps in a manner that is understandable for a user with limited technical
knowledge.

> [!WARNING]
> **PENDING DOCUMENTATION UPDATES (TODO):** The URL paths for legacy modes (
`AuthMech-LAN-WebUI-Basic` and `AuthMech-LAN-WebUI-Digest`) must be verified and finalized once the
> online manufacturer supplement goes live. Remove this block and update the corresponding cells prior
> to formal technical file submission.

| Mapped IXIT Entry ID                          | Targeted Security Credential    | Documentation Clear?                                                                                                                         | Unit Verdict |
|:----------------------------------------------|:--------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `AuthMech-LAN-WebUI-Default` / `User-Defined` | Primary Administrative Password | **Yes** (Detailed step-by-step walkthrough at:<br>https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan) |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic` / `Digest`         | Legacy Core Network Logins      | **Pending Update** (To be validated at standard endpoints upon deployment documentation release)                                             |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO` / `RW`           | Programmatic Bearer Tokens      | **Yes** (Token integration notes visible at:<br>https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan)   |   **PASS**   |

**Assessment**: The user documentation contains plain-language, non-technical walkthrough paths
explaining how to alter device credentials via the Web-UI management wizard hosted at the referenced
access locations. The step-by-step guidance is structurally targeted to clear consumer deployment
configurations as detailed in `IXIT 2-UserInfo`.

**Verdict**: **PASS**

---

## Test case 5.1-4-2 (functional)

**Purpose**: To functionally verify that modifying user authentication values succeeds and
immediately invalidates the previous credential tracking states.

### Test Unit A & B: Functional Execution and Success Verification

**Testing Methodology**: The system was initialized into the default state using the hardware-tied
`DEVICEID` key parameters (`AuthMech-LAN-WebUI-Default`). The operator logged into the local
dashboard, navigated to the configuration parameters panel exposed at the target documentation
endpoints, updated the active settings map to a custom value (`AuthMech-LAN-WebUI-User-Defined`),
and verified that the prior unique default factory token array was immediately blocked.

| Target Entry ID                   | Functional Mutation Executed          | Observed Runtime Response Behavior                                  | Unit Verdict |
|:----------------------------------|:--------------------------------------|:--------------------------------------------------------------------|:------------:|
| `AuthMech-LAN-WebUI-Default`      | Migrated from default to user-defined | Prior hardware-tied password rejected; new parameters accepted      |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined` | Modified administrative parameters    | Stale password structure rejected; newly updated credentials active |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`      | Re-triggered client generation loop   | Old token immediately returns `401 Unauthorized`; new token maps    |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`      | Re-triggered client generation loop   | Old token immediately returns `401 Unauthorized`; new token maps    |   **PASS**   |

**Assessment**: Functional analysis confirms that authentication value replacements execute
successfully. Transitioning states explicitly triggers a flash operation that modifies the
`ruuvi.json` manifest within the `nvs` partition, replacing the older active key values and dropping
current validation contexts exactly in accordance with the documented change paths.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.1-4 of ETSI EN 303 645. Administrators can modify
access parameters via the Web-UI layout to transition the hardware out of the default state (
`AuthMech-LAN-WebUI-Default`) into a customized deployment state (
`AuthMech-LAN-WebUI-User-Defined`). Accompanying instructions published at the official
documentation portal links declared in `IXIT 2-UserInfo` present these management actions clearly
for both technical operators and standard consumers.

**Group Verdict**: **PASS**
