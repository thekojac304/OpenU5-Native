// Alpha 4 A4-SAVE1 -- recovery hardening: which generation a save may overwrite.
//
// The REAL alpha_save.cpp over the fake SD card (host_tests/sd_shims, as
// a3_04g_storage_runtime): "/sd/..." is a host directory, faults are injected
// by path, and the card's files are read and damaged byte for byte. Saves and
// loads go through AlphaSaveService itself -- a fresh service object is a
// reboot (nothing it knew before counts) -- and, in group R, through the real
// AlphaRuntime's Alt+S / Alt+L / System Menu, whose one service keeps its save
// list between calls.
//
// The invariant (ALPHA4_UI.md section 3): until a newly written generation is
// written, read back, committed and accepted, the generation Continue would
// restore stays byte for byte on the card and loadable. A save writes over
// the other slot only when the newest generation by sequence is ACCEPTED;
// when that one is refused, it is the expendable one.
//
// Groups: A rotation, B fallback, C fallback then Save (the defect), D faults
// during that save, E rotation afterwards, F both refused, G sequence and slot
// identity, R the runtime's own service with a warm save list, H heap.
#include "../main/alpha_runtime.h"
#include "../main/alpha_save.h"
#include "../main/alpha_save_generation.h"
#include "esp_heap_caps.h"
#include "sd_shims/host_sd_card.h"

#include "openu5/save_json.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

// ---------------------------------------------------------------- heap census
// As a3_04g: "small" (<= 4 KiB) allocations are internal RAM first on the device.
namespace {
struct HeapCensus {
    uint64_t live = 0, live_small = 0, peak_small = 0;
};
HeapCensus g_heap;
constexpr size_t kSmall = 4096;
constexpr size_t kHeader = 16;
void *census_alloc(size_t n) {
    if (n > (size_t(1) << 40)) throw std::bad_alloc();
    auto *p = static_cast<size_t *>(std::malloc(n + kHeader));
    if (!p) throw std::bad_alloc();
    p[0] = n;
    g_heap.live += n;
    if (n <= kSmall) {
        g_heap.live_small += n;
        if (g_heap.live_small > g_heap.peak_small) g_heap.peak_small = g_heap.live_small;
    }
    return reinterpret_cast<char *>(p) + kHeader;
}
void census_free(void *q) noexcept {
    if (!q) return;
    auto *p = reinterpret_cast<size_t *>(static_cast<char *>(q) - kHeader);
    g_heap.live -= p[0];
    if (p[0] <= kSmall) g_heap.live_small -= p[0];
    std::free(p);
}
} // namespace
void *operator new(size_t n) { return census_alloc(n); }
void *operator new[](size_t n) { return census_alloc(n); }
void operator delete(void *p) noexcept { census_free(p); }
void operator delete[](void *p) noexcept { census_free(p); }
void operator delete(void *p, size_t) noexcept { census_free(p); }
void operator delete[](void *p, size_t) noexcept { census_free(p); }

// ------------------------------------------------------------------ harness
using namespace openu5;
using tdeck::RawInputKind;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

const tdeck::AlphaResourceOwners *g_owners = nullptr;
size_t g_dungeon_count = 0;

std::string card_path(int slot, const char *ext) {
    char p[96];
    std::snprintf(p, sizeof(p), "%s/alpha1-g%d.%s", tdeck::AlphaSaveService::kDirectory, slot, ext);
    return p;
}

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;
    size_t mark = 0;
    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.dungeons = g_owners->dungeons; hf.dungeon_count = g_dungeon_count;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30;
        g.party.active_character = 255;
        g.time.hour = 12; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.karma = 50; g.gold = 321;
        g.position.map = {0, 0};
    }
    GameState &g() { return rt->game(); }
    bool raw_key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent raw{};
        raw.kind = RawInputKind::Keyboard; raw.code = code; raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    void set_mark() { mark = rt->ui()->transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }
    bool alt_save() { set_mark(); raw_key('s', true); return saw("Save complete"); }
    bool alt_load() { set_mark(); raw_key('l', true); return saw("Load complete"); }
    bool open_menu() { raw_key('m', true); return rt->system_menu_open(); }
    bool close_menu() { raw_key('\b'); return !rt->system_menu_open(); }
};

