# BOOT2 — T-Deck startup 57.8 s → 10.7 s (closeout record)

Status: **hardware PASS, committed, not pushed** (2026-10-06). Scope: boot/startup only. No gameplay, UI, save-format or
resource-pack change; the resource pack is still fully CRC-validated on every boot.

## Why boot took 58 s

No timeout, retry or network wait exists in the startup path. The SD card ran at **800 kHz** (`kSdClockKhz`, copied from
LilyGO's factory UnitTest), ≈ 85–100 KB/s, and the 2.27 MB resource pack is read **twice**: once to CRC-validate
(`alpha-resource-open-validate`, 26.7 s) and once to load (`alpha-runtime-initialize`, 20.7 s). About 48 s of 58 s was SD
bandwidth. The two long screens the user saw were the diagnostics screen (30.0 s, over the validation) and the identity
screen (23.3 s, over the load).

## Baseline (RC5 image + timing markers, 800 kHz, 3 cold resets, spread < 10 ms)

| Stage | ms |
|---|---:|
| serial-ready delay | 1,493 |
| display init (incl. 500 ms power settle + ST7789 delays) | 923 |
| SD mount + read-back test | 545 |
| diagnostics screen draw | 202 |
| tile pack open/validate (132 KB) | 3,269 |
| resource pack open/validate (2.27 MB) | 26,740 |
| identity screen draw | 539 |
| identity hold (fixed) | 1,795 |
| runtime load (alpha-runtime-initialize: ownership 16,282, dungeon art 2,080, tile cache 798, save slots 1,399) | 20,670 |
| audio pack | 799 |
| first render | 92 |
| **reset → first screen** | 3,138 |
| **diagnostics screen on screen** | 30,019 |
| **identity screen on screen** | 23,269 |
| **reset → first interactive frame** | **57,807** |

(Log timestamps include ~0.70 s of ROM + bootloader before `app_main`; "reset →" figures add it.)

## SD clock sweep (cold boot, 3 per clock, old fixed waits still in place)

| Clock | Mount | Pack validate | Reset → first frame | Notes |
|---|---|---:|---:|---|
| 800 kHz | ok | 26.7 s | 57.8 s | baseline |
| 4 MHz | ok | 7.7 s | 20.3 s | clean |
| 10 MHz | ok | 4.9 s | 14.6 s | clean |
| **16 MHz** | ok | 4.2 s | 13.2 s | clean — **selected** |
| 20 MHz | ok | 3.9 s | 12.8 s | clean, one divider below the cliff |
| 26.7 MHz | **fails** | – | ~59 s | `ESP_ERR_INVALID_RESPONSE` reading the CSD; fell back to 800 kHz and booted fully |
| 40 MHz | **fails** | – | ~59 s | same |

Throughput saturates well below the clock (10 → 20 MHz saves only ~1.9 s), so 16 MHz (the 80 MHz / 5 divider) costs 0.47 s
against 20 MHz and keeps a wider timing margin on the SPI bus the TFT shares. The 26.7 / 40 MHz runs are the
**hardware proof of the mount fallback**.

## Final policy (`main/sd_clock_policy.h`, `Board::initialize_and_test_sd`)

- Mount at `kSdFastKhz = 16000`; run the existing read/write/read-back test.
- If the mount or the test fails: unmount, log `SD_CLOCK fallback: <khz> kHz failed (<err>); remounting at 800 kHz`, remount
  at `kSdSafeKhz = 800`. The safe clock has no further fallback.
- `main.cpp`: if either resource pack fails to open/validate on the fast clock, `Board::fall_back_sd_clock()` remounts at
  800 kHz and validates once more. A pack that still fails is **not** accepted (`packs_match` stays false, startup is
  blocked exactly as before). This pack-revalidate path was verified by code review and the host test only; the mount
  fallback is the part exercised on the device.

## Fixed waits

| Wait | Before | After |
|---|---:|---:|
| serial-ready | 1,500 ms | 300 ms |
| identity-screen hold | 1,800 ms | removed (the screen stays up through the real runtime load) |
| peripheral power settle | 500 ms | unchanged |
| keyboard rail settle (new) | – | up to 1,700 ms after the rail is enabled; ≈ 450 ms in practice |

### Keyboard-settle finding (do not remove)

The keyboard coprocessor needs ≈ 1.7 s after the GPIO 10 peripheral rail comes up before its first I²C snapshot. The slow
SD mount used to provide that by accident. With only the SD clock raised (old waits) 6 of 20 boots logged
`KEYBOARD_ERROR initial_snapshot=ESP_ERR_INVALID_RESPONSE` (the existing recovery then made it usable); with the waits also
cut, 4 of 30; the 800 kHz image: 0 of 20. `main.cpp` now waits out only the remainder of 1.7 s measured from just before
`initialize_display()` (the rail is enabled at its start): **0 of 30** after. Any later boot speed-up must keep
rail-enable → keyboard-init ≥ 1.7 s.

## After (final logic, 12 cold resets)

| | Before | After |
|---|---:|---:|
| reset → first screen | 3,138 ms | 1,936 ms |
| diagnostics screen on screen | 30,019 ms | 5,126 ms |
| identity screen on screen | 23,269 ms | 2,705 ms |
| resource pack validate | 26,740 ms | 4,158 ms |
| runtime load (alpha-runtime-initialize) | 20,670 ms | 2,484 ms |
| **reset → first interactive frame** | **57,807 ms** | **10,683 ms** (min 10,679, max 10,691) |

Saved 47.1 s (81.5 %).

## Reliability

- **12-boot soak** (final logic, software resets): 12/12 reached the ready title state; pack CRCs passed; SD at 16 MHz with
  no fallback; `SAVE_CATALOG slot1/2/3=verified` (22,901 B) every boot; audio pack valid; no watchdog, no panic, no
  `E (` lines; the only warning (`ledc: GPIO not usable`) also appears in the baseline. Keyboard: 0 initial errors.
- **Hardware PASS (reported by the owner, exact final Launcher image):** installed via the Launcher; true cold power-on;
  boot screens free of corruption; title/menu; load game; save game; no visible corruption or startup failure.
- Not exercised on hardware: the pack-revalidate fallback (only the mount fallback).

## Tests and builds

- Host: **202 / 202** serially (the previous 201 plus the new `boot2_sd_clock_runtime`, which drives the real `Board`
  mount/fallback loop against the fake bus's mount seam; mutation-checked: dropping the unmount, and a policy that never
  falls back, are both caught).
- Firmware, `-Werror`, ESP-IDF 6.1: clean, 0 warnings. Hardware-validated image (with the uncommitted UI5 working tree):
  1,049,520 B (0x1003b0), 261,200 B (19.9 %) free in the 1.25 MiB app partition; DIRAM 136,054 B and IRAM 16,384 B
  unchanged versus the BOOT1 image. Launcher allocation 1088 KiB (informational). Launcher file
  `OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin`, SHA-256 `35903e0390199886240dbf441f53885a1c482bb3ece08ae76194fcadf5b564b2`,
  identity line `Git 8ac06c746b18-dirty` (the shared RC5 file name; the `-dirty` Git line identifies it).
- The commit's own content (HEAD + BOOT2 hunks, no UI5) was rebuilt separately before committing: firmware `-Werror` clean,
  1,048,848 B (0x100110), 261,872 B (20.0 %) free; host suite re-run on that tree (see the commit report).

## Method notes

- Timing: `debug51::Step` `BOOT_TRACE` lines + one `BOOT_SUMMARY first_frame_us=` line (main.cpp), serial 115200 on the
  USB-Serial-JTAG port, hard reset via esptool's RTS sequence. "Reset →" adds the ~0.70 s before `app_main`.
- On the device the game lives in the Launcher-made `ota_3` ("openu5", 0xa10000, exactly 1 MiB). An image above 1 MiB
  (this one, 1,049,520 B) is refused by the bootloader in that slot; direct-flash soak builds used `-Os` on `main.cpp` and
  `tdeck_board.cpp` (code-identical, −4.4 KB) and the real image went in through the Launcher.
- The sweep images were built with a temporary CMake hook (`-DOPENU5_SD_SWEEP_KHZ=<n>`, removed); nothing of it remains.
