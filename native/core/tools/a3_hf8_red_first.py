#!/usr/bin/env python
"""Alpha 3 A3-HF8 (H-186) -- RED-first: the sacrifice-burst checks against the UNMODIFIED logic.

Swaps in <git-rev>'s blackthorn.cpp, blackthorn_scene.cpp and alpha_runtime.cpp,
builds a3_hf8_sacrifice_burst_runtime and blackthorn_scene_tests, runs both, and
restores the working files byte for byte and touches them (a restored file is
older than its object: without the touch ninja would keep the baseline build).

The ONE edit applied to a baseline file is the pure extraction of
bind_blackthorn_scene() out of <git-rev>'s AlphaRuntime::initialize() -- the
same four assignments, moved into the binder the host fixture now calls -- so
the fixture links. Without it the baseline cannot even stage the throne room
on the host (the fixture used to null capture_tiles), and every check would be
RED for a harness reason instead of the defect. The headers (the beat's burst
cell, the appended BlackthornSfx, the view's burst cell), the fixture and the
tests stay as in the working tree: the baseline .cpp files compile against them
unchanged and simply never set a burst -- which is the defect.

Usage: python native/core/tools/a3_hf8_red_first.py <build-dir> <log> [<git-rev>]
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
AUDIO = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
DS = os.path.join(ROOT, 'game/assets/ds-strings.json')
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
FILES = [RT, 'native/core/src/blackthorn.cpp', 'native/core/src/blackthorn_scene.cpp']
TARGETS = [('a3_hf8_sacrifice_burst_runtime', [RES, AUDIO]), ('blackthorn_scene_tests', [DS, RES])]

INIT_BLOCK = b'''    context_.blackthorn=&blackthorn_;
    // #324 / R-32. Wiring capture_tiles is what turns the staged scene on:
    // with the packed throne room absent the capture emits the same
    // text-only stream it always did, which is the degradation every parity
    // harness relies on.
    blackthorn_scene_services_.capture_tiles=resources_.blackthorn_scene_tiles;
    blackthorn_scene_services_.state=&blackthorn_scene_state_;
    blackthorn_scene_services_.script=blackthorn_script_;
    context_.blackthorn_scene=&blackthorn_scene_services_;
'''
BINDER = b'''void AlphaRuntime::bind_blackthorn_scene(){
    blackthorn_scene_services_.capture_tiles=resources_.blackthorn_scene_tiles;
    blackthorn_scene_services_.state=&blackthorn_scene_state_;
    blackthorn_scene_services_.script=blackthorn_script_;
    context_.blackthorn_scene=&blackthorn_scene_services_;
}

'''
SHRINE = b'void AlphaRuntime::bind_shrine_services(){'


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target'] + [t for t, _ in TARGETS], env=env,
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def run(out, label):
    for target, args in TARGETS:
        r = subprocess.run([os.path.join(BUILD, target + ".exe")] + args, capture_output=True, text=True,
                           errors='replace')
        out.append(f'## {target} against {label}: exit {r.returncode}')
        out += [l for l in (r.stdout + r.stderr).splitlines()
                if l.startswith(('GREEN ', 'RED ', '[FAIL]', '    burst on', '    captured')) or 'checks,' in l]


def baseline(path):
    text = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{path}'], capture_output=True, check=True).stdout
    if path == RT:
        assert text.count(INIT_BLOCK) == 1 and text.count(SHRINE) == 1, 'baseline anchors moved'
        text = text.replace(INIT_BLOCK, b'    context_.blackthorn=&blackthorn_;\n    bind_blackthorn_scene();\n')
        text = text.replace(SHRINE, BINDER + SHRINE)
    return text


def main():
    saved = {p: open(os.path.join(ROOT, p), 'rb').read() for p in FILES}
    out = [f'# A3-HF8 RED-first: built against {REV}:' + f', {REV}:'.join(FILES) + ';',
           '# (alpha_runtime.cpp with bind_blackthorn_scene() extracted, nothing else);',
           '# every header, the fixture and the tests as in the working tree.']
    try:
        for p in FILES:
            with open(os.path.join(ROOT, p), 'wb') as h:
                h.write(baseline(p))
        code, text = build()
        if code:
            print(text)
            return 2
        run(out, REV)
    finally:
        now = time.time()
        for p, text in saved.items():
            with open(os.path.join(ROOT, p), 'wb') as h:
                h.write(text)
            os.utime(os.path.join(ROOT, p), (now, now))
    code, text = build()
    if code:
        print(text)
        return 2
    run(out, 'the working tree')
    with open(LOG, 'w', encoding='utf-8', newline='\n') as h:
        h.write('\n'.join(out) + '\n')
    print('\n'.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
