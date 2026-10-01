"""A4-UI4 hardware follow-up (ALPHA4_UI.md section 8.22) -- mutation check of
the console-height fix: each mutant edits the production files, rebuilds
a4_ui4_console_runtime and must turn it RED. A mutant that does not build is
INVALID, not killed.

    python tools/a4_ui4_hf1_mutation_check.py <build-dir> [K1,K3,...]

Anchors are matched in each file's own line endings (checkout-line-endings-
are-mixed); every file is restored and touched after each mutant (a restored
file is older than the mutated object, so ninja would keep the mutant).
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
MAIN = 'native/targets/tdeck/main/'
TARGET = 'a4_ui4_console_runtime'

MUTANTS = {
    'K1': ('the hardware defect: page_rows 12 again', MAIN + 'alpha_runtime.h',
           'return {11, uint16_t(kConsoleMaxRows), 63};', 'return {11, 12, 63};'),
    'K2': ('page_rows 19 (the 8 px cell\'s rows): Small short', MAIN + 'alpha_runtime.h',
           'return {11, uint16_t(kConsoleMaxRows), 63};', 'return {11, 19, 63};'),
    'K3': ('page_rows one short of the Board', MAIN + 'alpha_runtime.h',
           'return {11, uint16_t(kConsoleMaxRows), 63};', 'return {11, uint16_t(kConsoleMaxRows-1), 63};'),
    'K4': ('the row cap from Medium\'s metrics (19)', MAIN + 'device_ui_views.h',
           'size_t((240-88)/ui_text_metrics(0).line_height)', 'size_t((240-88)/ui_text_metrics(1).line_height)'),
    'K5': ('the Board\'s rows back to the 8 px cell\'s 19', MAIN + 'tdeck_board.h',
           'constexpr size_t kAlphaTranscriptLines = kConsoleMaxRows;',
           'constexpr size_t kAlphaTranscriptLines = openu5::kHudTranscriptLines;'),
    'K6': ('the shared constructor ignores session_config()', MAIN + 'alpha_runtime.cpp',
           'ui_=new(memory)openu5::UiSession({blocks,block_count},{this,dispatch_ui},session_config());',
           'ui_=new(memory)openu5::UiSession({blocks,block_count},{this,dispatch_ui},{11,12,63});'),
}


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run(build):
    r = subprocess.run([NINJA, '-C', build, TARGET], capture_output=True, text=True)
    if r.returncode:
        return 'INVALID', r.stdout[-600:]
    r = subprocess.run([os.path.join(build, TARGET + '.exe'),
                        os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin'),
                        os.path.join(ROOT, 'native/assets/openu5-audio.bin')], capture_output=True, text=True)
    reds = [l for l in r.stdout.splitlines() if l.startswith('RED ')]
    summary = [l for l in r.stdout.splitlines() if 'console runtime:' in l]
    return ('KILLED' if r.returncode else 'SURVIVED'), '\n'.join(reds[:6] + summary)


def main():
    build = os.path.abspath(sys.argv[1])
    only = sys.argv[2].split(',') if len(sys.argv) > 2 else list(MUTANTS)
    results = {}
    for mid in only:
        what, rel, old, new = MUTANTS[mid]
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        assert text.count(old) == 1, (mid, old)
        try:
            open(path, 'wb').write(text.replace(old, new, 1).encode('utf-8'))
            touch(path)
            verdict, detail = run(build)
        finally:
            open(path, 'wb').write(original)
            touch(path)
        results[mid] = verdict
        print(f'{mid} {verdict:8} {what}\n    {detail.replace(chr(10), chr(10) + "    ")}', flush=True)
    verdict, detail = run(build)
    print(f'restored tree: {"GREEN" if verdict == "SURVIVED" else verdict}\n    {detail}')
    killed = sum(v == 'KILLED' for v in results.values())
    print(f'\nA4-UI4 hardware follow-up mutations: {killed}/{len(results)} killed')
    return 0 if killed == len(results) and verdict == 'SURVIVED' else 1


if __name__ == '__main__':
    sys.exit(main())
