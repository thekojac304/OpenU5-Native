#!/usr/bin/env python
"""Alpha 3 A3-HF3 -- RED-first: the D-63 checks against the UNMODIFIED runtime.

Swaps in HEAD's alpha_runtime.h / alpha_runtime.cpp (the A3-04F closeout
baseline, before the hit cue was wired), builds a3_hf3_combat_hit_runtime --
the test observes only seams that already exist at HEAD: the composed
viewport, the panel's GRAM, the combat state and a recording audio backend --
runs it, and restores the working files byte for byte and touches them (a
restored file is older than its object: without the touch ninja would keep
the baseline build).

Usage: python native/core/tools/a3_hf3_red_first.py <build-dir> <log> [<git-rev>]
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
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
FILES = ['native/targets/tdeck/main/alpha_runtime.h', 'native/targets/tdeck/main/alpha_runtime.cpp']
TARGET = 'a3_hf3_combat_hit_runtime'


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    saved = {f: open(os.path.join(ROOT, f), 'rb').read() for f in FILES}
    out = []
    try:
        for f in FILES:
            base = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{f}'], capture_output=True, check=True).stdout
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(base.replace(b'\r\n', b'\n'))
        code, text = build()
        if code:
            print(text)
            return 2
        exe = os.path.join(BUILD, TARGET + '.exe')
        run = subprocess.run([exe, RES, AUDIO], capture_output=True, text=True, errors='replace')
        out = [l for l in run.stdout.splitlines() if l.startswith(('GREEN ', 'RED ')) or 'checks,' in l]
        header = [f'# A3-HF3 RED-first: {TARGET} built against {REV}\'s alpha_runtime.h / .cpp',
                  f'# (git show {REV}:<file>), every other file as in the working tree. Exit {run.returncode}.']
        with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
            h.write('\n'.join(header + out) + '\n')
    finally:
        for f, data in saved.items():
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(data)
            now = time.time()
            os.utime(os.path.join(ROOT, f), (now, now))
        build()
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
