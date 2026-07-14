# CRA: 303 645 IXIT-2-USERINFO: User Information

Source: ETSI TS 103 701 V2.1.1 (2025-05):
A.3: "Implementation eXtra Information for Testing (IXIT) pro forma":
"IXIT 2-UserInfo: User Information"

## Documentation of Change Mechanisms

This information is documented on the manufacturer website.

The user can access it via:

- https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan

## Documentation of Replacement

Not applicable, because the DUT supports firmware updates. Therefore, no separate hardware
replacement guidance is required for the absence of update capability.

## Documentation of Sensors

Information about the sensing capabilities of the DUT is documented on the manufacturer website.

Ruuvi Gateway supports collecting data from Bluetooth Low Energy (BLE) devices. By default,
the DUT collects data only from Ruuvi BLE devices, with filtering based on the manufacturer ID.

The user can access documentation describing how to change the filtering via:

- https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/bluetooth-scanning-settings

## Documentation of Secure Setup

Methods for secure setup of the DUT are documented in the manufacturer’s user documentation.

The user can access them via:

- https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan

## Documentation of Setup Check

Methods to check the secure setup of the DUT are documented in the user documentation and
reflected in the Gateway Web UI.

The user can access this information via:

- Gateway Web UI configuration pages
- Access settings
  documentation: https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/access-settings-from-lan

The user can verify:

- which authentication method is enabled,
- whether access protection is configured,
- whether security-relevant settings have been changed from their defaults.

## Documentation of Maintenance Check

The user can access this information via:

- Gateway Web UI

Methods to check the secure maintenance state of the DUT are documented in the product
documentation.
The user can access it via:

- https://docs.ruuvi.com/ruuvi-gateway-firmware/gateway-html-pages/software-update

The documentation describes how to check the availability of firmware updates.

## Documentation of Personal Data

Information about processing of personal data is documented in the manufacturer’s
privacy documentation and product documentation.

The DUT primarily processes device and sensor data. Personal data may be processed only if the user
configures the DUT in a way that associates data with identifiable individuals.

The user can access this information via:

- Privacy notice: https://ruuvi.com/privacy/ (section "Privacy Statement & Policy on Ruuvi Station
  and Ruuvi Gateway and other software")
- Product documentation: https://docs.ruuvi.com/ruuvi-gateway-firmware/

## Documentation of Telemetry Data

Telemetry data collection is covered by the manufacturer's privacy terms.

The user can access it via:

- Privacy notice: https://ruuvi.com/privacy/ (section "Privacy Statement & Policy on Ruuvi Station
  and Ruuvi Gateway and other software")

## Documentation of Deletion

> **Documentation gap (to be resolved before submission):** Data-deletion methods are not yet
> documented. A dedicated section must be added covering (a) resetting the Gateway to factory
> settings via the physical Configure button (which erases all stored credentials, tokens and
> configuration from NVS — see IXIT 25-DelFunc), and (b) the Ruuvi Station "delete account"
> functionality for manufacturer-held data. A tracking issue must be open before this PR is merged.

Methods for deletion of personal data are documented in the product documentation
and privacy documentation.

The user can access them via:

- Product user documentation: https://docs.ruuvi.com/ruuvi-gateway-firmware/
- Privacy notice: https://ruuvi.com/privacy/
- Data deletion documentation: *(to be published — see gap note above)*

The documentation explains, as applicable:

- how to remove stored credentials and tokens,
- how to reset the DUT to factory settings,
- how to delete locally stored configuration or history data,
- how to request deletion of manufacturer-held personal data, if applicable.

## Model Designation

Model designation: Ruuvi Gateway

The model information is printed on the product label (sticker) on the underside of the Gateway
enclosure. The user recognizes the model designation by reading this label.

> **Documentation gap (to be resolved before submission):** The user manual must be updated to
> direct users to the sticker on the underside of the Gateway to check the model information.

The user can recognize the model designation from:

- the product label (sticker) on the underside of the DUT,
- the packaging,
- the product documentation.

Documentation of the model designation: *(to be added to the user manual — see gap note above)*

## Support Period

> **Documentation gap (to be resolved before submission):** The manufacturer publishes lifecycle
> promises at https://ruuvi.com/terms/lifecycle-promises/, but the Ruuvi Gateway is not currently
> covered by that page. The support period for the Gateway must be defined and published before
> submission. A tracking issue should be open before this PR is merged.

The support period is defined in the published lifecycle policy.

The user can access it via:

- Product lifecycle page: https://ruuvi.com/terms/lifecycle-promises/ *(to be extended to cover the
  Ruuvi Gateway — see gap note above)*

## Publication of Support Period

The defined support period is published on the manufacturer's website.

The user can access it via:

- Product lifecycle / support page: https://ruuvi.com/terms/lifecycle-promises/ *(pending coverage
  of the Ruuvi Gateway — see gap note above)*

The publication identifies the duration of support for the DUT, including update-related
maintenance where applicable.

## Publication of Vulnerability Disclosure Policy

The manufacturer’s vulnerability disclosure policy is published on the manufacturer website.

The user can access it via:

- Vulnerability disclosure / security policy: https://ruuvi.com/terms/vulnerability-policy/

The publication provides:

- the channel for reporting vulnerabilities,
- expectations for coordinated disclosure,
- information for security researchers and users.

## Publication of Non-Updatable

Not applicable, because the DUT supports software/firmware updates.
