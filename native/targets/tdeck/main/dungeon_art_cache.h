#pragma once

#include <cstddef>
#include <cstdint>

#include "alpha_resources.h"
#include "esp_err.h"
#include "openu5/dungeon_art.h"

namespace tdeck {

/**
 * Batch 9C / R-05 -- the resident authored dungeon art.
 *
 * The five `dungeon-*.art` entries of the alpha resource pack are read ONCE,
 * during AlphaRuntime::initialize(), while the pack file is still open (main.cpp
 * closes it immediately afterwards, so there is no later opportunity and, by the
 * same token, no SD access at all once play begins -- a dungeon transition can
 * never stall or fail on the card).
 *
 * WHAT IS RESIDENT, and why all of it: the three wall variants together are
 * 163 KB and the party can walk into any of the eight dungeons in one session,
 * so over a session all three ARE what is needed; ITEMS and the eight MON banks
 * are shared by every dungeon and are small.  The whole set is ~196 KB of an
 * 8 MB PSRAM.  The alternative -- one resident variant reloaded on entry --
 * would save ~109 KB of a part that has megabytes spare, in exchange for SD I/O
 * inside a dungeon transition and a new failure mode there.  The trade is made
 * deliberately in favour of the quieter runtime, and it is measured rather than
 * assumed (`psram_bytes()`).
 *
 * The cache is therefore a SELECTOR, not a loader: `select()` repoints the wall
 * surface array at the resident variant and reads nothing.  Its identity is
 * openu5::DungeonArtCacheKey, which is the wall bank alone -- turning, stepping,
 * changing level and losing light all leave it untouched, which
 * `dungeon_art_regression` proves on the host.
 *
 * Images keep the original's own 4bpp indexed encoding plus, for the masked
 * banks, the container's 1bpp AND-mask; nothing is pre-decoded to RGB565.  The
 * blit maps an index through openu5::kDungeonEgaRgb565 as it writes the pixel.
 */
class DungeonArtCache {
  public:
    ~DungeonArtCache();
    DungeonArtCache() = default;
    DungeonArtCache(const DungeonArtCache &) = delete;
    DungeonArtCache &operator=(const DungeonArtCache &) = delete;

    /**
     * Read and index every authored bank from an OPEN pack.  A missing or
     * malformed entry leaves the cache unready and returns the error; the
     * renderer then paints an empty viewport rather than reading wild pointers,
     * so a bad pack degrades to black and not to a crash.
     */
    esp_err_t load(const AlphaResourcePack &pack);
    void release();
    bool ready() const { return ready_; }

    /** Point the wall surfaces at `key`'s variant.  True when the key changed. */
    bool select(const openu5::DungeonArtCacheKey &key);
    const openu5::DungeonArtCacheKey &key() const { return key_; }

    /** The resident surfaces for the currently selected variant. */
    const openu5::DungeonArtSurfaces &surfaces() const { return surfaces_; }

    /** Total PSRAM held by the authored art, in bytes. */
    size_t psram_bytes() const { return psram_bytes_; }
    /** Times bytes were actually read from the pack: 5 after a successful load. */
    uint32_t read_count() const { return read_count_; }
    /** Times select() changed the resident wall variant. */
    uint32_t select_count() const { return select_count_; }
    /** Microseconds the one-time load spent reading and indexing. */
    int64_t load_micros() const { return load_micros_; }

  private:
    esp_err_t load_bank(const AlphaResourcePack &pack, const char *name, size_t banks,
                        size_t images_per_bank, bool masked, uint8_t **blob,
                        openu5::DungeonArtSurface *out);

    bool ready_ = false;
    openu5::DungeonArtCacheKey key_{};
    openu5::DungeonArtSurfaces surfaces_{};
    // The raw entry blobs live in PSRAM; the descriptor tables below stay in
    // internal RAM because the painter walks them on every dungeon redraw --
    // the same reasoning alpha_resources.h gives for its own pointer tables.
    uint8_t *wall_blob_[openu5::kDungeonWallVariants]{};
    uint8_t *items_blob_ = nullptr;
    uint8_t *mon_blob_ = nullptr;
    openu5::DungeonArtSurface wall_[openu5::kDungeonWallVariants][openu5::kDungeonWallImages]{};
    openu5::DungeonArtSurface items_[openu5::kDungeonItemImages]{};
    openu5::DungeonArtSurface mon_[openu5::kDungeonMonBanks * openu5::kDungeonMonImages]{};
    size_t psram_bytes_ = 0;
    uint32_t read_count_ = 0, select_count_ = 0;
    int64_t load_micros_ = 0;
};

} // namespace tdeck
