// Batch 24 host-test seam; two generations since Batch 28; since A4-SAVE2
// three logical slots of two generations each (ALPHA4_UI.md section 4).
//
// AlphaSaveService (alpha_save.h, unmodified) with the SD card replaced by two
// in-memory generation slots, so a host test can take the device's real Save
// and Load routes (Alt+S / Alt+L, the system menu and the title screen, all
// inside the real AlphaRuntime) and observe what a reload does to the world.
//
// Only the storage shell differs from alpha_save.cpp:
//   save()  -- the same capture chain: since Batch 53 the production
//              capture_save_document() itself (alpha_save_generation.cpp),
//              export_native_state over the INIT.GAM template, build_ool,
//              encode_json; the generation goes to the slot the production
//              choose_save_target() names (A4-SAVE1: never the newest
//              generation the gate accepts) with a
//              commit record carrying the three CRCs, then the post-write
//              self-check verify_candidate(), exactly as the device does.
//   load(), load_slot(), inspect_catalog(), inspect_generations() -- read
//              the slots and hand them to the
//              PRODUCTION alpha_save_generation.cpp (verify_candidate,
//              restore_candidate, restore_newest): CRC, parse, semantic
//              validation, generation choice and fallback are not copied here.
// Not reproduced: files, temp+rename, fsync, the PSRAM scratch, timing.
//
// Never linked into firmware: only host CMake targets compile it.
#include "openu5/frontend_settings.h"
#include "alpha_save.h"
#include "alpha_save_generation.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace tdeck {

namespace {
constexpr int kSlots = AlphaSaveService::kSlots;
struct Slot {
    bool present = false;
    AlphaSaveCommit commit{};
    std::vector<uint8_t> gam, ool, json;
};
Slot g_slots[kSlots][2];
AlphaSaveCandidate g_candidates[2];
AlphaSaveStage g_stage;

// alpha_save.cpp candidate(): read the commit and the three files, then verify.
bool read_slot(int slot, int gen, AlphaSaveCandidate &v) {
    v = AlphaSaveCandidate{};
    const auto &s = g_slots[slot][gen];
    if (!s.present) return false;
    v.commit = s.commit; v.gam = s.gam; v.ool = s.ool; v.json = s.json;
    return true;
}
bool candidate(int slot, int gen, AlphaSaveCandidate &v) { return read_slot(slot, gen, v) && verify_candidate(v, g_stage); }
bool present_of(int slot, bool (&present)[2], AlphaSaveCommit (&commits)[2]) {
    for (int i = 0; i < 2; ++i) { present[i] = g_slots[slot][i].present; commits[i] = g_slots[slot][i].commit; }
    return present[0] || present[1];
}
int newest_gen(int slot) {
    bool present[2]; AlphaSaveCommit commits[2];
    present_of(slot, present, commits);
    return newest_committed_slot(present, commits);
}
// The slots in alpha_save.cpp's Continue order: newest commit first, a tie to
// the lower slot.
int continue_order(int (&order)[kSlots]) {
    int n = 0;
    for (int k = 0; k < kSlots; ++k) {
        const int g = newest_gen(k);
        if (g < 0) continue;
        int at = n++;
        while (at > 0 && g_slots[order[at - 1]][newest_gen(order[at - 1])].commit.sequence < g_slots[k][g].commit.sequence) {
            order[at] = order[at - 1]; --at;
        }
        order[at] = k;
    }
    return n;
}
// The journey the seams act on: the slot holding the card's newest commit.
int journey_slot() { int order[kSlots]; return continue_order(order) > 0 ? order[0] : 0; }
int newest_slot() { return newest_gen(journey_slot()); }
int older_slot() {
    const int k = journey_slot(), n = newest_gen(k);
    return n >= 0 && g_slots[k][1 - n].present ? 1 - n : -1;
}
int restore_slot(int slot, openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                 openu5::NpcActors &a, openu5::save::Json &retained) {
    for (int i = 0; i < 2; ++i) candidate(slot, i, g_candidates[i]);
    return restore_newest(g_candidates, c, o, t, a, retained, g_stage);
}
} // namespace

