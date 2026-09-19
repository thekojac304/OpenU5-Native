#include "openu5/ui_session.h"
#include "openu5/combat.h"
#include "openu5/dialogue_orchestration.h"
#include "openu5/dungeon.h"
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
#include "openu5/ui_debug_menu.h"
#endif
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>

namespace openu5 {
namespace {
void copy_text(char *dst, size_t capacity, const char *src) {
    if (!capacity) return;
    const auto n = src ? std::min(capacity - 1, std::strlen(src)) : size_t(0);
    if (n) std::memcpy(dst, src, n);
    dst[n] = 0;
}
char lower_ascii(char16_t c) {
    return c >= u'A' && c <= u'Z' ? char(c + (u'a' - u'A'))
                                  : c <= 0x7f ? char(c) : 0;
}
int16_t combat_direction(Direction d) {
    switch (d) {
    case Direction::East: return int16_t(CombatDirection::East);
    case Direction::West: return int16_t(CombatDirection::West);
    case Direction::South: return int16_t(CombatDirection::South);
    case Direction::North: return int16_t(CombatDirection::North);
    }
    return 0;
}
bool is_selection(UiMode m) {
    return m == UiMode::PartySelection || m == UiMode::InventorySelection ||
           m == UiMode::EquipmentSelection || m == UiMode::SpellSelection;
}
bool is_modal(UiMode m) {
    return m == UiMode::TextEntry || m == UiMode::NumericEntry || m == UiMode::YesNo ||
           is_selection(m) || m == UiMode::TargetSelection;
}
UiTextChannel event_channel(GameEventKind k) {
    switch (k) {
    case GameEventKind::WalkEcho: return UiTextChannel::CommandEcho;
    case GameEventKind::Combat:
    case GameEventKind::CombatStarted:
    case GameEventKind::CombatEnded: return UiTextChannel::Combat;
    case GameEventKind::Dialogue: return UiTextChannel::Dialogue;
    case GameEventKind::Shop: return UiTextChannel::Shop;
    case GameEventKind::ShrineVisitPrompt:
    case GameEventKind::ShrineRestorePrompt:
    case GameEventKind::ShrineDonatePrompt:
    case GameEventKind::ShrineKeyWait:
    case GameEventKind::RitualInvert:
    case GameEventKind::BlackthornPrompt:
    case GameEventKind::GuardPasswordPrompt:
    case GameEventKind::GuardTributePrompt:
    case GameEventKind::GuardArrestPrompt:
    case GameEventKind::GameWon:
    case GameEventKind::Endgame: return UiTextChannel::Quest;
    default: return UiTextChannel::Message;
    }
}

// Stream transcript blocks into character-cell lines without allocating a
// second transcript.  Blocks may split a word, so the pending word survives a
// UiTextContinuesAfter boundary.  Whitespace is preserved inside a line, but
// whitespace that would begin a wrapped line is deliberately discarded.
template <typename Emit>
void for_each_wrapped_line(const UiSession &session, size_t columns, Emit emit) {
    UiRenderedLine line{};
    char word[kUiRenderedLineBytes]{};
    size_t line_length = 0, word_length = 0, spaces = 0;
    bool event_emitted = false;

    auto set_metadata = [&](const UiTextBlock &block) {
        if (line.sequence) return;
        line.sequence = block.sequence;
        line.channel = block.channel;
        line.flags = block.flags;
    };
    auto emit_line = [&](const UiTextBlock &block) {
        set_metadata(block);
        line.length = uint16_t(line_length);
        line.text[line_length] = 0;
        emit(line);
        line = {};
        line_length = 0;
        event_emitted = true;
    };
    auto append_word = [&](const UiTextBlock &block) {
        if (!word_length) return;
        set_metadata(block);
        if (line_length) {
            if (line_length + spaces + word_length > columns)
                emit_line(block);
            else {
                while (spaces && line_length < columns) {
                    line.text[line_length++] = ' ';
                    --spaces;
                }
            }
        }
        // Leading spaces are trimmed after a wrap (and at the beginning of an
        // event), exactly as the device transcript expects.
        for (size_t i = 0; i < word_length; ++i) line.text[line_length++] = word[i];
        word_length = 0;
        spaces = 0;
    };

    for (size_t i = 0; i < session.transcript_size(); ++i) {
        const auto *block = session.transcript_at(i);
        if (!block) continue;
        set_metadata(*block);
        for (size_t j = 0; j < block->length; ++j) {
            const char c = block->text[j];
            if (c == '\n') {
                append_word(*block);
                emit_line(*block); // Explicit newlines preserve empty lines.
                spaces = 0;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
                append_word(*block);
                if (line_length) ++spaces;
                continue;
            }
            if (word_length == columns) {
                // Only a single token can reach this path.  Keep any preceding
                // prose intact, then hard-break the overlong token.
                if (line_length) emit_line(*block);
                set_metadata(*block);
                for (size_t k = 0; k < word_length; ++k) line.text[line_length++] = word[k];
                emit_line(*block);
                word_length = 0;
                spaces = 0;
            }
            word[word_length++] = c;
        }
        if (!(block->flags & UiTextContinuesAfter)) {
            append_word(*block);
            if (line_length) emit_line(*block);
            else if (!event_emitted) emit_line(*block);
            spaces = 0;
            event_emitted = false;
        }
    }
}
} // namespace

UiSession::UiSession(UiTranscriptStorage t, UiControllerServices s, UiSessionConfig c)
    : transcript_(t), services_(s), config_(c) {
    if (!config_.wrap_columns) config_.wrap_columns = 1;
    config_.wrap_columns = uint16_t(std::min<size_t>(config_.wrap_columns,
                                                     kUiRenderedLineBytes - 1));
    if (!config_.page_rows) config_.page_rows = 1;
    config_.max_input_units = uint16_t(std::min<size_t>(config_.max_input_units,
                                                        kUiInputUnits - 1));
}

void UiSession::dispatch(const UiIntent &i) const {
    if (services_.dispatch) services_.dispatch(services_.context, i);
}

void UiSession::set_base_mode(UiMode m) {
    base_mode_ = m;
    if (mode_ == UiMode::DebugMenu) debug_return_mode_ = m;
    else if (!is_modal(mode_)) mode_ = m;
}

UiMode UiSession::world_return_mode(UiMode m) const {
    if (m == UiMode::Shop) return shop_return_mode_;
    if (m == UiMode::Dialogue) return dialogue_return_mode_;
    if (m == UiMode::ShrineSpecial) return shrine_return_mode_;
    return m;
}

void UiSession::enter_shrine_mode() {
    // Every shrine/Blackthorn prompt in the same ceremony (e.g. a re-prompted
    // donation) re-enters here; only the first entry may (re)capture, or a
    // later re-prompt would overwrite the register with ShrineSpecial itself.
    if (base_mode_ != UiMode::ShrineSpecial) shrine_return_mode_ = world_return_mode(base_mode_);
    set_base_mode(UiMode::ShrineSpecial);
}

void UiSession::settle_shrine_after_modal() {
    if (mode_ == UiMode::ShrineSpecial) set_base_mode(shrine_return_mode_);
}

EventSink UiSession::event_sink() {
    return {this, [](void *p, const GameEvent &e) { static_cast<UiSession *>(p)->consume(e); }};
}

void UiSession::push_block(UiTextChannel channel, const char *text, size_t length,
                           uint8_t flags) {
    if (!transcript_.blocks || !transcript_.capacity) return;
    const auto at = transcript_count_ < transcript_.capacity
                        ? (transcript_head_ + transcript_count_) % transcript_.capacity
                        : transcript_head_;
    auto &b = transcript_.blocks[at];
    b = {};
    b.sequence = ++block_sequence_;
    b.channel = channel;
    b.flags = flags;
    b.length = uint16_t(std::min(length, kUiTranscriptBlockBytes - 1));
    if (b.length) std::memcpy(b.text, text, b.length);
    b.text[b.length] = 0;
    if (transcript_count_ < transcript_.capacity) ++transcript_count_;
    else transcript_head_ = (transcript_head_ + 1) % transcript_.capacity;
    scroll_lines_ = 0;
}

void UiSession::append(UiTextChannel channel, const char *text, uint8_t flags) {
    blocked_repeat_ = 0;
    blocked_block_sequence_ = 0;
    blocked_actor_ = -1;
    if (!text) return;
    const size_t total = std::strlen(text);
    if (!total) {
        push_block(channel, "", 0, flags);
        return;
    }
    size_t at = 0;
    while (at < total) {
        const size_t n = std::min(total - at, kUiTranscriptBlockBytes - 1);
        uint8_t f = flags;
        if (at) f |= UiTextContinuesBefore;
        if (at + n < total) f |= UiTextContinuesAfter;
        push_block(channel, text + at, n, f);
        at += n;
    }
}

void UiSession::append_combat_event(const CombatEvent &event) {
    const bool blocked = event.kind == CombatEventKind::Message && event.text &&
                         std::strcmp(event.text, "Blocked!") == 0;
    if (!blocked) {
        append(event.kind==CombatEventKind::Echo?UiTextChannel::CommandEcho:UiTextChannel::Combat,
               event.text);
        return;
    }
    ++blocked_events_generated_;
    if (blocked_repeat_ && blocked_actor_ == event.actor && transcript_count_ && transcript_.blocks) {
        const auto index = (transcript_head_ + transcript_count_ - 1) % transcript_.capacity;
        auto &last = transcript_.blocks[index];
        if (last.sequence == blocked_block_sequence_ && last.channel == UiTextChannel::Combat) {
            ++blocked_repeat_;
            std::snprintf(last.text, sizeof(last.text), "Blocked! x%u", unsigned(blocked_repeat_));
            last.length = uint16_t(std::strlen(last.text));
            last.sequence = ++block_sequence_;
            blocked_block_sequence_ = last.sequence;
            scroll_lines_ = 0;
            return;
        }
    }
    append(UiTextChannel::Combat, event.text);
    ++blocked_events_presented_;
    blocked_actor_ = event.actor;
    blocked_repeat_ = 1;
    const auto index = (transcript_head_ + transcript_count_ - 1) % transcript_.capacity;
    blocked_block_sequence_ = transcript_.blocks[index].sequence;
}

void UiSession::append_utf16(UiTextChannel channel, const char16_t *text, size_t length,
                             uint8_t flags) {
    if (!text) return;
    char chunk[kUiTranscriptBlockBytes]{};
    size_t used = 0;
    bool continued = false;
    auto flush = [&](bool more) {
        uint8_t f = flags;
        if (continued) f |= UiTextContinuesBefore;
        if (more) f |= UiTextContinuesAfter;
        push_block(channel, chunk, used, f);
        used = 0;
        continued = true;
    };
    for (size_t i = 0; i < length; ++i) {
        uint32_t cp = text[i];
        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < length && text[i + 1] >= 0xdc00 &&
            text[i + 1] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + (text[++i] - 0xdc00);
        }
        char encoded[4];
        size_t n = 0;
        if (cp <= 0x7f) encoded[n++] = char(cp);
        else if (cp <= 0x7ff) {
            encoded[n++] = char(0xc0 | (cp >> 6)); encoded[n++] = char(0x80 | (cp & 0x3f));
        } else if (cp <= 0xffff) {
            encoded[n++] = char(0xe0 | (cp >> 12)); encoded[n++] = char(0x80 | ((cp >> 6) & 0x3f));
            encoded[n++] = char(0x80 | (cp & 0x3f));
        } else {
            encoded[n++] = char(0xf0 | (cp >> 18)); encoded[n++] = char(0x80 | ((cp >> 12) & 0x3f));
            encoded[n++] = char(0x80 | ((cp >> 6) & 0x3f)); encoded[n++] = char(0x80 | (cp & 0x3f));
        }
        if (used + n >= sizeof(chunk)) flush(true);
        std::memcpy(chunk + used, encoded, n); used += n;
    }
    if (used || !length) flush(false);
}

