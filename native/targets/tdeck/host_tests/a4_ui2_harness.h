#pragma once
// Alpha 4 UI Batch 2 (targets/tdeck/ALPHA4_UI.md section 2): the shared harness
// of the three A4-UI2 runtime tests. The REAL AlphaRuntime on the REAL
// tdeck_board.cpp over the fake ST7789 (A3-04E's harness, bus untimed), on the
// host esp_timer shim's virtual clock, with a recording audio backend.
#include "../main/alpha_audio.h"
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/audio.h"
#include "openu5/frontend.h"
#include "openu5/system_menu.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace a4_ui2 {
using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

inline int checks = 0, failures = 0;
inline void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
inline std::string n(long long v) { return std::to_string(v); }

inline const tdeck::AlphaResourceOwners *pack = nullptr;
inline size_t g_dungeon_count = 0;
inline const char *g_dump = nullptr;
// A4-END1: the dungeon rooms' arenas (combat map + sprites), for a test that
// fights in a dungeon room. Null keeps the fixture without them, as before.
inline const DungeonArena *g_arenas = nullptr;
inline size_t g_arena_count = 0;
constexpr int64_t kClockStartUs = 5'000'000;
constexpr uint16_t kBlack = 0x0000, kWhite = 0xffff;
constexpr uint16_t kDim = 0xad55; // EGA 7, the frontend's footers

/** Opens and loads the resource pack; false if it cannot run. */
inline bool load_pack(const char *path) {
    static tdeck::AlphaResourcePack source;
    static tdeck::AlphaResourceOwners owners{};
    tdeck::AlphaResourceReport report{};
    if (source.open(path, report) != ESP_OK || source.load(owners, report) != ESP_OK || !owners.ibm_font) return false;
    pack = &owners;
    g_dungeon_count = report.dungeon_count;
    return true;
}

// --- the panel ----------------------------------------------------------------
inline uint16_t px(int x, int y) { return bus::gram()[y * 320 + x]; }
inline bool ibm_bit(char c, int row, int col) {
    return (pack->ibm_font[size_t(uint8_t(c) & 0x7f) * 8 + size_t(row)] & (0x80U >> col)) != 0;
}
/** Pixels of an 8x8 IBM.CH run at (x0,y) that differ from `fg` on `bg`. */
inline int ibm_mismatch(int x0, int y, const std::string &text, uint16_t fg, uint16_t bg) {
    int bad = 0;
    for (size_t i = 0; i < text.size(); ++i)
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) bad += px(x0 + int(i) * 8 + c, y + r) != (ibm_bit(text[i], r, c) ? fg : bg);
    return bad;
}
/** Of a rectangle, how many pixels are not black. */
inline int lit(int x0, int y0, int w, int h) {
    int k = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) k += px(x, y) != kBlack;
    return k;
}
/** The bounding box of every non-black pixel in a rectangle; false if none. */
inline bool ink_box(int x0, int y0, int w, int h, int &l, int &t, int &r, int &b) {
    l = t = 1 << 20;
    r = b = -1;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (px(x, y) != kBlack) {
                l = std::min(l, x); r = std::max(r, x);
                t = std::min(t, y); b = std::max(b, y);
            }
    return r >= 0;
}

// A PNG of the GRAM (stored deflate blocks; no zlib on the host toolchain).
inline void dump(const char *name) {
    if (!g_dump) return;
    std::vector<uint8_t> raw;
    for (int y = 0; y < 240; ++y) {
        raw.push_back(0);
        for (int x = 0; x < 320; ++x) {
            const uint16_t p = px(x, y);
            raw.push_back(uint8_t(((p >> 11) & 31) * 255 / 31));
            raw.push_back(uint8_t(((p >> 5) & 63) * 255 / 63));
            raw.push_back(uint8_t((p & 31) * 255 / 31));
        }
    }
    auto crc = [](const std::vector<uint8_t> &d) {
        uint32_t c = 0xffffffffU;
        for (uint8_t v : d) {
            c ^= v;
            for (int b = 0; b < 8; ++b) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1U)));
        }
        return c ^ 0xffffffffU;
    };
    auto be32 = [](std::vector<uint8_t> &d, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) d.push_back(uint8_t(v >> s));
    };
    std::vector<uint8_t> z{0x78, 0x01};
    for (size_t at = 0; at < raw.size();) {
        const size_t len = std::min<size_t>(65535, raw.size() - at);
        z.push_back(at + len == raw.size() ? 1 : 0);
        z.push_back(uint8_t(len)); z.push_back(uint8_t(len >> 8));
        z.push_back(uint8_t(~len)); z.push_back(uint8_t(~len >> 8));
        z.insert(z.end(), raw.begin() + std::ptrdiff_t(at), raw.begin() + std::ptrdiff_t(at + len));
        at += len;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    be32(z, (b << 16) | a);
    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    auto chunk = [&](const char *type, const std::vector<uint8_t> &data) {
        be32(png, uint32_t(data.size()));
        std::vector<uint8_t> body(type, type + 4);
        body.insert(body.end(), data.begin(), data.end());
        png.insert(png.end(), body.begin(), body.end());
        be32(png, crc(body));
    };
    std::vector<uint8_t> ihdr;
    be32(ihdr, 320); be32(ihdr, 240);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    std::ofstream out(std::string(g_dump) + "/" + name + ".png", std::ios::binary);
    out.write(reinterpret_cast<const char *>(png.data()), std::streamsize(png.size()));
}

