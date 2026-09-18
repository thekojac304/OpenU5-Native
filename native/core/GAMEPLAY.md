# Platform-independent gameplay integration

This batch completes the four gameplay blockers recorded after the quest batch:
non-foot semantic Move, bridge tiles 106/107, outdoor encounter ownership, and
generic world-command orchestration. Native/core is ready to begin the first
playable-alpha UI/device/FATFS integration with the owner/resource contract below.
This is semantic reachability, not an uninterrupted campaign or on-device test.
No device, UI, rendering, audio, hardware input or FATFS implementation was changed.

## Reference and native implementation map

Paths in the reference column are relative to `game/src/core/`.

| Integration | Authoritative reference | Native implementation |
| --- | --- | --- |
| Foot, horse, carpet, ship and skiff Move; facing, wrapping, collision, wind, sails, HMS cadence, hull/sinking, waterfalls | `game.ts` movement/world-turn methods; `world/transport.ts` | `commands.cpp`, `movement.cpp`, `transport.cpp` |
| Board, disembark, parked transport ownership | `game.ts`, `world/transport.ts` | `world_transport_services` in `world_terrain.cpp`, existing transport rules |
| Bridge 106/107 sneak/toll/refusal | `game.ts` bridge/troll paths | command runner, `TrollToll`, existing encounter engine |
| Spawn selection, chunk cleanup, enemy turns, pirate/whirlpool behavior and combat return | `world/enemies.ts`, `game.ts` encounter methods | `outdoor.cpp`, generated `outdoor_tiles.inc`, existing `combat.cpp` |
| Open/Jimmy/Push, ordinary Get, Attack and Fire | `game.ts` corresponding methods, shared chest/loot/actor helpers | `world_commands.cpp`, `quest_search.cpp`, `scripted_encounter.cpp` |
| Look, crystal ball, wishes, spyglass | `game.ts` corresponding methods, LOOK2/sign assets | `look.cpp`, `LookServices`, semantic events |
| World/dungeon Cast, Mix, potions/scrolls/tools | `game.ts`, `magic/cast.ts`, `magic/spells.ts`, `usePotion.ts`, `useScroll.ts` | `world_magic.cpp`, existing magic/inventory/quest rules; `combat_use_consumable` |
| Rest and ambush acceptance, including dungeon camping | `world/camp.ts`, `Game.camp` | existing `rest.cpp`, command-to-combat handoff |
| Gem view/continuation, Ignite, NewOrder, active member | `game.ts`, `world/commands.ts` | semantic command dispatch |
| Night arches/bridges, persistent/transient terrain and reload | `game.ts` active-map/override/hour refresh methods | `WorldTerrain`, command/dungeon reload hooks |
| Dungeon room/corridor scene entry and return | `game.ts`, dungeon/combat modules | `DungeonEncounters`, existing fixed/corridor/combat engines |
| Live-owner save/load adapters | reference save-document fields and buffer APIs | `gameplay_save.cpp`, existing `save_core.cpp`/persistence APIs |

Quest conditions and rewards remain in the completed quest implementation.
Search, quest Get/Use, shrine/Blackthorn interactions, moonstones, Doom, Refuge,
and endgame are reached through those existing hooks; no second quest engine
or combat engine was introduced.

Bridge tiles were formerly rejected because movement could trigger a deferred
sneak/toll/encounter sequence. The native command path now executes the reference
transport gates, 32-beat sneak script, toll prompt, payment/refusal, turn tail and
existing troll encounter. Pending toll resolution gates subsequent commands.

## Reachability audit

