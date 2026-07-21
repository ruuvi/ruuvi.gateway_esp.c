# IXIT 22-ExtSens: External Sensors

The following declarations detail the external sensing capabilities supported by the Device Under
Test (DUT), distinguishing between internal physical hardware constraints and remote logical
data-gathering mechanisms.

---

## Table C.22: IXIT 22-ExtSens (External Sensors)

### **ID**: ExtSens-Physical-Hardware-Constraints

#### Description

The Ruuvi Gateway does not feature any physical, on-board hardware sensing components within or
attached to its enclosure layout. There are no built-in microphones, video cameras, infrared motion
sensors, or localized thermodynamic physical sensors wired to the internal microcontroller buses.

---

### **ID**: ExtSens-Logical-BLE-Radio-Scanning

#### Description

The device possesses an indirect, logical external environmental sensing capability driven entirely
by its Bluetooth Low Energy (BLE) radio architecture.

* **Sensing Mechanism:** The nRF52811 co-processor radio stack continuously executes passive,
  receive-only (Rx) scanning sweeps over local 2.4 GHz industrial wireless channels (specifically
  targeting channels 37, 38, and 39) to intercept broadcasted advertisement frames. It supports
  standard 1M, 2M, and Coded LE physical layers (PHY).
* **Target Elements:** It reads, decodes, and filters incoming data packets emitted by independent,
  remote peripheral beacons operating within its wireless range—such as RuuviTag environmental
  sensors (measuring temperature, humidity, air pressure, and motion vectors), Ruuvi Air air quality
  sensors, or custom compatible BLE transmitters.
* **Data Processing:** The captured broadcast packets are transferred internally over an isolated
  local serial bus (UART) to the main ESP32 processor to be compiled into transient runtime memory
  history arrays and relayed outbound to authorized target analytics layers according to
  user-defined routing profiles.

---

## Summary Matrix for the Technical File

| Sensing Capability ID                     | Physical Sensor On-Board? | Sensing Input Stream Medium                                           | Primary Processing Component    |
|:------------------------------------------|:-------------------------:|:----------------------------------------------------------------------|:--------------------------------|
| **ExtSens-Physical-Hardware-Constraints** |            No             | None (No physical audio, video, or ambient sensors)                   | N/A                             |
| **ExtSens-Logical-BLE-Radio-Scanning**    |            No             | 2.4 GHz BLE Wireless Broadcast Advertisement Frames (1M/2M/Coded PHY) | nRF52811 Radio Stack Sub-system |
