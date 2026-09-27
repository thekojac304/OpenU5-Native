// Alpha 3 A3-04E -- the game thread's pacing through the REAL AlphaRuntime
// AND the REAL tdeck_board.cpp (ALPHA3_AUDIO.md section 22). The Board is
// compiled for the host over host_tests/board_shims: a fake ST7789 that
// decodes the byte stream into a 320x240 panel, and a model of the SPI
// transaction cost and of the 100 Hz tick on the esp_timer shim's virtual
// clock (fake_tdeck_bus.h says which numbers and where they come from).
//
//   Y  the draw loops' pauses, counted in the real Board code: legacy = a tick
//      sleep at each of them, A3-04E = a yield at the same points, no sleep
//   M  the modelled TFT cost of the A3-04C/D measurement procedure (window
//      read inside the Developer menu, the repaint on leaving it, a walk),
//      legacy vs A3-04E: the reconciliation with the device's ~400 / ~120 ms
//   V  the panel: the same bytes in the same order under both, and after
//      every gameplay frame the panel's viewport IS the composed viewport
//   L  main.cpp's loop gate: idle -> one tick; legacy spin, a pending frame,
//      the benchmark, a paced Camp -> none
//   P  the two Developer probes: rows, labels, the report's pacing section,
//      A3E_PACE, the older rows' places, never saved
//   T  the game is untouched
//
//   a3_04e_pacing_runtime <openu5-alpha1-resources.bin>
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/audio_stream.h"
#include "openu5/perf_report.h"
#include "openu5/render_pacing.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <io.h>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int64_t kClockStartUs = 5'000'000;

std::string ms(uint64_t us) {
    char b[32];
    std::snprintf(b, sizeof b, "%llu.%llu ms", (unsigned long long)(us / 1000), (unsigned long long)(us % 1000 / 100));
    return b;
}

