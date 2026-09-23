// Batch 24 host-test seam; two generations since Batch 28.
//
// AlphaSaveService (alpha_save.h, unmodified) with the SD card replaced by two
// in-memory generation slots, so a host test can take the device's real Save
// and Load routes (Alt+S / Alt+L, the system menu and the title screen, all
// inside the real AlphaRuntime) and observe what a reload does to the world.
//
// Only the storage shell differs from alpha_save.cpp:
//   save()  -- the same capture chain (capture_gameplay, capture_terrain,
//              capture_npc_walk, capture_world_objects, capture_dungeon),
//              export_native_state over the INIT.GAM template, build_ool,
//              encode_json; the generation goes to slot (newest+1)&1 with a
//              commit record carrying the three CRCs, then the post-write
//              self-check verify_candidate(), exactly as the device does.
//   load(), load_slot(), inspect() -- read the slots and hand them to the
//              PRODUCTION alpha_save_generation.cpp (verify_candidate,
//              restore_candidate, restore_newest): CRC, parse, semantic
//              validation, generation choice and fallback are not copied here.
// Not reproduced: files, temp+rename, fsync, the PSRAM scratch, timing.
//
// Never linked into firmware: only host CMake targets compile it.
#include "alpha_save.h"
#include "alpha_save_generation.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace tdeck {

namespace {
struct Slot {
    bool present = false;
    AlphaSaveCommit commit{};
    std::vector<uint8_t> gam, ool, json;
};
Slot g_slots[2];
AlphaSaveCandidate g_candidates[2];
AlphaSaveStage g_stage;

// alpha_save.cpp candidate(): read the commit and the three files, then verify.
bool read_slot(int slot, AlphaSaveCandidate &v) {
    v = AlphaSaveCandidate{};
    const auto &s = g_slots[slot];
    if (!s.present) return false;
    v.commit = s.commit; v.gam = s.gam; v.ool = s.ool; v.json = s.json;
    return true;
}
bool candidate(int slot, AlphaSaveCandidate &v) { return read_slot(slot, v) && verify_candidate(v, g_stage); }
int newest_slot() {
    int best = -1;
    for (int i = 0; i < 2; ++i)
        if (g_slots[i].present && (best < 0 || g_slots[i].commit.sequence > g_slots[best].commit.sequence)) best = i;
    return best;
}
int older_slot() {
    const int n = newest_slot();
    return n >= 0 && g_slots[1 - n].present ? 1 - n : -1;
}
} // namespace

// Test seams. A test declares these itself; nothing in production calls them.
//   forget  -- no generation at all (a missing save);
//   damage  -- the newest generation's sidecar file cut in half with its
//              commit left alone: a torn or corrupt file, rejected by the CRC
//              in complete_generation (Batch 27's unreadable-save control);
//   edit    -- Batch 28 (H-166): parse the chosen generation's sidecar, let
//              the test change sidecar.gameState, re-encode it and re-seal the
//              commit CRC. The result is a well-formed, CRC-consistent
//              generation whose CONTENT is wrong, the class a CRC cannot see.
void host_memory_save_forget_for_test() { g_slots[0] = Slot{}; g_slots[1] = Slot{}; }
void host_memory_save_damage_for_test() {
    const int n = newest_slot();
    if (n >= 0) g_slots[n].json.resize(g_slots[n].json.size() / 2);
}
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state)) {
    const int slot = newest ? newest_slot() : older_slot();
    if (slot < 0) return false;
    auto &s = g_slots[slot];
    openu5::save::Json side;
    if (openu5::save::parse_json(std::string(s.json.begin(), s.json.end()), side) != openu5::save::JsonError::None)
        return false;
    edit(side["gameState"]);
    std::string text;
    if (openu5::save::encode_json(side, text) != openu5::save::JsonError::None) return false;
    s.json.assign(text.begin(), text.end());
    s.commit.json = openu5::save::save_crc32(s.json.data(), s.json.size());
    return true;
}
int host_memory_save_generations_for_test() { return int(g_slots[0].present) + int(g_slots[1].present); }

