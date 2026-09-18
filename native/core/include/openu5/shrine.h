#pragma once
#include "quest.h"
#include "commands.h"
namespace openu5 {
// Mirrors Game.shrinePending's semantic fields. Scene animation owns no quest
// mutation and is deliberately outside this platform-independent holder.
struct ShrineSession { int8_t visit = -1, restore = -1; uint8_t x = 0, y = 0; };
struct ShrineServices {
    ShrineSession &session;
    const ShrineData *data = nullptr;
    void *context = nullptr;
    const char *(*record)(void *, int32_t) = nullptr; // MISCMSG.DAT, borrowed English text.
};
enum class ShrineAction : uint8_t { Visit, Codex, SubmitVisit, SubmitRestore, Donate };
struct ShrineInput {
    ShrineAction action{};
    int32_t value = 0; // virtue for Visit, cycles for Donate
    TalkText virtue{};
    TalkView<TalkText> mantras{};
};
// No turn/RNG calls: this is the exact Game ceremony helper layer.
ActionResult execute_shrine(CommandContext &, ShrineInput);
ActionResult enter_shrine(CommandContext &, int32_t tile);
void shrine_guardian(GameState &, EventSink);
void shrine_entry(GameState &, ShrineServices &, int32_t tile, EventSink);
} // namespace openu5
