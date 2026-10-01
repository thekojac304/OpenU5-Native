// Alpha 4 A4-SAVE3 -- original PC/DOS save import and export
// (targets/tdeck/ALPHA4_UI.md section 5).
//
// The REAL alpha_save.cpp over the fake SD card (host_tests/sd_shims, as
// a4_save2_slots_runtime), the real AlphaRuntime and its title screen, and a
// genuine DOS save as the fixture: original/u5/ultima5/SAVED.GAM + SAVED.OOL
// (Kojac, Shamino and Iolo in Lord British's Castle; never written by Native).
// Variants of it (outdoors, vehicles, search bits, a dungeon, damage) are made
// here by patching documented offsets, never by the Native writer.
//
// The byte offsets this test reads are written out below from the format
// document (docs/formats/tlk-npc-dataovl-gam.md section 4) and the RE notes,
// not taken from pc_save.cpp or persistence.cpp: a verifier that shared the
// writer's tables would agree with any mistake in them.
//
// Groups: P the bridge's checks and codec, J import through the title, F
// failures during import, X export, T the round trip, R recovery afterwards,
// H card I/O and heap. `--emit <dir>` also writes the files check-pc-save.ts
// reads with the TypeScript reference (the independent reader).
#include "../main/alpha_runtime.h"
#include "../main/alpha_save.h"
#include "../main/alpha_save_generation.h"
#include "esp_heap_caps.h"
#include "sd_shims/host_sd_card.h"

#include "openu5/pc_save.h"
#include "openu5/quest_world.h"
#include "openu5/save_json.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------- heap census
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
namespace pc = openu5::save::pc;
using Bytes = std::vector<uint8_t>;

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
Bytes g_gam, g_ool;   // the DOS fixture

// ----------------------------------------------------- the original layout
// SAVED.GAM (docs/formats/tlk-npc-dataovl-gam.md section 4 and the cited notes).
namespace gam {
constexpr size_t kSize = 4192, kOolSize = 512;
constexpr size_t kRoster = 0x02, kRecord = 32;
constexpr size_t kFood = 0x202, kGold = 0x204, kKeys = 0x206, kGems = 0x207, kTorches = 0x208;
constexpr size_t kSkullTree = 0x20C;              // [0x57b2]            (state.ts)
constexpr size_t kEquipment = 0x21A, kSpells = 0x24A, kScrolls = 0x27A, kPotions = 0x282, kReagents = 0x2AA;
constexpr size_t kMoonX = 0x28A, kMoonY = 0x292, kMoonBuried = 0x29A, kMoonZ = 0x2A2;
constexpr size_t kReagentDays = 0x2B2;            // [0x5858..5a]        (state.ts)
constexpr size_t kPartySize = 0x2B5;
constexpr size_t kSearch = 0x2B6;                 // [0x585c] found-once (re/notes/sjog.md)
constexpr size_t kYear = 0x2CE, kTimeSpell = 0x2D4, kActive = 0x2D5, kTransport = 0x2D6, kMonth = 0x2D7, kDay = 0x2D8,
                 kHour = 0x2D9, kMinute = 0x2DB, kDrift = 0x2DD, kKarma = 0x2E2, kTurns = 0x2E5, kSpellTurns = 0x2E8,
                 kWind = 0x2EC, kLocation = 0x2ED, kFloor = 0x2EF, kX = 0x2F0, kY = 0x2F1, kLight = 0x300,
                 kTorchMins = 0x301, kSail = 0x3AF;
constexpr size_t kNpcDead = 0x5B4, kNpcMet = 0x634;
constexpr size_t kTable = 0x6B4;                  // DS:0x5C5A, 32 x 8 B
constexpr size_t kTypes = 0xFF8;                  // DS:0x659E (re/notes/npc.md 0.4)
int u16(const Bytes &b, size_t at) { return b[at] | b[at + 1] << 8; }
} // namespace gam

// The bytes Native never writes (section 5.5 L1/L5), and the ones the bridge
// adds (B1-B8). Anything else that differs between an original save and its
// Native export is a codec loss.
bool unbridged(size_t at) {
    if (at >= 0x02 && at < 0x02 + 16 * 32 && (at - 0x02) % 32 == 0x18) return true; // each record's +0x18
    const size_t singles[] = {0x2D0, 0x2D1, 0x2D2, 0x2D3, 0x2DC, 0x2E1, 0x2E6, 0x2E9, 0x2EA, 0x2FC, 0x2FD, 0x2FE, 0x2FF,
                              0x3A9, 0x3AA, 0x3AB, 0x3AC, 0x3B0 /* DS:0x5956, unnamed in re/ledger */, 0x3B1, 0x3B2, 0x105D};
    for (auto s : singles)
        if (at == s) return true;
    return (at >= 0x3B4 && at < 0x5B4)   // g_dng_map
           || (at >= 0x6BC && at < 0x105C); // the town's object slots and NPC tables (L1)
}
bool bridged(size_t at) {
    return (at >= gam::kSearch && at < gam::kSearch + 15) || at == gam::kWind || at == gam::kDrift || at == gam::kLight ||
           at == gam::kSail || at == gam::kSkullTree || (at >= gam::kReagentDays && at < gam::kReagentDays + 3) ||
           at == gam::kTimeSpell || at == gam::kSpellTurns;
}

Bytes read_host(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// ------------------------------------------------------------------ the card
constexpr const char *kSaves = "/sd/ultima5/saves";
constexpr const char *kImport = "/sd/ultima5/import";
std::string gen_path(int slot, int gen, const char *ext) {
    char p[96];
    if (slot == 0) std::snprintf(p, sizeof(p), "%s/alpha1-g%d.%s", kSaves, gen, ext);
    else std::snprintf(p, sizeof(p), "%s/alpha1-s%d-g%d.%s", kSaves, slot + 1, gen, ext);
    return p;
}
const char *slot_prefix(int slot) { return slot == 0 ? "alpha1-g" : slot == 1 ? "alpha1-s2-g" : "alpha1-s3-g"; }
std::string export_path(int slot, const char *name) { return std::string("/sd/ultima5/export/slot") + char('1' + slot) + "/" + name; }

using Card = std::map<std::string, std::string>;
// Every file under /sd/ultima5/<sub>, by relative name.
Card take_dir(const char *sub) {
    Card c;
    std::error_code ec;
    const auto dir = fs::path(host_sd::root()) / "sd/ultima5" / sub;
    if (!fs::exists(dir, ec)) return c;
    for (const auto &e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string rel = fs::relative(e.path(), dir, ec).generic_string();
        std::string bytes;
        host_sd::read_card_file((std::string("/sd/ultima5/") + sub + "/" + rel).c_str(), bytes);
        c[rel] = bytes;
    }
    return c;
}
Card take_card() { return take_dir("saves"); }
// A file's bytes, or "(missing)": a mutant that loses a file fails a check, not the run.
std::string file(const Card &c, const std::string &name) {
    const auto it = c.find(name);
    return it == c.end() ? std::string("(missing)") : it->second;
}
Card slot_of(const Card &c, int slot) {
    Card out;
    for (const auto &[name, bytes] : c)
        if (name.rfind(slot_prefix(slot), 0) == 0) out[name] = bytes;
    return out;
}
std::string names(const Card &c) {
    std::string out;
    for (const auto &kv : c) out += (out.empty() ? "" : " ") + kv.first;
    return out;
}
// The saves directory becomes `c`; the import folder is kept as it is. The
// import marker and any export go (format_card wipes the card).
void put_card(const Card &c) {
    const Card import = take_dir("import");
    host_sd::format_card();
    std::error_code ec;
    fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/saves", ec);
    for (const auto &[name, bytes] : c) host_sd::write_card_file((std::string(kSaves) + "/" + name).c_str(), bytes);
    if (!import.empty()) fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/import", ec);
    for (const auto &[name, bytes] : import) host_sd::write_card_file((std::string(kImport) + "/" + name).c_str(), bytes);
}
void blank_card() { put_card(Card{}); }
std::string str(const Bytes &b) { return std::string(b.begin(), b.end()); }
Bytes bytes_of(const std::string &s) { return Bytes(s.begin(), s.end()); }
void put_import(const Bytes *g, const Bytes *o) {
    std::error_code ec;
    fs::remove_all(fs::path(host_sd::root()) / "sd/ultima5/import", ec);
    fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/import", ec);
    if (g) host_sd::write_card_file((std::string(kImport) + "/SAVED.GAM").c_str(), str(*g));
    if (o) host_sd::write_card_file((std::string(kImport) + "/SAVED.OOL").c_str(), str(*o));
}
bool no_writes(const host_sd::Counters &c) { return c.writes == 0 && c.renames == 0 && c.unlinks == 0 && c.open_writes == 0; }
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
int newest_gen(int slot) { return seq(slot, 0) || seq(slot, 1) ? (seq(slot, 0) >= seq(slot, 1) ? 0 : 1) : -1; }
void tear(int slot, int gen, const char *ext = "json") {
    std::string j;
    host_sd::read_card_file(gen_path(slot, gen, ext).c_str(), j);
    host_sd::write_card_file(gen_path(slot, gen, ext).c_str(), j.substr(0, j.size() / 2));
}

// Writes to the import folder seen by the fake card (the probe sees reads and
// writes; renames and unlinks show in the folder's listing and bytes).
int g_import_writes = 0, g_import_reads = 0;
void probe(const char *op, const char *path) {
    if (!path || !std::strstr(path, "/import/")) return;
    if (!std::strcmp(op, "write")) ++g_import_writes;
    else ++g_import_reads;
}

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
        reset_game();
    }
    // A small outdoor journey, as a4_save2's harness keeps.
    void reset_game() {
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::memset(ch.name, 0, sizeof(ch.name));
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.gender = 0x0b; ch.dexterity = 30; ch.strength = 30; ch.level = 1;
        g.party.active_character = 255;
        g.time.year = 139; g.time.month = 4; g.time.day = 2; g.time.hour = 12; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.karma = 50; g.gold = 321;
        g.position.map = {0, 0};
        g.position.xy = {0x50, 0x60};
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
    FrontendView view() const { return rt->frontend_view(); }
    std::string title() const { const auto v = view(); return v.title ? v.title : ""; }
    std::string subtitle() const { const auto v = view(); return v.subtitle ? v.subtitle : ""; }
    std::string footer() const { const auto v = view(); return v.footer ? v.footer : ""; }
    FrontendState state() const { return rt->frontend_state(); }
    void select(int line) {
        for (int i = 0; i < 4 && view().selected_line != line; ++i) down();
    }
    // Wherever the runtime is, the title's main menu.
    void main_menu() {
        if (!rt->frontend_open()) { raw_key('m', true); up(); key('\r'); }
        for (int i = 0; i < 6 && state() != FrontendState::MainMenu; ++i) {
            if (state() == FrontendState::Title || state() == FrontendState::AttractDemo || state() == FrontendState::Error ||
                state() == FrontendState::Credits) key(' ');
            else key('\b');   // Back, outside name entry (ui_input_adapter.cpp)
        }
    }
    // Main menu -> PC Save Transfer (the runtime reads the import folder).
    void pc_page() { main_menu(); key('p'); }
    // PC Save Transfer -> Import -> slot `slot` -> Enter.
    void import_enter(int slot) { pc_page(); select(0); key('\r'); select(slot); key('\r'); }
    // PC Save Transfer -> Export -> slot `slot` -> Enter.
    void export_enter(int slot) { pc_page(); select(1); key('\r'); select(slot); key('\r'); }
    bool alt_save() { set_mark(); raw_key('s', true); return saw("Save complete"); }
};
Harness *g_h = nullptr;

