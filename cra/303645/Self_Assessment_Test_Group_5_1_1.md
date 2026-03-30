# Test group 5.1-1: No universal default passwords

## Test case 5.1-1-1 (conceptual)

**Purpose**: To assess whether pre-installed passwords are unique per device and whether 
user-defined mechanisms are correctly categorized.

| IXIT Entry | Description               | Password-Based | User-Defined | Unique Per Device | Verdict |
|------------|---------------------------|----------------|--------------|-------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | No             | N/A          | N/A               | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | Yes            | No           | Yes               | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | Yes            | Yes          | N/A               | PASS    |
| IXIT-1-4   | LAN Web-UI (Basic)        | Yes            | Yes          | N/A               | PASS    |
| IXIT-1-5   | LAN Web-UI (Digest)       | Yes            | Yes          | N/A               | PASS    |
| IXIT-1-6   | LAN Web-UI (Open)         | No             | N/A          | N/A               | PASS    |
| IXIT-1-7   | LAN Web-UI (Disabled)     | No             | N/A          | N/A               | PASS    |
| IXIT-1-8   | API (Bearer Token)        | No             | Yes          | N/A               | PASS    |
| IXIT-1-9   | API (Bearer Token)        | No             | Yes          | N/A               | PASS    |

**Assessment**: All password-based mechanisms that are not defined by the user (IXIT-1-2) utilize a 
generation mechanism ensuring uniqueness per device.

**Verdict**: **PASS**

## Test case 5.1-1-2 (functional)

### Test Unit A: Discovery of Undocumented Mechanisms

**Purpose**: Functional discovery of network-based authentication interfaces to ensure the IXIT is 
exhaustive.

**Tools Used**: nmap (Network Scanner), ble-sniffer (Wireless Discovery).

| Port/Interface    | Protocol     | Status        | IXIT Reference       |
|-------------------|--------------|---------------|----------------------|
| Port 80 (LAN)     | HTTP         | Documented    | IXIT-1-2 through 1-7 |
| Port 80 (Hotspot) | HTTP         | Documented    | IXIT-1-1             |
| BLE               | Bluetooth LE | No Auth Found | N/A                  | 

**Assessment**: There is no indication of any undocumented password-based authentication mechanisms 
available via network interfaces or described in the user documentation.

**Verdict**: **PASS**

### Test Unit B: Requirement to Set User-Defined Passwords

**Purpose**: Verify that mechanisms declared as "User-Defined" cannot be used until the user sets a 
value.

| IXIT Entry | Authentication Factor | Required to define before use | Verdict |
|------------|-----------------------|-------------------------------|---------|
| IXIT-1-3   | User-defined Password | Yes                           | PASS    |
| IXIT-1-4   | User-defined Password | Yes                           | PASS    |
| IXIT-1-5   | User-defined Password | Yes                           | PASS    |
| IXIT-1-7   | User-defined Token    | Yes                           | PASS    |
| IXIT-1-8   | User-defined Token    | Yes                           | PASS    |

**Assessment**: For all mechanisms where the user defines the authentication value, the DUT denies
access until the initial value is established by the administrator. There is no indication that
user-defined authentication mechanisms violate the requirement to ensure values are set before the
services are used.

**Verdict**: **PASS**

### Test Unit C: Verification of Generation Mechanisms

**Purpose**: Functional check that pre-installed passwords match the documented generation logic.

| IXIT Entry | State                             | Matches IXIT Description | Verdict |
|------------|-----------------------------------|--------------------------|---------|
| IXIT-1-2   | Any state (excl. Factory Default) | Yes (DEVICEID-based)     | PASS    |

**Assessment**: Functional testing of multiple DUT samples confirms that the pre-installed
passwords match the 64-bit hardware-unique identifier (DEVICEID) as described in IXIT-1-2. There is
no indication that the generation of non-user-defined passwords differs from the documented
mechanism.

**Verdict**: **PASS**

## Group Summary

The Ruuvi Gateway fulfills the requirement of Provision 5.1-1. The primary authentication
interface (IXIT-1-2) uses a hardware-unique credential. All alternative methods, including the
user-controlled challenge-response mechanism (IXIT-1-3) and machine-to-machine interfaces, are
either user-defined, token-based, or disabled by default.

**Group Verdict**: **PASS**
