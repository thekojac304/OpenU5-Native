#include "openu5/dungeon_view.h"

namespace openu5 {
namespace {

// Dungeon facing order is N/E/S/W, the same order dungeon.cpp's mover uses.
constexpr int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};

/** The floor is a torus: the ray marches unwrapped, the map is read wrapped. */
int wrap8(int v) { return ((v % 8) + 8) % 8; }

uint8_t cell_byte(const DungeonState &d, int x, int y) {
    return dungeon_cell(d, d.pos.floor, wrap8(x), wrap8(y));
}
uint8_t cell_kind(const DungeonState &d, int x, int y) { return uint8_t(cell_byte(d, x, y) >> 4); }

bool secret_revealed(const DungeonState &d, int x, int y) {
    const int n = int(d.pos.floor) * 64 + wrap8(y) * 8 + wrap8(x);
    return (d.revealed[n >> 3] & (1U << (n & 7))) != 0;
}

/** One pending contents op, held back so the painter can run far->near. */
struct Pending {
    DungeonDrawOp op{};
};

} // namespace

int dungeon_visible_depth(const GameState &g, const TurnState &t) {
    // DUNGEON:0x1AD6-0x1AE4 -- `if g_light_spell_mins == 0 AND g_torch_mins == 0
    // -> jmp past the raycast`.  The gate does not distinguish a torch from In
    // Lor / Vas Lor, so either source yields the same depth.  This is the same
    // double gate dungeon.cpp's (S)earch applies before printing "darkness.".
    const bool lit = g.torch_turns > 0 || t.light_spell_minutes > 0;
    return lit ? kDungeonLitDepth : 0;
}

uint8_t dungeon_wall_variant(uint8_t loc) {
    // g_dng_wall_variant, DUNGEON:0x0e7b-0x0ec4.  Deterministic, no RNG.
    const int idx = int(loc) - 0x20;
    if (idx == 1 || idx == 4 || idx == 5) return 3; // Deceit, Wrong, Covetous
    if (idx == 6 || idx == 7) return 2;             // Shame, Hythloth
    return 1;                                       // Despise, Destard, Doom
}

bool dungeon_view_blocks(const DungeonState &d, int x, int y) {
    // Reference `blocksView`.  Deliberately NOT the movement blocker: a normal
    // door, a room and a rooms-broke cell are walkable yet all stop the view,
    // and a REVEALED secret door becomes walkable while still stopping it.
    const uint8_t kind = cell_kind(d, x, y);
    if (kind == uint8_t(DungeonCellKind::SecretDoor) && secret_revealed(d, x, y)) return true;
    return kind >= uint8_t(DungeonCellKind::RoomsBroke);
}

uint8_t dungeon_side_slice_base(uint8_t kind) {
    // fn_1682 @0x1682, switch on tile & 0xf0.  The SpecialWall test comes before
    // the door group: an alcove is not a door.
    if (kind < uint8_t(DungeonCellKind::RoomsBroke)) return 0x10; // open side passage
    if (kind == uint8_t(DungeonCellKind::SpecialWall)) return 0x14; // alcove
    if (kind == uint8_t(DungeonCellKind::RoomsBroke) ||
        kind == uint8_t(DungeonCellKind::NormalDoor) || kind == uint8_t(DungeonCellKind::Room))
        return 4;  // side door
    return 0;      // plain side wall: Wall / SecretDoor
}

uint8_t dungeon_front_slice_base(uint8_t kind) {
    // fn_150a @0x150a, BYTE table 0x2e80[kind].
    if (kind == uint8_t(DungeonCellKind::Wall) || kind == uint8_t(DungeonCellKind::SecretDoor))
        return 8;  // plain dead end
    if (kind == uint8_t(DungeonCellKind::SpecialWall)) return 0x18;
    return 12;     // rooms-broke / door / room -> a dead end WITH a door
}

bool dungeon_feature_drawable(uint8_t cell_type, uint8_t sub) {
    // featureBlits(): a trap is only blitted with its low three bits clear
    // (fn_1952 @0x197b), and a magic field has no ITEMS.16 image at all -- it
    // is drawn by the separate sparkle subsystem.
    if (cell_type == uint8_t(DungeonCellKind::Trap)) return (sub & 7) == 0;
    return cell_type >= uint8_t(DungeonCellKind::LadderUp) &&
           cell_type <= uint8_t(DungeonCellKind::OpenChest);
}

