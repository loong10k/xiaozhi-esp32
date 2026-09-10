/*
 * as_skill.h -- Skill registry for grouping tools into capability sets.
 *
 * Maps to agentscope-java: AgentSkill + SkillRegistry.
 *
 * A "skill" is a named collection of tools with an optional system-prompt
 * instruction.  Skills can be activated/deactivated at runtime; only
 * active skills' tools are exposed to the LLM.
 *
 * Design (D-002): Fixed-pool, no malloc.  All string pointers in
 * as_skill_t point to .rodata (compile-time literals).
 *
 * The Core skill registry is compile-time sized.  Extensions can
 * provide a dynamic registry that allocates from PSRAM.
 */
#pragma once

#include "as_tool.h"
#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#ifndef CONFIG_AGENTSCOPE_CORE_MAX_SKILLS
#define CONFIG_AGENTSCOPE_CORE_MAX_SKILLS 8
#endif

#ifndef CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL
#define CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL 8
#endif

/* Maximum length of concatenated prompt from all active skills */
#ifndef CONFIG_AGENTSCOPE_CORE_SKILL_PROMPT_BUF_SIZE
#define CONFIG_AGENTSCOPE_CORE_SKILL_PROMPT_BUF_SIZE 512
#endif

/* --------------------------------------------------------------------------
 * Types
 * -------------------------------------------------------------------------- */

/**
 * A skill definition.
 *
 * All string fields point to compile-time .rodata (not owned by the
 * registry).  tool_names are strings that match as_tool_entry_t.name
 * values in the tool registry.
 */
typedef struct {
    const char *name;                                            /**< Skill name */
    const char *description;                                     /**< Human-readable description */
    const char *instructions;                                    /**< System prompt addition (may be NULL) */
    const char *tool_names[CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL]; /**< Tool name references */
    size_t tool_count;                                           /**< Number of tools in this skill */
    bool active;                                                 /**< Whether skill is currently active */
} as_skill_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/** Initialize the skill registry.  Clears all registered skills. */
void as_skill_registry_init(void);

/** Deinitialize the skill registry. */
void as_skill_registry_deinit(void);

/* --------------------------------------------------------------------------
 * Registration
 * -------------------------------------------------------------------------- */

/**
 * Register a skill.  Shallow copy -- all string pointers must outlive
 * the registry (i.e., point to .rodata or static storage).
 *
 * @param skill  Skill definition to register
 * @return 0 on success, -1 if registry is full or name is duplicate
 */
int as_skill_register(const as_skill_t *skill);

/**
 * Convenience macro for registering a skill at compile time.
 *
 * Example:
 *   static const char *my_tools[] = {"read_temp", "set_fan"};
 *   AS_SKILL_REGISTER("climate", "Climate control", "Monitor temperature.",
 *                      my_tools);
 */
#define AS_SKILL_REGISTER(name_val, desc_val, instr_val, tools_array) \
    do { \
        static const as_skill_t _skill = { \
            .name = (name_val), \
            .description = (desc_val), \
            .instructions = (instr_val), \
            .tool_count = sizeof(tools_array) / sizeof(tools_array[0]), \
            .active = true, \
            /* Copy tool name pointers into fixed array */ \
            /* NOTE: caller must ensure tools_array fits */ \
        }; \
        /* We use a compound literal to set tool_names at init time */ \
        as_skill_t _skill_mutable = _skill; \
        size_t _tc = _skill.tool_count > CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL \
                      ? CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL \
                      : _skill.tool_count; \
        for (size_t _i = 0; _i < _tc; _i++) { \
            _skill_mutable.tool_names[_i] = (tools_array)[_i]; \
        } \
        as_skill_register(&_skill_mutable); \
    } while (0)

/* --------------------------------------------------------------------------
 * Activation
 * -------------------------------------------------------------------------- */

/**
 * Activate a skill by name.
 * @return 0 on success, -1 if not found
 */
int as_skill_activate(const char *name);

/**
 * Deactivate a skill by name.
 * @return 0 on success, -1 if not found
 */
int as_skill_deactivate(const char *name);

/* --------------------------------------------------------------------------
 * Query
 * -------------------------------------------------------------------------- */

/** Get skill by name.  Returns NULL if not found. */
const as_skill_t* as_skill_get(const char *name);

/** Get skill by index.  Returns NULL if out of bounds. */
const as_skill_t* as_skill_get_by_index(size_t index);

/** Get number of registered skills. */
size_t as_skill_count(void);

/** Get number of active skills. */
size_t as_skill_active_count(void);

/* --------------------------------------------------------------------------
 * Tool aggregation
 * -------------------------------------------------------------------------- */

/**
 * Build a cJSON array of tool schemas from all active skills.
 *
 * Iterates active skills, looks up each tool name in the tool registry
 * (as_tool_find), and includes its MCP schema.  Deduplicates tools
 * that appear in multiple skills.
 *
 * Caller must cJSON_Delete().
 */
cJSON* as_skill_get_active_tools(void);

/**
 * Build a combined system prompt from all active skills' instructions.
 *
 * Concatenates each active skill's `instructions` field, separated
 * by newlines.  Returns pointer to a static internal buffer --
 * valid until next call to this function or as_skill_registry_deinit().
 *
 * Returns NULL if no active skills have instructions.
 */
const char* as_skill_build_prompt(void);

#ifdef __cplusplus
}
#endif
