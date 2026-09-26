// Batch 51 -- scripted-scene pacing, on a deterministic virtual clock.
//
// Every scene here is driven through the REAL producers (execute_command(Rest)
// and blackthorn_scene.cpp's script builders) into the REAL pacers
// (NarrativeScenePacer, BlackthornScenePacer), with the AlphaRuntime routing
// rule: offer each event to the pacer first, deliver it only when the pacer
// did not take it. The clock is an integer the test advances in 5 ms steps --
// the device loop's own vTaskDelay(5) -- so every assertion is about WHEN a
// beat was released, never about sleeping.
//
// The waits asserted are the original's, from the bytes (scene_timing.h):
// run_n_frames/delay ticks [A], tone_sweep sample holds [B] and the fizzle
// presentation floor [D]. Where a number is asserted it is always derived
// from a cited argument, and where only a relation is proven (the chord is 12
// chimes long) the relation is what is asserted.
#include "openu5/blackthorn_scene.h"
#include "openu5/commands.h"
#include "openu5/narrative_scene.h"
#include "openu5/rest.h"
#include "openu5/scene_timing.h"
#include "openu5/ui_session.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool yes, const char *name) {
    ++checks;
    if (!yes) ++failures;
    std::printf("%s %s\n", yes ? "GREEN" : "RED", name);
}

struct Release {
    GameEventKind kind;
    std::string text;
    uint32_t at;
    int note;
};

/** Camp through the real command path, with the AlphaRuntime pacer rule. */
struct CampFixture {
    GameState game{};
    TurnState turn{};
    TravelState travel{};
    CommandState commands{};
    std::array<uint8_t, 65536> tiles{};
    std::array<uint8_t, 1024> local_tiles{};
    MapData local{{17, 0}, local_tiles.data(), local_tiles.size()};
    WorldData world{tiles.data(), tiles.data(), tiles.size(), tiles.size(), &local, 1};
    RestServices rest{};
    std::array<UiTextBlock, 128> blocks{};
    UiSession ui;
    NarrativeScenePacer pacer{};
    std::vector<NarrativeSceneStep> steps = std::vector<NarrativeSceneStep>(64);
    std::vector<char> arena = std::vector<char>(2048);
    std::vector<Release> released;
    uint32_t now = 1000;
    CommandStatus last_status = CommandStatus::Success;

    CampFixture(int members, bool paced)
        : ui({blocks.data(), blocks.size()}, {this, [](void *p, const UiIntent &i) {
                  if (i.kind == UiIntentKind::Command)
                      static_cast<CampFixture *>(p)->issue(i.command.kind, i.command.hours);
              }}) {
        tiles.fill(5);
        local_tiles.fill(171);
        game.position.map = {0, 0};
        game.position.xy = {80, 80};
        game.time.hour = 12;
        game.time.minute = 55;
        game.food = 80;
        game.party.character_count = members;
        game.party.party_size = members;
        game.party.active_character = 255;
        for (int i = 0; i < members; ++i) {
            auto &m = game.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "Member%d", i);
            m.status = 'G';
            m.character_class = 'F';
            m.level = 1;
            m.max_hp = 30;
            m.current_hp = 5;
            m.strength = m.dexterity = m.intelligence = 10;
            m.exp = 0;
        }
        rest.context = this;
        rest.snap_npcs = [](void *) {};
        rest.occupied = [](void *, int32_t, int32_t, int32_t) { return false; };
        rest.karma_record = [](void *, int32_t) { return "\"Karma record\""; };
        pacer.attach({steps.data(), steps.size(), arena.data(), arena.size()});
        pacer.set_paced(paced);
    }
    void deliver(const GameEvent &e) {
        released.push_back({e.kind, e.text ? e.text : "", now, int(e.note)});
        ui.consume(e);
    }
    EventSink release_sink() {
        return {this, [](void *p, const GameEvent &e) { static_cast<CampFixture *>(p)->deliver(e); }};
    }
    ActionResult issue(CommandKind kind, int hours = 0) {
        CommandContext c{game, turn, travel, commands, world};
        c.rest_services = &rest;
        c.events = {this, [](void *p, const GameEvent &e) {
                        auto &f = *static_cast<CampFixture *>(p);
                        if (f.pacer.enqueue(e)) return; // the AlphaRuntime rule
                        f.deliver(e);
                    }};
        Command cmd{};
        cmd.kind = kind;
        cmd.hours = int16_t(hours);
        const auto result = execute_command(c, cmd);
        last_status = result.status;
        return result;
    }
    void pump() { pacer.pump(now, {}, release_sink()); }
    /** Advance the clock in 5 ms device-loop steps for `ms`. */
    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 5) {
            now += 5;
            pump();
        }
    }
    bool awaiting_key() const {
        return ui.mode() == UiMode::KeyWait && ui.request() == UiRequestId::CampAdvance;
    }
    /** Pump until the Camp getkey reaches the session, or give up. */
    bool run_to_key(uint32_t limit_ms = 60000) {
        for (uint32_t t = 0; t < limit_ms && !awaiting_key(); t += 5) {
            now += 5;
            pump();
        }
        return awaiting_key();
    }
    /** Index of the n-th release of `kind` (optionally with cue `text`), or -1. */
    int find(GameEventKind kind, int nth = 0, const char *cue = nullptr, size_t from = 0) const {
        for (size_t i = from; i < released.size(); ++i) {
            if (released[i].kind != kind) continue;
            if (cue && released[i].text != cue) continue;
            if (nth-- == 0) return int(i);
        }
        return -1;
    }
    uint32_t at(int index) const { return index >= 0 ? released[size_t(index)].at : 0; }
};

