#include "native_renderer.h"

#include <algorithm>
#include <array>

namespace openu5 {
namespace {

constexpr int kHalfViewport = kViewportTiles / 2;
constexpr size_t kMaxRequiredTiles = kViewportTiles * kViewportTiles + 1;

uint32_t crc32_u16le(const uint16_t *pixels, size_t count)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t bytes[] = {static_cast<uint8_t>(pixels[i]),
                                 static_cast<uint8_t>(pixels[i] >> 8)};
        for (uint8_t value : bytes) {
            crc ^= value;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
            }
        }
    }
    return crc ^ 0xffffffffU;
}

void expand_tile(const uint8_t (&indexed4)[128], const uint16_t (&palette)[16],
                 uint16_t *viewport, int cell_x, int cell_y)
{
    const int origin_x = cell_x * kTilePixels;
    const int origin_y = cell_y * kTilePixels;
    for (int y = 0; y < kTilePixels; ++y) {
        uint16_t *destination = viewport + (origin_y + y) * kViewportPixels + origin_x;
        for (int x_pair = 0; x_pair < kTilePixels / 2; ++x_pair) {
            const uint8_t packed = indexed4[y * (kTilePixels / 2) + x_pair];
            destination[x_pair * 2] = palette[packed >> 4];
            destination[x_pair * 2 + 1] = palette[packed & 0x0f];
        }
    }
}

}  // namespace

esp_err_t render_initial_view(AssetPackReader &assets,
                              const AssetPackReport &pack,
                              uint16_t *rgb565,
                              size_t pixel_count,
                              RenderReport &report)
{
    report = {};
    if (!assets.is_open() || rgb565 == nullptr || pixel_count < kViewportPixelCount ||
        pack.world_width != 256 || pack.world_height != 256 ||
        pack.initial_map_width != 32 || pack.initial_map_height != 32 || pack.tile_count != 512 ||
        pack.avatar_tile >= pack.tile_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t palette[16]{};
    if (assets.read_palette(palette) != ESP_OK) return ESP_FAIL;

    std::array<uint8_t, kViewportTiles * kViewportTiles> map_tiles{};
    const bool britannia = pack.initial_location == 0 && pack.initial_floor == 0;
    const bool iolos_hut = pack.initial_location == 13 && pack.initial_floor == 0;
    if (!britannia && !iolos_hut) return ESP_ERR_NOT_SUPPORTED;

    if (britannia) {
        report.map_context = "Britannia surface (256x256 wrapped)";
        for (int row = 0; row < kViewportTiles; ++row) {
            const uint8_t map_y = static_cast<uint8_t>(pack.initial_y - kHalfViewport + row);
            const uint8_t map_x = static_cast<uint8_t>(pack.initial_x - kHalfViewport);
            const size_t first = std::min<size_t>(kViewportTiles, 256U - map_x);
            uint8_t *destination = &map_tiles[row * kViewportTiles];
            if (assets.read_world_span(map_y, map_x, destination, first) != ESP_OK) return ESP_FAIL;
            if (first < kViewportTiles &&
                assets.read_world_span(map_y, 0, destination + first, kViewportTiles - first) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    } else {
        report.map_context = "Iolo's Hut (location 13 floor 0, 32x32 local)";
        uint8_t edge_fill = 0;
        if (assets.read_initial_map_span(31, 31, &edge_fill, 1) != ESP_OK) return ESP_FAIL;
        map_tiles.fill(edge_fill);
        const int left = static_cast<int>(pack.initial_x) - kHalfViewport;
        for (int row = 0; row < kViewportTiles; ++row) {
            const int map_y = static_cast<int>(pack.initial_y) - kHalfViewport + row;
            if (map_y < 0 || map_y >= 32) continue;
            const int first_x = std::max(0, left);
            const int last_x = std::min(31, left + kViewportTiles - 1);
            if (first_x > last_x) continue;
            const size_t count = static_cast<size_t>(last_x - first_x + 1);
            uint8_t *destination = &map_tiles[row * kViewportTiles + (first_x - left)];
            if (assets.read_initial_map_span(static_cast<uint8_t>(map_y),
                                             static_cast<uint8_t>(first_x), destination,
                                             count) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    }

    std::array<uint16_t, kMaxRequiredTiles> required{};
    size_t required_count = 0;
    const auto add_required = [&](uint16_t tile_id) {
        for (size_t i = 0; i < required_count; ++i) {
            if (required[i] == tile_id) return;
        }
        required[required_count++] = tile_id;
    };
    for (uint8_t tile : map_tiles) add_required(tile);
    add_required(pack.avatar_tile);

    uint8_t indexed4[128]{};
    for (size_t required_index = 0; required_index < required_count; ++required_index) {
        const uint16_t tile_id = required[required_index];
        if (assets.read_tile(tile_id, indexed4) != ESP_OK) return ESP_FAIL;
        ++report.tile_records_read;
        for (int row = 0; row < kViewportTiles; ++row) {
            for (int col = 0; col < kViewportTiles; ++col) {
                if (map_tiles[row * kViewportTiles + col] == tile_id) {
                    expand_tile(indexed4, palette, rgb565, col, row);
                }
            }
        }
        if (tile_id == pack.avatar_tile) {
            // CoreView's faithful ordering replaces the complete center tile last.
            expand_tile(indexed4, palette, rgb565, kHalfViewport, kHalfViewport);
        }
    }

    report.left = static_cast<int16_t>(pack.initial_x) - kHalfViewport;
    report.top = static_cast<int16_t>(pack.initial_y) - kHalfViewport;
    report.right = static_cast<int16_t>(pack.initial_x) + kHalfViewport;
    report.bottom = static_cast<int16_t>(pack.initial_y) + kHalfViewport;
    report.center_map_tile = map_tiles[kHalfViewport * kViewportTiles + kHalfViewport];
    report.avatar_tile = pack.avatar_tile;
    report.viewport_crc32 = crc32_u16le(rgb565, kViewportPixelCount);
    return ESP_OK;
}

}  // namespace openu5
