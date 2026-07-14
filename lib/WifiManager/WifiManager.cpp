#include "WifiManager.h"

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void startSoftAp() {
    const char* ssid = "CapacityTester";
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(ssid);
    Serial.print(F("[wifi] soft AP "));
    Serial.println(ok ? ssid : "FAILED");
    if (ok) {
        Serial.print(F("[wifi] AP IP: "));
        Serial.println(WiFi.softAPIP());
    }
}

void connectWifi() {
    // Detect the unchanged placeholder to avoid hanging on bogus credentials.
    if (strcmp(WIFI_SSID, "CHANGE_ME") == 0) {
        Serial.println(F("[wifi] no SSID configured, starting soft AP"));
        startSoftAp();
        return;
    }

    Serial.print(F("[wifi] connecting to "));
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const unsigned long timeoutMs = 20000UL;
    unsigned long start = millis();
    while (!wifiIsConnected()) {
        delay(250);
        Serial.print('.');
        if (millis() - start > timeoutMs) {
            Serial.println(F("\n[wifi] connect timeout, starting soft AP"));
            startSoftAp();
            return;
        }
    }

    Serial.println();
    Serial.print(F("[wifi] connected, IP: "));
    Serial.println(WiFi.localIP());
}