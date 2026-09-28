// Alpha 3 A3-05 -- the pure halves of the session mutes (ALPHA3_AUDIO.md
// section 29): AudioService's mute flags over a recording backend, the
// Settings row text, and the volume-edit report of the System Menu's and the
// title's Settings pages. The device wiring (Alt+Shift+M / Alt+Shift+S, the
// transcript, the unmute on edit) is a3_05_audio_controls.
//
//   a3_05_audio_mute
#include "openu5/audio.h"
#include "openu5/frontend.h"
#include "openu5/system_menu.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}

struct Recorder final : AudioBackend {
    std::vector<std::string> calls;
    std::vector<int> args;
    void log(const char *w, int a = 0) {
        calls.push_back(w);
        args.push_back(a);
    }
    bool play_sfx(const SfxRequest &r) override {
        log("sfx", int(r.gain_q15));
        return true;
    }
    void stop_sfx() override { log("stop_sfx"); }
    bool start_music(MusicSong, uint16_t g) override {
        log("music", int(g));
        return true;
    }
    void stop_music() override { log("stop_music"); }
    void set_gain(AudioChannel c, uint16_t g) override { log(c == AudioChannel::Sfx ? "gain_sfx" : "gain_music", int(g)); }
    size_t count(const char *w) const {
        size_t n = 0;
        for (const auto &c : calls) n += c == w;
        return n;
    }
    int last(const char *w) const {
        for (size_t i = calls.size(); i-- > 0;)
            if (calls[i] == w) return args[i];
        return -1;
    }
    void clear() { calls.clear(), args.clear(); }
};

UiAction act(UiActionKind k) {
    UiAction a{};
    a.kind = k;
    return a;
}
UiAction dir(Direction d) {
    UiAction a = act(UiActionKind::Direction);
    a.direction = d;
    return a;
}
} // namespace

