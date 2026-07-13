#ifndef BATTERY_TESTER_CONFIG_H
#define BATTERY_TESTER_CONFIG_H

// ---------------------------------------------------------------------------
// Configuration for the passive INA219 battery capacity logger.
// Override any of these via PlatformIO build_flags if needed.
// ---------------------------------------------------------------------------

#ifndef NUM_CHANNELS
#define NUM_CHANNELS 5
#endif

#ifndef BAUD_RATE
#define BAUD_RATE 115200
#endif

// How often to sample each channel and write a log entry (milliseconds)
#ifndef LOG_INTERVAL_MS
#define LOG_INTERVAL_MS 5000
#endif

// --- INA219 I2C addresses --------------------------------------------------
// Each INA219 breakout must have a unique address set via its A0/A1 jumpers.
// Default addresses from the datasheet (0x40..0x45); change to match your
// board wiring.
#ifndef INA219_ADDRESSES
#define INA219_ADDRESSES {0x40, 0x41, 0x42, 0x43, 0x44}
#endif

// INA219 calibration: shunt resistance in ohms and max current in amps.
// These match the common blue INA219 breakout (0.1 ohm, 3.2A max).
#ifndef SHUNT_OHMS
#define SHUNT_OHMS 0.1f
#endif

#ifndef MAX_CURRENT_A
#define MAX_CURRENT_A 3.2f
#endif

// --- Auto-detect thresholds ------------------------------------------------
// A reading above this means a battery is connected and we should start
// logging. Li-ion cells sit around 3.0–4.2V; use a conservative floor.
#ifndef CONNECT_THRESHOLD_V
#define CONNECT_THRESHOLD_V 2.5f
#endif

// A reading below this means the BMS has disconnected the battery and the
// test is complete. The cell's protection board cuts off around 2.5–2.9V.
#ifndef DISCONNECT_THRESHOLD_V
#define DISCONNECT_THRESHOLD_V 2.0f
#endif

// Web server port
#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT 80
#endif

// Max history samples per channel (kept in RAM). At 5s intervals this is
// ~8 minutes; increase for longer runs or rely on CSV download.
#ifndef HISTORY_LENGTH
#define HISTORY_LENGTH 600
#endif

#endif  // BATTERY_TESTER_CONFIG_H