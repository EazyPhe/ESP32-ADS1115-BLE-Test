#include "mcp_server.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include "adc_module.h"
#include "relay_module.h"
#include "wifi_module.h"
#include "config.h"
#include "sampling_config.h"

// Extern declarations for global buffers
extern float shuntBuffer[];
extern float ads2Buffer[];
extern bool relayStates[4];
extern SemaphoreHandle_t mcpServerMutex;

// WebSocket server for MCP communication
WebSocketsServer webSocket(9000);
bool webSocketStarted = false;

// Maximum number of resources and tools
#define MAX_RESOURCES 10
#define MAX_TOOLS 8
#define MAX_SUBSCRIPTIONS 5

// MCP Protocol version
#define MCP_VERSION "0.1.0"
#define WEBSOCKET_RETRY_DELAY 5000  // 5 seconds between retries

// Flag for Copilot connection status
bool copilotConnected = false;

// Custom resource data structure
struct Resource {
    const char* uri;      // Changed from String to const char*
    const char* type;     // Changed from String to const char*
    String (*getValue)();
    
    Resource() : uri(nullptr), type(nullptr), getValue(nullptr) {}
    
    Resource(const char* u, const char* t, String (*fn)()) 
        : uri(u), type(t), getValue(fn) {}
};

// Custom tool data structure
struct Tool {
    const char* uri;  // Change to const char* to match Resource structure
    void (*execute)(const JsonObject&, JsonObject&);
    
    Tool() : uri(nullptr), execute(nullptr) {}
    
    Tool(const char* u, void (*fn)(const JsonObject&, JsonObject&)) 
        : uri(u), execute(fn) {}
};

// Subscription data structure
struct Subscription {
    uint8_t clientId;
    String uri;  // Change back to String for easier management
    unsigned long lastUpdate;
    String lastValue;
    bool active;
};

// Collections for resources and tools
Resource resources[MAX_RESOURCES];
Tool tools[MAX_TOOLS];
Subscription subscriptions[MAX_SUBSCRIPTIONS];
int resourceCount = 0;
int toolCount = 0;
int subscriptionCount = 0;

// Forward declarations
void handleMcpRequest(uint8_t clientId, JsonObject& request);
void checkSubscriptions();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// Helper function to find average of a buffer
float getBufferAverage(float* buffer, int size) {
    float sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}

// Add helper function at the top with other helper functions
bool strEqual(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    return strcmp(a, b) == 0;
}

// Resource getter functions
String getShuntDiffValue() {
    float avg = getBufferAverage(shuntBuffer, 10);
    return String(avg);
}

String getAds2A0Value() {
    if (!ads2_available) return "unavailable";
    float avg = getBufferAverage(ads2Buffer, 10);
    return String(avg);
}

String getRelay0Value() {
    return relayStates[0] ? "on" : "off";
}

String getRelay1Value() {
    return relayStates[1] ? "on" : "off";
}

String getRelay2Value() {
    return relayStates[2] ? "on" : "off";
}

String getRelay3Value() {
    return relayStates[3] ? "on" : "off";
}

String getWifiStatusValue() {
    switch (WiFi.status()) {
        case WL_CONNECTED: return "connected";
        case WL_DISCONNECTED: return "disconnected";
        case WL_CONNECT_FAILED: return "connection_failed";
        case WL_IDLE_STATUS: return "idle";
        default: return "unknown";
    }
}

String getSamplingIntervalValue() {
    return String(getSamplingInterval());
}

// Tool execution functions
void setRelayTool(const JsonObject& params, JsonObject& result) {
    int index = -1;
    bool state = false;
    
    if (params.containsKey("index") && params.containsKey("state")) {
        index = params["index"].as<int>();
        state = params["state"].as<bool>();
        
        if (index >= 0 && index < 4) {
            setRelay(index, state);
            result["success"] = true;
            result["message"] = "Relay " + String(index) + " set to " + (state ? "ON" : "OFF");
        } else {
            result["success"] = false;
            result["message"] = "Invalid relay index";
        }
    } else {
        result["success"] = false;
        result["message"] = "Missing parameters";
    }
}

void scanWifiTool(const JsonObject& params, JsonObject& result) {
    scanWifiNetworks();
    result["success"] = true;
    result["message"] = "WiFi scan initiated";
}

