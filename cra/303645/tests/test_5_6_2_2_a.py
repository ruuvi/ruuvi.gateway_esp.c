"""ETSI 5.6-2-2 Unit A: unauthenticated LAN disclosure assessment."""

from __future__ import annotations

import base64
import json
import os
import re
import secrets
import select
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, replace
from datetime import datetime
from html.parser import HTMLParser
from http.cookies import SimpleCookie
from pathlib import Path
from typing import Any, BinaryIO, Callable
from urllib.parse import ParseResult, urljoin, urlparse

import requests
from Crypto.PublicKey import ECC

from lib.config import (
    AUTHENTICATION_DEFAULT_FIELDS,
    FACTORY_RESET_MESSAGE,
    OCTETS_6_RE,
    default_config_values,
    load_dut_config,
)
from lib.errors import GatewayAuthenticationModeError, GatewayProtocolError, InvalidSetup
from lib.evidence import AssertionEvidence, EvidenceLog, format_utc, utc_now
from lib.gateway import (
    GatewayCfgDesc,
    GatewayCfgLanAuthType,
    GatewayClient,
    InteractiveAuthResult,
    InteractiveLoginChallenge,
)
from lib.http_api import API_INVENTORY, GatewayApi, HttpHeader, HttpMethod, HttpStatus
from lib.models import DutConfig, ProgressReporter, RunResult
from lib.serial_dut import SerialPort, SerialTransport

TEST_ID: str = "ETSI EN 303 645 / ETSI TS 103 701 test case 5.6-2-2, Test Unit A"
HTTP_TIMEOUT: tuple[int, int] = (5, 15)
CAPTURE_WINDOW_SECONDS: int = 30
CAPTURE_READY_SECONDS: int = 5
CAPTURE_AFTER_MDNS_SECONDS: float = 3.0
CAPTURE_STOP_SECONDS: float = 5.0
MDNS_HOSTNAME_PATTERN: re.Pattern[str] = re.compile(r'### Start mDNS: Hostname: "([^"\r\n]+)"[^\r\n]*\r?\n')
CAPTURE_PERMISSION_INSTRUCTION: str = (
    "To grant tcpdump capture permission, run from cra/303645/tests: "
    "sudo .venv/bin/python test_5_6_2_2_a.py"
)
CAPTURE_VISIBILITY_INSTRUCTION: str = (
    "No DUT DHCP frame was observed. The live runner starts tcpdump before resetting the gateway "
    "through CH340, then waits for its boot-time DHCP exchange. Check that the USB cable controls "
    "this DUT and that the capture interface sees its LAN broadcasts; otherwise capture on the "
    "router's DUT-facing LAN, a switch mirror port, or a network TAP."
)
TOTAL_STEPS: int = 8
CHECKS: tuple[str, ...] = ("factory-default preflight", "HTTP disclosure matrix", "generic banner",
                           "redirects and challenge", "sensitive disclosure scan", "public metadata", "DHCP capture")
SECRET_FIELDS: tuple[str, ...] = (
    GatewayCfgDesc.LAN_AUTH_USER, GatewayCfgDesc.LAN_AUTH_PASS, GatewayCfgDesc.LAN_AUTH_API_KEY,
    GatewayCfgDesc.LAN_AUTH_API_KEY_RW, "wifi_sta_config.password", "wifi_ap_config.password",
    "remote_cfg_auth_bearer_token", "remote_cfg_auth_basic_pass", "http_user", "http_pass",
    "http_bearer_token", "http_api_key", "mqtt_user", "mqtt_pass", "mqtt_cli_key",
    "http_stat_pass", "stat_cli_key",
)
SECRET_FIELD_PATTERN: re.Pattern[str] = re.compile(
    r'(?i)["\']?(' + "|".join(re.escape(field) for field in SECRET_FIELDS) +
    r')["\']?\s*[:=]\s*(["\'])((?:\\.|(?!\2)[^\\])+)\2', re.DOTALL
)
HEADER_SECRET_FIELD_PATTERN: re.Pattern[str] = re.compile(
    r'(?i)(?<![\w.])(' + "|".join(re.escape(field) for field in SECRET_FIELDS) +
    r')\s*=\s*([^\s;,[\]{}"\']+)'
)
TAGS_PATTERN: re.Pattern[str] = re.compile(
    r'(?i)["\']tags["\']\s*:\s*\{\s*["\'](?:[0-9a-f]{2}:){5}[0-9a-f]{2}["\']\s*:\s*\{'
)
PEM_PATTERN: re.Pattern[str] = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")
POINTER_PATTERN: re.Pattern[str] = re.compile(r"\b0x[0-9a-fA-F]{6,}\b")
POINTER_CONTEXT_PATTERN: re.Pattern[str] = re.compile(
    r'(?i)\b(?:pointer|address|stack|heap|pc|sp)["\']?\s*[:=]\s*["\']?0x[0-9a-f]{6,}\b'
)
RUNTIME_BACKTRACE_PATTERN: re.Pattern[str] = re.compile(
    r"(?i)\b(?:backtrace|stack trace)\s*:\s*0x[0-9a-f]{6,}\s*:\s*0x[0-9a-f]{6,}\b"
)
COORDINATES_PATTERN: re.Pattern[str] = re.compile(
    r'(?is)["\']coordinates["\']\s*:\s*(["\'])((?:(?!\1).)+)\1'
)
DIAGNOSTIC_PATTERN: re.Pattern[str] = re.compile(r"(?i)\b(?:(?:backtrace|stack trace|panic)\b|abort\(\))")
BUILD_PATTERN: re.Pattern[str] = re.compile(r"(?i)\b(?:v?\d+\.\d+\.\d+(?:[-+][\w.]+)?|[0-9a-f]{40}|[0-9a-f]{7,}(?=\b)|esp-idf\s*[:/]?\s*v?\d+\.\d+)\b")
PUBLIC_CACHE_CONTROL_PATTERN: re.Pattern[str] = re.compile(
    r"(?i)\s*(?:(?:max-age|s-maxage|stale-while-revalidate|stale-if-error)\s*=\s*[0-9]+|"
    r"public|private|no-cache|no-store|must-revalidate|proxy-revalidate|immutable|no-transform)\s*"
)
BEARER_PATTERN: re.Pattern[str] = re.compile(r"(?i)\bBearer\s+[A-Za-z0-9._~+/-]+")
HISTORY_PATTERN: re.Pattern[str] = re.compile(r'(?i)["\'](?:history|sensor_data|ble_mac)["\']\s*:\s*(?:\[\s*\{|\[\s*["\']|\{\s*["\'])')
QUOTED_LITERAL: str = r'''(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')'''
WIFI_PASSWORD_PATTERN: re.Pattern[str] = re.compile(
    r"(?is)[\"']wifi_(?:sta|ap)_config[\"']\s*:\s*\{" +
    r"(?:" + QUOTED_LITERAL + r"|[^{}\"'])*?" +
    r"[\"']password[\"']\s*:\s*(?:\"(?:\\.|[^\"\\])+\"|'(?:\\.|[^'\\])+')"
)
STATIC_REF_PATTERN: re.Pattern[str] = re.compile(r'''(?:src|href)\s*=\s*["']([^"']+\.(?:js|css)(?:\?[^"']*)?)["']''', re.IGNORECASE)


