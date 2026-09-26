// Batch 53 -- the four Alpha 2 release blockers of the Batch 52 audit, driven
// through the T-Deck's OWN service/data binding shape.
//
//   RB-1 / H-189  the Wooden-Box victory ending and the final Doom arena
//   RB-2 / H-187  Words of Power and the eight dungeon seals
//   RB-3 / H-188 / H-191  moonstones, moongates, Vas Rel Por, the night gate tile
//   RB-4 / H-146  ship / skiff / horse purchases
//
// Batch 52's lesson is that the parity drivers bind core hooks the device
// never binds. Nothing in this file binds a hook: every service a check uses is
// whatever AlphaRuntime put on its CommandContext (quest_world, shop_services,
// rest_services), the data is the shipped SD pack, and every action goes
//
//   RawInputEvent -> UiInputAdapter -> UiSession -> AlphaRuntime::dispatch()
//
// exactly as a key on the device does. The pack is also read here byte for
// byte from the file, independently of the firmware loader, so a device string
// is compared with the original record and not with itself.
//
// Seam: the Batch 11 host fixture, which since Batch 53 calls the production
// binders (bind_quest_services / bind_shop_services) instead of re-declaring
// them; section B checks that from the two source files themselves.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"

#include "esp_timer.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/debug_map_picker.h"
#include "openu5/dungeon.h"
#include "openu5/presentation.h"
#include "openu5/quest_state.h"
#include "openu5/quest_world.h"
#include "openu5/rest.h"
#include "openu5/shops.h"
#include "openu5/world_commands.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state));
std::vector<uint8_t> host_memory_save_gam_for_test();
} // namespace tdeck

using namespace openu5;
using tdeck::RawInputKind;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

const tdeck::AlphaResourceOwners *g_owners = nullptr;
tdeck::AlphaResourceReport g_report{};
std::vector<DungeonArena> g_arenas;
const char *g_pack_path = nullptr;
const char *g_stale_pack_path = nullptr;

// ---------------------------------------------------------------------------
// The pack, read straight from the file (not through AlphaResourcePack).
// ---------------------------------------------------------------------------
std::vector<uint8_t> g_pack_bytes;
uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
struct RawEntry { std::string name; uint32_t offset = 0, length = 0; };
std::vector<RawEntry> raw_entries() {
    std::vector<RawEntry> out;
    if (g_pack_bytes.size() < 32) return out;
    const uint32_t n = le32(g_pack_bytes.data() + 16);
    for (uint32_t i = 0; i < n && 32 + (i + 1) * 64 <= g_pack_bytes.size(); ++i) {
        const uint8_t *t = g_pack_bytes.data() + 32 + i * 64;
        RawEntry e; e.name.assign(reinterpret_cast<const char *>(t), strnlen(reinterpret_cast<const char *>(t), 32));
        e.offset = le32(t + 32); e.length = le32(t + 36); out.push_back(e);
    }
    return out;
}
// The string-record layout of misc-records.bin (u32 count, count+1 offsets, blob).
std::vector<std::string> raw_records(const char *name) {
    std::vector<std::string> rows;
    for (const auto &e : raw_entries()) {
        if (e.name != name || e.offset + e.length > g_pack_bytes.size() || e.length < 4) continue;
        const uint8_t *p = g_pack_bytes.data() + e.offset;
        const uint32_t count = le32(p);
        const size_t dir = 4 + size_t(count + 1) * 4;
        if (dir > e.length) return rows;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t a = le32(p + 4 + i * 4);
            if (dir + a >= e.length) return {};
            rows.emplace_back(reinterpret_cast<const char *>(p + dir + a));
        }
    }
    return rows;
}
const RawEntry *raw_entry(const std::vector<RawEntry> &v, const char *name) {
    for (const auto &e : v) if (e.name == name) return &e;
    return nullptr;
}

std::string read_file(const char *path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
size_t occurrences(const std::string &hay, const std::string &needle) {
    size_t n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size())) ++n;
    return n;
}
// The body of `signature`'s function: from its first '{' to the matching '}'.
std::string function_body(const std::string &src, const char *signature) {
    const size_t at = src.find(signature);
    if (at == std::string::npos) return {};
    size_t open = src.find('{', at);
    if (open == std::string::npos) return {};
    int depth = 0;
    for (size_t i = open; i < src.size(); ++i) {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}' && --depth == 0) return src.substr(open, i - open + 1);
    }
    return {};
}

std::u16string widen(const std::string &s) { return std::u16string(s.begin(), s.end()); }

// ---------------------------------------------------------------------------
// The harness: the real AlphaRuntime over the shipped pack.
// ---------------------------------------------------------------------------
struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    size_t mark = 0;
    explicit Harness(bool render = false) {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.render_pixels = render;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.dungeons = g_owners->dungeons; hf.dungeon_count = g_report.dungeon_count;
        hf.arenas = g_arenas.data(); hf.arena_count = g_arenas.size();
        hf.enemy_defs = g_owners->combat_enemy_views; hf.enemy_def_count = g_owners->combat_enemy_count;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30; ch.intelligence = 30; ch.level = 8; ch.current_mp = 99;
        g.party.active_character = 0;
        g.time.hour = 12; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.karma = 50; g.gold = 321; g.food = 900;
        g.position.map = {0, 0};
        g.transport = TransportMode::Foot; rt->turn().transport_tile = 0x1c;
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const std::vector<QuestObject> &pool() const { return rt->objects_for_test(); }
    UiMode mode() const { return rt->ui()->mode(); }
    const DungeonState &d() const { return rt->dungeon_state(); }

    static void advance(int64_t us) { openu5_host_virtual_clock_us() += us; }
    bool raw_key(uint8_t code, bool alt = false) {
        advance(100000);
        tdeck::RawInputEvent raw{};
        raw.kind = RawInputKind::Keyboard; raw.code = code; raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    bool key(uint8_t code) { return raw_key(code); }
    void type(const char *s) { for (; *s; ++s) key(uint8_t(*s)); }
    bool ball(RawInputKind kind) {
        advance(100000);
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    // The device loop (main.cpp): 5 ms per pass, render() each pass, which is
    // where the scene pacers release their beats. Needs Harness(true).
    void run_ms(int64_t ms) { for (int64_t t = 0; t < ms; t += 5) { advance(5000); rt->render(board); } }
    void north() { ball(RawInputKind::TrackballUp); }
    void south() { ball(RawInputKind::TrackballDown); }
    void east() { ball(RawInputKind::TrackballRight); }
    void west() { ball(RawInputKind::TrackballLeft); }
    void step(Direction d) {
        if (d == Direction::North) north(); else if (d == Direction::South) south();
        else if (d == Direction::East) east(); else west();
    }

    void set_mark() { mark = rt->ui()->transcript_size(); }
    size_t count(const char *needle) const {
        size_t n = 0;
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) ++n;
        return n;
    }
    bool saw(const char *needle) const { return count(needle) > 0; }
    void dump(const char *tag) const {
        std::printf("         transcript since mark (%s):\n", tag);
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) std::printf("           | %s\n", b->text);
    }

    // The device's own shortcuts / menus.
    void alt_save() { raw_key('s', true); }
    void alt_load() { raw_key('l', true); }
    void menu_save() { raw_key('m', true); ball(RawInputKind::TrackballDown); key('\r'); raw_key('m', true); }
    void menu_load() { raw_key('m', true); ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown); key('\r'); key('\r'); }

    // Yell <word> the way a player types it.
    void yell(const char *word) { key('y'); type(word); key('\r'); }
    void to_surface(int x, int y, int16_t floor = 0) { g().position.map = {0, floor}; g().position.xy = {uint8_t(x), uint8_t(y)}; }
    // Doom (location 40) opens onto the Underworld; the other seven onto Britannia.
    static int16_t entrance_floor(int dungeon) { return dungeon == 40 ? int16_t(255) : int16_t(0); }

    const QuestWorldServices *quest() { return ctx().quest_world; }
    int32_t tile(int x, int y) { return quest_world_tile(ctx(), g().position.map, x, y, get_active_map(ctx().world, g().position.map).value.tile_at(x, y)); }
};

