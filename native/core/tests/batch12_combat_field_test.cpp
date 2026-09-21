// Batch 12 -- combat field magic.
//
// PREMISE CORRECTION.  This batch was opened on a hardware report that casting
// In Flam Grav / In Nox Grav / In Zu Grav in the arena "produces the flash but
// leaves no persistent field".  Phase A found that observation is the
// REFERENCE'S OWN BEHAVIOR, not a port defect, and this file exists to pin it
// so nobody "fixes" it back into a divergence.
//
// `cast_field_wall` (CAST.OVL 0x004c) splits at `0054 cmp byte [g_location],0x80`
// / `jb`:
//
//   * BELOW 0x80 -- DUNGEON.  Writes ONE field tile into the cell ahead, taken
//     from the field-tile table DS:0x4596 = {0x82,0x81,0x80,0x83}, guarded by
//     `00b8 test ...,0xf7` (empty floor only) and preserving bit 3
//     (`00c6 al=[bp-8]&8` / `00ce or al,[bx+0x4596]`).  This is the ONLY site in
//     the entire disassembly that references DS:0x4596.
//
//   * AT OR ABOVE 0x80 -- COMBAT.  Seeds NOTHING.  It sets a spell weapon
//     (`00ef mov al,[bx+0x4592]` = 0x35/0x33/0x34/0x36 for args 0..3) and calls
//     `0100 call 0xffffc14a` = COMSUBS.OVL:0x0c52 `attack_dispatch_by_reach` --
//     literally the same dispatcher, with the same two arguments, that Grav Por
//     / Vas Flam / Xen Corp reach through `cmb_set_weapon_then_attack`.  The
//     damage comes from `attackValues[0x33..0x36]` = 18 / 0 / 21 / 0.
//
// So in combat In Zu Grav and In Sanct Grav spend the mixed spell, the mana and
// the turn and do ZERO, and In Nox Grav / In Flam Grav are a dart, not a wall.
// That is registered as an ORIGINAL defect, deliberately cloned:
// docs/bugs-del-original.md SS2.9 (ticket #91), re/notes/field-spell-port.md,
// re/notes/field-grav-gate-testigo-20260808.md (live-binary witness: the four
// In*Grav masks in DS:0x1C90 are 0x03 = dungeon+combat only, read from running
// RAM, with In Lor as positive control).
//
// Arena fields DO exist -- they are authored .CBT units of family 0xE8 (26 of
// them across cm18 = Deceit r2, cm20 = Deceit r4, cm121 = Doom r9), seeded at
// arena construction, and consumed by COMBAT:0x0000 (occupancy) and
// COMBAT:0x1b1e (end-of-turn magnitude dispatch).  Those halves are covered
// here too, so "no field is created by the cast" cannot pass merely because the
// field subsystem is missing.
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/magic.h"
#include "openu5/presentation.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace openu5;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        std::cerr << "batch12: FAIL " << what << "\n";
        ++failures;
    }
}

// Spell ids, from the generated magic_tables.inc / MagicDefinitions.json order.
constexpr int kInFlamGrav = 14, kInNoxGrav = 15, kInZuGrav = 16, kAnGrav = 18,
              kInSanctGrav = 20, kInBetXen = 24, kKalXenCorp = 43;

// Arena field tiles: the 0xE8 family the .CBT seeds and COMBAT:0x1b1e reads.
constexpr int kFieldPoison = 0xE8, kFieldSleep = 0xE9, kFieldFire = 0xEA,
              kFieldEnergy = 0xEB;

// Dungeon field tiles: DS:0x4596, written only by the dungeon branch.
constexpr int kDungeonFire = 0x82, kDungeonPoison = 0x81, kDungeonSleep = 0x80,
              kDungeonEnergy = 0x83;

constexpr int kFloor = 5; // kCombatTileFlags[5] == 7: walkable, ground, spell-line clear.
constexpr int kOpaque = 12; // kCombatTileFlags[12] == 0: blocks the spell line.

