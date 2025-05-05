#include <unity.h>
#include "NetworkManager.h"
#include "mock_wifi.h"  // Mock implementation for testing

// Mock implementations for hardware-dependent functions
static bool mock_wifi_connected = false;
static String mock_ssid;
static String mock_password;

void setUp(void) {
    mock_wifi_connected = false;
    mock_ssid = "";
    mock_password = "";
}

void tearDown(void) {
}

// Test network manager initialization
void test_network_manager_init() {
    NetworkManager manager;
    TEST_ASSERT_FALSE(manager.isConnected());
    TEST_ASSERT_EQUAL_STRING("", manager.getSSID().c_str());
}

// Test AP mode functionality
void test_network_manager_ap_mode() {
    NetworkManager manager;
    manager.begin();  // Should start in AP mode without credentials
    
    TEST_ASSERT_FALSE(manager.isConnected());
    TEST_ASSERT_TRUE(manager.getIPAddress().startsWith("192.168.4."));  // Default AP IP range
    TEST_ASSERT_TRUE(manager.getSSID().startsWith("ESP32_"));          // AP SSID format
}

// Test WiFi connection process
void test_network_manager_wifi_connect() {
    NetworkManager manager;
    
    // Simulate saving valid credentials
    manager.saveCredentials("TestSSID", "TestPassword");
    
    // Verify credentials were saved
    TEST_ASSERT_EQUAL_STRING("TestSSID", mock_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPassword", mock_password.c_str());
    
    // Simulate successful connection
    mock_wifi_connected = true;
    
    // Let the manager process the connection
    for (int i = 0; i < 10; i++) {
        manager.handleClient();
        delay(100);
    }
    
    TEST_ASSERT_TRUE(manager.isConnected());
    TEST_ASSERT_EQUAL_STRING("TestSSID", manager.getSSID().c_str());
}

// Test connection failure and fallback
void test_network_manager_connection_failure() {
    NetworkManager manager;
    
    // Set invalid credentials
    manager.saveCredentials("InvalidSSID", "InvalidPass");
    
    // Simulate connection failure
    mock_wifi_connected = false;
    
    // Let the manager attempt connections
    for (int i = 0; i < 50; i++) {  // Give enough time for retries
        manager.handleClient();
        delay(100);
    }
    
    // Should be in AP mode after failures
    TEST_ASSERT_FALSE(manager.isConnected());
    TEST_ASSERT_TRUE(manager.getIPAddress().startsWith("192.168.4."));
}

// Test WebSocket notification system
void test_network_manager_websocket_notifications() {
    NetworkManager manager;
    MockWebSocketClient client;
    
    manager.begin();
    
    // Connect client
    client.connect();
    
    // Change network state
    manager.saveCredentials("NewSSID", "NewPass");
    mock_wifi_connected = true;
    
    // Let the manager process
    for (int i = 0; i < 5; i++) {
        manager.handleClient();
        delay(100);
    }
    
    // Verify client received status updates
    TEST_ASSERT_TRUE(client.receivedMessage().contains("connected"));
    TEST_ASSERT_TRUE(client.receivedMessage().contains("NewSSID"));
}

int runUnityTests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_network_manager_init);
    RUN_TEST(test_network_manager_ap_mode);
    RUN_TEST(test_network_manager_wifi_connect);
    RUN_TEST(test_network_manager_connection_failure);
    RUN_TEST(test_network_manager_websocket_notifications);
    
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runUnityTests();
}

void loop() {
}
#else
int main() {
    return runUnityTests();
}
#endif