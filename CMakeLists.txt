cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# 构建选项（统一放在开头，方便手动修改）
# ============================================================
option(LINCLI_BUILD_UNITY_TESTS "Build Unity unit tests" ON)
option(LINCLI_BUILD_TEST_COMMANDS "Build interactive test commands (led, log, motor, etc.)" OFF)
option(LINCLI_ENABLE_INLINE_TEST "Enable scheduler inline test mode (prints tick count every 50 loops)" OFF)

# ============================================================
# LinCLI 框架库（OBJECT + INTERFACE）
# ============================================================
add_subdirectory(src)
if(LINCLI_ENABLE_INLINE_TEST)
    target_compile_definitions(lincli_obj PRIVATE INLINE_TEST_EN=1)
endif()

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
target_link_libraries(${OUTPUT_NAME} PRIVATE lincli m)
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
