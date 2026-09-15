# Launcher packaging — Milestone 3

## Generate

In an activated ESP-IDF v6.1 shell, from `native/targets/tdeck`:

```sh
idf.py build
python package_launcher.py
```

Output:

`native/targets/tdeck/build/launcher/OpenU5-TDeck-M3-Launcher.bin`

The packager uses the existing normal ESP-IDF build, validates its ESP32-S3 app
header, segment checksum, and appended SHA-256, then copies it byte-for-byte.
It does not compile, merge, pad, flash, or modify the normal build output. For
another build directory use `python package_launcher.py --build-dir PATH`.

Current package details:

- Exact size: 303,440 bytes.
- SHA-256: `f9e7855c7b4b12fd4059cee2b47981813b8bc4c619a814cd99cedc0527e4ecfb`.
- Minimum aligned Launcher app allocation: 327,680 bytes (320 KiB).
- Source app image: `build/openu5_tdeck.bin`, also 303,440 bytes.

## Format and compatibility

The file starts with the standard ESP application header (`0xE9`) at offset
zero, followed by segments, checksum, and SHA-256. It contains no Launcher
wrapper, bootloader, partition table, filesystem, or leading address padding.
Launcher chooses the destination app partition.

This preserves the packaging model verified for Milestone 1 against
[bmorcelli Launcher installation documentation](https://github.com/bmorcelli/Launcher/wiki/Obtaining-binaries-to-launch),
the [SD installer](https://github.com/bmorcelli/Launcher/blob/main/src/sd_functions.cpp),
and its [partition install layout](https://github.com/bmorcelli/Launcher/blob/main/src/partition_install_layout.cpp).
Milestone 3 adds no application NVS or data-partition dependency; the asset pack
remains an ordinary file on the microSD card.

## Install on T-Deck Plus

Copy `OpenU5-TDeck-M3-Launcher.bin` to a FAT-formatted SD card, insert it, open
Launcher, choose **SD**, select the file, and choose **Install**. The package
requires one available app entry and at least 320 KiB of suitably contiguous
app space after alignment.

Do not use the standalone `idf.py flash` workflow for coexistence installation;
that separate workflow also writes this project's standalone partition table.
After testing, reset and enter Launcher again to confirm it remains selectable.

## Validation status

Local ESP-IDF v6.1 build, `idf.py size`, package validation, checksum validation,
and byte-for-byte comparison passed. Milestone 2 display, SD, shared-bus, and
Launcher return behavior were hardware-verified by the user. Milestone 3 asset
validation still needs its physical-device check.
