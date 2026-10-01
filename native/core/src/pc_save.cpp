#include "openu5/pc_save.h"
#include "openu5/gameplay_save.h"
#include "openu5/quest_world.h"
#include "openu5/transport.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace openu5::save::pc {
namespace {
using J = Json;

// SAVED.GAM offsets (docs/formats/tlk-npc-dataovl-gam.md section 4; the
// window is DS:0x55A6, so a global's offset is its DS address - 0x55A6).
constexpr size_t kRoster = 0x02, kRecord = 32;
constexpr size_t kSkullTreeDay = 0x20C;   // [0x57b2]      B6
constexpr size_t kReagentDays = 0x2B2;    // [0x5858..5a]  B7
constexpr size_t kPartySize = 0x2B5;
constexpr size_t kSearchFound = 0x2B6;    // [0x585c], 15 bytes  B1 (SJOG:0x0514)
constexpr size_t kMonth = 0x2D7, kDay = 0x2D8, kHour = 0x2D9, kMinute = 0x2DB;
constexpr size_t kTimeSpell = 0x2D4;      // g_time_spell       B8
constexpr size_t kTransportTile = 0x2D6;
constexpr size_t kWindDrift = 0x2DD;      // g_wind_drift_ctr   B4
constexpr size_t kTimeSpellTurns = 0x2E8; // g_time_spell_turns B8
constexpr size_t kWind = 0x2EC;           // g_wind             B2
constexpr size_t kLocation = 0x2ED, kFloor = 0x2EF, kX = 0x2F0, kY = 0x2F1;
constexpr size_t kLightSpell = 0x300;     // g_light_spell_mins B3
constexpr size_t kSailDir = 0x3AF;        // g_sail_dir         B5
constexpr size_t kLiveTable = 0x6B4;      // DS:0x5C5A, 32 records x 8 B
// SAVED.OOL: BRIT (the surface) then UNDER (the underworld).
constexpr size_t kBlock = 256;
constexpr int kSearchEntries = 113;       // SJOG:0x062b cmp si,0x71
bool search_gated(int i) { return i == 13 || i == 14 || i == 15; } // keys / daily tree / equipment

// The vehicle tile families (world/transport.ts; the object record's +0).
bool frigate(int b) { return (b & 0xf8) == 0x20; }
bool skiff(int b) { return (b & 0xfc) == 0x28; }
bool horse(int b) { return (b & 0xfe) == 0x10; }
bool carpet(int b) { return (b & 0xfc) == 0x14; }

constexpr const char *kModes[] = {"foot", "horse", "carpet", "skiff", "ship"};

std::string terrain_key(int floor, int x, int y) {
    char key[32];
    std::snprintf(key, sizeof(key), "0:%d:%d:%d", floor, x, y);
    return key;
}

// A vector as the object pool, so the documents it fills have exactly the
// shape the runtime's own capture writes.
struct VectorPool {
    std::vector<QuestObject> objects;
    QuestWorldServices services() {
        QuestWorldServices s{};
        s.context = this;
        s.count = [](void *p) { return static_cast<VectorPool *>(p)->objects.size(); };
        s.read = [](void *p, size_t i) { return static_cast<VectorPool *>(p)->objects[i]; };
        return s;
    }
};

// B10, import: the vehicles of one 32-record table of world `floor` (0 or 255).
void read_vehicles(const uint8_t *table, int floor, VectorPool &pool, J &overrides, ImportReport &r) {
    for (int i = 1; i < 32; ++i) {
        const uint8_t *o = table + i * 8;
        const int b = o[0];
        if (frigate(b)) {
            QuestObject q;
            q.location = 0; q.floor = floor; q.x = o[2]; q.y = o[3]; q.tile = 256 + b;
            q.hull = o[5]; q.skiffs = o[7]; q.ship = true; // as place_purchased_ship / park_ship
            pool.objects.push_back(q);
            ++r.frigates;
        } else if (horse(b) || skiff(b)) {
            // Native leaves a dismounted horse or skiff as a persistent terrain
            // cell (commands.cpp: s->drop(pos, drop_tile + 256)).
            overrides[terrain_key(floor, o[2], o[3]).c_str()] = J(256 + b);
            ++(horse(b) ? r.horses : r.skiffs);
        }
    }
}

// B10, export: one vehicle into the first free record of a table, from the top
// (the placement the codec's object_table uses for vehicles).
bool place_vehicle(uint8_t *table, int floor, int x, int y, int b, int hull, int skiffs) {
    for (int i = 31; i >= 1; --i) {
        uint8_t *o = table + i * 8;
        if (o[0]) continue;
        o[0] = o[1] = uint8_t(b);
        o[2] = uint8_t(x); o[3] = uint8_t(y); o[4] = uint8_t(floor);
        o[5] = uint8_t(frigate(b) ? hull : 0); o[6] = 0; o[7] = uint8_t(frigate(b) ? skiffs : 0);
        return true;
    }
    return false;
}

// The table a vehicle of world `floor` lives in: the live one when the party
// is outdoors in that world, else that world's parked block.
uint8_t *table_for(int floor, int location, int party_floor, Gam &gam, Ool &ool) {
    if (location == 0 && party_floor == floor) return gam.data() + kLiveTable;
    return ool.data() + (floor == 255 ? kBlock : 0);
}
const uint8_t *table_for(int floor, int location, int party_floor, const uint8_t *gam, const uint8_t *ool) {
    if (location == 0 && party_floor == floor) return gam + kLiveTable;
    return ool + (floor == 255 ? kBlock : 0);
}

// B1-B9 and B10 into a sidecar-less import's document.
void complete_import(const uint8_t *gam, const uint8_t *ool, J &s, ImportReport &r) {
    // B1: the "found once" bitmap; N=13/14/15 are gated by other state.
    for (int i = 0; i < kSearchEntries; ++i) {
        if (search_gated(i) || !(gam[kSearchFound + i / 8] & (1u << (i % 8)))) continue;
        char key[16];
        std::snprintf(key, sizeof(key), "search:%d", i);
        s["questFlags"][key] = J(true);
        ++r.search_found;
    }
    // B2-B7: single raw bytes, the value the runtime uses.
    s["wind"] = J(int(gam[kWind]));
    s["lightSpellMins"] = J(int(gam[kLightSpell]));
    s["windDriftCtr"] = J(int(gam[kWindDrift]));
    s["sailDir"] = J(int(gam[kSailDir]));
    s["skullTreeFoundDay"] = J(int(gam[kSkullTreeDay]));
    J days = J::array();
    for (size_t i = 0; i < 3; ++i) days.values.emplace_back(int(gam[kReagentDays + i]));
    s["reagentPatchFoundDay"] = std::move(days);
    // B8: 0 is "no effect"; the turns are only meaningful beside an effect.
    if (gam[kTimeSpell]) {
        J spell("");
        spell.string += char16_t(gam[kTimeSpell]);
        s["timeSpell"] = std::move(spell);
        s["timeSpellTurns"] = J(int(gam[kTimeSpellTurns]));
    } else {
        s.erase("timeSpell");
        s.erase("timeSpellTurns");
    }
    // B9: the mode follows the vehicle tile (empty_sidecar() says "foot").
    s["transport"] = J(kModes[unsigned(transport_mode(gam[kTransportTile]))]);
    // B10: the codec's reference-shaped vehicle entries are refused by
    // validate_world_objects (section 5.4); rebuild the pool from the tables.
    const int location = gam[kLocation], party_floor = gam[kFloor];
    VectorPool pool;
    J overrides = s.has("mapOverrides") && s["mapOverrides"].kind == J::Object ? s["mapOverrides"] : J::object();
    for (const int floor : {0, 255}) read_vehicles(table_for(floor, location, party_floor, gam, ool), floor, pool, overrides, r);
    s["mapOverrides"] = std::move(overrides);
    auto services = pool.services();
    capture_world_objects(services, s);
}
} // namespace

