#include "openu5/npc_path.h"
#include "openu5/pathfind.h"
#include "openu5/transitions.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace openu5;
static uint8_t tile(int mode, int x, int y) {
    if (x == 10 && y == 10)
        return 200;
    if (x == 20 && y == 20)
        return 201;
    if (mode % 6 == 1 && x == 16)
        return y == 7 ? 68 : 48;
    if (mode % 6 == 2 && (x * 17 + y * 23) % 11 < 3)
        return 48;
    if (mode % 6 == 3 && y == 12)
        return 184;
    if (mode % 6 == 4 && x == 12)
        return 144;
    if (mode % 6 == 5 && (x + y) % 3 == 0)
        return 48;
    return 68;
}
static void encode(std::vector<int> &v, const NpcActor &n) {
    const auto &s = n.schedule;
    v.insert(v.end(), {s.slot, s.type, s.dialog});
    for (auto a : {s.ai, s.x, s.y, s.z, s.times}) {
        size_t count = a == s.times ? 4 : 3;
        for (size_t j = 0; j < count; ++j)
            v.push_back(a[j]);
    }
    v.insert(v.end(), {n.x, n.y, n.z, n.location, n.state, n.served_slot, n.path_index, n.stuck});
    for (auto b : n.path)
        v.push_back(b);
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream file(argv[1]);
    if (!file)
        return 2;
    static PathScratch scratch;
    static std::array<uint8_t, 65536> tiles;
    NpcScanGrid grid;
    size_t cases = 0;
    int kind, ni, ne;
    while (file >> kind >> ni >> ne) {
        std::vector<int> input(size_t(ni), 0), expected(size_t(ne), 0), out;
        for (auto &x : input)
            file >> x;
        for (auto &x : expected)
            file >> x;
        size_t p = 0;
        auto read = [&]() { return input[p++]; };
        if (kind == 0) {
            const int mode = read(), hour = read(), px = read(), py = read(), seed = read(), count = read();
            NpcActors list;
            list.count = size_t(count);
            for (size_t j = 0; j < list.count; ++j) {
                auto &n = list.actors[j];
                auto &s = n.schedule;
                s.slot = uint8_t(read());
                s.type = uint8_t(read());
                s.dialog = uint8_t(read());
                for (auto a : {s.ai, s.x, s.y, s.z, s.times}) {
                    size_t cnt = a == s.times ? 4 : 3;
                    for (size_t k = 0; k < cnt; ++k)
                        a[k] = uint8_t(read());
                }
                n.x = int16_t(read());
                n.y = int16_t(read());
                n.z = int16_t(read());
                n.location = uint8_t(read());
                n.state = uint8_t(read());
                n.served_slot = uint8_t(read());
                n.path_index = int16_t(read());
                n.stuck = int16_t(read());
                for (auto &b : n.path)
                    b = uint8_t(read());
            }
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x)
                    tiles[size_t(y * 32 + x)] = tile(mode, x, y);
            MapData maps[4];
            size_t nmap = 0;
            for (int z = -1; z <= 2; ++z)
                if (mode != 23 || z == 0)
                    maps[nmap++] = {{2, int16_t(z)}, tiles.data(), 1024};
            WorldData world;
            world.small_maps = maps;
            world.small_map_count = nmap;
            auto m = get_active_map(world, {2, 0}).value;
            const auto floor = int16_t(read());
            const int delta = read();
            int event_count = 0;
            GameState g;
            g.position = {{uint8_t(px), uint8_t(py)}, {2, floor}};
            g.time.hour = hour;
            OriginalRng rng{uint16_t(seed)};
            if (delta)
                apply_stair_step(g, world, 196, delta > 0 ? Direction::North : Direction::South,
                                 {&event_count, nullptr,
                                  [](void *p, GameEventKind, const char *) { ++*static_cast<int *>(p); }});
            ScheduledTurnBinding binding{list, g, world, grid, rng_source(rng)};
            scheduled_npc_hook(binding)();
            if (binding.error != ActorError::None)
                return 3;
            out.insert(out.end(), {g.position.map.floor, event_count, rng.get_seed()});
            for (size_t j = 0; j < list.count; ++j)
                encode(out, list.actors[j]);
        } else if (kind == 1) {
            const auto x = int16_t(read()), y = int16_t(read());
            for (auto &b : grid)
                b = uint8_t(read());
            const auto f = npc_scan(grid, y, x);
            NpcActor n;
            size_t bytes = 0;
            if (f.found)
                bytes = npc_backtrace(n, f.y, f.x, grid);
            out = {f.found, f.x, f.y, int(bytes), n.path_index};
            for (auto b : n.path)
                out.push_back(b);
            for (auto b : grid)
                out.push_back(b);
        } else if (kind == 2) {
            const int mode = read(), seed = read(), wrap = read(), w = wrap ? 256 : 32;
            PathPoint from{int16_t(read()), int16_t(read())}, to{int16_t(read()), int16_t(read())};
            const int limit = read(), transport = read();
            for (int y = 0; y < w; ++y)
                for (int x = 0; x < w; ++x)
                    tiles[size_t(y * w + x)] = tile(mode, x % 32, y % 32);
            ActiveMap m;
            m.tiles = tiles.data();
            m.geometry = {uint16_t(w), uint16_t(w), wrap != 0};
            std::array<PathPoint, 4000> path;
            auto r = find_path(m, from, to, TransportMode(transport), scratch, path.data(), path.size(),
                               limit, {const_cast<int *>(&seed), [](void *c, int16_t x, int16_t y) {
                                           return (x * 13 + y * 19 + *static_cast<int *>(c)) % 47 == 0;
                                       }});
            out = {r.status == PathStatus::Found, int(r.count)};
            for (size_t i = 0; i < r.count; ++i) {
                out.push_back(path[i].x);
                out.push_back(path[i].y);
            }
        } else if (kind == 3) {
            int wrap = read();
            PathPoint from{int16_t(read()), int16_t(read())}, to{int16_t(read()), int16_t(read())};
            ActiveMap m;
            m.geometry = {uint16_t(wrap ? 256 : 32), uint16_t(wrap ? 256 : 32), wrap != 0};
            Direction d{};
            out = {step_direction(from, to, m, d) ? int(d) : -1};
        }
        if (kind == 4) {
            const int scenario = read(), mode = scenario % 6;
            const uint8_t locs[] = {2, 14, 29, 0}, ids[] = {2, 14, 29}, stone_locs[] = {0, 2, 255};
            const int16_t floors[] = {-1, 0, 1, 255};
            GameState g;
            g.position = {{10, 10}, {locs[(scenario / 6) % 4], floors[(scenario / 24) % 4]}};
            TurnState t;
            t.drunk_turns = 5;
            t.has_shadowlords = true;
            t.shadowlord_locations = {2, 14, 128};
            TravelState v{true, true, 2};
            MapData maps[9];
            size_t cnt = 0;
            for (auto id : ids)
                for (int z = -1; z <= 1; ++z)
                    maps[cnt++] = {{id, int16_t(z)}, tiles.data(), 1024};
            WorldData w;
            w.small_maps = maps;
            w.small_map_count = 9;
            std::array<uint8_t, 32> xs, ys;
            for (size_t i = 0; i < 32; ++i) {
                xs[i] = uint8_t(i + 40);
                ys[i] = uint8_t(i + 80);
            }
            LocationTable table{xs.data(), ys.data(), 32, 32};
            std::vector<int> trace;
            struct Capture {
                GameState &g;
                std::vector<int> &trace;
            } capture{g, trace};
            TransitionServices services{
                &capture,
                [](void *p, ReloadEffect e, uint8_t) {
                    auto &c = *static_cast<Capture *>(p);
                    const auto &pos = c.g.position;
                    c.trace.insert(c.trace.end(),
                                   {10, int(e), pos.map.location, pos.map.floor, pos.xy.x, pos.xy.y});
                },
                [](void *p, GameEventKind k, const char *text) {
                    auto &c = *static_cast<Capture *>(p);
                    if (k == GameEventKind::Message) {
                        c.trace.push_back(0);
                        c.trace.push_back(int(std::strlen(text)));
                        for (size_t i = 0; text[i]; ++i)
                            c.trace.push_back(text[i]);
                    } else
                        c.trace.push_back(int(k));
                }};
            int result = 0;
            Moonstone stone{9, 11, uint8_t(scenario % 2 ? 255 : 1), stone_locs[(scenario / 6) % 3], false};
            for (int repeat = 0; repeat < 3; ++repeat) {
                if (mode == 0)
                    apply_stair_step(g, w, 196 + (scenario / 6) % 4, Direction((scenario / 24) % 4),
                                     services);
                if (mode == 1)
                    klimb_ladder(g, w, scenario % 12 < 6 ? 1 : -1, services);
                if (mode == 2)
                    load_small_map(g, t, v, ids[(scenario / 6) % 3], "\n\nTEST\n", services);
                if (mode == 3 && exit_to_overworld(g, table, services) != Error::None)
                    result = 1;
                if (mode == 4)
                    result = moonstone_teleport(g, t, v, &stone, 1, 0, "\n\nTEST\n", services);
                if (mode == 5)
                    confirm_town_exit(g, false, table, services);
            }
            out = {result,           g.position.map.location, g.position.map.floor,    g.position.xy.x,
                   g.position.xy.y,  t.drunk_turns,           v.volatile_terrain_wipe, v.drunk_pre_rolled,
                   v.shadowlord_here};
            out.insert(out.end(), trace.begin(), trace.end());
        }
        if (kind == 5) {
            const bool wraps=read()!=0;
            Position pos{uint8_t(read()),uint8_t(read())}; const auto dir=Direction(read());
            ActiveMap map; map.kind=wraps?MapKind::Overworld:MapKind::Small;
            map.geometry={uint16_t(wraps?256:32),uint16_t(wraps?256:32),wraps};
            int events=0;
            const bool boundary=local_boundary(map,pos,dir,{&events,nullptr,[](void *p,GameEventKind k,const char *){
                if(k!=GameEventKind::TownExitPrompt)std::abort();++*static_cast<int*>(p);
            }});
            if(events!=int(boundary))return 4;
            out={boundary};
        }
        if (kind == 6) {
            const int i=read(); const uint8_t xs[]={1,2,2,4},ys[]={7,8,8};
            out={int(location_at({xs,ys,4,3},i%6,i/6))};
        }
        if (out != expected) {
            std::cerr << "Travel mismatch case " << cases << " kind " << kind << "\n";
            for (size_t i = 0; i < std::min(out.size(), expected.size()); ++i)
                if (out[i] != expected[i]) {
                    std::cerr << "field " << i << " native " << out[i] << " TS " << expected[i] << "\n";
                    break;
                }
            return 1;
        }
        ++cases;
    }
    std::cout << cases << " travel parity cases passed; scratch=" << sizeof(PathScratch)
              << " NPC grid=" << sizeof(NpcScanGrid) << " actor=" << sizeof(NpcActor)
              << " state=" << sizeof(GameState) << "\n";
}
