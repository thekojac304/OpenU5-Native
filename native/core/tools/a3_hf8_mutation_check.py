#!/usr/bin/env python
"""Alpha 3 A3-HF8 (H-186) -- mutation validation of the Blackthorn sacrifice burst.

Each mutation changes production anchors -- the burst beat in the sacrifice
script (native/core/src/blackthorn_scene.cpp build_sacrifice_script), the
pacer's apply / hold / view / compose (same file), a sacrifice path in
blackthorn.cpp, or the device's cue, input gate and load reset
(alpha_runtime.cpp) -- rebuilds the host targets below and records which checks
turn RED. Every file is restored byte for byte and touched (a restored file is
older than the mutated object: without the touch ninja keeps the mutant). A
mutation that leaves every target GREEN is a SURVIVOR and fails this run; one
that does not build is INVALID (not killed).

Targets: a3_hf8_sacrifice_burst_runtime (the real capture, runtime and Board,
pixels), blackthorn_scene_tests (the core event stream and the pacer) and
batch51_scene_pacing (the capture's Batch 51 holds).

Usage: python native/core/tools/a3_hf8_mutation_check.py <build-dir> <log> [M1,M2,...]
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
DS = os.path.join(ROOT, 'game/assets/ds-strings.json')
TARGETS = [('a3_hf8_sacrifice_burst_runtime', [RES, AUDIO]), ('blackthorn_scene_tests', [DS, RES]),
           ('batch51_scene_pacing', [])]

SCENE = 'native/core/src/blackthorn_scene.cpp'
BT = 'native/core/src/blackthorn.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

BURST_BLOCK = '''    auto &burst = b.emit();
    int burst_x = 0, burst_y = 0;
    sacrifice_victim_cell(state, burst_x, burst_y);
    burst.burst_x = int8_t(burst_x);
    burst.burst_y = int8_t(burst_y);
    burst.sfx = BlackthornSfx::Explosion;
    burst.sweep_samples = kBlackthornBurstSamples;
    state.objects[1].visible = false;
    auto &after = b.emit();
    b.stamp(after);
    after.patch = {kTortureX, kTortureY, kBlackthornTortureAfterTile, true};'''
BURST_LINES = '''    auto &burst = b.emit();
    int burst_x = 0, burst_y = 0;
    sacrifice_victim_cell(state, burst_x, burst_y);
    burst.burst_x = int8_t(burst_x);
    burst.burst_y = int8_t(burst_y);
    burst.sfx = BlackthornSfx::Explosion;
    burst.sweep_samples = kBlackthornBurstSamples;
'''
SIREN_BLOCK = '''    b.pause(10);
    auto &siren = b.emit();'''
WAIT_END = '''            waiting_ = false;
            // 0x355d viewport_redraw: the burst's hold is over.
            burst_x_ = burst_y_ = -1;'''
APPLY = '''    burst_x_ = beat.burst_x;
    burst_y_ = beat.burst_y;'''
PENDULUM = '''    else{auto victim=sacrifice_first_companion(g);event(sink,GameEventKind::Message,record(c,4));
        if(scene){build_sacrifice_script(*scene->state,*scene->script);emit_scene(*scene,sink);}'''
BETRAYAL = '''            if(s.living>1){build_sacrifice_script(*scene->state,*scene->script);emit_scene(*scene,sink);}'''
STRIP = 'for(uint8_t i=0;i<scene->script->count;++i)scene->script->beats[i].burst_x=-1;'

MUTATIONS = [
    # --- the burst itself ---
    ('M1 omit the burst (no cell on the beat)', [
        (SCENE, '    burst.burst_x = int8_t(burst_x);', '    burst.burst_x = int8_t(-1 + 0 * burst_x);')]),
    ('M2 wrong region (the cell to the right of slot 1)', [
        (SCENE, '    burst.burst_x = int8_t(burst_x);', '    burst.burst_x = int8_t(burst_x + 1);')]),
    ('M3 wrong primitive (tile 1 instead of tile 0)', [
        (SCENE, '            s.tiles[view.burst_y * kBlackthornSceneCols + view.burst_x] = kBlackthornBurstTile;',
         '            s.tiles[view.burst_y * kBlackthornSceneCols + view.burst_x] = kBlackthornBurstTile + 1;')]),
    ('M4 wrong duration (the hold doubled)', [
        (SCENE, '    burst.sweep_samples = kBlackthornBurstSamples;',
         '    burst.sweep_samples = 2 * kBlackthornBurstSamples;')]),
    ('M5 wrong number of phases (a second burst after one tick)', [
        (SCENE, '    burst.sweep_samples = kBlackthornBurstSamples;\n',
         '    burst.sweep_samples = kBlackthornBurstSamples;\n'
         '    { const BlackthornBeat copy = burst; b.emit().frames = 1; b.emit() = copy; }\n')]),
    ('M6 restore too early (the tile leaves 90 ms before the hold ends)', [
        (SCENE, '            if (unit_ms_ && int32_t(now_ms - resume_at_ms_) < 0) return;',
         '            if (int32_t(now_ms + 90 - resume_at_ms_) >= 0) burst_x_ = burst_y_ = -1;\n'
         '            if (unit_ms_ && int32_t(now_ms - resume_at_ms_) < 0) return;')]),
    ('M7 never restore (a burst sticks until the scene ends)', [
        (SCENE, WAIT_END, '            waiting_ = false;'),
        (SCENE, APPLY, '    if (beat.burst_x >= 0) {\n        burst_x_ = beat.burst_x;\n        burst_y_ = beat.burst_y;\n    }')]),
    # --- the order ---
    ('M8 burst BEFORE the siren instead of after it', [
        (SCENE, SIREN_BLOCK, '    b.pause(10);\n' + BURST_LINES + '    auto &siren = b.emit();'),
        (SCENE, BURST_LINES + '    state.objects[1].visible = false;', '    state.objects[1].visible = false;')]),
    ('M9 burst AFTER the victim clears (the HF7 order)', [
        (SCENE, BURST_BLOCK, '''    state.objects[1].visible = false;
    auto &after = b.emit();
    b.stamp(after);
    after.patch = {kTortureX, kTortureY, kBlackthornTortureAfterTile, true};
    auto &burst = b.emit();
    int burst_x = 0, burst_y = 0;
    sacrifice_victim_cell(state, burst_x, burst_y);
    burst.burst_x = int8_t(burst_x);
    burst.burst_y = int8_t(burst_y);
    burst.sfx = BlackthornSfx::Explosion;
    burst.sweep_samples = kBlackthornBurstSamples;''')]),
    # --- the paths ---
    ('M10 the pendulum path bypasses the burst', [
        (BT, PENDULUM, PENDULUM.replace('build_sacrifice_script(*scene->state,*scene->script);',
                                        'build_sacrifice_script(*scene->state,*scene->script);' + STRIP))]),
    ('M11 the betrayal path bypasses the burst', [
        (BT, BETRAYAL, BETRAYAL.replace('build_sacrifice_script(*scene->state,*scene->script);',
                                        'build_sacrifice_script(*scene->state,*scene->script);' + STRIP))]),
    # --- the device ---
    ('M12 the burst is mute (no noise_burst cue)', [
        (RT, 'sfx==openu5::BlackthornSfx::Explosion?openu5::SfxId::CombatHit:openu5::SfxId::None;',
         'sfx==openu5::BlackthornSfx::Explosion?openu5::SfxId::None:openu5::SfxId::None;')]),
    ('M13 input leaks while the scene runs (only a getkey swallows)', [
        (RT, '    if(blackthorn_pacer_.modal()&&shortcut==DeviceShortcut::None){',
         '    if(blackthorn_pacer_.awaiting_key()&&shortcut==DeviceShortcut::None){')]),
    ('M14 a successful load keeps the scene (and its burst)', [
        (RT, '    blackthorn_pacer_.cancel();blackthorn_scene_state_={};',
         '    blackthorn_scene_state_={};')]),
    ('M15 the view hides the burst (the pacer holds it, nothing is drawn)', [
        (SCENE, '        v.burst_x = burst_x_;', '        v.burst_x = int8_t(-1 + 0 * burst_x_);')]),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + [t for t, _ in TARGETS])


def test():
    results = []
    for t, args in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe')] + args)
        reds = [l for l in out.splitlines() if l.startswith(('RED', 'FAIL', '[FAIL'))]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF8 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = invalid = 0
    only = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    selected = [m for m in MUTATIONS if not only or m[0].split()[0] in only]
    for label, edits in selected:
        files = sorted({rel for rel, _, _ in edits})
        originals = {rel: open(os.path.join(ROOT, rel), 'rb').read() for rel in files}
        texts = {rel: originals[rel].decode('utf-8') for rel in files}
        bad = None
        for rel, old, new in edits:
            eol = '\r\n' if '\r\n' in texts[rel] else '\n'
            o, n = old.replace('\n', eol), new.replace('\n', eol)
            count = texts[rel].count(o)
            if count != 1:
                bad = '%s: ANCHOR NOT UNIQUE (%d) in %s: %r' % (label, count, rel, old[:60])
                break
            texts[rel] = texts[rel].replace(o, n)
        where = '+'.join(os.path.basename(f) for f in files)
        if bad:
            lines.append(bad)
            print(bad, flush=True)
            invalid += 1
            continue
        try:
            for rel in files:
                open(os.path.join(ROOT, rel), 'wb').write(texts[rel].encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, where))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                print(lines[-1], flush=True)
                invalid += 1
                continue
            results = test()
        finally:
            for rel in files:
                path = os.path.join(ROOT, rel)
                open(path, 'wb').write(originals[rel])
                os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, where, 'KILLED' if killed else 'SURVIVED'))
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
