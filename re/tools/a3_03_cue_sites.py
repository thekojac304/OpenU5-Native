"""A3-03 -- disassemble every PC-speaker site the A3-03 cues are built from,
straight from the shipped binaries, as whole ranges (several cues are loops
whose arguments only make sense with the loop head and tail around them).

    python re/tools/a3_03_cue_sites.py [original/u5/ultima5] > native/core/a3-03-cue-sites.log

Conventions are A3-02's (re/tools/a3_02_cue_sites.py):
    tone_sweep 0x2192 (a0 bx step, a1 bx start, a2 count, a3 delay, a4 inc)
    noise_burst 0x223c (a0 band, a1 dur, a2 step)
    beep 0x22c0 (a0 dur, a1 freq)          set_tone 0x22e2 (a0 value)
    glide 0x43ae (a0 total, a1 step, a2 end, a3 start)
    delay 0x20c8 (a0 count, a1 shift)      rand_range 0x2092 (a0 hi, a1 lo)
a0 is the LAST push. Every overlay `call` is annotated with its kernel target,
(rel + base) & 0xffff, using the base the A3-01 census chose for the file by
landing the most calls on known kernel entry points.
"""
import os
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

ROOT = sys.argv[1] if len(sys.argv) > 1 else "original/u5/ultima5"
BASES = {"ULTIMA.EXE": 0, "TOWN.OVL": 0x81D0, "MAINOUT.OVL": 0x81D0, "DUNGEON.OVL": 0x81D0,
         "OUTSUBS.OVL": 0xA290, "COMBAT.OVL": 0xA290, "SHOPPES.OVL": 0xA290, "ENDGAME.OVL": 0xA290,
         "BLCKTHRN.OVL": 0xA290, "LOOKOBJ.OVL": 0xA290, "CMDS.OVL": 0xBF80, "SJOG.OVL": 0xBF80,
         "CAST.OVL": 0xBF80, "TALK.OVL": 0xBF80, "CAST2.OVL": 0xE1E0, "COMSUBS.OVL": 0xE1E0,
         "FONT.OVL": 0xE1E0, "ZSTATS.OVL": 0xE1E0}
KERNEL = {0x2192: "tone_sweep", 0x223c: "noise_burst", 0x22c0: "beep", 0x22e2: "set_tone",
          0x230e: "stop", 0x43ae: "glide", 0x20c8: "delay", 0x20fa: "delay2", 0x2092: "rand_range",
          0x4368: "sfx_victory_fanfare", 0x3072: "screen_shake_rumble", 0x1850: "print_ds",
          0x16ba: "putchar", 0x5910: "viewport_redraw", 0x3ae6: "run_n_frames", 0x1b16: "kbd_flush",
          0x4402: "get_tile_ptr", 0x4102: "ambient_sfx_tick", 0x433e: "sfx_footstep",
          0x3fb2: "tone_sweep(thunk)", 0x1070: "key_wait_idle"}

# (file, first offset, last offset, what it is)
RANGES = [
    # -- combat victory and the fanfare itself
    ("COMBAT.OVL", 0x0cc8, 0x0d0a, "combat_main_loop: BATTLE IS LOST / VICTORY! -> fanfare -> kbd flush"),
    ("ULTIMA.EXE", 0x4368, 0x43ad, "sfx_victory_fanfare 0x4368: 3 x TS(4600) + TS(6100)"),
    # -- shard ritual (CAST) and its fanfare tail
    ("CAST.OVL", 0x15d8, 0x1632, "use_shard_at_flame: the two 460-call sweep loops"),
    ("CAST.OVL", 0x1740, 0x175e, "use_shard_at_flame tail: 'doom ... wrought' -> fanfare"),
    # -- shrine rites (CAST2 shrine_visit)
    ("CAST2.OVL", 0x0ac3, 0x0b06, "ORDAINED: 7-note table melody"),
    ("CAST2.OVL", 0x0bcb, 0x0c16, "donation accepted: two 460-call sweep loops (0xa8c, count 0xc8)"),
    ("CAST2.OVL", 0x0c3f, 0x0c8b, "WELL DONE: two 460-call sweep loops (0xc1c, count 0x96) -> shake"),
    # -- world
    ("ULTIMA.EXE", 0x48c8, 0x48f2, "moongate tile 0xdc: TS(0x170c,1,30000,2000,2)"),
    ("ULTIMA.EXE", 0x6200, 0x6230, "sceptre: TS(0xfd2,1,0xfde8,1,ax)"),
    ("TOWN.OVL", 0x11b8, 0x11f0, "shadowlord_announce drone"),
    ("ULTIMA.EXE", 0x3072, 0x3176, "screen_shake_rumble 0x3072 (quake / refuge thunder)"),
    ("SHOPPES.OVL", 0x13b0, 0x146a, "healer jingle: six tone_sweeps"),
    # -- ambient proximity tick
    ("ULTIMA.EXE", 0x4247, 0x4335, "ambient_sfx_tick 0x4102: class switch + phase counter"),
    ("ULTIMA.EXE", 0x1070, 0x10e8, "key-wait idle loop: every 8 polls -> viewport_redraw"),
    # -- combat derivations
    ("SJOG.OVL", 0x1c20, 0x1c3a, "combat escape (per member): 'Escape!' -> glide"),
    ("SJOG.OVL", 0x1ee8, 0x1f0b, "absorb: ' is absorbed!' -> glide"),
    ("SJOG.OVL", 0x1f26, 0x1f66, "combat reject funnel: code text -> two beeps"),
    ("CMDS.OVL", 0x1890, 0x18b0, "escape handler 0x17ec tail -> glide"),
    # -- Blackthorn
    ("BLCKTHRN.OVL", 0x03c8, 0x0412, "sacrifice siren loops"),
    ("BLCKTHRN.OVL", 0x0a10, 0x0a3a, "table sweep 0x0a34"),
    ("BLCKTHRN.OVL", 0x0b70, 0x0b92, "TS(ax,1,0x7530,0x7d0,2) 0x0b8d"),
    ("BLCKTHRN.OVL", 0x0ab8, 0x0ad8, "party_refuge: 'peal of thunder' -> 0x3072 x2"),
    # -- endgame / intro
    ("ENDGAME.OVL", 0x0778, 0x0792, "'lives!' sweep 0x078f"),
    ("ENDGAME.OVL", 0x0960, 0x098a, "orb launch sweep 0x0987"),
    ("FONT.OVL", 0x03b8, 0x0408, "intro moongate: thunder NB / chime beep"),
    ("FONT.OVL", 0x0878, 0x0892, "intro SUMMON NB"),
    # -- not yet attributed kernel glide
    ("ULTIMA.EXE", 0x6a20, 0x6a42, "kernel glide 0x6a3c (not in the 1988 catalogue)"),
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
    for name, lo, hi, label in RANGES:
        data = load(name)
        print(f"== {name} 0x{lo:04x}-0x{hi:04x}  {label}")
        if data is None:
            print("   missing")
            continue
        base = BASES.get(name, 0)
        for i in md.disasm(data[lo:hi + 1], lo):
            note = ""
            if i.mnemonic == "call" and i.op_str.startswith("0x"):
                rel = int(i.op_str, 16)
                k = rel if name == "ULTIMA.EXE" else (rel + base) & 0xFFFF
                note = f"   ; -> kernel 0x{k:04x} {KERNEL.get(k, '')}"
            print(f"   {i.address:04x}: {i.mnemonic} {i.op_str}{note}")


if __name__ == "__main__":
    main()
