// Alpha 3 A3-05 -- the audio finalization controls through the REAL
// AlphaRuntime: raw keys in, a recording backend out, on the host esp_timer
// shim's virtual clock.
//
//   a3_05_audio_controls <openu5-alpha1-resources.bin> <openu5-audio.bin> <stock audio fixture>
//
// What it proves, on the device's own code paths (ALPHA3_AUDIO.md section 29):
//   K  the shortcut map: Alt+Shift+M / Alt+Shift+S toggle a mute and nothing
//      else; Alt+M, Alt+S, Alt+D, Alt+L and a plain Shift+M / Shift+S keep
//      their meaning; a mute key is no command and no menu;
//   M  a music mute silences music only, restores the configured volume and
//      the context's song, never writes the volume, survives the System Menu,
//      Return to Title and a load, and is session-only (a reboot is unmuted);
//   X  the same for SFX, and the two buses never touch each other;
//   S  the Settings rows while muted, a Settings edit unmutes that bus only,
//      and a configured 0 % is not a mute;
//   A  stock assets: no music to mute, SFX mute still works;
//   D  a kill in combat plays only the 0x3564 hit burst (no 0x2fd0 burst).
//
// It uses only seams HEAD already had (raw keys, the recorder, the Settings
// view, the transcript), so native/core/tools/a3_05_red_first.py can build it
// against HEAD's device sources.
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/frontend_settings.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck
void batch37_reset_screen();

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
    };
    std::vector<Call> calls;
    void log(const char *w, int a = 0, int b = 0) { calls.push_back({w, a, b}); }
    bool play_sfx(const SfxRequest &r) override {
        log("sfx", int(r.id), int(r.gain_q15));
        return true;
    }
    void stop_sfx() override { log("stop_sfx"); }
    bool start_music(MusicSong s, uint16_t g) override {
        log("music", int(s), int(g));
        return true;
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
    /** The last gain handed to a channel (a set_gain or a start_music), -1 if none. */
    int last(const char *w) const {
        for (size_t i = calls.size(); i-- > 0;)
            if (calls[i].what == w) return calls[i].a;
        return -1;
    }
    int last_music_gain() const {
        for (size_t i = calls.size(); i-- > 0;)
            if (calls[i].what == "music") return calls[i].b;
        return -1;
    }
};

/** Writes settings.json before a Run boots, as the card would hold it. */
void card_settings(uint8_t sfx, uint8_t music) {
    tdeck::a3_host_settings_enabled() = true;
    FrontendSettings s{};
    s.sound_volume = sfx;
    s.music_volume = music;
    encode_settings(s, tdeck::a3_host_settings_text());
}

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    Recorder rec;
    Run(int seed, const AudioPackInfo &audio) {
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
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {80, 80};
        g.time.hour = 12;
        g.food = 80;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.level = 1;
        m.current_hp = m.max_hp = 30;
        g.rng.seed(uint32_t(seed));
        rt->configure_audio(audio, &rec);
        frames(3);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
        frames(1);
    }
    /** A key press as keyboard_matrix.cpp reports it: Shift upper-cases a letter. */
    void key(uint8_t code, bool alt = false, bool shift = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = shift && code >= 'a' && code <= 'z' ? uint8_t(code - 'a' + 'A') : code;
        e.base_code = code;
        e.modifiers.alt = alt;
        e.modifiers.shift = shift;
        raw(e);
    }
    void music_key() { key('m', true, true); } // Alt+Shift+M
    void sfx_key() { key('s', true, true); }   // Alt+Shift+S
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
    /** Alt+M -> Settings (root row 3), cursor on `row`. */
    void open_settings(int row = 0) {
        key('m', true);
        for (int i = 0; i < 3; ++i) down();
        key('\r');
        for (int i = 0; i < row; ++i) down();
    }
    void close_settings() {
        mic();
        key('m', true);
    }
    std::string line(int row) const {
        const auto v = rt->system_menu_view();
        return row >= 0 && size_t(row) < v.line_count && v.lines[row] ? v.lines[row] : "";
    }
    std::string transcript() const {
        std::string out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
        return out;
    }
    size_t lines_with(const char *needle) const {
        const auto t = transcript();
        size_t n = 0;
        for (size_t p = t.find(needle); p != std::string::npos; p = t.find(needle, p + 1)) ++n;
        return n;
    }
    /** A cue through the SAME sink the core's commands emit into. */
    void present(const GameEvent &e) {
        auto &ctx = rt->command_context_for_test();
        ctx.events.emit(ctx.events.context, e);
        frames(1);
    }
    void cue(const char *name) {
        GameEvent e{};
        e.kind = GameEventKind::Sfx;
        e.text = name;
        present(e);
    }
    size_t heard(SfxId id) {
        const size_t before = rec.sfx_of(id);
        cue(id == SfxId::MoveBlocked ? "move-blocked" : "move-step");
        return rec.sfx_of(id) - before;
    }
    MusicSong song() const { return rt->audio().current_song(); }
    /** Alt+S's own result line, whichever way the save went. */
    size_t save_lines() const { return lines_with("Save complete") + lines_with("Save failed"); }
};

