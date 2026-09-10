/*
 * as_event.h -- Agent event system for streaming model output.
 *
 * Maps to agentscope-java: AgentEvent with 26 subtypes.
 *
 * Covers the full streaming lifecycle: agent start/end, model calls,
 * text/thinking/tool/data blocks (each with start/delta/end), and
 * control events (max iters, user confirm, external exec, stop).
 *
 * Events carry a UUID-like ID, millisecond timestamp, and optional
 * cJSON payload.  All strings point to .rodata; cJSON payloads are
 * owned by the caller.
 *
 * Design (D-002): Fixed-pool, no malloc for event generation.
 * cJSON payloads use libcjson heap (unavoidable for JSON).
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Event type enumeration (26 types + sentinel)
 * -------------------------------------------------------------------------- */

typedef enum {
    /* Agent lifecycle */
    AS_EVT_AGENT_START,              /**< Agent begins processing a turn */
    AS_EVT_AGENT_END,                /**< Agent finishes a turn */

    /* Model call */
    AS_EVT_MODEL_CALL_START,         /**< LLM request is about to be sent */
    AS_EVT_MODEL_CALL_END,           /**< LLM response has been received */

    /* Text streaming */
    AS_EVT_TEXT_BLOCK_START,         /**< A new text block begins */
    AS_EVT_TEXT_BLOCK_DELTA,         /**< Incremental text content */
    AS_EVT_TEXT_BLOCK_END,           /**< Text block is complete */

    /* Thinking (extended thinking models) */
    AS_EVT_THINKING_START,           /**< Thinking block begins */
    AS_EVT_THINKING_DELTA,           /**< Incremental thinking content */
    AS_EVT_THINKING_END,             /**< Thinking block is complete */

    /* Tool calls */
    AS_EVT_TOOL_CALL_START,          /**< Tool invocation begins */
    AS_EVT_TOOL_CALL_DELTA,          /**< Incremental tool call args */
    AS_EVT_TOOL_CALL_END,            /**< Tool call args complete */

    /* Tool results */
    AS_EVT_TOOL_RESULT_START,        /**< Tool result delivery begins */
    AS_EVT_TOOL_RESULT_TEXT_DELTA,   /**< Text portion of tool result */
    AS_EVT_TOOL_RESULT_DATA_DELTA,   /**< Data/binary portion of tool result */
    AS_EVT_TOOL_RESULT_END,          /**< Tool result delivery complete */

    /* Data blocks (images, files, structured data) */
    AS_EVT_DATA_BLOCK_START,         /**< Data block begins */
    AS_EVT_DATA_BLOCK_DELTA,         /**< Incremental data content */
    AS_EVT_DATA_BLOCK_END,           /**< Data block is complete */

    /* Control events */
    AS_EVT_EXCEED_MAX_ITERS,         /**< Agent hit iteration limit */
    AS_EVT_REQUIRE_USER_CONFIRM,     /**< Agent needs user confirmation */
    AS_EVT_REQUIRE_EXTERNAL_EXEC,    /**< Agent needs external execution */
    AS_EVT_USER_CONFIRM_RESULT,      /**< User confirmation response */
    AS_EVT_EXTERNAL_EXEC_RESULT,     /**< External execution result */
    AS_EVT_REQUEST_STOP,             /**< Agent requests stop */

    AS_EVT_COUNT,                    /**< Sentinel: number of event types */
} as_event_type_t;

/* --------------------------------------------------------------------------
 * Event struct
 * -------------------------------------------------------------------------- */

/**
 * An agent event.
 *
 * The `data` field holds event-specific JSON payload.  It is NULL
 * for events that carry no payload (e.g., AS_EVT_AGENT_END).
 * The caller owns the lifecycle of `data` -- as_event_free() will
 * cJSON_Delete it.
 */
typedef struct {
    as_event_type_t type;
    char id[37];           /**< UUID-like string (counter-ts, no heap) */
    int64_t timestamp_ms;  /**< Milliseconds since boot */
    cJSON *data;           /**< Event-specific payload (may be NULL) */
} as_agent_event_t;

/* --------------------------------------------------------------------------
 * Event factory functions (fill and return on the stack)
 * -------------------------------------------------------------------------- */

/**
 * Create an AGENT_START event.
 * @param reply_id  Session/reply identifier
 */
as_agent_event_t as_event_agent_start(const char *agent_name,
                                      const char *reply_id);

/** Create an AGENT_END event. */
as_agent_event_t as_event_agent_end(const char *reply_id);

/** Create a TEXT_BLOCK_DELTA event with incremental text. */
as_agent_event_t as_event_text_delta(const char *reply_id,
                                     const char *text);

/** Create a THINKING_DELTA event with incremental thinking content. */
as_agent_event_t as_event_thinking_delta(const char *reply_id,
                                         const char *text);

/** Create a TOOL_CALL_START event. */
as_agent_event_t as_event_tool_call_start(const char *tool_id,
                                          const char *tool_name);

/** Create a TOOL_CALL_END event. */
as_agent_event_t as_event_tool_call_end(const char *tool_id);

/** Create a TOOL_RESULT_START event. */
as_agent_event_t as_event_tool_result_start(const char *tool_id);

/** Create a TOOL_RESULT_END event. */
as_agent_event_t as_event_tool_result_end(const char *tool_id);

/** Create a MODEL_CALL_START event. */
as_agent_event_t as_event_model_call_start(const char *model_name);

/** Create a MODEL_CALL_END event. */
as_agent_event_t as_event_model_call_end(const char *model_name,
                                         int64_t latency_ms);

/** Create a REQUEST_STOP event. */
as_agent_event_t as_event_request_stop(const char *reason);

/** Create an EXCEED_MAX_ITERS event. */
as_agent_event_t as_event_exceed_max_iters(int max_iters);

/** Create a REQUIRE_USER_CONFIRM event. */
as_agent_event_t as_event_require_user_confirm(const char *prompt);

/** Create a USER_CONFIRM_RESULT event. */
as_agent_event_t as_event_user_confirm_result(bool confirmed);

/** Create a REQUIRE_EXTERNAL_EXEC event. */
as_agent_event_t as_event_require_external_exec(const char *command);

/** Create an EXTERNAL_EXEC_RESULT event. */
as_agent_event_t as_event_external_exec_result(const char *command,
                                               int exit_code,
                                               const char *output);

/* --------------------------------------------------------------------------
 * Generic factory (for custom payloads)
 * -------------------------------------------------------------------------- */

/**
 * Create an event of arbitrary type with a custom payload.
 * The event takes ownership of `data` -- do not cJSON_Delete after calling.
 * Pass NULL for data if the event carries no payload.
 */
as_agent_event_t as_event_create(as_event_type_t type,
                                 cJSON *data);

/* --------------------------------------------------------------------------
 * Serialization
 * -------------------------------------------------------------------------- */

/**
 * Serialize event to JSON.
 * Format: { "type": "<name>", "id": "...", "ts": N, "data": {...} }
 * Caller must cJSON_Delete().
 */
cJSON* as_event_to_json(const as_agent_event_t *event);

/**
 * Get human-readable name for an event type.
 * Returns a static string; never returns NULL.
 */
const char* as_event_type_name(as_event_type_t type);

/* --------------------------------------------------------------------------
 * Cleanup
 * -------------------------------------------------------------------------- */

/**
 * Free the cJSON payload inside an event.
 * Does NOT free the event struct itself (events live on the stack).
 */
void as_event_free(as_agent_event_t *event);

#ifdef __cplusplus
}
#endif
