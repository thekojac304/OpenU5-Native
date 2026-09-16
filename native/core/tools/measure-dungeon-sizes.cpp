#include "openu5/dungeon.h"
#include "openu5/combat.h"
#include "openu5/transport.h"
using namespace openu5;
char size_GameState[sizeof(GameState)];
char size_PartyState[sizeof(PartyState)];
char size_TurnState[sizeof(TurnState)];
char size_DungeonState[sizeof(DungeonState)];
char size_DungeonScratch[sizeof(DungeonScratch)];
char size_DungeonContext[sizeof(DungeonContext)];
char size_CombatState[sizeof(CombatState)];
char size_CombatMap[sizeof(CombatMap)];
char size_CombatActor[sizeof(CombatActor)];
char size_CombatContext[sizeof(CombatContext)];
char size_CommandContext[sizeof(CommandContext)];
char size_Command[sizeof(Command)];
char size_GameEvent[sizeof(GameEvent)];
char size_DungeonEvent[sizeof(DungeonEvent)];
char size_TransportServices[sizeof(TransportServices)];
#ifdef OPENU5_MEASURE_MAIN
#include <cstdio>
int main(){
#define SHOW(T) std::printf(#T " %zu\n",sizeof(T))
SHOW(GameState);SHOW(PartyState);SHOW(TurnState);SHOW(DungeonState);SHOW(DungeonScratch);SHOW(DungeonContext);SHOW(CombatState);SHOW(CombatMap);SHOW(CombatActor);SHOW(CombatContext);SHOW(CommandContext);SHOW(Command);SHOW(GameEvent);SHOW(DungeonEvent);SHOW(TransportServices);
}
#endif
