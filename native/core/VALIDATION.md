# Native core validation — 2026-09-15

## Latest batch: physical combat foundation

Source mapping, interface contracts and explicit exclusions are in
[COMBAT.md](COMBAT.md). Changes are confined to `native/core`; TypeScript runtime
and all **33/33** snapshotted target source/configuration/packager files are
unchanged. T-Deck input still awaits physical verification. No flash or hardware
operation was performed.

| Check | Result |
| --- | --- |
| New combat parity | **97,344 snapshots passed**, 2,496 deterministic scenarios |
| Total generated parity | **192,605 passed** (95,261 previous + 97,344) |
| New native combat adapter checks | **7 passed** |
| Native host CTest | **14/14 passed**, including all preceding suites |
| TypeScript fixture drift | **6/6 passed**, included in CTest |
| Fixture TypeScript typecheck | Passed |
| Available relevant TypeScript game tests | **75/75 passed**, 7 files |
| Available extractor/native regressions | **15/15 passed**, 3 files |
| Real-arena game suites | **14 suites explicitly skipped** by `tools/test-combat-reference.ps1` |
| Combat-map extractor suite | **1 suite explicitly skipped**; initial direct run confirmed missing `BRIT.CBT` |
| ESP-IDF build / `idf.py size` | Passed; new combat source compiled |
| Launcher packaging | Passed image/header/chip/checksum/SHA-256 validation |
| Physical input verification | Outstanding, unchanged |

Missing assets: `game/assets/maps/combatmaps.json`,
`original/u5/ultima5/BRIT.CBT`, `original/u5/ultima5/DUNGEON.CBT`.
The skipped list includes both previously blocked camp suites. Skipped suites
are not counted as passes. Synthetic fixtures require none of these assets.

One behavioral mismatch was found and resolved: the initial native regeneration
pass checked zero, while the TS reference heals on `rand(0,7) === 7`. No remaining
observed mismatch is accepted. Host compilation also caught the new command enum
switch coverage; ESP compilation caught `int32_t` being `long` rather than `int`
in `std::max` deduction and a callback return type. Both portability issues were
fixed in native code. The TS implementation and its formulas were not changed.

### Combat size and memory

| Measurement | Before | After | Delta |
| --- | ---: | ---: | ---: |
| App / Launcher binary | 341,712 | 341,712 | **0 bytes** |
| Linked image (`idf.py size`) | 341,588 | 341,588 | **0 bytes** |
| Minimum aligned Launcher allocation | 393,216 | 393,216 | **0 bytes** |
| `GameState`, host / ESP | 1,720 / 1,720 | 1,720 / 1,720 | **0 / 0** |
| `PartyState`, host / ESP | 524 / 524 | 524 / 524 | **0 / 0** |
| `CharacterState`, host / ESP | 32 / 32 | 32 / 32 | **0 / 0** |
| `GameEvent`, host / ESP | 24 / 16 | 32 / 20 | **+8 / +4** |
| `Command`, host / ESP | 12 / 12 | 16 / 16 | **+4 / +4** |
| `CommandContext`, host / ESP | 192 / 96 | 200 / 100 | **+8 / +4** |
| New caller-owned `CombatState`, host / ESP | — | **3,424 / 3,244** | New |
| `CombatActor`, host / ESP | — | 104 / 96 | New |
| `CombatMap`, host / ESP | — | 524 / 524 | New |
| `CombatContext`, host / ESP | — | 96 / 48 | New |
| `CombatEvent`, host / ESP | — | 40 / 32 | New |

Unused combat routines are linker-stripped from the unchanged device slice.
The zero firmware delta is **not** the flash cost of enabling combat on device.
No `CombatState` is added to `app_main` or the existing static `GameState`.

There is no heap allocation, search scratch or persistent event buffer. Arena
state should be static or caller-owned when integrated. Explicit temporary
buffers are a 192-byte handoff message, 160-byte attack messages, an 80-byte
placement array/used bitmap and up to 26 borrowed enemy pointers (104 ESP bytes).
ESP32-S3 GCC `-Os -fstack-usage` reports a largest individual frame of **432 bytes**
for `start_encounter_combat`, and **192 bytes** for initialization. These are
individual compiler frames, not a total stack/high-water measurement including
callees or callbacks. Placement construction avoids a full-state stack temporary.
No new PSRAM requirement; a future catalogue of all 128 original maps and expanded
AI/search storage may belong in PSRAM. Only one active map lives in this state.

