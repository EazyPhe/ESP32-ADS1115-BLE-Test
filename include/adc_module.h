#ifndef ADC_MODULE_H
#define ADC_MODULE_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

// Global variable declaration for sampling interval
extern volatile uint16_t samplingIntervalMs;

// Function declarations
void setupADC();
void calibrateADC();
bool initializeADS(Adafruit_ADS1115& ads, uint8_t address, const char* name);
int16_t readShuntDifferential();
int16_t readADS2Channel0();

// Global variables
extern Adafruit_ADS1115 ads1;
extern Adafruit_ADS1115 ads2;
extern bool ads1_available; // Flag for ADS1115 #1 availability
extern bool ads2_available; // Flag for ADS1115 #2 availability

// Changed to float type for Android app compatibility
extern float shuntOffsetFloat;
extern float ads2OffsetFloat;

#endif // ADC_MODULE_H
