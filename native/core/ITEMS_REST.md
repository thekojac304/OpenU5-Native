# Inventory, equipment, item use and rest foundation

This batch extends the single `GameState`/`PartyState` model and the semantic
command engine. TypeScript in `game/src/core` remains authoritative. No target
source, input binding, renderer, driver or TypeScript runtime was changed.

**38,728 new reference cases; 95,261 total**, plus 11 new adapter checks.
See [VALIDATION.md](VALIDATION.md) for execution results and missing assets.

## Source mapping

Paths below are relative to `game/src/core/`.

| Native API / state | Executed TypeScript reference and contract |
| --- | --- |
| `InitialState` pack fields | `state.ts:GameState`, `createBaseGame`: party-owned equipment, scroll, potion and reagent counts; scalar keys/gems/torches/skullKeys/carpets/grapple. InitialState value copying carries these fields. Full INIT/save decoding remains separate. |
| Six existing `CharacterState` equipment bytes | `state.ts:CharacterState`: helmet, armor, weapon, shield, ring, amulet. No second equipment model or cached attack/defense stats. |
| `ItemId`, `ItemQuantity`, `InventoryEntry`, `EquipSlot`, `ItemResult` | Compact identifiers, signed integer quantities, explicit slot and result types. Canonical equipment IDs are 0..47; empty equipment is **255**, not zero. |
| `equip_type_of`, `slot_for_equip`, `hand_state`, `total_equipped_weight`, `is_item_equipped` | `equip.ts`: same-named camelCase functions, exact 48-entry type/weight tables. Natural slot is a query, not the allocation rule for hands. |
| `equip_item` | `equip.ts:equipItem`; `game.ts:Game.readyItem` outside combat. Same gate order, toggle-off behavior, capped pack return, ammunition prerequisite, encumbrance, slot assignment and conditional ring RNG. |
| `unequip_slot` | `equip.ts:unequipSlot`: **uncapped** +1 pack return. Empty slot and nonexistent character fail without mutation. |
| `unequip_item_by_id` | `equip.ts:unequipItemById`, or `unequipWeaponById` with `attack_slots_only=true`: first matching slot is destroyed, with no pack return. Record order versus attack-slot order is preserved. |
| `roll_ring_expiry` | `equip.ts:rollRingExpiry`: query result only; caller chooses when to destroy the returned rings. Dead members skip; sleepers participate. Only 42/44 draw; 43 does not. Expiry is roll 11, unlike Ready's roll 0. |
| `ready_items` | `readyPicker.ts:buildReadyRows` ownership/selection projection: ascending IDs 0..47 whose pack quantity is positive or which this member wears. Names, glyphs, scrolling and UI stay outside core. |
| `ammo_item_for`, `is_thrown_weapon` | `equip.ts:ammoItemFor`, `isThrownWeapon`: data queries for later combat; no shooting, damage or ammunition consumption implemented. |
| `add_capped` | `counters.ts:addByteCapped` / `addWordCapped` arithmetic with explicit cap (99 default, 9999 for word resources). Does not impose these caps on other mutation paths. |
| `ignite_torch` | `world/survival.ts:igniteTorch`: zero fails; outside dungeon assigns 240; dungeon primitive adds 112+rand(0,15), capped at 255. |
| `consume_potion` | Conditional decrement in `main.ts` potion selection, before target selection. Positive stock loses one; zero is unchanged. Cancellation does not refund. |
| `reroll_potion_color`, `apply_potion_effect` | `usePotion.ts:rerollPotionColor`, `applyPotionEffect`; narrow state helpers in `magic/cast.ts:applyMani/applyCure/applyAwaken/applyPoison/applySleep`. Yellow uses the reference rand30 draw, including its dead-member gate. This is healing, not attack/damage resolution. |
| `UseItem` IDs 34,35,37 | `endgame/use-tools.ts:useSextant/usePocketWatch/useWoodenBox` and Game facade methods. Same ordered messages, signed-floor sextant rule, and no state/RNG/turn cost. IDs are from the **extended use table**, not equipment IDs. |
| `camp_context`, `camp_watch_count` | `world/camp.ts:campContext/campWatchCount`: dungeon/ship/town/terrain/transport gate order; only left-bed tile 171 accepts town rest. |
| `camp_guard_walk`, `camp_sleep_step` | `world/camp.ts:campGuardWalk/campSleepStep`: twelve 5-minute steps; regen before guard movement; conditional second direction roll; bounds then occupancy; one ambush gate per nonfinal hour. |
| `camp_hole_up`, `camp_wake` | `world/commands.ts:campHoleUp`; `world/camp.ts:campWake`. Guard skips partial HP/MP recovery; full-health living members still draw; one apparition gate follows the member loop. |
| Apparition recovery within `camp_wake` | `quest/lordbritish.ts:campApparition/levelForExp/recomputeApparitionMp`: living members heal/wake/cure, experience determines level, changed level draws one attribute increase, MP is recomputed even for dead members. No dialogue, quest progression or endgame logic is included. |
| `camp` | `world/camp.ts:camp`: begin message, hourly steps, early ambush return or ordered wake events. |
| `bed_sleep_begin/step/end`, `bed_sleep` | `world/camp.ts:bedSleepBegin/Step/End/bedSleep`: G→S only; six 10-minute steps/hour; clock→NPC snap→occupancy query; interrupt message; S→G; x increment; party-changed. No healing. |
| `camp_repair_ship` | `world/camp.ts:campRepairShip`, `world/transport.ts:repairHull`: at least one rand(1,3), repeat until hull≥10, cap 99, advance 25 minutes, hull message then party-changed. Missing TS hull defaults to 99 in this projection. |
| `Ready`, `Unready`, `Ignite`, `Rest`, `RestCancel`, `UseItem` | `commands.cpp` composes the above with the existing turn/time/RNG/event engine. Direct Game actions and context-turn execution are reference-tested. |