Baseline firmware, build/size/package logs, size-symbol reports, stack-usage
report and target hashes are under ignored `native/core/build-combat/`.
Launcher output remains
`native/targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.
Final application and Launcher SHA-256:
`5872cbdf9779fa36e2657147c60910a00e9e0cc4685bc6b15ff3cdec03ff5cf1`.

### Combat remaining work

Deferred: combat spells, dynamic summons/splitting, special magic enemy AI,
fixed-from-map dungeon enemy/object/field seeding, arena object interactions,
quest/boss scripts, full world aftermath (including pirate prize ships), UI and
hardware integration. Unsupported enemy abilities reject explicitly. Generic
physical enemy turns are translated; full AI is not claimed. Restore real map
assets before validating original arenas, and define capacity/error handling for
TS actor-vector growth before magic/advanced AI. See [COMBAT.md](COMBAT.md) for the
complete admitted domain and exact exclusions.

## Previous batch: inventory, equipment, item use and rest

Source mapping, numeric domains, command semantics, callback contracts and
explicitly deferred behavior are in [ITEMS_REST.md](ITEMS_REST.md). Production
changes are confined to `native/core`. TypeScript runtime and target code are
unchanged. No hardware operation was performed.

| Check | Final result |
| --- | --- |
| New inventory/equipment/item/rest parity | **38,728 passed** |
| Total generated parity | **95,261 passed** (56,533 preceding + 38,728) |
| New native adapter checks | **11 passed**, including missing-service rejection, byte-coordinate boundary, invalid input, initialization copy, and pending-combat lock |
| Existing command adapter checks | **13 passed** |
| Native host CTest | **12/12 passed** |
| Live TypeScript fixture drift | **5/5 passed**, included in CTest |
| Fixture TypeScript typecheck | Passed |
| Relevant available game tests | **302/302 passed**, 18 files |
| Extractor input/movement/pack tests | **15/15 passed**, 3 files |
| Additional real-arena camp suites | **Blocked at collection**: `camp-ambush.test.ts`, `camp-guard-walk.test.ts` require missing `game/assets/maps/combatmaps.json` |
| ESP-IDF build and `idf.py size` | Passed; new production modules compiled |
| Launcher packaging | Passed image/header/chip/checksum/SHA-256 checks and byte-identical copy |
| Target source/configuration/packager isolation | **31/31 SHA-256 hashes unchanged** |
| Physical input verification | Still outstanding; not attempted |

The exhaustive 65,536-seed RNG digest remains passing and is not included in
the generated-case count. Cases compare pack and equipment mutations, member
HP/MP/status/stats, clock/resources/turns, complete RNG draw sequences, ordered
messages/SFX, bed scheduling/occupancy order, and guard-cell queries. Rare branch
coverage is asserted and recorded in `fixtures/items-coverage.json`.

No behavioral parity mismatch was accepted or remains. During harness setup,
an incorrect import, incomplete synthetic message registry, numeric encoding of
the `campHoleUp` result object, and a missing mock-world property were corrected.
The fixture writer now rejects all non-safe-integer numeric fields. Neither
TypeScript behavior nor expected outcomes were edited to accommodate C++.

The first broad game-test run passed 302 tests but failed collection for two
additional suites. The local original directory lacks both BRIT.CBT and
DUNGEON.CBT, so the unchanged extractor cannot produce the required real arena
asset. Those suites are not reported as passing or skipped. The final available
suite run passed all 302 tests; synthetic differential tests independently
exercise guard movement, ambush selection and early return. The test-name filter
also selected the existing six-test `combat-use-potion` suite; running it did not
add combat code to the native scope.

### Size and memory deltas

| Measurement | Before | After | Delta |
| --- | ---: | ---: | ---: |
| App / Launcher image | 340,544 | 341,712 | **+1,168 bytes** |
| Linked image (`idf.py size`) | 340,432 | 341,588 | **+1,156 bytes** |
| Minimum aligned Launcher allocation | 393,216 | 393,216 | **0 bytes** |
| `GameState`, host / ESP | 576 / 576 | 1,720 / 1,720 | **+1,144 / +1,144** |
| `PartyState`, host / ESP | 524 / 524 | 524 / 524 | **0 / 0** |
| `CharacterState`, host / ESP | 32 / 32 | 32 / 32 | **0 / 0** |
| Equipment per member | 6 | 6 | **0 bytes** |
| `TurnState`, host / ESP | 72 / 72 | 72 / 72 | **0 / 0** |
| `GameEvent`, host / ESP | 24 / 16 | 24 / 16 | **0 / 0** |
| `Command`, host / ESP | 3 / 3 | 12 / 12 | **+9 / +9** |
| `CommandState`, host / ESP | 20 / 20 | 24 / 24 | **+4 / +4** |
| `ActionResult`, host / ESP | 24 / 24 | 48 / 40 | **+24 / +16** |
| `CommandContext`, host / ESP | 184 / 92 | 192 / 96 | **+8 / +4** |

The legacy movement `CommandResult` remains 18 bytes. New caller-owned types:
`RestContext` 104 host / 52 ESP, `RestServices` 48 / 24, `RestResult` 20 / 20,
`ItemResult` 16 / 8, `InventoryEntry` 8 / 8, `ReadyItems` 49 / 49 and
`RingExpiries` 33 / 33. ESP sizes were measured with the installed S3 compiler,
not inferred from host pointer sizes.

No heap allocation or persistent event queue was added. Maximum explicit new
message scratch is 192 bytes plus a 16-byte apparition roll array; tools use
96-byte messages and repair uses 48. These are local-buffer requirements, not
a measured total stack bound including callees and supplied callbacks.
The unchanged target holds GameState statically, so the inventory increase is
not an app-task stack allocation. New routines are linked out of the limited
device slice; this firmware delta is not the cost of enabling them on hardware.
No new PSRAM requirement. Existing maps/arena data and 323,084-byte A* storage
remain candidates for future PSRAM ownership.

Final app and Launcher SHA-256:
`c7aaf2cb9b58c89f8587f9f755f5c02c6bead3166265b70371f2f3d16804a2ac`.

Package: `native/targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.
Baseline, build/packaging logs, ESP size object/report, target snapshot and final
isolation report are under ignored `native/core/build-items/`. Host details are
also in `build-zig/Testing/Temporary/LastTest.log`.

