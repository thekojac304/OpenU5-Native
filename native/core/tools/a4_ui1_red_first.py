"""Alpha 4 UI Batch 1: run the chrome checks against HEAD's production files.

Swaps git HEAD's tdeck_board.{h,cpp} and alpha_runtime.cpp into the tree (the
test file stays the working one), rebuilds a4_ui1_chrome_runtime, runs it, and
restores the working files (touched so ninja rebuilds them).

    python a4_ui1_red_first.py [build-dir] [rev]
"""
import os, subprocess, sys, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.join(CORE, sys.argv[1] if len(sys.argv) > 1 else 'build-a4-ui1')
REV = sys.argv[2] if len(sys.argv) > 2 else 'HEAD'
NINJA = r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin\ninja.exe'
FILES = ['native/targets/tdeck/main/tdeck_board.h', 'native/targets/tdeck/main/tdeck_board.cpp',
         'native/targets/tdeck/main/alpha_runtime.cpp']
env = dict(os.environ, PATH=r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin;' + os.environ['PATH'])
saved = {f: open(os.path.join(ROOT, f), 'rb').read() for f in FILES}
try:
    for f in FILES:
        blob = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{f}'], capture_output=True, check=True).stdout
        open(os.path.join(ROOT, f), 'wb').write(blob)
    build = subprocess.run([NINJA, '-C', BUILD, 'a4_ui1_chrome_runtime'], env=env, capture_output=True, text=True)
    if build.returncode:
        print(build.stdout[-3000:])
        sys.exit('RED-first build failed')
    run = subprocess.run([os.path.join(BUILD, 'a4_ui1_chrome_runtime.exe'),
                          os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin'),
                          os.path.join(ROOT, 'original', 'u5', 'ultima5', 'IBM.CH')], capture_output=True, text=True)
    lines = [l for l in run.stdout.splitlines() if l.startswith(('GREEN', 'RED', 'A4-UI1'))]
    print(f'# {REV}\'s Board / runtime with the working a4_ui1 checks')
    print('\n'.join(lines))
finally:
    for f, data in saved.items():
        open(os.path.join(ROOT, f), 'wb').write(data)
        os.utime(os.path.join(ROOT, f), None)
    time.sleep(1)
    for f in FILES:
        os.utime(os.path.join(ROOT, f), None)
    subprocess.run([NINJA, '-C', BUILD, 'a4_ui1_chrome_runtime'], env=env, capture_output=True)
