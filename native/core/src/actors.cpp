#include "openu5/actors.h"
#include "openu5/npc_path.h"
#include <algorithm>
#include <cstdlib>
namespace openu5 {
static int16_t norm_z(uint8_t z) { return z == 255 ? int16_t(-1) : int16_t(z); }
static bool object_type(uint8_t t) { return t == 1 || t == 30 || t == 27 || t == 14 || t == 181 || t == 182; }
ActorError enter_npc_map(NpcActors &out, const NpcSlot *slots, size_t count, uint8_t location, uint8_t hour, uint32_t dead) {
    if (count > 32) return ActorError::Capacity;
    NpcActors next;
    for (size_t i = 0; i < count; ++i) {
        const auto &s = slots[i];
        if (s.slot >= 32) return ActorError::Capacity;
        bool empty = s.type == 0;
        for (size_t j = 0; j < 3; ++j) empty = empty && s.x[j] == 0 && s.y[j] == 0;
        if (!s.slot || empty || object_type(s.type) || (dead & (uint32_t(1) << s.slot))) continue;
        const uint8_t idx = schedule_index(s.times,hour);
        auto &n = next.actors[next.count++]; n.schedule = s; n.location = location;
        n.x = s.x[idx]; n.y = s.y[idx]; n.z = norm_z(s.z[idx]); n.served_slot = idx;
    }
    out = next; return ActorError::None;
}
// 0x16c9-0x171b walks slots 1..31 whose type byte (DS 0x659E) is non-zero; a
// slot cleared by 0x00B0 has type 0 and is simply absent from NpcActors. Per
// slot: period from NPC.OVL:0x12E0 (0x16d7), X/Y/Z from the schedule into the
// live record (0x1841-0x1856), state 1, servedSlot = period (0x1705), pathIdx
// -1 (0x170c). DS 0x65C2 (stuck) is not written.
void snap_npcs_to_schedule(NpcActors &list, uint8_t location, uint8_t hour) {
    for (size_t i = 0; i < list.count; ++i) {
        auto &n = list.actors[i];
        if (n.location != location) continue;
        const uint8_t idx = schedule_index(n.schedule.times, hour);
        n.x = n.schedule.x[idx]; n.y = n.schedule.y[idx]; n.z = norm_z(n.schedule.z[idx]);
        n.state = 1; n.served_slot = idx; n.path_index = -1;
    }
}
int32_t npc_check_schedule(NpcActor &n, uint8_t hour, int16_t v) {
    const auto &s = n.schedule;
    if (std::find(std::begin(s.times),std::end(s.times),hour) == std::end(s.times)) return 0;
    const auto di = schedule_index(s.times,hour); int32_t result = di;
    const int16_t zd = norm_z(s.z[di]);
    if (n.served_slot == di) n.state = 1;
    else if (n.z != v && zd != v) n.state = 8;
    else if (n.z == v && zd == v) n.state = 2;
    else if (n.z == v && zd > v) n.state = 6;
    else if (n.z == v) n.state = 7;
    else if (n.z > v) n.state = 4;
    else n.state = 5;
    if (n.x == s.x[di] && n.y == s.y[di] && n.z == zd) { result = 0; n.state = 1; }
    return result + 1;
}
bool npc_occupied(const NpcActors &list, const WorldPosition &p, uint8_t loc, int16_t z, int16_t x, int16_t y, uint8_t except) {
    if (p.map.location == loc && p.map.floor == z && p.xy.x == x && p.xy.y == y) return true;
    for (size_t i = 0; i < list.count; ++i) {
        const auto &o = list.actors[i];
        if (o.schedule.slot != except && o.z == z && o.x == x && o.y == y) return true;
    }
    return false;
}
static int32_t distance(int32_t x, int32_t y, int32_t px, int32_t py) { return std::abs(x-px)+std::abs(y-py); }
void npc_ai_step(NpcActor &n, uint8_t idx, NpcActors &list, const GameState &g, const ActiveMap &map, Rand rand) {
    if (n.z != g.position.map.floor || idx >= 3) return;
    const auto &s = n.schedule; const auto &p = g.position;
    const auto cur = distance(n.x,n.y,p.xy.x,p.xy.y);
    const bool same = p.map.location == n.location && p.map.floor == n.z;
    const bool near = same && cur < 4;
    const uint8_t ai = s.ai[idx];
    auto allowed = [&](int16_t x, int16_t y) {
        return x >= 0 && y >= 0 && x < 32 && y < 32 &&
            is_passable(map.tile_at(x,y),TransportMode::Foot).value &&
            !npc_occupied(list,p,n.location,n.z,x,y,s.slot);
    };
    const bool merchant_near = same && distance(p.xy.x,p.xy.y,s.x[idx],s.y[idx]) < 4;
    if (ai == 1 || ai == 2 || (ai == 4 && !merchant_near)) {
        if ((rand(0,255) & 8) == 0) return;
        const int32_t dir = (rand(0,64) & 3)+1;
        int16_t x = n.x, y = n.y;
        switch (dir) {
        case 1: ++x; if (y > 32) x = 32; break;
        case 2: --y; if (x < 0) y = 0; break;
        case 3: --x; if (y < 0) x = 0; break;
        default: ++y; if (x > 32) y = 32; break;
        }
        if (ai != 2 && distance(x,y,s.x[idx],s.y[idx]) > 3) return;
        if (allowed(x,y)) { n.x = x; n.y = y; } return;
    }
    if (ai == 0 || ai > 7 || ((ai == 3 || ai == 6) && !near)) return;
    const bool hostile = ai == 5 || ai == 7;
    if (hostile && (!same || cur == 1)) return;
    constexpr int16_t dx[] = {0,0,1,-1}, dy[] = {-1,1,0,0};
    int32_t score[4]; int32_t chosen = -1, best = cur;
    for (int32_t i = 0; i < 4; ++i) {
        const int16_t x = int16_t(n.x + dx[i]), y = int16_t(n.y + dy[i]);
        const bool ok = allowed(x,y); score[i] = ok ? distance(x,y,p.xy.x,p.xy.y) : 99;
        if (!ok) continue;
        if (hostile) { if (chosen < 0 && score[i] < cur) chosen = i; }
        else if (ai == 3 ? score[i] > best : score[i] < best) { best = score[i]; chosen = i; }
    }
    if (hostile && rand(0,63) < 16) {
        int32_t pick = chosen;
        for (int32_t i = 0; i < 4; ++i) {
            if (i == chosen || score[i] == 99) continue;
            if (pick == chosen) { pick = i; continue; }
            if (rand(0,63) < 16) pick = i;
        }
        chosen = pick;
    }
    if (chosen >= 0) { n.x = int16_t(n.x + dx[chosen]); n.y = int16_t(n.y + dy[chosen]); }
}
ActorError tick_idle_npcs(NpcActors &list, const GameState &g, const ActiveMap &map, Rand rand) {
    if (g.position.map.location == 0 || list.count == 0) return ActorError::None;
    if (list.count > 32) return ActorError::Capacity;
    // Preflight on individual values preserves the entire input on unsupported walks.
    for (size_t i = 0; i < list.count; ++i) {
        auto n = list.actors[i];
        if (n.state > 1 || n.path_index != -1) return ActorError::NeedsPathfinding;
        npc_check_schedule(n,uint8_t(g.time.hour),g.position.map.floor);
        if (n.state > 1) return ActorError::NeedsPathfinding;
    }
    bool needs_map = false;
    for (size_t i = 0; i < list.count; ++i) if (list.actors[i].z == g.position.map.floor) needs_map = true;
    if (needs_map && (!map.tiles || map.geometry.width != 32 || map.geometry.height != 32 || map.id.location != g.position.map.location || map.id.floor != g.position.map.floor)) return ActorError::InvalidMap;
    // Compatibility entry point retains its explicit idle-only preflight, but
    // all supported ticks run the shared scheduling implementation.
    if (!needs_map) {
        for (size_t i=0;i<list.count;++i) npc_check_schedule(list.actors[i],uint8_t(g.time.hour),g.position.map.floor);
        return ActorError::None;
    }
    NpcScanGrid scratch{};
    const WorldData world{};
    return tick_npcs(list,g,{world,map,scratch},rand);
}
ActorError tick_guards(NpcActors &list, const GameState &g, const ActiveMap &map, Rand rand) {
    if (g.position.map.location == 0 || list.count == 0) return ActorError::None;
    if (list.count > 32) return ActorError::Capacity;
    std::array<size_t,32> order{}; size_t count = 0;
    for (size_t i = 0; i < list.count; ++i) {
        const auto &n = list.actors[i];
        if ((n.schedule.type & 254) != 16 || n.z != g.position.map.floor) continue;
        size_t j = count++;
        while (j && list.actors[order[j-1]].schedule.slot > n.schedule.slot) { order[j] = order[j-1]; --j; }
        order[j] = i;
    }
    if (!count) return ActorError::None;
    if (!map.tiles || map.geometry.width != 32 || map.geometry.height != 32 || map.id.location != g.position.map.location || map.id.floor != g.position.map.floor) return ActorError::InvalidMap;
    constexpr int16_t dx[] = {0,0,1,-1}, dy[] = {-1,1,0,0};
    for (size_t i = 0; i < count; ++i) {
        auto &n = list.actors[order[i]];
        if (rand(0,1) != 0) continue;
        bool blocked = false;
        for (size_t d = 0; d < 4; ++d) { const auto tile = map.tile_at(n.x+dx[d],n.y+dy[d]); if (tile == 162 || tile == 67) blocked = true; }
        if (blocked) continue;
        const auto axis = rand(0,1), sign = 2*rand(0,1)-1;
        int16_t x = n.x, y = n.y; uint8_t tile = n.schedule.type;
        if (axis == 1) { x = int16_t(x+sign); tile = sign > 0 ? 16 : 17; } else y = int16_t(y+sign);
        if (x < 0 || y < 0 || x >= 32 || y >= 32 || !is_passable(map.tile_at(x,y),TransportMode::Foot).value || npc_occupied(list,g.position,n.location,n.z,x,y,n.schedule.slot)) continue;
        n.x = x; n.y = y; n.schedule.type = tile;
    }
    return ActorError::None;
}
uint8_t find_free_actor_slot(uint32_t mask) { for (int32_t i = 31; i >= 1; --i) if (!(mask & (uint32_t(1)<<i))) return uint8_t(i); return 0; }
uint8_t first_free_recycle_slot(uint32_t mask) { for (uint8_t i = 1; i <= 23; ++i) if (!(mask & (uint32_t(1)<<i))) return i; return 0; }
uint8_t scan_recyclable_slot(const ActorPool &view, int32_t lo, int32_t hi, bool offscreen, int32_t px, int32_t py) {
    for (uint8_t i = 1; i <= 23; ++i) {
        const auto &v = view[i]; if (v.tile < lo || v.tile > hi || v.tile == 181) continue;
        if (offscreen && ((int64_t(v.x)-px+5)&255) <= 10 && ((int64_t(v.y)-py+5)&255) <= 10) continue;
        return i;
    }
    return 0;
}
uint8_t acquire_actor_slot(const ActorPool &v, int32_t px, int32_t py) {
    constexpr int32_t ranges[][3] = {{0,0,0},{1,15,1},{128,255,1},{16,17,1},{48,127,1},{1,15,0},{128,255,0},{16,17,0},{48,127,0},{0,255,0}};
    for (const auto &r : ranges) { const auto s = scan_recyclable_slot(v,r[0],r[1],r[2]!=0,px,py); if (s) return s; } return 0;
}
ActorPool compose_world_pool(int32_t loc, int32_t floor, PoolEntity *enemies, size_t ne, PoolEntity *objects, size_t no) {
    ActorPool view; uint32_t used = 0;
    auto valid = [](int32_t s) { return s >= 1 && s < 32; };
    if (loc != 0) ne = 0;
    for (size_t i = 0; i < ne; ++i) if (valid(enemies[i].slot)) used |= uint32_t(1)<<enemies[i].slot;
    for (size_t i = 0; i < no; ++i) if (objects[i].location == loc && valid(objects[i].slot)) used |= uint32_t(1)<<objects[i].slot;
    for (size_t i = 0; i < ne; ++i) if (!valid(enemies[i].slot)) { const auto s = first_free_recycle_slot(used); if (s) { enemies[i].slot = s; used |= uint32_t(1)<<s; } }
    for (size_t i = 0; i < no; ++i) if (objects[i].location == loc && !valid(objects[i].slot)) { const auto s = find_free_actor_slot(used); if (s) { objects[i].slot = s; used |= uint32_t(1)<<s; } }
    for (size_t i = 0; i < ne; ++i) { const auto &e = enemies[i]; if (valid(e.slot)) view[size_t(e.slot)] = {uint8_t(e.tile),e.x,e.y,floor,PoolOwnerKind::Enemy,i}; }
    for (size_t i = 0; i < no; ++i) { const auto &o = objects[i]; if (o.location == loc && valid(o.slot)) view[size_t(o.slot)] = {uint8_t(o.tile),o.x,o.y,o.floor,PoolOwnerKind::Object,i}; }
    return view;
}
}
