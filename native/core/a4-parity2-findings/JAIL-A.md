# ITEM JAIL -- investigator A (binary first) -- A4-PARITY2

Scope: does the 1988 jail (arrest) wait loop and the inn sleep loop call the per-turn
housekeeping (meals, starvation, poison, world tick, clock) per elapsed unit, and what do
they call instead. Read-only. Everything below marked "binary" was disassembled in this
session with `re/tools/dis16.py` / my own capstone wrappers (scratch scripts in the same
folder as this report: `dann.py` annotated disassembler, `cg.py` per-overlay call graph,
`census2.py` corrected by-band census, `basefind.py`/`basecmp.py`/`ovlmap.py` base
derivation, `jailsim.py` loop simulator). I derived the binary facts BEFORE opening any
note or port source; sections 4-6 are the comparison done afterwards.

---------------------------------------------------------------------------------------

## 1. Verdict

1. **The note is wrong.** `re/notes/kernel-survival.md` lines 293-295 ("Carcel/posada
   TOWN:0x1324: bucles de advance_clock(20) hasta la hora objetivo ... con comidas/hambre
   aplicandose en cada cambio de hora") is wrong twice:
   - Neither the jail loop (TOWN.OVL 0x1324-0x1330) nor the inn night loop (SHOPPES3.OVL
     0x01b5-0x01f4) ever calls the per-turn housekeeping routine (kernel 0x2AE8, the only
     routine that eats meals, prints "Starving!", ticks poison). Both call only the clock
     routine kernel 0x4F7C (plus, in the inn only, the ring-of-regeneration sweep 0x400C and
     redraws). Meals/hunger/poison/starvation do NOT run in either wait.
   - "posada TOWN:0x1324" is a conflation: TOWN 0x1324 is only the jail. The inn is
     SHOPPES3.OVL 0x01b5-0x01f4 and steps 12 x 5 min then 9-min steps, not 20-min steps.
2. **Housekeeping question: ports already match the binary.** Neither port runs housekeeping
   in the jail wait or the inn wait (native `blackthorn.cpp:61`, `shops.cpp:329`; TS
   `blackthorn.ts:741`, `shops.ts:461`). On the exact thing the note claims, only the note
   is wrong (documentation correction, no production change).
3. **Production DOES diverge in the jail wait, on a different axis: the clock.** The binary
   runs `advance_clock(20)` in a loop until `g_hour == 8` (66 iterations from 10:00).
   Both ports skip the loop and assign `hour = 8; minute = 0` (TS `guardArrestJail`, native
   `blackthorn_action` Arrest/Yes). Consequences (all derived in section 2): no calendar day
   advance when the wait crosses midnight (the clock goes BACKWARDS 22:00 -> 08:00 same day),
   no torch / light-spell consumption, no midnight Shadowlord re-roll and none of its RNG
   draws, wrong wake minute (binary: first 20-minute sample inside hour 8, i.e. 0..19; ports:
   always 0), no month/year rollover or month-end zeroing. The TS comments call it a
   "canonical observable" with "dia no modelado"; the binary says otherwise. The inn already
   went through exactly this fix (#122, `re/notes/aud-relojes-acta.md` section 1); the jail
   was left behind.
4. **Inn: ports match the binary** loop for loop (12 x advance_clock(5), then
   ringRegen + advance_clock(9) until hour 6, tile refresh at hours 20/5). One small
   ordering divergence found (section 5/6): both ports apply the wake-up heal/poison-death
   BEFORE the night loop, the binary does it AFTER, so a poisoned party member who wears the
   Ring of Regeneration draws no RNG in the ports but draws once per iteration in the binary.
   Rare; RNG-stream only.
5. Adjacent (not this item, flagged for the lead, section 8): sleep-in-bed (CMDS 0x0552) DOES
   call the housekeeping every 10-minute step (CMDS 0x0671 -> kernel 0x2AE8) -- native
   matches, the TS reference `camp.ts bedSleepStep` does not. The Refuge wait (BLCKTHRN
   0x0C2A) has the same direct-set shape as the jail in both ports.
6. Tooling warning that matters for any further census: the overlay base table in the task
   brief and in `re/tools/callers_banda.py` is wrong for 11 of 22 overlays (section 2.1).
   The old tool misses the inn's own clock calls and the DUNGEON housekeeping caller.

Confidence: high for the call-graph facts (positive controls in 2.1/3); high for the jail
loop arithmetic; medium for the exact real-DOS behaviour under Time Stop (static derivation
only, nothing was executed).

---------------------------------------------------------------------------------------

## 2. Binary facts

Conventions: addresses are ULTIMA.EXE image offsets (`--exe`, = CS:IP of the kernel) or
overlay FILE offsets. DS strings: DATA.OVL file offset = DS offset + 0x10 (verified by
reading the strings quoted below). `K<hhhh>` = kernel address. Stack args are pushed
left-to-right (callee reads `[bp+4]` = last pushed); all calls here are near, callee pops
(`ret n`).

### 2.1 Overlay near-call bases (CORRECTED; positive control)

The task brief's table (COMSUBS 0x85FE, SHOPPES3 0xA5F6, COMBAT 0xBFEC, DUNGEON 0xE1E0,
BLCKTHRN 0xE63E, LOOKOBJ 0xA444, DNGLOOK 0xA2B6, CAST 0xA8D8, INTRO 0xCD3A, OUTSUBS 0x81D0,
TALK 0xA290, SHOPPES2 0xA89E, ENDGAME ...) does not survive the prologue-landing test. I
tested every external `E8 rel16` of each overlay against all bases 0x0000-0xFFFF for
`55 8B EC` at the kernel target, and separately decoded the 24 PLINK86 load records at
ULTIMA.EXE 0x7780 (16 bytes each; word 5 = load segment). They agree:

| band | near_call_base | overlays (measured) |
|---|---|---|
| 1 | 0x81D0 | TOWN, MAINOUT, DUNGEON (INTRO 0x81C0: relocation header) |
| 2 | 0xA290 | FLAMES, NPC, COMBAT, BLCKTHRN, LOOKOBJ, DNGLOOK, OUTSUBS, SHOPPES, ENDGAME |
| 3 | 0xBF80 | SJOG, CMDS, CAST, TALK |
| 4 | 0xE1E0 | CAST2, ZSTATS, COMSUBS, SHOPPES2, SHOPPES3, FONT |

Evidence: e.g. SHOPPES3 `call 0x3670` (30 sites) -> base 0xE1E0 gives K1850 = `55 8b ec`
(the print routine; base 0xA5F6 gives 0xDC66, outside the kernel); COMBAT `call 0x75c0`
(34 sites) -> 0xA290 gives K1850, 0xBFEC gives 0x35AC (`f6 47 02`, not a prologue);
DUNGEON `call 0x9680` (43 sites) -> 0x81D0 gives K1850; COMSUBS `call 0x3670` -> 0xE1E0
gives K1850. Prologue-landing counts at the corrected base vs. the brief's base:
COMSUBS 82/107 vs 0, SHOPPES3 85/113 vs 0, COMBAT 115/170 vs 0, DUNGEON 166/195 vs 0,
BLCKTHRN 129/142 vs 0, INTRO 546/563 vs 0, OUTSUBS 68/73 vs 0 (the brief's values are the
minimum thunk target of each overlay, which is NOT the load address when the first exported
function is not at offset 0). The thunk "ovl N" numbers = the PLINK record order
(1 TOWN, 2 MAINOUT, 3 DUNGEON, 4 INTRO, 5 FLAMES, 6 NPC, 7 COMBAT, 8 BLCKTHRN, 9 LOOKOBJ,
10 DNGLOOK, 11 OUTSUBS, 12 SHOPPES, 13 ENDGAME, 14 SJOG, 15 CMDS, 16 CAST, 17 TALK,
18 CAST2, 19 ZSTATS, 20 COMSUBS, 21 SHOPPES2, 22 SHOPPES3, 23 FONT). Example: thunk 0x81A2
= ovl 22 target 0xEA94 = 0xE1E0 + 0x8B4 = SHOPPES3 file 0x8B4 (the inn dispatcher, prologue
confirmed). `re/notes/overlay-load-layout.md` section 1 holds the same table (its
`re/tools/dispatch_table.py` is absent from the tree).

Positive control for `census2.py` (my corrected census): K1850 (print string) call sites
found per file at the measured base: BLCKTHRN 42, CAST 68, CAST2 49, CMDS 67, COMBAT 34,
COMSUBS 18, DNGLOOK 9, DUNGEON 43, ENDGAME 31, FONT 2, INTRO 86, LOOKOBJ 28, MAINOUT 53,
OUTSUBS 21, SHOPPES 88, SHOPPES2 65, SHOPPES3 30, SJOG 91, TALK 33, TOWN 39, ULTIMA 79,
ZSTATS 38 (every file non-zero). Cross-check against the old tool: the old
`callers_banda.py` found only 13 callers of K4F7C and 3 of K2AE8; the corrected census
finds 21 and 4 (the missing ones are exactly in the overlays whose base it has wrong).

### 2.2 What the kernel clock routine K4F7C does (`advance_clock(n)`, `ret 2`)

```
4f7c  push bp / mov bp,sp / sub sp,0xa / push di / push si
4f84  cmp word [bp+4],0 ; jne 4f8d ; 4f8a jmp 50a1       ; n==0: skip to light/sky tail
4f8d  cmp byte [0x587a],0x51 ; jne 4fa0                  ; 'Q' Quickness
4f94  sar word [bp+4],1 ; cmp word [bp+4],0 ; jne ; inc word [bp+4]   ; n>>=1, min 1
4fa0  mov al,[0x587f] ; mov [0x5880],al                  ; g_prev_hour = g_hour   (ONLY write besides 0x2b9c)
4fa6  cmp byte [0x587a],0x54 ; je 4fc8                   ; 'T' Time Stop: skip minute add
4fad  mov al,[bp+4] ; add [0x5881],al                    ; minute += n   (byte)
4fb4  push 0x58a7 ; push n ; call 3f36                   ; sat-sub u8: light-spell minutes -= n
4fbe  push 0x58a6 ; push n ; call 3f36                   ; sat-sub u8: torch minutes -= n
4fc8  cmp byte [0x5881],0x3b ; ja 4fd2 ; jmp 50a1        ; no carry -> tail
4fd2  sub byte [0x5881],0x3c                              ; ONE carry only
4fd7  push 0x588c ; push 1 ; call 3f36                   ; byte [0x588c] -= 1 per hour (sat)
4fe2  inc byte [0x587f] ; cmp byte [0x587f],0x17 ; ja 4ff0 ; jmp 50a1
4ff0  mov byte [0x587f],0                                 ; midnight
4ff5..5048  for i in 0..2: if byte [0x58c8+i] < 0x80:     ; Shadowlord re-roll
              repeat { di = rand(1,8) [K2092(1,8)]; if di==[0x5893] di=0;
                       for j in 0..2: if [0x58c8+j]==di: di=0 } until di!=0
              [0x58c8+i] = di
5051  inc byte [0x587e] ; cmp byte [0x587e],0x1c ; jbe 509e      ; day++ ; >28 -> month
505c  [0x585a]=[0x5859]=[0x5858]=[0x57b2]=0 ; [0x587e]=1 ; [0x5959]=0
5072  for each char (0x55bf, stride 0x20, to 0x57bf): if byte < 0x19 inc   ; monthsAtInn
508a  inc byte [0x587d] ; cmp 0xd ; jbe ; [0x587d]=1 ; inc word [0x5874]   ; month, year
509e  call 2900                                            ; status redraw
50a1  tail: light level [0x58a5] from hour/minute tables (0x6a80), torch/light floors 0x12/0x0a
514a  mov al,[0x5880] ; cmp [0x587f],al ; je 5186          ; hour changed in THIS call?
5153  cmp byte [0x5893],0x21 ; jae 5164 ; cmp byte [0x5895],0x80 ; jae 5164
5161  call 4a84                                            ; sky strip + moon-phase latch ([0x5885]/[0x5886])
5164  [0x5884] = 12h-clock hour ; 5194 call 71aa (clock display, driver call 0x6c)
519c  ret 2
```
Conclusion: K4F7C is the clock + light timers + calendar + Shadowlord re-roll (RNG!) + sky
latch + display. It does NOT eat food, damage anyone, tick poison, or move NPCs. It leaves
`[0x5880]` (prev hour) stale after the hour flips; only the next 4F7C call (4fa3) or the
housekeeping (2b9c) overwrites it. `[0x587a]`: 0x51 'Q', 0x54 'T' (ASCII; matches the ports'
`timeSpell`). Variable roles confirmed by consumers: `[0x587f]` hour, `[0x5881]` minute,
`[0x587e]` day, `[0x587d]` month, `[0x5874]` word year, `[0x57a8]` word food, `[0x57aa]` word
gold (`SHOPPES3 0x11d sub [0x57aa],ax`), `[0x57ac]` keys (ZSTATS 0x40e prints it after DS
0x9742 "\n\n Keys.......", SJOG `dec [0x57ac]` at 0xc34/0xcc9/0xdf6),
`[0x5893]` location, `[0x5895]` floor, `[0x5896]/[0x5897]` x/y.

### 2.3 What the per-turn housekeeping K2AE8 does (`ret`, no args)

```
2ae8  push bp ; sub sp,8 ; push di/si ; [bp-2]=0 (eaters) ; [bp-4]=0
2afa  if [0x585b] (party size) == 0 goto 2b5d
2b03  p=0x55b3 (status byte of member 0, stride 0x20); i=0
loop  st=[p]
      if st=='D'(0x44) and i==[0x587b] : [0x587b]=0xff            ; 2b14-2b25
      if st=='D' or st=='S'(0x53) -> next                          ; 2b2c-2b36 (not counted)
      if st=='P'(0x50): push i ; push 1 ; call 2a52                ; poison: 1 HP via kernel 2a52 (HP -= 1,
                                                                    ;   <=0 -> HP=0, status 'D')
      inc [bp-2]                                                   ; 2b43  eater count (poisoned count)
2b5d  mov al,[0x5880] ; cmp [0x587f],al ; je 2b9f                  ; HOUR CHANGED since the last latch?
2b66  cmp word [0x57a8],0 ; jne 2b7a
2b6d  push 0x54c8 ; call 1850 ; call 2aa8                          ; food==0: "Starving!\n" (DS 0x54c8)
                                                                    ;   2aa8: each member i<min(party,6),
                                                                    ;   status!='D': 2a52(i, rand(1,8))
2b7a  hour in {6,12,18}: push 0x57a8 ; push [bp-2] ; call 3f54     ; food = max(0, food - eaters)  (word, floors at 0)
2b99  mov al,[0x587f] ; mov [0x5880],al                            ; consume the edge  (0x2b9c)
2b9f  push 0x588b ; push 1 ; push 0xff ; call 3ef0                 ; [0x588b] = min(0xff, +1)  (g_turn_count)
2bae  if [0x588e]!=0 and !=0xff: dec [0x588e]; if 0: [0x587a]=0 ; call 2900   ; Q/T spell duration
2bca  call 400c                                                    ; ring regen sweep
```
K400C (ring of regeneration): for each member i < [0x585b] with status != 'D' and
equipment byte [0x55c5+0x20*i] == 0x2C: `K2092(0,7)==7` (1 in 8) -> HP = min(maxHP, HP+1)
via K3f14, `[0xa9fa]=1`. RNG: one draw per living ring wearer per call, none otherwise.
Draw order in K2AE8 total: poison (no RNG) -> starvation `rand(1,8)` per living member (only
when the hour changed and food==0) -> ring draws.

### 2.4 The JAIL (TOWN.OVL 0x12AE, called only from 0x13D6 inside 0x1352, which is called
only from the town main loop 0x141E at 0x1683; not a thunk target)

