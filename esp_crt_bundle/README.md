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
when run with `--update`.

## Automatic update pull requests

The [Check CA Bundle workflow](../.github/workflows/check-ca-bundle.yml) runs weekly and can also
be started manually. It does not run on pushes or pull requests.

Every Monday at 06:00 UTC, it runs the script with `--update`, validates the PEM
certificates with OpenSSL, creates or reuses a release-note issue, and opens a pull request
against the default branch. It commits only `esp_crt_bundle/cacert.pem` and reuses the
`automation/update-ca-bundle` branch.

The issue and PR titles include the bundle's Mozilla data date in `YYYY-MM-DD` format.
For example, `## Certificate data from Mozilla as of: Thu Aug 13 03:12:01 2026 GMT` produces
the issue title `chore(esp_crt_bundle) Update Mozilla CA bundle to 2026-08-13`.
Before creating an issue, the workflow checks all open and closed repository issues for an
exact title match, excluding PRs returned by the issues API. A matching issue is reused without
changing its state. This prevents duplicates on retries, including after a failed PR creation.

If the issue number is 1234, both the commit title and PR title are
`chore(esp_crt_bundle) #1234 Update Mozilla CA bundle to 2026-08-13`.
The commit description contains `Closes #1234`, and the PR description ends with `Closed #1234`.
These references link the release-note issue to the update and close it when merged into the
default branch.

Before publishing an update, the workflow checks all open repository PRs, including drafts,
for an exact title match. If one exists, it skips publishing and logs the existing PR URL.
Otherwise, it creates a PR or updates the existing automation-branch PR with the newer bundle
and dated title. Closed PRs do not block a new update.

If the bundle is current, it creates no issue or PR and cleans up any obsolete
update PR/branch. Download, date-parsing, and PEM-validation errors fail the workflow before it
can publish changes. Issue lookup/creation or PR lookup errors also stop PR publishing.
The dated release-note issues replace the previous generic tracking-issue notification.

After merging the workflow changes into the default branch, it can also be run immediately via
**Actions → Check CA Bundle → Run workflow**. Select the default branch; the update job skips
manual runs on other branches. Review the certificate additions/removals and CI results, then
approve and merge the generated PR. The workflow does not approve or merge it automatically.

### Repository setup

For the built-in `GITHUB_TOKEN` fallback, enable **Allow GitHub Actions to create and approve
pull requests** in **Settings → Actions → General → Workflow permissions**. An organization
policy may require an administrator to enable this. A configured `CA_BUNDLE_PR_TOKEN` uses its
own permissions. The workflow grants `contents: write`, `issues: write`, and `pull-requests: write`
to the update job.

By default, the workflow uses `GITHUB_TOKEN`. GitHub requires a maintainer to select **Approve
workflows to run** before the repository's other PR CI workflows run on PRs created or updated
with this token. See
[GitHub's token documentation](https://docs.github.com/en/actions/concepts/security/github_token#when-github_token-triggers-workflow-runs).

To have CI start automatically so the routine human task is only review, approval, and merge,
create a fine-grained personal access token limited to this repository with **Contents: Read and
write**, **Issues: Read and write**, and **Pull requests: Read and write**. Save it as the
repository Actions secret `CA_BUNDLE_PR_TOKEN` under **Settings → Secrets and variables → Actions**.
The workflow uses it
for issue lookup/creation, branch pushes, and PR creation/updates. Existing tokens must also be
granted **Issues: Read and write**. Use a token whose owner has repository write
access, obtain organization approval if required, and renew it before expiration. The PR author
cannot approve their own PR, so use a bot/service account token if the token owner also needs to
review and approve these updates. A GitHub App installation token is another option, but would
need an additional workflow step to generate a short-lived token for each run.

Any tracking issue opened by the old workflow can be closed manually once the update is merged.
