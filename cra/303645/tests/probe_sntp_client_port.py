"""Send one safe NTP client request to a suspected SNTP client UDP port.

The gateway's SNTP client does not answer this request. Confirm delivery using
the temporary sntp_port_diag UART log in the diagnostic firmware build.
"""

from __future__ import annotations

import argparse
import ipaddress
import socket
import struct
import time

NTP_UNIX_EPOCH_OFFSET: int = 2_208_988_800
NANOSECONDS_PER_SECOND: int = 1_000_000_000


def build_ntp_client_request(now_ns: int) -> bytes:
    """Construct a 48-byte NTPv4 client-mode request, never a server response."""
    packet: bytearray = bytearray(48)
    packet[0] = 0x23  # LI=0, version=4, mode=3 (client).
    packet[2] = 6     # Poll interval: 2**6 seconds.
    packet[3] = 0xEC  # Precision: signed -20.
    seconds: int = now_ns // NANOSECONDS_PER_SECOND + NTP_UNIX_EPOCH_OFFSET
    fraction: int = ((now_ns % NANOSECONDS_PER_SECOND) << 32) // NANOSECONDS_PER_SECOND
    struct.pack_into("!II", packet, 40, seconds, fraction)
    return bytes(packet)


def main() -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gateway_ipv4", type=ipaddress.IPv4Address)
    parser.add_argument("port", type=int, help="current open|filtered UDP port")
    args: argparse.Namespace = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")

    packet: bytes = build_ntp_client_request(time.time_ns())
    destination: tuple[str, int] = (str(args.gateway_ipv4), args.port)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
        udp_socket.connect(destination)
        sent: int = udp_socket.send(packet)
        source: tuple[str, int] = udp_socket.getsockname()
    print(f"Sent {sent}-byte NTPv4 client request from UDP/{source[1]} to {destination[0]}:{destination[1]}")
    print(f"Check gateway UART for: sntp_port_diag: recv on UDP/{destination[1]} "
          f"from {source[0]}:{source[1]}, length=48")
    print("No network reply is expected from an SNTP client; this probe cannot resolve open|filtered.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
