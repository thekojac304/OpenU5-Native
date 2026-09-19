#pragma once

#include "debug_developer.h"
#include "debug_map_picker.h"
#include "inventory.h"
#include "types.h"

#include <cstddef>
#include <cstdint>

namespace openu5 {

// Single-source label/metadata seam for the Developer debug UI (Batch 4.5A-2).
// This header is ESP-free, host-testable, and allocates nothing: every table
// is static/constexpr and every accessor is a plain function over scalars.
// It owns *generic* debug-menu vocabulary (root category names, status/result
// text, quest-item metadata, transport/equipment naming). It deliberately does
// not duplicate the authoritative per-item name tables in display_names.h
// (equipment/spell/scroll/potion/reagent) -- callers should keep using those
// directly for those domains.

// A label paired with the canonical gameplay identifier it refers to (distinct
// from any menu ordinal or internal enum ordinal). canonical_id is -1 when the
// label has no associated gameplay id.
struct DebugItemLabel {
    const char *name = "Unknown";
    int32_t canonical_id = -1;
};

// Root category names, concise enough for the T-Deck's debug row width. This
// is the one table both UiDebugMenu (breadcrumb/title) and device presentation
// consult -- no other copy may exist. The array and count are constexpr so
// UiDebugMenu.h can statically assert it stays in lockstep with
// UiDebugCategory::Count.
inline constexpr const char *kDebugRootCategoryNames[] = {
    "Teleport", "Party", "Stats", "Inventory", "Equipment", "Reagents",
    "Quest Items", "Special Items", "Quest / World", "Time", "Transport",
    "NPC / Dungeon", "Presets", "Diagnostics"};
inline constexpr size_t kDebugRootCategoryCount =
    sizeof(kDebugRootCategoryNames) / sizeof(kDebugRootCategoryNames[0]);
constexpr size_t debug_root_category_count() { return kDebugRootCategoryCount; }
const char *debug_root_category_name(size_t index);

// Diagnostics/smoke-test group names as surfaced by the Diagnostics category
// (index 0 there is the menu-only "Run All" entry, not a real group). This
// mirrors DeviceSmokeTests' own group table 1:1; that table stays device-local
// because it is tightly coupled to Scenario/group internals owned there (see
// GAMEPLAY_INTEGRATION_AUDIT.md, Batch 4.5A-2).
inline constexpr const char *kDebugDiagnosticGroupNames[] = {
    "Overworld", "Local Maps", "Dialogue", "Shops / Inns", "Inventory / Equipment",
    "Combat", "Dungeons", "Shrines / Special", "Transport", "Quest / Progression",
    "Persistence", "Device Input", "Debug Tools", "Resources", "Presentation"};
inline constexpr size_t kDebugDiagnosticGroupCount =
    sizeof(kDebugDiagnosticGroupNames) / sizeof(kDebugDiagnosticGroupNames[0]);
constexpr size_t debug_diagnostic_group_count() { return kDebugDiagnosticGroupCount; }
const char *debug_diagnostic_group_name(size_t index);

// DebugQuestItem carries no gameplay-facing name or id of its own -- it is an
// internal enum ordinal. This is the one place that maps it to the canonical
// item name and the real gameplay item id (never the same as the enum
// ordinal or the menu's own quest-item selector index).
DebugItemLabel debug_quest_item_label(DebugQuestItem item);

// DebugSpecialItem name + canonical id (Batch 4.5A-3 Part 13). Grapple's
// canonical_id is -1: it is Klimb-only (commands.cpp / dungeon.cpp pit logic),
// never a (U)se-item id, and must never be confused with real id 18 (the
// Amulet of Lord British) -- that exact confusion was R-07. The remaining six
// ids (32-37) match UsableItemPickerInput's own documented ids 1:1.
DebugItemLabel debug_special_item_label(DebugSpecialItem item);

// Composes "Name [id]" when canonical_id >= 0, else just "Name" (Grapple).
// Writes into caller-owned storage; buf is always null-terminated on success.
void debug_format_item_label(const char *name, int32_t canonical_id, char *buf, size_t buf_size);

// DebugStatus/DebugTeleportStatus presentation text. "Unsupported" (not
// "Unavailable") is the exact required wording for DebugStatus::Unsupported.
const char *debug_status_name(DebugStatus status);
const char *debug_teleport_status_name(DebugTeleportStatus status);
// Combines a teleport status with its passability metadata exactly like the
// prior device-side teleport_result_label() did (e.g. "Applied (impassable)"
// when an explicit manual teleport lands on an impassable cell); semantics
// are unchanged, only ownership moves here.
const char *debug_teleport_result_label(DebugTeleportStatus status, bool passability_known,
                                        bool passable);

const char *transport_mode_name(TransportMode mode);
const char *equipment_slot_name(EquipSlot slot);

// Signed-z floor identity established by Batch 4.5A-1: z=-1 -> "Basement",
// z=0 -> "Ground Floor", z>=1 -> "Level N". Writes into caller-owned storage
// (zero heap, host-testable); buf is always null-terminated on success.
void debug_format_teleport_floor_label(int16_t signed_z, char *buf, size_t buf_size);

// Character row presentation: the party member's own name when set, else a
// safe "Character N" fallback (N = the roster index). Writes into
// caller-owned storage; buf is always null-terminated on success.
void debug_format_character_label(const char *name, size_t index, char *buf, size_t buf_size);

} // namespace openu5
