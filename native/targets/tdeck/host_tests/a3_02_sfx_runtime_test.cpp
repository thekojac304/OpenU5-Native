// Alpha 3 A3-02 -- real sound effects through the REAL AlphaRuntime: raw keys
// in, the PC-speaker synthesizer out, on the host esp_timer shim's virtual
// clock.
//
//   a3_02_sfx_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin>
//
// The backend here is the device's policy without the I2S transport: it
// declines a cue with no A3-02 program exactly as TdeckAudioBackend does, and
// hands the rest to an openu5::SfxPlayer the test pulls PCM from.
//
//   R  each wired cue reaches the synthesizer exactly once, and 0 % is silence;
//   P  the harpsichord by raw keys: notes, order, rapid keys, no stuck note;
//   C  combat / ceremony cues from the events the core already emits;
//   T  Camp and Blackthorn timelines are identical with audio on, muted,
//      failing, stalled (never pulled) and racing (pulled far ahead);
//   M  Alt+L, System Menu Load, title Continue, Return to Title, Developer
//      and Ending leave no stuck or stale effect;
//   S  the persisted SFX Volume is the gain real effects are rendered at.
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/blackthorn_scene.h"
#include "openu5/debug_map_picker.h"
#include "openu5/frontend_settings.h"
#include "openu5/scene_timing.h"
#include "openu5/sfx_synth.h"

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
AudioPackInfo g_real{};
constexpr int64_t kClockStartUs = 5'000'000;

/** The device's SFX path without I2S: sfx_supported gate + SfxPlayer. */
struct Synth final : AudioBackend {
    SfxPlayer player;
    uint16_t gain = 0;
    bool broken = false; // a backend whose hardware failed: declines everything
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
    /** Render `ms` of output at the live gain; returns the non-zero samples. */
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
    int64_t first_us(SfxId id) const {
        for (const auto &s : subs)
            if (s.id == id) return s.us;
        return -1;
    }
};

