#include "openu5/debug_developer.h"

#include "openu5/outdoor.h"
#include "openu5/quest.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace openu5 {
namespace {

constexpr int32_t kMaxWord = 9999;
constexpr int32_t kMaxCount = 99;
constexpr int32_t kMaxAttribute = 30;
constexpr int32_t kEquipmentCount = 48;

DebugResult result(DebugStatus status, uint16_t mutations = 0) {
    return {status, mutations};
}

CharacterState *character(GameState &g, size_t index) {
    return index < g.party.character_count && index < kRosterCapacity
               ? &g.party.characters[index]
               : nullptr;
}

bool in_range(int64_t value, int64_t lo, int64_t hi) {
    return value >= lo && value <= hi;
}

bool valid_class(char value) {
    return std::strchr("AMBFDTPRS", value) != nullptr;
}

bool valid_status(char value) {
    return std::strchr("GPCSD", value) != nullptr;
}

uint8_t level_for_exp(uint16_t exp) {
    uint8_t level = 1;
    for (int32_t n = exp / 100; n > 0; n >>= 1)
        ++level;
    return std::min<uint8_t>(8, level);
}

uint8_t max_mp(const CharacterState &c) {
    if (c.character_class == 'A' || c.character_class == 'M')
        return c.intelligence;
    if (c.character_class == 'B')
        return uint8_t(c.intelligence >> 1);
    return 0;
}

void maximize_character(CharacterState &c) {
    c.status = 'G';
    c.strength = c.dexterity = c.intelligence = kMaxAttribute;
    c.exp = kMaxWord;
    c.level = level_for_exp(c.exp);
    c.max_hp = uint16_t(30 * c.level);
    c.current_hp = c.max_hp;
    c.current_mp = max_mp(c);
}

size_t active_count(const GameState &g) {
    if (g.party.party_size <= 0)
        return 0;
    return std::min<size_t>({size_t(g.party.party_size), size_t(g.party.character_count),
                             size_t(kMaxParty)});
}

size_t fillable_party(const GameState &g) {
    size_t count = 0;
    for (; count < g.party.character_count && count < kMaxParty; ++count)
        if (!g.party.characters[count].name[0])
            break;
    return count;
}

uint16_t maximize_party(GameState &g, bool fill) {
    size_t count = active_count(g);
    if (fill) {
        count = fillable_party(g);
        if (count) {
            g.party.party_size = int32_t(count);
            for (size_t i = 0; i < count; ++i)
                g.party.characters[i].party_status = 0;
        }
    }
    for (size_t i = 0; i < count; ++i)
        maximize_character(g.party.characters[i]);
    return uint16_t(count);
}

uint16_t stock_inventory(GameState &g) {
    g.gold = g.food = kMaxWord;
    g.keys = g.gems = g.torches = g.skull_keys = g.magic_carpets = kMaxCount;
    // Quest-neutral tools only. HMS Cape plans, badge, wooden box, shards and
    // Lord British's regalia remain owned by their normal progression flags.
    g.grapple = g.spyglass = g.sextant = true;
    const size_t equipment_count = std::min<size_t>(g.equipment_count, 256);
    for (size_t i = 0; i < equipment_count; ++i)
        g.equipment_quantities[i] = kMaxCount;
    for (auto &v : g.spell_quantities)
        v = kMaxCount;
    for (auto &v : g.scroll_quantities)
        v = kMaxCount;
    for (auto &v : g.potion_quantities)
        v = kMaxCount;
    for (auto &v : g.reagent_quantities)
        v = kMaxCount;
    return uint16_t(10 + equipment_count + 72);
}

struct BestGear {
    uint8_t values[6]{kEquipmentNothing, kEquipmentNothing, kEquipmentNothing,
                      kEquipmentNothing, kEquipmentNothing, kEquipmentNothing};
};

BestGear best_gear(const CombatTables &tables) {
    BestGear best;
    int32_t scores[6]{-1, -1, -1, -1, -1, -1};
    for (int32_t id = 0; id < kEquipmentCount; ++id) {
        const auto slot = slot_for_equip(id);
        const auto slot_index = unsigned(slot);
        if (slot == EquipSlot::None || slot_index >= 6 || size_t(id) >= tables.count)
            continue;
        const int32_t *metric = slot == EquipSlot::Weapon ? tables.attack : tables.defense;
        if (!metric)
            continue;
        if (metric[id] >= scores[slot_index]) {
            scores[slot_index] = metric[id];
            best.values[slot_index] = uint8_t(id);
        }
    }
    const auto weapon = best.values[unsigned(EquipSlot::Weapon)];
    if (weapon != kEquipmentNothing && equip_type_of(weapon) == 48)
        best.values[unsigned(EquipSlot::Shield)] = kEquipmentNothing;
    return best;
}

uint16_t apply_best_gear(GameState &g, const CombatTables *tables) {
    if (!tables || (!tables->attack && !tables->defense))
        return 0;
    const auto gear = best_gear(*tables);
    uint16_t mutations = 0;
    for (size_t i = 0; i < active_count(g); ++i) {
        auto &c = g.party.characters[i];
        uint8_t *slots[] = {&c.helmet, &c.armor, &c.weapon, &c.shield, &c.ring, &c.amulet};
        for (size_t slot = 0; slot < 6; ++slot) {
            const auto id = gear.values[slot];
            *slots[slot] = id;
            ++mutations;
            if (id != kEquipmentNothing && g.equipment_quantities[id] < 1) {
                g.equipment_quantities[id] = 1;
                ++mutations;
            }
        }
    }
    return mutations;
}

const CombatTables *combat_tables(const CommandContext &c) {
    return c.combat_context ? &c.combat_context->tables : nullptr;
}

DebugResult maximize_all(CommandContext &c) {
    return result(DebugStatus::Applied, maximize_party(c.game, false));
}

} // namespace

