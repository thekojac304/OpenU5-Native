#pragma once

#include "combat.h"

namespace openu5 {

// Developer-only, platform-independent state editor ported from
// game/src/debug/debugApi.ts, shortcuts.ts, and saveEditorSections.ts.
// All functions mutate the real native owners supplied by the caller.
enum class DebugStatus : uint8_t {
    Applied,
    Unsupported,
    InvalidCharacter,
    InvalidIndex,
    InvalidValue,
    MissingContext,
    MissingData,
    CoreRejected
};

struct DebugResult {
    DebugStatus status = DebugStatus::Applied;
    uint16_t mutations = 0;
};

enum class DebugCharacterNumber : uint8_t {
    Strength,
    Dexterity,
    Intelligence,
    CurrentMp,
    CurrentHp,
    MaxHp,
    Experience,
    Level,
    MonthsAtInn,
    Gender,
    PartyStatus
};

enum class DebugCharacterText : uint8_t { Name, Class, Status };

enum class DebugResource : uint8_t {
    Food,
    Gold,
    Keys,
    Gems,
    Torches,
    SkullKeys,
    MagicCarpets,
    Karma,
    TurnsSinceStart,
    TorchTurns,
    PartySize,
    ActiveCharacter,
    ShipHull,
    ShipSkiffs
};

enum class DebugInventory : uint8_t { Equipment, Spells, Scrolls, Potions, Reagents };

enum class DebugSpecialItem : uint8_t {
    Grapple,
    Spyglass,
    HmsCape,
    Sextant,
    PocketWatch,
    BlackBadge,
    WoodenBox
};

enum class DebugQuestItem : uint8_t {
    ShardFalsehood,
    ShardHatred,
    ShardCowardice,
    Amulet,
    Crown,
    Sceptre
};

enum class DebugQuestNumber : uint8_t {
    ShrineQuestBitmap,
    ShrineVisitedBitmap,
    ShadowlordSummoned,
    ShadowlordDoomBits
};

enum class DebugRuntimeNumber : uint8_t {
    Wind,
    TransportTile,
    WindDriftCounter,
    SailDirection,
    HmsCapeToggle,
    PreviousHour,
    LightSpellMinutes,
    TimeSpellTurns,
    SkullTreeFoundDay,
    FeluccaPhase,
    TrammelPhase
};

enum class DebugClockPart : uint8_t { Year, Month, Day, Hour, Minute };
enum class DebugNpcFlag : uint8_t { Dead, Met };
enum class DebugPartyRestore : uint8_t { Heal, ClearStatus, Revive };

enum class DebugShortcut : uint8_t {
    MaximizeAll,
    BestEquipment,
    FullMaxParty,
    KillShadowlords,
    MaxResources
};

enum class DebugPreset : uint8_t {
    MaxedParty,
    StockedInventory,
    Combat,
    Dungeon,
    Shrine,
    Quest,
    Transport,
    Endgame,
    LowHealthStatus,
    SaveLoad
};

DebugResult debug_set_character_number(GameState &, size_t, DebugCharacterNumber, int32_t);
DebugResult debug_set_character_text(GameState &, size_t, DebugCharacterText, const char *);
DebugResult debug_set_equipment_slot(GameState &, size_t, EquipSlot, int32_t);
DebugResult debug_restore_party(GameState &, DebugPartyRestore);

DebugResult debug_set_resource(GameState &, DebugResource, int64_t);
DebugResult debug_set_inventory_quantity(GameState &, DebugInventory, size_t, int32_t);
DebugResult debug_set_special_item(GameState &, DebugSpecialItem, bool);
DebugResult debug_set_quest_item(GameState &, DebugQuestItem, bool);
DebugResult debug_set_quest_flag(GameState &, QuestFlag, bool);
DebugResult debug_set_quest_number(GameState &, DebugQuestNumber, int32_t);
DebugResult debug_set_shrine_destroyed(GameState &, size_t, uint8_t);

DebugResult debug_set_clock(GameState &, DebugClockPart, int32_t);
DebugResult debug_set_transport(GameState &, TransportMode);
DebugResult debug_set_runtime_number(TurnState &, DebugRuntimeNumber, int32_t);
DebugResult debug_set_time_spell(TurnState &, char);
DebugResult debug_set_shadowlord_location(TurnState &, size_t, int32_t);

DebugResult debug_set_npc_flag(GameState &, DebugNpcFlag, size_t location_index,
                               size_t npc_index, bool);
DebugResult debug_set_dungeon_room_cleared(GameState &, size_t dungeon_slot,
                                           size_t room, bool);
DebugResult debug_clear_overworld_enemies(CommandContext &);

// These are the exact existing web one-click actions. Missing combat tables make
// BestEquipment a safe no-op, as in DebugApi.bestEquipAll().
DebugResult apply_debug_shortcut(CommandContext &, DebugShortcut);

// Deterministic test-state compositions. They never create sessions or consume
// RNG: Combat/Dungeon/Shrine prepare valid state for those subsystems, while the
// existing command/map-picker APIs remain responsible for actually entering one.
DebugResult apply_debug_preset(CommandContext &, DebugPreset);

} // namespace openu5