struct Arena {
    GameState game{};
    TurnState turn{};
    CombatState combat{};
    CombatMap map{};
    std::vector<CombatEnemy> defs;
    std::vector<const CombatEnemy *> def_ptrs;
    CombatField fields[16]{};
    std::vector<CombatEvent> events;
    std::vector<std::string> texts;

    // 64 placeholder definitions so the summon indices the reference uses
    // (31 = insect swarm, 38 = daemon, 20 = rat) resolve without an asset load.
    Arena() {
        defs.resize(64);
        for (size_t i = 0; i < defs.size(); ++i) {
            auto &d = defs[i];
            d.index = int32_t(i);
            d.name = "Target";
            d.group_name = "Targets";
            d.hp = 200;
            d.strength = 10;
            d.dexterity = 1;
            d.intelligence = 1;
            d.armor = 0;
            d.damage = 1;
            d.max_per_map = 4;
            d.range = 1;
            d.tile = 0x40;
            d.move_class = 0;
        }
        for (auto &d : defs)
            def_ptrs.push_back(&d);

        game.position = {{40, 41}, {0x80, 0}};
        game.party.character_count = 1;
        game.party.party_size = 1;
        auto &m = game.party.characters[0];
        std::strncpy(m.name, "Mage", sizeof(m.name) - 1);
        m.character_class = 'M';
        m.status = 'G';
        m.party_status = 0;
        m.current_hp = m.max_hp = 200;
        m.strength = 20;
        m.dexterity = 30;
        m.intelligence = 30;
        m.current_mp = 99;
        m.level = 8;
        m.helmet = m.armor = m.weapon = m.shield = m.ring = m.amulet = 255;
        for (auto &q : game.spell_quantities)
            q = 20;

        for (auto &t : map.tiles)
            t = kFloor;
        for (int d = 0; d < 4; ++d) {
            map.start_count[d] = 1;
            map.starts[d][0] = {5, 9};
        }
    }

    CombatContext context() {
        CombatContext c{game, turn, combat};
        c.enemy_defs = def_ptrs.data();
        c.enemy_def_count = def_ptrs.size();
        c.events = {this, [](void *p, const CombatEvent &e) {
                        auto &self = *static_cast<Arena *>(p);
                        self.events.push_back(e);
                        self.texts.push_back(e.text ? e.text : "");
                    }};
        return c;
    }

    bool said(const char *needle) const {
        for (const auto &t : texts)
            if (t.find(needle) != std::string::npos)
                return true;
        return false;
    }
    void clear_events() {
        events.clear();
        texts.clear();
    }

    // Build the arena through the real production entry point.  `units` are
    // (x, y, sprite) triples handed to initialize_combat's FIXED path -- the
    // same path dungeon_encounter() uses, and the only one that seeds 0xE8
    // family arena fields.
    CombatResult build(CombatContext &c, const std::vector<std::array<int, 3>> &units) {
        uint8_t sprites[16]{};
        map.unit_count = uint8_t(units.size());
        for (size_t i = 0; i < units.size(); ++i) {
            map.units[i] = {int16_t(units[i][0]), int16_t(units[i][1])};
            sprites[i] = uint8_t(units[i][2]);
        }
        FixedCombatSetup fixed{sprites, 0, fields, 16};
        return initialize_combat(c, map, CombatDirection::North, nullptr, 0, true, &fixed);
    }

    CombatActor *find_enemy() {
        for (int i = 0; i < combat.count; ++i)
            if (combat.actors[i].enemy)
                return &combat.actors[i];
        return nullptr;
    }
    int player_index() const {
        for (int i = 0; i < combat.count; ++i)
            if (!combat.actors[i].enemy)
                return i;
        return -1;
    }
    // Hand the turn to the party member.  current() honours an already-set
    // index for an active actor, so this is the caller-owned scheduling the
    // arena's owner performs, not a bypass of the engine.
    void give_turn() { combat.current = player_index(); }
};

// Enemy sprite encoding in the .CBT unit table: 0x40 + 4*index.
constexpr int enemy_sprite(int index) { return 0x40 + 4 * index; }

