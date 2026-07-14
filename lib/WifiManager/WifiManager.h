#ifndef BATTERY_TESTER_WIFI_MANAGER_H
#define BATTERY_TESTER_WIFI_MANAGER_H

#include <ESP8266WiFi.h>

// Minimal WiFi credential holder.
//
// Copy `secrets.example.h` to `secrets.h`, fill in your SSID/password, and the
// build will pick it up automatically. `secrets.h` is git-ignored.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "CHANGE_ME"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "CHANGE_ME"
#endif

// Connect to the configured WiFi network (blocking, with retries).
// Prints progress to Serial. If SSID is still the placeholder, the board will
// instead start a soft AP named "CapacityTester" so the UI stays usable
// for offline / bench testing.
void connectWifi();

// True when the board is joined to a network in STA mode.
bool wifiIsConnected();

// Start the fallback soft AP.
void startSoftAp();

#endif  // BATTERY_TESTER_WIFI_MANAGER_H