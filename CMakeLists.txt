cmake_minimum_required(VERSION 3.10)
project(cli_project)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(OUTPUT_NAME "a.out")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/default.ld")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(MODULES lib cli init)

foreach(mod ${MODULES})
    add_subdirectory(${mod})
endforeach()

add_executable(${OUTPUT_NAME} ${CMAKE_SOURCE_DIR}/init/main.c)
target_link_libraries(${OUTPUT_NAME} PRIVATE cli_lib cli_core cli_init)
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)
