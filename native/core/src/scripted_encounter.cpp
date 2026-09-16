#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include <algorithm>
namespace openu5 {
CombatDirection dungeon_room_entry(const CombatMap &map, uint8_t facing) {
    static constexpr int opposite[] = {2, 1, 3, 0};
    int primary = opposite[facing & 3];
    auto degenerate = [&](int d) {
        if (!map.start_count[d])
            return false;
        for (int i = 0; i < map.start_count[d]; ++i)
            if (map.starts[d][i].x || map.starts[d][i].y)
                return false;
        return true;
    };
    if (!degenerate(primary))
        return CombatDirection(primary);
    for (int d : {2, 3, 0, 1})
        if (!degenerate(d))
            return CombatDirection(d);
    return CombatDirection(primary);
}
CombatResult start_fixed_combat(CommandContext &world, CombatContext &combat, const CombatMap &map,
                                const FixedCombatSetup &fixed, CombatDirection entry, bool room,
                                int cause) {
    if (world.combat || &world.game != &combat.game || &world.turn != &combat.turn)
        return CombatResult::Invalid;
    auto result = initialize_combat(combat, map, entry, nullptr, 0, room, &fixed);
    if (result != CombatResult::Ok)
        return result;
    auto &s = combat.combat;
    world.combat = true;
    world.combat_context = &combat;
    if (world.dungeon_context) {
        auto &d = *world.dungeon_context;
        d.corridor_cause = int8_t(cause);
        d.room_entry_valid = room && d.state.active;
        d.room_entry = d.state.pos;
        if (d.room_entry_valid) {
            s.victory_context = &world;
            s.victory_latch = [](void *p) {
                auto &w = *static_cast<CommandContext *>(p);
                auto &ctx = *w.dungeon_context;
                auto &e = ctx.room_entry;
                if (ctx.room_entry_valid && ctx.state.active)
                    dungeon_mark_room(w.game, ctx.state, e.floor, e.x, e.y);
            };
            if (s.victory)
                s.victory_latch(s.victory_context);
        }
    }
    auto emit = [&](GameEventKind kind, const char *text = nullptr) {
        GameEvent e;
        e.kind = kind;
        e.text = text;
        if (world.events.emit)
            world.events.emit(world.events.context, e);
    };
    if (room) {
        emit(GameEventKind::Message, "Entering room...");
        auto expired = roll_ring_expiry(world.game, rng_source(world.game.rng));
        for (int i = 0; i < expired.count; ++i) {
            unequip_item_by_id(world.game, expired.members[i], expired.ids[i]);
            emit(GameEventKind::Message, "A ring has vanished!\n");
            emit(GameEventKind::Sfx, "ring-vanishes");
            emit(GameEventKind::PartyChanged);
        }
    }
    emit(GameEventKind::CombatStarted);
    return CombatResult::Ok;
}
void build_corridor_map(GameState &g, const DungeonState &d, int max, CombatMap &map,
                        uint8_t sprites[16]) {
    map = {};
    map.index = -1;
    bool metal = d.pos.dungeon == 33 || d.pos.dungeon == 36 || d.pos.dungeon == 37;
    int floor = metal ? 0x45 : 5, wall = metal ? 0x4f : 0x4d;
    std::fill_n(map.tiles, 121, int16_t(floor));
    auto tile = [&](int x, int y) -> int16_t & { return map.tiles[y * 11 + x]; };
    for (int i = 0; i < 11; ++i) {
        tile(i, 1) = tile(i, 9) = tile(1, i) = tile(9, i) = int16_t(wall);
    }
    tile(0, 0) = tile(10, 0) = tile(0, 10) = tile(10, 10) = 255;
    static constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    int cell = dungeon_cell(d, d.pos.floor, d.pos.x, d.pos.y) >> 4;
    for (int side = 0; side < 4; ++side) {
        int t = dungeon_cell(d, d.pos.floor, (d.pos.x + dx[side] + 8) % 8,
                             (d.pos.y + dy[side] + 8) % 8) >>
                4;
        if (t < 10 || t == 10 || t == 14 || t == 15) {
            int from = t < 10 ? 2 : 3, to = t < 10 ? 8 : 7;
            for (int i = from; i <= to; ++i) {
                if (side == 0)
                    tile(i, 1) = int16_t(floor);
                else if (side == 1)
                    tile(9, i) = int16_t(floor);
                else if (side == 2)
                    tile(i, 9) = int16_t(floor);
                else
                    tile(1, i) = int16_t(floor);
            }
        } else {
            for (int i = 0; i < 11; ++i) {
                if (side == 0)
                    tile(i, 0) = 255;
                else if (side == 1)
                    tile(10, i) = 255;
                else if (side == 2)
                    tile(i, 10) = 255;
                else
                    tile(0, i) = 255;
            }
            if (cell == 14) {
                if (side == 0)
                    tile(5, 2) = tile(5, 8) = int16_t(wall);
                else if (side == 3)
                    tile(8, 5) = tile(2, 5) = int16_t(wall);
            }
        }
    }
    static constexpr int feature[] = {0, 0xc8, 0xc9, 0xc8, 0xdc, 0xd8};
    if (cell >= 1 && cell <= 5)
        tile(5, 5) = int16_t(feature[cell]);
    static constexpr int a[] = {5, 4, 6, 3, 5, 7}, b[] = {6, 7, 7, 8, 8, 8},
                         c[] = {4, 3, 3, 2, 2, 2};
    for (int i = 0; i < 6; ++i) {
        map.starts[0][i] = {int16_t(b[i]), int16_t(a[i])};
        map.starts[1][i] = {int16_t(c[i]), int16_t(a[i])};
        map.starts[2][i] = {int16_t(a[i]), int16_t(b[i])};
        map.starts[3][i] = {int16_t(a[i]), int16_t(c[i])};
    }
    for (auto &n : map.start_count)
        n = 6;
    static constexpr int p[] = {5, 4, 6, 3, 7, 2, 8, 5, 2, 8, 3, 7, 2, 4, 6, 8},
                         q[] = {8, 8, 8, 7, 7, 6, 6, 9, 8, 8, 9, 9, 10, 10, 10, 10},
                         r[] = {2, 2, 2, 3, 3, 4, 4, 1, 2, 2, 1, 1, 0, 0, 0, 0},
                         s[] = {5, 4, 6, 3, 7, 2, 8, 5, 2, 8, 7, 3, 2, 4, 6, 8};
    int order[16];
    for (int i = 0; i < 16; ++i)
        order[i] = i;
    for (int i = 0; i < 16; ++i)
        std::swap(order[i], order[g.rng.next(0, 15).value]);
    int roll = g.rng.next(1, std::max(1, max)).value, count = max == 8 || max == 16 ? max : roll;
    for (int i = 0; i < count && i < 16; ++i) {
        int j = order[i], f = int(d.pos.facing);
        map.units[i] = {int16_t(f == 0   ? p[j]
                                : f == 1 ? q[j]
                                : f == 2 ? s[j]
                                         : r[j]),
                        int16_t(f == 0   ? r[j]
                                : f == 1 ? s[j]
                                : f == 2 ? q[j]
                                         : p[j])};
        sprites[i] = uint8_t(0x40 + d.wanderer.type * 4);
        ++map.unit_count;
    }
}
} // namespace openu5
