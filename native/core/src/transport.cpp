#include "openu5/transport.h"
namespace openu5 {
namespace {
bool foot(int t) { return t == 0x1c || t == 0x1d; }
bool ship(int t) { return (t & 0xf8) == 0x20; }
bool skiff(int t) { return (t & 0xfc) == 0x28; }
} // namespace
TransportMode transport_mode(int t) {
    int b = t & 0xfc;
    return b == 0x10   ? TransportMode::Horse
           : b == 0x14 ? TransportMode::Carpet
           : b == 0x28 ? TransportMode::Skiff
           : ship(t)   ? TransportMode::Ship
                       : TransportMode::Foot;
}
TransportResult board_transport(GameState &g, int world, int from, bool owned) {
    TransportResult r;
    r.message = "What?";
    int loc = g.position.map.location;
    if (loc > 32 && loc < 41) {
        r.message = "Not here!";
        return r;
    }
    if ((world & 0xfe) == 0x10) {
        if (owned) {
            r.message = "\"Nay!\"\n";
            return r;
        }
        if (!foot(from)) {
            r.message = "On foot";
            return r;
        }
        r.ok = true;
        r.message = "horse";
        r.tile = world + 2;
    } else if (world == 0x1b || skiff(world)) {
        if (!foot(from)) {
            r.message = "On foot";
            return r;
        }
        r.ok = true;
        r.message = world == 0x1b ? "carpet" : "skiff";
        r.tile = world == 0x1b ? 0x14 : world;
    } else if (ship(world)) {
        if ((from & 0xfe) != 0x14 && !foot(from) && !skiff(from)) {
            r.message = "On foot";
            return r;
        }
        r.damaged_warning = g.ship_hull < 10;
        if ((from & 0xfe) == 0x14)
            ++g.magic_carpets;
        if (skiff(from))
            ++g.ship_skiffs;
        r.skiff_warning = g.ship_skiffs == 0;
        r.ok = true;
        r.message = "Ship";
        r.tile = world;
    }
    return r;
}
TransportResult disembark_transport(GameState &g, int tile, bool land, bool water, bool walkable) {
    TransportResult r;
    switch (tile & 0xfc) {
    case 0x10:
        r = {true, "horse!", 0x1c, tile - 2};
        break;
    case 0x14:
        if (!land && !walkable) {
            r.message = "No land nearby!";
            break;
        }
        r = {true, "carpet!", 0x1c, 0x1b};
        break;
    case 0x20:
        r.message = "Under sail!";
        break;
    case 0x24:
        if (land)
            r = {true, "ship!", 0x1c, -1, tile};
        else if (g.ship_skiffs > 0) {
            --g.ship_skiffs;
            r = {true, "ship!", tile + 4, -1, tile};
        } else if (g.magic_carpets > 0) {
            --g.magic_carpets;
            r = {true, "ship!", 0x14, -1, tile};
        } else
            r.message = "No skiffs on board!";
        break;
    case 0x28:
        if (!land)
            r.message = "No land nearby!";
        else if (water)
            r.message = "Not here!";
        else
            r = {true, "skiff!", 0x1c, tile};
        break;
    default:
        break;
    }
    return r;
}
} // namespace openu5
