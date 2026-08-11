# Test group 5.1-2: Password Generation Quality

## Test case 5.1-2-1 (conceptual)

**Purpose**: To assess whether the generation mechanism for pre-installed passwords avoids
predictable patterns, public information, and common strings while maintaining appropriate
cryptographic complexity.

| IXIT Entry ID                        | Description / Context              | Pre-installed Unique? | Unit A (Regularities) | Unit B (Common) | Unit C (Public) | Unit D (Complexity) | Case Verdict |
|:-------------------------------------|:-----------------------------------|:---------------------:|:---------------------:|:---------------:|:---------------:|:-------------------:|:------------:|
| `AuthMech-Hotspot-Provisioning`      | Wi-Fi Onboarding Hotspot           |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-LAN-WebUI-Default`         | LAN Web-UI (Default)               |          Yes          |         PASS          |      PASS       |      PASS       |        PASS         |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined`    | Custom Administrative Login        |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic`           | Legacy Basic Auth Fallback         |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-LAN-WebUI-Digest`          | Legacy Digest Auth Interface       |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-LAN-WebUI-Unauthenticated` | Open LAN Management State          |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-LAN-WebUI-Disabled`        | Restricted Local Network Access    |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`         | Programmatic REST API (`/history`) |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`         | Programmatic Configuration Node    |          No           |          N/A          |       N/A       |       N/A       |         N/A         |   **PASS**   |

**Assessment Justification (for `AuthMech-LAN-WebUI-Default`):**

* **Unit A (Regularities):** The generation mechanism does not induce regularities. It avoids
  incremental sequencing or predictable patterns (such as "password123") by relying on a
  hardware-unique 64-bit variable layout.
* **Unit B (Common Strings):** The mechanism does not utilize common strings. Hexadecimal
  representations of high-entropy 64-bit silicon constants do not match dictionary configurations or
  common credential databases (such as the NCSC PwnedPasswords corpus).
* **Unit C (Public Information):** The mechanism is completely independent of public identifiers.
  The `DEVICEID` is pulled from internal non-volatile registers on the silicon die (FICR) and is
  entirely distinct from public network layers like the Wi-Fi station MAC or Ethernet interface
  link-layer addresses.
* **Unit D (Complexity):** The password layout provides appropriate cryptographic complexity. The
  16-character uppercase colon-separated string maps an entropy pool of $2^{64}$ permutations,
  rendering manual guessing or distributed brute-force dictionary exploits computationally
  infeasible.

**Verdict**: **PASS**

---

## Test case 5.1-2-2 (functional)

**Purpose**: To functionally verify that the passwords found on the physical device units match the
documented generation mechanism.

### Test Unit A

| Target Entry ID              | Checked State           | Materialized Syntax Conformity                                   | Alignment Verdict |
|:-----------------------------|:------------------------|:-----------------------------------------------------------------|:-----------------:|
| `AuthMech-LAN-WebUI-Default` | Active Production Units | 16-character hex block string formatted as uppercase colon pairs |     **PASS**      |
| **All Other Mechanisms**     | Configured States       | N/A (User-configured or open interfaces)                         |     **PASS**      |

**Assessment Justification:**
Functional verification was executed by reading the hardware register configuration variables
directly out of five (5) randomly selected gateway factory units via the local debugging console
link. Each sample returned a unique 16-character string matching the syntax topology of
`AA:BB:CC:DD:EE:FF:00:11`.

There is zero indication that the deployed firmware runtime environment deviates from the generation
rule parameters detailed in `AuthMech-LAN-WebUI-Default`.

**Verdict**: **PASS**

---

## Test Evidence & References

The conceptual and functional assessments are verified by the following evidence maps:

1. **Hardware Register Mapping:** Nordic Semiconductor nRF52811 Product Specification, Section
   4.4.1.3 (Factory Information Configuration Registers - `DEVICEID[0]` and `DEVICEID[1]`).
2. **Dictionary Cross-Check:** Automated comparison script validating the `DEVICEID` string
   composition against the NCSC Top 100k password database corpus (zero collisions identified).

---

## Group Summary

The Ruuvi Gateway complies fully with Provision 5.1-2 of ETSI EN 303 645. The pre-installed default
administrative access password is a high-entropy, hardware-unique string that remains entirely
unlinked from any discoverable network metadata layer.

**Group Verdict**: **PASS**
