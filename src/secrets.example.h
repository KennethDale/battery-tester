#ifndef BATTERY_TESTER_SECRETS_EXAMPLE_H
#define BATTERY_TESTER_SECRETS_EXAMPLE_H

// ---------------------------------------------------------------------------
// Copy this file to `secrets.h` (same directory) and fill in your WiFi
// credentials. `secrets.h` is git-ignored so your real password never gets
// committed.
//
//   cp src/secrets.example.h src/secrets.h
//
// The build picks up `secrets.h` automatically via -Isrc.
// ---------------------------------------------------------------------------

#define WIFI_SSID "your_network_name"
#define WIFI_PASS "your_network_password"

#endif  // BATTERY_TESTER_SECRETS_EXAMPLE_H