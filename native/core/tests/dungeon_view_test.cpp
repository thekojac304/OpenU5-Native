// Batch 9 / R-05 -- dungeon presentation parity.
//
// Every case here asserts what the ORIGINAL presents, sourced in this order:
//   [REF-BIN] DUNGEON.OVL routine + address, as recorded in the project's RE
//             notes and quoted in game/src/skin/fiel/dungeon.ts;
//   [REF-TS]  the accepted TypeScript reference implementation
//             (game/src/skin/fiel/dungeon.ts, game/src/skin/coreview.ts,
//             game/src/core/dungeon/light.ts, .../dungeon.ts);
//   [NATIVE]  native/core/src/dungeon.cpp, which already implements the same
//             rule somewhere else and therefore fixes the native convention.
//
// The suite drives openu5::plan_dungeon_view(), the semantic render output the
// device renderer consumes.  It deliberately does NOT assert pixels: the whole
// point of the seam is that WHICH slice/feature/monster is drawn, at WHICH
// depth, in WHICH order, is decidable without a framebuffer.
//
// Naming: D<n> = a presentation requirement; each block names the transition of
// the Batch 9 matrix it exercises.
#include "openu5/dungeon_view.h"
#include "openu5/hud.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "dungeon_view regression: " << what << "\n";
        ++failures;
    }
}

constexpr uint8_t kNothing = 0x00; // open corridor
constexpr uint8_t kWall = 0xb0;
constexpr uint8_t kSpecialWall = 0xc0;
constexpr uint8_t kSecretDoor = 0xd0;
constexpr uint8_t kNormalDoor = 0xe0;
constexpr uint8_t kRoom = 0xf0;
constexpr uint8_t kRoomsBroke = 0xa0;
constexpr uint8_t kLadderUp = 0x10;
constexpr uint8_t kLadderDown = 0x20;
constexpr uint8_t kChest = 0x40;

int index_of(int f, int x, int y) { return f * 64 + y * 8 + x; }

// A floor that is solid wall except where the test carves it out.
struct Floor {
    DungeonState d{};
    Floor() {
        for (auto &c : d.cells) c = kWall;
        d.active = true;
        d.pos = {33, 0, 4, 4, DungeonFacing::North};
        d.wanderer = {};
        d.wanderer.type = 255;
    }
    void set(int x, int y, uint8_t cell) { d.cells[index_of(d.pos.floor, x & 7, y & 7)] = cell; }
    void reveal(int x, int y) {
        const int n = index_of(d.pos.floor, x & 7, y & 7);
        d.revealed[n >> 3] = uint8_t(d.revealed[n >> 3] | (1u << (n & 7)));
    }
    // Carve a north-running corridor of `len` open cells starting underfoot.
    void corridor(int len) {
        for (int i = 0; i < len; ++i) set(d.pos.x, d.pos.y - i, kNothing);
    }
};

// Lit party: a torch is burning.
GameState lit_game() {
    GameState g{};
    g.torch_turns = 20;
    return g;
}