void connectWifiTool(const JsonObject& params, JsonObject& result) {
    if (params.containsKey("ssid") && params.containsKey("password")) {
        String ssid = params["ssid"].as<String>();
        String password = params["password"].as<String>();
        
        connectToWifi(ssid, password);
        result["success"] = true;
        result["message"] = "Connecting to WiFi: " + ssid;
    } else {
        result["success"] = false;
        result["message"] = "Missing SSID or password";
    }
}

void calibrateAdcTool(const JsonObject& params, JsonObject& result) {
    calibrateADC();
    result["success"] = true;
    result["message"] = "ADC calibration completed";
}

void setSamplingIntervalTool(const JsonObject& params, JsonObject& result) {
    if (params.containsKey("interval")) {
        uint16_t interval = params["interval"].as<uint16_t>();
        if (interval >= 10 && interval <= 10000) {
            setSamplingInterval(interval);
            result["success"] = true;
            result["message"] = "Sampling interval set to " + String(interval) + "ms";
        } else {
            result["success"] = false;
            result["message"] = "Interval must be between 10ms and 10000ms";
        }
    } else {
        result["success"] = false;
        result["message"] = "Missing interval parameter";
    }
}

void printToSerial(const JsonObject& params, JsonObject& result) {
    if (params.containsKey("message")) {
        String message = params["message"].as<String>();
        Serial.println("[MCP:STDIO] " + message);
        result["success"] = true;
        result["message"] = "Message printed to serial";
    } else {
        result["success"] = false;
        result["message"] = "Missing message parameter";
    }
}

// Add a specific tool for Copilot to register itself
void registerCopilotTool(const JsonObject& params, JsonObject& result) {
    copilotConnected = true;
    result["success"] = true;
    result["message"] = "Copilot registered successfully";
    
    // Log available resources for Copilot
    String resourceList = "Available resources for Copilot: ";
    for (int i = 0; i < resourceCount; i++) {
        if (i > 0) resourceList += ", ";
        resourceList += resources[i].uri;
    }
    Serial.println(resourceList);
}

// Subscription management
int findSubscription(uint8_t clientId, const char* uri) {
    for (int i = 0; i < subscriptionCount; i++) {
        if (subscriptions[i].clientId == clientId && 
            subscriptions[i].active && 
            strEqual(subscriptions[i].uri.c_str(), uri)) {
            return i;
        }
    }
    return -1;
}

void addSubscription(uint8_t clientId, const char* uri) {  // Changed parameter type
    // Check if subscription already exists
    int index = findSubscription(clientId, uri);
    if (index >= 0) {
        return; // Already subscribed
    }
    
    // Find inactive subscription slot or use a new one
    int slotIndex = -1;
    for (int i = 0; i < subscriptionCount; i++) {
        if (!subscriptions[i].active) {
            slotIndex = i;
            break;
        }
    }
    
    if (slotIndex < 0 && subscriptionCount < MAX_SUBSCRIPTIONS) {
        slotIndex = subscriptionCount++;
    }
    
    if (slotIndex >= 0) {
        subscriptions[slotIndex].clientId = clientId;
        subscriptions[slotIndex].uri = uri;  // Now storing const char* directly
        subscriptions[slotIndex].lastUpdate = millis();
        subscriptions[slotIndex].lastValue = "";
        subscriptions[slotIndex].active = true;
    }
}

void removeSubscription(uint8_t clientId, const char* uri) {  // Changed parameter type
    int index = findSubscription(clientId, uri);
    if (index >= 0) {
        subscriptions[index].active = false;
    }
}

