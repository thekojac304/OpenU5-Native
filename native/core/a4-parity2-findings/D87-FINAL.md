# D-87 (crop food cap) -- FINAL reconciled report

Reconciler pass over D87-A.md and D87-B.md. Read-only; nothing in the repository was touched.
Own scratch scripts (this folder): `d87f_census.py` (per-overlay best-fit base + by-band census),
`d87f_census_allbases.py` (all-bases over-reporting census). Every binary fact below was re-disassembled
in this session with `re/tools/dis16.py` (SJOG.OVL / COMBAT.OVL by file offset, ULTIMA.EXE with `--exe`).
DS strings: DATA.OVL file offset = DS + 0x10 (read directly with Python, quoted below).

---------------------------------------------------------------------
## 1 Verdict

The ledger entry D-87 is CONFIRMED on the binary. A and B agree on every material point and I found no
disagreement that needed adjudication (STEP 1 list in 1.1). All their agreed claims survived my refutation
attempts (1.2).

What the original does when a crop is picked up:

* Every food-adding branch of SJOG `Get` (SJOG.OVL 0x18CE) ends in kernel `counter_add(&food, 1, 9999)`
  (ULTIMA.EXE 0x3F14, called as `call 0x7f94` from SJOG). Increment exactly 1, cap exactly 0x270F = 9999.
* `counter_add` has no failure path. It computes `s = int16(old + 1)`; if `s >= 9999` (SIGNED compare) it STORES
  9999, else it stores `old + 1` (16-bit wrap). So 9998 -> 9999, 9999 -> 9999, **10000 and above (up to 32766)
  -> 9999 (clamp DOWN, not "leave alone", not "refuse")**. Only `old >= 32767` leaves the plain-`min` family
  (32767 -> 0x8000, 65535 -> 0); unreachable in play.
* At the cap NOTHING else changes. The tile rewrite (`0x2D -> 0x2C`), the redraw/turn mark `[0x24E6] |= 2`,
  the text `Crops picked!` and the karma decrement all execute before/independently of `counter_add`, and no
  instruction after the call reads its result. A crop picked at food 9999 is still consumed, still prints
  `Crops picked!`, still costs 1 karma (if karma > 0), still passes the turn; food stays 9999.
* The cap applies to FOUR tiles, not just crops: crop 0x2D (call 0x1A50), plate 0x9A (call 0x1A50 via the shared
  tail), plates 0x9B and 0x9C (call 0x1ABE). The ledger's wording "crops" under-describes the fix scope.
* There is NO separate arena routine in COMBAT.OVL. The arena `G` key runs the same SJOG routine through the
  kernel thunk 0x7E06 (COMBAT.OVL 0x0580). The "arena twin" is a port-side duplicate.

Who is wrong:

* Original: correct as above.
* `re/notes/cmds.md` (lines 289, 304, 322): CORRECT (it already says +1 with cap 9999 and that the clone's
  `food++` lacks the cap). The note was right and was never acted on. No note change needed except marking it fixed.
* TypeScript reference: WRONG in two places, `game/src/core/game.ts` lines 6079 (`stealFood`, plates) and 6145
  (wheat branch), both bare `this.state.food++`.
* Native: WRONG in one statement, `native/core/src/quest_search.cpp` line 56 `++c.game.food`, which serves crops AND
  all three plates. Arena twins in both ports (`combat.ts:4025`, `combat.cpp:1516`) are already right (plain `min`).
* Task brief / `callers_banda.py` overlay table: COMBAT.OVL base 0xBFEC is WRONG; the right base is 0xA290 (1.2, 2.1).

### 1.1 Disagreements and unresolved items between A and B (STEP 1)

| point | A | B | settled by me |
|---|---|---|---|
| counter_add semantics | signed clamp, clamps down | same | confirmed by own disassembly (2.2) |
| food-adding sites | 0x1A50, 0x1ABE, plus 0x14EA | same | confirmed (2.3, 2.5) |
| COMBAT base | 0xA290 | 0xA290 | confirmed, contradicts brief (1.2, 2.1) |
| arena crop consumed | yes (same SJOG code) | yes | confirmed (2.4); consumption is unconditional |
| `[0xA9FA]=1` | crop and 0x9A only | same | confirmed (2.3); presentation only |
| other overlay bases | A: deliberately not adjudicated; B: heuristic list | | my fit differs from both tables for some overlays (2.1); not needed for D-87 |
| turn handling of a refused Get / dispatcher return | not traced | listed as out-of-scope doubt | not needed for the cap; left as residual 7.3 |
| exact-signed vs plain `min` | decision for implementer | same | I give a recommendation in 3/4, both options exact |

