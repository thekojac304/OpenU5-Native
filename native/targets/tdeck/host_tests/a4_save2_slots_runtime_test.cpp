// Alpha 4 A4-SAVE2 -- three manual save slots on the A4-SAVE1 recovery model.
//
// The REAL alpha_save.cpp over the fake SD card (host_tests/sd_shims, as
// a4_save1_recovery_runtime): "/sd/..." is a host directory, faults are
// injected by path, and the card's files are read, compared and damaged byte
// for byte. A fresh AlphaSaveService is a reboot (nothing it knew counts).
// Groups O and R go through the real AlphaRuntime: its System Menu, title
// screen, Alt+S and Alt+L, whose one service keeps its save list.
//
// The layout (ALPHA4_UI.md section 4.2), written out here rather than taken
// from alpha_save.cpp, so that a change of layout is a change of this test:
//   Slot 1  alpha1-g0.*     alpha1-g1.*      (the pre-SAVE2 pair, unchanged)
//   Slot 2  alpha1-s2-g0.*  alpha1-s2-g1.*
//   Slot 3  alpha1-s3-g0.*  alpha1-s3-g1.*
// each generation {gam, ool, json, commit}; one sequence across the card.
//
// Groups: E empty card, I slot independence, S Continue, O overwrite through
// the menus, D faults during a slot's save, G generation choice inside a slot,
// M a pre-SAVE2 card (migration), R the runtime's journey slot and New
// Journey, H heap and card I/O.
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
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

// ---------------------------------------------------------------- heap census
// As a3_04g / a4_save1: "small" (<= 4 KiB) allocations are internal RAM first
// on the device.
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

using namespace openu5;
using tdeck::RawInputKind;
namespace fs = std::filesystem;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const std::string &what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what.c_str());
    if (!ok) ++g_failures;
    return ok;
}

const tdeck::AlphaResourceOwners *g_owners = nullptr;
size_t g_dungeon_count = 0;

// ------------------------------------------------------------------ the card
constexpr const char *kSaves = "/sd/ultima5/saves";
std::string gen_path(int slot, int gen, const char *ext) {
    char p[96];
    if (slot == 0) std::snprintf(p, sizeof(p), "%s/alpha1-g%d.%s", kSaves, gen, ext);
    else std::snprintf(p, sizeof(p), "%s/alpha1-s%d-g%d.%s", kSaves, slot + 1, gen, ext);
    return p;
}
const char *slot_prefix(int slot) { return slot == 0 ? "alpha1-g" : slot == 1 ? "alpha1-s2-g" : "alpha1-s3-g"; }

// Every file in the saves directory, by name, with its bytes.
using Card = std::map<std::string, std::string>;
Card take_card() {
    Card c;
    std::error_code ec;
    const auto dir = fs::path(host_sd::root()) / "sd/ultima5/saves";
    if (!fs::exists(dir, ec)) return c;
    for (const auto &e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string name = e.path().filename().string();
        std::string bytes;
        host_sd::read_card_file((std::string(kSaves) + "/" + name).c_str(), bytes);
        c[name] = bytes;
    }
    return c;
}
void put_card(const Card &c) {
    host_sd::format_card();   // a blank card has no saves directory: make it
    std::error_code ec;
    fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/saves", ec);
    for (const auto &[name, bytes] : c) host_sd::write_card_file((std::string(kSaves) + "/" + name).c_str(), bytes);
}
void blank_card() { put_card(Card{}); }
Card slot_of(const Card &c, int slot) {
    Card out;
    for (const auto &[name, bytes] : c)
        if (name.rfind(slot_prefix(slot), 0) == 0) out[name] = bytes;
    return out;
}
// A file's bytes, or "(missing)": a mutant that loses a file fails a check, not the run.
std::string file(const Card &c, const char *name) {
    const auto it = c.find(name);
    return it == c.end() ? std::string("(missing)") : it->second;
}
std::string names(const Card &c) {
    std::string out;
    for (const auto &kv : c) out += (out.empty() ? "" : " ") + kv.first;
    return out;
}
std::string gen_names(int slot, int gen) {
    std::string base = gen_path(slot, gen, "").substr(std::strlen(kSaves) + 1);
    return base + "commit " + base + "gam " + base + "json " + base + "ool";
}

