/*
 * as_tool.h -- Tool registry for IoT agents.
 *
 * Maps to agentscope-java: AgentTool + Toolkit + @Tool.
 * Uses compile-time fixed pool, no heap allocation for registration.
 *
 * Design (D-002): Fixed pool, no malloc. Tool strings point to .rodata.
 */
#pragma once

#include "as_tool_param.h"
#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tool visibility -- who can see and invoke this tool.
 *
 * Maps to agentscope-java: Toolkit.registerUserOnlyTool() vs registerTool().
 */
typedef enum {
    AS_TOOL_VISIBILITY_AI   = 1 << 0,  /**< LLM can see and call */
    AS_TOOL_VISIBILITY_USER = 1 << 1,  /**< User UI can see */
    AS_TOOL_VISIBILITY_BOTH = 0x03,    /**< Both AI and user */
} as_tool_visibility_t;

/**
 * Tool execution function signature.
 *
 * @param params  Input parameters as cJSON object (owned by caller, do not free)
 * @param user_data  Opaque pointer from registration
 * @return Result as cJSON (caller must cJSON_Delete), or NULL on error
 */
typedef cJSON* (*as_tool_fn_t)(const cJSON *params, void *user_data);

/**
 * Tool registry entry.
 *
 * Maps to agentscope-java: ToolEntry / ToolSchema.
 * All string fields point to compile-time .rodata (no ownership).
 */
typedef struct {
    const char *name;                   /**< Tool name (e.g., "read_temperature") */
    const char *description;            /**< Description for LLM context */
    const as_tool_param_t *params;      /**< Parameter definitions (static array) */
    size_t param_count;                 /**< Number of parameters */
    as_tool_fn_t execute;               /**< Execution function */
    void *user_data;                    /**< Opaque user data passed to execute */
    as_tool_visibility_t visibility;    /**< Who can see/call this tool */
} as_tool_entry_t;

/**
 * Convenience macro for registering a tool with static parameters.
 *
 * Example:
 *   static const as_tool_param_t my_params[] = { ... };
 *   AS_TOOL_REGISTER("my_tool", "Does something", my_params, my_fn);
 */
#define AS_TOOL_REGISTER(name_val, desc_val, params_val, fn) \
    do { \
        static const as_tool_entry_t _entry = { \
            .name = (name_val), \
            .description = (desc_val), \
            .params = (params_val), \
            .param_count = sizeof(params_val) / sizeof(params_val[0]), \
            .execute = (fn), \
            .user_data = NULL, \
            .visibility = AS_TOOL_VISIBILITY_BOTH, \
        }; \
        as_tool_register(&_entry); \
    } while (0)

/**
 * Convenience macro for registering a parameterless tool.
 */
#define AS_TOOL_REGISTER_NO_PARAMS(name_val, desc_val, fn) \
    do { \
        static const as_tool_entry_t _entry = { \
            .name = (name_val), \
            .description = (desc_val), \
            .params = NULL, \
            .param_count = 0, \
            .execute = (fn), \
            .user_data = NULL, \
            .visibility = AS_TOOL_VISIBILITY_BOTH, \
        }; \
        as_tool_register(&_entry); \
    } while (0)

/* === Lifecycle === */

/** Initialize the tool registry. Clears all registered tools. */
void as_tool_registry_init(void);

/** Deinitialize the tool registry. */
void as_tool_registry_deinit(void);

/* === Registration === */

/**
 * Register a tool. Returns index on success, -1 if full.
 * Shallow copy -- entry strings must outlive the registry.
 */
int as_tool_register(const as_tool_entry_t *tool);

/* === Query === */

/** Get total number of registered tools. */
size_t as_tool_count(void);

/** Get tool entry by index. Returns NULL if out of bounds. */
const as_tool_entry_t* as_tool_get(size_t index);

/** Find tool by name. Returns NULL if not found. */
const as_tool_entry_t* as_tool_find(const char *name);

/** Get all registered tool entries. Returns pointer to internal array. */
const as_tool_entry_t* as_tool_registry_get_all(void);

/* === Execution === */

/**
 * Execute a tool by name.
 * Returns cJSON result (caller must cJSON_Delete), or NULL on error.
 */
cJSON* as_tool_execute(const char *name, const cJSON *params);

/* === Schema Generation === */

/**
 * Generate MCP-compatible tools/list JSON array.
 * Each element has "name", "description", "inputSchema".
 * Caller must cJSON_Delete() the returned array.
 */
cJSON* as_tool_registry_to_mcp_schema(void);

/**
 * Generate tools/list for AI-visible tools only.
 * Caller must cJSON_Delete() the returned array.
 */
cJSON* as_tool_registry_to_mcp_schema_ai_only(void);

#ifdef __cplusplus
}
#endif
