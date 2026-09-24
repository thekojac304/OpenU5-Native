"""Reproduce Batch 49's shipped-byte NPC/bed inputs without running the game.

This is a static trace. It deliberately does not claim a live DS:6608 value or
an original framebuffer result.
"""

from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = ROOT / "original/u5/ultima5"


def load(name: str) -> bytes:
    data = (ORIGINAL / name).read_bytes()
    print(f"{name}: bytes={len(data)} sha256={sha256(data).hexdigest()}")
    return data


def expect(data: bytes, offset: int, hex_bytes: str, label: str) -> None:
    want = bytes.fromhex(hex_bytes)
    got = data[offset : offset + len(want)]
    assert got == want, f"{label} at {offset:04x}: {got.hex()} != {want.hex()}"
    print(f"{label} {offset:04x}: {got.hex()}")


def schedule_index(times: bytes, hour: int) -> int:
    differences = [((hour - t) & 0xFF) for t in times]
    winner = min(range(4), key=lambda i: differences[i])
    return 1 if winner == 3 else winner


def main() -> None:
    exe_file = load("ULTIMA.EXE")
    header_size = int.from_bytes(exe_file[8:10], "little") * 16
    exe = exe_file[header_size:]
    town = load("TOWN.OVL")
    npc = load("CASTLE.NPC")
    castle = load("CASTLE.DAT")
    font = load("FONT.OVL")
    print(f"ULTIMA.EXE MZ image offset={header_size:#x}")

    # These assertions force the trace to fail if any cited original opcode differs.
    for off, raw, label in (
        (0x51BF, "837e041c", "selector actor-0x1c gate"),
        (0x51D1, "837e04407d12", "selector >=0x40 enters lookup"),
        (0x51E9, "ff7608ff7606e810f2", "selector terrain lookup 0x4402"),
        (0x51F4, "8a072ae48946fe", "selector terrain byte zero extension"),
        (0x524A, "817e0480007c03", "selector signed <0x80 gate"),
        (0x531A, "3d9e007f37", "terrain >0x9e reaches 0x5356"),
        (0x5356, "3dab007503e93cff", "terrain 0xab jumps to pose"),
        (0x529A, "c746041a00e9ce00", "pose writes local argument 0x1a"),
        (0x537A, "8a4604888064ac", "composite writes low actor byte"),
        (0x4402, "558bec83ec04", "terrain accessor entry"),
        (0x4420, "803e9358007557", "terrain accessor town branch"),
        (0x449E, "8b4604b105d3e0034606050866", "town DS:6608+y*32+x"),
        (0x3A7F, "8a461088845a5c", "object record copies type argument"),
        (0x55E6, "8a46fa2ae4508a46f850", "ordinary object caller coordinates"),
        (0x55F8, "8b5eda8a0750e8b7fb", "ordinary caller loads actor with AH already zero"),
    ):
        expect(exe, off, raw, label)
    for off, raw, label in (
        (0x17D9, "8b5e0a8a879e652ae48bf05656", "NPC type pushed unchanged twice"),
        (0x17E6, "ff7608ff7606ff7604ff76faff76fce8aca0", "NPC placement invokes object writer"),
    ):
        expect(town, off, raw, label)
    expect(font, 0x2E3, "2ae480c401", "actor bank adds 0x100")

    # Castle location 17 is the first CASTLE.NPC group. CASTLE.DAT floor 0
    # follows its floor -1, hence the 0x400 file offset.
    for slot, x, y, actor in ((13, 9, 7, 0x5C), (1, 17, 7, 0x70)):
        record = npc[slot * 16 : (slot + 1) * 16]
        npc_type = npc[0x200 + slot]
        dialog = npc[0x220 + slot]
        period = schedule_index(record[12:16], 23)
        sx, sy, sz = record[3 + period], record[6 + period], record[9 + period]
        terrain_offset = 0x400 + 32 * y + x
        head, foot = castle[terrain_offset : terrain_offset + 2]
        assert (npc_type, sx, sy, sz, head, foot) == (actor, x, y, 0, 0xAB, 0xAC)
        print(
            f"castle loc=17 slot={slot} at 23:45: schedule={record.hex()} "
            f"period={period} position=({sx},{sy},{sz}) authored_type={npc_type:#04x} "
            f"dialog={dialog:#04x} CASTLE.DAT[{terrain_offset:#06x}]="
            f"{head:#04x}/{foot:#04x}"
        )
        assert 0x40 <= npc_type < 0x80
        print(
            f"  predicted unchanged placement/render byte={npc_type:#04x}; "
            "IF live DS:6608 cell remains 0xab, selector output=0x1a, "
            "actor-bank tile=0x11a"
        )
    print("LIMIT: no live actor/terrain memory or original framebuffer is observed here")


if __name__ == "__main__":
    main()