| Major loop operation | Semantic route and evidence |
| --- | --- |
| Overworld movement and normal transports | Move, YellSails, Board/Disembark, Klimb; exhaustive movement corpus plus boarding/movement/disembarking sequences |
| Enter/leave towns and local floors | Existing Enter/Exit/Klimb and transition corpus; real-map night terrain refresh and reload integration |
| Dungeon exploration | EnterDungeon/DungeonCommand, dungeon Cast/Get/Open/Jimmy/Search, room/corridor entry, combat return, camp, Ignite and gem continuation |
| Objects and inventory | Open/Jimmy/Push/Get/Search/UseItem, existing Ready/Unready; doors, traps, chest loot, food/torch/carpet, prisoner and transport owners |
| Talk and shop | Existing dialogue and shop semantic sessions; regression includes real NPC and all 46 shop catalog coverage |
| Rest | Outdoor camp, town beds/inn services, ship repair and dungeon camp; camp-to-combat-to-return comparisons |
| Fight and cast | Outdoor/town/bridge/camp/dungeon triggers enter the same combat engine; physical/magic actions, consumables and victory/flee/defeat/Refuge returns |
| Quest progression and endgame | Existing completed quest corpus, including Doom/absorption, GameWon/Endgame and Refuge; not reimplemented |
| Save/load | Capture core, gameplay and terrain owners into retained document, existing byte-buffer save/load, restore owners; every gameplay sequence round-trips its last state |

Representative subsystem sequences include boarding -> travel -> disembarking ->
boarding; door/chest interaction -> movement -> turn-driven door closure; world
Attack -> combat Use -> combat completion -> return; camp -> ambush -> combat ->
return; dungeon forward/attack -> real arena -> escape -> dungeon continuation;
and defeated combat -> Refuge. Persistence checks accompany these sequences.

No remaining platform-independent blocker was found for this major gameplay
loop within the semantic contracts. This does not claim every optional UI action
in every context: e.g. combat equipment changes remain available as inventory
primitives rather than a new combat Ready command, and visual shrine/Blackthorn
timelines remain outside the existing no-scene quest contract. The alpha adapter
must bind supported semantic actions and required resources, not infer missing
rules from rendering callbacks.

## Ownership and adapter contract

* `CommandContext` borrows existing game, turn, travel, dialogue, shop, quest,
  dungeon, NPC and combat owners. `OutdoorServices` is the single live outdoor
  enemy owner. `QuestWorldServices` remains the single world-object owner.
* Bind `OutdoorServices.combat/resources`, quest storage callbacks and arena/enemy
  tables for automatic encounters. Keep these owners alive through combat return.
  Pirate prize capacity is reserved at entry and committed to the same object
  owner after victory. Missing resources produce explicit context/storage errors;
  the legacy unbound camp path still reports a pending encounter to its caller.
* Bind `WorldTerrain` and the existing quest raw/volatile/persistent storage
  callbacks to it. Use `world_transport_services(context)` for transport storage.
  Terrain rules live in core; the callbacks expose storage rather than gameplay.
  Hourly and transient layers clear on residence reload; persistent overrides
  survive buffer save/load. Refresh the hour layer after restoring a save.
* Bind caller-owned `DungeonEncounters` and all required real arenas to the
  dungeon context for automatic room/corridor entry; legacy callbacks remain
  available. Fixed rooms and corridors reuse the existing combat implementation.
* `LookServices` borrows LOOK2 descriptions and raw sign bytes. Events carry
  prompt requests, zodiac data, map/gem view requests and projectiles; no renderer
  or UI prompt loop was added. The UI supplies resolved semantic responses.
* Before saving, use `capture_core`, `capture_gameplay` and `capture_terrain` with
  the existing retained world-object/quest document. Load buffers with existing
  persistence APIs and restore those owners together. Outdoor/door and terrain
  restore routines validate their domains before committing their own owners.
  This is not a new filesystem or a duplicate live world.
* Live combat and conversation/shop UI sessions retain the existing persistence
  exclusions. A buffer round trip during a harness combat compares the supported
  save projection; it does not promise resumable mid-combat saves.

## Validation and counting

Final regression: **49/49 CTest checks passed**, zero failures, 204.61 seconds.
Fixture/harness TypeScript type checking and ESP-IDF build/package checks passed.

* **36,792** independent four-action movement sequences: 147,168 snapshots,
  all 256 terrain IDs, 18 transport/facing values and eight variants.
* **5,058** actual TypeScript Game versus native gameplay sequences. State,
  coordinates/maps/floors, transport, inventories, characters, NPCs/objects,
  enemies, combatants, terrain, semantic events, time and RNG are compared.
