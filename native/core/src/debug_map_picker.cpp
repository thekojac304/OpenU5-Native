#include "openu5/debug_map_picker.h"

#include "openu5/dungeon.h"

#include <limits>

namespace openu5 {
namespace {

constexpr int32_t kUnderworldEntryX = 126;
constexpr int32_t kUnderworldEntryY = 20;

bool dungeon_default_cell(const DungeonData &data, int floor, int &x, int &y) {
    if (floor < 0 || floor >= 8)
        return false;
    const size_t base = size_t(floor) * 64;
    for (int preferred : {1, 3, 2})
        for (int cell = 0; cell < 64; ++cell)
            if ((data.cells[base + size_t(cell)] >> 4) == preferred) {
                x = cell & 7;
                y = cell >> 3;
                return true;
            }
    return false;
}

const DungeonContext *dungeon_context(const CommandContext &c) { return c.dungeon_context; }

const DungeonData *dungeon_data(const CommandContext &c, uint8_t location) {
    const auto *d = dungeon_context(c);
    if (!d || (d->count && !d->data))
        return nullptr;
    for (size_t i = 0; i < d->count; ++i)
        if (d->data[i].location == location)
            return &d->data[i];
    return nullptr;
}

bool small_location_exists(const WorldData &world, uint8_t location) {
    if (world.small_map_count && !world.small_maps)
        return false;
    for (size_t i = 0; i < world.small_map_count; ++i)
        if (world.small_maps[i].id.location == location)
            return true;
    return false;
}

bool destination_exists(const CommandContext &c, DebugDestination d) {
    switch (d.kind) {
    case DebugDestinationKind::Britannia:
    case DebugDestinationKind::Underworld:
        return d.location == 0;
    case DebugDestinationKind::SmallMap:
        return d.location != 0 && small_location_exists(c.world, d.location);
    case DebugDestinationKind::Dungeon:
        return d.location >= 33 && d.location <= 40 && dungeon_data(c, d.location);
    }
    return false;
}

const MapData *small_floor(const WorldData &world, uint8_t location, int16_t floor) {
    if (world.small_map_count && !world.small_maps)
        return nullptr;
    for (size_t i = 0; i < world.small_map_count; ++i) {
        const auto &map = world.small_maps[i];
        if (map.id.location == location && map.id.floor == floor)
            return &map;
    }
    return nullptr;
}

size_t unique_small_locations(const WorldData &world) {
    size_t count = 0;
    for (int location = 1; location <= 255; ++location)
        if (small_location_exists(world, uint8_t(location)))
            ++count;
    return count;
}

size_t unique_dungeons(const CommandContext &c) {
    size_t count = 0;
    for (int location = 1; location <= 255; ++location)
        if (dungeon_data(c, uint8_t(location)))
            ++count;
    return count;
}

const char *destination_name(const CommandContext &c, DebugDestinationKind kind, uint8_t location) {
    if (kind == DebugDestinationKind::Britannia)
        return "Britannia";
    if (kind == DebugDestinationKind::Underworld)
        return "Underworld";
    return c.services.banner ? c.services.banner(c.services.context, location) : nullptr;
}

void emit_map_changed(CommandContext &c) {
    if (!c.events.emit)
        return;
    GameEvent event;
    event.kind = GameEventKind::MapChanged;
    c.events.emit(c.events.context, event);
}

void reload(CommandContext &c, ReloadEffect effect, uint8_t location) {
    if (c.services.reload)
        c.services.reload(c.services.context, effect, location, c.events);
}

DebugTeleportResult result(DebugTeleportStatus status) {
    DebugTeleportResult out;
    out.status = status;
    return out;
}

DebugTeleportResult validate_map_cell(const CommandContext &c, const DebugTeleportRequest &r) {
    DebugDestination destination{r.kind, r.location, nullptr};
    if (!destination_exists(c, destination))
        return result(r.kind == DebugDestinationKind::Dungeon && !c.dungeon_context
                          ? DebugTeleportStatus::MissingDungeonContext
                          : DebugTeleportStatus::InvalidDestination);

    DebugTeleportResult out = result(DebugTeleportStatus::Applied);
    switch (r.kind) {
    case DebugDestinationKind::Britannia:
    case DebugDestinationKind::Underworld: {
        const bool under = r.kind == DebugDestinationKind::Underworld;
        if ((!under && (!c.world.overworld || c.world.overworld_size < 65536)) ||
            (under && (!c.world.underworld || c.world.underworld_size < 65536)))
            return result(DebugTeleportStatus::MissingMapData);
        const auto x = wrap_coord(under && r.standard_entry ? kUnderworldEntryX : r.x);
        const auto y = wrap_coord(under && r.standard_entry ? kUnderworldEntryY : r.y);
        const auto *tiles = under ? c.world.underworld : c.world.overworld;
        out.passability_known = true;
        out.passable = is_walkable_tile(tiles[size_t(y) * kLargeMapSize + size_t(x)]);
        return out;
    }
    case DebugDestinationKind::SmallMap: {
        const auto *map = small_floor(c.world, r.location, r.floor);
        if (!map)
            return result(DebugTeleportStatus::InvalidFloor);
        if (!map->tiles || map->size < size_t(kSmallMapSize) * kSmallMapSize)
            return result(DebugTeleportStatus::MissingMapData);
        const int32_t x = r.standard_entry ? kSmallMapEntryX : r.x;
        const int32_t y = r.standard_entry ? kSmallMapEntryY : r.y;
        if (x < 0 || y < 0 || x >= kSmallMapSize || y >= kSmallMapSize)
            return result(DebugTeleportStatus::InvalidCoordinates);
        out.passability_known = true;
        out.passable = is_walkable_tile(map->tiles[size_t(y) * kSmallMapSize + size_t(x)]);
        return out;
    }
    case DebugDestinationKind::Dungeon: {
        if (r.floor < 0 || r.floor >= 8)
            return result(DebugTeleportStatus::InvalidFloor);
        const auto *data = dungeon_data(c, r.location);
        if (!data)
            return result(DebugTeleportStatus::InvalidDestination);
        int x=r.x,y=r.y;
        if(r.standard_entry&&!dungeon_default_cell(*data,r.floor,x,y))
            return result(DebugTeleportStatus::InvalidCoordinates);
        if (x < 0 || y < 0 || x >= 8 || y >= 8)
            return result(DebugTeleportStatus::InvalidCoordinates);
        const auto type = uint8_t(data->cells[size_t(r.floor) * 64 + size_t(y) * 8 + size_t(x)] >> 4);
        out.passability_known = true;
        // teleportPicker.ts DUNGEON_IMPASSABLE: wall, special wall, secret door.
        out.passable = type != 0x0b && type != 0x0c && type != 0x0d;
        const auto &state = c.dungeon_context->state;
        if ((!state.active || state.pos.dungeon != r.location) && c.combat)
            out.status = DebugTeleportStatus::ActiveCombat;
        return out;
    }
    }
    return result(DebugTeleportStatus::InvalidDestination);
}

} // namespace

size_t debug_destination_count(const CommandContext &c) {
    return 2 + unique_small_locations(c.world) + unique_dungeons(c);
}

Result<DebugDestination> debug_destination_at(const CommandContext &c, size_t index) {
    Result<DebugDestination> out;
    if (index == 0) {
        out.value = {DebugDestinationKind::Britannia, 0, "Britannia"};
        return out;
    }
    if (index == 1) {
        out.value = {DebugDestinationKind::Underworld, 0, "Underworld"};
        return out;
    }
    index -= 2;
    for (int location = 1; location <= 255; ++location) {
        if (!small_location_exists(c.world, uint8_t(location)))
            continue;
        if (index-- == 0) {
            out.value = {DebugDestinationKind::SmallMap, uint8_t(location),
                         destination_name(c, DebugDestinationKind::SmallMap, uint8_t(location))};
            return out;
        }
    }
    for (int location = 1; location <= 255; ++location) {
        if (!dungeon_data(c, uint8_t(location)))
            continue;
        if (index-- == 0) {
            out.value = {DebugDestinationKind::Dungeon, uint8_t(location),
                         destination_name(c, DebugDestinationKind::Dungeon, uint8_t(location))};
            return out;
        }
    }
    out.error = Error::InvalidRange;
    return out;
}

size_t debug_floor_count(const CommandContext &c, DebugDestination d) {
    if (!destination_exists(c, d))
        return 0;
    if (d.kind == DebugDestinationKind::Britannia || d.kind == DebugDestinationKind::Underworld)
        return 1;
    if (d.kind == DebugDestinationKind::Dungeon)
        return 8;
    size_t count = 0;
    for (size_t i = 0; i < c.world.small_map_count; ++i) {
        const auto &candidate = c.world.small_maps[i].id;
        if (candidate.location != d.location)
            continue;
        bool earlier = false;
        for (size_t j = 0; j < i; ++j)
            earlier |= c.world.small_maps[j].id.location == d.location &&
                       c.world.small_maps[j].id.floor == candidate.floor;
        if (!earlier)
            ++count;
    }
    return count;
}

Result<FloorId> debug_floor_at(const CommandContext &c, DebugDestination d, size_t index) {
    Result<FloorId> out;
    const auto count = debug_floor_count(c, d);
    if (index >= count) {
        out.error = Error::InvalidRange;
        return out;
    }
    if (d.kind == DebugDestinationKind::Britannia) {
        out.value = 0;
        return out;
    }
    if (d.kind == DebugDestinationKind::Underworld) {
        out.value = 255;
        return out;
    }
    if (d.kind == DebugDestinationKind::Dungeon) {
        out.value = FloorId(index);
        return out;
    }

    FloorId previous = std::numeric_limits<FloorId>::min();
    for (size_t rank = 0; rank <= index; ++rank) {
        FloorId next = std::numeric_limits<FloorId>::max();
        for (size_t i = 0; i < c.world.small_map_count; ++i) {
            const auto id = c.world.small_maps[i].id;
            if (id.location == d.location && id.floor > previous && id.floor < next)
                next = id.floor;
        }
        previous = next;
    }
    out.value = previous;
    return out;
}

DebugTeleportResult validate_debug_teleport(const CommandContext &c, const DebugTeleportRequest &r) {
    if (r.location != 0 &&
        (r.kind == DebugDestinationKind::Britannia || r.kind == DebugDestinationKind::Underworld))
        return result(DebugTeleportStatus::InvalidDestination);
    if (r.location == 0 &&
        (r.kind == DebugDestinationKind::SmallMap || r.kind == DebugDestinationKind::Dungeon))
        return result(DebugTeleportStatus::InvalidDestination);
    if ((r.kind == DebugDestinationKind::Britannia && r.floor != 0) ||
        (r.kind == DebugDestinationKind::Underworld && r.floor != 255))
        return result(DebugTeleportStatus::InvalidFloor);
    return validate_map_cell(c, r);
}

DebugTeleportResult apply_debug_teleport(CommandContext &c, const DebugTeleportRequest &r) {
    auto out = validate_debug_teleport(c, r);
    if (out.status != DebugTeleportStatus::Applied)
        return out;

    // Batch 9D.  A live arena owns the screen, the keys and a CombatState whose
    // return bookkeeping (encounter_location/floor, loot_x/y, the dungeon room
    // latch) describes the place the party left.  Every arm below rewrites
    // exactly that place -- the Britannia/Underworld/SmallMap arms additionally
    // clear DungeonContext::state.active -- while leaving CommandContext::combat
    // true.  The result is a mounted combat scene over a world that no longer
    // matches it and no reachable way out: the freeze the hardware session hit
    // when it tried to teleport away from a broken dungeon fight.
    //
    // The policy is the picker's own and already exists: refuse at apply time
    // with DebugTeleportStatus::ActiveCombat, which debug_labels.cpp already
    // renders as "Blocked by active combat" and the device screen already
    // shows.  It was only ever enforced on the cross-dungeon arm, which is the
    // one case that happened to route through execute_dungeon_command()'s own
    // combat guard.  Finish the fight (or flee) and the teleport works again.
    if (c.combat) {
        out.status = DebugTeleportStatus::ActiveCombat;
        return out;
    }

    // Debug-certification safety contract: a standard-entry (Default Entrance)
    // teleport must never silently drop the party on a known-impassable cell.
    // This is a refusal at *apply* time only -- validate_debug_teleport keeps
    // reporting Applied/passable=false so callers can inspect the cell before
    // deciding whether to apply. An explicit manual coordinate request
    // (standard_entry == false) is left untouched; see T4/Part 3.
    if (r.standard_entry && out.passability_known && !out.passable) {
        out.status = DebugTeleportStatus::ImpassableDestination;
        return out;
    }

    switch (r.kind) {
    case DebugDestinationKind::Britannia:
    case DebugDestinationKind::Underworld:
        // A dungeon retains the surface position as its return location, so
        // position alone cannot own session teardown.  A debug map teleport is
        // an explicit context change and must leave the authoritative session.
        if (c.dungeon_context)
            c.dungeon_context->state.active = false;
        c.dungeon = false;
        c.game.position = {{uint8_t(wrap_coord(r.kind==DebugDestinationKind::Underworld&&r.standard_entry?kUnderworldEntryX:r.x)), uint8_t(wrap_coord(r.kind==DebugDestinationKind::Underworld&&r.standard_entry?kUnderworldEntryY:r.y))},
                           {0, r.kind == DebugDestinationKind::Underworld ? int16_t(255) : int16_t(0)}};
        emit_map_changed(c);
        return out;

    case DebugDestinationKind::SmallMap: {
        if (c.dungeon_context)
            c.dungeon_context->state.active = false;
        c.dungeon = false;
        const int32_t x = r.standard_entry ? kSmallMapEntryX : r.x;
        const int32_t y = r.standard_entry ? kSmallMapEntryY : r.y;
        c.game.position = {{uint8_t(x), uint8_t(y)}, {r.location, r.floor}};
        // Exact DebugApi.goToLocation/teleportSmallMap recipe and order.
        reload(c, ReloadEffect::ResetDoors, r.location);
        reload(c, ReloadEffect::EnterNpcs, r.location);
        reload(c, ReloadEffect::HydrateInterior, r.location);
        emit_map_changed(c);
        out.reloaded = true;
        return out;
    }

    case DebugDestinationKind::Dungeon: {
        auto &state = c.dungeon_context->state;
        if (!state.active || state.pos.dungeon != r.location) {
            const auto seed=c.game.rng.get_seed();
            Command enter;
            enter.kind = CommandKind::EnterDungeon;
            enter.member = r.location;
            enter.hours = c.game.position.map.floor;
            const auto entered = execute_dungeon_command(c, enter);
            c.game.rng.seed(seed);
            if (entered.status != CommandStatus::Success || !state.active ||
                state.pos.dungeon != r.location) {
                out.status = c.combat ? DebugTeleportStatus::ActiveCombat
                                      : DebugTeleportStatus::CoreRejected;
                return out;
            }
        }
        int x=r.x,y=r.y;
        const auto *data=dungeon_data(c,r.location);
        if(r.standard_entry&&(!data||!dungeon_default_cell(*data,r.floor,x,y))){out.status=DebugTeleportStatus::InvalidCoordinates;return out;}
        // Same-dungeon moves preserve facing; default entry selects an authored stair.
        state.pos.floor = uint8_t(r.floor);
        state.pos.x = uint8_t(x);
        state.pos.y = uint8_t(y);
        // Do not let a superficially successful command escape as Applied.
        // The dungeon renderer derives directly from this authoritative state.
        if (!state.active || !c.dungeon || state.pos.dungeon != r.location ||
            state.pos.floor != uint8_t(r.floor) || state.pos.x != uint8_t(x) ||
            state.pos.y != uint8_t(y) || state.pos.x > 7 || state.pos.y > 7) {
            out.status = DebugTeleportStatus::CoreRejected;
            return out;
        }
        emit_map_changed(c);
        return out;
    }
    }
    return result(DebugTeleportStatus::InvalidDestination);
}

} // namespace openu5
