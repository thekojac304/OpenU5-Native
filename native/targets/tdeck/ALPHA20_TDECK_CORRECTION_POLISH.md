# Alpha 2.0 T-Deck correction and polish pass

Date: 2026-09-17

This pass changes only T-Deck input transport, input tuning, frontend layout,
HUD presentation, and shop presentation. It does not add audio, touch controls,
Ultima IV transfer, or new gameplay rules.

## 1. Keyboard freeze root cause and fix

The physical trace and the transport implementation identify a two-stage
failure. A long synchronous frontend SPI render can delay keyboard servicing
long enough for a raw snapshot read to fail. The old recovery path then turned
that transient into a permanent failure: it retained the failed ESP-IDF I2C
master-bus state, backed reads off to 250 ms, and repeatedly sent the raw-mode
command on the same wedged bus. Those commands could not resynchronize the
controller and extended the `ESP_ERR_INVALID_RESPONSE` streak.

The transport now allows one read retry after one complete 40 ms keyboard
matrix scan. If that retry also fails, it resets the ESP-IDF master bus, waits
10 ms, sends the raw-mode command once, waits another full matrix scan, and
accepts the first valid five-byte snapshot as a quiet baseline. Failed or
malformed snapshots never merge into the stable key state. INT may request a
poll, but can no longer bypass a recovery deadline.

## 2. Keyboard recovery and diagnostics

Recovery is staged and bounded rather than command-spammed. A transient read
may recover on the next scan without changing mode. A persistent failure
performs one bus reset and one raw-mode re-entry. Only a failed recovery cycle
uses the 250 ms backoff; normal service resumes immediately after a valid
baseline. No reboot is required.

Focused diagnostics now include `KEYBOARD_ERROR`, `KEYBOARD_RECOVER`,
`INPUT_SERVICE_GAP`, `CHAR_NAME`, `CHAR_GENDER`, `FRONTEND_RENDER`, and a
five-second `KEYBOARD_METRICS` line containing reads, errors, per-mille error
rate, recovery count, service-gap high-water mark, and recovery state. The SD
diagnostic logger retains these records.

Before this fix, every observed read after the first-character fault remained
in the physical error streak and no recovery completed. The prior physical
service-gap high-water mark was 210 ms. Post-fix host fault injection verifies
both a single failed read followed by a usable snapshot and a repeated-fault
bus-reset/raw-reentry recovery. A numerical post-fix physical error rate and
service-gap maximum are deliberately left pending the device retest; the new
metrics report both without inference.

## 3. Trackball percentage setting

`Trackball Speed` is now a persisted 25%-300% setting in 25% increments. The
existing settings key is retained and legacy Slow/Normal/Fast values migrate to
50%/100%/200%. The shared UI input adapter applies the setting to gameplay,
menus, character creation, and shops.

The mapping is deterministic and changes debounce/repeat acceptance rather
than dropping events:

| Speed | Direction debounce |
| ---: | ---: |
| 25% | 48,000 us |
| 50% | 24,000 us |
| 100% | 12,000 us |
| 200% | 6,000 us |
| 300% | 4,000 us |

Thus 100% restores the earlier responsive 12 ms behavior, while the extremes
produce materially different rates without synthesizing extra moves.

## 4. HUD layout and celestial/wind bars

The 176x176 authoritative viewport remains at `(4,4)`. A thin cyan U5-style
frame now surrounds it. A nine-pixel sky strip is composited at its top edge
using authoritative sun positions and moon phase symbols. A nine-pixel wind
strip is composited at its bottom edge from `TurnState.wind`. These strips
redraw only when the viewport overwrites them or the associated state changes;
they do not advance time or RNG.

The future touch-control area remains exactly `(4,184)` through `(179,239)`
(176x56), below the viewport frame. No celestial, wind, shop, prompt, or debug
text is drawn there.

The right side now has framed party and world areas plus a transcript
separator. The six party rows show active member, name, HP/max HP, and
condition. World information is limited to player-facing location, day, and
time. Coordinates and internal UI mode names remain absent outside developer
tools. Sky and wind are no longer duplicated in the world block.

The transcript uses the native 5x7 glyphs in 6x8 cells: 22 columns and up to 19
gameplay rows, with word-aware wrapping and the active prompt/input at the
bottom. Shop mode reserves six offer rows and eight transcript rows.

## 5. Shop presentation

The hybrid UI was caused by two independent presenters. `UiSession` appended
synthetic `Choice A`, `Choice B`, and similar rows to the authoritative
transcript, while the T-Deck runtime independently rendered the selected offer
in a detached bottom overlay. The shop cursor was also clamped to a generic
25-row ceiling rather than the current offering count.

