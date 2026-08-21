#!/usr/bin/env bash

# Identify which trusted Ruuvi Gateway key signed an ESP32 Secure Boot V2 image.
#
# The trusted public keys are loaded from the signing_keys directory next to
# this script, so the command can be run from any working directory.

set -euo pipefail

readonly SCRIPT_NAME="${0##*/}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
readonly DEV_KEY="${SCRIPT_DIR}/signing_keys/signing_key_pub-dev.pem"
readonly PROD_KEY="${SCRIPT_DIR}/signing_keys/signing_key_pub-prod.pem"

usage()
{
    cat <<EOF
Usage: ${SCRIPT_NAME} FIRMWARE

Verify an ESP32 Secure Boot V2 firmware image and report whether it was signed
with the trusted Ruuvi Gateway development or production key.

Arguments:
  FIRMWARE    Path to the signed application or bootloader binary.

Options:
  -h, --help  Show this help message and exit.

The public keys are read from:
  ${DEV_KEY}
  ${PROD_KEY}

Example:
  ${SCRIPT_NAME} build/ruuvi_gateway_esp.bin
EOF
}

error()
{
    echo "${SCRIPT_NAME}: error: $*" >&2
}

if [[ $# -eq 1 ]] && [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 1 ]]; then
    error "expected exactly one firmware path"
    echo >&2
    usage >&2
    exit 2
fi

readonly FIRMWARE="$1"

if [[ ! -f "${FIRMWARE}" ]]; then
    error "firmware file not found: ${FIRMWARE}"
    exit 2
fi

if [[ ! -r "${FIRMWARE}" ]]; then
    error "firmware file is not readable: ${FIRMWARE}"
    exit 2
fi

for key in "${DEV_KEY}" "${PROD_KEY}"; do
    if [[ ! -r "${key}" ]]; then
        error "public key is missing or not readable: ${key}"
        exit 2
    fi
done

if ! command -v espsecure.py >/dev/null 2>&1; then
    error "espsecure.py was not found; activate the ESP-IDF environment first"
    exit 2
fi

if espsecure.py verify_signature --version 2 \
    --keyfile "${DEV_KEY}" "${FIRMWARE}" >/dev/null 2>&1; then
    echo "DEV: firmware signature is valid"
elif espsecure.py verify_signature --version 2 \
    --keyfile "${PROD_KEY}" "${FIRMWARE}" >/dev/null 2>&1; then
    echo "PROD: firmware signature is valid"
else
    error "firmware is unsigned, corrupted, or signed with an unknown key"
    exit 1
fi
