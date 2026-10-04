> English migration of `re/deliberate-divergences.md` (the Spanish note in the private repo remains the source of truth; translated 2026-07-22).

# deliberate-divergences.md — deliberate divergences of the clone (Task F.2)

Final state of the exact-migration project of Ultima V (DOS). The coverage ledger is
at **100 % (202,800 / 202,800 B**, tag `re-complete`) and the game rules are
**re-derived from the binary with asm citation**. This document catalogs
everything that, after F.2, does **not** close as `✅ RUNTIME-VERIFIED` against DOSBox,
explaining **what** the divergence is, **why** it is deliberate (not a modeling
gap) and **how to close it** in the future.

It is the complement of `docs/FIDELITY.md`: after F.2 **no ⚠️ of FIDELITY is left
orphaned** — each one is either (a) `✅` closed, or (b) a deliberate divergence
cataloged here with its closure path. The project's closure criterion is
**complete honesty, not impossible perfection**.

---

## 0. The three levels of verification

Every ported rule lives at one of three levels. The first two are valid
closures; the third is a documented deliberate divergence.

1. **`✅ RUNTIME-VERIFIED`** — asm-derived rule **and** collated byte by byte
   against `ULTIMA.EXE` running in dosbox-x (the oracle of Task 0.3). It is the
   highest level. 6 subsystems reached it (see `re/COVERAGE.md §4`):
   movement/clock/food (3.1), combat (seeded trace), wind (byte-exact),
   lunar feed, gypsy seed==0, world-tick RNG orbit.

2. **STREAM PARITY (model↔clone)** — asm-derived rule and cross-checked between TWO
   independent models: the asm-derived Python prediction (`re/tools/*_parity.py`,
   `KernelRng`) and the clone's reproduction (`OriginalRng` + the TS core). Both must
   trace the **sequence of rands** and the resulting state. It is NOT an
   approximation: it is a second independent derivation that refutes errors of a
   single implementation (it exposed ≥3 real bugs during Phase 3). It is the established pattern
   for everything that **cannot be seeded headless** in DOSBox (BSS registers
   of chest/object, key protocol of a command not yet RE'd, etc.).

3. **DELIBERATE DIVERGENCE** — a conscious boundary: either the original's value
   has not yet been measured (oracle question), or the behavior is UI/presentation
   outside the rules core, or it is a scope decision (not porting the
   U4 transfer), or it is additive QoL (`❌`). Each one, cataloged below.

The **critical path gypsy→endgame** (F.2, `test_master_scenario.py`) is level-2 stream
parity that chains ALL the phases with RNG into a single seed — the
stream unification that 3.13 left pending.

---

## 1. Class A — STREAM parity instead of runtime-DOSBox (`⚠️→formulated`)

**What.** Asm-derived rules with citation, ported to the clone and cross-checked
model↔clone, but **without a parity scenario executed against live DOSBox**. In FIDELITY
they carry the label `⚠️→formulated`.

**Why it is deliberate.** The oracle collated live the 6 subsystems whose state
is **readable and seedable** by headless RAM (clock, wind, lunar phases, combat HP,
seed). The rest requires, for an honest live collation, either to **seed BSS
structures** that the binary only populates when loading resources (chest/object
registers of 8 B, small-map NPC arrays), or to **drive an interactive
command whose key protocol is not yet RE'd** (the Cast picker, the
Ready prompt). The pty channel of the dosbox-x debugger **loses pauses** (~1 of
every 12 reliably readable rolls; `re/notes/oracle.md` rule 7), which makes
capturing a long rand stream with a one-shot breakpoint infeasible. The model↔clone stream
parity gives the same exactness guarantee without depending on the pty.

**Subsystems at this level** (asm provenance + green stream harness):

| Subsystem | Rule | Harness | Live closure path |
|---|---|---|---|
| Magic (3.3) | table-driven dispatcher, weapon damage, states | `magic_parity.py` (10) | RE of the Cast key protocol (picker 0x8a08 + parser 0x00de) → `test_in_lor_light_live` |
| Dungeon (3.4) | traps/fields/pits/fountains, DEX contest | `dungeon_parity.py` (14) | seed DungeonState in headless RAM |
| NPC (3.5) | schedule_index, Manhattan wander, floor gate | `npc_parity.py` (14) | seeding of small-map NPC arrays (npc.md §14) |
| Shops (3.6) | INT-based haggling, reagents, guild, inn, tavern | `shops_parity.py` (10) | BP 0x02D8 (runtime price by INT) — see §3 |
| Transport (3.7) | wind-pushes, di%3 drift, broadside, Cape | `transport_parity.py` (17) | wire Board/Fire/X-it keys to the loop → §2 |
| Shrines (3.8) | 100·n donation, moongate destination, midnight edge | `shrines_parity.py` (46) | moongate teleport reloads chunk (write_mem is not enough) |
| Commands (3.9) | Push, Camp/Hole-up, search_dungeon | `cmds_parity.py` (24) | object table loaded → §2 |
| Blackthorn (3.10) | capture, interrogation, refuge, TALK 0xFF guards | `blackthorn_parity.py` (45) | refuge (F1.7-T1) ✅ + capture/interrogation (F1.7-T2) ✅ + **password (F1.7-T3) ✅** wired to the town-turn (TOWN 0x12ae before 0x1436, prompt-machine with MISCMSG strings); password = escape route from the capture loop (TALK 0x02a4, "IMPE" case-insensitive over the FIRST 4 chars — 0x02dc truncates the buffer; `blackthornPassGranted` suppresses re-capture). Gate `g_time_spell==0x1d` (Black Badge) ⚠️ and the password→capture connection NOT derived (added by the clone as conservative) = Class C §3 [⚠ 07-30: connection DERIVED — the capture path EXECUTES the guard_demand TALK 0x1e2 (0x2a4 gate included) via TALK 0x031E; the binary writes no persistent pass. Cards T-A/T-B, re/notes/talk-031e-resolucion.md]. **TRIGGER by adjacency (F1.7-T5)**: replaces the per-turn one of T2; captures upon being ADJACENT (manhattan==1) to a type 0x70 guard (`blackthornGuardCaptureTriggers`). MIXED grade: **adjacency NECESSARY ✅ derived** (active idx `[0x65bf]` is only set by the fast-path 0x06E4, 2 writes) and **sufficiency ✅ B-RUNTIME 2026-07-14** (previously Class C APPROXIMATION): witness in DOSBox — party driven to adjacency of a Palace guard (loc 0x12, entry via (E)nter which DOES populate the NPCs), BP at the npc_engine gate (TOWN 0x13b4 `load_seg:0x9584`) + capture (0x12ae `load_seg:0x947E`): **marker=0x74, result==1 (`[bp+4]=0`), and the CAPTURE FIRED** ⇒ since result≠2, the only route to 0x12ae is 0x13ba→0x1912, then **the OPAQUE handler `0x1912` returns ≠0 for the palace guards** (and `dialogNum≠0`, guards=255) [07-30: derived — it is TALK 0x031E→0x1e2, ret 1 without the badge]. **The clone (adjacency alone ⇒ capture) is CORRECT; the feared over-fire (`result==1 ∧ 0x1912==0`) does NOT materialize.** Detail: `re/notes/oracle-blackthorn.md`. (Seeding loc=0x12 directly does NOT load guards; you must ENTER via the map with (E).) Chase loop (b) `rand(0,0x3f)` = separate Class C (movement). Reachability green `e2e/blackthorn.spec.ts`. guardWander of the palace guards = standard NPC dispatch (distinct from the `guard_wander` TOWN 0x0C78 of tile-under 0x10 objects —stable horses included— not wired game-wide). drain (F1.7-T4) ✅ |
| Gypsy (3.11) | 4+2+1 tournament, stats with floor | `gypsy_parity.py` (11) | BP at pick_virtue 0x9a6 (title→quiz) — see §3 |
| Loops (3.13) | RNG order per turn, exact spawn | `loops_parity.py` (13) | MODEL↔CLONE stream unification ✅ CLOSED in F.2 (`test_master_scenario.py`); the LIVE stream of `game.ts` remains open → §2 |
| Endgame/Zstats (3.12) | encumbrance, playtime, special look, trap-detect | clone tests | seed roster + UI |

**How to close them (general).** Each subsystem needs either (a) a headless
seeding helper of its BSS structure in `oracle.py`, or (b) the RE of the key
protocol of the command that triggers it. Both are harness work, not re-derivation:
the rule is already fixed by the asm. In the meantime, stream parity is the
valid closure.

---

## 2. Class B — pending interactive wiring (pure functions + hook)

**What.** Verified primitives (pure functions with tests) that **are not yet
connected** to the clone's interactive command loop: they are exercised by harness and
by unit tests, but a player does not yet trigger them with a key.

**Why it is deliberate.** The migration was done at the **library** level: each rule was
ported as a verifiable pure function before wiring it to the UI. The wiring is
UI integration work, not fidelity to the binary; separating it kept each
task bounded and verifiable. The clone is playable at its core (movement, combat,
turns, creation); these branches are secondary commands.

**Inventory** (all with pure function + hook ready):

