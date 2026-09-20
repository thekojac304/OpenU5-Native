// Batch 7B -- the five remaining Y-04 feedback/VFX channels.
//
// Each section restates, through the mechanism that now exists, the claim the
// matching section of `batch7b_red_test.cpp` failed to satisfy against HEAD,
// and then pins the reference-derived timing and ordering the adjudication
// established. The derivations themselves live in the headers
// (openu5/world_fx.h, openu5/poison_tick.h, openu5/narrative_scene.h); this
// file only asserts them.
#include "openu5/commands.h"
#include "openu5/narrative_scene.h"
#include "openu5/poison_tick.h"
#include "openu5/presentation.h"
#include "openu5/quest_world.h"
#include "openu5/ui_session.h"
#include "openu5/world_fx.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
int checks = 0;
void check(bool ok, const char *what) {
    ++checks;
    if (ok) return;
    std::cerr << "batch7b check " << checks << " failed: " << what << "\n";
    std::exit(1);
}

struct Spy {
    static void send(void *, const UiIntent &) {}
};

bool transcript_contains(const UiSession &ui, const char *needle) {
    for (size_t i = 0; i < ui.transcript_size(); ++i) {
        const auto *b = ui.transcript_at(i);
        if (b && std::strstr(b->text, needle)) return true;
    }
    return false;
}

/** The device's own beat routing, reproduced exactly: append, or continue. */
struct BeatRouter {
    UiSession *ui = nullptr;
    std::vector<std::string> cues;
    std::vector<RefugePhase> phases;
    size_t beats = 0;
    static void on_beat(void *p, const NarrativeSceneBeat &beat) {
        auto &self = *static_cast<BeatRouter *>(p);
        ++self.beats;
        if (beat.phase != RefugePhase::None) self.phases.push_back(beat.phase);
        if (beat.text) {
            if (beat.append) self.ui->append_continuation(UiTextChannel::Message, beat.text);
            else self.ui->append(UiTextChannel::Message, beat.text);
        }
        if (beat.sfx) self.cues.emplace_back(beat.sfx);
    }
    NarrativeSceneSink sink() { return {this, on_beat}; }
};

struct Forwarded {
    std::vector<GameEventKind> kinds;
    std::vector<std::string> texts;
    static void emit(void *p, const GameEvent &e) {
        auto &self = *static_cast<Forwarded *>(p);
        self.kinds.push_back(e.kind);
        self.texts.emplace_back(e.text ? e.text : "");
    }
    EventSink sink() { return {this, emit}; }
};

struct Objects {
    std::vector<QuestObject> values;
};
size_t obj_count(void *p) { return static_cast<Objects *>(p)->values.size(); }
QuestObject obj_read(void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; }
bool obj_reserve(void *p, size_t n) {
    static_cast<Objects *>(p)->values.reserve(n);
    return true;
}
void obj_append(void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); }
void obj_erase(void *p, size_t i) {
    auto &v = static_cast<Objects *>(p)->values;
    v.erase(v.begin() + ptrdiff_t(i));
}
void obj_write(void *p, size_t i, const QuestObject &o) {
    static_cast<Objects *>(p)->values[i] = o;
}
const char *karma_record(void *, int32_t) { return "\"Thou art a paragon of virtue.\""; }
} // namespace