const char *check_text(Check c) {
    switch (c) {
    case Check::Ok: return "PC save is valid";
    case Check::GamSize: return "SAVED.GAM is not 4192 bytes";
    case Check::OolSize: return "SAVED.OOL is not 512 bytes";
    case Check::Party: return "SAVED.GAM: party size is not 1-6";
    case Check::Roster: return "SAVED.GAM: party records are not valid";
    case Check::Clock: return "SAVED.GAM: game clock is not valid";
    case Check::Location: return "SAVED.GAM: position is not valid";
    case Check::Dungeon: return "Dungeon saves cannot be transferred";
    case Check::Weather: return "SAVED.GAM: wind/sail bytes not valid";
    case Check::Codec: return "SAVED.GAM could not be read";
    }
    return "PC save is not valid";
}

Check check_original(const uint8_t *gam, size_t gam_size, const uint8_t *ool, size_t ool_size) {
    if (!gam || gam_size != kGamSize) return Check::GamSize;
    if (!ool || ool_size != kOolSize) return Check::OolSize;
    const int party = gam[kPartySize];
    if (party < 1 || party > 6) return Check::Party;
    for (int m = 0; m < party; ++m) {
        const uint8_t *r = gam + kRoster + size_t(m) * kRecord;
        if (r[0] < 0x20 || r[0] > 0x7e) return Check::Roster;
        for (int k = 1; k < 9 && r[k]; ++k)
            if (r[k] < 0x20 || r[k] > 0x7e) return Check::Roster;
        if (r[9] != 0x0b && r[9] != 0x0c) return Check::Roster;
        const char cls = char(r[10]), st = char(r[11]);
        if (cls != 'A' && cls != 'B' && cls != 'F' && cls != 'M') return Check::Roster;
        if (st != 'G' && st != 'P' && st != 'S' && st != 'D' && st != 'C') return Check::Roster;
    }
    if (gam[kMonth] < 1 || gam[kMonth] > 13 || gam[kDay] < 1 || gam[kDay] > 28 || gam[kHour] > 23 || gam[kMinute] > 59)
        return Check::Clock;
    const int location = gam[kLocation], floor = gam[kFloor];
    if (location > 40) return Check::Location;
    if (location >= 33) return Check::Dungeon;
    if (location == 0 && floor != 0 && floor != 0xff) return Check::Location;
    if (location && ((floor > 7 && floor != 0xff) || gam[kX] > 31 || gam[kY] > 31)) return Check::Location;
    if (gam[kWind] > 4 || gam[kSailDir] > 4) return Check::Weather;
    return Check::Ok;
}