### Reproduction and remaining boundary

```powershell
# Zig environment setup remains documented in README.md.
C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build native/core/build-zig --target host-test
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
node --import tsx native/core/tools/generate-item-fixtures.ts --check
npm run test -w extractor -- native-input.test.ts native-movement.test.ts native-pack.test.ts

# Exact available game-test selection is retained in build-items/game-tests.log.
# Activated installed ESP-IDF 6.1 environment:
idf.py -C native/targets/tdeck -B C:/Dev/OpenU5-TDeck/native/targets/tdeck/build-core build size
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core
```

Before full combat integration, provide the combat-map assets, bind arena and
object/actor rest services, and implement acceptance of the camp ambush handoff.
Full potion UI/ceremony/reveal, combat equipment aggregation/synchronization,
magic, quest-specific items, shops, persistence, dungeon orchestration and UI
remain explicitly deferred in ITEMS_REST.md. There is no remaining mismatch
blocking further platform-independent combat translation. Device gameplay
integration still awaits physical verification of the existing input cleanup.

## Previous batch: semantic command orchestration

Implemented scope, source mapping, event contracts, memory and remaining Game
methods are in [COMMANDS.md](COMMANDS.md). All production changes in this batch
are confined to `native/core`; TypeScript runtime and hardware are unchanged.

| Check | Final result |
| --- | --- |
| New command parity | **13,824 passed** |
| Total generated parity | **56,533 passed** (42,709 preceding + 13,824) |
| Native adapter invalid/unsupported-context checks | **13 passed**, atomic state/event/RNG checks |
| Native host CTest | **10/10 passed** |
| Live TS fixture drift checks | **4/4 passed**, included in CTest |
| Fixture TypeScript typecheck | Passed |
| Relevant asset-independent game tests | **246/246 passed**, 17 files |
| Extractor input/movement/pack tests | **15/15 passed**, 3 files |
| Additional real-map NPC census suite | **Unavailable**: `npc-escaleras-c4.test.ts` fails collection because `game/assets/maps/smallmaps.json` is missing; it also requires `npcs.json` |
| ESP-IDF build and `idf.py size` | Passed; final `commands.cpp` compiled |
| Launcher packaging | Passed header/chip/checksum/SHA-256 validation and byte-identical copy |
| Target file isolation | **33/33 SHA-256 hashes unchanged** |
| Physical hardware verification | Not performed; input cleanup remains unverified |