UiAction direction() {
    UiAction a{};
    a.kind = UiActionKind::Direction;
    a.direction = Direction::North;
    return a;
}

/** A seed whose one-hour Camp produces the apparition (25 %, rest.cpp). */
int apparition_seed(int members, int exp0) {
    for (int s = 1; s <= 512; ++s) {
        auto f = std::make_unique<CampFixture>(members, false);
        f->game.party.characters[0].exp = exp0;
        f->game.rng.seed(uint32_t(s));
        f->issue(CommandKind::Rest, 1);
        if (f->commands.camp_advance.phase != CommandState::CampAdvance::Phase::None) return s;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Blackthorn: drive the builders' scripts through the real pacer.
// ---------------------------------------------------------------------------
struct BlackthornFixture {
    BlackthornScenePacer pacer{};
    std::vector<BlackthornSceneStep> steps = std::vector<BlackthornSceneStep>(128);
    std::vector<char> arena = std::vector<char>(4096);
    std::array<int16_t, kBlackthornSceneCells> grid{};
    std::array<int16_t, kBlackthornSceneCells> room{};
    BlackthornSceneState state{};
    BlackthornSceneScript script{};
    uint32_t now = 1000;
    struct Sample {
        uint32_t at;
        uint32_t released;
        BlackthornStageSlot slot1, slot8;
    };
    std::vector<Sample> samples;
    explicit BlackthornFixture(uint32_t unit_ms) {
        pacer.attach({steps.data(), steps.size(), arena.data(), arena.size(), grid.data()});
        pacer.set_unit_ms(unit_ms);
        room.fill(0x44);
        init_capture_scene(state, "AFM", 3);
    }
    void offer() {
        GameEvent e{};
        e.kind = GameEventKind::BlackthornScene;
        e.blackthorn_scene = &script;
        pacer.enqueue(e);
    }
    void sample() {
        const auto v = pacer.view();
        samples.push_back({now, pacer.released_steps(), v.stage.slots[1], v.stage.slots[8]});
    }
    void pump() {
        pacer.pump(now, {});
        sample();
    }
    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += 5) {
            now += 5;
            pump();
        }
    }
    /** First sampled instant at which `pred` holds, or UINT32_MAX. */
    template <class P> uint32_t first(P pred) const {
        for (const auto &s : samples)
            if (pred(s)) return s.at;
        return UINT32_MAX;
    }
};

} // namespace

