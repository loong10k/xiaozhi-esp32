/*
 * as_hook.c -- Hook chain execution implementation.
 *
 * Maps to agentscope-java: Hook.onEvent() + MiddlewareChain.build() (onion pattern).
 * Simplified to linear chain for embedded (no Reactor/Flux).
 */
#include "as_hook.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "as_hook";

/* Fixed-pool hook storage */
#ifndef CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT
#define CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT 4
#endif

typedef struct {
    as_hook_fn_t fn;
    void *user_data;
    int priority;
} hook_entry_t;

static hook_entry_t s_hooks[AS_HOOK_COUNT][CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT];
static size_t s_hook_counts[AS_HOOK_COUNT] = {0};
static bool s_initialized = false;

static void sort_hooks(as_hook_point_t point) {
    /* Simple insertion sort by priority (small array, OK for embedded) */
    size_t n = s_hook_counts[point];
    for (size_t i = 1; i < n; i++) {
        hook_entry_t key = s_hooks[point][i];
        size_t j = i;
        while (j > 0 && s_hooks[point][j - 1].priority > key.priority) {
            s_hooks[point][j] = s_hooks[point][j - 1];
            j--;
        }
        s_hooks[point][j] = key;
    }
}

void as_hook_init(void) {
    memset(s_hooks, 0, sizeof(s_hooks));
    memset(s_hook_counts, 0, sizeof(s_hook_counts));
    s_initialized = true;
    ESP_LOGI(TAG, "Hook system initialized (%d hooks per point)",
             CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT);
}

void as_hook_deinit(void) {
    memset(s_hook_counts, 0, sizeof(s_hook_counts));
    s_initialized = false;
}

int as_hook_register(as_hook_point_t point, int priority,
                     as_hook_fn_t fn, void *user_data) {
    if (!s_initialized || point >= AS_HOOK_COUNT || !fn) return -1;

    if (s_hook_counts[point] >= CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT) {
        ESP_LOGE(TAG, "Hook table full for point %d", point);
        return -1;
    }

    size_t idx = s_hook_counts[point]++;
    s_hooks[point][idx].fn = fn;
    s_hooks[point][idx].user_data = user_data;
    s_hooks[point][idx].priority = priority;

    sort_hooks(point);

    ESP_LOGI(TAG, "Registered hook at point %d (priority=%d, total=%d)",
             point, priority, (int)s_hook_counts[point]);
    return 0;
}

int as_hook_unregister(as_hook_point_t point, as_hook_fn_t fn) {
    if (!s_initialized || point >= AS_HOOK_COUNT || !fn) return -1;

    for (size_t i = 0; i < s_hook_counts[point]; i++) {
        if (s_hooks[point][i].fn == fn) {
            /* Shift remaining hooks */
            for (size_t j = i; j < s_hook_counts[point] - 1; j++) {
                s_hooks[point][j] = s_hooks[point][j + 1];
            }
            s_hook_counts[point]--;
            return 0;
        }
    }
    return -1;
}

void as_hook_clear(as_hook_point_t point) {
    if (point < AS_HOOK_COUNT) {
        s_hook_counts[point] = 0;
    }
}

void as_hook_clear_all(void) {
    memset(s_hook_counts, 0, sizeof(s_hook_counts));
}

as_hook_response_t as_hook_execute(as_hook_point_t point,
                                   const char *tool_name,
                                   const cJSON *data) {
    as_hook_response_t resp = { .result = AS_HOOK_CONTINUE, .modified_data = NULL };

    if (!s_initialized || point >= AS_HOOK_COUNT) return resp;

    for (size_t i = 0; i < s_hook_counts[point]; i++) {
        as_hook_response_t r = s_hooks[point][i].fn(tool_name, data, s_hooks[point][i].user_data);

        if (r.result == AS_HOOK_SKIP) {
            return r;  /* Short-circuit: skip remaining hooks and tool */
        }

        if (r.result == AS_HOOK_MODIFIED && r.modified_data) {
            /* Chain: next hook sees modified data */
            resp = r;
            data = r.modified_data;
        }
    }

    return resp;
}
