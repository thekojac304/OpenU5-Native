#include "openu5/blackthorn_scene.h"

#include <algorithm>
#include <cstring>

// Native port of game/src/core/world/blackthorn-scene.ts. Every script below
// is the same anim_vm bytecode that module cites, expanded the same way; the
// bytes themselves are quoted in re/notes/blackthorn-escena-324.md sections 1
// and 2 and are not repeated here. Nothing in this file reads or writes
// GameState.
namespace openu5 {
namespace {

// Seats by living count (DS 0x1f0a + n*8), then the seat's cell.
constexpr int8_t kSeatCodes[7][6] = {
    {0, 0, 0, 0, 0, 0},
    {4, 0, 0, 0, 0, 0},
    {4, 5, 0, 0, 0, 0},
    {4, 5, 0, 0, 0, 0},
    {4, 5, 3, 0, 0, 0},
    {4, 5, 2, 1, 0, 0},
    {4, 5, 3, 2, 1, 0},
};
/** x of a seat code (DS 0x1f42) and y (DS 0x1f48). Codes 4/5 are the two
 *  manacles flanking the throne: the Avatar at (3,5), a companion at (7,5). */
constexpr int8_t kSeatX[6] = {0, 1, 9, 10, 3, 7};
constexpr int8_t kSeatY[6] = {1, 1, 1, 1, 5, 5};

/** Actor tile by class letter (DS 0x701a "AMBFDTPRS" -> DS 0x1ade, +0x100). */
int16_t class_tile(char letter) {
    switch (letter) {
    case 'M': return 0x140; // Wizard1
    case 'B': return 0x144; // Bard1
    case 'F': return 0x148; // Fighter1
    default: return 0x14c;  // Avatar1 -- also the binary's own fallback
    }
}

constexpr int8_t kGuardAStartX = 4, kGuardAStartY = 10;
constexpr int8_t kGuardBStartX = 6, kGuardBStartY = 10;
constexpr int8_t kBlackthornCellX = 5, kBlackthornCellY = 5;
constexpr int8_t kHourglassX = 5, kHourglassY = 9;
constexpr int8_t kTortureX = 5, kTortureY = 7;
constexpr int8_t kWestDoorX = 0, kWestDoorY = 4;

/** Mirror of the reference's runAnim over a mutable scene state. */
struct Builder {
    BlackthornSceneState *state;
    BlackthornSceneScript &out;

    explicit Builder(BlackthornSceneScript &script, BlackthornSceneState *s = nullptr)
        : state(s), out(script) {
        out.count = 0;
    }

