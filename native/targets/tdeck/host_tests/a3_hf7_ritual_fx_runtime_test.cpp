// Alpha 3 A3-HF7 (H-184 / D-41) -- the shrine rite's viewport negative and the
// Codex ceremony's XOR pulses reach the screen.
//
//   a3_hf7_ritual_fx_runtime <openu5-alpha1-resources.bin> [--only <name>]
//
// The player-visible defect: "ALAKAZAM!", "WELL DONE!" and the Codex ceremony
// showed no inversion at all, and everything after them arrived in the key's
// own frame. CAST2.OVL (derivation: openu5/ritual_fx.h, ALPHA3_AUDIO.md 33):
//
//   donation   0x0bcd rect XOR [0x13b0]=15; two sweep loops (184,000 samples);
//              0x0d1a run_n_frames(10) -- its first frame redraws = restore
//   WELL DONE  0x0c41 rect XOR 15; two sweep loops (138,000 samples);
//              0x0c88 shake; karma/STR/DEX/INT + prints; 0x0d1a run_n_frames(10)
//   Codex      after the page's getkey 0x0d9f: XOR 4, shake, XOR 15, shake,
//              XOR 4, shake (the XORs accumulate: 4, 11, 15); print "A STRANGE
//              WIND..."; 0x0df8 getkey, whose first idle pass redraws
//
// The rect is (8,8)-(0xb7,0xb7): the 176x176 map viewport and nothing else.
// Nothing in any of it reads a key.
//
// Everything below goes through the REAL AlphaRuntime -- raw keys in, the
// shipped pack's shrines, overworld and MISCMSG records -- drawing on the REAL
// tdeck_board.cpp over the fake ST7789, on the host esp_timer virtual clock.
// Every visual check reads the fake panel's GRAM: the fixture's patterned
// tiles use palette entry i = i*0x1111, so a pixel's EGA index is readable
// and the XOR mask of a frame against a normal frame is the MODE of
// index(frame) ^ index(normal) over the map (robust to animated cells; the
// shake's 2 px shift is searched).
//
//   N1  the inversion exists (WELL DONE, donation, Codex)
//   N2  the region: the whole map viewport, never the panels or the strips
//   N3  restoration: normal afterwards, and it stays normal
//   N4  cadence: the transitions, their order and their times (+-1 frame tick)
//   N5  the HF6 getkeys: the pulses follow the page's getkey, restore at the
//       next; keys inside an effect are swallowed; one key = one wait
//   N6  the quakes: WELL DONE's after the sweeps, inside the negative; each
//       Codex XOR lands with its shake
//   N7  the rules are committed at the Enter; only the text waits
//   N8  the System Menu inside an effect
//   N9  a successful load / Return to Title drops the effect; a failed load
//       does not
//   N10 unchanged: the unpaced harness drains at once and ends normal; the
//       ordained rite has no effect
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/hud.h"
#include "openu5/presentation.h"
#include "openu5/scene_timing.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

namespace tdeck {
void host_memory_save_forget_for_test();
} // namespace tdeck

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label.c_str());
    return good;
}
std::string n(long long v) { return std::to_string(v); }
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kFrameMs = 5;
constexpr int kCodexTile = 17, kShrineTile = 25;
constexpr int kW = 320, kH = 240;
// The binary's windows, in device milliseconds (openu5/ritual_fx.h,
// openu5/scene_timing.h). Stated from the bytes here, not from the model.
constexpr int64_t kWellDoneSweepMs = int64_t(2) * 460 * 0x96 * 1000 / 25806; // 5347
constexpr int64_t kDonationSweepMs = int64_t(2) * 460 * 0xc8 * 1000 / 25806; // 7130
constexpr int64_t kShakeMs = kQuakeDurationMs;                               // 936
constexpr int64_t kTick = kSceneTickMs;                                      // 55
constexpr int64_t kSlack = 2 * kFrameMs + 1;

std::string ascii(TalkText t) {
    std::string s;
    for (auto c : t) s += char(c);
    return s;
}