No factual conflict between A and B existed. Differences are only in emphasis (A: census table of every
`counter_add` caller; B: map census and fixture suggestions).

### 1.2 Adversarial checks (STEP 2): the three most consequential agreed claims

1. "counter_add clamps down and never refuses": re-disassembled 0x3F14 (2.2) and every post-call instruction
   (0x1A53, 0x1A58, 0x1AC1 -> 0x1A58): none reads AX. Confirmed. Also emulated the routine in Python for the full
   boundary table (2.2).
2. "COMBAT.OVL base is 0xA290, so the arena `G` runs SJOG 0x18CE and COMBAT has no crop code": COMBAT 0x0580
   `call 0xffffdb76` + 0xA290 = 0x7E06 (a thunk `9a ec 02 2e 07 | 0e | 00 | ea 4e d8 00 00`, target 0xD84E = SJOG
   0x18CE + 0xBF80, and SJOG 0x18CE is a prologue `55 8b ec 83 ec 16`). With the brief's 0xBFEC the same call
   resolves to 0x9B62, which is not a thunk. The 'G' handler arithmetic (jump-table entry 0xABFA - 0xA290 = 0x096A,
   which pushes DS 0x6E14 = `Get-` and calls the funnel 0x0544) independently confirms 0xA290. My own by-band fit
   (`d87f_census.py`): COMBAT.OVL best base 0xA290 = 160 of the file's near calls land on kernel prologues/thunks
   vs 8 for the runner-up. The brief's 0xBFEC is refuted.
3. "No other crop-pickup path / complete call-site census": own census with positive controls (section 3).
   Confirmed: three SJOG sites plus the ports' lists; the only callers of the Get entry thunk are ULTIMA.EXE
   0x3282 and COMBAT.OVL 0x0580.

---------------------------------------------------------------------
## 2 Binary facts

### 2.1 Tooling controls and base findings

* Positive control (kernel helper): SJOG 0x1A50 is `e8 41 65` = `call 0x7f94`; `0x7F94 + 0xBF80 = 0x13F14 -> 0x3F14`,
  where `dis16.py ULTIMA.EXE 3f14 --exe` shows the prologue `55 8b ec`. Same arithmetic: `0x58d0 -> 0x1850` (print),
  `0x8482 -> 0x4402` (tile pointer), `0x766c -> 0x35EC` (direction prompt), thunk `0x7e06`.
* Control that DS 0x57A8 is food: ULTIMA.EXE 0x2B66 `cmp word ptr [0x57a8], 0 / jne 0x2b7a / mov ax,0x54c8 /
  push ax / call 0x1850`; DATA.OVL DS 0x54C8 = `"Starving!\n"`. Karma: `0x5888 = 0x57A8 + 0xE0`; the native save layout
  has `food` at save offset 0x202 and `karma` at 0x2E2 (persistence.cpp lines 20 and 29), the same 0xE0 apart.
* Overlay-base findings (kernel-prologue fit over every near call, `d87f_census.py`): COMBAT.OVL 0xA290 (160 / 8),
  SJOG.OVL 0xBF80 (230 / 19), SHOPPES.OVL 0xA290, SHOPPES2.OVL 0xE1E0, CAST2.OVL 0xE1E0, TALK.OVL 0xBF80,
  CAST.OVL 0xBF80 (270 / 114), MAINOUT/TOWN/DUNGEON 0x81D0. The brief/`callers_banda.py` values for COMBAT (0xBFEC),
  SHOPPES2 (0xA89E), CAST (0xA8D8), CAST2 (0xC29E), DUNGEON (0xE1E0) do not fit. Only COMBAT and SJOG matter here and
  both are proven by concrete call-lands-on-prologue examples above. `callers_banda.py 3f14 7e06` as shipped
  therefore misses both COMBAT callers of 0x3F14 and the COMBAT caller of 0x7E06 (it finds the EXE caller only).
