#include "BatteryChannel.h"

const char* channelStateToString(ChannelState s) {
    switch (s) {
        case ChannelState::IDLE:        return "idle";
        case ChannelState::CHARGING:    return "charging";
        case ChannelState::DISCHARGING: return "discharging";
        case ChannelState::COMPLETE:    return "complete";
        case ChannelState::FAULT:       return "fault";
        default:                        return "unknown";
    }
}

BatteryChannel::BatteryChannel(uint8_t index,
                               uint8_t adcPin,
                               uint8_t chargePin,
                               uint8_t dischargePin,
                               uint8_t loadEnablePin)
    : _index(index),
      _adcPin(adcPin),
      _chargePin(chargePin),
      _dischargePin(dischargePin),
      _loadEnablePin(loadEnablePin) {}

void BatteryChannel::begin() {
    pinMode(_chargePin, OUTPUT);
    pinMode(_dischargePin, OUTPUT);
    pinMode(_loadEnablePin, OUTPUT);
    setOutputs(false, false);

    _state = ChannelState::IDLE;
    _lastVoltage = 0.0f;
    _lastCurrent = 0.0f;
    _capacityMah = 0.0f;
    _elapsedSeconds = 0;
    _lastUpdateMs = millis();
    _historyCount = 0;
    _historyHead = 0;
}

float BatteryChannel::readVoltage() {
    // Average a few readings to reduce noise.
    uint32_t sum = 0;
    const uint8_t samples = 8;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += analogRead(_adcPin);
        delayMicroseconds(200);
    }
    float adc = static_cast<float>(sum) / samples;
    return (adc / ADC_MAX) * ADC_REFERENCE_V * VOLTAGE_DIVIDER_RATIO;
}

float BatteryChannel::readCurrent() {
    // Placeholder until a current-sense front-end is wired in.
    // A real implementation would read from a dedicated current-sense ADC
    // (e.g. INA219) and convert to amps. For now we report a nominal value
    // derived from the configured discharge target when the load is active.
    if (_state == ChannelState::DISCHARGING) {
        return DISCHARGE_CURRENT_MA / 1000.0f;
    }
    if (_state == ChannelState::CHARGING) {
        return DISCHARGE_CURRENT_MA / 1000.0f;  // symmetric default
    }
    return 0.0f;
}

void BatteryChannel::pushSample(float v, float a) {
    _voltageHistory[_historyHead] = v;
    _currentHistory[_historyHead] = a;
    _timeHistory[_historyHead] = _elapsedSeconds;
    _historyHead = (_historyHead + 1) % HISTORY_LENGTH;
    if (_historyCount < HISTORY_LENGTH) ++_historyCount;
}

void BatteryChannel::setOutputs(bool charge, bool discharge) {
    digitalWrite(_chargePin, charge ? HIGH : LOW);
    digitalWrite(_dischargePin, discharge ? HIGH : LOW);
    digitalWrite(_loadEnablePin, (charge || discharge) ? HIGH : LOW);
}

void BatteryChannel::enterFault() {
    _state = ChannelState::FAULT;
    setOutputs(false, false);
}

void BatteryChannel::startCharge() {
    _elapsedSeconds = 0;
    _capacityMah = 0.0f;
    _historyCount = 0;
    _historyHead = 0;
    _state = ChannelState::CHARGING;
    setOutputs(true, false);
}

void BatteryChannel::startDischarge() {
    _elapsedSeconds = 0;
    _capacityMah = 0.0f;
    _historyCount = 0;
    _historyHead = 0;
    _state = ChannelState::DISCHARGING;
    setOutputs(false, true);
}

void BatteryChannel::stop() {
    _state = ChannelState::IDLE;
    setOutputs(false, false);
}

void BatteryChannel::update() {
    unsigned long now = millis();
    float dtSec = static_cast<float>(now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    float v = readVoltage();
    float a = readCurrent();
    _lastVoltage = v;
    _lastCurrent = a;

    // Integrate capacity (mAh) during active discharge/charge.
    if (_state == ChannelState::CHARGING || _state == ChannelState::DISCHARGING) {
        _elapsedSeconds += static_cast<unsigned long>(dtSec + 0.5f);
        _capacityMah += (a * 1000.0f) * (dtSec / 3600.0f);
    }

    // State-machine safety / completion logic.
    switch (_state) {
        case ChannelState::CHARGING:
            if (v >= 4.20f) {
                _state = ChannelState::COMPLETE;
                setOutputs(false, false);
            }
            break;
        case ChannelState::DISCHARGING:
            if (v <= DISCHARGE_CUTOFF_V) {
                _state = ChannelState::COMPLETE;
                setOutputs(false, false);
            }
            break;
        default:
            break;
    }

    // Safety: any clearly out-of-range voltage (e.g. no cell present => 0V
    // reading during an active test) is treated as a fault.
    if ((_state == ChannelState::CHARGING || _state == ChannelState::DISCHARGING) &&
        (v > 4.50f || v < 0.50f)) {
        enterFault();
    }

    pushSample(v, a);
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