// The commit record (fields, never memcmp: its padding is written uninitialised).
bool commit_of(int slot, int gen, tdeck::AlphaSaveCommit &r) {
    std::string c;
    if (!host_sd::read_card_file(gen_path(slot, gen, "commit").c_str(), c) || c.size() != sizeof(r)) return false;
    std::memcpy(&r, c.data(), sizeof(r));
    return true;
}
uint64_t seq(int slot, int gen) {
    tdeck::AlphaSaveCommit r;
    return commit_of(slot, gen, r) ? r.sequence : 0;
}
void write_commit(int slot, int gen, const tdeck::AlphaSaveCommit &r) {
    host_sd::write_card_file(gen_path(slot, gen, "commit").c_str(), std::string(reinterpret_cast<const char *>(&r), sizeof(r)));
}
void set_seq(int slot, int gen, uint64_t s) {
    tdeck::AlphaSaveCommit r;
    commit_of(slot, gen, r);
    r.sequence = s;
    write_commit(slot, gen, r);
}
int newest_gen(int slot) { return seq(slot, 0) >= seq(slot, 1) ? (seq(slot, 0) ? 0 : (seq(slot, 1) ? 1 : -1)) : 1; }
// Torn: a file cut in half under an intact commit (the CRC gate refuses).
void tear(int slot, int gen, const char *ext = "json") {
    std::string j;
    host_sd::read_card_file(gen_path(slot, gen, ext).c_str(), j);
    host_sd::write_card_file(gen_path(slot, gen, ext).c_str(), j.substr(0, j.size() / 2));
}
// Well-formed and CRC-consistent, but refused by the semantic gate
// (validate_world_objects): the class a CRC cannot see (Batch 28).
bool refuse_semantically(int slot, int gen) {
    std::string j;
    host_sd::read_card_file(gen_path(slot, gen, "json").c_str(), j);
    save::Json side;
    if (save::parse_json(j, side) != save::JsonError::None) return false;
    side["gameState"]["worldObjects"] = save::Json::object();
    std::string text;
    if (save::encode_json(side, text) != save::JsonError::None) return false;
    host_sd::write_card_file(gen_path(slot, gen, "json").c_str(), text);
    tdeck::AlphaSaveCommit r;
    commit_of(slot, gen, r);
    r.json = save::save_crc32(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    write_commit(slot, gen, r);
    return true;
}
bool no_writes(const host_sd::Counters &c) { return c.writes == 0 && c.renames == 0 && c.unlinks == 0 && c.open_writes == 0; }

// ------------------------------------------------------------------ harness
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
    bool key(uint8_t code) { return raw_key(code); }
    bool ball(RawInputKind kind) {
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.timestamp_us = (clock_us += 100000); return rt->handle(raw);
    }
    void down() { ball(RawInputKind::TrackballDown); }
    void up() { ball(RawInputKind::TrackballUp); }
    void set_mark() { mark = rt->ui()->transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }
    FrontendView menu() const { return rt->system_menu_view(); }
    std::string title() const { const auto v = menu(); return v.title ? v.title : ""; }
    std::string footer() const { const auto v = menu(); return v.footer ? v.footer : ""; }
    void close_menu() { if (rt->system_menu_open()) raw_key('m', true); }
    void select(int slot, bool frontend = false) {
        for (int i = 0; i < 3; ++i) {
            const auto v = frontend ? rt->frontend_view() : menu();
            if (v.selected_line == slot) return;
            down();
        }
    }
    // Alt+M, Save Game, the slot (Enter): the menu is then on the question or back at the root.
    void save_page_enter(int slot) { raw_key('m', true); down(); key('\r'); select(slot); key('\r'); }
    // Alt+M, Load Game, the slot, Enter.
    void menu_load(int slot) { raw_key('m', true); down(); down(); key('\r'); select(slot); key('\r'); }
    bool alt_save() { set_mark(); raw_key('s', true); return saw("Save complete"); }
    bool alt_load() { set_mark(); raw_key('l', true); return saw("Load complete"); }
    void return_to_title() { raw_key('m', true); up(); key('\r'); }
};
Harness *g_h = nullptr;

// A distinct, easily told apart game state: gold, karma, position, minute.
struct Mark {
    uint16_t gold;
    uint8_t karma, x, y, minute;
};
void apply(const Mark &m) {
    auto &g = g_h->g();
    g.gold = m.gold; g.karma = m.karma; g.position.xy = {m.x, m.y}; g.time.minute = m.minute;
}
bool is(const Mark &m) {
    const auto &g = g_h->g();
    return g.gold == m.gold && g.karma == m.karma && g.position.xy.x == m.x && g.position.xy.y == m.y &&
           int(g.time.minute) == int(m.minute);
}
void poison() { apply({9999, 1, 3, 3, 59}); }
std::string state() {
    const auto &g = g_h->g();
    char b[96];
    std::snprintf(b, sizeof(b), "gold=%u karma=%u xy=%u,%u min=%d", unsigned(g.gold), unsigned(g.karma),
                  unsigned(g.position.xy.x), unsigned(g.position.xy.y), int(g.time.minute));
    return b;
}

// A service over the runtime's CommandContext and owners of its own.
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
Owners *g_o = nullptr;
bool save_to(tdeck::AlphaSaveService &svc, int slot, const Mark &m) {
    apply(m);
    uint32_t ms = 0;
    return svc.save(g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained,
                    g_owners->initial_gam, g_owners->initial_gam_size, g_owners->initial_ool,
                    g_owners->initial_ool_size, ms, false, slot);
}
bool load_slot_with(tdeck::AlphaSaveService &svc, int slot) {
    uint32_t ms = 0;
    poison();
    return svc.load_slot(slot, g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors,
                         g_o->retained, ms);
}
bool continue_with(tdeck::AlphaSaveService &svc) {
    uint32_t ms = 0;
    poison();
    return svc.load(g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained, ms);
}
// After a reboot.
bool boot_slot_is(int slot, const Mark &m) {
    tdeck::AlphaSaveService fresh;
    return load_slot_with(fresh, slot) && is(m);
}
bool boot_continue_is(const Mark &m, int want_slot) {
    tdeck::AlphaSaveService fresh;
    return continue_with(fresh) && is(m) && fresh.last_slot() == want_slot;
}
FrontendSaveCatalog cold_catalog() {
    tdeck::AlphaSaveService fresh;
    FrontendSaveCatalog c;
    fresh.inspect_catalog(c);
    return c;
}
const char *status_name(SaveSlotStatus s) {
    switch (s) {
    case SaveSlotStatus::Empty: return "empty";
    case SaveSlotStatus::Saved: return "saved";
    case SaveSlotStatus::Recovered: return "recovered";
    case SaveSlotStatus::Damaged: return "damaged";
    }
    return "?";
}
void describe(const char *tag) {
    const auto c = cold_catalog();
    std::printf("         %-30s", tag);
    for (int i = 0; i < 3; ++i)
        std::printf(" [%d %s seq=%llu shows %llu]", i + 1, status_name(c.slots[i].status),
                    (unsigned long long)c.slots[i].sequence, (unsigned long long)c.slots[i].shown.sequence);
    std::printf("\n");
}

const Mark kA{1101, 11, 10, 11, 1}, kB{1202, 22, 20, 21, 2}, kC{1303, 33, 30, 31, 3};
const Mark kA2{2101, 41, 12, 13, 4}, kB2{2202, 42, 22, 23, 5}, kC2{2303, 43, 32, 33, 6};

