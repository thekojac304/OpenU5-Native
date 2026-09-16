# Reference map, parity contract and remaining work

## Latest batch: platform-independent persistence

See [PERSISTENCE.md](PERSISTENCE.md) for source mapping, binary compatibility,
sidecar/envelope precedence, live-state adapters, generation recovery and memory
limits. **619 new / 1,242,170 total compatibility scenarios** pass, plus **11
native persistence contract scenarios** kept outside the TypeScript parity total.
Native-produced saves/envelopes load back into TypeScript; two real save sources
complete six repeated round trips. No TypeScript runtime behavior changed.

Codecs and byte/document save/load hooks are implemented. Application/service
capture and restore wiring, SD/FATFS, UI, shops and quests remain deferred.
Session-only RNG/combat/dialogue/dungeon objects are not silently added to the
save format; their exact reference boundaries are listed in PERSISTENCE.md.
The retained document preserves world/quest/QoL data whose live owners remain
outside the bounded GameState. This supersedes historical blanket persistence
exclusions only within the documented codec and adapter domain.

## Previous batch: generic dialogue/conversation

See [DIALOGUE.md](DIALOGUE.md) for the exact TypeScript source map, text/state
contracts, asset path map and explicit quest/shop handoffs. **250,226 new /
1,241,551 total** parity snapshots cover the resumable TLK engine, effects,
semantic Talk/input, NPC meeting/recruitment/alarm state and event order.
All 135 real dialogue records and 313 labels are exercised, with 2,023 distinct
nonempty branch lines entered (1,623 without isolated-label redirects).
Every UTF-16 leading code unit, generic counter/roster edges, 7,124 orchestration
snapshots and 4,096 alarm seeds are included. No TypeScript behavior was changed.

There are no native dialogue asset skips. Two broader TypeScript tests fail for
the missing optional `game/e2e/espejo-tour/saves/ad01.gam`; they are not reported
as passed. Full shops, quests, persistence and UI remain deferred. Historical
blanket dialogue exclusions below are superseded only within DIALOGUE.md's domain.
See [VALIDATION.md](VALIDATION.md) for final tests, sizes and memory limits.

## Previous batch: dungeon/world orchestration

See [DUNGEON_WORLD.md](DUNGEON_WORLD.md) for the exact source map and boundaries.
**381,952 new / 991,325 total** checked snapshots/sequences cover dungeon rules,
entry/exit/levels, corridor arenas, fixed map/enemy/object setup, chest/loot,
board/disembark rules and dungeon combat return routing. Generic scripted entry
and semantic hooks are implemented. Non-foot semantic Move and quest narrative
remain explicit limitations; this is not a claim of complete transport gameplay.
Current evidence, including unchanged TS UI/e2e failures, is in VALIDATION.md.

## Previous batch: combat magic and advanced generic combat

See [MAGIC.md](MAGIC.md) for source mapping, resource/RNG/event behavior,
caller-owned growth, field/status integration and explicit exclusions.
**257,024 new / 609,373 total snapshots pass**, including **106,496** advanced
snapshots across all 128 real arenas and four entry directions. Historical
magic/special-AI/growth exclusions below are superseded within that domain.
Validation and memory measurements are in [VALIDATION.md](VALIDATION.md).

## Previous batch: physical combat foundation

See [COMBAT.md](COMBAT.md) for exact source mapping and the supported
encounter-array physical-combat domain. **97,344 new / 192,605 total** generated
parity snapshots pass, plus seven new native adapter checks. Actor placement,
countdown turns, movement, melee/ranged physical attacks, armor/damage/death,
ammo, victory/escape, generic physical enemy turns, world encounter handoff and
semantic combat commands are translated. Events and all RNG draws are compared.

Combat magic, special AI, dynamic actor growth, fixed dungeon-map seeding,
quest/arena interactions, full world aftermath and device integration remain
explicitly deferred. Missing real combat assets do not block synthetic parity.
All blanket historical statements below that combat is untranslated are now
superseded **only within [COMBAT.md](COMBAT.md)'s admitted domain**.
Current validation and memory measurements are in [VALIDATION.md](VALIDATION.md).

## Previous batch: inventory, equipment, item use and rest

See [ITEMS_REST.md](ITEMS_REST.md) for the exact source mapping, representation,
event/RNG/turn contracts and explicit deferred boundaries. **38,728 new /
95,261 total parity cases**, plus 11 new native adapter checks, pass. Supported
Ready/Unready/Ignite/UseItem/Rest commands are integrated with the existing state
and command engine. Camp, bed sleep, ship repair and camp-owned apparition
recovery are translated. Combat ambush initiation remains an explicit handoff.

