/*
 * test_rule.c -- Unit tests for as_rule local rule engine.
 */
#include "as_rule.h"
#include "as_tool.h"
#include "as_telemetry.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Mock sensor that returns a fixed temperature */
static cJSON* mock_temp_sensor(const cJSON *params, void *ud) {
    (void)params; (void)ud;
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "temperature_c", 85.0);
    return result;
}

static void test_rule_registration(void) {
    printf("  test_rule_registration... ");

    as_rule_engine_init();
    assert(as_rule_count() == 0);

    as_local_rule_t rule = {
        .name = "overheat",
        .sensor_tool = "read_temp",
        .op = AS_COND_GT,
        .threshold = 80.0f,
        .value_field = "temperature_c",
        .action_type = AS_ACT_ALERT,
        .cooldown_ms = 1000,
        .enabled = true,
    };
    rule.action.alert.led_pattern = 1;

    int idx = as_rule_add(&rule);
    assert(idx == 0);
    assert(as_rule_count() == 1);

    const as_local_rule_t *r = as_rule_get(0);
    assert(r != NULL);
    assert(strcmp(r->name, "overheat") == 0);

    printf("PASS\n");
}

static void test_rule_condition_types(void) {
    printf("  test_rule_condition_types... ");

    as_rule_engine_init();

    /* Test BETWEEN */
    as_local_rule_t rule = {
        .name = "comfort_zone",
        .sensor_tool = "read_temp",
        .op = AS_COND_BETWEEN,
        .threshold = 20.0f,
        .threshold2 = 25.0f,
        .value_field = "temperature_c",
        .action_type = AS_ACT_NOTIFY_CLOUD,
        .cooldown_ms = 5000,
        .enabled = true,
    };
    assert(as_rule_add(&rule) == 0);

    /* Test GTE */
    rule.name = "min_temp";
    rule.op = AS_COND_GTE;
    rule.threshold = 15.0f;
    assert(as_rule_add(&rule) == 1);

    /* Test LTE */
    rule.name = "max_temp";
    rule.op = AS_COND_LTE;
    rule.threshold = 40.0f;
    assert(as_rule_add(&rule) == 2);

    assert(as_rule_count() == 3);
    printf("PASS\n");
}

static void test_rule_list(void) {
    printf("  test_rule_list... ");

    as_rule_engine_init();

    as_local_rule_t rule = {
        .name = "test_rule", .sensor_tool = "sensor",
        .op = AS_COND_GT, .threshold = 50.0f,
        .value_field = "value", .action_type = AS_ACT_ALERT,
        .cooldown_ms = 2000, .enabled = true,
    };
    as_rule_add(&rule);

    cJSON *list = as_rule_list();
    assert(list != NULL);
    assert(cJSON_GetArraySize(list) == 1);

    cJSON *first = cJSON_GetArrayItem(list, 0);
    assert(strcmp(cJSON_GetObjectItem(first, "name")->valuestring, "test_rule") == 0);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(first, "enabled")));

    cJSON_Delete(list);
    printf("PASS\n");
}

static void test_rule_enable_disable(void) {
    printf("  test_rule_enable_disable... ");

    as_rule_engine_init();

    as_local_rule_t rule = {
        .name = "toggle_rule", .sensor_tool = "sensor",
        .op = AS_COND_GT, .threshold = 0.0f,
        .value_field = "val", .action_type = AS_ACT_ALERT,
        .cooldown_ms = 1000, .enabled = true,
    };
    as_rule_add(&rule);

    assert(as_rule_set_enabled("toggle_rule", false) == 0);
    assert(as_rule_get(0)->enabled == false);

    assert(as_rule_set_enabled("toggle_rule", true) == 0);
    assert(as_rule_get(0)->enabled == true);

    /* Not found */
    assert(as_rule_set_enabled("nope", false) == -1);

    printf("PASS\n");
}

int main(void) {
    printf("=== test_rule ===\n");
    test_rule_registration();
    test_rule_condition_types();
    test_rule_list();
    test_rule_enable_disable();
    printf("All tests passed!\n");
    return 0;
}