```
12ae  push bp ; sub sp,4 ; [bp-4]=0
12b9  cmp byte [0x5893],0x12 ; jne 12d8        ; location 0x12 (palace): K39fc, K0e72, 11f0(1), ret 0 -- no wait, no clock
12d8  push 0x27e2 ; call K1850   ; DS 0x27e2 = "\n\"Thou art under arrest!\"\n\n"
12df  push 0x27fe ; call K1850   ; DS 0x27fe = "\"Wilt thou come quietly?\"\n\n:"
12e6  call K266c (getkey) ; cmp al,0x4e 'N' ; je 12f4 ; cmp al,0x59 'Y' ; jne 12e6
12f4  cmp byte [bp-2],0x59 ; jne 133c
'Y':
12fa  push 0x281b ; call K1850   ; "Yes\n\nThe guard strikes thee unconscious!\n"
1301  push 0 ; call 0x88a0 (-> K0a70, set_color(0) driver call 0x2d)
1307  push 0x2845 ; call K1850   ; "\nThou dost awaken to...\n"
130e  mov byte [0x5893],4          ; location 4 (Yew)       <- BEFORE the wait loop
1313  mov byte [0x5896],0x19       ; x = 25
1318  mov byte [0x5897],4          ; y = 4
131d  mov byte [0x24e6],1          ; redraw mark
1322  jmp 132b
1324  mov ax,0x14 ; push ax ; call 0xffffcdac  (-> K4f7c)   ; advance_clock(0x14)  (word operand 20)
132b  cmp byte [0x587f],8 ; jne 1324                         ; while (hour != 8)
1332  sub al,al ; mov [0x57ac],al                            ; keys = 0
1337  mov [0x5895],al                                        ; floor = 0           <- AFTER the loop
133a  jmp 12cd -> push 1 ; call 11f0                         ; town reload with NPC reposition
134b  return [bp-4] (=0)
'N' (133c): push 0x285e "No\n\n\"Then defend thyself, rogue!\"\n" ; call 0x958 (alarm) ; [bp-4]=1 ; ret 1
```
Calls reachable from the 'Y' path (cg.py): K39fc, K0e72, TOWN 0x11F0, K1850, K266c, K0A70,
K4F7C, TOWN 0x958 ('N' only). TOWN 0x11F0 (arg 1): clears the NPC table, K7a76, K7aa6(hour),
0x408 (map rebuild: K4be8, K4a84, ..., `if hour<5||hour>0x13: call 0x170`, K5e4a, **K4F7C(0)**
at 0x511, 0x212, 0x1694 NPC schedule placement for the current hour), 0x2AE (Shadowlord
placement), K2900, K39fc, K5910, 0x11B8, 0x1156. None of them is K2AE8 (census 3.1).

