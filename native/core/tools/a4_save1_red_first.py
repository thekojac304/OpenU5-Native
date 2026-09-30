"""Alpha 4 A4-SAVE1 RED-first driver.

Swaps HEAD's alpha_save.cpp / alpha_save_generation.{h,cpp} (converted to the
checkout's line endings) into the tree, builds and runs
a4_save1_recovery_runtime, then restores the working files and touches them so
ninja rebuilds. Usage (w64devkit on PATH):

    python tools/a4_save1_red_first.py build-a4-save1 > a4-save1-red-first.log
"""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(CORE))
FILES = [
    'native/targets/tdeck/main/alpha_save.cpp',
    'native/targets/tdeck/main/alpha_save_generation.cpp',
    'native/targets/tdeck/main/alpha_save_generation.h',
]
CMAKE_BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'


def main():
    build = os.path.join(CORE, sys.argv[1] if len(sys.argv) > 1 else 'build-a4-save1')
    saved = {}
    for rel in FILES:
        path = os.path.join(REPO, rel)
        saved[rel] = open(path, 'rb').read()
        eol = b'\r\n' if b'\r\n' in saved[rel] else b'\n'
        head = subprocess.run(['git', '-C', REPO, 'show', 'HEAD:' + rel], capture_output=True, check=True).stdout
        head = head.replace(b'\r\n', b'\n')
        if eol == b'\r\n':
            head = head.replace(b'\n', b'\r\n')
        open(path, 'wb').write(head)
    try:
        ninja = os.path.join(CMAKE_BIN, 'ninja.exe')
        b = subprocess.run([ninja, '-C', build, 'a4_save1_recovery_runtime'], capture_output=True, text=True)
        print('build (HEAD production):', b.returncode)
        if b.returncode:
            print(b.stdout[-4000:], b.stderr[-2000:])
            return 1
        r = subprocess.run([os.path.join(build, 'a4_save1_recovery_runtime.exe'),
                            os.path.join(CORE, '..', 'assets', 'openu5-alpha1-resources.bin')],
                           capture_output=True, text=True)
        print(r.stdout)
        print('exit (HEAD production):', r.returncode)
    finally:
        now = time.time()
        for rel, data in saved.items():
            path = os.path.join(REPO, rel)
            open(path, 'wb').write(data)
            os.utime(path, (now + 2, now + 2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
