#pragma once
#include "commands.h"
namespace openu5 {
struct TransportResult {
    bool ok = false;
    const char *message = "what?";
    int32_t tile = -1, drop_tile = -1, parked_ship_tile = -1;
    bool damaged_warning = false, skiff_warning = false;
};
TransportResult board_transport(GameState &, int32_t world_tile, int32_t from_tile,
                                bool horse_owned = false);
TransportResult disembark_transport(GameState &, int32_t tile, bool land_nearby,
                                    bool water_under_skiff = false, bool walkable_under = false);
TransportMode transport_mode(int32_t tile);
int32_t mount_face_tile(int32_t tile, Direction);
bool boardable_actor_tile(int32_t actor_tile, int32_t transport_tile);
void sink_player_ship(GameState &,TurnState &,Rand,EventSink);
// Effective tile/object access belongs to the existing world owner. Mutators
// must be infallible after reserve succeeds; no parallel object pool is kept.
struct TransportServices {
    void *context = nullptr;
    int32_t (*tile_at)(void *, int32_t, int32_t) = nullptr;
    bool (*horse_owned)(void *, WorldPosition) = nullptr;
    bool (*ship_at)(void *, WorldPosition, int32_t &hull, int32_t &skiffs) = nullptr;
    bool (*reserve)(void *, bool parked_ship) = nullptr;
    void (*remove_boarded)(void *, WorldPosition, int32_t old_tile) = nullptr;
    void (*drop)(void *, WorldPosition, int32_t banked_tile) = nullptr;
    void (*park_ship)(void *, WorldPosition, int32_t banked_tile, int32_t hull,
                      int32_t skiffs) = nullptr;
};
} // namespace openu5
