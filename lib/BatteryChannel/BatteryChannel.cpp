#include "BatteryChannel.h"

const char* channelStateToString(ChannelState s) {
    switch (s) {
        case ChannelState::WAITING:      return "waiting";
        case ChannelState::LOGGING:      return "logging";
        case ChannelState::LOW_VOLTAGE:  return "low_voltage";
        default:                         return "unknown";
    }
}

BatteryChannel::BatteryChannel(uint8_t index, uint8_t i2cAddress)
    : _index(index), _ina(i2cAddress) {}

bool BatteryChannel::begin() {
    _sensorConnected = _ina.begin();
    if (!_sensorConnected) {
        Serial.printf("[ch%u] INA219 not found\n", _index);
        return false;
    }
    // Use the default 32V/2A calibration; sufficient for battery logging.
    _ina.setCalibration_32V_2A();
    Serial.printf("[ch%u] INA219 ready\n", _index);

    _state = ChannelState::WAITING;
    _lastVoltage = 0.0f;
    _lastCurrent = 0.0f;
    _capacityMahAccum = 0.0f;
    _capacityWh = 0.0f;
    _elapsedMs = 0;
    _sinceResetMs = 0;
    _lastSampleMs = millis();
    _lastLoggedMs = millis();
    _lastLoggedVoltage = 0.0f;
    _lowVoltageEntryLogged = false;
    _debounceCount = 0;
    _voltageBeforeDip = 0.0f;
    _lowVoltageSinceMs = 0;
    _interrupted = false;
    _historyBaseMah = 0.0f;
    _historyCount = 0;
    _historyHead = 0;
    return true;
}

void BatteryChannel::pushSample(float v, float a) {
    // Store voltage/current as fixed-point millivolts/milliamps to save RAM.
    int16_t mv = static_cast<int16_t>(v * 1000.0f + 0.5f);
    int16_t ma = static_cast<int16_t>(a * 1000.0f + 0.5f);

    size_t idx;
    if (_historyCount < HISTORY_PRESERVE_SLOTS) {
        // Linear fill of the preserved start region.
        idx = _historyCount;
    } else {
        // Wrap around the remaining ring region.
        idx = HISTORY_PRESERVE_SLOTS + _historyHead;
        _historyHead = (_historyHead + 1) % (HISTORY_LENGTH - HISTORY_PRESERVE_SLOTS);
    }

    _voltageHistory[idx] = mv;
    _currentHistory[idx] = ma;
    _timeHistory[idx] = _elapsedMs / 1000UL;
    _resetTimeHistory[idx] = _sinceResetMs / 1000UL;

    if (_historyCount < HISTORY_LENGTH) {
        ++_historyCount;
    }
}

void BatteryChannel::integrateCapacity(float a, float v, float dtSec) {
    // Current magnitude in amps (positive or negative shunt orientation).
    float amps = (a > 0.0f) ? a : -a;
    _capacityMahAccum += amps * 1000.0f * (dtSec / 3600.0f);
    _capacityWh += amps * v * (dtSec / 3600.0f);
}

bool BatteryChannel::shouldLogSample(float v) const {
    if (_historyCount == 0) return true;
    if (fabsf(v - _lastLoggedVoltage) >= LOG_VOLTAGE_DELTA_V) return true;
    if (millis() - _lastLoggedMs >= LOG_MAX_QUIET_MS) return true;
    return false;
}

void BatteryChannel::reset() {
    _state = ChannelState::WAITING;
    _lastVoltage = 0.0f;
    _lastCurrent = 0.0f;
    _capacityMahAccum = 0.0f;
    _capacityWh = 0.0f;
    _elapsedMs = 0;
    _sinceResetMs = 0;
    _lastSampleMs = millis();
    _lastLoggedMs = millis();
    _lastLoggedVoltage = 0.0f;
    _lowVoltageEntryLogged = false;
    _debounceCount = 0;
    _voltageBeforeDip = 0.0f;
    _interrupted = false;
    // History is intentionally preserved across reset.
}