int main() {
    // ===================================================================
    // C. The Camp apparition -- OUTSUBS camp_results 0x0658.
    // ===================================================================
    check(kApparitionChordSamples == 12 * kApparitionChimeSamples,
          "C0: the bytes fix the chord at twelve chimes (0x08b5 a2=0xea60 vs 0x088a a2=0x1388) -- the "
          "one ratio two independent witnesses measured (re/notes/cadencia-delay-pit.md 3)");
    check(kApparitionArpeggioNotes == 6,
          "C0: the arpeggio loop runs si=0x3a26..<0x3a32 step 2 -- six sweeps, not three");
    check(tone_sweep_ms(kApparitionChordSamples) == 0xea60u * 1000u / 25806u &&
              tone_sweep_ms(kApparitionChordSamples) <= 2750,
          "C0: the chord hold is the calibrated fast-host floor (0xea60 samples at 24000/0.93 = 25806 "
          "per second, speaker.ts DELAY_UNIT_MS) and never exceeds either 1988 witness (2.75 s, "
          "4.57 s) -- pinned independently of the production primitive");

    // exp 100 levels member 0 to 2: one Hail wait in the first segment.
    const int seed = apparition_seed(2, 100);
    check(seed > 0, "C0: a seed exists whose one-hour Camp brings the apparition");
    if (seed < 0) {
        std::printf("Batch 51 scene pacing: %d/%d checks\n", checks - failures, checks);
        return 1;
    }

    // The unpaced control: the same seed, the same command, no pacing.
    auto sync_owner = std::make_unique<CampFixture>(2, false);
    auto &sync = *sync_owner;
    sync.game.party.characters[0].exp = 100;
    sync.game.rng.seed(uint32_t(seed));
    sync.issue(CommandKind::Rest, 1);

    auto f_owner = std::make_unique<CampFixture>(2, true);
    auto &f = *f_owner;
    f.game.party.characters[0].exp = 100;
    f.game.rng.seed(uint32_t(seed));
    const uint32_t command_at = f.now;
    f.issue(CommandKind::Rest, 1);

    check(f.game.rng.get_seed() == sync.game.rng.get_seed() &&
              f.game.party.characters[0].level == sync.game.party.characters[0].level &&
              f.game.party.characters[0].current_hp == sync.game.party.characters[0].current_hp &&
              f.game.party.characters[1].current_hp == sync.game.party.characters[1].current_hp &&
              f.commands.camp_advance.phase == sync.commands.camp_advance.phase,
          "C1: pacing is presentation only -- RNG, member state and the Camp phase after the command "
          "are identical to the unpaced run");
    check(f.pacer.active() && f.pacer.modal() && !f.awaiting_key(),
          "C2: the apparition is STAGED: after the command the Hail getkey has not yet reached the "
          "session and the pacer holds the rest of the turn (modal, input swallowed)");
    check(f.find(GameEventKind::CampViewportXor) < 0 && f.find(GameEventKind::CampActorWake) < 0,
          "C2: no apparition visual was released synchronously inside the command");

    const bool reached = f.run_to_key();
    check(reached, "C3: the paced scene reaches the first Hail getkey on its own clock");

    const int begin = f.find(GameEventKind::CampSceneBegin);
    const int wake = f.find(GameEventKind::CampActorWake);
    const int chime = f.find(GameEventKind::Sfx, 0, kApparitionChimeCue);
    const int xor_pulse = f.find(GameEventKind::CampViewportXor);
    const int chord = f.find(GameEventKind::Sfx, 0, kApparitionChordCue);
    const int restore = f.find(GameEventKind::CampViewportRestore);
    const int hail = [&] {
        for (size_t i = restore >= 0 ? size_t(restore) : 0; i < f.released.size(); ++i)
            if (f.released[i].kind == GameEventKind::Message &&
                f.released[i].text.find("Hail, Member0!") != std::string::npos)
                return int(i);
        return -1;
    }();
    const int key = f.find(GameEventKind::CampKeyWait);
    check(begin >= 0 && wake > begin && chime > wake && xor_pulse > chime && chord > xor_pulse &&
              restore > chord && hail > restore && key > hail,
          "C4: release order is the original's: figure, wake, chime, XOR, chord, restore, Hail, getkey");

    const uint32_t lead = tone_sweep_ms(kApparitionMaterializeSamples) +
                          tone_sweep_ms(kApparitionArpeggioSamples);
    check(begin >= 0 && f.at(begin) - command_at >= lead,
          "C5: the figure cannot appear before the materialize sweep and the six-note arpeggio "
          "(0x067b + 0x0686-0x06a2) have held the sleeping camp");
    check(wake >= 0 && begin >= 0 && f.at(wake) - f.at(begin) >= kFizzleFloorMs,
          "C6: the figure's own frame survives at least one presentation tick before the first "
          "member stands (fizzle 0x06c2 is not collapsed into the wake)");
    check(xor_pulse >= 0 && wake >= 0 &&
              f.at(xor_pulse) - f.at(wake) >=
                  run_n_frames_ms(kApparitionWakeFrames) + tone_sweep_ms(kApparitionChimeSamples),
          "C7: the standing member is shown for run_n_frames(1) + the heal chime before the "
          "viewport inverts (0x087f, 0x0896)");
    check(restore >= 0 && xor_pulse >= 0 &&
              f.at(restore) - f.at(xor_pulse) >= tone_sweep_ms(kApparitionChordSamples),
          "C8: the XOR frame is held for the whole chord -- the restore cannot replace it early "
          "(0x08aa XOR, 0x08c1 chord, 0x08d5 first repaint)");
    check(hail >= 0 && restore >= 0 &&
              f.at(hail) - f.at(restore) >= run_n_frames_ms(kApparitionRestoreFrames),
          "C9: the Hail text waits for the three restore frames (0x08c4-0x08d9) -- it cannot "
          "replace the inverted frame immediately");
    check(restore >= 0 && xor_pulse >= 0 &&
              f.at(restore) - f.at(xor_pulse) < tone_sweep_ms(kApparitionChordSamples) + 20,
          "C10: and the hold is the chord, not an open-ended stall (released within one device loop "
          "of its deadline)");

    // Acknowledgement: the getkey blocks progression until a key.
    const size_t before_key = f.released.size();
    f.run(10000);
    check(f.released.size() == before_key && f.awaiting_key() &&
              f.commands.camp_advance.phase == CommandState::CampAdvance::Phase::MemberKey,
          "C11: ten seconds with no key release nothing -- the Hail getkey still blocks");
    f.ui.handle_input(direction());
    check(!f.awaiting_key() && f.pacer.active(),
          "C12: the key advances the core, and the NEXT member is staged again rather than dumped");
    const size_t after_key = f.released.size();
    const bool second = f.run_to_key();
    const int wake2 = f.find(GameEventKind::CampActorWake, 0, nullptr, after_key);
    const int xor2 = f.find(GameEventKind::CampViewportXor, 0, nullptr, after_key);
    const int restore2 = f.find(GameEventKind::CampViewportRestore, 0, nullptr, after_key);
    check(second && wake2 >= 0 && xor2 > wake2 && restore2 > xor2 &&
              f.at(restore2) - f.at(xor2) >= tone_sweep_ms(kApparitionChordSamples),
          "C13: the second member gets its own full chord hold after the acknowledgement");
    check(f.commands.camp_advance.phase == CommandState::CampAdvance::Phase::KarmaKey,
          "C13: and the scene ends on the karma getkey, as the unpaced run does");

    // Unpaced harness contract: nothing is taken, nothing is deferred.
    check(sync.awaiting_key() && sync.find(GameEventKind::CampViewportXor) >= 0 &&
              !sync.pacer.active(),
          "C14: set_paced(false) keeps the synchronous delivery every existing harness relies on");

    // Capacity: the longest real segment -- six live members, none levelling.
    const int seed6 = apparition_seed(6, 0);
    if (seed6 > 0) {
        auto six_owner = std::make_unique<CampFixture>(6, true);
        auto &six = *six_owner;
        six.game.rng.seed(uint32_t(seed6));
        six.issue(CommandKind::Rest, 1);
        const bool karma = six.run_to_key();
        int pulses = 0;
        for (const auto &r : six.released) pulses += r.kind == GameEventKind::CampViewportXor;
        check(karma && six.pacer.dropped_steps() == 0 && pulses == 6 &&
                  six.commands.camp_advance.phase == CommandState::CampAdvance::Phase::KarmaKey,
              "C15: a six-member segment (42 steps) fits the pacer: no step dropped, six pulses, "
              "then the karma getkey");
    } else {
        check(false, "C15: a six-member apparition seed exists");
    }

    // ===================================================================
    // B. Blackthorn -- BLCKTHRN.OVL 0x07ed-0x087c and 0x03c9-0x0429.
    // ===================================================================
    {
        BlackthornFixture b(55);
        build_blackthorn_entry_script(b.state, b.script);
        b.offer();
        b.pump();
        b.run(8000);
        const uint32_t circle = b.first([](const BlackthornFixture::Sample &s) {
            return s.slot8.visible && s.slot8.tile == kBlackthornHolySymbolTile;
        });
        const uint32_t arrived = b.first([](const BlackthornFixture::Sample &s) {
            return s.slot8.visible && s.slot8.tile == kBlackthornTile;
        });
        // The guards' pause(8) (anim_vm 0x3702's last op) is the last tick
        // beat before the materialization; find when it ended by replaying
        // the frame budget of every beat before the sweep.
        uint32_t tick_frames = 0;
        for (uint8_t i = 0; i < b.script.count; ++i) {
            if (b.script.beats[i].sfx == BlackthornSfx::Materialize) break;
            tick_frames += uint32_t(b.script.beats[i].frames);
        }
        const uint32_t sweep_starts = 1000 + tick_frames * 55;
        check(circle != UINT32_MAX,
              "B1: the holy circle (slot 8 = 0x116, 0x0854) is actually presented -- it was staged "
              "and replaced inside a single pump, so no frame ever showed it");
        check(circle != UINT32_MAX && circle >= sweep_starts &&
                  circle - sweep_starts >= tone_sweep_ms(kBlackthornMaterializeSamples),
              "B2: Blackthorn's cell stays EMPTY for the whole materialization sweep "
              "(tone_sweep a2=0x32c8 @0x083f precedes the 0x0854 placement)");
        check(circle != UINT32_MAX && arrived != UINT32_MAX && arrived > circle &&
                  arrived - circle >= kFizzleFloorMs,
              "B3: the circle survives at least one presentation tick before the fizzle resolves to "
              "Blackthorn (0x0860 fizzle_in 0x178)");
    }
    {
        // The mount and the sacrifice are offered back to back, as one turn,
        // so the throne-room stage stays up between them (a drained queue with
        // no prompt behind it takes the stage down -- the anti-stranding rule).
        BlackthornFixture b(55);
        build_throne_mount_script(b.state, b.room.data(), b.script);
        uint32_t mount_frames = 0;
        for (uint8_t i = 0; i < b.script.count; ++i) mount_frames += uint32_t(b.script.beats[i].frames);
        b.offer();
        build_sacrifice_script(b.state, b.script);
        b.offer();
        b.pump();
        b.run(20000);
        const uint32_t pause_starts = 1000 + mount_frames * 55; // sacrifice pause(10), 0x03cd
        const bool seated = b.first([&](const BlackthornFixture::Sample &s) {
                                return s.at >= pause_starts && s.slot1.visible;
                            }) == pause_starts;
        const uint32_t dark = b.first([&](const BlackthornFixture::Sample &s) {
            return s.at >= pause_starts && !s.slot1.visible;
        });
        check(seated, "B4: precondition -- the victim is seated in the mounted room when the "
                      "sacrifice's pause(10) begins");
        check(dark != UINT32_MAX &&
                  dark - pause_starts >= run_n_frames_ms(10) + tone_sweep_ms(kBlackthornSirenSamples),
              "B4: the victim stays in place through pause(10) AND the whole siren (460+460 sweeps "
              "of a2=0xc8, 0x03d0-0x040f) before going dark");
    }
    {
        BlackthornFixture b(0);
        build_blackthorn_entry_script(b.state, b.script);
        b.offer();
        b.pump();
        check(b.pacer.state() == BlackthornPacerState::Idle &&
                  b.pacer.released_steps() == b.script.count,
              "B5: a zero unit still drains every beat synchronously (harness contract unchanged)");
    }

    // ===================================================================
    // T. Control: TrollSneak shares the pacer and must be untouched.
    // ===================================================================
    {
        auto t_owner = std::make_unique<CampFixture>(1, true);
        auto &t = *t_owner;
        TrollSneakScript script{};
        script.beats[0] = {"Thou spieth trolls under the bridge!\n", 10, false};
        script.beats[1] = {"$ sneaks across", 5, false};
        script.count = 2;
        GameEvent e{};
        e.kind = GameEventKind::TrollSneak;
        e.troll_sneak = &script;
        const uint32_t start = t.now;
        t.pacer.enqueue(e);
        GameEvent tail{};
        tail.kind = GameEventKind::Message;
        tail.text = "tail";
        t.pacer.enqueue(tail);
        t.pump();
        t.run(2000);
        const int msg = t.find(GameEventKind::Message);
        check(msg >= 0 && t.at(msg) - start == 15 * kSceneFrameUnitMs &&
                  t.pacer.scene() == NarrativeScene::None,
              "T1: TrollSneak's deferred tail follows its pause(10)+pause(5) exactly -- no Camp hold "
              "leaks into another scene's forwarded events");
    }

    std::printf("Batch 51 scene pacing: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
