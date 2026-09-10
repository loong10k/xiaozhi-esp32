/*
 * as_event.c -- Agent event system implementation.
 *
 * Maps to agentscope-java: AgentEvent with 26 subtypes.
 *
 * Generates UUID-like IDs from a monotonic counter + timestamp
 * (no heap allocation for the ID).  Uses esp_timer for millisecond
 * timestamps.  cJSON payloads use libcjson heap (unavoidable).
 */
#include "as_event.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "as_event";

/* Monotonic counter for unique IDs (wraps at 2^32) */
static uint32_t s_event_counter = 0;

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/**
 * Fill common fields: type, id, timestamp.
 * Does NOT touch data (caller sets that).
 */
static void fill_common(as_agent_event_t *evt, as_event_type_t type) {
    evt->type = type;
    evt->timestamp_ms = esp_timer_get_time() / 1000;

    /* Generate a simple ID: "<counter>-<ts_low32>"
     * Not a real UUID, but unique enough for embedded event ordering. */
    uint32_t c = s_event_counter++;
    uint32_t ts_lo = (uint32_t)(evt->timestamp_ms & 0xFFFFFFFF);
    snprintf(evt->id, sizeof(evt->id), "%08x-%08x", (unsigned)c, (unsigned)ts_lo);

    evt->data = NULL;
}

/**
 * Helper: create an event with a cJSON object as payload.
 * Takes ownership of the object (stores it, caller must NOT free).
 */
static as_agent_event_t make_event_with_object(as_event_type_t type,
                                               cJSON *obj) {
    as_agent_event_t evt;
    fill_common(&evt, type);
    evt.data = obj;
    return evt;
}

/**
 * Helper: create a simple event with "key" : "value" string payload.
 */
static as_agent_event_t make_event_kv_str(as_event_type_t type,
                                          const char *key,
                                          const char *value) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, type);
        ESP_LOGW(TAG, "cJSON alloc failed for event %s", as_event_type_name(type));
        return evt;
    }
    cJSON_AddStringToObject(obj, key, value ? value : "");
    return make_event_with_object(type, obj);
}

/* --------------------------------------------------------------------------
 * Public event factory functions
 * -------------------------------------------------------------------------- */

as_agent_event_t as_event_agent_start(const char *agent_name,
                                      const char *reply_id) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_AGENT_START);
        return evt;
    }
    cJSON_AddStringToObject(obj, "agent", agent_name ? agent_name : "");
    cJSON_AddStringToObject(obj, "reply_id", reply_id ? reply_id : "");
    return make_event_with_object(AS_EVT_AGENT_START, obj);
}

as_agent_event_t as_event_agent_end(const char *reply_id) {
    return make_event_kv_str(AS_EVT_AGENT_END, "reply_id",
                             reply_id ? reply_id : "");
}

as_agent_event_t as_event_text_delta(const char *reply_id,
                                     const char *text) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_TEXT_BLOCK_DELTA);
        return evt;
    }
    cJSON_AddStringToObject(obj, "reply_id", reply_id ? reply_id : "");
    cJSON_AddStringToObject(obj, "text", text ? text : "");
    return make_event_with_object(AS_EVT_TEXT_BLOCK_DELTA, obj);
}

as_agent_event_t as_event_thinking_delta(const char *reply_id,
                                         const char *text) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_THINKING_DELTA);
        return evt;
    }
    cJSON_AddStringToObject(obj, "reply_id", reply_id ? reply_id : "");
    cJSON_AddStringToObject(obj, "text", text ? text : "");
    return make_event_with_object(AS_EVT_THINKING_DELTA, obj);
}

as_agent_event_t as_event_tool_call_start(const char *tool_id,
                                          const char *tool_name) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_TOOL_CALL_START);
        return evt;
    }
    cJSON_AddStringToObject(obj, "tool_id", tool_id ? tool_id : "");
    cJSON_AddStringToObject(obj, "tool_name", tool_name ? tool_name : "");
    return make_event_with_object(AS_EVT_TOOL_CALL_START, obj);
}

as_agent_event_t as_event_tool_call_end(const char *tool_id) {
    return make_event_kv_str(AS_EVT_TOOL_CALL_END, "tool_id",
                             tool_id ? tool_id : "");
}

as_agent_event_t as_event_tool_result_start(const char *tool_id) {
    return make_event_kv_str(AS_EVT_TOOL_RESULT_START, "tool_id",
                             tool_id ? tool_id : "");
}

as_agent_event_t as_event_tool_result_end(const char *tool_id) {
    return make_event_kv_str(AS_EVT_TOOL_RESULT_END, "tool_id",
                             tool_id ? tool_id : "");
}

as_agent_event_t as_event_model_call_start(const char *model_name) {
    return make_event_kv_str(AS_EVT_MODEL_CALL_START, "model",
                             model_name ? model_name : "");
}

as_agent_event_t as_event_model_call_end(const char *model_name,
                                         int64_t latency_ms) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_MODEL_CALL_END);
        return evt;
    }
    cJSON_AddStringToObject(obj, "model", model_name ? model_name : "");
    cJSON_AddNumberToObject(obj, "latency_ms", (double)latency_ms);
    return make_event_with_object(AS_EVT_MODEL_CALL_END, obj);
}

as_agent_event_t as_event_request_stop(const char *reason) {
    return make_event_kv_str(AS_EVT_REQUEST_STOP, "reason",
                             reason ? reason : "");
}

