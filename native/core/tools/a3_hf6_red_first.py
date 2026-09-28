#!/usr/bin/env python
"""Alpha 3 A3-HF6 (H-183) -- RED-first: the shrine/Codex key-wait checks against the UNMODIFIED logic.

Swaps in <git-rev>'s alpha_runtime.cpp and dialogue_pacer.cpp, builds
a3_hf6_shrine_key_wait_runtime and a3_hf6_shrine_key_wait_pacer, runs both, and
restores the working files byte for byte and touches them (a restored file is
older than its object: without the touch ninja would keep the baseline build).

Two mechanical edits are applied to the baseline files, neither a behaviour:
  * alpha_runtime.cpp -- A3-HF6's extraction of the three shrine-service
    assignments into bind_shrine_services(), which the host fixture now calls
    so the rite and the Codex read the pack's real MISCMSG records;
  * dialogue_pacer.cpp -- paced_event_pause() defined as the baseline's own
    rule (dialogue_event_pause: a ShrineKeyWait is no pause), so the unit test
    links and states what the baseline does rather than failing to build.
The headers, the fixture and the tests stay as in the working tree (the
header change is that one declaration and comments).

Usage: python native/core/tools/a3_hf6_red_first.py <build-dir> <log> [<git-rev>]
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
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
PACER = 'native/core/src/dialogue_pacer.cpp'
TARGETS = [('a3_hf6_shrine_key_wait_runtime', [RES]), ('a3_hf6_shrine_key_wait_pacer', [])]

BIND_OLD = (b'    shrine_services_.data=&resources_.shrine_data;\n'
            b'    shrine_services_.context=this;\n'
            b'    shrine_services_.record=[](void *p,int32_t index)->const char*{auto&r=*static_cast<AlphaRuntime*>(p);'
            b'return tdeck::misc_text_record({r.resources_.misc_text_offsets,r.resources_.misc_text_records,'
            b'r.resources_.misc_text_record_count},index);};\n')
BIND_NEW = b'    bind_shrine_services();\n'
ANCHOR = b'void AlphaRuntime::dispatch_ui(void *p,const openu5::UiIntent&i){'
BINDER = b'void AlphaRuntime::bind_shrine_services(){\n' + BIND_OLD + b'}\n\n'
PACER_ANCHOR = b'namespace {\n// Every payload pointer'
PACER_SHIM = (b'DialoguePause paced_event_pause(const GameEvent &e) { return dialogue_event_pause(e); }\n\n')


def build():
    env = dict(os.environ)
    env['PATH'] = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin;' + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target'] + [t for t, _ in TARGETS], env=env,
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def run(out, label):
    for target, args in TARGETS:
        r = subprocess.run([os.path.join(BUILD, target + '.exe')] + args, capture_output=True, text=True,
                           errors='replace')
        out.append(f'## {target} against {label}: exit {r.returncode}')
        out += [l for l in r.stdout.splitlines() if l.startswith(('GREEN ', 'RED ')) or 'checks,' in l]


def baseline(path):
    text = subprocess.run(['git', '-C', ROOT, 'show', f'{REV}:{path}'], capture_output=True, check=True).stdout
    return text.replace(b'\r\n', b'\n')


def main():
    saved = {p: open(os.path.join(ROOT, p), 'rb').read() for p in (RUNTIME, PACER)}
    out = [f'# A3-HF6 RED-first: built against {REV}:{RUNTIME} (plus the binder extraction) and',
           f'# {REV}:{PACER} (plus paced_event_pause = the baseline rule); every header, the fixture',
           '# and the tests as in the working tree.']
    try:
        rt = baseline(RUNTIME)
        assert rt.count(BIND_OLD) == 1 and rt.count(ANCHOR) == 1
        rt = rt.replace(BIND_OLD, BIND_NEW).replace(ANCHOR, BINDER + ANCHOR)
        pc = baseline(PACER)
        assert pc.count(PACER_ANCHOR) == 1
        pc = pc.replace(PACER_ANCHOR, PACER_SHIM + PACER_ANCHOR)
        for p, text in ((RUNTIME, rt), (PACER, pc)):
            with open(os.path.join(ROOT, p), 'wb') as h:
                h.write(text)
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