DungeonViewPlan plan_dungeon_view(const GameState &g, const TurnState &t, const DungeonState &d) {
    DungeonViewPlan plan{};
    plan.floor = d.pos.floor;
    plan.facing = d.pos.facing;
    plan.wall_variant = dungeon_wall_variant(d.pos.dungeon);
    plan.light_depth = uint8_t(dungeon_visible_depth(g, t));
    plan.lit = plan.light_depth > 0;
    // Hard gate: with no light the original jumps over the entire raycast and
    // the viewport stays black.  An inactive session has nothing to draw either.
    if (!plan.lit || !d.active) {
        plan.lit = plan.lit && d.active;
        return plan;
    }

    const auto push = [&plan](const DungeonDrawOp &op) {
        if (plan.count < kDungeonMaxOps) plan.ops[plan.count++] = op;
    };

    const int facing = int(d.pos.facing) & 3;
    const int left_dir = (facing + 3) & 3; // LEFT_OF[facing]
    const int max_depth =
        plan.light_depth - 1 < kDungeonMaxDepth ? plan.light_depth - 1 : kDungeonMaxDepth;

    // Contents (features and monster sightings) are collected near->far and
    // replayed far->near, exactly as the driver's painter does.
    Pending contents[kDungeonMaxOps];
    size_t contents_count = 0;
    const auto defer = [&contents, &contents_count](const DungeonDrawOp &op) {
        if (contents_count < kDungeonMaxOps) contents[contents_count++].op = op;
    };

    const bool monster_live = d.wanderer.type != 255 && d.wanderer.floor == d.pos.floor;

    for (int si = 0; si <= max_depth; ++si) {
        const int cx = int(d.pos.x) + dx[facing] * si;
        const int cy = int(d.pos.y) + dy[facing] * si;
        const uint8_t kind = cell_kind(d, cx, cy);

        if (si > 0 && dungeon_view_blocks(d, cx, cy)) {
            // A revealed secret door is drawn as an ordinary door, not as the
            // plain dead end its unrevealed self was.
            const uint8_t front_kind =
                (kind == uint8_t(DungeonCellKind::SecretDoor) && secret_revealed(d, cx, cy))
                    ? uint8_t(DungeonCellKind::NormalDoor)
                    : kind;
            DungeonDrawOp op{};
            op.kind = DungeonOpKind::Front;
            op.depth = uint8_t(si);
            op.slice = uint8_t(dungeon_front_slice_base(front_kind) + si);
            op.cell_type = front_kind;
            op.sub = uint8_t(cell_byte(d, cx, cy) & 15);
            push(op);
            break;
        }

        // Driver @0x1b1e: standing ON a door, the doorframe fills ring 0 and the
        // two side slices are not emitted.
        const bool standing_on_door = si == 0 && kind == uint8_t(DungeonCellKind::NormalDoor);
        if (!standing_on_door) {
            const int lnx = cx + dx[left_dir], lny = cy + dy[left_dir];
            const int rnx = cx - dx[left_dir], rny = cy - dy[left_dir];
            DungeonDrawOp l{};
            l.kind = DungeonOpKind::Side;
            l.depth = uint8_t(si);
            l.side = DungeonSide::Left;
            l.slice = uint8_t(dungeon_side_slice_base(cell_kind(d, lnx, lny)) + si);
            l.x = kDungeonSideXLeft[si];
            l.mirror = false;
            l.cell_type = cell_kind(d, lnx, lny);
            push(l);
            DungeonDrawOp r{};
            r.kind = DungeonOpKind::Side;
            r.depth = uint8_t(si);
            r.side = DungeonSide::Right;
            r.slice = uint8_t(dungeon_side_slice_base(cell_kind(d, rnx, rny)) + si);
            r.x = kDungeonSideXRight[si];
            r.mirror = true;
            r.cell_type = cell_kind(d, rnx, rny);
            push(r);
        }

        // Features are collected at EVERY depth including 0: standing on the
        // stairs still draws the stairs (driver loop @0x1b98 runs down to si=0).
        if (kind >= uint8_t(DungeonCellKind::LadderUp) &&
            kind <= uint8_t(DungeonCellKind::MagicField)) {
            DungeonDrawOp op{};
            op.kind = DungeonOpKind::Feature;
            op.depth = uint8_t(si);
            op.cell_type = kind;
            op.sub = uint8_t(cell_byte(d, cx, cy) & 15);
            defer(op);
        }

        // The wanderer is compared with the map wrap, like the ray itself, at
        // every depth from 1 outward -- not only at arm's length.
        if (si > 0 && monster_live && wrap8(cx) == int(d.wanderer.x) &&
            wrap8(cy) == int(d.wanderer.y)) {
            DungeonDrawOp op{};
            op.kind = DungeonOpKind::Monster;
            op.depth = uint8_t(si);
            op.bank = d.wanderer.bank;
            // `hidden` is the +15 attribute flag: a spider or slime lurking on
            // the CEILING row of table 0x2E32.  It is not invisibility.
            op.ceiling = d.wanderer.hidden;
            defer(op);
        }
    }

    for (size_t i = contents_count; i-- > 0;) push(contents[i].op);
    return plan;
}

} // namespace openu5