as_agent_event_t as_event_exceed_max_iters(int max_iters) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_EXCEED_MAX_ITERS);
        return evt;
    }
    cJSON_AddNumberToObject(obj, "max_iters", max_iters);
    return make_event_with_object(AS_EVT_EXCEED_MAX_ITERS, obj);
}

as_agent_event_t as_event_require_user_confirm(const char *prompt) {
    return make_event_kv_str(AS_EVT_REQUIRE_USER_CONFIRM, "prompt",
                             prompt ? prompt : "");
}

as_agent_event_t as_event_user_confirm_result(bool confirmed) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_USER_CONFIRM_RESULT);
        return evt;
    }
    cJSON_AddBoolToObject(obj, "confirmed", confirmed);
    return make_event_with_object(AS_EVT_USER_CONFIRM_RESULT, obj);
}

as_agent_event_t as_event_require_external_exec(const char *command) {
    return make_event_kv_str(AS_EVT_REQUIRE_EXTERNAL_EXEC, "command",
                             command ? command : "");
}

as_agent_event_t as_event_external_exec_result(const char *command,
                                               int exit_code,
                                               const char *output) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        as_agent_event_t evt;
        fill_common(&evt, AS_EVT_EXTERNAL_EXEC_RESULT);
        return evt;
    }
    cJSON_AddStringToObject(obj, "command", command ? command : "");
    cJSON_AddNumberToObject(obj, "exit_code", exit_code);
    cJSON_AddStringToObject(obj, "output", output ? output : "");
    return make_event_with_object(AS_EVT_EXTERNAL_EXEC_RESULT, obj);
}

/* --------------------------------------------------------------------------
 * Generic factory
 * -------------------------------------------------------------------------- */

as_agent_event_t as_event_create(as_event_type_t type,
                                 cJSON *data) {
    as_agent_event_t evt;
    fill_common(&evt, type);
    evt.data = data;  /* Transfer ownership */
    return evt;
}

/* --------------------------------------------------------------------------
 * Serialization
 * -------------------------------------------------------------------------- */

/**
 * Type name table -- indexed by as_event_type_t.
 * Must match the enum order exactly.
 */
static const char *const s_type_names[AS_EVT_COUNT] = {
    [AS_EVT_AGENT_START]             = "agent_start",
    [AS_EVT_AGENT_END]               = "agent_end",
    [AS_EVT_MODEL_CALL_START]        = "model_call_start",
    [AS_EVT_MODEL_CALL_END]          = "model_call_end",
    [AS_EVT_TEXT_BLOCK_START]        = "text_block_start",
    [AS_EVT_TEXT_BLOCK_DELTA]        = "text_block_delta",
    [AS_EVT_TEXT_BLOCK_END]          = "text_block_end",
    [AS_EVT_THINKING_START]          = "thinking_start",
    [AS_EVT_THINKING_DELTA]          = "thinking_delta",
    [AS_EVT_THINKING_END]            = "thinking_end",
    [AS_EVT_TOOL_CALL_START]         = "tool_call_start",
    [AS_EVT_TOOL_CALL_DELTA]         = "tool_call_delta",
    [AS_EVT_TOOL_CALL_END]           = "tool_call_end",
    [AS_EVT_TOOL_RESULT_START]       = "tool_result_start",
    [AS_EVT_TOOL_RESULT_TEXT_DELTA]  = "tool_result_text_delta",
    [AS_EVT_TOOL_RESULT_DATA_DELTA]  = "tool_result_data_delta",
    [AS_EVT_TOOL_RESULT_END]         = "tool_result_end",
    [AS_EVT_DATA_BLOCK_START]        = "data_block_start",
    [AS_EVT_DATA_BLOCK_DELTA]        = "data_block_delta",
    [AS_EVT_DATA_BLOCK_END]          = "data_block_end",
    [AS_EVT_EXCEED_MAX_ITERS]        = "exceed_max_iters",
    [AS_EVT_REQUIRE_USER_CONFIRM]    = "require_user_confirm",
    [AS_EVT_REQUIRE_EXTERNAL_EXEC]   = "require_external_exec",
    [AS_EVT_USER_CONFIRM_RESULT]     = "user_confirm_result",
    [AS_EVT_EXTERNAL_EXEC_RESULT]    = "external_exec_result",
    [AS_EVT_REQUEST_STOP]            = "request_stop",
};

const char* as_event_type_name(as_event_type_t type) {
    if (type >= 0 && type < AS_EVT_COUNT) {
        return s_type_names[type];
    }
    return "unknown";
}

cJSON* as_event_to_json(const as_agent_event_t *event) {
    if (!event) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    cJSON_AddStringToObject(json, "type", as_event_type_name(event->type));
    cJSON_AddStringToObject(json, "id", event->id);
    cJSON_AddNumberToObject(json, "ts", (double)event->timestamp_ms);

    if (event->data) {
        /* Add data as a nested object (shallow duplicate so caller retains
         * ownership of the original) */
        cJSON *dup = cJSON_Duplicate(event->data, 1);
        if (dup) {
            cJSON_AddItemToObject(json, "data", dup);
        }
    }

    return json;
}

/* --------------------------------------------------------------------------
 * Cleanup
 * -------------------------------------------------------------------------- */

void as_event_free(as_agent_event_t *event) {
    if (!event) return;
    if (event->data) {
        cJSON_Delete(event->data);
        event->data = NULL;
    }
}
