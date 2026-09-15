#pragma once
#include "commands.h"
namespace openu5 {
constexpr int kCombatGrid = 11, kCombatCells = 121, kCombatActors = 22;
struct CombatPoint {
    int16_t x = 0, y = 0;
};
enum class CombatDirection : uint8_t { East, West, South, North, NE, NW, SE, SW };
enum class CombatStatus : uint8_t { Active, Dead, Fled, Sleeping, Charmed, Absorbed };
struct CombatWeapon {
    int32_t id = 255, attack = 1, range = 1;
};
struct CombatEnemy {
    const char *name = "Enemy", *group_name = "";
    int32_t index = 0, strength = 0, dexterity = 0, intelligence = 0;
    int32_t armor = 0, damage = 0, hp = 0, range = 1, treasure = 0, max_per_map = 1;
    uint16_t abilities = 0; // EnemyAbilities masks, not Redux flags.
    uint8_t move_class = 0;
    bool stationary = false;
};
struct CombatTrigger {
    int16_t tile = 0;
    CombatPoint at{}, first{}, second{};
};
struct CombatMap {
    int32_t index = 0;
    int16_t tiles[kCombatCells]{};
    CombatPoint starts[4][6]{}, units[16]{};
    uint8_t start_count[4]{}, unit_count = 0, trigger_count = 0;
    CombatTrigger triggers[8]{};
};
struct CombatActor {
    const CombatEnemy *enemy = nullptr;
    int32_t hp = 0, max_hp = 0, strength = 0, dexterity = 0, intelligence = 0;
    int32_t defense = 0, attack = 1, range = 1;
    CombatWeapon weapons[3]{};
    CombatPoint position{};
    int16_t render_tile = -1, last_attacker = -1, last_target = -1;
    uint8_t id = 0, member = 255, speed = 0, counter = 0, weapon_count = 0;
    CombatStatus status = CombatStatus::Active;
    bool fleeing = false, sleeping = false, dragged = false, charmed = false, invisible = false;
};
enum class CombatEventKind : uint8_t {
    Message,
    Echo,
    Moved,
    Attacked,
    Died,
    Turn,
    Ended,
    Projectile
};
struct CombatEvent {
    CombatEventKind kind = CombatEventKind::Message;
    const char *text = nullptr;
    int32_t actor = -1, target = -1, x = -1, y = -1, damage = -1;
    int8_t hit = -1, grazed = -1,
           dragged = -1; // -1 = absent, preserving TS optional fields.
};
struct CombatSink {
    void *context = nullptr;
    void (*emit)(void *, const CombatEvent &) = nullptr;
};
struct CombatState {
    CombatActor actors[kCombatActors]{};
    CombatMap map{};
    int16_t loot[kCombatCells]{}, chest_contents[kCombatCells]{};
    int32_t xp[kRosterCapacity]{};
    CombatWeapon queue[3]{};
    OriginalRng rng{};
    uint32_t action_count = 0;
    int32_t spoil_chests = 0;
    int16_t current = -1, escape_border = -1;
    uint8_t count = 0, scan = 0, queue_count = 0;
    bool queue_live = false, ended = false, victory = false, room = false, initialized = false;
};
struct CombatTables {
    const int32_t *attack = nullptr, *range = nullptr, *defense = nullptr,
                  *strength_range = nullptr;
    size_t count = 0;
};
struct CombatContext {
    GameState &game;
    TurnState &turn;
    CombatState &combat;
    CombatTables tables{};
    CombatSink events{};
    TraceSink trace{};
};
enum class CombatResult : uint8_t { Ok, Invalid, Unsupported, MissingMap };
// Encounter order is already rolled, exactly like CombatOpts.enemies array.
// Definition/map storage is borrowed only during init; enemy definitions remain
// borrowed.
CombatResult initialize_combat(CombatContext &, const CombatMap &, CombatDirection,
                               const CombatEnemy *const *, size_t, bool room = false);
CombatActor *current_combat_actor(CombatContext &);
bool combat_over(const CombatState &);
enum class CombatAction : uint8_t {
    Move,
    Attack,
    Pass,
    Escape,
    EscapeQuick,
    AttackCancel,
    Yield,
    EnemyStep
};
CombatResult combat_action(CombatContext &, CombatAction, int32_t x = 0, int32_t y = 0);
int32_t combat_distance(int32_t dx, int32_t dy);
// World handoff does not own maps, enemies or NPC removal. Missing maps reject
// before RNG.
struct CombatResources {
    const CombatMap *const *maps = nullptr;
    size_t map_count = 0;
    const CombatEnemy *const *enemies = nullptr;
    size_t enemy_count = 0;
    CombatTables tables{};
    void *context = nullptr;
    void (*reset_doors)(void *) = nullptr;
    void (*remove_enemy)(void *, int32_t) = nullptr;
};
CombatResult start_encounter_combat(CommandContext &, CombatState &, const CombatResources &,
                                    int32_t enemy, int32_t tile, int32_t map_override = -1,
                                    CombatDirection entry = CombatDirection::South,
                                    bool intro = true);
CombatResult finish_encounter_combat(CommandContext &, CombatState &);
} // namespace openu5
