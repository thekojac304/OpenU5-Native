#pragma once
#include <stdint.h>
#include <stddef.h>

namespace openu5 {
enum class Direction : uint8_t { North, South, East, West };
struct Delta { int8_t dx, dy; };
constexpr Delta direction_delta(Direction d) {
    switch (d) {
    case Direction::North: return {0, -1};
    case Direction::South: return {0, 1};
    case Direction::East: return {1, 0};
    case Direction::West: return {-1, 0};
    }
    return {0, 0};
}
const char *direction_name(Direction direction);
struct Position { uint8_t x = 0, y = 0; };
using LocationId = uint8_t;
// Small-map basement floors may be -1; large-map underworld is explicitly 255.
using FloorId = int16_t;
struct MapId { LocationId location = 0; FloorId floor = 0; };
struct WorldPosition { Position xy{}; MapId map{}; };
enum class TransportMode : uint8_t { Foot, Horse, Carpet, Skiff, Ship };
enum class Error : uint8_t { None, InvalidRange, UnknownTile, InvalidMap, MissingMap };
template<class T> struct Result { T value{}; Error error = Error::None; };
}
