// Batch 9C / R-05 -- the AUTHORED-ART half of the dungeon view.
//
// Batch 9's `dungeon_view_regression` proves the semantic plan; this suite
// proves the next step and nothing else: for each op of that plan, which
// authored image is blitted, into which box, with which flips, in which order.
// It asserts no pixels -- the point of the seam is that the mapping is
// decidable without a framebuffer, exactly as the plan was.
//
// Evidence order, as in Batch 9:
//   [REF-BIN] DUNGEON.OVL routine + address and the DATA.OVL tables they read,
//             as recorded in re/notes/dungeon3d-audit.md;
//   [REF-TS]  game/src/skin/fiel/dungeon.ts (blitSlice / drawFront /
//             featureBlits / drawMonster) and the container parsers
//             extractor/src/parsers/{dngtiles,monview}.ts;
//   [NATIVE]  native/core/src/dungeon_view.cpp, which already fixes the plan's
//             own conventions (slice bases, depths, op order).
//
// Naming: A<n> = an authored-art requirement.
#include "openu5/dungeon_art.h"

#include <cstring>
#include <iostream>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "dungeon_art regression: " << what << "\n";
        ++failures;
    }
}

// Cell KINDS -- the high nibble of a cell byte, which is what a DungeonDrawOp
// carries in `cell_type` (dungeon_view.cpp stores cell_kind(), not the byte).
constexpr uint8_t kNothing = 0x0;
constexpr uint8_t kWall = 0xb;
constexpr uint8_t kSpecialWall = 0xc;
constexpr uint8_t kNormalDoor = 0xe;
constexpr uint8_t kLadderUp = 0x1;
constexpr uint8_t kLadderDown = 0x2;
constexpr uint8_t kLadderUpDown = 0x3;
constexpr uint8_t kChest = 0x4;
constexpr uint8_t kFountain = 0x5;
constexpr uint8_t kTrap = 0x6;
constexpr uint8_t kOpenChest = 0x7;
constexpr uint8_t kMagicField = 0x8;

/** The same kind as a map CELL byte, for the A8 floor fixture. */
constexpr uint8_t cell(uint8_t kind, uint8_t sub = 0) { return uint8_t((kind << 4) | sub); }

const DungeonArtCatalog kCatalog = dungeon_art_authored_catalog();

DungeonDrawOp side_op(uint8_t slice, uint8_t depth, DungeonSide s, int16_t x, bool mirror) {
    DungeonDrawOp op{};
    op.kind = DungeonOpKind::Side;
    op.slice = slice;
    op.depth = depth;
    op.side = s;
    op.x = x;
    op.mirror = mirror;
    return op;
}
DungeonDrawOp front_op(uint8_t slice, uint8_t depth) {
    DungeonDrawOp op{};
    op.kind = DungeonOpKind::Front;
    op.slice = slice;
    op.depth = depth;
    return op;
}
DungeonDrawOp feature_op(uint8_t cell_type, uint8_t sub, uint8_t depth) {
    DungeonDrawOp op{};
    op.kind = DungeonOpKind::Feature;
    op.cell_type = cell_type;
    op.sub = sub;
    op.depth = depth;
    return op;
}
DungeonDrawOp monster_op(uint8_t bank, uint8_t depth, bool ceiling) {
    DungeonDrawOp op{};
    op.kind = DungeonOpKind::Monster;
    op.bank = bank;
    op.depth = depth;
    op.ceiling = ceiling;
    return op;
}

/** A floor that is solid wall except where a case carves it out. */
struct Floor {
    DungeonState d{};
    Floor() {
        for (auto &c : d.cells) c = cell(kWall);
        d.active = true;
        d.pos = {33, 0, 4, 4, DungeonFacing::North};
        d.wanderer = {};
        d.wanderer.type = 255;
    }
    void put(int x, int y, uint8_t value) { d.cells[0 * 64 + y * 8 + x] = value; }
};

size_t resolve(const DungeonDrawOp &op, uint32_t phase, DungeonArtBlit *out) {
    return dungeon_art_blits(op, kCatalog, phase, out, kDungeonMaxBlitsPerOp);
}

// -- A1. The authored dimension tables are the containers' own ---------------
//
// [REF-TS] parseDngView/parseItemsView/parseMonView fix the slot counts and
// sizes; the exact byte-level identity of the packed images is proved
// separately by the node identity check (typescript_dungeon_art_identity),
// which reads DNG*/ITEMS/MON*.16 directly.  Here we only pin the invariants the
// GEOMETRY depends on, so a bad edit to the tables cannot silently move art.
void a1_dimension_invariants() {
    for (size_t i = 0; i < kDungeonWallImages; ++i) {
        const bool empty = i == 8 || i == 24; // depth-0 slots of front bases 8 and 24
        check((kDungeonWallDims[i].w == 0) == empty, "A1 wall empty slots are exactly 8 and 24");
        if (!empty) check(kDungeonWallDims[i].h == kDungeonSliceHeight, "A1 wall slice height 164");
    }
    // [REF-BIN] 0x2e62: the four rings of each side ABUT, left 16->96 and right
    // 96->176.  That is only true for these widths, so derive it, don't assume.
    for (int base : {0, 4, 16, 20}) {
        int left = kDungeonSideXLeft[0];
        for (int d = 0; d < 4; ++d) {
            check(left == kDungeonSideXLeft[d], "A1 left ring abuts");
            left += kDungeonWallDims[base + d].w;
        }
        check(left == kDungeonCenterX, "A1 left side closes on the centre");
        int right = kDungeonSideXRight[3];
        for (int d = 3; d >= 0; --d) {
            check(right == kDungeonSideXRight[d], "A1 right ring abuts");
            right += kDungeonWallDims[base + d].w;
        }
        check(right == 176, "A1 right side closes on the viewport edge");
    }
    // [REF-BIN] 0x2e72 = [56,72,80,88] is exactly 96 - w for the near feature
    // widths, which is why the resolver anchors pairs at 96 - w rather than
    // carrying a second table.
    const int16_t near_x[4] = {56, 72, 80, 88};
    for (int si = 0; si < 4; ++si)
        check(kDungeonCenterX - int(kDungeonItemDims[si].w) == near_x[si],
              "A1 feature X is 96-w (table 0x2e72)");
    // [REF-BIN] 0x2E2A = [72,80,88] is the same rule for the wanderer.
    const int16_t mon_x[3] = {72, 80, 88};
    for (int d = 0; d < 3; ++d)
        check(kDungeonCenterX - int(kDungeonMonDims[d].w) == mon_x[d],
              "A1 monster X is 96-w (table 0x2E2A)");
    // Both frames of a MON bank share a depth's size (frame*3 + depth-1).
    for (int d = 0; d < 3; ++d)
        check(kDungeonMonDims[d].w == kDungeonMonDims[d + 3].w &&
                  kDungeonMonDims[d].h == kDungeonMonDims[d + 3].h,
              "A1 monster frames share a depth's size");
}