// A walkable (on foot, no object/actor) neighbour of a surface cell.
bool free_neighbour(Harness &h, int x, int y, Direction &from, int &nx, int &ny) {
    constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
    for (auto d : dirs) {
        const auto dd = direction_delta(d);
        const int cx = (x + dd.dx) & 255, cy = (y + dd.dy) & 255;
        const int t = h.tile(cx, cy);
        if (t < 0 || !is_passable(t, TransportMode::Foot).value) continue;
        // `from` is the direction from the neighbour INTO (x, y).
        from = d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
             : d == Direction::East ? Direction::West : Direction::East;
        nx = cx; ny = cy; return true;
    }
    return false;
}

// ===========================================================================
// P. The SD resource pack: three new canonical sections and the stale-pack gate.
// ===========================================================================
bool rewrite_entry_name(std::vector<uint8_t> &bytes, const char *from, const char *to) {
    const uint32_t n = le32(bytes.data() + 16);
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t *t = bytes.data() + 32 + i * 64;
        if (std::strncmp(reinterpret_cast<const char *>(t), from, 32) != 0) continue;
        std::memset(t, 0, 32); std::memcpy(t, to, std::strlen(to));
        uint32_t crc = 0xffffffffU;                          // re-seal the TOC CRC (header +28)
        for (size_t k = 32; k < 32 + size_t(n) * 64; ++k) {
            crc ^= bytes[k];
            for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
        crc ^= 0xffffffffU;
        for (int b = 0; b < 4; ++b) bytes[28 + b] = uint8_t(crc >> (8 * b));
        return true;
    }
    return false;
}

void test_pack() {
    std::printf("P  the resource pack: ENDMSG / KARMA / Words of Power, and the stale-pack gate\n");
    const auto entries = raw_entries();
    const auto end = raw_records("endmsg-records.bin");
    const auto karma = raw_records("karma-records.bin");
    const auto words = raw_records("words-of-power.bin");
    expect(end.size() == 11 && end[9].find("\"FOLLOW!\" cries Lord British") == 0, "P1",
           "endmsg-records.bin: the 11 ENDMSG.DAT records; record 9 is the wooden-box line");
    expect(karma.size() == 6 && karma[0].find("Thou hast strayed far") == 0 &&
               karma[5].find("Thy destiny awaits thee!") != std::string::npos, "P2",
           "karma-records.bin: the 6 KARMA.DAT records, unquoted");
    expect(words.size() == 8 && words[0] == "FALLAX" && words[7] == "VERAMOCOR", "P3",
           "words-of-power.bin: DATA.OVL 0x44AD, FALLAX .. VERAMOCOR");
    expect(g_report.firmware_match && g_report.file_size == tdeck::kExpectedAlphaResourceSize &&
               g_report.payload_crc32 == tdeck::kExpectedAlphaResourceCrc32, "P4",
           "the firmware's identity lock names THIS pack (size + payload CRC)");

    // A pack that lacks any one of the three sections is refused by name at
    // open(), before a byte of it is used -- whatever its size and CRC.
    const char *required[] = {"endmsg-records.bin", "karma-records.bin", "words-of-power.bin"};
    int refused = 0;
    for (const char *name : required) {
        auto bytes = g_pack_bytes;
        if (!rewrite_entry_name(bytes, name, "batch53-renamed.bin")) continue;
        const std::string path = std::string(g_pack_path) + ".batch53-stale.tmp";
        { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size())); }
        tdeck::AlphaResourcePack probe; tdeck::AlphaResourceReport r{};
        const auto err = probe.open(path.c_str(), r);
        probe.close(); std::remove(path.c_str());
        if (err == ESP_ERR_NOT_FOUND) ++refused;
        else std::printf("         %s missing: open() returned %d\n", name, int(err));
    }
    expect(refused == 3, "P5", "** a pack missing endmsg / karma / words is rejected at open() (ESP_ERR_NOT_FOUND) **");
    if (g_stale_pack_path) {
        tdeck::AlphaResourcePack stale; tdeck::AlphaResourceReport r{};
        const auto err = stale.open(g_stale_pack_path, r);
        stale.close();
        expect(err == ESP_ERR_NOT_FOUND && !r.firmware_match, "P6",
               "** the Batch 51 pack (2,039,545 B, 434cd664) is rejected by this firmware **");
    }
}

