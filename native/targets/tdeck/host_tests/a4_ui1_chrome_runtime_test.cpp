// Alpha 4 UI Batch 1 (targets/tdeck/ALPHA4_UI.md) -- the "More Ultima V" chrome.
//
//   P  the resource pack: IBM.CH travels as "ibm.ch" (1,024 B, 128 glyphs x 8),
//      byte for byte the game's file, loaded into the owners, required by name,
//      and the firmware's identity lock names this pack
//   C  the gameplay chrome, read off the fake ST7789 after a REAL AlphaRuntime
//      render on the REAL tdeck_board.cpp: the EGA-blue frame band (touch
//      reserve included), the white play-window rule, the white-ruled roster
//      and status boxes joined by a blue bar, the unboxed console, and the
//      right rule at x=318 that the 135-px rows used to paint over
//   R  the roster row in IBM.CH, roster.ts's layout: name (9) -> on the active
//      member (not when asleep or dead), HP right-aligned in 4, the status
//      letter; the existing picker/actor colour tints are kept
//   S  the status box: the location caption (compact font, with MOVE and the
//      17-character clip in Movement Mode), F:/G:, and the M-D-Y date with the
//      digital time kept on its right
//   B  the sky and wind strips as bands with the original's notch and >< ends,
//      captions in IBM.CH (wind, and the dungeon's level / facing)
//   F  the frontend shell: >Title< band caption, white-ruled content window,
//      reverse-video menu selection (title art main menu included), grey
//      footer; Settings at Medium and Large
//   G  the frontend and chrome goldens (a4_ui1_goldens.h): the whole screen
//      after each scripted state above, recorded from the reviewed Board
//
//   a4_ui1_chrome_runtime <openu5-alpha1-resources.bin> <original IBM.CH>
//                         [--record <a4_ui1_goldens.h>] [--dump <dir>]
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/location_names.h"
#include "../main/native_renderer.h"
#include "../main/tdeck_board.h"
#include "a4_ui1_goldens.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/frontend.h"
#include "openu5/hud.h"
#include "openu5/system_menu.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
namespace bus = openu5_host_bus;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
std::string n(uint64_t v) { return std::to_string(v); }
std::string hex4(uint16_t v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%04x", unsigned(v));
    return b;
}

constexpr uint16_t kBlack = 0x0000, kWhite = 0xffff, kCyan = 0x07ff, kGreen = 0x07e0;
// The approved palette: the original's own EGA indices (web port frame.ts).
constexpr uint16_t kBand = 0x0015;  // EGA 1, #0000AA -- the frame
constexpr uint16_t kDim = 0xad55;   // EGA 7, #AAAAAA -- footers
constexpr int64_t kClockStartUs = 5'000'000;

const tdeck::AlphaResourceOwners *pack = nullptr;
size_t g_dungeon_count = 0;
const char *g_dump = nullptr;

