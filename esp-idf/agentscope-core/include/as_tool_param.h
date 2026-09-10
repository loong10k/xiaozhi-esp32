/*
 * as_tool_param.h -- Tool parameter definition.
 *
 * Maps to agentscope-java: @ToolParam annotation + ToolSchema parameter objects.
 * Each parameter describes one input to a tool (name, type, description, constraints).
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Parameter data types. */
typedef enum {
    AS_PARAM_BOOLEAN,
    AS_PARAM_INTEGER,
    AS_PARAM_STRING,
} as_param_type_t;

/**
 * Tool parameter definition.
 *
 * Corresponds to agentscope-java ToolSchema.Parameter / @ToolParam.
 * All string fields point to compile-time .rodata (no ownership).
 */
typedef struct {
    const char *name;           /**< Parameter name (e.g., "location") */
    const char *type;           /**< JSON Schema type: "boolean", "integer", "string" */
    const char *description;    /**< Human-readable description for LLM */
    bool required;              /**< Whether this parameter is mandatory */

    /* Default value (union, valid when has_default == true) */
    bool has_default;
    union {
        bool bool_default;
        int int_default;
        const char *str_default;
    };

    /* Integer range constraints (valid when has_range == true) */
    bool has_range;
    int min_value;
    int max_value;
} as_tool_param_t;

/**
 * Generate JSON Schema for a single parameter.
 * Caller must cJSON_Delete() the returned object.
 */
cJSON* as_tool_param_to_json(const as_tool_param_t *param);

/**
 * Generate JSON Schema "properties" object for an array of parameters.
 * Caller must cJSON_Delete() the returned object.
 */
cJSON* as_tool_params_to_json(const as_tool_param_t *params, size_t count);

/**
 * Generate JSON Schema "required" array for parameters where required == true.
 * Caller must cJSON_Delete() the returned object.
 */
cJSON* as_tool_params_required_array(const as_tool_param_t *params, size_t count);

#ifdef __cplusplus
}
#endif
