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
