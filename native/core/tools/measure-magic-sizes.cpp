#include "openu5/combat.h"
// Compile without linking; nm symbol sizes are the target ABI's sizeof values.
extern "C" {
char size_game_state[sizeof(openu5::GameState)];
char size_party_state[sizeof(openu5::PartyState)];
char size_character_state[sizeof(openu5::CharacterState)];
char size_combat_state[sizeof(openu5::CombatState)];
char size_combat_actor[sizeof(openu5::CombatActor)];
char size_combat_map[sizeof(openu5::CombatMap)];
char size_combat_context[sizeof(openu5::CombatContext)];
char size_combat_event[sizeof(openu5::CombatEvent)];
char size_event[sizeof(openu5::GameEvent)];
char size_command[sizeof(openu5::Command)];
char size_command_context[sizeof(openu5::CommandContext)];
char size_turn_state[sizeof(openu5::TurnState)];
char size_spell_effect[sizeof(openu5::SpellEffect)];
char size_combat_field[sizeof(openu5::CombatField)];
}
