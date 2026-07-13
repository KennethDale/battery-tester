#include "WebServer.h"
#include <LittleFS.h>
#include "config.h"

BatteryWebServer::BatteryWebServer(BatteryChannel* channels, uint8_t count)
    : _server(WEB_SERVER_PORT), _channels(channels), _count(count) {}

void BatteryWebServer::begin() {
    _server.on("/", HTTP_GET, [this]() { handleIndex(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/info", HTTP_GET, [this]() { handleInfo(); });
    _server.on(UriBraces("/api/channel/{}"), HTTP_GET, [this]() { handleChannel(); });
    _server.on(UriBraces("/api/history/{}.csv"), HTTP_GET, [this]() { handleHistoryCsv(); });
    _server.on(UriBraces("/api/channel/{}/start"), HTTP_POST, [this]() { handleStart(); });
    _server.on(UriBraces("/api/channel/{}/stop"), HTTP_POST, [this]() { handleStop(); });
    _server.onNotFound([this]() { handleNotFound(); });

    _server.serveStatic("/static/", LittleFS, "/static/");
    _server.begin();
    Serial.println(F("[web] server started"));
}

void BatteryWebServer::loop() { _server.handleClient(); }

void BatteryWebServer::sendJson(int code, const String& body) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(code, "application/json", body);
}

bool BatteryWebServer::validateChannelArg(uint8_t& outIndex) {
    if (!_server.hasArg("plain") && _server.pathArg(0).length() == 0) {
        sendJson(400, F("{\"error\":\"missing channel index\"}"));
        return false;
    }
    long idx = _server.pathArg(0).toInt();
    if (idx < 0 || idx >= _count) {
        sendJson(404, F("{\"error\":\"channel out of range\"}"));
        return false;
    }
    outIndex = static_cast<uint8_t>(idx);
    return true;
}

void BatteryWebServer::handleIndex() {
    if (LittleFS.exists("/index.html")) {
        File f = LittleFS.open("/index.html", "r");
        _server.streamFile(f, "text/html");
        f.close();
    } else {
        _server.send(200, "text/html",
                     F("<html><body><h1>Battery Tester</h1>"
                       "<p>Upload the LittleFS image to enable the UI.</p></body></html>"));
    }
}

void BatteryWebServer::handleStatus() {
    String body = "{\"channels\":[";
    for (uint8_t i = 0; i < _count; ++i) {
        _channels[i].serializeLatest(body);
        if (i < _count - 1) body += ',';
    }
    body += "]}";
    sendJson(200, body);
}

void BatteryWebServer::handleChannel() {
    uint8_t idx;
    if (!validateChannelArg(idx)) return;
    String body;
    _channels[idx].serializeLatest(body);
    sendJson(200, body);
}

void BatteryWebServer::handleHistoryCsv() {
    uint8_t idx;
    if (!validateChannelArg(idx)) return;

    String csv;
    _channels[idx].serializeHistoryCsv(csv);

    char name[32];
    snprintf(name, sizeof(name), "channel_%u.csv", idx);
    _server.sendHeader("Content-Disposition", String("attachment; filename=\"") + name + "\"");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "text/csv", csv);
}

void BatteryWebServer::handleStart() {
    uint8_t idx;
    if (!validateChannelArg(idx)) return;

    String mode;
    if (_server.hasArg("plain")) {
        // Parse {"mode":"charge"} lightly without a full JSON dependency.
        String raw = _server.arg("plain");
        if (raw.indexOf("charge") >= 0) mode = "charge";
        else if (raw.indexOf("discharge") >= 0) mode = "discharge";
    }

    if (mode == "charge") {
        _channels[idx].startCharge();
    } else if (mode == "discharge") {
        _channels[idx].startDischarge();
    } else {
        sendJson(400, F("{\"error\":\"missing or invalid 'mode'\"}"));
        return;
    }

    String body;
    _channels[idx].serializeLatest(body);
    sendJson(200, body);
}

void BatteryWebServer::handleStop() {
    uint8_t idx;
    if (!validateChannelArg(idx)) return;
    _channels[idx].stop();
    String body;
    _channels[idx].serializeLatest(body);
    sendJson(200, body);
}

void BatteryWebServer::handleInfo() {
    char body[256];
    snprintf(body, sizeof(body),
             "{\"firmware\":\"0.1.0\",\"chip_id\":\"0x%06X\",\"uptime_s\":%lu,\"channels\":%u}",
             ESP.getChipId(),
             millis() / 1000UL,
             _count);
    sendJson(200, body);
}

void BatteryWebServer::handleNotFound() {
    sendJson(404, F("{\"error\":\"not found\"}"));
}