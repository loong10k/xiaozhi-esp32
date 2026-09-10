/*
 * as_tool.c -- Tool registry implementation.
 *
 * Maps to agentscope-java: Toolkit / AgentTool / ToolEntry.
 * Uses compile-time fixed pool (D-002).
 */
#include "as_tool.h"
#include "as_telemetry.h"
#include "as_hook.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "as_tool";

/* Fixed-pool storage -- no malloc */
static as_tool_entry_t s_tools[CONFIG_AGENTSCOPE_CORE_MAX_TOOLS];
static size_t s_tool_count = 0;
static bool s_initialized = false;

void as_tool_registry_init(void) {
    memset(s_tools, 0, sizeof(s_tools));
    s_tool_count = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "Tool registry initialized (max %d tools)", CONFIG_AGENTSCOPE_CORE_MAX_TOOLS);
}

void as_tool_registry_deinit(void) {
    s_tool_count = 0;
    s_initialized = false;
    ESP_LOGI(TAG, "Tool registry deinitialized");
}

int as_tool_register(const as_tool_entry_t *tool) {
    if (!s_initialized || !tool || !tool->name || !tool->execute) {
        return -1;
    }

    /* Check for duplicate */
    for (size_t i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, tool->name) == 0) {
            ESP_LOGW(TAG, "Tool '%s' already registered, overwriting", tool->name);
            s_tools[i] = *tool;
            return (int)i;
        }
    }

    if (s_tool_count >= CONFIG_AGENTSCOPE_CORE_MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full (%d/%d), cannot register '%s'",
                 (int)s_tool_count, CONFIG_AGENTSCOPE_CORE_MAX_TOOLS, tool->name);
        return -1;
    }

    s_tools[s_tool_count] = *tool;
    ESP_LOGI(TAG, "Registered tool[%d]: '%s' (vis=%d, params=%d)",
             (int)s_tool_count, tool->name, tool->visibility, (int)tool->param_count);
    return (int)s_tool_count++;
}

size_t as_tool_count(void) {
    return s_tool_count;
}

const as_tool_entry_t* as_tool_get(size_t index) {
    if (index >= s_tool_count) return NULL;
    return &s_tools[index];
}

const as_tool_entry_t* as_tool_find(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            return &s_tools[i];
        }
    }
    return NULL;
}

const as_tool_entry_t* as_tool_registry_get_all(void) {
    return s_tools;
}

cJSON* as_tool_execute(const char *name, const cJSON *params) {
    const as_tool_entry_t *tool = as_tool_find(name);
    if (!tool) {
        ESP_LOGW(TAG, "Tool '%s' not found", name);
        return NULL;
    }

    /* Pre-execution hook */
    as_hook_response_t pre = as_hook_execute(AS_HOOK_PRE_EXEC, name, params);
    if (pre.result == AS_HOOK_SKIP) {
        ESP_LOGI(TAG, "Tool '%s' skipped by pre-hook", name);
        return NULL;
    }

    const cJSON *actual_params = (pre.result == AS_HOOK_MODIFIED && pre.modified_data)
                                  ? pre.modified_data : params;

    /* Execute tool */
    cJSON *result = tool->execute(actual_params, tool->user_data);

    /* Clean up pre-hook modified data */
    if (pre.result == AS_HOOK_MODIFIED && pre.modified_data) {
        cJSON_Delete(pre.modified_data);
    }

    /* Post-execution hook */
    if (result) {
        as_hook_response_t post = as_hook_execute(AS_HOOK_POST_EXEC, name, result);
        if (post.result == AS_HOOK_MODIFIED && post.modified_data) {
            cJSON_Delete(result);
            result = post.modified_data;
        }

        /* Update telemetry */
        as_telemetry_update_tool_call(name, result);
    }

    return result;
}

static cJSON* build_tool_schema(const as_tool_entry_t *tool) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "name", tool->name);
    cJSON_AddStringToObject(json, "description", tool->description);

    /* inputSchema */
    cJSON *input_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(input_schema, "type", "object");

    if (tool->params && tool->param_count > 0) {
        cJSON *properties = as_tool_params_to_json(tool->params, tool->param_count);
        if (properties) {
            cJSON_AddItemToObject(input_schema, "properties", properties);
        }

        cJSON *required = as_tool_params_required_array(tool->params, tool->param_count);
        if (required) {
            cJSON_AddItemToObject(input_schema, "required", required);
        }
    } else {
        cJSON_AddItemToObject(input_schema, "properties", cJSON_CreateObject());
    }

    cJSON_AddItemToObject(json, "inputSchema", input_schema);

    /* user-only annotation (maps to agentscope-java: McpTool.user_only) */
    if (tool->visibility == AS_TOOL_VISIBILITY_USER) {
        cJSON *annotations = cJSON_CreateObject();
        cJSON *audience = cJSON_CreateArray();
        cJSON_AddItemToArray(audience, cJSON_CreateString("user"));
        cJSON_AddItemToObject(annotations, "audience", audience);
        cJSON_AddItemToObject(json, "annotations", annotations);
    }

    return json;
}

cJSON* as_tool_registry_to_mcp_schema(void) {
    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (size_t i = 0; i < s_tool_count; i++) {
        cJSON *schema = build_tool_schema(&s_tools[i]);
        if (schema) {
            cJSON_AddItemToArray(array, schema);
        }
    }

    return array;
}

cJSON* as_tool_registry_to_mcp_schema_ai_only(void) {
    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (size_t i = 0; i < s_tool_count; i++) {
        if (s_tools[i].visibility & AS_TOOL_VISIBILITY_AI) {
            cJSON *schema = build_tool_schema(&s_tools[i]);
            if (schema) {
                cJSON_AddItemToArray(array, schema);
            }
        }
    }

    return array;
}
