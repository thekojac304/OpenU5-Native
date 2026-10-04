# D-89 (A4-PARITY2) -- naval / cactus "OUCH!" -- FINAL reconciled report

Reconciler. Inputs: D89-A.md (binary-first), D89-B.md (ports-first). Everything in section 2 was re-disassembled
in THIS session with `re/tools/dis16.py` (and scratch scripts `rec_*.py` in the findings folder); nothing in the
repository was modified, no build/ctest/idf was run. Notation: `K:xxxx` = ULTIMA.EXE offset with the MZ header
stripped (`--exe`); `MO:xxxx` = MAINOUT.OVL file offset (near calls resolve with base 0x81D0); DS strings are in
DATA.OVL at file offset DS+0x10.

---------------------------------------------------------------------------------------------------------

## 0. Reconciliation log (STEP 1 and STEP 2)

### 0.1 Where A and B disagreed or left something open, and how each was settled in the binary

| # | Point | A | B | Settled by me |
|---|---|---|---|---|
| 1 | Overlay base table and the completeness of the by-band census of K:2AA8 | used `callers_banda.py` as is: 10 call sites | says the brief's table (and `callers_banda.py` BASES) is wrong for OUTSUBS and DUNGEON, 14 sites | **B is right.** Fitted every overlay's near calls against kernel prologues (`rec_bases.py`): DUNGEON 166/190 hits at **0x81D0** (not 0xE1E0), OUTSUBS 28/33 at **0xA290** (not 0x81D0), MAINOUT 182/204 at 0x81D0, TOWN 144/158 at 0x81D0. Independent positive control for OUTSUBS: `OUTSUBS 0x05f1 mov ax,0x3a11` (DS 0x3A11 = "Burning!\n") / `0x05f5 call 0x75c0` -> (0x75C0+0xA290)&0xFFFF = **K:1850** (the string printer) / `0x05f8 call 0xffff8818` -> (0x8818+0xA290)&0xFFFF = **K:2AA8**. DUNGEON: `0x04f7 call 0xffffa8d8` -> (0xA8D8+0x81D0) = K:2AA8. Corrected census of K:2AA8 below (14 sites). A's list omitted OUTSUBS 0x05f8 and DUNGEON 0x04f7/0x0aea/0x0dc3; none of those is relevant to D-89. |
| 2 | Is the cactus "naval OUCH" a separate routine? | no, one tail MO:0312-0347 | no, same | Confirmed: one tail, one `call` at MO:0336. |
| 3 | `party-changed` / `PartyChanged` after the naval OUCH | optional, decide | "faithful choice is yes" | **Yes, emit it** (reasoning in 0.3); but as a separate, explicitly-regenerated step (section 5). |
| 4 | Clock tick after a blocked rowed step | ports tick, binary does not (adjacent) | same (adjacent, "medium") | **Confirmed from the dispatcher** (section 2.7); adjacent, NOT part of D-89, pinned by the same corpus. New divergence candidate. |
| 5 | Naval cactus in a TOWN | ports print OUCH (adjacent) | same, adds that `TOWN 0x083a` has no `cmp ...,0x2f` | **Confirmed** (section 2.8). Counted exactly: 119 of the 187 corpus naval-OUCH snapshots are town rows. Adjacent decision (section 4.3). |
| 6 | Record layout (status +0xB/+0xC, HP +0x10) | HP at record+0x10 | hedged | Only raw DS offsets matter: status byte DS 0x55B3 + 0x20*i, HP word DS 0x55B8 + 0x20*i (HP = status+5). |

No factual conflict about the verdict, the draw count/order, targets or text existed between A and B.

### 0.2 STEP 2 -- adversarial refutation of the agreed claims (three most consequential)

1. **"The OUCH call is `call 0xffffa8d8` = K:2AA8, the whole-party helper, and MO:0336 is its only D-89 site."**
   Tried to refute through (a) the overlay base: `call 0xffff9680` at MO:0212/0326/0333 resolves to K:1850 (prologue `55 8b ec 83 ec 42`, the string
   printer) and 0xA8D8+0x81D0 = K:2AA8 lands on `55 8b ec 83 ec 04 57 56`; (b) a census with a positive control over ALL overlays at fitted bases
   (`rec_census.py 2aa8`): 14 sites, MO:0336 is the only one inside the MAINOUT blocked-move tail; (c) the helper body itself (section 2.4).
   Not refuted.
2. **"The tail is shared by foot and every non-sailing vehicle; a sails-up ship never reaches it."** Re-read MO:01FE-0351 and the key handler MO:0490:
   the discriminator is `cmp byte [0x5955],0 ; je 0x312` (MO:02C0); `[0x5955]` is written only at MO:04AC (inside `(vehicle&0xFC)==0x20`),
   cleared at MO:0306 and MO:0C1B (`(vehicle&0xFC)!=0x20`) -- so non-zero implies a sails-up ship. Passability of tile 0x2F was evaluated by me
   from K:2C4C/2BD4/2C2E and the DATA tables (section 2.5): impassable for foot, horse, carpet, skiff and ship, so the tail is always reached.
   Not refuted. (One edge, section 7 item 4: a stale non-zero `[0x5955]` is not cleared before the first step after a command.)