// ------------------------------------------------------ Native saves to use
struct Owners {
    OutdoorServices outdoor{};
    WorldTerrain terrain{};
    NpcActors actors{};
    save::Json retained{};
    Owners() {
        GameState g{};
        TurnState t{};
        save::SidecarSource source;
        save::load_native_state(g_owners->initial_gam, g_owners->initial_gam_size, nullptr, g, t, retained, source, true);
    }
};
Owners *g_o = nullptr;
struct Mark {
    uint16_t gold;
    uint8_t karma, x, y, minute;
};
constexpr Mark kA{1111, 11, 0x51, 0x61, 11}, kB{2222, 22, 0x52, 0x62, 22}, kC{3333, 33, 0x53, 0x63, 33};
void apply(const Mark &m) {
    auto &g = g_h->g();
    g.gold = m.gold; g.karma = m.karma; g.position.map = {0, 0}; g.position.xy = {m.x, m.y}; g.time.minute = m.minute;
}
bool is(const Mark &m) {
    const auto &g = g_h->g();
    return g.gold == m.gold && g.karma == m.karma && g.position.xy.x == m.x && g.position.xy.y == m.y && int(g.time.minute) == int(m.minute);
}
bool save_to(tdeck::AlphaSaveService &svc, int slot, const Mark &m) {
    g_h->reset_game();
    apply(m);
    uint32_t ms = 0;
    return svc.save(g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained,
                    g_owners->initial_gam, g_owners->initial_gam_size, g_owners->initial_ool, g_owners->initial_ool_size, ms,
                    false, slot);
}
void poison() { g_h->g().gold = 9999; g_h->g().karma = 1; std::memset(g_h->g().party.characters[0].name, 'X', 8); }
bool load_slot_with(tdeck::AlphaSaveService &svc, int slot) {
    uint32_t ms = 0;
    poison();
    return svc.load_slot(slot, g_h->rt->command_context_for_test(), g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained, ms);
}
bool boot_slot_is(int slot, const Mark &m) {
    tdeck::AlphaSaveService fresh;
    return load_slot_with(fresh, slot) && is(m);
}
bool is_kojac() { return std::strcmp(g_h->g().party.characters[0].name, "Kojac") == 0 && g_h->g().gold == 9200; }
FrontendSaveCatalog cold_catalog() {
    tdeck::AlphaSaveService fresh;
    FrontendSaveCatalog c;
    fresh.inspect_catalog(c);
    return c;
}

// ---------------------------------------------------------- fixture variants
Bytes variant(void (*edit)(Bytes &)) {
    Bytes b = g_gam;
    edit(b);
    return b;
}
// Outdoors on the surface at (0x50,0x60), on foot; the live table holds a
// frigate (slot 30, hull 77, 1 skiff), a horse (slot 25), a skiff (slot 26)
// and a monster (slot 5: byte 0x44 = tile 324, the reference's definition 1).
void outdoors(Bytes &b) {
    b[gam::kLocation] = 0; b[gam::kFloor] = 0; b[gam::kX] = 0x50; b[gam::kY] = 0x60; b[gam::kTransport] = 0x1c;
    std::fill(b.begin() + gam::kTable, b.begin() + gam::kTable + 256, 0);
    auto rec = [&](int slot, int t, int x, int y, int hull, int skiffs) {
        uint8_t *o = b.data() + gam::kTable + slot * 8;
        o[0] = o[1] = uint8_t(t); o[2] = uint8_t(x); o[3] = uint8_t(y); o[4] = 0; o[5] = uint8_t(hull); o[6] = 0; o[7] = uint8_t(skiffs);
    };
    rec(0, 0x1c, 0x50, 0x60, 0, 0);
    rec(30, 0x24, 0x51, 0x60, 77, 1);
    rec(25, 0x10, 0x4f, 0x60, 0, 0);
    rec(26, 0x28, 0x52, 0x61, 0, 0);
    rec(5, 0x44, 0x48, 0x5a, 0, 0);
}
// A frigate parked in SAVED.OOL: `block` 0 = BRIT (surface), 1 = UNDER.
Bytes ool_with_frigate(int block, int x, int y, int hull, int skiffs) {
    Bytes o = g_ool;
    uint8_t *r = o.data() + block * 256 + 29 * 8;
    r[0] = r[1] = 0x25; r[2] = uint8_t(x); r[3] = uint8_t(y); r[4] = block ? 0xff : 0; r[5] = uint8_t(hull); r[6] = 0; r[7] = uint8_t(skiffs);
    return o;
}

std::string json_text(const save::Json &j) {
    std::string t;
    save::encode_json(j, t);
    return t;
}
save::Json slot_json(int slot) {
    save::Json j;
    std::string t;
    const int g = newest_gen(slot);
    if (g >= 0) host_sd::read_card_file(gen_path(slot, g, "json").c_str(), t);
    save::parse_json(t, j);
    return j;
}