using Frame = std::vector<uint16_t>;
Frame grab() { return Frame(bus::gram(), bus::gram() + kW * kH); }
int index_of(uint16_t p) { return p % 0x1111 == 0 && p / 0x1111 < 16 ? int(p / 0x1111) : -1; }

struct Xor {
    int mask = -1;
    double frac = 0;
};
// The mode of index(a) ^ index(b) over [x0,x1) x [y0,y1), b shifted by `dy`.
Xor xor_mode(const Frame &a, const Frame &b, int x0, int y0, int x1, int y1, int dy = 0) {
    int hist[16]{}, total = 0;
    for (int y = y0; y < y1; ++y) {
        const int by = y - dy;
        if (by < y0 || by >= y1) continue;
        for (int x = x0; x < x1; ++x) {
            const int ia = index_of(a[size_t(y * kW + x)]), ib = index_of(b[size_t(by * kW + x)]);
            if (ia < 0 || ib < 0) continue;
            ++hist[ia ^ ib];
            ++total;
        }
    }
    Xor out;
    for (int m = 0; m < 16; ++m)
        if (total && hist[m] > out.frac * total) out = {m, double(hist[m]) / total};
    return out;
}
// The map: the viewport between the device's sky and wind strips.
constexpr int kMapX0 = kHudViewportX, kMapX1 = kHudViewportX + kHudViewportW;
constexpr int kMapY0 = kHudViewportY + kHudSkyBarH, kMapY1 = kHudWindBarY;
Xor map_mode(const Frame &a, const Frame &ref) {
    Xor best;
    for (int dy : {0, kQuakeAmplitudePx, -kQuakeAmplitudePx}) {
        const auto m = xor_mode(a, ref, kMapX0, kMapY0, kMapX1, kMapY1, dy);
        if (m.frac > best.frac) best = m;
    }
    return best;
}

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    size_t mark = 0;
    explicit Run(bool paced = true) {
        openu5_host_virtual_clock_us() = 5'000'000;
        bus::install();
        bus::model() = bus::Model{};
        // The clock moves by the script alone: a transition's time is the
        // pacer's, never the modelled SPI cost of the frame that draws it.
        bus::model().timed = false;
        board.initialize_display();
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.patterned_test_tiles = true;
        f.paced_scenes = paced;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.gold = 500;
        g.karma = 50;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 300;
        m.strength = m.dexterity = m.intelligence = 20;
        g.quest.shrine_quest = g.quest.shrine_visited = 0;
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const DialoguePacer &pacer() { return rt->dialogue_pacer(); }
    bool waiting() { return pacer().awaiting_key(); }
    std::string overlay() { return rt->status_overlay(); }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            rt->render(board);
        }
    }
    void run_ms(int64_t ms) { frames(int(ms / kFrameMs)); }
    void raw(tdeck::RawInputEvent e, bool frame = true) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        if (frame) frames(1);
    }
    tdeck::RawInputEvent key_event(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        return e;
    }
    void key(uint8_t code, bool alt = false) { raw(key_event(code, alt)); }
    void type(const std::string &s) { for (char c : s) key(uint8_t(c)); }
    void mic() {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void ball(RawInputKind k) {
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void alt_save() { key('s', true); }
    void alt_load() { key('l', true); }
    void menu_toggle() { key('m', true); }
    void return_to_title() { key('m', true); ball(RawInputKind::TrackballUp); key('\r'); }

    void set_mark() { mark = ui().transcript_size(); }
    std::string since_mark() {
        std::string out;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) out += b->text;
        return out;
    }
    bool shown(const std::string &needle) { return since_mark().find(needle) != std::string::npos; }
    size_t count(const std::string &needle) {
        const auto all = since_mark();
        size_t k = 0;
        for (size_t at = all.find(needle); at != std::string::npos; at = all.find(needle, at + 1)) ++k;
        return k;
    }

    const ShrineData &shrines() { return pack->shrine_data; }
    int tile_at(int x, int y) { return ctx().terrain->effective(ctx().world, MapId{0, 0}, x, y); }
    void stand(int x, int y) {
        g().position.map = {0, 0};
        g().position.xy = {uint8_t(x), uint8_t(y)};
        frames(2);
    }
    // (E)nter shrine `v`, Visit, virtue, mantra -- everything but the last
    // Enter, which the caller presses (after grabbing its normal frame).
    bool rite_until_enter(int v) {
        if (v < 0 || v >= shrines().count) return false;
        stand(shrines().x[v], shrines().y[v]);
        if (tile_at(shrines().x[v], shrines().y[v]) != kShrineTile) return false;
        key('e');
        if (mode() != UiMode::YesNo) return false;
        key('y');
        if (mode() != UiMode::TextEntry) return false;
        type(ascii(shrines().virtues[v]));
        key('\r');
        if (mode() != UiMode::TextEntry) return false;
        type(ascii(shrines().mantras[v]));
        frames(4);
        set_mark();
        return true;
    }
    bool codex_xy(int &x, int &y) {
        for (y = 0; y < 256; ++y)
            for (x = 0; x < 256; ++x)
                if (tile_at(x, y) == kCodexTile) return true;
        return false;
    }
    bool codex() {
        int x = 0, y = 0;
        if (!codex_xy(x, y)) return false;
        stand(x, y);
        set_mark();
        key('e');
        return shown("Codex of Ultimate Wisdom");
    }
};

