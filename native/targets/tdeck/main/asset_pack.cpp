#include "asset_pack.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "esp_log.h"

namespace openu5 {
namespace {

constexpr char kTag[] = "OpenU5Assets";
constexpr std::array<uint8_t, 8> kMagic{'O', 'U', '5', 'P', 'A', 'C', 'K', 0};
constexpr uint16_t kVersionMajor = 2;
constexpr uint16_t kVersionMinor = 0;
constexpr uint16_t kHeaderSize = 148;
constexpr uint16_t kSectionCount = 5;
constexpr uint16_t kTileFormatIndexed4 = 1;
constexpr uint16_t kMapFormatU8RowMajor = 1;
constexpr uint32_t kPaletteSection = 1;
constexpr uint32_t kTilesSection = 2;
constexpr uint32_t kWorldSection = 3;
constexpr uint32_t kInitialSection = 4;
constexpr uint32_t kInitialMapSection = 5;
constexpr size_t kIoBufferSize = 1024;

struct Section {
    uint32_t type = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint32_t count = 0;
    uint32_t crc32 = 0;
};

uint16_t read_u16le(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

uint32_t read_u32le(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc;
}

bool read_exact(FILE *file, uint32_t offset, void *destination, size_t length)
{
    return std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
           std::fread(destination, 1, length, file) == length;
}

bool crc_range(FILE *file, uint32_t offset, uint32_t length, uint32_t &result,
               std::array<uint8_t, kIoBufferSize> &buffer)
{
    if (std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) return false;
    uint32_t remaining = length;
    uint32_t crc = 0xffffffffU;
    while (remaining > 0) {
        const size_t request = std::min<size_t>(buffer.size(), remaining);
        const size_t got = std::fread(buffer.data(), 1, request, file);
        if (got != request) return false;
        crc = crc32_update(crc, buffer.data(), got);
        remaining -= static_cast<uint32_t>(got);
    }
    result = crc ^ 0xffffffffU;
    return true;
}

const Section *find_section(const std::array<Section, kSectionCount> &sections, uint32_t type)
{
    const auto found = std::find_if(sections.begin(), sections.end(),
                                    [type](const Section &s) { return s.type == type; });
    return found == sections.end() ? nullptr : &*found;
}

esp_err_t invalid(const char *message)
{
    ESP_LOGE(kTag, "%s", message);
    return ESP_FAIL;
}

}  // namespace

AssetPackReader::~AssetPackReader()
{
    close();
}

void AssetPackReader::close()
{
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
    palette_offset_ = 0;
    tiles_offset_ = 0;
    world_offset_ = 0;
    initial_map_offset_ = 0;
}

esp_err_t AssetPackReader::open(const char *path, AssetPackReport &report)
{
    close();
    report = {};
    // One temporary heap allocation per validation, reused for all CRC ranges.
    // RAII releases it on every error path; independent readers do not share it.
    const auto scratch = std::unique_ptr<std::array<uint8_t, kIoBufferSize>>(
        new (std::nothrow) std::array<uint8_t, kIoBufferSize>);
    if (!scratch) return ESP_ERR_NO_MEM;
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Asset pack not found at %s (errno=%d)", path, errno);
        return ESP_ERR_NOT_FOUND;
    }
    std::array<uint8_t, kHeaderSize> header{};
    if (std::fread(header.data(), 1, header.size(), file) != header.size()) {
        std::fclose(file);
        return invalid("Asset pack header is truncated");
    }
    if (std::memcmp(header.data(), kMagic.data(), kMagic.size()) != 0) {
        std::fclose(file);
        return invalid("Asset pack magic does not match OU5PACK");
    }
    const uint16_t major = read_u16le(&header[8]);
    const uint16_t minor = read_u16le(&header[10]);
    if (major != kVersionMajor || minor != kVersionMinor ||
        read_u16le(&header[12]) != kHeaderSize || read_u16le(&header[14]) != kSectionCount) {
        ESP_LOGE(kTag, "Unsupported asset pack layout/version %u.%u", major, minor);
        std::fclose(file);
        return ESP_FAIL;
    }
    report.file_size = read_u32le(&header[16]);
    report.payload_crc32 = read_u32le(&header[20]);
    if (read_u32le(&header[24]) != 0) {
        std::fclose(file);
        return invalid("Asset pack uses unsupported v2 flags");
    }
    report.tile_count = read_u16le(&header[28]);
    report.tile_width = read_u16le(&header[30]);
    report.tile_height = read_u16le(&header[32]);
    report.world_width = read_u16le(&header[36]);
    report.world_height = read_u16le(&header[38]);
    report.initial_map_width = read_u16le(&header[44]);
    report.initial_map_height = read_u16le(&header[46]);
    if (report.tile_count != 512 || report.tile_width != 16 || report.tile_height != 16 ||
        read_u16le(&header[34]) != kTileFormatIndexed4 ||
        report.world_width != 256 || report.world_height != 256 ||
        read_u16le(&header[40]) != kMapFormatU8RowMajor ||
        report.initial_map_width != 32 || report.initial_map_height != 32) {
        std::fclose(file);
        return invalid("Asset pack dimensions or encodings are incompatible with v2");
    }
    if (std::fseek(file, 0, SEEK_END) != 0 || std::ftell(file) != static_cast<long>(report.file_size)) {
        std::fclose(file);
        return invalid("Asset pack file-size field does not match the SD file");
    }

