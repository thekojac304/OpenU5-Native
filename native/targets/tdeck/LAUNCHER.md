# Launcher packaging — Alpha 2 RC1

From an activated ESP-IDF v6.1 shell, on a clean, committed tree (the firmware
embeds `git rev-parse --short=12 HEAD` at CMake configure time):

```sh
idf.py --no-ccache -B build-batch54 build
python package_launcher.py --build-dir build-batch54
```

Validated output:

`build-batch54/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`

The packager checks the ESP32-S3 application header, the segment checksum and the
appended SHA-256, then makes a byte-identical copy. It does not merge a
bootloader or partition table, pad, flash, or otherwise modify the image. Since
Batch 54 the file name is derived from `PROJECT_VER` (`CMakeLists.txt`), so a
release candidate never shares a name with an ordinary batch image:

| `PROJECT_VER` | Launcher file |
|---|---|
| `2.0.0-alpha2-debug` (Batches up to 53B) | `OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin` |
| `2.0.0-alpha2-rc1-debug` (Batch 54, RC1) | `OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin` |

- Firmware version: `2.0.0-alpha2-rc1-debug` (identity screen: `FW 2.0.0-alpha2-rc1-debug`)
- Size: 878,752 bytes (`0xd68a0`). This leaves 169,824 B (16 %) free in the 1 MiB app partition.
- Minimum 64-KiB-aligned Launcher allocation: 917,504 bytes (896 KiB)
- ESP-IDF: 6.1, target ESP32-S3
- The image SHA-256 and its embedded `Git` hash are recorded in the annotated tag `alpha2-batch54-rc1`.
- Required SD resource pack: 2,041,466 B, CRC32 `26f75ae6` (unchanged since Batch 53)

Release notes and install instructions: [`../../../ALPHA2.md`](../../../ALPHA2.md).
The Alpha 2.0.0 packaging record (779,680 B, SHA-256 `895f7099…0cbe`) is in the
git history of this file and in [ALPHA20_FRONTEND_NEW_GAME.md](ALPHA20_FRONTEND_NEW_GAME.md).
