// Alpha 3 A3-HF8 (H-186 / D-43) -- the Blackthorn sacrifice burst reaches the
// screen, on the victim's cell, BEFORE the victim is taken away.
//
//   a3_hf8_sacrifice_burst_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--only <name>]
//
// The player-visible defect: after the siren the victim simply went dark; no
// explosion was ever drawn. BLCKTHRN.OVL sacrifice_member 0x03ae:
//
//   03c6 print rec4 (pendulum) / rec5 (betrayal)     03cd pause(10)
//   03d0-0411 the siren: 2 x 460 tone_sweeps          (7,130 ms, Batch 51)
//   0414-041e explosion_fx_at_cell(slot 1's LAST x/y) -> ULTIMA.EXE 0x3522:
//             354b blit_tile(cell, tile 0)  -- opaque, TileData `Explosion`
//             355a noise_burst(0x7d0, 0xbb8, 0xa)  -- blocks 9,000 half samples, 174 ms
//             355d viewport_redraw  -- the tile is gone
//   0421-0429 slot 1 off, table cell = 0x80           (the victim goes dark)
//   04d8 roster; mode 1: "<name> is sliced in half! ", getkey 0x04f6, rec6
//
// The capture set [0x5893] = 0xff at 0x06fc, so 0x3522 skips its world-to-window
// shift: the burst lands on the scene cell itself -- the table (5,7) once the
// victim was dragged there (every pendulum, and a betrayal after a warning),
// the victim's seat when the mantra is given at once. Nothing reads a key.
//
// Everything below goes through the REAL AlphaRuntime -- a pass beside a
// palace guard, the answers typed at the real prompt, the shipped pack's
// throne room, shrines and MISCMSG records -- drawing on the REAL tdeck_board.cpp
// over the fake ST7789, on the host esp_timer virtual clock (bus timing off:
// a transition's time is the pacer's, never the modelled SPI cost). The visual
// checks read the fake panel's GRAM; the fixture's patterned tiles have a
// closed form, so a cell's expected pixels are computed, not captured.
//
//   N1  the burst exists (pendulum and betrayal)
//   N2  the region: exactly the binary's cell; nothing else in the viewport or panels
//   N3  the primitive: an opaque blit of tile 0, pixel for pixel; no XOR anywhere
//   N4  cadence: one phase, starting after pause(10) + the siren, lasting 174 ms
//   N5  order: the rec line, the siren, the burst with the victim still drawn,
//       its noise burst, THEN the victim gone and "sliced in half!"
//   N6  keys, trackball and Mic inside the burst do nothing and advance nothing
//   N7  restoration: no tile 0 anywhere afterwards
//   N8  a successful load / Return to Title inside the burst drop it
//   N9  the System Menu inside the burst: nothing moves behind it
//   N10 unchanged: the capture's pacing, the siren's hold, the key waits
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/audio.h"
#include "openu5/blackthorn_scene.h"
#include "openu5/debug_map_picker.h"
#include "openu5/hud.h"
#include "openu5/scene_timing.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
AudioPackInfo g_audio{};
constexpr int64_t kFrameMs = 5;
constexpr int kW = 320;
// The binary's windows, in device milliseconds, stated from the bytes here and
// not from the model: pause(10) at 0x03cd, the siren's 920 sweeps of 0xc8, and
// the burst's noise_burst(0x7d0, 0xbb8, 0xa) = ceil(3000/10) x 10 x 1.5 samples.
constexpr int64_t kPauseMs = 10 * 55;
constexpr int64_t kSirenMs = int64_t(2) * 460 * 0xc8 * 1000 / 25806; // 7130
constexpr int64_t kBurstMs = int64_t(300) * 10 * 3 / 2 * 1000 / 25806; // 174
constexpr int64_t kSlack = 2 * kFrameMs + 1;
constexpr int kTable = 0x80, kBody = 0x82;

