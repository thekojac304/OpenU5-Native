// Alpha 3 A3-HF2 -- ambient SFX parity: the grandfather clock's strike and the
// fountain, through the REAL AlphaRuntime on A3-04E's device-loop model (the
// REAL tdeck_board.cpp over board_shims: timed SPI, the 100 Hz tick, main.cpp's
// idle wait and the IdleService guard), with the PC-speaker player behind a
// recording backend (ALPHA3_AUDIO.md section 24).
//
//   K  the clock. ULTIMA.EXE advance_clock 0x4f7c saves the hour in [0x5880]
//      (0x4fa0) and re-arms the strike counter [0x5884] to the 12-hour clock
//      (0x5164-0x5183) ONLY when the hour moved: 0x514a-0x5151
//      `mov al,[0x5880]; cmp [0x587f],al; je 0x5186` skips it otherwise.
//      [0x5884] has no other writer in ULTIMA.EXE or any overlay
//      (a3-hf2-derivation.log). A town step costs one minute (town_turn ->
//      advance_clock(1)): inside the hour it strikes nothing; the step that
//      crosses the hour strikes the new hour; tick / tock stay 0x4102's.
//   F  the fountain, standing still: getkey_with_redraw 0x266c calls the
//      redraw 0x5910 (and so 0x4102) on every pass while no key is down, so
//      the burble never depends on movement.
//
//   a3_hf2_ambient_parity <openu5-alpha1-resources.bin> <openu5-audio.bin>
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/ambient_sfx.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/debug_map_picker.h"
#include "openu5/sfx_synth.h"
#include "openu5/world.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
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
AudioPackInfo g_real{};
constexpr int64_t kClockStartUs = 5'000'000;

