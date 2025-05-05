#ifndef SAMPLING_CONFIG_H
#define SAMPLING_CONFIG_H

#include <Arduino.h>

// Functions to get/set the sampling interval
void setSamplingInterval(uint16_t intervalMs);
uint16_t getSamplingInterval();

#endif // SAMPLING_CONFIG_H