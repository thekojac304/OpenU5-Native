# OpenU5 T-Deck Alpha 1.2 correction pass

Alpha 1.2 is a device-input, combat-pacing, debug-frontend, map-picker, and
actor-presentation correction release. It does not add gameplay systems.

## Root causes and corrections

### Combat input freeze

The portable combat core deliberately rejects player commands while the
current combatant is an enemy. The browser frontend advances those turns with
its combat pacer, but the T-Deck runtime had no equivalent event pump and never
issued `CombatEnemyStep`. The first player action could therefore leave combat
parked forever on an enemy actor even though input, `UiAction`, `UiIntent`, and
the semantic command adapter were otherwise working.

The device runtime now runs the existing authoritative enemy-step command on a
400 ms presentation beat, handles charmed party actors through the same path,
queues up to eight inputs while the AI owns the turn, and returns control when
the next player actor becomes active. It does not auto-play player turns.
Movement, Attack and aim confirmation/cancellation, Cast and its party/map
targets, Pass, Get/Open/Klimb, inventory/equipment selection, escape, enemy
turns, and normal combat completion all remain semantic core commands. Fire is
still the existing contextual cannon command and is routed through its normal
direction picker where the world context permits it; no new combat weapon
system was invented.

Debug-level serial records now identify the current UI mode, incoming action,
emitted intent, command/result, active actor, AI/player ownership, and queued or
pending target state.

### Physical Cancel / Back key

The microphone key immediately left of Space is uniquely represented by raw
keyboard matrix **column 4, row 4** (zero based). LilyGO's character table
labels this position `$`. The input adapter matches the physical matrix
position before character/modifier processing and emits semantic
`UiActionKind::Cancel`; it does not inject ASCII Escape. This remains reliable
with Symbol or Shift active and is shared by menus, prompts, dialogue, shops,
combat, selection screens, target pickers, the developer UI, and cancellable
text/numeric entry.

### Developer UI and map picker

The platform-independent `UiDebugMenu` and debug APIs remain authoritative.
The T-Deck now presents a 320x240 hierarchy headed by `Developer`, a breadcrumb,
a highlighted row, scroll position, editable values, result/status text, and
the fixed footer hints `Trackball: Move`, `Enter: Select`, and `Mic: Back`.
Friendly device labels cover Teleport, Party, Stats, Inventory, Equipment,
Reagents, Quest, Time, Transport, NPC / Dungeon, and Presets.

The map picker was receiving Confirm and applying the backend mutation. Two
device presentation faults made that look inert: the frontend hid both the
chosen values and the returned status, and a teleport out of a dungeon left
the old `DungeonState::active` owner set. The renderer prioritizes an active
dungeon, so it kept composing the stale dungeon over the newly selected
Britannia/town map. Alpha 1.2 shows the exact result/error, deactivates the stale
dungeon owner for world/local destinations, rebinds dungeon/combat context,
refreshes terrain, resets the base UI mode, and forces a new snapshot.

### Actor animation

High-bank actor tiles were previously treated as a simple turn-number cycle,
while snapshot composition did not mark those actor cells animated. Device
idle redraws therefore never revisited them, and the selected frame did not
match the reference actor behavior. Alpha 1.2 ports the existing per-actor
presentation programs to a local, deterministic view clock (about 110 ms),
retains stable actor identity/seed in `PresentationSnapshot`, and marks NPC and
combat actor cells for partial animation updates. View PRNG is isolated from
gameplay RNG. The Avatar remains excluded from invented idle cycling; its
authoritative transport/facing tile now comes from `TurnState`.

## Performance and footprint

The Alpha 1.1 PSRAM tile cache, authoritative snapshot, visibility/fog,
live overlays, combat terrain, partial redraws, 40 MHz display path, and
resource caching remain in place.

- ESP-IDF image accounting: 654,844 bytes, up 13,228 bytes from Alpha 1.1.
- Packaged app binary: 654,960 bytes, up 13,232 bytes from 641,728 bytes.
- App partition: 1,048,576 bytes; 393,616 bytes (38%) remain.
- DIRAM: 90,602 / 341,760 bytes; 251,158 bytes remain.
- Static DIRAM delta: +1,168 bytes, primarily the 1,036-byte actor clock and
  device combat/UI state.
- Persistent PSRAM allocation: unchanged by this pass.
- Main task stack allocation: unchanged at 12,288 bytes. The render-local
  snapshot is 1,096 bytes, about 608 bytes larger than Alpha 1.1; the existing
  heartbeat high-water diagnostics remain the on-device guard and require at
  least 4 KiB free.

## Validation

- Native/core C++ regression: 27/27 passed, including UI/session, developer
  menu, map picker, gameplay integration, combat parity, presentation, and
  physical input adapter tests.
- Focused TypeScript combat/actor regressions: 70/70 passed.
- Extractor: 226 passed, 2 skipped.
- ESP-IDF v6.1 ESP32-S3 strict build: passed.
- Launcher validation: ESP32-S3 app header, segments, checksum, appended
  SHA-256, and byte-identical copy all passed.

The broad web-game suite was also sampled, but its unrelated Windows path,
missing private-fixture, and symlink-permission guards are not Alpha 1.2 release
gates; the correction-specific TypeScript tests above are green.

## Physical retest

1. Boot from Launcher and confirm the title reports Alpha 1.2 with no black
   full-screen flashes.
2. Press the mic key in a menu, dialogue/prompt, inventory/spell picker, target
   cursor, and Developer screen; each should back out one level.
3. Start combat. Move, Attack and confirm/cancel an aim, Pass, Cast through both
   target types, and use Get/Open/Klimb where applicable. Confirm enemy beats
   return control and combat can end by victory, flee, or defeat.
4. Use Fire from a legal cannon/ship context and confirm its existing direction
   and legality rules still apply.
5. In Developer > Teleport, test Britannia, a town/local map, and a dungeon
   floor. Confirm the visible `Result: Applied`, correct rendered map, and
   continued movement; verify Mic backs out without applying.
6. Watch representative NPCs and combatants while idle for reference cadence;
   confirm terrain animation and action latency remain as in Alpha 1.1.
7. Watch the serial heartbeat for at least 4 KiB stack margin and at least
   32 KiB internal RAM after the scenarios above.

## Packaged artifact

`native/targets/tdeck/build-alpha12-fw2/launcher/OpenU5-TDeck-Alpha1.2-Launcher.bin`

- Size: 654,960 bytes
- SHA-256: `0eb0b26b364a44315a767a85464ad0f6a556d0775202a76605552965d6a5cc06`

This artifact is host-built and awaits physical T-Deck Plus verification.