* Thunk table: `thunks.py --bases` prints `ovl 14 base 0xbfec`; that is merely the lowest thunk target (SJOG 0x006C is
  a prologue, thunk 0x7F3E). Overlay id 14 is SJOG.OVL at load base 0xBF80; id 7 is COMBAT.OVL at 0xA290.

### 2.2 `counter_add`, ULTIMA.EXE 0x3F14 (Pascal convention, `ret 6`)

```
3f14: 55            push bp
3f15: 8b ec         mov bp, sp
3f17: 8b 5e 08      mov bx, [bp+8]     ; arg1 = near pointer to the word (pushed first)
3f1a: 8b 07         mov ax, [bx]       ; old (16-bit)
3f1c: 03 46 06      add ax, [bp+6]     ; + arg2 amount, 16-bit wrap
3f1f: 3b 46 04      cmp ax, [bp+4]     ; vs arg3 cap (pushed last)
3f22: 7d 08         jge 0x3f2c         ; SIGNED
3f24: 8b 46 06      mov ax, [bp+6]
3f27: 01 07         add [bx], ax       ; below cap: *p += amount
3f29: eb 06         jmp 0x3f31
3f2c: 8b 46 04      mov ax, [bp+4]
3f2f: 89 07         mov [bx], ax       ; at/above: *p = cap
3f31: 5d            pop bp
3f32: c2 06 00      ret 6
```

Semantics: `s = int16(uint16(old + amount)); food = (s >= int16(cap)) ? cap : uint16(old + amount)`. AX at return is the
amount (1) or the cap; no caller uses it. No RNG, no text.

Emulated table (Python, amount 1, cap 9999) vs plain `min(9999, old+1)`:

| old | binary | plain min |
|---|---|---|
| 0 | 1 | 1 |
| 9997 | 9998 | 9998 |
| 9998 | 9999 | 9999 |
| 9999 | 9999 | 9999 |
| 10000 | 9999 | 9999 |
| 12345 | 9999 | 9999 |
| 32766 | 9999 | 9999 |
| 32767 | 32768 (0x8000) | 9999 |
| 32768 | 32769 | 9999 |
| 40000 | 40001 | 9999 |
| 65534 | 65535 | 9999 |
| 65535 | 0 | 9999 |

Siblings (not used by pickups): 0x3EF0 byte add (cap 99), 0x3F54 word floor-0 subtract (COMBAT 0x03A3 food theft, 5).

### 2.3 SJOG `Get` (SJOG.OVL 0x18CE, CS:IP 0xD84E), exact sequences

Entry: ULTIMA.EXE command switch -> jump table `jmp word cs:[bx+0x3490]` at 0x348B (key 'G' = 0x47 -> entry 0x3274);
0x3274 `cmp byte [0x5893],0x21 / jae 0x3282` (below 0x21 first prints DS 0xA16A `"Get-"`), 0x3282 `call 0x7e06`,
0x3285 `jmp 0x31ee` (AX not stored).

Prologue and dispatch (verified instruction by instruction):

```
18d6 cmp byte [0x5893],0x20 ; jbe 0x18ea       ; [0x5893] = location
18dd cmp byte [0x5893],0x29 ; jae 0x18ea
18e4 call 0x179e ; jmp 0x1b2e                   ; location 0x21..0x28 = dungeon rooms: chest-loot routine, no crops
18ea call 0x766c (kernel 0x35EC dir prompt) ; or ax,ax ; jne 0x18f4 ; jmp 0x1b2e   ; 0 = cancelled
18f4 [bp-0xa]=[0x5876] (dx word) ; [bp-0xe]=[0x5878] (dy word) ; [bp-8]=dx+[0x5896] ; [bp-0xc]=dy+[0x5897]
191a push 0x8de6 ("\n") ; call 0x58d0
1926..19bd object-slot sweep (table DS 0x5C62, stride 8); slot hit -> call 0x1458, jmp 0x1b2e
19c0 push [bp-8] ; push [bp-0xc] ; call 0x8482 ; mov bx,ax ; mov al,[bx] ; [bp-6]=tile
19d2 cmp ax,0x9a ; jne 0x19da ; jmp 0x1a6a       ; 0x9A
19da jle 0x19df ; jmp 0x1b0e                     ; > 0x9A -> chain 0x9B / 0x9C / 0xB0-0xB1
19df cmp ax,0x2d ; je 0x1a2a ; jmp 0x1b28        ; 0x2D crop, else "Nothing to get!"
```