// --- the recording audio backend ----------------------------------------------
struct Recorder final : AudioBackend {
    struct Call {
        std::string what; // "sfx", "music", "stop_music", "stop_sfx"
        int value = 0;    // SfxId / MusicSong
        int64_t us = 0;
    };
    std::vector<Call> calls;
    bool play_sfx(const SfxRequest &r) override {
        calls.push_back({"sfx", int(r.id), openu5_host_virtual_clock_us()});
        return true;
    }
    void stop_sfx() override { calls.push_back({"stop_sfx", 0, openu5_host_virtual_clock_us()}); }
    bool start_music(MusicSong s, uint16_t) override {
        calls.push_back({"music", int(s), openu5_host_virtual_clock_us()});
        return true;
    }
    void stop_music() override { calls.push_back({"stop_music", 0, openu5_host_virtual_clock_us()}); }
    void set_gain(AudioChannel, uint16_t) override {}
    size_t count(const char *what, int64_t after_us = -1) const {
        size_t k = 0;
        for (const auto &c : calls) k += c.what == what && c.us > after_us;
        return k;
    }
    size_t sfx(SfxId id, int64_t after_us = -1) const {
        size_t k = 0;
        for (const auto &c : calls) k += c.what == "sfx" && c.value == int(id) && c.us > after_us;
        return k;
    }
};

// --- the runtime ----------------------------------------------------------------
struct Member {
    const char *name;
    char status;
    uint16_t hp;
};
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Recorder audio{};
    Run(const std::vector<Member> &party, bool paced = false, const AudioPackInfo *sound = nullptr,
        bool patterned = false) {
        openu5_host_virtual_clock_us() = kClockStartUs;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = false;
        board.initialize_display();
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
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
        f.arenas = g_arenas;
        f.arena_count = g_arena_count;
        f.render_pixels = true;
        f.patterned_test_tiles = patterned;
        f.paced_scenes = paced;
        rt->attach_host_test_fixture(f);
        if (sound) rt->configure_audio(*sound, &audio);
        auto &g = rt->game();
        g.time.year = 139;
        g.time.month = 4;
        g.time.day = 5;
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.gold = 400;
        g.karma = 60;
        g.party.character_count = g.party.party_size = uint8_t(party.size());
        g.party.active_character = 255;
        for (size_t i = 0; i < party.size(); ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", party[i].name);
            m.status = uint8_t(party[i].status);
            m.party_status = 0;
            m.character_class = i ? 'F' : 'A';
            m.level = 1;
            m.current_hp = party[i].hp;
            m.max_hp = uint16_t(party[i].hp + 5);
            m.strength = m.dexterity = m.intelligence = 20;
        }
        g.rng.seed(2);
        render(true);
    }
    static int64_t now() { return openu5_host_virtual_clock_us(); }
    static int64_t now_ms() { return openu5_host_virtual_clock_us() / 1000; }
    void render(bool force = false) { rt->render(board, force); }
    /** Advance the clock in 5 ms frames, rendering each. */
    void run(int64_t ms) {
        for (int64_t t = 0; t < ms; t += 5) {
            openu5_host_virtual_clock_us() += 5000;
            render();
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = now();
        rt->handle(e);
    }
    void key(uint8_t code, bool alt = false, bool shift = false, int64_t advance_us = 100000) {
        openu5_host_virtual_clock_us() += advance_us;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        e.modifiers.shift = shift;
        raw(e);
        render();
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        render();
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    void mic() {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        openu5_host_virtual_clock_us() += 50000;
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
        render();
    }
    /** Alt+M, then up from Resume wraps to the last row, Return to Title. */
    void return_to_title() { key('m', true); up(); key('\r'); }
    std::string transcript() const {
        std::string out;
        for (size_t i = 0; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) out += std::string(b->text) + "\n";
        return out;
    }
};

inline const char *arg_after(int argc, char **argv, const char *flag) {
    for (int i = 1; i + 1 < argc; ++i)
        if (!std::strcmp(argv[i], flag)) return argv[i + 1];
    return nullptr;
}
} // namespace a4_ui2
