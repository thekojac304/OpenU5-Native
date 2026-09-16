#pragma once
#include "state.h"
#include <string>
#include <string_view>
#include <vector>

namespace openu5 {
// UTF-16 code units deliberately match JS charCodeAt/slice, including high-bit
// folding. Text and script storage belong to the asset provider, not GameState.
using TalkText = std::u16string_view;
template<class T> struct TalkView {
    const T *data = nullptr;
    size_t size = 0;
    const T *begin() const { return data; }
    const T *end() const { return size ? data + size : data; }
    const T &operator[](size_t i) const { return data[i]; }
};
enum class TalkOp : uint8_t {
    Text, AvatarsName, NewLine, Rune, Pause, KeyWait, Gold, Change,
    JoinParty, KarmaPlusOne, KarmaMinusOne, CallGuards, EndConversation,
    IfElseKnowsName, AskName, Label, StartLabelDefinition, DefineLabel,
    DoNothingSection, EndScript, Or, StartNewSection, Unknown
};
struct TalkItem { TalkOp op = TalkOp::Text; TalkText text{}; int32_t value = 0; };
using TalkLine = TalkView<TalkItem>;
struct TalkQA { TalkView<TalkText> keywords{}; TalkView<TalkLine> answer{}; };
struct TalkLabel { int32_t label = 0; TalkLine initial{}; TalkView<TalkLine> defaults{}; TalkView<TalkQA> qa{}; };
struct TalkScript {
    int32_t npc_index = 0;
    TalkLine name{}, description{}, greeting{}, job{}, bye{};
    TalkView<TalkQA> qa{};
    TalkView<TalkLabel> labels{};
};
enum class TalkMaster : uint8_t { None, Towne, Dwelling, Castle, Keep };
TalkMaster talk_master(int32_t location);
struct TalkRegistry {
    void *context = nullptr;
    // Load/borrow ONE script. Keep its backing storage alive until session end.
    const TalkScript *(*get)(void *, TalkMaster, int32_t dialog) = nullptr;
};
const TalkScript *talk_script_for(const TalkRegistry &, int32_t location, int32_t dialog);
struct TalkCatalog { TalkView<TalkScript> masters[4]{}; };
const TalkScript *talk_catalog_lookup(void *, TalkMaster, int32_t dialog);
TalkText talk_trim(TalkText);
bool talk_keyword_matches(TalkText input, TalkText keyword);
enum class DialogueEffectKind : uint8_t { JoinParty, Gold, GiveItem, Karma, CallGuards, End };
struct DialogueEffect { DialogueEffectKind kind = DialogueEffectKind::End; int32_t value = 0; };
enum class DialogueOutputKind : uint8_t { Line, Prompt, Effect };
enum class DialoguePause : uint8_t { None, Key, Timed };
struct DialogueSegment { std::u16string text; bool rune = false; };
struct DialogueOutput {
    DialogueOutputKind kind = DialogueOutputKind::Line;
    std::u16string text;
    std::vector<DialogueSegment> segments;
    bool rune = false, question = false;
    DialoguePause pause = DialoguePause::None;
    DialogueEffect effect{};
};
struct ConversationContext {
    TalkText avatar_name = u"Avatar";
    TalkView<TalkText> party_names{};
    bool has_party_names = false, knows = false;
    void *context = nullptr;
    int32_t (*self_intro_roll)(void *) = nullptr;
    void *self_intro_context = nullptr; // Defaults to context; separate from language callbacks.
    TalkView<TalkText> (*aliases)(void *, TalkText) = nullptr;
    std::u16string (*translate)(void *, TalkText) = nullptr;
    std::u16string (*see_compose)(void *, TalkText) = nullptr;
    TalkText quote_open = u"\"", quote_close = u"\"";
};
enum class DialoguePending : uint8_t { None, Interest, Name, Label };
// Explicit staged coroutine, no C++ coroutine heap or recursive label calls.
// Only current output/text/section indexes allocate, never a copy of the corpus.
// Returned output is borrowed until the next call; consume it synchronously.
class Conversation {
public:
    void bind(const TalkScript &, ConversationContext);
    const std::vector<DialogueOutput> &start();
    const std::vector<DialogueOutput> &input(TalkText);
    void cancel(); // Programmatic teardown; NOT a special text keyword or ESC.
    bool ended() const { return ended_; }
    bool met_avatar() const { return met_; }
    DialoguePending pending() const { return pending_; }
    int32_t active_label() const { return label_ ? label_->label : -1; }
    TalkText topic() const { return topic_; }
    size_t retained_bytes() const; // Container/text capacities, excludes allocator overhead and borrowed assets.
private:
    const TalkScript *script_ = nullptr;
    ConversationContext ctx_{};
    bool ended_ = false, met_ = false, started_ = false;
    bool rune_ = false, see_ = false, speech_ = false, open_ = false;
    bool name_topic_ = false, bye_topic_ = false;
    DialoguePending pending_ = DialoguePending::None;
    int root_ = 0, line_owner_ = 0, label_phase_ = 0, jumps_ = 0;
    int skip_ = -1;
    bool line_running_ = false;
    size_t section_ = 0, item_ = 0, default_ = 0;
    int32_t goto_ = -1;
    const TalkLabel *label_ = nullptr;
    TalkText topic_{};
    std::vector<TalkLine> sections_;
    std::vector<DialogueOutput> outputs_;
    std::u16string text_, prefix_;
    std::vector<DialogueSegment> segments_;
    size_t last_speech_ = size_t(-1);
    bool knows() const { return ctx_.knows || met_; }
    std::u16string tr(TalkText) const;
    void begin_speech();
    void abandon_speech();
    void end_speech();
    void flush(DialoguePause = DialoguePause::None);
    void effect(DialogueEffectKind, int32_t = 0);
    void prompt(DialoguePending);
    void run();
    void line(TalkLine, int owner);
    void run_line();
    void label_jump(int32_t);
    void run_label();
    void interest(TalkText);
    bool matches(TalkText, TalkText) const;
    const TalkLine *answer(TalkText, TalkView<TalkQA>, TalkText *key = nullptr) const;
};
struct DialogueEffectResult {
    TalkText messages[2]{};
    uint8_t message_count = 0;
    bool ended = false, despawn_npc = false, alarm = false;
};
DialogueEffectResult apply_dialogue_effect(GameState &, DialogueEffect, TalkText npc_name, int32_t npc_tile = -1);
} // namespace openu5
