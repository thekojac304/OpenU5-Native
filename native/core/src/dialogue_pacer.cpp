#include "openu5/dialogue_pacer.h"

#include <cstring>
#include <string>

namespace openu5 {

DialoguePause dialogue_event_pause(const GameEvent &e) {
    if (e.kind != GameEventKind::Dialogue || !e.dialogue || e.dialogue->kind != DialogueEventKind::Output ||
        !e.dialogue->output || e.dialogue->output->kind != DialogueOutputKind::Line)
        return DialoguePause::None;
    return e.dialogue->output->pause;
}

DialoguePause paced_event_pause(const GameEvent &e) {
    return e.kind == GameEventKind::ShrineKeyWait ? DialoguePause::Key : dialogue_event_pause(e);
}

namespace {
// Every payload pointer a GameEvent can borrow, other than `text`. A queued
// event outlives its emit, so it may keep none of them.
bool borrows_payload(const GameEvent &e) {
    return e.combat || e.dialogue || e.shop || e.endgame || e.npc || e.refuge || e.troll_sneak ||
           e.blackthorn_scene || e.zodiac || e.sign_raw;
}
} // namespace

void DialoguePacer::reset() {
    head_ = count_ = text_used_ = 0;
    state_ = DialoguePacerState::Idle;
    resume_at_ms_ = 0;
}

bool DialoguePacer::push(const GameEvent &e) {
    if (!storage_.steps || count_ >= storage_.step_capacity) return false;
    DialoguePacerStep step{};
    const void *text = nullptr;
    size_t bytes = 0;
    if (e.kind == GameEventKind::Dialogue) {
        const auto *d = e.dialogue;
        if (!d) return false;
        step.location = d->location;
        step.slot = d->slot;
        step.handoff = d->handoff;
        switch (d->kind) {
        case DialogueEventKind::Output:
            if (!d->output) return false;
            step.kind = d->output->kind == DialogueOutputKind::Prompt   ? DialoguePacerStepKind::Prompt
                        : d->output->kind == DialogueOutputKind::Effect ? DialoguePacerStepKind::Effect
                                                                        : DialoguePacerStepKind::Line;
            step.effect = d->output->effect;
            step.pause = d->output->pause;
            step.rune = d->output->rune;
            step.question = d->output->question;
            text = d->output->text.data();
            bytes = d->output->text.size() * sizeof(char16_t);
            break;
        case DialogueEventKind::EffectMessage:
            step.kind = DialoguePacerStepKind::EffectMessage;
            text = d->message.data();
            bytes = d->message.size() * sizeof(char16_t);
            break;
        case DialogueEventKind::Ended: step.kind = DialoguePacerStepKind::Ended; break;
        case DialogueEventKind::Handoff: step.kind = DialoguePacerStepKind::Handoff; break;
        }
        step.event.kind = GameEventKind::Dialogue;
    } else {
        if (borrows_payload(e)) return false;
        step.kind = DialoguePacerStepKind::Event;
        step.pause = paced_event_pause(e);
        step.event = e;
        step.event.text = nullptr;
        if (e.text) {
            text = e.text;
            bytes = std::strlen(e.text) + 1;
        }
    }
    if (text) {
        const size_t at = (text_used_ + 1) & ~size_t(1); // char16_t alignment
        if (!storage_.text || at + bytes > storage_.text_capacity) return false;
        if (bytes) std::memcpy(storage_.text + at, text, bytes);
        step.has_text = true;
        step.text_offset = uint32_t(at);
        step.text_bytes = uint32_t(bytes);
        text_used_ = at + bytes;
    }
    storage_.steps[(head_ + count_) % storage_.step_capacity] = step;
    ++count_;
    return true;
}

void DialoguePacer::deliver(const DialoguePacerStep &step, EventSink out) const {
    if (!out.emit) return;
    const char *at = step.has_text ? storage_.text + step.text_offset : nullptr;
    if (step.kind == DialoguePacerStepKind::Event) {
        GameEvent e = step.event;
        e.text = at;
        out.emit(out.context, e);
        return;
    }
    std::u16string text;
    if (at && step.text_bytes) {
        text.resize(step.text_bytes / sizeof(char16_t));
        std::memcpy(text.data(), at, text.size() * sizeof(char16_t));
    }
    DialogueOutput o;
    DialogueEvent d;
    d.location = step.location;
    d.slot = step.slot;
    d.handoff = step.handoff;
    switch (step.kind) {
    case DialoguePacerStepKind::Line:
    case DialoguePacerStepKind::Prompt:
    case DialoguePacerStepKind::Effect:
        o.kind = step.kind == DialoguePacerStepKind::Prompt   ? DialogueOutputKind::Prompt
                 : step.kind == DialoguePacerStepKind::Effect ? DialogueOutputKind::Effect
                                                              : DialogueOutputKind::Line;
        o.effect = step.effect;
        o.text = std::move(text);
        o.rune = step.rune;
        o.question = step.question;
        o.pause = step.pause;
        d.kind = DialogueEventKind::Output;
        d.output = &o;
        break;
    case DialoguePacerStepKind::EffectMessage:
        d.kind = DialogueEventKind::EffectMessage;
        d.message = text;
        break;
    case DialoguePacerStepKind::Ended: d.kind = DialogueEventKind::Ended; break;
    case DialoguePacerStepKind::Handoff: d.kind = DialogueEventKind::Handoff; break;
    case DialoguePacerStepKind::Event: break;
    }
    GameEvent e = step.event;
    e.dialogue = &d;
    out.emit(out.context, e);
}

void DialoguePacer::hold(DialoguePause pause, uint32_t now_ms) {
    state_ = pause == DialoguePause::Key ? DialoguePacerState::Key : DialoguePacerState::Timed;
    resume_at_ms_ = now_ms + pause_ms_;
}

void DialoguePacer::release(uint32_t now_ms, EventSink out) {
    state_ = DialoguePacerState::Idle;
    while (count_) {
        const auto step = storage_.steps[head_];
        head_ = (head_ + 1) % storage_.step_capacity;
        --count_;
        ++released_;
        deliver(step, out);
        if ((step.kind == DialoguePacerStepKind::Line || step.kind == DialoguePacerStepKind::Event) &&
            step.pause != DialoguePause::None) {
            hold(step.pause, now_ms);
            break;
        }
    }
    // Nothing references the arena once the queue is empty: every released
    // line was copied on by the sink.
    if (!count_) head_ = text_used_ = 0;
}

void DialoguePacer::collapse(EventSink out) {
    ++collapsed_;
    state_ = DialoguePacerState::Idle;
    while (count_) {
        const auto step = storage_.steps[head_];
        head_ = (head_ + 1) % storage_.step_capacity;
        --count_;
        ++released_;
        deliver(step, out);
    }
    head_ = text_used_ = 0;
}

bool DialoguePacer::offer(const GameEvent &e, uint32_t now_ms, EventSink out) {
    if (!pause_ms_) return false;
    if (state_ == DialoguePacerState::Idle) {
        const auto pause = paced_event_pause(e);
        if (pause == DialoguePause::None) return false;
        ++released_;
        if (out.emit) out.emit(out.context, e);
        hold(pause, now_ms);
        return true;
    }
    if (push(e)) return true;
    collapse(out);
    return false;
}

void DialoguePacer::pump(uint32_t now_ms, EventSink out) {
    if (state_ == DialoguePacerState::Timed && int32_t(now_ms - resume_at_ms_) >= 0) release(now_ms, out);
}

bool DialoguePacer::advance_key(uint32_t now_ms, EventSink out) {
    if (state_ == DialoguePacerState::Idle) return false;
    release(now_ms, out);
    return true;
}

} // namespace openu5