// A service over the runtime's CommandContext and owners of its own. A new
// Service object is a reboot: an empty save list, nothing remembered.
struct Owners {
    OutdoorServices outdoor{};
    WorldTerrain terrain{};
    NpcActors actors{};
    save::Json retained{};
    Owners() {
        GameState g{};
        TurnState t{};
        save::SidecarSource source;
        save::load_native_state(g_owners->initial_gam, g_owners->initial_gam_size, nullptr, g, t, retained, source,
                                true);
    }
};
Harness *g_h = nullptr;
Owners *g_o = nullptr;
bool save_with(tdeck::AlphaSaveService &svc, uint32_t gold) {
    g_h->g().gold = static_cast<uint16_t>(gold);
    uint32_t ms = 0;
    return svc.save(g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained,
                    g_owners->initial_gam, g_owners->initial_gam_size, g_owners->initial_ool,
                    g_owners->initial_ool_size, ms);
}
bool load_with(tdeck::AlphaSaveService &svc) {
    uint32_t ms = 0;
    return svc.load(g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained, ms);
}
// Continue after a reboot: the gold it restored, or -1 when nothing loads.
// The live gold is poisoned first, so a refused load is visible.
int boot_load() {
    tdeck::AlphaSaveService fresh;
    g_h->g().gold = 9999;
    return load_with(fresh) ? int(g_h->g().gold) : -1;
}

