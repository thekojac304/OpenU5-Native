#!/usr/bin/env python
"""Alpha 3 A3-HF10 (D-6 / D-70) -- RED-first: the Mix checks against the UNMODIFIED logic.

Native half: swaps in <git-rev>'s ui_session.{h,cpp}, magic.{h,cpp} and
alpha_runtime.{h,cpp}, builds a3_hf10_mix_parity_runtime, runs it, then
restores the working files byte for byte and touches them (a restored file is
older than its object: without the touch ninja would keep the baseline build),
rebuilds and runs it again. The test only uses APIs that predate HF10, so the
baseline compiles with NO shim: what runs is exactly <git-rev>'s device Mix --
the recipe's own mask and a quantity of 1, dispatched the moment a spell is
chosen.

TypeScript half: swaps in <git-rev>'s game/src/ui/prompt-manager.ts,
game/src/core/magic/mix.ts and game/src/main.ts (converted to the checkout's
line endings: `git show` writes LF blobs, A3-HF9) and runs the two vitest files
that pin the binary's getnum and quantity order; restored, runs them again.

Usage: python native/core/tools/a3_hf10_red_first.py <build-dir> <log> [<git-rev>]
"""
import os
import re
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
CMAKE = BIN + '/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
REV = sys.argv[3] if len(sys.argv) > 3 else 'HEAD'
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
TARGET = 'a3_hf10_mix_parity_runtime'
NATIVE = ['native/core/include/openu5/ui_session.h', 'native/core/src/ui_session.cpp',
          'native/core/include/openu5/magic.h', 'native/core/src/magic.cpp',
          'native/targets/tdeck/main/alpha_runtime.h', 'native/targets/tdeck/main/alpha_runtime.cpp']
TS = ['game/src/ui/prompt-manager.ts', 'game/src/core/magic/mix.ts', 'game/src/main.ts']
TS_TESTS = ['tests/mix-hf10-quantity.test.ts', 'tests/prompt-manager.test.ts']
GAME = os.path.join(ROOT, 'game')
NPX = 'npx.cmd' if os.name == 'nt' else 'npx'


def env():
    e = dict(os.environ)
    e['PATH'] = BIN + ';' + e.get('PATH', '')
    return e


def build():
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env(), capture_output=True, text=True,
                       errors='replace')
    return r.returncode, r.stdout + r.stderr


def run_native(out, label):
    r = subprocess.run([os.path.join(BUILD, TARGET + '.exe'), RES], capture_output=True, text=True, errors='replace')
    lines = (r.stdout + r.stderr).splitlines()
    red = [l for l in lines if l.startswith('RED ')]
    green = [l for l in lines if l.startswith('GREEN ')]
    out.append(f'## {TARGET} against {label}: exit {r.returncode}, {len(green)} GREEN, {len(red)} RED')
    out += ['    ' + l for l in lines if l.startswith(('GREEN ', 'RED ')) or 'checks,' in l]
    return len(red), len(green)


def run_ts(out, label):
    e = dict(os.environ, NO_COLOR='1', FORCE_COLOR='0')
    p = subprocess.run([NPX, 'vitest', 'run'] + TS_TESTS, cwd=GAME, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=e)
    text = p.stdout + p.stderr
    tally = [l.strip() for l in text.splitlines() if re.match(r'\s*(Test Files|Tests)\s', l)]
    fails = sorted({re.sub(r'[^\x20-\x7e]', '-', l.strip()) for l in text.splitlines()
                    if l.lstrip().startswith(('FAIL', '×', 'x ')) and '>' in l})
    out.append(f'## vitest {" ".join(TS_TESTS)} against {label}: exit {p.returncode}  ' + ' | '.join(tally))
    out += ['    ' + f for f in fails]
    return p.returncode


def baseline(path):
    text = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{path}'], capture_output=True, check=True).stdout
    current = open(os.path.join(ROOT, path), 'rb').read()
    if b'\r\n' in current:
        text = text.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    return text


def swap(paths):
    saved = {p: open(os.path.join(ROOT, p), 'rb').read() for p in paths}
    for p in paths:
        with open(os.path.join(ROOT, p), 'wb') as h:
            h.write(baseline(p))
    return saved


def restore(saved):
    now = time.time()
    for p, text in saved.items():
        with open(os.path.join(ROOT, p), 'wb') as h:
            h.write(text)
        os.utime(os.path.join(ROOT, p), (now, now))


def main():
    out = [f'# A3-HF10 RED-first: {TARGET} built against {REV}:' + f', {REV}:'.join(NATIVE) + ';',
           '# the fixture and the test as in the working tree, no shim.']
    saved = swap(NATIVE)
    try:
        code, text = build()
        if code:
            print(text[-4000:])
            return 2
        old_red, old_green = run_native(out, REV)
    finally:
        restore(saved)
    code, text = build()
    if code:
        print(text[-4000:])
        return 2
    new_red, new_green = run_native(out, 'the working tree')
    out.append('')
    out.append(f'# TypeScript: {REV}:' + f', {REV}:'.join(TS) + ' (checkout EOL) against the working-tree tests')
    saved = swap(TS)
    try:
        ts_old = run_ts(out, REV)
    finally:
        restore(saved)
    ts_new = run_ts(out, 'the working tree')
    verdict = old_red > 0 and new_red == 0 and ts_old != 0 and ts_new == 0
    out.append(f'# native: {REV} {old_red} RED / {old_green} GREEN -> tree {new_red} RED / {new_green} GREEN; '
               f'TS: {REV} exit {ts_old} -> tree exit {ts_new}; {"RED-first PROVEN" if verdict else "NOT PROVEN"}')
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(l for l in out if 'GREEN ' not in l or l.startswith('#')))
    return 0 if verdict else 1


if __name__ == '__main__':
    sys.exit(main())