// ------------------------------------------------------------------ groups
void test_empty() {
    std::printf("\n[E] a card without saves\n");
    host_sd::format_card();   // no saves directory at all
    const auto a = cold_catalog();
    blank_card();             // the directory, empty
    host_sd::reset_counters();
    const auto b = cold_catalog();
    const auto io = host_sd::counters();
    bool all_empty = true;
    for (const auto *c : {&a, &b})
        for (const auto &e : c->slots) all_empty = all_empty && e.status == SaveSlotStatus::Empty && e.sequence == 0;
    expect(all_empty && continue_slot(b) == -1 && first_empty_slot(b) == 0, "E1",
           "no directory, or an empty one: three empty slots; nothing for Continue; the first empty slot is Slot 1");
    expect(io.opens == 0 && io.open_failures == 6 && no_writes(io), "E2",
           "listing it costs six failed commit opens and writes nothing (opens=" + std::to_string(io.opens) +
               " failed=" + std::to_string(io.open_failures) + ")");
    tdeck::AlphaSaveService svc;
    const bool none = !continue_with(svc) && g_h->g().gold == 9999 && !load_slot_with(svc, 0) &&
                      !load_slot_with(svc, 1) && !load_slot_with(svc, 2) && g_h->g().gold == 9999 && svc.last_slot() == -1;
    expect(none, "E3", "Continue and every slot load refuse, and leave the live game untouched");
}

void test_independence() {
    std::printf("\n[I] three independent slots\n");
    blank_card();
    tdeck::AlphaSaveService svc;
    const bool s1 = save_to(svc, 0, kA);
    const Card c1 = take_card();
    expect(s1 && names(c1) == gen_names(0, 1) && seq(0, 1) == 1, "I1",
           "Slot 1 on a blank card: exactly " + gen_names(0, 1) + " (sequence 1), as every older firmware wrote");
    const bool s2 = save_to(svc, 1, kB);
    const Card c2 = take_card();
    expect(s2 && slot_of(c2, 0) == c1 && names(slot_of(c2, 1)) == gen_names(1, 1) && c2.size() == 8 && seq(1, 1) == 2, "I2",
           "Slot 2: only " + gen_names(1, 1) + " (sequence 2) are new; Slot 1 byte for byte");
    const bool s3 = save_to(svc, 2, kC);
    const Card c3 = take_card();
    expect(s3 && slot_of(c3, 0) == slot_of(c2, 0) && slot_of(c3, 1) == slot_of(c2, 1) &&
               names(slot_of(c3, 2)) == gen_names(2, 1) && c3.size() == 12 && seq(2, 1) == 3,
           "I3", "Slot 3: only its own files (sequence 3); Slots 1 and 2 byte for byte");
    expect(boot_slot_is(0, kA) && boot_slot_is(1, kB) && boot_slot_is(2, kC), "I4",
           "after a reboot each slot restores its own game state (gold, karma, position, clock)");
    const auto cat = cold_catalog();
    bool listed = true;
    for (int i = 0; i < 3; ++i)
        listed = listed && cat.slots[i].status == SaveSlotStatus::Saved && cat.slots[i].sequence == uint64_t(i + 1) &&
                 cat.slots[i].shown.sequence == uint64_t(i + 1) && std::strcmp(cat.slots[i].shown.name, "Avatar") == 0;
    expect(listed, "I5", "the list: three saved slots, sequences 1, 2, 3, each showing its own generation");
    // A second save to Slot 2: its pair rotates, nothing else moves.
    const bool s4 = save_to(svc, 1, kB2);
    const Card c4 = take_card();
    expect(s4 && slot_of(c4, 0) == slot_of(c3, 0) && slot_of(c4, 2) == slot_of(c3, 2) && seq(1, 0) == 4 && seq(1, 1) == 2 &&
               file(c4, "alpha1-s2-g1.gam") == file(c3, "alpha1-s2-g1.gam"),
           "I6", "Slot 2 again: its other generation (alpha1-s2-g0, sequence 4 = the card's newest + 1), its older kept, Slots 1 and 3 byte for byte");
    FrontendSaveSlot gens[2];
    tdeck::AlphaSaveService().inspect_generations(1, gens);
    expect(boot_slot_is(1, kB2) && gens[0].valid && gens[1].valid && gens[0].sequence == 4 && gens[1].sequence == 2, "I7",
           "Slot 2 restores the new save; its previous one is still a valid generation beside it");
}

void test_continue() {
    std::printf("\n[S] Continue: the slot saved last\n");
    // From [I]: Slot 1 seq 1 (A), Slot 2 seq 4 (B2) + 2 (B), Slot 3 seq 3 (C).
    expect(boot_continue_is(kB2, 1), "S1", "Continue restores Slot 2 (sequence 4, the card's newest): " + state());
    tdeck::AlphaSaveService svc;
    save_to(svc, 0, kA2);   // seq 5
    expect(boot_continue_is(kA2, 0), "S2", "a save to Slot 1 makes it Continue's: " + state());
    save_to(svc, 2, kC2);   // seq 6
    expect(boot_continue_is(kC2, 2) && boot_slot_is(0, kA2), "S3",
           "then Slot 3 saved last: Continue restores Slot 3, not Slot 1 (which still holds its own save)");
    const auto card = take_card();
    tear(2, newest_gen(2));
    auto cat = cold_catalog();
    expect(boot_continue_is(kC, 2) && cat.slots[2].status == SaveSlotStatus::Recovered && continue_slot(cat) == 2, "S4",
           "Slot 3's latest torn: Continue restores Slot 3's save before it (C, seq 3), not Slot 1's newer A2 (seq 5)");
    tear(2, 1 - newest_gen(2));
    cat = cold_catalog();
    expect(boot_continue_is(kA2, 0) && cat.slots[2].status == SaveSlotStatus::Damaged && continue_slot(cat) == 0, "S5",
           "Slot 3 wholly damaged: Continue restores the next most recent slot (Slot 1, seq 5 over Slot 2's 4)");
    put_card(card);
    set_seq(1, 0, seq(0, newest_gen(0)));   // Slot 2's newest equal to Slot 1's newest
    set_seq(2, newest_gen(2), 1);
    set_seq(2, 1 - newest_gen(2), 1);
    const bool tie = boot_continue_is(kA2, 0) && boot_continue_is(kA2, 0) && continue_slot(cold_catalog()) == 0;
    expect(tie, "S6", "a tie between slots (both sequence 5) goes to the lower slot, on every boot, in the list as in the load");
    put_card(card);
}

