// Alpha 3 A3-01 -- the audio contract in the portable core: the semantic
// vocabulary, the music-context selector, the service's channel/volume/
// capability policy, the audio-pack reader and the two Settings rows.
//
//   a3_01_audio_contract <game/src/core/sfx.ts> <game/src/ui/music.ts>
//                        <native/core> <stock fixture> <real audio pack>
//
// The real audio pack is built on the developer's machine from the user's own
// files (npm run pack:audio) and is never committed; the stock fixture is
// 112 bytes of pure format and is committed.
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/frontend.h"
#include "openu5/frontend_settings.h"
#include "openu5/persistence.h"
#include "openu5/system_menu.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *label) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "GREEN" : "RED", label);
}
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::vector<uint8_t> bytes_of(const std::string &path) {
    const auto s = slurp(path);
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---- a recording backend: the host's deterministic stand-in for I2S --------
struct Call {
    std::string what;
    int a = 0, b = 0, c = 0;
};
struct Recorder final : AudioBackend {
    std::vector<Call> calls;
    bool refuse_sfx = false, refuse_music = false;
    bool play_sfx(const SfxRequest &r) override {
        calls.push_back({"sfx", int(r.id), int(r.param), int(r.gain_q15)});
        return !refuse_sfx;
    }
    void stop_sfx() override { calls.push_back({"stop_sfx"}); }
    bool start_music(MusicSong s, uint16_t gain) override {
        calls.push_back({"music", int(s), int(gain)});
        return !refuse_music;
    }
    void stop_music() override { calls.push_back({"stop_music"}); }
    void set_gain(AudioChannel ch, uint16_t gain) override {
        calls.push_back({ch == AudioChannel::Sfx ? "gain_sfx" : "gain_music", int(gain)});
    }
    size_t count(const char *w) const {
        size_t n = 0;
        for (const auto &c : calls) n += c.what == w;
        return n;
    }
};

// ---- a tiny independent OU5AUDIO writer, for crafted packs ------------------
struct Entry {
    std::string name;
    uint32_t kind = 0, id = 0;
    std::vector<uint8_t> data;
};
void put16(std::vector<uint8_t> &b, size_t at, uint16_t v) { b[at] = uint8_t(v); b[at + 1] = uint8_t(v >> 8); }
void put32(std::vector<uint8_t> &b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + size_t(i)] = uint8_t(v >> (8 * i));
}
std::vector<uint8_t> write_pack(const std::vector<Entry> &entries, uint16_t major = 1) {
    const size_t table_end = 32 + 48 * entries.size();
    size_t size = table_end;
    for (const auto &e : entries) size += e.data.size();
    std::vector<uint8_t> b(size, 0);
    std::memcpy(b.data(), "OU5AUDIO", 8);
    put16(b, 8, major);
    put16(b, 10, 0);
    put16(b, 12, 32);
    put16(b, 14, 48);
    put32(b, 16, uint32_t(entries.size()));
    put32(b, 20, uint32_t(size));
    size_t at = table_end;
    for (size_t i = 0; i < entries.size(); ++i) {
        const size_t row = 32 + 48 * i;
        std::memcpy(b.data() + row, entries[i].name.c_str(), entries[i].name.size());
        put32(b, row + 24, entries[i].kind);
        put32(b, row + 28, entries[i].id);
        put32(b, row + 32, uint32_t(at));
        put32(b, row + 36, uint32_t(entries[i].data.size()));
        put32(b, row + 40, save::save_crc32(entries[i].data.data(), entries[i].data.size()));
        std::memcpy(b.data() + at, entries[i].data.data(), entries[i].data.size());
        at += entries[i].data.size();
    }
    put32(b, 24, save::save_crc32(b.data() + table_end, size - table_end));
    put32(b, 28, save::save_crc32(b.data() + 32, table_end - 32));
    return b;
}
std::vector<uint8_t> record(MusicCapability c, uint8_t driver, uint8_t bank, uint16_t present, uint16_t valid) {
    std::vector<uint8_t> r(32, 0);
    r[0] = 1;
    r[1] = uint8_t(c);
    r[2] = driver;
    r[3] = bank;
    put16(r, 4, present);
    put16(r, 6, valid);
    put32(r, 12, 1);
    return r;
}
std::vector<uint8_t> iff(const char *id, const std::vector<uint8_t> &body) {
    std::vector<uint8_t> out(8, 0);
    std::memcpy(out.data(), id, 4);
    const auto n = uint32_t(body.size());
    out[4] = uint8_t(n >> 24); out[5] = uint8_t(n >> 16); out[6] = uint8_t(n >> 8); out[7] = uint8_t(n);
    out.insert(out.end(), body.begin(), body.end());
    if (n & 1) out.push_back(0);
    return out;
}
std::vector<uint8_t> join(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
std::vector<uint8_t> tag(const char *t) { return std::vector<uint8_t>(t, t + 4); }
std::vector<uint8_t> shell_xmi(uint16_t sequences = 1, bool with_events = true) {
    const auto xdir = iff("FORM", join(tag("XDIR"), iff("INFO", {uint8_t(sequences), uint8_t(sequences >> 8)})));
    const auto body = with_events ? iff("EVNT", {0xff, 0x2f, 0x00}) : iff("TIMB", {0, 0});
    const auto xmid = iff("FORM", join(tag("XMID"), body));
    return join(xdir, iff("CAT ", join(tag("XMID"), xmid)));
}
std::vector<uint8_t> shell_bank() {
    std::vector<uint8_t> b(22, 0);
    put32(b, 2, 8);
    b[6] = 0xff; b[7] = 0xff;
    put16(b, 8, 14);
    return b;
}
std::vector<Entry> supported_entries(const std::vector<uint8_t> &song = shell_xmi()) {
    std::vector<Entry> e{{"capability", 0, 0, record(MusicCapability::SupportedMusicPatch, 1, 1, 0xffff, 0xffff)}};
    for (uint32_t i = 0; i < 16; ++i) e.push_back({"song", 1, i, song});
    e.push_back({"bank", 2, 0, shell_bank()});
    return e;
}

/** Every "..." literal of `line` that looks like a cue id (lower case words and dashes). */
std::vector<std::string> cue_literals(const std::string &line) {
    static const std::regex lit("\"([a-z][a-z0-9]*(?:-[a-z0-9]+)*)\"");
    std::vector<std::string> out;
    for (std::sregex_iterator it(line.begin(), line.end(), lit), end; it != end; ++it) out.push_back((*it)[1]);
    return out;
}
std::string strip_line_comments(const std::string &text) {
    std::string out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        const auto at = line.find("//");
        out += (at == std::string::npos ? line : line.substr(0, at)) + "\n";
    }
    return out;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 6) {
        std::printf("usage: a3_01_audio_contract <sfx.ts> <music.ts> <native/core> <stock fixture> <real audio pack>\n");
        return 2;
    }
    const std::string sfx_ts = slurp(argv[1]), music_ts = slurp(argv[2]), core_dir = argv[3];

    // ======================================================================
    // V -- the semantic SFX vocabulary
    // ======================================================================
    {
        bool round_trip = true;
        std::set<std::string> seen;
        for (size_t i = 1; i < kSfxIdCount; ++i) {
            const char *cue = sfx_cue(SfxId(i));
            round_trip = round_trip && cue && sfx_from_cue(cue) == SfxId(i) && seen.insert(cue).second;
        }
        check(round_trip, "V1 every SfxId has one unique cue string and the string maps back to it");
        check(sfx_from_cue(nullptr) == SfxId::None && sfx_from_cue("") == SfxId::None &&
                  sfx_from_cue("not-a-cue") == SfxId::None,
              "V2 null, empty and unknown cue text map to SfxId::None");
    }
    {
        // game/src/core/sfx.ts `export type SfxId = | "combat-hit" ...;` in declaration order.
        std::vector<std::string> ts;
        const auto begin = sfx_ts.find("export type SfxId =");
        const auto end = sfx_ts.find("\nexport ", begin + 1);
        std::istringstream in(sfx_ts.substr(begin, end - begin));
        static const std::regex member("^\\s*\\|\\s*\"([a-z0-9-]+)\"");
        for (std::string line; std::getline(in, line);) {
            std::smatch m;
            if (std::regex_search(line, m, member)) ts.push_back(m[1]);
        }
        bool same = ts.size() == kReferenceSfxCount;
        for (size_t i = 0; same && i < ts.size(); ++i) same = ts[i] == sfx_cue(SfxId(i + 1));
        check(begin != std::string::npos && same,
              "V3 drift: the first 52 SfxIds are the reference catalogue (game/src/core/sfx.ts), in its order");
        bool origins = true;
        for (size_t i = 1; i <= kReferenceSfxCount; ++i) origins = origins && sfx_origin(SfxId(i)) == SfxOrigin::Original;
        origins = origins && sfx_origin(SfxId::SpellCast) == SfxOrigin::NativeHook &&
                  sfx_origin(SfxId::InvalidMagic) == SfxOrigin::NativeHook &&
                  sfx_origin(SfxId::DiagnosticTone) == SfxOrigin::Diagnostic;
        check(origins, "V3 reference ids are Original; spell-cast..invalid-magic are native hooks; the tone is diagnostic");
    }
    {
        // Every cue the native core can emit must be in the vocabulary: scan the
        // sources for cue literals on the lines that emit them.
        std::set<std::string> emitted, unknown;
        const std::filesystem::path root(core_dir);
        for (const auto &dir : {root / "src", root / "include" / "openu5"}) {
            for (const auto &f : std::filesystem::directory_iterator(dir)) {
                const auto ext = f.path().extension().string();
                if (ext != ".cpp" && ext != ".h") continue;
                const auto name = f.path().filename().string();
                // The tables themselves: the vocabulary (A3-01) and the A3-02
                // synthesizer, whose class names sit on SfxClass lines.
                if (name == "audio.cpp" || name == "audio.h" || name == "sfx_synth.cpp" || name == "sfx_synth.h") continue;
                std::istringstream in(slurp(f.path().string()));
                for (std::string line; std::getline(in, line);) {
                    const bool emits = line.find("Sfx") != std::string::npos || line.find("sound(\"") != std::string::npos ||
                                       line.find("ceremony(") != std::string::npos || line.find("Cue[] =") != std::string::npos ||
                                       line.find("RefugeBeat") != std::string::npos || line.find("\"refuge-") != std::string::npos;
                    if (!emits) continue;
                    // RefugeBeat{scene, message, sfx, delay}: only the third field is a cue.
                    static const std::regex beat(R"re(\{\s*(?:nullptr|"[^"]*")\s*,\s*(?:nullptr|"[^"]*")\s*,\s*"([a-z0-9-]+)")re");
                    std::vector<std::string> found;
                    if (line.find("\"refuge-") != std::string::npos) {
                        for (std::sregex_iterator it(line.begin(), line.end(), beat), end; it != end; ++it) found.push_back((*it)[1]);
                    } else {
                        found = cue_literals(line);
                    }
                    for (const auto &c : found) {
                        emitted.insert(c);
                        if (sfx_from_cue(c.c_str()) == SfxId::None) unknown.insert(c);
                    }
                }
            }
        }
        const char *expected[] = {"cannon-fire", "waterfall-fall", "moongate", "quake", "move-blocked", "move-step",
                                  "dungeon-trap", "spell-cast", "potion-used", "scroll-used", "ring-vanishes",
                                  "combat-damage", "field-afflict", "dungeon-fail", "dungeon-zap", "invalid-magic",
                                  "torch-borrowed", "sceptre", "shard-sweep", "victory-fanfare", "instrument-note",
                                  "shadowlord-announce", "apparition-materialize", "apparition-arpeggio",
                                  "apparition-heal-chime", "apparition-chord", "shrine-donation", "shrine-ordained",
                                  "shrine-well-done", "mirror-break", "refuge-thunder"};
        bool all = true;
        for (const char *e : expected) all = all && emitted.count(e) && sfx_from_cue(e) != SfxId::None;
        for (const auto &u : unknown) std::printf("  unknown cue literal: %s\n", u.c_str());
        check(unknown.empty(), "V4 every cue literal on an emitting line of native/core maps to an SfxId (closed vocabulary)");
        check(all, "V4 the scan finds all 31 cue ids the core is known to emit (a positive control on the scan)");
    }

    // ======================================================================
    // M -- music songs and contexts
    // ======================================================================
    {
        // game/src/ui/music.ts: the MusicContext union order and CONTEXT_SONG.
        std::vector<std::string> names;
        const auto u0 = music_ts.find("export type MusicContext =");
        const auto u1 = music_ts.find(';', u0);
        static const std::regex member("\\|\\s*\"([a-z-]+)\"");
        const auto unions = music_ts.substr(u0, u1 - u0);
        for (std::sregex_iterator it(unions.begin(), unions.end(), member), end; it != end; ++it) names.push_back((*it)[1]);
        bool order = names.size() == kMusicContextCount;
        for (size_t i = 0; order && i < names.size(); ++i) order = names[i] == music_context_name(MusicContext(i));
        check(order, "M1 drift: MusicContext is music.ts's union, in its order");

        std::map<std::string, int> songs;
        const auto t0 = music_ts.find("const CONTEXT_SONG");
        const auto t1 = music_ts.find("};", t0);
        const auto table = music_ts.substr(t0, t1 - t0);
        static const std::regex row("\\n\\s*\"?([a-z-]+)\"?:\\s*(0x[0-9a-f]+|null),");
        for (std::sregex_iterator it(table.begin(), table.end(), row), end; it != end; ++it)
            songs[(*it)[1]] = (*it)[2] == "null" ? 0xff : int(std::stoul((*it)[2], nullptr, 16));
        bool table_ok = songs.size() == kMusicContextCount;
        for (size_t i = 0; table_ok && i < kMusicContextCount; ++i) {
            const auto hit = songs.find(music_context_name(MusicContext(i)));
            table_ok = hit != songs.end() && hit->second == int(song_for_context(MusicContext(i)));
        }
        check(table_ok, "M1 drift: song_for_context is music.ts CONTEXT_SONG, row for row (26 rows)");
        bool titles = true;
        for (size_t s = 0; s < kMusicSongCount; ++s) titles = titles && std::strlen(music_song_title(MusicSong(s))) > 0;
        check(titles && std::strcmp(music_song_title(MusicSong::RuleBritannia), "Rule Britannia") == 0 &&
                  std::strcmp(music_song_title(MusicSong::None), "") == 0,
              "M1 the 16 songs carry the patch's Files.txt titles");
    }
    {
        // mid.drv 0x016d, exhaustively over g_location (music-location-mapping.md section 3).
        auto expected = [](int loc, int floor) {
            if (loc == 0) return floor == 0 ? MusicContext::Overworld : MusicContext::Underworld;
            if (loc <= 8) return MusicContext::Cities;
            if (loc <= 0x0c) return MusicContext::Lighthouse;
            if (loc <= 0x10) return MusicContext::Hut;
            if (loc == 0x11) return MusicContext::Castle;
            if (loc == 0x12) return MusicContext::BlackthornPalace;
            if (loc <= 0x18) return MusicContext::Village;
            if (loc <= 0x1d) return MusicContext::Keep;
            if (loc <= 0x20) return MusicContext::Principle;
            if (loc <= 0x28) return MusicContext::Dungeon;
            return MusicContext::Silence;
        };
        bool all = true;
        for (int loc = 0; loc < 256; ++loc)
            for (int floor : {0, 0xff}) {
                LocationMusicInput in{};
                in.location = uint8_t(loc);
                in.floor = uint8_t(floor);
                in.transport_tile = 0x1c; // on foot
                all = all && music_context_for_location(in) == expected(loc, floor);
            }
        check(all, "M2 the location switch matches the driver for every g_location 0x00..0xff");
        LocationMusicInput frigate{};
        frigate.location = 0x05;
        bool fr = true;
        for (int t = 0x20; t <= 0x27; ++t) {
            frigate.transport_tile = uint8_t(t);
            fr = fr && music_context_for_location(frigate) == MusicContext::Frigate;
        }
        frigate.transport_tile = 0x28; // skiff: no override
        fr = fr && music_context_for_location(frigate) == MusicContext::Cities;
        check(fr, "M2 a frigate (tiles 0x20..0x27) overrides the location; a skiff does not");
        LocationMusicInput combat{};
        combat.in_combat = true;
        combat.transport_tile = 0x20;
        const bool c1 = music_context_for_location(combat) == MusicContext::Combat;
        combat.combat_victory = true;
        check(c1 && music_context_for_location(combat) == MusicContext::Victory,
              "M2 combat wins over everything (Engagement), and after VICTORY! the Theme");
        check(song_for_context(MusicContext::Hut) == song_for_context(MusicContext::Village) &&
                  song_for_context(MusicContext::Silence) == MusicSong::None &&
                  song_for_context(MusicContext::Creation) == MusicSong::Amiga,
              "M3 hut and village share Greyson's Tale; silence has no song; creation is the Amiga Theme");
    }

    // ======================================================================
    // S -- the service
    // ======================================================================
    {
        Recorder r;
        AudioService s;
        s.attach(&r);
        r.calls.clear();
        s.play_sfx(SfxId::MoveBlocked, 7);
        check(r.calls.size() == 1 && r.calls[0].what == "sfx" && r.calls[0].a == int(SfxId::MoveBlocked) &&
                  r.calls[0].b == 7 && r.calls[0].c == int(volume_to_gain_q15(kDefaultSfxVolume)),
              "S11 a semantic cue reaches the backend once, with its parameter and the SFX gain");
        s.play_sfx(SfxId::None);
        check(r.calls.size() == 1 && s.stats().sfx_unknown == 1, "S11 SfxId::None never reaches the backend");
    }
    {
        Recorder r;
        AudioService s;
        s.attach(&r);
        s.set_music_availability(MusicAvailability::Available);
        r.calls.clear();
        s.play_music(MusicContext::Castle);
        check(r.calls.size() == 1 && r.calls[0].what == "music" && r.calls[0].a == int(MusicSong::Monarch) &&
                  s.current_song() == MusicSong::Monarch && s.current_music_context() == MusicContext::Castle,
              "S12 with the supported patch a context reaches the backend as its song");
        s.play_music(MusicContext::Castle);
        s.play_music(MusicContext::Hut);
        s.play_music(MusicContext::Village); // same song (Greyson): no restart
        check(r.count("music") == 2, "S12 the same song is never restarted (the driver's cmp al,[0x11e])");
        s.play_music(MusicContext::Silence);
        check(r.calls.back().what == "stop_music" && s.current_song() == MusicSong::None,
              "S12 Silence stops the song (selector 0x03)");
    }
    {
        bool none_reach = true;
        for (auto a : {MusicAvailability::StockNoMusic, MusicAvailability::IncompletePatch, MusicAvailability::UnknownVariant,
                       MusicAvailability::NoAudioPack, MusicAvailability::AudioPackInvalid}) {
            Recorder r;
            AudioService s;
            s.attach(&r);
            s.set_music_availability(a);
            r.calls.clear();
            for (size_t c = 0; c < kMusicContextCount; ++c) s.play_music(MusicContext(c));
            s.set_music_volume(100);
            none_reach = none_reach && r.count("music") == 0 && s.current_song() == MusicSong::None &&
                         s.stats().music_unavailable == kMusicContextCount && !s.has_music();
        }
        check(none_reach, "S13 without the supported patch no context, at any volume, ever reaches the backend");
        Recorder r;
        AudioService s;
        s.attach(&r);
        s.set_music_availability(MusicAvailability::StockNoMusic);
        s.play_music(MusicContext::Dungeon);
        s.play_sfx(SfxId::DungeonTrap);
        check(s.current_music_context() == MusicContext::Dungeon && r.count("sfx") == 1,
              "S13 a stock install keeps the requested context (no-op) and still plays SFX");
    }
    {
        Recorder r;
        AudioService s;
        s.attach(&r);
        s.set_music_availability(MusicAvailability::Available);
        s.play_music(MusicContext::Overworld);
        r.calls.clear();
        s.set_sfx_volume(30);
        check(r.calls.size() == 1 && r.calls[0].what == "gain_sfx" && r.calls[0].a == int(volume_to_gain_q15(30)) &&
                  s.current_song() == MusicSong::BritannicLands,
              "S14 SFX volume changes the SFX channel only (no music gain, no restart)");
        r.calls.clear();
        s.set_music_volume(60);
        check(r.calls.size() == 1 && r.calls[0].what == "gain_music" && r.calls[0].a == int(volume_to_gain_q15(60)),
              "S15 music volume changes the music channel only");
        s.play_sfx(SfxId::Quake);
        check(r.calls.back().c == int(volume_to_gain_q15(30)), "S15 and SFX still carry the SFX gain");
        r.calls.clear();
        s.set_sfx_volume(30);
        s.set_music_volume(60);
        check(r.calls.empty(), "S14 re-applying unchanged volumes touches neither channel");
    }
    {
        Recorder r;
        AudioService s;
        s.attach(&r);
        s.set_music_availability(MusicAvailability::Available);
        s.play_music(MusicContext::Keep);
        s.set_sfx_volume(0);
        r.calls.clear();
        s.play_sfx(SfxId::MoveStep);
        check(r.count("sfx") == 0 && s.stats().sfx_muted == 1, "S16 SFX volume 0 is mute: nothing is submitted");
        s.set_music_volume(0);
        check(r.count("stop_music") == 1 && s.current_song() == MusicSong::None &&
                  s.current_music_context() == MusicContext::Keep,
              "S16 music volume 0 stops the song but remembers the context");
        s.set_music_volume(10);
        check(r.calls.back().what == "music" && r.calls.back().a == int(MusicSong::LadyNan),
              "S16 raising the music volume from 0 starts the context's song again");
    }
    {
        bool mono = true;
        for (int v = 1; v <= 100; ++v) mono = mono && volume_to_gain_q15(uint8_t(v)) > volume_to_gain_q15(uint8_t(v - 1));
        check(volume_to_gain_q15(0) == 0 && volume_to_gain_q15(50) == 8191 && volume_to_gain_q15(100) == kUnityGainQ15 &&
                  volume_to_gain_q15(255) == kUnityGainQ15 && mono,
              "S17 gain: 0 -> 0, 50 -> 8191, 100 -> 32767 exactly, above 100 clamps, strictly increasing");
        bool no_overflow = true;
        for (int x : {32767, -32768, 1, -1, 12345, -12345})
            for (int g : {0, 1, 8191, 32767, 65535}) {
                const int y = apply_gain_q15(int16_t(x), uint16_t(g));
                no_overflow = no_overflow && y >= -32768 && y <= 32767 && (x > 0 ? y <= x && y >= 0 : y >= x && y <= 0);
            }
        check(no_overflow && apply_gain_q15(32767, kUnityGainQ15) == 32766 && apply_gain_q15(-32768, kUnityGainQ15) == -32767 &&
                  apply_gain_q15(-32768, 0) == 0,
              "S17 applying any gain to any sample never overflows or flips sign; max gain is unity");
        check(step_volume(95, 1) == 100 && step_volume(100, 1) == 100 && step_volume(5, -1) == 0 && step_volume(0, -1) == 0 &&
                  step_volume(80, 1) == 90 && step_volume(80, -1) == 70 && step_volume(200, -1) == 90,
              "S17 a Settings step is 10, clamped at 0 and 100 without wrapping");
    }
    {
        Recorder r;
        r.refuse_sfx = r.refuse_music = true;
        AudioService s;
        s.attach(&r);
        s.set_music_availability(MusicAvailability::Available);
        r.calls.clear();
        s.play_sfx(SfxId::Moongate);
        s.play_music(MusicContext::Dungeon);
        check(r.count("sfx") == 1 && r.count("music") == 1 && s.stats().sfx_refused == 1 && s.stats().music_refused == 1 &&
                  s.current_song() == MusicSong::None,
              "S20 a refusing backend is asked once per request -- never retried, never waited on -- and "
              "the service degrades to silence");
        AudioService silent;
        silent.play_sfx(SfxId::Quake);
        silent.set_music_availability(MusicAvailability::Available);
        silent.play_music(MusicContext::Castle);
        check(silent.stats().sfx_muted == 1 && silent.current_song() == MusicSong::None,
              "S20 no backend at all (no audio hardware) is silence, not an error");
        Recorder r2;
        AudioService s2;
        s2.attach(&r2);
        s2.set_music_availability(MusicAvailability::Available);
        s2.play_music(MusicContext::Castle);
        r2.calls.clear();
        s2.flush_for_load();
        check(r2.calls.size() == 1 && r2.calls[0].what == "stop_sfx" && s2.current_song() == MusicSong::Monarch,
              "S18 a load flushes the old world's SFX and leaves music alone");
    }
    {
        // No blocking primitive in the service; the device backend's game-thread
        // entry posts with a zero timeout. Comments are stripped first.
        const auto service = strip_line_comments(slurp(core_dir + "/src/audio.cpp"));
        bool clean = !service.empty();
        for (const char *t : {"while", "vTaskDelay", "sleep", "delay", "wait", "yield", "xQueue", "Semaphore", "esp_rom"})
            clean = clean && service.find(t) == std::string::npos;
        check(clean, "S20 audio.cpp holds no loop-until, delay, sleep, wait or RTOS primitive");
        const auto device = slurp(core_dir + "/../targets/tdeck/main/tdeck_audio.cpp");
        const auto play = device.substr(device.find("bool TdeckAudioBackend::play_sfx"), 400);
        // A3-02 posts a {request, epoch} command instead of the bare request.
        check(std::regex_search(play, std::regex(R"(xQueueSend\(queue_, &\w+, 0\))")) &&
                  device.find("xTaskCreatePinnedToCore(task_entry, \"openu5-audio\"") != std::string::npos,
              "S20 the device backend's game-thread entry posts with a 0 timeout; I2S writes run on its own task");
    }

    // ======================================================================
    // P -- the audio pack
    // ======================================================================
    {
        const auto stock = bytes_of(argv[4]);
        const auto info = inspect_audio_pack(stock.data(), stock.size());
        check(stock.size() == 112 && info.state == AudioPackState::Valid &&
                  info.record.capability == MusicCapability::StockNoMusic && info.record.data_ovl_marker == 1 &&
                  info.song_entries == 0 && !info.bank_entry && music_availability(info) == MusicAvailability::StockNoMusic,
              "P1 the committed stock pack (written by the TS packer) is Valid and says stock: no music");
    }
    const auto real = bytes_of(argv[5]);
    {
        const auto info = inspect_audio_pack(real.data(), real.size());
        check(!real.empty() && info.state == AudioPackState::Valid &&
                  info.record.capability == MusicCapability::SupportedMusicPatch &&
                  info.record.driver == AudioPackDriver::ExodusUpgrade10 && info.record.bank == AudioPackBank::Valid &&
                  info.song_entries == 16 && info.bank_entry && info.record.songs_valid == 0xffff &&
                  music_availability(info) == MusicAvailability::Available,
              "P2 the real audio pack (npm run pack:audio on the patched install) is Valid and Available: "
              "16 structurally valid XMI songs and a valid timbre bank");
        std::printf("  real audio pack: %zu bytes, payload CRC %08lx, marker %u\n", real.size(),
                    (unsigned long)info.payload_crc32, unsigned(info.record.data_ovl_marker));
    }
    {
        auto mutate = [&](size_t at, uint8_t x) {
            auto m = real;
            if (at < m.size()) m[at] ^= x;
            return inspect_audio_pack(m.data(), m.size()).state;
        };
        check(real.size() > 2000 && mutate(real.size() - 100, 0x40) == AudioPackState::Corrupt &&
                  mutate(40, 0x01) == AudioPackState::Corrupt,
              "P3 one flipped payload or table byte: Corrupt");
        auto appended = real;
        appended.insert(appended.end(), {0xde, 0xad, 0xbe, 0xef});
        put32(appended, 20, uint32_t(appended.size())); // a size field that agrees
        check(inspect_audio_pack(appended.data(), appended.size()).state == AudioPackState::Corrupt,
              "P3 bytes outside every entry are covered too: appended data breaks the header's payload CRC");
        auto v2 = real;
        put16(v2, 8, 2);
        auto v0 = real;
        put16(v0, 8, 0);
        auto magic = real;
        magic[0] = 'X';
        auto truncated = real;
        truncated.resize(real.size() - 1);
        check(inspect_audio_pack(v2.data(), v2.size()).state == AudioPackState::UnsupportedVersion &&
                  inspect_audio_pack(v0.data(), v0.size()).state == AudioPackState::UnsupportedVersion &&
                  inspect_audio_pack(magic.data(), magic.size()).state == AudioPackState::NotAudioPack &&
                  inspect_audio_pack(truncated.data(), truncated.size()).state == AudioPackState::Corrupt &&
                  inspect_audio_pack(nullptr, 0).state == AudioPackState::Missing,
              "P4 stale/newer major version, wrong magic, truncation and absence are all refused -- and every "
              "refusal is 'no music', never a boot failure (music_availability)");
        bool no_music = true;
        for (auto st : {AudioPackState::Missing, AudioPackState::NotAudioPack, AudioPackState::UnsupportedVersion,
                        AudioPackState::Corrupt, AudioPackState::Inconsistent}) {
            AudioPackInfo i{};
            i.state = st;
            i.record.capability = MusicCapability::SupportedMusicPatch; // even a record that claims music
            no_music = no_music && music_availability(i) != MusicAvailability::Available;
        }
        check(no_music, "P4 only a Valid pack can make music available");
    }
    {
        const auto good = write_pack(supported_entries());
        check(inspect_audio_pack(good.data(), good.size()).state == AudioPackState::Valid,
              "P5 a well-formed supported pack of synthetic shells is Valid (the reader needs no real data)");
        auto e = supported_entries();
        e.erase(e.begin() + 5); // drop song 4
        const auto missing_song = write_pack(e);
        auto e2 = supported_entries(shell_xmi(2)); // two-sequence songs
        const auto bad_song = write_pack(e2);
        auto e3 = supported_entries();
        e3.pop_back(); // no bank
        const auto no_bank = write_pack(e3);
        auto e4 = supported_entries();
        e4[0].data[1] = uint8_t(MusicCapability::StockNoMusic); // claims stock, carries music
        const auto stock_with_music = write_pack(e4);
        auto e5 = supported_entries();
        e5[0].data[1] = 9;
        const auto bad_capability = write_pack(e5);
        auto e6 = supported_entries();
        e6.push_back({"song", 1, 3, shell_xmi()}); // a duplicate song id
        const auto duplicate = write_pack(e6);
        auto e7 = supported_entries(shell_xmi(1, false)); // no EVNT
        const auto no_events = write_pack(e7);
        auto st = [](const std::vector<uint8_t> &p) { return inspect_audio_pack(p.data(), p.size()).state; };
        check(st(missing_song) == AudioPackState::Inconsistent && st(bad_song) == AudioPackState::Inconsistent &&
                  st(no_bank) == AudioPackState::Inconsistent && st(stock_with_music) == AudioPackState::Inconsistent &&
                  st(bad_capability) == AudioPackState::Inconsistent && st(duplicate) == AudioPackState::Inconsistent &&
                  st(no_events) == AudioPackState::Inconsistent,
              "P5 never pretend: 'supported' without 16 valid songs + a bank, a stock record carrying music, an "
              "unknown capability or a duplicate song is Inconsistent -> no music");
        check(validate_xmi(shell_xmi().data(), shell_xmi().size()) && !validate_xmi(shell_xmi(2).data(), shell_xmi(2).size()) &&
                  !validate_xmi(shell_xmi().data(), 20) && validate_timbre_bank(shell_bank().data(), 22) &&
                  !validate_timbre_bank(shell_bank().data(), 21) && !validate_timbre_bank(shell_bank().data(), 6),
              "P5 validate_xmi / validate_timbre_bank accept the shapes and reject truncation and bad counts");
    }

    // ======================================================================
    // F -- the Settings rows and their persistence contract
    // ======================================================================
    UiAction east{}, west{}, next{}, confirm{}, back{};
    east.kind = west.kind = UiActionKind::Direction;
    east.direction = Direction::East;
    west.direction = Direction::West;
    next.kind = UiActionKind::Next;
    confirm.kind = UiActionKind::Confirm;
    back.kind = UiActionKind::Back;
    FrontendSaveCatalog slots{};
    auto open_settings = [&](SystemMenuSession &m, const FrontendSettings &s, MusicAvailability a) {
        m.set_music_availability(a);
        m.open(s, slots);
        for (int i = 0; i < 3; ++i) m.handle(next);
        m.handle(confirm);
    };
    {
        SystemMenuSession m;
        open_settings(m, FrontendSettings{}, MusicAvailability::StockNoMusic);
        auto v = m.view();
        check(v.kind == FrontendViewKind::Settings && v.line_count == SystemMenuSession::kSettingsRowCount &&
                  std::strcmp(v.lines[SystemMenuSession::kSfxVolumeRow], "SFX Volume: 80%") == 0 &&
                  std::strcmp(v.lines[SystemMenuSession::kMusicVolumeRow], "Music Volume: Unavailable") == 0 &&
                  SystemMenuSession::kSettingsRowCount == 6 && SystemMenuSession::kMusicVolumeRow == 5,
              "F5 System Menu Settings: six rows, SFX Volume and Music Volume last (A4-ENH1 removed the Developer row)");
        for (int i = 0; i < 4; ++i) m.handle(next);
        m.handle(east);
        const bool up = m.settings().sound_volume == 90;
        m.handle(east);
        m.handle(east);
        const bool top = m.settings().sound_volume == 100;
        for (int i = 0; i < 12; ++i) m.handle(west);
        const bool bottom = m.settings().sound_volume == 0;
        m.handle(confirm);
        check(up && top && bottom && m.settings().sound_volume == 10 &&
                  std::strcmp(m.view().lines[SystemMenuSession::kSfxVolumeRow], "SFX Volume: 10%") == 0,
              "F5 SFX Volume: Right +10, clamped at 100 and at 0 (no wrap), Enter +10 -- usable with no music at all");
        m.handle(next);
        const auto before = m.settings().music_volume;
        m.handle(east);
        m.handle(west);
        m.handle(confirm);
        v = m.view();
        check(m.settings().music_volume == before && before == kDefaultMusicVolume &&
                  std::strcmp(v.footer, "Stock DOS game files have no music") == 0,
              "F7 stock assets: Music Volume shows Unavailable, ignores every adjust key, keeps the stored 80, and "
              "the footer says why");
        m.handle(back);
        const auto intent = m.take_intent();
        check(intent.kind == SystemMenuIntentKind::PersistSettings && intent.settings.sound_volume == 10 &&
                  intent.settings.music_volume == 80,
              "F8 leaving Settings persists both volumes (the unavailable one unchanged)");
    }
    {
        SystemMenuSession m;
        open_settings(m, FrontendSettings{}, MusicAvailability::Available);
        for (int i = 0; i < 5; ++i) m.handle(next);
        m.handle(west);
        m.handle(west);
        const auto v = m.view();
        check(m.settings().music_volume == 60 && std::strcmp(v.lines[SystemMenuSession::kMusicVolumeRow], "Music Volume: 60%") == 0 &&
                  std::strcmp(v.footer, "Left/right changes; Mic saves") == 0, // A4-UI4: one footer for both Settings pages
              "F6 supported patch: Music Volume is a live 0..100 % row");
        bool reasons = true;
        for (auto a : {MusicAvailability::IncompletePatch, MusicAvailability::UnknownVariant, MusicAvailability::NoAudioPack,
                       MusicAvailability::AudioPackInvalid}) {
            SystemMenuSession u;
            open_settings(u, FrontendSettings{}, a);
            for (int i = 0; i < 5; ++i) u.handle(next);
            reasons = reasons && u.view().footer && std::strcmp(u.view().footer, music_unavailable_reason(a)) == 0 &&
                      std::strlen(u.view().footer) <= 40;
        }
        check(reasons, "F7 every no-music state names its own reason in the footer (<= 40 characters)");
    }
    {
        FrontendSession title;
        title.set_music_availability(MusicAvailability::Available);
        title.start(0, false, {});
        UiAction s{};
        s.kind = UiActionKind::Character;
        s.character = u's';
        title.handle(confirm, 1);
        title.handle(s, 2);
        const auto v = title.view();
        check(title.state() == FrontendState::Settings && v.line_count == 6 &&
                  std::strcmp(v.lines[4], "SFX Volume: 80%") == 0 && std::strcmp(v.lines[5], "Music Volume: 80%") == 0,
              "F5 title Settings (release build): six rows, the same two audio rows at 4 and 5");
        FrontendSession dev;
        dev.set_music_availability(MusicAvailability::StockNoMusic);
        dev.start(0, true, {});
        dev.handle(confirm, 1);
        dev.handle(s, 2);
        for (int i = 0; i < 5; ++i) dev.handle(next, 3);
        dev.handle(east, 3);
        // A4-ENH1 removed the developer build's seventh row ("Developer: Visible/
        // Hidden"); both builds now show the same six.
        check(dev.view().line_count == 6 && dev.settings().music_volume == 80 &&
                  std::strcmp(dev.view().footer, "Stock DOS game files have no music") == 0,
              "F7 title Settings (developer build): six rows, as a release build; Music Volume unavailable and unchanged");
        dev.start(0, true, {});
        dev.handle(confirm, 1);
        dev.handle(s, 2);
        check(std::strcmp(dev.view().lines[5], "Music Volume: Unavailable") == 0,
              "F7 music availability survives a Return to Title (start() keeps device configuration)");
    }
    {
        FrontendSettings changed{};
        changed.sound_volume = 30;
        changed.music_volume = 0;
        std::string text;
        FrontendSettings back_in{};
        check(encode_settings(changed, text) && decode_settings(text, back_in) && back_in.sound_volume == 30 &&
                  back_in.music_volume == 0,
              "F8 both volumes survive encode -> decode (settings.json across a reboot), 0 included");
        FrontendSettings keep{};
        keep.sound_volume = 55;
        const bool empty = !decode_settings("", keep) && keep.sound_volume == 55;
        const bool garbage = !decode_settings("{\"version\":1,\"brightness\"", keep) && keep.sound_volume == 55;
        const bool range = !decode_settings("{\"version\":1,\"brightness\":80,\"movementMode\":false,"
                                            "\"trackballResponsiveness\":100,\"uiSize\":1,\"developerToolsVisible\":false,"
                                            "\"soundVolume\":101,\"musicVolume\":80,\"touchControls\":false}", keep) &&
                           keep.sound_volume == 55;
        const bool version = !decode_settings("{\"version\":2}", keep) && keep.sound_volume == 55;
        check(empty && garbage && range && version,
              "F9 a missing, truncated, out-of-range or future settings document is refused whole and leaves the "
              "caller's values (the defaults at boot) untouched");
        // What every Alpha 2 firmware wrote (encode_settings with defaults): its
        // audio keys have existed since the file's first version.
        const std::string alpha2 = "{\"version\":1,\"brightness\":80,\"movementMode\":false,\"trackballResponsiveness\":100,"
                                   "\"uiSize\":1,\"developerToolsVisible\":false,\"soundVolume\":80,\"musicVolume\":80,"
                                   "\"touchControls\":false}";
        FrontendSettings from_alpha2{};
        from_alpha2.sound_volume = 1;
        std::string default_text;
        // A4-ENH1 (ALPHA4_UI.md section 10) appended one optional key, the
        // trackball speed level; every older key is written as before, so an
        // older firmware still reads the file, and a file without the key
        // loads with the default level.
        const std::string alpha4_enh1 = alpha2.substr(0, alpha2.size() - 1) + ",\"trackballSpeed\":5}";
        check(decode_settings(alpha2, from_alpha2) && from_alpha2.sound_volume == 80 && from_alpha2.music_volume == 80 &&
                  from_alpha2.trackball_speed == kTrackballSpeedDefault &&
                  encode_settings(FrontendSettings{}, default_text) && default_text == alpha4_enh1,
              "F10 an Alpha 2 settings.json loads unchanged (80 % / 80 %, default trackball speed); the default "
              "document is the Alpha 2 one plus \"trackballSpeed\"");
        FrontendSettings odd{};
        const bool hand = decode_settings(std::regex_replace(alpha2, std::regex("\"soundVolume\":80"), "\"soundVolume\":85"), odd);
        check(hand && odd.sound_volume == 85 && step_volume(odd.sound_volume, 1) == 95 && step_volume(95, 1) == 100,
              "F10 a hand-edited off-step value is kept and steps deterministically (85 -> 95 -> 100)");
    }

    std::printf("A3-01 audio contract: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
