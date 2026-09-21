// Batch 12B -- the two hardware discrepancies the Batch 12 adjudication left open.
//
// Batch 12 proved the COMBAT half: In *Grav seeds no arena field, because the
// original seeds none. That verdict stands and is untouched here. The device
// then reported two separate things that verdict does not cover, and this suite
// is the host reproduction of both, on the real production paths and the real
// shipped data.
//
// OBSERVATION A -- a Destard dungeon cast logs `Cast`, and nothing appears in
// the 3D corridor. Split into the questions the report asks:
//   A1  does the destination cell actually change, with the reference tile, and
//       does it SURVIVE turning away, turning back, ticking and a step cycle?
//       (It does. The write is openu5::execute_dungeon_command()'s own; here it
//       is re-proved through those cycles and through the cell's own gameplay
//       effect, so "invisible" cannot be confused with "absent".)
//   A2  does the production 3D renderer emit anything for it? (It does not.
//       openu5::plan_dungeon_view() emits the Feature op -- the core knows the
//       field is there -- and openu5::dungeon_art_blits() returns 0 for it,
//       because a magic field has no ITEMS.16 image: the original draws it with
//       the procedural SPARKLE subsystem `magic_field_sparkle_drawer` @0x127e,
//       which the port had never carried. The viewport is therefore identical
//       with and without the field.)
//   A2b the same omission hides the 55 AUTHORED fields DUNGEON.DAT ships in
//       Wrong and Covetous, so this is not only a spell-feedback gap.
//
// OBSERVATION B -- an enemy rendered in the black void beyond the wall. Two
// independent findings, deliberately NOT merged:
//   B1  standing on BlackSquare (tile 255) is the ORIGINAL'S OWN AUTHORED
//       PLACEMENT. A large minority of the 128 shipped .CBT boards do it, and
//       the presentation transform is the identity (compose_combat_presentation
//       writes an actor at exactly y*11+x), so nothing is being displaced.
//       Guarded, not fixed.
//   B2  a real native defect found while establishing B1: the alpha resource
//       pack stores combatmaps.json's PER-TERRITORY index (britannia 0..15 then
//       dungeon 0..111), while openu5::dungeon_encounter() addresses an arena by
//       the GLOBAL catalog index openu5::dungeon_room_map() produces (16..127).
//       Every dungeon room therefore resolved to the NEXT dungeon's room, and
//       Doom's 112..127 resolved to nothing at all.
//
// Evidence, in the project's own order:
//   * CAST.OVL `cast_field_wall` @0x004c dungeon arm, table DS:0x4596 =
//     {0x82,0x81,0x80,0x83} (Batch 12's own trace, unchanged);
//   * DUNGEON.OVL `magic_field_sparkle_drawer` @0x127e, reached from
//     `feature_overlay_drawer_by_nibble` @0x19f6, tables DS 0x2e42/0x2e4a/
//     0x2e52/0x2e5a and the colour globals DS 0x13ae/0x13b2/0x13b4/0x13b6 --
//     derived instruction by instruction in re/notes/dungeon-decor-mazmorra.md
//     (sections 2/5 and section 6 ticket 4) and implemented in the accepted
//     reference `game/src/skin/fiel/dungeon-decor.ts` (`fieldSparkRects`);
//   * DUNGEON.OVL 0x003a / DNGLOOK 0x0844 room-board arithmetic, and the
//     reference `roomCombatMapIndex` / `dungeonOrderSkippingDespise` in
//     `game/src/core/dungeon/dungeon.ts`;
//   * the shipped data itself: native/assets/openu5-alpha1-resources.bin, the
//     exact bytes on the T-Deck's card.
#include "alpha_resources.h"
#include "dungeon_art_cache.h"
#include "native_renderer.h"
#include "openu5/commands.h"
#include "openu5/dungeon.h"
#include "openu5/dungeon_art.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/dungeon_view.h"
#include "openu5/presentation.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "batch12b: FAIL " << what << "\n";
        ++failures;
    }
}

// Spell ids, from the generated magic_tables.inc / MagicDefinitions.json order.
constexpr int kInFlamGrav = 14, kInNoxGrav = 15, kInZuGrav = 16, kInSanctGrav = 20;
// Dungeon field tiles: DS:0x4596, written only by CAST.OVL's dungeon arm.
constexpr int kDungeonFire = 0x82, kDungeonPoison = 0x81, kDungeonSleep = 0x80,
              kDungeonEnergy = 0x83;
constexpr int16_t kBlackSquare = 255;