// The normal picture minus every pixel that animates on its own: grabbed over
// twelve frames 60 ms apart, a pixel that differs in any of them is dropped
// (0xffff has no palette index, so xor_mode skips it).
Frame stable(Run &r) {
    auto base = grab();
    for (int i = 1; i < 12; ++i) {
        r.run_ms(60);
        const auto f = grab();
        for (size_t p = 0; p < base.size(); ++p)
            if (base[p] != f[p]) base[p] = 0xffff;
    }
    return base;
}

// One frame of a timeline: the screen's mask against the normal frame.
struct Sample {
    int64_t t = 0; // ms since the key
    Xor map;
    bool quake = false, effect = false, key_wait = false;
    std::string text;
};
struct Timeline {
    std::vector<Sample> s;
    // The distinct masks the screen showed, with the first time of each.
    std::vector<std::pair<int, int64_t>> changes() const {
        std::vector<std::pair<int, int64_t>> out;
        for (const auto &x : s) {
            if (x.map.frac < 0.8) continue;
            if (out.empty() || out.back().first != x.map.mask) out.push_back({x.map.mask, x.t});
        }
        return out;
    }
    int64_t first(bool (*pred)(const Sample &)) const {
        for (const auto &x : s)
            if (pred(x)) return x.t;
        return -1;
    }
    int64_t first_text(const std::string &needle) const {
        for (const auto &x : s)
            if (x.text.find(needle) != std::string::npos) return x.t;
        return -1;
    }
    std::string describe() const {
        std::string out;
        for (const auto &c : changes()) out += " " + n(c.first) + "@" + n(long(c.second));
        return out.empty() ? " (none)" : out;
    }
};
// Press `code` at t0 and sample every frame for `ms`.
Timeline press_and_record(Run &r, const Frame &normal, uint8_t code, int64_t ms) {
    Timeline tl;
    const int64_t t0 = Run::now_ms();
    r.raw(r.key_event(code), false);
    while (Run::now_ms() - t0 < ms) {
        r.frames(1);
        Sample x;
        x.t = Run::now_ms() - t0;
        const auto f = grab();
        x.map = map_mode(f, normal);
        x.quake = r.rt->transient_probe_for_test().quake;
        x.effect = r.pacer().in_effect();
        x.key_wait = r.waiting();
        x.text = r.since_mark();
        tl.s.push_back(std::move(x));
    }
    return tl;
}
bool near(int64_t t, int64_t want) { return t >= want - kSlack && t <= want + kSlack; }