// ======================================================================= P
void test_bridge() {
    std::printf("\n[P] the bridge: checks and codec (pure)\n");
    const auto ok = pc::check_original(g_gam.data(), g_gam.size(), g_ool.data(), g_ool.size());
    expect(ok == pc::Check::Ok, "P1", "the genuine DOS SAVED.GAM/SAVED.OOL pass the original-format check");
    const Bytes init(g_owners->initial_gam, g_owners->initial_gam + g_owners->initial_gam_size);
    expect(pc::check_original(init.data(), init.size(), g_ool.data(), g_ool.size()) == pc::Check::Roster, "P2",
           "INIT.GAM is not a save (its Avatar record has no name): Roster");
    struct Case { const char *what; pc::Check want; void (*edit)(Bytes &); };
    const Case cases[] = {
        {"party size 0", pc::Check::Party, [](Bytes &b) { b[gam::kPartySize] = 0; }},
        {"party size 7", pc::Check::Party, [](Bytes &b) { b[gam::kPartySize] = 7; }},
        {"member 2's class 'Z'", pc::Check::Roster, [](Bytes &b) { b[gam::kRoster + gam::kRecord + 10] = 'Z'; }},
        {"a control byte in the Avatar's name", pc::Check::Roster, [](Bytes &b) { b[gam::kRoster + 1] = 0x01; }},
        {"month 14", pc::Check::Clock, [](Bytes &b) { b[gam::kMonth] = 14; }},
        {"minute 60", pc::Check::Clock, [](Bytes &b) { b[gam::kMinute] = 60; }},
        {"location 41", pc::Check::Location, [](Bytes &b) { b[gam::kLocation] = 41; }},
        {"inside Deceit (33)", pc::Check::Dungeon, [](Bytes &b) { b[gam::kLocation] = 33; b[gam::kX] = 1; b[gam::kY] = 1; }},
        {"inside Doom (40)", pc::Check::Dungeon, [](Bytes &b) { b[gam::kLocation] = 40; }},
        {"town x = 32", pc::Check::Location, [](Bytes &b) { b[gam::kX] = 32; }},
        {"outdoors on floor 5", pc::Check::Location, [](Bytes &b) { b[gam::kLocation] = 0; b[gam::kFloor] = 5; }},
        {"wind 5", pc::Check::Weather, [](Bytes &b) { b[gam::kWind] = 5; }},
        {"sail direction 9", pc::Check::Weather, [](Bytes &b) { b[gam::kSail] = 9; }},
    };
    bool all = true;
    std::string bad;
    for (const auto &c : cases) {
        const Bytes b = variant(c.edit);
        if (pc::check_original(b.data(), b.size(), g_ool.data(), g_ool.size()) != c.want) { all = false; bad += std::string(" ") + c.what; }
    }
    const Bytes shortg(g_gam.begin(), g_gam.end() - 1), longg = [] { Bytes b = g_gam; b.push_back(0); return b; }();
    const Bytes shorto(g_ool.begin(), g_ool.end() - 1);
    all = all && pc::check_original(shortg.data(), shortg.size(), g_ool.data(), g_ool.size()) == pc::Check::GamSize &&
          pc::check_original(longg.data(), longg.size(), g_ool.data(), g_ool.size()) == pc::Check::GamSize &&
          pc::check_original(g_gam.data(), g_gam.size(), shorto.data(), shorto.size()) == pc::Check::OolSize &&
          pc::check_original(g_gam.data(), g_gam.size(), nullptr, 0) == pc::Check::OolSize;
    expect(all, "P3", "each invalid, incomplete or dungeon variant is refused with its own reason (sizes, party, roster, clock, position, dungeon, weather)" + bad);

    // The fixture through the bridge: every modelled field equals the file's bytes.
    GameState g{};
    TurnState t{};
    save::Json doc;
    pc::ImportReport rep;
    const auto e = pc::import_original(g_gam.data(), g_ool.data(), g, t, doc, &rep);
    bool fields = e == save::Error::None && g.party.party_size == g_gam[gam::kPartySize] && g.party.character_count == 16;
    for (int m = 0; m < 16 && fields; ++m) {
        const uint8_t *r = g_gam.data() + gam::kRoster + m * gam::kRecord;
        const auto &c = g.party.characters[m];
        fields = std::strncmp(c.name, reinterpret_cast<const char *>(r), 9) == 0 && c.gender == r[9] &&
                 uint8_t(c.character_class) == r[10] && uint8_t(c.status) == r[11] && c.strength == r[12] &&
                 c.dexterity == r[13] && c.intelligence == r[14] && c.current_mp == r[15] && c.current_hp == (r[16] | r[17] << 8) &&
                 c.max_hp == (r[18] | r[19] << 8) && c.exp == (r[20] | r[21] << 8) && c.level == r[22] && c.helmet == r[25] &&
                 c.armor == r[26] && c.weapon == r[27] && c.shield == r[28] && c.ring == r[29] && c.amulet == r[30] &&
                 c.party_status == r[31];
    }
    fields = fields && g.food == gam::u16(g_gam, gam::kFood) && g.gold == gam::u16(g_gam, gam::kGold) && g.keys == g_gam[gam::kKeys] &&
             g.gems == g_gam[gam::kGems] && g.torches == g_gam[gam::kTorches] && g.karma == g_gam[gam::kKarma] &&
             g.turns_since_start == g_gam[gam::kTurns] && g.torch_turns == g_gam[gam::kTorchMins] &&
             g.time.year == gam::u16(g_gam, gam::kYear) && g.time.month == g_gam[gam::kMonth] && g.time.day == g_gam[gam::kDay] &&
             g.time.hour == g_gam[gam::kHour] && g.time.minute == g_gam[gam::kMinute] &&
             g.position.map.location == g_gam[gam::kLocation] && g.position.map.floor == g_gam[gam::kFloor] &&
             g.position.xy.x == g_gam[gam::kX] && g.position.xy.y == g_gam[gam::kY];
    for (int i = 0; i < 48 && fields; ++i) fields = g.equipment_quantities[i] == g_gam[gam::kEquipment + i] && g.spell_quantities[i] == g_gam[gam::kSpells + i];
    for (int i = 0; i < 8 && fields; ++i)
        fields = g.scroll_quantities[i] == g_gam[gam::kScrolls + i] && g.potion_quantities[i] == g_gam[gam::kPotions + i] &&
                 g.reagent_quantities[i] == g_gam[gam::kReagents + i];
    for (int i = 0; i < 32 && fields; ++i)
        for (int k = 0; k < 32 && fields; ++k) {
            const size_t bit = size_t(i) * 32 + size_t(k);
            fields = ((g.npc_dead[i] >> k) & 1u) == ((g_gam[gam::kNpcDead + bit / 8] >> (7 - bit % 8)) & 1u) &&
                     ((g.npc_met[i] >> k) & 1u) == ((g_gam[gam::kNpcMet + bit / 8] >> (7 - bit % 8)) & 1u);
        }
    const auto &moons = doc["moonstones"];
    for (size_t i = 0; i < 8 && fields; ++i)
        fields = moons.at(i)["x"].integer() == g_gam[gam::kMoonX + i] && moons.at(i)["y"].integer() == g_gam[gam::kMoonY + i] &&
                 moons.at(i)["buried"].truth() == (g_gam[gam::kMoonBuried + i] != 0xff) && moons.at(i)["z"].integer() == g_gam[gam::kMoonZ + i];
    expect(fields, "P4", "the DOS fixture imported: all 16 roster records, supplies, clock, position, the 5 inventories, 8 moonstones and both NPC bitmaps equal the file's own bytes");
    expect(e == save::Error::None && t.wind == 4 && doc["wind"].integer() == 4 && doc["transport"].string == save::Json("foot").string &&
               !doc.has("timeSpell") && doc["worldObjects"].kind == save::Json::Array && doc["worldObjects"].values.empty() &&
               !doc.has("dungeon") && save::validate_world_objects(doc) == save::Error::None,
           "P5", "the sidecar-less import is completed: wind West from 0x2EC (B2), transport foot from the tile (B9), no time spell, an empty pool (no vehicles parked), no dungeon session");

    // B1-B9 on a variant carrying every bridged byte.
    const Bytes rich = variant([](Bytes &b) {
        for (int i : {0, 7, 12, 13, 14, 15, 16, 112}) b[gam::kSearch + i / 8] = uint8_t(b[gam::kSearch + i / 8] | (1u << (i % 8)));
        b[gam::kWind] = 2; b[gam::kDrift] = 3; b[gam::kLight] = 45; b[gam::kSail] = 1; b[gam::kSkullTree] = 9;
        b[gam::kReagentDays] = 4; b[gam::kReagentDays + 1] = 5; b[gam::kReagentDays + 2] = 6;
        b[gam::kTimeSpell] = 'P'; b[gam::kSpellTurns] = 20;
    });
    GameState g2{};
    TurnState t2{};
    save::Json d2;
    pc::import_original(rich.data(), g_ool.data(), g2, t2, d2, &rep);
    std::string found;
    for (int i = 0; i < 113; ++i) {
        char k[16];
        std::snprintf(k, sizeof(k), "search:%d", i);
        if (d2["questFlags"][k].truth()) found += " " + std::to_string(i);
    }
    const bool searches = found == " 0 7 12 16 112" && g2.quest.search_found[0] == 0x81 && (g2.quest.search_found[1] & 0x10) && (g2.quest.search_found[2] & 1);
    expect(searches, "P6", "B1: the found-once bitmap 0x2B6 becomes search:N (0, 7, 12, 16, 112); N = 13/14/15 stay with their own gates (found:" + found + ")");
    expect(t2.wind == 2 && t2.wind_drift_counter == 3 && t2.light_spell_minutes == 45 && t2.sail_dir == 1 && t2.skull_tree_day == 9 &&
               t2.reagent_days[0] == 4 && t2.reagent_days[1] == 5 && t2.reagent_days[2] == 6 && t2.time_spell == 'P' && t2.spell_turns == 20,
           "P7", "B2-B8: wind, drift, light minutes, sail, skull-tree day, the 3 reagent days and the time spell reach the live turn state");
    const Bytes ship_bytes = variant([](Bytes &b) { outdoors(b); b[gam::kTransport] = 0x24; });
    GameState g3{};
    TurnState t3{};
    save::Json d3;
    pc::import_original(ship_bytes.data(), g_ool.data(), g3, t3, d3, nullptr);
    expect(g3.transport == TransportMode::Ship && t3.transport_tile == 0x24, "P8",
           "B9: a party aboard a frigate (tile 0x24) imports as TransportMode::Ship, not the empty sidecar's foot");

    // B10: the codec alone refuses vehicle records; the bridge converts them.
    const Bytes out = variant(outdoors);
    const Bytes under = ool_with_frigate(1, 0x20, 0x21, 55, 2);
    {
        GameState cg{};
        TurnState ct{};
        save::Json cd;
        save::SidecarSource src;
        save::load_native_state(out.data(), out.size(), nullptr, cg, ct, cd, src, true);
        expect(save::validate_world_objects(cd) == save::Error::NativeDomain, "P9",
               "baseline (section 5.4): the codec alone turns a DOS vehicle into a reference entry the gate refuses (NativeDomain)");
    }
    GameState g4{};
    TurnState t4{};
    save::Json d4;
    pc::ImportReport r4;
    pc::import_original(out.data(), under.data(), g4, t4, d4, &r4);
    const auto &objs = d4["worldObjects"].values;
    auto frigate_at = [&](int floor, int x, int y, int hull, int skiffs) {
        for (const auto &o : objs)
            if (o["ship"].truth() && o["location"].integer() == 0 && o["floor"].integer() == floor && o["x"].integer() == x &&
                o["y"].integer() == y && o["hull"].integer() == hull && o["skiffs"].integer() == skiffs) return true;
        return false;
    };
    // The new-game UNDER.OOL (the fixture's UNDER block) parks a skiff at
    // (14,242): 0x29 0x29 0x0e 0xf2 0xff in its first record.
    expect(r4.frigates == 2 && r4.horses == 1 && r4.skiffs == 2 && objs.size() == 2 && frigate_at(0, 0x51, 0x60, 77, 1) &&
               frigate_at(255, 0x20, 0x21, 55, 2) && d4["mapOverrides"]["0:0:79:96"].integer() == 0x110 &&
               d4["mapOverrides"]["0:0:82:97"].integer() == 0x128 && d4["mapOverrides"]["0:255:14:242"].integer() == 0x129 &&
               d4["mapOverrides"].keys.size() == 3 && save::validate_world_objects(d4) == save::Error::None &&
               d4["overworldEnemies"].values.size() == 1 && d4["overworldEnemies"].at(0)["defIndex"].integer() == 1 &&
               d4["overworldEnemies"].at(0)["x"].integer() == 0x48,
           "P10", "B10: the live table's frigate (hull 77, 1 skiff) and UNDER.OOL's parked one (hull 55) become pool ships; the horse, the skiff and the new-game underworld skiff at (14,242) become the terrain cells Native drops; the gate accepts it (report " +
               std::to_string(r4.frigates) + "/" + std::to_string(r4.horses) + "/" + std::to_string(r4.skiffs) + ", pool " + std::to_string(objs.size()) +
               ", cells " + json_text(d4["mapOverrides"]) + ")");
    const Bytes town_parked = ool_with_frigate(0, 0x44, 0x45, 66, 0);
    GameState g5{};
    TurnState t5{};
    save::Json d5;
    pc::import_original(g_gam.data(), town_parked.data(), g5, t5, d5, nullptr);
    bool town_ship = false;
    for (const auto &o : d5["worldObjects"].values) town_ship |= o["ship"].truth() && o["floor"].integer() == 0 && o["x"].integer() == 0x44 && o["hull"].integer() == 66;
    expect(town_ship && d5["worldObjects"].values.size() == 1, "P11", "B10: saved in a town, the frigate parked in SAVED.OOL's BRIT block comes back as a surface pool ship");

    // The codec is exact for what it models; only the documented bytes differ.
    save::Gam self{}, native{};
    save::Json side;
    save::export_native_state(g, t, doc, g_gam.data(), g_gam.size(), self, side, true);
    size_t self_diff = 0;
    for (size_t i = 0; i < gam::kSize; ++i) self_diff += self[i] != g_gam[i];
    save::export_native_state(g, t, doc, init.data(), init.size(), native, side, true);
    save::Ool ool{};
    std::copy(g_owners->initial_ool, g_owners->initial_ool + gam::kOolSize, ool.begin());
    pc::complete_export(doc, native, ool, nullptr);
    size_t codec_loss = 0, allowed = 0;
    std::string where;
    for (size_t i = 0; i < gam::kSize; ++i) {
        if (native[i] == g_gam[i]) continue;
        if (unbridged(i)) { ++allowed; continue; }
        ++codec_loss;
        char w[16];
        std::snprintf(w, sizeof(w), " 0x%03zx", i);
        where += w;
    }
    expect(self_diff == 0, "P12", "the codec is exact: the fixture re-exported over itself differs in 0 of 4192 bytes");
    expect(codec_loss == 0 && allowed > 0 && native[gam::kWind] == 4, "P13",
           "Native's own .gam (INIT template) + the bridge equals the DOS file except in the documented unbridged bytes (" +
               std::to_string(allowed) + " of them: town tables, combat scratch, light radius, door tracker ...); wind restored" + where);

    // Export: B1-B8 written, vehicles placed, Native-only counted.
    save::Json ex = doc;
    ex["questFlags"]["search:20"] = save::Json(true);
    ex["questFlags"]["search:13"] = save::Json(true);
    ex["wind"] = save::Json(3); ex["sailDir"] = save::Json(2); ex["lightSpellMins"] = save::Json(30); ex["windDriftCtr"] = save::Json(1);
    ex["skullTreeFoundDay"] = save::Json(7);
    ex["reagentPatchFoundDay"] = save::Json::array();
    for (int v : {1, 2, 3}) ex["reagentPatchFoundDay"].values.emplace_back(v);
    save::Json spell("");
    spell.string += u'Q';
    ex["timeSpell"] = spell; ex["timeSpellTurns"] = save::Json(9);
    save::Gam eg = native;
    save::Ool eo = ool;
    pc::ExportReport er;
    pc::complete_export(ex, eg, eo, &er);
    expect((eg[gam::kSearch + 2] & 0x10) && !(eg[gam::kSearch + 1] & 0x20) && eg[gam::kWind] == 3 && eg[gam::kSail] == 2 &&
               eg[gam::kLight] == 30 && eg[gam::kDrift] == 1 && eg[gam::kSkullTree] == 7 && eg[gam::kReagentDays] == 1 &&
               eg[gam::kReagentDays + 2] == 3 && eg[gam::kTimeSpell] == 'Q' && eg[gam::kSpellTurns] == 9 && er.search_found == 1 &&
               er.town_npcs_left_out,
           "P14", "export B1-B8: search:20 sets its bit (13 does not: gated), wind/sail/light/drift/tree/reagents/time spell written; a town save is reported as leaving its NPC tables out (L1)");
    // Vehicles: a town save's surface frigate and horse go to BRIT.OOL; an underworld frigate to UNDER.OOL.
    save::Json v = doc;
    QuestObject f;
    f.location = 0; f.floor = 0; f.x = 0x40; f.y = 0x41; f.tile = 0x124; f.hull = 88; f.skiffs = 2; f.ship = true;
    QuestObject u = f;
    u.floor = 255; u.x = 0x10; u.tile = 0x126;
    QuestObject loot;
    loot.location = 17; loot.loot = true;
    struct Pool { std::vector<QuestObject> v; } pool{{f, u, loot}};
    QuestWorldServices q{};
    q.context = &pool;
    q.count = [](void *p) { return static_cast<Pool *>(p)->v.size(); };
    q.read = [](void *p, size_t i) { return static_cast<Pool *>(p)->v[i]; };
    save::capture_world_objects(q, v);
    v["mapOverrides"] = save::Json::object();
    v["mapOverrides"]["0:0:66:67"] = save::Json(0x111);
    v["mapOverrides"]["0:0:70:70"] = save::Json(0x115);   // a carpet: L4
    v["mapOverrides"]["17:0:3:3"] = save::Json(0x110);    // not location 0: not a vehicle
    save::Gam vg = native;
    save::Ool vo = ool;
    pc::ExportReport vr;
    pc::complete_export(v, vg, vo, &vr);
    auto find = [](const uint8_t *table, int t, int x, int y) {
        for (int i = 1; i < 32; ++i)
            if (table[i * 8] == t && table[i * 8 + 1] == t && table[i * 8 + 2] == x && table[i * 8 + 3] == y) return i;
        return -1;
    };
    const int fs1 = find(vo.data(), 0x24, 0x40, 0x41), hs = find(vo.data(), 0x11, 66, 67), us = find(vo.data() + 256, 0x26, 0x10, 0x41);
    expect(fs1 > 0 && hs > 0 && us > 0 && vo[fs1 * 8 + 5] == 88 && vo[fs1 * 8 + 7] == 2 && vo[256 + us * 8 + 4] == 0xff &&
               vr.frigates == 2 && vr.horses == 1 && vr.carpets == 1 && vr.loot == 1 && vr.unplaced == 0 &&
               std::equal(vg.begin() + gam::kTable, vg.begin() + gam::kTable + 256, native.begin() + gam::kTable),
           "P15", "export B10 (in a town): the surface frigate (hull 88, 2 skiffs) and horse are written to SAVED.OOL's BRIT block, the underworld frigate to UNDER; the town's live table is untouched; a carpet and a loot pile are counted, not written");
    // Outdoors: the live table, and a full table reports what did not fit.
    save::Json w = v;
    w["position"]["location"] = save::Json(0); w["position"]["floor"] = save::Json(0);
    save::Gam wg = native;
    save::Ool wo = ool;
    for (int i = 1; i < 32; ++i) wg[gam::kTable + i * 8] = i == 31 ? 0 : 0x80;   // one free record
    pc::ExportReport wr;
    pc::complete_export(w, wg, wo, &wr);
    expect(wg[gam::kTable + 31 * 8] == 0x24 && wr.unplaced == 1 && find(wo.data() + 256, 0x26, 0x10, 0x41) > 0, "P16",
           "export B10 outdoors: vehicles of the party's world go into the live table (.gam +0x6B4); with one free record the second is reported unplaced, never written over another");
    save::Json dn = doc;
    dn["position"]["location"] = save::Json(35);
    save::Json ds = doc;
    ds["dungeon"] = save::Json::object();
    expect(pc::exportable(doc) == pc::Check::Ok && pc::exportable(dn) == pc::Check::Dungeon && pc::exportable(ds) == pc::Check::Dungeon,
           "P17", "a dungeon document (location 33-40 or a session) is not exportable (L2)");
}

