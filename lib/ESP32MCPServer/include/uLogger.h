#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include <functional>
#include <mutex>

class uLogger {
public:
    static const size_t MAX_NAME_LENGTH = 64;
    static const size_t MAX_DATA_LENGTH = 128;
    
    // Record structure for storing metric data
    struct Record {
        uint64_t timestamp;          // Timestamp in milliseconds
        char name[MAX_NAME_LENGTH];  // Metric name
        uint16_t dataSize;          // Size of data payload
        uint8_t data[MAX_DATA_LENGTH]; // Data payload
        
        Record() : timestamp(0), dataSize(0) {
            memset(name, 0, MAX_NAME_LENGTH);
            memset(data, 0, MAX_DATA_LENGTH);
        }
    };

    uLogger();
    ~uLogger();

    /**
     * Initialize the logger
     * @param logFile Path to log file (default: "/metrics.log")
     * @return true if initialization successful
     */
    bool begin(const char* logFile = "/metrics.log");

    /**
     * Shutdown the logger
     */
    void end();

    /**
     * Log a metric record
     * @param name Metric name
     * @param data Pointer to data
     * @param dataSize Size of data in bytes
     * @return true if log successful
     */
    bool logMetric(const char* name, const void* data, size_t dataSize);

    /**
     * Query metric records
     * @param name Metric name (empty string for all metrics)
     * @param startTime Start timestamp (0 for all time)
     * @param records Vector to store matching records
     * @return Number of records found
     */
    size_t queryMetrics(const char* name, uint64_t startTime, std::vector<Record>& records);

    /**
     * Query metrics with callback
     * @param callback Function to call for each matching record
     * @param name Metric name filter
     * @param startTime Start timestamp filter
     * @return Number of records processed
     */
    size_t queryMetrics(std::function<bool(const Record&)> callback, 
                       const char* name = "", uint64_t startTime = 0);

    /**
     * Get total number of records
     * @return Record count
     */
    size_t getRecordCount();

    /**
     * Clear all log data
     * @return true if successful
     */
    bool clear();

    /**
     * Compact the log file by removing old records
     * @param maxAge Maximum age of records to keep (in milliseconds)
     * @return true if successful
     */
    bool compact(uint64_t maxAge);

private:
    static const size_t BUFFER_SIZE = 4096;
    static const size_t MAX_FILE_SIZE = 1024 * 1024; // 1MB
    
    File logFile;
    String logFilePath;
    std::mutex mutex;
    bool initialized;

    bool openLog(const char* mode);
    void closeLog();
    bool writeRecord(const Record& record);
    bool readRecord(Record& record);
    bool seekToStart();
    bool rotateLog();
};