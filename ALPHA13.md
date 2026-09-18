# OpenU5 T-Deck Alpha 1.3 consolidated correction pass

Alpha 1.3 completes the remaining physical-input, transcript, combat-presentation,
enemy-identity, and actor-presentation work. It preserves the existing developer
menu layout and map-picker backend, the Alpha 1.1 display/performance path, native
movement rules, fog/visibility, live overlays, combat terrain, trackball behavior,
and the PSRAM tile cache. It adds no gameplay system.

## 1. Why Mic required Symbol

The physical microphone key is keyboard-matrix **column 4, row 4** (zero based).
LilyGO's base character table maps that switch to raw `$`; the Symbol table maps
the same switch to raw `0`. The faulty path inferred key identity from that
layer-resolved character. Consequently the plain key was treated as printable
`$`, while the Symbol-layer representation happened to reach the non-printable
cancel path.

The adapter now recognizes `(4,4)` before character or modifier interpretation.
Neither raw value is emitted as text. A short release produces one semantic
`UiActionKind::Cancel` in gameplay, modals, target selection, and the developer
menu, with no modifier required.

## 2. Why long-hold Mic did not enable Movement Mode

The same character-identity error prevented a plain Mic press from reliably
starting a hold. The old event-only route also had no reliable threshold service
while the matrix key remained down: a held switch creates no new edge, so waiting
for another keyboard event could never make WASD active at 1.0–1.2 seconds.

The runtime now services the hold state from its regular 5 ms presentation/input
pump. The threshold is 1.1 seconds, independent of keyboard repeats or modifiers.

## 3. Final Mic state machine and Movement Mode

- First physical `(4,4)` press latches the timestamp.
- Release before 1.1 seconds emits exactly one Cancel.
- Crossing 1.1 seconds toggles the persistent Movement Mode flag and sets a
  handled latch. Continued holding cannot repeat the toggle.
- The matching release after a handled hold emits neither Cancel nor a second
  toggle.
- A later hold toggles the flag again. Matrix resynchronization abandons an
  incomplete gesture so an orphaned release cannot act.
- A small `MOVE` indicator appears beside the exploration/dungeon mode label.
- While enabled, W/A/S/D become North/West/South/East only in Exploration and
  Dungeon. Text, number, dialogue, shop, combat, target, and developer contexts
  retain ordinary letters; returning to exploration resumes the enabled setting.
- Trackball input remains independent and unchanged.

Host coverage exercises a short tap, threshold timing, toggle on/off, release
suppression, repeated cycles, all four WASD directions, ordinary letters,
text/numeric isolation, resynchronization, and interleaved trackball use.

## 4. Developer-menu Back behavior

The developer menu's visual design is unchanged. The correction is the reliable
physical Cancel action feeding its existing hierarchy. Tests cover exactly:

`Developer > Teleport > destination editor > Cancel > Teleport > Cancel > Developer > Cancel > gameplay`

They also prove that backing out of the editor does not apply a teleport.

## 5. Map-picker device flow

Confirm already reached the portable picker and mutated `GameState`. Two device
presentation faults made a successful operation look inert: selected values and
the returned status were not visible, and a world/local teleport made while a
dungeon owner was active left `DungeonState::active` set. Snapshot composition
therefore kept choosing the stale dungeon over the new authoritative position.

The unchanged picker backend now flows through the device frontend with visible
chosen values and `Result: ...` text, including specific invalid-destination,
floor, coordinate, missing-data/context, active-combat, and core-rejection errors.
After an applied teleport, the device clears a stale dungeon owner for Britannia
or local maps, rebinds dungeon/combat context, refreshes terrain, restores the
correct base UI mode, dirties the presentation, and composes a fresh snapshot.

Host coverage applies Britannia, local maps, and dungeon floors, checks atomic
invalid requests, verifies continued commands after teleport, and checks the
three-level Cancel path. The ESP32-S3 integration compiles and links; actual
screen/input behavior remains part of the physical checklist below.

## 6. Transcript flashing cause and fix

