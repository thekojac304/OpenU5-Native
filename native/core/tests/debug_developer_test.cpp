#include "openu5/debug_developer.h"

#include "openu5/outdoor.h"
#include "openu5/persistence.h"
#include "openu5/quest.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

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
    check(debug_set_special_item(h.game, DebugSpecialItem::PocketWatch, true).status == DebugStatus::Unsupported,
          "unowned always-available pocket watch is explicit");
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
              presets.game.party.active_character == 0 && !presets.context.combat && presets.game.rng.get_seed() == preset_seed,
          "combat preset prepares state without inventing a combat session");
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
    check(apply_debug_preset(presets.context, DebugPreset::Transport).status == DebugStatus::Applied &&
              presets.game.transport == TransportMode::Ship && presets.turn.transport_tile == 0x24 &&
              presets.game.ship_hull == 50 && presets.turn.hms_cape_toggle == 1,
          "coherent ship/transport preset");
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

    std::cout << checks << " debug developer checks passed\n";
}
