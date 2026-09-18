#include "openu5/gameplay_save.h"
#include "openu5/dungeon.h"
#include "openu5/dungeon_encounters.h"
#include "openu5/look.h"
using namespace openu5;
#define SIZE(T) char size_##T[sizeof(T)]
SIZE(GameState); SIZE(TurnState); SIZE(Command); SIZE(CommandState);
SIZE(CommandContext); SIZE(GameEvent); SIZE(QuestObject); SIZE(QuestWorldServices);
SIZE(OutdoorEnemy); SIZE(OutdoorServices); SIZE(LookServices); SIZE(ZodiacView);
SIZE(WorldTerrain); SIZE(TerrainCell); SIZE(CombatState); SIZE(DungeonContext);
SIZE(DungeonEncounters); SIZE(DungeonScratch); SIZE(NpcScanGrid);
