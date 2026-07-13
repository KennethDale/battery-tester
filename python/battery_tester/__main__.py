#!/usr/bin/env python3
"""Entry point shim so `python -m battery_tester` and `battery-tester` both work."""

from battery_tester.cli import main

if __name__ == "__main__":
    main()