// ===========================================================================
void test_well_done() {
    std::printf("\nN1/N2/N3/N4/N6/N7 WELL DONE (CAST2 0x0c18-0x0d1a)\n");
    Run r;
    r.g().quest.shrine_visited = 1;
    r.g().quest.shrine_quest = 1;
    if (!check(r.rite_until_enter(0), "N1.0", "the rite of " + ascii(r.shrines().virtues[0]) + " with its Quest done"))
        return;
    const auto normal = stable(r);
    const auto karma = r.g().karma;
    const auto tl = press_and_record(r, normal, '\r', 9000);
    const auto ch = tl.changes();
    std::printf("    screen masks:%s\n", tl.describe().c_str());
    // N7 -- the rules at the Enter.
    const auto &first = tl.s.front();
    check(r.g().party.characters[0].intelligence == 21 && r.g().karma == karma + 3 && (r.g().quest.shrine_quest & 1) == 0,
          "N7.1", "karma +3, INT +1 and the Quest bit are committed at the Enter (the core is unchanged)");
    check(first.text.find("WELL DONE!") != std::string::npos && first.text.find("Intelligence +1") == std::string::npos,
          "N7.2", "the first frame shows \"WELL DONE!\" but not yet \"Intelligence +1\" (printed at 0x0cfc, after the sweeps)");
    // N1 -- the negative, on the first frame.
    check(first.map.mask == 15 && first.map.frac > 0.75, "N1.1",
          "the first frame after the Enter shows the viewport XORed with 15 (0x0c41; mode " + n(first.map.mask) + " at " +
              n(long(first.map.frac * 100)) + "%)");
    // N4 -- exactly two transitions: 0 -> 15 at the Enter, 15 -> 0 at the restore.
    check(ch.size() == 2 && ch[0].first == 15 && ch[1].first == 0, "N4.1",
          "the screen goes negative once and returns once (" + tl.describe() + ")");
    const int64_t restore = ch.size() >= 2 ? ch[1].second : -1;
    check(near(restore, kWellDoneSweepMs + kShakeMs), "N4.2",
          "it is restored after the two sweep loops and the shake: " + n(long(restore)) + " ms (want " +
              n(long(kWellDoneSweepMs + kShakeMs)) + ")");
    const int64_t quake = tl.first([](const Sample &x) { return x.quake; });
    check(near(quake, kWellDoneSweepMs), "N6.1",
          "the shake (0x0c88) starts when the sweeps end: " + n(long(quake)) + " ms (want " + n(long(kWellDoneSweepMs)) + ")");
    bool inverted_during_quake = true;
    for (const auto &x : tl.s)
        if (x.quake && x.t < restore - kSlack && x.map.frac > 0.8) inverted_during_quake &= x.map.mask == 15;
    check(quake > 0 && inverted_during_quake, "N6.2", "the whole shake plays inside the negative");
    const int64_t label = tl.first_text("Intelligence +1");
    check(near(label, kWellDoneSweepMs + kShakeMs) && label <= restore, "N4.3",
          "\"Intelligence +1\" prints after the shake, as the restore frame arrives: " + n(long(label)) + " ms");
    const int64_t free_at = tl.first([](const Sample &x) { return x.t > 100 && !x.effect; });
    check(near(free_at, kWellDoneSweepMs + kShakeMs + 10 * kTick), "N4.4",
          "the turn is held through run_n_frames(10) after the restore: free at " + n(long(free_at)) + " ms");
    // N3 -- restored and staying so.
    r.run_ms(5000);
    const auto after = map_mode(grab(), normal);
    std::printf("    after: mode %d at %d%%, active %d\n", after.mask, int(after.frac * 100),
                int(r.rt->transient_probe_for_test().ritual_fx));
    check(after.mask == 0 && after.frac > 0.75 && !r.rt->transient_probe_for_test().ritual_fx, "N3.1",
          "5 s later the viewport is the normal picture (mode " + n(after.mask) + ") and no effect is pending");
    check(r.count("Intelligence +1") == 1 && r.count("WELL DONE!") == 1, "N3.2", "each line shown exactly once");
    const auto routed = r.rt->routed_command_count();
    r.ball(RawInputKind::TrackballDown);
    check(r.rt->routed_command_count() == routed + 1, "N3.3", "the next input is an ordinary command again");
}

