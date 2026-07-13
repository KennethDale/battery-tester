#include "BatteryChannel.h"

const char* channelStateToString(ChannelState s) {
    switch (s) {
        case ChannelState::WAITING:  return "waiting";
        case ChannelState::LOGGING:  return "logging";
        case ChannelState::COMPLETE: return "complete";
        default:                     return "unknown";
    }
}

BatteryChannel::BatteryChannel(uint8_t index, uint8_t i2cAddress)
    : _index(index), _ina(i2cAddress) {}

bool BatteryChannel::begin() {
    if (!_ina.begin()) {
        Serial.printf("[ch%u] INA219 not found\n", _index);
        return false;
    }
    // Use the default 32V/2A calibration; sufficient for battery logging.
    _ina.setCalibration_32V_2A();
    Serial.printf("[ch%u] INA219 ready\n", _index);

    _state = ChannelState::WAITING;
    _lastVoltage = 0.0f;
    _lastCurrent = 0.0f;
    _capacityMah = 0.0f;
    _elapsedSeconds = 0;
    _lastSampleMs = millis();
    _historyCount = 0;
    _historyHead = 0;
    return true;
}

void BatteryChannel::pushSample(float v, float a) {
    _voltageHistory[_historyHead] = v;
    _currentHistory[_historyHead] = a;
    _timeHistory[_historyHead] = _elapsedSeconds;
    _historyHead = (_historyHead + 1) % HISTORY_LENGTH;
    if (_historyCount < HISTORY_LENGTH) ++_historyCount;
}

void BatteryChannel::reset() {
    _state = ChannelState::WAITING;
    _lastVoltage = 0.0f;
    _lastCurrent = 0.0f;
    _capacityMah = 0.0f;
    _elapsedSeconds = 0;
    _lastSampleMs = millis();
    _historyCount = 0;
    _historyHead = 0;
}

void BatteryChannel::update() {
    // Read raw values from the INA219.
    float v = _ina.getBusVoltage_V();            // Vin- to GND (battery +)
    float shuntV = _ina.getShuntVoltage_mV() / 1000.0f;  // V across shunt
    // Current = V_shunt / R_shunt. Positive when the load is drawing.
    float a = shuntV / SHUNT_OHMS;

    _lastVoltage = v;
    _lastCurrent = a;

    // Elapsed time since last sample (seconds)
    unsigned long now = millis();
    float dtSec = static_cast<float>(now - _lastSampleMs) / 1000.0f;
    _lastSampleMs = now;

    switch (_state) {
        case ChannelState::WAITING:
            // Auto-detect battery connection.
            if (v > CONNECT_THRESHOLD_V) {
                _state = ChannelState::LOGGING;
                _elapsedSeconds = 0;
                _capacityMah = 0.0f;
                _historyCount = 0;
                _historyHead = 0;
                Serial.printf("[ch%u] battery connected (%.2fV), logging\n", _index, v);
            }
            break;

        case ChannelState::LOGGING:
            _elapsedSeconds += static_cast<unsigned long>(dtSec + 0.5f);
            // Integrate capacity (mAh). Only count when current is flowing
            // (negative shunt => current flowing out to the load).
            _capacityMah += (a > 0 ? a : -a) * 1000.0f * (dtSec / 3600.0f);

            // Auto-detect BMS disconnect / cutoff.
            if (v < DISCONNECT_THRESHOLD_V) {
                _state = ChannelState::COMPLETE;
                Serial.printf("[ch%u] battery disconnected (%.2fV), capacity=%.1f mAh\n",
                              _index, v, _capacityMah);
            }
            break;

        case ChannelState::COMPLETE:
            // Stay frozen until manually reset. No more accumulation.
            break;
    }

    // Record samples whenever we're active or just finished.
    if (_state != ChannelState::WAITING) {
        pushSample(v, a);
    }
}

void BatteryChannel::serializeLatest(String& out) const {
    out += "{\"channel\":";
    out += _index;
    out += ",\"state\":\"";
    out += channelStateToString(_state);
    out += "\",\"voltage\":";
    out += String(_lastVoltage, 3);
    out += ",\"current\":";
    out += String(_lastCurrent, 3);
    out += ",\"capacity_mah\":";
    out += String(_capacityMah, 1);
    out += ",\"elapsed_s\":";
    out += _elapsedSeconds;
    out += "}";
}

void BatteryChannel::serializeHistoryCsv(String& out) const {
    out += "elapsed_s,voltage,current_a\n";
    if (_historyCount == 0) return;
    size_t start = (_historyCount < HISTORY_LENGTH) ? 0 : _historyHead;
    for (size_t i = 0; i < _historyCount; ++i) {
        size_t idx = (start + i) % HISTORY_LENGTH;
        out += String(_timeHistory[idx]);
        out += ',';
        out += String(_voltageHistory[idx], 3);
        out += ',';
        out += String(_currentHistory[idx], 3);
        out += '\n';
    }
}