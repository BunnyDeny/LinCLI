cmake_minimum_required(VERSION 3.10)

# ============================================================
# 交互式测试命令（演示示例）
# ============================================================
set(TEST_CMDS_DIRS commands)
get_filename_component(PRO_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

foreach(dir ${TEST_CMDS_DIRS})
    file(GLOB TMP "${dir}/*.c")
    list(APPEND TEST_CMDS_SOURCES ${TMP})
endforeach()
add_library(tests_commands OBJECT ${TEST_CMDS_SOURCES})
target_compile_definitions(tests_commands PRIVATE CLI_ENABLE_TESTS=1)
target_include_directories(tests_commands PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/lib
)

# ============================================================
# Unity 单元测试
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
