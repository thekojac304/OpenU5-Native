# D-88 -- the combat clock: FINAL reconciled report

Reconciler: re-disassembled in this session; nothing in the repository was edited, built or run.
My own scripts (not the investigators' copies) are in `findings/d88f_work/`:
`lib.py` (loaders, prologue test, base fit), `bases_all.py` (base fit of all 24 overlays),
`census.py` (exhaustive E8/E9 caller census, all overlays + kernel), `local_callers.py`, `encl.py`,
`d1.py` / `kdis.py` (annotated disassembly, thunk targets resolved), `thunkmap.py`,
`model.py` / `model2.py` (executable transcription of the quoted instructions; NOT a DOSBox witness).

Conventions: overlay addresses are FILE offsets of the .OVL; kernel addresses are ULTIMA.EXE image
offsets after the 0x800-byte MZ header (the notes' CS:IP); DS strings: DATA.OVL file offset = DS + 0x10.
Near call arithmetic: `target = (addr + 3 + rel16) & 0xFFFF`, kernel address `(target + base) & 0xFFFF`.

---------------------------------------------------------------------------------------------------

## 1. Verdict

### 1.1 What the 1988 game does (all re-derived by me)

1. `COMBAT.OVL` round loop (file 0x0B94, thunk 0x7C32) counts every unit ACTIVATION in a DS **byte**
   `[0x5882]` (`inc byte [0x5882]` at 0x0C64). At the 10th it zeroes the byte and calls
   **`advance_clock(1)`** (kernel 0x4F7C, `ret 2`) at 0x0C76. It is the plain clock routine, not a
   world turn and not housekeeping.
2. The tick happens at the START of the 10th activation: after the unit's countdown was reloaded and
   the per-turn flags cleared, BEFORE the AI/human turn routine (0x03F4 / 0x063E) is entered, so before
   any of that unit's text, prompts, sounds or RNG draws. The counter is zeroed BEFORE the call.
3. The compare is `cmp byte [0x5882],0xA / jne` (equality, 8-bit wrap). A loaded value 11..255 does not
   tick until it wraps: from v the next tick comes after `(256 - v) + 10` activations.
4. `[0x5882]` is NOT combat-local. It is never initialised at combat entry or exit. It lies inside the
   0x1060-byte `SAVED.GAM` window (DS 0x55A6..0x6606) at **file offset 0x2DC**, loaded and saved
   verbatim by DOS file I/O. It survives fights, saves and loads and returns to 0 only by reaching 10
   (or a fresh INIT.GAM, whose byte is 0). Exactly three instructions in the whole program reference it.
5. What counts: every unit that survives four skip tests and whose countdown reaches 0 on this pass:
   empty slot (`flags & 0xC0 == 0`), gone slot (`flags & 0x20`), a party member whose roster status is
   'D' (swept: flag 0x20 + `0x1574(idx,0x63)`, no turn), a unit standing on tile 0x84/0x85
   (stocks/manacles: `jmp 0xD08` before the decrement, it never ticks) are NOT counted. Everything else
   counts regardless of side, control, sleep, charm, Time Stop, Quickness skips, auto-pass of a
   non-active party member, and every activation after "VICTORY!" (a victory does not end the fight).
6. Inside an arena `[0x5893]` (g_location) is **0xFF** (set at 0x5FB4, restored at 0x6091-0x6094;
   DUNGEON.OVL does the same around its direct call). Hence inside the combat tick: the sky strip /
   moon latch (0x4A84) and the clock hook (0x71AA) are skipped, and the midnight Shadowlord re-roll
   excludes nothing (compare against 0xFF).
7. `advance_clock(1)` in a fight does: minute+1 (one carry), torch and light-spell minutes -1 (saturating),
   the hourly countdown `[0x588C]` -1 on an hour crossing, hour/day/month/year rollover, the 12-hour dial
   `[0x5884]`, the light-level recompute, and at midnight the unbounded `rand_range(1,8)` Shadowlord
   re-roll, all inline in the fight's RNG stream. It does NOT touch food, HP, status, `[0x588B]`,
   `[0x588E]`: those are housekeeping.
8. **No meal, starvation damage or poison tick ever runs inside a fight.** Housekeeping (kernel 0x2AE8)
   has exactly four call sites, all in world loops (CMDS 0x0671, DUNGEON 0x0E22, MAINOUT 0x0CD3,
   TOWN 0x10D0). Poison is 1 HP per housekeeping CALL per poisoned member (not per hour); meals (06, 12,
   18) and starvation fire only on the flank `hour != [0x5880]` (prev_hour). `[0x5880]` has exactly four
   references: 0x2B5D (read), 0x2B9C (consume), 0x4FA3 (snapshot, every nonzero advance), 0x514A (read).
   Consequence: **an hour crossed by a combat tick is charged iff the LAST nonzero advance before the
   next housekeeping is the one that crossed it.**
9. After a fight the clock call at kernel 0x6356 is `advance_clock(0)` (inside 0x6150): no add and no
   snapshot, but it runs the light recompute and the 0x514A tail with the restored `[0x5893]`.
10. Charge or swallow, per entry point (traced in this session, see 2.9):
    * MAINOUT, TOWN, ship, overworld camp: **swallowed** (the next consumed turn's `advance_clock(2|1)`
      overwrites prev_hour before the housekeeping that follows it).
    * MAINOUT **bridge-troll** branch: **charged** (fight sits between `advance_clock(2)` at 0x0C3D and
      housekeeping at 0x0CD3).
    * DUNGEON corridor/room/hole-fall fights: **charged** (the fights are inside 0x0C76, whose tail runs
      0x0E22 with no advance between); a ticking fight also SWALLOWS the step's own head-advance crossing.
    * Dungeon (A)ttack returns 0: no post-step, no housekeeping: swallowed.

### 1.2 Reconciliation of A and B (STEP 1)

| point | A | B | resolution (by binary) |
|---|---|---|---|
| Dungeon fights: charged or swallowed | skeleton only, entry points "not traced" | charged immediately | **B right.** DUNGEON 0x0C76 contains both fight sites (0x0CEB corridor, 0x0D40 room) and the hole-fall room fight (0x0A4C, only caller 0x0E00) and ends in 0x0E22. Refinement: Attack (0x1D4A) returns 0, so no post-step. |
| Bridge troll | exception: charged | "swallowed on every outdoor path I traced" (troll ordering untraced) | **A right.** MAINOUT 0x0C5F `call 0x1BE8` (tile&0xFE == 0x6A) -> 0x1C84 -> 0x1B3E -> 0x1BE1 `call 0x6150`, between 0x0C3D and 0x0CD3. |
| Post-fight `advance_clock(0)` (0x6356) | optional fidelity | recommended | Exists only on the 0x6150 paths. Its only effect is the moon-latch/12h refresh when a flank is pending. TOWN already refreshes via 0x0408 (0x0413 `call 0x4A84`, 0x0511 `advance(0)`). Small, optional, see 3.6. |
| Ledger wording "no longer swallows" | exact only for command-started fights | asymmetry is the main thing | Both partly right; the real table is 1.1 item 10. |
| `[0x5876]/[0x5878]` meaning | AI-controlled / human-controlled | enemies / players (as the note) | **A right.** SJOG 0x1B6C counts by kernel 0x5646: nonzero (AI) = uncharmed monsters, party members named with 'j' at name[4] (Saduj) or flag 1 (possessed); zero (human) = other party members and charmed monsters. |
| Overlay bases | COMBAT 0xA290 | same | Both right; independently confirmed (2.1). |
| Persistence at +0x2DC | yes | yes | Confirmed (2.3). No runtime witness (7.1). |

### 1.3 Who is wrong

* **Notes.** `re/notes/combat.md`: the table row `0x5882 g_cmb_action_count` ("actions since the last minute")
  reads the instructions right but implies a combat-local counter; it is persisted save state. The exit
  bullets of section 2 have conditions swapped: SJOG 0x21CE (0x0CCA) runs when the human side is empty and
  the AI side is not (`[0x5878]==0, [0x5876]!=0`), "VICTORY!" (0x0CE8-0x0CFA) when humans remain and the AI
  side is empty; the note says the opposite for both. `re/notes/kernel-survival.md` line 87 ("2 callers", census
  incomplete) is superseded: 21 sites. Ledger row D-88 and ALPHA4_UI.md 2959/3339/3351 ("touches
  housekeeping timing after every fight, widest corpus impact") overstate: only the arena tick is new;
  swallow emerges from the existing flank; only `gameplay_parity` and `quest_parity` move (5).
* **Tooling and brief.** `re/tools/callers_banda.py` and the task brief's base table are wrong for COMBAT
  (0xBFEC), OUTSUBS, COMSUBS, SHOPPES3, LOOKOBJ, DNGLOOK, CAST, INTRO, DUNGEON, BLCKTHRN (and ENDGAME,
  SHOPPES2, CAST2's table value). `callers_banda.py 4f7c` prints 13 of the 21 sites and misses COMBAT 0x0C76.
  `thunks.py --bases` labels id 14 "base 0xbfec" (that is the SJOG stub; SJOG's call base is 0xBF80).
* **Reference (TS) and native.** Both lack: the tick; the persisted byte; the 0xFF location override for the
  Shadowlord exclusion; the post-fight `advance_clock(0)`. Both keep a per-combat unbounded activation count
  that nothing reads. Adjacent divergences that become observable once ticks exist: bridge troll and dungeon
  housekeeping order (4.5, 3.5); the 0x84/0x85 scheduler skip.

### 1.4 Adversarial spot-checks of what A and B AGREED on (STEP 2) -- all survived

| agreed claim | how I tried to refute it | outcome |
|---|---|---|
| The tick is `advance_clock(1)` at the start of the 10th activation, COMBAT base 0xA290 | independent base fit of all 24 overlays with a 4,096-candidate scan (COMBAT: 115/210 prologue hits at 0xA290 vs 0 at the brief's 0xBFEC); thunk 0x7C32 -> 0xAE24 -> 0x0B94; full disassembly of 0x0B94-0x0D2F and 0x4F7C; census of 0x4F7C over E8 and E9 in every overlay and the kernel (21 sites), plus a raw-word scan for function-pointer use (none) | not refuted; the only site reachable from inside an arena is COMBAT 0x0C76 |
| `[0x5882]` is persistent save state, never initialised at combat entry/exit | raw `82 58` scan of all binaries (3 hits, all COMBAT 0x0C64-0x0C6F); indexed-access scan for bases 0x5860..0x5882; all 84 `rep stos/movs` sites; every reference to the window base 0x55A6/end 0x6606 (found INTRO 0x0050-0x0140, read-only); decoded kernel 0x256E/0x25D8 and the DOS calls; read INIT.GAM and SAVED.GAM | not refuted; a non-`rep` bulk clear with an unseen base is the only theoretical gap (7.4) |
| combat never charges a meal, starvation or poison; the swallow comes from prev_hour | decoded 0x2AE8 in full; census of its 4 callers and of every reference to the food word 0x57A8 (COMBAT's only use is the crop pickup) and to the turn counter 0x588B; the four references of 0x5880; then traced EVERY fight starter against its loop (MAINOUT, TOWN, DUNGEON, camp) | the generic rule survived, but two entry points contradict the blanket "swallowed" reading (bridge troll, dungeon); that is the one place where agreement between the reports was incomplete, not wrong |

---------------------------------------------------------------------------------------------------

## 2. Binary facts

### 2.1 Bases and positive controls

My own fit (`bases_all.py`): fraction of an overlay's `E8` near calls whose resolved kernel target is a
`55 8B EC` prologue, best base over all 16-byte-aligned values:

| overlay | calls | best base (hits) | runner-up | brief/table value (hits) |
|---|---|---|---|---|
| COMBAT (7,408 B) | 210 | **0xA290 (115)** | 0xEB90 (39) | 0xBFEC (**0**) |
| DUNGEON | 263 | 0x81D0 (166) | 0xCAD0 (47) | 0xE1E0 (0) |
| TOWN | 216 | 0x81D0 (144) | 0xCAD0 (43) | as brief |
| MAINOUT | 270 | 0x81D0 (182) | 0x9280 (57) | as brief |
| CMDS | 284 | 0xBF80 (210) | 0x0880 (78) | as brief |
| SJOG | 268 | 0xBF80 (212) | 0xA730 (95) | as brief |
| INTRO | 614 | 0x81C0 (546) | 0x9880 (126) | 0xCD3A |
| OUTSUBS 0xA290 (68), BLCKTHRN 0xA290 (129), ENDGAME 0xA290 (114), LOOKOBJ 0xA290 (183), DNGLOOK 0xA290 (104), SHOPPES 0xA290 (210), NPC 0xA290 (13/63, weak but best), CAST 0xBF80 (154), TALK 0xBF80 (109), CAST2/COMSUBS/ZSTATS/FONT/SHOPPES2/SHOPPES3 0xE1E0 | | | | |

Positive controls actually executed:
* Thunk record at kernel 0x7C32 = `9a ec 02 2e 07 | 07 00 | ea 24 ae 00 00` -> flat 0xAE24; 0xAE24 - 0xA290 = 0x0B94
  = the round-loop prologue `55 8B EC 83 EC 0A 57 56`. (Kernel stub `0E49 call 0E1D ; 0E4C call 7C32 ; ret`,
  only caller 0x6069 inside 0x5F86.)
* `call 0xFFFFACEC` at COMBAT 0x0C76 -> (0xACEC + 0xA290) & 0xFFFF = 0x4F7C, a real prologue whose body reads
  the clock bytes; with 0xBFEC it lands mid-instruction at 0x6CD8.
* Thunk 0x7BAE (`... ea bc 88 00 00`) -> 0x88BC - 0x81D0 = MAINOUT 0x06EC (the Attack handler, prints DS 0x29FE).
* INTRO 0x0EC6 -> 0x256E (prologue) with its pushes `0x31E6 "SAVED.GAM"`, `0x55A6`, `0x6606-0x55A6`.
* `callers_banda.py 4f7c` = 13 sites; my census = 21 (below). The difference is exactly the COMBAT, DUNGEON,
  BLCKTHRN x2, CAST2, OUTSUBS, SHOPPES3 x2 sites the repo tool cannot resolve.

### 2.2 The round loop, quoted (COMBAT.OVL, flat 0xAE24; locals `[bp-2]` result=1, `[bp-8]` exit flag)

Unit table DS 0xBA14, 32 records x 8 B: +1 speed, +2 flags (0x80 party, 0x40 monster, 0x20 gone, 0x10
invisible, 0x08 asleep, 0x01 charmed/possessed), +3 roster/monster index, +5 initiative countdown, +6 x, +7 y.

```
0bca c6 06 9e 58 00      mov byte [0x589e],0           ; unit index = 0 (also entered at 0xd16 -> 0xbca)
0bcf c6 06 4f 59 00      mov byte [0x594f],0
0bd4..0bdf               si = 0xba14 + idx*8
0be3 8a 44 02            mov al,[si+2]       ; flags
0be8 a8 c0 / 75 03 / e9 19 01       test al,0xc0 ; jne ; jmp 0xd08        ; empty slot      -> not counted
0bef a8 20 / 74 03 / e9 12 01       test al,0x20 ; je  ; jmp 0xd08        ; gone            -> not counted
0bf6 a8 80 / 74 2c                  test al,0x80 ; je 0xc26
0bfa 8a 5c 03 / 2a ff / b1 05 / d3 e3 / 80 bf b3 55 44 / 75 1c            ; roster status 'D' ?
0c0a 80 4c 02 20 ; push idx ; call 0x3564 ; push idx ; push 0x63 ; call 0x1574 ; jmp 0xd08   ; swept, not counted
0c26 ... al=[0xAD14 + y*32 + x] ; 0c3a 24 fe ; 0c3c 3c 84 ; 0c3e 75 03 ; 0c40 e9 c5 00        ; tile&0xFE == 0x84 -> jmp 0xd08 (never ticks)
0c43 fe 4c 05            dec byte [si+5]
0c46 74 03 / 0c48 e9 bd 00          je 0xc4b ; jmp 0xd08                  ; countdown != 0 -> not counted
0c4b b0 24 / 2a 44 01 / 88 44 05    mov al,0x24 ; sub al,[si+1] ; mov [si+5],al   ; reload 36 - speed
0c53 2a c0                          sub al,al
0c55 a2 98 58 / a2 a2 58 / a2 8f 58 / a2 90 58 / a2 9d 58                    ; clear [5898],[58a2],[588f],[5890],[589d]
0c64 fe 06 82 58         inc byte [0x5882]             ; <<< THE COUNTER
0c68 80 3e 82 58 0a      cmp byte [0x5882],0xa
0c6d 75 0a               jne 0xc79                     ; equality
0c6f a2 82 58            mov [0x5882],al               ; al == 0 here: reset BEFORE the call
0c72 b8 01 00 / 0c75 50  mov ax,1 ; push ax            ; delta = 1 minute
0c76 e8 73 a0            call 0xffffacec -> kernel 0x4f7c
0c79 c6 06 9f 58 01      mov byte [0x589f],1
0c7e..0c8b               push idx ; call 0x5646 (side classifier) ; or ax,ax ; je 0xc90 ; call 0x3f4 (AI turn)
0c90 call 0x63e (human turn)
0c93..0ca3               [idx+0x58a8]=0xff ; push idx ; call 0x1b1e (hazard-tile effect)
0ca6 call thunk 0x7e8a (SJOG 0x1b6c recount: [0x5876] = units with 0x5646 != 0, [0x5878] = units with 0x5646 == 0)
0ca9 cmp word [0x5878],0 ; jne 0xce8
0cb0 cmp word [0x5876],0 ; jne 0xcca ; 0cb7 [bp-2]=0 ; [bp-8]=1 ; -> exit        ; both sides empty: result 0
0cca call thunk 0x7e7e (SJOG 0x21ce) ; inc ax ; jne 0xd08   ; humans gone, AI units left
0cd3 cmp byte [0x58a3],0 ; jne 0xcbc ; push 0x6eee "\nBATTLE IS LOST!" ; [bp-2]=1 ; jmp 0xcbc (exit)
0ce8 cmp word [0x5876],0 ; jne 0xd08                    ; humans left, AI units left: continue
0cef cmp byte [0x58a3],0 ; jne 0xd08 ; push 0x6f00 "\nVICTORY!\n" ; [0x58a3]=1 ; call 0x4368 ; call 0x1b16 ; falls to 0xd08 (CONTINUE)
0d08 inc byte [0x589e] ; cmp byte [0x589e],0x20 ; jae 0xcc1 ; jmp 0xbcf
0d16 cmp word [bp-8],0 ; jne 0xd1f (exit: call 0x1b16 ; [0x2186]=0xff ; ax=[bp-2] ; ret) ; jmp 0xbca (next pass)
```
Strings (DS, file offset = DS+0x10): 0x6F00 `"\nVICTORY!\n"`, 0x6EEE `"\nBATTLE IS LOST!"` (verified).

Kernel 0x5646 (classifier, quoted logic): flags=[0xBA16+idx*8]; `test 0x20` -> 0; party (0x80): if roster index != 0 and
`[0x55AC + idx3*32] == 0x6A` ('j', Saduj) -> 1, else `flags & 1`; monster: 1 if `(flags&1)==0`, else 0.
So the loop ends only when no HUMAN-controlled live unit remains; walking off the edge after victory costs
counted activations.

### 2.3 `[0x5882]` is persistent save state at SAVED.GAM+0x2DC

* Window: INTRO 0x0EB4-0x0EC6 (load), CAST2 0x1185-0x1194 (write via kernel 0x25D8; 4 writers total: names at
  DS 0x31E6, 0x9698, 0x364B, 0xA0D6 are all `"SAVED.GAM"`). `0x6606 - 0x55A6 = 0x1060 = 4192 = len(INIT.GAM) =
  len(SAVED.GAM)`. Kernel 0x256E loops `call 0x7234` (DOS 3Dh open, 42h seek, 3Fh read, 3Eh close); 0x25D8 loops
  `call 0x7296` (3Ch create, 40h write, 3Eh close). `0x5882 - 0x55A6 = 0x2DC`.
* Neighbours (same arithmetic): year 0x5874 = +0x2CE (word), time spell 0x587A = +0x2D4, transport 0x587C = +0x2D6,
  month 0x587D = +0x2D7, day 0x587E = +0x2D8, hour 0x587F = +0x2D9, **prev_hour 0x5880 = +0x2DA**, minute 0x5881 = +0x2DB,
  **0x5882 = +0x2DC**, wind drift 0x5883 = +0x2DD, hour12 0x5884 = +0x2DE, moon latches 0x5885/0x5886 = +0x2DF/+0x2E0,
  hourly countdown 0x588C = +0x2E6, time-spell turns 0x588E = +0x2E8, location 0x5893 = +0x2ED, torch 0x58A7 = +0x301,
  Shadowlords 0x58C8 = +0x322, food word 0x57A8 = +0x202, party size 0x585B = +0x2B5.
* Real files: `INIT.GAM[0x2CC..0x2DC] = 00 00 8B 00 00 00 00 00 00 FF 1C 04 05 08 00 23 00` (year 139, month 4, day 5,
  hour 8, prev_hour 0, minute 0x23, **+0x2DC = 0**); the shipped `SAVED.GAM` has +0x2DC = 0 (prev_hour 3). INIT.GAM
  Shadowlord slots (+0x322..0x324) = `00 00 00` (all < 0x80, so a midnight re-roll draws for all three).
* References: raw `82 58` scan of ULTIMA.EXE and all 24 .OVL (DATA.OVL included): exactly COMBAT 0x0C66 (inc),
  0x0C6A (cmp), 0x0C70 (mov). No indexed access with a base in 0x5860..0x5882 (scanned: only direct operands and
  instruction-boundary noise). All 84 `rep stos/movs` sites: none covers 0x5882 (the 4 EXE `rep stosb` are C runtime
  command-line code). The only code touching the whole window besides load/save is INTRO 0x0050-0x0140, a read-only
  loop (`test`/`cmp` of `[bx+si]`). **Nothing initialises it at combat entry/exit.**

### 2.4 `advance_clock` -- kernel 0x4F7C, quoted (arg word `[bp+4]`, minutes)

```
4f84 837e0400 / 4f88 7503 / 4f8a e91401   cmp [bp+4],0 ; jne 4f8d ; jmp 50a1   ; n==0: tail only, NO snapshot, NO add
4f8d 803e7a5851 / 4f92 750c               cmp [0x587a],0x51('Q') ; jne 4fa0
4f94 d17e04 / 4f97 837e0400 / 4f9b 7503 / 4f9d ff4604   sar [bp+4],1 ; cmp 0 ; jne ; inc [bp+4]   ; n=1 stays 1 under 'Q'
4fa0 a07f58 / 4fa3 a28058                 mov al,[0x587f] ; mov [0x5880],al        ; prev_hour = hour (BEFORE the 'T' test)
4fa6 803e7a5854 / 4fab 741b               cmp [0x587a],0x54('T') ; je 4fc8          ; 'T': no add, no torch/light
4fad 8a4604 / 4fb0 00068158               mov al,[bp+4] ; add [0x5881],al           ; byte add
4fb4 b8a758 50 ff7604 e878ef              &torch,n -> call 0x3f36 ; 4fbe: &light-spell [0x58a6],n -> 0x3f36
4fc8 803e81583b / 4fcd 7703 / 4fcf e9cf00 cmp [0x5881],0x3b ; ja 4fd2 ; jmp 50a1    ; unsigned, ONE carry
4fd2 802e81583c                           sub [0x5881],0x3c
4fd7 b88c58 50 b80100 50 e854ef           &[0x588c],1 -> 0x3f36                     ; hourly countdown -1
4fe2 fe067f58 / 4fe6 803e7f5817 / 4feb 7703 / 4fed e9b100    inc [0x587f] ; cmp 0x17 ; ja 4ff0 ; jmp 50a1
4ff0 c6067f5800                           hour = 0                                  ; midnight
4ff5..504b  for i in 0..2: cmp byte [0x58c8+i],0x80 ; jae skip ;
            { 5004 push 1 ; push 8 ; call 0x2092 ; di=ax ; 5011 al=[0x5893] ; cmp ax,di ; jne ; di=0 ;
              501c for si in 0..2: cx = (byte [0x58c8+si] == cx ? 0 : cx) ; 5037 or di,di ; je 0x5004 (re-draw) } ; [0x58c8+i]=al
5051 fe067e58 / 5055 803e7e581c / 505a 7642   inc [0x587e] ; cmp 0x1c ; jbe 509e
505c..506f  zero [0x585a],[0x5859],[0x5858],[0x57b2],[0x5959] ; [0x587e]=1 ; 5072 loop 16 roster records (0x55bf+0x20*i): if <0x19 inc
508a..509a  inc [0x587d] ; cmp 0xd ; jbe ; [0x587d]=1 ; inc word [0x5874]
509e e85fd8 call 0x2900 (status redraw) ;  50a1..513b light level [0x58a5] ; sets [0x24e6]=1 if it changed
514a a08058 / 38067f58 / 7433    mov al,[0x5880] ; cmp [0x587f],al ; je 5186        ; hour changed IN THIS CALL (or pending)
5153 803e935821 / 730a            cmp [0x5893],0x21 ; jae 5164                       ; arena (0xFF) skips the sky strip
515a 803e955880 / 7303            cmp [0x5895],0x80 ; jae 5164
5161 e820f9                       call 0x4a84                                        ; sky strip + moon latch (writes [0x5885],[0x5886])
5164..5183                        [0x5884] = hour==0 ? 12 : hour>12 ? hour-12 : hour  ; runs in combat too
5186 803e935800 / 740a / 803e935821 / 7303 / e81320   [0x5893]==0 -> skip ; >=0x21 -> skip ; else call 0x71aa
5197 5e 5f 8b e5 5d c2 0200       ret 2
```
Helpers: 0x3F36(ptr,n) `if ([ptr] > n) [ptr] -= n else [ptr] = 0` (unsigned byte, `jbe`); 0x2092 rand_range:
`s = ((ror3(s + 0x9248) ^ 0x9248) + 0x11)` stored at `[0x5420]`, result `lo + ((s & 0x7FFF) % (hi-lo+1))`, one state step per call.
The midnight loop draws once per attempt per slot (slots with `< 0x80` only), unbounded retry, rejecting the party's
location `[0x5893]` AND any value equal to any of the three slots, including the slot's own old value.

### 2.5 `turn_housekeeping` -- kernel 0x2AE8, quoted

```
2b0b..2b55  for i < [0x585b]: st = [0x55b3+0x20*i]; 'D'(0x44): if i == [0x587b] then [0x587b]=0xff ;
            'D'/'S'(0x53): no eat ; 'P'(0x50): push i ; push 1 ; call 0x2a52 (1 HP) ; every non-D/S member (P included): eaters++ ([bp-2])
2b5d a08058 / 2b60 38067f58 / 2b64 7439      cmp [0x587f],[0x5880] ; je 2b9f        ; FLANK
2b66 833ea85700 / 750d                       cmp word [0x57a8],0 ; jne 2b7a
2b6d b8c854 50 e8dcec e831ff                 print DS 0x54c8 "Starving!\n" ; call 0x2aa8 (rand(1,8) per non-'D' member i<size)
2b7a  hour 6 / 0xc / 0x12 -> &food,eaters -> call 0x3f54 (word saturating sub)
2b99 a07f58 / a28058                         [0x5880] = [0x587f]                    ; consume the flank
2b9f  [0x588b] saturating +1 ; [0x588e] time-spell turns -- (0 -> [0x587a]=0 + 0x2900) ; call 0x400c (ring regen)
```
Starvation hits on EVERY hour change while food == 0 (not only meal hours). Poison runs on every call, not only on the flank.

### 2.6 Exhaustive census (my `census.py`, E8 and E9, every overlay with its fitted base + the kernel; positive control = COMBAT 0x0C76 found)

**`advance_clock` 0x4F7C: 21 sites, nothing else (no raw word `7C 4F`, no function-pointer use anywhere).**

| site | arg (`preceding push`) | what | relevant to D-88 |
|---|---|---|---|
| **COMBAT 0x0C76** | 1 | the combat tick | THE NEW CALL |
| ULTIMA.EXE 0x6356 | 0 | tail of 0x6150 after a field/town fight | yes (post-fight tail) |
| MAINOUT 0x0C3D | 2 | world-loop advance | yes (the swallow) |
| TOWN 0x15D4 | 1 | town loop advance | yes (the swallow) |
| DUNGEON 0x0F2F | 1 | head of every dungeon iteration (before the key read at 0x0F36) | yes (dungeon order) |
| TOWN 0x0511 | 0 | end of floor loader 0x0408 | yes (town fight tail) |
| MAINOUT 0x0006, 0x0066, 0x0A56 | 0 | light/tail refresh on loads | no |
| MAINOUT 0x0462, 0x068C | 2 | slow terrain, ship leg | no |
| TOWN 0x1328 | 0x14 | inn/jail | no |
| ULTIMA.EXE 0x3CFC | 5 | ship repair inside the "Hole up &" command (kernel 0x3C9A; strings DS 0xA2C2 "Hole up & ", 0xA2CE "\nrepair...\n\n", 0xA306 "camp!\n\n"; called from the dispatcher at 0x3296; the key letter was not verified) | no |
| CMDS 0x0318, 0x064B | 5, 10 | camp / wait | no |
| BLCKTHRN 0x05B4, 0x0C2E | 2, 9 | Blackthorn | no |
| CAST2 0x10F1 | 0x10 | spell | no |
| OUTSUBS 0x0993 | 0 | not combat | no |
| SHOPPES3 0x01C1, 0x01DB | 5, 9 | inn | no |

Only COMBAT 0x0C76 is reachable from inside an arena. **`turn_housekeeping` 0x2AE8: 4 sites:** TOWN 0x10D0 (tail of fn 0x0F02, sole
caller TOWN 0x15EC right after 0x15D4), MAINOUT 0x0CD3 (reachable only through 0x0C3D), DUNGEON 0x0E22 (tail of fn 0x0C76, sole caller
0x0F84), CMDS 0x0671 (fn 0x0552, a command; no local callers). No kernel, COMBAT, COMSUBS, CAST, SJOG caller, no function pointer.

**Fight starters.** kernel 0x6150 (5): TOWN 0x09D0 (fn 0x09BC), MAINOUT 0x077C (Attack fn 0x06EC), 0x080F (Enter fn 0x0790, location 0x27
planting a 0xFC actor), 0x1310 (fn 0x1248, actor loop), 0x1BE1 (fn 0x1B3E, bridge troll). kernel 0x5F86 (5): DUNGEON 0x0C53 (fn 0x0B7E
corridor, flags 2), DUNGEON 0x1DB5 (fn 0x1D4A, dungeon Attack, flags 2), EXE 0x0E5D (stub 0x0E5A <- jmp 0x6373 in wrapper 0x6360 <- 0x3EE2:
overworld camp, mode 4), EXE 0x0E69 (stub 0x0E66 <- jmp 0x3E7C: dungeon camp, mode 6), EXE 0x6347 (inside 0x6150, mode 0). Loop thunk 0x7C32 (2):
EXE 0x0E4C (the stub, only caller 0x6069 inside 0x5F86) and DUNGEON 0x00C4 (direct, room arena builder fn 0x0000; callers 0x0B5E (in fn 0x0A4C),
0x0D40, 0x0EFC). Positive control for the census method: `callers_banda.py` prints 13/21 for 0x4F7C, mine 21/21 including its 13.

### 2.7 `start_combat` 0x5F86 and the 0x6150 wrapper

* 0x5F86 entry (args bp+8 mode, bp+6, bp+4): `5FA8 mov al,[0x5893] ; 5FAB mov [0x5894],al ; 5FB4 mov byte [0x5893],0xff`; restored at
  `6091 mov al,[0x5894] ; 6094 mov [0x5893],al`; also restores `[0x5896]/[0x5897]/[0x5895]` (0x607F-0x608E), sets `[0x24E6]=1`, calls 0x2900
  (0x609C). No clock call.
* Modes: 0 field/town (0x6150), 2 dungeon corridor, 4 overworld camp, 6 dungeon camp. For mode 4/6 (0x602E `test [bp+8],4`) the loop is entered
  only if `call 0x8076` (thunk -> CMDS+0x0000, the camp routine; it starts by clearing `[0x588E]` and `[0x587A]`) returns nonzero (0x6040
  `or ax,ax ; je 0x606C`). Every other path reaches 0x605C (`[0x587B]=[0x589E]=0xFF`, `[0x58A3]=0`) then 0x6069 `call 0x0E49`.
* 0x6150 converges on 0x633A for every case (`push idx ; call 0x60EC` arena load; `push 0 ; push npc ; push 0 ; call 0x5F86 ; push npc ; call 0x8016`
  = SJOG 0x203E cleanup, which also does `[0x5893]=[0x5894]` (0x20CA-0x20CD); `call 0x5E4A ; 6353 sub ax,ax ; push ax ; 6356 call 0x4F7C`).
  So every field/town/shipboard fight started through 0x6150 ends with **`advance_clock(0)`** with the restored location.
* TOWN fn 0x09BC (town attack, callers 0x0B3A in the Attack command fn 0x09E6, 0x1408 in the guard/NPC engine fn 0x1352): `call 0x52 ;
  push [..]; call 0x6150 ; call 0xB0 ; push 0 ; call 0x408 ; call 0x2AE` and 0x0408 starts with `0413 call 0x4A84` and ends with `0511 advance_clock(0)`.

### 2.8 World-loop orders (this decides charge vs swallow)

* **MAINOUT loop (fn 0x0A84):** command -> `[bp-8]` = handler result (0x0C0F) -> `0C30 cmp [bp-8],0 ; jne 0C39 ; jmp 0D14`
  (no advance, no housekeeping when the handler returned 0) -> `0C3D advance_clock(2)` -> terrain: `0C56 (tile&0xFE)==0x6A -> call 0x1BE8 (bridge
  troll) ; jmp 0CD0`, swamp 0x0C64.., lava 0x0C7E.. -> `0CD0 call 0xA60 ; 0CD3 call 0x2AE8` -> `0D11 call 0x1A60` (actor loop: monster-initiated
  fights via 0x131A -> 0x1399 -> 0x1248 -> 0x1310). The Attack handler 0x06EC sets `[bp-2]=0` (0x06F2, 0x072B) and never changes it: it returns **0**
  (kernel dispatcher 0x3178 stores AX into `[bp-2]` and returns it, 0x3231/0x31EE).
* **TOWN loop (fn 0x141E):** command -> `[bp-0xA]` (0x159A) -> `0x15B6 cmp [bp-0xA],0 ; je 0x1686` -> `0x15BF call 0x39FC ; inc ax ; jne 0x15C8` -> `0x15D4
  advance_clock(1)` -> `0x15EC call 0x0F02` (post-move effects; tail `0x10D0 call 0x2AE8`) -> `0x165F` guard_wander, `0x166E` NPC tick, `0x1683 call 0x1352`
  (NPC engine; fights via 0x1408). Housekeeping is reachable only after 0x15D4.
* **DUNGEON loop (fn 0x0E2E):** `0F93..0FD4` (party-state check via 0x39FC) -> **head** `advance_clock(1)` at 0x0F2F (normal path 0x0FEA `mov di,1 ; mov ax,di ;
  jmp 0F2E`; 'Q': every second iteration, `xor di,1` at 0x0F25; 'T': never, 0x0F34 `sub di,di`) -> `0F36 call 0x3D6` (key read) -> `call 0x6C4` (command, result `si`;
  the default result is 1, 0x06CB) -> if `si != 0` (0x0F7B `or si,si ; je 0x0F87`): `0F84 call 0x0C76` (post-step): sleeper wake loop, wanderer `call 0x7E2` and, if
  it fired, **corridor fight `0x0CEB call 0xB7E`** (-> 0x0C53 `call 0x5F86`), tile 0xF0/0xA0 **room fight `0x0D40 call 0x0000`**, tile 0x61/0x69 hole **`0x0E00 call 0x0A4C`**
  (which can start a room fight at 0x0B5E) -> common tail `0E1F call 0x2900 ; 0E22 call 0x2AE8`. A room fight on entering the dungeon (0x0EFC) precedes the first
  head advance. Dungeon (A)ttack (kernel dispatcher 0x322E -> thunk 0x7CAA -> DUNGEON 0x1D4A) returns `[bp-2] = 0` (set at 0x1D51, never changed), so `si == 0`: no
  post-step, no housekeeping that iteration.

### 2.9 Charge / swallow table (derived from 2.6-2.8; this is the table the tests must pin)

| fight started by | where | relative to the loop's last advance and housekeeping | a tick-crossing hour |
|---|---|---|---|
| overworld (A)ttack, 0x077C | command | handler returns 0: no advance, no housekeeping this iteration; the next consumed turn's `advance(2)` overwrites prev_hour | swallowed |
| Enter at location 0x27 (0x080F), overworld camp (0x3E7C/0x3EE2 mode 4), field commands | command | before 0x0C3D | swallowed (whatever AX returns: 0xCD3 is reachable only after 0x0C3D) |
| **bridge troll 0x1BE1** | between 0x0C3D and 0x0CD3 | `advance(2)` -> fight -> housekeeping | **charged iff the last tick (or `advance(2)` if no tick) crossed** |
| monster-initiated (actor loop 0x1310) | after 0x0CD3 | next iteration's `advance(2)` overwrites | swallowed |
| town (A)ttack (0x09D0 via fn 0x09E6) | command | before 0x15D4 | swallowed |
| town guards/NPC engine (0x1408 via 0x1352) | after 0x10D0 | next iteration | swallowed |
| dungeon corridor 0x0C53, room 0x0D40, hole room 0x0B5E | inside 0x0C76 | `head advance` -> fight -> `0x0E22` | **charged iff the last tick crossed; a ticking fight swallows the head advance's own crossing** |
| dungeon (A)ttack 0x1D4A | command, returns 0 | no 0x0C76 | swallowed |
| dungeon room on entry 0x0EFC | before the first head advance | next head advance overwrites | swallowed |
| dungeon camp (mode 6, 0x3E7C) | command | depends on AX returned by the Hole-up command (kernel 0x3C9A) | **unknown** (7.2) |

### 2.10 Interactions in one place

* Food/starvation/poison: none during the fight; only the flank logic above. Only COMBAT reference to food is 0x0381/0x039B (the arena crop pickup, D-87's twin), not a meal.
* Moon/time state: arena tick never refreshes the moon latch or clock hook (0xFF). The post-fight `advance_clock(0)` (0x6356, and TOWN 0x0408) does when a flank is pending and `[0x5893] < 0x21`,
  `[0x5895] < 0x80`. The 12-hour dial `[0x5884]` is updated inside the arena.
* 'T' (Time Stop): the tick only does `prev_hour := hour` (discarding a pending flank); minute and torch unchanged. 'Q': arg 1 stays 1. The camp routine clears `[0x587A]` before an ambush.
* Encounters: the tick does not gate spawns; spawn thresholds read `[0x587F]` later.
* RNG: the tick draws only at 23:59 -> 00:00, for each Shadowlord slot `< 0x80`, in slot order, unbounded; with INIT.GAM's `00 00 00` that is >= 3 draws; with all slots >= 0x80 zero.
* Sound/UI: the tick emits no text and no sound. The device ambient chime re-arms from a change of `game_.time` (alpha_runtime.cpp:3881-3894), which matches 0x5164-0x5183 running in combat.
* Light: 0x50A1 recomputes `[0x58A5]` and `[0x24E6]`; the ports compute light on demand.

---------------------------------------------------------------------------------------------------

## 3. Reference (TypeScript) -- exact change needed

Today: no combat clock at all. `actionCount` (combat.ts:625-627, per `Combat` instance, unbounded) is incremented at combat.ts:1203 and read by nothing but the fixture generators.
`Combat` already receives the live state (`state: this.state` at game.ts:7239, 7946, 8054) and already imports from `world/survival.js` (combat.ts:38), so no new module edge.

3.1 `game/src/core/state.ts` after `prevHour?: number;` (line 217): `combatClock?: number; // g_cmb_action_count DS:0x5882, SAVED.GAM +0x2DC (u8); undefined == 0`.
3.2 `game/src/core/saveNative.ts`: `const COMBAT_CLOCK_OFFSET = 0x2dc;` next to `PREV_HOUR_OFFSET` (line 70; fix the "hole 0x2DC-0x2E1" comment at 75); write next to line 980
    `if (state.combatClock !== undefined) gam[COMBAT_CLOCK_OFFSET] = state.combatClock & 0xff;`; read next to line 1069 following the `prevHour` pattern. New game value 0.
3.3 `game/src/core/world/survival.ts`: `relocateShadowlordsAtMidnight(state, rand)` (204-218) compares `state.position.location`; add an optional third parameter `partyLocation = state.position.location`
    and thread it as a 5th optional parameter of `advanceClock` (298-387, the call at ~346). The combat tick passes 0xFF. No other change to `advanceClock` (it is faithful: n==0 skip, 'Q', snapshot before 'T', one carry).
3.4 `game/src/core/combat/combat.ts`: import `advanceClock`; in `findNextActor` right after `this.actionCount++` (line 1203), before `skipsForActiveChar` (1209):
    ```ts
    this.tickCombatClock();            // COMBAT 0x0C64-0x0C76
    ```
    ```ts
    private tickCombatClock(): void {
      const st = this.opts.state;
      if (!st.time) return;                       // bare states of the fixture generators / 48 unit tests
      st.combatClock = (((st.combatClock ?? 0) + 1) & 0xff);
      if (st.combatClock === 10) {                // equality, 8-bit wrap
        st.combatClock = 0;                       // BEFORE the call
        advanceClock(st, 1, (lo, hi) => this.crng.randRange(lo, hi), undefined, 0xff);   // no sky ctx; location 0xFF
      }
    }
    ```
    `findNextActor` has exactly one caller (the `currentUnit` getter, 1293-1298, cached), so one tick per activation. `actionCount` stays untouched (hashed by the fixtures).
3.5 `game/src/core/game.ts` `endCombat` (8103): OPTIONAL fidelity item, for fights not in a dungeon (`!this.dungeonState`), right after the RNG write-back (`this.liveRng.seed(this.combat.finalSeed)`, 8167) / before
    the `if (townFight)` block (8190): `advanceClock(this.state, 0, undefined, this.skyRefreshCtx);` (kernel 0x6356). Effect is only the moon-latch refresh when a tick crossed an hour.
3.6 NOT required for D-88, but explicitly recorded as divergences that this fix makes observable: (a) `dungeon/dungeon-cmds.ts` 326, 410, 437 run `advanceTurn` (clock + housekeeping) BEFORE the command and the fight;
    the binary is head advance -> command -> fight -> housekeeping. (b) `world/loops/turn.ts` `outdoorTurn` runs housekeeping (step 7) BEFORE the bridge-troll toll/fight that `game.ts` resolves after it (2427-2492);
    the binary runs troll -> housekeeping. Fixing (a) also moves housekeeping's RNG draws (ring regen `rand(0,7)`, starvation) relative to the command's draws. Decide: document as new ledger rows (recommended for A4-PARITY2)
    or restructure with a dedicated batch (touches `dungeon_flow_parity`, `turn_parity`).
3.7 Tests: TS vitest states without `time` or `combatClock` must keep working (guard); tests that build `GameState` literals need no edit.

## 4. Native -- exact change needed

Today: `CombatState::action_count` (combat.h:108, `uint32_t`, rebuilt by `initialize_combat` at combat.cpp:1164-1333) is incremented at combat.cpp:265 (`++s.action_count`, right after `a.counter = uint8_t(36 - a.speed)` at 264,
before the Set-Active-Player skip) and never read. `CombatContext` (combat.h:139-148) already holds `GameState &game; TurnState &turn;`.

4.1 `native/core/include/openu5/turn.h` `TurnState` (9-21): add `uint8_t combat_clock = 0; // g_cmb_action_count DS:0x5882, +0x2DC`. Extend `advance_clock` (decl 79, def turn.cpp:25-62) with a trailing
    `int32_t party_location = INT32_MIN` meaning "use `g.position.map.location`" (the exclusion compare at turn.cpp:48); the combat tick passes 0xFF.
4.2 `native/core/src/combat.cpp` `Engine::current()` immediately after `++s.action_count;` (265): 
    ```cpp
    if (++c.turn.combat_clock == 10) { c.turn.combat_clock = 0; const Rand r = arena_rand(); advance_clock(g, c.turn, 1, &r, nullptr, 0xFF); }
    ```
    (uint8_t wraps at 256; equality; reset before the call). `arena_rand()` must draw from `s.rng` (the arena stream, the same one `Engine::rand` at 75-80 uses) so a midnight re-roll sits at the
    right place of the stream and `finish_encounter_combat` writes it back (1838); route it through the trace sink with its own site name. The cached early return at the top of `current()` must not tick.
4.3 Persistence: `native/core/src/persistence.cpp:40` add `{"combatClock", 0x2dc, 1}` to `optional_bytes[]` (read/written by the loops at 330 and 445); default-0 list at line 560;
    `native/core/src/save_core.cpp`: `F("combatClock", combat_clock)` next to 175 (capture) and 407 (restore), domain list at 231; `native/core/src/pc_save.cpp`: `kCombatClock = 0x2DC`
    (21-25), import near 121 (`s["combatClock"]`), export near 248 (`b[kCombatClock]`), B-table entry. Today 0x2DC is mapped nowhere (an imported original save drops the counter; an export writes 0).
    A load must NOT clear it (it is persistent, unlike `reset_transient_after_load()` state); a new game starts at 0.
4.4 Optional fidelity: `finish_encounter_combat` (combat.cpp:1805-1870), for non-dungeon fights (no `world.dungeon_context`), after the `world.game.rng.seed(...)` (1838) and `CombatEnded`:
    `advance_clock(world.game, world.turn, 0, nullptr, world.sky);` (kernel 0x6356; `CommandContext` carries `TurnState &turn` and `const SkyRefresh *sky`, commands.h ~124-133).
4.5 Recorded, not required: dungeon order `dungeon_orchestration.cpp:192-193` (`advance_turn` then `dungeon_action`, fights in `translate(c)` after) and `commands.cpp:789`; bridge troll order in `outdoor_turn`
    (turn.cpp:156-195, housekeeping at the end before the troll is resolved). Same decision as 3.6.
4.6 Native `advance_clock` vs the binary (checked): minutes==0 branch, 'Q', snapshot-before-'T', one carry, torch/light saturation, month zeros: faithful. Differences that remain: no `[0x588C]` countdown, no light
    recompute, Shadowlord re-roll gated on `has_shadowlords` (binary: only on slot `< 0x80`), `skull_tree_day`/`reagent_days` model only part of the five zeroed bytes. None is introduced by D-88.

---------------------------------------------------------------------------------------------------

## 5. Fixtures and corpora

Design assumption: counter outside `Combat`, TS hook guarded by `state.time`, `actionCount`/`action_count` untouched, tick draws only at midnight.

**Must stay byte-identical (no regeneration):**
* `fixtures/combat.txt`, `combat-negate.txt` (`combat_parity`, `combat_negate_parity`; hashes `finalSeed, actionCount, scanIdx, ..., s.food, HP/xp, draws`); `fixtures/advanced-combat.txt`
  (`advanced_combat_parity`, includes `timeSpell` 'T'/'Q' states and `timeSpellTurns`); `fixed-combat/fixed-maps/fixed-enemies/transport.txt` (`world_flow_parity`; hashes `torchTurns`, generator state `torchTurns:0` so a tick saturates at 0);
  all of them are produced by TS generators whose states have no `time` (hook inert). On the native replay the tick DOES run (native always has `time`), but nothing hashed changes: time is not hashed, RNG draws occur only at midnight
  and the replays start at 00:00 (default `GameTime{0,1,1,0,0}`, `has_shadowlords=false`), so reaching 23:59 would need ~14,000 activations.
* `fixtures/dungeon-flow.txt` (`dungeon_flow_parity`, 05:59, prevHour 5, `combatants:[]` mock): unchanged unless the dungeon step order is restructured (3.6/4.5); then only event order could move.
* `commands.txt`, `turns.json/.inc` (`turn_parity`), `travel/movement-flow/transport-flow`, `foundation.*`: no real arena or no combat clock involved.
* `persistence_parity`/`core_parity`: byte-identical only if the new byte is optional and not emitted for states that lack it (follow `prevHour`); run them.
* `typescript_*_fixture_drift` (generators with `--check`): must pass with the TS change as specified.

**Change (both sides identically, no stored data):**
* `gameplay_parity` (`tools/check-gameplay.ts` vs `gameplay_driver`; real `Game.startCombat -> fight/flee (repeat 2000/300) -> quick -> escape -> end`; start time 139/4/5 at 00:00 or 12:00 (`s.time.hour=seed%2?0:12`);
  projection `fields` include `time`, `turnsSinceStart`, `food`, `torchTurns`): after N activations `time.minute` gains floor(N/10) and rolls hours/days; `torchTurns` stays 0 (start 0). Add `combatClock` to the projection `fields` so a one-sided counter bug turns it red.
* `quest_parity` (`tools/check-quests.ts` vs `quest_driver`): constructs `new Combat({... state: s ...})` DIRECTLY (not through `Game`), so the hook must live in `Combat` (3.4), which is where it is.
* Tests that must be extended: persistence round trips (`a4_save3_pc_bridge_runtime`, `batch27_alt_load`, `batch28_save_validation`, TS `saveNative` tests) where they enumerate fields.
* Native tests that drive real fights and read the clock/torch were checked statically: `a4_enh1_preservation` (hashes day/hour only; the combat golden hashes HP/status/cell), `a4_enh1_rules`, `a4_enh2_rules`, `debug_developer`,
  `presentation`, `batch21a_dungeon_room` (`torch_turns = 500`, a light gate), `dungeon_combat` (`torch_turns = 200`) do not assert a clock value after a fight; `batch12_combat_field` and `batch21a3_combat_active_player` assert `action_count`
  deltas (unchanged). TS vitest: `attack.test.ts` asserts only that STARTING an attack costs no minute; `refuge-live.test.ts:445` is a town party-wipe without activations. Confirm by running, not by this list.
* Out of ctest: `game/src/core/__parity__/combat-run.ts` (oracle runner, real `createNewGame` state, so it would tick; start hour 8).

---------------------------------------------------------------------------------------------------

## 6. Test plan (timeline from `model.py`/`model2.py`; start S: 12:30, day 5, torch 50, light 0, prev_hour 12, food 100, 3 eaters, no time spell, counter c0)

### 6.1 Counter and clock

| scenario | clock after fight | prev_hour | counter | torch | draws |
|---|---|---|---|---|---|
| c0=0, 9 activations | 12:30 | 12 | 9 | 50 | 0 |
| c0=0, 10th activation (tick at the START of the 10th, before its turn) | 12:31 | 12 | 0 | 49 | 0 |
| c0=0, 11 / 19 / 20 / 21 / 30 | 12:31 / 12:31 / 12:32 / 12:32 / 12:33 | 12 | 1 / 9 / 0 / 1 / 0 | 49/49/48/48/47 | 0 |
| c0=7, 3 activations; c0=9, 1 activation | 12:31 | 12 | 0 | 49 | 0 |
| fight A ends at 9, fight B's first activation | tick in fight B | | | | counter survives fight, save and load |
| c0=12 (edited), 244 / 245 / 254 activations | 12:30 / 12:30 / 12:31 | 12 | 0 / 1 / 0 | 50/50/49 | 0 (tick only at 254) |
| c0=255, 1 activation; 11 activations | 12:30 (wrap, no tick); 12:31 | 12 | 0; 0 | 50; 49 | 0 |
| torch 1 / 0, c0=9 | 12:31 | 12 | 0 | 0 / 0 | 0 |
| 'T', 05:59 c0=9 | 05:59 | 5 (= hour) | 0 | 50 | 0 |
| 'Q', 12:30 c0=9 | 12:31 (1, not 0.5) | 12 | 0 | 49 | 0 |
| fight ends on the 9th activation / the 10th is the last | counter 9, clock +0 / counter 0, clock +1 (tick fires at the start of that activation, then the unit leaves) | | | | |
| roster-'D' member, flag-0x20 slot, empty slot | not counted | | | | |
| asleep enemy, charmed, Quickness-skipped enemy, non-active member auto-pass, every post-victory walk-out turn | counted | | | | |
| unit on tile 0x84/0x85 | not counted (ports do not model this: DUNGEON.CBT index 40 has tile 0x85 at (7,2),(8,2) and map unit 0, sprite 0x50, at (8,2); index 38 has none under a unit) | | | | |

### 6.2 Hour crossings (after an overworld (A)ttack-started fight, delta-0 tail, then one outdoor world turn `advance(2)` + housekeeping)

| start | c0 / activations | after fight | after the world turn | food | note |
|---|---|---|---|---|---|
| 12:59 | 9 / 1 | 13:00, prev 12, `[0x588C]` -1 | 13:02, prev 13 | 100 | non-meal hour |
| 05:59 | 9 / 1 | 06:00, prev 5 | 06:02, prev 6 | **100** | meal SWALLOWED (port today: 06:01, food 97) |
| 11:59, 17:59 | 9 / 1 | 12:00 / 18:00 | 12:02 / 18:02 | 100 | same |
| 06:59, 07:59 | 9 / 1 | 07:00 / 08:00 | 07:02 / 08:02 | 100 | no difference |
| 05:58 | 9 / 1 | 05:59 | 06:01, prev 6 | **97** | the world turn crosses: charged as today |
| 08:59 food 0 | 9 / 1 | 09:00 | 09:02 | 0 | "Starving!" SWALLOWED, 0 draws (port today: 09:01, "Starving!", 3 draws) |
| 23:59, slots [3,5,7] | 9 / 1 | 00:00, day+1, prev 23 | | | >= 3 `rand_range(1,8)` draws inside the arena stream; the party's town (even 3) not excluded; slots [0x80,0x80,0x80]: 0 draws; [0,0,0]: >= 3 |
| day 28 23:59 month 13 | 9 / 1 | day 1, month 1, year+1, five bytes zeroed, roster `+0x17` counters +1 (cap 0x19) | | | |

### 6.3 Charged paths (bare housekeeping after the fight)

| path | start | activations | result at housekeeping |
|---|---|---|---|
| dungeon, head `advance(1)` crosses | 05:59 -> 06:00 | 10 (tick -> 06:01) | NO meal (head crossing swallowed) |
| dungeon, head crosses, no tick | 05:59 | 3 | meal (food 97) |
| dungeon, tick crosses | 05:58 -> 05:59, c0=9 | 1 | meal charged |
| dungeon 08:59 food 0 | head crosses 09:00 | 10 | no starvation; tick-crossing variant (08:58, c0=9): starvation, 3 draws |
| dungeon, poisoned member, 40 activations | | | 1 HP lost at the single housekeeping, none during the fight |
| bridge troll | 05:57 -> 05:59 | 10, c0=9 (tick crosses) | meal charged (food 97) |
| bridge troll | 05:58 -> 06:00 | 5, c0=0 (no tick) | meal charged (the advance's own crossing) |
| bridge troll | 05:58 -> 06:00 | 10, c0=9 (tick re-snapshots) | no meal |
| any, 05:59, c0=0, 20 activations | tick 1 crosses, tick 2 re-snapshots | | no meal; with 05:58: tick 2 crosses: meal |

### 6.4 RED-first and mutation guidance

RED today (each fails on the current tree, passes after): the 10th-activation clock tick; tick-before-turn (observe the clock inside the 10th turn, before the first prompt); counter survives two fights; counter round trips
through SAVED.GAM +0x2DC (TS `saveNative`, native `persistence`, PC bridge both ways); equality/wrap (c0=11 ticks at 254); the swallow (05:59 fight then a move gives food 100; 05:58 gives 97); midnight draws come from the arena RNG (>= 3, none excluded
by a town location); 'T'/'Q' rows; no housekeeping in combat (poison HP, food 0 keeps HP and prints nothing, `turnsSinceStart`/`timeSpellTurns` unchanged over 100 activations).
Controls that must pass before and after: 9 activations change nothing; skipped/dead slots do not count; `action_count` stays monotone per fight and hashed identically.
Mutations the suite must kill: threshold 9 or 11, `>=` for `==`; reset at combat entry; tick AFTER the turn; counting only player or only AI turns; counting before the skip tests (counts the 'D' sweep);
2 minutes instead of 1; housekeeping or meals called from the tick; passing a sky context (moon latch refreshed in combat); excluding the world location from the Shadowlord roll; drawing from the live RNG instead of the arena RNG;
not persisting or loading +0x2DC; clearing the counter in `reset_transient_after_load()`; 8-bit wrap replaced by a wider counter.

---------------------------------------------------------------------------------------------------

## 7. Residual unknowns (exact)

1. **No runtime witness.** Persistence of +0x2DC and every charge/swallow outcome are proven from static code, not from a DOSBox run. A watch on `DS:5882` across a fight, a quit-save, a reload (`re/tools/dosbox.conf`), or an
   original SAVED.GAM written after a fight with a non-multiple-of-10 activation count would settle it (none in the tree: both references hold 0).
2. **Dungeon camp ambush (mode 6).** Whether the Hole-up command (kernel 0x3C9A) returns nonzero (so DUNGEON 0x0C76 and the housekeeping at 0x0E22 run with the pending flank) depends on AX left at kernel 0x3EE5 by DUNGEON 0x0134 (thunk 0x7C7A) after the stub
   0x0E66 -> 0x3E7F; not decoded. The overworld camp (mode 4) is swallowed whatever AX is. Also not decoded: what CMDS+0x0000 returns (it gates whether the arena loop runs at all, 0x6040).
3. **Enter at location 0x27 (MAINOUT 0x0790)** returns whatever AX the call to 0x6150 leaves; it cannot change the classification (the fight precedes 0x0C3D) but the world turn that follows is undetermined.
4. A bulk clear of `[0x5882]` by a non-`rep` loop with a base outside 0x5860..0x5882 cannot be excluded by pattern scans; a direct-reference scan, `rep` scan and INTRO window loop (read-only) found none.
5. `0x71AA` (clock hook) and the body of `0x4A84` beyond its gates (and its UI effect) were not read; the moon-latch consequence of a missing post-fight `advance_clock(0)` rests on the gate arithmetic at 0x514A-0x5161 and the writes at 0x4AEB/0x4B25.
6. COMBAT 0x03F4/0x063E (turn bodies) were read for their call sites only; the claim that no clock, housekeeping or damage-per-hour routine is called from them rests on the exhaustive census of 0x4F7C and 0x2AE8.
7. Unit record creation (kernel 0x6506-0x65F9: countdown, speed, flag setup) was taken from the investigators, cross-checked only against the uses at 0x0B94/0x5646/0x1B6C.
8. Arena lighting after a tick that changes `[0x58A5]` and `[0x24E6]` (a torch burning out mid-fight) was not derived visually.
9. The 0x84/0x85 scheduler skip is a binary rule neither port models; whether the DUNGEON.CBT index 40 prisoner (unit 0, sprite 0x50, at the manacle tile (8,2)) is meant never to act is not established; porting it would change the `real_arena_*` corpora.
10. Dungeon head-advance-per-iteration versus the ports' per-consumed-command cost (the ports skip the clock for Attack and other non-consuming commands; the binary advances at the head of every iteration unless 'T') was
    observed but not adjudicated here; it is outside D-88.