std::vector<uint8_t> slurp(const char *path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

// ---------------------------------------------------------------------------
// P -- the pack.
// ---------------------------------------------------------------------------
struct Toc { std::string name; uint32_t offset = 0, length = 0, records = 0, stride = 0; };
std::vector<Toc> toc(const std::vector<uint8_t> &b) {
    std::vector<Toc> out;
    if (b.size() < 32) return out;
    const uint32_t count = le32(b.data() + 16);
    for (uint32_t i = 0; i < count && 32 + size_t(i + 1) * 64 <= b.size(); ++i) {
        const uint8_t *t = b.data() + 32 + size_t(i) * 64;
        Toc e;
        e.name.assign(reinterpret_cast<const char *>(t), strnlen(reinterpret_cast<const char *>(t), 32));
        e.offset = le32(t + 32); e.length = le32(t + 36); e.records = le32(t + 44); e.stride = le32(t + 48);
        out.push_back(e);
    }
    return out;
}
// batch53_release_blockers' seam: rename one TOC entry and re-seal the TOC CRC.
bool rename_entry(std::vector<uint8_t> &bytes, const char *from, const char *to) {
    const uint32_t count = le32(bytes.data() + 16);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t *t = bytes.data() + 32 + size_t(i) * 64;
        if (std::strncmp(reinterpret_cast<const char *>(t), from, 32) != 0) continue;
        std::memset(t, 0, 32);
        std::memcpy(t, to, std::strlen(to));
        uint32_t crc = 0xffffffffU;
        for (size_t k = 32; k < 32 + size_t(count) * 64; ++k) {
            crc ^= bytes[k];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
        crc ^= 0xffffffffU;
        for (int k = 0; k < 4; ++k) bytes[28 + k] = uint8_t(crc >> (8 * k));
        return true;
    }
    return false;
}

void test_pack(const char *pack_path, const char *ibm_path) {
    std::printf("P  the resource pack carries IBM.CH\n");
    const auto bytes = slurp(pack_path);
    const auto original = slurp(ibm_path);
    const auto entries = toc(bytes);
    const Toc *ibm = nullptr;
    for (const auto &e : entries)
        if (e.name == "ibm.ch") ibm = &e;
    const bool shaped = ibm && ibm->length == 1024 && ibm->records == 128 && ibm->stride == 8 &&
                        size_t(ibm->offset) + ibm->length <= bytes.size();
    check(shaped, "P1 the pack has an \"ibm.ch\" entry of 1,024 B (128 records x 8) among its " + n(entries.size()) +
                      " entries");
    check(shaped && original.size() == 1024 &&
              std::equal(original.begin(), original.end(), bytes.begin() + std::ptrdiff_t(ibm->offset)),
          "P2 its bytes are the game's IBM.CH, byte for byte (" + n(original.size()) + " B on disk)");

    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    const auto opened = source.open(pack_path, report);
    tdeck::AlphaResourceOwners owners{};
    const auto loaded = opened == ESP_OK ? source.load(owners, report) : opened;
    check(loaded == ESP_OK && owners.ibm_font && original.size() == 1024 &&
              std::memcmp(owners.ibm_font, original.data(), 1024) == 0,
          "P3 load() puts the 1,024 font bytes in the owners (ibm_font), unchanged");
    check(report.firmware_match && report.file_size == tdeck::kExpectedAlphaResourceSize &&
              report.payload_crc32 == tdeck::kExpectedAlphaResourceCrc32,
          "P4 the firmware's identity lock names THIS pack (" + n(report.file_size) + " B)");
    owners.release();
    source.close();

    auto stale = bytes;
    int err = -1;
    if (rename_entry(stale, "ibm.ch", "a4-ui1-renamed.ch")) {
        const std::string path = std::string(pack_path) + ".a4-ui1-stale.tmp";
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char *>(stale.data()), std::streamsize(stale.size()));
        }
        tdeck::AlphaResourcePack probe;
        tdeck::AlphaResourceReport r{};
        err = probe.open(path.c_str(), r);
        probe.close();
        std::remove(path.c_str());
    }
    check(err == ESP_ERR_NOT_FOUND, "P5 a pack without \"ibm.ch\" is refused by NAME at open() (ESP_ERR_NOT_FOUND, got " +
                                        std::to_string(err) + ")");
}

// ---------------------------------------------------------------------------
// Reading the panel.
// ---------------------------------------------------------------------------
uint16_t px(int x, int y) { return bus::gram()[y * 320 + x]; }
uint64_t fnv_screen() {
    uint64_t h = 1469598103934665603ull;
    const uint16_t *p = bus::gram();
    for (size_t i = 0; i < 320 * 240; ++i) {
        h = (h ^ (p[i] & 0xff)) * 1099511628211ull;
        h = (h ^ (p[i] >> 8)) * 1099511628211ull;
    }
    return h;
}
bool ibm_bit(char c, int row, int col) { return (pack->ibm_font[size_t(uint8_t(c) & 0x7f) * 8 + size_t(row)] & (0x80U >> col)) != 0; }
/** Pixels of an 8x8 IBM.CH text run at (x0,y) that differ from `fg` on `bg`. */
int ibm_mismatch(int x0, int y, const std::string &text, uint16_t fg, uint16_t bg) {
    int bad = 0;
    for (size_t i = 0; i < text.size(); ++i)
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) bad += px(x0 + int(i) * 8 + c, y + r) != (ibm_bit(text[i], r, c) ? fg : bg);
    return bad;
}
/** The compact (6x8 cell) run's per-cell occupancy and colours. */
struct Cells { std::string occupied; bool only[2]{}; };
std::string compact_occupancy(int x0, int y, size_t cells, uint16_t *colours_seen = nullptr) {
    std::string out;
    for (size_t i = 0; i < cells; ++i) {
        bool lit = false;
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 6; ++c) {
                const uint16_t p = px(x0 + int(i) * 6 + c, y + r);
                if (p != kBlack) {
                    lit = true;
                    if (colours_seen) colours_seen[i] = p;
                }
            }
        out += lit ? '#' : '.';
    }
    return out;
}
std::string expected_occupancy(const std::string &text, size_t cells) {
    std::string out;
    for (size_t i = 0; i < cells; ++i) out += i < text.size() && text[i] != ' ' ? '#' : '.';
    return out;
}
/** Of a rectangle, how many pixels are `colour`. */
int count_colour(int x0, int y0, int w, int h, uint16_t colour) {
    int k = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) k += px(x, y) == colour;
    return k;
}
std::string split15(const std::string &left, const std::string &right) {
    std::string s = left;
    while (s.size() + right.size() < 15) s += ' ';
    return s + right;
}

