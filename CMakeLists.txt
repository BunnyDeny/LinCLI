cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# LinCLI 框架静态库
# ============================================================
set(LINCLI_SRC
    src/lib/cli_string.c
    src/lib/cli_atoi.c
    src/lib/cli_float.c
    src/lib/cli_mpool.c
    src/lib/cli_vsnprintf.c
    src/lib/cli_errno.c
    src/lib/stateM.c
    src/lib/tVector.c
    src/cli/cli_io.c
    src/cli/cli_cmd_line.c
    src/cli/cli_completion.c
    src/cli/cli_edit.c
    src/cli/cli_env.c
    src/cli/cli_history.c
    src/cli/cli_logo.c
    src/cli/cli_parse.c
    src/cli/cli_user.c
    src/cli/cli_var.c
    src/cli/cmd_dispose.c
    src/cli/cli_candidate.c
    src/init/scheduler.c
    src/init/init_d.c
    src/init/section_markers.c
    src/init/critical_default.c
)

add_library(lincli STATIC ${LINCLI_SRC})
target_include_directories(lincli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/init
)

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
