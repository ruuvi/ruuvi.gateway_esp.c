"""Case-specific local HTTP credential witness; no network access during import."""

from __future__ import annotations

import base64
import hmac
import http.server
import ipaddress
import socket
import threading
from dataclasses import dataclass
from typing import Protocol

from lib.errors import InvalidSetup


@dataclass(frozen=True)
class HttpRoute:
    path: str
    method: str
    username: str
    password: str
    response: bytes = b"{}"


class HttpFixture(Protocol):
    base_url: str

    def counts(self, path: str) -> tuple[int, int]: ...

    def close(self) -> None: ...


class CredentialWitness:
    def __init__(self, routes: tuple[HttpRoute, ...]) -> None:
        self.routes: dict[str, HttpRoute] = {route.path: route for route in routes}
        self.observed: dict[str, tuple[int, int]] = dict.fromkeys(self.routes, (0, 0))
        self.lock: threading.Lock = threading.Lock()

    def counts(self, path: str) -> tuple[int, int]:
        with self.lock:
            return self.observed[path]

    def respond(self, method: str, path: str, authorization: str) -> tuple[int, bytes]:
        route: HttpRoute | None = self.routes.get(path)
        if route is None:
            return 404, b"{}"
        if method != route.method:
            return 405, b"{}"
        expected: str = "Basic " + base64.b64encode(f"{route.username}:{route.password}".encode()).decode()
        accepted: bool = hmac.compare_digest(authorization.encode(), expected.encode())
        with self.lock:
            good: int
            bad: int
            good, bad = self.observed[path]
            self.observed[path] = (good + int(accepted), bad + int(not accepted))
        return (200, route.response) if accepted else (401, b"{}")


def host_lan_address(gateway_hostname: str) -> str:
    """Ask the OS which IPv4 source address routes to the verified LAN Gateway."""
    connection: socket.socket
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
            connection.connect((gateway_hostname, 80))  # UDP connect selects a route; sends no packet.
            address: str = connection.getsockname()[0]
    except OSError as error:
        error: OSError
        raise InvalidSetup("Cannot select the host's LAN IPv4 address for the HTTP fixture") from error
    parsed: ipaddress.IPv4Address = ipaddress.IPv4Address(address)
    if parsed.is_loopback or parsed.is_unspecified or parsed.is_multicast:
        raise InvalidSetup("HTTP fixture needs a LAN IPv4 address reachable from the Gateway")
    return address


class LocalHttpFixture:
    def __init__(self, gateway_hostname: str, routes: tuple[HttpRoute, ...]) -> None:
        witness: CredentialWitness = CredentialWitness(routes)
        self.witness: CredentialWitness = witness

        class Handler(http.server.BaseHTTPRequestHandler):
            def __init__(self, request: socket.socket, client_address: tuple[str, int], server: http.server.ThreadingHTTPServer) -> None:
                super().__init__(request, client_address, server)

            def setup(self) -> None:
                self.request.settimeout(5.0)
                super().setup()

            def do_GET(self) -> None:
                self.respond()

            def do_POST(self) -> None:
                self.respond()

            def log_message(self, format: str, *args: object) -> None:
                pass  # Credentials and telemetry bodies must not be printed by the server.

            def respond(self) -> None:
                # Close each response; no request body is retained or served from disk.
                # A bounded body read lets a POST sender finish before the reply.
                try:
                    length: int = int(self.headers.get("Content-Length", "0"))
                    if length < 0 or length > 1024 * 1024 or self.headers.get("Transfer-Encoding"):
                        self.send_error(413)
                        return
                    if length and len(self.rfile.read(length)) != length:
                        self.send_error(400)
                        return
                except (ValueError, OSError):
                    self.send_error(400)
                    return
                status: int
                body: bytes
                status, body = witness.respond(self.command, self.path, self.headers.get("Authorization", ""))
                self.send_response(status)
                if status == 401:
                    self.send_header("WWW-Authenticate", 'Basic realm="CRA fixture"')
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(body)
                self.close_connection = True

        address: str = host_lan_address(gateway_hostname)
        try:
            self.server: http.server.ThreadingHTTPServer = http.server.ThreadingHTTPServer((address, 0), Handler)
        except OSError as error:
            error: OSError
            raise InvalidSetup("Cannot bind the local HTTP fixture; check the host LAN interface") from error
        self.base_url: str = f"http://{address}:{self.server.server_port}"
        self.thread: threading.Thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        try:
            self.thread.start()
        except RuntimeError:
            self.server.server_close()
            raise

    def counts(self, path: str) -> tuple[int, int]:
        return self.witness.counts(path)

    def close(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5.0)