const DungeonDrawOp *find(const DungeonViewPlan &p, DungeonOpKind kind, int depth) {
    for (uint8_t i = 0; i < p.count; ++i)
        if (p.ops[i].kind == kind && p.ops[i].depth == depth) return &p.ops[i];
    return nullptr;
}
const DungeonDrawOp *find_side(const DungeonViewPlan &p, DungeonSide side, int depth) {
    for (uint8_t i = 0; i < p.count; ++i)
        if (p.ops[i].kind == DungeonOpKind::Side && p.ops[i].side == side && p.ops[i].depth == depth)
            return &p.ops[i];
    return nullptr;
}
size_t count_of(const DungeonViewPlan &p, DungeonOpKind kind) {
    size_t n = 0;
    for (uint8_t i = 0; i < p.count; ++i)
        if (p.ops[i].kind == kind) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// D1 -- LIGHT GATE.  [REF-TS] light.ts visibleDepth(): lit = lightSpellMins > 0
// or torchTurns > 0, depth 4 lit / 0 dark; [REF-TS] planDungeonView() returns an
// EMPTY op list when the view is not lit, and paintDungeon leaves the viewport
// black.  [REF-BIN] DUNGEON:0x1AD6-0x1AE4 jumps over the whole raycast.
// [NATIVE] dungeon.cpp's (S)earch already applies exactly this double gate
// before printing "darkness.", so the state is present -- only the view ignores
// it.  Transition exercised: enter a dungeon without a light source.
// ---------------------------------------------------------------------------
void d1_light_gate() {
    Floor f;
    f.corridor(4);

    GameState dark{};
    TurnState t{};
    check(dungeon_visible_depth(dark, t) == 0, "D1 unlit party has ray depth 0");
    const auto plan = plan_dungeon_view(dark, t, f.d);
    check(!plan.lit, "D1 unlit dungeon view reports lit=false");
    check(plan.count == 0, "D1 unlit dungeon view emits no draw ops (black viewport)");

    GameState torch = lit_game();
    check(dungeon_visible_depth(torch, t) == kDungeonLitDepth, "D1 torch gives ray depth 4");
    check(plan_dungeon_view(torch, t, f.d).lit, "D1 torch-lit view reports lit=true");

    // In Lor / Vas Lor light the same way: the gate does not distinguish them.
    GameState spell{};
    TurnState lit_turn{};
    lit_turn.light_spell_minutes = 5;
    check(dungeon_visible_depth(spell, lit_turn) == kDungeonLitDepth,
          "D1 a light spell alone gives ray depth 4");
    check(plan_dungeon_view(spell, lit_turn, f.d).lit, "D1 spell-lit view reports lit=true");
}

// ---------------------------------------------------------------------------
// D2 -- SIGHT IS NOT MOVEMENT.  [REF-TS] blocksView(): every kind >= 0xa stops
// the view, and a REVEALED secret door stops it too (it is drawn as a door).
// [REF-TS] isPassable(): only Wall / SpecialWall / unrevealed SecretDoor stop
// MOVEMENT -- native/core/src/dungeon.cpp implements that one correctly and must
// stay untouched.  Transition exercised: face a door / a room / a revealed
// secret door down an open corridor.
// ---------------------------------------------------------------------------
void d2_sight_blocker() {
    Floor f;
    f.corridor(4);

    // A normal door two cells ahead: walkable, but it stops the view.
    f.set(4, 2, kNormalDoor);
    check(dungeon_view_blocks(f.d, 4, 2), "D2 a normal door stops the view");

    f.set(4, 2, kRoom);
    check(dungeon_view_blocks(f.d, 4, 2), "D2 a room cell stops the view");

    f.set(4, 2, kRoomsBroke);
    check(dungeon_view_blocks(f.d, 4, 2), "D2 a rooms-broke cell stops the view");

    // Secret door: the two rules invert.  Unrevealed it looks like a wall;
    // revealed it becomes walkable yet is still drawn as a door.
    f.set(4, 2, kSecretDoor);
    check(dungeon_view_blocks(f.d, 4, 2), "D2 an unrevealed secret door stops the view");
    f.reveal(4, 2);
    check(dungeon_view_blocks(f.d, 4, 2), "D2 a REVEALED secret door still stops the view");

    f.set(4, 2, kNothing);
    check(!dungeon_view_blocks(f.d, 4, 2), "D2 open corridor does not stop the view");
    f.set(4, 2, kLadderDown);
    check(!dungeon_view_blocks(f.d, 4, 2), "D2 a ladder cell does not stop the view");
}

// ---------------------------------------------------------------------------
// D3 -- FRONT-WALL CLASSIFICATION.  [REF-TS] frontBase(): Wall / SecretDoor ->
// 8 (plain dead end), SpecialWall -> 0x18, everything else (RoomsBroke /
// NormalDoor / Room) -> 12, a dead end WITH A DOOR.  A revealed secret door is
// reclassified as a normal door before the lookup.  [REF-BIN] fn_150a @0x150a,
// BYTE table 0x2e80[kind].  pieceCode = base + depth.
// Transition exercised: walk up to a wall / a door / a special wall.
// ---------------------------------------------------------------------------
void d3_front_classification() {
    const GameState g = lit_game();
    TurnState t{};

    struct Case {
        uint8_t cell;
        uint8_t base;
        const char *what;
    };
    const Case cases[] = {
        {kWall, 8, "D3 a plain wall dead end uses front base 8"},
        {kSecretDoor, 8, "D3 an unrevealed secret door uses front base 8"},
        {kSpecialWall, 0x18, "D3 a special wall uses front base 0x18"},
        {kNormalDoor, 12, "D3 a normal door uses front base 12 (door dead end)"},
        {kRoom, 12, "D3 a room entrance uses front base 12 (door dead end)"},
        {kRoomsBroke, 12, "D3 a rooms-broke cell uses front base 12 (door dead end)"},
    };
    for (const auto &c : cases) {
        Floor f;
        f.corridor(4);
        f.set(4, 2, c.cell); // two cells ahead, facing north
        const auto plan = plan_dungeon_view(g, t, f.d);
        const auto *front = find(plan, DungeonOpKind::Front, 2);
        check(front != nullptr, c.what);
        if (front) check(front->slice == uint8_t(c.base + 2), c.what);
    }

    // A REVEALED secret door is drawn as a door, not as the plain dead end its
    // unrevealed self was.
    Floor f;
    f.corridor(4);
    f.set(4, 2, kSecretDoor);
    f.reveal(4, 2);
    const auto plan = plan_dungeon_view(g, t, f.d);
    const auto *front = find(plan, DungeonOpKind::Front, 2);
    check(front && front->slice == uint8_t(12 + 2),
          "D3 a REVEALED secret door is drawn as a door front (base 12)");
}

// ---------------------------------------------------------------------------
// D4 -- SIDE-WALL CLASSIFICATION.  [REF-TS] sideBase(): kind < 0xa -> 0x10, an
// OPEN side passage; SpecialWall -> 0x14, an alcove; RoomsBroke / NormalDoor /
// Room -> 4, a side door; otherwise 0, plain side wall.  [REF-BIN] fn_1682
// @0x1682, switch on tile & 0xf0.  Both sides are always emitted, the right one
// mirrored, at the fixed X of table 0x2e62.  Transition exercised: stand in a
// junction with a side opening / a side door / an alcove.
// ---------------------------------------------------------------------------
void d4_side_classification() {
    const GameState g = lit_game();
    TurnState t{};

    struct Case {
        uint8_t cell;
        uint8_t base;
        const char *what;
    };
    const Case cases[] = {
        {kNothing, 0x10, "D4 an open side cell is a side PASSAGE (base 0x10)"},
        {kLadderDown, 0x10, "D4 a ladder side cell is a side PASSAGE (base 0x10)"},
        {kSpecialWall, 0x14, "D4 a special wall beside you is an ALCOVE (base 0x14)"},
        {kNormalDoor, 4, "D4 a door beside you is a SIDE DOOR (base 4)"},
        {kRoom, 4, "D4 a room beside you is a SIDE DOOR (base 4)"},
        {kRoomsBroke, 4, "D4 a rooms-broke cell beside you is a SIDE DOOR (base 4)"},
        {kWall, 0, "D4 a wall beside you is a plain SIDE WALL (base 0)"},
        {kSecretDoor, 0, "D4 an unrevealed secret door beside you is a plain SIDE WALL"},
    };
    for (const auto &c : cases) {
        Floor f;
        f.corridor(4);
        f.set(3, 4, c.cell); // west of the party = its LEFT when facing north
        const auto plan = plan_dungeon_view(g, t, f.d);
        const auto *left = find_side(plan, DungeonSide::Left, 0);
        check(left != nullptr, c.what);
        if (left) check(left->slice == c.base, c.what);
    }

    // Both sides are always present, at the table's fixed X, right one mirrored.
    Floor f;
    f.corridor(4);
    const auto plan = plan_dungeon_view(g, t, f.d);
    for (int depth = 0; depth <= kDungeonMaxDepth; ++depth) {
        const auto *l = find_side(plan, DungeonSide::Left, depth);
        const auto *r = find_side(plan, DungeonSide::Right, depth);
        check(l && r, "D4 every lit ring emits BOTH side slices");
        if (l)
            check(l->x == kDungeonSideXLeft[depth] && !l->mirror,
                  "D4 the left slice sits at its table X, unmirrored");
        if (r)
            check(r->x == kDungeonSideXRight[depth] && r->mirror,
                  "D4 the right slice sits at its table X, mirrored");
    }
}

// ---------------------------------------------------------------------------
// D5 -- STANDING ON A DOOR.  [REF-TS] planDungeonView(): standingOnDoor (depth 0
// and the cell underfoot is a NormalDoor) suppresses BOTH side blits of ring 0 --
// the doorframe fills them.  [REF-BIN] driver @0x1b1e.  Transition exercised: an
// action that mutates the cell you stand on (walking onto a door).
// ---------------------------------------------------------------------------
void d5_standing_on_door() {
    const GameState g = lit_game();
    TurnState t{};
    Floor f;
    f.corridor(4);
    f.set(4, 4, kNormalDoor); // underfoot

    const auto plan = plan_dungeon_view(g, t, f.d);
    check(find_side(plan, DungeonSide::Left, 0) == nullptr &&
              find_side(plan, DungeonSide::Right, 0) == nullptr,
          "D5 standing on a door suppresses ring 0's side slices");
    check(find_side(plan, DungeonSide::Left, 1) != nullptr,
          "D5 standing on a door leaves the deeper rings alone");
}

// ---------------------------------------------------------------------------
// D6 -- FEATURES AT EVERY DEPTH, PAINTED FAR->NEAR.  [REF-TS] planDungeonView()
// collects a FeatureOp for every cone cell whose kind is LadderUp(1)..
// MagicField(8), depth 0..3 INCLUSIVE, then appends them in reverse so the
// nearest is painted last.  [REF-BIN] driver loop @0x1b98 calls fn_1952 from far
// down to depth 0.  Transition exercised: an object appears ahead of you, and an
// action that mutates an adjacent visible cell.
// ---------------------------------------------------------------------------
void d6_feature_depths() {
    const GameState g = lit_game();
    TurnState t{};
    Floor f;
    f.corridor(4);
    f.set(4, 4, kLadderDown); // underfoot, depth 0
    f.set(4, 3, kChest);      // depth 1
    f.set(4, 2, kLadderUp);   // depth 2

    const auto plan = plan_dungeon_view(g, t, f.d);
    const auto *d0 = find(plan, DungeonOpKind::Feature, 0);
    const auto *d1 = find(plan, DungeonOpKind::Feature, 1);
    const auto *d2 = find(plan, DungeonOpKind::Feature, 2);
    check(d0 && d0->cell_type == 0x2, "D6 the ladder underfoot is drawn (depth 0)");
    check(d1 && d1->cell_type == 0x4, "D6 a chest one cell ahead is drawn (depth 1)");
    check(d2 && d2->cell_type == 0x1, "D6 a ladder two cells ahead is drawn (depth 2)");
    check(count_of(plan, DungeonOpKind::Feature) == 3, "D6 all three features are drawn");

    // Painter order: far first, near last, and all of them after the corridor.
    int first_feature = -1, last_corridor = -1, prev_depth = 99;
    bool descending = true;
    for (uint8_t i = 0; i < plan.count; ++i) {
        const auto k = plan.ops[i].kind;
        if (k == DungeonOpKind::Side || k == DungeonOpKind::Front) last_corridor = int(i);
        if (k == DungeonOpKind::Feature) {
            if (first_feature < 0) first_feature = int(i);
            if (int(plan.ops[i].depth) > prev_depth) descending = false;
            prev_depth = int(plan.ops[i].depth);
        }
    }
    check(first_feature > last_corridor, "D6 features paint after the corridor");
    check(descending, "D6 features paint far->near");
}

// ---------------------------------------------------------------------------
// D7 -- TRAP GATE / MAGIC FIELD.  [REF-TS] featureBlits(): a Trap is only
// blitted when (sub & 7) == 0; a MagicField has no ITEMS.16 image at all -- it
// belongs to the separate sparkle subsystem.  [REF-BIN] fn_1952 @0x197b.
// ---------------------------------------------------------------------------
void d7_feature_gates() {
    check(dungeon_feature_drawable(0x6, 0x0), "D7 a trap with its low bits clear is drawn");
    check(!dungeon_feature_drawable(0x6, 0x3), "D7 a trap with its low bits set is NOT drawn");
    check(!dungeon_feature_drawable(0x8, 0x0), "D7 a magic field has no ITEMS.16 art");
    check(dungeon_feature_drawable(0x4, 0x0), "D7 a chest is drawn");
    check(!dungeon_feature_drawable(0x0, 0x0), "D7 an empty cell has no feature");
}

// ---------------------------------------------------------------------------
// D8 -- THE WANDERER.  [REF-TS] planDungeonView(): the monster is tested at
// depth 1..maxDepth with the cell coordinate wrapped &7, and
// coreview.dungeonView() publishes it whenever its type is live and it is on
// THIS floor.  `hidden` maps to `ceiling` -- api.ts states in terms that the
// ceiling row is a spider or slime lurking above, "NO es invisibilidad".
// [REF-BIN] tables 0x2E2A / 0x2E32.  Transition exercised: an entity appears at
// range; a combat-adjacent update.
// ---------------------------------------------------------------------------
void d8_wanderer() {
    const GameState g = lit_game();
    TurnState t{};

    // Two cells ahead: the original draws it.
    {
        Floor f;
        f.corridor(4);
        f.d.wanderer.type = 3;
        f.d.wanderer.bank = 5;
        f.d.wanderer.floor = 0;
        f.d.wanderer.x = 4;
        f.d.wanderer.y = 2;
        const auto plan = plan_dungeon_view(g, t, f.d);
        const auto *m = find(plan, DungeonOpKind::Monster, 2);
        check(m != nullptr, "D8 a wanderer two cells ahead is drawn");
        if (m) check(m->bank == 5, "D8 the wanderer carries its own sprite bank");
    }
    // Three cells ahead is still inside the lit cone.
    {
        Floor f;
        f.corridor(4);
        f.d.wanderer.type = 3;
        f.d.wanderer.floor = 0;
        f.d.wanderer.x = 4;
        f.d.wanderer.y = 1;
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Monster, 3) != nullptr,
              "D8 a wanderer three cells ahead is drawn");
    }
    // A `hidden` wanderer is drawn ON THE CEILING, not suppressed.
    {
        Floor f;
        f.corridor(4);
        f.d.wanderer.type = 3;
        f.d.wanderer.floor = 0;
        f.d.wanderer.x = 4;
        f.d.wanderer.y = 3;
        f.d.wanderer.hidden = true;
        const auto plan = plan_dungeon_view(g, t, f.d);
        const auto *m = find(plan, DungeonOpKind::Monster, 1);
        check(m != nullptr, "D8 a hidden wanderer is still drawn");
        if (m) check(m->ceiling, "D8 a hidden wanderer is drawn on the ceiling row");
    }
    // Another floor is never drawn.
    {
        Floor f;
        f.corridor(4);
        f.d.wanderer.type = 3;
        f.d.wanderer.floor = 4;
        f.d.wanderer.x = 4;
        f.d.wanderer.y = 3;
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Monster, 1) == nullptr,
              "D8 a wanderer on another floor is not drawn");
    }
}

