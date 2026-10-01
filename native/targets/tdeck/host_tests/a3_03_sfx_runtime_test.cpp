// Alpha 3 A3-03 -- the remaining sound effects through the REAL AlphaRuntime:
// raw keys and the core's own events in, the PC-speaker synthesizer out, on
// the host esp_timer shim's virtual clock.
//
//   a3_03_sfx_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin>
//
// The backend is the device's policy without I2S (as in a3_02_sfx_runtime):
// it declines a cue compile_sfx has no program for, and hands the rest to an
// openu5::SfxPlayer the test pulls PCM from.
//
//   F  a real fountain: one burble per 55 ms tick, short, stops out of range, muted at 0 %
//   C  a real clock: tick / tock, and the hour struck after a step crosses the hour
//   V  combat victory through the real engine (trolls, enemy 41, and another
//      arena): exactly one fanfare, none for a lost battle or an escape, and
//      exploration returns at the same instant with or without audio
//   Q  the quake (harpsichord melody): one rumble per shake, visuals unchanged
//   S  shrine and ritual cues in the binary's order, presentation unchanged
//   R  the Refuge: thunder x2, slumber, revival, paced exactly as without audio
//   W  world / shop / intro derivations at presentation time
//   M  Alt+L, System Menu Load, title Continue, Return to Title, System Menu,
//      dungeon, Ending: no ambience or fanfare survives into a new context
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/ambient_sfx.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/blackthorn_scene.h"
#include "openu5/debug_map_picker.h"
#include "openu5/frontend_settings.h"
#include "openu5/quest_world.h"
#include "openu5/scene_timing.h"
#include "openu5/sfx_synth.h"
#include "openu5/world.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck
void batch37_reset_screen();
size_t batch37_frame_count();
int64_t batch37_frame_time_us(size_t);

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}
const tdeck::AlphaResourceOwners *pack = nullptr;
size_t g_dungeon_count = 0;
AudioPackInfo g_real{};
constexpr int64_t kClockStartUs = 5'000'000;

