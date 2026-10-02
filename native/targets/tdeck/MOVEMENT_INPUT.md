# T-Deck movement input

This layer changes only how physical T-Deck controls become existing semantic
UI actions. It does not change movement, combat, turns, passability, commands,
or any other native/core rule.

## Mic key

The Mic key is recognized by the physically verified raw matrix position `(0,6)`, independently of its
vendor `$` character label and the active keyboard layer.

- Press and release in under 1.1 seconds: emit one semantic `Cancel` action on
  release. This is the Escape/Back action in gameplay and the developer menu.
- Hold for 1.1 seconds: toggle Movement Mode immediately. The later release is
  consumed, so it cannot also emit Cancel.
- Repeated matrix snapshots cannot toggle twice because only raw edges begin a
  gesture and each hold has a handled latch.
- If an I2C read fails, the raw keyboard layer emits a resynchronization event.
  The adapter abandons any in-progress Mic gesture; an orphaned release can
  neither toggle nor cancel.

## Movement Mode and context safety

While Movement Mode is enabled and active, `W/A/S/D` emit the same semantic
North/West/South/East actions as the trackball. A cyan `MOVE` indicator appears
beside the current UI mode.

Movement Mode is active only in exploration and dungeon modes. It is suspended,
not forgotten, in text entry, numeric entry, dialogue, shops, selections,
special screens, the developer menu, and combat. Consequently literal keyboard
input cannot become movement in those contexts; returning to exploration or a
dungeon resumes the user's enabled setting.

Combat deliberately retains the existing controls: trackball directions are
combat movement/target directions and letter keys keep their normal combat
command meanings. WASD is not remapped during combat.

## Trackball filtering

*Superseded on 2026-10-01 by A4-ENH1 (below); kept as the record of the
earlier window.*

The GPIO reader still emits one event per falling switch edge. The semantic
controller keeps a separate timestamp for each direction and rejects only a
same-direction edge within 35 ms. This replaces the 65 ms window, allowing a
new deliberate edge at 35 ms (about 28 detents/second) while short 1-2 ms bounce
edges remain suppressed. Opposite and perpendicular directions never suppress
one another.

## A4-ENH1 (2026-10-01): the click, and speed levels instead of a window

Details, numbers and evidence: `ALPHA4_UI.md` section 10.

**What the hardware gives.** The ball has no X/Y counter. Each of the four
direction pins (up GPIO 3, down 15, left 1, right 2) pulses once per few degrees
of roll; the input task wakes on every falling edge (GPIO interrupt, 1-tick
fallback poll) and queues one raw event per pulse, timestamped. There is no
hardware delta, scaling or acceleration anywhere below the semantic layer.

**What the old "Trackball %" did.** It set only a same-direction minimum gap
(48 ms at 25 %, 12 ms at 100 %, 4 ms at 300 %); every pulse that passed was one
step. Any roll slower than about 21 pulses a second therefore stepped on every
pulse at every setting: the setting capped the rate, it was never a gain. That
is why even 25 % felt fast.

**Now (`InputController::normalize`).** pulse -> click guard (60 ms after a
press-switch edge) -> same-direction bounce window -> step gap (pulses within
it are dropped, so a flick cannot queue moves that play out after the ball
stops) -> a per-axis accumulator: N pulses on one axis make one step, the
remainder is kept; a pulse the other way clears the count (no unwinding); 400 ms
without a pulse on that axis clears it too (a tiny roll moves nothing). The two
axes count separately, so a diagonal roll alternates its two directions. No
acceleration. The Settings row "Trackball speed: N/10" picks the row of
`kTrackballLevels` (PROVISIONAL, to be tuned on hardware):

| Speed | Pulses per step | Step gap | Bounce window |
|---|---|---|---|
| 1 | 8 | 250 ms | 4 ms |
| 2 | 6 | 200 ms | 4 ms |
| 3 | 5 | 160 ms | 4 ms |
| 4 | 4 | 130 ms | 4 ms |
| **5 (default)** | 3 | 100 ms | 4 ms |
| 6 | 2 | 80 ms | 4 ms |
| 7 | 1 | 50 ms | 12 ms (close to the old 25 %) |
| 8 | 1 | 30 ms | 12 ms |
| 9 | 1 | 20 ms | 12 ms |
| 10 | 1 | none | 12 ms (exactly the old 100 %) |

`settings.json` gains `trackballSpeed` (1..10); a file without it takes the
default whatever its old percentage said (the model changed), and
`trackballResponsiveness` is still written, unchanged, for older firmware.

**The click.** GPIO 0 (the trackball's press switch, BOOT strap) is read on both
edges. One physical press toggles Movement (WASD) Mode on the press edge
itself -- no hold, no delay -- with "WASD Mode: ON/OFF" in the transcript. A
press within 30 ms of a release is bounce, a second toggle needs 150 ms, a held
press never repeats. It is a device control on every screen (handled beside the
A3-05 mutes), never a key: no Confirm, no menu action, no direction. It is not
saved; "Movement default" in Settings stays the boot value. The 1.1 s Mic hold
still toggles the same mode.

**Movement Mode contexts, as the code has them** (`ui_input_adapter.cpp`
`is_movement_context`): exploration, dungeon, combat and target selection, plus
any phase that accepts a direction (shop and equipment pickers). The section
above predates combat and targeting being added.

**Diagnostics.** Developer > Diagnostics > "Trackball stats (live)": pulses and
steps per direction, drops by reason (bounce, step gap, after-click), cleared
partial steps (idle, reversal) and the count held now, same-direction gaps,
clicks, and the last six rolls; reading it starts a new window. One
`TRACKBALL_GESTURE` serial line per finished roll; no per-pulse logging.
