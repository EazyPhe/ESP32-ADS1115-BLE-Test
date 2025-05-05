#include <Arduino.h>
#include <LittleFS.h>
#include "NetworkManager.h"
#include "MCPServer.h"

using namespace mcp;
// Global instances
NetworkManager networkManager;
MCPServer mcpServer;

// Task handles
TaskHandle_t mcpTaskHandle = nullptr;

// MCP task function
void mcpTask(void* parameter) {
    while (true) {
        mcpServer.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up...");

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    // Start network manager
    networkManager.begin();

    // Wait for network connection or AP mode
    while (!networkManager.isConnected() && networkManager.getIPAddress().isEmpty()) {
        delay(100);
    }

    Serial.print("Device IP: ");
    Serial.println(networkManager.getIPAddress());

    // Initialize MCP server
    mcpServer.begin(networkManager.isConnected());

    // Create MCP task
    xTaskCreatePinnedToCore(
        mcpTask,
        "MCPTask",
        8192,
        nullptr,
        1,
        &mcpTaskHandle,
        1  // Run on core 1
    );
}

void loop() {
    // Main loop can be used for other tasks
    // Network and MCP handling is done in their respective tasks
    delay(1000);
}