// ======================================================================= J
std::string import_dir_state() {
    const auto c = take_dir("import");
    return names(c) + "|" + (c.count("SAVED.GAM") ? c.at("SAVED.GAM") : "") + "|" + (c.count("SAVED.OOL") ? c.at("SAVED.OOL") : "");
}
void test_import(Harness &h) {
    std::printf("\n[J] import through the title's PC Save Transfer page\n");
    blank_card();
    put_import(nullptr, nullptr);
    host_sd::reset_counters();
    h.pc_page();
    const bool page = h.state() == FrontendState::PcTransfer && h.title() == "PC Save Transfer";
    const auto missing = h.subtitle();
    h.select(0);
    h.key('\r');
    const auto io = host_sd::counters();
    expect(page && missing == "No PC save in /ultima5/import" && h.state() == FrontendState::PcTransfer &&
               h.footer() == "No PC save in /ultima5/import" && no_writes(io) && take_card().empty(),
           "J1", "no import files: the page says so, Import only repeats it, nothing is written (subtitle \"" + missing + "\")");

    put_import(&g_gam, nullptr);
    host_sd::reset_counters();
    h.pc_page();
    const auto incomplete = h.subtitle();
    h.select(0);
    h.key('\r');
    tdeck::AlphaSaveService::PcSource src;
    const auto read = tdeck::AlphaSaveService().read_pc_import(src);
    expect(incomplete == "PC save incomplete: SAVED.OOL is missing" && h.state() == FrontendState::PcTransfer &&
               read == tdeck::AlphaSaveService::PcRead::NoOol && no_writes(host_sd::counters()) && take_card().empty(),
           "J2", "an incomplete set (SAVED.GAM alone) is refused before any slot is offered; no card writes");

    const Bytes big(4000, 0x41);
    put_import(&big, &g_ool);
    h.pc_page();
    const auto size = h.subtitle();
    tdeck::AlphaSaveService::PcSource src2;
    tdeck::AlphaSaveService().read_pc_import(src2);
    expect(size == "SAVED.GAM is not 4192 bytes" && src2.gam.empty() && src2.gam_size == 4000 && take_card().empty(), "J3",
           "a SAVED.GAM of the wrong size is reported and never read into memory");
    const Bytes corrupt = variant([](Bytes &b) { b[gam::kPartySize] = 0; });
    put_import(&corrupt, &g_ool);
    h.pc_page();
    const auto bad = h.subtitle();
    const Bytes dungeon = variant([](Bytes &b) { b[gam::kLocation] = 35; });
    put_import(&dungeon, &g_ool);
    h.pc_page();
    const auto dng = h.subtitle();
    h.select(0);
    h.key('\r');
    expect(bad == "SAVED.GAM: party size is not 1-6" && dng == "Dungeon saves cannot be transferred" &&
               h.footer() == dng && take_card().empty(),
           "J4", "a corrupt save and a save inside a dungeon are refused with their reasons; nothing is written");

    // The genuine save into an empty card.
    put_import(&g_gam, &g_ool);
    const auto source = import_dir_state();
    g_import_writes = 0;
    h.pc_page();
    const auto ready = h.subtitle();
    h.select(0);
    h.key('\r');
    const bool list = h.state() == FrontendState::PcImportSlot && h.view().selected_line == 0 && h.title() == "Import PC Save";
    h.key('\r');
    const Card card = take_card();
    const auto notice = h.footer();
    expect(ready == "PC save: Kojac, Lord British's Castle" && list && h.state() == FrontendState::PcTransfer &&
               notice == "Imported into Slot 1. The PC files are kept" && names(card) == "alpha1-g1.commit alpha1-g1.gam alpha1-g1.json alpha1-g1.ool",
           "J5", "the genuine DOS save: listed by name and place, Import -> Slot 1 (empty) -> one ordinary generation, alpha1-g1.* (\"" + notice + "\")");
    expect(import_dir_state() == source && g_import_writes == 0, "J6", "the PC files are untouched: same names, same bytes, no write to the import folder");

    // The sidecar it wrote.
    const auto j = slot_json(0);
    const auto &gs = j["gameState"];
    size_t chests = 0, npcs = gs["npcWalk"]["slots"].values.size();
    bool all_here = true;
    for (const auto &o : gs["worldObjects"].values) {
        if (o["chest"].truth()) ++chests;
        all_here = all_here && o["location"].integer() == 17;
    }
    expect(j["version"].integer() == 1 && gs["transport"].string == save::Json("foot").string && gs["wind"].integer() == 4 &&
               gs["npcWalk"]["location"].integer() == 17 && npcs > 0 && chests > 0 && all_here && !gs.has("dungeon") &&
               j["qol"]["journal"].kind == save::Json::Array && gs["hmsCapeToggle"].integer() == 0 && gs["openDoors"].values.empty(),
           "J7", "the generated sidecar: version 1, foot, wind 4 (B2), npcWalk of Lord British's Castle (" + std::to_string(npcs) +
               " NPCs at their posts, L1), its " + std::to_string(chests) + " chests re-seeded, no dungeon, Native-only fields at their defaults");

    // Independently loadable: a reboot, the slot's own load, every field.
    {
        tdeck::AlphaSaveService fresh;
        const bool loaded = load_slot_with(fresh, 0);
        const auto &g = g_h->g();
        bool same = loaded && g.party.party_size == 3 && g.gold == 9200 && g.position.map.location == 17 && g.position.xy.x == 15 &&
                    g.position.xy.y == 20 && g.time.year == 139 && g.time.month == 4 && g.time.day == 14 && g.time.hour == 3 &&
                    g.time.minute == 39;
        for (int m = 0; m < 3 && same; ++m) {
            const uint8_t *r = g_gam.data() + gam::kRoster + m * gam::kRecord;
            same = std::strncmp(g.party.characters[m].name, reinterpret_cast<const char *>(r), 9) == 0 &&
                   g.party.characters[m].max_hp == (r[18] | r[19] << 8) && g.party.characters[m].level == r[22];
        }
        expect(same, "J8", "after a reboot Slot 1 loads by itself: Kojac, Shamino, Iolo; 9200 gold; Lord British's Castle (15,20); 139-4-14 03:39");
    }
    // Through the title: the NPCs are there.
    h.main_menu();
    h.key('j');
    h.down();
    h.key('\r');
    h.select(0);
    h.key('\r');
    const auto *actors = h.rt->command_context_for_test().actors;
    expect(!h.rt->frontend_open() && is_kojac() && actors && actors->count > 0 && h.rt->objects_for_test().size() > 0, "J9",
           "Journey Onward -> Load Game -> Slot 1: the game starts in the castle with its people (" +
               std::to_string(actors ? actors->count : 0) + " NPCs) and objects (" + std::to_string(h.rt->objects_for_test().size()) + ")");

    // Slot independence and the overwrite question.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA);
        save_to(svc, 2, kC);
    }
    const Card before = take_card();
    h.import_enter(1);
    const Card after = take_card();
    const auto j10 = h.footer();
    expect(j10 == "Imported into Slot 2. The PC files are kept" && slot_of(after, 0) == slot_of(before, 0) &&
               slot_of(after, 2) == slot_of(before, 2) && !slot_of(after, 1).empty() && boot_slot_is(0, kA) && boot_slot_is(2, kC),
           "J10", "import into Slot 2: Slots 1 and 3 byte for byte, and each still loads its own journey (\"" + j10 + "\", state " +
               std::to_string(int(h.state())) + ", before: " + names(before) + ", after: " + names(after) + ")");
    {
        tdeck::AlphaSaveService fresh;
        expect(load_slot_with(fresh, 1) && is_kojac(), "J11", "Slot 2 loads the imported journey");
    }

    // Occupied: the question starts on No; No writes nothing.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 1, kB);
    }
    const Card occupied = take_card();
    h.import_enter(1);
    const auto question = h.title();
    const int start = h.view().selected_line;
    host_sd::reset_counters();
    h.key('\r');   // No
    expect(question == "Replace Slot 2?" && start == 0 && h.state() == FrontendState::PcImportSlot &&
               h.footer() == "Import cancelled. Slot 2 is unchanged" && no_writes(host_sd::counters()) && take_card() == occupied &&
               boot_slot_is(1, kB),
           "J12", "an occupied slot asks \"Replace Slot 2?\" on No; No (and so Back) leaves the slot byte for byte");
    h.key('\r');   // the question again
    h.down();
    h.key('\r');   // Yes
    const int newest = newest_gen(1);
    FrontendSaveSlot gens[2];
    tdeck::AlphaSaveService().inspect_generations(1, gens);
    {
        tdeck::AlphaSaveService fresh;
        const bool imported = load_slot_with(fresh, 1) && is_kojac();
        expect(imported && gens[0].valid && gens[1].valid && newest >= 0 && seq(1, newest) > seq(1, 1 - newest), "J13",
               "Yes: the imported generation is Slot 2's newest and loads; the journey it replaced stays as the slot's older generation (A/B)");
    }
    // Idempotency: the same files again ask first, even for an empty slot.
    h.pc_page();
    const auto again = h.subtitle();
    h.select(0);
    h.key('\r');
    h.select(2);
    host_sd::reset_counters();
    h.key('\r');
    const auto repeat = h.title(), why = h.subtitle();
    h.key('\r');   // No
    const bool untouched = take_card() == take_card() && slot_of(take_card(), 2).empty() && no_writes(host_sd::counters());
    expect(again == "PC save: Kojac, Lord British's Castle (in Slot 2)" && repeat == "Import it again?" &&
               why == "These PC files went into Slot 2 before" && untouched,
           "J14", "the same PC files, presented again: the page names the slot they went to, and even an empty slot asks \"Import it again?\" (No writes nothing)");
    h.key('\r');
    h.down();
    h.key('\r');   // Yes, deliberately
    expect(h.footer() == "Imported into Slot 3. The PC files are kept" && !slot_of(take_card(), 2).empty(), "J15",
           "a deliberate second import (Yes) is allowed and goes where the player chose");
    // Still on the page, straight away: Import again must know what just happened.
    const Card now = take_card();
    h.select(0);
    h.key('\r');   // Import
    h.select(0);   // Slot 1: empty
    host_sd::reset_counters();
    h.key('\r');
    const auto at_once = h.title(), since = h.subtitle();
    h.key('\r');   // No
    expect(at_once == "Import it again?" && since == "These PC files went into Slot 3 before" && no_writes(host_sd::counters()) &&
               take_card() == now,
           "J17", "without leaving the page, Import at once asks \"Import it again?\" (the page was told where the files went); No writes nothing");
    expect(import_dir_state() == source && g_import_writes == 0, "J16", "after five imports and refusals the PC files are still byte for byte what was copied in");
}

