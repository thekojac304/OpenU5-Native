#!/usr/bin/env python
"""Alpha 3 A3-05 -- RED-first: the audio-control checks against the UNMODIFIED sources.

Swaps in <git-rev>'s copies of every production file A3-05 changes (the
AudioService mute, the Settings rows, the input adapter, the runtime, the
combat-kill cue and its 0x2fe3 provenance), builds

  a3_05_audio_controls  -- which observes only seams the baseline already has
                           (raw keys, a recording backend, the Settings view,
                           the transcript), and
  a3_02_sfx_synth / a3_02_sfx_runtime -- whose kill-cue expectations A3-05
                           corrected (a death is silent),

runs them, and restores the working files byte for byte and touches them (a
restored file is older than its object: without the touch ninja would keep
the baseline build).

Usage: python native/core/tools/a3_05_red_first.py <build-dir> <log> [<git-rev>]
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CORE = os.path.join(ROOT, 'native/core')
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
REV = sys.argv[3] if len(sys.argv) > 3 else 'HEAD'
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
STOCK = os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin')
FILES = [
    'native/core/include/openu5/audio.h', 'native/core/src/audio.cpp',
    'native/core/include/openu5/system_menu.h', 'native/core/src/system_menu.cpp',
    'native/core/include/openu5/frontend.h', 'native/core/src/frontend.cpp',
    'native/core/include/openu5/sfx_synth.h', 'native/core/src/sfx_synth.cpp',
    'native/core/src/sfx_inventory.cpp',
    'native/targets/tdeck/main/ui_input_adapter.h', 'native/targets/tdeck/main/ui_input_adapter.cpp',
    'native/targets/tdeck/main/alpha_runtime.h', 'native/targets/tdeck/main/alpha_runtime.cpp',
]
RUNS = [
    ('a3_05_audio_controls', [RES, AUDIO, STOCK]),
    ('a3_02_sfx_synth', [os.path.join(CORE, 'fixtures/a3-02-sfx-reference.txt'), CORE]),
    ('a3_02_sfx_runtime', [RES, AUDIO]),
]


def build(target):
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', target], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    saved = {f: open(os.path.join(ROOT, f), 'rb').read() for f in FILES}
    out = [f'# A3-05 RED-first: built against {REV}\'s production files (git show {REV}:<file>),',
           '# every test file as in the working tree:']
    out += ['#   ' + f for f in FILES]
    try:
        for f in FILES:
            base = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{f}'], capture_output=True, check=True).stdout
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(base.replace(b'\r\n', b'\n'))
        for target, args in RUNS:
            code, text = build(target)
            if code:
                print(text)
                return 2
            run = subprocess.run([os.path.join(BUILD, target + '.exe')] + args, capture_output=True, text=True,
                                 errors='replace')
            out.append(f'## {target}: exit {run.returncode}')
            out += [l for l in run.stdout.splitlines()
                    if l.startswith(('GREEN ', 'RED ', 'FAIL', 'PASS ')) or 'checks,' in l or 'failures' in l]
    finally:
        for f, data in saved.items():
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(data)
            now = time.time()
            os.utime(os.path.join(ROOT, f), (now, now))
        for target, _ in RUNS:
            build(target)
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
