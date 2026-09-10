/*
 * as_telemetry.c -- Device telemetry implementation.
 *
 * Maps to agentscope-java:
 *   - AgentscopeStatusEndpoint → status()
 *   - MetricsHook → recordSafe()
 *   - heartbeat mechanism
 */
#include "as_telemetry.h"
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>

static const char *TAG = "as_telemetry";

/* Sensor data cache (fixed pool) */
typedef struct {
    char key[32];
    cJSON *value;
    bool used;
} sensor_entry_t;

static sensor_entry_t s_sensors[CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS];
static uint32_t s_tool_call_count = 0;
static uint32_t s_rule_trigger_count = 0;
static int64_t s_boot_time_us = 0;
static bool s_initialized = false;
static const char *s_firmware_version = NULL;
static const char *s_board_type = NULL;

void as_telemetry_init(void) {
    memset(s_sensors, 0, sizeof(s_sensors));
    s_tool_call_count = 0;
    s_rule_trigger_count = 0;
    s_boot_time_us = esp_timer_get_time();
    s_initialized = true;
    ESP_LOGI(TAG, "Telemetry initialized (max %d sensors)",
             CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS);
}

void as_telemetry_deinit(void) {
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS; i++) {
        if (s_sensors[i].used && s_sensors[i].value) {
            cJSON_Delete(s_sensors[i].value);
            s_sensors[i].value = NULL;
            s_sensors[i].used = false;
        }
    }
    s_initialized = false;
}

void as_telemetry_update_sensor(const char *key, const cJSON *value) {
    if (!s_initialized || !key || !value) return;

    /* Find existing entry */
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS; i++) {
        if (s_sensors[i].used && strcmp(s_sensors[i].key, key) == 0) {
            if (s_sensors[i].value) cJSON_Delete(s_sensors[i].value);
            s_sensors[i].value = cJSON_Duplicate(value, 1);
            return;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS; i++) {
        if (!s_sensors[i].used) {
            strncpy(s_sensors[i].key, key, sizeof(s_sensors[i].key) - 1);
            s_sensors[i].key[sizeof(s_sensors[i].key) - 1] = '\0';
            s_sensors[i].value = cJSON_Duplicate(value, 1);
            s_sensors[i].used = true;
            return;
        }
    }

    ESP_LOGW(TAG, "Sensor cache full, cannot store '%s'", key);
}

void as_telemetry_update_tool_call(const char *tool_name, const cJSON *result) {
    (void)tool_name;
    (void)result;
    if (s_initialized) s_tool_call_count++;
}

void as_telemetry_increment_rule_triggers(void) {
    if (s_initialized) s_rule_trigger_count++;
}

cJSON* as_telemetry_build_heartbeat(void) {
    if (!s_initialized) return NULL;

    cJSON *hb = cJSON_CreateObject();
    if (!hb) return NULL;

    int64_t uptime_us = esp_timer_get_time() - s_boot_time_us;
    cJSON_AddStringToObject(hb, "type", "heartbeat");
    cJSON_AddNumberToObject(hb, "uptime_s", (double)(uptime_us / 1000000));
    cJSON_AddNumberToObject(hb, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(hb, "tool_calls", s_tool_call_count);
    cJSON_AddNumberToObject(hb, "rule_triggers", s_rule_trigger_count);

    /* Sensor data */
    cJSON *sensors = cJSON_AddObjectToObject(hb, "sensors");
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS; i++) {
        if (s_sensors[i].used && s_sensors[i].value) {
            cJSON_AddItemToObject(sensors, s_sensors[i].key,
                                  cJSON_Duplicate(s_sensors[i].value, 1));
        }
    }

    return hb;
}

cJSON* as_telemetry_build_status(void) {
    if (!s_initialized) return NULL;

    cJSON *status = cJSON_CreateObject();
    if (!status) return NULL;

    int64_t uptime_us = esp_timer_get_time() - s_boot_time_us;

    /* Device info */
    cJSON *device = cJSON_AddObjectToObject(status, "device");
    cJSON_AddStringToObject(device, "firmware", s_firmware_version ? s_firmware_version : "unknown");
    cJSON_AddStringToObject(device, "board", s_board_type ? s_board_type : "unknown");
    cJSON_AddNumberToObject(device, "uptime_s", (double)(uptime_us / 1000000));

    /* Memory */
    cJSON *memory = cJSON_AddObjectToObject(status, "memory");
    cJSON_AddNumberToObject(memory, "free_heap", esp_get_free_heap_size());

    /* Stats */
    cJSON *stats = cJSON_AddObjectToObject(status, "stats");
    cJSON_AddNumberToObject(stats, "tool_calls", s_tool_call_count);
    cJSON_AddNumberToObject(stats, "rule_triggers", s_rule_trigger_count);

    /* Sensors */
    cJSON *sensors = cJSON_AddObjectToObject(status, "sensors");
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS; i++) {
        if (s_sensors[i].used && s_sensors[i].value) {
            cJSON_AddItemToObject(sensors, s_sensors[i].key,
                                  cJSON_Duplicate(s_sensors[i].value, 1));
        }
    }

    return status;
}