// ======================================================================= F
void test_failures(Harness &h) {
    std::printf("\n[F] failures during import leave the destination recoverable\n");
    put_import(&g_gam, &g_ool);
    // A read failure after the page listed the files.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 1, kB);
    }
    const Card before = take_card();
    h.pc_page();
    h.select(0);
    h.key('\r');
    h.select(0);
    host_sd::faults().fail_open = "import/SAVED.GAM";
    h.key('\r');
    host_sd::faults() = host_sd::Faults{};
    const auto err = h.view().lines[0] ? std::string(h.view().lines[0]) : "";
    const bool error_page = h.state() == FrontendState::Error && h.title() == "PC Save Transfer";
    h.key(' ');
    expect(error_page && err == "The PC save could not be read from SD" && h.state() == FrontendState::PcTransfer && take_card() == before,
           "F1", "an SD read failure: the error page (\"" + err + "\"), then back to PC Save Transfer; the card unchanged");

    // A write failure inside the slot transaction, at every stage.
    struct Stage { const char *name; bool rename; const char *path; };
    const Stage stages[] = {{"gam temp", false, "alpha1-s2-g0.gam.tmp"}, {"ool temp", false, "alpha1-s2-g0.ool.tmp"},
                            {"json temp", false, "alpha1-s2-g0.json.tmp"}, {"gam rename", true, "alpha1-s2-g0.gam"},
                            {"json rename", true, "alpha1-s2-g0.json"}, {"commit temp", false, "alpha1-s2-g0.commit.tmp"},
                            {"commit rename", true, "alpha1-s2-g0.commit"}};
    bool all = true;
    std::string bad;
    for (const auto &s : stages) {
        put_card(before);   // Slot 2 = kB in g1, so the import targets g0
        h.pc_page();
        h.select(0);
        h.key('\r');
        h.select(1);
        h.key('\r');        // the question
        h.down();
        (s.rename ? host_sd::faults().fail_rename : host_sd::faults().fail_write) = s.path;
        h.key('\r');        // Yes
        host_sd::faults() = host_sd::Faults{};
        const bool failed = h.state() == FrontendState::Error && h.view().lines[0] &&
                            std::string(h.view().lines[0]) == "Import failed. Slot 2 is unchanged";
        h.key(' ');
        const bool kept = boot_slot_is(1, kB) && take_card().count("alpha1-s2-g1.gam") && slot_of(take_card(), 1).count("alpha1-s2-g1.commit") &&
                          file(take_card(), "alpha1-s2-g1.json") == file(before, "alpha1-s2-g1.json");
        if (!failed || !kept) { all = false; bad += std::string(" ") + s.name + (failed ? "" : "(not reported)") + (kept ? "" : "(lost)"); }
    }
    expect(all, "F2", "a write or rename failing at each stage of the slot transaction (gam/ool/json temps, renames, commit): the import reports failure and Slot 2's previous generation still loads, its files intact" + bad);
    // The post-write check failing: a short read of the new generation.
    put_card(before);
    h.pc_page();
    h.select(0);
    h.key('\r');
    h.select(1);
    h.key('\r');
    h.down();
    host_sd::faults().short_read = "alpha1-s2-g0.json";
    h.key('\r');
    const bool reported = h.state() == FrontendState::Error;
    const bool fallback = boot_slot_is(1, kB);
    host_sd::faults() = host_sd::Faults{};
    h.key(' ');
    expect(reported && fallback, "F3", "the post-write check failing (the new generation reads back short): failure is reported and the slot still restores its previous journey");
    // Retry without the fault.
    h.import_enter(1);
    h.down();
    h.key('\r');
    {
        tdeck::AlphaSaveService fresh;
        expect(load_slot_with(fresh, 1) && is_kojac(), "F4", "retrying after the fault imports normally");
    }
}

