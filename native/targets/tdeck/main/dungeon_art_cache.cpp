#include "dungeon_art_cache.h"

#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "DungeonArt";
constexpr uint32_t kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
} // namespace

DungeonArtCache::~DungeonArtCache() { release(); }

void DungeonArtCache::release() {
    for (auto *&blob : wall_blob_) {
        if (blob) heap_caps_free(blob);
        blob = nullptr;
    }
    if (items_blob_) heap_caps_free(items_blob_);
    if (mon_blob_) heap_caps_free(mon_blob_);
    items_blob_ = nullptr;
    mon_blob_ = nullptr;
    std::memset(wall_, 0, sizeof(wall_));
    std::memset(items_, 0, sizeof(items_));
    std::memset(mon_, 0, sizeof(mon_));
    surfaces_ = {};
    key_ = {};
    ready_ = false;
    psram_bytes_ = 0;
    read_count_ = select_count_ = 0;
    load_micros_ = 0;
}

esp_err_t DungeonArtCache::load_bank(const AlphaResourcePack &pack, const char *name, size_t banks,
                                     size_t images_per_bank, bool masked, uint8_t **blob,
                                     openu5::DungeonArtSurface *out) {
    const uint32_t length = pack.entry_length(name);
    if (length == 0) {
        ESP_LOGE(kTag, "%s is missing from the resource pack", name);
        return ESP_ERR_NOT_FOUND;
    }
    auto *bytes = static_cast<uint8_t *>(heap_caps_malloc(length, kPsram));
    if (!bytes) {
        ESP_LOGE(kTag, "No PSRAM for %s (%lu bytes)", name, (unsigned long)length);
        return ESP_ERR_NO_MEM;
    }
    const esp_err_t read = pack.read_entry(name, bytes, length);
    if (read != ESP_OK) {
        heap_caps_free(bytes);
        ESP_LOGE(kTag, "Read of %s failed: %s", name, esp_err_to_name(read));
        return read;
    }
    ++read_count_;
    // The container's shape and every pixel/mask span are validated by the
    // portable parser in native/core, which `dungeon_art_regression` exercises
    // against both real and deliberately malformed bytes. The device does not
    // carry a second copy of that reasoning.
    if (!openu5::dungeon_art_parse(bytes, length, banks, images_per_bank, masked, out,
                                   banks * images_per_bank)) {
        heap_caps_free(bytes);
        ESP_LOGE(kTag, "%s is not a valid %ux%u%s art container (%lu bytes)", name, unsigned(banks),
                 unsigned(images_per_bank), masked ? " masked" : "", (unsigned long)length);
        return ESP_ERR_INVALID_SIZE;
    }
    *blob = bytes;
    psram_bytes_ += length;
    return ESP_OK;
}

esp_err_t DungeonArtCache::load(const AlphaResourcePack &pack) {
    release();
    const int64_t started = esp_timer_get_time();
    static const char *kWallNames[openu5::kDungeonWallVariants] = {
        "dungeon-dng1.art", "dungeon-dng2.art", "dungeon-dng3.art"};
    for (size_t v = 0; v < openu5::kDungeonWallVariants; ++v) {
        const esp_err_t err = load_bank(pack, kWallNames[v], 1, openu5::kDungeonWallImages, false,
                                        &wall_blob_[v], wall_[v]);
        if (err != ESP_OK) {
            release();
            return err;
        }
    }
    esp_err_t err = load_bank(pack, "dungeon-items.art", 1, openu5::kDungeonItemImages, true,
                              &items_blob_, items_);
    if (err != ESP_OK) {
        release();
        return err;
    }
    err = load_bank(pack, "dungeon-mon.art", openu5::kDungeonMonBanks, openu5::kDungeonMonImages,
                    true, &mon_blob_, mon_);
    if (err != ESP_OK) {
        release();
        return err;
    }

    surfaces_.items = items_;
    surfaces_.mon = mon_;
    key_ = {};
    surfaces_.wall = wall_[0];
    ready_ = true;
    load_micros_ = esp_timer_get_time() - started;
    ESP_LOGI(kTag,
             "Authored dungeon art resident: %zu PSRAM bytes across %lu entries in %lld us "
             "(3 wall variants, ITEMS, MON0-7); no SD access after this point",
             psram_bytes_, (unsigned long)read_count_, (long long)load_micros_);
    return ESP_OK;
}

bool DungeonArtCache::select(const openu5::DungeonArtCacheKey &key) {
    if (!ready_) return false;
    const uint8_t bank = key.wall_bank < openu5::kDungeonWallVariants
                             ? key.wall_bank
                             : uint8_t(openu5::kDungeonWallVariants - 1);
    if (surfaces_.wall == wall_[bank]) return false;
    // A repoint, not a reload: every variant is already resident, so changing
    // dungeon costs no SD read and no decode.
    surfaces_.wall = wall_[bank];
    key_.wall_bank = bank;
    ++select_count_;
    ESP_LOGI(kTag, "Wall variant -> DNG%u (resident, no reload)", unsigned(bank) + 1);
    return true;
}

} // namespace tdeck