// -- A2. Wall variant -> authored bank ---------------------------------------
//
// [REF-TS] paintDungeon resolves `rectsByVariant[wallVariant - 1]`; [NATIVE]
// dungeon_wall_variant() already produces V in {1,2,3} from the dungeon number.
void a2_wall_variant_selects_bank() {
    check(dungeon_art_wall_bank_index(1) == 0, "A2 V1 -> DNG1");
    check(dungeon_art_wall_bank_index(2) == 1, "A2 V2 -> DNG2");
    check(dungeon_art_wall_bank_index(3) == 2, "A2 V3 -> DNG3");
    check(dungeon_art_wall_bank_index(0) == 0, "A2 out-of-range low clamps to DNG1");
    check(dungeon_art_wall_bank_index(9) == 2, "A2 out-of-range high clamps to DNG3");

    // Every dungeon's variant maps to a real bank, via the proven derivation.
    for (uint8_t location = 0x20; location < 0x28; ++location) {
        const uint8_t v = dungeon_wall_variant(location);
        check(v >= 1 && v <= 3, "A2 derived variant is 1..3");
        check(dungeon_art_wall_bank_index(v) < kDungeonWallVariants, "A2 bank index in range");
    }
}

// -- A3. Cache identity -------------------------------------------------------
//
// ITEMS and MON0-7 are shared by every dungeon, so the wall bank is the WHOLE
// key: an ordinary turn or step inside one dungeon must never invalidate it,
// and entering a dungeon with a different variant must.
void a3_cache_key() {
    DungeonViewPlan a{}, b{};
    a.wall_variant = 1;
    b.wall_variant = 1;
    a.facing = DungeonFacing::North;
    b.facing = DungeonFacing::East;
    a.floor = 0;
    b.floor = 5;
    a.light_depth = kDungeonLitDepth;
    b.light_depth = 0;
    check(dungeon_art_cache_key(a) == dungeon_art_cache_key(b),
          "A3 turning, moving, changing level or losing light keeps the key");
    b.wall_variant = 3;
    check(dungeon_art_cache_key(a) != dungeon_art_cache_key(b),
          "A3 a different wall variant changes the key");
    check(dungeon_art_cache_key(b).wall_bank == 2, "A3 key carries the resolved bank index");

    // The device derives the key straight from the dungeon number rather than
    // building a whole plan just to read one field off it (alpha_runtime.cpp).
    // That shortcut is only safe while it agrees with the plan for EVERY
    // dungeon, which is what this asserts -- otherwise the corridor could be
    // painted from one variant while the plan described another.
    for (uint8_t dungeon = 0x20; dungeon < 0x28; ++dungeon) {
        Floor floor;
        floor.d.pos.dungeon = dungeon;
        GameState g{};
        g.torch_turns = 20;
        TurnState t{};
        const auto plan = plan_dungeon_view(g, t, floor.d);
        const DungeonArtCacheKey shortcut{dungeon_art_wall_bank_index(dungeon_wall_variant(dungeon))};
        check(dungeon_art_cache_key(plan) == shortcut,
              "A3 the runtime shortcut agrees with the plan for every dungeon");
    }
}

