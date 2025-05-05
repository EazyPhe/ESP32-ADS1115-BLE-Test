#include <Wire.h> // Provides I2C communication support for interfacing with ADS1115 ADC chips
#include <Adafruit_ADS1X15.h> // Library for interacting with ADS1115 ADC chips for analog-to-digital conversion
#include <BLEDevice.h> // Core library for setting up BLE functionality on the ESP32
#include <BLEUtils.h> // Utility functions for BLE operations, such as UUID handling
#include <BLEServer.h> // Enables the ESP32 to act as a BLE server
#include <Preferences.h> // Provides non-volatile storage for saving and retrieving settings (e.g., WiFi credentials, relay states)
#include <esp_log.h> // ESP-IDF logging library for structured logging and debugging
#include <ArduinoOTA.h> // Enables Over-The-Air (OTA) firmware updates for the ESP32
#include <WiFi.h> // Handles WiFi connectivity, including scanning, connecting, and managing access points
#include <esp_sleep.h> // Provides functions for managing sleep modes to save power
#include <esp_task_wdt.h> // Manages the Task Watchdog Timer to monitor and reset unresponsive tasks
#include <soc/rtc_wdt.h> // Allows control over the RTC Watchdog Timer, which can cause resets during long tasks
#include <esp_system.h> // Provides system-level functions, such as restarting the ESP32
#include <rom/rtc.h> // Used for retrieving crash diagnostics and reset reasons
#include <ArduinoJson.h> // Library for creating and parsing JSON, used for BLE data serialization
#include "sampling_config.h" // Project-specific configuration for ADC sampling intervals and settings
#include "config.h" // Contains system-wide constants and configuration values
#include "ble_module.h" // Handles BLE initialization, services, and characteristics
#include "wifi_module.h" // Manages WiFi connectivity, including AP mode and client mode
#include "adc_module.h" // Provides functions for interacting with ADS1115 ADC chips and processing analog data
#include "relay_module.h" // Controls GPIO pins connected to relays and manages their states
#include "ble_callbacks.h" // Implements BLE command processing and characteristic callbacks
#include "mcp_server.h" // Implements the MCP server for remote management and communication

// Function prototypes for local functions only
void monitorTask(void *pvParameters);

static const char* TAG = "ESP32_ADS1115";

// Local buffers and mutex for averaging and synchronization
const int avgWindow = 10;
float shuntBuffer[avgWindow] = {0};
float ads2Buffer[avgWindow] = {0};
int bufferIndex = 0;

Preferences prefs;
SemaphoreHandle_t bufferMutex;

// LED pin for relay feedback (used by blinkRelayFeedback in relay_module.cpp)
const int relayFeedbackLedPin = 33;

// Define the global variable currentLogLevel to resolve the linker error
LogLevel currentLogLevel = INFO_LEVEL;

// Function to track MCP server state
bool mcpServerStarted = false;
SemaphoreHandle_t mcpServerMutex = NULL;

