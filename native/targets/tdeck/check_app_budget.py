"""A4-FLASH1 app-size budget for the OpenU5-TDeck image (ALPHA4_UI.md section 12).

Run after every firmware build (CMakeLists.txt target ``openu5_app_budget``) and
by package_launcher.py. It reads the built app image and the built partition
table, so the thresholds follow partitions.csv instead of repeating its size:

- fewer than RESERVE_WARN bytes free in the smallest app partition: a warning,
  the next feature is due a size review first;
- fewer than RESERVE_FAIL bytes free: the build fails, before ESP-IDF's own
  check_sizes.py would stop it at the partition's last byte.

It also prints the Launcher allocation, which is what an install really costs on
the device: Launcher creates an app partition of the image size rounded up to
64 KiB, whatever this build's partition table says.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

RESERVE_WARN = 128 * 1024
RESERVE_FAIL = 64 * 1024
LAUNCHER_ALIGN = 0x10000
_ENTRY = struct.Struct("<2sBBII16sI")
_TYPE_APP = 0x00


def smallest_app_partition(table: bytes) -> int:
    """The smallest app partition in a partition-table.bin, as check_sizes.py measures it."""
    sizes = []
    for offset in range(0, len(table) - _ENTRY.size + 1, _ENTRY.size):
        magic, ptype, _subtype, _start, size, _label, _flags = _ENTRY.unpack_from(table, offset)
        if magic != b"\xaa\x50":
            break
        if ptype == _TYPE_APP:
            sizes.append(size)
    if not sizes:
        raise ValueError("The partition table has no app partition")
    return min(sizes)


def launcher_allocation(image_size: int) -> int:
    return (image_size + LAUNCHER_ALIGN - 1) & ~(LAUNCHER_ALIGN - 1)


def evaluate(image_size: int, partition_size: int) -> tuple[str, list[str]]:
    """Return ("ok" | "warn" | "fail", report lines)."""
    free = partition_size - image_size
    allocation = launcher_allocation(image_size)
    lines = [
        f"OpenU5 app budget: image {image_size} B (0x{image_size:x}), app partition "
        f"{partition_size} B (0x{partition_size:x}), free {free} B ({100.0 * free / partition_size:.1f} %)",
        f"OpenU5 Launcher allocation: {allocation} B ({allocation // 1024} KiB, the image rounded up to "
        f"64 KiB); the next 64 KiB step is {allocation - image_size + 1} B away",
    ]
    if free < RESERVE_FAIL:
        lines.append(
            f"ERROR: fewer than {RESERVE_FAIL} B free in the app partition. Free space first, or raise "
            "the partition deliberately (ALPHA4_UI.md section 12 policy)."
        )
        return "fail", lines
    if free < RESERVE_WARN:
        lines.append(
            f"WARNING: fewer than {RESERVE_WARN} B free in the app partition; the next feature is due a "
            "size review first (ALPHA4_UI.md section 12)."
        )
        return "warn", lines
    return "ok", lines


def check_build(build: Path) -> tuple[str, list[str]]:
    metadata = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
    image_size = (build / metadata["app_bin"]).stat().st_size
    table = (build / "partition_table" / "partition-table.bin").read_bytes()
    return evaluate(image_size, smallest_app_partition(table))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    status, lines = check_build(args.build_dir.resolve())
    for line in lines:
        print(line)
    return 1 if status == "fail" else 0


if __name__ == "__main__":
    sys.exit(main())