// Zero the totals and history and enter LOGGING. Shared by battery detection
// in WAITING and new-battery detection in LOW_VOLTAGE.
void BatteryChannel::startTest(unsigned long now, float v) {
    _state = ChannelState::LOGGING;
    _elapsedMs = 0;
    _sinceResetMs = 0;
    _capacityMahAccum = 0.0f;
    _capacityWh = 0.0f;
    _lastLoggedMs = now;
    _lastLoggedVoltage = v;
    _lowVoltageEntryLogged = false;
    _debounceCount = 0;
    _voltageBeforeDip = v;
    _interrupted = false;
    _historyBaseMah = 0.0f;
    _historyCount = 0;
    _historyHead = 0;
}

void BatteryChannel::captureSnapshot(ChannelSnapshot& out) const {
    out.capacityMah = _capacityMahAccum;
    out.capacityWh = _capacityWh;
    out.elapsedMs = _elapsedMs;
    out.sinceResetMs = _sinceResetMs;
    out.state = static_cast<uint8_t>(_state);
    out.interrupted = _interrupted ? 1 : 0;
    out.reserved[0] = 0;
    out.reserved[1] = 0;
}

bool BatteryChannel::restoreSnapshot(const ChannelSnapshot& s) {
    if (!_sensorConnected) return false;
    ChannelState st = static_cast<ChannelState>(s.state);
    if (st != ChannelState::LOGGING && st != ChannelState::LOW_VOLTAGE) {
        return false;  // no test in progress before the reboot
    }
    _state = st;
    _capacityMahAccum = s.capacityMah;
    _capacityWh = s.capacityWh;
    _elapsedMs = s.elapsedMs;
    _sinceResetMs = s.sinceResetMs;
    _interrupted = s.interrupted != 0;
    // Continue the CSV capacity column from the restored total; the pre-reboot
    // samples themselves are gone.
    _historyBaseMah = s.capacityMah;
    _historyCount = 0;
    _historyHead = 0;
    _lastSampleMs = millis();
    _lastLoggedMs = millis();
    _lastLoggedVoltage = 0.0f;  // forces an early history entry
    _lowVoltageEntryLogged = false;
    _debounceCount = 0;
    // The pre-dip voltage is unknown after a reboot; disable the voltage-jump
    // new-battery heuristic and rely on the LOW_VOLTAGE timeout alone.
    _voltageBeforeDip = 99.0f;
    _lowVoltageSinceMs = millis();
    Serial.printf("[ch%u] restored %s test after reboot: %u mAh, %lus elapsed\n",
                  _index, channelStateToString(st), capacityMah(), _elapsedMs / 1000UL);
    return true;
}