OriginalSummary summarize_original(const uint8_t *gam) {
    OriginalSummary o;
    for (size_t k = 0; k < 9 && gam[kRoster + k]; ++k) o.name[k] = char(gam[kRoster + k]);
    o.location = gam[kLocation];
    o.party = gam[kPartySize];
    o.floor = gam[kFloor] == 0xff ? int16_t(-1) : int16_t(gam[kFloor]);
    return o;
}

Error import_original(const uint8_t *gam, const uint8_t *ool, GameState &game, TurnState &turn, Json &document,
                      ImportReport *report) {
    // The codec's own reading, as New Journey reads INIT.GAM: no sidecar
    // (empty_sidecar), the NPC fidelity gate on.
    J doc;
    SidecarSource source;
    auto e = import_save(gam, kGamSize, nullptr, doc, source, true);
    if (e != Error::None) return e;
    ImportReport r;
    complete_import(gam, ool, doc, r);
    GameState g = game;
    TurnState t = turn;
    e = restore_core(doc, g, t);
    if (e != Error::None) return e;
    game = g;
    turn = t;
    document = std::move(doc);
    if (report) *report = r;
    return Error::None;
}

Check exportable(const Json &s) {
    const auto location = s["position"]["location"].integer();
    if ((location >= 33 && location <= 40) || s["dungeon"].kind == J::Object) return Check::Dungeon;
    return Check::Ok;
}

