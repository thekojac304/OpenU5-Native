"""A3-02 -- print the pushed arguments of every PC-speaker call site the A3-02
cue set is built from, straight from the shipped binaries.

    python re/tools/a3_02_cue_sites.py [original/u5/ultima5]

For each site it disassembles a short window that ends at the call and lists
the instructions, so the push order can be read against the primitive's C
signature (arguments are pushed last-to-first; cdecl a0 is the last push):

    tone_sweep 0x2192 (a0 bx step, a1 bx start, a2 count, a3 delay, a4 inc)
    noise_burst 0x223c (a0 band, a1 dur, a2 step)
    beep 0x22c0 (a0 dur, a1 freq)
    glide 0x43ae (a0 total, a1 step, a2 end, a3 start)
    delay 0x20c8 (a0 count, a1 shift index)

Overlay near calls resolve through the overlay base (re/tools/thunks.py,
memory note re-disasm-does-not-exist): target = (rel + base) & 0xffff.
"""
import os
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

ROOT = sys.argv[1] if len(sys.argv) > 1 else "original/u5/ultima5"
# The base a kernel call from each overlay resolves through (checked here by
# every site landing on a speaker primitive; DUNGEON's kernel calls use 0x81D0
# and OUTSUBS's 0xA290, not the object-call bases of the memory note).
BASES = {"ULTIMA.EXE": 0, "TOWN.OVL": 0x81D0, "OUTSUBS.OVL": 0xA290, "MAINOUT.OVL": 0x81D0,
         "CMDS.OVL": 0xBF80, "SJOG.OVL": 0xBF80, "DUNGEON.OVL": 0x81D0, "BLCKTHRN.OVL": 0xA290}
PRIMS = {0x2192: "tone_sweep", 0x223c: "noise_burst", 0x22c0: "beep", 0x43ae: "glide",
         0x20c8: "delay", 0x22e2: "set_tone"}

# (file, call offset, what the port calls it)
SITES = [
    ("MAINOUT.OVL", 0x0344, "move-blocked (overworld wall bump)"),
    ("TOWN.OVL", 0x0849, "move-blocked (town wall bump)"),
    ("ULTIMA.EXE", 0x434a, "move-step burst 1"),
    ("ULTIMA.EXE", 0x4355, "move-step gap"),
    ("ULTIMA.EXE", 0x4364, "move-step burst 2"),
    ("ULTIMA.EXE", 0x35c9, "combat-hit-heavy (target is a party member)"),
    ("ULTIMA.EXE", 0x35de, "combat-hit (target is an enemy)"),
    ("ULTIMA.EXE", 0x2a68, "combat-damage (party_member_take_damage)"),
    ("ULTIMA.EXE", 0x2fe3, "combat-defeat / dungeon-trap (0x2fd0 dispatcher)"),
    ("DUNGEON.OVL", 0x04b9, "dungeon-zap"),
    ("DUNGEON.OVL", 0x099e, "field-afflict"),
    ("DUNGEON.OVL", 0x1cfb, "dungeon-fail"),
    ("SJOG.OVL", 0x1a21, "torch-borrowed"),
    ("CMDS.OVL", 0x09d5, "cannon-fire"),
    ("OUTSUBS.OVL", 0x0492, "waterfall-fall"),
    ("TOWN.OVL", 0x0a75, "mirror-break (loop body, band = si)"),
    ("OUTSUBS.OVL", 0x067b, "apparition-materialize"),
    ("OUTSUBS.OVL", 0x0698, "apparition-arpeggio (per note)"),
    ("OUTSUBS.OVL", 0x0896, "apparition-heal-chime"),
    ("OUTSUBS.OVL", 0x08c1, "apparition-chord"),
    ("BLCKTHRN.OVL", 0x083f, "blackthorn-materialize"),
    ("TOWN.OVL", 0x0e6d, "instrument-note"),
]


def load(name):
    for f in os.listdir(ROOT):
        if f.upper() == name:
            data = open(os.path.join(ROOT, f), "rb").read()
            if name == "ULTIMA.EXE":
                data = data[int.from_bytes(data[8:10], "little") * 16:]
            return data
    return None


def main():
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    for name, call, label in SITES:
        data = load(name)
        if data is None:
            print(f"{name}: missing")
            continue
        # A linear sweep can desync on data; try window starts until one
        # decodes an instruction that begins exactly at `call`.
        tail = []
        for back in range(0x2c, 0x10, -1):
            start = max(0, call - back)
            insns = [i for i in md.disasm(data[start:call + 3], start)]
            tail = [i for i in insns if i.address <= call]
            if tail and tail[-1].address == call:
                break
        target = ""
        if tail and tail[-1].address == call and tail[-1].mnemonic == "call":
            rel = int(tail[-1].op_str, 16)
            base = BASES.get(name) or 0
            kernel = (rel + base) & 0xFFFF
            target = f" -> kernel 0x{kernel:04x} {PRIMS.get(kernel, '?')}"
        print(f"== {name} 0x{call:04x}  {label}{target}")
        for i in tail[-11:]:
            print(f"   {i.address:04x}: {i.mnemonic} {i.op_str}")


if __name__ == "__main__":
    main()