// ---------------------------------------------------------------------------
// A dungeon session driven through the REAL production command entry point,
// openu5::execute_dungeon_command() -- the same one AlphaRuntime::command()
// reaches for a Cast while c.dungeon is set (commands.cpp's dungeon gate).
// ---------------------------------------------------------------------------
struct DungeonHarness {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> tiles;
    DungeonState state{};
    DungeonScratch scratch{};
    DungeonContext dungeon;
    std::vector<std::string> texts;

    DungeonHarness() : tiles(256 * 256, 5), dungeon{state, scratch} {
        game.position = {{40, 41}, {0, 0}};
        game.party.character_count = 1;
        game.party.party_size = 1;
        auto &m = game.party.characters[0];
        std::strncpy(m.name, "Mage", sizeof(m.name) - 1);
        m.character_class = 'M';
        m.status = 'G';
        m.party_status = 0;
        m.current_hp = m.max_hp = 200;
        m.intelligence = 30;
        m.dexterity = 1; // A field save is rand(1,30) >= dexterity: always afflicts.
        m.current_mp = 99;
        m.level = 8;
        for (auto &q : game.spell_quantities)
            q = 20;
        // Destard is dungeon 35 (33..40 = Deceit, Despise, Destard, Wrong,
        // Covetous, Shame, Hythloth, Doom) -- the dungeon the report names.
        state.active = true;
        state.pos = {35, 0, 4, 4, DungeonFacing::North};
        game.torch_turns = 60; // DUNGEON:0x1AD6's light gate; without it, black.
    }

    CommandContext context() {
        CommandContext c{game, turn, travel, commands,
                         WorldData{tiles.data(), tiles.data(), tiles.size(), tiles.size()}};
        c.dungeon_context = &dungeon;
        c.dungeon = true;
        return c;
    }
    ActionResult run(Command cmd) {
        auto c = context();
        c.events = {this, [](void *p, const GameEvent &e) {
                        if (e.text)
                            static_cast<DungeonHarness *>(p)->texts.push_back(e.text);
                    }};
        return execute_dungeon_command(c, cmd);
    }
    ActionResult cast(int spell) {
        Command cmd;
        cmd.kind = CommandKind::Cast;
        cmd.item = int16_t(spell);
        cmd.caster = 0;
        return run(cmd);
    }
    ActionResult act(DungeonAction action) {
        Command cmd;
        cmd.kind = CommandKind::DungeonCommand;
        cmd.item = int16_t(action);
        return run(cmd);
    }
    uint8_t &cell(int x, int y) {
        return state.cells[size_t(state.pos.floor) * 64 + size_t(y) * 8 + size_t(x)];
    }
    // Facing North from (4,4): the cell the cast targets is (4,3).
    uint8_t &ahead() { return cell(4, 3); }
    bool said(const char *needle) const {
        for (const auto &t : texts)
            if (t.find(needle) != std::string::npos)
                return true;
        return false;
    }
    void clear() { texts.clear(); }
};

// ---------------------------------------------------------------------------
// A1 -- the destination cell DOES change, with the reference tile, and the
// change survives turning away, turning back, ticking and a step cycle.
// ---------------------------------------------------------------------------
void test_dungeon_field_state() {
    struct Row {
        int spell, tile;
    } rows[] = {{kInFlamGrav, kDungeonFire},
                {kInNoxGrav, kDungeonPoison},
                {kInZuGrav, kDungeonSleep},
                {kInSanctGrav, kDungeonEnergy}};
    for (const auto &row : rows) {
        DungeonHarness h;
        const int mp = h.game.party.characters[0].current_mp;
        h.cast(row.spell);
        // The device log line the report quotes is the SUCCESS path: no "Failed!".
        check(!h.said("Failed!"), "A1: the cast succeeds (no Failed!)");
        check(h.game.spell_quantities[row.spell] == 19, "A1: the mixed spell is consumed");
        check(h.game.party.characters[0].current_mp < mp, "A1: mana is consumed");
        check(h.ahead() == row.tile, "A1: the cell ahead holds the reference field tile");
        check((h.ahead() >> 4) == uint8_t(DungeonCellKind::MagicField),
              "A1: the cell ahead is kind 8 (MagicField)");
        int written = 0;
        for (auto c : h.state.cells)
            if (c)
                ++written;
        check(written == 1, "A1: exactly one cell was written");

        // Turn away, turn back. Four Rights return the party to North.
        for (int i = 0; i < 4; ++i)
            h.act(DungeonAction::Right);
        check(h.state.pos.facing == DungeonFacing::North, "A1: four Rights restore the facing");
        check(h.ahead() == row.tile, "A1: turning away and back leaves the field in place");

        // TurnAround twice -- the other rotation path.
        h.act(DungeonAction::TurnAround);
        h.act(DungeonAction::TurnAround);
        check(h.ahead() == row.tile, "A1: two TurnArounds leave the field in place");

        // A refresh/update cycle: Pass ticks the dungeon clock.
        for (int i = 0; i < 5; ++i)
            h.act(DungeonAction::Pass);
        check(h.ahead() == row.tile, "A1: five Passes leave the field in place");

        // Step away from the field cell and back to the original stance.
        h.act(DungeonAction::TurnAround);
        h.act(DungeonAction::Forward);
        h.act(DungeonAction::TurnAround);
        h.act(DungeonAction::Forward);
        check(h.state.pos.x == 4 && h.state.pos.y == 4, "A1: the party returns to its own cell");
        check(h.ahead() == row.tile, "A1: a full move-out/move-back cycle keeps the field");
    }
}

