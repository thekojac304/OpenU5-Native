# Native OpenU5 core: semantic commands and world simulation

## Current gameplay status

The platform-independent gameplay integration is complete for the major semantic
loop: **41,836 new sequences / 2,076,501 cumulative**. See [GAMEPLAY.md](GAMEPLAY.md)
for transports, bridge tolls, outdoor encounters, generic commands, dungeon
handoffs, buffer persistence, owner setup and alpha readiness. No device/UI/FATFS
work was started. The older sections below are historical batch records, not
current blockers; the quest continuation is complete as recorded in QUESTS.md.

Historical partial quest checkpoint: 4,249 new differential observations,
including two bounded nontermination observations, bring the cumulative count to
2,033,537. See [QUESTS.md](QUESTS.md) for exact coverage and unfinished quest
orchestration at that checkpoint. The completed continuation in QUESTS.md and
current gameplay status above supersede this historical exclusion.

Shop commerce now adds **787,118 snapshots**, bringing the cumulative total to
**2,029,288**. See [SHOPS.md](SHOPS.md) for APIs, all 46 real shops, validation,
and presentation/storage/quest boundaries. Generate its helper and real-flow
fixtures before building a fresh checkout.

Platform-independent persistence now adds **619 compatibility scenarios** for
**1,242,170 total**, plus 11 separate generation/resource/API contract scenarios.
See [PERSISTENCE.md](PERSISTENCE.md) for native GAM/OOL, JSON sidecars/envelopes,
semantic byte-buffer hooks, explicit session-state exclusions and the remaining
storage/application integration work. No SD writes or hardware bindings were added.

The generic dialogue batch brings the total to **1,241,551 checked parity
snapshots/sequences**. See [DIALOGUE.md](DIALOGUE.md) for the conversation API,
TypeScript source mapping, 135 real records, UTF-16 behavior and explicit
quest/shop boundaries. Dungeon/world behavior remains documented in
[DUNGEON_WORLD.md](DUNGEON_WORLD.md).
Before a fresh build, regenerate the gitignored asset transports and loot names:

```powershell
node --import tsx native/core/tools/generate-world-flow-fixtures.ts
node --import tsx native/core/tools/generate-dungeon-fixtures.ts
node --import tsx native/core/tools/generate-dungeon-flow-fixtures.ts
node --import tsx native/core/tools/generate-transport-flow-fixtures.ts
node --import tsx native/core/tools/generate-dialogue-fixtures.ts
```

The preceding combat magic and advanced generic combat batch reached **609,373 total parity
snapshots**, including all 128 real arenas in four directions. See
[MAGIC.md](MAGIC.md) for APIs, source mapping, memory ownership and remaining
boundaries, and [VALIDATION.md](VALIDATION.md) for current checks and sizes.

## Previous physical-combat batch

Physical combat foundation and encounter handoff are translated within
[COMBAT.md](COMBAT.md)'s explicit domain: **352,349 total parity snapshots**,
**16/16 host checks**, including real arenas. See [REAL_ARENAS.md](REAL_ARENAS.md)
for source extraction and the 15 formerly skipped suites, now passing.
The caller-owned combat state is 3,424 host / 3,244 ESP
bytes; `GameState` remains 1,720 bytes. Hardware targets remain unchanged.
See [VALIDATION.md](VALIDATION.md) for current results and deferred combat work.

The preceding inventory, equipment, supported item-use primitives, camp/bed/ship rest and
their command integration are translated. See [ITEMS_REST.md](ITEMS_REST.md):
**95,261 total parity cases**, with current validation in [VALIDATION.md](VALIDATION.md).
`GameState` is now 1,720 bytes; the historical 576-byte measurements below
predate inventory. Hardware target sources remain unchanged.

The semantic command layer is implemented for the translated Game projection.
See [COMMANDS.md](COMMANDS.md) for dispatch, events, supported commands,
extension boundaries, memory and **56,533 total parity cases**. The older
descriptions below of event scaffolding apply only to the preserved device slice.

