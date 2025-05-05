#pragma once

#include <string>
#include <vector>
#include <functional>

// Mock WiFi class for testing
class MockWiFi {
public:
    static bool isConnected() { return connected; }
    static void setConnected(bool state) { connected = state; }
    
    static void setAPIP(const std::string& ip) { apIP = ip; }
    static void setSTAIP(const std::string& ip) { staIP = ip; }
    
    static std::string getAPIP() { return apIP; }
    static std::string getSTAIP() { return staIP; }

private:
    static bool connected;
    static std::string apIP;
    static std::string staIP;
};

bool MockWiFi::connected = false;
std::string MockWiFi::apIP = "192.168.4.1";
std::string MockWiFi::staIP = "192.168.1.100";

// Mock WiFi event handler
struct MockWiFiEvent {
    enum EventType {
        CONNECTED,
        DISCONNECTED,
        GOT_IP,
        AP_START,
        AP_STOP
    };

    EventType type;
    std::string data;
};

class MockWiFiEventHandler {
public:
    static void onEvent(MockWiFiEvent event) {
        for (auto& handler : handlers) {
            handler(event);
        }
    }

    static void addHandler(std::function<void(MockWiFiEvent)> handler) {
        handlers.push_back(handler);
    }

    static void clearHandlers() {
        handlers.clear();
    }

private:
    static std::vector<std::function<void(MockWiFiEvent)>> handlers;
};

std::vector<std::function<void(MockWiFiEvent)>> MockWiFiEventHandler::handlers;

#define WIFI_EVENT_STA_CONNECTED MockWiFiEvent::CONNECTED
#define WIFI_EVENT_STA_DISCONNECTED MockWiFiEvent::DISCONNECTED
#define WIFI_EVENT_STA_GOT_IP MockWiFiEvent::GOT_IP
#define WIFI_EVENT_AP_START MockWiFiEvent::AP_START
#define WIFI_EVENT_AP_STOP MockWiFiEvent::AP_STOP