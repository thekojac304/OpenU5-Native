# Native combat foundation

Update: [MAGIC.md](MAGIC.md) supersedes this batch's magic, special-AI, field
resolution and actor-growth exclusions. It documents caller-owned overflow,
wider actor IDs, semantic Cast and the current parity domain. Historical sizes
and totals here are superseded by [VALIDATION.md](VALIDATION.md).

Authoritative source: the current `game/src/core` TypeScript implementation.
This is the **encounter-array, physical-combat projection**, not a complete
translation of the 5,000-line `Combat` class. Hardware and TypeScript runtime
files are unchanged. Spell casting, UI, rendering and device input are excluded.

## Source mapping

All native implementation below lives in `src/combat.cpp` with public types in
`include/openu5/combat.h`; command routing is in `src/commands.cpp`.

| TypeScript reference | Native translation |
| --- | --- |
| `combat/combat.ts`: `Combatant`, `CombatWeapon`, `CombatMapData`, `CombatOpts` | `CombatActor`, `CombatWeapon`, `CombatMap`, `CombatContext`, `CombatTables` |
| `Combat.constructor`, `placePlayers`, `makeEnemy`, array branch of `placeEnemies`, `shuffle` | `initialize_combat` |
| `equip.ts`: `characterWeapons`, `characterDefense` | Initialization weapon list and `Engine::defense`; existing party/equipment fields remain authoritative |
| `isActive`, `sideOf`, `findNextActor`, `skipsForActiveChar`, `currentUnit`, `maybePossessWithChaosSword` | `active`, `party_side`, `Engine::current`, `current_combat_actor` |
| `advanceTurn`, `perTurnStatusPass`, `ringRegenCombatSweep`, terrain branch of `endOfTurnFieldDamage` | `Engine::advance`, `status_pass`, existing roster and combat HP synchronization |
| `tilePassableFor`, `occupantAt`, `cellFree`, `fireTriggers`, `playerMove`, `borderForCell` | `Engine::passable`, `occupant`, `free`, `triggers`, `move` |
| `playerAttack`, `ensureAttackQueue`, `projectileLanding`, `resolveRangedFlight`, `rangedInterference`, `attackWith` | `Engine::attack`, `queue`, `shift`, `interference`, `attack_with` |
| `defenseStat`, `attackStat`, `rollWeaponHit`, `strike`, `strikeIsMagic` | `Engine::hit`, `strike` |
| `consumeAmmo`, `removeWeapon`, `unequipWeaponFromAllMembers` | `Engine::ammo`, `remove_weapon`, existing `unequip_item_by_id` |
| `applyDamage`, `syncPlayerHp`, `killByRosterDeath`, `kill`, `dropLoot`, `woundClassify` | `Engine::damage`, `roster_dead`, `kill`, `wound` |
| `poisonAttack`, `putToSleep`, `wakeUp`, `playerDraggedTurn`, `playerSleepTurn` | `Engine::poison`, `sleep`, `wake`, `disabled` |
| `maybeLatchVictory`, `collapsePossessed`, `over`, `endEvent`, `playerEscape`, `playerEscapeQuick` | `Engine::latch`, `collapse`, `combat_over`, `end`, `escape`, `combat_action` |
| `playerPass`, `playerAttackCancel`, `playerYieldTurn` | `CombatAction::Pass`, `AttackCancel`, `Yield` |
| `Game.combatActivePlayer` (COMBAT:0x063E @0x09ec / @0x09fe -> SJOG.OVL 0x1F7A) | `CombatAction::SetActive`, reached by `CommandKind::SetActivePlayer` while `CommandContext::combat` is set |
| `tickEnemyTurnStep`, `selectTarget`, `enemyTurn`, physical `enemyAttack`, non-teleport `enemyMove` | `CombatAction::EnemyStep`, `Engine::target`, `enemy_turn`, `enemy_attack`, `enemy_move` |
| `combat/formulas.ts`: `initiativeReset`, `enemySpawnSpeed`, `rollHit`, `weaponBaseDamage`, `applyDefense`, `adjustEnemyDamage`, `xpForKill`, `woundClassify`, `chestRoll`, `combatDistance`, `randomAdjacentCell` | Inlined formulas in the corresponding routines; `combat_distance` is public |
| `combat/encounters.ts`: `combatMapForTile`, `rollEncounterGroup` | Generated map-index table and `start_encounter_combat` |
| `game.ts`: `Game.startCombat`, `ringExpiryEvents`, non-dungeon `endCombat` | `start_encounter_combat`, `finish_encounter_combat`, existing ring-expiry/unequip helpers and Refuge callback |
| `tiles.ts`: final `TILE_INFO`; `magic/areaSpellTables.ts`: `blocksSpellLine` | Generated `src/combat_tiles.inc`; LOS metadata only, no spell implementation |

