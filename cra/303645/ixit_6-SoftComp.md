# IXIT 6-SoftComp: Software Components

The following table lists the individual software components of the Ruuvi Gateway, their
updatability, and their use of cryptographic primitives.

## Table C.6: IXIT 6-SoftComp (Software Components)

### SoftComp-ROMBoot: First-stage Bootloader (ROM)

#### Description

The internal primary bootloader residing in the ESP32 silicon ROM. It is responsible for
initializing the hardware and loading the second-stage bootloader from flash memory.

#### Update Mechanism

The first-stage bootloader cannot be updated as it is a hardware-integrated component stored in Mask
ROM.

#### Cryptographic Usage

Yes, the ROM bootloader contains the cryptographic primitives required to verify the digital
signature of the second-stage bootloader (if Secure Boot is enabled).
No, side effects were not considered for this component as it is immutable hardware.

---

### SoftComp-SecondBoot: ESP-IDF Second-stage Bootloader

#### Description

The software-based bootloader provided by the ESP-IDF framework. It manages partition tables and
verifies the integrity/signature of the main application firmware before execution.

#### Update Mechanism

The second-stage bootloader is updatable via USB described in UpdMech-USB (Local Manual Update (USB)).

#### Cryptographic Usage

Yes, it performs hash-based integrity checks and digital signature verification of the application
image.
Yes, side effects of replacing these primitives are considered by the Software Development
Department (SDD) through rigorous validation of the boot sequence and secure-boot path testing.

---

### SoftComp-MainFW: Main Gateway Firmware (ESP32 Application)

#### Description

The core application firmware responsible for system logic, Wi-Fi/Ethernet connectivity, MQTT
telemetry, and cloud synchronization. This component includes the mbedtls stack and LwIP network
stack.

#### Update Mechanism

Updatable via the OTA mechanism described in UpdMech-WebUI (User-Initiated Network Update),
UpdMech-Auto (Automatic Background Update) and UpdMech-USB (Local Manual Update (USB)).

#### Cryptographic Usage

Yes, the firmware utilizes mbedtls for TLS connections, AES-CBC for Web-UI session payload
encryption, HMAC-SHA256 for outbound payload authentication, and SHA-256/MD5 for the Web-UI
authentication challenge.
Yes, side effects of updating algorithms are considered via exhaustive regression testing, ensuring
backward compatibility with cloud endpoints and existing user credentials.

---

### SoftComp-nRF52FW: nRF52 Co-processor Firmware

#### Description

Firmware specifically for the nRF52 co-processor, managing Bluetooth Low Energy (BLE) scanning,
data filtering, and radio communication with Ruuvi sensors.

#### Update Mechanism

Updatable via UpdMech-WebUI, UpdMech-Auto and UpdMech-USB.
The ESP32 flashes the nRF52 via a serial/SWD interface during the system update.

#### Cryptographic Usage

No, the current nRF52 firmware implementation does not utilize cryptographic primitives for its
primary BLE scanning functions. (Link-layer encryption is handled by the ESP32 application layer
where applicable).
No, side effects are not applicable at this layer.

---

### SoftComp-WebUI: Web-UI Assets

#### Description

A collection of HTML, CSS, and JavaScript files served by the internal HTTP server for user
configuration. Includes libraries such as crypto-js and elliptic.

#### Update Mechanism

Updatable via UpdMech-WebUI, UpdMech-Auto and UpdMech-USB.
These assets are stored in a dedicated FAT-FS partition.

#### Cryptographic Usage

Yes, the JavaScript components utilize crypto-js and elliptic to perform client-side hashing and
ECDH key exchange for secure authentication.
Yes, side effects of replacing these libraries are considered through cross-browser compatibility
testing and verification of the authentication handshake logic.

----------------------------------------------------------------------------------------------------

## Summary of Component Dependencies

| Component ID        | Updatable | Primary Responsibility                       |
|---------------------|-----------|----------------------------------------------|
| SoftComp-ROMBoot    | No        | Hardware Initialization / Root of Trust      |
| SoftComp-SecondBoot | Yes       | Partition Management / Firmware Verification |
| SoftComp-MainFW     | Yes       | Networking, Logic, and Data Relaying         |
| SoftComp-nRF52FW    | Yes       | Bluetooth LE Scanning & Filtering            |
| SoftComp-WebUI      | Yes       | Local User Interface & Client-side Auth      |
