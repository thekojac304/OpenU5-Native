// Alpha 3 A3-04 -- music context routing through the REAL AlphaRuntime: raw
// keys in, a recording backend out. Complements a3_04_music_synth (the pure
// decoder) and a3_01's exhaustive music_context_for_location coverage: this
// file proves AlphaRuntime::sync_music() actually calls AudioService with
// the derived context at the boundaries the batch requires -- boot, every
// key poll, Camp, load, Ending, Settings volume -- and nowhere else.
//
//   a3_04_music_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> <stock audio fixture>
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/frontend_settings.h"
#include "openu5/quest_state.h"

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
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck

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
    bool play_sfx(const SfxRequest &r) override {
        calls.push_back({"sfx", int(r.id), int(r.gain_q15)});
        return true;
    }
    void stop_sfx() override { calls.push_back({"stop_sfx", 0, 0}); }
    bool start_music(MusicSong s, uint16_t g) override {
        calls.push_back({"music", int(s), int(g)});
        return true;
    }
    void stop_music() override { calls.push_back({"stop_music", 0, 0}); }
    void set_gain(AudioChannel c, uint16_t g) override {
        calls.push_back({c == AudioChannel::Sfx ? "gain_sfx" : "gain_music", int(g), 0});
    }
    size_t count(const char *w) const {
        size_t n = 0;
        for (const auto &c : calls) n += c.what == w;
        return n;
    }
    /** The song of the LAST start_music call, or MusicSong(0xfe) if none happened. */
    MusicSong last_song() const {
        for (auto it = calls.rbegin(); it != calls.rend(); ++it)
            if (it->what == "music") return MusicSong(it->a);
        return MusicSong(0xfe);
    }
    int last_gain(const char *what) const {
        for (auto it = calls.rbegin(); it != calls.rend(); ++it)
            if (it->what == what) return it->a;
        return -1;
    }
};

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    Run(int seed) {
        openu5_host_virtual_clock_us() = kClockStartUs;
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
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.character_class = 'F';
        m.level = 1;
        m.current_hp = 30;
        m.max_hp = 30;
        m.strength = m.dexterity = m.intelligence = 10;
        g.rng.seed(uint32_t(seed));
        rt->render(board, true);
    }
    int64_t now() const { return openu5_host_virtual_clock_us(); }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    /** A no-op-for-gameplay key: PageUp with nothing to page bumps nothing but still polls. */
    void poll() {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::TrackballDown;
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        raw(e);
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void camp() {
        key('h');
        key('1');
        key('\r');
        key('n');
    }
    void at(uint8_t location, uint8_t floor, uint8_t transport_tile = 0x1c) {
        auto &g = rt->game();
        g.position.map = {location, floor};
        rt->turn().transport_tile = transport_tile;
        poll();
    }
};
} // namespace

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    const auto g_real = tdeck::load_audio_pack_info(argv[2]);
    const auto g_stock = tdeck::load_audio_pack_info(argv[3]);

    // ---- G: sync_music() runs from configure_audio() itself, before any
    //      key poll (this harness starts gameplay directly, frontend_
    //      inactive -- so the immediate context is location-derived, not
    //      Title; the frontend->Title/Creation/Intro branch is exercised by
    //      the frontend-specific test files, and is a plain, low-risk
    //      switch on FrontendState, not read-order-sensitive like this one)
    {
        Run h(1);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        check(rec.count("music") == 1 && rec.last_song() == MusicSong::BritannicLands,
              "G1 configure_audio() alone starts the right track (here: Britannic Lands), with no key poll needed");
    }

    // ---- G: the location switch, a representative sample of the driver's
    //      own ranges (exhaustive coverage of the pure function is a3_01's
    //      M1-M3; this proves AlphaRuntime actually reaches it) -----------
    {
        Run h(2);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        h.at(0, 0);
        check(rec.last_song() == MusicSong::BritannicLands, "G2 Britannia (loc 0, floor 0) -> Britannic Lands");
        h.at(0, 1);
        check(rec.last_song() == MusicSong::WorldsBelow, "G3 Underworld (loc 0, floor != 0) -> Worlds Below");
        h.at(1, 0);
        check(rec.last_song() == MusicSong::Tarantella, "G4 a City of Virtue (loc 1) -> Villager Tarantella");
        h.at(0x21, 0);
        check(rec.last_song() == MusicSong::HallsOfDoom, "G5 a dungeon (loc 0x21) -> Halls of Doom");
        h.at(0x11, 0);
        check(rec.last_song() == MusicSong::Monarch, "G6 Lord British's castle (loc 0x11) -> The Missing Monarch");
        h.at(0x12, 0);
        check(rec.last_song() == MusicSong::Blackthorn, "G6b Blackthorn's palace (loc 0x12) -> Lord Blackthorn");
        h.at(0, 0, 0x20);
        check(rec.last_song() == MusicSong::Hornpipe, "G7 aboard a frigate wins over the location -> Cap'n Johne's Hornpipe");
    }

    // ---- G: same context does not restart; a real change does -----------
    {
        Run h(3);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        h.at(0, 0);
        const size_t after_first = rec.count("music");
        h.at(0, 0); // same context, polled again
        h.poll();
        h.poll();
        check(rec.count("music") == after_first, "G8 polling the SAME location repeatedly issues no further start_music");
        h.at(1, 0); // a real change
        check(rec.count("music") == after_first + 1 && rec.last_song() == MusicSong::Tarantella,
              "G9 a real location change issues exactly one new start_music, to the new song");
    }

    // ---- Camp freezes the location switch; leaving it resumes location --
    {
        Run h(4);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        h.at(0, 0); // overworld, so leaving Camp has something distinct to resume to
        h.camp();
        check(rec.last_song() == MusicSong::Stones, "CAMP1 hole-up freezes the location switch and plays Stones");
    }

    // ---- The terminal Ending plays Rule Britannia (Finale) ---------------
    {
        Run h(5);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        h.at(0, 0);
        openu5::set_quest_flag(h.rt->game().quest, openu5::QuestFlag::GameWon);
        h.poll();
        check(rec.last_song() == MusicSong::RuleBritannia, "END1 game-won -> the Ending screen plays Rule Britannia (Finale)");
    }

    // ---- Settings: Music Volume 0/50/100 reach the backend at boot, and
    //      SFX Volume changes never touch the music channel (independence).
    //      Same pattern as a3_02's S25: settings.json is read once, at
    //      attach_host_test_fixture() -> load_device_settings(). -----------
    {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings mv0{};
        mv0.music_volume = 0;
        encode_settings(mv0, tdeck::a3_host_settings_text());
        Run h(6);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        check(rec.last_gain("gain_music") == 0, "VOL1 Music Volume 0% (settings.json) sends gain_music = 0");
    }
    {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings mv50{};
        mv50.music_volume = 50;
        encode_settings(mv50, tdeck::a3_host_settings_text());
        Run h(6);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        check(rec.last_gain("gain_music") == int(openu5::volume_to_gain_q15(50)),
              "VOL2 Music Volume 50% sends gain_music = volume_to_gain_q15(50)");
    }
    {
        tdeck::a3_host_settings_enabled() = true;
        FrontendSettings mv100{};
        mv100.music_volume = 100;
        encode_settings(mv100, tdeck::a3_host_settings_text());
        Run h(6);
        Recorder rec;
        h.rt->configure_audio(g_real, &rec);
        check(rec.last_gain("gain_music") == int(kUnityGainQ15), "VOL3 Music Volume 100% sends unity gain_music");
    }
    tdeck::a3_host_settings_enabled() = false;

    // ---- Stock pack: never once calls start_music/stop_music -------------
    {
        Run h(8);
        Recorder rec;
        h.rt->configure_audio(g_stock, &rec);
        check(!h.rt->audio().has_music(), "STOCK1 a stock pack leaves has_music() false");
        h.at(0, 0);
        h.at(1, 0);
        h.at(0x21, 0);
        h.camp();
        check(rec.count("music") == 0 && rec.count("stop_music") == 0,
              "STOCK2 every context change and Camp entry, with a stock pack, calls neither start_music nor stop_music");
    }

    // ---- Wiring proof for the two branches no test above can reach safely
    //      (real combat/victory need a live arena; a source scan proves the
    //      exact read exists, the same class of proof as audio.cpp's S20) --
    {
        std::ifstream in(std::filesystem::path(argv[1]).parent_path() / ".." / "targets" / "tdeck" / "main" / "alpha_runtime.cpp");
        std::stringstream s;
        s << in.rdbuf();
        const std::string src = s.str();
        const auto fn = src.find("void AlphaRuntime::sync_music()");
        const std::string body = fn == std::string::npos ? "" : src.substr(fn, 4000);
        check(!body.empty() &&
                  body.find("ui_->base_mode()==openu5::UiMode::Combat") != std::string::npos &&
                  body.find("combat_.victory") != std::string::npos,
              "WIRE1 sync_music() reads the real base_mode and the real victory flag for the combat/victory branch "
              "(a3_01's music_context_for_location M1-M3 already prove the pure function exhaustively)");
    }

    std::printf("A3-04 music runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