const UiTextBlock *UiSession::transcript_at(size_t index) const {
    if (index >= transcript_count_ || !transcript_.blocks) return nullptr;
    return &transcript_.blocks[(transcript_head_ + index) % transcript_.capacity];
}

void UiSession::enter_modal(UiMode m, UiRequestId r, const char *p) {
    if (!is_modal(mode_)) return_mode_ = mode_;
    mode_ = m;
    request_ = r;
    copy_text(prompt_, sizeof(prompt_), p);
    input_[0] = 0; input_length_ = 0;
    selection_ = {}; selection_cursor_ = 0;
}

void UiSession::begin_text(UiRequestId r, const char *p, size_t max_units,
                           bool escape_clears) {
    enter_modal(UiMode::TextEntry, r, p);
    input_limit_ = std::min({max_units, size_t(config_.max_input_units), kUiInputUnits - 1});
    escape_clears_ = escape_clears;
}

void UiSession::begin_number(UiRequestId r, const char *p, int32_t minimum,
                             int32_t maximum, size_t max_digits) {
    enter_modal(UiMode::NumericEntry, r, p);
    number_min_ = minimum; number_max_ = maximum;
    input_limit_ = std::min({max_digits, size_t(config_.max_input_units), kUiInputUnits - 1});
}

void UiSession::begin_yes_no(UiRequestId r, const char *p, bool cancel_means_no) {
    enter_modal(UiMode::YesNo, r, p);
    cancel_means_no_ = cancel_means_no;
}

