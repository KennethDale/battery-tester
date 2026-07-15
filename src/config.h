#ifndef BATTERY_TESTER_CONFIG_H
#define BATTERY_TESTER_CONFIG_H

// ---------------------------------------------------------------------------
// Configuration for the passive INA219 battery capacity logger.
// Override any of these via PlatformIO build_flags if needed.
// ---------------------------------------------------------------------------

#ifndef NUM_CHANNELS
#define NUM_CHANNELS 8
#endif

#ifndef BAUD_RATE
#define BAUD_RATE 115200
#endif

// How often to sample each channel (milliseconds). The firmware samples the
// INA219 every second and decides whether to push a history entry based on
// voltage change or a maximum quiet period.
#ifndef SAMPLE_INTERVAL_MS
#define SAMPLE_INTERVAL_MS 1000
#endif

// Minimum voltage change (volts) that triggers a new history entry.
#ifndef LOG_VOLTAGE_DELTA_V
#define LOG_VOLTAGE_DELTA_V 0.05f
#endif

// Maximum time between history entries when the voltage is stable (ms).
#ifndef LOG_MAX_QUIET_MS
#define LOG_MAX_QUIET_MS 60000
#endif

// --- INA219 I2C addresses --------------------------------------------------
// Each INA219 breakout must have a unique address set via its A0/A1 jumpers.
// Default addresses from the datasheet (0x40..0x45); change to match your
// board wiring.
#ifndef INA219_ADDRESSES
#define INA219_ADDRESSES {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47}
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

// Consecutive samples required past a threshold before a state transition.
// Filters single-sample noise/contact bounce so one glitched reading can't
// start, stop, or wipe a test.
#ifndef DEBOUNCE_SAMPLES
#define DEBOUNCE_SAMPLES 3
#endif

// If the voltage recovers after the channel has sat in LOW_VOLTAGE longer
// than this, assume a fresh battery was connected and start a new test
// instead of adding to the previous battery's totals.
#ifndef LOW_VOLTAGE_NEW_TEST_MS
#define LOW_VOLTAGE_NEW_TEST_MS 600000UL
#endif

// A reconnect voltage this far above the last healthy voltage also counts
// as a fresh battery (e.g. 4.1V after a cell that sagged to 3.0V).
#ifndef NEW_TEST_VOLTAGE_JUMP_V
#define NEW_TEST_VOLTAGE_JUMP_V 0.8f
#endif

// A battery detected within this long after boot was connected before the
// device started (e.g. power loss mid-test): flag the test "interrupted"
// so its capacity total isn't trusted blindly.
#ifndef BOOT_CONNECT_GRACE_MS
#define BOOT_CONNECT_GRACE_MS 15000UL
#endif

// --- Early final-capacity estimate -----------------------------------------
// Predict the final delivered capacity (mAh) from the pack voltage at the very
// start of the discharge: final_mAh ~= SLOPE * V0 + INTERCEPT. Fit by
// leave-one-out cross-validation over the 13 clean runs in results/ (see
// results/capacity_estimator.py); median error ~7%. Because these cells have a
// flat voltage plateau under constant-current load, the first minutes of the
// curve add no information over V0 alone, so the estimate is available as soon
// as a test starts. It is only trustworthy while V0 sits in the calibrated
// band [V0_MIN, V0_MAX]; outside it the estimate is reported as invalid.
#ifndef CAPACITY_ESTIMATE_SLOPE_MAH_PER_V
#define CAPACITY_ESTIMATE_SLOPE_MAH_PER_V 1228.6f
#endif
#ifndef CAPACITY_ESTIMATE_INTERCEPT_MAH
#define CAPACITY_ESTIMATE_INTERCEPT_MAH (-3486.5f)
#endif
#ifndef CAPACITY_ESTIMATE_V0_MIN
#define CAPACITY_ESTIMATE_V0_MIN 3.0f
#endif
#ifndef CAPACITY_ESTIMATE_V0_MAX
#define CAPACITY_ESTIMATE_V0_MAX 3.6f
#endif

// Web server port
#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT 80
#endif

// Max history samples per channel (kept in RAM). 300 samples is sized for
// ~1.25 hours of change-driven logging. The first 60 slots preserve the
// initial 15 minutes; the remaining 240 slots form a ring for recent samples.
#ifndef HISTORY_LENGTH
#define HISTORY_LENGTH 300
#endif

// Number of initial history slots reserved for the start of the test.
#ifndef HISTORY_PRESERVE_SLOTS
#define HISTORY_PRESERVE_SLOTS 60
#endif

#endif  // BATTERY_TESTER_CONFIG_H