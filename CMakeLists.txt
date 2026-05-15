cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# Kconfig integration
# ============================================================
find_program(PYTHON3_EXECUTABLE python3)
set(KCONFIG_DOT_CONFIG "${CMAKE_SOURCE_DIR}/.config")
set(KCONFIG_HEADER_DIR "${CMAKE_BINARY_DIR}/include")
set(KCONFIG_HEADER "${KCONFIG_HEADER_DIR}/cli_kconfig.h")

if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 is required for Kconfig support")
endif()

if(NOT EXISTS ${KCONFIG_DOT_CONFIG})
    message(STATUS "No .config found, copying configs/lincli_defconfig")
    configure_file(${CMAKE_SOURCE_DIR}/configs/lincli_defconfig ${KCONFIG_DOT_CONFIG} COPYONLY)
endif()

# Only regenerate cli_kconfig.h when .config is newer than the header.
# This avoids unnecessary CMake reconfiguration on every build.
set(NEED_REGENERATE TRUE)
if(EXISTS ${KCONFIG_HEADER} AND EXISTS ${KCONFIG_DOT_CONFIG})
    file(TIMESTAMP ${KCONFIG_DOT_CONFIG} CONFIG_TIME "%s")
    file(TIMESTAMP ${KCONFIG_HEADER} HEADER_TIME "%s")
    if(HEADER_TIME GREATER_EQUAL CONFIG_TIME)
        set(NEED_REGENERATE FALSE)
    endif()
endif()

if(NEED_REGENERATE)
    message(STATUS "Generating cli_kconfig.h from .config")
    execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/config_to_header.py
                ${KCONFIG_DOT_CONFIG} ${KCONFIG_HEADER}
        RESULT_VARIABLE KCONFIG_GEN_RESULT
    )
    if(NOT KCONFIG_GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate cli_kconfig.h from .config")
    endif()
endif()
include_directories(${KCONFIG_HEADER_DIR})
set_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS ${KCONFIG_DOT_CONFIG})

# ============================================================
# Global settings
# ============================================================
set(OUTPUT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cli.ld ${CMAKE_BINARY_DIR}/cli.ld COPYONLY)

# ============================================================
# Submodules
# ============================================================
add_subdirectory(src)
add_subdirectory(tests)

# ============================================================
# Main executable
# ============================================================
add_executable(${OUTPUT_NAME}
    examples/pc_linux/main.c
    $<TARGET_OBJECTS:src>
    $<TARGET_OBJECTS:commands>
)
target_include_directories(${OUTPUT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/init
)
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

enable_testing()
