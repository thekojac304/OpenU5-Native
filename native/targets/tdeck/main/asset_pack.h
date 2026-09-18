#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdint>

#include "esp_err.h"

namespace openu5 {

constexpr char kAssetPackPath[] = "/sd/ultima5/openu5-assets.bin";
constexpr uint16_t kAssetPackVersionMajor = 2;
constexpr uint16_t kAssetPackVersionMinor = 0;
constexpr uint32_t kExpectedAssetPackSize = 132284;
constexpr uint32_t kExpectedAssetPackCrc32 = 0x933c9b82U;
constexpr char kExpectedAssetPackSha256[] =
    "6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188";

struct AssetPackReport {
    uint16_t version_major = 0;
    uint16_t version_minor = 0;
    uint32_t file_size = 0;
    uint32_t payload_crc32 = 0;
    uint16_t tile_count = 0;
    uint16_t tile_width = 0;
    uint16_t tile_height = 0;
    uint16_t world_width = 0;
    uint16_t world_height = 0;
    uint16_t initial_map_width = 0;
    uint16_t initial_map_height = 0;
    uint8_t initial_location = 0;
    uint8_t initial_floor = 0;
    uint8_t initial_x = 0;
    uint8_t initial_y = 0;
    uint16_t transport_tile = 0;
    uint16_t avatar_tile = 0;
    uint8_t sample_map_tile = 0;
    uint32_t sample_tile_crc32 = 0;
    bool firmware_match = false;
};

class AssetPackReader {
public:
    AssetPackReader() = default;
    ~AssetPackReader();
    AssetPackReader(const AssetPackReader &) = delete;
    AssetPackReader &operator=(const AssetPackReader &) = delete;

    /** Open and fully validate the v2 pack while retaining it for bounded reads. */
    esp_err_t open(const char *path, AssetPackReport &report);
    void close();
    bool is_open() const { return file_ != nullptr; }

    esp_err_t read_palette(uint16_t (&palette)[16]);
    esp_err_t read_tile(uint16_t tile_id, uint8_t (&indexed4)[128]);
    esp_err_t read_world_span(uint8_t y, uint8_t x, uint8_t *tiles, size_t count);
    esp_err_t read_initial_map_span(uint8_t y, uint8_t x, uint8_t *tiles, size_t count);

private:
    FILE *file_ = nullptr;
    uint32_t palette_offset_ = 0;
    uint32_t tiles_offset_ = 0;
    uint32_t world_offset_ = 0;
    uint32_t initial_map_offset_ = 0;
};

/** Validate the SD pack by streaming it; no section is loaded wholesale into RAM. */
esp_err_t validate_asset_pack(const char *path, AssetPackReport &report);

}  // namespace openu5
