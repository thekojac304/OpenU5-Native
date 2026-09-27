cmake_minimum_required(VERSION 3.16)

# This is an independent ESP-IDF project; no web-project tooling is required.
set(IDF_TARGET "esp32s3" CACHE STRING "Target chip")
option(OPENU5_ENABLE_DEVELOPER_TOOLS "Build the Alpha developer menu" ON)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "The T-Deck Plus target requires IDF_TARGET=esp32s3")
endif()

# Display/log identity only (the identity screen's "FW" line and the Launcher
# file name); no save, pack or gameplay code reads it. Batch 54 set
# 2.0.0-alpha2-rc1-debug for the Alpha 2 release image. A3-01 is an Alpha 3
# DEVELOPMENT build -- not an Alpha 3 release -- and says so here.
set(PROJECT_VER "3.0.0-alpha3-dev-a3-hf3-debug")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(openu5_tdeck)