void UiSession::begin_selection(UiMode m, UiRequestId r, const char *p,
                                UiSelectionSource source, size_t initial) {
    if (!is_selection(m)) return;
    enter_modal(m, r, p);
    selection_ = source;
    const auto n = source.count ? source.count(source.context) : 0;
    selection_cursor_ = n ? std::min(initial, n - 1) : 0;
}

void UiSession::begin_target(UiRequestId r, const char *p, Command cmd, int16_t x, int16_t y) {
    enter_modal(UiMode::TargetSelection, r, p);
    pending_command_ = cmd;
    pending_command_.combat_x = x; pending_command_.combat_y = y;
}

void UiSession::cancel_modal() {
    if (!is_modal(mode_)) return;
    if (request_ == UiRequestId::Shop) {
        finish_modal(false);
        return;
    }
    const auto request = request_;
    // R-06 combat (R)eady action cost. The reference charges the acting
    // combatant's turn ONCE PER 'R' INTERACTION, when the equipment picker
    // CLOSES -- never per item equipped, and whether or not anything was
    // equipped (main.ts::openCombatReadyPicker -> closeAndEndTurn ->
    // CombatSession.playerReady(), which is `requirePlayerTurn()` +
    // `advanceTurn()` and nothing else). Equipping keeps the picker open and
    // costs nothing; a rejected equip likewise; ESC still costs the turn (unlike
    // the (U)se picker's free "None!" cancel). This close is that single
    // charge point: the equip path leaves through finish_modal(), never here,
    // and AlphaRuntime's per-equip reopen is a fresh begin_selection, so
    // inspecting or equipping any number of rows still yields exactly one
    // charge. Overworld/town/dungeon Ready has no cost at all in the reference
    // (openReadyPicker::close is silent), hence the Combat-only gate; the
    // member-selection step (EquipmentMember) is never charged either, matching
    // the reference's free "None!" exit from the player picker.
    const bool combat_ready_close =
        request == UiRequestId::Equipment && return_mode_ == UiMode::Combat;
    mode_ = return_mode_;
    request_ = UiRequestId::None;
    selection_ = {};
    input_[0] = 0; input_length_ = 0;
    prompt_[0] = 0;
    UiIntent i; i.kind = UiIntentKind::ModalResponse; i.request = request;
    i.value.accepted = false;
    dispatch(i);
    // Order mirrors closeAndEndTurn(): tear the picker down first, then spend
    // the turn. CombatYield is the port's existing silent advance-the-turn
    // action (combat.cpp: `if (action == CombatAction::Yield) { e.advance(); }`
    // behind the same not-a-player-turn guard), i.e. the byte-equivalent of the
    // reference's playerReady()/playerYieldTurn() pair.
    if (combat_ready_close) {
        UiIntent yield; yield.kind = UiIntentKind::Command;
        yield.command.kind = CommandKind::CombatYield;
        dispatch(yield);
    }
    settle_shrine_after_modal();
}

void UiSession::finish_modal(bool accepted, bool yes, int32_t number, int32_t index) {
    const auto request = request_;
    const auto old_mode = mode_;
    mode_ = return_mode_;
    request_ = UiRequestId::None;
    selection_ = {};
    UiIntent i;
    i.kind = UiIntentKind::ModalResponse; i.request = request;
    i.value = {accepted, yes, number, index, input_, input_length_};
    if (request == UiRequestId::TownExit) {
        i.kind = UiIntentKind::Command;
        i.command.kind = yes ? CommandKind::Exit : CommandKind::DeclineExit;
    } else if (request == UiRequestId::Dialogue && old_mode == UiMode::TextEntry) {
        i.kind = UiIntentKind::Command;
        i.command.kind = accepted ? CommandKind::DialogueText : CommandKind::EndConversation;
        i.command.text = input_; i.command.text_length = input_length_;
    } else if (request == UiRequestId::Dialogue && old_mode == UiMode::YesNo) {
        i.kind = UiIntentKind::Command;
        i.command.kind = yes ? CommandKind::DialogueYes : CommandKind::DialogueNo;
    } else if (request == UiRequestId::RestHours && old_mode == UiMode::NumericEntry) {
        i.kind = UiIntentKind::Command;
        i.command.kind = accepted ? CommandKind::Rest : CommandKind::RestCancel;
        i.command.hours = int16_t(number);
    } else if (request == UiRequestId::YellText && old_mode == UiMode::TextEntry) {
        i.kind = UiIntentKind::Command;
        i.command.kind = CommandKind::Yell;
        i.command.text = input_; i.command.text_length = input_length_;
    } else if (request == UiRequestId::Shop &&
               (old_mode == UiMode::TextEntry || old_mode == UiMode::NumericEntry)) {
        i.kind = UiIntentKind::Shop;
        i.shop.action = accepted ? ShopAction::Text : ShopAction::Cancel;
        i.shop.text = input_;
        i.shop.length = input_length_;
    }
    dispatch(i);
    input_[0] = 0; input_length_ = 0; prompt_[0] = 0;
    settle_shrine_after_modal();
}

bool UiSession::selection_view(UiSelectionView &out) const {
    if (!is_selection(mode_)) return false;
    out = {}; out.mode = mode_; out.cursor = selection_cursor_;
    out.count = selection_.count ? selection_.count(selection_.context) : 0;
    if (out.count && selection_.item) out.current = selection_.item(selection_.context, out.cursor);
    return true;
}

bool UiSession::accepts_direction_input() const {
    if (mode_ == UiMode::Exploration || mode_ == UiMode::Dungeon ||
        mode_ == UiMode::Combat || mode_ == UiMode::TargetSelection ||
        mode_ == UiMode::DebugMenu || is_selection(mode_)) return true;
    if (mode_ != UiMode::Shop) return false;
    return shop_phase_ == ShopPhase::Buy || shop_phase_ == ShopPhase::Sell ||
           shop_phase_ == ShopPhase::Reagent || shop_phase_ == ShopPhase::Guild ||
           shop_phase_ == ShopPhase::Ship || shop_phase_ == ShopPhase::Wine ||
           shop_phase_ == ShopPhase::HealerMember ||
           shop_phase_ == ShopPhase::InnLeaveMember ||
           shop_phase_ == ShopPhase::InnPickupMember ||
           (shop_phase_ >= ShopPhase::LegacyHeal && shop_phase_ <= ShopPhase::LegacyResurrect);
}

