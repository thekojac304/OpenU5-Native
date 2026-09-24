#include "../../main/tdeck_board.h"
#include "../../main/native_renderer.h"
#include <algorithm>
#include <array>

namespace {
std::array<uint16_t, 320 * 240> screen{};
int fills = 0;
int draws = 0;
}
void batch37_reset_screen() {
    screen.fill(0x1357);
    fills = draws = 0;
}
uint16_t batch37_pixel(int x, int y) { return screen[size_t(y) * 320 + size_t(x)]; }
int batch37_fill_count() { return fills; }
int batch37_draw_count() { return draws; }
bool batch37_map_black() {
    const auto r=tdeck::bed_viewport_rect();
    if (r.x!=4 || r.y!=13 || r.width!=176 || r.height!=158) return false;
    for (int y=r.y;y<r.y+r.height;++y)
        for (int x=r.x;x<r.x+r.width;++x)
            if (screen[size_t(y)*320+size_t(x)]!=0) return false;
    return true;
}

namespace tdeck {
esp_err_t Board::initialize_display() { return ESP_OK; }
SdStatus Board::initialize_and_test_sd() { return {}; }
void Board::show_diagnostics(bool) {}
void Board::show_runtime_identity(const char *, const char *, const char *, const char *, const char *, bool) {}
esp_err_t Board::show_view(const uint16_t *, int, int, const char *, const char *, bool) { return ESP_OK; }
esp_err_t Board::show_alpha(const uint16_t *pixels, const openu5::UiSession &, const openu5::GameState &,
                            const openu5::TurnState &, const openu5::HudWorldState &, const uint8_t *,
                            const char *, const uint8_t *, bool, const DeviceDebugScreen *, bool,
                            uint8_t, const DeviceShopView *, const DeviceSelectionView *,
                            const DeviceContextActionBar *, DevicePartyHighlight, uint32_t,
                            const openu5::HudDungeonBands *, bool) {
    if (!pixels) return ESP_ERR_INVALID_ARG;
    ++draws;
    // Mirror Board's physical viewport placement and the sky/wind overlays.
    for (int y = 0; y < openu5::kViewportPixels; ++y)
        for (int x = 0; x < openu5::kViewportPixels; ++x)
            screen[size_t(y + 4) * 320 + size_t(x + 4)] =
                y < 9 || y >= 167 ? 0x07ff : pixels[size_t(y) * 176 + size_t(x)];
    return ESP_OK;
}
esp_err_t Board::show_frontend(const openu5::FrontendView &, const uint16_t *, const uint16_t *,
                                const uint16_t *, const uint16_t *, uint8_t) { return ESP_OK; }
esp_err_t Board::fill_bed_viewport() {
    ++fills;
    const auto r=bed_viewport_rect();
    for (int y = r.y; y < r.y + r.height; ++y)
        for (int x = r.x; x < r.x + r.width; ++x)
            screen[size_t(y) * 320 + size_t(x)] = 0;
    return ESP_OK;
}
esp_err_t Board::set_brightness(uint8_t) { return ESP_OK; }
}
