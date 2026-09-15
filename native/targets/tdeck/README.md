# OpenU5-TDeck: Milestone 4 corrected initial screen

Standalone ESP-IDF firmware for the LilyGO T-Deck Plus (ESP32-S3, 16 MiB
flash, 8 MiB octal PSRAM). Milestone 4 preserves the hardware-verified display,
microSD, shared-SPI, asset validation, and Launcher packaging, and renders the
initial 11x11 Iolo's Hut viewport selected by INIT.GAM. Pack generation
and format details are in [../../ASSETS.md](../../ASSETS.md). The web/TypeScript
runtime is independent and unchanged.

No movement, input, animation, NPC, combat, dialogue, audio, save, or turn logic
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
- [Official TFT_eSPI T-Deck setup](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/lib/TFT_eSPI/User_Setups/Setup210_LilyGo_T_Deck.h):
  `TFT_RGB_ORDER` is `TFT_RGB`; landscape rotation 1 therefore uses ST7789
  MADCTL `MX | MV | RGB` (`0x60`), not BGR (`0x68`).
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
   explicitly selects RGB color order, clears the screen, then enables the
   backlight.
4. Mounts the FAT microSD card without ever formatting it.
5. Prints the card type, capacity, negotiated bus frequency, and ESP-IDF's full
   card information to serial.
6. Reads `/sd/openu5-m2-test.txt` when present. Otherwise it creates
   `/sd/.openu5-m2-diag.tmp`, flushes and reads it back, verifies its exact
   payload, and removes it.
7. Alternates three matching SD reads with three TFT writes on the shared bus.
8. Opens `/sd/ultima5/openu5-assets.bin`, streams all CRC checks, and retains the
   validated file for bounded palette, map-span, and whole-tile reads.
9. Uses `location=13`, `floor=0` to select the packed 32x32 Iolo's Hut floor,
   then reads its 11x11 row-major window centered at `x=15`, `y=15`. Britannia
   remains available only for initial states whose selected location is zero.
10. Expands indexed-4bpp pixels to a 61,952-byte RGB565 buffer in PSRAM, then
    replaces the center terrain cell with Avatar tile `0x11c` as CoreView does.
11. Transfers the native-scale 176x176 viewport at `(8,8)` and draws temporary
    Milestone 4 coordinates/location status in the surrounding space.
12. Logs viewport bounds, tile IDs/read count, render time, heap before/after,
    and a deterministic CRC32 over the final RGB565 little-endian pixel stream.
13. Continues the five-second serial heartbeat even when display, SD, or asset
    validation fails.

An SD error is logged with its ESP-IDF error name and does not reboot or stop
the application. A passing interleave test is runtime evidence produced on the
device; the local build alone cannot establish electrical or card reliability.

## Current local validation

- ESP-IDF v6.1 compile: passed.
- `idf.py size`: passed. Linked image total is 308,116 bytes. Flash code is
  157,458 bytes, flash data is 68,440 bytes, and DIRAM use is 68,814 of
  341,760 bytes (20.14%).
- App binary: 308,240 bytes (`0x4b410`), leaving 71% of the standalone 1 MiB
  app partition free.
- Launcher package: 308,240 bytes; minimum Launcher app allocation is 327,680
  bytes after 64 KiB alignment.
- Native pack v2 generation, host validation, and format tests pass. The pack is
  132,284 bytes with payload CRC32 `933c9b82` and SHA-256
  `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188`.
- The corrected initial view has bounds `(10,10)` through `(20,20)`, center tile
  `0x91`, 20 distinct tile records including the Avatar, and RGB565-LE viewport
  CRC32 `980473ac`.
- The Milestone 4 color correction changes only the ST7789 MADCTL color-order
  bit: the native framebuffer remains standard RGB565 and the asset pack is
  unchanged. The prior `0x68` selected BGR and swapped red/blue; the corrected
  landscape value is `0x60`, matching LilyGO's T-Deck setup.
- Hardware status: Milestone 3 asset validation and the preceding display, SD,
  shared-SPI, and Launcher behavior were verified by the user. The corrected
  Milestone 4 colors still require physical-device visual verification.

## Hardware validation checklist

Install the Milestone 4 Launcher image, attach a 115200-baud monitor, and verify:

- Serial reports `CPU frequency: 240 MHz`.
- Screen orientation is landscape; the 176x176 viewport appears at native 1:1 scale.
- With a FAT-formatted card inserted, status shows `SD: OK`; serial prints
  card information and `Shared SPI interleave test passed`.
- With a generated v2 pack at `/ultima5/openu5-assets.bin`, serial reports
  `Iolo's Hut (location 13 floor 0, 32x32 local)`, coordinates, viewport bounds,
  center terrain/Avatar IDs, read count, duration,
  heap measurements, and viewport CRC32.
- Visually confirm the Iolo's Hut tile orientation and the Avatar centered at `(5,5)`.
- With the pack absent or deliberately corrupted, serial reports that condition
  and heartbeats continue without rendering or rebooting.
- Without a card, screen ends at `SD: FAIL`, serial explains the error, and
  heartbeats continue.
- Reset back into Launcher afterward to confirm Launcher remains available.

Do not claim Milestone 4 visual correctness until the rendered screen passes on
the physical T-Deck Plus.
