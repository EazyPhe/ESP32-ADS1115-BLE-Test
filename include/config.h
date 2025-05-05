#ifndef CONFIG_H
#define CONFIG_H

// Define Tag
#define ESP32_LOG_TAG "ESP32_ADS1115"

#include <esp_log.h>
#include <Preferences.h>

// Protocol version using semantic versioning
#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 2
#define PROTOCOL_VERSION_PATCH 0
#define PROTOCOL_VERSION "1.2.0"

// Global configuration variables
extern volatile uint16_t samplingIntervalMs;

// Log level enum and macros
enum LogLevel { DEBUG_LEVEL, INFO_LEVEL, ERROR_LEVEL };
extern LogLevel currentLogLevel;
#define LOG_DEBUG(fmt, ...) if (currentLogLevel <= DEBUG_LEVEL) ESP_LOGD(ESP32_LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  if (currentLogLevel <= INFO_LEVEL)  ESP_LOGI(ESP32_LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) if (currentLogLevel <= INFO_LEVEL) ESP_LOGW(ESP32_LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) if (currentLogLevel <= ERROR_LEVEL) ESP_LOGE(ESP32_LOG_TAG, fmt, ##__VA_ARGS__)

extern Preferences prefs;

#endif // CONFIG_H