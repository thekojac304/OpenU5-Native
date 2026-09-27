#!/usr/bin/env python
"""Alpha 3 A3-HF2 -- mutation validation of the ambient clock / fountain guards.

Each mutation changes ONE production line, rebuilds, runs a3_hf2_ambient_parity
and the A3-03 ambient tests, and records which checks turn RED; the file is
then restored byte for byte and touched (a restored file is older than the
mutated object: without the touch ninja keeps the mutant). A mutation that
leaves every test GREEN is a SURVIVOR and fails this run.

Usage: python native/core/tools/a3_hf2_mutation_check.py <build-dir> <log> [H1,H4,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO_PACK = os.path.join(ROOT, 'native/assets/openu5-audio.bin')

TESTS = {
    'a3-hf2 parity': [os.path.join(BUILD, 'a3_hf2_ambient_parity.exe'), RES, AUDIO_PACK],
    'a3-03 runtime': [os.path.join(BUILD, 'a3_03_sfx_runtime.exe'), RES, AUDIO_PACK],
    'a3-03 inventory': [os.path.join(BUILD, 'a3_03_sfx_inventory.exe'), CORE],
}
TARGETS = ['a3_hf2_ambient_parity', 'a3_03_sfx_runtime', 'a3_03_sfx_inventory']

AMB = 'native/core/src/ambient_sfx.cpp'
AMBH = 'native/core/include/openu5/ambient_sfx.h'
SYN = 'native/core/src/sfx_synth.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

MUTATIONS = [
    # -- the clock's strike: which advance re-arms [0x5884] ---------------------------
    ('H1 A3-03\'s minute key: every step re-arms the strike (the hardware defect)', RT,
     '    const int64_t key=(((int64_t(t.year)*13+t.month)*32+t.day)*24)+t.hour;',
     '    const int64_t key=((((int64_t(t.year)*13+t.month)*32+t.day)*24+t.hour)*60)+t.minute;'),
    ('H2 the strike is never re-armed', RT,
     '    else if(key!=ambient_clock_key_){ambient_clock_key_=key;ambient_.rearm(uint8_t(t.hour));}',
     '    else if(key!=ambient_clock_key_){ambient_clock_key_=key;}'),
    ('H3 re-armed on every ambient tick, moved or not', RT,
     '    else if(key!=ambient_clock_key_){ambient_clock_key_=key;ambient_.rearm(uint8_t(t.hour));}',
     '    else{ambient_clock_key_=key;ambient_.rearm(uint8_t(t.hour));}'),
    ('H4 re-armed per day, not per hour (the key drops the hour)', RT,
     '    const int64_t key=(((int64_t(t.year)*13+t.month)*32+t.day)*24)+t.hour;',
     '    const int64_t key=(((int64_t(t.year)*13+t.month)*32+t.day)*24);'),
    ('H5 the strike counts the 24-hour hour (0x5164-0x5183 dial dropped)', AMBH,
     '    void rearm(uint8_t hour) { chimes_ = ambient_chime_hour(hour); }',
     '    void rearm(uint8_t hour) { chimes_ = hour; }'),
    ('H6 the strike ignores the phase gate (0x4269)', AMB,
     '        if (chimes_ && (phase_ == 0 || phase_ == 4)) cue = SfxId::AmbientClockChime; // 0x4277',
     '        if (chimes_) cue = SfxId::AmbientClockChime;'),
    # -- the tick / tock stays --------------------------------------------------------
    ('H7 the tock is lost', AMB,
     '        else if (phase_ == 4) cue = SfxId::AmbientClockTock;                         // 0x42ad',
     '        else if (false) cue = SfxId::AmbientClockTock;'),
    # -- the fountain, standing still ---------------------------------------------------
    ('H8 the ambience dies out about a second after arriving (the unconfirmed fountain report)', RT,
     '    ++ambient_ticks_;\n    const auto cue=ambient_.tick(openu5::ambient_nearest_class(w));',
     '    if(ambient_ticks_>18)return;\n    ++ambient_ticks_;\n    const auto cue=ambient_.tick(openu5::ambient_nearest_class(w));'),
    ('H9 the ambience ticks every other 55 ms tick', RT,
     '    const uint32_t tick=uint32_t(now_us/(int64_t(openu5::kSceneTickMs)*1000));',
     '    const uint32_t tick=uint32_t(now_us/(int64_t(openu5::kSceneTickMs)*2000));'),
    ('H10 an ambient cue is always skipped by the player (submitted, never heard)', SYN,
     '        if (voice_.active() || pending_count_) {\n            ++stats_.ambient_skipped;',
     '        if (true) {\n            ++stats_.ambient_skipped;'),
]


def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS, ROOT)


def run_tests():
    results = {}
    for name, cmd in TESTS.items():
        code, out = run(cmd, ROOT)
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results[name] = (code, reds)
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF2 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = 0
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
            survivors += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                survivors += 1
                continue
            results = run_tests()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for code, _ in results.values())
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for name, (code, reds) in results.items():
            if code != 0:
                lines.append('    %-15s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r for r in reds[:6]]
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join('%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed: %d, survived: %d' % (len(selected), len(selected) - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if survivors or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