void test_region() {
    std::printf("\nN2 the region: the map viewport, not the panels, not the strips\n");
    Run r;
    r.g().quest.shrine_visited = 1;
    r.g().quest.shrine_quest = 1;
    if (!r.rite_until_enter(0)) return;
    const auto normal = stable(r);
    r.key('\r');
    r.run_ms(1000); // mid-sweep
    const auto inv = grab();
    // The WELL DONE command is a game turn: the world near the shrine can move
    // at the Enter. The reference for the region is the SAME world, restored.
    r.run_ms(8000);
    const auto restored = stable(r);
    (void)normal;
    int quads = 0;
    const int mx = (kMapX0 + kMapX1) / 2, my = (kMapY0 + kMapY1) / 2;
    for (int qy = 0; qy < 2; ++qy)
        for (int qx = 0; qx < 2; ++qx) {
            const auto m = xor_mode(inv, restored, qx ? mx : kMapX0, qy ? my : kMapY0, qx ? kMapX1 : mx, qy ? kMapY1 : my);
            quads += m.mask == 15 && m.frac > 0.5; // chance agreement is ~6 %
            std::printf("    quarter %d,%d: mode %d at %d%%\n", qx, qy, m.mask, int(m.frac * 100));
        }
    check(quads == 4, "N2.1", "all four quarters of the map viewport are XORed with 15 (" + n(quads) + " of 4)");
    const auto party = xor_mode(inv, restored, kHudRightX, kHudRightY, kW, kHudTranscriptSeparatorY);
    check(party.mask == 0 && party.frac > 0.9, "N2.2",
          "the party and status panels are not inverted (mode " + n(party.mask) + ")");
    const auto sky = xor_mode(inv, restored, kMapX0, kHudViewportY, kMapX1, kMapY0);
    const auto wind = xor_mode(inv, restored, kMapX0, kHudWindBarY, kMapX1, kHudViewportY + kHudViewportH);
    check(sky.mask == 0 && wind.mask == 0, "N2.3",
          "the device's sky and wind strips are not inverted (modes " + n(sky.mask) + ", " + n(wind.mask) + ")");
    // Alpha 4 UI Batch 1: the area below the viewport is the frame band
    // (#0000AA, not a palette index -- xor_mode() would count nothing there).
    // Not inverted = byte for byte the same mid-sweep as restored.
    size_t touch_differ = 0;
    for (int y = kHudTouchY; y < kH; ++y)
        for (int x = kHudTouchX; x < kHudTouchX + kHudTouchW; ++x)
            touch_differ += inv[size_t(y * kW + x)] != restored[size_t(y * kW + x)];
    check(touch_differ == 0, "N2.4", "the area below the viewport is not inverted: identical mid-sweep and restored (" +
                                         n(touch_differ) + " pixels differ)");
}

void test_donation() {
    std::printf("\nN1/N4 the donation's ALAKAZAM (CAST2 0x0b7c-0x0c14)\n");
    Run r;
    r.g().quest.shrine_visited = 1;
    if (!check(r.rite_until_enter(0), "N1.2", "an already visited shrine")) return;
    r.key('\r');
    if (!check(r.mode() == UiMode::NumericEntry, "N1.3", "\"How many cycles?\"")) return;
    r.type("1");
    r.frames(2);
    r.set_mark();
    const auto normal = stable(r);
    const auto gold = r.g().gold;
    const auto tl = press_and_record(r, normal, '\r', 9000);
    const auto ch = tl.changes();
    std::printf("    screen masks:%s\n", tl.describe().c_str());
    check(r.g().gold == gold - 100 && tl.s.front().text.find("ALAKAZAM") != std::string::npos, "N7.3",
          "the gold is taken at the Enter and ALAKAZAM is on the first frame");
    check(ch.size() == 2 && ch[0].first == 15 && ch[0].second <= kFrameMs, "N1.4",
          "the donation inverts the viewport at once (0x0bcd; " + tl.describe() + ")");
    const int64_t restore = ch.size() >= 2 ? ch[1].second : -1;
    check(near(restore, kDonationSweepMs), "N4.5",
          "and restores it after its two sweep loops (no shake): " + n(long(restore)) + " ms (want " +
              n(long(kDonationSweepMs)) + ")");
    check(tl.first([](const Sample &x) { return x.quake; }) < 0, "N6.3", "the donation has no shake");
    r.run_ms(3000);
    check(map_mode(grab(), normal).mask == 0 && !r.rt->transient_probe_for_test().ritual_fx, "N3.4",
          "the viewport is normal afterwards");
}