## Semantics preserved

- Initiative scans slots, decrements a **byte** counter, reloads `36-speed`,
  and counts selected and automatically passed actions. A reset value of zero
  requires a full byte cycle. Sleeping actors remain active; roster-dead actors
  are silently removed during the scan. Active-member selection and ring passes
  retain their order.
- Formation indexes retain dead-member holes. Enemy slots undergo the same
  Fisher-Yates shuffle, then movement-class preference and first-unused fallback.
  Every enemy spawn consumes its speed roll in placement order.
- Physical attacks use helmet/weapon/shield order and a separate active queue.
  They preserve automatic-hit weapons, strength selectors, sleeping/mimic defense,
  base damage, armor rolls, negative grazes, undead reduction, immortality,
  glass-sword removal, and **99 as lethal damage**, even above 99 HP.
- Ranged flight and enemy LOS use different reference property tables. Polearms
  bypass flight. Miss scatter consumes pairs of RNG draws until a valid cell is
  selected; friendly fire is possible. No extra hit roll is inserted on scatter.
- Ammunition decrements before hit rolls. Zero ammo wraps to 255. Depleting ammo
  removes one matching weapon from each party member and returns those weapons
  using byte arithmetic. Thrown weapons use pack spares, then remove the equipped
  weapon. The active attack queue is not rebuilt mid-turn.
- Death writes corpse/loot state, awards capped roster XP, rolls treasure in
  order, and detects victory at the reference call sites. Gargoyles become rock;
  no-corpse/disappearing enemies and water/floor exceptions skip loot rolls.
- Victory latches once and does **not** end the arena. Walking out, quick escape,
  same-exit room restrictions and ship restrictions follow the reference.
  Player escape itself has no success/failure RNG roll. Enemy retreat does.
- Supported generic enemy turns include target tie order, physical melee/ranged
  attacks, movement, fleeing/healing, sleep/wake, food theft, poison, gazer sleep
  and Corpser effects. Existing time-spell flags are honored; casting spells is
  not implemented. Chaos Sword possession and collapse are physical-weapon effects.
- Terrain damage/poison and one-shot room triggers are included. Arena object
  fields and quest absorption are outside the admitted map domain below.

## Ownership, commands and events

`CombatContext` references the existing `GameState`, `TurnState` and a caller-owned
`CombatState`. Combat HP/stat caches match the TypeScript actor records; inventory,
equipment, roster status, XP and persistent HP use the existing party model.
Enemy definitions and table arrays must remain alive and immutable throughout
combat. The map is copied into the active state, including mutable triggers.

The caller binds its persistent `CombatContext` through
`CommandContext::combat_context` and sets `combat=true` (the start handoff sets
this boolean). Supported semantic commands are `CombatMove`, `CombatAttack`,
`CombatPass`, `CombatEscape`, `CombatEscapeQuick`, `CombatAttackCancel`,
`CombatYield` and `CombatEnemyStep`. Move/escape use the `CombatDirection` ordinal
in `combat_x`; Attack uses arena coordinates in `combat_x/combat_y`.

`SetActivePlayer` is supported in BOTH loops and is the one command whose meaning
changes with `combat`: outside an arena it is the kernel's set-active (0x4080),
inside one it routes to `CombatAction::SetActive`, which is COMBAT.OVL's own --
different echo string, arena-based validity, and an action cost: a successful
selection or a `0` clear cedes the current actor's turn, a rejected one does not.
The digit travels in `Command::member`, identically in both.
These commands never call world turns or hardware input. `Success` means the
reference method was dispatched: gameplay refusals such as `Blocked!` are events,
because the TS methods return event arrays, not a success boolean.

The TS engine returns `CombatEvent[]`, distinct from `GameEvent[]` returned by
`Game.startCombat/endCombat`. Native `CombatEvent` preserves optional fields
using -1 for absent numeric/boolean payloads and nullptr for absent text.
The semantic dispatcher delivers these through a native `GameEvent::Combat`
envelope; its pointed-to payload preserves the original event kind and fields.
Start/end events use `GameEvent::CombatStarted/CombatEnded` directly.

Delivery is synchronous and streaming, with no heap or unbounded event queue.
Callbacks must copy needed text/payloads, must not reenter or mutate the engine,
and must inspect the final state after the action returns. This is a native
delivery adapter, not the TS array lifetime contract. Event order is unchanged,
including duplicate `ended` emissions when the reference kill/advance paths
both emit one. No audio implementation is added; existing ring-vanish SFX tags
remain observable events.

