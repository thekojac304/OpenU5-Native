> English migration of `re/notes/combat.md` (the Spanish note in the private repo remains the source of truth; translated 2026-07-22).

# Exact combat: COMBAT.OVL + COMSUBS.OVL (Task 3.2)

EXACT re-derivation of the tactical combat engine from the disassembly.
Addresses: `COMBAT.OVL` fileoff (CS = fileoff + 0xA290, load_seg 0xA29);
`COMSUBS.OVL` fileoff (CS = fileoff + 0xE1E0, load_seg 0xE1E); kernel =
offsets from the ULTIMA.EXE pool (fileoff = pool + 0x800). Pascal convention:
the FIRST push is the left-hand argument. `rand(lo,hi)` =
`rng_rand_range` kernel 0x2092, both inclusive (re/notes/rng.md).

Port: `game/src/core/combat/formulas.ts` (pure formulas) +
`combat.ts` (engine). Parity: `re/parity/combat/` + test_parity.py.

## 0. Kernel RNG helpers (used throughout combat)

- `kernel_rand0(n)` — kernel **0x3AAE**: `push 0; push n; call 0x2092`
  → `rand(0, n)` BOTH inclusive (3ab1-3ab7).
- `kernel_rand30()` — kernel **0x3ABE**: `v = rand0(0x3C) >> 1`
  (3ac9-3ad3, sar with cdq correction = division rounded toward 0);
  `if v == 0: v = 1` (3ada-3adc). Returns **1..30**; the 1 comes out with
  probability 4/61 (r∈{0,1,2,3}), the rest 2/61.
- `kernel_flash(n)` — kernel 0x3AE6: n flashes+beep (no RNG).

## 1. Combat state in the DGROUP

### Combatant records — DS:0xBA14, 32 × 8 bytes

Slots 0..5 = party (kernel 0x6506 kind=1 starts at 0), 6..31 = enemies
(kind=0 starts at 6; 0x652c). Fields (evidence: 0x6506 + uses):

| off | field | notes |
|-----|-------|-------|
| +0 | current HP (byte) | enemies: HP = ENEMY_STATS[type].hp fixed at spawn (65cb); players: HP lives in the roster, +0 is unused |
| +1 | effective speed/dex | player: roster DEX (65a0); enemy: `dex + rand0(7) − 4`, and if the result (byte) > 30 → dex unmodified (65d2-65f1) |
| +2 | flags | 0x80 player, 0x40 enemy, 0x20 dead/cleared, 0x10 invisible, 0x08 asleep, 0x04 "dragged under", 0x02 fleeing, 0x01 charmed/possessed (switches side) |
| +3 | player: roster slot; enemy: monster type (0..47) |
| +4 | index into the object table 0x5C5A |
| +5 | initiative counter (countdown) = `0x24 − speed` at spawn (65ad/65f6) |
| +6/+7 | x, y (0..10) |

### Object/sprite table — DS:0x5C5A, 32 × 8 bytes

[+0 tile, +1 tile2 (0 = invisible), +2/+3 x,y, +4 floor, +5 aux
(HP-copy at spawn; **chest contents** on death), +6 sprite state
(0xFF asleep, 0x20 revealed), +7 last target (0xFF = nothing)].
Enemy sprite = `type*4 + 0x40` (0x6643).

### Combat map — DS:0xAD14, 11 rows × stride 32

Floor tile at `[0xAD14 + y*32 + x]` (16a1-16aa and passim).
.CBT triggers: DS tables 0xAE1F/0xAE27 (x,y), 0xAE3F/0xAE47 and
0xAE5F/0xAE67 (2 destination positions), 0xAD1F (new tile) — COMBAT:0x111A.

### New globals (added to globals.json)

| DS | name | evidence |
|----|------|----------|
| 0x589E | g_cmb_actor | index of the active combatant (loop 0x0B94: 0xbca/0xd08) |
| 0x589D | g_cmb_weapon | weapon/spell of the strike in progress (0x0D3C:0d7b; 0xFF = hands) |
| 0x588F | g_cmb_is_spell | 1 = the attack is a spell (COMBAT 0x8f0:091d-0920) |
| 0x5890 | g_cmb_is_magic | 1 = magic weapon (id ≥ 0x23) or spell (COMSUBS 0x0C52:0c5f) |
| 0x58A2 | g_cmb_result_flags | result bits of the strike: 1 killed, 2 vanished, 4 slept, 8 poisoned, 0x10 escaped-map, 0x20 grazed (0x1574:159e, 0x68AE:6900…) |
| 0x58A8 | g_cmb_last_attacker[32] | byte per combatant: who hit it last (0x0226:0345, cleared to 0xFF in 0xc9d) |
| 0x5876/0x5878 | g_cmb_scratch_x/y | exit cell of 0x07D4/0x120E/0x12DE and live counters of SJOG:0x1B6C (live enemies/players in 0x0B94) |
| 0x5882 | g_cmb_action_count | actions since the last minute; at 10 → advance_clock(1) (0xc64-0xc76) |
| 0x58A3 | g_cmb_victory_flag | 1 = "VICTORY!" already announced (0xbb9/0xcfd) |
| 0x5899/0x589A | g_cmb_aim_x/y | Aim! cursor (COMSUBS 0x0504:0578-057f) |
| 0x55C0+slot*0x20 | g_char_defense | roster+0x18 ("unknown" in docs §4) = **cached defense** of the PC (0x12B0:136d uses it as armor; kernel 0x6DA8 recomputes it) |

## 2. Main loop and INITIATIVE — COMBAT:0x0B94

