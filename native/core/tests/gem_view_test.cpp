// Batch 10 -- R-17/Y-14: View Gem presentation parity.
//
// Every case here asserts what the ORIGINAL presents, sourced in this order:
//   [REF-BIN] LOOKOBJ.OVL `gem_view`/`draw_gem_map_tile` and DNGLOOK.OVL
//             0x06a8/0x0340, as recorded in the project reference sources;
//   [REF-TS]  game/src/core/world/gem-view.ts (`buildGemView`),
//             game/src/core/world/chunk-origin.ts (`initChunkOrigin`) and
//             game/src/skin/fiel/gemmap-overworld.ts (`GEM_CATEGORY`);
//   [NATIVE]  native/core/src/gem_view.cpp, the ported semantic layer.
//
// The suite drives openu5::build_world_gem_view()/build_dungeon_gem_view(),
// the semantic data the device renderer paints. It deliberately does NOT
// assert pixels: which category/cell goes where is decidable without a
// framebuffer, exactly like dungeon_view_regression's plan_dungeon_view().
#include "openu5/gem_view.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "gem_view regression: " << what << "\n";
        ++failures;
    }
}

// -- A. Terrain classification -----------------------------------------
// One representative tile per category actually emitted by GEM_CATEGORY
// (index = tile id, value = category), read directly off the ported table
// (native/core/src/gem_category.inc, guarded against drift from
// gemmap-overworld.ts by the separate gem_category_table_drift test). This
// proves the classification function agrees with the reference categories,
// not raw tile-number ranges.
void a_terrain_classification() {
    struct Case { uint8_t tile, category; const char *name; };
    constexpr Case cases[] = {
        {0, 0, "void"},       {5, 1, "grass"},        {9, 2, "fill-green"},
        {7, 3, "fill-red"},   {29, 4, "hlines-white"}, {16, 5, "dashes-white"},
        {13, 6, "frame-white"}, {12, 7, "fill-white"}, {11, 8, "hills-yellow"},
        {6, 9, "forest-green"}, {3, 10, "coast"},       {2, 11, "water-ltblue"},
        {1, 12, "deepwater-ltblue"}, {4, 13, "swamp"},  {224, 14, "signpost-white"},
        {216, 15, "fountain-ltblue"}, {32, 16, "road"},
    };
    for (const auto &c : cases) {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "category(tile %u) should be %u (%s)", c.tile, c.category, c.name);
        check(gem_terrain_category(c.tile) == c.category, msg);
    }
}

// -- B. Mixed town map fixture + full-square + marker not centred -------
void b_mixed_town_map() {
    std::array<uint8_t, 1024> tiles{};
    tiles.fill(0); // background: tile 0 -> category 0 (void)
    tiles[0 * 32 + 0] = 5;    // (x=0,y=0)  -> category 1 (grass)
    tiles[5 * 32 + 10] = 216; // (x=10,y=5) -> category 15 (fountain)
    tiles[20 * 32 + 20] = 32; // (x=20,y=20)-> category 16 (road)
    tiles[31 * 32 + 31] = 11; // (x=31,y=31)-> category 8 (hills)

    ActiveMap map{{5, 0}, MapKind::Small, {32, 32, false}, tiles.data(), 0};
    const GemView v = build_world_gem_view(map, Position{16, 16});

    check(!v.dungeon, "town gem view must not set the dungeon flag");
    check(v.width == 32 && v.height == 32, "town gem view must be the full 32x32 square (Y-14)");
    check(v.cells[0][0].value == 1, "town (0,0) grass should classify as category 1");
    check(v.cells[5][10].value == 15, "town (x=10,y=5) fountain should land at cells[row=5][col=10]");
    check(v.cells[20][20].value == 16, "town (20,20) road should classify as category 16");
    check(v.cells[31][31].value == 8, "town far corner (31,31) must be reachable, not cropped");
    check(v.cells[1][1].value == 0, "town background tile should classify as category 0 (void)");
    // A town's gem view is the WHOLE fixed map, never centred/scrolled on the
    // party -- the marker is the party's raw map position.
    check(v.marker_x == 16 && v.marker_y == 16, "town marker must be the party's raw (unscrolled) position");
}

