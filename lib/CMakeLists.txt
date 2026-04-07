cmake_minimum_required(VERSION 3.10)
project(cli_lib)

# Find all source files
file(GLOB SOURCES "*.c")

# Create static library
add_library(cli_lib STATIC ${SOURCES})

# Include directories
target_include_directories(cli_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
