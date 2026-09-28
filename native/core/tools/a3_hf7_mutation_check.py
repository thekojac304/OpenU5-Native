#!/usr/bin/env python
"""Alpha 3 A3-HF7 (H-184) -- mutation validation of the rite's viewport negative and the Codex's pulses.

Each mutation changes ONE production anchor -- the effect model
(native/core/src/ritual_fx.cpp), the DialoguePacer's Effect hold
(dialogue_pacer.{h,cpp}), its device wiring and the render XOR
(alpha_runtime.cpp) or one of the core's effect sites (shrine.cpp) --
rebuilds the host targets below and records which checks turn RED. The file is
restored byte for byte and touched (a restored file is older than the mutated
object: without the touch ninja keeps the mutant). A mutation that leaves every
target GREEN is a SURVIVOR and fails this run; one that does not build is
INVALID (not killed).

Targets: a3_hf7_ritual_fx_runtime (the real runtime + Board, pixels),
a3_hf7_ritual_fx (the model and the queue alone), and the A3-HF6 / A3-HF5
suites the same queue serves.

Usage: python native/core/tools/a3_hf7_mutation_check.py <build-dir> <log> [M1,M2,...]
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
TARGETS = ['a3_hf7_ritual_fx_runtime', 'a3_hf7_ritual_fx', 'a3_hf6_shrine_key_wait_runtime',
           'a3_hf6_shrine_key_wait_pacer', 'a3_hf5_dialogue_pacing_runtime', 'a3_hf5_dialogue_pacer']

FX = 'native/core/src/ritual_fx.cpp'
PACER = 'native/core/src/dialogue_pacer.cpp'
PACER_H = 'native/core/include/openu5/dialogue_pacer.h'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
SHRINE = 'native/core/src/shrine.cpp'

XOR_LOOP = ('        for(size_t p=0;p<openu5::kViewportPixelCount;++p)\n'
            '            viewport_[p]=magic_xor_palette_pixel(viewport_[p],tile_cache_.palette,viewport_xor);')
WELL_DONE = ('            d.emit(GameEventKind::RitualInvert); d.sound("shrine-well-done"); '
             'd.emit(GameEventKind::Quake); d.sound("quake");')
CODEX_TAIL = (r'''    d.emit(GameEventKind::PartyChanged); d.wait();
    if (lesson.ceremony) {
        // H-184: each shake is bracketed by an XOR of the viewport (CAST2
        // 0x0dbd/0x0dd4/0x0deb); the note carries its colour register.
        for (unsigned i=0;i<3;++i) { d.emit(GameEventKind::Quake,nullptr,kCodexPulseMasks[i]); d.sound("quake"); }
''')
CODEX_EARLY = (r'''    d.emit(GameEventKind::PartyChanged);
    if (lesson.ceremony) for (unsigned i=0;i<3;++i) { d.emit(GameEventKind::Quake,nullptr,kCodexPulseMasks[i]); d.sound("quake"); }
    d.wait();
    if (lesson.ceremony) {
''')

MUTATIONS = [
    # --- the effect itself ---
    ('M1 omit the rite inversion (the XOR mask is 0)', FX,
     '        mask_ ^= kRitualInvertMask;', '        mask_ ^= 0;'),
    ('M2 invert the wrong region (half the viewport)', RT, XOR_LOOP,
     XOR_LOOP.replace('p<openu5::kViewportPixelCount;', 'p<openu5::kViewportPixelCount/2;')),
    ('M3 the render ignores the rite (mask never drawn)', RT,
     '^ritual_fx_.mask());', '^0U);'),
    ('M4 the donation / WELL DONE negative is never restored', FX,
     '    } else if (e.kind == GameEventKind::PartyChanged && loose_) {',
     '    } else if (e.kind == GameEventKind::PartyChanged && loose_ && false) {'),
    ('M5 restored too early (at the shake, not after the rewards)', FX,
     '    } else if (e.kind == GameEventKind::PartyChanged && loose_) {',
     '    } else if (e.kind == GameEventKind::Quake && loose_) {'),
    ('M6 the Codex negative is never restored (no getkey redraw)', FX,
     '    } else if (e.kind == GameEventKind::ShrineKeyWait && pulses_) {',
     '    } else if (e.kind == GameEventKind::ShrineKeyWait && pulses_ && false) {'),
    ('M7 two Codex pulses instead of three', SHRINE,
     'd.emit(GameEventKind::Quake,nullptr,kCodexPulseMasks[i]);',
     'd.emit(GameEventKind::Quake,nullptr,i<2?kCodexPulseMasks[i]:0);'),
    ('M8 the pulses do not accumulate (each replaces the last)', FX,
     '        mask_ ^= uint8_t(e.note);', '        mask_ = uint8_t(e.note);'),
    # --- cadence ---
    ('M9 the sweep hold is half its length', FX,
     '    if (cue(e, kWellDoneCue)) return tone_sweep_ms(kWellDoneSweepSamples);',
     '    if (cue(e, kWellDoneCue)) return tone_sweep_ms(kWellDoneSweepSamples / 2);'),
    ('M10 the shakes do not block', FX,
     '    if (e.kind == GameEventKind::Quake && (codex_pulse_quake(e) || loose_)) return uint32_t(kQuakeDurationMs);',
     '    if (e.kind == GameEventKind::Quake && (codex_pulse_quake(e) || loose_)) return 0;'),
    ('M11 no run_n_frames(10) after the restore', FX,
     '    if (e.kind == GameEventKind::PartyChanged && loose_) return run_n_frames_ms(kRitualRestoreFrames);',
     '    if (e.kind == GameEventKind::PartyChanged && loose_) return 0;'),
    ('M12 no getkey idle tick: the text and the restore share a frame', FX,
     '    if (e.kind == GameEventKind::Message && pulses_ && !loose_) return run_n_frames_ms(kGetkeyRedrawFrames);',
     '    if (e.kind == GameEventKind::Message && pulses_ && !loose_) return 0;'),
    ('M13 chained effects drift (each hold restarts at the noticing frame)', PACER,
     '        release(now_ms - resume_at_ms_ <= kSceneTickMs ? resume_at_ms_ : now_ms, out);',
     '        release(now_ms, out);'),
    ('M14 the effect hold is not wired on the device', RT,
     '    dialogue_pacer_.set_effect_hold({this,ritual_hold_ms});', '    (void)&AlphaRuntime::ritual_hold_ms;'),
    # --- order ---
    ('M15 the Codex pulses before the page\'s getkey (0x0d9f)', SHRINE, CODEX_TAIL, CODEX_EARLY),
    ('M16 the WELL DONE XOR after the shake instead of before it', SHRINE, WELL_DONE,
     '            d.sound("shrine-well-done"); d.emit(GameEventKind::Quake); d.sound("quake"); '
     'd.emit(GameEventKind::RitualInvert);'),
    # --- input ---
    ('M17 a key ends an effect (input leaks through)', PACER,
     '    if (state_ != DialoguePacerState::Effect) release(now_ms, out);', '    release(now_ms, out);'),
    ('M18 one key cuts the effect AND the getkey behind it', PACER,
     '    if (state_ != DialoguePacerState::Effect) release(now_ms, out);',
     '    if (state_ != DialoguePacerState::Effect) release(now_ms, out);\n'
     '    else { release(now_ms, out); if (state_ == DialoguePacerState::Key) release(now_ms, out); }'),
    ('M19 the effect wears the getkey cue (awaiting_key during an Effect)', PACER_H,
     '    bool awaiting_key() const { return state_ == DialoguePacerState::Key; }',
     '    bool awaiting_key() const { return state_ != DialoguePacerState::Idle; }'),
    # --- lifecycle ---
    ('M20 a load / Return to Title leaves the negative on screen', RT,
     '    ritual_fx_.clear();', '    (void)ritual_fx_;'),
    ('M21 the unpaced harness holds effects too', PACER,
     '    if (!pause_ms_) return false;', '    if (!pause_ms_ && !effect_ms(e)) return false;'),
    # --- one real site bypassed ---
    ('M22 the donation site emits no RitualInvert', SHRINE,
     r'd.message("ALAKAZAM!\n"); d.emit(GameEventKind::RitualInvert,"donation");', r'd.message("ALAKAZAM!\n");'),
    ('M23 the WELL DONE site emits no RitualInvert', SHRINE, WELL_DONE,
     WELL_DONE.replace('d.emit(GameEventKind::RitualInvert); ', '')),
    ('M24 the Codex quakes carry no pulse', SHRINE,
     'd.emit(GameEventKind::Quake,nullptr,kCodexPulseMasks[i]);', 'd.emit(GameEventKind::Quake,nullptr,0);'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe'), RES])
        reds = [l for l in out.splitlines() if l.startswith(('RED', 'FAIL', '[FAIL'))]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF7 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
            print(lines[-1], flush=True)
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                print(lines[-1], flush=True)
                invalid += 1
                continue
            results = test()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        print(lines[-1], flush=True)
        for t, code, reds in results:
            if code or reds:
                lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
                lines += ['        ' + r[:200] for r in reds[:12]]
    bcode, _ = build()
    restored = test()
    lines.append('')
    lines.append('restored: build exit=%d; %s' % (bcode, ', '.join('%s exit=%d' % (t, c) for t, c, _ in restored)))
    lines.append('mutations: %d, killed: %d, survived: %d, invalid: %d' %
                 (len(selected), len(selected) - survivors - invalid, survivors, invalid))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-2:]))
    sys.exit(1 if survivors or invalid or bcode or any(c for _, c, _ in restored) else 0)


if __name__ == '__main__':
    main()