struct CueSpy {
    EventSink inner{};
    std::vector<std::string> names;
    static void emit(void *p, const GameEvent &e) {
        auto &s = *static_cast<CueSpy *>(p);
        if (e.kind == GameEventKind::Sfx && e.text) s.names.push_back(e.text);
        if (s.inner.emit) s.inner.emit(s.inner.context, e);
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    CueSpy spy;
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
        g.party.characters[1].exp = 0;
        g.rng.seed(uint32_t(seed));
        auto &ctx = rt->command_context_for_test();
        spy.inner = ctx.events;
        ctx.events = {&spy, CueSpy::emit};
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
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    void left() { ball(RawInputKind::TrackballLeft); }
    void right() { ball(RawInputKind::TrackballRight); }
    void camp() { key('h'); key('1'); key('\r'); key('n'); }
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
    /** A presented event, through the SAME sink the core's commands emit into. */
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
    /** Advance `ms` in 5 ms render ticks; `each` runs after every tick. */
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
          << ',' << int(g.party.characters[0].current_hp) << ",rng" << g.rng.get_seed() << ",cmds"
          << rt->routed_command_count() << '\n'
          << transcript();
        return s.str();
    }
    bool harpsichord() {
        auto &g = rt->game();
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::SmallMap;
        r.location = 17; // Lord British's castle
        r.floor = 2;     // the trigger harpsichord at (17,18), its chair at (17,17)
        r.x = 17;
        r.y = 17;
        const bool ok = apply_debug_teleport(rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
        rt->render(board, true);
        return ok && g.position.map.location == 17 && g.position.map.floor == 2;
    }
};

/** Who sounded: the backend setups the timing checks compare. */
enum class Setup { None, Synth, Muted, Broken, Stalled, Racing };
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

struct Timeline {
    std::vector<std::string> samples; // one line per 5 ms tick
    std::vector<int64_t> frames;
    std::string state;
    std::unique_ptr<Synth> synth = std::make_unique<Synth>();
    bool reached = false;
    size_t sounding = 0;
};

void configure(Run &h, Timeline &out, Setup s) {
    if (s == Setup::None) return;
    out.synth->broken = s == Setup::Broken;
    h.rt->configure_audio(g_real, out.synth.get());
}
void pump_audio(Timeline &out, Setup s) {
    if (s == Setup::Synth || s == Setup::Muted || s == Setup::Broken) out.sounding += out.synth->pull(5);
    if (s == Setup::Racing) out.sounding += out.synth->pull(500); // audio finishes long before the scene
    // Stalled: never pulled -- every sound "lasts forever"
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

int camp_seed() {
    for (int s = 1; s <= 512; ++s) {
        Run candidate(s, false);
        candidate.camp();
        if (candidate.rt->commands().camp_advance.phase != CommandState::CampAdvance::Phase::None) return s;
    }
    return -1;
}

std::unique_ptr<Timeline> camp_timeline(int seed, Setup s) {
    auto out = std::make_unique<Timeline>();
    muted_settings(s == Setup::Muted);
    Run h(seed, true);
    muted_settings(false);
    configure(h, *out, s);
    h.camp();
    auto key_wait = [&] { return h.rt->ui()->mode() == UiMode::KeyWait && h.rt->ui()->request() == UiRequestId::CampAdvance; };
    for (int64_t t = 0; t < 60000 && !key_wait(); t += 5) {
        openu5_host_virtual_clock_us() += 5000;
        h.rt->render(h.board);
        pump_audio(*out, s);
        std::ostringstream line;
        line << h.now() << ' ' << h.rt->camp_scene_inverted() << ' ' << h.rt->narrative_pacer().active() << ' '
             << batch37_frame_count();
        out->samples.push_back(line.str());
    }
    out->reached = key_wait();
    for (size_t i = 0; i < batch37_frame_count(); ++i) out->frames.push_back(batch37_frame_time_us(i));
    out->state = h.state();
    return out;
}

std::unique_ptr<Timeline> blackthorn_timeline(Setup s) {
    auto out = std::make_unique<Timeline>();
    muted_settings(s == Setup::Muted);
    Run h(21, true);
    muted_settings(false);
    configure(h, *out, s);
    static BlackthornSceneState state{};
    static BlackthornSceneScript script{};
    state = {};
    script = {};
    init_capture_scene(state, "AFM", 3);
    build_blackthorn_entry_script(state, script);
    GameEvent e{};
    e.kind = GameEventKind::BlackthornScene;
    e.blackthorn_scene = &script;
    h.present(e);
    for (int64_t t = 0; t < 12000 && h.rt->blackthorn_pacer().active(); t += 5) {
        openu5_host_virtual_clock_us() += 5000;
        h.rt->render(h.board);
        pump_audio(*out, s);
        std::ostringstream line;
        line << h.now() << ' ' << h.rt->blackthorn_pacer().released_steps() << ' ' << int(h.rt->blackthorn_pacer().state())
             << ' ' << batch37_frame_count();
        out->samples.push_back(line.str());
    }
    out->reached = !h.rt->blackthorn_pacer().active() && !out->samples.empty();
    for (size_t i = 0; i < batch37_frame_count(); ++i) out->frames.push_back(batch37_frame_time_us(i));
    out->state = h.state();
    return out;
}

/** Non-zero samples in the next `ms`, after at most the 2 ms release. */
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
    g_real = tdeck::load_audio_pack_info(argv[2]);
    check(g_real.state == AudioPackState::Valid, "R0 the real audio pack loads (music capability only; SFX need none)");

    // ---- R: world cues, exactly once, and 0 % is silence --------------------
    {
        Synth synth;
        Run loud(7, false);
        loud.rt->configure_audio(g_real, &synth);
        for (int i = 0; i < 6; ++i) loud.up();
        for (int i = 0; i < 6; ++i) loud.left();
        for (int i = 0; i < 6; ++i) loud.down();
        for (int i = 0; i < 6; ++i) loud.right();
        std::vector<SfxId> want;
        for (const auto &n : loud.spy.names)
            if (sfx_supported(sfx_from_cue(n.c_str()))) want.push_back(sfx_from_cue(n.c_str()));
        std::vector<SfxId> got;
        for (const auto &s : synth.subs) got.push_back(s.id);
        const size_t steps = size_t(std::count(want.begin(), want.end(), SfxId::MoveStep));
        std::printf("  walk: %zu cues emitted, %zu rendered, %zu footsteps\n", loud.spy.names.size(), got.size(), steps);
        check(steps > 0 && got == want, "R1 every wired cue the walk emitted reached the synthesizer exactly once, in order");
        check(synth.pull(3000) > 0 && synth.player.stats().started > 0, "R1 and it is audible: the synthesizer rendered it");
        check(synth.player.idle() && synth.pull(500) == 0, "R1 and it ends: silence after the last footstep");
        muted_settings(true);
        Synth quiet;
        Run muted(7, false);
        muted_settings(false);
        muted.rt->configure_audio(g_real, &quiet);
        for (int i = 0; i < 6; ++i) muted.up();
        for (int i = 0; i < 6; ++i) muted.left();
        check(quiet.subs.empty() && quiet.gain == 0 && quiet.pull(2000) == 0 && muted.rt->audio().stats().sfx_muted > 0,
              "R2 at SFX Volume 0 nothing is submitted and the output is exact silence");
    }

    // ---- P: the harpsichord by raw keys --------------------------------------
    {
        Synth synth;
        Run h(11, false), silent(11, false);
        h.rt->configure_audio(g_real, &synth);
        const bool moved = h.harpsichord() && silent.harpsichord();
        const int melody[] = {6, 7, 8, 9, 8};
        h.key(uint8_t('0' + melody[0]));
        silent.key(uint8_t('0' + melody[0]));
        // The seat is re-derived as input is routed (the tile south is 0x8D).
        check(moved && h.rt->ui()->harpsichord_active() && silent.rt->ui()->harpsichord_active(),
              "P0 Developer Teleport to LB castle floor 2 (17,17): the party sits at the harpsichord");
        for (int i = 1; i < 5; ++i) {
            h.key(uint8_t('0' + melody[i])); // all five at the same instant: rapid keys
            silent.key(uint8_t('0' + melody[i]));
        }
        bool order = synth.subs.size() == 5;
        for (size_t i = 0; order && i < 5; ++i) order = synth.subs[i].id == SfxId::InstrumentNote && synth.subs[i].param == melody[i];
        check(order, "P1 five digit keys play five notes, in key order, each as its digit (TOWN 0x0e34)");
        h.down();
        silent.down();
        check(h.state() == silent.state(), "P2 rapid notes never hold input: the next command runs at once, state as without audio");
        std::vector<int16_t> pcm;
        const size_t sounding = synth.pull(1200, &pcm);
        const size_t bump = 2976; // beep(0xa5,0xc8), queued behind the notes
        check(sounding > 5 * 2480 + bump - 64 && sounding <= 5 * 2480 + bump && synth.player.idle(),
              "P3 the five 155 ms notes and then the bump play back to back (none dropped, none overlapped) and stop");
        check(synth.count(SfxId::MoveBlocked) == 1, "P3 the step south into the instrument is the wall bump beep");
        // The notes' pitches, measured from the PCM the runtime produced.
        auto pitch = [&](size_t note) {
            size_t n = 0, first = 0, last = 0;
            for (size_t i = note * 2480 + 300; i < note * 2480 + 2180 && i < pcm.size(); ++i)
                if (pcm[i - 1] <= 0 && pcm[i] > 0) {
                    if (!n) first = i;
                    last = i;
                    ++n;
                }
            return n > 1 ? double(n - 1) * 16000 / double(last - first) : 0.0;
        };
        const double p6 = pitch(0), p7 = pitch(1), p8 = pitch(2), p9 = pitch(3), p8b = pitch(4);
        std::printf("  harpsichord 6 7 8 9 8: %.0f %.0f %.0f %.0f %.0f Hz\n", p6, p7, p8, p9, p8b);
        check(p6 < p7 && p7 < p8 && p8 < p9 && std::abs(p8b - p8) < p8 * 0.02,
              "P4 the pitch follows the keys: 6 < 7 < 8 < 9, and 8 again is the same note");
        for (int d : {1, 2, 3, 4}) h.key(uint8_t('0' + d));
        h.key('m', true); // the System Menu opens mid-phrase
        h.run(1000, [&] { synth.pull(5); });
        check(synth.player.idle() && synth.pull(300) == 0, "P5 opening the System Menu mid-phrase leaves no stuck note");
        h.key('m', true);
        h.key('5');
        h.menu_save();
        h.key('6');
        h.key('l', true);
        check(silent_after_release(synth), "P5 Alt+L during a note: faded out within 2 ms, nothing queued survives");
    }

    // ---- C: combat and the ceremony, from the events the core emits -----------
    {
        Synth synth;
        Run h(13, false);
        h.rt->configure_audio(g_real, &synth);
        auto &cs = h.rt->combat_state_for_test();
        cs.count = 2;
        cs.actors[0].id = 1;
        cs.actors[0].member = 0; // a party member
        cs.actors[1].id = 7;
        cs.actors[1].member = 255; // an enemy
        auto combat = [&](CombatEventKind k, int32_t target, int8_t hit) {
            CombatEvent ce{};
            ce.kind = k;
            ce.actor = target == 1 ? 7 : 1;
            ce.target = target;
            ce.hit = hit;
            ce.text = "x";
            GameEvent e{};
            e.kind = GameEventKind::Combat;
            e.combat = &ce;
            h.present(e);
        };
        combat(CombatEventKind::Attacked, 7, 1);
        combat(CombatEventKind::Attacked, 1, 1);
        combat(CombatEventKind::Attacked, 7, 0);
        combat(CombatEventKind::Died, 7, -1);
        combat(CombatEventKind::Message, 7, -1);
        // A3-05: a death is silent; the killing blow's Attacked carries 0x3564's burst.
        const bool mapped = synth.subs.size() == 2 && synth.subs[0].id == SfxId::CombatHit &&
                            synth.subs[1].id == SfxId::CombatHitHeavy;
        check(mapped, "C1 arena events: hit on an enemy -> combat-hit, on the party -> heavy, miss and death silent");
        check(synth.pull(1500) > 0 && synth.player.idle(), "C1 and each burst renders and ends");
        GameEvent m{};
        m.kind = GameEventKind::MagicCeremony;
        m.note = 3;
        h.cue("spell-cast");
        h.present(m);
        check(synth.subs.size() == 3 && synth.subs[2].id == SfxId::TimeSpell && synth.subs[2].param == 3 &&
                  h.rt->audio().stats().sfx_refused >= 1,
              "C2 a ceremonial spell: the marker cue is declined, the MagicCeremony(3) event plays CAST2 0x0000(3) once");
        SpeakerProgram p;
        compile_sfx(SfxId::TimeSpell, 3, p);
        check(synth.pull(int(speaker_program_frames(p) / 16) + 50) > 0 && synth.player.idle(),
              "C2 the ceremony renders for its lead + two sweeps and ends");
    }

    // ---- T: scene timing is the pacer's, never the audio's -------------------
    const int seed = camp_seed();
    check(seed > 0, "T0 a shipped-pack Camp reaches the apparition");
    if (seed > 0) {
        std::vector<std::unique_ptr<Timeline>> runs;
        const Setup setups[] = {Setup::None, Setup::Synth, Setup::Muted, Setup::Broken, Setup::Stalled, Setup::Racing};
        for (auto s : setups) runs.push_back(camp_timeline(seed, s));
        bool same = true, reached = true;
        for (size_t i = 1; i < runs.size(); ++i) {
            const bool eq = runs[i]->samples == runs[0]->samples && runs[i]->frames == runs[0]->frames &&
                            runs[i]->state == runs[0]->state;
            if (!eq) std::printf("  camp timeline differs: %s\n", setup_name(setups[i]));
            same = same && eq;
            reached = reached && runs[i]->reached;
        }
        check(reached && runs[0]->reached, "T19 the paced apparition reaches its Hail getkey under every audio setup");
        check(same, "T19 Camp: every 5 ms sample, every frame timestamp and the final state are identical with no audio, "
                    "the synthesizer, SFX muted, a failing backend, audio stalled and audio racing ahead");
        const auto &syn = *runs[1]->synth;
        const int64_t m = syn.first_us(SfxId::ApparitionMaterialize), a = syn.first_us(SfxId::ApparitionArpeggio),
                      c = syn.first_us(SfxId::ApparitionHealChime), ch = syn.first_us(SfxId::ApparitionChord);
        std::printf("  camp cues at %lld / %lld / %lld / %lld ms; sounding %zu frames\n", (long long)(m - kClockStartUs) / 1000,
                    (long long)(a - kClockStartUs) / 1000, (long long)(c - kClockStartUs) / 1000,
                    (long long)(ch - kClockStartUs) / 1000, runs[1]->sounding);
        check(syn.count(SfxId::ApparitionMaterialize) == 1 && syn.count(SfxId::ApparitionArpeggio) == 1 &&
                  syn.count(SfxId::ApparitionHealChime) >= 1 && syn.count(SfxId::ApparitionChord) >= 1 && m >= 0 && a > m &&
                  c > a && ch > c,
              "T20 the four apparition cues reach the synthesizer as the pacer releases them, in scene order");
        check(runs[1]->sounding > 0 && runs[2]->sounding == 0 && runs[3]->synth->subs.empty(),
              "T16 audible with the synthesizer; exact silence muted; nothing accepted by a failing backend");
    }
    {
        std::vector<std::unique_ptr<Timeline>> runs;
        const Setup setups[] = {Setup::None, Setup::Synth, Setup::Muted, Setup::Broken, Setup::Stalled, Setup::Racing};
        for (auto s : setups) runs.push_back(blackthorn_timeline(s));
        bool same = true;
        for (size_t i = 1; i < runs.size(); ++i) {
            const bool eq = runs[i]->samples == runs[0]->samples && runs[i]->frames == runs[0]->frames &&
                            runs[i]->state == runs[0]->state;
            if (!eq) std::printf("  blackthorn timeline differs: %s\n", setup_name(setups[i]));
            same = same && eq;
        }
        check(runs[0]->reached && runs[0]->samples.size() > 100,
              "T21 the Blackthorn entry segment runs paced through the device's pacer and comes down");
        check(same, "T21 Blackthorn: identical samples, frames and state with no audio, the synthesizer, muted, failing, "
                    "stalled and racing audio");
        const auto &syn = *runs[1]->synth;
        const int64_t at = syn.first_us(SfxId::BlackthornMaterialize);
        // The beat released right after the sweep beat: the first sample whose
        // released count moved past the one at the cue.
        int64_t next = -1;
        {
            size_t released_at_cue = 0;
            for (const auto &line : runs[1]->samples) {
                long long us = 0;
                size_t rel = 0;
                std::sscanf(line.c_str(), "%lld %zu", &us, &rel);
                if (us == at) released_at_cue = rel;
                if (at >= 0 && us > at && rel > released_at_cue && released_at_cue) {
                    next = us;
                    break;
                }
            }
        }
        std::printf("  blackthorn materialize cue at %lld ms, next beat at %lld ms (hold %u ms)\n",
                    (long long)(at - kClockStartUs) / 1000, (long long)(next - kClockStartUs) / 1000,
                    unsigned(tone_sweep_ms(kBlackthornMaterializeSamples)));
        check(syn.count(SfxId::BlackthornMaterialize) == 1 && at > kClockStartUs && next - at >= int64_t(tone_sweep_ms(kBlackthornMaterializeSamples)) * 1000,
              "T21 the materialization sweep sounds once, when its beat is applied, and the scene holds it for the "
              "PACER's 503 ms, not the sound's");
    }

    // ---- M: load and mode safety --------------------------------------------
    {
        Synth synth;
        Run h(17, false);
        h.rt->configure_audio(g_real, &synth);
        h.menu_save();
        auto sustained = [&] {
            h.cue(kApparitionChordCue); // 2.3 s, the longest A3-02 cue
            h.cue("move-blocked");      // and one queued behind it
            return synth.pull(100) > 0 && synth.player.pending() == 1;
        };
        int stops = synth.stops;
        check(sustained(), "M0 a sustained cue is playing with another queued");
        h.key('l', true);
        check(synth.stops == stops + 1 && silent_after_release(synth) && h.saw("Load complete"),
              "M1 Alt+L: the playing cue fades in 2 ms, the queued one is dropped, silence after the load");
        stops = synth.stops;
        sustained();
        h.menu_load();
        check(synth.stops == stops + 1 && silent_after_release(synth), "M2 System Menu Load: the same");
        stops = synth.stops;
        sustained();
        h.title_continue();
        check(synth.stops >= stops + 2 && silent_after_release(synth) && h.rt->game().position.map.location == 0,
              "M3 Return to Title flushes, and title Continue's load flushes again: silence in the loaded world");
        stops = synth.stops;
        sustained();
        h.return_to_title();
        check(synth.stops == stops + 1 && silent_after_release(synth),
              "M4 Return to Title alone flushes SFX (new in A3-02: the title replaces the world)");
        h.key('x');
        h.key('j');
        h.key('\r');
        h.cue("move-blocked");
        const size_t before = synth.subs.size();
        h.key('d', true);
        h.up();
        h.up();
        h.key('\r');
        h.up();
        h.key('\r'); // Developer > Diagnostics > Audio test tone
        synth.pull(3); // the preempted cue's 2 ms fade
        const bool diag = synth.subs.size() == before + 1 && synth.subs.back().id == SfxId::DiagnosticTone &&
                          synth.player.playing() == SfxId::DiagnosticTone;
        h.key('\b');
        h.key('\b');
        check(diag && synth.pull(600) > 0 && synth.player.idle() && synth.pull(200) == 0,
              "M5 Developer: the test tone preempts the playing cue, plays, and the audio path is idle and healthy after");
        GameEvent won{};
        won.kind = GameEventKind::GameWon;
        h.cue(kApparitionChordCue);
        h.present(won);
        const bool ending = h.rt->ui()->ending_active();
        h.run(3000, [&] { synth.pull(5); });
        check(ending && synth.player.idle() && synth.pull(200) == 0,
              "M6 Ending: the cue playing when the game ends finishes on its own; nothing is stuck in the terminal mode");
        h.cue("move-blocked");
        const bool alive = synth.pull(300) > 0;
        stops = synth.stops;
        h.return_to_title();
        check(alive && synth.stops == stops + 1 && silent_after_release(synth),
              "M6 the audio path still works in Ending, and Return to Title from it flushes");
    }

    // ---- S: the persisted SFX Volume is the gain of real effects ---------------
    {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings s30{};
        s30.sound_volume = 30;
        encode_settings(s30, tdeck::a3_host_settings_text());
        Synth synth;
        Run h(19, false); // a boot reading settings.json
        h.rt->configure_audio(g_real, &synth);
        std::vector<int16_t> got;
        h.cue("move-blocked");
        synth.pull(300, &got);
        SfxPlayer ref;
        SfxRequest r{};
        r.id = SfxId::MoveBlocked;
        std::vector<int16_t> want(got.size());
        ref.render(want.data(), 0, 0);
        ref.submit(r);
        ref.render(want.data(), want.size(), volume_to_gain_q15(30));
        check(h.rt->audio().sfx_volume() == 30 && synth.gain == volume_to_gain_q15(30) && got == want && !got.empty(),
              "S25 settings.json's 30 % reaches the service after a reboot and a real effect renders at exactly that gain");
        tdeck::a3_host_settings_enabled() = false;
        tdeck::a3_host_settings_text().clear();
    }

    openu5_host_virtual_clock_us() = -1;
    std::printf("A3-02 sfx runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