// ===========================================================================
// B. Production binder completeness -- behaviour, not pointers.
// ===========================================================================
void test_binders() {
    std::printf("B  the device binds every hook RB-1..RB-4 need, from the pack\n");
    Harness h;
    const auto *q = h.quest();
    const auto *shop = h.ctx().shop_services;
    const auto *rest = h.ctx().rest_services;
    const auto end = raw_records("endmsg-records.bin");
    const auto karma = raw_records("karma-records.bin");
    const auto words = raw_records("words-of-power.bin");

    bool end_ok = q && q->end_record && end.size() == 11;
    for (int i = 0; end_ok && i < 11; ++i) {
        const char *s = q->end_record(q->context, i);
        end_ok = s && end[size_t(i)] == s;
    }
    end_ok = end_ok && !q->end_record(q->context, 11) && !q->end_record(q->context, -1);
    expect(end_ok, "B1", "** quest_world.end_record(0..10) is ENDMSG.DAT byte for byte; out of range is null **");

    bool karma_ok = q && q->karma_record && rest && rest->karma_record && karma.size() == 6;
    for (int i = 0; karma_ok && i < 6; ++i) {
        const std::string want = "\"" + karma[size_t(i)] + "\"";
        const char *a = q->karma_record(q->context, i), *b = rest->karma_record(rest->context, i);
        karma_ok = a && b && want == a && want == b;
    }
    expect(karma_ok, "B2", "** quest and rest karma_record(0..5) are the quoted KARMA.DAT records (H-190) **");
    expect(!(rest && rest->karma_record && std::strstr(rest->karma_record(rest->context, 2), "Rest well, Avatar")), "B2b",
           "the invented \"Rest well, Avatar\" line is gone");

    bool words_ok = q && q->words.size == 8 && words.size() == 8;
    for (size_t i = 0; words_ok && i < 8; ++i) words_ok = q->words[i] == widen(words[i]);
    expect(words_ok, "B3", "** quest_world.words is the eight Words of Power **");

    const uint8_t *gam = h.rt->game().party.character_count ? g_owners->initial_gam : nullptr;
    bool stones_ok = q && q->moonstones && q->moonstone_count == 8 && gam;
    for (size_t i = 0; stones_ok && i < 8; ++i) {
        const auto &m = q->moonstones[i];
        stones_ok = m.x == gam[0x28a + i] && m.y == gam[0x292 + i] && m.location == gam[0x29a + i] &&
                    m.z == int16_t(gam[0x2a2 + i]) && m.buried == (gam[0x29a + i] != 255);
    }
    expect(stones_ok, "B4", "** quest_world.moonstones: 8 stones owned by the runtime, hydrated from INIT.GAM 0x28a **");
    expect(shop && shop->ship && shop->horse && shop->reserve, "B5",
           "shop_services.ship / horse / reserve are bound (exercised in S)");

    // The fixture must not own device semantics: it calls the production
    // binders, and no QuestWorldServices / ShopServices hook is assigned in it.
    const std::string runtime = read_file(OPENU5_RUNTIME_SOURCE), fixture = read_file(OPENU5_FIXTURE_SOURCE);
    const std::string init = function_body(runtime, "esp_err_t AlphaRuntime::initialize(");
    const std::string attach = function_body(fixture, "void AlphaRuntime::attach_host_test_fixture(");
    const bool read_ok = !init.empty() && !attach.empty();
    expect(read_ok && occurrences(init, "bind_quest_services();") == 1 && occurrences(attach, "bind_quest_services();") == 1 &&
               occurrences(init, "bind_shop_services();") == 1 && occurrences(attach, "bind_shop_services();") == 1 &&
               occurrences(init, "bind_rest_services();") == 1 && occurrences(attach, "bind_rest_services();") == 1,
           "B6", "** initialize() and the host fixture each call the same production binders once **");
    expect(read_ok && occurrences(attach, "quest_.") == 0 && occurrences(attach, "shop_services_.") == 0 &&
               occurrences(init, "quest_.") == 0 && occurrences(init, "shop_services_.") == 0, "B7",
           "** neither initialize() nor the fixture assigns a quest/shop hook outside those binders **");
}

// ===========================================================================
// W. RB-2 / H-187: Words of Power open the dungeon seals.
// ===========================================================================
constexpr const char *kWords[8] = {"FALLAX", "VILIS", "INOPIA", "MALUM", "AVIDUS", "INFAMA", "IGNAVUS", "VERAMOCOR"};
constexpr const char *kDungeons[8] = {"Deceit", "Despise", "Destard", "Wrong", "Covetous", "Shame", "Hythloth", "Doom"};
QuestFlag word_flag(int i) { return QuestFlag(int(QuestFlag::Word33) + i); }
int word_flags(const GameState &g) {
    int bits = 0;
    for (int i = 0; i < 8; ++i) if (quest_flag(g.quest, word_flag(i))) bits |= 1 << i;
    return bits;
}

void test_words() {
    std::printf("W  Words of Power at the dungeon entrances\n");
    {
        Harness h;
        const int ex = g_owners->location_x[32], ey = g_owners->location_y[32];
        Direction in{}; int nx = 0, ny = 0;
        expect(free_neighbour(h, ex, ey, in, nx, ny), "W0", "precondition: a walkable cell next to Deceit's entrance");
        h.to_surface(nx, ny);
        expect(word_flags(h.g()) == 0 && h.tile(ex, ey) == 223, "W1",
               "new game: every seal closed; Deceit's entrance reads as the sealed tile 223");
        h.step(in);
        expect(h.g().position.xy.x == nx && h.g().position.xy.y == ny, "W1b", "the sealed entrance cannot be walked onto");

        h.set_mark(); h.yell("VILIS");
        expect(h.saw("A word of power is uttered") && h.saw("No effect!") && word_flags(h.g()) == 0, "W2",
               "a real Word at the WRONG dungeon is uttered (quake) but opens nothing");
        h.set_mark(); h.yell("FALLA");
        expect(!h.saw("A word of power is uttered") && h.saw("No effect!") && word_flags(h.g()) == 0, "W3",
               "a partial word is no Word at all");
        h.set_mark(); h.yell("fallax");
        if (!expect(h.saw("A word of power is uttered") && !h.saw("No effect!") && word_flags(h.g()) == 1, "W4",
                    "** Yell `fallax` beside Deceit: uttered, seal 33 (and only 33) opens -- case-insensitive, as the core's contains() **"))
            h.dump("W4");
        expect(h.tile(ex, ey) != 223, "W5", "the entrance is an entrance again");
        h.step(in);
        expect(h.g().position.xy.x == ex && h.g().position.xy.y == ey, "W6", "the party walks onto it");
        h.key('e');
        expect(h.d().active && h.d().pos.dungeon == 33, "W7", "** ordinary (E)nter takes the party into Deceit -- no Developer tool **");
        h.menu_save();
        const auto gam = tdeck::host_memory_save_gam_for_test();
        bool bits = gam.size() > 0x331 && (gam[0x32a] & 0x80);
        for (int i = 1; bits && i < 8; ++i) bits = !(gam[0x32a + i] & 0x80);
        expect(bits, "W7b", "** the save's GAM carries the opened seal as the original does: 0x32a bit 0x80, the other seven clear **");
    }
    {
        Harness h;                                                   // power cycle: fresh runtime, INIT.GAM seals
        expect(word_flags(h.g()) == 0, "W8", "precondition: a fresh runtime starts with every seal closed");
        h.alt_load();
        expect(quest_flag(h.g().quest, QuestFlag::Word33) && h.d().active && h.d().pos.dungeon == 33, "W9",
               "** the opened seal (GAM 0x32a bit 0x80) survives save + power cycle + load **");
    }
    {
        Harness h;
        int opened = 0;
        for (int i = 0; i < 8; ++i) {
            const int ex = g_owners->location_x[32 + i], ey = g_owners->location_y[32 + i];
            const int16_t floor = Harness::entrance_floor(33 + i);
            h.to_surface(ex, ey, floor);
            Direction in{}; int nx = 0, ny = 0;
            if (!free_neighbour(h, ex, ey, in, nx, ny)) { std::printf("         no free cell beside %s\n", kDungeons[i]); continue; }
            h.to_surface(nx, ny, floor);
            const int before = word_flags(h.g());
            h.yell(kWords[i]);
            if (word_flags(h.g()) == (before | (1 << i))) ++opened;
            else std::printf("         %s: flags %02x -> %02x\n", kDungeons[i], before, word_flags(h.g()));
        }
        expect(opened == 8 && word_flags(h.g()) == 0xff, "W10", "** all eight Words open all eight seals, each its own bit **");
        const int ex = g_owners->location_x[32], ey = g_owners->location_y[32];
        Direction in{}; int nx = 0, ny = 0; free_neighbour(h, ex, ey, in, nx, ny); h.to_surface(nx, ny);
        h.yell("FALLAX");
        expect(word_flags(h.g()) == 0xfe, "W11", "a second FALLAX toggles Deceit's seal shut again (quest.cpp's toggle, as the reference)");
    }
}