The T-Deck now uses one `DeviceShopView` derived from the existing authoritative
shop state. The right panel contains the shop/keeper header, phase, six
paginated offer rows, real item or service names, prices and quantities, a
visible selection, gold, action hints, the preserved dialogue/result
transcript, and integrated text/numeric input. Buy, sell, healers, inns,
taverns, guilds, reagents, ships, and party-member service lists share this
presentation. `B`, `S`, Enter, Mic, and trackball/direction retain the existing
semantics. No offer IDs, generic Choice labels, or detached selection text are
player-facing.

The authoritative shop tables, pricing, inventory changes, dialogue, and
service outcomes were not changed.

## 6. Character-creation placement

Name editing now draws at y=116, below the reference title art which ends at
y=114. Questionnaire copy begins below its reference artwork at y=154. Title
and fire animation updates are suspended during name, gender, and questionnaire
input, eliminating unrelated redraw pressure and preventing typed text from
appearing over the artwork. Character-creation semantics are unchanged.

## 7. Dirty rendering and measured cost

The viewport has a content CRC, so a shop selection change with an unchanged
world does not retransmit all 30,976 viewport pixels. A same-page shop move
redraws only the old and new 134x12 rows (3,216 pixels), plus a footer/status
region only when its content changes. Character name editing redraws a 280x9
text region (2,520 pixels), and no longer performs the periodic 14,112-pixel
title-fire redraw while text entry is active. There are no SD reads or resource
conversions in these render paths.

The final compiler stack-usage files report:

- `AlphaRuntime::render`: 2,128 bytes (unchanged)
- `Board::show_alpha`: 416 bytes (+48 bytes)
- `InputHardware::poll`: 176 bytes
- `AlphaRuntime::compose_shop_view`: 64 bytes

The main task remains 24,576 bytes. The new retained view/cache and transport
state add approximately 956 bytes of internal static storage across the board,
runtime, and input owners. The existing 325,706-byte PSRAM allocation is
unchanged. Runtime free-RAM and physical stack high-water values must be read
from the device; no physical result is claimed here.

## 8. Validation and package

- Warnings-as-errors ESP-IDF 6.1 build: passed.
- Final host/reference suite: 53/53 passed, including Alpha 1.4 regressions,
  frontend/input tests, shop adapters/flow/parity, and all TypeScript drift
  checks.
- Focused coverage includes eight-character edit/submit, gender and
  questionnaire input, transient/persistent keyboard faults, 25%-300%
  trackball timing, touch-strip exclusion, celestial/wind bounds, six-member
  party fit, transcript geometry/wrapping, names/prices/pagination, buy/sell and
  service flows, Mic/Enter/direction shop navigation, exactly two dirty rows on
  a same-page selection move, absence of Choice labels/raw IDs, and
  artwork/text non-overlap.
- `git diff --check`: passed (line-ending notices only).
- Device smoke harness: compiled into the firmware with the existing scenarios;
  execution remains part of the physical retest and is not claimed.
- Firmware: 752,176 bytes (`0xB7A30`), leaving 295,376 bytes (`0x485D0`, 28%)
  in the 1 MiB app partition.
- Firmware SHA-256:
  `11FB54FC2F16B0E5C4387D6BB441673AF7967F7DFD825E905C0BBB94BAD997F7`
- Resource pack: 1,824,829 bytes, unchanged; SHA-256
  `530286B2BF2A82A20E6279F3B6261B546B800AD2A749CBF3B97B263F85051AA4`.
- Launcher path:
  `C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-correction\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

## 9. Physical retest checklist

1. Start New Journey and type/edit an eight-character name, then submit it.
2. Complete gender and the full questionnaire; confirm no title-art overlap or
   continuing fire animation during input.
3. Leave the frontend active for ten minutes, inject normal rapid typing, and
   confirm any `KEYBOARD_ERROR` is followed by a successful recovery without a
   reboot. Record `KEYBOARD_METRICS` error-per-mille and service-gap high water.
4. Compare 25%, 100%, 200%, and 300% in a menu and during gameplay; check for
   material rate changes and no bounce/double movement.
5. Inspect the six-member HUD, top sun/moon strip, bottom wind strip,
   transcript/prompt, and untouched lower-left reserved strip.
6. Exercise buy, sell, inn/healer/service, pagination, Enter, B, S, trackball,
   and Mic leave. Confirm real names/prices and transcript results, with no
   Choice labels or detached bottom offer.
7. Run the on-device smoke harness and archive the focused diagnostic lines.