// ======================================================================= X
void test_export(Harness &h) {
    std::printf("\n[X] export\n");
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA);
    }
    std::error_code ec;
    fs::remove_all(fs::path(host_sd::root()) / "sd/ultima5/export", ec);
    const Card saves = take_card();
    std::string native_gam;
    host_sd::read_card_file(gen_path(0, 1, "gam").c_str(), native_gam);
    h.export_enter(0);
    const Card out = take_dir("export");
    const auto notice = h.footer();
    expect(notice == "Slot 1 written to /ultima5/export/slot1" && names(out) == "slot1/EXPORT.TXT slot1/SAVED.GAM slot1/SAVED.OOL" &&
               file(out, "slot1/SAVED.GAM").size() == 4192 && file(out, "slot1/SAVED.OOL").size() == 512 && take_card() == saves,
           "X1", "Export -> Slot 1: export/slot1 holds SAVED.GAM (4192 B), SAVED.OOL (512 B) and EXPORT.TXT only; the slot's files byte for byte (\"" + notice + "\")");
    const Bytes eg = bytes_of(file(out, "slot1/SAVED.GAM"));
    expect(gam::u16(eg, gam::kGold) == kA.gold && eg[gam::kKarma] == kA.karma && eg[gam::kX] == kA.x && eg[gam::kY] == kA.y &&
               eg[gam::kMinute] == kA.minute && eg[gam::kLocation] == 0 && eg[gam::kPartySize] == 1 &&
               std::strncmp(reinterpret_cast<const char *>(eg.data() + 2), "Avatar", 6) == 0,
           "X2", "the exported bytes, read at the documented offsets: gold, karma, position, minute, party and the Avatar's name");
    size_t other = 0;
    for (size_t i = 0; i < 4192; ++i) other += uint8_t(native_gam[i]) != eg[i] && !bridged(i) && !(i >= gam::kTable && i < gam::kTable + 256);
    const auto text = file(out, "slot1/EXPORT.TXT");
    expect(other == 0 && text.find("{") == std::string::npos && eg.end() == std::search(eg.begin(), eg.end(), std::begin("U5PARTIDA"), std::end("U5PARTIDA") - 1) &&
               text.find("Native-only state") != std::string::npos,
           "X3", "the export is the slot's own .gam plus the bridged cells only; no JSON, envelope, commit or slot data in it; EXPORT.TXT names what was left behind");
    // The generation a load would restore, never the damaged one.
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kB);
    }
    const int newest = newest_gen(0);
    tear(0, newest);
    const Card torn = take_card();
    h.export_enter(0);
    const Bytes fb = bytes_of(file(take_dir("export"), "slot1/SAVED.GAM"));
    expect(h.footer() == "Slot 1 written to /ultima5/export/slot1" && gam::u16(fb, gam::kGold) == kA.gold && take_card() == torn,
           "X4", "Slot 1's newest generation is torn: the export is the one before it (kA, what load_slot restores), never the damaged kB; the slot untouched");
    tear(0, 1 - newest);
    const Card dead = take_card();
    tdeck::AlphaSaveService::PcExportResult r;
    const auto none = tdeck::AlphaSaveService().export_pc_save(0, r);
    // The runtime's list still holds both commit records unchanged (the A4-SAVE2
    // list memory is keyed on them), so the menu offers the slot: the export's
    // own gate refuses it.
    h.export_enter(0);
    const auto gate = h.view().lines[0] ? std::string(h.view().lines[0]) : "";
    const bool gate_error = h.state() == FrontendState::Error;
    h.key(' ');
    expect(none == tdeck::AlphaSaveService::PcExport::NoLoadable && gate_error && gate == "Slot 1 has no loadable save" &&
               bytes_of(file(take_dir("export"), "slot1/SAVED.GAM")) == fb && take_card() == dead,
           "X5", "no loadable generation: the export gate refuses it (NoLoadable, \"" + gate + "\"), even when the list still offers it; the previous export is not replaced; the slot untouched");
    // With changed commit records the list reads the slot again: Damaged, and the menu refuses it itself.
    for (int g = 0; g < 2; ++g) {
        tdeck::AlphaSaveCommit rec;
        commit_of(0, g, rec);
        rec.json ^= 1u;
        host_sd::write_card_file(gen_path(0, g, "commit").c_str(), std::string(reinterpret_cast<const char *>(&rec), sizeof(rec)));
    }
    h.export_enter(0);
    expect(h.footer() == "Slot 1 cannot be loaded; nothing to export" && h.state() == FrontendState::PcExportSlot &&
               bytes_of(file(take_dir("export"), "slot1/SAVED.GAM")) == fb,
           "X5b", "a slot the list knows is Damaged: the menu says \"cannot be loaded; nothing to export\" and sends no export (\"" + h.footer() + "\")");
    h.export_enter(2);
    expect(h.footer() == "Slot 3 is empty" && h.state() == FrontendState::PcExportSlot, "X6", "an empty slot: \"Slot 3 is empty\", nothing exported");
    // A write failure: the previous export stays.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kC);
    }
    h.export_enter(0);
    const std::string previous = file(take_dir("export"), "slot1/SAVED.GAM");
    {
        tdeck::AlphaSaveService svc;
        save_to(svc, 0, kA);
    }
    const Card c = take_card();
    host_sd::faults().fail_write = "export/slot1/SAVED.GAM";
    h.export_enter(0);
    host_sd::faults() = host_sd::Faults{};
    const auto err = h.view().lines[0] ? std::string(h.view().lines[0]) : "";
    const bool on_error = h.state() == FrontendState::Error;
    h.key(' ');
    expect(on_error && err == "Export failed: SD card write error" && previous.size() == 4192 &&
               gam::u16(bytes_of(previous), gam::kGold) == kC.gold && file(take_dir("export"), "slot1/SAVED.GAM") == previous &&
               take_card() == c && !take_dir("export").count("slot1/SAVED.GAM.tmp"),
           "X7", "an SD write failure while exporting kA over an earlier kC export: the error page, the kC files byte for byte, no temp left, the slot untouched (\"" +
               err + "\")");
    // Dungeon: refused.
    blank_card();
    {
        tdeck::AlphaSaveService svc;
        g_h->reset_game();
        apply(kA);
        auto &d = g_h->rt->command_context_for_test();
        d.game.position.map = {35, 0};
        d.game.position.xy = {1, 1};
        uint32_t ms = 0;
        svc.save(d, g_o->outdoor, g_o->terrain, g_o->actors, g_o->retained, g_owners->initial_gam, g_owners->initial_gam_size,
                 g_owners->initial_ool, g_owners->initial_ool_size, ms, false, 1);
    }
    fs::remove_all(fs::path(host_sd::root()) / "sd/ultima5/export", ec);
    h.export_enter(1);
    const auto why = h.view().lines[0] ? std::string(h.view().lines[0]) : "";
    h.key(' ');
    expect(why == "Dungeon saves cannot be transferred" && take_dir("export").empty(), "X8", "a save inside a dungeon: refused, nothing written (L2)");
}

