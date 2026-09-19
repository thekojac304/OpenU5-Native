#include "openu5/debug_developer.h"

#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/inventory_picker.h"
#include "openu5/outdoor.h"
#include "openu5/persistence.h"
#include "openu5/quest.h"
#include "openu5/quest_world.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "debug developer check " << checks << " failed: " << message << "\n";
        std::exit(1);
    }
}

struct Harness {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    WorldData world{};
    CommandContext context{game, turn, travel, commands, world};
    CombatState combat{};
    CombatContext combat_owner{game, turn, combat};
    OutdoorServices outdoor{};
    int32_t attack[48]{}, defense[48]{};

    Harness() {
        static constexpr const char *names[] = {"Avatar", "Iolo", "Shamino", "Mariah"};
        static constexpr char classes[] = {'A', 'B', 'F', 'M'};
        game.party.character_count = 4;
        game.party.party_size = 2;
        game.party.active_character = 255;
        for (size_t i = 0; i < 4; ++i) {
            auto &c = game.party.characters[i];
            std::strcpy(c.name, names[i]);
            c.character_class = classes[i];
            c.status = 'G';
            c.strength = c.dexterity = c.intelligence = 20;
            c.level = 2;
            c.current_hp = c.max_hp = 60;
            c.party_status = i < 2 ? 0 : 255;
            c.helmet = c.armor = c.weapon = c.shield = c.ring = c.amulet = 255;
        }
        for (int32_t i = 0; i < 48; ++i)
            attack[i] = defense[i] = i;
        combat_owner.tables = {attack, nullptr, defense, nullptr, 48};
        context.combat_context = &combat_owner;
        context.outdoor = &outdoor;
        game.rng.seed(0x12345678);
    }
};
} // namespace

