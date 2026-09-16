#include "openu5/persistence.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace openu5::save {
namespace {
using J = Json;
bool fits(const J &v, int64_t lo, int64_t hi) {
    return v.kind == J::Number && std::isfinite(v.number) && std::floor(v.number) == v.number &&
           v.number >= double(lo) && v.number <= double(hi);
}
constexpr const char *modes[] = {"foot", "horse", "carpet", "skiff", "ship"};
// Capture only fields owned by these structs. World services retain their JSON
// in the same document until their save/restore adapter is installed.
void char_out(const CharacterState &c, J &j) {
    j["name"] = J("");
    for (size_t i = 0; i < 9 && c.name[i]; ++i)
        j["name"].string += char16_t(uint8_t(c.name[i]));
    j["class"] = J("");
    j["class"].string += char16_t(uint8_t(c.character_class));
    j["status"] = J("");
    j["status"].string += char16_t(uint8_t(c.status));
#define F(key, field) j[key] = J(int(c.field))
    F("gender", gender);
    F("strength", strength);
    F("dexterity", dexterity);
    F("intelligence", intelligence);
    F("currentMp", current_mp);
    F("currentHp", current_hp);
    F("maxHp", max_hp);
    F("exp", exp);
    F("level", level);
    F("monthsAtInn", months_at_inn);
    F("helmet", helmet);
    F("armor", armor);
    F("weapon", weapon);
    F("shield", shield);
    F("ring", ring);
    F("amulet", amulet);
    F("partyStatus", party_status);
#undef F
}
bool char_in(const J &j, CharacterState &c) {
    if (j["name"].kind != J::String || j["name"].string.size() > 9)
        return false;
    for (size_t i = 0; i < j["name"].string.size(); ++i) {
        auto ch = j["name"].string[i];
        if (ch > 255 || ch == 0)
            return false;
        c.name[i] = char(ch);
    }
    for (auto k : {"class", "status"})
        if (j[k].kind != J::String || j[k].string.size() != 1 || j[k].string[0] > 255)
            return false;
    c.character_class = char(j["class"].string[0]);
    c.status = char(j["status"].string[0]);
#define F(key, field, hi)                                                                                    \
    if (!fits(j[key], 0, hi))                                                                                \
        return false;                                                                                        \
    c.field = decltype(c.field)(j[key].integer())
    F("gender", gender, 255);
    F("strength", strength, 255);
    F("dexterity", dexterity, 255);
    F("intelligence", intelligence, 255);
    F("currentMp", current_mp, 255);
    F("currentHp", current_hp, 65535);
    F("maxHp", max_hp, 65535);
    F("exp", exp, 65535);
    F("level", level, 255);
    F("monthsAtInn", months_at_inn, 255);
    F("helmet", helmet, 255);
    F("armor", armor, 255);
    F("weapon", weapon, 255);
    F("shield", shield, 255);
    F("ring", ring, 255);
    F("amulet", amulet, 255);
    F("partyStatus", party_status, 255);
#undef F
    return true;
}
} // namespace
void capture_core(const GameState &g, const TurnState &t, Json &s) {
    s["version"] = J(int(g.version));
    s["characters"].kind = J::Array;
    s["characters"].values.resize(g.party.character_count);
    for (size_t i = 0; i < g.party.character_count; ++i)
        char_out(g.party.characters[i], s["characters"].values[i]);
#define F(key, field) s[key] = J(double(g.field))
    F("partySize", party.party_size);
    F("activeCharacter", party.active_character);
    F("food", food);
    F("gold", gold);
    F("keys", keys);
    F("gems", gems);
    F("torches", torches);
    F("skullKeys", skull_keys);
    F("magicCarpets", magic_carpets);
    F("karma", karma);
    F("turnsSinceStart", turns_since_start);
    F("torchTurns", torch_turns);
    F("shipHull", ship_hull);
    F("shipSkiffs", ship_skiffs);
#undef F
    s["grapple"] = J(g.grapple);
    s["wornCrown"] = J(g.worn_crown);
    s["transport"] = J(modes[unsigned(g.transport)]);
#define F(key, field) s["specialItems"][key] = J(g.field)
    F("spyglass", spyglass);
    F("sextant", sextant);
    F("blackBadge", black_badge);
    F("woodenBox", wooden_box);
#undef F
#define F(key, field) s["time"][key] = J(g.time.field)
    F("year", year);
    F("month", month);
    F("day", day);
    F("hour", hour);
    F("minute", minute);
#undef F
    s["position"]["location"] = J(int(g.position.map.location));
    s["position"]["floor"] = J(int(g.position.map.floor));
    s["position"]["x"] = J(int(g.position.xy.x));
    s["position"]["y"] = J(int(g.position.xy.y));
    auto array = [&](const char *key, const auto &a, size_t count) {
        J v = J::array();
        for (size_t i = 0; i < count; ++i)
            v.values.emplace_back(int(a[i]));
        s[key] = std::move(v);
    };
    array("equipmentQuantities", g.equipment_quantities, g.equipment_count);
    array("spellQuantities", g.spell_quantities, 48);
    array("scrollQuantities", g.scroll_quantities, 8);
    array("potionQuantities", g.potion_quantities, 8);
    array("reagentQuantities", g.reagent_quantities, 8);
    array("dungeonRoomsCleared", g.dungeon_rooms_cleared, 14);
    for (auto key : {"npcDead", "npcMet"}) {
        const auto *rows = std::strcmp(key, "npcDead") == 0 ? g.npc_dead : g.npc_met;
        J a = J::array();
        for (size_t i = 0; i < 32; ++i) {
            J row = J::array();
            for (size_t bit = 0; bit < 32; ++bit)
                row.values.emplace_back((rows[i] & (uint32_t(1) << bit)) != 0);
            a.values.push_back(row);
        }
        s[key] = std::move(a);
    }
#define F(key, field) s[key] = J(t.field)
    F("prevHour", prev_hour);
    F("lightSpellMins", light_spell_minutes);
    F("drunkTurns", drunk_turns);
    F("transportTile", transport_tile);
    F("wind", wind);
    F("windDriftCtr", wind_drift_counter);
    F("skullTreeFoundDay", skull_tree_day);
#undef F
    if (t.spell_turns < 0)
        s.erase("timeSpellTurns");
    else
        s["timeSpellTurns"] = J(t.spell_turns);
    if (t.time_spell) {
        s["timeSpell"] = J("");
        s["timeSpell"].string += char16_t(uint8_t(t.time_spell));
    } else
        s.erase("timeSpell");
    array("reagentPatchFoundDay", t.reagent_days, 3);
    array("shadowlordLocs", t.shadowlord_locations, t.has_shadowlords ? 3 : 0);
    // Negative phase values represent the reference's absence-significant latch.
    if (t.felucca_phase < 0)
        s.erase("feluccaPhase");
    else
        s["feluccaPhase"] = J(t.felucca_phase);
    if (t.trammel_phase < 0)
        s.erase("trammelPhase");
    else
        s["trammelPhase"] = J(t.trammel_phase);
}
Error restore_core(const Json &s, GameState &game, TurnState &turn) {
    // The document codec accepts TS's broad JSON domain. A live bounded C++
    // projection must reject fields it cannot represent, not narrow silently.
    for (auto key : {"shipHull", "shipSkiffs", "prevHour", "lightSpellMins", "drunkTurns", "transportTile",
                     "wind", "windDriftCtr", "skullTreeFoundDay"})
        if (s.has(key) && !fits(s[key], INT32_MIN, INT32_MAX))
            return Error::NativeDomain;
    for (auto key : {"feluccaPhase", "trammelPhase", "timeSpellTurns"})
        if (s.has(key) && !fits(s[key], 0, INT32_MAX))
            return Error::NativeDomain;
    for (auto key : {"spellQuantities", "scrollQuantities", "potionQuantities", "reagentQuantities"}) {
        const size_t count = std::strcmp(key, "spellQuantities") == 0 ? 48 : 8;
        if (s[key].kind != J::Array || s[key].values.size() != count)
            return Error::NativeDomain;
    }
    for (auto key : {"reagentPatchFoundDay", "shadowlordLocs"}) {
        if (!s.has(key))
            continue;
        if (s[key].kind != J::Array || (s[key].values.size() != 3 && !s[key].values.empty()))
            return Error::NativeDomain;
        for (const auto &v : s[key].values)
            if (!fits(v, INT32_MIN, INT32_MAX))
                return Error::NativeDomain;
    }
    if (s.has("dungeonRoomsCleared")) {
        if (s["dungeonRoomsCleared"].kind != J::Array || s["dungeonRoomsCleared"].values.size() > 14)
            return Error::NativeDomain;
        for (const auto &v : s["dungeonRoomsCleared"].values)
            if (!fits(v, 0, 255))
                return Error::NativeDomain;
    }
    for (auto key : {"npcDead", "npcMet"}) {
        if (s[key].kind != J::Array || s[key].values.size() != 32)
            return Error::NativeDomain;
        for (const auto &row : s[key].values) {
            if (row.kind != J::Array || row.values.size() != 32)
                return Error::NativeDomain;
            for (const auto &v : row.values)
                if (v.kind != J::Bool)
                    return Error::NativeDomain;
        }
    }
    GameState g{};
    TurnState t{};
    g.rng = game.rng; // TS persistence does not serialize Game.liveRng.
    if (s["characters"].kind != J::Array || s["characters"].values.size() > 16)
        return Error::NativeDomain;
    g.party.character_count = uint8_t(s["characters"].values.size());
    for (size_t i = 0; i < g.party.character_count; ++i)
        if (!char_in(s["characters"].at(i), g.party.characters[i]))
            return Error::NativeDomain;
#define F(key, field, lo, hi)                                                                                \
    if (!fits(s[key], lo, hi))                                                                               \
        return Error::NativeDomain;                                                                          \
    g.field = decltype(g.field)(s[key].integer())
    F("partySize", party.party_size, 0, 16);
    F("activeCharacter", party.active_character, 0, 255);
    F("food", food, 0, 65535);
    F("gold", gold, 0, 65535);
    F("keys", keys, INT32_MIN, INT32_MAX);
    F("gems", gems, INT32_MIN, INT32_MAX);
    F("torches", torches, INT32_MIN, INT32_MAX);
    F("skullKeys", skull_keys, INT32_MIN, INT32_MAX);
    F("magicCarpets", magic_carpets, INT32_MIN, INT32_MAX);
    F("karma", karma, 0, 255);
    F("turnsSinceStart", turns_since_start, -9007199254740991LL, 9007199254740991LL);
    F("torchTurns", torch_turns, 0, 65535);
#undef F
    g.ship_hull = s.has("shipHull") ? int32_t(s["shipHull"].integer()) : 99;
    g.ship_skiffs = int32_t(s["shipSkiffs"].integer());
    g.grapple = s["grapple"].truth();
    g.worn_crown = s["wornCrown"].truth();
    g.spyglass = s["specialItems"]["spyglass"].truth();
    g.sextant = s["specialItems"]["sextant"].truth();
    g.black_badge = s["specialItems"]["blackBadge"].truth();
    g.wooden_box = s["specialItems"]["woodenBox"].truth();
    bool found = false;
    for (unsigned i = 0; i < 5; ++i)
        if (s["transport"].string == J(modes[i]).string) {
            g.transport = TransportMode(i);
            found = true;
        }
    if (!found)
        return Error::NativeDomain;
#define F(key, field)                                                                                        \
    if (!fits(s["time"][key], INT32_MIN, INT32_MAX))                                                         \
        return Error::NativeDomain;                                                                          \
    g.time.field = int32_t(s["time"][key].integer())
    F("year", year);
    F("month", month);
    F("day", day);
    F("hour", hour);
    F("minute", minute);
#undef F
    const auto &p = s["position"];
    if (!fits(p["location"], 0, 255) || !fits(p["floor"], INT16_MIN, INT16_MAX) || !fits(p["x"], 0, 255) ||
        !fits(p["y"], 0, 255))
        return Error::NativeDomain;
    g.position.map = {uint8_t(p["location"].integer()), int16_t(p["floor"].integer())};
    g.position.xy = {uint8_t(p["x"].integer()), uint8_t(p["y"].integer())};
    auto array = [&](const char *key, auto &a, size_t max) {
        const auto &v = s[key];
        if (v.kind != J::Array || v.values.size() > max)
            return false;
        for (size_t i = 0; i < v.values.size(); ++i) {
            if (!fits(v.at(i), INT32_MIN, INT32_MAX))
                return false;
            a[i] = int32_t(v.at(i).integer());
        }
        return true;
    };
    if (!array("equipmentQuantities", g.equipment_quantities, 256) ||
        !array("spellQuantities", g.spell_quantities, 48) ||
        !array("scrollQuantities", g.scroll_quantities, 8) ||
        !array("potionQuantities", g.potion_quantities, 8) ||
        !array("reagentQuantities", g.reagent_quantities, 8))
        return Error::NativeDomain;
    g.equipment_count = uint16_t(s["equipmentQuantities"].values.size());
    for (size_t i = 0; i < 14; ++i)
        g.dungeon_rooms_cleared[i] = uint8_t(s["dungeonRoomsCleared"].at(i).integer());
    for (size_t i = 0; i < 32; ++i)
        for (size_t bit = 0; bit < 32; ++bit) {
            if (s["npcDead"].at(i).at(bit).truth())
                g.npc_dead[i] |= uint32_t(1) << bit;
            if (s["npcMet"].at(i).at(bit).truth())
                g.npc_met[i] |= uint32_t(1) << bit;
        }
#define F(key, field) t.field = int32_t(s[key].integer())
    F("prevHour", prev_hour);
    F("lightSpellMins", light_spell_minutes);
    F("drunkTurns", drunk_turns);
    F("transportTile", transport_tile);
    F("wind", wind);
    F("windDriftCtr", wind_drift_counter);
    F("skullTreeFoundDay", skull_tree_day);
#undef F
    t.felucca_phase = int32_t(s["feluccaPhase"].integer(-1));
    t.trammel_phase = int32_t(s["trammelPhase"].integer(-1));
    t.spell_turns = int32_t(s["timeSpellTurns"].integer(-1));
    if (s["timeSpell"].kind == J::String && !s["timeSpell"].string.empty()) {
        if (s["timeSpell"].string.size() != 1 || s["timeSpell"].string[0] > 255)
            return Error::NativeDomain;
        t.time_spell = char(s["timeSpell"].string[0]);
    }
    for (size_t i = 0; i < 3; ++i) {
        t.reagent_days[i] = int32_t(s["reagentPatchFoundDay"].at(i).integer());
        t.shadowlord_locations[i] = int32_t(s["shadowlordLocs"].at(i).integer(128));
    }
    t.has_shadowlords = !s["shadowlordLocs"].values.empty();
    game = g;
    turn = t;
    return Error::None;
}
Error load_state(const std::string &text, GameState &g, TurnState &t, Json &retained) {
    Json doc;
    auto e = deserialize(text, doc);
    if (e != Error::None)
        return e;
    e = restore_core(doc, g, t);
    if (e == Error::None)
        retained = std::move(doc);
    return e;
}
Error save_state(const GameState &g, const TurnState &t, const Json &retained, std::string &out) {
    Json doc = retained;
    capture_core(g, t, doc);
    return encode_json(doc, out) == JsonError::None ? Error::None : Error::Capacity;
}
Error load_native_state(const uint8_t *b, size_t len, const std::string *side, GameState &g, TurnState &t,
                        Json &retained, SidecarSource &source, bool gate) {
    Json doc;
    SidecarSource selected;
    auto e = import_save(b, len, side, doc, selected, gate);
    if (e != Error::None)
        return e;
    e = restore_core(doc, g, t);
    if (e == Error::None) {
        retained = std::move(doc);
        source = selected;
    }
    return e;
}
Error export_native_state(const GameState &g, const TurnState &t, const Json &retained, const uint8_t *base,
                          size_t length, Gam &gam, Json &side, bool gate) {
    Json doc = retained;
    capture_core(g, t, doc);
    return export_native(doc, base, length, gam, side, gate);
}
void capture_npc_walk(const NpcActors &actors, uint8_t location, Json &state) {
    Json walk = Json::object();
    walk["location"] = Json(int(location));
    walk["slots"] = Json::array();
    for (size_t i = 0; i < actors.count; ++i) {
        const auto &a = actors.actors[i];
        Json v = Json::object();
        v["slot"] = Json(int(a.schedule.slot));
        v["x"] = Json(int(a.x));
        v["y"] = Json(int(a.y));
        v["z"] = Json(int(a.z));
        v["state"] = Json(int(a.state));
        v["servedSlot"] = Json(int(a.served_slot));
        v["pathIdx"] = Json(int(a.path_index));
        v["stuck"] = Json(int(a.stuck));
        v["pathBuf"] = Json::array();
        for (auto b : a.path)
            v["pathBuf"].values.emplace_back(int(b));
        walk["slots"].values.push_back(std::move(v));
    }
    state["npcWalk"] = std::move(walk);
}
Error restore_npc_walk(const Json &state, uint8_t location, bool gate, NpcActors &actors) {
    if (actors.count > actors.actors.size())
        return Error::NativeDomain;
    for (size_t i = 0; i < actors.count; ++i)
        if (actors.actors[i].schedule.slot >= 32)
            return Error::NativeDomain;
    const auto &walk = state["npcWalk"];
    if (!walk.truth()) {
        if (gate)
            actors.count = 0;
        return Error::None;
    }
    if (walk["location"].integer(-1) != location)
        return Error::None;
    if (walk["slots"].kind != Json::Array)
        return Error::Schema;
    // Validate before mutating; duplicate slots use the last record as TS Map does.
    const Json *by_slot[32]{};
    for (const auto &w : walk["slots"].values) {
        if (!fits(w["slot"], 0, 31))
            return Error::NativeDomain;
        for (auto key : {"x", "y", "z", "pathIdx", "stuck"})
            if (!fits(w[key], INT16_MIN, INT16_MAX))
                return Error::NativeDomain;
        for (auto key : {"state", "servedSlot"})
            if (!fits(w[key], 0, 255))
                return Error::NativeDomain;
        if (w["pathBuf"].kind != Json::Array)
            return Error::Schema;
        for (size_t i = 0; i < std::min(size_t(32), w["pathBuf"].values.size()); ++i)
            if (!fits(w["pathBuf"].at(i), 0, 255))
                return Error::NativeDomain;
        by_slot[size_t(w["slot"].integer())] = &w;
    }
    size_t count = 0;
    for (size_t i = 0; i < actors.count; ++i) {
        auto a = actors.actors[i];
        const auto *v = by_slot[a.schedule.slot];
        if (!v)
            continue;
        const auto &w = *v;
        a.x = int16_t(w["x"].integer());
        a.y = int16_t(w["y"].integer());
        a.z = int16_t(w["z"].integer());
        a.path_index = int16_t(w["pathIdx"].integer());
        a.stuck = int16_t(w["stuck"].integer());
        a.state = uint8_t(w["state"].integer());
        a.served_slot = uint8_t(w["servedSlot"].integer());
        for (size_t j = 0; j < 32; ++j)
            a.path[j] = uint8_t(w["pathBuf"].at(j).integer());
        actors.actors[count++] = a;
    }
    actors.count = count;
    return Error::None;
}
} // namespace openu5::save
