"""HTTP client for the battery-tester firmware API."""

from __future__ import annotations

import csv
import io
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

import requests


class BatteryTesterError(RuntimeError):
    """Raised when the firmware API returns an error or is unreachable."""


@dataclass
class ChannelStatus:
    channel: int
    state: str
    connected: bool
    voltage: float
    current: float
    capacity_mah: float
    elapsed_s: int

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "ChannelStatus":
        return cls(
            channel=int(d.get("channel", -1)),
            state=str(d.get("state", "unknown")),
            connected=bool(d.get("connected", False)),
            voltage=float(d.get("voltage", 0.0)),
            current=float(d.get("current", 0.0)),
            capacity_mah=float(d.get("capacity_mah", 0.0)),
            elapsed_s=int(d.get("elapsed_s", 0)),
        )


class BatteryTesterClient:
    """Thin wrapper around the ESP8266 REST API."""

    def __init__(self, host: str = "batterytester.local", timeout: float = 5.0) -> None:
        # Accept either "http://host" or bare "host".
        if not host.startswith("http://") and not host.startswith("https://"):
            host = f"http://{host}"
        self.base_url = host.rstrip("/")
        self.timeout = timeout

    # --- low-level --------------------------------------------------------
    def _get(self, path: str, *, accept: Optional[str] = None) -> requests.Response:
        headers = {"Accept": accept} if accept else {}
        try:
            r = requests.get(self.base_url + path, headers=headers, timeout=self.timeout)
        except requests.RequestException as e:
            raise BatteryTesterError(f"unable to reach {self.base_url}: {e}") from e
        if not r.ok:
            raise BatteryTesterError(f"GET {path} -> HTTP {r.status_code}: {r.text[:120]}")
        return r

    def _post(self, path: str, json_body: Optional[Dict[str, Any]] = None) -> requests.Response:
        try:
            r = requests.post(self.base_url + path, json=json_body, timeout=self.timeout)
        except requests.RequestException as e:
            raise BatteryTesterError(f"unable to reach {self.base_url}: {e}") from e
        if not r.ok:
            raise BatteryTesterError(f"POST {path} -> HTTP {r.status_code}: {r.text[:120]}")
        return r

    # --- high-level -------------------------------------------------------
    def info(self) -> Dict[str, Any]:
        return self._get("/api/info").json()

    def status(self) -> List[ChannelStatus]:
        data = self._get("/api/status").json()
        return [ChannelStatus.from_dict(c) for c in data.get("channels", [])]

    def channel(self, index: int) -> ChannelStatus:
        return ChannelStatus.from_dict(self._get(f"/api/channel?n={index}").json())

    def reset(self, index: int) -> ChannelStatus:
        """Clear a channel's accumulated data and return to WAITING."""
        return ChannelStatus.from_dict(self._post(f"/api/reset?n={index}").json())

    def history_csv(self, index: int) -> str:
        """Return the raw CSV text for a channel's buffered history."""
        return self._get(f"/api/history?n={index}", accept="text/csv").text

    def history_rows(self, index: int) -> List[Dict[str, str]]:
        """Parse the CSV history into a list of row dicts."""
        text = self.history_csv(index)
        reader = csv.DictReader(io.StringIO(text))
        return list(reader)
