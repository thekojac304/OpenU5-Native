# Real-arena physical-combat validation

## Extract the original maps

Put the original DOS files in `original/u5/ultima5/` alongside the other
extractor inputs. `BRIT.CBT` must be 5,632 bytes; `DUNGEON.CBT` must be 39,424
bytes. From the repository root, the exact command used was:

```sh
npm run extract -- --skip-tiles
```

The existing CLI defaults to that source directory and `game/assets` as output.
It runs the shared `extractor/src/pipeline.ts`, which calls **the same
`parseCombatMaps` function** in `extractor/src/parsers/combatmap.ts` for both
files. No parser or extractor wiring needed changing. The full pipeline also
refreshes its other generated assets and manifest; `--skip-tiles` skips tile
art generation.

Canonical result: **`game/assets/maps/combatmaps.json`**. On this checkout:
`C:\Dev\OpenU5-TDeck\game\assets\maps\combatmaps.json`.
It contains 16 Britannia maps followed by 112 dungeon maps. Each map has an
11×11 row-major grid, six party starts per direction, nonzero-sprite unit slots,
and nonzero-sprite triggers. The parser intentionally preserves units at (0,0).

Both `original/` and `game/assets` are intentionally gitignored. The JSON and
all real-data parity transports/expectations remain generated, untracked data.
No hand-authored arena data is used.

| File | SHA-256 |
| --- | --- |
| BRIT.CBT | `ea93924f649eb2406df8507f008e6eadc7dbf54b8822ea6cda6f44fbd3429aac` |
| DUNGEON.CBT | `101f127141b5e90d7186263e47c81ee496c0e6a44acbe0a39eadc646090f9936` |
| combatmaps.json | `ecc9c0286d4f625ed6bae85eee4e9c611aced23b42be709df2e5854af80389ea` |

## Reproduce validation

Run the 14 previously skipped game suites and combat-map parser suite, plus
the existing selected reference regressions:

```powershell
./native/core/tools/test-combat-reference.ps1
npm run test -w extractor
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
```

Enable original-map validation in a configured host build (compiler setup is
in [README.md](README.md)). The build used here is `build-zig`:

```sh
cmake -S native/core -B native/core/build-zig -DOPENU5_REAL_ARENAS=ON
cmake --build native/core/build-zig --target host-test
```

With this option enabled, missing source files, missing JSON, missing Node,
stale JSON, generator failures, and parity mismatches fail validation. CTest's
`real_arena_reference` fixture setup regenerates the ignored real expectations;
`real_arena_parity` depends on successful setup. Default asset-free host builds
retain their existing synthetic corpus.

For a direct real-data run after compiling `combat_parity_tests`:

```sh
node --import tsx native/core/tools/generate-combat-fixtures.ts --real-arenas
native/core/build-zig/combat_parity_tests.exe native/core/build-real-arenas/combat.txt native/core/build-real-arenas/maps.txt
node --import tsx native/core/tools/generate-combat-fixtures.ts --real-arenas --check
```

The generator first compares the entire JSON with a fresh call to the existing
CBT parser. It transports those JSON grids, starts, unit coordinates and triggers
to C++; there is no second CBT decoder. Both engines receive the same unmodified
arena data. Expectations come from the unchanged TypeScript engine. The test
reports the first differing scenario, action and snapshot field and exits with
failure. Real expectations live in ignored `native/core/build-real-arenas/`.

In the existing ESP-IDF environment, firmware verification remains:

```sh
idf.py -C native/targets/tdeck -B C:/Dev/OpenU5-TDeck/native/targets/tdeck/build-core build size
python native/targets/tdeck/package_launcher.py --build-dir native/targets/tdeck/build-core
```

## Results — 2026-09-15

- **15/15 formerly skipped suites executed and passed**: 14 game suites
  (183 tests) and the parser suite (12 tests).
- Selected TypeScript reference run: **258 tests in 21 files passed**.
- Full extractor: **226 passed, 2 unrelated tests skipped**, 29 files passed.
  The skips are the optional user seed save
  `original/u5/saves/partida-javier-2026-07-15/SAVED.GAM` and the historical font
  atlas comparison. The font test uses URL `.pathname` for its existence guard,
  which does not resolve the generated file as a Windows filesystem path.
  **No combat-map-dependent tests remain skipped in the requested suites.**
- Native host: **16/16 CTest checks passed**, including the six existing
  synthetic TS drift checks, all native regressions, and the real-map fixture
  setup/parity pair. Fixture TypeScript typecheck passed.
- New real-map corpus: **159,744 parity snapshots**, from 128 maps × four
  directions × eight seeds = 4,096 scenarios, each with 39 snapshots.
- Total parity: **352,349** = previous 192,605 + new 159,744. The existing
  97,344 synthetic combat snapshots remain unchanged. Seven native adapter
  checks are additional and are not counted as parity snapshots.

Snapshots cover initialization, party/enemy placement, actor selection,
movement/passability, physical attacks, terrain mutation, room triggers,
victory/exit state, ordered events, RNG draws, and encounter start/end handoff.
All 15,488 real terrain cells are included in initialization snapshots; all four
sets of party starts are exercised. Required aggregate branch coverage includes
movement, blocking, deaths, projectiles, spent triggers, victory, combat end and
same-exit refusal; counts are in `build-real-arenas/combat-coverage.json`.

### Mismatch corrected

**Combat logic**, not parsing, representation, placement or fixture generation:
native movement inferred the exit border from the destination coordinates.
TypeScript `borderForCell` uses ordered direction-string checks; because
`"west"` and `"east"` contain `"s"`, a horizontal exit from the bottom row
records **south**. The first failure was CampFire, north entry, seed 51,
scenario k=3, row 967: native border west (1), TS south (2).

Native `Engine::move` now matches the reference's ordered checks exactly.
The original real-data expectations were retained and the full corpus passed.
No TypeScript behavior or parser behavior was changed. No observed mismatch
remains unresolved.

### Firmware and scope

ESP-IDF build, size check and Launcher validation passed after the correction.
App/Launcher image: **341,712 bytes**, linked image: **341,588 bytes**,
minimum aligned Launcher allocation: **393,216 bytes** — all **0-byte deltas**.
Final app and Launcher SHA-256 remain
`5872cbdf9779fa36e2657147c60910a00e9e0cc4685bc6b15ff3cdec03ff5cf1`.
All **33/33** snapshotted target source/configuration/packager hashes are unchanged.
Unused combat code is stripped from the existing firmware slice; this is not a
measurement of the cost of enabling device combat. No flashing was performed.

This validates the existing encounter-array physical-combat projection on all
real arena layouts, including dungeon room movement/triggers/exits. Enemy types
are the harness's controlled physical definitions, placed using the real slots.
Fixed dungeon sprite decoding, random EC groups, object/field seeding, quest
absorption, full dungeon world aftermath and advanced abilities remain outside
the translated domain described in [COMBAT.md](COMBAT.md). No magic, dialogue,
shops, quests, UI or hardware/input/display work was added.
