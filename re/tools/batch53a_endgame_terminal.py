#!/usr/bin/env python
"""Batch 53A -- what the original does after the final Doom absorption.

Reads the shipped binaries directly (original/ is gitignored EA material; this
prints offsets, bytes and decoded instructions, never the files themselves):

  1. entry: the absorption sentinel 0x4d at DS 0x58a0 diverts the combat
     teardown (DUNGEON.OVL 0x00cb / SJOG.OVL 0x2046) into the ONE overlay-13
     stub (ULTIMA.EXE 0x7c4a) -> ENDGAME.OVL endgame_main 0x0648;
  2. endgame_main loads its own scene map (MISCMAPS.DAT) and ENDMSG.DAT, paces
     each page on getkey_with_redraw, and asks Y/N in two getkey loops;
  3. every `ret` inside endgame_main and endgame_datestamp, and every jump out
     of endgame_datestamp -- to show neither returns to the dungeon loop;
  4. the two terminal loops (0x04f9 victory, 0x0ac9 stranded) and what the
     loop body does in THIS build.

Usage: batch53a_endgame_terminal.py [original/u5/ultima5]
Kernel near-call targets from ENDGAME.OVL: fileoff_ULTIMA = (CS + 0xA290) & 0xFFFF
(re/ledger; the same slot as COMBAT/NPC/TALK).
"""
import hashlib
import sys
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_16

ROOT = Path(__file__).resolve().parents[2]
D = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "original/u5/ultima5"
BASE = 0xA290
KERNEL = {0x1850: "print_string", 0x266c: "getkey_with_redraw", 0x3ae6: "run_n_frames",
          0x256e: "read_file_block_with_disk_retry", 0x1d5e: "kernel_getkey (int 16h AH=01 poll)",
          0x0878: "restore_video_mode", 0x02f4: "exit() -> 0x034c -> 0x0e2f int 21h AH=4Ch",
          0x2900: "draw_status_panel", 0x0e03: "sound-driver dispatch (dx = function)",
          0x7c4a: "overlay-13 stub -> ENDGAME.OVL", 0x0e2f: "driver shutdown + int 21h AH=4Ch",
          0x0e26: "sound-driver shutdown (dx=3)", 0x0350: "crt0 restore int vectors"}

md = Cs(CS_ARCH_X86, CS_MODE_16)


def load(name):
    return (D / name).read_bytes()


def exe_image():
    b = load("ULTIMA.EXE")
    return b[int.from_bytes(b[8:10], "little") * 16:]


def dis(data, start, end):
    return [i for i in md.disasm(data[start:end], start)]


def target(ins):
    try:
        return int(ins.op_str, 16) & 0xFFFF
    except ValueError:
        return None


# Near-call slot of each module (re/notes; the overlay-base table).
SLOTS = {"ENDGAME.OVL": BASE, "DUNGEON.OVL": 0x81D0, "SJOG.OVL": 0xBF80, "ULTIMA.EXE": None}


def name(ins, module):
    t = target(ins)
    if t is None or ins.mnemonic not in ("call", "jmp"):
        return ""
    slot = SLOTS[module]
    if slot is None:
        return f"  ; kernel 0x{t:04x} {KERNEL.get(t, '')}"
    if t < len(load(module)):
        return f"  ; {module} 0x{t:04x}"
    k = (t + slot) & 0xFFFF
    return f"  ; kernel 0x{k:04x} {KERNEL.get(k, '(overlay-13 stub)' if k == 0x7c4a else '')}"


def show(data, start, end, title, module="ENDGAME.OVL"):
    print(f"\n--- {title} [0x{start:04x}, 0x{end:04x})")
    for i in dis(data, start, end):
        print(f"  {i.address:04x}: {i.bytes.hex():<14} {i.mnemonic} {i.op_str}{name(i, module)}")


def ds_string(data_ovl, ds):
    o = ds + 0x10
    return data_ovl[o:data_ovl.index(b"\0", o)].decode("latin-1")


