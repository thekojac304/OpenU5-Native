#pragma once
#include "dungeon_view.h"
#include <cstddef>
#include <cstdint>

namespace openu5 {

// ---------------------------------------------------------------------------
// R-05 (Batch 9C) -- the AUTHORED-ART half of the dungeon view.
//
// Batch 9 proved WHAT the corridor shows (`plan_dungeon_view`).  This header
// answers the only remaining question: for each op of that plan, WHICH authored
// image from the original's own art banks is blitted, WHERE, and WITH WHICH
// flips.  It is deliberately pure and portable -- no framebuffer, no ESP-IDF,
// no pack reader -- so `dungeon_art_regression` proves the mapping on the host
// exactly as `dungeon_view_regression` proves the plan.  The device renderer
// consumes these blits; it does not re-derive any of them, and it never
// re-decides anything `DungeonViewPlan` already decided.
//
// Authoritative model, in evidence order:
//   * DUNGEON.OVL `dng_blit_piece` @0x134A (the blit + its X/Y tables),
//     `fn_1952` @0x1952 (features, the two-block scheme), `fn_150a` @0x150a
//     (front pair), `fn_1682` @0x1682 (sides), monster blit @0x1210-0x1273;
//   * DATA.OVL (DS+0x10) tables 0x2e62 (side X), 0x2e72 (near/feature X),
//     0x2e7a + 0x2e82 (feature Y), 0x2f16 + 0x2f1e (feature image bases),
//     0x2E2A + 0x2E32 (monster X and Y-by-row);
//   * the accepted reference implementation `game/src/skin/fiel/dungeon.ts`
//     (`blitSlice`, `drawFront`, `featureBlits`, `drawMonster`);
//   * the accepted container parsers `extractor/src/parsers/dngtiles.ts`
//     (`parseDngView`, `parseItemsView`) and `.../monview.ts` (`parseMonView`),
//     which fix the bank layouts and image indices used below.
// ---------------------------------------------------------------------------

/** The three authored banks the corridor is composed from. */
enum class DungeonArtBank : uint8_t {
    /** DNG1/2/3.16 -- 28 perspective corridor slices, one file per wall variant. */
    Wall,
    /** ITEMS.16 -- 20 masked half-features (ladder/fountain/trap/chest). */
    Items,
    /** MON0-7.16 -- 8 banks of 6 masked wanderer sprites (2 frames x 3 depths). */
    Monster,
};

/** Slots per bank, fixed by the containers themselves (see the parsers above). */
constexpr size_t kDungeonWallImages = 28;
constexpr size_t kDungeonItemImages = 20;
constexpr size_t kDungeonMonBanks = 8;
constexpr size_t kDungeonMonImages = 6;
/** Wall variants: V=1/2/3 selects DNG1/DNG2/DNG3 (DUNGEON:0x0e7b). */
constexpr size_t kDungeonWallVariants = 3;

/** One authored image's source size. `w == 0` marks an empty container slot. */
struct DungeonArtImage {
    uint16_t w = 0, h = 0;
};

/**
 * Source dimensions of every authored image, by bank.  The resolver needs them
 * because the original anchors the centred pairs at `96 - width` (table 0x2e72)
 * rather than at a constant, so the destination box cannot be derived without
 * the art itself.  All eight MON banks share one dimension table -- the
 * containers are identical -- which `parseMonView` and the identity test below
 * both confirm.
 */
struct DungeonArtCatalog {
    const DungeonArtImage *wall = nullptr;  // kDungeonWallImages entries
    const DungeonArtImage *items = nullptr; // kDungeonItemImages entries
    const DungeonArtImage *mon = nullptr;   // kDungeonMonImages entries
};

/**
 * The authored dimension tables, read out of DNG1/2/3.16, ITEMS.16 and
 * MON0-7.16.  They are immutable facts of the 1988 data files, so they are
 * stated once here and enforced at two ends: the packer refuses to build a pack
 * whose extracted images disagree, and `dungeon_art_regression` re-derives the
 * geometry invariants (slice abutment, `96 - w` anchoring) from them.
 */
extern const DungeonArtImage kDungeonWallDims[kDungeonWallImages];
extern const DungeonArtImage kDungeonItemDims[kDungeonItemImages];
extern const DungeonArtImage kDungeonMonDims[kDungeonMonImages];

/** The catalog built from the tables above. */
DungeonArtCatalog dungeon_art_authored_catalog();

/**
 * One authored blit, already resolved to a destination box.  The image occupies
 * screen `[x, x+w) x [y, y+h)`; `mirror` flips it horizontally inside that box
 * and `vflip` vertically, which is exactly what the binary's two blit
 * primitives (0x8b7c normal, 0x8a2c mirrored, with the vertical-flip argument)
 * do.  Transparency is the bank's own AND-mask -- never colour-0 keying, since
 * the open chest's black interior is opaque (`parseItemsView`).
 */
struct DungeonArtBlit {
    DungeonArtBank bank = DungeonArtBank::Wall;
    /** Index within the bank. */
    uint8_t image = 0;
    /** MON0-7 selector; 0 for the Wall and Items banks. */
    uint8_t mon_bank = 0;
    int16_t x = 0, y = 0;
    uint16_t w = 0, h = 0;
    bool mirror = false;
    bool vflip = false;
};

/**
 * Worst case for one op is the LadderUpDown feature: the flipped up-block pair
 * plus the normal down-block pair (`fn_1952` @0x1984 then @0x19ae) = 4.
 */
constexpr size_t kDungeonMaxBlitsPerOp = 4;

/**
 * Expand one `DungeonViewPlan` op into the authored blits the original emits
 * for it, in the original's own paint order.  Returns the number written.
 *
 * `phase` only picks the wanderer's two animation frames; it is presentation
 * state and touches nothing semantic (the binary rolls RNG per redraw here --
 * a sanctioned divergence, dungeon.md 12.11).  Returns 0 for an op with no
 * authored art at all: a magic field (sparkle subsystem, no ITEMS image) and a
 * trap whose low three tile bits are set (`fn_1952` @0x197b).
 */
size_t dungeon_art_blits(const DungeonDrawOp &op, const DungeonArtCatalog &catalog,
                         uint32_t phase, DungeonArtBlit *out, size_t capacity);

/**
 * Which wall bank a variant selects: V=1/2/3 -> index 0/1/2 (DNG1/DNG2/DNG3).
 * Out-of-range variants clamp, matching the reference's own pack selection.
 */
uint8_t dungeon_art_wall_bank_index(uint8_t wall_variant);

/**
 * The runtime cache identity for a view.  Only the wall bank is variant-
 * dependent -- ITEMS and MON0-7 are shared by every dungeon -- so this is the
 * whole key, and an ordinary turn or step inside one dungeon never changes it.
 */
struct DungeonArtCacheKey {
    uint8_t wall_bank = 0;
    bool operator==(const DungeonArtCacheKey &other) const {
        return wall_bank == other.wall_bank;
    }
    bool operator!=(const DungeonArtCacheKey &other) const { return !(*this == other); }
};

DungeonArtCacheKey dungeon_art_cache_key(const DungeonViewPlan &);

/**
 * One authored image as it sits in memory, in the original's OWN encoding:
 * 4-bit palette indices, two pixels per byte, the HIGH nibble being the left
 * pixel.  Nothing is pre-decoded to RGB565 -- keeping the authored indices is
 * half the bytes and preserves byte-level identity with the source file, and
 * the painter maps an index through the EGA palette as it writes the pixel.
 *
 * `mask` is the container's own 1bpp AND-mask, MSB-first, where bit 1 means
 * BACKGROUND.  A null mask means the image is fully opaque, which is true of
 * every corridor slice (floor speckle and ceiling are baked in).  Transparency
 * therefore never comes from colour-0 keying: the open chest's black interior
 * is opaque, and treating index 0 as transparent would punch a hole in it.
 */
struct DungeonArtSurface {
    const uint8_t *pixels = nullptr;
    const uint8_t *mask = nullptr;
    uint16_t w = 0, h = 0;
};

/**
 * Every surface a view can need, indexed exactly as `DungeonArtBlit` addresses
 * them.  `wall` points at the SELECTED variant's 28 slices, so changing dungeon
 * variant is a repoint, not a re-decode.  A null bank simply paints nothing --
 * the picture degrades to black rather than to garbage.
 */
struct DungeonArtSurfaces {
    const DungeonArtSurface *wall = nullptr;  // kDungeonWallImages
    const DungeonArtSurface *items = nullptr; // kDungeonItemImages
    const DungeonArtSurface *mon = nullptr;   // kDungeonMonBanks * kDungeonMonImages
};

/** The surface a blit paints, or nullptr when that bank or slot is unavailable. */
const DungeonArtSurface *dungeon_art_surface(const DungeonArtSurfaces &, const DungeonArtBlit &);

// ---------------------------------------------------------------------------
// The PACKED container, as `native/tools/u5pack/alpha1-dungeon-art.ts` writes it
// into the alpha resource pack's `dungeon-*.art` entries:
//
//   0   char magic[8] = "OU5DART1"
//   8   u16 bank_count, 10 u16 images_per_bank, 12 u16 flags, 14 u16 reserved
//   16  image table, bank-major, 12 bytes each:
//         u16 width, u16 height, u32 pixel_offset, u32 mask_offset
//       An empty container slot is width = height = 0.
//   ..  payload; offsets are absolute within the entry.
//
// Parsing lives here, not in the device, so the bounds checking that stands
// between a corrupt SD card and a wild pointer is host-testable.
// ---------------------------------------------------------------------------
constexpr char kDungeonArtMagic[8] = {'O', 'U', '5', 'D', 'A', 'R', 'T', '1'};
constexpr size_t kDungeonArtHeaderBytes = 16;
constexpr size_t kDungeonArtImageBytes = 12;
constexpr uint16_t kDungeonArtFlagMasked = 1;

/**
 * Validate a packed art entry and index its images into `out`, which must hold
 * `banks * images_per_bank` surfaces.  The pointers written into `out` alias
 * `bytes`, so the caller keeps that buffer alive for as long as the surfaces.
 *
 * Returns false -- writing nothing -- when the magic, shape or mask flag is not
 * the one asked for, or when ANY image's pixel or mask span would fall outside
 * `length`.  That is the check a truncated or tampered pack has to fail: past
 * it, the painter dereferences these pointers every redraw for the rest of the
 * session.
 */
bool dungeon_art_parse(const uint8_t *bytes, size_t length, size_t banks, size_t images_per_bank,
                       bool masked, DungeonArtSurface *out, size_t out_capacity);

/** The 16 EGA entries every authored dungeon image is indexed against, as RGB565. */
extern const uint16_t kDungeonEgaRgb565[16];

// ---------------------------------------------------------------------------
// Batch 12B -- the MAGIC FIELD, the one corridor feature with no authored image.
//
// `dungeon_art_blits()` returns 0 for cell kind 8 and always has: ITEMS.16 has
// no field picture.  The original does not leave the cell blank either -- it
// draws the field PROCEDURALLY, with `magic_field_sparkle_drawer` @0x127e,
// reached from `feature_overlay_drawer_by_nibble` @0x19f6 when the tile's high
// nibble is 8.  Until Batch 12B the port carried the gap rather than the
// subsystem, so a cast In Flam/Nox/Zu/Sanct Grav changed the map and showed
// nothing, and the 55 fields DUNGEON.DAT authors in Wrong and Covetous had
// never been visible at all.
//
// The body (0x127e-0x1346, `ret 4`) draws `count[depth]` horizontal strokes
// inside a square box, two `rand_range` rolls per stroke -- x first, then y:
//
//     x = rand(lo[depth], hi[depth] - len[depth])
//     y = rand(lo[depth], hi[depth])
//     hline(x, y, x + len[depth])            ; INCLUSIVE, so width = len + 1
//
// with the four tables living contiguously at DS 0x2e42 / 0x2e4a / 0x2e52 /
// 0x2e5a, four words each, indexed by DEPTH; and the colour switch at 0x1292
// selecting on the FIELD TYPE (`tile & 7`) from the shared procedural palette
// globals DS 0x13b6/0x13b4/0x13ae/0x13b2 = 2/1/2/1, each +8 by the `add ax, 8`
// at 0x12b7 -> 10/9/10/9.  Argument order and the depth/type split are settled
// by the caller at 0x19f6 and cross-checked by the tables' own 8-byte stride;
// both are derived in full in re/notes/dungeon-decor-mazmorra.md (sections 2, 5
// and 6 ticket 4) and implemented in the accepted reference
// game/src/skin/fiel/dungeon-decor.ts (`fieldSparkRects`).
//
// The randomness is RENDER randomness: the binary re-rolls the whole field on
// every corridor redraw, which is one pass of its key poll, so the consumption
// is unbounded and wall-clock dependent.  It is the sanctioned divergence class
// of dungeon.md 12.11 (the same one the wanderer's animation frame uses) and it
// must never touch GameState::rng.  `phase` is therefore the only entropy here,
// exactly as it is for a Monster op.
// ---------------------------------------------------------------------------

/** One procedural stroke: the inclusive hline `(x, y) .. (x + w - 1, y)`. */
struct DungeonFieldSpark {
    int16_t x = 0, y = 0;
    uint8_t w = 0;
    /** EGA palette index, to be mapped through kDungeonEgaRgb565 by the painter. */
    uint8_t color = 0;
};

/** Strokes per depth 0..3 (DS 0x2e52). */
extern const uint16_t kDungeonFieldSparkCount[4];
/** Low edge of the box per depth (DS 0x2e42). */
extern const int16_t kDungeonFieldSparkLo[4];
/** High edge of the box per depth (DS 0x2e4a). */
extern const int16_t kDungeonFieldSparkHi[4];
/** Stroke length per depth (DS 0x2e5a); the hline is inclusive, so width = len+1. */
extern const int16_t kDungeonFieldSparkLen[4];
/** Colour by field type `tile & 7` (0x1292 switch, palette globals +8). */
extern const uint8_t kDungeonFieldSparkColor[4];

/** How many strokes a field at `depth` draws; 0 outside 0..3. */
uint16_t dungeon_field_spark_count(uint8_t depth);

/**
 * Stroke `index` of the field at `depth` whose tile low nibble is `sub`.  PURE:
 * the same (depth, sub, phase, index) always yields the same stroke, so the
 * picture is reproducible in a host test while still re-rolling per redraw on
 * device, where `phase` advances.  `index` at or beyond
 * `dungeon_field_spark_count(depth)` -- or a depth outside 0..3 -- yields a
 * stroke of width 0, which paints nothing.
 *
 * A field type outside 0..3 cannot occur through the cast path (DS:0x4596 holds
 * exactly four tiles) and the binary would paint it with whatever colour the
 * brush last held; the port falls back to type 0's colour rather than inherit
 * an undefined one, which is what the reference does too.
 */
DungeonFieldSpark dungeon_field_spark(uint8_t depth, uint8_t sub, uint32_t phase, uint16_t index);

} // namespace openu5
