#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H
#include <Arduino.h>

// AP mode constants
extern const char* AP_SSID;
extern const char* AP_PASSWORD;

void scanWifiNetworks();
void connectToWifi(String ssid, String password);
void disconnectWifi();

// AP Mode functionality
bool startAPMode(); // Start ESP32 in Access Point mode for WiFi configuration
bool isAPModeActive(); // Check if AP mode is currently active
void handleWiFiConfig(); // Handle WiFi configuration web server when in AP mode

#endif // WIFI_MODULE_H
