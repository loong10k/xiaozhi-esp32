/*
 * as_skill.c -- Skill registry implementation.
 *
 * Maps to agentscope-java: AgentSkill + SkillRegistry.
 *
 * Fixed-pool storage in SRAM.  Skill and tool name strings point to
 * .rodata (not copied).  The tool aggregation iterates active skills,
 * looks up each tool name in the tool registry, and builds a
 * deduplicated cJSON array.
 */
#include "as_skill.h"
#include "as_tool.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>

static const char *TAG = "as_skill";

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

static as_skill_t s_skills[CONFIG_AGENTSCOPE_CORE_MAX_SKILLS];
static bool s_initialized = false;

/* Static buffer for concatenated prompt (no malloc) */
static char s_prompt_buf[CONFIG_AGENTSCOPE_CORE_SKILL_PROMPT_BUF_SIZE];

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/** Find skill by name.  Returns index or -1. */
static int find_skill(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (s_skills[i].name && strcmp(s_skills[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/** Find a free slot (name == NULL).  Returns index or -1. */
static int find_free_slot(void) {
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (s_skills[i].name == NULL) return i;
    }
    return -1;
}

/**
 * Check whether a tool name has already been added to a cJSON array.
 * Simple linear scan (small arrays on embedded).
 */
static bool tool_already_in_array(const cJSON *array, const char *tool_name) {
    if (!array || !tool_name) return false;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        const cJSON *nm = cJSON_GetObjectItem(item, "name");
        if (nm && nm->valuestring &&
            strcmp(nm->valuestring, tool_name) == 0) {
            return true;
        }
    }
    return false;
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void as_skill_registry_init(void) {
    memset(s_skills, 0, sizeof(s_skills));
    s_prompt_buf[0] = '\0';
    s_initialized = true;
    ESP_LOGI(TAG, "Skill registry initialized (max %d skills, %d tools each)",
             CONFIG_AGENTSCOPE_CORE_MAX_SKILLS,
             CONFIG_AGENTSCOPE_CORE_MAX_TOOLS_PER_SKILL);
}

void as_skill_registry_deinit(void) {
    memset(s_skills, 0, sizeof(s_skills));
    s_prompt_buf[0] = '\0';
    s_initialized = false;
    ESP_LOGI(TAG, "Skill registry deinitialized");
}

/* --------------------------------------------------------------------------
 * Registration
 * -------------------------------------------------------------------------- */

int as_skill_register(const as_skill_t *skill) {
    if (!s_initialized || !skill || !skill->name) {
        ESP_LOGW(TAG, "Invalid skill registration arguments");
        return -1;
    }

    /* Check for duplicate name */
    int existing = find_skill(skill->name);
    if (existing >= 0) {
        ESP_LOGW(TAG, "Skill '%s' already registered, overwriting", skill->name);
        s_skills[existing] = *skill;
        return 0;
    }

    int idx = find_free_slot();
    if (idx < 0) {
        ESP_LOGE(TAG, "Skill registry full (%d), cannot register '%s'",
                 CONFIG_AGENTSCOPE_CORE_MAX_SKILLS, skill->name);
        return -1;
    }

    s_skills[idx] = *skill;

    ESP_LOGI(TAG, "Registered skill[%d]: '%s' (tools=%d, instructions=%s)",
             idx, skill->name, (int)skill->tool_count,
             skill->instructions ? "yes" : "no");
    return 0;
}

/* --------------------------------------------------------------------------
 * Activation
 * -------------------------------------------------------------------------- */

int as_skill_activate(const char *name) {
    if (!s_initialized || !name) return -1;

    int idx = find_skill(name);
    if (idx < 0) {
        ESP_LOGW(TAG, "Skill '%s' not found", name);
        return -1;
    }

    s_skills[idx].active = true;
    ESP_LOGI(TAG, "Activated skill '%s'", name);
    return 0;
}

int as_skill_deactivate(const char *name) {
    if (!s_initialized || !name) return -1;

    int idx = find_skill(name);
    if (idx < 0) {
        ESP_LOGW(TAG, "Skill '%s' not found", name);
        return -1;
    }

    s_skills[idx].active = false;
    ESP_LOGI(TAG, "Deactivated skill '%s'", name);
    return 0;
}

/* --------------------------------------------------------------------------
 * Query
 * -------------------------------------------------------------------------- */

const as_skill_t* as_skill_get(const char *name) {
    if (!s_initialized || !name) return NULL;
    int idx = find_skill(name);
    return (idx >= 0) ? &s_skills[idx] : NULL;
}

const as_skill_t* as_skill_get_by_index(size_t index) {
    if (!s_initialized || index >= CONFIG_AGENTSCOPE_CORE_MAX_SKILLS) return NULL;
    if (s_skills[index].name == NULL) return NULL;
    return &s_skills[index];
}

size_t as_skill_count(void) {
    if (!s_initialized) return 0;
    size_t count = 0;
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (s_skills[i].name) count++;
    }
    return count;
}

size_t as_skill_active_count(void) {
    if (!s_initialized) return 0;
    size_t count = 0;
    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (s_skills[i].name && s_skills[i].active) count++;
    }
    return count;
}

/* --------------------------------------------------------------------------
 * Tool aggregation
 * -------------------------------------------------------------------------- */

cJSON* as_skill_get_active_tools(void) {
    if (!s_initialized) return NULL;

    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (!s_skills[i].name || !s_skills[i].active) continue;

        const as_skill_t *sk = &s_skills[i];
        for (size_t j = 0; j < sk->tool_count; j++) {
            const char *tname = sk->tool_names[j];
            if (!tname) continue;

            /* Skip if already added (from a previous skill) */
            if (tool_already_in_array(array, tname)) continue;

            /* Look up tool in the tool registry */
            const as_tool_entry_t *tool = as_tool_find(tname);
            if (!tool) {
                ESP_LOGW(TAG, "Skill '%s' references unknown tool '%s'",
                         sk->name, tname);
                continue;
            }

            /* Build tool schema (same format as as_tool_registry_to_mcp_schema) */
            cJSON *tool_json = cJSON_CreateObject();
            if (!tool_json) continue;

            cJSON_AddStringToObject(tool_json, "name", tool->name);
            cJSON_AddStringToObject(tool_json, "description",
                                    tool->description ? tool->description : "");

            /* inputSchema */
            cJSON *input_schema = cJSON_CreateObject();
            if (input_schema) {
                cJSON_AddStringToObject(input_schema, "type", "object");
                if (tool->params && tool->param_count > 0) {
                    cJSON *props = as_tool_params_to_json(tool->params,
                                                         tool->param_count);
                    if (props) {
                        cJSON_AddItemToObject(input_schema, "properties", props);
                    }
                    cJSON *req = as_tool_params_required_array(tool->params,
                                                              tool->param_count);
                    if (req) {
                        cJSON_AddItemToObject(input_schema, "required", req);
                    }
                } else {
                    cJSON_AddItemToObject(input_schema, "properties",
                                          cJSON_CreateObject());
                }
                cJSON_AddItemToObject(tool_json, "inputSchema", input_schema);
            }

            cJSON_AddItemToArray(array, tool_json);
        }
    }

    ESP_LOGI(TAG, "Built active tools JSON (%d items)",
             cJSON_GetArraySize(array));
    return array;
}