void removeAllSubscriptions(uint8_t clientId) {
    for (int i = 0; i < subscriptionCount; i++) {
        if (subscriptions[i].clientId == clientId) {
            subscriptions[i].active = false;
        }
    }
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            if (copilotConnected && num == 0) { // Assuming Copilot typically connects as the first client
                copilotConnected = false;
                Serial.println("Copilot disconnected");
            }
            removeAllSubscriptions(num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                
                // Send a welcome message
                String message = "{\"event\":\"connected\",\"message\":\"Welcome to ESP32 MCP Server\"}";
                webSocket.sendTXT(num, message);
            }
            break;
            
        case WStype_TEXT:
            {
                // Handle incoming MCP request
                String text = String((char*)payload);
                Serial.printf("[%u] Received text: %s\n", num, text.c_str());
                
                // Check if this might be a Copilot connection
                if (text.indexOf("\"method\":\"initialize\"") >= 0 && text.indexOf("\"client\":\"copilot\"") >= 0) {
                    copilotConnected = true;
                    Serial.println("VS Code Copilot connected!");
                }
                
                StaticJsonDocument<1024> doc;
                DeserializationError error = deserializeJson(doc, text);
                
                if (error) {
                    String errorMsg = "{\"error\":\"Invalid JSON\"}";
                    webSocket.sendTXT(num, errorMsg);
                    return;
                }
                
                // Process the request
                JsonObject requestObj = doc.as<JsonObject>();
                handleMcpRequest(num, requestObj);
            }
            break;
    }
}

// Handle MCP request
void handleMcpRequest(uint8_t clientId, JsonObject& request) {
    if (!request.containsKey("method") || !request.containsKey("id")) {
        String errorMsg = "{\"error\":\"Invalid request format\"}";
        webSocket.sendTXT(clientId, errorMsg);
        return;
    }
    
    String method = request["method"].as<String>();
    int id = request["id"].as<int>();
    
    if (method == "initialize") {
        // Handle initialize request
        StaticJsonDocument<512> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        response["id"] = id;
        response["result"]["serverName"] = "esp32-mcp-server";
        response["result"]["serverVersion"] = MCP_VERSION;
        response["result"]["capabilities"]["supportsSubscriptions"] = true;
        response["result"]["capabilities"]["supportsResources"] = true;
        response["result"]["capabilities"]["supportsTelemetry"] = true;
        
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(clientId, responseStr);
    }
    else if (method == "resources.list") {
        // Handle resources.list request
        StaticJsonDocument<1024> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        response["id"] = id;
        
        JsonArray resourcesArray = response["result"]["resources"].to<JsonArray>();
        
        // Add all registered resources to the response
        for (int i = 0; i < resourceCount; i++) {
            JsonObject resObj = resourcesArray.createNestedObject();
            resObj["uri"] = resources[i].uri;
            resObj["type"] = resources[i].type;
        }
        
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(clientId, responseStr);
    }
    else if (method == "resource.read") {
        // Handle resource.read request
        if (!request.containsKey("params") || !request["params"].containsKey("uri")) {
            String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":400,\"message\":\"Missing URI parameter\"}}";
            webSocket.sendTXT(clientId, errorMsg);
            return;
        }
        
        String uri = String(request["params"]["uri"].as<const char*>());
        
        // Find the requested resource
        for (int i = 0; i < resourceCount; i++) {
            if (strEqual(resources[i].uri, uri.c_str())) {
                String value = resources[i].getValue();
                
                StaticJsonDocument<512> responseDoc;
                JsonObject response = responseDoc.to<JsonObject>();
                response["id"] = id;
                JsonArray contents = response["result"]["contents"].to<JsonArray>();
                JsonObject content = contents.createNestedObject();
                content["data"] = value;
                
                String responseStr;
                serializeJson(response, responseStr);
                webSocket.sendTXT(clientId, responseStr);
                return;
            }
        }
        
        // Resource not found
        String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":404,\"message\":\"Resource not found\"}}";
        webSocket.sendTXT(clientId, errorMsg);
    }
    else if (method == "subscribe") {
        // Handle subscribe request
        if (!request.containsKey("params") || !request["params"].containsKey("uri")) {
            String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":400,\"message\":\"Missing URI parameter\"}}";
            webSocket.sendTXT(clientId, errorMsg);
            return;
        }
        
        const char* uri = request["params"]["uri"].as<const char*>();
        
        // Check if the resource exists
        bool resourceExists = false;
        for (int i = 0; i < resourceCount; i++) {
            if (strEqual(resources[i].uri, uri)) {
                resourceExists = true;
                break;
            }
        }
        
        if (!resourceExists) {
            String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":404,\"message\":\"Resource not found\"}}";
            webSocket.sendTXT(clientId, errorMsg);
            return;
        }
        
        // Add subscription
        addSubscription(clientId, uri);
        
        // Send success response
        StaticJsonDocument<256> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        response["id"] = id;
        response["result"]["success"] = true;
        
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(clientId, responseStr);
    }
    else if (method == "unsubscribe") {
        // Handle unsubscribe request
        if (!request.containsKey("params") || !request["params"].containsKey("uri")) {
            String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":400,\"message\":\"Missing URI parameter\"}}";
            webSocket.sendTXT(clientId, errorMsg);
            return;
        }
        
        const char* uri = request["params"]["uri"].as<const char*>();
        
        // Remove subscription
        removeSubscription(clientId, uri);
        
        // Send success response
        StaticJsonDocument<256> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        response["id"] = id;
        response["result"]["success"] = true;
        
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT(clientId, responseStr);
    }
    else if (method == "tool.execute") {
        // Handle tool.execute request
        if (!request.containsKey("params") || !request["params"].containsKey("uri")) {
            String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":400,\"message\":\"Missing URI parameter\"}}";
            webSocket.sendTXT(clientId, errorMsg);
            return;
        }
        
        String uri = request["params"]["uri"].as<String>();
        JsonObject toolParams = request["params"].containsKey("params") ? 
                                request["params"]["params"].as<JsonObject>() : 
                                JsonObject();
        
        // Find the requested tool
        for (int i = 0; i < toolCount; i++) {
            if (strEqual(tools[i].uri, uri.c_str())) {
                StaticJsonDocument<512> resultDoc;
                JsonObject result = resultDoc.to<JsonObject>();
                
                // Execute the tool
                tools[i].execute(toolParams, result);
                
                // Create response
                StaticJsonDocument<512> responseDoc;
                JsonObject response = responseDoc.to<JsonObject>();
                response["id"] = id;
                response["result"] = result;
                
                String responseStr;
                serializeJson(response, responseStr);
                webSocket.sendTXT(clientId, responseStr);
                return;
            }
        }
        
        // Tool not found
        String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":404,\"message\":\"Tool not found\"}}";
        webSocket.sendTXT(clientId, errorMsg);
    }
    else {
        // Unknown method
        String errorMsg = "{\"id\":" + String(id) + ",\"error\":{\"code\":400,\"message\":\"Unknown method\"}}";
        webSocket.sendTXT(clientId, errorMsg);
    }
}