// The field is not decoration: walking into it fires the cell's own effect,
// which is the strongest available proof that the state is logically live.
void test_dungeon_field_is_live() {
    {
        DungeonHarness h;
        h.cast(kInFlamGrav);
        const int hp = h.game.party.characters[0].current_hp;
        h.clear();
        h.act(DungeonAction::Forward);
        check(h.said("Fire!!"), "A1: stepping into an In Flam Grav field reports Fire!!");
        check(h.game.party.characters[0].current_hp < hp, "A1: the fire field damages the party");
        check(h.cell(4, 3) == kDungeonFire, "A1: a fire field survives being stepped on");
    }
    {
        DungeonHarness h;
        h.cast(kInZuGrav);
        h.clear();
        h.act(DungeonAction::Forward);
        check(h.said("Sleep spell!"), "A1: stepping into an In Zu Grav field reports Sleep spell!");
        // DUNGEON's own rule: a sleep field consumes itself on entry (the
        // write-back at dungeon.cpp's `field == 0` arm), unlike fire/poison.
        check((h.cell(4, 3) >> 4) != uint8_t(DungeonCellKind::MagicField),
              "A1: a sleep field consumes itself on entry, as the reference does");
    }
}

// ---------------------------------------------------------------------------
// A2 -- the production 3D renderer. The plan KNOWS about the field; the art
// layer emits nothing for it; the painted viewport is therefore identical with
// and without it.
// ---------------------------------------------------------------------------
struct Viewport {
    std::vector<uint16_t> pixels;
    RenderReport report{};
    uint16_t primitives = 0;
    Viewport() : pixels(kViewportPixelCount, 0) {}
    bool any_non_black() const {
        for (auto p : pixels)
            if (p != 0)
                return true;
        return false;
    }
};

void paint(DungeonHarness &h, const DungeonArtSurfaces &art, Viewport &out) {
    const auto e = render_dungeon_view(h.game, h.turn, h.state, art, /*phase=*/0,
                                       out.pixels.data(), out.pixels.size(), out.report,
                                       out.primitives);
    check(e == ESP_OK, "A2: render_dungeon_view accepts the live dungeon session");
}

const DungeonDrawOp *find_op(const DungeonViewPlan &plan, DungeonOpKind kind, int depth) {
    for (uint8_t i = 0; i < plan.count; ++i)
        if (plan.ops[i].kind == kind && plan.ops[i].depth == depth)
            return &plan.ops[i];
    return nullptr;
}

void test_dungeon_field_render(const DungeonArtSurfaces &art) {
    // Positive control: an ordinary corridor with real authored art paints.
    DungeonHarness base;
    Viewport before;
    paint(base, art, before);
    check(before.primitives > 0, "A2 control: an ordinary lit corridor emits blits");
    check(before.any_non_black(), "A2 control: an ordinary lit corridor paints pixels");
    check(std::strcmp(before.report.map_context, "dungeon3d authored viewport") == 0,
          "A2 control: the corridor reports the authored viewport");

    // Negative control: the same corridor with no light paints nothing, so the
    // comparison below measures art and not the light gate.
    {
        DungeonHarness dark;
        dark.game.torch_turns = 0;
        Viewport unlit;
        paint(dark, art, unlit);
        check(unlit.primitives == 0 && !unlit.any_non_black(),
              "A2 control: an unlit corridor stays black");
    }

    for (int spell : {kInFlamGrav, kInNoxGrav, kInZuGrav, kInSanctGrav}) {
        DungeonHarness h;
        h.cast(spell);
        check((h.ahead() >> 4) == uint8_t(DungeonCellKind::MagicField),
              "A2: the fixture really does hold a field before rendering");

        // The CORE knows: plan_dungeon_view emits the Feature op for the field
        // cell at depth 1, exactly as the reference planner does.
        const auto plan = plan_dungeon_view(h.game, h.turn, h.state);
        const auto *feature = find_op(plan, DungeonOpKind::Feature, 1);
        check(feature != nullptr, "A2: the plan emits a Feature op for the field cell");
        if (feature) {
            check(feature->cell_type == uint8_t(DungeonCellKind::MagicField),
                  "A2: that Feature op is the magic field");
        }

        Viewport after;
        paint(h, art, after);
        check(after.primitives > before.primitives,
              "A2 RED: painting a field adds at least one primitive");
        check(after.pixels != before.pixels,
              "A2 RED: the viewport differs once a field stands in the corridor");
    }
}

