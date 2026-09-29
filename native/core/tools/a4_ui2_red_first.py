"""Alpha 4 UI Batch 2 -- RED-first: run a subtrack's new test against the
production files as they were BEFORE that subtrack, then restore them.

    python tools/a4_ui2_red_first.py <build-dir> A|B|C [...] [--dump <dir>]

Each mode swaps in the named files from the commit before its subtrack (the
blob converted to the checkout's own line endings, see
checkout-line-endings-are-mixed), touches them so ninja rebuilds, builds and
runs the subtrack's test, and restores the working files whatever happens.

  A  the title: tdeck_board.cpp + device_ui_views.h of 9ab2564e (UI1). The
     frontend keeps its TitleCredits kind; the old Board ignores it and draws
     the generic body, which is exactly the old screen.
  B  the music: alpha_runtime.cpp of 8aa930e3 (commit A).
  C  the menus: frontend.{h,cpp}, system_menu.{h,cpp}, alpha_runtime.cpp,
     alpha_save.cpp, alpha_save_generation.{h,cpp}, the memory card stub and
     tdeck_board.cpp (the shell subtitle's body clear) of d81733f4 (commit B). The test is black-box (views and keys only), so it
     compiles against either.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
PACK = 'native/assets/openu5-alpha1-resources.bin'
AUDIO = 'native/assets/openu5-audio.bin'

MODES = {
    'A': ('9ab2564e', ['native/targets/tdeck/main/tdeck_board.cpp',
                       'native/targets/tdeck/main/device_ui_views.h'],
          'a4_ui2_title_runtime', [PACK]),
    'B': ('8aa930e3', ['native/targets/tdeck/main/alpha_runtime.cpp'],
          'a4_ui2_death_music_runtime', [PACK, AUDIO]),
    'C': ('d81733f4', ['native/core/include/openu5/frontend.h',
                       'native/core/src/frontend.cpp',
                       'native/core/include/openu5/system_menu.h',
                       'native/core/src/system_menu.cpp',
                       'native/targets/tdeck/main/alpha_runtime.cpp',
                       'native/targets/tdeck/main/alpha_save.cpp',
                       'native/targets/tdeck/main/alpha_save_generation.h',
                       'native/targets/tdeck/main/alpha_save_generation.cpp',
                       'native/targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp',
                       'native/targets/tdeck/main/tdeck_board.cpp'],
          'a4_ui2_save_menu_runtime', [PACK]),
}


def blob(rev, path, like):
    data = subprocess.run(['git', '-C', ROOT, 'show', f'{rev}:{path}'], check=True, capture_output=True).stdout
    data = data.replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if b'\r\n' in like else data


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run_mode(build, mode, extra=()):
    rev, files, target, args = MODES[mode]
    saved = {}
    out = [f'# {target} against the production files of {rev} ({", ".join(os.path.basename(f) for f in files)})']
    try:
        for f in files:
            p = os.path.join(ROOT, f)
            saved[p] = open(p, 'rb').read()
            open(p, 'wb').write(blob(rev, f, saved[p]))
            touch(p)
        b = subprocess.run([NINJA, '-C', build, target], capture_output=True, text=True)
        if b.returncode:
            out.append('BUILD FAILED')
            out.append(b.stdout[-4000:])
        else:
            exe = os.path.join(build, target + '.exe')
            r = subprocess.run([exe] + [os.path.join(ROOT, a) for a in args] + list(extra), capture_output=True, text=True)
            out += [l for l in r.stdout.splitlines() if l.startswith(('GREEN', 'RED', 'A4-UI2')) or
                    (len(l) > 3 and l[1:3] == '  ' and l[0].isupper())]
    finally:
        for p, data in saved.items():
            open(p, 'wb').write(data)
            touch(p)
        subprocess.run([NINJA, '-C', build, target], capture_output=True)
    return '\n'.join(out)


if __name__ == '__main__':
    build = os.path.abspath(sys.argv[1])
    rest = sys.argv[2:]
    extra = []
    if '--dump' in rest:  # --dump <dir>: the test's own PNG dump of the BEFORE screens
        i = rest.index('--dump')
        extra = rest[i:i + 2]
        rest = rest[:i] + rest[i + 2:]
    for m in rest:
        print(run_mode(build, m, extra))
