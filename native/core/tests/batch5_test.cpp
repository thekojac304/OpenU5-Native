// Batch 5 -- world spell targeting (audit R-11) and the Rel Hur scroll getdir
// (audit Y-21).  See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
//
// Three seams are exercised, all of them production code:
//
//  A. openu5::cast_target_prompt() -- the ESP-free extraction of the decision
//     AlphaRuntime::cast_selected_spell makes about which prompt a (C)ast owes
//     the player.  cast_selected_spell itself lives behind ESP-IDF headers
//     (audit Y-05); the Batch 3 precedent (openu5::usable_item_picker_rows) is
//     to extract the decision so the host suite and the device share it.
//
//  B. The real UiSession TargetSelection machinery -- driven exactly the way
//     AlphaRuntime drives it (begin_target + handle_input), with a spy
//     dispatcher, as in ui_session_test.cpp and batch3_group_a_test.cpp.
//
//  C. The real openu5::world_magic() -- the charge/MP consumption ordering and
//     the direction-consuming effects themselves.
//
// REFERENCE SEMANTICS the ordering assertions are pinned to (game/src/main.ts,
// the `doCast` world branch and the pendingCast* consumers at the top of the
// key handler):
//
//   castSpell() runs FIRST and consumes the charge and the mana; ONLY THEN are
//   pendingCastDoor / pendingCastUnlock / pendingCastBlink armed and the getdir
//   entered.  Cancelling the getdir prints "Cancelled." (seal/unlock) or
//   nothing at all (blink) and does NOT refund.  That is bug-for-bug original
//   behaviour and these tests hold it in place: a cancelled world cast must
//   still spend the charge.
//
//   Exactly three world casts take a direction in the reference: An Ex Por
//   (sealDoor), An Sanct (disarmOrOpen) and In Por (blink).  An Ylem, An Grav
//   and In Ex Por have no world effect branch at all -- they consume and do
//   nothing, silently, without a prompt.  world_magic.cpp agrees independently:
//   its only pre-flight target guards name items 6, 25 and 17.
#include "openu5/magic.h"
#include "openu5/outdoor.h"
#include "openu5/quest_world.h"
#include "openu5/ui_session.h"
#include "openu5/world_commands.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool v, const char *what) {
    ++checks;
    if (!v) {
        ++failures;
        std::cerr << "[FAIL] batch5 check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch5 check " << checks << ": " << what << "\n";
    }
}
struct Spy {
    std::vector<UiIntent> intents;
    static void send(void *p, const UiIntent &i) { static_cast<Spy *>(p)->intents.push_back(i); }
};
UiAction action(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction dir(Direction d) { UiAction a; a.kind = UiActionKind::Direction; a.direction = d; return a; }

struct Seen { GameEventKind kind{}; std::string text; };
void capture(void *context, const GameEvent &e) {
    static_cast<std::vector<Seen> *>(context)->push_back({e.kind, e.text ? e.text : ""});
}
bool said(const std::vector<Seen> &events, const char *text) {
    for (const auto &e : events)
        if (e.kind == GameEventKind::Message && e.text == text) return true;
    return false;
}
bool emitted(const std::vector<Seen> &events, GameEventKind kind) {
    for (const auto &e : events)
        if (e.kind == kind) return true;
    return false;
}

// One town-sized world with a live QuestWorldServices whose volatile_tile
// writes straight back into the tile vector, so the Seal/Disarm branches of
// world_magic have the mutable terrain owner they demand.
struct TownWorld {
    std::vector<uint8_t> tiles = std::vector<uint8_t>(32 * 32, 5);
    MapData map_data{{1, 0}, tiles.data(), tiles.size()};
    WorldData world{};
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    QuestWorldServices quest{};
    OutdoorServices outdoor{};
    TownWorld() {
        map_data.tiles = tiles.data();
        world.small_maps = &map_data;
        world.small_map_count = 1;
        game.position = {{4, 4}, {1, 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        m.party_status = 0; m.status = 'G';
        m.current_hp = m.max_hp = 100; m.current_mp = 50; m.level = 8; m.intelligence = 30;
        quest.context = this;
        quest.volatile_tile = [](void *c, int32_t x, int32_t y, int32_t tile) {
            auto &self = *static_cast<TownWorld *>(c);
            if (x >= 0 && x < 32 && y >= 0 && y < 32) self.tiles[size_t(y) * 32 + size_t(x)] = uint8_t(tile);
        };
    }
};
} // namespace

// ---------------------------------------------------------------------------
// A. The targeting policy seam (R-11).
// ---------------------------------------------------------------------------
static void policy_tests() {
    // A1 RED -- outside combat, the three direction-consuming world casts must
    // ask for a direction.  Today cast_selected_spell gates the prompt on
    // context_.combat, so the world answer is None and the spell dispatches
    // with has_direction=false: charge and mana gone, no effect (R-11).
    check(cast_target_prompt(SpellId::AnSanct, false, false) == CastTargetPrompt::WorldDirection,
          "A1: An Sanct in the world must raise a direction prompt (RED: combat-only gating)");
    check(cast_target_prompt(SpellId::InPor, false, false) == CastTargetPrompt::WorldDirection,
          "A1: In Por in the world must raise a direction prompt (RED: combat-only gating)");
    check(cast_target_prompt(SpellId::AnExPor, false, false) == CastTargetPrompt::WorldDirection,
          "A1: An Ex Por in the world must raise a direction prompt (RED: combat-only gating)");

    // A2 GREEN guard -- the spells whose target_type mentions a map unit or a
    // map position but which have NO world effect must NOT grow a fabricated
    // prompt.  The reference's doCast has no branch for An Ylem (Poof),
    // An Grav (Dispel) or In Ex Por (Animation): they consume and do nothing.
    check(cast_target_prompt(SpellId::AnYlem, false, false) == CastTargetPrompt::None,
          "A2 guard: An Ylem has no world effect and must not prompt");
    check(cast_target_prompt(SpellId::AnGrav, false, false) == CastTargetPrompt::None,
          "A2 guard: An Grav has no world effect and must not prompt");
    check(cast_target_prompt(SpellId::InExPor, false, false) == CastTargetPrompt::None,
          "A2 guard: In Ex Por has no world effect and must not prompt");
    check(cast_target_prompt(SpellId::InLor, false, false) == CastTargetPrompt::None,
          "A2 guard: a targetless spell must not prompt in the world");

    // A3 GREEN guard -- combat is unchanged, exactly as today: every spell
    // whose target_type names a map position / map unit / direction opens the
    // combat aim reticle, and nothing else does.
    check(cast_target_prompt(SpellId::VasFlam, true, false) == CastTargetPrompt::CombatReticle,
          "A3 guard: Vas Flam still opens the combat reticle");
    check(cast_target_prompt(SpellId::AnSanct, true, false) == CastTargetPrompt::CombatReticle,
          "A3 guard: An Sanct still opens the combat reticle");
    check(cast_target_prompt(SpellId::GravPor, true, false) == CastTargetPrompt::CombatReticle,
          "A3 guard: a direction spell still opens the combat reticle");
    check(cast_target_prompt(SpellId::InLor, true, false) == CastTargetPrompt::None,
          "A3 guard: a targetless spell still dispatches straight through in combat");

    // A4 GREEN guard -- underground there is no getdir in the reference.
    // doDungeonCast resolves An Sanct against the party's dungeon FACING
    // (applyAnSanctOpenChest) and has no blink/seal branch at all, and
    // world_magic is never reached from the dungeon path.
    check(cast_target_prompt(SpellId::AnSanct, false, true) == CastTargetPrompt::None,
          "A4 guard: An Sanct underground uses dungeon facing, never a getdir");
    check(cast_target_prompt(SpellId::InPor, false, true) == CastTargetPrompt::None,
          "A4 guard: In Por underground must not prompt");
}

// ---------------------------------------------------------------------------
// B. The UiSession world-getdir seam (R-11), driven the way AlphaRuntime
//    drives it.
// ---------------------------------------------------------------------------
static void session_tests() {
    // B1 RED -- a world cast parked on UiRequestId::Direction must behave like
    // every other world getdir: one direction press dispatches the Cast with
    // has_direction set and returns to Exploration.  Today handle_modal treats
    // EVERY CommandKind::Cast as a combat aim reticle, so the direction only
    // walks combat_x/combat_y and nothing is ever dispatched -- world_magic
    // reads has_direction only, so the effect could never fire even with the
    // prompt opened.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 6; cast.caster = 0;
        ui.begin_target(UiRequestId::Direction, "Direction?", cast, -1, -1);
        check(ui.mode() == UiMode::TargetSelection, "B1: the world cast getdir opens");
        const auto before = spy.intents.size();
        ui.handle_input(dir(Direction::East));
        const bool dispatched = spy.intents.size() > before &&
                                spy.intents.back().kind == UiIntentKind::Command &&
                                spy.intents.back().command.kind == CommandKind::Cast &&
                                spy.intents.back().command.item == 6 &&
                                spy.intents.back().command.has_direction &&
                                spy.intents.back().command.direction == Direction::East;
        check(dispatched && ui.mode() == UiMode::Exploration,
              "B1: a direction at the world cast getdir must dispatch Cast with has_direction "
              "(RED expected: handle_modal walks the combat reticle instead)");
    }

    // B2 GREEN guard -- combat aim is untouched.  Seeded at the caster's cell
    // with UiRequestId::Target, directions walk the reticle, Confirm dispatches
    // with has_target and WITHOUT has_direction.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 13;
        ui.begin_target(UiRequestId::Target, "Spell aim", cast, 4, 5);
        const auto before = spy.intents.size();
        ui.handle_input(dir(Direction::North));
        check(spy.intents.size() == before && ui.target_y() == 4,
              "B2 guard: a direction in combat walks the reticle and dispatches nothing");
        ui.handle_input(action(UiActionKind::Confirm));
        check(spy.intents.size() > before && spy.intents.back().command.kind == CommandKind::Cast &&
                  spy.intents.back().command.has_target &&
                  !spy.intents.back().command.has_direction &&
                  spy.intents.back().command.combat_y == 4,
              "B2 guard: Confirm in combat dispatches the aimed Cast unchanged");
    }

    // B3 ORDERING guard -- cancelling the world getdir must still DISPATCH the
    // Cast (with no direction), because the reference has already spent the
    // charge and the mana by the time the getdir is on screen.  A cancel that
    // silently returned would refund the charge and diverge; see the reference
    // citation at the top of this file.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 6; cast.caster = 0;
        ui.begin_target(UiRequestId::Direction, "Direction?", cast, -1, -1);
        const auto before = spy.intents.size();
        ui.handle_input(action(UiActionKind::Cancel));
        check(spy.intents.size() > before && spy.intents.back().kind == UiIntentKind::Command &&
                  spy.intents.back().command.kind == CommandKind::Cast &&
                  spy.intents.back().command.item == 6 &&
                  !spy.intents.back().command.has_direction &&
                  ui.mode() == UiMode::Exploration,
              "B3 ordering guard: a cancelled world cast still dispatches, so the charge is "
              "spent exactly as the reference spends it before the getdir");
    }

    // B4 Y-21 -- the Rel Hur scroll getdir, driven exactly as AlphaRuntime's
    // (U)se picker drives it (UiRequestId::UseTarget, "Direction?").  The audit
    // says the device never supplies a direction for scroll use; this pins the
    // route end to end.
    {
        UiTextBlock blocks[8];
        Spy spy;
        UiSession ui{{blocks, 8}, {&spy, Spy::send}, {40, 8, 12}};
        Command use; use.kind = CommandKind::UseItem; use.item = 1;
        ui.begin_target(UiRequestId::UseTarget, "Direction?", use, -1, -1);
        ui.handle_input(dir(Direction::West));
        check(spy.intents.back().kind == UiIntentKind::Command &&
                  spy.intents.back().command.kind == CommandKind::UseItem &&
                  spy.intents.back().command.item == 1 &&
                  spy.intents.back().command.has_direction &&
                  spy.intents.back().command.direction == Direction::West,
              "B4 (Y-21): the Rel Hur getdir dispatches UseItem with a direction");
    }
}

