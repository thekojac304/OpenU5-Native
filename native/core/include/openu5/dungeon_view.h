#pragma once
#include "dungeon.h"
#include "state.h"
#include "turn.h"
#include <cstddef>
#include <cstdint>

namespace openu5 {

// ---------------------------------------------------------------------------
// R-05 -- the dungeon first-person view PLAN.
//
// The original does not vectorise the corridor: it COMPOSES it by blitting
// pre-drawn perspective slices at a fixed X per depth.  Which slice goes where
// is pure logic, driven entirely by the live DungeonState, and it is the half
// of the view that a host test can prove.  The device renderer (which owns the
// pixels) consumes this plan; it does not re-derive it.
//
// Authoritative model, in evidence order:
//   * DUNGEON.OVL `dng_draw_view` @0x1a90 (driver), `fn_1682` @0x1682 (sides),
//     `fn_150a` @0x150a (front wall), `fn_1952` @0x1952 (features),
//     `dng_blit_piece` @0x134A (the blit itself);
//   * the project reference implementation `game/src/skin/fiel/dungeon.ts`
//     (`planDungeonView`, `sideBase`, `frontBase`, `blocksView`, `featureBlits`)
//     and `game/src/skin/coreview.ts` (`dungeonView`, the lit cone);
//   * `game/src/core/dungeon/light.ts` (`visibleDepth`) for the light gate.
// ---------------------------------------------------------------------------

/** Cell kind = high nibble of a dungeon cell byte (DNGLOOK dict). */
enum class DungeonCellKind : uint8_t {
    Nothing = 0x0,
    LadderUp = 0x1,
    LadderDown = 0x2,
    LadderUpDown = 0x3,
    Chest = 0x4,
    Fountain = 0x5,
    Trap = 0x6,
    OpenChest = 0x7,
    MagicField = 0x8,
    RoomsBroke = 0xa,
    Wall = 0xb,
    SpecialWall = 0xc,
    SecretDoor = 0xd,
    NormalDoor = 0xe,
    Room = 0xf,
};

// Compositor geometry (DATA.OVL, DS+0x10; verified byte-for-byte upstream).
// 0x2e62: screen X of a side slice by [side][depth].  The four rings of each
// side abut (left 16->96, right 96->176).
constexpr int16_t kDungeonSideXLeft[4] = {16, 40, 72, 88};
constexpr int16_t kDungeonSideXRight[4] = {152, 120, 104, 96};
/** Screen Y of the top of every slice (0x134A `push 0xe`). */
constexpr int16_t kDungeonSliceY = 14;
/** Native slice height; floor speckle and ceiling are baked into each slice. */
constexpr int16_t kDungeonSliceHeight = 164;
/** The binary's X override for the mirrored half of a centred pair (0x60). */
constexpr int16_t kDungeonCenterX = 0x60;
/** Deepest ring the driver marches (`planDungeonView`: min(lightDepth, 3)). */
constexpr int kDungeonMaxDepth = 3;
/** Ray length with any light source at all (light.ts `visibleDepth`). */
constexpr int kDungeonLitDepth = 4;

enum class DungeonOpKind : uint8_t { Side, Front, Feature, Monster };
enum class DungeonSide : uint8_t { Left, Right };

/** One entry of the deterministic draw list for the current view. */
struct DungeonDrawOp {
    DungeonOpKind kind = DungeonOpKind::Side;
    /** Ring 0..3; 0 is the cell the party stands on. */
    uint8_t depth = 0;
    /** Side/Front: slice piece code = base + depth. */
    uint8_t slice = 0;
    DungeonSide side = DungeonSide::Left;
    /** Side: screen X of the slice's left edge. */
    int16_t x = 0;
    /** Side: the right-hand slice is blitted mirrored (tunnel symmetry). */
    bool mirror = false;
    /** Feature: the cell's kind and sub-nibble. */
    uint8_t cell_type = 0, sub = 0;
    /** Monster: MON0-7 sprite bank, and the ceiling row flag. */
    uint8_t bank = 0;
    bool ceiling = false;
};

/**
 * Worst case: 4 rings x 2 side slices + 1 front wall + 4 features + 3 monster
 * sightings = 16.  32 leaves headroom without putting the plan on the heap.
 */
constexpr size_t kDungeonMaxOps = 32;

struct DungeonViewPlan {
    DungeonDrawOp ops[kDungeonMaxOps]{};
    uint8_t count = 0;
    /** False = total darkness: the viewport is black and `count` is 0. */
    bool lit = false;
    /** 0 unlit, otherwise kDungeonLitDepth. */
    uint8_t light_depth = 0;
    /** 1/2/3 -> DNG1 olive / DNG2 red / DNG3 grey. */
    uint8_t wall_variant = 1;
    uint8_t floor = 0;
    DungeonFacing facing = DungeonFacing::North;
};

/**
 * Lit ray length.  DUNGEON:0x1AD6-0x1AE4 gates on `g_light_spell_mins == 0 AND
 * g_torch_mins == 0`; the gate does not distinguish a torch from In Lor, so
 * either source gives the same depth.  This is the same double gate
 * `dungeon.cpp`'s (S)earch already applies before printing "darkness."
 */
int dungeon_visible_depth(const GameState &, const TurnState &);

/**
 * Special-wall variant `g_dng_wall_variant` (DUNGEON:0x0e7b-0x0ec4), derived
 * deterministically from the dungeon number: idx = location - 0x20, with
 * {1,4,5} -> 3, {6,7} -> 2, else 1.  No RNG, no global.
 */
uint8_t dungeon_wall_variant(uint8_t dungeon_location);

/**
 * Does the cell cut the line of sight?  This is NOT the movement blocker: a
 * normal door, a room and a rooms-broke cell are all walkable yet all stop the
 * view (the compositor draws a front wall with a door).  A REVEALED secret door
 * becomes walkable but still stops the view, so the two rules invert there.
 * Reference `blocksView`; movement's own rule stays in `dungeon.cpp`.
 */
bool dungeon_view_blocks(const DungeonState &, int x, int y);

/** Side slice base by the neighbouring cell's kind (fn_1682 switch tile&0xf0). */
uint8_t dungeon_side_slice_base(uint8_t cell_type);

/** Front slice base by the blocking cell's kind (fn_150a BYTE table 0x2e80). */
uint8_t dungeon_front_slice_base(uint8_t cell_type);

/**
 * Does this feature have art to blit at all?  A trap is only drawn when the
 * tile's low three bits are clear (fn_1952 @0x197b), and a magic field has no
 * ITEMS.16 image -- it belongs to the separate sparkle subsystem.
 */
bool dungeon_feature_drawable(uint8_t cell_type, uint8_t sub);

/**
 * The deterministic draw list for the current view: near->far for the corridor
 * rings, far->near for the contents, exactly as `dng_draw_view` emits them.
 * Pure -- it reads GameState/TurnState/DungeonState and mutates nothing.
 */
DungeonViewPlan plan_dungeon_view(const GameState &, const TurnState &, const DungeonState &);

} // namespace openu5
