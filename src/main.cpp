// ---------------------------------------------------------------------------
// 5-Channel Passive Battery Capacity Logger
// Target: ESP8266 (NodeMCU 1.0 / Wemos D1 R2)
//
// Each channel reads voltage/current from an INA219 sensor over I2C. A
// pre-charged battery with a fixed load attached is connected to each
// INA219. The logger:
//   * auto-detects when a battery is connected,
//   * logs voltage, current, and accumulated capacity every 5 seconds,
//   * stops when the BMS disconnects the battery (auto-detect),
//   * serves a simple web UI showing final capacity per channel.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>
#include <ESP8266mDNS.h>
#include <coredecls.h>  // crc32()

#include "config.h"
#include "BatteryChannel.h"
#include "WebServer.h"
#include "WifiManager.h"

// --- I2C / INA219 setup ----------------------------------------------------
// ESP8266 default I2C: SCL=D1(GPIO5), SDA=D2(GPIO4). Each INA219 breakout
// needs a unique address set via its A0/A1 solder jumpers (0x40..0x4F).
static const uint8_t INA_ADDRESSES[NUM_CHANNELS] = INA219_ADDRESSES;

// --- Globals ---------------------------------------------------------------
BatteryChannel* channels = nullptr;
BatteryWebServer* webServer = nullptr;

// --- RTC snapshot ------------------------------------------------------------
// Channel state is mirrored into RTC user memory once per sample tick so an
// in-progress test survives a crash or watchdog reset. RTC memory is cleared
// by power loss; in that case channels that find a battery already connected
// at boot flag their test "interrupted" instead (see BatteryChannel::update).
struct RtcSnapshot {
    uint32_t magic;
    uint16_t version;
    uint16_t numChannels;
    ChannelSnapshot ch[NUM_CHANNELS];
    uint32_t crc;
};
static_assert(sizeof(RtcSnapshot) % 4 == 0, "RTC writes need 4-byte multiples");
static_assert(sizeof(RtcSnapshot) <= 512, "RTC user memory is 512 bytes");

static const uint32_t RTC_MAGIC = 0x42544C47;  // "BTLG"
static const uint16_t RTC_VERSION = 2;  // v2 adds ChannelSnapshot::startVoltage
static RtcSnapshot rtcSnap;

static void saveRtcSnapshot() {
    rtcSnap.magic = RTC_MAGIC;
    rtcSnap.version = RTC_VERSION;
    rtcSnap.numChannels = NUM_CHANNELS;
    for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
        channels[i].captureSnapshot(rtcSnap.ch[i]);
    }
    rtcSnap.crc = crc32(&rtcSnap, sizeof(rtcSnap) - sizeof(rtcSnap.crc));
    ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtcSnap), sizeof(rtcSnap));
}

static void restoreRtcSnapshot() {
    if (!ESP.rtcUserMemoryRead(0, reinterpret_cast<uint32_t*>(&rtcSnap), sizeof(rtcSnap))) {
        return;
    }
    if (rtcSnap.magic != RTC_MAGIC || rtcSnap.version != RTC_VERSION ||
        rtcSnap.numChannels != NUM_CHANNELS ||
        rtcSnap.crc != crc32(&rtcSnap, sizeof(rtcSnap) - sizeof(rtcSnap.crc))) {
        Serial.println(F("[boot] no valid RTC snapshot (cold boot)"));
        return;
    }
    uint8_t restored = 0;
    for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
        if (channels[i].restoreSnapshot(rtcSnap.ch[i])) ++restored;
    }
    Serial.printf("[boot] restored %u in-progress test(s) from RTC memory\n", restored);
}

// --- Helpers ---------------------------------------------------------------
static void scanI2C() {
    Serial.println(F("[i2c] scanning bus..."));
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("[i2c] device found at 0x%02X\n", addr);
            ++found;
        }
    }
    Serial.printf("[i2c] scan complete, %u device(s) found\n", found);
}

static BatteryChannel* makeChannels(const uint8_t addresses[], uint8_t count) {
    // Allocate raw storage, then construct each channel in place.
    void* mem = ::operator new[](sizeof(BatteryChannel) * count);
    BatteryChannel* arr = static_cast<BatteryChannel*>(mem);
    for (uint8_t i = 0; i < count; ++i) {
        new (&arr[i]) BatteryChannel(i, addresses[i]);
    }
    return arr;
}

// --- setup / loop ----------------------------------------------------------
void setup() {
    Serial.begin(BAUD_RATE);
    delay(100);
    Serial.println();
    Serial.println(F("[boot] battery logger starting"));

    // I2C for the INA219 sensors
    Wire.begin();
    Serial.println(F("[boot] I2C started"));
    scanI2C();

    // Mount LittleFS (web UI assets)
    if (!LittleFS.begin()) {
        Serial.println(F("[boot] LittleFS mount failed"));
    } else {
        Serial.println(F("[boot] LittleFS mounted"));
    }

    // Allocate channels dynamically so NUM_CHANNELS can be changed without
    // editing the initializer list.
    channels = makeChannels(INA_ADDRESSES, NUM_CHANNELS);

    // Initialize each INA219
    uint8_t found = 0;
    for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
        if (channels[i].begin()) {
            ++found;
        }
    }
    Serial.printf("[boot] %u/%u INA219 sensors found\n", found, NUM_CHANNELS);

    restoreRtcSnapshot();

    connectWifi();

    if (MDNS.begin("batterylogger")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println(F("[boot] mDNS: http://batterylogger.local"));
    }

    webServer = new BatteryWebServer(channels, NUM_CHANNELS);
    webServer->begin();
    Serial.println(F("[boot] ready"));
}

void loop() {
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    // Sample every channel at the configured interval
    if (now - lastTick >= SAMPLE_INTERVAL_MS) {
        lastTick = now;
        for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
            channels[i].update();
        }
        // Mirror state to RTC memory so tests survive a crash/watchdog reset.
        saveRtcSnapshot();
    }

    webServer->loop();
    MDNS.update();
}