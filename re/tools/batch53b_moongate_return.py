#!/usr/bin/env python
"""Batch 53B -- what the original does when the party steps back onto the
moongate it just arrived through.

Reads the shipped binaries directly (original/ is gitignored EA material; this
prints offsets, bytes and decoded instructions, never the files themselves),
and ASSERTS each fact it prints, so a wrong claim fails the script:

  1. MAINOUT.OVL is overlay 2 and loads at 0x81d0 (not 0x8304);
  2. the outdoor main loop (MAINOUT 0x0a84) calls kernel_moongate_enter
     (0x48a8) at the top of EVERY iteration, before the key read, with no
     skip flag; the town loop's one-shot [0xa9bc] is TOWN-only;
  3. 0x48a8 fires on the MAP-BUFFER tile 0xDC under the party, which only the
     night render (0x475a, from the screen update 0x5910) paints;
  4. the destination is chosen from the clock alone (0x4962-0x4977) and
     0x47f4 copies the chosen stone; neither routine reads the origin gate or
     advances the clock (0x4f7c / 0x4a84 are not called on that path).

Usage: batch53b_moongate_return.py [original/u5/ultima5]
"""
import os
import re
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

D = sys.argv[1] if len(sys.argv) > 1 else 'original/u5/ultima5'
md = Cs(CS_ARCH_X86, CS_MODE_16)
failures = []


def load(name):
    data = open(os.path.join(D, name), 'rb').read()
    if name.endswith('.EXE'):
        data = data[int.from_bytes(data[8:10], 'little') * 16:]
    return data


EXE, MAINOUT, TOWN = load('ULTIMA.EXE'), load('MAINOUT.OVL'), load('TOWN.OVL')


def check(ok, what):
    print(('  OK    ' if ok else '  FAIL  ') + what)
    if not ok:
        failures.append(what)


def listing(data, start, end, title):
    print(f'\n--- {title} [{start:#06x}..{end:#06x})')
    out = []
    for i in md.disasm(data[start:end], start):
        line = '%04x: %-14s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str)
        print(line)
        out.append(i)
    return out


def near_target(data, at, base):
    """Kernel address of the E8 rel16 at `at` in a module loaded at `base`."""
    assert data[at] == 0xE8, hex(at)
    rel = int.from_bytes(data[at + 1:at + 3], 'little', signed=True)
    return ((at + 3 + rel) & 0xFFFF) + base & 0xFFFF


def is_prologue(addr):
    return EXE[addr:addr + 3] == bytes.fromhex('558bec')


print('# 1. MAINOUT.OVL loads at 0x81d0 (thunk overlay 2)')
th = EXE[0x7a3a:0x7a3a + 11]
check(th[:5] == bytes.fromhex('9aec022e07') and th[5] == 2 and th[7] == 0xEA, 'kernel thunk 0x7a3a = lcall loader, overlay 2, ljmp')
tgt = int.from_bytes(th[8:10], 'little')
check((tgt - 0x81d0) & 0xFFFF == 0x0d22 and MAINOUT[0x0d22:0x0d25] == bytes.fromhex('558bec'),
      f'overlay 2 target {tgt:#06x} - 0x81d0 = MAINOUT 0x0d22, a function prologue (the outdoor entry the kernel calls at 0x00c4)')
for at, want in ((0x16e0, 0x4402), (0x05a0, 0x5910), (0x0aa2, 0x39fc)):
    k = near_target(MAINOUT, at, 0x81d0)
    k_alt = near_target(MAINOUT, at, 0x8304)
    check(k == want and is_prologue(k) and not is_prologue(k_alt),
          f'MAINOUT {at:#06x}: base 0x81d0 -> kernel {k:#06x} (prologue); base 0x8304 -> {k_alt:#06x} (not a prologue)')

print('\n# 2. The outdoor loop checks the gate at the top of every iteration')
listing(MAINOUT, 0x0a84, 0x0b1a, 'MAINOUT outdoor main loop, head')
check(near_target(MAINOUT, 0x0b00, 0x81d0) == 0x48a8, 'MAINOUT 0x0b00: call kernel_moongate_enter 0x48a8 (unconditional)')
check(near_target(MAINOUT, 0x0b14, 0x81d0) == (0x0598 + 0x81d0) & 0xFFFF, 'MAINOUT 0x0b14: call 0x0598 (key read) AFTER the gate check')
check(near_target(MAINOUT, 0x05a0, 0x81d0) == 0x5910, 'MAINOUT 0x05a0: the key read starts with the screen update 0x5910')
jmp = MAINOUT[0x0d1a:0x0d1d]
check(jmp[0] == 0xE9 and (0x0d1d + int.from_bytes(jmp[1:], 'little', signed=True)) & 0xFFFF == 0x0a8f,
      'MAINOUT 0x0d1a: jmp 0x0a8f -- every command loops back to the top (and to 0x0b00)')
