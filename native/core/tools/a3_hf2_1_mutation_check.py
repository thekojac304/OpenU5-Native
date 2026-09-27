#!/usr/bin/env python
"""Alpha 3 A3-HF2.1 -- mutation validation of the two cleanup guards.

N*: PRESENTATION_DISPATCH on change only (alpha_runtime.cpp), guarded by the
    host target a3_hf2_1_dispatch_log.
T*: the TypeScript skin's clock strike armed only when the hour changes
    (game/src/skin/coreview.ts), guarded by the A3-HF2.1 rows of
    game/tests/sfx-bus.test.ts (vitest).

Each mutation changes ONE production anchor, rebuilds (native) or re-runs
vitest (TS), and records which checks turn RED; the file is then restored
byte for byte and touched (a restored file is older than the mutated object:
without the touch ninja keeps the mutant). A mutation that leaves every guard
GREEN is a SURVIVOR and fails this run; one that does not build is INVALID.

Usage: python native/core/tools/a3_hf2_1_mutation_check.py <build-dir> <log> [N1,T4,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import re
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
GAME = os.path.join(ROOT, 'game')
NPX = 'npx.cmd' if os.name == 'nt' else 'npx'

RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
CV = 'game/src/skin/coreview.ts'

MUTATIONS = [
    # -- PRESENTATION_DISPATCH ---------------------------------------------------------
    ('N1 logged on every frame again (the A3-04E.1 flood)', RT,
     '    if(dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0){',
     '    if(true||dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0){'),
    ('N2 never logged', RT,
     '    if(dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0){',
     '    if(false&&(dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0)){'),
    ('N3 a UI-mode change is not compared', RT,
     '    if(dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0){',
     '    if(std::strcmp(presentation_source,dispatch_logged_source_)!=0){'),
    ('N4 a source change is not compared', RT,
     '    if(dispatch_mode!=dispatch_logged_mode_||std::strcmp(presentation_source,dispatch_logged_source_)!=0){',
     '    if(dispatch_mode!=dispatch_logged_mode_){'),
    ('N5 the logged mode is never remembered', RT,
     '        dispatch_logged_mode_=dispatch_mode;\n',
     ''),
    ('N6 the logged source is never remembered', RT,
     '        dispatch_logged_source_=presentation_source;\n',
     ''),
    # -- the TypeScript skin's clock strike ------------------------------------------------
    ('T1 re-armed on every turn again (the refuted section 5.1 model)', CV,
     '    for (const l of this.listeners) l.onTurn?.(events);',
     '    this.clockChimeCounter = chimeHour12(this.game.state.time.hour);\n    for (const l of this.listeners) l.onTurn?.(events);'),
    ('T2 never re-armed', CV,
     '    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);',
     '    if (false) this.clockChimeCounter = chimeHour12(t.hour);'),
    ('T3 re-armed on every ambient tick', CV,
     '    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);',
     '    if (true) this.clockChimeCounter = chimeHour12(t.hour);'),
    ('T4 the key carries the minute (native A3-03\'s defect)', CV,
     '    const key = ((t.year * 13 + t.month) * 32 + t.day) * 24 + t.hour;',
     '    const key = (((t.year * 13 + t.month) * 32 + t.day) * 24 + t.hour) * 60 + t.minute;'),
    ('T5 the key drops the day', CV,
     '    const key = ((t.year * 13 + t.month) * 32 + t.day) * 24 + t.hour;',
     '    const key = t.hour;'),
    ('T6 the first observation arms instead of recording', CV,
     '    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);',
     '    if (key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);'),
    ('T7 a load keeps the pending strikes', CV,
     '  resetAmbientClock(): void {\n    this.clockChimeCounter = 0;\n',
     '  resetAmbientClock(): void {\n'),
    ('T8 a load keeps the recorded hour', CV,
     '    this.clockHourKey = -1;\n  }',
     '  }'),
    ('T9 the 24-hour hour is struck (0x5164-0x5183 dial dropped)', CV,
     '    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);',
     '    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = t.hour;'),
    ('T10 the ambient tick never observes the clock', CV,
     '    this.observeClock(); // antes de los gates, como `service_ambient` nativo\n',
     ''),
]


def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def native_build():
    return run([CMAKE, '--build', BUILD, '--target', 'a3_hf2_1_dispatch_log'], ROOT)


def native_test():
    code, out = run([os.path.join(BUILD, 'a3_hf2_1_dispatch_log.exe'), RES], ROOT)
    return code, [l for l in out.splitlines() if l.startswith('RED')]


def ts_test():
    env = dict(os.environ, NO_COLOR='1', FORCE_COLOR='0')
    p = subprocess.run([NPX, 'vitest', 'run', 'tests/sfx-bus.test.ts'], cwd=GAME, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env)
    out = p.stdout + p.stderr
    reds = set()
    for line in out.splitlines():
        if not line.lstrip().startswith('FAIL') or '>' not in line:
            continue
        row = re.search(r'TS-K\d+', line)
        # A failing row outside the A3-HF2.1 block is named by its describe (ASCII only).
        reds.add(row.group(0) if row else re.sub(r'[^\x20-\x7e]', '-', line.split('>')[1]).strip())
    reds = sorted(reds, key=lambda r: (not r.startswith('TS-K'), int(r[4:]) if r.startswith('TS-K') else 0, r))
    tally = re.search(r'Tests\s+(.+)', out)
    return p.returncode, reds, tally.group(1).strip() if tally else '?'


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-HF2.1 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
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
            if rel == RT:
                code, out = native_build()
                if code != 0:
                    lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                    lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                    survivors += 1
                    continue
                code, reds = native_test()
                detail = ['    a3_hf2_1_dispatch_log exit=%d  %d RED' % (code, len(reds))] + ['        ' + r for r in reds]
            else:
                code, reds, tally = ts_test()
                detail = ['    vitest sfx-bus exit=%d  (%s)' % (code, tally)] + ['        RED ' + r for r in reds]
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = code != 0
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        lines += detail
    bcode, _ = native_build()
    ncode, _ = native_test()
    tcode, _, tally = ts_test()
    lines.append('')
    lines.append('restored: native build exit=%d, a3_hf2_1_dispatch_log exit=%d; vitest sfx-bus exit=%d (%s)' %
                 (bcode, ncode, tcode, tally))
    lines.append('mutations: %d, killed: %d, survived: %d' % (len(selected), len(selected) - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    sys.exit(1 if survivors or bcode or ncode or tcode else 0)


if __name__ == '__main__':
    main()
