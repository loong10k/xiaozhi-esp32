/*
 * as_telemetry.h -- Device telemetry and status aggregation.
 *
 * Maps to agentscope-java:
 *   - AgentscopeStatusEndpoint → as_telemetry_build_status()
 *   - MetricsHook              → as_telemetry_update_tool_call()
 *   - heartbeat                → as_telemetry_build_heartbeat()
 *
 * Collects device metrics (uptime, heap, WiFi, sensor data) and
 * generates JSON for cloud reporting.
 */
#pragma once

#include <cJSON.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Device status snapshot. */
typedef struct {
    int64_t uptime_seconds;         /**< Seconds since boot */
    int free_heap_bytes;            /**< Current free heap */
    int min_free_heap_bytes;        /**< Minimum free heap since boot */
    int wifi_rssi;                  /**< WiFi signal strength (dBm) */
    int task_count;                 /**< Number of FreeRTOS tasks */
    const char *firmware_version;   /**< Firmware version string */
    const char *board_type;         /**< Board type */
    uint32_t tool_call_count;       /**< Total tool calls since boot */
    uint32_t rule_trigger_count;    /**< Total rule triggers since boot */
} as_device_status_t;

/* === Lifecycle === */

/** Initialize the telemetry system. */
void as_telemetry_init(void);

/** Deinitialize the telemetry system. */
void as_telemetry_deinit(void);

/* === Sensor Data Cache === */

/**
 * Update a sensor data entry in the telemetry cache.
 * Stores latest value keyed by name (e.g., "temperature", "humidity").
 * String values are copied into internal buffer.
 *
 * @param key    Sensor name (e.g., "temperature")
 * @param value  Sensor value as cJSON (number, string, or bool)
 */
void as_telemetry_update_sensor(const char *key, const cJSON *value);

/**
 * Update tool call statistics.
 * Called automatically by MCP bridge after each tool execution.
 */
void as_telemetry_update_tool_call(const char *tool_name, const cJSON *result);

/** Increment rule trigger counter. */
void as_telemetry_increment_rule_triggers(void);

/* === Status Generation === */

/**
 * Build heartbeat JSON for periodic cloud reporting.
 *
 * Format:
 *   { "type": "heartbeat", "uptime_s": N, "free_heap": N,
 *     "wifi_rssi": N, "tool_calls": N, "rule_triggers": N,
 *     "sensors": { ... } }
 *
 * Caller must cJSON_Delete().
 */
cJSON* as_telemetry_build_heartbeat(void);

/**
 * Build full device status JSON.
 *
 * Format:
 *   { "device": { "firmware", "board", "uptime" },
 *     "memory": { "free_heap", "min_free_heap" },
 *     "network": { "wifi_rssi" },
 *     "stats": { "tool_calls", "rule_triggers", "tasks" },
 *     "sensors": { ... } }
 *
 * Caller must cJSON_Delete().
 */
cJSON* as_telemetry_build_status(void);

#ifdef __cplusplus
}
#endif