int field_at(const CombatState &s, int x, int y) {
    for (int i = 0; i < s.field_count; ++i)
        if (s.fields[i].position.x == x && s.fields[i].position.y == y)
            return s.fields[i].tile;
    return -1;
}

// ---------------------------------------------------------------------------
// A -- the location gate.  DS:0x1C90 mask 0x03 for indices 14/15/16/20:
// dungeon and combat ONLY.  Outside those, castSpell never reaches
// cast_field_wall at all: "Not here!" leaves the mixed spell and the mana
// untouched (the gate exits through the epilogue at 0x11d9, BEFORE the 0x0ec8
// decrement and the 0x0ef8 mana charge).
// ---------------------------------------------------------------------------
void test_location_gate() {
    for (int id : {kInFlamGrav, kInNoxGrav, kInZuGrav, kInSanctGrav}) {
        for (int location : {0, 17}) { // overworld, town
            Arena a;
            auto &m = a.game.party.characters[0];
            const int mp = m.current_mp, qty = a.game.spell_quantities[id];
            auto r = cast_spell(a.game, a.turn, m, SpellId(id), {location, false},
                                rng_source(a.game.rng));
            check(!r.ok && !r.consumed, "A: In*Grav outside dungeon/combat is refused");
            check(std::strcmp(r.message, "Not here!") == 0, "A: refusal is Not here!");
            check(a.game.spell_quantities[id] == qty, "A: refused cast keeps the mixed spell");
            check(m.current_mp == mp, "A: refused cast keeps the mana");
        }
        // The two contexts the mask DOES permit reach a real effect descriptor.
        Arena a;
        auto &m = a.game.party.characters[0];
        auto dungeon = cast_spell(a.game, a.turn, m, SpellId(id), {33, false},
                                  rng_source(a.game.rng));
        check(dungeon.ok && dungeon.effect.kind == MagicEffect::Field,
              "A: dungeon context yields the Field descriptor");
        auto fight = cast_spell(a.game, a.turn, m, SpellId(id), {0x80, true},
                                rng_source(a.game.rng));
        check(fight.ok && fight.effect.kind == MagicEffect::Field,
              "A: combat context yields the same Field descriptor");
    }
    // The spell-weapon ids the combat branch loads from DS:0x4592, arg 0..3.
    const int expected[4][2] = {{kInFlamGrav, 53}, {kInNoxGrav, 51}, {kInZuGrav, 52},
                               {kInSanctGrav, 54}};
    for (auto &row : expected) {
        Arena a;
        auto r = cast_spell(a.game, a.turn, a.game.party.characters[0], SpellId(row[0]),
                            {0x80, true}, rng_source(a.game.rng));
        check(r.effect.value == row[1], "A: In*Grav carries its DS:0x4592 spell-weapon id");
    }
}

// ---------------------------------------------------------------------------
// C1/C2/C3 + C7 -- the combat cast.  Production combat_cast() on a live arena
// that ALREADY owns field storage and one authored field, so "no field was
// created" cannot be satisfied by absent storage.
// ---------------------------------------------------------------------------
struct CastOutcome {
    int hp_lost = 0, mana_spent = 0, qty_spent = 0, field_count = 0, field_at_aim = -1;
    bool turn_advanced = false;
};