// -- C. Orientation: N/S/E/W and row/col indexing ------------------------
void c_orientation() {
    std::array<uint8_t, 65536> tiles{};
    // Party at (104,104): initChunkOrigin aligns to the 16-block (96,96)
    // (104 & 0xf = 8, not < 8, so no leftward shift) -> marker (8,8). The
    // marker sits only 8 cells from the window's north/west edge (the
    // original never centres it -- "columnas 8..23 del 32"), so every probe
    // below stays within a 6-cell offset to remain inside the 32x32 window.
    const uint8_t north_tile = 5, south_tile = 224, east_tile = 216, west_tile = 32;
    tiles[98 * 256 + 104] = north_tile;  // world (104,98): 6 north of the party
    tiles[110 * 256 + 104] = south_tile; // world (104,110): 6 south
    tiles[104 * 256 + 110] = east_tile;  // world (110,104): 6 east
    tiles[104 * 256 + 98] = west_tile;   // world (98,104): 6 west

    ActiveMap map{{0, 0}, MapKind::Overworld, {256, 256, true}, tiles.data(), -1};
    const GemView v = build_world_gem_view(map, Position{104, 104});

    check(v.marker_x == 8 && v.marker_y == 8, "chunk-origin marker for party (104,104) should be (8,8)");
    // North = smaller world y = smaller ROW, at the same column as the party.
    check(v.cells[2][8].value == gem_terrain_category(north_tile), "north probe must land at a smaller row, same column");
    // South = larger world y = larger row.
    check(v.cells[14][8].value == gem_terrain_category(south_tile), "south probe must land at a larger row, same column");
    // East = larger world x = larger column, same row.
    check(v.cells[8][14].value == gem_terrain_category(east_tile), "east probe must land at a larger column, same row");
    // West = smaller world x = smaller column, same row.
    check(v.cells[8][2].value == gem_terrain_category(west_tile), "west probe must land at a smaller column, same row");
}

// -- D. Party marker: correct cell, terrain classification untouched ----
void d_marker_does_not_alter_terrain() {
    std::array<uint8_t, 65536> tiles{};
    tiles[104 * 256 + 104] = 1;  // under the party: category 12
    tiles[104 * 256 + 105] = 11; // immediate east neighbour: category 8
    ActiveMap map{{0, 0}, MapKind::Overworld, {256, 256, true}, tiles.data(), -1};
    const GemView v = build_world_gem_view(map, Position{104, 104});

    check(v.marker_x == 8 && v.marker_y == 8, "marker cell must be (8,8) for party (104,104)");
    check(v.cells[8][8].value == gem_terrain_category(1),
          "the cell under the marker must still report its own terrain category");
    check(v.cells[8][9].value == gem_terrain_category(11),
          "a neighbour of the marker cell must be unaffected by the marker");
}

// -- E. Full-square output: overworld/town always fully populated; the
//       dungeon display is the whole 22x22, not a cropped sub-window. -----
void e_full_square_no_clipping() {
    std::array<uint8_t, 65536> tiles{};
    tiles.fill(7); // uniform category 3 (any non-zero, distinguishable fill)
    ActiveMap map{{0, 0}, MapKind::Overworld, {256, 256, true}, tiles.data(), -1};
    const GemView v = build_world_gem_view(map, Position{50, 50});
    check(v.width == 32 && v.height == 32, "overworld gem view must report the full 32x32 square");
    bool every_cell_populated = true;
    for (int row = 0; row < 32 && every_cell_populated; ++row)
        for (int col = 0; col < 32; ++col)
            if (v.cells[row][col].value != gem_terrain_category(7)) { every_cell_populated = false; break; }
    check(every_cell_populated, "every one of the 32x32 overworld cells must be classified, none left cropped/blank");

    // An all-open dungeon floor: nothing blocks the flood, so it must fill
    // the ENTIRE 22x22 display -- proving the display isn't silently
    // truncated to a smaller centred sub-square before rendering.
    DungeonState d{};
    d.active = true;
    d.pos = {33, 0, 4, 4, DungeonFacing::North};
    for (auto &c : d.cells) c = 0x00; // open corridor everywhere
    const GemView g = build_dungeon_gem_view(d);
    check(g.dungeon, "dungeon gem view must set the dungeon flag");
    check(g.width == 22 && g.height == 22, "dungeon gem view must report the full 22x22 display");
    int unreached = 0;
    for (int row = 0; row < 22; ++row)
        for (int col = 0; col < 22; ++col)
            if (g.cells[row][col].value == kGemDungeonUnreached) ++unreached;
    // Only the centre (the party's own seed cell) is never marked reached --
    // it carries the marker instead, exactly as the reference never draws it.
    check(unreached == 1, "an open floor must flood-fill all but the centre seed cell of the 22x22 display");
    check(g.cells[kGemDungeonCenter][kGemDungeonCenter].value == kGemDungeonUnreached,
          "the centre seed cell itself must stay unreached (the marker paints over it)");
}

