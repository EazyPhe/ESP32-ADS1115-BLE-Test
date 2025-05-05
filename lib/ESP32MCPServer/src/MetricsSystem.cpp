#include "MetricsSystem.h"
#include <WiFi.h>
#include <mutex>
#include <ArduinoJson.h>
#include <esp_timer.h>

using namespace mcp;

// Constants
static const char* BOOT_METRICS_FILE = "/boot_metrics.bin";
static const char* CONFIG_FILE = "/metrics_config.json";
static const uint32_t SAVE_INTERVAL = 60000; // 1 minute
static const size_t MAX_METRICS = 50;

// Static members initialization
std::mutex MetricsSystem::metricsMutex;

MetricsSystem::MetricsSystem() 
    : lastSaveTime(0)
    , initialized(false) {
}

MetricsSystem::~MetricsSystem() {
    end();
}

bool MetricsSystem::begin() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    if (initialized) {
        return true;
    }

    if (!LittleFS.begin(true)) {
        log_e("Failed to mount filesystem");
        return false;
    }

    // Initialize database
    if (!logger.begin()) {
        log_e("Failed to initialize logger");
        return false;
    }

    // Load boot metrics if they exist
    if (!loadBootMetrics()) {
        resetBootMetrics();
    }

    // Register system metrics
    initializeSystemMetrics();

    initialized = true;
    lastSaveTime = millis();
    return true;
}

void MetricsSystem::end() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    if (initialized) {
        saveBootMetrics();
        logger.end();
        initialized = false;
    }
}

void MetricsSystem::registerMetric(const String& name, MetricType type, const String& description,
                                   const String& unit, const String& category) {
    std::lock_guard<std::mutex> lock(metricsMutex);

    if (metrics.size() >= MAX_METRICS) {
        log_w("Max metrics limit reached, ignoring: %s", name.c_str());
        return;
    }

    MetricInfo info = {name, type, description, unit, category};
    metrics[name] = info;

    MetricValue value = {millis(), {}};
    switch (type) {
        case MetricType::COUNTER:
            value.counter = 0;
            break;
        case MetricType::GAUGE:
            value.gauge = 0.0;
            break;
        case MetricType::HISTOGRAM:
            value.histogram = {0.0, 0.0, 0.0, 0.0, 0};
            break;
    }
    bootMetrics[name] = value;
}

void MetricsSystem::registerCounter(const String& name, const String& description,
                                    const String& unit, const String& category) {
    registerMetric(name, MetricType::COUNTER, description, unit, category);
}

void MetricsSystem::registerGauge(const String& name, const String& description,
                                  const String& unit, const String& category) {
    registerMetric(name, MetricType::GAUGE, description, unit, category);
}

void MetricsSystem::registerHistogram(const String& name, const String& description,
                                      const String& unit, const String& category) {
    registerMetric(name, MetricType::HISTOGRAM, description, unit, category);
}
MetricValue MetricsSystem::getMetric(const String& name, bool fromBoot) {
    std::lock_guard<std::mutex> lock(metricsMutex);

    auto it = metrics.find(name);
    if (it == metrics.end()) {
        return MetricValue{};
    }

    if (fromBoot) {
        return bootMetrics[name];
    }

    std::vector<uLogger::Record> records;
    logger.queryMetrics(name.c_str(), 0, records); // Fetch records

    if (records.empty()) {
        return MetricValue{};
    }

    MetricValue result = {millis(), {}};
    switch (it->second.type) {
        case MetricType::COUNTER:
            result.counter = 0;
            for (const auto& record : records) {
                int64_t value;
                memcpy(&value, record.data, sizeof(value));
                result.counter += value;
            }
            break;
        case MetricType::GAUGE:
            memcpy(&result.gauge, records.back().data, sizeof(result.gauge)); // Most recent value
            break;
        case MetricType::HISTOGRAM:
            std::vector<MetricValue> histValues;
            for (const auto& record : records) {
                MetricValue hist;
                memcpy(&hist.histogram, record.data, sizeof(hist.histogram));
                histValues.push_back(hist);
            }
            result = calculateHistogram(histValues);
            break;
    }

    return result;
}


