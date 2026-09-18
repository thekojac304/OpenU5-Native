#pragma once
#include "quest_world.h"
namespace openu5 {
struct WorldCommandResult {
    CommandStatus status=CommandStatus::Success;
    bool turn=false,map_after=false,moved_after=false,gate=false;
};
WorldCommandResult world_interaction(CommandContext &,Command,const ActiveMap &,EventSink,Rand);
WorldCommandResult world_magic(CommandContext &,Command,const ActiveMap &,EventSink,Rand);
int32_t open_door_tile(const CommandContext &,MapId,int32_t x,int32_t y,int32_t tile);
}