// -- F. Bounds behaviour: wall/special-wall/secret cut the flood; doors and
//       rooms do not; wrap at map edges; a secret door blocks the gem even
//       when `revealed`, unlike movement passability. ---------------------
void f_bounds_and_blockers() {
    // F1: an isolated party in a 1-cell box of ordinary wall should only
    // light the 8 immediate neighbours; nothing beyond the box propagates.
    {
        DungeonState d{};
        d.active = true;
        d.pos = {33, 0, 4, 4, DungeonFacing::North};
        for (auto &c : d.cells) c = 0xb0; // solid wall everywhere
        const GemView g = build_dungeon_gem_view(d);
        int reached_count = 0;
        for (int row = 0; row < 22; ++row)
            for (int col = 0; col < 22; ++col)
                if (g.cells[row][col].value != kGemDungeonUnreached) ++reached_count;
        // The 8 ring cells around the centre are drawn as the flood's
        // border (walls are drawn, just not propagated through).
        check(reached_count == 8, "a party boxed in by ordinary wall must only light the 8 bordering cells");
    }
    // F2: a normal door and a room do NOT cut the flood -- the gem sees
    // through them, unlike a wall/special-wall/secret door.
    {
        DungeonState d{};
        d.active = true;
        d.pos = {33, 0, 4, 4, DungeonFacing::North};
        for (auto &c : d.cells) c = 0xb0;
        auto set = [&](int x, int y, uint8_t v) { d.cells[(x & 7) + (y & 7) * 8] = v; };
        set(5, 4, 0xe0); // a normal door one step east of the party
        set(6, 4, 0xf0); // a room beyond the door
        const GemView g = build_dungeon_gem_view(d);
        check(g.cells[kGemDungeonCenter][kGemDungeonCenter + 1].value == 0xe,
              "a door adjacent to the party must be classified as a door");
        check(g.cells[kGemDungeonCenter][kGemDungeonCenter + 2].value == 0xf,
              "the room beyond an open door must still be reached -- doors/rooms do not cut the flood");
    }
    // F3: a secret door blocks the gem's flood even when `revealed` is set --
    // the gem view (unlike movement) never consults discovery state. The rest
    // of the floor is solid wall so the "beyond" cell has only ONE possible
    // route in, exactly like the F2 door/room corridor above.
    {
        DungeonState d{};
        d.active = true;
        d.pos = {33, 0, 4, 4, DungeonFacing::North};
        for (auto &c : d.cells) c = 0xb0; // solid wall everywhere
        auto index_of = [](int f, int x, int y) { return f * 64 + (y & 7) * 8 + (x & 7); };
        d.cells[index_of(0, 5, 4)] = 0xd0; // secret door one step east of the party
        d.cells[index_of(0, 6, 4)] = 0x00; // open corridor beyond the door
        const int n = index_of(0, 5, 4);
        d.revealed[n >> 3] = uint8_t(d.revealed[n >> 3] | (1u << (n & 7))); // discovered
        const GemView g = build_dungeon_gem_view(d);
        check(g.cells[kGemDungeonCenter][kGemDungeonCenter + 1].value == 0xd,
              "a revealed secret door is still classified/drawn as a secret door on the gem");
        check(g.cells[kGemDungeonCenter][kGemDungeonCenter + 2].value == kGemDungeonUnreached,
              "a REVEALED secret door must still block the gem's flood -- the gem ignores discovery state");
    }
    // F4: overworld wrap at the map edge must be reproduced end to end.
    {
        std::array<uint8_t, 65536> tiles{};
        tiles[5 * 256 + 5] = 11; // world (5,5) -> category 8
        ActiveMap map{{0, 0}, MapKind::Overworld, {256, 256, true}, tiles.data(), -1};
        // Party near the high edge: initChunkOrigin(250,250) -> (240,240).
        const GemView v = build_world_gem_view(map, Position{250, 250});
        // col/row where origin(240)+idx wraps to world 5: idx = (5-240) mod 256 = 21.
        check(v.cells[21][21].value == gem_terrain_category(11),
              "the toroidal wrap at the overworld edge must reach a tile placed just past 255");
    }
}

} // namespace

int main() {
    a_terrain_classification();
    b_mixed_town_map();
    c_orientation();
    d_marker_does_not_alter_terrain();
    e_full_square_no_clipping();
    f_bounds_and_blockers();
    if (failures) {
        std::cerr << "gem_view regression: " << failures << " failing assertion(s)\n";
        return 1;
    }
    std::cout << "gem_view regression: all assertions pass\n";
    return 0;
}