void test_overwrite(Harness &h) {
    std::printf("\n[O] overwrite through the System Menu (real shell, real runtime)\n");
    blank_card();
    apply(kA);
    // A4-UI3: a save returns to the game; the transcript names the slot.
    h.set_mark();
    h.save_page_enter(0);
    const bool first = h.saw("Save complete: Slot 1") && !h.rt->system_menu_open();
    h.close_menu();
    apply(kB);
    h.set_mark();
    h.save_page_enter(1);
    const bool second = h.saw("Save complete: Slot 2") && !h.rt->system_menu_open();
    h.close_menu();
    expect(first && second && boot_slot_is(0, kA) && boot_slot_is(1, kB), "O1",
           "an empty slot saves at once, no question (Slot 1: A, Slot 2: B)");
    // Slot 2 is occupied: the question, No selected.
    apply(kC);
    h.save_page_enter(1);
    const bool asked = h.title() == "Overwrite Slot 2?" && h.menu().selected_line == 0;
    const Card before = take_card();
    host_sd::reset_counters();
    h.key('\r');   // No
    const auto io_no = host_sd::counters();
    const bool kept = take_card() == before && no_writes(io_no) && h.footer() == "Slot 2 kept" && h.title() == "Save Game";
    expect(asked && kept, "O2", "\"Overwrite Slot 2?\" starts on No; Enter there writes nothing: every byte of every slot as it was");
    h.key('\r');   // the question again
    host_sd::reset_counters();
    h.key('\b');   // Back
    const auto io_back = host_sd::counters();
    const bool back = take_card() == before && no_writes(io_back) && h.title() == "Save Game";
    h.key('\r');
    h.close_menu();   // Alt+M with the question up (the close persists settings.json, outside the saves)
    expect(back && take_card() == before && boot_slot_is(1, kB), "O3",
           "Back from the question writes nothing, and closing the menu on it leaves every save byte for byte; Slot 2 still restores B");
    apply(kC);
    h.save_page_enter(1);
    h.down();      // Yes
    h.set_mark();
    h.key('\r');
    const bool yes = h.saw("Save complete: Slot 2") && !h.rt->system_menu_open();
    const Card after = take_card();
    FrontendSaveSlot gens[2];
    tdeck::AlphaSaveService().inspect_generations(1, gens);
    h.close_menu();
    expect(yes && boot_slot_is(1, kC) && slot_of(after, 0) == slot_of(before, 0) && gens[0].valid && gens[1].valid, "O4",
           "Yes: Slot 2 restores the new save (C); Slot 1 byte for byte; Slot 2 keeps two valid generations");
    tear(1, newest_gen(1));
    expect(boot_slot_is(1, kB), "O5", "the replaced save (B) is Slot 2's fallback: with C torn, Slot 2 restores B");
}

struct Fault {
    const char *id, *what;
    void (*arm)();
    bool committed;   // the new generation is complete on the card despite the failure
};
bool g_arm_on_commit = false;
void commit_probe(const char *op, const char *path) {
    // Once Slot 2's new commit record is written, every later read of that
    // generation's sidecar comes back short: the post-write self-check fails
    // after the commit.
    const char *at = std::strstr(path, "alpha1-s2-g");
    if (g_arm_on_commit && std::strcmp(op, "write") == 0 && at && std::strstr(path, ".commit.tmp")) {
        host_sd::faults().short_read = std::string(at, 12) + ".json";   // "alpha1-s2-gN.json"
        g_arm_on_commit = false;
    }
}
void clear_faults() {
    host_sd::faults() = host_sd::Faults{};
    g_arm_on_commit = false;
    host_sd::set_probe(nullptr);
}