void complete_export(const Json &s, Gam &gam, Ool &ool, ExportReport *report) {
    ExportReport r;
    uint8_t *b = gam.data();
    // B1
    for (int i = 0; i < kSearchEntries; ++i) {
        if (search_gated(i)) continue;
        char key[16];
        std::snprintf(key, sizeof(key), "search:%d", i);
        const uint8_t bit = uint8_t(1u << (i % 8));
        uint8_t &cell = b[kSearchFound + i / 8];
        if (s["questFlags"][key].truth()) {
            cell = uint8_t(cell | bit);
            ++r.search_found;
        } else
            cell = uint8_t(cell & ~bit);
    }
    // B2-B7 (absent = the runtime's default, 0).
    b[kWind] = uint8_t(s["wind"].integer());
    b[kLightSpell] = uint8_t(s["lightSpellMins"].integer());
    b[kWindDrift] = uint8_t(s["windDriftCtr"].integer());
    b[kSailDir] = uint8_t(s["sailDir"].integer());
    b[kSkullTreeDay] = uint8_t(s["skullTreeFoundDay"].integer());
    for (size_t i = 0; i < 3; ++i) b[kReagentDays + i] = uint8_t(s["reagentPatchFoundDay"].at(i).integer());
    // B8
    const auto &spell = s["timeSpell"];
    if (spell.kind == J::String && !spell.string.empty()) {
        b[kTimeSpell] = uint8_t(spell.string[0]);
        b[kTimeSpellTurns] = uint8_t(s["timeSpellTurns"].integer());
    } else
        b[kTimeSpell] = b[kTimeSpellTurns] = 0;
    // B10. The two tables written are the vehicles Native holds, exactly: the
    // template's own vehicle records go first (the new-game UNDER.OOL parks a
    // skiff at (14,242), which a Native journey only has if it was imported,
    // as a terrain cell -- kept, it would come back twice).
    const int location = int(s["position"]["location"].integer());
    const int party_floor = int(s["position"]["floor"].integer());
    for (const int floor : {0, 255}) {
        uint8_t *table = table_for(floor, location, party_floor, gam, ool);
        for (int i = 1; i < 32; ++i)
            if (frigate(table[i * 8]) || skiff(table[i * 8]) || horse(table[i * 8])) std::fill(table + i * 8, table + i * 8 + 8, uint8_t(0));
    }
    auto put = [&](int floor, int x, int y, int tile, int hull, int skiffs) {
        const int byte = tile - 256;
        if (!place_vehicle(table_for(floor, location, party_floor, gam, ool), floor, x, y, byte, hull, skiffs)) {
            ++r.unplaced;
            return;
        }
        ++(frigate(byte) ? r.frigates : horse(byte) ? r.horses : r.skiffs);
    };
    for (const auto &o : s["worldObjects"].values) {
        if (o["loot"].truth()) ++r.loot;
        if (o["shadowlord"].truth()) ++r.shadowlords;
        const int tile = int(o["tile"].integer()), floor = int(o["floor"].integer());
        if (o["location"].integer() != 0 || !o["ship"].truth() || (floor != 0 && floor != 255)) continue;
        if (!frigate(tile - 256) && !skiff(tile - 256)) continue;
        put(floor, int(o["x"].integer()), int(o["y"].integer()), tile, int(o["hull"].integer()), int(o["skiffs"].integer()));
    }
    const auto &cells = s["mapOverrides"];
    for (size_t i = 0; i < cells.keys.size() && i < cells.values.size(); ++i) {
        const std::string key(cells.keys[i].begin(), cells.keys[i].end());
        int loc = -1, floor = 0, x = 0, y = 0, n = 0;
        if (std::sscanf(key.c_str(), "%d:%d:%d:%d%n", &loc, &floor, &x, &y, &n) != 4 || size_t(n) != key.size()) continue;
        if (loc != 0 || (floor != 0 && floor != 255)) continue;
        const int tile = int(cells.values[i].integer()), byte = tile - 256;
        if (carpet(byte)) { ++r.carpets; continue; }
        if (horse(byte) || skiff(byte)) put(floor, x, y, tile, 0, 0);
    }
    r.town_npcs_left_out = location >= 1 && location <= 32;
    if (report) *report = r;
}

} // namespace openu5::save::pc
