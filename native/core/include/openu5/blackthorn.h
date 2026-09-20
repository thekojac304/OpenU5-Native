#pragma once
#include "quest_world.h"
#include "blackthorn_scene.h"
namespace openu5 {
struct BlackthornSession {
    int8_t shrine=-1,round=0,living=0;
    bool password=false,tribute=false,arrest=false;
    int16_t npc_slot=-1;
};
enum class BlackthornAction : uint8_t { Capture,Answer,Password,Tribute,Arrest };
CommandStatus blackthorn_action(CommandContext &,BlackthornAction,TalkText,bool,EventSink,Rand);
bool blackthorn_turn_effect(CommandContext &,CommandEffect,EventSink,Rand);
CommandStatus talk_guard(CommandContext &,const NpcActor &,EventSink);
int32_t count_living(const GameState &);
int32_t pick_interrogation_shrine(const GameState &);
struct GuardDemand { int32_t kind=0,ret=1,gold_taken=0; };
GuardDemand guard_demand(GameState &,const TurnState &,TalkText,bool);
}