// ===========================================================================
// M. RB-3 / H-188 / H-191: moonstones, the night gate, transit, Vas Rel Por.
// ===========================================================================
const Moonstone *stones(Harness &h) { const auto *q = h.quest(); return q && q->moonstone_count == 8 ? q->moonstones : nullptr; }
int gate_tile_at(Harness &h, int x, int y) {
    const auto map = get_active_map(h.ctx().world, h.g().position.map);
    const auto s = compose_world_presentation(h.ctx(), map.value, h.g().position.xy, 0x11c);
    const int col = x - int(h.g().position.xy.x) + kPresentationWindow / 2, row = y - int(h.g().position.xy.y) + kPresentationWindow / 2;
    if (col < 0 || row < 0 || col >= kPresentationWindow || row >= kPresentationWindow) return -1;
    return s.tiles[row * kPresentationWindow + col];
}

void test_moongates() {
    std::printf("M  moongates and Vas Rel Por\n");
    Harness h;
    expect(stones(h) != nullptr, "M0", "the runtime owns the eight moonstones");
    // The expected stones are INIT.GAM's own bytes (0x28a x, 0x292 y, 0x29a
    // location, 0x2a2 floor), read from the pack -- not the device's copy --
    // so every check below still runs, and fails, on a device with no owner.
    Moonstone init[8];
    const uint8_t *gam = g_owners->initial_gam;
    for (int i = 0; i < 8; ++i) init[i] = {gam[0x28a + i], gam[0x292 + i], int16_t(gam[0x2a2 + i]), gam[0x29a + i], gam[0x29a + i] != 255};
    const Moonstone *m = init;
    // Stone 1 (96,102) and a walkable neighbour.
    const int gx = m[1].x, gy = m[1].y;
    Direction in{}; int nx = 0, ny = 0;
    expect(free_neighbour(h, gx, gy, in, nx, ny), "M1", "precondition: a walkable cell beside stone 1");
    h.to_surface(nx, ny);
    h.g().time.hour = 12;
    const int day_tile = gate_tile_at(h, gx, gy);
    expect(day_tile != 0xdc, "M2", "no gate by day: the cell shows its own terrain");
    h.g().time.hour = 21; h.g().time.minute = 0;
    expect(gate_tile_at(h, gx, gy) == 0xdc, "M3", "** at night the gate tile 0xDC is composed over the buried stone (H-191) **");
    expect(gate_tile_at(h, nx, ny) != 0xdc, "M3b", "the party's own cell is not a gate");

    // Walk onto the gate: transit to the active phase's stone.
    // The expected phase from the data, not from the core: INIT.GAM latches
    // no phase (0x2df/0x2e0 = 0), so it is the day's pair of DATA.OVL 0x1EEA
    // bytes minus '0', Felucca before noon and Trammel after (kernel 0x4962).
    const int day = h.g().time.day;
    const int fel = g_owners->moon_phases[(day - 1) * 2] - 48, tra = g_owners->moon_phases[(day - 1) * 2 + 1] - 48;
    const int phase = h.g().time.hour < 12 ? fel : tra;
    expect(phase >= 0 && phase < 8 && active_gate_phase(h.g(), h.rt->turn(), *h.quest()) == phase, "M3c",
           "the night's gate phase is Trammel's for this day (moon-phase table bound)");
    h.set_mark(); h.step(in);
    const auto &dest = m[phase >= 0 ? phase : 0];
    if (!expect(phase >= 0 && h.g().position.map.location == 0 && h.g().position.xy.x == dest.x && h.g().position.xy.y == dest.y, "M4",
                "** stepping onto the night gate transports the party to the active phase's stone **")) {
        std::printf("         phase=%d pos=(%d,%d) dest=(%d,%d)\n", phase, h.g().position.xy.x, h.g().position.xy.y, dest.x, dest.y);
        h.dump("M4");
    }
    // No stale gate: by day the destination cell composes its terrain again.
    h.g().time.hour = 12;
    expect(gate_tile_at(h, dest.x == 255 ? 0 : dest.x + 1, dest.y) != 0xdc && gate_tile_at(h, gx, gy) != 0xdc, "M5",
           "the gate leaves no stale tile once the night is over");

    // Vas Rel Por through the real (C)ast menu, 'To phase:' and a digit.
    h.g().time.hour = 12;
    h.to_surface(nx, ny);
    auto &q = h.g().spell_quantities;
    for (auto &v : q) v = 0;
    q[46] = 1;
    h.set_mark(); h.key('c');
    expect(h.mode() == UiMode::SpellSelection, "M6", "(C)ast opens the spell menu");
    h.key('\r');
    expect(h.mode() == UiMode::TargetSelection && h.rt->ui()->request() == UiRequestId::GatePhase, "M7",
           "Vas Rel Por asks for the phase");
    expect(std::strcmp(h.rt->status_overlay(), "To phase:") == 0, "M7b",
           "** the status line reads \"To phase:\" (D-53), not the aim reticle's \"Aim: empty\" **");
    h.key('3');
    if (!expect(h.g().position.map.location == m[2].location && h.g().position.xy.x == m[2].x && h.g().position.xy.y == m[2].y &&
                    !h.saw("Failed!"), "M8",
                "** Vas Rel Por + '3' lands on moonstone 2 (H-12/H-13) **")) {
        std::printf("         pos=(%d,%d) stone2=(%d,%d)\n", h.g().position.xy.x, h.g().position.xy.y, m[2].x, m[2].y);
        h.dump("M8");
    }
    expect(h.g().spell_quantities[46] == 0, "M9", "the mixture is spent");
}

// ===========================================================================
// S. RB-4 / H-146: buying a ship, a skiff and a horse.
// ===========================================================================
struct Keeper { int location, slot; };
const NpcActor *actor(Harness &h, int slot) {
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &a = h.rt->actors().actors[i];
        if (a.location == h.g().position.map.location && a.schedule.slot == slot) return &a;
    }
    return nullptr;
}
bool enter_town(Harness &h, int location) {
    h.to_surface(g_owners->location_x[location - 1], g_owners->location_y[location - 1]);
    h.key('e');
    return h.g().position.map.location == location;
}
int open_hour(const NpcLocationData &n, int slot) {
    for (size_t i = 0; i < n.count; ++i)
        if (n.slots[i].slot == slot)
            for (int hour = 8; hour < 20; ++hour) if (shop_is_open(n.slots[i].times, uint8_t(hour))) return hour;
    return -1;
}
// Stand beside the keeper and Talk to it; true = the shop session opened.
bool open_shop(Harness &h, int slot, bool stable_spot) {
    const auto *a = actor(h, slot);
    if (!a) return false;
    constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
    for (auto d : dirs) {
        const auto dd = direction_delta(d);
        const int x = a->x + dd.dx, y = a->y + dd.dy;
        const auto map = get_active_map(h.ctx().world, h.g().position.map);
        const int t = h.ctx().terrain->effective(h.ctx().world, h.g().position.map, x, y);
        (void)map;
        if (!is_passable(t, TransportMode::Foot).value || h.ctx().shop_services->occupied(h.ctx().shop_services->context, x, y)) continue;
        if (stable_spot) {
            bool spot = false;
            for (auto e : dirs) {
                const auto ee = direction_delta(e);
                const int sx = x + ee.dx, sy = y + ee.dy;
                const int st = h.ctx().terrain->effective(h.ctx().world, h.g().position.map, sx, sy);
                if ((st == 0x44 || st == 0x45 || st == 5) && !(sx == a->x && sy == a->y)) spot = true;
            }
            if (!spot) continue;
        }
        h.g().position.xy = {uint8_t(x), uint8_t(y)};
        const Direction toward = d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
                               : d == Direction::East ? Direction::West : Direction::East;
        h.key('t'); h.step(toward);
        return h.mode() == UiMode::Shop;
    }
    return false;
}
size_t ships(const std::vector<QuestObject> &pool) { size_t n = 0; for (const auto &o : pool) n += o.ship; return n; }
const QuestObject *last_ship(const std::vector<QuestObject> &pool) { const QuestObject *s = nullptr; for (const auto &o : pool) if (o.ship) s = &o; return s; }

