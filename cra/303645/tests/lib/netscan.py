"""Injected, verdict-free nmap and mDNS acquisition for LAN interface tests."""

from __future__ import annotations

import math
import os
import re
import shutil
import socket
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Protocol, Tuple, Union, cast
from xml.etree import ElementTree

from .errors import InvalidSetup

RunCommand = Callable[..., subprocess.CompletedProcess]
SocketAddress = Union[Tuple[str, int], Tuple[str, int, int, int]]
ResolveRecords = List[Tuple[int, int, int, str, SocketAddress]]
Resolver = Callable[..., ResolveRecords]


class DatagramSocket(Protocol):
    def settimeout(self, timeout: float) -> None: ...
    def sendto(self, data: bytes, address: tuple[str, int]) -> int: ...
    def recvfrom(self, size: int) -> tuple[bytes, tuple[str, int]]: ...
    def close(self) -> None: ...


def system_resolver(hostname: str, port: int | None, **kwargs: int) -> ResolveRecords:
    return cast(ResolveRecords, socket.getaddrinfo(hostname, port, **kwargs))


def find_nmap(name: str) -> str | None:
    return shutil.which(name)


def new_datagram_socket(family: int, kind: int) -> DatagramSocket:
    # AF_INET recvfrom has a concrete (address, port) tuple; socket stubs also cover other families.
    return cast(DatagramSocket, cast(object, socket.socket(family, kind)))


class ScanError(InvalidSetup):
    """A scan prerequisite, execution, or XML result is incomplete."""


@dataclass(frozen=True)
class ResolvedHost:
    ip: str
    family: int
    addresses: tuple[str, ...]


@dataclass(frozen=True)
class NmapInfo:
    path: str
    version: tuple[int, int]
    has_raw_privilege: bool
    privilege_error: str = ""


@dataclass(frozen=True)
class PortObservation:
    port: int
    state: str
    service: str
    product: str
    reason: str


@dataclass(frozen=True)
class ExtraportsGroup:
    state: str
    count: int


@dataclass(frozen=True)
class ScanResult:
    open_ports: frozenset[int]
    closed_ports: frozenset[int]
    filtered_ports: frozenset[int]
    closed_count: int
    filtered_count: int
    open_filtered: frozenset[int]
    open_filtered_count: int
    raw_xml: str
    elapsed: float
    command: tuple[str, ...]
    observations: tuple[PortObservation, ...]
    extraports: tuple[ExtraportsGroup, ...] = ()
    closed_filtered: frozenset[int] = frozenset()
    closed_filtered_count: int = 0


@dataclass(frozen=True)
class MdnsAnswer:
    source: str
    name: str
    record_type: int
    data: bytes


def resolve_host(hostname: str, *, resolver: Resolver = system_resolver) -> ResolvedHost:
    try:
        records: list[tuple[int, int, int, str, SocketAddress]] = resolver(
            hostname, None, type=socket.SOCK_STREAM,
        )
    except OSError as error:
        error: OSError
        raise ScanError(
            f"cannot resolve {hostname}: {error}; enable host mDNS resolution or set gw_hostname to an IPv4 literal"
        ) from error
    addresses: tuple[str, ...] = tuple(dict.fromkeys(cast(str, record[4][0]) for record in records))
    record: tuple[int, int, int, str, SocketAddress]
    for record in records:
        if record[0] == socket.AF_INET:
            return ResolvedHost(cast(str, record[4][0]), record[0], addresses)
    raise ScanError(f"{hostname} has no IPv4 address; IPv6-only scanning is outside this test")


def raw_socket_privilege() -> bool:
    if os.geteuid() == 0:
        return True
    try:
        status: str = Path("/proc/self/status").read_text(encoding="ascii")
    except OSError:
        return False
    match: re.Match[str] | None = re.search(r"^CapEff:\s*([0-9a-fA-F]+)$", status, re.MULTILINE)
    return match is not None and bool(int(match.group(1), 16) & (1 << 13))


