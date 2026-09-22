// Batch 21A.2 probe -- dump the REAL DungeonViewPlan for the authored Deceit
// floor-7 door cluster, with each op's pixel extent from the authored art dims.
//
// Not a regression: a throwaway inspector. It links the production planner
// (plan_dungeon_view) and the production art catalog (dungeon_art_authored_
// catalog) and prints, in painter order, what the device would blit.
#include "openu5/dungeon_view.h"
#include "openu5/dungeon_art.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace openu5;

namespace {

const char *kind_name(uint8_t k) {
    switch (DungeonCellKind(k)) {
    case DungeonCellKind::Nothing: return "Nothing";
    case DungeonCellKind::LadderUp: return "LadderUp";
    case DungeonCellKind::LadderDown: return "LadderDown";
    case DungeonCellKind::LadderUpDown: return "LadderUpDown";
    case DungeonCellKind::Chest: return "Chest";
    case DungeonCellKind::Fountain: return "Fountain";
    case DungeonCellKind::Trap: return "Trap";
    case DungeonCellKind::OpenChest: return "OpenChest";
    case DungeonCellKind::MagicField: return "MagicField";
    case DungeonCellKind::RoomsBroke: return "RoomsBroke";
    case DungeonCellKind::Wall: return "Wall";
    case DungeonCellKind::SpecialWall: return "SpecialWall";
    case DungeonCellKind::SecretDoor: return "SecretDoor";
    case DungeonCellKind::NormalDoor: return "NormalDoor";
    case DungeonCellKind::Room: return "Room";
    }
    return "?";
}

const char *slice_family(const DungeonDrawOp &op) {
    if (op.kind == DungeonOpKind::Front) {
        const int base = op.slice - op.depth;
        return base == 8 ? "FRONT plain dead end" : base == 12 ? "FRONT door"
               : base == 0x18                                  ? "FRONT special wall"
                                                               : "FRONT ?";
    }
    if (op.kind == DungeonOpKind::Side) {
        const int base = op.slice - op.depth;
        return base == 0      ? "side plain wall"
               : base == 4    ? "SIDE DOOR"
               : base == 0x10 ? "side open passage"
               : base == 0x14 ? "side alcove"
                              : "side ?";
    }
    return op.kind == DungeonOpKind::Feature ? "feature" : "monster";
}

int wrap8(int v) { return ((v % 8) + 8) % 8; }

constexpr int kFwdX[4] = {0, 1, 0, -1}, kFwdY[4] = {-1, 0, 1, 0};
constexpr int kLeftOf[4] = {3, 0, 1, 2};
const char *kFacingName[4] = {"North", "East", "South", "West"};

void dump(const DungeonState &d, const GameState &g, const TurnState &t, const char *label) {
    const auto catalog = dungeon_art_authored_catalog();
    const auto plan = plan_dungeon_view(g, t, d);
    const int f = int(d.pos.facing) & 3;
    const int ld = kLeftOf[f];

    std::printf("\n==================================================================\n");
    std::printf("%s\n", label);
    std::printf("  party (%u,%u) floor %u facing %s   underfoot 0x%02X %s\n", unsigned(d.pos.x),
                unsigned(d.pos.y), unsigned(d.pos.floor), kFacingName[f],
                dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y),
                kind_name(dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y) >> 4));
    for (int si = 0; si <= 2; ++si) {
        const int cx = wrap8(int(d.pos.x) + kFwdX[f] * si), cy = wrap8(int(d.pos.y) + kFwdY[f] * si);
        const int lx = wrap8(cx + kFwdX[ld]), ly = wrap8(cy + kFwdY[ld]);
        const int rx = wrap8(cx - kFwdX[ld]), ry = wrap8(cy - kFwdY[ld]);
        std::printf("  ring %d cell (%d,%d)=%-11s | L (%d,%d)=%-11s | R (%d,%d)=%s\n", si, cx, cy,
                    kind_name(dungeon_cell(d, d.pos.floor, cx, cy) >> 4), lx, ly,
                    kind_name(dungeon_cell(d, d.pos.floor, lx, ly) >> 4), rx, ry,
                    kind_name(dungeon_cell(d, d.pos.floor, rx, ry) >> 4));
    }
    std::printf("  --- plan: %u ops, lit=%d, wall_variant=%u (DNG%u) ---\n", unsigned(plan.count),
                plan.lit, unsigned(plan.wall_variant), unsigned(plan.wall_variant));
    std::printf("  %-3s %-5s %-6s %-21s %-5s %-6s %-6s %s\n", "#", "depth", "side", "family",
                "image", "destX", "width", "covers");
    for (uint8_t i = 0; i < plan.count; ++i) {
        const auto &op = plan.ops[i];
        if (op.kind == DungeonOpKind::Side) {
            const auto dims = catalog.wall[op.slice];
            std::printf("  %-3u %-5u %-6s %-21s %-5u %-6d %-6u %d..%d%s\n", unsigned(i),
                        unsigned(op.depth), op.side == DungeonSide::Left ? "left" : "right",
                        slice_family(op), unsigned(op.slice), int(op.x), unsigned(dims.w),
                        int(op.x), int(op.x) + int(dims.w), op.mirror ? "  (mirrored)" : "");
        } else if (op.kind == DungeonOpKind::Front) {
            // centred_pair(): left half at 96-w, mirrored half at 96, width w each.
            const auto dims = catalog.wall[op.slice];
            std::printf("  %-3u %-5u %-6s %-21s %-5u %-6d %-6u %d..%d  (centred PAIR)\n",
                        unsigned(i), unsigned(op.depth), "pair", slice_family(op),
                        unsigned(op.slice), kDungeonCenterX - int(dims.w), unsigned(dims.w),
                        kDungeonCenterX - int(dims.w), kDungeonCenterX + int(dims.w));
        } else {
            std::printf("  %-3u %-5u %-6s %-21s type=0x%X sub=%u\n", unsigned(i),
                        unsigned(op.depth), "-", slice_family(op), unsigned(op.cell_type),
                        unsigned(op.sub));
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: b21a2_door_probe <dungeon-maps.txt>\n");
        return 2;
    }
    std::ifstream mf(argv[1]);
    if (!mf) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    uint8_t deceit[512]{};
    bool found = false;
    int loc;
    while (mf >> loc) {
        uint8_t cells[512];
        for (auto &c : cells) {
            int v;
            mf >> v;
            c = uint8_t(v);
        }
        if (loc == 33) {
            std::memcpy(deceit, cells, 512);
            found = true;
        }
    }
    if (!found) {
        std::fprintf(stderr, "Deceit (33) not present in the fixture\n");
        return 2;
    }

    GameState g{};
    g.torch_turns = 20; // lit
    TurnState t{};

    DungeonState d{};
    d.active = true;
    std::memcpy(d.cells, deceit, 512);
    d.wanderer = {};
    d.wanderer.type = 255; // no monster, so the plan is pure geometry

    struct View {
        uint8_t x, y;
        DungeonFacing facing;
        const char *label;
    };
    const View views[] = {
        {1, 4, DungeonFacing::West,
         "CASE A+B  -- the hardware view: front door r12 (0,4), ring-0 SIDE DOOR r10 (1,3),\n"
         "             and r14 (0,5) lateral to the FRONTAL door"},
        {1, 5, DungeonFacing::West,
         "CASE A    -- front door r14 (0,5); r12 (0,4) is lateral to the FRONTAL door"},
        {7, 3, DungeonFacing::South,
         "CASE A    -- front door r11 (7,4); r12 (0,4) lateral to it across the x-wrap"},
        {7, 6, DungeonFacing::North,
         "CASE A    -- front door r13 (7,5); r14 (0,5) lateral to it across the x-wrap"},
        {1, 4, DungeonFacing::North,
         "CASE B    -- front door r10 (1,3); r12 (0,4) is a ring-0 SIDE DOOR beside the party"},
        {1, 4, DungeonFacing::South,
         "CONTROL   -- open corridor ahead; both door neighbours are ring-0 side slices"},
    };

    std::printf("Batch 21A.2 -- authored Deceit (33) floor 7 door cluster\n");
    std::printf("Authored front/side slice widths (dungeon_art.cpp kDungeonWallDims):\n");
    const auto catalog = dungeon_art_authored_catalog();
    std::printf("  side  door   depth0..3 = %u/%u/%u/%u   at X left %d/%d/%d/%d, right %d/%d/%d/%d\n",
                unsigned(catalog.wall[4].w), unsigned(catalog.wall[5].w),
                unsigned(catalog.wall[6].w), unsigned(catalog.wall[7].w), kDungeonSideXLeft[0],
                kDungeonSideXLeft[1], kDungeonSideXLeft[2], kDungeonSideXLeft[3],
                kDungeonSideXRight[0], kDungeonSideXRight[1], kDungeonSideXRight[2],
                kDungeonSideXRight[3]);
    std::printf("  front door   depth0..3 = %u/%u/%u/%u   centred pair spans 96-w .. 96+w\n",
                unsigned(catalog.wall[12].w), unsigned(catalog.wall[13].w),
                unsigned(catalog.wall[14].w), unsigned(catalog.wall[15].w));

    for (const auto &v : views) {
        d.pos = {33, 7, v.x, v.y, v.facing};
        dump(d, g, t, v.label);
    }
    return 0;
}
