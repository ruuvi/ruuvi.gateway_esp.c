# Test group 5.1-4: Changing Authentication Values

## Test case 5.1-4-1 (conceptual)

### Test Unit A: Availability of Change Mechanism

**Purpose**: To assess whether a mechanism exists to change authentication values 
(passwords/tokens).

| IXIT Entry | Description               | Password-Based | Auth factor can be changed? | Verdict |
|------------|---------------------------|----------------|-----------------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | No             | N/A (Unauthenticated)       | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | Yes            | Yes (via Web-UI Settings)   | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | Yes            | Yes (via Web-UI Settings)   | PASS    |
| IXIT-1-4   | LAN Basic Auth            | Yes            | Yes (via Config File)       | PASS    |
| IXIT-1-5   | LAN Digest Auth           | Yes            | Yes (via Config File)       | PASS    |
| IXIT-1-8   | API (Bearer Token)        | No             | Yes (Token Regeneration)    | PASS    |
| IXIT-1-9   | API (Bearer Token)        | No             | Yes (Token Regeneration)    | PASS    |

**Assessment**: All active authentication mechanisms provide a mechanism for the user to change the
authentication value. For the default interface (IXIT-1-2/3), this is done via the Web-UI. For API
access, tokens can be regenerated.

**Verdict**: **PASS**

### Test Unit B: Documentation Understandability

**Purpose**: To assess if the instructions for changing values are understandable for non-technical 
users.

| IXIT Entry | Auth Factor      | Documentation Clear? | Verdict |
|------------|------------------|----------------------|---------|
| IXIT-1-2/3 | Primary Password | Yes                  | PASS    |
| IXIT-1-4/5 | Legacy Auth      | Yes                  | PASS    |
| IXIT-1-8/9 | Bearer Tokens    | Yes                  | PASS    |

**Assessment**: The Ruuvi Gateway documentation provides step-by-step instructions for changing
passwords in the Web-UI. Advanced legacy modes (IXIT-1-4/5) are documented in the technical
supplement with clear warnings for non-technical users.

**Verdict**: **PASS**

## Test case 5.1-4-2 (functional)

### Test Unit A & B: Functional Execution and Success

**Purpose**: To verify that changing the password actually works and invalidates the old one.

**Test Method**: Log into the Web-UI using factory credentials (IXIT-1-2), navigate to 
the **Remote Access Settings** page, change the credentials to a user-defined value (IXIT-1-3), 
and verify that the old password no longer grants access.

| IXIT Entry | Action Performed          | Result                               | Verdict |
|------------|---------------------------|--------------------------------------|---------|
| IXIT-1-2   | Switched to IXIT-1-3      | Old password rejected; new accepted. | PASS    |
| IXIT-1-3   | Changed username/password | Old password rejected; new accepted. | PASS    |
| IXIT-1-8   | Regenerated Bearer Token  | Old token rejected; new accepted.    | PASS    |
| IXIT-1-9   | Regenerated Bearer Token  | Old token rejected; new accepted.    | PASS    |

**Assessment**: Functional testing confirms that authentication value changes are successful and
correctly applied. Transitioning from the factory-default state to the user-defined
challenge-response state effectively invalidates the pre-installed credentials.

**Verdict**: **PASS**

## Group Summary

The Ruuvi Gateway fulfills the requirement of Provision 5.1-4. Users can easily change their
passwords via the Web-UI, successfully transitioning the device from a pre-installed unique
password (IXIT-1-2) to a user-defined secure state (IXIT-1-3). Documentation provides clear
instructions for both standard and advanced authentication methods.

**Group Verdict**: **PASS**
