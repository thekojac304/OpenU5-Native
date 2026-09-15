#include "openu5/transitions.h"
namespace openu5 {
static void effect(TransitionServices s, ReloadEffect e, uint8_t id) {
    if (s.effect)
        s.effect(s.context, e, id);
}
static void message(TransitionServices s, const char *text) {
    if (s.event)
        s.event(s.context, GameEventKind::Message, text);
}
static void changed(TransitionServices s) {
    if (s.event)
        s.event(s.context, GameEventKind::MapChanged, nullptr);
}
int32_t location_at(LocationTable t, int32_t x, int32_t y) {
    for (size_t i = 0; i < t.x_count && i < t.y_count; ++i)
        if (t.x[i] == x && t.y[i] == y)
            return int32_t(i + 1);
    return 0;
}
bool floor_exists(const WorldData &w, MapId id) {
    for (size_t i = 0; i < w.small_map_count; ++i)
        if (w.small_maps[i].id.location == id.location && w.small_maps[i].id.floor == id.floor)
            return true;
    return false;
}
Error load_small_map(GameState &g, TurnState &t, TravelState &v, uint8_t id, const char *banner,
                     TransitionServices s) {
    if (banner && !(id >= 14 && id <= 18) && id != 40)
        message(s, banner);
    g.position = {{15, 30}, {id, 0}};
    effect(s, ReloadEffect::EnterNpcs, id);
    effect(s, ReloadEffect::ClearEnemies, id);
    effect(s, ReloadEffect::ResetDoors, id);
    v.volatile_terrain_wipe = false;
    effect(s, ReloadEffect::ClearTerrain, id);
    t.drunk_turns = 0;
    v.drunk_pre_rolled = false;
    effect(s, ReloadEffect::HydrateInterior, id);
    effect(s, ReloadEffect::RefreshHourTiles, id);
    v.shadowlord_here = -1;
    if (t.has_shadowlords && g.position.xy.y != 4)
        for (size_t i = 0; i < 3; ++i)
            if (t.shadowlord_locations[i] == id) {
                v.shadowlord_here = int8_t(i);
                break;
            }
    effect(s, ReloadEffect::UrbanEffects, id);
    changed(s);
    return Error::None;
}
Error exit_to_overworld(GameState &g, LocationTable table, TransitionServices s) {
    const auto id = g.position.map.location;
    if (!id || !table.x || !table.y || size_t(id) > table.x_count || size_t(id) > table.y_count)
        return Error::MissingMap;
    const auto x = table.x[id - 1], y = table.y[id - 1];
    effect(s, ReloadEffect::DiscardInterior, id);
    effect(s, ReloadEffect::ResetDoors, id);
    effect(s, ReloadEffect::ClearTerrain, id);
    g.position = {{x, y}, {0, 0}};
    message(s, "\nExit to\nBritannia!");
    changed(s);
    return Error::None;
}
Error confirm_town_exit(GameState &g, bool yes, LocationTable t, TransitionServices s) {
    if (yes)
        return exit_to_overworld(g, t, s);
    effect(s, ReloadEffect::ContextTurn, g.position.map.location);
    return Error::None;
}
bool apply_stair_step(GameState &g, const WorldData &w, int32_t tile, Direction dir, TransitionServices s) {
    if ((tile & 252) != 196)
        return false;
    int32_t d = 0;
    switch (dir) {
    case Direction::North:
        d = 0;
        break;
    case Direction::East:
        d = 1;
        break;
    case Direction::South:
        d = 2;
        break;
    case Direction::West:
        d = 3;
        break;
    }
    const auto orient = tile - 196;
    const int16_t delta = orient == d ? 1 : orient == (d ^ 2) ? -1 : 0;
    MapId target = g.position.map;
    target.floor = int16_t(target.floor + delta);
    if (!delta || !floor_exists(w, target))
        return false;
    g.position.map = target;
    effect(s, ReloadEffect::RefreshHourTiles, target.location);
    message(s, delta > 0 ? "Up!" : "Down!");
    changed(s);
    return true;
}
bool klimb_ladder(GameState &g, const WorldData &w, int16_t delta, TransitionServices s) {
    MapId target = g.position.map;
    target.floor = int16_t(target.floor + delta);
    if (!floor_exists(w, target)) {
        message(s, "Klimb-What?");
        return false;
    }
    g.position.map = target;
    effect(s, ReloadEffect::RefreshHourTiles, target.location);
    effect(s, ReloadEffect::ContextTurn, target.location);
    message(s, delta > 0 ? "Klimb-Up!" : "Klimb-Down!");
    changed(s);
    return true;
}
bool moonstone_teleport(GameState &g, TurnState &t, TravelState &v, const Moonstone *stones, size_t count,
                        int32_t phase, const char *banner, TransitionServices s) {
    if (phase < 0 || size_t(phase) >= count || !stones || stones[phase].location == 255)
        return false;
    const auto dest = stones[phase];
    g.position = {{dest.x, dest.y}, {dest.location, int16_t(dest.z == 255 ? 255 : 0)}};
    if (dest.location) {
        load_small_map(g, t, v, dest.location, banner, s);
        g.position.xy = {dest.x, dest.y};
    } else
        effect(s, ReloadEffect::HydrateUnderworld, 0);
    changed(s);
    return true;
}
bool local_boundary(const ActiveMap &m, Position p, Direction dir, TransitionServices s) {
    if (m.kind != MapKind::Small)
        return false;
    const auto d = direction_delta(dir);
    if (in_bounds(int32_t(p.x) + d.dx, int32_t(p.y) + d.dy, m.geometry))
        return false;
    if (s.event)
        s.event(s.context, GameEventKind::TownExitPrompt, nullptr);
    return true;
}
TurnHook scheduled_npc_hook(ScheduledTurnBinding &b) {
    return {&b, [](void *p) {
                auto &v = *static_cast<ScheduledTurnBinding *>(p);
                const auto map = v.effective_map ? v.effective_map(v.map_context, v.state.position.map)
                                                 : get_active_map(v.world, v.state.position.map).value;
                v.error = tick_npcs(v.actors, v.state, {v.world, map, v.scratch}, v.rand);
            }};
}
} // namespace openu5
