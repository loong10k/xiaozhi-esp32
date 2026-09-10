/*
 * as_tool_param.c -- Tool parameter JSON Schema generation.
 *
 * Maps to agentscope-java: ToolSchema.Parameter serialization.
 */
#include "as_tool_param.h"
#include <string.h>

cJSON* as_tool_param_to_json(const as_tool_param_t *param) {
    if (!param) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    /* Type */
    if (param->type) {
        cJSON_AddStringToObject(json, "type", param->type);
    }

    /* Description */
    if (param->description) {
        cJSON_AddStringToObject(json, "description", param->description);
    }

    /* Default value */
    if (param->has_default) {
        if (strcmp(param->type, "boolean") == 0) {
            cJSON_AddBoolToObject(json, "default", param->bool_default);
        } else if (strcmp(param->type, "integer") == 0) {
            cJSON_AddNumberToObject(json, "default", param->int_default);
        } else if (strcmp(param->type, "string") == 0 && param->str_default) {
            cJSON_AddStringToObject(json, "default", param->str_default);
        }
    }

    /* Range constraints for integers */
    if (param->has_range && strcmp(param->type, "integer") == 0) {
        cJSON_AddNumberToObject(json, "minimum", param->min_value);
        cJSON_AddNumberToObject(json, "maximum", param->max_value);
    }

    return json;
}

cJSON* as_tool_params_to_json(const as_tool_param_t *params, size_t count) {
    if (!params || count == 0) return NULL;

    cJSON *properties = cJSON_CreateObject();
    if (!properties) return NULL;

    for (size_t i = 0; i < count; i++) {
        cJSON *prop = as_tool_param_to_json(&params[i]);
        if (prop) {
            cJSON_AddItemToObject(properties, params[i].name, prop);
        }
    }

    return properties;
}

cJSON* as_tool_params_required_array(const as_tool_param_t *params, size_t count) {
    if (!params || count == 0) return NULL;

    cJSON *required = cJSON_CreateArray();
    if (!required) return NULL;

    for (size_t i = 0; i < count; i++) {
        if (params[i].required) {
            cJSON_AddItemToArray(required, cJSON_CreateString(params[i].name));
        }
    }

    /* Return NULL if empty (no required params) */
    if (cJSON_GetArraySize(required) == 0) {
        cJSON_Delete(required);
        return NULL;
    }

    return required;
}
