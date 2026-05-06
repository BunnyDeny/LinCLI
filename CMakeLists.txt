cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# LinCLI 框架静态库
# ============================================================
option(LINCLI_ENABLE_INLINE_TEST "Enable scheduler inline test mode (prints tick count every 50 loops)" OFF)
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
target_link_libraries(${OUTPUT_NAME} PRIVATE lincli)
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

# ============================================================
# 测试（可选）
# ============================================================
option(LINCLI_BUILD_TESTS "Build LinCLI tests" ON)
if(LINCLI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    # 将演示示例中的交互式测试命令也链接到 a.out
    target_link_libraries(${OUTPUT_NAME} PRIVATE tests_commands)
    add_test(NAME unit_tests COMMAND unit_tests)
endif()
