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
// H-166: restore_world_objects' own rules, with no pool to write. The save
// generation gate runs this (and restore_dungeon into scratch) BEFORE a load
// commits, because the two restores themselves only run afterwards.
Error validate_world_objects(const Json &);
// Batch 53 (RB-3 / H-188). The eight moonstones are GAM state, not a sidecar
// field: 0x28a x, 0x292 y, 0x29a location (0xFF = carried), 0x2a2 floor --
// DS 0x5830..0x5848, the four arrays kernel 0x47f4 teleports through.
// load_native_state already imports them into the document's "moonstones" and
// export_native_state writes them back. QuestWorldServices::moonstones is the
// runtime owner commands mutate; these two move it to and from that document,
// so there is no second model and no new save field. capture writes all
// `moonstone_count` stones; restore is all-or-nothing and needs 8 valid ones.
void capture_moonstones(const QuestWorldServices &,Json &);
Error restore_moonstones(const Json &,QuestWorldServices &);
Error validate_moonstones(const Json &);
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