// The Codex ceremony, from the page's getkey (0x0d9f) on.
struct Ceremony {
    Run r;
    Frame normal;
    bool ok = false;
    Ceremony() {
        r.g().quest.shrine_quest = 1;
        r.g().quest.shrine_visited = 0xff;
        if (!r.codex()) return;
        for (int k = 0; k < 3; ++k) r.key(' '); // 0x0d2b 0x0d35 0x0d3f
        r.frames(4);
        ok = r.waiting() && r.shown("\"\n\n");
        normal = stable(r);
    }
};

void test_codex() {
    std::printf("\nN1/N4/N5/N6 the Codex ceremony's three pulses (0x0dac-0x0df8)\n");
    Ceremony c;
    auto &r = c.r;
    if (!check(c.ok, "N5.0", "the Codex, three keys: the page is up and 0x0d9f waits")) return;
    r.run_ms(10000);
    check(map_mode(grab(), c.normal).mask == 0 && r.waiting(), "N5.1",
          "while 0x0d9f waits (10 s) the viewport is normal: the pulses follow that getkey");
    const auto tl = press_and_record(r, c.normal, ' ', 4000);
    const auto ch = tl.changes();
    std::printf("    screen masks:%s\n", tl.describe().c_str());
    const bool sequence = ch.size() == 4 && ch[0].first == 4 && ch[1].first == 11 && ch[2].first == 15 && ch[3].first == 0;
    check(sequence, "N1.5", "the screen shows XOR 4, then 11, then 15, then normal (" + tl.describe() + ")");
    check(sequence && ch[0].second <= kFrameMs && near(ch[1].second, kShakeMs) && near(ch[2].second, 2 * kShakeMs),
          "N4.6", "a pulse per shake: at 0, " + n(long(kShakeMs)) + ", " + n(long(2 * kShakeMs)) + " ms");
    const int64_t wind = tl.first_text("STRANGE WIND");
    check(near(wind, 3 * kShakeMs), "N4.7",
          "\"A STRANGE WIND...\" prints after the third shake: " + n(long(wind)) + " ms (want " + n(long(3 * kShakeMs)) + ")");
    check(sequence && near(ch[3].second, 3 * kShakeMs + kTick) && wind < ch[3].second, "N4.8",
          "it stands over the negative for the getkey's first idle pass (delay(1)), then the redraw restores: " +
              n(long(sequence ? ch[3].second : -1)) + " ms");
    bool quake_throughout = true;
    for (const auto &x : tl.s)
        if (x.t < 3 * kShakeMs - kSlack) quake_throughout &= x.quake;
    check(quake_throughout, "N6.4", "the three shakes run back to back from the key, each under its own XOR");
    const auto wait_at = tl.first([](const Sample &x) { return x.t > 100 && x.key_wait; });
    check(sequence && wait_at >= ch[3].second - kFrameMs && wait_at <= ch[3].second + kFrameMs, "N5.2",
          "the restore is the next getkey (0x0df8) starting: waiting from " + n(long(wait_at)) + " ms");
    bool cue_hidden = true;
    for (const auto &x : tl.s)
        if (x.effect) cue_hidden &= !x.key_wait;
    check(cue_hidden && r.overlay() == "Enter: continue", "N5.3",
          "no key-wait inside the effect; the getkey after it wears its cue (\"" + r.overlay() + "\")");
    check(!r.shown("Thou dost read:"), "N5.4", "0x0df8 holds: \"Thou dost read:\" is not shown");
    r.key(' ');
    check(r.shown("Thou dost read:") && r.waiting() && map_mode(grab(), c.normal).mask == 0, "N5.5",
          "one key releases one section; the viewport stays normal");
}