// ---------------------------------------------------------------------------
// C. The effects and the charge/mana ordering (R-11, Y-21) in world_magic.
// ---------------------------------------------------------------------------
static void effect_tests() {
    // C1 -- An Ex Por with a direction seals the locked door in front.
    {
        TownWorld w;
        w.tiles[4 * 32 + 5] = 184; // locked door, East of the party
        w.game.spell_quantities[25] = 1;
        CommandContext c{w.game, w.turn, w.travel, w.commands, w.world};
        c.quest_world = &w.quest;
        const auto active = get_active_map(w.world, w.game.position.map);
        check(active.error == Error::None, "C1: the town map loads");
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 25; cast.caster = 0;
        cast.direction = Direction::East; cast.has_direction = true;
        world_magic(c, cast, active.value, sink, rng_source(w.game.rng));
        check(w.tiles[4 * 32 + 5] == 151 && said(events, "Locked!") &&
                  emitted(events, GameEventKind::MapChanged),
              "C1: An Ex Por East seals the locked door");
        check(w.game.spell_quantities[25] == 0 && w.game.party.characters[0].current_mp == 45,
              "C1: the successful seal spends one charge and the circle in mana");
    }

    // C2 -- An Sanct with a direction unlocks the door in front.
    {
        TownWorld w;
        w.tiles[4 * 32 + 5] = 185; // magically locked door
        w.game.spell_quantities[6] = 1;
        CommandContext c{w.game, w.turn, w.travel, w.commands, w.world};
        c.quest_world = &w.quest;
        const auto active = get_active_map(w.world, w.game.position.map);
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 6; cast.caster = 0;
        cast.direction = Direction::East; cast.has_direction = true;
        world_magic(c, cast, active.value, sink, rng_source(w.game.rng));
        check(w.tiles[4 * 32 + 5] == 184 && said(events, "Success!"),
              "C2: An Sanct East unlocks the door in front");
        check(w.game.spell_quantities[6] == 0, "C2: the unlock spends its charge");
    }

    // C3 ORDERING -- the SAME cast with no direction (the cancelled getdir)
    // still spends the charge and the mana and prints "Cancelled.", exactly as
    // the reference does: castSpell has already run before pendingCastUnlock is
    // armed, so cancelling refunds nothing.
    {
        TownWorld w;
        w.tiles[4 * 32 + 5] = 185;
        w.game.spell_quantities[6] = 1;
        CommandContext c{w.game, w.turn, w.travel, w.commands, w.world};
        c.quest_world = &w.quest;
        const auto active = get_active_map(w.world, w.game.position.map);
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 6; cast.caster = 0;
        cast.cancel_target = true; // what UiSession stamps on a cancelled aim
        world_magic(c, cast, active.value, sink, rng_source(w.game.rng));
        check(said(events, "Cancelled."), "C3 ordering: a cancelled An Sanct says Cancelled.");
        check(w.game.spell_quantities[6] == 0 && w.game.party.characters[0].current_mp == 48,
              "C3 ordering: a cancelled An Sanct still spends the charge and the mana "
              "(reference: castSpell runs before the getdir)");
        check(w.tiles[4 * 32 + 5] == 185, "C3 ordering: a cancelled An Sanct changes no terrain");
    }

    // C4 -- In Por blinks along the ray to the last grass cell of the chunk
    // window.  Overworld only (TimePermitted 9 = combat|overworld).
    {
        std::vector<uint8_t> terrain(65536, 5);
        WorldData world{}; world.overworld = terrain.data(); world.overworld_size = terrain.size();
        GameState game{}; TurnState turn{}; TravelState travel{}; CommandState commands{};
        OutdoorServices outdoor{};
        game.position = {{20, 20}, {0, 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        m.party_status = 0; m.status = 'G'; m.current_hp = m.max_hp = 100;
        m.current_mp = 50; m.level = 8; m.intelligence = 30;
        game.spell_quantities[17] = 1;
        CommandContext c{game, turn, travel, commands, world};
        c.outdoor = &outdoor;
        const auto active = get_active_map(world, game.position.map);
        check(active.error == Error::None, "C4: the overworld map loads");
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 17; cast.caster = 0;
        cast.direction = Direction::East; cast.has_direction = true;
        world_magic(c, cast, active.value, sink, rng_source(game.rng));
        check(game.position.xy.x == 31 && game.position.xy.y == 20 &&
                  emitted(events, GameEventKind::MapChanged),
              "C4: In Por East blinks to the last grass cell of the chunk window");
        check(game.spell_quantities[17] == 0, "C4: the blink spends its charge");
    }

    // C5 ORDERING -- In Por with no direction (the cancelled getdir) spends the
    // charge, moves nothing, and stays SILENT: the reference's
    // applyBlinkSpell(null) returns zero events.
    {
        std::vector<uint8_t> terrain(65536, 5);
        WorldData world{}; world.overworld = terrain.data(); world.overworld_size = terrain.size();
        GameState game{}; TurnState turn{}; TravelState travel{}; CommandState commands{};
        OutdoorServices outdoor{};
        game.position = {{20, 20}, {0, 0}};
        game.party.character_count = game.party.party_size = 1;
        auto &m = game.party.characters[0];
        m.party_status = 0; m.status = 'G'; m.current_hp = m.max_hp = 100;
        m.current_mp = 50; m.level = 8; m.intelligence = 30;
        game.spell_quantities[17] = 1;
        CommandContext c{game, turn, travel, commands, world};
        c.outdoor = &outdoor;
        const auto active = get_active_map(world, game.position.map);
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command cast; cast.kind = CommandKind::Cast; cast.item = 17; cast.caster = 0;
        cast.cancel_target = true;
        world_magic(c, cast, active.value, sink, rng_source(game.rng));
        check(game.position.xy.x == 20 && game.spell_quantities[17] == 0 &&
                  game.party.characters[0].current_mp == 47,
              "C5 ordering: a cancelled In Por spends the charge and the mana and does not move");
        check(!said(events, "Cancelled."),
              "C5 ordering: a cancelled In Por is silent (reference: applyBlinkSpell(null) "
              "emits nothing)");
    }

    // C6 Y-21 -- the Rel Hur scroll, given the direction the getdir of B4
    // produces, actually changes the wind and spends the scroll.
    {
        TownWorld w;
        w.game.scroll_quantities[1] = 1;
        w.turn.wind = 0;
        CommandContext c{w.game, w.turn, w.travel, w.commands, w.world};
        c.quest_world = &w.quest;
        const auto active = get_active_map(w.world, w.game.position.map);
        std::vector<Seen> events; EventSink sink{&events, capture};
        Command use; use.kind = CommandKind::UseItem; use.item = 1;
        use.direction = Direction::West; use.has_direction = true;
        world_magic(c, use, active.value, sink, rng_source(w.game.rng));
        check(said(events, "Wind change!") && w.game.scroll_quantities[1] == 0,
              "C6 (Y-21): Rel Hur reports its result and spends the scroll");
        check(w.turn.wind == int(Direction::West) + 1,
              "C6 (Y-21): Rel Hur with a direction actually sets the wind");
    }
}

int main() {
    policy_tests();
    session_tests();
    effect_tests();
    std::cerr << "batch5: " << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