Scheduled NPC routes, cross-floor travel, general A*, and generic map transition
primitives are translated. See [TRAVEL.md](TRAVEL.md) for their source mapping,
service boundaries, memory requirements, and preceding **42,709-case** validation.

This is a portable C++17 core translated from the existing TypeScript core.
It is **not a full native Game implementation**. The device still runs the limited
Milestone 5 movement slice, now against core-owned state. No hardware driver,
input behavior, turn loop, or presentation feature is added by this phase.
Milestone 5.1 input cleanup remains physically unverified.

## Layout and ownership

| Module | Responsibility |
| --- | --- |
| `include/openu5/types.h` | Byte map coordinates, signed deltas, contextual map IDs, directions, transport IDs, explicit error results |
| `state.h`, `src/state.cpp` | Bounded 16-record roster, six-member selection, foundation state and independent initialization copy |
| `rng.h`, `src/rng.cpp` | Explicit 16-bit original RNG stream and DOS time hash |
| `time.h`, `src/time.cpp` | Calendar arithmetic, day phase and four-entry schedule selection |
| `world.h`, `src/world.cpp` | Borrowed world/map views, coordinate access, basic tile properties and transport passability predicates |
| `movement.h`, `src/movement.cpp` | On-foot unoccupied step geometry, preserved device slice, semantic action/result and event scaffolding |
| `turn.h`, `src/turn.cpp` | Gameplay clock, survival, town/outdoor sequencing, cadence and encounter gates |
| `actors.h`, `src/actors.cpp` | Schedule latches, fresh entry, basic NPC/guard updates, occupancy and actor pool |
| `src/tile_flags.inc` | Generated basic property table from final TypeScript `TILE_INFO`, including overrides |
| `fixtures/`, `tools/`, `tests/` | Synthetic reference vectors, generator, compiled differential tests, and mixed-action replay |
| `sources.cmake` | One production source list shared by host and ESP-IDF builds |
| `commands.h`, `src/commands.cpp` | Semantic dispatch, supported Game actions, context turns, ordered events and explicit deferred-subsystem seams |

Dependency direction:

```text
T-Deck raw input -> existing InputController -> Direction / MoveAction
                                               |
asset pack adapter -> geometry + target tile -> native core -> CommandResult
                                               |
                                       GameState.position
                                               |
                                      native renderer snapshot
```

The core has no ESP-IDF, T-Deck, filesystem, renderer, wall-clock, or asset-pack
dependencies. `WorldData` borrows caller-owned row-major map buffers; its active
views must not outlive those buffers. The T-Deck currently reads destination
tiles from SD above the core, so it need not allocate a complete world map.
`ActiveMap` is the contiguous host/future-memory implementation of the map
interface. Streaming adapters can continue supplying the pure slice's tile
input; a richer tile-provider abstraction can be introduced when required by
the next translated subsystem.

`GameState` deliberately contains only an `InitialState` foundation projection,
transport, version and RNG. Do not use it to export/import saves. The device
initializes only position/map from its existing pack metadata; the remaining
scaffold is not presented as loaded INIT.GAM party data.

Turn fields and actors are caller-owned `TurnState`/`NpcActors` values. Their
APIs, exact TS mapping, fixture counts, callback ownership, supported numeric
domains and untranslated boundaries are documented in [WORLD_TURNS.md](WORLD_TURNS.md).
The compatibility idle-only entry point retains its `NeedsPathfinding` preflight.
`tick_npcs` implements scheduled walking; the device does not invoke this framework.

`resolve_unoccupied_foot_step` mirrors `resolveStep` for `transport=foot` and
`actorTile=0`. It returns the reference minutes, slow-terrain class, exit and
hazard flags without executing them. `apply_movement_slice` preserves the
existing device policy: update legal coordinates, block local boundaries, do
not run time/turns/transitions or emit Game events. These are separate named
contracts. The device is not silently upgraded to a partial full-turn loop.

`GameEvent` now carries the supported command engine's ordered semantic payloads.
The device slice retains a separate two-byte `MovementSliceEvent` scaffold and
zero `CommandResult.event_count`. See [COMMANDS.md](COMMANDS.md) for the streaming
event lifetime, result semantics and explicitly untranslated event families.

