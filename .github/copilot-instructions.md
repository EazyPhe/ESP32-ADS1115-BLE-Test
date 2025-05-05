# ESP32-ADS1115 BLE Project - Copilot Instructions

This document provides guidance for GitHub Copilot when working with this ESP32-based Bluetooth Low Energy project.

## Project Overview

This project consists of firmware for an ESP32 microcontroller that interfaces with ADS1115 ADC chips to measure electrical current/voltage, control multiple relays, and communicate over BLE with an Android application. It includes:

- ESP32 firmware with BLE server functionality
- ADC data acquisition and calibration
- Relay control for 4 external devices
- WiFi connectivity for OTA updates
- MCP (Machine Control Protocol) server for remote management

## Code Organization

### Key Components

- **BLE Module**: Handles Bluetooth communication with defined characteristics for data, relay control, and WiFi management
- **ADC Module**: Interfaces with ADS1115 chips and processes analog readings
- **Relay Module**: Controls GPIO pins connected to relays with state persistence
- **WiFi Module**: Manages network scanning, connection, and OTA updates
- **MCP Server**: Provides a WebSocket interface for external tools (including Copilot)

### Important Files

- ble_callbacks.h: Command processing and BLE callbacks
- ble_module.h: BLE service and characteristic definitions
- config.h: System-wide configuration and constants
- main.cpp: Application entry point and task management
- copilot-manifest.json: Copilot integration manifest

## BLE Command Protocol

When implementing new features, follow this established command protocol:

1. Define command in `SUPPORTED_COMMANDS[]` array in ble_callbacks.h
2. Add command validation in `validateCommand()` function
3. Implement command handling in appropriate callback class
4. Return responses using the corresponding characteristic's setValue/notify methods

### Command Format

Commands use a consistent format:
- `ACTION`: Simple commands (e.g., `CALIBRATE`, `SCAN`)
- `ACTION_PARAM`: Commands with one parameter (e.g., `TOGGLE_12`)
- `ACTION_PARAM_VALUE`: Commands with param and value (e.g., `SET_12_ON`)

### Response Format

Responses follow these patterns:
- `LOG:message`: Informational message
- `ERROR:type:details`: Error with type and details
- `ACTION_UPDATE:param:value`: State change notification

## Development Guidelines

### Adding New Features

1. If adding a new BLE command:
   - Add to `SUPPORTED_COMMANDS[]` array
   - Update validation in `validateCommand()`
   - Implement in appropriate callback class

2. If adding a new sensor or peripheral:
   - Create a dedicated module (.h/.cpp pair)
   - Initialize in `setup()`
   - Handle in dedicated FreeRTOS task if needed

3. If adding MCP functionality:
   - Register resource/tool in `registerResourcesAndTools()`
   - Implement handler function
   - Update copilot-manifest.json

### Code Conventions

- Use descriptive variable names in camelCase
- Functions should follow verbNoun naming pattern
- Constants in UPPER_CASE
- Add LOG_INFO/LOG_ERROR calls at appropriate points
- Use FreeRTOS mutex for shared resource access
- Store persistent settings using Preferences library

## Testing

When implementing new features:
1. Test BLE commands directly from Android app
2. Verify state persistence across reboots
3. Validate error handling with invalid inputs
4. Check memory usage impact with ESP.getFreeHeap()

## Android App Integration

The Android app communicates with the ESP32 using BLE. When modifying the firmware's BLE interface:
1. Ensure command formats remain backward compatible
2. Update protocol version if breaking changes are necessary
3. Add appropriate error responses for client handling

## Copilot Integration

This project uses VS Code Copilot integration via MCP protocol. The copilot-manifest.json defines available tools and resources. When adding new functionality:
1. Register in the manifest with appropriate descriptions
2. Implement the corresponding tools/resources in the MCP server
3. Test both via BLE and via Copilot interface