3. **"A blocked step consumes no clock and 0x01FE has exactly one caller."** Census of `0x01FE` (`rec_local.py 1fe` over MAINOUT: only MO:0514; all
   other 'raw word 01fe' hits are `c7 46 fe 01 00` immediates; kernel-level census of 0x83CE/0x8660 over every overlay at fitted bases: only
   MAINOUT 0x0514 / 0x0BB3; raw byte search `ce 83` / `60 86` in every file found only false positives); no thunk targets MO:01FE (thunk table
   ovl 2 offsets 0x0D22, 0x08DE, 0x1A60, 0x0000, 0x007A, 0x06EC, 0x0354, 0x105C by `thunks.py`). The tick check is MO:0C30-0C39 (section 2.7).
   Not refuted.

### 0.3 Why `PartyChanged` is the faithful choice
`K:2A52` ends with `call 0x2900` (the full party-panel redraw) after EVERY member's HP update (K:2AA0), and the ports' foot cactus already
emits `party-changed` (TS game.ts:1524, native commands.cpp:503). Leaving it off the naval path leaves HP on screen stale after a skiff bump.
It is a presentation event, so it does not move any RNG draw; it changes only the event hash in the corpus.

---------------------------------------------------------------------------------------------------------

## 1. Verdict

* The original calls **`party_random_damage` (K:2AA8)** for the cactus OUCH, from ONE site, **MAINOUT.OVL 0x0336**. It takes no arguments and
  damages **the whole party**: one `rand(1,8)` for each member slot `i < min([0x585B],6)` whose status byte is not `'D'`, each draw immediately
  followed by that member's `apply_damage` (K:2A52). It is the same routine for foot, horse, carpet, **skiff (0x28-0x2B)** and a **rowed
  frigate (0x24-0x27)**; a ship **under sail** never gets here (it takes COLLISION!/BREAKING UP!/Docked! with `rand(1,30)` hull damage).
