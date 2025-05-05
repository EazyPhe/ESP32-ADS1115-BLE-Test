#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include "uLogger.h"

namespace mcp{
struct MetricValue {
        uint64_t timestamp;
        union {
            int64_t counter;
            double gauge;
            struct {
                double value;    // Current/last value
                double min;      // Minimum value
                double max;      // Maximum value
                double sum;      // Sum of all values
                uint32_t count;  // Number of values
            } histogram;
        };
    };

class MetricsSystem {
public:
    // Metric types supported by the system
    enum class MetricType {
        COUNTER,    // Incremental values
        GAUGE,      // Point-in-time values
        HISTOGRAM   // Statistical distribution
    };

    // Structure to hold metric values
   
    // Metadata about a metric
    struct MetricInfo {
        String name;
        MetricType type;
        String description;
        String unit;        // Optional unit of measurement
        String category;    // Optional grouping category
    };

    // Singleton instance access
    static MetricsSystem& getInstance() {
        static MetricsSystem instance;
        return instance;
    }

    /**
     * Initialize the metrics system
     * @return true if initialization successful
     */
    bool begin();

    /**
     * Shutdown the metrics system
     */
    void end();

    /**
     * Register a new counter metric
     * @param name Unique metric identifier
     * @param description Human-readable description
     * @param unit Optional unit of measurement
     * @param category Optional grouping category
     */
    void registerCounter(const String& name, const String& description,
                        const String& unit = "", const String& category = "");

    /**
     * Register a new gauge metric
     * @param name Unique metric identifier
     * @param description Human-readable description
     * @param unit Optional unit of measurement
     * @param category Optional grouping category
     */
    void registerGauge(const String& name, const String& description,
                      const String& unit = "", const String& category = "");

    /**
     * Register a new histogram metric
     * @param name Unique metric identifier
     * @param description Human-readable description
     * @param unit Optional unit of measurement
     * @param category Optional grouping category
     */
    void registerHistogram(const String& name, const String& description,
                         const String& unit = "", const String& category = "");

    /**
     * Increment a counter metric
     * @param name Metric identifier
     * @param value Amount to increment by (default: 1)
     */
    void incrementCounter(const String& name, int64_t value = 1);

    /**
     * Set a gauge metric value
     * @param name Metric identifier
     * @param value New gauge value
     */
    void setGauge(const String& name, double value);

    /**
     * Record a value in a histogram metric
     * @param name Metric identifier
     * @param value Value to record
     */
    void recordHistogram(const String& name, double value);

    /**
     * Get current value of a metric
     * @param name Metric identifier
     * @param fromBoot If true, return value since last boot, otherwise all-time
     * @return Current metric value
     */
    MetricValue getMetric(const String& name, bool fromBoot = true);

    /**
     * Get historical values for a metric
     * @param name Metric identifier
     * @param seconds Time window in seconds (0 for all time)
     * @return Vector of metric values
     */
    std::vector<MetricValue> getMetricHistory(const String& name, uint32_t seconds = 0);

    /**
     * Get information about all registered metrics
     * @param category Optional category filter
     * @return Map of metric names to their information
     */
    std::map<String, MetricInfo> getMetrics(const String& category = "");

    /**
     * Update system metrics (called periodically)
     */
    void updateSystemMetrics();

    /**
     * Reset boot-time metrics
     */
    void resetBootMetrics();

    /**
     * Save current metrics state
     * @return true if save successful
     */
    bool saveBootMetrics();

    /**
     * Load saved metrics state
     * @return true if load successful
     */
    bool loadBootMetrics();

    /**
     * Clear all historical metric data
     */
    void clearHistory();

    /**
     * Check if metrics system is initialized
     * @return true if initialized
     */
    bool isInitialized() const;

private:
    MetricsSystem();
    ~MetricsSystem();
    MetricsSystem(const MetricsSystem&) = delete;
    MetricsSystem& operator=(const MetricsSystem&) = delete;

    static std::mutex metricsMutex;
    bool initialized;
    uint32_t lastSaveTime;

    std::map<String, MetricInfo> metrics;
    std::map<String, MetricValue> bootMetrics;
    uLogger logger;

    void initializeSystemMetrics();
    void registerMetric(const String& name, MetricType type, const String& description,
                       const String& unit = "", const String& category = "");
    MetricValue calculateHistogram(const std::vector<MetricValue>& values);
};

/**
 * Helper class for timing operations and recording them as histogram metrics
 */
class MetricTimer {
public:
    /**
     * Start timing an operation
     * @param metricName Name of histogram metric to record to
     */
    MetricTimer(const String& metricName) 
        : name(metricName), startTime(micros()) {}
    
    /**
     * Stop timing and record duration
     */
    ~MetricTimer() {
        uint32_t duration = micros() - startTime;
        MetricsSystem::getInstance().recordHistogram(name, duration / 1000.0); // Convert to ms
    }

private:
    String name;
    uint32_t startTime;
};

// Macro for timing a scoped operation
#define METRIC_TIMER(name) MetricTimer __timer(name)
} // namespace mcp