void test_shops() {
    std::printf("S  shipwright and horse seller\n");
    constexpr int kEastBritanny = 21, kShipwright = 2, kPaws = 22, kHorseSeller = 4;
    const int ship_hour = open_hour(g_owners->npc_locations[kEastBritanny - 1], kShipwright);
    const int horse_hour = open_hour(g_owners->npc_locations[kPaws - 1], kHorseSeller);
    {   // Decline (N) is the control that already worked on the device.
        Harness h; h.g().time.hour = uint8_t(ship_hour); h.g().gold = 5000;
        expect(enter_town(h, kEastBritanny) && open_shop(h, kShipwright, false), "S0", "East Britanny's shipwright opens its shop");
        const auto &shop = h.rt->command_context().shop_services->session;
        h.set_mark(); h.key('y');                                        // "May I help thee?" -> the ship list
        h.key('a');                                                      // the frigate
        const bool deal = shop.phase == ShopPhase::ShipDeal;
        if (!deal) { std::printf("         phase=%d\n", int(shop.phase)); h.dump("S1"); }
        h.key('n');
        expect(deal && h.g().gold == 5000 && ships(h.pool()) == 0, "S1", "N declines the frigate: no gold, no ship (control)");
    }
    {   // Y buys the frigate.
        Harness h; h.g().time.hour = uint8_t(ship_hour); h.g().gold = 5000;
        enter_town(h, kEastBritanny); open_shop(h, kShipwright, false);
        h.set_mark(); h.key('y'); h.key('a');
        h.key('y');
        const auto *s = last_ship(h.pool());
        if (!expect(h.g().gold < 5000 && ships(h.pool()) == 1 && s && s->location == 0 && s->floor == 0 && s->tile == 0x125 &&
                        s->hull == 99 && s->skiffs == 2, "S2",
                    "** Y buys the frigate: gold spent, a frigate (0x125, hull 99, 2 skiffs) waits at the overworld dock **"))
            h.dump("S2");
        expect(h.saw("She awaits thee at the dock!"), "S3", "\"She awaits thee at the dock!\"");
        if (s) {                                                        // board it
            const QuestObject ship = *s;
            for (int i = 0; i < 4 && h.mode() == UiMode::Shop; ++i) h.key('n');   // "Anything else?" -> no
            h.to_surface(ship.x, ship.y);
            h.set_mark(); h.key('b');
            if (!expect(h.g().transport == TransportMode::Ship && h.rt->turn().transport_tile == 0x25 && ships(h.pool()) == 0 &&
                            h.g().ship_hull == 99 && h.g().ship_skiffs == 2, "S4",
                        "** (B)oard at the dock: the party sails the frigate it bought **")) {
                std::printf("         mode=%d transport=%d tile=0x%x ships=%zu hull=%d skiffs=%d at (%d,%d) ship (%d,%d) L%d F%d\n",
                            int(h.mode()), int(h.g().transport), unsigned(h.rt->turn().transport_tile), ships(h.pool()),
                            int(h.g().ship_hull), int(h.g().ship_skiffs), h.g().position.xy.x, h.g().position.xy.y, ship.x, ship.y,
                            ship.location, ship.floor);
                h.dump("S4");
            }
        }
    }
    {   // Y buys the skiff.
        Harness h; h.g().time.hour = uint8_t(ship_hour); h.g().gold = 5000;
        enter_town(h, kEastBritanny); open_shop(h, kShipwright, false);
        h.key('y'); h.key('b'); h.key('y');
        const auto *s = last_ship(h.pool());
        expect(h.g().gold < 5000 && s && s->tile == 0x129 && s->skiffs == 0 && s->location == 0, "S5",
               "** Y buys the skiff: 0x129 at the dock **");
    }
    {   // Too poor: nothing is placed.
        Harness h; h.g().time.hour = uint8_t(ship_hour); h.g().gold = 10;
        enter_town(h, kEastBritanny); open_shop(h, kShipwright, false);
        h.set_mark(); h.key('y'); h.key('b'); h.key('y');
        expect(h.g().gold == 10 && ships(h.pool()) == 0 && h.rt->command_context().shop_services->session.thrown_out, "S6",
               "without the gold no skiff appears (buy_ship's !ok: the shipwright throws the party out)");
    }
    {   // The horse.
        Harness h; h.g().time.hour = uint8_t(horse_hour); h.g().gold = 5000;
        const bool opened = enter_town(h, kPaws) && open_shop(h, kHorseSeller, true);
        expect(opened, "S7", "Paws' horse seller opens its shop");
        const auto before = h.ctx().terrain->persistent.size();
        h.set_mark(); h.key('y');                                        // the greeting -> the price
        const bool deal = h.rt->command_context().shop_services->session.phase == ShopPhase::HorseDeal;
        h.key('y');                                                      // buy
        expect(deal, "S7b", "the horse seller names the price and waits for Y/N");
        const TerrainCell *horse = nullptr;
        for (const auto &c : h.ctx().terrain->persistent) if (c.tile == 0x110 && c.map.location == kPaws) horse = &c;
        if (!expect(h.g().gold < 5000 && horse && h.ctx().terrain->persistent.size() == before + 1, "S8",
                    "** Y buys the horse: gold spent, a horse (0x110) stands beside the party **"))
            h.dump("S8");
        if (horse) {
            const TerrainCell cell = *horse;
            for (int i = 0; i < 4 && h.mode() == UiMode::Shop; ++i) h.key('n');
            h.g().position.xy = {uint8_t(cell.x), uint8_t(cell.y)};
            h.key('b');
            expect(h.g().transport == TransportMode::Horse, "S9", "** (B)oard mounts the horse it bought **");
        }
    }
    {   // Too poor for the horse.
        Harness h; h.g().time.hour = uint8_t(horse_hour); h.g().gold = 1;
        enter_town(h, kPaws); open_shop(h, kHorseSeller, true);
        const auto before = h.ctx().terrain->persistent.size();
        h.set_mark(); h.key('y'); h.key('y');
        expect(h.g().gold == 1 && h.ctx().terrain->persistent.size() == before && h.saw("afford to feed it"), "S10",
               "without the gold no horse appears (\"Thou couldst not afford to feed it!\")");
    }
}

