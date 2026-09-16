#include "openu5/persistence.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace openu5::save {
namespace {
using J = Json;
struct Field {
    const char *name;
    size_t offset;
    size_t count;
};
constexpr Field chars[] = {
    {"gender", 9, 1},     {"class", 10, 0},        {"status", 11, 0},     {"strength", 12, 1},
    {"dexterity", 13, 1}, {"intelligence", 14, 1}, {"currentMp", 15, 1},  {"currentHp", 16, 2},
    {"maxHp", 18, 2},     {"exp", 20, 2},          {"level", 22, 1},      {"monthsAtInn", 23, 1},
    {"helmet", 25, 1},    {"armor", 26, 1},        {"weapon", 27, 1},     {"shield", 28, 1},
    {"ring", 29, 1},      {"amulet", 30, 1},       {"partyStatus", 31, 1}};
constexpr Field scalars[] = {{"food", 0x202, 2},
                             {"gold", 0x204, 2},
                             {"keys", 0x206, 1},
                             {"gems", 0x207, 1},
                             {"torches", 0x208, 1},
                             {"magicCarpets", 0x20a, 1},
                             {"skullKeys", 0x20b, 1},
                             {"partySize", 0x2b5, 1},
                             {"activeCharacter", 0x2d5, 1},
                             {"karma", 0x2e2, 1},
                             {"turnsSinceStart", 0x2e5, 1},
                             {"torchTurns", 0x301, 1}};
constexpr Field times[] = {
    {"year", 0x2ce, 2}, {"month", 0x2d7, 1}, {"day", 0x2d8, 1}, {"hour", 0x2d9, 1}, {"minute", 0x2db, 1}};
constexpr Field positions[] = {{"location", 0x2ed, 1}, {"floor", 0x2ef, 1}, {"x", 0x2f0, 1}, {"y", 0x2f1, 1}};
constexpr Field arrays[] = {{"equipmentQuantities", 0x21a, 48},
                            {"spellQuantities", 0x24a, 48},
                            {"scrollQuantities", 0x27a, 8},
                            {"potionQuantities", 0x282, 8},
                            {"reagentQuantities", 0x2aa, 8}};
constexpr Field optional_bytes[] = {{"transportTile", 0x2d6, 1},     {"prevHour", 0x2da, 1},
                                    {"feluccaPhase", 0x2df, 1},      {"trammelPhase", 0x2e0, 1},
                                    {"shrineQuestBitmap", 0x326, 1}, {"shrineVisitedBitmap", 0x328, 1}};
constexpr Field optional_arrays[] = {
    {"shrineDestroyed", 0x332, 8}, {"dungeonRoomsCleared", 0x33a, 14}, {"shadowlordLocs", 0x322, 3}};
constexpr const char *extras[] = {
    "openDoors",      "mapOverrides",       "skullTreeFoundDay", "reagentPatchFoundDay", "overworldEnemies",
    "worldObjects",   "lightSpellMins",     "timeSpell",         "timeSpellTurns",       "wind",
    "sailDir",        "windDriftCtr",       "shipHull",          "shipSkiffs",           "hmsCapeToggle",
    "shadowlordLocs", "shadowlordSummoned", "shadowlordDoomBits"};
int n(const J &j) { return int(j.integer()); }
bool eq(const J &j, const char *s) { return j.kind == J::String && j.string == J(s).string; }
J arr(const uint8_t *b, size_t count) {
    J a = J::array();
    for (size_t i = 0; i < count; ++i)
        a.values.emplace_back(int(b[i]));
    return a;
}
int read(const uint8_t *b, const Field &f) {
    return b[f.offset] + (f.count == 2 ? b[f.offset + 1] * 256 : 0);
}
void put(uint8_t *b, size_t at, int64_t v) { b[at] = uint8_t(v); }
void fields_in(J &j, const uint8_t *b, const Field *f, size_t count) {
    for (size_t i = 0; i < count; ++i)
        j[f[i].name] = J(read(b, f[i]));
}
void fields_out(const J &j, uint8_t *b, const Field *f, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int64_t v = j[f[i].name].integer();
        put(b, f[i].offset, v);
        if (f[i].count == 2)
            put(b, f[i].offset + 1, v >> 8);
    }
}
template <size_t N> void fin(J &j, const uint8_t *b, const Field (&f)[N]) { fields_in(j, b, f, N); }
template <size_t N> void fout(const J &j, uint8_t *b, const Field (&f)[N]) { fields_out(j, b, f, N); }
void flags_in(J &s, const uint8_t *b, const char *group, size_t off,
              std::initializer_list<const char *> names) {
    for (auto k : names)
        s[group][k] = J(b[off++] != 0);
}
void flags_out(const J &s, uint8_t *b, const char *group, size_t off,
               std::initializer_list<const char *> names) {
    for (auto k : names) {
        bool v = s[group][k].truth();
        if ((b[off] != 0) != v)
            b[off] = v ? 255 : 0;
        ++off;
    }
}
bool ship(int b) { return (b & 0xf8) == 0x20 || (b & 0xfc) == 0x28; }
bool horse(int b) { return (b & 0xfe) == 0x10; }
int enemy(int b) {
    int tile = b + 256;
    if (tile == 300)
        return 8;
    int d = (tile - 320) / 4;
    return tile >= 320 && (tile - 320) % 4 == 0 && d >= 0 && d < 48 ? d : -1;
}
int free_slot(uint32_t mask, int end) {
    if (end == 31) {
        for (int i = 31; i >= 1; --i)
            if (!(mask & (uint32_t(1) << i)))
                return i;
    } else {
        for (int i = 1; i <= end; ++i)
            if (!(mask & (uint32_t(1) << i)))
                return i;
    }
    return 0;
}
void enemy_table(uint8_t *b, const J &enemies, int floor, uint32_t reserved) {
    for (int i = 1; i <= 23; ++i)
        if (!(reserved & (uint32_t(1) << i)))
            std::fill(b + i * 8, b + i * 8 + 8, 0);
    uint32_t used = reserved;
    for (const auto &e : enemies.values)
        if (e.has("slot") && n(e["slot"]) >= 0 && n(e["slot"]) < 32)
            used |= uint32_t(1) << n(e["slot"]);
    for (const auto &e : enemies.values) {
        int slot = e.has("slot") ? n(e["slot"]) : free_slot(used, 23);
        if (slot < 1 || slot > 23 || (reserved & (uint32_t(1) << slot)))
            continue;
        used |= uint32_t(1) << slot;
        auto *o = b + slot * 8;
        o[0] = o[1] = uint8_t(n(e["tile"]) - 256);
        o[2] = uint8_t(n(e["x"]));
        o[3] = uint8_t(n(e["y"]));
        o[4] = uint8_t(floor);
        o[5] = n(e["defIndex"]) == 8 ? uint8_t(n(e["hull"])) : 0;
        o[6] = 0;
        o[7] = n(e["defIndex"]) == 8 ? uint8_t(n(e["windCtr"])) : 0;
    }
}
void object_table(uint8_t *b, const J &s) {
    uint32_t used = 0;
    auto candidate = [](const J &o) {
        return n(o["location"]) == 0 && (eq(o["kind"], "ship") || eq(o["kind"], "horse"));
    };
    const auto &objects = s["worldObjects"].values;
    for (const auto &o : objects)
        if (candidate(o) && n(o["slot"]) >= 1 && n(o["slot"]) < 32)
            used |= uint32_t(1) << n(o["slot"]);
    for (const auto &o : objects) {
        if (!candidate(o))
            continue;
        int slot = n(o["slot"]);
        if (slot < 1 || slot >= 32)
            slot = free_slot(used, 31);
        if (!slot)
            continue;
        used |= uint32_t(1) << slot;
        auto *p = b + slot * 8;
        p[0] = p[1] = uint8_t(n(o["tile"]) - 256);
        p[2] = uint8_t(n(o["x"]));
        p[3] = uint8_t(n(o["y"]));
        p[4] = uint8_t(n(s["position"]["floor"]));
        p[5] = eq(o["kind"], "ship") ? uint8_t(n(o["hull"])) : 0;
        p[6] = 0;
        p[7] = eq(o["kind"], "ship") ? uint8_t(n(o["skiffs"])) : 0;
    }
    for (int i = 1; i <= 23; ++i) {
        int v = b[i * 8];
        if (v && enemy(v) < 0 && !ship(v) && !horse(v))
            used |= uint32_t(1) << i;
    }
    enemy_table(b, s["overworldEnemies"], n(s["position"]["floor"]), used);
}
void read_objects(const uint8_t *b, J &s) {
    J enemies = J::array(), objects = J::array();
    for (int i = 1; i < 32; ++i) {
        auto *o = b + i * 8;
        int def = enemy(o[0]);
        if (o[0] && i <= 23 && def >= 0) {
            J e = J::object();
            e["slot"] = J(i);
            e["defIndex"] = J(def);
            e["tile"] = J(o[0] + 256);
            e["water"] = J(def == 8 || (def >= 16 && def <= 19) || def == 43);
            e["x"] = J(int(o[2]));
            e["y"] = J(int(o[3]));
            if (def == 8) {
                e["hull"] = J(int(o[5]));
                if (o[7])
                    e["windCtr"] = J(int(o[7]));
            }
            enemies.values.push_back(e);
        }
        if (ship(o[0]) || horse(o[0])) {
            J v = J::object();
            v["location"] = J(0);
            v["floor"] = J(int(o[4]));
            v["x"] = J(int(o[2]));
            v["y"] = J(int(o[3]));
            v["slot"] = J(i);
            v["tile"] = J(o[0] + 256);
            v["kind"] = J(ship(o[0]) ? "ship" : "horse");
            if (ship(o[0])) {
                v["hull"] = J(int(o[5]));
                v["skiffs"] = J(int(o[7]));
            }
            objects.values.push_back(v);
        }
    }
    s["overworldEnemies"] = std::move(enemies);
    s["worldObjects"] = std::move(objects);
}
Error parse(const std::string &s, J &j) {
    auto e = parse_json(s, j);
    return e == JsonError::None       ? Error::None
           : e == JsonError::Capacity ? Error::Capacity
                                      : Error::JsonSyntax;
}
} // namespace
Json empty_sidecar() {
    J s = J::object();
    s["version"] = J(1);
    s["qol"]["journal"] = J::array();
    s["gameState"]["transport"] = J("foot");
    s["gameState"]["questFlags"] = J::object();
    return s;
}
bool sane_sidecar(const J &s) {
    return (s.kind == J::Object || s.kind == J::Array) &&
           (s["qol"].kind == J::Object || s["qol"].kind == J::Array) &&
           s["qol"]["journal"].kind == J::Array &&
           (s["gameState"].kind == J::Object || s["gameState"].kind == J::Array);
}
Envelope read_envelope(const uint8_t *b, size_t len) {
    constexpr char marker[] = "U5PARTIDA1\n";
    constexpr size_t marker_size = sizeof(marker) - 1;
    Envelope e;
    if (len < kGamSize + marker_size || std::memcmp(b + kGamSize, marker, marker_size))
        return e;
    e.kind = EnvelopeKind::Bad;
    size_t start = kGamSize + marker_size;
    if (len - start > kMaxJsonBytes + 3)
        return e;
    if (len >= start + 3 && b[start] == 0xef && b[start + 1] == 0xbb && b[start + 2] == 0xbf)
        start += 3; // TextDecoder strips a leading UTF-8 BOM.
    if (parse(std::string(reinterpret_cast<const char *>(b + start), len - start), e.document) != Error::None)
        return e;
    if (eq(e.document["formato"], "openu5-partida") && e.document["version"].kind == J::Number &&
        e.document["version"].number == 1 && sane_sidecar(e.document["sidecar"]))
        e.kind = EnvelopeKind::Ok;
    return e;
}
Error write_envelope(const Gam &g, const J &meta, const J &s, std::vector<uint8_t> &out) {
    J j = J::object();
    j["formato"] = J("openu5-partida");
    j["version"] = J(1);
    j["meta"] = meta;
    j["sidecar"] = s;
    std::string json;
    if (encode_json(j, json) != JsonError::None)
        return Error::Capacity;
    auto tail = std::string("U5PARTIDA1\n") + json;
    std::vector<uint8_t> result(g.begin(), g.end());
    result.insert(result.end(), tail.begin(), tail.end());
    out = std::move(result);
    return Error::None;
}
Error import_native(const uint8_t *b, size_t len, const J &side, J &out, bool gate) {
    if (len < kGamSize)
        return Error::ShortGam;
    // Explicit sidecars bypass isSaneSidecar in TS, but null/missing sections throw.
    if (side["gameState"].kind == J::Null || side["qol"].kind == J::Null)
        return Error::Schema;
    J s = J::object();
    s["version"] = J(1);
    s["characters"] = J::array();
    for (size_t i = 0; i < 16; ++i) {
        auto *r = b + 2 + i * 32;
        J c = J::object(), name;
        name.kind = J::String;
        for (size_t k = 0; k < 9 && r[k]; ++k)
            name.string += char16_t(r[k]);
        c["name"] = name;
        fin(c, r, chars);
        for (auto k : {"class", "status"}) {
            auto v = n(c[k]);
            c[k] = J("");
            c[k].string += char16_t(v);
        }
        s["characters"].values.push_back(c);
    }
    fin(s, b, scalars);
    fin(s["time"], b, times);
    fin(s["position"], b, positions);
    if (n(s["position"]["location"]) && b[0x2ef] == 255)
        s["position"]["floor"] = J(-1);
    s["grapple"] = J(b[0x209] != 0);
    flags_in(s, b, "lbArtifacts", 0x20d, {"amulet", "crown", "sceptre"});
    flags_in(s, b, "shards", 0x210, {"falsehood", "hatred", "cowardice"});
    flags_in(s, b, "specialItems", 0x214,
             {"spyglass", "hmsCape", "sextant", "pocketWatch", "blackBadge", "woodenBox"});
    for (auto f : arrays)
        s[f.name] = arr(b + f.offset, f.count);
    J moons = J::array();
    for (size_t i = 0; i < 8; ++i) {
        J m = J::object();
        m["x"] = J(int(b[0x28a + i]));
        m["y"] = J(int(b[0x292 + i]));
        m["buried"] = J(b[0x29a + i] != 255);
        m["location"] = J(int(b[0x29a + i]));
        m["z"] = J(int(b[0x2a2 + i]));
        moons.values.push_back(m);
    }
    s["moonstones"] = moons;
    for (auto k : {"npcDead", "npcMet"}) {
        size_t base = std::strcmp(k, "npcDead") == 0 ? 0x5b4 : 0x634;
        J grid = J::array();
        for (size_t row = 0; row < 32; ++row) {
            J a = J::array();
            for (size_t col = 0; col < 32; ++col) {
                size_t bit = row * 32 + col;
                a.values.emplace_back((b[base + bit / 8] & (1 << (7 - bit % 8))) != 0);
            }
            grid.values.push_back(a);
        }
        s[k] = grid;
    }
    for (auto f : optional_bytes)
        if (f.offset != 0x2df && f.offset != 0x2e0)
            s[f.name] = J(int(b[f.offset]));
    if (b[0x2df] >= 0x30 && b[0x2df] <= 0x37 && b[0x2e0] >= 0x30 && b[0x2e0] <= 0x37) {
        s["feluccaPhase"] = J(int(b[0x2df]));
        s["trammelPhase"] = J(int(b[0x2e0]));
    }
    for (auto f : optional_arrays)
        s[f.name] = arr(b + f.offset, f.count);
    if (!b[0x322] && !b[0x323] && !b[0x324])
        s["shadowlordLocs"] = J::array();
    if (b[0x325] != 255)
        s["shadowlordSummoned"] = J(int(b[0x325]));
    if (b[0x624] & 14)
        s["shadowlordDoomBits"] = J(b[0x624] & 14);
    if (!n(s["position"]["location"])) {
        read_objects(b + 0x6b4, s);
        s["shipHull"] = J(int(b[0x6b9]));
        s["shipSkiffs"] = J(int(b[0x6bb]));
    }
    const J &gs = side["gameState"];
    for (auto k : {"transport", "questFlags"})
        if (gs.has(k))
            s[k] = gs[k];
    for (auto k : extras)
        if (gs.has(k))
            s[k] = gs[k];
    if (gate)
        s["npcWalk"] = gs["npcWalk"];
    for (int i = 0; i < 8; ++i)
        if (b[0x32a + i] >= 128) {
            if (s["questFlags"].kind != J::Object && s["questFlags"].kind != J::Array)
                return Error::Schema;
            if (s["questFlags"].kind == J::Object)
                s["questFlags"][("word-spoken:" + std::to_string(33 + i)).c_str()] = J(true);
        }
    const char *dead[] = {"shadowlord-dead:falsehood", "shadowlord-dead:hatred", "shadowlord-dead:cowardice"};
    for (size_t i = 0; i < 3; ++i)
        if (n(s["shadowlordLocs"].at(i)) >= 128) {
            if (s["questFlags"].kind != J::Object && s["questFlags"].kind != J::Array)
                return Error::Schema;
            if (s["questFlags"].kind == J::Object)
                s["questFlags"][dead[i]] = J(true);
        }
    for (auto k : {"journal", "explored", "treasuryLoot"})
        if (side["qol"].has(k))
            s[k] = side["qol"][k];
    out = std::move(s);
    return Error::None;
}
Error export_native(const J &s, const uint8_t *base, size_t length, Gam &out, J &side, bool gate) {
    if (length < kGamSize)
        return Error::ShortTemplate;
    for (auto key : {"characters", "equipmentQuantities", "spellQuantities", "scrollQuantities",
                     "potionQuantities", "reagentQuantities", "moonstones", "npcDead", "npcMet"})
        if (s[key].kind != J::Array)
            return Error::Schema;
    for (auto key : {"specialItems", "lbArtifacts", "shards", "questFlags", "time", "position"})
        if (s[key].kind != J::Object)
            return Error::Schema;
    if (s["moonstones"].values.size() < 8)
        return Error::Schema;
    std::copy(base, base + kGamSize, out.begin());
    auto *b = out.data();
    for (size_t i = 0; i < 16 && i < s["characters"].values.size(); ++i) {
        const auto &c = s["characters"].at(i);
        if (!c.truth())
            continue;
        auto *r = b + 2 + i * 32;
        size_t count = std::min(size_t(9), c["name"].string.size());
        for (size_t k = 0; k < count; ++k)
            r[k] = uint8_t(c["name"].string[k]);
        if (count < 9)
            r[count] = 0;
        for (auto f : chars) {
            auto v = c[f.name].integer();
            if (f.count == 0)
                v = c[f.name].string.empty() ? 0 : c[f.name].string[0];
            put(r, f.offset, v);
            if (f.count == 2)
                put(r, f.offset + 1, v >> 8);
        }
    }
    fout(s, b, scalars);
    put(b, 0x2e5, std::min(int64_t(255), s["turnsSinceStart"].integer()));
    fout(s["time"], b, times);
    fout(s["position"], b, positions);
    if ((b[0x209] != 0) != s["grapple"].truth())
        b[0x209] = s["grapple"].truth() ? 1 : 0;
    flags_out(s, b, "lbArtifacts", 0x20d, {"amulet", "crown", "sceptre"});
    flags_out(s, b, "shards", 0x210, {"falsehood", "hatred", "cowardice"});
    flags_out(s, b, "specialItems", 0x214,
              {"spyglass", "hmsCape", "sextant", "pocketWatch", "blackBadge", "woodenBox"});
    for (auto f : arrays)
        for (size_t i = 0; i < f.count; ++i)
            put(b, f.offset + i, s[f.name].at(i).integer());
    for (size_t i = 0; i < 8; ++i) {
        const auto &m = s["moonstones"].at(i);
        put(b, 0x28a + i, m["x"].integer());
        put(b, 0x292 + i, m["y"].integer());
        put(b, 0x29a + i, m["buried"].truth() ? m["location"].integer() : 255);
        put(b, 0x2a2 + i, m["z"].integer());
    }
    b[0x6b6] = b[0x2f0];
    b[0x6b7] = b[0x2f1];
    if (s.has("transportTile")) {
        int t = n(s["transportTile"]);
        b[0x6b4] = b[0x6b5] = uint8_t(t);
        b[0x6b8] = b[0x2ef];
        bool keep = (t & 0xf8) == 0x20 || !n(s["position"]["location"]);
        b[0x6b9] = keep ? uint8_t(n(s["shipHull"])) : 0;
        b[0x6bb] = keep ? uint8_t(n(s["shipSkiffs"])) : 0;
    }
    if (!n(s["position"]["location"]))
        object_table(b + 0x6b4, s);
    for (auto f : optional_bytes)
        if (s.has(f.name))
            put(b, f.offset, s[f.name].integer());
    for (auto f : optional_arrays)
        if (s.has(f.name))
            for (size_t i = 0; i < f.count; ++i)
                put(b, f.offset + i, s[f.name].at(i).integer());
    b[0x325] = s.has("shadowlordSummoned") ? uint8_t(n(s["shadowlordSummoned"])) : 255;
    for (int i = 0; i < 8; ++i) {
        const auto &v = s["questFlags"][("word-spoken:" + std::to_string(33 + i)).c_str()];
        b[0x32a + i] = uint8_t((b[0x32a + i] & 127) | (v.kind == J::Bool && v.truth() ? 128 : 0));
    }
    for (auto k : {"npcDead", "npcMet"}) {
        size_t off = std::strcmp(k, "npcDead") == 0 ? 0x5b4 : 0x634;
        std::fill(b + off, b + off + 128, 0);
        for (size_t i = 0; i < 32; ++i)
            for (size_t j = 0; j < 32; ++j)
                if (s[k].at(i).at(j).truth())
                    b[off + i * 4 + j / 8] |= uint8_t(1 << (7 - j % 8));
    }
    int doom = n(s["shadowlordDoomBits"]);
    b[0x624] |= uint8_t(doom);
    b[0x625] |= uint8_t(doom >> 8);
    side = J::object();
    side["version"] = J(1);
    side["qol"] = J::object();
    side["gameState"] = J::object();
    for (auto k : {"transport", "questFlags"})
        if (s.has(k))
            side["gameState"][k] = s[k];
    for (auto k : extras)
        if (s.has(k))
            side["gameState"][k] = s[k];
    if (gate && s.has("npcWalk"))
        side["gameState"]["npcWalk"] = s["npcWalk"];
    for (auto k : {"journal", "explored", "treasuryLoot"})
        if (s.has(k))
            side["qol"][k] = s[k];
    return Error::None;
}
Ool build_ool(const J &s, const uint8_t *base, size_t len) {
    Ool out{};
    if (base && len >= kOolSize)
        std::copy(base, base + kOolSize, out.begin());
    if (n(s["position"]["location"]))
        return out;
    auto *b = out.data() + (n(s["position"]["floor"]) == 255 ? 256 : 0);
    b[0] = b[1] = uint8_t(s.has("transportTile") ? n(s["transportTile"]) : 28);
    b[2] = uint8_t(n(s["position"]["x"]));
    b[3] = uint8_t(n(s["position"]["y"]));
    b[4] = uint8_t(n(s["position"]["floor"]));
    b[5] = uint8_t(n(s["shipHull"]));
    b[7] = uint8_t(n(s["shipSkiffs"]));
    enemy_table(b, s["overworldEnemies"], n(s["position"]["floor"]), 0);
    return out;
}
Error import_save(const uint8_t *b, size_t len, const std::string *explicit_sidecar, J &state,
                  SidecarSource &source, bool gate) {
    if (len < kGamSize)
        return Error::ShortGam;
    J side;
    SidecarSource selected;
    if (explicit_sidecar) {
        auto e = parse(*explicit_sidecar, side);
        if (e != Error::None)
            return e;
        selected = SidecarSource::File;
    } else {
        auto env = read_envelope(b, len);
        selected = env.kind == EnvelopeKind::Ok ? SidecarSource::Envelope : SidecarSource::None;
        side = env.kind == EnvelopeKind::Ok ? env.document["sidecar"] : empty_sidecar();
    }
    auto e = import_native(b, len, side, state, gate);
    if (e == Error::None)
        source = selected;
    return e;
}
Error deserialize(const std::string &text, J &out) {
    J s;
    auto error = parse(text, s);
    if (error != Error::None)
        return error;
    if (s["version"].kind != J::Number || s["version"].number != 1)
        return Error::Version;
    if (s["characters"].kind != J::Array || s["characters"].values.empty())
        return Error::Characters;
    auto num = [](const J &v) { return v.kind == J::Number && std::isfinite(v.number); };
    for (const auto &c : s["characters"].values) {
        if (c.kind != J::Object || c["name"].kind != J::String || c["status"].kind != J::String)
            return Error::Schema;
        for (auto k : {"strength", "dexterity", "intelligence", "currentMp", "currentHp", "maxHp", "exp",
                       "level", "helmet", "armor", "weapon", "shield", "ring", "amulet"})
            if (!num(c[k]))
                return Error::Schema;
    }
    if (!num(s["partySize"]) || std::floor(s["partySize"].number) != s["partySize"].number ||
        s["partySize"].number < 1 || s["partySize"].number > double(s["characters"].values.size()))
        return Error::Schema;
    for (auto k : {"location", "floor", "x", "y"})
        if (!num(s["position"][k]))
            return Error::Schema;
    for (auto k : {"year", "month", "day", "hour", "minute"})
        if (!num(s["time"][k]))
            return Error::Schema;
    for (auto k : {"food", "gold", "keys", "gems", "torches", "skullKeys", "karma", "turnsSinceStart"})
        if (s.has(k) && !num(s[k]))
            return Error::Schema;
    for (auto f : arrays)
        if (s.has(f.name)) {
            if (s[f.name].kind != J::Array)
                return Error::Schema;
            for (const auto &v : s[f.name].values)
                if (!num(v))
                    return Error::Schema;
        }
    for (auto k : {"prevHour", "lightSpellMins", "wind", "sailDir", "windDriftCtr", "shipHull", "shipSkiffs",
                   "hmsCapeToggle", "drunkTurns", "shrineQuestBitmap", "shrineVisitedBitmap",
                   "shadowlordDoomBits", "skullTreeFoundDay"})
        if (!s.has(k))
            s[k] = J(0);
    if (!s.has("wornCrown"))
        s["wornCrown"] = J(false);
    if (!s.has("transportTile"))
        s["transportTile"] = J(28);
    for (auto k : {"shrineDestroyed", "shadowlordLocs", "openDoors", "overworldEnemies", "worldObjects"})
        if (!s.has(k))
            s[k] = J::array();
    if (!s.has("mapOverrides"))
        s["mapOverrides"] = J::object();
    if (!s.has("dungeonRoomsCleared")) {
        s["dungeonRoomsCleared"] = J::array();
        s["dungeonRoomsCleared"].values.resize(14, J(0));
    }
    if (!s.has("reagentPatchFoundDay")) {
        s["reagentPatchFoundDay"] = J::array();
        s["reagentPatchFoundDay"].values.resize(3, J(0));
    }
    for (auto k : {"overworldEnemies", "worldObjects"})
        if (s[k].kind != J::Array)
            s[k] = J::array();
    if (s.has("questFlags"))
        for (auto k : {"search:13", "search:14", "search:15"})
            s["questFlags"].erase(k);
    s.erase("treasuryLoot");
    for (auto k : {"17:-1:6:10", "17:-1:7:10", "17:-1:10:10", "17:-1:11:10", "17:-1:14:10", "17:-1:15:10",
                   "17:-1:14:22", "17:-1:16:22"}) {
        const auto &v = static_cast<const J &>(s)["mapOverrides"][k];
        if (v.kind == J::Number && (v.number == 257 || v.number == 68))
            s["mapOverrides"].erase(k);
    }
    out = std::move(s);
    return Error::None;
}
uint32_t save_crc32(const uint8_t *bytes, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return crc ^ 0xffffffff;
}
bool complete_generation(const Generation &g) {
    if (!g.committed || !g.identity_matches || !g.gam || g.gam_size < kGamSize ||
        (g.requires_ool && (!g.ool || g.ool_size != kOolSize)) || (g.requires_sidecar && !g.sidecar))
        return false;
    if (save_crc32(g.gam, g.gam_size) != g.gam_crc)
        return false;
    if (g.ool && (g.ool_size != kOolSize || save_crc32(g.ool, g.ool_size) != g.ool_crc))
        return false;
    if (g.sidecar) {
        if (save_crc32(reinterpret_cast<const uint8_t *>(g.sidecar->data()), g.sidecar->size()) !=
            g.sidecar_crc)
            return false;
        J side;
        if (parse(*g.sidecar, side) != Error::None || !sane_sidecar(side))
            return false;
    } else if (read_envelope(g.gam, g.gam_size).kind == EnvelopeKind::Bad)
        return false;
    J decoded;
    SidecarSource source;
    return import_save(g.gam, g.gam_size, g.sidecar, decoded, source) == Error::None;
}
int select_generation(const Generation *g, size_t count) {
    int best = -1;
    for (size_t i = 0; i < count; ++i)
        if (complete_generation(g[i]) && (best < 0 || g[i].sequence > g[size_t(best)].sequence))
            best = int(i);
    return best;
}
int autosave_slot(int64_t pointer) { return int(pointer % 3) + 1; }
int most_recent_slot(const Json &index) {
    if (index.kind != J::Array || index.values.empty())
        return -1;
    size_t best = 0;
    for (size_t i = 1; i < index.values.size(); ++i)
        if (index.at(i)["timestamp"].number > index.at(best)["timestamp"].number)
            best = i;
    return int(best);
}
} // namespace openu5::save