@dataclass(frozen=True)
class Probe:
    method: str
    path: str
    expected_status: int
    previous_cookie: bool | None = None


@dataclass(frozen=True)
class Observation:
    probe: Probe
    headers: tuple[tuple[str, str], ...]
    body: str
    status: int
    public_header_fragments: tuple[tuple[str, str], ...] = ()
    challenge_problem: str | None = None
    public_key_problem: str | None = None


@dataclass(frozen=True)
class DisclosureFinding:
    method: str
    path: str
    location: str
    category: str
    token: str


@dataclass(frozen=True)
class CaptureResult:
    frame: str | None
    interface: str | None
    problem: str | None = None
    option_values: tuple[tuple[int, str], ...] | None = None
    raw_hex: str | None = None
    source_mac: str | None = None
    hostname: str | None = None
    ethernet_mac: str | None = None
    uart_hostname: str | None = None
    prior_frames: tuple[CaptureResult, ...] = ()
    bootp_fixed_values: tuple[tuple[str, str], ...] = ()


STANDARD_DHCP_OPTIONS: frozenset[int] = frozenset({1, 3, 6, 12, 15, 28, 50, 51, 53, 54, 55, 57, 58, 59, 61})
DHCP_HOSTNAME_PREFIX: str = "RuuviGateway"
EARLY_BOOT_HOSTNAME: str = "espressif"
def _decode_dhcp_frame(frame: bytes, interface: str) -> CaptureResult:
    """Decode one Ethernet/IPv4/UDP/BOOTP frame."""
    if len(frame) < 14 or frame[12:14] != b"\x08\x00":
        return CaptureResult(None, interface, "captured frame was not Ethernet IPv4")
    ip_offset: int = 14
    if len(frame) < ip_offset + 20 or frame[ip_offset] >> 4 != 4:
        return CaptureResult(None, interface, "captured frame was not complete IPv4")
    ip_header_length: int = (frame[ip_offset] & 0x0F) * 4
    if ip_header_length < 20 or len(frame) < ip_offset + ip_header_length + 8 or frame[ip_offset + 9] != 17:
        return CaptureResult(None, interface, "captured frame was not complete IPv4 UDP")
    ip_length: int = struct.unpack_from("!H", frame, ip_offset + 2)[0]
    ip_end: int = ip_offset + ip_length
    if ip_length < ip_header_length + 8 or ip_end > len(frame):
        return CaptureResult(None, interface, "captured IPv4 datagram length was invalid or truncated")
    udp_offset: int = ip_offset + ip_header_length
    udp_length: int = struct.unpack_from("!H", frame, udp_offset + 4)[0]
    udp_end: int = udp_offset + udp_length
    bootp_offset: int = udp_offset + 8
    if udp_length < 8 + 240 or udp_end != ip_end:
        return CaptureResult(None, interface, "captured UDP datagram length was invalid or truncated")
    if frame[bootp_offset + 236:bootp_offset + 240] != b"\x63\x82\x53\x63":
        return CaptureResult(None, interface, "captured UDP payload was not complete DHCP")
    source_mac: str = ":".join(f"{octet:02x}" for octet in frame[6:12])
    options: list[tuple[int, str]] = []
    offset: int = bootp_offset + 240
    end_seen: bool = False
    while offset < udp_end:
        code: int = frame[offset]
        offset += 1
        if code == 255:
            end_seen = True
            break
        if code == 0:
            continue
        if offset >= udp_end:
            return CaptureResult(None, interface, "truncated DHCP option length")
        length: int = frame[offset]
        offset += 1
        if offset + length > udp_end:
            return CaptureResult(None, interface, "truncated DHCP option value")
        options.append((code, frame[offset:offset + length].hex()))
        offset += length
    if not end_seen:
        return CaptureResult(None, interface, "DHCP options are missing the end marker")
    # Only zero DHCP padding may follow END inside the UDP payload. Ethernet
    # trailer bytes outside the declared datagram are not DHCP option data.
    if any(frame[offset:udp_end]):
        return CaptureResult(None, interface, "DHCP payload contains non-padding bytes after the end marker")
    hostname_values: tuple[str, ...] = tuple(
        bytes.fromhex(value).decode("ascii", errors="replace") for code, value in options if code == 12
    )
    hostname: str = hostname_values[-1] if hostname_values else ""
    codes: str = ", ".join(f"{code:d}" for code, _ in options)
    summary: str = f"{source_mac} DHCP Hostname {hostname}; option codes {codes}"
    return CaptureResult(summary, interface, option_values=tuple(options), raw_hex=frame.hex(),
                         source_mac=source_mac, hostname=hostname,
                         bootp_fixed_values=(("sname", frame[bootp_offset + 44:bootp_offset + 108].hex()),
                                             ("file", frame[bootp_offset + 108:bootp_offset + 236].hex())))


def decode_dhcp_pcap(data: bytes, interface: str) -> CaptureResult:
    """Decode all captured packets and retain the last complete DUT DHCP frame."""
    magic: bytes = data[:4]
    endian: str = "<" if magic == b"\xd4\xc3\xb2\xa1" else ">" if magic == b"\xa1\xb2\xc3\xd4" else ""
    if not endian or len(data) < 24:
        return CaptureResult(None, interface, "capture did not contain a complete pcap header")
    link_type: int = struct.unpack_from(f"{endian}I", data, 20)[0]
    if link_type != 1:
        return CaptureResult(None, interface, "capture was not Ethernet")
    offset: int = 24
    frames: list[CaptureResult] = []
    while offset < len(data):
        if len(data) - offset < 16:
            return CaptureResult(None, interface, "truncated pcap record header")
        frame_length: int = struct.unpack_from(f"{endian}I", data, offset + 8)[0]
        offset += 16
        if len(data) - offset < frame_length:
            return CaptureResult(None, interface, "truncated pcap Ethernet frame")
        frame: bytes = data[offset:offset + frame_length]
        offset += frame_length
        decoded: CaptureResult = _decode_dhcp_frame(frame, interface)
        if decoded.problem:
            return decoded
        frames.append(decoded)
    if not frames:
        return CaptureResult(None, interface, "no DUT DHCP frame in capture")
    return replace(frames[-1], prior_frames=tuple(frames[:-1]))


def _wait_for_tcpdump_ready(process: subprocess.Popen[bytes]) -> str | None:
    """Return only after tcpdump reports that its capture socket is listening."""
    if process.stderr is None:
        return "tcpdump stderr pipe is unavailable"
    deadline: float = time.monotonic() + CAPTURE_READY_SECONDS
    diagnostic: bytes = b""
    while time.monotonic() < deadline:
        remaining: float = deadline - time.monotonic()
        ready: bool = bool(select.select([process.stderr], [], [], remaining)[0])
        if not ready:
            break
        chunk: bytes = os.read(process.stderr.fileno(), 4096)
        diagnostic += chunk
        if b"listening on" in diagnostic.lower():
            return None
        if not chunk or process.poll() is not None:
            break
    detail: str = diagnostic.decode("utf-8", errors="replace").strip()
    return f"tcpdump did not become ready within {CAPTURE_READY_SECONDS}s: {detail}"


