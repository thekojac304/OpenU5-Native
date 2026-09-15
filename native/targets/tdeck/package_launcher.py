"""Package the existing ESP-IDF app for Launcher; never merge or flash it."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def validate(data: bytes) -> None:
    if len(data) <= 0x8010:
        raise ValueError("Image too short for the inspected Launcher SD detection path")
    if data[0] != 0xE9 or not 1 <= data[1] <= 16:
        raise ValueError("Expected an ESP application image at file offset zero")
    if struct.unpack_from("<H", data, 12)[0] != 9:
        raise ValueError("Expected ESP32-S3 chip ID 9")
    if data[32:36] != struct.pack("<I", 0xABCD5432):
        raise ValueError("Missing app descriptor; do not package a bootloader/merged image")
    if data[0x8000:0x8003] == bytes([0xAA, 0x50, 0x01]):
        raise ValueError("Image collides with Launcher's merged-image detection")
    cursor, checksum = 24, 0xEF
    for _ in range(data[1]):
        if cursor + 8 > len(data):
            raise ValueError("Truncated segment header")
        length = struct.unpack_from("<I", data, cursor + 4)[0]
        cursor += 8
        if cursor + length > len(data):
            raise ValueError("Truncated segment")
        for byte in data[cursor:cursor + length]:
            checksum ^= byte
        cursor += length
    end = (cursor + 1 + 15) & ~15
    if end > len(data) or data[end - 1] != checksum:
        raise ValueError("Invalid image checksum")
    if data[23] != 1 or len(data) != end + 32:
        raise ValueError("Expected unsigned ESP-IDF image with appended SHA-256")
    if hashlib.sha256(data[:end]).digest() != data[end:]:
        raise ValueError("Invalid image SHA-256")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).resolve().parent / "build")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    metadata = json.loads((build / "project_description.json").read_text(encoding="utf-8"))
    if metadata["target"] != "esp32s3" or metadata["project_name"] != "openu5_tdeck":
        raise ValueError("Expected the OpenU5-TDeck ESP32-S3 build")
    source = (build / metadata["app_bin"]).resolve()
    if source.parent != build:
        raise ValueError("App image must be inside the build directory")
    data = source.read_bytes()
    validate(data)
    destination = build / "launcher" / "OpenU5-TDeck-M5-Launcher.bin"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    if destination.read_bytes() != data:
        raise OSError("Packaged image differs from app image")
    print(f"Launcher image: {destination}")
    print(f"Size: {len(data)} bytes")
    print(f"App partition minimum: {(len(data) + 0xFFFF) & ~0xFFFF} bytes (64 KiB alignment)")
    print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
    print(f"Existing build ESP-IDF: {metadata.get('git_revision', 'unknown')}")


if __name__ == "__main__":
    main()
