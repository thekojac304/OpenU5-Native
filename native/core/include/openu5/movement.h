#pragma once
#include "world.h"
#include "state.h"
namespace openu5 {
struct CombatEvent;
struct DialogueEvent;
struct ShopEvent;
struct EndgameScript;
struct RefugeScript;
struct BlackthornSceneScript;
struct ZodiacView;
struct TrollSneakScript {
    struct Beat { const char *text = nullptr; int8_t pause_units = -1; bool append = false; };
    Beat beats[32]{};
    uint8_t count = 0;
};
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
// Reference resolveStep for foot/mount/carpet (naval commands have their own turn).
// actor_tile is the outdoor enemy owner's tile, zero when unoccupied.
Result<StepGeometry> resolve_world_step(GameState &, const ActiveMap &, Direction,
                                        int32_t transport_tile, int32_t actor_tile = 0);
// Ordered semantic event subset. See commands.h / COMMANDS.md for payloads.
struct NpcActor;
enum class GameEventKind : uint8_t { Message, Moved, MapChanged, PartyChanged, TownExitPrompt,
    WalkEcho, Sfx, PoisonTick, Quake, NeedsDirection, CombatStarted, CombatEnded, Combat, DungeonEntered, DungeonExited, Dialogue, Shop,
    ShrineVisitPrompt, ShrineRestorePrompt, ShrineDonatePrompt, ShrineKeyWait, RitualInvert, CellExplosion, GameWon, Endgame, BlackthornPrompt, GuardPasswordPrompt, GuardTributePrompt, GuardArrestPrompt, NpcInitiatesTalk, NpcInitiatesShop, Refuge, TrollSneak, TrollTollPrompt, CrystalBallPrompt, WellDropPrompt, FountainDrinkPrompt, WellWishPrompt, Zodiac, GemView, MapReveal, CellProjectile, MagicCeremony, BlackthornScene, BedViewportFill, BedStatusRefresh, CampStatusRefresh, CampKeyWait };
struct GameEvent {
    GameEventKind kind = GameEventKind::Moved;
    StepMessage message = StepMessage::None; // Legacy movement vocabulary.
    int16_t dungeon_id = -1; // DungeonEntered payload; -1 = absent.
    const char *text = nullptr; // Borrowed during synchronous delivery; also SFX id / command name.
    uint8_t slots[6]{};
    uint8_t slot_count = 0;
    const CombatEvent *combat = nullptr; // Borrowed synchronous CombatEvent envelope.
    const DialogueEvent *dialogue = nullptr;
    const ShopEvent *shop = nullptr;
    struct CellFx { int16_t dx=0,dy=0,bursts=0,pre_delay_units=0,under_tile=0; } cell_fx{};
    int32_t note=0;
    const EndgameScript *endgame=nullptr;
    const NpcActor *npc=nullptr; // Borrowed identity for semantic conversation/shop initiation.
    const RefugeScript *refuge=nullptr;
    const TrollSneakScript *troll_sneak=nullptr;
    // Borrowed for the synchronous delivery only, like every other payload
    // here: the Blackthorn capture scene segment (#324 / R-32). A consumer
    // that outlives the emit must copy the beats it needs.
    const BlackthornSceneScript *blackthorn_scene=nullptr;
    const ZodiacView *zodiac=nullptr;
    const uint8_t *sign_raw=nullptr;
    size_t sign_raw_size=0;
    bool sign=false, gem_from_crystal=false;
    struct Projectile {int16_t from_dx=0,from_dy=0,to_dx=0,to_dy=0;} projectile;
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
