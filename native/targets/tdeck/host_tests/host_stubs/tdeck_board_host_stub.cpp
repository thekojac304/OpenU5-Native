// Batch 11 host-test seam.
//
// Harmless-sink stand-in for tdeck::Board's PUBLIC interface (tdeck_board.h,
// unmodified) so alpha_runtime.cpp links on host without pulling in the real
// display/SPI/SD driver implementation (tdeck_board.cpp), which needs actual
// T-Deck hardware. AlphaRuntime::command()/dispatch()/handle() -- the seam
// Batch 11 host-tests exercise -- never call Board at all (only render() and
// initialize() do, and host tests call neither); this stub exists so the
// translation unit links even if a future test reaches those paths, without
// ever touching real pixels or GPIO.
//
// Never linked into the T-Deck firmware: only this CMake host test target
// compiles it. tdeck_board.cpp (the real device implementation) is untouched
// and still the only Board implementation main/CMakeLists.txt builds.
#include "tdeck_board.h"

namespace tdeck {

esp_err_t Board::initialize_display() { return ESP_OK; }

SdStatus Board::initialize_and_test_sd() {
    SdStatus s;
    s.ok = false;
    s.error = ESP_ERR_NOT_SUPPORTED;
    s.type = "HOST-STUB";
    return s;
}

void Board::show_diagnostics(bool) {}

void Board::show_runtime_identity(const char *, const char *, const char *, const char *,
                                   const char *, bool) {}

esp_err_t Board::show_view(const uint16_t *, int, int, const char *, const char *, bool) {
    return ESP_OK;
}

esp_err_t Board::show_alpha(const uint16_t *, const openu5::UiSession &, const openu5::GameState &,
                             const openu5::TurnState &, const openu5::HudWorldState &, const uint8_t *,
                             const char *, const uint8_t *, bool, const DeviceDebugScreen *, bool,
                             uint8_t, const DeviceShopView *, const DeviceSelectionView *,
                             const DeviceContextActionBar *, DevicePartyHighlight, uint32_t,
                             const openu5::HudDungeonBands *, bool) {
    return ESP_OK;
}

esp_err_t Board::show_frontend(const openu5::FrontendView &, const uint16_t *, const uint16_t *,
                                const uint16_t *, const uint16_t *, uint8_t) {
    return ESP_OK;
}

esp_err_t Board::fill_bed_viewport() { return ESP_OK; }
esp_err_t Board::set_brightness(uint8_t) { return ESP_OK; }

} // namespace tdeck