void BatteryChannel::update() {
    if (!_sensorConnected) return;

    unsigned long now = millis();
    unsigned long dtMs = now - _lastSampleMs;
    _lastSampleMs = now;

    // Read raw values from the INA219, dropping the sample if either I2C
    // transaction fails (loose wiring, bus noise) so garbage can't flip the
    // state machine or corrupt the totals.
    float v = _ina.getBusVoltage_V();                     // Vin- to GND (battery +)
    bool ok = _ina.success();
    float shuntV = _ina.getShuntVoltage_mV() / 1000.0f;   // V across shunt
    ok = ok && _ina.success();
    if (!ok) return;
    // Current = V_shunt / R_shunt. Positive when the load is drawing.
    float a = shuntV / SHUNT_OHMS;

    _lastVoltage = v;
    _lastCurrent = a;

    float dtSec = static_cast<float>(dtMs) / 1000.0f;

    switch (_state) {
        case ChannelState::WAITING:
            // Auto-detect battery connection (debounced).
            if (v > CONNECT_THRESHOLD_V) {
                if (++_debounceCount >= DEBOUNCE_SAMPLES) {
                    startTest(now, v);
                    // A battery present this soon after boot was connected
                    // before we started; part of its discharge is unmeasured.
                    _interrupted = (now < BOOT_CONNECT_GRACE_MS);
                    Serial.printf("[ch%u] battery connected (%.2fV), logging%s\n",
                                  _index, v, _interrupted ? " (interrupted test)" : "");
                }
            } else {
                _debounceCount = 0;
            }
            break;

        case ChannelState::LOGGING:
            _elapsedMs += dtMs;
            _sinceResetMs += dtMs;
            integrateCapacity(a, v, dtSec);

            // Log the sample if it meets the change/timeout criteria.
            if (shouldLogSample(v)) {
                pushSample(v, a);
                _lastLoggedVoltage = v;
                _lastLoggedMs = now;
            }

            // Transition to low-voltage suppression when the BMS cuts out
            // (debounced so one glitched sample can't end the test).
            if (v < DISCONNECT_THRESHOLD_V) {
                if (++_debounceCount >= DEBOUNCE_SAMPLES) {
                    _state = ChannelState::LOW_VOLTAGE;
                    _debounceCount = 0;
                    _lowVoltageSinceMs = now;
                    Serial.printf("[ch%u] battery disconnected (%.2fV), capacity=%u mAh\n",
                                  _index, v, capacityMah());
                }
            } else {
                _debounceCount = 0;
                _voltageBeforeDip = v;
            }
            break;

        case ChannelState::LOW_VOLTAGE:
            // Suppress repeated samples below the threshold. Log exactly one
            // entry for the dip, then wait until the voltage recovers.
            if (!_lowVoltageEntryLogged) {
                pushSample(v, a);
                _lastLoggedVoltage = v;
                _lastLoggedMs = now;
                _lowVoltageEntryLogged = true;
            }

            if (v > CONNECT_THRESHOLD_V) {
                if (++_debounceCount >= DEBOUNCE_SAMPLES) {
                    _debounceCount = 0;
                    // A long gap or a much higher voltage means a fresh
                    // battery, not the same one recovering from a dip.
                    bool longGap = (now - _lowVoltageSinceMs) >= LOW_VOLTAGE_NEW_TEST_MS;
                    bool jumped = v >= _voltageBeforeDip + NEW_TEST_VOLTAGE_JUMP_V;
                    if (longGap || jumped) {
                        startTest(now, v);
                        Serial.printf("[ch%u] new battery detected (%.2fV), starting fresh test\n",
                                      _index, v);
                    } else {
                        _state = ChannelState::LOGGING;
                        _lowVoltageEntryLogged = false;
                        Serial.printf("[ch%u] battery reconnected (%.2fV), resuming logging\n",
                                      _index, v);
                    }
                }
            } else {
                _debounceCount = 0;
            }
            break;
    }
}

void BatteryChannel::serializeLatest(String& out) const {
    out += "{\"channel\":";
    out += _index;
    out += ",\"state\":\"";
    out += channelStateToString(_state);
    out += "\",\"connected\":";
    out += _sensorConnected ? "true" : "false";
    out += ",\"interrupted\":";
    out += _interrupted ? "true" : "false";
    out += ",\"voltage\":";
    out += (_state == ChannelState::WAITING) ? "null" : String(_lastVoltage, 3);
    out += ",\"current\":";
    out += (_state == ChannelState::WAITING) ? "null" : String(_lastCurrent, 3);
    out += ",\"capacity_mah\":";
    out += capacityMah();
    out += ",\"capacity_wh\":";
    out += String(_capacityWh, 3);
    out += ",\"elapsed_s\":";
    out += _elapsedMs / 1000UL;
    out += ",\"seconds_since_reset\":";
    out += _sinceResetMs / 1000UL;
    out += "}";
}

bool BatteryChannel::serializeHistoryCsvChunk(String& out, CsvCursor& cur, size_t maxRows) const {
    if (cur.index == 0) {
        out += F("seconds_since_reset,elapsed_s,voltage,current_a,capacity_mah\n");
        // Reconstruct approximate capacity at each sample by integrating from
        // the start (or from the restored pre-reboot total).
        cur.capMah = _historyBaseMah;
    }

    size_t end = cur.index + maxRows;
    if (end > _historyCount) end = _historyCount;

    for (; cur.index < end; ++cur.index) {
        size_t i = cur.index;
        if (i > 0) {
            float dt = static_cast<float>(_timeHistory[i] - _timeHistory[i - 1]);
            float amps = _currentHistory[i] / 1000.0f;
            cur.capMah += fabsf(amps) * 1000.0f * (dt / 3600.0f);
        }
        out += String(_resetTimeHistory[i]);
        out += ',';
        out += String(_timeHistory[i]);
        out += ',';
        out += String(_voltageHistory[i] / 1000.0f, 3);
        out += ',';
        out += String(_currentHistory[i] / 1000.0f, 3);
        out += ',';
        out += String(static_cast<uint32_t>(cur.capMah + 0.5f));
        out += '\n';
    }
    return cur.index < _historyCount;
}