// Byte b of patterned tile t is t*37 + b*11 + (b>>3)*7, palette entry i is
// i*0x1111, and expand_tile puts the high nibble on the left (A3-HF3).
void tile_pixels(int tile, uint16_t (&out)[256]) {
    for (int b = 0; b < 128; ++b) {
        const uint8_t v = uint8_t(tile * 37 + b * 11 + (b >> 3) * 7);
        const int y = b / 8, x = (b % 8) * 2;
        out[y * 16 + x] = uint16_t((v >> 4) * 0x1111);
        out[y * 16 + x + 1] = uint16_t((v & 15) * 0x1111);
    }
}

/** The recording backend: every SFX the service submits, with the virtual time. */
struct Recorder final : AudioBackend {
    struct Sub {
        int64_t us;
        SfxId id;
    };
    std::vector<Sub> subs;
    bool play_sfx(const SfxRequest &r) override {
        subs.push_back({openu5_host_virtual_clock_us(), r.id});
        return true;
    }
    void stop_sfx() override {}
    bool start_music(MusicSong, uint16_t) override { return true; }
    void stop_music() override {}
    void set_gain(AudioChannel, uint16_t) override {}
    int64_t first(SfxId id, int64_t after_us = 0) const {
        for (const auto &s : subs)
            if (s.id == id && s.us >= after_us) return s.us;
        return -1;
    }
    size_t count(SfxId id, int64_t after_us = 0) const {
        return size_t(std::count_if(subs.begin(), subs.end(),
                                    [&](const Sub &s) { return s.id == id && s.us >= after_us; }));
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Recorder audio{};
    size_t mark = 0;
    Run() {
        openu5_host_virtual_clock_us() = 5'000'000;
        bus::install();
        bus::model() = bus::Model{};
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
        f.paced_scenes = true;
        rt->attach_host_test_fixture(f);
        rt->configure_audio(g_audio, &audio);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.gold = 500;
        g.karma = 50;
        const char *names[] = {"Avatar", "Shamino", "Iolo"};
        const char classes[] = {'A', 'F', 'B'};
        g.party.character_count = g.party.party_size = 3;
        g.party.active_character = 255;
        for (int i = 0; i < 3; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", names[i]);
            m.status = 'G';
            m.character_class = classes[i];
            m.level = 1;
            m.current_hp = m.max_hp = 300;
            m.strength = m.dexterity = m.intelligence = 20;
        }
        std::memset(g.quest.shrine_destroyed, 0, sizeof(g.quest.shrine_destroyed));
        g.quest.destroyed_count = 0;
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const BlackthornScenePacer &pacer() { return rt->blackthorn_pacer(); }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    std::function<void()> each; // sampled after every frame
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            rt->render(board);
            if (each) each();
        }
    }
    void run_ms(int64_t ms) { frames(int(ms / kFrameMs)); }
    bool until(const std::function<bool()> &done, int64_t max_ms) {
        for (int64_t t = 0; t < max_ms; t += kFrameMs) {
            if (done()) return true;
            frames(1);
        }
        return done();
    }
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

    // --- the panel -----------------------------------------------------------
    /** Scene cell (cx,cy)'s 16x16 pixels in the panel's GRAM are tile `t`'s. */
    static bool gram_cell_is(int cx, int cy, int t) {
        uint16_t want[256];
        tile_pixels(t, want);
        const uint16_t *p = bus::gram();
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                if (p[(kHudViewportY + cy * 16 + y) * kW + kHudViewportX + cx * 16 + x] != want[y * 16 + x])
                    return false;
        return true;
    }
    /** The same, accepting the other frame of a two-frame TileCycle (the
     *  table 0x80/0x81 and the body 0x82/0x83 animate on their own). */
    static bool gram_cell_shows(int cx, int cy, int t) {
        if (t >= 0x80 && t <= 0x83) return gram_cell_is(cx, cy, t & ~1) || gram_cell_is(cx, cy, t | 1);
        return gram_cell_is(cx, cy, t);
    }
    /** Cells of the 11x11 viewport whose GRAM shows tile 0. */
    static int tile0_cells() {
        int k = 0;
        for (int y = 0; y < 11; ++y)
            for (int x = 0; x < 11; ++x) k += gram_cell_is(x, y, 0);
        return k;
    }
    /** FNV of the right-hand panels (party, world, transcript). */
    static uint64_t panel_hash() {
        const uint16_t *p = bus::gram();
        uint64_t h = 1469598103934665603ull;
        for (int y = 0; y < 240; ++y)
            for (int x = kHudRightX; x < kW; ++x) h = (h ^ p[y * kW + x]) * 1099511628211ull;
        return h;
    }