int main() {
    // =====================================================================
    // A -- CellProjectile (#313). One flight per shot, 55 ms/cell (borrowed
    // Class C), origin and destination as offsets from the party, painted as
    // a sub-cell dot. No world tile is ever written.
    // =====================================================================
    {
        WorldFx shot;
        shot.kind = WorldFxKind::Projectile;
        shot.from_dx = 0; // broadside: `push 5 / push 5` -- the ship itself
        shot.from_dy = 0;
        shot.to_dx = 3; // the full ray when nothing was hit (0x0AD2)
        shot.to_dy = 0;
        check(WorldFxLayer::projectile_cells(shot) == 3, "A1 Chebyshev path length");
        check(WorldFxLayer::duration_ms(shot) == 3 * kWorldFxProjectileMsPerCell,
              "A2 flight lasts one cadence unit per cell");

        WorldFx diagonal = shot;
        diagonal.to_dx = 2;
        diagonal.to_dy = -3;
        check(WorldFxLayer::projectile_cells(diagonal) == 3, "A3 Chebyshev, not Manhattan");
        WorldFx degenerate = shot;
        degenerate.to_dx = 0;
        check(WorldFxLayer::projectile_cells(degenerate) == 1, "A4 zero-length flight never divides by zero");

        WorldFxLayer layer;
        check(layer.push(shot, 1000), "A5 the layer accepts the flight");
        int32_t dx = 0, dy = 0;
        check(layer.projectile_at(1000, dx, dy) && dx == 0 && dy == 0,
              "A6 the ball leaves the origin cell");
        // Half way through: 82 ms of 165 -> 3 cells * 496/1000, just under the
        // midpoint, by the truncating form of the reference's interpolation.
        check(layer.projectile_at(1000 + 82, dx, dy) && dx == 1488 && dy == 0,
              "A7 the ball travels cell by cell, between cells");
        check(layer.projectile_at(1000 + 164, dx, dy) && dx > 2900 && dx < 3000,
              "A8 the ball is still short of its destination on the last frame");
        check(!layer.projectile_at(1000 + 165, dx, dy),
              "A9 the flight is over when its duration is");

        // A live flight paints a Dot and nothing else -- never a cell blit,
        // so no world tile can be mutated to show it.
        WorldFxOp ops[8]{};
        size_t n = layer.paint(1000 + 82, ops, 8);
        check(n == 1 && ops[0].kind == WorldFxOpKind::Dot, "A10 the flight paints a sub-cell dot only");
        PresentationSnapshot window;
        for (int i = 0; i < kPresentationCells; ++i) window.tiles[i] = 5;
        check(apply_world_fx(window, ops, n) == 0, "A11 a dot never lands on a world tile");
        for (int i = 0; i < kPresentationCells; ++i)
            check(window.tiles[i] == 5, "A12 the composed window is untouched by the flight");
        // ...and it expires by itself, with no caller bookkeeping.
        check(layer.remaining_ms(1000 + 82) == 83, "A13 the layer reports what it still owes");
        layer.paint(1000 + 165, ops, 8);
        check(!layer.active() && layer.remaining_ms(1000 + 165) == 0,
              "A14 the layer expires the flight on its own");

        // The cannon on foot leaves the CANNON's cell, not the party's
        // (0x0BAD/0x0BB9 are never rewritten in the loop).
        WorldFx on_foot;
        on_foot.kind = WorldFxKind::Projectile;
        on_foot.from_dx = 1;
        on_foot.from_dy = 0;
        on_foot.to_dx = 4;
        on_foot.to_dy = 0;
        WorldFxLayer foot_layer;
        foot_layer.push(on_foot, 0);
        check(foot_layer.projectile_at(0, dx, dy) && dx == 1000 && dy == 0,
              "A15 the cannon's ball leaves the cannon's cell");
    }

    // =====================================================================
    // B -- CellExplosion (#201/#243). `bursts` alternating blits of tile 0
    // over one cell, after `pre_delay_units` of 0x3ae6 and after whatever
    // lead the owner's own shake costs, with `under_tile` held underneath for
    // the WHOLE choreography. Presentation only.
    // =====================================================================
    {
        WorldFx ritual; // the shard ritual's own payload: {0,-1,7,3,252}
        ritual.kind = WorldFxKind::Explosion;
        ritual.dx = 0;
        ritual.dy = -1;
        ritual.bursts = 7;
        ritual.pre_delay_units = 3;
        ritual.under_tile = 252;
        ritual.lead_ms = 0;
        check(WorldFxLayer::duration_ms(ritual) ==
                  3 * kWorldFxPauseUnitMs + 7 * kWorldFxExplosionBurstMs,
              "B1 pause then bursts is the whole duration");

        WorldFxLayer layer;
        layer.push(ritual, 0);
        WorldFxOp ops[8]{};
        // During the pause the cell shows the Shadowlord and NOTHING else:
        // the original is silent here, it does not paint.
        size_t n = layer.paint(10, ops, 8);
        check(n == 1 && ops[0].kind == WorldFxOpKind::Blit &&
                  ops[0].tile == int16_t(252 + kWorldFxSpriteBank),
              "B2 the Shadowlord holds the cell through the pause");
        check(ops[0].dx == 0 && ops[0].dy == -1, "B3 the cell is the party's own, one north");

        // First burst: under tile first, explosion on top.
        const uint32_t pause_end = 3 * kWorldFxPauseUnitMs;
        n = layer.paint(pause_end, ops, 8);
        check(n == 2, "B4 the burst is painted over the held tile, not beside it");
        check(ops[0].tile == int16_t(252 + kWorldFxSpriteBank), "B5 held tile is the lower layer");
        check(ops[1].tile == kWorldFxExplosionTile && ops[1].kind == WorldFxOpKind::Blit,
              "B6 the burst blits TileData's Explosion");

        // The blits ALTERNATE -- the original repaints the viewport between
        // one and the next, so the cell flickers instead of holding a tile
        // still for 420 ms.
        n = layer.paint(pause_end + kWorldFxExplosionBurstMs, ops, 8);
        check(n == 1, "B7 the second burst slot is the gap");
        n = layer.paint(pause_end + 2 * kWorldFxExplosionBurstMs, ops, 8);
        check(n == 2, "B8 the third burst slot paints again");
        n = layer.paint(pause_end + 6 * kWorldFxExplosionBurstMs, ops, 8);
        check(n == 2, "B9 the last burst slot paints");

        // ...and the state write is NOT deferred to make this work: the
        // overlay lands on an already-composed, already-committed window.
        PresentationSnapshot window;
        for (int i = 0; i < kPresentationCells; ++i) {
            window.tiles[i] = 143; // the Flame the core already committed
            window.visible[i] = 1;
        }
        constexpr int half = kPresentationWindow / 2;
        const int cell = (half - 1) * kPresentationWindow + half;
        check(apply_world_fx(window, ops, n) == 2, "B10 both layers land on the window");
        check(window.tiles[cell] == kWorldFxExplosionTile,
              "B11 RED R3b: the burst is on the cell");
        WorldFxOp under_only[1]{ops[0]};
        PresentationSnapshot held;
        for (int i = 0; i < kPresentationCells; ++i) held.tiles[i] = 143;
        apply_world_fx(held, under_only, 1);
        check(held.tiles[cell] == int16_t(252 + kWorldFxSpriteBank),
              "B12 RED R3a: the Shadowlord is what the cell shows under the burst");

        layer.paint(WorldFxLayer::duration_ms(ritual), ops, 8);
        check(!layer.active(), "B13 the explosion expires on its own");

        // LEAD: everything the owner's own clock owes before the pause even
        // starts (in native, the live viewport shake). The fx is ALIVE
        // throughout -- that is what holds `under_tile` on the cell.
        WorldFx led = ritual;
        led.lead_ms = 2808; // three quakes of eight pulses at 117 ms
        WorldFxLayer lead_layer;
        lead_layer.push(led, 0);
        n = lead_layer.paint(1000, ops, 8);
        check(n == 1 && ops[0].tile == int16_t(252 + kWorldFxSpriteBank),
              "B14 during the shake only the held tile is painted");
        n = lead_layer.paint(2808 + pause_end, ops, 8);
        check(n == 2, "B15 the burst starts once the shake window has closed");

        // The Blackthorn sacrifice uses the SAME primitive with its own shape:
        // one burst, no pause, no held tile (0x0414-0x041E).
        WorldFx sacrifice;
        sacrifice.kind = WorldFxKind::Explosion;
        sacrifice.dx = 0;
        sacrifice.dy = 2;
        sacrifice.bursts = 1;
        check(WorldFxLayer::duration_ms(sacrifice) == kWorldFxExplosionBurstMs,
              "B16 a single-burst explosion shares the same primitive");
        WorldFxLayer sac_layer;
        sac_layer.push(sacrifice, 0);
        n = sac_layer.paint(0, ops, 8);
        check(n == 1 && ops[0].tile == kWorldFxExplosionTile,
              "B17 with no held tile only the burst is painted");

        // Clipping: a cell outside the 11x11 window is dropped, as the
        // original's blit is clipped by the viewport it draws into.
        WorldFxOp off_window[1]{};
        off_window[0].kind = WorldFxOpKind::Blit;
        off_window[0].tile = kWorldFxExplosionTile;
        off_window[0].dx = 9;
        off_window[0].dy = 0;
        check(apply_world_fx(window, off_window, 1) == 0, "B18 off-window ops are clipped away");
    }

    // =====================================================================
    // C -- PoisonTick (#213). Slot order, one 93 ms blip each, the shared
    // roster row inversion, no modality.
    // =====================================================================
    {
        check(kPoisonBlipMs == 93, "C1 the blip is the synthesized cue's own length");
        PoisonFlashPacer pacer;
        const uint8_t slots[] = {0, 2, 3};
        pacer.run(slots, 3, 1000);
        check(pacer.active() && pacer.flash_row() == 0, "C2 the first slot's row inverts at once");
        check(pacer.cues() == 1, "C3 its damage cue sounds with it");
        // It cannot advance before the cue it is pacing has played out.
        check(!pacer.pump(1000 + kPoisonBlipMs - 1), "C4 no advance before the blip elapses");
        check(pacer.flash_row() == 0, "C5 the row stays inverted meanwhile");
        check(pacer.pump(1000 + kPoisonBlipMs), "C6 the blip elapses and the next slot takes over");
        check(pacer.flash_row() == 2 && pacer.cues() == 2, "C7 SLOT order, ascending, skipping the healthy");
        check(pacer.pump(1000 + 2 * kPoisonBlipMs) && pacer.flash_row() == 3,
              "C8 the third poisoned member follows");
        check(pacer.cues() == 3, "C9 one cue per poisoned member, never overlapping");
        check(pacer.active(), "C10 the last flash is still on screen");
        check(pacer.pump(1000 + 3 * kPoisonBlipMs), "C11 the last blip elapses");
        check(pacer.flash_row() == -1 && !pacer.active(),
              "C12 0x2a6e of the last member: the row goes back to normal");

        // Cancel (skin change, save load) kills the timer and the flash.
        pacer.run(slots, 3, 5000);
        check(pacer.flash_row() == 0, "C13 a second script starts clean");
        pacer.cancel();
        check(pacer.flash_row() == -1 && !pacer.active(), "C14 cancel switches the flash off");

        // Drain-synchronously mode: every cue, no timers, no flash.
        PoisonFlashPacer quiet;
        quiet.set_blip_ms(0);
        quiet.run(slots, 3, 0);
        check(quiet.cues() == 3 && !quiet.active() && quiet.flash_row() == -1,
              "C15 a zero blip drains synchronously, as the reference does under automation");

        // The precedence of the three markers that share 0x2a28.
        check(roster_invert_row(2, 1, 4, 5) == 2, "C16 the damage flash wins while it lasts");
        check(roster_invert_row(-1, 1, 4, 5) == 1, "C17 then the Ztats cursor");
        check(roster_invert_row(-1, -1, 4, 5) == 4, "C18 then the picker cursor");
        check(roster_invert_row(-1, -1, -1, 5) == 5, "C19 then the combat actor");
        check(roster_invert_row(-1, -1, -1, -1) == -1, "C20 and otherwise no inverted row");
    }

    // =====================================================================
    // D -- TrollSneak (MAINOUT 0x1c0e-0x1ca6): a staged modal scene whose
    // beats cannot be collapsed, with the rest of the turn deferred behind it.
    // =====================================================================
    {
        UiTextBlock blocks[64];
        Spy spy;
        UiSession ui{{blocks, 64}, {&spy, Spy::send}, {8, 3, 64}};
        NarrativeSceneStep steps[64];
        char arena[2048];
        NarrativeScenePacer pacer;
        pacer.attach({steps, 64, arena, sizeof(arena)});
        BeatRouter router;
        router.ui = &ui;
        Forwarded tail;

        TrollSneakScript script;
        script.beats[script.count++] = {"\nThou spieth trolls under the bridge!\n\n", 10, false};
        script.beats[script.count++] = {"Shamino sneaks across", 5, false};
        script.beats[script.count++] = {".", 5, true};
        script.beats[script.count++] = {".", 5, true};
        script.beats[script.count++] = {".", -1, true};
        script.beats[script.count++] = {"\n", -1, false};
        script.beats[script.count++] = {"Trolls evaded!\n", -1, false};
        GameEvent scene;
        scene.kind = GameEventKind::TrollSneak;
        scene.troll_sneak = &script;
        check(pacer.enqueue(scene), "D1 the pacer takes ownership of the scene");
        // The rest of the turn is emitted synchronously by the core and must
        // be DEFERRED: releasing it now would print `Caught!` before the party
        // has been seen sneaking.
        GameEvent caught;
        caught.kind = GameEventKind::Message;
        caught.text = "Caught!";
        check(pacer.enqueue(caught), "D2 the rest of the turn is deferred behind the scene");
        GameEvent prompt;
        prompt.kind = GameEventKind::TrollTollPrompt;
        prompt.note = 30;
        check(pacer.enqueue(prompt), "D3 the toll prompt is deferred too");

        uint32_t now = 10000;
        pacer.pump(now, router.sink(), tail.sink());
        check(pacer.active() && pacer.modal(), "D4 the scene is up and input is swallowed");
        check(transcript_contains(ui, "Thou spieth trolls under the bridge!"),
              "D5 RED R1a: the preamble reaches the player");
        check(router.beats == 1, "D6 only the first beat has been released");
        check(tail.kinds.empty(), "D7 nothing of the deferred turn has escaped");

        // pause(10) of 0x3AE6: the preamble cannot be replaced before ~549 ms.
        check(pacer.resume_at_ms() == now + 10 * kSceneFrameUnitMs, "D8 pause(10) = ten frame ticks");
        pacer.pump(now + 10 * kSceneFrameUnitMs - 1, router.sink(), tail.sink());
        check(router.beats == 1, "D9 the preamble holds for its whole pause");
        now += 10 * kSceneFrameUnitMs;
        pacer.pump(now, router.sink(), tail.sink());
        check(router.beats == 2 && transcript_contains(ui, "Shamino sneaks across"),
              "D10 the name follows once the pause has elapsed");

        // pause(5) before EACH dot (0x1c56-0x1c65), and each dot CONTINUES
        // the line rather than starting a new one.
        now += 5 * kSceneFrameUnitMs;
        pacer.pump(now, router.sink(), tail.sink());
        check(transcript_contains(ui, "Shamino sneaks across."), "D11 first dot, same line");
        check(!transcript_contains(ui, "Shamino sneaks across.."), "D12 and only the first");
        now += 5 * kSceneFrameUnitMs;
        pacer.pump(now, router.sink(), tail.sink());
        check(transcript_contains(ui, "Shamino sneaks across.."), "D13 second dot");
        check(tail.kinds.empty(), "D14 the turn is still deferred mid-scene");
        // The third dot prints dry (no pause of its own), so it flushes with
        // everything else that has no dwell -- exactly the reference's own
        // drain loop.
        now += 5 * kSceneFrameUnitMs;
        pacer.pump(now, router.sink(), tail.sink());
        check(transcript_contains(ui, "Shamino sneaks across..."),
              "D15 RED R1b: the three dots are one line");
        check(transcript_contains(ui, "Trolls evaded!"), "D16 RED R1c: the outcome is printed");

        // Only NOW may the deferred turn run.
        check(tail.kinds.size() == 2, "D17 the deferred turn is released when the beats run out");
        check(tail.kinds[0] == GameEventKind::Message && tail.texts[0] == "Caught!",
              "D18 in the order the core emitted it");
        check(tail.kinds[1] == GameEventKind::TrollTollPrompt, "D19 prompt last");
        check(!pacer.active() && !pacer.modal(), "D20 input returns when the scene ends");
        check(pacer.take_completion() == NarrativeScene::TrollSneak, "D21 the scene reports completion");
        check(pacer.take_completion() == NarrativeScene::None, "D22 exactly once");
        check(pacer.phase() == RefugePhase::None, "D23 TrollSneak mounts no viewport scene");
    }

    // =====================================================================
    // E -- Refuge (BLCKTHRN 0x0910): the death + resurrection staging. Phases
    // accumulate over a black viewport; the state mutation happens only after
    // the last beat, so the roster keeps showing the FALLEN party throughout.
    // =====================================================================
    {
        // The figures, per phase, and where they are blitted.
        RefugeSceneFigure figures[4]{};
        check(refuge_scene_figures(RefugePhase::Void, 0x11c, figures, 4) == 1,
              "E1 the Avatar is alone in the nothingness");
        check(figures[0].col == 5 && figures[0].row == 5 && figures[0].tile == 0x11c,
              "E2 in the centre of the window");
        check(refuge_scene_figures(RefugePhase::GhostLeft, 0x11c, figures, 4) == 2,
              "E3 0x0a70 adds the left figure");
        check(figures[1].tile == kRefugeGhostLeftTile && figures[1].col == 2 && figures[1].row == 7,
              "E4 tile 0x5e at (col2,row7)");
        check(refuge_scene_figures(RefugePhase::GhostBoth, 0x11c, figures, 4) == 3,
              "E5 0x0aa2 adds the right figure");
        check(figures[2].tile == kRefugeGhostRightTile && figures[2].col == 8 && figures[2].row == 7,
              "E6 tile 0x5f at (col8,row7)");
        check(refuge_scene_figures(RefugePhase::Apparition, 0x11c, figures, 4) == 4,
              "E7 0x0ae9 adds the apparition");
        check(figures[3].tile == kRefugeApparitionTile && figures[3].col == 5 && figures[3].row == 2,
              "E8 tile 0x174 at (col5,row2)");
        check(refuge_scene_figures(RefugePhase::Vertigo, 0x11c, figures, 4) == 4,
              "E9 the vertigo flash keeps the whole cast");
        check(refuge_scene_figures(RefugePhase::None, 0x11c, figures, 4) == 0,
              "E10 no scene, no figures");

        // The viewport is BLACK: every uncovered cell is the hidden value the
        // rasterizer already leaves black (0x0962).
        auto window = compose_refuge_presentation(RefugePhase::Apparition, 0x11c);
        int painted = 0;
        for (int i = 0; i < kPresentationCells; ++i)
            if (window.tiles[i] != kPresentationHidden) ++painted;
        check(painted == 4, "E11 only the cast is painted; the rest is black");
        check(window.tiles[5 * kPresentationWindow + 5] == 0x11c, "E12 the Avatar holds the centre");
        check(window.tiles[2 * kPresentationWindow + 5] == kRefugeApparitionTile,
              "E13 the apparition is top-centre");
        check(window.tiles[7 * kPresentationWindow + 2] == kRefugeGhostLeftTile, "E14 left ghost");
        check(window.tiles[7 * kPresentationWindow + 8] == kRefugeGhostRightTile, "E15 right ghost");

        // ---- the paced scene, over a real check_refuge / resolve_refuge ----
        std::vector<uint8_t> tiles(32 * 32, 5);
        MapData small{{17, 1}, tiles.data(), tiles.size()};
        WorldData world{};
        world.small_maps = &small;
        world.small_map_count = 1;
        GameState game{};
        game.position = {{4, 4}, {13, 0}};
        game.karma = 40;
        game.time.hour = 21;
        game.party.character_count = game.party.party_size = 2;
        for (int i = 0; i < 2; ++i) {
            auto &ch = game.party.characters[i];
            ch.status = 'D';
            ch.current_hp = 0;
            ch.max_hp = 40;
        }
        TurnState turn{};
        TravelState travel{};
        CommandState commands{};
        CommandContext context{game, turn, travel, commands, world};
        Objects objects{};
        QuestWorldServices quest{};
        quest.context = &objects;
        quest.count = obj_count;
        quest.read = obj_read;
        quest.reserve = obj_reserve;
        quest.append = obj_append;
        quest.erase = obj_erase;
        quest.write = obj_write;
        quest.karma_record = karma_record;
        context.quest_world = &quest;

        UiTextBlock blocks[64];
        Spy spy;
        UiSession ui{{blocks, 64}, {&spy, Spy::send}, {8, 3, 64}};
        NarrativeSceneStep steps[64];
        char arena[2048];
        NarrativeScenePacer pacer;
        pacer.attach({steps, 64, arena, sizeof(arena)});
        BeatRouter router;
        router.ui = &ui;
        Forwarded tail;

        struct Capture {
            NarrativeScenePacer *pacer;
            static void emit(void *p, const GameEvent &e) {
                static_cast<Capture *>(p)->pacer->enqueue(e);
            }
        } capture{&pacer};
        check(check_refuge(context, {&capture, Capture::emit}) == CommandStatus::Success,
              "E16 a wiped party emits the refuge script");
        check(quest.refuge_pending, "E17 and latches the pending scene");
        check(game.party.characters[0].status == 'D',
              "E18 check_refuge mutates NOTHING: the party is still fallen");

        uint32_t now = 100000;
        pacer.pump(now, router.sink(), tail.sink());
        check(pacer.active() && pacer.modal(), "E19 the scene is modal from its first beat");
        check(pacer.phase() == RefugePhase::Void && pacer.mounted(),
              "E20 0x0962: the viewport goes black with the Avatar alone");
        check(transcript_contains(ui, "An unending darkness engulfs thee..."),
              "E21 RED R2a: the narration reaches the player");
        check(router.beats == 1, "E22 one beat, not the whole script");

        // The first beat prints, so it carries the reading floor on top of
        // its ten raw `delay` units.
        check(pacer.resume_at_ms() == now + 10 * kRefugeUnitMs + kRefugeTextFloorMs,
              "E23 delay units plus the reading floor");
        pacer.pump(pacer.resume_at_ms() - 1, router.sink(), tail.sink());
        check(router.beats == 1, "E24 the line stays up for its whole interval");
        check(!transcript_contains(ui, "Thou hast found refuge."), "E25 and the next one waits");

        // Drive the scene to its end, one due beat at a time. Nothing about
        // the world may change while it runs.
        int guard = 0;
        while (pacer.active() && guard++ < 64) {
            now = pacer.waiting() ? pacer.resume_at_ms() : now;
            pacer.pump(now, router.sink(), tail.sink());
            if (pacer.active())
                check(game.party.characters[0].status == 'D' && game.position.map.location == 13,
                      "E26 no state mutation happens while the scene is on screen");
        }
        check(guard < 64, "E27 the scene terminates on its own");
        check(transcript_contains(ui, "Thou hast found refuge."), "E28 RED R2b: every beat is printed");
        check(transcript_contains(ui, "\"FORTIS FORTUNA AVENTARI\""), "E29 the shout");
        check(transcript_contains(ui, "paragon of virtue"), "E30 the karma speech, read at DEATH karma");
        check(transcript_contains(ui, "Vertigo..."), "E31 the transition line");
        check(router.cues.size() == 2 && router.cues[0] == "refuge-thunder",
              "E32 both peals of thunder (0x0acc/0x0acf)");
        check(router.phases.size() == 5, "E33 five visual phases");
        check(router.phases[0] == RefugePhase::Void && router.phases[1] == RefugePhase::GhostLeft &&
                  router.phases[2] == RefugePhase::GhostBoth &&
                  router.phases[3] == RefugePhase::Apparition &&
                  router.phases[4] == RefugePhase::Vertigo,
              "E34 in the order the blits occur");
        check(!pacer.mounted(), "E35 the scene comes down before anything is revealed");
        check(pacer.take_completion() == NarrativeScene::Refuge,
              "E36 and only then is the resurrection due");

        // The owner's half of the contract, in the reference's order.
        check(resolve_refuge(context, tail.sink()) == CommandStatus::Success, "E37 resolve_refuge runs");
        check(game.party.characters[0].status == 'G' && game.party.characters[0].current_hp == 40,
              "E38 the party is revived");
        check(game.karma == 75, "E39 karma floors at 75");
        check(game.position.map.location == 17 && game.position.xy.x == 10 &&
                  game.position.xy.y == 10,
              "E40 and wakes in Lord British's castle");
        check(game.time.hour == 6 && game.time.minute == 0, "E41 with the clock at 6:00");
        check(!quest.refuge_pending, "E42 the pending latch is cleared");
    }

    // =====================================================================
    // F -- the sequencer's shared contract, and the drain-synchronously mode
    // every automated harness relies on.
    // =====================================================================
    {
        NarrativeSceneStep steps[64];
        char arena[2048];
        NarrativeScenePacer pacer;
        pacer.attach({steps, 64, arena, sizeof(arena)});
        pacer.set_paced(false);
        BeatRouter router;
        UiTextBlock blocks[64];
        Spy spy;
        UiSession ui{{blocks, 64}, {&spy, Spy::send}, {8, 3, 64}};
        router.ui = &ui;
        Forwarded tail;
        TrollSneakScript script;
        script.beats[script.count++] = {"\nThou spieth trolls under the bridge!\n\n", 10, false};
        script.beats[script.count++] = {"Trolls evaded!\n", -1, false};
        GameEvent scene;
        scene.kind = GameEventKind::TrollSneak;
        scene.troll_sneak = &script;
        pacer.enqueue(scene);
        pacer.pump(0, router.sink(), tail.sink());
        check(!pacer.active() && router.beats == 2,
              "F1 an unpaced pacer drains the whole scene in one pump");

        // A scene event with no payload stages nothing but is still owned, so
        // it can never leak to a consumer that would print a bare kind.
        NarrativeScenePacer bare;
        bare.attach({steps, 64, arena, sizeof(arena)});
        GameEvent empty;
        empty.kind = GameEventKind::Refuge;
        check(bare.enqueue(empty) && !bare.active(), "F2 an empty script stages nothing");
        GameEvent unrelated;
        unrelated.kind = GameEventKind::Message;
        unrelated.text = "hello";
        check(!bare.enqueue(unrelated), "F3 and an idle pacer passes ordinary events through");

        // cancel() is the load/mode-change escape hatch: no stranded scene.
        NarrativeScenePacer stuck;
        stuck.attach({steps, 64, arena, sizeof(arena)});
        stuck.enqueue(scene);
        stuck.pump(0, router.sink(), tail.sink());
        check(stuck.active(), "F4 a paced scene is live after its first beat");
        stuck.cancel();
        check(!stuck.active() && !stuck.mounted() &&
                  stuck.take_completion() == NarrativeScene::None,
              "F5 cancel tears the scene down without faking a completion");
    }

    // =====================================================================
    // G -- the Batch 7 channels this batch must not regress.
    // =====================================================================
    {
        check(quake_offset_at(0) == kQuakeAmplitudePx, "G1 Quake starts on the down phase");
        check(quake_offset_at(kQuakeDownMs) == 0, "G2 and rests for the remainder of the pulse");
        check(quake_offset_at(kQuakeDurationMs) == 0, "G3 and is over after its pulses");
        check(quake_offset_at(kQuakePeriodMs) == kQuakeAmplitudePx, "G4 the next pulse strikes");
        // MapReveal's bypass is a composer flag, not a flood: still reachable.
        std::vector<uint8_t> tiles(32 * 32, 5);
        tiles[3 * 32 + 3] = 0x0a; // an opaque wall
        MapData small{{13, 0}, tiles.data(), tiles.size()};
        WorldData world{};
        world.small_maps = &small;
        world.small_map_count = 1;
        GameState game{};
        game.position = {{16, 16}, {13, 0}};
        game.time.hour = 0; // night: the radius is small
        TurnState turn{};
        TravelState travel{};
        CommandState commands{};
        CommandContext context{game, turn, travel, commands, world};
        auto active = get_active_map(world, {13, 0});
        check(active.error == Error::None, "G5 map setup");
        auto dark = compose_world_presentation(context, active.value, {16, 16}, 0x11c, false);
        auto lit = compose_world_presentation(context, active.value, {16, 16}, 0x11c, true);
        int dark_visible = 0, lit_visible = 0;
        for (int i = 0; i < kPresentationCells; ++i) {
            dark_visible += dark.visible[i] ? 1 : 0;
            lit_visible += lit.visible[i] ? 1 : 0;
        }
        check(lit_visible == kPresentationCells, "G6 MapReveal uncensors the whole window");
        check(dark_visible < lit_visible, "G7 and the ordinary composition is still censored");
    }

    std::cout << "batch7b: " << checks << " checks passed\n";
    return 0;
}
