#include "openu5/npc_path.h"
#include <algorithm>
#include <cstdlib>
namespace openu5 {
static int16_t nz(uint8_t z) { return z == 255 ? int16_t(-1) : int16_t(z); }
// Out-of-array Uint8Array reads in TS fail the scanner's <16 check.
static uint8_t cell(const NpcScanGrid &g, int32_t x, int32_t y) {
    const int32_t i = y * 32 + x;
    return i >= 0 && i < int32_t(g.size()) ? g[size_t(i)] : uint8_t(144);
}
static void put(NpcScanGrid &g, int32_t x, int32_t y, uint8_t v) {
    const int32_t i = y * 32 + x;
    if (i >= 0 && i < int32_t(g.size()))
        g[size_t(i)] = v;
}
ScanPoint npc_scan(NpcScanGrid &g, int16_t sy, int16_t sx) {
    std::array<uint8_t, 32> qx{}, qy{};
    qx[0] = uint8_t(sx);
    qy[0] = uint8_t(sy);
    size_t read = 0, write = 1;
    for (;;) {
        const int16_t cx = qx[read], cy = qy[read];
        int32_t dir = cell(g, cx, cy) >> 4;
        for (int i = 0; i < 4; ++i) {
            int16_t x = cx, y = cy;
            bool oob = false;
            switch (dir) {
            case 1:
                --x;
                oob = x < 0;
                break;
            case 2:
                ++y;
                oob = y > 32;
                break;
            case 3:
                ++x;
                oob = x > 32;
                break;
            case 4:
                --y;
                oob = y < 0;
                break;
            default:
                break;
            }
            if (!oob) {
                const auto c = cell(g, x, y);
                if (c < 16) {
                    put(g, x, y, uint8_t(dir << 4));
                    if ((c & 15) == 5)
                        return {x, y, true};
                    if (write != read) {
                        qx[write] = uint8_t(x);
                        qy[write] = uint8_t(y);
                        write = (write + 1) % 32;
                    }
                }
            }
            dir = (dir & 3) + 1;
        }
        read = (read + 1) % 32;
        if (read == write)
            return {};
    }
}
size_t npc_backtrace(NpcActor &n, int16_t y, int16_t x, const NpcScanGrid &g) {
    n.path_index = 0;
    auto c = cell(g, x, y);
    int32_t di = c & 15, si = c >> 4, run_dir = si, run = 0;
    size_t count = 0;
    for (;;) {
        switch (si) {
        case 1:
            ++x;
            break;
        case 2:
            --y;
            break;
        case 3:
            --x;
            break;
        case 4:
            ++y;
            break;
        default:
            break;
        }
        if (run_dir == si && di != 6)
            ++run;
        if (run_dir != si || di == 6) {
            n.path[count++] = uint8_t(run);
            n.path[count++] = uint8_t(run_dir);
            if (di == 6)
                break;
            run_dir = si;
            run = 1;
        }
        c = cell(g, x, y);
        si = c >> 4;
        di = c & 15;
        if (count >= 32)
            break;
    }
    for (size_t i = 0, j = count - 2; j >= i; i += 2) {
        std::swap(n.path[i], n.path[j]);
        const auto a = uint8_t(((n.path[i + 1] + 1) & 3) + 1), b = uint8_t(((n.path[j + 1] + 1) & 3) + 1);
        n.path[i + 1] = b;
        n.path[j + 1] = a;
        if (j < 2)
            break;
        j -= 2;
    }
    return count;
}
static int32_t tile_at(const ActiveMap &m, int16_t x, int16_t y) {
    const auto t = m.tile_at(x, y);
    return t == 184 || t == 186 ? 68 : t;
}
static int walkable(const NpcActor &n, uint8_t i, int16_t x, int16_t y, int16_t floor, int32_t t) {
    if (n.schedule.x[i] == x && n.schedule.y[i] == y && nz(n.schedule.z[i]) == floor)
        return 2;
    if (n.state == 3 && (t == 200 || t == 201))
        return 1;
    return is_passable(t, TransportMode::Foot).value ? 1 : 0;
}
static void raster(NpcScanGrid &grid, const NpcActor &n, uint8_t idx, int sel, int16_t ty, int16_t tx,
                   int16_t sy, int16_t sx, const GameState &g, const ActiveMap &m, const NpcActors &list) {
    grid.fill(144);
    const auto z = g.position.map.floor;
    for (int16_t y = 0; y < 32; ++y)
        for (int16_t x = 0; x < 32; ++x) {
            auto t = tile_at(m, x, y);
            put(grid, x, y, walkable(n, idx, x, y, z, t) ? 0 : 144);
            if (sel < 0 && t == (sel == -1 ? 200 : 201))
                put(grid, x, y, 5);
        }
    for (size_t j = 0; j < list.count; ++j) {
        const auto &o = list.actors[j];
        if (o.schedule.slot != n.schedule.slot && o.z == z && std::abs(n.x - o.x) + std::abs(n.y - o.y) < 4)
            put(grid, o.x, o.y, 144);
    }
    const auto &p = g.position;
    if (p.map.location == n.location && std::abs(n.x - p.xy.x) + std::abs(n.y - p.xy.y) < 4)
        put(grid, p.xy.x, p.xy.y, 144);
    if (sel >= 0)
        put(grid, tx, ty, 5);
    put(grid, sx, sy, 70);
}
static void wander(NpcActor &n, NpcActors &list, const GameState &g, const ActiveMap &map, Rand r) {
    if ((r(0, 255) & 8) == 0)
        return;
    int16_t x = n.x, y = n.y;
    const auto d = (r(0, 64) & 3) + 1;
    switch (d) {
    case 1:
        ++x;
        if (y > 32)
            x = 32;
        break;
    case 2:
        --y;
        if (x < 0)
            y = 0;
        break;
    case 3:
        --x;
        if (y < 0)
            x = 0;
        break;
    default:
        ++y;
        if (x > 32)
            y = 32;
        break;
    }
    if (x < 0 || y < 0 || x >= 32 || y >= 32 || !is_passable(map.tile_at(x, y), TransportMode::Foot).value ||
        npc_occupied(list, g.position, n.location, n.z, x, y, n.schedule.slot))
        return;
    n.x = x;
    n.y = y;
}
static void follow(NpcActor &n, uint8_t idx, NpcActors &list, const GameState &g, const ActiveMap &m,
                   Rand r) {
    int16_t x = n.x, y = n.y;
    const auto d = n.path[size_t(n.path_index + 1)];
    switch (d) {
    case 1:
        ++x;
        if (y > 32)
            x = 32;
        break;
    case 2:
        --y;
        if (x < 0)
            y = 0;
        break;
    case 3:
        --x;
        if (y < 0)
            x = 0;
        break;
    default:
        ++y;
        if (x > 32)
            y = 32;
        break;
    }
    int w = 0;
    if (x >= 0 && y >= 0 && x < 32 && y < 32) {
        const auto t = tile_at(m, x, y), group = t & 252;
        w = group == 48                    ? 1
            : group == 144 && n.state != 2 ? 0
                                           : walkable(n, idx, x, y, g.position.map.floor, t);
        if (npc_occupied(list, g.position, n.location, g.position.map.floor, x, y, n.schedule.slot))
            w = 0;
    }
    if (!w) {
        ++n.stuck;
        wander(n, list, g, m, r);
        if (n.stuck > 3) {
            n.path_index = -1;
            n.stuck = 0;
        }
        return;
    }
    n.x = x;
    n.y = y;
    --n.path[size_t(n.path_index)];
    n.stuck = 0;
    if (n.path[size_t(n.path_index)])
        return;
    n.path[size_t(n.path_index + 1)] = 0;
    n.path_index += 2;
    if (n.path_index >= 32 || n.path[size_t(n.path_index)] < 1)
        n.path_index = -1;
    if (w == 2) {
        n.served_slot = idx;
        n.state = 1;
        n.path_index = -1;
    }
}
ActorError tick_npcs(NpcActors &list, const GameState &g, const NpcTravelContext &ctx, Rand r) {
    if (!g.position.map.location || !list.count)
        return ActorError::None;
    if (list.count > 32)
        return ActorError::Capacity;
    const auto &m = ctx.map;
    auto &grid = ctx.scratch;
    if (!m.tiles || m.geometry.width != 32 || m.geometry.height != 32 ||
        m.id.location != g.position.map.location || m.id.floor != g.position.map.floor)
        return ActorError::InvalidMap;
    std::array<size_t, 32> order{};
    for (size_t i = 0; i < list.count; ++i) {
        if (list.actors[i].path_index < -1 || list.actors[i].path_index > 30 ||
            (list.actors[i].path_index >= 0 && list.actors[i].path_index % 2) ||
            list.actors[i].served_slot > 2)
            return ActorError::Capacity;
        order[i] = i;
    }
    for (size_t i = 1; i < list.count; ++i) {
        const auto v = order[i];
        size_t j = i;
        while (j && list.actors[order[j - 1]].schedule.slot > list.actors[v].schedule.slot) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = v;
    }
    bool budget = false;
    for (size_t j = 0; j < list.count; ++j) {
        auto &n = list.actors[order[j]];
        const auto idx = schedule_index(n.schedule.times, uint8_t(g.time.hour));
        auto scan = [&](int sel, int16_t ty, int16_t tx, int16_t sy, int16_t sx) {
            raster(grid, n, idx, sel, ty, tx, sy, sx, g, m, list);
            return npc_scan(grid, sy, sx);
        };
        if (n.state <= 1 && !npc_check_schedule(n, uint8_t(g.time.hour), g.position.map.floor)) {
            npc_ai_step(n, idx, list, g, m, r);
            continue;
        }
        if (n.state <= 3) {
            if (n.path_index >= 0 && n.path[size_t(n.path_index)] != 0) {
                follow(n, idx, list, g, m, r);
                continue;
            }
            if (n.path_index == -1 && n.state == 3) {
                n.state = nz(n.schedule.z[idx]) > g.position.map.floor ? 6 : 7;
                return ActorError::None;
            }
            if (budget) {
                npc_ai_step(n, n.served_slot, list, g, m, r);
                continue;
            }
            if (n.state == 1)
                continue;
            if (n.stuck < 200 && (n.stuck == 0 || r(0, 2) == 1)) {
                if (n.path_index == -1) {
                    budget = true;
                    if (scan(0, n.schedule.y[idx], n.schedule.x[idx], n.y, n.x).found) {
                        npc_backtrace(n, n.schedule.y[idx], n.schedule.x[idx], grid);
                        n.stuck = 0;
                        continue;
                    }
                    n.stuck = 200;
                }
                wander(n, list, g, m, r);
                continue;
            }
            if (n.stuck >= 200)
                ++n.stuck;
            if (n.stuck > 204)
                n.stuck = 0;
            continue;
        }
        if (n.state == 4 || n.state == 5) {
            if (budget)
                continue;
            budget = true;
            const auto mode = n.state == 4 ? 3 : 4;
            const auto stairs = scan(mode == 3 ? -1 : -2, 0, 0, n.schedule.y[idx], n.schedule.x[idx]);
            if (!stairs.found)
                continue;
            if (!scan(mode, n.schedule.y[idx], n.schedule.x[idx], stairs.y, stairs.x).found)
                continue;
            npc_backtrace(n, n.schedule.y[idx], n.schedule.x[idx], grid);
            const auto t = tile_at(m, stairs.x, stairs.y);
            if ((mode == 3 && t == 200) || (mode == 4 && t == 201) || (t & 252) == 196) {
                n.x = stairs.x;
                n.y = stairs.y;
                n.z = g.position.map.floor;
            }
            n.state = 2;
            continue;
        }
        const auto t = tile_at(m, n.x, n.y);
        const auto dest = nz(n.schedule.z[idx]);
        if (n.state == 8 ||
            (dest < g.position.map.floor ? t == 201 || (t & 244) == 196 : t == 200 || (t & 244) == 196)) {
            for (size_t i = 0; i < ctx.world.small_map_count; ++i)
                if (ctx.world.small_maps[i].id.location == n.location &&
                    ctx.world.small_maps[i].id.floor == dest) {
                    n.x = n.schedule.x[idx];
                    n.y = n.schedule.y[idx];
                    n.z = dest;
                    break;
                }
            n.served_slot = idx;
            n.path_index = -1;
            n.state = 1;
            continue;
        }
        if (budget)
            continue;
        budget = true;
        const auto stairs = scan(n.state == 6 ? -1 : -2, 0, 0, n.y, n.x);
        if (!stairs.found)
            continue;
        if (scan(n.state == 6 ? 1 : 2, stairs.y, stairs.x, n.y, n.x).found) {
            npc_backtrace(n, stairs.y, stairs.x, grid);
            n.state = 3;
        }
    }
    return ActorError::None;
}
} // namespace openu5