* **41,850 new** parity sequences; **2,076,515 cumulative**, from 2,034,665.
  Internal steps are not counted again. The inherited cumulative count includes
  two bounded reference-nontermination observations documented in QUESTS.md.
* **21 native owner/domain assertions** are separate from the parity count.
* Gameplay witnesses: 473 combat starts, 449 completed returns (18 victories,
  415 flees, 16 defeats), 16 Refuge transitions, 27 toll prompts. The 24 remaining
  active scenes are explicit unfinished/ship-escape cases; the harness does not
  force completion or count them as successful returns.
* Real sources include overworld data, all 16 surface arenas, all 112 dungeon
  room arenas, 48 corridor sequences, 184 town-hour sequences, all 79 real signs,
  LOOK2, enemy and item/spell tables. Directed town NPC fixtures derive from a
  real record; the prior dialogue/quest suites cover full real populations.
* Outdoor/door state is JSON-buffer round-tripped after every action; full
  byte-buffer save/load plus terrain restoration is checked at each final step.
* No required new asset skips and no waived mismatches. Historical exceptions:
  three long dry-foot shrine routes (all eight shrine rules remain covered),
  optional font-atlas comparison and optional user seed-save extractor tests.

## ESP32-S3 sizes and allocation

Firmware build, size report and Launcher packaging pass without flashing.
The application is **342,288 bytes**, **delta 0** from this batch's baseline;
linked unpadded image 342,168 bytes, Launcher allocation 393,216 bytes. The
existing device slice strips unused core code: this is not the footprint of a
fully wired playable alpha.

| Type | Xtensa bytes | Delta from pre-batch measurement |
| --- | ---: | ---: |
| GameState | 2,304 | 0 |
| CombatState | 3,604 | 0 |
| Command | 32 | 0 |
| CommandContext | 140 | +12 |
| GameEvent | 84 | +28 |
| QuestObject | 68 | +16 |
| QuestWorldServices | 116 | +8 |
| TurnState | 80 | +8 for sail direction/HMS cadence |
| CommandState | 52 | Prior size not separately measured |
| DungeonContext | 52 | Prior size not separately measured |
| OutdoorServices / OutdoorEnemy | 128 / 36 | New owner / element; vector payloads additional |
| WorldTerrain / TerrainCell | 48 / 16 | New owner / element; vector payloads additional |
| LookServices / ZodiacView | 12 / 192 | New |
| DungeonEncounters | 648 | New caller-owned arena scratch |

External A* scratch remains **323,084 bytes**, with its future PSRAM placement
contract unchanged. Combat state, dungeon scratch (2,564 bytes), encounter scratch
and variable populations remain caller/heap owned, not large task-stack locals.
Largest newly measured optimized Xtensa frame is `outdoor_tick`, **1,712 bytes**;
world interaction 1,152, world magic 1,056, command turn 800. The prior persistence
measurement includes a 2,544-byte `restore_core` frame. These are individual
compiler frames, not measured worst-case call chains or device stack high-water
marks; those require the later device integration.

## Reproduction and artifacts

Generate `generate-movement-flow-fixtures.ts` and `generate-outdoor-tables.ts`
when rebuilding fixtures; generated table drift is checked by CTest. Build the
host `build-zig` tree with Zig 0.13.0 and run CTest. `check-gameplay.ts` runs the
actual reference and native driver; never run it concurrently with its CTest
entry because both write `build-gameplay/`.

`node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json` includes
the new generators/harness. `tools/validate-gameplay-esp.ps1` performs the IDF
build/size, Launcher packaging, Xtensa size probe and optimized frame measurement.
No flashing is performed. Generated inputs/results, coverage.json, build logs,
esp-sizes.txt and largest-stack.txt live under ignored `build-gameplay/`.

This ledger supersedes older non-foot/bridge/outdoor/generic-command exclusions
in COMMANDS, ITEMS_REST, DUNGEON_WORLD, MAGIC and QUESTS only for the integrations
listed here. Other documented numeric/resource and presentation boundaries remain.
