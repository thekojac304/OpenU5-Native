// Batch 24 host-test seam.
//
// AlphaSaveService (alpha_save.h, unmodified) with the SD card replaced by one
// in-memory generation, so a host test can take the device's real Save and
// Load routes (Alt+S / Alt+L and the system menu, all inside the real
// AlphaRuntime) and observe what a reload does to the live world.
//
// Only the storage shell differs from alpha_save.cpp. The semantic chain is
// that file's, call for call:
//   save()  -- capture_gameplay, capture_terrain, capture_npc_walk,
//              capture_world_objects, capture_dungeon, export_native_state
//              over the INIT.GAM template, then encode_json of the sidecar;
//   load()  -- restore_candidate(): load_native_state of the .GAM bytes and
//              sidecar into scratch copies of the live owners, then
//              restore_gameplay, restore_terrain, restore_npc_walk, and only
//              on success the commit into the live owners.
// Not reproduced: the .OOL (build_ool writes a file nothing on the load side
// reads), generation/CRC selection (one generation, no fallback) and the
// PSRAM scratch. Everything AlphaRuntime then does after a load --
// synchronize_loaded_world() or the Alt+L arm -- is production code.
//
// Never linked into firmware: only the batch24 CMake host target compiles it.
#include "alpha_save.h"

#include <cstdio>
#include <string>

namespace tdeck {

namespace {
struct Generation {
    bool present = false;
    openu5::save::Gam gam{};
    std::string sidecar;
};
Generation g_generation;
} // namespace

// Batch 27 (H-164) failure seams. A test declares these itself; nothing in
// production or in the other host targets calls them.
//   forget -- no generation at all (a missing save);
//   damage -- the stored sidecar cut in half, so load_native_state's parse
//             rejects it (a corrupt save). Not H-166: a domain-invalid but
//             well-formed payload is a different path and is not exercised.
void host_memory_save_forget_for_test() { g_generation = Generation{}; }
void host_memory_save_damage_for_test() { g_generation.sidecar.resize(g_generation.sidecar.size() / 2); }

bool AlphaSaveService::reserve_dma_headroom() { return true; }

bool AlphaSaveService::save(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, const uint8_t *base,
                             size_t base_size, const uint8_t *, size_t, uint32_t &elapsed_ms, bool new_journey) {
    elapsed_ms = 0;
    last_failure_[0] = 0;
    openu5::save::capture_gameplay(c.commands, o, retained);
    openu5::save::capture_terrain(t, retained);
    openu5::save::capture_npc_walk(a, c.game.position.map.location, retained);
    if (c.quest_world) openu5::save::capture_world_objects(*c.quest_world, retained);
    if (c.dungeon_context) openu5::save::capture_dungeon(c.dungeon_context->state, retained);
    Generation next;
    openu5::save::Json side;
    if (openu5::save::export_native_state(c.game, c.turn, retained, base, base_size, next.gam, side, true) !=
        openu5::save::Error::None) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host memory save: gam-encode failed");
        return false;
    }
    if (new_journey) openu5::save::preserve_new_journey_template_bytes(next.gam, base, base_size);
    if (openu5::save::encode_json(side, next.sidecar) != openu5::save::JsonError::None) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host memory save: json-encode failed");
        return false;
    }
    next.present = true;
    g_generation = std::move(next);
    return true;
}

bool AlphaSaveService::load(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    if (!g_generation.present) return false;
    // restore_candidate(), transactional: scratch copies of every live owner.
    openu5::GameState game = c.game;
    openu5::TurnState turn = c.turn;
    openu5::CommandState commands = c.commands;
    openu5::OutdoorServices outdoor = o;
    openu5::WorldTerrain terrain = t;
    openu5::NpcActors actors = a;
    openu5::save::Json document;
    openu5::save::SidecarSource source;
    auto err = openu5::save::load_native_state(g_generation.gam.data(), g_generation.gam.size(),
                                               &g_generation.sidecar, game, turn, document, source, true);
    if (err == openu5::save::Error::None) err = openu5::save::restore_gameplay(document, commands, outdoor);
    if (err == openu5::save::Error::None) err = openu5::save::restore_terrain(document, terrain);
    if (err == openu5::save::Error::None)
        err = openu5::save::restore_npc_walk(document, game.position.map.location, true, actors);
    if (err != openu5::save::Error::None) return false;
    c.game = std::move(game); c.turn = std::move(turn); c.commands = std::move(commands);
    o = std::move(outdoor); t = std::move(terrain); a = std::move(actors); retained = std::move(document);
    return true;
}

bool AlphaSaveService::load_slot(int slot, openu5::CommandContext &c, openu5::OutdoorServices &o,
                                  openu5::WorldTerrain &t, openu5::NpcActors &a, openu5::save::Json &retained,
                                  uint32_t &elapsed_ms) {
    return slot == 0 && load(c, o, t, a, retained, elapsed_ms);
}

void AlphaSaveService::inspect(openu5::FrontendSaveSlot (&slots)[2]) {
    slots[0] = openu5::FrontendSaveSlot{};
    slots[1] = openu5::FrontendSaveSlot{};
    slots[0].present = slots[0].valid = g_generation.present;
}

bool AlphaSettingsService::load(openu5::FrontendSettings &) const { return false; }
bool AlphaSettingsService::save(const openu5::FrontendSettings &) const { return false; }

} // namespace tdeck
