cmake_minimum_required(VERSION 3.10)
project(cli_project)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

# ============================================================
# LinCLI 框架静态库
# ============================================================
set(LINCLI_SRC
    lib/cli_string.c
    lib/cli_atoi.c
    lib/cli_float.c
    lib/cli_mpool.c
    lib/cli_vsnprintf.c
    lib/cli_errno.c
    lib/stateM.c
    lib/tVector.c
    cli/Src/cli_io.c
    cli/Src/cli_cmd_line.c
    cli/Src/cli_completion.c
    cli/Src/cli_edit.c
    cli/Src/cli_env.c
    cli/Src/cli_history.c
    cli/Src/cli_logo.c
    cli/Src/cli_parse.c
    cli/Src/cli_user.c
    cli/Src/cli_var.c
    cli/Src/cmd_dispose.c
    cli/Src/cli_candidate.c
)

add_library(lincli STATIC ${LINCLI_SRC})
target_include_directories(lincli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# ============================================================
# PC 端演示可执行文件
# ============================================================
set(OUTPUT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cli.ld ${CMAKE_BINARY_DIR}/cli.ld COPYONLY)

add_executable(${OUTPUT_NAME}
    init/main.c
    init/scheduler.c
    init/init_d.c
    init/section_markers.c
    init/critical_default.c
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
    target_link_libraries(${OUTPUT_NAME} PRIVATE tests)
    add_test(NAME unit_tests COMMAND unit_tests)
endif()