So the wait loop's ONLY callee is K4F7C(20). Exact semantics of the loop, derived:
- iterations = number of 20-minute steps until the hour field first equals 8 (test is at
  the loop head: if `[0x587f]` is already 8, zero iterations and the minute is untouched);
- Quickness (`[0x587a]==0x51`) halves the step to 10; Time Stop (`==0x54`) stops the minute
  add, the hour never changes and the original loops forever (no exit, no cap);
- every step: light/torch minutes sat-sub by the step, minute add with one carry; every
  hour flip: `[0x588c]` -1, K4a84 sky latch if `[0x5893]<0x21 && [0x5895]<0x80`; every
  midnight: Shadowlord re-roll (rand(1,8), `di==[0x5893]` excluded -- and `[0x5893]` is
  ALREADY 4 here, so Yew is excluded, not the town the party was arrested in), day++, and
  at day 29 the month rollover (zero the reagent/skull-tree day stamps, monthsAtInn++, month,
  year);
- the floor byte is still the OLD floor during the loop (reset at 0x1337);
- the stale-latch consequence for housekeeping: the last step leaves `[0x5880]=7`,
  `[0x587f]=8`. Nothing calls K2AE8 before the next town turn, and that turn starts with
  `K4F7C(1)` (TOWN 0x15d4) which rewrites `[0x5880]=8`; hour 8 is not a meal hour in any
  case. So even the final 7->8 edge never reaches the food code. The 6:00 meal edge crossed
  during the wait is never seen either.

