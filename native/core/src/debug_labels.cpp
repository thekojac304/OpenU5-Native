#include "openu5/debug_labels.h"

#include <cstdio>

namespace openu5 {
namespace {

// Ordinal position is the DebugQuestItem enum ordinal; canonical_id is the
// real gameplay item id (never equal to the ordinal, and never to be
// confused with the menu's own quest-item selector index).
constexpr DebugItemLabel kQuestItemLabels[] = {
    {"Shard of Falsehood", 29}, {"Shard of Hatred", 30}, {"Shard of Cowardice", 31},
    {"Amulet of Lord British", 18}, {"Crown of Lord British", 19}, {"Sceptre of Lord British", 20}};

// Ordinal position is the DebugSpecialItem enum ordinal; canonical_id is the
// real gameplay item id, matching UsableItemPickerInput's own documented ids
// (native/core/include/openu5/inventory_picker.h) 1:1. Grapple carries -1: it
// is Klimb-only and never a (U)se-item id (see R-07).
constexpr DebugItemLabel kSpecialItemLabels[] = {
    {"Grapple", -1}, {"Spyglass", 32}, {"HMS Cape Plans", 33}, {"Sextant", 34},
    {"Pocket Watch", 35}, {"Black Badge", 36}, {"Wooden Box", 37}};

void copy_text(const char *text, char *buf, size_t buf_size) {
    if (!buf || !buf_size) return;
    std::snprintf(buf, buf_size, "%s", text ? text : "");
}

// Batch 4.5A-4 PART 1-3: one static effect-line list per DebugPreset, in
// enum order. "LIVE: " marks a mutation to a live gameplay register or
// meaningful progression state (PART 2) -- current spot-checked presets are
// Combat (active_character), Shrine (shrine_quest/visited), Transport
// (transport mode gates real movement rules), Endgame (InDoom/GameWon/
// Shadowlord-dead/artifacts) and SaveLoad (time_spell). Re-audited against
// apply_debug_preset() (debug_developer.cpp) as of this batch -- see PART 3.
constexpr const char *kMaxedPartyEffects[] = {
    "Fill empty party slots (fillable roster only)",
    "Clear party status on newly filled slots",
    "Max stats/level/HP/MP for the active party",
    "Equip best available gear",
};
constexpr const char *kStockedInventoryEffects[] = {
    "Max Gold and Food",
    "Max Keys/Gems/Torches/Skull Keys/Magic Carpets",
    "Grant Grapple, Spyglass, Sextant (quest-neutral tools only)",
    "Max all Equipment/Spell/Scroll/Potion/Reagent quantities",
    "Does NOT grant HMS Cape, Black Badge, Wooden Box, or Shards",
};
constexpr const char *kCombatEffects[] = {
    "Max Party", "Max Resources", "Equip Best Gear",
    "LIVE: Active Player: unrestricted (255, no turn-order lock)",
    "No turn consumed", "No RNG advanced",
};
constexpr const char *kDungeonEffects[] = {
    "Max Resources (stocked inventory)", "Max Torch Turns (255)",
    "LIVE: Reset dungeon-room-cleared bitmap (all rooms unclear)",
    "LIVE: Set Word-of-Power progression flags (Word33-Word40)",
    "No turn consumed", "No RNG advanced",
};
constexpr const char *kShrineEffects[] = {
    "Max Karma (99)", "Max Gold",
    "LIVE: Mark all Shrines Visited (bitmap 0xFF)",
    "LIVE: Set Shrine Quest state to complete",
};
constexpr const char *kQuestEffects[] = {
    "LIVE: Grant all three Shards (Falsehood/Hatred/Cowardice)",
    "LIVE: Set Word-of-Power progression flags (Word33-Word40)",
    "LIVE: Mark all Shrines Visited (bitmap 0xFF)",
};
// Batch 4.5A-4 PART 11: HMS Cape is deliberately no longer part of this
// preset -- see debug_developer.cpp and the PART 11 adjudication note there.
constexpr const char *kTransportEffects[] = {
    "LIVE: Set Transport = Ship (changes real movement rules)",
    "Set sane Ship Hull (50) and Skiffs (2)",
    "Set deterministic Wind and Sail Direction",
    "Does NOT grant HMS Cape (adjudicated PART 11 -- test that separately "
    "via Special Items)",
};
constexpr const char *kEndgameEffects[] = {
    "Max Party", "Max Resources",
    "LIVE: Grant all three Lord British artifacts (Amulet/Crown/Sceptre)",
    "LIVE: Grant Wooden Box",
    "LIVE: Mark all three Shadowlords dead (Falsehood/Hatred/Cowardice)",
    "LIVE: Set InDoom progression flag",
    "LIVE: Ensure GameWon stays unset (ready, not already won)",
};
constexpr const char *kLowHealthStatusEffects[] = {
    "LIVE: Cycle each active member's status letter (P/S/C/D)",
    "LIVE: Force HP to match status (1, or 0 for Dead)",
};
// PART 12 adjudication: the active Quickness effect is a deliberate
// persistence fixture (round-trip coverage for a live temporary effect), not
// an accidental leak -- see debug_developer.cpp and the PART 12 note there.
constexpr const char *kSaveLoadEffects[] = {
    "Set deterministic clock/resource/karma fixture values",
    "Grant Grapple and Wooden Box (fixture only)",
    "Set Transport = Horse; fixture transport_tile/wind/prev_hour",
    "LIVE: Set active Quickness effect (time_spell='Q', 42 turns) -- "
    "deliberate temporary-effect persistence fixture (PART 12)",
    "LIVE: Set Word33 flag and FalsehoodDead flag (fixture)",
    "LIVE: Set NPC met/dead bits and one dungeon-room-cleared bit (fixture)",
};

template <size_t N> constexpr size_t array_len(const char *const (&)[N]) { return N; }

constexpr DebugEffectSheet kPresetSheets[] = {
    {"Maxed Party", kMaxedPartyEffects, array_len(kMaxedPartyEffects)},
    {"Stocked Inventory", kStockedInventoryEffects, array_len(kStockedInventoryEffects)},
    {"Combat Test Setup", kCombatEffects, array_len(kCombatEffects)},
    {"Dungeon Test Setup", kDungeonEffects, array_len(kDungeonEffects)},
    {"Shrine Test Setup", kShrineEffects, array_len(kShrineEffects)},
    {"Quest Progression Setup", kQuestEffects, array_len(kQuestEffects)},
    {"Transport Test Setup", kTransportEffects, array_len(kTransportEffects)},
    {"Endgame Test Setup", kEndgameEffects, array_len(kEndgameEffects)},
    {"Low Health / Status Setup", kLowHealthStatusEffects, array_len(kLowHealthStatusEffects)},
    {"Save/Load Fixture Setup", kSaveLoadEffects, array_len(kSaveLoadEffects)},
};
constexpr size_t kPresetSheetCount = sizeof(kPresetSheets) / sizeof(kPresetSheets[0]);

// Batch 4.5A-4 PART 4-10: one effect-line list per DebugCertification, in
// enum order. See apply_debug_certification() (debug_developer.cpp) for the
// exact setter/teleport composition each of these documents.
constexpr const char *kShipSailsEffects[] = {
    "LIVE: Set Transport = Ship", "Set sane Ship Hull (50) and Skiffs (2)",
    "Set deterministic Wind and Sail Direction",
    "Does NOT grant HMS Cape -- tests normal (unrigged) sail timing",
    "Teleport to Britain, ground floor / default entrance",
    "Player must press Yell to trigger real sail behavior "
    "(Board not needed -- already aboard)",
};
constexpr const char *kDungeonCertEffects[] = {
    "Compose Dungeon preset: Max Resources, Max Torch Turns",
    "LIVE: Reset dungeon-room-cleared bitmap",
    "LIVE: Set Word-of-Power progression flags (Word33-Word40)",
    "Teleport to Deceit, floor 0 via the real EnterDungeon command path",
    "Player is now in a real dungeon session; proceed normally",
};
constexpr const char *kBlackthornBadgeEffects[] = {
    "LIVE: Grants Black Badge possession",
    "Does NOT wear the badge (time_spell unchanged)",
    "Teleport to Palace of Blackthorn, ground floor / default entrance",
    "Player must exit Developer, press U, select Black Badge to wear it",
    "NOTE: Blackthorn capture text is still blocked by a separate MISCMSG "
    "integration issue (informational only)",
};
constexpr const char *kFlameShardEffects[] = {
    "LIVE: Grant all three Shards (Falsehood/Hatred/Cowardice)",
    "Shadowlord/flame progression state left untouched",
    "Teleport to Serpent's Hold, ground floor / default entrance",
    "Player must perform the real Yell / shard interaction at the flame",
};
constexpr const char *kShopNpcEffects[] = {
    "Set Gold to 9999",
    "LIVE: Set clock to 12:00 noon (changes NPC/shop schedules)",
    "Teleport to Britain, ground floor / standard entrance",
    "Player must approach an NPC/shopkeeper (wait/Talk) to trigger it",
};

constexpr DebugEffectSheet kCertificationSheets[] = {
    {"Ship / Sails Test", kShipSailsEffects, array_len(kShipSailsEffects)},
    {"Dungeon Test", kDungeonCertEffects, array_len(kDungeonCertEffects)},
    {"Blackthorn Badge Test", kBlackthornBadgeEffects, array_len(kBlackthornBadgeEffects)},
    {"Flame / Shard Test", kFlameShardEffects, array_len(kFlameShardEffects)},
    {"Shop / NPC Test", kShopNpcEffects, array_len(kShopNpcEffects)},
};
constexpr size_t kCertificationSheetCount =
    sizeof(kCertificationSheets) / sizeof(kCertificationSheets[0]);

} // namespace

const char *debug_root_category_name(size_t index) {
    return index < kDebugRootCategoryCount ? kDebugRootCategoryNames[index] : "";
}

const char *debug_diagnostic_group_name(size_t index) {
    return index < kDebugDiagnosticGroupCount ? kDebugDiagnosticGroupNames[index] : "";
}

DebugItemLabel debug_quest_item_label(DebugQuestItem item) {
    const auto index = size_t(item);
    constexpr size_t count = sizeof(kQuestItemLabels) / sizeof(kQuestItemLabels[0]);
    return index < count ? kQuestItemLabels[index] : DebugItemLabel{};
}

DebugItemLabel debug_special_item_label(DebugSpecialItem item) {
    const auto index = size_t(item);
    constexpr size_t count = sizeof(kSpecialItemLabels) / sizeof(kSpecialItemLabels[0]);
    return index < count ? kSpecialItemLabels[index] : DebugItemLabel{};
}

void debug_format_item_label(const char *name, int32_t canonical_id, char *buf, size_t buf_size) {
    if (!buf || !buf_size) return;
    if (canonical_id >= 0) std::snprintf(buf, buf_size, "%s [%d]", name ? name : "Unknown", int(canonical_id));
    else copy_text(name, buf, buf_size);
}

const char *debug_status_name(DebugStatus status) {
    switch (status) {
    case DebugStatus::Applied: return "Applied";
    case DebugStatus::Unsupported: return "Unsupported";
    case DebugStatus::InvalidCharacter: return "Invalid character";
    case DebugStatus::InvalidIndex: return "Invalid index";
    case DebugStatus::InvalidValue: return "Invalid value";
    case DebugStatus::MissingContext: return "Missing context";
    case DebugStatus::MissingData: return "Missing data";
    case DebugStatus::CoreRejected: return "Core rejected";
    }
    return "Unknown";
}

const char *debug_teleport_status_name(DebugTeleportStatus status) {
    switch (status) {
    case DebugTeleportStatus::Applied: return "Applied";
    case DebugTeleportStatus::InvalidDestination: return "Invalid destination";
    case DebugTeleportStatus::InvalidFloor: return "Invalid floor";
    case DebugTeleportStatus::InvalidCoordinates: return "Invalid coordinates";
    case DebugTeleportStatus::MissingMapData: return "Missing map data";
    case DebugTeleportStatus::MissingDungeonContext: return "Missing dungeon context";
    case DebugTeleportStatus::ActiveCombat: return "Blocked by active combat";
    case DebugTeleportStatus::CoreRejected: return "Core rejected";
    case DebugTeleportStatus::ImpassableDestination: return "Impassable destination";
    }
    return "Unknown";
}

const char *debug_teleport_result_label(DebugTeleportStatus status, bool passability_known,
                                        bool passable) {
    if (status == DebugTeleportStatus::Applied && passability_known && !passable)
        return "Applied (impassable)";
    return debug_teleport_status_name(status);
}

const char *transport_mode_name(TransportMode mode) {
    switch (mode) {
    case TransportMode::Foot: return "Foot";
    case TransportMode::Horse: return "Horse";
    case TransportMode::Carpet: return "Carpet";
    case TransportMode::Skiff: return "Skiff";
    case TransportMode::Ship: return "Ship";
    }
    return "Unknown";
}

const char *equipment_slot_name(EquipSlot slot) {
    switch (slot) {
    case EquipSlot::Helmet: return "Helmet";
    case EquipSlot::Armor: return "Armor";
    case EquipSlot::Weapon: return "Weapon";
    case EquipSlot::Shield: return "Shield";
    case EquipSlot::Ring: return "Ring";
    case EquipSlot::Amulet: return "Amulet";
    case EquipSlot::None: break;
    }
    return "Unknown";
}

const DebugEffectSheet &debug_preset_info(DebugPreset preset) {
    static constexpr DebugEffectSheet kEmpty{};
    const auto index = size_t(preset);
    return index < kPresetSheetCount ? kPresetSheets[index] : kEmpty;
}

const DebugEffectSheet &debug_certification_info(DebugCertification certification) {
    static constexpr DebugEffectSheet kEmpty{};
    const auto index = size_t(certification);
    return index < kCertificationSheetCount ? kCertificationSheets[index] : kEmpty;
}

void debug_format_teleport_floor_label(int16_t signed_z, char *buf, size_t buf_size) {
    if (!buf || !buf_size) return;
    if (signed_z < 0) copy_text("Basement", buf, buf_size);
    else if (signed_z == 0) copy_text("Ground Floor", buf, buf_size);
    else std::snprintf(buf, buf_size, "Level %d", int(signed_z));
}

void debug_format_character_label(const char *name, size_t index, char *buf, size_t buf_size) {
    if (!buf || !buf_size) return;
    if (name && name[0]) copy_text(name, buf, buf_size);
    else std::snprintf(buf, buf_size, "Character %zu", index);
}

} // namespace openu5