    BlackthornBeat overflow{};
    BlackthornBeat &emit() {
        if (out.count >= kBlackthornMaxBeats) return overflow = BlackthornBeat{};
        auto &b = out.beats[out.count++];
        b = BlackthornBeat{};
        return b;
    }
    BlackthornStage snapshot() const {
        BlackthornStage stage{};
        if (state)
            for (int i = 0; i < kBlackthornSlots; ++i) stage.slots[i] = state->objects[i];
        return stage;
    }
    void stamp(BlackthornBeat &b) {
        b.stage = snapshot();
        b.has_stage = true;
    }
    /** anim_vm op 5 -- run-n-frames(n). */
    void pause(int16_t frames) { emit().frames = frames; }
    /** anim_vm op 8 -- beep_delay(n): n footsteps, each with its own pause(2). */
    void footsteps(int n) {
        for (int i = 0; i < n; ++i) {
            auto &b = emit();
            b.frames = kBlackthornStepFrames;
            b.footstep = true;
        }
    }
    /** anim_vm op >= 0x10 -- one or two slots stepping, `times` repeats. */
    void step(int slot_a, int dx_a, int dy_a, int times) {
        step2(slot_a, dx_a, dy_a, -1, 0, 0, times);
    }
    void step2(int slot_a, int dx_a, int dy_a, int slot_b, int dx_b, int dy_b, int times) {
        for (int t = 0; t < times; ++t) {
            if (state) {
                auto move = [&](int slot, int dx, int dy) {
                    if (slot < 0 || slot >= kBlackthornSlots) return;
                    auto &o = state->objects[slot];
                    if (!o.present) return;
                    o.x = int8_t(o.x + dx);
                    o.y = int8_t(o.y + dy);
                };
                move(slot_a, dx_a, dy_a);
                move(slot_b, dx_b, dy_b);
            }
            auto &b = emit();
            stamp(b);
            b.frames = kBlackthornStepFrames;
            b.footstep = true;
        }
    }
    /** anim_vm op 6 -- plot a tile into the room buffer. */
    void plot(int8_t x, int8_t y, int16_t tile) {
        auto &b = emit();
        b.patch = {x, y, tile, true};
    }
    /** anim_vm op 9 -- clear an object's tiles; x/y are deliberately retained. */
    void remove(int slot) {
        if (state && slot >= 0 && slot < kBlackthornSlots) state->objects[slot].visible = false;
        stamp(emit());
    }
};

} // namespace

void init_capture_scene(BlackthornSceneState &state, const char *classes, int living) {
    state = BlackthornSceneState{};
    const int n = std::min(std::max(living, 0), 6);
    const auto &codes = kSeatCodes[n];
    for (int i = 0; i < n; ++i) {
        const int code = std::min<int>(std::max<int>(codes[i], 0), 5);
        auto &o = state.objects[i];
        o.x = kSeatX[code];
        o.y = kSeatY[code];
        o.tile = class_tile(classes ? classes[i] : 0);
        o.visible = true;
        o.present = true;
    }
}

void build_blackout_intro_script(BlackthornSceneScript &out) {
    Builder b(out);
    // pause(2) at 0x0672, then the five {delay_ticks(5) + footstep} drags of
    // 0x069b-0x06ae. The whole segment runs behind the blindfold.
    auto &first = b.emit();
    first.frames = 2;
    first.blackout = true;
    for (int i = 0; i < 5; ++i) {
        auto &beat = b.emit();
        beat.frames = kBlackthornDragFrames;
        beat.footstep = true;
        beat.blackout = true;
    }
}

void build_throne_mount_script(BlackthornSceneState &state, const int16_t *tiles,
                               BlackthornSceneScript &out) {
    Builder b(out, &state);
    // Eighteen more drags (0x06b9-0x06e0), then the room itself mounts with
    // the party already seated, over the pause(0x10) of 0x07d1.
    for (int i = 0; i < 18; ++i) {
        auto &beat = b.emit();
        beat.frames = kBlackthornDragFrames;
        beat.footstep = true;
        beat.blackout = true;
    }
    auto &mount = b.emit();
    b.stamp(mount);
    mount.tiles = tiles;
    mount.mount = true;
    mount.frames = 0x10;
}

void build_chained_pause_script(BlackthornSceneScript &out) {
    Builder b(out);
    b.pause(0x32); // 0x07df, between "chained and manacled!" and "Footsteps!"
}

void build_blackthorn_entry_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    b.footsteps(8); // beep_delay(8) at 0x07ed -- the approaching guards
    state.objects[6] = {kGuardAStartX, kGuardAStartY, kBlackthornGuardTile, true, true};
    state.objects[7] = {kGuardBStartX, kGuardBStartY, kBlackthornGuardTile, true, true};
    b.stamp(b.emit());
    // anim_vm 0x3702: beep_delay(1), both guards N, then W/E x3 -> (1,9)/(9,9).
    b.footsteps(1);
    b.step2(6, 0, -1, 7, 0, -1, 1);
    b.step2(6, -1, 0, 7, 1, 0, 3);
    b.pause(8);
    // tone_sweep 0x082b, the holy circle (0x0842), then Blackthorn (0x0863).
    // The LFSR fizzle texture itself is not modelled; he appears on the cut.
    state.objects[8] = {kBlackthornCellX, kBlackthornCellY, kBlackthornHolySymbolTile, true, true};
    auto &materialize = b.emit();
    b.stamp(materialize);
    materialize.sfx = BlackthornSfx::Materialize;
    state.objects[8].tile = kBlackthornTile;
    auto &arrive = b.emit();
    b.stamp(arrive);
    arrive.frames = 8;
}

void build_guard_release_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    // anim_vm 0x370e: guard A leaves his post for the Avatar -- and then
    // Blackthorn says "Wait!".
    b.pause(11);
    b.step(6, -1, 0, 1);
    b.step(6, 0, -1, 4);
    b.step(6, 1, 0, 1);
}

void build_warning_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    // anim_vm 0x36da: guard A marches the companion (slot 1) to the torture
    // table at (5,7) -- the object is cleared and the cell becomes the
    // chained body -- and returns to (1,5); guard B plants the full hourglass
    // at (5,9) and goes back to (9,9).
    b.pause(22);
    b.step(6, 0, 1, 1);
    b.step(6, 1, 0, 7);
    b.step(1, 0, 1, 1);
    b.step2(6, -1, 0, 1, -1, 0, 2);
    b.step(1, 0, 1, 1);
    b.pause(3);
    b.plot(kTortureX, kTortureY, kBlackthornTortureBodyTile);
    b.remove(1);
    b.step(6, -1, 0, 5);
    b.step(6, 0, -1, 1);
    b.pause(12);
    b.step(7, -1, 0, 3);
    b.plot(kHourglassX, kHourglassY, kBlackthornHourglassFullTile);
    b.step(7, 1, 0, 3);
}

