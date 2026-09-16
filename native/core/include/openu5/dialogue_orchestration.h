#pragma once
#include "commands.h"
#include "dialogue.h"
namespace openu5 {
enum class DialogueHandoff : uint8_t { None, Shop, Guard, QuestEnd };
enum class DialogueEventKind : uint8_t { Output, EffectMessage, Ended, Handoff };
struct DialogueEvent {
    DialogueEventKind kind = DialogueEventKind::Output;
    const DialogueOutput *output = nullptr;
    TalkText message{};
    DialogueHandoff handoff = DialogueHandoff::None;
    uint8_t location = 0, slot = 0;
};
struct DialogueSession {
    Conversation conversation;
    GameState *game = nullptr; // Borrowed owner; stable across command-context lifetimes.
    TraceSink trace{};
    bool active = false;
    DialogueHandoff deferred = DialogueHandoff::None;
    NpcActor npc{}; // Identity snapshot survives NPC removal/array compaction.
    std::u16string avatar, names[kMaxParty], npc_name;
    TalkText name_views[kMaxParty]{};
};
struct DialogueServices {
    DialogueSession &session;
    TalkRegistry registry{};
    // Optional translation and alias injection; names/knows/RNG are orchestration-owned.
    ConversationContext language{};
    void *context = nullptr;
    // False/absent leaves an explicit deferred result. QuestEnd is the exact seam
    // for Game.faulineiTheftOnTalkEnd, not an invented successful quest outcome.
    bool (*handoff)(void *, DialogueHandoff, const NpcActor &, EventSink) = nullptr;
};
void dialogue_alarm(NpcActors &, uint8_t location, Rand);
void dialogue_despawn(GameState &, NpcActors *, const NpcActor &);
ActionResult execute_dialogue_command(CommandContext &, Command);
} // namespace openu5
