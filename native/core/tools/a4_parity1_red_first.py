"""A4-PARITY1 RED-first: run a4_parity1_runtime against the PRE-PARITY1 production files.

Usage: python tools/a4_parity1_red_first.py <build-dir> <pre-ui4-baseline-dir>

PARITY1 sits on the uncommitted A4-UI4/PRES1 tree, so "before PARITY1" is not HEAD for every
file: quest_world.cpp and ui_session.cpp come from the saved UI4/hf1 working-tree copy
(<pre-ui4-baseline-dir>/native/core/src/...), the rest from `git show HEAD:` (converted to the
file's own line endings -- `git show` emits LF). Each file is restored and touched afterwards
so ninja rebuilds it (a restored file is older than the mutated object).
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE_BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
FROM_HEAD = ['native/core/src/magic.cpp', 'native/core/src/combat.cpp', 'native/core/src/combat_magic.inc',
             'native/core/src/commands.cpp', 'native/core/src/look.cpp']
FROM_UI4 = ['native/core/src/quest_world.cpp', 'native/core/src/ui_session.cpp']


def eol_of(data):
    return b'\r\n' if b'\r\n' in data else b'\n'


def main():
    build, ui4 = sys.argv[1], sys.argv[2]
    env = dict(os.environ, PATH=CMAKE_BIN + os.pathsep + os.environ['PATH'])
    saved = {}
    try:
        for rel in FROM_HEAD + FROM_UI4:
            path = os.path.join(ROOT, rel)
            cur = open(path, 'rb').read()
            saved[path] = cur
            if rel in FROM_HEAD:
                old = subprocess.run(['git', 'show', 'HEAD:' + rel], cwd=ROOT, capture_output=True, check=True).stdout
            else:
                old = open(os.path.join(ui4, rel), 'rb').read()
            old = old.replace(b'\r\n', b'\n')
            if eol_of(cur) == b'\r\n':
                old = old.replace(b'\n', b'\r\n')
            open(path, 'wb').write(old)
        r = subprocess.run([CMAKE_BIN + '/ninja.exe', '-C', build, 'a4_parity1_runtime'], env=env,
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-3000:], r.stderr[-3000:])
            print('BUILD FAILED')
            return 2
        exe = os.path.join(build, 'a4_parity1_runtime.exe')
        r = subprocess.run([exe, os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin'),
                            os.path.join(ROOT, 'native/assets/openu5-audio.bin')], capture_output=True, text=True,
                           errors='replace')
        for line in r.stdout.splitlines():
            if line.startswith(('GREEN', 'RED', 'C  ', 'D  ', 'W  ', 'A4-PARITY1')):
                print(line)
        print('exit', r.returncode)
    finally:
        for path, data in saved.items():
            open(path, 'wb').write(data)
            os.utime(path, None)
        subprocess.run([CMAKE_BIN + '/ninja.exe', '-C', build, 'a4_parity1_runtime'], env=env, capture_output=True)
        print('restored', len(saved), 'files and rebuilt')
    return 0


if __name__ == '__main__':
    sys.exit(main())