eg = load("ENDGAME.OVL")
data_ovl = load("DATA.OVL")
exe = exe_image()
print(f"source: {D}")
print(f"ENDGAME.OVL {len(eg)} B sha1 {hashlib.sha1(eg).hexdigest()}")
print(f"ULTIMA.EXE  {len(load('ULTIMA.EXE'))} B sha1 {hashlib.sha1(load('ULTIMA.EXE')).hexdigest()}")

print("\n=== 1. entry: sentinel 0x4d -> overlay-13 stub -> endgame_main")
show(load("DUNGEON.OVL"), 0x00cb, 0x00d5, "DUNGEON.OVL room return", "DUNGEON.OVL")
show(load("SJOG.OVL"), 0x2046, 0x2050, "SJOG.OVL combat-exit restore", "SJOG.OVL")
stub = exe[0x7c4a:0x7c54]
print(f"\n  ULTIMA.EXE 0x7c4a: {stub.hex()}  = lcall loader, overlay #{stub[5]}, ljmp 0x{int.from_bytes(stub[8:10], 'little'):04x}"
      f" -> ENDGAME.OVL 0x{int.from_bytes(stub[8:10], 'little') - BASE:04x}")

print("\n=== 2. endgame_main 0x0648: its own files, its own key loops")
main = dis(eg, 0x0648, 0x0aee)
for i in main:
    if i.mnemonic == "mov" and i.op_str.startswith("ax, 0x84") and i.address in (0x065f, 0x0672):
        print(f"  {i.address:04x}: file name DS {i.op_str[4:]} = {ds_string(data_ovl, int(i.op_str[4:], 16))!r}")
getkeys = [i.address for i in main if i.mnemonic == "call" and (target(i) + BASE) & 0xFFFF == 0x266c]
print(f"  getkey_with_redraw call sites in endgame_main: {len(getkeys)} -> {' '.join(f'0x{a:04x}' for a in getkeys)}")
show(eg, 0x0852, 0x087c, "Y/N loop 1 ('Didst thou bring my box?'): loops until 'Y' or 'N'")
show(eg, 0x08b9, 0x08cc, "branch: answer 'Y' AND g_wooden_box (DS 0x57bf) != 0")

print("\n=== 3. every ret / exit")
rets = [i.address for i in main if i.mnemonic.startswith("ret")]
print(f"  endgame_main [0x0648,0x0aee): ret at {' '.join(f'0x{a:04x}' for a in rets)}")
show(eg, 0x0a6d, 0x0a76, "the only path to that ret: call endgame_datestamp, then jmp 0x0ae8")
ds = dis(eg, 0x0326, 0x04fe)
print(f"  endgame_datestamp [0x0326,0x04fe): ret count {sum(i.mnemonic.startswith('ret') for i in ds)}; "
      f"jumps leaving the body: {[hex(target(i)) for i in ds if i.mnemonic.startswith('j') and not 0x0326 <= target(i) < 0x04fe]}")

print("\n=== 4. the terminal loops")
show(eg, 0x04eb, 0x04fe, "victory: last print, then the loop at 0x04f9")
show(eg, 0x0ac9, 0x0aee, "stranded: the wander loop")
show(eg, 0x0b0b, 0x0b24, "loop body helpers (0x0b0b poll, 0x0b1f stranded tail)")
show(exe, 0x1d5e, 0x1d72, "kernel 0x1d5e: non-blocking key poll", "ULTIMA.EXE")
show(exe, 0x0346, 0x034f, "kernel exit(): 0x0346 close handles, 0x034c -> 0x0e2f", "ULTIMA.EXE")
show(exe, 0x0e2f, 0x0e36, "kernel 0x0e2f: driver shutdown, DOS terminate", "ULTIMA.EXE")
print("\nConclusion: after game-won the machine is inside endgame_main for good. In this build a key\n"
      "pressed in either terminal loop restores the video mode and terminates the program; a build\n"
      "whose 0x04f9 is `ff 46 fe eb fb` (re/notes/fanfarria-endgame-espectral.md, original/u5/play)\n"
      "spins without reading the keyboard. Neither returns to the dungeon or accepts a game command.")