The command panel cleared its entire region before replacing the text, then sent
many small per-glyph LCD transactions. It also recomposed unchanged text during
unrelated actions and animation ticks. The blank interval was therefore visible.

Alpha 1.3 keeps a bounded cache of displayed transcript rows plus status, mode,
prompt, and input text. Only changed regions are redrawn. Each changed text row is
composed as an opaque region in the existing transfer-row buffer and sent under
one display window; replacement pixels are ready before presentation, with no
intermediate panel clear. Animation-only ticks still update only animated
viewport cells and no longer touch the text panel.

## 7. Font metrics

The transcript uses the existing 5x7 bitmap font with vertical 2x scaling:

- visible glyph: **5 x 14 pixels**
- character cell: **6 x 16 pixels**
- **21 characters per line**
- **12 visible transcript lines**

This materially enlarges the text while preserving the 320x240 layout and all
semantic strings.

## 8. Word-aware wrapping

Wrapping now streams across continuation blocks without allocating another
transcript. It wraps at whitespace, retains punctuation with its token, trims
leading whitespace after a wrap, preserves explicit newlines (including empty
lines), preserves multiple internal spaces when they fit, and hard-breaks only a
single token wider than the line. Tests cover normal prose, punctuation, explicit
newlines, exact-width and overlong words, multiple spaces, block boundaries, and
a long paragraph.

## 9. Active-combatant indicator

`compose_combat_presentation` derives the marker from authoritative
`CombatState::current`. A living player actor receives yellow corner brackets; a
living enemy (including a charmed actor on the enemy turn) receives red corner
brackets. The marker follows turn changes and is omitted for dead, fled, absorbed,
skipped, missing, or out-of-grid actors. It is rendered after tiles and is
separate from the target reticle.

## 10. Targeting reticle

Existing `UiSession::TargetSelection` coordinates now enter the presentation
snapshot. The target uses an inset cyan box and center cross; an invalid/empty
Attack target is red. The side overlay reports coordinates and, when occupied,
the actor name plus current/max HP.

- Attack and targeted Cast move their retained 11x11 cell cursor with Direction,
  dispatch on Confirm, and return to combat on Mic/Cancel.
- Fire now retains an adjacent-cell reticle after Direction and waits for
  Confirm. It then dispatches the same authoritative directional Fire command;
  no core targeting or legality mechanic was added. The center/start state is
  visibly invalid until a direction is chosen. Mic cancels without firing.
- Active-actor corners and target box/cross use different geometry and colors.

## 11. Giant Rat / Giant Spider root cause

Sprite/type identity was correct: enemy definition 20 selects sprite family
`320 + 20 * 4 = 400`, the Giant Rat art. The Alpha resource pack incorrectly
indexed `monsterNamesMixed` with the definition index. That compressed name pool
omits definitions 8, 9, 42, and 43; after the first omissions, direct index 20 is
`Giant Spider`, shifting the death-event/display name away from the actor type.

Packing now resolves the compressed pool with both omission offsets and uses the
authoritative additional-enemy name data for omitted definitions. It is not a
name special case. Regression coverage samples definitions across both gaps,
including Mage, Pirates, Giant Rat, Giant Spider, Skeleton, Dragon, field,
whirlpool, Corpser, and Shadow Lord. The corrected SD resource pack must be
copied along with the firmware.

## 12. Actor animation root cause and fix

Terrain carried animation metadata, but actor cells did not. High-bank actor
tiles were also being treated like a simple turn-number tile cycle, so idle
presentation ticks neither revisited actors nor followed the reference per-actor
programs.

Snapshots now retain stable actor identity and seed, mark NPC/enemy/combatant
cells as presentation-animated, and run the reference-style actor programs on a
local deterministic view clock (approximately 110 ms). The view PRNG is isolated
from gameplay RNG. The Avatar and transports do not receive invented idle
cycling: their authoritative movement/facing/transport tile changes remain driven
by core state. Tests cover world actors, combatants, stable identity, and gameplay
RNG isolation.

## 13. Firmware-size delta

