# OpenU5 T-Deck Alpha 1.4 HARDWARE-TRUTH diagnostic pass

> The physical trace requested by this document has now established Mic/0 as
> matrix `(0,6)`. Alpha 1.4.0 alpha2 applies that correction; see
> [`ALPHA14_ALPHA2.md`](ALPHA14_ALPHA2.md). The earlier `(4,4)` hypothesis below
> is retained only as historical context for the alpha1 trace build.

Alpha 1.4 is a diagnostic firmware. It is compiled, linked, packaged, and host-tested,
but it has **not** been flashed or physically validated on a T-Deck Plus in this
environment. The firmware deliberately distinguishes host evidence from device evidence.

## 1. Prior assumptions that were wrong

- Alpha 1.3 treated matrix `(4,4)` as a proven physical Mic identity. It was only a
  vendor-table assumption. Alpha 1.4 records every real matrix edge with the full current
  and previous five-byte snapshots, position, both layer codes, modifiers, UI mode, UI
  action, shortcut, and any gameplay command. The resulting device trace disproved
  `(4,4)` and established `(0,6)`.
- Developer-menu animation was not isolated from world presentation. A previously visible
  animated world cell woke the render path every 55 ms while the menu was open; the menu
  path then converted that animation update into a full-screen clear and redraw. This is
  the concrete idle-flash cause.
- Host resource tests did not prove that the rebuilt pack was on the SD card. Alpha 1.4
  displays and logs both pack identities and refuses to start gameplay on a mismatch.
- The native combat target cursor used board bounds only. The TypeScript reference bounds
  each weapon-aim cursor move by the active weapon's integer combat distance.
- The 5x7 font was enlarged only vertically. That made 5x14 glyphs with distorted aspect.

## 2. Firmware and resource identity

Boot displays and logs:

- firmware `1.4.0-alpha1-debug`
- Git commit embedded by CMake (current build: `4d50203e9c68`)
- ESP-IDF app build date and time
- Alpha resource pack version, file size, and payload CRC
- asset pack version, file size, and payload CRC

This firmware expects:

- `openu5-alpha1-resources.bin`: v1.1, 1,227,341 bytes, payload CRC `a75228d8`,
  SHA-256 `c1758446b4d446cd8aabc586b408b7d388501c1471278eb4bfa3bb0c6fc9b988`
- `openu5-assets.bin`: v2.0, 132,284 bytes, payload CRC `933c9b82`,
  SHA-256 `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188`

Size and CRC must both match. A mismatch shows `RESOURCE PACK MISMATCH`, logs the expected
and observed identities, and blocks runtime startup rather than silently using stale data.

## 3. Mic and Sym diagnostic path

Every physical keyboard edge now emits `INPUT_EDGE` with:

`raw`, `prev`, `matrix`, `physical`, `edge`, modifiers, base code, symbol code, UI mode,
emitted UI action, and shortcut.

The synchronous UI route then emits `INPUT_ROUTE` with the action and either a concrete
gameplay command number or `gameplay_command=none`. Long-hold classification emits
`INPUT_HOLD`. Matrix read loss emits `INPUT_RESYNC` and abandons the gesture.

Corrected alpha2 behavior for the physically proven Mic at `(0,6)`:

- press: no UI action and no gameplay command
- short release: one `cancel` and no gameplay-letter command
- long hold: one movement-mode toggle; release emits nothing
- Sym+Mic: the Mic position remains semantic and does not leak `$`, `0`, or `What?`

The captured serial trace establishes base `0x00` and Symbol `0x30` at the same
physical `(0,6)` position. Alpha2 binds that position before translation.

## 4. Developer menu and Back hierarchy

Animation ticks are now excluded before the Developer render gate is evaluated. A menu
opened over animated terrain therefore stays idle. Each real menu render logs
`DEBUG_RENDER count=... reason=... clear=... full_redraw=... reconstructed=...`.

Each menu action logs prior/new depth, category, index, and UI-mode transition. Host tests
cover:

`Developer -> Teleport -> editor -> Cancel -> Teleport -> Cancel -> Developer -> Cancel -> gameplay`

The physical Mic-to-Cancel edge is established by the device trace and covered by
the alpha2 input regression.

## 5. Teleport device-path trace

