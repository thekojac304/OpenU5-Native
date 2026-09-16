# Combat magic and generic advanced combat

The existing TypeScript runtime is authoritative. No TypeScript runtime,
original arena asset, hardware control, renderer or sound behavior was changed.
This ledger supersedes COMBAT.md's magic/special-AI/growth exclusions only in the
domain below; it does not claim complete world or dungeon magic integration.

## Source map

Reference paths are relative to `game/src/core/` unless specified otherwise.

| Exact reference | Native implementation |
| --- | --- |
| `magic/spells.ts:buildSpellDefs`, `data/MagicDefinitions.json`, `magic/tables.ts:TIME_PERMITTED_BITS` | `SpellId`, `SpellDef`, generated `magic_tables.inc`, `spell_definition` |
| `magic/mix.ts:mixSpell` | `mix_spell`, existing reagent inventory, new 48-slot mixed-spell array |
| `magic/cast.ts:castSpell`, `effectFor`, `applyGlobal`, `castAbsorbedOutOfCombat`; `magic/tables.ts` | `cast_spell`, `CastResult`, `SpellEffect`, existing mana/food/wind/light/temporary-status owners |
| `magic/cast.ts:applyMani`, `applyVasMani`, `applyCure`, `applyAwaken`, `applyResurrect` | `apply_target_spell` |
| `combat/combat.ts:combatCastAbsorbed`, `combatCastEffect`, `playerCast`; semantic combat Cast callback in `game/src/main.ts` | `combat_cast`, `CommandKind::Cast`, `GameEvent::Combat` delivery |
| `Combat.castCombatAttack`, `nearestEnemy`, `resolveRangedFlight` | `Engine::spell_flight`, `nearest`, existing physical hit/strike pipeline |
| `Combat.castEarthquake`, `castMassSaving`, `savingResist`, `castCharm` | `Engine::cast_effect`, `saving` |
| `magic/areaSpell.ts:spraySpellCells`, `areaSpellTables.ts:SPRAY_SLOPE_CURVE`, `blocksSpellLine`; `Combat.castLineAoe`, `applyLineCell`, `lineSpellTypeImmune` | `Engine::spray`, `line_cell`, existing generated LOS bitmap |
| `Combat.castPolymorph`, `castIllusion`, `boardHasCellFor`, `castSummon`, `castSummonDaemon`, `makeEnemy`; `combat/formulas.ts:randomBoardCell`, `pickSummonCell` | `Engine::cast_effect`, `spawn`, `board_cell`, `summon_cell` |
| `Combat.castBlink`, `castReveal`, `castDispelField`, `fieldBlocksCell`, `endOfTurnFieldDamage` | `Engine::cast_effect`, `field_blocks`, `advance`, caller-owned ordered `CombatField` slots |
| `Combat.divide`, `applyDamage`, `enemySpecial`, `enemyAttack`, `enemyMove`, `enemyTurn` | `Engine::divide`, damage hook, `enemy_special`, amulet negation, teleportation and existing turn pipeline |
| `Combat.putToSleep`, `wakeUp`, `collapsePossessed`, `advanceTurn`, `kill`; `Game.endCombat` | Existing helpers reused by effects; new parity through `finish_encounter_combat` |

## Exact behavior retained

- 49 metadata entries, 48 tracked mixed-spell quantities. Nox has no permitted
  context. Mana cost equals circle. Metadata preserves original target/type/time
  strings and capitalization.
- Mixing validates all required reagents before mutation, consumes them, and
  caps mixed stock at 99. Casting consumes no raw reagents. It checks absorption
  and context, then stock, consumes a mixed spell, checks mana, spends mana,
  then checks level. Insufficient mana still loses the mixed spell. No invented
  generic success roll was added.
- Combat negate/Blackthorn absorption consumes the turn without resources.
  Failed context/stock/mana/level casts also finish the actor turn, matching the
  combat callback. Paid aim cancellation does not finish it. Target helpers
  retain the reference's HP/status synchronization quirks; full-HP Mani still
  rolls and reports failure. Resurrection is rejected in combat by the TS gate.
- Field-wall casts become weapon attacks, not field objects. The raw
  `combat_cast_effect(Field)` remains a no-op as in raw `playerCast(fieldWall)`.
  Attacks reuse hit, armor, scatter and loot RNG. Xen Corp does not gain an INT
  save absent from this TS pipeline. Quake and line effects preserve the default
  non-magic damage flag and its undead reduction.
- Sprays trace 21 pixel rays and at most 63 ordered unique cells. Each actor is
  hit once, but a newly split actor can be hit in a later cell. Quake snapshots
  the original actor list. Sleep/death immunity follows the INT roll. Charm
  toggles the existing charm flag.
- Summon makes up to eight global board picks, consuming two draws even on an
  off-board pick. Swarms share one placement, with a speed draw per actor.
  Daemon allegiance follows its speed draw; always-ally skips allegiance RNG.
  Illusion copies the record and rerolls placement without a speed draw. Its
  potentially unbounded reference retry is retained. Polymorph retires the old
  actor without death/loot and appends a fresh rat.
- Divide follows every nonlethal damage application, including zero damage,
  before wound classification. It tries eight valid adjacent picks and creates
  fresh actor state with the parent's HP/max-HP. Dead/fled records remain in
  order. There is no invented 22-actor gameplay cap.
