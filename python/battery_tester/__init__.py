"""Python companion tools for the ESP8266 battery tester.

Provides a small client for the firmware's JSON/CSV HTTP API plus a CLI that can
poll live status, export per-channel history, and stream readings to CSV.
"""

from .client import BatteryTesterClient, BatteryTesterError

__all__ = ["BatteryTesterClient", "BatteryTesterError"]
__version__ = "0.2.0"