bool UiSession::handle_modal(const UiAction &a) {
    if (mode_ == UiMode::TextEntry || mode_ == UiMode::NumericEntry) {
        if (a.kind == UiActionKind::Confirm) {
            if (mode_ == UiMode::NumericEntry) {
                int64_t n = 0;
                for (size_t i = 0; i < input_length_; ++i) n = n * 10 + (input_[i] - u'0');
                n = std::max<int64_t>(number_min_, std::min<int64_t>(number_max_, n));
                finish_modal(true, false, int32_t(n));
            } else finish_modal(true);
            return true;
        }
        if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
            if (mode_ == UiMode::TextEntry && escape_clears_) {
                input_length_ = 0; input_[0] = 0;
            } else cancel_modal();
            return true;
        }
        if (a.kind == UiActionKind::DeleteCharacter) {
            if (input_length_) input_[--input_length_] = 0;
            else if (mode_ == UiMode::TextEntry && !escape_clears_) cancel_modal();
            return true;
        }
        auto add = [&](char16_t c) {
            if (input_length_ >= input_limit_) return;
            if (mode_ == UiMode::NumericEntry && (c < u'0' || c > u'9')) return;
            if (c < u' ') return;
            input_[input_length_++] = c; input_[input_length_] = 0;
        };
        if (a.kind == UiActionKind::Character) add(a.character);
        else if (a.kind == UiActionKind::TextInput && a.text)
            for (size_t i = 0; i < a.text_length; ++i) add(a.text[i]);
        return true;
    }
    if (mode_ == UiMode::YesNo) {
        bool valid = false, yes = false;
        if (a.kind == UiActionKind::Confirm) valid = yes = true;
        else if (a.kind == UiActionKind::Character) {
            const auto c = lower_ascii(a.character);
            valid = c == 'y' || c == 'n'; yes = c == 'y';
        } else if ((a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) &&
                   cancel_means_no_) valid = true;
        if (valid) finish_modal(true, yes);
        return true;
    }
    if (is_selection(mode_)) {
        const auto count = selection_.count ? selection_.count(selection_.context) : 0;
        if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) cancel_modal();
        else if (count && (a.kind == UiActionKind::Next || a.kind == UiActionKind::Direction)) {
            const bool forward = a.kind == UiActionKind::Next || a.direction == Direction::South ||
                                 a.direction == Direction::East;
            if (forward) selection_cursor_ = (selection_cursor_ + 1) % count;
            else selection_cursor_ = (selection_cursor_ + count - 1) % count;
        } else if (count && a.kind == UiActionKind::Previous)
            selection_cursor_ = (selection_cursor_ + count - 1) % count;
        else if (count && a.kind == UiActionKind::SelectIndex && a.index >= 0 &&
                 size_t(a.index) < count) {
            selection_cursor_ = size_t(a.index);
            const auto item = selection_.item ? selection_.item(selection_.context, selection_cursor_)
                                              : UiSelectionItem{};
            if (item.enabled) finish_modal(true, false, 0, int32_t(selection_cursor_));
        } else if (count && a.kind == UiActionKind::Confirm) {
            const auto item = selection_.item ? selection_.item(selection_.context, selection_cursor_)
                                              : UiSelectionItem{};
            if (item.enabled) finish_modal(true, false, 0, int32_t(selection_cursor_));
        }
        return true;
    }
    if (mode_ == UiMode::TargetSelection) {
        if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
            const auto cancelled_request = request_;
            const bool klimb = pending_command_.kind == CommandKind::Klimb;
            const bool attack = pending_command_.kind == CommandKind::CombatAttack;
            const bool cast = pending_command_.kind == CommandKind::Cast;
            auto cancelled = pending_command_;
            mode_ = return_mode_; request_ = UiRequestId::None;
            prompt_[0]=0;pending_command_={};
            if (klimb) { Command c; c.kind = CommandKind::KlimbCancel; command(c); }
            // Device Cancel leaves Aim without dispatching an attack. The old
            // CombatAttackCancel path intentionally consumed the queued melee
            // swing and therefore a turn, which is wrong for the requested
            // handheld UI contract.
            else if (attack) { }
            else if (cast) { cancelled.cancel_target=true; command(cancelled); }
            else { UiIntent i; i.kind=UiIntentKind::ModalResponse; i.request=cancelled_request;
                   i.value.accepted=false; dispatch(i); }
        } else if (a.kind == UiActionKind::Direction) {
            if (pending_command_.kind == CommandKind::CombatAttack ||
                pending_command_.kind == CommandKind::Cast) {
                const auto d = direction_delta(a.direction);
                const int nx=std::max(0,std::min(10,int(pending_command_.combat_x)+d.dx));
                const int ny=std::max(0,std::min(10,int(pending_command_.combat_y)+d.dy));
                if(pending_command_.kind!=CommandKind::CombatAttack||
                   combat_distance(nx-combat_origin_x_,ny-combat_origin_y_)<=combat_aim_range_){
                    pending_command_.combat_x=int16_t(nx);
                    pending_command_.combat_y=int16_t(ny);
                }
            } else if (pending_command_.kind == CommandKind::Fire) {
                const auto d = direction_delta(a.direction);
                // Fire remains the authoritative one-direction core command;
                // the UI holds its adjacent-cell reticle until Confirm.
                pending_command_.combat_x = int16_t(5 + d.dx);
                pending_command_.combat_y = int16_t(5 + d.dy);
                pending_command_.direction = a.direction;
                pending_command_.has_direction = true;
            } else {
                if (pending_command_.kind == CommandKind::CombatOpen ||
                    pending_command_.kind == CommandKind::CombatGet ||
                    pending_command_.kind == CommandKind::CombatSearch) {
                    const auto d = direction_delta(a.direction);
                    pending_command_.combat_x = int16_t(combat_origin_x_ + d.dx);
                    pending_command_.combat_y = int16_t(combat_origin_y_ + d.dy);
                    target_render_x_ = pending_command_.combat_x;
                    target_render_y_ = pending_command_.combat_y;
                    target_render_marker_ = true;
                }
                auto cmd = pending_command_; cmd.direction = a.direction; cmd.has_direction = true;
                command_echo(openu5::direction_name(a.direction));
                mode_ = return_mode_; request_ = UiRequestId::None;prompt_[0]=0;pending_command_={};command(cmd);
            }
        } else if (a.kind == UiActionKind::Confirm &&
                   (pending_command_.kind == CommandKind::CombatAttack ||
                    pending_command_.kind == CommandKind::Cast ||
                    (pending_command_.kind == CommandKind::Fire &&
                     pending_command_.has_direction))) {
            if(pending_command_.kind==CommandKind::CombatAttack&&
               pending_command_.combat_x==combat_origin_x_&&
               pending_command_.combat_y==combat_origin_y_)return true;
            auto cmd = pending_command_;
            cmd.has_target = cmd.kind != CommandKind::Fire;
            if (cmd.kind == CommandKind::Fire) command_echo(openu5::direction_name(cmd.direction));
            mode_ = return_mode_; request_ = UiRequestId::None;prompt_[0]=0;pending_command_={};command(cmd);
        }
        return true;
    }
    return false;
}