// The audio task's published window, as the device backend gives it (the
// benchmark row needs one).
struct FakeAudioPerf final : AudioPerfSource {
    AudioPerfSnapshot snap{};
    bool perf_snapshot(AudioPerfSnapshot &out) const override {
        out = snap;
        return true;
    }
    void perf_reset() override {}
    bool set_music_bypass(bool) override { return true; }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{}; // A3-04E.1: the device's own guard, on the modelled idle counter
    bool guard = true;
    bool streaming = false; // input always queued: main.cpp's peek never waits
    size_t viewport_mismatches = 0, gameplay_checks = 0;
    // A model-level task watchdog: the longest time core 0's idle loop went unrun.
    uint32_t seen_passes = 0;
    int64_t seen_at = 0, idle_gap_max = 0;
    Run(int seed, bool timed, bool paced = false, bool rows_feed_idle = true, bool with_guard = true) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = timed;
        bus::model().rows_feed_idle = rows_feed_idle;
        board.initialize_display(); // the real ST7789 sequence and boot text, on the fake bus
        openu5_host_virtual_clock_us() = kClockStartUs;
        guard = with_guard;
        if (guard) { // as main.cpp wires it
            idle.attach(bus::idle_passes());
            board.set_idle_service(&idle);
            rt->attach_idle_service(&idle);
        }
        seen_passes = *bus::idle_passes();
        seen_at = openu5_host_virtual_clock_us();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        f.paced_scenes = paced;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {80, 80};
        g.time.hour = 12;
        g.time.minute = 55;
        g.food = 80;
        g.party.character_count = g.party.party_size = 2;
        g.party.active_character = 255;
        for (int i = 0; i < 2; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "Member%d", i);
            m.status = 'G';
            m.character_class = i ? 'M' : 'F';
            m.level = 1;
            m.current_hp = 5;
            m.max_hp = 30;
            m.strength = m.dexterity = m.intelligence = 10;
        }
        g.party.characters[0].exp = 100;
        g.rng.seed(uint32_t(seed));
        render(true);
    }
    bool gameplay() const { return rt->ui() && rt->ui()->mode() != UiMode::DebugMenu; }
    // After every gameplay render, the panel's viewport (below the 9 px sky
    // strip, above the 9 px wind strip) must be exactly the composed viewport.
    void render(bool force = false) {
        rt->render(board, force);
        if (!gameplay() || !rt->composed_viewport()) return;
        ++gameplay_checks;
        const uint16_t *vp = rt->composed_viewport(), *panel = bus::gram();
        for (int y = 9; y < 167; ++y)
            if (!std::equal(vp + y * 176, vp + y * 176 + 176, panel + (4 + y) * 320 + 4)) {
                ++viewport_mismatches;
                break;
            }
    }
    void observe_idle() {
        const uint32_t passes = *bus::idle_passes();
        const int64_t now = openu5_host_virtual_clock_us();
        if (passes != seen_passes) {
            seen_passes = passes;
            seen_at = now;
        } else if (now - seen_at > idle_gap_max) {
            idle_gap_max = now - seen_at;
        }
    }
    /** main.cpp's end of a pass: the idle wait (unless input is queued), then the guard. */
    void pass_end() {
        observe_idle();
        if (!streaming && rt->loop_wait_ticks()) bus::idle_wait_one_tick();
        if (guard) idle.enforce();
        observe_idle();
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        render();
        pass_end();
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    void left() { ball(RawInputKind::TrackballLeft); }
    void right() { ball(RawInputKind::TrackballRight); }
    /** The device loop: 5 ms passes with render() each, for `ms` of script time. */
    void run(int64_t ms) {
        const int64_t end = openu5_host_virtual_clock_us() + ms * 1000;
        while (openu5_host_virtual_clock_us() < end) {
            openu5_host_virtual_clock_us() += 5000;
            render();
            pass_end();
        }
    }
    /**
     * Continuous walking with the trackball's events always queued (main.cpp's
     * peek returns at once): 1 ms passes of busy work, a step every 100 ms.
     */
    void stream_walk(int steps) {
        streaming = true;
        int64_t next_step = openu5_host_virtual_clock_us();
        int step = 0;
        while (step < steps || openu5_host_virtual_clock_us() < next_step) {
            openu5_host_virtual_clock_us() += 1000;
            if (step < steps && openu5_host_virtual_clock_us() >= next_step) {
                tdeck::RawInputEvent e{};
                e.kind = step++ % 2 ? RawInputKind::TrackballRight : RawInputKind::TrackballLeft;
                e.timestamp_us = openu5_host_virtual_clock_us();
                rt->handle(e);
                next_step = openu5_host_virtual_clock_us() + 100000;
            }
            render();
            pass_end();
        }
        streaming = false;
    }
    void camp() { key('h'); key('1'); key('\r'); key('n'); }
    void open_diagnostics() {
        key('d', true);
        up();
        up();
        key('\r');
    }
    void ups(int n) {
        for (int i = 0; i < n; ++i) up();
    }
    void leave_menu() {
        key('\b');
        key('\b');
    }
    /** Developer > Diagnostics: the legacy probes (7 and 6 rows up), then out. */
    void set_legacy(bool tft, bool loop) {
        open_diagnostics();
        ups(7);
        if (tft) key('\r');
        down();
        if (loop) key('\r');
        leave_menu();
    }
    /** The A3-04C/D checklist's window: read "Audio/render stats (live)" (two up), dismiss, leave. */
    void start_window_in_menu() {
        open_diagnostics();
        ups(2);
        key('\r'); // the read starts the window
        key('\r'); // dismiss the report
        key('\b'); // out of Diagnostics
    }
    void walk(int steps) {
        for (int i = 0; i < steps; ++i) {
            if (i % 2) right();
            else left();
            run(300);
        }
    }
    std::string state() {
        auto &g = rt->game();
        std::ostringstream s;
        s << int(g.position.map.location) << ',' << int(g.position.xy.x) << ',' << int(g.position.xy.y) << ','
          << int(g.time.hour) << ':' << int(g.time.minute) << ',' << g.food << ",rng" << g.rng.get_seed();
        return s.str();
    }
};

std::string report_text(const tdeck::AlphaRuntime &rt) {
    std::string out;
    for (size_t i = 0; i < rt.perf_report_line_count(); ++i) out += std::string(rt.perf_report_line(i)) + "\n";
    return out;
}

