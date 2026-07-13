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
            "state": "logging",
            "voltage": "3.742",
            "current": "0.520",
            "capacity_mah": "10.8",
            "elapsed_s": "75",
        }
    )
    assert s.channel == 2
    assert s.state == "logging"
    assert abs(s.voltage - 3.742) < 1e-9
    assert abs(s.current - 0.520) < 1e-9
    assert abs(s.capacity_mah - 10.8) < 1e-9
    assert s.elapsed_s == 75


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
    assert len(rows) == 16
    # First row has the battery freshly connected and load drawing current.
    assert float(rows[0]["voltage"]) > 4.0
    assert float(rows[0]["current_a"]) > 0.4
    # Last row: BMS has cut off — voltage low, current zero.
    assert float(rows[-1]["voltage"]) < 3.0
    assert float(rows[-1]["current_a"]) == 0.0
    # Voltage monotonically decreases as the cell discharges.
    voltages = [float(r["voltage"]) for r in rows]
    for prev, cur in zip(voltages, voltages[1:]):
        assert cur <= prev + 0.001  # allow tiny measurement noise