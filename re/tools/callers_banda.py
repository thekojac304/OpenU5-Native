#!/usr/bin/env python
"""Exhaustive BY-BAND census: every 3-byte E8 rel16 (near call) or E9 rel16 (near
jmp) at EVERY offset whose resolved target is one of the requested kernel
addresses. Over-reports (a literal can look like a call) but never misses one,
which is the safe direction for a universal claim about call sites."""
import os, sys
D = 'original/u5/ultima5'
BASES = {
    'ULTIMA.EXE': 0, 'TOWN.OVL': 0x81d0, 'OUTSUBS.OVL': 0x81d0, 'MAINOUT.OVL': 0x8304,
    'COMSUBS.OVL': 0x85fe, 'NPC.OVL': 0xa290, 'TALK.OVL': 0xa290, 'SHOPPES.OVL': 0xa290,
    'SHOPPES2.OVL': 0xa89e, 'SHOPPES3.OVL': 0xa5f6, 'LOOKOBJ.OVL': 0xa444,
    'DNGLOOK.OVL': 0xa2b6, 'CAST.OVL': 0xa8d8, 'CMDS.OVL': 0xbf80, 'SJOG.OVL': 0xbf80,
    'COMBAT.OVL': 0xbfec, 'DUNGEON.OVL': 0xe1e0, 'BLCKTHRN.OVL': 0xe63e,
    'CAST2.OVL': 0xc29e, 'ENDGAME.OVL': 0xe84c, 'FLAMES.OVL': 0xea94, 'INTRO.OVL': 0xcd3a,
}
targets = {int(a, 16) for a in sys.argv[1:]}
for name, base in BASES.items():
    data = open(os.path.join(D, name), 'rb').read()
    if name.endswith('.EXE'):
        data = data[int.from_bytes(data[8:10], 'little') * 16:]
    for i in range(len(data) - 2):
        if data[i] not in (0xE8, 0xE9):
            continue
        rel = int.from_bytes(data[i + 1:i + 3], 'little', signed=True)
        t = (i + 3 + rel) & 0xFFFF
        if ((t + base) & 0xFFFF) in targets:
            print("%-13s 0x%04x  %s 0x%04x -> kernel 0x%04x"
                  % (name, i, 'call' if data[i] == 0xE8 else 'jmp', t, (t + base) & 0xFFFF))