// -- A4. Side slices ----------------------------------------------------------
//
// [REF-TS] blitSlice: ONE blit at the plan's own X, natural size, Y = 14; the
// right-hand slice is mirrored.  The plan already chose the slice (base+depth),
// so the resolver must not re-derive it.
void a4_side_blits() {
    DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
    for (int base : {0, 4, 16, 20}) {
        for (uint8_t d = 0; d < 4; ++d) {
            const uint8_t slice = uint8_t(base + d);
            const size_t n = resolve(side_op(slice, d, DungeonSide::Left, kDungeonSideXLeft[d], false), 0, b);
            check(n == 1, "A4 a side slice is one blit");
            check(b[0].bank == DungeonArtBank::Wall, "A4 side comes from the wall bank");
            check(b[0].image == slice, "A4 side keeps the plan's slice index");
            check(b[0].x == kDungeonSideXLeft[d], "A4 side X is the plan's X");
            check(b[0].y == kDungeonSliceY, "A4 side Y is 14");
            check(b[0].w == kDungeonWallDims[slice].w && b[0].h == kDungeonSliceHeight,
                  "A4 side is drawn at natural size");
            check(!b[0].mirror && !b[0].vflip, "A4 left side is not flipped");

            const size_t m = resolve(side_op(slice, d, DungeonSide::Right, kDungeonSideXRight[d], true), 0, b);
            check(m == 1 && b[0].mirror && !b[0].vflip, "A4 right side is mirrored only");
            check(b[0].x == kDungeonSideXRight[d], "A4 right side X is the plan's X");
        }
    }
    // The four side bases are visually distinct art, not one slice recoloured:
    // plain wall 0-3, door 4-7, open passage 16-19, alcove 20-23.  A passage
    // and an alcove must not resolve to the same image at the same depth.
    DungeonArtBlit wall_b[kDungeonMaxBlitsPerOp]{}, door_b[kDungeonMaxBlitsPerOp]{};
    DungeonArtBlit pass_b[kDungeonMaxBlitsPerOp]{}, alcove_b[kDungeonMaxBlitsPerOp]{};
    resolve(side_op(uint8_t(dungeon_side_slice_base(kWall) + 1), 1, DungeonSide::Left, 40, false), 0, wall_b);
    resolve(side_op(uint8_t(dungeon_side_slice_base(kNormalDoor) + 1), 1, DungeonSide::Left, 40, false), 0, door_b);
    resolve(side_op(uint8_t(dungeon_side_slice_base(kNothing) + 1), 1, DungeonSide::Left, 40, false), 0, pass_b);
    resolve(side_op(uint8_t(dungeon_side_slice_base(kSpecialWall) + 1), 1, DungeonSide::Left, 40, false), 0, alcove_b);
    check(wall_b[0].image == 1, "A4 plain side wall -> image 1 at depth 1");
    check(door_b[0].image == 5, "A4 side door -> image 5 at depth 1");
    check(pass_b[0].image == 17, "A4 side passage -> image 17 at depth 1");
    check(alcove_b[0].image == 21, "A4 side alcove -> image 21 at depth 1");
    check(pass_b[0].image != alcove_b[0].image, "A4 passage and alcove are different art");
}

// -- A5. Front wall: the centred mirrored PAIR -------------------------------
//
// [REF-TS] drawFront: left half normal at 96-w, right half mirrored at the
// binary's X override 0x60.  Together they cover [96-w, 96+w) with no gap and
// no overlap.
void a5_front_blits() {
    DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
    for (int base : {8, 12, 24}) {
        for (uint8_t d = 1; d <= 3; ++d) {
            const uint8_t slice = uint8_t(base + d);
            const size_t n = resolve(front_op(slice, d), 0, b);
            check(n == 2, "A5 a front wall is a pair");
            const uint16_t w = kDungeonWallDims[slice].w;
            check(b[0].image == slice && b[1].image == slice, "A5 both halves are the same image");
            check(b[0].x == int16_t(kDungeonCenterX - int(w)) && !b[0].mirror,
                  "A5 left half ends at the centre, unmirrored");
            check(b[1].x == kDungeonCenterX && b[1].mirror,
                  "A5 right half is anchored at 0x60, mirrored");
            check(b[0].x + int(w) == b[1].x, "A5 the halves abut exactly");
            check(b[0].y == kDungeonSliceY && b[1].y == kDungeonSliceY, "A5 front Y is 14");
            check(!b[0].vflip && !b[1].vflip, "A5 a front wall is never flipped vertically");
        }
    }
    // A DOOR ahead and a blank dead end are different authored art -- the
    // Batch 9 defect this closes was that both painted the same grey wall.
    check(dungeon_front_slice_base(kWall) == 8, "A5 wall ahead -> dead-end base");
    check(dungeon_front_slice_base(kNormalDoor) == 12, "A5 door ahead -> door base");
    check(dungeon_front_slice_base(kSpecialWall) == 0x18, "A5 special wall ahead -> its own base");
    resolve(front_op(uint8_t(dungeon_front_slice_base(kWall) + 1), 1), 0, b);
    const uint8_t dead_end = b[0].image;
    resolve(front_op(uint8_t(dungeon_front_slice_base(kNormalDoor) + 1), 1), 0, b);
    check(b[0].image != dead_end, "A5 a front door is not the dead-end image");
    // The empty depth-0 slots of bases 8 and 24 must resolve to nothing rather
    // than to a bogus box -- the plan never emits them, so this is a guard.
    check(resolve(front_op(8, 0), 0, b) == 0, "A5 empty front slot resolves to no blit");
    check(resolve(front_op(24, 0), 0, b) == 0, "A5 empty special front slot resolves to no blit");
}

