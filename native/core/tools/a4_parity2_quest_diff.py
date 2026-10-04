"""A4-PARITY2: what a native change does to the quest_parity corpus, case by case.

Usage: python a4_parity2_quest_diff.py <old quest_driver.exe> <new quest_driver.exe>

quest_parity is a LIVE comparison (the TypeScript Game builds the expected stream when ctest runs; nothing
is stored). The input rows are written by check-quests.ts to native/core/build-quests/input.jsonl; this
script runs both drivers over the same input and reports which cases differ and exactly which JSON paths
changed in each (recursively), so a claim like "15 of 5,377 cases move, characters[0].currentMp only" is
checkable. Run `ctest -R quest_parity` (or check-quests.ts once) first so input.jsonl is current."""
import json
import os
import subprocess
import sys
import tempfile
from collections import Counter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
inp = os.path.join(ROOT, 'native/core/build-quests/input.jsonl')


def run(exe):
    out = os.path.join(tempfile.mkdtemp(), 'out.jsonl')
    r = subprocess.run([exe, inp, out], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'{exe} failed: {r.stderr[-300:]}')
    return [json.loads(l) for l in open(out, encoding='utf-8') if l.strip()]


def walk(a, b, path, out):
    if type(a) != type(b):
        out.append(path)
    elif isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                out.append(path + '.' + k)
            else:
                walk(a[k], b[k], path + '.' + k, out)
    elif isinstance(a, list):
        if len(a) != len(b):
            out.append(path + '[len]')
        else:
            for i, (x, y) in enumerate(zip(a, b)):
                walk(x, y, path + f'[{i}]', out)
    elif a != b:
        out.append(path)


old, new = run(os.path.abspath(sys.argv[1])), run(os.path.abspath(sys.argv[2]))
print(f'cases: {len(old)} vs {len(new)}')
changed = [i for i in range(min(len(old), len(new))) if old[i] != new[i]]
paths = Counter()
for i in changed:
    diffs = []
    walk(old[i], new[i], '', diffs)
    for d in diffs:
        # collapse step indices so the account is by field
        import re
        paths[re.sub(r'\[\d+\]', '[]', d)] += 1
print(f'cases that differ: {len(changed)}; first: {changed[:10]}')
print('changed JSON paths (indices collapsed):')
for p, n in sorted(paths.items()):
    print(f'  {n:4}  {p}')
