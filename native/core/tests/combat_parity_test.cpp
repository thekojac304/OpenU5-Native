#include "openu5/combat.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace openu5;
using V = std::vector<int64_t>;
struct Harness {
    GameState g{}, initial{};
    TurnState t{};
    CombatState s{};
    CombatEnemy enemy{};
    CombatMap map{};
    TurnState initial_turn{};
    TurnState unused_turn{};
    TravelState travel{};
    CommandState commands{};
    WorldData world{};
    V game_events;
    int game_event_count = 0;
    CommandContext world_context() {
        CommandContext w{g, t, travel, commands, world};
        w.combat = s.initialized;
        w.events = {this, [](void *p, const GameEvent &e) {
                        auto &h = *static_cast<Harness *>(p);
                        if (e.kind == GameEventKind::Combat) {
                            h.event(*e.combat);
                            return;
                        }
                        ++h.game_event_count;
                        int kind = e.kind == GameEventKind::Message         ? 0
                                   : e.kind == GameEventKind::Sfx           ? 1
                                   : e.kind == GameEventKind::PartyChanged  ? 2
                                   : e.kind == GameEventKind::CombatStarted ? 3
                                   : e.kind == GameEventKind::CombatEnded   ? 4
                                                                            : -1;
                        h.game_events.push_back(kind);
                        if (!e.text)
                            h.game_events.push_back(-1);
                        else {
                            h.game_events.push_back(int64_t(std::strlen(e.text)));
                            for (auto p = e.text; *p; ++p)
                                h.game_events.push_back(uint8_t(*p));
                        }
                    }};
        w.rng_trace = context().trace;
        return w;
    }
    int32_t attacks[256]{}, ranges[256]{}, defenses[256]{}, strength[256]{};
    V events, draws;
    int event_count = 0;
    CombatContext context() {
        return {g,
                t,
                s,
                {attacks, ranges, defenses, strength, 256},
                {this, [](void *p, const CombatEvent &e) { static_cast<Harness *>(p)->event(e); }},
                {this, [](void *p, const char *, int32_t lo, int32_t hi, int32_t v) {
                     auto &d = static_cast<Harness *>(p)->draws;
                     d.insert(d.end(), {lo, hi, v});
                 }}};
    }
    void event(const CombatEvent &e) {
        ++event_count;
        events.insert(events.end(), {int(e.kind), e.actor, e.target, e.x, e.y, e.damage, e.hit,
                                     e.grazed, e.dragged});
        if (!e.text)
            events.push_back(-1);
        else {
            events.push_back(int64_t(std::strlen(e.text)));
            for (auto p = e.text; *p; ++p)
                events.push_back(uint8_t(*p));
        }
    }
    void init(int k, int seed) {
        g = {};
        t = {};
        s = {};
        map = {};
        events.clear();
        draws.clear();
        event_count = 0;
        std::fill_n(attacks, 256, 0);
        std::fill_n(ranges, 256, 0);
        std::fill_n(defenses, 256, 0);
        std::fill_n(strength, 256, 0);
        const int ids[] = {255, 18, 25, 26, 28, 36, 17, 19, 34, 35, 38, 39, 40},
                  values[] = {1, 20, 99}, rs[] = {2, 4, 15}, hps[] = {1, 10, 100};
        int w = ids[k % 13];
        attacks[w] = values[k % 3];
        ranges[w] = k % 13 < 2 ? 1 : rs[k % 3];
        defenses[10] = k % 4 * 10;
        strength[17] = 8;
        g.rng.seed(seed);
        g.party.character_count = 2;
        g.party.party_size = 2;
        g.party.active_character = k % 5 == 0 ? 0 : 255;
        g.food = 100;
        t.transport_tile = k % 23 == 0 ? 32 : 28;
        for (int i = 0; i < 2; ++i) {
            auto &r = g.party.characters[i];
            std::snprintf(r.name, sizeof(r.name), "MEM%d", i);
            r.character_class = 'A';
            r.status = i == 1 && k % 7 == 0 ? 'D' : i == 0 && k % 11 == 0 ? 'S' : 'G';
            r.current_hp = uint16_t(i == 0 && k % 29 == 0 ? 0 : hps[k % 3]);
            r.max_hp = 100;
            r.strength = 20;
            const int dex[] = {0, 15, 36, 255};
            r.dexterity = uint8_t(i == 0 ? 30 : dex[k % 4]);
            r.intelligence = 20;
            r.exp = 9990;
            r.level = 1;
            r.helmet = 255;
            r.armor = 10;
            r.weapon = uint8_t(w);
            r.shield = uint8_t(k % 6 == 0 ? w : 255);
            r.ring = uint8_t(k % 17 == 0 ? 44 : k % 19 == 0 ? 42 : 255);
            r.amulet = 255;
            r.party_status = 0;
        }
        g.equipment_quantities[16] = g.equipment_quantities[27] = k % 3;
        g.equipment_quantities[w] = k % 2;
        std::fill_n(map.tiles, 121, int16_t(5));
        map.starts[2][0] = {5, 5};
        map.starts[2][1] = {4, 5};
        map.start_count[2] = 2;
        map.units[0] = {6, 5};
        map.units[1] = {8, 5};
        map.units[2] = {5, 7};
        map.unit_count = 3;
        const int terrain[] = {5, 1, 7, 4};
        map.tiles[5 * 11 + 8] = int16_t(terrain[k % 4]);
        if (k % 31 == 0)
            map.tiles[5 * 11 + 5] = 143;
        if (k % 5 == 0) {
            map.trigger_count = 1;
            map.triggers[0] = {12, {5, 4}, {9, 9}, {10, 9}};
        }
        if (k % 4 == 0)
            map.tiles[5 * 11 + 7] = 12;
        if (k % 9 == 0)
            map.tiles[4 * 11 + 5] = 4;
        const int masks[] = {0, 0x2000, 0x800, 0x100, 0x8000, 0x200, 2, 0x400, 0x10},
                  ehps[] = {1, 25, 100};
        enemy = {};
        enemy.index = k % 10 == 0 ? 30 : k % 14 == 0 ? 45 : 20;
        enemy.name = "Rat";
        enemy.group_name = "RATS";
        enemy.strength = 15;
        enemy.dexterity = 10;
        enemy.intelligence = 15;
        enemy.armor = k % 4 * 8;
        enemy.damage = values[k % 3];
        enemy.hp = ehps[k % 3];
        enemy.max_per_map = 3;
        enemy.treasure = k % 3 * 15;
        enemy.range = k % 2 ? 4 : 1;
        enemy.abilities = uint16_t(masks[k % 9]);
        const int classes[] = {0, 1, 2, 4, 7, 8, 9, 10, 255};
        enemy.move_class = uint8_t(classes[k % 9]);
        if (k % 13 == 12) {
            for (int i = 2; i < 6; ++i) {
                g.party.characters[i] = g.party.characters[1];
                std::snprintf(g.party.characters[i].name, 9, "MEM%d", i);
            }
            g.party.character_count = 6;
            g.party.party_size = 6;
            map.start_count[2] = 6;
            for (int i = 0; i < 6; ++i)
                map.starts[2][i] = {int16_t(5 - i), 5};
            map.unit_count = 16;
            for (int i = 0; i < 16; ++i)
                map.units[i] = {int16_t(i % 11), int16_t(i / 11)};
        }
        const int limits[] = {1, 3, 8, 16, 30};
        enemy.max_per_map = limits[k % 5];
        if (k % 4 == 0)
            enemy.index = 12;
        g.position.map.location = k % 3 == 0 ? 1 : 0;
        initial = g;
        initial_turn = t;
        const CombatEnemy *defs[16];
        std::fill_n(defs, 16, &enemy);
        auto c = context();
        if (initialize_combat(c, map, CombatDirection::South, defs,
                              k % 8 == 0     ? 0
                              : k % 13 == 12 ? 16
                                             : 3,
                              k % 5 == 0) != CombatResult::Ok)
            std::abort();
    }
    V snapshot() {
        V v{s.rng.get_seed(), s.action_count,
            s.scan,           s.current < 0 ? -1 : s.actors[s.current].id,
            s.ended,          s.victory,
            combat_over(s),   s.spoil_chests,
            s.escape_border,  s.count};
        for (int i = 0; i < s.count; ++i) {
            auto &a = s.actors[i];
            v.insert(v.end(), {a.id,           a.member,        a.enemy ? a.enemy->index : -1,
                               a.position.x,   a.position.y,    a.hp,
                               a.max_hp,       a.strength,      a.dexterity,
                               a.intelligence, a.defense,       a.attack,
                               a.range,        int(a.status),   a.fleeing,
                               a.speed,        a.counter,       a.sleeping,
                               a.dragged,      a.charmed,       a.invisible,
                               a.render_tile,  a.last_attacker, a.last_target,
                               a.weapon_count});
            for (int j = 0; j < a.weapon_count; ++j) {
                auto &w = a.weapons[j];
                v.insert(v.end(), {w.id, w.attack, w.range});
            }
        }
        v.push_back(s.queue_live ? s.queue_count : -1);
        if (s.queue_live)
            for (int i = 0; i < s.queue_count; ++i) {
                auto &w = s.queue[i];
                v.insert(v.end(), {w.id, w.attack, w.range});
            }
        v.insert(v.end(), {g.party.active_character, g.food});
        for (int i = 0; i < g.party.character_count; ++i) {
            auto &r = g.party.characters[i];
            v.insert(v.end(), {r.current_hp, r.status, r.exp, r.helmet, r.armor, r.weapon, r.shield,
                               r.ring, r.amulet});
        }
        for (auto n : g.equipment_quantities)
            v.push_back(n);
        for (auto n : s.xp)
            v.push_back(n);
        for (auto n : s.map.tiles)
            v.push_back(n);
        for (auto n : s.loot)
            v.push_back(n);
        for (auto n : s.chest_contents)
            v.push_back(n);
        v.push_back(s.map.trigger_count);
        for(int i=0;i<s.map.trigger_count;++i){auto &t=s.map.triggers[i];v.insert(v.end(),{t.tile,t.at.x,t.at.y,t.first.x,t.first.y,t.second.x,t.second.y});}
        v.push_back(event_count);
        v.insert(v.end(), events.begin(), events.end());
        v.push_back(int64_t(draws.size()));
        v.insert(v.end(), draws.begin(), draws.end());
        return v;
    }
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream file(argv[1]);
    if (!file)
        return 2;
    Harness h;
    std::string line;
    V previous;
    size_t row = 0;
    while (std::getline(file, line)) {
        ++row;
        std::istringstream in(line);
        std::string tok;
        V data;
        while (in >> tok) {
            if (tok[0] == 'p') {
                auto n = size_t(std::stoul(tok.substr(1)));
                auto i = data.size();
                if (i + n > previous.size())
                    return 2;
                data.insert(data.end(), previous.begin() + i, previous.begin() + i + n);
            } else if (tok[0] == 'r') {
                auto sep = tok.find(':');
                data.insert(data.end(), size_t(std::stoul(tok.substr(1, sep - 1))),
                            std::stoll(tok.substr(sep + 1)));
            } else
                data.push_back(std::stoll(tok));
        }
        previous = data;
        int k = int(data[0]), seed = int(data[1]), op = int(data[2]), x = int(data[3]),
            y = int(data[4]);
        h.events.clear();
        h.draws.clear();
        h.event_count = 0;
        h.game_events.clear();
        h.game_event_count = 0;
        if (op == -1)
            h.init(k, seed);
        else if (op == 9) {
            h.g = h.initial;
            h.t = h.initial_turn;
            h.s = {};
            const CombatEnemy *defs[48]{};
            defs[h.enemy.index] = &h.enemy;
            const CombatMap *maps[] = {&h.map};
            CombatResources r{maps, 1, defs, 48, h.context().tables};
            auto w = h.world_context();
            if (start_encounter_combat(w, h.s, r, h.enemy.index, 5, -1, CombatDirection::South,
                                       k % 2 != 0) != CombatResult::Ok)
                std::abort();
        } else if (op == 10) {
            auto w = h.world_context();
            if (finish_encounter_combat(w, h.s) != CombatResult::Ok)
                std::abort();
        } else {
            auto c = h.context();
            if (op == 8)
                current_combat_actor(c);
            else {
                auto w = h.world_context();
                w.combat_context = &c;
                Command cmd;
                cmd.kind = CommandKind(int(CommandKind::CombatMove) + op);
                cmd.combat_x = int16_t(x);
                cmd.combat_y = int16_t(y);
                if (execute_command(w, cmd).status != CommandStatus::Success) {
                    std::cerr << "action rejected " << row;
                    return 1;
                }
            }
        }
        auto got = h.snapshot();
        if (op == 9 || op == 10) {
            got.push_back(h.g.rng.get_seed());
            got.push_back(h.game_event_count);
            got.insert(got.end(), h.game_events.begin(), h.game_events.end());
        }
        V expected(data.begin() + 5, data.end());
        if (got != expected) {
            size_t i = 0;
            while (i < got.size() && i < expected.size() && got[i] == expected[i])
                ++i;
            std::cerr << "combat row " << row << " k=" << k << " seed=" << seed << " op=" << op
                      << " field=" << i << " got=" << (i < got.size() ? got[i] : -999999)
                      << " expected=" << (i < expected.size() ? expected[i] : -999999)
                      << " lengths " << got.size() << "/" << expected.size() << "\n";
            return 1;
        }
    }
    int adapters = 0;
    auto require = [&](bool ok) {
        if (!ok) {
            std::cerr << "combat adapter " << adapters + 1 << " failed\n";
            std::exit(1);
        }
        ++adapters;
    };
    h.init(1, 123);
    auto c = h.context();
    auto before = h.snapshot();
    auto w = h.world_context();
    w.combat = false;
    CombatResources absent;
    const CombatEnemy *defs[48]{};
    defs[h.enemy.index] = &h.enemy;
    absent.enemies = defs;
    absent.enemy_count = 48;
    require(start_encounter_combat(w, h.s, absent, h.enemy.index, 5) == CombatResult::MissingMap &&
            h.snapshot() == before && h.g.rng.get_seed() == 123);
    h.enemy.abilities = 0x1000;
    const CombatEnemy *one[] = {&h.enemy};
    require(initialize_combat(c, h.map, CombatDirection::South, one, 1) ==
                CombatResult::Unsupported &&
            h.snapshot() == before);
    h.enemy.abilities = 0;
    h.map.unit_count = 17;
    require(initialize_combat(c, h.map, CombatDirection::South, one, 1) == CombatResult::Invalid &&
            h.snapshot() == before);
    h.map.unit_count = 3;
    require(initialize_combat(c, h.s.map, CombatDirection::South, one, 1) ==
                CombatResult::Invalid &&
            h.snapshot() == before);
    require(combat_action(c, CombatAction::Attack, 11, 5) == CombatResult::Invalid &&
            h.snapshot() == before);
    require(combat_action(c, CombatAction::Move, 8) == CombatResult::Invalid &&
            h.snapshot() == before);
    Command command;
    command.kind = CommandKind::CombatPass;
    require(execute_command(w, command).status == CommandStatus::InvalidContext &&
            h.snapshot() == before);
    std::cout << adapters << " combat adapter checks passed\n";
    std::cout << row << " combat parity snapshots passed; GameState=" << sizeof(GameState)
              << " CombatState=" << sizeof(CombatState) << " CombatActor=" << sizeof(CombatActor)
            << " CombatMap=" << sizeof(CombatMap) << "\n";
  std::cout << "CombatContext=" << sizeof(CombatContext) << " CombatEvent=" << sizeof(CombatEvent)
            << " GameEvent=" << sizeof(GameEvent) << " Command=" << sizeof(Command)
            << " CommandContext=" << sizeof(CommandContext) << "\n";
}