// ======================================================================= T
// The emitted round trip, for check-pc-save.ts: the modified journey's state as
// Native holds it, and the files exported from it.
std::string g_emit;
void emit_file(const std::string &name, const std::string &bytes) {
    if (g_emit.empty()) return;
    std::ofstream f(fs::path(g_emit) / name, std::ios::binary);
    f << bytes;
}
std::string state_json(const GameState &g, const TurnState &t, const save::Json &doc) {
    save::Json s = save::Json::object();
    s["gold"] = save::Json(int(g.gold)); s["food"] = save::Json(int(g.food)); s["karma"] = save::Json(int(g.karma));
    s["location"] = save::Json(int(g.position.map.location)); s["floor"] = save::Json(int(g.position.map.floor));
    s["x"] = save::Json(int(g.position.xy.x)); s["y"] = save::Json(int(g.position.xy.y));
    s["partySize"] = save::Json(int(g.party.party_size));
    s["wind"] = save::Json(int(t.wind)); s["sailDir"] = save::Json(int(t.sail_dir));
    s["names"] = save::Json::array();
    for (int m = 0; m < g.party.party_size; ++m) s["names"].values.emplace_back(save::Json(g.party.characters[m].name));
    s["search"] = save::Json::array();
    for (int i = 0; i < 113; ++i)
        if (g.quest.search_found[i / 8] & (1u << (i % 8))) s["search"].values.emplace_back(i);
    s["frigates"] = save::Json::array();
    for (const auto &o : doc["worldObjects"].values)
        if (o["ship"].truth() && o["location"].integer() == 0) {
            save::Json f = save::Json::object();
            f["floor"] = o["floor"]; f["x"] = o["x"]; f["y"] = o["y"]; f["hull"] = o["hull"]; f["skiffs"] = o["skiffs"];
            s["frigates"].values.push_back(f);
        }
    return json_text(s);
}
void test_round_trip(Harness &h) {
    std::printf("\n[T] original -> Native -> modified -> original\n");
    blank_card();
    put_import(&g_gam, &g_ool);
    std::error_code ec;
    fs::remove_all(fs::path(host_sd::root()) / "sd/ultima5/export", ec);
    h.import_enter(0);
    h.main_menu();
    h.key('j');
    h.down();
    h.key('\r');
    h.select(0);
    h.key('\r');   // the imported journey, live
    auto &c = h.rt->command_context_for_test();
    c.game.gold = 1234;
    c.game.position.xy.x = 16;
    h.rt->turn().wind = 2;
    c.game.quest.search_found[2] |= 0x10;
    c.game.quest.search_present[2] |= 0x10;   // search:20, as quest_search.cpp marks a find
    QuestObject ship;
    ship.location = 0; ship.floor = 0; ship.x = 0x3a; ship.y = 0x6b; ship.tile = 0x125; ship.hull = 61; ship.skiffs = 1; ship.ship = true;
    c.quest_world->append(c.quest_world->context, ship);   // docked outside the castle
    const bool saved = h.alt_save();
    h.export_enter(0);
    const Card out = take_dir("export");
    const Bytes eg = bytes_of(out.count("slot1/SAVED.GAM") ? file(out, "slot1/SAVED.GAM") : std::string());
    const Bytes eo = bytes_of(out.count("slot1/SAVED.OOL") ? file(out, "slot1/SAVED.OOL") : std::string());
    bool ship_out = false;
    for (int i = 1; i < 32 && eo.size() == 512; ++i)
        ship_out |= eo[i * 8] == 0x25 && eo[i * 8 + 2] == 0x3a && eo[i * 8 + 3] == 0x6b && eo[i * 8 + 5] == 61 && eo[i * 8 + 7] == 1;
    expect(saved && eg.size() == 4192 && gam::u16(eg, gam::kGold) == 1234 && eg[gam::kX] == 16 && eg[gam::kWind] == 2 &&
               (eg[gam::kSearch + 2] & 0x10) && ship_out,
           "T1", "the imported journey, changed (1234 gold, one step east, wind South, search item 20 found, a frigate docked outside) and saved, exports those changes");
    // Everything the original had and nobody changed is still the original's byte.
    std::string lost;
    size_t changed = 0;
    for (size_t i = 0; i < 4192 && eg.size() == 4192; ++i) {
        if (eg[i] == g_gam[i]) continue;
        const bool expected = i == gam::kGold || i == gam::kGold + 1 || i == gam::kX || i == gam::kWind || i == gam::kSearch + 2 ||
                              i == gam::kTable + 2 /* obj0 x follows the party */;
        if (expected) { ++changed; continue; }
        if (unbridged(i)) continue;
        char w[16];
        std::snprintf(w, sizeof(w), " 0x%03zx", i);
        lost += w;
    }
    expect(lost.empty() && changed >= 5, "T2", "every other byte the original had is still the original's, except the documented unbridged ones (L1/L5)" + lost);
    // The exported files, imported again (a DOS round trip without DOS).
    put_import(&eg, &eo);
    h.import_enter(1);
    {
        tdeck::AlphaSaveService fresh;
        const bool loaded = load_slot_with(fresh, 1);
        const auto &g = g_h->g();
        const auto &t = g_h->rt->command_context_for_test().turn;
        bool ship_back = false;
        for (const auto &o : g_o->retained["worldObjects"].values)
            ship_back |= o["ship"].truth() && o["x"].integer() == 0x3a && o["y"].integer() == 0x6b && o["hull"].integer() == 61;
        expect(loaded && g.gold == 1234 && g.position.xy.x == 16 && t.wind == 2 && (g.quest.search_found[2] & 0x10) && ship_back &&
                   std::strcmp(g.party.characters[1].name, "Shamino") == 0,
               "T3", "the export imported back: gold, position, wind, the found item and the docked frigate survive; the party is intact");
        expect(g_o->retained["hmsCapeToggle"].integer() == 0 && g_o->retained["openDoors"].values.empty() && !g_o->retained.has("dungeon"),
               "T4", "Native-only state after the PC round trip is at its documented defaults (HMS Cape toggle 0, no open doors, no session)");
        if (!g_emit.empty()) {
            emit_file("roundtrip-SAVED.GAM", str(eg));
            emit_file("roundtrip-SAVED.OOL", str(eo));
            emit_file("roundtrip-state.json", state_json(g, t, g_o->retained));
        }
    }
    if (!g_emit.empty()) {
        // The fixture as Native holds it after import, for the reference to compare.
        GameState g{};
        TurnState t{};
        save::Json d;
        pc::import_original(g_gam.data(), g_ool.data(), g, t, d, nullptr);
        emit_file("fixture-state.json", state_json(g, t, d));
    }
    // Outdoors, with vehicles and a monster, through the runtime: import, load, export.
    const Bytes outdoor = variant(outdoors);
    const Bytes under = ool_with_frigate(1, 0x20, 0x21, 55, 2);
    blank_card();
    put_import(&outdoor, &under);
    h.import_enter(0);
    const auto imported = h.footer();
    h.main_menu();
    h.key('j');
    h.down();
    h.key('\r');
    h.select(0);
    h.key('\r');
    auto &live = h.rt->command_context_for_test();
    size_t pool_frigates = 0, cells = 0;
    for (const auto &o : h.rt->objects_for_test())
        pool_frigates += o.ship && o.location == 0 && ((o.floor == 0 && o.x == 0x51 && o.hull == 77) || (o.floor == 255 && o.x == 0x20 && o.hull == 55));
    for (const auto &cell : live.terrain->persistent)
        cells += cell.map.location == 0 && ((cell.map.floor == 0 && cell.x == 0x4f && cell.y == 0x60 && cell.tile == 0x110) ||
                                            (cell.map.floor == 0 && cell.x == 0x52 && cell.y == 0x61 && cell.tile == 0x128) ||
                                            (cell.map.floor == 255 && cell.x == 14 && cell.y == 242 && cell.tile == 0x129));
    const bool monster = live.outdoor->enemies.size() == 1 && live.outdoor->enemies[0].definition == 1 && live.outdoor->enemies[0].x == 0x48;
    expect(imported == "Imported into Slot 1. The PC files are kept" && !h.rt->frontend_open() && live.game.position.map.location == 0 &&
               pool_frigates == 2 && cells == 3 && monster,
           "T5", "outdoors through the runtime: both frigates in the pool, the horse, the skiff and the new-game underworld skiff as terrain cells, the monster on the map (" +
               std::to_string(pool_frigates) + " frigates, " + std::to_string(cells) + " cells, " + std::to_string(live.outdoor->enemies.size()) + " monsters)");
    fs::remove_all(fs::path(host_sd::root()) / "sd/ultima5/export", ec);
    h.export_enter(0);
    const Card v = take_dir("export");
    const Bytes vg = bytes_of(file(v, "slot1/SAVED.GAM")), vo = bytes_of(file(v, "slot1/SAVED.OOL"));
    auto table_vehicles = [](const uint8_t *table) {
        std::multiset<std::string> out;
        for (int i = 1; i < 32; ++i) {
            const int b = table[i * 8];
            if ((b & 0xf8) != 0x20 && (b & 0xfc) != 0x28 && (b & 0xfe) != 0x10 && b != 0x44) continue;
            char k[48];
            std::snprintf(k, sizeof(k), "%02x@%d,%d h%d s%d", b, table[i * 8 + 2], table[i * 8 + 3], table[i * 8 + 5], table[i * 8 + 7]);
            out.insert(k);
        }
        return out;
    };
    const bool same_live = vg.size() == 4192 && table_vehicles(vg.data() + gam::kTable) == table_vehicles(outdoor.data() + gam::kTable);
    const bool same_under = vo.size() == 512 && table_vehicles(vo.data() + 256) == table_vehicles(under.data() + 256);
    expect(same_live && same_under, "T6",
           "exported again: the live table holds the same frigate, horse, skiff and monster, and UNDER.OOL the same parked frigate and new-game skiff (records may move slots)");
    if (!g_emit.empty()) {
        emit_file("vehicles-in-SAVED.GAM", str(outdoor));
        emit_file("vehicles-in-SAVED.OOL", str(under));
        emit_file("vehicles-out-SAVED.GAM", str(vg));
        emit_file("vehicles-out-SAVED.OOL", str(vo));
    }
}

