# Launcher packaging — Milestone 5.1 stack correction

## Generate

In an activated ESP-IDF v6.1 shell, from `native/targets/tdeck`:

```sh
idf.py -B build-m5 build
python package_launcher.py --build-dir build-m5
```

Output:

`native/targets/tdeck/build-m5/launcher/OpenU5-TDeck-M5-Launcher.bin`

The packager uses the existing normal ESP-IDF build, validates its ESP32-S3 app
header, segment checksum, and appended SHA-256, then copies it byte-for-byte.
It does not compile, merge, pad, flash, or modify the normal build output. For
another build directory use `python package_launcher.py --build-dir PATH`.

Current package details:

- Version: `0.5.1-input-final`; stability capture in [STACK51.md](STACK51.md).
- Exact size: 339,728 bytes.
- SHA-256: `5784cc45d38fdd1d50d17a3debf73737a8ff08ed70367836fa3d16ccd7e0cb01`.
- Minimum aligned Launcher app allocation: 393,216 bytes (384 KiB).
- Source app image: `build-m5/openu5_tdeck.bin`, also 339,728 bytes.

## Format and compatibility

The file starts with the standard ESP application header (`0xE9`) at offset
zero, followed by segments, checksum, and SHA-256. It contains no Launcher
wrapper, bootloader, partition table, filesystem, or leading address padding.
Launcher chooses the destination app partition.

This preserves the packaging model verified for Milestone 1 against
[bmorcelli Launcher installation documentation](https://github.com/bmorcelli/Launcher/wiki/Obtaining-binaries-to-launch),
the [SD installer](https://github.com/bmorcelli/Launcher/blob/main/src/sd_functions.cpp),
and its [partition install layout](https://github.com/bmorcelli/Launcher/blob/main/src/partition_install_layout.cpp).
Milestone 5 adds no application NVS or data-partition dependency; the asset pack
remains an ordinary file on the microSD card.

## Install on T-Deck Plus

Copy `OpenU5-TDeck-M5-Launcher.bin` to a FAT-formatted SD card, insert it, open
Launcher, choose **SD**, select the file, and choose **Install**. The package
requires one available app entry and at least 384 KiB of suitably contiguous
app space after alignment.

Do not use the standalone `idf.py flash` workflow for coexistence installation;
that separate workflow also writes this project's standalone partition table.
After testing, reset and enter Launcher again to confirm it remains selectable.

## Validation status

Local ESP-IDF v6.1 diagnostic build, package validation, checksum validation,
and byte-for-byte comparison passed. Avatar movement and trackball feel were
hardware-verified by the user. The prior keyboard cleanup failed physical
verification. Keyboard lockup and repeated startup screens are unresolved;
this build adds evidence collection only.