```
for idx = 0..31 (0xbca..0xd13):
  rec = 0xBA14 + idx*8
  if !(flags & 0xC0) or (flags & 0x20): next            ; 0be3-0bf3
  if player and roster.status == 'D': flags|=0x20 + 0x1574(idx, 99)  ; 0bfa-0c1f
  if tile under the actor & 0xFE == 0x84: next           ; 0c26-0c3e (cell 0x84/0x85: loses the turn)
  rec[5]--; if != 0: next                                ; 0c43-0c48
  rec[5] = 0x24 − rec[1]                                 ; 0c4b-0c50  ← THE INITIATIVE
  clear g_5898/g_58A2/g_588F/g_5890/g_589D               ; 0c53-0c61
  g_5882++; if == 10: g_5882 = 0; advance_clock(1)       ; 0c64-0c76 (kernel 0x4F7C)
  if kernel_0x5646(idx): AI turn (0x03F4) else player turn (0x063E)  ; 0c84-0c90
  g_cmb_last_attacker[idx] = 0xFF                        ; 0c9d
  0x1B1E(idx)  ← terrain/field damage on closing the turn ; 0ca3
  SJOG:0x1B6C → recount live in g_5876 (enemies) / g_5878 (players)
  if enemies == 0 and players == 0: defeat (ret 0)       ; 0cb0-0cb7
  if enemies == 0: SJOG:0x21CE; if it returns −1 → "BATTLE IS LOST!" and exit  ; 0cca-0ce6
  if enemies > 0 and players == 0 and !g_58a3: "\nVICTORY!\n"… and CONTINUE  ; 0ce8-0d05
```

**EXACT initiative**: each combatant carries a countdown (rec+5); the
loop walks the 32 slots in order and decrements; whoever reaches 0 acts
and their counter reloads to `36 − speed`. The player's speed is
their DEX; the enemy's is `dex ± rand0(7)−4` fixed on entering combat
(kernel 0x6506). Ties: the lowest slot wins (players go
first). The clock advances 1 minute for every 10 actions.

Note: g_5876 = live enemies and g_5878 = live players (the names
"VICTORY when 0 remain in 5876" are read in 0xca9-0xd05). Combat does NOT
end on winning: it continues to collect chests; it exits via SJOG:0x21CE = −1
(party dead/fled) with [bp−2]=1 ("lost") or 0 if everyone died.

### Side of a combatant — kernel 0x5646(idx)

- dead (0x20) → 0.
- player (0x80): AI if `flags & 1` (possessed) **or** if `slot != 0` and
  `name[4] == 'j'` (0x5674-0x568b, DS 0x55AC+slot*32) — the detection of
  **Saduj** (the traitor: 5th letter 'j'), who fights against the party.
- enemy (0x40): player side if `flags & 1` (charmed).

## 3. Strike: hit — COMBAT:0x14D6 (hit roll)

`hit(defender=[bp+A], attacker=[bp+8], _unused, weapon=[bp+4])`, ret 8.

```
if g_588f (spell):                                    ; 14e6
    spells 0x2A..0x31 and 0x33 → AUTOMATIC HIT        ; 14ed-14fd
    rest: stat_def = stat_atk = INT (sel −1)          ; 1506-150c
else (physical weapon):
    weapons 0x27 (glass sword), 0x23 (chaos sword), 0x28 (jeweled sword)
      → AUTOMATIC HIT                                 ; 1512-1522
    rest: stat_def = defender's DEX (sel −2), stat_atk = per weapon  ; 1524-152c
a = stat(defender); b = stat(attacker)                ; 1535-154a (via 0x13E2)
threshold = (a − b + 30) / 2   (division rounded toward 0) ; 154d-155b
HITS if kernel_rand30() >= threshold                  ; 155e-1566
```

- **Settles the FIDELITY question**: it uses BOTH dexterities.
  P(hit) with equal stats ≈ 53% (r ≥ 15, r ∈ 1..30).
- The attacker's stat depends on the WEAPON (0x13E2:141e-1426): if
  `spellAttackRange[weapon−1] == 8` (table DS 0x169C 1-based, blunt
  weapons: spiked helm 0x03, spiked shield 0x06, club 0x12,
  mace 0x18, 2H hammer 0x1F) it uses **STR**; the rest use **DEX**.
  Enemy attacker: STR if flag LE 0x0080 ("bludgeons"), otherwise DEX
  (0x13E2:1403-140f with sel 0).
- Defender's effective DEX (0x139A): **1** if asleep (flag 8), if it is
  type 0x1A (Mimic), or if it is an enemy under Time-stop ('T'); otherwise rec+1.
- A defender with dex 31+ above the attacker's stat is untouchable
  (threshold > 30); an attacker 28+ above always hits (threshold ≤ 1).

## 4. Strike: damage — COMBAT:0x12B0

`dmg(attacker=[bp+6], defender=[bp+4])`, 2 rands at most:

```
if attacker is enemy (flag 0x40): base = ENEMY_STATS[type].damage (FIXED)  ; 12be-12d3
else (player), per g_589d (weapon):
    0x27 glass sword: print "Thy sword hath shattered!\n" (DS 0x6F1A) +
        kernel 0x6E60(attacker, 0x27) (unequips/destroys) and base = 99  ; 12e0,130e-131e
    0x28 jeweled sword: base = 0                                        ; 12e5,1338
    0xFF bare hands: base = 1                                           ; 12ea,1330
    rest: base = ATTACK_VALUES[weapon] (DS 0x15FC);
        if base > 1 and base != 99: base = rand(1, base)               ; 12ef-130c
if base == 99 → returns 99 WITHOUT subtracting defense (glass/chaos ignore armor)  ; 1323-132e
defense = enemy: ENEMY_STATS[type].armour (13bf); player: g_char_defense
    (roster+0x18 cached, 0x55C0+slot*32)                               ; 1340-1373
if defense != 0: dmg = base − rand(1, defense)                         ; 137c-138c
returns dmg (may be NEGATIVE → "grazed")
```

