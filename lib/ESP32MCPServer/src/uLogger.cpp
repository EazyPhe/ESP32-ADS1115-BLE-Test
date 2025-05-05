#include "uLogger.h"

uLogger::uLogger() : initialized(false) {}

uLogger::~uLogger() {
    end();
}

bool uLogger::begin(const char* logFile) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return true;
    }

    logFilePath = logFile;
    
    // Try to open existing log file
    if (!openLog("r+")) {
        // If file doesn't exist, create it
        if (!openLog("w+")) {
            log_e("Failed to create log file");
            return false;
        }
    }
    
    closeLog();
    initialized = true;
    return true;
}

void uLogger::end() {
    std::lock_guard<std::mutex> lock(mutex);
    closeLog();
    initialized = false;
}

bool uLogger::logMetric(const char* name, const void* data, size_t dataSize) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !name || !data || dataSize > MAX_DATA_LENGTH) {
        return false;
    }

    Record record;
    record.timestamp = millis();
    strncpy(record.name, name, MAX_NAME_LENGTH - 1);
    record.dataSize = static_cast<uint16_t>(dataSize);
    memcpy(record.data, data, dataSize);

    if (!openLog("a+")) {
        return false;
    }

    bool success = writeRecord(record);
    
    // Check if we need to rotate the log
    if (logFile.size() >= MAX_FILE_SIZE) {
        rotateLog();
    }

    closeLog();
    return success;
}

size_t uLogger::queryMetrics(const char* name, uint64_t startTime, std::vector<Record>& records) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !openLog("r")) {
        return 0;
    }

    size_t count = 0;
    Record record;
    
    while (readRecord(record)) {
        if (record.timestamp >= startTime &&
            (name[0] == '\0' || strcmp(record.name, name) == 0)) {
            records.push_back(record);
            count++;
        }
    }

    closeLog();
    return count;
}

size_t uLogger::queryMetrics(std::function<bool(const Record&)> callback, 
                           const char* name, uint64_t startTime) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !openLog("r")) {
        return 0;
    }

    size_t count = 0;
    Record record;
    
    while (readRecord(record)) {
        if (record.timestamp >= startTime &&
            (name[0] == '\0' || strcmp(record.name, name) == 0)) {
            if (!callback(record)) {
                break;
            }
            count++;
        }
    }

    closeLog();
    return count;
}

size_t uLogger::getRecordCount() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized || !openLog("r")) {
        return 0;
    }

    size_t count = 0;
    Record record;
    
    while (readRecord(record)) {
        count++;
    }

    closeLog();
    return count;
}

bool uLogger::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    
    closeLog();
    return LittleFS.remove(logFilePath.c_str());
}

bool uLogger::compact(uint64_t maxAge) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!initialized) {
        return false;
    }

    String tempPath = logFilePath + ".tmp";
    File tempFile = LittleFS.open(tempPath.c_str(), "w+");
    if (!tempFile) {
        return false;
    }

    if (!openLog("r")) {
        tempFile.close();
        LittleFS.remove(tempPath.c_str());
        return false;
    }

    uint64_t cutoffTime = millis() - maxAge;
    Record record;
    size_t count = 0;

    while (readRecord(record)) {
        if (record.timestamp >= cutoffTime) {
            size_t recordSize = sizeof(record.timestamp) + sizeof(record.dataSize) +
                              strlen(record.name) + 1 + record.dataSize;
            if (tempFile.write((uint8_t*)&record, recordSize) != recordSize) {
                closeLog();
                tempFile.close();
                LittleFS.remove(tempPath.c_str());
                return false;
            }
            count++;
        }
    }

    closeLog();
    tempFile.close();

    // Replace old file with new one
    LittleFS.remove(logFilePath.c_str());
    LittleFS.rename(tempPath.c_str(), logFilePath.c_str());

    return true;
}

bool uLogger::openLog(const char* mode) {
    if (logFile) {
        return true;
    }
    
    logFile = LittleFS.open(logFilePath.c_str(), mode);
    return logFile;
}

void uLogger::closeLog() {
    if (logFile) {
        logFile.close();
    }
}

bool uLogger::writeRecord(const Record& record) {
    size_t recordSize = sizeof(record.timestamp) + sizeof(record.dataSize) +
                       strlen(record.name) + 1 + record.dataSize;
                       
    return logFile.write((uint8_t*)&record, recordSize) == recordSize;
}

bool uLogger::readRecord(Record& record) {
    if (!logFile.available()) {
        return false;
    }

    // Read timestamp and data size
    if (logFile.read((uint8_t*)&record.timestamp, sizeof(record.timestamp)) != sizeof(record.timestamp) ||
        logFile.read((uint8_t*)&record.dataSize, sizeof(record.dataSize)) != sizeof(record.dataSize)) {
        return false;
    }

    // Read name
    size_t nameLen = 0;
    while (nameLen < MAX_NAME_LENGTH - 1) {
        char c = logFile.read();
        if (c == '\0') break;
        record.name[nameLen++] = c;
    }
    record.name[nameLen] = '\0';

    // Read data
    if (record.dataSize > MAX_DATA_LENGTH ||
        logFile.read(record.data, record.dataSize) != record.dataSize) {
        return false;
    }

    return true;
}

bool uLogger::seekToStart() {
    return logFile.seek(0);
}

bool uLogger::rotateLog() {
    // Create a temporary buffer for the most recent records
    std::vector<Record> recentRecords;
    size_t totalSize = 0;
    
    // Read records from the end until we reach half the maximum file size
    while (totalSize < MAX_FILE_SIZE / 2) {
        Record record;
        if (!readRecord(record)) {
            break;
        }
        recentRecords.push_back(record);
        totalSize += sizeof(record.timestamp) + sizeof(record.dataSize) +
                    strlen(record.name) + 1 + record.dataSize;
    }

    // Clear the file
    closeLog();
    if (!clear() || !openLog("w+")) {
        return false;
    }

    // Write back the recent records
    for (const auto& record : recentRecords) {
        if (!writeRecord(record)) {
            return false;
        }
    }

    return true;
}