- Alpha 1.2 packaged app: 654,960 bytes
- Alpha 1.3 packaged app: **658,384 bytes**
- Delta: **+3,424 bytes**
- ESP-IDF accounted image: 658,268 bytes
- App partition: 1,048,576 bytes; **390,192 bytes (37%) free**

## 14. RAM, PSRAM, and stack impact

- DIRAM: **91,962 / 341,760 bytes**, up **1,360 bytes** from Alpha 1.2, primarily
  the bounded coherent transcript/status caches.
- Persistent PSRAM allocation: unchanged. The existing RGB565 viewport and tile
  cache remain the only large presentation owners affected by this path.
- Main task stack allocation: unchanged at **12,288 bytes**.
- `PresentationSnapshot`: 1,100 bytes, up 4 bytes for marker/reticle metadata.
- Word wrapping uses bounded local line/token buffers (200 bytes total payload),
  and the device renderer uses the existing 640-byte transfer row rather than a
  new panel framebuffer. The existing heartbeat remains the physical high-water
  guard: at least 4 KiB stack margin and 32 KiB internal RAM are required.

## 15. Packaged artifacts

Launcher binary:

`C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha12-fw2\launcher\OpenU5-TDeck-Alpha1.3-Launcher.bin`

- Size: 658,384 bytes
- SHA-256: `876c835aa01995b224b0c31e17f2c638eda9ba613dcb53e69f262fe0839a702b`

Corrected Alpha resource pack for the SD card:

`C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`

- Size: 1,167,073 bytes
- SHA-256: `2cb86d7d8fc4e8e04f864e3183d72cad3ca2cf3b0bd10deaa42851b4d0b11bcb`

## Validation

- Final native/core, UI, input, developer, picker, gameplay, combat,
  presentation, parity, and fixture-drift suite: **50/50 passed**.
- Final UI session: **89 checks passed**.
- Extractor/resource tests: **228 passed, 2 expected skips**.
- Focused TypeScript combat/render tests: **88/88 passed**.
- Enemy identity/resource-pack regression: passed; deterministic Alpha pack
  rebuilt successfully.
- ESP-IDF v6.1 ESP32-S3 build and partition-size gate: passed.
- Launcher header, segments, checksum, appended SHA-256, and byte-identical copy:
  passed.
- Whitespace/error check: passed. A full extractor project typecheck still
  reports its three pre-existing strict-mode/JSON-import errors in untouched
  files; the extractor runtime suite and new packer tests are green.

## 16. Short physical retest checklist

1. Install the Alpha 1.3 Launcher app and copy the matching resource pack to the
   SD card. Confirm the title says Alpha 1.3 and gameplay has no black flash.
2. Tap Mic without Symbol in gameplay, a prompt, target mode, and each Developer
   depth. Confirm one-level Back behavior and no `$`/garbage input.
3. Hold Mic for about 1.1 seconds: verify `MOVE`, W/A/S/D, trackball coexistence,
   no release Cancel, a second hold disables it, and text/numeric entry receives
   ordinary letters only.
4. In Developer > Teleport, apply Britannia, a town/local map, and a dungeon
   floor. Verify the visible result, correct map/floor, continued movement, an
   invalid-coordinate error, and Mic backing out without applying.
5. Exercise long dialogue and rapid commands. Confirm larger readable text,
   word-aware wrapping, explicit line breaks, and no text-panel blank flash.
6. In combat, verify yellow player/red enemy active markers, turn changes, no
   dead marker, Attack and targeted Cast reticles, red invalid aim, name/HP text,
   Confirm, and Mic Cancel.
7. From a legal cannon/ship context, choose Fire, move its reticle, Confirm, and
   Cancel; confirm the original directional legality/result still applies.
8. Fight Giant Rats and several other species; verify sprite, attack/death text,
   and names agree. Observe NPC/enemy/combatant reference animation while Avatar
   and transport facing remain authoritative.
9. Watch the serial heartbeat through these scenarios for at least 4 KiB main
   stack margin and 32 KiB free internal RAM.

This artifact is fully host-validated but has not been flashed or physically
validated on a T-Deck Plus in this environment.
