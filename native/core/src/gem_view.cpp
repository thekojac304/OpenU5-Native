#include "openu5/gem_view.h"

#include "gem_category.inc"

namespace openu5 {
namespace {

/** Flood-fill blockers for the DUNGEON gem (DNGLOOK 0x0608): the cell is
 *  still drawn (the flood's border), it just never propagates past it. */
bool gem_dungeon_blocks(uint8_t type) { return type == 0xb || type == 0xc || type == 0xd; }

constexpr int kNeighborDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
constexpr int kNeighborDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

} // namespace

uint8_t gem_terrain_category(uint8_t tile) { return kGemCategoryTable[tile]; }

GemChunkOrigin gem_chunk_origin(uint8_t party_x, uint8_t party_y) {
    auto axis = [](uint8_t p) -> int32_t {
        int32_t o = int32_t(p) & 0xf0;
        if ((int32_t(p) & 0x0f) < 8) o = (o - 0x10) & 0xf0;
        return o & 0xff;
    };
    return {axis(party_x), axis(party_y)};
}

GemView build_world_gem_view(const ActiveMap &map, Position party) {
    GemView v{};
    v.dungeon = false;
    v.width = v.height = uint8_t(kGemWindow);

    int32_t origin_x = 0, origin_y = 0;
    if (map.geometry.wraps) {
        const GemChunkOrigin origin = gem_chunk_origin(party.x, party.y);
        origin_x = origin.x;
        origin_y = origin.y;
    }
    for (int row = 0; row < kGemWindow; ++row) {
        for (int col = 0; col < kGemWindow; ++col) {
            const int32_t tile = map.tile_at(origin_x + col, origin_y + row);
            v.cells[row][col].value = gem_terrain_category(tile >= 0 ? uint8_t(tile & 0xff) : 0);
        }
    }
    if (map.geometry.wraps) {
        v.marker_x = uint8_t((int32_t(party.x) - origin_x) & 0x1f);
        v.marker_y = uint8_t((int32_t(party.y) - origin_y) & 0x1f);
    } else {
        v.marker_x = party.x;
        v.marker_y = party.y;
    }
    return v;
}

GemView build_dungeon_gem_view(const DungeonState &dungeon) {
    GemView v{};
    v.dungeon = true;
    constexpr int size = kGemDungeonDisplay;
    constexpr int center = kGemDungeonCenter;
    v.width = v.height = uint8_t(size);
    v.marker_x = v.marker_y = uint8_t(center);

    const int floor = int(dungeon.pos.floor);
    const int px = int(dungeon.pos.x), py = int(dungeon.pos.y);
    auto raw_at = [&](int col, int row) -> uint8_t {
        const int wx = (px + col - center + 8) & 7;
        const int wy = (py + row - center + 8) & 7;
        return dungeon_cell(dungeon, floor, wx, wy);
    };

    bool visited[size][size] = {};
    bool reached[size][size] = {};
    int qx[size * size], qy[size * size];
    size_t head = 0, tail = 0;
    visited[center][center] = true;
    qx[tail] = center;
    qy[tail] = center;
    ++tail;
    while (head < tail) {
        const int cx = qx[head], cy = qy[head];
        ++head;
        for (int n = 0; n < 8; ++n) {
            const int nx = cx + kNeighborDx[n], ny = cy + kNeighborDy[n];
            if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
            if (visited[ny][nx]) continue;
            visited[ny][nx] = true;
            reached[ny][nx] = true;
            const uint8_t type = raw_at(nx, ny) >> 4;
            if (!gem_dungeon_blocks(type)) {
                qx[tail] = nx;
                qy[tail] = ny;
                ++tail;
            }
        }
    }
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (!reached[row][col]) {
                v.cells[row][col].value = kGemDungeonUnreached;
                continue;
            }
            const uint8_t raw = raw_at(col, row);
            v.cells[row][col].value = raw >> 4;
            v.cells[row][col].sub = raw & 0xf;
        }
    }
    return v;
}

} // namespace openu5