The historical inventory/rest omissions and 576-byte state claims below are
superseded within this scope. `GameState` is now 1,720 bytes; no raw-struct save
format is implied. Full combat, magic, quests, UI and device integration remain
deferred. All target source/configuration files snapshotted in this batch are
unchanged; physical input verification remains outstanding.

## Previous batch: semantic command orchestration

See [COMMANDS.md](COMMANDS.md) for the current source map and boundaries:
**13,824 new / 56,533 total parity cases**, plus 13 native adapter checks.
The command dispatcher, supported Game actions and ordered event projection
are now implemented. Historical statements below about all Game orchestration
being untranslated are superseded within that documented scope. Travel and
scheduled/general pathfinding status is recorded in [TRAVEL.md](TRAVEL.md).

## Historical batch: world turns and scheduling

The next platform-independent batch is complete within the domains in
[WORLD_TURNS.md](WORLD_TURNS.md). That ledger maps every new API to the exact TS
functions and defines the explicit scheduled-pathfinding boundary. It adds
15,608 reference cases, bringing the generated total to 22,877 plus the existing
exhaustive 65,536-seed RNG digest. The foundation ledger below remains valid;
its original case count is historical. `turns_since_start` is now 64-bit.

## Architecture inspection

The authoritative implementation is `game/src/core/`, not a reconstruction of
Ultima V from memory. Inspection for this phase covered:

- `state.ts`: serializable state, `createNewGame`/`createBaseGame`, initialization
  copies, optional-save defaults and absence-sensitive fields.
- `game.ts`: `GameData`, `GameSystems`, `GameEvent`, `Game`, live RNG ownership,
  orchestration and subsystem injection. These responsibilities must stay above
  pure helpers; a direction alone is not a complete turn.
- `world/map.ts`, `movement.ts`, and the world directory: map access and pure
  `resolveStep` are distinct from `tryMove`, survival, actor occupancy, chunk
  origin, transport, world objects, NPC loops and encounter processing.
- `party.ts`, `time.ts`, `rng-original.ts`: foundational functions listed below.
- `combat/combat.ts` and companion modules: separate combat state/events,
  equipment and RNG consumers; not translated.
- `dialogue/conversation.ts`: scripts, contexts, effects and conversation state;
  not translated. Party selection does not import this layer.
- `magic/`: cast effects, area geometry/tables and resource mutations; not translated.
- `shops/shops.ts` and pickers/tables: pricing, purchases, inns and UI state;
  not translated, including roster movement for inn/join behavior.
- `quest/`: item grants, words, flags/keys, endgame reporting and world seeding;
  not translated.
- `saveNative.ts`: 16 records of 32 bytes, explicit little-endian save window,
  bitmap grids, contextual signed floor decoding, native object/enemy mirrors,
  and sidecar state. `u5gam.ts` wraps native bytes with metadata/sidecar validation.
  `state.ts` JSON serialize/deserialize has a separate default/migration contract.
  None can be replaced with a raw native struct dump.

Future logical modules can extend `state`, `world` and a game dispatcher, then
add separate combat/dialogue/magic/shops/quest/persistence modules when explicitly
authorized. Empty implementations of those systems are intentionally absent.

## Translation ledger

Paths below are relative to `game/src/core/`. “Parity” means only the stated
function/domain, not an entire TypeScript file or full game subsystem.

