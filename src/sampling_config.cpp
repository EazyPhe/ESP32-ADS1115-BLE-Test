#include "sampling_config.h"

// Define the global variable here (internal linkage)
static volatile uint16_t _samplingIntervalMs = 17; // Default ~60Hz

volatile uint16_t samplingIntervalMs = 17; // Default ~60Hz, global definition for linker

void setSamplingInterval(uint16_t intervalMs) {
    if (intervalMs >= 5 && intervalMs <= 1000) {
        _samplingIntervalMs = intervalMs;
    }
}

uint16_t getSamplingInterval() {
    return _samplingIntervalMs;
}