CastOutcome cast_in_arena(int spell, CombatPoint aim, bool aim_at_enemy) {
    Arena a;
    auto c = a.context();
    // One authored poison field at (1,1): live storage the cast could write to.
    check(a.build(c, {{{1, 1, kFieldPoison}}, {{8, 5, enemy_sprite(1)}}}) == CombatResult::Ok,
          "C: arena builds");
    auto *enemy = a.find_enemy();
    check(enemy != nullptr, "C: enemy present");
    if (!enemy)
        return {};
    enemy->position = {int16_t(aim.x), int16_t(aim.y)};
    if (!aim_at_enemy)
        enemy->position = {8, 8};
    enemy->speed = 0; // hit() -> r30() >= (0 - 30 + 30)/2 == 0: always connects.
    enemy->defense = 0;
    const int hp_before = enemy->hp;
    auto &m = a.game.party.characters[0];
    const int mp_before = m.current_mp, qty_before = a.game.spell_quantities[spell];
    a.give_turn();
    const uint32_t actions_before = a.combat.action_count;
    const auto status = combat_cast(c, SpellId(spell), &aim, 0, false);
    check(status == CombatResult::Ok, "C: combat_cast succeeds");
    CastOutcome out;
    out.hp_lost = hp_before - enemy->hp;
    out.mana_spent = mp_before - m.current_mp;
    out.qty_spent = qty_before - a.game.spell_quantities[spell];
    out.field_count = a.combat.field_count;
    out.field_at_aim = field_at(a.combat, aim.x, aim.y);
    // advance() -> current() re-schedules and bumps action_count; a cast that
    // never finished the actor's turn would leave it untouched.
    out.turn_advanced = a.combat.action_count > actions_before;
    // The authored field is untouched by an unrelated cast.
    check(field_at(a.combat, 1, 1) == kFieldPoison, "C: authored field survives the cast");
    return out;
}

void test_combat_cast_creates_no_field() {
    struct Row {
        int spell;
        int max_damage;
        const char *label;
    } rows[] = {
        {kInFlamGrav, 21, "In Flam Grav"},
        {kInNoxGrav, 18, "In Nox Grav"},
        {kInZuGrav, 0, "In Zu Grav"},
    };
    for (auto &row : rows) {
        // C1/C2/C3: aimed at an occupied cell -- the reference resolves a
        // spell-weapon attack through the ordinary hit/damage pipeline.
        auto hit = cast_in_arena(row.spell, {8, 5}, true);
        check(hit.qty_spent == 1, "C: the mixed spell is consumed");
        check(hit.mana_spent == 3, "C: mana equals the circle");
        check(hit.turn_advanced, "C: the cast finishes the actor's turn");
        check(hit.field_count == 1, "C: combat seeds NO new field (reference SS2.9)");
        check(hit.field_at_aim == -1, "C: the aimed cell holds no field after the cast");
        if (row.max_damage == 0)
            check(hit.hp_lost == 0,
                  "C: In Zu Grav's attackValues entry is 0 -- no field AND no damage");
        else
            check(hit.hp_lost >= 1 && hit.hp_lost <= row.max_damage,
                  "C: damage stays inside the transcribed attackValues bound");

        // C7: aimed at an EMPTY cell -- exactly the hardware gesture that
        // prompted this batch.  The reference outcome is a projectile that
        // lands on nothing: no field, no damage, resources still spent.
        auto empty = cast_in_arena(row.spell, {3, 5}, false);
        check(empty.qty_spent == 1 && empty.mana_spent == 3,
              "C7: an empty-cell cast still spends the spell and mana");
        check(empty.field_count == 1 && empty.field_at_aim == -1,
              "C7: an empty-cell cast leaves no field behind");
        check(empty.hp_lost == 0, "C7: an empty-cell cast damages nobody");
    }
}

// C7 (second half) -- the spell line stops at an opaque tile, so a cast aimed
// past a wall cannot reach the actor behind it.  Same raycast every ranged
// weapon uses; no field-specific placement rule exists to violate.
void test_combat_cast_line_blocked() {
    Arena a;
    a.map.tiles[5 * 11 + 6] = kOpaque;
    auto c = a.context();
    check(a.build(c, {{{8, 5, enemy_sprite(1)}}}) == CombatResult::Ok, "C7b: arena builds");
    auto *enemy = a.find_enemy();
    if (!enemy)
        return;
    enemy->position = {8, 5};
    enemy->speed = 0;
    enemy->defense = 0;
    const int hp = enemy->hp;
    a.combat.actors[a.player_index()].position = {5, 5};
    a.give_turn();
    CombatPoint aim{8, 5};
    check(combat_cast(c, SpellId(kInFlamGrav), &aim, 0, false) == CombatResult::Ok,
          "C7b: cast dispatches");
    check(enemy->hp == hp, "C7b: the spell line stops at the opaque tile");
    check(a.combat.field_count == 0, "C7b: a blocked cast still seeds no field");
}

