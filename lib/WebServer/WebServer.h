#ifndef BATTERY_TESTER_WEB_SERVER_H
#define BATTERY_TESTER_WEB_SERVER_H

#include <ESP8266WebServer.h>
#include <BatteryChannel.h>

// HTTP routes for the passive battery logger.
//
// Routes:
//   GET  /                      -> index.html (from LittleFS)
//   GET  /api/status            -> JSON snapshot of all channels
//   GET  /api/channel/<n>       -> JSON snapshot of one channel
//   GET  /api/history/<n>.csv   -> CSV download of a channel's history
//   POST /api/channel/<n>/reset -> clear a channel's data, return to WAITING
//   GET  /api/info              -> device info (chip id, firmware, uptime)
class BatteryWebServer {
public:
    BatteryWebServer(BatteryChannel* channels, uint8_t count);

    void begin();
    void loop();

private:
    ESP8266WebServer _server;
    BatteryChannel* _channels;
    uint8_t _count;

    void handleIndex();
    void handleStatus();
    void handleChannel();
    void handleHistoryCsv();
    void handleReset();
    void handleInfo();
    void handleNotFound();

    bool validateChannelArg(uint8_t& outIndex);
    void sendJson(int code, const String& body);
};

#endif  // BATTERY_TESTER_WEB_SERVER_H