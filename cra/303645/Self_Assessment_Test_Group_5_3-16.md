# Test group 5.3-16: Clearly Recognizable Model Designation

Provision 5.3-16 — Status: **M**. Related IXIT: `IXIT 2-UserInfo`.

---

## Test case 5.3-16-1 (conceptual)

**Purpose**: To conceptually assess whether the model designation of the DUT can be obtained in a
clearly recognizable way via physical labeling on the device or through a user interface according
to `IXIT 2-UserInfo`.

---

### Test Unit A: Assessment of Model Designation Identification Method

**Testing Methodology**: The test laboratory evaluated the "Model Designation" declarations in
`IXIT 2-UserInfo` to verify that the identification method is clear and recognizable.

| Declared Identification Method (`IXIT 2-UserInfo`) | Implementation & Location                                                                                      | Recognizability & Clarity Assessment                                                                                                                                            | Unit Verdict |
|:---------------------------------------------------|:---------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Physical Enclosure Labeling**                    | Permanently affixed tracking label (sticker) on the lower exterior underside surface of the gateway enclosure. | **Clearly Recognizable.** Printed in high-contrast simple text on the physical serial/regulatory label, ensuring immediate visibility without requiring network setup or power. |   **PASS**   |

* **Assessment Justification**: The model designation `"Ruuvi Gateway"` is permanently printed on
  the physical tracking label on the underside of the device casing. This physical labeling method
  provides a clearly recognizable model identification mechanism suitable for non-technical users.
* **Verdict**: **PASS**

---

## Test case 5.3-16-2 (functional)

**Purpose**: To functionally verify that the model designation of the DUT can be extracted using the
method described in `IXIT 2-UserInfo` (`a`), and that the extracted designation is available in
simple text matching the IXIT declaration (`b`).

---

### Test Units Functional Assessment Matrix

| Test Unit  | Purpose / Focus                     | Functional Verification & Observed Result                                                                                                                                                       | Unit Verdict |
|:-----------|:------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a** | **Extraction via Described Method** | Laboratory personnel inspected the physical tracking label affixed to the underside enclosure of the DUT per `IXIT 2-UserInfo`. The model designation was extracted successfully without tools. |   **PASS**   |
| **Unit b** | **Simple Text & IXIT Matching**     | The extracted string reads `"Ruuvi Gateway"`. It is rendered in plain, unencoded simple text and matches the expected model string in `IXIT 2-UserInfo` precisely.                              |   **PASS**   |

* **Assessment Justification**: Physical inspection confirms that the label is affixed to the device
  casing, the model designation is clearly legible in plain text, and the string matches
  `"Ruuvi Gateway"`.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.3-16-2

| Test Unit  | Purpose / Focus         | Verification Strategy & Result                                 | Unit Verdict |
|:-----------|:------------------------|:---------------------------------------------------------------|:------------:|
| **Unit a** | Extraction Verification | Direct physical inspection of underside tracking label.        |   **PASS**   |
| **Unit b** | Plain Text & IXIT Match | Plain text string `"Ruuvi Gateway"` matches `IXIT 2-UserInfo`. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.3-16 of `ETSI EN 303 645`. The model
designation (`Ruuvi Gateway`) is permanently printed in high-contrast simple text on the physical
tracking label on the underside casing of the device. Functional inspection confirms that the model
designation is easily obtainable, legible in plain text, and matches the technical file
documentation.

**Group Verdict**: **PASS**
