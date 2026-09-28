#!/usr/bin/env python
"""Alpha 3 A3-HF4 -- RED-first: the load-transient checks against the UNMODIFIED behaviour.

Swaps in <git-rev>'s copies of the two production files whose behaviour A3-HF4
changes (UiSession, AlphaRuntime), builds a3_hf4_load_transient_runtime, runs
it, and restores the working files byte for byte and touches them (a restored
file is older than its object: without the touch ninja would keep the baseline
build).

The headers stay as in the working tree. Their A3-HF4 additions are a
declaration nothing in the baseline .cpp calls (UiSession::reset_after_load,
AlphaRuntime::reset_transient_after_load) and the inline, read-only
TransientProbe accessor the test observes through; neither changes behaviour.

Usage: python native/core/tools/a3_hf4_red_first.py <build-dir> <log> [<git-rev>]
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
FILES = ['native/core/src/ui_session.cpp', 'native/targets/tdeck/main/alpha_runtime.cpp']
TARGET = 'a3_hf4_load_transient_runtime'


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def run():
    r = subprocess.run([os.path.join(BUILD, TARGET + '.exe'), RES], capture_output=True, text=True, errors='replace')
    lines = [l for l in r.stdout.splitlines() if l.startswith(('GREEN ', 'RED ')) or 'checks,' in l]
    return r.returncode, lines


def main():
    saved = {f: open(os.path.join(ROOT, f), 'rb').read() for f in FILES}
    out = [f'# A3-HF4 RED-first: built against {REV}\'s production files (git show {REV}:<file>),',
           '# every header and test file as in the working tree:']
    out += ['#   ' + f for f in FILES]
    try:
        for f in FILES:
            base = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{f}'], capture_output=True, check=True).stdout
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(base)
        code, text = build()
        if code:
            print(text)
            return 2
        rc, lines = run()
        out.append(f'## {TARGET} against {REV}: exit {rc}')
        out += lines
    finally:
        for f, data in saved.items():
            with open(os.path.join(ROOT, f), 'wb') as h:
                h.write(data)
            now = time.time()
            os.utime(os.path.join(ROOT, f), (now, now))
    code, text = build()
    if code:
        print(text)
        return 2
    rc, lines = run()
    out.append(f'## {TARGET} against the working tree: exit {rc}')
    out += lines
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
