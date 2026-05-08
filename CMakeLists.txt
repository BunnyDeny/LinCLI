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

# 让 CMake 在构建时监控 .config 的变化，如有改动则自动重新运行配置阶段
set_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS ${KCONFIG_DOT_CONFIG})

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
# 测试
# ============================================================
add_subdirectory(tests)
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
target_link_libraries(${OUTPUT_NAME} PRIVATE tests_commands)
