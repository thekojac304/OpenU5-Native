#pragma once
#include "combat.h"
#include "quest_world.h"
#include <vector>
namespace openu5 {
struct OutdoorEnemy {
    int32_t slot=-1, definition=0, tile=0, x=0, y=0;
    bool water=false;
    bool cannon_target=false; // Temporary identity across the synchronous Fire turn and pool compaction.
    int32_t hull=-1, phase=-1, wind_counter=-1;
};
// One live outdoor enemy owner, borrowed by commands. Variable populations and
// pool-composition scratch live on the heap, never in the device task stack.
struct OutdoorServices {
    std::vector<OutdoorEnemy> enemies;
    CombatContext *combat=nullptr;
    const CombatResources *resources=nullptr;
    std::vector<PoolEntity> enemy_view, object_view;
    QuestObject prize{};
    QuestWorldServices *prize_owner=nullptr;
    int32_t encounter_location=0,encounter_floor=0,encounter_x=0,encounter_y=0;
    bool pending_prize=false;
    bool has_chunk_origin=false;
    int32_t chunk_x=0,chunk_y=0;
};
int32_t outdoor_actor_tile(const CommandContext &,int32_t x,int32_t y);
CommandStatus outdoor_tick(CommandContext &,const ActiveMap &,bool spawn,Rand,EventSink);
CommandStatus outdoor_start(CommandContext &,size_t enemy,const ActiveMap &,EventSink,bool intro=true);
int32_t spawn_monster_tile(int32_t terrain,int32_t floor,Rand);
}
