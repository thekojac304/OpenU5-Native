idf_component_register(
    SRCS "main.cpp" "tdeck_board.cpp" "asset_pack.cpp" "native_renderer.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_system esp_hw_support esp_psram heap spi_flash freertos esp_timer
                  esp_driver_gpio esp_driver_spi esp_driver_sdspi fatfs sdmmc
)