AudioPackInfo g_real{}, g_stock{};
} // namespace

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    g_real = tdeck::load_audio_pack_info(argv[2]);
    g_stock = tdeck::load_audio_pack_info(argv[3]);
    const auto g70 = int(volume_to_gain_q15(70)), g40 = int(volume_to_gain_q15(40));

    // ---- K: the shortcut map ------------------------------------------------
    card_settings(40, 70);
    {
        Run h(5, g_real);
        const MusicSong playing = h.song();
        check(playing != MusicSong::None && h.rec.last_music_gain() == g70,
              "K0 control: the patched pack plays the context's song at the configured 70 %");
        const auto cmds = h.rt->routed_command_count();
        h.key('M', false, true); // Shift+M alone: a gameplay letter, not a mute
        h.key('\b');             // leave whatever prompt it opened
        h.key('S', false, true);
        h.key('\b');
        check(h.song() == playing && h.lines_with("muted") == 0 && h.rec.count("stop_music") == 0,
              "K1 Shift+M / Shift+S without Alt are ordinary keys: no mute");
        h.rec.calls.clear();
        h.music_key();
        check(!h.rt->system_menu_open() && h.song() == MusicSong::None && h.rec.count("stop_music") == 1,
              "K2 Alt+Shift+M is the music mute, not Alt+M's System Menu");
        const auto cmds_after = h.rt->routed_command_count();
        h.music_key();
        h.rec.calls.clear();
        h.sfx_key();
        check(h.save_lines() == 0 && h.rec.last("gain_sfx") == 0,
              "K3 Alt+Shift+S is the SFX mute, not Alt+S's Save");
        h.sfx_key();
        check(cmds_after == h.rt->routed_command_count() && cmds <= cmds_after,
              "K4 a mute key routes no game command");
        h.key('m', true);
        check(h.rt->system_menu_open(), "K5 control: Alt+M still opens the System Menu");
        h.key('m', true);
        const size_t saves = h.save_lines();
        h.key('s', true);
        check(h.save_lines() == saves + 1, "K5 control: Alt+S still takes the Save route");
        const size_t loads = h.lines_with("Load complete") + h.lines_with("No valid save");
        h.key('l', true, true);
        check(h.lines_with("Load complete") + h.lines_with("No valid save") == loads + 1 && h.song() == playing,
              "K6 control: any other Alt+Shift chord keeps its Alt meaning (Alt+Shift+L loads)");
    }

    // ---- M: music mute ------------------------------------------------------
    card_settings(40, 70);
    {
        Run h(5, g_real);
        const MusicSong playing = h.song();
        h.rec.calls.clear();
        h.music_key();
        check(h.song() == MusicSong::None && h.rec.count("stop_music") == 1 && h.rec.count("gain_music") == 0 &&
                  h.rt->audio().music_volume() == 70,
              "M1 Alt+Shift+M stops the song; the configured 70 % is untouched");
        check(h.lines_with("Music muted.") == 1, "M1 the transcript says 'Music muted.'");
        check(h.heard(SfxId::MoveBlocked) == 1 && h.rec.count("gain_sfx") == 0,
              "M2 SFX still sound while music is muted, at their own gain");
        h.frames(40);
        check(h.song() == MusicSong::None && h.rec.count("music") == 0,
              "M3 a muted context does not restart its song on its own");
        h.music_key();
        check(h.song() == playing && h.rec.count("music") == 1 && h.rec.last_music_gain() == g70,
              "M4 Alt+Shift+M again restores the context's song at 70 %, not 100 %");
        check(h.lines_with("Music restored.") == 1, "M4 the transcript says 'Music restored.'");
        for (int i = 0; i < 6; ++i) h.music_key();
        check(h.song() == playing && h.rt->audio().music_volume() == 70 && h.rec.last_music_gain() == g70 &&
                  h.lines_with("Music muted.") == 4 && h.lines_with("Music restored.") == 4,
              "M5 six more toggles: no drift, one line per toggle");
        // Survives the System Menu both ways.
        h.music_key();
        h.key('m', true);
        h.key('m', true);
        check(h.song() == MusicSong::None && h.rt->audio().music_volume() == 70,
              "M6 opening and closing the System Menu keeps the mute");
        const auto &text = tdeck::a3_host_settings_text();
        check(text.find("\"musicVolume\":70") != std::string::npos,
              "M6 closing the menu persists the configured 70 %, never a muted 0");
        h.key('m', true);
        const bool was_muted = h.song() == MusicSong::None;
        h.music_key();
        check(was_muted && h.rt->system_menu_open() && h.song() == playing,
              "M7 the toggle works with the System Menu open, and leaves it open");
        h.music_key();
        h.key('m', true);
        h.key('l', true); // a load
        check(h.song() == MusicSong::None, "M8 a load keeps the mute");
        h.music_key();
        check(h.song() != MusicSong::None && h.rec.last_music_gain() == g70, "M8 ... and it restores after the load");
        h.music_key(); // leave it muted for the reboot
    }
    {
        Run b(5, g_real); // a reboot, same card
        check(b.song() != MusicSong::None && b.rec.last_music_gain() == g70 && b.lines_with("muted") == 0,
              "M9 the mute is session-only: a reboot plays music at the configured 70 %");
    }

    // ---- X: SFX mute --------------------------------------------------------
    card_settings(40, 70);
    {
        Run h(5, g_real);
        const MusicSong playing = h.song();
        h.rec.calls.clear();
        h.sfx_key();
        check(h.rec.last("gain_sfx") == 0 && h.rec.count("gain_music") == 0 && h.rec.count("stop_music") == 0 &&
                  h.rt->audio().sfx_volume() == 40,
              "X1 Alt+Shift+S silences the SFX channel only; the configured 40 % is untouched");
        check(h.lines_with("SFX muted.") == 1, "X1 the transcript says 'SFX muted.'");
        const auto muted0 = h.rt->audio().stats().sfx_muted;
        check(h.heard(SfxId::MoveBlocked) == 0 && h.rt->audio().stats().sfx_muted == muted0 + 1,
              "X2 a cue while SFX are muted is not submitted");
        check(h.song() == playing, "X3 music keeps playing while SFX are muted");
        h.sfx_key();
        check(h.rec.last("gain_sfx") == g40 && h.heard(SfxId::MoveBlocked) == 1 &&
                  h.rec.calls.back().b == g40,
              "X4 Alt+Shift+S again restores 40 %, not 100 %, and cues sound at it");
        check(h.lines_with("SFX restored.") == 1, "X4 the transcript says 'SFX restored.'");
        for (int i = 0; i < 6; ++i) h.sfx_key();
        check(h.rec.last("gain_sfx") == g40 && h.rt->audio().sfx_volume() == 40 && h.song() == playing,
              "X5 six more toggles: no drift, music untouched");
        h.sfx_key();
        h.music_key();
        check(h.song() == MusicSong::None && h.rec.last("gain_sfx") == 0 && h.heard(SfxId::MoveBlocked) == 0,
              "X6 both muted at once");
        h.sfx_key();
        check(h.song() == MusicSong::None && h.heard(SfxId::MoveBlocked) == 1,
              "X6 unmuting SFX does not unmute music");
        h.music_key();
    }

    // ---- S: Settings while muted --------------------------------------------
    card_settings(40, 60);
    {
        Run h(5, g_real);
        h.music_key();
        h.open_settings(5);
        check(h.line(5) == "Music Volume: 60% (muted)" && h.line(4) == "SFX Volume: 40%",
              "S1 Settings shows the configured 60 % and marks the row muted");
        h.rec.calls.clear();
        h.right();
        check(h.line(5) == "Music Volume: 70%" && h.song() != MusicSong::None &&
                  h.rec.last_music_gain() == g70,
              "S2 a Music Volume edit while muted unmutes music at the new 70 %");
        h.up();
        h.sfx_key();
        check(h.line(4) == "SFX Volume: 40% (muted)" && h.line(5) == "Music Volume: 70%",
              "S3 the SFX mute from inside Settings marks only the SFX row");
        h.music_key();
        h.rec.calls.clear();
        h.left();
        check(h.line(4) == "SFX Volume: 30%" && h.rec.last("gain_sfx") == int(volume_to_gain_q15(30)) &&
                  h.line(5) == "Music Volume: 70% (muted)" && h.song() == MusicSong::None,
              "S4 an SFX edit unmutes SFX only; music stays muted");
        h.close_settings();
        check(tdeck::a3_host_settings_text().find("\"soundVolume\":30") != std::string::npos &&
                  tdeck::a3_host_settings_text().find("\"musicVolume\":70") != std::string::npos,
              "S5 settings.json holds the configured volumes, not the mute");
        h.music_key();
    }
    card_settings(0, 70);
    {
        Run h(5, g_real);
        h.open_settings(4);
        check(h.line(4) == "SFX Volume: 0%", "S6 a configured 0 % is shown as 0 %, not as muted");
        h.sfx_key();
        check(h.line(4) == "SFX Volume: 0% (muted)" && h.rt->audio().sfx_volume() == 0,
              "S6 muting a 0 % bus is a separate state on top of it");
        h.sfx_key();
        check(h.line(4) == "SFX Volume: 0%" && h.heard(SfxId::MoveBlocked) == 0,
              "S6 restoring it gives back 0 %, which stays silent");
        h.close_settings();
    }

    // ---- T: Return to Title and the title Settings -----------------------------
    card_settings(40, 70);
    {
        Run h(5, g_real);
        h.key('s', true); // a save for Journey Onward to continue
        h.music_key();
        const size_t lines = h.lines_with("Music ");
        h.key('m', true); // Alt+M > Return to Title
        h.up();
        h.key('\r');
        h.frames(5);
        check(h.song() == MusicSong::None, "T1 Return to Title keeps the mute: the title song is not started");
        h.key('x'); // title -> main menu
        for (int i = 0; i < 6; ++i) h.down();
        h.key('\r'); // Settings
        for (int i = 0; i < 5; ++i) h.down();
        h.rec.calls.clear();
        h.right();
        check(h.song() != MusicSong::None && h.rec.last_music_gain() == int(volume_to_gain_q15(80)),
              "T2 a title Settings Music Volume edit unmutes music at the new 80 %");
        h.music_key();
        check(h.song() == MusicSong::None && h.lines_with("Music ") == lines,
              "T3 the music mute works on the title screen (which has no transcript line)");
        h.mic();     // save Settings, back to the menu
        h.key('j');  // Journey Onward
        h.key('\r'); // Continue Latest
        h.frames(5);
        check(h.song() == MusicSong::None, "T4 back in the game the mute still holds");
        h.music_key();
        check(h.song() != MusicSong::None && h.rec.last_music_gain() == int(volume_to_gain_q15(80)),
              "T4 ... and restores at the 80 % set on the title");
    }

    // ---- A: stock assets ----------------------------------------------------
    card_settings(40, 70);
    {
        Run h(5, g_stock);
        h.music_key();
        check(h.rec.count("music") == 0 && h.rec.count("stop_music") == 0 &&
                  h.lines_with("Music muted.") == 0 && h.lines_with("Music unavailable") == 1,
              "A1 stock files: Alt+Shift+M does not pretend music exists");
        h.open_settings(5);
        check(h.line(5) == "Music Volume: Unavailable", "A1 ... and the Music row still reads Unavailable");
        h.close_settings();
        h.sfx_key();
        check(h.rec.last("gain_sfx") == 0 && h.heard(SfxId::MoveBlocked) == 0 && h.lines_with("SFX muted.") == 1,
              "A2 stock files: the SFX mute works");
        h.sfx_key();
        check(h.heard(SfxId::MoveBlocked) == 1, "A2 ... and restores");
    }

    // ---- D: a kill in combat -------------------------------------------------
    tdeck::a3_host_settings_text().clear();
    {
        Run h(5, g_real);
        auto &cs = h.rt->combat_state_for_test();
        cs.count = 2;
        cs.actors[0].id = 1;
        cs.actors[0].member = 0;
        cs.actors[1].id = 7;
        cs.actors[1].member = 255;
        auto combat = [&](CombatEventKind k, int32_t actor, int32_t target, int8_t hit, const char *text) {
            CombatEvent ce{};
            ce.kind = k;
            ce.actor = actor;
            ce.target = target;
            ce.hit = hit;
            ce.text = text;
            GameEvent e{};
            e.kind = GameEventKind::Combat;
            e.combat = &ce;
            h.present(e);
        };
        h.rec.calls.clear();
        // What combat.cpp damage() emits for a killing blow: Attacked, then kill()'s Died.
        combat(CombatEventKind::Attacked, 1, 7, 1, nullptr);
        combat(CombatEventKind::Died, 1, 7, -1, "Troll killed!");
        check(h.rec.sfx_of(SfxId::CombatHit) == 1, "D1 a kill on an enemy plays the 0x3564 enemy burst once");
        check(h.rec.sfx_of(SfxId::CombatDefeat) == 0 && h.rec.count("sfx") == 1,
              "D2 ... and no second (0x2fd0 chest-trap) burst");
        h.rec.calls.clear();
        combat(CombatEventKind::Attacked, 7, 1, 1, nullptr);
        combat(CombatEventKind::Died, 7, 1, -1, "Avatar killed!");
        check(h.rec.sfx_of(SfxId::CombatHitHeavy) == 1 && h.rec.count("sfx") == 1,
              "D3 a party member's death: the party burst once, nothing more");
        h.rec.calls.clear();
        combat(CombatEventKind::Died, 1, 7, -1, "Troll killed!");
        check(h.rec.count("sfx") == 0, "D4 a lone Died is silent");
    }

    std::printf("a3_05_audio_controls: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
