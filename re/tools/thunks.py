#!/usr/bin/env python
"""Dump the ULTIMA.EXE overlay thunk table: 11-byte records of
`lcall loader` + overlay-id + `ljmp 0:target`."""
import sys
data = open('original/u5/ultima5/ULTIMA.EXE', 'rb').read()
hdr = int.from_bytes(data[8:10], 'little') * 16
img = data[hdr:]
PAT = bytes.fromhex('9aec022e07')
out = {}
i = 0
while True:
    i = img.find(PAT, i)
    if i < 0:
        break
    ovl = img[i + 5]
    if img[i + 7] == 0xEA:
        target = int.from_bytes(img[i + 8:i + 10], 'little')
        out[i] = (ovl, target)
    i += 1
BASES = {}
for addr, (ovl, tgt) in sorted(out.items()):
    BASES.setdefault(ovl, []).append(tgt)
bases = {o: min(t) for o, t in BASES.items()}
if len(sys.argv) > 1 and sys.argv[1] == '--bases':
    for o in sorted(bases):
        print("ovl %2d base 0x%04x  (%d thunks)" % (o, bases[o], len(BASES[o])))
else:
    for addr, (ovl, tgt) in sorted(out.items()):
        print("thunk 0x%04x -> ovl %2d  target 0x%04x  (ovlbase 0x%04x  off 0x%04x)"
              % (addr, ovl, tgt, bases[ovl], tgt - bases[ovl]))