The original exhaustive 65,536-seed RNG digest also passes; it is not added to
the generated-case count. Command coverage asserts both exit responses and rare
branches. See `fixtures/commands-coverage.json` for all measured witnesses.
No behavioral parity mismatches were accepted. All supported command traces
match; no TypeScript runtime or expectation was changed to accommodate C++.

The extra NPC census failure is a missing-fixture limitation, not a passing or
skipped test. The local originals contain DWELLING.DAT but lack the other
CASTLE/TOWNE/KEEP and NPC sources needed to generate that complete asset set.
No substitute game data was fabricated. Existing travel parity and new command
replays independently exercise synthetic scheduled cross-floor NPC behavior.

### Size deltas

| Measurement | Before | After | Delta |
| --- | ---: | ---: | ---: |
| App / Launcher image | 340,544 | 340,544 | **0 bytes** |
| Linked image (`idf.py size`) | 340,432 | 340,432 | **0 bytes** |
| Minimum aligned Launcher allocation | 393,216 | 393,216 | **0 bytes** |
| `GameState` host / ESP | 576 / 576 | 576 / 576 | **0 / 0** |
| `TurnState` host / ESP | 72 / 72 | 72 / 72 | **0 / 0** |
| `GameEvent` host / ESP | 2 / 2 | 24 / 16 | **+22 / +14** |
| Existing slice `CommandResult` host / ESP | 18 / 18 | 18 / 18 | **0 / 0** |

New caller-owned types: `Command` 3 bytes, `CommandState` 20, `ActionResult` 24
on both architectures; `CommandContext` 184 host / 92 ESP. MoveAction remains
one byte. The legacy movement slice keeps its two-byte event scaffold, avoiding
a result-layout change in the unchanged device loop. Full event payloads belong
to the new command engine. Existing NPC/TravelState layouts are unchanged.

ESP sizes were measured with an unlinked object compiled by the installed
ESP32-S3 compiler, not inferred from host alignment. New routines compile but
remain linker-stripped in the unchanged device slice. Zero firmware delta is
not an estimate of the cost of enabling full commands on-device. No new command
state, NPC raster or A* scratch is allocated by `app_main`. General A* remains
323,084 bytes of caller-owned storage, a future PSRAM allocation.

Final app and Launcher SHA-256:
`ab6fb1dc6b39128c595d959dd8d224f184db0c9097d38b1b8d4b4105756cdb12`.

Output: `native/targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.
No flash, reset or physical operation was performed.

### Commands, evidence and resolved setup issues

```powershell
# Existing Zig compiler/cache environment from README.md:
C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build native/core/build-zig --target host-test
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
node --import tsx native/core/tools/generate-command-fixtures.ts --check

npm run test -w game -- rng-original.test.ts time.test.ts survival.test.ts loops.test.ts loops-integration.test.ts town-npc-cadence.test.ts town-npc-bloqueo-mudo.test.ts town-stairs.test.ts town-sleep-turn.test.ts turn-counter.test.ts cactus-ouch.test.ts npc-caminata-108.test.ts npc-live-stream.test.ts npc-pestillo-horario-84.test.ts commands.test.ts prompts-town-exit.test.ts drunk-hic.test.ts
npm run test -w extractor -- native-input.test.ts native-movement.test.ts native-pack.test.ts