bool build_hourglass_script(int round, BlackthornSceneScript &out) {
    // interrogate 0x05da (round 1) / 0x05e2 (round 2). The 0x05d2 case is
    // unreachable in the binary (warned is always armed in round 0) and is
    // deliberately not ported -- see the reference module's note.
    int16_t tile = 0;
    if (round == 1) tile = kBlackthornHourglassRound1Tile;
    else if (round == 2) tile = kBlackthornHourglassRound2Tile;
    else return false;
    Builder b(out);
    b.plot(kHourglassX, kHourglassY, tile);
    return true;
}

void build_sacrifice_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    // sacrifice_member 0x03ae: pause(10), the two mirrored tone_sweep loops
    // (the same five arguments as the shard ritual, so the same cue), then the
    // explosion at slot 1's cell -- emitted as a sibling CellExplosion event --
    // the victim's object going dark, and the table left empty (0x0429).
    b.pause(10);
    auto &siren = b.emit();
    siren.sfx = BlackthornSfx::ShardSweep;
    state.objects[1].visible = false;
    auto &after = b.emit();
    b.stamp(after);
    after.patch = {kTortureX, kTortureY, kBlackthornTortureAfterTile, true};
}

void build_finale_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    // anim_vm 0x369e: the west secret door opens, guard A escorts the Avatar
    // out to the cell corridor, the door shuts, then Blackthorn and both
    // guards leave by the south door and six footsteps fade away.
    b.step(6, -1, 0, 1);
    b.plot(kWestDoorX, kWestDoorY, kBlackthornWestDoorOpenTile);
    b.step(6, 1, 0, 2);
    b.step2(6, -1, 0, 0, -1, 0, 2);
    b.step(6, 0, 1, 1);
    b.step(6, 1, 0, 2);
    b.step(6, 0, -1, 1);
    b.step2(6, -1, 0, 0, -1, 0, 1);
    b.step2(6, -1, 0, 0, 0, -1, 1);
    b.step2(6, 0, -1, 0, 0, -1, 1);
    b.step(0, 0, -1, 1);
    b.step(6, 0, 1, 1);
    b.plot(kWestDoorX, kWestDoorY, kBlackthornWestDoorShutTile);
    b.footsteps(1);
    b.step(8, 1, 0, 1);
    b.step2(8, 0, 1, 6, 0, 1, 4);
    b.step(8, 0, 1, 1);
    b.step(6, 1, 0, 1);
    b.remove(8);
    b.step(6, 1, 0, 3);
    b.step(6, 0, 1, 1);
    b.remove(6);
    b.step(7, -1, 0, 3);
    b.step(7, 0, 1, 1);
    b.remove(7);
    b.footsteps(6);
    if (out.count) out.beats[out.count - 1].dismount = true;
}

void build_blackthorn_exit_script(BlackthornSceneState &state, BlackthornSceneScript &out) {
    Builder b(out, &state);
    // anim_vm 0x3716, only when slot 8 is still standing (the 0x08d9 gate).
    b.pause(4);
    b.step(8, 1, 0, 1);
    b.step(8, 0, 1, 5);
    b.remove(8);
    if (out.count) out.beats[out.count - 1].dismount = true;
}

bool blackthorn_on_stage(const BlackthornSceneState &state) {
    return state.objects[8].present && state.objects[8].visible;
}

void sacrifice_victim_cell(const BlackthornSceneState &state, int &x, int &y) {
    const auto &victim = state.objects[1];
    if (victim.present) {
        x = victim.x;
        y = victim.y;
    } else {
        x = kTortureX;
        y = kTortureY;
    }
}

