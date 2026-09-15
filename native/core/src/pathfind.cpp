#include "openu5/pathfind.h"
#include <algorithm>
#include <cstdlib>
namespace openu5 {
PathOutput find_path(const ActiveMap &m, PathPoint start, PathPoint goal, TransportMode mode, PathScratch &s,
                     PathPoint *out, size_t capacity, int32_t limit, PathBlocker blocker) {
    const int32_t w = m.geometry.width, h = m.geometry.height;
    if (!m.tiles || w <= 0 || h <= 0 || w > 256 || h > 256)
        return {PathStatus::InvalidMap, 0};
    auto norm = [&](PathPoint p) {
        if (m.geometry.wraps) {
            p.x = int16_t((p.x % w + w) % w);
            p.y = int16_t((p.y % h + h) % h);
        }
        return p;
    };
    start = norm(start);
    goal = norm(goal);
    if (start.x == goal.x && start.y == goal.y)
        return {PathStatus::Found, 0};
    if (limit > int32_t(kPathExpansions))
        return {PathStatus::Capacity, 0};
    if (!in_bounds(start.x, start.y, m.geometry) || !in_bounds(goal.x, goal.y, m.geometry))
        return {PathStatus::InvalidMap, 0};
    auto dist = [&](PathPoint p) {
        int32_t dx = std::abs(p.x - goal.x), dy = std::abs(p.y - goal.y);
        if (m.geometry.wraps) {
            dx = std::min(dx, w - dx);
            dy = std::min(dy, h - dy);
        }
        return dx + dy;
    };
    auto key = [&](PathPoint p) { return size_t(p.y) * size_t(w) + size_t(p.x); };
    s.scores.fill(65535);
    s.scores[key(start)] = 0;
    s.nodes[0] = {start, 0, uint16_t(dist(start)), 65535};
    s.open[0] = 0;
    size_t count = 1, open = 1;
    int32_t expanded = 0;
    while (open && expanded < limit) {
        size_t best = 0;
        for (size_t i = 1; i < open; ++i)
            if (s.nodes[s.open[i]].f < s.nodes[s.open[best]].f)
                best = i;
        const auto index = s.open[best];
        const auto current = s.nodes[index];
        for (size_t i = best + 1; i < open; ++i)
            s.open[i - 1] = s.open[i];
        --open;
        ++expanded;
        if (current.p.x == goal.x && current.p.y == goal.y) {
            size_t len = 0;
            for (auto i = index; s.nodes[i].parent != 65535; i = s.nodes[i].parent)
                ++len;
            if (len > capacity || (len && !out))
                return {PathStatus::Capacity, len};
            size_t pos = len;
            for (auto i = index; s.nodes[i].parent != 65535; i = s.nodes[i].parent)
                out[--pos] = s.nodes[i].p;
            return {PathStatus::Found, len};
        }
        constexpr int16_t dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
        for (size_t d = 0; d < 4; ++d) {
            auto p = norm({int16_t(current.p.x + dx[d]), int16_t(current.p.y + dy[d])});
            if (!in_bounds(p.x, p.y, m.geometry) || !is_passable(m.tile_at(p.x, p.y), mode).value)
                continue;
            if (blocker.blocked && blocker.blocked(blocker.context, p.x, p.y))
                continue;
            const auto g = uint16_t(current.g + 1);
            if (g >= s.scores[key(p)])
                continue;
            s.scores[key(p)] = g;
            s.nodes[count] = {p, g, uint16_t(g + dist(p)), index};
            s.open[open++] = uint16_t(count++);
        }
    }
    return {PathStatus::NoPath, 0};
}
bool step_direction(PathPoint from, PathPoint to, const ActiveMap &m, Direction &d) {
    int32_t dx = to.x - from.x, dy = to.y - from.y;
    if (m.geometry.wraps) {
        const int32_t w = m.geometry.width, h = m.geometry.height;
        if (2 * dx > w)
            dx -= w;
        if (2 * dx < -w)
            dx += w;
        if (2 * dy > h)
            dy -= h;
        if (2 * dy < -h)
            dy += h;
    }
    if (dx == 1 && dy == 0)
        d = Direction::East;
    else if (dx == -1 && dy == 0)
        d = Direction::West;
    else if (dx == 0 && dy == 1)
        d = Direction::South;
    else if (dx == 0 && dy == -1)
        d = Direction::North;
    else
        return false;
    return true;
}
} // namespace openu5
