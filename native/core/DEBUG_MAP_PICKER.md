# Native developer map picker

This is a semantic port of the existing OpenU5 developer tool, not a new game
travel system. It deliberately adds no T-Deck UI and changes no board, input, or
display code.

## Reference trace

The web feature is split across these existing sources:

- `game/src/debug/index.ts`: always creates `window.__u5debug`; F4, Backquote,
  and `?debug=1` open the browser drawer.
- `game/src/debug/registry.ts`: builds the Teleport section. It enumerates
  `world.smallMaps`, derives each location's floors from the same live map data,
  and enumerates `game.dungeons`.
- `game/src/debug/teleportPicker.ts`: browser-only modal, canvas maps, mouse/touch
  hit testing, zoom/pan, DOM selects, numeric inputs, localStorage “last
  destination,” keyboard suppression, tile labels, and passability warnings.
- `game/src/debug/teleportMap.ts`: the smaller browser canvas used by the debug
  drawer for large-map point-and-click teleports.
- `game/src/debug/debugApi.ts`: the actual debug mutation facade. The map picker
  calls `teleportOverworld`, `teleportSmallMap`, or `teleportDungeon` here.
- `game/src/core/world/map.ts`: authoritative large/small geometry, wrapping,
  floor lookup, and live map metadata.
- `game/src/core/dungeon/dungeon-cmds.ts`: real dungeon entry and the separate
  zero-RNG `setDungeonPos` harness seam.
- `game/src/core/game.ts`, `game/src/core/npc/manager.ts`: small-map entry,
  NPC reconstruction, door reset, and interior-object hydration.
- `extractor/src/data/locations.ts`: the authoritative 32 small-map IDs, source
  file categories, names, and signed floor lists used to build `smallmaps.json`.
- `game/assets/maps/dungeons.json`: the eight live dungeon definitions (33..40),
  names, eight floors, and cell data.
- `game/tests/debug-api.test.ts` and
  `game/tests/picker-gemmap-parity.test.ts`: mutation/RNG and picker-map parity.

`panel.ts`, the DOM field definitions in `registry.ts`, nearly all of
`teleportPicker.ts`, and all of `teleportMap.ts` are browser presentation. They
are intentionally not ported. The native API also does not port canvas drawing,
pointer/touch behavior, localStorage, CSS, DOM keyboard capture, or the gem-view
display-to-floor coordinate transform. A future native menu should select the
actual 8x8 dungeon floor coordinate directly.

## Ported semantics and safety

`include/openu5/debug_map_picker.h` exposes allocation-free enumeration, floor
enumeration, validation, and apply operations. Metadata comes from the same
resources already attached to `CommandContext`: `WorldData::small_maps`,
`DungeonContext::data`, and the normal banner/name callback. There is no second
destination table and no fake game state.

With the full standard resources attached, enumeration yields 42 destinations:
Britannia, Underworld, 32 small-map locations, and eight dungeons. A partial
host/device resource set exposes only the destinations it can actually load,
matching the web picker's live-resource behavior.

The translated operations are:

- Britannia/Underworld: assign the real `GameState::position`, wrap signed x/y
  to 256x256, do not consume RNG, and emit `MapChanged` (the native equivalent
  of the injected web `notify()`).
- Small map: assign the exact cell or standard `(15,30)` entry, then run
  `ResetDoors`, `EnterNpcs`, and `HydrateInterior` in the same order as
  `DebugApi`. It intentionally does not run normal-entry Shadowlord/urban
  effects, so the operation remains zero-RNG.
- Dungeon: when switching dungeons, execute the normal native dungeon-entry
  path (map copy, cleared-room application, wanderer respawn, overworld-enemy
  clear, shared RNG). Then assign the requested floor/x/y. Moving within the
  same dungeon assigns only floor/x/y, preserves facing, and consumes no RNG.
- Coordinates: large-map values wrap, matching `teleportOverworld`; small-map
  coordinates must be 0..31 and dungeon coordinates 0..7, extracting the bounds
  checks previously enforced by the web picker before its facade call.
- Passability: wall/impassable cells report `passable=false` but are still legal,
  exactly as the picker warning says.

Persistent party, inventory, quest, clock, transport, and save state are not
reset. The reference debug API also does not close dialogue/shop prompts, end
combat, change transport, or clear other sessions. Native large/small teleports
therefore leave those owners alone. A cross-dungeon switch uses the real core
entry API and is rejected atomically while combat owns the context; same-dungeon
coordinate relocation remains available. Selecting a large/small destination
while a dungeon session remains active changes the saved surface position but
does not implicitly destroy that session, matching the current web behavior.

## Build control and size

The developer implementation is omitted from normal builds by default:

```sh
cmake -S native/core -B native/core/build-debug -DOPENU5_ENABLE_DEVELOPER_TOOLS=ON
cmake --build native/core/build-debug
```

`OPENU5_ENABLE_DEVELOPER_TOOLS=OFF` is the default. Host tests compile the one
developer source privately so the behavior stays covered without adding it to
`openu5_core`. ESP-IDF includes the source only when the option is explicitly
set before `sources.cmake` is loaded. When disabled there is no debug object to
link; when enabled in the static host library, ordinary section/archive linker
stripping also removes it from a final image unless a frontend references the
API.

No native menu invokes this API yet. A future T-Deck menu should use three
levels—destination, floor, coordinate—with trackball focus movement, Enter to
open/apply, Back to return/cancel, optional keyboard filtering by the borrowed
name, and optional touch as a second input path. It should show the passability
warning but keep the apply action enabled.

For the validated ESP32-S3 build, the developer translation unit contains 3,114
bytes of allocatable code/read-only data and no data or BSS. With no frontend
reference, linker garbage collection removes all of it: enabled and disabled
builds both report a 342,168-byte image payload (342,288-byte padded `.bin`) and
identical section totals. Once a menu calls the API, the linker will retain the
referenced sections, up to that translation-unit total plus any newly retained
shared callees. The API adds no field to `GameState`, `CommandState`,
`DungeonState`, or another persistent owner, so persistent state-size impact is
zero bytes; request/result values are caller-owned temporaries.
