#include "openu5/movement.h"
#include "openu5/transport.h"

namespace openu5 {
namespace {

// One bit per tile, MSB first; 1 means walkable. Generated from the final
// TILE_INFO walkable values in game/src/core/tiles.ts (including its documented
// canonical overrides), rather than independently reinterpreting terrain names.
constexpr uint8_t kWalkableTiles[32] = {
    0x8f, 0xf3, 0xff, 0xf7, 0xfe, 0x0c, 0xff, 0x42,
    0x8d, 0xc0, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00,
    0x03, 0x09, 0xf0, 0x00, 0x00, 0x38, 0x00, 0x08,
    0x0f, 0xc0, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x40,
};

int wrap(int value, int limit)
{
    const int remainder = value % limit;
    return remainder < 0 ? remainder + limit : remainder;
}

}  // namespace

bool is_walkable_tile(uint8_t tile_id)
{
    return (kWalkableTiles[tile_id >> 3] & (0x80U >> (tile_id & 7U))) != 0;
}

MoveReport resolve_foot_step(Position &position, MapGeometry map, Direction direction,
                             uint8_t target_tile)
{
    MoveReport report{};
    report.old_position = position;
    report.resulting_position = position;
    report.attempted_x = position.x;
    report.attempted_y = position.y;
    switch (direction) {
    case Direction::North: --report.attempted_y; break;
    case Direction::South: ++report.attempted_y; break;
    case Direction::East: ++report.attempted_x; break;
    case Direction::West: --report.attempted_x; break;
    }
    Position target{};
    if (!target_for_step(position, map, direction, target)) return report;
    report.target_in_bounds = true;
    report.target_position = target;
    report.attempted_x = target.x;
    report.attempted_y = target.y;
    report.target_tile = target_tile;
    report.passable = is_walkable_tile(target_tile);
    if (report.passable) {
        position = report.target_position;
        report.resulting_position = position;
        report.moved = true;
    }
    return report;
}

bool target_for_step(Position position, MapGeometry map, Direction direction,
                     Position &target)
{
    const auto delta = direction_delta(direction);
    const int dx = delta.dx, dy = delta.dy;

    int target_x = static_cast<int>(position.x) + dx;
    int target_y = static_cast<int>(position.y) + dy;
    if (map.width == 0 || map.height == 0 || map.width > 256 || map.height > 256) return false;
    if (map.wraps) {
        target_x = wrap(target_x, map.width);
        target_y = wrap(target_y, map.height);
    } else if (target_x < 0 || target_y < 0 || target_x >= map.width ||
               target_y >= map.height) {
        // Milestone 5 deliberately stops at the local-map boundary. The full
        // leave-map prompt and transition belong to a later command/turn milestone.
        return false;
    }
    target = {static_cast<uint8_t>(target_x), static_cast<uint8_t>(target_y)};
    return true;
}

const char *direction_name(Direction direction)
{
    switch (direction) {
    case Direction::North: return "north";
    case Direction::South: return "south";
    case Direction::East: return "east";
    case Direction::West: return "west";
    }
    return "unknown";
}

const char *step_message_text(StepMessage message) {
    switch (message) {
    case StepMessage::None: return nullptr;
    case StepMessage::Blocked: return "Blocked!";
    case StepMessage::SlowProgress: return "Slow progress!";
    case StepMessage::VerySlow: return "Very slow!";
    }
    return nullptr;
}

Result<StepGeometry> resolve_unoccupied_foot_step(GameState &state, const ActiveMap &map, Direction direction) {
    if (state.transport != TransportMode::Foot) return {{}, Error::InvalidRange};
    return resolve_world_step(state, map, direction, 28);
}

Result<StepGeometry> resolve_world_step(GameState &state, const ActiveMap &map, Direction direction,
                                        int32_t transport_tile, int32_t actor_tile) {
    StepGeometry r{};
    if (!map.tiles || (map.geometry.wraps && (map.geometry.width != 256 || map.geometry.height != 256)) ||
        map.geometry.width == 0 || map.geometry.height == 0 || map.geometry.width > 256 || map.geometry.height > 256)
        return {r, Error::InvalidMap};
    Position target{};
    if (!target_for_step(state.position.xy, map.geometry, direction, target)) {
        // Reference uses constant grass filler even in a void basement.
        r.exited_map = is_passable(5, state.transport).value;
        if (!r.exited_map) {
            r.blocked = true;
            r.message = StepMessage::Blocked;
            r.minutes = 1;
        }
        return {r, Error::None};
    }
    const int32_t tile = map.tile_at(target.x, target.y);
    const bool occupied = map.geometry.wraps && actor_tile != 0 &&
                          !boardable_actor_tile(actor_tile, transport_tile);
    const auto passable = occupied ? Result<bool>{false, Error::None} : is_passable(tile, state.transport);
    if (passable.error != Error::None) return {r, passable.error};
    if (!passable.value) {
        r.blocked = true;
        r.message = StepMessage::Blocked;
        r.minutes = map.geometry.wraps ? 0 : 1;
        r.on_cactus = map.geometry.wraps && tile == 0x2f;
        return {r, Error::None};
    }
    state.position.xy = target;
    r.moved = true;
    r.minutes = map.geometry.wraps ? 2 : 1;
    if (map.geometry.wraps) {
        r.speed_class = terrain_speed_class(tile);
        r.message = r.speed_class == 1 ? StepMessage::SlowProgress :
                    r.speed_class == 2 ? StepMessage::VerySlow : StepMessage::None;
        r.on_bridge = (tile == 0x6a || tile == 0x6b) && state.transport == TransportMode::Foot;
        r.on_swamp = tile == 4 && state.transport == TransportMode::Foot;
    }
    return {r, Error::None};
}

CommandResult apply_movement_slice(GameState &state, MapGeometry map, MoveAction action, uint8_t tile) {
    CommandResult result{};
    result.movement = resolve_foot_step(state.position.xy, map, action.direction, tile);
    return result;
}
}  // namespace openu5
