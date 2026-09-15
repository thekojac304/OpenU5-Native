#include "openu5/commands.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace openu5;
using V = std::vector<int64_t>;
static void check(const V &a, const V &b, int row, const char *label) {
    if (a == b)
        return;
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i])
        ++i;
    std::cerr << label << " row " << row << " field " << i << " actual " << (i < a.size() ? a[i] : -999)
              << " expected " << (i < b.size() ? b[i] : -999) << " sizes " << a.size() << '/' << b.size()
              << '\n';
    std::exit(1);
}
static void encode(V &v, const NpcActor &n) {
    const auto &s = n.schedule;
    v.insert(v.end(), {s.slot, s.type, s.dialog});
    for (auto a : {s.ai, s.x, s.y, s.z})
        for (int i = 0; i < 3; ++i)
            v.push_back(a[i]);
    for (auto x : s.times)
        v.push_back(x);
    v.insert(v.end(), {n.x, n.y, n.z, n.location, n.state, n.served_slot, n.path_index, n.stuck});
    for (auto x : n.path)
        v.push_back(x);
}
struct Harness {
    GameState g{};
    TurnState t{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    NpcScanGrid scratch{};
    std::vector<uint8_t> large, small;
    MapData maps[3]{};
    WorldData world{};
    uint8_t lx[2]{40, 10}, ly[2]{80, 10};
    V events, effects, draws;
    NpcSlot slots[2]{{1, 64, 0, {0, 1, 0}, {11, 18, 8}, {10, 18, 8}, {0, 1, 0}, {0, 6, 12, 18}},
                     {2, 16, 0, {0, 0, 0}, {8, 8, 8}, {8, 8, 8}, {0, 0, 0}, {0, 6, 12, 18}}};
    CommandContext context{g, t, travel, commands, world};
    NpcLocationData npc_data{2, slots, 2, 0};
    Harness() {
        context.locations = {lx, ly, 2, 2};
        context.actors = &actors;
        context.npc_scratch = &scratch;
        context.npc_data = &npc_data;
        context.npc_data_count = 1;
        context.events = {this, [](void *p, const GameEvent &e) {
                              auto &h = *static_cast<Harness *>(p);
                              auto &v = h.events;
                              v.push_back(int(e.kind));
                              const auto len = e.text ? std::strlen(e.text) : 0;
                              v.push_back(int64_t(len));
                              for (size_t i = 0; i < len; ++i)
                                  v.push_back(uint8_t(e.text[i]));
                              v.push_back(e.slot_count);
                              for (uint8_t i = 0; i < e.slot_count; ++i)
                                  v.push_back(e.slots[i]);
                          }};
        context.services = {this,
                            [](void *p, CommandEffect e, EventSink) {
                                static_cast<Harness *>(p)->effect(int(e));
                                return false;
                            },
            [](void *p, ReloadEffect e, uint8_t, EventSink) {
                                auto &h = *static_cast<Harness *>(p);
                                h.effect(20 + int(e));
                            },
                            [](void *, uint8_t) { return "\n\nTEST\n"; }};
        context.rng_trace = {this, [](void *p, const char *, int32_t lo, int32_t hi, int32_t value) {
                                 auto &v = static_cast<Harness *>(p)->draws;
                                 v.insert(v.end(), {lo, hi, value});
                             }};
    }
    void effect(int id) {
        effects.insert(effects.end(), {id, g.position.map.location, g.position.map.floor, g.position.xy.x,
                                       g.position.xy.y, g.rng.get_seed()});
    }
    void initialize(int scenario, const V &v) {
        g = {};
        t = {};
        travel = {};
        commands = {};
        actors = {};
        context.combat = false;
        context.dungeon = false;
        const bool local = (scenario >= 8 && scenario <= 18) || scenario == 21 ||
                           (scenario >= 25 && scenario <= 27) || scenario >= 29;
        const uint8_t terrain[] = {5,   48,  4,   9,   47, 4,  143, 5,  68, 48, 68, 196,
                                   200, 201, 134, 202, 68, 68, 68,  20, 26, 68, 5,  20,
                                   20,  68,  68,  68,  5,  68, 188, 68, 68, 68, 68, 68};
        large.assign(65536, terrain[scenario]);
        small.assign(1024, local ? terrain[scenario] : 68);
        if (scenario == 1 || scenario == 4)
            large[10 * 256 + 10] = 5;
        if (scenario == 9 || scenario == 15)
            small[10 * 32 + 10] = 68;
        for (int i = 0; i < 3; ++i)
            maps[i] = {{2, int16_t(i - 1)}, small.data(), small.size()};
        world = {large.data(), large.data(), large.size(), large.size(), maps, 3};
        lx[1] = (scenario == 23 || scenario == 24) ? 41 : 10;
        lx[0] = scenario == 24 ? 10 : 40;
        ly[0] = scenario == 24 ? 10 : 80;
        size_t i = 0;
        auto next = [&]() { return v.at(i++); };
        g.time.year = int32_t(next());
        g.time.month = int32_t(next());
        g.time.day = int32_t(next());
        g.time.hour = int32_t(next());
        g.time.minute = int32_t(next());
        g.position.map.location = uint8_t(next());
        g.position.map.floor = int16_t(next());
        g.position.xy.x = uint8_t(next());
        g.position.xy.y = uint8_t(next());
        g.party.party_size = int32_t(next());
        g.party.active_character = uint8_t(next());
        g.food = uint16_t(next());
        g.torch_turns = uint16_t(next());
        g.turns_since_start = next();
        t.prev_hour = int32_t(next());
        t.light_spell_minutes = int32_t(next());
        t.drunk_turns = int32_t(next());
        t.time_spell = char(next());
        t.spell_turns = int32_t(next());
        t.transport_tile = int32_t(next());
        t.wind = int32_t(next());
        t.wind_drift_counter = int32_t(next());
        t.has_shadowlords = true;
        for (auto &x : t.shadowlord_locations)
            x = int32_t(next());
        t.skull_tree_day = int32_t(next());
        for (auto &x : t.reagent_days)
            x = int32_t(next());
        t.felucca_phase = int32_t(next());
        t.trammel_phase = int32_t(next());
        g.party.character_count = 3;
        for (int j = 0; j < 3; ++j) {
            auto &ch = g.party.characters[j];
            ch.status = char(next());
            ch.current_hp = uint16_t(next());
            ch.max_hp = uint16_t(next());
            ch.ring = uint8_t(next());
            ch.strength = uint8_t(next());
            ch.dexterity = uint8_t(next());
            ch.months_at_inn = uint8_t(next());
            ch.party_status = 0;
        }
        g.rng.seed(int32_t(next()));
        commands.town_location = int16_t(next());
        commands.town_phases.mount = int32_t(next());
        commands.town_phases.quickness = int32_t(next());
        commands.outdoor_phases.mount = int32_t(next());
        commands.outdoor_phases.quickness = int32_t(next());
        travel.drunk_pre_rolled = next() != 0;
        commands.awaiting_exit = next() != 0;
        travel.shadowlord_here = int8_t(next());
        actors.count = size_t(next());
        for (size_t j = 0; j < actors.count; ++j) {
            auto &n = actors.actors[j];
            auto &s = n.schedule;
            s.slot = uint8_t(next());
            s.type = uint8_t(next());
            s.dialog = uint8_t(next());
            for (auto a : {s.ai, s.x, s.y, s.z})
                for (int k = 0; k < 3; ++k)
                    a[k] = uint8_t(next());
            for (auto &x : s.times)
                x = uint8_t(next());
            n.x = int16_t(next());
            n.y = int16_t(next());
            n.z = int16_t(next());
            n.location = uint8_t(next());
            n.state = uint8_t(next());
            n.served_slot = uint8_t(next());
            n.path_index = int16_t(next());
            n.stuck = int16_t(next());
            for (auto &x : n.path)
                x = uint8_t(next());
        }
        if (i != v.size())
            std::abort();
    }
    V state() {
        V v{g.time.year,
            g.time.month,
            g.time.day,
            g.time.hour,
            g.time.minute,
            g.position.map.location,
            g.position.map.floor,
            g.position.xy.x,
            g.position.xy.y,
            g.party.party_size,
            g.party.active_character,
            g.food,
            g.torch_turns,
            g.turns_since_start,
            t.prev_hour,
            t.light_spell_minutes,
            t.drunk_turns,
            t.time_spell,
            t.spell_turns,
            t.transport_tile,
            t.wind,
            t.wind_drift_counter};
        for (auto x : t.shadowlord_locations)
            v.push_back(x);
        v.push_back(t.skull_tree_day);
        for (auto x : t.reagent_days)
            v.push_back(x);
        v.insert(v.end(), {t.felucca_phase, t.trammel_phase});
        for (int i = 0; i < 3; ++i) {
            const auto &ch = g.party.characters[i];
            v.insert(v.end(), {ch.status, ch.current_hp, ch.max_hp, ch.ring, ch.strength, ch.dexterity,
                               ch.months_at_inn});
        }
        v.insert(v.end(), {g.rng.get_seed(), commands.town_location, commands.town_phases.mount,
                           commands.town_phases.quickness, commands.outdoor_phases.mount,
                           commands.outdoor_phases.quickness, travel.drunk_pre_rolled, commands.awaiting_exit,
                           travel.shadowlord_here, int64_t(actors.count)});
        for (size_t i = 0; i < actors.count; ++i)
            encode(v, actors.actors[i]);
        return v;
    }
    V run(Command cmd, bool dispatch = false) {
        events.clear();
        effects.clear();
        draws.clear();
        const auto r = dispatch ? dispatch_world_command(context, cmd) : execute_command(context, cmd);
        if (r.error != Error::None || r.actor_error != ActorError::None)
            std::abort();
        const auto s = state();
        V v{int(r.status), r.turns, r.world_turns, r.event_count, int64_t(s.size())};
        v.insert(v.end(), s.begin(), s.end());
        for (const auto *a : {&events, &effects, &draws}) {
            v.push_back(int64_t(a->size()));
            v.insert(v.end(), a->begin(), a->end());
        }
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
    int scenario, seed, step, cmd, dir, has, row = 0;
    size_t n;
    while (file >> scenario >> seed >> step >> cmd >> dir >> has >> n) {
        V before(n);
        for (auto &x : before)
            file >> x;
        file >> n;
        V expected(n);
        for (auto &x : expected)
            file >> x;
        if (!file)
            return 2;
        if (step == 0)
            h.initialize(scenario, before);
        check(h.state(), before, row, "before");
        check(h.run({CommandKind(cmd), Direction(dir), has != 0}, scenario >= 32), expected, row, "after");
        ++row;
    }
    // Native adapter errors have no TS Game return equivalent: assert atomic
    // rejection.
    const auto before = h.state();
    for (int mode = 0; mode < 7; ++mode) {
        h.context.combat = mode == 0;
        h.context.dungeon = mode == 1;
        h.g.transport = mode == 2 ? TransportMode::Ship : TransportMode::Foot;
        h.events.clear();
        h.effects.clear();
        h.draws.clear();
        Command c{mode == 3   ? CommandKind::Exit
                  : mode == 4 ? CommandKind(255)
                              : CommandKind::Move,
                  mode == 5 ? Direction(255) : Direction::East};
        if (mode == 6)
            h.context.npc_scratch = nullptr;
        const auto r = execute_command(h.context, c);
        if (r.status != CommandStatus::InvalidContext && r.status != CommandStatus::Unsupported)
            return 3;
        check(h.state(), before, row, "atomic rejection");
        if (!h.events.empty() || !h.effects.empty() || !h.draws.empty())
            return 4;
    }
    h.context.combat = h.context.dungeon = false;
    h.context.npc_scratch = &h.scratch;
    h.g.transport = TransportMode::Foot;
    std::vector<uint8_t> trapdoor_floor(1024, 140);
    for (int mode = 0; mode < 6; ++mode) {
        h.g.position = {{10, 10}, {2, 0}};
        h.commands.awaiting_exit = false;
        h.small.assign(1024, 68);
        h.large.assign(65536, 5);
        for (auto &m : h.maps)
            m.tiles = h.small.data();
        h.world.overworld = h.world.underworld = h.large.data();
        Command cmd{CommandKind::Move, Direction::East};
        CommandStatus status = CommandStatus::Unsupported;
        if (mode == 0) {
            h.g.position.map.location = 0;
            h.large[10 * 256 + 11] = 106;
        }
        if (mode == 1) {
            h.small[10 * 32 + 10] = 140;
            cmd.kind = CommandKind::Pass;
        }
        if (mode == 2) {
            h.small[10 * 32 + 10] = 200;
            h.maps[2].tiles = trapdoor_floor.data();
            cmd.kind = CommandKind::Klimb;
        }
        if (mode == 3) {
            h.g.position.map.location = 0;
            h.large[10 * 256 + 10] = 17;
            cmd.kind = CommandKind::Enter;
        }
        if (mode == 4) {
            h.commands.awaiting_exit = true;
            status = CommandStatus::InvalidContext;
        }
        if (mode == 5) {
            h.g.position.map.location = 99;
            status = CommandStatus::CoreError;
        }
        const auto snapshot = h.state();
        h.events.clear();
        h.effects.clear();
        h.draws.clear();
        const auto r = dispatch_world_command(h.context, cmd);
        if (r.status != status)
            return 5;
        check(h.state(), snapshot, row, "unsupported atomicity");
        if (!h.events.empty() || !h.effects.empty() || !h.draws.empty())
            return 6;
    }
    std::cout << row << " command parity cases; 13 atomic adapter checks\n";
    std::cout << "sizes GameState=" << sizeof(GameState) << " Command=" << sizeof(Command)
              << " ActionResult=" << sizeof(ActionResult) << " GameEvent=" << sizeof(GameEvent)
              << " CommandState=" << sizeof(CommandState) << " CommandContext=" << sizeof(CommandContext)
              << " legacy CommandResult=" << sizeof(CommandResult) << '\n';
}
