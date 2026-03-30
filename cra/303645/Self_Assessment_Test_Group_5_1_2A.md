# Test group 5.1-2A: Passwords for M2M Authentication

## Test case 5.1-2A-1 (conceptual)

**Purpose**: To conceptually assess whether any mechanism declared for machine-to-machine (M2M) 
interaction uses passwords as an authentication factor.

### Test Unit A

| IXIT Entry | Description               | Used for M2M? | Auth Factor Includes Password? | Verdict |
|------------|---------------------------|---------------|--------------------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | No            | N/A                            | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | No            | N/A                            | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | No            | N/A                            | PASS    |
| IXIT-1-4   | LAN Web-UI (Basic)        | No            | N/A                            | PASS    |
| IXIT-1-5   | LAN Web-UI (Digest)       | No            | N/A                            | PASS    |
| IXIT-1-6   | LAN Web-UI (Open)         | No            | N/A                            | PASS    |
| IXIT-1-7   | LAN Web-UI (Disabled)     | No            | N/A                            | PASS    |
| IXIT-1-8   | API (Bearer Token)        | Yes           | No (Token only)                | PASS    |
| IXIT-1-9   | API (Bearer Token)        | Yes           | No (Token only)                | PASS    |

**Assessment**: There is no indication that the machine-to-machine authentication mechanisms
(IXIT-1-8, 1-9) violate the requirement to ensure that passwords are not used for automated system
interactions.

**Verdict**: **PASS**

## Test case 5.1-2A-2 (functional)

### Test Unit A

**Purpose**: To verify that no undocumented M2M interfaces (which might use passwords) exist on the 
device.

**Results**:
Functional network scanning (referencing results in 5.1-1-2 Unit A) confirms that all addressable
interfaces are documented. No hidden APIs or undocumented automated interfaces were discovered.

**Verdict**: **PASS**

### Test Unit B: Password Rejection for M2M

**Purpose**: To functionally verify that M2M-specific interfaces do not accept passwords, even if 
an attacker attempts to force their use (e.g., via downgrade or MITM).

| IXIT Entry      | M2M Interface  | Tested "Accept Password"?               | Result   | Verdict |
|-----------------|----------------|-----------------------------------------|----------|---------|
| IXIT-1-2 to 1-7 | Web-UI         | N/A (Human Interface)                   | N/A      | PASS    |
| IXIT-1-8        | Read-only API  | Attempted Basic Auth / Password in JSON | Rejected | PASS    |
| IXIT-1-9        | Read/Write API | Attempted Basic Auth / Password in JSON | Rejected | PASS    |

**Assessment**: Functional testing (attempting to use passwords on token-only endpoints) confirms
that the machine-to-machine authentication process does not accept passwords. There is no indication
that the implementation violates the requirement to restrict automated interactions to
non-password-based methods.

**Verdict**: **PASS**

## Group Summary

The Ruuvi Gateway complies with Provision 5.1-2A. Human interfaces are password-protected (or open
by choice), while all machine-to-machine (M2M) API interactions are strictly restricted to 
**Bearer Tokens**, which prevents the risks associated with hardcoded or shared M2M passwords.

**Group Verdict**: **PASS**
