// Alpha 3 A3-01 -- the audio seam through the REAL AlphaRuntime: raw keys in,
// a recording backend out, on the host esp_timer shim's virtual clock.
//
//   a3_01_audio_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> <stock audio fixture>
//
// What it proves, on the device's own code paths:
//   L  the device loader reads the real, the stock and a missing audio pack;
//   G  every cue the core emits reaches the backend when it is PRESENTED, and
//      game state / RNG are identical with audio on, off or refused;
//   T  the paced Camp apparition runs the same timeline, frame for frame, with
//      no audio, a recording backend, a refusing backend and SFX muted;
//   S  SFX / Music Volume through the System Menu by raw keys, the per-channel
//      gains, the no-music rows, and settings.json across a reboot;
//   D  Developer > Diagnostics > Audio test tone reaches the backend;
//   H  a load flushes SFX and changes nothing a control run does not;
//   I  the game-pack identity gate is untouched by the audio pack.
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/frontend_settings.h"
#include "openu5/scene_timing.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
void host_memory_save_forget_for_test();   // alpha_save_memory_host_stub.cpp
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
constexpr int64_t kClockStartUs = 5'000'000;

struct Recorder final : AudioBackend {
    struct Call {
        std::string what;
        int a = 0, b = 0;
        int64_t us = 0;
    };
    std::vector<Call> calls;
    bool refuse = false;
    void log(const char *w, int a = 0, int b = 0) { calls.push_back({w, a, b, openu5_host_virtual_clock_us()}); }
    bool play_sfx(const SfxRequest &r) override {
        log("sfx", int(r.id), int(r.gain_q15));
        return !refuse;
    }
    void stop_sfx() override { log("stop_sfx"); }
    bool start_music(MusicSong s, uint16_t g) override {
        log("music", int(s), int(g));
        return !refuse;
    }
    void stop_music() override { log("stop_music"); }
    void set_gain(AudioChannel c, uint16_t g) override { log(c == AudioChannel::Sfx ? "gain_sfx" : "gain_music", int(g)); }
    size_t count(const char *w) const {
        size_t n = 0;
        for (const auto &c : calls) n += c.what == w;
        return n;
    }
    size_t sfx_of(SfxId id) const {
        size_t n = 0;
        for (const auto &c : calls) n += c.what == "sfx" && c.a == int(id);
        return n;
    }
    int64_t first_sfx_us(SfxId id) const {
        for (const auto &c : calls)
            if (c.what == "sfx" && c.a == int(id)) return c.us;
        return -1;
    }
};

// Counts the Sfx events the core emits (before any pacing) and forwards them,
// or -- the oracle for "sound is state-free" -- drops every Sfx before the
// runtime ever sees it, so no audio code runs at all.
struct CueSpy {
    EventSink inner{};
    int cues = 0;
    bool drop_sfx = false;
    std::vector<std::string> names;
    static void emit(void *p, const GameEvent &e) {
        auto &s = *static_cast<CueSpy *>(p);
        if (e.kind == GameEventKind::Sfx && e.text) {
            ++s.cues;
            s.names.push_back(e.text);
            if (s.drop_sfx) return;
        }
        if (s.inner.emit) s.inner.emit(s.inner.context, e);
    }
};

struct Sample {
    int64_t us;
    bool inverted, key_wait, pacer;
    size_t frames;
    bool operator==(const Sample &o) const {
        return us == o.us && inverted == o.inverted && key_wait == o.key_wait && pacer == o.pacer && frames == o.frames;
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    std::vector<Sample> timeline;
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
    bool key_wait() const {
        return rt->ui()->mode() == UiMode::KeyWait && rt->ui()->request() == UiRequestId::CampAdvance;
    }
    void sample() {
        timeline.push_back({now(), rt->camp_scene_inverted(), key_wait(), rt->narrative_pacer().active(), batch37_frame_count()});
    }
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
            sample();
        }
    }
    bool run_to_key(int64_t limit_ms = 60000) {
        for (int64_t t = 0; t < limit_ms && !key_wait(); t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
            sample();
        }
        return key_wait();
    }
    std::string transcript() const {
        std::string out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
        return out;
    }
    bool saw(const char *needle) const { return transcript().find(needle) != std::string::npos; }
    /** Alt+M -> Settings (root row 3). */
    void open_settings() {
        key('m', true);
        for (int i = 0; i < 3; ++i) down();
        key('\r');
    }
    const char *menu_line(int row) const {
        static FrontendView v;
        v = rt->system_menu_view();
        return row >= 0 && size_t(row) < v.line_count && v.lines[row] ? v.lines[row] : "";
    }
    std::string menu_footer() const {
        const auto v = rt->system_menu_view();
        return v.footer ? v.footer : "";
    }
    /** A compact fingerprint of the game state an audio path must never touch. */
    std::string state() {
        auto &g = rt->game();
        std::ostringstream s;
        s << int(g.position.map.location) << ',' << int(g.position.map.floor) << ',' << g.position.xy.x << ','
          << g.position.xy.y << ',' << int(g.time.hour) << ':' << int(g.time.minute) << ',' << g.food << ','
          << g.gold << ',' << int(g.party.characters[0].current_hp) << ',' << int(g.party.characters[0].level)
          << ",rng" << g.rng.get_seed(); // the whole 16-bit generator state
        s << ",cmds" << rt->routed_command_count() << '\n' << transcript();
        return s.str();
    }
};