Crop branch (tile 0x2D), jump from 0x19E2:

```
1a2a push [bp-8] ; push [bp-0xc] ; call 0x8482 ; mov bx,ax
1a35 mov byte [bx],0x2c                  ; unconditional, before the add
1a38 or byte [0x24e6],2
1a3d mov ax,0x8df4 ; 1a40 push ax ; 1a41 call 0x58d0   ; DS 0x8DF4 = "Crops picked!\n" (file 0x8E04)
1a44 mov ax,0x57a8 ; push ax             ; &food
1a48 mov ax,1      ; push ax
1a4c mov ax,0x270f ; push ax
1a50 call 0x7f94                         ; counter_add, result unused
1a53 mov byte [0xa9fa],1                 ; stats panel dirty
1a58 cmp byte [0x5888],0 ; jne 0x1a62 ; jmp 0x1b2e
1a62 dec byte [0x5888]                   ; karma byte, decremented only when non-zero
1a66 jmp 0x1b2e
```

Plates (all share these two add sites):

| tile | accept | writes | text DS (file) | add site | `[0xA9FA]=1` | karma |
|---|---|---|---|---|---|---|
| 0x9A | `cmp [bp-0xe],1` (dy == +1, 0x1A6A) | 0x95 (0x1A7B) | `Mmmmm...!\n` 0x8E04 (0x8E14) via `jmp 0x1a40` | 0x1A50 | yes | 0x1A58 |
| 0x9B | `cmp [bp-0xe],-1` (dy == -1, 0x1A92) | 0x95 (0x1AA3) | `Mmmmm...!\n` 0x8E24 (0x8E34) | 0x1ABE | NO (`jmp 0x1a58`) | 0x1A58 |
| 0x9C | dx == +-1 refuses (0x1ACA..0x1AD4); dy +1 -> 0x9B (0x1AED), dy -1 -> 0x9A (0x1B01) | as left | `Mmmmm...!\n` 0x8E58 (0x8E68), `jmp 0x1aae` | 0x1ABE | NO | 0x1A58 |

Refusals print `Can't reach plate!\n` (DS 0x8E10 / 0x8E30 / 0x8E44; files 0x8E20 / 0x8E40 / 0x8E54) and `jmp 0x1b2e`: no
tile, food, karma or `[0x24E6]` change. Default `0x1B28: mov ax,0x8e64` = `"Nothing to get!\n"` (file 0x8E74).
Torch tiles 0xB0/0xB1 (0x1B1B -> 0x19E8) are not food (they set `[0x24E6]=1`, not `|= 2`).
Order inside every success path: tile write, `[0x24E6]`, text, counter_add, `[0xA9FA]` (crop/0x9A only), karma.
RNG: zero draws (print, tile pointer, direction prompt and counter_add never call the RNG), on every path.

### 2.4 Arena

* COMBAT.OVL 0x096A (key 'G', dispatch table at 0x0ACE): `mov ax,0x6e14 ; push ax ; sub ax,ax ; push ax ;
  call 0x544`; DS 0x6E14 = `"Get-"`. Funnel 0x0544 prints the prompt (0x75C0 -> 0x1850), tests
  `test byte [bx-0x45ea],0x80` (clear -> prints DS 0x6D98 `"Can't!\n"`, returns), then `or ax,ax / je 0x580`;
  `0x0580 call 0xffffdb76` (-> thunk 0x7E06 -> SJOG 0x18CE) `jmp 0x5b0` -> `sub ax,ax ; ret 4`.
* So the arena runs the full 2.3 routine with `[0x5893] = 0xFF`: no dungeon diversion; the object sweep skips the Z test
  (0x1950 `cmp dl,0x7f / ja`); kernel 0x4402 for location > 0x7F returns `0xAD14 + (y<<5) + x` (live arena buffer), so
  the tile writes hit the arena map; the only location-sensitive difference is the torch branch's repaint skip
  (0x19FB). COMBAT.OVL contains no crop/plate/food-add code and none of the strings (they exist only in DATA.OVL, pushed
  only by SJOG). COMBAT's `counter_add` calls (0x193E, 0x1A51) take `0x55BC + 32*member` (HP/experience); its food
  references are the theft (0x0380, 0x039B..0x03A3, subtract 5).