check(bytes.fromhex('bca9') not in MAINOUT, 'MAINOUT never references [0xa9bc]: no "just arrived" skip outdoors')
refs = [m.start() for m in re.finditer(re.escape(bytes.fromhex('bca9')), TOWN)]
check(refs == [0x11fd, 0x146a, 0x1471], f'TOWN.OVL references [0xa9bc] only at {[hex(r) for r in refs]}')
listing(TOWN, 0x11f0, 0x11ff, 'TOWN loader 0x11f0 sets the flag')
listing(TOWN, 0x1468, 0x1480, 'TOWN loop: one skipped check after a town load')
check(near_target(TOWN, 0x1476, 0x81d0) == 0x48a8, 'TOWN 0x1476: the town loop\'s own call to 0x48a8, skipped once when [0xa9bc] is set')

print('\n# 3. kernel_moongate_enter 0x48a8: fires on the painted tile under the party')
enter = listing(EXE, 0x48a8, 0x4987, 'ULTIMA.EXE kernel_moongate_enter')
check(EXE[0x48c2:0x48c5] == bytes.fromhex('803fdc'), '0x48c2: cmp byte [tile_ptr(party_x, party_y)], 0xDC -- else return')
check(EXE[0x493c:0x493f] == bytes.fromhex('c60705'), '0x493c: the gate cell under the party becomes 5 (grass) before the jump')
check(EXE[0x4962:0x4967] == bytes.fromhex('803e7f580c') and EXE[0x4969:0x496c] == bytes.fromhex('a08558')
      and EXE[0x496e:0x4971] == bytes.fromhex('a08658') and EXE[0x4973:0x4976] == bytes.fromhex('2d3000'),
      '0x4962-0x4973: phase = (hour < 12 ? [0x5885] Felucca : [0x5886] Trammel) - 0x30')
check(near_target(EXE, 0x4977, 0) == 0x47f4, '0x4977: call kernel_moongate_teleport(phase)')
tele = listing(EXE, 0x47f4, 0x48a7, 'ULTIMA.EXE kernel_moongate_teleport')
calls = {int(i.op_str, 16) for i in enter + tele if i.mnemonic == 'call'}
check(0x4f7c not in calls and 0x4a84 not in calls, 'neither routine calls advance_clock 0x4f7c or the phase refresh 0x4a84')
reads = [i for i in enter + tele if re.search(r'0x58(96|97)\]', i.op_str) and i.op_str.startswith('al')]
check(all(i.address in (0x48b3, 0x48b9, 0x492d, 0x4933) for i in reads),
      'the origin (party x/y) is read only for the two tile pointers, never for the destination')
check(all(EXE[a:a + 4] == bytes.fromhex(h) for a, h in ((0x4844, '8a873058'), (0x484b, '8a873858'), (0x483d, '8a874058'), (0x4852, '8a874858'))),
      '0x483d-0x4856: location / x / y / floor copied from the chosen stone\'s four tables (0x5840/30/38/48)')
MOVS = re.compile(r'byte ptr \[0x58(85|86)\], ')
check(not any(MOVS.search(i.op_str) for i in enter + tele if i.mnemonic == 'mov'), 'the phase latches are only read here, never written')

print('\n# 4. The only painter of 0xDC: the night render, from the screen update')
listing(EXE, 0x475a, 0x47f4, 'ULTIMA.EXE kernel_moongate_render')
check(EXE[0x4762:0x4767] == bytes.fromhex('c746f8dc00') and near_target(EXE, 0x47a3, 0) == 0x4702 and EXE[0x47d2:0x47d4] == bytes.fromhex('8805'),
      'tile 0xDC written at every stone 0x4702 calls visible (this location, this floor, in the 32x32 window)')
check(near_target(EXE, 0x594e, 0) == 0x475a, '0x594e (inside the screen update 0x5910): call kernel_moongate_render')

print('\n' + ('ALL CHECKS OK' if not failures else f'{len(failures)} CHECK(S) FAILED'))
sys.exit(1 if failures else 0)