A Teleport confirmation logs the selected destination kind/ID, floor, coordinates,
standard-entry flag, returned status, GameState before/after, DungeonState before/after,
and active context after the device rebind. The next composed presentation logs
`TELEPORT_RENDERER` with map/floor/coordinates and renderer snapshot center.

The result remains visible on the Developer screen. Portable and host integration tests
pass; the physical result is not yet known.

## 6. Enemy identity

Runtime combat creation and death now log `ENEMY_ID` with actor ID, enemy definition,
sprite family, render tile, name index, resolved/displayed name, and the loaded Alpha-pack
version/CRC. The packer regression still verifies definition 20 as `Giant Rat` and 24 as
`Giant Spider` without name special-casing.

The device must show the matching pack identity before a Giant Rat result is accepted.

## 7. Melee targeting parity and correction

The reference weapon cursor uses `floor(sqrt(dx^2 + dy^2)) <= weapon range`, within the
11x11 grid. It starts on the actor's last still-live in-range target, otherwise on the
actor. Alpha 1.4 now uses those constraints for `CombatAttack` only; Fire and targeted
spells retain their separate behavior.

For melee range 1, a second move away from the actor is rejected, including after a first
cardinal/diagonal move. Confirm on the actor is ignored. The handheld acceptance rule is
also enforced: Cancel exits Aim without dispatching `CombatAttackCancel`, so it cannot
consume a turn. The authoritative combat core itself is unchanged.

Host parity checks cover range-1 rejection, range-3 Euclidean bounds, retained-target
start, self-confirm rejection, valid confirmation, and no-command Cancel.

## 8. Font correction

Transcript glyphs now use uniform 2x scaling:

- visible glyph: **10 x 14 pixels**
- character cell: **12 x 16 pixels**
- **11 characters per line**
- **12 visible transcript lines**

Word-aware wrapping and coherent row updates are retained. The font keeps the original
5:7 aspect instead of the Alpha 1.3 vertical stretch.

## 9. Build and validation

Launcher:

`native/targets/tdeck/build-alpha12-fw2/launcher/OpenU5-TDeck-Alpha1.4-Debug-Launcher.bin`

- size: 667,216 bytes
- SHA-256: `8f17ac016525c05387cf27c94d5b6f1debf62380eef7d3a068f4f4a3aeb70f40`
- app partition free: 381,360 bytes (36%)

Validation completed:

- ESP-IDF 6.1 ESP32-S3 compile, link, image generation, and partition-size gate
- Launcher image structure/checksum/appended-SHA validation
- full native/core, UI, input, debug, gameplay, parity, and fixture-drift suite: 51/51 passed
- UI session assertions: 94 passed
- Alpha enemy identity tests: 2/2 passed
- targeted whitespace/error check: passed

## 10. Physical test sequence

1. Copy both exact packs above to `/ultima5/` on the SD card, install the Alpha 1.4 Debug
   Launcher binary, boot, and photograph the identity screen.
2. Tap Mic alone in exploration; then press/release Sym; then press Sym+Mic; then hold Mic
   for at least 1.2 seconds.
3. Open Developer with Alt+D and leave it untouched for five seconds.
4. Enter Teleport, enter one value editor, then tap Mic three times to return through
   editor -> Teleport -> Developer -> gameplay.
5. Reopen Developer, teleport to one town, and record the on-screen result and map.
6. Enter combat with a Giant Rat if practical and capture its creation/death lines.
7. Start melee Attack, try to move the cursor more than one cell away, confirm a legal
   adjacent target, then start Attack again and cancel with Mic.
8. Save the complete serial output from boot through the final Cancel.

If any issue remains, capture the complete lines beginning with:

- `IDENTITY`
- `RESOURCE_MISMATCH`
- `SNAPSHOT`, `EDGE`, `INPUT_EDGE`, `INPUT_HOLD`, `INPUT_RESYNC`, `INPUT_ROUTE`
- `DEBUG_OPEN`, `DEBUG_ACTION`, `DEBUG_RENDER`, `UI_MODE`
- `TELEPORT`, `TELEPORT_RENDERER`, `debug teleport rebind`
- `ENEMY_ID`
- `combat command`
- `METRICS`

For Mic specifically, do not trim the preceding `SNAPSHOT` line or either press/release
`INPUT_EDGE` line; those are the evidence needed to correct the physical matrix mapping.
