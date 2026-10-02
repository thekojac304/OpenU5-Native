cmake_minimum_required(VERSION 3.16)

# This is an independent ESP-IDF project; no web-project tooling is required.
set(IDF_TARGET "esp32s3" CACHE STRING "Target chip")
option(OPENU5_ENABLE_DEVELOPER_TOOLS "Build the Alpha developer menu" ON)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "The T-Deck Plus target requires IDF_TARGET=esp32s3")
endif()

# Display/log identity only (the identity screen's "FW" line and the Launcher
# file name); no save, pack or gameplay code reads it. Batch 54 set
# 2.0.0-alpha2-rc1-debug for the Alpha 2 release image. The Alpha 3 RC1 batch
# sets 3.0.0-alpha3-rc1-debug: a release CANDIDATE (hardware smoke pending),
# not a final Alpha 3 release. A3-01 .. A3-HF10 were development builds.
# Alpha 4 UI Batch 1 sets 4.0.0-alpha4-ui1-debug (ALPHA4_UI.md); A4-UI3 (the
# save/load UX, ALPHA4_UI.md section 6) set 4.0.0-alpha4-ui3-debug; A4-END1
# (the full ending, ALPHA4_UI.md section 7) set 4.0.0-alpha4-end1-debug;
# A4-UI4/PRES1 (frontend and presentation, ALPHA4_UI.md section 8) sets
# 4.0.0-alpha4-ui4-debug; its hardware follow-up (section 8.22: the console
# height) sets 4.0.0-alpha4-ui4-hf1-debug, so the retest image never shares
# a name or an FW line with the first A4-UI4 image. A4-PARITY1 (section 9)
# set 4.0.0-alpha4-parity1-debug. The Alpha 4 RC1 sets 4.0.0-alpha4-rc1-debug:
# a release CANDIDATE (its hardware session, section 9.18, is pending), not a
# final Alpha 4 release. A4-ENH1 (the trackball, Developer entry, cheats and
# difficulty pass, section 10) sets 4.0.0-alpha4-enh1-debug, so its test image
# never shares a name or an FW line with the RC1 image.
set(PROJECT_VER "4.0.0-alpha4-enh1-debug")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(openu5_tdeck)