def capture_dhcp(
    hostname: str, duration: int = CAPTURE_WINDOW_SECONDS,
    rebooter: Callable[[], str] | None = None,
) -> CaptureResult:
    """Observe boot DHCP through the UART mDNS marker, then select the last frame."""
    tool: str | None = shutil.which("tcpdump")
    if not tool:
        return CaptureResult(None, None, "tcpdump is unavailable")
    route_tool: str | None = shutil.which("ip")
    if not route_tool:
        return CaptureResult(None, None, "ip route tool is unavailable")
    interface: str | None = None
    ethernet_mac: str | None = None
    packet: subprocess.CompletedProcess[bytes] | None = None
    uart_hostname: str | None = None
    try:
        dut_ipv4: str = socket.gethostbyname(hostname)
        route: subprocess.CompletedProcess[str] = subprocess.run(
            [route_tool, "route", "get", dut_ipv4], capture_output=True, text=True, timeout=5, check=False,
        )
        interface_match: re.Match[str] | None = re.search(r"\bdev\s+(\S+)", route.stdout)
        if route.returncode != 0 or interface_match is None:
            return CaptureResult(None, None, f"no LAN capture interface: {route.stderr.strip()}")
        interface: str = interface_match.group(1)
        if re.search(r"\bvia\s+\S+", route.stdout):
            return CaptureResult(None, interface, "DUT IPv4 is reached through a router, not on this L2 segment")
        neighbor: subprocess.CompletedProcess[str] = subprocess.run(
            [route_tool, "-4", "neigh", "show", "to", dut_ipv4, "dev", interface],
            capture_output=True, text=True, timeout=5, check=False,
        )
        neighbor_match: re.Match[str] | None = re.search(
            r"\blladdr\s+((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})\b", neighbor.stdout,
        )
        if neighbor.returncode != 0 or neighbor_match is None:
            return CaptureResult(None, interface, "cannot identify DUT Ethernet MAC in on-link neighbor table: "
                                 f"{neighbor.stderr.strip()}")
        ethernet_mac = neighbor_match.group(1)
        command: list[str] = [tool, "-i", interface, "-n", "-s", "0", "-U", "-w", "-",
                              "udp and (port 67 or port 68) and ether src", ethernet_mac]
        if rebooter is None:
            packet = subprocess.run(
                command[:6] + ["-c", "1"] + command[6:], capture_output=True, timeout=duration, check=False,
            )
        else:
            packet_file: BinaryIO
            with tempfile.TemporaryFile() as packet_file:
                process: subprocess.Popen[bytes] = subprocess.Popen(
                    command, stdout=packet_file, stderr=subprocess.PIPE,
                )
                try:
                    readiness_problem: str | None = _wait_for_tcpdump_ready(process)
                    if readiness_problem is not None:
                        return CaptureResult(None, interface, readiness_problem, ethernet_mac=ethernet_mac)
                    try:
                        uart_hostname = rebooter()
                    except Exception as error:  # noqa: BLE001 - Reset/UART errors are capture preconditions.
                        error: Exception
                        return CaptureResult(None, interface,
                                             f"gateway reboot or UART observation failed: {type(error).__name__}: {error}",
                                             ethernet_mac=ethernet_mac)
                    if not uart_hostname:
                        return CaptureResult(None, interface,
                                             f"UART mDNS hostname marker not observed within {duration}s after reboot",
                                             ethernet_mac=ethernet_mac)
                    time.sleep(CAPTURE_AFTER_MDNS_SECONDS)
                    if process.poll() is not None:
                        stderr: bytes = process.communicate()[1]
                        return CaptureResult(None, interface,
                                             f"tcpdump exited before UART mDNS marker (exit {process.returncode}): "
                                             f"{stderr.decode('utf-8', errors='replace').strip()}",
                                             ethernet_mac=ethernet_mac)
                    process.terminate()
                    try:
                        stderr = process.communicate(timeout=CAPTURE_STOP_SECONDS)[1]
                    except subprocess.TimeoutExpired:
                        process.kill()
                        stderr = process.communicate()[1]
                    packet_file.seek(0)
                    packet = subprocess.CompletedProcess(command, process.returncode, packet_file.read(), stderr)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()
    except subprocess.TimeoutExpired as error:
        if ethernet_mac is None:
            return CaptureResult(None, interface, f"DUT route or neighbor lookup timed out: {error}")
        return CaptureResult(None, interface,
                             f"no complete DUT DHCP frame within {duration}s: {error}",
                             ethernet_mac=ethernet_mac)
    except OSError as error:
        return CaptureResult(None, interface, f"capture setup or launch failed: {error}")
    if packet is None:
        return CaptureResult(None, interface, "capture finished without a tcpdump result", ethernet_mac=ethernet_mac)
    if packet.returncode != 0 and rebooter is None:
        diagnostic: str = packet.stderr.decode("utf-8", errors="replace").strip()
        return CaptureResult(None, interface, f"tcpdump capture failed (exit {packet.returncode}): {diagnostic}",
                             ethernet_mac=ethernet_mac)
    if not packet.stdout:
        return CaptureResult(None, interface, f"no DUT DHCP frame within {duration}s", ethernet_mac=ethernet_mac)
    decoded_capture: CaptureResult = decode_dhcp_pcap(packet.stdout, interface)
    return replace(decoded_capture, ethernet_mac=ethernet_mac, uart_hostname=uart_hostname)


def capture_dhcp_after_reset(
    hostname: str, duration: int, evidence: EvidenceLog,
    output: Callable[[str], None] | None = None,
) -> CaptureResult:
    """Discover the attached CH340, then capture boot DHCP around a bounded serial reset."""
    transport: SerialTransport = SerialTransport()
    try:
        transport.preflight()
        port: SerialPort = transport.discover()
    except (InvalidSetup, ModuleNotFoundError) as error:
        return CaptureResult(None, None, f"cannot prepare CH340 reboot: {type(error).__name__}: {error}")
    evidence.write("DHCP CAPTURE SERIAL PORT", port)

    def reboot() -> str:
        evidence.write("DHCP CAPTURE RESET START", port)
        if output is not None:
            output("DHCP capture ready; rebooting gateway through CH340")
        transport.reset(port, evidence)
        def configured_mdns_seen(text: str) -> bool:
            matches_in_console: tuple[re.Match[str], ...] = tuple(MDNS_HOSTNAME_PATTERN.finditer(text))
            return any(marker.group(1).casefold() != EARLY_BOOT_HOSTNAME for marker in matches_in_console)

        console: str = transport.observe(port, evidence, duration, stop_when=configured_mdns_seen)
        markers: tuple[re.Match[str], ...] = tuple(
            marker for marker in MDNS_HOSTNAME_PATTERN.finditer(console)
            if marker.group(1).casefold() != EARLY_BOOT_HOSTNAME
        )
        if not markers:
            return ""
        uart_hostname: str = markers[-1].group(1)
        evidence.write("DHCP CAPTURE UART MDNS HOSTNAME", uart_hostname)
        return uart_hostname

    return capture_dhcp(hostname, duration, reboot)


