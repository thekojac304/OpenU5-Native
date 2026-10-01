#!/usr/bin/env python
"""A4-END1: full annotated listing of the shipped ENDGAME.OVL.

Reads original/u5/ultima5/{ENDGAME.OVL,ULTIMA.EXE,DATA.OVL,ENDMSG.DAT} (never writes them)
and prints every instruction of the overlay with:
  * near calls resolved -- local targets (inside the overlay) by function name, kernel
    targets through the ENDGAME slot: kernel = (printed + 0xA290) & 0xFFFF (the overlay-13
    stub's `ljmp 0xa8d8` lands on endgame_main 0x0648, so the slot is 0xA290);
  * `mov ax, imm` / `push imm` values that are DS offsets of DATA.OVL strings
    (fileoff = DS + 0x10) or of the ENDMSG.DAT load buffer (DS 0xb21e + record offset);
  * the function boundaries named in re/notes/endgame.md.

Usage: python re/tools/a4_end1_endgame_listing.py [game_dir] > listing.txt

The output quotes EA's code bytes and ENDMSG.DAT / DATA.OVL text: keep it local, never
commit it. The derived facts are in re/notes/a4-end1-ending-reconstruction.md.
"""
import hashlib
import sys
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "original/u5/ultima5"
OVL = (GAME / "ENDGAME.OVL").read_bytes()
EXE_RAW = (GAME / "ULTIMA.EXE").read_bytes()
EXE = EXE_RAW[int.from_bytes(EXE_RAW[8:10], "little") * 16:]
DATA = (GAME / "DATA.OVL").read_bytes()
ENDMSG = (GAME / "ENDMSG.DAT").read_bytes()
SLOT = 0xA290
ENDMSG_DS = 0xB21E

LOCAL = {
    0x0000: "story_screens",
    0x023A: "text_accum_char",
    0x028C: "spell_cardinal",
    0x02D6: "spell_ordinal",
    0x0326: "endgame_datestamp",
    0x04FE: "sprite_step_redraw",
    0x0510: "move_sprite_toward",
    0x05A2: "wander_sprite",
    0x0648: "endgame_main",
    0x0AF0: "patch_tail",
    0x0B0B: "patch_poll_exit",
    0x0B1F: "patch_stranded_tail",
}

KERNEL = {
    0x266C: "getkey_with_redraw",
    0x1850: "print_string",
    0x3AE6: "run_n_frames",
    0x1D5E: "key_poll",
    0x0878: "restore_video_mode",
    0x02F4: "exit",
    0x0E03: "sound_driver(dx)",
    0x2192: "tone_sweep",
    0x20FA: "delay",
    0x0F46: "fizzle_rect(fn34 clc)",
}


def cstr(off, limit=160):
    end = off
    while end < len(DATA) and DATA[end] != 0 and end - off < limit:
        end += 1
    return DATA[off:end]


def describe_imm(v):
    notes = []
    if ENDMSG_DS <= v < ENDMSG_DS + len(ENDMSG):
        rel = v - ENDMSG_DS
        end = ENDMSG.find(b"\0", rel)
        notes.append("ENDMSG@0x%03x %r" % (rel, ENDMSG[rel:end][:70]))
    fo = v + 0x10
    if 0x40 <= v and fo < len(DATA):
        s = cstr(fo)
        if len(s) >= 2 and all(32 <= b < 127 or b in (10, 13) for b in s):
            notes.append("DS:%04x %r" % (v, s[:70]))
    return "; ".join(notes)


def main():
    print("source:", GAME)
    print("ENDGAME.OVL %d B sha1 %s" % (len(OVL), hashlib.sha1(OVL).hexdigest()))
    print("ENDMSG.DAT  %d B sha1 %s" % (len(ENDMSG), hashlib.sha1(ENDMSG).hexdigest()))
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    pos = 0
    while pos < len(OVL):
        if pos in LOCAL:
            print("\n======== %04x %s" % (pos, LOCAL[pos]))
        insn = next(md.disasm(OVL[pos:pos + 16], pos), None)
        if insn is None:
            print("%04x: %02x                   db" % (pos, OVL[pos]))
            pos += 1
            continue
        note = ""
        if insn.mnemonic in ("call", "jmp") and insn.op_str.startswith("0x"):
            t = int(insn.op_str, 16) & 0xFFFF
            if t < len(OVL):
                fn = max((k for k in LOCAL if k <= t), default=None)
                note = "local %s" % (LOCAL[t] if t in LOCAL else "%s+0x%x" % (LOCAL.get(fn, "?"), t - (fn or 0)))
            else:
                k = (t + SLOT) & 0xFFFF
                note = "kernel 0x%04x %s" % (k, KERNEL.get(k, ""))
        elif insn.mnemonic in ("mov", "push") and "0x" in insn.op_str and "[" not in insn.op_str:
            try:
                v = int(insn.op_str.split(",")[-1].strip(), 16)
                note = describe_imm(v)
            except ValueError:
                pass
        elif "[" in insn.op_str:
            # DS memory operands: name a few globals the notes fix
            for g, name in ((0x57BF, "g_wooden_box"), (0x58A0, "g_cmb_sentinel"),
                            (0x5887, "gate_stage"), (0x5C5A, "actor_table"),
                            (0xAD99, "scene_map(5,4)"), (0xAD14, "scene_map"),
                            (0xAC64, "miscmaps_buf")):
                if "0x%x]" % g in insn.op_str:
                    note = name
        print("%04x: %-20s %-6s %-30s %s" % (insn.address, insn.bytes.hex(), insn.mnemonic,
                                            insn.op_str, ("; " + note) if note else ""))
        pos += insn.size


if __name__ == "__main__":
    main()
