# IXIT 6-SoftComp: Software Components

The following declarations detail the individual software components comprising the Ruuvi Gateway,
specifying division boundaries for updatability, flash partition mapping references, and structural
cryptographic dependency matrices.

---

## Table C.6: IXIT 6-SoftComp (Software Components)

### **ID**: SoftComp-ROMBoot

#### Description

The internal first-stage bootloader hardwired inside the primary ESP32 silicon Mask ROM. It manages
initial silicon power-on routines, checks GPIO hardware-strapping constraints, and loads the
second-stage bootloader from flash memory.

#### Update Mechanism

The first-stage bootloader cannot be updated as it is a hardware-integrated component structurally
stored within Mask ROM.

#### Cryptographic Usage

* **Cryptographic Primitives Utilized:** No. Because hardware secure boot fuses are not burned on
  the production platform layout, the ROM bootloader performs standard dynamic instruction jumps
  without parsing cryptographic keys.
* **Side Effects Considered:** No. Side effects were not considered for this component as it is
  immutable hardware.

---

### **ID**: SoftComp-SecondBoot

#### Description

The software-based second-stage bootloader provided by the ESP-IDF framework. It resides at the base
of the primary flash storage block, manages execution partition tables, and hands off system
execution loops to the active application slot.

#### Update Mechanism

Updatable via local direct physical connection paths described under `UpdMech-USB` (Local Manual
Update via the Type-C USB port).

#### Cryptographic Usage

* **Cryptographic Primitives Utilized:** Yes. Performs basic hash-based integrity validation
  parameters over the partition layout map before jumping to the main application vector.
* **Side Effects Considered:** Yes. Side effects of updating or replacing this runtime layer are
  fully evaluated by the Software Development Department (SDD) through regression validation testing
  of the early system boot sequence and flash partition layout constraints.

---

### **ID**: SoftComp-MainFW

#### Description

The core application firmware executing within the active ESP32 partition slots (`ota_0` or
`ota_1`). It handles the primary network connectivity stack (`LwIP`), system telemetry
synchronization, outbound telemetry data clients, and the diagnostic console log loops.

#### Update Mechanism

Fully updatable via the network and local paths described under `UpdMech-WebUI` (User-Initiated
Network Update), `UpdMech-Auto` (Automatic Background Update), and `UpdMech-USB` (Local Manual
Update).

#### Cryptographic Usage

* **Cryptographic Primitives Utilized:** Yes. The firmware integrates the mature `mbedtls`
  cryptographic library to negotiate secure outbound pipelines (`HTTPS`, `MQTTS`, `WSS`), implements
  `HMAC-SHA256` payload validation signing transformations (using `SecParam-Hardware-DeviceID` as
  the root seed), hashes incoming local credentials using `MD5`, and executes `esp_image_verify`
  signature scans.
* **Side Effects Considered:** Yes. Side effects of updating these primitives are considered by the
  manufacturer through comprehensive positive and negative validation testing loops prior to
  release, ensuring persistent compatibility with cloud endpoint targets and active NVS
  configuration parameters.

---

### **ID**: SoftComp-nRF52FW

#### Description

The low-level co-processor firmware running on the nRF52811 chip architecture. It controls the
connectionless passive 2.4 GHz radio scanner loop, parses BLE broadcast advertisement frames over
channels 37/38/39, and relays structured metrics matrices to the ESP32 over a local isolated serial
bus (UART).

#### Update Mechanism

Fully updatable via `UpdMech-WebUI`, `UpdMech-Auto`, and `UpdMech-USB`. The main ESP32 application
acts as the programmer, streaming signed co-processor binary updates out of the read-only
`fatfs_nrf52` or `fatfs_nrf52_2` partitions straight to the co-processor during system updates.

#### Cryptographic Usage

* **Cryptographic Primitives Utilized:** No. The current co-processor firmware implementation does
  not utilize cryptographic primitives for its passive, receive-only BLE scanning tasks.
* **Side Effects Considered:** No. Side effects are not applicable at this layer.

---

### **ID**: SoftComp-WebUI

#### Description

The complete collection of HTML, CSS, and compressed JavaScript frontend application bundles served
by the internal HTTP server to present the management configuration wizard dashboard to the
operator.

#### Update Mechanism

Fully updatable via `UpdMech-WebUI`, `UpdMech-Auto`, and `UpdMech-USB`. The frontend asset blocks
are persistently managed within the read-only dual-filesystem partitions (`fatfs_gwui` and
`fatfs_gwui_2`).

#### Cryptographic Usage

* **Cryptographic Primitives Utilized:** Yes. The compiled JavaScript files utilize `crypto-js` and
  `elliptic` library primitives to process client-side `MD5` concatenation pipelines for interactive
  login verification challenges and execute ECDH key exchange parameters for local administrative
  configuration encryption.
* **Side Effects Considered:** Yes. Side effects of modifying or replacing these library assets are
  managed through rigorous cross-browser compatibility verification and verification checks of the
  client-side challenge-response validation logic.

---

## Summary Matrix for the Technical File

| Component ID            | Updatable? | Primary Stored Flash Partition  | Cryptographic Primitives Present? |  Side Effects Evaluated by SDD?  |
|:------------------------|:----------:|:--------------------------------|:---------------------------------:|:--------------------------------:|
| **SoftComp-ROMBoot**    |     No     | Silicon Mask ROM Block          |                No                 |     No (Immutable Hardware)      |
| **SoftComp-SecondBoot** |    Yes     | Flash Base Sector               |     Yes (LwIP/Partition CRC)      |  Yes (Boot Validation Testing)   |
| **SoftComp-MainFW**     |    Yes     | `ota_0` / `ota_1` Slots         |     Yes (mbedTLS, HMAC, MD5)      |  Yes (Exhaustive CI/CD Sweeps)   |
| **SoftComp-nRF52FW**    |    Yes     | `fatfs_nrf52` / `fatfs_nrf52_2` |                No                 |    No (Rx-Only Radio Parser)     |
| **SoftComp-WebUI**      |    Yes     | `fatfs_gwui` / `fatfs_gwui_2`   |     Yes (crypto-js, Elliptic)     | Yes (Handshake Integrity Checks) |