| Native symbol/module | Exact TS reference | Status and tests |
| --- | --- | --- |
| `Direction`, `direction_delta` | `world/movement.ts:Direction`, `DIRECTION_DELTA` | Four directions; fixtures and every movement direction |
| `Position`, `WorldPosition`, `MapId` | `state.ts:Position`; `world/map.ts:ActiveMap` | Scaffolding, byte x/y, separate location/floor; negative basement fixture |
| `OriginalRng` | `rng-original.ts:OriginalRng` constructor, `seed`, `getSeed`, `nextRaw16`, `next` | Parity over word seeds and integer range domain; raw chain, exhaustive one-step digest, signed ranges and rejected ranges without progression |
| `time_hash_seed` | `rng-original.ts:timeHashSeed` | Signed integer inputs; mask boundaries |
| `GameTime`, `advance_minutes` | `time.ts:GameTime`, `advanceMinutes` | Copy-return calendar arithmetic, positive carry only; negative, zero, multi-year, day/month/year boundaries |
| `day_phase`, `schedule_index` | `time.ts:dayPhase`, `scheduleIndex` | Hour transitions; all 256 hours for four schedules, ties and index 3 -> 1 |
| `CharacterState`, `PartyState`, `GameState` | `state.ts:CharacterState`, foundation fields of `GameState` | Bounded scaffolding; all character scalar fields and native-length names copied/tested |
| `create_foundation_state` | `state.ts:createNewGame` -> `createBaseGame` | Foundation projection only; copied initialization, independent roster/name data, foot/version defaults; excludes gypsy creation, inventories and optional full-state fields |
| `party_members` | `party.ts:partyMembers` | Scan roster by `partyStatus==0`, first six, independent of partySize; returns indices instead of object references |
| `first_conscious_index` | `party.ts:firstConsciousIndex` | First G/P in prefix min(partySize,6); short/empty/negative-size synthetic cases; does not conflate with the other consciousness function |
| `wrap_coord`, `get_active_map`, `ActiveMap::tile_at` | `world/map.ts:wrapCoord`, `getActiveMap` including returned tileAt/edgeFillTile | Large wrap, local bounds, (31,31) edge fill, separate -1/255 floor behavior, missing map errors; rectangular complete buffers |
| `in_bounds`, `target_for_step` | `world/movement.ts:resolveStep` coordinate/bounds branch plus preserved M5 geometry | Geometry utility, byte-representable dimensions 1..256; invalid dimensions explicitly rejected |
| `tile_properties`, `is_walkable_tile` | `tiles.ts:TILE_INFO`, `tileInfo` | Basic walk/horse/skiff/boat property projection; generated final values including overrides; all entries plus unknown/negative IDs |
| `is_passable` | `world/movement.ts:isPassable` | All five predicate modes; carpet uses reference water/stream/walkability rule, not `carpetPassable` raw data |
| `terrain_speed_class` | `world/movement.ts:terrainSpeedClass` | Exact slow set and 9..15 range; all tile fixtures |
| `resolve_unoccupied_foot_step` | `world/movement.ts:resolveStep`, restricted to foot and actorTile=0 | Position and every result field compared, every base tile/direction/local-large combination plus edges; clock, RNG, turns unchanged |
| `apply_movement_slice`, `MoveReport` | Prior native M5 contract; position-only projection of the above reference | Device compatibility, not full `resolveStep`/`Game.move`; no exits, turn costs or Game events dispatched |
| `MoveAction`, `CommandResult`, `GameEvent` | `game.ts:GameEvent` and direction entry to `Game.move` | Interface scaffolding only; four event kinds reserved, full dispatcher/events untranslated |

## Fixture and differential workflow

`tools/generate-fixtures.ts` imports and **executes** the real TS functions.
It emits `fixtures/foundation.json` (schema 1, named numeric tables) and the
equivalent typed C++ include. The JSON is human/tool readable; `.inc` lets the
host run without adding a JSON parser to production or test dependencies.
The generator defines each row's field order adjacent to its producer; the
consumer is `tests/parity_test.cpp`. Table names identify subsystem fixtures.

The current corpus has 7,269 cases plus a digest over all 65,536 raw RNG seeds.
It includes a continuous 120-action replay mixing explicit RNG draws, clock
advances and moves. Both implementations start at the same synthetic state and
compare coordinates, time, RNG seed, selected party state, map ID, result fields
and empty emitted-event projection after each action. Explicit test clock/RNG
actions are not a new in-game command system.

The C++ test executable compares actual return values and mutations against
those expected values, with checks active in Release builds. Failure messages
name the fixture table/row and C++ assertion line. `typescript_fixture_drift`
recomputes every artifact and fails if the current TS reference no longer
matches. Tests never edit TypeScript behavior or silently regenerate expectations.
The old compact 256-tile movement bitmap is retained for the device slice; C++
fixtures and the existing extractor test verify it against final `TILE_INFO`.

Pure helpers emit no `GameEvent`s. Their messages are return fields, not event
emissions. The slice's empty event count is tested. Full `Game.move` event
ordering is **not claimed tested or translated**. Its parity will need action
traces with full state/event payloads and the correct shared RNG stream when
that orchestrator is translated.

All committed fixtures are synthetic or existing tile-property metadata, with
no original map/save assets. Tile metadata attribution remains
`game/src/core/data/ATTRIBUTION.txt` (Ultima5Redux, Brad Hannah, MIT).

## JavaScript/C++ semantic boundaries

1. **Numbers:** JS uses doubles, bitwise operators coerce to 32-bit integers.
   Native public numeric inputs represent finite integers in their declared
   ranges. NaN, infinity, fractions and string coercion are outside this phase's
   contract; no parity is claimed for them. JS integers above native ranges must
   be validated by future codecs/adapters before conversion.
