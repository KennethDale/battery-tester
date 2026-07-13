# Hardware notes

This document describes the reference hardware design the firmware expects.
You are free to adapt it — just keep the pin mapping in [`src/main.cpp`](../src/main.cpp)
in sync with your wiring.

> ⚠️ **Lithium safety.** A single 18650 holds ~10 Wh. Five of them can dump
> >100 A into a short. Use cell holders with welded protection boards, a per-
> channel fuse, and a temperature sensor. The software cut-offs here are a
> convenience, **not** a safety device.

## Block diagram

```
            ┌──────────────┐
   5V ─────▶│ 5→4.2V charge│──┬──▶ [ch0 TP4056 + DW01] ──▶ cell 0
            │ regulators   │  ├─▶ [ch1 ...]              ──▶ cell 1
   USB/PSU  │ (per cell)   │  ├─▶ ...                    ──▶ cell 2
            └──────────────┘  ├─▶ ...                    ──▶ cell 3
                              └─▶ ...                    ──▶ cell 4

   Each cell+ ── R1 ──┬── ADC mux input (CD4051 ch0..ch4)
                      │
                     R2
                      │
                     GND

   Each cell+ ──▶ load MOSFET (low-side) ── current-sense resistor ──▶ GND
                                                              │
                                                  (sense amp → optional ADC)
```

## ESP8266 pin map (NodeMCU 1.0 / Wemos D1)

| GPIO  | NodeMCU pin | Firmware role                  |
| ----- | ----------- | ------------------------------ |
| A0    | A0          | Shared ADC (via analog mux)    |
| GPIO5 | D1          | MUX select bit 0               |
| GPIO4 | D2          | MUX select bit 1               |
| GPIO0 | D3          | MUX select bit 2               |
| GPIO2 | D4          | Global load-enable (active high)|
| GPIO14| D5          | Channel 0 gate                 |
| GPIO12| D6          | Channel 1 gate                 |
| GPIO13| D7          | Channel 2 gate                 |
| GPIO15| D8          | Channel 3 gate                 |
| GPIO16| D0          | Channel 4 gate                 |

> These pin assignments are defaults in `src/main.cpp` — change them there if
> your board differs.

## Suggested bill of materials

| Part                         | Qty | Notes                                            |
| ---------------------------- | --- | ------------------------------------------------ |
| ESP8266 dev board            | 1   | NodeMCU 1.0 or Wemos D1 R2                       |
| CD4051 analog mux            | 1   | 8:1 mux, only 5 inputs used                      |
| TP4056 + protection board    | 5   | Per-cell charge + DW01 overcharge/overdischarge  |
| P-channel MOSFET (charge)    | 5   | e.g. IRF4905, driven by a small NPN level shifter|
| N-channel MOSFET (discharge) | 5   | e.g. IRLZ44N (logic-level)                       |
| Power resistor (load)        | 5   | 8Ω 5W gives ~0.5A at 4V; pick for your target    |
| Current-sense resistor       | 5   | 0.1Ω 1% → 50 mV at 0.5A                          |
| INA219 breakout (optional)   | 5   | Accurate I²C current/voltage (recommended)       |
| 18650 cell holder            | 5   | With welded protection board preferred           |
| Fuse (per channel)           | 5   | ~2A polyfuse or glass fuse                       |

## Voltage divider sizing

The ESP8266 ADC reads 0–1 V on the bare chip, or 0–3.3 V on dev boards with an
onboard divider. To measure a 4.2 V cell safely on a bare chip, use:

```
V_adc = V_cell * R2 / (R1 + R2)
1.0 V = 4.2 V * R2 / (R1 + R2)  →  R1 = 3.2 * R2
```

Example: `R1 = 320 kΩ`, `R2 = 100 kΩ`. Set `VOLTAGE_DIVIDER_RATIO` in
`config.h` to `(R1 + R2) / R2 = 4.2`.

On NodeMCU/Wemos (onboard divider, 3.3 V max), a 2:1 external divider
(`R1 = R2 = 100 kΩ`) brings 4.2 V down to 2.1 V, comfortably inside range.
The default `VOLTAGE_DIVIDER_RATIO = 2.0` matches this.

## Current measurement (upgrade path)

`BatteryChannel::readCurrent()` currently returns a nominal value. For real
capacity integration, add either:

- **INA219** modules on the I²C bus (one per channel — set different I²C
  addresses via the A0/A1 jumpers), or
- An **ADS1115** 4-channel ADC reading the voltage across each shunt resistor.

Both libraries are available in the PlatformIO registry; replace
`readCurrent()` in `lib/BatteryChannel/BatteryChannel.cpp` with the real read.