MetricValue MetricsSystem::calculateHistogram(const std::vector<MetricValue>& values) {
    MetricValue result = {millis(), {.histogram = {0.0, 0.0, 0.0, 0.0, 0}}};

    if (values.empty()) {
        return result;
    }

    auto& hist = result.histogram;
    hist.min = values[0].histogram.value;
    hist.max = values[0].histogram.value;
    hist.sum = 0;
    hist.count = values.size();

    for (const auto& v : values) {
        hist.min = std::min(hist.min, v.histogram.value);
        hist.max = std::max(hist.max, v.histogram.value);
        hist.sum += v.histogram.value;
    }

    hist.value = hist.sum / hist.count;
    return result;
}

void MetricsSystem::updateSystemMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    // Update WiFi signal strength if connected
    if (WiFi.status() == WL_CONNECTED) {
        setGauge("system.wifi.signal", WiFi.RSSI());
    }

    // Update heap metrics
    setGauge("system.heap.free", ESP.getFreeHeap());
    setGauge("system.heap.min", ESP.getMinFreeHeap());
    setGauge("system.uptime", millis());

    // Check if it's time to save boot metrics
    uint32_t now = millis();
    if (now - lastSaveTime >= SAVE_INTERVAL) {
        saveBootMetrics();
        lastSaveTime = now;
    }
}
bool MetricsSystem::saveBootMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex);

    File file = LittleFS.open(BOOT_METRICS_FILE, "w");
    if (!file) {
        log_e("Failed to open boot metrics file for writing");
        return false;
    }

    // Use StaticJsonDocument if the size is known at compile time.
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    for (const auto& pair : metrics) {
        JsonObject metric = root[pair.first].to<JsonObject>();
        metric["type"] = static_cast<int>(pair.second.type);
        metric["description"] = pair.second.description;
        metric["unit"] = pair.second.unit;
        metric["category"] = pair.second.category;
    }

    if (serializeJson(doc, file) == 0) {
        log_e("Failed to write metrics configuration");
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool MetricsSystem::loadBootMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    File file = LittleFS.open(BOOT_METRICS_FILE, "r");
    if (!file) {
        return false;
    }

    // Load metrics configuration
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        log_e("Failed to parse metrics configuration");
        file.close();
        return false;
    }

    metrics.clear();
    JsonObject root = doc.as<JsonObject>();
    for (JsonPair pair : root) {
        MetricInfo info;
        info.name = pair.key().c_str();
        info.type = static_cast<MetricType>(pair.value()["type"].as<int>());
        info.description = pair.value()["description"].as<const char*>();
        metrics[info.name] = info;
    }

    // Load boot metrics values
    if (!file.read((uint8_t*)&bootMetrics, sizeof(bootMetrics))) {
        log_e("Failed to read boot metrics values");
        file.close();
        return false;
    }

    file.close();
    return true;
}

void MetricsSystem::resetBootMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    
    bootMetrics.clear();
    for (const auto& pair : metrics) {
        MetricValue value = {millis(), {}};
        switch (pair.second.type) {
            case MetricType::COUNTER:
                value.counter = 0;
                break;
            case MetricType::GAUGE:
                value.gauge = 0.0;
                break;
            case MetricType::HISTOGRAM:
                value.histogram = {0.0, 0.0, 0.0, 0.0, 0};
                break;
        }
        bootMetrics[pair.first] = value;
    }
    
    saveBootMetrics();
}

bool MetricsSystem::isInitialized() const {
    return initialized;
}

void MetricsSystem::clearHistory() {
    std::lock_guard<std::mutex> lock(metricsMutex);
    logger.clear();
    resetBootMetrics();
}