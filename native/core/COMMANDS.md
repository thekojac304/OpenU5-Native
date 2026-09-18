# Semantic command orchestration

## Current integration

[GAMEPLAY.md](GAMEPLAY.md) is the current orchestration/reachability ledger.
Non-foot movement, bridge troll continuations, outdoor encounters, generic
Open/Jimmy/Push/Get/Look/Use/Cast/Attack/Fire, rest ambushes, dungeon camping,
terrain ownership and buffer adapters are now connected. Earlier restrictions
below describe the initial command batch and are superseded where listed there.

## Inventory/rest extension

The current command engine additionally supports resolved `Ready`, `Unready`,
`Rest`, `RestCancel`, and `UseItem` (extended IDs 34/35/37), plus primary `Ignite`.
See [ITEMS_REST.md](ITEMS_REST.md) for exact TypeScript mappings, turn costs,
messages versus ItemResult fields, rest services and the pending combat handoff.
This extension supersedes earlier inventory/rest omissions below. No hardware
input mapping has been added. Current total parity: **95,261 cases**.

## Scope

This batch translates the supported projection of `Game`, using the existing
movement, world-turn, survival, NPC and transition primitives. It does not enable
the command engine on the device. All hardware sources and configuration remain
unchanged; the T-Deck input cleanup still needs physical verification.

`dispatch_world_command(context, command)` is the new-command entry point.
`execute_command` invokes the corresponding Game action directly, useful for
already-dispatched actions and reference tests. Both receive semantic enums,
never keycodes, browser events or trackball data.

| Command | Reference and supported behavior |
| --- | --- |
| `Move(direction)` | `Game.move`: foot movement, drunken direction remapping, NPC blocking, direction echo, result messages, cactus damage, movement events, automatic stairs, context turn, ordered on-step extension calls |
| `Pass` | `Game.pass`: context turn with the town pass-command cadence exception |
| `Enter` | `Game.enter`: interior rejection, outdoor enterable-tile messages, ruins turn, missing location/map no-op, ordinary small-map entry |
| `Exit` / `DeclineExit` | `Game.confirmTownExit`: accepted boundary response exits with no turn; declined response runs a town turn. Native requires an outstanding boundary prompt |
| `Klimb` | `Game.klimb` / `klimbTown`: horse rejection, ladders/grates, semantic direction request, low-wall/fence traversal, invalid target without a turn |
| `KlimbCancel` | `Game.klimbCancel`: town turn; outdoor no-op |
| `AutoSleep` | `Game.townAutoSleepTurn`: sleeping-party turn or no-op |

There is no arbitrary up/down-floor command: the authoritative `Klimb` action
chooses up/down from the ladder or grate under the party. Automatic stairs
choose from the incoming movement direction. This preserves validation rather
than exposing an unrestricted floor mutation as a gameplay command.

## Exact source map

Paths are relative to the repository root.

| Native implementation | Authoritative TypeScript |
| --- | --- |
| `commands.cpp:execute`, dispatch prelude | `game/src/core/game.ts:Game.commandDrunkIntercept`, `townAutoSleepTurn`; semantic order in `game/src/main.ts` immediately before the normal command dispatch |
| `Runner::move` | `Game.move`, `moveEcho`, `npcAtTarget`, `targetCoord`; `world/loops/turn.ts:drunkConfusionRoll`, `DRUNK_STAGGER_DIRS` |
| `Runner::turn` | `Game.runContextTurn`, ordinary town and outdoor branches |
| `Runner::world` | `Game.outdoorWorldTurn` through the `!combatResources` return; `outdoorWorldTurnRuns` and `rollSpawnGate` remain shared primitives |
| `Runner::actors` | `Game.tickGuards`, `tickNpcs`, `npcEngineSecondTurn`; existing `tick_guards` and `tick_npcs` do the mutation |
| `Runner::conscious` | `world/blackthorn.ts:partyConsciousState`, valid party-size domain 0..6 |
| `Runner::enter`, `klimb` | `Game.enter`, `klimbTown`, `klimbLadder`, `klimbCancel` |
| Transition bridge | Existing `load_small_map`, `confirm_town_exit`, `apply_stair_step`, `klimb_ladder`; see [TRAVEL.md](TRAVEL.md) |
| Fresh actor entry | `Game.loadSmallMap` -> `NpcManager.enterMap`; calls existing `enter_npc_map` inside core using borrowed `NpcLocationData` |
| `Runner::turn_events` / `GameEvent` | `Game.runContextTurn` poison-tick, message and hazard-event aggregation; `GameEvent` and `sfxEvent` payloads |