**STR does not factor into damage** (only into the hit of blunt
weapons). The enemy's damage is not rolled: it is the fixed stat minus
`rand(1, defense)`.

## 5. Strike resolution — COMBAT:0x194A

`strike(attacker=[bp+4], defender=[bp+6])` — called ONCE the hit is DECIDED:

```
if attacker is enemy with flags LE & 0x204 (poisonous):                ; 195d-1984
    if rand(0,3) != 0 (3/4): POISON ATTACK (0x18BA) and done          ; 1986-199d
if attacker is Gazer (type 0x1C) and defender is not asleep: kernel 0x68AE
    (SLEEP the defender) and done                                      ; 19ab-19cb
if g_5890 and g_589d == 0x34 (spell In Zu): sleep and done             ; 19d6-19ea
if g_5890 and g_589d == 0x33 (poison spell): poison attack and done    ; 19ee-19fa
dmg = 0x12B0(attacker, defender)                                       ; 19fc-1a05
if dmg < 0 and defender is player: g_58a2 = 0x20 (grazed) and done     ; 1a08-1a1f
xp = 0x1574(defender, dmg)   ← applies the damage; returns the XP value ; 1a22-1a2b
if attacker is player: counter_add(exp, xp, 9999)                      ; 1a2e-1a51 (exp = roster+0x14)
```

### Poison attack — COMBAT:0x18BA

```
if defender is player with status 'G': status = 'P', "<name> is poisoned!\n",
    g_58a2 = 8; NO damage                                  ; 18c9-1902
else: dmg = rand0(0x14) (0..20); xp = 0x1574(defender, dmg);
    if attacker ≥ 0 and is player: exp += xp (cap 9999)    ; 1904-193e
```

## 6. Apply damage, death, XP and loot — COMBAT:0x1574

`apply(defender=[bp+6], dmg=[bp+4]) → xp`:

```
if dmg < 1: g_58a2 = 0x20 (grazed); dmg = 0                            ; 1598-15a3
PLAYER (flag 0x80):                                                     ; 15ab
    HP (word roster 0x55B8+slot*32) −= dmg                             ; 15b1-15c3
    if HP < 1 or dmg == 99: HP = 0, flags|=0x20, status = 'D',
        object → tile 0x1E (corpse), if it was the active one → active = 0xFF   ; 15c5-1604
    redraw; returns 0 (enemies gain no XP)                             ; 1609
ENEMY:
    if flags LE & 0x20 (undead/resistant) and !g_5890: dmg /= 2        ; 161a-1631
    if flags LE & 0x08 (immortal): dmg = 0                             ; 163e-1645
    hp = max(0, hp − dmg)                                              ; 164a-165d
    if hp > 0 and dmg != 99:                                           ; 165f-166a
        if flags LE & 0x10 (DIVIDE): up to 8 attempts:                 ; 17f6-18ab
            cell = 0x07D4(x, y) (random adjacent, see §10)
            if COMBAT:0x0000(sprite, cell) free:
                spawn kernel 0x6506(type by sprite, 0, x, y, floor)
                new.hp = CURRENT hp of the original; "<name> divides!\n"
        returns 0   ← the XP of a non-lethal strike is ZERO!
    DIES:
        xp = ENEMY_STATS[type].hp/4 + 1                                ; 167a-1683
        loot_rating = ENEMY_STATS[type].treasure (stats+7)            ; 1686
        flags = 0x20; hp = 0                                          ; 168f-1693
        if flags LE & 0x1000: "<name> vanishes!" (tile 0x16) and no remains ; 1782-17e8
        if flags LE & 0x0001 (noCorpse): removed with no chest or blood ; 16b5,17ec
        Gazer (0x1C): leaves a spawn of Insect Swarm (type 0x1F) in its cell ; 16c0-16f5
        Gargoyle (0x1E): the map cell becomes tile 0x4C                ; 16f8-170f
        if floor tile == 0x87 or < 4 (water): no chest                ; 1712-1729
        if kernel_rand30() <= loot_rating: object → CHEST (tile 1),
            contents = loot_rating; if another rand30() < loot_rating:
            contents |= 0x80 (TRAPPED CHEST)                          ; 172c-1763
        else: object → tile 0x1F (blood)                              ; 1766-177a
    returns xp
```

**EXACT XP** = `maxHP/4 + 1` of the killed enemy, for whoever lands the
killing blow, with the counter saturated at 9999. Closes the FIDELITY question about
XP gain in combat (the level-up threshold and HP per level
live in OUTSUBS:0x658/inn — Task 3.9, outside this overlay).

**Chests**: probability = P(rand30 ≤ treasure); trap = P(rand30 <
treasure). The contents byte of the chest-object = rating (the exact gold
when opened is resolved by SJOG (G)et, Task 3.3).

### Wound messages — COMSUBS:0x0312 + COMBAT:0x1A5C

