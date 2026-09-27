#!/usr/bin/env python3
"""A3-04F (ALPHA3_AUDIO.md section 26): the rasterizer's per-frame work, read
from the linked firmware image.

The host cannot time Xtensa code, so this reads the loops the device actually
runs. For each routine it prints every loop (a backward conditional branch to a
target inside the function) with its body: the instructions reachable from the
loop's head along real control flow up to the branch. objdump decodes linearly
and reads alignment padding as code, so every branch target is re-decoded as
its own stream and the walk follows jumps.

For the viewport CRC it checks the property A3-04F changed:

  C1  crc32_u16le has no per-bit loop -- no loop whose body is the reflected
      CRC-32 bit step (shift right 1, isolate bit 0, negate, AND with the
      0xedb88320 polynomial) -- i.e. it is table-driven.
  C2  its innermost loop body is at most 24 instructions.

The per-frame figures are loop bodies x trip counts for a 176 x 176 viewport
(30,976 pixels, 61,952 bytes, 495,616 bits): a lower bound at one instruction
per cycle, which branch penalties, load-use stalls and PSRAM misses only raise.

    python a3_04f_image_check.py <openu5_tdeck.elf>      exit 0 = C1 and C2 hold
"""
import os
import re
import subprocess
import sys

OBJDUMPS = [
    r'C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe',
    os.path.expanduser(r'~/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe'),
]
PIXELS, BYTES, BITS = 176 * 176, 176 * 176 * 2, 176 * 176 * 16
INSN = re.compile(r'^\s*([0-9a-f]{8}):\s+(\S+)\s*(.*)$')
BRANCH = re.compile(r'^(b\w+|j)$')
ADDRESS = re.compile(r'\b([0-9a-f]{8})\b')


def objdump():
    tool = next((p for p in OBJDUMPS if os.path.exists(p)), None)
    if not tool:
        sys.exit('objdump not found')
    return tool


def symbols(elf):
    nm = objdump().replace('objdump.exe', 'nm.exe')
    out = subprocess.run([nm, '-S', elf], capture_output=True, text=True, check=True).stdout
    table = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4:
            table[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    return table


def disassemble(elf, start, stop):
    out = subprocess.run([objdump(), '-d', '--no-show-raw-insn', f'--start-address={start:#x}',
                          f'--stop-address={stop:#x}', elf], capture_output=True, text=True, check=True).stdout
    insns = []
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            insns.append((int(m[1], 16), m[2], m[3]))
    return insns


def target_of(args):
    m = ADDRESS.search(args.split('<')[0])
    return int(m[1], 16) if m else None


def decode_map(elf, start, size):
    """address -> (address, op, args, next address), each from the nearest decode stream seeded at or below it."""
    end = start + size
    seeds = {start}
    for _, op, args in disassemble(elf, start, end):
        if BRANCH.match(op):
            t = target_of(args)
            if t is not None and start <= t < end:
                seeds.add(t)
    best = {}
    for seed in sorted(seeds):
        stream = disassemble(elf, seed, end)
        for i, (a, op, args) in enumerate(stream):
            nxt = stream[i + 1][0] if i + 1 < len(stream) else end
            if a not in best or best[a][0] <= seed:
                best[a] = (seed, (a, op, args, nxt))
    return {a: v[1] for a, v in best.items()}, seeds


def loop_body(dmap, head, branch):
    """The instructions reachable from `head` along control flow without leaving [head, branch]."""
    seen, work = set(), [head]
    while work:
        a = work.pop()
        if a is None or a in seen or a not in dmap or not head <= a <= branch:
            continue
        seen.add(a)
        _, op, args, nxt = dmap[a]
        if op == 'j':
            work.append(target_of(args))
            continue
        if a != branch:
            work.append(nxt)
        if BRANCH.match(op):
            work.append(target_of(args))
    return [dmap[a][:3] for a in sorted(seen)]


def loops(dmap, start, seeds):
    found = []
    for addr in sorted(dmap):
        _, op, args, _ = dmap[addr]
        if not BRANCH.match(op) or op == 'j':
            continue
        head = target_of(args)
        if head is not None and head in seeds and start <= head < addr:
            body = loop_body(dmap, head, addr)
            if body and body[-1][0] == addr:
                found.append((head, addr, op, args.split('<')[0].strip(), body))
    return found


def is_bit_step(body):
    ops = [op for _, op, _ in body]
    text = ' '.join(f'{op} {args}' for _, op, args in body)
    return ('srli' in ops and 'neg' in ops and 'and' in ops and re.search(r'extui\s+\w+, \w+, 0, 1\b', text) is not None
            and 'edb88320' in text)


def main(elf):
    syms = symbols(elf)
    crc = next((k for k in syms if 'crc32_u16le' in k), None)
    if not crc:
        sys.exit('crc32_u16le not found (is this an OpenU5 T-Deck ELF?)')
    print(f'image: {elf}')
    ok = True
    names = [crc] + sorted(k for k in syms if k.startswith('_ZN6openu515render_snapshot') or
                           ('expand_tile' in k and 'openu5' in k))
    for name in names:
        start, size = syms[name]
        dmap, seeds = decode_map(elf, start, size)
        found = loops(dmap, start, seeds)
        print(f'\n{name}  @ {start:#x}, {size} bytes')
        for head, addr, op, args, body in found:
            tag = '  <- per-bit CRC step' if is_bit_step(body) else ''
            print(f'  loop {head:#x}..{addr:#x} ({op} {args}): body {len(body)} instructions{tag}')
        if name != crc:
            continue
        innermost = min(found, key=lambda l: len(l[4])) if found else None
        bit = innermost is not None and is_bit_step(innermost[4]) and len(innermost[4]) <= 12
        if innermost:
            print('  innermost loop body:')
            for a, op, args in innermost[4]:
                print(f'    {a:#x}: {op} {args.split("<")[0].strip()}')
        if bit:
            body = len(innermost[4])
            print(f'  per frame (176x176): the per-bit body {body} x {BITS:,} bits = {body * BITS:,} instructions '
                  f'= {body * BITS / 240e3:.1f} ms at 240 MHz, 1 instruction/cycle, before the per-byte and per-pixel work')
        elif innermost:
            body = len(innermost[4])
            print(f'  per frame (176x176): the innermost body {body} x {PIXELS:,} pixels = {body * PIXELS:,} instructions '
                  f'= {body * PIXELS / 240e3:.1f} ms at 240 MHz, 1 instruction/cycle')
        c1 = not any(is_bit_step(l[4]) and len(l[4]) <= 12 for l in found)
        c2 = innermost is not None and len(innermost[4]) <= 24
        print(f'  C1 no per-bit loop (table-driven): {"GREEN" if c1 else "RED"}')
        print(f'  C2 innermost loop body <= 24 instructions: {"GREEN" if c2 else "RED"} '
              f'({len(innermost[4]) if innermost else "no loop"})')
        ok = ok and c1 and c2
    print('\nA3-04F image check:', 'GREEN' if ok else 'RED')
    return 0 if ok else 1


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
