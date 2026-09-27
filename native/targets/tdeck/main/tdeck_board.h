#pragma once

#include <algorithm>
#include <cstddef>
#include <array>
#include <cstdint>

#include "esp_err.h"
#include "openu5/state.h"
#include "openu5/frontend.h"
#include "openu5/hud.h"
#include "openu5/perf_report.h"
#include "openu5/render_pacing.h"
#include "openu5/turn.h"
#include "openu5/ui_session.h"
#include "device_ui_views.h"

struct spi_transaction_t;
namespace tdeck {
class IdleService;

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

struct BedViewportRect { int x, y, width, height; };
constexpr BedViewportRect bed_viewport_rect() {
    return {openu5::kHudViewportX, openu5::kHudViewportY+openu5::kHudSkyBarH,
            openu5::kHudViewportW,
            openu5::kHudViewportH-openu5::kHudSkyBarH-openu5::kHudWindBarH};
}
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
                         bool full_square_viewport = false,
                         bool preserve_party_panel = false);
    esp_err_t show_frontend(const openu5::FrontendView &, const uint16_t *preview = nullptr,
                            const uint16_t *title_art = nullptr,
                            const uint16_t *panel_art = nullptr,
                            const uint16_t *creation_art = nullptr,
                            uint8_t ui_size = 1);
    esp_err_t set_brightness(uint8_t percent);
    // CMDS bed entry: fill only the map image; the relocated sky/wind strips
    // and viewport frame remain visible. The next normal draw restores it.
    esp_err_t fill_bed_viewport();
    // OUTSUBS Camp apparition writes only the 176x176 game window.
    esp_err_t show_camp_viewport(const uint16_t *pixels, uint32_t viewport_crc);
    // CMDS 0x060a/0x0674: update the status panel without touching the map.
    esp_err_t refresh_bed_status_panel(const openu5::GameState &, DevicePartyHighlight);
    bool debug_last_full_redraw() const { return debug_last_full_redraw_; }
    size_t debug_last_dirty_regions() const { return debug_last_dirty_regions_; }
    size_t debug_last_pixels() const { return debug_last_pixels_; }
    // Alpha 3 A3-04C (ALPHA3_AUDIO.md section 20): what the TFT write cost,
    // transaction by transaction, since the last take (the runtime takes it
    // around every gameplay frame). `flag` is the audio task's "running"
    // word (TdeckAudioBackend::activity_flag), read at each row's two ends.
    void set_audio_activity_flag(const volatile uint32_t *flag) { audio_active_ = flag; }
    // A3-04D (section 21): the SD-log writer's burst word (sdlog::burst_flag()),
    // read at both ends of every transaction; the SD card shares this SPI bus.
    void set_sd_activity_flag(const volatile uint32_t *flag) { sd_active_ = flag; }
    void take_tft_timing(openu5::TftTiming &out) {
        out = tft_timing_;
        out.cpu_mhz = tft_cpu_mhz_;
        tft_timing_ = openu5::TftTiming{};
    }
    // A3-04E (ALPHA3_AUDIO.md section 22): what the draw loops' pause every
    // 16 rows does -- a yield (A3-04E) or a sleep to the next tick (legacy).
    // The runtime sets it before every draw from its (never saved) policy.
    void set_tft_pacing(openu5::TftPacing pacing) { tft_pacing_ = pacing; }
    openu5::TftPacing tft_pacing() const { return tft_pacing_; }
    // A3-04E.1 (section 23): the idle-service guarantee the yield pause
    // consults (a tick sleep when core 0's idle loop has not run for 200 ms).
    void set_idle_service(IdleService *service) { idle_ = service; }

private:
    // A3-04C: every TFT transaction and every draw-loop yield goes through
    // these, so the split (row building / SPI transfer / tick yield) and
    // the audio-running classification cover the whole write.
    struct RowMark {
        uint32_t cycles = 0;
        bool busy = false;
    };
    bool audio_running() const { return audio_active_ && *audio_active_ != 0; }
    bool sd_log_burst() const { return sd_active_ && *sd_active_ != 0; }
    RowMark row_mark() const;
    /** One pixel row (or fill chunk) that started building at `start`. */
    esp_err_t tft_row(spi_transaction_t &transaction, RowMark start);
    /** A command / window-setup transaction (no row to build). */
    esp_err_t tft_command(spi_transaction_t &transaction);
    esp_err_t tft_transmit(spi_transaction_t &transaction, const RowMark *start);
    /**
     * The draw loops' pause every 16 rows / 32 fill chunks (Alpha 2.0's
     * cadence). A3-04E: taskYIELD, not vTaskDelay(1) -- each row already
     * blocks in spi_device_transmit (the idle task runs there) and the input
     * task outranks this one, so the tick sleep only made every band wait.
     */
    void tft_yield();
    esp_err_t draw_party_rows(const openu5::GameState &, DevicePartyHighlight);
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
    openu5::TftTiming tft_timing_{};
    uint32_t tft_cpu_mhz_ = 0; // set with the display; 0 on the host (no timing)
    const volatile uint32_t *audio_active_ = nullptr;
    const volatile uint32_t *sd_active_ = nullptr;
    openu5::TftPacing tft_pacing_ = openu5::kPacingDefault.tft;
    IdleService *idle_ = nullptr;
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
