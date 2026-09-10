/*
 * test_tool.c -- Unit tests for as_tool registry.
 */
#include "as_tool.h"
#include "as_hook.h"
#include "as_telemetry.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Mock tool functions */
static cJSON* mock_temperature(const cJSON *params, void *ud) {
    (void)params; (void)ud;
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "temperature_c", 25.5);
    return result;
}

static cJSON* mock_led(const cJSON *params, void *ud) {
    (void)ud;
    cJSON *result = cJSON_CreateObject();
    const cJSON *state = cJSON_GetObjectItem(params, "state");
    cJSON_AddBoolToObject(result, "success", cJSON_IsTrue(state));
    return result;
}

static const as_tool_param_t temp_params[] = {
    { .name = "unit", .type = "string", .description = "Temperature unit", .required = false,
      .has_default = true, .str_default = "celsius" },
};

static const as_tool_param_t led_params[] = {
    { .name = "state", .type = "boolean", .description = "LED on/off", .required = true },
    { .name = "brightness", .type = "integer", .description = "Brightness 0-100", .required = false,
      .has_default = true, .int_default = 50, .has_range = true, .min_value = 0, .max_value = 100 },
};

static void test_registration(void) {
    printf("  test_registration... ");

    as_tool_registry_init();
    assert(as_tool_count() == 0);

    const as_tool_entry_t temp_tool = {
        .name = "read_temperature", .description = "Read temperature sensor",
        .params = temp_params, .param_count = 1,
        .execute = mock_temperature, .user_data = NULL,
        .visibility = AS_TOOL_VISIBILITY_BOTH,
    };
    int idx = as_tool_register(&temp_tool);
    assert(idx == 0);
    assert(as_tool_count() == 1);

    const as_tool_entry_t led_tool = {
        .name = "set_led", .description = "Control LED",
        .params = led_params, .param_count = 2,
        .execute = mock_led, .user_data = NULL,
        .visibility = AS_TOOL_VISIBILITY_AI,
    };
    idx = as_tool_register(&led_tool);
    assert(idx == 1);
    assert(as_tool_count() == 2);

    printf("PASS\n");
}

static void test_find(void) {
    printf("  test_find... ");

    const as_tool_entry_t *t = as_tool_find("read_temperature");
    assert(t != NULL);
    assert(strcmp(t->name, "read_temperature") == 0);
    assert(t->param_count == 1);

    t = as_tool_find("nonexistent");
    assert(t == NULL);

    printf("PASS\n");
}

static void test_execute(void) {
    printf("  test_execute... ");

    as_telemetry_init();

    cJSON *result = as_tool_execute("read_temperature", NULL);
    assert(result != NULL);
    const cJSON *temp = cJSON_GetObjectItem(result, "temperature_c");
    assert(cJSON_IsNumber(temp));
    assert(temp->valuedouble == 25.5);
    cJSON_Delete(result);

    /* Test with params */
    cJSON *params = cJSON_CreateObject();
    cJSON_AddBoolToObject(params, "state", true);
    result = as_tool_execute("set_led", params);
    assert(result != NULL);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(result, "success")));
    cJSON_Delete(result);
    cJSON_Delete(params);

    /* Test not found */
    result = as_tool_execute("no_such_tool", NULL);
    assert(result == NULL);

    as_telemetry_deinit();
    printf("PASS\n");
}

static void test_mcp_schema(void) {
    printf("  test_mcp_schema... ");

    cJSON *schema = as_tool_registry_to_mcp_schema();
    assert(schema != NULL);
    assert(cJSON_GetArraySize(schema) == 2);

    /* Check first tool schema */
    cJSON *first = cJSON_GetArrayItem(schema, 0);
    assert(cJSON_GetObjectItem(first, "name") != NULL);
    assert(cJSON_GetObjectItem(first, "description") != NULL);
    assert(cJSON_GetObjectItem(first, "inputSchema") != NULL);

    cJSON_Delete(schema);

    /* AI-only should exclude user-only tools */
    /* (none are user-only in this test, so both should appear) */
    schema = as_tool_registry_to_mcp_schema_ai_only();
    assert(cJSON_GetArraySize(schema) == 2);
    cJSON_Delete(schema);

    printf("PASS\n");
}

static void test_macro_registration(void) {
    printf("  test_macro_registration... ");

    /* AS_TOOL_REGISTER macro test */
    static const as_tool_param_t echo_params[] = {
        { .name = "message", .type = "string", .description = "Echo message", .required = true },
    };

    AS_TOOL_REGISTER("echo", "Echo back the message", echo_params, mock_temperature);
    assert(as_tool_find("echo") != NULL);

    /* AS_TOOL_REGISTER_NO_PARAMS macro test */
    AS_TOOL_REGISTER_NO_PARAMS("ping", "Ping the device", mock_temperature);
    assert(as_tool_find("ping") != NULL);

    printf("PASS\n");
}

int main(void) {
    printf("=== test_tool ===\n");
    test_registration();
    test_find();
    test_execute();
    test_mcp_schema();
    test_macro_registration();
    printf("All tests passed!\n");
    return 0;
}
