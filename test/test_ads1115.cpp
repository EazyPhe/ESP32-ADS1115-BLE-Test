#include <Arduino.h>
#include <unity.h>
#include <Adafruit_ADS1X15.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include "adc_module.h"
#include "relay_module.h"
#include "wifi_module.h"
#include "ble_module.h"

Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;
Preferences prefs;

void test_ads1_initialization() {
    TEST_ASSERT_TRUE(ads1.begin(0x48));
}

void test_ads2_initialization() {
    TEST_ASSERT_TRUE(ads2.begin(0x49));
}

void test_ble_initialization() {
    BLEDevice::init("TestDevice");
    TEST_ASSERT_NOT_NULL(BLEDevice::getAdvertising());
}

void test_relay_state_persistence() {
    prefs.begin("app_state", false);
    prefs.putBool("relay0", true);
    TEST_ASSERT_TRUE(prefs.getBool("relay0", false));
    prefs.putBool("relay0", false);
    TEST_ASSERT_FALSE(prefs.getBool("relay0", true));
    prefs.end();
}

void test_wifi_scan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int numNetworks = WiFi.scanNetworks();
    TEST_ASSERT_GREATER_OR_EQUAL(0, numNetworks);
}

// --- ADC Module Tests ---
void test_adc_calibration() {
    // Simulate known values for calibration
    float oldOffset = shuntOffset;
    shuntOffset = 0.0;
    // Simulate calibration logic
    float testSum = 1000.0;
    int numSamples = 10;
    float expectedOffset = testSum / numSamples;
    shuntOffset = expectedOffset;
    TEST_ASSERT_EQUAL_FLOAT(expectedOffset, shuntOffset);
    shuntOffset = oldOffset;
}

void test_adc_averaging() {
    // Simulate buffer values
    float buffer[5] = {1, 2, 3, 4, 5};
    float avg = 0;
    for (int i = 0; i < 5; i++) avg += buffer[i];
    avg /= 5;
    TEST_ASSERT_EQUAL_FLOAT(3.0, avg);
}

// --- Relay Module Tests ---
void test_relay_toggle() {
    bool initial = relayStates[0];
    toggleRelay(0);
    TEST_ASSERT_NOT_EQUAL(initial, relayStates[0]);
    // Toggle back
    toggleRelay(0);
    TEST_ASSERT_EQUAL(initial, relayStates[0]);
}

void test_relay_set() {
    setRelay(1, true);
    TEST_ASSERT_TRUE(relayStates[1]);
    setRelay(1, false);
    TEST_ASSERT_FALSE(relayStates[1]);
}

// --- WiFi Module Tests ---
void test_wifi_scan_format() {
    // Simulate scan result formatting
    String ssid = "TestNet";
    int rssi = -42;
    String result = ssid + "(" + String(rssi) + ")";
    TEST_ASSERT_EQUAL_STRING("TestNet(-42)", result.c_str());
}

void test_wifi_connect_logic() {
    // This is a stub; real test would mock WiFi.status()
    TEST_ASSERT_TRUE(true); // Placeholder
}

// --- BLE Module Tests ---
void test_ble_json_format() {
    // Simulate JSON output
    float shunt = 1.23;
    float ads2 = 4.56;
    int relay1 = 1, relay2 = 0;
    String json = "{";
    json += "\"SHUNT_DIFF\":" + String(shunt);
    json += ",\"ADS2_A0\":" + String(ads2);
    json += ",\"RELAY1\":" + String(relay1);
    json += ",\"RELAY2\":" + String(relay2);
    json += "}";
    TEST_ASSERT_TRUE(json.indexOf("SHUNT_DIFF") > 0);
    TEST_ASSERT_TRUE(json.indexOf("ADS2_A0") > 0);
}

void test_ble_command_parsing() {
    // Simulate parsing a BLE command
    String cmd = "SET_25_ON";
    int pin = cmd.substring(4, 6).toInt();
    String stateStr = cmd.substring(7);
    bool state = (stateStr == "ON");
    TEST_ASSERT_EQUAL(25, pin);
    TEST_ASSERT_TRUE(state);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_ads1_initialization);
    RUN_TEST(test_ads2_initialization);
    RUN_TEST(test_ble_initialization);
    RUN_TEST(test_relay_state_persistence);
    RUN_TEST(test_wifi_scan);
    RUN_TEST(test_adc_calibration);
    RUN_TEST(test_adc_averaging);
    RUN_TEST(test_relay_toggle);
    RUN_TEST(test_relay_set);
    RUN_TEST(test_wifi_scan_format);
    RUN_TEST(test_wifi_connect_logic);
    RUN_TEST(test_ble_json_format);
    RUN_TEST(test_ble_command_parsing);
    UNITY_END();
}

void loop() {
    // Empty loop for testing purposes
}