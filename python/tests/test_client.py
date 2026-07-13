"""Unit tests for the data parsing/path logic (no network required)."""

from __future__ import annotations

import csv
import io
from pathlib import Path

from battery_tester.client import ChannelStatus

FIXTURE = Path(__file__).parent / "fixtures" / "sample_history.csv"


def test_channel_status_from_dict() -> None:
    s = ChannelStatus.from_dict(
        {
            "channel": 2,
            "state": "discharging",
            "voltage": "3.742",
            "current": "0.501",
            "capacity_mah": "123.4",
            "elapsed_s": "600",
        }
    )
    assert s.channel == 2
    assert s.state == "discharging"
    assert abs(s.voltage - 3.742) < 1e-9
    assert abs(s.current - 0.501) < 1e-9
    assert abs(s.capacity_mah - 123.4) < 1e-9
    assert s.elapsed_s == 600


def test_channel_status_defaults_missing_fields() -> None:
    s = ChannelStatus.from_dict({})
    assert s.channel == -1
    assert s.state == "unknown"
    assert s.voltage == 0.0
    assert s.current == 0.0
    assert s.capacity_mah == 0.0
    assert s.elapsed_s == 0


def test_fixture_csv_is_parseable() -> None:
    """Ensures the bundled sample CSV matches the firmware's export schema."""
    text = FIXTURE.read_text(encoding="utf-8")
    reader = csv.DictReader(io.StringIO(text))
    rows = list(reader)
    assert reader.fieldnames == ["elapsed_s", "voltage", "current_a"]
    assert len(rows) == 12
    # First row is the start-of-test sample (no current yet).
    assert float(rows[0]["current_a"]) == 0.0
    # Discharge ramps the voltage down over time.
    voltages = [float(r["voltage"]) for r in rows]
    assert voltages[-1] < voltages[0]