`start_encounter_combat` accepts supplied maps/definitions, selects the map with
fallback to map 0, rolls the group using the world stream, copies its seed into
the arena, initializes it, and then runs ring expiry on the **world** stream.
This deliberately follows the actual TS execution order, including its separate
streams. `finish_encounter_combat` synchronizes HP and copies the arena seed back.
Pending camp enemy handoffs are cleared only after successful initialization.
Door reset and world-enemy removal are caller callbacks; Refuge uses the existing
command effect seam. Post-group custom prose, pirate prize ships, dungeon exits,
quest aftermath and actor-attack arena selection are not implemented here.

## Admitted domain and explicit deferrals

- At most six party members, sixteen initial arena slots, eight triggers and
  twenty-two actors. Those initial counts come from the existing `.CBT` parser
  layout. The TS vector can grow later: this capacity is **not** claimed to cover
  summons or splitting. Such enemy definitions are rejected before initialization.
- `CombatMap` models the encounter-array path. Fixed-from-map dungeon sprite
  decoding, random EC groups, seeded chests/items/fields and their inventory are
  deferred. No `.CBT` decoder or substitute real arena is fabricated.
- Enemies with possession, division, ranged magic, teleportation, invisibility
  AI or daemon-gating abilities return `Unsupported`; their behavior is not
  silently replaced with ordinary physical AI. Encounter handoff also checks a
  possible companion definition before consuming RNG.
- Quest soul-absorption arenas, boss scripts, magic, summons, field spells,
  chest/get/open/search/push/klimb actions, live Ready/potion synchronization,
  directional aim convenience, multi-enemy batching and presentation remain
  deferred. The caller must not supply quest/object-field arenas to this API.
- Numeric inputs are finite integers in native game domains: party stats/equipment
  IDs are bytes, HP/XP are words, enemy stats and table values are nonnegative
  game-data values. Combat damage and cached stats use signed 32-bit values;
  initiative speed/counters alone wrap as bytes. Action counts must fit uint32.
  Attack targets are board cells; malformed out-of-board requests return `Invalid`
  before changing state. Map unit coordinates are board cells; start arrays use
  the reference last-entry/(0,0) fallback. Enemy names are nonnull ASCII strings
  up to 120 bytes, group names up to 160, roster names use the existing eight-byte
  domain. English event strings are compared; localization services are deferred.
- Initialization requires a separate input map, not an alias of the destination
  map. Callers retain responsibility for supplying validated table lengths and
  definitions. No serialization or raw-layout persistence is introduced.

## Differential validation

`tools/generate-combat-fixtures.ts` executes the **unchanged** `Combat` and actual
`Game.startCombat/endCombat` methods. Only excluded world services are stubbed in
the latter. The corpus contains 2,496 deterministic scenarios (78 configurations
times 32 seeds), each with initialization, 18 actor-selection/action pairs and
world start/end checks: **97,344 snapshots**. Native actions go through the
semantic dispatcher, not only direct helpers. Seven adapter rejection checks
are additional and are not counted as TS parity cases.

Snapshots compare actor order/positions/HP/status/stats, initiative/current actor,
weapon queues, equipped slots, all 256 pack counts, XP, mutable terrain, loot,
chest contents, victory/end state, every event field/text in order, RNG seeds and
every RNG draw including constructor/group/expiry draws. Scenario coverage
includes miss, graze, lethal/nonlethal, 99 damage, armor, ammo zero/one, empty
targets, blocked tiles/occupants, mixed water/land placement, dead party holes,
sleep, zero-HP living entrants, counter wrap, maximum initial capacity, last-enemy
death, friendly fire, repeated turns and exits. Required branch coverage is
asserted; counts live in `fixtures/combat-coverage.json`.

The text format compresses runs and unchanged portions of the preceding row;
the test expands these to full integer snapshots and fails at the first unequal
field. Drift checks regenerate expectations in memory and compare exactly.
No mismatch is waived and no TypeScript runtime behavior was edited.

Run `node --import tsx native/core/tools/generate-combat-fixtures.ts --check`,
build/CTest as in README, and `native/core/tools/test-combat-reference.ps1` from
the repository root. The latter explicitly skips fourteen selected real-arena
game suites and the combat-map extractor suite when assets are absent. Those
assets are now restored and **all fifteen suites pass**. The additional real-map
mode validates **159,744 snapshots** across all 128 original arenas and all four
entry directions. Total parity is **352,349**. Extraction, strict CTest wiring,
the corrected native exit-border mismatch and results are in
[REAL_ARENAS.md](REAL_ARENAS.md).

Required generated/source files are `game/assets/maps/combatmaps.json`,
`original/u5/ultima5/BRIT.CBT` and `original/u5/ultima5/DUNGEON.CBT`.
Before full encounters: implement fixed-map seeding and remaining world
aftermath, then extend parity coverage. Before magic/advanced
AI: choose a capacity/error contract for TS vector growth and translate the
deferred status/field/special-action paths without altering RNG order.

Memory measurements and build results are in [VALIDATION.md](VALIDATION.md).