// ===========================================================================
// E. RB-1 / H-189: the final Doom room, absorption and the victory ending.
// ===========================================================================
bool reach_final_room(Harness &h) {
    // The same route the Phase 7E checklist gives: Doom, level 7 (floor 6),
    // cell (4,7), facing East; one step onto the 0x61 pit drops the party
    // into floor 7 (5,7), room 15 = combat map 127.
    // Developer -> Shortcuts -> Preset: Endgame (the Phase 7E setup): the three
    // Shadowlords dead, so Doom's gate does not intercept, the box and the
    // artifacts. The test then keeps one member, so one walk ends the room.
    const bool box = h.g().wooden_box;
    apply_debug_preset(h.ctx(), DebugPreset::Endgame);
    h.g().wooden_box = box;
    h.g().party.character_count = h.g().party.party_size = 1;
    h.g().party.active_character = 0;
    set_quest_flag(h.g().quest, QuestFlag::GameWon, false);
    set_quest_flag(h.g().quest, QuestFlag::Word40);
    h.to_surface(g_owners->location_x[39], g_owners->location_y[39], Harness::entrance_floor(40));
    h.key('e');
    if (!h.d().active || h.d().pos.dungeon != 40) return false;
    auto &d = h.rt->dungeon_state_for_test();
    d.pos.floor = 6; d.pos.x = 4; d.pos.y = 7; d.pos.facing = DungeonFacing::East;
    h.north();
    if (!h.rt->command_context().combat) {
        std::printf("         dungeon: floor=%u (%u,%u) facing=%d; floor 6 row 7:", unsigned(d.pos.floor), unsigned(d.pos.x),
                    unsigned(d.pos.y), int(d.pos.facing));
        for (int x = 0; x < 8; ++x) std::printf(" %02x", d.cells[6 * 64 + 7 * 8 + x]);
        std::printf("\n");
        h.dump("final room");
    }
    return h.rt->command_context().combat;
}
// One combat input per enemy beat, on the virtual clock.
void combat_step(Harness &h, RawInputKind k) { Harness::advance(600000); h.ball(k); }
void pump(Harness &h, int n) { for (int i = 0; i < n; ++i) { Harness::advance(600000); h.ball(RawInputKind::TrackballUp); } }

void test_endgame(bool box) {
    std::printf("E%s the final room with%s the wooden box\n", box ? "V" : "S", box ? "" : "out");
    Harness h;
    h.g().wooden_box = box;
    for (auto &a : h.g().quest.artifacts) a = true;
    expect(!quest_flag(h.g().quest, QuestFlag::GameWon), box ? "EV0" : "ES0", "precondition: the game is not won");
    const bool in_room = reach_final_room(h);
    expect(in_room && h.mode() == UiMode::Combat, box ? "EV1" : "ES1", "Doom floor 6 pit -> room 15: the final arena is live");
    h.set_mark();
    for (int i = 0; i < 4 && h.rt->command_context().combat; ++i) combat_step(h, RawInputKind::TrackballUp);
    pump(h, 6);
    const bool absorbed = h.saw("is absorbed!");
    expect(absorbed, box ? "EV2" : "ES2", "the Avatar walks to (5,2) under the trapped soul and is absorbed");
    const bool won = quest_flag(h.g().quest, QuestFlag::GameWon);
    if (box) {
        if (!expect(!h.rt->command_context().combat && !h.rt->combat_state().initialized, "EV3",
                    "** the final arena tears down (no COMBAT_TEARDOWN deferred) **"))
            h.dump("EV3");
        expect(won, "EV4", "** game-won is set **");
        expect(h.count("\"FOLLOW!\" cries Lord British") == 1 && h.count("Lord British carefully opens the box...") == 1, "EV5",
               "** ENDMSG record 9 is read from the pack and shown exactly once **");
        expect(h.count("THE QUEST OF THE AVATAR IS FOREVER") == 1 && h.count("Report now, thy Quest compleat") == 1, "EV6",
               "the proclamation and the report appear once (no duplicated ending)");
        expect(h.mode() == UiMode::Dungeon && !h.rt->command_context().combat, "EV7",
               "input returns to the dungeon view, not the arena");
        const auto facing = h.d().pos.facing;
        h.east();                                                        // turn right in the dungeon
        expect(h.d().active && h.d().pos.facing != facing, "EV8", "** the next key is a normal dungeon command: no input wedge **");
        h.set_mark(); h.key(' '); h.key(' ');
        expect(h.count("THE QUEST OF THE AVATAR IS FOREVER") == 0 && quest_flag(h.g().quest, QuestFlag::GameWon), "EV9",
               "further input neither re-runs the ending nor clears game-won");
        h.set_mark(); h.alt_save();
        expect(h.saw("Save complete"), "EV10", "Alt+S still saves after the ending");
        h.raw_key('m', true);
        const bool menu = h.rt->system_menu_open();
        h.raw_key('m', true);
        expect(menu && !h.rt->system_menu_open() && h.mode() == UiMode::Dungeon, "EV11", "Alt+M opens and closes the System Menu over the ended game");
    } else {
        expect(!h.rt->command_context().combat && won && h.saw("pull up a chair") && !h.saw("FOLLOW!"), "ES3",
               "control: without the box the stranded ending runs (it never needed ENDMSG)");
    }
}

// ===========================================================================
// K. H-190 / H-137: the Refuge after a party wipe speaks KARMA.DAT.
// ===========================================================================
void test_refuge() {
    std::printf("K  Refuge after a party wipe, with the KARMA.DAT speech\n");
    Harness h(true);
    // Phase 7E-E's route: Developer -> Preset: Low health/status on a party
    // of one leaves the Avatar poisoned at 1 HP; passing turns lets the
    // poison kill it, and the next turn raises the Refuge.
    h.to_surface(90, 100);
    apply_debug_preset(h.ctx(), DebugPreset::LowHealthStatus);
    h.g().karma = 50;                                                 // karma/20 = 2
    expect(h.g().party.characters[0].status == 'P' && h.g().party.characters[0].current_hp == 1, "K0",
           "precondition: the preset leaves the lone Avatar poisoned at 1 HP");
    const auto karma = raw_records("karma-records.bin");
    h.set_mark();
    for (int i = 0; i < 40 && !h.saw("Thou hast found refuge."); ++i) { h.key(' '); h.run_ms(2000); }
    h.run_ms(20000);                                                  // let the whole Refuge scene play
    expect(h.saw("An unending darkness engulfs thee...") && h.saw("Thou hast found refuge."), "K1",
           "the poison kills the Avatar and the Refuge scene runs");
    const std::string speech = karma.size() == 6 ? "\"" + karma[2] + "\"" : std::string("?");
    if (!expect(h.saw(speech.c_str()) && !h.saw("Rest well, Avatar"), "K2",
                "** the apparition speaks KARMA.DAT record 2, quoted, not the invented line (H-190) **"))
        h.dump("K2");
    for (int i = 0; i < 6 && h.g().position.map.location != 17; ++i) { h.key(' '); h.run_ms(5000); }
    const auto &ch = h.g().party.characters[0];
    expect(h.g().position.map.location == 17 && h.g().position.map.floor == 1 && h.g().position.xy.x == 10 &&
               h.g().position.xy.y == 10 && ch.status == 'G' && ch.current_hp == ch.max_hp, "K3",
           "** the party wakes in Lord British's castle (17, floor 1, 10,10), healed **");
}