`0x1A5C(idx)` classifies the ENEMY after the strike (base = maxHP>>2):
hp < base → 1 "critical!" and activates the flee local; hp < 2·base → 2
"heavily wounded!" (and if rand0(0x100) > 0xFB it ALSO flees: 1ad0 jumps to
1aa9, AFTER the `mov [bp-4],1` of 1aa4 — it only activates the flee local,
the returned level remains 2); hp < 3·base → 3 "lightly wounded!";
otherwise 4 "barely wounded!" (1a84-1afd). The flee flag (+2) of the record
is written per the flee LOCAL ([bp−6]), not per the level: it is set
if the classification set it and is CLEARED otherwise (1aff-1b15).
Players get only " hit!" printed (03fc). "killed!" if flags==0 or &0x20; " grazed!" if
g_58a2&0x20; " slept!" if &4; " dragged under!" if the attacker is
Corpser (0x2D) → flag 4 + hidden sprite (03aa-03f9).

## 7. Player turn — COMBAT:0x063E

- If wielding **chaos sword** (0x23 in left/right hand, roster+0x1B/+0x1C):
  the PC becomes POSSESSED (flag 1) and passes to the AI (069a-06c4).
- "dragged under" (flag 4): "ARGH!\n" + escape attempt 0x1C66:
  escapes if `dex > rand30()` → " regurgitated!\n", clears flag (1c72-1cdc).
- Asleep (flag 8): wakes up if `rand0(255) < 0x10` (kernel 0x6800);
  prints "Zzzzz...\n" (080a-082d).
