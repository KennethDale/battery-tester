# Hardware notes

This document describes the reference hardware for the passive INA219 battery
capacity logger. The default firmware supports up to 8 channels (0–7).

> ⚠️ **Lithium safety.** Use cells with protection boards (BMS). The BMS
> determines when the test ends by cutting off. Always include a fuse per
> channel.

## Wiring overview

```text
                     I2C Bus (shared)
   ESP8266 D1 (SCL) ──┬──┬──┬──┬──┬──┬──┬──┬──▶ INA219 #0..7 SCL
   ESP8266 D2 (SDA) ──┼──┼──┼──┼──┼──┼──┼──┼──▶ INA219 #0..7 SDA
   ESP8266 3.3V    ───┼──┼──┼──┼──┼──┼──┼──┼──▶ INA219 #0..7 VCC
   ESP8266 GND     ───┴──┴──┴──┴──┴──┴──┴──┴──▶ INA219 #0..7 GND
                        │  │  │  │  │  │  │  │
                        └──┴──┴──┴──┴──┴──┴──┴──▶ Battery- (all channels, see below)

  Per channel:
  Battery+ ──▶ INA219 Vin+ ──▶ INA219 Vin- ──▶ Load Resistor ──▶ Battery- ──▶ common GND
                     │
              (INA219 measures voltage & current here)
```

Each channel is identical: the battery connects through the INA219's shunt to
a fixed load resistor. The INA219 measures the bus voltage (battery voltage)
and the shunt voltage (proportional to current).

> ⚠️ **`Battery-` must be tied to the common GND net** (the same rail as the
> ESP8266 GND and INA219 GND pins). The INA219's bus voltage reading is
> Vin- measured *relative to its own GND pin*, not Vin+ minus Vin-. If the
> load-resistor return path isn't referenced to that shared ground, the bus
> voltage reads near 0V no matter what a multimeter shows across the battery
> terminals directly — the channel will stay stuck in `waiting` even with a
> battery connected.

## ESP8266 pin map (NodeMCU 1.0 / Wemos D1)

| GPIO  | NodeMCU pin | Role                          |
| ----- | ----------- | ----------------------------- |
| GPIO5 | D1          | I2C SCL (all INA219s)         |
| GPIO4 | D2          | I2C SDA (all INA219s)         |
| 3.3V  | 3V3         | INA219 VCC                    |
| GND   | GND         | Common ground                 |

Only 2 GPIO pins are needed for all 8 channels — everything goes over I2C.

## INA219 address configuration

Each INA219 breakout has two address jumpers (A0, A1) that set its I2C address.
You need 8 unique addresses. The default addresses available are:

| A1 jumper | A0 jumper | I2C address | Channel | Notes              |
| --------- | --------- | ----------- | ------- | ------------------ |
| GND       | GND       | 0x40        | 0       | default            |
| GND       | VS        | 0x41        | 1       | A0 to pad          |
| GND       | SDA       | 0x42        | 2       |                    |
| GND       | SCL       | 0x43        | 3       |                    |
| VS        | GND       | 0x44        | 4       | A1 to pad          |
| VS        | VS        | 0x45        | 5       | A1 and A2 to pad   |
| VS        | SDA       | 0x46        | 6       |                    |
| VS        | SCL       | 0x47        | 7       |                    |


Set these addresses on each breakout by soldering the A0/A1 pads according to
the breakout's silkscreen. Match the order to `INA219_ADDRESSES` in
`config.h`.

> **Pull-ups:** The I2C bus needs 4.7kΩ pull-up resistors on SDA and SCL.
> Many INA219 breakouts include them — if using 8 boards, only enable pull-ups
> on one (cut the traces on the other 7) to avoid overloading the bus.

## Load resistor sizing

Choose a load resistor appropriate for the cell and target discharge rate:

| Target current | Load resistor (at 3.7V) | Power rating |
| -------------- | ----------------------- | ------------ |
| 250 mA         | ~15Ω                    | ≥ 1W         |
| 500 mA         | ~7.5Ω                   | ≥ 2W         |
| 1 A            | ~3.7Ω                   | ≥ 5W         |

Use wirewound or ceramic power resistors. The resistor must dissipate
`P = I² × R` continuously — err on the side of a higher wattage rating.

## Bill of materials

| Part                         | Qty | Notes                                    |
| ---------------------------- | --- | ---------------------------------------- |
| ESP8266 dev board            | 1   | NodeMCU 1.0 or Wemos D1 R2               |
| INA219 breakout              | 8   | Each at a unique I2C address (0x40–0x47) |
| Power resistor (load)        | 8   | Sized for your target current            |
| 18650 cell holder + BMS      | 8   | Protected cells or add a protection board |
| Fuse (per channel)           | 8   | ~2A polyfuse or glass fuse               |
| 4.7kΩ resistor (I2C pull-up) | 2   | Only needed if breakouts lack pull-ups   |
| Breadboard / perfboard       | 1   | For wiring                               |

## How auto-detect works

The firmware uses two voltage thresholds (defined in `config.h`):

1. **`CONNECT_THRESHOLD_V` (2.5V)** — When voltage rises above this, the
   logger starts recording. This detects when you plug in a charged cell.

2. **`DISCONNECT_THRESHOLD_V` (2.0V)** — When voltage drops below this, the
   BMS has cut off the battery. The logger freezes the capacity reading and
   marks the channel as `COMPLETE`.

A typical li-ion BMS cuts off around 2.5V under load. The default 2.0V
threshold is conservative — you can raise it to match your BMS if needed.

## Calibration

The INA219 breakout comes with a 0.1Ω shunt resistor and is calibrated for
3.2A max by default. If your breakout has a different shunt, update
`SHUNT_OHMS` and `MAX_CURRENT_A` in `config.h`.

To verify calibration, connect a known load and compare the firmware's current
reading against a multimeter. Fine-tune the shunt value if needed.