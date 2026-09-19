// Batch 3 GROUP C -- RED tests for R-06 (Ready rejected in combat and in
// dungeons while the UI offers it identically everywhere). See
// native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
//
// These tests drive the REAL command()/execute_command() routing path in
// native/core/src/commands.cpp -- never a direct equip_item() call standing
// in for routing -- except where the test explicitly says it is
// deliberately bypassing routing to isolate a second question (C4's cache
// question), matching the spec's stated fallback.
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/inventory.h"
#include "openu5/outdoor.h"
#include "openu5/world_commands.h"
#include "openu5/world_terrain.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "[FAIL] batch3 group C check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch3 group C check " << checks << ": " << what << "\n";
    }
}
Rand rng_from(GameState &g) {
    return {&g, [](void *p, int32_t lo, int32_t hi) { return static_cast<GameState *>(p)->rng.next(lo, hi).value; }};
}
} // namespace

// C1: GREEN guard -- world Ready is unchanged. native/core/tests/item_parity_test.cpp
// (fixtures/items.txt, op==0) already drives CommandKind::Ready through
// execute_command() in plain world context and is GREEN; this is a small,
// non-duplicating sanity check using the same routing path so this file is
// self-contained.
static void c1_world_ready_green_guard() {
    GameState game{};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.status = 'G';
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.weapon = avatar.shield = avatar.ring = avatar.amulet = 255;
    game.equipment_quantities[17] = 1; // Dagger, owned, unequipped.
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    static uint8_t large[65536]{};
    std::fill_n(large, sizeof(large), uint8_t(5));
    WorldData world{large, large, sizeof(large), sizeof(large)};
    CommandContext ctx{game, turn, travel, commands, world};
    Command ready;
    ready.kind = CommandKind::Ready;
    ready.member = 0;
    ready.item = 17;
    auto result = execute_command(ctx, ready);
    check(result.status == CommandStatus::Success && avatar.weapon == 17,
          "C1 GREEN guard: world Ready still equips normally");
    std::cout << "C1 (world Ready, GREEN guard) executed\n";
}

// C2: RED -- dungeon active, NOT combat. Build a real dungeon session via the
// production CommandKind::EnterDungeon path (same construction as
// native/core/tests/world_flow_adapter_test.cpp), then attempt Ready through
// execute_command(). Desired (reference): dungeon-corridor Ready behaves like
// world Ready -- a free action, no combat-turn semantics. Actual: rejected
// outright by the c.dungeon && !dungeon_camp blanket gate in commands.cpp.
static void c2_dungeon_ready_red() {
    GameState game{};
    game.position.xy = {20, 20};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.status = 'G';
    avatar.current_hp = avatar.max_hp = 100;
    avatar.dexterity = 20;
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.weapon = avatar.shield = avatar.ring = avatar.amulet = 255;
    game.equipment_quantities[17] = 1; // Dagger, owned, unequipped.
    std::vector<uint8_t> terrain(65536, 5);
    WorldData world;
    world.overworld = terrain.data();
    world.overworld_size = terrain.size();
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    CommandContext ctx{game, turn, travel, commands, world};

    DungeonState d;
    DungeonScratch scratch;
    DungeonData data;
    data.location = 33;
    data.cells[9] = 0x10;
    DungeonContext dc{d, scratch};
    dc.data = &data;
    dc.count = 1;
    ctx.dungeon_context = &dc;
    Command enter;
    enter.kind = CommandKind::EnterDungeon;
    enter.member = 33;
    auto entered = execute_command(ctx, enter);
    check(entered.status == CommandStatus::Success && d.active, "C2 setup: production EnterDungeon succeeds");

    ctx.dungeon = true; // Mirrors how AlphaRuntime/UiSession track context_.dungeon.
    const auto turns_before = game.turns_since_start;
    Command ready;
    ready.kind = CommandKind::Ready;
    ready.member = 0;
    ready.item = 17;
    auto result = execute_command(ctx, ready);
    const bool desired = result.status == CommandStatus::Success && avatar.weapon == 17 &&
                         game.turns_since_start == turns_before;
    check(desired,
          "C2: expected dungeon-corridor Ready (dungeon active, not in an active arena) to succeed "
          "as a free action like world Ready -- RED expected against baseline");
    if (!desired) {
        std::cerr << "  C2 detail: status=" << int(result.status) << " weapon=" << int(avatar.weapon)
                  << " (RED expected: current code rejects with InvalidContext="
                  << int(CommandStatus::InvalidContext) << ")\n";
    }
    std::cout << "C2 (dungeon Ready) executed\n";
}

