cmake_minimum_required(VERSION 3.10)
project(cli_init)

# Find all source files
file(GLOB SOURCES "*.c")

# Create executable - 使用父级传递的变量
add_executable(${OUTPUT_NAME} ${SOURCES})

# Include directories
target_include_directories(${OUTPUT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)

# Link libraries
target_link_libraries(${OUTPUT_NAME} PRIVATE cli_core cli_lib)

# Set linker script
set_target_properties(${OUTPUT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)
