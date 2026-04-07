cmake_minimum_required(VERSION 3.10)
project(cli_project)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(init)

# Linker script - set as linker flags for cli_init target
set_target_properties(cli_init PROPERTIES
    LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/default.ld"
)
