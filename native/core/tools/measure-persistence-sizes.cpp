#include "openu5/persistence.h"
using namespace openu5;
using namespace openu5::save;
#define SIZE(T) char size_##T[sizeof(T)]
SIZE(GameState); SIZE(PartyState); SIZE(CharacterState); SIZE(TurnState); SIZE(Json); SIZE(Generation); SIZE(Gam); SIZE(Ool);
