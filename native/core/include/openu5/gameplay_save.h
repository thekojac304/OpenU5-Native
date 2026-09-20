#pragma once
#include "persistence.h"
#include "outdoor.h"
#include "world_terrain.h"
#include "dungeon.h"
namespace openu5::save {
// Patch the retained reference document from the existing live owners before
// save_state/export_native_state. World-object/quest owners retain their own
// existing adapters. No filesystem operations or second live world are created.
void capture_gameplay(const CommandState &,const OutdoorServices &,Json &);
// Transactional domain validation; only commits these owners after all entries
// validate. Call beside restore_core when restoring the same buffer document.
Error restore_gameplay(const Json &,CommandState &,OutdoorServices &);
void capture_terrain(const WorldTerrain &,Json &);
Error restore_terrain(const Json &,WorldTerrain &);
// R-14: `objects_` (combat-promoted chests, search-revealed items, spilled
// loot, docked ships/horses...) is a single pool with no owner-side backing
// store, unlike terrain/enemies/doors above. Captured/restored as one opaque
// snapshot; the caller (synchronize_loaded_world) owns clearing the live pool
// first so a failed/absent restore leaves it empty rather than stale.
void capture_world_objects(const QuestWorldServices &,Json &);
Error restore_world_objects(const Json &,QuestWorldServices &);
// R-15: a dungeon session (`DungeonState`) is caller-owned runtime state, not
// part of GameState/CommandState. Absent from the document (surface save, the
// common case) restores to an inactive default. Present-but-invalid is a
// domain error: `pos.floor/x/y` and `facing` index fixed-size arrays
// (dungeon cell grid, direction deltas) with no bounds check downstream.
void capture_dungeon(const DungeonState &,Json &);
Error restore_dungeon(const Json &,DungeonState &);
// FONT.OVL creates the first SAVED.GAM by modifying INIT.GAM in place. The
// generic exporter refreshes an inactive runtime object header at 0x6b4 even
// on a local map; preserve INIT's bytes for this one creation-only commit.
void preserve_new_journey_template_bytes(Gam &, const uint8_t *base, size_t length);
}