/** The device's policy without I2S: every accepted cue goes to an SfxPlayer the test pulls PCM from. */
struct Synth final : AudioBackend {
    SfxPlayer player;
    uint16_t gain = 0;
    struct Sub {
        SfxId id;
        int64_t us;
    };
    std::vector<Sub> subs;
    bool play_sfx(const SfxRequest &r) override {
        if (!sfx_supported(r.id)) return false;
        subs.push_back({r.id, openu5_host_virtual_clock_us()});
        player.submit(r);
        return true;
    }
    void stop_sfx() override { player.flush(); }
    bool start_music(MusicSong, uint16_t) override { return false; }
    void stop_music() override {}
    void set_gain(AudioChannel c, uint16_t g) override {
        if (c == AudioChannel::Sfx) gain = g;
    }
    /** Render `us` of output (16 kHz); the number of non-zero samples. */
    size_t pull_us(int64_t us) {
        std::vector<int16_t> b(size_t(us * 16 / 1000));
        if (b.empty()) return 0;
        player.render(b.data(), b.size(), gain);
        size_t n = 0;
        for (auto v : b) n += v != 0;
        return n;
    }
    size_t count_after(SfxId id, int64_t us) const {
        return size_t(std::count_if(subs.begin(), subs.end(), [&](const Sub &s) { return s.id == id && s.us > us; }));
    }
    std::vector<int64_t> times_after(SfxId id, int64_t us) const {
        std::vector<int64_t> out;
        for (const auto &s : subs)
            if (s.id == id && s.us > us) out.push_back(s.us);
        return out;
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Synth synth;
    size_t sounding = 0; // non-zero samples the player rendered, in step with the virtual clock
    int64_t pulled_to = 0;
    Run(int seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = true;
        board.initialize_display();
        openu5_host_virtual_clock_us() = kClockStartUs;
        idle.attach(bus::idle_passes()); // as main.cpp wires it
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
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
            m.level = 3;
            m.current_hp = 200;
            m.max_hp = 200;
            m.strength = m.dexterity = m.intelligence = 20;
        }
        g.rng.seed(uint32_t(seed));
        rt->configure_audio(g_real, &synth);
        rt->render(board, true);
        pulled_to = now();
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    /** The audio task keeps up with the game thread: render what the clock has passed. */
    void pump_audio() {
        sounding += synth.pull_us(now() - pulled_to);
        pulled_to = now();
    }
    /** main.cpp's pass: render, then the idle wait (nothing queued) and the guard. */
    void pass() {
        rt->render(board);
        pump_audio();
        if (rt->loop_wait_ticks()) bus::idle_wait_one_tick();
        idle.enforce();
        pump_audio();
    }
    void run(int64_t ms) {
        const int64_t end = now() + ms * 1000;
        while (now() < end) {
            openu5_host_virtual_clock_us() += 5000;
            pass();
        }
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        pump_audio();
        tdeck::RawInputEvent e{};
        e.kind = k;
        e.timestamp_us = now();
        rt->handle(e);
        pass();
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    bool teleport(uint8_t location, int x, int y) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::SmallMap;
        r.location = location;
        r.floor = 0;
        r.x = x;
        r.y = y;
        const bool ok = apply_debug_teleport(rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
        rt->render(board, true);
        pump_audio();
        return ok;
    }
    std::string clock() const {
        char b[16];
        std::snprintf(b, sizeof b, "%02d:%02d", int(rt->game().time.hour), int(rt->game().time.minute));
        return b;
    }
    void set_time(int hour, int minute) {
        rt->game().time.hour = uint8_t(hour);
        rt->game().time.minute = uint8_t(minute);
    }
};

/** A tile of `cls` in a town, and a passable cell beside it whose window's nearest sounding object is it. */
struct Spot {
    int location = -1, x = 0, y = 0;
};
Spot find_spot(uint8_t cls) {
    for (int loc = 1; loc <= 32; ++loc) {
        const auto m = get_active_map(pack->world, MapId{LocationId(loc), 0});
        if (m.error != Error::None || !m.value.tiles) continue;
        const auto &map = m.value;
        for (int y = 1; y < 31; ++y)
            for (int x = 1; x < 31; ++x) {
                if (ambient_tile_class(map.tile_at(x, y)) != cls) continue;
                const int nx[] = {x, x, x + 1, x - 1}, ny[] = {y + 1, y - 1, y, y};
                for (int k = 0; k < 4; ++k) {
                    const int t = map.tile_at(nx[k], ny[k]);
                    if (t < 0 || !is_passable(t, TransportMode::Foot).value) continue;
                    AmbientWindow w{};
                    for (int j = 0; j < 11; ++j)
                        for (int i = 0; i < 11; ++i) w.tiles[j * 11 + i] = int16_t(map.tile_at(nx[k] + i - 5, ny[k] + j - 5));
                    if (ambient_nearest_class(w) != cls) continue;
                    return {loc, nx[k], ny[k]};
                }
            }
    }
    return {};
}

/** 55 ms ambient ticks between the first and last cue with no cue of their own (and ticks with two). */
size_t missed_ticks(const std::vector<int64_t> &t) {
    size_t missed = 0;
    for (size_t i = 1; i < t.size(); ++i) {
        const int64_t a = t[i - 1] / 55000, b = t[i] / 55000;
        missed += b == a ? 1 : size_t(b - a - 1);
    }
    return missed;
}
std::string n(size_t v) { return std::to_string(v); }
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    g_real = tdeck::load_audio_pack_info(argv[2]);

    const Spot fountain = find_spot(3), clock = find_spot(1);
    std::printf("  fountain spot: location %d (%d,%d); clock spot: location %d (%d,%d)\n", fountain.location, fountain.x,
                fountain.y, clock.location, clock.x, clock.y);
    check(fountain.location > 0 && clock.location > 0, "S0 the game's own maps hold a reachable fountain and clock");

    // ---- K: the clock ---------------------------------------------------------
    {
        Run h(5);
        const bool at = h.teleport(uint8_t(clock.location), clock.x, clock.y);
        h.set_time(12, 30); // the runtime recorded 12:55: same hour (A3-03's minute key re-armed here)
        h.run(3500);         // longer than twelve strikes (12 x 220 ms), so K1 is a control under either key
        const auto t0 = h.now();
        h.run(1760);
        const size_t ticks = h.synth.count_after(SfxId::AmbientClockTick, t0),
                     tocks = h.synth.count_after(SfxId::AmbientClockTock, t0),
                     strikes0 = h.synth.count_after(SfxId::AmbientClockChime, t0);
        check(at && ticks == 4 && tocks == 4 && strikes0 == 0,
              "K1 standing by a real clock at 12:30: tick / tock, one of each per 8 ambient ticks (" + n(ticks) + " / " +
                  n(tocks) + " in 1.76 s, " + n(strikes0) + " strikes)");

        // K2: steps inside the hour -- the hardware report's "beep after every move".
        const auto t1 = h.now();
        const std::string before = h.clock();
        for (int i = 0; i < 3; ++i) {
            h.down();
            h.run(700);
            h.up();
            h.run(700);
        }
        const std::string after = h.clock();
        const size_t strikes = h.synth.count_after(SfxId::AmbientClockChime, t1);
        const bool minutes_moved = h.rt->game().time.hour == 12 && h.rt->game().time.minute > 30;
        check(minutes_moved && strikes == 0 && h.rt->ambient().chimes() == 0,
              "K2 six steps inside the hour (" + before + " -> " + after + ") strike nothing: advance_clock re-arms "
              "[0x5884] only when the hour moves (0x514a je 0x5186) -- " + n(strikes) + " strikes");

        // K3: and the tick / tock is intact after them.
        const auto t2 = h.now();
        h.run(1760);
        const size_t ticks2 = h.synth.count_after(SfxId::AmbientClockTick, t2),
                     tocks2 = h.synth.count_after(SfxId::AmbientClockTock, t2);
        check(ticks2 == 4 && tocks2 == 4 && h.synth.count_after(SfxId::AmbientClockChime, t2) == 0,
              "K3 after the steps the clock goes on ticking and tocking (" + n(ticks2) + " / " + n(tocks2) +
                  " in 1.76 s), no strike");

        // K4: the step that crosses the hour strikes the new one (13:00 -> one).
        h.set_time(12, 59);
        h.run(3500); // the same hour; under A3-03's minute key this edit re-armed, so wait it out
        const bool quiet = h.rt->ambient().chimes() == 0;
        const auto t3 = h.now();
        h.down();
        const bool crossed = h.rt->game().time.hour == 13;
        h.run(2200 + 440);
        const auto strike_times = h.synth.times_after(SfxId::AmbientClockChime, t3);
        const size_t tick_after = h.synth.count_after(SfxId::AmbientClockTick, strike_times.empty() ? t3 : strike_times.back()) +
                                  h.synth.count_after(SfxId::AmbientClockTock, strike_times.empty() ? t3 : strike_times.back());
        check(quiet && crossed && strike_times.size() == 1 && tick_after > 0 && h.rt->ambient().chimes() == 0,
              "K4 the step from 12:59 to " + h.clock() + " strikes the new hour once (13 = one o'clock on the dial): " +
                  n(strike_times.size()) + " strike(s), then tick / tock again");

        // K5: further steps inside the new hour re-arm nothing.
        const auto t4 = h.now();
        h.up();
        h.run(700);
        h.down();
        h.run(700);
        h.up();
        h.run(1760);
        check(h.rt->game().time.hour == 13 && h.synth.count_after(SfxId::AmbientClockChime, t4) == 0 &&
                  h.synth.count_after(SfxId::AmbientClockTick, t4) > 0,
              "K5 three more steps inside 13:xx (" + h.clock() + ") strike nothing; the clock keeps ticking");
    }
    {
        // K6: noon strikes twelve. The test's own 12:55 -> 11:59 edit is an hour
        // change too, so the counter is left to run down first.
        Run h(6);
        const bool at = h.teleport(uint8_t(clock.location), clock.x, clock.y);
        h.set_time(11, 59);
        h.run(3500);
        const bool quiet = h.rt->ambient().chimes() == 0;
        const auto t0 = h.now();
        h.down();
        const bool noon = h.rt->game().time.hour == 12;
        h.run(2200 + 440 * 12);
        const size_t strikes = h.synth.count_after(SfxId::AmbientClockChime, t0);
        check(at && quiet && noon && strikes == 12 && h.rt->ambient().chimes() == 0,
              "K6 the step from 11:59 to noon strikes twelve (" + n(strikes) + "), then tick / tock");
    }

    // ---- F: the fountain, standing still -------------------------------------------
    {
        Run h(3);
        const bool at = h.teleport(uint8_t(fountain.location), fountain.x, fountain.y);
        h.run(300);
        const std::string before = h.clock();
        const auto x0 = h.rt->game().position.xy;
        const auto t0 = h.now();
        const size_t sounding0 = h.sounding;
        const auto skipped0 = h.synth.player.stats().ambient_skipped;
        h.run(25000); // 25 s, no input at all
        const auto burbles = h.synth.times_after(SfxId::AmbientFountain, t0);
        const size_t missed = missed_ticks(burbles);
        const size_t expected = size_t(25000 / 55);
        std::printf("  standing 25 s by the fountain (%s -> %s): %zu burbles, %zu ticks missed, %zu samples sounding, "
                    "%u ambient skipped\n",
                    before.c_str(), h.clock().c_str(), burbles.size(), missed, h.sounding - sounding0,
                    unsigned(h.synth.player.stats().ambient_skipped - skipped0));
        const bool still = h.rt->game().position.xy.x == x0.x && h.rt->game().position.xy.y == x0.y;
        check(at && still && burbles.size() + 1 >= expected && burbles.size() <= expected + 1 && missed == 0,
              "F1 standing still by a real fountain for 25 s, no input: one burble in every 55 ms tick throughout (" +
                  n(burbles.size()) + ", " + n(missed) + " ticks missed) -- the sound never needs a move");
        check(h.synth.player.stats().ambient_skipped == skipped0 && h.sounding - sounding0 > burbles.size() * 20 &&
                  h.synth.player.idle(),
              "F2 ... and every one is heard: nothing else plays, none skipped, each a short click that ends");
        // F3: a step, then standing again: the burble resumes and stays.
        h.down();
        h.up();
        const auto t1 = h.now();
        h.run(5000);
        const auto after = h.synth.times_after(SfxId::AmbientFountain, t1);
        check(after.size() + 1 >= size_t(5000 / 55) && missed_ticks(after) == 0,
              "F3 after two steps it resumes and sustains for the next 5 s (" + n(after.size()) + " burbles)");
    }

    std::printf("A3-HF2 ambient parity: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
