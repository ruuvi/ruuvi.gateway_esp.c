# Test group 5.3-2: At Least One Secure Update Mechanism Exists

Provision 5.3-2 — Status: **M C (15)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-2-1 (conceptual)

**Purpose**: To conceptually assess whether the update installation mechanisms defined in
`IXIT 7-UpdMech` incorporate adequate design measures (signature verification, cross-component
manifest validation, dual-bank staging, and automated rollback) to prevent an attacker from misusing
the update process under the baseline attacker model.

### Test Unit A: Conceptual Anti-Misuse Assessment

| Mechanism ID    | Delivery & Medium                     | Stated Security Guarantees & Cryptographic Safeguards                                                                                                                                                                                                                          | Attacker Misuse Vector Evaluated                                                                                                                                                                                                                                             |          Case Verdict          |
|:----------------|:--------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------:|
| `UpdMech-WebUI` | User-Initiated Network Update (HTTPS) | The main candidate is verified with RSA-3072-PSS and signer-key continuity before boot selection. The selected main application then verifies the auxiliary partitions during early initialization; signatures for those partitions are embedded in that new main application. | **MitM / Component Substitution:** A failed main-image verification invalidates the staged partitions; a failed selected-partition check leaves the OTA image pending and invokes rollback.<br>**Bricking Vector:** Dual-slot staging preserves the previously working slot. |            **PASS**            |
| `UpdMech-Auto`  | Automated Background Update (HTTPS)   | TLS transport plus the same on-device main-candidate and selected-partition verification sequence as `UpdMech-WebUI`.                                                                                                                                                          | **Unattended Remote Compromise & Partial Version Mix-Match:** Prevented by main-candidate verification before boot selection and selected-partition verification before normal service initialization.                                                                       |            **PASS**            |
| `UpdMech-USB`   | Local Manual Update (USB-UART Bridge) | Requires sustained physical access and ROM-loader boot-strap entry. For an official Ruuvi Gateway image, the normal application self-check validates the selected partition set after boot. v1.17.x does not authenticate a complete physical ROM-loader reflash.              | **Remote attacks:** Isolated from remote network attack vectors. The physical recovery/service limitation and planned v1.18.x hardware enforcement are recorded in `IXIT 7-UpdMech`.                                                                                         | **PASS (ETSI baseline scope)** |

**Assessment Justification**: The supported network update mechanisms use staged partitions,
main-candidate signature verification before boot selection, and selected-partition verification
during early initialization. The latter timing is required because the auxiliary signatures are
embedded in the new main application. A failure before boot selection invalidates the staged
partitions; a failure after reboot retains the OTA rollback state and returns to the prior working
slot. `UpdMech-USB` remains a documented physical recovery/service path with its v1.17.x limitation
explicitly recorded. The device has no resource constraint preventing the execution of an update
mechanism (satisfying Condition 15).

**Verdict**: **PASS**

---

## Test case 5.3-2-2 (functional)

**Purpose**: To functionally verify the effectiveness of the update mechanisms in rejecting
manipulated, incomplete, substituted, or out-of-band tampered software updates under simulated
adverse conditions.

### Test Units A & B: Devised Attack Simulations & Resilience Verification

**Testing Methodology**: The test laboratory (TL) devised and executed targeted adverse actions
against each update mechanism declared in `IXIT 7-UpdMech` to verify that security guarantees are
maintained in runtime environments.

| Target Mechanism                 | Devised Adverse Action / Attack Scenario                                                                                                                                                  | Observed System Reaction & Safeguard Execution                                                                                                                                                                                                                                       | Unit Verdict |
|:---------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` / `UpdMech-Auto` | **MitM / Main Payload Bit-Flipping:** Intercepted network transit and flipped bits within `ruuvi_gateway_esp.bin`.                                                                        | `esp_image_verify` immediately detected the RSA-3072-PSS signature mismatch post-download. The inactive slot was invalidated, execution handoff was aborted, and the DUT maintained active operational continuity on the primary slot.                                               |   **PASS**   |
| `UpdMech-WebUI` / `UpdMech-Auto` | **Component Mix-and-Match / Partial Substitution:** Uploaded a validly signed `ruuvi_gateway_esp.bin` combined with an old or altered `fatfs_gwui.bin` or `fatfs_nrf52.bin` image.        | Main firmware initialized at boot, executed `esp_secure_boot_verify_rsa_signature_block` checks against the auxiliary images using signatures embedded in `ruuvi_gateway_esp.bin`, detected the manifest mismatch, and executed an immediate rollback to the previous partition set. |   **PASS**   |
| `UpdMech-WebUI` / `UpdMech-Auto` | **Interrupted Power / Staging Failure:** Cut physical system power at 50% completion of the background binary download loop to simulate a mid-update failure.                             | Upon reboot, the system detected an incomplete staging block in the inactive slot, invalidated the incomplete slot, and safely booted from the known-good active application partition.                                                                                              |   **PASS**   |
| `UpdMech-WebUI` / `UpdMech-Auto` | **Direct Co-Processor SWD Tampering:** Flashed an unverified binary directly to the nRF52 chip via an external hardware SWD debugger probe to simulate an out-of-band physical injection. | Upon boot, the ESP32 host halted the nRF52, injected the `nrf52swd_calc_sha256_digest_on_nrf52` stub into nRF52 RAM, detected that the live nRF52 flash digest did not match `fatfs_nrf52.bin`, and automatically re-flashed the nRF52 chip to restore factory integrity.            |   **PASS**   |
| `UpdMech-USB`                    | **Local Serial Flashing & Partition Cross-Verification:** Flashed Ruuvi Gateway firmware (v1.17.3+) over USB using `esptool.py` alongside an altered auxiliary filesystem partition.      | The main firmware booted, executed self-signature validation, and checked the signatures of `fatfs_gwui.bin` and `fatfs_nrf52.bin`. Detecting the auxiliary signature mismatch, the system invalidated the corrupted staging partition and initiated rollback/recovery routines.     |   **PASS**   |

**Assessment Justification**: Functional testing demonstrates that all update mechanisms effectively
withstand adverse manipulation. Injected signature errors, partial component mix-and-match
substitutions, interrupted flashing loops, and direct SWD co-processor tampering are reliably
detected and neutralized without compromising system integrity or causing permanent device lockout.

**Verdict**: **PASS**
