#!/usr/bin/env python
"""Alpha 3 A3-HF5 -- RED-first: the dialogue-pacing checks against the UNMODIFIED runtime.

Swaps in <git-rev>'s alpha_runtime.cpp, builds a3_hf5_dialogue_pacing_runtime,
runs it, and restores the working file byte for byte and touches it (a restored
file is older than its object: without the touch ninja would keep the baseline
build).

One mechanical edit is applied to the baseline file: A3-HF5's no-behaviour
extraction of the one-line TLK registry binding into bind_dialogue_services(),
which the host fixture now calls so that it talks through the pack's real
corpus. Everything else in the baseline .cpp is <git-rev>'s. The headers stay as
in the working tree: their additions are the pacer's storage (allocated by the
fixture, never attached by the baseline binder, so its cadence stays 0 and
offer() never engages), declarations the baseline never calls, and a read-only
accessor.

Usage: python native/core/tools/a3_hf5_red_first.py <build-dir> <log> [<git-rev>]
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
FILE = 'native/targets/tdeck/main/alpha_runtime.cpp'
TARGET = 'a3_hf5_dialogue_pacing_runtime'
BIND_OLD = (b'    dialogue_assets_.bind(resources_.dialogue_data,resources_.dialogue_data_size);'
            b'dialogue_services_.registry={&dialogue_assets_,AlphaDialogueCache::lookup};')
BIND_NEW = b'    bind_dialogue_services();'
BINDER = (b'void AlphaRuntime::bind_dialogue_services(){\n'
          b'    dialogue_assets_.bind(resources_.dialogue_data,resources_.dialogue_data_size);\n'
          b'    dialogue_services_.registry={&dialogue_assets_,AlphaDialogueCache::lookup};\n'
          b'}\n\n')
ANCHOR = b'void AlphaRuntime::dispatch_ui(void *p,const openu5::UiIntent&i){'


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
    saved = open(os.path.join(ROOT, FILE), 'rb').read()
    out = [f'# A3-HF5 RED-first: built against {REV}:{FILE} (plus the binder extraction),',
           '# every header, the fixture and the test as in the working tree.']
    try:
        base = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{FILE}'], capture_output=True, check=True).stdout
        base = base.replace(b'\r\n', b'\n')
        assert base.count(BIND_OLD) == 1 and base.count(ANCHOR) == 1
        base = base.replace(BIND_OLD, BIND_NEW).replace(ANCHOR, BINDER + ANCHOR)
        with open(os.path.join(ROOT, FILE), 'wb') as h:
            h.write(base)
        code, text = build()
        if code:
            print(text)
            return 2
        rc, lines = run()
        out.append(f'## {TARGET} against {REV}: exit {rc}')
        out += lines
    finally:
        with open(os.path.join(ROOT, FILE), 'wb') as h:
            h.write(saved)
        now = time.time()
        os.utime(os.path.join(ROOT, FILE), (now, now))
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