void UiSession::command(Command c) {
    UiIntent i; i.kind = UiIntentKind::Command; i.command = c; dispatch(i);
}
void UiSession::command_echo(const char *s) { append(UiTextChannel::CommandEcho, s); }
void UiSession::direction_request(CommandKind kind, const char *echo) {
    command_echo(echo);
    Command c; c.kind = kind;
    begin_target(UiRequestId::Direction, "Direction?", c, -1, -1);
}

bool UiSession::handle_exploration(const UiAction &a) {
    if (a.kind == UiActionKind::Direction) {
        Command c; c.kind = CommandKind::Move; c.direction = a.direction; c.has_direction = true;
        command(c); return true;
    }
    if (a.kind == UiActionKind::Confirm) {
        command_echo("Pass"); Command c; c.kind = CommandKind::Pass; command(c); return true;
    }
    if (a.kind != UiActionKind::Character) return false;
    const auto k = lower_ascii(a.character);
    Command c;
    switch (k) {
    case 'a': direction_request(CommandKind::Attack, "Attack-"); return true;
    case 'b': command_echo("Board"); c.kind=CommandKind::Board; break;
    case 'c': { command_echo("Cast");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }
    case 'e': command_echo("Enter"); c.kind=CommandKind::Enter; break;
    case 'f': c.kind=CommandKind::Fire; command_echo("Fire-");
              begin_target(UiRequestId::Direction,"Fire-",c,5,5); return true;
    case 'g': direction_request(CommandKind::Get, "Get-"); return true;
    case 'h': command_echo("Hole up"); begin_number(UiRequestId::RestHours,"Hours (1-9)?",1,9,1); return true;
    case 'i': command_echo("Ignite torch!"); c.kind=CommandKind::Ignite; break;
    case 'j': direction_request(CommandKind::Jimmy, "Jimmy-"); return true;
    case 'k': command_echo("Klimb"); c.kind=CommandKind::Klimb; break;
    case 'l': direction_request(CommandKind::Look, "Look-"); return true;
    case 'm': { command_echo("Mix");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Custom; dispatch(i); return true; }
    case 'n': { UiIntent i; i.kind=UiIntentKind::OpenPartySelection; i.request=UiRequestId::Party; dispatch(i); return true; }
    case 'o': direction_request(CommandKind::Open, "Open-"); return true;
    case 'p': direction_request(CommandKind::Push, "Push-"); return true;
    case 'r': { command_echo("Ready");UiIntent i; i.kind=UiIntentKind::OpenEquipmentSelection; i.request=UiRequestId::Equipment; dispatch(i); return true; }
    case 's': direction_request(CommandKind::Search, "Search-"); return true;
    case 't': direction_request(CommandKind::Talk, "Talk-"); return true;
    case 'u': { command_echo("Use item");UiIntent i; i.kind=UiIntentKind::OpenInventorySelection; i.request=UiRequestId::Inventory; dispatch(i); return true; }
    case 'v': command_echo("View a gem!"); c.kind=CommandKind::ViewGem; break;
    case 'x': command_echo("X-it "); c.kind=CommandKind::Disembark; break;
    // (Y)ell dispatches exactly as the reference's yell() does: aboard a frigate
    // (transport 0x20-0x27) outside the Underworld (location < 0x80) it is the
    // HOIST/FURL sails toggle -- a state-driven command with no word prompt and
    // no choice modal; anywhere else it is the ordinary word-of-power prompt.
    // The two predicates are mirrored in from the owner (set_sail_context);
    // CommandKind::YellSails re-checks them authoritatively in commands.cpp, so
    // a stale mirror can only mis-route, never mis-apply (R-19).
    case 'y':
        command_echo("Yell");
        if (sail_context_frigate_ && sail_context_location_ok_) { c.kind=CommandKind::YellSails; break; }
        begin_text(UiRequestId::YellText,"Yell what?",15); return true;
    case 'z': { command_echo("Z-stats");UiIntent i;i.kind=UiIntentKind::OpenStatusSelection;i.request=UiRequestId::Status;dispatch(i);return true; }
    case ' ': command_echo("Pass"); c.kind=CommandKind::Pass; break;
    // Digits '0'-'9'.  Reference key order (main.ts): the harpsichord intercept
    // is tested FIRST and, when the party is seated at the instrument, the digit
    // plays a note and never reaches the set-active-player arm; otherwise every
    // digit is a SET ACTIVE PLAYER with the literal key value (kernel 0x4080
    // takes key-'1' itself, which the CommandKind::SetActivePlayer handler
    // reproduces with its own member-1). Digit validity/party range and the
    // None!/Invalid! outcomes stay in that core handler -- no modal, no second
    // selection UI (R-20, Y-20).
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
        const int16_t digit = int16_t(k - '0');
        if (harpsichord_active_) { c.kind=CommandKind::HarpsichordNote; c.item=digit; break; }
        command_echo("Set Active Plr:");
        c.kind=CommandKind::SetActivePlayer; c.member=digit; break;
    }
    default: append(UiTextChannel::Message, k == 'd' ? "D-What?" : k == 'w' ? "W-What?" : "What?"); return true;
    }
    command(c); return true;
}

