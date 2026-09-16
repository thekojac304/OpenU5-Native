// Reuse the physical harness's exhaustive state/event projection and map
// loader.
#define main physical_parity_main
#include "combat_parity_test.cpp"
#undef main
struct Advanced : Harness {
    CombatActor extra[2048]{};
    CombatEnemy definitions[48]{};
    const CombatEnemy *defs[48]{};
    V modes;
    CombatField fields[3]{};
    void load_maps(const char *path) {
        std::ifstream file(path);
        for (int i = 0; i < 128; ++i) {
            CombatMap m;
            file >> m.index;
            for (auto &t : m.tiles)
                file >> t;
            for (int d = 0; d < 4; ++d) {
                int n;
                file >> n;
                if (n < 0 || n > 6)
                    std::abort();
                m.start_count[d] = uint8_t(n);
                for (int j = 0; j < n; ++j)
                    file >> m.starts[d][j].x >> m.starts[d][j].y;
            }
            int n;
            file >> n;
            if (n < 0 || n > 16)
                std::abort();
            m.unit_count = uint8_t(n);
            for (int j = 0; j < n; ++j)
                file >> m.units[j].x >> m.units[j].y;
            file >> n;
            if (n < 0 || n > 8)
                std::abort();
            m.trigger_count = uint8_t(n);
            for (int j = 0; j < n; ++j) {
                auto &t = m.triggers[j];
                file >> t.tile >> t.at.x >> t.at.y >> t.first.x >> t.first.y >> t.second.x >>
                    t.second.y;
            }
            if (!file) {
                std::cerr << "map read failure " << i << "\n";
                std::exit(2);
            }
            real_maps.push_back(m);
        }
    }
    CombatContext ctx() {
        auto c = context();
        c.enemy_defs = defs;
        c.enemy_def_count = 48;
        c.events = {this, [](void *p, const CombatEvent &e) {
                        auto &h = *static_cast<Advanced *>(p);
                        h.event(e);
                        h.modes.push_back(e.mode);
                    }};
        return c;
    }
    void setup(int arena, int variant, int seed) {
        int v = variant;
        g = {};
        t = {};
        s = {};
        map = {};
        events.clear();
        draws.clear();
        modes.clear();
        event_count = 0;
        s.actors.overflow = extra;
        s.actors.overflow_capacity = 2048;
        g.party.character_count = 2;
        g.party.party_size = 2;
        g.party.active_character = 255;
        g.food = 100;
        g.karma = 75;
        g.worn_crown = v % 2 == 0;
        g.position.map.location = uint8_t(v % 17 == 0 ? 18 : 0);
        g.rng.seed(seed);
        t.transport_tile = 28;
        t.time_spell = v == 18 ? 'N' : v == 19 ? 'C' : v == 20 ? 'T' : v == 21 ? 'Q' : 0;
        t.spell_turns = 30;
        std::fill_n(g.spell_quantities, 48, v % 13 == 0 ? 0 : 5);
        std::fill_n(g.reagent_quantities, 8, 10);
        for (int i = 0; i < 2; ++i) {
            auto &p = g.party.characters[i];
            std::snprintf(p.name, 9, "MEM%d", i);
            p.character_class = 'A';
            p.status = i == 1 ? "GPSD"[v % 4] : 'G';
            p.current_hp = uint16_t(v % 9 == 0 ? 100 : 50);
            p.max_hp = 100;
            p.current_mp = uint8_t(v % 7 == 0 ? 0 : 30);
            p.level = uint8_t(v % 11 == 0 ? 1 : 8);
            p.strength = 20;
            p.dexterity = 30;
            p.intelligence = uint8_t(v % 3 == 0 ? 1 : 30);
            p.exp = 1000;
            p.helmet = p.armor = p.weapon = p.shield = p.ring = 255;
            p.amulet = uint8_t(v % 2 == 0 ? 45 : 255);
            p.party_status = 0;
        }
        const int flags[] = {0,     0x1000, 0x4000, 8,      4, 0x20, 0x80, 0x2000,
                             0x800, 0x402,  0x402c, 0x1080, 0, 0,    0,    0};
        for (int i = 0; i < 48; ++i) {
            auto &d = definitions[i];
            d = {};
            d.index = i;
            d.name = "Enemy";
            d.group_name = "ENEMIES";
            d.strength = 15;
            d.dexterity = 10;
            d.intelligence = v % 3 == 0 ? 30 : 1;
            d.armor = v % 5 == 0 ? 20 : 0;
            d.damage = 10;
            d.hp = v % 5 == 0 ? 10 : 100;
            d.max_per_map = 3;
            d.treasure = 10;
            d.range = 5;
            d.abilities = uint16_t(i == 20 ? flags[v % 16] : 0);
            defs[i] = &d;
        }
        std::fill_n(map.tiles, 121, int16_t(v == 22 ? 12 : 5));
        for (int d = 0; d < 4; ++d) {
            map.start_count[d] = 2;
            map.starts[d][0] = {2, 2};
            map.starts[d][1] = {3, 3};
        }
        map.unit_count = 16;
        for (int i = 0; i < 16; ++i)
            map.units[i] = {int16_t(4 + i % 5), int16_t(2 + i / 5)};
        if (v == 23)
            map.tiles[2 * 11 + 3] = 12;
        if (v == 24)
            map.tiles[2 * 11 + 2] = 143;
        if (!real_maps.empty())
            map = real_maps.at(size_t(arena / 4));
        const CombatEnemy *group[16];
        std::fill_n(group, 16, &definitions[20]);
        auto c = ctx();
        auto result = initialize_combat(c, map, CombatDirection(arena % 4), group, v == 25 ? 16 : 3,
                                        !real_maps.empty() && arena / 4 >= 16);
        if (result != CombatResult::Ok) {
            std::cerr << "init failed arena " << arena << " status " << int(result) << "\n";
            std::exit(2);
        }
        s.current = 0;
        s.actors[0].counter = 6;
        if (real_maps.empty() && v >= 24 && v <= 27) {
            fields[0] = {{2, 2}, int16_t(232 + v - 24)};
            fields[1] = {{2, 2}, 233};
            fields[2] = {{4, 2}, 235};
            s.fields = fields;
            s.field_count = 3;
        }
        if (v == 26)
            s.actors[2].sleeping = true;
        if (v == 27)
            s.actors[2].fleeing = true;
        if (v == 28)
            s.actors[2].invisible = true;
        if (v == 29)
            s.actors[2].charmed = true;
        if (v == 30) {
            s.actors[2].hp = 0;
            s.actors[2].status = CombatStatus::Dead;
        }
        const int immune[] = {47, 14, 15};
        if (v >= 32 && v <= 34)
            s.actors[2].enemy = defs[immune[v - 32]];
        if (v == 35)
            for (int i = 0; i < s.count; ++i)
                if (s.actors[i].enemy)
                    s.actors[i].hp = 1;
        if (v == 36)
            for (int i = 0; i < 300; ++i) {
                auto &a = s.actors[s.count++];
                a = s.actors[2];
                a.id = s.count;
            }
        if (v == 37)
            for (int y = 0; y < 11; ++y)
                for (int x = 0; x < 11; ++x) {
                    auto &a = s.actors[s.count++];
                    a = s.actors[2];
                    a.id = s.count;
                    a.position = {int16_t(x), int16_t(y)};
                }
    }
    V snap() {
        auto v = snapshot();
        for (int i = 0; i < 2; ++i) {
            auto &p = g.party.characters[i];
            v.insert(v.end(), {p.current_mp, p.max_hp, p.level, p.intelligence});
        }
        v.insert(v.end(), std::begin(g.spell_quantities), std::end(g.spell_quantities));
        v.insert(v.end(), std::begin(g.reagent_quantities), std::end(g.reagent_quantities));
        v.push_back(t.time_spell);
        v.push_back(t.spell_turns);
        v.insert(v.end(), modes.begin(), modes.end());
        v.push_back(s.field_count);
        for (int i = 0; i < s.field_count; ++i)
            v.insert(v.end(), {s.fields[i].position.x, s.fields[i].position.y, s.fields[i].tile});
        return v;
    }
};
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    static Advanced h;
    if (argc > 2)
        h.load_maps(argv[2]);
    std::ifstream file(argv[1]);
    std::string line;
    V previous;
    int row = 0;
    while (std::getline(file, line)) {
        V data;
        std::istringstream in(line);
        std::string token;
        while (in >> token) {
            if (token[0] == 'p') {
                int n = std::stoi(token.substr(1));
                auto off = data.size();
                for (int i = 0; i < n; ++i)
                    data.push_back(previous.at(off + size_t(i)));
            } else if (token[0] == 'r') {
                auto colon = token.find(':');
                int n = std::stoi(token.substr(1, colon - 1));
                auto value = std::stoll(token.substr(colon + 1));
                for (int i = 0; i < n; ++i)
                    data.push_back(value);
            } else
                data.push_back(std::stoll(token));
        }
        previous = data;
        ++row;
        int arena = int(data[0]), spell = int(data[1]), v = int(data[2]), seed = int(data[3]),
            op = int(data[4]), x = int(data[5]), y = int(data[6]);
        if (argc > 3)
            std::cerr << "row " << row << " arena " << arena << " spell " << spell << " v " << v
                      << " op " << op << "\n";
        h.events.clear();
        h.draws.clear();
        h.modes.clear();
        h.event_count = 0;
        if (op == -1)
            h.setup(arena, v, seed);
        else if(op==6) {
            h.game_events.clear();h.game_event_count=0;
            auto w=h.world_context();
            if(finish_encounter_combat(w,h.s)!=CombatResult::Ok)return 4;
        }
        else {
            auto c = h.ctx();
            if (op == 0)
                h.s.current = 0;
            if (op == 4)
                h.s.current = h.s.count > 2 ? 2 : -1;
            auto *a = current_combat_actor(c);
            CombatResult result;
            if (op == 0) {
                auto w = h.world_context();
                w.combat_context = &c;
                w.events = {&h, [](void *p, const GameEvent &e) {
                                auto &h = *static_cast<Advanced *>(p);
                                if (e.kind != GameEventKind::Combat)
                                    std::abort();
                                h.event(*e.combat);
                                h.modes.push_back(e.combat->mode);
                            }};
                Command cmd;
                cmd.kind = CommandKind::Cast;
                cmd.item = int16_t(spell);
                cmd.member = 1;
                cmd.has_target = x >= 0;
                cmd.combat_x = int16_t(x);
                cmd.combat_y = int16_t(y);
                cmd.cancel_target = v == 31;
                result = execute_command(w, cmd).status == CommandStatus::Success
                             ? CombatResult::Ok
                             : CombatResult::Invalid;
            } else if (op == 3)
                result = combat_action(c, CombatAction::EscapeQuick);
            else if (op == 4)
                result =
                    combat_action(c, a && (a->member == 255 || a->charmed) ? CombatAction::EnemyStep
                                                                           : CombatAction::Pass);
            else
                result = combat_action(c, op == 1 ? CombatAction::EnemyStep : CombatAction::Pass);
            if (result != CombatResult::Ok) {
                std::cerr << "rejected row " << row << " status " << int(result) << "\n";
                return 1;
            }
        }
        auto got = h.snap();
        if(op==6){got.push_back(h.g.rng.get_seed());got.push_back(h.game_event_count);got.insert(got.end(),h.game_events.begin(),h.game_events.end());}
        V expected(data.begin() + 7, data.end());
        if (got != expected) {
            size_t i = 0;
            while (i < got.size() && i < expected.size() && got[i] == expected[i])
                ++i;
            std::cerr << "advanced row " << row << " arena " << arena << " spell " << spell << " v "
                      << v << " seed " << seed << " op " << op << " field " << i << " got "
                      << (i < got.size() ? got[i] : -999999) << " expected "
                      << (i < expected.size() ? expected[i] : -999999) << " lengths " << got.size()
                      << "/" << expected.size() << "\n";
            return 1;
        }
    }
    if (!row)
        return 2;
    h.real_maps.clear();
    h.setup(0, 1, 123);
    h.events.clear();
    h.draws.clear();
    h.modes.clear();
    h.event_count = 0;
    auto c = h.ctx();
    h.s.actors.overflow_capacity = 0;
    auto before = h.snap();
    if (combat_cast(c, SpellId::InBetXen) != CombatResult::NeedsActorStorage || h.snap() != before)
        return 3;
    if (combat_action(c, CombatAction::EnemyStep) != CombatResult::NeedsActorStorage ||
        h.snap() != before)
        return 3;
    h.s.actors.overflow_capacity = 2048;
    if (combat_cast(c, SpellId(255)) != CombatResult::Invalid || h.snap() != before)
        return 3;
    std::cout << row
              << " advanced combat snapshots passed; 3 capacity/invalid-input "
                 "adapter checks passed\n";
    return 0;
}
