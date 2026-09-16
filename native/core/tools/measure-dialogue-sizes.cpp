#include "openu5/dialogue_orchestration.h"
using namespace openu5;
#define SIZE(T) char size_##T[sizeof(T)]
SIZE(GameState);SIZE(PartyState);SIZE(TurnState);SIZE(CommandContext);SIZE(Command);SIZE(GameEvent);
SIZE(Conversation);SIZE(ConversationContext);SIZE(DialogueSession);SIZE(DialogueServices);
SIZE(DialogueOutput);SIZE(DialogueEvent);SIZE(TalkScript);SIZE(TalkItem);SIZE(TalkLabel);
#ifdef OPENU5_MEASURE_MAIN
#include <cstdio>
int main(){
#define SHOW(T) std::printf(#T " %zu\n",sizeof(T))
SHOW(GameState);SHOW(PartyState);SHOW(TurnState);SHOW(CommandContext);SHOW(Command);SHOW(GameEvent);
SHOW(Conversation);SHOW(ConversationContext);SHOW(DialogueSession);SHOW(DialogueServices);
SHOW(DialogueOutput);SHOW(DialogueEvent);SHOW(TalkScript);SHOW(TalkItem);SHOW(TalkLabel);
}
#endif
