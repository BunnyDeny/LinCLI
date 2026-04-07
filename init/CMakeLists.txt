cmake_minimum_required(VERSION 3.10)
project(cli_init)

# Find all source files
file(GLOB SOURCES "*.c")

# Create executable
add_executable(cli_init ${SOURCES})

# Include directories
target_include_directories(cli_init PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cli/Inc
)

# Link libraries
target_link_libraries(cli_init PRIVATE cli_core cli_lib)