Simulated results (`jailsim.py`, faithful to 4F7C incl. single carry):

| arrested at | iterations | midnights crossed | wake | with Quickness (10-min step) |
|---|---|---|---|---|
| 00:00 | 24 | 0 | 08:00 | 48 it, 08:00 |
| 03:05 | 15 | 0 | 08:05 | 30 it, 08:05 |
| 07:40 | 1 | 0 | 08:00 | 2 it, 08:00 |
| 07:59 | 1 | 0 | 08:19 | 1 it, 08:09 |
| 08:00 / 08:30 | 0 | 0 | unchanged (08:00 / 08:30) | 0 |
| 09:00 | 69 | 1 | 08:00 next day | 138 it |
| 09:59 | 67 | 1 | 08:19 next day | 133 it, 08:09 |
| 10:00 | 66 | 1 | 08:00 next day | 132 it |
| 12:35 | 59 | 1 | 08:15 next day | 117 it, 08:05 |
| 21:00 | 33 | 1 | 08:00 next day | 66 it |
| 23:50 | 25 | 1 | 08:10 next day | 49 it, 08:00 |

### 2.5 The INN (SHOPPES3.OVL, base 0xE1E0; dispatcher 0x08B4 entered through thunk 0x81A2
from TALK 0x01C5; rest function 0x0072, args `[bp+6]`=shop/inn id, `[bp+8]`=price modifier)

Sleep section (annotated listing at 0x01a8):
```
0160-016f  [0x5896],[0x5897] = room x,y from the inn table
0177       [bp-2] = [0x587b]                      ; save "who is leader"
0193       for each member: if status=='G'(0x47) -> 'S'(0x53)    ; everybody falls asleep
01a8  call K2900 ; call K5910 ; print DS 0x4e44 "Zzzzzz....\n\n"
01b5  si = 12
01bd  push 5 ; call 0x6d9c (K4F7C)            ; advance_clock(5)  -- 12 times, NO hour test
01c4  dec si ; je 01ef ; jmp 01bd
01ca  loop body:
        push 1 ; call K3ae6                    ; 1 animation frame (gated by [0x58a4]); no RNG
        call K400C                             ; ring regen sweep
        call K2900                             ; status redraw
        push 9 ; call K4F7C                    ; advance_clock(9)
        cmp [0x587f],0x14 ; je 01ec ; cmp [0x587f],5 ; jne 01ef
01ec    call 0xffff98ba (K7a9a -> TOWN 0x170)  ; window/lamp tile transform, EVERY iteration whose
                                               ;   resulting hour is 20 or 5 (a xor 0xdd TOGGLE, ~7x/hour)
01ef  cmp [0x587f],6 ; jne 01ca                ; loop until hour == 6 (test at the head, after the 12x5)
01f6  print DS 0x4e51 "Morning!\n"
01fd  call 0xffff98ae (K7a8e -> TOWN 0x1694)   ; NPCs placed at their hour-6 schedule slot
0200..0282  for each member (si=0x55a8, stride 0x20), skipping status 'D':
        HP[+0x10] = maxHP[+0x12]
        class[+0xa]=='A'(0x41) or 'M'(0x4d): MP[+0xf] = INT[+0xe];  'B'(0x42): MP = INT>>1
        if status=='P': status='D'; HP=0; K16ba(0x0a); print name; print DS 0x4e5b " has\npassed away.\n"
        elif status=='S': status='G'
0285  [0x587b] = saved ; inc [0x5896] (step out of bed) ; [0x24e6]=1 ; K2900 ; ret -1
```
Callees of the whole function 0x72 (cg.py, base 0xE1E0): K16ba, 0x2c (helper), K0442/K0496
(long mul/div for the price), K7fb6, K1850, K7f62, K7f6e, K7fda, K2900, K3ae6, K39cc, K5910,
K4F7C (2 sites), K400C, K7a9a, K7a8e, K16ba. **No K2AE8.** K400C is the last call of K2AE8's
body, so the inn runs exactly that tail of the housekeeping and nothing else.