PresentationSnapshot compose_blackthorn_presentation(const BlackthornSceneView &view) {
    PresentationSnapshot s;
    // The scene owns its own 11x11 window; centring it on (5,5) keeps every
    // cell-offset consumer (the explosion's dx/dy, the render report) reading
    // the same geometry the reference uses.
    s.center = {kBlackthornSceneCols / 2, kBlackthornSceneRows / 2};
    const bool throne = view.phase == BlackthornScenePhase::Throne && view.tiles;
    for (int i = 0; i < kBlackthornSceneCells; ++i) {
        // The blindfold is 0x0676 set_color(0) + 0x0689 fill_rect over the
        // viewport: kPresentationHidden is exactly the "draw nothing here"
        // value render_snapshot already leaves black.
        s.tiles[i] = throne ? view.tiles[i] : kPresentationHidden;
        s.visible[i] = throne ? 1 : 0;
    }
    if (throne) {
        for (int slot = 0; slot < kBlackthornSlots; ++slot) {
            const auto &f = view.stage.slots[slot];
            if (!f.present || !f.visible) continue;
            if (f.x < 0 || f.y < 0 || f.x >= kBlackthornSceneCols || f.y >= kBlackthornSceneRows)
                continue;
            s.tiles[f.y * kBlackthornSceneCols + f.x] = f.tile;
        }
        // The throne room's braziers are the ordinary fire tiles, and the
        // hourglass is an ordinary animated group: classify them exactly as
        // the world composer does so the shared rasterizer keeps them alive
        // instead of freezing the room between beats.
        for (int i = 0; i < kBlackthornSceneCells; ++i) {
            const auto kind = tile_animation_kind(s.tiles[i]);
            // ActorProgram cells are deliberately excluded: the scene stages
            // its cast at fixed reference cells and runs no actor clock, so
            // marking them animated would only cost redraws.
            const bool animated = kind != TileAnimationKind::Static && kind != TileAnimationKind::ActorProgram;
            s.animated[i] = animated ? 1 : 0;
            s.any_animated = s.any_animated || animated;
        }
    }
    return s;
}

// ---------------------------------------------------------------------------
// BlackthornScenePacer
// ---------------------------------------------------------------------------

bool BlackthornScenePacer::push(const GameEvent &e, BlackthornStepKind kind,
                                const BlackthornBeat *beat) {
    if (!storage_.steps || count_ >= storage_.step_capacity) {
        ++dropped_;
        return false;
    }
    auto &step = storage_.steps[(head_ + count_) % storage_.step_capacity];
    step = BlackthornSceneStep{};
    step.kind = kind;
    step.event = e;
    // Every borrowed payload pointer dies with the synchronous delivery that
    // produced it; the scene queue outlives that, so none of them may survive.
    step.event.text = nullptr;
    step.event.combat = nullptr;
    step.event.dialogue = nullptr;
    step.event.shop = nullptr;
    step.event.endgame = nullptr;
    step.event.npc = nullptr;
    step.event.refuge = nullptr;
    step.event.troll_sneak = nullptr;
    step.event.zodiac = nullptr;
    step.event.blackthorn_scene = nullptr;
    step.event.sign_raw = nullptr;
    step.event.sign_raw_size = 0;
    if (beat) step.beat = *beat;
    if (e.text && storage_.text) {
        const size_t length = std::strlen(e.text);
        if (text_used_ + length + 1 <= storage_.text_capacity) {
            step.text_offset = uint32_t(text_used_);
            step.text_length = uint32_t(length);
            std::memcpy(storage_.text + text_used_, e.text, length + 1);
            text_used_ += length + 1;
        } else {
            ++dropped_;
        }
    }
    ++count_;
    return true;
}

bool BlackthornScenePacer::enqueue(const GameEvent &e) {
    if (e.kind == GameEventKind::BlackthornScene) {
        const auto *script = e.blackthorn_scene;
        if (!script) return true; // A scene event with no payload draws nothing.
        if (state_ == BlackthornPacerState::Idle) {
            reset_queue();
            state_ = BlackthornPacerState::Running;
            waiting_ = false;
        } else if (state_ == BlackthornPacerState::Prompt) {
            state_ = BlackthornPacerState::Running;
        }
        for (uint8_t i = 0; i < script->count; ++i) {
            GameEvent beat_event{};
            beat_event.kind = GameEventKind::BlackthornScene;
            push(beat_event, BlackthornStepKind::Beat, &script->beats[i]);
        }
        return true;
    }
    if (state_ == BlackthornPacerState::Idle) return false;
    const auto kind = e.kind == GameEventKind::ShrineKeyWait    ? BlackthornStepKind::KeyWait
                      : e.kind == GameEventKind::BlackthornPrompt ? BlackthornStepKind::Prompt
                      : e.kind == GameEventKind::Message          ? BlackthornStepKind::Message
                                                                  : BlackthornStepKind::Forward;
    push(e, kind, nullptr);
    if (state_ == BlackthornPacerState::Prompt) state_ = BlackthornPacerState::Running;
    return true;
}