void test_codex_keys() {
    std::printf("\nN5 keys inside the Codex's effect are swallowed\n");
    Ceremony c;
    auto &r = c.r;
    if (!c.ok) return;
    const auto routed = r.rt->routed_command_count();
    const auto xy = r.g().position.xy;
    r.key(' '); // 0x0d9f ends: the pulses start
    r.run_ms(300);
    r.key('y');
    r.key('e');
    r.run_ms(700);
    r.mic();
    r.ball(RawInputKind::TrackballLeft);
    r.raw(r.key_event(' '), false); // a burst: two keys in one frame
    r.raw(r.key_event(' '), false);
    r.frames(1);
    r.run_ms(2500);
    check(r.shown("STRANGE WIND") && !r.shown("Thou dost read:") && r.waiting(), "N5.6",
          "seven inputs inside the pulses release nothing: 0x0df8 still waits");
    check(r.rt->routed_command_count() == routed && r.g().position.xy.x == xy.x && r.g().position.xy.y == xy.y &&
              r.mode() == UiMode::Exploration && r.ui().input_length() == 0,
          "N5.7", "and they are consumed: no command, no movement, no second Codex, nothing typed");
    r.key(' ');
    check(r.shown("Thou dost read:") && r.waiting() && r.count("Thou dost read:") == 1, "N5.8",
          "the next key ends exactly that one getkey");
}

void test_well_done_keys() {
    std::printf("\nN5 keys inside WELL DONE are swallowed\n");
    Run r;
    r.g().quest.shrine_visited = 1;
    r.g().quest.shrine_quest = 1;
    if (!r.rite_until_enter(0)) return;
    r.key('\r');
    const auto routed = r.rt->routed_command_count();
    const auto xy = r.g().position.xy;
    r.run_ms(500);
    r.key('e');
    r.ball(RawInputKind::TrackballDown);
    r.key('y');
    check(r.rt->routed_command_count() == routed && r.g().position.xy.y == xy.y && r.mode() != UiMode::YesNo &&
              !r.shown("Intelligence +1") && r.pacer().in_effect(),
          "N5.9", "a command key, a step and a 'y' inside the sweeps start nothing and cut nothing");
    r.run_ms(8000);
    const auto after = r.rt->routed_command_count();
    r.ball(RawInputKind::TrackballDown);
    check(r.count("Intelligence +1") == 1 && r.rt->routed_command_count() == after + 1, "N5.10",
          "after the effect the next input is play again");
}

void test_menu() {
    std::printf("\nN8 the System Menu inside WELL DONE\n");
    Run r;
    r.g().quest.shrine_visited = 1;
    r.g().quest.shrine_quest = 1;
    if (!r.rite_until_enter(0)) return;
    const auto normal = stable(r);
    r.key('\r');
    r.run_ms(1000);
    r.menu_toggle();
    const bool open = r.rt->system_menu_open();
    r.run_ms(10000);
    const bool behind = r.shown("Intelligence +1") || r.rt->transient_probe_for_test().quake;
    r.menu_toggle();
    check(open && !behind && !r.rt->system_menu_open(), "N8.1",
          "nothing is released behind the menu (no shake, no reward line)");
    r.frames(1);
    const auto reopened = map_mode(grab(), normal);
    r.run_ms(3000);
    const auto settled = map_mode(grab(), normal);
    check(reopened.mask == 15 && settled.mask == 0 && r.count("Intelligence +1") == 1 &&
              !r.rt->transient_probe_for_test().ritual_fx && !r.pacer().holding(),
          "N8.2", "the closed menu shows the negative again, the rite resumes (shake, reward) and ends normal (" +
                      n(reopened.mask) + " -> " + n(settled.mask) + ")");
}

