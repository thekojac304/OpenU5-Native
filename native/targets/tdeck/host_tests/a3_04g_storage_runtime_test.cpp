// Alpha 3 A3-04G -- the System Menu's save inspection and the storage heap.
//
// The REAL alpha_save.cpp (never host-compiled before this batch) over a fake
// SD card (host_tests/sd_shims): "/sd/..." is redirected to a host directory,
// every card call is counted, and faults are injected by path. Around it the
// real AlphaRuntime (Alt+M opens the System Menu exactly as on the device) and
// a census of every operator new/delete in the process.
//
// The device's allocator is not the host's, so this test never claims device
// bytes. It measures what is ALLOCATED, WHEN, and for HOW LONG:
//   * "small" = at most 4,096 B. On the device CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL
//     = 4096 sends exactly these to internal RAM first (PSRAM only when internal
//     cannot hold them); larger ones go to PSRAM first. So small bytes are the
//     host's measure of internal-RAM pressure, in host (64-bit) sizes.
//   * what an operation leaves allocated after it returns (retention);
//   * what is allocated while the card is read or written (the Alpha 2.0 class:
//     every sector the device reads into PSRAM needs a DMA bounce buffer from
//     internal RAM, sdmmc_cmd.c allocate_dma_buf);
//   * which files are opened and how many bytes are read per menu open.
//
// Groups: C census (informational + sanity), R retention and I/O-time
// pressure, S repeated open/close, K the save-list cache and its invalidation,
// F failure paths, V save/load correctness through the real shell.
#include "../main/alpha_runtime.h"
#include "../main/alpha_save.h"
#include "../main/alpha_save_generation.h"
#include "esp_heap_caps.h"
#include "sd_shims/host_sd_card.h"

#include "openu5/save_json.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