// ===========================================================================
// V. The Phase 7E routes, executed. Every Developer route the hardware
// checklist gives is run here through the same core call the Developer menu
// makes (apply_debug_teleport), and its coordinates are printed from the data,
// so the checklist never carries a guessed cell.
// ===========================================================================
bool dev_teleport(Harness &h, DebugDestinationKind kind, uint8_t location, int16_t floor, int x, int y) {
    DebugTeleportRequest r;
    r.kind = kind; r.location = location; r.floor = floor; r.x = x; r.y = y; r.standard_entry = false;
    return apply_debug_teleport(h.ctx(), r).status == DebugTeleportStatus::Applied;
}
const char *dir_name(Direction d) {
    return d == Direction::North ? "North" : d == Direction::South ? "South" : d == Direction::East ? "East" : "West";
}

void test_routes() {
    std::printf("V  Phase 7E routes (Developer teleport + the cells the checklist names)\n");
    {   // A: the final room, the checklist's way (Endgame preset, Party size 1, teleport, one step).
        Harness h;
        apply_debug_preset(h.ctx(), DebugPreset::Endgame);
        h.g().party.character_count = h.g().party.party_size = 1;
        h.g().party.active_character = 0;
        const bool at = dev_teleport(h, DebugDestinationKind::Dungeon, 40, 6, 4, 7);
        h.rt->dungeon_state_for_test().pos.facing = DungeonFacing::East;
        h.north();
        std::printf("         route 7E-A: Doom, Floor Level 6, X=4 Y=7, Use default entrance Off; face East, forward\n");
        expect(at && h.rt->command_context().combat && h.rt->combat_state().initialized, "V0",
               "7E-A: the Developer teleport + one step east drops the party into the final room");
        h.set_mark();
        for (int i = 0; i < 4 && h.rt->command_context().combat; ++i) combat_step(h, RawInputKind::TrackballUp);
        pump(h, 6);
        expect(quest_flag(h.g().quest, QuestFlag::GameWon) && h.saw("FOLLOW!") && !h.rt->command_context().combat, "V0b",
               "7E-A: four steps north, the Avatar is absorbed and the victory ending runs");
    }
    {   // B: Deceit's entrance.
        Harness h;
        const int ex = g_owners->location_x[32], ey = g_owners->location_y[32];
        Direction in{}; int nx = 0, ny = 0;
        const bool ok = free_neighbour(h, ex, ey, in, nx, ny);
        std::printf("         route 7E-B: Britannia X=%d Y=%d (beside Deceit's entrance %d,%d); step %s onto it after the Word\n",
                    nx, ny, ex, ey, dir_name(in));
        expect(ok && dev_teleport(h, DebugDestinationKind::Britannia, 0, 0, nx, ny) && h.g().position.xy.x == nx, "V1",
               "7E-B: the Developer teleport lands beside Deceit's sealed entrance");
    }
    {   // C: the Britain moongate (stone 1).
        Harness h;
        const uint8_t *gam = g_owners->initial_gam;
        const int gx = gam[0x28a + 1], gy = gam[0x292 + 1];
        Direction in{}; int nx = 0, ny = 0;
        const bool ok = free_neighbour(h, gx, gy, in, nx, ny);
        std::printf("         route 7E-C: Britannia X=%d Y=%d, Time Hour=21; the gate is at %d,%d (%s of the party)\n",
                    nx, ny, gx, gy, dir_name(in));
        for (int i = 0; i < 8; ++i)
            std::printf("           stone %d (Vas Rel Por '%d'): %d,%d\n", i, i + 1, gam[0x28a + i], gam[0x292 + i]);
        expect(ok && dev_teleport(h, DebugDestinationKind::Britannia, 0, 0, nx, ny), "V2", "7E-C: the teleport lands beside the Britain gate");
    }
    {   // D: the two keepers.
        constexpr int kEastBritanny = 21, kShipwright = 2, kPaws = 22, kHorseSeller = 4;
        for (int k = 0; k < 2; ++k) {
            Harness h;
            const int loc = k ? kPaws : kEastBritanny, slot = k ? kHorseSeller : kShipwright;
            const int hour = open_hour(g_owners->npc_locations[loc - 1], slot);
            h.g().time.hour = uint8_t(hour);
            h.g().gold = 5000;
            const bool opened = enter_town(h, loc) && open_shop(h, slot, k == 1);
            const auto *a = actor(h, slot);
            std::printf("         route 7E-D: %s, Time Hour=%d; keeper at %d,%d; stand at %d,%d and Talk toward it\n",
                        k ? "Paws horse seller" : "East Britanny shipwright", hour, a ? a->x : -1, a ? a->y : -1,
                        h.g().position.xy.x, h.g().position.xy.y);
            expect(opened, k ? "V4" : "V3", k ? "7E-D: Paws' horse seller trades at that hour" : "7E-D: East Britanny's shipwright trades at that hour");
        }
        std::printf("         route 7E-D: a ship bought in East Britanny waits at the overworld dock 79,109\n");
    }
    {   // H: Gorn's brazier (Palace of Blackthorn basement, R-33).
        Harness h;
        h.g().keys = 0;
        constexpr int bx = 8, by = 6;
        const bool at = dev_teleport(h, DebugDestinationKind::SmallMap, 18, -1, bx, by + 1);
        std::printf("         route 7E-H: Palace of Blackthorn, Floor Basement, X=%d Y=%d; (S)earch North at the brazier %d,%d\n",
                    bx, by + 1, bx, by);
        h.set_mark(); h.key('s'); h.north();
        const bool found = h.saw("keys");
        const int after_search = h.g().keys;
        h.key('g'); h.north();
        std::printf("         route 7E-H: keys after (S)earch=%d, after (G)et=%d\n", after_search, int(h.g().keys));
        expect(at && h.g().position.map.location == 18 && h.g().position.map.floor == -1, "V5", "7E-H: the teleport lands in the jail");
        if (!expect(found && h.g().keys == 9, "V6", "7E-H: (S)earch North finds Gorn's keys at the brazier; (G)et North takes all 9"))
            h.dump("V6");
    }
    {   // F: Destard room 9 (Slime/Gargoyle board, H-151).
        Harness h;
        const bool at = dev_teleport(h, DebugDestinationKind::Dungeon, 35, 6, 3, 2);
        auto &d = h.rt->dungeon_state_for_test();
        d.pos.facing = DungeonFacing::North;
        h.north();
        bool divider = false;
        const auto &cs = h.rt->combat_state();
        for (int i = 0; i < cs.count; ++i)
            if (cs.actors[i].enemy && (cs.actors[i].enemy->index == 24 || cs.actors[i].enemy->index == 30)) divider = true;
        std::printf("         route 7E-F: Destard, Floor Level 6, X=3 Y=2, face North, forward into room 9\n");
        expect(at && h.rt->command_context().combat && divider, "V7",
               "7E-F: that step opens Destard room 9 with a Slime/Gargoyle on the board");
        int acted = 0;
        for (int i = 0; i < 6 && h.rt->command_context().combat; ++i) {
            const auto before = cs.current; combat_step(h, RawInputKind::TrackballDown); acted += cs.current != before;
        }
        expect(acted > 0, "V8", "7E-F: the arena takes turns (no H-151 freeze)");
    }
}

