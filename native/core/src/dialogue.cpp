#include "openu5/dialogue.h"
#include <algorithm>

namespace openu5 {
namespace {
bool space(char16_t c) {
    return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
        c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
int fold(char16_t c) { const int n = c & 0x7f; return n > 0x60 ? n & 0x5f : n; }
bool contains(TalkLine l, TalkOp op) {
    for (const auto &i : l) if (i.op == op) return true;
    return false;
}
bool isolating(TalkOp op) {
    return op == TalkOp::IfElseKnowsName || op == TalkOp::DoNothingSection ||
        op == TalkOp::Label || op == TalkOp::Change || op == TalkOp::Gold;
}
bool starts(TalkText a, TalkText b) { return a.substr(0, b.size()) == b; }
bool ends(TalkText a, TalkText b) { return a.size() >= b.size() && a.substr(a.size()-b.size()) == b; }
const TalkLine empty_line{};
constexpr TalkText profanity[] = {
    u"FUCK",u"SHIT",u"DAMN",u"DICK",u"PRICK",u"PUSSY",u"CUNT",u"ASS",u"BUTT",
    u"BOOGER",u"PISS",u"JACK OFF",u"MASTURBATE",u"SUCK",u"FART",u"TITS",u"BOOB",
    u"MELONS",u"BLOW",u"PENIS",u"BREAST",u"CLIT",u"BALLS",u"SCROTUM",u"NUTS",
    u"BULLSHIT",u"CUM",u"CROTCH",u"MOTHERFUCKER"
};
void segment_push(std::vector<DialogueSegment> &out, TalkText t, bool rune) {
    if (t.empty()) return;
    if (!out.empty() && out.back().rune == rune) out.back().text += t;
    else out.push_back({std::u16string(t),rune});
}
}
TalkText talk_trim(TalkText s) {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
bool talk_keyword_matches(TalkText input, TalkText key) {
    if (key.empty() || key.size() > input.size()) return false;
    size_t pos = 0;
    while (pos <= input.size()-key.size()) {
        size_t n = 0;
        while (n < key.size() && fold(input[pos+n]) == fold(key[n])) ++n;
        if (n == key.size()) return pos == 0 || input[pos-1] == u' ';
        pos += n+1;
    }
    return false;
}
TalkMaster talk_master(int32_t loc) {
    if (loc < 1 || loc > 32) return TalkMaster::None;
    return static_cast<TalkMaster>(1+(loc-1)/8);
}
const TalkScript *talk_script_for(const TalkRegistry &r, int32_t loc, int32_t dialog) {
    const auto master = talk_master(loc);
    return r.get && master != TalkMaster::None && dialog > 0 && dialog <= 127 ?
        r.get(r.context, master, dialog) : nullptr;
}
const TalkScript *talk_catalog_lookup(void *p, TalkMaster master, int32_t dialog) {
    if (!p || master == TalkMaster::None || uint8_t(master) > 4) return nullptr;
    const auto scripts = static_cast<const TalkCatalog *>(p)->masters[uint8_t(master)-1];
    // JS new Map([...]) keeps the LAST record for duplicate NPC indexes.
    for (size_t i = scripts.size; i; --i) if (scripts[i-1].npc_index == dialog) return &scripts[i-1];
    return nullptr;
}
std::u16string Conversation::tr(TalkText s) const {
    return ctx_.translate ? ctx_.translate(ctx_.context,s) : std::u16string(s);
}
void Conversation::bind(const TalkScript &s, ConversationContext c) {
    *this = Conversation{};
    script_ = &s; ctx_ = c;
}
void Conversation::begin_speech() { speech_ = open_ = true; last_speech_ = size_t(-1); }
void Conversation::abandon_speech() { speech_ = open_ = false; last_speech_ = size_t(-1); }
void Conversation::end_speech() {
    if (speech_ && last_speech_ < outputs_.size()) {
        auto &o = outputs_[last_speech_];
        size_t at = o.text.size();
        while (at && o.text[at-1] == u'\n') --at;
        if (!ends(TalkText(o.text).substr(0,at),ctx_.quote_close)) {
            o.text.insert(at,ctx_.quote_close);
            if (!o.segments.empty()) {
                std::vector<DialogueSegment> segs;
                size_t pos = 0; bool placed = false;
                for (const auto &s : o.segments) {
                    if (!placed && at >= pos && at <= pos+s.text.size()) {
                        segment_push(segs,TalkText(s.text).substr(0,at-pos),s.rune);
                        segment_push(segs,ctx_.quote_close,false);
                        segment_push(segs,TalkText(s.text).substr(at-pos),s.rune);
                        placed = true;
                    } else segment_push(segs,s.text,s.rune);
                    pos += s.text.size();
                }
                if (!placed) segment_push(segs,ctx_.quote_close,false);
                o.segments = std::move(segs);
            }
        }
    }
    abandon_speech();
}
void Conversation::flush(DialoguePause pause) {
    if (!text_.empty()) segments_.push_back({std::move(text_),rune_});
    text_.clear();
    std::u16string joined;
    bool has_rune = false, has_latin = false;
    for (const auto &s : segments_) {
        joined += s.text; has_rune |= s.rune; has_latin |= !s.rune;
    }
    const bool mixed = has_rune && has_latin;
    DialogueOutput o; o.pause = pause;
    if (see_) {
        o.text = ctx_.see_compose ? ctx_.see_compose(ctx_.context,joined) : u"You see " + joined;
        const auto at = o.text.find(joined);
        if (mixed && at != std::u16string::npos) {
            if (at) o.segments.push_back({o.text.substr(0,at),false});
            for (const auto &s : segments_) o.segments.push_back(s);
            if (at+joined.size() < o.text.size()) o.segments.push_back({o.text.substr(at+joined.size()),false});
        }
    } else if (mixed) {
        const auto prefix = tr(prefix_);
        if (!prefix.empty()) o.segments.push_back({prefix,false});
        for (const auto &s : segments_) o.segments.push_back({s.rune ? s.text : tr(s.text),s.rune});
        for (const auto &s : o.segments) o.text += s.text;
    } else o.text = tr(prefix_) + tr(joined);
    const bool all_rune = segments_.empty() ? rune_ : !has_latin;
    segments_.clear(); see_ = false; prefix_.clear();
    if (talk_trim(o.text).empty()) {
        if (pause != DialoguePause::None) { o.text.clear(); o.segments.clear(); outputs_.push_back(std::move(o)); }
        return;
    }
    if (speech_ && open_) {
        if (!starts(o.text,ctx_.quote_open)) {
            o.text.insert(0,ctx_.quote_open);
            if (!o.segments.empty()) {
                if (!o.segments.front().rune) o.segments.front().text.insert(0,ctx_.quote_open);
                else o.segments.insert(o.segments.begin(),{std::u16string(ctx_.quote_open),false});
            }
        }
        open_ = false;
    }
    o.rune = o.segments.empty() && all_rune;
    if (speech_) last_speech_ = outputs_.size();
    outputs_.push_back(std::move(o));
}
void Conversation::effect(DialogueEffectKind kind, int32_t value) {
    DialogueOutput o; o.kind = DialogueOutputKind::Effect; o.effect = {kind,value};
    outputs_.push_back(std::move(o));
}
void Conversation::prompt(DialoguePending p) {
    pending_ = p;
    DialogueOutput o; o.kind = DialogueOutputKind::Prompt; o.question = p != DialoguePending::Interest;
    outputs_.push_back(std::move(o));
}
bool Conversation::matches(TalkText input, TalkText key) const {
    if (talk_keyword_matches(input,key)) return true;
    if (ctx_.aliases) for (auto a : ctx_.aliases(ctx_.context,key)) if (talk_keyword_matches(input,a)) return true;
    return false;
}
const TalkLine *Conversation::answer(TalkText input, TalkView<TalkQA> qa, TalkText *key) const {
    // Duplicate keys are harmless here: first insertion wins and a later identical
    // key cannot match when its first occurrence did not match.
    for (const auto &q : qa) for (auto k : q.keywords) {
        k = talk_trim(k);
        if (!k.empty() && matches(input,k)) {
            if (key) *key = k;
            return q.answer.size ? &q.answer[0] : &empty_line;
        }
    }
    return nullptr;
}
void Conversation::line(TalkLine l, int owner) {
    sections_.clear(); section_ = item_ = 0; skip_ = -1; line_owner_ = owner;
    size_t start = 0;
    auto push = [&](size_t end) { if (end > start) sections_.push_back({l.data+start,end-start}); };
    for (size_t i = 0; i < l.size; ++i) {
        const auto op = l[i].op;
        if (op == TalkOp::StartNewSection) { push(i); start = i+1; }
        else if (op == TalkOp::StartLabelDefinition) {
            push(i); start = i;
            if (i+1 < l.size && l[i+1].op == TalkOp::Label) ++i;
            push(i+1); start = i+1;
        } else if (isolating(op)) { push(i); start = i; push(i+1); start = i+1; }
    }
    push(l.size); line_running_ = true; goto_ = -1;
}
void Conversation::run_line() {
    while (section_ < sections_.size()) {
        const auto s = sections_[section_];
        if (!item_) {
            if (skip_ == 0) { --skip_; ++section_; continue; }
            if (contains(s,TalkOp::AvatarsName) && !knows()) { ++section_; continue; }
            if (contains(s,TalkOp::AskName) && knows()) {
                if (skip_ != -1) --skip_;
                ++section_; continue;
            }
        }
        int branch = 0;
        while (item_ < s.size) {
            const auto &i = s[item_++];
            switch (i.op) {
            case TalkOp::Text: text_ += i.text; break;
            case TalkOp::AvatarsName: text_ += ctx_.avatar_name; break;
            case TalkOp::NewLine: text_ += u'\n'; break;
            case TalkOp::Rune:
                if (!text_.empty()) { segments_.push_back({std::move(text_),rune_}); text_.clear(); }
                rune_ = !rune_; break;
            case TalkOp::Pause: flush(DialoguePause::Timed); break;
            case TalkOp::KeyWait: flush(DialoguePause::Key); break;
            case TalkOp::Gold: flush(); effect(DialogueEffectKind::Gold,i.value); break;
            case TalkOp::Change: flush(); effect(DialogueEffectKind::GiveItem,i.value); break;
            case TalkOp::KarmaPlusOne: flush(); effect(DialogueEffectKind::Karma,1); break;
            case TalkOp::KarmaMinusOne: flush(); effect(DialogueEffectKind::Karma,-1); break;
            case TalkOp::CallGuards: flush(); effect(DialogueEffectKind::CallGuards); break;
            case TalkOp::JoinParty:
            case TalkOp::EndConversation:
                flush(); abandon_speech();
                if (i.op == TalkOp::JoinParty) effect(DialogueEffectKind::JoinParty);
                effect(DialogueEffectKind::End); ended_ = true; line_running_ = false; return;
            case TalkOp::IfElseKnowsName: branch = knows() ? 1 : 2; item_ = s.size; break;
            case TalkOp::AskName: {
                flush(); abandon_speech();
                DialogueOutput o; o.text = std::u16string(ctx_.quote_open)+tr(u"What is thy name?\"\n");
                outputs_.push_back(std::move(o)); prompt(DialoguePending::Name); return;
            }
            case TalkOp::Label:
                flush(); abandon_speech(); goto_ = i.value; line_running_ = false; return;
            case TalkOp::StartLabelDefinition:
                if (item_ < s.size && s[item_].op == TalkOp::Label) ++item_;
                break;
            default: break;
            }
        }
        if (!branch) flush();
        if (ended_) break;
        if (skip_ != -1) --skip_;
        if (branch == 1) skip_ = 1;
        if (branch == 2) ++section_;
        ++section_; item_ = 0;
    }
    line_running_ = false;
}
void Conversation::label_jump(int32_t id) {
    label_ = nullptr; label_phase_ = 0; goto_ = -1;
    if (++jumps_ > 16) return;
    for (const auto &l : script_->labels) if (l.label == id) { label_ = &l; break; }
    if (!label_) return;
    if (contains(label_->initial,TalkOp::AvatarsName) && !knows()) { label_ = nullptr; return; }
    label_phase_ = 1; line(label_->initial,1);
}
void Conversation::run_label() {
    if (goto_ >= 0) { const auto id = goto_; label_jump(id); return; }
    if (label_phase_ == 1) {
        if (!label_->defaults.size) { label_ = nullptr; label_phase_ = 0; return; }
        prompt(DialoguePending::Label); label_phase_ = 2; return;
    }
    if (label_phase_ == 3 && default_ < label_->defaults.size) {
        line(label_->defaults[default_++],1); return;
    }
    label_ = nullptr; label_phase_ = 0;
}
void Conversation::interest(TalkText response) {
    if (talk_trim(response).empty()) response = u"bye";
    const TalkText standards[] = {u"name",u"job",u"work",u"bye"};
    bool standard = false;
    for (auto k : standards) if (matches(response,k)) { standard = true; break; }
    if (!standard) {
        if (talk_keyword_matches(response,u"THANK")) response = u"bye";
        else for (auto k : profanity) if (talk_keyword_matches(response,k)) {
            begin_speech(); open_ = false;
            text_ = u"\"With language like that, how did you become an Avatar?";
            flush(); end_speech(); root_ = 5; return;
        }
    }
    name_topic_ = response.size() == 4;
    constexpr TalkText name = u"name";
    for (size_t i = 0; name_topic_ && i < 4; ++i)
        name_topic_ = (response[i] == name[i] || response[i] == name[i]-32);
    bye_topic_ = false; topic_ = {};
    if (name_topic_) prefix_ = u"\"My name is ";
    const TalkLine *a = nullptr;
    const TalkLine *lines[] = {&script_->name,&script_->job,&script_->job,&script_->bye};
    for (size_t i = 0; i < 4; ++i) if (matches(response,standards[i])) {
        // The topic must refer to static storage, not the local standards array.
        static constexpr TalkText keys[] = {u"name",u"job",u"work",u"bye"};
        topic_ = keys[i]; a = lines[i]; break;
    }
    if (!a) a = answer(response,script_->qa,&topic_);
    if (a) {
        bye_topic_ = topic_ == u"bye"; begin_speech();
        if (name_topic_) open_ = false;
        root_ = 6; line(*a,0);
    } else {
        prefix_.clear(); begin_speech(); open_ = false;
        text_ = u"\"I cannot help thee with that."; flush(); end_speech(); root_ = 5;
    }
}
void Conversation::run() {
    while (pending_ == DialoguePending::None) {
        if (line_running_) { run_line(); continue; }
        if (label_) { if (ended_) label_ = nullptr; else run_label(); continue; }
        switch (root_) {
        case 0: see_ = true; root_ = 1; line(script_->description,0); break;
        case 1: root_ = 2; if (goto_ >= 0 && !ended_) { jumps_ = 0; label_jump(goto_); } break;
        case 2:
            root_ = 3;
            if (ctx_.knows) { begin_speech(); line(script_->greeting,0); }
            else if (!ctx_.self_intro_roll || ctx_.self_intro_roll(ctx_.self_intro_context ? ctx_.self_intro_context : ctx_.context) != 0) {
                prefix_ = u"\"I am called "; begin_speech(); open_ = false; line(script_->name,0);
            }
            break;
        case 3: root_ = 4; if (goto_ >= 0 && !ended_) { jumps_ = 0; label_jump(goto_); } break;
        case 4: end_speech(); prefix_.clear(); root_ = 5; break;
        case 5: if (ended_) return; prompt(DialoguePending::Interest); break;
        case 6: root_ = 7; if (goto_ >= 0 && !ended_) { jumps_ = 0; label_jump(goto_); } break;
        case 7:
            end_speech(); if (name_topic_) prefix_.clear();
            if (bye_topic_ && !ended_) { effect(DialogueEffectKind::End); ended_ = true; }
            root_ = 5; break;
        default: return;
        }
    }
}
const std::vector<DialogueOutput> &Conversation::start() {
    outputs_.clear();
    if (!script_) return outputs_;
    // Like TS start(), restart the generator without resetting interpreter fields.
    started_ = true; root_ = 0; pending_ = DialoguePending::None; line_running_ = false;
    label_ = nullptr; goto_ = -1;
    run(); return outputs_;
}
const std::vector<DialogueOutput> &Conversation::input(TalkText s) {
    outputs_.clear();
    if (ended_ || !started_) return outputs_;
    const auto p = pending_; pending_ = DialoguePending::None;
    if (p == DialoguePending::Interest) interest(s);
    else if (p == DialoguePending::Name) {
        bool matched = false;
        if (ctx_.has_party_names) {
            for (auto n : ctx_.party_names) if (talk_keyword_matches(s,talk_trim(n).substr(0,4))) matched = true;
        } else matched = talk_keyword_matches(s,talk_trim(ctx_.avatar_name).substr(0,4));
        met_ |= matched;
        text_ = matched ? u"\n\n\"A pleasure!" : u"\n\n\"If you say so...";
        flush();
    } else if (p == DialoguePending::Label) {
        jumps_ = 0;
        if (talk_trim(s).empty()) {
            text_ = u"\n\n\"What didst thou say?"; flush(); prompt(DialoguePending::Label);
        } else {
            const auto *a = answer(s,label_->qa);
            if (a) { label_phase_ = 4; line(*a,1); }
            else { label_phase_ = 3; default_ = 0; }
        }
    }
    run(); return outputs_;
}
void Conversation::cancel() { ended_ = true; pending_ = DialoguePending::None; label_ = nullptr; line_running_ = false; }
size_t Conversation::retained_bytes() const {
    size_t bytes = sections_.capacity()*sizeof(TalkLine) + outputs_.capacity()*sizeof(DialogueOutput) +
        (text_.capacity()+prefix_.capacity()+2)*sizeof(char16_t) + segments_.capacity()*sizeof(DialogueSegment);
    for (const auto &s : segments_) bytes += (s.text.capacity()+1)*sizeof(char16_t);
    for (const auto &o : outputs_) {
        bytes += (o.text.capacity()+1)*sizeof(char16_t) + o.segments.capacity()*sizeof(DialogueSegment);
        for (const auto &s : o.segments) bytes += (s.text.capacity()+1)*sizeof(char16_t);
    }
    return bytes;
}
} // namespace openu5