int main() {
    Harness h;

    check(debug_set_character_number(h.game, 0, DebugCharacterNumber::Strength, 30).status == DebugStatus::Applied &&
              h.game.party.characters[0].strength == 30,
          "character numeric edit");
    const auto strength = h.game.party.characters[0].strength;
    check(debug_set_character_number(h.game, 0, DebugCharacterNumber::Strength, 99).status == DebugStatus::InvalidValue &&
              h.game.party.characters[0].strength == strength,
          "character UI bounds are atomic");
    check(debug_set_character_number(h.game, 9, DebugCharacterNumber::Level, 8).status == DebugStatus::InvalidCharacter,
          "character index validation");
    check(debug_set_character_text(h.game, 0, DebugCharacterText::Name, "LongAvatarName").status == DebugStatus::Applied &&
              std::string(h.game.party.characters[0].name) == "LongAvata",
          "native save name width is preserved");
    check(debug_set_character_text(h.game, 0, DebugCharacterText::Class, "X").status == DebugStatus::InvalidValue &&
              debug_set_character_text(h.game, 0, DebugCharacterText::Status, "P").status == DebugStatus::Applied,
          "class and status validation");
    check(debug_set_equipment_slot(h.game, 0, EquipSlot::Weapon, 41).status == DebugStatus::Applied &&
              h.game.party.characters[0].weapon == 41 &&
              debug_set_equipment_slot(h.game, 0, EquipSlot::None, 1).status == DebugStatus::InvalidIndex,
          "raw equipment slot editor");

    check(debug_set_resource(h.game, DebugResource::Gold, 9999).status == DebugStatus::Applied && h.game.gold == 9999,
          "gold editor");
    check(debug_set_resource(h.game, DebugResource::PartySize, 5).status == DebugStatus::InvalidValue &&
              h.game.party.party_size == 2,
          "party size cannot exceed the real roster");
    check(debug_set_resource(h.game, DebugResource::ActiveCharacter, 1).status == DebugStatus::Applied &&
              debug_set_resource(h.game, DebugResource::ActiveCharacter, 2).status == DebugStatus::InvalidValue,
          "active character invariant");
    check(debug_set_inventory_quantity(h.game, DebugInventory::Reagents, 7, 88).status == DebugStatus::Applied &&
              h.game.reagent_quantities[7] == 88 &&
              debug_set_inventory_quantity(h.game, DebugInventory::Reagents, 8, 1).status == DebugStatus::InvalidIndex,
          "inventory/reagent editor");

    check(debug_set_special_item(h.game, DebugSpecialItem::Spyglass, true).status == DebugStatus::Applied && h.game.spyglass,
          "special item editor");
    {
        GameState before_pw = h.game;
        check(debug_set_special_item(h.game, DebugSpecialItem::PocketWatch, true).status == DebugStatus::Unsupported &&
                  std::memcmp(&before_pw, &h.game, sizeof(GameState)) == 0,
              "unowned always-available pocket watch is explicit and never mutates GameState");
    }

    // Black Badge semantic boundary (Batch 4.5A-3 Part 4): the debug toggle
    // must alter possession (GameState::black_badge) only. It must never set
    // TurnState::time_spell -- that wear-state transition belongs exclusively
    // to the real (U)se path (quest_world.cpp's use_quest_item), which is what
    // the Palace guard/password behavior keys off of.
    check(debug_set_special_item(h.game, DebugSpecialItem::BlackBadge, true).status == DebugStatus::Applied &&
              h.game.black_badge && h.turn.time_spell == 0,
          "debug Black Badge toggle sets possession only, not time_spell");
    {
        UsableItemPickerInput picker_input{};
        picker_input.black_badge = h.game.black_badge;
        const auto rows = usable_item_picker_rows(picker_input);
        bool found_badge = false;
        for (size_t i = 0; i < rows.count; ++i)
            if (rows.rows[i].id == 36) found_badge = true;
        check(found_badge, "Use picker contains Black Badge after debug possession toggle");
    }
    check(use_quest_item(h.context, 36, EventSink{}) == CommandStatus::Success && h.turn.time_spell == '\x1d',
          "real gameplay Use path still owns the wear-state transition");
    check(debug_set_special_item(h.game, DebugSpecialItem::BlackBadge, false).status == DebugStatus::Applied &&
              !h.game.black_badge && h.turn.time_spell == '\x1d',
          "debug Black Badge toggle off is possession-only too; it does not touch time_spell");
    check(debug_set_quest_item(h.game, DebugQuestItem::ShardHatred, true).status == DebugStatus::Applied && h.game.quest.shards[1] &&
              debug_set_quest_item(h.game, DebugQuestItem::Crown, true).status == DebugStatus::Applied && h.game.quest.artifacts[1],
          "shard and artifact editors");
    check(debug_set_quest_flag(h.game, QuestFlag::Word40, true).status == DebugStatus::Applied &&
              quest_flag(h.game.quest, QuestFlag::Word40),
          "known progression flag editor");
    check(debug_set_quest_number(h.game, DebugQuestNumber::ShrineVisitedBitmap, 0xa5).status == DebugStatus::Applied &&
              h.game.quest.shrine_visited == 0xa5 &&
              debug_set_shrine_destroyed(h.game, 7, 0x80).status == DebugStatus::Applied,
          "shrine and quest numeric editors");

    check(debug_set_clock(h.game, DebugClockPart::Month, 6).status == DebugStatus::Applied &&
              debug_set_clock(h.game, DebugClockPart::Month, 7).status == DebugStatus::InvalidValue,
          "reference six-month calendar bounds");
    check(debug_set_transport(h.game, TransportMode::Carpet).status == DebugStatus::Applied &&
              h.game.transport == TransportMode::Carpet,
          "transport mode editor");
    check(debug_set_runtime_number(h.turn, DebugRuntimeNumber::Wind, 4).status == DebugStatus::Applied && h.turn.wind == 4 &&
              debug_set_runtime_number(h.turn, DebugRuntimeNumber::Wind, 5).status == DebugStatus::InvalidValue,
          "wind editor");
    check(debug_set_time_spell(h.turn, 'Q').status == DebugStatus::Applied &&
              debug_set_time_spell(h.turn, 'X').status == DebugStatus::InvalidValue && h.turn.time_spell == 'Q',
          "temporal effect editor");
    check(debug_set_shadowlord_location(h.turn, 2, 31).status == DebugStatus::Applied &&
              h.turn.has_shadowlords && h.turn.shadowlord_locations[2] == 31,
          "shadowlord location editor");

    check(debug_set_npc_flag(h.game, DebugNpcFlag::Dead, 4, 31, true).status == DebugStatus::Applied &&
              (h.game.npc_dead[4] & 0x80000000u) != 0 &&
              debug_set_npc_flag(h.game, DebugNpcFlag::Met, 32, 0, true).status == DebugStatus::InvalidIndex,
          "NPC dead/met bitmaps");
    check(debug_set_dungeon_room_cleared(h.game, 6, 15, true).status == DebugStatus::Applied &&
              (h.game.dungeon_rooms_cleared[13] & 0x80) != 0,
          "dungeon cleared-room bitmap");
    h.outdoor.enemies.push_back({0, 1, 2, 3, 4});
    h.outdoor.enemy_view.push_back({0, 2, 3, 4, 0, 0});
    check(debug_clear_overworld_enemies(h.context).status == DebugStatus::Applied &&
              h.outdoor.enemies.empty() && h.outdoor.enemy_view.empty(),
          "live outdoor enemy owner is cleared");

    h.game.party.characters[0].status = 'D';
    h.game.party.characters[0].current_hp = 0;
    check(debug_restore_party(h.game, DebugPartyRestore::Heal).status == DebugStatus::Applied &&
              h.game.party.characters[0].status == 'D',
          "heal does not silently resurrect");
    check(debug_restore_party(h.game, DebugPartyRestore::Revive).status == DebugStatus::Applied &&
              h.game.party.characters[0].status == 'G' && h.game.party.characters[0].current_hp == 60,
          "explicit revive restores live state");

    Harness shortcuts;
    const auto seed = shortcuts.game.rng.get_seed();
    check(apply_debug_shortcut(shortcuts.context, DebugShortcut::MaximizeAll).status == DebugStatus::Applied &&
              shortcuts.game.party.characters[0].level == 8 && shortcuts.game.party.characters[0].max_hp == 240 &&
              shortcuts.game.party.characters[0].current_mp == 30 && shortcuts.game.party.characters[1].current_mp == 15 &&
              shortcuts.game.gold == 0 && shortcuts.game.reagent_quantities[7] == 0 && !shortcuts.game.quest.shards[2],
          "Max Party changes only legitimate fields of current members");
    check(shortcuts.game.rng.get_seed() == seed, "non-teleport debug operations are zero-rand");
    check(apply_debug_shortcut(shortcuts.context, DebugShortcut::MaxResources).status == DebugStatus::Applied &&
              shortcuts.game.gold == 9999 && shortcuts.game.food == 9999 &&
              shortcuts.game.spell_quantities[47] == 99 && shortcuts.game.reagent_quantities[7] == 99 &&
              shortcuts.game.equipment_quantities[47] == 99 && shortcuts.game.grapple &&
              shortcuts.game.spyglass && shortcuts.game.sextant && !shortcuts.game.quest.shards[2] &&
              !shortcuts.game.hms_cape && !shortcuts.game.black_badge && !shortcuts.game.wooden_box,
          "Max Resources stocks legal quest-neutral test inventory");
    check(shortcuts.game.rng.get_seed() == seed && shortcuts.game.turns_since_start == 0 &&
              shortcuts.game.time.hour == 0 && shortcuts.game.time.minute == 0,
          "resource helper advances no RNG, turn, or world time");
    check(apply_debug_shortcut(shortcuts.context, DebugShortcut::BestEquipment).status == DebugStatus::Applied &&
              shortcuts.game.party.characters[0].helmet == 3 && shortcuts.game.party.characters[0].armor == 15 &&
              shortcuts.game.party.characters[0].weapon == 41 && shortcuts.game.party.characters[0].shield == 255 &&
              shortcuts.game.party.characters[0].ring == 44 && shortcuts.game.party.characters[0].amulet == 47,
          "best equipment derives from live combat tables and observes two-hand rule");
    shortcuts.game.party.party_size = 1;
    check(apply_debug_shortcut(shortcuts.context, DebugShortcut::FullMaxParty).status == DebugStatus::Applied &&
              shortcuts.game.party.party_size == 1 && shortcuts.game.party.characters[0].level == 8 &&
              shortcuts.game.gold == 9999 && shortcuts.game.party.characters[0].weapon == 41 &&
              shortcuts.game.rng.get_seed() == seed && shortcuts.game.turns_since_start == 0,
          "full test setup orchestrates the helpers without adding party members");
    check(apply_debug_shortcut(shortcuts.context, DebugShortcut::KillShadowlords).status == DebugStatus::Applied &&
              can_reach_doom(shortcuts.game),
          "endgame story shortcut sets exactly the three death flags");

    Harness presets;
    check(apply_debug_preset(presets.context, DebugPreset::MaxedParty).status == DebugStatus::Applied &&
              presets.game.party.party_size == 4 && presets.game.gold == 0,
          "maxed-party preset does not implicitly stock resources");
    check(apply_debug_preset(presets.context, DebugPreset::StockedInventory).status == DebugStatus::Applied &&
              presets.game.gold == 9999 && presets.game.equipment_quantities[47] == 99 && presets.game.grapple &&
              !presets.game.hms_cape && !presets.game.black_badge && !presets.game.wooden_box &&
              !presets.game.quest.shards[0],
          "quest-neutral stocked preset");
    const auto preset_seed = presets.game.rng.get_seed();
    check(apply_debug_preset(presets.context, DebugPreset::Combat).status == DebugStatus::Applied &&
              presets.game.party.active_character == 255 && !presets.context.combat && presets.game.rng.get_seed() == preset_seed,
          "combat preset prepares state without inventing a combat session or locking active player");

    // RED-before-GREEN regression: DebugPreset::Combat used to set
    // active_character=0, which silently engages the real Set Active Player
    // mechanic and makes combat scheduling auto-pass every member but one.
    // Prove the preset instead leaves every party member manually reachable
    // over a real combat turn cycle.
    CombatMap combat_map{};
    std::fill(std::begin(combat_map.tiles), std::end(combat_map.tiles), int16_t(5));
    combat_map.start_count[2] = uint8_t(presets.game.party.party_size);
    for (int32_t i = 0; i < presets.game.party.party_size; ++i)
        combat_map.starts[2][i] = {int16_t(5 - i), 5};
    combat_map.unit_count = 1;
    combat_map.units[0] = {8, 8};
    CombatEnemy combat_enemy{};
    combat_enemy.hp = 50;
    combat_enemy.damage = 1;
    const CombatEnemy *combat_defs[] = {&combat_enemy};
    check(initialize_combat(presets.combat_owner, combat_map, CombatDirection::South, combat_defs, 1) ==
              CombatResult::Ok,
          "combat preset produces a valid multi-member arena");
    bool member_took_turn[kRosterCapacity]{};
    int distinct_players = 0;
    for (int iter = 0; iter < 200 && !combat_over(presets.combat); ++iter) {
        auto *actor = current_combat_actor(presets.combat_owner);
        if (!actor)
            break;
        if (actor->member != 255) {
            if (!member_took_turn[actor->member]) {
                member_took_turn[actor->member] = true;
                ++distinct_players;
            }
            combat_action(presets.combat_owner, CombatAction::Pass);
        } else {
            combat_action(presets.combat_owner, CombatAction::EnemyStep);
        }
    }
    check(distinct_players > 1,
          "combat preset leaves every party member reachable for manual turns, not just one");
    std::memset(presets.game.dungeon_rooms_cleared, 0xff, sizeof(presets.game.dungeon_rooms_cleared));
    check(apply_debug_preset(presets.context, DebugPreset::Dungeon).status == DebugStatus::Applied &&
              quest_flag(presets.game.quest, QuestFlag::Word33) && quest_flag(presets.game.quest, QuestFlag::Word40) &&
              presets.game.torch_turns == 255 && presets.game.dungeon_rooms_cleared[13] == 0,
          "dungeon-ready preset");
    check(apply_debug_preset(presets.context, DebugPreset::Shrine).status == DebugStatus::Applied &&
              shrine_mode(presets.game, 0) == ShrineMode::QuestComplete,
          "shrine ceremony preset");
    check(apply_debug_preset(presets.context, DebugPreset::Quest).status == DebugStatus::Applied &&
              presets.game.quest.shards[0] && quest_flag(presets.game.quest, QuestFlag::Word36),
          "quest progression preset");
    // P6 (Batch 4.5A-4 PART 11): RED characterization proved the prior
    // Transport preset also set game.hms_cape = true and
    // turn.hms_cape_toggle = 1, silently rigging the ship (commands.cpp's
    // hms_cape-gated movement-timing/tile-advance logic) even though nothing
    // ties a generic "prepare a coherent transport test" preset specifically
    // to HMS Cape behavior. Adjudicated: generic Transport preset now means
    // normal transport only -- HMS Cape possession stays false, and the
    // now-inert toggle bit is reset to 0 rather than left half-set. See
    // debug_developer.cpp's DebugPreset::Transport case for the full note.
    check(apply_debug_preset(presets.context, DebugPreset::Transport).status == DebugStatus::Applied &&
              presets.game.transport == TransportMode::Ship && presets.turn.transport_tile == 0x24 &&
              presets.game.ship_hull == 50 && !presets.game.hms_cape && presets.turn.hms_cape_toggle == 0,
          "P6 GREEN: coherent ship/transport preset no longer silently grants HMS Cape");
    check(apply_debug_preset(presets.context, DebugPreset::Endgame).status == DebugStatus::Applied &&
              endgame_ready(presets.game) && quest_flag(presets.game.quest, QuestFlag::InDoom) &&
              !quest_flag(presets.game.quest, QuestFlag::GameWon) && presets.game.wooden_box,
          "endgame-ready but not already won preset");
    check(apply_debug_preset(presets.context, DebugPreset::LowHealthStatus).status == DebugStatus::Applied &&
              presets.game.party.characters[0].status == 'P' && presets.game.party.characters[0].current_hp == 1 &&
              presets.game.party.characters[3].status == 'D' && presets.game.party.characters[3].current_hp == 0,
          "low-health/status recovery preset");

    Harness save_load;
    check(apply_debug_preset(save_load.context, DebugPreset::SaveLoad).status == DebugStatus::Applied,
          "save/load test-state preset applied");
    save::Json retained = save::Json::object();
    std::string serialized;
    check(save::save_state(save_load.game, save_load.turn, retained, serialized) == save::Error::None,
          "preset serializes through real save adapter");
    GameState loaded{};
    TurnState loaded_turn{};
    save::Json loaded_retained;
    check(save::load_state(serialized, loaded, loaded_turn, loaded_retained) == save::Error::None &&
              loaded.gold == 4321 && loaded.time.year == 142 && loaded.transport == TransportMode::Horse &&
              loaded_turn.time_spell == 'Q' && quest_flag(loaded.quest, QuestFlag::FalsehoodDead) &&
              (loaded.npc_met[0] & (1u << 3)) != 0 && (loaded.dungeon_rooms_cleared[0] & (1u << 2)) != 0,
          "save/load preset round-trips representative native owners");

    // --- Batch 4.5A-4: Certification setups (C3, C4, C5, C6) ---
    // C1 (category/row wiring) and C2 (Blackthorn Badge, UI-driven per PART
    // 14) live in ui_debug_menu_test.cpp; the rest are exercised directly
    // through apply_debug_certification() here, mirroring how presets above
    // are tested at the core-API layer.
    {
        // Deterministic world/dungeon fixture: Britain(2), Blackthorn
        // Palace(18) and Serpent's Hold(32) ground-floor small maps (tile 5,
        // the same generic-walkable convention debug_map_picker_test.cpp's
        // own default Harness uses), plus eight dungeons with an authored
        // floor-0 ladder-up entry cell, matching Deceit(33)'s real shape.
        GameState g; TurnState t; TravelState tr; CommandState cs;
        std::vector<uint8_t> large(65536, 5), local(1024, 5);
        MapData maps[] = {
            {{2, 0}, local.data(), local.size()},
            {{18, 0}, local.data(), local.size()},
            {{32, 0}, local.data(), local.size()},
        };
        WorldData world{large.data(), large.data(), large.size(), large.size(), maps,
                        sizeof(maps) / sizeof(maps[0])};
        DungeonData dungeons[8]{};
        for (int i = 0; i < 8; ++i) {
            dungeons[i].location = uint8_t(33 + i);
            dungeons[i].cells[2] = 0x10; // authored ladder-up entry, floor 0, (2,0)
        }
        DungeonState dungeon_state{};
        DungeonScratch scratch{};
        DungeonContext dungeon_owner{dungeon_state, scratch};
        dungeon_owner.data = dungeons;
        dungeon_owner.count = 8;
        CommandContext c{g, t, tr, cs, world};
        c.dungeon_context = &dungeon_owner;
        g.party.character_count = 1;
        g.party.party_size = 1;
        g.party.active_character = 255;
        g.rng.seed(0x2468ace0);

        // C5: Flame/Shard setup grants the three shards, leaves Shadowlord
        // progression untouched, and teleports to Serpent's Hold.
        const auto rng_before = g.rng.get_seed();
        auto flame = apply_debug_certification(c, DebugCertification::FlameShard);
        check(flame.setup.status == DebugStatus::Applied && flame.teleport.status == DebugTeleportStatus::Applied,
              "C5: Flame/Shard setup applies");
        check(g.quest.shards[0] && g.quest.shards[1] && g.quest.shards[2],
              "C5: all three shards granted");
        check(g.quest.summoned == -1 && g.quest.doom_bits == 0,
              "C5: Shadowlord/flame progression left untouched (summoned default is -1, unset)");
        check(g.position.map.location == 32 && g.position.map.floor == 0,
              "C5: deterministic teleport to Serpent's Hold");
        check(g.rng.get_seed() == rng_before, "C5: no RNG advanced by the setup itself");

        // C3: Shop/NPC setup sets Gold/clock and teleports to Britain; no
        // dialogue/shop session exists anywhere in CommandContext for this
        // setup to patch, so "not patched" is structurally guaranteed --
        // asserted here as "no such field was touched" via the plain
        // GameState/TurnState assertions below.
        auto shop = apply_debug_certification(c, DebugCertification::ShopNpc);
        check(shop.setup.status == DebugStatus::Applied && shop.teleport.status == DebugTeleportStatus::Applied,
              "C3: Shop/NPC setup applies");
        check(g.gold == 9999, "C3: Gold set to 9999");
        check(g.time.hour == 12 && g.time.minute == 0, "C3: clock set to 12:00");
        check(g.position.map.location == 2 && g.position.map.floor == 0,
              "C3: deterministic teleport to Britain ground floor");

        // C6: Ship/Sails setup prepares sane transport state, never grants
        // HMS Cape, and never fires a Sails/Yell command itself.
        auto ship = apply_debug_certification(c, DebugCertification::ShipSails);
        check(ship.setup.status == DebugStatus::Applied && ship.teleport.status == DebugTeleportStatus::Applied,
              "C6: Ship/Sails setup applies");
        check(g.transport == TransportMode::Ship && g.ship_hull == 50 && g.ship_skiffs == 2,
              "C6: sane transport/hull/skiffs");
        check(!g.hms_cape, "C6: HMS Cape remains false");
        check(g.position.map.location == 2 && g.position.map.floor == 0,
              "C6: deterministic teleport to Britain (transport-capable location)");

        // C4: Dungeon setup composes the Dungeon preset (stocked inventory,
        // torch, room-bitmap reset, word-of-power flags already proven above
        // for DebugPreset::Dungeon) and teleports to Deceit floor 0 via the
        // real EnterDungeon command path -- not a patched DungeonState. The
        // dungeon session is genuinely active afterward, exactly like any
        // other Developer > Teleport dungeon destination.
        std::memset(g.dungeon_rooms_cleared, 0xff, sizeof(g.dungeon_rooms_cleared));
        auto dungeon = apply_debug_certification(c, DebugCertification::Dungeon);
        check(dungeon.setup.status == DebugStatus::Applied &&
                  dungeon.teleport.status == DebugTeleportStatus::Applied,
              "C4: Dungeon setup applies");
        check(g.torch_turns == 255 && g.dungeon_rooms_cleared[0] == 0,
              "C4: Dungeon preset's torch/room-bitmap reset composed in");
        check(quest_flag(g.quest, QuestFlag::Word33) && quest_flag(g.quest, QuestFlag::Word40),
              "C4: Dungeon preset's word-of-power flags composed in");
        check(dungeon_state.active && dungeon_state.pos.dungeon == 33 && dungeon_state.pos.floor == 0,
              "C4: real dungeon session entered via EnterDungeon, not a patched DungeonState");
    }

    std::cout << checks << " debug developer checks passed\n";
}
