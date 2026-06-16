---
title: "CA bundle (esp_crt_bundle/cacert.pem) is out of date"
labels: maintenance, security
---
The scheduled **Check CA Bundle** workflow found a newer Mozilla CA certificate
bundle on <https://curl.se/docs/caextract.html>.

The bundle embedded in the firmware (`esp_crt_bundle/cacert.pem`) should be
refreshed so that TLS connections continue to trust newly-added roots and stop
trusting roots that have been removed.

**Failing run:**
{{ env.GITHUB_SERVER_URL }}/{{ env.GITHUB_REPOSITORY }}/actions/runs/{{ env.GITHUB_RUN_ID }}

### How to update

```bash
scripts/check_ca_bundle_up_to_date.sh --update
git add esp_crt_bundle/cacert.pem
git commit -m "esp_crt_bundle: update Mozilla CA bundle"
```

Then open a PR. The next successful run of the **Check CA Bundle** workflow
will close this issue automatically.

