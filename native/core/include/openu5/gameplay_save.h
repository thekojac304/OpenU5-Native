#pragma once
#include "persistence.h"
#include "outdoor.h"
#include "world_terrain.h"
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
// FONT.OVL creates the first SAVED.GAM by modifying INIT.GAM in place. The
// generic exporter refreshes an inactive runtime object header at 0x6b4 even
// on a local map; preserve INIT's bytes for this one creation-only commit.
void preserve_new_journey_template_bytes(Gam &, const uint8_t *base, size_t length);
}
