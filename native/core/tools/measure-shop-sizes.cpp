#include "openu5/shop_orchestration.h"
using namespace openu5;
#define SIZE(T) char size_##T[sizeof(T)]
SIZE(GameState); SIZE(PartyState); SIZE(CharacterState); SIZE(TurnState); SIZE(ShopSession); SIZE(ShopResult); SIZE(ShopData); SIZE(ShopServices); SIZE(ShopEvent); SIZE(CommandContext); SIZE(GameEvent);
#ifdef OPENU5_MEASURE_MAIN
#include <cstdio>
int main(){
#define PRINT(T) std::printf("%s %zu\n",#T,sizeof(T))
PRINT(GameState); PRINT(PartyState); PRINT(TurnState); PRINT(ShopSession); PRINT(ShopResult); PRINT(ShopData); PRINT(ShopServices); PRINT(ShopEvent); PRINT(CommandContext); PRINT(GameEvent);
}
#endif