void test_faults() {
    std::printf("\n[D] faults during a save to Slot 2 (Slots 1 and 3 hold journeys of their own)\n");
    static const Fault faults[] = {
        {"D1", "the sidecar temp write fails", [] { host_sd::faults().fail_write = "alpha1-s2-g0.json.tmp"; }, false},
        {"D2", "the CRC readback fails (short read of a temp)", [] { host_sd::faults().short_read = "alpha1-s2-g0.json.tmp"; }, false},
        {"D3", "the first rename (gam) fails", [] { host_sd::faults().fail_rename = "alpha1-s2-g0.gam"; }, false},
        {"D4", "the sidecar rename fails (gam, ool already replaced)", [] { host_sd::faults().fail_rename = "alpha1-s2-g0.json"; }, false},
        {"D5", "the commit rename fails (all three files replaced)", [] { host_sd::faults().fail_rename = "alpha1-s2-g0.commit"; }, false},
        {"D6", "the post-write self-check fails after the commit",
         [] { g_arm_on_commit = true; host_sd::set_probe(commit_probe); }, true},
    };
    // Slot 1: A (seq 1). Slot 2: B (seq 2, g1) and B2 (seq 4, g0). Slot 3: C (seq 3).
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA); save_to(svc, 1, kB); save_to(svc, 2, kC); save_to(svc, 1, kB2);
    }
    // The newest (B2) torn, as in A4-SAVE1's defect: the save must replace it
    // (alpha1-s2-g0) and keep B -- the generation Slot 2 restores.
    tear(1, 0);
    const Card pre = take_card();
    for (const auto &f : faults) {
        put_card(pre);
        tdeck::AlphaSaveService svc;
        const bool fell_back = load_slot_with(svc, 1) && is(kB);
        f.arm();
        const bool saved = save_to(svc, 1, kC2);
        clear_faults();
        const Card now = take_card();
        const bool isolated = slot_of(now, 0) == slot_of(pre, 0) && slot_of(now, 2) == slot_of(pre, 2);
        const bool kept = file(now, "alpha1-s2-g1.gam") == file(pre, "alpha1-s2-g1.gam") &&
                          file(now, "alpha1-s2-g1.json") == file(pre, "alpha1-s2-g1.json") &&
                          file(now, "alpha1-s2-g1.commit") == file(pre, "alpha1-s2-g1.commit");
        tdeck::AlphaSaveService boot;
        const bool loaded = load_slot_with(boot, 1);
        const bool recovers = loaded && (is(kB) || (f.committed && is(kC2)));
        const auto cat = cold_catalog();
        // The list shows B (sequence 2): Recovered while the replaced
        // generation keeps its old commit, Saved once the failed commit rename
        // has taken that commit away (D5: move_temp unlinks before renaming).
        const bool not_selected = f.committed || (cat.slots[1].shown.sequence == 2 && slot_loadable(cat, 1));
        const bool others = boot_slot_is(0, kA) && boot_slot_is(2, kC);
        std::printf("         %s: save=%s, slot 2 now %s (shows seq %llu), %s\n", f.id, saved ? "ok" : "failed",
                    status_name(cat.slots[1].status), (unsigned long long)cat.slots[1].shown.sequence, svc.last_failure());
        expect(fell_back && !saved && isolated && kept && recovers && not_selected && others, f.id,
               std::string(f.what) + ": the save fails; Slot 2's previous save (B) is byte for byte and restores; "
               "the bad candidate is not listed; Slots 1 and 3 are byte for byte and restore their own");
        const bool retried = save_to(svc, 1, kB2);
        const Card after = take_card();
        char label[8];
        std::snprintf(label, sizeof(label), "%sr", f.id);
        expect(retried && boot_slot_is(1, kB2) && slot_of(after, 0) == slot_of(pre, 0) && slot_of(after, 2) == slot_of(pre, 2),
               label, "the retry in the same session succeeds, and Slots 1 and 3 are still untouched");
    }
    // The first save into an EMPTY slot fails: nothing listed there, nothing else moved.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA); save_to(svc, 1, kB);
    }
    const Card two = take_card();
    tdeck::AlphaSaveService svc;
    host_sd::faults().fail_rename = "alpha1-s3-g1.json";
    const bool saved = save_to(svc, 2, kC);
    clear_faults();
    const Card now = take_card();
    const auto cat = cold_catalog();
    expect(!saved && cat.slots[2].status == SaveSlotStatus::Empty && slot_of(now, 0) == slot_of(two, 0) &&
               slot_of(now, 1) == slot_of(two, 1) && boot_continue_is(kB, 1),
           "D7", "a failed first save into empty Slot 3 (no commit written) leaves it listed empty; Continue still restores Slot 2");
}

void test_generations() {
    std::printf("\n[G] which generation a slot restores\n");
    // Slot 1: A. Slot 2: B (seq 2, g1), B2 (seq 3, g0).
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA); save_to(svc, 1, kB); save_to(svc, 1, kB2);
    }
    const Card base = take_card();
    auto slot2 = [&](const char *id, const std::string &what, void (*damage)(), const Mark *want, SaveSlotStatus listed,
                     uint64_t shows) {
        put_card(base);
        damage();
        const auto cat = cold_catalog();
        tdeck::AlphaSaveService svc;
        const bool loaded = load_slot_with(svc, 1);
        const bool ok = want ? loaded && is(*want) : !loaded && g_h->g().gold == 9999;
        expect(ok && cat.slots[1].status == listed && (listed == SaveSlotStatus::Damaged || cat.slots[1].shown.sequence == shows) &&
                   boot_slot_is(0, kA),
               id, what + " (listed " + status_name(cat.slots[1].status) + "; Slot 1 unaffected)");
    };
    slot2("G1", "both valid: the newer (B2)", [] {}, &kB2, SaveSlotStatus::Saved, 3);
    slot2("G2", "newer torn (CRC): the older (B)", [] { tear(1, 0); }, &kB, SaveSlotStatus::Recovered, 2);
    slot2("G3", "older torn, newer valid: the newer; the older never shown", [] { tear(1, 1); }, &kB2, SaveSlotStatus::Saved, 3);
    slot2("G4", "newer GAM truncated: the older", [] { tear(1, 0, "gam"); }, &kB, SaveSlotStatus::Recovered, 2);
    slot2("G5", "newer commit missing: the older, listed as the slot's save", [] { host_sd::remove_card_file(gen_path(1, 0, "commit").c_str()); },
          &kB, SaveSlotStatus::Saved, 2);
    slot2("G6", "newer commit with a bad magic (not a commit): the older", [] {
        tdeck::AlphaSaveCommit r; commit_of(1, 0, r); r.magic ^= 1u; write_commit(1, 0, r); }, &kB, SaveSlotStatus::Saved, 2);
    slot2("G7", "newer commit of an unknown version: the older", [] {
        tdeck::AlphaSaveCommit r; commit_of(1, 0, r); r.version = 2; write_commit(1, 0, r); }, &kB, SaveSlotStatus::Saved, 2);
    slot2("G8", "newer commit record truncated: the older", [] {
        std::string c; host_sd::read_card_file(gen_path(1, 0, "commit").c_str(), c);
        host_sd::write_card_file(gen_path(1, 0, "commit").c_str(), c.substr(0, 20)); }, &kB, SaveSlotStatus::Saved, 2);
    slot2("G9", "newer refused by the semantic gate (CRC-consistent): the older", [] { refuse_semantically(1, 0); }, &kB,
          SaveSlotStatus::Recovered, 2);
    slot2("G10", "the older's sequence raised above the newer's: the order follows sequence, not the file", [] { set_seq(1, 1, 9); },
          &kB, SaveSlotStatus::Saved, 9);
    slot2("G11", "equal sequences: generation 0 (select_generation's first of equals)", [] { set_seq(1, 1, 3); }, &kB2,
          SaveSlotStatus::Saved, 3);
    slot2("G12", "both refused: nothing restores, the live game is untouched", [] { tear(1, 0); refuse_semantically(1, 1); },
          nullptr, SaveSlotStatus::Damaged, 0);
    // Saving into the wholly damaged slot works and touches nothing else.
    put_card(base);
    tear(1, 0);
    refuse_semantically(1, 1);
    tdeck::AlphaSaveService svc;
    const Card before = take_card();
    const bool saved = save_to(svc, 1, kC);
    expect(saved && boot_slot_is(1, kC) && slot_of(take_card(), 0) == slot_of(before, 0), "G13",
           "a save into a wholly damaged slot succeeds and restores; Slot 1 byte for byte");
}

