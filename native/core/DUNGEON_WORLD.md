# Dungeon and world orchestration

The reference is the checked-out TypeScript implementation. No TypeScript runtime
or extractor behavior was changed. This batch adds **381,952 checked snapshots /
sequences**, bringing the running total to **991,325**. This is a portable core
library batch; the device application still runs its existing movement slice.

## Source map

| Native entry points | Authoritative TypeScript |
| --- | --- |
| `DungeonState`, `dungeon_cell`, `dungeon_action` | `dungeon/dungeon.ts`: `DungeonState.cellAt`, `forward`, `back`, `step`, `turnLeft/Right/Around`, `klimbCaps`, `parejaDeEscaleraBajoSala`, `klimb`, `magicChangeLevel`, `onEnterCell`, `pitFall`, `contestDex`, `wakeSleepers`, `turnTick` |
| `dungeon_respawn`, wanderer tick | `dungeon/wanderer.ts`: `respawnWanderer`, `spawnWandererPos`, `moveWanderer`, `ambushDirection`; `DungeonState.worldAdvances` |
| `dungeon_room_map`, cleared bitmap and room marking | `dungeon.ts`: `roomCombatMapIndex`, `dungeonClearedBitIndex`, `dungeonMarkRoomCleared`, `applyClearedRooms`, `markRoomClearedAt` |
| `dungeon_load`, `execute_dungeon_command`, `exit_dungeon` | `dungeon/dungeon-cmds.ts`: `enterDungeon`, `dungeonCommand`, `dungeonMagicChangeLevel`, `dungeonSpellTurn`, `translateDungeonEvents`, `exitDungeonTo` |
| `build_corridor_map` | `DungeonState.buildCorridorArena`; `wanderer.ts`: `corridorTilesFor`, `buildCorridorCombatMap`, start/monster coordinate tables |
| `dungeon_room_entry` | `combat/roomEntry.ts`: `roomEntryDirectionFor`, including fallback order |
| `initialize_combat(..., FixedCombatSetup)` | `Combat.placeEnemies` fixed branch, `rollEcGroupPool`, `seedArenaObject`, `makeEnemy` |
| `start_fixed_combat` | `Game.startDungeonRoomCombat`, `startDungeonCorridorCombat`, `ringExpiryEvents`; generic caller-approved scripted entry |
| `dungeon_combat_return`, `finish_encounter_combat` | `Game.endCombat`: roster/RNG sync, corridor cause, escape border, floor delta, exit, respawn, refuge ordering |
| `board_transport`, `disembark_transport`, semantic Board/Disembark | `world/transport.ts`: `board`, `exitTransport`, `transportMode`; `Game.board`, `exitVehicle`, `orthogonalLandNearby` |
| `chest_trap`, `chest_loot`, `dungeon_chest_loot`, `apply_loot_grant` | `world/commands.ts`: `chestTrap`, `applyTrapEffect`, `chestLoot`, `dungeonChestLoot`, `applyLootGrant`, `lootItemName`, `lootOpenLine` |
| dungeon Open/Get/Jimmy/Drink/Search | corresponding `DungeonState` methods, including light gate, directional search, field/trap/secret-door mutations |
| combat Get/Open/Klimb | `Combat.playerGet`, `playerOpen`, `resolveBoardGet/Open`, `openChestIntoPile`, `resyncPartyHpFromRoster`, `playerKlimbEscape` |

## State and ordering contracts

- Dungeon IDs are 33–40. Dungeon floors are 0–7; the large-map Underworld floor
  remains 255. `GameState.position` keeps the exterior return context while the
  caller-owned `DungeonState` holds dungeon coordinates and N/E/S/W facing.
  `DungeonFacing` deliberately differs from the existing `Direction` enum order.
- Entry scans floor zero in row order for an up/up-down ladder, defaulting to
  (1,1), facing south. Underworld entry uses floor 7, (7,7), west; Doom always
  uses the top-floor path. No entry turn is charged.
- Movement wraps each axis with `&7`. Cell lookup itself does not wrap. Walls,
  unrevealed secret doors, wanderers, doorway turns and the exact unlit energy
  field byte retain their separate gates. Level changes retain X/Y/facing.
- Dungeon commands advance the existing clock/housekeeping by **one minute**
  before the command. Attack is the no-clock exception. Successful movement
  performs sleeper recovery and wanderer processing before destination effects.
  Failed movement and non-movement commands receive the separate tick tail.
- Dungeon events are collected in caller-owned scratch and translated only after
  the rule sequence finishes, matching TS array construction. This is essential
  when a room/corridor event is followed by more dungeon RNG draws. Damage and
  movement markers have the same projection as TS; logical SFX IDs are events,
  with no audio implementation. `GameEvent.dungeon_id` carries entry identity.
- Room-map indexing and persistent cleared-bit indexing are intentionally
  different around Despise. The six room-clear exemptions are preserved.
  Victory marks the recorded entry cell, including silent initial victory.
- Fixed setup draws the four EC-group choices before scanning map units. It does
  not shuffle fixed unit positions. Fields, chests, piles and enemy definitions
  use the existing combat state and RNG. Room entry uses the opposite facing,
  with the reference's south/north/east/west fallback for degenerate starts.
