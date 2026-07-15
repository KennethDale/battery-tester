#ifndef BATTERY_TESTER_BATTERY_CHANNEL_H
#define BATTERY_TESTER_BATTERY_CHANNEL_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include "config.h"

// Logging states for a passive channel.
enum class ChannelState : uint8_t {
    WAITING = 0,       // No battery connected (voltage below threshold)
    LOGGING = 1,       // Battery detected, actively recording
    LOW_VOLTAGE = 2,   // Voltage fell below disconnect threshold; one entry logged
};

const char* channelStateToString(ChannelState s);

// Snapshot of a channel's accumulated state. main.cpp persists an array of
// these to RTC user memory each sample tick so a test survives a watchdog
// reset or crash (RTC memory does not survive power loss). Keep the size a
// multiple of 4 bytes for ESP.rtcUserMemoryWrite.
struct ChannelSnapshot {
    float capacityMah;
    float capacityWh;
    uint32_t elapsedMs;
    uint32_t sinceResetMs;
    uint8_t state;
    uint8_t interrupted;
    uint8_t reserved[2];
};

// One passive logging channel.
//
// Each channel wraps a single INA219 on the I2C bus. A pre-charged battery
// with a fixed load is connected to the INA219's Vin-/Vin+ shunt. The logger:
//   * auto-detects when a battery is connected (voltage > CONNECT_THRESHOLD_V),
//   * samples voltage / current every SAMPLE_INTERVAL_MS,
//   * records a history entry on voltage change or a quiet timeout,
//   * auto-detects BMS disconnect (voltage < DISCONNECT_THRESHOLD_V) and
//     suppresses further entries until the voltage recovers.
//
// Threshold transitions are debounced over DEBOUNCE_SAMPLES consecutive
// samples, and samples whose I2C reads fail are dropped without touching
// state or totals.
class BatteryChannel {
public:
    // Cursor for streaming the history CSV in chunks (see
    // serializeHistoryCsvChunk). Zero-initialized state starts a new export.
    struct CsvCursor {
        size_t index = 0;
        float capMah = 0.0f;
    };

    BatteryChannel(uint8_t index, uint8_t i2cAddress);

    // Initialize the INA219. Returns false if the sensor wasn't found.
    bool begin();

    // True if the INA219 was found during begin().
    bool sensorConnected() const { return _sensorConnected; }

    // Read the INA219 and update the state machine. Call every SAMPLE_INTERVAL_MS.
    void update();

    // Reset accumulated capacity and history; return to WAITING.
    void reset();

    // Fill `out` with the state needed to resume this test after a reboot.
    void captureSnapshot(ChannelSnapshot& out) const;

    // Resume a test from a snapshot taken before a reboot. Returns false if
    // the snapshot holds no test in progress (WAITING or invalid state).
    // History is not persisted, so the CSV restarts; its capacity column
    // continues from the restored total.
    bool restoreSnapshot(const ChannelSnapshot& s);

    // --- Accessors ----------------------------------------------------------
    uint8_t index() const { return _index; }
    ChannelState state() const { return _state; }
    float voltage() const { return _lastVoltage; }      // Volts (bus)
    float current() const { return _lastCurrent; }      // Amps (shunt)
    uint32_t capacityMah() const { return static_cast<uint32_t>(_capacityMahAccum + 0.5f); }  // mAh accumulated (rounded)
    float capacityWh() const { return _capacityWh; }    // Wh accumulated
    unsigned long elapsedSeconds() const { return _elapsedMs / 1000UL; }
    // True if the battery was already connected when the device booted, so
    // part of the discharge was never measured.
    bool interrupted() const { return _interrupted; }

    // Serialize the latest sample as a JSON object into `out`.
    void serializeLatest(String& out) const;

    // Append up to `maxRows` history CSV rows to `out`, advancing `cur`.
    // The first call (cur.index == 0) also writes the header row. Returns
    // true while more rows remain.
    bool serializeHistoryCsvChunk(String& out, CsvCursor& cur, size_t maxRows) const;

    // True if a sample should be logged now (voltage change or timeout).
    bool shouldLogSample(float v) const;

private:
    uint8_t _index;
    Adafruit_INA219 _ina;
    bool _sensorConnected = false;

    ChannelState _state = ChannelState::WAITING;
    float _lastVoltage = 0.0f;
    float _lastCurrent = 0.0f;
    float _capacityMahAccum = 0.0f;  // mAh accumulated (exact, rounded only on read)
    float _capacityWh = 0.0f;        // Wh accumulated
    unsigned long _elapsedMs = 0;    // Time spent LOGGING (ms)
    unsigned long _sinceResetMs = 0;
    unsigned long _lastSampleMs = 0;
    unsigned long _lastLoggedMs = 0;
    float _lastLoggedVoltage = 0.0f;
    bool _lowVoltageEntryLogged = false;
    uint8_t _debounceCount = 0;
    float _voltageBeforeDip = 0.0f;       // Last healthy voltage before LOW_VOLTAGE
    unsigned long _lowVoltageSinceMs = 0; // When LOW_VOLTAGE was entered
    bool _interrupted = false;
    float _historyBaseMah = 0.0f;    // Capacity already accumulated before the
                                     // oldest history entry (set on restore)

    // History buffer. The first HISTORY_PRESERVE_SLOTS are filled linearly
    // to preserve the start of the test; remaining slots form a ring.
    int16_t _voltageHistory[HISTORY_LENGTH];
    int16_t _currentHistory[HISTORY_LENGTH];
    uint32_t _timeHistory[HISTORY_LENGTH];
    uint32_t _resetTimeHistory[HISTORY_LENGTH];
    size_t _historyCount = 0;
    size_t _historyHead = 0;

    void startTest(unsigned long now, float v);
    void pushSample(float v, float a);
    void integrateCapacity(float a, float v, float dtSec);
};

#endif  // BATTERY_TESTER_BATTERY_CHANNEL_H