struct Synth final : AudioBackend {
    SfxPlayer player;
    uint16_t gain = 0;
    bool broken = false;
    struct Sub {
        SfxId id;
        int32_t param;
        int64_t us;
    };
    std::vector<Sub> subs;
    int stops = 0;
    bool play_sfx(const SfxRequest &r) override {
        if (broken || !sfx_supported(r.id)) return false;
        subs.push_back({r.id, r.param, openu5_host_virtual_clock_us()});
        player.submit(r);
        return true;
    }
    void stop_sfx() override {
        ++stops;
        player.flush();
    }
    bool start_music(MusicSong, uint16_t) override { return false; }
    void stop_music() override {}
    void set_gain(AudioChannel c, uint16_t g) override {
        if (c == AudioChannel::Sfx) gain = g;
    }
    size_t pull(int ms, std::vector<int16_t> *keep = nullptr) {
        std::vector<int16_t> b(size_t(ms) * 16);
        player.render(b.data(), b.size(), gain);
        size_t n = 0;
        for (auto v : b) n += v != 0;
        if (keep) keep->insert(keep->end(), b.begin(), b.end());
        return n;
    }
    size_t count(SfxId id) const {
        return size_t(std::count_if(subs.begin(), subs.end(), [&](const Sub &s) { return s.id == id; }));
    }
    size_t count_after(SfxId id, int64_t us) const {
        return size_t(std::count_if(subs.begin(), subs.end(), [&](const Sub &s) { return s.id == id && s.us > us; }));
    }
    size_t ambient_after(int64_t us) const {
        return size_t(std::count_if(subs.begin(), subs.end(),
                                    [&](const Sub &s) { return s.us > us && sfx_class(s.id) == SfxClass::Ambient; }));
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    Run(int seed, bool paced) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        batch37_reset_screen();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.dungeons = pack->dungeons;
        f.dungeon_count = g_dungeon_count;
        f.enemy_defs = pack->combat_enemy_views;
        f.enemy_def_count = pack->combat_enemy_count;
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
        g.gold = 400;
        g.karma = 60;
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
        rt->render(board, true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
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
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    // Alpha 4 A4-SAVE2: Save Game opens Slots 1-3 on the journey's slot; Enter
    // saves there, and an occupied slot asks "Overwrite Slot N?" (No first).
    void menu_save() {
        key('m', true); down(); key('\r'); key('\r');
        if (std::strncmp(rt->system_menu_view().title, "Overwrite", 9) == 0) { down(); key('\r'); }
        if (rt->system_menu_open()) key('m', true);   // A4-UI3: a successful save returns to the game; only a failed one leaves the menu open.
    }
    void menu_load() { key('m', true); down(); down(); key('\r'); key('\r'); }
    void return_to_title() { key('m', true); up(); key('\r'); }
    void title_continue() { return_to_title(); key('x'); key('j'); key('\r'); }
    void present(const GameEvent &e) {
        auto &ctx = rt->command_context_for_test();
        ctx.events.emit(ctx.events.context, e);
    }
    void cue(const char *name, int32_t note = 0) {
        GameEvent e{};
        e.kind = GameEventKind::Sfx;
        e.text = name;
        e.note = note;
        present(e);
    }
    void message(const char *text) {
        GameEvent e{};
        e.kind = GameEventKind::Message;
        e.text = text;
        present(e);
    }
    template <class F> void run(int64_t ms, F each) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
            each();
        }
    }
    void run(int64_t ms) { run(ms, [] {}); }
    std::string transcript() const {
        std::string out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
        return out;
    }
    bool saw(const char *needle) const { return transcript().find(needle) != std::string::npos; }
    std::string state() {
        auto &g = rt->game();
        std::ostringstream s;
        s << int(g.position.map.location) << ',' << int(g.position.map.floor) << ',' << g.position.xy.x << ','
          << g.position.xy.y << ',' << int(g.time.hour) << ':' << int(g.time.minute) << ',' << g.food << ',' << g.gold
          << ',' << int(g.party.characters[0].current_hp) << ',' << int(g.party.characters[0].status)
          << ",rng" << g.rng.get_seed() << ",cmds" << rt->routed_command_count() << ",mode" << int(rt->ui()->mode()) << '\n'
          << transcript();
        return s.str();
    }
    bool teleport(DebugDestinationKind kind, uint8_t location, int16_t floor, int x, int y) {
        DebugTeleportRequest r{};
        r.kind = kind;
        r.location = location;
        r.floor = floor;
        r.x = x;
        r.y = y;
        const bool ok = apply_debug_teleport(rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
        rt->render(board, true);
        return ok;
    }
    UiMode mode() const { return rt->ui()->mode(); }
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

enum class Setup { None, Synth, Muted, Broken, Stalled, Racing };
const Setup kSetups[] = {Setup::None, Setup::Synth, Setup::Muted, Setup::Broken, Setup::Stalled, Setup::Racing};
const char *setup_name(Setup s) {
    switch (s) {
    case Setup::None: return "none";
    case Setup::Synth: return "synth";
    case Setup::Muted: return "muted";
    case Setup::Broken: return "broken";
    case Setup::Stalled: return "stalled";
    case Setup::Racing: return "racing";
    }
    return "?";
}
void muted_settings(bool on) {
    tdeck::a3_host_settings_enabled() = on;
    tdeck::a3_host_settings_text().clear();
    if (on) {
        FrontendSettings m{};
        m.sound_volume = 0;
        encode_settings(m, tdeck::a3_host_settings_text());
    }
}
struct Timeline {
    std::vector<std::string> samples;
    std::vector<int64_t> frames;
    std::string state;
    std::unique_ptr<Synth> synth = std::make_unique<Synth>();
    int64_t done_us = -1;
};
/** A fresh run under one audio setup. */
std::unique_ptr<Run> setup_run(Timeline &out, Setup s, int seed, bool paced) {
    muted_settings(s == Setup::Muted);
    auto h = std::make_unique<Run>(seed, paced);
    muted_settings(false);
    if (s != Setup::None) {
        out.synth->broken = s == Setup::Broken;
        h->rt->configure_audio(g_real, out.synth.get());
    }
    return h;
}
void pump_audio(Timeline &out, Setup s) {
    if (s == Setup::Synth || s == Setup::Muted || s == Setup::Broken) out.synth->pull(5);
    if (s == Setup::Racing) out.synth->pull(500);
}
void finish(Run &h, Timeline &out) {
    for (size_t i = 0; i < batch37_frame_count(); ++i) out.frames.push_back(batch37_frame_time_us(i));
    out.state = h.state();
}
bool same_timelines(const std::vector<std::unique_ptr<Timeline>> &t) {
    bool same = true;
    for (size_t i = 1; i < t.size(); ++i) {
        const bool s = t[i]->samples == t[0]->samples && t[i]->frames == t[0]->frames && t[i]->state == t[0]->state &&
                       t[i]->done_us == t[0]->done_us;
        if (!s) {
            std::printf("  timeline differs: %s vs none (samples %zu/%zu frames %zu/%zu done %lld/%lld)\n",
                        setup_name(kSetups[i]), t[i]->samples.size(), t[0]->samples.size(), t[i]->frames.size(),
                        t[0]->frames.size(), (long long)t[i]->done_us, (long long)t[0]->done_us);
            for (size_t k = 0; k < std::min(t[i]->samples.size(), t[0]->samples.size()); ++k)
                if (t[i]->samples[k] != t[0]->samples[k]) {
                    std::printf("    first diff at %zu: '%s' vs '%s'\n", k, t[i]->samples[k].c_str(), t[0]->samples[k].c_str());
                    break;
                }
        }
        same = same && s;
    }
    return same;
}

// ---- combat -----------------------------------------------------------------
/** The troll bridge's own entry: start_encounter_combat(enemy 41), as the toll refusal does (commands.cpp). */
bool start_arena(Run &h, int32_t enemy) {
    auto &ctx = h.rt->command_context_for_test();
    if (!ctx.outdoor || !ctx.outdoor->combat || !ctx.outdoor->resources) return false;
    auto &arena = *ctx.outdoor->combat;
    auto resources = *ctx.outdoor->resources;
    resources.remove_enemy = nullptr;
    const auto status = start_encounter_combat(ctx, arena.combat, resources, enemy, 5, -1, CombatDirection::South, false);
    if (status != CombatResult::Ok) return false;
    ctx.combat_context = &arena;
    h.rt->render(h.board, true);
    return ctx.combat && arena.combat.initialized;
}
int enemies_alive(const CombatState &cs) {
    int n = 0;
    for (int i = 0; i < cs.count; ++i)
        if (cs.actors[i].enemy && (cs.actors[i].status == CombatStatus::Active || cs.actors[i].status == CombatStatus::Sleeping)) ++n;
    return n;
}
/** Every monster falls (staged, as batch tests stage positions); the next beat's advance() latches VICTORY. */
void fell_all(Run &h) {
    auto &cs = h.rt->combat_state_for_test();
    for (int i = 0; i < cs.count; ++i)
        if (cs.actors[i].enemy) {
            cs.actors[i].status = CombatStatus::Dead;
            cs.actors[i].hp = 0;
        }
}
/** Drive the arena by trackball steps until it closes (or a budget runs out). */
void drive_out(Run &h, int budget = 40) {
    for (int i = 0; i < budget && h.rt->command_context().combat; ++i) {
        openu5_host_virtual_clock_us() += 600000;
        h.rt->render(h.board);
        h.ball(RawInputKind::TrackballUp);
    }
    h.run(300);
}

std::unique_ptr<Timeline> troll_timeline(Setup s) {
    auto out = std::make_unique<Timeline>();
    auto h = setup_run(*out, s, 41, false);
    if (!start_arena(*h, 41)) return out;
    fell_all(*h);
    for (int64_t t = 0; t < 30000; t += 5) {
        if (t % 600 == 0 && h->rt->command_context().combat) h->ball(RawInputKind::TrackballUp);
        openu5_host_virtual_clock_us() += 5000;
        h->rt->render(h->board);
        pump_audio(*out, s);
        if (out->done_us < 0 && !h->rt->command_context().combat && h->mode() == UiMode::Exploration) out->done_us = h->now();
        std::ostringstream line;
        line << h->now() << ' ' << h->rt->command_context().combat << ' ' << int(h->mode()) << ' ' << batch37_frame_count();
        out->samples.push_back(line.str());
    }
    finish(*h, *out);
    return out;
}

bool silent_after_release(Synth &s, int ms = 500) {
    std::vector<int16_t> pcm;
    s.pull(ms, &pcm);
    size_t last = 0;
    for (size_t i = 0; i < pcm.size(); ++i)
        if (pcm[i]) last = i + 1;
    return last <= SfxPlayer::kReleaseFrames && s.player.idle();
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    g_dungeon_count = report.dungeon_count;
    g_real = tdeck::load_audio_pack_info(argv[2]);

    const Spot fountain = find_spot(3), clock = find_spot(1);
    std::printf("  fountain spot: location %d (%d,%d); clock spot: location %d (%d,%d)\n", fountain.location, fountain.x,
                fountain.y, clock.location, clock.x, clock.y);
    check(fountain.location > 0 && clock.location > 0, "F0 the game's own maps hold a reachable fountain and clock");

    // ---- F: the fountain ------------------------------------------------------
    {
        Synth synth;
        Run h(3, false);
        h.rt->configure_audio(g_real, &synth);
        const bool at = h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        const auto t0 = h.now();
        size_t sounding = 0;
        h.run(1100, [&] { sounding += synth.pull(5); });
        const size_t burbles = synth.count_after(SfxId::AmbientFountain, t0);
        std::vector<int64_t> at_us;
        for (const auto &s : synth.subs)
            if (s.id == SfxId::AmbientFountain && s.us > t0) at_us.push_back(s.us);
        bool cadence = at_us.size() > 2;
        for (size_t i = 1; i < at_us.size() && cadence; ++i) cadence = at_us[i] - at_us[i - 1] == 55000;
        std::printf("  by the fountain: %zu burbles in 1.1 s, %zu samples sounding\n", burbles, sounding);
        check(at && burbles >= 19 && burbles <= 21, "F1 standing by a real fountain: one burble per 55 ms tick (0x266c + 0x4102)");
        check(cadence, "F2 the cadence is the BIOS tick exactly: 55 ms apart on the virtual clock");
        check(sounding > burbles * 20 && sounding < burbles * 40 && synth.player.idle() && synth.pull(200) == 0,
              "F3 each burble is a ~1.7 ms click and ends: no continuous or stuck tone");
        // out of range: the open field of the overworld
        h.teleport(DebugDestinationKind::Britannia, 0, 0, 80, 80);
        const auto t1 = h.now();
        h.run(1100, [&] { synth.pull(5); });
        check(synth.ambient_after(t1) == 0 && h.rt->ambient_ticks() > 20,
              "F4 walking out of range stops it (the ticker still runs; nothing sounding is near)");
        // entering again
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        const auto t2 = h.now();
        h.run(550, [&] { synth.pull(5); });
        check(synth.count_after(SfxId::AmbientFountain, t2) >= 9, "F4 and coming back starts it again");
        // walking by it: steps preempt ambience, nothing piles up
        const auto before_overflow = synth.player.stats().overflowed;
        for (int i = 0; i < 4; ++i) {
            h.down();
            h.up();
        }
        bool ambient_waits = false;
        h.run(2000, [&] {
            synth.pull(5);
            for (size_t i = 0; i < synth.player.pending(); ++i)
                ambient_waits = ambient_waits || sfx_class(synth.player.pending_at(i).id) == SfxClass::Ambient;
        });
        const auto skipped = synth.player.stats().ambient_skipped;
        check(synth.player.stats().overflowed == before_overflow && !ambient_waits && skipped > 0,
              "F5 walking by the fountain: ticks during a step are skipped, never queued; nothing overflows");
        muted_settings(true);
        Synth quiet;
        Run m(3, false);
        muted_settings(false);
        m.rt->configure_audio(g_real, &quiet);
        m.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        m.run(1100, [&] { quiet.pull(5); });
        check(quiet.subs.empty() && quiet.pull(500) == 0 && m.rt->audio().stats().sfx_muted >= 19,
              "F6 at SFX Volume 0 the fountain is exact silence (muted in the service, never submitted)");
    }

    {
        // F7 in an arena g_location >= 0x80: 0x266c redraws and 0x4102 scans the
        // arena's own 11 x 11 cells around (5,5) (0x4114), not the world outside.
        Synth synth;
        Run h(71, false);
        h.rt->configure_audio(g_real, &synth);
        const bool live = start_arena(h, 41);
        h.rt->combat_state_for_test().map.tiles[3 * 11 + 3] = 0xd9; // a fountain cell, staged
        const auto t0 = h.now();
        h.run(550, [&] { synth.pull(5); });
        check(live && synth.count_after(SfxId::AmbientFountain, t0) >= 9,
              "F7 a fountain cell in an arena burbles: the scan reads the arena, centre (5,5)");
    }

    // ---- C: the clock -------------------------------------------------------------
    {
        Synth synth;
        Run h(5, false);
        h.rt->configure_audio(g_real, &synth);
        const bool at = h.teleport(DebugDestinationKind::SmallMap, uint8_t(clock.location), 0, clock.x, clock.y);
        h.run(1100, [&] { synth.pull(5); }); // settle: a new world strikes nothing
        const auto t0 = h.now();
        h.run(1760, [&] { synth.pull(5); });
        const size_t ticks = synth.count_after(SfxId::AmbientClockTick, t0), tocks = synth.count_after(SfxId::AmbientClockTock, t0);
        std::printf("  clock: %zu ticks, %zu tocks in 1.76 s\n", ticks, tocks);
        check(at && ticks == 4 && tocks == 4 && synth.count_after(SfxId::AmbientClockChime, t0) == 0,
              "C1 a real clock ticks and tocks, one of each per 8 ticks (440 ms)");
        // a step across the hour: advance_clock re-arms [0x5884] only when the hour
        // moves (0x514a je 0x5186 -- A3-HF2; A3-03 re-armed on every minute, so
        // this row once struck on the 12:55 step), and the clock strikes the new hour
        h.rt->game().time.minute = 59;
        h.down();
        h.up();
        const int hour12 = ambient_chime_hour(uint8_t(h.rt->game().time.hour));
        const auto t1 = h.now();
        h.run(2200 + 440 * hour12, [&] { synth.pull(5); });
        const size_t chimes = synth.count_after(SfxId::AmbientClockChime, t1);
        std::printf("  after the clock moved (hour %d): %zu chimes\n", int(h.rt->game().time.hour), chimes);
        check(h.rt->game().time.hour == 13 && chimes >= size_t(hour12) && chimes <= size_t(hour12) + 1 &&
                  h.rt->ambient().chimes() == 0,
              "C2 after a step crosses the hour it strikes the new 12-hour hour, then goes back to tick / tock");
    }

    // ---- V: combat victory -------------------------------------------------------
    {
        Synth synth;
        Run h(41, false);
        h.rt->configure_audio(g_real, &synth);
        const bool live = start_arena(h, 41);
        const auto &cs = h.rt->combat_state();
        bool trolls = cs.count > 0;
        for (int i = 0; i < cs.count; ++i)
            if (cs.actors[i].enemy) trolls = trolls && cs.actors[i].enemy->index == 41;
        check(live && trolls && h.mode() == UiMode::Combat, "V0 the troll arena (enemy 41, the toll refusal's entry) is live");
        fell_all(h);
        drive_out(h);
        std::printf("  troll arena: %zu fanfare(s)\n", synth.count(SfxId::VictoryFanfare));
        check(!h.rt->command_context().combat && h.saw("VICTORY!") && synth.count(SfxId::VictoryFanfare) == 1,
              "V1 winning the troll fight plays the victory fanfare (COMBAT 0x0d02) exactly once");
        std::vector<int16_t> pcm;
        const size_t sounding = synth.pull(2500, &pcm);
        check(sounding > size_t(1.9 * 16000) && synth.player.idle(), "V1 and it is heard in full, 2.09 s, then silence");
        h.run(1000, [&] { synth.pull(5); });
        check(synth.count(SfxId::VictoryFanfare) == 1 && h.saw("VICTORY!"), "V2 no replay on redraw or on the Ended line");
    }
    {
        Synth synth;
        Run h(9, false);
        h.rt->configure_audio(g_real, &synth);
        const bool live = start_arena(h, 5); // an ordinary arena
        fell_all(h);
        drive_out(h);
        check(live && synth.count(SfxId::VictoryFanfare) == 1, "V3 an ordinary combat victory plays it once too");
    }
    {
        // a lost battle: the party falls instead
        Synth synth;
        Run h(11, false);
        h.rt->configure_audio(g_real, &synth);
        start_arena(h, 41);
        auto &cs = h.rt->combat_state_for_test();
        for (int i = 0; i < cs.count; ++i)
            if (!cs.actors[i].enemy) cs.actors[i].status = CombatStatus::Dead, cs.actors[i].hp = 0;
        for (int i = 0; i < 2; ++i) h.rt->game().party.characters[i].status = 'D', h.rt->game().party.characters[i].current_hp = 0;
        drive_out(h, 10);
        check(synth.count(SfxId::VictoryFanfare) == 0, "V4 a lost battle is silent (0x0cda BATTLE IS LOST! calls no fanfare)");
    }
    {
        // an escape: every member walks off the north edge
        Synth synth;
        Run h(13, false);
        h.rt->configure_audio(g_real, &synth);
        start_arena(h, 41);
        auto &cs = h.rt->combat_state_for_test();
        for (int k = 0; k < 40 && h.rt->command_context().combat; ++k) {
            for (int i = 0; i < cs.count; ++i)
                if (!cs.actors[i].enemy) cs.actors[i].position.y = 0;
            openu5_host_virtual_clock_us() += 600000;
            h.rt->render(h.board);
            h.ball(RawInputKind::TrackballUp);
        }
        h.run(300);
        check(synth.count(SfxId::CombatEscape) >= 1 && synth.count(SfxId::VictoryFanfare) == 0,
              "V4 escaping plays the escape glide (SJOG 0x1c37) and no fanfare");
    }
    {
        std::vector<std::unique_ptr<Timeline>> t;
        for (auto s : kSetups) t.push_back(troll_timeline(s));
        std::printf("  troll victory: exploration returns at %lld us (none) / %lld us (stalled); last sample '%s'\n",
                    (long long)t[0]->done_us, (long long)t[4]->done_us, t[0]->samples.empty() ? "" : t[0]->samples.back().c_str());
        check(t[0]->done_us > 0 && same_timelines(t),
              "V5 the fanfare never delays the return to exploration: identical timeline and state under all six setups");
        check(t[1]->synth->count(SfxId::VictoryFanfare) == 1 && t[2]->synth->subs.empty(),
              "V5 (synth: one fanfare; muted: nothing submitted)");
    }

    // ---- Q: the quake (the harpsichord melody on LB castle floor 2) ------------------
    auto quake_timeline = [](Setup s) {
        auto out = std::make_unique<Timeline>();
        auto h = setup_run(*out, s, 17, false);
        h->teleport(DebugDestinationKind::SmallMap, 17, 2, 17, 17);
        for (int d : {6, 7, 8, 9, 8, 7, 8, 7, 6, 7, 6, 5, 3}) h->key(uint8_t('0' + d));
        for (int64_t t = 0; t < 3000; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            h->rt->render(h->board);
            pump_audio(*out, s);
            std::ostringstream line;
            line << h->now() << ' ' << batch37_frame_count();
            out->samples.push_back(line.str());
        }
        finish(*h, *out);
        return out;
    };
    {
        std::vector<std::unique_ptr<Timeline>> t;
        for (auto s : kSetups) t.push_back(quake_timeline(s));
        const auto &synth = *t[1]->synth;
        std::printf("  harpsichord melody: %zu notes, %zu quake rumble(s)\n", synth.count(SfxId::InstrumentNote),
                    synth.count(SfxId::Quake));
        check(synth.count(SfxId::InstrumentNote) == 13 && synth.count(SfxId::Quake) == 1,
              "Q1 the melody's quake (TOWN 0x0ea3 -> 0x3072) plays one rumble, after the 13 notes");
        check(same_timelines(t), "Q2 the quake's frames and the game state are identical under all six audio setups");
    }

    // ---- S: shrines and the shard ritual -------------------------------------------
    {
        Synth synth;
        Run h(19, false);
        h.rt->configure_audio(g_real, &synth);
        GameEvent inv{};
        inv.kind = GameEventKind::RitualInvert;
        GameEvent quake{};
        quake.kind = GameEventKind::Quake;
        // WELL DONE, as shrine.cpp emits it: invert, sweeps, shake, "quake"
        h.present(inv);
        h.cue("shrine-well-done");
        h.present(quake);
        h.cue("quake");
        std::vector<SfxId> got;
        for (const auto &s : synth.subs) got.push_back(s.id);
        check((got == std::vector<SfxId>{SfxId::ShrineWellDone, SfxId::Quake}),
              "S1 WELL DONE: the two 460-call loops, then ONE rumble (the shake carries it; the cue does not double it)");
        std::vector<int16_t> pcm;
        synth.pull(7000, &pcm);
        size_t first_rumble = 0;
        for (size_t i = size_t(5.2 * 16000); i < pcm.size(); ++i)
            if (synth.player.stats().started >= 2) { first_rumble = i; break; }
        check(synth.player.stats().started == 2 && synth.player.idle() && first_rumble > 0,
              "S1 in the binary's order: the sweeps (5.35 s) then the rumble, one after the other");
        Synth ritual;
        Run r(23, false);
        r.rt->configure_audio(g_real, &ritual);
        // the shard ritual, as quest_world.cpp emits it: 3 shakes, one "quake" cue
        r.cue("shard-sweep");
        for (int i = 0; i < 3; ++i) r.present(quake);
        r.cue("quake");
        r.cue("victory-fanfare");
        std::vector<SfxId> seq;
        for (const auto &s : ritual.subs) seq.push_back(s.id);
        check((seq == std::vector<SfxId>{SfxId::ShardSweep, SfxId::Quake, SfxId::Quake, SfxId::Quake, SfxId::VictoryFanfare}),
              "S2 the shard ritual: the sweep, THREE rumbles (CAST 0x169d/0x16a0/0x16a3), the fanfare (CAST 0x1759)");
        Synth donate;
        Run d(29, false);
        d.rt->configure_audio(g_real, &donate);
        d.cue("shrine-donation");
        d.cue("shrine-ordained");
        check(donate.count(SfxId::ShrineDonation) == 1 && donate.count(SfxId::ShrineOrdained) == 1,
              "S3 ALAKAZAM and ORDAINED reach the synthesizer as the shrine emits them");
    }
    {
        auto shrine_timeline = [](Setup s) {
            auto out = std::make_unique<Timeline>();
            auto h = setup_run(*out, s, 19, false);
            GameEvent inv{}, quake{};
            inv.kind = GameEventKind::RitualInvert;
            quake.kind = GameEventKind::Quake;
            h->present(inv);
            h->cue("shrine-well-done");
            h->present(quake);
            h->cue("quake");
            for (int64_t t = 0; t < 2000; t += 5) {
                openu5_host_virtual_clock_us() += 5000;
                h->rt->render(h->board);
                pump_audio(*out, s);
                std::ostringstream line;
                line << h->now() << ' ' << batch37_frame_count();
                out->samples.push_back(line.str());
            }
            finish(*h, *out);
            return out;
        };
        std::vector<std::unique_ptr<Timeline>> t;
        for (auto s : kSetups) t.push_back(shrine_timeline(s));
        check(same_timelines(t), "S4 the shrine's presentation (frames, shake, state) is identical with audio on, off, failing or stalled");
    }

    // ---- R: the Refuge (paced) -----------------------------------------------------
    auto refuge_timeline = [](Setup s) {
        auto out = std::make_unique<Timeline>();
        auto h = setup_run(*out, s, 31, true);
        auto &g = h->rt->game();
        for (int i = 0; i < 2; ++i) g.party.characters[i].status = 'D', g.party.characters[i].current_hp = 0;
        auto &ctx = h->rt->command_context_for_test();
        const auto status = check_refuge(ctx, ctx.events);
        for (int64_t t = 0; t < 30000 && h->rt->narrative_pacer().active(); t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            // A3-HF9 (H-185): the karma speech waits for a key (getkey 0x0b3e);
            // one Space, the frame it is shown, keeps every setup on one clock.
            if (h->rt->narrative_pacer().awaiting_key()) h->key(' ');
            h->rt->render(h->board);
            pump_audio(*out, s);
            std::ostringstream line;
            line << h->now() << ' ' << h->rt->narrative_pacer().active() << ' ' << batch37_frame_count();
            out->samples.push_back(line.str());
        }
        out->done_us = status == CommandStatus::Success ? h->now() : -1;
        finish(*h, *out);
        return out;
    };
    {
        std::vector<std::unique_ptr<Timeline>> t;
        for (auto s : kSetups) t.push_back(refuge_timeline(s));
        const auto &synth = *t[1]->synth;
        int64_t slumber = -1, revival = -1, thunder = -1;
        int32_t revived = -1;
        for (const auto &x : synth.subs) {
            if (x.id == SfxId::RefugeSlumber && slumber < 0) slumber = x.us;
            if (x.id == SfxId::RefugeThunder && thunder < 0) thunder = x.us;
            if (x.id == SfxId::RefugeRevival && revival < 0) revival = x.us, revived = x.param;
        }
        std::printf("  refuge: slumber %lld, thunder %lld, revival %lld (members %d), ended %lld us\n", (long long)slumber,
                    (long long)thunder, (long long)revival, int(revived), (long long)t[1]->done_us);
        check(t[1]->done_us > 0 && slumber > 0 && thunder > slumber && revival > thunder &&
                  synth.count(SfxId::RefugeThunder) == 2 && revived == 2,
              "R1 the Refuge's sounds in order: the slumber melody, two peals of thunder, one revival tone per member");
        check(same_timelines(t), "R2 the Refuge is paced exactly as without audio under all six setups (the pacer owns time)");
    }

    // ---- B: the Blackthorn sacrifice siren (paced) -----------------------------------
    auto sacrifice_timeline = [](Setup s) {
        auto out = std::make_unique<Timeline>();
        auto h = setup_run(*out, s, 21, true);
        static BlackthornSceneState state{};
        static BlackthornSceneScript script{};
        state = {};
        script = {};
        init_capture_scene(state, "AFM", 3);
        build_sacrifice_script(state, script);
        GameEvent e{};
        e.kind = GameEventKind::BlackthornScene;
        e.blackthorn_scene = &script;
        h->present(e);
        for (int64_t t = 0; t < 20000 && h->rt->blackthorn_pacer().active(); t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            h->rt->render(h->board);
            pump_audio(*out, s);
            std::ostringstream line;
            line << h->now() << ' ' << h->rt->blackthorn_pacer().released_steps() << ' '
                 << int(h->rt->blackthorn_pacer().state()) << ' ' << batch37_frame_count();
            out->samples.push_back(line.str());
        }
        out->done_us = h->rt->blackthorn_pacer().active() ? -1 : h->now();
        finish(*h, *out);
        return out;
    };
    {
        std::vector<std::unique_ptr<Timeline>> t;
        for (auto s : kSetups) t.push_back(sacrifice_timeline(s));
        const auto &synth = *t[1]->synth;
        int64_t siren = -1;
        for (const auto &x : synth.subs)
            if (x.id == SfxId::ShardSweep) siren = x.us;
        // the first beat released after the siren's
        int64_t next = -1;
        size_t at_cue = 0;
        for (const auto &line : t[1]->samples) {
            long long us = 0;
            size_t rel = 0;
            std::sscanf(line.c_str(), "%lld %zu", &us, &rel);
            if (us == siren) at_cue = rel;
            if (siren >= 0 && us > siren && at_cue && rel > at_cue) {
                next = us;
                break;
            }
        }
        std::printf("  sacrifice: siren at %lld us, next beat %lld us later, scene ends %lld us\n", (long long)siren,
                    (long long)(next - siren), (long long)t[1]->done_us);
        check(synth.count(SfxId::ShardSweep) == 1 && t[1]->done_us > 0 &&
                  next - siren >= int64_t(tone_sweep_ms(kBlackthornSirenSamples)) * 1000,
              "B1 the sacrifice siren (BLCKTHRN 0x03d0-0x040f) now sounds, and the scene holds it for the PACER's 7.1 s");
        check(same_timelines(t), "B2 the sacrifice is paced identically with audio on, muted, failing, stalled or racing");
    }

    // ---- W: world / shop / intro derivations -----------------------------------------
    {
        Synth synth;
        Run h(37, false);
        h.rt->configure_audio(g_real, &synth);
        h.message("COLLISION!");
        h.message("Docked!");
        h.message("\nWHIRLPOOL!\n");
        h.message("\nSomething was stolen!\n");
        h.message("Poof!");
        h.message("A TRAPDOOR!"); // not location 29: the fall to the floor below is mute
        check(synth.count(SfxId::ShipCollision) == 1 && synth.count(SfxId::ShipSinking) == 1 &&
                  synth.count(SfxId::TheftDetected) == 1 && synth.count(SfxId::WishGranted) == 0 &&
                  synth.count(SfxId::TrapdoorFall) == 0,
              "W1 world messages sound where the binary does, and only there (Docked!, the potion's Poof!, a plain trapdoor: mute)");
        h.teleport(DebugDestinationKind::SmallMap, 29, 0, 15, 15);
        h.message("A TRAPDOOR!");
        const bool trap = synth.count(SfxId::TrapdoorFall) == 1 && synth.subs.back().param == 2;
        check(trap, "W1 location 29's trapdoor (TOWN 0x0f96) plays the 28 s fall and a burst per member");
        ShopSession healer{};
        healer.type = ShopType::Healer;
        ShopResult done{true, "It is done."}, need{false, "Thou hast no need of this art!"};
        ShopEvent se{ShopEventKind::Result, &healer, &done, nullptr};
        GameEvent e{};
        e.kind = GameEventKind::Shop;
        e.shop = &se;
        h.present(e);
        se.result = &need;
        h.present(e);
        ShopSession smith{};
        smith.type = ShopType::Blacksmith;
        ShopResult sold{true, "Anything else?"}, other{true, "It is done."};
        se.session = &smith;
        se.result = &sold;
        h.present(e);
        se.result = &other; // the same words from any other counter: still not the healer
        h.present(e);
        ShopResult thanks{true, "Anything else?"};
        se.session = &healer;
        se.result = &thanks; // a healer result that is not the service
        h.present(e);
        check(synth.count(SfxId::ShopTransaction) == 1,
              "W2 the healer's jingle (SHOPPES 0x13b0) plays when the service is done, not on a refusal or another shop");
    }
    {
        Synth synth;
        Run h(43, false);
        h.rt->configure_audio(g_real, &synth);
        h.return_to_title();
        const auto t0 = h.now();
        h.run(90000, [&] { synth.pull(5); });
        std::printf("  title / attract, 90 s: thunder %zu, chime %zu, summon %zu\n", synth.count(SfxId::IntroThunder),
                    synth.count(SfxId::IntroChime), synth.count(SfxId::IntroSummon));
        bool pitches = false;
        for (const auto &x : synth.subs)
            if (x.id == SfxId::IntroChime && x.param == 4) pitches = true;
        check(synth.count(SfxId::IntroThunder) > 0 && synth.count(SfxId::IntroChime) > 0 && synth.count(SfxId::IntroSummon) > 0 &&
                  pitches && synth.ambient_after(t0) == 0,
              "W3 the attract demo sounds each of its thunder, chime (3000 / 2000) and summon; no ambience on the title");
    }

    {
        // W4 the runtime drops no supported cue: every one, presented as the core
        // presents a cue, reaches the backend -- except "quake", which its Quake
        // event sounds (one rumble per shake).
        Synth synth;
        Run h(61, false);
        h.rt->configure_audio(g_real, &synth);
        std::vector<std::string> dropped;
        for (size_t i = 1; i < kSfxIdCount; ++i) {
            const auto id = SfxId(i);
            if (!sfx_supported(id) || sfx_origin(id) == SfxOrigin::Diagnostic || id == SfxId::Quake) continue;
            const size_t before = synth.subs.size();
            h.cue(sfx_cue(id));
            if (synth.subs.size() != before + 1 || synth.subs.back().id != id) dropped.push_back(sfx_cue(id));
            synth.player.flush();
        }
        for (const auto &d : dropped) std::printf("  dropped by the runtime: %s\n", d.c_str());
        GameEvent shake{};
        shake.kind = GameEventKind::Quake;
        const size_t before = synth.subs.size();
        h.present(shake);
        h.cue("quake");
        check(dropped.empty() && synth.subs.size() == before + 1 && synth.subs.back().id == SfxId::Quake,
              "W4 every supported cue presented through the runtime reaches the synthesizer (quake: once, by its shake)");
    }

    // ---- M: load and mode safety -----------------------------------------------------
    {
        Synth synth;
        Run h(47, false);
        h.rt->configure_audio(g_real, &synth);
        h.menu_save(); // a save in the open field
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        h.run(550, [&] { synth.pull(5); });
        const bool burbling = synth.count(SfxId::AmbientFountain) >= 9;
        h.key('l', true);
        const auto t0 = h.now();
        const bool quiet = silent_after_release(synth, 5);
        h.run(1100, [&] { synth.pull(5); });
        check(burbling && quiet && synth.ambient_after(t0) == 0 && h.rt->game().position.map.location == 0,
              "M1 Alt+L while by the fountain: silent at once, and no burble follows into the loaded world");
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        h.run(300, [&] { synth.pull(5); });
        h.menu_load();
        const auto t1 = h.now();
        h.run(1100, [&] { synth.pull(5); });
        check(synth.ambient_after(t1) == 0, "M2 System Menu Load: the same");
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        h.key('m', true);
        const auto ticks = h.rt->ambient_ticks();
        const auto t2 = h.now();
        h.run(1100, [&] { synth.pull(5); });
        check(h.rt->ambient_ticks() == ticks && synth.ambient_after(t2) == 0, "M3 the System Menu is not the key wait: no ambience");
        h.key('m', true);
        h.run(300, [&] { synth.pull(5); });
        check(synth.ambient_after(t2) > 0, "M3 closing it: the fountain again");
        h.return_to_title();
        const auto t3 = h.now();
        const bool faded = silent_after_release(synth, 5);
        h.run(1100, [&] { synth.pull(5); });
        check(faded && synth.ambient_after(t3) == 0, "M4 Return to Title: silent, and no ambience on the title");
        h.key('x');
        h.key('j');
        h.key('\r');
        const auto t4 = h.now();
        h.run(1100, [&] { synth.pull(5); });
        check(synth.ambient_after(t4) == 0 && h.rt->game().position.map.location == 0,
              "M5 title Continue into the saved field: nothing from the fountain survives");
    }
    {
        // A load starts the ambient counters over: strikes armed in the old world
        // do not ring in the loaded one.
        Synth synth;
        Run h(67, false);
        h.rt->configure_audio(g_real, &synth);
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(clock.location), 0, clock.x, clock.y);
        // A3-HF2: only a step across the hour arms a strike. 11:59 -> noon arms
        // twelve, which outlast the probe below (one o'clock's single strike rings
        // before it). The 12:55 -> 11:59 edit is an hour change of its own: it
        // strikes eleven, run down here before the save.
        h.rt->game().time.hour = 11;
        h.rt->game().time.minute = 59;
        h.run(3000, [&] { synth.pull(5); });
        h.menu_save();
        h.run(110, [&] { synth.pull(5); });
        const bool quiet = h.rt->ambient().chimes() == 0;
        h.down();
        h.up(); // the hour moves: [0x5884] = the new hour
        h.run(60, [&] { synth.pull(5); });
        const bool armed = quiet && h.rt->game().time.hour == 12 && h.rt->ambient().chimes() > 0;
        h.key('l', true);
        const auto t0 = h.now();
        h.run(2200, [&] { synth.pull(5); });
        check(armed && synth.count_after(SfxId::AmbientClockChime, t0) == 0 && synth.count_after(SfxId::AmbientClockTick, t0) > 0,
              "M9 Alt+L beside a clock that was about to strike: the loaded world only ticks (no strike survives the load)");
    }
    {
        Synth synth;
        Run h(53, false);
        h.rt->configure_audio(g_real, &synth);
        start_arena(h, 41);
        fell_all(h);
        drive_out(h, 3);
        const bool playing = synth.count(SfxId::VictoryFanfare) == 1 && !synth.player.idle();
        h.key('l', true);
        check(playing && silent_after_release(synth, 5), "M6 Alt+L during the victory fanfare: faded within 2 ms, nothing queued survives");
    }
    {
        Synth synth;
        Run h(59, false);
        h.rt->configure_audio(g_real, &synth);
        DebugTeleportRequest dr{};
        dr.kind = DebugDestinationKind::Dungeon;
        dr.location = 35;
        dr.floor = 6;
        dr.x = 3;
        dr.y = 2;
        const auto st = apply_debug_teleport(h.rt->command_context_for_test(), dr).status;
        h.rt->render(h.board, true);
        const bool in = st == DebugTeleportStatus::Applied && h.rt->command_context().dungeon;
        const auto ticks = h.rt->ambient_ticks();
        h.run(1100, [&] { synth.pull(5); });
        check(in && h.rt->ambient_ticks() == ticks, "M7 in a dungeon 0x266c never redraws: no ambient tick at all");
        h.teleport(DebugDestinationKind::SmallMap, uint8_t(fountain.location), 0, fountain.x, fountain.y);
        GameEvent won{};
        won.kind = GameEventKind::GameWon;
        h.present(won);
        h.run(100);
        const auto t0 = h.now();
        h.run(1100, [&] { synth.pull(5); });
        check(h.mode() == UiMode::Ending && synth.ambient_after(t0) == 0, "M8 the Ending: no ambience");
    }

    std::printf("A3-03 sfx runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
