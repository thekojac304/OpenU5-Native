#include "openu5/quest.h"
#include "openu5/commands.h"
#include "openu5/shrine.h"
using namespace openu5;
#define SIZE(T) char size_##T[sizeof(T)]
SIZE(GameState); SIZE(QuestState); SIZE(ShrineData); SIZE(RitualResult); SIZE(TheftResult); SIZE(CommandContext);
SIZE(ShrineSession); SIZE(ShrineServices); SIZE(ShrineInput); SIZE(Command);
#include "openu5/quest_world.h"
#include "openu5/blackthorn.h"
#include "openu5/combat.h"
SIZE(QuestWorldServices); SIZE(QuestObject); SIZE(EndgameScript); SIZE(BlackthornSession); SIZE(CombatState); SIZE(ActiveMap); SIZE(GameEvent);

SIZE(RefugeScript); SIZE(Moonstone);
