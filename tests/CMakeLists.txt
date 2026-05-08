cmake_minimum_required(VERSION 3.10)

# ============================================================
# Interactive demo commands (conditional per-file)
# ============================================================
set(TEST_CMDS_SOURCES "")

if(CONFIG_CLI_ENABLE_DEMO_AUTO_CMD)
    list(APPEND TEST_CMDS_SOURCES commands/test_auto_cmd.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_BOOL)
    list(APPEND TEST_CMDS_SOURCES commands/test_bool.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_BUF_INSUFFICIENT)
    list(APPEND TEST_CMDS_SOURCES commands/test_buf_insufficient.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_CALLBACK)
    list(APPEND TEST_CMDS_SOURCES commands/test_callback.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_CLI_VAR)
    list(APPEND TEST_CMDS_SOURCES commands/test_cli_var.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_CONFLICTS)
    list(APPEND TEST_CMDS_SOURCES commands/test_conflicts.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_DOUBLE)
    list(APPEND TEST_CMDS_SOURCES commands/test_double.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_ENV)
    list(APPEND TEST_CMDS_SOURCES commands/test_env.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_INIT_D)
    list(APPEND TEST_CMDS_SOURCES commands/test_init_d.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_INT)
    list(APPEND TEST_CMDS_SOURCES commands/test_int.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_INT_ARRAY)
    list(APPEND TEST_CMDS_SOURCES commands/test_int_array.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_KEY_INTERACTION)
    list(APPEND TEST_CMDS_SOURCES commands/test_key_interaction.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_LED)
    list(APPEND TEST_CMDS_SOURCES commands/test_led.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_LOG)
    list(APPEND TEST_CMDS_SOURCES commands/test_log.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_MOTOR)
    list(APPEND TEST_CMDS_SOURCES commands/test_motor.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_REQUIRED)
    list(APPEND TEST_CMDS_SOURCES commands/test_required.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_SCOPE)
    list(APPEND TEST_CMDS_SOURCES commands/test_scope.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_STRING)
    list(APPEND TEST_CMDS_SOURCES commands/test_string.c)
endif()
if(CONFIG_CLI_ENABLE_DEMO_WITH_BUF)
    list(APPEND TEST_CMDS_SOURCES commands/test_with_buf.c)
endif()

if(TEST_CMDS_SOURCES)
    add_library(tests_commands OBJECT ${TEST_CMDS_SOURCES})
    target_include_directories(tests_commands PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/cli
        ${CMAKE_SOURCE_DIR}/src/lib
    )
else()
    # Create an empty interface library when no demo commands are enabled
    add_library(tests_commands INTERFACE)
    target_include_directories(tests_commands INTERFACE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/cli
        ${CMAKE_SOURCE_DIR}/src/lib
    )
endif()

# ============================================================
# Unity unit tests
# ============================================================
set(UNIT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_atoi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_mpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_vector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_vsnprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_statem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_unit_errno.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_runners.c
    ${CMAKE_SOURCE_DIR}/src/init/critical_default.c
)

add_executable(unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE lincli)
target_include_directories(unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
)
