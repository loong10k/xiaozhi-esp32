# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

# AgentScope is an opt-in external ESP-IDF component. Keep the disabled build
# independent of the sibling checkout; IDF discovers components before Kconfig.
if(DEFINED ENV{AGENTSCOPE_CPP_ROOT} AND NOT "$ENV{AGENTSCOPE_CPP_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{AGENTSCOPE_CPP_ROOT}" AGENTSCOPE_CPP_ROOT)
    foreach(AGENTSCOPE_COMPONENT agentscope-core agentscope-extensions agentscope-framework)
        set(AGENTSCOPE_COMPONENT_DIR "${AGENTSCOPE_CPP_ROOT}/esp-idf/${AGENTSCOPE_COMPONENT}")
        if(NOT EXISTS "${AGENTSCOPE_COMPONENT_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "Missing AgentScope ESP-IDF component: ${AGENTSCOPE_COMPONENT_DIR}")
        endif()
        list(APPEND EXTRA_COMPONENT_DIRS "${AGENTSCOPE_COMPONENT_DIR}")
    endforeach()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.5.1")
project(xiaozhi)
