#pragma once

#include <cstdint>

#include "esp_err.h"

namespace openu5 {

constexpr char kAssetPackPath[] = "/sd/ultima5/openu5-assets.bin";

struct AssetPackReport {
    uint32_t file_size = 0;
    uint32_t payload_crc32 = 0;
    uint16_t tile_count = 0;
    uint16_t tile_width = 0;
    uint16_t tile_height = 0;
    uint16_t world_width = 0;
    uint16_t world_height = 0;
    uint8_t initial_location = 0;
    uint8_t initial_floor = 0;
    uint8_t initial_x = 0;
    uint8_t initial_y = 0;
    uint16_t transport_tile = 0;
    uint16_t avatar_tile = 0;
    uint8_t sample_map_tile = 0;
    uint32_t sample_tile_crc32 = 0;
};

/** Validate the SD pack by streaming it; no section is loaded wholesale into RAM. */
esp_err_t validate_asset_pack(const char *path, AssetPackReport &report);

}  // namespace openu5