# Installed ESP-IDF v6.1 environment:
idf.py -C native/targets/tdeck -B C:/Dev/OpenU5-TDeck/native/targets/tdeck/build-core build size
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core
```

Evidence in ignored `native/core/build-command/`: `baseline.bin`,
`target-before.json`, `host-tests.log`, `game-tests.log`, `idf-build-size.log`,
`launcher.log`, `esp-sizes.txt`. Full CTest outputs are in
`build-zig/Testing/Temporary/LastTest.log`.

An initial synthetic midnight fixture with live Shadowlords encountered an
unbounded candidate reroll in the authoritative TS RNG/state combination.
This command corpus uses inactive Shadowlords; midnight calendar behavior is
still exercised, and the earlier survival corpus retains its live-relocation
fixtures. No native-specific escape or altered RNG behavior was introduced.
Source inspection corrected the drunken NS/E/W table and bridge tile IDs before
the compiled comparison. ESP-IDF initially lacked `ESP_IDF_VERSION` in the shell;
setting the installed version and process-local UTF-8 option resolved startup.
The final build and all supported parity cases pass.

Remaining translations: full transport/naval commands, bridge encounters,
trapdoor/quest effects, inventory/object/door actions, camp/rest workflows,
combat, dialogue, shops, magic, quests, dungeons, persistence, rendering and UI.
Their state/event/continuation owners and effective-map composition remain the
architectural integration work; the exact method list is in COMMANDS.md.

## Previous batch: scheduled travel and generic transitions

See [TRAVEL.md](TRAVEL.md): **19,832 new / 42,709 total parity cases**, **8/8 host
checks**, **230 game tests**, **15 extractor tests**, typecheck, ESP-IDF build/size,
and Launcher packaging passed. Firmware and existing state-size deltas are zero.
All target source/configuration/packaging hashes are unchanged. The sections below
record earlier batches; their unsupported-pathfinding boundary is historical.

## Current batch: world-turn and scheduling foundation

| Check | Result |
| --- | --- |
| Release native host CTest | **6/6 passed** |
| New compiled world-turn parity | **15,608 cases passed**, plus unsupported-walk atomicity assertion |
| Existing compiled foundation parity | **7,269 cases passed**, exhaustive 65,536-seed digest retained |
| Total generated parity cases | **22,877** |
| Both live TS fixture drift checks | Passed |
| Fixture-generator TypeScript typecheck | Passed |
| Relevant game TypeScript tests | **192 passed, 8 skipped**, 16 files passed |
| Extractor native input/movement tests | **11/11 passed**, 2 files |
| ESP-IDF v6.1 build and `idf.py size` | Passed; both new production modules compiled |
| Launcher packaging | Passed image header/chip/checksum/SHA-256 and byte-identical copy validation |
| Hardware verification | Not performed; M5.1 input cleanup remains physically unverified |

The eight skipped tests are existing asset-dependent cases in
`game/tests/trapdoor-fall.test.ts`; `ds-strings.json` and its source message
assets are unavailable locally. Synthetic native fixtures separately exercise
trapdoor re-reading, carpet bypass, `none`/`tpk` callbacks and the nine-iteration
guard. Full Stonegate/quest/presentation behavior is outside this native batch.
The initial moon-phase suite failed because `game/assets/data.json` was absent.
It was generated from local `original/u5/ultima5/DATA.OVL` with the unchanged
`extractor/src/parsers/dataovl.ts:parseDataOvl`; all 14 moon-phase tests then passed.
This ignored asset is not included in native parity fixtures.

### Exact firmware delta from the preceding foundation build

| Measurement | Before this batch | After this batch | Delta |
| --- | ---: | ---: | ---: |
| App/Launcher `.bin` | 340,544 bytes | 340,544 bytes | **0 bytes** |
| Linked image (`idf.py size`) | 340,432 bytes | 340,432 bytes | **0 bytes** |
| Minimum aligned Launcher allocation | 393,216 bytes | 393,216 bytes | **0 bytes** |
| Static `core_state` symbol | 568 bytes | 576 bytes | **+8 bytes** |

The new turn/actor routines are compiled through the shared `sources.cmake` but
remain unused and linker-stripped in the unchanged M5 device loop. The 64-bit
turn counter increases the static core state by eight bytes (ELF symbol size
`0x240`); aggregate image size remains unchanged. This is **not** an estimate of
firmware cost once the full turn framework is enabled on-device.

The preserved input image is `native/core/build-turn/baseline.bin`, SHA-256:

```text
9d169fb7217f577664f52a94fd7998144189ef13f57b2b333621e788053d61aa
```

Final app and packaged Launcher SHA-256:

```text
b057c6cfb324423cf2f89ab18d563dd5c91acb8e78108a9c84333b612ceb7084
```

Output: `native/targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.
Project version and packaging name remain unchanged. `build-m5` was preserved.
No flash, upload, board reset or other physical operation was performed.

### Commands and evidence