// ======================================================================= R
void test_recovery(Harness &h) {
    std::printf("\n[R] an imported slot follows the ordinary A/B rules\n");
    blank_card();
    put_import(&g_gam, &g_ool);
    h.import_enter(1);
    const uint64_t first = seq(1, 1);
    h.main_menu();
    h.key('j');
    h.key('\r');   // Continue: the slot saved last
    const bool continued = !h.rt->frontend_open() && is_kojac() && h.rt->save_service_for_test().last_slot() == 1;
    h.rt->game().gold = 4321;
    const bool s1 = h.alt_save();
    h.rt->game().gold = 4322;
    const bool s2 = h.alt_save();
    FrontendSaveSlot gens[2];
    tdeck::AlphaSaveService().inspect_generations(1, gens);
    expect(continued && s1 && s2 && gens[0].valid && gens[1].valid && seq(1, 0) > first && seq(1, 1) > seq(1, 0) &&
               names(slot_of(take_card(), 1)) == "alpha1-s2-g0.commit alpha1-s2-g0.gam alpha1-s2-g0.json alpha1-s2-g0.ool alpha1-s2-g1.commit alpha1-s2-g1.gam alpha1-s2-g1.json alpha1-s2-g1.ool",
           "R1", "Continue restores the imported slot; two saves rotate its pair like any Native slot, one card-wide sequence");
    tear(1, newest_gen(1));
    const auto cat = cold_catalog();
    tdeck::AlphaSaveService fresh;
    expect(cat.slots[1].status == SaveSlotStatus::Recovered && load_slot_with(fresh, 1) && g_h->g().gold == 4321, "R2",
           "its newest generation torn: listed Recovered, and it loads the one before (4321 gold)");
}

// ======================================================================= H
void test_heap_io(Harness &h) {
    std::printf("\n[H] card I/O and heap\n");
    blank_card();
    put_import(&g_gam, &g_ool);
    h.pc_page();
    h.select(0);
    h.key('\r');
    h.select(0);
    g_import_reads = 0;
    host_sd::reset_counters();
    h.key('\r');   // the import, then the page re-reads the folder
    const auto io = host_sd::counters();
    expect(g_import_reads > 0 && io.max_open_handles <= 2, "H1",
           "an import reads the PC files through the fake card (" + std::to_string(g_import_reads) + " reads) and never holds more than 2 files open (" +
               std::to_string(io.max_open_handles) + ")");
    // Export's small-heap peak stays within a load's (one staged document at a time).
    tdeck::AlphaSaveService svc;
    auto base = g_heap;
    g_heap.peak_small = g_heap.live_small;
    load_slot_with(svc, 0);
    const int64_t load_peak = int64_t(g_heap.peak_small) - int64_t(base.live_small);
    base = g_heap;
    g_heap.peak_small = g_heap.live_small;
    tdeck::AlphaSaveService::PcExportResult r;
    svc.export_pc_save(0, r);
    const int64_t export_peak = int64_t(g_heap.peak_small) - int64_t(base.live_small);
    const int64_t kept = int64_t(g_heap.live) - int64_t(base.live);
    expect(export_peak <= load_peak + 16384 && kept <= 0, "H2",
           "export's small-block peak " + std::to_string(export_peak) + " B vs a load's " + std::to_string(load_peak) + " B (one document at a time); " +
               std::to_string(kept) + " B kept afterwards");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_save3_pc_bridge_runtime <openu5-alpha1-resources.bin> <dir with SAVED.GAM, SAVED.OOL> [--emit <dir>]\n");
        return 2;
    }
    for (int i = 3; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], "--emit")) g_emit = argv[i + 1];
    g_gam = read_host(fs::path(argv[2]) / "SAVED.GAM");
    g_ool = read_host(fs::path(argv[2]) / "SAVED.OOL");
    if (g_gam.size() != 4192 || g_ool.size() != 512) {
        std::fprintf(stderr, "the DOS fixture SAVED.GAM/SAVED.OOL is not in %s\n", argv[2]);
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    const auto dir = fs::temp_directory_path() / "openu5-a4-save3-card";
    host_sd::set_root(dir.string());
    host_sd::format_card();
    host_sd::set_probe(probe);
    tdeck::AlphaSaveService::reserve_dma_headroom();

    Harness h;
    Owners o;
    g_h = &h;
    g_o = &o;
    test_bridge();
    test_import(h);
    test_failures(h);
    test_export(h);
    test_round_trip(h);
    test_recovery(h);
    test_heap_io(h);

    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("\na4_save3_pc_bridge_runtime: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