Timing facts: total minutes = 60 + 9*n where the loop exits at the FIRST landing inside hour
6 (wake minute 0..8); if the 60-minute prelude already leaves the hour at 6 (entering at
05:xx) no 9-minute step runs and the minute can be anything; if you rest at 06:xx the
prelude moves to 07:xx and the loop runs the whole day (~23 h) -- original quirk, the ports
inherit it. Quickness halves each step (5>>1=2, 9>>1=4). Time Stop (`T`) hangs the original;
the TS port caps at 1000 steps (declared).

### 2.6 Contrast: the other waits (to prove the scan discriminates)

| wait | where | calls K4F7C(n) | K400C | K2AE8 |
|---|---|---|---|---|
| jail | TOWN 0x1324 | 20, until hour==8 | no | no |
| inn night | SHOPPES3 0x1c1/0x1db | 5 x12, then 9 until hour==6 | yes, per 9-min step | no |
| camp (Hole up) | CMDS 0x0000 fn; K4F7C at 0x318, K400C at 0x207; starts with `[0x588e]=[0x587a]=0` | 5 per step | yes | no |
| bed sleep (Z) | CMDS 0x0552 fn; K4F7C at 0x64b | 10 per step | via K2AE8 | **yes, every step** (0x671), after K4a84, before K2900/K7a8e/K368e gate |
| Refuge wake | BLCKTHRN 0x0C2A-0x0C36 | 9 until hour==6 (after `[0x588e]=[0x587a]=0`) | no | no |
| normal town turn | TOWN 0x15d4 | 1, then 0x170 at hours 20/5, then 0xF02 (K2AA8 hazards...) ending in K2AE8 at 0x10d0 | via K2AE8 | yes |

### 2.7 DS strings quoted (DATA.OVL file offset = DS + 0x10; read in this session)
0x27e2 `\n"Thou art under arrest!"\n\n` | 0x27fe `"Wilt thou come quietly?"\n\n:` |
0x281b `Yes\n\nThe guard strikes thee unconscious!\n` | 0x2845 `\nThou dost awaken to...\n` |
0x285e `No\n\n"Then defend thyself, rogue!"\n` | 0x4e44 `Zzzzzz....\n\n` | 0x4e51 `Morning!\n` |
0x4e5b ` has\npassed away.\n` | 0x4e13 `"Have a pleasant\nnight, ` | 0x54c8 `Starving!\n` |
0x421e `Zzzzzzz...\n` (bed) | 0x422a `Thrown out of bed!\n` | 0x41ec `Party rested!\n` (camp).

---------------------------------------------------------------------------------------

## 3. Callers census (corrected bases; `census2.py`; over-reports by construction, flagged "ALT BASE" hits are excluded)

### 3.1 K2AE8 (housekeeping) -- ALL callers
| caller | note |
|---|---|
| TOWN 0x10D0 | end of TOWN 0x0F02 (per-step hazard + upkeep); 0x0F02's only caller is TOWN 0x15EC in the main loop 0x141E. RELEVANT: this is the normal town turn, not the jail |
| MAINOUT 0x0CD3 | overworld turn |
| DUNGEON 0x0E22 | dungeon turn (the old tool missed it: wrong base) |
| CMDS 0x0671 | sleep-in-bed step |
No call from ULTIMA.EXE itself, COMBAT, SHOPPES3, TOWN 0x12AE/0x11F0/0x0408/0x1694/0x170, NPC, BLCKTHRN, CAST2, OUTSUBS. Hence jail and inn cannot reach it directly; indirect reach is impossible because the only way into 0x0F02 is the main loop.