    // --- the capture ---------------------------------------------------------
    /** Pass beside each palace guard until one captures the party. */
    bool capture(int &gx, int &gy, int &px, int &py) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::SmallMap;
        r.location = 18;
        r.floor = 0;
        r.standard_entry = true;
        if (apply_debug_teleport(ctx(), r).status != DebugTeleportStatus::Applied) return false;
        frames(4);
        const int dx[] = {0, 0, -1, 1}, dy[] = {1, -1, 0, 0};
        auto &actors = rt->actors();
        for (size_t i = 0; i < actors.count; ++i) {
            const auto &a = actors.actors[i];
            if (a.location != 18 || a.z != 0 || a.schedule.type != 112) continue;
            for (int d = 0; d < 4; ++d) {
                const auto ax = actors.actors[i].x, ay = actors.actors[i].y;
                g().position.map = {18, 0};
                g().position.xy = {uint8_t(ax + dx[d]), uint8_t(ay + dy[d])};
                frames(2);
                set_mark();
                key(' ');
                frames(2);
                if (pacer().active()) {
                    gx = ax;
                    gy = ay;
                    px = ax + dx[d];
                    py = ay + dy[d];
                    return true;
                }
            }
        }
        return false;
    }
    /** Enter at every getkey until the interrogation prompt is up. */
    bool to_prompt(int64_t max_ms = 60000) {
        return until(
            [&] {
                if (pacer().awaiting_key()) key('\r');
                return pacer().state() == BlackthornPacerState::Prompt && mode() == UiMode::TextEntry;
            },
            max_ms);
    }
    std::string mantra() {
        const auto &sd = pack->shrine_data;
        const int s = ctx().blackthorn->shrine;
        std::string out;
        if (s >= 0 && s < sd.count)
            for (auto c : sd.mantras[s]) out += char(c);
        return out;
    }
    /** Type an answer; the final Enter is left to the caller. */
    void answer_without_enter(const std::string &text) { type(text); }
};

// One frame of the sacrifice, sampled after it was drawn.
struct Sample {
    int64_t t = 0;            // ms since the answer's Enter
    bool burst = false;       // the binary's cell shows tile 0 in GRAM
    int tile0 = 0;            // cells showing tile 0
    bool victim = false;      // the victim still drawn on that cell
    bool dark = false;        // the cell shows what 0x0421/0x0429 leave
    bool sliced = false, rec = false;
    uint64_t panel = 0;       // the right-hand panels
    int state = 0;
    uint32_t released = 0;
};

struct Scene {
    int cx = -1, cy = -1;       // the burst's cell, from the binary's rule
    int victim_tile = -1;       // what the cell shows before (slot tile or 0x82)
    int after_tile = -1;        // what it shows after
    int64_t key_ms = 0;         // the answer's Enter
    size_t audio_mark = 0;
    std::vector<Sample> s;
    int64_t first(bool (*pred)(const Sample &)) const {
        for (const auto &x : s)
            if (pred(x)) return x.t;
        return -1;
    }
    int64_t last(bool (*pred)(const Sample &)) const {
        int64_t out = -1;
        for (const auto &x : s)
            if (pred(x)) out = x.t;
        return out;
    }
    int phases() const {
        int k = 0;
        bool prev = false;
        for (const auto &x : s) {
            if (x.burst && !prev) ++k;
            prev = x.burst;
        }
        return k;
    }
};

enum class Path { Pendulum, BetrayalAtOnce, BetrayalWarned };
const char *path_name(Path p) {
    return p == Path::Pendulum ? "pendulum" : p == Path::BetrayalAtOnce ? "betrayal at once" : "betrayal after a warning";
}

/**
 * Capture, interrogate and stop with the final answer typed, its Enter not yet
 * pressed. Fills the scene's cell and the tiles the binary puts on it.
 */