- Possession, invisibility toggles, daemon gates, teleportation, amulet negation
  before range testing, poison/sleep, confusion, quickness, time stop, fleeing
  regeneration and charmed melee retain their decision/draw order. The plague
  flag has no consuming generic TS behavior; none was invented.
- Temporary global status remains in `TurnState`. Actor turns do not decrement
  its duration: TS `Combat.advanceTurn` does not. Previously translated world
  progression owns expiration. Field slots retain overlap/order; energy blocks
  movement/placement, terrain takes precedence over field damage, and dispel
  removes the first matching slot. No new field duration exists.
- Events preserve text, actor/target, optional fields, order, death, victory
  linger and repeated end notifications. Quake/line-spray tags have no renderer
  dependency. TS emits no separate resource-consumed or summon-success event;
  none was invented. Presentation ceremony/audio/rune prompts are excluded.

## API and memory ownership

`GameState::spell_quantities[48]` extends the existing inventory. Mana remains in
`CharacterState`, reagents in `GameState`, and actor statuses in `CombatActor`.
Bind immutable definitions through `CombatContext::enemy_defs/ enemy_def_count`.
Missing summoned definitions produce the reference's silent no-op.

`CombatState::actors` retains 22 inline records plus a caller-owned overflow
span. IDs, counts, scan positions and remembered actor IDs are signed 32-bit.
Addresses remain stable within each action. TS has no combatant growth cap.
Before an action, provision this conservative number of additional records:

- `combat_action`: `combat_growth_reserve(state)`.
- Cast/effect: that reservation plus four.
- With a divide-capable record the reservation is `max(count,63)+1`; otherwise
  it is one for daemon gating or zero. Closing terrain damage is covered.

Insufficient storage returns `NeedsActorStorage` / semantic `NeedsStorage`
before selection, events, resources or RNG changes. This is a storage error,
not an in-game summon failure. A caller can provide more storage and retry,
preserving used records; it must not resize during callbacks. Counts/IDs must
fit signed 32-bit arithmetic. Raw swarms admit at most the reference's four
actors per cast. Ordered field slots are also caller-owned through CombatState.

For `CommandKind::Cast`, `item` is SpellId, `member` the party target,
`has_target` enables `combat_x/combat_y`, and `cancel_target` models paid aim
cancellation. Command success means dispatch completed, as for existing combat
commands; gameplay refusals are events. `cast_spell` separately exposes
`ok/consumed/effect`. No hardware keys are bound.

Callbacks remain synchronous, ephemeral and read-only as in COMBAT.md. Copy
retained payloads; do not reenter. Final state is compared after actions;
callback-time state is not a TS event-array API. No heap or event queue is added.
See VALIDATION.md for measured state sizes and scratch requirements.

## Parity

**257,024 new / 609,373 total snapshots pass.**

| Corpus | New snapshots |
| --- | ---: |
| Resource/target helpers: 49 spells × 32 configurations × 16 seeds | 25,088 |
| Synthetic combat: 49 spells × 40 configurations × 8 seeds × 8 snapshots | 125,440 |
| Real arenas: 128 maps × 4 entries × 13 spells × 2 configurations × 8 snapshots | 106,496 |

Traces cover initialization, repeated casts, enemy turns, escape, and actual
`Game.endCombat` roster/RNG handoff. They compare all actors, mutable terrain,
resources, statuses, initiative/queue, XP/loot, field order, event fields/text,
and every RNG range/result. Native Cast uses the semantic dispatcher. Coverage
assertions require successful effects and ability witnesses, including on real
maps with sufficient mana/levels. Cases include empty resources, full healing,
repeated statuses, dead/immune targets, blocked/full boards, stacked fields,
cancellation, no enemy slots, growth beyond 22 and IDs above 255 (300 additional
actors). Three native adapter checks verify storage/invalid-input atomicity.

Real maps come from the existing BRIT.CBT/DUNGEON.CBT extractor. CTest's physical
real-map setup compares JSON with freshly parsed CBT bytes. Advanced tests use
controlled synthetic definitions to isolate abilities on real terrain, not
claimed original encounter populations. Derived real fixtures remain ignored
in `build-real-arenas/`; no original data was fabricated.

Run both new generators with `node --import tsx ... --check`, then the existing
CMake `host-test` with `OPENU5_REAL_ARENAS=ON`. CTest generates the advanced real
corpus. Expectations execute the unchanged reference; mismatches fail at the
first field. VALIDATION.md records checks, sizes and firmware commands.

## Deferred boundaries

World/dungeon targeted magic orchestration (doors, travel, dungeon levels,
world blink), rune/reagent pickers, incorrect selected-recipe trap orchestration,
remaining scroll/potion selection workflows, ceremony/animation/audio remain
outside this combat batch. Pure global dispatch and resurrection helpers are
available without claiming their entire world command flow.

Fixed dungeon sprite/EC-group/object seeding, room/corridor quest persistence,
absorption/endgame/capture scripts, scripted bosses, combat board get/open/loot
selection and full world aftermath retain the previous boundaries. Generic
field slots can now be supplied/resolved; the excluded fixed-room loader does
not populate them automatically. Raw TS no-ops remain no-ops. No observed generic
magic/advanced-combat parity mismatch blocks future dialogue/shops/quests work.
Dialogue, shops, quests, UI and device integration were not started.