### 3.2 K4F7C (clock) -- ALL callers (21)
| caller | arg | relevance |
|---|---|---|
| TOWN 0x1328 | 0x14 | **the jail loop (relevant)** |
| TOWN 0x15D4 | 1 | standard town turn |
| TOWN 0x0511 | 0 | map rebuild resync (reached from the jail's 0x11F0 -> 0x408) |
| SHOPPES3 0x01C1 | 5 (x12) | **inn prelude (relevant)** |
| SHOPPES3 0x01DB | 9 | **inn night loop (relevant)** |
| CMDS 0x0318 | 5 | camp step |
| CMDS 0x064B | 10 | bed-sleep step (followed by K2AE8 at 0x671) |
| MAINOUT 0x0006, 0x0066, 0x0A56 | 0 | overworld resyncs |
| MAINOUT 0x0462, 0x068C, 0x0C3D | 2 / 1-or-2 / 2 | overworld turns/actions |
| DUNGEON 0x0F2F | 1 (Quickness: only every other turn, `xor di,1`) | dungeon turn |
| COMBAT 0x0C76 | 1, every 10th action (`[0x5882]` counter == 0x0A) | combat clock (D-88) |
| BLCKTHRN 0x0C2E | 9, until hour==6 | Refuge wake loop |
| BLCKTHRN 0x05B4 | 2 | Blackthorn castle routine, not examined beyond the operand |
| OUTSUBS 0x0993 | 0 | resync, not examined further |
| CAST2 0x10F1 | 0x10 | a spell routine (sets `[0x5893]` from `[0xbd15]` first), not examined further |
| ULTIMA.EXE 0x3CFC | 5 (up to 5x, only if `[0x587c]&0xfc==0x24`) | kernel loop, not examined further |
| ULTIMA.EXE 0x6356 | 0 | resync, not examined further |

### 3.3 Other routines of interest
- K400C (ring regen): CMDS 0x0207 (camp), SHOPPES3 0x01D1 (inn), ULTIMA 0x2BCA (inside K2AE8), ULTIMA 0x67F6.
- K2AA8 (starvation/party damage): CMDS 0x0D91, DUNGEON 0x04F7/0x0AEA/0x0DC3, MAINOUT 0x0336/0x0A80/0x1155/0x1160, OUTSUBS 0x05F8, TOWN 0x0F8D/0x10C4/0x10E4, ULTIMA 0x2B74 (K2AE8 starvation), 0x304F.
- K2A52 (member damage): CMDS 0x1CE2, DNGLOOK 0x027B, LOOKOBJ 0x03A1/0x0A20, OUTSUBS 0x04D9, ULTIMA 0x2AD3/0x2B40/0x3032 (K2AE8 poison = 0x2B40).
- K3F54 (food word sat-sub): COMBAT 0x03A3, SHOPPES 0x01B1, SHOPPES2 0x0450, TALK 0x1272, ULTIMA 0x2B96 (K2AE8), 0x2E16.
- Thunk K7A9A (-> TOWN 0x170 tile transform): CMDS 0x0664 (bed), SHOPPES3 0x01EC (inn). Thunk K7A8E (-> TOWN 0x1694 NPC placement): CMDS 0x0677, SHOPPES3 0x01FD, plus TOWN 0x051D (map load). Thunk K7A46 (-> TOWN 0x11F0): BLCKTHRN 0x0C5D, ULTIMA 0x00F7, 0x4876.
- Writers/readers of `[0x5880]` (prev hour): exactly four byte-pattern hits in the whole install: write ULTIMA 0x2B9C and 0x4FA3, read 0x2B5D and 0x514A.

---------------------------------------------------------------------------------------

## 4. Reference (TypeScript) status

- Jail: `game/src/core/world/blackthorn.ts` `guardArrestJail` (lines 741-758; consts 708-714;
  docblock 726-740) sets location 4, x=0x19, y=4, floor 0, keys 0, `shadowlordHere =
  computeShadowlordHere(state)`, then `if (hour !== 8) { hour = 8; minute = 0 }`. It never
  calls `advanceClock`. The docblock's "bucle advance_clock(0x14)" is cited but not run,
  and says "el clon fija minute=0 como observable canonico (dia no modelado, mismo criterio)
  as in partyRefuge". Called from `guard-encounters.ts` `resolveGuardArrest` line 494
  (docblock 463-484, the #328 note, correctly derives that the loop contains no fill_rect
  and calls K4F7C). `GuardCtx` (guard-encounters.ts 62-100) has `rand` but no sky context.
  The `Game` wrapper is `game.ts:5649`.
- `advanceClock` (`world/survival.ts` 298-398) is faithful to my 2.2 reading: Q halving
  (4F8D-4F9D), prevHour snapshot, T return, torch/light sat-sub, single carry,
  `relocateShadowlordsAtMidnight` (204-218; matches 4FF5-5048 including the `[0x5893]`
  exclusion and the self-slot check), day/month/year, month-end zeros, monthsAtInn, moon latch.
  Not modelled anywhere: `[0x588c]` (hourly countdown, camp cooldown; `camp.ts` 376-378,
  `commands.ts` 868-894 declare it unmodelled).
- Inn: `shops/shops.ts` `innNightPass` (461-484) = 12 x `advanceClock(5)`, loop `while
  (hour !== 6)`: `ringRegenSweep`, `advanceClock(9)`, `onHourTiles()` at hour 20/5; cap 1000
  steps (Time Stop). Faithful. Driver `ui/shop-console.ts` `innRestYes` (2338-2385) calls
  `innRest` (heal / P->D / S->G, `shops.ts` 383-413) FIRST, then `innSleepUntilMorning`
  (`game.ts` 4844), then `wakeSnapNpcs` (4893), then the "has passed away" messages. No
  housekeeping call. Divergence: heal before the night (see 5/6).
- Bed sleep (adjacent): `world/camp.ts` `bedSleepStep` (311-326) = `advanceClock(10)` +
  `snapNpcsToSchedule` + gate; **no `turnHousekeeping`** (binary CMDS 0x0671 has it) and no
  hour-20/5 tile call (CMDS 0x0664) -- see section 8.

## 5. Native status

- Jail: `native/core/src/blackthorn.cpp:61` (`blackthorn_action`, `BlackthornAction::Arrest`,
  agree): sets message events, `g.position={{25,4},{4,0}}`, `g.keys=0`,
  `c.travel.shadowlord_here=-1`, `if(g.time.hour!=8){g.time.hour=8;g.time.minute=0;}`, emits
  MapChanged/PartyChanged. No `advance_clock`. (Compare line 103 of the same file, which does
  call `advance_clock(g,c.turn,2,&rand,c.sky)` for the capture scene -- the machinery is in
  reach: `c.turn`, `c.sky`, `rand` are all in scope.) Trigger path: `ui_session.cpp:657`
  (GuardArrest YesNo -> `BlackthornAction::Arrest`), `commands.cpp:631`.
- `advance_clock` (`turn.cpp:25-67`) mirrors the TS function (Q, T, torch `g.torch_turns`,
  `s.light_spell_minutes`, single carry, Shadowlord re-roll guarded by
  `rand && s.has_shadowlords`, `refresh_moon_phase_latch`). Same gap for `[0x588c]`.
- Housekeeping `turn.cpp` `housekeeping_into` (66-96) is a faithful port of 2.3 (poison via
  `rules_poison_due`, food/starve on `hour != prev_hour`, `++turns_since_start`, spell turns,
  ring regen). Not called by the jail or the inn.
- Inn: `shops.cpp` `inn_night_pass` (329-344) faithful (12x5, ring sweep, 9, `tiles()` at
  20/5, cap 1000); driver `shop_orchestration.cpp` 494-509 calls `inn_rest` (230-265: heal,
  P->D unless `poison_harmless`, S->G) BEFORE `inn_night_pass`, then `wake_npcs`, then
  `++position.xy.x`. Same ordering divergence as TS.
- Bed sleep (adjacent): `rest.cpp` `bed_sleep_step` (323-348): `advance_clock(10)`, tile
  refresh at 5/20 on hour change, **`turn_housekeeping`** (matches CMDS 0x0671), status
  refresh, `snap_npcs`, occupied gate -- matches the binary. `camp_sleep_step` 209-240 uses
  `advance_clock(5)`.

## 6. Fixtures, corpora and tests that pin it, and what a fix changes

No stored oracle fixture pins the jail. The pins are:

| pin | what it asserts | effect of the proposed fix |
|---|---|---|
| `quest_parity` ctest -> `native/core/tools/check-quests.ts` rows at lines 200-205 (loc {1,5} x gold {0,9,10,101} x pay x quietly = 32 `world-flow` rows; start `time` = {139,4,5,hour 10,min 0}, no `shadowlordLocs`), live TS-vs-native comparison of `projection(s)` (fields include `time`, `position`, `keys`, `torches` count) | TS `resolveGuardArrest` and native `blackthorn_action` agree today because both set hour 8 min 0 day 5 | Rows where the jail executes (quietly=true AND the prompt was reached: pay=false for 4 golds x 2 locs = 8 rows, plus pay=true with gold < 10 for golds 0 and 9 x 2 locs = 4 rows; 12 rows): `time` becomes {139,4,**6**,8,0} (66 iterations, one midnight). Nothing else in the projection changes (burning torch minutes and Shadowlord locations are not projected; no `shadowlordLocs` in these rows so no RNG draws). TS and native MUST change in the same commit or `quest_parity` goes red. The corpus is generated live, so "regenerate" = nothing to regenerate; per the project rule fix the reference first, then native, token-diff the row set (only `time.day` of those 12 rows differs). |
| `game/tests/guard-arrest-live.test.ts` 258-277 | hour 8 after 'Y' (start 12:35, day 7) | still green (hour 8); strengthen: minute 15, day 8 |
| `game/tests/npc-hostil-ataca.test.ts` 264-271 | no combat after 'Y' | unaffected |
| `game/tests/shadowlord-physical-flag.test.ts` 82-91 | calls `guardArrestJail(s)` with 1 arg at 21:00 | signature must stay callable with one arg (optional `rand`, `sky`) |
| `native/core/tests/batch4_group_a_test.cpp` A3 (194-219) | location 4, (25,4), keys 0, hour 8, `arrest` cleared | still green; add day/minute asserts |
| `game/src/core/__parity__/*` | no arrest/jail content | none |
| `native/core/fixtures/*.json|txt` | no arrest content (only `advanced-combat-coverage.json` mentions the word in a combat context) | none |
| `re/notes/kernel-survival.md` 293-295; `ALPHA4_UI.md` 2961 and the "also 11.15" row of 14.5 (3342) | the wrong claim / "a byte scan shows none" | correct the text (see 7) |

Inn: pinned by `shop_parity`/`shop_flow` and `liston-c-207-t2` tests; none depends on the
heal-before-night ordering except an RNG count when a poisoned ring wearer exists (no known
corpus row). If the ordering is fixed, such a row (poisoned + ring 0x2C + inn rest) would
add one `rand(0,7)` per night iteration for that member.

---------------------------------------------------------------------------------------

## 7. Proposed fix and boundary test cases

### 7.1 Documentation (do now, zero risk)
1. `re/notes/kernel-survival.md` 293-295: replace with: "Carcel: TOWN 0x1324 `advance_clock(20)`
   until `g_hour==8` (loop test at the head; no housekeeping: K2AE8 is called only from TOWN
   0x10D0, MAINOUT 0x0CD3, DUNGEON 0x0E22, CMDS 0x0671). Posada: SHOPPES3 0x01B5-0x01F4,
   12 x advance_clock(5) then ring regen + advance_clock(9) until hour 6, no housekeeping.
   Meals/hunger/poison do NOT run in either." Keep the pointers to `blackthorn-captura-coste.md`
   (already right) and `aud-relojes-acta.md` section 1.
2. `ALPHA4_UI.md` 2961 / 3342: the note is wrong; the byte scan was right (but see the base
   correction in 2.1); and record the new real divergence (propose a ledger id after D-89,
   e.g. D-90 "jail wait does not run the clock").
3. `re/tools/callers_banda.py` BASES: replace by the measured table of 2.1 (and keep a
   positive control per file).

### 7.2 Production fix for the jail clock (reference first)
TS (`blackthorn.ts` + `guard-encounters.ts` + `game.ts guardCtx`):
```
guardArrestJail(state, rand?, sky?: () => SkyRefreshCtx | undefined):
  position.location = 4; x = 0x19; y = 4;            // 0x130e-0x1318 FIRST (relocate excludes [0x5893]==4)
  for (n = 0; state.time.hour !== 8 && n < JAIL_MAX_STEPS; n++)
      advanceClock(state, 20, rand, sky?.());        // 0x1324-0x132b; Q halving is inside advanceClock
  position.floor = 0; keys = 0;                      // 0x1332-0x1337 AFTER the loop
  shadowlordHere = computeShadowlordHere(state);     // the reload (0x12cd -> 0x11f0) comes after
```
`JAIL_MAX_STEPS`: the original has none; Time Stop (`T`) makes it loop forever, so cap
exactly like `INN_NIGHT_MAX_STEPS` (declare the divergence; recommend: if `timeSpell==='T'`
break immediately rather than spin 1000 times). Add `sky?: SkyRefreshCtx` to `GuardCtx`
(supplied as a getter so `effectiveLocation` is read AFTER `position.location = 4`) and pass
`ctx.rand`. Do NOT call `turnHousekeeping` (the binary does not).
Native (`blackthorn.cpp:61`): after `g.position={{25,4},{4,0}}`-style assignment of
location/x/y (keep floor assignment for after the loop), run
`for(n=0; g.time.hour!=8 && n<cap; ++n) advance_clock(g,c.turn,20,&rand,sky_with_location_4);`
then `floor=0`, `keys=0`, `shadowlord_here=-1` as today. `c.sky` carries a stale `location`
(`SkyRefresh.location`), build a copy with location 4 (harmless in practice: both < 0x21).
Then run `quest_parity` (12 rows move by day+1), keep unit tests green, extend them.

### 7.3 Inn ordering (optional, low value)
Move the heal/P->D/S->G pass to after `inn_night_pass`/`innNightPass` (binary 0x0200-0x0282
after the "Morning!" print and the 0x1694 NPC placement); keep the "has passed away"
messages where they are. Only visible as RNG draws for a poisoned ring wearer.

### 7.4 Boundary tests (derived from the loop, 2.4 table)
Jail, TS and native, with a Rand that records draws and with `torchTurns=2000`,
`lightSpellMins=100`:
- 10:00 day 5 -> 08:00 day 6, 66 `advance_clock(20)` calls, torch 2000-1320=680, light 0.
- 12:35 day 7 -> 08:15 day 8, 59 iterations.
- 23:50 -> 08:10 next day (25 it); 23:59 -> 08:19.
- 00:00 -> 08:00 SAME day (24 it, zero midnights, no re-roll); 03:05 -> 08:05 same day.
- 07:40 -> 08:00 (1 it); 07:59 -> 08:19 (1 it).
- 08:00 and 08:30 -> unchanged, zero iterations, no RNG, no torch change.
- Quickness: 10:00 with `timeSpell='Q'` -> 132 iterations of 10 min, wake 08:00 next day.
- Day 28 at 21:00: month rolls (day 1, month+1, `monthsAtInn`+1 up to 25, reagent/skull
  day stamps zeroed); month 13 day 28 -> year+1.
- Shadowlords `[2,5,255]`, scripted Rand returning 4 first: the draw 4 is REJECTED (location
  is Yew=4 during the wait); returning a value equal to another slot is rejected; slot with
  255/>=0x80 consumes no draw. With position set after the loop the test would see the
  arrest town excluded instead -- this test pins the 0x130e-before-0x1324 order.
- Floor during the loop is the old floor (moon latch skipped when floor >= 0x80 at arrest),
  0 after; keys 0; gold untouched; `shadowlordHere` = -1 afterwards; no `turnHousekeeping`
  effects: with food 0 at an hour flip and a poisoned member in the party, `food`, HP and the
  message list are unchanged by the jail.
- Time Stop: no hang (cap or immediate break); document as the only intentional divergence.
Inn: night from 22:00 -> 06:0x with prelude 12x5; resting at 05:30 (prelude lands in hour 6,
no 9-min step, minute stays); resting at 06:30 (loop runs ~23 h); poisoned ring wearer RNG
count if 7.3 is applied; food/HP unchanged by the wait for a starving party (no "Starving!").

---------------------------------------------------------------------------------------

## 8. Unresolved / out of scope

- Nothing was executed under DOS; the Time Stop hang, the loop counts and the stale-latch
  analysis are static derivations from the instructions above.
- Location byte 4 = Yew and the "keys" meaning of `[0x57ac]` come from the ports' naming plus
  consumers I checked (ZSTATS prints it after "Keys.......", SJOG decrements it); the town
  name itself is not in the binary text I read.
- TOWN 0x12AE's palace branch (`[0x5893]==0x12`: K39fc, K0E72, 0x11F0(1)) was read only far
  enough to see it has no clock loop.
- Purposes of BLCKTHRN 0x05B4, CAST2 0x10F1, OUTSUBS 0x0993, EXE 0x3CFC/0x6356 (K4F7C
  callers) were not analysed beyond their operand; none is the jail or the inn.
- `[0x588c]` (hourly countdown, camp cooldown 0x0E set at CMDS 0x0505) is decremented by
  K4F7C but modelled by neither port anywhere; the jail wait would decrement it by the
  hours crossed. Pre-existing, separate gap.
- Adjacent, for the lead (not fixed by this item): (a) the TS reference's `bedSleepStep`
  omits `turnHousekeeping` and the hour-20/5 tile call, native `bed_sleep_step` has the
  housekeeping (matches CMDS 0x0671) -- TS/native disagree and the `kernel-survival` claim
  "meals/hunger during the wait" is TRUE for the bed, false for jail/inn; (b) the Refuge wake
  (BLCKTHRN 0x0C2A: `advance_clock(9)` until hour 6, first `[0x588e]=[0x587a]=0`; then
  `[0x58a6]=[0x58a7]=0`, food floor 0x3F) is a direct `hour=6; minute=0` in TS
  `partyRefuge` (blackthorn.ts 528-563) -- same defect class as the jail (day, Shadowlord
  re-roll and its RNG draws skipped); (c) the inn's wake-order edge in 7.3.
