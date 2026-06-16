#!/usr/bin/env bash
#
# Check whether the bundled Mozilla CA certificates in esp_crt_bundle/cacert.pem
# are up-to-date with the latest cacert.pem published by curl
# (https://curl.se/docs/caextract.html).
#
# Both the local and the remote bundle embed their generation date in a header
# comment line of the form:
#   ## Certificate data from Mozilla as of: Tue May 20 03:12:02 2025 GMT
#
# Usage:
#   check_ca_bundle_up_to_date.sh           # check only
#   check_ca_bundle_up_to_date.sh --update  # check, and overwrite the local
#                                           # bundle if the remote is newer
#
# Exit codes:
#   0 — local bundle is up-to-date (or was successfully updated with --update)
#   1 — local bundle is older than remote (only without --update)
#   2 — internal error (download failed, parsing failed, bad arguments, etc.)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUNDLE_DIR="${REPO_ROOT}/esp_crt_bundle"
LOCAL_FILE="${BUNDLE_DIR}/cacert.pem"
REMOTE_URL="https://curl.se/ca/cacert.pem"

err()  { echo "ERROR: $*" >&2; }
info() { echo "$*"; }

# --- Parse arguments --------------------------------------------------------

UPDATE=0
for arg in "$@"; do
    case "${arg}" in
        --update) UPDATE=1 ;;
        -h|--help)
            sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            err "Unknown argument: ${arg}"
            err "Usage: $(basename "${BASH_SOURCE[0]}") [--update]"
            exit 2
            ;;
    esac
done

# --- Helpers ---------------------------------------------------------------

# Extract the "Certificate data from Mozilla as of: ..." date from a PEM bundle
# and convert it to YYYY-MM-DD (UTC). Prints the date on stdout, or returns
# non-zero if the header line is missing or unparsable.
extract_bundle_date() {
    local file="$1"
    local label="$2"
    local header_line date_str date_utc

    header_line="$(grep -m1 '^## Certificate data from Mozilla as of:' "${file}" || true)"
    if [[ -z "${header_line}" ]]; then
        err "Could not find 'Certificate data from Mozilla as of:' header in ${label}"
        return 1
    fi

    date_str="${header_line#*as of: }"
    if ! date_utc="$(date -u -d "${date_str}" +%Y-%m-%d 2>/dev/null)"; then
        err "Failed to parse date string from ${label}: '${date_str}'"
        return 1
    fi

    printf '%s\n' "${date_utc}"
}

# --- Locate the local bundle ------------------------------------------------

if [[ ! -f "${LOCAL_FILE}" ]]; then
    err "Local CA bundle not found: ${LOCAL_FILE}"
    exit 2
fi

if ! local_date="$(extract_bundle_date "${LOCAL_FILE}" "${LOCAL_FILE}")"; then
    exit 2
fi
info "Local bundle:  ${LOCAL_FILE} (date: ${local_date})"

# --- Download the remote bundle --------------------------------------------

tmpfile="$(mktemp)"
trap 'rm -f "${tmpfile}"' EXIT

if ! curl --silent --show-error --fail --location \
        --retry 3 --retry-delay 5 --max-time 60 \
        -o "${tmpfile}" "${REMOTE_URL}"; then
    err "Failed to download ${REMOTE_URL}"
    exit 2
fi

if ! remote_date="$(extract_bundle_date "${tmpfile}" "${REMOTE_URL}")"; then
    exit 2
fi
info "Remote bundle: ${REMOTE_URL} (date: ${remote_date})"

# --- Compare ---------------------------------------------------------------

if [[ ! "${local_date}" < "${remote_date}" ]]; then
    info "OK: local CA bundle is up-to-date (>= remote)."
    exit 0
fi

if [[ "${UPDATE}" -eq 1 ]]; then
    info "Updating ${LOCAL_FILE} (${local_date} -> ${remote_date})..."
    install -m 0644 "${tmpfile}" "${LOCAL_FILE}"
    info "OK: local CA bundle updated to ${remote_date}."
    exit 0
fi

cat >&2 <<EOF

================================================================================
A newer Mozilla CA certificate bundle is available!

  Local  : ${LOCAL_FILE}        (${local_date})
  Remote : ${REMOTE_URL}  (${remote_date})

To update, run:
  scripts/$(basename "${BASH_SOURCE[0]}") --update

…or download cacert.pem from https://curl.se/docs/caextract.html and replace
${LOCAL_FILE} manually.
================================================================================
EOF
exit 1

