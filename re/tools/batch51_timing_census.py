#!/usr/bin/env python
"""Batch 51 -- scripted-scene timing evidence straight from the shipped binaries.

Usage: batch51_timing_census.py <original/u5/ultima5 dir> <out-dir>

Writes three logs:
  batch51-original-primitives.log   the kernel timing primitives' bodies
  batch51-original-scene-census.log every call to a timing / sound / input
                                    primitive in every overlay and the kernel,
                                    with the immediates pushed before it
  batch51-original-scene-disasm.log the staged scenes' routines, calls resolved

Overlay near calls resolve as (printed + base) & 0xFFFF. The base of each
overlay is chosen as the candidate that resolves the MOST calls onto known
kernel entry points (print_ds 0x1850, putchar 0x16ba, the compositor 0x5910,
rand 0x2092 and the primitives below) -- a positive control that a wrong base
cannot pass by accident at these hit counts.
"""
import glob
import os
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

PRIM = {0x20fa: 'DELAY_TICKS(n)', 0x20c8: 'DELAY_CAL', 0x2192: 'TONE_SWEEP', 0x223c: 'NOISE_BURST',
        0x3ae6: 'RUN_N_FRAMES(n)', 0x266c: 'GETKEY', 0x1068: 'FIZZLE_IN', 0x0f46: 'RECT_DISSOLVE',
        0x0b86: 'RECT_XOR', 0x1b24: 'KBD_FLUSH'}
NAMES = dict(PRIM)
NAMES.update({0x5910: 'render/compositor', 0x1850: 'print_ds', 0x16ba: 'putchar', 0x0a70: 'set_color',
              0x2900: 'status_redraw', 0x1b38: 'poll_key', 0x1d5e: 'kbhit', 0x4f7c: 'advance_clock',
              0x2092: 'rand', 0x1112: 'blit_partial', 0x10e0: 'blit_tile', 0x3a74: 'write_object_record',
              0x256e: 'load_record', 0x3522: 'explosion_fx_at_cell'})
KNOWN = set(PRIM) | {0x1850, 0x16ba, 0x5910, 0x2092}
BASES = [0x81d0, 0x8304, 0x85fe, 0xa290, 0xa89e, 0xa5f6, 0xa444, 0xa2b6, 0xa8d8, 0xbfec, 0xbf80, 0xcd3a,
         0xc29e, 0xe1e0, 0xe63e, 0xe84c, 0xea94]
md = Cs(CS_ARCH_X86, CS_MODE_16)


def sweep(data):
    """Linear sweep that resynchronises on undecodable bytes."""
    out, off = [], 0
    while off < len(data):
        got = False
        for i in md.disasm(data[off:], off):
            got = True
            out.append(i)
            off = i.address + i.size
        if not got:
            off += 1
    return out


def exe_image(path):
    data = open(path, 'rb').read()
    return data[int.from_bytes(data[8:10], 'little') * 16:]


def target(i, base):
    return (int(i.op_str, 16) + base) & 0xffff


def pushed_args(ins, k):
    args, j = [], k - 1
    while j >= 0 and len(args) < 5 and k - j < 14:
        p = ins[j]
        if p.mnemonic == 'push':
            q = ins[j - 1] if j > 0 else None
            if q is not None and q.mnemonic == 'mov' and q.op_str.startswith('ax, ') and p.op_str == 'ax':
                args.append(q.op_str[4:])
            else:
                args.append(p.op_str)
        elif p.mnemonic == 'call':
            break
        j -= 1
    return args  # a0 first: the last push is [bp+4]


def census(name, ins, base, f):
    for k, c in enumerate(ins):
        if c.mnemonic != 'call' or not c.op_str.startswith('0x'):
            continue
        t = target(c, base)
        if t in PRIM:
            f.write('  %-12s %04x %-15s a0..=%s\n' % (name, c.address, PRIM[t], ','.join(pushed_args(ins, k))))


