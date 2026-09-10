/*
 * as_rule.c -- Local rule engine implementation.
 *
 * Maps to agentscope-java: Middleware intercepting acting phase + RequestStopEvent.
 * Edge-specific: evaluates sensor data against thresholds, triggers local actions.
 */
#include "as_rule.h"
#include "as_tool.h"
#include "as_telemetry.h"
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "as_rule";

static as_local_rule_t s_rules[CONFIG_AGENTSCOPE_CORE_MAX_RULES];
static size_t s_rule_count = 0;
static bool s_initialized = false;

void as_rule_engine_init(void) {
    memset(s_rules, 0, sizeof(s_rules));
    s_rule_count = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "Rule engine initialized (max %d rules)", CONFIG_AGENTSCOPE_CORE_MAX_RULES);
}

void as_rule_engine_deinit(void) {
    s_rule_count = 0;
    s_initialized = false;
}

int as_rule_add(const as_local_rule_t *rule) {
    if (!s_initialized || !rule || !rule->name || !rule->sensor_tool) return -1;

    if (s_rule_count >= CONFIG_AGENTSCOPE_CORE_MAX_RULES) {
        ESP_LOGE(TAG, "Rule table full");
        return -1;
    }

    /* Check duplicate */
    for (size_t i = 0; i < s_rule_count; i++) {
        if (strcmp(s_rules[i].name, rule->name) == 0) {
            ESP_LOGW(TAG, "Rule '%s' already exists, overwriting", rule->name);
            s_rules[i] = *rule;
            return (int)i;
        }
    }

    s_rules[s_rule_count] = *rule;
    s_rules[s_rule_count].last_triggered_ms = 0;
    if (s_rules[s_rule_count].cooldown_ms == 0) {
        s_rules[s_rule_count].cooldown_ms = 5000; /* default 5s */
    }
    ESP_LOGI(TAG, "Registered rule[%d]: '%s' (%s %s %.1f)",
             (int)s_rule_count, rule->name, rule->sensor_tool,
             rule->op == AS_COND_GT ? ">" :
             rule->op == AS_COND_LT ? "<" :
             rule->op == AS_COND_EQ ? "==" :
             rule->op == AS_COND_GTE ? ">=" :
             rule->op == AS_COND_LTE ? "<=" : "between",
             rule->threshold);
    return (int)s_rule_count++;
}

static float extract_float_field(const cJSON *json, const char *field) {
    const cJSON *item = cJSON_GetObjectItem(json, field);
    if (cJSON_IsNumber(item)) {
        return (float)item->valuedouble;
    }
    return 0.0f;
}

static bool evaluate_condition(as_cond_op_t op, float value, float threshold, float threshold2) {
    switch (op) {
        case AS_COND_GT:      return value > threshold;
        case AS_COND_LT:      return value < threshold;
        case AS_COND_EQ:      return value == threshold;
        case AS_COND_GTE:     return value >= threshold;
        case AS_COND_LTE:     return value <= threshold;
        case AS_COND_BETWEEN: return value >= threshold && value <= threshold2;
    }
    return false;
}

static void execute_action(const as_local_rule_t *rule) {
    switch (rule->action_type) {
        case AS_ACT_SET_GPIO:
            ESP_LOGW(TAG, "Rule '%s': SET_GPIO %d = %d",
                     rule->name, rule->action.gpio.gpio_num, rule->action.gpio.level);
            /* TODO: gpio_set_level() on real hardware */
            break;

        case AS_ACT_INVOKE_TOOL:
            ESP_LOGW(TAG, "Rule '%s': INVOKE_TOOL '%s'",
                     rule->name, rule->action.invoke.tool);
            as_tool_execute(rule->action.invoke.tool, rule->action.invoke.params);
            break;

        case AS_ACT_NOTIFY_CLOUD:
            ESP_LOGW(TAG, "Rule '%s': NOTIFY_CLOUD event='%s'",
                     rule->name, rule->action.notify.event);
            /* TODO: send via xiaozhi Protocol when integrated */
            break;

        case AS_ACT_ALERT:
            ESP_LOGW(TAG, "Rule '%s': ALERT pattern=%d",
                     rule->name, rule->action.alert.led_pattern);
            /* TODO: trigger LED/buzzer pattern */
            break;
    }
}

static void evaluate_rule(const as_local_rule_t *rule, bool ignore_cooldown) {
    if (!rule->enabled) return;

    /* Check cooldown */
    if (!ignore_cooldown) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (rule->last_triggered_ms > 0 &&
            (uint32_t)(now_ms - rule->last_triggered_ms) < rule->cooldown_ms) {
            return; /* Still in cooldown */
        }
    }

    /* Read sensor via registered tool */
    cJSON *result = as_tool_execute(rule->sensor_tool, NULL);
    if (!result) return;

    /* Extract value from result */
    float value = extract_float_field(result, rule->value_field);
    cJSON_Delete(result);

    /* Evaluate condition */
    if (evaluate_condition(rule->op, value, rule->threshold, rule->threshold2)) {
        ESP_LOGW(TAG, "Rule '%s' TRIGGERED: %s=%.2f %s %.2f",
                 rule->name, rule->value_field, value,
                 rule->op == AS_COND_GT ? ">" :
                 rule->op == AS_COND_LT ? "<" : "...",
                 rule->threshold);

        /* Update cooldown */
        as_local_rule_t *mutable_rule = (as_local_rule_t *)rule;
        mutable_rule->last_triggered_ms = esp_timer_get_time() / 1000;

        /* Execute action */
        execute_action(rule);
        as_telemetry_increment_rule_triggers();
    }
}

void as_rule_engine_tick(void) {
    if (!s_initialized) return;
    for (size_t i = 0; i < s_rule_count; i++) {
        evaluate_rule(&s_rules[i], false);
    }
}

void as_rule_engine_evaluate_now(const char *rule_name) {
    if (!s_initialized || !rule_name) return;
    for (size_t i = 0; i < s_rule_count; i++) {
        if (strcmp(s_rules[i].name, rule_name) == 0) {
            evaluate_rule(&s_rules[i], true);
            return;
        }
    }
    ESP_LOGW(TAG, "Rule '%s' not found", rule_name);
}

int as_rule_set_enabled(const char *name, bool enabled) {
    if (!name) return -1;
    for (size_t i = 0; i < s_rule_count; i++) {
        if (strcmp(s_rules[i].name, name) == 0) {
            s_rules[i].enabled = enabled;
            ESP_LOGI(TAG, "Rule '%s' %s", name, enabled ? "enabled" : "disabled");
            return 0;
        }
    }
    return -1;
}

size_t as_rule_count(void) { return s_rule_count; }

const as_local_rule_t* as_rule_get(size_t index) {
    if (index >= s_rule_count) return NULL;
    return &s_rules[index];
}

cJSON* as_rule_list(void) {
    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (size_t i = 0; i < s_rule_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s_rules[i].name);
        cJSON_AddStringToObject(item, "sensor_tool", s_rules[i].sensor_tool);
        cJSON_AddBoolToObject(item, "enabled", s_rules[i].enabled);
        cJSON_AddNumberToObject(item, "threshold", s_rules[i].threshold);
        cJSON_AddNumberToObject(item, "cooldown_ms", s_rules[i].cooldown_ms);
        cJSON_AddNumberToObject(item, "last_triggered_ms", s_rules[i].last_triggered_ms);
        cJSON_AddItemToArray(array, item);
    }

    return array;
}
