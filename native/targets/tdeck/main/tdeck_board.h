#pragma once

#include <algorithm>
#include <cstddef>
#include <array>
#include <cstdint>

#include "esp_err.h"
#include "openu5/state.h"
#include "openu5/endgame_scene.h"
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
// A4-UI4 hardware follow-up (section 8.22): sized for the smallest text's rows
// (21), not the 8 px cell's 19, so Small fills the console to the glass too.
constexpr size_t kAlphaTranscriptLines = kConsoleMaxRows;
static_assert(openu5::kHudTranscriptY == 88, "kConsoleMaxRows counts its rows from y 88");
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

// Alpha 3 A3-04F (ALPHA3_AUDIO.md section 26): how often each draw primitive
// ran since the last reset. Counting only; the host render census reads it.
struct BoardDrawCalls {
    uint32_t fill_rects = 0, rgb565 = 0, text_boxes = 0, metric_text_boxes = 0, sky_strips = 0;
};

struct BedViewportRect { int x, y, width, height; };
constexpr BedViewportRect bed_viewport_rect() {
    return {openu5::kHudViewportX, openu5::kHudViewportY+openu5::kHudSkyBarH,
            openu5::kHudViewportW,
            openu5::kHudViewportH-openu5::kHudSkyBarH-openu5::kHudWindBarH};
}
class Board {
public:
    // Alpha 4 A4-UI4: the release the boot screens name (device_ui_views.h
    // firmware_release_label); set before initialize_display().
    void set_release_label(const char *label) { release_label_ = label; }
    esp_err_t initialize_display();
    SdStatus initialize_and_test_sd();
    // BOOT2: the clock the card is mounted at (kHz), and a one-shot retreat to the
    // safe clock for a pack that failed to validate on the fast one.
    int sd_clock_khz() const { return sd_clock_khz_; }
    bool fall_back_sd_clock();
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
    // Alpha 4 A4-END1 (ALPHA4_UI.md section 7). ENDGAME.OVL's full-screen
    // frames. Its screen is 320 x 200; the T-Deck shows it at y 20, with the
    // 20 rows above and below black.
    //  - show_endgame_page: one of the pack's 4bpp screens (a story page or
    //    the scroll) through `palette`, with endgame_datestamp's 40 x 25 cells
    //    (`cells`, may be null) over it, opaque, as put_char 0x16ba draws them
    //    (IBM.CH or RUNES.CH; reverse video = black ink on white). Redrawn
    //    only when `key` changes, or when `force`d.
    //  - the fizzle dissolves what is on the panel, which the Board does not
    //    keep: begin_endgame_capture() mirrors every pixel it sends into a
    //    PSRAM copy of the screen (and makes the next show_alpha a full
    //    repaint); fizzle_endgame() blacks out the next `pixels` of fn34's order
    //    over the 320 x 200 picture -- and, in step, the device's two bands as
    //    their own 320 x 40 rect -- and sends the screen.
    esp_err_t show_endgame_page(const uint8_t *page4bpp, const uint16_t *palette,
                                const openu5::EndgameScrollCell *cells, const uint8_t *normal_font,
                                const uint8_t *rune_font, int key, bool force);
    bool begin_endgame_capture();
    void end_endgame_capture() { endgame_capturing_ = false; }
    esp_err_t fizzle_endgame(openu5::EndgameFizzle &picture, openu5::EndgameFizzle &bands, uint32_t pixels);
    void release_endgame_capture();
    const uint16_t *endgame_capture_for_test() const { return endgame_shadow_; }
    // Alpha 4 UI Batch 1 (ALPHA4_UI.md): IBM.CH, the game's 8x8 font, for the
    // fixed chrome only -- band captions, the roster, the food/gold/date box.
    // The runtime hands it over before every draw; the transcript's text never
    // uses it (A4-UI4: its wait cursor does, below).
    // Without it the chrome falls back to the 5x7 font (nothing is refused).
    void set_chrome_font(const uint8_t *ibm8x8) { ibm_font_ = ibm8x8; }
    // Alpha 4 A4-UI4 (ALPHA4_UI.md section 8.20): the console's wait cursor,
    // poll_key_blink_cursor 0x1b38's flame wave (IBM.CH 0x05 + phase, from the
    // pack's font; none without it). show_alpha places it where the session's
    // console_cursor() says; the runtime sets the phase before every draw, and
    // animate_console_cursor() redraws the cursor's one cell when the phase
    // moved and nothing else is being drawn.
    struct ConsoleCursorState {
        int16_t x = 0, y = 0;
        uint8_t width = 0, height = 0, phase = 0xff;
        bool visible = false;
    };
    void set_console_cursor_phase(uint8_t phase) { console_phase_ = uint8_t(phase & 3U); }
    esp_err_t animate_console_cursor();
    const ConsoleCursorState &console_cursor_state() const { return console_cursor_; }
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
    const BoardDrawCalls &draw_calls() const { return draw_calls_; }
    void reset_draw_calls() { draw_calls_ = {}; }

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
    /** A4-END1: copy a pixel transaction into the capture at the window's cursor. */
    void mirror_pixels(const uint8_t *bytes, size_t count);
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
    /**
     * A3-04F (ALPHA3_AUDIO.md section 26): the row loops build whole rows
     * back to back in transfer_row_ and send them as one transaction -- the
     * panel's window wraps each row into the next. `used` counts this row's
     * bytes too. A batch ends with the rectangle, when the next row would not
     * fit (640 B, the bus's max_transfer_sz), or at a pause row, so a pause
     * still falls after row 16, 32, ... has gone out.
     */
    bool row_batch_ends(size_t used, size_t row_bytes, int row, int height) const {
        return row + 1 == height || used + row_bytes > transfer_row_.size() || openu5::tft_row_yield_due(row);
    }
    /** `force`: draw every row whatever it last showed (the panel was painted over). */
    esp_err_t draw_party_rows(const openu5::GameState &, DevicePartyHighlight, bool force);
    // Alpha 4 UI Batch 1: which font a chrome row is drawn in.
    enum class ChromeFont : uint8_t { Compact, Ibm };
    /**
     * A3-04F: one 8-px row of the right panel's party / status block, drawn only
     * when its text, colour or reverse video differs from what that row last
     * drew, or when `force`d. Slots 0-5 are the party, 6 the location (and
     * MOVE), 7 food/gold, 8 the date and clock (Alpha 4 UI Batch 1). Characters
     * from `accent_from` on are drawn in `accent`.
     */
    esp_err_t draw_panel_row(size_t slot, int y, const char *text, uint16_t color, bool invert, bool force,
                             ChromeFont font = ChromeFont::Compact, size_t accent_from = SIZE_MAX,
                             uint16_t accent = 0);
    /** Alpha 4 UI Batch 1: the status box's three rows, shared by show_alpha and the bed refresh. */
    esp_err_t draw_status_rows(const openu5::GameState &, const char *place, bool move, bool force);
    /** One window of 6-px (Compact) or 8-px (IBM.CH) cells; text starts at `text_x`. */
    esp_err_t draw_cells(int x, int y, int width, int height, int text_x, const char *text, uint16_t color,
                         size_t accent_from, uint16_t accent, bool invert, ChromeFont font);
    /**
     * A band of the frame with the original's notch and >< ends (bandBracket.ts):
     * `rule_row` (or -1) is a white rule row; the other rows are the band. The
     * notch holds `caption` in IBM.CH, or the 12-cell sky track of `sky`.
     */
    esp_err_t draw_band_strip(int x, int y, int width, int height, int rule_row, const char *caption,
                              const openu5::HudWorldState *sky, const uint8_t *runes_font);
    /** The frontend shell: blue band, >title< caption, white-ruled content window. */
    esp_err_t draw_frontend_shell(const char *title);
    bool chrome_glyph_bit(char c, int row, int col) const;
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
    // Alpha 4 UI Batch 1: `invert` is draw_text_box's reverse video (kernel
    // 0x2a28's look) and `top_pad` the background rows above the glyphs, for
    // the menus' selection bar. The defaults draw exactly what they always did.
    esp_err_t draw_text_box_metrics(int x,int y,int width,int height,const char *text,
                                    uint16_t color,DeviceTextMetrics metrics,
                                    bool invert = false,int top_pad = 0);
    esp_err_t draw_shared_bus_marker(int pass);