    std::array<Section, kSectionCount> sections{};
    for (size_t i = 0; i < sections.size(); ++i) {
        const size_t base = 48 + i * 20;
        sections[i] = {read_u32le(&header[base]), read_u32le(&header[base + 4]),
                       read_u32le(&header[base + 8]), read_u32le(&header[base + 12]),
                       read_u32le(&header[base + 16])};
        const uint64_t end = static_cast<uint64_t>(sections[i].offset) + sections[i].length;
        if (sections[i].offset < kHeaderSize || end > report.file_size) {
            std::fclose(file);
            return invalid("Asset pack section lies outside the file");
        }
        uint32_t observed_crc = 0;
        if (!crc_range(file, sections[i].offset, sections[i].length, observed_crc, *scratch) ||
            observed_crc != sections[i].crc32) {
            ESP_LOGE(kTag, "Section %lu CRC32 mismatch", static_cast<unsigned long>(sections[i].type));
            std::fclose(file);
            return ESP_FAIL;
        }
    }
    auto ordered = sections;
    std::sort(ordered.begin(), ordered.end(), [](const Section &a, const Section &b) {
        return a.offset < b.offset;
    });
    for (size_t i = 1; i < ordered.size(); ++i) {
        if (static_cast<uint64_t>(ordered[i - 1].offset) + ordered[i - 1].length > ordered[i].offset) {
            std::fclose(file);
            return invalid("Asset pack sections overlap");
        }
    }

    const Section *palette = find_section(sections, kPaletteSection);
    const Section *tiles = find_section(sections, kTilesSection);
    const Section *world = find_section(sections, kWorldSection);
    const Section *initial = find_section(sections, kInitialSection);
    const Section *initial_map = find_section(sections, kInitialMapSection);
    if (palette == nullptr || palette->length != 32 || palette->count != 16 ||
        tiles == nullptr || tiles->length != 512U * 128U || tiles->count != 512 ||
        world == nullptr || world->length != 256U * 256U || world->count != 256U * 256U ||
        initial == nullptr || initial->length != 8 || initial->count != 1 ||
        initial_map == nullptr || initial_map->length != 32U * 32U ||
        initial_map->count != 32U * 32U) {
        std::fclose(file);
        return invalid("Asset pack is missing a required v2 section or section size");
    }
    uint32_t payload_crc = 0;
    if (!crc_range(file, kHeaderSize, report.file_size - kHeaderSize, payload_crc, *scratch) ||
        payload_crc != report.payload_crc32) {
        std::fclose(file);
        return invalid("Asset pack payload CRC32 mismatch");
    }

