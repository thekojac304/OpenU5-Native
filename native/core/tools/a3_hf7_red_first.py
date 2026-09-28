#!/usr/bin/env python
"""Alpha 3 A3-HF7 (H-184) -- RED-first: the ritual-effect checks against the UNMODIFIED logic.

Swaps in <git-rev>'s alpha_runtime.cpp, dialogue_pacer.cpp and shrine.cpp, builds
a3_hf7_ritual_fx_runtime and a3_hf7_ritual_fx, runs both, and restores the
working files byte for byte and touches them (a restored file is older than its
object: without the touch ninja would keep the baseline build).

No edit is applied to the baseline files. The headers (the new ritual_fx.h, the
DialoguePacer's Effect declarations, device_ui_views.h's defaulted mask), the
new ritual_fx.cpp, the fixture and the tests stay as in the working tree: the
baseline .cpp files compile against them unchanged, and simply never consult
the effect model -- which is the defect.

Usage: python native/core/tools/a3_hf7_red_first.py <build-dir> <log> [<git-rev>]
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
REV = sys.argv[3] if len(sys.argv) > 3 else 'HEAD'
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
FILES = ['native/targets/tdeck/main/alpha_runtime.cpp', 'native/core/src/dialogue_pacer.cpp',
         'native/core/src/shrine.cpp']
TARGETS = [('a3_hf7_ritual_fx_runtime', [RES]), ('a3_hf7_ritual_fx', [])]


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target'] + [t for t, _ in TARGETS], env=env,
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def run(out, label):
    for target, args in TARGETS:
        r = subprocess.run([os.path.join(BUILD, target + '.exe')] + args, capture_output=True, text=True,
                           errors='replace')
        out.append(f'## {target} against {label}: exit {r.returncode}')
        out += [l for l in r.stdout.splitlines()
                if l.startswith(('GREEN ', 'RED ', '    screen masks')) or 'checks,' in l]


def baseline(path):
    return subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{path}'], capture_output=True, check=True).stdout


def main():
    saved = {p: open(os.path.join(ROOT, p), 'rb').read() for p in FILES}
    out = [f'# A3-HF7 RED-first: built against {REV}:' + f', {REV}:'.join(FILES) + ';',
           '# every header, ritual_fx.cpp, the fixture and the tests as in the working tree.']
    try:
        for p in FILES:
            with open(os.path.join(ROOT, p), 'wb') as h:
                h.write(baseline(p))
        code, text = build()
        if code:
            print(text)
            return 2
        run(out, REV)
    finally:
        now = time.time()
        for p, text in saved.items():
            with open(os.path.join(ROOT, p), 'wb') as h:
                h.write(text)
            os.utime(os.path.join(ROOT, p), (now, now))
    code, text = build()
    if code:
        print(text)
        return 2
    run(out, 'the working tree')
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