// C3: RED -- a real combat arena via the production TrollToll entry path
// (same construction as native/core/tests/direct_troll_handoff_test.cpp),
// then Ready through execute_command(). Desired (reference): combat Ready
// succeeds and charges a turn/action. Actual: rejected outright by c.combat
// in the same blanket gate. This function ONLY covers the ROUTING question;
// the SEPARATE CombatActor cache-staleness question (whether cached
// weapon/attack fields resync after equipment changes) is isolated
// independently from routing in c4_combat_actor_cache_staleness_isolated()
// below, per the batch correction: C3's routing rejection means the cache
// question is never reached through the real path here, so it cannot serve
// as evidence either way about the cache defect -- only about routing.
static void c3_combat_ready_red() {
    constexpr int player_x = 101, player_y = 102, bridge_x = 101, bridge_y = 102, bridge_tile = 106;
    std::vector<uint8_t> tiles(256 * 256, 5);
    tiles[bridge_y * 256 + bridge_x] = bridge_tile;
    WorldData world{tiles.data(), tiles.data(), tiles.size(), tiles.size()};
    GameState game{};
    game.position = {{uint8_t(player_x), uint8_t(player_y)}, {0, 0}};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.party_status = 0;
    avatar.status = 'G';
    avatar.current_hp = avatar.max_hp = 100;
    avatar.dexterity = 20;
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.shield = avatar.ring = avatar.amulet = 255;
    avatar.weapon = 17; // Dagger, equipped.
    game.equipment_quantities[17] = 1;
    game.equipment_quantities[31] = 1; // Long Sword, owned, unequipped -- a different weapon to Ready.
    game.rng.seed(0x2600);
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    CommandContext context{game, turn, travel, commands, world};

    struct Objects {
        std::vector<QuestObject> values;
    };
    Objects objects{};
    QuestWorldServices quest{};
    quest.context = &objects;
    quest.count = [](void *p) { return static_cast<Objects *>(p)->values.size(); };
    quest.read = [](void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; };
    quest.reserve = [](void *p, size_t n) { static_cast<Objects *>(p)->values.reserve(n); return true; };
    quest.append = [](void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); };
    quest.erase = [](void *p, size_t i) { auto &v = static_cast<Objects *>(p)->values; v.erase(v.begin() + ptrdiff_t(i)); };
    quest.write = [](void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; };
    context.quest_world = &quest;
    WorldTerrain terrain{};
    context.terrain = &terrain;
    terrain.refresh(world, game);

    CombatMap arena{};
    for (auto &tile : arena.tiles) tile = 5;
    arena.start_count[int(CombatDirection::South)] = 1;
    arena.starts[int(CombatDirection::South)][0] = {5, 6};
    arena.unit_count = 1;
    arena.units[0] = {6, 4};
    CombatEnemy troll{};
    troll.index = 41;
    troll.name = "Troll";
    troll.hp = 10;
    troll.max_per_map = 1;
    troll.tile = 400;
    const CombatMap *maps[] = {&arena};
    const CombatEnemy *enemies[42]{};
    enemies[41] = &troll;
    CombatState battle{};
    CombatContext battle_owner{game, turn, battle};
    CombatResources resources{maps, 1, enemies, 42, {}};
    OutdoorServices outdoor{};
    outdoor.combat = &battle_owner;
    outdoor.resources = &resources;
    context.outdoor = &outdoor;

    commands.awaiting_troll = true;
    commands.troll_toll = 99;
    commands.troll_x = bridge_x;
    commands.troll_y = bridge_y;
    Command refuse{};
    refuse.kind = CommandKind::TrollToll;
    refuse.member = 0;
    auto entered = execute_command(context, refuse);
    check(entered.status == CommandStatus::Success && context.combat && battle.initialized,
          "C3 setup: production TrollToll refusal enters a real combat arena");

    int player_actor = -1;
    for (int i = 0; i < battle.count; ++i)
        if (!battle.actors[i].enemy) player_actor = i;
    check(player_actor >= 0, "C3 setup: a live player CombatActor exists in the arena");

    // C3: attempt Ready through the real routing path while c.combat is true.
    Command ready;
    ready.kind = CommandKind::Ready;
    ready.member = 0;
    ready.item = 31; // Long Sword.
    auto result = execute_command(context, ready);
    const bool c3_desired = result.status == CommandStatus::Success && avatar.weapon == 31;
    check(c3_desired,
          "C3: expected combat Ready to succeed and re-equip through the real routing path -- "
          "RED expected against baseline");
    if (!c3_desired)
        std::cerr << "  C3 detail: status=" << int(result.status)
                  << " (RED expected: InvalidContext=" << int(CommandStatus::InvalidContext) << ")."
                     " GameState equipment unchanged: avatar.weapon=" << int(avatar.weapon)
                  << " (still 17). This routing rejection is the ONLY thing C3 demonstrates -- it does"
                     " not by itself prove or disprove anything about CombatActor cache staleness; see"
                     " c4_combat_actor_cache_staleness_isolated() below for that separate question.\n";
    std::cout << "C3 (combat Ready routing) executed\n";
}

