#!/usr/bin/env python
"""Alpha 3 A3-HF6 (H-183) -- mutation validation of the shrine / Codex key waits.

Each mutation changes ONE production anchor -- the DialoguePacer marker rule
(native/core/src/dialogue_pacer.cpp), its device wiring (alpha_runtime.cpp) or
one of the core's getkey sites (shrine.cpp) -- rebuilds the host targets below
and records which checks turn RED. The file is restored byte for byte and
touched (a restored file is older than the mutated object: without the touch
ninja keeps the mutant). A mutation that leaves every target GREEN is a
SURVIVOR and fails this run; one that does not build is INVALID (not killed).

Targets: a3_hf6_shrine_key_wait_runtime (the real runtime), a3_hf6_shrine_key_wait_pacer
(the queue alone), and A3-HF5's a3_hf5_dialogue_pacing_runtime / a3_hf5_dialogue_pacer
(the TLK pauses the same queue serves).

Usage: python native/core/tools/a3_hf6_mutation_check.py <build-dir> <log> [S1,S2,...]
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
TARGETS = ['a3_hf6_shrine_key_wait_runtime', 'a3_hf6_shrine_key_wait_pacer',
           'a3_hf5_dialogue_pacing_runtime', 'a3_hf5_dialogue_pacer']

PACER = 'native/core/src/dialogue_pacer.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
SHRINE = 'native/core/src/shrine.cpp'
MARKER = '    return e.kind == GameEventKind::ShrineKeyWait ? DialoguePause::Key : dialogue_event_pause(e);'
GATE = ('    const bool scene_getkey=e.kind==openu5::GameEventKind::ShrineKeyWait&&blackthorn_pacer_.active()'
        '&&!dialogue_pacer_.holding();')
HOLD = '    if(dialogue_pacer_.holding()&&shortcut==DeviceShortcut::None){'
HOLD_TAIL = '        dirty_=true;dirty_reason_="dialogue-pause";\n        return true;\n    }\n    // R-12'
OFFERED = '            dirty_=true;dirty_reason_="dialogue-pause";\n            return;\n        }'
ADVANCE = '    if (state_ == DialoguePacerState::Idle) return false;\n    release(now_ms, out);\n    return true;'
RELEASE = ('        if ((step.kind == DialoguePacerStepKind::Line || step.kind == DialoguePacerStepKind::Event) &&\n'
           '            step.pause != DialoguePause::None) {')
LOAD_MSG = 'ui_->append(openu5::UiTextChannel::System,ok?"Load complete":"No valid save");}'
RECORD = ('shrine_services_.record=[](void *p,int32_t index)->const char*{auto&r=*static_cast<AlphaRuntime*>(p);'
          'return tdeck::misc_text_record(')

MUTATIONS = [
    # --- the defect itself ---
    ('S1 the marker is no pause (HEAD behaviour)', PACER, MARKER,
     MARKER.replace('DialoguePause::Key', 'DialoguePause::None')),
    ('S2 the wait is released as soon as it is taken', RT, OFFERED,
     OFFERED.replace('            return;',
                     '            if(e.kind==openu5::GameEventKind::ShrineKeyWait)'
                     'dialogue_pacer_.advance_key(0,{this,release_dialogue_event});\n            return;')),
    ('S3 the getkey times out like a TLK Pause (Timed)', PACER, MARKER,
     MARKER.replace('DialoguePause::Key', 'DialoguePause::Timed')),
    ('S4 every held pause times out on the clock', PACER,
     '    if (state_ == DialoguePacerState::Timed && int32_t(now_ms - resume_at_ms_) >= 0) release(now_ms, out);',
     '    if (state_ != DialoguePacerState::Idle && int32_t(now_ms - resume_at_ms_) >= 0) release(now_ms, out);'),
    # --- input ownership ---
    ('S5 the key that ends a getkey also reaches the game', RT, HOLD_TAIL,
     HOLD_TAIL.replace('        return true;\n    }\n    // R-12',
                       '        if(state!=openu5::DialoguePacerState::Key)return true;\n    }\n    // R-12')),
    ('S6 a trackball step bypasses the hold (movement leaks)', RT, HOLD,
     HOLD.replace('shortcut==DeviceShortcut::None', 'shortcut==DeviceShortcut::None&&'
                  'action.kind!=openu5::UiActionKind::Direction')),
    ('S7 Mic bypasses the hold', RT, HOLD,
     HOLD.replace('shortcut==DeviceShortcut::None', 'shortcut==DeviceShortcut::None&&'
                  'action.kind!=openu5::UiActionKind::Cancel')),
    ('S8 the cue is not shown at a getkey', RT,
     '    if(dialogue_pacer_.awaiting_key()){std::snprintf(text,sizeof(text),"Enter: continue");return text;}',
     '    if(false&&dialogue_pacer_.awaiting_key()){std::snprintf(text,sizeof(text),"Enter: continue");return text;}'),
    # --- one key, one wait ---
    ('S9 a queued marker does not stop the release (one key skips waits)', PACER, RELEASE,
     RELEASE.replace(' || step.kind == DialoguePacerStepKind::Event', '')),
    ('S10 a queued marker loses its pause', PACER, '        step.pause = paced_event_pause(e);',
     '        step.pause = dialogue_event_pause(e);'),
    ('S11 one key ends two getkeys', PACER, ADVANCE,
     ADVANCE.replace('    release(now_ms, out);\n',
                     '    release(now_ms, out);\n    if (state_ == DialoguePacerState::Key) release(now_ms, out);\n')),
    ('S12 the last getkey (nothing queued) never ends', PACER, ADVANCE,
     ADVANCE.replace('    release(now_ms, out);\n',
                     '    if (state_ == DialoguePacerState::Key && !count_) return true;\n    release(now_ms, out);\n')),
    # --- order ---
    ('S13 the ceremony quakes jump the queue', PACER, '    if (push(e)) return true;',
     '    if (e.kind == GameEventKind::Quake) return false;\n    if (push(e)) return true;'),
    # --- lifecycle ---
    ('S14 a successful load keeps the wait', RT, '    dialogue_pacer_.cancel();\n', '    (void)0;\n'),
    ('S15 a failed load drops the wait', RT, LOAD_MSG, 'if(!ok)dialogue_pacer_.cancel();' + LOAD_MSG),
    ('S16 opening the System Menu releases the getkey behind it', RT,
     'system_menu_.open(settings_,slots);}',
     'system_menu_.open(settings_,slots);dialogue_pacer_.advance_key(0,{this,release_dialogue_event});}'),
    # --- scope ---
    ('S17 the Blackthorn scene loses its getkeys to this pacer', RT, GATE,
     GATE.replace('&&!dialogue_pacer_.holding()', '&&dialogue_pacer_.holding()')),
    ('S18 the unpaced harness pauses at markers', PACER, '    if (!pause_ms_) return false;',
     '    if (!pause_ms_ && e.kind != GameEventKind::ShrineKeyWait) return false;'),
    ('S19 the device shrine binder loses the Codex pages', RT, RECORD,
     RECORD.replace('return tdeck::misc_text_record(', 'if(index>=20)return nullptr;return tdeck::misc_text_record(')),
    # --- the core's own getkey sites (one site bypassed) ---
    ('S20 ordained: the second getkey (0x0abc) is skipped', SHRINE,
     'd.message(text.c_str()); d.wait(); d.message("\\n\\"Return again',
     'd.message(text.c_str()); d.message("\\n\\"Return again'),
    ('S21 ceremony: the page getkeys (0x0e16..0x0e5b) are skipped', SHRINE,
     'for (int32_t i=41;i<45;++i) { d.message(record(i)); d.wait(); }',
     'for (int32_t i=41;i<45;++i) { d.message(record(i)); }'),
    ('S22 Codex: the first getkey (0x0d2b) is skipped', SHRINE,
     '    d.wait(); d.message("\\nThe book is open',
     '    d.message("\\nThe book is open'),
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
    lines = ['# A3-HF6 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
            if code or reds:
                lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
                lines += ['        ' + r[:200] for r in reds[:12]]
        print(lines[-1] if len(lines) else '', flush=True)
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
