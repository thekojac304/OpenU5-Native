# Launcher packaging — Alpha 2.0.0 alpha2 physical correction

From an activated ESP-IDF v6.1 shell:

```sh
idf.py -B build-alpha20-final build
python package_launcher.py --build-dir build-alpha20-final
```

Validated output:

`build-alpha20-final/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

The packager validates the ESP32-S3 application header, segment checksum, and
appended SHA-256 before making a byte-identical copy. It does not merge a
bootloader or partition table, pad, flash, or otherwise modify the image.

- Firmware version: `2.0.0-alpha2-debug`
- Size: 779,680 bytes
- SHA-256: `895f7099a7d5eee72d295390a1149af75f4f4ee06307fb3ab8a964e701330cbe`
- Minimum 64-KiB-aligned Launcher allocation: 786,432 bytes (768 KiB)
- ESP-IDF: 6.1, target ESP32-S3

This image is host-built and has not been flashed or physically timed in this
environment. See [ALPHA20_FRONTEND_NEW_GAME.md](ALPHA20_FRONTEND_NEW_GAME.md)
for the frontend/new-game design, verification record, and physical checklist.