bool UiSession::handle_dungeon(const UiAction &a) {
    Command c; c.kind = CommandKind::DungeonCommand;
    if (a.kind == UiActionKind::Direction) {
        c.item = int16_t(a.direction == Direction::North ? DungeonAction::Forward
                         : a.direction == Direction::South ? DungeonAction::Back
                         : a.direction == Direction::West ? DungeonAction::Left
                                                          : DungeonAction::Right);
        command(c); return true;
    }
    if (a.kind == UiActionKind::Confirm) { c.item=int16_t(DungeonAction::Pass); command(c); return true; }
    if (a.kind != UiActionKind::Character) return false;
    switch (lower_ascii(a.character)) {
    case 'a': c.item=int16_t(DungeonAction::Attack); break;
    case 'g': c.item=int16_t(DungeonAction::Get); break;
    case 'j': c.item=int16_t(DungeonAction::Jimmy); break;
    case 'k': c.item=int16_t(DungeonAction::Klimb); break;
    case 'o': c.item=int16_t(DungeonAction::Open); break;
    case 's': c.item=int16_t(DungeonAction::Search); break;
    case 'c': { command_echo("Cast");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }
    // The dungeon has its own command context, but these menus are deliberately
    // shared UI affordances.  They return to UiMode::Dungeon via return_mode_.
    case 'm': { command_echo("Cast");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }
    case 'r': { command_echo("Ready");UiIntent i; i.kind=UiIntentKind::OpenEquipmentSelection; i.request=UiRequestId::Equipment; dispatch(i); return true; }
    case 'u': { command_echo("Use item");UiIntent i; i.kind=UiIntentKind::OpenInventorySelection; i.request=UiRequestId::Inventory; dispatch(i); return true; }
    case 'v': command_echo("View a gem!"); c.kind=CommandKind::ViewGem; break;
    case 'z': { command_echo("Z-stats");UiIntent i;i.kind=UiIntentKind::OpenStatusSelection;i.request=UiRequestId::Status;dispatch(i);return true; }
    case ' ': c.item=int16_t(DungeonAction::Pass); break;
    default: append(UiTextChannel::Message,"What?"); return true;
    }
    command(c); return true;
}

bool UiSession::handle_combat(const UiAction &a) {
    Command c;
    if (a.kind == UiActionKind::Direction) {
        c.kind=CommandKind::CombatMove; c.combat_x=combat_direction(a.direction); command(c); return true;
    }
    if (a.kind == UiActionKind::Confirm) { c.kind=CommandKind::CombatPass; command(c); return true; }
    if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
        c.kind=CommandKind::CombatEscapeQuick; command(c); return true;
    }
    if (a.kind != UiActionKind::Character) return false;
    switch (lower_ascii(a.character)) {
    case 'a': c.kind=CommandKind::CombatAttack; command_echo("Attack"); begin_target(UiRequestId::Target,"Aim",c,combat_initial_x_,combat_initial_y_); return true;
    case 'c': { command_echo("Cast");UiIntent i; i.kind=UiIntentKind::OpenSpellSelection; i.request=UiRequestId::Spell; dispatch(i); return true; }
    case 'g': c.kind=CommandKind::CombatGet; command_echo("Get");
              begin_target(UiRequestId::Direction,"Direction?",c,-1,-1); return true;
    case 'k': c.kind=CommandKind::CombatKlimb; break;
    // Combat Open is the original directional getdir operation.  Sending an
    // immediate direction-less command targets the actor's own cell and makes
    // a visible adjacent chest appear inert on the handheld.
    case 'o': c.kind=CommandKind::CombatOpen; command_echo("Open");
              begin_target(UiRequestId::Direction,"Direction?",c,-1,-1); return true;
    case 's': c.kind=CommandKind::CombatSearch; command_echo("Search");
              begin_target(UiRequestId::Direction,"Direction?",c,-1,-1); return true;
    case 'r': { command_echo("Ready");UiIntent i; i.kind=UiIntentKind::OpenEquipmentSelection; i.request=UiRequestId::Equipment; dispatch(i); return true; }
    case 'u': { command_echo("Use item");UiIntent i; i.kind=UiIntentKind::OpenInventorySelection; i.request=UiRequestId::Inventory; dispatch(i); return true; }
    case ' ': c.kind=CommandKind::CombatPass; break;
    default: append(UiTextChannel::Combat,"What?"); return true;
    }
    command(c); return true;
}

bool UiSession::handle_shop(const UiAction &a) {
    ShopInput s;
    bool send = true;
    const bool member=shop_phase_==ShopPhase::HealerMember||shop_phase_==ShopPhase::InnLeaveMember||
                      shop_phase_==ShopPhase::InnPickupMember||
                      (shop_phase_>=ShopPhase::LegacyHeal&&shop_phase_<=ShopPhase::LegacyResurrect);
    const bool list=member||shop_phase_==ShopPhase::Buy||shop_phase_==ShopPhase::Sell||
                    shop_phase_==ShopPhase::Reagent||shop_phase_==ShopPhase::Guild||
                    shop_phase_==ShopPhase::Ship||shop_phase_==ShopPhase::Wine;
    // Back is hierarchical: the shop service decides whether the current
    // sub-flow returns to its menu/list or exits the shop altogether.
    if (a.kind == UiActionKind::Cancel || a.kind == UiActionKind::Back) {
        const bool deal=shop_phase_==ShopPhase::BuyDeal||shop_phase_==ShopPhase::SellDeal||
            shop_phase_==ShopPhase::ReagentDeal||shop_phase_==ShopPhase::GuildDeal||
            shop_phase_==ShopPhase::HorseDeal||shop_phase_==ShopPhase::ShipDeal||
            shop_phase_==ShopPhase::HealerDeal||shop_phase_==ShopPhase::InnRestDeal||
            shop_phase_==ShopPhase::InnLeaveDeal||shop_phase_==ShopPhase::RumorDeal||
            shop_phase_==ShopPhase::TavernRoundDeal||shop_phase_==ShopPhase::TavernDrinkDeal;
        s.action=deal?ShopAction::Decline:ShopAction::Cancel;
    }
    else if (a.kind == UiActionKind::SelectIndex) {
        s.action=member?ShopAction::SelectMember:ShopAction::SelectItem;s.value=a.index;
    }
    else if (list && a.kind == UiActionKind::Direction) {
        const bool forward=a.direction==Direction::South||a.direction==Direction::East;
        const int32_t last=shop_offer_count_?int32_t(shop_offer_count_-1):0;
        shop_cursor_=std::max<int32_t>(0,std::min<int32_t>(last,shop_cursor_+(forward?1:-1)));
        // The handheld renderer owns the highlighted authoritative row. Avoid
        // adding synthetic "Choice A/B" debug labels to the game transcript.
        send=false;
    }
    else if (a.kind == UiActionKind::Confirm) {
        if(list)s.action=member?ShopAction::SelectMember:ShopAction::SelectItem;
        else if(shop_phase_==ShopPhase::BlacksmithPause||shop_phase_==ShopPhase::BuyFull||
                shop_phase_==ShopPhase::ReagentFull)s.action=ShopAction::Continue;
        else s.action=ShopAction::Confirm;
        s.value=shop_cursor_;
    }
    else if (a.kind == UiActionKind::Character) {
        const char key=lower_ascii(a.character);
        if(list&&key>='a'&&key<='z'){shop_cursor_=key-'a';s.action=member?ShopAction::SelectMember:ShopAction::SelectItem;s.value=shop_cursor_;}
        else switch (key) {
        case 'b': s.action=ShopAction::Buy; break; case 's': s.action=ShopAction::Sell; break;
        case 'h': s.action=ShopAction::Heal; break; case 'c': s.action=ShopAction::Cure; break;
        case 'r': s.action=shop_type_==ShopType::Barkeeper?ShopAction::Rations:ShopAction::Rest; break;
        case 'd': s.action=ShopAction::Drink; break; case 't': s.action=ShopAction::Rumor; break;
        case 'a': s.action=ShopAction::Round; break; case 'l': s.action=ShopAction::LeaveMember; break;
        case 'p': s.action=ShopAction::Pickup; break; case 'y': s.action=ShopAction::Confirm; break;
        case 'n': s.action=ShopAction::Decline; break; case ' ': s.action=ShopAction::End; break;
        default: send=false; break;
        }
    } else send=false;
    if (send) { UiIntent i; i.kind=UiIntentKind::Shop; i.request=UiRequestId::Shop; i.shop=s; dispatch(i); }
    return true;
}

