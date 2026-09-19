#include "openu5/debug_map_picker.h"

#include "openu5/dungeon.h"
#include "openu5/quest_state.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "debug map picker check " << checks << " failed: " << message << "\n";
        std::exit(1);
    }
}

struct Harness {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::vector<uint8_t> large = std::vector<uint8_t>(65536, 5);
    std::vector<uint8_t> local = std::vector<uint8_t>(1024, 5);
    std::vector<MapData> maps;
    DungeonData dungeons[8]{};
    WorldData world{};
    DungeonState dungeon{};
    DungeonScratch scratch{};
    DungeonContext dungeon_owner{dungeon, scratch};
    CommandContext context{game, turn, travel, commands, world};
    std::vector<ReloadEffect> reloads;
    std::vector<GameEventKind> events;

    Harness() {
        // Equivalent to the standard extracted web resources: 32 small-map
        // locations and eight dungeons. Extra floors exercise native z semantics.
        maps.reserve(40);
        for (int location = 1; location <= 32; ++location)
            maps.push_back({{uint8_t(location), 0}, local.data(), local.size()});
        maps.push_back({{2, 1}, local.data(), local.size()});
        for (int floor : {-1, 1, 2, 3})
            maps.push_back({{17, int16_t(floor)}, local.data(), local.size()});
        maps.push_back({{30, 1}, local.data(), local.size()});
        maps.push_back({{30, 2}, local.data(), local.size()});
        maps.push_back({{32, -1}, local.data(), local.size()});
        maps.push_back({{32, 1}, local.data(), local.size()});
        world = {large.data(), large.data(), large.size(), large.size(), maps.data(), maps.size()};

        for (int i = 0; i < 8; ++i) {
            dungeons[i].location = uint8_t(33 + i);
            for (auto &cell : dungeons[i].cells)
                cell = 0x00;
            dungeons[i].cells[2] = 0x10; // authored ladder-up entry at floor 0, (2,0)
        }
        // Floor 3, (5,6) is a wall. Picker parity allows the teleport but warns.
        dungeons[0].cells[3 * 64 + 6 * 8 + 5] = 0xb0;
        dungeon_owner.data = dungeons;
        dungeon_owner.count = 8;
        context.dungeon_context = &dungeon_owner;

        game.position = {{10, 20}, {0, 0}};
        game.party.character_count = 1;
        game.party.party_size = 1;
        game.party.active_character = 255;
        auto &avatar = game.party.characters[0];
        avatar.status = 'G';
        avatar.current_hp = avatar.max_hp = 100;
        avatar.dexterity = 20;
        avatar.party_status = 0;
        avatar.ring = 255;
        game.food = 100;
        game.gold = 777;
        game.transport = TransportMode::Horse;
        game.rng.seed(0x12345678);
        set_quest_flag(game.quest, QuestFlag::FalsehoodDead);

        context.services.context = this;
        context.services.banner = [](void *, uint8_t location) {
            if (location == 2) return "Britain";
            if (location == 13) return "Iolo's Hut";
            if (location == 17) return "Lord British's Castle";
            if (location == 30) return "Lycaeum";
            if (location == 33) return "Deceit";
            if (location == 40) return "Doom";
            return "Location";
        };
        context.services.reload = [](void *p, ReloadEffect effect, uint8_t, EventSink) {
            static_cast<Harness *>(p)->reloads.push_back(effect);
        };
        context.events = {this, [](void *p, const GameEvent &event) {
                              static_cast<Harness *>(p)->events.push_back(event.kind);
                          }};
    }
};

DebugTeleportRequest request(DebugDestinationKind kind, int location, int floor,
                             int x, int y, bool standard = false) {
    DebugTeleportRequest r;
    r.kind = kind;
    r.location = uint8_t(location);
    r.floor = int16_t(floor);
    r.x = x;
    r.y = y;
    r.standard_entry = standard;
    return r;
}
} // namespace

