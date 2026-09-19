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
    check(category_count == 15, "L5: root category count (Batch 4.5A-4 adds Certification)");
    for (size_t i = 0; i < category_count; ++i) {
        const char *name = debug_root_category_name(i);
        check(name != nullptr && name[0] != '\0', "L5: category name non-null/non-empty");
        for (size_t j = 0; j < i; ++j)
            check(std::strcmp(name, debug_root_category_name(j)) != 0, "L5: category names unique");
    }
    check(std::strcmp(debug_root_category_name(6), "Quest Items") == 0, "L5: Quest Items at index 6");
    check(std::strcmp(debug_root_category_name(7), "Special Items") == 0, "L5: Special Items at index 7");
    check(std::strcmp(debug_root_category_name(8), "Quest / World") == 0, "L5: Quest / World at index 8");
    check(std::strcmp(debug_root_category_name(14), "Certification") == 0, "L5: Certification at index 14");

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

    // S4 (Batch 4.5A-3): exact DebugSpecialItem name + canonical id for all
    // seven values, including Grapple's -1 (Klimb-only, never a Use-item id --
    // see R-07, PART13).
    struct { DebugSpecialItem item; const char *name; int32_t id; } special_items[] = {
        {DebugSpecialItem::Grapple, "Grapple", -1},
        {DebugSpecialItem::Spyglass, "Spyglass", 32},
        {DebugSpecialItem::HmsCape, "HMS Cape Plans", 33},
        {DebugSpecialItem::Sextant, "Sextant", 34},
        {DebugSpecialItem::PocketWatch, "Pocket Watch", 35},
        {DebugSpecialItem::BlackBadge, "Black Badge", 36},
        {DebugSpecialItem::WoodenBox, "Wooden Box", 37},
    };
    for (const auto &entry : special_items) {
        const auto label = debug_special_item_label(entry.item);
        check(label.name && std::strcmp(label.name, entry.name) == 0, "S4: special item name");
        check(label.canonical_id == entry.id, "S4: special item canonical id");
    }

    // Item label composition: "Name [id]" when canonical_id >= 0, else "Name".
    debug_format_item_label("Black Badge", 36, buf, sizeof(buf));
    check(std::strcmp(buf, "Black Badge [36]") == 0, "item label with canonical id");
    debug_format_item_label("Grapple", -1, buf, sizeof(buf));
    check(std::strcmp(buf, "Grapple") == 0, "item label without canonical id");

    // Batch 4.5A-4 P1: every DebugPreset has non-empty effect metadata -- a
    // real display name and at least one non-null effect line. Iterates all
    // ten values so a future preset added without metadata fails loudly here
    // rather than surfacing as a blank confirmation sheet on device.
    static constexpr DebugPreset kAllPresets[] = {
        DebugPreset::MaxedParty,      DebugPreset::StockedInventory, DebugPreset::Combat,
        DebugPreset::Dungeon,         DebugPreset::Shrine,           DebugPreset::Quest,
        DebugPreset::Transport,       DebugPreset::Endgame,          DebugPreset::LowHealthStatus,
        DebugPreset::SaveLoad,
    };
    for (const auto preset : kAllPresets) {
        const auto &sheet = debug_preset_info(preset);
        check(sheet.display_name && sheet.display_name[0] != '\0', "P1: preset display name non-empty");
        check(sheet.effect_count > 0, "P1: preset has at least one effect line");
        check(sheet.effects != nullptr, "P1: preset effect array non-null");
        for (size_t i = 0; i < sheet.effect_count; ++i)
            check(sheet.effects[i] != nullptr && sheet.effects[i][0] != '\0', "P1: no null/empty effect line");
    }
    // Exact display names for the presets PART 1/3 calls out by name.
    check(std::strcmp(debug_preset_info(DebugPreset::Combat).display_name, "Combat Test Setup") == 0,
          "P1: Combat preset display name matches the PART 1 example");
    check(std::strcmp(debug_preset_info(DebugPreset::Transport).display_name, "Transport Test Setup") == 0,
          "P1: Transport preset display name");

    // P2: known live-state presets contain at least one "LIVE: " line.
    // Spot-check exactly the four PART 2 names -- Shrine, Transport,
    // Endgame, SaveLoad -- plus Combat, which PART 1's own worked example
    // shows carrying a LIVE line (Active Player).
    auto has_live_line = [](const DebugEffectSheet &sheet) {
        for (size_t i = 0; i < sheet.effect_count; ++i)
            if (std::strncmp(sheet.effects[i], "LIVE: ", 6) == 0) return true;
        return false;
    };
    check(has_live_line(debug_preset_info(DebugPreset::Combat)), "P2: Combat preset discloses a LIVE line");
    check(has_live_line(debug_preset_info(DebugPreset::Shrine)), "P2: Shrine preset discloses a LIVE line");
    check(has_live_line(debug_preset_info(DebugPreset::Transport)), "P2: Transport preset discloses a LIVE line");
    check(has_live_line(debug_preset_info(DebugPreset::Endgame)), "P2: Endgame preset discloses a LIVE line");
    check(has_live_line(debug_preset_info(DebugPreset::SaveLoad)), "P2: SaveLoad preset discloses a LIVE line");
    // StockedInventory is deliberately quest-neutral -- it must NOT claim a
    // LIVE mutation (see PART 3's "quest-neutral tools only" contract).
    check(!has_live_line(debug_preset_info(DebugPreset::StockedInventory)),
          "P2: quest-neutral StockedInventory preset has no LIVE line");

    // C1 (metadata half): Certification has exactly five entries with the
    // exact PART 4 names, each carrying real effect metadata. The
    // UiDebugMenu-driven half of C1 (category/row wiring) lives in
    // ui_debug_menu_test.cpp per PART 15.
    check(size_t(DebugCertification::Count) == 5, "C1: exactly five Certification setups");
    static constexpr const char *kCertificationNames[] = {
        "Ship / Sails Test", "Dungeon Test", "Blackthorn Badge Test", "Flame / Shard Test", "Shop / NPC Test",
    };
    for (size_t i = 0; i < size_t(DebugCertification::Count); ++i) {
        const auto &sheet = debug_certification_info(DebugCertification(i));
        check(std::strcmp(sheet.display_name, kCertificationNames[i]) == 0, "C1: exact Certification name");
        check(sheet.effect_count > 0, "C1: Certification has at least one effect line");
        for (size_t j = 0; j < sheet.effect_count; ++j)
            check(sheet.effects[j] != nullptr && sheet.effects[j][0] != '\0', "C1: no null/empty effect line");
    }
    // Ship/Sails and Blackthorn Badge each disclose their explicit "does NOT
    // grant/wear" non-mutation, per PART 6/8.
    auto has_line_containing = [](const DebugEffectSheet &sheet, const char *needle) {
        for (size_t i = 0; i < sheet.effect_count; ++i)
            if (std::strstr(sheet.effects[i], needle)) return true;
        return false;
    };
    check(has_line_containing(debug_certification_info(DebugCertification::ShipSails), "Does NOT grant HMS Cape"),
          "C1: Ship/Sails sheet discloses HMS Cape is not granted");
    check(has_line_containing(debug_certification_info(DebugCertification::BlackthornBadge), "Does NOT wear"),
          "C1: Blackthorn Badge sheet discloses the badge is not worn");

    std::cout << checks << " debug labels checks passed\n";
}
