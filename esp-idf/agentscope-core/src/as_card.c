/*
 * as_card.c -- AgentCard implementation.
 *
 * Maps to agentscope-java: ConfigurableAgentCard.toAgentCard()
 */
#include "as_card.h"
#include <string.h>
#include <stdio.h>
#include <esp_log.h>

static const char *TAG = "as_card";

void as_card_init(as_agent_card_t *card,
                  const char *device_id,
                  const char *name,
                  const char *description,
                  const char *firmware_version,
                  const char *board_type) {
    if (!card) return;
    memset(card, 0, sizeof(as_agent_card_t));
    card->device_id = device_id;
    card->name = name;
    card->description = description;
    card->firmware_version = firmware_version;
    card->board_type = board_type;
    card->skill_count = 0;
    ESP_LOGI(TAG, "AgentCard initialized: %s (%s)", name, device_id);
}

void as_card_set_location(as_agent_card_t *card, const char *location) {
    if (card) card->location = location;
}

int as_card_add_skill(as_agent_card_t *card,
                      const char *name,
                      const char *description,
                      const char **tool_names,
                      size_t tool_count) {
    if (!card || !name) return -1;
    if (card->skill_count >= CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS) {
        ESP_LOGE(TAG, "Skill table full (%d)", CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS);
        return -1;
    }

    as_card_skill_t *skill = &card->skills[card->skill_count++];
    skill->name = name;
    skill->description = description;
    skill->tool_names = tool_names;
    skill->tool_count = tool_count;

    ESP_LOGI(TAG, "Added skill '%s' with %d tools", name, (int)tool_count);
    return 0;
}

cJSON* as_agent_card_to_json(const as_agent_card_t *card) {
    if (!card) return NULL;

    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    /* Basic info */
    cJSON_AddStringToObject(json, "name", card->name ? card->name : "");
    cJSON_AddStringToObject(json, "description", card->description ? card->description : "");
    cJSON_AddStringToObject(json, "version", card->firmware_version ? card->firmware_version : "0.0.0");

    /* Capabilities */
    cJSON *caps = cJSON_AddObjectToObject(json, "capabilities");
    cJSON_AddBoolToObject(caps, "streaming", false);    /* ESP32 doesn't do streaming inference */
    cJSON_AddBoolToObject(caps, "pushNotifications", true); /* MQTT push */

    /* Skills */
    cJSON *skills = cJSON_AddArrayToObject(json, "skills");
    for (size_t i = 0; i < card->skill_count; i++) {
        const as_card_skill_t *s = &card->skills[i];
        cJSON *skill = cJSON_CreateObject();
        cJSON_AddStringToObject(skill, "name", s->name ? s->name : "");
        cJSON_AddStringToObject(skill, "description", s->description ? s->description : "");

        cJSON *methods = cJSON_AddArrayToObject(skill, "methods");
        for (size_t j = 0; j < s->tool_count; j++) {
            if (s->tool_names && s->tool_names[j]) {
                cJSON_AddItemToArray(methods, cJSON_CreateString(s->tool_names[j]));
            }
        }
        cJSON_AddItemToArray(skills, skill);
    }

    /* Transport (maps to agentscope-java NacosA2aRegistryTransportProperties) */
    cJSON *transports = cJSON_AddArrayToObject(json, "transport");
    cJSON *transport = cJSON_CreateObject();
    cJSON_AddStringToObject(transport, "type", "mcp");
    if (card->device_id) {
        char topic[128];
        snprintf(topic, sizeof(topic), "agentscope/device_%s", card->device_id);
        cJSON_AddStringToObject(transport, "topic_prefix", topic);
    }
    cJSON_AddItemToArray(transports, transport);

    return json;
}

cJSON* as_agent_card_build_capability_manifest(const as_agent_card_t *card) {
    if (!card) return NULL;

    cJSON *manifest = cJSON_CreateObject();
    if (!manifest) return NULL;

    cJSON_AddStringToObject(manifest, "device_id", card->device_id ? card->device_id : "");
    cJSON_AddStringToObject(manifest, "name", card->name ? card->name : "");
    cJSON_AddStringToObject(manifest, "board_type", card->board_type ? card->board_type : "");
    cJSON_AddStringToObject(manifest, "firmware", card->firmware_version ? card->firmware_version : "");
    if (card->location) {
        cJSON_AddStringToObject(manifest, "location", card->location);
    }

    /* Skill count summary */
    cJSON_AddNumberToObject(manifest, "skill_count", card->skill_count);

    size_t total_tools = 0;
    for (size_t i = 0; i < card->skill_count; i++) {
        total_tools += card->skills[i].tool_count;
    }
    cJSON_AddNumberToObject(manifest, "total_tools", total_tools);

    return manifest;
}