/** The pauses of one frame, grouped by the panel window they interrupted. */
std::string pause_breakdown() {
    std::map<std::tuple<int, int, int, int>, int> by_window;
    for (const auto &p : bus::pauses()) ++by_window[{p.x0, p.y0, p.x1, p.y1}];
    std::string out;
    for (const auto &[w, n] : by_window) {
        char b[80];
        std::snprintf(b, sizeof b, "\n      %2d in window x %d-%d y %d-%d (%dx%d)", n, std::get<0>(w), std::get<2>(w),
                      std::get<1>(w), std::get<3>(w), std::get<2>(w) - std::get<0>(w) + 1,
                      std::get<3>(w) - std::get<1>(w) + 1);
        out += b;
    }
    return out;
}

struct Window {
    ContentionSnapshot c{};
    RenderPerfSnapshot r{};
    uint64_t full_sleeps = 0, full_yields = 0, full_transactions = 0, full_sleep_us = 0;
    uint64_t step_sleeps = 0, step_yields = 0, step_transactions = 0;
    std::string full_pauses;
    size_t mismatches = 0, gameplay_checks = 0;
    std::string line, report;
};

/** The A3-04C/D procedure on the modelled bus: window read in the menu, leave (full repaint), walk. */
Window measure(bool legacy_tft, bool legacy_loop) {
    Run h(7, /*timed=*/true);
    if (legacy_tft || legacy_loop) h.set_legacy(legacy_tft, legacy_loop);
    else {
        h.open_diagnostics();
        h.leave_menu();
    }
    h.start_window_in_menu();
    Window w{};
    bus::reset_stats();
    h.key('\b'); // leave the Developer menu: the next gameplay frame repaints the whole screen
    w.full_sleeps = bus::stats().tick_sleeps;
    w.full_yields = bus::stats().yields;
    w.full_transactions = bus::stats().transactions;
    w.full_sleep_us = bus::stats().sleep_us;
    w.full_pauses = pause_breakdown();
    h.run(300);
    bus::reset_stats();
    h.left(); // one walking step: the viewport's 158 rows and the panels
    w.step_sleeps = bus::stats().tick_sleeps;
    w.step_yields = bus::stats().yields;
    w.step_transactions = bus::stats().transactions;
    h.run(300);
    h.walk(11);
    const uint64_t now = uint64_t(openu5_host_virtual_clock_us());
    h.rt->contention().snapshot(now, w.c);
    h.rt->render_perf().snapshot(now, w.r);
    char line[kPacingLineBytes];
    h.rt->pacing_line(line, sizeof line);
    w.line = line;
    w.mismatches = h.viewport_mismatches;
    w.gameplay_checks = h.gameplay_checks;
    return w;
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

    // ---- Y / M: the pauses, and the modelled cost of the device's procedure --
    const Window legacy = measure(true, true), e = measure(false, false);
    std::printf("legacy full repaint pauses:%s\n", legacy.full_pauses.c_str());
    check(legacy.full_sleeps == 37 && legacy.full_yields == 0,
          "Y1 legacy: the repaint on leaving the Developer screen sleeps to the next tick " +
              std::to_string(legacy.full_sleeps) + " times (full clear 7, viewport 9, right-panel reflow 7, the two "
              "180-row viewport-frame sides 5+5 -- one transaction per 2 px row -- and the party and world frame sides "
              "1+1+1+1)");
    check(legacy.step_sleeps == 9 && legacy.step_yields == 0,
          "Y2 legacy: a walking step sleeps " + std::to_string(legacy.step_sleeps) +
              " times: the viewport's 158 rows, one sleep after rows 16, 32 .. 144");
    check(e.full_sleeps == 0 && e.step_sleeps == 0 && e.full_yields == legacy.full_sleeps &&
              e.step_yields == legacy.step_sleeps && e.full_transactions == legacy.full_transactions &&
              e.step_transactions == legacy.step_transactions,
          "Y3 A3-04E: no tick sleep in either; a yield at exactly the same " + std::to_string(e.full_yields) + " + " +
              std::to_string(e.step_yields) + " points, and the same " + std::to_string(e.full_transactions) + " + " +
              std::to_string(e.step_transactions) + " SPI transactions");

    std::printf("modelled window, legacy : %s\n", legacy.line.c_str());
    std::printf("modelled window, A3-04E : %s\n", e.line.c_str());
    check(legacy.c.full_frames == 1 && legacy.c.full_tft_max_us >= 360000 && legacy.r.tft_max_us == legacy.c.full_tft_max_us,
          "M1 legacy: the window's TFT maximum IS its one full-screen repaint, " + ms(legacy.c.full_tft_max_us) +
              " modelled (37 sleeps span >= 36 ticks; device A3-04D: 404-408 ms)");
    check(legacy.c.vp_only_frames >= 10 && legacy.c.vp_only_tft_avg_us >= 80000,
          "M2 legacy: a walking viewport frame costs " + ms(legacy.c.vp_only_tft_avg_us) +
              " modelled (9 sleeps span >= 8 ticks; device viewport avg ~122 ms incl. the repaint and row building)");
    check(e.c.full_frames == 1 && e.c.full_tft_max_us * 2 < legacy.c.full_tft_max_us &&
              e.c.vp_only_tft_avg_us * 2 < legacy.c.vp_only_tft_avg_us && e.c.yield_total_us < 1000,
          "M3 A3-04E: the repaint " + ms(e.c.full_tft_max_us) + " and a step " + ms(e.c.vp_only_tft_avg_us) +
              " modelled (transfers only; the device adds row building), " + ms(e.c.yield_total_us) +
              " of pauses in the whole window");
    check(e.r.input_max_us < legacy.r.input_max_us && e.r.frame_max_us < legacy.r.frame_max_us,
          "M4 ... input-to-screen max " + ms(e.r.input_max_us) + " (legacy " + ms(legacy.r.input_max_us) +
              "), frame max " + ms(e.r.frame_max_us) + " (legacy " + ms(legacy.r.frame_max_us) + ")");
    check(legacy.line.find("pace=[tft TICK  loop SPIN]") == 0 && e.line.find("pace=[tft yield  loop idle-wait]") == 0 &&
              e.line.find(" | full=1:") != std::string::npos && e.line.find(" | yld n=") != std::string::npos,
          "M5 A3E_PACE names the pacing and carries the repaint / viewport split and the pauses");

    // ---- V / T: the panel and the game are the same under both ------------
    {
        struct Result {
            uint64_t hash;
            std::vector<uint16_t> gram;
            std::string state;
            size_t mismatches, gameplay_checks;
            uint64_t sleeps, yields, malformed;
        };
        auto scenario = [](bool legacy_tft) {
            Run h(11, /*timed=*/false); // the script alone moves the clock: identical content
            h.set_legacy(legacy_tft, false);
            // From here on the two runs differ in the pause alone (the probe
            // row's own "ON" / "off" is behind us, on the menu just left).
            bus::restart_stream();
            h.walk(10);
            h.open_diagnostics(); // the menu and its repaint on leaving
            h.leave_menu();
            h.run(600); // animation frames
            h.walk(6);
            return Result{bus::stream_hash(), std::vector<uint16_t>(bus::gram(), bus::gram() + 320 * 240), h.state(),
                          h.viewport_mismatches, h.gameplay_checks, bus::stats().tick_sleeps, bus::stats().yields,
                          bus::stats().malformed};
        };
        const Result tick = scenario(true), yield = scenario(false);
        check(tick.hash == yield.hash && tick.gram == yield.gram && tick.sleeps > 0 && yield.sleeps == 0 &&
                  tick.yields == 0 && yield.yields == tick.sleeps,
              "V1 legacy and A3-04E pacing put byte-identical streams on the panel (every transaction, in order: "
              "hash " + std::to_string(tick.hash) + " / " + std::to_string(yield.hash) +
              "), the same final panel; only the pause differs (" + std::to_string(tick.sleeps) + " tick sleeps vs " +
              std::to_string(yield.yields) + " yields)");
        check(tick.mismatches == 0 && yield.mismatches == 0 && yield.gameplay_checks > 100 && tick.malformed == 0 &&
                  yield.malformed == 0,
              "V2 after every one of " + std::to_string(yield.gameplay_checks) +
                  " gameplay renders the panel's viewport is the composed viewport, row for row: no incomplete "
                  "frame, no corrupted or missing row, no malformed transaction");
        check(tick.state == yield.state, "T1 the game after the same script is the same (" + yield.state + ")");
    }
    {
        const Window a = measure(true, true), b = measure(false, false);
        check(a.mismatches == 0 && b.mismatches == 0 && b.gameplay_checks > 100,
              "V3 ... also on the timed runs (" + std::to_string(b.gameplay_checks) + " renders)");
    }

    // ---- L: main.cpp's loop gate ------------------------------------------
    {
        Run h(3, true);
        h.run(100);
        check(h.rt->loop_may_sleep() && h.rt->loop_wait_ticks() == kLoopIdleWaitTicks && kLoopIdleWaitTicks >= 1,
              "L1 idle exploration: the loop blocks on the input queue for one tick after a pass");
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::TrackballLeft;
        openu5_host_virtual_clock_us() += 100000;
        e.timestamp_us = openu5_host_virtual_clock_us();
        h.rt->handle(e);
        check(!h.rt->loop_may_sleep() && h.rt->loop_wait_ticks() == 0,
              "L2 a handled input whose frame is not drawn yet: no sleep");
        h.render();
        check(h.rt->loop_wait_ticks() == 1, "L3 ... drawn: the idle wait again");
        h.set_legacy(false, true);
        h.run(50);
        check(h.rt->loop_may_sleep() && h.rt->loop_wait_ticks() == 0 && h.rt->pacing().loop == LoopPacing::Spin,
              "L4 \"Probe: legacy loop spin: ON\": idle, yet 0 -- the pre-A3-04E reschedule");
        h.set_legacy(false, true);
        FakeAudioPerf audio;
        h.rt->attach_audio_perf(&audio);
        h.open_diagnostics();
        h.ups(3);
        h.key('\r'); // "Audio/render performance": the benchmark, a timeline
        check(h.rt->audio_benchmark_running() && !h.rt->loop_may_sleep() && h.rt->loop_wait_ticks() == 0,
              "L5 while the audio benchmark runs: no sleep");
    }
    {
        int seed = -1;
        for (int s = 1; s <= 512 && seed < 0; ++s) {
            Run candidate(s, false);
            candidate.camp();
            if (candidate.rt->commands().camp_advance.phase != CommandState::CampAdvance::Phase::None) seed = s;
        }
        check(seed > 0, "L6 setup: a raw Camp reaches the paced apparition");
        if (seed > 0) {
            Run h(seed, false, /*paced=*/true);
            h.camp();
            const bool staged = h.rt->narrative_pacer().active() && !h.rt->loop_may_sleep() && h.rt->loop_wait_ticks() == 0;
            size_t slept_while_paced = 0;
            for (int i = 0; i < 4000 && (h.rt->narrative_pacer().active() || h.rt->narrative_pacer().mounted()); ++i) {
                if (h.rt->loop_wait_ticks()) ++slept_while_paced;
                openu5_host_virtual_clock_us() += 5000;
                h.render();
            }
            check(staged && slept_while_paced == 0,
                  "L6 a paced Camp scene (resume_at = now + dwell): the loop never sleeps while the pacer is active "
                  "or mounted, so each dwell is A3-04D's to the microsecond");
        }
    }

    // ---- W: the idle-service guarantee (A3-04E.1, section 23) ----------------
    // The hardware's worst case, modelled: the rows' own blocks do not let the
    // idle task finish a pass (rows_feed_idle=false), and the trackball keeps
    // an event queued, so main.cpp's peek never waits.
    {
        Run bare(21, true, false, /*rows_feed_idle=*/false, /*guard=*/false);
        bare.stream_walk(60);
        check(bare.idle_gap_max >= 5000000,
              "W1 A3-04E as shipped (yields, no guard) reproduces the hardware's task watchdog in the model: core 0's "
              "idle loop goes " + ms(uint64_t(bare.idle_gap_max)) + " without a pass while the game thread walks "
              "(the watchdog's limit is 5 s)");
        Run g(21, true, false, false, true);
        g.stream_walk(60);
        const auto &st = g.idle.stats();
        const int64_t window = openu5_host_virtual_clock_us() - kClockStartUs;
        check(g.idle_gap_max <= int64_t(kIdleServiceBudgetUs) + 50000 && st.unserviced == 0 && st.enforcements > 0 &&
                  st.forced_us * 100 <= uint64_t(window) * 6,
              "W2 with the guard the longest idle gap is " + ms(uint64_t(g.idle_gap_max)) + " (budget " +
                  ms(kIdleServiceBudgetUs) + "); it slept " + std::to_string(st.forced_sleeps) + " ticks in " +
                  std::to_string(st.enforcements) + " enforcements = " + ms(st.forced_us) + " of " +
                  ms(uint64_t(window)) + " (60 steps), never in vain");
        g.open_diagnostics();
        g.ups(2);
        g.key('\r'); // "Audio/render stats (live)": publishes this window, starts the next
        const bool shown = report_text(*g.rt).find("idle0 gap max 200.") != std::string::npos;
        check(shown && g.idle.stats().enforcements == 0 && g.idle.stats().forced_sleeps == 0,
              "W7 the live report shows the idle gap, and the read starts a new idle-service window with the others");
        check(g.state() == bare.state(),
              "W3 ... and the 60 steps end where they do without the guard's sleeps (" + g.state() +
                  "): the game is untouched");
    }
    {
        Run h(7, true, false, /*rows_feed_idle=*/false);
        h.streaming = true;
        h.run(300);
        h.open_diagnostics();
        bus::reset_stats();
        h.key('\b');
        h.key('\b'); // leave: the full-screen repaint, with the idle loop starved throughout
        const uint64_t sleeps = bus::stats().tick_sleeps;
        check(sleeps <= 1, "W4 the menu-exit repaint under the worst case costs " + std::to_string(sleeps) +
                               " guard tick sleep(s) at most one per 200 ms -- not the 37 of the legacy pause");
    }
    {
        Run fed(22, true, false, /*rows_feed_idle=*/true);
        fed.stream_walk(30);
        Run legacy_tft(22, true, false, /*rows_feed_idle=*/false);
        legacy_tft.set_legacy(true, false);
        legacy_tft.idle.reset_stats();
        legacy_tft.stream_walk(30);
        Run standing(22, true, false, /*rows_feed_idle=*/false);
        standing.run(3000);
        check(fed.idle.stats().enforcements == 0 && legacy_tft.idle.stats().enforcements == 0 &&
                  standing.idle.stats().enforcements == 0 && standing.idle_gap_max < int64_t(kIdleServiceBudgetUs),
              "W5 the guard costs nothing wherever the idle loop already runs: rows that let it (A3-04E's model), "
              "the legacy tick-sleep pacing, and the loop's own idle wait when nothing is queued (enforcements " +
                  std::to_string(fed.idle.stats().enforcements) + "/" + std::to_string(legacy_tft.idle.stats().enforcements) +
                  "/" + std::to_string(standing.idle.stats().enforcements) + "; the longest gap standing is the first full frame, " +
                  ms(uint64_t(standing.idle_gap_max)) + ")");
        char line[kPacingLineBytes];
        fed.rt->pacing_line(line, sizeof line, 7);
        const std::string l = line;
        check(l.find("hb=7 pace=[tft yield  loop idle-wait]") == 0 && l.find(" | idle0 gap=") != std::string::npos &&
                  l.find(" forced=0:0/0.0 miss=0") != std::string::npos,
              "W6 A3E_PACE carries the heartbeat number and the idle-service window:\n    " + l);
    }

    // ---- H: A3E_PACE is emitted by every heartbeat, right after A3C_PERF -----
    {
        Run h(8, true);
        h.walk(2);
        const char *path = "a3_04e_heartbeat_capture.txt";
        std::fflush(stdout);
        const int saved = _dup(_fileno(stdout));
        FILE *file = std::fopen(path, "w+");
        _dup2(_fileno(file), _fileno(stdout));
        h.rt->log_metrics("heartbeat");
        h.rt->log_metrics("heartbeat");
        std::fflush(stdout);
        _dup2(saved, _fileno(stdout));
        _close(saved);
        std::fclose(file);
        const std::string out = [&] {
            std::ifstream in(path, std::ios::binary);
            std::stringstream s;
            s << in.rdbuf();
            return s.str();
        }();
        std::remove(path);
        const auto first_c = out.find("A3C_PERF "), first_e = out.find("A3E_PACE hb=1 pace=[");
        const auto second_c = out.find("A3C_PERF ", first_c + 1), second_e = out.find("A3E_PACE hb=2 pace=[");
        const auto npos = std::string::npos;
        check(first_c != npos && first_e != npos && second_c != npos && second_e != npos && first_e > first_c &&
                  second_c > first_e && second_e > second_c && out.find('\n', first_e) < second_c,
              "H1 every heartbeat logs A3E_PACE (hb=1, hb=2) directly after A3C_PERF, as its last perf line -- the "
              "code emits it; on the device the USB-Serial/JTAG console drops the tail of a burst once the host "
              "stops reading for 50 ms (section 23)");
    }

    // ---- P: the probes ------------------------------------------------------
    {
        Run h(5, true);
        check(h.rt->pacing().tft == kPacingDefault.tft && h.rt->pacing().loop == kPacingDefault.loop &&
                  h.board.tft_pacing() == TftPacing::Yield,
              "P1 a fresh runtime (a reboot) paces as A3-04E: yields, idle wait; the Board draws with it");
        h.open_diagnostics();
        h.ups(7);
        h.key('\r');
        check(h.rt->pacing().tft == TftPacing::TickSleep && h.rt->pacing().loop == LoopPacing::IdleWait &&
                  h.board.tft_pacing() == TftPacing::TickSleep,
              "P2 seven rows up, Enter: \"Probe: legacy TFT pacing\" ON -- the Board sleeps to the tick again from "
              "the next draw on; the loop is untouched");
        h.down();
        h.key('\r');
        check(h.rt->pacing().loop == LoopPacing::Spin && h.rt->pacing().tft == TftPacing::TickSleep,
              "P3 six rows up, Enter: \"Probe: legacy loop spin\" ON");
        for (int i = 0; i < 4; ++i) h.down();
        h.key('\r'); // "Audio/render stats (live)", two up
        const std::string text = report_text(*h.rt);
        check(text.find("-- Pacing (A3-04E) --\ntft TICK  loop SPIN\nfull-screen ") != std::string::npos &&
                  text.find("pauses/vp frm ") != std::string::npos && text.find("loop asleep ") != std::string::npos,
              "P4 the live report names the pacing in use and its cost:\n" + text);
        h.key('\r'); // dismiss
        h.ups(4);
        h.key('\r');
        h.up();
        h.key('\r');
        check(h.rt->pacing().tft == TftPacing::Yield && h.rt->pacing().loop == LoopPacing::IdleWait,
              "P5 Enter again on each: both back to A3-04E");
        FakeAudioPerf audio;
        h.rt->attach_audio_perf(&audio);
        for (int i = 0; i < 3; ++i) h.down(); // four up
        h.key('\r');
        const bool bypass = h.rt->music_bypass();
        h.up(); // five up
        h.key('\r');
        check(bypass && h.rt->perf_report_open() &&
                  report_text(*h.rt).find("SD diag logging: no SD card logger on this device") != std::string::npos,
              "P6 the older rows keep their places counted from the end: the synth bypass is four up, the SD log "
              "five up");
        h.key('\r');
        h.leave_menu();
        Run fresh(5, true);
        check(fresh.rt->pacing().tft == TftPacing::Yield && fresh.rt->pacing().loop == LoopPacing::IdleWait,
              "P7 nothing is saved: a new runtime starts at A3-04E whatever the last one did");
    }

    std::printf("A3-04E pacing runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
