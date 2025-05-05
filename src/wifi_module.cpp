#include "wifi_module.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include "ble_module.h"
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

extern Preferences prefs;

// Define the AP mode constants
const char* AP_SSID = "ESP32-Setup";
const char* AP_PASSWORD = "configme";

// Global variables for AP mode
WebServer server(80);
bool apModeActive = false;

// HTML for the configuration page (if SPIFFS is not available)
const char* CONFIG_HTML = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 WiFi Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0 auto; max-width: 400px; padding: 20px; }
        input, button { display: block; width: 100%; padding: 10px; margin: 10px 0; }
        h1 { color: #0066cc; }
    </style>
</head>
<body>
    <h1>ESP32 WiFi Setup</h1>
    <form method="post" action="/connect">
        <label for="ssid">WiFi Network:</label>
        <input type="text" id="ssid" name="ssid" required>
        <label for="password">Password:</label>
        <input type="password" id="password" name="password">
        <button type="submit">Connect</button>
    </form>
    <div id="status"></div>
    <button onclick="scanNetworks()">Scan Networks</button>
    <div id="networks"></div>
    <script>
        function scanNetworks() {
            fetch('/scan')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('networks').innerHTML = data;
                });
        }
    </script>
</body>
</html>
)html";

void scanWifiNetworks() {
    int numNetworks = WiFi.scanNetworks();
    if (numNetworks == 0) {
        pWifiCharacteristic->setValue("No networks found");
    } else {
        String networkList = "";
        for (int i = 0; i < numNetworks; i++) {
            if (i > 0) networkList += ",";
            networkList += WiFi.SSID(i) + "(" + String(WiFi.RSSI(i)) + ")";
        }
        pWifiCharacteristic->setValue(networkList.c_str());
    }
    pWifiCharacteristic->notify();
}

void disconnectWifi() {
    WiFi.disconnect(true);
    if (pWifiCharacteristic) {
        pWifiCharacteristic->setValue("WIFI_STATUS:DISCONNECTED");
        pWifiCharacteristic->notify();
    }
}

void connectToWifi(String ssid, String password) {
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 10000)) {
        delay(500);
    }
    String statusMsg;
    if (WiFi.status() == WL_CONNECTED) {
        prefs.putString("ssid", ssid);
        prefs.putString("password", password);
        ArduinoOTA.begin();
        int rssi = WiFi.RSSI();
        statusMsg = String("WIFI_STATUS:CONNECTED:" + WiFi.SSID() + ":" + String(rssi));
    } else {
        statusMsg = String("WIFI_STATUS:FAILED:" + ssid);
        if (pWifiCharacteristic) {
            pWifiCharacteristic->setValue(String("ERROR:WIFI:CONNECT_FAIL:" + ssid).c_str());
            pWifiCharacteristic->notify();
        }
    }
    if (pWifiCharacteristic) {
        pWifiCharacteristic->setValue(statusMsg.c_str());
        pWifiCharacteristic->notify();
    }
}

// AP Mode functionality implementation
bool startAPMode() {
    // Initialize SPIFFS for serving the configuration page
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        // We'll use the embedded HTML if SPIFFS fails
    }
    
    // Set up AP mode
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    if (!success) {
        Serial.println("Failed to start AP mode");
        return false;
    }
    
    Serial.print("AP Started with IP: ");
    Serial.println(WiFi.softAPIP());
    
    // Set up mDNS responder for http://esp32setup.local
    if (MDNS.begin("esp32setup")) {
        Serial.println("mDNS responder started: http://esp32setup.local");
    }
    
    // Set up server routes
    server.on("/", HTTP_GET, []() {
        if (SPIFFS.exists("/wifi_setup.html")) {
            File file = SPIFFS.open("/wifi_setup.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            server.send(200, "text/html", CONFIG_HTML);
        }
    });
    
    server.on("/scan", HTTP_GET, []() {
        String html = "<ul>";
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; ++i) {
            html += "<li onclick=\"document.getElementById('ssid').value='" + WiFi.SSID(i) + "'\">";
            html += WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)";
            html += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : " 🔒";
            html += "</li>";
        }
        html += "</ul>";
        server.send(200, "text/html", html);
    });
    
    server.on("/connect", HTTP_POST, []() {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        
        if (ssid.length() > 0) {
            // Save credentials
            prefs.putString("ssid", ssid);
            prefs.putString("password", password);
            
            server.send(200, "text/html", "<html><body><h1>Credentials Saved</h1><p>ESP32 will restart and try to connect to " + ssid + "</p></body></html>");
            
            // Wait for response to be sent
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "SSID is required");
        }
    });
    
    server.begin();
    apModeActive = true;
    return true;
}

bool isAPModeActive() {
    return apModeActive;
}

void handleWiFiConfig() {
    if (apModeActive) {
        server.handleClient();
    }
}