    void *display_device_ = nullptr;
    openu5::TftTiming tft_timing_{};
    uint32_t tft_cpu_mhz_ = 0; // set with the display; 0 on the host (no timing)
    const volatile uint32_t *audio_active_ = nullptr;
    const volatile uint32_t *sd_active_ = nullptr;
    openu5::TftPacing tft_pacing_ = openu5::kPacingDefault.tft;
    IdleService *idle_ = nullptr;
    BoardDrawCalls draw_calls_{};
    // Sole app task; synchronous spi_device_transmit completes before reuse.
    alignas(4) std::array<uint8_t, 320 * 2> transfer_row_{};
    // A4-END1: the fizzle's copy of the panel (PSRAM, only while it runs), the
    // window the next pixels land in, and the ending page last drawn.
    uint16_t *endgame_shadow_ = nullptr;
    bool endgame_capturing_ = false;
    int16_t window_x_ = 0, window_y_ = 0, window_w_ = 0, window_h_ = 0, cursor_x_ = 0, cursor_y_ = 0;
    int endgame_page_key_ = -1;
    bool shared_spi_initialized_ = false;
    SdStatus mount_and_test_sd(int khz);
    void unmount_sd();
    void *sd_card_ = nullptr; // sdmmc_card_t *, owned by the VFS mount
    int sd_clock_khz_ = 0;
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
    // A3-04F: what each party / status row last drew (draw_panel_row). Only a
    // frame with alpha_ui_cache_valid_ set may skip one: everything that paints
    // over the right panel clears that flag.
    struct CachedPanelRow {
        char text[24]{};
        uint16_t color = 0;
        bool invert = false;
        bool valid = false;
    } panel_rows_[9]{};
    // A3-04F: the transcript's per-frame scratch (1,976 B) is a local of
    // show_alpha now, on the main task's stack, and four caches nothing read or
    // wrote (112 B) are gone: the retained panel rows above cost internal RAM,
    // and section 23.10.5 left the internal heap's low-water mark unexplained.
    bool alpha_ui_cache_valid_ = false;
    uint8_t alpha_ui_size_cache_ = 0xff;
    // Alpha 4 UI Batch 1: the chrome font (owned by the resource pack) and the
    // Movement Mode the status box last showed, so the bed refresh draws the
    // same caption show_alpha would.
    const uint8_t *ibm_font_ = nullptr;
    const char *release_label_ = nullptr;
    ConsoleCursorState console_cursor_{}; // A4-UI4 (section 8.20)
    uint8_t console_phase_ = 0;
    esp_err_t draw_console_cursor_cell();
    bool status_move_ = false;
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
        bool shell = false; // Alpha 4 UI Batch 1: the band shell was drawn
        uint8_t ui_size = 0xff;
        uint8_t selected_span = 1; // A4-UI3: a slot page selects two rows
    } frontend_cache_{};
    bool frontend_cache_valid_ = false;
};

}  // namespace tdeck