def probes(static_path: str, unknown_path: str) -> tuple[Probe, ...]:
    inventory: tuple[str, ...] = tuple(route.path for route in API_INVENTORY if route.method == HttpMethod.GET)
    expected: set[str] = {GatewayApi.AUTH, GatewayApi.AP, GatewayApi.STATUS, GatewayApi.CONFIG,
                          GatewayApi.FIRMWARE_UPDATE, GatewayApi.INFO, GatewayApi.METRICS,
                          GatewayApi.HISTORY, GatewayApi.VALIDATE_URL, GatewayApi.EXTRA_CFG}
    if len(inventory) != len(expected) or set(inventory) != expected:
        raise InvalidSetup("canonical GET inventory changed; review disclosure coverage")
    routes: list[Probe] = [Probe(HttpMethod.GET, "/", HttpStatus.C_200_OK),
                           Probe(HttpMethod.GET, static_path, HttpStatus.C_200_OK)]
    path: str
    for path in inventory:
        if path == GatewayApi.AUTH:
            routes.append(Probe(HttpMethod.GET, path, HttpStatus.C_401_UNAUTHORIZED))
        else:
            routes.append(Probe(HttpMethod.GET, path, HttpStatus.C_302_FOUND,
                                path not in {GatewayApi.AP, GatewayApi.STATUS}))
    routes.extend((Probe(HttpMethod.GET, unknown_path, HttpStatus.C_302_FOUND, True),
                   Probe("HEAD", "/", 400)))
    return tuple(routes)


