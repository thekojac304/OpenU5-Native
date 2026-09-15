# Milestone 5.1 physical diagnostic build

Superseded by `0.5.1-stackfix`: physical logs confirmed main-task stack overflow.
Use [STACK51.md](STACK51.md) and its stability-only capture first. The package
at the path below has been replaced; the size/hash below are historical.
Do not resume this keyboard investigation until the stack margin is verified.

Version: `0.5.1-debug`. Both reported failures remain unresolved pending device
traces. This build instruments the current behavior; it does not reset keyboard
state on trackball events, change movement, change trackball debounce, or add
delays. Existing keyboard read-error recovery is retained and logged.

Launcher image: `build-m5/launcher/OpenU5-TDeck-M5-Launcher.bin`, 335,824 bytes
(`0x51fd0`), SHA-256
`b3d138d6bc5aae4e29c58c30bd24076f2433799510d8821ffee6b98fde21011b`.
The existing packager validates the app checksum/digest and byte-identical copy.
No packaging format or installation architecture changed.

## Capture procedure

Record the complete 115200-baud USB serial session to a file, beginning before
launching the app. Preserve ESP bootloader, panic, watchdog and backtrace output,
including across USB disconnect/reconnect. Do not filter to movement lines.

1. Boot through the repeated startup screens and reach the game.
2. Make several successful Symbol+number moves, including alternating directions.
3. Continue until it stops. Release all keys, then press/release Symbol and the
   direction again. Leave several seconds of unchanged polling in the recording.
4. Make exactly one trackball move, then try the same keyboard chord again.
5. Stop the capture after several subsequent keyboard attempts. Note the rough
   wall-clock times of failure and trackball recovery in the report.

Detailed logging stays enabled throughout this dedicated debug firmware. It can
alter polling timing and CPU scheduling; if the failure disappears, report that
too. It is evidence of timing sensitivity, not proof of a fix.

## Line reference

All instrumentation uses tag `M51`. `t`, `event_t`, `gap`, and `io_us` are
microseconds. ESP's ordinary log prefix is milliseconds. `n`/`poll` correlate
the I2C sample with matrix transitions and queue entries.

- `POLL`: every actual I2C receive, result, requested length, returned buffer,
  prior decoder state, synchronization, read duration, time since previous read,
  and observed keyboard INT GPIO46. On error the buffer is invalid (possibly
  partial) and is explicitly marked `valid=0`. The IDF receive API does not
  expose an actual slave-response byte count, so `requested=5` is not a claim of
  five meaningful bytes or proof the controller supports raw mode.
- `MATRIX`: five decoded columns plus press/release bit masks before baseline
  suppression. In every column hex byte, bit 0 is row 0, bit 6 is row 6; 1 means
  down. `S` is Symbol c0r2, `A` Alt c0r4, `L` Shift c1r6, `R` Shift c2r3.
  Raw bit 7 remains visible in POLL; the existing decoder masks it.
- `QUEUE`: generated event count or exact reason no edges were queued:
  unchanged, initial baseline, resynchronization baseline, or failed read.
- `EDGE`: each queued physical coordinate, resolved code, press/release,
  modifiers and modifier-key identity. `DEQUEUE` records deferred queue delivery.
- `ACTION`: delivered event and timestamp, accepted/rejected, exact controller
  reason, direction or none, and movement readiness. No keyboard debounce exists
  in this version. Releases, missing Symbol, and unmapped codes are distinguished.
- `TB`: physical trackball direction/time, pending keyboard queue and last read
  time. Trackball return defers the next keyboard read as in the previous build.
- `MODE`: every initial/retry 0x03 write, count, result. A successful write only
  establishes I2C ACK, not firmware capability. No TFT operation is in this path.
- `REDRAW begin/end`: duration during which the single app task cannot poll.
  Keep normal `Move ... blocked/passable` lines too: accepted input and legal
  world movement are separate decisions.
- `HEALTH`: minimum remaining main-task stack space. The main stack remains
  3584 bytes, as previously configured; instrumentation also uses stack space.

## Boot versus screen initialization

`BOOT app_main-entry` reports numeric/named `esp_reset_reason()`, uptime, and a
best-effort RTC boot counter and prior stage. It is emitted before the existing
1500 ms startup delay. `STAGE serial-ready-M51-DIAGNOSTIC` repeats the boot count
after that delay. The counter makes no flash writes and may be lost on cold
power, brownout, or Launcher activity; it is supporting evidence only.

Stages: 1 app entry, 2 serial ready, 3 board construction, 4 display initialization,
5 SD initialization, 6 startup-screen draw, 7 keyboard entry, 8 keyboard return,
9 asset validation, 10 initial render, 11 game-screen transition, 12 input loop.
Shared-SPI entry also logs whether it was already initialized.

A new ROM/IDF boot sequence and new `BOOT` entry with restarted uptime indicate
a real reboot. Use the reset reason plus the preceding panic/watchdog/brownout
output to identify its cause. Repeated display or startup-screen STAGE entries
with continuous uptime and no new BOOT indicate repeated application/UI calls.
`prior_stage` can locate a reset before it reaches the next stage. If the device
resets before app_main, the ROM/IDF output is essential.

## What source inspection establishes

The prior report that shared debounce was the exact lockup cause is withdrawn.
The old gate lasted only 120 ms since the last accepted keyboard event; it could
drop a rapid repeated press but did not establish a persistent latch. The current
controller has no keyboard time gate or trackball-triggered keyboard reset.

The published [LilyGO controller source](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/Keyboard_ESP32C3/Keyboard_ESP32C3.ino)
sets persistent rawMode with 0x03 (until 0x04/reset). Reads return five columns
from lastValue, without consuming edges or requiring an application ACK. The
sketch has no interrupt output/clear protocol and no held-modifier read gate.
Scanning proceeds column/row sequentially, with a 1 ms wait per cell (35 cells).
The response is not an atomic completed scan; modifier/key transition ordering
is therefore not guaranteed. This describes the published sketch, not proof
that the installed keyboard MCU runs that revision. No version/capability
handshake is provided. INT is observed only, never used as a polling gate.

The native app polls and redraws synchronously in app_main. It does not create
an input task. Complete press/release cycles during redraw or queue draining
can be missed by state sampling. Read failures discard synchronization; the next
good sample suppresses all initial edges. Both paths need measured traces.

`CONFIG_FREERTOS_HZ=100` makes `pdMS_TO_TICKS(5)` zero. Thus the existing input
loop does not request a positive blocked interval. `SCHED` logs this explicitly.
Idle-task starvation is a watchdog hypothesis, not an established reset reason.
Timing is retained for reproduction. Keyboard initialization adds I2C allocation
and matrix/queue storage to the main task; stack headroom is now measured.

There is one display initialization call and one startup-screen call in app_main;
neither is inside keyboard retry handling. The initial game-screen transfer
clears the TFT once. Later movement only draws the viewport/status. Source alone
does not explain five or six startup cycles; capture the reset and stage lines.

Host snapshot models cannot validate the installed keyboard firmware or either
reported physical failure. No hardware fix is claimed by this diagnostic build.
