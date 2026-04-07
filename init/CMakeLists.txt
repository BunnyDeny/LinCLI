cmake_minimum_required(VERSION 3.10)
project(cli_init)

# Find all source files
file(GLOB SOURCES "*.c")

# Create executable - 使用父级传递的变量
add_executable(${CLI_INIT_NAME} ${SOURCES})

# Include directories
target_include_directories(${CLI_INIT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)

# Link libraries
target_link_libraries(${CLI_INIT_NAME} PRIVATE cli_core cli_lib)

# Set linker script
set_target_properties(${CLI_INIT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)