int main() {
    Harness h;

    // The picker does not own a destination database. With the standard live
    // resources it derives 2 large layers + 32 small locations + 8 dungeons.
    check(debug_destination_count(h.context) == 42, "standard destination count");
    auto britannia = debug_destination_at(h.context, 0);
    auto underworld = debug_destination_at(h.context, 1);
    auto britain = debug_destination_at(h.context, 3); // small id 2
    auto deceit = debug_destination_at(h.context, 34); // first dungeon
    auto doom = debug_destination_at(h.context, 41);
    check(britannia.error == Error::None && britannia.value.kind == DebugDestinationKind::Britannia,
          "Britannia is first");
    check(underworld.value.kind == DebugDestinationKind::Underworld && underworld.value.name,
          "Underworld is second");
    check(britain.value.location == 2 && std::string(britain.value.name) == "Britain",
          "small-map name comes from normal metadata callback");
    check(deceit.value.location == 33 && deceit.value.kind == DebugDestinationKind::Dungeon,
          "dungeons follow small maps");
    check(doom.value.location == 40 && std::string(doom.value.name) == "Doom",
          "last dungeon metadata");
    check(debug_destination_at(h.context, 42).error == Error::InvalidRange,
          "destination index validation");

    DebugDestination castle{DebugDestinationKind::SmallMap, 17, nullptr};
    check(debug_floor_count(h.context, castle) == 5, "castle exposes extracted floors");
    const int castle_floors[] = {-1, 0, 1, 2, 3};
    for (size_t i = 0; i < 5; ++i)
        check(debug_floor_at(h.context, castle, i).value == castle_floors[i],
              "small floors preserve signed z and sort order");
    check(debug_floor_count(h.context, deceit.value) == 8 &&
              debug_floor_at(h.context, deceit.value, 7).value == 7,
          "dungeons expose eight levels");

    // Exact DebugApi.teleportOverworld behavior: signed coordinates wrap and
    // persistent state/RNG/transport are not changed.
    const auto seed = h.game.rng.get_seed();
    auto r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Britannia, 0, 0, 300, -1));
    check(r.status == DebugTeleportStatus::Applied && h.game.position.map.location == 0 &&
              h.game.position.map.floor == 0 && h.game.position.xy.x == 44 &&
              h.game.position.xy.y == 255,
          "overworld wraps exactly like TypeScript DebugApi");
    check(h.game.rng.get_seed() == seed && h.game.gold == 777 &&
              h.game.transport == TransportMode::Horse &&
              quest_flag(h.game.quest, QuestFlag::FalsehoodDead),
          "large teleport preserves RNG and persistent gameplay state");

    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Underworld, 0, 255, 12, 34));
    check(r.status == DebugTeleportStatus::Applied && h.game.position.map.floor == 255 &&
              h.game.position.xy.x == 12 && h.game.position.xy.y == 34,
          "Underworld uses floor sentinel 0xff");
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Underworld, 0, 255, 0, 0, true));
    check(r.status == DebugTeleportStatus::Applied && h.game.position.map.floor == 255 &&
              h.game.position.xy.x == 126 && h.game.position.xy.y == 20 && r.passable,
          "Underworld standard entry uses the authored Wrong-bottom destination");

    // Town/local-map exact coordinate path: only doors, NPCs and interior
    // objects are rebuilt, in the same order as createDebugApi.
    h.reloads.clear();
    const auto before_small_seed = h.game.rng.get_seed();
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::SmallMap, 2, 1, 20, 25));
    check(r.status == DebugTeleportStatus::Applied && r.reloaded &&
              h.game.position.map.location == 2 && h.game.position.map.floor == 1 &&
              h.game.position.xy.x == 20 && h.game.position.xy.y == 25,
          "town exact-cell teleport");
    check(h.reloads.size() == 3 && h.reloads[0] == ReloadEffect::ResetDoors &&
              h.reloads[1] == ReloadEffect::EnterNpcs &&
              h.reloads[2] == ReloadEffect::HydrateInterior,
          "small-map transient rebuild parity");
    check(h.game.rng.get_seed() == before_small_seed, "small map teleport is zero-rand");

    // Representative dwelling, castle/basement, and keep standard-entry moves.
    for (const auto &entry : std::vector<DebugTeleportRequest>{
             request(DebugDestinationKind::SmallMap, 13, 0, 0, 0, true),
             request(DebugDestinationKind::SmallMap, 17, -1, 0, 0, true),
             request(DebugDestinationKind::SmallMap, 30, 2, 0, 0, true)}) {
        r = apply_debug_teleport(h.context, entry);
        check(r.status == DebugTeleportStatus::Applied && h.game.position.xy.x == 15 &&
                  h.game.position.xy.y == 30 && h.game.position.map.location == entry.location &&
                  h.game.position.map.floor == entry.floor,
              "goToLocation standard entry parity");
    }

    // Invalid local requests are atomic. The UI currently performs these bounds
    // checks before calling its mutation facade; native exposes the status.
    const auto saved_position = h.game.position;
    const auto saved_events = h.events.size();
    const auto saved_reloads = h.reloads.size();
    check(apply_debug_teleport(
              h.context, request(DebugDestinationKind::SmallMap, 2, 9, 1, 1)).status ==
              DebugTeleportStatus::InvalidFloor,
          "invalid floor rejected");
    check(apply_debug_teleport(
              h.context, request(DebugDestinationKind::SmallMap, 2, 0, 32, 1)).status ==
              DebugTeleportStatus::InvalidCoordinates,
          "invalid small coordinate rejected");
    check(h.game.position.xy.x == saved_position.xy.x &&
              h.game.position.xy.y == saved_position.xy.y &&
              h.game.position.map.location == saved_position.map.location &&
              h.events.size() == saved_events && h.reloads.size() == saved_reloads,
          "invalid requests do not partially mutate");

    // The post-teleport state remains a normal command target.
    Command pass;
    pass.kind = CommandKind::Pass;
    h.commands.awaiting_exit = false;
    const auto before_turn = h.game.turns_since_start;
    const auto pass_result = execute_command(h.context, pass);
    check(pass_result.status == CommandStatus::Success &&
              h.game.turns_since_start > before_turn,
          "normal semantic command executes after local teleport");

    // Dungeon switch uses the real entry path while preserving debug-tool RNG;
    // exact relocation then allows an impassable wall by design.
    h.reloads.clear();
    const auto before_dungeon_seed = h.game.rng.get_seed();
    r = validate_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 33, 3, 5, 6));
    check(r.status == DebugTeleportStatus::Applied && r.passability_known && !r.passable &&
              !r.rng_may_advance,
          "dungeon wall is warning-only and debug entry preserves RNG");
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 33, 3, 5, 6));
    check(r.status == DebugTeleportStatus::Applied && h.context.dungeon && h.dungeon.active &&
              h.dungeon.pos.dungeon == 33 && h.dungeon.pos.floor == 3 &&
              h.dungeon.pos.x == 5 && h.dungeon.pos.y == 6,
          "real dungeon entry then exact floor/cell relocation");
    check(h.game.rng.get_seed() == before_dungeon_seed && !r.rng_may_advance,
          "new dungeon entry restores shared RNG after initialization");
    check(h.game.turns_since_start == before_turn + 1,
          "debug dungeon entry does not consume an additional turn");
    check(h.reloads.size() == 1 && h.reloads[0] == ReloadEffect::ClearEnemies,
          "real dungeon entry clears overworld enemies");

    h.dungeon.active = false;
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 34, 0, 0, 0, true));
    check(r.status == DebugTeleportStatus::Applied && h.dungeon.active &&
              h.dungeon.pos.dungeon == 34 && h.dungeon.pos.floor == 0 &&
              h.dungeon.pos.x == 2 && h.dungeon.pos.y == 0,
          "standard dungeon entry initializes 3D mode at an authored staircase");

    h.dungeon.pos.facing = DungeonFacing::North;
    const auto same_dungeon_seed = h.game.rng.get_seed();
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 34, 7, 4, 4));
    check(r.status == DebugTeleportStatus::Applied && !r.rng_may_advance &&
              h.dungeon.pos.facing == DungeonFacing::North && h.dungeon.pos.floor == 7 &&
              h.dungeon.pos.x == 4 && h.dungeon.pos.y == 4 &&
              h.game.rng.get_seed() == same_dungeon_seed,
          "same-dungeon relocation preserves facing and RNG");

    // Deep-game destinations must all be usable sessions, rather than raw map
    // coordinates that leave the caller in exploration mode.
    for (const int dungeon : {33, 34, 35, 36, 37, 38}) {
        const auto seed_before = h.game.rng.get_seed();
        const auto turns_before = h.game.turns_since_start;
        r = apply_debug_teleport(h.context,
                                 request(DebugDestinationKind::Dungeon, dungeon, 0, 0, 0, true));
        check(r.status == DebugTeleportStatus::Applied && h.dungeon.active &&
                  h.context.dungeon && h.dungeon.pos.dungeon == dungeon &&
                  h.dungeon.pos.floor == 0,
              "requested dungeon is entered as an active session");
        check(h.game.rng.get_seed() == seed_before && h.game.turns_since_start == turns_before,
              "debug dungeon switch has no RNG or turn side effect");
    }
    r = apply_debug_teleport(h.context,
                             request(DebugDestinationKind::Dungeon, 37, 3, 4, 4));
    check(r.status == DebugTeleportStatus::Applied && h.dungeon.active &&
              h.dungeon.pos.dungeon == 37 && h.dungeon.pos.floor == 3 &&
              h.dungeon.pos.x == 4 && h.dungeon.pos.y == 4,
          "explicit deeper floor remains an active dungeon session");

    const auto dungeon_before = h.dungeon.pos;
    check(apply_debug_teleport(
              h.context, request(DebugDestinationKind::Dungeon, 33, 8, 0, 0)).status ==
              DebugTeleportStatus::InvalidFloor,
          "dungeon floor range validation");
    check(apply_debug_teleport(
              h.context, request(DebugDestinationKind::Dungeon, 33, 0, -1, 0)).status ==
              DebugTeleportStatus::InvalidCoordinates,
          "dungeon coordinate validation");
    check(h.dungeon.pos.floor == dungeon_before.floor && h.dungeon.pos.x == dungeon_before.x &&
              h.dungeon.pos.y == dungeon_before.y,
          "invalid dungeon requests are atomic");

    Command dungeon_pass;
    dungeon_pass.kind = CommandKind::DungeonCommand;
    dungeon_pass.item = int16_t(DungeonAction::Pass);
    const auto before_dungeon_turn = h.game.turns_since_start;
    const auto dungeon_result = execute_command(h.context, dungeon_pass);
    check(dungeon_result.status == CommandStatus::Success &&
              h.game.turns_since_start > before_dungeon_turn,
          "normal dungeon command executes after teleport");

    // Dungeon sessions deliberately retain their surface return position, so
    // an explicit debug map teleport—not that position—owns session teardown.
    r = apply_debug_teleport(h.context,
                             request(DebugDestinationKind::Britannia, 0, 0, 22, 33));
    check(r.status == DebugTeleportStatus::Applied && !h.context.dungeon && !h.dungeon.active &&
              h.game.position.map.location == 0 && h.game.position.xy.x == 22 &&
              h.game.position.xy.y == 33,
          "non-dungeon debug teleport cleanly leaves the authoritative dungeon session");
    r = apply_debug_teleport(h.context,
                             request(DebugDestinationKind::Dungeon, 33, 0, 0, 0, true));
    check(r.status == DebugTeleportStatus::Applied && h.context.dungeon && h.dungeon.active,
          "dungeon debug teleport reuses the normal active-session initialization after exit");

    // The native real-entry API refuses to start a second dungeon while combat
    // owns the command context. No state is changed on that safety error.
    h.context.combat = true;
    const auto active_before = h.dungeon.pos;
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 40, 0, 1, 1));
    check(r.status == DebugTeleportStatus::ActiveCombat &&
              h.dungeon.pos.dungeon == active_before.dungeon,
          "cross-dungeon switch is atomic during combat");

    h.context.combat = false;
    h.game.party.party_size = 7;
    r = apply_debug_teleport(
        h.context, request(DebugDestinationKind::Dungeon, 40, 0, 1, 1));
    check(r.status == DebugTeleportStatus::CoreRejected &&
              h.dungeon.pos.dungeon == active_before.dungeon,
          "non-combat core rejection is reported without partial mutation");

    // --- Batch 4.5A-1 RED: Default Entrance floor selection (T3, T4, T6) ---

    // T6 (characterization): debug_floor_at returns signed z sorted ascending,
    // not a player-facing floor identity. Lord British's Castle (location 17)
    // already exposes floors {-1,0,1,2,3} above: ordinal index 0 is the
    // basement (z=-1) and signed z=0 sits at ordinal index 1. This is the
    // exact semantic gap Default Entrance must not confuse: "the chosen floor
    // used by teleport" must be the signed value 0, not list index 0.
    check(debug_floor_at(h.context, castle, 0).value == -1 &&
              debug_floor_at(h.context, castle, 1).value == 0,
          "T6: floor ordinal 0 is the basement, not signed z=0 -- ordinal is not floor identity");

    // T3: standard entrance must refuse an impassable destination cell.
    // Synthetic single-floor small map: location 99, z=0, (15,30) is void.
    std::vector<uint8_t> impassable_floor(1024, 5);
    impassable_floor[30 * kSmallMapSize + 15] = 255;
    h.maps.push_back({{99, 0}, impassable_floor.data(), impassable_floor.size()});
    h.world = {h.large.data(), h.large.data(), h.large.size(), h.large.size(), h.maps.data(), h.maps.size()};
    {
        const auto before_position = h.game.position;
        const auto before_events = h.events.size();
        const auto before_reloads = h.reloads.size();
        auto vr = validate_debug_teleport(
            h.context, request(DebugDestinationKind::SmallMap, 99, 0, 0, 0, true));
        check(vr.status == DebugTeleportStatus::Applied && vr.passability_known && !vr.passable,
              "T3 setup: synthetic standard-entry destination cell is confirmed impassable");

        auto ar = apply_debug_teleport(
            h.context, request(DebugDestinationKind::SmallMap, 99, 0, 0, 0, true));
        // Expected future behavior is a dedicated DebugTeleportStatus::ImpassableDestination,
        // which does not exist in the production enum yet and must not be added in this
        // RED-only pass. Nearest honest observable proxy: the request must not be
        // reported as Applied, and no GameState mutation or map transition may occur.
        check(ar.status != DebugTeleportStatus::Applied,
              "T3 RED: standard-entry Default Entrance must refuse an impassable cell "
              "(currently always applies; DebugTeleportStatus::ImpassableDestination is "
              "not yet a production enum value)");
        check(h.game.position.map.location == before_position.map.location &&
                  h.game.position.xy.x == before_position.xy.x &&
                  h.game.position.xy.y == before_position.xy.y &&
                  h.events.size() == before_events && h.reloads.size() == before_reloads,
              "T3 RED: a refused Default Entrance must not mutate GameState or emit a transition");
    }

    // T4 (GREEN characterization guard): an explicit manual coordinate may
    // remain impassable. Only the *default* entrance is required to be valid;
    // basement access and free tester placement (e.g. Blackthorn's prison,
    // Serpent's Hold's Flame of Courage) must keep working unchanged.
    {
        auto r4 = apply_debug_teleport(
            h.context, request(DebugDestinationKind::SmallMap, 99, 0, 15, 30, false));
        check(r4.status == DebugTeleportStatus::Applied && r4.passability_known && !r4.passable,
              "T4: explicit manual coordinate teleport still applies to an impassable cell "
              "and reports it as not passable");
        check(h.game.position.map.location == 99 && h.game.position.xy.x == 15 &&
                  h.game.position.xy.y == 30,
              "T4: explicit manual coordinate teleport actually relocates the party");
    }

    std::cout << checks << " debug map picker checks passed\n";
}
