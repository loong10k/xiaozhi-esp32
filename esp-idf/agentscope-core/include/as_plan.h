/*
 * as_plan.h -- Lightweight plan system for multi-step agent workflows.
 *
 * Maps to agentscope-java: Plan / PlanStep / PlanManager.
 *
 * Core-only implementation that fits entirely in SRAM (no PSRAM).
 * Fixed-pool: at most AS_PLAN_MAX_ACTIVE plans, each with at most
 * AS_PLAN_MAX_STEPS steps.  All strings stored in fixed-size char
 * arrays (no heap allocation).
 *
 * Use extensions' as_planner.h for the full LLM-driven planner
 * that requires PSRAM and JSON parsing.
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#ifndef CONFIG_AGENTSCOPE_CORE_MAX_PLANS
#define CONFIG_AGENTSCOPE_CORE_MAX_PLANS 3
#endif

#ifndef CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS
#define CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS 8
#endif

/* --------------------------------------------------------------------------
 * Types
 * -------------------------------------------------------------------------- */

/** Plan step status. */
typedef enum {
    AS_PLAN_PENDING,   /**< Not yet started */
    AS_PLAN_RUNNING,   /**< Currently executing */
    AS_PLAN_DONE,      /**< Completed successfully */
    AS_PLAN_FAILED,    /**< Failed */
} as_plan_status_t;

/** A single step within a plan. */
typedef struct {
    char id[8];                     /**< Short step ID (e.g., "s0", "s1") */
    char description[64];           /**< What this step does */
    as_plan_status_t status;        /**< Step status */
    uint8_t progress;               /**< 0-100 progress percentage */
} as_plan_step_t;

/** A complete plan with goal and steps. */
typedef struct {
    char plan_id[16];               /**< Plan identifier (e.g., "p0") */
    char goal[128];                 /**< High-level goal description */
    as_plan_step_t steps[CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS];
    uint8_t step_count;             /**< Total number of steps */
    uint8_t current_step;           /**< Index of current/next step */
    as_plan_status_t status;        /**< Overall plan status */
} as_plan_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/** Initialize the plan manager.  Clears all plans. */
void as_plan_init(void);

/** Deinitialize the plan manager. */
void as_plan_deinit(void);

/* --------------------------------------------------------------------------
 * Plan management
 * -------------------------------------------------------------------------- */

/**
 * Create a new plan with the given goal and step descriptions.
 *
 * @param goal               High-level goal string (copied, max 127 chars)
 * @param step_descriptions  Array of step description strings (copied, max 63 chars each)
 * @param step_count         Number of steps (must be <= CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS)
 * @return Pointer to created plan, or NULL if pool is full or input is invalid
 *
 * The plan is created with all steps in AS_PLAN_PENDING status.
 * The returned pointer is stable until the plan is destroyed.
 */
const as_plan_t* as_plan_create(const char *goal,
                                const char **step_descriptions,
                                int step_count);

/**
 * Destroy a plan by ID, freeing its slot for reuse.
 * @return 0 on success, -1 if not found
 */
int as_plan_destroy(const char *plan_id);

/**
 * Advance the current step to the next one.
 * Marks the current step as AS_PLAN_DONE and the next step as AS_PLAN_RUNNING.
 * If this was the last step, the plan status becomes AS_PLAN_DONE.
 *
 * @param plan_id  Plan identifier
 * @return 0 on success, -1 if plan not found or cannot advance
 */
int as_plan_advance(const char *plan_id);

/**
 * Mark the entire plan as completed (all remaining pending steps become done).
 * @return 0 on success, -1 if plan not found
 */
int as_plan_complete(const char *plan_id);

/**
 * Mark the plan as failed.
 * The current step is marked AS_PLAN_FAILED; the plan status becomes AS_PLAN_FAILED.
 *
 * @param plan_id  Plan identifier
 * @return 0 on success, -1 if plan not found
 */
int as_plan_fail(const char *plan_id);

/**
 * Update the progress of the current step.
 * @param plan_id   Plan identifier
 * @param progress  0-100 percentage
 * @return 0 on success, -1 if plan not found
 */
int as_plan_set_progress(const char *plan_id, uint8_t progress);

/* --------------------------------------------------------------------------
 * Query
 * -------------------------------------------------------------------------- */

/** Get plan by ID.  Returns NULL if not found. */
const as_plan_t* as_plan_get(const char *plan_id);

/** Get number of active (non-terminal) plans. */
size_t as_plan_active_count(void);

/**
 * Build JSON array of all active plans.
 * Each entry: { "plan_id", "goal", "status", "current_step",
 *               "steps": [ { "id", "description", "status", "progress" }, ... ] }
 * Caller must cJSON_Delete().
 */
cJSON* as_plan_get_active_json(void);

/**
 * Get human-readable name for a plan status.
 * Returns a static string; never returns NULL.
 */
const char* as_plan_status_name(as_plan_status_t status);

#ifdef __cplusplus
}
#endif