## Semantics that must not be simplified

- Inventory is shared by the party. Readied gear belongs to individual roster
  records. Readying transfers one item from pack to that member; toggling it off
  returns one only if the count is below 99. Another member cannot ready the
  last unit while it is worn. Explicit unready has a different, uncapped return.
- Ready checks ammunition IDs and battle armor restrictions before toggle-off;
  then possession and ammo prerequisites; then slot conflicts; then strength.
  A one-handed shield can occupy the weapon hand. A two-handed weapon needs
  both free but writes only the weapon slot. No class-based restrictions are invented.
- Already wearing an item toggles the first occurrence, not every occurrence.
  The TS `equipItem(255)` empty-slot quirk is preserved: it can report removal
  and increase `equipmentQuantities[255]`. It is not normalized into an invalid-ID
  rejection. Invalid non-equipped IDs with no count produce `Thou hast none!`.
- Ready's message is an `ItemResult` field. `Game.readyItem` emits **no
  GameEvents**, including on ring disappearance. There is no invented
  party-changed event or turn cost. Destructive removal also emits no events.
- Camp, bed sleep and ship repair do **not** consume food, increment the
  ordinary turn counter, or invoke survival housekeeping in the reference.
  Zero food does not prevent rest. Poison damage and starvation can resume on
  the next ordinary world turn; replay fixtures exercise that transition.
- Camp partial healing does not cure poison/sleep; apparition does. The guard
  misses partial healing, but participates in apparition. Dead members do not
  heal or level; apparition still recomputes their MP. Full-health healing
  still consumes RNG. The reference omits historical camp cooldown/hour gates;
  native code does not restore them from original-game assumptions.
- Bed end wakes **all S** in the party prefix, including members asleep before
  rest, and increments x even when thrown out. Zero-hour direct `bedSleep(0)`
  still begins/ends; semantic `RestCancel` and a resolved zero-hour Rest do not.
- Failed Ignite and rejected camp context consume the existing standard turn.
  Rest cancellation and ready/unready do not. `Rest` resolves context before
  cancellation; ship rest repairs directly without an hours prompt.

## Continuations and services

`RestServices` borrows pure core callbacks. The bed path invokes `snap_npcs`
before `occupied` on **every** step. These correspond to `Game.wakeSnapNpcs`
(`NpcManager.enterMap`) and `Game.objectOrNpcAt` (objects plus actors), not to
an ordinary NPC movement tick. A host can bind existing `enter_npc_map` and
its object/actor ownership layer. The fixture records callback order and
arguments. No terrain-only approximation of occupancy is supplied.

Guard start/free-cell callbacks represent the CampFire arena formation and
passability. No callback means no guard start cell, or an unrestricted free-cell
predicate, matching the optional TS `CampCtx` callbacks. Full Game arena-data
wiring awaits the combat owner and the missing combat-map asset.

Camp/wake require a provider of **already quoted** KARMA.DAT records. The native
event borrows that string synchronously, so there is no fixed-buffer truncation
of asset prose. Tests install six explicitly synthetic records in the TS asset
registry and provide identical native records. Original message assets are not
embedded in fixtures. Absent providers are explicitly rejected before mutation.

An ambush returns the exact enemy definition index after the same RNG draws
and `Ambushed!` message. The command reports `AwaitingResponse`, stores
`CommandState.pending_camp_enemy`, and blocks subsequent commands until a future
combat owner accepts/clears that handoff. It does not heal, print Party rested,
spawn an approximate combatant, or execute combat. Full `startCombat` output
and its RNG fork are explicitly outside this batch.

