#include <unity.h>

// Declare test functions from other test files
void test_request_queue();
void test_network_manager();
void test_mcp_server();

void setUp(void) {
    // Global setup
}

void tearDown(void) {
    // Global cleanup
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Run all test groups
    test_request_queue();
    test_network_manager();
    test_mcp_server();
    
    return UNITY_END();
}