// -- A6. Features -------------------------------------------------------------
//
// [REF-BIN] fn_1952: the FLIPPED up-ladder block (table 0x2f16) is drawn first,
// the normal block (0x2f1e) second, each as a centred pair.  Y comes from
// 0x2e82 (flipped), 0x2e7a (image >= 8, low objects) or the constant horizon.
void a6_feature_blits() {
    DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
    struct Case { uint8_t kind; int image_base; bool flipped; const char *what; };
    const int16_t up_y[4] = {15, 39, 71, 87};    // 0x2e82
    const int16_t floor_y[4] = {152, 120, 104, 96}; // 0x2e7a
    const Case cases[] = {
        {kLadderDown, 0, false, "A6 ladder down -> images 0-3"},
        {kFountain, 4, false, "A6 fountain -> images 4-7"},
        {kTrap, 8, false, "A6 trap -> images 8-11"},
        {kChest, 12, false, "A6 closed chest -> images 12-15"},
        {kOpenChest, 16, false, "A6 open chest -> images 16-19"},
        {kLadderUp, 0, true, "A6 ladder up -> images 0-3, flipped"},
    };
    for (const auto &c : cases) {
        for (uint8_t si = 0; si <= 3; ++si) {
            const size_t n = resolve(feature_op(c.kind, 0, si), 0, b);
            check(n == 2, "A6 one feature block is a pair");
            const int image = c.image_base + si;
            check(b[0].bank == DungeonArtBank::Items, c.what);
            check(b[0].image == image && b[1].image == image, c.what);
            // The image at depth si, with NO off-by-one.
            const uint16_t w = kDungeonItemDims[image].w;
            check(b[0].x == int16_t(kDungeonCenterX - int(w)) && !b[0].mirror,
                  "A6 feature left half at 96-w");
            check(b[1].x == kDungeonCenterX && b[1].mirror, "A6 feature right half mirrored at 96");
            check(b[0].vflip == c.flipped && b[1].vflip == c.flipped, "A6 up-ladder is V-flipped");
            const int16_t expect_y =
                c.flipped ? up_y[si] : image >= 8 ? floor_y[si] : int16_t(96);
            check(b[0].y == expect_y && b[1].y == expect_y, "A6 feature Y table");
        }
    }
    // LadderUpDown carries BOTH blocks, flipped one FIRST (fn_1952 0x1984 then
    // 0x19ae) -- four blits whose order is the original's paint order.
    const size_t n = resolve(feature_op(kLadderUpDown, 0, 1), 0, b);
    check(n == 4, "A6 up-and-down ladder emits both blocks");
    check(b[0].vflip && b[1].vflip, "A6 the flipped up block is painted first");
    check(!b[2].vflip && !b[3].vflip, "A6 the normal down block is painted second");
    check(b[0].y == 39 && b[2].y == 96, "A6 the two blocks use their own Y tables");

    // A magic field has no ITEMS art at all (sparkle subsystem), and a trap is
    // gated on the tile's low three bits -- the resolver must emit nothing
    // rather than invent a box.
    check(resolve(feature_op(kMagicField, 0, 1), 0, b) == 0, "A6 magic field has no authored art");
    check(resolve(feature_op(kTrap, 1, 1), 0, b) == 0, "A6 a gated trap draws nothing");
    check(resolve(feature_op(kTrap, 8, 1), 0, b) == 2, "A6 sub&7==0 still draws the trap");
    check(resolve(feature_op(kNothing, 0, 1), 0, b) == 0, "A6 an empty cell has no feature art");
    check(resolve(feature_op(kWall, 0, 1), 0, b) == 0, "A6 a wall cell has no feature art");
}

// -- A7. The wanderer ---------------------------------------------------------
//
// [REF-TS] drawMonster: two mirrored halves, sprite = frame*3 + depth-1; X from
// 0x2E2A, Y from 0x2E32 where row 1 is the CEILING.
void a7_monster_blits() {
    DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
    const int16_t floor_y[3] = {86, 96, 98};
    const int16_t ceil_y[3] = {40, 70, 85};
    for (uint8_t bank = 0; bank < kDungeonMonBanks; ++bank) {
        for (uint8_t d = 1; d <= 3; ++d) {
            const size_t n = resolve(monster_op(bank, d, false), 0, b);
            check(n == 2, "A7 the wanderer is a mirrored pair");
            check(b[0].bank == DungeonArtBank::Monster, "A7 wanderer comes from a MON bank");
            check(b[0].mon_bank == bank && b[1].mon_bank == bank, "A7 both halves use the plan's bank");
            check(b[0].x == int16_t(kDungeonCenterX - int(kDungeonMonDims[d - 1].w)) && !b[0].mirror,
                  "A7 left half at 96-w");
            check(b[1].x == kDungeonCenterX && b[1].mirror, "A7 right half mirrored at 96");
            check(b[0].y == floor_y[d - 1], "A7 floor row Y (table 0x2E32 row 0)");

            resolve(monster_op(bank, d, true), 0, b);
            check(b[0].y == ceil_y[d - 1] && b[1].y == ceil_y[d - 1],
                  "A7 ceiling row Y (table 0x2E32 row 1)");
            check(b[0].y < floor_y[d - 1], "A7 a ceiling wanderer sits above the floor one");
        }
    }
    // The two halves animate independently: phase bit 0 picks the left frame,
    // bit 1 the right (drawMonster's frameA / frameB).
    resolve(monster_op(0, 1, false), 0, b);
    check(b[0].image == 0 && b[1].image == 0, "A7 phase 0 -> frame 0 both halves");
    resolve(monster_op(0, 1, false), 1, b);
    check(b[0].image == 3 && b[1].image == 0, "A7 phase 1 -> left frame 1");
    resolve(monster_op(0, 1, false), 2, b);
    check(b[0].image == 0 && b[1].image == 3, "A7 phase 2 -> right frame 1");
    resolve(monster_op(0, 2, false), 3, b);
    check(b[0].image == 4 && b[1].image == 4, "A7 sprite index is frame*3 + depth-1");
    // Depth 0 is the cell the party stands on: the wanderer is never drawn
    // there (the plan already gates it, this is the resolver's own guard).
    check(resolve(monster_op(0, 0, false), 0, b) == 0, "A7 no wanderer at depth 0");
    check(resolve(monster_op(kDungeonMonBanks, 1, false), 0, b) == 0, "A7 bank out of range draws nothing");
}

