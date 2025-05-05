#ifndef BLE_CALLBACKS_H
#define BLE_CALLBACKS_H

#include <BLEServer.h>          // Defines BLEServerCallbacks
#include <BLECharacteristic.h>  // Defines BLECharacteristicCallbacks
#include "ble_module.h"         // For deviceConnected, BLEDevice, etc.
#include "wifi_module.h"        // For scanWifiNetworks, connectToWifi
#include "relay_module.h"       // For relayPins, relayStates, blinkRelayFeedback
#include "adc_module.h"         // For calibrateADC
#include <ArduinoOTA.h>
#include <Preferences.h>
#include "config.h"             // For LOG_INFO macros and prefs

// Command validation structure for documenting and validating incoming commands
struct CommandDefinition {
    const char* command;
    const char* format;
    const char* description;
};

// Array of supported commands with their expected formats and descriptions
const CommandDefinition SUPPORTED_COMMANDS[] = {
    {"CALIBRATE", "CALIBRATE", "Initiates ADC calibration procedure"},
    {"OTA", "OTA", "Enables Over-The-Air updates"},
    {"TOGGLE", "TOGGLE_<pin>", "Toggles the relay with the specified pin number"},
    {"SET", "SET_<pin>_<ON|OFF>", "Sets the relay with specified pin to ON or OFF"},
    {"SET_SAMPLING_RATE", "SET_SAMPLING_RATE_<interval>", "Sets sampling interval in ms (5-1000)"},
    {"SCAN", "SCAN", "Scans for available WiFi networks"},
    {"SELECT", "SELECT_<ssid>:<password>", "Connects to specified WiFi network"},
    {"DISCONNECT", "DISCONNECT", "Disconnects from WiFi network"}
};

// Make function inline to avoid multiple definition errors
inline bool validateCommand(const String& command, String& errorMessage) {
    if (command == "CALIBRATE" || command == "OTA" || command == "SCAN" || command == "DISCONNECT") {
        return true;
    } 
    else if (command.startsWith("TOGGLE_")) {
        int pin = command.substring(7).toInt();
        bool validPin = false;
        for (int i = 0; i < 4; i++) {
            if (relayPins[i] == pin) {
                validPin = true;
                break;
            }
        }
        if (!validPin) {
            errorMessage = "ERROR:INVALID_PIN:" + String(pin);
            return false;
        }
        return true;
    }
    else if (command.startsWith("SET_")) {
        int pin = command.substring(4, 6).toInt();
        String stateStr = command.substring(7);
        bool validPin = false;
        
        for (int i = 0; i < 4; i++) {
            if (relayPins[i] == pin) {
                validPin = true;
                break;
            }
        }
        
        if (!validPin) {
            errorMessage = "ERROR:INVALID_PIN:" + String(pin);
            return false;
        }
        
        if (stateStr != "ON" && stateStr != "OFF") {
            errorMessage = "ERROR:INVALID_STATE:" + stateStr;
            return false;
        }
        
        return true;
    }
    else if (command.startsWith("SET_SAMPLING_RATE_")) {
        int interval = command.substring(18).toInt();
        if (interval < 5 || interval > 1000) {
            errorMessage = "ERROR:INVALID_SAMPLING_RATE:Value must be between 5-1000";
            return false;
        }
        return true;
    }
    else if (command.startsWith("SELECT_")) {
        String wifiData = command.substring(7);
        int colonIndex = wifiData.indexOf(':');
        if (colonIndex == -1) {
            errorMessage = "ERROR:INVALID_WIFI_FORMAT:Missing colon separator";
            return false;
        }
        String ssid = wifiData.substring(0, colonIndex);
        if (ssid.length() == 0) {
            errorMessage = "ERROR:INVALID_SSID:Empty SSID";
            return false;
        }
        return true;
    }
    
    errorMessage = "ERROR:UNKNOWN_COMMAND:" + command;
    return false;
}

// Make function inline to avoid multiple definition errors
inline bool checkProtocolVersionCompatibility(const String& clientVersion, String& errorMessage) {
    // Extract version components
    int clientMajor = 0, clientMinor = 0, clientPatch = 0;
    sscanf(clientVersion.c_str(), "%d.%d.%d", &clientMajor, &clientMinor, &clientPatch);
    
    // Compare with our version
    if (clientMajor != PROTOCOL_VERSION_MAJOR) {
        errorMessage = "ERROR:INCOMPATIBLE_VERSION:Major version mismatch";
        return false;
    }
    
    // Minor version differences are okay if client is lower than server
    if (clientMinor > PROTOCOL_VERSION_MINOR) {
        errorMessage = "ERROR:INCOMPATIBLE_VERSION:Client using newer minor version";
        return false;
    }
    
    return true;
}

class MyServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        LOG_INFO("Device connected");
        
        // Send protocol version information on connection
        if (pDataCharacteristic) {
            String versionInfo = "{\"protocol_version\":\"" PROTOCOL_VERSION "\",\"device_name\":\"ESP32_ADS1115\"}";
            pDataCharacteristic->setValue(versionInfo.c_str());
            pDataCharacteristic->notify();
            LOG_INFO("Sent protocol version: %s", PROTOCOL_VERSION);
        }
    }
    
    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        LOG_INFO("Device disconnected");
    }
};

class RelayControlCallback : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        String command = String(value.c_str());
        LOG_INFO("Received command: %s", command.c_str());
        
        // Validate command
        String errorMessage = "";
        if (!validateCommand(command, errorMessage)) {
            LOG_ERROR("Command validation failed: %s", errorMessage.c_str());
            if (pRelayCharacteristic) {
                pRelayCharacteristic->setValue(errorMessage.c_str());
                pRelayCharacteristic->notify();
            }
            return;
        }
        
        // Process valid command
        if (command == "CALIBRATE") {
            LOG_INFO("Calibration started");
            calibrateADC();
            LOG_INFO("Calibration complete");
            if (pRelayCharacteristic) {
                pRelayCharacteristic->setValue("LOG:Calibration complete");
                pRelayCharacteristic->notify();
            }
        } else if (command == "OTA") {
            if (pRelayCharacteristic) {
                pRelayCharacteristic->setValue("OTA:START");
                pRelayCharacteristic->notify();
            }
            // OTA is handled in main loop/task by ArduinoOTA.handle()
        } else if (command.startsWith("TOGGLE_")) {
            int pin = command.substring(7).toInt();
            for (int i = 0; i < 4; i++) {
                if (relayPins[i] == pin) {
                    relayStates[i] = !relayStates[i];
                    digitalWrite(relayPins[i], relayStates[i] ? HIGH : LOW);
                    blinkRelayFeedback();
                    LOG_INFO("Relay %d toggled to %s", pin, relayStates[i] ? "ON" : "OFF");
                    
                    // Save relay state to preferences
                    prefs.putBool(("relay" + String(i)).c_str(), relayStates[i]);
                    
                    if (pRelayCharacteristic) {
                        pRelayCharacteristic->setValue(String("LOG:Relay " + String(pin) + " toggled to " + (relayStates[i] ? "ON" : "OFF")).c_str());
                        pRelayCharacteristic->notify();
                    }
                    if (pRelayCharacteristic) {
                        pRelayCharacteristic->setValue(String("RELAY_UPDATE:" + String(pin) + ":" + (relayStates[i] ? "ON" : "OFF")).c_str());
                        pRelayCharacteristic->notify();
                    }
                    break;
                }
            }
        } else if (command.startsWith("SET_")) {
            int pin = command.substring(4, 6).toInt();
            String stateStr = command.substring(7);
            bool state = (stateStr == "ON");
            for (int i = 0; i < 4; i++) {
                if (relayPins[i] == pin) {
                    relayStates[i] = state;
                    digitalWrite(relayPins[i], state ? HIGH : LOW);
                    blinkRelayFeedback();
                    
                    // Save relay state to preferences
                    prefs.putBool(("relay" + String(i)).c_str(), relayStates[i]);
                    
                    if (pRelayCharacteristic) {
                        pRelayCharacteristic->setValue(String("RELAY_UPDATE:" + String(pin) + ":" + (state ? "ON" : "OFF")).c_str());
                        pRelayCharacteristic->notify();
                    }
                    break;
                }
            }
        } else if (command.startsWith("SET_SAMPLING_RATE_")) {
            int interval = command.substring(18).toInt();
            if (interval >= 5 && interval <= 1000) {
                extern volatile uint16_t samplingIntervalMs;
                samplingIntervalMs = interval;
                prefs.putUInt("samplingIntervalMs", interval);
                if (pRelayCharacteristic) {
                    pRelayCharacteristic->setValue(String("SAMPLING_RATE:" + String(interval)).c_str());
                    pRelayCharacteristic->notify();
                }
            }
        }
    }
};

class WifiControlCallback : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        String command = String(value.c_str());
        LOG_INFO("Received WiFi command: %s", command.c_str());
        
        // Validate command
        String errorMessage = "";
        if (!validateCommand(command, errorMessage)) {
            LOG_ERROR("WiFi command validation failed: %s", errorMessage.c_str());
            if (pWifiCharacteristic) {
                pWifiCharacteristic->setValue(errorMessage.c_str());
                pWifiCharacteristic->notify();
            }
            return;
        }
        
        // Process valid command
        if (command == "SCAN") {
            scanWifiNetworks();
        } else if (command.startsWith("SELECT_")) {
            String wifiData = command.substring(7);
            int colonIndex = wifiData.indexOf(':');
            if (colonIndex != -1) {
                String ssid = wifiData.substring(0, colonIndex);
                String password = wifiData.substring(colonIndex + 1);
                connectToWifi(ssid, password);
            }
        } else if (command == "DISCONNECT") {
            disconnectWifi();
        }
    }
};

#endif // BLE_CALLBACKS_H