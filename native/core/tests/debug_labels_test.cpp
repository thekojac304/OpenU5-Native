#include "openu5/debug_labels.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace openu5;

namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "debug labels check " << checks << " failed: " << message << "\n";
        std::exit(1);
    }
}
} // namespace

int main() {
    // L1: exact label + canonical id for all six DebugQuestItem values.
    struct { DebugQuestItem item; const char *name; int32_t id; } quest_items[] = {
        {DebugQuestItem::ShardFalsehood, "Shard of Falsehood", 29},
        {DebugQuestItem::ShardHatred, "Shard of Hatred", 30},
        {DebugQuestItem::ShardCowardice, "Shard of Cowardice", 31},
        {DebugQuestItem::Amulet, "Amulet of Lord British", 18},
        {DebugQuestItem::Crown, "Crown of Lord British", 19},
        {DebugQuestItem::Sceptre, "Sceptre of Lord British", 20},
    };
    for (const auto &entry : quest_items) {
        const auto label = debug_quest_item_label(entry.item);
        check(label.name && std::strcmp(label.name, entry.name) == 0, "L1: quest item name");
        check(label.canonical_id == entry.id, "L1: quest item canonical id");
    }

    // L2: transport mode names.
    check(std::strcmp(transport_mode_name(TransportMode::Foot), "Foot") == 0, "L2: Foot");
    check(std::strcmp(transport_mode_name(TransportMode::Horse), "Horse") == 0, "L2: Horse");
    check(std::strcmp(transport_mode_name(TransportMode::Carpet), "Carpet") == 0, "L2: Carpet");
    check(std::strcmp(transport_mode_name(TransportMode::Skiff), "Skiff") == 0, "L2: Skiff");
    check(std::strcmp(transport_mode_name(TransportMode::Ship), "Ship") == 0, "L2: Ship");

    // L3: equipment slot names (actual existing enum order -- see inventory.h).
    check(std::strcmp(equipment_slot_name(EquipSlot::Helmet), "Helmet") == 0, "L3: Helmet");
    check(std::strcmp(equipment_slot_name(EquipSlot::Armor), "Armor") == 0, "L3: Armor");
    check(std::strcmp(equipment_slot_name(EquipSlot::Weapon), "Weapon") == 0, "L3: Weapon");
    check(std::strcmp(equipment_slot_name(EquipSlot::Shield), "Shield") == 0, "L3: Shield");
    check(std::strcmp(equipment_slot_name(EquipSlot::Ring), "Ring") == 0, "L3: Ring");
    check(std::strcmp(equipment_slot_name(EquipSlot::Amulet), "Amulet") == 0, "L3: Amulet");

    // L4: DebugStatus::Unsupported must render "Unsupported", not "Unavailable".
    check(std::strcmp(debug_status_name(DebugStatus::Unsupported), "Unsupported") == 0,
          "L4: Unsupported wording");
    check(std::strcmp(debug_status_name(DebugStatus::Applied), "Applied") == 0, "L4: Applied");

    // L5: root category labels -- count and non-null unique names.
    const auto category_count = debug_root_category_count();
    check(category_count == 12, "L5: root category count");
    for (size_t i = 0; i < category_count; ++i) {
        const char *name = debug_root_category_name(i);
        check(name != nullptr && name[0] != '\0', "L5: category name non-null/non-empty");
        for (size_t j = 0; j < i; ++j)
            check(std::strcmp(name, debug_root_category_name(j)) != 0, "L5: category names unique");
    }

    // L6: no ordinal/id conflation -- enum ordinal must never equal the
    // canonical gameplay id by coincidence of matching the wrong field.
    check(int(DebugQuestItem::ShardFalsehood) == 0, "L6: ShardFalsehood ordinal");
    check(debug_quest_item_label(DebugQuestItem::ShardFalsehood).canonical_id == 29,
          "L6: ShardFalsehood canonical id");
    check(int(DebugQuestItem::Amulet) == 3, "L6: Amulet ordinal");
    check(debug_quest_item_label(DebugQuestItem::Amulet).canonical_id == 18, "L6: Amulet canonical id");
    check(int(DebugQuestItem::Crown) == 4, "L6: Crown ordinal");
    check(debug_quest_item_label(DebugQuestItem::Crown).canonical_id == 19, "L6: Crown canonical id");
    check(int(DebugQuestItem::Sceptre) == 5, "L6: Sceptre ordinal");
    check(debug_quest_item_label(DebugQuestItem::Sceptre).canonical_id == 20, "L6: Sceptre canonical id");

    // Diagnostic group table sanity (shared with the Diagnostics category
    // and device_smoke_tests.cpp's own group table -- see debug_labels.h).
    check(debug_diagnostic_group_count() == 15, "diagnostic group count");
    check(std::strcmp(debug_diagnostic_group_name(0), "Overworld") == 0, "diagnostic group 0");
    check(std::strcmp(debug_diagnostic_group_name(14), "Presentation") == 0, "diagnostic group 14");

    // Teleport floor label formatting (signed-z identity from Batch 4.5A-1).
    char buf[24]{};
    debug_format_teleport_floor_label(-1, buf, sizeof(buf));
    check(std::strcmp(buf, "Basement") == 0, "floor label z=-1");
    debug_format_teleport_floor_label(0, buf, sizeof(buf));
    check(std::strcmp(buf, "Ground Floor") == 0, "floor label z=0");
    debug_format_teleport_floor_label(2, buf, sizeof(buf));
    check(std::strcmp(buf, "Level 2") == 0, "floor label z=2");

    // Character label fallback formatting.
    debug_format_character_label("Avatar", 0, buf, sizeof(buf));
    check(std::strcmp(buf, "Avatar") == 0, "character label uses name");
    debug_format_character_label("", 3, buf, sizeof(buf));
    check(std::strcmp(buf, "Character 3") == 0, "character label fallback");
    debug_format_character_label(nullptr, 1, buf, sizeof(buf));
    check(std::strcmp(buf, "Character 1") == 0, "character label fallback (null name)");

    // Teleport result label composition (status + passability metadata).
    check(std::strcmp(debug_teleport_result_label(DebugTeleportStatus::Applied, true, false),
                       "Applied (impassable)") == 0,
          "teleport result label: impassable");
    check(std::strcmp(debug_teleport_result_label(DebugTeleportStatus::Applied, true, true),
                       "Applied") == 0,
          "teleport result label: walkable");
    check(std::strcmp(debug_teleport_result_label(DebugTeleportStatus::InvalidFloor, false, false),
                       "Invalid floor") == 0,
          "teleport result label: non-applied status passthrough");

    std::cout << checks << " debug labels checks passed\n";
}
