// Alpha 3 A3-HF3 -- D-63: the arena hit cue (ULTIMA.EXE 0x3564), core half.
// The cue's length against the burst the device renders, the queue's
// sequencing, and which combat events are a 0x3564 cue at all
// (ALPHA3_AUDIO.md section 27). The device half is a3_hf3_combat_hit_runtime.
#include "openu5/combat_hit_cue.h"
#include "openu5/sfx_synth.h"
#include "openu5/world_fx.h"

#include <cstdio>
#include <string>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
CombatHitCue cue(int x, int y, int member) {
    CombatHitCue c;
    c.x = int8_t(x);
    c.y = int8_t(y);
    c.member = int8_t(member);
    return c;
}
bool showing(const CombatHitCuePacer &p, int x, int y, int member) {
    int8_t mx = -1, my = -1;
    return p.marker(mx, my) && mx == x && my == y && p.flash_row() == member;
}
bool dark(const CombatHitCuePacer &p) {
    int8_t mx, my;
    return !p.marker(mx, my) && p.flash_row() == -1;
}
} // namespace

int main() {
    // ---- C: the length is the burst's --------------------------------------
    {
        SpeakerProgram heavy{}, light{};
        const bool ok = compile_sfx(SfxId::CombatHitHeavy, 0, heavy) && compile_sfx(SfxId::CombatHit, 0, light);
        check(ok && heavy.half_samples() == kCombatHitCueHalfSamples && light.half_samples() == kCombatHitCueHalfSamples,
              "C1 the cue lasts exactly the device's CombatHitHeavy / CombatHit programs (9,000 half samples each)");
        check(kCombatHitCueMs == 174 && kCombatHitCueHalfSamples == 9000,
              "C2 9,000 half samples at 2 x 25,806 Hz = 174 ms (174.38)");
        check(kCombatHitMarkerTile == 0 && kCombatHitMarkerTile == kWorldFxExplosionTile,
              "C3 the marker is tile 0, the tile explosion_fx_at_cell (0x3522) blits too");
    }
    // ---- P: the pacer ----------------------------------------------------------
    {
        CombatHitCuePacer p;
        check(!p.active() && dark(p), "P0 control: an idle pacer shows nothing");
        p.push(cue(5, 5, 1), 1000);
        check(p.active() && showing(p, 5, 5, 1), "P1 a cue starts at the instant it is pushed: marker + row");
        p.pump(1000 + kCombatHitCueMs - 1);
        check(showing(p, 5, 5, 1), "P2 it is held for 173 ms");
        const bool changed = p.pump(1000 + kCombatHitCueMs);
        check(changed && dark(p) && !p.active(), "P3 at 174 ms it ends; nothing is left on");
    }
    {
        CombatHitCuePacer p;
        p.push(cue(4, 4, 0), 0);
        p.push(cue(4, 6, 2), 0);
        const bool first = showing(p, 4, 4, 0) && p.queued() == 1;
        p.pump(kCombatHitCueMs);
        const bool gap = dark(p) && p.active();
        p.pump(kCombatHitCueMs + kCombatHitCueGapMs - 1);
        const bool still_gap = dark(p);
        p.pump(kCombatHitCueMs + kCombatHitCueGapMs);
        const bool second = showing(p, 4, 6, 2);
        p.pump(2 * kCombatHitCueMs + kCombatHitCueGapMs);
        check(first && gap && still_gap && second && dark(p) && !p.active() && p.shown() == 2,
              "P4 two cues in one instant play in order, 174 ms each, a 55 ms restore between, none lost");
    }
    {
        // A triple strike: three cues in one instant. With two, the first starts at
        // once and the order of the rest is trivially right; three expose it.
        CombatHitCuePacer p;
        p.push(cue(1, 1, -1), 0);
        p.push(cue(2, 2, -1), 0);
        p.push(cue(3, 3, -1), 0);
        const bool a = showing(p, 1, 1, -1);
        p.pump(kCombatHitCueMs);
        p.pump(kCombatHitCueMs + kCombatHitCueGapMs);
        const bool b = showing(p, 2, 2, -1);
        p.pump(2 * kCombatHitCueMs + kCombatHitCueGapMs);
        p.pump(2 * (kCombatHitCueMs + kCombatHitCueGapMs));
        const bool c = showing(p, 3, 3, -1);
        check(a && b && c, "P4b three cues in one instant play in event order (first in, first shown)");
    }
    {
        CombatHitCuePacer p;
        p.push(cue(4, 4, 0), 0);
        p.push(cue(4, 4, 0), 0);
        p.pump(kCombatHitCueMs);
        const bool off = dark(p);
        p.pump(kCombatHitCueMs + kCombatHitCueGapMs);
        check(off && showing(p, 4, 4, 0) && p.shown() == 2, "P5 the same cell and row twice: two cues, restored between");
    }
    {
        CombatHitCuePacer p;
        p.push(cue(6, 5, -1), 0);
        check(showing(p, 6, 5, -1) && p.flash_row() == -1, "P6 an enemy's cue marks the cell and inverts no row");
        p.pump(kCombatHitCueMs);
        p.push(cue(6, 5, -1), 5000); // long after: no restore wait is owed
        check(showing(p, 6, 5, -1), "P7 a cue pushed after the pacer went quiet starts at once");
    }
    {
        CombatHitCuePacer p;
        p.push(cue(1, 1, 0), 0); // showing
        bool all = true;
        for (size_t i = 0; i < kCombatHitCueSlots; ++i) all = all && p.push(cue(1, 1, 0), 0);
        const bool full = !p.push(cue(2, 2, 1), 0);
        check(all && full && p.dropped() == 1 && p.queued() == kCombatHitCueSlots,
              "P8 32 cues wait behind the one showing; the 33rd waiting one is refused and counted");
        p.cancel();
        check(!p.active() && dark(p) && p.queued() == 0, "P9 cancel drops everything");
    }
    {
        CombatHitCuePacer p;
        const uint32_t t = 0xffffffffu - 50;
        p.push(cue(3, 3, 1), t);
        p.pump(t + 100); // wraps
        const bool held = showing(p, 3, 3, 1);
        p.pump(t + kCombatHitCueMs);
        check(held && dark(p), "P10 the millisecond clock may wrap under a cue");
    }
    // ---- E: which events are a 0x3564 cue ------------------------------------
    {
        CombatEvent hit{};
        hit.kind = CombatEventKind::Attacked;
        hit.actor = 3;
        hit.target = 7;
        hit.hit = 1;
        CombatEvent graze = hit;
        graze.grazed = 1;
        graze.damage = 0;
        CombatEvent miss = hit;
        miss.hit = 0;
        CombatEvent absent = hit;
        absent.hit = -1;
        CombatEvent died{};
        died.kind = CombatEventKind::Died;
        died.target = 7;
        CombatEvent poisoned{};
        poisoned.kind = CombatEventKind::Message;
        poisoned.target = 7;
        poisoned.text = "Iolo is poisoned!";
        CombatEvent slept = poisoned;
        slept.text = "troll slept!";
        CombatEvent untargeted = poisoned;
        untargeted.target = -1;
        CombatEvent passes{};
        passes.kind = CombatEventKind::Message;
        passes.actor = 7;
        passes.text = "Iolo passes out!";
        CombatEvent other = poisoned;
        other.text = "Iolo critical!";
        CombatEvent moved{};
        moved.kind = CombatEventKind::Moved;
        moved.target = 7;
        check(combat_hit_cue_target(hit) == 7 && combat_hit_cue_target(graze) == 7,
              "E1 a hit and a graze are cues on their target (0x3564 precedes 0x194A's graze)");
        check(combat_hit_cue_target(poisoned) == 7 && combat_hit_cue_target(slept) == 7 &&
                  combat_status_hit(poisoned) && combat_status_hit(slept),
              "E2 the status-only strikes 'is poisoned!' / 'slept!' are cues on their target");
        check(combat_hit_cue_target(miss) == -1 && combat_hit_cue_target(absent) == -1 &&
                  combat_hit_cue_target(died) == -1 && combat_hit_cue_target(untargeted) == -1 &&
                  combat_hit_cue_target(passes) == -1 && combat_hit_cue_target(other) == -1 &&
                  combat_hit_cue_target(moved) == -1 && !combat_status_hit(hit),
              "E3 control: a miss, a lone Died, an untargeted or other message and a move are not cues");
    }
    std::printf("a3_hf3_combat_hit_cue: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
