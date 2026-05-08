cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# 构建选项（统一放在开头，方便手动修改）
# ============================================================
option(LINCLI_BUILD_UNITY_TESTS "Build Unity unit tests" ON)
option(LINCLI_BUILD_TEST_COMMANDS "Build interactive test commands (led, log, motor, etc.)" ON)

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
    message(STATUS "No .config found, copying default.config")
    configure_file(${CMAKE_SOURCE_DIR}/default.config ${KCONFIG_DOT_CONFIG} COPYONLY)
endif()

message(STATUS "Generating cli_kconfig.h from .config")
execute_process(
    COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/config_to_header.py
            ${KCONFIG_DOT_CONFIG} ${KCONFIG_HEADER}
    RESULT_VARIABLE KCONFIG_GEN_RESULT
)
if(NOT KCONFIG_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate cli_kconfig.h from .config")
endif()
include_directories(${KCONFIG_HEADER_DIR})

# ============================================================
# LinCLI 框架库（OBJECT + INTERFACE）
# ============================================================
add_subdirectory(src)

# ============================================================
# PC 端演示可执行文件
# ============================================================
set(OUTPUT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cli.ld ${CMAKE_BINARY_DIR}/cli.ld COPYONLY)

add_executable(${OUTPUT_NAME}
    examples/pc_linux/main.c
)
target_link_libraries(${OUTPUT_NAME} PRIVATE lincli)
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

# ============================================================
# 测试（可选）
# ============================================================
if(LINCLI_BUILD_UNITY_TESTS OR LINCLI_BUILD_TEST_COMMANDS)
    add_subdirectory(tests)
endif()

if(LINCLI_BUILD_UNITY_TESTS)
    enable_testing()
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

if(LINCLI_BUILD_TEST_COMMANDS)
    target_link_libraries(${OUTPUT_NAME} PRIVATE tests_commands)
endif()