// A2b -- the same omission hides the fields DUNGEON.DAT already ships. Wrong and
// Covetous author magic-field cells that have never been drawn. This uses the
// pack's own dungeon cells, not a synthesised fixture.
void test_authored_dungeon_field_render(const tdeck::AlphaResourceOwners &owners,
                                        const DungeonArtSurfaces &art) {
    const DungeonData *wrong = nullptr;
    for (size_t i = 0; i < 8; ++i)
        if (owners.dungeons && owners.dungeons[i].location == 36)
            wrong = &owners.dungeons[i];
    check(wrong != nullptr, "A2b: the shipped pack carries Wrong (dungeon 36)");
    if (!wrong)
        return;

    int authored = 0, floor = -1, fx = -1, fy = -1;
    for (int n = 0; n < 512; ++n)
        if ((wrong->cells[n] >> 4) == uint8_t(DungeonCellKind::MagicField)) {
            ++authored;
            if (floor < 0) {
                floor = n / 64;
                fx = (n % 64) % 8;
                fy = (n % 64) / 8;
            }
        }
    check(authored > 0, "A2b: Wrong authors magic-field cells in DUNGEON.DAT");
    std::cout << "batch12b: authored magic-field cells in Wrong: " << authored << "\n";
    if (floor < 0)
        return;

    // Stand one cell SOUTH of the authored field and look north at it.
    DungeonHarness h;
    h.state.pos = {36, uint8_t(floor), uint8_t(fx), uint8_t((fy + 1) & 7), DungeonFacing::North};
    std::memcpy(h.state.cells, wrong->cells, sizeof(h.state.cells));
    // Clear whatever the authored map puts under the party's feet so the only
    // difference between the two renders below is the field itself.
    h.cell(h.state.pos.x, h.state.pos.y) = 0;
    check((h.cell(fx, fy) >> 4) == uint8_t(DungeonCellKind::MagicField),
          "A2b: the party is facing the authored field");

    Viewport with;
    paint(h, art, with);
    const uint8_t saved = h.cell(fx, fy);
    h.cell(fx, fy) = 0;
    Viewport without;
    paint(h, art, without);
    h.cell(fx, fy) = saved;
    check(with.pixels != without.pixels,
          "A2b RED: an AUTHORED dungeon field is visible in the 3D corridor");
}

// ---------------------------------------------------------------------------
// B1 -- an enemy on a BlackSquare is the original's own authored placement, and
// the presentation transform is the identity.
// ---------------------------------------------------------------------------
void test_authored_out_of_wall_placement(const tdeck::AlphaResourceOwners &owners) {
    int boards_with_void_units = 0, void_units = 0;
    for (size_t i = 0; i < owners.combat_map_count; ++i) {
        const auto &m = *owners.combat_map_views[i];
        bool any = false;
        for (int u = 0; u < m.unit_count; ++u) {
            const uint8_t sprite = owners.combat_sprites[i * 16 + size_t(u)];
            if (!sprite)
                continue;
            const auto p = m.units[u];
            if (p.x < 0 || p.y < 0 || p.x > 10 || p.y > 10)
                continue;
            if (m.tiles[p.y * 11 + p.x] == kBlackSquare) {
                any = true;
                ++void_units;
            }
        }
        if (any)
            ++boards_with_void_units;
    }
    // Not a tolerance: this is what the 1988 .CBT files contain. It is asserted
    // as a GUARD, so that a later "tidy-up" of the authored boards is caught.
    check(boards_with_void_units > 0 && void_units > 0,
          "B1: the shipped .CBT boards author unit slots on BlackSquare");
    std::cout << "batch12b: authored boards placing units on BlackSquare: "
              << boards_with_void_units << " of " << owners.combat_map_count << " (" << void_units
              << " slots)\n";

    // The renderer does not move an actor: compose_combat_presentation writes it
    // at exactly y*11+x, so a unit authored on a black cell is DRAWN on that
    // black cell. Nothing is displaced by a transform.
    GameState game{};
    game.party.character_count = 1;
    game.party.party_size = 1;
    auto &member = game.party.characters[0];
    member.status = 'G';
    member.character_class = 'F';
    member.current_hp = member.max_hp = 50;
    CombatState combat{};
    for (auto &t : combat.map.tiles)
        t = kBlackSquare;
    CombatEnemy def{};
    def.index = 1;
    def.name = "Enemy";
    def.group_name = "Enemies";
    def.hp = 10;
    combat.count = 1;
    combat.actors[0].enemy = &def;
    combat.actors[0].hp = combat.actors[0].max_hp = 10;
    combat.actors[0].id = 1;
    combat.actors[0].position = {0, 0};
    combat.actors[0].render_tile = -1;
    const auto snapshot = compose_combat_presentation(combat, game);
    check(snapshot.tiles[0] != kBlackSquare,
          "B1: an actor on a black corner cell is drawn at that corner");
    check(snapshot.actor_ids[0] == 0x40000U + 1U,
          "B1: the presentation transform is the identity (y*11+x)");
    combat.actors[0].position = {10, 7};
    const auto moved = compose_combat_presentation(combat, game);
    check(moved.actor_ids[7 * 11 + 10] == 0x40000U + 1U,
          "B1: an actor at (10,7) lands at index 7*11+10, not anywhere else");
}