```powershell
# Use the existing Zig environment described in README.md.
C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe --build native/core/build-zig --target host-test
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
node --import tsx native/core/tools/generate-fixtures.ts --check
node --import tsx native/core/tools/generate-turn-fixtures.ts --check

npm run test -w game -- rng-original.test.ts time.test.ts survival.test.ts loops.test.ts loops-integration.test.ts town-npc-cadence.test.ts npc-pestillo-horario-84.test.ts npc-aitype4-persigue.test.ts npc-aitype6-persigue.test.ts npc-aitype57-hostil.test.ts npc-live-stream.test.ts actor-pool-103.test.ts moon-phase-latch.test.ts trapdoor-fall.test.ts turn-counter.test.ts town-sleep-turn.test.ts
npm run test -w extractor -- native-input.test.ts native-movement.test.ts

# In the installed ESP-IDF environment, from the repository root:
idf.py -C native/targets/tdeck -B C:/Dev/OpenU5-TDeck/native/targets/tdeck/build-core build size
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core
```

Host CTest evidence: `native/core/build-zig/Testing/Temporary/LastTest.log`.
Firmware evidence: `native/core/build-turn/idf-build-size.log` and the final ELF/map.
The generator reports per-family case counts and writes rare-branch coverage
counts into `fixtures/turns.json`. Exact API mapping and remaining scope are in
[WORLD_TURNS.md](WORLD_TURNS.md).

### Mismatches, limits and isolation

- No unexpected behavioral parity mismatches remain in the supported domains.
  No TS behavior or expected value was altered to accommodate native behavior.
- The first ESP compile exposed a portability issue: this toolchain defines
  `int32_t` as `long`, unlike the host's `int`. Explicit template types on
  `std::min`/`std::max` fixed compilation without changing mechanics. Final
  host and ESP builds both pass.
- A relative IDF `-B` initially created a build directory at repository root.
  It was removed after verifying its resolved path and CMake project owner.
  The final build uses the absolute existing target build path above.
- All snapshotted target source/configuration/packager files match their
  pre-batch SHA-256 values. Existing uncommitted M5/5.1 work was preserved.
  This batch's production changes are confined to `native/core`.
- Native scheduled-pathfinding is explicitly unsupported, not approximated:
  `tick_idle_npcs` returns `NeedsPathfinding` without mutations or RNG draws.
  Full `Game` orchestration, enemy placement/combat, rest workflow and excluded
  systems remain untranslated as listed in [WORLD_TURNS.md](WORLD_TURNS.md).
- Host sizes: `GameState` 576, `TurnState` 72, `NpcActor` 66, `NpcActors` 2,120,
  `ActorPool` 1,024 bytes. Core uses no heap allocation. A full NPC container
  and fresh-entry temporary are about 2 KiB each; use caller/static storage
  and review stack budget before hardware integration. Whole world maps and
  many cached town populations are candidates for PSRAM. Host owner indices
  use 64-bit `size_t`; ESP32 uses 32-bit `size_t`. Neither layout is a save codec.

## Historical record: preceding foundation batch

The remainder records the prior batch and its M5.1 baseline comparison. Its
case counts, state size and hashes are historical, superseded above where noted.

## Results

| Check | Result |
| --- | --- |
| Native host CMake `host-test`, Release, Windows x86-64 | 4/4 passed |
| Compiled C++ parity | 7,269 generated reference cases, 65,536-seed raw RNG digest, 512-step raw chain, 120-action mixed replay passed |
| Existing compiled native movement harness | Passed |
| Existing compiled input controller/matrix harness | Passed; input sources unchanged |
| Live TS fixture drift check | Passed; fixtures match executed reference functions |
| Fixture-generator TypeScript typecheck | Passed |
| Existing game tests (six files) | 58/58 passed |
| Existing extractor input/movement tests | 11/11 passed |
| ESP-IDF v6.1 `idf.py -B build-core build size` | Passed |
| Existing Launcher packager | Passed header, chip ID, segment checksum, appended SHA-256 and byte-identical copy checks |
| Physical T-Deck run | Not performed; Milestone 5.1 cleanup remains unverified |

Game tests run:

```sh
npm run test -w game -- rng-original.test.ts time.test.ts party.test.ts \
  party-roster-contiguity.test.ts carpet-passability.test.ts los-passability-audit.test.ts
npm run test -w extractor -- native-input.test.ts native-movement.test.ts
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
```

