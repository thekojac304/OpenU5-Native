# World-turn and scheduling foundation

Scheduled travel and generic transitions now have a follow-up implementation:
see [TRAVEL.md](TRAVEL.md). Use `tick_npcs` and `scheduled_npc_hook` for scheduled
and cross-floor movement. The limitations below describe the earlier batch;
the idle-only compatibility API retains its preflight contract.

This batch translates platform-independent turn primitives from the current
TypeScript implementation. It does not enable world turns on the T-Deck.
Milestone 5.1 input cleanup still needs physical verification.

## Reference mapping

All reference paths are relative to `game/src/core/`.

| Native API | Authoritative TypeScript | Translated domain |
| --- | --- | --- |
| `advance_clock`, `refresh_moon_phase_latch` | `world/survival.ts:advanceClock`, `refreshMoonPhaseLatch`, `relocateShadowlordsAtMidnight` | Gameplay clock, Q/T handling, torch/light decay, single calendar carry, monthly counters/inn aging, ordered midnight rejection sampling, optional sky latch |
| `apply_damage`, `party_random_damage`, `turn_housekeeping` | `world/survival.ts:applyDamage`, `partyRandomDamage`, `turnHousekeeping`, `ringRegenSweep` | Positive world damage, death/deselection, poison tick census, meals/starvation, unsaturated turn counter, spell duration, regeneration |
| `advance_turn` | `world/movement.ts:advanceTurn` | Shared advance-clock then housekeeping sequence; also exposes poison tick indices as diagnostics |
| `maybe_change_wind` | `world/wind.ts:maybeChangeWind`, `setWind` | Exact rejection loop and drift-counter reset |
| `outdoor_turn` | `world/loops/turn.ts:outdoorTurn` | Full pure loop function and callbacks, including blocked turns and `skipWorldTurn` |
| `town_turn` | `world/loops/turn.ts:townTurn`, `drunkConfusionRoll`, `enAlfombra` | Full pure loop function, confusion draws, waking, trapdoor re-read/guard, burning/swamp, housekeeping and NPC callback cadence |
| Internal outdoor/town hazard helpers | `world/loops/hazards.ts:underworldHazard`, `bridgeTrollAmbush`, `trollToll`, `swampPoison`, `townSwampPoison` | Trigger rolls, damage/poison, troll payer/toll and ordered dex rolls; no combat or toll command |
| `spawn_threshold`, `roll_spawn_gate` | `world/loops/spawn.ts:spawnThreshold`, `rollSpawnGate` | Gate only, strict threshold comparison; no spawn placement or enemy definitions |
| `town_npc_tail_runs` | `world/loops/turn.ts:townNpcTailRuns` | Mounted-byte range, pass exception, T/Q ordering; caller-owned visit phases |
| `outdoor_world_turn_runs` | `game.ts:Game.outdoorWorldTurnRuns` | Q before transport phase, T stop; fixture calls the actual TS prototype method on a minimal receiver |
| `enter_npc_map` | `npc/manager.ts:NpcManager.enterMap(restore=false)`, `npcSlotObjectKind`, `normZ` | Fresh activation, input ordering, slot-zero/empty/object/dead filtering, schedule placement and walk-state reset |
| `npc_check_schedule` | `npc/manager.ts:npcCheckSchedule` | All six floor partitions, served-slot latch, exact start hour and at-post override |
| `npc_occupied`, `npc_ai_step` | `NpcManager.isOccupied`, `aiStep`, `wanderStep`, `runAwayStep`, `chaseStep`, `chaseMove`, `hostileStep`, `fleeStep`, `stepDir` | All eight basic AI modes; current TS distance, tie, erratic RNG and boundary behavior |
| `tick_idle_npcs` | `NpcManager.tick`, `tickInner` | Exact subset with idle actors, no buffered path and no newly required scheduled walk; stable slot ordering and shared RNG |
| `tick_guards` | `NpcManager.tickGuards`, `guardsOnFloor`; `world/loops/guards.ts:guardWanderStep` | Guard selection/order, neighboring blockers, destination bounds/passability/occupancy, facing and draw order |
| `find_free_actor_slot`, `first_free_recycle_slot`, `scan_recyclable_slot`, `acquire_actor_slot`, `compose_world_pool` | Same camelCase functions in `world/actorPool.ts`, `ACQUIRE_CASCADE` | Full shared pool allocation/composition projection, slot mutation, collision overwrite order, crown exclusion and wrapped visibility |

