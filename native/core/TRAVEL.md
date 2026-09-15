# Native scheduled travel and map transitions

## Scope and source mapping

The TypeScript runtime is unchanged. All additions are in `native/core`.
The device continues to use its existing movement slice; this does not enable
new gameplay or change the physically unverified input implementation.

| Native API/module | Authoritative TypeScript |
| --- | --- |
| `npc_path.h`, `npc_path.cpp`: `npc_scan`, `npc_backtrace` | `npc/manager.ts`: `npcScan`, `npcBacktrace` |
| `tick_npcs`, raster, walkability, route consumption and cross-floor cases | `NpcManager.tick`, `tickInner`, `buildScanGrid`, `walkTileAt`, `walkableForScan`, `canMoveStep`, `followPathStep`, `recomputeOffFloor`, `changeFloorStep` |
| Existing schedule, idle AI and occupancy reused | `npcCheckSchedule`, `NpcManager.aiStep`, `wanderStep`, `isOccupied` |
| `pathfind.h`, `pathfind.cpp`: `find_path`, `step_direction` | `world/pathfind.ts`: `findPath`, `stepDirection` |
| `transitions.h`, `transitions.cpp`: `floor_exists` | `Game.floorExists`, `NpcManager.floorExists` |
| `location_at` | `world/movement.ts`: `locationAt` (first match, 1-based ID) |
| `local_boundary` | `world/movement.ts`: `resolveStep.exitedMap`; `Game.move` town-exit-prompt branch |
| `apply_stair_step`, `klimb_ladder` | `Game.applyStairStep`, `Game.klimbLadder` |
| `load_small_map`, `exit_to_overworld`, `confirm_town_exit` | `Game.loadSmallMap`, `exitToOverworld`, `confirmTownExit` |
| `moonstone_teleport` | `world/moongates.ts`: `moonstoneDestination`; `Game.moonstoneTeleport` |
| Loader physical-presence latch | `world/blackthorn.ts`: `computeShadowlordHere`, `shadowlordPresentIndex` |
| `scheduled_npc_hook` | `Game` NPC-turn integration seam; existing native `town_turn` callback after housekeeping, gated by `town_npc_tail_runs` |

Paths above are relative to `game/src/core`. `tick_idle_npcs` retains its old
idle-only preflight contract for existing callers/tests, then delegates supported
visible-floor ticks to `tick_npcs`. There is one scheduled route implementation.
New callers should use `tick_npcs` with a reusable raster, or bind
`scheduled_npc_hook` into `TownTurnContext.after_housekeeping`. The binding reports
an explicit `ActorError`; a caller must check it. It resolves the current floor
at invocation time, including immediately after a player transition. Its optional
map provider supplies effective door overrides; without it the base map is used.

## Preserved mechanics

Scheduled NPCs do **not** use A*. They use the manager's circular 32-entry BFS
queue, rotating from the incoming direction, marking cells even when a full
queue discards their enqueue. The 33rd raster row is blocked, while x=32 aliases
the following row as in TS. Destination and start marks overwrite occupancy in
that order. Starting at the destination therefore removes the destination mark.
The route contains at most 16 run-length pairs; truncation discards the start
end, rather than yielding a conventional shortened route.

The port preserves slot order, one shared planning budget per tick, planning
without taking a step, stale pairs, blocked-step wandering, three-block retry
threshold, the rescan coin and 200–204 cooldown. State 3 without a route aborts
the remaining NPC pass. Off-floor recomputation uses the visible floor; ladder
arrival can teleport to the scheduled destination without an occupancy check.
Missing destination floors close the schedule latch without relocation, as TS
does. Regular doors 184/186 become floor 68 only for scheduled walking; wandering
uses the supplied effective tile unchanged. Chairs, target posts, and stair
groups retain their distinct walkability tests.