## Host build and test

The portable presentation/controller boundary is documented in
[UI_SESSION.md](UI_SESSION.md).  It is independent of the existing T-Deck input
and renderer slice.

With a host C++ compiler and CMake available, from the repository root:

```sh
cmake -S native/core -B native/core/build-host -DCMAKE_BUILD_TYPE=Release
cmake --build native/core/build-host --config Release
ctest --test-dir native/core/build-host -C Release --output-on-failure
# Alternatively, after configuration:
cmake --build native/core/build-host --config Release --target host-test
```

Host CTest runs compiled parity tests, the pre-existing movement and input
regression harnesses, and live TypeScript fixture drift detection when Node is
available. The input harness uses C++20; the core itself requires C++17.
Pure C++ tests use committed synthetic fixtures and need no Node, SD pack, or
original game files. Install repository npm dependencies to run the live TS
drift check. Missing dependencies cause a failure, not a silent regeneration.

```sh
node --import tsx native/core/tools/generate-fixtures.ts --check
node --import tsx native/core/tools/generate-turn-fixtures.ts --check
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
# Only after reviewing reference changes:
node --import tsx native/core/tools/generate-fixtures.ts
```

On this Windows machine, the installed ESP clang cannot target x86-64. A
portable Zig 0.13.0 compiler was downloaded to ignored `build-tools/` for real
Windows executable tests; no system compiler or PATH installation was made.
For a reproducible portable build, set `ZIG_EXE` to your local `zig.exe`, and
`ZIG_GLOBAL_CACHE_DIR` to a writable build cache, then configure CMake with:

```powershell
$env:ZIG_EXE = (Resolve-Path native/core/build-tools/zig-windows-x86_64-0.13.0/zig.exe).Path
$env:ZIG_GLOBAL_CACHE_DIR = "$PWD/native/core/build-tools/cache"
# Use CMake and Ninja on PATH, or their full installed paths.
cmake -S native/core -B native/core/build-zig -G Ninja `
  "-DCMAKE_CXX_COMPILER=$PWD/native/core/tools/zig-cxx.cmd" `
  "-DCMAKE_AR=$PWD/native/core/tools/zig-ar.cmd" `
  "-DCMAKE_RANLIB=$PWD/native/core/tools/zig-ranlib.cmd" `
  -DCMAKE_BUILD_TYPE=Release
cmake --build native/core/build-zig --target host-test
```

## ESP-IDF and Launcher

In the existing activated ESP-IDF v6.1 environment:

```sh
cd native/targets/tdeck
idf.py -B build-core build
idf.py -B build-core size
python package_launcher.py --build-dir build-core
```

The shared production source list is compiled by IDF; host tests and fixtures
are never linked into firmware. Unused translated routines are linker-stripped.
Launcher packaging and project version/naming are preserved. `build-m5` remains
the previous baseline; the new package is under `build-core/launcher/`.

## Memory and representation

Measured host and ESP32-S3 `GameState` size: 576 bytes, with 32-byte character
records. No core heap allocation is used. Character layout is **not** a binary
save codec: never serialize C++ object memory, padding, booleans or enums.
The device holds its state in static storage to avoid adding a roster to the
unverified app-task stack. The renderer still receives a coordinate snapshot.
The eight-byte increase supports TS's unsaturated turn counter across 32-bit
boundaries. New state is not allocated in the device loop. Host sizes are
72 bytes for `TurnState`, 66 per NPC, 2,120 per 32-NPC container, and 1,024 per
actor pool (owner indices follow host `size_t`). No production heap allocation
is used; full maps and many cached locations remain candidates for PSRAM.

Whole overworld plus underworld maps need 131,072 bytes; many town floors, future
actor/quest state and presentation buffers are likely PSRAM candidates. They
remain outside this small value state. A single town floor needs 1,024 bytes.
Host map fixture buffers are synthetic test-only data. No full JS object graph
was copied into firmware. See [PARITY.md](PARITY.md) for numeric and array domains,
exact source mapping and deliberate limits, and [VALIDATION.md](VALIDATION.md)
for results.
