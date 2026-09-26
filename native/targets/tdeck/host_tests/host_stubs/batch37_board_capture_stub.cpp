#include "../../main/tdeck_board.h"
#include "../../main/native_renderer.h"
#include "esp_timer.h"
#include <algorithm>
#include <array>
#include <vector>

namespace {
std::array<uint16_t, 320 * 240> screen{};
std::vector<std::array<uint16_t,176*176>> frames;
// Batch 51: the (virtual) presentation instant of every captured frame.
std::vector<int64_t> frame_times;
std::vector<std::array<uint16_t,7>> camp_panels;
int fills = 0;
int draws = 0;
int world_draws = 0;
int ui_draws = 0;
int panel_draws = 0;
uint16_t first_panel_map_pixel = 0;
void capture_panel(const openu5::GameState &game, tdeck::DevicePartyHighlight highlight) {
    for (int row = 0; row < 6; ++row) {
        const auto &member = game.party.characters[row];
        screen[size_t(4 + row * 8) * 320 + 200] = row < game.party.party_size ? uint16_t(member.status) : 0;
        screen[size_t(4 + row * 8) * 320 + 201] = row < game.party.party_size ? member.current_hp : 0;
        screen[size_t(4 + row * 8) * 320 + 202] = highlight.selected == row ? 1 : 0;
    }
    screen[68 * 320 + 200] = uint16_t(game.time.hour * 60 + game.time.minute);
}
}
void batch37_reset_screen() {
    screen.fill(0x1357);
    frames.clear();
    frame_times.clear();
    camp_panels.clear();
    fills = draws = panel_draws = world_draws = ui_draws = 0;
    first_panel_map_pixel = 0;
}
uint16_t batch37_pixel(int x, int y) { return screen[size_t(y) * 320 + size_t(x)]; }
size_t batch37_frame_count() { return frames.size(); }
int64_t batch37_frame_time_us(size_t frame) { return frame<frame_times.size()?frame_times[frame]:-1; }
uint16_t batch37_frame_pixel(size_t frame,int x,int y) {
    return frame<frames.size()&&x>=0&&x<176&&y>=0&&y<176
        ?frames[frame][size_t(y)*176+size_t(x)]:0xffff;
}
int batch37_fill_count() { return fills; }
int batch37_draw_count() { return draws; }
int batch37_world_draw_count() { return world_draws; }
int batch37_ui_draw_count() { return ui_draws; }
int batch37_panel_draw_count() { return panel_draws; }
uint16_t batch37_panel_value(size_t draw,int slot) {
    return draw<camp_panels.size()&&slot>=0&&slot<7?camp_panels[draw][slot]:0xffff;
}
uint16_t batch37_first_panel_map_pixel() { return first_panel_map_pixel; }
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
esp_err_t Board::show_alpha(const uint16_t *pixels, const openu5::UiSession &, const openu5::GameState &game,
                            const openu5::TurnState &, const openu5::HudWorldState &, const uint8_t *,
                            const char *, const uint8_t *, bool, const DeviceDebugScreen *, bool,
                            uint8_t, const DeviceShopView *, const DeviceSelectionView *,
                            const DeviceContextActionBar *, DevicePartyHighlight highlight, uint32_t,
                            const openu5::HudDungeonBands *, bool full_square_viewport, bool preserve_party_panel) {
    if (!pixels) return ESP_ERR_INVALID_ARG;
    ++draws;
    ++ui_draws;
    if(!full_square_viewport)++world_draws;
    frames.emplace_back();
    frame_times.push_back(esp_timer_get_time());
    std::copy(pixels,pixels+176*176,frames.back().begin());
    if(!preserve_party_panel)capture_panel(game, highlight);
    // Mirror Board's physical viewport placement and the sky/wind overlays.
    for (int y = 0; y < openu5::kViewportPixels; ++y)
        for (int x = 0; x < openu5::kViewportPixels; ++x)
            screen[size_t(y + 4) * 320 + size_t(x + 4)] =
                (!full_square_viewport && (y < 9 || y >= 167)) ? 0x07ff : pixels[size_t(y) * 176 + size_t(x)];
    return ESP_OK;
}
esp_err_t Board::show_frontend(const openu5::FrontendView &, const uint16_t *, const uint16_t *,
                                const uint16_t *, const uint16_t *, uint8_t) { return ESP_OK; }
esp_err_t Board::refresh_bed_status_panel(const openu5::GameState &game,DevicePartyHighlight highlight) {
    if(panel_draws==0)first_panel_map_pixel=screen[92 * 320 + 92];
    ++panel_draws;
    std::array<uint16_t,7> sample{};
    for(int slot=0;slot<6;++slot)sample[slot]=uint16_t(game.party.characters[slot].status);
    sample[6]=uint16_t(game.time.hour*60+game.time.minute);
    camp_panels.push_back(sample);
    capture_panel(game,highlight);
    return ESP_OK;
}
esp_err_t Board::show_camp_viewport(const uint16_t *pixels,uint32_t) {
    if(!pixels)return ESP_ERR_INVALID_ARG;
    ++draws;
    frames.emplace_back();
    frame_times.push_back(esp_timer_get_time());
    std::copy(pixels,pixels+176*176,frames.back().begin());
    for(int y=0;y<176;++y)for(int x=0;x<176;++x)
        screen[size_t(y+4)*320+size_t(x+4)]=pixels[size_t(y)*176+size_t(x)];
    return ESP_OK;
}
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
