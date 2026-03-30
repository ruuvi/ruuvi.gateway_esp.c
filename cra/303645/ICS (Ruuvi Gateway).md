The following tables constitute the Implementation Conformance Statement (ICS) for the Ruuvi
Gateway, based on the requirements defined in ETSI EN 303 645.

- **Reference**: Provision number in ETSI EN 303 645.
- **Status**:
  - **M**: Mandatory.
  - **R**: Recommendation.
  - **C**: Conditional (Triggered by a numbered condition).
  - **F**: Feature-dependent (Triggered by a lettered feature).
- **Support**: **Y** (Yes), **N** (No), **N/A** (Not applicable).
- **Detail**: Summary of the implementation and reference to the corresponding IXIT entry.

## 5.1 No universal default passwords

| Reference        | Status       | Support | Detail                                                                                             |
|------------------|--------------|---------|----------------------------------------------------------------------------------------------------|
| Provision 5.1-1  | M F (a)      |   Y     | Supported via IXIT-1-2. This is the exclusive factory-default state. No universal passwords exist. |
| Provision 5.1-2  | M F (b)      |   Y     | Supported via IXIT-1-2. Passwords are hardware-unique and pre-installed at manufacturing.          |
| Provision 5.1-2A | R F (c)      |   Y     | Best practice followed: M2M uses tokens, not passwords. See IXIT-1-7, 1-8.                         |
| Provision 5.1-3  | M F (d)      |   Y     | Uses ECDH, AES-CBC, and SHA-256. See IXIT-1-2, 1-4, 1-7, 1-8.                                      |
| Provision 5.1-4  | M F (e)      |   Y     | Change mechanism provided via Web-UI/Configuration. See IXIT-1-2, 1-3, 1-4.                        |
| Provision 5.1-5  | M C F (1, f) |   Y     | Progressive time delays and rate limiting implemented. See IXIT-1-2, 1-7, 1-8.                     |

**Condition**:

1) Resource Constraints: The device has no resource constraints (processing or memory) that prevent
   the implementation of mechanisms to make brute-force attacks via network interfaces
   impracticable.

**Features (Triggers)**:
The provisions above apply because the following features, capabilities, or mechanisms are present
in the DUT:

- a) Passwords are used to authenticate users against the device or for machine-to-machine
  authentication;
- b) Pre-installed unique per-device passwords are used to authenticate users against the device or
  for machine-to-machine authentication;
- c) Machine-to-machine authentication is used;
- d) Cryptographic authentication mechanisms are used for user or machine-to-machine authentication;
- e) User authentication against the consumer IoT device is supported;
- f) Authentication mechanisms are addressable via network interfaces;