DebugResult debug_set_character_number(GameState &g, size_t index, DebugCharacterNumber field,
                                       int32_t value) {
    auto *c = character(g, index);
    if (!c)
        return result(DebugStatus::InvalidCharacter);
    switch (field) {
    case DebugCharacterNumber::Strength:
    case DebugCharacterNumber::Dexterity:
    case DebugCharacterNumber::Intelligence:
        if (!in_range(value, 1, 30)) return result(DebugStatus::InvalidValue);
        if (field == DebugCharacterNumber::Strength) c->strength = uint8_t(value);
        else if (field == DebugCharacterNumber::Dexterity) c->dexterity = uint8_t(value);
        else c->intelligence = uint8_t(value);
        break;
    case DebugCharacterNumber::CurrentMp:
        if (!in_range(value, 0, 99)) return result(DebugStatus::InvalidValue);
        c->current_mp = uint8_t(value); break;
    case DebugCharacterNumber::CurrentHp:
        if (!in_range(value, 0, 9999)) return result(DebugStatus::InvalidValue);
        c->current_hp = uint16_t(value); break;
    case DebugCharacterNumber::MaxHp:
        if (!in_range(value, 0, 9999)) return result(DebugStatus::InvalidValue);
        c->max_hp = uint16_t(value); break;
    case DebugCharacterNumber::Experience:
        if (!in_range(value, 0, 9999)) return result(DebugStatus::InvalidValue);
        c->exp = uint16_t(value); break;
    case DebugCharacterNumber::Level:
        if (!in_range(value, 1, 8)) return result(DebugStatus::InvalidValue);
        c->level = uint8_t(value); break;
    case DebugCharacterNumber::MonthsAtInn:
        if (!in_range(value, 0, 255)) return result(DebugStatus::InvalidValue);
        c->months_at_inn = uint8_t(value); break;
    case DebugCharacterNumber::Gender:
        if (value != 0x0b && value != 0x0c) return result(DebugStatus::InvalidValue);
        c->gender = uint8_t(value); break;
    case DebugCharacterNumber::PartyStatus:
        if (!in_range(value, 0, 255)) return result(DebugStatus::InvalidValue);
        c->party_status = uint8_t(value); break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_character_text(GameState &g, size_t index, DebugCharacterText field,
                                     const char *value) {
    auto *c = character(g, index);
    if (!c)
        return result(DebugStatus::InvalidCharacter);
    if (!value)
        return result(DebugStatus::InvalidValue);
    switch (field) {
    case DebugCharacterText::Name:
        std::strncpy(c->name, value, sizeof(c->name) - 1);
        c->name[sizeof(c->name) - 1] = 0;
        break;
    case DebugCharacterText::Class:
        if (!value[0] || value[1] || !valid_class(value[0])) return result(DebugStatus::InvalidValue);
        c->character_class = value[0];
        break;
    case DebugCharacterText::Status:
        if (!value[0] || value[1] || !valid_status(value[0])) return result(DebugStatus::InvalidValue);
        c->status = value[0];
        break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_equipment_slot(GameState &g, size_t index, EquipSlot slot, int32_t id) {
    auto *c = character(g, index);
    if (!c)
        return result(DebugStatus::InvalidCharacter);
    if (unsigned(slot) >= unsigned(EquipSlot::None))
        return result(DebugStatus::InvalidIndex);
    if (!(id == kEquipmentNothing || in_range(id, 0, 47)))
        return result(DebugStatus::InvalidValue);
    uint8_t *slots[] = {&c->helmet, &c->armor, &c->weapon, &c->shield, &c->ring, &c->amulet};
    *slots[unsigned(slot)] = uint8_t(id);
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_restore_party(GameState &g, DebugPartyRestore mode) {
    uint16_t mutations = 0;
    for (size_t i = 0; i < active_count(g); ++i) {
        auto &c = g.party.characters[i];
        if (mode == DebugPartyRestore::Heal) {
            if (c.status != 'D') { c.current_hp = c.max_hp; ++mutations; }
        } else if (mode == DebugPartyRestore::ClearStatus) {
            if (c.status != 'D') { c.status = 'G'; c.current_hp = c.max_hp; mutations += 2; }
        } else {
            c.status = 'G';
            if (!c.max_hp)
                c.max_hp = uint16_t(30 * std::max<int>(1, c.level));
            c.current_hp = c.max_hp;
            c.current_mp = max_mp(c); mutations += 3;
        }
    }
    return result(DebugStatus::Applied, mutations);
}

DebugResult debug_set_resource(GameState &g, DebugResource field, int64_t value) {
    switch (field) {
    case DebugResource::Food:
    case DebugResource::Gold:
        if (!in_range(value, 0, 9999)) return result(DebugStatus::InvalidValue);
        if (field == DebugResource::Food) g.food = uint16_t(value); else g.gold = uint16_t(value);
        break;
    case DebugResource::Keys:
    case DebugResource::Gems:
    case DebugResource::Torches:
    case DebugResource::SkullKeys:
    case DebugResource::MagicCarpets:
        if (!in_range(value, 0, 99)) return result(DebugStatus::InvalidValue);
        if (field == DebugResource::Keys) g.keys = int32_t(value);
        else if (field == DebugResource::Gems) g.gems = int32_t(value);
        else if (field == DebugResource::Torches) g.torches = int32_t(value);
        else if (field == DebugResource::SkullKeys) g.skull_keys = int32_t(value);
        else g.magic_carpets = int32_t(value);
        break;
    case DebugResource::Karma:
        if (!in_range(value, 0, 99)) return result(DebugStatus::InvalidValue);
        g.karma = uint8_t(value); break;
    case DebugResource::TurnsSinceStart:
        if (!in_range(value, 0, 999999)) return result(DebugStatus::InvalidValue);
        g.turns_since_start = value; break;
    case DebugResource::TorchTurns:
        if (!in_range(value, 0, 999)) return result(DebugStatus::InvalidValue);
        g.torch_turns = uint16_t(value); break;
    case DebugResource::PartySize:
        if (!in_range(value, 1, std::min<int32_t>(kMaxParty, g.party.character_count)))
            return result(DebugStatus::InvalidValue);
        g.party.party_size = int32_t(value); break;
    case DebugResource::ActiveCharacter:
        if (!(value == 255 || (value >= 0 && value < g.party.party_size)))
            return result(DebugStatus::InvalidValue);
        g.party.active_character = uint8_t(value); break;
    case DebugResource::ShipHull:
    case DebugResource::ShipSkiffs:
        if (!in_range(value, 0, 99)) return result(DebugStatus::InvalidValue);
        if (field == DebugResource::ShipHull) g.ship_hull = int32_t(value);
        else g.ship_skiffs = int32_t(value);
        break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_inventory_quantity(GameState &g, DebugInventory array, size_t index,
                                         int32_t quantity) {
    if (!in_range(quantity, 0, 99))
        return result(DebugStatus::InvalidValue);
    int32_t *values = nullptr;
    size_t count = 0;
    switch (array) {
    case DebugInventory::Equipment: values = g.equipment_quantities; count = std::min<size_t>(g.equipment_count, 256); break;
    case DebugInventory::Spells: values = g.spell_quantities; count = 48; break;
    case DebugInventory::Scrolls: values = g.scroll_quantities; count = 8; break;
    case DebugInventory::Potions: values = g.potion_quantities; count = 8; break;
    case DebugInventory::Reagents: values = g.reagent_quantities; count = 8; break;
    }
    if (index >= count)
        return result(DebugStatus::InvalidIndex);
    values[index] = quantity;
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_special_item(GameState &g, DebugSpecialItem item, bool value) {
    switch (item) {
    case DebugSpecialItem::Grapple: g.grapple = value; break;
    case DebugSpecialItem::Spyglass: g.spyglass = value; break;
    case DebugSpecialItem::HmsCape: g.hms_cape = value; break;
    case DebugSpecialItem::Sextant: g.sextant = value; break;
    case DebugSpecialItem::PocketWatch: return result(DebugStatus::Unsupported);
    case DebugSpecialItem::BlackBadge: g.black_badge = value; break;
    case DebugSpecialItem::WoodenBox: g.wooden_box = value; break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_quest_item(GameState &g, DebugQuestItem item, bool value) {
    const auto raw = unsigned(item);
    if (raw < 3) g.quest.shards[raw] = value;
    else if (raw < 6) g.quest.artifacts[raw - 3] = value;
    else return result(DebugStatus::InvalidIndex);
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_quest_flag(GameState &g, QuestFlag flag, bool value) {
    if (unsigned(flag) >= unsigned(QuestFlag::Count))
        return result(DebugStatus::InvalidIndex);
    set_quest_flag(g.quest, flag, value);
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_quest_number(GameState &g, DebugQuestNumber field, int32_t value) {
    if (!in_range(value, 0, 255))
        return result(DebugStatus::InvalidValue);
    switch (field) {
    case DebugQuestNumber::ShrineQuestBitmap: g.quest.shrine_quest = value; g.quest.optional_present |= 1; break;
    case DebugQuestNumber::ShrineVisitedBitmap: g.quest.shrine_visited = value; g.quest.optional_present |= 2; break;
    case DebugQuestNumber::ShadowlordSummoned: g.quest.summoned = value; g.quest.optional_present |= 8; break;
    case DebugQuestNumber::ShadowlordDoomBits: g.quest.doom_bits = value; g.quest.optional_present |= 4; break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_shrine_destroyed(GameState &g, size_t index, uint8_t value) {
    if (index >= 8)
        return result(DebugStatus::InvalidIndex);
    g.quest.shrine_destroyed[index] = value;
    g.quest.destroyed_count = std::max(g.quest.destroyed_count, uint8_t(index + 1));
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_clock(GameState &g, DebugClockPart part, int32_t value) {
    switch (part) {
    case DebugClockPart::Year: if (!in_range(value, 0, 999)) return result(DebugStatus::InvalidValue); g.time.year = value; break;
    case DebugClockPart::Month: if (!in_range(value, 1, 6)) return result(DebugStatus::InvalidValue); g.time.month = value; break;
    case DebugClockPart::Day: if (!in_range(value, 1, 28)) return result(DebugStatus::InvalidValue); g.time.day = value; break;
    case DebugClockPart::Hour: if (!in_range(value, 0, 23)) return result(DebugStatus::InvalidValue); g.time.hour = value; break;
    case DebugClockPart::Minute: if (!in_range(value, 0, 59)) return result(DebugStatus::InvalidValue); g.time.minute = value; break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_transport(GameState &g, TransportMode mode) {
    if (unsigned(mode) > unsigned(TransportMode::Ship))
        return result(DebugStatus::InvalidValue);
    g.transport = mode;
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_runtime_number(TurnState &t, DebugRuntimeNumber field, int32_t value) {
    int32_t lo = 0, hi = 255;
    if (field == DebugRuntimeNumber::Wind) hi = 4;
    else if (field == DebugRuntimeNumber::SailDirection) hi = 4;
    else if (field == DebugRuntimeNumber::HmsCapeToggle) hi = 1;
    else if (field == DebugRuntimeNumber::PreviousHour) hi = 23;
    if (!in_range(value, lo, hi)) return result(DebugStatus::InvalidValue);
    switch (field) {
    case DebugRuntimeNumber::Wind: t.wind = value; break;
    case DebugRuntimeNumber::TransportTile: t.transport_tile = value; break;
    case DebugRuntimeNumber::WindDriftCounter: t.wind_drift_counter = value; break;
    case DebugRuntimeNumber::SailDirection: t.sail_dir = value; break;
    case DebugRuntimeNumber::HmsCapeToggle: t.hms_cape_toggle = value; break;
    case DebugRuntimeNumber::PreviousHour: t.prev_hour = value; break;
    case DebugRuntimeNumber::LightSpellMinutes: t.light_spell_minutes = value; break;
    case DebugRuntimeNumber::TimeSpellTurns: t.spell_turns = value; break;
    case DebugRuntimeNumber::SkullTreeFoundDay: t.skull_tree_day = value; break;
    case DebugRuntimeNumber::FeluccaPhase: t.felucca_phase = value; break;
    case DebugRuntimeNumber::TrammelPhase: t.trammel_phase = value; break;
    }
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_time_spell(TurnState &t, char value) {
    if (value && !std::strchr("PQCNT", value))
        return result(DebugStatus::InvalidValue);
    t.time_spell = value;
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_shadowlord_location(TurnState &t, size_t index, int32_t value) {
    if (index >= t.shadowlord_locations.size()) return result(DebugStatus::InvalidIndex);
    if (!in_range(value, 0, 255)) return result(DebugStatus::InvalidValue);
    t.shadowlord_locations[index] = value;
    t.has_shadowlords = true;
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_npc_flag(GameState &g, DebugNpcFlag flag, size_t location, size_t npc,
                               bool value) {
    if (location >= 32 || npc >= 32)
        return result(DebugStatus::InvalidIndex);
    auto &row = flag == DebugNpcFlag::Dead ? g.npc_dead[location] : g.npc_met[location];
    const uint32_t mask = uint32_t(1) << unsigned(npc);
    row = value ? row | mask : row & ~mask;
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_set_dungeon_room_cleared(GameState &g, size_t dungeon, size_t room,
                                           bool value) {
    if (dungeon >= 7 || room >= 16)
        return result(DebugStatus::InvalidIndex);
    const size_t bit = (dungeon << 4) + room;
    const uint8_t mask = uint8_t(1u << unsigned(bit & 7));
    auto &byte = g.dungeon_rooms_cleared[bit >> 3];
    byte = uint8_t(value ? byte | mask : byte & ~mask);
    return result(DebugStatus::Applied, 1);
}

DebugResult debug_clear_overworld_enemies(CommandContext &c) {
    if (!c.outdoor)
        return result(DebugStatus::MissingContext);
    const auto count = c.outdoor->enemies.size() + c.outdoor->enemy_view.size();
    c.outdoor->enemies.clear();
    c.outdoor->enemy_view.clear();
    return result(DebugStatus::Applied, uint16_t(std::min<size_t>(count, 65535)));
}

DebugResult apply_debug_shortcut(CommandContext &c, DebugShortcut shortcut) {
    switch (shortcut) {
    case DebugShortcut::MaximizeAll:
        return maximize_all(c);
    case DebugShortcut::BestEquipment:
        return result(DebugStatus::Applied, apply_best_gear(c.game, combat_tables(c)));
    case DebugShortcut::FullMaxParty: {
        auto mutations = maximize_party(c.game, false);
        mutations = uint16_t(mutations + stock_inventory(c.game));
        mutations = uint16_t(mutations + apply_best_gear(c.game, combat_tables(c)));
        return result(DebugStatus::Applied, mutations);
    }
    case DebugShortcut::KillShadowlords:
        set_quest_flag(c.game.quest, QuestFlag::FalsehoodDead);
        set_quest_flag(c.game.quest, QuestFlag::HatredDead);
        set_quest_flag(c.game.quest, QuestFlag::CowardiceDead);
        return result(DebugStatus::Applied, 3);
    case DebugShortcut::MaxResources:
        return result(DebugStatus::Applied, stock_inventory(c.game));
    }
    return result(DebugStatus::Unsupported);
}

DebugResult apply_debug_preset(CommandContext &c, DebugPreset preset) {
    auto &g = c.game;
    uint16_t mutations = 0;
    switch (preset) {
    case DebugPreset::MaxedParty:
        mutations = maximize_party(g, true);
        mutations = uint16_t(mutations + apply_best_gear(g, combat_tables(c)));
        break;
    case DebugPreset::StockedInventory:
        mutations = stock_inventory(g);
        break;
    case DebugPreset::Combat:
        mutations = maximize_party(g, true);
        mutations = uint16_t(mutations + stock_inventory(g));
        mutations = uint16_t(mutations + apply_best_gear(g, combat_tables(c)));
        // Prepare a multi-member combat test state without invoking the real
        // Set Active Player mechanic: leaving it at the "unrestricted" sentinel
        // keeps combat scheduling free to cycle every party member, matching
        // normal (non-debug) turn order. See GAMEPLAY_INTEGRATION_AUDIT.md.
        g.party.active_character = 255;
        break;
    case DebugPreset::Dungeon:
        mutations = stock_inventory(g);
        g.torch_turns = 255;
        std::fill(std::begin(g.dungeon_rooms_cleared), std::end(g.dungeon_rooms_cleared), 0);
        for (int i = int(QuestFlag::Word33); i <= int(QuestFlag::Word40); ++i)
            set_quest_flag(g.quest, QuestFlag(i));
        mutations = uint16_t(mutations + 10);
        break;
    case DebugPreset::Shrine:
        g.karma = 99;
        g.gold = 9999;
        g.quest.shrine_visited = 0xff;
        g.quest.shrine_quest = 1;
        g.quest.optional_present |= 3;
        mutations = 4;
        break;
    case DebugPreset::Quest:
        for (auto &v : g.quest.shards) v = true;
        for (int i = int(QuestFlag::Word33); i <= int(QuestFlag::Word40); ++i)
            set_quest_flag(g.quest, QuestFlag(i));
        g.quest.shrine_visited = 0xff;
        g.quest.optional_present |= 2;
        mutations = 12;
        break;
    case DebugPreset::Transport:
        // Batch 4.5A-4 PART 11 adjudication: this preset used to also set
        // g.hms_cape = true. RED characterization (pre-fix) proved that made
        // every generic Transport-preset setup silently rig the ship (half
        // movement cost + tile-advance parity flip -- commands.cpp:382-389),
        // even though nothing about "prepare a coherent transport test"
        // implies "and also test the HMS Cape speed bonus". No comment, test
        // name, or prior audit entry ties this preset specifically to HMS
        // Cape behavior (unlike StockedInventory's explicit "quest-neutral
        // tools only" contract, which already deliberately excludes it).
        // Adjudicated: generic Transport preset now means normal transport;
        // HMS Cape is tested on its own via Special Items or the dedicated
        // Ship/Sails Certification setup (which also never grants it).
        // hms_cape_toggle is reset to 0 (its default) rather than left at
        // its old 1: that field only has any effect while hms_cape is true
        // (commands.cpp:382), so leaving it "half-toggled" here would be a
        // misleading residue of the removed grant, not a real state.
        g.transport = TransportMode::Ship;
        g.ship_hull = 50;
        g.ship_skiffs = 2;
        c.turn.transport_tile = 0x24;
        c.turn.wind = 3;
        c.turn.sail_dir = 3;
        c.turn.wind_drift_counter = 0;
        c.turn.hms_cape_toggle = 0;
        mutations = 8;
        break;
    case DebugPreset::Endgame:
        mutations = maximize_party(g, true);
        mutations = uint16_t(mutations + stock_inventory(g));
        for (auto &v : g.quest.artifacts) v = true;
        g.wooden_box = true;
        set_quest_flag(g.quest, QuestFlag::FalsehoodDead);
        set_quest_flag(g.quest, QuestFlag::HatredDead);
        set_quest_flag(g.quest, QuestFlag::CowardiceDead);
        set_quest_flag(g.quest, QuestFlag::InDoom);
        set_quest_flag(g.quest, QuestFlag::GameWon, false);
        mutations = uint16_t(mutations + 9);
        break;
    case DebugPreset::LowHealthStatus: {
        static constexpr char statuses[] = {'P', 'S', 'C', 'D'};
        const auto count = active_count(g);
        for (size_t i = 0; i < count; ++i) {
            auto &ch = g.party.characters[i];
            ch.status = statuses[i % 4];
            ch.current_hp = ch.status == 'D' ? 0 : 1;
            mutations += 2;
        }
        break;
    }
    case DebugPreset::SaveLoad:
        g.time = {142, 6, 28, 23, 59};
        g.gold = 4321; g.food = 8765; g.keys = 12; g.gems = 23; g.torches = 34;
        g.karma = 77; g.grapple = true; g.wooden_box = true;
        g.transport = TransportMode::Horse;
        c.turn.transport_tile = 0x10; c.turn.wind = 4; c.turn.prev_hour = 22;
        c.turn.time_spell = 'Q'; c.turn.spell_turns = 42;
        set_quest_flag(g.quest, QuestFlag::Word33);
        set_quest_flag(g.quest, QuestFlag::FalsehoodDead);
        g.npc_met[0] |= 1u << 3;
        g.npc_dead[1] |= 1u << 4;
        g.dungeon_rooms_cleared[0] |= 1u << 2;
        mutations = 20;
        break;
    }
    return result(DebugStatus::Applied, mutations);
}

namespace {

// Real canonical location ids (matching debug_map_picker's destination
// enumeration / the extracted location table -- see debug_map_picker_test.cpp
// and GAMEPLAY_INTEGRATION_AUDIT.md): Britain=2, Blackthorn Palace=18,
// Empath Abbey=31, Deceit=33 (the first of the eight dungeons).
// Certification setups look these up by id directly through
// apply_debug_teleport(), exactly like any other DebugTeleportRequest caller
// -- there is no separate id table to keep in sync (PART 5).
constexpr uint8_t kLocationBritain = 2;
constexpr uint8_t kLocationBlackthornPalace = 18;
constexpr uint8_t kLocationEmpathAbbey = 31;
constexpr uint8_t kLocationDeceit = 33;

// Batch 4.5A-4 correction pass: the real, authoritative ritual/flame-cast
// check is cast_shard_into_flame() (quest.cpp), invoked from
// quest_world.cpp:103 with the party's live position:
//   cast_shard_into_flame(id-29, g.position.xy.x, g.position.xy.y,
//                          g.position.map.location, g.position.map.floor, ...)
// It requires x==15, y==flame_y[i], location==30+i, floor==flame_floor[i],
// where flame_y[]={9,3,16} and flame_floor[]={2,1,-1} (quest.cpp). For i=1
// (Hatred shard / Astaroth / Flame of Love) that is location 31 (Empath
// Abbey), floor 1, (15,3) -- exactly the coordinate this batch's design
// review pointed at. It is the Hatred chain, not Falsehood, per this exact
// table (30+i for i=0 is the Lycaeum/Falsehood chain, at (15,9,2); i=2 is
// Serpent's Hold/Cowardice, at (15,16,-1)) -- corrected here rather than
// silently kept mislabeled. Any of the three would be an equally valid,
// equally sourced choice since FlameShard grants all three shards; Empath
// Abbey's is used because it was independently verified against
// re/notes/shadowlord-ritual.md and game/src/momentos/defs.ts as well.
constexpr uint8_t kLocationEmpathAbbeyFlameFloor = 1;
constexpr int32_t kLocationEmpathAbbeyFlameX = 15, kLocationEmpathAbbeyFlameY = 3;

// Batch 4.5A-4 correction pass: CommandKind::YellSails (commands.cpp) -- the
// real command a Yell dispatches to while aboard a frigate -- gates purely on
// TurnState::transport_tile (must be in the frigate range 0x20-0x27) and
// GameState::position.map.location (< 0x80); it never reads terrain. The
// device layer mirrors the exact same transport_tile test before routing 'y'
// at all (alpha_runtime.cpp's refresh_session_context():
// set_sail_context((turn_.transport_tile&0xf8)==0x20, ...)). TurnState::
// transport_tile defaults to 0x1c (Foot, turn.h) and debug_set_transport()
// only ever writes GameState::transport, never TurnState::transport_tile --
// so setting GameState::transport=Ship alone (the prior ShipSails
// composition) left transport_tile at 0x1c and Yell would still show the
// ordinary word-of-power prompt, never the real sails toggle. 0x24 is the
// exact "hoisted frigate" tile real boarding assigns (transport.cpp's
// board_transport ship() branch; world_flow_adapter_test.cpp's own real-
// boarding fixture and this file's own DebugPreset::Transport both already
// use 0x24) -- reused here, not invented.
//
// No concrete real-map "on open water" (x,y) coordinate is recorded anywhere
// in this codebase's tests, fixtures, or docs (checked transport_flow_parity_
// test.cpp, world_flow_adapter_test.cpp, debug_map_picker_test.cpp, and every
// *.md under native/targets/tdeck -- all ship-adjacent coordinates found are
// synthetic test-harness fixtures, not real extracted overworld terrain).
// Per this batch's own instruction not to invent one, the teleport target
// stays Britain's already-evidenced default entrance: the YellSails gate
// above is location-content-independent (any non-dungeon, non-Underworld-
// style location id satisfies it), so what actually made the old setup
// non-functional was the missing transport_tile write, not the destination.
constexpr int32_t kShipSailsTransportTile = 0x24;

DebugTeleportRequest standard_small_map_entry(uint8_t location) {
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::SmallMap;
    r.location = location;
    r.standard_entry = true;
    return r;
}

DebugTeleportRequest standard_dungeon_entry(uint8_t location) {
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::Dungeon;
    r.location = location;
    r.floor = 0;
    r.standard_entry = true;
    return r;
}

// Explicit manual coordinate (standard_entry=false): apply_debug_teleport
// never refuses this even onto a cell that fails the walkability check
// (T4, debug_map_picker_test.cpp) -- the same "free tester placement"
// mechanism already used for e.g. Blackthorn's prison or Serpent's Hold's
// Flame of Courage. Required here because the flame ritual cell is not a
// destination's standard/default entrance.
DebugTeleportRequest explicit_small_map_entry(uint8_t location, int16_t floor, int32_t x, int32_t y) {
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::SmallMap;
    r.location = location;
    r.floor = floor;
    r.x = x;
    r.y = y;
    r.standard_entry = false;
    return r;
}

} // namespace

DebugCertificationResult apply_debug_certification(CommandContext &c, DebugCertification cert) {
    auto &g = c.game;
    DebugCertificationResult out;
    uint16_t mutations = 0;
    DebugStatus status = DebugStatus::Applied;
    auto accumulate = [&](DebugResult r) {
        mutations = uint16_t(mutations + r.mutations);
        if (r.status != DebugStatus::Applied) status = r.status;
    };

    switch (cert) {
    case DebugCertification::ShipSails:
        // PART 6, corrected: normal (unrigged) sail timing -- HMS Cape is
        // deliberately never granted here (see debug_set_special_item(
        // HmsCape, ...), which this setup never calls). Also sets
        // TurnState::transport_tile into the frigate range (0x24) -- without
        // it, real Yell/YellSails routing never actually engages (see the
        // kShipSailsTransportTile comment above); GameState::transport alone
        // is not enough.
        accumulate(debug_set_transport(g, TransportMode::Ship));
        accumulate(debug_set_resource(g, DebugResource::ShipHull, 50));
        accumulate(debug_set_resource(g, DebugResource::ShipSkiffs, 2));
        accumulate(debug_set_runtime_number(c.turn, DebugRuntimeNumber::Wind, 2));
        accumulate(debug_set_runtime_number(c.turn, DebugRuntimeNumber::SailDirection, 2));
        accumulate(debug_set_runtime_number(c.turn, DebugRuntimeNumber::TransportTile,
                                            kShipSailsTransportTile));
        out.teleport_request = standard_small_map_entry(kLocationBritain);
        out.teleport = apply_debug_teleport(c, out.teleport_request);
        break;

    case DebugCertification::Dungeon:
        // PART 7: reuse the existing Dungeon preset verbatim, then teleport
        // via the real EnterDungeon command path (apply_debug_teleport's
        // Dungeon branch) -- DungeonState itself is never patched directly.
        accumulate(apply_debug_preset(c, DebugPreset::Dungeon));
        out.teleport_request = standard_dungeon_entry(kLocationDeceit);
        out.teleport = apply_debug_teleport(c, out.teleport_request);
        break;

    case DebugCertification::BlackthornBadge:
        // PART 8: possession only -- must never set turn.time_spell (that
        // wear-state transition belongs exclusively to the real (U)se path,
        // exactly like the Special Items Black Badge toggle it reuses).
        accumulate(debug_set_special_item(g, DebugSpecialItem::BlackBadge, true));
        out.teleport_request = standard_small_map_entry(kLocationBlackthornPalace);
        out.teleport = apply_debug_teleport(c, out.teleport_request);
        break;

    case DebugCertification::FlameShard:
        // PART 9, corrected: grant the three shards only; Shadowlord/flame
        // progression (ShadowlordSummoned, doom bits) is deliberately left
        // untouched. The prior version teleported to Serpent's Hold's
        // Default Entrance (its town-square door), which is NOT the verified
        // flame/ritual cell -- cast_shard_into_flame() checks the party's
        // exact live position (quest_world.cpp:103), and no town's default
        // entrance coincides with it. This now uses an explicit manual
        // coordinate teleport to Empath Abbey (location 31), floor 1,
        // (15,3) -- the exact, sourced ritual cell for the Hatred shard /
        // Astaroth / Flame of Love chain (see the constants above).
        accumulate(debug_set_quest_item(g, DebugQuestItem::ShardFalsehood, true));
        accumulate(debug_set_quest_item(g, DebugQuestItem::ShardHatred, true));
        accumulate(debug_set_quest_item(g, DebugQuestItem::ShardCowardice, true));
        out.teleport_request = explicit_small_map_entry(kLocationEmpathAbbey, kLocationEmpathAbbeyFlameFloor,
                                                         kLocationEmpathAbbeyFlameX, kLocationEmpathAbbeyFlameY);
        out.teleport = apply_debug_teleport(c, out.teleport_request);
        break;

    case DebugCertification::ShopNpc:
        // PART 10: clock is tagged LIVE in the effect sheet -- it directly
        // gates NPC schedules/shop hours, which is the entire point of this
        // setup, but is still a real, disclosed side effect.
        accumulate(debug_set_resource(g, DebugResource::Gold, 9999));
        accumulate(debug_set_clock(g, DebugClockPart::Hour, 12));
        accumulate(debug_set_clock(g, DebugClockPart::Minute, 0));
        out.teleport_request = standard_small_map_entry(kLocationBritain);
        out.teleport = apply_debug_teleport(c, out.teleport_request);
        break;

    case DebugCertification::Count:
        break;
    }

    if (out.teleport.status != DebugTeleportStatus::Applied)
        status = DebugStatus::CoreRejected;
    out.setup = result(status, mutations);
    return out;
}

} // namespace openu5
