#pragma once

#include <cstddef>
#include <cstdint>

#include "asset_pack.h"
#include "openu5/presentation.h"
#include "openu5/dungeon.h"
#include "openu5/intro_view.h"
#include "openu5/world.h"
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
    uint16_t animated_cell_count = 0;
    uint32_t viewport_bytes = 0;
    uint8_t animated_cells[kPresentationCells]{};
    uint32_t viewport_crc32 = 0;
    const char *map_context = "unknown";
};

constexpr size_t kCachedTileBytes = 512U * 128U;
struct PresentationTileCache {
    uint16_t palette[16]{};
    uint8_t *tiles = nullptr; // Caller-owned kCachedTileBytes buffer (normally PSRAM).
};

/** Read the palette and every 4-bpp tile once; animation never touches SD afterward. */
esp_err_t initialize_tile_cache(AssetPackReader &, const AssetPackReport &,
                                uint8_t *storage, size_t storage_size,
                                PresentationTileCache &);

/** Compose the faithful 11x11 view centered on the supplied live coordinates. */
esp_err_t render_view(AssetPackReader &assets, const AssetPackReport &pack,
                      uint8_t center_x, uint8_t center_y, uint16_t *rgb565,
                      size_t pixel_count, RenderReport &report);

/** Compose any native WorldData map, including dynamic terrain callbacks. */
esp_err_t render_active_view(AssetPackReader &assets, const AssetPackReport &pack,
                             const ActiveMap &map, Position center, uint16_t avatar_tile,
                             uint16_t *rgb565, size_t pixel_count, RenderReport &report);

/** Rasterize an authoritative snapshot with presentation-time tile effects. */
esp_err_t render_snapshot(const PresentationTileCache &, const PresentationSnapshot &,
                          uint32_t animation_tick, int64_t world_turn,
                          uint16_t *rgb565, size_t pixel_count, RenderReport &report);

/**
 * Render the active 8x8 dungeon floor as the handheld first-person viewport.
 * This deliberately consumes DungeonState, rather than surface return
 * coordinates, so a live dungeon session can never fall through to world art.
 */
esp_err_t render_dungeon_view(const DungeonState &, uint16_t *rgb565,
                              size_t pixel_count, RenderReport &report,
                              uint16_t &primitives);

/** Render the reference-style 22x22 connected-floor map shown by View Gem. */
esp_err_t render_dungeon_gem_view(const DungeonState &, uint16_t *rgb565,
                                  size_t pixel_count, RenderReport &report,
                                  uint16_t &primitives);

/** Render the reference-style terrain overview shown by View Gem outside dungeons. */
esp_err_t render_world_gem_view(const ActiveMap &, Position, uint16_t *rgb565,
                                size_t pixel_count, RenderReport &report,
                                uint16_t &primitives);

/** Render the authoritative FONT.OVL four-row View band at its native 304x64 size. */
esp_err_t render_intro_view(const PresentationTileCache &, const IntroViewFrame &,
                            uint32_t animation_tick, uint16_t *rgb565,
                            size_t pixel_count, RenderReport &report);

}  // namespace openu5
