#pragma once
#include "inventory.h"
#include "transitions.h"
namespace openu5 {
struct RestServices;
struct CombatContext;
struct DungeonContext;
struct TransportServices;
struct DialogueServices;
// Semantic intent only. Exit is the accepted town-boundary response.
enum class CommandKind : uint8_t {
    Move,
    Pass,
    Enter,
    Exit,
    DeclineExit,
    Klimb,
    KlimbCancel,
    AutoSleep,
    Ready,
    Unready,
    Ignite,
    Rest,
    RestCancel,
    UseItem,
    CombatMove, CombatAttack, CombatPass, CombatEscape, CombatEscapeQuick,
    CombatAttackCancel, CombatYield, CombatEnemyStep, Cast,
    EnterDungeon, DungeonCommand, Board, Disembark, CombatKlimb, CombatGet, CombatOpen,
    Talk, BeginConversation, DialogueText, DialogueYes, DialogueNo, EndConversation
};
struct Command {
    CommandKind kind = CommandKind::Pass;
    Direction direction = Direction::North;
    bool has_direction = false;
    int16_t member = -1, item = -1, hours = 0;
    EquipSlot slot = EquipSlot::None;
    int16_t combat_x = 0, combat_y = 0; // Move/escape: CombatDirection ordinal in x.
    bool has_target = false, cancel_target = false; // Cast: item=SpellId, member=party target.
    const char16_t *text = nullptr;
    size_t text_length = 0; // Borrowed UTF-16, no UI length limit. BeginConversation: member=NPC slot.
};
enum class CommandStatus : uint8_t {
    Success,
    Rejected,
    NoOp,
    AwaitingResponse,
    Unsupported,
    InvalidContext,
    CoreError,
    NeedsStorage
};
struct ActionResult {
    CommandStatus status = CommandStatus::Success;
    Error error = Error::None;
    ActorError actor_error = ActorError::None;
    uint32_t world_turns = 0, event_count = 0;
    int64_t turns = 0;
    ItemResult item{};
    int32_t pending_camp_enemy = -1; // Combat handoff; no attack resolution here.
};
struct EventSink {
    void *context = nullptr;
    void (*emit)(void *, const GameEvent &) = nullptr;
};
// Ordered extension seams; implementations belong to their future core
// subsystem, never the renderer. Returning true pauses the tail at
// Capture/Tribute.
enum class CommandEffect : uint8_t {
    Doors,
    RefreshHourTiles,
    Refuge,
    Capture,
    Tribute,
    WaterfallUnder,
    Waterfall,
    ShrineGuardian,
    Moongate,
    ShrineEntry
};
struct CommandServices {
    void *context = nullptr;
    bool (*effect)(void *, CommandEffect, EventSink) = nullptr;
    void (*reload)(void *, ReloadEffect, uint8_t, EventSink) = nullptr;
    const char *(*banner)(void *, uint8_t) = nullptr;
};
struct CommandState {
    TurnPhases town_phases{}, outdoor_phases{};
    int16_t town_location = -1;
    bool awaiting_exit = false;
    int32_t pending_camp_enemy = -1; // Future combat owner clears after accepting handoff.
};
struct NpcLocationData {
    uint8_t location = 0;
    const NpcSlot *slots = nullptr;
    size_t count = 0;
    uint32_t dead_slots = 0;
};
struct CommandContext {
    GameState &game;
    TurnState &turn;
    TravelState &travel;
    CommandState &commands;
    const WorldData &world;
    LocationTable locations{};
    NpcActors *actors = nullptr;
    NpcScanGrid *npc_scratch = nullptr;
    const NpcLocationData *npc_data = nullptr;
    size_t npc_data_count = 0;
    const SkyRefresh *sky = nullptr;
    CommandServices services{};
    EventSink events{};
    TraceSink rng_trace{}; // Optional read-only observer of the shared live stream.
    // Full enemy/combat/quest/dungeon contexts require later translations.
    bool combat = false, dungeon = false;
    const RestServices *rest_services = nullptr;
    CombatContext *combat_context = nullptr; // Caller-owned arena and resource tables.
    DungeonContext *dungeon_context = nullptr;
    const TransportServices *transport_services = nullptr;
    DialogueServices *dialogue_services = nullptr;
};
// Supported projection: foot movement, no outdoor actors, no bridge ambush,
// trapdoor/quest triggers or combat resources. See COMMANDS.md for exact seams.
// No allocation; NPC scratch is caller-owned. Never allocates general A*
// scratch.
ActionResult execute_command(CommandContext &, Command);
// New world-command dispatch additionally applies Game.commandDrunkIntercept
// and townAutoSleepTurn in main.ts semantic order. Nested responses bypass them.
ActionResult dispatch_world_command(CommandContext &, Command);
} // namespace openu5
