include("${CMAKE_CURRENT_LIST_DIR}/../../../core/sources.cmake")
idf_component_register(
    SRCS "main.cpp" "sd_diagnostic_logger.cpp" "tdeck_board.cpp" "tdeck_input.cpp" "input_controller.cpp"
         "keyboard_matrix.cpp"
         "device_smoke_tests.cpp"
         "asset_pack.cpp" "native_renderer.cpp" "alpha_resources.cpp" "alpha_dialogue.cpp"
         "alpha_save.cpp" "alpha_save_generation.cpp" "alpha_runtime.cpp" "ui_input_adapter.cpp" "dungeon_art_cache.cpp"
         "alpha_audio.cpp" "tdeck_audio.cpp" "system_perf.cpp" "idle_service.cpp"
         ${OPENU5_CORE_SOURCES}
    INCLUDE_DIRS "." "../../../core/include"
    # A3-04B (ALPHA3_AUDIO.md section 19.8): the audio task's per-sample path in IRAM.
    LDFRAGMENTS "audio_iram.lf"
    PRIV_REQUIRES esp_system esp_app_format esp_hw_support esp_psram heap spi_flash freertos esp_timer
                  esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_driver_spi esp_driver_sdspi fatfs sdmmc
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wconversion -Werror
    # Keep per-function stack reports beside target objects. These are build
    # diagnostics only and do not change the firmware's runtime behavior.
    -fstack-usage)
# A3-04A (native/targets/tdeck/ALPHA3_AUDIO.md section 18.6): the OPL2 synth
# steps ~50,000 chip samples a second on the audio task, and at the project's
# -Og every operator step is an out-of-line call chain. This ONE file is built
# at -O2; -ffp-contract=off keeps its float results bit-identical to -Og (no
# fused multiply-add; -O2 enables no fast-math), so what plays is unchanged.
#
# A3-04B (section 19.8): the files whose per-sample functions audio_iram.lf
# puts in IRAM are built without jump tables -- GCC keeps a switch's table in
# .rodata, i.e. in flash behind the data cache the renderer shares, which is
# exactly what the IRAM placement is for. Code generation only: same results.
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../../../core/src/music_synth.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2;-ffp-contract=off;-fno-jump-tables")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../../../core/src/sfx_synth.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../../core/src/audio_stream.cpp"
    PROPERTIES COMPILE_OPTIONS "-fno-jump-tables")
# A4-END1 (ALPHA4_UI.md section 7.14): ENDGAME.OVL's sequencer runs once a
# playthrough and has no per-sample path (a 121-cell compose a frame, the
# fizzle's LFSR during the dissolve), so it is built for size: the 1 MiB app
# partition is the constraint. Code generation only: same results.
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../../../core/src/endgame_scene.cpp"
    PROPERTIES COMPILE_OPTIONS "-Os")
find_program(OPENU5_GIT_EXECUTABLE NAMES git.exe git
    PATHS "C:/Program Files/Git/cmd" NO_DEFAULT_PATH)
if(OPENU5_GIT_EXECUTABLE)
    execute_process(
        COMMAND "${OPENU5_GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../../.."
        OUTPUT_VARIABLE OPENU5_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    # A4-SAVE3: an image configured from uncommitted tracked changes says so
    # ("<hash>-dirty"), so it can never pass for a build of HEAD itself.
    # Untracked files (evidence logs) do not count.
    execute_process(
        COMMAND "${OPENU5_GIT_EXECUTABLE}" status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../../.."
        OUTPUT_VARIABLE OPENU5_GIT_CHANGES
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(OPENU5_GIT_COMMIT AND OPENU5_GIT_CHANGES)
        string(APPEND OPENU5_GIT_COMMIT "-dirty")
    endif()
endif()
if(NOT OPENU5_GIT_COMMIT)
    set(OPENU5_GIT_COMMIT "unknown")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OPENU5_GIT_COMMIT="${OPENU5_GIT_COMMIT}")
if(OPENU5_ENABLE_DEVELOPER_TOOLS)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC OPENU5_ENABLE_DEVELOPER_TOOLS=1)
endif()
