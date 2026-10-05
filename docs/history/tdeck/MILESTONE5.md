# Milestone 5 — native movement and directional input

Final input policy: trackball is the sole directional movement source. Raw
keyboard input is retained for future Ultima commands and text, but no keyboard
event is normalized as movement.

Milestone 5 adds only immediate, one-tile, on-foot movement to the current packed
map. It keeps the 11x11 Avatar-centered renderer, validated v2 asset pack, SD
mount, shared SPI bus, and Launcher app-image format from Milestone 4.

## Input hardware and directional policy

The implementation follows LilyGO's official T-Deck sources:

- Keyboard: ESP32-C3 slave at I2C address `0x55`, SDA GPIO 18, SCL GPIO 8,
  interrupt GPIO 46. The host selects the official raw mode (`0x03`) and reads
  all five matrix-column bytes instead of the lossy one-character mailbox.
- Trackball: G01 GPIO 3 is up, G03 GPIO 15 is down, G04 GPIO 1 is left, and G02
  GPIO 2 is right. The native driver accepts only each falling edge, avoiding
  the two events that accepting both switch edges would create.

The trackball is the sole movement/navigation input. Symbol+number direction
chords were unreliable in the raw keyboard mode and have been removed instead
of being special-cased further. Symbol itself and all number/punctuation layer
codes remain normal raw keyboard data.

The raw matrix layer derives press and release edges, resolves printable codes,
and carries Symbol/Alt/Shift state with every event. Every successful snapshot
is authoritative. A failed read clears synchronization and all modifiers; the
next successful snapshot becomes a quiet baseline. This prevents a missed
release or keyboard-controller restart from leaving a modifier latched.

The previous controller kept one debounce record shared by keyboard and
trackball. That could suppress rapid repeated input for 120 ms but is not an
established explanation of the physical lockup. Keyboard matrix edges now
require no timing debounce, while the
unchanged 65 ms trackball debounce is isolated per trackball direction. No
trackball event resets keyboard or modifier state.

No keyboard characters are mapped to movement. Their press/release, case,
matrix position and Symbol/Alt/Shift information remain available for later
command and text consumers, but no command system is added here. Held-key repeat
remains absent.

Known raw-protocol limits are retained: the keyboard supplies a five-byte state
snapshot rather than an ordered event stream; simultaneous changes are emitted
in matrix scan order. There is no firmware key-repeat event, Unicode/layout
translation, or Alt character transformation. Symbol output is limited to the
LilyGO firmware's fixed table, and some matrix positions intentionally resolve
to code zero (modifier identity is still available by matrix coordinates and
`modifier_key`). After an I2C read failure, the next successful snapshot is a
quiet resynchronization baseline, so a key held across recovery must be released
and pressed again. Release events retain the character resolved on press while
their modifier flags describe the latest snapshot.

Primary references:

- <https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/Keyboard_T_Deck_Master/Keyboard_T_Deck_Master.ino>
- <https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/Keyboard_ESP32C3/Keyboard_ESP32C3.ino>
- <https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/UnitTest.ino>
- <https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h>

## Movement subset

`native_movement.cpp` mirrors only the geometry and final on-foot passability
used by `game/src/core/world/movement.ts` and `game/src/core/tiles.ts`:

- four cardinal directions, one tile per accepted input;
- the exact final `TILE_INFO.walkable` values for byte-sized map tile IDs;
- 256x256 wrapping when the packed Britannia surface is selected;
- strict boundaries on the current 32x32 Iolo's Hut map;
- no position change and no redraw for a blocked or out-of-bounds step.

The 32-byte native passability bitmap is mechanically checked against all 256
corresponding TypeScript `TILE_INFO` values by the host test. The movement test
also covers the real initial-map south floor (`0x44`), east table (`0x94`), a
local-map edge, and Britannia wrapping.

The v2 pack contains terrain, not the dynamic NPC/object actor table. Milestone
5 therefore preserves the Milestone 4 scene model and applies terrain
passability only; dynamic actor occupancy is intentionally deferred with NPC
turns/object hydration. The local-map edge is blocked rather than starting the
deferred leave-map prompt or world transition.

The full `Game.move()` epilogue is intentionally absent: no clock advance,
slow-terrain extra turns, NPC/world turns, encounters, transport, survival,
door transitions, or other command processing occurs.

## Runtime diagnostics

Normal serial output logs accepted direction actions, old/target coordinates,
target tile, passable/blocked decision, resulting coordinates, and successful
redraw duration. Detailed raw key matrix coordinates, press/release state,
modifiers, and trackball direction remain at debug log level so normal builds
are not flooded. The five-second heartbeat reports live coordinates and free
internal/PSRAM heap.

## Build and validation

From an activated ESP-IDF 6.1 shell:

```sh
cd native/targets/tdeck
idf.py -B build-m5 build
idf.py -B build-m5 size
python package_launcher.py --build-dir build-m5
```

Run the host contract tests with:

```sh
npm run test -w extractor -- native-input.test.ts native-movement.test.ts
```

Hardware input, debounce feel, movement, and redraw timing remain unverified
until the resulting Launcher image is tested on a physical T-Deck Plus.
