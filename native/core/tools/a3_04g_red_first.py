#!/usr/bin/env python
"""Alpha 3 A3-04G -- RED-first: the storage checks against the UNMODIFIED shell.

Swaps in <git-rev>'s alpha_save.cpp, alpha_save_generation.h / .cpp and
alpha_runtime.cpp (the A3-HF3 baseline), builds a3_04g_storage_runtime -- the
test only uses what those files already declare (AlphaSaveService's public
calls, AlphaSaveCandidate / AlphaSaveStage / verify_candidate, the runtime's
keys) plus the fake card and the heap census -- runs it, then restores the
working files byte for byte and touches them (a restored file is older than
its object: without the touch ninja would keep the baseline build).
openu5/system_menu.h keeps the working tree's additive set_save_slots(), which
the baseline runtime does not call.

Usage: python native/core/tools/a3_04g_red_first.py <build-dir> <log> [<git-rev>]
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
FILES = ['native/targets/tdeck/main/alpha_save.cpp',
         'native/targets/tdeck/main/alpha_save_generation.h',
         'native/targets/tdeck/main/alpha_save_generation.cpp',
         'native/targets/tdeck/main/alpha_runtime.cpp']
TARGET = 'a3_04g_storage_runtime'


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def keep(line):
    s = line.strip()
    if s.startswith(('[I]', '[W]', '[E]')):
        return False
    return s.startswith(('GREEN ', 'RED ', '[')) or 'checks GREEN' in s or (
        line.startswith('         ') and not line.startswith('         TMP'))


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
        run = subprocess.run([exe, RES], capture_output=True, text=True, errors='replace')
        out = [l.rstrip() for l in run.stdout.splitlines() if keep(l)]
        header = [f'# A3-04G RED-first: {TARGET} built against {REV}\'s alpha_save.cpp,',
                  f'# alpha_save_generation.h/.cpp and alpha_runtime.cpp (git show {REV}:<file>);',
                  f'# every other file as in the working tree. Exit {run.returncode}.']
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
