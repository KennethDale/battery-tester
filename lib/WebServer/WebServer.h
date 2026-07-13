#ifndef BATTERY_TESTER_WEB_SERVER_H
#define BATTERY_TESTER_WEB_SERVER_H

#include <ESP8266WebServer.h>
#include <BatteryChannel.h>

// Thin wrapper around ESP8266WebServer that registers all the routes used by
// the battery-tester web UI and JSON/CSV API.
//
// Routes:
//   GET  /                      -> index.html (from LittleFS)
//   GET  /api/status            -> JSON snapshot of all channels
//   GET  /api/channel/<n>       -> JSON snapshot of one channel
//   GET  /api/history/<n>.csv   -> CSV download of a channel's history
//   POST /api/channel/<n>/start -> start a test { "mode": "charge"|"discharge" }
//   POST /api/channel/<n>/stop  -> stop the running test
//   GET  /api/info              -> device info (chip id, firmware, uptime)
class BatteryWebServer {
public:
    // `channels` must outlive this server.
    BatteryWebServer(BatteryChannel* channels, uint8_t count);

    void begin();
    void loop();

private:
    ESP8266WebServer _server;
    BatteryChannel* _channels;
    uint8_t _count;

    // Route handlers
    void handleIndex();
    void handleStatus();
    void handleChannel();
    void handleHistoryCsv();
    void handleStart();
    void handleStop();
    void handleInfo();
    void handleNotFound();

    // Helpers
    bool validateChannelArg(uint8_t& outIndex);
    void sendJson(int code, const String& body);
};

#endif  // BATTERY_TESTER_WEB_SERVER_H