// C4 -- revised per the Batch 3 RED-test adjudication pass. The prior shape
// of this test PRESCRIBED an architecture (a direct standalone equip_item()
// call must itself mutate CombatActor's cache) that the audit does not
// actually require: a correct fix may legitimately route Ready ->
// equip_item() -> an EXPLICIT CombatActor refresh/resync step, in which case
// equip_item() itself stays entirely unaware of CombatState, and the old
// C4's RED assertion would never turn GREEN even under a correct fix. C4 is
// therefore now TWO functions:
//
//   1. c4_ready_equipment_cache_invariant_red() -- the actual required
//      invariant, stated without prescribing who owns the resync: "After a
//      successful CommandKind::Ready while in combat, authoritative
//      GameState equipment and the active player's cached CombatActor
//      equipment-derived state agree." Exercised through the REAL
//      command()/execute_command() routing path, same construction as C3.
//      On today's baseline this fails first at ROUTING (Ready is rejected
//      outright while c.combat is true, exactly like C3), so the cache
//      question is never reached here either -- that is expected and
//      acceptable; this function is the one whose GREEN condition the
//      eventual fix must satisfy end-to-end.
//
//   2. c4_characterization_direct_equip_cache_staleness() -- the prior
//      direct-equip() investigation, KEPT as independent evidence of the
//      underlying cache defect, but explicitly labeled CHARACTERIZATION
//      (documents CURRENT behavior as a GREEN guard, like C5 below) rather
//      than a must-turn-GREEN regression test. It intentionally bypasses
//      Ready/command() routing, so it says nothing about who should own the
//      eventual resync -- only that no resync seam exists anywhere today.
static void c4_ready_equipment_cache_invariant_red() {
    constexpr int player_x = 101, player_y = 102, bridge_x = 101, bridge_y = 102, bridge_tile = 106;
    std::vector<uint8_t> tiles(256 * 256, 5);
    tiles[bridge_y * 256 + bridge_x] = bridge_tile;
    WorldData world{tiles.data(), tiles.data(), tiles.size(), tiles.size()};
    GameState game{};
    game.position = {{uint8_t(player_x), uint8_t(player_y)}, {0, 0}};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.party_status = 0;
    avatar.status = 'G';
    avatar.current_hp = avatar.max_hp = 100;
    avatar.dexterity = 20;
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.shield = avatar.ring = avatar.amulet = 255;
    avatar.weapon = 255; // No weapon equipped yet (equipment A).
    game.equipment_quantities[31] = 1; // Long Sword, owned, unequipped -- equipment B to Ready into.
    game.rng.seed(0x2600);
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    CommandContext context{game, turn, travel, commands, world};

    struct Objects {
        std::vector<QuestObject> values;
    };
    Objects objects{};
    QuestWorldServices quest{};
    quest.context = &objects;
    quest.count = [](void *p) { return static_cast<Objects *>(p)->values.size(); };
    quest.read = [](void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; };
    quest.reserve = [](void *p, size_t n) { static_cast<Objects *>(p)->values.reserve(n); return true; };
    quest.append = [](void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); };
    quest.erase = [](void *p, size_t i) { auto &v = static_cast<Objects *>(p)->values; v.erase(v.begin() + ptrdiff_t(i)); };
    quest.write = [](void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; };
    context.quest_world = &quest;
    WorldTerrain terrain{};
    context.terrain = &terrain;
    terrain.refresh(world, game);

    CombatMap arena{};
    for (auto &tile : arena.tiles) tile = 5;
    arena.start_count[int(CombatDirection::South)] = 1;
    arena.starts[int(CombatDirection::South)][0] = {5, 6};
    arena.unit_count = 1;
    arena.units[0] = {6, 4};
    CombatEnemy troll{};
    troll.index = 41;
    troll.name = "Troll";
    troll.hp = 10;
    troll.max_per_map = 1;
    troll.tile = 400;
    const CombatMap *maps[] = {&arena};
    const CombatEnemy *enemies[42]{};
    enemies[41] = &troll;
    CombatState battle{};
    CombatContext battle_owner{game, turn, battle};
    CombatResources resources{maps, 1, enemies, 42, {}};
    OutdoorServices outdoor{};
    outdoor.combat = &battle_owner;
    outdoor.resources = &resources;
    context.outdoor = &outdoor;

    commands.awaiting_troll = true;
    commands.troll_toll = 99;
    commands.troll_x = bridge_x;
    commands.troll_y = bridge_y;
    Command refuse{};
    refuse.kind = CommandKind::TrollToll;
    refuse.member = 0;
    auto entered = execute_command(context, refuse);
    check(entered.status == CommandStatus::Success && context.combat && battle.initialized,
          "C4 setup: production TrollToll refusal enters a real combat arena");

    int player_actor = -1;
    for (int i = 0; i < battle.count; ++i)
        if (!battle.actors[i].enemy) player_actor = i;
    check(player_actor >= 0, "C4 setup: a live player CombatActor exists in the arena");
    check(avatar.weapon == 255, "C4 setup: authoritative equipment A is confirmed as 'no weapon equipped'");

    // C4: attempt Ready through the real routing path while c.combat is true,
    // then check the INVARIANT -- not any particular resync mechanism.
    Command ready;
    ready.kind = CommandKind::Ready;
    ready.member = 0;
    ready.item = 31; // Long Sword.
    auto result = execute_command(context, ready);
    const auto &actor = battle.actors[player_actor >= 0 ? player_actor : 0];
    const int32_t cached_weapon_id = actor.weapon_count ? actor.weapons[0].id : -1;
    const bool invariant_holds =
        result.status == CommandStatus::Success && avatar.weapon == 31 && cached_weapon_id == 31;
    check(invariant_holds,
          "C4: expected that after a successful combat Ready, authoritative GameState equipment "
          "(avatar.weapon) and the active player's cached CombatActor equipment-derived state "
          "agree -- RED expected against baseline: Ready routing itself is rejected while "
          "c.combat is true, exactly like C3, so the invariant cannot yet be reached, let alone "
          "hold. This test does NOT prescribe whether the eventual fix keeps equip_item() "
          "CombatState-unaware and resyncs the cache elsewhere, or has equip_item() itself notify "
          "combat -- either design satisfies this invariant.");
    if (!invariant_holds)
        std::cerr << "  C4 detail: status=" << int(result.status) << " avatar.weapon=" << int(avatar.weapon)
                  << " cached weapons[0].id=" << cached_weapon_id
                  << " (RED expected: InvalidContext=" << int(CommandStatus::InvalidContext) << ")\n";
    std::cout << "C4 (Ready equipment/cache invariant, routing-exercised) executed\n";
}