The fresh NPC entry's serialized `npcWalk` mirror is represented directly by
`NpcActors`; native save serialization is not implemented. AI probe/debug counters
are instrumentation, not game state. No TypeScript runtime source was changed.

## State, events and ownership

`GameState` keeps the existing party/time/position foundation. `TurnState` adds
the turn-specific fields without allocating them in the device loop. Both are
caller-owned values. RNG is supplied explicitly through `Rand`; `rng_source`
adapts the existing `OriginalRng`. Callbacks must share that stream when the TS
caller does. There is no new singleton RNG, clock or actor manager.

`TurnResult` explicitly separates ordered message IDs, poisoned indices, poison
tick indices, optional spawn/troll results and hazard/burning flags. Text comes
from `turn_message_text`. These are the outputs of the TS pure turn functions,
not a claim to have translated `Game`'s entire `GameEvent` dispatcher. In
particular, outdoor burning remains a flag, not an inserted message. Trapdoor
effects come from the supplied callback. A `PartyKilled` callback result does
not itself kill anyone; that mirrors TS's `"tpk"` result contract.

`TraceSink` streams the exact internal RNG site/range/value sequence without
allocation or truncation. Callback draws are outside that internal trace in TS;
fixtures compare an additional complete draw stream so their placement is tested.
The clock inherits the currently selected trace site, even across midnight.
Result arrays are bounded by the reference: at most six member indices and at
most eleven messages (Hic + nine guarded trapdoor iterations + starvation).
Public housekeeping returns a fresh result, preventing accidental accumulation
across turns. Hooks are synchronous and their contexts must remain valid.

`NpcActors` holds one active location. `tick_idle_npcs` preflights the whole
list and returns `NeedsPathfinding` before any mutation or RNG consumption if
an active/new scheduled walk is needed. It does **not** teleport walkers, run
idle AI instead, or silently omit an actor. This is an explicit scaffolding
boundary. The full six-way schedule latch remains separately available and
tested. Caller-supplied effective map tiles incorporate door overrides; door
timers and map loading are not implemented here. Counts must fit capacities,
and records must use valid schedule slots and coordinate domains.

Pool owners use kind + caller-array index instead of a JS object reference.
Keep the referenced array order stable while using a composed view. Objects on
other floors still reserve slots when their location matches, as in TS. Duplicate
slots are intentionally overwritten by later entries, with objects after enemies.
Town NPC slot IDs are not pooled actor IDs; TS itself does not compose them here.

`advance_turn` integrates the movement layer's reusable time/state progression
with the new framework. Existing pure movement and the M5 device movement slice
keep their existing contracts. Calling a turn automatically from that slice
would change unverified hardware-facing gameplay behavior, so no such call was
added. There are no ESP-IDF, T-Deck, rendering or filesystem includes in core.

## Numeric and optional-field contract

- `turns_since_start` is now signed 64-bit to preserve TS's deliberately
  unsaturated counter, including progression across `0xffffffff`. Supported
  exact integer values end at JavaScript's `2^53-1` safe-integer limit.
- Gameplay `advanceClock` intentionally carries only once per call, unlike
  `advanceMinutes`. Negative minutes return before changing `prevHour`; zero
  minutes may refresh the sky latch. Q halves positive minutes, minimum one.
  T records `prevHour` but does not advance time. Inputs/intermediate results
  must fit signed 32-bit clock arithmetic; fixtures include 2,147,483,000 minutes.
