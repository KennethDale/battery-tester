#include "WebServer.h"
#include <LittleFS.h>
#include "config.h"

BatteryWebServer::BatteryWebServer(BatteryChannel* channels, uint8_t count)
    : _server(WEB_SERVER_PORT), _channels(channels), _count(count) {}

void BatteryWebServer::begin() {
    _server.on("/", HTTP_GET, [this]() { handleIndex(); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/info", HTTP_GET, [this]() { handleInfo(); });
    _server.on("/api/channel", HTTP_GET, [this]() { handleChannel(); });
    _server.on("/api/history", HTTP_GET, [this]() { handleHistoryCsv(); });
    _server.on("/api/reset", HTTP_POST, [this]() { handleReset(); });
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
    if (!_server.hasArg("n")) {
        sendJson(400, F("{\"error\":\"missing channel index (use ?n=)\"}"));
        return false;
    }
    long idx = _server.arg("n").toInt();
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
                     F("<html><body><h1>Battery Logger</h1>"
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

void BatteryWebServer::handleReset() {
    uint8_t idx;
    if (!validateChannelArg(idx)) return;
    _channels[idx].reset();
    String body;
    _channels[idx].serializeLatest(body);
    sendJson(200, body);
}

void BatteryWebServer::handleInfo() {
    char body[256];
    snprintf(body, sizeof(body),
             "{\"firmware\":\"0.2.0\",\"chip_id\":\"0x%06X\",\"uptime_s\":%lu,\"channels\":%u,\"log_interval_s\":%u}",
             ESP.getChipId(),
             millis() / 1000UL,
             _count,
             LOG_INTERVAL_MS / 1000U);
    sendJson(200, body);
}

void BatteryWebServer::handleNotFound() {
    sendJson(404, F("{\"error\":\"not found\"}"));
}