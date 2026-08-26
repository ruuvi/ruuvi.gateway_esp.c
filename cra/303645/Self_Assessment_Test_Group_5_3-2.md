# Test group 5.3-2: At Least One Secure Update Mechanism Exists

Provision 5.3-2 — Status: **M C (15)**. Related IXIT: `IXIT 7-UpdMech`.

---

## Test case 5.3-2-1 (conceptual)

**Purpose**: To conceptually assess whether the update installation mechanisms defined in
`IXIT 7-UpdMech` incorporate adequate design measures (signature verification, cross-component
manifest validation, dual-bank staging, and automated rollback) to prevent an attacker from misusing
the update process under the baseline attacker model.

### Test Unit A: Conceptual Anti-Misuse Assessment

| Mechanism ID    | Delivery & Medium                     | Stated Security Guarantees & Cryptographic Safeguards                                                                                                                                                                                                                                                                   | Attacker Misuse Vector Evaluated                                                                                                                                                                                     | Case Verdict |
|:----------------|:--------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `UpdMech-WebUI` | User-Initiated Network Update (HTTPS) | Main binary (`ruuvi_gateway_esp.bin`) carries RSA-3072-PSS signature checked immediately post-download. Main FW contains embedded signatures for `fatfs_gwui.bin` and `fatfs_nrf52.bin` checked at boot. Images stream to inactive slots (`ota_0`/`ota_1`, `fatfs_gwui`/`fatfs_gwui_2`, `fatfs_nrf52`/`fatfs_nrf52_2`). | **MitM / Component Substitution:** Prevented via mandatory RSA-3072-PSS signature checks post-download and cross-manifest signature validation at boot.<br>**Bricking Vector:** Prevented via dual-slot A/B staging. |   **PASS**   |
| `UpdMech-Auto`  | Automated Background Update (HTTPS)   | TLS 1.2/1.3 transport security to `https://network.ruuvi.com/firmwareupdate`. Application-layer RSA-3072-PSS checks over all 3 image parts (`ruuvi_gateway_esp`, `fatfs_gwui`, `fatfs_nrf52`) plus boot-time SWD-injected nRF52 SHA-256 RAM checks.                                                                     | **Unattended Remote Compromise & Partial Version Mix-Match:** Prevented because all 3 downloaded parts undergo cross-manifest cryptographic signature validation before partition slot commitment.                   |   **PASS**   |
| `UpdMech-USB`   | Local Manual Update (USB-UART Bridge) | Requires immediate physical proximity access to Type-C USB port. Local serial flasher writes images to flash; upon booting Ruuvi Gateway firmware (v1.17.3+), the application layer validates its own RSA-3072-PSS signature and cross-verifies auxiliary filesystem signatures (`fatfs_gwui`/`fatfs_nrf52`).           | **Remote Network Exploits:** Completely isolated from remote network attack vectors due to requirement for physical USB access. Official firmware images validate self and auxiliary partition integrity on boot.    |   **PASS**   |

**Assessment Justification**: Conceptual review confirms that all three update mechanisms
incorporate robust multi-part application-layer cryptographic authenticity checks (RSA-3072-PSS) and
structural staging isolation. The firmware architecture prevents partial component substitution (
e.g., swapping `fatfs_gwui.bin` or `fatfs_nrf52.bin` with an older or tampered image) by
cross-verifying auxiliary filesystem signatures against the main firmware manifest at boot. The
device has no resource constraint preventing the execution of an update mechanism (satisfying
Condition 15).

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