The dispatch prelude applies drunken interception to new non-movement commands,
then the sleeping-party guard. Movement performs its own drunken pre-roll after
that guard. Exit answers, Klimb direction continuations and cancellation bypass
new-command interception. No browser modal, key filtering, prompt rendering or
command echo from the HUD was copied. Pass still has no Game message echo.

## State, results and event order

`CommandState` holds town/outdoor cadence phases, the town phase owner, and the
outstanding exit response. `TravelState` retains the one-shot drunken pre-roll;
entry clears it as TS does. State and RNG are explicit caller-owned values.

`ActionResult.status` is native adapter metadata: TS methods return arrays or
null, not a success boolean. Tests derive Success/Rejected/NoOp/AwaitingResponse
from the executed reference branch, state and events. InvalidContext,
Unsupported and CoreError have separate native-only checks.

- Success does not imply time cost. Enter and accepted exit can succeed at zero
  minutes. Time-stop can leave the clock unchanged while housekeeping advances.
- Rejected town movement consumes a turn; rejected outdoor terrain movement
  rolls wind without advancing clock, housekeeping or the final world tail.
- NoOp may include a message (e.g. enterable tile without a matching map).
  Dispatch interception can already have consumed RNG before a no-op handler.
- `turns` is the actual `turns_since_start` delta. `world_turns` counts executed
  gated outdoor world tails or town guard/NPC tails, not wind rolls. State clock
  values remain authoritative for elapsed game minutes.
- CoreError is explicit; do not assume arbitrary downstream actor errors roll
  back earlier mutations. Valid map/population buffers are required throughout
  a synchronous command. Adapter preflight rejections are atomic and tested.

Event kinds preserve the first five legacy enum values. The supported payloads
are message/walk-echo text, SFX identifier metadata, poison-tick roster slots,
and `needs-direction` command name. Moved, map-changed, party-changed,
town-exit-prompt and quake have no additional payload in this projection.
The six poison slots are bounded by the party limit. Sound metadata is retained
to preserve the observable TS event stream; no audio implementation is added.

`EventSink` streams synchronously, avoiding queue overflow or silent truncation.
The event reference and text pointer are borrowed only during delivery. A
consumer retaining events must copy payloads into its own storage. It must not
re-enter the engine or mutate state during delivery. `event_count` includes all
produced events, even with no sink bound.

Examples of tested ordering:

- Movement: walk-echo -> optional slow message -> moved -> move-step cue ->
  stair message/map-changed -> turn events -> on-step extensions.
- Cactus: walk-echo -> Blocked! -> OUCH! -> party-changed -> wind-only tail.
- Ladder: floor mutation -> hour-tile refresh -> context turn/events ->
  Klimb-Up!/Down! -> map-changed.
- Poison-tick precedes housekeeping messages; earthquake message, quake and
  quake cue precede poison-tick. NPC/guard draws occur at their original turn
  hook before returned town-turn messages are emitted.

## Deliberate boundaries and extension services

This is a Game projection with **no combat resources or outdoor actor population**.
Movement is foot/transport tile 28. Mounted and naval movement are not routed
through the foot resolver. Pass/other supported handlers reuse the existing
turn cadence machinery; full vehicle command handling remains deferred.

Bridge-step ambushes, town trapdoors (including a ladder destination), shrine or
Codex Enter, dungeon entry, outdoor grapple, combat/dungeon contexts and malformed
command values return Unsupported/InvalidContext before events or RNG. Movement
preflight conservatively checks possible drunken targets and all loaded floors
at the target coordinate; a trapdoor on another floor can therefore reject a
move outside this supported profile. This is an explicit domain restriction,
not TS blocked-movement behavior.

`WorldData` supplies effective map bytes. Full dynamic object/door/quest layers
are not implemented here. `CommandServices` preserves their synchronous seams:

- door timers and hour-tile refresh;
- refuge, capture and guard-tribute checks;
- waterfalls, shrine guardian, moongate and shrine-entry checks;
- non-NPC reload effects: enemy clearing, door/terrain reset, object hydration,
  urban effects, and discarded interior objects;
- an already localized optional location banner.

These callbacks belong to future **core subsystem owners**, never renderer or
input code. Null callbacks mean the subsystem is absent, not that its gameplay
effects have been translated. Capture/tribute return true to stop their tail;
their event/state internals and continuation workflows remain untranslated.
Both effect and reload callbacks receive the ordered event sink. `EnterNpcs`
is a reload notification only: the command core performs fresh NPC entry itself
immediately after that notification; callbacks must not repeat that mutation.
Fixtures instrument the no-effect seams and compare invocation order, position
and RNG at each call. They do not claim parity for those subsystem internals.
Fresh NPC entry, guard updates and scheduled walking are real native mutations,
not callbacks implemented by the fixture. Missing NPC location data means an
empty fresh population. Providers must supply all intended location records.

