"""A4-PARITY2: token-level account of a regenerated fixture against its committed (HEAD) version.

Usage: python a4_parity2_corpus_diff.py <fixture path relative to the repo root> [--base <rev>]

Splits both versions into lines and each line into whitespace-separated tokens; reports the number of
lines and tokens that differ, the row indices, and (for rows of equal width) which token columns changed,
so a claim like "exactly the OUCH snapshots of one transport class moved" can be checked mechanically.
Line endings are ignored (the checkout mixes LF and CRLF; blobs are LF)."""
import subprocess
import sys
from collections import Counter

rel = sys.argv[1]
base = 'HEAD'
if '--base' in sys.argv:
    base = sys.argv[sys.argv.index('--base') + 1]
old = subprocess.run(['git', 'show', f'{base}:{rel}'], capture_output=True, check=True).stdout.decode('utf-8')
new = open(rel, 'rb').read().decode('utf-8')
a = old.replace('\r\n', '\n').split('\n')
b = new.replace('\r\n', '\n').split('\n')
print(f'{rel}: lines {len(a)} -> {len(b)}')
if len(a) != len(b):
    print('LINE COUNT CHANGED')
rows = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
tok_diff = 0
tok_total = 0
cols = Counter()
width_changed = 0
for i in range(min(len(a), len(b))):
    ta, tb = a[i].split(), b[i].split()
    tok_total += len(ta)
    if ta == tb:
        continue
    if len(ta) != len(tb):
        width_changed += 1
        tok_diff += max(len(ta), len(tb))
        continue
    for k, (x, y) in enumerate(zip(ta, tb)):
        if x != y:
            tok_diff += 1
            cols[k] += 1
print(f'rows differing: {len(rows)}; tokens differing: {tok_diff} of {tok_total}; rows with a changed width: {width_changed}')
print('changed token columns (column: count):', dict(sorted(cols.items())))
print('first rows:', rows[:8], '... last rows:', rows[-3:])