// ------------------------------------------------------------------ the card
struct SlotFiles {
    std::string gam, ool, json, commit;
    bool operator==(const SlotFiles &o) const {
        return gam == o.gam && ool == o.ool && json == o.json && commit == o.commit;
    }
};
SlotFiles take(int slot) {
    SlotFiles f;
    host_sd::read_card_file(card_path(slot, "gam").c_str(), f.gam);
    host_sd::read_card_file(card_path(slot, "ool").c_str(), f.ool);
    host_sd::read_card_file(card_path(slot, "json").c_str(), f.json);
    host_sd::read_card_file(card_path(slot, "commit").c_str(), f.commit);
    return f;
}
void put(int slot, const SlotFiles &f) {
    host_sd::write_card_file(card_path(slot, "gam").c_str(), f.gam);
    host_sd::write_card_file(card_path(slot, "ool").c_str(), f.ool);
    host_sd::write_card_file(card_path(slot, "json").c_str(), f.json);
    host_sd::write_card_file(card_path(slot, "commit").c_str(), f.commit);
}
struct CardState {
    SlotFiles slot[2];
    void take_all() { slot[0] = take(0); slot[1] = take(1); }
    void put_all() const {
        host_sd::format_card();   // a blank card has no saves directory: make it
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(host_sd::root()) / "sd/ultima5/saves", ec);
        put(0, slot[0]);
        put(1, slot[1]);
    }
};
// The commit record's sequence, or 0 when the slot has none (fields, never
// memcmp: the record's padding is written uninitialised).
uint64_t seq(int slot) {
    std::string c;
    if (!host_sd::read_card_file(card_path(slot, "commit").c_str(), c) || c.size() != sizeof(tdeck::AlphaSaveCommit))
        return 0;
    tdeck::AlphaSaveCommit r;
    std::memcpy(&r, c.data(), sizeof(r));
    return r.sequence;
}
void set_seq(int slot, uint64_t sequence) {
    std::string c;
    host_sd::read_card_file(card_path(slot, "commit").c_str(), c);
    tdeck::AlphaSaveCommit r;
    std::memcpy(&r, c.data(), sizeof(r));
    r.sequence = sequence;
    host_sd::write_card_file(card_path(slot, "commit").c_str(), std::string(reinterpret_cast<char *>(&r), sizeof(r)));
}
// Torn: the sidecar cut in half under an intact commit (the CRC gate refuses).
void tear(int slot) {
    std::string j;
    host_sd::read_card_file(card_path(slot, "json").c_str(), j);
    host_sd::write_card_file(card_path(slot, "json").c_str(), j.substr(0, j.size() / 2));
}
// Well-formed and CRC-consistent, but refused by the semantic gate
// (validate_world_objects): the class a CRC cannot see (Batch 28).
bool refuse_semantically(int slot) {
    std::string j;
    host_sd::read_card_file(card_path(slot, "json").c_str(), j);
    save::Json side;
    if (save::parse_json(j, side) != save::JsonError::None) return false;
    side["gameState"]["worldObjects"] = save::Json::object();
    std::string text;
    if (save::encode_json(side, text) != save::JsonError::None) return false;
    host_sd::write_card_file(card_path(slot, "json").c_str(), text);
    std::string c;
    host_sd::read_card_file(card_path(slot, "commit").c_str(), c);
    tdeck::AlphaSaveCommit r;
    std::memcpy(&r, c.data(), sizeof(r));
    r.json = save::save_crc32(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    host_sd::write_card_file(card_path(slot, "commit").c_str(), std::string(reinterpret_cast<char *>(&r), sizeof(r)));
    return true;
}
void cold_inspect(FrontendSaveSlot (&out)[2]) {
    tdeck::AlphaSaveService fresh;
    fresh.inspect(out);
}
bool valid(int slot) {
    FrontendSaveSlot s[2];
    cold_inspect(s);
    return s[slot].present && s[slot].valid;
}
void describe(const char *tag) {
    FrontendSaveSlot s[2];
    cold_inspect(s);
    std::printf("         %-34s slot0 [%s%s seq=%llu] slot1 [%s%s seq=%llu]\n", tag, s[0].present ? "P" : "-",
                s[0].valid ? "V" : "-", (unsigned long long)s[0].sequence, s[1].present ? "P" : "-",
                s[1].valid ? "V" : "-", (unsigned long long)s[1].sequence);
}
// Two saves on a blank card: `older` in slot 1 (sequence 1), `newer` in slot 0
// (sequence 2), as every firmware since Alpha 1 lays them out.
bool two_generations(uint32_t older, uint32_t newer) {
    host_sd::format_card();
    tdeck::AlphaSaveService svc;
    return save_with(svc, older) && save_with(svc, newer) && seq(1) == 1 && seq(0) == 2;
}

// ------------------------------------------------------------------ groups
void test_rotation() {
    std::printf("\n[A] normal two-generation rotation\n");
    host_sd::format_card();
    tdeck::AlphaSaveService svc;
    bool progression = true, other_untouched = true, latest = true;
    for (uint32_t k = 1; k <= 5; ++k) {
        const int written = int(k & 1), other = 1 - written;
        const auto before = take(other);
        const bool ok = save_with(svc, 100 + k);
        progression = progression && ok && seq(written) == k && (k == 1 ? seq(other) == 0 : seq(other) == k - 1);
        other_untouched = other_untouched && take(other) == before;
        latest = latest && boot_load() == int(100 + k);
        std::printf("         save %u: slot0 seq=%llu slot1 seq=%llu, boot load gold=%d\n", k,
                    (unsigned long long)seq(0), (unsigned long long)seq(1), boot_load());
    }
    expect(progression, "A1", "five saves: sequence k lands in slot k&1, the other slot holds k-1");
    expect(other_untouched, "A2", "each save leaves the other generation byte for byte");
    expect(latest, "A3", "after each save a reboot's Continue restores that save");
    FrontendSaveSlot s[2];
    cold_inspect(s);
    expect(s[0].valid && s[1].valid && s[1].sequence == 5 && s[0].sequence == 4, "A4",
           "the card lists two valid generations, sequences 5 (slot 1) and 4 (slot 0)");
}

void test_fallback() {
    std::printf("\n[B] newest generation damaged -> fallback\n");
    expect(two_generations(201, 202), "B0", "two generations: 201 (slot 1, seq 1), 202 (slot 0, seq 2)");
    tear(0);
    describe("newest torn:");
    expect(boot_load() == 201, "B1", "Continue restores the older, valid generation (201)");
    two_generations(201, 202);
    refuse_semantically(0);
    expect(!valid(0) && valid(1) && boot_load() == 201, "B2",
           "a CRC-consistent newest the semantic gate refuses: Continue restores 201 too");
}

// The A4-SAVE1 defect.
void recovery_save(const char *id, void (*damage)(int), const char *what) {
    std::printf("\n[C%s] fallback then Save: %s\n", id, what);
    two_generations(301, 302);
    damage(0);
    const auto good = take(1);
    tdeck::AlphaSaveService svc;   // after a reboot: Continue, then Save
    const bool fell_back = load_with(svc) && g_h->g().gold == 301;
    const bool saved = save_with(svc, 303);
    describe("after the recovery save:");
    std::printf("         slot0 seq=%llu slot1 seq=%llu\n", (unsigned long long)seq(0), (unsigned long long)seq(1));
    const bool kept = take(1) == good;
    char label[16], text[160];
    std::snprintf(label, sizeof(label), "C%s1", id);
    expect(fell_back, label, "Continue fell back to the only valid generation (301, slot 1)");
    std::snprintf(label, sizeof(label), "C%s2", id);
    std::snprintf(text, sizeof(text), "the Save completed and left slot 1 (the only valid generation) byte for byte");
    expect(saved && kept, label, text);
    std::snprintf(label, sizeof(label), "C%s3", id);
    expect(seq(0) == 3 && valid(0), label, "the refused slot 0 was replaced: sequence 3, accepted by the gate");
    std::snprintf(label, sizeof(label), "C%s4", id);
    expect(boot_load() == 303, label, "a reboot's Continue restores the new save (303)");
    FrontendSaveSlot s[2];
    cold_inspect(s);
    std::snprintf(label, sizeof(label), "C%s5", id);
    expect(s[0].valid && s[1].valid && s[0].sequence > s[1].sequence, label,
           "the card again holds two valid generations, the new one newest");
}

void test_after_recovery() {
    std::printf("\n[E] after the recovery save, rotation resumes\n");
    // State from the last C run: slot 0 seq 3 (303), slot 1 seq 1 (301).
    tdeck::AlphaSaveService svc;
    const auto newest = take(0);
    const bool s1 = save_with(svc, 304);
    expect(s1 && seq(1) == 4 && take(0) == newest && boot_load() == 304, "E1",
           "the next save goes to slot 1 (sequence 4) and leaves slot 0 (303) byte for byte");
    const auto newest2 = take(1);
    const bool s2 = save_with(svc, 305);
    expect(s2 && seq(0) == 5 && take(1) == newest2 && boot_load() == 305, "E2",
           "and the one after to slot 0 (sequence 5): alternating, not stuck on one slot");
    tdeck::AlphaSaveService rebooted;
    const bool s3 = save_with(rebooted, 306);
    expect(s3 && seq(1) == 6 && valid(0) && valid(1) && boot_load() == 306, "E3",
           "after a reboot (no save list) the rotation continues: slot 1, sequence 6");
}

struct Fault {
    const char *id, *what;
    void (*arm)();
    bool committed;   // the new generation is complete on the card despite the failure
};
bool g_arm_on_commit = false;
void commit_probe(const char *op, const char *path) {
    // Once a commit record is written, every later read of that slot's
    // sidecar comes back short: the post-write self-check fails after the
    // commit, whichever slot the save chose.
    const char *at = std::strstr(path, "alpha1-g");
    if (g_arm_on_commit && std::strcmp(op, "write") == 0 && at && std::strstr(path, ".commit.tmp")) {
        host_sd::faults().short_read = std::string(at, 9) + ".json";   // "alpha1-gN.json"
        g_arm_on_commit = false;
    }
}
void clear_faults() {
    host_sd::faults() = host_sd::Faults{};
    g_arm_on_commit = false;
    host_sd::set_probe(nullptr);
}

void test_faults() {
    std::printf("\n[D] faults during the recovery save\n");
    static const Fault faults[] = {
        {"D1", "the sidecar temp write fails", [] { host_sd::faults().fail_write = ".json.tmp"; }, false},
        {"D2", "the CRC readback fails (short read of a temp)", [] { host_sd::faults().short_read = ".json.tmp"; }, false},
        {"D3", "the first rename (gam) fails", [] { host_sd::faults().fail_rename = ".gam"; }, false},
        {"D4", "the sidecar rename fails (gam, ool already replaced)", [] { host_sd::faults().fail_rename = ".json"; }, false},
        {"D5", "the commit rename fails (all three files replaced)", [] { host_sd::faults().fail_rename = ".commit"; }, false},
        {"D6", "the post-write self-check fails after the commit",
         [] { g_arm_on_commit = true; host_sd::set_probe(commit_probe); }, true},
    };
    CardState pre;
    two_generations(401, 402);
    tear(0);
    pre.take_all();
    for (const auto &f : faults) {
        pre.put_all();
        tdeck::AlphaSaveService svc;
        const bool fell_back = load_with(svc) && g_h->g().gold == 401;
        f.arm();
        const bool saved = save_with(svc, 403);
        clear_faults();
        const bool kept = take(1) == pre.slot[1];
        const int after = boot_load();
        const bool recovers = f.committed ? (after == 401 || after == 403) : after == 401;
        std::printf("         %s: save=%s, slot 1 kept=%s, boot load gold=%d; %s\n", f.id, saved ? "ok" : "failed",
                    kept ? "yes" : "NO", after, svc.last_failure());
        char label[16], text[200];
        std::snprintf(text, sizeof(text), "%s: the save reports failure, slot 1 (401) is byte for byte, Continue recovers", f.what);
        expect(fell_back && !saved && kept && recovers, f.id, text);
        // The same session retries (the player loads and saves again): it must
        // succeed without touching the generation Continue just restored --
        // slot 1 (401), or slot 0 (403) when the failed save had committed.
        g_h->g().gold = 9999;
        load_with(svc);
        const int keep = g_h->g().gold == 403 ? 0 : 1;
        const auto kept_files = take(keep);
        const bool retried = save_with(svc, 404);
        std::snprintf(label, sizeof(label), "%sr", f.id);
        std::snprintf(text, sizeof(text), "the retry in the same session succeeds, keeps slot %d (the generation it "
                      "loaded) byte for byte, and Continue restores 404", keep);
        expect(retried && take(keep) == kept_files && boot_load() == 404, label, text);
    }
}

void test_both_refused() {
    std::printf("\n[F] both generations refused\n");
    two_generations(501, 502);
    tear(0);
    tear(1);
    tdeck::AlphaSaveService svc;
    g_h->g().gold = 777;
    const bool loaded = load_with(svc);
    expect(!loaded && g_h->g().gold == 777, "F1", "Continue refuses and leaves the live game untouched (no fabricated recovery)");
    const bool saved = save_with(svc, 503);
    expect(saved && boot_load() == 503, "F2", "a save onto such a card succeeds and a reboot's Continue restores it");
    two_generations(501, 502);
    tear(0);
    host_sd::remove_card_file(card_path(1, "commit").c_str());
    tdeck::AlphaSaveService svc2;
    const bool saved2 = save_with(svc2, 504);
    FrontendSaveSlot rows[2];
    cold_inspect(rows);
    const int written = rows[0].sequence == 3 ? 0 : 1;
    expect(saved2 && boot_load() == 504 && rows[written].sequence == 3 && rows[written].valid, "F3",
           "newest refused and no older commit at all: the save succeeds as sequence 3 and Continue restores it");
}

void test_sequences() {
    std::printf("\n[G] sequence and slot identity\n");
    // Sequence parity no longer matches the slot: 7 in slot 0, 4 in slot 1.
    two_generations(601, 602);
    set_seq(0, 7);
    set_seq(1, 4);
    auto newest = take(0);
    tdeck::AlphaSaveService svc;
    const bool s1 = save_with(svc, 603);
    expect(s1 && take(0) == newest && seq(1) == 8 && boot_load() == 603, "G1",
           "seq 7 in slot 0, seq 4 in slot 1: the save writes slot 1 as sequence 8, slot 0 kept (not the parity slot)");
    // Past 32 bits: 0x1'0000'0001 is newer than 2 (a 32-bit compare says 1).
    two_generations(611, 612);
    set_seq(0, 0x100000001ull);
    set_seq(1, 2);
    newest = take(0);
    tdeck::AlphaSaveService svc2;
    const bool s2 = save_with(svc2, 613);
    expect(s2 && take(0) == newest && seq(1) == 0x100000002ull && boot_load() == 613, "G2",
           "64-bit sequences: 0x100000001 is the newest; the save writes slot 1 as 0x100000002");
    // A tie: both 9. Continue takes slot 0 (select_generation: first of equals),
    // so that is the one a save must keep.
    two_generations(621, 622);
    set_seq(0, 9);
    set_seq(1, 9);
    const int tie_load = boot_load();
    newest = take(0);
    tdeck::AlphaSaveService svc3;
    const bool s3 = save_with(svc3, 623);
    expect(tie_load == 622 && s3 && take(0) == newest && seq(1) == 10 && boot_load() == 623, "G3",
           "equal sequences: Continue restores slot 0; the save keeps it and writes slot 1 as sequence 10");
    // The refused newest in slot 1 (parity reversed by an earlier recovery).
    two_generations(631, 632);
    set_seq(1, 5);   // slot 1 (631) now the newest by sequence ...
    tear(1);         // ... and refused
    const auto good = take(0);
    tdeck::AlphaSaveService svc4;
    const bool fell_back = load_with(svc4) && g_h->g().gold == 632;
    const bool s4 = save_with(svc4, 633);
    expect(fell_back && s4 && take(0) == good && seq(1) == 6 && boot_load() == 633, "G4",
           "refused newest in slot 1 (seq 5): the save replaces slot 1 as sequence 6 and keeps slot 0");
}

// Through the runtime's one service: its save list (InspectCache) knows both
// generations as accepted before the newest is damaged behind its back.
void test_runtime(Harness &h) {
    std::printf("\n[R] the runtime's own service, save list warm\n");
    host_sd::format_card();
    h.g().gold = 701;
    const bool a = h.alt_save();
    h.g().gold = 702;
    const bool b = h.alt_save();
    h.open_menu(); h.close_menu();   // the list is known: both generations accepted
    const int newest = seq(0) > seq(1) ? 0 : 1, older = 1 - newest;
    tear(newest);                    // commit untouched: the list's key still matches
    const auto good = take(older);
    h.g().gold = 9999;
    const bool loaded = h.alt_load() && h.g().gold == 701;
    h.g().gold = 703;
    const bool saved = h.alt_save();
    describe("after the runtime's recovery save:");
    expect(a && b && loaded, "R1", "Alt+S twice, the newest torn behind the service, Alt+L restores 701");
    expect(saved && take(older) == good && boot_load() == 703, "R2",
           "Alt+S then keeps the 701 generation byte for byte (a stale save-list entry is not trusted)");
    h.open_menu(); h.close_menu();
    FrontendSaveSlot s[2];
    cold_inspect(s);
    expect(s[0].valid && s[1].valid, "R3", "and the card lists two valid generations again");
    // Again, with the menu opened after the damage: the list still holds the
    // save's own entry for the 703 generation (its commit is unchanged).
    tear(older == 0 ? 1 : 0);   // tear the 703 generation
    const auto good2 = take(older);
    h.open_menu(); h.close_menu();
    h.g().gold = 9999;
    const bool loaded2 = h.alt_load() && h.g().gold == 701;
    h.g().gold = 704;
    const bool saved2 = h.alt_save();
    expect(loaded2 && saved2 && take(older) == good2 && boot_load() == 704, "R4",
           "damage listed by the menu, Alt+L, Alt+S: the 701 generation is kept and 704 loads after a reboot");
}

void test_heap() {
    std::printf("\n[H] heap: the target check adds no second document\n");
    two_generations(801, 802);
    // Warm: the service's own last save is the newest (its list knows it).
    tdeck::AlphaSaveService warm;
    save_with(warm, 803);
    auto measure = [](tdeck::AlphaSaveService &svc, uint32_t gold) {
        const auto base = g_heap;
        g_heap.peak_small = g_heap.live_small;
        const bool ok = save_with(svc, gold);
        struct R { bool ok; int64_t peak, retained; } r{ok, int64_t(g_heap.peak_small) - int64_t(base.live_small),
                                                        int64_t(g_heap.live) - int64_t(base.live)};
        return r;
    };
    const auto w = measure(warm, 804);
    tdeck::AlphaSaveService cold;   // a reboot: the newest is verified before the save
    const auto c = measure(cold, 805);
    std::printf("         warm save: peak small %lld B, retained %lld B; cold save: peak small %lld B, retained %lld B\n",
                (long long)w.peak, (long long)w.retained, (long long)c.peak, (long long)c.retained);
    expect(w.ok && c.ok && c.peak <= w.peak + w.peak / 8, "H1",
           "a cold save's small-heap peak stays within 1/8 of a warm save's (the check runs before the export)");
    expect(c.retained <= w.retained + 4096, "H2", "and it retains nothing a warm save does not");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_save1_recovery_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    const auto dir = std::filesystem::temp_directory_path() / "openu5-a4-save1-card";
    host_sd::set_root(dir.string());
    host_sd::format_card();
    tdeck::AlphaSaveService::reserve_dma_headroom();

    Harness h;
    Owners o;
    g_h = &h;
    g_o = &o;
    test_rotation();
    test_fallback();
    recovery_save("a", tear, "newest torn (CRC)");
    recovery_save("b", [](int s) { refuse_semantically(s); }, "newest refused by the semantic gate");
    test_after_recovery();
    test_faults();
    test_both_refused();
    test_sequences();
    test_runtime(h);
    test_heap();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::printf("\na4_save1_recovery_runtime: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
