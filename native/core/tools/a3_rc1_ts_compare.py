"""Alpha 3 RC1: compare the whole game/ vitest FAIL set against the saved A3-HF10 baseline.

Usage: python a3_rc1_ts_compare.py <vitest-json-report> <baseline-fails.txt> <out-fails.txt>
Compares the SET of failing tests ("<file> :: <full name>"), never the totals.

The saved baseline (a3-hf10-ts-base-fails.txt) lists assertion-level failures only. Suites that
fail to LOAD (missing git-ignored data, an unparsable file) have no assertions; they are listed
separately as "<file> :: <FILE-LEVEL FAILURE>" so they cannot hide, but they are not part of the
baseline comparison: the caller proves game/ unchanged since the baseline instead."""
import json
import sys

report, baseline, out = sys.argv[1:4]
r = json.load(open(report, encoding='utf-8'))
print({k: r[k] for k in ('numTotalTests', 'numPassedTests', 'numFailedTests', 'numPendingTests', 'numTodoTests')})
import os
root = os.path.abspath('game').replace(os.sep, '/')  # run from the repository root
fails, load_fails = [], []
for f in r['testResults']:
    n = f['name'].replace('\\', '/')
    rel = n[len(root):] if n.startswith(root) else n
    failed = [a for a in f['assertionResults'] if a['status'] == 'failed']
    for a in failed:
        fails.append(rel + ' :: ' + a['fullName'])
    if f['status'] == 'failed' and not failed:
        load_fails.append(rel + ' :: <FILE-LEVEL FAILURE> ' + f.get('message', '')[:60].replace('\n', ' '))
open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(sorted(fails)) + '\n')
base = [l.rstrip('\r\n') for l in open(baseline, encoding='utf-8') if l.strip()]
bs, fs = set(base), set(fails)
print('assertion-level fails: RC1', len(fails), '| baseline', len(base))
print('only in baseline', sorted(bs - fs))
print('only in RC1', sorted(fs - bs))
print('assertion-level FAIL set:', 'IDENTICAL' if bs == fs and len(fails) == len(base) else 'DIFFERENT')
print('suites that fail to load (%d, not in the saved baseline):' % len(load_fails))
for x in sorted(load_fails):
    print('  ' + x)
