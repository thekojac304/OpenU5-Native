#pragma once

#include <cstddef>
#include <cstdint>

#include "asset_pack.h"
#include "esp_err.h"

namespace openu5 {

constexpr int kViewportTiles = 11;
constexpr int kTilePixels = 16;
constexpr int kViewportPixels = kViewportTiles * kTilePixels;
constexpr size_t kViewportPixelCount = kViewportPixels * kViewportPixels;

struct RenderReport {
    int16_t left = 0;
    int16_t top = 0;
    int16_t right = 0;
    int16_t bottom = 0;
    uint8_t center_map_tile = 0;
    uint16_t avatar_tile = 0;
    uint16_t tile_records_read = 0;
    uint32_t viewport_crc32 = 0;
    const char *map_context = "unknown";
};

/** Compose the static faithful 11x11 surface view into native-endian RGB565. */
esp_err_t render_initial_view(AssetPackReader &assets,
                              const AssetPackReport &pack,
                              uint16_t *rgb565,
                              size_t pixel_count,
                              RenderReport &report);

}  // namespace openu5