// Failover/reconnect monitor task
void monitorTask(void *pvParameters) {
    while (1) {
        // WiFi reconnect logic
        if (WiFi.status() != WL_CONNECTED) {
            String ssid = prefs.getString("ssid", "");
            String password = prefs.getString("password", "");
            
            // Take mutex before modifying MCP server state
            if (xSemaphoreTake(mcpServerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (mcpServerStarted) {
                    mcpServerStarted = false; // Reset MCP server state
                }
                xSemaphoreGive(mcpServerMutex);
            }
            
            if (ssid != "" && password != "") {
                LOG_INFO("Reconnecting to WiFi: %s", ssid.c_str());
                WiFi.disconnect();
                WiFi.begin(ssid.c_str(), password.c_str());
                // Wait a bit for connection
                vTaskDelay(pdMS_TO_TICKS(5000));
            } else {
                LOG_WARNING("WiFi credentials missing, skipping reconnect");
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        } else {
            // WiFi is connected but MCP server isn't running
            if (xSemaphoreTake(mcpServerMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!mcpServerStarted) {
                    LOG_INFO("WiFi connected, starting MCP server");
                    setupMcpServer();
                    mcpServerStarted = true;
                }
                xSemaphoreGive(mcpServerMutex);
            }
        }
        
        // ADS1115 #1 recovery logic
        if (!ads1_available) {
            LOG_INFO("Attempting to recover ADS1115 #1...");
            
            // Try to re-initialize with a delay
            vTaskDelay(pdMS_TO_TICKS(100));
            
            if (ads1.begin(0x48)) {
                ads1.setGain(GAIN_EIGHT);
                ads1.setDataRate(RATE_ADS1115_860SPS);
                ads1_available = true;
                LOG_INFO("ADS1115 #1 reinitialized successfully.");
            } else {
                LOG_ERROR("ADS1115 #1 reinitialization failed!");
            }
        }
        
        // ADS1115 #2 recovery logic
        if (!ads2_available) {
            LOG_INFO("Attempting to recover ADS1115 #2...");
            
            // Try to re-initialize with a delay
            vTaskDelay(pdMS_TO_TICKS(100));
            
            if (ads2.begin(0x49)) {
                ads2.setGain(GAIN_ONE);
                ads2.setDataRate(RATE_ADS1115_860SPS);
                ads2_available = true;
                LOG_INFO("ADS1115 #2 reinitialized successfully.");
            } else {
                LOG_ERROR("ADS1115 #2 reinitialization failed!");
            }
        }
        
        // Give other tasks time to run and avoid too frequent checks
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds
    }
}

// FreeRTOS task for ADC data collection with mutex protection
void dataTask(void *pvParameters) {
    // Tell the watchdog that this is a monitored task
    esp_task_wdt_add(NULL);
    
    while (1) {
        // Reset watchdog timer for this task
        esp_task_wdt_reset();
        
        // Use our robust reading functions that handle errors and timeouts
        int16_t rawShuntDiff = readShuntDifferential();
        float calibratedShuntDiff = rawShuntDiff - shuntOffsetFloat;
        
        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            shuntBuffer[bufferIndex] = calibratedShuntDiff;
            
            // Use our robust reading function for ADS2
            int16_t rawAds2A0 = readADS2Channel0();
            float calibratedAds2A0 = rawAds2A0 - ads2OffsetFloat;
            ads2Buffer[bufferIndex] = calibratedAds2A0;
            
            bufferIndex = (bufferIndex + 1) % avgWindow;
            xSemaphoreGive(bufferMutex);
        } else {
            LOG_ERROR("Failed to acquire mutex in dataTask");
        }
        
        // Make sure we yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(getSamplingInterval()));
    }
    
    // Should never reach here, but just in case
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

// FreeRTOS task for BLE communication with mutex protection
void bleTask(void *pvParameters) {
    while (1) {
        // Handle BLE connections for reconnection
        handleBLEConnections();

        if (deviceConnected) {
            float shuntDiffAvg = 0;
            float ads2A0Avg = 0;
            if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                for (int i = 0; i < avgWindow; i++) {
                    shuntDiffAvg += shuntBuffer[i];
                    if (ads2_available) {
                        ads2A0Avg += ads2Buffer[i];
                    }
                }
                shuntDiffAvg /= avgWindow;
                if (ads2_available) {
                    ads2A0Avg /= avgWindow;
                }
                xSemaphoreGive(bufferMutex);
            } else {
                LOG_ERROR("Failed to acquire mutex in bleTask");
            }

            // Apply deadband
            if (abs(shuntDiffAvg) < 1.0) shuntDiffAvg = 0;
            if (abs(ads2A0Avg) < 1.0) ads2A0Avg = 0;

            // Build standardized JSON data with protocol version using ArduinoJson
            DynamicJsonDocument doc(512);
            doc["protocol_version"] = PROTOCOL_VERSION;
            doc["timestamp"] = millis();
            
            // Organize measurements in a nested structure
            JsonObject measurements = doc.createNestedObject("measurements");
            measurements["shunt_diff"] = shuntDiffAvg;
            measurements["ads2_a0"] = ads2A0Avg;
            
            // Organize relay states in a nested structure
            JsonObject relays = doc.createNestedObject("relays");
            for (int i = 0; i < 4; i++) {
                relays["relay" + String(i + 1)] = relayStates[i] ? 1 : 0;
            }

            // Serialize JSON
            String jsonData;
            serializeJson(doc, jsonData);

            if (jsonData.length() > 0 && jsonData.length() < 512) {
                notifyData(jsonData.c_str());
                LOG_INFO("BLE Data Sent: %s", jsonData.c_str());
            } else {
                LOG_ERROR("Error: JSON data too large or invalid!");
            }
        }
        ArduinoOTA.handle(); // Handle OTA updates
        vTaskDelay(pdMS_TO_TICKS(100)); // Notify every 100ms
    }
}

void setup() {
    Serial.begin(115200);

    // Print reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.println("\n\n=== ESP32 Reset Information ===");
    Serial.print("Reset reason: ");
    switch (reset_reason) {
        case ESP_RST_UNKNOWN: Serial.println("Unknown reset"); break;
        case ESP_RST_POWERON: Serial.println("Power-on reset"); break;
        case ESP_RST_EXT: Serial.println("External reset via pin"); break;
        case ESP_RST_SW: Serial.println("Software reset"); break;
        case ESP_RST_PANIC: Serial.println("Software panic"); break;
        case ESP_RST_INT_WDT: Serial.println("Internal watchdog timeout"); break;
        case ESP_RST_TASK_WDT: Serial.println("Task watchdog timeout"); break;
        case ESP_RST_WDT: Serial.println("Other watchdog timeout"); break;
        case ESP_RST_DEEPSLEEP: Serial.println("Exit from deep sleep"); break;
        case ESP_RST_BROWNOUT: Serial.println("Brownout reset"); break;
        case ESP_RST_SDIO: Serial.println("SDIO reset"); break;
        default: Serial.println("Unknown reason"); break;
    }

    // Print RTC watchdog info if reset was due to a watchdog
    if (reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_INT_WDT || reset_reason == ESP_RST_WDT) {
        Serial.print("RTC watchdog cause: ");
        RESET_REASON rtc_reset = rtc_get_reset_reason(0);
        switch (rtc_reset) {
            case POWERON_RESET: Serial.println("Power-on reset (RTC)"); break;
            case SW_RESET: Serial.println("Software reset (RTC)"); break;
            case OWDT_RESET: Serial.println("Legacy watch dog reset (RTC)"); break;
            case DEEPSLEEP_RESET: Serial.println("Deep Sleep reset (RTC)"); break;
            case SDIO_RESET: Serial.println("SPI reset (RTC)"); break;
            case TG0WDT_SYS_RESET: Serial.println("Timer Group 0 Watch dog reset (RTC)"); break;
            case TG1WDT_SYS_RESET: Serial.println("Timer Group 1 Watch dog reset (RTC)"); break;
            case RTCWDT_SYS_RESET: Serial.println("RTC Watch dog reset (RTC)"); break;
            case INTRUSION_RESET: Serial.println("Intrusion reset (RTC)"); break;
            case TGWDT_CPU_RESET: Serial.println("Timer Group reset CPU (RTC)"); break;
            case SW_CPU_RESET: Serial.println("Software reset CPU (RTC)"); break;
            case RTCWDT_CPU_RESET: Serial.println("RTC Watch dog reset CPU (RTC)"); break;
            case EXT_CPU_RESET: Serial.println("External CPU reset (RTC)"); break;
            case RTCWDT_BROWN_OUT_RESET: Serial.println("Brownout reset (RTC)"); break;
            case RTCWDT_RTC_RESET: Serial.println("RTC watch dog reset digital core and rtc (RTC)"); break;
            default: Serial.println("Unknown (RTC)"); break;
        }
    }
    Serial.println("=== End Reset Information ===\n");

    // Add debug option to force a crash for testing
    if (prefs.getBool("force_crash", false)) {
        Serial.println("Forcing a crash for backtrace testing!");
        prefs.putBool("force_crash", false); // Reset the flag
        delay(1000); // Give time for the message to be sent
        // Force a crash by writing to an invalid memory address
        int *p = NULL;
        *p = 0;
    }

    // Disable RTC Watchdog Timer to prevent resets
    rtc_wdt_protect_off(); // Disable write protection
    rtc_wdt_disable();     // Disable RTC Watchdog
    rtc_wdt_protect_on();  // Re-enable write protection

    // Configure Task Watchdog Timer
    esp_task_wdt_init(30, false); // 30-second timeout, no panic

    Wire.begin(21, 22); // SDA on GPIO 21, SCL on GPIO 22
    Wire.setClock(100000); // 100 kHz I2C speed
    Wire.setTimeout(100); // 100ms timeout for I2C

    // Initialize Preferences
    prefs.begin("app_state", false);

    // Restore relay states from NVS
    for (int i = 0; i < 4; i++) {
        relayStates[i] = prefs.getBool(("relay" + String(i)).c_str(), false);
        LOG_INFO("Restored Relay %d state: %d", i + 1, relayStates[i]);
    }

    // Initialize relay pins
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], relayStates[i] ? HIGH : LOW);
    }

    // Initialize ADS1 for current measurement
    if (!initializeADS(ads1, 0x48, "ADS1115 #1")) {
        LOG_ERROR("Critical failure: ADS1115 #1 not initialized! Restarting...");
        if (pRelayCharacteristic) {
            pRelayCharacteristic->setValue("ERROR:ADC:ADS1115_1_INIT_FAIL");
            pRelayCharacteristic->notify();
        }
        ESP.restart();
    }
    ads1.setGain(GAIN_EIGHT); // ±0.512V range
    ads1.setDataRate(RATE_ADS1115_860SPS); // 860 SPS
    ads1_available = true; // Mark ADS1 as available after successful initialization

    // Initialize ADS2 for voltage measurement
    if (initializeADS(ads2, 0x49, "ADS1115 #2")) {
        ads2.setGain(GAIN_ONE); // ±4.096V range
        ads2.setDataRate(RATE_ADS1115_860SPS); // 860 SPS
        ads2_available = true;
    } else {
        LOG_ERROR("ADS1115 #2 unavailable, proceeding without it.");
        if (pRelayCharacteristic) {
            pRelayCharacteristic->setValue("ERROR:ADC:ADS1115_2_INIT_FAIL");
            pRelayCharacteristic->notify();
        }
        ads2_available = false;
    }

    // Perform auto-calibration on every boot
    calibrateADC();

    // Initialize BLE with our improved setup function
    setupBLE();
    LOG_INFO("BLE Server is running...");

    // Restore WiFi credentials and attempt to connect
    String ssid = prefs.getString("ssid", "");
    String password = prefs.getString("password", "");
    if (ssid != "" && password != "") {
        LOG_INFO("Attempting WiFi connection with SSID: %s", ssid.c_str());
        connectToWifi(ssid, password);
    } else {
        LOG_WARNING("WiFi credentials not found in NVS, starting AP mode");
        // Start AP mode for configuration when no credentials exist
        if (startAPMode()) {
            LOG_INFO("AP Mode started successfully. Connect to WiFi network: %s with password: %s", AP_SSID, AP_PASSWORD);
            LOG_INFO("Then navigate to http://192.168.4.1 in your browser");
        } else {
            LOG_ERROR("Failed to start AP Mode");
        }
    }

    // Create mutex for buffer synchronization
    bufferMutex = xSemaphoreCreateMutex();
    if (bufferMutex == NULL) {
        LOG_ERROR("Failed to create mutex!");
        if (pRelayCharacteristic) {
            pRelayCharacteristic->setValue("ERROR:RTOS:MUTEX_CREATE_FAIL");
            pRelayCharacteristic->notify();
        }
        ESP.restart();
    }

    // Create mutex for MCP server state synchronization
    mcpServerMutex = xSemaphoreCreateMutex();
    if (mcpServerMutex == NULL) {
        LOG_ERROR("Failed to create MCP server mutex!");
        if (pRelayCharacteristic) {
            pRelayCharacteristic->setValue("ERROR:RTOS:MCP_MUTEX_CREATE_FAIL");
            pRelayCharacteristic->notify();
        }
        ESP.restart();
    }

    // Create FreeRTOS tasks
    xTaskCreatePinnedToCore(dataTask, "DataTask", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(bleTask, "BleTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(monitorTask, "MonitorTask", 4096, NULL, 1, NULL, 1);

    pinMode(relayFeedbackLedPin, OUTPUT); // Setup relay feedback LED
    digitalWrite(relayFeedbackLedPin, LOW);

    // Only start MCP server if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        // Set flag to indicate MCP server needs to be set up
        // The monitorTask will handle setting up the MCP server
        mcpServerStarted = false;
        Serial.println("WiFi connected, MCP server will be started by monitorTask");
    } else {
        LOG_WARNING("MCP server not started: WiFi not connected");
    }
}

void loop() {
    // Handle MCP server if available
    handleMcpLoop();
    
    // Handle WiFi configuration in AP mode
    if (isAPModeActive()) {
        handleWiFiConfig();
    }
    // Only enter light sleep if both BLE and MCP are inactive
    else if (!deviceConnected && !mcpServerStarted) {
        LOG_INFO("No active connections. Entering light sleep mode.");
        esp_sleep_enable_timer_wakeup(1000000); // Wake up after 1 second
        esp_light_sleep_start();
        LOG_INFO("Woke from light sleep.");
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Reduced delay for better responsiveness
}