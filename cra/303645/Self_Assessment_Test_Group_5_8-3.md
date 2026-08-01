# Test group 5.8-3: Clear Documentation of External Sensing Capabilities

Provision 5.8-3 — Status: **M F (v)**. Related IXIT: `IXIT 2-UserInfo`, `IXIT 22-ExtSens`.

---

## Condition Evaluation (`ETSI EN 303 645` Annex B)

* **Condition 22 (v) Requirement**: *"The device has external sensing capabilities (e.g. optic,
  acoustic, biometric or location sensors)."*
* **DUT Capabilities Assessment**: As declared in `IXIT 22-ExtSens`, the DUT incorporates an
  indirect logical external sensing capability via its Bluetooth Low Energy (BLE) radio sub-system (
  `ExtSens-Logical-BLE-Radio-Scanning`). The nRF52 co-processor passively scans local 2.4 GHz
  channels to intercept, decode, and process environmental metrics emitted by nearby BLE beacons (
  such as RuuviTag sensors measuring temperature, humidity, air pressure, and motion).
* **Condition Result**: Condition 22 evaluates to **TRUE**. Provision 5.8-3 is **Mandatory (M)**.

---

## Test case 5.8-3-1 (functional)

**Purpose**: To functionally assess whether the documentation of external sensing capabilities is
publicly accessible (`a`), understandable for a user with limited technical knowledge according to
Clause D.3 (`b`), and whether all physical and logical sensing capabilities of the DUT are
completely documented in `IXIT 22-ExtSens` (`c`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Accessibility of Sensor Documentation

* **Requirement**: Verify that the documentation describing external sensing capabilities is
  publicly accessible via the access vectors specified in "Documentation of Sensors" in
  `IXIT 2-UserInfo`.
* **Evaluation**: Functional navigation checks confirm that the documentation is publicly accessible
  without login gating or paywalls at:
  * `https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/bluetooth-scanning-settings`
* **Unit A Verdict**: **PASS**

#### Test Unit B: Understandability for Non-Technical Users (Clause D.3)

* **Requirement**: Assess whether the sensor documentation explains the presence, purpose, data
  collection scope, and user control options of the sensing capabilities in clear, plain language
  suitable for non-technical users.
* **Evaluation**:

| Assessment Criterion (Clause D.3)   | Documented Explanation (`IXIT 2-UserInfo` / `IXIT 22-ExtSens`)                                                                       | Understandability Audit Assessment                                                                                                      | Unit Verdict |
|:------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Sensing Function Identification** | Explains that the gateway listens for wireless Bluetooth signals sent by nearby RuuviTag or compatible environmental sensors.        | **Clear & Transparent.** Clearly identifies that the device captures radio signals from surrounding weather and environmental monitors. |   **PASS**   |
| **Purpose & Operational Scope**     | Explains that the device gathers temperature, humidity, pressure, and movement data to display on user dashboards or apps.           | **Clear & Transparent.** Plain-language explanation connects radio scanning to user-visible environmental graphs and alerts.            |   **PASS**   |
| **User Control & Filtering**        | Outlines how users can restrict scanning to specific sensors, set up whitelists/blacklists, or pause data forwarding via the Web-UI. | **Clear & Transparent.** Provides step-by-step instructions for non-technical users to control or restrict data collection.             |   **PASS**   |

* **Unit B Verdict**: **PASS**

#### Test Unit C: Completeness of Sensing Capabilities Documentation & Physical Audit

* **Requirement**: Perform a physical inspection of the DUT casing and mainboard to confirm that all
  obvious or covert physical sensing capabilities (e.g. cameras, microphones, PIR sensors) are
  identified and documented in `IXIT 22-ExtSens`.
* **Evaluation**:

| Inspection Target                             | Physical / Visual Teardown Findings                                                                     | Documentation Alignment (`IXIT 22-ExtSens`)                                | Unit Verdict |
|:----------------------------------------------|:--------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------|:------------:|
| **Acoustic Sensors** (Microphones)            | No microphone components, acoustic ports, or sound transducers present on PCB or casing.                | Accurately documented under `ExtSens-Physical-Hardware-Constraints`.       |   **PASS**   |
| **Optical / Vision Sensors** (Cameras/Lenses) | No optical lenses, image sensors, or ambient light sensors present on PCB or casing.                    | Accurately documented under `ExtSens-Physical-Hardware-Constraints`.       |   **PASS**   |
| **Biometric / Motion Sensors** (PIR/IR)       | No biometric scanners, infrared motion detectors, or thermal sensors on-board.                          | Accurately documented under `ExtSens-Physical-Hardware-Constraints`.       |   **PASS**   |
| **Wireless Radio Sensing** (2.4 GHz BLE)      | Dedicated nRF52811 radio co-processor connected to external BLE antenna for passive broadcast scanning. | Fully documented and explained under `ExtSens-Logical-BLE-Radio-Scanning`. |   **PASS**   |

* **Functional Assessment Justification**: External sensor documentation is publicly accessible and
  written in clear, plain language understandable to non-technical users. Physical teardown and
  casing inspection confirm that the DUT possesses zero undocumented physical or covert sensing
  hardware, and all logical BLE radio scanning capabilities are fully cataloged.

* **Unit C Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.8-3-1

| Test Case          | Purpose / Focus                          | Assessment Summary                                                                                           | Unit Verdict |
|:-------------------|:-----------------------------------------|:-------------------------------------------------------------------------------------------------------------|:------------:|
| **5.8-3-1 Unit a** | Sensor Documentation Accessibility       | Documentation is publicly accessible via declared online URLs (`docs.ruuvi.com`).                            |   **PASS**   |
| **5.8-3-1 Unit b** | Understandability for Users (Clause D.3) | Sensor function, data scope, and user control options are explained in clear, non-technical terms.           |   **PASS**   |
| **5.8-3-1 Unit c** | Physical Inspection & Completeness Check | Physical casing teardown confirms zero covert hardware sensors; all logical BLE sensing is fully documented. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.8-3 of `ETSI EN 303 645`. The technical
documentation (`IXIT 2-UserInfo`, `IXIT 22-ExtSens`) clearly and transparently describes the
device's external sensing capabilities (passive 2.4 GHz BLE broadcast radio scanning for
environmental tags). The documentation is publicly accessible and easily understandable for
non-technical users. Physical inspection and mainboard teardown confirm that no undocumented or
covert physical sensors (such as cameras, microphones, or biometric detectors) exist on the device.

**Group Verdict**: **PASS**