General A* separately preserves north/south/east/west neighbor ordering, first
minimum-f tie selection, stable removal, immutable parent nodes, stale open
entries, transport passability, occupancy callback timing, toroidal distance,
and expansion counting (including the goal pop). It consumes no RNG.

Local entry sets `(15,30,0)` even if blocked or occupied. Stairs use NESW
orientation, preserve x/y, and only change floor if that floor exists. A ladder
refreshes hour tiles, runs the context turn, then emits its message and
`map-changed`. Stairs emit their message before the subsequent movement turn.
Local boundary attempts emit a prompt without relocation or RNG. Accepted exits
use the location table and always return to Britannia/floor 0, matching the
current TS limitation. A rejected exit runs a town turn. Small-map -1 basement
and large-map 255 Underworld remain distinct.

Moonstone teleport tests `location == 255`, **not** the `buried` flag. Large-map
floor is 255 only for stone z=255, otherwise 0. Small-map teleport executes the
loader, then restores stone x/y (floor stays 0), and emits the additional
`map-changed` event. This preserves the TS loader's intermediate entry position
and duplicate map-change event.

## Reload and event ownership

`TransitionServices` is a synchronous service boundary for existing systems,
similar to native turn hooks. Position, floor, drunk counter, loader flags, and
physical-presence latch mutate in core. Message and map/prompt events stream to
the consumer without a bounded event queue that could silently drop events.
The message pointer is borrowed for the duration of the callback.

Loader services run in this order: fresh NPC entry, clear outdoor enemies,
reset doors, clear volatile terrain, hydrate interior objects, refresh hour
tiles, urban effects, map-changed. Exit services discard interior objects,
reset doors, and clear terrain before relocation. NPC entry services should
call the already translated `enter_npc_map` with the destination's schedule
records and dead-slot mask. A destination reload replaces the active native
population; ordinary floor changes retain it.

Services are explicit dependency seams, **not implementations of excluded
systems**. Owners of doors, world-object storage, urban effects and the context
turn must bind those operations for a complete Game integration. Null service
pointers mean the corresponding subsystem is absent in a primitive-only caller.
The tests instrument these seams and compare invocation order and live position
against the actual Game methods. They do not claim parity for the internals of
those excluded services. A renderer should consume events, not implement reload
mutations. The banner parameter is the caller's already localized banner;
the loader preserves the ID 14–18/40 suppression rules.

## Memory and limits

| Storage | Bytes / bound | Ownership |
| --- | ---: | --- |
| NPC raster | 1,056 | Caller-owned, reused across scans, including two-scan floor routes |
| NPC scan queue | 64 | Local stack arrays |
| NPC ordering indices | 256 host / 128 ESP | Tick stack; insertion sort, no allocator |
| NPC route | 32 per actor | Already in `NpcActor`; no size increase |
| A* immutable nodes | 160,010 (16,001 × 10) | Caller-owned `PathScratch` |
| A* open indices | 32,002 | Same scratch |
| A* best-g scores | 131,072 | Same scratch |
| **A* scratch total** | **323,084** | Static/arena allocation recommended; future PSRAM candidate |
| A* output | Up to 16,000 (4,000 × 4) | Caller-owned; can request a smaller buffer |
| `GameState` | 576, delta **0** | Unchanged |
| `NpcActor` / `NpcActors` | 66 / 2,120 host, delta **0** | Unchanged |
| `TravelState` | 3 | New optional caller-owned state; not allocated by device |

These are buffer sizes, not a measured worst-case call-stack or on-device timing
claim. Scheduled NPCs never allocate the A* scratch. Production code uses no heap
allocator. The compatibility idle wrapper uses a local raster; new bindings use
caller-owned reusable storage. No global mutable scratch is introduced.

The scanner retains the TS 32-entry queue limit and 32-byte route. Its raster is
bounded to 1,056 bytes; the guard row is the TS rule, not a new native restriction.
Public backtrace input must be a successful scanner's unmodified grid.

