/*
 * as_plan.c -- Lightweight plan system implementation.
 *
 * Maps to agentscope-java: Plan / PlanStep / PlanManager.
 *
 * Fixed-pool storage in SRAM.  No malloc for plan or step structs.
 * All strings copied into fixed-size char arrays via strncpy.
 */
#include "as_plan.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>

static const char *TAG = "as_plan";

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static as_plan_t s_plans[CONFIG_AGENTSCOPE_CORE_MAX_PLANS];
static bool s_plan_used[CONFIG_AGENTSCOPE_CORE_MAX_PLANS];
static uint32_t s_plan_counter = 0;
static bool s_initialized = false;

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/** Find plan by ID.  Returns index or -1. */
static int find_plan(const char *plan_id) {
    if (!plan_id) return -1;
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_PLANS; i++) {
        if (s_plan_used[i] && strcmp(s_plans[i].plan_id, plan_id) == 0) {
            return i;
        }
    }
    return -1;
}

/** Find a free slot.  Returns index or -1. */
static int find_free_slot(void) {
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_PLANS; i++) {
        if (!s_plan_used[i]) return i;
    }
    return -1;
}

/** Generate a short plan ID like "p0", "p1", ... */
static void gen_plan_id(char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "p%lu", (unsigned long)s_plan_counter++);
}

/** Generate a short step ID like "s0", "s1", ... */
static void gen_step_id(char *buf, size_t buf_size, int index) {
    snprintf(buf, buf_size, "s%d", index);
}

/** Build a cJSON step object from a plan step. */
static cJSON* step_to_json(const as_plan_step_t *step) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddStringToObject(obj, "id", step->id);
    cJSON_AddStringToObject(obj, "description", step->description);
    cJSON_AddStringToObject(obj, "status", as_plan_status_name(step->status));
    cJSON_AddNumberToObject(obj, "progress", step->progress);
    return obj;
}

/* --------------------------------------------------------------------------
 * Status names
 * -------------------------------------------------------------------------- */

static const char *const s_status_names[] = {
    [AS_PLAN_PENDING] = "pending",
    [AS_PLAN_RUNNING] = "running",
    [AS_PLAN_DONE]    = "done",
    [AS_PLAN_FAILED]  = "failed",
};

const char* as_plan_status_name(as_plan_status_t status) {
    if (status >= 0 && status <= AS_PLAN_FAILED) {
        return s_status_names[status];
    }
    return "unknown";
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void as_plan_init(void) {
    memset(s_plans, 0, sizeof(s_plans));
    memset(s_plan_used, 0, sizeof(s_plan_used));
    s_plan_counter = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "Plan system initialized (max %d plans, %d steps each)",
             CONFIG_AGENTSCOPE_CORE_MAX_PLANS,
             CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS);
}

void as_plan_deinit(void) {
    memset(s_plans, 0, sizeof(s_plans));
    memset(s_plan_used, 0, sizeof(s_plan_used));
    s_initialized = false;
    ESP_LOGI(TAG, "Plan system deinitialized");
}

/* --------------------------------------------------------------------------
 * Plan management
 * -------------------------------------------------------------------------- */

const as_plan_t* as_plan_create(const char *goal,
                                const char **step_descriptions,
                                int step_count) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Plan system not initialized");
        return NULL;
    }
    if (!goal || !step_descriptions || step_count <= 0) {
        ESP_LOGW(TAG, "Invalid plan create arguments");
        return NULL;
    }
    if (step_count > CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS) {
        ESP_LOGE(TAG, "Too many steps: %d (max %d)",
                 step_count, CONFIG_AGENTSCOPE_CORE_MAX_PLAN_STEPS);
        return NULL;
    }

    int idx = find_free_slot();
    if (idx < 0) {
        ESP_LOGE(TAG, "Plan pool full (%d plans)", CONFIG_AGENTSCOPE_CORE_MAX_PLANS);
        return NULL;
    }

    as_plan_t *p = &s_plans[idx];
    memset(p, 0, sizeof(*p));

    /* Generate plan ID */
    gen_plan_id(p->plan_id, sizeof(p->plan_id));

    /* Copy goal */
    strncpy(p->goal, goal, sizeof(p->goal) - 1);
    p->goal[sizeof(p->goal) - 1] = '\0';

    /* Initialize steps */
    p->step_count = (uint8_t)step_count;
    p->current_step = 0;
    p->status = AS_PLAN_PENDING;

    for (int i = 0; i < step_count; i++) {
        as_plan_step_t *s = &p->steps[i];
        gen_step_id(s->id, sizeof(s->id), i);
        if (step_descriptions[i]) {
            strncpy(s->description, step_descriptions[i],
                    sizeof(s->description) - 1);
            s->description[sizeof(s->description) - 1] = '\0';
        } else {
            s->description[0] = '\0';
        }
        s->status = AS_PLAN_PENDING;
        s->progress = 0;
    }

    s_plan_used[idx] = true;

    ESP_LOGI(TAG, "Created plan '%s': goal='%s', %d steps",
             p->plan_id, p->goal, step_count);
    return p;
}