- Clock and housekeeping may expire T before later spawn/NPC phase gates in the
  same turn. This ordering is preserved. Meals count a poisoned member even if
  that tick killed them. Dead/sleeping handling differs by helper exactly as TS.
- World damage is positive with native word HP/food and byte character statistics.
  General signed combat/healing arithmetic is not implied by `apply_damage`.
- `time_spell=0` means absent; `spell_turns=-1` means absent, and 255 permanent.
  Missing `prevHour` can be supplied as -1 for the valid 0..23 clock domain.
  Absent light/drunk/wind fields use their zero-value projection; this is not an
  absence-preserving save codec. Shadowlord presence has a separate flag, with
  -1 representing an absent entry. Native .GAM location values are nonnegative.
- Basic NPC x/y are 0..31, normalized floor -1 or a native floor byte. The crossed
  `stepDir` boundary guards and 65-value wander direction draw are preserved.
  Schedule hour/times use bytes. Pool visibility uses byte masks, not Euclidean
  distance, and owners/coordinates remain explicit.

## Fixtures and verification

`tools/generate-turn-fixtures.ts` executes repository TypeScript functions on
synthetic inputs and emits `fixtures/turns.json` plus equivalent C++ numeric
records. The format is test-only, not a save format. Field order is defined in
`state`, `slotWire`, `actorWire` and each case producer. C++ consumes it in
`tests/turn_parity_test.cpp`; failures report row, field and both values.

| Case family | Reference cases |
| --- | ---: |
| Repeated town/outdoor/advance-turn actions | 6,912 |
| Town/outdoor phase gates | 1,008 |
| Schedule latch floor/start-hour matrix | 1,296 |
| Fresh NPC entry | 192 |
| Repeated guard + basic NPC ticks | 4,608 |
| Pool assignment/recycling | 256 |
| Standalone clock boundaries and optional contexts | 324 |
| Spawn threshold/gate boundaries | 756 |
| Combined town → guards → NPC → second wind replay | 256 |
| **New total** | **15,608** |

Repeated replays retain native state across actions and verify each next input
against the reference, rather than reinitializing every row. Comparisons cover
time, party damage/status/resources, world position, turn fields, schedule/walk
state, actor positions, pool ownership/slot mutations, messages/flags, phases,
RNG seed and internal/complete draw traces. The combined replay verifies actual
NPC mutation inside the town callback on the shared stream.

The generator asserts coverage of rare earthquake/spawn/troll branches, a troll
payer, town/outdoor burning, trapdoor `none`/`tpk` callbacks, and the nine-fall
guard. Tests include short rosters, empty/no-op paths, blocked movement, map
edges, multiple actors, starvation, basement/underworld masks, Q/T duration,
monthly/yearly changes and counter progression beyond 32 bits. A host-only
assertion verifies unsupported scheduled walks return atomically.

The existing 7,269 foundation cases and exhaustive 65,536-seed RNG digest remain.
Together the two generated corpora have **22,877 cases**. No unexpected
behavioral mismatches remain within the documented domains.

## Deliberately untranslated

- Full `Game.move`/`pass`/command dispatch and complete `GameEvent` presentation.
- Scheduled NPC path scan/backtrace/follow, stuck recovery, scan budget and
  cross-floor travel; saved `npcWalk` restoration and per-location caches.
- NPC dialogue/possession/arrest/quest reactions and actor encounter resolution.
- Enemy placement, overworld enemy movement/attacks, spawn coordinate selection,
  encounter instantiation, troll toll command continuation and full combat.
- Full camp/rest workflow, wake repositioning, doors/timers, transport/drift,
  world-object interaction, map transitions and dungeon engine.
- Magic commands, dialogue, shops, quests, save codecs, UI and audio.
- All T-Deck drivers/input/display/SD/touch/audio/GPS/LoRa and Launcher behavior.

These are scope boundaries, not placeholder implementations. The batch stops
here; physical hardware is not required for any check above.