bool UiSession::handle_input(const UiAction &a) {
    if (a.kind == UiActionKind::PageUp) {
        const auto total=wrapped_line_count();
        const auto max=total>config_.page_rows?total-config_.page_rows:0;
        scroll_lines_=std::min(max,scroll_lines_+config_.page_rows); return true;
    }
    if (a.kind == UiActionKind::PageDown) {
        scroll_lines_=scroll_lines_>config_.page_rows?scroll_lines_-config_.page_rows:0; return true;
    }
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
    if (mode_ == UiMode::DebugMenu && debug_menu_) {
        debug_menu_->handle_input(a);
        if (!debug_menu_->is_open()) mode_ = debug_return_mode_;
        return true;
    }
#endif
    if (is_modal(mode_)) return handle_modal(a);
    switch (mode_) {
    case UiMode::Exploration: return handle_exploration(a);
    case UiMode::Dungeon: return handle_dungeon(a);
    case UiMode::Combat: return handle_combat(a);
    case UiMode::Shop: return handle_shop(a);
    case UiMode::Dialogue:
        if (a.kind==UiActionKind::Cancel||a.kind==UiActionKind::Back) { Command c;c.kind=CommandKind::EndConversation;command(c);return true; }
        return true;
    default: return false;
    }
}

#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
bool UiSession::open_debug_menu() {
    if (!debug_menu_) return false;
    if (mode_ != UiMode::DebugMenu) debug_return_mode_ = mode_;
    debug_menu_->open(); mode_ = UiMode::DebugMenu; return true;
}
#endif

