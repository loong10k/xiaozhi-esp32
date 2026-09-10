/*
 * as_card.h -- AgentCard: device identity and capability description.
 *
 * Maps to agentscope-java:
 *   - ConfigurableAgentCard (A2A spec) → as_agent_card_t
 *   - AgentCardResolver.getAgentCard() → as_agent_card_to_json()
 *   - A2A AgentCard specification fields
 *
 * Describes this ESP32 device to the cloud agent and other A2A participants.
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of skills that can be registered in the card. */
#ifndef CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS
#define CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS 4
#endif

/** Skill entry -- describes one capability category. */
typedef struct {
    const char *name;           /**< Skill name (e.g., "environmental_monitoring") */
    const char *description;    /**< What this skill does */
    const char **tool_names;    /**< Array of tool names in this skill */
    size_t tool_count;          /**< Number of tools */
} as_card_skill_t;

/**
 * AgentCard -- device identity and capabilities.
 *
 * Maps to agentscope-java ConfigurableAgentCard fields:
 *   name, description, version, capabilities, skills, transport
 */
typedef struct {
    const char *device_id;          /**< Unique device ID (MAC/UUID) */
    const char *name;               /**< Human-readable name */
    const char *description;        /**< Device description */
    const char *firmware_version;   /**< Firmware version string */
    const char *board_type;         /**< Board type (matches xiaozhi BOARD_TYPE) */
    const char *location;           /**< Physical location (optional) */

    /* Skills (dynamic, up to CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS) */
    as_card_skill_t skills[CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS];
    size_t skill_count;
} as_agent_card_t;

/* === Initialization === */

/**
 * Initialize the agent card with basic device info.
 * Strings are NOT copied -- caller must ensure they outlive the card.
 */
void as_card_init(as_agent_card_t *card,
                  const char *device_id,
                  const char *name,
                  const char *description,
                  const char *firmware_version,
                  const char *board_type);

/** Set optional location field. */
void as_card_set_location(as_agent_card_t *card, const char *location);

/* === Skill Management === */

/**
 * Add a skill to the card.
 * @return 0 on success, -1 if skill table is full
 */
int as_card_add_skill(as_agent_card_t *card,
                      const char *name,
                      const char *description,
                      const char **tool_names,
                      size_t tool_count);

/* === Serialization === */

/**
 * Generate full A2A AgentCard JSON.
 *
 * Format matches agentscope-java ConfigurableAgentCard.toAgentCard():
 *   { "name", "description", "version", "capabilities",
 *     "skills": [...], "transport": [...] }
 *
 * Caller must cJSON_Delete().
 */
cJSON* as_agent_card_to_json(const as_agent_card_t *card);

/**
 * Generate device capability manifest for xiaozhi protocol.
 * Simplified format suitable for device registration.
 * Caller must cJSON_Delete().
 */
cJSON* as_agent_card_build_capability_manifest(const as_agent_card_t *card);

#ifdef __cplusplus
}
#endif
