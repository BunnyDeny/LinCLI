cmake_minimum_required(VERSION 3.10)

set(LINCLI_SRC
    lib/cli_string.c
    lib/cli_atoi.c
    lib/cli_float.c
    lib/cli_mpool.c
    lib/cli_vsnprintf.c
    lib/cli_errno.c
    lib/stateM.c
    lib/tVector.c
    cli/cli_io.c
    cli/cli_cmd_line.c
    cli/cli_completion.c
    cli/cli_edit.c
    cli/cli_env.c
    cli/cli_history.c
    cli/cli_logo.c
    cli/cli_parse.c
    cli/cli_user.c
    cli/cli_var.c
    cli/cmd_dispose.c
    cli/cli_candidate.c
    init/scheduler.c
    init/init_d.c
    init/section_markers.c
    init/critical_default.c
)

# OBJECT library: object files are passed directly to the linker,
# preventing custom section contents from being discarded by the
# static-library archive extraction logic.
add_library(lincli_obj OBJECT ${LINCLI_SRC})
target_include_directories(lincli_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/init
)

# INTERFACE wrapper so consumers can use target_link_libraries()
# without worrying about OBJECT-library quirks.
add_library(lincli INTERFACE)
target_sources(lincli INTERFACE $<TARGET_OBJECTS:lincli_obj>)
target_include_directories(lincli INTERFACE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/init
)