int as_plan_destroy(const char *plan_id) {
    if (!s_initialized || !plan_id) return -1;

    int idx = find_plan(plan_id);
    if (idx < 0) return -1;

    ESP_LOGI(TAG, "Destroyed plan '%s'", s_plans[idx].plan_id);
    s_plan_used[idx] = false;
    memset(&s_plans[idx], 0, sizeof(s_plans[idx]));
    return 0;
}

int as_plan_advance(const char *plan_id) {
    if (!s_initialized || !plan_id) return -1;

    int idx = find_plan(plan_id);
    if (idx < 0) return -1;

    as_plan_t *p = &s_plans[idx];

    if (p->status == AS_PLAN_DONE || p->status == AS_PLAN_FAILED) {
        ESP_LOGW(TAG, "Cannot advance plan '%s' (status=%s)",
                 p->plan_id, as_plan_status_name(p->status));
        return -1;
    }

    if (p->current_step >= p->step_count) {
        ESP_LOGW(TAG, "Plan '%s' already at end", p->plan_id);
        return -1;
    }

    /* Mark current step as done */
    p->steps[p->current_step].status = AS_PLAN_DONE;
    p->steps[p->current_step].progress = 100;
    p->current_step++;

    if (p->current_step >= p->step_count) {
        /* All steps complete */
        p->status = AS_PLAN_DONE;
        ESP_LOGI(TAG, "Plan '%s' completed", p->plan_id);
    } else {
        /* Start next step */
        p->steps[p->current_step].status = AS_PLAN_RUNNING;
        p->status = AS_PLAN_RUNNING;
        ESP_LOGI(TAG, "Plan '%s' advanced to step %d/%d: '%s'",
                 p->plan_id, p->current_step + 1, p->step_count,
                 p->steps[p->current_step].description);
    }

    return 0;
}

int as_plan_complete(const char *plan_id) {
    if (!s_initialized || !plan_id) return -1;

    int idx = find_plan(plan_id);
    if (idx < 0) return -1;

    as_plan_t *p = &s_plans[idx];

    /* Mark all pending/running steps as done */
    for (int i = 0; i < p->step_count; i++) {
        if (p->steps[i].status == AS_PLAN_PENDING ||
            p->steps[i].status == AS_PLAN_RUNNING) {
            p->steps[i].status = AS_PLAN_DONE;
            p->steps[i].progress = 100;
        }
    }

    p->current_step = p->step_count;
    p->status = AS_PLAN_DONE;

    ESP_LOGI(TAG, "Plan '%s' force-completed", p->plan_id);
    return 0;
}

int as_plan_fail(const char *plan_id) {
    if (!s_initialized || !plan_id) return -1;

    int idx = find_plan(plan_id);
    if (idx < 0) return -1;

    as_plan_t *p = &s_plans[idx];

    /* Mark current step as failed */
    if (p->current_step < p->step_count) {
        p->steps[p->current_step].status = AS_PLAN_FAILED;
    }

    p->status = AS_PLAN_FAILED;

    ESP_LOGW(TAG, "Plan '%s' failed at step %d/%d",
             p->plan_id, p->current_step + 1, p->step_count);
    return 0;
}

int as_plan_set_progress(const char *plan_id, uint8_t progress) {
    if (!s_initialized || !plan_id) return -1;

    int idx = find_plan(plan_id);
    if (idx < 0) return -1;

    as_plan_t *p = &s_plans[idx];
    if (p->current_step >= p->step_count) return -1;

    p->steps[p->current_step].progress = progress > 100 ? 100 : progress;
    return 0;
}

/* --------------------------------------------------------------------------
 * Query
 * -------------------------------------------------------------------------- */

const as_plan_t* as_plan_get(const char *plan_id) {
    if (!s_initialized || !plan_id) return NULL;
    int idx = find_plan(plan_id);
    return (idx >= 0) ? &s_plans[idx] : NULL;
}

size_t as_plan_active_count(void) {
    if (!s_initialized) return 0;
    size_t count = 0;
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_PLANS; i++) {
        if (s_plan_used[i] &&
            s_plans[i].status != AS_PLAN_DONE &&
            s_plans[i].status != AS_PLAN_FAILED) {
            count++;
        }
    }
    return count;
}

cJSON* as_plan_get_active_json(void) {
    if (!s_initialized) return NULL;

    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_PLANS; i++) {
        if (!s_plan_used[i]) continue;
        if (s_plans[i].status == AS_PLAN_DONE ||
            s_plans[i].status == AS_PLAN_FAILED) {
            continue;
        }

        const as_plan_t *p = &s_plans[i];
        cJSON *obj = cJSON_CreateObject();
        if (!obj) continue;

        cJSON_AddStringToObject(obj, "plan_id", p->plan_id);
        cJSON_AddStringToObject(obj, "goal", p->goal);
        cJSON_AddStringToObject(obj, "status", as_plan_status_name(p->status));
        cJSON_AddNumberToObject(obj, "current_step", p->current_step);
        cJSON_AddNumberToObject(obj, "step_count", p->step_count);

        cJSON *steps_arr = cJSON_CreateArray();
        if (steps_arr) {
            for (int j = 0; j < p->step_count; j++) {
                cJSON *s = step_to_json(&p->steps[j]);
                if (s) cJSON_AddItemToArray(steps_arr, s);
            }
            cJSON_AddItemToObject(obj, "steps", steps_arr);
        }

        cJSON_AddItemToArray(array, obj);
    }

    return array;
}
