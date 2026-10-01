"""A4-UI4 hardware follow-up (ALPHA4_UI.md section 8.22) -- RED-first for the
console height: run a4_ui4_console_runtime against the production files as
they were before the follow-up, then restore them.

    python tools/a4_ui4_hf1_red_first.py <build-dir> <snapshot-dir> [target]

[target] defaults to a4_ui4_console_runtime; a3_04f_render_runtime runs the
panel golden's script on the old files (the golden proof, section 8.22).

<snapshot-dir> holds the pre-follow-up copies, laid out relative to native/
(targets/tdeck/main/alpha_runtime.{h,cpp}, tdeck_board.h, device_ui_views.h)
plus alpha_runtime_host_fixture.cpp at its root (the fixture's pre-follow-up
literal config). Each is converted to the checkout's own line endings (see
checkout-line-endings-are-mixed), swapped in and touched; the target is built
and run; the working files are restored and touched whatever happens.
"""
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
FILES = {
    'native/targets/tdeck/main/alpha_runtime.h': 'targets/tdeck/main/alpha_runtime.h',
    'native/targets/tdeck/main/alpha_runtime.cpp': 'targets/tdeck/main/alpha_runtime.cpp',
    'native/targets/tdeck/main/tdeck_board.h': 'targets/tdeck/main/tdeck_board.h',
    'native/targets/tdeck/main/device_ui_views.h': 'targets/tdeck/main/device_ui_views.h',
    'native/targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp': 'alpha_runtime_host_fixture.cpp',
}


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def main():
    build, snap = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    saved = {}
    try:
        for path, rel in FILES.items():
            src = os.path.join(snap, rel)
            if not os.path.exists(src):
                continue
            full = os.path.join(ROOT, path)
            saved[full] = open(full, 'rb').read()
            data = open(src, 'rb').read().replace(b'\r\n', b'\n')
            if b'\r\n' in saved[full]:
                data = data.replace(b'\n', b'\r\n')
            open(full, 'wb').write(data)
            touch(full)
            print('swapped', path)
        target = sys.argv[3] if len(sys.argv) > 3 else 'a4_ui4_console_runtime'
        r = subprocess.run([NINJA, '-C', build, target], capture_output=True, text=True)
        print(r.stdout[-2000:], r.stderr[-2000:])
        if r.returncode:
            print('BUILD FAILED')
            return 2
        exe = os.path.join(build, target + '.exe')
        args = [os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')]
        if target == 'a4_ui4_console_runtime':
            args.append(os.path.join(ROOT, 'native/assets/openu5-audio.bin'))
        r = subprocess.run([exe] + args, capture_output=True, text=True)
        print(r.stdout)
        print('exit', r.returncode)
        return 0
    finally:
        for full, data in saved.items():
            open(full, 'wb').write(data)
            touch(full)
        print('restored', len(saved), 'files')


if __name__ == '__main__':
    sys.exit(main())