// -- A8. Paint order ----------------------------------------------------------
//
// The composite order is: black background, then the plan's ops IN ORDER, each
// expanded to its blits IN ORDER.  The plan already emits corridor rings
// near->far with the front wall at the first blocker, then contents far->near;
// what this asserts is that expansion PRESERVES that, so nothing authored can
// reorder the picture behind the plan's back.
void a8_paint_order() {
    // A corridor: party at (4,4) facing north, open to (4,1), chest at (4,2),
    // wanderer on (4,3), dead end beyond.
    Floor floor;
    DungeonState &d = floor.d;
    floor.put(4, 4, cell(kNothing));
    floor.put(4, 3, cell(kNothing));
    floor.put(4, 2, cell(kChest));
    floor.put(4, 1, cell(kNothing));
    GameState g{};
    g.torch_turns = 20; // a burning torch: the plan's own light gate
    TurnState t{};

    const auto plan = plan_dungeon_view(g, t, d);
    check(plan.lit, "A8 the fixture is lit");
    check(plan.count > 0, "A8 the fixture produces a plan");

    DungeonArtBlit flat[kDungeonMaxOps * kDungeonMaxBlitsPerOp]{};
    size_t total = 0;
    int first_front = -1, last_side = -1, first_feature = -1;
    for (uint8_t i = 0; i < plan.count; ++i) {
        DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
        const size_t n = resolve(plan.ops[i], 0, b);
        if (plan.ops[i].kind == DungeonOpKind::Front && first_front < 0) first_front = int(total);
        if (plan.ops[i].kind == DungeonOpKind::Side) last_side = int(total);
        if (plan.ops[i].kind == DungeonOpKind::Feature && first_feature < 0)
            first_feature = int(total);
        for (size_t k = 0; k < n; ++k) flat[total++] = b[k];
    }
    check(total > 0, "A8 the plan expands to authored blits");
    check(total <= kDungeonMaxOps * kDungeonMaxBlitsPerOp, "A8 expansion fits the bound");
    check(last_side >= 0, "A8 the corridor emits side slices");
    if (first_front >= 0 && last_side >= 0)
        check(first_front > last_side, "A8 the front wall is painted after the side slices");
    if (first_feature >= 0 && first_front >= 0)
        check(first_feature > first_front, "A8 contents are painted over the corridor");

    // Side rings arrive near->far, so a nearer slice is always painted before a
    // farther one and the farther one can never overdraw it.
    int previous_depth = -1;
    for (uint8_t i = 0; i < plan.count; ++i) {
        if (plan.ops[i].kind != DungeonOpKind::Side) continue;
        check(int(plan.ops[i].depth) >= previous_depth, "A8 side rings are emitted near->far");
        previous_depth = int(plan.ops[i].depth);
    }
    // Every blit the corridor paints lands inside the 176x176 viewport's
    // horizontal span; the slices' baked-in floor and ceiling fill it.
    for (size_t i = 0; i < total; ++i) {
        check(flat[i].x >= 0 && flat[i].x + int(flat[i].w) <= 176, "A8 blit fits the viewport width");
        check(flat[i].w > 0 && flat[i].h > 0, "A8 no zero-sized blit is emitted");
    }
}

// -- A9. An unlit view has no authored blits at all --------------------------
//
// Batch 9's light gate must survive: with neither torch nor light spell the
// plan is empty, so no authored art can leak into a black viewport.
void a9_unlit_draws_nothing() {
    DungeonState d{};
    for (auto &c : d.cells) c = cell(kNothing);
    d.active = true;
    d.pos = {33, 0, 4, 4, DungeonFacing::North};
    d.wanderer = {};
    d.wanderer.type = 255;
    GameState g{};
    TurnState t{};
    const auto plan = plan_dungeon_view(g, t, d);
    check(!plan.lit, "A9 no light source leaves the plan unlit");
    check(plan.count == 0, "A9 an unlit plan emits no ops");
    size_t total = 0;
    for (uint8_t i = 0; i < plan.count; ++i) {
        DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
        total += resolve(plan.ops[i], 0, b);
    }
    check(total == 0, "A9 an unlit view paints no authored art");
}

// -- A10. A null catalog degrades safely -------------------------------------
//
// The device paints the plan even if the art failed to load from SD; the
// resolver must then emit nothing rather than dereference a missing bank.
void a10_missing_catalog() {
    DungeonArtCatalog empty{};
    DungeonArtBlit b[kDungeonMaxBlitsPerOp]{};
    check(dungeon_art_blits(side_op(1, 1, DungeonSide::Left, 40, false), empty, 0, b,
                            kDungeonMaxBlitsPerOp) == 0,
          "A10 no wall bank -> no side blit");
    check(dungeon_art_blits(front_op(9, 1), empty, 0, b, kDungeonMaxBlitsPerOp) == 0,
          "A10 no wall bank -> no front blit");
    check(dungeon_art_blits(feature_op(kChest, 0, 1), empty, 0, b, kDungeonMaxBlitsPerOp) == 0,
          "A10 no items bank -> no feature blit");
    check(dungeon_art_blits(monster_op(0, 1, false), empty, 0, b, kDungeonMaxBlitsPerOp) == 0,
          "A10 no mon bank -> no wanderer blit");
    // A capacity too small for a pair must write nothing, not half a pair.
    check(dungeon_art_blits(front_op(9, 1), kCatalog, 0, b, 1) == 0,
          "A10 a pair is all-or-nothing against capacity");
}

// -- A11. The PACKED container ------------------------------------------------
//
// `dungeon_art_parse` is the only thing standing between a corrupt SD card and a
// wild pointer the painter would dereference on every redraw for the rest of the
// session, so it is exercised here against both a well-formed container and a
// catalogue of specific corruptions.  The bytes are synthesised rather than read
// from a pack: the pack's own contents are proved separately, byte for byte, by
// the node identity check, and a unit test that needed the user's original data
// could not run at all without it.
struct Container {
    uint8_t bytes[4096]{};
    size_t length = 0;
};

