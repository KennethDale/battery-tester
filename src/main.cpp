// ---------------------------------------------------------------------------
// 5-Channel Battery Tester - main firmware entry point
// Target: ESP8266 (NodeMCU 1.0 / Wemos D1 R2)
//
// This firmware:
//   * samples voltage/current on 5 channels through an analog mux,
//   * drives charge/discharge MOSFETs to run a per-cell test,
//   * exposes a JSON + CSV HTTP API and serves a web UI from LittleFS,
//   * prints status lines to Serial for the optional Python companion.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>

#include "config.h"
#include "BatteryChannel.h"
#include "WebServer.h"
#include "WifiManager.h"

// --- Pin map ---------------------------------------------------------------
// The ESP8266 exposes a single ADC pin (A0). 5 channels share it through an
// analog multiplexer (e.g. CD4051/CD4052). The mux select lines D1..D3 pick
// the active channel. A dedicated ADC (e.g. ADS1115 via I2C) is recommended
// for production; this scaffold keeps the mapping in one place.
//
// CHARGE_PIN / DISCHARGE_PIN / LOAD_ENABLE_PIN below are example mappings to
// GPIOs wired to gate drivers. Adjust to match your hardware.
static const uint8_t MUX_SELECT_PINS[3] = {D1, D2, D3};
static const uint8_t SHARED_ADC_PIN      = A0;

static const uint8_t CHARGE_PINS[NUM_CHANNELS]      = {D5, D6, D7, D8, D0};  // gate enables
static const uint8_t DISCHARGE_PINS[NUM_CHANNELS]   = {D5, D6, D7, D8, D0};  // shared example
static const uint8_t LOAD_ENABLE_PINS[NUM_CHANNELS] = {D4, D4, D4, D4, D4};  // global enable

// --- Globals ---------------------------------------------------------------
BatteryChannel channels[NUM_CHANNELS] = {
    BatteryChannel(0, SHARED_ADC_PIN, CHARGE_PINS[0], DISCHARGE_PINS[0], LOAD_ENABLE_PINS[0]),
    BatteryChannel(1, SHARED_ADC_PIN, CHARGE_PINS[1], DISCHARGE_PINS[1], LOAD_ENABLE_PINS[1]),
    BatteryChannel(2, SHARED_ADC_PIN, CHARGE_PINS[2], DISCHARGE_PINS[2], LOAD_ENABLE_PINS[2]),
    BatteryChannel(3, SHARED_ADC_PIN, CHARGE_PINS[3], DISCHARGE_PINS[3], LOAD_ENABLE_PINS[3]),
    BatteryChannel(4, SHARED_ADC_PIN, CHARGE_PINS[4], DISCHARGE_PINS[4], LOAD_ENABLE_PINS[4]),
};
BatteryWebServer webServer(channels, NUM_CHANNELS);

// --- Helpers ---------------------------------------------------------------
static void selectMuxChannel(uint8_t ch) {
    // 3 select lines -> up to 8 inputs; we use the low 3 bits of `ch`.
    digitalWrite(MUX_SELECT_PINS[0], bitRead(ch, 0));
    digitalWrite(MUX_SELECT_PINS[1], bitRead(ch, 1));
    digitalWrite(MUX_SELECT_PINS[2], bitRead(ch, 2));
}

// One pass of the per-channel control loop.
static void runChannelTick() {
    static uint8_t activeChannel = 0;
    // Round-robin: pick one channel per tick to give the ADC settle time.
    selectMuxChannel(activeChannel);
    // Minimal settle delay; tune for your mux + divider impedance.
    delayMicroseconds(500);
    channels[activeChannel].update();
    activeChannel = (activeChannel + 1) % NUM_CHANNELS;
}

// --- setup / loop ----------------------------------------------------------
void setup() {
    Serial.begin(BAUD_RATE);
    delay(100);
    Serial.println();
    Serial.println(F("[boot] battery tester starting"));

    // Mux select pins as outputs
    for (uint8_t i = 0; i < 3; ++i) {
        pinMode(MUX_SELECT_PINS[i], OUTPUT);
    }

    // Mount LittleFS (web UI assets)
    if (!LittleFS.begin()) {
        Serial.println(F("[boot] LittleFS mount failed"));
    } else {
        Serial.println(F("[boot] LittleFS mounted"));
    }

    for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
        channels[i].begin();
    }

    connectWifi();

    if (MDNS.begin("batterytester")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println(F("[boot] mDNS: http://batterytester.local"));
    }

    webServer.begin();
    Serial.println(F("[boot] ready"));
}

void loop() {
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (now - lastTick >= LOOP_INTERVAL_MS) {
        lastTick = now;
        runChannelTick();
    }

    webServer.loop();
    MDNS.update();
}