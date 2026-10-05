# Milestone 5.1 stack-overflow correction

Introduced in `0.5.1-stackfix` and retained by `0.5.1-input-final`.
Build/package verified; physical stability and actual
high-water margin are pending device capture. No keyboard lockup fix is claimed.

## Cause and measured evidence

The supplied physical logs explicitly identify main-task stack overflow, not a
keyboard reset or ordinary screen repaint: 3,584-byte task stack, approximately
584 bytes historical minimum free before asset validation, then panic and reboot.

Disassembly of the saved `build` (version 0.4.0) gives a 240-byte app_main frame;
the previous `build-m5` diagnostic binary used 768 bytes. The additional 528
bytes remain resident through all nested calls. M5/M5.1 added the keyboard matrix,
35-entry event queue, controller and diagnostic state to app_main's locals.
InputHardware alone is 376 bytes, including the 280-byte keyboard event queue.
The input controller is 40 bytes. Diagnostic variadic logging also adds nested
formatting/driver call depth; its exact total cannot be inferred from a single
direct function frame or the supplied stage marker.

Asset CRC streaming already used a 1,024-byte local array in M4. Together with
app_main (768), AssetPackReader::open (592) and crc_range (1,056), the diagnostic
validation path consumed 2,416 bytes in just these three frames, before CRC
helpers, stdio, FAT, SD, SPI and their logging. Display routines also already
used 640-byte local rows. M5's resident growth exhausted this tight inherited
budget. The stage marker locates the failure in validation; without a decoded
crash backtrace it does not identify the exact library instruction that crossed
the stack boundary. No large custom printf buffer was found in diagnostics.

Actual compiler-generated direct frames, in bytes (Xtensa `entry` instructions):

| Function | Previous diagnostic | Stack correction |
| --- | ---: | ---: |
| app_main | 768 | 288 |
| AssetPackReader::open | 592 | 592 |
| crc_range | 1,056 | 48 |
| Board::fill_rect | 752 | 96 |
| Board::draw_rgb565 | 752 | 112 |

The three-frame validation subtotal falls from 2,416 to 928 bytes, a 1,488-byte
reduction, independent of the stack-size increase. These are direct frames, not
a complete worst-case call-chain bound.

## Allocation changes and budget

- Board, InputHardware, InputController, AssetPackReader and AssetPackReport now
  have app-lifetime static storage in internal RAM. Their emitted object sizes
  are 648, 376, 40, 20 and 36 bytes respectively. Static initialization preserves
  the prior initialization order and keyboard state; trackball events never
  reset these objects.
- Both display operations reuse one aligned 640-byte Board member. The existing
  synchronous SPI transfer completes before the row can be reused. Byte order,
  transmitted lengths, bus arbitration and display/SD clocks are unchanged.
- CRC validation allocates a 1,024-byte heap scratch array once per open, reuses
  it for all section/payload CRCs, and frees it on all exits through RAII. Failed
  allocation returns ESP_ERR_NO_MEM; no shared global CRC state is introduced.
- The existing 61,952-byte viewport remains in PSRAM.
- Inspected and retained bounded validation locals: 148-byte header, two
  100-byte section tables, 8-byte initial record and 128-byte sample tile.
  Renderer scratch remains bounded: 32-byte palette, 121-byte map window,
  244-byte required-tile IDs and 128-byte indexed tile (608-byte direct frame).
  SD verification's two 160-byte arrays and small raw matrix snapshots remain
  local. No unbounded local allocation or new recursion was found.

Main task stack: **3,584 -> 12,288 bytes** (12 KiB), set in sdkconfig.defaults and
the active sdkconfig and verified in generated build-m5/config/sdkconfig.h.
The extra 8,704 bytes intentionally budget nested FAT/SD/SPI and diagnostic
formatting calls after removing avoidable stack allocations. Expected normal
remaining margin is more than 6 KiB, an engineering estimate, not a measurement
or proven worst-case bound. Hardware acceptance requires at least **4,096 bytes**
historical minimum free throughout startup and sustained ordinary runtime.
Only a few hundred bytes free is a failure, even if no panic occurs.

## Physical stability capture (before any keyboard investigation)

Record full USB serial at 115200 baud from launch through at least ten minutes
of idle and ordinary trackball movement/redraws. Repeat several launches.

- Verify version `0.5.1-input-final` and `main_stack=12288`.
- Require `STACK` checkpoints: app_main-entry, after-board-display-init,
  after-sd-init, after-keyboard-init, before-asset-validation,
  after-asset-validation, after-renderer-init, after-game-init and
  running-input-loop (every five seconds).
- Require `ASSET validation-return result=ESP_OK` and
  `RENDER initial-return result=ESP_OK`, with the expected viewport displayed.
- Require all `free_min_bytes >= 4096`, no `STACK LOW`, no panic/watchdog,
  repeated app_main-entry or flashing startup-screen loop. The stack values are
  ESP-IDF bytes and historical minima, not instantaneous free stack.
- Keep BOOT reset reason, retained boot count and prior stage in the recording.
  The first launch may report a prior panic; subsequent unexpected panic boots
  fail validation. RTC retention is best effort across power/Launcher changes.

If assets are absent or initialization fails, result logs report the error;
reaching the input loop alone does not prove successful validation/rendering.
No physical device run was available for this build. Keyboard raw mode remains;
the later final input cleanup removes keyboard direction bindings by policy.

## Build and package

ESP-IDF v6.1 build completed successfully. Existing framework configuration
warnings remain; packaging validated the app header, checksum, appended digest
and byte-identical Launcher copy. No packaging script or asset format changes.

- Launcher: `build-m5/launcher/OpenU5-TDeck-M5-Launcher.bin`
- Source: `build-m5/openu5_tdeck.bin`
- Size: **339,728 bytes** (`0x52f10`)
- SHA-256: `5784cc45d38fdd1d50d17a3debf73737a8ff08ed70367836fa3d16ccd7e0cb01`
- Minimum aligned Launcher allocation: 393,216 bytes (384 KiB).

Files changed in this stack-only pass: CMakeLists.txt, sdkconfig.defaults,
active generated sdkconfig, main/main.cpp, main/boot_trace.h,
main/asset_pack.cpp, main/tdeck_board.h, main/tdeck_board.cpp, README.md,
LAUNCHER.md, DEBUG51.md and this report. Generated build/package outputs updated.
Earlier uncommitted Milestone 5/5.1 work is preserved. No gameplay scope added.
