/*
 * as_rule.h -- Local rule engine for edge-side fast response.
 *
 * Maps to agentscope-java:
 *   - MiddlewareBase emitting RequestStopEvent → as_rule_engine_tick()
 *   - No direct Java equivalent -- this is an embedded-specific addition
 *     for scenarios where cloud round-trip is too slow (e.g., safety cutoff).
 *
 * Rules evaluate sensor data against thresholds and trigger local actions
 * without cloud involvement. Millisecond-level response time.
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Comparison operators for rule conditions. */
typedef enum {
    AS_COND_GT,       /**< Greater than */
    AS_COND_LT,       /**< Less than */
    AS_COND_EQ,       /**< Equal to */
    AS_COND_GTE,      /**< Greater than or equal */
    AS_COND_LTE,      /**< Less than or equal */
    AS_COND_BETWEEN,  /**< Between threshold and threshold2 (inclusive) */
} as_cond_op_t;

/** Action types triggered by rule evaluation. */
typedef enum {
    AS_ACT_SET_GPIO,       /**< Directly set a GPIO pin */
    AS_ACT_INVOKE_TOOL,    /**< Call a registered tool */
    AS_ACT_NOTIFY_CLOUD,   /**< Send event to cloud agent */
    AS_ACT_ALERT,          /**< Local alert (buzzer/LED pattern) */
} as_action_type_t;

/**
 * Local rule definition.
 *
 * Maps to agentscope-java: a simplified version of Middleware intercepting
 * acting phase and emitting RequestStopEvent.
 */
typedef struct {
    const char *name;               /**< Rule name (e.g., "overheat_protection") */
    const char *sensor_tool;        /**< Tool name to read sensor (e.g., "read_temperature") */
    as_cond_op_t op;                /**< Comparison operator */
    float threshold;                /**< Primary threshold value */
    float threshold2;               /**< Secondary threshold (for BETWEEN) */
    const char *value_field;        /**< JSON field name in tool result (e.g., "temperature_c") */

    as_action_type_t action_type;   /**< What to do when condition is met */
    union {
        struct { int gpio_num; int level; } gpio;       /**< GPIO action */
        struct { const char *tool; cJSON *params; } invoke; /**< Tool invocation */
        struct { const char *event; cJSON *data; } notify;  /**< Cloud notification */
        struct { int led_pattern; } alert;              /**< Local alert pattern */
    } action;

    uint32_t cooldown_ms;           /**< Minimum interval between triggers (default 5000) */
    uint32_t last_triggered_ms;     /**< Timestamp of last trigger (internal) */
    bool enabled;                   /**< Whether this rule is active */
} as_local_rule_t;

/* === Lifecycle === */

/** Initialize the rule engine. */
void as_rule_engine_init(void);

/** Deinitialize the rule engine. */
void as_rule_engine_deinit(void);

/* === Registration === */

/**
 * Register a rule. Returns index on success, -1 if full.
 * The rule struct is copied; the caller retains ownership of string pointers.
 */
int as_rule_add(const as_local_rule_t *rule);

/* === Evaluation === */

/**
 * Evaluate all enabled rules.
 * For each rule: calls the sensor_tool, checks condition, triggers action.
 * Respects cooldown_ms between triggers.
 *
 * Should be called periodically (every CONFIG_AGENTSCOPE_CORE_RULE_TICK_MS).
 */
void as_rule_engine_tick(void);

/**
 * Immediately evaluate a specific rule by name, ignoring cooldown.
 * Useful for emergency/safety scenarios.
 */
void as_rule_engine_evaluate_now(const char *rule_name);

/* === Management === */

/** Enable or disable a rule by name. Returns 0 on success. */
int as_rule_set_enabled(const char *name, bool enabled);

/** Get rule count. */
size_t as_rule_count(void);

/** Get rule by index. Returns NULL if out of bounds. */
const as_local_rule_t* as_rule_get(size_t index);

/**
 * List all rules as a JSON array.
 * Each element: { "name", "sensor_tool", "enabled", "last_triggered_ms" }
 * Caller must cJSON_Delete().
 */
cJSON* as_rule_list(void);

#ifdef __cplusplus
}
#endif