bool reach_final_answer(Run &r, Path path, Scene &sc, bool save_first = false) {
    int gx = 0, gy = 0, px = 0, py = 0;
    if (save_first) {
        // A save in the palace, before the guard's turn (N8's load target).
        DebugTeleportRequest q{};
        q.kind = DebugDestinationKind::SmallMap;
        q.location = 18;
        q.standard_entry = true;
        apply_debug_teleport(r.ctx(), q);
        r.frames(4);
        r.alt_save();
        r.frames(20);
    }
    if (!r.capture(gx, gy, px, py)) return false;
    static bool printed = false;
    if (!printed) {
        printed = true;
        std::printf("    captured passing at (%d,%d) beside the guard at (%d,%d), location 18 floor 0\n", px, py, gx,
                    gy);
    }
    if (!r.to_prompt()) return false;
    const std::string right = r.mantra();
    if (right.empty()) return false;
    int wrong = path == Path::Pendulum ? 3 : path == Path::BetrayalWarned ? 1 : 0;
    for (int i = 0; i < wrong; ++i) {
        r.type("XYZZY");
        r.key('\r');
        if (!r.to_prompt()) return false;
    }
    r.type(path == Path::Pendulum ? "XYZZY" : right);
    // The binary's cell, by its own rule (slot 1's last x/y), from the stage.
    const auto v = r.pacer().view();
    const auto &slot = v.stage.slots[1];
    if (slot.present) {
        sc.cx = slot.x;
        sc.cy = slot.y;
    }
    if (!v.tiles || sc.cx < 0) return false;
    sc.victim_tile = slot.visible ? slot.tile : v.tiles[sc.cy * kBlackthornSceneCols + sc.cx];
    sc.after_tile = slot.visible ? v.tiles[sc.cy * kBlackthornSceneCols + sc.cx] : kTable;
    return true;
}

/**
 * Sample every frame until `stop` (or `max_ms`). With `press`, the final Enter
 * is pressed first (its own frame is sampled) and the clock starts there;
 * without it the times stay relative to that earlier Enter.
 */
void watch(Run &r, Scene &sc, const std::function<bool(const Sample &)> &stop, int64_t max_ms, bool press) {
    if (press) {
        sc.key_ms = Run::now_ms();
        r.set_mark();
    }
    r.each = [&] {
        Sample x;
        x.t = Run::now_ms() - sc.key_ms;
        x.burst = Run::gram_cell_is(sc.cx, sc.cy, 0);
        x.tile0 = Run::tile0_cells();
        x.victim = Run::gram_cell_shows(sc.cx, sc.cy, sc.victim_tile);
        x.dark = Run::gram_cell_shows(sc.cx, sc.cy, sc.after_tile);
        x.panel = Run::panel_hash();
        x.sliced = r.shown("is sliced in half!");
        x.rec = !r.since_mark().empty();
        x.state = int(r.pacer().state());
        x.released = r.pacer().released_steps();
        sc.s.push_back(x);
    };
    if (press) r.key(13);
    for (int64_t t = 0; t < max_ms; t += kFrameMs) {
        if (!sc.s.empty() && stop(sc.s.back())) break;
        r.frames(1);
    }
    r.each = nullptr;
}
/** Press the final Enter and sample every frame until `stop` (or 12 s). */
void record(Run &r, Scene &sc, const std::function<bool(const Sample &)> &stop, int64_t max_ms = 12000) {
    watch(r, sc, stop, max_ms, true);
}

bool burst_on(const Sample &x) { return x.burst; }
bool is_dark(const Sample &x) { return x.dark && !x.burst; }
bool is_sliced(const Sample &x) { return x.sliced; }
bool has_rec(const Sample &x) { return x.rec; }

