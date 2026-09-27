#!/usr/bin/env python
"""Alpha 3 A3-HF3 -- mutation validation of the combat hit cue (D-63,
ALPHA3_AUDIO.md section 27).

Each mutation changes ONE production anchor -- the core pacer and trigger
(openu5/combat_hit_cue.h, src/combat_hit_cue.cpp), the runtime's wiring
(alpha_runtime.cpp) or the Board's row cache (tdeck_board.cpp) -- rebuilds the
two host targets, a3_hf3_combat_hit_cue (the pacer and the trigger) and
a3_hf3_combat_hit_runtime (the REAL runtime and Board over the fake ST7789),
and records which checks turn RED. The file is restored byte for byte and
touched (a restored file is older than the mutated object: without the touch
ninja keeps the mutant). A mutation that leaves every check GREEN is a
SURVIVOR and fails this run; one that does not build is INVALID (not killed).

Usage: python native/core/tools/a3_hf3_mutation_check.py <build-dir> <log> [K1,R2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
TARGETS = ['a3_hf3_combat_hit_cue', 'a3_hf3_combat_hit_runtime']
ARGS = {'a3_hf3_combat_hit_cue': [], 'a3_hf3_combat_hit_runtime': [RES, AUDIO]}

RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
CUE_H = 'native/core/include/openu5/combat_hit_cue.h'
CUE = 'native/core/src/combat_hit_cue.cpp'

MUTATIONS = [
    # --- the marker (0x359f) ---
    ('K1 marker suppressed', RT,
     'if(combat_source&&!debug_mode&&hit_cue_.marker(hit_x,hit_y)){',
     'if(false&&combat_source&&!debug_mode&&hit_cue_.marker(hit_x,hit_y)){'),
    ('K2 marker on the transposed cell', RT,
     'hit_op.dx=int16_t(hit_x-openu5::kPresentationWindow/2);hit_op.dy=int16_t(hit_y-openu5::kPresentationWindow/2);',
     'hit_op.dx=int16_t(hit_y-openu5::kPresentationWindow/2);hit_op.dy=int16_t(hit_x-openu5::kPresentationWindow/2);'),
    # --- the roster row (0x2a28) ---
    ('R1 the wrong member\'s row', RT,
     'cue.member=a.member!=255?int8_t(a.member):int8_t(-1);',
     'cue.member=a.member!=255?int8_t(a.member^1):int8_t(-1);'),
    ('R2 an enemy hit flashes a party row', RT,
     'cue.member=a.member!=255?int8_t(a.member):int8_t(-1);',
     'cue.member=a.member!=255?int8_t(a.member):int8_t(0);'),
    ('R3 the party highlight ignores the hit cue', RT,
     '    if(hit_cue_.flash_row()>=0)out.damage_flash=hit_cue_.flash_row();',
     '    if(false&&hit_cue_.flash_row()>=0)out.damage_flash=hit_cue_.flash_row();'),
    ('R4 the reverse video never clears', CUE_H,
     'int8_t flash_row() const { return showing_ ? on_.member : int8_t(-1); }',
     'int8_t flash_row() const { return on_.member; }'),
    # --- timing ---
    ('T1 the cue clears too early (half the burst)', CUE,
     'ends_at_ = now_ms + kCombatHitCueMs;',
     'ends_at_ = now_ms + kCombatHitCueMs / 2;'),
    ('T2 the cue is never restored', CUE,
     'if (showing_ && int32_t(now_ms - ends_at_) >= 0) {',
     'if (false && showing_ && int32_t(now_ms - ends_at_) >= 0) {'),
    ('T3 the render loop never services the cue', RT,
     '    if(service_hit_cue()){dirty_=true;dirty_reason_="combat-hit-cue";}\n',
     ''),
    ('T4 a queued hit marks no frame dirty', RT,
     '        dirty_=true;dirty_reason_="combat-hit-cue";\n        ESP_LOGI(kTag,"COMBAT_HIT_CUE',
     '        ESP_LOGI(kTag,"COMBAT_HIT_CUE'),
    ('T5 the main loop may sleep through a cue', RT,
     'if(poison_.active()||hit_cue_.active()||camp_scene_active_)return false;',
     'if(poison_.active()||camp_scene_active_)return false;'),
    # --- sequencing ---
    ('Q1 a new hit replaces the cue on screen', CUE,
     'bool CombatHitCuePacer::push(const CombatHitCue &cue, uint32_t now_ms) {\n',
     'bool CombatHitCuePacer::push(const CombatHitCue &cue, uint32_t now_ms) {\n'
     '    on_ = cue; showing_ = true; ends_at_ = now_ms + kCombatHitCueMs; return true;\n'),
    ('Q2 no restore between queued cues', CUE,
     'ready_at_ = ends_at_ + kCombatHitCueGapMs;',
     'ready_at_ = ends_at_;'),
    ('Q3 queued cues play newest first', CUE,
     '        on_ = queue_[head_];\n        head_ = uint8_t((head_ + 1) % kCombatHitCueSlots);\n',
     '        on_ = queue_[(head_ + count_ - 1) % kCombatHitCueSlots];\n'),
    # --- which events are a cue ---
    ('E1 a miss is cued', CUE,
     'if (e.kind == CombatEventKind::Attacked && e.hit > 0) return e.target;',
     'if (e.kind == CombatEventKind::Attacked && e.hit >= 0) return e.target;'),
    ('E2 the status-only strikes are not cued', CUE,
     '    if (combat_status_hit(e)) return e.target;\n',
     ''),
    ('E3 the status-only strikes stay silent', RT,
     '        else if(openu5::combat_status_hit(c))',
     '        else if(false&&openu5::combat_status_hit(c))'),
    # --- transitions and the row cache ---
    ('D1 the wiping blow tears the arena down at once', RT,
     '    if(hit_cue_.active())return false;\n',
     ''),
    ('C1 the row cache ignores reverse video', BOARD,
     'cached.color==color&&cached.invert==invert&&',
     'cached.color==color&&'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe')] + ARGS[t])
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF3 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = invalid = 0
    only = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    selected = [m for m in MUTATIONS if not only or m[0].split()[0] in only]
    for label, rel, old, new in selected:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        count = text.count(o)
        if count != 1:
            lines.append('%s: ANCHOR NOT UNIQUE (%d) in %s' % (label, count, rel))
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                invalid += 1
                continue
            results = test()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for t, code, reds in results:
            lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
            lines += ['        ' + r[:200] for r in reds]
    bcode, _ = build()
    restored = test()
    lines.append('')
    lines.append('restored: build exit=%d; %s' % (bcode, ', '.join('%s exit=%d' % (t, c) for t, c, _ in restored)))
    lines.append('mutations: %d, killed: %d, survived: %d, invalid: %d' %
                 (len(selected), len(selected) - survivors - invalid, survivors, invalid))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    sys.exit(1 if survivors or invalid or bcode or any(c for _, c, _ in restored) else 0)


if __name__ == '__main__':
    main()
