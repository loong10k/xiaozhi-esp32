# agentscope-core test suite
cmake_minimum_required(VERSION 3.16)

# Note: These tests compile on host (POSIX) for CI verification.
# On ESP32, they run via unity test framework.
# This CMakeLists is for standalone host compilation only.

project(agentscope-core-tests C)

set(CMAKE_C_STANDARD 11)

# Include core headers
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${CORE_DIR}/include)

# ESP-IDF stubs for host compilation
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/stubs)

# Source files under test
set(CORE_SOURCES
    ${CORE_DIR}/src/as_tool_param.c
    ${CORE_DIR}/src/as_tool.c
    ${CORE_DIR}/src/as_hook.c
    ${CORE_DIR}/src/as_rule.c
    ${CORE_DIR}/src/as_card.c
    ${CORE_DIR}/src/as_telemetry.c
)

# Test files
set(TESTS
    test_tool
    test_rule
    test_hook
    test_card
    test_telemetry
)

# Create stub directory for ESP-IDF headers
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

foreach(TEST ${TESTS})
    add_executable(${TEST} ${TEST}.c ${CORE_SOURCES})
    target_compile_definitions(${TEST} PRIVATE
        CONFIG_AGENTSCOPE_CORE_ENABLE=1
        CONFIG_AGENTSCOPE_CORE_MAX_TOOLS=16
        CONFIG_AGENTSCOPE_CORE_MAX_PARAMS_PER_TOOL=8
        CONFIG_AGENTSCOPE_CORE_MAX_RULES=8
        CONFIG_AGENTSCOPE_CORE_MAX_HOOKS_PER_POINT=4
        CONFIG_AGENTSCOPE_CORE_TELEMETRY_MAX_SENSORS=8
        CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS=4
        CONFIG_AGENTSCOPE_CORE_RULE_TICK_MS=1000
        CONFIG_AGENTSCOPE_CORE_MCP_BRIDGE=0
    )
    add_test(NAME ${TEST} COMMAND ${TEST})
endforeach()