## Parity and validation

`tools/generate-command-fixtures.ts` executes actual Game prototype methods,
the actual TS turn code and an actual `NpcManager`. Only excluded service
boundaries are instrumented. It produces 36 scenarios x 32 seeds x 12 actions
= **13,824 new cases**, **56,533 total** including preceding corpora.

Each replay checks state before and after every action, location/floor/x/y,
clock/calendar, survival/party fields, turn count, cadence and pending flags,
all NPC records and route bytes, event types/order/payloads, extension-call
order and coordinates, and every shared RNG draw plus final seed. Native state
persists between rows; it is not reset to the expected state after each action.

`commands-coverage.json` records branch witnesses, including all eight commands,
all ten event kinds, both exit responses, zero/costly turns, slow/very-slow
terrain, cactus, burning, earthquake, poison, drunken replacement, sleeping
dispatch, scheduling, and absent-map/invalid-target paths. Required rare branch
witnesses are asserted by the generator. Thirteen additional adapter checks
cover invalid contexts, unavailable movement, bridge/trapdoor/shrine boundaries,
missing maps and pending-response locking without mutations/events/RNG.

See [VALIDATION.md](VALIDATION.md) for final check counts and the unavailable
asset-dependent TypeScript census suite. No TS runtime behavior was changed.

## Memory

| Type | Host bytes | ESP32-S3 bytes | Change |
| --- | ---: | ---: | --- |
| `GameState` | 576 | 576 | 0 |
| `TurnState` | 72 | 72 | 0 |
| `TravelState` | 3 | 3 | 0 |
| `NpcActor` | 66 | 66 | 0 |
| `Command` | 3 | 3 | New; existing one-byte MoveAction unchanged |
| `CommandState` | 20 | 20 | New, caller-owned |
| `ActionResult` | 24 | 24 | New |
| `GameEvent` | 24 | 16 | +22 / +14 from the two-byte scaffold |
| Legacy slice `CommandResult` | 18 | 18 | 0; retains two-byte MovementSliceEvent |
| `CommandContext` | 184 | 92 | New borrowed bindings |

No heap allocation or hidden global mutable state is introduced in production.
The NPC raster remains caller-owned (1,056 bytes). General A* remains separate:
its **323,084-byte** scratch must stay external and should be placed in PSRAM
at future device integration, never internal RAM or the `app_main` stack.
This command layer never allocates or invokes that A* scratch. Neither the new
CommandState nor a full map/population is instantiated by the device loop.

## Remaining Game methods and command categories

Untranslated methods include all of the following families, beyond the narrow
shared orchestration listed above:

- `navalMove`, `resolveNavalStep`, `runNavalTurn`, transport facing/boarding,
  `board`, `exitVehicle`, `yellSails`, waterfalls and vehicle relocation effects.
- `resolveTrollToll`, troll scripts/placement, full `outdoorWorldTurn` enemy
  lifecycle, `startCombat`, `attack`, `fire`, `fireCannon`, `endCombat`.
- `talkTarget`, all guard/merchant/possessed dialogue, dialogue continuations,
  `submitDonation`, capture/interrogation/refuge workflows and their scripts.
- `open`, `search`, `get`, `jimmy`, `push`, `look`, chest/door/object/inventory
  interactions; `readyItem`, `newOrder`, active-player commands and roster UI.
- `klimbGrapple`; all camp/bed/inn workflows (`camp`, `campSleepStep`,
  `campWake`, `bedSleep*`, `innSleepUntilMorning`). AutoSleep is only the town
  sleeping-party loop, not camp/rest implementation.
- `ignite`, `view`, harpsichord actions, `mixReagents`, casting and every
  `apply*Spell`/tool/use method; shops, purchases and quest actions.
- `enterDungeon`, `dungeonCommand`, dungeon movement/combat/light, shrine
  ceremonies, physical moongate activation, trapdoor falls and Stonegate wipe.
- Full reload object/door/urban-effect owners, complete state initialization,
  save/load and route persistence codecs, audio, rendering and all platform UI.

Before integrating larger subsystems, supply their state/event payloads and
continuations, effective-map and world-object ownership, encounter resources,
and complete persisted state. The current service seams and semantic results
make those dependencies explicit. No larger subsystem translation is started.