class PublicHtmlText(HTMLParser):
    """Collect rendered text without treating public JavaScript expressions as credentials."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.fragments: list[str] = []
        self.hidden_element: str | None = None
        self.credential_values: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag in {"script", "style"}:
            self.hidden_element = tag
        if tag == "input" and self.hidden_element is None:
            attributes: dict[str, str | None] = dict(attrs)
            identifiers: tuple[str, ...] = tuple((attributes.get(key) or "").casefold() for key in ("name", "id"))
            value: str = attributes.get("value") or ""
            if value and any(identifier in SECRET_FIELDS for identifier in identifiers):
                self.credential_values.append(value)

    def handle_endtag(self, tag: str) -> None:
        if tag == self.hidden_element:
            self.hidden_element = None

    def handle_data(self, data: str) -> None:
        if self.hidden_element is None:
            self.fragments.append(data)


def scan_observation(
    observation: Observation, gw_id: str, default_username: str, public_base_url: str = "",
) -> tuple[DisclosureFinding, ...]:
    findings: list[DisclosureFinding] = []
    seed_pattern: re.Pattern[str] = re.compile(
        r"(?<![0-9a-f])(?:" + re.escape(gw_id) + "|" + re.escape(gw_id.replace(":", "")) + r")(?![0-9a-f])",
        re.IGNORECASE,
    )
    parts: list[tuple[str, str]] = [(f"header:{name}", value) for name, value in observation.headers]
    parts.append(("body", observation.body))
    media_type: str = next((value.split(";", 1)[0].strip().lower()
                            for name, value in observation.headers if name.lower() == "content-type"), "")
    rendered_body: bool = media_type in {"text/html", "text/plain"}
    executable_body: bool = media_type in {
        "application/javascript", "text/javascript", "application/ecmascript", "text/ecmascript", "text/css",
    } or (not media_type and observation.probe.path.endswith((".js", ".css")))
    if media_type == "text/html":
        html_text: PublicHtmlText = PublicHtmlText()
        html_text.feed(observation.body)
        html_text.close()
        parts.append(("rendered body", "".join(html_text.fragments)))
        form_value: str
        for form_value in html_text.credential_values:
            findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                              "body", "credential form value", form_value))
    elif media_type == "text/plain":
        parts.append(("rendered body", observation.body))
    location: str
    content: str
    for location, content in parts:
        patterns: tuple[tuple[str, re.Pattern[str]], ...] = (
            ("private key", PEM_PATTERN),
            ("bearer token", BEARER_PATTERN),
            ("personal telemetry", HISTORY_PATTERN), ("sensor tags", TAGS_PATTERN),
            ("coordinates", COORDINATES_PATTERN),
            ("Wi-Fi PSK", WIFI_PASSWORD_PATTERN),
        )
        if location != "body" or not rendered_body:
            patterns += (("runtime backtrace", RUNTIME_BACKTRACE_PATTERN),)
            if location == "body" and observation.status == HttpStatus.C_200_OK:
                patterns += (("memory address", POINTER_CONTEXT_PATTERN),)
            else:
                patterns += (("memory address", POINTER_PATTERN),)
        if location.startswith("header:") or (location == "body" and observation.status != HttpStatus.C_200_OK):
            patterns += (("build identifier", BUILD_PATTERN),)
        if location.startswith("header:") or location == "rendered body" or (
            location == "body" and not rendered_body and (
                not executable_body or observation.status != HttpStatus.C_200_OK
            )
        ):
            patterns += (("diagnostic", DIAGNOSTIC_PATTERN),)
        category: str
        pattern: re.Pattern[str]
        for category, pattern in patterns:
            hit: re.Match[str]
            scanned_content: str = content
            if category in {"build identifier", "diagnostic", "memory address"}:
                public_location: str
                fragment: str
                for public_location, fragment in observation.public_header_fragments:
                    if location.lower() == public_location:
                        scanned_content = scanned_content.replace(fragment, "", 1)
            if category == "build identifier" and location.lower() == "header:location" and (
                public_base_url and content.startswith(public_base_url + "/")
            ):
                # Only the verified redirect authority is public; retain path/query/fragment leaks.
                scanned_content = content[len(public_base_url):]
            if category == "build identifier" and (
                (location.lower() == "header:content-length" and re.fullmatch(r"[0-9]+", content)) or
                (location.lower() == "header:cache-control" and all(
                    PUBLIC_CACHE_CONTROL_PATTERN.fullmatch(directive) for directive in content.split(",")
                ))
            ):
                # Exempt only complete standard numeric metadata, never appended build/custom fields.
                scanned_content = ""
            for hit in pattern.finditer(scanned_content):
                if category == "bearer token" and location in {"body", "rendered body"}:
                    candidate: str = hit.group(0).split()[-1]
                    context: str = content[max(0, hit.start() - 24):hit.start()].lower()
                    if candidate.isalpha() and len(candidate) < 24 and "authorization" not in context:
                        # Public UI prose such as "Bearer authentication" is not a credential.
                        continue
                findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                  location, category, hit.group(0)))
        if gw_id and seed_pattern.search(content):
            findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                              location, "DeviceID password seed", gw_id))
        if location.startswith("header:"):
            header_name: str = location[len("header:"):].lower().replace("-", "_")
            if header_name in SECRET_FIELDS and content.strip():
                findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                  location, "credential value", content))
        if location.startswith("header:") or location == "rendered body":
            bare_match: re.Match[str]
            for bare_match in HEADER_SECRET_FIELD_PATTERN.finditer(content):
                findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                  location, "credential value", bare_match.group(0)))
        # Decode JSON containers and strings embedded in a public asset or rendered dump.
        # This handles escaped property names/values without interpreting JavaScript expressions.
        decoder: json.JSONDecoder = json.JSONDecoder()
        opening: re.Match[str]
        for opening in re.finditer(r'[\[{"]', content):
            try:
                decoded: Any
                _end: int
                decoded, _end = decoder.raw_decode(content, opening.start())
            except json.JSONDecodeError:
                continue
            normalized: str = json.dumps(decoded, ensure_ascii=False)
            if gw_id and seed_pattern.search(normalized):
                findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                  location, "DeviceID password seed", gw_id))
            if isinstance(decoded, dict):
                # Canonical serialization exposes escaped keys at every nesting depth while
                # retaining value types: empty defaults and JSON schemas remain public.
                decoded_patterns: tuple[tuple[str, re.Pattern[str]], ...] = (
                    ("Wi-Fi PSK", WIFI_PASSWORD_PATTERN), ("personal telemetry", HISTORY_PATTERN),
                    ("sensor tags", TAGS_PATTERN), ("coordinates", COORDINATES_PATTERN),
                    ("private key", PEM_PATTERN), ("bearer token", BEARER_PATTERN),
                )
                decoded_category: str
                decoded_pattern: re.Pattern[str]
                for decoded_category, decoded_pattern in decoded_patterns:
                    decoded_hit: re.Match[str]
                    for decoded_hit in decoded_pattern.finditer(normalized):
                        if decoded_category == "bearer token":
                            decoded_candidate: str = decoded_hit.group(0).split()[-1]
                            decoded_context: str = normalized[max(0, decoded_hit.start() - 24):decoded_hit.start()].lower()
                            if decoded_candidate.isalpha() and len(decoded_candidate) < 24 and "authorization" not in decoded_context:
                                continue
                        findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                          location, decoded_category, decoded_hit.group(0)))
                decoded_field: re.Match[str]
                for decoded_field in SECRET_FIELD_PATTERN.finditer(normalized):
                    if not (
                        location == "body" and observation.probe.path.endswith((".js", ".css"))
                        and decoded_field.group(1) == GatewayCfgDesc.LAN_AUTH_USER
                        and decoded_field.group(3) == default_username
                    ):
                        findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                          location, "credential value", decoded_field.group(0)))
        field_match: re.Match[str]
        for field_match in SECRET_FIELD_PATTERN.finditer(content):
            field: str = field_match.group(1)
            value: str = field_match.group(3)
            public_ui_default: bool = (
                location == "body" and observation.probe.path.endswith((".js", ".css"))
                and field == GatewayCfgDesc.LAN_AUTH_USER and value == default_username
            )
            if not public_ui_default:
                findings.append(DisclosureFinding(observation.probe.method, observation.probe.path,
                                                  location, "credential value", field_match.group(0)))
    return tuple(findings)


class FunctionalTest_5_6_2_2_a:
    def __init__(
        self, config: DutConfig, evidence: EvidenceLog,
        session_factory: Callable[[], requests.Session] = requests.Session,
        random_bytes: Callable[[int], bytes] = secrets.token_bytes,
        ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
        capturer: Callable[[str, int], CaptureResult] | None = None,
        clock: Callable[[], datetime] = utc_now,
        progress: Callable[[str], None] | None = None,
        output: Callable[[str], None] = print,
    ) -> None:
        self.config: DutConfig = config
        self.evidence: EvidenceLog = evidence
        self.gateway: GatewayClient = GatewayClient(config, evidence, session_factory=session_factory,
                                                    random_bytes=random_bytes, ecc_generate=ecc_generate,
                                                    timeout=HTTP_TIMEOUT)
        self.capturer: Callable[[str, int], CaptureResult] = (
            capturer if capturer is not None else
            lambda hostname, duration: capture_dhcp_after_reset(hostname, duration, evidence, self.output)
        )
        self.clock: Callable[[], datetime] = clock
        self.random_bytes: Callable[[int], bytes] = random_bytes
        self.progress: Callable[[str], None] = progress or (lambda message: None)
        self.output: Callable[[str], None] = output
        self.outcomes: dict[str, str] = {check: "NOT RUN" for check in CHECKS}
        self.reset_advice: bool = False
        self.observations: list[Observation] = []
        self.findings: list[DisclosureFinding] = []
        self.default_username: str | None = None
        self.verified_mac: str | None = None

    def _assert(self, check: str, condition: bool, description: str, actual: Any) -> None:
        self.evidence.write("ASSERTION", AssertionEvidence(description, "PASS" if condition else "FAIL", actual))
        if not condition:
            self.outcomes[check] = "FAIL"
            self.output(f"{check} FAIL: {description}")

    def _preflight(self) -> None:
        try:
            login: InteractiveAuthResult = self.gateway.authenticate_interactive("Admin", self.config.gw_id)
        except GatewayAuthenticationModeError:
            self.reset_advice = True
            raise
        if login.login_response.status_code != HttpStatus.C_200_OK:
            self.reset_advice = True
            raise InvalidSetup(f"default login returned {login.login_response.status_code}")
        if login.auth_payload.get(GatewayCfgDesc.LAN_AUTH_TYPE) != GatewayCfgLanAuthType.DEFAULT:
            self.reset_advice = True
            raise InvalidSetup("GET /auth reported a non-default authentication mode")
        response: requests.Response = self.gateway.request(login.session, HttpMethod.GET, GatewayApi.CONFIG)
        if response.status_code != HttpStatus.C_200_OK:
            raise InvalidSetup(f"authenticated GET /ruuvi.json returned {response.status_code}")
        payload: dict[str, Any] = self.gateway.response_json(response, "GET /ruuvi.json", dict)
        mac: Any = payload.get(GatewayCfgDesc.GW_MAC)
        if mac is not None and (
            not isinstance(mac, str) or OCTETS_6_RE.fullmatch(mac) is None or
            mac.replace(":", "").lower() != self.config.gw_mac.replace(":", "").lower()
        ):
            raise InvalidSetup("DUT MAC identity is malformed or differs from .env")
        defaults: dict[str, Any] = default_config_values(AUTHENTICATION_DEFAULT_FIELDS)
        field: str
        expected: Any
        for field, expected in defaults.items():
            actual: Any = payload.get(field)
            if field not in payload or type(actual) is not type(expected) or actual != expected:
                self.reset_advice = mac is not None
                raise InvalidSetup(f"{field} does not match generated factory default")
        expected_user: Any = defaults[GatewayCfgDesc.LAN_AUTH_USER]
        if not isinstance(expected_user, str):
            raise InvalidSetup("generated default LAN username is not a string")
        self.verified_mac = mac if isinstance(mac, str) else None
        self.default_username = expected_user
        self.evidence.write("AUTHENTICATED PREFLIGHT", {"identity_verified": mac is not None,
                                                        "default_fields": defaults})
        login.session.close()
        self.outcomes["factory-default preflight"] = "PASS"

    def _request_probe(self, probe: Probe) -> Observation:
        session: requests.Session = self.gateway.new_session()
        public_fragments: list[tuple[str, str]] = []
        challenge_problem: str | None = None
        public_key_problem: str | None = None
        try:
            response: requests.Response = self.gateway.request(
                session, probe.method, probe.path, allow_redirects=False,
            )
            if probe.path == GatewayApi.AUTH and response.status_code == HttpStatus.C_401_UNAUTHORIZED:
                try:
                    validated: InteractiveLoginChallenge = self.gateway.interactive_login_challenge_from_response(
                        session, response, "unauthenticated GET /auth",
                    )
                    parameter: str
                    value: str
                    for parameter, value in validated.challenge.items():
                        if parameter in {"challenge", "session_id", "session_cookie"} or (
                            parameter == "realm" and value == "Ruuvi Gateway"
                        ):
                            public_fragments.append(("header:www-authenticate", f'{parameter}="{value}"'))
                    cookie_header: str = response.headers.get("Set-Cookie", "")
                    cookie_assignment: str = f"RUUVISESSION={validated.cookie}"
                    if re.search(r"(?:^|;\s*)" + re.escape(cookie_assignment) + r"(?=;|$)", cookie_header):
                        public_fragments.append(("header:set-cookie", cookie_assignment))
                    else:
                        raise GatewayProtocolError("auth response cookie header disagrees with cookie jar")
                except GatewayProtocolError as error:
                    challenge_problem = str(error)
            if probe.previous_cookie and response.status_code == HttpStatus.C_302_FOUND:
                previous_url: str = f"RUUVI_PREV_URL={probe.path}"
                if re.search(r"(?:^|;\s*)" + re.escape(previous_url) + r"(?=;|$)",
                             response.headers.get("Set-Cookie", "")):
                    public_fragments.append(("header:set-cookie", previous_url))
            public_key: str | None = response.headers.get(HttpHeader.RUUVI_ECDH_PUBLIC_KEY)
            if public_key is not None:
                try:
                    raw_key: bytes = base64.b64decode(public_key, validate=True)
                    if len(raw_key) != 65 or raw_key[0] != 4:
                        raise ValueError("unexpected uncompressed P-256 key encoding")
                    key: ECC.EccKey = ECC.construct(curve="P-256", point_x=int.from_bytes(raw_key[1:33], "big"),
                                                   point_y=int.from_bytes(raw_key[33:], "big"))
                    if key.pointQ.is_point_at_infinity():
                        raise ValueError("P-256 point at infinity")
                    public_fragments.append(("header:ruuvi-ecdh-pub-key", public_key))
                except (ValueError, TypeError):
                    # Keep invalid text in the scan; report ERROR after preserving any disclosure findings.
                    public_key_problem = "invalid gateway ECDH public key"
        finally:
            session.close()
        raw_version: int | None = getattr(response.raw, "version", None)
        version: str = "1.0" if raw_version == 10 else "1.1" if raw_version == 11 else "unknown"
        self.evidence.write("HTTP STATUS LINE", f"HTTP/{version} {response.status_code} {response.reason}")
        observation: Observation = Observation(probe, tuple(response.headers.items()), response.text,
                                               response.status_code, tuple(public_fragments), challenge_problem,
                                               public_key_problem)
        self.evidence.write("DISCLOSURE PROBE", observation)
        return observation

    def _static_path(self) -> str:
        root: Observation = self._request_probe(Probe(HttpMethod.GET, "/", HttpStatus.C_200_OK))
        self.observations.append(root)
        headers: dict[str, str] = {name.lower(): value for name, value in root.headers}
        content_type: str = headers.get(HttpHeader.CONTENT_TYPE.lower(), "")
        if root.status != HttpStatus.C_200_OK or "text/html" not in content_type.lower():
            raise GatewayProtocolError("public root is not 200 text/html")
        candidate: re.Match[str]
        for candidate in STATIC_REF_PATTERN.finditer(root.body):
            resolved: str = urljoin(self.config.base_url + "/", candidate.group(1))
            parsed: ParseResult = urlparse(resolved)
            if parsed.netloc.lower() == urlparse(self.config.base_url).netloc.lower():
                return parsed.path
        raise GatewayProtocolError("public HTML contains no same-origin JS/CSS asset")

    def _matrix(self) -> None:
        static_path: str = self._static_path()
        unknown_path: str = f"/cra-{self.random_bytes(8).hex()}.json"
        self.evidence.write("CANONICAL GET INVENTORY", [(route.method, route.path) for route in API_INVENTORY
                                                        if route.method == HttpMethod.GET])
        probe: Probe
        for probe in probes(static_path, unknown_path)[1:]:
            observation: Observation = self._request_probe(probe)
            self.observations.append(observation)
            self._assert("HTTP disclosure matrix", observation.status == probe.expected_status,
                         f"{probe.method} {probe.path} expected {probe.expected_status}", observation.status)
            if probe.expected_status in {HttpStatus.C_302_FOUND, HttpStatus.C_401_UNAUTHORIZED} and (
                observation.status == HttpStatus.C_200_OK
            ):
                self.evidence.write("NEGATIVE PROBE ABORT", f"{probe.method} {probe.path} unexpectedly authorized")
                break
        self.evidence.write("HEAD DISCREPANCY", "Firmware returns 400 for HEAD /; GET supplies the banner")
        if self.outcomes["HTTP disclosure matrix"] == "NOT RUN":
            self.outcomes["HTTP disclosure matrix"] = "PASS"

    def _banner(self) -> None:
        observation: Observation
        for observation in self.observations:
            headers: dict[str, str] = {name.lower(): value for name, value in observation.headers}
            if observation.probe.method == HttpMethod.GET:
                self._assert("generic banner", headers.get("server") == "Ruuvi Gateway",
                             f"generic Server banner on {observation.probe.path}", headers.get("server"))
        if self.outcomes["generic banner"] == "NOT RUN":
            self.outcomes["generic banner"] = "PASS"

    def _redirects(self) -> None:
        observation: Observation
        for observation in self.observations:
            headers: dict[str, str] = {name.lower(): value for name, value in observation.headers}
            if observation.probe.expected_status == HttpStatus.C_302_FOUND:
                expected_location: str = f"{self.config.base_url}/#auth"
                self._assert("redirects and challenge", headers.get("location") == expected_location,
                             f"auth Location on {observation.probe.path}", headers.get("location"))
                cookie: str = headers.get("set-cookie", "")
                expected_cookie: bool = bool(observation.probe.previous_cookie)
                parsed_cookie: SimpleCookie = SimpleCookie()
                parsed_cookie.load(cookie)
                previous_cookie: bool = "RUUVI_PREV_URL" in parsed_cookie
                correct_cookie: bool = (
                    previous_cookie and parsed_cookie["RUUVI_PREV_URL"].value == observation.probe.path
                    if expected_cookie else not previous_cookie
                )
                self._assert("redirects and challenge", correct_cookie,
                             f"previous URL cookie on {observation.probe.path}", cookie)
                self._assert("redirects and challenge", not observation.body.strip(),
                             f"empty redirect body on {observation.probe.path}", observation.body)
            if observation.probe.path == GatewayApi.AUTH and observation.probe.method == HttpMethod.GET and (
                observation.status == HttpStatus.C_401_UNAUTHORIZED
            ):
                if observation.challenge_problem is not None:
                    raise GatewayProtocolError(observation.challenge_problem)
                challenge: dict[str, str] = self.gateway.parse_interactive_challenge(
                    headers.get(HttpHeader.WWW_AUTHENTICATE.lower()),
                )
                self._assert("redirects and challenge", challenge["realm"] == "Ruuvi Gateway",
                             "ephemeral challenge has declared Ruuvi Gateway realm", challenge["realm"])
                self._assert("redirects and challenge", bool(challenge["challenge"] and challenge["session_id"]),
                             "ephemeral challenge has nonce and session id", challenge)
                self._assert("redirects and challenge", "RUUVISESSION=" in headers.get("set-cookie", ""),
                             "auth challenge has session cookie", headers.get("set-cookie"))
                self.evidence.write("PUBLIC EPHEMERAL CHALLENGE", challenge)
        if self.outcomes["redirects and challenge"] == "NOT RUN":
            self.outcomes["redirects and challenge"] = "PASS"

    def _scan(self) -> None:
        if self.default_username is None:
            raise InvalidSetup("factory-default username was not verified")
        self.evidence.write("SENSITIVE PATTERNS AND SOURCES", {
            "fields": SECRET_FIELDS, "regexes": (PEM_PATTERN.pattern, POINTER_PATTERN.pattern,
                DIAGNOSTIC_PATTERN.pattern, BUILD_PATTERN.pattern, BEARER_PATTERN.pattern,
                POINTER_CONTEXT_PATTERN.pattern,
                HISTORY_PATTERN.pattern, TAGS_PATTERN.pattern, HEADER_SECRET_FIELD_PATTERN.pattern,
                COORDINATES_PATTERN.pattern, WIFI_PASSWORD_PATTERN.pattern,
                RUNTIME_BACKTRACE_PATTERN.pattern),
            "sources": ("IXIT 10-SecParam sections WebUI/WiFi/remote/custom/bearer/DeviceID",
                        "IXIT 1-AuthMech", "IXIT 21-PersData BLE telemetry",
                        "IXIT 28-LogIntf Disclosed Information"),
        })
        observation: Observation
        for observation in self.observations:
            matches: tuple[DisclosureFinding, ...] = scan_observation(
                observation, self.config.gw_id, self.default_username, self.config.base_url,
            )
            self.evidence.write("SENSITIVE SCAN RESULT", {"method": observation.probe.method,
                "path": observation.probe.path, "findings": matches})
            self.findings.extend(matches)
        self.outcomes["sensitive disclosure scan"] = "FAIL" if self.findings else "PASS"
        finding: DisclosureFinding
        for finding in self.findings:
            self.output(f"sensitive disclosure FAIL: {finding.method} {finding.path} {finding.location} {finding.category}")
            self.evidence.write("DISCLOSURE FINDING", finding)

    def _public(self) -> None:
        observation: Observation
        for observation in self.observations:
            if observation.public_key_problem is not None:
                raise GatewayProtocolError(observation.public_key_problem)
        root: Observation = self.observations[0]
        asset: Observation = self.observations[1]
        self._assert("public metadata", root.status == 200 and asset.status == 200,
                     "public HTML and linked static asset are accessible", (root.status, asset.status))
        self.evidence.write("DECLARED PUBLIC METADATA", {
            "banner": "Ruuvi Gateway", "hostname": self.config.gw_hostname,
            "static_asset": asset.probe.path, "challenge": "ephemeral nonce/session/ECDH public key",
        })
        if self.outcomes["public metadata"] == "NOT RUN":
            self.outcomes["public metadata"] = "PASS"

    def _dhcp(self) -> None:
        if self.verified_mac is None:
            self.outcomes["DHCP capture"] = "ERROR"
            identity_problem: str = "authenticated DUT MAC identity is unavailable; capture not started"
            self.evidence.write("DHCP CAPTURE ERROR", identity_problem)
            self.output(f"DHCP capture ERROR: {identity_problem}")
            return
        result: CaptureResult = self.capturer(self.config.gw_hostname, CAPTURE_WINDOW_SECONDS)
        self.evidence.write("DHCP CAPTURE", result)
        if result.problem or not result.frame:
            self.outcomes["DHCP capture"] = "ERROR"
            self.output(f"DHCP capture ERROR: {result.problem or 'no DUT frame observed'}")
            problem: str = (result.problem or "").lower()
            if "permission" in problem or "operation not permitted" in problem:
                self.output(CAPTURE_PERMISSION_INSTRUCTION)
                self.evidence.write("DHCP CAPTURE RETRY INSTRUCTION", CAPTURE_PERMISSION_INSTRUCTION)
            if any(reason in problem for reason in (
                "no complete dut dhcp frame", "no dut dhcp frame", "not on this l2 segment",
                "cannot identify dut ethernet mac",
            )):
                self.output(CAPTURE_VISIBILITY_INSTRUCTION)
                self.evidence.write("DHCP CAPTURE VISIBILITY INSTRUCTION", CAPTURE_VISIBILITY_INSTRUCTION)
            return
        frame: str = result.frame
        if not re.search(r"(?i)\b(?:DHCP|BOOTP)\b", frame):
            self.outcomes["DHCP capture"] = "ERROR"
            self.output("DHCP capture ERROR: observed frame was not decoded as DHCP")
            return
        observed_frames: tuple[CaptureResult, ...] = result.prior_frames + (result,)
        source_matches: bool = result.ethernet_mac is not None and all(
            observed.source_mac is not None and observed.source_mac.lower() == result.ethernet_mac.lower()
            for observed in observed_frames
        )
        if not source_matches:
            self.outcomes["DHCP capture"] = "ERROR"
            self.output("DHCP capture ERROR: frame source is not the verified DUT Ethernet MAC")
            return
        nrf52_mac_octets: list[str] = self.verified_mac.split(":")
        expected_dhcp_hostname: str = (
            f"{DHCP_HOSTNAME_PREFIX}{nrf52_mac_octets[-2]}{nrf52_mac_octets[-1]}"
        )
        if result.uart_hostname is not None and result.uart_hostname.casefold() != expected_dhcp_hostname.casefold():
            self.outcomes["DHCP capture"] = "ERROR"
            self.output(
                f"DHCP capture ERROR: UART mDNS hostname {result.uart_hostname} "
                f"does not match expected {expected_dhcp_hostname}"
            )
            return
        hostname_matches: bool = (
            result.hostname.casefold() == expected_dhcp_hostname.casefold() if result.hostname is not None else
            expected_dhcp_hostname.casefold() in frame.casefold()
        )
        if not hostname_matches or (result.uart_hostname is not None and
                                    result.hostname is not None and
                                    result.hostname.casefold() != result.uart_hostname.casefold()):
            self.outcomes["DHCP capture"] = "ERROR"
            self.output(
                f"DHCP capture ERROR: last Option 12 hostname {result.hostname or '<missing>'} "
                f"does not match UART/expected {result.uart_hostname or expected_dhcp_hostname}"
            )
            return
        suspicious: bool = False
        observed: CaptureResult
        for observed in observed_frames:
            # RFC 2131 BOOTP fixed fields are not DHCP options. This client should send
            # zero-filled server-name/boot-file fields; any payload is undeclared data,
            # including binary DeviceID seeds and option-overload contents.
            suspicious = suspicious or any(
                any(bytes.fromhex(value)) for _, value in observed.bootp_fixed_values
            )
            observed_text: str = observed.frame or ""
            suspicious = suspicious or any(pattern.search(observed_text) for pattern in
                                           (PEM_PATTERN, POINTER_PATTERN, DIAGNOSTIC_PATTERN,
                                            BEARER_PATTERN, BUILD_PATTERN))
            suspicious = suspicious or re.search(r"(?i)(?:vendor-specific|option 43|option 125|password|token|secret)",
                                                  observed_text) is not None
            if observed.option_values is not None:
                observed_hostname: str = observed.hostname or ""
                suspicious = suspicious or any(
                    code not in STANDARD_DHCP_OPTIONS and not (
                        code == 60 and observed_hostname and
                        bytes.fromhex(value).decode("ascii", errors="replace").casefold()
                        == observed_hostname.casefold()
                    )
                    for code, value in observed.option_values
                )
                option_value: str
                for _, option_value in observed.option_values:
                    raw_option: bytes = bytes.fromhex(option_value)
                    decoded: str = raw_option.decode("utf-8", errors="replace")
                    suspicious = suspicious or bytes.fromhex(self.config.gw_id.replace(":", "")) in raw_option
                    suspicious = suspicious or self.config.gw_id.casefold() in decoded.casefold()
                    suspicious = suspicious or any(pattern.search(decoded) for pattern in
                                                   (PEM_PATTERN, POINTER_PATTERN, DIAGNOSTIC_PATTERN, SECRET_FIELD_PATTERN,
                                                    HEADER_SECRET_FIELD_PATTERN, WIFI_PASSWORD_PATTERN, HISTORY_PATTERN,
                                                    COORDINATES_PATTERN, TAGS_PATTERN, BEARER_PATTERN, BUILD_PATTERN))
        self.outcomes["DHCP capture"] = "FAIL" if suspicious else "PASS"
        if suspicious:
            self.output("DHCP disclosure FAIL: unexpected or sensitive DHCP field or option")
        self.evidence.write("DHCP DISCLOSURE ASSERTION", AssertionEvidence(
            "DUT DHCP frame contains no sensitive or vendor payload", self.outcomes["DHCP capture"], frame))

    def run(self) -> RunResult:
        started_at: datetime = self.clock()
        self.evidence.write("RUN UTC START", format_utc(started_at))
        self.progress("Authenticating and checking initialized factory-default DUT")
        active_check: str = "factory-default preflight"
        incomplete: bool = False
        try:
            self._preflight()
            self.progress("Executing isolated unauthenticated GET/HEAD matrix")
            active_check = "HTTP disclosure matrix"
            self._matrix()
            self.progress("Checking generic GET Server banner")
            active_check = "generic banner"
            self._banner()
            self.progress("Checking redirects and interactive challenge")
            active_check = "redirects and challenge"
            self._redirects()
            self.progress("Scanning every unauthenticated header and body")
            active_check = "sensitive disclosure scan"
            self._scan()
            self.progress("Confirming deliberately public metadata")
            active_check = "public metadata"
            self._public()
            self.progress("Capturing DUT DHCP frame on LAN")
            active_check = "DHCP capture"
            self._dhcp()
        except Exception as error:  # noqa: BLE001 - Preserve evidence and classify all incomplete observations.
            self.evidence.exception(error)
            self.output(f"test ERROR: {type(error).__name__}: {error}")
            incomplete = True
            self.evidence.write("INCOMPLETE CHECK", {"check": active_check, "error": type(error).__name__,
                                                      "earlier_result": self.outcomes[active_check]})
            self.output(f"{active_check} ERROR: incomplete observation; earlier result {self.outcomes[active_check]}")
            if self.outcomes[active_check] != "FAIL":
                self.outcomes[active_check] = "ERROR"
        values: tuple[str, ...] = tuple(self.outcomes.values())
        verdict: str = "ERROR" if incomplete or "ERROR" in values or "NOT RUN" in values else (
            "FAIL" if "FAIL" in values else "PASS"
        )
        if self.outcomes["DHCP capture"] == "ERROR" and all(
            state == "PASS" for check, state in self.outcomes.items() if check != "DHCP capture"
        ):
            self.output("HTTP disclosure core passed; documented L2 capture precondition was not satisfied")
        recovery: str | None = FACTORY_RESET_MESSAGE if self.reset_advice else None
        if recovery:
            self.output(recovery)
            self.evidence.write("RECOVERY", recovery)
        check: str
        state: str
        for check, state in self.outcomes.items():
            self.evidence.write("FINAL CHECK RESULT", {"check": check, "result": state})
        self.evidence.write("OVERALL RESULT", verdict)
        ended_at: datetime = self.clock()
        self.evidence.write("RUN UTC END", format_utc(ended_at))
        self.evidence.write("RUN DURATION SECONDS", (ended_at - started_at).total_seconds())
        return RunResult(0 if verdict == "PASS" else 1 if verdict == "FAIL" else 2,
                         verdict, dict(self.outcomes), set(), recovery)


def execute_test_5_6_2_2_a(
    work_dir: Path | None = None, *,
    session_factory: Callable[[], requests.Session] = requests.Session,
    random_bytes: Callable[[int], bytes] = secrets.token_bytes,
    ecc_generate: Callable[..., ECC.EccKey] = ECC.generate,
    capturer: Callable[[str, int], CaptureResult] | None = None,
    now: Callable[[], datetime] = utc_now,
    output: Callable[[str], None] = print,
) -> RunResult:
    directory: Path = Path.cwd() if work_dir is None else work_dir
    log: EvidenceLog = EvidenceLog.create(directory / "logs", "test_5_6_2_2_a", now)
    output(f"Open log file: {log.path}")

    def report(message: str) -> None:
        output(message)
        log.write_line(message)

    progress: ProgressReporter = ProgressReporter(report, TOTAL_STEPS)
    result: RunResult = RunResult(2, "ERROR", {check: "NOT RUN" for check in CHECKS}, set())
    log.write("TEST CASE AND UNIT", TEST_ID)
    log.write("UTC START", format_utc(log.started_at))
    try:
        progress.step("Loading and validating .env")
        config: DutConfig = load_dut_config(directory / ".env")
        log.write("DUT CONFIGURATION", config)
        runner: FunctionalTest_5_6_2_2_a = FunctionalTest_5_6_2_2_a(
            config, log, session_factory=session_factory, random_bytes=random_bytes,
            ecc_generate=ecc_generate, capturer=capturer, clock=now,
            progress=progress.step, output=output,
        )
        result = runner.run()
    except Exception as error:
        log.exception(error)  # noqa: TRY401 - EvidenceLog.exception needs the exception instance.
        output(f"test ERROR: {type(error).__name__}: {error}")
    finally:
        log.finish(result.verdict, now)
    output(f"Overall verdict: {result.verdict}")
    return result


def main() -> int:
    return execute_test_5_6_2_2_a().exit_code


if __name__ == "__main__":
    sys.exit(main())