// C4 CHARACTERIZATION (not a must-turn-GREEN regression): independent
// evidence of the underlying cache defect, isolated from the routing
// question above. This builds a real CombatActor via the SAME production
// TrollToll entry path as C3, records its cached fields for starting
// equipment A (no weapon equipped), then mutates the AUTHORITATIVE
// equipment by calling equip_item() DIRECTLY (native/core/src/inventory.cpp)
// -- deliberately bypassing Ready/command() routing, exactly as the spec's
// stated fallback allows, to isolate the cache question from the routing
// question C2/C3/c4_ready_equipment_cache_invariant_red already cover. It
// only calls real production functions (equip_item() and nothing else) and
// never reassigns CombatActor's fields by hand; grep of
// combat.cpp/combat.h/inventory.cpp during this pass found no function
// anywhere that writes to CombatActor::weapons[]/attack/defense/range other
// than the one-time population in combat.cpp's arena-construction code
// (~1120-1150), executed once by TrollToll's arena entry and never again --
// there is no resync seam at all today. This is asserted as a GREEN
// characterization of that CURRENT state (like C5 below), not as a RED
// requirement that a direct equip_item() call itself must resync the cache
// -- see the header comment above for why that would be the wrong contract.
//
// Note on starting equipment: this host fixture (like the pre-existing
// direct_troll_handoff_test.cpp/C3 above) does not wire a CombatContext::
// tables attack/range table (those are supplied at runtime from loaded game
// assets by AlphaRuntime, not available to a host unit test), so
// combat.cpp's per-weapon table() lookups all resolve to 0 regardless of
// which real item is equipped at arena-construction time, and every actor
// falls back to the CombatWeapon{} sentinel (id=255, attack=1, range=1).
// That is an orthogonal fixture limitation, not the resync question: it
// affects what the STARTING cached values look like, not whether they
// change. Equipment A is therefore deliberately "no weapon equipped" (so
// the authoritative starting state has a real, unambiguous value: none),
// and the test's actual claim is the invariance one -- the cached actor
// fields are observed to be BYTE-FOR-BYTE IDENTICAL before and after
// equip_item() changes the authoritative equipment from A (none) to B (Long
// Sword, id 31), because nothing in production touches them again after
// arena construction.
static void c4_characterization_direct_equip_cache_staleness() {
    constexpr int player_x = 101, player_y = 102, bridge_x = 101, bridge_y = 102, bridge_tile = 106;
    std::vector<uint8_t> tiles(256 * 256, 5);
    tiles[bridge_y * 256 + bridge_x] = bridge_tile;
    WorldData world{tiles.data(), tiles.data(), tiles.size(), tiles.size()};
    GameState game{};
    game.position = {{uint8_t(player_x), uint8_t(player_y)}, {0, 0}};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.party_status = 0;
    avatar.status = 'G';
    avatar.current_hp = avatar.max_hp = 100;
    avatar.dexterity = 20;
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.weapon = avatar.shield = avatar.ring = avatar.amulet = 255;
    // Starting equipment A: no weapon equipped (see the note above on why).
    game.equipment_quantities[31] = 1; // Long Sword, owned, unequipped -- equipment B to equip into.
    game.rng.seed(0x2600);
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    CommandContext context{game, turn, travel, commands, world};

    struct Objects {
        std::vector<QuestObject> values;
    };
    Objects objects{};
    QuestWorldServices quest{};
    quest.context = &objects;
    quest.count = [](void *p) { return static_cast<Objects *>(p)->values.size(); };
    quest.read = [](void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; };
    quest.reserve = [](void *p, size_t n) { static_cast<Objects *>(p)->values.reserve(n); return true; };
    quest.append = [](void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); };
    quest.erase = [](void *p, size_t i) { auto &v = static_cast<Objects *>(p)->values; v.erase(v.begin() + ptrdiff_t(i)); };
    quest.write = [](void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; };
    context.quest_world = &quest;
    WorldTerrain terrain{};
    context.terrain = &terrain;
    terrain.refresh(world, game);

    CombatMap arena{};
    for (auto &tile : arena.tiles) tile = 5;
    arena.start_count[int(CombatDirection::South)] = 1;
    arena.starts[int(CombatDirection::South)][0] = {5, 6};
    arena.unit_count = 1;
    arena.units[0] = {6, 4};
    CombatEnemy troll{};
    troll.index = 41;
    troll.name = "Troll";
    troll.hp = 10;
    troll.max_per_map = 1;
    troll.tile = 400;
    const CombatMap *maps[] = {&arena};
    const CombatEnemy *enemies[42]{};
    enemies[41] = &troll;
    CombatState battle{};
    CombatContext battle_owner{game, turn, battle};
    CombatResources resources{maps, 1, enemies, 42, {}};
    OutdoorServices outdoor{};
    outdoor.combat = &battle_owner;
    outdoor.resources = &resources;
    context.outdoor = &outdoor;

    commands.awaiting_troll = true;
    commands.troll_toll = 99;
    commands.troll_x = bridge_x;
    commands.troll_y = bridge_y;
    Command refuse{};
    refuse.kind = CommandKind::TrollToll;
    refuse.member = 0;
    auto entered = execute_command(context, refuse);
    check(entered.status == CommandStatus::Success && context.combat && battle.initialized,
          "C4 setup: production TrollToll refusal enters a real combat arena");

    int player_actor = -1;
    for (int i = 0; i < battle.count; ++i)
        if (!battle.actors[i].enemy) player_actor = i;
    check(player_actor >= 0, "C4 setup: a live player CombatActor exists in the arena");

    const auto &actor_before = battle.actors[player_actor];
    const int32_t weapon_count_before = actor_before.weapon_count;
    const int32_t weapon_id_before = weapon_count_before ? actor_before.weapons[0].id : -1;
    const int32_t weapon_attack_before = weapon_count_before ? actor_before.weapons[0].attack : -1;
    const int32_t attack_before = actor_before.attack;
    const int32_t range_before = actor_before.range;
    const int32_t defense_before = actor_before.defense;
    check(avatar.weapon == 255,
          "C4 setup: authoritative equipment A is confirmed as 'no weapon equipped'");

    // Directly mutate the AUTHORITATIVE equipment via the real equip_item()
    // (bypassing Ready/command() routing on purpose -- C2/C3 already cover
    // that routing question; this isolates the cache-sync question).
    auto rand = rng_from(game);
    auto equip_result = equip_item(game, 0, 31, &rand, /*battle=*/false);
    check(equip_result.ok && avatar.weapon == 31,
          "C4 setup: direct equip_item() call succeeds and updates the AUTHORITATIVE GameState "
          "(avatar.weapon) to equipment B (Long Sword, id 31)");

    const auto &actor_after = battle.actors[player_actor];
    const int32_t weapon_count_after = actor_after.weapon_count;
    const int32_t weapon_id_after = weapon_count_after ? actor_after.weapons[0].id : -1;
    const int32_t weapon_attack_after = weapon_count_after ? actor_after.weapons[0].attack : -1;
    const int32_t attack_after = actor_after.attack;
    const int32_t range_after = actor_after.range;
    const int32_t defense_after = actor_after.defense;

    // CHARACTERIZATION, not a RED requirement: this documents CURRENT
    // behavior (no resync seam exists today, so the cache is unchanged) as a
    // GREEN guard, the same style as C5 below. It intentionally does NOT
    // assert that a direct equip_item() call must itself resync the cache --
    // that would prescribe an architecture the audit does not require (see
    // the C4 header comment above). Once a real fix lands (via whichever
    // design it chooses), this characterization is expected to need updating
    // -- that is fine; it is not gating anything today.
    const bool cache_resynced = weapon_count_after != weapon_count_before || weapon_id_after != weapon_id_before ||
                                weapon_attack_after != weapon_attack_before || attack_after != attack_before ||
                                range_after != range_before;
    check(!cache_resynced,
          "C4 CHARACTERIZATION: CombatActor's cached weapon_count/weapons[]/attack/range remain "
          "byte-for-byte unchanged after a direct equip_item() call changes the authoritative "
          "GameState (id 31, Long Sword) -- documents that no resync seam exists today; this is "
          "evidence for the defect, not a prescription for how the eventual fix must resync it");
    std::cerr << "  C4 detail: authoritative avatar.weapon before=255 after=" << int(avatar.weapon)
              << " (real change). CombatActor.weapon_count before=" << weapon_count_before
              << " after=" << weapon_count_after << "; weapons[0].id before=" << weapon_id_before
              << " after=" << weapon_id_after << "; weapons[0].attack before=" << weapon_attack_before
              << " after=" << weapon_attack_after << "; .attack before=" << attack_before
              << " after=" << attack_after << "; .range before=" << range_before
              << " after=" << range_after << "; .defense before=" << defense_before
              << " after=" << defense_after
              << (cache_resynced ? " -- cache WAS resynced (unexpected)\n"
                                  : " -- cache is STALE: every cached field is byte-for-byte identical "
                                    "before and after despite the authoritative equipment genuinely "
                                    "changing from 'none' to id 31\n");
    std::cout << "C4 (characterization: direct-equip cache staleness, GREEN) executed\n";
}