// ---------------------------------------------------------------- heap census
namespace {
struct HeapCensus {
    uint64_t live = 0, live_small = 0, allocs = 0, frees = 0, small_allocs = 0;
    uint64_t peak_small = 0;
    uint64_t trough_small = 0;   // lowest live_small since the last reset
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
    ++g_heap.allocs;
    if (n <= kSmall) {
        g_heap.live_small += n;
        ++g_heap.small_allocs;
        if (g_heap.live_small > g_heap.peak_small) g_heap.peak_small = g_heap.live_small;
    }
    return reinterpret_cast<char *>(p) + kHeader;
}
void census_free(void *q) noexcept {
    if (!q) return;
    auto *p = reinterpret_cast<size_t *>(static_cast<char *>(q) - kHeader);
    const size_t n = p[0];
    g_heap.live -= n;
    ++g_heap.frees;
    if (n <= kSmall) {
        g_heap.live_small -= n;
        if (g_heap.live_small < g_heap.trough_small) g_heap.trough_small = g_heap.live_small;
    }
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

// The probe the fake card runs at every read and write.
uint64_t g_io_small_max = 0, g_io_live_max = 0;
uint32_t g_io_events = 0;
// At every card read or write: the small bytes allocated since the lowest point
// of the operation so far. An operation that starts with an earlier document
// still held frees it on the way (the trough), so this sees what is BUILT and
// still alive when the card is touched, however the operation began.
void io_probe(const char *, const char *) {
    ++g_io_events;
    g_io_small_max = std::max(g_io_small_max, g_heap.live_small - g_heap.trough_small);
    g_io_live_max = std::max(g_io_live_max, g_heap.live);
}

struct Measure {
    host_sd::Counters card{};
    int64_t retained = 0, retained_small = 0;   // live after - live before
    int64_t peak_small = 0;                     // above the start
    int64_t io_small = 0;                       // small bytes above the start at the busiest card call
    uint64_t allocs = 0;
    uint64_t peak_small_abs = 0;                // the census's own peak, not relative
    double host_ms = 0;
};
template <class F> Measure measure(F &&f) {
    host_sd::reset_counters();
    const auto base = g_heap;
    g_heap.peak_small = g_heap.live_small;
    g_heap.trough_small = g_heap.live_small;
    g_io_small_max = 0;
    g_io_live_max = g_heap.live;
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    Measure m;
    m.card = host_sd::counters();
    m.retained = int64_t(g_heap.live) - int64_t(base.live);
    m.retained_small = int64_t(g_heap.live_small) - int64_t(base.live_small);
    m.peak_small = int64_t(g_heap.peak_small) - int64_t(base.live_small);
    m.peak_small_abs = g_heap.peak_small;
    m.io_small = int64_t(g_io_small_max);
    m.allocs = g_heap.allocs - base.allocs;
    m.host_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return m;
}
void print(const char *what, const Measure &m) {
    std::printf("         %-26s opens=%u(fail %u) reads=%u bytes_read=%llu writes=%u bytes_written=%llu "
                "retained=%lld small=%lld peak_small=%lld io_small=%lld allocs=%llu host=%.2f ms\n",
                what, m.card.opens, m.card.open_failures, m.card.reads, (unsigned long long)m.card.bytes_read,
                m.card.writes, (unsigned long long)m.card.bytes_written, (long long)m.retained,
                (long long)m.retained_small, (long long)m.peak_small, (long long)m.io_small,
                (unsigned long long)m.allocs, m.host_ms);
}

bool same_slot(const FrontendSaveSlot &a, const FrontendSaveSlot &b) {
    return a.present == b.present && a.valid == b.valid && a.sequence == b.sequence &&
           std::strcmp(a.name, b.name) == 0;
}
bool same_slots(const FrontendSaveSlot (&a)[2], const FrontendSaveSlot (&b)[2]) {
    return same_slot(a[0], b[0]) && same_slot(a[1], b[1]);
}
void describe(const FrontendSaveSlot (&s)[2], const char *tag) {
    std::printf("         %s: [%s%s seq=%llu '%s'] [%s%s seq=%llu '%s']\n", tag, s[0].present ? "P" : "-",
                s[0].valid ? "V" : "-", (unsigned long long)s[0].sequence, s[0].name, s[1].present ? "P" : "-",
                s[1].valid ? "V" : "-", (unsigned long long)s[1].sequence, s[1].name);
}
// A cold look at the card: a fresh service, so nothing it knew before counts.
void cold_inspect(FrontendSaveSlot (&out)[2]) {
    tdeck::AlphaSaveService fresh;
    fresh.inspect(out);
}
// Alpha 4 A4-SAVE2: the menus' save list, cold, and Slot 1's row and footer
// on the Load page as a cold list would show them.
void cold_catalog(FrontendSaveCatalog &out) {
    tdeck::AlphaSaveService fresh;
    fresh.inspect_catalog(out);
}
// A4-UI3: the row is its two lines ("row|detail"), Slot 1 marked CURRENT (the
// journey's slot in every case that uses it).
void slot1_page(const FrontendSaveCatalog &c, std::string &row, std::string &detail) {
    char r[96], r2[96], d[96];
    format_slot_rows(r, r2, sizeof(r), c, 0, 0, kCurrentSlotTag);
    format_slot_detail(d, sizeof(d), c, 0, false);
    row = std::string(r) + "|" + r2; detail = d;
}
bool same_catalog(const FrontendSaveCatalog &a, const FrontendSaveCatalog &b) {
    for (int i = 0; i < kSaveSlotCount; ++i)
        if (a.slots[i].status != b.slots[i].status || a.slots[i].sequence != b.slots[i].sequence ||
            !same_slot(a.slots[i].shown, b.slots[i].shown))
            return false;
    return true;
}

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
    bool key(uint8_t code) { return raw_key(code); }
    bool ball(RawInputKind kind) {
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.timestamp_us = (clock_us += 100000); return rt->handle(raw);
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
    bool close_menu() { key('\b'); return !rt->system_menu_open(); }   // Back at the root = Resume
    // The Load page as the player reads it. Since Alpha 4 A4-SAVE2 it lists
    // Slots 1-3 and starts on the journey's slot (Slot 1 here): g1 is Slot 1's
    // row, g2 the footer describing it (the date and time, or its state).
    bool load_page(std::string &g1, std::string &g2) {
        if (!rt->system_menu_open()) return false;
        ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown); key('\r');
        const auto v = rt->system_menu_view();
        if (v.line_count < 3 || v.selected_line != 0) return false;
        g1 = std::string(v.lines[0]) + "|" + (v.details[0] ? v.details[0] : ""); g2 = v.footer;   // A4-UI3: both rows
        return true;
    }
};

// ------------------------------------------------------------ DOM census
struct DomSize {
    size_t nodes = 0, blocks = 0, small_blocks = 0, large_blocks = 0;
    size_t dev_bytes = 0, dev_small = 0, dev_large = 0;   // 32-bit estimate
};
// ESP32-S3 (32-bit libstdc++): sizeof(Json) = 64 (kind 4 + pad 4, double 8,
// u16string 24, two vectors 12 + 12); sizeof(u16string) = 24 with a 7-unit
// in-object buffer. The TLSF block header is taken as 4 B with 4-B rounding.
// Capacities are the host's: libstdc++'s growth policy is the same.
void dev_block(DomSize &d, size_t bytes) {
    const size_t b = ((bytes + 3) & ~size_t(3)) + 4;
    ++d.blocks;
    d.dev_bytes += b;
    if (bytes <= kSmall) { ++d.small_blocks; d.dev_small += b; }
    else { ++d.large_blocks; d.dev_large += b; }
}
void dev_string(DomSize &d, const std::u16string &s) {
    if (s.capacity() > 7) dev_block(d, (s.capacity() + 1) * 2);
}
void walk(const save::Json &j, DomSize &d) {
    ++d.nodes;
    dev_string(d, j.string);
    if (j.values.capacity()) dev_block(d, j.values.capacity() * 64);
    if (j.keys.capacity()) dev_block(d, j.keys.capacity() * 24);
    for (const auto &k : j.keys) dev_string(d, k);
    for (const auto &v : j.values) walk(v, d);
}

// ------------------------------------------------------------ the groups
struct Card {
    std::string gam[2], ool[2], json[2], commit[2];
    void take() {
        for (int i = 0; i < 2; ++i) {
            host_sd::read_card_file(card_path(i, "gam").c_str(), gam[i]);
            host_sd::read_card_file(card_path(i, "ool").c_str(), ool[i]);
            host_sd::read_card_file(card_path(i, "json").c_str(), json[i]);
            host_sd::read_card_file(card_path(i, "commit").c_str(), commit[i]);
        }
    }
    void put() const {
        for (int i = 0; i < 2; ++i) {
            host_sd::write_card_file(card_path(i, "gam").c_str(), gam[i]);
            host_sd::write_card_file(card_path(i, "ool").c_str(), ool[i]);
            host_sd::write_card_file(card_path(i, "json").c_str(), json[i]);
            host_sd::write_card_file(card_path(i, "commit").c_str(), commit[i]);
        }
    }
};

size_t g_dom_small = 0;     // host small bytes of one staged save document (C2b)
size_t g_verify_peak = 0;   // host small-byte peak of one verify from an empty stage (C2b)

void test_census(Harness &h) {
    std::printf("\n[C] census: two generations, one sidecar DOM, one cold inspect\n");
    host_sd::format_card();
    const bool s1 = h.alt_save();
    h.g().gold = 400;
    const bool s2 = h.alt_save();
    expect(s1 && s2, "C0", "two Alt+S saves through the real alpha_save.cpp both complete");
    Card card;
    card.take();
    for (int i = 0; i < 2; ++i)
        std::printf("         slot %d: gam=%zu ool=%zu json=%zu commit=%zu B\n", i, card.gam[i].size(),
                    card.ool[i].size(), card.json[i].size(), card.commit[i].size());
    expect(card.gam[0].size() == 4192 && card.ool[0].size() == 512 && card.commit[0].size() == 32 &&
               card.json[0].size() > 0 && card.json[1].size() > 0,
           "C1", "each generation is gam 4,192 + ool 512 + commit 32 B + a non-empty sidecar");

    save::Json dom;
    const auto m = measure([&] { save::parse_json(card.json[0], dom); });
    DomSize d;
    walk(dom, d);
    std::printf("         sidecar DOM (host): %llu allocations, %lld B live (%lld B small), peak small %lld B\n",
                (unsigned long long)m.allocs, (long long)m.retained, (long long)m.retained_small,
                (long long)m.peak_small);
    std::printf("         sidecar DOM (32-bit estimate): %zu nodes, %zu blocks (%zu small / %zu large), "
                "%zu B (%zu small / %zu large)\n",
                d.nodes, d.blocks, d.small_blocks, d.large_blocks, d.dev_bytes, d.dev_small, d.dev_large);
    expect(m.retained_small > 0 && m.retained == m.retained_small, "C2",
           "the sidecar itself parses to a small tree (all of it small allocations)");

    // The generation as the gate stages it: the save document (GAM + sidecar)
    // and every owner decoded from it. verify_candidate() is the production
    // gate every inspect, load and post-save check runs per slot.
    {
        auto stage = std::make_unique<tdeck::AlphaSaveStage>();
        tdeck::AlphaSaveCandidate v;
        std::memcpy(&v.commit, card.commit[0].data(), sizeof(v.commit));
        v.gam.assign(card.gam[0].begin(), card.gam[0].end());
        v.ool.assign(card.ool[0].begin(), card.ool[0].end());
        v.json.assign(card.json[0].begin(), card.json[0].end());
        bool ok1 = false, ok2 = false;
        const auto v1 = measure([&] { ok1 = tdeck::verify_candidate(v, *stage); });
        DomSize doc;
        walk(stage->document, doc);
        const auto v2 = measure([&] { ok2 = tdeck::verify_candidate(v, *stage); });
        print("verify_candidate (empty)", v1);
        print("verify_candidate (again)", v2);
        // The stage still holds the first document when the second verify
        // starts; it must be gone before the second import is built.
        const int64_t excess = int64_t(v2.peak_small_abs) - int64_t(v1.peak_small_abs);
        std::printf("         second verify on one stage peaks %lld B above the first\n", (long long)excess);
        expect(ok1 && excess <= int64_t(v1.retained_small / 8), "C4",
               "a verify on a stage that still holds a document never holds both (peak = one verify's)");
        g_verify_peak = size_t(v1.peak_small);
        std::printf("         staged save document (32-bit estimate): %zu nodes, %zu blocks (%zu small / %zu large), "
                    "%zu B (%zu small / %zu large)\n",
                    doc.nodes, doc.blocks, doc.small_blocks, doc.large_blocks, doc.dev_bytes, doc.dev_small,
                    doc.dev_large);
        g_dom_small = size_t(v1.retained_small);
        expect(ok1 && ok2 && g_dom_small > 32 * 1024, "C2b",
               "a verified generation stages a large document of small allocations (> 32 KiB small)");
    }

    FrontendSaveSlot out[2];
    tdeck::AlphaSaveService svc;
    const auto cold = measure([&] { svc.inspect(out); });
    print("cold inspect", cold);
    describe(out, "cold list");
    expect(out[0].valid && out[1].valid && out[0].sequence + out[1].sequence == 3 &&
               std::strcmp(out[0].name, "Avatar") == 0,
           "C3", "a cold inspect lists both generations (sequences 1 and 2, 'Avatar')");
}

// A service of its own, over the runtime's CommandContext and local owners,
// so its save and load start from a service that holds nothing.
struct Owners {
    OutdoorServices outdoor{};
    WorldTerrain terrain{};
    NpcActors actors{};
    save::Json retained{};
    explicit Owners(Harness &h) {
        GameState g{};
        TurnState t{};
        save::SidecarSource source;
        save::load_native_state(g_owners->initial_gam, g_owners->initial_gam_size, nullptr, g, t, retained, source,
                                true);
        (void)h;
    }
};
bool service_save(Harness &h, tdeck::AlphaSaveService &svc, Owners &o) {
    uint32_t ms = 0;
    return svc.save(h.rt->command_context_for_test(), o.outdoor, o.terrain, o.actors, o.retained,
                    g_owners->initial_gam, g_owners->initial_gam_size, g_owners->initial_ool,
                    g_owners->initial_ool_size, ms);
}
bool service_load(Harness &h, tdeck::AlphaSaveService &svc, Owners &o) {
    uint32_t ms = 0;
    return svc.load(h.rt->command_context_for_test(), o.outdoor, o.terrain, o.actors, o.retained, ms);
}

void test_retention(Harness &h) {
    std::printf("\n[R] retention, peak, and what is allocated while the card is busy\n");
    const size_t bound = std::max<size_t>(g_dom_small / 8, 4096);
    const size_t peak_bound = g_verify_peak + g_verify_peak / 8;
    std::printf("         bounds: I/O-time small bytes <= %zu B (1/8 of one staged document); "
                "peak small <= %zu B (one verify from an empty stage + 1/8)\n",
                bound, peak_bound);
    {
        tdeck::AlphaSaveService svc;
        FrontendSaveSlot out[2];
        const auto m = measure([&] { svc.inspect(out); });
        print("inspect (fresh service)", m);
        expect(m.retained_small <= 1024 && m.retained <= 1024, "R1",
               "an inspect keeps nothing allocated after it returns (<= 1 KiB)");
        expect(m.io_small <= int64_t(bound), "R2",
               "no staged generation is alive while the next slot's files are read");
        expect(m.peak_small <= int64_t(peak_bound), "R6",
               "two slots never hold two documents at once: the peak is one verify's");
        const auto again = measure([&] { svc.inspect(out); });
        print("inspect again", again);
        expect(again.retained <= 1024 && again.io_small <= int64_t(bound) &&
                   again.peak_small <= int64_t(peak_bound),
               "R2b", "a second inspect: nothing kept, nothing alive at the card, the same peak");
    }
    {
        tdeck::AlphaSaveService svc;
        Owners o(h);
        bool loaded = false;
        const auto m = measure([&] { loaded = service_load(h, svc, o); });
        print("load (fresh service)", m);
        expect(loaded && m.io_small <= int64_t(bound), "R3",
               "Continue Latest: no staged generation is alive while the other slot is read");
    }
    {
        Owners o(h);
        tdeck::AlphaSaveService warm;
        const bool first = service_save(h, warm, o);   // the document gains its captured sections once
        tdeck::AlphaSaveService svc;
        bool saved = false;
        const auto m = measure([&] { saved = service_save(h, svc, o); });
        print("save (fresh service)", m);
        expect(first && saved && m.io_small <= int64_t(bound), "R4",
               "a save: no second document is alive while the generation is written and read back");
        expect(m.retained <= 2048, "R5", "a save keeps nothing allocated after it returns (<= 2 KiB)");
    }
    {
        const auto m = measure([&] { h.alt_save(); });
        print("Alt+S (runtime)", m);
        const auto l = measure([&] { h.alt_load(); });
        print("Alt+L (runtime)", l);
    }
}

void test_cycles(Harness &h) {
    std::printf("\n[S] 100 System Menu open/close cycles\n");
    h.open_menu();
    h.close_menu();
    std::vector<uint64_t> live, small;
    live.reserve(100);   // reserved first: a growing record would be counted in itself
    small.reserve(100);
    bool menus = true, handles = true, dma = true, windows = true, no_writes = true;
    uint32_t opens_first = 0, opens_rest_max = 0;
    for (int i = 0; i < 100; ++i) {
        host_sd::reset_counters();
        menus &= h.open_menu();
        menus &= h.close_menu();
        const auto &c = host_sd::counters();
        handles &= c.open_handles == 0;
        windows &= c.windows_opened == c.windows_closed && c.windows_opened == 1;
        no_writes &= c.writes == 0 && c.open_writes == 0;
        dma &= host_heap::caps_counters().live_dma_bytes == 8192;
        if (i == 0) opens_first = c.opens; else opens_rest_max = std::max(opens_rest_max, c.opens);
        live.push_back(g_heap.live);
        small.push_back(g_heap.live_small);
    }
    const auto [lo, hi] = std::minmax_element(live.begin(), live.end());
    std::printf("         live heap over cycles 1..100: min %llu max %llu (spread %llu B); files opened per open: "
                "first %u, then <= %u\n",
                (unsigned long long)*lo, (unsigned long long)*hi, (unsigned long long)(*hi - *lo), opens_first,
                opens_rest_max);
    expect(menus, "S0", "every Alt+M opens the System Menu and every Back closes it");
    expect(*hi - *lo == 0, "S1", "repeated opens leave the live heap exactly flat (no growth, no drift)");
    expect(handles, "S2", "no file handle is left open after any cycle");
    expect(windows, "S3", "one SD window per open, always closed (the storage transaction is balanced)");
    expect(dma, "S4", "the 8 KiB DMA headroom is re-reserved after every window");
    expect(no_writes, "S5", "opening and closing the menu writes nothing to the card");
}

void test_cache(Harness &h) {
    std::printf("\n[K] the save list: reuse and invalidation\n");
    FrontendSaveSlot cold[2];
    std::string g1, g2;
    // Warm path: the runtime's own service has listed the card before.
    h.open_menu(); h.close_menu();
    host_sd::reset_counters();
    h.open_menu();
    const auto warm_card = host_sd::counters();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    std::printf("         warm open: opens=%u bytes_read=%llu; list '%s' / '%s'\n", warm_card.opens,
                (unsigned long long)warm_card.bytes_read, g1.c_str(), g2.c_str());
    expect(warm_card.opens == 2 && warm_card.bytes_read == 2 * sizeof(tdeck::AlphaSaveCommit), "K1",
           "an unchanged card costs two 32-byte commit reads per open (was 8 files and both sidecars)");
    cold_inspect(cold);
    {
        tdeck::AlphaSaveService svc;
        FrontendSaveSlot warm[2];
        svc.inspect(warm);
        svc.inspect(warm);
        expect(same_slots(warm, cold), "K2", "a reused list equals a cold inspection, field for field");
    }
    // A save the runtime makes: the written slot comes from the save's own
    // post-write check, the other slot is untouched.
    h.g().gold = 777;
    const bool saved = h.alt_save();
    host_sd::reset_counters();
    h.open_menu();
    const auto after_save = host_sd::counters();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    cold_inspect(cold);
    describe(cold, "cold after save");
    std::printf("         open after save: opens=%u; list '%s' / '%s'\n", after_save.opens, g1.c_str(), g2.c_str());
    {
        // A4-SAVE2: the Load page shows Slot 1 (row and footer), from the save list.
        FrontendSaveCatalog cc, warm;
        cold_catalog(cc);
        h.rt->save_service_for_test().inspect_catalog(warm);
        std::string want1, want2;
        slot1_page(cc, want1, want2);
        expect(saved && cold[0].valid && cold[1].valid && g1 == want1 && g2 == want2 && same_catalog(warm, cc), "K3a",
               "after a save the menu lists what a cold inspection lists");
    }
    expect(after_save.opens == 2, "K3b", "the save's own post-write check supplies the new slot: two commit reads");
    // Out-of-band: a commit rewritten with another sequence (CRCs unchanged).
    Card card;
    card.take();
    const int newest = cold[0].sequence > cold[1].sequence ? 0 : 1;
    {
        tdeck::AlphaSaveCommit c{};
        std::memcpy(&c, card.commit[newest].data(), sizeof(c));
        c.sequence += 100;
        host_sd::write_card_file(card_path(newest, "commit").c_str(), std::string(reinterpret_cast<char *>(&c), sizeof(c)));
        host_sd::reset_counters();
        h.open_menu();
        const auto k4 = host_sd::counters();
        h.close_menu();
        FrontendSaveSlot now[2];
        cold_inspect(now);
        h.open_menu();
        h.load_page(g1, g2);
        h.close_menu(); h.close_menu();
        std::printf("         commit rewritten: opens=%u\n", k4.opens);
        expect(k4.opens == 2 + 3 && now[newest].sequence == cold[newest].sequence + 100, "K4",
               "a changed commit record is a miss: that slot alone is read and verified again");
    }
    card.put();
    // Deleted and re-created.
    host_sd::remove_card_file(card_path(newest, "commit").c_str());
    h.open_menu();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    // A4-SAVE2: with the newest commit gone Slot 1 shows the older generation
    // (its sequence, from the list's own entry for it), never the deleted one.
    {
        FrontendSaveCatalog cc, warm;
        cold_catalog(cc);
        h.rt->save_service_for_test().inspect_catalog(warm);
        std::string want1, want2;
        slot1_page(cc, want1, want2);
        expect(g1 == want1 && g2 == want2 && same_catalog(warm, cc) && cc.slots[0].status == SaveSlotStatus::Saved &&
                   cc.slots[0].sequence == cold[1 - newest].sequence,
               "K5a", "a deleted generation is dropped from the list at once (Slot 1 shows the one before it)");
    }
    card.put();
    h.open_menu();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    const std::string back = g1; // the newest generation is the Latest row
    expect(back.find("Avatar") != std::string::npos, "K5b", "the same generation put back is listed again");
    // A refused generation is never reused: read and verified on every open.
    {
        tdeck::AlphaSaveCommit c{};
        std::memcpy(&c, card.commit[newest].data(), sizeof(c));
        c.json ^= 1u;   // the sidecar no longer matches its commit
        host_sd::write_card_file(card_path(newest, "commit").c_str(), std::string(reinterpret_cast<char *>(&c), sizeof(c)));
        uint32_t opens[2];
        for (auto &o : opens) {
            host_sd::reset_counters();
            h.open_menu();
            o = host_sd::counters().opens;
            h.load_page(g1, g2);
            h.close_menu(); h.close_menu();
        }
        // A4-SAVE2: the slot is listed as recovered (its load restores the
        // generation before); the refused one is re-read on every open.
        const std::string bad = g2;
        expect(bad == "Last save damaged; Enter loads the one before" && opens[0] == 5 && opens[1] == 5, "K6",
               "a refused generation is listed damaged and re-read on every open (never cached)");
        card.put();
        h.open_menu();
        h.load_page(g1, g2);
        h.close_menu(); h.close_menu();
        const std::string fixed = g1;
        expect(fixed.find("Avatar") != std::string::npos, "K6b", "repaired, it is listed valid on the next open");
    }
    // Save from inside the open menu, then its Load page, without closing it.
    // The newest generation's commit is removed first, so the menu opens with
    // that slot empty and the Save writes into it: a stale page would still
    // say "empty".
    {
        FrontendSaveSlot before[2];
        cold_inspect(before);
        const int newest_now = before[0].sequence > before[1].sequence ? 0 : 1;
        host_sd::remove_card_file(card_path(newest_now, "commit").c_str());
        // A4-SAVE2: a new game time, so Slot 1's footer changes with the save.
        h.g().time.hour = 15; h.g().time.minute = 42;
        h.open_menu();
        h.ball(RawInputKind::TrackballDown);
        h.set_mark();
        h.key('\r');                                   // Save Game: Slots 1-3, on Slot 1
        h.key('\r');                                   // occupied: "Overwrite Slot 1?"
        h.ball(RawInputKind::TrackballDown);            // Yes
        h.key('\r');
        // A4-UI3: a save from the menu returns to the game; the next open's
        // Load page must show it (the menu re-reads the list on every open).
        const bool saved_in_menu = h.saw("Save complete") && !h.rt->system_menu_open();
        h.open_menu();
        std::string r1, r2;
        h.load_page(r1, r2);
        h.close_menu(); h.close_menu();
        FrontendSaveSlot c[2];
        cold_inspect(c);
        FrontendSaveCatalog cc;
        cold_catalog(cc);
        std::string want1, want2;
        slot1_page(cc, want1, want2);
        describe(c, "cold after in-menu save");
        std::printf("         Load page after an in-menu save: '%s' / '%s'\n", r1.c_str(), r2.c_str());
        expect(saved_in_menu && r1 == want1 && r2 == want2 && r2.find("15:42") != std::string::npos, "K10",
               "a Save made from the menu is on the Load page of the next open (the card as it now is)");
    }
}

void test_save_invalidation(Harness &h) {
    std::printf("\n[K] a failed save forgets the slot it touched\n");
    FrontendSaveSlot cold[2];
    std::string g1, g2;
    h.open_menu(); h.close_menu();   // the list is known
    // The rename of the new sidecar fails: the target slot now holds a new gam
    // and ool under its OLD commit (same bytes as the cached one).
    host_sd::faults().fail_rename = ".json";
    h.g().gold = 1234;
    h.set_mark();
    h.raw_key('s', true);
    const bool failed = h.saw("Save failed");
    host_sd::faults().fail_rename.clear();
    cold_inspect(cold);
    describe(cold, "cold after failed save");
    h.open_menu();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    FrontendSaveCatalog cc;
    cold_catalog(cc);
    std::string want1, want2;
    slot1_page(cc, want1, want2);   // A4-SAVE2: Slot 1's row and footer
    std::printf("         menu after failed save: '%s' / '%s'\n", g1.c_str(), g2.c_str());
    expect(failed && g1 == want1 && g2 == want2 && (!cold[0].valid || !cold[1].valid), "K7",
           "after a save that failed mid-rename the menu lists the card as it is (the torn slot corrupt)");
    // Write failure: nothing renamed.
    host_sd::faults().fail_write = ".json.tmp";
    h.set_mark();
    h.raw_key('s', true);
    const bool failed2 = h.saw("Save failed");
    host_sd::faults().fail_write.clear();
    cold_inspect(cold);
    h.open_menu();
    h.load_page(g1, g2);
    h.close_menu(); h.close_menu();
    cold_catalog(cc);
    slot1_page(cc, want1, want2);
    expect(failed2 && g1 == want1 && g2 == want2, "K8", "after a failed temp write the menu lists the card as it is");
    // Recover: two good saves.
    expect(h.alt_save() && h.alt_save(), "K9", "the next saves succeed and replace the torn slot");
}

void test_failures() {
    std::printf("\n[F] failure paths of the real shell\n");
    FrontendSaveSlot out[2], good[2];
    cold_inspect(good);
    {
        host_sd::faults().no_card = true;
        tdeck::AlphaSaveService svc;
        host_sd::reset_counters();
        svc.inspect(out);
        const auto c = host_sd::counters();
        host_sd::faults().no_card = false;
        expect(!out[0].present && !out[1].present && c.open_handles == 0 && c.windows_opened == c.windows_closed,
               "F1", "no card: both slots empty, no handle left, the window closed");
        svc.inspect(out);
        expect(same_slots(out, good), "F1b", "the card back: the same service lists it (nothing negative kept)");
    }
    {
        host_sd::faults().fail_open = "g1.commit";
        tdeck::AlphaSaveService svc;
        svc.inspect(out);
        host_sd::faults().fail_open.clear();
        expect(same_slot(out[0], good[0]) && !out[1].present, "F2", "an unreadable commit lists that slot empty only");
    }
    {
        host_sd::faults().short_read = "g0.json";
        tdeck::AlphaSaveService svc;
        svc.inspect(out);
        host_sd::faults().short_read.clear();
        expect(out[0].present && !out[0].valid && same_slot(out[1], good[1]), "F3",
               "a short sidecar read lists that slot corrupt");
        svc.inspect(out);
        expect(same_slots(out, good), "F3b", "the read good again: the same service lists it valid");
    }
    {
        std::string gam;
        host_sd::read_card_file(card_path(0, "gam").c_str(), gam);
        std::string bad = gam;
        bad[100] = char(bad[100] ^ 0x5a);
        host_sd::write_card_file(card_path(0, "gam").c_str(), bad);
        FrontendSaveSlot c[2];
        cold_inspect(c);
        host_sd::write_card_file(card_path(0, "gam").c_str(), gam);
        expect(c[0].present && !c[0].valid && same_slot(c[1], good[1]), "F4", "a CRC mismatch lists that slot corrupt");
    }
    {
        host_heap::fail_next(MALLOC_CAP_SPIRAM);
        tdeck::AlphaSaveService svc;
        host_sd::reset_counters();
        svc.inspect(out);
        const auto c = host_sd::counters();
        expect(!out[0].present && !out[1].present && c.windows_opened == c.windows_closed &&
                   host_heap::caps_counters().live_dma_bytes == 8192,
               "F5", "no PSRAM for the workspace: an empty list, the window closed, the headroom restored");
        svc.inspect(out);
        expect(same_slots(out, good), "F5b", "the next open allocates the workspace and lists the card");
    }
    {
        std::string commit;
        host_sd::read_card_file(card_path(1, "commit").c_str(), commit);
        host_sd::write_card_file(card_path(1, "commit").c_str(), commit.substr(0, 31));
        FrontendSaveSlot c[2];
        cold_inspect(c);
        host_sd::write_card_file(card_path(1, "commit").c_str(), commit);
        expect(same_slot(c[0], good[0]) && !c[1].present, "F6", "a truncated commit record lists that slot empty");
    }
}

void test_correctness(Harness &h) {
    std::printf("\n[V] save and load through the real shell\n");
    h.g().gold = 555;
    const bool saved = h.alt_save();
    h.g().gold = 1;
    const bool loaded = h.alt_load();
    expect(saved && loaded && h.g().gold == 555, "V1", "Alt+S then Alt+L restores the saved gold (555)");
    // The newest generation damaged: Continue Latest falls back to the older.
    FrontendSaveSlot c[2];
    cold_inspect(c);
    const int newest = c[0].sequence > c[1].sequence ? 0 : 1;
    std::string json;
    host_sd::read_card_file(card_path(newest, "json").c_str(), json);
    host_sd::write_card_file(card_path(newest, "json").c_str(), json.substr(0, json.size() / 2));
    h.g().gold = 2;
    const bool fell_back = h.alt_load();
    host_sd::write_card_file(card_path(newest, "json").c_str(), json);
    expect(fell_back && h.g().gold != 2, "V2", "a damaged newest generation: Continue Latest restores the older one");
    // A cached-valid slot corrupted behind the service's back (commits untouched):
    // the list may still name it, but Load re-verifies and refuses, and the live
    // game is untouched. Since Alpha 4 A4-SAVE2 the Load page loads a slot
    // (its newest generation, else the one before), so both generations of
    // Slot 1 are corrupted.
    h.open_menu(); h.close_menu();
    std::string both[2];
    for (int g = 0; g < 2; ++g) {
        host_sd::read_card_file(card_path(g, "json").c_str(), both[g]);
        host_sd::write_card_file(card_path(g, "json").c_str(), both[g].substr(0, both[g].size() / 2));
    }
    h.g().gold = 42;
    h.set_mark();
    h.open_menu();
    h.ball(RawInputKind::TrackballDown); h.ball(RawInputKind::TrackballDown); h.key('\r'); // Load Game (on Slot 1)
    h.key('\r');
    const bool refused = h.saw("No valid save") || !h.saw("Load complete");
    if (h.rt->system_menu_open()) { h.close_menu(); h.close_menu(); }
    for (int g = 0; g < 2; ++g) host_sd::write_card_file(card_path(g, "json").c_str(), both[g]);
    expect(refused && h.g().gold == 42, "V3",
           "Load on a slot damaged out of band refuses and leaves the live game untouched");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a3_04g_storage_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    const auto dir = std::filesystem::temp_directory_path() / "openu5-a3-04g-card";
    host_sd::set_root(dir.string());
    host_sd::format_card();
    host_sd::set_probe(io_probe);
    tdeck::AlphaSaveService::reserve_dma_headroom();

    Harness h;
    test_census(h);
    test_retention(h);
    test_cycles(h);
    test_cache(h);
    test_save_invalidation(h);
    test_failures();
    test_correctness(h);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::printf("\na3_04g_storage_runtime: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