/** Build a valid `banks x images` container of `w x h` images. */
Container make_container(size_t banks, size_t images, uint16_t w, uint16_t h, bool masked) {
    Container c{};
    const size_t slots = banks * images;
    const size_t table = kDungeonArtHeaderBytes + slots * kDungeonArtImageBytes;
    const size_t pixel_bytes = size_t(w >> 1) * h;
    const size_t mask_bytes = masked ? size_t(w >> 3) * h : 0;
    c.length = table + slots * (pixel_bytes + mask_bytes);
    for (size_t i = 0; i < sizeof(kDungeonArtMagic); ++i) c.bytes[i] = uint8_t(kDungeonArtMagic[i]);
    auto put16 = [&c](size_t at, uint16_t v) {
        c.bytes[at] = uint8_t(v & 0xff);
        c.bytes[at + 1] = uint8_t(v >> 8);
    };
    auto put32 = [&c](size_t at, uint32_t v) {
        for (size_t b = 0; b < 4; ++b) c.bytes[at + b] = uint8_t((v >> (8 * b)) & 0xff);
    };
    put16(8, uint16_t(banks));
    put16(10, uint16_t(images));
    put16(12, masked ? kDungeonArtFlagMasked : uint16_t(0));
    size_t cursor = table;
    for (size_t i = 0; i < slots; ++i) {
        const size_t at = kDungeonArtHeaderBytes + i * kDungeonArtImageBytes;
        put16(at, w);
        put16(at + 2, h);
        put32(at + 4, uint32_t(cursor));
        // A recognisable fill, so a wrongly indexed slot is visible as such.
        for (size_t b = 0; b < pixel_bytes; ++b) c.bytes[cursor + b] = uint8_t(i * 0x11 + b);
        cursor += pixel_bytes;
        if (masked) {
            put32(at + 8, uint32_t(cursor));
            cursor += mask_bytes;
        }
    }
    return c;
}

void a11_packed_container() {
    DungeonArtSurface out[16]{};
    {
        Container c = make_container(2, 3, 16, 4, true);
        check(dungeon_art_parse(c.bytes, c.length, 2, 3, true, out, 16), "A11 a valid container parses");
        for (size_t i = 0; i < 6; ++i) {
            check(out[i].w == 16 && out[i].h == 4, "A11 every slot keeps its size");
            check(out[i].pixels != nullptr && out[i].mask != nullptr, "A11 masked slots carry both spans");
            check(out[i].pixels >= c.bytes && out[i].pixels < c.bytes + c.length,
                  "A11 pixel pointers land inside the entry");
            check(out[i].mask >= c.bytes && out[i].mask < c.bytes + c.length,
                  "A11 mask pointers land inside the entry");
        }
        // Slots are indexed BANK-MAJOR: bank 1 image 0 is slot 3, not slot 1.
        check(out[3].pixels[0] == 0x33, "A11 the image table is bank-major");
        // An unmasked container must not hand out a mask pointer.
        Container plain = make_container(1, 4, 8, 4, false);
        check(dungeon_art_parse(plain.bytes, plain.length, 1, 4, false, out, 16),
              "A11 an unmasked container parses");
        check(out[0].mask == nullptr, "A11 an unmasked slot has no mask");
    }
    // Rejections.  Each mutates ONE thing about an otherwise valid container, so
    // a pass cannot be an accident of some other check firing.
    {
        Container c = make_container(1, 4, 16, 4, true);
        DungeonArtSurface probe[16]{};
        auto rejects = [&](Container broken, const char *what) {
            for (auto &surface : probe) surface = {};
            check(!dungeon_art_parse(broken.bytes, broken.length, 1, 4, true, probe, 16), what);
            check(probe[0].pixels == nullptr, "A11 a rejected container indexes nothing");
        };
        Container bad_magic = c;
        bad_magic.bytes[0] = 'X';
        rejects(bad_magic, "A11 wrong magic is rejected");
        Container bad_banks = c;
        bad_banks.bytes[8] = 2;
        rejects(bad_banks, "A11 a different bank count is rejected");
        Container bad_images = c;
        bad_images.bytes[10] = 5;
        rejects(bad_images, "A11 a different image count is rejected");
        Container bad_flag = c;
        bad_flag.bytes[12] = 0;
        rejects(bad_flag, "A11 a mask-flag disagreement is rejected");
        Container truncated = c;
        truncated.length = c.length - 1;
        rejects(truncated, "A11 a truncated entry is rejected");
        Container short_header = c;
        short_header.length = kDungeonArtHeaderBytes + 3;
        rejects(short_header, "A11 an entry shorter than its own table is rejected");
        // A pixel span that starts inside the entry but ENDS past it: the case a
        // naive `offset < length` check would wave through.
        const size_t at = kDungeonArtHeaderBytes;
        Container overrun = c;
        overrun.bytes[at + 4] = uint8_t((c.length - 2) & 0xff);
        overrun.bytes[at + 5] = uint8_t(((c.length - 2) >> 8) & 0xff);
        rejects(overrun, "A11 a pixel span ending past the entry is rejected");
        Container mask_overrun = c;
        mask_overrun.bytes[at + 8] = uint8_t((c.length - 2) & 0xff);
        mask_overrun.bytes[at + 9] = uint8_t(((c.length - 2) >> 8) & 0xff);
        rejects(mask_overrun, "A11 a mask span ending past the entry is rejected");
        // An offset pointing into the table itself would alias the descriptors.
        Container into_table = c;
        into_table.bytes[at + 4] = 0;
        into_table.bytes[at + 5] = 0;
        rejects(into_table, "A11 an offset inside the header is rejected");
        // Widths the 4bpp / 1bpp strides cannot express.
        Container odd_width = c;
        odd_width.bytes[at] = 15;
        rejects(odd_width, "A11 an odd width is rejected");
        Container unaligned_mask = c;
        unaligned_mask.bytes[at] = 4; // even, but not a whole mask byte
        rejects(unaligned_mask, "A11 a masked width below 8 is rejected");
    }
    // An EMPTY slot (the DNG containers really have two) is legal and simply
    // paints nothing -- it must not be mistaken for corruption.
    {
        Container c = make_container(1, 4, 16, 4, false);
        for (size_t b = 0; b < 4; ++b) c.bytes[kDungeonArtHeaderBytes + b] = 0;
        DungeonArtSurface probe[4]{};
        check(dungeon_art_parse(c.bytes, c.length, 1, 4, false, probe, 4),
              "A11 an empty slot is legal");
        check(probe[0].pixels == nullptr && probe[0].w == 0, "A11 an empty slot indexes nothing");
        check(probe[1].pixels != nullptr, "A11 the slots after an empty one still index");
    }
    // Too small an output array must be refused rather than overrun.
    {
        Container c = make_container(2, 3, 8, 2, false);
        DungeonArtSurface probe[4]{};
        check(!dungeon_art_parse(c.bytes, c.length, 2, 3, false, probe, 4),
              "A11 an undersized output array is refused");
    }
    check(!dungeon_art_parse(nullptr, 100, 1, 1, false, out, 16), "A11 a null entry is refused");
}