- Keys: A=attack (COMSUBS:0x0D96 with the SUM of attack values of
  helm+left hand+right hand, 0x071e-0x0755/08e0-08e9), arrows = move
  (SJOG:0x1C56 — move/flee, Task 3.3), Space = "Pass", C = Cast
  (g_588f=g_5890=1; an adjacent enemy that attacked you "interferes!" and
  blocks it, 0x9FC; Negate or LB's crown in place → "Absorbed!"),
  G/J/O/R/S/U/Z/B/... via overlays.

### Player attack: TRIPLE strike — COMSUBS:0x0D96/0x0D3C

If the sum of the equipment's attack values is 0 → one bare-hand strike
(weapon 0xFF). Otherwise, it attacks ONCE PER SLOT with attack value > 0:
**helm (roster+0x19), left hand (+0x1B) and right hand (+0x1C)**
(0df1-0e1c) — spiked helm + two weapons = 3 attacks per turn. 0x0D3C
filters `ATTACK_VALUES[w] == 0` (0d49) and sets g_589d = w.

### Aim and execution — COMSUBS:0x0C52/0x0BF8/0x0A68

- `0x0C52(actor, w)`: if w ≥ 0x23 → g_5890 = 1 (magic weapon). Range =
  ATTACK_RANGE_VALUES[w] (DS 0x1664); 0 → melee. Enemies: range =
  reach[type] (DS 0x159C, 1→0) and projectile = enemyRangeThing[type]
  (DS 0x15CC).
- Melee (0x0BF8): hit 0x14D6 → kernel 0x3564 (flash) + 0x194A + 0x0312;
  miss → " missed!"/"Failed!" (0x00D2).
- Ranged (0x0A68): weapons with ammunition {bow 0x1A→arrows,
  crossbow 0x1C→quarrels, magic bow 0x24→arrows, flaming oil 0x13,
  sling 0x11}: an adjacent enemy that hit you "interferes!" and you lose
  the strike (0a6f-0a97). Ammunition (0x097C): bow/crossbow/magic bow
  decrement arrows/quarrels (and unequip when depleted); dagger/spear/
  throwing axe thrown at range > 1 spend 1 unit (or unequip the
  last one). The hit is rolled BEFORE flying; if it MISSES, the projectile lands
  on a random cell adjacent to the target (0x07D4, retries if it is
  the thrower's) and hits WHOEVER IS THERE at full damage (friendly
  fire, 0852-0870 + 0930-094f). Morning star (0x19) and halberd (0x22) do not
  fly (direct strike from above, 087e-08a3). Magic axe (0x26) returns
  (second return flight, 0bcb-0bed). The flight (0x12DE) traverses the
  Bresenham line and stops at the first opaque cell (kernel 0x5D8E).
  Spells 0x13/0x33/0x34/0x35/0x36 sow FIELDS on landing
  (fire 0xEA/poison 0xE8/sleep 0xE9/energy 0xEB, 08eb-0961).

## 8. AI turn — COMBAT:0x03F4

```
g_589d = 0
Time-stop ('T'): loses the turn                         ; 0418
Quickness ('Q'): loses the turn if rand(0,1) == 0       ; 0422-0437
flag 4 (dragged under): loses the turn                  ; 043d
flag 8 (asleep): wakes up if rand0(16) == 16 (1/17)     ; 0446-0467
Corpser (0x2D): re-hides (sprite off)                   ; 046a-047f
if fleeing (flag 2): if rand0(3) == 3: hp++ (¼ of healing 1); re-classify 0x1A5C  ; 0482-04ab
else:
    COMSUBS:0x00F4 (specials §9): if it acted, done     ; 04ae-04bb
    0x0226 (attack §8.1): if it attacked, done          ; 04be-04c9
move 0x0EE4 (§8.2); if it left the map: " escapes!\n" + retreat
    (Shadow Lord 0x2F fires SJOG:0x21CE)                ; 04cb-0527
if it was fleeing and could not move: attacks cornered (0x0226)  ; 052e-053d
```

### 8.1 AI attack — COMBAT:0x0226 (+0x014E at range)

```
victim = 0x0D30 (nearest Euclidean of the opposing side, §8.2); if none → 0
if actor charmed (flag 1): if dist == 1 → melee with weapon 0x21 (COMSUBS:0x0BF8); done  ; 0271-0298
if victim is player with AMULET (roster+0x1E == 0x2D) and attacker magic
    (flag LE 0x8000): amulet_negate = (rand0(255) < 0x80)  (50%)        ; 029c-02dc
dist = COMSUBS:0x04D4 (Euclidean, §10); if dist > reach[type]: done (0)  ; 02df-02f5
if dist > 1: RANGED ATTACK (0x014E):                                    ; 0309-0317,03e0
    Mimic (0x1A) always fires; the rest only if rand(0,255) >= 0x80 (50%)  ; 016e-0180
    if flag LE 0x8000 (magic) and Negate active: does not fire          ; 0182-019b
    hit = 0x14D6(victim, actor, weapon 0) — except amulet_negate → miss  ; 01b5-01d6
    projectile enemyRangeThing[type] flies (0x0822); damage to whoever is
    in the final cell (0x194A + 0x0312)                                 ; 01d9-021c
if dist == 1: MELEE:
    Mimic/Corpser reveal themselves (sprite 0x20)                       ; 02f8-0304,031a-0327
    g_cmb_last_attacker[victim] = actor                                 ; 033f-0345
    hit = 0x14D6(victim, actor, weapon 0); miss → done (no message)     ; 0349-035d
    if flag LE 0x0002 (STEALS FOOD): if rand(0,3) != 0 and g_food > 0:
        "\nA <name> stole some food!\n"; food −= 5; NO damage            ; 0366-03bc
    else: 0x194A + 0x0312 (normal damage)                               ; 03be-03dd
```

### 8.2 Target and movement — COMBAT:0x0D30 + 0x0EE4

- `0x0D30(actor)`: chooses the LIVE combatant of the opposing side that is
  NEAREST (integer Euclidean distance, §10), ignoring invisibles (except
  Shadow Lord 0x2F, who sees them) and "dragged under". Under spell 'C'
  (confusion): if rand30() > INT(actor) the side is evaluated in reverse
  (0d5d-0d79). If no target remains: ALL enemies become hp=1 +
  flee (0e3b-0e5d). Leaves in g_5876/78 the sign vector toward the target
  (or its opposite if the actor is fleeing) (0e67-0ec9).
- `0x0EE4(actor)`: Reaper (0x1B) and Mimic (0x1A) NEVER move (0f05-0f14).
  Teleport (flag LE 0x2000, without Negate): if not already in contact
  (SJOG:0x2148) or rand0(3)==3, it jumps to a random free cell
  (0x120E: x=rand0(15), y=rand0(15), valid if both ≤ 10 — ONE attempt)
  → " teleports!\n" (0f20-0fa8). If it does not teleport and is not in contact:
  it picks an axis with rand0(255) > 0x7F (X first as it is larger), slides along
  the free axis if the other is blocked (SJOG:0x20D8 = walkable cell);
  if neither: up to 4 attempts of random direction rand0(3)
  (0→S,1→E,2→N,3→W) (0fca-10c4). On moving off the map the loop
  sets g_58a2 0x10 → " escapes!\n" and retreat (10fa+…04d8-0524).

## 9. Per-turn specials — COMSUBS:0x00F4

Chain with FALL-THROUGH (an enemy with several flags may try the
next one if the previous one does not get executed); all blocked by
Negate ('N') or time-stop:

1. **Possess** (flag LE 0x0040 — Gazer, Blackthorn, Shadow Lord): picks a
   slot at random rand0(0x1F); if it is NOT a clean player (`test 0x80` in
   0x157 / `test 0x3d` in 0x15d, both `j.. 0x1ca`) it FALLS to the
   invisibility check WITHOUT consuming the turn — and if nothing else applies either, the
   function returns 0 and the enemy attacks/moves. With a valid player:
   INT contest (COMSUBS:0x0000: victim RESISTS if
   `(INT_v − INT_a + 30)/2 > rand30()`); resistance is SILENT
   (0x171 jne 0x1c4 → ret 1, no message) but consumes the turn; if it does not
   resist → flag 1 ("possessed!"), loses control. Type 0x26
   (Daemon) disappears after possessing (01b4-01c1).
2. **Invisibility** (flag LE 0x0800 — Ghost, Blackthorn, Shadow Lord):
   with probability rand0(255) < 0x20 (1/8) it toggles visible/invisible
   (" reappears!/disappears!" — flag 0x10 + sprite on/off) (01cf-023b);
   if the roll fails, it FALLS to the daemon check (01e4 `jge 0x23e`).
3. **Summon daemon** (flag LE 0x0400 — Daemon, Dragon): with probability
   rand0(255) < 0x20 (1/8): random cell 0x120E (one attempt; fails →
   nothing); if it is free (0x0000) spawn kernel 0x6506 of type 0x26
   (Daemon); " gates in a daemon!\n" (023e-0306).

## 10. Geometry and utilities

- **Distance** = `floor(sqrt(dx² + dy²))` — COMSUBS:0x0458 (squares) +
  0x048A (isqrt by odds) + 0x04D4 (between combatants). Adjacent
  diagonals give 1 (√2→1): diagonal melee OK.
- **Random adjacent cell** — COMSUBS:0x07D4(x,y): repeats
  `x' = x + rand(1,3) − 2; y' = y + rand(1,3) − 2` until 0 ≤ x',y' ≤ 10
  (2 rands per attempt).
- **Free cell** — COMBAT:0x0000(tile, x, y): passable for the sprite
  (kernel 0x2C4C) + without 0xFF + without a blocking object (fields 0xE8-0xEA and
  corpses 0x1E/0x1F do not block; 0xEB does) + without a live combatant.
  Quirk: out of range returns 1 (callers already bound it).
- **Fields** — COMSUBS:0x0056: each object with tile & 0xFC == 0xE8
  dissipates with probability rand0(255) < 0x10 (1/16) (per round).
- **Terrain damage at end of turn** — COMBAT:0x1B1E: floor 0x8F/0xBC
  (lava) or fire field 0xEA → flash + `0x1574(idx, rand0(10))`; floor
  4 (swamp) or poison field 0xE8 → poison (0x18BA, only sprites < 0x80);
  sleep field 0xE9 → unconditional sleep (kernel 0x68AE).
- **Sleep/wake** — kernel 0x68AE/0x6800: asleep player = status
  'S' + sprite 0x1E; waking restores 'G'. kernel 0x6880 = sleep except
  poisoned.
- **.CBT triggers** — COMBAT:0x111A(x,y) (implemented in the port
  `Combat.fireTriggers`, lane arena-triggers):
  - **Effect (0x111A):** walks the 8 triggers (`at` X=0xAE1F / Y=0xAE27); if
    (x,y)==at, writes the tile `sprite` (0xAD1F) into newPos1 (0xAE3F/0xAE47) and
    newPos2 (0xAE5F/0xAE67), each destination ONLY if both of its coords < 0xB
    (`cmp 0xb / jae skip`), and CONSUMES the one-shot trigger (writes 0xFF into
    at.x/at.y, 116f-1173). Opens walls (0x4F→0x44 BrickFloor) or sows lava
    (0x8F) per the .CBT. Redraws (call 0xffffb680), NO message.
  - **Callers (3):** SJOG:0x1d42 = after a SUCCESSFUL MOVE by a
    combatant (0x1d11 `or ax,ax; je` = only if the move landed), gate
    `test [g_unk_58a1],0x82` (= ROOM combat; the field omits it → no
    triggers). Pushes the DESTINATION (X,Y). Applies to BOTH sides (party and enemy):
    whoever STEPS on the plate triggers it. // SJOG:0x1e8c = special move over tile
    0x4c. // CMDS:0x1699 = via command, but gate `cmp g_location,0x7f; jbe
    skip` ⇒ **does NOT fire in a dungeon** (loc 0x21-0x28 ≤ 0x7f).
  - **Corollary (anomaly "at on a non-walkable cell", 35/128):** in ROOM combat
    the plate fires only via a SUCCESSFUL MOVE; if its `at` is a tile
    impassable in combat (Brazier 0xB2, Portcullis 0x99, StoneHeadstone 0x8A,
    Fountain 0xD8, Sconce 0xB0/B1, LargeRockWall 0x4D…), NO combatant
    occupies it → it NEVER fires (the CMDS command-path that would attack the
    cell is gated outside dungeons). The port reproduces it faithfully (move rejected by
    `tilePassableFor` = `walkable`/`landEnemyPassable`). Full census: 61/128
    maps with triggers; the walkable `at`s (0x44/0x40/0x45/0x05/…) DO fire.
  - **#29/ch16b (Deceit combatmap 29 = DUNGEON.CBT[13], BY ARRAY POSITION):**
    **2 Dragon + 9 Headless** (ch16b names it CORRECTLY; ⚠ resolve names with the
    index-hack of `enemyDefs` —gaps 8/9/42/43, i>8 −2, i>41 −2— NOT with
    `monsterNamesMixed[idx]` directly). The 9 Headless are sealed behind 0x4F in
    x1-3/y4-6; the plate (5,5)=BrickFloor WALKABLE writes 0x44 over that wall →
    it unseals them for melee. The 2 Dragon (DryStone 0x46 pocket) are RANGED-
    reachable (ch16b kills them). ⇒ #29 IS winnable if the resolver STEPS on the plate.
    ch16b stays GREEN with live triggers because the ranged resolver does NOT step on (5,5)
    → the wall does not open → 9 Headless sealed → "unwinnable" holds. Its
    "dead-end" is an artifact of the RESOLVER (not of the binary: a player would step on the
    plate); it is rewritten when conquerRoom (P1) detonates plates.

## 11. DATA.OVL tables used (all already in the extractor)

| DS | fileoff | table | use here |
|----|---------|-------|----------|
| 0x13BC | 0x13CC | ENEMY_STATS 48×8 (`enemyStats`) | +0 str, +1 dex, +2 int, +3 armour, +4 damage, +5 maxHP, +6 (maxPerMap, not used in COMBAT), +7 treasure/loot |
| 0x153C | 0x154C | flags LE per word (`enemyFlags`, the clone combines it byte-swapped) | see bits §5-§9 |
| 0x159C | 0x15AC | attack range per enemy (`enemyAttackRange`) | reach |
| 0x15CC | 0x15DC | `enemyRangeThing` | projectile tile |
| 0x15FC | 0x160C | `attackValues` (55) | max damage per weapon |
| 0x1664 | 0x1674 | `attackRangeValues` | range per weapon |
| 0x169C+w | 0x16AD (1-based) | `spellAttackRange[w−1]` | ==8 → STR weapon; otherwise the weapon's projectile tile |
| 0x1856 | 0x1866 | pointers to monster names | messages |
| 0x17F6 | 0x1806 | pointers to equipment names | "armed with" |

Ability bits verified in the code (LE word from 0x153C; in
parentheses the clone's name after the byte-swap): 0x0001 no corpse or
loot (noCorpse), 0x0002 steals food (stealsFood), 0x0004/0x0200 poison
on hit (poisonAtRange/poison — identical in 0x194A), 0x0008 immune
to damage (immortal), 0x0010 divides (divideOnHit), 0x0020 half damage
without magic (undead), 0x0040 possesses (possessCharm), 0x0080 hits with STR
(bludgeons), 0x0400 summons daemons (gatesInDaemon), 0x0800 invisibility,
0x1000 " vanishes!" (disappearsOnDeath), 0x2000 teleports, 0x8000
magic projectile (Negate/amulet). They do not appear in COMBAT/COMSUBS: 0x0100
(infectWithPlague) and 0x4000 (ranged) — semantics in other overlays.

## 12. Census of rand per overlay (the 20 near call sites)

COMBAT: 0x017A (50% AI shot), 0x02CC (amulet 50%), 0x0379 (steal 3/4),
0x0430 (Quickness 50%), 0x0453 (wake 1/17), 0x0495 (healing on flee
1/4), 0x0818 (wake PC 1/16), 0x1309 (weapon damage rand(1,atk)),
0x1389 (defense rand(1,def)), 0x198D (poison 3/4).
COMSUBS: 0x0072 (dissipate field 1/16), 0x0142 (slot to possess), 0x01DE
(invisibility 1/8), 0x0255 (daemon 1/8), 0x07E7/0x07FA (adjacent cell
x/y), 0x125B/0x1268/0x1275/0x1282 (sparks of the impact animation —
presentation only, but they CONSUME 4 rands per explosion frame…
0x0F4A:1214-12c4; order matters for the stream parity).

Additionally all of combat uses kernel_rand0 (0x3AAE) and kernel_rand30
(0x3ABE), which each consume 1 rand from the stream.

## 13. Runtime findings from the verification (2026-07-09/10)

- **Cosmetic consumers of kernel rand during combat**: the
  sprite animation consumes ~12 rands per frame via kernel
  0x4625/0x466D/0x469F (`call 0x2092` with rand(0,255); patchable in RAM
  with `mov ax,0xFF` — the "no-anim" branch in all three; automated in
  combat_parity.patch_anim_rand) and at least the kernel 0x2F70 family
  (rand(0,63) per sprite, observed ret 0x2F73 with the sprite on the stack).
  Any stream parity must silence or model them.
- **0x0EE4 (AI movement), exact semantics verified live**:
  `v = rand0(255)`; v > 0x7F → tries the X axis and, if SJOG:0x20D8 rejects
  it, the Y axis; v <= 0x7F → tries ONLY the Y axis; if the tried axis
  fails, it goes directly to wandering (up to 4 × rand0(3): 0→S,1→E,2→N,3→W).
  SJOG:0x20D8 returns 0 = walkable (inverts COMBAT:0x0000) and for a
  fleeing actor the cells off the board are "walkable" (exit).
- **SJOG:0x2148**: counts orthogonal neighbors blocked via 0x20D8 (no
  rand).
- **kernel 0x2C4C**: the class is indexed by the TILE of the mover
  ([0x54F4 + tile>>2]); rats (0x90) and party (0x44-0x4C) are class 0 →
  on-foot bitmap 0x54D4, MSB-first (mask `0x80 >> (tile&7)`, kernel
  0x2BD4 2bda-2be5), bit 1 = blocked.
- **Verified live** (registers read in real combat): enemy spawn
  speed = dex ± rand0(7)−4 (rats dex 20 → 17..23), players
  = roster DEX; countdown = 36 − speed; full layout of
  records and objects as in §1.

## 14. Deliberate divergences of the clone (documented in the code)

- The Aim UI (interactive cursor COMSUBS:0x0504) and the projectile/explosion
  animations are not replicated pixel by pixel; the clone uses its cell-by-cell
  raycast. The 4 spark rands are consumed anyway
  in the parity harness to keep the stream aligned when it applies.
  The free Aim cursor (starts pinned on the nearest enemy in range
  `bp+4` = ATTACK_RANGE_VALUES[w], 0539-0568; it moves in cardinals bounded by
  range and grid, 05c5-0628) is MAPPED to two controls of the clone (fix #44): the
  CLICK aims at any cell in range (free cursor) and the KEYBOARD `A`+arrow
  walks the cardinal line to the first enemy within range
  (`playerAttackDir`). Previously the keyboard only struck the adjacent cell (a useless arc
  at 2+ cells). The engine already modeled range + line of fire
  (`playerAttack`/`canReach`).
- The **ammunition consumption** (0x097C) is WIRED (fix #51): PER-SHOT (decrement
  in 0x0B3D, before the hit of 0x0B51 → it is spent whether it hits or misses). Bow 0x1a/magic bow
  0x24 → Arrows 0x1b; crossbow 0x1c → Quarrels 0x1d (shared inventory
  `equipmentQuantities`); at 0 it unequips the weapon and returns it to the pack (09a2-09ab) —
  ⚠ **AMENDED 2026-08-06 (#36)**: the unequip at 09a2 reaches the **ENTIRE PARTY** (the `call`
  goes to SJOG.OVL:0x1b34, which sweeps `si < g_party_size` over `unequip_item` and returns the
  count) and the `add` at 09ab adds **that N**, with no cap. See `re/notes/municion-36-acta.md` —
  WITHOUT printing anything (the `dec`/`jne` falls into the unequip `call` and RETurns 0x09af without
  pushing a string; the clone's previous "Thou art out of ammunition!" was FABRICATED —
  the ONLY ammunition string, DS 0x981c "Thou hast no ammunition for that weapon!",
  is used by the (R)eady gate in ZSTATS:0x0d2c, another event).
  Thrown {Dagger 0x10, Spear 0x15, Throwing Axe 0x16} at dist>1 spend 1 of themselves;
  the last one is lost (09ce → call 0xffff8c80 = kernel 0x6e60, unequip/destroy
  weapon) ALSO without printing anything (there is no print before the 0x6e60, unlike the glass
  sword which does print "Thy sword hath shattered!" before; the clone's «{} has no more to throw!»
  was FABRICATED — it is in no binary). Impl: `combat.ts consumeAmmo`
  (player only) + `equip.ts` (`ammoItemFor`/`isThrownWeapon`/`unequipWeaponById`).
  Sling 0x11 does not consume; Flaming Oil 0x13 is consumed in 0x0ACE (outside 0x097C).
  Bug-for-bug PORTED (#18, not a divergence): the `dec` u8 underflows-to-255 on 0
  (0x099c) and the `jne` does NOT fall into the unequip → the bow stays equipped with 255
  free arrows (test combat.test.ts:517). See re/deliberate-divergences.md §2.
- The PC's movement in combat (SJOG:0x1C56) and the opening of chests
  (SJOG (G)et) belong to Task 3.3; the clone keeps its current
  behavior with the exact rating already stored in the chest.
- kernel 0x5D8E (opacity of a cell to projectiles) is approximated with
  `rangeWeaponPassable` of TileData until that kernel fn is ported.

## 15. Live-trace parity harness (2026-07-10)

`test_combat_trace_parity_live` (`re/tools/test_combat_parity.py`) compares the
clone's combat engine against the binary in dosbox-x, roll by roll of
`rand_range`. Final design, after verifying live three things:

1. **AI movement is EXCLUDED from the comparison** (deliberate range
   divergence). The rand stream of the movement (axis COMBAT:0x0EE4 rand0(255),
   §8.2) is NOT captured reliably over the pty channel: with the enemies far,
   each move emits an axis roll whose SS:SP stack read at the BP comes out
   misaligned most of the time (of ~12 moves in a real trace only 1
   read as caller 0xB261; the rest as garbage). So
   `seed_melee_positions` (combat_parity) **seeds the enemies already in melee**
   (adjacent, Chebyshev ≤ 1 = distance 1, §10) in cells EXCLUSIVE to a
   player and inflates the HP of the live players: nobody moves or dies, there are
   no movement rolls anywhere, and the comparison isolates EXACTLY
   hit (§3) + damage/defense (§4) + poison (§5) + HP trajectory. The
   movement will still be verified by another route (its formula is in §8.2/§13).
2. **All cosmetics are silenced** so that the stream is ONLY game
   rolls. In addition to `patch_anim_rand` (0x4625/0x466D/0x469F, §13),
   `patch_sprite_rand` neutralizes the sprite-frame randomizer kernel
   0x2F62 (consumes rand(0,63) on each call, ~90 % of the BP hits) with a
   `ret` (C3) at the entry — it is void and cosmetic. Verified live: with
   both patches the combat rand stream is ENTIRELY game rolls (hit
   0xB7F1, poison-check 0xBC20, poison-dmg 0xBB9E, defense 0xB61C), with no
   residual cosmetic consumers.
3. **The comparison is by seed SUBSEQUENCE** (`compare_subseq`), not
   positional. Since there is no cosmetics, the clone's RNG orbit (free-run
   from the snapshot's seed) matches EXACTLY that of the binary; the
   trace is a sampling with possible losses from pty reads. Each captured roll
   is matched by advancing through the clone's rolls up to the one of the SAME
   SEED, requiring that range, party HP and enemy HP/position match.
   It tolerates capture drops WITHOUT weakening (each captured roll
   is verified by seed+range+state; a modeling failure misaligns the
   orbit → the seed does not appear → it is reported). The clone runs in
   `subseq` mode (`game/src/core/__parity__/combat-run.ts`, `LoggingRng`).

**Reliability of the BP reads** (indispensable for 2 and 3): the
misreads of SS:SP were TRANSIENT (a re-break of the same roll gave a garbage
read and a correct one) and sometimes PLAUSIBLE (a hit read as a
defense roll). Two fixes in the oracle/harness eliminate them: (a)
`oracle._read_reg` drains the pty and takes the LAST match (stale EV
responses from previous pauses gave stale CS/IP → false at_code_bp and SS:SP of
another stack); (b) `combat_parity._stack_words` requires that TWO reads of the stack
match (the correct one is stable and repeats; the stale ones do not). The capture
additionally deduplicates by seed (the BP may re-fire without executing; the RNG is
bijective over 16 bits — add/ror3/xor/add, see re/notes/rng.md —, a repeated
seed = the same roll).


> **A4-PARITY2 D-88 (2026-10-03) — superseding note.** `0x5882` (`g_cmb_action_count`) is NOT combat-local: it is a byte of the 0x1060-byte SAVED.GAM window at file offset `0x2DC`, loaded and saved verbatim, never initialised at combat entry or exit (exactly three instructions reference it, COMBAT `0x0C64-0x0C70`). The round loop (file `0x0B94`) increments it for every unit ACTIVATION that survives the four skip tests (empty slot, gone slot, roster-'D' party member, tile `0x84/0x85`) and whose countdown reaches 0; `cmp byte [0x5882],0xA / jne` (equality, 8-bit wrap); at 10 the byte is zeroed BEFORE `advance_clock(1)` (kernel `0x4F7C`, a plain clock call, no world turn, no housekeeping), at the START of the activation, before the unit acts. Inside an arena `g_location` is `0xFF`. The exit conditions in section 2 are swapped: SJOG `0x21CE` (`0x0CCA`) runs when the human side is empty and the AI side is not; "VICTORY!" (`0x0CE8-0x0CFA`) when humans remain and the AI side is empty. `kernel-survival.md`'s "2 callers" of `0x4F7C` is superseded: 21 sites, of which only COMBAT `0x0C76` is reachable from an arena. Fixed in both ports. `native/core/a4-parity2-findings/D88-FINAL.md`.
