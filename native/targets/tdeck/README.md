# OpenU5-TDeck: Milestone 3 native asset validation

Standalone ESP-IDF firmware for the LilyGO T-Deck Plus (ESP32-S3, 16 MiB
flash, 8 MiB octal PSRAM). Milestone 3 preserves the hardware-verified display,
microSD, shared-SPI, diagnostics, and Launcher packaging from Milestone 2, and
adds validation of the first compact native asset pack. Pack generation and
format details are in [../../ASSETS.md](../../ASSETS.md). The web/TypeScript
runtime is independent and unchanged.

No game logic, keyboard, touch, trackball, audio, GPS, LoRa, or Ultima rendering
is included. Launcher app-only packaging remains available in
[LAUNCHER.md](LAUNCHER.md).

## Hardware basis

The implementation follows LilyGO's current references:

- [T-Deck Plus documentation](https://wiki.lilygo.cc/products/t-deck-series/t-deck-plus/):
  ESP32-S3 at 240 MHz, 16 MB flash, 8 MB PSRAM, 320x240 ST7789, shared SPI
  pins, peripheral power GPIO 10, and the all-CS-high requirement.
- [Official board pin definitions](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h):
  SPI2 SCK/MOSI/MISO 40/41/38; TFT CS/DC/backlight 12/11/42; SD CS 39; LoRa
  CS 9.
- [Official display profile](https://github.com/Xinyuan-LilyGO/LilyGo-display-library/blob/main/src/LilyGo_T_Deck.h):
  ST7789 native 240x320, landscape rotation 1, shared 40 MHz SPI bus, no reset
  GPIO, and 320x240 logical dimensions.
- [LilyGO corrected ST7789 initialization](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/lib/TFT_eSPI/TFT_Drivers/ST7789_Init.h),
  including panel power/gamma values and display inversion.

`main/tdeck_pins.h` centralizes those pins. `main/tdeck_board.cpp` owns power,
safe chip-select setup, the one SPI2 bus, the lightweight native ST7789 driver,
microSD mounting, file verification, and display primitives. It intentionally
does not use a general-purpose GUI framework.

The TFT is a 40 MHz SPI device. SD is kept at the conservative 800 kHz used by
LilyGO's factory UnitTest for this shared-bus path. Before bus initialization,
TFT CS 12, SD CS 39, and LoRa CS 9 are configured as outputs and driven high.
Peripheral power GPIO 10 is then driven high and given 500 ms to settle.

## Build

ESP-IDF v6.1 is the validated environment. In an activated ESP-IDF shell:

```sh
cd native/targets/tdeck
idf.py set-target esp32s3
idf.py build
idf.py size
python package_launcher.py
```

Normal build output remains `build/openu5_tdeck.bin`; the package step only
validates and copies that app image. `sdkconfig.defaults` selects 16 MiB QIO
flash, 8 MiB OPI PSRAM, USB Serial/JTAG console, and 240 MHz CPU operation.

## Boot behavior

After the existing hardware and heap diagnostics, firmware:

1. Powers peripherals with the backlight off and all shared SPI CS lines high.
2. Initializes SPI2 once and attaches the ST7789 at 40 MHz.
3. Runs LilyGO's corrected panel sequence, selects 320x240 landscape rotation,
   clears the screen, then enables the backlight.
4. Mounts the FAT microSD card without ever formatting it.
5. Prints the card type, capacity, negotiated bus frequency, and ESP-IDF's full
   card information to serial.
6. Reads `/sd/openu5-m2-test.txt` when present. Otherwise it creates
   `/sd/.openu5-m2-diag.tmp`, flushes and reads it back, verifies its exact
   payload, and removes it.
7. Alternates three matching SD reads with three TFT writes on the shared bus.
8. Opens `/sd/ultima5/openu5-assets.bin` when present, streams all CRC checks,
   and reports its dimensions, initial position, tile-0 sample, and initial map tile.
9. Draws the final screen with `OpenU5-TDeck`, `Milestone 3`, `ESP32-S3`,
   `16 MB Flash`, `8 MB PSRAM`, and `SD: OK` or `SD: FAIL`.
10. Continues the five-second serial heartbeat even when display, SD, or asset
    validation fails.

An SD error is logged with its ESP-IDF error name and does not reboot or stop
the application. A passing interleave test is runtime evidence produced on the
device; the local build alone cannot establish electrical or card reliability.

## Current local validation

- ESP-IDF v6.1 compile: passed.
- `idf.py size`: passed. Linked image total is 303,328 bytes. Flash code is
  154,094 bytes, flash data is 67,016 bytes, and DIRAM use is 68,814 of
  341,760 bytes (20.14%).
- App binary: 303,440 bytes (`0x4a150`), leaving 71% of the standalone 1 MiB
  app partition free.
- Launcher package: 303,440 bytes; minimum Launcher app allocation is 327,680
  bytes after 64 KiB alignment.
- Hardware status: Milestone 2 display, SD, shared SPI, and Launcher behavior
  were verified successfully by the user. Milestone 3 asset validation is not
  yet hardware-tested.

## Hardware validation checklist

Install the Milestone 3 Launcher image, attach a 115200-baud monitor, and verify:

- Serial reports `CPU frequency: 240 MHz`.
- Screen orientation is landscape and all required text is readable.
- With a FAT-formatted card inserted, screen ends at `SD: OK`; serial prints
  card information and `Shared SPI interleave test passed`.
- With a generated pack at `/ultima5/openu5-assets.bin`, serial reports asset
  pack v1.0, 512 16x16 tiles, Britannia 256x256, and the initial sanity values.
- With the pack absent or deliberately corrupted, serial reports that condition
  and heartbeats continue without rendering or rebooting.
- Without a card, screen ends at `SD: FAIL`, serial explains the error, and
  heartbeats continue.
- Reset back into Launcher afterward to confirm Launcher remains available.

Do not claim Milestone 3 hardware success until those asset checks pass on the
physical T-Deck Plus.
