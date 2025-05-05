#ifndef BLE_MODULE_H
#define BLE_MODULE_H
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define RELAY_CONTROL_UUID  "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
#define WIFI_CONTROL_UUID   "c1d2e3f4-a5b6-7890-abcd-ef1234567890"

// Forward declarations for BLE callback classes
class MyServerCallbacks;
class RelayControlCallback;
class WifiControlCallback;

void setupBLE();
void notifyData(const char* data);
void handleBLEConnections(); // Added function declaration
extern BLECharacteristic* pDataCharacteristic;
extern BLECharacteristic* pRelayCharacteristic;
extern BLECharacteristic* pWifiCharacteristic;
extern BLEServer* pServer;
extern bool deviceConnected;

#endif // BLE_MODULE_H