// ---------------------------------------------------------- a pre-SAVE2 card
// Written the way Alpha 3 / A4-UI1 / A4-UI2 / A4-SAVE1 wrote it, independently
// of AlphaSaveService: alpha1-g<gen>.{gam,ool,json} and a v1 commit record.
bool legacy_write(int gen, uint64_t sequence, const Mark &m) {
    apply(m);
    auto &c = g_h->rt->command_context_for_test();
    tdeck::capture_save_document(c, g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained);
    save::Gam gam{};
    save::Json side;
    if (save::export_native_state(c.game, c.turn, g_o->retained, g_owners->initial_gam, g_owners->initial_gam_size, gam,
                                  side, true) != save::Error::None)
        return false;
    const auto ool = save::build_ool(g_o->retained, g_owners->initial_ool, g_owners->initial_ool_size);
    std::string json;
    if (save::encode_json(side, json) != save::JsonError::None) return false;
    char base[96];
    std::snprintf(base, sizeof(base), "%s/alpha1-g%d.", kSaves, gen);
    host_sd::write_card_file((std::string(base) + "gam").c_str(), std::string(gam.begin(), gam.end()));
    host_sd::write_card_file((std::string(base) + "ool").c_str(), std::string(ool.begin(), ool.end()));
    host_sd::write_card_file((std::string(base) + "json").c_str(), json);
    tdeck::AlphaSaveCommit r{};
    r.sequence = sequence;
    r.gam = save::save_crc32(gam.data(), gam.size());
    r.ool = save::save_crc32(ool.data(), ool.size());
    r.json = save::save_crc32(reinterpret_cast<const uint8_t *>(json.data()), json.size());
    host_sd::write_card_file((std::string(base) + "commit").c_str(), std::string(reinterpret_cast<const char *>(&r), sizeof(r)));
    return true;
}
// What an older firmware sees: the newest alpha1-g<gen> whose v1 commit's CRCs
// match its files (-1: none).
int legacy_reader_newest(uint64_t &sequence) {
    int best = -1;
    for (int g = 0; g < 2; ++g) {
        tdeck::AlphaSaveCommit r;
        if (!commit_of(0, g, r) || r.magic != 0x31533555 || r.version != 1) continue;
        std::string gam, ool, json;
        host_sd::read_card_file(gen_path(0, g, "gam").c_str(), gam);
        host_sd::read_card_file(gen_path(0, g, "ool").c_str(), ool);
        host_sd::read_card_file(gen_path(0, g, "json").c_str(), json);
        auto crc = [](const std::string &s) { return save::save_crc32(reinterpret_cast<const uint8_t *>(s.data()), s.size()); };
        if (crc(gam) != r.gam || crc(ool) != r.ool || crc(json) != r.json) continue;
        if (best < 0 || r.sequence > sequence) { best = g; sequence = r.sequence; }
    }
    return best;
}

