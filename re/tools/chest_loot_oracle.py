#!/usr/bin/env python
"""Batch 23 -- independent oracle for the 1988 world-chest loot roll.

Transcribed from the shipped binaries, NOT from the native or TypeScript
ports, so it can serve as the expected-value generator for the native tests:

  kernel  ULTIMA.EXE:0x2092  rand(lo, hi)  (SJOG `call 0x6112` = 0x6112+0xBF80)
          state = (ror3(state + 0x9248) ^ 0x9248) + 0x11
          return lo + (state & 0x7fff) % (hi - lo + 1)
  SJOG.OVL:0x112C open_chest_world   contents = object byte +5; & 0x7f if
          trapped; then loot_fixed(contents), loot_random(contents)
  SJOG.OVL:0x1040 loot_fixed         rows si = 7..0 over DS 0x4124/0x412C/0x4134
  SJOG.OVL:0x10B8 loot_random        (contents/2)+1 draws over DS 0x413C/0x416C
  SJOG.OVL:0x0F88 loot_place         id 3/4: base-1; id 1: rand(1,contents);
                                     id 2: rand(1,3*contents); else base

The four tables are read from original/u5/ultima5/DATA.OVL (fileoff = DS+0x10)
every run, so a transcription error in this file cannot hide a table error.

usage: chest_loot_oracle.py <seed> [contents]      -> one roll, with the call log
       chest_loot_oracle.py --search [contents]     -> seeds covering each category
"""
import os
import sys

ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
DATA = open(os.path.join(ROOT, 'original', 'u5', 'ultima5', 'DATA.OVL'), 'rb').read()


def table(ds, n):
    return list(DATA[ds + 0x10:ds + 0x10 + n])


FIXED_ITEM, FIXED_GUARD, FIXED_MAXQ = table(0x4124, 8), table(0x412C, 8), table(0x4134, 8)
RAND_ITEM, RAND_GUARD = table(0x413C, 48), table(0x416C, 48)


class Rng:
    def __init__(self, seed):
        self.state = seed & 0xFFFF
        self.log = []

    def __call__(self, lo, hi):
        x = (self.state + 0x9248) & 0xFFFF
        x = ((x >> 3) | (x << 13)) & 0xFFFF
        self.state = ((x ^ 0x9248) + 0x11) & 0xFFFF
        v = lo + (self.state & 0x7FFF) % (hi - lo + 1)
        self.log.append((lo, hi, v))
        return v


def open_chest(contents, rand):
    """Returns the pieces in the order loot_place writes them: (id, byte+5)."""
    pieces = []

    def place(item, base):
        if item in (3, 4):
            base -= 1
        if item == 1:
            qty = rand(1, contents)
        elif item == 2:
            qty = rand(1, 3 * contents)
        else:
            qty = base
        pieces.append((item, qty & 0xFF))

    for si in range(7, -1, -1):                      # 0x107a dec si / js
        if FIXED_GUARD[si] > contents:               # 0x1083 ja -> no roll
            continue
        if FIXED_GUARD[si] > rand(1, 30):            # 0x1090 / 0x1099
            continue
        base = 1 if FIXED_MAXQ[si] == 1 else rand(1, FIXED_MAXQ[si])
        place(FIXED_ITEM[si], base)
    for _ in range(contents // 2 + 1):               # 0x1117: count..0
        idx = rand(0, 47)                            # 0x10db
        if RAND_GUARD[idx] > contents:               # 0x10e6 ja -> no roll
            continue
        if RAND_GUARD[idx] > rand(1, 30):            # 0x10f2 / 0x10fb
            continue
        place(RAND_ITEM[idx], idx)                   # base = the table index
    return pieces


def main():
    args = sys.argv[1:]
    if args and args[0] == '--search':
        contents = int(args[1], 0) if len(args) > 1 else 0x1E
        want = {'equipment': {5, 6, 9, 10, 11, 12}, 'potion': {3}, 'scroll': {4},
                'nested-chest': {1}, 'keys': {7}, 'gems': {8}}
        found = {}
        for seed in range(0x10000):
            p = open_chest(contents, Rng(seed))
            ids = {i for i, _ in p}
            for k, s in want.items():
                if k not in found and ids & s and len(p) <= 6:
                    found[k] = (seed, p)
            if len(found) == len(want):
                break
        for k, (seed, p) in found.items():
            print('%-12s seed=0x%04x pieces=%s' % (k, seed, p))
        return
    seed = int(args[0], 0)
    contents = int(args[1], 0) if len(args) > 1 else 0x1E
    r = Rng(seed)
    p = open_chest(contents, r)
    print('seed=0x%04x contents=%d pieces=%s' % (seed, contents, p))
    print('calls=%s' % [(lo, hi) for lo, hi, _ in r.log])
    print('final_state=0x%04x draws=%d' % (r.state, len(r.log)))


if __name__ == '__main__':
    main()
