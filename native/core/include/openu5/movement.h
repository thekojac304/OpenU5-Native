#pragma once
#include "world.h"
#include "state.h"
namespace openu5 {
struct CombatEvent;
struct MoveReport {
    Position old_position{}, target_position{}, resulting_position{};
    int16_t attempted_x = 0, attempted_y = 0;
    uint8_t target_tile = 0xff;
    bool target_in_bounds = false, passable = false, moved = false;
};
// Preserved M5 policy: no exits, clock, turns, RNG, or Game events.
MoveReport resolve_foot_step(Position &position, MapGeometry map, Direction direction, uint8_t target_tile);
enum class StepMessage : uint8_t { None, Blocked, SlowProgress, VerySlow };
const char *step_message_text(StepMessage message);
struct StepGeometry {
    bool moved = false, blocked = false, exited_map = false;
    StepMessage message = StepMessage::None;
    uint8_t minutes = 0, speed_class = 0;
    bool on_bridge = false, on_swamp = false, on_cactus = false;
};
// Exact resolveStep projection for transport=foot, actorTile=0. No full turn.
Result<StepGeometry> resolve_unoccupied_foot_step(GameState &state, const ActiveMap &map, Direction direction);
// Ordered semantic event subset. See commands.h / COMMANDS.md for payloads.
enum class GameEventKind : uint8_t { Message, Moved, MapChanged, PartyChanged, TownExitPrompt,
    WalkEcho, Sfx, PoisonTick, Quake, NeedsDirection, CombatStarted, CombatEnded, Combat };
struct GameEvent {
    GameEventKind kind = GameEventKind::Moved;
    StepMessage message = StepMessage::None; // Legacy movement vocabulary.
    const char *text = nullptr; // Borrowed during synchronous delivery; also SFX id / command name.
    uint8_t slots[6]{};
    uint8_t slot_count = 0;
    const CombatEvent *combat = nullptr; // Borrowed synchronous CombatEvent envelope.
};
struct MoveAction { Direction direction = Direction::North; };
// Preserve the existing device-slice result layout; it never emits events.
struct MovementSliceEvent { GameEventKind kind = GameEventKind::Moved; StepMessage message = StepMessage::None; };
struct CommandResult {
    MoveReport movement{};
    MovementSliceEvent events[1]{};
    uint8_t event_count = 0; // The existing slice emits no Game events.
};
CommandResult apply_movement_slice(GameState &state, MapGeometry map, MoveAction action, uint8_t target_tile);
}