// ---------------------------------------------------------------------------
// D9 -- WALL VARIANT REACHES THE VIEW.  [REF-TS] wallVariant(): idx = location
// - 0x20, with {1,4,5} -> 3, {6,7} -> 2, else 1; coreview publishes it on every
// DungeonViewInfo.  [NATIVE] dungeon.cpp's (S)earch already derives the same
// number for its stalactite / caved-in / skeleton messages, so the two must
// agree by construction.  Transition exercised: enter each dungeon.
// ---------------------------------------------------------------------------
void d9_wall_variant() {
    check(dungeon_wall_variant(33) == 3, "D9 Deceit is wall variant 3");
    check(dungeon_wall_variant(34) == 1, "D9 Despise is wall variant 1");
    check(dungeon_wall_variant(35) == 1, "D9 Destard is wall variant 1");
    check(dungeon_wall_variant(36) == 3, "D9 Wrong is wall variant 3");
    check(dungeon_wall_variant(37) == 3, "D9 Covetous is wall variant 3");
    check(dungeon_wall_variant(38) == 2, "D9 Shame is wall variant 2");
    check(dungeon_wall_variant(39) == 2, "D9 Hythloth is wall variant 2");
    check(dungeon_wall_variant(40) == 1, "D9 Doom is wall variant 1");

    const GameState g = lit_game();
    TurnState t{};
    for (uint8_t loc = 33; loc <= 40; ++loc) {
        Floor f;
        f.d.pos.dungeon = loc;
        f.corridor(4);
        check(plan_dungeon_view(g, t, f.d).wall_variant == dungeon_wall_variant(loc),
              "D9 the plan carries the dungeon's own wall variant");
    }
}

