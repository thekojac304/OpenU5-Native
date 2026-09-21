#pragma once

#include <algorithm>
#include <cstddef>
#include <array>
#include <cstdint>

#include "esp_err.h"
#include "openu5/state.h"
#include "openu5/frontend.h"
#include "openu5/hud.h"
#include "openu5/turn.h"
#include "openu5/ui_session.h"
#include "device_ui_views.h"

namespace tdeck {

constexpr size_t kDebugScreenRows = 9;
constexpr size_t kAlphaTranscriptLines = openu5::kHudTranscriptLines;
// Shop and selector panels reserve a fixed-size transcript log strip that
// does not resize with the text-size setting (device_ui_views.h/tdeck_board.cpp).
constexpr size_t kShopLogRows = 7;
constexpr size_t kSelectorLogRows = 5;

// Shared by the world/dialogue transcript panel's renderer and by the
// input-routing page-geometry mirror (AlphaRuntime::refresh_session_context())
// so a single Shift+Up/Shift+Down press pages by exactly what is on screen.
// Both the text-size setting and whether a context bar is reserving space at
// the bottom change how many rows/columns actually fit (Batch 4.5C).
inline void world_transcript_geometry(uint8_t ui_size, bool context_active,
                                       size_t &columns, size_t &rows) {
    const auto text_metrics = ui_text_metrics(ui_size);
    columns = size_t(openu5::kHudRightW / text_metrics.cell_width);
    const int transcript_bottom = context_active ? kContextBarTop : 240; // 240: physical display height.
    rows = std::min<size_t>(kAlphaTranscriptLines,
        size_t((transcript_bottom - openu5::kHudTranscriptY) / text_metrics.line_height));
}
struct DeviceDebugScreen {
    char breadcrumb[48]{};
    char position[24]{};
    char rows[kDebugScreenRows][52]{};
    size_t row_count = 0;
    size_t selected_row = 0;
    char status[52]{};
};

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
    void show_runtime_identity(const char *firmware, const char *git_commit,
                               const char *build_timestamp, const char *alpha_identity,
                               const char *asset_identity, bool packs_match);
    esp_err_t show_view(const uint16_t *pixels, int width, int height,
                        const char *coordinates, const char *location, bool first_draw);
    esp_err_t show_alpha(const uint16_t *pixels, const openu5::UiSession &,
                         const openu5::GameState &, const openu5::TurnState &,
                         const openu5::HudWorldState &, const uint8_t *runes_font,
                         const char *overlay = nullptr,
                         const uint8_t *animated_cells = nullptr,
                         bool animation_only = false,
                         const DeviceDebugScreen *debug = nullptr,
                         bool movement_mode = false,
                         uint8_t ui_size = 1,
                         const DeviceShopView *shop = nullptr,
                         const DeviceSelectionView *selection = nullptr,
                         const DeviceContextActionBar *context_bar = nullptr,
                         DevicePartyHighlight party_highlight = {},
                         uint32_t viewport_crc = 0,
                         // R-05: while a dungeon view is mounted the two 9 px
                         // strips over the viewport carry the dungeon's own
                         // level and facing instead of a blank sky and
                         // "Wind: --".  Null or inactive = the world bars.
                         const openu5::HudDungeonBands *dungeon_bands = nullptr,
                         // R-17/Y-14: true for the View Gem presentation (world
                         // or dungeon) -- a full-square 176x176 composition
                         // that must not lose its top/bottom 9 px to the sky
                         // and wind strips, nor have them overdrawn across it.
                         bool full_square_viewport = false);
    esp_err_t show_frontend(const openu5::FrontendView &, const uint16_t *preview = nullptr,
                            const uint16_t *title_art = nullptr,
                            const uint16_t *panel_art = nullptr,
                            const uint16_t *creation_art = nullptr,
                            uint8_t ui_size = 1);
    esp_err_t set_brightness(uint8_t percent);
    bool debug_last_full_redraw() const { return debug_last_full_redraw_; }
    size_t debug_last_dirty_regions() const { return debug_last_dirty_regions_; }
    size_t debug_last_pixels() const { return debug_last_pixels_; }

private:
    esp_err_t initialize_shared_spi();
    esp_err_t write_display_command(uint8_t command, const uint8_t *data = nullptr,
                                    size_t data_length = 0);
    esp_err_t set_display_window(int x, int y, int width, int height);
    esp_err_t fill_rect(int x, int y, int width, int height, uint16_t color);
    esp_err_t draw_rgb565(int x, int y, int width, int height, const uint16_t *pixels);
    esp_err_t draw_rgb565_strided(int x, int y, int width, int height,
                                  const uint16_t *pixels, int stride);
    esp_err_t draw_rgb565_scaled(int x, int y, int width, int height,
                                 const uint16_t *pixels, int source_width,
                                 int source_height, int source_stride);
    esp_err_t draw_text(int x, int y, const char *text, uint16_t color, int scale);
    esp_err_t draw_text_box(int x, int y, int width, int height, const char *text,
                            uint16_t color, int scale_x = 1, int scale_y = 1,
                            bool invert = false);
    esp_err_t draw_text_box_metrics(int x,int y,int width,int height,const char *text,
                                    uint16_t color,DeviceTextMetrics metrics);
    esp_err_t draw_shared_bus_marker(int pass);

