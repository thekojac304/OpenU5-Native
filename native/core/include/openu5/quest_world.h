#pragma once
#include "commands.h"
#include "quest.h"
namespace openu5 {
struct CombatResources;
struct EndgameBeat { const char *phase=nullptr,*message=nullptr,*reply=nullptr;int16_t delay=-1,page=-1; };
struct EndgameScript { bool victory=false;EndgameBeat beats[21]{};uint8_t count=0;std::string greeting; };
struct RefugeBeat {const char *scene=nullptr,*message=nullptr,*sfx=nullptr;int16_t delay=-1;};
struct RefugeScript {RefugeBeat beats[16]{};};
// A value view into the existing world owner. No object pool is allocated here.
// The owner retains all fields not relevant to quests when erasing/inserting.
struct QuestObject {
    int32_t location=0, floor=0, x=0, y=0, tile=0, plot_z=-1;
    PlotItem item=PlotItem::None;
    bool plot=false, shadowlord=false;
    bool search=false, loot=false;
    int32_t item_id=0,quality=0;
    bool chest=false,prop=false;int32_t contents=0;bool trapped=false;
    int32_t slot=-1, hull=0, skiffs=0;
    bool ship=false;
    bool torch=false;
};
struct SearchObject { int32_t id=0,quality=0,location=0,floor=0,x=0,y=0; };
struct ShardSpawn { int32_t x=0,y=0,z=0; };
// Batch 22 diagnostic observer for hydrate_interior_objects(). Every hook is
// optional and none can change the outcome: the core reports what it decided,
// in order, and the owner chooses what to print. `slot` reports each .NPC
// slot once, with the schedule entry it used and either a drop reason or the
// object it is about to append; `end` reports every return path, including
// the early ones that never reach the slot loop.
struct InteriorHydrationTrace {
    void *context=nullptr;
    void (*begin)(void *,int32_t location,const NpcLocationData *source,size_t table_count)=nullptr;
    void (*slot)(void *,size_t index,const NpcSlot &,uint8_t schedule,const char *drop_reason,const QuestObject *accepted)=nullptr;
    void (*end)(void *,int32_t location,bool result,const char *reason)=nullptr;
};
struct QuestWorldServices {
    // Borrowed services and assets must remain valid throughout a command.
    // reserve(n) must guarantee n subsequent appends without failure. Erase
    // compacts indices. Event text/scripts are synchronous views: an async
    // presentation owner must copy them before its emit callback returns.
    void *context=nullptr;
    size_t (*count)(void *)=nullptr;
    QuestObject (*read)(void *,size_t)=nullptr;
    bool (*reserve)(void *,size_t additional)=nullptr;
    void (*append)(void *,const QuestObject &)=nullptr;
    void (*erase)(void *,size_t)=nullptr;
    int32_t (*tile_at)(void *,int32_t,int32_t)=nullptr;
    void (*volatile_tile)(void *,int32_t,int32_t,int32_t)=nullptr;
    const ShardSpawn *spawns=nullptr;
    size_t spawn_count=0;
    TalkView<TalkText> words{};
    int32_t melody_progress=0;
    bool passage_open=false; // Session state, as Game.harpsichordPassageOpen.
    const char *(*end_record)(void *,int32_t)=nullptr; // Authoritative ENDMSG.DAT record.
    CombatContext *encounter=nullptr;
    const CombatResources *combat_resources=nullptr;
    Moonstone *moonstones=nullptr;size_t moonstone_count=0; // Same owner used by transitions.
    const SearchObject *search_objects=nullptr;size_t search_count=0;
    const int32_t *moon_phases=nullptr;size_t moon_phase_count=0;
    bool endgame_script=false;
    const char *(*end_narration)(void *,int32_t)=nullptr;
    bool refuge_pending=false;
    const char *(*karma_record)(void *,int32_t)=nullptr; // Quoted KARMA.DAT, shared with rest.
    void (*write)(void *,size_t,const QuestObject &)=nullptr;
    void (*persistent_tile)(void *,int32_t,int32_t,int32_t)=nullptr;
    const InteriorHydrationTrace *hydration_trace=nullptr; // Diagnostics only.
};
bool hydrate_underworld_plot(GameState &,QuestWorldServices &);
// Batch 23. The +5 byte TOWN.OVL:0x1726 gives a placed .NPC type-1 chest
// (0x1795 `mov word [bp-6],0x1e`). SJOG.OVL:0x112C rolls the loot from it at
// (O)pen time, so it is the only per-chest input to the loot tables.
constexpr int32_t kInteriorChestContents=0x1e;
bool hydrate_interior_objects(CommandContext &,int32_t);
void discard_interior_objects(GameState &,QuestWorldServices &,int32_t);
// Returns turn consumption separately so the command runner owns clock/RNG.
struct QuestCommandResult { CommandStatus status=CommandStatus::Success; bool turn=false, map_after_turn=false; };
QuestCommandResult yell_in_world(CommandContext &,TalkText,EventSink);
QuestCommandResult pickup_plot(CommandContext &,size_t,EventSink);
CommandStatus use_quest_item(CommandContext &,int32_t,EventSink);
CommandStatus play_harpsichord(CommandContext &,int32_t,EventSink);
int32_t harpsichord_passage_tile(const QuestWorldServices &,MapId,int32_t,int32_t,int32_t);
CommandStatus rescue_events(CommandContext &,bool absorption,EventSink);
CommandStatus absorption_endgame(CommandContext &,EventSink);
CommandStatus check_refuge(CommandContext &,EventSink);
CommandStatus resolve_refuge(CommandContext &,EventSink);
TrapdoorOutcome quest_trapdoor(CommandContext &,EventSink);
CommandStatus urban_shadowlord(CommandContext &,EventSink,Rand);
QuestCommandResult doom_entrance(CommandContext &,int32_t,EventSink);
int32_t actor_attack_arena(int32_t tile,int32_t creature,int32_t transport,int32_t location);
CommandStatus town_attack_commit(CommandContext &,const NpcActor &,bool hostile,EventSink);
// Authored SearchObject floors are the raw DATA.OVL byte (0..255). For a
// small-map location (location != 0) that byte is DOS's basement encoding
// and must decode as signed two's-complement (0xFF -> -1), matching every
// other small-map floor convention (smallmaps.json's basement floor -1;
// blackthorn.cpp's deposit() and every other basement destination). For the
// WORLD map (location == 0), 255 is itself the correct, already-meaningful
// runtime floor for the Underworld (types.h's FloorId comment; world.cpp's
// get_active_map()) and must be left untouched -- decoding it too would
// silently break Underworld search objects. This is the one place
// SearchObject::floor is ever compared against a runtime floor (search_at,
// quest_search.cpp); no other file needs to know about the encoding.
int32_t decode_authored_floor(int32_t location, int32_t raw);
int32_t search_at(GameState &,TurnState &,const SearchObject *,size_t,int32_t,int32_t,bool occupied);
int32_t apply_search_grant(GameState &,QuestWorldServices &,int32_t id,int32_t quality);
QuestCommandResult get_quest_object(CommandContext &,Direction,EventSink);
QuestCommandResult search_world(CommandContext &,const Direction *,EventSink,Rand,int32_t searcher=-1);
CommandStatus use_moonstone(CommandContext &,int32_t,EventSink);
int32_t active_gate_phase(const GameState &,const TurnState &,const QuestWorldServices &);
bool moongate_at(const GameState &,const TurnState &,const QuestWorldServices &);
int32_t quest_world_tile(CommandContext &,MapId,int32_t,int32_t,int32_t);
}