void test_load(bool succeed) {
    const char *id = succeed ? "N9" : "N9F";
    std::printf("\n%s a %s load inside WELL DONE\n", id, succeed ? "successful" : "failed");
    Run r;
    r.g().quest.shrine_visited = 1;
    r.g().quest.shrine_quest = 1;
    r.stand(r.shrines().x[0], r.shrines().y[0]);
    if (succeed) {
        r.alt_save();
        r.frames(2);
    }
    if (!r.rite_until_enter(0)) return;
    const auto normal = stable(r);
    r.key('\r');
    r.run_ms(1000);
    r.alt_load();
    if (succeed) {
        r.run_ms(8000);
        const auto m = map_mode(grab(), normal);
        check(m.mask == 0 && !r.rt->transient_probe_for_test().ritual_fx && !r.pacer().holding() &&
                  !r.shown("Intelligence +1"),
              "N9.1", "the loaded game is drawn normal; the rest of the rite never plays (mode " + n(m.mask) + ")");
    } else {
        const bool still = r.pacer().in_effect() && map_mode(grab(), normal).mask == 15;
        r.run_ms(8000);
        check(r.shown("No valid save") && still && r.count("Intelligence +1") == 1 &&
                  map_mode(grab(), normal).mask == 0,
              "N9F.1", "a failed load leaves the rite running; it completes and restores");
    }
}

void test_title() {
    std::printf("\nN9 Return to Title inside the Codex's pulses, then Continue\n");
    Run r;
    r.g().quest.shrine_quest = 1;
    r.g().quest.shrine_visited = 0xff;
    int x = 0, y = 0;
    if (!r.codex_xy(x, y)) return;
    r.stand(x, y);
    r.alt_save();
    r.frames(2);
    if (!r.codex()) return;
    for (int k = 0; k < 3; ++k) r.key(' ');
    r.frames(4);
    const auto normal = stable(r);
    r.key(' ');
    r.run_ms(1200); // the second pulse
    r.return_to_title();
    r.key('x');
    r.key('j');
    r.key('\r'); // the title's Continue
    r.run_ms(5000);
    const auto m = map_mode(grab(), normal);
    check(m.mask == 0 && !r.rt->transient_probe_for_test().ritual_fx && !r.pacer().holding() &&
              !r.shown("STRANGE WIND"),
          "N9.2", "the continued game is drawn normal and the ceremony is gone (mode " + n(m.mask) + ")");
}

void test_unchanged() {
    std::printf("\nN10 what must not change\n");
    {
        Run u(false);
        u.g().quest.shrine_visited = 1;
        u.g().quest.shrine_quest = 1;
        if (!u.rite_until_enter(0)) return;
        const auto normal = stable(u);
        u.key('\r');
        check(u.shown("Intelligence +1") && !u.rt->transient_probe_for_test().ritual_fx &&
                  map_mode(grab(), normal).mask == 0,
              "N10.1", "the unpaced harness (the reference's automation rule) drains WELL DONE at once and ends normal");
    }
    {
        Run u(false);
        u.g().quest.shrine_quest = 1;
        u.g().quest.shrine_visited = 0xff;
        u.codex();
        check(u.shown("Thou dost read:") && !u.rt->transient_probe_for_test().ritual_fx, "N10.2",
              "the unpaced Codex ceremony drains at once and leaves no pulse behind");
    }
    {
        Run r;
        if (!r.rite_until_enter(1)) return;
        const auto normal = stable(r);
        r.key('\r');
        r.run_ms(2000);
        check(r.waiting() && map_mode(grab(), normal).mask == 0 && !r.pacer().in_effect(), "N10.3",
              "the ordained rite (0x0a9b) has no effect: its getkey waits over the normal viewport");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    const char *only = argc > 3 && !std::strcmp(argv[2], "--only") ? argv[3] : nullptr;
    const struct {
        const char *name;
        void (*run)();
    } tests[] = {{"well-done", test_well_done},
                 {"region", test_region},
                 {"donation", test_donation},
                 {"codex", test_codex},
                 {"codex-keys", test_codex_keys},
                 {"well-done-keys", test_well_done_keys},
                 {"menu", test_menu},
                 {"load", [] { test_load(true); }},
                 {"failed-load", [] { test_load(false); }},
                 {"title", test_title},
                 {"unchanged", test_unchanged}};
    for (const auto &t : tests)
        if (!only || !std::strcmp(only, t.name)) t.run();
    std::printf("\nA3-HF7 ritual effects: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
