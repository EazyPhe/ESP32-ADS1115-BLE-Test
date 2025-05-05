# ESP32 MCP Implementation Guide

This document provides instructions for building, testing, and using the ESP32 Model Context Protocol (MCP) implementation.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Project Structure](#project-structure)
- [Building](#building)
- [Testing](#testing)
- [Development Guide](#development-guide)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software
- PlatformIO Core (CLI) or PlatformIO IDE
- Python 3.7 or higher
- Git

### Required Hardware
- ESP32 S3 DevKitC-1 board
- USB cable for programming
- (Optional) NeoPixel LED strip for LED control functionality

### Dependencies
All required libraries are managed through PlatformIO's dependency system:
- FastLED
- ArduinoJson
- LittleFS
- ESPAsyncWebServer
- AsyncTCP
- Unity (for testing)

## Project Structure

```
project_root/
├── platformio.ini          # Project configuration
├── HOWTO.md               # This file
├── data/
│   └── wifi_setup.html    # Web interface file
├── include/
│   ├── NetworkManager.h   # Network manager header
│   ├── RequestQueue.h     # Thread-safe queue header
│   ├── MCPServer.h        # MCP server header
│   └── MCPTypes.h         # MCP types header
├── src/
│   ├── main.cpp           # Main application file
│   ├── NetworkManager.cpp # Network manager implementation
│   └── MCPServer.cpp      # MCP server implementation
└── test/
    ├── test_main.cpp
    ├── test_request_queue.cpp
    ├── test_network_manager.cpp
    ├── test_mcp_server.cpp
    └── mock/              # Mock implementations for testing
        ├── mock_wifi.h
        ├── mock_websocket.h
        └── mock_littlefs.h
```

## Building

### Initial Setup
1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd <project-directory>
   ```

2. Install project dependencies:
   ```bash
   pio pkg install
   ```

3. Build the filesystem:
   ```bash
   pio run -t uploadfs
   ```

### Building the Project
1. For development build:
   ```bash
   pio run
   ```

2. For release build:
   ```bash
   pio run -e release
   ```

3. Build and upload to device:
   ```bash
   pio run -t upload
   ```

## Testing

### Running Tests
1. Run all tests:
   ```bash
   pio test -e native
   ```

2. Run specific test file:
   ```bash
   pio test -e native -f test_request_queue
   ```

3. Run tests with verbose output:
   ```bash
   pio test -e native -v
   ```

### Test Coverage
To generate test coverage report:
```bash
pio test -e native --coverage
```

## Development Guide

### Adding New MCP Resources
1. Define the resource in MCPTypes.h
2. Implement the resource handler in MCPServer
3. Register the resource in main.cpp

Example:
```cpp
// In main.cpp
mcp::MCPResource timeResource{
    "system_time",
    "system://time",
    "application/json",
    "System time information"
};
mcpServer.registerResource(timeResource);
```

### Network Configuration
The device starts in AP mode if no WiFi credentials are configured:
1. Connect to the ESP32 AP (SSID format: ESP32_XXXXXX)
2. Navigate to http://192.168.4.1
3. Enter WiFi credentials
4. Device will attempt to connect to the network

### MCP Protocol Implementation
The implementation follows the MCP specification with support for:
- Resource discovery
- Resource reading
- Resource updates via WebSocket
- Subscription system

## API Reference

### NetworkManager
- `begin()`: Initializes network management
- `isConnected()`: Checks WiFi connection status
- `getIPAddress()`: Gets current IP address
- `getSSID()`: Gets current network SSID

### MCPServer
- `begin(bool isNetworkConnected)`: Starts MCP server
- `registerResource(const MCPResource& resource)`: Registers new resource
- `unregisterResource(const std::string& uri)`: Removes resource
- `handleClient()`: Processes client requests

### RequestQueue
Thread-safe queue implementation for handling requests:
- `push(const T& item)`: Adds item to queue
- `pop(T& item)`: Removes and returns item from queue
- `empty()`: Checks if queue is empty
- `size()`: Returns number of items in queue

## Troubleshooting

### Common Issues
1. **Upload Failed**
   - Check USB connection
   - Verify board selection in platformio.ini
   - Try pressing the BOOT button during upload

2. **Network Connection Failed**
   - Verify WiFi credentials
   - Check signal strength
   - Ensure network supports ESP32

3. **Test Failures**
   - Run tests with -v flag for detailed output
   - Check mock implementations match real hardware behavior
   - Verify test dependencies are installed

### Debug Logging
Enable debug logging in platformio.ini:
```ini
build_flags = 
    -D CORE_DEBUG_LEVEL=5
```

### Support
For additional support:
1. Check issue tracker
2. Review MCP protocol documentation
3. Contact development team

## License
[License information here]