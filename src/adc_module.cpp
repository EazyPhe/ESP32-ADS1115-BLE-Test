#include "adc_module.h"
#include "config.h"
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>

// Global ADS1115 instances
Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

// Global calibration values - ensuring float type for Android app compatibility
float shuntOffsetFloat = 0.0f;
float ads2OffsetFloat = 0.0f;

// Status flags
bool ads1_available = false;
bool ads2_available = false;

// Last good readings (for error recovery)
int16_t lastGoodShuntReading = 0;
int16_t lastGoodAds2Reading = 0;

// Error counters for I2C failures
uint8_t ads1ErrorCount = 0;
uint8_t ads2ErrorCount = 0;
const uint8_t MAX_ERRORS_BEFORE_RESET = 5;

// Time tracking for I2C recovery
unsigned long lastAds1RecoveryAttempt = 0;
unsigned long lastAds2RecoveryAttempt = 0;
const unsigned long RECOVERY_INTERVAL_MS = 5000; // Try recovery every 5 seconds

extern Preferences prefs;

bool initializeADS(Adafruit_ADS1115 &ads, uint8_t i2cAddress, const char* deviceName) {
    Wire.beginTransmission(i2cAddress);
    bool devicePresent = (Wire.endTransmission() == 0);
    
    if (!devicePresent) {
        LOG_ERROR("Could not find %s at I2C address 0x%02X", deviceName, i2cAddress);
        return false;
    }
    
    // Set a timeout for I2C operations
    Wire.setTimeOut(100); // 100ms timeout
    
    // Try to initialize with error handling
    bool success = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        try {
            success = ads.begin(i2cAddress);
            if (success) {
                LOG_INFO("%s initialized successfully at address 0x%02X", deviceName, i2cAddress);
                break;
            }
        } catch (...) {
            LOG_ERROR("%s initialization exception, retry %d", deviceName, attempt);
            delay(50); // Short delay between retries
        }
    }
    
    return success;
}

void setupADC() {
    if (!initializeADS(ads1, 0x48, "ADS1115 #1")) {
        // handle error or restart
    } else {
        ads1.setGain(GAIN_EIGHT);
        ads1.setDataRate(RATE_ADS1115_860SPS);
        ads1_available = true;
    }
    
    if (initializeADS(ads2, 0x49, "ADS1115 #2")) {
        ads2.setGain(GAIN_ONE);
        ads2.setDataRate(RATE_ADS1115_860SPS);
        ads2_available = true;
    } else {
        ads2_available = false;
    }
}

void calibrateADC() {
    LOG_INFO("Starting ADC calibration...");
    
    // Only calibrate if ADS1115 #1 is available
    if (ads1_available) {
        // Take multiple readings and average them for better accuracy
        int32_t sum = 0;
        const int samples = 16;
        
        for (int i = 0; i < samples; i++) {
            try {
                // Timeout protection for I2C operations
                int16_t reading = ads1.readADC_Differential_0_1();
                sum += reading;
                delay(10);
            } catch (...) {
                LOG_ERROR("Exception during calibration of ADS1115 #1");
                i--; // Retry this sample
                delay(50);
            }
        }
        
        shuntOffsetFloat = static_cast<float>(sum) / samples;
        LOG_INFO("ADS1115 #1 calibrated with offset: %f", shuntOffsetFloat);
    }
    
    // Only calibrate if ADS1115 #2 is available
    if (ads2_available) {
        int32_t sum = 0;
        const int samples = 16;
        
        for (int i = 0; i < samples; i++) {
            try {
                // Timeout protection for I2C operations
                int16_t reading = ads2.readADC_SingleEnded(0);
                sum += reading;
                delay(10);
            } catch (...) {
                LOG_ERROR("Exception during calibration of ADS1115 #2");
                i--; // Retry this sample
                delay(50);
            }
        }
        
        ads2OffsetFloat = static_cast<float>(sum) / samples;
        LOG_INFO("ADS1115 #2 calibrated with offset: %f", ads2OffsetFloat);
    }
}

int16_t readShuntDifferential() {
    if (!ads1_available) {
        // If device is marked unavailable, check if it's time to try recovery
        unsigned long currentTime = millis();
        if (currentTime - lastAds1RecoveryAttempt > RECOVERY_INTERVAL_MS) {
            lastAds1RecoveryAttempt = currentTime;
            LOG_INFO("Attempting to recover ADS1115 #1...");
            if (ads1.begin(0x48)) {
                ads1.setGain(GAIN_EIGHT);
                ads1.setDataRate(RATE_ADS1115_860SPS);
                ads1_available = true;
                ads1ErrorCount = 0;
                LOG_INFO("ADS1115 #1 recovered successfully");
            }
        }
        return lastGoodShuntReading;
    }
    
    // Try to read with error handling
    int16_t reading = 0;
    bool success = false;
    
    // Set a timeout for the I2C operation
    Wire.setTimeOut(100);
    
    // Attempt to read with timeout and error handling
    try {
        reading = ads1.readADC_Differential_0_1();
        success = true;
        ads1ErrorCount = 0; // Reset error counter on success
        lastGoodShuntReading = reading; // Update last good reading
    } catch (...) {
        LOG_ERROR("Exception during ADS1115 #1 read");
        success = false;
    }
    
    if (!success) {
        ads1ErrorCount++;
        LOG_WARNING("ADS1115 #1 read failed, error count: %d", ads1ErrorCount);
        
        if (ads1ErrorCount >= MAX_ERRORS_BEFORE_RESET) {
            LOG_ERROR("ADS1115 #1 marked unavailable after %d consecutive errors", ads1ErrorCount);
            ads1_available = false;
        }
        
        return lastGoodShuntReading;
    }
    
    return reading;
}

int16_t readADS2Channel0() {
    if (!ads2_available) {
        // If device is marked unavailable, check if it's time to try recovery
        unsigned long currentTime = millis();
        if (currentTime - lastAds2RecoveryAttempt > RECOVERY_INTERVAL_MS) {
            lastAds2RecoveryAttempt = currentTime;
            LOG_INFO("Attempting to recover ADS1115 #2...");
            if (ads2.begin(0x49)) {
                ads2.setGain(GAIN_ONE);
                ads2.setDataRate(RATE_ADS1115_860SPS);
                ads2_available = true;
                ads2ErrorCount = 0;
                LOG_INFO("ADS1115 #2 recovered successfully");
            }
        }
        return lastGoodAds2Reading;
    }
    
    // Try to read with error handling
    int16_t reading = 0;
    bool success = false;
    
    // Set a timeout for the I2C operation
    Wire.setTimeOut(100);
    
    // Attempt to read with timeout and error handling
    try {
        reading = ads2.readADC_SingleEnded(0);
        success = true;
        ads2ErrorCount = 0; // Reset error counter on success
        lastGoodAds2Reading = reading; // Update last good reading
    } catch (...) {
        LOG_ERROR("Exception during ADS1115 #2 read");
        success = false;
    }
    
    if (!success) {
        ads2ErrorCount++;
        LOG_WARNING("ADS1115 #2 read failed, error count: %d", ads2ErrorCount);
        
        if (ads2ErrorCount >= MAX_ERRORS_BEFORE_RESET) {
            LOG_ERROR("ADS1115 #2 marked unavailable after %d consecutive errors", ads2ErrorCount);
            ads2_available = false;
        }
        
        return lastGoodAds2Reading;
    }
    
    return reading;
}