// --- N1-N5, N7, N10: each path, watched frame by frame --------------------------
void test_path(Path path, const char *id) {
    std::printf("\n%s the %s\n", id, path_name(path));
    Run r;
    Scene sc;
    const bool reached = reach_final_answer(r, path, sc);
    check(reached, id, std::string("the real capture reaches the final answer (") + path_name(path) +
                           "), the burst cell (" + n(sc.cx) + "," + n(sc.cy) + ")");
    if (!reached) return;
    const int64_t audio_from = openu5_host_virtual_clock_us();
    // Stop once the victim is gone and the scene has moved past the burst.
    record(r, sc, [&](const Sample &x) { return x.dark && !x.burst && x.t > kPauseMs + kSirenMs + kBurstMs + 400; });
    const int64_t on = sc.first(burst_on), off_last = sc.last(burst_on);
    const int64_t dark = sc.first(is_dark), sliced = sc.first(is_sliced), rec = sc.first(has_rec);
    const int64_t shown_ms = on >= 0 ? off_last - on + kFrameMs : -1;
    std::printf("    burst on %lld ms after the Enter, shown %lld ms, %d phase(s); victim dark at %lld; "
                "rec line at %lld; \"sliced\" at %lld\n",
                (long long)on, (long long)shown_ms, sc.phases(), (long long)dark, (long long)rec, (long long)sliced);

    const std::string p = std::string(" (") + path_name(path) + ")";
    // N1
    check(on >= 0, "N1", "the explosion is drawn on the victim's cell" + p);
    // N2
    bool only_cell = on >= 0;
    for (const auto &x : sc.s)
        if (x.burst && x.tile0 != 1) only_cell = false;
    const int wanted_x = path == Path::BetrayalAtOnce ? sc.cx : 5, wanted_y = path == Path::BetrayalAtOnce ? sc.cy : 7;
    check(only_cell && sc.cx == wanted_x && sc.cy == wanted_y, "N2",
          "exactly one cell, the binary's (" + n(sc.cx) + "," + n(sc.cy) + "): slot 1's last x/y" + p);
    bool panel_still = on >= 0;
    for (size_t i = 1; i < sc.s.size(); ++i)
        if (sc.s[i].burst && sc.s[i].panel != sc.s[i - 1].panel) panel_still = false;
    check(panel_still, "N2.1", "the side panels do not change while the burst is up" + p);
    // N3: tile 0's own pixels, every one of them -- an opaque blit, not an XOR.
    check(on >= 0 && !r.rt->ritual_fx().active() && r.rt->ritual_fx().mask() == 0, "N3",
          "an opaque blit of tile 0 (the cell is tile 0 pixel for pixel), no viewport XOR" + p);
    // N4
    const int64_t want_on = kPauseMs + kSirenMs;
    check(on >= want_on - kSlack && on <= want_on + kSlack, "N4",
          "the burst starts after pause(10) + the siren: " + n(on) + " ms (want " + n(want_on) + " +-" + n(kSlack) + ")" + p);
    check(sc.phases() == 1 && shown_ms >= kBurstMs - kSlack && shown_ms <= kBurstMs + kSlack, "N4.1",
          "one phase, held for the noise burst: " + n(shown_ms) + " ms (want " + n(kBurstMs) + ")" + p);
    // N5
    bool victim_under = true, victim_before = false;
    for (const auto &x : sc.s) {
        if (x.t >= on - kFrameMs && x.t < on && x.victim) victim_before = true;
        if (x.burst && x.dark && sc.victim_tile != sc.after_tile) victim_under = false;
    }
    check(rec >= 0 && rec < on && victim_before, "N5",
          "the rec line, then the victim still drawn right up to the burst" + p);
    check(dark >= 0 && dark >= off_last && dark <= off_last + kSlack, "N5.1",
          "the victim goes dark only when the burst ends (0x041e before 0x0421): dark at " + n(dark) + p);
    const int64_t noise = r.audio.first(SfxId::CombatHit, audio_from), siren = r.audio.first(SfxId::ShardSweep, audio_from);
    const int64_t noise_ms = noise >= 0 ? noise / 1000 - sc.key_ms : -1;
    check(siren >= 0 && noise >= 0 && siren < noise && noise_ms >= on - kFrameMs && noise_ms <= on + kFrameMs &&
              r.audio.count(SfxId::CombatHit, audio_from) == 1 && r.audio.count(SfxId::Quake, audio_from) == 0,
          "N5.2",
          "its sound is the kernel's noise burst, once, with the tile (0x355a; " + n(noise_ms) +
              " ms), after the siren; no shake" + p);
    if (path == Path::Pendulum)
        check(sliced >= off_last && sliced <= off_last + kSlack, "N5.3",
              "\"sliced in half!\" follows the burst, never before it: " + n(sliced) + p);
    // N7
    r.run_ms(1500);
    check(Run::tile0_cells() == 0 && r.pacer().view().burst_x < 0, "N7",
          "restored: no tile 0 anywhere afterwards, and the pacer holds no burst" + p);
    // N10: the key waits are still the original's
    if (path == Path::Pendulum) {
        check(r.pacer().awaiting_key(), "N10", "the pendulum still waits at getkey 0x04f6 after \"sliced in half!\"");
        r.set_mark();
        r.key('\r');
        r.run_ms(200);
        check(r.shown("treachery") || r.shown("\""), "N10.1", "one Enter releases rec6 (0x04f9), as before");
    } else {
        check(r.pacer().awaiting_key(), "N10", "the betrayal still waits at getkey 0x0510 before the finale");
    }
}