* Result: arena at the cap is identical to the overworld (2.3): crop consumed (0x2D -> 0x2C in the arena buffer), text
  printed, karma decremented, food 9999 (10000+ -> 9999).

### 2.5 Other food writers through the same helper (not crops; same cap)

* SJOG 0x14EA (object/pile type 0x0F, quantity `[bp+6]`; text `" food!\n"` DS 0x8C6E; tail 0x177A clears the slot,
  `[0x24E6] |= 2`, `[0xA9FA] = 1`): `counter_add(food, qty, 9999)`. It also clamps a value above 9999 down.
* Dungeon room Get (SJOG 0x179E) row 0 reaches the same 0x14EA via 0x1458.
* SHOPPES2 0x015D, 0x045F, 0x049C; TALK 0x06C4; CAST 0x05CF (Create Food) all use cap 9999.

---------------------------------------------------------------------
## 3 Callers census (by band, over-reports, own run with positive control)

Method: every `E8`/`E9` rel16 in every `*.OVL` (except DATA/FONT) and ULTIMA.EXE, resolved with each overlay's fitted
base (`d87f_census.py`), plus an all-bases over-reporting pass (`d87f_census_allbases.py`). Positive control: the three
SJOG sites 0x14EA / 0x1A50 / 0x1ABE of the ledger reproduce.

Callers of kernel 0x3F14 (counter_add):

| caller | operand | relevant to D-87 |
|---|---|---|
| SJOG 0x1A50 | `&food,1,9999` | YES: crop 0x2D and plate 0x9A |
| SJOG 0x1ABE | `&food,1,9999` | YES: plates 0x9B and 0x9C |
| SJOG 0x14EA | `&0x57A8` or `&0x57AA`, qty, 9999 | food/gold OBJECT pickup, same cap, not a crop |
| COMBAT 0x193E, 0x1A51 | `0x55BC + 32*member` | no (HP/exp; the shipped tool misses them) |
| CAST 0x05CF (Create Food), 0x097E, 0x20F9; CAST2 0x03F3 | spells | no |
| SHOPPES 0x0F3D (gold); SHOPPES2 0x015D, 0x045F, 0x049C (food); TALK 0x06C4 | shops/dialogue | no |
| ULTIMA.EXE 0x2E39, 0x4057 | word counters | no |

All-bases extra hits: MAINOUT 0x00CE/0x06A4/0x0C78/0x1C19/0x1C5A at base 0x85FE are false positives (at MAINOUT's own
base 0x81D0 they resolve to 0x3AE6, a different function).

Callers of the Get thunk 0x7E06: ULTIMA.EXE 0x3282 (all non-arena locations; dungeon rooms are split inside SJOG) and
COMBAT.OVL 0x0580 (arena). False positives at base 0xBFEC (CAST 0x0A49, SJOG 0x1EEE / 0x1FE0): with base 0xBF80 they
resolve to thunk 0x7D9A.
Callers of the dispatcher 0x3178: TOWN 0x158F, MAINOUT 0x0C00, DUNGEON 0x07A3.

Crop contexts that actually exist (own census of `game/assets/maps`): overworld 0, underworld 0 (so "outdoor" in D-87
means "outside combat"), small maps: Britain 26 crop tiles, New Magincia 90, Iolo's Hut 17, Sin Vraal's Hut 1, West
Britanny 67, North Britanny 51. Plates 0x9A/0x9B/0x9C occur in 27 small maps. Crops/plates are volatile terrain and
regrow on map reload, so New Magincia allows 90 pickups per visit. Dungeons (0x21..0x28) never reach this tail.
No farm-specific routine exists; 0x2E is not gettable.

---------------------------------------------------------------------
## 4 Reference (TypeScript) exact change

All in `game/src/core/game.ts` (LF line endings, 8286 LF-only lines), method `get()` (starts at 5864):

