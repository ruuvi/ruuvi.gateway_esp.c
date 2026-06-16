# Generating the List of Root Certificates

The list of root certificates comes from Mozilla’s NSS root certificate store, 
which can be found [here](https://wiki.mozilla.org/CA/Included_Certificates).

The list can be downloaded and created by running the script mk-ca-bundle.pl 
that is distributed as a part of curl. 

Another alternative would be to download the finished list directly from the curl website: 
[CA certificates extracted from Mozilla](https://curl.se/docs/caextract.html)

The bundle is stored at a fixed path, `esp_crt_bundle/cacert.pem`, which is referenced from
`sdkconfig` via `CONFIG_MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE_PATH`. To refresh the bundle, replace
the contents of that file in place — there is no need to update `sdkconfig`. The build system
tracks `cacert.pem` as a dependency of the embedded certificate blob, so the firmware is
rebuilt automatically on the next build.

The `scripts/check_ca_bundle_up_to_date.sh` helper compares the local bundle's date with the
latest one published at <https://curl.se/ca/cacert.pem> and can update the local file in place
when run with `--update`. The same script is executed by the **Check CA Bundle** GitHub Actions
workflow on every push, PR, and weekly schedule.
