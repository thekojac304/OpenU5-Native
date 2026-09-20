#include "openu5/narrative_scene.h"

#include <cstring>

#include "openu5/quest_world.h"

// Mirror of the reference presenters: game/src/ui/troll-sneak.ts (the whole
// class) and main.ts `runRefugeScene` (the refuge half), plus
// game/src/skin/refugeScene.ts for the baked figures. The derivation lives in
// the header; only the machinery lives here.
namespace openu5 {

RefugePhase refuge_phase_from_name(const char *name) {
    if (!name) return RefugePhase::None;
    if (std::strcmp(name, "void") == 0) return RefugePhase::Void;
    if (std::strcmp(name, "ghostLeft") == 0) return RefugePhase::GhostLeft;
    if (std::strcmp(name, "ghostBoth") == 0) return RefugePhase::GhostBoth;
    if (std::strcmp(name, "apparition") == 0) return RefugePhase::Apparition;
    if (std::strcmp(name, "vertigo") == 0) return RefugePhase::Vertigo;
    return RefugePhase::None;
}

size_t refuge_scene_figures(RefugePhase phase, int16_t avatar_tile,
                            RefugeSceneFigure *out, size_t capacity) {
    if (phase == RefugePhase::None) return 0;
    size_t written = 0;
    auto add = [&](int8_t col, int8_t row, int16_t tile) {
        if (out && written < capacity) out[written] = RefugeSceneFigure{col, row, tile};
        ++written;
    };
    // The Avatar is always there, alone in the centre of the nothingness.
    add(int8_t(kPresentationWindow / 2), int8_t(kPresentationWindow / 2), avatar_tile);
    if (phase == RefugePhase::Void) return written;
    add(kRefugeGhostLeftCol, kRefugeGhostLeftRow, kRefugeGhostLeftTile);
    if (phase == RefugePhase::GhostLeft) return written;
    add(kRefugeGhostRightCol, kRefugeGhostRightRow, kRefugeGhostRightTile);
    if (phase == RefugePhase::GhostBoth) return written;
    add(kRefugeApparitionCol, kRefugeApparitionRow, kRefugeApparitionTile);
    return written;
}

PresentationSnapshot compose_refuge_presentation(RefugePhase phase, int16_t avatar_tile) {
    PresentationSnapshot s;
    // The scene owns its own 11x11 window; centring it on (5,5) keeps every
    // cell-offset consumer reading the same geometry the reference uses.
    s.center = {kPresentationWindow / 2, kPresentationWindow / 2};
    for (int i = 0; i < kPresentationCells; ++i) {
        // 0x0962 blackens the window: kPresentationHidden is exactly the
        // "draw nothing here" value render_snapshot already leaves black.
        s.tiles[i] = kPresentationHidden;
        s.visible[i] = 0;
    }
    if (phase == RefugePhase::None) return s;
    RefugeSceneFigure figures[4]{};
    const size_t count = refuge_scene_figures(phase, avatar_tile, figures, 4);
    for (size_t i = 0; i < count && i < 4; ++i) {
        const auto &f = figures[i];
        if (f.col < 0 || f.row < 0 || f.col >= kPresentationWindow || f.row >= kPresentationWindow)
            continue;
        const int at = f.row * kPresentationWindow + f.col;
        s.tiles[at] = f.tile;
        s.visible[at] = 1;
        // The apparition is an ordinary four-frame animated group; classify it
        // exactly as the world composer would so the shared rasterizer keeps
        // it alive instead of freezing it between beats.
        const auto kind = tile_animation_kind(f.tile);
        const bool animated = kind != TileAnimationKind::Static &&
                              kind != TileAnimationKind::ActorProgram;
        s.animated[at] = animated ? 1 : 0;
        s.any_animated = s.any_animated || animated;
    }
    return s;
}

// ---------------------------------------------------------------------------
// NarrativeScenePacer
// ---------------------------------------------------------------------------

bool NarrativeScenePacer::copy_text(const char *text, uint32_t &offset, uint32_t &length) {
    if (!text || !storage_.text) return false;
    const size_t n = std::strlen(text);
    if (text_used_ + n + 1 > storage_.text_capacity) {
        ++dropped_;
        return false;
    }
    offset = uint32_t(text_used_);
    length = uint32_t(n);
    std::memcpy(storage_.text + text_used_, text, n + 1);
    text_used_ += n + 1;
    return true;
}

NarrativeSceneStep *NarrativeScenePacer::push(NarrativeSceneStepKind kind) {
    if (!storage_.steps || count_ >= storage_.step_capacity) {
        ++dropped_;
        return nullptr;
    }
    auto &step = storage_.steps[(head_ + count_) % storage_.step_capacity];
    step = NarrativeSceneStep{};
    step.kind = kind;
    ++count_;
    return &step;
}

void NarrativeScenePacer::begin(NarrativeScene scene) {
    if (state_ == NarrativeScenePacerState::Idle) {
        reset_queue();
        state_ = NarrativeScenePacerState::Running;
        waiting_ = false;
    }
    scene_ = scene;
}

bool NarrativeScenePacer::enqueue(const GameEvent &e) {
    if (e.kind == GameEventKind::TrollSneak) {
        const auto *script = e.troll_sneak;
        if (!script) return true; // a scene event with no payload stages nothing
        begin(NarrativeScene::TrollSneak);
        for (uint8_t i = 0; i < script->count && i < 32; ++i) {
            const auto &beat = script->beats[i];
            auto *step = push(NarrativeSceneStepKind::Beat);
            if (!step) break;
            step->append = beat.append;
            step->has_text = copy_text(beat.text, step->text_offset, step->text_length);
            // 0x3AE6(n) = run-n-frames: waits n MUTE ticks and returns. A beat
            // with no pause has no dwell and flushes with the next paced one,
            // exactly as the reference's own while-loop drains it.
            step->dwell_ms = beat.pause_units > 0 && paced_
                                 ? uint32_t(beat.pause_units) * kSceneFrameUnitMs
                                 : 0;
        }
        return true;
    }
    if (e.kind == GameEventKind::Refuge) {
        const auto *script = e.refuge;
        if (!script) return true;
        begin(NarrativeScene::Refuge);
        for (const auto &beat : script->beats) {
            // RefugeScript is a fixed array with no count: an unused slot is
            // the all-null beat, and that is where the script ends.
            if (!beat.scene && !beat.message && !beat.sfx) break;
            auto *step = push(NarrativeSceneStepKind::Beat);
            if (!step) break;
            step->phase = refuge_phase_from_name(beat.scene);
            step->has_text = copy_text(beat.message, step->text_offset, step->text_length);
            step->has_sfx = copy_text(beat.sfx, step->sfx_offset, step->sfx_length);
            // A reading floor on beats that print, a short transition floor on
            // the figure/sound beats; the raw `delay` units ride on top.
            const uint32_t floor = beat.message ? kRefugeTextFloorMs : kRefugeSceneFloorMs;
            const uint32_t units = beat.delay > 0 ? uint32_t(beat.delay) : 0;
            step->dwell_ms = paced_ ? units * kRefugeUnitMs + floor : 0;
        }
        return true;
    }
    if (state_ == NarrativeScenePacerState::Idle) return false;
    // Everything else in the turn is deferred behind the scene, in order.
    auto *step = push(NarrativeSceneStepKind::Forward);
    if (!step) return true;
    step->event = e;
    // Every borrowed payload pointer dies with the synchronous delivery that
    // produced it; this queue outlives that, so none of them may survive.
    step->event.text = nullptr;
    step->event.combat = nullptr;
    step->event.dialogue = nullptr;
    step->event.shop = nullptr;
    step->event.endgame = nullptr;
    step->event.npc = nullptr;
    step->event.refuge = nullptr;
    step->event.troll_sneak = nullptr;
    step->event.zodiac = nullptr;
    step->event.blackthorn_scene = nullptr;
    step->event.sign_raw = nullptr;
    step->event.sign_raw_size = 0;
    step->has_text = copy_text(e.text, step->text_offset, step->text_length);
    return true;
}

void NarrativeScenePacer::pump(uint32_t now_ms, NarrativeSceneSink beats, EventSink forward) {
    if (state_ != NarrativeScenePacerState::Running) return;
    while (state_ == NarrativeScenePacerState::Running) {
        if (waiting_) {
            // The beat on screen has not served its dwell: nothing may replace
            // it yet. This is the whole of the pacing contract, and it is why
            // the owner can call pump() every frame for free.
            if (int32_t(now_ms - resume_at_ms_) < 0) return;
            waiting_ = false;
        }
        if (!count_) {
            // Last beat seen. Unmount FIRST, then report the completion -- the
            // reference's own order (`view.setRefugeScene(null)` precedes
            // `game.resolveRefuge()`), so the castle is never revealed behind
            // a stage that is still up.
            text_used_ = 0;
            completion_ = scene_;
            scene_ = NarrativeScene::None;
            phase_ = RefugePhase::None;
            state_ = NarrativeScenePacerState::Idle;
            return;
        }
        auto &step = storage_.steps[head_];
        head_ = (head_ + 1) % storage_.step_capacity;
        --count_;
        ++released_;
        if (step.kind == NarrativeSceneStepKind::Beat) {
            if (step.phase != RefugePhase::None) phase_ = step.phase;
            if (beats.beat) {
                NarrativeSceneBeat out;
                out.text = step.has_text ? storage_.text + step.text_offset : nullptr;
                out.sfx = step.has_sfx ? storage_.text + step.sfx_offset : nullptr;
                out.phase = step.phase;
                out.append = step.append;
                beats.beat(beats.context, out);
            }
            if (step.dwell_ms) {
                resume_at_ms_ = now_ms + step.dwell_ms;
                waiting_ = true;
                return;
            }
            continue;
        }
        GameEvent released = step.event;
        if (step.has_text) released.text = storage_.text + step.text_offset;
        else if (step.event.kind == GameEventKind::Message) released.text = "";
        if (forward.emit) forward.emit(forward.context, released);
    }
}

NarrativeScene NarrativeScenePacer::take_completion() {
    const auto out = completion_;
    completion_ = NarrativeScene::None;
    return out;
}

void NarrativeScenePacer::reset_queue() {
    head_ = 0;
    count_ = 0;
    text_used_ = 0;
}

void NarrativeScenePacer::cancel() {
    reset_queue();
    scene_ = NarrativeScene::None;
    completion_ = NarrativeScene::None;
    phase_ = RefugePhase::None;
    state_ = NarrativeScenePacerState::Idle;
    waiting_ = false;
    resume_at_ms_ = 0;
}

} // namespace openu5
