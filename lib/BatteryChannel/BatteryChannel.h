#ifndef BATTERY_TESTER_BATTERY_CHANNEL_H
#define BATTERY_TESTER_BATTERY_CHANNEL_H

#include <Arduino.h>
#include "config.h"

// Test states a single channel can be in.
enum class ChannelState : uint8_t {
    IDLE = 0,        // Nothing happening, channel disabled
    CHARGING = 1,    // Charging the cell
    DISCHARGING = 2, // Discharging the cell through the load
    COMPLETE = 3,    // Test finished, results available
    FAULT = 4,       // Fault condition (over-temp, voltage out of range, etc.)
};

// Convert a ChannelState to its lowercase string label (for JSON / UI).
const char* channelStateToString(ChannelState s);

// Single test-channel model.
//
// One instance represents one of the 5 battery slots. It owns:
//   * the analog input pin used to measure cell voltage,
//   * a digital output pin that enables the charge circuit,
//   * a digital output pin that enables the discharge load,
//   * a rolling history buffer of voltage / current samples.
class BatteryChannel {
public:
    // Construct a channel. Pins are passed in from main so the mapping lives
    // in one obvious place.
    BatteryChannel(uint8_t index,
                   uint8_t adcPin,
                   uint8_t chargePin,
                   uint8_t dischargePin,
                   uint8_t loadEnablePin);

    // Set up pin modes and zero out state. Call once in setup().
    void begin();

    // Read sensors, update state machine, and push a sample into history.
    // Should be called every LOOP_INTERVAL_MS.
    void update();

    // --- Controls (called from web/serial commands) ------------------------
    void startCharge();
    void startDischarge();
    void stop();

    // --- Accessors ----------------------------------------------------------
    uint8_t index() const { return _index; }
    ChannelState state() const { return _state; }
    float voltage() const { return _lastVoltage; }
    float current() const { return _lastCurrent; }
    float capacityMah() const { return _capacityMah; }
    unsigned long elapsedSeconds() const { return _elapsedSeconds; }

    // Serialize the latest sample as a JSON object line into `out`.
    void serializeLatest(String& out) const;

    // Write all buffered history samples (CSV) into `out`. Used for downloads.
    void serializeHistoryCsv(String& out) const;

private:
    // Hardware mapping
    uint8_t _index;
    uint8_t _adcPin;
    uint8_t _chargePin;
    uint8_t _dischargePin;
    uint8_t _loadEnablePin;

    // Runtime state
    ChannelState _state = ChannelState::IDLE;
    float _lastVoltage = 0.0f;     // Volts
    float _lastCurrent = 0.0f;     // Amps
    float _capacityMah = 0.0f;     // Accumulated mAh during discharge
    unsigned long _elapsedSeconds = 0;
    unsigned long _lastUpdateMs = 0;

    // Rolling history buffers (index 0 = oldest once full)
    float _voltageHistory[HISTORY_LENGTH];
    float _currentHistory[HISTORY_LENGTH];
    uint32_t _timeHistory[HISTORY_LENGTH];
    size_t _historyCount = 0;
    size_t _historyHead = 0;

    // Internal helpers
    float readVoltage();
    float readCurrent();
    void pushSample(float v, float a);
    void setOutputs(bool charge, bool discharge);
    void enterFault();
};

#endif  // BATTERY_TESTER_BATTERY_CHANNEL_H