// Test seams. A test declares these itself; nothing in production calls them.
// A4-SAVE2: they act on the journey slot (the one holding the card's newest
// commit); a card written by one journey is Slot 1, as before.
//   forget  -- no generation at all (a missing save);
//   damage  -- the newest generation's sidecar file cut in half with its
//              commit left alone: a torn or corrupt file, rejected by the CRC
//              in complete_generation (Batch 27's unreadable-save control);
//   gam     -- Batch 53: read the newest generation's GAM bytes.
//   edit    -- Batch 28 (H-166): parse the chosen generation's sidecar, let
//              the test change sidecar.gameState, re-encode it and re-seal the
//              commit CRC. The result is a well-formed, CRC-consistent
//              generation whose CONTENT is wrong, the class a CRC cannot see.
void host_memory_save_forget_for_test() {
    for (auto &slot : g_slots) { slot[0] = Slot{}; slot[1] = Slot{}; }
}
void host_memory_save_damage_for_test() {
    const int k = journey_slot(), n = newest_slot();
    if (n >= 0) g_slots[k][n].json.resize(g_slots[k][n].json.size() / 2);
}
// A4-SAVE1: the same tear, applied to the older generation.
void host_memory_save_damage_older_for_test() {
    const int k = journey_slot(), o = older_slot();
    if (o >= 0) g_slots[k][o].json.resize(g_slots[k][o].json.size() / 2);
}
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state)) {
    const int k = journey_slot();
    const int slot = newest ? newest_slot() : older_slot();
    if (slot < 0) return false;
    auto &s = g_slots[k][slot];
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
int host_memory_save_generations_for_test() {
    int n = 0;
    for (const auto &slot : g_slots) n += int(slot[0].present) + int(slot[1].present);
    return n;
}
// Batch 53: the newest generation's GAM bytes as written, so a test can check
// the on-card format itself (the Word-of-Power bits at 0x32a, the stones at
// 0x28a) and not only what this runtime reads back.
std::vector<uint8_t> host_memory_save_gam_for_test() {
    const int k = journey_slot(), n = newest_slot();
    return n >= 0 ? g_slots[k][n].gam : std::vector<uint8_t>{};
}

bool AlphaSaveService::reserve_dma_headroom() { return true; }

bool AlphaSaveService::save(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, const uint8_t *base,
                             size_t base_size, const uint8_t *base_ool, size_t base_ool_size, uint32_t &elapsed_ms,
                             bool new_journey, int requested_slot) {
    elapsed_ms = 0;
    last_failure_[0] = 0;
    if (requested_slot < -1 || requested_slot >= kSlots) {
        std::snprintf(last_failure_, sizeof(last_failure_), "No such save slot %d", requested_slot);
        return false;
    }
    capture_save_document(c, o, t, a, retained);   // Batch 53: the production chain, not a copy
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
    // A4-SAVE2: the logical slot (-1: the one holding the card's newest
    // commit) and the card-wide sequence.
    const int logical = requested_slot >= 0 ? requested_slot : journey_slot();
    uint64_t card_newest = 0;
    for (const auto &k : g_slots)
        for (const auto &g : k)
            if (g.present && g.commit.sequence > card_newest) card_newest = g.commit.sequence;
    // A4-SAVE1: the production target rule (alpha_save_generation.cpp): the
    // newest generation is kept only when the gate accepts it.
    bool present[2];
    AlphaSaveCommit commits[2];
    present_of(logical, present, commits);
    const int newest = newest_committed_slot(present, commits);
    const bool newest_ok = newest >= 0 && candidate(logical, newest, g_candidates[newest]);
    const AlphaSaveTarget target = choose_save_target(present, commits, newest_ok, card_newest);
    const uint64_t sequence = target.sequence;
    const int slot = target.slot;
    Slot next;
    next.present = true;
    next.gam.assign(gam.begin(), gam.end());
    next.ool.assign(ool.begin(), ool.end());
    next.json.assign(json.begin(), json.end());
    next.commit.sequence = sequence;
    next.commit.gam = openu5::save::save_crc32(next.gam.data(), next.gam.size());
    next.commit.ool = openu5::save::save_crc32(next.ool.data(), next.ool.size());
    next.commit.json = openu5::save::save_crc32(next.json.data(), next.json.size());
    g_slots[logical][slot] = std::move(next);  // the commit rename: the generation now exists
    // alpha_save.cpp "semantic-validation" stage, after the commit rename.
    if (!candidate(logical, slot, g_candidates[slot])) {
        std::snprintf(last_failure_, sizeof(last_failure_), "%s", "semantic-validation failed (errno 0)");
        return false;
    }
    last_slot_ = logical;
    return true;
}

// A4-UI3: alpha_save.cpp's restore_slot `older` -- the other generation's commit is newer.
static bool restored_older(int slot, int gen) {
    bool present[2];
    AlphaSaveCommit commits[2];
    present_of(slot, present, commits);
    return present[1 - gen] && commits[1 - gen].sequence > commits[gen].sequence;
}

bool AlphaSaveService::load(openu5::CommandContext &c, openu5::OutdoorServices &o, openu5::WorldTerrain &t,
                             openu5::NpcActors &a, openu5::save::Json &retained, uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    int order[kSlots];
    const int n = continue_order(order);
    for (int i = 0; i < n; ++i) {
        const int gen = restore_slot(order[i], c, o, t, a, retained);
        if (gen >= 0) { last_slot_ = order[i]; last_load_recovered_ = i > 0 || restored_older(order[i], gen); return true; }
    }
    return false;
}

bool AlphaSaveService::load_slot(int slot, openu5::CommandContext &c, openu5::OutdoorServices &o,
                                  openu5::WorldTerrain &t, openu5::NpcActors &a, openu5::save::Json &retained,
                                  uint32_t &elapsed_ms) {
    elapsed_ms = 0;
    if (slot < 0 || slot >= kSlots) return false;
    const int gen = restore_slot(slot, c, o, t, a, retained);
    if (gen < 0) return false;
    last_slot_ = slot;
    last_load_recovered_ = restored_older(slot, gen);
    return true;
}

void AlphaSaveService::inspect_generations(int k, openu5::FrontendSaveSlot (&slots)[2]) {
    for (auto &slot : slots) slot = openu5::FrontendSaveSlot{};
    if (k < 0 || k >= kSlots) return;
    for (int i = 0; i < 2; ++i) {
        auto &v = g_candidates[i];
        if (!candidate(k, i, v)) {
            slots[i].present = g_slots[k][i].present;
            slots[i].sequence = g_slots[k][i].present ? g_slots[k][i].commit.sequence : 0; // as alpha_save.cpp
            continue;
        }
        slots[i] = summarize_candidate(v, g_stage); // the production summary (A4-UI2: place, date, party)
    }
}

// As alpha_save.cpp inspect_catalog: the newest generation, and the one before
// it only when the newest is refused (no save-list memory here).
void AlphaSaveService::inspect_catalog(openu5::FrontendSaveCatalog &out) {
    out = openu5::FrontendSaveCatalog{};
    for (int k = 0; k < kSlots; ++k) {
        auto &e = out.slots[k];
        const int n = newest_gen(k);
        if (n < 0) continue;
        e.sequence = g_slots[k][n].commit.sequence;
        if (candidate(k, n, g_candidates[n])) {
            e.status = openu5::SaveSlotStatus::Saved; e.shown = summarize_candidate(g_candidates[n], g_stage); continue;
        }
        if (g_slots[k][1 - n].present && candidate(k, 1 - n, g_candidates[1 - n])) {
            e.status = openu5::SaveSlotStatus::Recovered; e.shown = summarize_candidate(g_candidates[1 - n], g_stage); continue;
        }
        e.status = openu5::SaveSlotStatus::Damaged;
    }
}

// A3-01: an opt-in in-memory settings.json. Off by default, so every older
// test still runs with no settings file at all. On, it holds the exact text
// the production service would write (encode_settings) and reads it back the
// way the production service does (decode_settings), so a second runtime in
// the same process is a reboot of the first.
std::string &a3_host_settings_text() { static std::string text; return text; }
bool &a3_host_settings_enabled() { static bool enabled = false; return enabled; }
// Alpha 4 A4-SAVE3: no card here, so no PC save to import and nothing to export.
AlphaSaveService::PcRead AlphaSaveService::read_pc_import(PcSource &source) {
    source = PcSource{};
    return PcRead::NoFiles;
}
int AlphaSaveService::pc_import_marker(uint32_t, uint32_t) { return -1; }
bool AlphaSaveService::record_pc_import(uint32_t, uint32_t, int) { return false; }
AlphaSaveService::PcExport AlphaSaveService::export_pc_save(int, PcExportResult &result) {
    result = PcExportResult{};
    return PcExport::NoLoadable;
}

bool AlphaSettingsService::load(openu5::FrontendSettings &s) const {
    return a3_host_settings_enabled() && openu5::decode_settings(a3_host_settings_text(), s);
}
bool AlphaSettingsService::save(const openu5::FrontendSettings &s) const {
    if (!a3_host_settings_enabled()) return false;
    std::string text;
    if (!openu5::encode_settings(s, text)) return false;
    a3_host_settings_text() = text;
    return true;
}

} // namespace tdeck
