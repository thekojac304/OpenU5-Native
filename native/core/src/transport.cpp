#include "openu5/transport.h"
namespace openu5 {
namespace {
bool foot(int t) { return t == 0x1c || t == 0x1d; }
bool ship(int t) { return (t & 0xf8) == 0x20; }
bool skiff(int t) { return (t & 0xfc) == 0x28; }
} // namespace
int32_t mount_face_tile(int32_t tile, Direction dir) {
    const auto base = tile & 0xfc;
    if ((base != 0x10 && base != 0x14) ||
        (dir != Direction::East && dir != Direction::West)) return tile;
    return (base == 0x10 ? 0x12 : 0x14) + (dir == Direction::West ? 1 : 0);
}
bool boardable_actor_tile(int32_t actor_tile, int32_t transport_tile) {
    const auto a = actor_tile & 0xff, t = transport_tile & 0xff;
    if (t >= 0x30 || t < 0x20)
        return (a >= 0x24 && a < 0x2c) || a == 0x1b || (a & 0xfe) == 0x10;
    if (t < 0x28) return false;
    return a >= 0x24 && a < 0x28;
}
void sink_player_ship(GameState &g, TurnState &t, Rand rand, EventSink sink) {
    auto message=[&](const char *text){GameEvent e;e.kind=GameEventKind::Message;e.text=text;if(sink.emit)sink.emit(sink.context,e);};
    message("Ship sunk!");
    if(g.ship_skiffs>0){message("Abandon ship!");t.transport_tile=0x28+(t.transport_tile&3);}
    else if(g.magic_carpets>0){message("Abandon ship!");t.transport_tile=0x14+rand(0,1);--g.magic_carpets;}
    else {message("DROWNING!!!");t.transport_tile=0;}
    g.transport=transport_mode(t.transport_tile);
}
TransportMode transport_mode(int32_t t) {
    int b = t & 0xfc;
    return b == 0x10   ? TransportMode::Horse
           : b == 0x14 ? TransportMode::Carpet
           : b == 0x28 ? TransportMode::Skiff
           : ship(t)   ? TransportMode::Ship
                       : TransportMode::Foot;
}
TransportResult board_transport(GameState &g, int32_t world, int32_t from, bool owned) {
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
TransportResult disembark_transport(GameState &g, int32_t tile, bool land, bool water, bool walkable) {
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