// -- A12. Surface lookup ------------------------------------------------------
//
// The painter asks for a blit's surface once per blit; an out-of-range bank,
// slot or monster bank must come back null so the corridor keeps its black
// rather than reading past an array.
void a12_surface_lookup() {
    static uint8_t pixel = 0;
    DungeonArtSurface wall[kDungeonWallImages]{};
    DungeonArtSurface items[kDungeonItemImages]{};
    DungeonArtSurface mon[kDungeonMonBanks * kDungeonMonImages]{};
    for (auto &surface : wall) surface = {&pixel, nullptr, 8, 8};
    for (auto &surface : items) surface = {&pixel, &pixel, 8, 8};
    for (auto &surface : mon) surface = {&pixel, &pixel, 8, 8};
    wall[8] = {}; // the real empty container slots
    wall[24] = {};

    DungeonArtSurfaces surfaces{};
    surfaces.wall = wall;
    surfaces.items = items;
    surfaces.mon = mon;

    DungeonArtBlit blit{};
    blit.bank = DungeonArtBank::Wall;
    blit.image = 1;
    check(dungeon_art_surface(surfaces, blit) == &wall[1], "A12 a wall blit finds its slice");
    blit.image = 8;
    check(dungeon_art_surface(surfaces, blit) == nullptr, "A12 an empty wall slot resolves to null");
    blit.image = uint8_t(kDungeonWallImages);
    check(dungeon_art_surface(surfaces, blit) == nullptr, "A12 a wall index past the bank is null");

    blit.bank = DungeonArtBank::Items;
    blit.image = 19;
    check(dungeon_art_surface(surfaces, blit) == &items[19], "A12 a feature blit finds its image");
    blit.image = uint8_t(kDungeonItemImages);
    check(dungeon_art_surface(surfaces, blit) == nullptr, "A12 a feature index past the bank is null");

    blit.bank = DungeonArtBank::Monster;
    blit.image = 4;
    blit.mon_bank = 7;
    check(dungeon_art_surface(surfaces, blit) == &mon[7 * kDungeonMonImages + 4],
          "A12 a wanderer blit indexes bank-major");
    blit.mon_bank = uint8_t(kDungeonMonBanks);
    check(dungeon_art_surface(surfaces, blit) == nullptr, "A12 a bank past MON7 is null");

    // Nothing resident at all: every lookup is null, which is what makes the
    // renderer's "paint black rather than guess" behaviour safe.
    DungeonArtSurfaces none{};
    blit.mon_bank = 0;
    for (auto bank : {DungeonArtBank::Wall, DungeonArtBank::Items, DungeonArtBank::Monster}) {
        blit.bank = bank;
        blit.image = 0;
        check(dungeon_art_surface(none, blit) == nullptr, "A12 an empty cache resolves to null");
    }
}

// -- A13. The EGA palette the painter indexes through -------------------------
void a13_palette() {
    // Black must be exactly 0, because it is also the viewport's own clear
    // colour and the vanishing point beyond the light.
    check(kDungeonEgaRgb565[0] == 0x0000, "A13 index 0 is black");
    check(kDungeonEgaRgb565[15] == 0xffff, "A13 index 15 is white");
    // The conversion rule is (r&0xf8)<<8 | (g&0xfc)<<3 | b>>3 over EGA 16.
    const uint8_t ega[16][3] = {
        {0, 0, 0},     {0, 0, 170},    {0, 170, 0},    {0, 170, 170},
        {170, 0, 0},   {170, 0, 170},  {170, 85, 0},   {170, 170, 170},
        {85, 85, 85},  {85, 85, 255},  {85, 255, 85},  {85, 255, 255},
        {255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255},
    };
    for (int i = 0; i < 16; ++i) {
        const uint16_t want = uint16_t(((ega[i][0] & 0xf8) << 8) | ((ega[i][1] & 0xfc) << 3) |
                                       (ega[i][2] >> 3));
        check(kDungeonEgaRgb565[i] == want, "A13 EGA entry converts by the project's rule");
    }
    // All sixteen are distinct: a collision would make two authored colours
    // indistinguishable on the panel.
    for (int i = 0; i < 16; ++i)
        for (int j = i + 1; j < 16; ++j)
            check(kDungeonEgaRgb565[i] != kDungeonEgaRgb565[j], "A13 no two EGA entries collide");
}