const char* as_skill_build_prompt(void) {
    if (!s_initialized) return NULL;

    s_prompt_buf[0] = '\0';
    size_t offset = 0;
    bool found_any = false;

    for (int i = 0; i < CONFIG_AGENTSCOPE_CORE_MAX_SKILLS; i++) {
        if (!s_skills[i].name || !s_skills[i].active) continue;
        if (!s_skills[i].instructions) continue;

        const char *instr = s_skills[i].instructions;
        size_t len = strlen(instr);
        if (len == 0) continue;

        /* Add separator if not first */
        if (found_any) {
            if (offset + 2 < CONFIG_AGENTSCOPE_CORE_SKILL_PROMPT_BUF_SIZE) {
                s_prompt_buf[offset++] = '\n';
                s_prompt_buf[offset] = '\0';
            }
        }

        /* Append instruction */
        size_t remaining = CONFIG_AGENTSCOPE_CORE_SKILL_PROMPT_BUF_SIZE - offset - 1;
        size_t copy_len = (len < remaining) ? len : remaining;
        if (copy_len > 0) {
            memcpy(s_prompt_buf + offset, instr, copy_len);
            offset += copy_len;
            s_prompt_buf[offset] = '\0';
        }

        found_any = true;

        if (len > remaining) {
            ESP_LOGW(TAG, "Prompt buffer overflow, truncated skill '%s'",
                     s_skills[i].name);
            break;
        }
    }

    return found_any ? s_prompt_buf : NULL;
}
