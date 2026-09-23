#!/usr/bin/env python
"""Independent oracle for the NPC half of TOWN.OVL:0x1694 town_populate_npcs.

Reads a location's .NPC record straight out of original/u5/ultima5 and prints,
for every slot 1..31 with a non-zero type byte, where 0x1694 puts it at a given
hour: schedule_index is transcribed from NPC.OVL:0x12E0 (reached from TOWN
0x16d7 through kernel thunk 0x7B36), and 0x1841-0x1856 copy the chosen
period's X/Y/Z (schedule +3/+6/+9) into the live record, set state=1,
servedSlot=index (0x1705) and pathIdx=-1 (0x170c).

Record selection is NPC.OVL:0x0000: file by (loc-1)>>3, record by (loc-1)&7,
0x240 bytes per record: 32 x 16-byte schedules, 32 type bytes, 32 dialog bytes.
Schedule layout: ai[3] x[3] y[3] z[3] times[4].

usage: npc_schedule_oracle.py <location> <hour>
"""
import os, sys

FILES = ['TOWNE.NPC', 'DWELLING.NPC', 'CASTLE.NPC', 'KEEP.NPC']


def schedule_index(times, hour):
    # NPC.OVL:0x12E0 -- byte arithmetic, strict '>' comparisons (jbe keeps).
    d = [(hour - t) & 0xFF for t in times]
    best, idx = d[0], 0
    if best > d[1]:
        best, idx = d[1], 1
    if best > d[2]:
        best, idx = d[2], 2
    if best > d[3]:
        idx = 1          # 0x131e: the fourth time is period 1 again; best is not updated
    return idx


def main():
    loc, hour = int(sys.argv[1]), int(sys.argv[2])
    path = os.path.join('original', 'u5', 'ultima5', FILES[(loc - 1) >> 3])
    data = open(path, 'rb').read()
    rec = data[((loc - 1) & 7) * 0x240:][:0x240]
    types, dialogs = rec[0x200:0x220], rec[0x220:0x240]
    for slot in range(1, 32):
        if not types[slot]:
            continue
        s = rec[slot * 16:slot * 16 + 16]
        ai, xs, ys, zs, times = s[0:3], s[3:6], s[6:9], s[9:12], s[12:16]
        i = schedule_index(times, hour)
        z = zs[i] - 256 if zs[i] > 127 else zs[i]
        print(f"slot {slot:2d} type 0x{types[slot]:02x} dialog {dialogs[slot]:3d} ai {list(ai)} "
              f"times {list(times)} xyz {list(xs)}/{list(ys)}/{[v - 256 if v > 127 else v for v in zs]} "
              f"-> idx {i} at ({xs[i]},{ys[i]},{z}) ai {ai[i]}")


if __name__ == '__main__':
    main()