// ===========================================================================
// R. Moonstone persistence across every load route.
// ===========================================================================
bool same_stones(const Moonstone *a, const Moonstone *b) {
    for (int i = 0; i < 8; ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z || a[i].location != b[i].location || a[i].buried != b[i].buried)
            return false;
    return true;
}
void describe(const Moonstone *m, const char *tag) {
    std::printf("         %s:", tag);
    for (int i = 0; i < 8; ++i) std::printf(" [%d:%d,%d L%d z%d %s]", i, m[i].x, m[i].y, m[i].location, m[i].z, m[i].buried ? "b" : "c");
    std::printf("\n");
}

Moonstone h_stones_for_gam[8]{};

void test_moonstone_persistence() {
    std::printf("R  moonstone state survives save / load\n");
    tdeck::host_memory_save_forget_for_test();
    Moonstone saved[8]{};
    {
        Harness h;
        auto *q = h.ctx().quest_world;
        if (!expect(q && q->moonstones && q->moonstone_count == 8, "R0", "precondition: the runtime owns the stones")) return;
        // Dig stone 4 up with the real (S)earch, then bury stone 6 somewhere new.
        const auto s4 = q->moonstones[4];
        h.to_surface(s4.x, s4.y + 1);
        h.key('s'); h.north();                                        // (S)earch finds it ...
        h.key('g'); h.north();                                        // ... (G)et takes it
        expect(!q->moonstones[4].buried && q->moonstones[4].location == 255, "R1", "(S)earch + (G)et on stone 4's cell digs it up");
        q->moonstones[6] = {10, 20, 0, 0, true};
        std::memcpy(saved, q->moonstones, sizeof(saved));
        h.menu_save();
        q->moonstones[1] = {1, 2, 0, 0, false};                       // clobber after the save
        h.menu_load();
        if (!expect(same_stones(q->moonstones, saved), "R2", "** System Menu Continue Latest restores the saved stones **")) {
            describe(saved, "saved"); describe(q->moonstones, "loaded");
        }
        q->moonstones[3] = {7, 7, 0, 255, false};
        h.alt_load();
        expect(same_stones(q->moonstones, saved), "R3", "** Alt+L restores them **");
    }
    {
        Harness h;                                                     // power cycle
        auto *q = h.ctx().quest_world;
        expect(q && q->moonstones && !same_stones(q->moonstones, saved), "R4", "precondition: a fresh runtime has INIT.GAM's stones");
        h.alt_load();
        expect(q && q->moonstones && same_stones(q->moonstones, saved), "R5", "** after a power cycle the stones come back from storage **");
    }
    {
        Harness h;                                                     // title -> Continue
        auto *q = h.ctx().quest_world;
        // Return to Title -> Journey Onward -> Continue (batch28's route).
        h.raw_key('m', true); h.ball(RawInputKind::TrackballUp); h.key('\r');
        h.key('x');
        h.key('j'); h.key('\r');
        expect(q && q->moonstones && same_stones(q->moonstones, saved), "R6", "** title Continue restores them too **");
    }
    {
        // A newer generation whose GAM stones are fine but whose sidecar is torn:
        // the gate falls back to the older generation, stones included.
        Harness h;
        auto *q = h.ctx().quest_world;
        h.alt_load();
        q->moonstones[0] = {50, 60, 0, 0, true};
        Moonstone newer[8]; std::memcpy(newer, q->moonstones, sizeof(newer));
        h.alt_save();
        tdeck::host_memory_save_damage_for_test();
        q->moonstones[0] = {1, 1, 0, 0, true};
        h.alt_load();
        expect(same_stones(q->moonstones, saved) && !same_stones(q->moonstones, newer), "R7",
               "** a corrupt newest generation falls back to the older one, with the older one's stones **");
    }
    {
        // Every save context: the stones are GAM bytes, so the context the
        // party saves in must not matter.
        struct Ctx { const char *id, *what; };
        const Ctx contexts[] = {{"R9", "town (Lord British's castle)"}, {"R10", "dungeon (Deceit)"}, {"R11", "aboard a ship at sea"}};
        for (int k = 0; k < 3; ++k) {
            tdeck::host_memory_save_forget_for_test();
            Harness h;
            auto *q = h.ctx().quest_world;
            if (k == 0) { h.to_surface(g_owners->location_x[16], g_owners->location_y[16]); h.key('e'); }
            if (k == 1) { set_quest_flag(h.g().quest, QuestFlag::Word33); h.to_surface(g_owners->location_x[32], g_owners->location_y[32]); h.key('e'); }
            if (k == 2) { h.to_surface(80, 110); h.g().transport = TransportMode::Ship; h.rt->turn().transport_tile = 0x24; h.g().ship_hull = 70; }
            const bool where = k == 0 ? h.g().position.map.location == 17 : k == 1 ? h.d().active : h.g().transport == TransportMode::Ship;
            q->moonstones[2] = {uint8_t(30 + k), 40, 0, 0, true};
            q->moonstones[5] = {0, 0, 0, 255, false};
            Moonstone want[8]; std::memcpy(want, q->moonstones, sizeof(want));
            std::memcpy(h_stones_for_gam, want, sizeof(want));
            h.alt_save();
            q->moonstones[2] = q->moonstones[5] = Moonstone{};
            h.alt_load();
            expect(where && same_stones(q->moonstones, want), contexts[k].id,
                   (std::string("save + Alt+L in ") + contexts[k].what + " keeps the stones").c_str());
        }
        const auto gam = tdeck::host_memory_save_gam_for_test();
        bool bytes = gam.size() > 0x2a9;
        for (int i = 0; bytes && i < 8; ++i) {
            const auto &m = h_stones_for_gam[i];
            bytes = gam[0x28a + i] == m.x && gam[0x292 + i] == m.y && gam[0x29a + i] == (m.buried ? m.location : 255) &&
                    gam[0x2a2 + i] == uint8_t(m.z);
        }
        expect(bytes, "R12b", "** and those GAM bytes are exactly the runtime's stones (0x28a x, 0x292 y, 0x29a location, 0x2a2 floor) **");
        static bool sidecar_has_stones = true;
        expect(tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) { sidecar_has_stones = gs.has("moonstones"); }) &&
                   !sidecar_has_stones, "R12",
               "** no new save field: the sidecar has no \"moonstones\" key -- they travel in GAM 0x28a..0x2a9 **");
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch53_release_blockers_test <openu5-alpha1-resources.bin> [stale-pack.bin]\n");
        return 2;
    }
    g_pack_path = argv[1];
    if (argc > 2) g_stale_pack_path = argv[2];
    openu5_host_virtual_clock_us() = 1'000'000;
    {
        std::ifstream in(g_pack_path, std::ios::binary);
        g_pack_bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    tdeck::AlphaResourcePack pack;
    if (pack.open(argv[1], g_report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, g_report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    for (size_t i = 0; i < owners.combat_map_count; ++i) g_arenas.push_back({owners.combat_map_views[i], owners.combat_sprites + i * 16});

    test_pack();
    test_binders();
    test_words();
    test_moongates();
    test_shops();
    test_endgame(true);
    test_endgame(false);
    test_refuge();
    test_routes();
    test_moonstone_persistence();

    std::printf("\nbatch53_release_blockers: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
