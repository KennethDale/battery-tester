#ifndef BATTERY_TESTER_BATTERY_CHANNEL_H
#define BATTERY_TESTER_BATTERY_CHANNEL_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include "config.h"

// Logging states for a passive channel.
enum class ChannelState : uint8_t {
    WAITING = 0,    // No battery connected (voltage below threshold)
    LOGGING = 1,    // Battery detected, actively recording
    COMPLETE = 2,   // BMS disconnected the battery; capacity recorded
};

const char* channelStateToString(ChannelState s);

// One passive logging channel.
//
// Each channel wraps a single INA219 on the I2C bus. A pre-charged battery
// with a fixed load is connected to the INA219's Vin-/Vin+ shunt. The logger:
//   * auto-detects when a battery is connected (voltage > CONNECT_THRESHOLD_V),
//   * samples voltage / current / accumulated mAh every LOG_INTERVAL_MS,
//   * auto-detects BMS disconnect (voltage < DISCONNECT_THRESHOLD_V) and
//     freezes the recorded capacity as the final result.
class BatteryChannel {
public:
    BatteryChannel(uint8_t index, uint8_t i2cAddress);

    // Initialize the INA219. Returns false if the sensor wasn't found.
    bool begin();

    // True if the INA219 was found during begin().
    bool sensorConnected() const { return _sensorConnected; }

    // Read the INA219 and update the state machine. Call every LOG_INTERVAL_MS.
    void update();

    // Reset accumulated capacity and history; return to WAITING.
    void reset();

    // --- Accessors ----------------------------------------------------------
    uint8_t index() const { return _index; }
    ChannelState state() const { return _state; }
    float voltage() const { return _lastVoltage; }      // Volts (bus)
    float current() const { return _lastCurrent; }      // Amps (shunt)
    float capacityMah() const { return _capacityMah; }  // mAh accumulated
    float capacityWh() const { return _capacityWh; }    // Wh accumulated
    unsigned long elapsedSeconds() const { return _elapsedSeconds; }

    // Serialize the latest sample as a JSON object into `out`.
    void serializeLatest(String& out) const;

    // Write all buffered history samples (CSV) into `out`.
    void serializeHistoryCsv(String& out) const;

private:
    uint8_t _index;
    Adafruit_INA219 _ina;
    bool _sensorConnected = false;

    ChannelState _state = ChannelState::WAITING;
    float _lastVoltage = 0.0f;
    float _lastCurrent = 0.0f;
    float _capacityMah = 0.0f;
    float _capacityWh = 0.0f;
    unsigned long _elapsedSeconds = 0;
    unsigned long _lastSampleMs = 0;

    // Rolling history ring buffer, stored in fixed-point to save RAM.
    // Voltage and current are stored as millivolts / milliamps.
    int16_t _voltageHistory[HISTORY_LENGTH];
    int16_t _currentHistory[HISTORY_LENGTH];
    uint32_t _timeHistory[HISTORY_LENGTH];
    size_t _historyCount = 0;
    size_t _historyHead = 0;

    void pushSample(float v, float a);
};

#endif  // BATTERY_TESTER_BATTERY_CHANNEL_H