int main() {
    const int g40 = volume_to_gain_q15(40), g50 = volume_to_gain_q15(50), g70 = volume_to_gain_q15(70);

    // ---- A: AudioService ----------------------------------------------------
    {
        Recorder rec;
        AudioService a;
        a.set_sfx_volume(40);
        a.set_music_volume(70);
        a.set_music_availability(MusicAvailability::Available);
        a.attach(&rec);
        a.play_music(MusicContext::Title);
        check(a.current_song() != MusicSong::None && rec.last("music") == g70 && !a.sfx_muted() && !a.music_muted(),
              "A0 control: nothing is muted at start; the song plays at 70 %");
        rec.clear();
        a.set_sfx_muted(true);
        a.set_sfx_muted(true);
        check(rec.count("gain_sfx") == 1 && rec.last("gain_sfx") == 0 && rec.calls.size() == 1 && a.sfx_volume() == 40,
              "A1 an SFX mute is one SFX gain of 0; the volume stays 40 and music is not touched");
        const auto muted0 = a.stats().sfx_muted;
        a.play_sfx(SfxId::MoveBlocked);
        check(rec.count("sfx") == 0 && a.stats().sfx_muted == muted0 + 1, "A2 a muted SFX request is dropped and counted");
        a.set_sfx_volume(50);
        check(rec.last("gain_sfx") == 0 && a.sfx_volume() == 50, "A3 a volume change while muted keeps the gain at 0");
        a.set_sfx_muted(false);
        a.play_sfx(SfxId::MoveBlocked);
        check(rec.last("gain_sfx") == g50 && rec.last("sfx") == g50, "A4 the unmute restores the volume (50 %), not 100 %");
        rec.clear();
        a.set_music_muted(true);
        check(rec.calls.size() == 1 && rec.calls[0] == "stop_music" && a.current_song() == MusicSong::None &&
                  a.music_volume() == 70 && a.current_music_context() == MusicContext::Title,
              "A5 a music mute stops the song only; volume and context are kept");
        a.play_music(MusicContext::Title);
        a.set_music_volume(50);
        check(rec.count("music") == 0, "A6 no song starts while music is muted, whatever the context or volume");
        a.set_music_muted(false);
        check(a.current_song() != MusicSong::None && rec.last("music") == g50 && rec.count("gain_sfx") == 0,
              "A7 the music unmute restarts the context's song at the current volume, SFX untouched");
        Recorder other;
        a.set_sfx_muted(true);
        a.attach(&other);
        check(other.last("gain_sfx") == 0 && other.last("gain_music") == g50,
              "A8 a newly attached backend gets the muted SFX gain and the music volume");
        a.set_sfx_volume(0);
        a.set_sfx_muted(false);
        check(!a.sfx_muted() && a.sfx_volume() == 0 && other.last("gain_sfx") == 0,
              "A9 a configured 0 % is not a mute: unmuting it leaves it silent at 0 %");
    }
    {
        Recorder rec;
        AudioService a;
        a.set_music_availability(MusicAvailability::StockNoMusic);
        a.attach(&rec);
        rec.clear();
        a.set_music_muted(true);
        a.set_music_muted(false);
        check(rec.calls.empty(), "A10 without music a music mute reaches nothing");
    }

    // ---- R: the rows --------------------------------------------------------
    {
        char b[64];
        format_sfx_volume_row(b, sizeof b, 40, true);
        const bool sfx = std::strcmp(b, "SFX Volume: 40% (muted)") == 0;
        format_sfx_volume_row(b, sizeof b, 40);
        const bool plain = std::strcmp(b, "SFX Volume: 40%") == 0;
        format_music_volume_row(b, sizeof b, 100, MusicAvailability::Available, true);
        const bool music = std::strcmp(b, "Music Volume: 100% (muted)") == 0;
        format_music_volume_row(b, sizeof b, 70, MusicAvailability::StockNoMusic, true);
        const bool stock = std::strcmp(b, "Music Volume: Unavailable") == 0;
        check(sfx && plain && music && stock,
              "R1 rows: '(muted)' after the configured value; Unavailable stays Unavailable");
    }

    // ---- M: the System Menu's Settings page ----------------------------------
    {
        SystemMenuSession m;
        m.set_music_availability(MusicAvailability::Available);
        FrontendSettings s{};
        s.sound_volume = 40;
        s.music_volume = 70;
        FrontendSaveSlot slots[2]{};
        m.open(s, slots);
        for (int i = 0; i < 3; ++i) m.handle(act(UiActionKind::Next));
        m.handle(act(UiActionKind::Confirm));
        m.set_audio_mutes(false, true);
        auto v = m.view();
        check(std::strcmp(v.lines[4], "SFX Volume: 40%") == 0 && std::strcmp(v.lines[5], "Music Volume: 70% (muted)") == 0,
              "M1 the System Menu rows show the session mutes");
        for (int i = 0; i < 4; ++i) m.handle(act(UiActionKind::Next));
        check(m.take_volume_edits() == 0, "M2 moving the cursor edits nothing");
        m.handle(dir(Direction::East));
        check(m.take_volume_edits() == kSfxVolumeEdited && m.take_volume_edits() == 0 && m.settings().sound_volume == 50,
              "M3 an SFX Volume key reports the SFX row once");
        m.handle(act(UiActionKind::Next));
        m.handle(dir(Direction::West));
        check(m.take_volume_edits() == kMusicVolumeEdited && m.settings().music_volume == 60,
              "M4 a Music Volume key reports the Music row");
        m.handle(act(UiActionKind::Previous));
        m.handle(act(UiActionKind::Previous));
        m.handle(dir(Direction::East));
        check(m.take_volume_edits() == 0, "M5 another row's key reports no volume edit");
        SystemMenuSession stock;
        stock.set_music_availability(MusicAvailability::StockNoMusic);
        stock.open(s, slots);
        for (int i = 0; i < 3; ++i) stock.handle(act(UiActionKind::Next));
        stock.handle(act(UiActionKind::Confirm));
        for (int i = 0; i < 5; ++i) stock.handle(act(UiActionKind::Next));
        stock.handle(dir(Direction::East));
        check(stock.take_volume_edits() == 0, "M6 without music the Music row's key edits nothing and reports nothing");
    }

    // ---- T: the title's Settings page -----------------------------------------
    {
        FrontendSession t;
        t.set_music_availability(MusicAvailability::Available);
        FrontendSettings s{};
        s.sound_volume = 40;
        s.music_volume = 70;
        t.start(0, false, s);
        UiAction key_s = act(UiActionKind::Character);
        key_s.character = u's';
        t.handle(act(UiActionKind::Confirm), 1);
        t.handle(key_s, 2);
        t.set_audio_mutes(true, false);
        auto v = t.view();
        check(t.state() == FrontendState::Settings && std::strcmp(v.lines[4], "SFX Volume: 40% (muted)") == 0 &&
                  std::strcmp(v.lines[5], "Music Volume: 70%") == 0,
              "T1 the title rows show the session mutes");
        for (int i = 0; i < 5; ++i) t.handle(act(UiActionKind::Next), 3);
        t.handle(dir(Direction::East), 4);
        check(t.take_volume_edits() == kMusicVolumeEdited && t.take_volume_edits() == 0,
              "T2 a title Music Volume key reports the Music row once");
        t.handle(act(UiActionKind::Previous), 5);
        t.handle(dir(Direction::West), 6);
        check(t.take_volume_edits() == kSfxVolumeEdited, "T3 a title SFX Volume key reports the SFX row");
    }

    std::printf("a3_05_audio_mute: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
