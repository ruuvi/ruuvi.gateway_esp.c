# Test group 5.1-5: Brute Force Prevention

## Test case 5.1-5-1 (conceptual)

### Test Unit A

**Purpose**: To assess whether the documented mechanisms (Rate Limiting, Time Delays, and Entropy)
make successful brute-force attacks via network interfaces impracticable.

| IXIT Entry | Description               | Password/Token Based | Prevention Mechanism                     | Verdict |
|------------|---------------------------|----------------------|------------------------------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | No                   | N/A (Unauthenticated)                    | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | Yes                  | Implemented (Entropy + Time Delays)      | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | Yes                  | Implemented (Entropy + Time Delays)      | PASS    |
| IXIT-1-4   | LAN Basic Auth            | Yes                  | Implemented (Global Rate Limiting)       | PASS    |
| IXIT-1-5   | LAN Digest Auth           | Yes                  | Implemented (Global Rate Limiting)       | PASS    |
| IXIT-1-8   | API (Read-Only)           | Yes                  | Implemented (Token Entropy + Rate Limit) | PASS    |
| IXIT-1-9   | API (Read/Write)          | Yes                  | Implemented (Token Entropy + Rate Limit) | PASS    |

**Assessment Justification**:

- **Primary Interfaces (IXIT-1-2, 1-3)**: The device utilizes high-entropy passwords (64-bit) which,
  combined with progressive time delays between failed attempts, makes automated guessing attacks
  computationally and practically infeasible.
- **Legacy Interfaces (IXIT-1-4, 1-5)**: These modes, while less secure by design, are protected by 
  a global rate-limiting mechanism on the HTTP server to mitigate high-speed brute-forcing.
- **M2M Interfaces (IXIT-1-8, 1-9)**: These utilize high-entropy Bearer Tokens. Brute-force is made
  impracticable through the vast keyspace of the tokens and system-wide rate-limiting on the HTTP
  server.

- **Assessment Result**: There is no indication that the documented brute-force prevention
  mechanisms violate the requirement to make successful attacks via network interfaces
  impracticable.

**Verdict**: **PASS**

## Test case 5.1-5-2 (functional)

### Test Unit A: Discovery of Undocumented Interfaces

**Purpose**: To functionally verify that no undocumented authentication interfaces exist that might 
lack brute-force protection.

**Assessment Result**: Reference is made to the discovery results in Test Case 5.1-1-2 Unit A. All
network-addressable authentication mechanisms found via nmap and protocol sniffing are documented in
the IXIT. There is no indication that the network-based discovery violates the requirement to
identify all available interfaces.

**Verdict**: **PASS**

### Test Unit B: Functional Brute-Force Attempt

**Purpose**: To attempt a brute-force attack to verify that the documented prevention mechanisms
(Delays/Rate Limiting) are active and effective.

**Test Method**: An automated script was used to attempt 1,000 rapid authentication requests 
against the /auth (Web-UI) and /history (API) endpoints.

| IXIT Entry | Observed Behavior                                                  | Successful Brute Force? | Verdict |
|------------|--------------------------------------------------------------------|-------------------------|---------|
| IXIT-1-2   | Responses delayed by 1 second (Time Delay).                        | No                      | PASS    |
| IXIT-1-3   | Responses delayed by 1 second (Time Delay).                        | No                      | PASS    |
| IXIT-1-8   | Server began returning 429 Too Many Requests or delayed responses. | No                      | PASS    |
| IXIT-1-9   | Server began returning 429 Too Many Requests or delayed responses. | No                      | PASS    |

**Assessment Justification**:
Functional testing confirms that the device correctly enforces the documented mitigations. The
implementation of time delays and rate limiting successfully prevented high-speed automated
guessing.

**Assessment Result**: There is no indication that the functional implementation of brute-force
protection violates the requirement to adhere to the prevention mechanisms described in the IXIT.

**Verdict**: **PASS**

## Group Summary

The Ruuvi Gateway fulfills the requirement of Provision 5.1-5. It combines cryptographic entropy (
unique per-device identifiers) with active network-layer protections (time delays)
to ensure that brute-force attacks are impracticable on all addressable interfaces.

**Group Verdict**: **PASS**
