#pragma once
#include "world.h"
#include <array>
namespace openu5 {
struct PathPoint {
    int16_t x = 0, y = 0;
};
enum class PathStatus : uint8_t { Found, NoPath, Capacity, InvalidMap };
// Every expansion creates at most four immutable nodes. Stale open entries
// remain in insertion order, matching TS. No allocator and no global scratch.
constexpr size_t kPathExpansions = 4000, kPathNodes = 1 + 4 * kPathExpansions;
struct PathNode {
    PathPoint p{};
    uint16_t g = 0, f = 0, parent = 65535;
};
struct PathScratch {
    std::array<PathNode, kPathNodes> nodes{};
    std::array<uint16_t, kPathNodes> open{};
    std::array<uint16_t, 65536> scores{};
};
struct PathOutput {
    PathStatus status = PathStatus::NoPath;
    size_t count = 0;
};
struct PathBlocker {
    void *context = nullptr;
    bool (*blocked)(void *, int16_t, int16_t) = nullptr;
};
PathOutput find_path(const ActiveMap &, PathPoint from, PathPoint to, TransportMode, PathScratch &,
                     PathPoint *output, size_t capacity, int32_t max_nodes = 4000, PathBlocker = {});
bool step_direction(PathPoint from, PathPoint to, const ActiveMap &, Direction &);
} // namespace openu5
