#include "../main/native_movement.h"

using openu5::Direction;
using openu5::MapGeometry;
using openu5::Position;

extern "C" int run_tests()
{
    const MapGeometry local{32, 32, false};

    // Canonical INIT position: south is Floor (0x44), so one legal step updates y only.
    Position legal{15, 15};
    const auto legal_move = openu5::resolve_foot_step(legal, local, Direction::South, 0x44);
    if (!(legal_move.moved && legal.x == 15 && legal.y == 16)) return __LINE__;

    // Canonical INIT east neighbor is TableLeft (0x94), which must not move the party.
    Position blocked{15, 15};
    const auto blocked_move = openu5::resolve_foot_step(blocked, local, Direction::East, 0x94);
    if (!(!blocked_move.moved && !blocked_move.passable &&
          blocked.x == 15 && blocked.y == 15)) return __LINE__;

    // Local maps do not wrap, and Milestone 5 deliberately does not start an exit transition.
    Position edge{0, 15};
    const auto edge_move = openu5::resolve_foot_step(edge, local, Direction::West, 0x05);
    if (!(!edge_move.target_in_bounds && !edge_move.moved && edge.x == 0 && edge.y == 15)) {
        return __LINE__;
    }

    // The same pure geometry retains the existing 256x256 overworld wrap behavior.
    Position wrapped{0, 0};
    const auto wrapped_move = openu5::resolve_foot_step(
        wrapped, MapGeometry{256, 256, true}, Direction::North, 0x05);
    if (!(wrapped_move.moved && wrapped.x == 0 && wrapped.y == 255)) return __LINE__;

    return 0;
}

#ifndef OPENU5_WASM_TEST
int main() { return run_tests(); }
#endif
