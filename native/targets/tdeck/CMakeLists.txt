cmake_minimum_required(VERSION 3.16)

# This is an independent ESP-IDF project; no web-project tooling is required.
set(IDF_TARGET "esp32s3" CACHE STRING "Target chip")
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "The T-Deck Plus target requires IDF_TARGET=esp32s3")
endif()

set(PROJECT_VER "0.4.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(openu5_tdeck)