// ---------------------------------------------------------------------------
// C1' / C6 -- arena fields are REAL state with no duration.  They arrive from
// the .CBT unit table, survive ordinary turns indefinitely (MAGIC.md: "No new
// field duration exists"), and leave only through An Grav or the Sceptre.
// ---------------------------------------------------------------------------
void test_authored_fields_persist() {
    Arena a;
    auto c = a.context();
    check(a.build(c, {{{1, 1, kFieldPoison}},
                      {{2, 1, kFieldSleep}},
                      {{3, 1, kFieldFire}},
                      {{4, 1, kFieldEnergy}},
                      {{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok,
          "C1': arena with four authored fields builds");
    check(a.combat.field_count == 4, "C1': every 0xE8-family unit becomes a field slot");
    check(a.combat.fields != nullptr, "C1': field storage is bound to the arena");
    check(field_at(a.combat, 1, 1) == kFieldPoison && field_at(a.combat, 2, 1) == kFieldSleep &&
              field_at(a.combat, 3, 1) == kFieldFire && field_at(a.combat, 4, 1) == kFieldEnergy,
          "C1': each slot keeps its RAW sprite as its tile");
    check(a.combat.count == 2, "C1': a field unit is not an actor");

    // C6 -- twelve full turns, party pass and enemy step alternating.  Nothing
    // decays: there is no per-field counter anywhere in the reference.
    a.combat.actors[a.player_index()].position = {9, 1};
    for (int i = 0; i < 12; ++i) {
        a.give_turn();
        combat_action(c, CombatAction::Pass);
        combat_action(c, CombatAction::EnemyStep);
    }
    check(a.combat.field_count == 4, "C6: fields do not expire with turns");
    check(field_at(a.combat, 3, 1) == kFieldFire, "C6: the fire field is still in its cell");
}

// ---------------------------------------------------------------------------
// C4 -- render visibility, through the real renderer seam the device calls.
// ---------------------------------------------------------------------------
void test_field_render() {
    Arena a;
    auto c = a.context();
    check(a.build(c, {{{1, 1, kFieldPoison}},
                      {{2, 1, kFieldSleep}},
                      {{3, 1, kFieldFire}},
                      {{4, 1, kFieldEnergy}},
                      {{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok,
          "C4: arena builds");
    const auto scene = compose_combat_presentation(a.combat, a.game);
    check(scene.combat, "C4: the snapshot is an arena snapshot");
    check(scene.tiles[1 * 11 + 1] == kFieldPoison && scene.tiles[1 * 11 + 2] == kFieldSleep &&
              scene.tiles[1 * 11 + 3] == kFieldFire && scene.tiles[1 * 11 + 4] == kFieldEnergy,
          "C4: every field paints its own tile over the terrain");
    check(scene.tiles[1 * 11 + 5] == kFloor, "C4: a cell without a field keeps its terrain");
}

// ---------------------------------------------------------------------------
// C5 -- gameplay effect.  COMBAT:0x1b1e resolves a MAGNITUDE and dispatches:
// 0xE8 -> 0x32 poison, 0xEA -> 0x64 damage, 0xE9 -> 0x96 sleep.  0xEB is
// deliberately absent from that table because COMBAT:0x0000 @00a4 makes it
// BLOCK the cell, so nobody can ever stand on it.
// ---------------------------------------------------------------------------
void test_field_gameplay_effect() {
    struct Row {
        int tile;
        const char *label;
    } rows[] = {{kFieldPoison, "poison"}, {kFieldFire, "damage"}, {kFieldSleep, "sleep"}};
    for (auto &row : rows) {
        Arena a;
        auto c = a.context();
        check(a.build(c, {{{5, 5, row.tile}}, {{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok,
              "C5: arena builds");
        auto &avatar = a.combat.actors[a.player_index()];
        avatar.position = {5, 5};
        const int hp_before = avatar.hp;
        a.game.party.characters[0].status = 'G';
        a.give_turn();
        a.clear_events();
        check(combat_action(c, CombatAction::Pass) == CombatResult::Ok, "C5: pass resolves");
        if (row.tile == kFieldPoison)
            check(a.game.party.characters[0].status == 'P',
                  "C5: standing on 0xE8 poisons at end of turn");
        else if (row.tile == kFieldFire)
            check(avatar.hp < hp_before, "C5: standing on 0xEA costs hit points");
        else
            check(avatar.sleeping, "C5: standing on 0xE9 puts the actor to sleep");
        check(a.combat.field_count == 1, "C5: applying a field does not consume it");
    }
    // 0xEB: blocks, never damages.  The move is refused and the actor stays put.
    Arena a;
    auto c = a.context();
    check(a.build(c, {{{5, 4, kFieldEnergy}}, {{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok,
          "C5b: arena builds");
    auto &avatar = a.combat.actors[a.player_index()];
    avatar.position = {5, 5};
    const int hp_before = avatar.hp;
    a.give_turn();
    a.clear_events();
    combat_action(c, CombatAction::Move, 3); // north
    check(avatar.position.x == 5 && avatar.position.y == 5,
          "C5b: 0xEB blocks the cell (COMBAT:0x0000 @00a4)");
    check(avatar.hp == hp_before, "C5b: 0xEB has no end-of-turn magnitude");
}

// ---------------------------------------------------------------------------
// An Grav in combat (CAST2.OVL:0x07bc) -- the ONE combat spell that touches
// field slots.  It removes exactly one matching slot and reports Success!,
// or Failed! when the aimed cell holds none.
// ---------------------------------------------------------------------------
void test_dispel_field() {
    Arena a;
    auto c = a.context();
    check(a.build(c, {{{1, 1, kFieldPoison}},
                      {{1, 1, kFieldFire}},
                      {{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok,
          "D: stacked-field arena builds");
    check(a.combat.field_count == 2, "D: two slots can share one cell");
    CombatPoint aim{1, 1};
    a.give_turn();
    a.clear_events();
    check(combat_cast(c, SpellId(kAnGrav), &aim, 0, false) == CombatResult::Ok, "D: An Grav casts");
    check(a.combat.field_count == 1, "D: An Grav removes exactly ONE slot");
    check(a.said("Success!"), "D: a removed slot reports Success!");

    aim = {7, 7};
    a.give_turn();
    a.clear_events();
    check(combat_cast(c, SpellId(kAnGrav), &aim, 0, false) == CombatResult::Ok, "D: An Grav casts");
    check(a.combat.field_count == 1, "D: an empty cell removes nothing");
    check(a.said("Failed!"), "D: an empty cell reports Failed!");
}

// ---------------------------------------------------------------------------
// The dungeon branch -- the only place In*Grav creates a field.  ONE cell, the
// one ahead by facing, empty floor only, bit 3 preserved.
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
        m.current_mp = 99;
        m.level = 8;
        for (auto &q : game.spell_quantities)
            q = 20;
        state.active = true;
        state.pos = {33, 0, 4, 4, DungeonFacing::North};
    }

    ActionResult cast(int spell) {
        CommandContext c{game, turn, travel, commands,
                         WorldData{tiles.data(), tiles.data(), tiles.size(), tiles.size()}};
        c.dungeon_context = &dungeon;
        c.events = {this, [](void *p, const GameEvent &e) {
                        if (e.text)
                            static_cast<DungeonHarness *>(p)->texts.push_back(e.text);
                    }};
        Command cmd;
        cmd.kind = CommandKind::Cast;
        cmd.item = int16_t(spell);
        cmd.caster = 0;
        return execute_dungeon_command(c, cmd);
    }
    uint8_t &ahead() { return state.cells[0 * 64 + 3 * 8 + 4]; } // facing North from (4,4)
    bool said(const char *needle) const {
        for (const auto &t : texts)
            if (t.find(needle) != std::string::npos)
                return true;
        return false;
    }
};

void test_dungeon_field_creation() {
    struct Row {
        int spell, tile;
    } rows[] = {{kInFlamGrav, kDungeonFire},
                {kInNoxGrav, kDungeonPoison},
                {kInZuGrav, kDungeonSleep},
                {kInSanctGrav, kDungeonEnergy}};
    for (auto &row : rows) {
        DungeonHarness h;
        const int mp = h.game.party.characters[0].current_mp;
        h.cast(row.spell);
        check(h.ahead() == row.tile, "E: the dungeon branch writes DS:0x4596's tile ahead");
        check(h.game.spell_quantities[row.spell] == 19, "E: the mixed spell is consumed");
        check(h.game.party.characters[0].current_mp < mp, "E: mana is consumed");
        // Only ONE cell: the 5-cell overworld stamp the port once carried was
        // retracted on 2026-08-08 (the binary has no overworld branch at all).
        int written = 0;
        for (auto cell : h.state.cells)
            if (cell)
                ++written;
        check(written == 1, "E: exactly one cell is written");

        // Bit 3 survives (00c6 al=[bp-8]&8 / 00ce or al,[bx+0x4596]).
        DungeonHarness bit3;
        bit3.ahead() = 8;
        bit3.cast(row.spell);
        check(bit3.ahead() == (row.tile | 8), "E: bit 3 of the target cell is preserved");
    }
    // The 0xf7 guard: anything but empty floor refuses.
    DungeonHarness occupied;
    occupied.ahead() = 0x20;
    occupied.cast(kInFlamGrav);
    check(occupied.ahead() == 0x20, "E: a non-empty cell is left alone");
    check(occupied.said("Failed!"), "E: a non-empty cell reports Failed!");
}

// ---------------------------------------------------------------------------
// C8 -- the summons the hardware report confirmed working still execute through
// their own production arms, untouched by anything above.
// ---------------------------------------------------------------------------
void test_summons_unchanged() {
    {
        Arena a;
        auto c = a.context();
        check(a.build(c, {{{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok, "C8: arena builds");
        const int before = a.combat.count;
        a.give_turn();
        check(combat_cast(c, SpellId(kInBetXen), nullptr, 0, false) == CombatResult::Ok,
              "C8: In Bet Xen casts");
        check(a.combat.count > before, "C8: In Bet Xen still summons its swarm");
        int swarms = 0;
        for (int i = before; i < a.combat.count; ++i)
            if (a.combat.actors[i].enemy && a.combat.actors[i].enemy->index == 31)
                ++swarms;
        check(swarms == a.combat.count - before && swarms >= 1 && swarms <= 4,
              "C8: swarm members are enemy index 31, at most four per cast");
        check(a.combat.field_count == 0, "C8: a summon creates no field");
    }
    {
        Arena a;
        auto c = a.context();
        check(a.build(c, {{{9, 9, enemy_sprite(1)}}}) == CombatResult::Ok, "C8: arena builds");
        const int before = a.combat.count;
        a.give_turn();
        check(combat_cast(c, SpellId(kKalXenCorp), nullptr, 0, false) == CombatResult::Ok,
              "C8: Kal Xen Corp casts");
        check(a.combat.count == before + 1, "C8: Kal Xen Corp still gates in one daemon");
        check(a.combat.actors[before].enemy && a.combat.actors[before].enemy->index == 38,
              "C8: the gated actor is enemy index 38");
    }
}

} // namespace

int main() {
    test_location_gate();
    test_combat_cast_creates_no_field();
    test_combat_cast_line_blocked();
    test_authored_fields_persist();
    test_field_render();
    test_field_gameplay_effect();
    test_dispel_field();
    test_dungeon_field_creation();
    test_summons_unchanged();
    if (failures) {
        std::cerr << "batch12: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "Batch 12 combat field magic: reference-faithful (no combat field creation, "
                 "dungeon branch seeds one cell, arena fields persist/render/apply)\n";
    return 0;
}