2. **RNG:** addition/rotation uses unsigned widened intermediates then explicit
   word truncation. Seed inputs are signed 32-bit integers masked to 16 bits.
   Range endpoints are signed 32-bit integers, with an int64 span; a 65,536 span
   is NOT word-wrapped. Reversed/empty ranges return `InvalidRange` in place of
   TS `RangeError`, before consuming RNG. The optional global JS debug probe is
   instrumentation, not game state, and is not installed or reproduced here.
3. **Clock:** no byte overflow is invented. Signed fields allow the exact TS
   negative-minute result (e.g. midnight plus -1 retains minute=-1). Carry is
   positive only. int64 intermediates prevent int32 addition overflow, with the
   supported domain requiring resulting fields to fit int32. The calendar is
   13 months of 28 days, not Gregorian. This is not `survival.advanceClock`.
4. **Signed coordinates/floors:** byte x/y are promoted before subtraction;
   negative remainders normalize before conversion back to bytes. A small-map
   basement floor can be -1, whereas location 0 underworld is specifically 255.
   Future `.GAM` decoding must reproduce `parseSaveWindow`'s contextual conversion,
   not universally cast a floor byte to signed/unsigned. No codec is implemented.
5. **Arrays:** native rosters have capacity 16 plus actual count (0..16); callers
   must supply valid counts and native-length names. Short arrays are exercised;
   sparse arrays/undefined records are not represented. `partyMembers` returns
   indices that refer to current records, not copied characters or stable identity
   handles. Roster mutation and name normalization remain untranslated.
6. **Strings:** the scaffold's 8-byte name plus terminator fits the native save
   domain. Arbitrary JS Unicode/long names and `effectiveName` trimming/casing
   are not translated and must not be silently truncated by a future adapter.
7. **Maps:** JS nested arrays/Map entries become borrowed flat buffers. Factory
   map sizes must be exactly 256x256 or 32x32. Missing locations/floors return
   `MissingMap` instead of throwing. Invalid/null/truncated buffers return
   `InvalidMap` rather than exposing JS undefined or C++ out-of-bounds memory.
   Duplicate IDs use first-match order; providers should supply unique IDs.
   Local tileAt off-map is -1, independently of edgeFillTile at (31,31).
8. **Tiles:** IDs are signed integers, not all bytes: -1 is off-map and the table
   includes graphics IDs above 255. Unknown nonnegative IDs return `UnknownTile`
   where TS throws; negative passability returns false before lookup as in TS.
   Existing TS deliberate deviations from the DOS game (including tile 255 and
   BrokenShrine) remain unchanged.
9. **State:** native value copying replaces `structuredClone` for the supported
   fields. No inventory, optional undefined-sensitive state, NPC arrays, journal,
   quest state or serialization migrations are defaulted/invented. Full state
   and initialization are explicitly incomplete. RNG is explicitly carried in
   native state although TS `Game` owns it beside the serializable object.
   Character statistics currently use native save byte/word domains; future
   combat translation must audit transient negative/overflowing TS values before
   reusing those widths. The scaffold is not evidence for combat arithmetic.
10. **Events/errors:** explicit result codes replace exceptions to work in the
    existing exception-disabled firmware. They are adapter-level equivalents,
    not game messages. Full Game events, ordering and commands remain reserved.

## Deliberate device slice limitation

The TS pure resolver returns `exitedMap=true` for a valid local foot exit; the
device slice blocks the boundary without a prompt. It also ignores returned
time costs, slow-progress messages and hazard flags. This is the pre-existing
Milestone 5 restriction, preserved at the user's request while the input cleanup
is unverified. The exact pure foot projection exists and is tested separately;
no complete `resolveStep`, `tryMove`, or `Game.move` claim is made.

## Untranslated after this pass

- Complete GameState/INIT data, inventory, optional defaults, gypsy creation,
  JSON save migrations, binary native codecs and sidecar envelopes.
- Party roster swaps/compaction/pickup, join outcome logic, names/labels and UI.
- Complete `Game` command orchestration/event dispatch and device world-turn integration.
- Movement for other transports, door overrides/timers, map transitions, chunk
  origins, world-object interaction and encounter resolution/placement.
- Scheduled NPC pathfinding, active walk processing, cross-floor travel,
  persisted walk restore and full rest/camp orchestration. Clock, housekeeping,
  pure town/outdoor loops, basic NPC/guard AI, schedule latches and pool occupancy
  are now translated within [WORLD_TURNS.md](WORLD_TURNS.md)'s explicit domains.
- Combat, dialogue, magic, shops, quests, dungeons, save/load UI, audio,
  web/mobile UI, enhanced presentation and all new hardware behavior.

Stop after this world-turn foundation. Do not extend device gameplay until its
hardware validation constraint has been addressed in a separately authorized batch.
