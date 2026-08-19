# Firmware signing public keys

This directory contains the trusted RSA public keys used to identify Ruuvi
Gateway firmware signed with ESP32 Secure Boot V2:

- `signing_key_pub-dev.pem` - development signing key
- `signing_key_pub-prod.pem` - production signing key

Only public keys belong in this directory. Never commit private signing keys.

## Generate a public key from a private key

Generate a PEM public key from an existing RSA private key:

```bash
openssl pkey \
  -in secure_boot_signing_key.pem \
  -pubout \
  -out secure_boot_signing_key_pub.pem
```

For use by `check_fw_signature.sh`, save the resulting trusted key as either
`signing_key_pub-dev.pem` or `signing_key_pub-prod.pem` in this directory.

## Verify firmware against a specified private key

### Compare with a trusted repository key

The easiest method is to verify the firmware with `check_fw_signature.sh`, then
confirm that the specified private key corresponds to the trusted public key
reported by the script.

```bash
source ~/esp-idf-env.sh

scripts/check_fw_signature.sh build/ruuvi_gateway_esp.bin
# Expected output for the production key:
# PROD: firmware signature is valid

temporary_public_key="$(mktemp)"
trap 'rm -f "${temporary_public_key}"' EXIT

openssl pkey \
  -in /path/to/secure_boot_signing_key.pem \
  -pubout \
  -out "${temporary_public_key}"

cmp "${temporary_public_key}" \
  scripts/signing_keys/signing_key_pub-prod.pem
```

If `check_fw_signature.sh` reports `PROD` and `cmp` succeeds, the firmware was
signed with the specified production private key. For a development key,
expect `DEV: firmware signature is valid` and compare against
`signing_key_pub-dev.pem` instead.

`cmp` exits silently with status `0` when the public keys are identical. If PEM
formatting differs, compare their canonical DER encodings instead:

```bash
cmp \
  <(openssl pkey -pubin -in "${temporary_public_key}" -outform DER) \
  <(openssl pkey -pubin \
      -in scripts/signing_keys/signing_key_pub-prod.pem \
      -outform DER)
```

### Verify directly with `espsecure.py`

Alternatively, derive a temporary public key from the specified private key and
pass it directly to `espsecure.py`:

```bash
source ~/esp-idf-env.sh

temporary_public_key="$(mktemp)"
trap 'rm -f "${temporary_public_key}"' EXIT

openssl pkey \
  -in /path/to/secure_boot_signing_key.pem \
  -pubout \
  -out "${temporary_public_key}"

espsecure.py verify_signature \
  --version 2 \
  --keyfile "${temporary_public_key}" \
  build/ruuvi_gateway_esp.bin
```

The command prints a successful verification message and exits with status `0`
only when the firmware has a valid signature from the corresponding private
key. A nonzero status means that the firmware is corrupted, has an invalid
signature, or was signed with a different key.

Do not copy private keys into this repository or pass them to scripts that only
need public keys.

## Identify a firmware signing key

Activate the ESP-IDF environment so that `espsecure.py` is available, then run:

```bash
source ~/esp-idf-env.sh
scripts/check_fw_signature.sh build/ruuvi_gateway_esp.bin
```

The script cryptographically verifies the firmware against both trusted public
keys and prints one of:

```text
DEV: firmware signature is valid
PROD: firmware signature is valid
```

It exits with status `0` when a trusted key matches, status `1` when the
firmware is unsigned, corrupted, or signed with an unknown key, and status `2`
for invalid arguments, missing files, or a missing `espsecure.py` command. The
key paths are resolved relative to the script, so it can be run from any
working directory.

See the complete command help with:

```bash
scripts/check_fw_signature.sh --help
```

## Extract the public key embedded in firmware

An ESP32 Secure Boot V2 signature block contains the signer's RSA public key.
Extract it to a PEM file with:

```bash
scripts/extract_signing_key_pub.py \
  build/ruuvi_gateway_esp.bin \
  firmware-public.pem
```

The script validates the signature block CRC, firmware digest, RSA-3072 key,
and RSA-PSS signature before writing the public key. It requires the Python
`cryptography` package, which is available in the ESP-IDF environment used by
this project.

See the complete command help with:

```bash
scripts/extract_signing_key_pub.py --help
```

### Trust limitation

Extraction proves only that the firmware is internally consistent with its
embedded key. It does not prove that the key is an authorized development or
production key, because an untrusted signer can embed its own key. Use
`check_fw_signature.sh` or compare the extracted key with a public key obtained
from a trusted source:

```bash
cmp firmware-public.pem scripts/signing_keys/signing_key_pub-prod.pem
```