AudioPackInfo g_real{}, g_stock{}, g_missing{};

int camp_seed() {
    for (int s = 1; s <= 512; ++s) {
        Run candidate(s, false);
        candidate.camp();
        if (candidate.rt->commands().camp_advance.phase != CommandState::CampAdvance::Phase::None) return s;
    }
    return -1;
}

/** The paced Camp apparition up to the Hail getkey, under one audio setup. */
struct CampResult {
    std::vector<Sample> timeline;
    std::vector<int64_t> frame_times;
    std::string state;
    Recorder rec;
    bool reached = false;
    bool queued_at_command = false; // no cue had reached the backend inside the command
};
enum class AudioSetup { None, Recording, Refusing, SfxMuted };
std::unique_ptr<CampResult> camp_run(int seed, AudioSetup setup) {
    auto out = std::make_unique<CampResult>();
    if (setup == AudioSetup::SfxMuted) {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings muted{};
        muted.sound_volume = 0;
        muted.trackball_speed = openu5::kTrackballSpeedLegacy; // A4-ENH1: the one-pulse-per-step speed (10) this route was written for
        encode_settings(muted, tdeck::a3_host_settings_text());
    }
    Run h(seed, true);
    tdeck::a3_host_settings_enabled() = false;
    out->rec.refuse = setup == AudioSetup::Refusing;
    if (setup != AudioSetup::None) h.rt->configure_audio(g_real, &out->rec);
    const size_t before = out->rec.count("sfx");
    h.camp();
    // Straight after the command: the cues are queued in the pacer, not played.
    out->queued_at_command = out->rec.count("sfx") == before;
    out->reached = h.run_to_key();
    h.run(50);
    out->timeline = h.timeline;
    for (size_t i = 0; i < batch37_frame_count(); ++i) out->frame_times.push_back(batch37_frame_time_us(i));
    out->state = h.state();
    return out;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;

    // ---- L: the device loader --------------------------------------------
    g_real = tdeck::load_audio_pack_info(argv[2]);
    g_stock = tdeck::load_audio_pack_info(argv[3]);
    g_missing = tdeck::load_audio_pack_info("a3-01-no-such-audio-pack.bin");
    check(g_real.state == AudioPackState::Valid && music_availability(g_real) == MusicAvailability::Available,
          "L1 the device loader reads the real audio pack: Valid, music Available");
    check(g_stock.state == AudioPackState::Valid && music_availability(g_stock) == MusicAvailability::StockNoMusic,
          "L1 ... the stock pack: Valid, stock -- no music");
    check(g_missing.state == AudioPackState::Missing && music_availability(g_missing) == MusicAvailability::NoAudioPack,
          "L1 ... and no file at all: Missing, no music, no error");
    {
        const char *junk = "a3-01-junk-audio-pack.bin";
        std::ofstream(junk, std::ios::binary) << "not an audio pack at all, just text";
        const auto j = tdeck::load_audio_pack_info(junk);
        std::remove(junk);
        check(j.state == AudioPackState::NotAudioPack && music_availability(j) == MusicAvailability::AudioPackInvalid,
              "L1 ... and a foreign file: refused, no music");
    }

    // ---- G: routing at presentation, and nothing else changes ------------
    {
        Run silent(7, false);
        check(silent.rt->audio().music_availability() == MusicAvailability::NoAudioPack &&
                  silent.rt->audio_pack().state == AudioPackState::Missing,
              "G0 before configure_audio the runtime is silent and reports no audio pack");
        Recorder rec;
        Run loud(7, false);
        loud.rt->configure_audio(g_real, &rec);
        check(rec.count("gain_sfx") == 1 && rec.count("gain_music") == 1 &&
                  rec.calls[rec.calls.size() - 2].a == int(volume_to_gain_q15(80)),
              "G0 configure_audio hands the backend both channel gains (80 % defaults) before any sound");
        const size_t sfx0 = rec.count("sfx");
        Run no_cues(7, false);
        no_cues.spy.drop_sfx = true;
        for (Run *r : {&silent, &loud, &no_cues}) {
            r->up(); r->up(); r->left(); r->left(); r->down(); r->right(); r->right(); r->down();
            r->key('x'); r->key('\r');
        }
        const size_t routed = rec.count("sfx") - sfx0;
        bool ids = routed == size_t(loud.spy.cues);
        size_t k = sfx0;
        for (const auto &name : loud.spy.names) {
            while (k < rec.calls.size() && rec.calls[k].what != "sfx") ++k;
            ids = ids && k < rec.calls.size() && rec.calls[k].a == int(sfx_from_cue(name.c_str())) &&
                  rec.calls[k].b == int(volume_to_gain_q15(80));
            ++k;
        }
        check(loud.spy.cues > 0 && ids,
              "G11 every Sfx the core emitted during play reached the backend once, in order, as its SfxId, at the "
              "SFX gain");
        check(silent.state() == loud.state() && silent.spy.names == loud.spy.names,
              "G18 position, clock, food, gold, HP, the RNG stream, the command count and the transcript are "
              "identical with the audio path silent or recording");
        check(no_cues.state() == loud.state() && no_cues.spy.cues == loud.spy.cues,
              "G18 and identical to a run whose Sfx events never reach the runtime at all: presenting sound "
              "draws no RNG and writes no state");
        Recorder refusing;
        refusing.refuse = true;
        Run refused(7, false);
        refused.rt->configure_audio(g_real, &refusing);
        refused.up(); refused.up(); refused.left(); refused.left(); refused.down(); refused.right(); refused.right();
        refused.down(); refused.key('x'); refused.key('\r');
        check(refused.state() == silent.state() && refused.rt->audio().stats().sfx_refused > 0,
              "G20 a backend that refuses every request changes nothing either: failures degrade to silence");
    }

    // ---- T: scene timing is the pacer's, never the audio's ----------------
    const int seed = camp_seed();
    check(seed > 0, "T0 a shipped-pack Camp reaches the apparition");
    if (seed > 0) {
        const auto none = camp_run(seed, AudioSetup::None);
        const auto rec = camp_run(seed, AudioSetup::Recording);
        const auto refuse = camp_run(seed, AudioSetup::Refusing);
        const auto muted = camp_run(seed, AudioSetup::SfxMuted);
        check(none->reached && rec->reached && refuse->reached && muted->reached,
              "T19 the paced apparition reaches its Hail getkey under every audio setup");
        check(none->timeline == rec->timeline && none->timeline == refuse->timeline && none->timeline == muted->timeline,
              "T19 the 5 ms timeline (XOR, getkey, pacer, frame count) is identical: none / recording / refusing / muted");
        check(none->frame_times == rec->frame_times && none->frame_times == refuse->frame_times &&
                  none->frame_times == muted->frame_times,
              "T19 every presented frame carries the same timestamp under every audio setup");
        check(none->state == rec->state && none->state == refuse->state && none->state == muted->state,
              "T18 and the game state after the scene is the same");
        const auto m = rec->rec.first_sfx_us(SfxId::ApparitionMaterialize);
        const auto a = rec->rec.first_sfx_us(SfxId::ApparitionArpeggio);
        const auto c = rec->rec.first_sfx_us(SfxId::ApparitionHealChime);
        const auto ch = rec->rec.first_sfx_us(SfxId::ApparitionChord);
        std::printf("  cue times (ms after the command clock origin): materialize %lld arpeggio %lld chime %lld chord %lld\n",
                    (long long)(m - kClockStartUs) / 1000, (long long)(a - kClockStartUs) / 1000,
                    (long long)(c - kClockStartUs) / 1000, (long long)(ch - kClockStartUs) / 1000);
        check(rec->queued_at_command && m >= 0 && a > m && c > a && ch > c,
              "T20 no cue is played inside the Camp command; the four apparition cues reach the backend one by one, "
              "in scene order, as the pacer releases them");
        check(a - m >= int64_t(tone_sweep_ms(kApparitionMaterializeSamples)) * 1000,
              "T20 the arpeggio cue waits for the materialize hold: the PACER releases sound, sound never paces the scene");
        check(muted->rec.count("sfx") == 0 && muted->rec.count("gain_sfx") >= 1,
              "T16 with SFX Volume 0 no apparition cue is submitted at all");
    }

    // ---- S: the Settings rows by raw keys, and settings.json --------------
    tdeck::a3_host_settings_enabled() = true;
    tdeck::a3_host_settings_text().clear();
    {
        Recorder rec;
        Run a(11, false);
        a.rt->configure_audio(g_real, &rec);
        a.open_settings();
        check(a.rt->system_menu_open() && std::strcmp(a.menu_line(4), "SFX Volume: 80%") == 0 &&
                  std::strcmp(a.menu_line(5), "Music Volume: 80%") == 0,
              "S5 Alt+M > Settings shows SFX Volume and Music Volume (supported patch)");
        for (int i = 0; i < 4; ++i) a.down();
        rec.calls.clear();
        a.right();
        check(a.rt->audio().sfx_volume() == 90 && std::strcmp(a.menu_line(4), "SFX Volume: 90%") == 0 &&
                  rec.count("gain_sfx") == 1 && rec.count("gain_music") == 0 &&
                  rec.calls.back().a == int(volume_to_gain_q15(90)),
              "S5/S14 trackball Right: SFX 90 %, applied live to the SFX channel only");
        a.down();
        rec.calls.clear();
        a.left();
        a.left();
        check(a.rt->audio().music_volume() == 60 && std::strcmp(a.menu_line(5), "Music Volume: 60%") == 0 &&
                  rec.count("gain_music") == 2 && rec.count("gain_sfx") == 0,
              "S6/S15 Music Volume steps down to 60 %, applied live to the music channel only");
        a.mic();         // leave Settings: PersistSettings
        a.key('m', true); // close the menu
        const auto &text = tdeck::a3_host_settings_text();
        check(!a.rt->system_menu_open() && text.find("\"soundVolume\":90") != std::string::npos &&
                  text.find("\"musicVolume\":60") != std::string::npos,
              "S8 closing the menu writes both volumes to settings.json");
    }
    {
        Recorder rec;
        Run b(11, false); // a reboot: a new runtime reading the same settings.json
        check(b.rt->device_settings().sound_volume == 90 && b.rt->device_settings().music_volume == 60 &&
                  b.rt->audio().sfx_volume() == 90 && b.rt->audio().music_volume() == 60,
              "S8 after a reboot the volumes come back from settings.json and reach the audio service");
        b.rt->configure_audio(g_stock, &rec);
        b.open_settings();
        for (int i = 0; i < 5; ++i) b.down();
        b.right();
        b.key('\r');
        check(std::strcmp(b.menu_line(5), "Music Volume: Unavailable") == 0 &&
                  b.menu_footer() == "Stock DOS game files have no music" && b.rt->device_settings().music_volume == 60 &&
                  rec.count("music") == 0,
              "S7 stock game files: Music Volume reads Unavailable, the footer says why, adjust keys do nothing, "
              "the stored 60 % is kept and nothing reaches the music channel");
        b.up();
        b.right();
        check(b.rt->audio().sfx_volume() == 100, "S7 SFX Volume stays fully usable without music");
        b.mic();
        b.key('m', true);
        check(tdeck::a3_host_settings_text().find("\"musicVolume\":60") != std::string::npos,
              "S7 the unavailable Music Volume preference is persisted unchanged");
    }
    {
        Recorder rec;
        Run c(11, false); // the music patch is installed later
        c.rt->configure_audio(g_real, &rec);
        c.open_settings();
        check(std::strcmp(c.menu_line(5), "Music Volume: 60%") == 0 && c.rt->audio().music_volume() == 60,
              "S7 a later supported audio pack finds the kept 60 % preference live again");
        Run d(11, false);
        d.rt->configure_audio(g_missing, nullptr);
        d.open_settings();
        for (int i = 0; i < 5; ++i) d.down();
        check(std::strcmp(d.menu_line(5), "Music Volume: Unavailable") == 0 &&
                  d.menu_footer() == "No audio pack: npm run pack:audio",
              "S7 audio pack removed: gracefully unavailable again, with the reason");
    }
    {
        tdeck::a3_host_settings_text() = "{\"version\":1,\"brightness\":";
        Run e(11, false);
        check(e.rt->audio().sfx_volume() == 80 && e.rt->audio().music_volume() == 80 &&
                  e.rt->device_settings().brightness == 80,
              "S9 a corrupt settings.json falls back to the defaults (80 % / 80 %) and the game starts");
        tdeck::a3_host_settings_text() =
            "{\"version\":1,\"brightness\":70,\"movementMode\":false,\"trackballResponsiveness\":100,\"uiSize\":1,"
            "\"developerToolsVisible\":false,\"soundVolume\":80,\"musicVolume\":80,\"touchControls\":false}";
        Run f(11, false);
        check(f.rt->device_settings().brightness == 70 && f.rt->audio().sfx_volume() == 80 &&
                  f.rt->audio().music_volume() == 80,
              "S10 an Alpha 2 settings.json loads as it is: its own brightness, and 80 % / 80 % for the audio rows");
    }

    // ---- D: the Developer test tone ---------------------------------------
    {
        tdeck::a3_host_settings_text().clear();
        Recorder rec;
        Run t(11, false);
        t.rt->configure_audio(g_real, &rec);
        t.key('d', true);
        t.up();
        t.up();       // root: Diagnostics is two rows above row 0
        t.key('\r');
        t.up();       // Diagnostics: the last row, "Audio test tone (SFX)"
        t.key('\r');
        check(rec.sfx_of(SfxId::DiagnosticTone) == 1 && rec.calls.back().b == int(volume_to_gain_q15(80)),
              "D1 Alt+D > Diagnostics > Audio test tone sends ONE diagnostic tone at the SFX gain");
        t.key('\b');
        t.key('\b');
        check(t.saw("Audio test: tone sent, SFX 80%"), "D1 and says so on a System line");
        FrontendSettings muted{};
        muted.sound_volume = 0;
        muted.trackball_speed = openu5::kTrackballSpeedLegacy; // A4-ENH1: the one-pulse-per-step speed (10) this route was written for
        encode_settings(muted, tdeck::a3_host_settings_text());
        Recorder rec0;
        Run z(11, false);
        z.rt->configure_audio(g_real, &rec0);
        z.key('d', true);
        z.up();
        z.up();
        z.key('\r');
        z.up();
        z.key('\r');
        z.key('\b');
        z.key('\b');
        check(rec0.sfx_of(SfxId::DiagnosticTone) == 0 && z.saw("Audio test: silent (SFX Volume 0% or no output)"),
              "D2 at SFX Volume 0 % the test tone is not submitted, and the line says why");
    }
    tdeck::a3_host_settings_enabled() = false;
    tdeck::a3_host_settings_text().clear();

    // ---- H: a load flushes audio and nothing else --------------------------
    {
        Recorder rec;
        Run with(13, false), without(13, false);
        with.rt->configure_audio(g_real, &rec);
        for (Run *r : {&with, &without}) {
            // menu Save (A4-SAVE2: Save Game -> the empty Slot 1 saves at once;
            // A4-UI3: and the save returns to the game, naming the slot). Each
            // run starts from a blank card, so both save into Slot 1.
            tdeck::host_memory_save_forget_for_test();
            r->key('m', true); r->down(); r->key('\r'); r->key('\r');
            r->up(); r->left();                                               // walk away
            r->key('l', true);                                                // Alt+L: load
        }
        size_t last_stop = 0;
        for (size_t i = 0; i < rec.calls.size(); ++i)
            if (rec.calls[i].what == "stop_sfx") last_stop = i + 1;
        check(with.saw("Save complete") && last_stop > 0, "H1 Alt+L flushes the audio service's SFX (stop_sfx)");
        check(with.state() == without.state(), "H1 and the loaded world is identical with or without audio");
    }

    // ---- I: the game-pack identity gate is untouched ------------------------
    {
        // argv[1] is <native>/core/../assets/openu5-alpha1-resources.bin
        std::ifstream in(std::filesystem::path(argv[1]).parent_path() / ".." / "targets" / "tdeck" / "main" / "main.cpp");
        std::stringstream s;
        s << in.rdbuf();
        const auto main_cpp = s.str();
        const auto gate = main_cpp.find("const bool packs_match=tiles==ESP_OK&&alpha==ESP_OK&&tile_report.firmware_match&&alpha_report.firmware_match;");
        const auto load = main_cpp.find("load_audio_pack_info(");
        const auto close = main_cpp.find("\"alpha-resource-close\"");
        check(!main_cpp.empty() && gate != std::string::npos && load != std::string::npos && load > gate && load > close,
              "I4 main.cpp's identity gate is the Alpha 2 expression, and the audio pack is read only after it passed");
        check(report.firmware_match, "I4 the unchanged Alpha 2 game pack still matches the firmware's identity lock");
    }

    openu5_host_virtual_clock_us() = -1;
    std::printf("A3-01 audio runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