// ---------------------------------------------------------------------------
// D10 -- TURNING AND MOVING REBUILD THE VIEW.  The plan is a pure function of
// the live DungeonState, so a turn in place must change what is drawn whenever
// the two directions differ, and stepping forward must too.  Transitions
// exercised: turn left / turn right, move forward, move back.
// ---------------------------------------------------------------------------
void d10_orientation() {
    const GameState g = lit_game();
    TurnState t{};
    Floor f;
    // A T junction: three open cells north (so the dead end at y-3 sits at the
    // far edge of the lit cone), plus an opening to the east only.
    f.corridor(3);
    f.set(5, 4, kNothing);

    f.d.pos.facing = DungeonFacing::North;
    const auto north = plan_dungeon_view(g, t, f.d);
    f.d.pos.facing = DungeonFacing::East;
    const auto east = plan_dungeon_view(g, t, f.d);

    check(north.facing == DungeonFacing::North && east.facing == DungeonFacing::East,
          "D10 the plan reports the live facing");
    const auto *n_right = find_side(north, DungeonSide::Right, 0);
    const auto *e_left = find_side(east, DungeonSide::Left, 0);
    check(n_right && n_right->slice == 0x10,
          "D10 facing north, the east opening is the RIGHT side passage");
    check(e_left && e_left->slice == 0x10,
          "D10 facing east, the north corridor is the LEFT side passage");

    // Moving forward changes the depth at which the dead end appears.
    f.d.pos.facing = DungeonFacing::North;
    const auto before = plan_dungeon_view(g, t, f.d);
    f.d.pos.y = uint8_t(f.d.pos.y - 1);
    const auto after = plan_dungeon_view(g, t, f.d);
    check(find(before, DungeonOpKind::Front, 3) != nullptr,
          "D10 before the step the dead end is three cells away");
    check(find(after, DungeonOpKind::Front, 2) != nullptr,
          "D10 after the step the dead end is two cells away");
}