// Check for subscription updates
void checkSubscriptions() {
    static unsigned long lastCheck = 0;
    const unsigned long checkInterval = 200; // Check subscriptions every 200ms
    
    unsigned long currentTime = millis();
    if (currentTime - lastCheck < checkInterval) {
        return;
    }
    
    lastCheck = currentTime;
    
    for (int i = 0; i < subscriptionCount; i++) {
        if (!subscriptions[i].active) continue;
        
        // Find the resource
        for (int j = 0; j < resourceCount; j++) {
            if (strEqual(resources[j].uri, subscriptions[i].uri.c_str())) {
                String currentValue = resources[j].getValue();
                
                // Check if the value has changed
                if (currentValue != subscriptions[i].lastValue) {
                    subscriptions[i].lastValue = currentValue;
                    
                    // Send notification
                    StaticJsonDocument<512> notificationDoc;
                    notificationDoc["jsonrpc"] = "2.0";
                    notificationDoc["method"] = "resource.change";
                    notificationDoc["params"]["uri"] = subscriptions[i].uri;
                    notificationDoc["params"]["contents"][0]["data"] = currentValue;
                    
                    String notificationStr;
                    serializeJson(notificationDoc, notificationStr);
                    webSocket.sendTXT(subscriptions[i].clientId, notificationStr);
                }
                
                break;
            }
        }
    }
}

// Register all resources and tools
void registerResourcesAndTools() {
    // Register resources - using string literals directly
    resources[resourceCount++] = Resource("adc.shunt_diff", "number", getShuntDiffValue);
    resources[resourceCount++] = Resource("adc.ads2_a0", "number", getAds2A0Value);
    resources[resourceCount++] = Resource("relay.0", "boolean", getRelay0Value);
    resources[resourceCount++] = Resource("relay.1", "boolean", getRelay1Value);
    resources[resourceCount++] = Resource("relay.2", "boolean", getRelay2Value);
    resources[resourceCount++] = Resource("relay.3", "boolean", getRelay3Value);
    resources[resourceCount++] = Resource("wifi.status", "string", getWifiStatusValue);
    resources[resourceCount++] = Resource("config.sampling_interval", "number", getSamplingIntervalValue);
    
    // Register tools - using string literals directly
    tools[toolCount++] = Tool("relay.set", setRelayTool);
    tools[toolCount++] = Tool("wifi.scan", scanWifiTool);
    tools[toolCount++] = Tool("wifi.connect", connectWifiTool);
    tools[toolCount++] = Tool("adc.calibrate", calibrateAdcTool);
    tools[toolCount++] = Tool("config.set_sampling_interval", setSamplingIntervalTool);
    tools[toolCount++] = Tool("copilot.register", registerCopilotTool);
    tools[toolCount++] = Tool("stdio.print", printToSerial);
    
    Serial.println("Registered " + String(resourceCount) + " resources and " + String(toolCount) + " tools");
}

