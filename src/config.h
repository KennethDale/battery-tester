#ifndef BATTERY_TESTER_CONFIG_H
#define BATTERY_TESTER_CONFIG_H

// ---------------------------------------------------------------------------
// Global compile-time configuration for the 5-channel battery tester firmware.
// Override any of these via PlatformIO build_flags if needed.
// ---------------------------------------------------------------------------

#ifndef NUM_CHANNELS
#define NUM_CHANNELS 5
#endif

#ifndef BAUD_RATE
#define BAUD_RATE 115200
#endif

// Sampling / control loop cadence (milliseconds)
#ifndef LOOP_INTERVAL_MS
#define LOOP_INTERVAL_MS 1000
#endif

// Voltage divider ratio:  V_batt = ADC * (R1 + R2) / R2
// Default assumes a 2:1 divider (R1 = R2 = 100k) feeding the ESP8266 1.0V ADC
// via a resistor divider; adjust to match your hardware.
#ifndef VOLTAGE_DIVIDER_RATIO
#define VOLTAGE_DIVIDER_RATIO 2.0f
#endif

// ADC reference voltage for the ESP8266 (1.0V full scale on the bare chip,
// but NodeMCU/Wemos boards include an on-board divider giving ~3.3V range).
#ifndef ADC_REFERENCE_V
#define ADC_REFERENCE_V 3.3f
#endif

// ADC bit depth (ESP8266 ADC is 10-bit: 0..1023)
#define ADC_MAX 1023.0f

// Discharge cut-off threshold (volts). Stop discharging below this to avoid
// over-discharging li-ion cells. Override per-chemistry in firmware.
#ifndef DISCHARGE_CUTOFF_V
#define DISCHARGE_CUTOFF_V 3.0f
#endif

// Default discharge current target (milliamps). Set by hardware load circuit.
#ifndef DISCHARGE_CURRENT_MA
#define DISCHARGE_CURRENT_MA 500
#endif

// Web server port
#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT 80
#endif

// History buffer length per channel (oldest samples are dropped first)
#ifndef HISTORY_LENGTH
#define HISTORY_LENGTH 120
#endif

#endif  // BATTERY_TESTER_CONFIG_H