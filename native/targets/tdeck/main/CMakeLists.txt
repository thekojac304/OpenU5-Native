include("${CMAKE_CURRENT_LIST_DIR}/../../../core/sources.cmake")
idf_component_register(
    SRCS "main.cpp" "tdeck_board.cpp" "tdeck_input.cpp" "input_controller.cpp"
         "keyboard_matrix.cpp"
         "asset_pack.cpp" "native_renderer.cpp"
         ${OPENU5_CORE_SOURCES}
    INCLUDE_DIRS "." "../../../core/include"
    PRIV_REQUIRES esp_system esp_hw_support esp_psram heap spi_flash freertos esp_timer
                  esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_sdspi fatfs sdmmc
)