    void *display_device_ = nullptr;
    // Sole app task; synchronous spi_device_transmit completes before reuse.
    alignas(4) std::array<uint8_t, 320 * 2> transfer_row_{};
    bool shared_spi_initialized_ = false;
    bool display_initialized_ = false;
    bool backlight_pwm_initialized_ = false;
    bool alpha_drawn_ = false;
    bool frontend_drawn_ = false;
    bool debug_drawn_ = false;
    DeviceDebugScreen debug_cache_{};
    bool debug_cache_valid_ = false;
    bool debug_last_full_redraw_ = false;
    size_t debug_last_dirty_regions_ = 0;
    size_t debug_last_pixels_ = 0;
    struct CachedTranscriptLine {
        uint32_t sequence = 0;
        uint16_t color = 0;
        char text[openu5::kUiRenderedLineBytes]{};
    } transcript_cache_[kAlphaTranscriptLines]{};
    openu5::UiRenderedLine transcript_lines_[kAlphaTranscriptLines]{};
    char status_cache_[24]{};
    char mode_cache_[24]{};
    char prompt_cache_[32]{};
    char input_cache_[32]{};
    bool alpha_ui_cache_valid_ = false;
    uint8_t alpha_ui_size_cache_ = 0xff;
    bool viewport_cache_valid_ = false;
    uint32_t viewport_crc_ = 0;
    uint32_t sky_bar_signature_ = 0;
    bool sky_bar_cache_valid_ = false;
    // R-17/Y-14: whether the last frame drew the gem view's own full-square
    // 176x176 composition (no sky/wind strips). Toggling this forces both
    // caches so neither a stale bar nor a stale clipped-viewport rectangle
    // lingers across the transition either way.
    bool full_square_active_ = false;
    char wind_bar_cache_[34]{};
    DeviceShopView shop_cache_{};
    bool shop_cache_valid_ = false;
    DeviceSelectionView selection_cache_{};
    bool selection_cache_valid_ = false;
    DeviceContextActionBar context_cache_{};
    bool context_cache_valid_ = false;
    struct FrontendCache {
        openu5::FrontendState state = openu5::FrontendState::EnterGame;
        openu5::FrontendViewKind kind = openu5::FrontendViewKind::Generic;
        char title[64]{};
        char subtitle[64]{};
        char lines[12][96]{};
        size_t line_count = 0;
        int selected_line = -1;
        char footer[96]{};
        const uint16_t *title_art = nullptr;
        bool preview = false;
        bool panel = false;
        bool creation_art = false;
        uint8_t ui_size = 0xff;
    } frontend_cache_{};
    bool frontend_cache_valid_ = false;
};

}  // namespace tdeck