// Run a path until the burst is up; returns false if it never comes.
bool to_burst(Run &r, Path path, Scene &sc, bool save_first = false) {
    if (!reach_final_answer(r, path, sc, save_first)) return false;
    record(r, sc, [](const Sample &x) { return x.burst; }, kPauseMs + kSirenMs + 2000);
    return !sc.s.empty() && sc.s.back().burst;
}

// --- N6: input inside the burst ---------------------------------------------------
void test_input() {
    std::printf("\nN6 keys, the trackball and Mic inside the burst (pendulum)\n");
    Run r;
    Scene sc;
    const bool up = to_burst(r, Path::Pendulum, sc);
    check(up, "N6.0", "the burst is up");
    if (!up) return;
    const uint32_t cmds = r.rt->routed_command_count(), released = r.pacer().released_steps();
    const auto pos = r.g().position;
    const int64_t t0 = Run::now_ms();
    r.set_mark();
    r.key('e');
    r.key('\r');
    r.ball(RawInputKind::TrackballLeft);
    r.ball(RawInputKind::TrackballDown);
    r.mic();
    const int64_t used = Run::now_ms() - t0;
    const bool still = Run::gram_cell_is(sc.cx, sc.cy, 0) || used >= kBurstMs - kSlack;
    check(r.rt->routed_command_count() == cmds && r.g().position.xy.x == pos.xy.x && r.g().position.xy.y == pos.xy.y &&
              r.pacer().released_steps() == released + 0 && still,
          "N6", "no command, no move and no released step (" + n(used) + " ms of input)");
    // The burst runs to its end and the scene continues on its own clock --
    // and the Enter pressed inside the burst did NOT pre-answer getkey 0x04f6.
    r.until([&] { return r.shown("is sliced in half!"); }, 1000);
    r.run_ms(300);
    check(r.shown("is sliced in half!") && r.pacer().awaiting_key() && !r.shown("treachery"), "N6.1",
          "the burst ends by itself; the Enter inside it did not satisfy getkey 0x04f6");
}

// --- N8: load / title inside the burst --------------------------------------------
void test_load() {
    std::printf("\nN8 a successful load inside the burst\n");
    Run r;
    Scene sc;
    const bool up = to_burst(r, Path::Pendulum, sc, true);
    check(up, "N8.0", "the burst is up (a save was made in the palace first)");
    if (!up) return;
    r.alt_load();
    r.run_ms(600);
    check(!r.pacer().active() && r.pacer().view().burst_x < 0 && Run::tile0_cells() == 0 &&
              r.g().position.map.location == 18 && r.g().party.party_size == 3,
          "N8", "the loaded game is drawn with no burst and no scene; the party is whole again");
    r.run_ms(8000);
    check(Run::tile0_cells() == 0 && !r.pacer().active(), "N8.1", "and nothing of the sacrifice replays later");
}
void test_title() {
    std::printf("\nN8 Return to Title inside the burst, then Continue\n");
    Run r;
    Scene sc;
    const bool up = to_burst(r, Path::Pendulum, sc, true);
    check(up, "N8T.0", "the burst is up (a save was made in the palace first)");
    if (!up) return;
    // The title only hands the screen over (as for HF7's rite); the world and
    // its scene are replaced by the load Continue performs.
    r.return_to_title();
    r.key('x');
    r.key('j');
    r.key('\r'); // the title's Continue (HF7's own sequence)
    r.run_ms(600);
    check(!r.pacer().active() && r.pacer().view().burst_x < 0 && Run::tile0_cells() == 0 &&
              r.g().party.party_size == 3,
          "N8T", "Continue from the title shows the saved game: no scene, no burst, the party whole");
    r.run_ms(8000);
    check(Run::tile0_cells() == 0 && !r.pacer().active(), "N8T.1", "and nothing of the sacrifice replays later");
}

