# 🔋 ESP8266 Battery Tester

A 5-channel battery charge/discharge tester built around an ESP8266. Each
channel can independently charge or discharge a single li-ion / li-po cell
while logging voltage, current, elapsed time, and accumulated capacity. Results
are served through a built-in web UI and a small JSON/CSV HTTP API, with an
optional Python companion for terminal monitoring and bulk export.

> ⚠️ **Safety:** Lithium cells store a lot of energy and can vent or catch fire
> if over-charged, over-discharged, or short-circuited. This firmware enforces
> software cut-offs, but **software is not a safety device**. Always include
> hardware protection (fuses, cell-level protection ICs, temperature sensing)
> and never leave charging cells unattended.

---

## Features

- **5 independent channels** – charge, discharge, or idle each cell separately.
- **Web UI** – live status cards with charge/discharge/stop controls, served
  from the ESP8266 itself (no internet required).
- **JSON + CSV API** – integrate with Home Assistant, Grafana, or your own
  scripts.
- **Downloadable data** – every channel exposes a CSV of its buffered history.
- **Python companion** – CLI for `status`, `watch`, `start`, `stop`, and
  `export` from your laptop.
- **mDNS** – reach the board at `http://batterytester.local` (or its AP IP).
- **Soft-AP fallback** – if no WiFi credentials are configured, the board
  starts its own access point so the UI still works on the bench.

## Project layout

```
battery-tester/
├── platformio.ini            # PlatformIO build / board config
├── data/                     # LittleFS assets (uploaded with `pio run -t uploadfs`)
│   └── index.html            # Web UI (self-contained)
├── src/                      # Firmware application
│   ├── main.cpp              # Entry point: setup/loop, pin map, mux control
│   ├── config.h              # Compile-time tunables
│   ├── secrets.example.h     # Copy to secrets.h with your WiFi creds
│   └── (secrets.h)           # git-ignored, created from the example
├── lib/                      # Local firmware libraries
│   ├── BatteryChannel/       # Per-channel model + state machine
│   ├── WebServer/            # HTTP routes (JSON/CSV API + static UI)
│   └── WifiManager/          # WiFi connect + soft-AP fallback
├── python/                   # Python companion package + CLI
│   ├── battery_tester/       # Importable package
│   ├── tests/                # pytest unit tests + CSV fixture
│   ├── requirements.txt
│   └── pyproject.toml
└── docs/
    └── HARDWARE.md           # Pin map, suggested components, wiring
```

## Requirements

| Tool        | Version   | Notes                                   |
| ----------- | --------- | --------------------------------------- |
| PlatformIO  | ≥ 6.x     | `pip install platformio` or `brew install platformio` |
| Python      | ≥ 3.9     | Only needed for the companion CLI/tests |
| ESP8266 board | NodeMCU 1.0 / Wemos D1 R2 (or compatible) |

## Quick start (firmware)

```bash
# 1. Configure WiFi (optional but recommended)
cp src/secrets.example.h src/secrets.h
# edit src/secrets.h with your SSID/password

# 2. Build + flash the firmware
pio run -t upload

# 3. Upload the web UI to LittleFS
pio run -t uploadfs

# 4. Open the serial monitor to see the assigned IP
pio device monitor
```

Then visit **http://batterytester.local** (or the printed IP) in a browser.

If you skip the WiFi config, the board boots into AP mode and you can join the
`BatteryTester-XXXXXX` network; the UI will be at `192.168.4.1`.

## Quick start (Python companion)

```bash
# from the repository root
source .venv/bin/activate          # the repo already includes a .venv
pip install -e python              # installs the `battery-tester` CLI

battery-tester status             # one-shot snapshot
battery-tester watch              # live-refreshing table (Ctrl-C to quit)
battery-tester start 0 discharge  # start discharging channel 0
battery-tester export 0 -o run.csv
```

You can point the CLI at a different host with `--host 192.168.1.50` or the
`BATTERY_TESTER_HOST` environment variable.

### Running the tests

```bash
pip install pytest
pytest python/tests
```

## HTTP API

| Method | Path                          | Description                                  |
| ------ | ----------------------------- | -------------------------------------------- |
| GET    | `/`                           | Web UI (`index.html` from LittleFS)          |
| GET    | `/api/info`                   | Firmware version, chip id, uptime, channels  |
| GET    | `/api/status`                 | JSON snapshot of **all** channels            |
| GET    | `/api/channel/{n}`            | JSON snapshot of one channel                 |
| POST   | `/api/channel/{n}/start`      | Body `{"mode":"charge"|"discharge"}`         |
| POST   | `/api/channel/{n}/stop`       | Stop the running test                        |
| GET    | `/api/history/{n}.csv`        | CSV download of channel `n` history          |

`n` is a 0-indexed channel number (0–4).

## Configuration

Tunable constants live in [`src/config.h`](src/config.h) and can be overridden
from `platformio.ini`'s `build_flags`. Notable settings:

| Define                  | Default | Meaning                                    |
| ----------------------- | ------- | ------------------------------------------ |
| `NUM_CHANNELS`          | 5       | Number of active channels                   |
| `LOOP_INTERVAL_MS`      | 1000    | Control-loop cadence per channel            |
| `VOLTAGE_DIVIDER_RATIO` | 2.0     | ADC divider ratio                           |
| `ADC_REFERENCE_V`       | 3.3     | ADC reference (NodeMCU onboard divider)     |
| `DISCHARGE_CUTOFF_V`    | 3.0     | Stop discharging below this voltage         |
| `DISCHARGE_CURRENT_MA`  | 500     | Nominal load current (until HW current loop)|
| `HISTORY_LENGTH`        | 120     | Samples buffered per channel                |

## Hardware

See [`docs/HARDWARE.md`](docs/HARDWARE.md) for the suggested schematic, pin
map, and bill of materials. The firmware currently assumes:

- an **analog multiplexer** (e.g. CD4051) sharing the ESP8266's single ADC
  across 5 channels,
- **MOSFET-based** charge and discharge switches per channel,
- a **current-sense** path (placeholder in firmware — replace `readCurrent()`
  with a real INA219 / shunt read for accurate mAh integration).

## Roadmap

- [ ] ADS1115 / INA219 driver for accurate voltage + current measurement
- [ ] NTC thermistor per channel for thermal cutoff
- [ ] Persistent test results (LittleFS JSON log)
- [ ] WebSocket push instead of polling
- [ ] Plotly / Chart.js history graphs in the UI

## License

MIT — see [`LICENSE`](LICENSE).