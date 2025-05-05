#include "ble_module.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "wifi_module.h"
#include "relay_module.h"
#include "adc_module.h"
#include <ArduinoOTA.h>
#include <Preferences.h>
#include "ble_callbacks.h"

BLECharacteristic* pDataCharacteristic = nullptr;
BLECharacteristic* pRelayCharacteristic = nullptr;
BLECharacteristic* pWifiCharacteristic = nullptr;
BLEServer* pServer = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastReconnectionAttempt = 0;
#define RECONNECTION_DELAY 2000 // Time between reconnection attempts in ms

static const char* TAG = "ESP32_ADS1115";

// Bluetooth Service & Characteristic UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define RELAY_CONTROL_UUID  "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
#define WIFI_CONTROL_UUID   "c1d2e3f4-a5b6-7890-abcd-ef1234567890"

// Notification rate limiting
#define MIN_NOTIFICATION_INTERVAL_MS 100
unsigned long lastNotificationTime = 0;

void setupBLE() {
    BLEDevice::init("ESP32_ADS1115");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pDataCharacteristic = pService->createCharacteristic(
        DATA_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pRelayCharacteristic = pService->createCharacteristic(
        RELAY_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRelayCharacteristic->setCallbacks(new RelayControlCallback());
    pWifiCharacteristic = pService->createCharacteristic(
        WIFI_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWifiCharacteristic->setCallbacks(new WifiControlCallback());
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    
    // Adjust connection parameters for better stability
    pAdvertising->setMinInterval(0x100); // Increase min interval (160ms)
    pAdvertising->setMaxInterval(0x200); // Increase max interval (320ms)
    
    // Set a more conservative MTU size for better compatibility
    BLEDevice::setMTU(256);
    
    BLEDevice::startAdvertising();
}

void notifyData(const char* data) {
    if (pDataCharacteristic && deviceConnected) {
        // Apply rate limiting for notifications
        unsigned long currentTime = millis();
        if (currentTime - lastNotificationTime >= MIN_NOTIFICATION_INTERVAL_MS) {
            pDataCharacteristic->setValue(data);
            pDataCharacteristic->notify();
            lastNotificationTime = currentTime;
        }
    }
}

// Connection management function - call this in your main loop
void handleBLEConnections() {
    // Handle connection state changes
    if (!deviceConnected && oldDeviceConnected) {
        // Device disconnected
        delay(500); // Give the Bluetooth stack time to get ready
        BLEDevice::startAdvertising(); // Restart advertising
        oldDeviceConnected = deviceConnected;
        ESP_LOGI(TAG, "Device disconnected, restarting advertising");
    }
    
    // If connected, update the state
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        ESP_LOGI(TAG, "Device connected");
    }
}