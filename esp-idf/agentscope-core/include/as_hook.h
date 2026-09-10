/*
 * as_hook.h -- Hook system for tool execution interception.
 *
 * Maps to agentscope-java:
 *   - Hook (onPreCall/onPostCall) → AS_HOOK_PRE_EXEC / AS_HOOK_POST_EXEC
 *   - MiddlewareBase.onActing()   → AS_HOOK_PRE_EXEC / AS_HOOK_POST_EXEC
 *   - MetricsHook                → AS_HOOK_ON_TELEMETRY
 *   - MiddlewareChain             → internal chain execution
 *
 * Simplified from Java's 5 interception points to 3:
 *   PRE_EXEC (before tool), POST_EXEC (after tool), ON_TELEMETRY (data event)
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Hook interception points. */
typedef enum {
    AS_HOOK_PRE_EXEC,      /**< Before tool execution (param validation, permission) */
    AS_HOOK_POST_EXEC,     /**< After tool execution (result filtering, logging) */
    AS_HOOK_ON_TELEMETRY,  /**< When telemetry data is generated */
    AS_HOOK_COUNT,
} as_hook_point_t;

/** Hook chain execution result. */
typedef enum {
    AS_HOOK_CONTINUE,      /**< Continue with normal execution */
    AS_HOOK_SKIP,          /**< Skip tool execution entirely */
    AS_HOOK_MODIFIED,      /**< Data has been modified (use modified_data) */
} as_hook_result_t;

/** Response from a hook execution. */
typedef struct {
    as_hook_result_t result;
    cJSON *modified_data;  /**< If result == AS_HOOK_MODIFIED, replaces original data */
} as_hook_response_t;

/**
 * Hook function signature.
 *
 * @param tool_name  Name of the tool being intercepted
 * @param data       Input data (params for PRE_EXEC, result for POST_EXEC)
 * @param user_data  Opaque pointer from registration
 * @return Hook response indicating how to proceed
 */
typedef as_hook_response_t (*as_hook_fn_t)(
    const char *tool_name,
    const cJSON *data,
    void *user_data
);

/* === Lifecycle === */

/** Initialize the hook system. Clears all hooks. */
void as_hook_init(void);

/** Deinitialize the hook system. */
void as_hook_deinit(void);

/* === Registration === */

/**
 * Register a hook at the specified interception point.
 * Hooks are executed in priority order (lower number = higher priority).
 *
 * @param point     Interception point
 * @param priority  Execution priority (lower = earlier)
 * @param fn        Hook function
 * @param user_data Opaque pointer passed to fn
 * @return 0 on success, -1 if hook table is full
 */
int as_hook_register(as_hook_point_t point, int priority,
                     as_hook_fn_t fn, void *user_data);

/**
 * Unregister a specific hook.
 * @return 0 on success, -1 if not found
 */
int as_hook_unregister(as_hook_point_t point, as_hook_fn_t fn);

/** Clear all hooks at a specific point. */
void as_hook_clear(as_hook_point_t point);

/** Clear all hooks at all points. */
void as_hook_clear_all(void);

/* === Execution === */

/**
 * Execute the hook chain for a given interception point.
 * Chains all registered hooks in priority order.
 *
 * @param point      Interception point
 * @param tool_name  Tool being intercepted
 * @param data       Input data
 * @return Final hook response after chain execution
 */
as_hook_response_t as_hook_execute(as_hook_point_t point,
                                   const char *tool_name,
                                   const cJSON *data);

#ifdef __cplusplus
}
#endif