def disasm(data, base, start, end, f, title):
    f.write('\n==== %s  [%04x, %04x)  near-call base %04x\n' % (title, start, end, base))
    for i in md.disasm(data[start:end], start):
        note = ''
        if i.mnemonic == 'call' and i.op_str.startswith('0x'):
            t = target(i, base)
            note = '  -> k%04x %s' % (t, NAMES.get(t, ''))
        f.write('%04x: %-18s %s %s%s\n' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str, note))


def main():
    src, out = sys.argv[1], sys.argv[2]
    exe = exe_image(os.path.join(src, 'ULTIMA.EXE'))
    bases = {}
    with open(os.path.join(out, 'batch51-original-scene-census.log'), 'w') as f:
        f.write('# Batch 51 timing-primitive census (a0 = [bp+4] ... a4 = [bp+0xc])\n')
        f.write('== ULTIMA.EXE kernel (direct calls)\n')
        census('ULTIMA.EXE', sweep(exe), 0, f)
        for path in sorted(glob.glob(os.path.join(src, '*.OVL'))):
            data = open(path, 'rb').read()
            ins = sweep(data)
            calls = [i for i in ins if i.mnemonic == 'call' and i.op_str.startswith('0x')]
            best = max(BASES, key=lambda b: sum(1 for c in calls if target(c, b) in KNOWN))
            hits = sum(1 for c in calls if target(c, best) in KNOWN)
            name = os.path.basename(path)
            bases[name] = best
            f.write('== %s base=%04x known_hits=%d\n' % (name, best, hits))
            census(name, ins, best, f)
    with open(os.path.join(out, 'batch51-original-primitives.log'), 'w') as f:
        f.write('# Batch 51 kernel timing primitives (ULTIMA.EXE, offsets after the MZ header)\n')
        for start, end, title in [(0x11b4, 0x1226, 'boot calibration of [0x5356] + its INT 1Ch handler'),
                                  (0x1068, 0x10e0, 'fx_tile_fizzle_in -- 256 blits, 31 compositor calls, NO timer'),
                                  (0x1b24, 0x1b94, 'keyboard flush 0x1b24 and poll_key_blink_cursor 0x1b38'),
                                  (0x1d5e, 0x1d86, 'kbhit 0x1d5e (flush only after a read, [0x538c] gated)'),
                                  (0x20fa, 0x2167, 'delay(n): INT 1Ch tick counter'),
                                  (0x2192, 0x223b, 'tone_sweep: a2 samples x calibrated inner delay; mute branch 0x21c4'),
                                  (0x266c, 0x26a3, 'getkey_with_redraw: no flush before the poll'),
                                  (0x3ae6, 0x3b1b, 'run_n_frames(n): n x (compositor + delay(1))'),
                                  (0x48ca, 0x492d, 'moongate transit: 15 stages x delay(2)')]:
            disasm(exe, 0, start, end, f, title)
    with open(os.path.join(out, 'batch51-original-scene-disasm.log'), 'w') as f:
        f.write('# Batch 51 staged scenes, calls resolved through each overlay base\n')
        for ovl, start, end, title in [
                ('OUTSUBS.OVL', 0x0658, 0x099b, 'Camp apparition camp_results'),
                ('BLCKTHRN.OVL', 0x03c4, 0x0440, 'Blackthorn sacrifice_member: pause(10) + mirrored siren'),
                ('BLCKTHRN.OVL', 0x07cc, 0x08e8, 'Blackthorn entry: guards, 0x3702, materialize sweep, circle, fizzle'),
                ('BLCKTHRN.OVL', 0x0910, 0x0c20, 'party_refuge'),
                ('MAINOUT.OVL', 0x1c00, 0x1caa, 'TrollSneak'),
                ('CMDS.OVL', 0x0140, 0x0250, 'Camp sleep loop and bard lute run_n_frames(0x34)'),
                ('CAST2.OVL', 0x0bc0, 0x0e80, 'shrine donation / WELL DONE / Codex ceremony')]:
            data = open(os.path.join(src, ovl), 'rb').read()
            disasm(data, bases[ovl], start, end, f, '%s %s' % (ovl, title))


if __name__ == '__main__':
    main()
