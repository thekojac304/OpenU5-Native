#include "openu5/dungeon_art.h"

namespace openu5 {
namespace {

/** Y of the top of every corridor slice (`dng_blit_piece` @0x134A, `push 0xe`). */
constexpr int16_t kSliceY = kDungeonSliceY;

// Feature anchoring (`dng_blit_piece` @0x13c6-0x1452, DATA.OVL DS+0x10):
//   X: the normal half at 0x2e72[si] = 96 - w, its mirror ABUTTED at 0x60 = 96.
//   Y: the vertically flipped up-ladder block at 0x2e82[si]; a low object
//      (image >= 8: trap and both chests) at 0x2e7a[si]; everything else
//      (image < 8: fountain, down-ladder) at the horizon, a constant 96.
constexpr int16_t kFeatureYLadderUp[4] = {15, 39, 71, 87};  // 0x2e82
constexpr int16_t kFeatureYFloor[4] = {152, 120, 104, 96};  // 0x2e7a
constexpr int16_t kFeatureYHorizon = 96;                    // 0x1412

// Wanderer geometry: X of the left half by depth 1-3 (0x2E2A) -- again 96 - w --
// and Y by [row][depth] (0x2E32), where row 1 is the CEILING row.
constexpr int16_t kMonsterX[3] = {72, 80, 88};
constexpr int16_t kMonsterY[2][3] = {{86, 96, 98}, {40, 70, 85}};

/** Image base of the vertically FLIPPED block, by cell kind (table 0x2f16). */
int feature_up_image_base(uint8_t cell_type) {
    switch (DungeonCellKind(cell_type)) {
    case DungeonCellKind::LadderUp:
    case DungeonCellKind::LadderUpDown:
        return 0; // 0x1f -> images 0-3
    default:
        return -1;
    }
}

/** Image base of the NORMAL block, by cell kind (table 0x2f1e). */
int feature_image_base(uint8_t cell_type) {
    switch (DungeonCellKind(cell_type)) {
    case DungeonCellKind::LadderDown:
    case DungeonCellKind::LadderUpDown:
        return 0; // 0x1f -> ladder 0-3
    case DungeonCellKind::Fountain:
        return 4; // 0x27
    case DungeonCellKind::Trap:
        return 8; // 0x2f
    case DungeonCellKind::Chest:
        return 12; // 0x37
    case DungeonCellKind::OpenChest:
        return 16; // 0x3f
    default:
        return -1;
    }
}

int clamp_depth(int depth) { return depth < 0 ? 0 : depth > kDungeonMaxDepth ? kDungeonMaxDepth : depth; }

/**
 * The centred PAIR every non-side blit is drawn as: the normal half ending at
 * the centre (X = 96 - w) and its horizontal mirror abutted at X = 96.  Writes
 * two blits, or none when the slot is empty.
 */
size_t centred_pair(DungeonArtBank bank, uint8_t image, uint8_t mon_bank,
                    const DungeonArtImage &dims, int16_t y, bool vflip,
                    DungeonArtBlit *out, size_t capacity, size_t at) {
    if (dims.w == 0 || dims.h == 0 || at + 2 > capacity) return 0;
    DungeonArtBlit blit{};
    blit.bank = bank;
    blit.image = image;
    blit.mon_bank = mon_bank;
    blit.y = y;
    blit.w = dims.w;
    blit.h = dims.h;
    blit.vflip = vflip;
    blit.x = int16_t(kDungeonCenterX - int(dims.w));
    blit.mirror = false;
    out[at] = blit;
    blit.x = kDungeonCenterX;
    blit.mirror = true;
    out[at + 1] = blit;
    return 2;
}

} // namespace

// Extracted from the containers themselves: every DNGn.16 decompresses to
// 54,666 B holding 28 slots of height 164 (slots 8 and 24 are empty -- they are
// the depth-0 entries of front bases 8 and 24, which a front op can never
// reach because the driver only emits one at si > 0).  Widths 24/32/16/8 make
// each side's four rings abut exactly (left 16->96, right 96->176), and the
// front slices 56/24/8 anchor at 96 - w = 40/72/88, matching table 0x2e72.
const DungeonArtImage kDungeonWallDims[kDungeonWallImages] = {
    {24, 164}, {32, 164}, {16, 164}, {8, 164},  // 0-3   side: plain wall
    {24, 164}, {32, 164}, {16, 164}, {8, 164},  // 4-7   side: door
    {0, 0},    {56, 164}, {24, 164}, {8, 164},  // 8-11  front: dead end
    {80, 164}, {56, 164}, {24, 164}, {8, 164},  // 12-15 front: door
    {24, 164}, {32, 164}, {16, 164}, {8, 164},  // 16-19 side: open passage
    {24, 164}, {32, 164}, {16, 164}, {8, 164},  // 20-23 side: alcove
    {0, 0},    {56, 164}, {24, 164}, {8, 164},  // 24-27 front: special wall
};

// ITEMS.16 decompresses to 10,442 B: 20 masked half-features in five groups of
// four depths -- ladder 0-3, fountain 4-7, trap 8-11, closed chest 12-15, open
// chest 16-19.  The near widths 40/24/16/8 again anchor at 96 - w = 56/72/80/88
// (table 0x2e72).  Slot 19 is 16x16, not 8x8: the open chest's farthest frame
// reuses the mid size, which the container itself states.
const DungeonArtImage kDungeonItemDims[kDungeonItemImages] = {
    {40, 80}, {24, 56}, {16, 24}, {8, 8},  // 0-3   ladder
    {40, 80}, {24, 56}, {16, 24}, {8, 8},  // 4-7   fountain
    {40, 24}, {24, 32}, {16, 16}, {8, 8},  // 8-11  trap
    {40, 24}, {24, 32}, {16, 16}, {8, 8},  // 12-15 closed chest
    {40, 24}, {24, 32}, {16, 16}, {16, 16} // 16-19 open chest
};

// Every MONn.16 decompresses to 2,614 B: 6 masked slots = 2 animation frames x
// 3 depths, indexed frame*3 + (depth-1).  Widths 24/16/8 anchor the left half
// at 96 - w = 72/80/88, which is table 0x2E2A verbatim.
const DungeonArtImage kDungeonMonDims[kDungeonMonImages] = {
    {24, 66}, {16, 25}, {8, 6}, // frame 0: near, mid, far
    {24, 66}, {16, 25}, {8, 6}, // frame 1: near, mid, far
};

DungeonArtCatalog dungeon_art_authored_catalog() {
    DungeonArtCatalog catalog{};
    catalog.wall = kDungeonWallDims;
    catalog.items = kDungeonItemDims;
    catalog.mon = kDungeonMonDims;
    return catalog;
}

uint8_t dungeon_art_wall_bank_index(uint8_t wall_variant) {
    if (wall_variant <= 1) return 0;
    if (wall_variant >= uint8_t(kDungeonWallVariants)) return uint8_t(kDungeonWallVariants - 1);
    return uint8_t(wall_variant - 1);
}

DungeonArtCacheKey dungeon_art_cache_key(const DungeonViewPlan &plan) {
    DungeonArtCacheKey key{};
    key.wall_bank = dungeon_art_wall_bank_index(plan.wall_variant);
    return key;
}

// EGA 16, converted to RGB565 by the project's single conversion rule
// (r&0xf8)<<8 | (g&0xfc)<<3 | b>>3.  These are the same values the packless
// placeholder already used for the entries it needed, extended to all 16.
const uint16_t kDungeonEgaRgb565[16] = {
    0x0000, 0x0015, 0x0540, 0x0555, 0xa800, 0xa815, 0xaaa0, 0xad55,
    0x52aa, 0x52bf, 0x57ea, 0x57ff, 0xfaaa, 0xfabf, 0xffea, 0xffff,
};

bool dungeon_art_parse(const uint8_t *bytes, size_t length, size_t banks, size_t images_per_bank,
                       bool masked, DungeonArtSurface *out, size_t out_capacity) {
    const size_t slots = banks * images_per_bank;
    if (!bytes || !out || out_capacity < slots || slots == 0) return false;
    if (length < kDungeonArtHeaderBytes + slots * kDungeonArtImageBytes) return false;
    for (size_t i = 0; i < sizeof(kDungeonArtMagic); ++i)
        if (bytes[i] != uint8_t(kDungeonArtMagic[i])) return false;

    auto u16 = [bytes](size_t at) {
        return uint16_t(uint16_t(bytes[at]) | uint16_t(uint16_t(bytes[at + 1]) << 8));
    };
    auto u32 = [bytes](size_t at) {
        return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 | uint32_t(bytes[at + 2]) << 16 |
               uint32_t(bytes[at + 3]) << 24;
    };
    if (u16(8) != banks || u16(10) != images_per_bank) return false;
    if (((u16(12) & kDungeonArtFlagMasked) != 0) != masked) return false;

    // Validate EVERY slot before writing any of them: a half-indexed bank would
    // be worse than an unindexed one, because the painter cannot tell which
    // half it is holding.
    for (size_t i = 0; i < slots; ++i) {
        const size_t at = kDungeonArtHeaderBytes + i * kDungeonArtImageBytes;
        const uint16_t w = u16(at), h = u16(at + 2);
        if (w == 0 || h == 0) continue;
        if ((w & 1) != 0 || (masked && (w & 7) != 0)) return false;
        const uint32_t pixel_offset = u32(at + 4);
        const size_t pixel_bytes = size_t(w >> 1) * h;
        if (pixel_offset < kDungeonArtHeaderBytes || pixel_offset > length ||
            pixel_bytes > length - pixel_offset)
            return false;
        if (masked) {
            const uint32_t mask_offset = u32(at + 8);
            const size_t mask_bytes = size_t(w >> 3) * h;
            if (mask_offset < kDungeonArtHeaderBytes || mask_offset > length ||
                mask_bytes > length - mask_offset)
                return false;
        }
    }
    for (size_t i = 0; i < slots; ++i) {
        const size_t at = kDungeonArtHeaderBytes + i * kDungeonArtImageBytes;
        const uint16_t w = u16(at), h = u16(at + 2);
        out[i] = {};
        if (w == 0 || h == 0) continue;
        out[i].w = w;
        out[i].h = h;
        out[i].pixels = bytes + u32(at + 4);
        out[i].mask = masked ? bytes + u32(at + 8) : nullptr;
    }
    return true;
}

const DungeonArtSurface *dungeon_art_surface(const DungeonArtSurfaces &surfaces,
                                             const DungeonArtBlit &blit) {
    const DungeonArtSurface *surface = nullptr;
    switch (blit.bank) {
    case DungeonArtBank::Wall:
        if (surfaces.wall && blit.image < kDungeonWallImages) surface = surfaces.wall + blit.image;
        break;
    case DungeonArtBank::Items:
        if (surfaces.items && blit.image < kDungeonItemImages) surface = surfaces.items + blit.image;
        break;
    case DungeonArtBank::Monster:
        if (surfaces.mon && blit.mon_bank < kDungeonMonBanks && blit.image < kDungeonMonImages)
            surface = surfaces.mon + size_t(blit.mon_bank) * kDungeonMonImages + blit.image;
        break;
    }
    if (!surface || !surface->pixels || surface->w == 0 || surface->h == 0) return nullptr;
    return surface;
}

size_t dungeon_art_blits(const DungeonDrawOp &op, const DungeonArtCatalog &catalog,
                         uint32_t phase, DungeonArtBlit *out, size_t capacity) {
    if (!out || capacity == 0) return 0;

    switch (op.kind) {
    case DungeonOpKind::Side: {
        // A side slice is a SINGLE blit at the plan's own X (table 0x2e62); the
        // right-hand one is mirrored so the tunnel reads symmetrically
        // (primitives 0x8b7c left / 0x8a2c right, `fn_1682`).
        if (!catalog.wall || op.slice >= kDungeonWallImages) return 0;
        const DungeonArtImage &dims = catalog.wall[op.slice];
        if (dims.w == 0 || dims.h == 0) return 0;
        DungeonArtBlit blit{};
        blit.bank = DungeonArtBank::Wall;
        blit.image = op.slice;
        blit.x = op.x;
        blit.y = kSliceY;
        blit.w = dims.w;
        blit.h = dims.h;
        blit.mirror = op.mirror;
        out[0] = blit;
        return 1;
    }
    case DungeonOpKind::Front: {
        // The dead end / door is drawn as a centred PAIR: left half normal
        // ending at the centre, right half mirrored anchored on the binary's
        // X override 0x60 (`fn_150a` via `dng_blit_piece` @0x134A).
        if (!catalog.wall || op.slice >= kDungeonWallImages) return 0;
        return centred_pair(DungeonArtBank::Wall, op.slice, 0, catalog.wall[op.slice], kSliceY,
                            false, out, capacity, 0);
    }
    case DungeonOpKind::Feature: {
        // `fn_1952`: up to two BLOCKS, the vertically flipped up-ladder first
        // (@0x1984) and the normal block second (@0x19ae), each drawn as a
        // centred pair.  A magic field has no ITEMS image at all, and a trap is
        // gated on the tile's low three bits (@0x197b) -- both already decided
        // by `dungeon_feature_drawable`, which is reused here rather than
        // restated.
        if (!catalog.items) return 0;
        if (op.cell_type == uint8_t(DungeonCellKind::MagicField)) return 0;
        if (!dungeon_feature_drawable(op.cell_type, op.sub)) return 0;
        const int si = clamp_depth(op.depth);
        size_t written = 0;
        const int up = feature_up_image_base(op.cell_type);
        if (up >= 0) {
            const int image = up + si;
            if (image >= 0 && size_t(image) < kDungeonItemImages)
                written += centred_pair(DungeonArtBank::Items, uint8_t(image), 0,
                                        catalog.items[image], kFeatureYLadderUp[si], true, out,
                                        capacity, written);
        }
        const int normal = feature_image_base(op.cell_type);
        if (normal >= 0) {
            const int image = normal + si;
            if (image >= 0 && size_t(image) < kDungeonItemImages) {
                const int16_t y = image >= 8 ? kFeatureYFloor[si] : kFeatureYHorizon;
                written += centred_pair(DungeonArtBank::Items, uint8_t(image), 0,
                                        catalog.items[image], y, false, out, capacity, written);
            }
        }
        return written;
    }
    case DungeonOpKind::Monster: {
        // Two mirrored halves, each picking its own animation frame from the
        // presentation phase (the binary rolls RNG per redraw; dungeon.md
        // 12.11 sanctions the divergence).  Row 1 of table 0x2E32 is the
        // CEILING row -- a lurking spider or slime, not an invisible monster.
        if (!catalog.mon || op.depth < 1 || op.depth > 3) return 0;
        if (op.bank >= kDungeonMonBanks || capacity < 2) return 0;
        const int di = int(op.depth) - 1;
        const int16_t y = kMonsterY[op.ceiling ? 1 : 0][di];
        const uint8_t left_image = uint8_t((phase & 1u) * 3 + unsigned(di));
        const uint8_t right_image = uint8_t(((phase >> 1) & 1u) * 3 + unsigned(di));
        const DungeonArtImage &left_dims = catalog.mon[left_image];
        const DungeonArtImage &right_dims = catalog.mon[right_image];
        if (left_dims.w == 0 || right_dims.w == 0) return 0;
        DungeonArtBlit blit{};
        blit.bank = DungeonArtBank::Monster;
        blit.mon_bank = op.bank;
        blit.y = y;
        blit.image = left_image;
        blit.w = left_dims.w;
        blit.h = left_dims.h;
        blit.x = kMonsterX[di];
        blit.mirror = false;
        out[0] = blit;
        blit.image = right_image;
        blit.w = right_dims.w;
        blit.h = right_dims.h;
        blit.x = kDungeonCenterX;
        blit.mirror = true;
        out[1] = blit;
        return 2;
    }
    }
    return 0;
}

} // namespace openu5
