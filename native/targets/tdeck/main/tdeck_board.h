#pragma once

#include <cstddef>
#include <array>
#include <cstdint>

#include "esp_err.h"

namespace tdeck {

struct SdStatus {
    bool ok = false;
    bool used_existing_test_file = false;
    bool shared_bus_verified = false;
    uint64_t capacity_bytes = 0;
    const char *type = "NONE";
    esp_err_t error = ESP_FAIL;
};

class Board {
public:
    esp_err_t initialize_display();
    SdStatus initialize_and_test_sd();
    void show_diagnostics(bool sd_ok);
    esp_err_t show_view(const uint16_t *pixels, int width, int height,
                        const char *coordinates, const char *location, bool first_draw);

private:
    esp_err_t initialize_shared_spi();
    esp_err_t write_display_command(uint8_t command, const uint8_t *data = nullptr,
                                    size_t data_length = 0);
    esp_err_t set_display_window(int x, int y, int width, int height);
    esp_err_t fill_rect(int x, int y, int width, int height, uint16_t color);
    esp_err_t draw_rgb565(int x, int y, int width, int height, const uint16_t *pixels);
    esp_err_t draw_text(int x, int y, const char *text, uint16_t color, int scale);
    esp_err_t draw_shared_bus_marker(int pass);

    void *display_device_ = nullptr;
    // Sole app task; synchronous spi_device_transmit completes before reuse.
    alignas(4) std::array<uint8_t, 320 * 2> transfer_row_{};
    bool shared_spi_initialized_ = false;
    bool display_initialized_ = false;
};

}  // namespace tdeck
