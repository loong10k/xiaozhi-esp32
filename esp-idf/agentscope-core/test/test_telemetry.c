/*
 * test_telemetry.c -- Unit tests for as_telemetry.
 */
#include "as_telemetry.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_init(void) {
    printf("  test_init... ");
    as_telemetry_init();
    as_telemetry_deinit();
    printf("PASS\n");
}

static void test_sensor_cache(void) {
    printf("  test_sensor_cache... ");

    as_telemetry_init();

    cJSON *temp = cJSON_CreateNumber(25.5);
    as_telemetry_update_sensor("temperature", temp);
    cJSON_Delete(temp);

    cJSON *humidity = cJSON_CreateNumber(60.0);
    as_telemetry_update_sensor("humidity", humidity);
    cJSON_Delete(humidity);

    /* Update existing */
    temp = cJSON_CreateNumber(26.0);
    as_telemetry_update_sensor("temperature", temp);
    cJSON_Delete(temp);

    cJSON *heartbeat = as_telemetry_build_heartbeat();
    assert(heartbeat != NULL);

    cJSON *sensors = cJSON_GetObjectItem(heartbeat, "sensors");
    assert(sensors != NULL);
    assert(cJSON_GetObjectItem(sensors, "temperature") != NULL);
    assert(cJSON_GetObjectItem(sensors, "humidity") != NULL);

    cJSON_Delete(heartbeat);
    as_telemetry_deinit();

    printf("PASS\n");
}

static void test_tool_call_counter(void) {
    printf("  test_tool_call_counter... ");

    as_telemetry_init();

    as_telemetry_update_tool_call("tool1", NULL);
    as_telemetry_update_tool_call("tool2", NULL);
    as_telemetry_update_tool_call("tool1", NULL);

    cJSON *status = as_telemetry_build_status();
    assert(status != NULL);
    cJSON *stats = cJSON_GetObjectItem(status, "stats");
    assert(cJSON_GetObjectItem(stats, "tool_calls")->valuedouble == 3);

    cJSON_Delete(status);
    as_telemetry_deinit();

    printf("PASS\n");
}

static void test_heartbeat_format(void) {
    printf("  test_heartbeat_format... ");

    as_telemetry_init();

    cJSON *hb = as_telemetry_build_heartbeat();
    assert(hb != NULL);
    assert(strcmp(cJSON_GetObjectItem(hb, "type")->valuestring, "heartbeat") == 0);
    assert(cJSON_GetObjectItem(hb, "uptime_s") != NULL);
    assert(cJSON_GetObjectItem(hb, "free_heap") != NULL);
    assert(cJSON_GetObjectItem(hb, "tool_calls") != NULL);
    assert(cJSON_GetObjectItem(hb, "rule_triggers") != NULL);

    cJSON_Delete(hb);
    as_telemetry_deinit();

    printf("PASS\n");
}

int main(void) {
    printf("=== test_telemetry ===\n");
    test_init();
    test_sensor_cache();
    test_tool_call_counter();
    test_heartbeat_format();
    printf("All tests passed!\n");
    return 0;
}
