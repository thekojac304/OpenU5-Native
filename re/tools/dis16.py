#!/usr/bin/env python
"""Minimal 16-bit disassembler front-end over the shipped 1988 binaries.

Usage: dis16.py <FILE> <start-hex> [count] [--exe]
Offsets are FILE offsets for .OVL; for ULTIMA.EXE pass --exe to skip the MZ header
so the offsets match the CS:IP addresses used across re/notes.
"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

path, start = sys.argv[1], int(sys.argv[2], 16)
count = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith('-') else 120
data = open(path, 'rb').read()
base = 0
if '--exe' in sys.argv:
    hdr = int.from_bytes(data[8:10], 'little') * 16   # header paragraphs
    data = data[hdr:]
md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = False
n = 0
for i in md.disasm(data[start:start + count * 8], start):
    print("%04x: %-20s %s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
    n += 1
    if n >= count:
        break
