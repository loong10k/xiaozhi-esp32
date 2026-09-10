# agentscope-core ESP-IDF component
cmake_minimum_required(VERSION 3.16)

set(SRCS
    # Core tool system
    "src/as_tool.c"
    "src/as_tool_param.c"
    "src/as_hook.c"
    "src/as_rule.c"
    "src/as_card.c"
    "src/as_telemetry.c"
    "src/as_mcp_bridge.cc"

    # Formatter (provider-specific LLM API formatting)
    "src/formatter/as_formatter.c"

    # Credential (API key management)
    "src/credential/as_credential.c"

    # Event system (26 event types)
    "src/event/as_event.c"

    # Plan system (lightweight, no PSRAM)
    "src/plan/as_plan.c"

    # Skill system (compile-time registration)
    "src/skill/as_skill.c"

    # RAG abstractions (lightweight, no PSRAM)
    "src/rag/as_rag.c"
)

set(INCLUDE_DIRS "include")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    PRIV_REQUIRES json freertos esp_timer nvs_flash esp_http_client mbedtls
)