void UiSession::consume(const GameEvent &e) {
    ++event_sequence_;
    if (e.kind == GameEventKind::Message || e.kind == GameEventKind::WalkEcho)
        append(event_channel(e.kind), e.text ? e.text : "");
    switch (e.kind) {
    case GameEventKind::CombatStarted:
        // Combat is authoritative core state, not a session UiSession owns.
        // If base_mode_ is somehow still a live Shop/Dialogue/ShrineSpecial
        // session, reduce through its own return register rather than
        // capturing the session mode itself as a combat return destination.
        pre_combat_mode_=world_return_mode(base_mode_); set_base_mode(UiMode::Combat); break;
    case GameEventKind::CombatEnded:
        // Combat can finish while aim, inventory, or another combat-owned
        // modal is open. A base-mode-only update leaves that modal alive and
        // routes the first exploration shortcut through stale combat state.
        request_=UiRequestId::None;selection_={};selection_cursor_=0;
        input_[0]=0;input_length_=0;prompt_[0]=0;pending_command_={};target_render_marker_=false;
        mode_=pre_combat_mode_;base_mode_=pre_combat_mode_;return_mode_=pre_combat_mode_;
        break;
    case GameEventKind::DungeonEntered: set_base_mode(UiMode::Dungeon); break;
    case GameEventKind::DungeonExited: set_base_mode(UiMode::Exploration); break;
    case GameEventKind::Combat:
        if (e.combat && e.combat->text) append_combat_event(*e.combat);
        break;
    case GameEventKind::Dialogue:
        if (!e.dialogue) break;
        if (e.dialogue->kind==DialogueEventKind::Output && e.dialogue->output) {
            const auto &o=*e.dialogue->output;
            if (o.kind==DialogueOutputKind::Line)
                append_utf16(UiTextChannel::Dialogue,o.text.data(),o.text.size(),o.rune?UiTextRune:UiTextNone);
            else if (o.kind==DialogueOutputKind::Prompt) {
                // Capture BEFORE set_base_mode(Dialogue) overwrites base_mode_,
                // reduced through any dangling session mode to the real world
                // mode underneath it (R-18). Repeated prompts within the same
                // conversation must not re-capture Dialogue itself.
                if (base_mode_ != UiMode::Dialogue) dialogue_return_mode_ = world_return_mode(base_mode_);
                set_base_mode(UiMode::Dialogue);
                begin_text(UiRequestId::Dialogue,o.question?"You respond-":"Your interest?",15,true);
            }
        } else if (e.dialogue->kind==DialogueEventKind::EffectMessage)
            append_utf16(UiTextChannel::Dialogue,e.dialogue->message.data(),e.dialogue->message.size());
        else if (e.dialogue->kind==DialogueEventKind::Ended) set_base_mode(dialogue_return_mode_);
        break;
    case GameEventKind::Shop:
        if (!e.shop) break;
        if (e.shop->session) {
            const auto old_phase=shop_phase_;
            const auto &shop=*e.shop->session;
            if(old_phase!=shop.phase)shop_cursor_=0;
            shop_phase_=shop.phase;shop_type_=shop.type;
            if(e.shop->kind==ShopEventKind::Entered&&shop.record){
                char heading[96]{};
                std::snprintf(heading,sizeof(heading),"%s\n%s, proprietor",
                              shop.record->name&&*shop.record->name?shop.record->name:"Shop",
                              shop.record->keeper&&*shop.record->keeper?shop.record->keeper:"Merchant");
                append(UiTextChannel::Shop,heading);
            }
            if(old_phase!=shop.phase||e.shop->kind==ShopEventKind::Entered){
                const char *p="";
                switch(shop.phase){
                case ShopPhase::Greeting: p="Enter: continue  Mic: leave"; break;
                case ShopPhase::BlacksmithPause: p="Enter: hear offer  Mic: leave"; break;
                case ShopPhase::Menu:
                    p=shop.type==ShopType::Blacksmith?"B Buy  S Sell  Mic Leave":
                      shop.type==ShopType::Healer?"H Heal  C Cure  R Resurrect":
                      shop.type==ShopType::InnKeeper?"R Rest  L Leave  P Pickup":"Choose service  Mic: leave";
                    break;
                case ShopPhase::Tavern: p="A Round D Drink R Rations T Rumor"; break;
                case ShopPhase::Buy: case ShopPhase::Sell: case ShopPhase::Reagent:
                case ShopPhase::Guild: case ShopPhase::Ship: case ShopPhase::Wine:
                case ShopPhase::HealerMember: case ShopPhase::InnLeaveMember:
                case ShopPhase::InnPickupMember: case ShopPhase::LegacyHeal:
                case ShopPhase::LegacyCure: case ShopPhase::LegacyResurrect:
                    p="Trackball: choose  Enter: select  Mic: back"; break;
                case ShopPhase::BuyDeal: case ShopPhase::SellDeal: case ShopPhase::ReagentDeal:
                case ShopPhase::GuildDeal: case ShopPhase::HorseDeal: case ShopPhase::ShipDeal:
                case ShopPhase::HealerDeal: case ShopPhase::InnRestDeal:
                case ShopPhase::InnLeaveDeal: case ShopPhase::RumorDeal:
                case ShopPhase::TavernRoundDeal: case ShopPhase::TavernDrinkDeal:
                    p="Y Yes  N No  Mic: back"; break;
                case ShopPhase::HealerNeed: p="H Heal  C Cure  R Resurrect"; break;
                case ShopPhase::RationsQuantity: p="How many?"; break;
                case ShopPhase::RumorText: p="What rumor?"; break;
                case ShopPhase::Closed: p=""; break;
                default: p="Enter: continue  Mic: back"; break;
                }
                copy_text(prompt_,sizeof(prompt_),p);
            }
        }
        if (e.shop->result && e.shop->result->message) append(UiTextChannel::Shop,e.shop->result->message);
        if (e.shop->kind==ShopEventKind::Exited || shop_phase_==ShopPhase::Closed) {
            prompt_[0]=0;
            set_base_mode(shop_return_mode_);
        } else {
            // Capture BEFORE set_base_mode(Shop) overwrites base_mode_,
            // reduced through any dangling session mode to the real world
            // mode underneath it (R-18). Every non-exit Shop event re-enters
            // here, so only the first one (base_mode_ != Shop yet) may
            // (re)capture -- otherwise navigating the shop would overwrite
            // the register with Shop itself.
            if (base_mode_ != UiMode::Shop) shop_return_mode_ = world_return_mode(base_mode_);
            set_base_mode(UiMode::Shop);
            if(shop_phase_==ShopPhase::RumorText)begin_text(UiRequestId::Shop,"What rumor?",15);
            else if(shop_phase_==ShopPhase::RationsQuantity)
                begin_number(UiRequestId::Shop,"How many?",0,std::numeric_limits<int32_t>::max(),10);
        }
        break;
    case GameEventKind::NeedsDirection:
        direction_request(e.text && std::strcmp(e.text,"klimb")==0?CommandKind::Klimb:CommandKind::Pass,
                          e.text?e.text:"Direction"); break;
    case GameEventKind::TownExitPrompt: begin_yes_no(UiRequestId::TownExit,"Leave this place?",true); break;
    case GameEventKind::ShrineVisitPrompt: enter_shrine_mode(); begin_yes_no(UiRequestId::ShrineVisit,"Visit?",false); break;
    case GameEventKind::ShrineRestorePrompt: enter_shrine_mode(); begin_text(UiRequestId::ShrineRestore,"Virtue:",15); break;
    case GameEventKind::ShrineDonatePrompt: enter_shrine_mode(); begin_number(UiRequestId::ShrineDonate,"How many cycles?",0,99,2); break;
    case GameEventKind::BlackthornPrompt: enter_shrine_mode(); begin_text(UiRequestId::Blackthorn,e.text?e.text:"Your response?",14); break;
    case GameEventKind::GuardPasswordPrompt: begin_text(UiRequestId::GuardPassword,e.text?e.text:"Password?",14); break;
    case GameEventKind::GuardTributePrompt: begin_yes_no(UiRequestId::GuardTribute,"Pay tribute?",false); break;
    case GameEventKind::GuardArrestPrompt: begin_yes_no(UiRequestId::GuardArrest,"Go quietly?",false); break;
    case GameEventKind::TrollTollPrompt: begin_yes_no(UiRequestId::TrollToll,"Pay toll?",false); break;
    case GameEventKind::CrystalBallPrompt: begin_yes_no(UiRequestId::CrystalBall,"Peer into it?",false); break;
    case GameEventKind::WellDropPrompt: begin_yes_no(UiRequestId::WellDrop,"Drop a coin?",false); break;
    case GameEventKind::FountainDrinkPrompt: begin_yes_no(UiRequestId::FountainDrink,"Drink?",false); break;
    case GameEventKind::WellWishPrompt: begin_text(UiRequestId::WellWish,"What dost thou wish?",12); break;
    case GameEventKind::GameWon:
    case GameEventKind::Endgame:
        if(e.text)append(UiTextChannel::Quest,e.text);
        break;
    default: break;
    }
}

size_t UiSession::wrapped_line_count(size_t columns) const {
    columns=columns?columns:config_.wrap_columns;
    columns=std::max<size_t>(1,std::min(columns,kUiRenderedLineBytes-1));
    size_t lines=0;
    for_each_wrapped_line(*this,columns,[&](const UiRenderedLine &){++lines;});
    return lines;
}

size_t UiSession::visible_lines(UiRenderedLine *out,size_t capacity,size_t columns) const {
    if(!out||!capacity)return 0;
    columns=columns?columns:config_.wrap_columns;
    columns=std::max<size_t>(1,std::min(columns,kUiRenderedLineBytes-1));
    const auto total=wrapped_line_count(columns);const auto rows=std::min<size_t>(capacity,config_.page_rows);
    const auto end=total>scroll_lines_?total-scroll_lines_:0;const auto start=end>rows?end-rows:0;
    size_t line_no=0,written=0;
    for_each_wrapped_line(*this,columns,[&](const UiRenderedLine &line){
        if(line_no>=start&&line_no<end&&written<capacity)out[written++]=line;
        ++line_no;
    });
    return written;
}
} // namespace openu5