// ---------------------------------------------------------------------------
// D11 -- THE LIT CONE ENDS AT DEPTH 3.  [REF-TS] planDungeonView(): maxDepth =
// min(lightDepth, 3), so the ray marches depth 0..3 and no ring 4 is ever
// emitted.  An open corridor longer than the cone runs out into the vanishing
// point rather than drawing a phantom wall.
// ---------------------------------------------------------------------------
void d11_cone_depth() {
    const GameState g = lit_game();
    TurnState t{};
    Floor f;
    f.corridor(8); // wraps the 8-cell torus: open all the way round

    const auto plan = plan_dungeon_view(g, t, f.d);
    for (uint8_t i = 0; i < plan.count; ++i)
        check(plan.ops[i].depth <= kDungeonMaxDepth, "D11 no op is emitted past depth 3");
    check(count_of(plan, DungeonOpKind::Side) == 8,
          "D11 an unobstructed corridor emits exactly four rings of side pairs");
    check(count_of(plan, DungeonOpKind::Front) == 0,
          "D11 an unobstructed corridor draws no front wall");
}

// ---------------------------------------------------------------------------
// D12 -- THE DUNGEON BANDS.  [REF-TS] skin.ts paints, while a dungeon view is
// mounted, a top band `dungeonLevelLabel(floor)` = "L1".."L8" and a bottom band
// `dungeonDirLabel(DUNGEON_DIR_NAMES[facing])` = "Dir:" + the name right-
// justified in a field of 7.  [REF-BIN] dng_draw_panel @DUNGEON:0x01D2.
// Before Batch 9 the device showed a blank sky strip and "Wind: --" here, so a
// turn in place in a symmetric corridor produced NO on-screen change at all.
// Transitions exercised: turn left/right, climb up/down, enter/leave a dungeon.
// ---------------------------------------------------------------------------
void d12_bands() {
    Floor f;
    f.corridor(4);

    check(!hud_dungeon_bands(f.d, false).active,
          "D12 the bands are inactive outside a dungeon view");
    DungeonState closed{};
    check(!hud_dungeon_bands(closed, true).active,
          "D12 the bands are inactive when no session is loaded");

    // Level tracks the floor, which is what a Klimb up/down changes.
    for (uint8_t floor = 0; floor < 8; ++floor) {
        f.d.pos.floor = floor;
        const auto b = hud_dungeon_bands(f.d, true);
        const char expect[3] = {'L', char('1' + floor), '\0'};
        check(b.active && std::strcmp(b.level, expect) == 0,
              "D12 the top band is the dungeon level L1..L8");
    }
    f.d.pos.floor = 0;

    struct Case {
        DungeonFacing facing;
        const char *text;
    };
    const Case cases[] = {
        {DungeonFacing::North, "Dir:  North"},
        {DungeonFacing::East, "Dir:   East"},
        {DungeonFacing::South, "Dir:  South"},
        {DungeonFacing::West, "Dir:   West"},
    };
    for (const auto &c : cases) {
        f.d.pos.facing = c.facing;
        const auto b = hud_dungeon_bands(f.d, true);
        check(b.active && std::strcmp(b.direction, c.text) == 0,
              "D12 the bottom band is Dir: + the facing, right-justified in 7");
    }

    // The band is the only immediate feedback a turn gives in a corridor whose
    // two ends look identical, so it must differ for every pair of facings.
    f.d.pos.facing = DungeonFacing::North;
    const auto north = hud_dungeon_bands(f.d, true);
    f.d.pos.facing = DungeonFacing::South;
    const auto south = hud_dungeon_bands(f.d, true);
    check(std::strcmp(north.direction, south.direction) != 0,
          "D12 turning around changes the direction band");
}

