#!/usr/bin/env python
"""Alpha 3 A3-01 -- every PC-speaker primitive call site, straight from the shipped binaries.

Usage: a3_01_sound_census.py <original/u5/ultima5 dir> <out-log>

Counts and lists every near call to the six speaker primitives of ULTIMA.EXE
(tone_sweep 0x2192, noise_burst 0x223c, beep 0x22c0, set_tone 0x22e2,
stop 0x230e, glide 0x43ae) in the kernel and in every overlay, with the
immediates pushed before each call (a0 = [bp+4], the LAST push).

Mechanics are Batch 51's (re/tools/batch51_timing_census.py): linear sweep that
resynchronises on undecodable bytes, overlay near calls resolved as
(printed + base) & 0xFFFF, the base chosen as the candidate that resolves the
most calls onto known kernel entry points (print_ds, putchar, compositor,
rand and the primitives).

Positive control, printed at the end: re/notes/sfx-catalog.md section 2 counts
44 tone_sweep / 25 noise_burst / 27 glide / 8 set_tone / 5 stop / 2 beep call
sites (about 109) in the 1988 binaries. The patched Exodus set in this tree
must land on the same order of magnitude; a wrong base would not.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import batch51_timing_census as b51  # noqa: E402

SPEAKER = {0x2192: 'TONE_SWEEP', 0x223c: 'NOISE_BURST', 0x22c0: 'BEEP', 0x22e2: 'SET_TONE',
           0x230e: 'SPK_STOP', 0x43ae: 'GLIDE'}
KNOWN = set(SPEAKER) | {0x1850, 0x16ba, 0x5910, 0x2092, 0x20fa, 0x20c8, 0x3ae6, 0x266c}
CATALOG = {'TONE_SWEEP': 44, 'NOISE_BURST': 25, 'GLIDE': 27, 'SET_TONE': 8, 'SPK_STOP': 5, 'BEEP': 2}


def census(name, ins, base, f, totals, per_file):
    for k, c in enumerate(ins):
        if c.mnemonic != 'call' or not c.op_str.startswith('0x'):
            continue
        t = b51.target(c, base)
        if t in SPEAKER:
            prim = SPEAKER[t]
            totals[prim] = totals.get(prim, 0) + 1
            per_file.setdefault(name, {}).setdefault(prim, 0)
            per_file[name][prim] += 1
            f.write('  %-12s %04x %-11s a0..=%s\n' % (name, c.address, prim, ','.join(b51.pushed_args(ins, k))))


def main():
    src, out = sys.argv[1], sys.argv[2]
    totals, per_file = {}, {}
    with open(out, 'w', newline='\n') as f:
        f.write('# A3-01 PC-speaker primitive census (a0 = [bp+4] ... a4 = [bp+0xc])\n')
        f.write('# source: %s\n' % src.replace('\\', '/'))
        f.write('== ULTIMA.EXE kernel (direct calls)\n')
        census('ULTIMA.EXE', b51.sweep(b51.exe_image(os.path.join(src, 'ULTIMA.EXE'))), 0, f, totals, per_file)
        for path in sorted(glob.glob(os.path.join(src, '*.OVL'))):
            data = open(path, 'rb').read()
            ins = b51.sweep(data)
            calls = [i for i in ins if i.mnemonic == 'call' and i.op_str.startswith('0x')]
            best = max(b51.BASES, key=lambda b: sum(1 for c in calls if b51.target(c, b) in KNOWN))
            hits = sum(1 for c in calls if b51.target(c, best) in KNOWN)
            name = os.path.basename(path)
            f.write('== %s base=%04x known_hits=%d\n' % (name, best, hits))
            census(name, ins, best, f, totals, per_file)
        f.write('\n== per file\n')
        for name in sorted(per_file):
            f.write('  %-12s %s\n' % (name, ' '.join('%s=%d' % kv for kv in sorted(per_file[name].items()))))
        f.write('\n== totals (this tree)   vs   re/notes/sfx-catalog.md section 2 (1988 corpus)\n')
        for prim in ['TONE_SWEEP', 'NOISE_BURST', 'GLIDE', 'SET_TONE', 'SPK_STOP', 'BEEP']:
            f.write('  %-11s %4d   vs %4d\n' % (prim, totals.get(prim, 0), CATALOG[prim]))
        f.write('  %-11s %4d   vs %4d\n' % ('ALL', sum(totals.values()), sum(CATALOG.values())))


if __name__ == '__main__':
    main()
