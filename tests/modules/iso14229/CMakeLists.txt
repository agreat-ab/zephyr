# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Enable Kconfig discovery for this test app
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Pull in Zephyr and set up the project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iso14229_tests)

# Make sure CMake can see the iso14229 module inside Zephyr
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/iso14229)

# Collect all test source files automatically
file(GLOB app_sources
    src/*.c
    mock_log_backend.c
)

# Every Zephyr app must link against Zephyr
target_link_libraries(app PRIVATE zephyr)

# Add all sources to the test target
target_sources(app PRIVATE ${app_sources})

# Include directories
zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ZEPHYR_BASE}/modules/iso14229/include
)