// --- N9: the System Menu inside the burst -----------------------------------------
void test_menu() {
    std::printf("\nN9 the System Menu inside the burst\n");
    Run r;
    Scene sc;
    const bool up = to_burst(r, Path::Pendulum, sc);
    check(up, "N9.0", "the burst is up");
    if (!up) return;
    const uint32_t released = r.pacer().released_steps();
    r.set_mark();
    r.menu_toggle();
    const bool open = r.rt->system_menu_open();
    r.run_ms(2000);
    check(open && r.pacer().released_steps() == released && r.since_mark().empty(), "N9",
          "nothing is released behind the open menu (no victim cleared, no \"sliced\")");
    r.menu_toggle();
    r.run_ms(400);
    check(!r.rt->system_menu_open() && Run::tile0_cells() == 0 && r.shown("is sliced in half!") &&
              r.pacer().awaiting_key(),
          "N9.1", "closed: the scene resumes, the burst is gone, and it waits at getkey 0x04f6");
}

// --- N9.2: the System Menu inside the SIREN (H-208 step 11) -------------------------
void test_menu_siren() {
    std::printf("\nN9.2 the System Menu inside the siren, before the burst\n");
    Run r;
    Scene sc;
    const bool ok = reach_final_answer(r, Path::Pendulum, sc);
    if (!ok) {
        check(false, "N9.2", "the real capture reaches the final answer");
        return;
    }
    record(r, sc, [](const Sample &x) { return x.t >= 3000; });
    const bool none_yet = sc.phases() == 0;
    r.menu_toggle();
    r.run_ms(3000);
    const bool open = r.rt->system_menu_open();
    r.menu_toggle();
    watch(r, sc, [](const Sample &x) { return x.dark && !x.burst && x.sliced; }, 8000, false);
    const int64_t on = sc.first(burst_on), off_last = sc.last(burst_on), dark = sc.first(is_dark);
    const int64_t shown_ms = on >= 0 ? off_last - on + kFrameMs : -1;
    check(none_yet && open && sc.phases() == 1 && shown_ms >= kBurstMs - kSlack && shown_ms <= kBurstMs + kSlack &&
              dark >= off_last && sc.first(is_sliced) >= off_last,
          "N9.2",
          "closed after the siren's hold ran out behind it: the burst still shows once, for " + n(shown_ms) +
              " ms, on the table, before the victim goes and before \"sliced in half!\"");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <openu5-alpha1-resources.bin> <openu5-audio.bin> [--only <name>]\n", argv[0]);
        return 2;
    }
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    const char *only = argc > 4 && std::strcmp(argv[3], "--only") == 0 ? argv[4] : nullptr;
    const std::pair<const char *, std::function<void()>> tests[] = {
        {"pendulum", [] { test_path(Path::Pendulum, "P"); }},
        {"betrayal", [] { test_path(Path::BetrayalAtOnce, "B"); }},
        {"betrayal-warned", [] { test_path(Path::BetrayalWarned, "W"); }},
        {"input", test_input},
        {"load", test_load},
        {"title", test_title},
        {"menu", test_menu},
        {"menu-siren", test_menu_siren},
    };
    for (const auto &t : tests)
        if (!only || std::strcmp(only, t.first) == 0) t.second();
    std::printf("\na3_hf8_sacrifice_burst_runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
