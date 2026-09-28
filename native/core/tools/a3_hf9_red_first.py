#!/usr/bin/env python
"""Alpha 3 A3-HF9 (H-185) -- RED-first: the Refuge checks against the UNMODIFIED logic.

Swaps in <git-rev>'s narrative_scene.cpp, quest_world.cpp and alpha_runtime.cpp,
builds a3_hf9_refuge_cadence and a3_hf9_refuge_cadence_runtime, runs both, and
restores the working files byte for byte and touches them (a restored file is
older than its object: without the touch ninja would keep the baseline build).

The headers, the fixture and the tests stay as in the working tree. The
baseline .cpp files compile against them with ONE appended shim, in the
baseline narrative_scene.cpp only, so the tests link:
  - the three Class-C constants the baseline pacer reads (70 / 900 / 260 ms,
    removed from the header by this batch), restated verbatim;
  - NarrativeScenePacer::advance_key() -> false (the baseline has no getkey:
    a key never ends anything) and refuge_beat_hold_ms() -> 0 (unused by the
    baseline pacer).
Nothing else is edited: the baseline script, pacer, input rule, cue and load
path are exactly <git-rev>'s -- which is the defect.

A second, independent control shows the TypeScript reference pins the script:
<git-rev>'s game/src/core/game.ts is swapped in and quest_parity (the working
tree's native driver) must FAIL; restored, it must pass.

Usage: python native/core/tools/a3_hf9_red_first.py <build-dir> <log> [<git-rev>]
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
REV = sys.argv[3] if len(sys.argv) > 3 else 'HEAD'
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
SCENE = 'native/core/src/narrative_scene.cpp'
FILES = [SCENE, 'native/core/src/quest_world.cpp', 'native/targets/tdeck/main/alpha_runtime.cpp']
TS = 'game/src/core/game.ts'
TARGETS = [('a3_hf9_refuge_cadence', []), ('a3_hf9_refuge_cadence_runtime', [RES, AUDIO])]

SHIM = b'''
// ---- A3-HF9 RED-first shim (tools/a3_hf9_red_first.py), not production ----
namespace openu5 {
bool NarrativeScenePacer::advance_key() { return false; }
uint32_t refuge_beat_hold_ms(const RefugeBeat &) { return 0; }
} // namespace openu5
'''
CONSTANTS = b'''namespace openu5 {
constexpr uint32_t kRefugeUnitMs = 70;       // the baseline's Class-C cadence,
constexpr uint32_t kRefugeTextFloorMs = 900; // restated from its header
constexpr uint32_t kRefugeSceneFloorMs = 260;
'''


def env():
    e = dict(os.environ)
    e['PATH'] = BIN + ';' + e.get('PATH', '')
    return e


def build():
    r = subprocess.run([CMAKE, '--build', BUILD, '--target'] + [t for t, _ in TARGETS] + ['quest_driver'],
                       env=env(), capture_output=True, text=True, errors='replace')
    return r.returncode, r.stdout + r.stderr


def run(out, label):
    for target, args in TARGETS:
        r = subprocess.run([os.path.join(BUILD, target + '.exe')] + args, capture_output=True, text=True,
                           errors='replace')
        out.append(f'## {target} against {label}: exit {r.returncode}')
        out += [l for l in (r.stdout + r.stderr).splitlines()
                if l.startswith(('GREEN ', 'RED ', '    ')) or 'checks,' in l]


def parity(out, label):
    r = subprocess.run([CTEST, '--test-dir', BUILD, '-R', '^quest_parity$', '--output-on-failure'], env=env(),
                       capture_output=True, text=True, errors='replace')
    lines = (r.stdout + r.stderr).splitlines()
    out.append(f'## quest_parity with {label}: exit {r.returncode}')
    out += ['    ' + l for l in lines if 'mismatch' in l.lower() or 'tests passed' in l or 'Passed' in l
            or 'Failed' in l][:6]
    return r.returncode


def baseline(path):
    text = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{path}'], capture_output=True, check=True).stdout
    if path == SCENE:
        eol = b'\r\n' if b'\r\n' in text else b'\n'
        anchor = b'namespace openu5 {' + eol
        assert text.count(anchor) == 1, 'baseline anchor moved'
        text = text.replace(anchor, CONSTANTS.replace(b'\n', eol), 1) + SHIM.replace(b'\n', eol)
    return text


def swap(paths, make):
    saved = {p: open(os.path.join(ROOT, p), 'rb').read() for p in paths}
    for p in paths:
        with open(os.path.join(ROOT, p), 'wb') as h:
            h.write(make(p))
    return saved


def restore(saved):
    now = time.time()
    for p, text in saved.items():
        with open(os.path.join(ROOT, p), 'wb') as h:
            h.write(text)
        os.utime(os.path.join(ROOT, p), (now, now))


def main():
    out = [f'# A3-HF9 RED-first: built against {REV}:' + f', {REV}:'.join(FILES) + ';',
           '# (narrative_scene.cpp + the link shim and the three restated constants, nothing else);',
           '# every header, the fixture and the tests as in the working tree.']
    saved = swap(FILES, baseline)
    try:
        code, text = build()
        if code:
            print(text[-4000:])
            return 2
        run(out, REV)
    finally:
        restore(saved)
    code, text = build()
    if code:
        print(text[-4000:])
        return 2
    run(out, 'the working tree')
    # The parity control: the reference pins the script the native core emits.
    out.append('')
    out.append(f'# Control: {REV}:{TS} (the old script) against the working tree native quest_driver')
    saved = swap([TS], lambda p: subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{p}'], capture_output=True,
                                                check=True).stdout)
    try:
        old = parity(out, f'{REV}:{TS}')
    finally:
        restore(saved)
    new = parity(out, 'the working tree game.ts')
    out.append(f'# control: old reference {"FAILS (pinned)" if old else "PASSES (NOT pinned!)"}; '
               f'new reference {"passes" if not new else "FAILS"}')
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(l for l in out if not l.startswith('GREEN ')))
    return 0 if old and not new else 1


if __name__ == '__main__':
    sys.exit(main())
