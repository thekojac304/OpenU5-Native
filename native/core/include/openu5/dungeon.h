#pragma once
#include "commands.h"
namespace openu5 {
struct DungeonEncounters;
// Facing order is the dungeon's N/E/S/W order, not Direction's order.
enum class DungeonFacing : uint8_t { North, East, South, West };
struct DungeonPosition {
    uint8_t dungeon = 33, floor = 0, x = 1, y = 1;
    DungeonFacing facing = DungeonFacing::South;
};
struct DungeonWanderer {
    uint8_t bank = 0, type = 255, x = 255, y = 255, floor = 0, attr = 0;
    bool hidden = false;
    uint8_t prev_x = 255, prev_y = 255;
};
struct DungeonData {
    uint8_t location = 33;
    uint8_t cells[512]{};
};
// Caller-owned session, exactly like Game.dungeonState; world.position remains
// the surface return context. Cells are a mutable copy of the extracted map.
struct DungeonState {
    DungeonPosition pos{};
    DungeonWanderer wanderer{};
    uint8_t cells[512]{}, revealed[64]{};
    uint8_t quickness_toggle = 0;
    bool active = false;
};
enum class DungeonEventKind : uint8_t {
    Message,
    Damage,
    Room,
    ExitSurface,
    ExitUnderworld,
    Moved,
    Turned,
    FloorChanged,
    Corridor,
    Sfx,
    DamageScript,
    Loot,
    Error
};
struct DungeonEvent {
    DungeonEventKind kind = DungeonEventKind::Message;
    const char *text = nullptr;
    int32_t value = -1, member = -1;
};
struct DungeonSink {
    void *context = nullptr;
    void (*emit)(void *, const DungeonEvent &) = nullptr;
};
enum class DungeonAction : uint8_t {
    Forward,
    Back,
    Left,
    Right,
    TurnAround,
    Klimb,
    Pass,
    Attack,
    MagicUp,
    MagicDown,
    Tick,
    Open,
    Get,
    Jimmy,
    Drink,
    Search
};
uint8_t dungeon_cell(const DungeonState &, int floor, int x, int y);
int dungeon_room_map(int location, int room);
bool dungeon_room_cleared(const GameState &, int location, int room);
void dungeon_mark_room(GameState &, DungeonState &, int floor, int x, int y);
void dungeon_load(GameState &, DungeonState &, const DungeonData &, bool underworld);
void dungeon_respawn(GameState &, DungeonState &);
bool dungeon_klimb_choice(const GameState &, const DungeonState &);
// dir: 0/default or -1/up, 1/down, 2/cancel. Low-level rules: no clock advance.
void dungeon_action(GameState &, TurnState &, DungeonState &, DungeonAction, DungeonSink,
                    int dir = 0, int member = -1);
struct DungeonScratch {
    DungeonEvent events[160]{};
    size_t count = 0;
};
struct DungeonContext {
    DungeonState &state;
    DungeonScratch &scratch; // Deferred translation is required for RNG/event order.
    const DungeonData *data = nullptr;
    size_t count = 0;
    void *context = nullptr;
    void (*start_room)(void *, int32_t map, EventSink) = nullptr;
    void (*start_corridor)(void *, bool attack, EventSink) = nullptr;
    void (*rescue_hook)(void *, EventSink) = nullptr; // Narrative condition deferred.
    DungeonPosition room_entry{};
    int8_t corridor_cause = -1; // -1 absent, 0 ambush, 1 attack.
    bool room_entry_valid = false;
    // Return true when a quest-specific entrance encounter consumed Enter.
    bool (*entry_hook)(void *, uint8_t dungeon, EventSink) = nullptr;
    DungeonEncounters *encounters = nullptr;
    CommandStatus encounter_status = CommandStatus::Success;
};
ActionResult execute_dungeon_command(CommandContext &, Command);
void exit_dungeon(CommandContext &, bool underworld);
void dungeon_combat_return(CommandContext &, int floor_delta, int escape_border, bool victory);
} // namespace openu5