// ---------------------------------------------------------------------------
// D13 -- MUTATION -> VISIBLE STATE, IN ONE STEP.  Everything above drives the
// plan from a hand-built DungeonState.  This block instead drives the REAL core
// mutator, openu5::dungeon_action(), and asserts the plan already reflects the
// change on the very call that made it -- no second input, no cached frame.
// Transitions: turn left/right, move forward, climb down (level transition),
// an action that mutates an ADJACENT visible cell (Search reveals a secret
// door), and an action that mutates the CURRENT cell (a bomb trap clears).
// ---------------------------------------------------------------------------
void d13_mutation_is_immediately_visible() {
    const auto run = [](GameState &g, TurnState &t, DungeonState &d, DungeonAction action,
                        int dir) { dungeon_action(g, t, d, action, DungeonSink{}, dir); };

    // Turn: the facing, the side classification and the band all move together.
    {
        GameState g = lit_game();
        TurnState t{};
        Floor f;
        f.corridor(3);
        f.set(5, 4, kNothing); // an opening to the east only
        const auto before = plan_dungeon_view(g, t, f.d);
        const auto band_before = hud_dungeon_bands(f.d, true);
        run(g, t, f.d, DungeonAction::Right, 0);
        const auto after = plan_dungeon_view(g, t, f.d);
        const auto band_after = hud_dungeon_bands(f.d, true);
        check(after.facing == DungeonFacing::East, "D13 (T)urn right updates the plan facing");
        const auto *l = find_side(after, DungeonSide::Left, 0);
        check(l && l->slice == 0x10,
              "D13 after turning east the north corridor is immediately the LEFT passage");
        check(std::strcmp(band_before.direction, band_after.direction) != 0,
              "D13 the direction band changes on the turn itself");
        check(before.facing != after.facing, "D13 the pre-turn plan is not reused");
    }

    // Forward: the dead end closes in by one ring on the step itself.
    {
        GameState g = lit_game();
        TurnState t{};
        Floor f;
        f.corridor(3);
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Front, 3) != nullptr,
              "D13 the dead end starts three rings out");
        run(g, t, f.d, DungeonAction::Forward, 0);
        check(f.d.pos.y == 3, "D13 Advance moved the party");
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Front, 2) != nullptr,
              "D13 the dead end is two rings out on the step itself");
    }

    // Level transition: Klimb down changes the floor, the plan and the band.
    {
        GameState g = lit_game();
        TurnState t{};
        Floor f;
        f.corridor(3);
        f.set(4, 4, kLadderDown);                  // stand on a down ladder
        f.d.cells[index_of(1, 4, 4)] = kLadderUp;  // the cell it lands on
        f.d.cells[index_of(1, 4, 3)] = kNothing;
        check(hud_dungeon_bands(f.d, true).level[1] == '1', "D13 the band starts at level 1");
        run(g, t, f.d, DungeonAction::Klimb, 1);
        check(f.d.pos.floor == 1, "D13 Klimb down changed the floor");
        const auto after = plan_dungeon_view(g, t, f.d);
        check(after.floor == 1, "D13 the plan is on the new floor immediately");
        check(hud_dungeon_bands(f.d, true).level[1] == '2',
              "D13 the level band reads L2 on the descent itself");
        const auto *feat = find(after, DungeonOpKind::Feature, 0);
        check(feat && feat->cell_type == uint8_t(DungeonCellKind::LadderUp),
              "D13 the ladder arrived on is drawn without a further input");
    }

    // An ADJACENT visible cell mutates: (S)earch reveals a secret door ahead,
    // and the front wall must stop being a blank dead end at once.
    {
        GameState g = lit_game();
        TurnState t{};
        Floor f;
        f.corridor(2);
        f.set(4, 3, kSecretDoor); // one cell ahead
        const auto *before = find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Front, 1);
        check(before && before->slice == uint8_t(8 + 1),
              "D13 an unfound secret door is a plain dead end");
        run(g, t, f.d, DungeonAction::Search, 0);
        const auto *after = find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Front, 1);
        check(after && after->slice == uint8_t(12 + 1),
              "D13 finding the secret door turns the dead end into a door at once");
    }

    // The CURRENT cell mutates: stepping onto a bomb trap clears the cell, so
    // the feature drawn underfoot must disappear on that same step.
    {
        GameState g = lit_game();
        TurnState t{};
        g.party.party_size = 1;
        g.party.characters[0].status = 'G';
        g.party.characters[0].current_hp = 40;
        g.party.characters[0].max_hp = 40;
        g.party.characters[0].dexterity = 20;
        Floor f;
        f.corridor(3);
        f.set(4, 3, uint8_t(0x60 | 2)); // bomb trap one cell ahead
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Feature, 1) != nullptr,
              "D13 the trap ahead is drawn before it is sprung");
        run(g, t, f.d, DungeonAction::Forward, 0);
        check(f.d.pos.y == 3, "D13 the party stepped onto the trap");
        check(find(plan_dungeon_view(g, t, f.d), DungeonOpKind::Feature, 0) == nullptr,
              "D13 the sprung trap stops being drawn on the step itself");
    }
}

} // namespace

int main() {
    d1_light_gate();
    d2_sight_blocker();
    d3_front_classification();
    d4_side_classification();
    d5_standing_on_door();
    d6_feature_depths();
    d7_feature_gates();
    d8_wanderer();
    d9_wall_variant();
    d10_orientation();
    d11_cone_depth();
    d12_bands();
    d13_mutation_is_immediately_visible();
    if (failures) {
        std::cerr << "dungeon_view regression: " << failures << " failing assertion(s)\n";
        return 1;
    }
    std::cout << "dungeon_view regression: all assertions pass\n";
    return 0;
}