    std::array<uint8_t, 8> initial_bytes{};
    std::array<uint8_t, 128> sample_tile{};
    if (!read_exact(file, initial->offset, initial_bytes.data(), initial_bytes.size()) ||
        !read_exact(file, tiles->offset, sample_tile.data(), sample_tile.size())) {
        std::fclose(file);
        return invalid("Could not read asset-pack sanity records");
    }
    report.initial_location = initial_bytes[0];
    report.initial_floor = initial_bytes[1];
    report.initial_x = initial_bytes[2];
    report.initial_y = initial_bytes[3];
    report.transport_tile = read_u16le(&initial_bytes[4]);
    report.avatar_tile = read_u16le(&initial_bytes[6]);
    if (report.initial_location != 13 || report.initial_floor != 0) {
        std::fclose(file);
        return invalid("Milestone 4 v2 pack does not select Iolo's Hut location 13 floor 0");
    }
    report.sample_tile_crc32 = crc32_update(0xffffffffU, sample_tile.data(), sample_tile.size()) ^ 0xffffffffU;
    const uint32_t map_index = static_cast<uint32_t>(report.initial_y) * report.initial_map_width +
                               report.initial_x;
    if (!read_exact(file, initial_map->offset + map_index, &report.sample_map_tile, 1)) {
        std::fclose(file);
        return invalid("Could not read the initial-position map tile");
    }
    file_ = file;
    palette_offset_ = palette->offset;
    tiles_offset_ = tiles->offset;
    world_offset_ = world->offset;
    initial_map_offset_ = initial_map->offset;
    ESP_LOGI(kTag, "Asset pack v%u.%u valid: %lu bytes, payload CRC32=%08lx",
             major, minor, static_cast<unsigned long>(report.file_size),
             static_cast<unsigned long>(report.payload_crc32));
    ESP_LOGI(kTag, "Tiles: %u x %ux%u indexed4; tile[0] CRC32=%08lx",
             report.tile_count, report.tile_width, report.tile_height,
             static_cast<unsigned long>(report.sample_tile_crc32));
    ESP_LOGI(kTag, "Britannia: %ux%u u8 row-major", report.world_width, report.world_height);
    ESP_LOGI(kTag, "Initial map/context: Iolo's Hut location 13 floor 0; %ux%u; map[%u,%u]=0x%02x",
             report.initial_map_width, report.initial_map_height, report.initial_x,
             report.initial_y, report.sample_map_tile);
    ESP_LOGI(kTag, "Initial: location=%u floor=0x%02x transport=0x%03x avatar=0x%03x",
             report.initial_location, report.initial_floor, report.transport_tile, report.avatar_tile);
    return ESP_OK;
}

esp_err_t AssetPackReader::read_palette(uint16_t (&palette)[16])
{
    std::array<uint8_t, 32> bytes{};
    if (file_ == nullptr || !read_exact(file_, palette_offset_, bytes.data(), bytes.size())) {
        return ESP_FAIL;
    }
    for (size_t i = 0; i < 16; ++i) palette[i] = read_u16le(&bytes[i * 2]);
    return ESP_OK;
}

esp_err_t AssetPackReader::read_tile(uint16_t tile_id, uint8_t (&indexed4)[128])
{
    if (file_ == nullptr || tile_id >= 512 ||
        !read_exact(file_, tiles_offset_ + static_cast<uint32_t>(tile_id) * 128U,
                    indexed4, 128)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t AssetPackReader::read_world_span(uint8_t y, uint8_t x, uint8_t *tiles,
                                           size_t count)
{
    if (file_ == nullptr || tiles == nullptr || count == 0 || count > 256U - x) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t offset = world_offset_ + static_cast<uint32_t>(y) * 256U + x;
    return read_exact(file_, offset, tiles, count) ? ESP_OK : ESP_FAIL;
}

esp_err_t AssetPackReader::read_initial_map_span(uint8_t y, uint8_t x, uint8_t *tiles,
                                                 size_t count)
{
    if (file_ == nullptr || tiles == nullptr || y >= 32 || x >= 32 || count == 0 ||
        count > 32U - x) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t offset = initial_map_offset_ + static_cast<uint32_t>(y) * 32U + x;
    return read_exact(file_, offset, tiles, count) ? ESP_OK : ESP_FAIL;
}

esp_err_t validate_asset_pack(const char *path, AssetPackReport &report)
{
    AssetPackReader reader;
    return reader.open(path, report);
}

}  // namespace openu5