- Combat RNG is forked at initialization and copied back at finish. Room ring
  expiry uses the world stream after initialization, matching the reference.
  Corridor maps consume the world stream before the combat fork.
- Get removes one pile item in LIFO order. Open generates ground loot without
  granting it. Chest traps resynchronize actor HP/death from the existing roster.
  Torch borrowing and directional food-plate interactions are also translated.
- Boarding loads parked hull/skiffs before the success gate. Disembarking keeps
  coordinates, parks the ship with its remaining skiffs, and switches the existing
  transport fields. Successful actions run the existing context-turn machinery;
  failed actions do not. All object tiles retain the high-bank convention.

## Integration and ownership

Attach `DungeonContext` to `CommandContext`; supply the extracted `DungeonData`
catalog and session/scratch storage. `EnterDungeon` uses `member` as location and
`hours` as the source floor. `DungeonCommand.item` is `DungeonAction`; `hours`
selects climb direction (-1/up, 1/down, 2/cancel) or search target
(0/ahead, 1/here, 2/left, 3/right). Search `member=-1` uses the active member.
The semantic API has no hardware keys.

World `Enter` routes dungeon locations to that same context. Doom's
quest-specific entrance condition requires `entry_hook`; absent that hook, world
Enter reports Unsupported. Direct `EnterDungeon` corresponds to the reference's
already-approved loader. The room/corridor callbacks select their exact resource
and call `start_fixed_combat`; no quest predicate is invented by the core.
The command context passed to `start_fixed_combat` must remain alive throughout
the battle because the victory callback references its dungeon session.

`TransportServices` borrows the existing world owner's effective tile/object
operations. It does not create another object pool. Reserve must guarantee the
following park/drop operation; failure produces NeedsStorage before those
mutations. The owner also supplies local horse ownership and ship metadata.

Combat field and pile overflow arrays are caller-owned. Fixed input has at most
16 unit records. Chest growth can exceed the inline 16 piles; Open preflights a
conservative `9 + (largest contents >> 1)` additional slots before actor scanning
or RNG. Capacity failure is explicit and retryable, not a gameplay cap.

## Parity and data

| Suite | Checked cases |
| --- | ---: |
| Dungeon rules, objects, search, state and RNG | 294,912 |
| Fixed combat setup, climb/escape, opening and repeated Get | 53,248 |
| Board/disembark rule sequences | 16,384 |
| Actual Game.board/exitVehicle with context-turn and object mutations | 6,144 |
| Actual TS dungeon orchestration, corridor construction and `Game.endCombat` routing | 11,264 |
| **New total** | **381,952** |

Dungeon rules cover all 4,096 cells in the eight real dungeons, four facing/status
configurations, action sequences, cell mutations, revealed doors, the cleared
bitmap, party/inventory and RNG state. Fixed combat covers all 128 original
arenas, four facings, and eight floor values, including empty-enemy arenas.
Orchestration fixtures compare event order, effect calls, return coordinates,
clock/turn progression, corridor map/unit placement and RNG continuation.
Board rule fixtures compare both results and inventory/ship state. The additional
transport-flow corpus calls actual Game boarding/disembark and outdoor-turn
methods, comparing repeated cycles, object records, events, RNG and time.
12 native
adapter checks additionally cover semantic boarding, storage rejection,
underworld entry, silent victory marking and field-storage rejection.

Snapshots use deterministic FNV-1a hashes of canonical integer/text streams; any
hash mismatch fails the suite. They are not a proof of all reachable gameplay
states. Object persistence/serialization remains outside these in-memory replays.

The generators assert extracted dungeons against `parseDungeons(DUNGEON.DAT)`
and arenas against `parseCombatMaps(BRIT.CBT, DUNGEON.CBT)`. This preserves the
extractor's 0x08-to-ladder interpretation. Original data is never fabricated.
Raw test map/enemy/location transports and generated loot-name tables are
gitignored and regenerated from the existing assets. No new asset-dependent
parity suite was skipped.

## Remaining boundaries

- Dialogue, shops, quest predicates/rewards, Doom rescue/entrance narrative,
  absorption endgame, shrine narratives and persistent quest progression remain
  deferred. Entry/rescue/refuge/hydration callbacks retain their ordered handoffs.
- The existing semantic `Move` path still rejects non-foot transports. This batch
  implements boarding/disembarking and transport state/context switching, **not
  complete mounted/naval travel**. That earlier movement limitation must be
  resolved before claiming complete transport exploration.
- Dungeon Look/presentation, light rendering, spell-specific dungeon field
  creation/dispel/use-item bridges, save serialization and device integration
  are not added here. Ascend/descend orchestration is present. World object owner
  persistence and quest-specific object interpretation remain with their owners.
- Generic special encounters require caller-approved triggers and exact assets;
  no quest narrative condition is inferred. World defeat continues through the
  existing refuge service seam; this batch does not implement Lord British's
  narrative recovery sequence.
- A synthetic floor-zero dungeon chest can request `rand(1,0)` in the TS
  reference. Native reports InvalidRange after the same preceding mutations;
  the factory maps contain no floor-zero chests. This is not silently replaced
  with invented loot.

See [VALIDATION.md](VALIDATION.md) for compiler/build evidence, sizes, stack and
the unchanged UI/e2e test failures. These boundaries prevent describing the
entire world/transport/quest game loop as complete.