void BlackthornScenePacer::apply(const BlackthornBeat &beat) {
    if (beat.blackout && phase_ == BlackthornScenePhase::Inactive)
        phase_ = BlackthornScenePhase::Blackout;
    if (beat.mount && beat.tiles && storage_.grid) {
        std::memcpy(storage_.grid, beat.tiles, sizeof(int16_t) * kBlackthornSceneCells);
        phase_ = BlackthornScenePhase::Throne;
    }
    if (beat.patch.valid && storage_.grid && beat.patch.x >= 0 && beat.patch.y >= 0 &&
        beat.patch.x < kBlackthornSceneCols && beat.patch.y < kBlackthornSceneRows)
        storage_.grid[beat.patch.y * kBlackthornSceneCols + beat.patch.x] = beat.patch.tile;
    if (beat.has_stage) stage_ = beat.stage;
    // The deposit (0x08e7) restores the screen. Take the room down the instant
    // the beat that says so is applied, not when the queue finally drains:
    // the map/party events that follow it must not be released over a stage
    // that is still on screen, or the jail would flash in behind the throne.
    if (beat.dismount) {
        saw_dismount_ = true;
        tear_down_stage();
    }
}

void BlackthornScenePacer::pump(uint32_t now_ms, EventSink out) {
    if (state_ != BlackthornPacerState::Running) return;
    while (state_ == BlackthornPacerState::Running) {
        if (waiting_) {
            if (unit_ms_ && int32_t(now_ms - resume_at_ms_) < 0) return;
            waiting_ = false;
        }
        if (!count_) {
            // The turn's deferred tail is exhausted. Either the room is still
            // up because an interrogation prompt is now waiting on the player
            // (the reference keeps the throne room behind every question), or
            // there is nothing left to show and the scene ends. The second
            // arm is also the anti-stranding rule: a scene may never stay
            // mounted with no queue and no prompt behind it.
            // Nothing references the text arena once the queue is empty --
            // every released message was copied on by the sink -- so reclaim
            // it here. Without this, a scene that survives four interrogation
            // rounds would keep accumulating narrative into a buffer sized
            // for one turn.
            text_used_ = 0;
            if (phase_ != BlackthornScenePhase::Inactive && awaiting_prompt_) {
                state_ = BlackthornPacerState::Prompt;
                return;
            }
            tear_down_stage();
            state_ = BlackthornPacerState::Idle;
            saw_dismount_ = false;
            return;
        }
        auto &step = storage_.steps[head_];
        head_ = (head_ + 1) % storage_.step_capacity;
        --count_;
        ++released_;
        if (step.kind == BlackthornStepKind::Beat) {
            apply(step.beat);
            if (step.beat.frames > 0) {
                if (unit_ms_) {
                    resume_at_ms_ = now_ms + uint32_t(step.beat.frames) * unit_ms_;
                    waiting_ = true;
                    return;
                }
            }
            continue;
        }
        awaiting_prompt_ = step.kind == BlackthornStepKind::Prompt;
        if (step.kind == BlackthornStepKind::KeyWait) {
            state_ = BlackthornPacerState::AwaitingKey;
            // Under a zero unit (host harnesses and every automated digest)
            // the getkey points drain straight through, exactly as the
            // reference pacer does with unitMs 0.
            if (!unit_ms_) state_ = BlackthornPacerState::Running;
            return;
        }
        GameEvent released = step.event;
        if (step.text_length || step.text_offset) released.text = storage_.text + step.text_offset;
        else if (step.event.kind == GameEventKind::Message) released.text = "";
        if (out.emit) out.emit(out.context, released);
    }
}

bool BlackthornScenePacer::advance_key() {
    if (state_ != BlackthornPacerState::AwaitingKey) return false;
    state_ = BlackthornPacerState::Running;
    waiting_ = false;
    return true;
}

void BlackthornScenePacer::tear_down_stage() {
    phase_ = BlackthornScenePhase::Inactive;
    stage_ = BlackthornStage{};
}

void BlackthornScenePacer::reset_queue() {
    head_ = 0;
    count_ = 0;
    text_used_ = 0;
}

void BlackthornScenePacer::cancel() {
    reset_queue();
    tear_down_stage();
    state_ = BlackthornPacerState::Idle;
    saw_dismount_ = false;
    waiting_ = false;
    awaiting_prompt_ = false;
}

BlackthornSceneView BlackthornScenePacer::view() const {
    BlackthornSceneView v;
    v.phase = phase_;
    v.tiles = phase_ == BlackthornScenePhase::Throne ? storage_.grid : nullptr;
    v.stage = stage_;
    return v;
}

} // namespace openu5