void test_legacy() {
    std::printf("\n[M] a card written before A4-SAVE2 (migration)\n");
    const Mark l1{3101, 51, 40, 41, 7}, l2{3202, 52, 42, 43, 8}, l3{3303, 53, 44, 45, 9};
    blank_card();
    const bool wrote = legacy_write(1, 1, l1) && legacy_write(0, 2, l2);
    const Card legacy = take_card();
    host_sd::reset_counters();
    const auto cat = cold_catalog();
    const bool cont = boot_continue_is(l2, 0);
    const bool slot = boot_slot_is(0, l2);
    const auto io = host_sd::counters();
    expect(wrote && names(legacy) == gen_names(0, 0) + " " + gen_names(0, 1) && cat.slots[0].status == SaveSlotStatus::Saved &&
               cat.slots[0].sequence == 2 && cat.slots[1].status == SaveSlotStatus::Empty &&
               cat.slots[2].status == SaveSlotStatus::Empty,
           "M1", "the pre-SAVE2 pair is listed as Slot 1 (its newest, sequence 2); Slots 2 and 3 empty");
    expect(cont && slot && no_writes(io) && take_card() == legacy, "M2",
           "Continue and Slot 1 restore its newest journey; listing and loading write nothing (writes=" +
               std::to_string(io.writes) + " renames=" + std::to_string(io.renames) + "): the card is byte for byte");
    bool idempotent = true;
    for (int boot = 0; boot < 3; ++boot) {
        host_sd::reset_counters();
        const auto again = cold_catalog();
        const bool loads = boot_continue_is(l2, 0);
        idempotent = idempotent && loads && no_writes(host_sd::counters()) && take_card() == legacy &&
                     again.slots[0].sequence == 2 && again.slots[1].status == SaveSlotStatus::Empty;
    }
    expect(idempotent, "M3", "three more boots: the same list and the same Continue, and still not one byte written");
    tear(0, 0);
    expect(boot_continue_is(l1, 0) && cold_catalog().slots[0].status == SaveSlotStatus::Recovered, "M4",
           "its own fallback survives: the newest torn, Continue restores the older generation (l1)");
    put_card(legacy);
    // A save to Slot 2 leaves the old files alone.
    tdeck::AlphaSaveService svc;
    const bool s2 = save_to(svc, 1, l3);
    const Card c2 = take_card();
    uint64_t legacy_seq = 0;
    const int legacy_pick = legacy_reader_newest(legacy_seq);
    expect(s2 && slot_of(c2, 0) == legacy && names(slot_of(c2, 1)) == gen_names(1, 1) && seq(1, 1) == 3 &&
               boot_continue_is(l3, 1) && boot_slot_is(0, l2) && legacy_pick == 0 && legacy_seq == 2,
           "M5", "a save to Slot 2 (sequence 3) adds only its own files: the old pair is byte for byte, still what "
                 "an older firmware reads (alpha1-g0, seq 2), and Continue now restores Slot 2");
    // Alt+S's default (no journey slot yet) on an old card: Slot 1, the old names.
    put_card(legacy);
    tdeck::AlphaSaveService svc2;
    const bool quick = save_to(svc2, -1, l3);
    const Card c3 = take_card();
    legacy_seq = 0;
    const int pick = legacy_reader_newest(legacy_seq);
    expect(quick && names(c3) == names(legacy) && file(c3, "alpha1-g0.gam") == file(legacy, "alpha1-g0.gam") && seq(0, 1) == 3 &&
               pick == 1 && legacy_seq == 3 && boot_slot_is(0, l3),
           "M6", "a save with no slot named (Alt+S before any load) on an old card rotates the old pair (alpha1-g1, seq 3), "
                 "which an older firmware reads as its newest; no new files");
    // An older firmware then saves Slot 1 while Slot 2 exists: the same
    // sequence on both slots. The choice is fixed: the lower slot.
    put_card(legacy);
    tdeck::AlphaSaveService svc3;
    save_to(svc3, 1, l3);         // Slot 2, sequence 3
    legacy_write(1, 3, l1);       // the older firmware's next save: alpha1-g1, 2 + 1
    expect(boot_continue_is(l1, 0) && boot_continue_is(l1, 0) && boot_slot_is(1, l3), "M7",
           "after a downgrade and upgrade, equal sequences (3 and 3): Continue restores Slot 1 on every boot; Slot 2 intact");
}

void test_runtime(Harness &h) {
    std::printf("\n[R] the runtime: the journey's slot, Alt+S / Alt+L, New Journey\n");
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA); save_to(svc, 1, kB); save_to(svc, 2, kC);
    }
    // Load Slot 2 (not the newest) through the menu, then Alt+S.
    h.set_mark();
    h.menu_load(1);
    const bool loaded = h.saw("Load complete") && is(kB) && h.rt->save_service_for_test().last_slot() == 1;
    const Card before = take_card();
    apply(kB2);
    const bool saved = h.alt_save();
    const Card after = take_card();
    expect(loaded && saved && slot_of(after, 0) == slot_of(before, 0) && slot_of(after, 2) == slot_of(before, 2) &&
               boot_slot_is(1, kB2) && boot_continue_is(kB2, 1),
           "R1", "Slot 2 loaded from the menu, then Alt+S: it saves Slot 2 (not Slot 3, the newest); Slots 1 and 3 byte for byte");
    {
        tdeck::AlphaSaveService other;   // Slot 3 saved last: Continue would now restore Slot 3
        save_to(other, 2, kC2);
    }
    apply(kA2);
    const bool reloaded = h.alt_load() && is(kB2) && boot_continue_is(kC2, 2);
    expect(reloaded, "R2", "Alt+L reloads the journey's slot (Slot 2, B2), not Continue's (Slot 3, saved last)");
    // A save from the menu to another slot makes that the journey's slot.
    apply(kA2);
    h.save_page_enter(0);
    h.down();
    h.key('\r');   // Yes
    h.close_menu();
    apply(kC2);
    const bool quick = h.alt_save() && boot_slot_is(0, kC2) && boot_slot_is(1, kB2);
    expect(quick, "R3", "after Save Game to Slot 1, Alt+S follows the journey to Slot 1");

    // New Journey with Slot 3 empty.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA); save_to(svc, 1, kB);
    }
    const Card two = take_card();
    auto create = [&](const char *name) {
        for (const char *p = name; *p; ++p) h.key(uint8_t(*p));
        h.key('\r');
        h.key('f');
        for (int i = 0; i < 7; ++i) h.key(uint8_t((i & 1) ? 'b' : 'a'));
    };
    h.return_to_title();
    const bool at_title = h.rt->frontend_open();
    h.key(' ');
    h.key('c');
    const bool creating = h.rt->frontend_state() == FrontendState::CharacterCreation;
    create("Nova");
    const Card three = take_card();
    const auto cat = cold_catalog();
    expect(at_title && creating && !h.rt->frontend_open() && slot_of(three, 0) == slot_of(two, 0) &&
               slot_of(three, 1) == slot_of(two, 1) && names(slot_of(three, 2)) == gen_names(2, 1) &&
               cat.slots[2].status == SaveSlotStatus::Saved && std::strcmp(cat.slots[2].shown.name, "Nova") == 0 &&
               h.rt->save_service_for_test().last_slot() == 2,
           "R4", "Create New Character with Slot 3 empty: the new journey is saved in Slot 3 alone; Slots 1 and 2 byte for byte");
    {
        tdeck::AlphaSaveService fresh;
        poison();
        const bool cont = continue_with(fresh) && fresh.last_slot() == 2 &&
                          std::strcmp(g_h->g().party.characters[0].name, "Nova") == 0;
        expect(cont, "R5", "after a reboot Continue restores the new journey (Slot 3, leader Nova)");
    }
    // Every slot in use: the choice, No first.
    h.return_to_title();
    h.key(' ');
    h.key('c');
    const bool picking = h.rt->frontend_state() == FrontendState::NewJourneySlot;
    h.select(0, true);
    h.key('\r');
    const auto q = h.rt->frontend_view();
    const bool question = std::string(q.title) == "Replace Slot 1?" && q.selected_line == 0;
    host_sd::reset_counters();
    h.key('\r');   // No
    const bool list_again = h.rt->frontend_state() == FrontendState::NewJourneySlot;
    const auto io = host_sd::counters();
    expect(picking && question && list_again && no_writes(io) && take_card() == three, "R6",
           "all slots in use: Create New Character asks which to replace; \"Replace Slot 1?\" starts on No, and No writes nothing");
    h.key('\r');
    h.down();      // Yes
    h.key('\r');
    const bool creating2 = h.rt->frontend_state() == FrontendState::CharacterCreation;
    create("Rune");
    const Card four = take_card();
    const auto cat2 = cold_catalog();
    FrontendSaveSlot gens[2];
    tdeck::AlphaSaveService().inspect_generations(0, gens);
    expect(creating2 && slot_of(four, 1) == slot_of(three, 1) && slot_of(four, 2) == slot_of(three, 2) &&
               std::strcmp(cat2.slots[0].shown.name, "Rune") == 0 && gens[0].valid && gens[1].valid,
           "R7", "Yes: the new journey replaces Slot 1 (its latest, leader Rune); Slots 2 and 3 byte for byte");
}

