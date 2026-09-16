#pragma once
#include "commands.h"
#include "magic.h"
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
    int16_t render_tile = -1;
    int32_t last_attacker = -1, last_target = -1, id = 0;
    uint8_t member = 255, speed = 0, counter = 0, weapon_count = 0;
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
    Projectile,
    Quake,
    LineSpray
};
struct CombatEvent {
    CombatEventKind kind = CombatEventKind::Message;
    const char *text = nullptr;
    int32_t actor = -1, target = -1, x = -1, y = -1, damage = -1;
    int16_t mode = -1;
    int8_t hit = -1, grazed = -1,
           dragged = -1; // -1 = absent, preserving TS optional fields.
};
struct CombatSink {
    void *context = nullptr;
    void (*emit)(void *, const CombatEvent &) = nullptr;
};
// Stable addresses even when new actors are appended. Overflow storage belongs
// to the caller and must remain alive for the battle. TS has no actor-count
// cap.
struct CombatActorStorage {
    CombatActor initial[kCombatActors]{};
    CombatActor *overflow = nullptr;
    int32_t overflow_capacity = 0;
    CombatActor &operator[](int i) {
        return i < kCombatActors ? initial[i] : overflow[i - kCombatActors];
    }
    const CombatActor &operator[](int i) const {
        return i < kCombatActors ? initial[i] : overflow[i - kCombatActors];
    }
    int32_t capacity() const { return kCombatActors + (overflow ? overflow_capacity : 0); }
};
struct CombatField {
    CombatPoint position{};
    int16_t tile = 0;
};
struct CombatLootPile { CombatPoint position{}; int16_t id = 0, quantity = 0; };
struct CombatPileStorage {
    CombatLootPile initial[16]{};
    CombatLootPile *overflow = nullptr;
    int32_t overflow_capacity = 0;
    CombatLootPile &operator[](int i){return i<16?initial[i]:overflow[i-16];}
    const CombatLootPile &operator[](int i)const{return i<16?initial[i]:overflow[i-16];}
    int32_t capacity()const{return 16+(overflow?overflow_capacity:0);}
};
struct CombatState {
    CombatActorStorage actors{};
    CombatMap map{};
    int16_t loot[kCombatCells]{}, chest_contents[kCombatCells]{};
    int32_t xp[kRosterCapacity]{};
    CombatWeapon queue[3]{};
    OriginalRng rng{};
    uint32_t action_count = 0;
    int32_t spoil_chests = 0;
    int32_t current = -1, count = 0, scan = 0;
    int16_t escape_border = -1;
    uint8_t queue_count = 0;
    CombatField *fields = nullptr; // Caller-owned arena slots; order and overlaps are significant.
    int32_t field_count = 0;
    CombatPileStorage piles{};
    int32_t pile_count = 0, spoil_gold = 0;
    int8_t escape_floor_delta = 0; // 0 = absent.
    void *victory_context = nullptr;
    void (*victory_latch)(void *) = nullptr;
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
    const CombatEnemy *const *enemy_defs = nullptr;
    size_t enemy_def_count = 0;
};
enum class CombatResult : uint8_t { Ok, Invalid, Unsupported, MissingMap, NeedsActorStorage, NeedsLootStorage };
struct FixedCombatSetup {
    const uint8_t *sprites = nullptr; // Parallel to map.units, canonical CBT sprite bytes.
    int32_t dungeon_floor = 0;
    CombatField *fields = nullptr;
    size_t field_capacity = 0;
};
// Encounter order is already rolled, exactly like CombatOpts.enemies array.
// Definition/map storage is borrowed only during init; enemy definitions remain
// borrowed.
CombatResult initialize_combat(CombatContext &, const CombatMap &, CombatDirection,
                               const CombatEnemy *const *, size_t, bool room = false, const FixedCombatSetup *fixed = nullptr);
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
    EnemyStep,
    Klimb, Get, Open
};
CombatResult combat_action(CombatContext &, CombatAction, int32_t x = 0, int32_t y = 0);
// Maximum additional actor storage required conservatively before any action.
int32_t combat_growth_reserve(const CombatState &);
CombatResult combat_cast_effect(CombatContext &, SpellEffect, const CombatPoint *aim = nullptr);
// Atomic semantic cast: target_member=-1 cancels a required party target;
// cancel_aim loses already-paid resources without advancing the turn, as in TS.
CombatResult combat_cast(CombatContext &, SpellId, const CombatPoint *aim = nullptr,
                         int32_t target_member = -1, bool cancel_aim = false);
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
// Generic fixed/scripted entry. The caller supplies the already-approved trigger
// and exact arena/units; no quest conditions or narrative rewards live here.
CombatDirection dungeon_room_entry(const CombatMap &, uint8_t facing);
CombatResult start_fixed_combat(CommandContext &, CombatContext &, const CombatMap &,
                                const FixedCombatSetup &, CombatDirection, bool room,
                                int corridor_cause = -1);
void build_corridor_map(GameState &, const struct DungeonState &, int max_per_map,
                         CombatMap &, uint8_t sprites[16]);
} // namespace openu5
