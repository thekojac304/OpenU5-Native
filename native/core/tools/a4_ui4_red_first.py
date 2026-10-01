"""Alpha 4 A4-UI4/PRES1 -- RED-first: run the batch's runtime tests against the
production files of the batch's baseline, then restore them.

    python tools/a4_ui4_red_first.py <red-build-dir> [--snapshot <dir>] [--dump <dir>] [target ...]

<red-build-dir> is a host build configured with -DCMAKE_CXX_FLAGS=-DA4_UI4_HEAD_API,
which compiles out the few checks that need the batch's new API (they print
RED). Every production file the working tree changed since the baseline
(native/core/{src,include}, native/targets/tdeck/main) is swapped for its
baseline blob -- converted to the checkout's own line endings, see
checkout-line-endings-are-mixed -- and touched so ninja rebuilds; the target
is built and run; the working files are restored and touched whatever happens.

--snapshot <dir> (section 8.20, the console package): swap in only the files
a snapshot directory holds (paths relative to native/, e.g. <dir>/core/src/
ui_session.cpp) instead of the baseline's -- the batch's own state before a
change. --dump <dir> passes --dump to every target (screens to compare).
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
BASE = 'e53741b2'  # alpha4-end1-hardware-validated
PACK = 'native/assets/openu5-alpha1-resources.bin'
AUDIO = 'native/assets/openu5-audio.bin'
TARGETS = {
    'a4_ui4_presentation_runtime': [PACK, AUDIO],
    'a4_ui4_moongate_runtime': [PACK, AUDIO],
    'a4_ui4_console_runtime': [PACK, AUDIO],
    'a4_ui1_chrome_runtime': [PACK, 'original/u5/ultima5/IBM.CH'],
}
PRODUCTION = ('native/core/src/', 'native/core/include/', 'native/targets/tdeck/main/')


def changed_production():
    out = subprocess.run(['git', '-C', ROOT, 'diff', '--name-only', BASE, '--'] + list(PRODUCTION),
                         check=True, capture_output=True, text=True).stdout.split()
    return [f for f in out if os.path.exists(os.path.join(ROOT, f))]


def snapshot_blob(snapshot, path, like):
    p = os.path.join(snapshot, path[len('native/'):])
    if not os.path.exists(p):
        return None  # not part of the snapshot: the working file stays
    data = open(p, 'rb').read().replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if b'\r\n' in like else data


def blob(path, like):
    r = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{path}'], capture_output=True)
    if r.returncode:
        return None  # new in the batch: the baseline has no such file
    data = r.stdout.replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if b'\r\n' in like else data


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    build = sys.argv[1]
    rest = sys.argv[2:]
    snapshot = dump = None
    while rest and rest[0] in ('--snapshot', '--dump'):
        if rest[0] == '--snapshot':
            snapshot = rest[1]
        else:
            dump = rest[1]
        rest = rest[2:]
    targets = rest or list(TARGETS)
    files = changed_production()
    if snapshot:
        files = [f for f in files if snapshot_blob(snapshot, f, b'') is not None]
    saved = {}
    base = f'snapshot {snapshot}' if snapshot else f'baseline {BASE}'
    out = [f'# A4-UI4 RED-first: {base}; production files swapped: {", ".join(files) or "none"}']
    try:
        for f in files:
            p = os.path.join(ROOT, f)
            saved[p] = open(p, 'rb').read()
            old = snapshot_blob(snapshot, f, saved[p]) if snapshot else blob(f, saved[p])
            if old is None:
                continue
            open(p, 'wb').write(old)
            touch(p)
        for target in targets:
            out.append(f'## {target}')
            b = subprocess.run([NINJA, '-C', build, target], capture_output=True, text=True)
            if b.returncode:
                out.append('BUILD FAILED')
                out.append(b.stdout[-4000:])
                continue
            exe = os.path.join(build, target + '.exe')
            extra = ['--dump', dump] if dump else []
            r = subprocess.run([exe] + [os.path.join(ROOT, a) for a in TARGETS[target]] + extra,
                               capture_output=True, text=True)
            out += [l for l in r.stdout.splitlines() if l.startswith(('GREEN', 'RED', 'A4-UI4')) or
                    (len(l) > 3 and l[1:3] == '  ' and l[0].isupper())]
    finally:
        for p, data in saved.items():
            open(p, 'wb').write(data)
            touch(p)
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