// A PNG of the GRAM (stored deflate blocks; no zlib on the host toolchain).
void dump(const char *name) {
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
    auto crc = [](const std::vector<uint8_t> &d, size_t from) {
        uint32_t c = 0xffffffffU;
        for (size_t i = from; i < d.size(); ++i) {
            c ^= d[i];
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
        be32(png, crc(body, 0));
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

std::vector<std::pair<std::string, uint64_t>> g_states; // G: every scripted screen
void state(const char *name) {
    g_states.emplace_back(name, fnv_screen());
    dump(name);
}

// ---------------------------------------------------------------------------
// The runtime on the real Board (A3-04E / A3-04F's harness, untimed).
// ---------------------------------------------------------------------------
struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    Run() {
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
        f.render_pixels = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.time.year = 139;
        g.time.month = 4;
        g.time.day = 5;
        g.time.hour = 6;
        g.time.minute = 40;
        g.food = 246;
        g.gold = 411;
        g.party.character_count = g.party.party_size = 4;
        const char *names[] = {"Avatar", "Shamino", "Iolo", "Jaana"};
        const uint16_t hp[] = {112, 98, 87, 74};
        const char status[] = {'G', 'G', 'P', 'S'};
        for (int i = 0; i < 4; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", names[i]);
            m.status = uint8_t(status[i]);
            m.party_status = 0; // in the party (INIT.GAM's other slots wait elsewhere)
            m.character_class = 'A';
            m.level = 1;
            m.current_hp = hp[i];
            m.max_hp = uint16_t(hp[i] + 5);
            m.strength = m.dexterity = m.intelligence = 20;
        }
        g.party.active_character = 1; // Shamino: the -> and today's green tint
        render(true);
    }
    void render(bool force = false) { rt->render(board, force); }
};

// A direct show_alpha over the runtime's own session and viewport, with a
// changed game / movement mode / dungeon bands.
esp_err_t direct(Run &h, const GameState &game, bool move, const HudDungeonBands *bands = nullptr) {
    const auto hud = hud_world_state(game, h.rt->turn(), nullptr, 0, bands && bands->active);
    return h.board.show_alpha(h.rt->composed_viewport(), *h.rt->ui(), game, h.rt->turn(), hud, pack->runes_font,
                              nullptr, nullptr, false, nullptr, move, 1, nullptr, nullptr, nullptr, {}, 0x5a5a1234U,
                              bands, false, false);
}

void test_gameplay() {
    std::printf("\nC/R/S/B the gameplay chrome\n");
    Run h;
    h.render(true);
    state("gameplay");

    // C -- the frame and the boxes.
    int rule318 = 0, rule318_total = 0;
    for (int y = 2; y <= 86; ++y) {
        if (y == 54 || y == 55) continue;
        ++rule318_total;
        rule318 += px(318, y) == kWhite;
    }
    check(rule318 == rule318_total, "C1 the right rule x=318 is whole beside the roster and status boxes after a full "
                                    "render (" + n(rule318) + " of " + n(rule318_total) + " rows white)");
    struct Probe { int x, y; uint16_t want; const char *what; };
    const Probe probes[] = {
        {1, 100, kBand, "left frame band"},          {3, 100, kWhite, "play-window rule, left"},
        {180, 100, kWhite, "play-window rule, right"}, {181, 100, kBand, "separator band"},
        {90, 2, kBand, "top frame band"},            {90, 182, kBand, "frame band under the wind strip"},
        {90, 210, kBand, "touch reserve (frame band)"}, {10, 239, kBand, "touch reserve, bottom"},
        {250, 0, kBand, "band above the boxes"},     {319, 40, kBand, "band right of the boxes"},
        {250, 54, kBand, "blue bar between roster and status boxes"},
        {250, 2, kWhite, "roster box top rule"},     {182, 30, kWhite, "roster box left rule"},
        {250, 53, kWhite, "roster box bottom rule"}, {250, 56, kWhite, "status box top rule"},
        {250, 86, kWhite, "status box bottom rule"}, {182, 150, kWhite, "console left rule"},
        {319, 150, kBlack, "console runs to the glass"},
    };
    int good = 0;
    std::string bad;
    for (const auto &p : probes) {
        if (px(p.x, p.y) == p.want) ++good;
        else if (bad.size() < 200) bad += std::string(bad.empty() ? "" : "; ") + p.what + " (" + n(p.x) + "," + n(p.y) +
                                          ")=" + hex4(px(p.x, p.y));
    }
    check(good == int(std::size(probes)), "C2 the frame band, rules and boxes are where the approved mockup puts them (" +
                                              n(good) + " of " + n(std::size(probes)) + ")" + (bad.empty() ? "" : ": " + bad));
    int cyan = 0;
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x)
            if (!(x >= 184 && y >= 58 && y < 66) && !(x >= 184 && y >= 88)) cyan += px(x, y) == kCyan;
    check(cyan == 0, "C3 no #00FFFF frame is left anywhere outside the location caption and the console (" + n(cyan) +
                         " cyan pixels)");

    // R -- the roster, roster.ts's row.
    const auto &g = h.rt->game();
    int roster_bad = 0;
    std::string roster_rows;
    for (int r = 0; r < 4; ++r) {
        const auto &m = g.party.characters[r];
        const bool arrow = r == g.party.active_character && m.status != 'D' && m.status != 'S';
        char line[24];
        std::snprintf(line, sizeof(line), "%-9.9s%c%4u%c", m.name, arrow ? '\x1a' : ' ', unsigned(m.current_hp), char(m.status));
        const uint16_t fg = r == g.party.active_character ? kGreen : kWhite; // today's tint, kept
        const int bad_px = ibm_mismatch(188, 4 + r * 8, line, fg, kBlack);
        roster_bad += bad_px;
        roster_rows += std::string(roster_rows.empty() ? "" : " | ") + line;
    }
    check(roster_bad == 0, "R1 the roster rows are IBM.CH at x=188: name(9) -> HP(4) status, the active member tinted "
                           "as before (" + n(roster_bad) + " pixels differ)");
    auto &gm = h.rt->game();
    gm.party.active_character = 3; // Jaana is asleep ('S'): no arrow
    h.render(true);
    char sleeping[24];
    std::snprintf(sleeping, sizeof(sleeping), "%-9.9s%c%4u%c", "Jaana", ' ', 74u, 'S');
    check(ibm_mismatch(188, 4 + 3 * 8, sleeping, kGreen, kBlack) == 0,
          "R2 no -> on an active member who is asleep (roster.ts: the arrow is suppressed for 'S' and 'D')");
    gm.party.active_character = 1;
    h.render(true);

    // S -- the status box.
    const std::string food = split15("F:246", "G:411"), date = split15("4-5-139", "06:40");
    const int food_bad = ibm_mismatch(188, 68, food, kWhite, kBlack), date_bad = ibm_mismatch(188, 78, date, kWhite, kBlack);
    check(food_bad == 0, "S1 F:246 on the left, G:411 on the right, IBM.CH at y=68 (" + n(food_bad) + " pixels differ)");
    check(date_bad == 0, "S2 the date 4-5-139 (month-day-year) with the digital time 06:40 kept on the right, IBM.CH at "
                         "y=78 (" + n(date_bad) + " pixels differ)");
    const std::string here = tdeck::hud_location_caption(g.position.map.location, g.position.map.floor, false, 0);
    const std::string occ = compact_occupancy(184, 58, 22);
    check(occ == expected_occupancy(here, 22), "S3 the location caption (\"" + here + "\") keeps its compact font at y=58 (" +
                                                   occ + ")");

    // S4/S5 -- Movement Mode at the two longest names: clipped to 17, then MOVE.
    for (const uint8_t location : {uint8_t(17), uint8_t(18)}) {
        GameState copy = g;
        copy.position.map.location = location;
        copy.position.map.floor = 0;
        const std::string name = tdeck::location_display_name(location);
        char want[32];
        std::snprintf(want, sizeof(want), "%-17.17s MOVE", name.c_str());
        direct(h, copy, true);
        uint16_t seen[22]{};
        const std::string lit = compact_occupancy(184, 58, 22, seen);
        bool colours = true;
        for (size_t i = 0; i < 22; ++i)
            if (lit[i] == '#') colours = colours && seen[i] == (i >= 18 ? kGreen : kCyan);
        check(lit == expected_occupancy(want, 22) && colours,
              std::string("S") + (location == 17 ? "4" : "5") + " Movement Mode at \"" + name + "\": the caption reads \"" +
                  want + "\" -- clipped to 17, one blank cell, MOVE in green (" + lit + ")");
        state(location == 17 ? "move-lord-british" : "move-blackthorn");
        direct(h, copy, false);
        const std::string whole = compact_occupancy(184, 58, 22);
        check(whole == expected_occupancy(name, 22), std::string("S") + (location == 17 ? "6" : "7") +
                                                         " out of Movement Mode the whole name shows (" + whole + ")");
    }
    h.render(true);

    // B -- the strips as bands.
    int rule12 = count_colour(4, 12, 176, 1, kWhite), rule171 = count_colour(4, 171, 176, 1, kWhite);
    check(rule12 == 176 && rule171 == 176, "B1 the sky strip's last row and the wind strip's first row are the white rule "
                                           "that bounds the map (" + n(rule12) + ", " + n(rule171) + " of 176)");
    const auto hud = hud_world_state(g, h.rt->turn(), nullptr, 0, false);
    const bool sky_band = px(4, 6) == kBand && px(179, 6) == kBand && px(44, 11) == kBlack && px(139, 11) == kBlack;
    // The left >: the white edge's first stroke is row 1, columns 1-2 of the cell before the notch.
    const bool bracket = px(4 + 32 + 1, 5) == kWhite && px(4 + 32 + 2, 5) == kWhite && px(4 + 32 + 0, 5) == kBand;
    check(hud.sky_visible && sky_band && bracket, "B2 the sky strip is band with a black notch over the 12-cell track "
                                                  "and the original's > < ends");
    std::string wind = hud.wind_visible ? hud.wind : "";
    const int ww = int(wind.size()) * 8 + 4, wx = 4 + (176 - ww) / 2 + 2;
    const int wind_bad = wind.empty() ? 1 : ibm_mismatch(wx, 172, wind, kWhite, kBlack);
    check(wind_bad == 0, "B3 the wind strip's caption is the wind table's own text in IBM.CH, centred in the notch (\"" +
                             wind + "\", " + n(wind_bad) + " pixels differ)");
    HudDungeonBands bands{};
    bands.active = true;
    std::snprintf(bands.level, sizeof(bands.level), "L3");
    std::snprintf(bands.direction, sizeof(bands.direction), "Dir:  North");
    direct(h, g, false, &bands);
    const int lw = 2 * 8 + 4, dw = 11 * 8 + 4;
    const int level_bad = ibm_mismatch(4 + (176 - lw) / 2 + 2, 4, "L3", kWhite, kBlack);
    const int dir_bad = ibm_mismatch(4 + (176 - dw) / 2 + 2, 172, "Dir:  North", kWhite, kBlack);
    check(level_bad == 0 && dir_bad == 0, "B4 a mounted dungeon captions the same bands: L3 above, Dir:  North below (" +
                                              n(level_bad) + ", " + n(dir_bad) + " pixels differ)");
    state("dungeon-bands");
    h.render(true);
}

// ---------------------------------------------------------------------------
// F -- the frontend.
// ---------------------------------------------------------------------------
UiAction ch(char c) {
    UiAction a{};
    a.kind = UiActionKind::Character;
    a.character = char16_t(c);
    return a;
}
UiAction south() {
    UiAction a{};
    a.kind = UiActionKind::Direction;
    a.direction = Direction::South;
    return a;
}
bool inverted(int y, int h) { return count_colour(8, y, 304, h, kWhite) * 2 > 304 * h; }
bool plain(int y, int h) { return count_colour(8, y, 304, h, kBlack) * 2 > 304 * h; }
bool caption_ok(const std::string &title) {
    const int w = int(title.size()) * 8 + 4, x = (320 - w) / 2 + 2;
    return px(0, 0) == kBand && px(160, 11) == kBand && ibm_mismatch(x, 2, title, kWhite, kBlack) == 0 &&
           count_colour(2, 12, 316, 1, kWhite) == 316 && px(1, 120) == kBand && px(2, 120) == kWhite &&
           px(317, 120) == kWhite && px(319, 120) == kBand && count_colour(2, 237, 316, 1, kWhite) == 316;
}
bool footer_grey() {
    const int grey = count_colour(8, 228, 304, 9, kDim), green = count_colour(8, 228, 304, 9, kGreen);
    return grey > 0 && green == 0;
}

void test_frontend() {
    std::printf("\nF the frontend shell\n");
    Run h;
    FrontendSettings settings{};
    for (const uint8_t size : {uint8_t(1), uint8_t(2)}) {
        const auto m = tdeck::ui_text_metrics(size);
        FrontendSession s;
        s.start(0, true, settings);
        s.handle(ch(' '), 10);
        s.handle(ch('S'), 20);
        auto v = s.view();
        h.board.show_frontend(v, nullptr, nullptr, nullptr, nullptr, size);
        const int step = m.line_height + 3, box = m.line_height + 2;
        const bool first = inverted(23, box) && plain(23 + step, box);
        const std::string tag = size == 1 ? "" : " (Large)";
        check(v.kind == FrontendViewKind::Settings && caption_ok("Settings") && first && footer_grey(),
              "F" + std::string(size == 1 ? "1" : "4") + " title Settings" + tag + ": >Settings< in the top band, white-ruled "
              "content window, row 0 in reverse video, grey footer");
        state(size == 1 ? "settings" : "settings-large");
        const auto before = bus::stats();
        s.handle(south(), 30);
        v = s.view();
        h.board.show_frontend(v, nullptr, nullptr, nullptr, nullptr, size);
        const auto moved = bus::stats();
        const uint64_t windows = moved.windows - before.windows, pixels = (moved.pixel_bytes - before.pixel_bytes) / 2;
        check(plain(23, box) && inverted(23 + step, box) && plain(23 + 2 * step, box) && windows == 2 &&
                  pixels == uint64_t(2 * 304 * box),
              "F" + std::string(size == 1 ? "2" : "5") + " moving the selection moves the reverse video" + tag +
                  " (row 0 plain, row 1 inverted) and redraws exactly those two rows: " + n(windows) + " windows, " +
                  n(pixels) + " px (Alpha 3: 2 x 304 x " + n(m.line_height) + " = " + n(2 * 304 * m.line_height) + ")");
        UiAction east{};
        east.kind = UiActionKind::Direction;
        east.direction = Direction::East;
        s.handle(south(), 40); // row 2: Trackball
        v = s.view();
        h.board.show_frontend(v, nullptr, nullptr, nullptr, nullptr, size);
        const auto at_trackball = bus::stats();
        s.handle(east, 50);
        v = s.view();
        h.board.show_frontend(v, nullptr, nullptr, nullptr, nullptr, size);
        const auto changed = bus::stats();
        const uint64_t cw = changed.windows - at_trackball.windows, cp = (changed.pixel_bytes - at_trackball.pixel_bytes) / 2;
        check(cw == 1 && cp == uint64_t(304 * box) && inverted(23 + 2 * step, box),
              "F" + std::string(size == 1 ? "7" : "8") + " changing a value redraws that one row, still inverted" + tag +
                  ": " + n(cw) + " window, " + n(cp) + " px");
    }
    FrontendSaveCatalog slots{};
    SystemMenuSession menu;
    menu.open(settings, slots);
    const auto sv = menu.view();
    h.board.show_frontend(sv, nullptr, nullptr, nullptr, nullptr, 1);
    check(caption_ok("System Menu") && inverted(23, 10) && plain(34, 10) && footer_grey(),
          "F3 System Menu: >System Menu< band caption, Resume in reverse video, grey footer");
    state("system-menu");

    FrontendSession title;
    title.start(0, true, settings);
    title.handle(ch(' '), 10);
    const auto mv = title.view();
    h.board.show_frontend(mv, nullptr, pack->intro_title, nullptr, nullptr, 1);
    check(mv.kind == FrontendViewKind::Menu && inverted(115, 10) && plain(126, 10) && footer_grey(),
          "F6 the main menu under the title art uses the same reverse-video selection (Journey Onward) and grey footer");
    state("main-menu");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui1_chrome_runtime <pack> <IBM.CH> [--record <goldens.h>] [--dump <dir>]\n");
        return 2;
    }
    const char *record = nullptr;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--record")) record = argv[i + 1];
        if (!std::strcmp(argv[i], "--dump")) g_dump = argv[i + 1];
    }
    test_pack(argv[1], argv[2]);

    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    static tdeck::AlphaResourceOwners owners{};
    if (source.open(argv[1], report) != ESP_OK || source.load(owners, report) != ESP_OK || !owners.ibm_font) {
        std::printf("RED the pack does not load with an ibm.ch -- the chrome checks cannot run\n");
        std::printf("A4-UI1 chrome runtime: %d/%d checks\n", checks - failures, checks + 1);
        return 1;
    }
    pack = &owners;
    g_dungeon_count = report.dungeon_count;
    test_gameplay();
    test_frontend();

    if (record) {
        FILE *out = std::fopen(record, "wb");
        if (!out) return 3;
        std::fprintf(out, "#pragma once\n// Alpha 4 UI Batch 1 (ALPHA4_UI.md): the whole 320x240 screen after each scripted\n"
                          "// state of a4_ui1_chrome_runtime, FNV-1a over every pixel. Recorded from the\n"
                          "// reviewed A4-UI1 Board with `a4_ui1_chrome_runtime <pack> <IBM.CH> --record <this file>`.\n"
                          "#include <cstddef>\n#include <cstdint>\nnamespace a4_ui1_goldens {\n");
        std::fprintf(out, "constexpr size_t kCount = %zu;\nconstexpr const char *kName[%zu] = {", g_states.size(), g_states.size());
        for (size_t i = 0; i < g_states.size(); ++i) std::fprintf(out, "%s\"%s\"", i ? ", " : "", g_states[i].first.c_str());
        std::fprintf(out, "};\nconstexpr uint64_t kScreen[%zu] = {\n", g_states.size());
        for (size_t i = 0; i < g_states.size(); ++i)
            std::fprintf(out, "    0x%016llxull, // %s\n", (unsigned long long)g_states[i].second, g_states[i].first.c_str());
        std::fprintf(out, "};\n} // namespace a4_ui1_goldens\n");
        std::fclose(out);
        std::printf("recorded %zu screen states to %s\n", g_states.size(), record);
    }
    size_t same = 0;
    std::string differ;
    for (size_t i = 0; i < g_states.size() && i < a4_ui1_goldens::kCount; ++i) {
        if (g_states[i].first == a4_ui1_goldens::kName[i] && g_states[i].second == a4_ui1_goldens::kScreen[i]) ++same;
        else differ += std::string(differ.empty() ? "" : ", ") + g_states[i].first;
    }
    check(a4_ui1_goldens::kCount == g_states.size() && same == g_states.size(),
          "G1 every scripted screen equals the recorded A4-UI1 golden (" + n(same) + " of " + n(g_states.size()) +
              " states; " + n(a4_ui1_goldens::kCount) + " recorded" + (differ.empty() ? "" : "; differ: " + differ) + ")");

    std::printf("A4-UI1 chrome runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