* "The naval case" = a skiff or a sails-down ship pushing into a tile whose terrain id is 0x2F. There is no separate naval routine.
  (Not reachable in stock Britannia: the 14 cactus tiles are ringed by desert; UNDER.DAT has none; Sin Vraal's Hut has no navigable tile.)
* **Who is wrong:**
  * `re/notes/cactus-ouch-acta.md` (and `re/notes/kernel-survival.md`): **RIGHT** (helper, "rand(1,8) per living member", exclusivity with the beep).
  * TypeScript reference: **half wrong.** Foot OUCH (`game.ts:1519-1528`) and `partyRandomDamage` (`world/survival.ts:393-413`) are right.
    The NAVAL OUCH (`game.ts:1929-1933`, `resolveNavalStep`) is wrong: one `rand(1,8)`, active member only (member 0 when `activeCharacter==0xFF`),
    no `'D'`, no active-member reset, no skipping of the dead, no `party-changed`. Its comments (game.ts:1522, 1836-1838; transport.ts:435) claim the opposite.
  * Native: **wrong in the same place only** (`commands.cpp:373-381` in `Runner::naval_step`). Native foot (`commands.cpp:498-506`) is right.
  * Ledger D-89 and `re/deliberate-divergences.md:825-828` (which says the helper was "not measured"): correct as a description of the defect; now measured, close it.
    `ALPHA4_UI.md:2835` ("on the active member") encodes the wrong model and needs rewording.
* Nothing in the checked-in parity corpora can see the defect (all use a ONE-member party), so the minimal fix changes **no** fixture byte.

---------------------------------------------------------------------------------------------------------

## 2. Binary facts

### 2.0 Controls
* `thunks.py --bases`: `ovl 2 base 0x81d0`; thunk 0x7A3A -> MO:0D22 = `55 8b ec 83 ec 06` (prologue).
* MAINOUT near calls (base 0x81D0): `call 0xffff9680` -> K:1850 (printer), `0xffff9ec2` -> K:2092 (RNG), `0xffffa8d8` -> K:2AA8,
  `0xffffa06c` -> K:223C (noise burst), `0xffffa0f0` -> K:22C0 (beep), `0xffff9946` -> K:1B16 (kbd flush), `0xffffaa7c` -> K:2C4C (passable),
  `0xffffb4be` -> K:368E (object lookup), `0xffffcdac` -> K:4F7C (advance_clock). All land on prologues.
* `callers_banda.py` is only correct for the bases it lists; see 0.1 #1 (OUTSUBS and DUNGEON entries are wrong, so its K:2AA8 census misses 4 sites).

### 2.1 Strings (DATA.OVL, file = DS + 0x10, read in this session)
| DS | text | used at |
|---|---|---|
| 0x29B8 | `OUCH!\n` | MO:032F only (no other overlay references it; the dungeon has its own `Ouch!\n` DS 0x2CA8) |
| 0x29AE | `Blocked!\n` | MO:0322 (TOWN has its own copy DS 0x26D6 at TOWN:083A) |
| 0x2982 | `Rowing!\n` | MO:020E |
| 0x298B / 0x2999 / 0x29A5 | `BREAKING UP!\n` / `COLLISION!\n` / `Docked!\n` | MO:02CD / 02D8 / 02E5 |
| 0x29DB / 0x29E2 / 0x29E9 / 0x29EF | `North\n` `South\n` `East\n` `West\n` | MO:0507 / 0557 / 0571 / 058B |

### 2.2 Dispatch chain
`MO:0A84` (outdoor main loop) -> `MO:0BB3 call 0x490` (`push key; push [bp-2]`; only caller of 0x490) -> `MO:0490` (outdoor_move: key 1=W 2=E 3=N 4=S;
second argument = "no step even if free" flag from `MO:0A1A`) -> `MO:00DA` (transport_face; non-zero return aborts, MO:04F9/054E/0566/0582) ->
direction echo if `[0x5955]==0` (MO:0500-050B) -> **`MO:0514 call 0x01FE`** (ship_try_move; single caller in all files).
Frigate turning: MO:016A..01B0 prints "Head <dir>" and returns 1 (turn consumes the key), so a rowed frigate bumps only when already facing the
cactus; a skiff (MO:0152, prints "Row ", writes `[0x587C]=(tile&0xFC)+turn`, returns 0) turns AND steps in one key, and its tile is already the new facing
when 0x01FE runs.

### 2.3 MAINOUT 0x01FE, quoted (return [bp-2]; `ret 4`; args [bp+6]=dx, [bp+4]=dy)
```
0205 a0 7c58 / 24 fc / 3c 24 / 75 07     mov al,[0x587c]; and al,0xfc; cmp al,0x24; jne 0x215
020e b8 8229 / 50 / e8 6b94              mov ax,0x2982; push; call 0xffff9680        "Rowing!\n" (0x24-0x27 only; on ENTRY, before any test)
0215 c746fe 0100                         mov word [bp-2],1
0236 e8 85b2 -> K:368E                   object at (x+dx,y+dy,floor): [bp-4] = actor id or 0
023c 0bc0 / 7448 ; 0240 [bp-2]=0         an actor is there: blocked unless boardable (0245-0283)
0288 8b7604 / b1 05 / d3e6 / 8b5e06 / 8a80 a7ab / 2ae4 / 8946fa
                                          si=dy<<5; bx=dx; al=[bx+si-0x5459] (=0xABA7 party cell +dy*32+dx); [bp-6]=al   <- DESTINATION TERRAIN (word)
029b cmp word [bp-2],0 ; 7413            skip passable() when an actor already blocks
02a1..02a8  push [0x587c]; push [bp-6]; call 0xffffaa7c (K:2C4C)   passable(vehicle, tile)
02b9 0bc0 / 7403 / e98a00                passable -> jmp 0x34a (return 1)
02c0 803e 5559 00 / 744b                 cmp byte [0x5955],0 ; je 0x312          <- sailing vs everything else
 sailing arm 02c7-0310: tile==3 "BREAKING UP!" / tile!=0x47 "COLLISION!" / tile==0x47 "Docked!" + add [0x587c],4 (no noise, no damage);
                        collision: push 0x64,0x7d0,0x12c; call 0xffffa06c; call 0x109e (hull rand(1,30)); 0306 [0x5955]=0; 030b [0x5956]=1; jmp 0x34a
0312 803e 7c58 20 / 7209                 cmp byte [0x587c],0x20 ; jb 0x322       vehicles are >= 0x20
0319 8a46fc / 24fc / 3cec / 7428         al=[bp-4]; and 0xfc; cmp 0xec; je 0x34a  vehicle + whirlpool actor (0xEC-0xEF): SILENT return 0
0322 b8 ae29 / 50 / e8 5793              "Blocked!\n"                            <- ALWAYS, first
0329 837efa 2f / 750d                    cmp word [bp-6],0x2f ; jne 0x33c         <- terrain == cactus
032f b8 b829 / 50 / e8 4a93              "OUCH!\n"
0336 e8 9fa5                             call 0xffffa8d8 = K:2AA8  party_random_damage()   (no arguments, nothing pushed)
0339 eb0c                                jmp 0x347
033c b8 a500 / 50 / b8 c800 / 50 / e8 a99d   push 0xa5; push 0xc8; call 0xffffa0f0 (K:22C0 beep)  <- ELSE only; mutually exclusive with OUCH
0347 e8 fc95                             call 0xffff9946 = K:1B16: 0040:001A = 0040:001C = 0x1E (flush BIOS keyboard buffer)
034a 8b46fe / 5e / 8be5 / 5d / c2 0400   return [bp-2] (= 0 on every blocked path)
```
Cactus test is on the TERRAIN WORD at [bp-6] (no signedness issue); it sits BELOW the sailing test and the `<0x20` test.

### 2.4 The helper K:2AA8 and K:2A52, quoted
```
2aa8 55 8bec 83ec04 57 56      prologue ; 2ab0 sub si,si (i=0) ; 2ab2 mov di,0x55b3 (&status[0], +0x20 per member)
2ab5 mov ax,si ; mov cl,[0x585b] ; sub ch,ch ; cmp ax,cx ; jae 0x2ad6     i < party_size (UNSIGNED, byte)
2ac1 cmp byte [di],0x44 ; je 0x2ad6                                        status 'D' -> skip (NO draw, NO damage)
2ac6 push si ; 2ac7 push 1 ; 2acb push 8 ; 2acf call 0x2092 ; 2ad2 push ax ; 2ad3 call 0x2a52    apply(i, rand(1,8))  (draw BEFORE apply)
2ad6 add di,0x20 ; inc si ; 2ada cmp si,6 ; jl 0x2ab5                      at most 6 slots
2a52 apply(member=[bp+6], dmg=[bp+4]):
  push [bp+6]; call 0x2a28 (invert the roster row) ; push 0xa; push 0x640; push 0x7d0; call 0x223c (noise burst) ; push [bp+6]; call 0x2a28 (un-invert)
  si=member<<5 ; ax=dmg ; 2a7b sub word [si+0x55b8],ax ; 2a7f cmp word [si+0x55b8],0 ; 2a84 jg 0x2aa0     (SIGNED: HP>0 survives)
  2a86 mov word [si+0x55b8],0 ; 2a8c mov byte [si+0x55b3],0x44 ; 2a91 al=[0x587b]; cmp [bp+6],ax ; jne ; 2a9b mov byte [0x587b],0xff
  2aa0 call 0x2900 (whole party panel redraw)
```
RNG K:2092: `ax=[0x5420]; add ax,0x9248; ror ax,1 (x3); xor ax,0x9248; add ax,0x11; [0x5420]=ax; ax&=0x7fff; cx=hi-lo+1; dx:ax / cx; dx+=lo`
(first pushed = lo [bp+6], second = hi [bp+4]); `rand(1,8) = ((seed'&0x7fff)%8)+1`. I re-derived the stream: seed 0x0C4C -> 4 (0x01AB), 8 (0xE047), 3 (0x7C2A), 8 (0xD397),
5 (0x7F04), 3 (0x1072), 1 (0xC630), 1 (0x9958); `rand(1,30)` from 0x0C4C: 8 (0x01AB), 18 (0xE047).
Effects: targets = every non-'D' slot below `min(party_size,6)` in ascending slot order; death at HP-dmg <= 0 (HP == dmg kills), HP 0, status 'D', active slot -> 0xFF
if it was the dying member; nothing else writes a status byte, so 'S' (asleep) is NOT woken and 'P' is neither added nor removed; no armour/difficulty hook; no party-wipe test
(the main loop's `K:39FC` at MO:0AA2 notices on the next iteration).
Only RNG consumer reachable from K:2AA8: `K:2ACF`. Proof: transitive closure over kernel near calls from 0x2AA8 (B's `reach2.py`, re-run: functions 0xA70 0xB10 0xB86 0x16BA 0x1850 0x1A3E 0x1B94
0x1BF2 0x1CCA 0x1CEE 0x1F12 0x1F26 0x2092 0x216C 0x223C 0x2726 0x2884 0x2900 0x2A28 0x2A52 0x2AA8 0x4C2A 0x4CCE + non-prologue leaves 0x4F3C 0xB2D 0x935 0x8E6 0x17F4 0x1F77), and the
kernel's `call 0x2092` census whose lowest site is 0x2ACF. The noise burst K:223C uses its own PRNG at [0x545C] (0x2255-0x2265), gated by the sound flag [0xA9CE] only for the speaker, and never touches [0x5420].
One unexcluded item: `0xB2D` ends in an indirect `lcall [0x5350]` (video-driver hook), which cannot be statically proven RNG-free, but it is graphics code and no `20 54` ([0x5420]) byte pair occurs in the swept leaves.

### 2.5 Passability of tile 0x2F (K:2C4C, evaluated by me)
Class table DS 0x54F4 indexed by vehicle>>2: foot 0x1C->0, horse 0x10->3, carpet 0x14->2, ship 0x20/0x24->6, skiff 0x28->5. Bit table DS 0x54D4 byte for tile 0x2F = 0xF3, mask 0x01 set (blocked).
Class 0 (K:2C6A -> 2BD4): 0. Class 3 (K:2CAE): 2BD4 = 0 -> 2CA9 -> 0. Class 2 (K:2C80): not 0x60-0x6F, 2C2E(0x2F)=0 (needs tile<4 or 0x60-0x6F), 2BD4 = 0 -> 0. Class 6 (K:2D34): `cmp tile,2 ; jle` -> tile>2 -> 0.
Class 5 (K:2CDC): tile&0xFC != 0x34 -> 2D08: 2C2E(0x2F)=0 -> 2CA9 -> 0. => 0x2F is impassable to all, so MO:02B9 never short-circuits and the tail is always reached.

### 2.6 Order of effects for a blocked step into a cactus (outdoors, `[0x5955]==0`)
1. foot/horse: footstep noise K:433E at MO:04C8 (before text; mounts `0x12`/`0x1C`). 2. (MO:00DA text: "Ride "/"Fly "/"Row "/"Head " as applicable.) 3. direction echo (MO:0507..). 4. rowed frigate only: `Rowing!` (MO:020E).
5. object lookup + terrain read, not passable. 6. `Blocked!` (MO:0322). 7. `OUCH!` (MO:032F). 8. K:2AA8: for each living slot in order: rand(1,8) -> flash -> noise_burst(10,1600,2000) -> unflash -> HP/death -> panel redraw.
9. K:1B16 keyboard flush. 10. return 0: **no beep, no clock tick**. => text before RNG; RNG interleaved with SFX (SFX consumes no game RNG); clock never.

### 2.7 No clock after the bump
`MO:0C0F mov [bp-8],ax` (return of 0x490) ... `MO:0C12-0C1B` clears `[0x5955]` when class != 0x20 ... `MO:0C30 cmp word [bp-8],0 ; jne 0xC39 ; jmp 0xD14`; the clock is `MO:0C39 mov ax,2 ; push ax ; call 0xffffcdac` (= K:4F7C
advance_clock) -> reached only for a non-zero return. 0x490 returns [bp-2] (MO:053C), i.e. 0 for every blocked path. (The per-iteration wind/getkey tick `K:5910` via MO:0598 still runs.)

### 2.8 TOWN control
`TOWN 0x083A mov ax,0x26d6` (DS 0x26D6 = `Blocked!\n`) / print / `push 0xa5; push 0xc8; call 0xffffa0f0` / `call 0xffff9946` / `[bp-6]=0` -- no `cmp ...,0x2f` anywhere in that tail. Town blocked step ticks 1 minute at
`TOWN 0x15D4 push 1 ; call 0xffffcdac`. So in a town a cactus is "Blocked!" + beep, no OUCH, no damage, for foot and boats alike.

### 2.9 Corrected census of K:2AA8 (all overlays, FITTED bases; positive control = the known ULTIMA.EXE 0x2B74 starvation site and OUTSUBS 0x05F8 "Burning!")
ULTIMA.EXE 0x2B74 (Starving!), 0x304F (chest-trap BOMB!); TOWN 0x0F8D (A TRAPDOOR!), 0x10C4 (Burning!), 0x10E4 ("Begone, vermin!"); **MAINOUT 0x0336 (THIS ITEM)**, 0x0A80 (EARTHQUAKE!), 0x1155
(DROWNING loop in 0x109E), 0x1160 (non-ship arm of 0x109E: callers MO:0D02 "Rough seas!", 0x123D, 0x1267, 0x12AF, 0x143A); CMDS 0x0D91 (powder keg); OUTSUBS 0x05F8 (fire field "Burning!"); DUNGEON 0x04F7
(energy field "Ouch!"), 0x0AEA (pit), 0x0DC3 (Fire!!/Bomb Trap!). Only MAINOUT 0x0336 is relevant. K:2A52 callers (fitted bases): ULTIMA.EXE 0x2AD3 (inside 2AA8), 0x2B40 (poison tick), 0x3032 (ACID); CMDS 0x1CE2;
DNGLOOK 0x027B, LOOKOBJ 0x03A1/0x0A20, OUTSUBS 0x04D9 (bases for DNGLOOK weakly fitted, 7 hits; irrelevant here).
MAINOUT 0x01FE: one caller (MO:0514). MAINOUT 0x0490: one caller (MO:0BB3). MAINOUT 0x109E (hull damage): MO:0303, 0D02, 123D, 1267, 12AF, 143A.

---------------------------------------------------------------------------------------------------------

## 3. Reference (TypeScript) -- exact change

File `game/src/core/game.ts`, method `resolveNavalStep` (docblock at ~1836-1838), the block at **lines 1929-1933**:
```ts
if (res.partyDamageRoll) {
  const dmg = this.rand(1, 8); // cactus 0x01FE/0xA8D8: rand(1,8) al party
  const active = this.state.characters[this.state.activeCharacter] ?? this.state.characters[0];
  if (active) active.currentHp = Math.max(0, active.currentHp - dmg);
}
```
becomes (same position: AFTER the `res.messages` loop at ~1892 and after the hull/sink handling, BEFORE the transport sync and the `res.moves` step):
```ts
if (res.partyDamageRoll) {
  partyRandomDamage(this.state, this.rand); // MAINOUT 0x0336 -> K:2AA8
  events.push({ kind: "party-changed" });    // 0x2900 inside every apply_damage (step 2 of the plan, section 5)
}
```
`partyRandomDamage` is already imported/used at game.ts:1523 (same module). `world/survival.ts:393-413` (`applyDamage` / `partyRandomDamage`) is correct as is.
Also fix wording: game.ts:1522 and 1836-1838, `world/transport.ts:435` ("el caller llama a `partyRandomDamage`"), `docs/FIDELITY.md:393`, `re/deliberate-divergences.md:246` and `:825-828` (close: measured), `re/notes/transport.md:102`
("cactus -> OUCH + party_random_damage"), `game/tests/naval-live.test.ts:299` comment ("rand(1,8) al party activo"). `shipTryMove` (transport.ts:563-565) is correct and stays (the flag is a boolean the caller honours).
Ordering already right in TS: messages are pushed first (`Blocked!`, `OUCH!`), the beep is `outcome==="blocked"` only, no hull damage on a cactus.

## 4. Native -- exact change

### 4.1 Core fix
File `native/core/src/commands.cpp`, `Runner::naval_step`, **lines 373-381**:
```cpp
if (dest == 0x2f) {
    message("OUCH!");
    const auto damage = rand(1, 8);
    auto i = c.game.party.active_character;
    if (i >= c.game.party.character_count) i = 0;
    if (i < c.game.party.character_count && !party_damage_blocked(c.game)) { // A4-ENH2: God Mode here too
        auto &hp = c.game.party.characters[i].current_hp;
        hp = uint16_t(hp > damage ? hp - damage : 0);
    }
} else event(GameEventKind::Sfx, "move-blocked");
```
becomes
```cpp
if (dest == 0x2f) {
    message("OUCH!");
    party_random_damage(c.game, rand);          // MAINOUT 0x0336 -> K:2AA8 (starvation=false: never difficulty-scaled)
    event(GameEventKind::PartyChanged);          // step 2 of the plan; K:2900 inside every apply_damage
} else event(GameEventKind::Sfx, "move-blocked");
```
(identical to the foot path, commands.cpp:502-503). `turn.cpp:63-78` `apply_damage` / `party_random_damage` are the reuse target: `i < party_size && i < 6`, skip `'D'` for `i < character_count`, a draw even for a missing roster entry, God Mode
skips only the HP write AFTER the draw (so X6's "same draws under God Mode" stays true), death sets `'D'` and `active_character = 255`. Keep the whirlpool gate (line 371), the `message("Blocked!")` (372) and the else-branch Sfx unchanged.
Update `native/core/include/openu5/enhanced.h:126-128` only if its wording changes (it already says the cactus shares `party_random_damage()`, which becomes true).

### 4.2 Collateral that breaks if the fix is applied naively
* **`native/core/tools/a4_enh2_mutation_check.py` mutant X7** ("God Mode misses the naval OUCH again") anchors on the exact line being deleted (`if (i < c.game.party.character_count && !party_damage_blocked(c.game)) {`).
  After the fix its anchor matches zero times (`--anchors` fails, the mutant is INVALID). Retarget it: e.g. anchor on the new naval pair
  `            message("OUCH!");\n            party_random_damage(c.game, rand);` (note the foot path has the same two calls at deeper indentation, so include indentation / the following `event(...)` line to keep it unique once) and mutate to an inline
  active-member roll or drop the helper call; keep `['rules2']` as the killing test. a4_enh1's apply_damage-guard mutants (a4_enh1_mutation_check.py:97-103) remain valid and X6 also kills them.
* Doc wording: `native/targets/tdeck/ALPHA4_UI.md:2835` (naval OUCH "rand(1,8) on the active member"), `:2960` and `:3340` (D-89 rows), `ALPHA2_PRESERVATION_LEDGER.md:234` (D-89 row), `GAMEPLAY_INTEGRATION_AUDIT.md:8765`, test comment `a4_enh2_rules_test.cpp:653-655`.

### 4.3 Adjacent, NOT D-89 (do not fold in silently)
* **Naval cactus in a TOWN is layer-ungated in both ports** (TS `resolveNavalStep` reached from `navalMove` with `enPueblo`; native `naval_step` has no layer test; `movement.cpp:134` gates only the foot `on_cactus`). After the D-89 fix a boat in a town would
  damage the whole party instead of one member where the binary (TOWN 0x083A) prints Blocked! + beep and damages nobody. Unreachable in real data. If gated (TS in `resolveNavalStep`/`shipTryMove` caller by location, native in `naval_step` by `position.map.location`),
  119 corpus snapshots (36 rows, v odd) change in a different way (OUCH and the HP loss disappear, `move-blocked` Sfx appears, RNG draws vanish).
* **Blocked naval step still ticks the clock** (TS `runNavalTurn` after `resolveNavalStep`, game.ts:1819/1828; native `commands.cpp:431-432` `naval_step(dir); naval_turn();`). Binary: no tick (2.7). Pinned by every blocked naval row.
* "Rough seas!" (MO:0CD6-0D02) and the DROWNING loop (MO:1148-115E) call the same helper and are not modelled; per-member flash + `combat-damage` cue (NB(10,1600,2000)) and the keyboard flush K:1B16 exist in the binary but in neither port for any overworld `party_random_damage` site.

---------------------------------------------------------------------------------------------------------

## 5. Fixtures, corpora and tests

### 5.1 Must stay byte-identical under the core fix (step 1: swap the roll for the helper, no new event)
* `native/core/fixtures/movement-flow.txt` (36,792 lines; ctest `movement_flow_parity` via `tests/movement_flow_parity_test.cpp`; `movement_flow_drift` = generator `--check`, CMakeLists.txt:76-78, 3013-3014). The corpus is a ONE-member party
  (`partySize 1`, `activeCharacter 255`, HP 100, status 'G'): one `rand(1,8)` on slot 0 == the inline roll, bit for bit (same draw, same HP, same status, same seed). Replayed by A (`d89a_mf.ts` over terrain 47): 283 OUCH snapshots,
  of which **187 are naval** (from 32..43: 68 outdoor snapshots in 20 rows, 119 town snapshots in 36 rows -- 56 rows total) and 96 foot/horse/carpet. **Zero hash changes in step 1.**
* `fixtures/commands.txt` / `commands-coverage.json` (`message:OUCH!` 288, `event:party-changed` 288): foot only (`transport:'foot'`, tile 28). `fixtures/transport-flow.txt`: board/exit only. Unchanged.
* TS `game/tests/naval-live.test.ts:288-299, 308-319` (one member HP 50, `HP<50`, Blocked! before OUCH), `transport-exact.test.ts:600-665` (pure `shipTryMove`, `partyDamageRoll:true`), `remolino-282-ocupacion.test.ts:146`,
  `colas-224-colas-bloqueo.test.ts:176-250` (cactus never beeps; foot-in-town no OUCH), `cactus-ouch.test.ts` (foot): all still pass.
* Native `tests/a4_enh2_rules_test.cpp` X6 (lines 653-690): skiff 0x2A east into 0x2F, one member HP 100, seed 0x0C4C, with/without God Mode: `hurt in [92,100)` (draw 4 -> HP 96), `god == 100`, `seed_hurt == seed_god`. Still passes; its comment must be reworded.
  `a4_enh2_preservation_test.cpp:139-149` and S2 (`a4_enh2_rules_test.cpp:527-545`) call `party_random_damage` directly: unchanged.

### 5.2 Must change only if step 2 (PartyChanged on the naval OUCH) is taken -- this is the recommended second commit
* `movement-flow.txt`: exactly the **187 snapshot hashes** (56 rows) whose event list contains `message:OUCH!` for from 32..43 change (event list gains `party-changed:` right after `message:OUCH!`; seed/HP/position unchanged);
  the 96 foot/horse/carpet snapshots and everything else stay. Procedure per "fix the reference at the pinned layer": change TS first; run the generator with `--check` as the control (must throw `movement flow drift` BEFORE regeneration, proving the corpus
  sees the change); regenerate with `generate-movement-flow-fixtures.ts`; show that exactly 187 of 36,792 hash tokens differ; then native passes `movement_flow_parity`. Native emits at the same stream position (inside `naval_step`, before `naval_turn()`'s events, before `MapChanged`).
* Event order in the corpus row: `walk-echo|message:Rowing!|message:Blocked!|message:OUCH!|party-changed|...turn events...|map-changed`.

### 5.3 Pins that do not exist today (so nothing guards the multi-member behaviour): add them (section 6).

---------------------------------------------------------------------------------------------------------

## 6. Test plan

All with a skiff 0x2A (or rowed frigate 0x25 facing east) at a free tile, cactus 0x2F to the east, seed 0x0C4C (draw stream 4,8,3,8,5,3; states 0x01AB, 0xE047, 0x7C2A, 0xD397, 0x7F04, 0x1072), written RED FIRST against the current code, in both TS (`game/tests/`, pattern of `naval-live.test.ts`/`cactus-ouch.test.ts`, with `makeChar`) and native (a new case beside X6 or a new `a4_parity2_*` test registered in CMakeLists next to `a4_enh2_rules`, 2206-2209).

1. **Multi-member (the RED case):** 3 members 'G' HP 100, `active_character = 1`. Expect HP 96 / 92 / 97, seed 0x7C2A, messages `Blocked!`,`OUCH!`, no `move-blocked`, position unchanged. Current code: only member 1 takes 4 (HP 96), members 0 and 2 untouched, seed 0x01AB.
2. **Dead skipped, no draw:** members 0,1,2 with slot 1 'D' (HP 0): draws 4,8 -> slot0 96, slot2 92, slot1 untouched, seed 0xE047 (slot 2 gets the SECOND draw).
3. **'S' and 'P' are damaged and keep status** (no wake, no poison change).
4. **Lethal boundary:** HP 4 with draw 4 -> HP 0 and 'D'; HP 5 with draw 4 -> HP 1, status unchanged; the active member dying -> `active_character == 0xFF/255`; a non-active member dying leaves the active index alone.
5. **Active index irrelevant:** `active_character = 255` and out of range: all living members damaged (old code clamped to member 0).
6. **party_size bounds:** `party_size 6` -> six draws; `party_size 2` with 4 records -> two draws; `party_size > character_count` -> draws still taken for missing records (turn.cpp:73 / survival.ts, same as the foot path).
7. **God Mode:** N draws, no HP change, seed equal to the non-God run. **Easy/Relaxed:** damage NOT scaled (`starvation=false`, S2 pattern).
8. **Rowed frigate** 0x25 (and facing): `Rowing!`,`Blocked!`,`OUCH!` in that order; hull, `sail_dir`, position unchanged; **sails-up** ship (`sail_dir != 0`, tile 0x20-0x23) into a cactus: NO `OUCH!`, `COLLISION!`, exactly one draw `rand(1,30)` (guards against an over-reaching fix), no party damage.
9. **Whirlpool control:** vehicle >= 0x20 with actor 0xEC-0xEF at the destination: no text, no beep, no draw, even over a cactus. **Non-cactus control:** mountain -> `Blocked!` + `move-blocked`, no OUCH, no draw.
10. **Foot/naval equivalence (the single assertion that would have caught D-89):** the same party/seed/cactus gives identical HP vectors and seeds via the foot path and the skiff path.
11. **Event assertion (step 2):** exactly one `party-changed` after `OUCH!` on the naval path.
12. Do NOT assert clock/minutes in these tests (the ports still tick on a blocked naval step; adjacent divergence).

What a RED-first test and a mutation must distinguish: (a) case 1 separates "one active-member roll" from "whole-party helper" (HP vector and final seed); (b) case 2 separates "skip dead with no draw" from "draw then skip" (final seed 0xE047 vs 0x7C2A); (c) case 4 separates `<=` from `<` at HP == damage and the 'D'/0xFF writes;
(d) case 8 sails-up control separates the OUCH tail from the sailing arm; (e) mutants: M1 helper replaced by active-member-only roll (killed by 1), M2 skip-dead moved after the draw (killed by 2), M3 loop bound 6 -> 5 or `party_size` ignored (6), M4 God Mode guard moved before the draw (7), M5 `PartyChanged` dropped (11), M6 beep also emitted on cactus (colas-224 test + corpus),
M7 retargeted X7 (a4_enh2_mutation_check.py) must still be KILLED by `a4_enh2_rules`.

---------------------------------------------------------------------------------------------------------

## 7. Residual unknowns (exact)

1. **No live witness.** The naval cactus cannot be produced from shipped data (14 cacti all ringed by 0x07 except one west neighbour 0x1C at (205,38); UNDER.DAT none; the only town map with cacti has no navigable tile). The static derivation has no ambiguity in the branch, helper, draw count or text; a DOSBox witness would need a save-game edit placing a boat beside a cactus.
2. **Actor-masked terrain read.** MO:0292 reads the composed 11x11 view buffer (base 0xAB02, party cell 0xABA7), which the view composer (K:5394-~5600) also writes object glyphs into. Not traced: whether an actor standing on a cactus tile hides the 0x2F (then the original would take the beep branch). The ports test the map tile. Irrelevant for the helper fix; to settle, trace K:5394-5600 for which actor classes overwrite/zero the cell.
3. **`lcall [0x5350]`** (video-driver hook reached from K:0xB2D) is the only call in the helper's transitive closure that cannot be statically bounded; no evidence it touches [0x5420].
4. **Stale `[0x5955]`.** `[0x5955]` is cleared after each command (MO:0C1B) and at MO:0306, set at MO:04AC; the ports use `sail_dir != 0 && class 0x20`. If a SAVED.GAM can store a non-zero g_sail_dir with a furled ship, the binary's first blocked step after load would take the sailing arm (COLLISION), not OUCH. Not checked whether the saved image contains 0x5955; indirect writers to the 0x5950 area were censused only by immediate address.
5. **Callers of MO:109E at 0x123D/0x1267/0x12AF/0x143A and CMDS 0x0D91** were classified only roughly (irrelevant to D-89).
6. Whether the ports' `PartyChanged`/`party-changed` triggers a roster redraw on the device and in the browser for the naval path is inferred from the foot path, not observed (grep of `native/targets` finds no direct consumer name; the runtime consumes the event list generically).
7. The brief's overlay-base table and `re/tools/callers_banda.py` BASES are wrong for OUTSUBS (should be 0xA290) and DUNGEON (0x81D0); other entries (CAST2 0xE1E0, COMSUBS, ENDGAME, SHOPPES2/3, ZSTATS, FONT) were fitted by weak counts (<= 4 hits for several small overlays) and should be re-verified individually before relying on any by-band census for them.