void test_heap_io() {
    std::printf("\n[H] the list's card I/O and heap\n");
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        for (int k = 0; k < 3; ++k) { save_to(svc, k, kA); save_to(svc, k, kB); }
    }
    const size_t gen_bytes = [] {
        std::string a, b, c;
        host_sd::read_card_file(gen_path(0, 0, "gam").c_str(), a);
        host_sd::read_card_file(gen_path(0, 0, "ool").c_str(), b);
        host_sd::read_card_file(gen_path(0, 0, "json").c_str(), c);
        return a.size() + b.size() + c.size();
    }();
    tdeck::AlphaSaveService svc;
    FrontendSaveCatalog cat;
    host_sd::reset_counters();
    const auto base = g_heap;
    g_heap.peak_small = g_heap.live_small;
    svc.inspect_catalog(cat);
    const int64_t cold_peak = int64_t(g_heap.peak_small) - int64_t(base.live_small);
    const int64_t cold_kept = int64_t(g_heap.live) - int64_t(base.live);
    const auto cold = host_sd::counters();
    std::printf("         cold list, 3 slots x 2 generations: opens=%u failed=%u bytes_read=%llu peak_small=%lld retained=%lld\n",
                cold.opens, cold.open_failures, (unsigned long long)cold.bytes_read, (long long)cold_peak, (long long)cold_kept);
    expect(cold.opens == 6 + 3 * 3 && cold.bytes_read <= 6 * 32 + 3 * gen_bytes + 3 * 64 && no_writes(cold), "H1",
           "a cold list reads six commits and ONE generation per slot (the newest), never the backups: opens=" +
               std::to_string(cold.opens));
    host_sd::reset_counters();
    svc.inspect_catalog(cat);
    const auto warm = host_sd::counters();
    expect(warm.opens == 6 && warm.bytes_read == 6 * sizeof(tdeck::AlphaSaveCommit) && no_writes(warm), "H2",
           "a warm list of an unchanged card is the six 32-byte commit reads alone");
    // One document at a time: the cold list's peak is one verify's.
    FrontendSaveSlot one[2];
    tdeck::AlphaSaveService single;
    const auto base2 = g_heap;
    g_heap.peak_small = g_heap.live_small;
    single.inspect_generations(0, one);
    const int64_t pair_peak = int64_t(g_heap.peak_small) - int64_t(base2.live_small);
    expect(cold_peak <= pair_peak + pair_peak / 8 && cold_kept <= 1024, "H3",
           "the cold list's small-heap peak (" + std::to_string(cold_peak) + " B) is one staged document's (" +
               std::to_string(pair_peak) + " B for a pair); it keeps nothing (" + std::to_string(cold_kept) + " B)");
    // A pre-SAVE2-shaped card: two generations in Slot 1 only.
    blank_card();
    {
        tdeck::AlphaSaveService s;
        save_to(s, 0, kA); save_to(s, 0, kB);
    }
    host_sd::reset_counters();
    FrontendSaveCatalog legacy;
    tdeck::AlphaSaveService().inspect_catalog(legacy);
    const auto now = host_sd::counters();
    host_sd::reset_counters();
    tdeck::AlphaSaveService().inspect_generations(0, one);
    const auto before = host_sd::counters();
    std::printf("         a one-journey card, cold: the A4-SAVE2 list opens=%u failed=%u bytes=%llu; the pre-SAVE2 list "
                "(both generations) opens=%u bytes=%llu\n",
                now.opens, now.open_failures, (unsigned long long)now.bytes_read, before.opens,
                (unsigned long long)before.bytes_read);
    expect(now.opens == 2 + 3 && now.open_failures == 4 && before.opens == 2 + 6 && now.bytes_read < before.bytes_read, "H4",
           "on a one-journey card a cold list reads one generation (the pre-SAVE2 list read both), plus four failed "
           "commit opens for the empty slots");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_save2_slots_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    const auto dir = fs::temp_directory_path() / "openu5-a4-save2-card";
    host_sd::set_root(dir.string());
    host_sd::format_card();
    tdeck::AlphaSaveService::reserve_dma_headroom();

    Harness h;
    Owners o;
    g_h = &h;
    g_o = &o;
    test_empty();
    test_independence();
    test_continue();
    test_overwrite(h);
    test_faults();
    test_generations();
    test_legacy();
    test_runtime(h);
    test_heap_io();

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("\na4_save2_slots_runtime: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
