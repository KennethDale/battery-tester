# 🔋 ESP8266 Battery Capacity Logger

A simple, passive 5-channel battery capacity logger built around an ESP8266 and
INA219 current/voltage sensors. You connect a **pre-charged battery with a fixed
load** to each channel and the logger does the rest:

1. **Auto-detects** when a battery is connected (voltage rises above threshold)
2. **Logs** voltage, current, and accumulated capacity every 5 seconds
3. **Auto-stops** when the BMS disconnects the battery (voltage collapses)
4. **Displays** the final measured capacity (mAh) on a simple web interface

No charge/discharge control. No MOSFETs to drive. The ESP8266 just watches and
records — the battery's own BMS decides when the test is over.

> ⚠️ **Safety:** Lithium cells store a lot of energy. Always use cells with
> protection boards (BMS), appropriate load resistors, and fuses. Never leave
> discharging cells unattended.

---

## How it works

```
                 ┌──────────┐
  Battery+ ─────▶│ INA219   │─── Load Resistor ─── GND
                 │ (shunt)  │
  Battery- ─────▶│          │
                 └──────────┘
                      │ I2C (SDA/SCL)
                      ▼
                 ┌──────────┐
                 │ ESP8266  │ ── WiFi ──▶ Web UI
                 └──────────┘
```

Each INA219 sits between the battery and its load, measuring both voltage
(bus) and current (shunt). The ESP8266 polls all 5 sensors over I2C every 5
seconds, integrates the current into mAh, and watches for the BMS cutoff.

## Project layout

```
battery-tester/
├── platformio.ini            # PlatformIO build config
├── data/
│   └── index.html            # Web UI (self-contained, served from LittleFS)
├── src/
│   ├── main.cpp              # Entry point: I2C init, sampling loop
│   ├── config.h              # Compile-time tunables (intervals, thresholds)
│   ├── secrets.example.h     # Copy to secrets.h with your WiFi creds
│   └── (secrets.h)           # git-ignored
├── lib/
│   ├── BatteryChannel/       # INA219 wrapper + auto-detect state machine
│   ├── WebServer/            # HTTP routes (JSON/CSV API + static UI)
│   └── WifiManager/          # WiFi connect + soft-AP fallback
├── python/                   # Optional CLI companion
│   ├── battery_tester/
│   ├── tests/
│   ├── requirements.txt
│   └── pyproject.toml
└── docs/
    └── HARDWARE.md           # Wiring, INA219 address setup, BOM
```

## Requirements

| Tool        | Version  | Notes                                        |
| ----------- | -------- | -------------------------------------------- |
| PlatformIO  | ≥ 6.x    | `pip install platformio` or `brew install platformio` |
| Python      | ≥ 3.9    | Only needed for the companion CLI/tests       |
| ESP8266     | NodeMCU 1.0 / Wemos D1 R2 or compatible      |
| INA219      | 5× breakouts, each at a unique I2C address   |

## Quick start (firmware)

```bash
# 1. Configure WiFi (optional — falls back to AP mode)
cp src/secrets.example.h src/secrets.h
# edit src/secrets.h with your SSID/password

# 2. Build + flash
pio run -t upload

# 3. Upload the web UI to LittleFS
pio run -t uploadfs

# 4. Watch the serial monitor for the IP address
pio device monitor
```

Then visit **http://batterylogger.local** (or the printed IP).

If you skip WiFi config, the board boots into AP mode — join the
`BatteryTester-XXXXXX` network and browse to `192.168.4.1`.

## Quick start (Python companion)

```bash
source .venv/bin/activate
pip install -e python

battery-tester status              # one-shot snapshot
battery-tester watch               # live-refreshing table (Ctrl-C to quit)
battery-tester export 0 -o run.csv # download channel 0 history
battery-tester reset 0             # clear channel 0 data
```

### Running the tests

```bash
pip install pytest
pytest python/tests
```

## HTTP API

| Method | Path                          | Description                              |
| ------ | ----------------------------- | ---------------------------------------- |
| GET    | `/`                           | Web UI (`index.html` from LittleFS)      |
| GET    | `/api/info`                   | Firmware, chip id, uptime, log interval  |
| GET    | `/api/status`                 | JSON snapshot of **all** channels        |
| GET    | `/api/channel?n={n}`          | JSON snapshot of one channel             |
| POST   | `/api/reset?n={n}`            | Clear channel data, return to waiting    |
| GET    | `/api/history?n={n}`          | CSV download of channel `n` history      |

`n` is a 0-indexed channel number (0–4).

## Configuration

Tunable constants live in [`src/config.h`](src/config.h) and can be overridden
from `platformio.ini`'s `build_flags`:

| Define                   | Default | Meaning                                    |
| ------------------------ | ------- | ------------------------------------------ |
| `NUM_CHANNELS`           | 5       | Number of active channels                   |
| `LOG_INTERVAL_MS`        | 5000    | How often to sample + record               |
| `INA219_ADDRESSES`       | 0x40–0x44 | I2C addresses of each INA219             |
| `SHUNT_OHMS`             | 0.1     | Shunt resistance on the INA219 breakout    |
| `MAX_CURRENT_A`          | 3.2     | Max current for INA219 calibration         |
| `CONNECT_THRESHOLD_V`    | 2.5     | Voltage above this = battery connected     |
| `DISCONNECT_THRESHOLD_V` | 2.0     | Voltage below this = BMS cutoff / done     |
| `HISTORY_LENGTH`         | 600     | Max samples per channel in RAM             |

## Hardware

See [`docs/HARDWARE.md`](docs/HARDWARE.md) for wiring, INA219 address
configuration, and a bill of materials.

## License

MIT — see [`LICENSE`](LICENSE).