// C5: armour-lock semantics in battle. Reused/characterized rather than
// duplicated: native/core/tests/item_parity_test.cpp (fixtures/items.txt,
// op==1) already drives equip_item(..., battle=true) directly for armour ids
// 9-15 and is GREEN. This is a small, self-contained characterization of the
// same rule via the same production function, for readability of this batch.
static void c5_armour_lock_characterization() {
    // The battle armour lock in inventory.cpp::equip_item covers ids 9-15
    // (Jewel Shield through Plate Mail body armour), not helmets. Use id 10
    // (Cloth Armour) -> 11 (Leather Armour) to land in that range.
    GameState game{};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.status = 'G';
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.weapon = avatar.shield = avatar.ring = avatar.amulet = 255;
    game.equipment_quantities[11] = 1; // Leather Armour, owned, unequipped, nothing worn yet.
    auto rand = rng_from(game);
    auto result = equip_item(game, 0, 11, &rand, /*battle=*/true);
    check(!result.ok && avatar.armor == 255,
          "C5 characterization: equip_item(battle=true) still refuses to equip armour "
          "(reused rule, see item_parity_test.cpp op==1)");
    std::cout << "C5 (armour lock characterization, GREEN) executed\n";
}

// C6: dungeon Ready must NOT hit the combat-only armour restriction and must
// NOT consume a combat turn -- i.e. for equip_item's purposes, dungeon !=
// battle. This isolates the underlying equip_item() rule (already correct)
// from the routing defect covered by C2: once C2's gate is fixed with
// `battle = c.combat` as the audit's fix shape specifies, a dungeon Ready
// (c.combat == false) must pass battle=false and therefore must NOT be
// blocked from changing armour.
static void c6_dungeon_not_battle_for_equip() {
    GameState game{};
    game.party.party_size = game.party.character_count = 1;
    auto &avatar = game.party.characters[0];
    avatar.status = 'G';
    avatar.strength = 50;
    avatar.helmet = avatar.armor = avatar.weapon = avatar.shield = avatar.ring = avatar.amulet = 255;
    game.equipment_quantities[11] = 1; // Leather Armour, owned, unequipped, nothing worn yet.
    auto rand = rng_from(game);
    // battle=false is what a dungeon (non-combat) Ready must pass, per the
    // fix shape "battle = c.combat". This call bypasses routing on purpose,
    // to isolate the equip_item rule from the C2 routing gate.
    auto result = equip_item(game, 0, 11, &rand, /*battle=*/false);
    check(result.ok && avatar.armor == 11,
          "C6 GREEN guard: equip_item(battle=false) -- the value a fixed dungeon Ready would pass "
          "-- allows changing armour, confirming dungeon != battle for equip_item's own rule");
    std::cout << "C6 (dungeon != battle for equip_item, GREEN guard) executed\n";
}

int main() {
    c1_world_ready_green_guard();
    c2_dungeon_ready_red();
    c3_combat_ready_red();
    c4_ready_equipment_cache_invariant_red();
    c4_characterization_direct_equip_cache_staleness();
    c5_armour_lock_characterization();
    c6_dungeon_not_battle_for_equip();
    std::cout << checks << " batch3 group C checks executed, " << failures << " failed\n";
    return failures > 0 ? 1 : 0;
}
