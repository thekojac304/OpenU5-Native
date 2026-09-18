#include "openu5/world.h"
#include "tile_flags.inc"
namespace openu5 {
int32_t wrap_coord(int32_t value) { return ((value % 256) + 256) % 256; }
bool in_bounds(int32_t x, int32_t y, MapGeometry m) {
    return x >= 0 && y >= 0 && x < m.width && y < m.height;
}
int32_t ActiveMap::tile_at(int32_t x, int32_t y) const {
    if (geometry.wraps) { x = wrap_coord(x); y = wrap_coord(y); }
    else if (!in_bounds(x, y, geometry)) return kOffMap;
    if (!tiles && !resolve_tile) return kOffMap;
    const int32_t tile=tiles?tiles[size_t(y) * geometry.width + size_t(x)]:kOffMap;
    return resolve_tile?resolve_tile(resolve_context,id,x,y,tile):tile;
}
Result<ActiveMap> get_active_map(const WorldData &w, MapId id) {
    ActiveMap m{};
    m.id = id;
    if (id.location == 0) {
        const bool under = id.floor == 255;
        m.kind = under ? MapKind::Underworld : MapKind::Overworld;
        m.geometry = {256, 256, true};
        m.tiles = under ? w.underworld : w.overworld;
        const size_t size = under ? w.underworld_size : w.overworld_size;
        if (!m.tiles || size != 65536) return {{}, Error::InvalidMap};
    } else {
        const MapData *data = nullptr;
        for (size_t i = 0; i < w.small_map_count; ++i) {
            if (w.small_maps[i].id.location == id.location && w.small_maps[i].id.floor == id.floor) {
                data = &w.small_maps[i]; break;
            }
        }
        if (!data) return {{}, Error::MissingMap};
        if (!data->tiles || data->size != 1024) return {{}, Error::InvalidMap};
        m.geometry = {32, 32, false};
        m.tiles = data->tiles;
        m.edge_fill_tile = data->tiles[1023];
    }
    return {m, Error::None};
}
Result<TileProperties> tile_properties(int32_t tile) {
    if (tile < 0 || size_t(tile) >= sizeof(kTileFlags) || kTileFlags[tile] == 255)
        return {{}, Error::UnknownTile};
    const uint8_t f = kTileFlags[tile];
    return {{bool(f & 1), bool(f & 2), bool(f & 4), bool(f & 8)}, Error::None};
}
Result<bool> is_passable(int32_t tile, TransportMode mode) {
    if (tile < 0) return {false, Error::None};
    const auto p = tile_properties(tile);
    if (p.error != Error::None) return {false, p.error};
    switch (mode) {
    case TransportMode::Foot: return {p.value.walkable, Error::None};
    case TransportMode::Horse: return {p.value.horse, Error::None};
    case TransportMode::Carpet: return {tile < 4 || (tile & 0xf0) == 0x60 || p.value.walkable, Error::None};
    case TransportMode::Skiff: return {p.value.skiff, Error::None};
    case TransportMode::Ship: return {p.value.ship, Error::None};
    }
    return {false, Error::InvalidRange};
}
uint8_t terrain_speed_class(int32_t tile) {
    if (tile == 4 || tile == 6 || tile == 7 || tile == 8 || tile == 30 || tile == 31) return 1;
    return tile >= 9 && tile <= 15 ? 2 : 0;
}
}
