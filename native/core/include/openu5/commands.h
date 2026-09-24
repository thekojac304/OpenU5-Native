#pragma once
#include "inventory.h"
#include "transitions.h"
namespace openu5 {
struct RestServices;
struct CombatContext;
struct DungeonContext;
struct TransportServices;
struct DialogueServices;
struct ShopServices;
struct ShrineServices;
struct ShrineInput;
struct QuestWorldServices;
struct BlackthornSession;
struct BlackthornSceneServices;
struct OutdoorServices;
struct LookServices;
struct WorldTerrain;
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
    EnterDungeon, DungeonCommand, Board, Disembark, CombatKlimb, CombatGet, CombatOpen, CombatSearch,
    Talk, BeginConversation, DialogueText, DialogueYes, DialogueNo, EndConversation, ShopAction,
    ShrineAction, Yell, HarpsichordNote, Get, Search, UseMoonstone, BlackthornAction, YellSails, TrollToll,
    Open, Jimmy, Push, Look, CrystalBall, DropCoin, MakeWish, Attack, Fire, Mix,
    ViewGem, AfterGemView, NewOrder, SetActivePlayer, CampAcknowledge
};
struct Command {
    CommandKind kind = CommandKind::Pass;
    Direction direction = Direction::North;
    bool has_direction = false;
    int16_t member = -1, item = -1, hours = 0;
    int16_t caster = -1; // World Cast: explicit caster; member is the target, hours is gate phase.
    uint8_t reagent_mask=0; // Mix: bit r marks selected reagent r; hours is quantity.
    EquipSlot slot = EquipSlot::None;
    int16_t combat_x = 0, combat_y = 0; // Move/escape: CombatDirection ordinal in x.
    bool has_target = false, cancel_target = false; // Cast: item=SpellId, member=party target.
    bool watch_requested = false; // Rest: guard picker was attempted, even if cancelled.
    const char16_t *text = nullptr;
    size_t text_length = 0; // Borrowed UTF-16, no UI length limit. BeginConversation: member=NPC slot.
    const ShrineInput *shrine = nullptr;
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
    struct CampAdvance {
        enum class Phase : uint8_t { None, MemberKey, KarmaKey };
        Phase phase = Phase::None;
        uint8_t slot = 0; // Member awaiting a key, then the next roster slot.
    } camp_advance{};
    struct Door { MapId map{}; int32_t x=0,y=0,tile=184,turns=0; } door;
    TurnPhases town_phases{}, outdoor_phases{};
    int16_t town_location = -1;
    bool awaiting_exit = false;
    bool awaiting_troll = false;
    int32_t troll_toll = 0, troll_under_party = 0, troll_x = 0, troll_y = 0;
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
    // Session contexts are supplied by the existing world/combat/dungeon owners.
    bool combat = false, dungeon = false;
    const RestServices *rest_services = nullptr;
    CombatContext *combat_context = nullptr; // Caller-owned arena and resource tables.
    DungeonContext *dungeon_context = nullptr;
    const TransportServices *transport_services = nullptr;
    DialogueServices *dialogue_services = nullptr;
    ShopServices *shop_services = nullptr;
    ShrineServices *shrine_services = nullptr;
    QuestWorldServices *quest_world = nullptr;
    BlackthornSession *blackthorn = nullptr;
    // #324 / R-32. Absent (or with no packed room) the capture emits the
    // same text-only stream it always did -- the discriminant that keeps
    // every existing parity fixture and pure harness byte-identical.
    BlackthornSceneServices *blackthorn_scene = nullptr;
    OutdoorServices *outdoor = nullptr;
    const LookServices *look = nullptr;
    WorldTerrain *terrain = nullptr;
};
// Foot world movement plus optional combat, dungeon, dialogue, shop and quest
// services. See COMMANDS.md and QUESTS.md for resource and domain contracts.
// NPC/path scratch is caller-owned. Quest text construction may allocate.
ActionResult execute_command(CommandContext &, Command);
// New world-command dispatch additionally applies Game.commandDrunkIntercept
// and townAutoSleepTurn in main.ts semantic order. Nested responses bypass them.
ActionResult dispatch_world_command(CommandContext &, Command);
} // namespace openu5