bool AlphaSaveService::reserve_dma_headroom() { return true; }

bool AlphaSaveService::save(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, const uint8_t *base,
                             size_t base_size, const uint8_t *base_ool, size_t base_ool_size, uint32_t &elapsed_ms,
                             bool new_journey) {
    elapsed_ms = 0;
    last_failure_[0] = 0;
    openu5::save::capture_gameplay(c.commands, o, retained);
    openu5::save::capture_terrain(t, retained);
    openu5::save::capture_npc_walk(a, c.game.position.map.location, retained);
    if (c.quest_world) openu5::save::capture_world_objects(*c.quest_world, retained);
    if (c.dungeon_context) openu5::save::capture_dungeon(c.dungeon_context->state, retained);
    openu5::save::Gam gam{};
    openu5::save::Json side;
    if (openu5::save::export_native_state(c.game, c.turn, retained, base, base_size, gam, side, true) !=
        openu5::save::Error::None) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host memory save: gam-encode failed");
        return false;
    }
    if (new_journey) openu5::save::preserve_new_journey_template_bytes(gam, base, base_size);
    const auto ool = openu5::save::build_ool(retained, base_ool, base_ool_size);
    std::string json;
    if (ool.size() != openu5::save::kOolSize ||
        openu5::save::encode_json(side, json) != openu5::save::JsonError::None) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "host memory save: ool/json-encode failed");
        return false;
    }
    const int newest = newest_slot();
    const uint64_t sequence = newest >= 0 ? g_slots[newest].commit.sequence + 1 : 1;
    const int slot = int(sequence & 1);
    Slot next;
    next.present = true;
    next.gam.assign(gam.begin(), gam.end());
    next.ool.assign(ool.begin(), ool.end());
    next.json.assign(json.begin(), json.end());
    next.commit.sequence = sequence;
    next.commit.gam = openu5::save::save_crc32(next.gam.data(), next.gam.size());
    next.commit.ool = openu5::save::save_crc32(next.ool.data(), next.ool.size());
    next.commit.json = openu5::save::save_crc32(next.json.data(), next.json.size());
    g_slots[slot] = std::move(next);  // the commit rename: the generation now exists
    // alpha_save.cpp "semantic-validation" stage, after the commit rename.
    if (!candidate(slot, g_candidates[slot])) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "semantic-validation failed (errno 0)");
        return false;
    }
    return true;
}

bool AlphaSaveService::load(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    for (int i = 0; i < 2; ++i) candidate(i, g_candidates[i]);
    return restore_newest(g_candidates, c, o, t, a, retained, g_stage) >= 0;
}

bool AlphaSaveService::load_slot(int slot, openu5::CommandContext &c, openu5::OutdoorServices &o,
                                  openu5::WorldTerrain &t, openu5::NpcActors &a, openu5::save::Json &retained,
                                  uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    if (slot < 0 || slot > 1) return false;
    auto &pick = g_candidates[slot];
    if (!candidate(slot, pick)) return false;
    return restore_candidate(pick, c, o, t, a, retained, g_stage);
}

void AlphaSaveService::inspect(openu5::FrontendSaveSlot (&slots)[2]) {
    for (auto &slot : slots) slot = openu5::FrontendSaveSlot{};
    for (int i = 0; i < 2; ++i) {
        auto &v = g_candidates[i];
        if (!candidate(i, v)) { slots[i].present = g_slots[i].present; continue; }
        slots[i].present = slots[i].valid = true;
        slots[i].sequence = v.commit.sequence;
        if (g_stage.game.party.character_count)
            std::snprintf(slots[i].name, sizeof(slots[i].name), "%.9s", g_stage.game.party.characters[0].name);
    }
}

bool AlphaSettingsService::load(openu5::FrontendSettings &) const { return false; }
bool AlphaSettingsService::save(const openu5::FrontendSettings &) const { return false; }

} // namespace tdeck
