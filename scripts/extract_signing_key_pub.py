#!/usr/bin/env python

"""Extract the public key embedded in an ESP32 Secure Boot V2 image.

Secure Boot V2 signature blocks contain the RSA public key used to sign the
image. Extracting and verifying that key proves that the image is internally
consistent; it does not establish that the key is trusted. Compare the output
with a public key obtained from a trusted source before identifying an image as
a development or production image.
"""

import argparse
import hashlib
import struct
import sys
import zlib
from pathlib import Path

try:
    from cryptography import exceptions
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding, rsa, utils
except ImportError as error:
    raise SystemExit(
        "error: missing Python dependency 'cryptography'; activate the ESP-IDF "
        "environment or install it with: python -m pip install cryptography"
    ) from error

SECTOR_SIZE = 4096
SIGNATURE_BLOCK_SIZE = 1216
SIGNATURE_DATA_SIZE = 1196
SIGNATURE_MAGIC = 0xE7
SIGNATURE_VERSION = 0x02
RSA_KEY_SIZE_BITS = 3072


class ExtractionError(Exception):
    """Indicate that a public key cannot be safely extracted from an image."""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Extract the RSA public key embedded in the first ESP32 Secure "
            "Boot V2 signature block."
        ),
        epilog=(
            "example:\n"
            "  extract_signing_key_pub.py firmware.bin firmware-public.pem\n\n"
            "The extracted key is supplied by the image itself and is not "
            "automatically trusted. Compare it with a public key obtained "
            "from a trusted source."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "firmware",
        type=Path,
        metavar="FIRMWARE",
        help="signed application or bootloader binary",
    )
    parser.add_argument(
        "output",
        type=Path,
        metavar="PUBLIC_KEY",
        help="destination for the extracted PEM public key",
    )
    return parser.parse_args()


def read_firmware(path: Path) -> bytes:
    try:
        image = path.read_bytes()
    except OSError as error:
        raise ExtractionError(f"cannot read '{path}': {error}") from error

    if len(image) < SECTOR_SIZE or len(image) % SECTOR_SIZE != 0:
        raise ExtractionError(
            "invalid Secure Boot V2 image size: expected a non-empty multiple "
            f"of {SECTOR_SIZE} bytes"
        )
    return image


def extract_public_key(image: bytes) -> rsa.RSAPublicKey:
    block = image[-SECTOR_SIZE : -SECTOR_SIZE + SIGNATURE_BLOCK_SIZE]
    if block[0] != SIGNATURE_MAGIC or block[1] != SIGNATURE_VERSION:
        raise ExtractionError(
            "ESP32 Secure Boot V2 signature block not found in the last sector"
        )

    expected_crc = struct.unpack_from("<I", block, SIGNATURE_DATA_SIZE)[0]
    actual_crc = zlib.crc32(block[:SIGNATURE_DATA_SIZE]) & 0xFFFFFFFF
    if actual_crc != expected_crc:
        raise ExtractionError("invalid Secure Boot V2 signature block CRC")

    image_digest = hashlib.sha256(image[:-SECTOR_SIZE]).digest()
    stored_digest = block[4:36]
    if image_digest != stored_digest:
        raise ExtractionError("signature block digest does not match the firmware")

    modulus = int.from_bytes(block[36:420], "little")
    exponent = struct.unpack_from("<I", block, 420)[0]
    try:
        public_key = rsa.RSAPublicNumbers(exponent, modulus).public_key(
            default_backend()
        )
    except ValueError as error:
        raise ExtractionError(f"invalid RSA public key: {error}") from error

    if public_key.key_size != RSA_KEY_SIZE_BITS:
        raise ExtractionError(
            f"invalid RSA key size: expected {RSA_KEY_SIZE_BITS} bits, "
            f"got {public_key.key_size}"
        )

    signature = block[812:1196][::-1]
    try:
        public_key.verify(
            signature,
            image_digest,
            padding.PSS(
                mgf=padding.MGF1(hashes.SHA256()),
                salt_length=32,
            ),
            utils.Prehashed(hashes.SHA256()),
        )
    except exceptions.InvalidSignature as error:
        raise ExtractionError(
            "firmware signature is not valid for the embedded public key"
        ) from error

    return public_key


def write_public_key(path: Path, public_key: rsa.RSAPublicKey) -> None:
    pem = public_key.public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    try:
        path.write_bytes(pem)
    except OSError as error:
        raise ExtractionError(f"cannot write '{path}': {error}") from error


def main() -> int:
    args = parse_args()
    try:
        image = read_firmware(args.firmware)
        public_key = extract_public_key(image)
        write_public_key(args.output, public_key)
    except ExtractionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"Public key written to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
