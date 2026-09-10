/*
 * as_mcp_bridge.cc -- Bridge agentscope-core tools into xiaozhi's McpServer.
 *
 * Maps to agentscope-java: Tool injection into McpServer.
 * Design (D-003): Bridge, don't replace.
 * Design (D-006): C++ file for calling xiaozhi C++ APIs.
 */
#include "as_mcp_bridge.h"
#include "as_tool.h"
#include "as_hook.h"
#include "as_telemetry.h"

#include <esp_log.h>

static const char *TAG = "as_bridge";

/* Conditional xiaozhi integration */
#if __has_include("mcp_server.h")
#include "mcp_server.h"
#define HAS_XIAOZHI_MCP 1
#else
#define HAS_XIAOZHI_MCP 0
#endif

static bool s_initialized = false;
static bool s_connected = false;

#if HAS_XIAOZHI_MCP

/* Bridge context -- maps as_tool_fn_t to xiaozhi McpServer callback */
struct BridgeCtx {
    as_tool_fn_t execute;
    void *tool_user_data;
    const char *tool_name;
};

/* Convert xiaozhi PropertyList params to cJSON */
static cJSON* propertylist_to_cjson(const PropertyList& params) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    /* xiaozhi PropertyList::begin() 无 const 重载：桥接层只读遍历，
     * 回调传入的原对象本身非 const，const_cast 安全。 */
    auto& pl = const_cast<PropertyList&>(params);
    for (auto it = pl.begin(); it != pl.end(); ++it) {
        const Property& prop = *it;
        switch (prop.type()) {
            case kPropertyTypeBoolean:
                cJSON_AddBoolToObject(json, prop.name().c_str(), prop.value<bool>());
                break;
            case kPropertyTypeInteger:
                cJSON_AddNumberToObject(json, prop.name().c_str(), prop.value<int>());
                break;
            case kPropertyTypeString:
                cJSON_AddStringToObject(json, prop.name().c_str(), prop.value<std::string>().c_str());
                break;
        }
    }
    return json;
}

/* Convert as_tool_param_type to xiaozhi PropertyType */
static PropertyType param_type_to_property_type(const char *type) {
    if (!type) return kPropertyTypeString;
    if (strcmp(type, "boolean") == 0) return kPropertyTypeBoolean;
    if (strcmp(type, "integer") == 0) return kPropertyTypeInteger;
    return kPropertyTypeString;
}

/* Bridge callback: xiaozhi McpServer → as_tool_fn_t with hooks */
static ReturnValue bridge_callback(const PropertyList& params, BridgeCtx *ctx) {
    /* Convert params to cJSON */
    cJSON *params_json = propertylist_to_cjson(params);

    /* Pre-hook */
    as_hook_response_t pre = as_hook_execute(AS_HOOK_PRE_EXEC, ctx->tool_name, params_json);
    if (pre.result == AS_HOOK_SKIP) {
        cJSON_Delete(params_json);
        return std::string("Skipped by hook");
    }

    const cJSON *actual = (pre.result == AS_HOOK_MODIFIED && pre.modified_data)
                           ? pre.modified_data : params_json;

    /* Execute tool */
    cJSON *result = ctx->execute(actual, ctx->tool_user_data);

    /* Clean up pre-hook */
    if (pre.result == AS_HOOK_MODIFIED && pre.modified_data) {
        cJSON_Delete(pre.modified_data);
    }
    cJSON_Delete(params_json);

    if (!result) {
        return std::string("Tool execution failed");
    }

    /* Post-hook */
    as_hook_response_t post = as_hook_execute(AS_HOOK_POST_EXEC, ctx->tool_name, result);
    cJSON *final_result = (post.result == AS_HOOK_MODIFIED && post.modified_data)
                          ? post.modified_data : result;

    /* Telemetry */
    as_telemetry_update_tool_call(ctx->tool_name, final_result);

    /* Convert result to string */
    char *json_str = cJSON_PrintUnformatted(final_result);
    std::string ret(json_str ? json_str : "{}");
    cJSON_free(json_str);

    if (final_result != result) cJSON_Delete(final_result);
    cJSON_Delete(result);

    return ret;
}

void as_mcp_bridge_init(void) {
    s_initialized = true;
    s_connected = false;
    ESP_LOGI(TAG, "MCP bridge initialized (xiaozhi McpServer available)");
}

void as_mcp_bridge_deinit(void) {
    s_initialized = false;
    s_connected = false;
}

void as_mcp_bridge_register_all(void) {
    if (!s_initialized) {
        ESP_LOGW(TAG, "Bridge not initialized");
        return;
    }

    auto& server = McpServer::GetInstance();
    size_t count = as_tool_count();

    for (size_t i = 0; i < count; i++) {
        const as_tool_entry_t *t = as_tool_get(i);
        if (!t) continue;

        /* Convert as_tool_param_t → xiaozhi PropertyList */
        PropertyList props;
        if (t->params && t->param_count > 0) {
            for (size_t p = 0; p < t->param_count; p++) {
                const as_tool_param_t *param = &t->params[p];
                PropertyType pt = param_type_to_property_type(param->type);

                if (param->has_default) {
                    if (pt == kPropertyTypeBoolean) {
                        props.AddProperty(Property(param->name, pt, param->bool_default));
                    } else if (pt == kPropertyTypeInteger) {
                        if (param->has_range) {
                            props.AddProperty(Property(param->name, pt, param->int_default,
                                                       param->min_value, param->max_value));
                        } else {
                            props.AddProperty(Property(param->name, pt, param->int_default));
                        }
                    } else {
                        props.AddProperty(Property(param->name, pt,
                                                   std::string(param->str_default ? param->str_default : "")));
                    }
                } else {
                    if (pt == kPropertyTypeInteger && param->has_range) {
                        props.AddProperty(Property(param->name, pt, param->min_value, param->max_value));
                    } else {
                        props.AddProperty(Property(param->name, pt));
                    }
                }
            }
        }

        /* Create bridge callback */
        auto *ctx = new BridgeCtx{t->execute, t->user_data, t->name};

        /* Register with appropriate method based on visibility */
        if (t->visibility == AS_TOOL_VISIBILITY_USER) {
            server.AddUserOnlyTool(t->name, t->description, props,
                [ctx](const PropertyList& p) -> ReturnValue {
                    return bridge_callback(p, ctx);
                });
        } else {
            server.AddTool(t->name, t->description, props,
                [ctx](const PropertyList& p) -> ReturnValue {
                    return bridge_callback(p, ctx);
                });
        }
    }

    s_connected = true;
    ESP_LOGI(TAG, "Registered %d tools with xiaozhi McpServer", (int)count);
}

int as_mcp_bridge_is_connected(void) {
    return s_connected ? 1 : 0;
}

#else /* No xiaozhi McpServer available -- standalone mode */

void as_mcp_bridge_init(void) {
    s_initialized = true;
    s_connected = false;
    ESP_LOGI(TAG, "MCP bridge initialized (standalone mode, no xiaozhi)");
}

void as_mcp_bridge_deinit(void) {
    s_initialized = false;
}

void as_mcp_bridge_register_all(void) {
    ESP_LOGW(TAG, "No xiaozhi McpServer available, bridge is no-op");
}

int as_mcp_bridge_is_connected(void) {
    return 0;
}

#endif /* HAS_XIAOZHI_MCP */