// ---------------------------------------------------------------------------
// B2 -- the arena a dungeon ROOM resolves to. The shipped pack IS the catalog
// openu5::dungeon_room_map() indexes (16 britannia boards followed by 7x16
// dungeon rooms), so room r of dungeon `loc` must resolve to the board at that
// position. It resolves 16 slots -- one whole dungeon -- too far, and Doom
// resolves to nothing at all.
// ---------------------------------------------------------------------------
void test_dungeon_room_arena_identity(const tdeck::AlphaResourceOwners &owners) {
    check(owners.combat_map_count == 128,
          "B2: the pack carries 16 britannia boards plus 7x16 dungeon rooms");
    std::vector<DungeonArena> arenas(owners.combat_map_count);
    for (size_t i = 0; i < owners.combat_map_count; ++i) {
        arenas[i].map = owners.combat_map_views[i];
        arenas[i].sprites = owners.combat_sprites + i * 16;
    }

    const auto resolve = [&arenas](int global) {
        for (size_t i = 0; i < arenas.size(); ++i)
            if (arenas[i].map && arenas[i].map->index == global)
                return int(i);
        return -1;
    };

    for (int loc = 33; loc <= 40; ++loc) {
        if (loc == 34)
            continue; // Despise authors no room cells (re/notes/dungeon.md 14.1).
        for (int room = 0; room < 16; room += 5) {
            const int global = dungeon_room_map(loc, room);
            check(resolve(global) == global,
                  "B2 RED: a dungeon room resolves to its own board in the shipped catalog");
        }
    }
    // The sharpest single case: Doom's rooms are 112..127 and match nothing, so
    // entering one returns MissingMap instead of a fight.
    check(resolve(dungeon_room_map(40, 0)) >= 0,
          "B2 RED: Doom's room 0 resolves to a board at all");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: batch12b_hardware_test <openu5-alpha1-resources.bin>\n";
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) {
        std::cerr << "batch12b: cannot open the alpha resource pack at " << argv[1] << "\n";
        return 2;
    }
    tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) {
        std::cerr << "batch12b: cannot load the alpha resource pack\n";
        return 2;
    }
    tdeck::DungeonArtCache art;
    if (art.load(pack) != ESP_OK || !art.ready()) {
        std::cerr << "batch12b: cannot load the authored dungeon art\n";
        return 2;
    }
    // The production repoint AlphaRuntime performs before a dungeon redraw.
    art.select(DungeonArtCacheKey{dungeon_art_wall_bank_index(dungeon_wall_variant(35))});

    test_dungeon_field_state();
    test_dungeon_field_is_live();
    test_dungeon_field_render(art.surfaces());
    test_authored_dungeon_field_render(owners, art.surfaces());
    test_authored_out_of_wall_placement(owners);
    test_dungeon_room_arena_identity(owners);

    art.release();
    owners.release();
    pack.close();
    if (failures) {
        std::cerr << "batch12b: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "Batch 12B hardware discrepancies: dungeon field state, persistence and 3D "
                 "rendering, and dungeon room arena identity, all reference-faithful\n";
    return 0;
}