The host run uses ordinary Windows executable code, CMake 4.0.3/Ninja 1.12.1
and portable Zig 0.13.0 (Clang 18.1.6), using the adapters documented in
[README.md](README.md). The final tested host build is `native/core/build-zig`.
Production core compiles with `-Wall -Wextra -Wconversion -Werror` on this host.
There is no ESP-IDF or board dependency in any production core header/source.

## Firmware size

Baseline is the actual existing `build-m5` firmware, not a reconstruction of HEAD.
That directory and image were preserved.

| Measurement | M5.1 baseline | Foundation build | Change |
| --- | ---: | ---: | ---: |
| App/Launcher `.bin` | 339,728 bytes | 340,544 bytes | +816 bytes |
| Linked image (`esp_idf_size`) | 339,608 bytes | 340,432 bytes | +824 bytes |
| Minimum aligned Launcher allocation | 393,216 bytes | 393,216 bytes | 0 |

The standalone 1 MiB app partition still has 68% free. The core state occupies
568 bytes in ESP `.data` (`core_state`, map symbol size `0x238`), matching the
host `sizeof(GameState)`. Host `sizeof(CharacterState)` is 32 bytes. The live
state is static, not a new 568-byte app-task stack allocation. Unused pure core
functions are compiled by IDF and linker-stripped until used by the application.
No on-device performance or stack margin claim follows from a host test/build.

New app and Launcher image SHA-256:

```text
9d169fb7217f577664f52a94fd7998144189ef13f57b2b333621e788053d61aa
```

Baseline SHA-256:

```text
5784cc45d38fdd1d50d17a3debf73737a8ff08ed70367836fa3d16ccd7e0cb01
```

New output:
`native/targets/tdeck/build-core/launcher/OpenU5-TDeck-M5-Launcher.bin`.
The existing packaging name and project version remain unchanged. No flash,
upload, board reset, or other physical operation was performed.

## Mismatches and resolved validation issues

- **No unexpected behavioral parity mismatches remain** in the supported
  translated domains. Reference outputs were not changed to accommodate C++.
- The existing device slice intentionally differs from full movement: it does
  not dispatch a local exit, returned turn costs, slow/hazard messages or Game
  events. This pre-existing restriction is retained. The pure foot/actor-free
  `resolveStep` projection is tested separately with all of its result fields.
- Native error codes substitute for TS exceptions; bounded integer/roster/name
  domains substitute for arbitrary JS values. These explicit interface/domain
  differences are listed in [PARITY.md](PARITY.md); they are not full-file parity
  claims. Negative time additions, schedule ties, unknown tile behavior, and
  the grass-vs-map-edge-filler distinction match the reference.
- Initial TS party tests failed only because the ignored
  `game/assets/initial-state.json` was absent. It was generated from the existing
  local `original/u5/ultima5/INIT.GAM` using the unchanged
  `extractor/src/parsers/savegame.ts:parseSaveGame`. All selected tests then
  passed. Synthetic core fixtures never require that asset.
- Installed ESP clang could not emit PC executables. A portable PC compiler was
  placed in ignored `native/core/build-tools`, with no system installation.
  CMake's optional verbose ABI probe is unsupported by this Zig linker, but
  its compiler check, actual executable links, and all runtime tests pass.
- ESP-IDF needed its existing tools/venv/version environment initialized; the
  ROM ELF path was also supplied for final configuration. Existing framework
  Kconfig notifications remain. Final build, size command and packaging pass.

## Change isolation

SHA-256 comparison against the start-of-phase snapshots confirms unchanged:
`tdeck_board.cpp`, `tdeck_input.cpp`, `input_controller.cpp`, and
`keyboard_matrix.cpp`. No driver, pin, keyboard, trackball, display, touch,
audio, GPS, LoRa or board configuration was edited in this phase.

Existing uncommitted M5/5.1 changes remain in the working tree. This phase's
production changes are confined to `native/core`, the native movement
compatibility include, small `main.cpp` integration edits, the shared
source build integration, and documentation/build-output ignore rules. The
existing extractor bitmap test now points at the moved core implementation.
No TypeScript game behavior source was edited.

All excluded systems remain untranslated; the complete list and scope ledger
are in [PARITY.md](PARITY.md). This phase stops at the foundation/framework.
