#pragma once

#include "commands.h"

namespace openu5 {

/**
 * Developer-only map picker semantics ported from game/src/debug/debugApi.ts and
 * teleportPicker.ts.  The API owns no maps and no parallel game state: all
 * metadata is enumerated from CommandContext::world / DungeonContext::data and
 * every successful operation changes CommandContext::game directly.
 */
enum class DebugDestinationKind : uint8_t { Britannia, Underworld, SmallMap, Dungeon };

struct DebugDestination {
    DebugDestinationKind kind = DebugDestinationKind::Britannia;
    uint8_t location = 0;
    // Borrowed from CommandServices::banner for named locations. Large-map
    // labels are static. May be null when the host has no presentation metadata.
    const char *name = nullptr;
};

enum class DebugTeleportStatus : uint8_t {
    Applied,
    InvalidDestination,
    InvalidFloor,
    InvalidCoordinates,
    MissingMapData,
    MissingDungeonContext,
    ActiveCombat,
    // The 1988 town-exit Y/N/Esc loop must finish before another command can
    // enter a dungeon. Developer teleport has no original-game equivalent.
    PendingQuestion,
    CoreRejected,
    // Standard-entry (Default Entrance) refusal: the resolved destination cell
    // is known and not walkable. GameState is not mutated and no MapChanged
    // event is emitted. An explicit manual coordinate (standard_entry == false)
    // is never refused this way -- see apply_debug_teleport.
    ImpassableDestination
};

struct DebugTeleportRequest {
    DebugDestinationKind kind = DebugDestinationKind::Britannia;
    uint8_t location = 0;
    int16_t floor = 0;
    int32_t x = 0, y = 0;
    // Matches DebugApi.goToLocation: small maps use (15,30), while large maps
    // retain x/y. Ignored for dungeons.
    bool standard_entry = false;
};

struct DebugTeleportResult {
    DebugTeleportStatus status = DebugTeleportStatus::InvalidDestination;
    // Picker parity: impassable destinations are reported but never blocked.
    bool passability_known = false;
    bool passable = false;
    // Small-map re-entry rebuilds browser-equivalent transient map resources.
    bool reloaded = false;
    // Debug teleports preserve the shared RNG stream, including dungeon setup.
    bool rng_may_advance = false;
};

// Stable order matches the web picker: Britannia, Underworld, small-map IDs,
// dungeon IDs. Small maps and dungeons are sorted by location ID.
size_t debug_destination_count(const CommandContext &);
Result<DebugDestination> debug_destination_at(const CommandContext &, size_t index);

// Floors are read from live map resources, sorted numerically. Dungeons expose
// their eight native levels (0..7); UI labels may present these as 1..8.
size_t debug_floor_count(const CommandContext &, DebugDestination);
Result<FloorId> debug_floor_at(const CommandContext &, DebugDestination, size_t index);

// Validation is read-only. Teleport repeats validation and is atomic on errors.
DebugTeleportResult validate_debug_teleport(const CommandContext &, const DebugTeleportRequest &);
DebugTeleportResult apply_debug_teleport(CommandContext &, const DebugTeleportRequest &);

} // namespace openu5