`execute_command` operates on resolved semantic actions. Ready/Unready and
resolved UseItem/Rest choices are modal continuations and bypass initial
key interception. Ignite is a primary command in `dispatch_world_command`
and uses the existing drunk/auto-sleep prelude. No hardware keys are bound.
Initial inventory/rest prompt UI and its opening-key dispatch remain outside
this API. Presentation SFX are ordered event IDs only; no audio code is added.

## Domains and memory

Existing character equipment occupies six byte fields; **no per-member size
increase**. Existing byte coordinate and 16-record roster contracts remain.
Commands require partySize 0..6 and valid borrowed maps. Pure prefix helpers
accept valid rosters up to 16; quantity arithmetic must remain in int32 range.
NaN, fractions, arbitrary JS properties and IDs outside byte equipment storage
are not representable and must be validated by adapters/codecs.

The 48 canonical equipment counts use int32 because `unequipSlot` has no byte
cap. The same fixed array has 208 additional byte-ID entries to preserve invalid
equipped IDs and the 255 sentinel behavior without a heap or parallel inventory.
That compatibility tail costs 832 bytes. It is state, not a new gameplay item
table. Missing entries map to numeric zero; JS array length/property presence
is not reproduced. Count operations are not universally capped or byte-wrapped.

`GameState`: **576→1,720 bytes (+1,144)** on host and ESP. `PartyState` stays 524,
`CharacterState` stays 32, and its equipment stays 6 bytes. All new counts and
ship hull live in the existing state; no alternative pack owner exists.

New bounded scratch: apparition has 16 roll bytes and a 192-byte transient
level-up message; tool messages use 96 bytes; repair uses 48 bytes. These are
automatic locals, with synchronous borrowed event delivery. No heap allocation
or persistent event queue. `ReadyItems` is 49 bytes, `RingExpiries` 33 bytes.
These are not whole-call-stack measurements. Caller callbacks retain their own
stack and storage costs. Borrowed maps/arena data and the pre-existing 323,084-byte
A* scratch remain future PSRAM candidates; this batch allocates none of them
in the target. No new PSRAM allocation is required for the item/rest helpers.

Never serialize these structs or their padding as a save format. Binary save
offsets, signed floors, optional fields and JS migrations still require explicit
codecs. Native default construction is not complete INIT.GAM initialization.

## Explicitly deferred

- Full combat, damage/attack formulas, ammo firing/underflow, thrown-weapon loss
  orchestration, `characterWeapons`/`characterDefense` combat table aggregation,
  arena synchronization, invisibility/shape rendering, combat Ready turn cost.
  Existing equipment state and type/weight/ammo/ownership queries are available
  to the next batch without adding cached combat modifiers.
- Full potion command ceremony/target prompt and map reveal. Consumption,
  reroll and state effects are implemented; purple/black return the reference
  result but do not synchronize combat records, and white exposes `reveal` for
  the future map-reveal owner. No full spell engine or duplicated attack formula.
- Scroll casting, reagent mixing, spells and their quantities, inventory for
  special flags/artifacts/shards/moonstones, full extended-use selector,
  magic carpet transport, spyglass/zodiac view, badge/artifact powers, skull-key
  door/dungeon/quest actions, loot/search/drop/world-object transfer, shops,
  dialogue and quest grants. Shared-pack transfer through Ready is covered;
  no general transfer action is invented where there is no corresponding helper.
- Full combat ambush initiation and Game arena/formation construction; dungeon
  camp orchestration; inn rest and quest rescue. Only the camp-owned apparition
  recovery loop is translated from lordbritish.ts.
- Bed x=255→256 is outside the existing byte-coordinate model. Atomic bed sleep
  and bed end reject it before mutation; ordinary local-map x=31→32 is preserved.
  Stepped bed callers must supply valid scheduling/occupancy callbacks and honor
  bed-end's return value. Do not silently wrap this coordinate in an adapter.
- UI, input, renderer, audio, save/load and all hardware integration.

## Fixture contract

`tools/generate-item-fixtures.ts` executes the real runtime functions and Game
methods. `fixtures/items.txt` uses numeric rows with `zN` encoding N zero tokens;
the expanded vector lengths are explicit. Before/after state includes the full
byte-ID pack, potion counts, all relevant member fields, time, resources,
turn state and RNG seed. Results, ordered messages/SFX, every RNG draw and
bed/guard callback order are compared. Replays include shared-pack ownership,
depletion, repeated rests, zero/full-health/dead/sleeping/poisoned members,
empty/short parties, world progression after rest, and both turn-cost outcomes.

Coverage witnesses for disappearance, expiry, ambush, apparition, bed
interruption and toggle-off are required, not merely printed. Both fixture and
coverage JSON are checked for live drift. No mismatch is tolerated or used to
change TypeScript behavior.
