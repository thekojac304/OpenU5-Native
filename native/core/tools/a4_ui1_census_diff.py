"""Alpha 4 UI Batch 1: compare the A3-04F render census (B1..Bn rows) of two runs.

    python a4_ui1_census_diff.py <before.log> <after.log>

Prints, per scenario, per-frame transactions / windows / pixels / modelled
transfer time before -> after, and the census checks (R/T/P/C) of both runs.
"""
import re, sys

ROW = re.compile(r'^\s+(B\d+[a-z]?) (.*?)\s+frames\s+(\d+) \| per frame: txns\s+([\d.]+) .*?windows\s+([\d.]+) px\s+(\d+) xfer\s+([\d.]+) ms')


def load(path):
    rows, checks = {}, {}
    for line in open(path, encoding='utf8', errors='replace'):
        m = ROW.match(line)
        if m:
            rows[m.group(1)] = (m.group(2).strip(), int(m.group(3)), float(m.group(4)), float(m.group(5)), int(m.group(6)), float(m.group(7)))
        m = re.match(r'^(GREEN|RED) ([A-Z]\d+[a-z]?) ', line)
        if m:
            checks[m.group(2)] = m.group(1)
    return rows, checks


before, cb = load(sys.argv[1])
after, ca = load(sys.argv[2])
print(f'{"scenario":42} {"frames":>7} {"txns/frame":>17} {"windows":>13} {"pixels/frame":>17} {"xfer ms/frame":>15}')
for key in before:
    if key not in after:
        continue
    b, a = before[key], after[key]
    same = b[1:] == a[1:]
    print(f'{key+" "+b[0][:38]:42} {b[1]:>3}->{a[1]:<3} {b[2]:>7.1f}->{a[2]:<8.1f} {b[3]:>5.1f}->{a[3]:<6.1f} '
          f'{b[4]:>7}->{a[4]:<8} {b[5]:>6.2f}->{a[5]:<7.2f}{"" if same else "  *"}')
print('\ncensus checks before -> after:')
for key in sorted(set(cb) | set(ca), key=lambda k: (k[0], int(re.sub(r"\D", "", k) or 0))):
    print(f'  {key}: {cb.get(key, "-")} -> {ca.get(key, "-")}')