// A14 -- Batch 12B.  The magic field's PROCEDURAL strokes
// (`magic_field_sparkle_drawer` @0x127e).  A field still has no ITEMS.16 image,
// so a6 above still expects zero blits for it; what is proved here is that the
// subsystem the original uses instead exists, carries the reference tables, and
// stays inside its box.
//   [REF-BIN] DUNGEON.OVL 0x127e-0x1346, tables DS 0x2e42/0x2e4a/0x2e52/0x2e5a,
//             colour switch 0x1292 with the `add ax, 8` at 0x12b7, caller
//             `feature_overlay_drawer_by_nibble` @0x19f6.
//   [REF-TS]  game/src/skin/fiel/dungeon-decor.ts `fieldSparkRects` and its
//             FIELD_SPARK_{COUNT,LO,HI,LEN,COLOR} tables.
//   [NOTE]    re/notes/dungeon-decor-mazmorra.md sections 2, 5 and 6 ticket 4.
void a14_field_sparks() {
    const uint16_t counts[4] = {300, 100, 50, 15};
    const int16_t lo[4] = {16, 56, 80, 92}, hi[4] = {167, 135, 111, 99}, len[4] = {7, 7, 5, 2};
    const uint8_t colors[4] = {10, 9, 10, 9};
    for (int d = 0; d < 4; ++d) {
        check(kDungeonFieldSparkCount[d] == counts[d], "A14 stroke count matches DS 0x2e52");
        check(kDungeonFieldSparkLo[d] == lo[d], "A14 box low edge matches DS 0x2e42");
        check(kDungeonFieldSparkHi[d] == hi[d], "A14 box high edge matches DS 0x2e4a");
        check(kDungeonFieldSparkLen[d] == len[d], "A14 stroke length matches DS 0x2e5a");
        check(kDungeonFieldSparkColor[d] == colors[d], "A14 colour matches the 0x1292 switch");
        check(dungeon_field_spark_count(uint8_t(d)) == counts[d],
              "A14 the accessor reports the table's own count");
    }
    check(dungeon_field_spark_count(4) == 0, "A14 a depth past 3 draws nothing");
    check(dungeon_field_spark(4, 2, 0, 0).w == 0, "A14 a depth past 3 yields an empty stroke");

    // A field still has no authored image -- the blit layer must stay silent, or
    // the two subsystems would both draw it.
    {
        DungeonDrawOp op{};
        op.kind = DungeonOpKind::Feature;
        op.depth = 1;
        op.cell_type = 0x8;
        op.sub = 2;
        DungeonArtBlit blits[kDungeonMaxBlitsPerOp]{};
        check(dungeon_art_blits(op, dungeon_art_authored_catalog(), 0, blits,
                                kDungeonMaxBlitsPerOp) == 0,
              "A14 a magic field still emits no ITEMS.16 blit");
    }

    for (uint8_t d = 0; d < 4; ++d) {
        for (uint8_t sub = 0; sub < 4; ++sub) {
            const uint16_t n = dungeon_field_spark_count(d);
            for (uint16_t i = 0; i < n; ++i) {
                const auto s = dungeon_field_spark(d, sub, 7, i);
                check(s.w == uint8_t(len[d] + 1),
                      "A14 the inclusive hline is len+1 pixels wide");
                check(s.color == colors[sub], "A14 the colour is chosen by the field TYPE");
                check(s.x >= lo[d] && s.x + int16_t(s.w) - 1 <= hi[d],
                      "A14 the whole stroke stays inside the box");
                check(s.y >= lo[d] && s.y <= hi[d], "A14 the stroke's row stays inside the box");
            }
            check(dungeon_field_spark(d, sub, 7, uint16_t(n)).w == 0,
                  "A14 an index past the count yields an empty stroke");
        }
    }
    // The bit-3 carrier the dungeon cast preserves must not change the colour:
    // the binary passes `tile & 7` (caller @0x19f6), so 0x8a is type 2.
    check(dungeon_field_spark(1, 0xa, 3, 0).color == dungeon_field_spark(1, 0x2, 3, 0).color,
          "A14 bit 3 of the tile does not reach the colour switch");

    // Pure: the same coordinates always reproduce the same stroke, so a host
    // test can assert the picture; a new phase re-rolls it, as a redraw does.
    check(dungeon_field_spark(0, 2, 11, 5).x == dungeon_field_spark(0, 2, 11, 5).x &&
              dungeon_field_spark(0, 2, 11, 5).y == dungeon_field_spark(0, 2, 11, 5).y,
          "A14 a stroke is reproducible from (depth, sub, phase, index)");
    int moved = 0;
    for (uint16_t i = 0; i < 100; ++i) {
        const auto a = dungeon_field_spark(0, 2, 0, i);
        const auto b = dungeon_field_spark(0, 2, 1, i);
        if (a.x != b.x || a.y != b.y) ++moved;
    }
    check(moved > 50, "A14 advancing the phase re-rolls the field, as a redraw does");
    // And the strokes are not all stacked on one row -- the box is really filled.
    int distinct_rows = 0;
    bool seen[256]{};
    for (uint16_t i = 0; i < dungeon_field_spark_count(0); ++i) {
        const auto s = dungeon_field_spark(0, 2, 0, i);
        if (s.y >= 0 && s.y < 256 && !seen[s.y]) {
            seen[s.y] = true;
            ++distinct_rows;
        }
    }
    check(distinct_rows > 50, "A14 the strokes spread across the box, not one row");
}

} // namespace

int main() {
    a1_dimension_invariants();
    a2_wall_variant_selects_bank();
    a3_cache_key();
    a4_side_blits();
    a5_front_blits();
    a6_feature_blits();
    a7_monster_blits();
    a8_paint_order();
    a9_unlit_draws_nothing();
    a10_missing_catalog();
    a11_packed_container();
    a12_surface_lookup();
    a13_palette();
    a14_field_sparks();
    if (failures != 0) {
        std::cerr << failures << " dungeon_art regression failure(s)\n";
        return 1;
    }
    std::cout << "dungeon_art regression: all checks passed\n";
    return 0;
}
