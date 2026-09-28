#!/usr/bin/env python
"""Alpha 3 A3-HF9 (H-185) -- mutation validation of the Refuge cadence and its karma getkey.

Each mutation changes production anchors -- the script check_refuge emits
(native/core/src/quest_world.cpp), the pacer's holds and getkey
(native/core/include/openu5/narrative_scene.h, src/narrative_scene.cpp), or the
device's input rule, cue, shake, music, menu and load paths
(native/targets/tdeck/main/alpha_runtime.cpp) -- rebuilds the targets below and
records which checks turn RED. Every file is restored byte for byte and touched
(a restored file is older than the mutated object: without the touch ninja
keeps the mutant). A mutation that leaves every target GREEN is a SURVIVOR and
fails this run; one that does not build is INVALID (not killed).

Targets: a3_hf9_refuge_cadence (the core script and pacer), a3_hf9_refuge_cadence_runtime
(the real runtime and Board), batch7b (the Refuge's E section) and quest_parity
(the TypeScript reference pins the script; run through ctest).

Usage: python native/core/tools/a3_hf9_mutation_check.py <build-dir> <log> [M1,M2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
CMAKE = BIN + '/cmake.exe'
CTEST = BIN + '/ctest.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
TARGETS = [('a3_hf9_refuge_cadence', []), ('a3_hf9_refuge_cadence_runtime', [RES, AUDIO]), ('batch7b_tests', [])]
BUILD_TARGETS = [t for t, _ in TARGETS] + ['quest_driver']

QW = 'native/core/src/quest_world.cpp'
NS = 'native/core/src/narrative_scene.cpp'
NH = 'native/core/include/openu5/narrative_scene.h'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

SPEECH = '        {nullptr,speech,nullptr,-1,true},'
WORDS = '        {nullptr,"Strange words are intoned.",nullptr,4,false,revival},'
PREROLL = '        {nullptr,nullptr,nullptr,10},'
DARK = '        {"void","An unending darkness engulfs thee...",nullptr,-1,false,0,true},'
TOP = '        if (awaiting_key_) return;'
KEYBEAT = '''            if (step.key_wait) {
                // The beat is on screen; nothing after it may be, until a key.
                awaiting_key_ = true;
                return;
            }'''
ADVANCE = '''    if (!awaiting_key_) return false;
    awaiting_key_ = false;
    return true;'''
MEMBERS = '    bool waiting_ = false, paced_ = true, awaiting_key_ = false;'
DWELL = '''            if (step.dwell_ms) {
                resume_at_ms_ = now_ms + step.dwell_ms;
                waiting_ = true;
                return;
            }
            continue;'''
RULE = '''        const bool ended=narrative_pacer_.advance_key();
        if(ended){dirty_=true;dirty_reason_="narrative-scene";}'''
RULE_RETURN = '''                 action_name(action.kind),ended?"key-wait-ended":"swallowed",int(narrative_pacer_.scene()));
        return true;'''

MUTATIONS = [
    # --- the getkey itself ---
    ('M1 ignore the wait (the speech is a timed beat again)', [
        (QW, SPEECH, '        {nullptr,speech,nullptr,-1,false},')]),
    ('M2 release immediately (the getkey ends at the next frame)', [
        (NS, TOP, '        if (awaiting_key_) awaiting_key_ = false;')]),
    ('M3 add a timeout (30 s)', [
        (NS, TOP, '        if (awaiting_key_) {\n            if (int32_t(now_ms - resume_at_ms_) < 30000) return;\n'
                  '            awaiting_key_ = false;\n        }')]),
    ('M4 the ending key leaks into the game (routed as a command)', [
        (RT, RULE_RETURN, RULE_RETURN.replace('        return true;', '        if(!ended)return true;'))]),
    ('M5 one key ends the getkey AND the next beat\'s hold', [
        (NH, MEMBERS, '    bool waiting_ = false, paced_ = true, awaiting_key_ = false, skip_hold_ = false;'),
        (NS, ADVANCE, '    if (!awaiting_key_) return false;\n    awaiting_key_ = false;\n    skip_hold_ = true;\n    return true;'),
        (NS, DWELL, DWELL.replace('            if (step.dwell_ms) {', '            if (step.dwell_ms && !skip_hold_) {')
         .replace('            continue;', '            skip_hold_ = false;\n            continue;'))]),
    ('M6 buffered keys (DOS typeahead): a key in a busy loop pre-satisfies the getkey', [
        (NH, MEMBERS, '    bool waiting_ = false, paced_ = true, awaiting_key_ = false, typeahead_ = false;'),
        (NS, ADVANCE, '    if (!awaiting_key_) {\n        typeahead_ = true;\n        return false;\n    }\n'
                      '    awaiting_key_ = false;\n    return true;'),
        (NS, KEYBEAT, KEYBEAT.replace('                awaiting_key_ = true;\n                return;',
                                      '                awaiting_key_ = !typeahead_;\n                typeahead_ = false;\n'
                                      '                if (awaiting_key_) return;'))]),
    # --- the cadence ---
    ('M7 wrong cadence (the Class-C 70 ms/unit + 900/260 ms floors)', [
        (NS, '            step->dwell_ms = paced_ ? refuge_beat_hold_ms(beat) : 0;',
         '            step->dwell_ms = paced_ ? (beat.delay > 0 ? uint32_t(beat.delay) * 70u : 0u) + '
         '(beat.message ? 900u : 260u) : 0;')]),
    ('M8 wrong tick (70 ms per delay unit)', [
        (NS, '    uint32_t ms = beat.delay > 0 ? delay_ticks_ms(uint32_t(beat.delay)) : 0;',
         '    uint32_t ms = beat.delay > 0 ? uint32_t(beat.delay) * 70u : 0;')]),
    ('M9 the slumber / revival sweeps unheld', [
        (NS, '    ms += tone_sweep_ms(beat.sweep_samples);\n', '')]),
    ('M10 the fizzle / dissolve floor dropped', [
        (NS, '    if (beat.fizzle) ms += kFizzleFloorMs;\n', '')]),
    ('M11 the shakes unheld', [
        (NS, '    if (beat.shake) ms += kRefugeShakeMs;\n', '')]),
    ('M12 the shakes not drawn', [
        (RT, '    if(beat.shake)self.begin_quake();\n', '')]),
    ('M13 bypass the 0x0946 site (delay(10) after the darkness line, as before)', [
        (QW, PREROLL, ''),
        (QW, DARK, '        {"void","An unending darkness engulfs thee...",nullptr,10,false,0,true},'),
        (QW, '        {"vertigo",nullptr,nullptr,-1,false,0,true}',
         '        {"vertigo",nullptr,nullptr,-1,false,0,true},{}')]),
    # --- karma / state ordering ---
    ('M14 karma floored BEFORE the wait (at check_refuge)', [
        (QW, '    s->refuge_pending=true;GameEvent e;e.kind=GameEventKind::Refuge;',
         '    if(c.game.karma<75){c.game.karma=75;}\n    s->refuge_pending=true;GameEvent e;e.kind=GameEventKind::Refuge;')]),
    ('M15 the resurrection applied AT the key (not at the scene\'s end)', [
        (RT, RULE, RULE.replace('if(ended){dirty_=true;dirty_reason_="narrative-scene";}',
                                'if(ended){dirty_=true;dirty_reason_="narrative-scene";'
                                'openu5::resolve_refuge(context_,ui_->event_sink());}'))]),
    # --- deferral ---
    ('M16 post-wait text not deferred (the getkey after "Strange words")', [
        (QW, SPEECH, '        {nullptr,speech,nullptr,-1,false},'),
        (QW, WORDS, '        {nullptr,"Strange words are intoned.",nullptr,4,true,revival},')]),
    # --- lifecycle ---
    ('M17 a successful load keeps the scene', [
        (RT, '    narrative_pacer_.cancel();world_fx_.clear();', '    world_fx_.clear();')]),
    ('M18 a successful load keeps the Refuge latch', [
        (RT, '    quest_.refuge_pending=false;\n', '')]),
    ('M19 released behind the System Menu (a menu key ends the getkey)', [
        (RT, '    if(system_menu_.active()){\n        const bool accepted=system_menu_.handle(action);',
         '    if(system_menu_.active()){\n        narrative_pacer_.advance_key();\n'
         '        const bool accepted=system_menu_.handle(action);')]),
    ('M20 cancel (scene exit) leaves the getkey armed', [
        (NS, '    waiting_ = false;\n    awaiting_key_ = false;\n    resume_at_ms_ = 0;', '    waiting_ = false;\n    resume_at_ms_ = 0;')]),
    # --- the device's other halves ---
    ('M21 no `Enter: continue` cue at the getkey', [
        (RT, '    if(narrative_pacer_.awaiting_key()){std::snprintf(text,sizeof(text),"Enter: continue");return text;}\n', '')]),
    ('M22 the music plays on through the first delay(10)', [
        (RT, '    if(narrative_pacer_.active()&&narrative_pacer_.scene()==openu5::NarrativeScene::Refuge){',
         '    if(narrative_pacer_.mounted()&&narrative_pacer_.scene()==openu5::NarrativeScene::Refuge){')]),
    ('M23 the unpaced harness waits for a key', [
        (NS, '            step->key_wait = paced_ && beat.key_wait;', '            step->key_wait = beat.key_wait;')]),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + BUILD_TARGETS)


def test():
    results = []
    for t, args in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe')] + args)
        reds = [l for l in out.splitlines() if l.startswith(('RED', 'FAIL', '[FAIL', 'batch7b check'))]
        results.append((t, code, reds))
    code, out = run([CTEST, '--test-dir', BUILD, '-R', '^quest_parity$', '--output-on-failure'])
    results.append(('quest_parity', code, [l.strip() for l in out.splitlines() if 'mismatch' in l][:2]))
    return results


def main():
    os.environ['PATH'] = BIN + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF9 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
                lines += ['        ' + r[:200] for r in reds[:8]]
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