def check_nmap(
    min_version: tuple[int, int], *, runner: RunCommand = subprocess.run,
    find_executable: Callable[[str], str | None] = find_nmap,
    privilege: Callable[[], bool] | None = None,
) -> NmapInfo:
    path: str | None = find_executable("nmap")
    if path is None:
        raise ScanError("nmap is required; install it with apt-get install nmap")
    try:
        completed: subprocess.CompletedProcess[str] = runner(
            [path, "--version"], capture_output=True, text=True, timeout=10, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        error: OSError | subprocess.TimeoutExpired
        raise ScanError(f"nmap version preflight failed: {error}") from error
    match: re.Match[str] | None = re.search(r"Nmap version (\d+)\.(\d+)", completed.stdout)
    if completed.returncode != 0 or match is None:
        raise ScanError("nmap version is unavailable or unparseable")
    version: tuple[int, int] = (int(match.group(1)), int(match.group(2)))
    if version < min_version:
        raise ScanError(f"nmap {version[0]}.{version[1]} is older than required {min_version[0]}.{min_version[1]}")
    if privilege is not None:
        return NmapInfo(path, version, privilege())
    probe_command: list[str] = [
        path, "--privileged", "-sU", "-Pn", "-n", "--max-retries", "0",
        "--host-timeout", "10s", "-p", "1", "-oX", "-", "127.0.0.1",
    ]
    try:
        probe: subprocess.CompletedProcess[str] = runner(
            probe_command, capture_output=True, text=True, timeout=15, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return NmapInfo(path, version, False, f"Nmap UDP privilege probe failed: {error}")
    if probe.returncode != 0:
        detail: str = probe.stderr.strip() or probe.stdout.strip() or "no diagnostic output"
        return NmapInfo(path, version, False, f"Nmap UDP privilege probe exited {probe.returncode}: {detail}")
    return NmapInfo(path, version, True)


def _port_set(specification: str) -> frozenset[int]:
    selected: set[int] = set()
    token: str
    try:
        for token in specification.split(","):
            if "-" in token:
                bounds: list[str] = token.split("-", 1)
                first: int = int(bounds[0])
                last: int = int(bounds[1])
                if first < 1 or last > 65535 or first > last:
                    raise ScanError("invalid scan port range")
                selected.update(range(first, last + 1))
            else:
                selected.add(int(token))
    except ValueError as error:
        error: ValueError
        raise ScanError(f"invalid scan port range {specification}") from error
    if not selected or min(selected) < 1 or max(selected) > 65535:
        raise ScanError("invalid scan port range")
    return frozenset(selected)


def parse_nmap_xml(xml_text: str, protocol: str, requested: frozenset[int],
                   elapsed: float, command: tuple[str, ...]) -> ScanResult:
    try:
        root: ElementTree.Element = ElementTree.fromstring(xml_text)
        scaninfo: ElementTree.Element | None = root.find(f"scaninfo[@protocol='{protocol}']")
        finished: ElementTree.Element | None = root.find("runstats/finished")
        host: ElementTree.Element | None = root.find("host")
        if scaninfo is None or finished is None or finished.get("exit") != "success" or host is None:
            raise ScanError("nmap XML is incomplete or scan did not finish successfully")
        if host.get("timedout") == "true":
            raise ScanError("nmap host timed out before scan completion")
        status: ElementTree.Element | None = host.find("status")
        if status is None or status.get("state") != "up":
            raise ScanError("nmap did not scan an available host")
        if _port_set(scaninfo.attrib["services"]) != requested:
            raise ScanError("nmap XML does not cover the requested ports")
        if int(scaninfo.attrib["numservices"]) != len(requested):
            raise ScanError("nmap XML service count does not match the requested ports")
        observations: list[PortObservation] = []
        seen: set[int] = set()
        state_counts: dict[str, int] = {"closed": 0, "filtered": 0, "open|filtered": 0,
                                        "closed|filtered": 0}
        extra_groups: list[ExtraportsGroup] = []
        ports: ElementTree.Element | None = host.find("ports")
        if ports is None:
            raise ScanError("nmap XML has no port results; host scan may have timed out")
        extra: ElementTree.Element
        for extra in ports.findall("extraports"):
            state: str = extra.attrib["state"]
            if state not in state_counts:
                raise ScanError(f"unsupported nmap extraports state {state}")
            count: int = int(extra.attrib["count"])
            if count < 0:
                raise ScanError("nmap XML contains a negative extraports count")
            extra_groups.append(ExtraportsGroup(state, count))
            state_counts[state] += count
        element: ElementTree.Element
        for element in ports.findall("port"):
            if element.get("protocol") != protocol:
                continue
            port: int = int(element.attrib["portid"])
            if port not in requested or port in seen:
                raise ScanError("nmap XML contains an unexpected or duplicate port")
            seen.add(port)
            state_element: ElementTree.Element | None = element.find("state")
            if state_element is None:
                raise ScanError(f"nmap XML lacks state for {port}/{protocol}")
            state = state_element.attrib["state"]
            if state not in {"open", "closed", "filtered", "open|filtered", "closed|filtered"}:
                raise ScanError(f"unsupported nmap state {state}")
            if state in state_counts:
                state_counts[state] += 1
            service: ElementTree.Element | None = element.find("service")
            observations.append(PortObservation(
                port, state, service.get("name", "") if service is not None else "",
                service.get("product", "") if service is not None else "",
                state_element.get("reason", ""),
            ))
        if sum(state_counts.values()) + sum(item.state == "open" for item in observations) != len(requested):
            raise ScanError("nmap XML port totals do not cover the requested range")
        omitted: frozenset[int] = requested.difference(seen)
        if len(omitted) != sum(group.count for group in extra_groups):
            raise ScanError("nmap extraports do not match omitted ports")
        extra_state: str | None = extra_groups[0].state if len(extra_groups) == 1 else None
        if protocol == "tcp" and (state_counts["open|filtered"] or state_counts["closed|filtered"]):
            raise ScanError("TCP result has an unexpected ambiguous state")
        return ScanResult(
            frozenset(item.port for item in observations if item.state == "open"),
            frozenset(item.port for item in observations if item.state == "closed") |
            (omitted if extra_state == "closed" else frozenset()),
            frozenset(item.port for item in observations if item.state == "filtered") |
            (omitted if extra_state == "filtered" else frozenset()),
            state_counts["closed"], state_counts["filtered"],
            frozenset(item.port for item in observations if item.state == "open|filtered") |
            (omitted if extra_state == "open|filtered" else frozenset()),
            state_counts["open|filtered"],
            xml_text, elapsed, command, tuple(observations), tuple(extra_groups),
            frozenset(item.port for item in observations if item.state == "closed|filtered") |
            (omitted if extra_state == "closed|filtered" else frozenset()),
            state_counts["closed|filtered"],
        )
    except (ElementTree.ParseError, KeyError, ValueError) as error:
        error: ElementTree.ParseError | KeyError | ValueError
        raise ScanError(f"invalid nmap XML: {error}") from error


def _run_scan(
    ip: str, ports: str, protocol: str, *, nmap_path: str, runner: RunCommand,
    host_timeout: int, max_retries: int, clock: Callable[[], float],
    enumerate_ports: bool = False, scan_delay_ms: int | None = None,
    defeat_icmp_ratelimit: bool = True, service_detection: bool = True,
) -> ScanResult:
    requested: frozenset[int] = _port_set(ports)
    limits: tuple[int, ...] = (host_timeout, max_retries) + (() if scan_delay_ms is None else (scan_delay_ms,))
    if (any(type(value) is not int for value in limits) or host_timeout <= 0 or max_retries < 0
            or (scan_delay_ms is not None and scan_delay_ms <= 0)):
        raise ScanError("scan timeout and limits must be finite integers with valid ranges")
    directory: str
    with tempfile.TemporaryDirectory(prefix="ruuvi-netscan-") as directory:
        xml_path: Path = Path(directory) / "nmap.xml"
        command: tuple[str, ...] = (
            nmap_path, *(("--privileged",) if protocol == "udp" else ()),
            "-sT" if protocol == "tcp" else "-sU", "-Pn", "-n", "--reason",
            *(("-sV",) if service_detection else ()),
            *(("-d3",) if enumerate_ports else ()),
            *(("--defeat-icmp-ratelimit",) if protocol == "udp" and defeat_icmp_ratelimit else ()),
            *(("--scan-delay", f"{scan_delay_ms}ms") if scan_delay_ms is not None else ()),
            "--max-retries", f"{max_retries:d}", "--host-timeout", f"{host_timeout}s",
            "-p", ports, "-oX", str(xml_path), ip,
        )
        started: float = clock()
        try:
            completed: subprocess.CompletedProcess[str] = runner(
                list(command), capture_output=True, text=True, timeout=host_timeout + 30, check=False,
            )
        except (OSError, UnicodeError, subprocess.TimeoutExpired) as error:
            error: OSError | UnicodeError | subprocess.TimeoutExpired
            raise ScanError(f"{protocol.upper()} scan failed or timed out: {error}") from error
        if completed.returncode != 0:
            raise ScanError(f"{protocol.upper()} scan exited {completed.returncode}: {completed.stderr.strip()}")
        try:
            xml_text: str = xml_path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            raise ScanError(f"{protocol.upper()} scan produced no readable XML: {error}") from error
        elapsed: float = clock() - started
        result: ScanResult = parse_nmap_xml(xml_text, protocol, requested, elapsed, command)
        if enumerate_ports and any(
            group.state in {"open|filtered", "closed|filtered"} for group in result.extraports
        ):
            raise ScanError("nmap did not enumerate indeterminate port states with -d3")
        return result


def run_tcp_connect_scan(
    ip: str, ports: str, *, nmap_path: str = "nmap", runner: RunCommand = subprocess.run,
    host_timeout: int = 600, max_retries: int = 1, clock: Callable[[], float] = time.monotonic,
    scan_delay_ms: int | None = None,
) -> ScanResult:
    return _run_scan(ip, ports, "tcp", nmap_path=nmap_path, runner=runner,
                     host_timeout=host_timeout, max_retries=max_retries, clock=clock,
                     scan_delay_ms=scan_delay_ms)


def run_udp_scan(
    ip: str, ports: str, *, nmap_path: str = "nmap", runner: RunCommand = subprocess.run,
    host_timeout: int = 1800, max_retries: int = 1, clock: Callable[[], float] = time.monotonic,
    has_raw_privilege: bool | None = None,
    enumerate_ports: bool = False, scan_delay_ms: int | None = None,
    defeat_icmp_ratelimit: bool = True, service_detection: bool = True,
) -> ScanResult:
    if not (raw_socket_privilege() if has_raw_privilege is None else has_raw_privilege):
        raise ScanError("UDP scan requires root or CAP_NET_RAW")
    return _run_scan(ip, ports, "udp", nmap_path=nmap_path, runner=runner,
                     host_timeout=host_timeout, max_retries=max_retries, clock=clock,
                     enumerate_ports=enumerate_ports, scan_delay_ms=scan_delay_ms,
                     defeat_icmp_ratelimit=defeat_icmp_ratelimit,
                     service_detection=service_detection)


def _dns_name(packet: bytes, offset: int) -> tuple[str, int]:
    labels: list[str] = []
    next_offset: int = offset
    jumped: bool = False
    visited: set[int] = set()
    while True:
        if offset >= len(packet) or offset in visited:
            raise ScanError("invalid mDNS name compression")
        visited.add(offset)
        length: int = packet[offset]
        if length == 0:
            return ".".join(labels), next_offset if jumped else offset + 1
        if length & 0xC0 == 0xC0:
            if offset + 1 >= len(packet):
                raise ScanError("truncated mDNS compression pointer")
            if not jumped:
                next_offset = offset + 2
                jumped = True
            offset = ((length & 0x3F) << 8) | packet[offset + 1]
            continue
        if length & 0xC0 or offset + 1 + length > len(packet):
            raise ScanError("invalid mDNS label")
        try:
            labels.append(packet[offset + 1:offset + 1 + length].decode("ascii"))
        except UnicodeDecodeError as error:
            error: UnicodeDecodeError
            raise ScanError("non-ASCII mDNS label") from error
        offset += 1 + length
    raise ScanError("unterminated mDNS name")


def probe_mdns(
    ip: str, *, timeout: float = 3.0, socket_factory: Callable[..., DatagramSocket] = new_datagram_socket,
) -> MdnsAnswer | None:
    if (isinstance(timeout, bool) or not isinstance(timeout, (int, float))
            or not math.isfinite(timeout) or timeout <= 0):
        raise ScanError("mDNS probe timeout must be finite and positive")
    query: bytes = (
        b"\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"
        b"\x09_services\x07_dns-sd\x04_udp\x05local\x00\x00\x0c\x80\x01"
    )
    try:
        channel: DatagramSocket = socket_factory(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            channel.settimeout(timeout)
            channel.sendto(query, (ip, 5353))
            try:
                packet: bytes
                address: tuple[str, int]
                packet, address = channel.recvfrom(4096)
            except socket.timeout:
                return None
        finally:
            channel.close()
    except OSError as error:
        error: OSError
        raise ScanError(f"mDNS probe failed for {ip}: {error}") from error
    if address != (ip, 5353) or len(packet) < 12 or packet[2] & 0x80 == 0:
        return None
    question_count: int = int.from_bytes(packet[4:6], "big")
    answer_count: int = int.from_bytes(packet[6:8], "big")
    offset: int = 12
    index: int
    for index in range(question_count):
        _, offset = _dns_name(packet, offset)
        if offset + 4 > len(packet):
            raise ScanError("truncated mDNS question")
        offset += 4
    for index in range(answer_count):
        name: str
        name, offset = _dns_name(packet, offset)
        if offset + 10 > len(packet):
            raise ScanError("truncated mDNS answer")
        record_type: int = int.from_bytes(packet[offset:offset + 2], "big")
        size: int = int.from_bytes(packet[offset + 8:offset + 10], "big")
        offset += 10
        if offset + size > len(packet):
            raise ScanError("truncated mDNS answer data")
        data_offset: int = offset
        data: bytes = packet[offset:offset + size]
        offset += size
        if name.lower() == "_http._tcp.local" and record_type == 12:
            return MdnsAnswer(address[0], name, record_type, data)
        if name.lower() == "_services._dns-sd._udp.local" and record_type == 12:
            service: str
            service_end: int
            service, service_end = _dns_name(packet, data_offset)
            if service_end <= offset and service.lower() == "_http._tcp.local":
                return MdnsAnswer(address[0], service, record_type, data)
    return None
