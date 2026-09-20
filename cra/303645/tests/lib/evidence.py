"""Exclusive evidence logging shared by live-DUT functional tests."""

from __future__ import annotations

import json
import traceback
from dataclasses import asdict, dataclass, is_dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, TextIO

import requests


@dataclass(frozen=True)
class AssertionEvidence:
    description: str
    result: str
    actual: Any


@dataclass(frozen=True)
class MechanismResultEvidence:
    mechanism: str
    result: str


@dataclass(frozen=True)
class RouteResultEvidence:
    mechanism: str
    method: str
    path: str
    result: str


@dataclass(frozen=True)
class HashComparisonEvidence:
    baseline: str
    final: str


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def format_utc(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


class EvidenceLog:
    def __init__(self, path: Path, stream: TextIO, started_at: datetime) -> None:
        self.path: Path = path
        self._stream: TextIO = stream
        self.started_at: datetime = started_at

    @classmethod
    def create(
        cls,
        log_dir: Path,
        filename_prefix: str,
        now: Callable[[], datetime] = utc_now,
    ) -> EvidenceLog:
        log_dir.mkdir(parents=True, exist_ok=True)
        started_at: datetime = now()
        stamp: str = started_at.astimezone(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        collision: int
        for collision in range(1000):
            suffix: str = "" if collision == 0 else f"_{collision}"
            path: Path = log_dir / f"{filename_prefix}_{stamp}{suffix}.log"
            try:
                stream: TextIO = path.open("x", encoding="utf-8")
                return cls(path, stream, started_at)
            except FileExistsError:
                continue
        raise FileExistsError("could not create a unique evidence log")

    def write(self, label: str, value: Any = "") -> None:
        structured_value: Any = asdict(value) if is_dataclass(value) else value
        if isinstance(structured_value, (dict, list, tuple)):
            rendered: str = json.dumps(
                structured_value,
                ensure_ascii=True,
                sort_keys=True,
                default=lambda item: asdict(item) if is_dataclass(item) else str(item),
            )
        else:
            rendered = str(value)
        self._stream.write(f"[{format_utc(utc_now())}] {label}: {rendered}\n")
        self._stream.flush()

    def write_line(self, message: str) -> None:
        self._stream.write(f"[{format_utc(utc_now())}] {message}\n")
        self._stream.flush()

    def write_http_request(self, request: requests.PreparedRequest) -> None:
        body: Any = request.body
        if isinstance(body, bytes):
            body = body.decode("utf-8", errors="backslashreplace")
        self.write("HTTP REQUEST BEGIN")
        self._stream.write(f"{request.method} {request.url}\n")
        value: str
        name: str
        for name, value in request.headers.items():
            self._stream.write(f"{name}: {value}\n")
        self._stream.write(f"\n{'' if body is None else body}\n")
        self.write("HTTP REQUEST END")

    def write_http_response(self, response: requests.Response) -> None:
        self.write("HTTP RESPONSE BEGIN")
        self._stream.write(f"HTTP STATUS {response.status_code}\n")
        value: str
        name: str
        for name, value in response.headers.items():
            self._stream.write(f"{name}: {value}\n")
        cookies: dict[str, str] = response.cookies.get_dict() if hasattr(response.cookies, "get_dict") else {}
        self._stream.write(f"Cookies: {json.dumps(cookies, sort_keys=True)}\n\n")
        self._stream.write(f"{response.text}\n")
        self.write("HTTP RESPONSE END")

    def exception(self, error: BaseException) -> None:
        self.write("EXCEPTION TYPE", type(error).__name__)
        self._stream.write("".join(traceback.format_exception(type(error), error, error.__traceback__)))
        self._stream.flush()

    def finish(self, verdict: str, now: Callable[[], datetime] = utc_now) -> None:
        ended_at: datetime = now()
        ended_stamp: str = format_utc(ended_at)
        self._stream.write(f"[{ended_stamp}] UTC END: {ended_stamp}\n")
        self._stream.write(f"[{ended_stamp}] DURATION SECONDS: {(ended_at - self.started_at).total_seconds():.3f}\n")
        self._stream.write(f"[{ended_stamp}] OVERALL VERDICT: {verdict}\n")
        self._stream.flush()
        self._stream.close()
