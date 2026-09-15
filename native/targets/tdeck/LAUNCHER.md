# Launcher packaging — Milestone 2

## Generate

In an activated ESP-IDF v6.1 shell, from `native/targets/tdeck`:

```sh
idf.py build
python package_launcher.py
```

Output:

`native/targets/tdeck/build/launcher/OpenU5-TDeck-M2-Launcher.bin`

The packager uses the existing normal ESP-IDF build, validates its ESP32-S3 app
header, segment checksum, and appended SHA-256, then copies it byte-for-byte.
It does not compile, merge, pad, flash, or modify the normal build output. For
another build directory use `python package_launcher.py --build-dir PATH`.

Current package details:

- Exact size: 298,064 bytes.
- SHA-256: `031005dd809df923fdeef476a1ab5ed3726baf07999679aba5d8fbe7c0b52729`.
- Minimum aligned Launcher app allocation: 327,680 bytes (320 KiB).
- Source app image: `build/openu5_tdeck.bin`, also 298,064 bytes.

## Format and compatibility

The file starts with the standard ESP application header (`0xE9`) at offset
zero, followed by segments, checksum, and SHA-256. It contains no Launcher
wrapper, bootloader, partition table, filesystem, or leading address padding.
Launcher chooses the destination app partition.

This preserves the packaging model verified for Milestone 1 against
[bmorcelli Launcher installation documentation](https://github.com/bmorcelli/Launcher/wiki/Obtaining-binaries-to-launch),
the [SD installer](https://github.com/bmorcelli/Launcher/blob/main/src/sd_functions.cpp),
and its [partition install layout](https://github.com/bmorcelli/Launcher/blob/main/src/partition_install_layout.cpp).
Milestone 2 adds no application NVS or data-partition dependency.

## Install on T-Deck Plus

Copy `OpenU5-TDeck-M2-Launcher.bin` to a FAT-formatted SD card, insert it, open
Launcher, choose **SD**, select the file, and choose **Install**. The package
requires one available app entry and at least 320 KiB of suitably contiguous
app space after alignment.

Do not use the standalone `idf.py flash` workflow for coexistence installation;
that separate workflow also writes this project's standalone partition table.
After testing, reset and enter Launcher again to confirm it remains selectable.

## Validation status

Local ESP-IDF v6.1 build, `idf.py size`, package validation, checksum validation,
and byte-for-byte comparison passed. Milestone 1 has been hardware-tested by
the user through Launcher. Milestone 2 display, SD, shared-bus, and Launcher
return behavior remain pending physical T-Deck Plus testing.
