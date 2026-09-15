#pragma once
#include "types.h"
namespace openu5 {
constexpr uint16_t kLargeMapSize = 256, kSmallMapSize = 32;
constexpr int32_t kOffMap = -1;
struct MapGeometry { uint16_t width = 0, height = 0; bool wraps = false; };
enum class MapKind : uint8_t { Overworld, Underworld, Small };
int32_t wrap_coord(int32_t value);
bool in_bounds(int32_t x, int32_t y, MapGeometry map);
bool target_for_step(Position position, MapGeometry map, Direction direction, Position &target);
// Borrowed row-major bytes. No core allocation or ESP-IDF dependency.
struct MapData { MapId id{}; const uint8_t *tiles = nullptr; size_t size = 0; };
struct ActiveMap {
    MapId id{};
    MapKind kind = MapKind::Small;
    MapGeometry geometry{};
    const uint8_t *tiles = nullptr;
    int32_t edge_fill_tile = kOffMap;
    int32_t tile_at(int32_t x, int32_t y) const;
};
struct WorldData {
    const uint8_t *overworld = nullptr, *underworld = nullptr;
    size_t overworld_size = 0, underworld_size = 0;
    const MapData *small_maps = nullptr;
    size_t small_map_count = 0;
};
Result<ActiveMap> get_active_map(const WorldData &world, MapId id);
// Basic properties only; presentation and combat properties remain untranslated.
struct TileProperties {
    bool walkable = false, horse = false, skiff = false, ship = false;
};
Result<TileProperties> tile_properties(int32_t tile);
Result<bool> is_passable(int32_t tile, TransportMode transport);
bool is_walkable_tile(uint8_t tile);
uint8_t terrain_speed_class(int32_t tile);
}