* Add `import { addWordCapped } from "./counters.js";` with the other top imports (lines 5-225). `game.ts` currently has no
  `counters.js` import (`dialogue/effects.ts:41`, `world/commands.ts:18` already import it).
* Line 6079 inside `stealFood` (plates): `this.state.food++;` -> `this.state.food = addWordCapped(this.state.food, 1);`
* Line 6145 wheat branch: `this.state.food++;` -> `this.state.food = addWordCapped(this.state.food, 1);`
* Keep order (tile, food, karma, message, turn); keep the refusal branches (lines 6123-6136) untouched: no refusal at the cap.

`addWordCapped` is `Math.min(cap, value + add)` (`counters.ts` lines 28-31, CRLF): equals the binary for 0..32766.
Option for bit-exactness (only matters for 32767..65535): `const sum=(v+1)&0xFFFF; const sgn=sum>=0x8000?sum-0x10000:sum;
v = sgn>=9999 ? 9999 : sum`. Recommendation: plain `addWordCapped` (it is the helper every other TS food writer and
the arena twin use, and the fixtures' expected stream comes from TS); record the 32767+ edge as a documented unreachable
divergence. If the exact form is chosen it must replace the arena twins too (`combat.ts:4025`, `combat.cpp:1516`) so the
three paths stay identical.
Already correct, do not change: arena `combat/combat.ts` line 4025 (`FOOD_CAP` line 136); loot food
`world/commands.ts:789` (`addWordCapped`); `dialogue/effects.ts:215`; `magic/cast.ts:328`; `shops/shops.ts`.

## 5 Native exact change

`native/core/src/quest_search.cpp` (CRLF, 91 lines), function `get_quest_object` (line 33). The single long line **56**
(the branch for tiles 154/155/156/45, lines 52-57) contains `s->volatile_tile(s->context,x,y,next);++c.game.food;
c.game.karma=...`. Replace `++c.game.food;` with
`c.game.food=uint16_t(std::min<int32_t>(9999,int32_t(c.game.food)+1));` (`std::min<int32_t>` is already used on line 44;
this is the same expression as the arena at `combat.cpp:1516`). One edit covers crop AND the three plates. The
`{CommandStatus::Success,true}` result and `commands.cpp:1004` turn handling stay as they are. Do NOT guard with
`if(food<9999)` (that would leave 10000+ alone, wrong) and add no refusal. `GameState::food` is `uint16_t`
(`include/openu5/state.h:31`); today `++` wraps 65535 -> 0, which coincides with the binary only at 65535.
Exact-signed alternative: `{int16_t s=int16_t(uint16_t(c.game.food+1)); c.game.food = s>=9999 ? uint16_t(9999) :
uint16_t(c.game.food+1);}` (see 4 for the policy).

Sibling divergence (same helper, NOT one of the two ledger sites, so list or include deliberately):
`native/core/src/loot.cpp` lines 138-141 `case 15: ... if(g.food<9999)g.food = uint16_t(std::min(9999,int(g.food)+q));`
leaves a value above 9999 alone, where SJOG 0x14EA clamps it to 9999, and where the TS twin (`addWordCapped`) clamps. Gold
at line 130 has the same shape (`counter_add(&0x57AA,...)`, same 0x14EA tail). Only differs for food/gold already above
9999. Dropping the `if(...<9999)` guard removes it. Callers: `quest_search.cpp:36-39` and `combat.cpp:1490` (pile Get).
Not to change: `combat.cpp:1516` (arena, correct), `enhanced.cpp` Max Food (a native cheat, deliberately never lowers).

Stale text once fixed: `enhanced.cpp:333-335` comment ("crops picked at 9999"), `ALPHA4_UI.md` lines 2847, 2958, 3338,
3350, ledger row `ALPHA2_PRESERVATION_LEDGER.md:232` (its "(Max Food (10,000))" is wrong: Max Food sets 9999; 10000 is only
reached by picking a crop at 9999), `re/notes/cmds.md:322`.

---------------------------------------------------------------------
## 6 Parity fixtures and corpora

Nothing currently pins either behaviour near the cap on the non-arena path.

Must stay byte-identical (no food near the cap, so the fix cannot change a token):
* `gameplay_parity` (`native/core/tools/check-gameplay.ts`, CMake line 3006): the row at line 104
  (`tile of [154,155,156,45,176,177] x dir 0..3`, two `get` per row, party at (10,10), `initial()` food 100 / karma 50,
  line 29; projection includes `food`, `karma`, `mapOverrides`, events). Expected stream is produced LIVE by the TS `Game`
  and compared with `gameplay_driver`; no stored fixture.
* `quest_parity` (`tools/check-quests.ts`, CMake 3015): `get` steps at lines 170/197/234 hit search/NPC-slot cells, none
  holding tiles 45/154/155/156; food 100.
* `command_parity` / `fixtures/commands.txt` (no Get), `item_parity`, `combat_parity`, `advanced_combat_parity`,
  world-flow fixtures (`playerGet` for loot piles only), dialogue/magic fixtures (food 9999 / 9998 are for TLK and Create
  Food, not Get), `combat_loot_open_regression_test.cpp` (food pile well below cap), `a4_enh2_rules_test.cpp:627-634` X4
  (sets food 10000 by hand and expects Max Food to keep it: unaffected, it never goes through Get).
* TS tests: `game/tests/get-plates-direction-gate.test.ts` (food 100 -> 101), `game/e2e/commands.spec.ts:70-88` (crop
  `foodBefore + 1`), `game/tests/get-torch-arena-375.test.ts:263-281` (arena plate at 9999 -> 9999; wheat food0+1).
  `game/tests/fixtures/approved-strings.json` and `es.json` hold only the strings.
* No golden or hardware pin on "Crops picked!" / "Mmmmm" under `native/targets` (grep: none).

Must change: nothing existing. New pins are required (6).

What changes at runtime when the fix lands: only the stored food when the result is `>= 9999` (9999 stays 9999 instead of
10000; 10000..32766 become 9999). Tile, text, karma, events, turn and RNG count unchanged. Saves already holding > 9999
are clamped on the next food-adding Get, as the original does.

IMPORTANT for the implementer: `gameplay_parity` can NOT go RED against the binary. Its expected values come from the TS
`Game`; with both ports still uncapped a new 9999/10000 row passes (both say 10000). It only turns RED if the two ports
disagree, i.e. fix one port first. Binary conformance needs absolute-value assertions in unit tests (below).

---------------------------------------------------------------------
## 7 Test plan

### 7.1 RED-first tests (absolute expectations, written before the fix)
* New TS vitest (model on `game/tests/get-plates-direction-gate.test.ts`): overworld/town `get()` with `state.food` in
  {0, 9997, 9998, 9999, 10000, 12345, 32766} against tile 45 and the plate cases; expect food {1, 9998, 9999, 9999, 9999,
  9999, 9999} and, for every row: tile 44 (crop) / 149 (0x9A, 0x9B) / 155 or 154 (0x9C), message `Crops picked!` or
  `Mmmmm...!`, karma -1 (karma 50) or stays 0 (karma 0), a turn consumed, RNG not drawn.
* New native test (e.g. in `native/core/tests/quest_search_test.cpp`, which today has no food/crop/plate case): same table
  driven through `get_quest_object` with a `volatile_tile` stub; assert `g.food`, the stub call `(x,y,next)`, karma, the
  Message text and `CommandStatus::Success` with `turn == true`. These two are RED today (9999 -> 10000, 10000 -> 10001).
* Plates: 0x9A with dy=+1 (dir south), 0x9B with dy=-1 (north), 0x9C with dy=+1 and dy=-1, each at food 9999 and 10000 (they
  go through both add sites 0x1A50 and 0x1ABE, so crop-only fixes must fail). Refusals (0x9A with dy != +1, 0x9B with dy != -1,
  0x9C with dx != 0) must leave food, tile, karma and the turn untouched at food 9999 and 10000 and print `Can't reach plate!`.
* Second Get on the same cell: `Nothing to get!`, food unchanged, no turn.
* Arena twin: native currently has NO test of `combat.cpp:1516`. Add crop and plate rows at food 9999/10000 (TS arena has
  `get-torch-arena-375.test.ts:263-271` for a plate at 9999 only; add wheat at the cap and a 10000 row).
* Cross-port rows in `check-gameplay.ts` next to line 104: `add(7,68,s=>{s.food=F},false,'flee',{inspect:true,
  inspectCoords:[[10+dx,10+dy]],edits:[[10+dx,10+dy,tile]],actions:[{kind:'get',dir},{kind:'get',dir}]})` for F in
  {9997,9998,9999,10000} x tiles {45,154,155,156} x the valid direction(s): N=dir 0 (dy -1), S=dir 1 (dy +1), E=dir 2, W=dir 3.
  Valid: 154 needs dir 1; 155 needs dir 0; 156 dirs 0 and 1; 45 any. Catches a port that was fixed alone.

### 7.2 Mutations the tests must distinguish
* M1 revert TS to `food++` (lines 6079 or 6145 independently) -> TS table fails, parity rows fail; one site only must still fail.
* M2 revert native line 56 -> native table fails, parity rows fail.
* M3 "leave alone" variant `if(food<9999)++food` -> the 10000 -> 9999 rows fail (clamp down vs leave alone).
* M4 refusal at the cap (crop not consumed or no message when food == 9999) -> tile/karma/message/turn assertions fail.
* M5 off by one (cap 10000, or `food+1 > 9999` vs `>=`) -> 9998 -> 9999 and 9999 -> 9999 rows.
* M6 cap applied to crops only (0x1A50 path but not 0x1ABE) -> plate 0x9B / 0x9C rows fail.
* M7 karma skipped at the cap -> karma rows fail (karma 50 -> 49 and karma 0 -> 0 both).
* If the exact-signed helper is chosen add rows 32767 -> 32768 and 65535 -> 0 (in both ports and both arena twins); with plain
  `min` pin the divergence explicitly (32767 -> 9999) so it is a recorded decision rather than an accident.
* Build/run notes: the suite is run serially, ctest and an ESP-IDF build must not share the machine; the working tree mixes
  EOLs (quest_search.cpp, combat.cpp, loot.cpp, combat.ts, counters.ts, check-gameplay.ts are CRLF; game.ts is LF); do not use
  `sed -i` or Python text mode.

---------------------------------------------------------------------
## 8 Residual unknowns (exact)

1. No runtime witness: all of the above is static. A DOSBox run with a save whose word at DS:0x57A8 is 0x270F / 0x2710, Get on
   a crop, then reading the word would settle it. Risk judged very low: no instruction after the call reads the result.
2. Values >= 32767 (signed quirk): the original gives `old+1` (32767 -> 32768, 65535 -> 0); plain `min` gives 9999. Reachable only
   by a save import (native `save_core.cpp:336` accepts food 0..65535) or a hand edit; Developer rows and cheats cap at 9999.
   Needs a project decision (4/5), not more binary evidence.
3. Turn semantics: the EXE dispatcher (0x3178, `[bp-2] = 1` at 0x317E, the Get case at 0x3282 does not store AX) appears to
   report "turn consumed" for every Get, while the ports skip the turn for `Nothing to get!` / `Can't reach plate!` on the
   strength of the `[0x24E6]` marker. I did not trace `[0x24E6]`'s consumer or TOWN 0x158F / MAINOUT 0x0C00 / DUNGEON 0x07A3
   past the call. Irrelevant to the cap (identical at and below it) but relevant if anyone re-derives the refusal-turn rule.
4. The arena always burning the command's turn after the funnel returns (`sub ax,ax`, loop flag `[bp-4]=1` at COMBAT 0x083E)
   was read, not exhaustively traced; again independent of the cap.
5. `[0xA9FA]` (stats-panel redraw) is set only on the crop and 0x9A paths (0x1A53), not 0x9B/0x9C. Whether the native T-Deck HUD
   repaints food from a `Message` alone (`quest_search.cpp` emits no PartyChanged on this branch) was not checked; with the cap
   food changes by at most 1 and not at all at the cap.
6. Whether any real arena (`combatmaps`) holds tile 0x2D is not established from the binary (the ports' census says 0 of 128 maps
   hold crops, one dungeon record holds a 0x9A); it does not affect the fix.
7. Overlay base table in `re/tools/callers_banda.py` (and the brief) is wrong at least for COMBAT.OVL; my fitted bases for other
   overlays differ from the table (2.1) and were not individually adjudicated. Any other A4-PARITY2 census relying on that
   table should re-verify the base with a call-lands-on-prologue example first.