- 🐛→✅ **FIDELITY BUG — (U)se Skull Key missing + In Ex Por wired wrong** (detected
  2026-07-13 by the user playing + asm audit; **Skull Key half RESOLVED 2026-07-14, task #22 —
  ⚠️ the In Ex Por half of this entry was RETRACTED 2026-08-07, see point 2**).
  In the original the skull doors (0x97/0x98) open with **(U)se → Skull Key** pointing
  at the door (`dec g_skull_keys` + demagifies) — **and also with the In Ex Por spell, which
  reaches the same worker** (point 2). Not with (O)pen. The
  `CAST.OVL 0x18c4` that the clone attributed to the **In Ex Por** spell was actually the
  **Skull Key branch of the (U)se item dispatcher** (0x1792–0x1baa, jump-table `0x185d`,
  neighbor of the carpet `0x18a1`). **RESOLUTION (hard citations):**
  1. **(U)se Skull Key ported** — `game.ts::useSkullKey(dir)` reproduces 0x18c4-0x1902:
     `dec g_skull_keys` (0x18c4) → "Skull Key" (DS **0x48fe**, byte-exact) → gate by
     `g_location` (dungeon 0x21..0x7F → "Not here!", DS **0x4909**; combat ≥0x80 →
     silent; overworld/town → getdir + demagifies 0x97→0xB8 / 0x98→0xBA via
     kernel 0x75a2, 0x18f4). Wired in `main.ts` (U → item picker → Skull Key →
     getdir). **Underflow guard RESOLVED**: the (U)se picker only lists items with
     count≥1 (the picker nav skips qty 0, ZSTATS `find_next/prev_owned` 0x05a4/0x056c;
     the extended table 0xB9EE is flattened by `build_extended_item_table` 0x099a) ⇒ the `dec`
     goes from 1→0 at minimum: the underflow to 255 is **NOT reachable** in normal play.
     Bug-for-bug ported: the key is spent even if the direction is canceled or there is no
     door in front (dec 0x18c4 goes BEFORE the getdir and the tile check). **⚠ Grade of the
     tile mapping of the demagifier** (0x97→0xB8 / 0x98→0xBA): ~~derived by SYMMETRY~~
     **✅ CLOSED 2026-07-25: READ BYTE BY BYTE.** It was not at "kernel 0x75a2" because
     0x75a2 is not a kernel address — it is the RAW near-call target of CAST.OVL 0x18fc
     (per the overlay-load-layout rule ⇒ CS 0x3522, which is only the EFFECT: coordinate
     conversion + PLINK thunk 0x10e0 + glide 2000→3000 + redraw). The mapping lives in
     **CAST2.OVL 0x0768** (reached from CAST.OVL 0x18dd `call 0xffffc16e` → kernel stub
     0x80ee): `0782 call 0x6222` = kernel **0x4402 get_tile_ptr** → bx = pointer to the tile
     byte; `078f al=[bx]`; `0793 cmp 0x97 → 07a0 mov [bx],0xB8`; `0798 cmp 0x98 → 07b2
     mov [bx],0xBA`. The port's mapping is EXACT; this item is now full-A.
  2. 🔴 **RETRACTED 2026-08-07 — In Ex Por (#26) DOES open magic doors.** This item used to
     read: *"its REAL handler `CAST:0x1026` is getdir + cast animation (effect 5, kernel
     0xffffc186) and NOTHING ELSE: it does not touch g_skull_keys nor change tiles (verified
     0x1026-0x1037)"*. **The conclusion was false**, and so was the call it named.
     - **The worker has TWO callers**, both through stub `0x80ee`: `CAST.OVL:0x18dd` (the Skull
       Key success path, item 1 above) **and `CAST.OVL:0x1026`, the arm of spell #26**. A near
       `call` is encoded **relative**, so `grep`ping the literal finds **0 of 2**; resolved by
       load-slot band instead.
     - **The first instruction of the arm is `call 0xffffc16e`** → `CAST2.OVL:0x0768`, which
       takes a **pointer into the live map** and writes `0x97→0xB8` / `0x98→0xBA`. That is
       opening a magic door.
     - The `0xffffc186` this item named is a **different stub** (→ `CAST2.OVL:0x0000`, the cast
       jingle/flash) and it is **not "kernel"**; it occurs 34 times in `CAST.OVL` and **zero
       times inside `0x1026-0x1037`**. Whatever animation plays comes from the tail at `0xf2a`,
       which **nobody has read yet**.
     - **Reachability POSITIVE**: the per-spell context mask at `DS:0x1c90` (bit 8 overworld,
       4 town, 2 dungeon, 1 combat) gives In Ex Por = `0x05` = **town + combat** — exactly where
       magic doors are. Bit mapping accredited against **seven** already-published spell
       contexts; `An Ex Por`, the **sealing** spell, carries the **same** `0x05`.
     - **The jump table** that proves the arm is spell #26: at fileoff `0x1146`, 48 entries
       (cardinal fixed by the `cmp ax,0x2f / jbe` guard), **48/48 inside the routine body, 48
       distinct**. ⚠️ **Entries hold CS offsets, not file offsets — subtract the `0xBF80` slot.**
       Reading them raw yields a clean, wrong "0 of 48 inside the body".
     - **Why the error happened** — the cited range `0x1026-0x1037` was **exactly right**: seven
       instructions, the whole arm. But **"and nothing else" is not a property of the RANGE; it
       is a property of the CALLEE**, and the callee was never opened. `0xffffc16e` does not
       *look* like CAST2, and the worker begins with a getdir — so "getdir" describes precisely
       what is visible from outside. **Rule: never assert "and nothing else" about a range that
       begins or ends with a `call` without opening the callee.**

     ⇒ The correct statement is **not** "it was the key, not the spell": it is **one worker, TWO
     callers**. Task #22 correctly identified the Skull Key branch and **over-corrected** by
     removing the spell without censusing the callers.

     **Everything else in item 1 above stands**, including the underflow-guard resolution.

     **Clone status: still divergent.** `cast.ts:26 → {kind:"castAnimOnly"}` omits a mechanic the
     original has. Tracked as a fidelity defect to fix separately (it touches `game/src` and
     requires measuring whether it moves the RNG stream first).

     Chain confirmed by three independent channels; full derivation, controls and declared
     unmeasured ends in `re/notes/inexpor-dos-llamadores-acta.md`.
  3. **An Ex Por (#25) verified as a real SEAL** — `CAST:0x1020 → sub 0x846`: getdir +
     effect, and over the pointed tile **0xB8∨0xB9→0x97, 0xBA∨0xBB→0x98** (asm 0x878-0x88a
     read byte-by-byte: the `jbe` cover BOTH frames ⇒ **it also seals the door with
     KEY lock** 0xB9/0xBB, not just the normally-closed one); it does NOT consume skull key.
     `game.ts::applyDoorSpell` (now only sealDoor) maps the 4 entries.
  Tests: `doors.test.ts` (useSkullKey 0x97→0xB8 −1 key, no-door spends key, getdir
  canceled spends key, dungeon "Not here!", An Ex Por seals 0xB8 and 0xB9→0x97) + e2e `commands.spec.ts`
  ((U)se Skull Key → −1 · 0x97→0xB8 · Open opens · autoclose does not re-cast the lock). Doc:
  **`docs/superpowers/specs/2026-07-13-finding-skull-key-use.md`**. See §3 C2 (updated).
  The strings "Skull Key"/"Not here!" are now **[V]** (byte-exact from DATA.OVL); the
  "Locked!" of the An Ex Por sealing remains **Class C**.

- **Unification of the LIVE stream of `game.ts`** — ✅ **CLOSED in Phase 1.1** (PORT
  live stream; commits `e0458af` T1, `19575cc`+`c8e667d` T2, `0385aee` T3 and the
  T4 closure). `game.ts` now consumes a **single live `OriginalRng`** (`liveRng`, the
  shared `g_rng_seed`, game.ts:210): `move()` separates the step geometry
  (`resolveStep`) from the turn and **delegates the RNG skeleton** to
  `outdoorTurn`/`townTurn` of `turn.ts`, with the EXACT order of the binary
  (wind → clock → hazards → housekeeping → spawn gate). The three separate
  sources were eliminated (`encounterRng = Math.random`, the spawn picker's
  `Rng`-hash, the seed derived from combat). The sub-details that came in with this
  closure, all **resolved**:
  - **`pick_spawn_coords` exact** — 2×`rand(0,31)` with re-roll by distance
    (MAINOUT 0x0FC4/0x0F4E) consuming the live stream; goodbye to the heuristic
    placement and the `Rng`-hash. (`spawn.ts`, `enemies.ts`.)
  - **2nd world-turn of the npc_engine** (`[0x65BF]`) and **Quickness/mount phases** of
    the world_turn (mount = alternate phase by `turnsSinceStart`). (T2.)
  - **NPC wander over the live stream** — ✅ **CLOSED in #57** (residue of F1.1),
    with the GATE still approximate (see below). `NpcManager` consumed a **parallel**
    `OriginalRng(0)` (`setRng` was not called from anywhere); now `game.ts`
    **shares the `liveRng` instance** with the manager (`setRng` in the constructor),
    so the wander (NPC.OVL:0x0C50, same rand kernel 0x2092) rolls on the
    single `g_rng_seed`. The **ORDER is exact**: the `npc_tick_all` falls via the hook
    `afterHousekeeping` of `townTurn` AFTER post_turn/housekeeping (and of
    `guard_wander` 0x165F, not modeled) and BEFORE the 2nd world_turn of the npc_engine
    (TOWN 0x166E, between 0x165F and 0x1683), not at the end of the turn.
    ⚠️ **GATE approximate** (same limitation already declared of the heuristic
    2nd world-turn): the binary runs `npc_tick_all` ONLY with `result==1` — with
    `result>=2` it SKIPS it (`0x1662 cmp [bp-0xa],2; jge 0x1671`) but nevertheless
    fires the gate of the 2nd world_turn (npc_engine 0x1671). The clone uses
    `consumesTurn` (boolean `result!=0`) and **does not distinguish `result==1` vs `>=2`**,
    so it rolls the wander in one case (`result==2`) where the binary would not;
    closing this requires the real `result` of the dispatch of `town_read_command`
    (same pending getkey-level wiring as the 2nd world-turn → F.2).
    (`game/tests/npc-live-stream.test.ts`.)
  - **Combat and dungeon over the stream** — the combat seed is a fork of
    `liveRng.getSeed()` and `endCombat` **resyncs** `liveRng` with `finalSeed`
    (COMBAT.OVL churns the same `g_rng_seed`); the live dungeon **injects the
    `liveRng` instance** into `DungeonState` (not a fork), so that
    traps/fields/rooms and the room combat share the stream
    (re/notes/dungeon.md L11-13: `rand` = single kernel 0x2092). (T3 + T4 closure.)
  - **Shadowlords re-roll at midnight** (0x4FF5→0x5004) by the live stream, in
    overworld/town (via `turn.ts`→`advanceClock`) **and** in dungeon/ignite (via
    `advanceTurn`→`advanceClock`, which now forwards the `rand`). (T3 + T4 closure.)
  - **`guards.ts`** — facing is **X-only by the asm** (0x0D36/0x0D3E re-faces only the
    X branch; the Y branch jumps to 0x0D55 without touching the tile — it was not a clone bug but
    the real rule), with validation of the **DESTINATION after the 3 rands** (`destBlocked`,
    0x0D55-0x0D8B). (T3.)

  **E2E determinism by seed**: `game/e2e/determinism.spec.ts` proves that two
  games with the same seed (`?seed=` / `__u5test.reseed`, which seed `liveRng`)
  reproduce the SAME state sequence (position/time/gold/HP after N identical
  actions), including the midnight crossing; different seeds diverge. **Only
  open sub-detail**: the **`troll_toll` with interactive Y/N prompt** (today
  auto-pay) is deferred to **Phase 1.3** (UI prompts layer), along with the rest of the
  Y/N prompts of this §2.
- **Naval transport**: ✅ **CLOSED in Phase 1.2** (naval PORT). Keys Board (B,
  CMDS 0x07F6), X-it (X, 0x0EB4), Yell (Y, 0x1418) and Fire (F, 0x0962) wired to the
  dispatch of `main.ts`; `move()` gains the naval branch (`shipFacingStep` row/turn +
  `di%3` drift while sailing + `ship_try_move` 0x01FE: "Docked!" (auto-FURL, without stepping on the
  dock 0x0306) / "COLLISION!"/"BREAKING UP!" with `rand(1,30)` damage / cactus "OUCH!"
  `rand(1,8)` / "Blocked!") with `navalStepCost` (HMS Cape) and the wind tick in order;
  the broadside runs the world-turn (wind tick) BEFORE the `rand(1,20)` (0x0A5E). The
  **player's sinking** (`sinkPlayerShip`, damage_ship 0x10D6) is wired:
  "Ship sunk!" + skiff > carpet > drowning ("DROWNING!!!") conversion. Journey E2E:
  `game/e2e/naval.spec.ts` (hoist/sail/fire/disembark). **Sub-detail (a) —
  ORGANIC acquisition of the ship — ✅ CLOSED in F1.5**: the live Shipwright
  (`shop.ts` branch 0x84 → `buyShip` → `game.spawnDockShip(dockX,dockY,flags,0)`) places
  the ship as a world object (g_world_objects 0x5C5A) at the OVERWORLD dock; the
  Board→X-it→re-dock cycle keeps it with its live hull/skiffs (persistent). The phantom hook
  `__u5test.boardTile`/`?ttile` **was REMOVED** from `main.ts`: naval.spec now
  boards a ship-object seeded by `__u5test.addWorldObject` (same assertions,
  real object→Board path). E2E of the complete cycle: `game/e2e/objects.spec.ts`.
  **Sub-detail (b) — drowning** (party-wipe/HP/teleport, 3 opaque helpers) → oracle §3.
  **Resolved in F1.2** (no longer open): the **initial hull of the pirate ship** (obj+5
  = 0x64=100, MAINOUT spawn 0x1050, seeded at the spawn); the **hull at sinking** (the
  binary does NOT touch it — the `sub g_hull` is only from the survival branch — and the clone
  leaves it intact, faithful); and the **world_turn↔naval displacement order** (Concern C of
  T2), aligned ONLY WHILE SAILING (drift: the binary runs the world_turn 0x069D before
  outdoor_move 0x0BB3 applies the step — monsters before the ship); **ROWING
  keeps its post-move order** (it goes via outdoor_move like on foot: move_party 0x0354 and
  THEN world_turn 0x0539). (FIDELITY §Transport.)
- **Shrine meditation (mantra) and wishing well** — ✅ **CLOSED in Phase 1.4.**
  The ON-STEP trigger of the live shrine (tile 0x19) / Codex (0x11) / restore (0x1a) goes via
  `game.ts checkShrineEntry` (sibling of `checkMoongate`) → prompt `shrine-meditate-prompt`
  (yesno-esc) / chain `shrine-restore-prompt` (virtue + mantra×3 by `selector.prompt`); the
  well triggers ON-LOOK (`look`→`well-drop-prompt`→`well-wish-prompt`) with the horse mountable
  by (B)oard. Ceremony resolved in `meditate`/`submitShrineRestore`/`dropCoin`/`makeWish`.
  Tests: `shrine-trigger.test.ts` + `wishing-well.test.ts` + `e2e/shrines.spec.ts` (6 journeys).
  Residual: placeholder strings, meditate ESC, minutes (0), horse coord → Class C §3
  F1.4 (below); final ceremony of the 8 virtues → endgame F1.10. (FIDELITY §Shrines.)
- **Chest-objects and item switch** (SJOG 0x112C/0x1458): ✅ **WIRED in F1.5**
  over the `worldObjects` layer (g_world_objects 0x5C5A). `open()` over an object
  `kind:"chest"` runs trap (`chestTrap`)→loot (`chestLoot`, `applyLootGrant`)→theft-karma
  in town (−2/=0)→removes the object, with the RNG of the **live stream** (`this.rand`).
  `get()` over `kind:"torch"` (sconce branch of get_special_item 0x148c) raises torches (cap
  0x63) and removes the torch. Tests: `chest-object.test.ts`, `torch-object.test.ts`,
  `e2e/objects.spec.ts`. **Residual (oracle §3)**: initial seeding of town chests
  (O5, not derived → today by test hook); complete jump-table of get_special_item (O3,
  only the torch branch ported); id→index mapping of the quantity-array in `applyLootGrant`
  (O-loot); slot+6/+1 (O2). (FIDELITY §Commands.)
- **Ready-RNG** (ZSTATS) — ✅ **CLOSED in F1.6**: `game.readyItem(charIdx,id)` invokes
  `equipItem` with `randRange:this.rand`, so the "Ring vanishes!" 1/16 (0x0e11
  `rand(0,15)==0`, only after placing a ring id 0x2a/0x2c) runs over the **live stream**
  and now fires in play (before `main.ts` did not pass `randRange` → fixed roll=1, never
  vanished). The rand is consumed ONCE, only on successful equip of 0x2a/0x2c: not on
  toggle-off, failed equip nor other items (derived from the asm, citation §Ready below). Ready is a
  **free action** (ZSTATS.OVL does not touch g_unk_24e6 → does not charge a turn). Tests:
  `game/tests/ready-rng.test.ts` (derived sequence vs `next(0,15)`, exact stream
  count, non-consumption, free turn). (FIDELITY §Endgame.)
- **trapCheck** (SJOG): trapCheck is a hook. Closure: pass the stream to the traps.
  (FIDELITY §Endgame.)
- **(V)iew a gem** (kernel dispatcher 0x341A) — ✅ **WIRED in F2-View (task #2)**, the
  LAST gap of the command matrix (`scout-dispatch.md`). `game.view()` traces byte by
  byte case V (ULTIMA.EXE.asm 0x341A-0x344D): it prints **"View a gem!\n"** (DS 0xa258,
  0x341e) ALWAYS before the gate → `cmp [g_gems],0; je` (0x3421) → **"You have none!\n"**
  (DS 0xa266, 0x344a) WITHOUT opening the view → `dec [g_gems]` (0x3428, consumes **exactly 1**,
  BEFORE painting) → `cmp [g_location],0x21; jae` (0x342c) dungeon branch (DNGLOOK 0x06a8,
  8×8 floor) vs rest (LOOKOBJ gem_view 0x10fc, 32×32 window). **[bp-2]=1 in ALL
  branches** (prologue 0x317e, no V branch writes it) → **charges 1 standard turn of the
  context** (runContextTurn / advanceTurn in dungeon). **Bug-for-bug ported**: pressing V
  without gems ALSO charges a turn (0x344a→0x33ea→0x31ee returns the default 1). Both strings
  are **[V]** byte-exact (DATA.OVL, fileoff=DS+0x10). The view's descriptor is produced by
  `buildGemView` (pure core): overworld = 32×32 window centered on the player read with
  toroidal wrap (`g_chunk_origin=party−16` approx. of the centered VMAP), town = whole 32×32
  map (chunk_origin=(0,0)), dungeon = 8×8 floor. **Unlike the minimap (QoL, with fog)
  the view reveals the WHOLE window** — faithful to the `gem_view` that reads all the tiles without
  exploration gate. Tests: `game/tests/view.test.ts` (gate/consumption/map-per-environment/turn) +
  `game/e2e/viewgem-overworld.spec.ts` + `game/e2e/viewgem-autorepeat.spec.ts` (opens/closes by key, the grid on the faithful canvas and under the shader skin, and keyboard auto-repeat). **L3 declared**: the pixel-exact RENDER
  (category `byte[tile+0x1d1a]`→jump-table `draw_gem_map_tile` 0xf7e, invoked
  at the call-site 0x1152 of the double loop) is
  UI-phase; `ui/viewgem.ts` paints 1 cell/tile with the palette (faithful-enough) and closes with
  any key (polling 0x11b6). **Turn order (review fix)**: `game.view()` DEFERS the
  turn to `afterGemView()` (the UI invokes it upon closing the panel) because in the binary
  `gem_view` (call 0x343d) is blocking and returns only upon pressing a key; the context
  loop charges the turn AFTER the return (epilogue 0x31ee) → ambush/spawn starts
  upon closing the view, not with the panel open. Without gems the turn is immediate (there is no
  view to close). The MECHANIC (gem consumed, environment map, closing,
  turn) is exact. **(NOT of View, cataloged separately §3/§4)**: the "looking at the SUN damages"
  ✅ PORTED (F1.9, apply_damage(active,1) in Game.look()); the crystal ball 0x29 and its
  `roster+0x0e` (de-oracled = INT, previously 0x55b6) of the (L)ook at the gem remain Class D
  (it is of the View-gem, UI layer). (`re/notes/lookobj.md §gem_view`, `re/disasm/ULTIMA.EXE.asm
  0x341A`, `re/disasm/LOOKOBJ.OVL.asm 0x10fc`.)
- **Capture/refuge/arrest and guards** (Blackthorn): ✅ WIRED to the town-turn
  (F1.7-T1..T5): refuge (death-check), capture+interrogation, password, drain, and the
  **guard-adjacency trigger** (T5, adjacency NECESSARY derived + sufficiency
  Class C, §password row). The town `arrest` (non-Palace guards that react to
  a crime) is still not wired. The `guard_wander` of tile-under 0x10/0x11 OBJECTS
  (TOWN 0x0C78, `guards.ts`) is a pure engine not wired game-wide (the Palace DOES have
  those objects —stable horses, slots 2-4 type 16/17— but it is a system DISTINCT
  from the palace guards NPC type 0x70 and does not affect the capture). (FIDELITY §Blackthorn.)
- **Falsehood's post-purchase drain** (SHOPPES 0x019A): ✅ **WIRED in F1.7-T4**
  (piece E). The pure engine (`postPurchaseGoldDrain` + `shadowlordPresentIndex`) is
  wrapped in `shops.ts:postPurchaseDrain(state, rand)` — gate FIRST (0x019a-0x019f
  `cmp [g_shadowlord_here_idx],0 ; jne ret`): only if the present Shadowlord is
  Falsehood (index 0) is `rand(1,64)` rolled (0x01a1-0x01ad, push 1/0x40) and `gold -= roll`
  with floor 0 (0x01b1 = kernel 0x3f54 sub_word_floored). `game.shopPostPurchaseDrain()`
  injects the live stream (`this.rand` → liveRng); the UI (`ui/shop.ts:drainOnPurchase`)
  calls it after each successful PURCHASE of the SHOPPES.OVL group. **Scope derived from the asm**:
  the binary invokes the drain after each `sub [g_gold],ax` of SHOPPES.OVL — 5 sites, all
  `sub [gold];call 0x19a`: guild (0x037b), reagents (0x0623), transport (0x0951),
  smith-buy (0x0ab1), healer (0x14ed). **NOT** on the sale (gold intake) nor in
  SHOPPES2 (tavern/shipyard) / SHOPPES3 (inn), whose `sub [gold]` are followed by
  kernel 0x9dfa, not by the drain. Consumes 1 rand/purchase from the live stream ONLY with
  Falsehood present (0 rands otherwise: the gate cuts before the rand). **Silent**:
  the asm skims without `kernel_print` → there is no message (verbatim nor ⚠). Tests:
  `game/tests/shops.test.ts` (pure: gate/count/floor with spy rand) +
  `game/tests/merma-live.test.ts` (5: live stream, seed advances EXACTLY 1 rand only with
  Falsehood, purchase+drain flow, floor 0). **Parity intact**: the harness `shops-run.ts` does not
  invoke the drain and no scenario of `re/parity/shops/` places a Shadowlord → the 240
  PURE scenarios byte-identical (stream untouched). **Declared debt**: the panel
  `Barkeeper` (synthetic food+torches) is NOT wired — it mixes tavern-ration (SHOPPES2,
  no drain) with guild-torch (SHOPPES.OVL, with drain); the faithful split is future.
- **Blackthorn Palace guard password** (TALK 0x01e2/0x02a4, NPC
  dialogNum 0xFF): ✅ **WIRED in F1.7-T3** (piece C, the escape route from the T2
  capture loop). When talking (T) to a guard (dialogNumber >= 0xFD, tile 0x70) in
  loc 0x12, `game.tryTalkGuard` emits the challenge verbatim `"Give now the password,
  bearer of the Badge!"` (DS 0x90fc) + "Your response?" (DS 0x9128) as a text
  prompt (Words of Power pattern); `game.submitGuardPassword` runs the pure engine
  `guardDemand` (CASE-INSENSITIVE strcmp over the **FIRST 4 chars** against "IMPE"
  DS:0x4A9A: 0x02dc `mov byte[bp-0xc],0` NUL-terminates the input at buf[4] BEFORE the
  strcmp 0x02e0 → "IMPER"/"IMPERIAL"/"IMPE " PASS, "IMP"/"XMPE" fail; **bug-for-bug,
  NOT exact equality** — corrected in the review fix, both the `blackthorn.ts` engine
  and the `blackthorn_parity.py` model had a consistent-but-wrong `[:14]`, a
  false-green that the reviewer's external re-derivation exposed). Hit → "Pass,
  friend!" (DS 0x913a) + marks `state.blackthornPassGranted`, which
  **`checkBlackthornCapture` excludes** → no re-capture. The guards (type 112) already
  live in `npcs.json` loc 18 (slots 8-15), without dependency on F1.5. Tests:
  `game/tests/password-live.test.ts` (11) + parity scenario `guard-password-prefix`
  (model↔clone). **Class C declared** (§3, row "Blackthorn password gate/pass"):
  the prior gate `g_time_spell==0x1d` (0x587a normally 'Q'/'T'/0 → anomalous sentinel =
  Black Badge equipped) remains ⚠️ oracle; and **the password→capture connection is NOT
  derived from the asm** (the handler 0x02a4 and the gate 0x12ae do not share a known helper nor
  flag — the binary is not known to link the two): `blackthornPassGranted` is a
  **declared CONSERVATIVE addition of the clone** (not an opaque mechanism), persists in the
  save. **TRIGGER by ADJACENCY wired in F1.7-T5** (replaces the approximate per-turn one
  of T2): the trigger is NO LONGER "each turn in loc 0x12" but the **attack of an
  ADJACENT guard**. `npc_engine` (TOWN 0x1352, gate `[0x65bf]≠0 ∨ result==2`) invokes 0x12ae.
  **ADJACENCY NECESSARY ✅ DERIVED**: the active NPC's idx `[0x65bf]` is only set by the
  fast-path of `npc_target_for_attack` (NPC.OVL 0x06E4, 0x0723 `cmp ax,1`, write
  0x074e) — `[0x65bf]` has exactly 2 writes in NPC.OVL (0x074e =idx, 0x0dc6 =0)
  and ZERO in TOWN.OVL; loop (b) does not touch it. Without an adjacent guard there is no capture. The clone:
  `blackthornGuardCaptureTriggers`/`palaceGuardAdjacent` (loc 0x12 + conscious>=0 + NPC
  type 0x70 at manhattan==1), fed by `Game.palaceGuards()`.
  **⚠️ HONESTY CORRECTION (T5 review fix) — sufficiency = Class C APPROXIMATION, NOT
  0x13a4**: my 1st pass cited branch 0x13a4 (`g_npc_attack_tile==0x61` → tile 0x70), but
  that branch requires aiType 6/7 and **the real palace guards are aiType 0/4** (npcs.json loc
  18 slots 8-15, dialog 255) → their fast-path sets marker **0x74**, NOT 0x61 → they take the
  **`npc_engine` 0x13b4** branch (`cmp [bp+4],0 ; jne 0x13d6`, `[bp+4]=result−1`). There the
  merchant guard's capture depends on **`result==2`** (town command dispatcher,
  not isolated) **or** on the ~~0x1912 OPAQUE~~ handler [⚠ 07-30 RESOLVED: TALK 0x031E
  `talk_converse_dispatch`, read IN FULL — re/notes/talk-031e-resolucion.md. Without
  the badge the guard_demand 0x1e2 returns ret 1 → capture: adjacency SUFFICES in the
  no-badge case and the clone is CORRECT; with the badge worn the binary issues the
  password challenge AT interception time (card T-A) and writes no persistent pass
  (card T-B)]. The clone fires by **adjacency alone** = matches the no-badge case.
  ~~Class C row; DOSBox BP to close~~ (the BP lost its target: the internal gate does
  not exist; the `result==2` leg remains unisolated — card T-C). The
  **loop (b)** of 0x06E4 (`rand(0,0x3f)` threshold 0x10, 0x083d) is chase MOVEMENT that
  **does NOT set a marker nor `[0x65bf]`** → does NOT fire capture; its rand is from the movement
  stream, not the trigger → separate **chase Class C** (`manager.ts:fleeStep`).
  Observable consequence: after the deposit at (10,7) no guard remains adjacent → there is NO
  per-turn re-capture; the player **walks up to a guard, gives "IMPE" and leaves**
  (reachability green `e2e/blackthorn.spec.ts`: capture-by-adjacency → deposit → no
  re-capture → Talk IMPE → "Pass, friend!" → free; + the inverse). The **pass avoids the
  capture even if you remain adjacent** (`blackthornPassGranted` cuts before the gate —
  declared conservative). **"guardWander" is NOT a proper piece of the palace guards**: they are
  type 0x70 aiType 0/4 NPCs that already wander via the standard NPC dispatch (npc.md §4.0);
  the `guard_wander` TOWN 0x0C78 (`world/loops/guards.ts`) is the sway of tile-under
  0x10/0x11 OBJECTS (`tile & 0xFE == 0x10`), a DISTINCT system — the Palace DOES have
  those objects (**stable horses**, slots 2-4 type 16/17) but `guard_wander` remains
  not wired game-wide and does not affect the capture (scout-blackthorn piece B conflated
  the two). The Minoc guards (half-gold) and tribute (same TALK 0xFF table, Y/N prompts)
  remain not wired — separate pieces.
- **Canceling the getdir of the town (K)limb charges 1 turn; "What?" does NOT** — ✅
  **CLOSED in fix #50** (ONLY klimb; the other 7 directionals → Class C §3). In
  town_klimb (TOWN 0x0B82) the return is a flag `[bp-2]` 0/1: upon **canceling** the getdir
  (0x0C3E) it marks `[bp-2]=1` → the town loop advances 1 min; with an **invalid target**
  ("Klimb-What?", 0x0C38 → `[bp-2]=0`) it does NOT charge. It is an asymmetry VALIDATED byte by
  byte (the only one of the 3 return conventions that is indeed a turn flag). Ported in
  `game.ts` (`klimbCancel()` = `runContextTurn({consumed:true})`) and invoked from the
  cancel branch of `main.ts` ONLY for `pendingDirCommand==="klimb"` (the other 7
  keep "Cancelled." without a turn). Tests: `tests/town-stairs.test.ts` (cancel +1 min,
  "What?" 0 min) + `e2e/town-stairs.spec.ts`. ⚠ **Key-policy nuance declared**:
  the clone cancels a directional command with **any non-arrow key**, but **Escape
  does NOT** (the panel-close handler intercepts it first, main.ts, for the 8
  commands — pre-existing, out of scope of #50); the binary's getdir returns 0 on
  cancel but its exact key set (ESC only? non-arrow?) lives in the resident
  kernel not dumped. Citation: `re/notes/town-klimb.md`, TOWN.OVL.asm:1191-1228.
- **Interactive Y/N prompts** — Flow 3 (shrine donation): ✅ **CLOSED in Phase 1.4.**
  The function + digit prompt + loop re-entry were already ready from F1.3; the **meditation
  TRIGGER** that was missing was wired by F1.4 (`checkShrineEntry`→`meditate`, mode `donation` →
  `shrine-donate-prompt`), so the donation **is now playable end-to-end** (E2E in
  `shrines.spec.ts` "donation"). (Flow 1 town exit and Flow 2 troll toll already CLOSED
  — see below.)
- **Town exit by the edge** (`kernel-survival.md §5.1`, TOWN:0x600,
  verified against DOSBox): ✅ **CLOSED in Phase 1.3 (Flow 1, commit `386c171`).**
  The original marks the edge (y<1/y>30/x>30/x<1) and asks **"Dost thou wish to
  leave?"** (DS 0x2690) with a RAW getkey (0xa49c, does not consume the live stream) —
  'Y' exits to the overworld **without consuming a minute** (the loop does not call advance_clock
  on exit, VERIFIED DOSBox), 'N'/ESC **charge 1 minute** (TOWN:0x15D4). Now
  `move()` intercepts the `exitedMap` branch and emits `town-exit-prompt` (pauses, no longer
  auto-exits); `game.confirmTownExit(yes)` resolves Y (→`exitToOverworld`, strings
  "Yes"/"Exit to Britannia!" DS 0x26ab/0x26c6, 0 min) or N/ESC ("No" DS 0x26d2 +
  town turn 1 min). The key capture lives in the `pendingPrompt` machine
  (type `yesno-esc`) of `main.ts`. Tests: `game/tests/prompts-town-exit.test.ts`
  (engine, with direct test of 0-RNG by live seed), `game/e2e/movement.spec.ts`
  (the `test.fixme` was removed and is now a green Y/N/ESC test) and `game/e2e/prompts.spec.ts`
  (Y/N/ESC + invalid key = re-reads). **Open sub-detail** (Class C, §3): the
  **Underworld** destination (loc 0x19) uses tables 0x1e89/0x1eb1 not yet modeled → today
  every exit goes to Britannia (floor 0). **Fast-follow** (§2 below, task #45): ✅
  CLOSED — the edge passability gate (TOWN:0x788) and the panel guard
  journal/minimap (pre-existing, not introduced by 386c171) are already resolved.
- **Troll bridge toll** (`loops.md §1.4`, MAINOUT:0x1B3E/0x1BE8,
  `scout-prompts.md` Flow 2): ✅ **CLOSED in Phase 1.3 (Flow 2, commit `e81c6e6`).**
  The original ambushes upon stepping on a bridge (tile 0x6A/0x6B) on foot with the gate
  rand(0,7)==0 (`bridge_troll_ambush` 0x1BE8, internal wind tick + rand(1,30)
  per conscious member up to the 1st DEX<roll) and asks **"The trolls demand a
  <N> gp toll! / Dost thou pay?"** (0x6b2c+0x6b4a, N=99−3·STR of the 1st conscious)
  with a RAW getkey (0x1b86, does not consume the live stream) that accepts ONLY Y/N and
  **IGNORES ESC** (⚠ unlike Flow 1, where ESC=N). The prompt jumps in the MIDDLE of the
  outdoor turn (after advance_clock(2)+ambush, BEFORE the final world_turn 0xD11).
  Now `runContextTurn` no longer auto-pays: it sets `pendingTroll` and emits
  `troll-toll-prompt{toll}`, pausing the turn queue. `game.resolveTrollToll(pay)`
  resolves — Y with gold≥toll → charges (0x1ba9) and RESUMES the EXACT deferred queue of the
  turn (`tickDoorsAndNpcs`+`outdoorWorldTurn(underParty)`) WITHOUT spawn; Y with gold<toll
  → REFUNDS (0x1bb9) and falls to spawn; N or cannot-pay → `spawnTrollCombat` (troll
  `defIndex 41`='Troll', table of 48 = `monsterNamesUpper[41]='TROLLS'`) which REPLACES
  the turn queue (0xb714→0xdf80). The key capture lives in the
  `pendingPrompt` machine (type `yesno`, ESC ignored) of `main.ts`. **0-RNG verified**:
  `re:parity:all` (239) was left intact — the raw getkey does not alter the live stream.
  Tests: `game/tests/prompts-troll.test.ts` (6 cases: pause+toll, pay/reject/
  refund, cite defIndex, 0-RNG by live seed) and `game/e2e/prompts.spec.ts`
  (Flow 2: ESC ignored + Y charges 54 gp + N → `*** COMBAT! ***`). **Open
  sub-detail** (Class C, §3): the troll spawn in the reject branch is assumed 0-RNG
  (bodies 0xb714/0xb8a4/0xdf80 outside the repo's disassembled overlays).
- **Shrine donation** (`shrines.md:159`, CAST2:0x0B1D, `scout-prompts.md`
  Flow 3): ⚠️ **PARTIAL in Phase 1.3 (Flow 3) — function + prompt + re-entry ready;
  meditation TRIGGER → F1.4.** The original, upon meditating with the Codex and without an active
  quest, asks for a **DIGIT** with a RAW getkey (0x448c, does not consume the live stream):
  n≤0 → NO-OP (" gp", DS 0x959c); 100·n>gold → "not enough" (0xb6b9) + **RE-ASKS**
  (the loop 0xb5f→0xb63 re-requests a digit without charging); if it reaches → `100·n` gold for
  `+n` karma (clamp 99). Now `game.submitDonation(n)` resolves the three branches
  (delegating the rule to the pure `shrineDonate` of `world/shrines.ts`, intact) and
  **re-emits `shrine-donate-prompt`** in the insufficient case; `main.ts` wires the
  branch in `applyEvents` with the `pendingPrompt` capture type `digit` (only '0'-'9', the
  digit is embedded in the result text; the resolve re-enters
  `submitDonation` and the re-emit re-arms the same prompt = the original's loop).
  **0-RNG verified by live seed** in the unit. **NOT playable end-to-end yet**: the
  TRIGGER that launches the prompt (meditation with Codex) is wired in **F1.4** — without it,
  `submitDonation`/`shrine-donate-prompt` have no one to fire them. Tests:
  `game/tests/prompts-donation.test.ts` (6 cases: NO-OP, insufficient→re-ask,
  valid donation, exact gold limit, clamp 99, 0-RNG by live seed). **Without
  E2E** (declared frontier of the plan: there is no trigger until F1.4). **Open
  sub-detail** (Class C, §3): the local CAST2 strings (prompt 0xb692, "not enough"
  0xb6b9) remain placeholders ⚠ without derived text (pre-existing RE gap).
- **Fast-follow of the town exit (F1.3, task #45)** — ✅ **CLOSED** (fix
  #45). 2 **pre-existing** divergences exposed when wiring Flow 1 (none
  introduced by `386c171`; both in code prior to the task), both resolved:
  1. **Edge passability gate** (TOWN:0x788) — ✅ implemented. The
     original validates the destination tile BEFORE opening the prompt: 0x785 pushes the
     destination tile, 0x788 `call kernel_tile_passable(transport, tile)`;
     `or ax,ax; jne 0x792` → only if it is passable does it reach the exit flag `[bp-6]`
     and open "Dost thou wish to leave?"; if not, `jmp 0x83a` = "Blocked!" (DS 0x26d6)
     and the TOWN loop charges 1 min (0x15D4). **Key finding**: the "destination tile"
     upon stepping outside the 32×32 is the **viewport filler** (0x678 reads the neighbor
     of the viewport centered on the party). That filler, per Ultima5Redux
     `SmallMap.GetOutOfBoundsSprite`, is **Grass (5) for every normal town**
     (Sin Vraal=Desert 7, Sutek/Grendel=Swamp 4, Stonegate=Hills 11) — **all
     passable on foot**. Consequence: the door **never blocks an exit on
     foot** (the only possible transport in a town), so the fix is **structural
     fidelity without observable change on foot** — that is why it was Class B and not a live
     bug. Implemented in `world/movement.ts` (`TOWN_EDGE_FILLER_TILE=Grass`,
     gate in the OOB branch of `tryMove` **and** `resolveStep`: "Blocked!" + 1 min if the
     filler is not passable for the transport, exit if it is). Tests:
     `tests/world.test.ts` (gate exercised with transport=ship, for which Grass
     is impassable). **Residue CLOSED** (task #70): the EXACT identity of the filler
     was DERIVED from the kernel `0x4402` — it is **cell (31,31)** of the small map (fixed pointer
     `DS:0x6A07`), not a constant; the "3 exceptions" of Ultima5Redux are just the
     (31,31) of those maps. The RENDER already uses the real tile (`edgeFillTile`); the
     MOVEMENT gate keeps Grass as a behaviorally identical approximation on foot (see §3,
     row "Viewport filler tile per location").
  2. **journal/minimap panels do not block movement** (`main.ts`) — ✅ resolved.
     The panel guard (dialogue/save/shop/selector + the explicit ztats one) did NOT
     include `journalPanel` nor `minimapPanel`; a movement key could reach
     `game.move()` with those panels visible and fire `town-exit-prompt` with the
     panel overlaid (the minimap leaves the focus on `body`, so the arrow
     reached the global handler). Fix: two guards in `main.ts` mirroring the ztats one
     (`journalPanel.visible && key!=='F6'` / `minimapPanel.visible && key!=='Tab'`).
     E2E test: `e2e/panels.spec.ts` (with the panel open, ArrowLeft at the edge of
     Iolo's Hut does not move nor emit the prompt).
- **Ammunition consumption in combat** — ✅ **CLOSED in fix #51** (port commit; Class B
  reclassified after the #44 review and wired). The state already existed and the rule was
  derived; only the decrement on firing was missing, now hooked.
  - **State (already present)**: `equipmentQuantities` (array of 48 slots = party's shared
    inventory, `state.ts:65/184`) holds arrows/quarrels (Arrows 0x1b, Quarrels
    0x1d — non-equipable ammunition, `equip.ts:64/66`); Combat sees the state (`opts.state`). The
    equip ammo-gate (`equip.ts:206-211`, citation **0x0d0c**) already required Arrows/Quarrels > 0
    to equip Bow/Magic Bow/Crossbow → the weapon↔ammo mapping was already modeled.
  - **Derived and implemented rule** (COMSUBS:0x097C): **PER-SHOT** decrement — the binary
    does it at 0x0B3D, BEFORE resolving the strike at 0x0B51, so the ammunition is spent
    whether it hits or misses (NOT per hit). Bow 0x1a / magic bow 0x24 → Arrows; crossbow 0x1c →
    Quarrels; upon reaching 0 they UNEQUIP the weapon and RETURN it to the pack (09a2-09ab, `add
    equip_qty[weaponId]`). ⚠ **AMENDED 2026-08-06 (card #36)**: that "unequip" was ambiguous and
    the port read it as "the shooter, +1". The `call` at 09a2 goes to **SJOG.OVL:0x1b34** (via
    stub CS 0x800a), which sweeps `si < g_party_size` calling `unequip_item` and returns the
    COUNT; the `add` at 09ab adds **that N**, and it is an `add byte` **with no cap**. That is:
    it **disarms the ENTIRE PARTY** and returns N. All three halves (scope, amount, cap) are
    cloned as of #36; derivation in `re/notes/municion-36-acta.md`.
    Throwing weapons {Dagger 0x10, Spear 0x15, Throwing Axe 0x16} at
    dist > 1 (09b8): spend 1 of themselves from the inventory; the last is thrown and LOST
    (09ce, without returning). **The throwables do NOT underflow**: their branch 0x9b8 checks
    `[equip_qty+weaponId] == 0` (0x09c1 `cmp`+`je 0x9ce`) and unequips BEFORE decrementing, so
    it never drops below 0. Sling 0x11 does not consume; Flaming Oil 0x13 is consumed ELSEWHERE in the
    shot (0x0ACE) — both outside 0x097C, not modeled here.
  - **Wiring**: `combat.ts` `attackWith` → `consumeAmmo` (only `cur.kind === "player"`;
    the AI uses `strike` directly, never `attackWith` → the `combat_parity` traces do not
    change, verified 239 green). Reuses `equip.ts`: `ammoItemFor`, `isThrownWeapon`,
    `unequipWeaponById` (throwables) and `unequipItemById` (the #36 sweep — it is the clone of
    0x6e60, the real callee). Tests: `combat.test.ts` (13: decrement per shot bow/crossbow,
    unequip+return at 0, throwable with/without reserves, dist 1 without consumption, AI does not touch
    the pack, clamp with reserve at 0, a shot that MISSES the roll spending an arrow anyway, and
    the FOUR from #36 — party-wide sweep with N, sweep by weapon id, `add` with no cap, and the
    CONTROL that the throwable branch stays actor-only)
    + e2e `combat-ranged.spec.ts` (arrows drop from 10 to 9).
  - ✅ **BUG PORTED by contract (#18, FIRST application of the bug-for-bug policy of
    docs/FIDELITY-CONTRACT.md)**. REAL mechanism — it does not occur in a single PC's turn: the three
    bows are TWO-HANDED (`TYPE_TABLE[0x1a]=[0x1c]=[0x24]=0x30`, equip.ts:48-52), so a PC
    wields at most ONE bow and fires ONCE per turn (the triple strike cannot slip in two
    bow shots). It IS reachable BETWEEN PCs sharing the pool: the ammo-gate 0x0d0c only
    blocks EQUIPPING with 0, not FIRING. 🔴 **CORRECTED 2026-08-06 (card #36) — the path published
    here DOES NOT EXIST**: it said "if PC A empties the pool and PC B fires ITS OWN BOW afterward",
    which presupposed the unequip reached only the shooter. It does not: the sweep at 09a2 disarms
    every member carrying **that same weapon id**, so if B was also wielding a Bow 0x1a, A's shot
    already took it away. The path that DOES remain is narrower and relies on **distinct ids over
    a shared pool**: **Bow 0x1a and Magic Bow 0x24 share Arrows 0x1b** but are distinct items, so
    the sweep for 0x1a does not touch the 0x24. If A empties the pool with the Bow and B fires its
    Magic Bow AFTERWARD, the binary does `dec` over 0 →
    **underflow to 255** (0x099c `dec`+`jne`, and does NOT unequip B's weapon) → the party gains 255 arrows
    free (original bug). The Crossbow does not qualify: its pool (Quarrels 0x1d) is a different one.
    The bug is STILL PORTED and still reachable; what changes is the route, and the test
    "#36 el barrido es por ID DE ARMA…" in `combat.test.ts` pins exactly that route. **The "tacit fix" REVERTED**: `consumeAmmo` (combat.ts) NO LONGER
    clamps to 0 nor disarms B's bow; it now decrements with `dec` u8 with wrap (`(qty−1) & 0xff` →
    0−1 = 255) and only unequips when the result is EXACTLY 0 (the binary's `jne` does not fall to
    the unequip with 255). The fidelity contract required this reversion (§bug-for-bug policy: "the
    arrow underflow IS PORTED"; the tacit "fixes" are divergences to REVERT). Test:
    `combat.test.ts` ("2nd shot with the reserve at 0 UNDERFLOWs to 255…").

**How to close it.** It is the natural continuation of the clone as a playable product: a
UI integration step per command, each guided by its already-verified pure function.
It does not require returning to the binary.

---

## 3. Class C — oracle questions (unmeasured values of the original)

**What.** Concrete values of the original that the clone fixes with a derived source
(Redux, design formula, or partial asm) but that **have not yet been read live**
from DOSBox. Here there IS a pending measurement, not just a wiring.

**Why it is deliberate.** Each one requires a new breakpoint or a specific
screenshot/palette capture; the measurements that anchor game rules were prioritized
(the 6 of COVERAGE §4). These are of second order.

| Question | Current clone value | How to measure it |
|---|---|---|
| **Open-door duration** (Class C, C1) | 4 turns (DoorManager `DOOR_OPEN_TURNS`) — **PARTIAL DERIVED (task #15)**: the original uses `[0x5952]=4` decremented 1/town turn (TOWN 0x15ef); confirming the exact phase of the decrement relative to the Open turn (3/4/5?) is missing | BP at `TOWN.OVL 0x15f6` (dec `[0x5952]`) + restore `0x39cc`; count complete turns with 0x44 before returning to 0xB8 |
| **Open-door tile** | BrickFloor 68 (0x44) — **CONFIRMED by asm (task #15)**: SJOG cmd_open 0x1417 writes `map[x,y]=0x44` | (derived, no longer requires BP) |
| **(U)se Skull Key — mechanism/guard/strings** (ex-Class C, C2) | ✅ **RESOLVED 2026-07-14 (task #22)** — see 🐛→✅ above. `0x18c4` is the **Skull Key branch of the (U)se item command**, ported in `game.ts::useSkullKey` (dec 0x18c4 → "Skull Key" DS 0x48fe → gate g_location → demagifies 0x97→0xB8 / 0x98→0xBA). **Underflow guard resolved by picker design**: (U)se only lists items with count≥1 (ZSTATS nav `find_next/prev_owned` 0x05a4/0x056c skips qty 0), so the `dec` never underflows. **Byte-exact strings** (DATA.OVL): "Skull Key" (0x48fe), "Not here!" (0x4909), "No usable items!" (0x489f). 🔴 **RETRACTED 2026-08-07**: this cell used to read *"In Ex Por #26 = getdir + animation (does not touch doors, CAST:0x1026)"* — **false. In Ex Por DOES open magic doors**: the arm at `CAST:0x1026` calls `CAST2.OVL:0x0768` through stub `0x80ee`, the **same worker** the Skull Key uses from `0x18dd` — one worker, **two callers**. See point 2 of the 🐛→✅ entry above. **Everything else in this cell stands, including the underflow-guard resolution**, which was independently re-derived from `ZSTATS 0x05ba`. An Ex Por #25 = real seal (CAST:0x846) — unaffected. Remains **[C] residual**: string "Locked!" of the An Ex Por sealing (not transcribed). | (mechanism derived by asm; runtime DOSBox anchor optional: BP `CAST.OVL 0x18c4` → (U)se→Skull Key facing 0x97 → `g_skull_keys−1` and `0x97→0xB8`, already covered by asm + clone tests/e2e) |
| **Reagent price curve by karma** | `base·(1+(100−karma)/100)` real division | BP in the price calculation with karma seeded |
| **[0x5891]** (wind gate) | rolls per consumed turn | BP at 0x2F62 + counter per turn; decides the town model |
| **BP pick_virtue 0x9a6** | seed 0 assumed on the 1st pick | BP at 0x9a6 + read g_rng_seed (title→quiz link) |
| **BP shops 0x02D8** | INT-based haggling (model) | BP at 0x02D8 with INT seeded (runtime price by INT) |
| **NPC array seeding** | schedule/wander by harness | populate the small-map NPC array in RAM (npc.md §14) |
| **In Quas Xen / In Quas Wis** (effect) | derived with low confidence (scout) | BP in the Cast dispatcher (CAST 0x0dba) upon casting them + observe the effect/state written in RAM; collate against `magic.md` |
| **Dialogue fallback texts** | taken from Redux | read the exact strings from DATA.OVL / screen |
| **Canonical EGA palette** | derived (not Redux's) | DOSBox screenshot and color sampling |
| **Revive status byte** (0xdc66), **g_floor of the capture deposit** | assumed canonical. The capture deposit (F1.7-T2, `finishCaptureDeposit`) lands on **floor 0** as safe observable behavior (the asm 0x08e7 leaves `g_floor=0xff` sentinel and 0x08f6 does NOT re-set it) | BP at the revive/deposit with a high floor: read the real floor after the capture |
| **Blackthorn password gate/pass** (F1.7-T3, TALK 0x02a4) | the prior gate `g_time_spell==0x1d` (0x587a, normally 'Q'/'T'/0 → anomalous sentinel = Black Badge, confirmed by the string "bearer of the Badge!") **is NOT modeled as a pre-condition** (gating it without being able to track the Badge would make the escape unreachable). **The password→capture connection is NOT derived from the asm**: the password handler (0x02a4) and the capture gate (0x12ae) do not share a known helper nor flag — the binary **is not known to link the two**. `blackthornPassGranted` is a **declared CONSERVATIVE addition of the clone** (not an "opaque mechanism": we do not claim there is a link we cannot read, but that the clone ADDS it so the escape exists), persists in the save | BP at TALK 0x02a4: read 0x587a upon entering the Palace with/without Badge; trace whether the "Pass, friend!" leaves any state that the capture gate consults after a turn (or confirm that there is NO such link and the real escape is the guard-attack trigger of T5) |
| **Player drowning — animation (damage_ship 0x1120)** | tile 0 + "DROWNING!!!", without party-wipe/HP/teleport (opaque helpers) | BP at `0xffffc1de` (4 args): does the drowning animation consume RNG? |
| **Player drowning — consequence** | none (only tile+message change) | BP in the loop `0xffffb352/0xffffa8d8/0xffffb82c→0xFFFF`: does it touch HP/party (wipe) or RNG? |
| **Player drowning — refresh** | not modeled | BP at `0xffffd740`: screen refresh without RNG? |
| **Underworld destination on town exit** (loc 0x19, town exit F1.3) | always Britannia (floor 0); destination tables 0x1e89/0x1eb1 not modeled | read the tables `[loc+0x1e89]`/`[loc+0x1eb1]` (party_x/y) and the g_floor (0xff for loc 0x19) after exiting; DS "Underworld!" 0x26b9 |
| **Viewport filler tile per location** (town exit gate TOWN 0x788, fix #45) — ✅ **DERIVED from the asm (task #70)**, no longer an approximation | **RESOLVED by static reading of the kernel `0x4402` (get_tile_ptr):** for `g_location` 1..0x7f (small map) any coord with `x<0 \| y<0 \| x>31 \| y>31` jumps to `0x4496` and returns the **FIXED POINTER `DS:0x6A07`** = the LAST byte of the 32×32 buffer (base `0x6608`, `+0x3FF`) = **cell (31,31)** of the map itself. **It is NOT an index clamp, NOT a wrap, NOT a hardcoded constant: ALWAYS cell (31,31).** This **reconciles** the table of `SmallMap.GetOutOfBoundsSprite` (Ultima5Redux): Grass in normal towns (Britain (31,31)=5) and the 3 exceptions (Sin Vraal=Desert 7, Sutek/Grendel=Swamp 4, Stonegate=Hills 11) are SIMPLY their respective (31,31) cells. **Faithful RENDER:** `ActiveMap.edgeFillTile` (`core/world/map.ts`) + `skin/coreview.ts` (window/LOS/ambient use the same fetch, like the original) — fixes the pixel-diff finding `britain_legit` (black south edge→grass). **MOVEMENT:** `TOWN_EDGE_FILLER_TILE=Grass` (`world/movement.ts`) remains an approximation but is **behaviorally identical on foot** (the 4 cells are passable; the only transport in town) — leave as is or align to `edgeFillTile` in a future cleanup | ~~BP at TOWN 0x788~~ — **static derivation closed**; a DOSBox witness of the Sin Vraal/Stonegate edge would confirm (31,31)≠Grass as an optional finish |
| **Troll toll spawn (reject branch)** (troll_toll 0x1B3E, F1.3 Flow 2) | assumed **0-RNG**: `spawnTrollCombat` places the troll at party_x/y without rolling rands and forks the combat seed at the current point of the stream (`startCombat`). The bodies of `0xb714` (chooses the enemy), `0xb8a4` (placement, explicit coords) and `0xdf80` (combat entry) are OUTSIDE the repo's disassembled overlays | DOSBox trace of a reject: BP at `0xb714` + reading of `g_rng_seed` BEFORE/AFTER the spawn (and at `0xb8a4`/`0xdf80`); if they differ, `0xb714` consumes rands and they must be modeled before the combat fork |
| **CAST2 strings of the shrine donation** (prompt 0xb692, "not enough" 0xb6b9; F1.3 Flow 3) | placeholders ⚠ without derived text ("How many cycles wilt thou donate?", "Thou hast not enough gold!"): pre-existing RE gap (`shrines.md:159`). The LOGIC (100·n gold / +n karma / clamp 99 / re-ask) is fixed by asm; only the TEXT is missing | DOSBox capture donating at a shrine with the Codex: read the exact strings from CAST2.OVL 0xb692/0xb6b9 and replace the placeholders |
| **FLYING row of the movement-class bitmap** (combat, fix #43; kernel 0x2C4C / table [0x54F4 + tile>>2]) | the clone approximates `canFlyOverWater` as `landEnemyPassable ∪ waterEnemyPassable`. It is NOT the asm (the bitmap row for the flying class remains underived) nor the reference (`Enemy.CanMoveToDumb` uses `IsWaterTile` = `Name.contains("water")`, not `waterEnemyPassable` — they diverge on 9 tiles: Waterfall1-4, CornerWithWater1-4, WaterJugTable). Safe approximation (does not reintroduce the freeze). Also pending: `isSand` (the reference uses `Contains("sand")`; harmless today, only Sand Trap in Desert with land=walkable) | disasm of kernel 0x2C4C + dump of the class table 0x54F4 (which row the flying sprites Bat/Ghost/Gazer/Wisp/Dragon/MongBat index and which bitmap 0x54xx they point to); or DOSBox trace with a Bat over a Waterfall/CornerWithWater tile and see if it steps on it. Collate against the land∪water approximation |
| **Per-environment persistence of the actor table 0x5C5A** (O6; overworld wanderers, fix #49) | the clone keeps **ONE global list** `overworldEnemies` (in `GameState` since #49, `state.ts`) that is **kept EXACT across excursions** to town/dungeon: it only ticks in overworld (`outdoorWorldTurn`, loc 0); upon entering another environment it is **not cleared** (frozen, does not tick); upon return, the `>22` tile cleanup (with wrap) prunes the distant ones and the chase resumes with the survivors. The binary **REHYDRATES** the 0x5C5A table **per-environment** upon loading each map (⚠ NOTE 2026-07-13, scout-regen/claims-audit: for INTERIORS the source is the static .NPC — see row O6 below; for the OVERWORLD the source upon return is plausibly the persistent overlay SAVED.OOL, BUT whether the wanderers travel in it or are regenerated from the spawner remains **underived** — do not cite the size match 0x100/0x200↔0x6B4/0x9B8 as evidence: it is the single-root inference flagged by F1.13), so an overworld you return to **could REGENERATE** its wanderers instead of keeping the exact ones. **PRE-EXISTING** divergence of the spawner (F.2/loops), **NOT introduced by #49**: #49 only moved the STORE (class field → GameState) so the list survives save/load, **without touching** the cross-environment semantics (which already lived in process memory) | DOSBox trace (question O6 of `scout-objects.md`): spawn a wanderer in overworld, enter a town/dungeon and return; read the 0x5C5A table BEFORE/AFTER the excursion — does it keep the same 8 B per slot or regenerate them from the loader? If it regenerates, the clone's global preservation is unfaithful and is corrected in the **`worldObjects` unification of F1.5** (per-location hydration/persistence according to how O6 turns out) |
| **Well horse spawn coord algorithm** (wishing_well, LOOKOBJ 0x0132-0x014e; F1.4 T3) | the clone places the horse (tile 0x10) at the **1st passable adjacent cell** to the party in order N,E,S,W via `mapOverride` (`spawnWishHorse`, `game.ts`). Parity of **VALUE** (a MOUNTABLE horse appears in Paws/Empath), not of coord. The spawn body **IS disassembled** (against the prior note that gave it as absent): `0x0132 mov ax,0x10` (tile) → push of coords derived from `[bp+8]+1` / `[bp+6]` / `[bp+4]` → `0x014e call 0x97e4` (kernel_spawn_object). What is **missing to derive** is the coord ALGORITHM of `0x97e4`: what those 3 params represent (x/y/dir of the well vs. of the party) and how `0x97e4` maps them to the final cell (the `inc ax` over `[bp+8]` suggests a +1 on one axis). It closes upon unifying `worldObjects` in **F1.5** (validated behaviorally by the E2E journey `shrines.spec.ts`: the horse appears in Paws and is mountable by (B)oard), where the spawn stops being an override and takes the canonical coord | disasm of `kernel_spawn_object` 0x97e4 (outside LOOKOBJ; locate in the resident/another overlay) + meaning of `[bp+4/6/8]` in the `wishing_well` frame; alternatively DOSBox trace: ask for "Horse" in Paws and read the exact cell where the tile 0x10 appears relative to the party/well |
| **Underived shrine/well strings** (CAST2 0x10fe "Meditate?"; DATA 0x4b5e quest / 0x4b6e lesson; LOOKOBJ 0x720c-0x728c of the well; F1.4 T1-T3) | placeholders ⚠ in `game.ts`/`main.ts` ("Wilt thou meditate?", "Drop a coin?", "Thy wish?", "The Quest of X is complete!", "A lesson of the Codex is revealed.", "Poof!", "No effect...", "Nothing happens."…): the LOGIC is fixed by asm; only the TEXT is missing. Sibling of the pre-existing donation gap (row CAST2 0xb692/0xb6b9 above) — consolidated here under the same closure | DOSBox capture of each screen (meditate at a live shrine, pilgrimage to the Codex, restore a destroyed one, make a wish at the well): read the exact strings and replace the placeholders |
| **ESC of the "Meditate?" prompt** (raw getkey CAST2 0x10fe undecoded; F1.4 T1) | the clone chooses **ESC = decline** (harmless: no effect nor world turn, same as 'N'; type `yesno-esc` in `main.ts`). The getkey is not decoded byte by byte, so if the original re-reads ESC instead of declining it is an open question | trace of the binary's getkey (BP at 0x10fe): see which scancodes it accepts and whether ESC closes the ceremony or re-reads it |
| **Meditation minutes** (shrine/Codex ceremony, g_location=0xFF; F1.4 T1) | the clone charges **0 minutes** (the ceremony sets g_location=0xFF, which suspends the world-turn; the cycles 4/7 are cosmetic animation without world turns inside). Probably exact | BP at `advance_clock` during a full meditation: read g_clock BEFORE/AFTER and confirm delta 0 |
| **Does canceling the getdir charge a turn in the 7 non-klimb directionals?** (open/look/talk/get/fire/search/jimmy; fix #50) | the clone does NOT charge a turn on cancel (the "Cancelled." branch of the `pendingDirCommand` flow, `main.ts`). In the binary the consumed-turn flag is set by EACH command after its getdir, and **the return convention is NOT uniform** (verified in the dumped asm): town_klimb (0x0B82) returns a **flag 0/1** (cancel→1, ported in #50); (O)pen (SJOG 0x1374) returns a **pointer to a message string** (cancel→0=no message, collated with DATA.OVL: "Locked!"/"Too heavy!"/"What?"…); the SJOG 0x1E1C command **prints its own message and returns a flag** (cancel→1). Who converts each return into `advance_clock` is the **RESIDENT kernel dispatcher** + the getdir body (0x766c/0xB41C), **absent from the dumped overlays** (same structural limit as 0xb714 / the drowning helpers) ⇒ undecidable from the available asm whether the cancel charges a turn in these 7 | DOSBox trace: read the time BEFORE/AFTER canceling the getdir of each of the 7 (BP at the clock / reading of `g_clock`) — the minute delta says whether they charge; alternatively, dump the resident dispatcher + getdir 0x766c and see how it maps each return to `advance_clock`. Only klimb was confirmed (flag 0/1) and ported in #50. **Reviewer note (F1.9) — LOOK specifically:** the LOOKOBJ frame sets `[bp-2]=1` in the prologue (0x3178) and returns it via the **SHARED epilogue** (0x31ee) — the same pattern as gem_view ("without gems it ALSO charges a turn", 0x344a→0x33ea→0x31ee default 1) — which suggests that **(L)ook DOES charge a turn in BOTH branches** (valid target and cancel). That raises this C for Look from "no data" to **strong indication (BP pending)**; the current clone treats `look()` as a free action (divergence to re-measure if the BP confirms the charge) |
| **Does the Ignite-FAILURE ("None owned!") charge a turn?** (F1.8-T1; cmd_ignite CMDS 0x0D98) | the clone CHARGES the standard turn also on failure (`game.ignite()` → `runContextTurn({consumed:true})` in both branches), fixed by `easy-keys.test.ts`. It is a **reasonable inference**, not a fact: the kernel dispatcher returns "consumed" (AX=1) by DEFAULT and `cmd_ignite` 0x0D98 **does not touch `g_unk_24e6` in any branch** (neither success nor failure) — unlike Push/Jimmy which do set it only on success. Moreover the dispatcher **IGNORES the overlay's AX for I** (and for P), and the MAINOUT point that translates return→`advance_clock` is **unmeasured**. Only the **SUCCESS** is runtime-verified (`04-torch-ignite-town`); the FAILURE charge remains without oracle. The failure assert in `easy-keys.test.ts` carries a ⚠ comment pointing to this row | DOSBox trace: BP at `g_clock`/`advance_clock`, press I with **0 torches** (`g_torches=0` → "None owned!") in town/outdoor and read the time BEFORE/AFTER — the minute delta says whether the failure charges a turn. If it does NOT charge, correct the assert with a documented exception (bug-for-bug of the clone → branch without turn) |
| **Location of the bought ship's dock — overworld vs town** (F1.5 T4/T5; MAINOUT 0x0D22) | the clone places the ship bought at the Shipwright as a `worldObjects` object in the **OVERWORLD (location 0)** with coords `SHIP_DOCK_X/Y` (`shop.ts` → `game.spawnDockShip(...,0)`). `SHIP_DOCK_X/Y = [39,151,79,138]`/`[221,21,109,159]` are 0-255 coords of Britannia ⇒ buying in a coastal town the ship appears on the **overworld coast**, not inside the town. Parity of VALUE (the ship exists, boardable at the dock); the loc-0 choice is inferred from the range of the coords, **without byte-by-byte verifying** that the binary writes `g_location=0` in the object's slot | DOSBox trace: buy a frigate at a Shipwright and read the just-allocated 0x5C5A slot (obj+4 = the object's g_location) — 0 (overworld) or the town's id? And confirm where the sprite appears upon exiting the town |
| **Camp — event handler 0xbfd6 = the APPARITION (camp_results)** ✅ **RESOLVED-RUNTIME 2026-07-14** (F1.8-T2; gate CMDS 0x04e7-0x0502) — previously Class C "undecidable" | **`0xbfd6` is NO LONGER opaque: it is the apparition, NOT an ambush.** Address resolution: `call 0xffffbfd6` is a near-call E8; with CBASE(CMDS)=0xBF80, target = `(0xbfd6+0xBF80)&0xFFFF = kernel 0x7F56`. At kernel 0x7F56 there is a **resident PLINK stub** `lcall 0x72e:0x2ec` (overlay-load thunk) + inline word `0x000B` (overlay OUTSUBS) + `ljmp 0:0xa8e8` (= OUTSUBS 0xA290+0x0658 = `outsubs_camp_results`). **RUNTIME (grade B-runtime, live RAM at `load_seg:0x7F56` = `9A EC 02 52 0F 0B 00 EA E8 A8 24 08`):** the thunk's segment relocates 0x72E→0x0F52 (=0x72E+load_seg) and the ljmp's 0x0000→0x0824 → real destination `load_seg:0xA8E8` = camp_results. ⇒ the 25% event loads OUTSUBS and executes the APPARITION (full heal + `status='G'` + level-up + karma speech). **There is no ambush branch.** The port's `"Ambushed!\n\n"` is **MISLABELED** (Task T-A). The gate `test [bp+8],0x82==0` (0x04e7, skips the event) remains Class C (flag semantics underived). Detail in `re/notes/oracle-camp-event.md`. `send_key('h')`→camp entry 0x3C9A confirmed live; the SCENE could not be driven headless (hours prompt 0x266c/0x1b38/getkey 0x1d5e with `int21 AH=6` = Class A input limitation) | ✅ mechanism closed by live RAM. Residual: witness of the EFFECT on the roster (heal P→G/HP maxed) requires RE of the hours sub-prompt (harness) |
| **Camp — apparition level-up (OUTSUBS `outsubs_camp_results` 0x0658)** — FABRICATION CORRECTION (#27) | **Lord British trigger RETIRED as fabrication.** The clone fired the level-up upon closing LB's dialogue (`main.ts`, folklore "talking to LB raises levels"); the binary computes it UPON CAMPING and **NEVER in a TALK/CASTLE handler** — the level byte (DS 0x55be @0x070e) and the maxHP (0x55ba @0x0717) are written in ONE SINGLE place in the whole game: `outsubs_camp_results`. Now `Game.camp()` invokes `campLevelUp`. **DERIVED [D]:** iteration over the `partySize` members (0x07fb), **skip dead** ('D'/0x44, 0x080f), `level=levelForExp(exp)` with idempotent guard `cmp ax,dx;jne` (0x0704), `maxHP=currentHP=30·level` SET (0x0717/0x071b), **+1 boost of STR/DEX/INT** by `rand(1,3)` (0x0752) cap 30 (0x0784), and the byte-exact TEXT of DATA.OVL: "An apparition!" (DS 0x7750) + `"Hail, {n}! For thy valiant deeds, I shall reward thee! Thou art now level {N}, and {stronger!/quicker!/wiser!}"` (DS 0x776a/0x7774/0x77a4/0x77b8/0x77c0.., fileoff DS+0x10). Rand-neutral except real level-up (no parity scenario crosses a threshold; the 16 PCs of initial-state are already level↔exp coherent ⇒ 0 level-downs in normal play). **GATE RESOLVED-RUNTIME 2026-07-14 (previous row):** `camp_results` (the apparition) is NOT unconditional — the ONLY call site of `0xbfd6`(→kernel 0x7F56→camp_results) is CMDS 0x0502, INSIDE the `rand(0,99)<25` branch. ⇒ the original computes the level-up **ONLY within the 25% apparition**; the port runs it on EVERY camp (`campLevelUp` unconditional). The idempotent guard makes it harmless for the level *value* but **the timing DIVERGES**: the port levels up on the next camp after gaining exp, the original requires the lucky apparition (25%) — **DIVERGENCE confirmed, Task T-A** (gate `campLevelUp` under the 25% + add full heal + `status='G'` + karma speech that the port omits). Ref `re/notes/oracle-camp-event.md` §C | ✅ GATE resolved by live RAM (call-target = camp_results, single call site in the 25% branch). Residual: witness of the scene (input limitation, see previous row) |
| **Camp — the apparition gated at 25% + `"Ambushed!"` re-attributed** ✅ **RESOLVED (Task T-A, #7 `port(live)`)** — closes the divergence of the two previous rows | **The port already gates the apparition.** `campHoleUp` returns `apparition = rand(0,99)<25` (0x04f4→0x0502 call 0xbfd6) and `Game.camp()` runs the apparition **ONLY if it crosses**. `campLevelUp`→**`campApparition`** (moved inside the gate): for each LIVE member (skip 'D', 0x080f), in the asm order — **full heal** `currentHP:=maxHP` (0x0820/0x0824) + **`status:='G'`** (0x0828, cures poison/sleep of ALL live, including the one on watch — the apparition loop does NOT respect the watch) + conditional level-up (0x0704; the SET of maxHP=30·level overwrites the full heal). "An apparition!" (DS 0x7750) is printed on EVERY apparition, not just with level-up. Rand order intact: partial-heal+MP (campHoleUp) → gate → rand(1,3) per member that goes up (identical to the asm CMDS 0x046e→0x04f4→0x0502→OUTSUBS 0x0752). **`"Ambushed!"` was MIS-ATTRIBUTED and is REMOVED from the 25% gate:** the string DS 0x41e0 (fileoff 0x41f0) IS real but its ONLY XREF is **CMDS 0x0247** — an AMBUSH BRANCH distinct from the same camp/rest routine (CMDS 0x0000, `sub sp,0x3c`), gated by **`rand(0,63)==0`** (≈1.5%, 0x021d-0x0228) which shuffles an enemy table (0x1734) and places combatants (status='G' to 6 slots, 0x026e-0x0290) — NOT the `rand(0,99)<25` gate of the apparition. That ambush branch (real combat setup) remains **NOT wired** (major combat scene, not derived here); "Ambushed!" removed from the manifest (green string guard). **Class C residual:** (1) the **speech by karma tier** upon closing the scene (OUTSUBS 0x090e-0x099b: `karma/20` indexes table 0x1a74 of 5 lines, or special format 0x77ee if karma≥80; strings 0x77e0/0x77e4/0x77ee/0x77f8 + composition helper 0x82de with args 0x7d0/0x29f — large block untranscribed); (2) the MP restoration of the apparition (0x079c-0x07f1) which `campHoleUp` already covers except the one on watch; (3) the opaque flag `[bp+8]&0x82` (0x04e7, skips the gate — still underived). Tests: `game.test.ts` (with-event raises+heals+strings / with-event-without-exp heals+status='G' without harangue / without-event nothing but rand consumed), `commands.test.ts` (`apparition` true/false), `quest.test.ts` (`campApparition` heals+status). Ref `re/notes/oracle-camp-event.md`, disasm OUTSUBS 0x0658-0x099b + CMDS 0x0000/0x0247 | ✅ mechanism and re-attribution closed by disasm+XREF. Class C residual: karma speech (offsets above) + ambush branch CMDS 0x0247 (combat-setup) + flag `[bp+8]&0x82` |
| **Camp — real heal gate (cooldown `g_unk_588c` + `hours>5`)** (F1.8-T2; helper 0x0400, 0x03ea/0x03f4/0x0453/0x0505) | the clone (`campHoleUp`) heals **ONCE per camp** each eligible member (excluding the dead and the one on watch `si≠[bp+6]`, 0x0461, DERIVED) + 1 ambush gate — **rand count corrected in the review** (previously it ran the pass N times). ⚠️ What the clone does NOT model: (a) the cooldown `g_unk_588c` is **assumed 0** (1st camp; if you camp twice in a row the 2nd would not heal until the cooldown expires), and (b) the gate `[bp+4] hours>5` (0x03f4/0x0453 skip the whole helper — heal AND gate — if hours≤5). Both gates depend on time state NOT reproducible headless. EXCLUDED from the seed-exact set | DOSBox trace: camp 3h and 9h with the party damaged and BP at 0x0400/0x046e — confirm the cutoff at hours≤5 and that there is only 1 heal pass per member; read `g_unk_588c` before/after |
| **Camp — sleep loop + per-hour ambush roll (`rand(0,63)==0`)** ✅ **PORTED BEHAVIORALLY (Task #8, `port(live)`; unblocked by the static resolution `camp-ambush-resolution.md`)** — the MECHANIC no longer diverges; the visual scene + 1 runtime residual remain Class C | **`Game.camp()` already models the per-hour ambush roll.** The loop rolls `rand(0,63)==0` (CMDS 0x021d) on EACH hour crossing BEFORE the helper 0x0400: **N−1 rolls** per camp of N h (the last hour == target_hour exits via 0x01f3 before the check). On a hit (~1/64): it picks enemyType with `rand(0,7)` (0x0239) in `AMBUSH_TABLE` (bytes DS 0x1734 = DATA.OVL fileoff 0x1744, `29 14 15 18 16 19 24 14`, **anchored byte by byte** by `camp-ambush.test.ts`). **The byte IS `defIndex`** (monster TYPE index), CONFIRMED by disasm in the resolution: `0x6BC2`=render_animated_tile → `kernel_spawn_actor` 0x6506 uses the byte `[bp+0xc]<<3+0x13c1` (derives the sprite) and stores it as the actor's TYPE field in `0xba14`; same space as `TROLL_DEF_INDEX`=41=0x29. Real list (`enemyDefs[i].name`): **Troll·Giant Rat×2·Bat·Slime·Giant Spider·Gremlin·Headless** (Giant Rat doubled ⇒ 25%). ⚠️ the scout spec's list (BARD/Gazer/Crawler/Orc/Gargoyle/Skeleton/Mongbat) was INCORRECT (it used the `monsterNamePtrs1866` space, not the `defIndex`) — corrected. It prints **"Ambushed!"** (DS 0x41e0, 0x0247, re-added to the manifest) and sets up REAL combat via `startCampAmbush`→`startCombat` (fork of the live stream), with an **early-return** (0x0306 ax=1): no heal/gate/apparition, clock at the cutoff hour, remaining hours NOT slept. **Combat route:** `0x6BC2` only does the actor SPAWN; the real combat entry is the **kernel-resident chain 0xb714/0xb8a4/0xdf80** (OUTSIDE the dumped overlays) — the **SAME gap** as the troll toll (row :565, 0-RNG Class C assumption); the port reuses that accepted pattern (synthetic enemy at party_x/y → startCombat), it does NOT fabricate. **Arena = `CampFire` DERIVED FROM DATA:** BRIT.CBT brings arena idx 0 with the literal name "CampFire", **unreachable** by `combatMapForTile` (TileData.json sweep: 0 tiles route to it; the terrain route would give Glade = UNFAITHFUL) ⇒ the kernel entry `0xdf80` forces it (the exact INSTRUCTION is not dumped). The port's override is necessary and correct. **BEHAVIORAL tests** (not seed-exact): N−1 rolls, on-hit interrupts (clock+type+arena+no apparition), anchored table, no-heal. **Still EXCLUDED from the seed-exact set** (zero `camp*.json`; the INTERNAL stream of the loop diverges by design — the port does not reproduce the rands per wind tick 0x5910/ring_regen 0x400c/housekeeping that `advanceClock` does not roll step by step; it only models the observable RESULT ~1/64 per hour + type). **Class C / residuals:** (1) the visual SCENE (alarm 0x2056/0x207e, "sleeping party waking" 0x0254-0x02bc, SFX) → AV/#28; (2) the WATCH sub-event (§4, no watch modeled); (3) **RUNTIME residual** (`suppressIntro`): whether `0xdf80` adds "{name} attacks!" after "Ambushed!" only a DOSBox witness will tell (in U5 "Ambushed!" replaces the per-monster intro → faithful). Ref `re/notes/camp-ambush-resolution.md`, `camp-ambush-spec.md`, `.superpowers/sdd/port-t8-review.md` | scene/arena/SFX = Class C (task #28, the faithful combat screen captures the CampFire arena); 1 DOSBox witness (BP at the ax=1 return of camp() + step-in to 0x3C9A) closes the 3 residuals (0xdf80 route + combat-map arg + "attacks!"?); the user's video does NOT capture an ambush (event ~10%/night) |
| **Camp — clock unit per hour + scene cadence** (F1.8-T2; CMDS 0x0066-0x0079 + scene 0x5f86/0x0318) | **DERIVED (decisive)**: 0x0066 sets `target_hour=(g_hour+hours) mod 24` ⇒ camping N hours makes **N game hours pass**. The clone advances `advanceClock(60)` per hour (N×60 min). The original animates them in steps `advance_clock(5)` per frame (0x0318) up to the target; the port discretizes (L4 timing out of base) ⇒ it does NOT replicate the per-frame cadence nor run the per-hour housekeeping (meals/poison) during sleep — the `cmds.md §5` "advance_clock(10) per hour" is imprecise about the literal (real=5/frame), but the NET (N hours) is what is modeled | none (NET unit derived from the asm); for the housekeeping-during-sleep: BP at `advance_clock`/`kernel_turn_housekeeping` during a camp that crosses 6:00/12:00/18:00 and see if it subtracts food |
| **Camp — camp scene (render)** (F1.8-T2; kernel 0x5f86 assembles the party on a scene-map 0xa9fc, bedroll tile) | the clone wires the MECHANIC (heal/clock/gate) but **does NOT paint** the scene the user captured (party lying around the campfire + bedroll). The exact tile of the bedroll/campfire of the scene is not derived ⇒ render deferred, Class C | dump the scene-map that 0x5f86 copies to 0xa9fc (movsw ×4 from 0xaa04, 0x3e8c) and the lying sprites; capture the DOSBox screen of a camp |
| **Coord of the ship drop upon X-it on land** (F1.5 T4; CMDS 0x0EB4 frigate→land branch) | upon disembarking a frigate on land the clone re-docks the ship-object at the **party's cell** (`exitVehicle`, `parkedShipTile`), keeping its live hull/skiffs. Parity of VALUE (the ship remains re-boardable where you exited). The **exact offset of the X-it write** (party's cell vs. adjacent/the one the frigate left) is NOT in the scout's disassembly | DOSBox trace: sail a frigate next to land, X-it, and read the coords (obj+2/obj+3) of the 0x5C5A slot where the ship ends up relative to the party |
| **Attack in town — combat/karma/guards** (F1.8-T3; TOWN 0x09e6) | the clone wires in town the key + `"Attack-"` (DS 0x26e0) + `"Nothing to attack!"` (0x26fb), but the ATTACK on an adjacent inhabitant — which in the binary applies **karma −5** (`g_karma` 0x5888 via 0xbd66, in the NPC-tile<0x80 branches 0xaef and guard 0xb53), prints **"Murdered!"** (0x2718) upon striking a guard (tiles 0x84/0x85/0x9f/0xab), **summons guards** (0xb352) and **starts combat on the town map** — is NOT modeled: the clone has no combat-entity in town (the NPCs are dialogue) nor the guards subsystem. `game.attack` in town responds `"Nothing to attack!\n"` without fabricating karma/murder/combat (GOLDEN RULE: do not invent). The OVERWORLD (MAINOUT 0x06ec) is wired (adjacent wandering enemy → `startCombat`) | DOSBox trace: attack an inhabitant and a guard in a town; BP at `g_karma` 0x5888 (exact delta), at 0xb352 (which guards it spawns and their coords/def), and at the combat entry (0xdf80) — does the NPC become a combatant or are new guards summoned? Model when town-combat exists |
| **World object layer — gaps O1-O5/O-loot** (F1.5; g_world_objects 0x5C5A) | the clone delivers the MECHANISM of the layer (`worldObjects`: chests/torches/ships) but several data of the original remain underived: **O1** byte-by-byte mapping SAVED.GAM 0x6B4 (0x100 B) / 0x9B8 (0x200 B) ↔ 0x5C5A (opaque loader); **O2** semantics of slot+6 (no name) and slot+1 (tile2/frame); **O3** complete jump-table of `get_special_item` 0x1458 (branches torch 0x148c and **keys 0x1568 ported** — the latter in #69, ItemKey→skull/regular keys by the quality byte; **shard/amulet 0x16b6/0x1712 ported** in F1.10-T3 and **crown/sceptre 0x16e6/0x1706 ported** in F1.10-T4 (grantPlotItem); carpet/gems/food/gold/wooden box/HMS Cape not wired); **O4** torch flags array @0x5840 (the clone fulfills the observable semantics by removing the worldObject); **O5** origin of the initial content of town chests (today seeded by test hook, not in play); **O-loot** id→index mapping of the quantity-array in `applyLootGrant` (equipment/potion/scroll/sandalwood not applied; counters gold/keys/gems/torches/food yes, exact). **O6 OPEN — verdict INVERTED 2026-07-13** (`scout-regen.md` Re-analysis 2; flag F1.13 `re/claims-audit.md`): INTERIOR objects are **REGENERATED upon re-entering** the map — there is no SAVED.NPC (the town state is not written to disk); entering re-reads the block `0x240×(loc&7)` of the static .NPC (NPC.OVL 0x0000, `call 0x82de`); `open_chest_world` 0x112C clears the slot only IN MEMORY; SAVED.GAM 0x6B4 saves only the CURRENT ENVIRONMENT (+ bitmaps npcDead 0x5B4/npcMet 0x634/search-found 0x2B6); only the overworld persists as an overlay (SAVED.OOL). Confirmed by Redux (`InitializeFromLegacy`: "otherwise we assume it's fresh and new") and known behavior (LB basement farming). Nuance: the map where you SAVE restores from 0x6B4. ⇒ the clone's global persistence of `worldObjects` (F1.5) was **UNFAITHFUL for interiors**. **✅ CORRECTED in task #3**: the clone now distinguishes **interior-rehydrates vs overworld-persists** — `Game.hydrateInteriorObjects` re-seeds the interior objects from the .NPC object-slots on each ENTRY (checkLocationEntry), `discardInteriorObjects` drops them on EXIT, and the load does NOT rehydrate (it trusts the save's `worldObjects` = the 0x6B4 nuance of the current environment); the overworld ships/objects (loc 0) persist as before (SAVED.OOL). Only the **byte-by-byte closure** (O1) and the **initial contents** (O5) remain open. The previous "RESOLVED by design" was single-root inference-by-sizes | O1: dump of SAVED.GAM 0x6B4/0x9B8 + BP at the map loader (copy to 0x5C5A) collating byte by byte. O3: disasm of the complete jump-table 0x1458. O5: BP at the hydration of slots 23/24/25 upon entering loc 17 → read the real +5 byte (contents & 0x7f + bit 0x80 trap) that the clone today approximates with `INTERIOR_CHEST_CONTENTS=8` (Class C). O-loot: BP at loot_fixed/loot_random (0x1040/0x10B8) reading which quantity array each id writes to |
| **NATIVE save SAVED.GAM + sidecar** (task #27, interview decision #12) | The clone's save IS a byte-valid `SAVED.GAM` of **4192 B** (0x1060) + a **JSON sidecar**. Codec on two sides (the game does not import from the extractor, mirror pattern): `serializeSaveGame`/`parseSaveGame` (extractor/src/parsers/savegame.ts) and `exportNativeSave`/`importNativeSave` (game/src/core/saveNative.ts). **"Template + patch" strategy**: the modeled fields are patched over a base template and the **dark bytes are PRESERVED** (padding + object table 0x6B4 [O1] + character-states 0x9B8 + movement lists 0xBB8+ + sprite indexes 0xFF8) — NOT synthesized (the clone does not derive those tables; see row O1-O5). **BYTE-EXACT round-trip verified** over INIT.GAM and over the **user's real save** (partida-javier-2026-07-15): `serialize(parse(x),x)===x`. **Fields the format has NO room to save (they go to the SIDECAR):** the sidecar DELIBERATELY separates two categories for the Grand Tour MIRROR — (a) **QoL** (journal `journal`, minimap `explored`, `treasuryLoot` dep): a mirror against the original IGNORES them; (b) **UNMAPPED GAME STATE** (`worldObjects`, `overworldEnemies`, `questFlags`, `openDoors`, `mapOverrides`, `skullTreeFoundDay`, and runtime `wind`/`sailDir`/`windDriftCtr`/`shipHull`/`shipSkiffs`/`hmsCapeToggle`/`shadowlordLocs`/`shadowlordSummoned`/`shadowlordDoomBits`/`blackthornPassGranted`): THIS is the list the mirror compares alongside the `.GAM` (a `.GAM` checkpoint by itself does NOT capture it). **Extras with REAL room in the .GAM** (written to byte, not to the sidecar): `transportTile`→0x2D6, `prevHour`→0x2DA, shrine bitmaps→0x326/0x328/0x332, and `obj0`@0x6B6/0x6B7 synchronized with `position` (engine gotcha: otherwise the avatar's sprite gets misplaced). **⚠ Discrepancy 0x2E5:** `g_turn_count` (0x2E5) is **u8 SATURATING at 0xFF** and **0x2E6 is ANOTHER hourly counter** (RE: oracle.py:98), NOT the high byte of a u16. The **game-side writer is FAITHFUL** (writes `min(turns,255)` in 0x2E5 without touching 0x2E6); the **extractor's parser reads u16** (byte-exact mirror for INIT.GAM where it is 0) with a **cross comment** so nobody "fixes" it the wrong way; the game-side parser reads u8 (self-consistent with its writer). ⇒ for a save with >255 turns, the clone saves 255 (faithful to the binary, which also saturates). **SAVED.OOL** (512 B, overlay of overworld map-units) documented but the clone does NOT yet generate it (the overworld wanderers live in the sidecar `overworldEnemies`; see row O6 on whether the binary persists them in SAVED.OOL or regenerates them — underived). | Manual byte-diff against DOSBox: export a clone save and load it in the original Ultima V (mirror verification); confirm that the 0x6B4/0x9B8 preserved from the template suffice for the original to accept the file. Generating a byte-valid SAVED.OOL remains pending (depends on O6: do the wanderers travel in the .OOL?). Citation: docs/formats/tlk-npc-dataovl-gam.md §4, extractor+game savegame codecs, tests savegame.test.ts / save-native.test.ts |
| **Table food — plates mechanic** (get, #69; tiles 0x9A/0x9B/0x9C) | the clone MODELS the plates as theft: +1 food, karma−1, mapOverride, string "Borrowed!". The binary (GET 0x18CE) on 0x9A/0x9B/0x9C **eats in place** ("Mmmmm…!"/"Can't reach!", cmds.md §10) — without theft nor karma; "Borrowed!" is real but from the **wall torch** 0xB0/0xB1 (DS 0x8de8). The clone's table mechanic is clone-era (Ultima5Redux) and was not rewritten in #69 to avoid inventing underived deltas | DOSBox trace: (G)et over a plate 0x9A/0x9B/0x9C and read g_food/g_karma BEFORE/AFTER + the exact string (¿"Mmmmm…!"? reachability?). With that, port the eat-in-place and retire the theft+karma |
| ~~**useShard string** (`game.ts useShard`, F1.10 plot)~~ ✅ **CLOSED (F1.10-T5)** | The fabricated string "Nothing happens here…" is **RETIRED**: `useShard` now runs the REAL state machine (CAST.OVL 0x15b4) with the byte-exact strings of DATA.OVL ("Gem Shard\n\nThou dost hold above thee the evil Shard of …", "…and cast it into the Flame of …", "The doom of the Shadowlord … is wrought!", "No effect!"). The whole ritual (summoning by (Y)ell + destruction by (U)se) is DERIVED, not simplified — see §5 below and `re/notes/shadowlord-ritual.md`. Citation: `re/disasm/CAST.OVL.asm` 0x15b4 + `CMDS.OVL.asm` 0x1030/0x1418, tables DS 0x4882/0x4892, strings DS 0x4794… |
| ~~**String "A Word of Power seals this dungeon…"** (`enter`, dungeon entry without the Word spoken)~~ ✅ **CLOSED (lane DOOM-SEAL-WIRE, 2026-07-20)** | The fabricated string (without citation) is **RETIRED** along with its `enter`-over-sealed flow. The original does NOT print a prompt: it presents the sealed entry as the **collapse tile 0xDF "BlockEntrance"** (TileData[223], `IsWalking_Passable:false`) and IMPASSABLE, via the 16×16 compose render (OUTSUBS 0x98 → func1 0x0, `0149: mov byte [bx],0xdf`), reading `0x58d0[i]` (byte 0x00=sealed→0xDF; bit 0x80=open→cave 0x16-0x18; by default they are BORN sealed). Wired in `activeMap.tileAt` (same pass as moongates/shrine ruins), floor-independent (Doom lives in the Underworld). The faithful flow: impassable collapse → (Y)ell the adjacent Word (opens the seal, persisted in `questFlags["word-spoken:<loc>"]`) → the cell returns to cave → step on it → (E)nter. Passability and interactions come for free (0xDF already impassable); (L)ook gives the faithful collapse text (`lookSpecialDescription` 0xDF → DUNGEON_BY_X). Static verdict of the doom-seal-tile lane. Citation: OUTSUBS.OVL 0x98/0x0, DS 0x58d0, tile 0xDF | — |
| **Underived combat strings** (`combat.ts:516` "Victory!"/"Defeat!"; `game.ts` "The room is guarded!" 0x235x, "Victory!" / "The battle is over." 0x237x) | without citation nor ⚠; plausibly-original (COMBAT.OVL/DATA.OVL) but not collated byte by byte | DOSBox capture of a combat (room entry, victory, defeat, closing): read the exact strings from COMBAT.OVL and collate. F2 (DOSBox capture sweep) |
| **Edge sub-lighting by light sources** (E1-S2, `core/world/visibility.ts::computeVisibleWindow`; emitters table 0x6A9A) | the port only considers as emitters the tiles INSIDE the 11×11 window (the sampler `terrainAt` never leaves the window — verified by the reviewer instrumenting it), while the original scans the full **32×32** chunk at 0x5E4A and then the party pass consults the buffer 0xAD14. ⇒ a torch/brazier located 1-3 cells OUTSIDE the window's edge, whose halo (radius 10) would reach cells of the inner edge, does NOT light in the port. It is **under-lighting** (a corner the original would show dim is left black): on the SAFE side of hard rule #2 (never reveal MORE than the original). The rest of the merge (own radius OR lit-by-emitter, with per-emitter LOS and wall sealed to dark) was **verified EXACT** against the binary by the reviewer (6000 brute-force configs, no violation) | extend the emitters scan to a band of ±3 around the window (or replicate the 0xAD14 buffer over the 32×32 chunk) and collate the edge against a DOSBox trace / pixel-diff (F3 harness) standing next to a torch at the viewport's edge |
| **Diagonal corner-cut — 8-connected flood vs cardinal-gated** (lot 5 sweep #16, internal reading of `0x5A28`; `core/world/visibility.ts::floodFOV`) | the binary's flood-fill (0x5A28) traverses the parent's 8-NEIGHBOR RING (Moore) via an 8-entry jump-table (5b16) over an accumulator that **does not reset** between directions: it reveals each lit+transparent diagonal **without a flanks condition** (real corner-cut). The port gates the diagonal to BOTH orthogonal flanks being transparent ⇒ a **diagonal slit between two wall corners** (open diagonal cell, both flanks opaque, within radius) the original SHOWS and the clone leaves BLACK. It is **under-revelation** (SAFE side of hard rule #2) but NOT pixel-exact — it occurs at every room corner (towns/castles/dungeon). ⚠ The equivalence declared in E1-S2 (`port-e1s12-review.md`, 8000 configs) was **circular**: it compared the port against a *cardinal-only* reconstruction of the reviewer himself (same wrong model on both sides), never against a Moore flood nor against DOSBox. **✅ ADJUDICATED 2026-07-14 (task #23): Moore CONFIRMED** by a 3rd reading of the disasm (`cmp ax,7`+table of 8 words+accumulator without reset) **and runtime witness in DOSBox** (BP at 5b26: the binary visits the 8 neighbors/cell, 4 diagonals included, without a flanks gate — `re/notes/flood-adjudication.md`). The reviewer's addendum "strictly cardinal, 4 entries" IS REFUTED | DOSBox witness **ALREADY DONE** (closure condition met). Only the **SDD dispatch of the fix** is pending: eliminate the flanks gate of `floodFOV` (line 199) — leave the 8 edges unconditional — and invert the E1-S2 regression tests that fix the hidden diagonal. Byte-by-byte body in `re/notes/kernel-flood-0x5a28.md` §3/§6; adjudication + dumps in `re/notes/flood-adjudication.md` |

**Effort state in F.2.** The **live infrastructure was re-verified** in this
session: `test_seed_zero_at_title_live` again read `g_rng_seed == 0` at the DOSBox
title (headless boot in freewheel, ~5 s), re-confirming the anchor that sustains
the WHOLE critical path gypsy→endgame (the master scenario starts from seed 0). The
**new measurements** of the table (BP pick_virtue 0x9a6, `[0x5891]`, BP shops 0x02D8,
NPC array seeding) each require setting up a new code breakpoint after
navigating to a concrete game state and capturing a stream through the pty, which
loses pauses (`re/notes/oracle.md` rule 7) — high-risk harness work for
a background headless session. They are left as **open oracle questions with their
BP address noted** above, ready to resume. The underlying game rule
is already fixed by the asm and cross-checked model↔clone, so the clone does not wait for these
measurements to be correct — they would only raise the label from ⚠️→formulated to ✅.

---

## 3.5 Clone-era extras — retired/derived (task #69)

**What.** Additions of phases 1-8 (clone-era, BEFORE the exact migration) that
**contradicted the purist mandate** "original 100%" and survived uncataloged
(exposed by `scout-inverse-audit.md`, inverse sweep port→asm). User decision
(2026-07-13): **100% purist base mode** → they are RETIRED or DERIVED, remaining
cataloged here as **candidates for an explicit post-100% option** (not deleted
from the roadmap, taken out of base mode).

| Extra (clone-era) | Where it was | Action #69 | Basis |
|---|---|---|---|
| **Minoc skull tree** — `searchSkullKeyTree()`: hardcoded trigger (party at (3,3)±1), **daily refill** of 5 keys, cap `taken>=5`, strings "Buried among the roots… a skull key!" / "The tree yields nothing more today.", field `state.skullTree` | `game.ts` (own method + branch in `search()`) | ✅ **DERIVED and replaced**. ORIGINAL mechanic implemented from memory → now data-driven: directional `search()` consults `searchObjects[14]` (loc5/f0/**(2,2)** = the real tree, tile 46 Tree) → tile 0x107 ItemKey → `applySearchGrant` (get_special_item 0x1458 keys branch, **SJOG 0x1568**): quality **0x85** → `0x85&0x7f = 5` **skull keys** (high bit → g_skull_keys 0x57b1), cap 0x63. Generic message "Thou dost find an object!". The "5" was NOT an invention: it comes from the `quality` byte (0x85&0x7f). **RETIRED** the memory strings, the `taken>=5` drip and `state.skullTree`. **⚠ Fidelity nuance (correction to the predecessor #69):** the daily refill was NOT "clone-era without backing" — the binary **DOES re-find the tree daily**: `search_fixed_hidden_items` (SJOG 0x0514) gates index 0x0e with `g_day != [0x57b2]` (0x0574) and upon finding it writes `[0x57b2]=g_day` (0x05b4). The clone models it **ONCE** (`search:14` questFlag), which was the **divergence ⚠** for the 3 special re-findable entries 0x0d/0x0e/0x0f (`re/notes/sjog.md`), NOT a new gap. The clone-era `searchSkullKeyTree` (drip 1/search cap 5/day at (3,3)±1) was a by-hand approximation of that real daily cycle, with wrong coord and count-semantics; its retirement folds the case into the pre-existing once-only divergence. **✅ CLOSED (task #1, 2026-07-13):** the 3 special gates are PORTED faithfully — 0x0e uses the DAILY gate `time.day !== state.skullTreeFoundDay` (mirror of `[0x57b2]`, written on finding, SJOG 0x05b1-0x05b4); 0x0d uses `state.keys === 0` (SJOG 0x055d); 0x0f uses `equipmentQuantities[39] === 0` (equip-ID 39 = **Glass Sword**, SJOG 0x0587). They are NO longer once-only by questFlag; `deserialize` cleans the `search:13/14/15` flags of old saves. **Class C** residuals (see §3.5-C below): the `call 0x770e` of 0x0d/0x0f (cell check, semantics underived → not modeled, more permissive clone) and the grant of the **id5** item of 0x0f (gap O3, not ported → 0x0f reveals the object but grants nothing). **Additionally**, the original reveals the ItemKey as an object and grants it in the subsequent (G)et (0x199f→0x1458); the clone collapses reveal+get into the Search itself (parity of VALUE) — a TWIN divergence of the chest, which **task #13 DID resolve** (loot to the floor + Get; see row §3.5); the Minoc tree remains the only residual reveal+get collapse (accepted parity of VALUE). | `re/disasm/SJOG.OVL.asm 0x1568/0x0514/0x0574`, `re/notes/sjog.md` (once-only ⚠), `data.searchObjects[14/13]`, `scout-inverse-audit.md` (reclassification) |
| **Moongate flavor** — string "You step through the shimmering gate…" upon crossing | `game.ts checkMoongate` | ✅ **RETIRED** (silence). `kernel_moongate_enter` (0x48a8-0x494d) has NO `call print`: it only animates (`g_moongate_anim`) and teleports. The clone keeps only the `map-changed`. | `re/notes/shrines.md §1.4` |
| **String "Harvested!"** (wheat harvest) | `game.ts get()` (tile 0x2D→0x2C) | ✅ **REPLACED** by the derived **"Crops picked!"** (GET 0x18CE, string DS 0x8df4). The mechanic (+1 food, karma dec if ≠0) was already faithful; only the string was an Ultima5Redux fabrication. Test `commands.spec.ts:42` corrected (the assert enshrined the fabrication — authorized exception #69). | `re/notes/cmds.md §10`, SJOG 0x18CE/0x1a58 |
| **String "Borrowed!"** (table food theft, plates 0x9A-0x9C) | `game.ts get()` `stealFood` | ⚠️ **Class C — NOT touched** (see §3, new row). "Borrowed!" IS a real string of the binary (DS 0x8de8) but for the **wall torch** (0xB0/0xB1); on the **plates** the original EATS in place ("Mmmmm…!"/"Can't reach!", cmds.md §10), without the clone's theft+karma. The clone's table mechanic (steal +food, karma−1, mapOverride) **diverges from the derived one** and its correction requires measuring in DOSBox the food/karma deltas of the plates (not derived in the notes). Do not invent (GOLDEN RULE). | `re/notes/cmds.md §10` (0x9A/0x9B/0x9C) |
| **Well horse** — `spawnWishHorse()` (spawn coord) | `game.ts` (via `makeWish`, the REAL well path wired in F1.4) | ✅ **VERIFIED reachable, already Class C**. It is the real well path (not dead code); it **validates passability** (`tileInfo(t).walkable`, 1st adjacent cell N/E/S/W). Parity of VALUE (a mountable horse appears in Paws/Empath). The coord algorithm of `kernel_spawn_object 0x97e4` remains Class C (§3, existing row). No code change in #69. | `re/deliberate-divergences.md §3` (horse row) |
| **String "citizen of Britannia"** ((L)ook shortcut over NPCs) — task #4/#65 | `game.ts look()` (`npcAt` branch → fixed string, retired) | ✅ **RETIRED and DERIVED**. `cmd_look` (LOOKOBJ 0x099c) does NOT branch by NPC: it describes the **COMPOSITED** tile `*ext_a172(x,y)` (the actor covers the terrain) via **LOOK2.DAT** (re/notes/lookobj.md:8-18). The clone did not composite the NPC layer (it is painted separately, `main.ts:235` with `type+0x100`), so it fabricated "Thou dost see a citizen of Britannia" (unsupported string: it does not exist in any derivation). **Replaced**: `look()` composes `tile = npc ? npc.type+0x100 : mapTile` and describes it with the real phrases of LOOK2.DAT (new parser `extractor/src/parsers/look2.ts` → `game/assets/look2.json`, indexed by tile, article already included). Result by data: chest-NPC → "a chest", corpse → "a corpse", carpet → "an odd rug", guard → "a guard", jester → "a jester", etc. **Additionally** the GENERIC description of all tiles moves from `TileData.json` (Ultima5Redux, clone-era) to real LOOK2.DAT (partially closes the "look_generic vs TileData.json" that `re/verified/lookobj.md:46` left to Task F). Tests: `extractor/tests/look2.test.ts`, `game/tests/look-npc-tile.test.ts`, `commands.spec.ts` (castle chest ≠ "citizen"). | `re/notes/lookobj.md:8-18`, LOOK2.DAT (string pool), `scout-castle-bugs.md` anomaly 2 |
| **look_dispatch concatenates look_generic + special (clock/Flame/dungeon)** — task #9 (finding of the #4 reviewer) | `game.ts` `lookSpecialDescription`/`look()` (the special was printed as a complete string with the LOOK2 phrase hardcoded inside, without deriving it from the data) | ✅ **RESOLVED and DERIVED.** Re-derived `look_dispatch` (LOOKOBJ 0x0502) byte by byte: ONLY **sky(0x59)** (0x0558 → `jmp 0x69c` after look_sky), **well(0xA1)** (0x0564) and **fountain((tile&0xFC)==0xD8)** (0x0580) **skip** look_generic; for **clock(0xFA/FB)/Flame(0xDE)/dungeon(0xDF)** the binary calls **look_generic UNCONDITIONALLY** (0x0590 `call 0` = base phrase of LOOK2.DAT) and **CONCATENATES** the dynamic text afterward: clock (0x0596) = "H:MM AM/PM." (H=hour%12→12, MM 2 dig, " AM.\n" 0x7310 / " PM.\n" 0x730a); Flame (0x05fd) = virtue by location 0x1e/0x1f/0x20; dungeon (0x0626) = name by X band. **Pre-existing (Task 3.12):** `lookSpecialDescription` returned the WHOLE string with the LOOK2 phrase **copied by hand** ("a grandfather clock, showing: "/"the Flame of "/"the collapsed entrance to the dungeon ") — it did not take it from the data. **Replaced:** it now returns `LookDispatch` = `{mode:"replace",text}` (sky: skips generic, only its text) | `{mode:"concat",suffix}` (clock/Flame/dungeon: `look()` puts `describeTile(tile)` from LOOK2 + dynamic `suffix`). The **output text is byte-identical** for the 3 concatenating ones (the LOOK2 phrases and the copied ones matched), but the generic phrase becomes **data-driven** (LOOK2.DAT, single source) instead of literal. Location/band without match → concat with empty suffix (faithful: the asm calls look_generic and jumps to the end without suffix). Tests: `look.test.ts` (pure dispatch replace/concat), `look-concat.test.ts` (Game.look over real clock castle LB (12,19) + Flame Lycaeum (15,8): LOOK2 sentinel proves the generic comes from the data; sky Moonglow (17,13) = regression "replaces, does not concatenate"). | `re/disasm/LOOKOBJ.OVL.asm` 0x0502 (0x0558 sky skips · 0x0590 look_generic unconditional · 0x0596 clock · 0x05fd Flame · 0x0626 dungeon), `re/notes/lookobj.md:20-38`, `.superpowers/sdd/port-f4-review.md` (finding) |
| **ERAS system (temporal enemy progression)** — task #16 (Fix A) | `encounters.ts` (`eraByTurn` + `BEGINNING_OF_ERAS [0,10000,30000]`, param `turn` of `pickEnemyForTile`; call-site `game.ts` passed `state.turnsSinceStart`) | ✅ **RETIRED**. Unfaithful Redux-ism (`GetEraWeightByTurn` / `OddsAndLogic.BeginningOfEras` of Ultima5Redux): after 10000/30000 turns the clone **changed the enemy set** — something the 1988 DOS binary NEVER does. The real selector (`tile_to_monster` MAINOUT 0x0E4E → `weighted_pick` 0x0E04) chooses the monster ONLY by (1) terrain tile, (2) `g_floor` (outdoor vs underworld ≥0x80) and (3) RNG over FIXED weight tables of DATA.OVL; **it does not read any counter of turns/days/eras**. `g_turn_count [0x588B]` is incremented 1×/turn (u8 saturating at 255) and **is never read** (write-only/vestigial). The picker now ALWAYS uses the frozen set (`eraWeights[0]`) and no longer receives `turn`; `turnsSinceStart` still exists (other uses + save-compat) but stops feeding the spawn. Live stream intact: `eraByTurn` consumed no rand and the only draw is still `rand(0,total-1)`; in normal play (<10000 turns) era 0 was already used, so there is no stream shift. **✅ VALUES residual CLOSED in Fix B (task #19):** the Redux weight TABLES (`eraWeights[0]` = `AdditionalEnemyFlags.Era1Weight`) and the picker's `canGoOnTile`/`rand(0,total-1)` were **RETIRED** from the spawn and replaced byte-by-byte by the real mechanism: `pickSpawnEnemy` (`encounters.ts`) ports `tile_to_monster` 0x0E4E + `weighted_pick rand(0,255)` 0x0E04 over the **4 fixed tables of DATA.OVL** (DS ids `0x2bd4/0x2bda/0x2bc0/0x2bcc`, weights `0x2bf0/0x2bf6/0x2bdc/0x2be8`; each weight table sums to 256). The returned id is the **sprite tile − 0x100** → the enemy is `def.tile === id+0x100` (includes the pirate ship id 0x2c → tile 300). NEW rands contract (changes the live stream): WATER branch (tile<4 ∨ 0x60-6f ∨ 0xd4-d7 ∨ 0xe4-e7) = gate `rand(0,64)<16` → `weighted_pick rand(0,255)` (+ `rand(0,7)==7`→Whirlpool on tile 1); LAND branch = `weighted_pick rand(0,255)` without gate (+ fixed routes: tile 7 `rand(0,3)==0`→Sand Trap, tile 4 on floor 0xFF→Rot Worm, tiles 0xc/0xd and `(tile&0xfc)≠0x30 ∧ tile≥0x10`→no monster). Consequence: the **Dragon IS reachable** on land (weight 2/256) and underworld (8/256) — the "Dragon unreachable" claim of Fix A was from the Redux residual. The tables are extracted to `data.json` (extractor `parseDataOvl.spawnTables`, each weight-table verified to sum 256) and a test **locks** `SPAWN_TABLES` (cited literal) == `data.json.spawnTables`. Fix A tests ("frozen pool"/"1 rand per pick") **replaced** by the contract battery (draw order, water-only, Dragon reachable, fixed routes, id→def mapping) in `combat.test.ts`. Parity re-anchor: `spawn_pick`/`tile_to_monster` in `loops_parity.py` (INDEPENDENT Python extraction of DATA.OVL) ↔ clone `loops-run.ts` kind `spawnPick`, 6 scenarios in `re/parity/loops/` + anti-false-green guard (`test_spawn_pick_cross_check_discriminates`). **All ids of the 4 tables + fixed routes map to a real `EnemyDef`** (verified semantically: water→serpents/kraken/pirates, land→orcs/trolls, desert→Sand Trap, swamp→Rot Worm) — **0 unmapped ids, 0 new Class C** on this axis. | `.superpowers/sdd/oracle-eras.md` (grade A), MAINOUT.OVL.asm 0x0E4E/0x0E04, ULTIMA.EXE.asm:4866 (`[0x588B]`), DATA.OVL (fixed tables; DS 0x2BC0-0x2BF6) |
| **Chest open+credit collapse — loot TO THE FLOOR** — task #13 | `game.ts openChestObject` (credited `applyLootGrant` in the Open itself) | ✅ **RESOLVED (authorized divergence, twin of the Minoc tree).** The clone credited the chest's loot on opening; the binary **PLACES** each piece as a floor-object in the chest's cell (`open_chest_world` 0x112C → scatter `loot_fixed` 0x1040 / `loot_random` 0x10B8 → `loot_place` 0x0F88 → `write_object_slot` 0x3A74) and the **(G)et** picks them up one by one (`Get` 0x18CE → `apply_item_grant` 0x1458: name + counter, deletes the slot 0x178B). Now faithful: `openChestObject` invokes `placeChestLoot` (find_free_actor_slot 0x0000 sweeps 31→1, cap 31, aborts without a slot; `write_object_slot` composes id/id/X/Y/floor/qty&0xff) and creates a stacked WorldObject `kind:"loot"`; the 1st piece emits **"Found:"** (0x8B5C) and loot_place prints **one line per piece** via the text dispatcher **0x12A** (local jump-table file-off 0x1B0, 31 handlers; `lootOpenLine`, verbatim DATA.OVL 0x850E-0x85F4: id2 "a sack of gold!", id8 "a gem!", id13 "some torches!"… — table DISTINCT from the Get name), 0 pieces → **"Chest empty!"** (0x8B88). The (G)et names via `lootItemName` (verbatim DATA.OVL: gold/keys/gems/torches/food/sandalwood/nested EXACT; potion/scroll/equipment without the array-name ⚠C, O-loot) and credits via `applyLootGrant` (same exact counters, now PER PIECE). Interior loot is discarded on exit (faithful, `discardInteriorObjects`); field (overworld) loot persists. **Residuals:** ✅**O-render RESOLVED (task #21)** — the floor-loot is PAINTED: tile = **id + 0x100** (high sprite bank; slot+0 = slot+1 = category id in `loot_place` 0x0F88, drawn as `type+0x100` like chests/props/NPCs). 7 sprites collated 1:1 with the forensic video (2026-07-13). The clone paints it as a render-only ENTITY (`Game.lootRenderTiles`, top of the LIFO stack, passable, without touching passability). Remains ⚠**O-loot** (potion/scroll/equipment arrays not credited). (G)et order = LIFO by slot (places 31↓, reads 1↑) — derived (A), approximated in the clone's flat list. Asserts corrected (authorized exception, they enshrined the collapse): `chest-object.test.ts`, `interior-objects.test.ts` (b/c/d), `objects.spec.ts`. | `.superpowers/sdd/scout-7af4.md`, `re/disasm/SJOG.OVL.asm 0x112C/0x1040/0x0F88/0x18CE/0x1458`, `re/disasm/ULTIMA.EXE.asm 0x3A74`, `re/notes/objects.md` O3/O-loot/O-render, `.superpowers/sdd/port-f13-report.md` |
| **Invented TREASURY_CHESTS system + "broken wall" (#F13-1)** — task #3/#64 | `game.ts` (`TREASURY_CHESTS`/`refreshTreasury`/`tryLootTreasury`, retired) | ✅ **RETIRED and REPLACED by the real chest-objects**. The invented overlay painted `mapOverride` Chest(257) over **8 by-hand coords** in loc 17 floor -1, with a fabricated **daily respawn**; 3 of those coords fell on StoneBrickWall(79) → when looted it left BrickFloor(68) permanent = **"broken wall"** (#F13-1, unseen in catalogs). Replaced by the **per-entry rehydration** of the .NPC object-slots (census `re/notes/npc-object-actors.md`): upon ENTERING the castle, `Game.hydrateInteriorObjects` seeds in `worldObjects` the 3 real chests (slots 23/24/25, tile 257, coords **(16,21)/(17,22)/(13,23)** floor -1 — all BrickFloor, none over a wall) + the corpse (slot 28) as `kind:"prop"`; (O)pen opens them via the F1.5 branch (chestLoot/karma/trap) and removes the object; on EXIT they are discarded (`discardInteriorObjects`) and on RE-ENTRY they are reborn fresh (original farming mechanic, see row O6). The object-slots come from `NpcManager` (classifier `npcSlotObjectKind`, types {1,14,27,30}) → (L)ook describes them by their tile (`mapTile` branch), not by the NPC shortcut. Initial `contents` = **Class C** (O5, `INTERIOR_CHEST_CONTENTS=8`). Migration of old saves in `deserialize` (deletes `treasuryLoot` + the 257/68 overrides of the 8 TREASURY coords → the base wall rules again). Tests: `game/tests/interior-objects.test.ts` (a-h), `commands.spec.ts` (castle chest by deep-link). | `.superpowers/sdd/scout-castle-bugs.md` Anomaly 1, `scout-regen.md` Re-analysis 2, `re/notes/npc-object-actors.md` |

**Note (real treasury):** the other extra of the `scout-inverse-audit` (TREASURY_CHESTS)
is **RETIRED** in task #3 (new row below): replaced by the real chest-objects
of the .NPC with per-entry rehydration; with it the "broken wall" disappears
(#F13-1). The look-shortcut "citizen of Britannia" is **CLOSED** in task #4/#65
(previous row).

**Declared residuals of the faithful (L)ook (task #4/#65):**
- **Prefix without a final period (Class C).** The clone prints `"Thou dost see " + <LOOK2 phrase>`
  without a period (inherited format, green across the whole suite). The original has the prefix
  string in DATA.OVL (DS_off 0x751c → fileoff 0x752c; ref. LOOKOBJ.OVL.asm 0x0a40)
  as `"\nThou dost see\n"` (with initial and final `\n`, without a period) and the LOOK2.DAT
  phrases do not carry it either; the exact punctuation/break that the print of
  `look_generic` composes are not derived to the byte → measure in DOSBox whether a final "." appears.
- **Tiles with the "x" marker (faithful).** 16 tiles of LOOK2.DAT (256, 274-278, 284-285,
  372-375, 380-383) carry the literal marker `"x"` of the original file (without a real phrase);
  e.g. the StarPattern of Blackthorn (tile 256) → "Thou dost see x". It is **binary data**,
  not a gap: it is emitted as is (GOLDEN RULE). Fallback to `TileData.json` only if the asset
  `look2.json` is not loaded (harnesses that do not inject it) — never by-hand strings.

**How to come back post-100% (explicit option).** Two distinct natures: (a) the
**table food theft with karma** is pure additive clone-era (the original eats
in place) → optional "clone-era QoL" layer. (b) The **daily re-findability of the Minoc
tree (and of the 0x0d/0x0f entries)** was NOT QoL but **ORIGINAL mechanic not ported**
(gate `g_day != [0x57b2]`) → **✅ already PORTED in task #1** (`search_fixed_hidden_items`
with its 3 special gates; see the tree row above). Case (a) remains open,
retired from base mode #69.

### 3.5-C — Class C residuals of the Search gates (task #1)

Having ported the 3 special gates of `search_fixed_hidden_items`, two NON-derived gaps
remain that are declared here (GOLDEN RULE: they are not invented):

| Gap | What it is | Effect on the clone | Closure BP (DOSBox) |
|---|---|---|---|
| **`call 0x770e`** (gate 0x0d/0x0f) | ADDITIONAL precondition to the inventory gate (`cmp [g_keys],0` / `cmp [g_equip_qty+39],0`): the binary calls `0x770e` with (x,y,floor) and only finds if it returns 0 (SJOG 0x056d/0x059a). Helper semantics **underived** (empty/passable cell? object already present?). | **NOT modeled** → the clone is MORE permissive: it finds as soon as the inventory gate passes, without the cell check. | BP at `call 0x770e` (0x056d) with known arguments (x,y,floor); observe which RAM/table it consults and its 0/≠0 return. |
| **Grant of the `id5` item** (entry 0x0f, overworld 64,80) | The jump-table `get_special_item` 0x1458 for `id==5` (gap O3): what the (64,80) entry grants. | **Outside the grant's scope** → 0x0f reveals the object (tile 0x105, "Thou dost find an object!") but `applySearchGrant` grants nothing (only ItemKey ported). The gate `equipmentQuantities[39]==0` IS ported. | disasm/BP at `get_special_item` case id5 → which counter it writes to. |

---

## 4. Class D — conscious scope boundaries

**What.** Behavior of the original deliberately NOT ported by a scope decision
of the clone.

- **Ultima IV character transfer** (INTRO.OVL, U4→U5 rescale):
  derived and documented (`re/notes/gypsy.md`), not ported — the clone does not import
  U4 games. Closure: only if one wanted to support U4 import.
- **"Looking at the SUN damages the looker"** (LOOKOBJ look_sky 0x0383–0x03a4): ✅ **PORTED
  (F1.9)**. By day, `Game.look()` over the sky (tile 0x59) calls `apply_damage(active,1)`
  (kernel 0x2A52) + emits `party-changed` (redraw 0x8670): 1 literal HP, WITHOUT RNG, can
  KILL (at 1 HP → 0, status 'D', deselection of the active). The damage lives in `Game.look()`,
  not in the pure `lookSpecialDescription`, so that `look.test.ts` exercises it without effects.
  The active without selection falls to member 0 (declared Class C fallback). ⚠ The asm, with
  `g_active_char==0xff`, calls an unidentified sub (0x976c) and, if it returns 0, copies
  `g_cmb_scratch_x` — a combat scratch BYTE, **NOT a character index** — to
  `g_active_char`; the semantics are undecidable from the dumped asm, so the
  member-0 approximation (inherited from the scout-classD) is declared as such (BP pending).
  Tests: `look-concat.test.ts` (day subtracts 1, night does not damage, kills at 1 HP, fallback 0xff).
  Citation: `re/notes/lookobj.md:28-29`, scout-classD ITEM 1a.
- **"Looking at the GEM/crystal ball (0x29) damages/shows vision"** (LOOKOBJ 0x099c case
  0x29): DE-ORACLED (F1.9) but **NOT ported** (it is of the (V)iew-gem command, UI layer).
  The field `roster+0x0e` (previously marked oracle 0x55b6 in lookobj.md) = **INTELLIGENCE**
  (identified in `traps.ts:11-16`): the gem is an INT vs `rand(1,30)` contest — win →
  "Strange vision!" + 32×32 aerial view; lose → "Death vision!" + 1 HP. Class D residual:
  the 32×32 renderer is graphical (UI) and the `idx` (active vs selected, 0xffffa6f8?) and the
  trigger (View-gem command) are documented, not ported in F1.9. Closure: wire to the
  (V)iew-gem with the aerial-view UI. Citation: scout-classD ITEM 1b.
- **Z-stats display** (pagination, ♂/♀, 4 scroll lists) and **FONT engine**
  (pixel-exact justification, scene animator, 80-star night field):
  they are UI/presentation. The clone uses its own UI; the derived TEXT (endgame, look)
  IS exact. Closure: only if one wanted to replicate the EGA UI 1:1.
- **"Readied."**: ✅ **RETIRED (F1.9)**. It was a clone convenience phrase WITHOUT
  backing: the binary does NOT print anything on a successful equip (the only derived echo of the
  command is the prompt "Ready...\n\n", DS 0xa1f0). `equipItem` now returns `message:""`
  and `main.ts` does not paint the empty one. Tests: `equip.test.ts` (success → `message===""`),
  `magic-ready.spec.ts` (asserts the real effect, not a log line). Citation: scout-classD ITEM 3.
- **"the stars."**: clone convenience phrase for the (L)ook at the NIGHT sky; the
  binary prints no text (it paints an 80-star field `rand(9,182)`/`rand(9,172)` +
  zodiac). Cosmetic Class D divergence — requires the stars UI.
- **turnsSinceStart does not saturate** (the original uses u8 saturating at 255, kernel 0x2B9F @0x2b9f):
  the clone uses a wide counter (`survival.ts:209`). The CONFLATION that blocked it (the
  scout-classD feared that an enemy-tier selector indexed this counter and that
  saturating would freeze the spawns) was **RESOLVED by #16**: the clone no longer progresses era
  (nor does the binary — `encounters.ts:122`), so no gameplay subsystem reads
  `turnsSinceStart`; its only consumers are cosmetic (journal/persistence/metadata).
  Porting the saturation remains **gated by a runtime anchor** that confirms that `[0x588B]` ≡
  `turnsSinceStart` and that no other subsystem reads it (contract L0: citation A + DOSBox anchor).
  Without observable effect except extreme overflow. Citation: scout-classD ITEM 2, task #16.
- **Ring of Regeneration — per-turn HP regen** (`kernel_ring_regen 0x400C`, task #21):
  ✅ **VERIFIED, WITHOUT DIVERGENCE — the clone already ports it faithfully.** The full sweep
  (`kernel-sweep-4.md §3`) left it as "the only consumer of GAMEPLAY RNG per
  turn without a model/without a confirmed label"; the triple-cross static verification
  (`re/notes/oracle-ring-regen.md`) confirms that **`0x400C` is the regeneration of the Ring
  of Regeneration** and that `[0x55C5]` is **NOT a "secondary status"/glyph `','`** but the
  record's **RING slot** (record+0x1D; roster DS 0x55A6; HP=+0x10=0x55B8,
  MaxHP=+0x12=0x55BA, RING=+0x1D=0x55C5; item 0x2C = Ring of Regeneration id 44 — the
  same item on which the "Ring vanishes!" of ZSTATS acts, `re/verified/zstats.md:36`).
  Rule: as the LAST step of `kernel_turn_housekeeping` (0x2AE8, after poison + hunger +
  turn++ + time-spell tick), for each **non-'D'** member with ring 0x2C
  `rand(0,7)` is rolled (ALWAYS, unconditional of HP) and with `==7` (1/8) → `HP=min(HP+1,MaxHP)`
  (helper `0x3F14`). Rands/turn = |live members with the ring|, of the same seed as
  hunger (0x5420). The clone `game/src/core/world/survival.ts:219-227` traces it byte-by-byte
  (`RING_OF_REGENERATION=44`, same order, same injected `rand`) and is **wired** to the
  live turn (`world/loops/turn.ts:167/281`, `world/movement.ts:129`); the (R)eady sets
  `ch.ring` (`equip.ts` slot "ring"). **Declared frontier:** the 2nd call-site of 0x400C
  is the COMBAT status-tick (`0x6794`←0x6b6b, per actor whose character carries 0x2C) —
  combat parity = COMBAT.OVL, outside this turn. And carrying the ring additionally makes
  the sprite **flicker** (flash `0x6936`, `rand(0,15)`) = RENDER consumer that the clone
  OMITS (Class 3, task #17), unrelated to the regen. Citation: `re/notes/oracle-ring-regen.md`,
  ULTIMA.EXE 0x400C/0x3F14/0x2AE8, `re/verified/zstats.md:30-37`.
- **Frigate advance with sails hoisted = DRIFT (real-time→turn-based discretization).**
  (Phase 1.2, `navalMove`): the binary does NOT move the hoisted frigate on the aligned key.
  The key is read INSIDE `tick_and_getkey` (0x0598), which filters by heading: key==heading
  falls to the DRIFT branch (0x6C8 `je 0x5E6`) and advances ONLY if the di%3 cadence fires
  (0x64F `ja` = no drift) — when it fires, it returns the heading as "key" and the loop
  calls `outdoor_move`→`ship_try_move`→`move_party` (that 0x0542→0x050E IS the drift move,
  not a move-on-key). Key≠heading → TURNS (0x04AF sets heading + `wind_drift_ctr=0`),
  `transport_face` returns turned=1 → return without moving. The clone is a **faithful
  discretization**: 1 player action = 1 drift step (`windDriftStep` di%3, without RNG); the skiff
  and the furled frigate row on the key (`shipFacingStep`). The ONLY divergence is the
  universal real-time→turn-based one that the whole port assumes (the binary keeps drifting between
  keys; the clone advances one drift step per action). Citation: `re/notes/transport.md §2/§3`,
  disasm MAINOUT 0x0490/0x0598 (0x6C8/0x64F/0x04AF).
- **Ship sinking (COLLISION/BREAKING UP with damage ≥ hull)** (Phase 1.2): the
  hull damage is `rand(1,30)` EXACT (damage_ship 0x109E: `push 1; push 0x1e; call
  rand; sub [g_hull],al`), derived and ported. If the damage ≥ hull, the binary 0x10D6
  converts the ship (to skiff/carpet if any, or drowns the party); the F1.2 clone
  only **clamps the hull to 0** and marks `sunk` in `ShipMoveResult`, without the conversion/
  drowning. Closure: port the branch 0x10D6-0x1160 (scope F1.2-T3/T4).
- **Cactus damage to the party** (Phase 1.2, `resolveNavalStep`): the clone rolls `rand(1,8)`
  over the **active character**; the original calls 0xA8D8 (outside the OVL, without measuring whether
  it distributes to the whole party or to one). Minor Class C — measure with a BP at 0xA8D8. Citation:
  `re/notes/transport.md §2`.
  *(A4-PARITY2 D-89, 2026-10-03: MEASURED and CLOSED. MAINOUT 0x0336 `call 0xffffa8d8` is K:2AA8 party_random_damage: one rand(1,8) per living member (slot order, no draw for a dead one), the whole party, for foot, horse, carpet, skiff and a rowed frigate alike. The reference's naval path and native's `naval_step` both called it wrongly (one roll on the active member) and are fixed; the foot paths were already right. `native/core/a4-parity2-findings/D89-FINAL.md`.)*
- **Guard `floorExists` in the town stairs/ladders transition** (Phase 1.11,
  fix #46, `game.ts` `applyStairStep`/`klimbLadder`): the binary TOWN 0x052E does
  `inc`/`dec g_floor` UNCONDITIONALLY (0x0548/0x0566) — it does NOT check that the destination
  floor exists; it trusts the wall geometry to prevent the "dead-end" approximation
  (a stair is only passable from the axis that orients it). The clone adds
  an `if (!floorExists(loc, target)) return;` that does not exist in the binary. It is a
  **conscious frontier, NOT an oracle**: the original's rule is DERIVED with
  certainty (cited disasm) and the guard is only a defensive net against malformed
  map data (`getActiveMap` throws if the floor is missing). With the original maps the
  stairs are paired (verified in loc 17: z0↔z1 at (15,8), z0↔z−1 at (12,7))
  → the guard NEVER fires and the behavior is identical to the binary. Closure: remove
  the guard only if one pursued byte-exact parity of the behavior with corrupt
  data (which the original "handles" by corrupting g_floor). Citation: `re/notes/town-klimb.md §3`,
  TOWN.OVL.asm:529-560.

---

## 5. Class E — plot markers (Phase 7, not re-derived)

**What.** Phase 7 (global plot/quest) used **derived playable** locations, not
the binary's canonical ones, because the migration focused on engine and rules.

- **Shard and artifact positions** — SHARDS + AMULET REPLACED (F1.10-T2, ✅):
  the clone's "lore-coherent" fabricated coords (Underworld next to the exit
  of the corresponding dungeon; amulet next to Doom) were replaced by the REAL ones
  of the OUTSUBS 0x0566 seeder (DATA.OVL table 0x3a06 + amulet immediates). Old
  vs real (all were wrong, verified in T1):
    · Falsehood: fabricated (120,52) → real **(192,80)**, z=0xF0
    · Hatred:    fabricated (24,53)  → real **(130,65)**, z=0xF1
    · Cowardice: fabricated (216,53) → real **(176,184)**, z=0xF2
    · Amulet:    fabricated (91,56)  → real **(105,225)**, z=0xF3 (tile 0xB7)
  The seeder (Game.hydrateUnderworldPlot) plants the tiles 0xB4 (shards) / 0xB7
  (amulet) at these coords upon entering the Underworld, gated by plot (shard not-taken
  + its Shadowlord alive; amulet not-taken).
  **Pickup — UNIFIED to (G)et over the tile (F1.10-T3, ✅)**: the Search-radius
  model (searchQuestItem with an invented proximity radius) is **RETIRED** for shards and
  amulet; they are now picked up with **(G)et over the tile 0xB4/0xB7**, the REAL mechanic —
  apply_item_grant (SJOG Get 0x18ce → 0x1458) dispatches BY TILE to the plot branches
  (shard 0x16b6 sets g_shard_taken[idx]=0xff DS 0x57B6+idx; amulet 0x1712 sets
  g_amulet_lb=0xff DS 0x57B3). After the grant the layer is re-derived (same not-taken gate)
  → the tile disappears and does not re-spawn. The **z byte (0xF0..0xF3) ✅ RESOLVED-STATIC (grade A)
  2026-07-14**: the shard branch reads `[bp+6] & 3` (SJOG 0x16b9 `and si,3`) = the shard's index
  (0xF0→0=Falsehood, 0xF1→1=Hatred, 0xF2→2=Cowardice; DS 0x8D18/24/2E) — **load-bearing
  because the 3 shards share the tile 0xB4**, z&3 is their ONLY discriminator. **The high
  bits (0xF_) are NOT read ANYWHERE** (masked; sweep of readers: only the
  Get touches them, and with `&3`); inert base of the seeder. **z is NOT layer/height** — the floor
  is byte+4 (=0xFF, filtered against g_floor 0x5895 by NPC.OVL); the render uses the tile
  byte+0 (sprite+0x100), NOT byte+5. Hypothesis "layer/height?" REFUTED. The amulet
  (tile 0xB7, 0x1712) does NOT read z. `WorldObject.plotZ` remains inert-and-correct (the port already
  derives the shard by `plotItem`). Detail: `re/notes/oracle-underworld-z.md`. (Runtime
  not needed: `and si,3` unambiguous + confirmed without another reader.)
  **CROWN + SCEPTRE REPLACED (F1.10-T4, ✅)**: CROWN (tile 0xB5 → g_crown 0x57B4,
  DS 0x8D3A, branch 0x16e6) and SCEPTRE (tile 0xB6 → g_sceptre 0x57B5, DS 0x8D56, branch 0x1706)
  are the other two branches of this jump-table. Unlike the shards/amulet (seeded
  by code in the Underworld), the crown and sceptre are **OBJECT-SLOTS of the .NPC file**
  (same table 0x5C5A as the castle chests, task #3), with their `type` = plot tile:
    · Crown: fabricated Blackthorn loc 18 z3 **(15,5)** → real loc 18 (Palace_of_Blackthorn),
      slot 1, type 0xB5, **(15,13)**, z3 (the throne room, upper floor; only the Y was
      wrong: 5→13). Source: `game/assets/npcs.json` loc 18.
    · Sceptre: fabricated Stonegate loc 29 z0 (15,15) → real loc 29 (Stonegate), slot 9,
      type 0xB6, **(15,15)**, z0 (the fabricated coords turned out EXACT; only the
      model changes: from searchQuestItem to object-slot). Source: `npcs.json` loc 29.
  The sceptre is **CONFIRMED by asm**: TOWN.OVL 0x1253 (`cmp g_location,0x1d` (29) /
  `cmp g_sceptre,0` → `push 9; call 0xb0`) removes the NPC **slot 9** of Stonegate when
  g_sceptre!=0 — exactly the slot and the location derived. Clone model: objects
  kind "plot" with RAW tile 0xB5/0xB6, hydrated by `Game.hydrateInteriorObjects` gated
  by !taken (`lbArtifacts.crown/sceptre`); the (G)et picks them up (grantPlotItem) and removes
  them on the spot. The **gate !taken** reproduces the OBSERVABLE non-reappearance behavior: the
  sceptre by TOWN 0x1253 (removes the slot on loading), the crown by its own (G)et branch
  0x16e6, which does EXTRA work over the others (kernel `0xBB9E`=object-index-on-tile,
  npc.md:180, + `0xBB86`=slot removal in the table 0x5C5A) → deletes the world object.
  The **EXACT binary mechanism of the crown's re-spawn suppression** (0xBB9E/0xBB86 +
  possible npcDead mark vs a g_crown gate in the loader that does not appear in TOWN/BLCKTHRN)
  remains **Class C** (the observable behavior —present until taken, absent afterward— is
  reproduced). The fabricated Search-radius model (`questItemSpots`/`searchQuestItem`) is
  **RETIRED entirely** (function, types and imports).
  **Strings (L2) — PORTED BYTE-EXACT (T3 review fix, ✅)**: the 7 messages of the
  jump-table come verbatim from DATA.OVL via the canonical formula `fileoff = DS_off + 0x10`:
  shard = DOS prints "The Shard of\n" (DS 0x8D0A) + name (DS 0x8D18 Falsehood / 0x8D24
  Hatred / 0x8D2E Cowardice); amulet DS 0x8D74; crown DS 0x8D3A; sceptre DS 0x8D56. (The
  jump-table default DS 0x8D92 = "Nothing to get!\n" is outside this fix — the
  clone emits it via another route, without the `\n`, F1.10-T4/L2.)
  The initial ⚠ ("needs a runtime anchor, sub-string block") was an **irresolvable false
  by ANCHORING METHOD**: the dump anchored by text-search
  (`data.find("Falsehood")`), which returned a DUPLICATE copy of the string at 0x47DC instead of
  the canonical 0x8D28 (=DS+0x10), giving the spurious delta 0x454C and off-by-one in chain. The
  file was NEVER the problem: `ultima5/` and `play/` are byte-identical (same sha256).
  **Method rule** noted in `re/notes/dataovl-strings.md §Scope`: anchor ALWAYS by
  the formula `fileoff = DS_off + 0x10`, NEVER by text-search (there are duplicated strings
  in DATA.OVL).
- **Shadowlord ritual** ✅ **PORTED (F1.10-T5, grade A, without RNG)**: COMPLETE state
  machine derived from CAST.OVL 0x15b4 (Use Shard) + CMDS.OVL 0x1030 (Yell
  name). The real flow: in the room of the Flame of the opposite Virtue (Lycaeum
  loc 30 / Empath Abbey 31 / Serpent's Hold 32) the Shadowlord's name is YELLED
  (FAULINEI/ASTAROTH/NOSFENTOR) to summon it over the Flame (tile 0xFC at
  party_y-2), the ritual cell is stepped on and the Shard is USED, which destroys it if
  the tile to the north is the correct Shadowlord. Effects: `g_shadowlord_locs[idx]=0xFF`,
  consumes the shard, OR doom-bit. Pairing corrected (Astaroth↔Nosfentor
  were swapped). Byte-exact strings. floor 0xFF of the Flame of Courage
  = -1 = basement of Serpent's Hold (grade A). The AV EFFECTS (animation of raising the
  shard, pulses/flash of the Flame, SFX 0x7b66) go to the audiovisual catalog (Class C,
  tasks #11/#12), they are not fabricated. Detail: `re/notes/shadowlord-ritual.md`.
- **Urban Shadowlords (presence in towns)** ✅ **PORTED (F1.10-T6)**:
  complete derivation `re/notes/shadowlord-urban.md` (TOWN.OVL 0x11f0 tail +
  TALK.OVL consumers). The three SL roam the 8 virtue cities
  (g_location 1..8, `rand(1,8)` re-draw at midnight already ported). Upon ENTERING a
  town with an SL present (`shadowlordPresentIndex`, `checkLocationEntry` →
  `applyUrbanShadowlord`):
  1. **Physical sprite** (TOWN 0x2ae): worldObject tile 0xFC at (15, `SL_SPAWN_Y[loc]`
     of DATA.OVL DS:0x13a5, byte-exact). The sprite's AI MOVEMENT (aiType 6) is NOT
     modeled (static sprite) — **Class C** (presentation).
  2. **Announcement** (TOWN 0x11b8): `"An air of <falsehood|hatred|cowardice> doth surround
     thee..."` **[V]** byte-exact (DS 0x27b8/name/0x27c4). In **Stonegate** (loc
     0x1d=29, its lair) all THREE living ones are announced (order 2→1→0), without possession.
     The PC-speaker **beep** (0x11d5) = **Class C** (AV, tasks #11/#12).
  3. **Effects** (TOWN 0x1156):
     - Faulinei (0): shop drain ✅ (F1.7-E) + "Something was stolen!" at the end of
       each talk (TALK 0x1180, **derived**; end-of-talk wiring pending,
       partial Class C). Does not possess NPCs.
     - Astaroth (1): possesses → `dialogNumber` 0xFE (talk → `"Begone, vermin!"` — the hostile's
       phrase; the combat start by talk = opaque kernel 0xffffbb02, **Class C**;
       + a possessed-0xFE that attacks self-converts to 0xFD via TOWN 0x10da→0x8d4, combat
       AI = Class C) + aiType 7.
     - Nosfentor (2): possesses → 0xFD (talk → `"Don't hurt me! Please go away!"` **[V]**) + aiType 3.
  - 🐛 **BUG-FOR-BUG of the eligibility gate (TOWN 0x111f-0x1121)** — ported after the
    review round. The "person-tile" gate of 0x10f2 reads `g_npc_type_tbl[4]` (the type of the
    town's **slot #4**, CONSTANT), not that of the evaluated slot: `bx=cx` with `cx`=exhausted
    counter (=4), `si` destroyed in `shl si,4`. Confirmed bug (not intention) by contrast:
    0x85e/0x8d4 DO reload `[bp+4]`. Observable consequences ported:
    (a) if slot #4 is a person the gate passes for all; if not, it fails for all;
    (b) **Astaroth** (0x85e, without re-check) can possess **non-person** NPCs;
    (c) **Nosfentor** (0x8d4, with internal re-check) only possesses person-tiles, BUT if slot
    #4 is not a person it possesses **no one** even if there are eligible ones.
    **DORMANT in canonical play**: the 8 virtue cities all have slot #4 =
    person (0x50-0x70) and NPCs all person-tiles → possessed set identical to the naive one.
    The bug does NOT alter the count of 32 rand (stream parity intact). Modeled:
    `possessGateRoll` + `possessForShadowlord` (manager.ts); tests with synthetic fixtures
    (slot4 non-person → Nosfentor does not possess; slot4 person + daemon → Astaroth DOES possess it,
    Nosfentor does not).
  **RNG:** the possession consumes EXACTLY **32 rand(0,1)** from the live stream (1/slot,
  order si=0..31; 0x10f2 rolls the rand for all 32 even if the slot is empty) ONLY with
  Astaroth/Nosfentor; Faulinei/none/Stonegate → 0 rand. Stream parity
  model↔clone (`shadowlord-urban.test.ts`: 32 next(0,1), identical seed, possessed set =
  the predicted one). Live DOSBox = **Class A** (BSS seeding of small-map NPC arrays,
  like all the town-NPC — see §1). Tests: `game/tests/shadowlord-urban.test.ts` (24) +
  `game/e2e/shadowlord-urban.spec.ts` (reachability of entry with/without SL).
- **Doom-bit ↔ Windemere aliasing** (open question of F1.10-T5): **observationally
  INERT, closed** (`shadowlord-urban.md §5`). The destruction OR `[0x5bca]|=bit`
  falls in `npc_dead_bitmap[Windemere]` (base 0x5B5A) slots 4/5/6 (MSB-first) = **daemons
  dialog 0**. ⚠️ Corrected after review: it is **NOT true that "nobody reads 0x5B5A..0x5BDA"** —
  TALK 0x0d42/0x0d7a read/write the **recruit** sub-array (base 0x5BD6) by-(location,
  NPC). The real inertness: the bit is consulted **by the NPC you talk to**, and the affected
  slots are un-talkable daemons → that bit is never consulted via that route; the clone models
  the write as `shadowlordDoomBits` separately — faithful. There is NO bug-for-bug of "killing an SL
  corrupts conversable NPCs of Windemere".

**Why it is deliberate.** The migration's objective was the **exactness of the engine
and the rules** (turns, RNG, combat, shops, dungeon…), not to re-derive the plot's
script, which is data content. The clone is completable with the derived
locations.

---

## 6. Additive QoL (`❌`) — not closed, it is intentional

Additions that the original DOS did not have; they do not affect the logic:

- **Music**: community XMI with GeneralUser GS (the DOS was mute).
- **HD tiles**: xBR 4× upscaling (the 1:1 EGA atlas is also generated).
- **Smooth camera, 21×15 viewport**: presentation; the logic (LOS, distances) does not
  depend on the viewport.
- **`D` key = drink (dungeon shortcut)**: the DOS has no `D` letter; one drinks via
  `(L)ook` at the fountain ("Will you drink?", DNGLOOK 0x0134). The port adds `D`
  (`main.ts:649`) as a direct shortcut: over a fountain it reproduces the faithful prompt and effect
  (`drinkFountain`), off a fountain it responds "No fountain here." (QoL message).
  It is input convenience, without a change of mechanic nor of presentation chain over
  a fountain — same class as F5/F6/Tab. (Census: `docs/censo-teclado-refcard.md` §5.)
- ~~**Pre-block of the shot at a target behind a wall = «Blocked by wall!»**~~ **✅ RESOLVED
  (historical) — Phase 2 of faithful combat, lot 3.** The port cut with «Blocked by wall!»
  (`combat.ts:1280` / `:1573`) WITHOUT spending ammunition nor turn; the string was **FABRICATED** (it does
  not exist in DATA.OVL, only movement's «Blocked!»). It is NO longer a divergence: `playerAttack` and
  `castCombatAttack` at range aim-at-CELL and the projectile FLIES along the line (raycast
  `projectileLanding`, COMSUBS:0x0822→0x12de), stops at the 1st opaque cell and, if there is
  no one there, is WASTED (ammunition+turn spent, WITHOUT text; anim by event
  `projectile`) — faithful. The pole weapons «(p)» keep striking over the obstacle. String
  purged from `approved-strings.json` and `es.json`. Derivation: `re/notes/blocked-by-wall-fabricated.md`.

---

## 7. The 130 provisional `kernel_fn_<addr>` functions

Of the ~181 kernel prologues, **130** keep the provisional name
`kernel_fn_<addr>` (ledger at 100 %: they are `code` bytes justified by prologue, not
a coverage gap). They are **sound, graphics and I/O helpers without a game
rule**: no migrated subsystem depends on their fine semantics. The 44 that a
subsystem does exercise are named (RNG, apply_damage, chest_trap, advance_clock,
spawn_actor, wind_anim_tick, dispatcher, getkey, print…). The deliberate divergence:
**coverage, not byte-by-byte truth** of each one. How to close them: individual RE of
each body if its exact behavior were ever needed (e.g. to replicate
the audio timing or the EGA video pipeline) — today unnecessary for
rule parity. Justified in `re/COVERAGE.md §3`.

---

## 8. Index: each ⚠️ of FIDELITY → its class and closure

Every ⚠️ entry of `docs/FIDELITY.md` falls into one of the previous classes:

- **Engine/movement**: overworld wrap (A, impracticable edge with the harness),
  class-1 slow terrain nuance (A), turnsSinceStart (D), Shadowlord midnight
  relocation (B, with its subsystem).
- **Doors**: duration/tile/NPC-pass-through (C — 3 oracle questions).
- **Dialogue**: Redux port interpreter (A), gold-demand quantity (A), post-pay karma
  branch (B), NPC 0xFD-FF hardcode (B), fallback texts (C), floor karma
  0x7FB6 thunk (A).
- **Karma**: shrine donation (A, provenance resolved), price curve (C), range.
- **Shops**: INT haggling / reagents / guild / transport / healer / inn /
  tavern (A), Falsehood drain (B wired).
- **NPCs**: schedule/wander/distance/floor-gate/AI-types/teleport (A),
  refusal texts (C), pass through doors (C).
- **Combat**: hit formula ✅ RUNTIME; rest of mechanics (A); AI movement
  (D — rand stream not capturable by pty, seeded already-in-melee); OUTSUBS levels
  0x658 (A/B, not re-derived).
- **Magic**: dispatcher + formulas (A); 33 effects (A/B depending on whether they touch the map); In Quas
  Xen/Wis (C, low confidence).
- **Dungeons**: traps/fields/fountains (A); Klimb (A); render flicker (D cosmetic);
  wandering monster (A/B); light/raycast (A).
- **Transport**: wind ✅ RUNTIME; push/drift/broadside/Cape (A); ship_try_move
  ✅ **WIRED (F1.2)**; broadside-world-turn ✅ **WIRED (F1.2)**; player
  sinking ✅ **WIRED (F1.2)**; pirate ship hull (0x64) ✅ **DERIVED (F1.2)**.
  Open: organic ship acquisition (B, object layer → 1.5/1.8); drowning
  party-wipe/HP (C, §3).
- **Shrines**: lunar phases ✅ RUNTIME; moongate destination/edge/donation/state
  machine/restore/well (A); Codex ceremony + horse spawn (B); live teleport (A).
- **Commands**: Klimb/Jimmy/NewOrder/trap/loot/Ignite/Search ✅; town stairs
  automatic on walking + town Klimb (TOWN 0x0810/0x052E/0x0B82) ✅ **PORTED
  (F1.11, fix #46)** with the `floorExists` guard (D §4); the Klimb getdir cancel **charges
  a turn** (0x0C3E) vs "What?" which does not — ✅ **PORTED (fix #50)**, and the same cancel
  in the **other 7 directionals** remains an unmeasured oracle (C §3, non-uniform
  return convention + resident dispatcher not dumped); Push/Camp/search_dungeon/
  chest-objects (A/B).
- **Blackthorn**: capture/interrogation/refuge/guards/drain ✅ (model). **REFUGE
  WIRED TO LIVE PLAY ✅ (F1.7-T1, piece D)**: `game.checkRefuge()` intercepts the
  total death (`partyConsciousState==-1`, kernel 0x39fc) and calls `partyRefuge()`
  (waking at LB 0x11/floor 1/(10,10), revive, karma≥75, clock 6:00, food 63).
  It is NOT a game-over and is NOT gated by Blackthorn's location (unlike the
  capture 0x12ae). The death-check is hooked at **4 points, but ONLY 3 are
  call-sites with asm citation** (the loop checks `-1` after each context's turn):
  overworld `runContextTurn` (**MAINOUT 0x0ac2**), town `runContextTurn` (**TOWN
  0x1436**), dungeon `dungeonCommand` (**DUNGEON 0x1014**). The **4th — `endCombat` —
  is a DECLARED ARCHITECTURAL COMPENSATION, not a binary call-site**: in the
  clone combat is a decoupled MODAL state (not a subroutine nested under the
  loop as in the asm), so the loop's death-check does not cover it by inheritance;
  it is replicated at combat close with an identical destination (0x0910). Verified by the
  reviewer: exhaustive grep of COMBAT.OVL → 0 hits of party_refuge. **Narration now
  byte-exact from DATA.OVL (grade A)**, not the Class D paraphrase of the notes:
  `Game.REFUGE_NARRATION` = DS 0x70e2/0x7108/0x7122/0x7152/0x7172/0x719e/0x71cc/
  0x71ea (kernel_print_ds 0x75c0, fileoff=DS+0x10); LB's greeting indexed by
  karma (0x0b03, DS 0x71c2="KARMA.DAT") remains cosmetic, not ported. Tests:
  `refuge-live.test.ts` (8: town/overworld/dungeon/combat + narration +
  no-triggers). When the refuge fires from a dungeon, `checkRefuge` clears
  `dungeonState` (BRIT.DAT loads a small map → exits dungeon mode). Pending
  of the subphase: live capture/interrogation (A), password (C/B), drain (E),
  guardWander (B). Class D remains the exact minute/day of the clock (loop
  advance_clock(9)); Class C the revive status byte (kernel 0xdc66) and g_floor
  of the deposit; Class B the password gate; cannon thunks (helpers §7).
- **Gypsy**: seed==0 ✅ RUNTIME; deterministic bracket (A, BP pick_virtue in C);
  U4 transfer (D).
- **Endgame/Zstats/Look**: encumbrance/playtime/look/trap-detect (A); **sun-damages ✅
  PORTED (F1.9, A — apply_damage(active,1) in Game.look())**; crystal ball/View-gem
  (D — de-oracled: roster+0x0e=INT, not ported because it is UI); Z-stats UI (D);
  **Ready-RNG wired ✅ (F1.6, A)**; **"Readied." retired ✅ (F1.9)**; trapCheck wired (B).
- **Context loops** (FIDELITY lines "Live stream unification",
  "Details → F.2" and "Ready RNG wiring"): RNG order per turn + exact
  spawn ✅(asm+cross) and the MODEL↔CLONE unification closed by the master scenario; the
  **LIVE stream unification of `game.ts`** ✅ **CLOSED in Phase 1.1** (§2, first
  bullet): `game.ts` consumes a single ordered `OriginalRng`, with exact `pick_spawn_coords`,
  `[0x65BF]`, Quickness/mount, SL midnight re-roll (overworld/town AND
  dungeon) and `guards.ts` facing (X-only by asm) all resolved, and E2E determinism
  by seed (`determinism.spec.ts`). Only `troll_toll` Y/N remains open → Phase
  1.3. **Ready-RNG ✅ closed in F1.6** (`game.readyItem` with `randRange:this.rand`);
  trapCheck remains **Class B** (§2).
- **Plot**: shards/ritual/wanderers (E).
- **Audio/Assets**: music/tiles/camera (❌ §6); EGA palette (C).

**Conclusion.** After F.2 no orphaned ⚠️ remains: each one is `✅`, or a
deliberate divergence of class A–E / QoL with its closure path noted. The project
closes with **the ledger at 100 %, the rules asm-derived with citation, 6 subsystems
verified live, the rest in model↔clone stream parity, and the ORDER of the binary's
stream demonstrated with a single seed threaded through the pure engines** (critical
path gypsy→endgame). After **Phase 1.1** (PORT of the live stream), the playable clone
(`game.ts`) **already consumes a single ordered `OriginalRng`** — E2E determinism by seed
demonstrated (`determinism.spec.ts`); only the rest of the interactive wiring remains as **Class B**
(naval transport, mantras, chest-objects, Blackthorn, Y/N
prompts incl. `troll_toll` → Phase 1.3) with its closure path noted. **Ready-RNG ✅ closed
in F1.6** (`game.readyItem`, "Ring vanishes!" 1/16 by the live stream).

---

## Addendum — SWEEP lot 1 (task #16): RNG consumer in the render path (`0x6936`)

**Uncataloged mechanic found** when reading `party_anim_build` (`0x6936`,
`re/notes/kernel-render-sweep.md §2`). It is not a divergence of the existing port (the
port does **not** replicate this), but an **RNG consumer of the game in the render path**
that must be decided whether it affects the stream parity:

- On each assembly of the party's sprite table, for **each member with the field
  status char+0x1d (`0x55c5`) == `0x2a`('*') or `0x2c`(',')**, the kernel rolls
  **`rand_range(0,0xf)` (`0x2092`, THE game's rand, `rng.md`)**; if `0xb` comes up
  it fires a flash (string 0xa422 + tone `0x43ae` + `0x6e60`).
- **Parity risk:** `0x6936` runs in the **render** chain (`0x6bee`), not once
  per turn. If a party member has that status, each redraw consumes ≥1
  rand → it can **misalign the stream order** relative to the clone (which does not consume it).
- **Provisional class:** B (interactive wiring not yet ported) / to be confirmed against
  the oracle: **how many redraws occur per turn** and **whether the `*`/`,` status occurs on the
  critical path** (probably only with a poisoned/afflicted member). On the safe side
  today because the clone does not emit it (it does not add spurious rands), but **the original DOES
  consume them** → the clone could end up *behind* in the stream when there is status.
- **Closure path:** measure in DOSBox with a poisoned member standing still (nº of rands
  consumed by N redraws) and decide whether the clone's `OriginalRng` must emit those
  flash rands to keep the order. Pending SDD task (the team-lead creates it).

Note: the other suspects of the lot (the "ambient ticks" `0x5394`/`0x4102`) were
verified **WITHOUT RNG consumption** — see `kernel-render-sweep.md §5`.

---

## Addendum — SWEEP lot 2 (task #16): second RNG consumer in render (`0x6bc2`)

**Uncataloged mechanic found** when reading `render_animated_tile` (`0x6bc2`,
`re/notes/kernel-sweep-2.md §6`). It is the **second** RNG consumer of the game's stream
in the render path (the first was the status flash of `0x6936`, lot 1), and
**much more frequent** than that one.

- `render_animated_tile` executes for **each animated tile** that enters the viewport
  (water, flags, fountains, fire…), not once per turn.
- Per tile, if its number of frames `∉ {1,8,0x10}`, it rolls **`rand_range(1, frames)`
  (`0x2092`, THE game's rand)** for the starting frame; and **if `[0x5959]!=0` it rolls
  a SECOND time**. Furthermore, with the flag `[bp+6]&4`, it shuffles 16 elements with a
  **Fisher-Yates that consumes up to 16 `rand_range(0,0xf)`**.
- **Parity risk:** if the clone does not emit these rands, it will be **misaligned from the
  stream** as soon as there is ≥1 visible animated tile (a very normal case). It is more serious than
  the `0x6936` finding because the trigger is not conditional on a rare status, but
  on the content of the viewport itself.
- **Provisional class:** B (render wiring not yet ported). On the safe side today
  only if the clone does **not** replicate any tile animation by RNG (it probably uses
  time-based deterministic frames). **To be confirmed against the oracle**: how many
  animated tiles there are per typical screen and how many rands `render_animated_tile` consumes per
  redraw, and whether `[0x5959]` (double roll) is active in normal play.
- **Closure path:** hooks with task #17 (RNG in render). Measure in DOSBox the consumption
  of rands per redraw with N animated tiles on screen and decide whether `OriginalRng`
  must emit them. Pending SDD task (the team-lead creates it).

Relation: `0x6bc2` **calls** `0x6936` (which already consumes its own flash rand), so
a redraw with status `*`/`,` **and** animated tiles accumulates both consumptions. The
render chain `0x6bc2 → 0x6936 → 0x2092` is the surface to audit for the stream parity.

---

## Addendum — RESOLUTION task #17 (oracle, 2026-07-14): RNG-in-render = DELIBERATE DIVERGENCE (non-deterministic)

Oracle verdict on the TWO previous addenda (`0x6936` flash + `0x6bc2`
tiles) **plus a third found when tracing callers**: `0x2f62`.
Measured live (`re/tools/flash_rng_probe.py`, `re/tools/render_rng_probe.py`).
Full detail with raw numbers: **`re/notes/oracle-flash-rng.md`**.

> **⚠ CORRECTION (Task #20):** this addendum called `0x2f62`
> "sprite_frame_randomizer". It is **wrong**: the disasm of `0x2E96` (`mov
> [g_wind(0x5892)],al`) proves that `0x2f62` is **`kernel_maybe_change_wind`** (as
> `re/ledger/coverage.json` already names it, and as
> `test_transport_parity` labeled it). On the 1/64 hit it calls `0x2E96` = set_wind, which WRITES
> the wind. It does not change the non-determinism verdict — it does AGGRAVATE it (see the block
> "OBSERVABLE Divergence" below). Re-derivation: `re/notes/transport.md §4.1`.

**The three consumers touch the global seed** `g_rng_seed` (`0x5420`) via `call 0x2092`:

| Routine | Roll | Trigger | Cadence |
|--------|--------|---------|----------|
| `0x2f62` `maybe_change_wind` (→`0x2E96` set_wind, `mov [g_wind],al`) | `rand(0,63)`; 1/64 CHANGES the wind | EVERY IDLE animation tick (only caller `0x5910`) | **real time** |
| `0x6bc2` animated tile | `rand(1,frames)` ×(1-2, `[0x5959]`=1 in play → 2) +Fisher-Yates | per animated tile, on full redraw `0x5f86` | per-move + content |
| `0x6936` flash | `rand(0,15)`/member `*`/`,` | afflicted member where the party is a sprite | per-redraw |

**FINAL classification: Class 3 — DELIBERATE DIVERGENCE (NON-DETERMINISTIC), not Class B.**
The cadence depends on **real time** (animation timer) and on the **content of the
screen**, not on turn events. Measured: in a STILL overworld (no key) the seed
advances (~1.4 rands/BIOS tick by `0x2f62`) with the flash at **0 hits** and `0x6bc2` at **0**
(it only fires on a redraw by movement). The flash is **inert in overworld** (party =
a single avatar). ⇒ **the clone must NOT emit these rands** (and it does not); trying to
replicate them would be impossible (they depend on the wall clock).

**OBSERVABLE Divergence (not just of stream) — ROW (Task #20):**

| Divergence | In the ORIGINAL | In the CLONE | Class | Closure path |
|-------------|----------------|------------|-------|---------------|
| **Wind by wall clock in idle** (`0x2F62 maybe_change_wind → 0x2E96 set_wind`; `transport.md §4.1`) | the WIND changes by wall clock in idle: 1/64 per animation tick (`0x5910`), **without player action** — a still player with a frigate sees `g_wind`/the heading indicator change | the wind changes only **by consumed turn** (`game.ts tickTurn`, 1 rand(0,63)/turn) — standing still, the wind does NOT change | **DELIBERATE / NON-DETERMINISTIC** (consistent with #17) | **DECLARED**: the idle tick is not emulated (emulating it requires an animation clock in the core and reintroduces non-determinism). Safe side: the clone never invents changes that the original would not make in a turn |

Detail/evidence: on correcting the label (`0x2f62` = `maybe_change_wind`, not
cosmetic), consumer #1 stops being "cosmetic" and starts to mutate **game
state**. Measured: `0x2f62` fires ~38 times / 150 idle resumes WITHOUT a key
(`oracle-flash-rng.md §3`) and 1/64 of those change the wind ⇒ the wind change in
idle is real, not theoretical. Emulating the idle tick would require an animation clock in
the core and would reintroduce non-determinism — it is discarded unless the F3 mirror asks for it.

**Why it does NOT break the already-verified parity:** the live verification of F.2 **never
claimed** byte-by-byte alignment of the live stream against DOSBox (so it is recorded in
`re/verified/loops.md`). What was verified live is **immune by construction** to these
consumers: `test_world_tick_rng_orbit_live` requires membership in the orbit within
**1..MAXSTEPS forward-steps** (the render rands only add steps), `test_rng_parity`
verifies the formula by ordered membership, `test_wind_value_live` **re-seeds** at the
consumer's entry, and combat parity **patches** the animation consumers
(`patch_sprite_rand`/`patch_anim_rand`). The clone's seed-exact parity is **model↔clone
(level 2)**, which never sees render rands. **There is no contradiction.**

**Implication for the F3 mirror:** no checkpoint can be seed-exact byte-by-byte
against DOSBox in an animated scene (≈all of them). Anchor on **forward-membership in the orbit**,
on **game state** (positions/HP/result), or **re-seed** by decision. The
afflicted-party subset is NOT special: **the whole** render path is non-deterministic
vs the wall clock — this **generalizes** the already-existing stance, it does not create it.

**SDD — CLOSED in Task #20:**
- ✅ **Neutralization completed**: `combat_parity.patch_render_rand` adds the flash
  `0x6a1a` (`mov ax,0xFF`, `0xFF!=0xB` ⇒ never flashes) and the 3 rolls of `0x6bc2`
  (Fisher-Yates `0x6C23`→0 because it is a swap index in a local array of 16 words; frames
  `0x6CA7`/`0x6CBB`→1), invoked in `capture_trace`. Outside `patch_anim_rand` on
  purpose (`flash_combat_probe.py` isolates it). PURE block 249 green.
- ✅ **Transport RESOLVED**: the label `WIND_TICK=0x2F62` in `test_transport_parity`
  was the **CORRECT** one (`maybe_change_wind`); it was THIS addendum that mislabeled it
  as sprite randomizer. `0x2F62 → 0x2E96` writes `g_wind`; `test_wind_value_live` is
  valid by mechanism (forcing the seed at the entry of `0x2F62` forces the rand of the
  wind's own consumer). See `re/notes/transport.md §4.1`.

---

## Addendum — SWEEP lot 3 (task #16): two more RNG consumers (same verdict)

Reading lot 3, **two additional consumers of `g_rng` (`call 0x2092`)** appear beyond
the three above. Both fall under the **same already-issued verdict** (task #17: Class 3,
non-deterministic deliberate divergence) — they do not open a new class:

| Routine | Roll | Trigger | Cadence |
|--------|--------|---------|----------|
| `0x4552` `anim_script_tick` | `rand(0,0xff)` in the blink guard + opcodes 5/6 of the bytecode | per animated actor with an expired timer, on the anim tick (caller `0x5941`) | **real time** (depends on which animated tiles there are and their phase) |
| `0x3072` `fx_flash_border` | `rand(0x13,0x96)` for the **pitch of the PC-speaker beep** | perimeter×8 per invocation of the effect (spell/moongate/damage) | discrete event, frozen state |

- **`0x4552`** is the bytecode interpreter of tile/creature animation (flags,
  water, fountains, torches flickering). Like `0x6bc2`/`0x2f62`, its cadence depends on the
  **screen content and the animation clock**, not on turn events → same
  non-determinism argument. The clone must **not** emit it. Detail: `kernel-sweep-3.md §1`.
- **`0x3072`** consumes rands only during a full-screen effect with the game
  frozen; the result (post-effect) is a seed advance dependent on the drawn
  perimeter. Cosmetic; same stance. Detail: `kernel-sweep-3.md §8.4`.

**Pending SDD (extends that of task #17):** if any combat/world parity trace
can have animated tiles via `0x4552` on screen, its `rand` sites should also enter
the neutralization set `patch_anim_rand` (today `ANIM_RAND_SITES` only covers
`0x6bc2`/`0x6936`/`0x2f62`). Not urgent (the clone does not port the `0x4552` bytecode).

---

## Addendum — SWEEP lot 4 (task #16): 6th RNG consumer in render + 1 of gameplay

The closure of the kernel's level A (`kernel-sweep-4.md`) adds **one more consumer of `g_rng`
in the render path** (same task #17 verdict, Class 3 non-deterministic) and **detects
one of GAMEPLAY per turn** that is of another class (it does need parity):

| Routine | Roll | Trigger | Class |
|--------|--------|---------|-------|
| `0x51b8` `tile_interact` (helper `0x51a0`) | `rand_range(0,3)` for the **animation phase** of an edge tile | for each coast/animated-water tile that the auto-tiler resolves in the viewport composition | **render/tick** — the clone OMITS it |
| `0x400c` `kernel_ring_regen` | `rand_range(0,7)`, acts if `==7` (~1/8) | per party member with secondary status `[0x55c5]==0x2c`, once per turn | **gameplay/turn** — the clone MUST model it |

- **`0x51b8`/`0x51a0`** is the **6th** RNG consumer in render after `0x6936`, `0x6bc2`,
  `0x4552`, `0x3072`, `0x2f62`. Its cadence depends on the screen content (how many
  animated-edge tiles there are), not on turn events → same non-determinism argument.
  Freezable with `g_time_spell==0x54` (gem/spell). The clone must **not** emit it. If a
  world parity trace had coast auto-tiling on screen, its `rand` site
  (`0x51a0`) should enter the neutralization set `patch_anim_rand` alongside
  `0x6bc2`/`0x6936`/`0x2f62`/`0x4552`. Detail: `kernel-sweep-4.md §1`.
- **`0x400c`** is **different**: it consumes `g_rng` in the TURN LOGIC (status
  recovery/regeneration, prob 1/8 per afflicted member), not in render. **It does advance the
  gameplay seed** and therefore **is relevant for world rand-stream parity** — it is not
  a deliberate divergence but a mechanic to model. ⚠ **Its label `ring_regen` is a
  hypothesis; verify in the oracle that it writes `0x3f14` and that it is `[0x55ba]`** before
  porting. Detail: `kernel-sweep-4.md §3`.

## Addendum — combat cross-check T9 (2026-07-22): post-victory ESC IN A ROOM withdraws (the binary says "-Not here!")

**Firm derivation** (CMDS.OVL `0x17ec`, reached by combat ESC via COMBAT `0x09dc`
→ kernel stub `0x7d8e`): it prints "Escape" (DS 0x4574) ALWAYS (0x17f4) and then:
room (`g_unk_58a1&0x80`, gate 0x1822 — BEFORE the victory one) → "-Not here!" (DS 0x457b)
and NEVER withdraws in a room (the exit is by walking along the edge or Klimb); enemies alive
(0x183a) → "-Not yet!" (DS 0x4587) without effect; victory in the FIELD → '!' (0x1853) and
total withdrawal.

**Divergence kept in the port**: with the room WON, the port's ESC STILL
withdraws the whole party and closes the scene (prior behavior, esc-wire wiring). The
echoes "-Not here!"/"-Not yet!" of the mid-fight ARE traced (kind "echo"); what diverges
is ONLY the fast post-victory withdrawal IN a ROOM.

**Why**: the room chapters harness (`conquerRoom`/`resolveArenaCombat`, nav.ts)
closes conquered rooms with ESC; several sealed rooms are WALLED-IN pockets without
a reachable edge (ch35 Hythloth r7: verified — with the faithful gate the resolver gets stuck and
the seal THROWs). Applying the faithful gate requires re-deriving the harness closure
(exit via interior structure/Klimb) and RE-SEALING the room chapters → the lead's re-seal window,
not a fix lane. Closing it = implementing `roomCombat → "Escape-Not here!"
without withdrawal` in `Combat.playerEscapeQuick` (combat.ts) and re-validating the room chain.