void setupMcpServer() {
    Serial.println("[MCP] Setting up MCP server...");
    
    // Initialize static setup flag to track if setup has been completed successfully
    static bool setupCompleted = false;
    
    // Check if setup has already been completed
    if (setupCompleted) {
        Serial.println("[MCP] MCP server already initialized, skipping setup");
        return;
    }
    
    // Check WiFi status
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MCP] WiFi not connected, MCP server not started");
        Serial.println("[MCP] WiFi status code: " + String(WiFi.status()));
        return;
    }

    Serial.println("[MCP] WiFi connected, IP: " + WiFi.localIP().toString());
    
    // Give a longer delay to ensure the mutex is fully created before attempting to acquire it
    delay(100);
    
    // Try to take the mutex with a longer timeout
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(500); // Wait up to 500ms
    
    if (mcpServerMutex == NULL) {
        Serial.println("[MCP] ERROR: mcpServerMutex is NULL!");
        return;
    }
    
    Serial.println("[MCP] Attempting to acquire mutex...");
    
    // Only initialize WebSocket server if not already started
    // Use mutex for thread-safe state check and modification
    if (xSemaphoreTake(mcpServerMutex, xMaxBlockTime) == pdTRUE) {
        Serial.println("[MCP] Mutex acquired successfully");
        
        if (!webSocketStarted) {
            webSocket.close();  // Ensure any existing connections are closed
            delay(100);  // Give time for connections to close
            
            webSocket.begin();
            webSocket.onEvent(webSocketEvent);
            webSocketStarted = true;
            setupCompleted = true;  // Mark setup as completed
            Serial.println("[MCP] WebSocket server initialized");
            
            // Register resources and tools
            registerResourcesAndTools();
            Serial.println("[MCP] Resources and tools registered");
            Serial.println("[MCP] MCP server started on port 9000");
        } else {
            Serial.println("[MCP] WebSocket server already running");
        }
        xSemaphoreGive(mcpServerMutex);
        Serial.println("[MCP] Mutex released");
    } else {
        Serial.println("[MCP] Failed to acquire mutex for MCP server setup");
    }
}

void handleMcpLoop() {
    static unsigned long lastRetry = 0;
    static unsigned long lastCheck = 0;
    
    // Check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        if (xSemaphoreTake(mcpServerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // If WebSocket was running, stop it
            if (webSocketStarted) {
                webSocket.close();
                webSocketStarted = false;
                Serial.println("[MCP] WebSocket server stopped due to WiFi disconnect");
            }
            xSemaphoreGive(mcpServerMutex);
        }
        Serial.println("[MCP] WiFi disconnected, status: " + String(WiFi.status()));
        return;
    }
    
    // If WebSocket server isn't running and enough time has passed since last retry
    if (xSemaphoreTake(mcpServerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!webSocketStarted && (millis() - lastRetry > WEBSOCKET_RETRY_DELAY)) {
            Serial.println("[MCP] Attempting to restart WebSocket server...");
            setupMcpServer();
            lastRetry = millis();
        }
        
        // Only run WebSocket loop if server is started
        if (webSocketStarted) {
            webSocket.loop();
            checkSubscriptions();
        }
        xSemaphoreGive(mcpServerMutex);
    }
    
    // Print periodic connection status (every 5 seconds)
    if (millis() - lastCheck > 5000) {
        lastCheck = millis();
        if (xSemaphoreTake(mcpServerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (webSocketStarted) {
                Serial.println("[MCP] Server running, IP: " + WiFi.localIP().toString());
            }
            xSemaphoreGive(mcpServerMutex);
        }
    }
}
