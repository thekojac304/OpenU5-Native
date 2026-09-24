"""Print 16-bit shipped Ultima V code ranges, using OVL offsets or EXE image offsets."""
import argparse
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

p = argparse.ArgumentParser()
p.add_argument("file")
p.add_argument("start", type=lambda s: int(s, 0))
p.add_argument("end", type=lambda s: int(s, 0))
a = p.parse_args()
data = (Path(__file__).parents[2] / "original/u5/ultima5" / a.file).read_bytes()
if a.file.upper().endswith(".EXE"):
    data = data[int.from_bytes(data[8:10], "little") * 16:]
md = Cs(CS_ARCH_X86, CS_MODE_16)
for insn in md.disasm(data[a.start:a.end], a.start):
    print(f"{insn.address:04x}: {insn.bytes.hex():<20} {insn.mnemonic} {insn.op_str}".rstrip())