General A* accepts integer in-bounds coordinates (or wrapped signed-16-bit
coordinates), map dimensions through 256, and at most **4,000 expansions**, the
TS default. Explicit TS requests above 4,000 are outside this native bounded
domain and return `Capacity` before searching; they are never silently clamped.
At most four nodes are created per expansion, so 16,001 node slots suffice.
Insufficient output capacity also returns `Capacity` with required path length.
Malformed maps/route indices return explicit errors rather than TS exceptions or
undefined array access. Native actor capacity remains 32 and schedule slots 0–2.

## Validation, 2026-09-15

| Check | Result |
| --- | ---: |
| New travel cases | **19,832** |
| Scheduled ticks, including player floor changes followed by NPC turns | 15,360 |
| Scanner/backtrace (including explicit 16-pair truncation) | 122 |
| General A* | 768 |
| Step directions | 3,072 |
| Game transition cases (three repeated attempts each) | 240 |
| Boundary cases | 200 |
| Location-table cases | 70 |
| Prior foundation + turn cases | 22,877 |
| **Total generated parity cases** | **42,709** |
| Native host CTest | **8/8** |
| Live TS fixture drift checks | **3/3** |
| Fixture TypeScript typecheck | Passed |
| Relevant game tests | **230/230**, 14 files |
| Extractor input/movement/pack tests | **15/15**, 3 files |
| ESP-IDF build + size | Passed |
| Launcher packaging | Passed validation and byte-identical copy |

The old exhaustive 65,536-seed RNG digest is retained and passes. It is not
double-counted as 65,536 generated cases. Travel fixtures serialize complete
NPC records and routes after every tick, RNG state, player floor changes and
their event counts; occupancy is derived from those same party/NPC coordinates
and the existing independently tested occupancy implementation. Game transition
fixtures additionally compare messages, flags, positions and every reload hook
in order. Excluded service internals are instrumented, not approximated.

No behavioral parity mismatch was accepted or required a TypeScript change.
Initial fixture setup selected an empty floor; it was corrected to initialize
NPCs at hour 0 before advancing the schedule. ESP GCC caught misleading
indentation that host Clang accepted; formatting resolved it. ESP environment
activation required its installed Python path and disabled the absent installer
constraints-file lookup; installed dependency checks passed. No tools were
installed or hardware configuration changed.

Firmware measurements against the preserved pre-batch image:

| Metric | Before | After | Delta |
| --- | ---: | ---: | ---: |
| App / Launcher image | 340,544 | 340,544 | **0** |
| `idf.py size` total | 340,432 | 340,432 | **0** |
| Minimum aligned Launcher allocation | 393,216 | 393,216 | **0** |
| Core state | 576 | 576 | **0** |

New routines compile through `sources.cmake` but are unused and linker-stripped
in the existing device slice. Zero image delta does not predict the cost of
enabling the full world loop. Final image SHA-256:
`8e650f12216715b179a46820b6a2dac698c4883eb978dbdbfaabc82e691064a0`.
Target file SHA-256 comparison found **zero changes**. Hardware was not flashed.

Evidence: `build-zig/Testing/Temporary/LastTest.log`,
`build-travel/idf-build-size.log`, `build-travel/target-before.json`,
and `build-travel/baseline.bin`. The Launcher image is in
`../targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.

## Remaining work outside this batch

Full `Game.enter` command dispatch, shrine/quest ceremonies, dungeon-specific
loaders and portals, physical moongate presentation/activation, complete door
and world-object reload services, urban Shadowlord effects, enemy lifecycle and
combat/dialogue/shop/magic systems are not implemented by these primitives.
Save-window serialization of NPC routes remains separate from the live bounded
actor state. Full Game/UI/input orchestration and enabling the world loop on
hardware are also separate work. No combat, dialogue, magic, shops, quests,
rendering, audio or hardware behavior was started here.
