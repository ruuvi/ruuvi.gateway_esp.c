# Test group 5.1-2: Password Generation Quality

## Test case 5.1-2-1 (conceptual)

**Purpose**: To assess whether the generation mechanism for pre-installed passwords avoids
predictable patterns, public information, and common strings while maintaining appropriate
complexity.

| IXIT Entry | Description               | Pre-installed Unique? | Unit A (Regularities) | Unit B (Common) | Unit C (Public) | Unit D (Complexity) | Verdict |
|------------|---------------------------|-----------------------|-----------------------|-----------------|-----------------|---------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | Yes                   | PASS                  | PASS            | PASS            | PASS                | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-4   | Basic Auth                | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-5   | Digest Auth               | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-6   | Open Access               | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-7   | Access Denied             | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-8   | API (Read)                | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |
| IXIT-1-9   | API (R/W)                 | No                    | N/A                   | N/A             | N/A             | N/A                 | PASS    |

**Assessment Justification (IXIT-1-2)**:
- **Unit A (Regularities)**: There is no indication that the generation mechanism induces obvious
  regularities; it utilizes a hardware-unique 64-bit identifier rather than incremental counters.
- **Unit B (Common Strings)**: There is no indication that the mechanism violates the requirement
  to be free from common strings; hexadecimal representations of 64-bit random values do not appear
  in standard password dictionaries (e.g., PwnedPasswords).
- **Unit C (Public Info)**: There is no indication that the mechanism violates the requirement
  regarding public information; the DEVICEID is an internal silicon register and is not derived from
  the MAC address or Wi-Fi SSID.
- **Unit D (Complexity)**: There is no indication that the mechanism violates complexity
  requirements; a 16-character hexadecimal string provides an entropy of 2^64, making guessing
  attacks computationally infeasible.

**Verdict**: **PASS**

## Test case 5.1-2-2 (functional)

**Purpose**: To functionally verify that the passwords found on the physical device units match the 
documented generation mechanism.

### Test Unit A

| IXIT Entry | State            | Matches IXIT Description | Verdict |
|------------|------------------|--------------------------|---------|
| IXIT-1-2   | Production Units | Yes                      | PASS    |
| All Others | N/A              | N/A                      | PASS    |

**Assessment Justification**:
Functional verification was performed by reading the DEVICEID from five (5) randomly selected
production units and comparing them to the default Web-UI password.

**Result**: There is no indication that the implementation of pre-installed password generation
mechanisms violates the requirement to adhere to the specific hardware-unique descriptions provided
in IXIT-1-2.

**Verdict**: **PASS**

## Test Evidence & References

The conceptual and functional assessments are supported by the following evidence:

1. Hardware Specification: nRF52840 Product Specification, Section 4.4.1.3 (DEVICEID).

2. Dictionary Check: Comparison of DEVICEID format against NCSC Top 100k password lists (No 
matches found).

## Group Summary

The Ruuvi Gateway fulfills the requirements of Provision 5.1-2. The pre-installed password used for
the default management interface is high-entropy, hardware-unique, and independent of any publicly
discoverable device attributes.

**Group Verdict**: **PASS**
