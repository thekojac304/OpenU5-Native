# ITEM JAIL -- FINAL reconciled report (A4-PARITY2, ledger 11.15 "also" row)

Reconciler: independent re-derivation from the binaries in this session. Reports reconciled: JAIL-A.md, JAIL-B.md.
Scratch scripts (all read-only against the repo) live beside this file: `jf_census.py` (overlay-base fit),
`jf_census2.py` (by-band census, E8+E9, any file), `jf_anybase.py`, `jf_reach3.py` (bounded kernel call-graph
closure), `jf_townreach.py` (closure from TOWN 0x12AE over TOWN + kernel + thunks), `jf_sim.py` (byte-faithful loop
simulator), `jq/check-quests-copy.ts` + `jq/guardrows.json` (a COPY of check-quests.ts, run with the repo untouched,
that dumps what the TS reference does in the 40 `guard:true` quest rows). Nothing inside the repository was edited,
no build or ctest was run (`git status` shows only the four pre-existing untracked baseline logs).

Notation: `K<hhhh>` = resident kernel address = ULTIMA.EXE image offset after the MZ header (`--exe`, = CS:IP of the
notes). Overlay addresses are FILE offsets. Stack args are pushed left to right, callee reads `[bp+4]` = last pushed,
all calls near, callee pops (`ret n`). DATA.OVL file offset = DS offset + 0x10.

---------------------------------------------------------------------------------------------------------------

## 1. Verdict

1. **The note is wrong.** `re/notes/kernel-survival.md` lines 293-295 ("Carcel/posada TOWN:0x1324: bucles de
   `advance_clock(20)` hasta la hora objetivo ... con comidas/hambre aplicandose en cada cambio de hora") is false on
   every count that matters:
   - The jail wait loop (TOWN 0x1324-0x1330) calls exactly one routine per iteration: `advance_clock(0x14)` (K4F7C).
   - The inn night loop is not at TOWN 0x1324. It is SHOPPES3.OVL 0x01B5-0x01F4 (12 x `advance_clock(5)`, then
     `beep_ticks(1)`, ring-regen sweep, status redraw, `advance_clock(9)`, tile transform at hours 20/5, until
     `g_hour == 6`). It never calls the housekeeping either.
   - The per-turn housekeeping (meal at hours 6/12/18, "Starving!", poison tick, turn counter, time-spell countdown,
     ring regen) is kernel K2AE8, and it has exactly four call sites in the whole install (TOWN 0x10D0, MAINOUT 0x0CD3,
     DUNGEON 0x0E22, CMDS 0x0671). Neither wait reaches it, directly or through any callee (closure test in 2.6).
   - So meals, hunger, starvation damage, poison damage and the turn counter do NOT run during the jail wait or the
     inn wait. "ALPHA4_UI.md: a byte scan shows no housekeeping there" is correct.
   - The note's idea is TRUE for one thing it does not name: the bed hole-up ((H)ole up on a bed tile, CMDS 0x0552)
     runs K2AE8 every 10-minute step (CMDS 0x0671). That is probably where the claim came from.
2. **Housekeeping question: both ports already match the binary** (neither calls housekeeping in the jail or the inn
   wait). On the exact claim of the note, only the note (documentation) is wrong.
3. **But production DOES diverge in the jail wait, on a different axis: the clock.** The original runs
   `while (g_hour != 8) advance_clock(20)` (test first). Both ports skip the loop and assign `hour=8; minute=0`
   (TS `guardArrestJail`, native `blackthorn_action` Arrest/agree). Observable consequences, all derived below:
   wake minute is `start_minute mod 20` (mod 10 under Quickness), not 0; the calendar day advances when the party was
   arrested after 08:xx (and does not when arrested 00:00-07:59); midnight Shadowlord re-roll and its RNG draws are
   skipped; torch and light-spell minutes are not burned; month/year rollover, month-end zeroing and `monthsAtInn`
   ageing are skipped; `prevHour` is not left at 7.
4. **Inn: loop shape, steps and exit are faithful in both ports.** One ordering defect (both ports, same): the
   wake-up refill + poison death (SHOPPES3 0x0200-0x0282) runs BEFORE the night in the ports and AFTER it in the
   binary. Visible only as RNG: a poisoned Ring-of-Regeneration bearer draws `rand(0,7)` once per night iteration in
   the original, zero in the ports. Plus the already-declared beep/redraw wind rolls (see 2.5; JAIL-A's "no RNG"
   for `beep_ticks` was wrong, JAIL-B right).
5. Which artefact is wrong: the note (documentation) -- yes; the TS reference -- yes, jail clock (and, adjacent, bed
   hole-up housekeeping, ledger D-20 residual); native -- yes, jail clock only (native bed is right); the shortcut was
   copied native <- TS ("exact reference guardArrestJail mutation", batch4_group_a_test.cpp A3 comment).
6. Tooling warning (confirmed independently): the overlay base table in the brief and `re/tools/callers_banda.py`
   is wrong for 11 of 22 overlays (2.1). `callers_banda.py` therefore under-reports K2AE8 (misses DUNGEON 0x0E22) and
   K4F7C (misses SHOPPES3 and others). It does NOT "never miss".

Confidence: high on the call-graph facts (positive controls, two independent census implementations, closure
tests); high on the loop arithmetic (byte-faithful simulation, three implementations agree); medium on real-DOS
behaviour (static derivation only, nothing was executed).

---------------------------------------------------------------------------------------------------------------

## 2. Binary facts (all disassembled in this session)

### 2.1 Overlay near-call bases (measured; the brief's table is wrong for these)

Method (`jf_census.py`): for each overlay, take every `E8 rel16` whose raw target falls outside the overlay image,
and score every candidate base 0x8000..0xFFFF (step 0x10) by how many resolved targets land on `55 8B EC` in the
resident image. Winner / runner-up hits: TOWN 0x81D0 144/158 (43), MAINOUT 0x81D0 182/204, DUNGEON 0x81D0 166/190,
INTRO 0x81C0 546/562; FLAMES, NPC, COMBAT, BLCKTHRN, LOOKOBJ, DNGLOOK, OUTSUBS, SHOPPES, ENDGAME 0xA290
(e.g. COMBAT 115/164, BLCKTHRN 129/136); SJOG, CMDS, CAST, TALK 0xBF80 (CMDS 210/252); CAST2, ZSTATS, COMSUBS,
SHOPPES2, SHOPPES3, FONT 0xE1E0 (SHOPPES3 85/111, runner-up 0xF960 39). The brief says SHOPPES3 0xA5F6, COMSUBS
0x85FE, COMBAT 0xBFEC, DUNGEON 0xE1E0, BLCKTHRN 0xE63E, LOOKOBJ 0xA444, DNGLOOK 0xA2B6, CAST 0xA8D8, INTRO 0xCD3A,
SHOPPES2 0xA89E: all fail the landing test. Direct controls: SHOPPES3 `call 0x6d9c` + 0xE1E0 = K4F7C (`55 8b ec`),
+ 0xA5F6 = K1392 (`a1 c4 53`, not a prologue); SHOPPES3 `call 0x3670` + 0xE1E0 = K1850 (print) vs + 0xA5F6 = 0xDC66
(past the end of the resident image); TOWN `call 0xcdac` + 0x81D0 = K4F7C. Thunk cross-check
(`re/tools/thunks.py`): ovl 1 (TOWN) thunks target 0x81D0 + offset (e.g. thunk 0x7A46 -> 0x93C0 = TOWN 0x11F0).

TOWN is the only overlay this item's jail needs (0x81D0, brief correct). The inn needs SHOPPES3 = 0xE1E0 (brief WRONG).

### 2.2 The jail: TOWN.OVL 0x12AE (base 0x81D0)

Entry: exactly one caller. `TOWN 0x13D6 call 0x12AE` inside npc_engine TOWN 0x1352, whose only caller is
`TOWN 0x1683 call 0x1352` in the town main loop 0x141E (`jf_census2`/raw scan: callers of 0x12AE = ['13d6'],
of 0x1352 = ['1683']; no thunk targets 0x947E/0x9522 and no raw word 0x947E/0x9522 exists anywhere in the install).

```
12ae push bp / mov bp,sp / sub sp,4 / mov word [bp-4],0
12b9 cmp byte [0x5893],0x12 ; jne 12d8          ; palace (loc 0x12): K39fc, K0e72, 11f0(1), no prompt, no wait
12d8 push 0x27e2 ; call K1850                    ; "\n\"Thou art under arrest!\"\n\n"
12df push 0x27fe ; call K1850                    ; "\"Wilt thou come quietly?\"\n\n:"
12e6 call K266c (getkey) ; mov [bp-2],al ; cmp al,0x4e 'N' ; je 12f4 ; cmp al,0x59 'Y' ; jne 12e6
12f4 cmp byte [bp-2],0x59 ; jne 133c
12fa push 0x281b ; call K1850                    ; "Yes\n\nThe guard strikes thee unconscious!\n"
1301 sub ax,ax ; push ax ; call 0x88a0 (K0a70)   ; set_color(0) -- no fill_rect
1307 push 0x2845 ; call K1850                    ; "\nThou dost awaken to...\n"
130e mov byte [0x5893],4                         ; g_location = 4 (Yew)      BEFORE the wait loop
1313 mov byte [0x5896],0x19                      ; x = 25
1318 mov byte [0x5897],4                         ; y = 4
131d mov byte [0x24e6],1                         ; redraw mark
1322 jmp 132b                                    ; ENTER AT THE TEST
1324 mov ax,0x14 ; push ax ; call 0xffffcdac     ; (+0x81D0 = K4F7C) advance_clock(20), WORD operand
132b cmp byte [0x587f],8 ; jne 1324              ; while (g_hour != 8)   unsigned byte equality
1332 sub al,al ; mov [0x57ac],al                 ; keys = 0                  AFTER the loop
1337 mov [0x5895],al                             ; floor = 0                 AFTER the loop
133a jmp 12cd -> 12cd mov ax,1 ; push ax ; call 11f0 ; (return 0 via 134b)   ; town reload, NPC reposition (arg 1)
133c push 0x285e ; call K1850 ; "No\n\n\"Then defend thyself, rogue!\"\n" ; call 958 (alarm) ; [bp-4]=1 ; ret 1
```
Strings read from DATA.OVL (DS+0x10): 0x27e2, 0x27fe, 0x281b, 0x2845, 0x285e exactly as above.
Property list a port must reproduce:
- Test FIRST (1322 `jmp 132b`): if the hour is already 8 there is no call at all (minute, prev_hour, torch, day
  untouched).
- The loop body is the single call `advance_clock(0x14)`. No other call between 0x1324 and 0x1330.
- During the loop g_location is already 4 and (x,y) already (25,4); g_floor and g_keys still hold their ARRESTED
  values until 0x1332-0x1337.
- No cap. Time Stop (`[0x587A]==0x54`) would make `advance_clock` skip the minute add (4FA6/4FAB) and the loop would
  never end -- but this is UNREACHABLE: 0x1683 (the only way to 0x1352) is reached only through TOWN 0x1642
  `cmp byte [0x587a],0x54 / je 0x1686` (jumps into 0x165F/0x1662/0x1671/0x167E come only from 0x164E, 0x1666, 0x1676,
  all downstream of that test; scan of every `j*` in 0x1400-0x1700). A Time-Stop party cannot be arrested in the
  original. JAIL-B right, JAIL-A's "hangs the original" is true of the loop in isolation only.
- Quickness (`'Q'` 0x51): the NPC pass runs on alternate turns (0x1649-0x165D), but once reached, `advance_clock`
  halves the step (20 -> 10) on every call.

### 2.3 The clock routine K4F7C (`advance_clock(n)`, `ret 2`) -- re-read line by line

```
4f84 cmp word [bp+4],0 ; jne 4f8d ; jmp 50a1       ; n==0: tail only (no prev_hour write)
4f8d cmp byte [0x587a],0x51 ; jne 4fa0 ; sar word [bp+4],1 ; cmp ...,0 ; jne 4fa0 ; inc word [bp+4]   ; 'Q': n>>=1, min 1
4fa0 mov al,[0x587f] ; mov [0x5880],al             ; g_prev_hour = g_hour   (EVERY n>0 call, before the 'T' test)
4fa6 cmp byte [0x587a],0x54 ; je 4fc8              ; 'T': skip the minute add AND both saturating subs
4fad mov al,[bp+4] ; add [0x5881],al               ; minute += (n & 0xff)    (byte add)
4fb4 push 0x58a7 ; push n ; call 3f36              ; [0x58a7] sat-sub n   (torch minutes)
4fbe push 0x58a6 ; push n ; call 3f36              ; [0x58a6] sat-sub n   (light-spell minutes)
4fc8 cmp byte [0x5881],0x3b ; ja 4fd2 ; jmp 50a1   ; ONE carry only
4fd2 sub byte [0x5881],0x3c
4fd7 push 0x588c ; push 1 ; call 3f36              ; [0x588c] -= 1 (u8 saturating) per hour crossed
4fe2 inc byte [0x587f] ; cmp byte [0x587f],0x17 ; ja 4ff0 ; jmp 50a1
4ff0 mov byte [0x587f],0                           ; midnight
4ff5 [bp-4]=0 ; loop while [bp-4] < 3 (504b):
4ffd   cmp byte [bx+0x58c8],0x80 ; jae 5048        ; absent/destroyed slot: no draw
5004   push 1 ; push 8 ; call 2092 ; mov di,ax     ; di = rand(1,8)   (K2092(lo,hi) inclusive)
5011   if byte [0x5893] == di: di = 0               ; excludes the party's CURRENT g_location
501c   for si=0..2: if byte [si+0x58c8] == cx(di): cx=0   ; ALL THREE slots incl. the slot being moved (old value)
5037   di=cx ; or di,di ; je 5004                   ; redraw until nonzero
5044   [bx+0x58c8] = di
5051 inc byte [0x587e] ; cmp byte [0x587e],0x1c ; jbe 509e   ; day++ ; >28 -> month block
505c [0x585a]=[0x5859]=[0x5858]=[0x57b2]=0 ; [0x587e]=1 ; [0x5959]=0
5072 for si=0x55bf; si<0x57bf; si+=0x20: if byte [si] < 0x19: inc byte [si]     ; 16 records, monthsAtInn cap 25
508a inc byte [0x587d] ; cmp ...,0xd ; jbe 509e ; [0x587d]=1 ; inc word [0x5874]  ; month 1..13, year++
509e call 2900                                       ; status redraw
50a1.. light recompute from [0x587f]/[0x5881]/[0x5893]/[0x5895]; floors: [0x58a6]!=0 -> >=0x12, [0x58a7]!=0 -> >=0x0a
514a mov al,[0x5880] ; cmp [0x587f],al ; je 5186    ; hour changed in THIS call?
5153 cmp byte [0x5893],0x21 ; jae 5164 ; cmp byte [0x5895],0x80 ; jae 5164
5161 call 4a84                                       ; sky strip + moon-phase latch  (gated by LOCATION < 0x21 and FLOOR < 0x80)
5164 [0x5884] = 12h-clock hour ; 5186 if [0x5893] in 1..0x20: call 71aa (lcall [0x5350], presentation) ; 519c ret 2
```
`3f36` = u8 saturating subtract: `cmp ax,[bp+4] ; jbe -> byte=0 ; else sub byte,[bp+4]` (read at 0x3F36-0x3F50).
Byte identity (settles an A/B conflict): **[0x58A7] = torch minutes, [0x58A6] = light-spell minutes** (JAIL-B right,
JAIL-A swapped them). Evidence: CMDS 0x0DD6 `mov byte [0x58a7],0xf0`, SJOG 0x1A05 `mov byte [0x58a7],0x64`, CAST2
0x08F0 writes [0x58a6]; tail floors 0x0A for [0x58A7] (small light) and 0x12 for [0x58A6] (large light). No
behavioural consequence for the fix (both are decremented by the same n).
Closure test (`jf_reach3.py 4f7c`, 34 functions, controls below): the only `call 0x2092` (rand) reachable from K4F7C is
the one at 0x500C; K2AE8, K400C, K2F62 (wind), K5910 (viewport) are NOT reachable. So the loop's total RNG
consumption is the Shadowlord re-roll at midnight, nothing else.

### 2.4 What the per-turn housekeeping K2AE8 is, and its complete caller census

```
2ae8 push bp ; sub sp,8 ... ; eaters=0
2b0b..2b55 for i<[0x585b] (stride 0x20 from 0x55b3): 'D' && i==[0x587b] -> [0x587b]=0xff ; skip 'D','S' ;
           'P' -> push i ; push 1 ; call 2a52 (1 HP) ; eaters++
2b5d mov al,[0x5880] ; cmp [0x587f],al ; je 2b9f          ; ONLY on an hour change vs the g_prev_hour latch
2b66 cmp word [0x57a8],0 ; jne 2b7a ; push 0x54c8 ("Starving!\n") ; call 1850 ; call 2aa8 ; jmp 2b99
2b7a hour in {6,0xc,0x12}: push 0x57a8 ; push eaters ; call 3f54      ; food -= eaters (word, floor 0)
2b99 mov al,[0x587f] ; mov [0x5880],al                   ; consume the edge
2b9f turn counter [0x588b] +1 (cap 0xff) ; 2bae time-spell countdown [0x588e] -> [0x587a]=0 ; 2bca call 400c
```
K400C (ring regen): per member `i<[0x585b]`, status != 'D' (4032), equipment byte [0x55c5+0x20*i] == 0x2C (4037),
`push 0 ; push 7 ; call 2092 ; cmp ax,7` (one draw per living ring wearer, 4043); on 7 heal +1 via K3F14.

Callers of K2AE8 (my census `jf_census2.py 2ae8`, all files, measured bases, E8 and E9): TOWN 0x10D0, MAINOUT 0x0CD3,
DUNGEON 0x0E22, CMDS 0x0671. ULTIMA.EXE itself: none. Positive controls: (a) the same tool finds the 21 K4F7C sites
and the 79+ K1850 sites; (b) `jf_anybase.py` re-resolves every overlay near call under all five bases and finds the same
four and no more; (c) the three raw 0x2AE8 words in ULTIMA.EXE (0x1E8D, 0x4B9D, 0x6972) are mid-instruction (`e8 2a f8`,
`e8 2a d1`, `e8 2a c0`), not pointers; (d) closure from K2AE8 reaches K2A52, K2AA8, K3F54, K400C (so the closure code works).
The old tool finds 3 (misses DUNGEON 0x0E22). TOWN 0x10D0 sits at the end of TOWN 0x0F02, whose only caller is
`TOWN 0x15EC call 0x0F02` in the main loop.

Callers of K4F7C (21; same set in A and B and in `jf_census2.py 4f7c`): ULTIMA 0x3CFC (5) and 0x6356 (0); TOWN 0x0511 (0),
**0x1328 (0x14, the jail)**, 0x15D4 (1); MAINOUT 0x0006, 0x0066, 0x0462, 0x068C, 0x0A56, 0x0C3D; DUNGEON 0x0F2F; COMBAT 0x0C76;
BLCKTHRN 0x05B4, 0x0C2E (Refuge wake, 9 until hour==6); OUTSUBS 0x0993 (0); CMDS 0x0318 (5), 0x064B (10, bed); CAST2 0x10F1;
**SHOPPES3 0x01C1 (5 x12), 0x01DB (9) = the inn**. Callers of K400C: ULTIMA 0x2BCA (inside K2AE8) and 0x67F6, CMDS 0x0207
(camp), SHOPPES3 0x01D1 (inn).

### 2.5 The inn night (SHOPPES3.OVL 0x0072, base 0xE1E0), re-read 0x0150-0x02A3

```
0160 [0x5896],[0x5897] = room x,y from DS tables 0x4e7a/0x4e80 indexed by [0xb114]
0172 [0x24e6]=1 ; 0177 [bp-2]=[0x587b] ; 0182-01a5 for i<[0x585b]: status 'G'(0x47) -> 'S'(0x53)
01a8 call K2900 ; 01ab call K5910 ; 01ae push 0x4e44 ; call K1850                       ; "Zzzzzz....\n\n"
01b5 mov word [bp-8],0xc ; mov si,0xc
01bd push 5 ; call 0x6d9c (K4F7C) ; dec si ; je 01ef ; jmp 01bd                           ; 12 x advance_clock(5), NO hour test
01ca push 1 ; call 0x5906 (K3AE6 beep_ticks) ; call 0x5e2c (K400C) ; call 0x4720 (K2900) ;
01d7 push 9 ; call K4F7C ; cmp [0x587f],0x14 ; je 01ec ; cmp [0x587f],5 ; jne 01ef
01ec call 0xffff98ba (K7A9A -> TOWN 0x0170)                                               ; hour-tile transform, every iteration landing in hour 20 or 5
01ef cmp byte [0x587f],6 ; jne 01ca                                                       ; loop while hour != 6 (test at head, after the 12x5)
01f6 push 0x4e51 ; call K1850 ("Morning!\n") ; 01fd call 0xffff98ae (K7A8E -> TOWN 0x1694)  ; NPC snap
0200-0280 per member (stride 0x20 from 0x55a8, i<[0x585b]): skip 'D'; HP=maxHP; class A/M: MP=INT, B: MP=INT>>1;
          'P' -> 'D', HP=0, K16ba(0xa), print name, print DS 0x4e5b " has\npassed away.\n";  'S' -> 'G'
0285 [0x587b]=[bp-2] ; 028b inc byte [0x5896] ; [0x24e6]=1 ; call K2900 ; ret -1
```
Timing: 60 minutes of prelude, then 9-minute steps until the first landing in hour 6 (wake minute 0..8; if the prelude
already lands in hour 6 no 9-minute step runs and the minute can be anything; resting at 06:xx loops about 23 h).
No call to K2AE8 anywhere in SHOPPES3 (census) and K2AE8 is not in the closure of 0x0072.

`K3AE6` (beep_ticks, settles the A/B conflict): `cmp byte [0x58a4],0 ; je out ; cmp word [bp+4],0 ; jle out ;
si=n ; loop { call 5910 (viewport_redraw) ; push 1 ; call 20fa (delay) ; dec si ; jne }`. K5910 reaches the wind roll
K2F62 (`rand(0,63)` at 0x2F70, plus rand(0,4)/rand(0,255) on a 1/64 hit), the anim-script tick K4552 and K51A0/51B3
(`rand(0,3)`), when `[0x5891]!=0` (latch re-armed to 1 at the end of every pass, 0x5A1D per re/notes/wind-rand-decision.md).
`[0x58A4]` is written only by TOWN 0x121D (`mov byte [0x58a4],1`, inside the town reload 0x11F0) and MAINOUT 0x0011,
so inside a town it is 1. Therefore the inn night DOES consume wind/anim RNG once per iteration, plus once at 0x01AB.
JAIL-A's "1 animation frame, no RNG" is wrong; JAIL-B is right. This is the already declared Class 3 divergence
(re/notes/wind-rand-decision.md) and is NOT part of this item; the TS docblock "sin RNG" at shops.ts ~line 435 is
inaccurate. The jail loop has no such call.

### 2.6 Adversarial closure: nothing in the jail or the inn reaches the housekeeping

`jf_townreach.py 12ae` (closure over TOWN near calls + resident calls + TOWN thunks; includes the 'N' alarm 0x958 and the
reload 0x11F0): TOWN functions reached = 0x0, 0xb0, 0x170, 0x212, 0x2ae, 0x408, 0x85e, 0x8d4, 0x958, 0x10f2, 0x1156,
0x11b8, 0x11f0, 0x12ae, 0x1694, 0x1726. K4F7C and K5910 reached (controls); **K2AE8, K400C, TOWN 0xF02, 0x15D4/0x141E,
0x1352 NOT reached.** The reload tail (TOWN 0x04FA-0x0514) runs `K5e4a ; push 0 ; call K4F7C ; call 0x212` -- unconditional
`advance_clock(0)`, i.e. only the light/sky tail.

Stale-edge analysis (why not even the final 7 -> 8 flank ever reaches the meal code): after the jail loop `[0x5880]==7`
and `[0x587f]==8`. In the town main loop the order per turn is `0x15D4 advance_clock(1)` -> `0x15EC call 0x0F02`
(housekeeping at 0x10D0) -> ... -> `0x1683 call 0x1352` (arrest) -> `0x1686` -> back to 0x142C. The arrest is AFTER that
turn's housekeeping, and the next turn's housekeeping is always preceded by `0x15D4 advance_clock(1)` whose 4FA3 write
re-latches `[0x5880]=8`. (The only jumps into 0x15EC are from 0x15DF, after 0x15D4.) Hour 8 is not a meal hour in any case.
The 6:00 breakfast edge crossed during the wait is never seen. The wait also never prints "Starving!".

### 2.7 Adjacent: the bed hole-up DOES run housekeeping (so the note is half-true)
CMDS 0x0634-0x068D (base 0xBF80): `0634 push 1 ; call 20fa ; 063b cmp si,[0x587f] ; je 0692 ; 0647 push 0xa ; call K4F7C ;
0650-0664 if hour changed and (hour==0x14 or 5): call K7A9A ; 066e call K4A84 ; 0671 call 0x6b68 (= K2AE8) ;
0674 call K2900 ; 0677 call K7A8E ; 067a-068b find_object_at_xy ...`. Native `rest.cpp bed_sleep_step` (323-348) matches;
TS `camp.ts bedSleepStep` (311-326) does NOT (no K2AE8, no 0x7A9A tile call): ledger D-20's "TypeScript still omits both".
Out of this item's scope; recorded.

---------------------------------------------------------------------------------------------------------------

## 3. Reference (TypeScript) -- exact change needed

Current state (all line numbers as read in this session):
- `game/src/core/world/blackthorn.ts` `guardArrestJail(state)` lines 741-758 (constants 711-716, docblock 726-740): assigns
  location/x/y, **floor = 0 BEFORE** (line ~745), keys = 0, `shadowlordHere = computeShadowlordHere(state)`, then
  `if (hour !== 8) { hour = 8; minute = 0 }`. No `advanceClock`. The docblock says "el clon fija minute=0 como observable
  canonico (dia no modelado)" -- that is the divergence.
- `game/src/core/world/guard-encounters.ts`: `resolveGuardArrest` 485-504, call at 494 `guardArrestJail(ctx.state)`;
  `GuardCtx` 63-100 has `rand: RandFn` (line 66) and no sky context.
- `game/src/core/game.ts`: `guardCtx()` 5604-5632 (supplies `rand: this.rand`, no sky); wrapper `resolveGuardArrest` 5649;
  `skyRefreshCtx` getter 4832 (location = `effectiveLocation`, 4814, which reads the LIVE `state.position.location`).
- `world/survival.ts` `advanceClock` 298-398 is a faithful port of 2.3 (Q halving, prevHour write before the T test, torch
  and light-spell clamp, single carry, midnight relocation with the all-three-slots rule 204-218, month block, `monthsAtInn`,
  moon latch). Not modelled anywhere: `[0x588C]` (see 7).

Change (reference first, then native, then token-diff the quest_parity row set):
```
guardArrestJail(state, rand?: RandFn, sky?: () => SkyRefreshCtx | undefined): void
  state.position.location = LOC_YEW; x = 0x19; y = 0x04;       // 0x130e-0x1318  FIRST. Do NOT touch floor or keys here.
  for (let n = 0; state.time.hour !== 8 && n < JAIL_MAX_STEPS; n++)   // 0x1322 jmp 132b / 0x1324-0x1330: test FIRST
      advanceClock(state, 20, rand, sky?.());                  // WORD 20; Quickness halving is INSIDE advanceClock
  state.keys = 0; state.position.floor = 0;                    // 0x1332-0x1337  AFTER the loop (floor moves from before to after)
  state.shadowlordHere = computeShadowlordHere(state);         // unchanged value (y == 4 -> -1 always); keep after the loop
```
- Remove the `if (hour !== JAIL_WAKE_HOUR) {...}` shortcut (keep `JAIL_WAKE_HOUR = 8` as the loop condition).
- `JAIL_MAX_STEPS`: the binary has none and the Time-Stop hang is unreachable (2.2). Add a defensive cap anyway, same value
  and declaration style as `INN_NIGHT_MAX_STEPS = 1000` (shops.ts 422); worst real case is Quickness: 144 calls.
- Signature MUST stay callable as `guardArrestJail(state)`: `game/tests/shadowlord-physical-flag.test.ts:85` calls it with one
  argument at 21:00. With `rand` absent `advanceClock` skips the relocation (survival.ts ~348), the calendar still advances.
- `GuardCtx`: add `sky?: () => SkyRefreshCtx | undefined`; `Game.guardCtx()` passes `sky: () => this.skyRefreshCtx` (a thunk, not a
  pre-built object, so `effectiveLocation` is read AFTER the position write; a pre-built object would carry the arrest town and
  open the moon-latch gate wrongly). `resolveGuardArrest` passes `ctx.rand` and `ctx.sky`. Do NOT call `turnHousekeeping` /
  `advanceTurn`; do not add redraws, delay, wind roll or beep (the binary loop has none).
- Optional completeness (low value, exact reason): the reload's `0x0511 advance_clock(0)` tail re-latches the moon phases when
  `prevHour != hour` (prev = 7 after the loop) with floor 0 and location 4. If the party was arrested with floor >= 0x80 the
  in-loop latches (old floor) were skipped; after the loop add `advanceClock(state, 0, undefined, sky?.())` when
  `state.prevHour !== state.time.hour` to reproduce the latch. Invisible in every quest_parity row (no moon table).
- Leave `state.prevHour` at whatever the last call wrote (7); do not "fix" it (the next town turn re-latches, 2.6).

---------------------------------------------------------------------------------------------------------------

## 4. Native -- exact change needed

Current state:
- `native/core/src/blackthorn.cpp:61` (`blackthorn_action`, `BlackthornAction::Arrest`, agree branch, one source line):
  `g.position={{25,4},{4,0}};g.keys=0;c.travel.shadowlord_here=-1;if(g.time.hour!=8){g.time.hour=8;g.time.minute=0;}` then
  MapChanged/PartyChanged events. Location, x, y AND floor (0) are all assigned before any wait, so the "old floor during the
  loop" property is also lost.
- `blackthorn_action` already has `Rand rand` (parameter) and `c.turn`, `c.sky` in scope (same file, line ~103 calls
  `advance_clock(g,c.turn,2,&rand,c.sky)` for the capture scene). Trigger: `commands.cpp:631` (`rand=rng_source(c.game.rng)`),
  `ui_session.cpp:657-660`.
- `native/core/src/turn.cpp` `advance_clock` 25-67 is faithful (`rand && s.has_shadowlords` gates the re-roll; slot rule
  `loc>=128||loc<0` skip, `next==g.position.map.location` or `find(all three slots)` redraw; one carry; `g.torch_turns`,
  `s.light_spell_minutes`; `refresh_moon_phase_latch` 18-24 gates on `sky.location>=0x21` and `(floor&255)>=128`).
- `c.sky` is assigned in production nowhere (the T-Deck passes null); in tests only `quest_driver.cpp:51`, built once from the
  INITIAL position (`SkyRefresh sky{...,g.position.map.location}`) and only when `data.moonPhases` is non-empty.

Change (in the agree branch, after the two Message events):
```
const auto old_floor = g.position.map.floor;
g.position.map.location = 4; g.position.xy = {25,4};            // 0x130e-0x1318; floor NOT yet changed
SkyRefresh sky{}; if (c.sky) { sky = *c.sky; sky.location = 4; }   // local copy: the moon-latch gate reads location 4
for (int n = 0; g.time.hour != 8 && n < 1000; ++n)
    advance_clock(g, c.turn, 20, &rand, c.sky ? &sky : nullptr);   // 0x1324-0x1330; Quickness halved inside advance_clock
g.keys = 0; g.position.map.floor = 0;                              // 0x1332-0x1337 AFTER the loop
c.travel.shadowlord_here = -1;                                     // unchanged (y==4)
```
(`old_floor` is only documentation: do not set the floor before the loop; the gate in `refresh_moon_phase_latch` reads
`g.position.map.floor`.) The `Rand` is the live stream (`rng_source(c.game.rng)`); `s.has_shadowlords` stays whatever the state
says. Keep the cap identical to the TS one. Do not call `turn_housekeeping`/`advance_turn`. Optional tail latch as in section 3.

---------------------------------------------------------------------------------------------------------------

## 5. Parity fixtures and corpora that pin it

**Jail -- quest_parity IS the pin (JAIL-B said nothing pins the jail; JAIL-A was right and I confirmed it by running the TS side).**
`ctest quest_parity` = `node --import tsx native/core/tools/check-quests.ts <quest_driver>` is a LIVE TS-versus-native comparison
(no stored oracle): rows built by `add('world-flow', ...)` are executed by the TS `Game` and by `quest_driver.exe` and compared with
`isDeepStrictEqual` on the whole step record including `projection(state)` whose `fields` list (line 35) contains `time`
(year, month, day, hour, minute), `position`, `keys`, `torches`, `turnsSinceStart`, `gold`, `food`, `characters`, plus `seed:
g.liveSeed()`. The jail rows are lines 200-205 of check-quests.ts: location {1,5} x gold {0,9,10,101} x pay x quietly = 32 rows,
start `time` {139,4,5, hour 10, minute 0} (from `initial()`/`s.time.hour=10`), no `shadowlordLocs`.
I executed a COPY of the generator (`jq/check-quests-copy.ts`, QUEST_FOCUS=guard, native step removed): of the 32 rows the jail
actually executes in exactly **10**, not 12: pay=false in both towns (4 gold values x 2 = 8: Minoc (loc 5) is the charity branch
where 'Y' always succeeds, 'N' goes to arrest) plus pay=true at loc 1 with gold 0 and 9 (2 rows; the tribute is 10 gp, and at loc 5
paying always succeeds). Today each ends `time {139,4,5,8,0}`. After the fix each ends `{139,4,**6**,8,0}` (10:00 -> 66 iterations,
one midnight). Nothing else in the projection changes in these rows (torch minutes and light-spell minutes are not in `fields`;
`torches` is a count; no `shadowlordLocs` so `relocateShadowlordsAtMidnight` returns early in TS and `has_shadowlords` is false in
native -> no RNG draws -> `liveSeed` unchanged; no moon table -> `sky` null on both sides). TS and native MUST change in the same
commit or quest_parity goes red; the corpus is generated live so there is nothing to regenerate. Token-diff proof: only `time.day`
of those 10 rows differs.

Other pins (all weak; none pins minute/RNG):
- `game/tests/guard-arrest-live.test.ts` 258-277: start `{139,4,7,12,35}`, asserts hour 8, location/x/y/floor/keys, gold -> stays green;
  strengthen with minute 15, day 8 (59 calls).
- `game/tests/shadowlord-physical-flag.test.ts` 82-91: `guardArrestJail(s)` one-argument call at 21:00 -> must stay callable;
  stays green (shadowlordHere -1, y==4).
- `game/tests/npc-hostil-ataca.test.ts` ~264-271 (no combat after 'Y'), `blackthorn-fiel.test.ts`, `npc-interpela-adyacencia-301`,
  `tienda-proximidad-304` (prompt only) -> unaffected.
- `native/core/tests/batch4_group_a_test.cpp` A3 (lines 194-219; `GuardWorld(2)` starts hour 10): asserts location 4, (25,4), keys 0,
  hour 8 -> still green (10:00 -> 08:00 next day, time_spell 0 so the loop ends); add day/minute asserts. A4 (No) unaffected.
  `native/targets/tdeck/host_tests/batch25_shard_ritual_test.cpp` U4 only checks the prompt.
- No jail content in `native/core/fixtures/*`, `native/core/build-*`, any `generate-*-fixtures.ts`, `game/src/core/__parity__`
  (grep for guardArrestJail/resolveGuardArrest/GuardArrestPrompt/"dost awaken" hits only the files above and e2e comments:
  `game/e2e/grandtour/ch08-yew.spec.ts:237-241` uses "not at (25,4)" as a NO-JAIL control).

Inn (only relevant if the optional ordering fix is taken -- not required for this item):
- `typescript_shop_fixture_drift` (generate-shop-fixtures.ts, op 10 `innRest`, op 15 `innNightPass`) and `typescript_shop_flow_drift`
  (generate-shop-flow-fixtures.ts, drives Inn rest/leave/pickup through `innSleepUntilMorning`). Their `initial(v)`
  (`native/core/tools/shop-test-state.ts` line 5) has `status:['G','P','S','D'][(i+v)%4]`, `ring: i%3 ? 255 : 44`,
  `timeSpell:['','Q','T'][v%3]`, so poisoned ring wearers exist in the corpus (e.g. v=1, member 0). Moving the refill/poison death
  after the night would add one `rand(0,7)` per night iteration for such a member (and 1000 per member in the 'T' cap case),
  so `shop_flow` digests/seed would move and both fixture generators and `shop_parity_test`/`shop_flow` expectations must be
  regenerated TS-first. If the change is made only at the call site (console `innRestYes` shop-console.ts 2338-2392, native
  `shop_orchestration.cpp:492-509`) by splitting `innRest` into pay and wake halves while leaving `innRest`/`inn_rest` byte
  identical, op 10 rows stay identical but flow rows still move. Recommendation: do NOT bundle with the jail; record as its own row.

---------------------------------------------------------------------------------------------------------------

## 6. Test plan

Binary-derived oracle for every expected value: `jf_sim.py` (byte-faithful: u8 minute/hour/day, single carry, Q halving,
saturating torch/light, 20-minute steps). Start dates are 139-04-xx unless stated; arrest location != 4; time_spell none.

| id | start (y-m-d h:m) | expected after the wait | calls |
|---|---|---|---|
| J1 | 139-04-07 12:35 | 139-04-08 08:15 | 59, 1 midnight |
| J2 | 139-04-07 07:59 | 139-04-07 08:19 | 1, 0 midnights, 0 draws |
| J3 | 139-04-07 08:40 (and 08:00) | unchanged, prevHour/torch/RNG untouched | 0 |
| J4 | 139-04-07 23:55 | 139-04-08 08:15 | 25 |
| J5 | 139-04-07 00:00 | 139-04-07 08:00 SAME day, no midnight, no draws | 24 |
| J5b | 139-04-07 03:05 | 139-04-07 08:05 | 15 |
| J6 | 139-04-07 09:00 | 139-04-08 08:00 | 69 |
| J6b | 139-04-05 10:00 (the quest_parity rows) | 139-04-06 08:00 | 66 |
| J7 | 139-13-28 20:05 | 140-01-01 08:05; every record `monthsAtInn<25` +1; `skullTreeFoundDay`, reagent stamps zeroed | 36 |
| J7b | 139-04-28 21:00 | 139-05-01 08:00 | 33 |
| J8 | J1 with timeSpell 'Q' | 139-04-08 08:05 | 117 (step 10) |
| J8b | 07:59 with 'Q' | 08:09 | 1 |
| J9 | J1 with torchTurns 255 and lightSpellMins 50 | both 0 | - |
| J10 | J1 with food 0, a 'P' member, a ring-0x2C bearer, turnsSinceStart/timeSpellTurns set | food, HP, statuses, turn counter, time-spell countdown UNCHANGED; no "Starving!" message; ring draws 0 | - |
| J11 | J1 with Shadowlord slots [2,5,255], scripted Rand returning 4 first | the 4 is REJECTED (g_location is already 4), a value equal to any of the three slots is rejected (including the slot being moved), slot 255 draws nothing; record the exact draw sequence | - |
| J12 | any start != 8 | location 4, (25,4), keys 0, floor 0, shadowlordHere -1, prevHour 7, gold untouched | - |
| J13 | direct `guardArrestJail(state)` without rand | no throw, calendar/torch advance, no relocation | - |
| J14 | arrested with floor 255 (castle basement) and a moon table | no in-loop latch (floor gate), floor 0 after | - |
| J15 | arrested with `timeSpell 'T'` (unreachable in the binary) | no hang (cap), documented as the only intentional non-binary behaviour | cap |
| J16 | 139-04-07 12:35, `[0x588C]`-style countdown | NOT asserted (unmodelled, section 7) | - |

Also add quest_parity rows (live comparison, no regeneration): extend the jail row loop with start hours {0, 7(+minute 59), 8, 10, 23},
a minute value, `s.shadowlordLocs=[2,5,255]` (native derives `has_shadowlords` from a non-empty `shadowlordLocs`, save_core.cpp:430) and
`s.timeSpell='Q'`; because the projection includes `seed: g.liveSeed()` the RNG-draw parity of the midnight re-roll is then compared
too. Keep the 32 existing rows unchanged.

What a RED-first test must show (state it in the batch log): before the change J1/J2/J4/J6/J6b/J8 fail on `day`/`minute`, J5/J5b fail on
`minute`, J9 fails on torch, J11 fails on draw count; J3, J10, J12, J13 pass both before and after (they pin what must NOT change).
Mutations that the suite must kill (one at a time):
- M1 restore the direct `hour=8;minute=0` -> J1, J2, J5, J6, J8, J9, J11 red.
- M2 test-at-tail (do/while) instead of test-first -> J3 red (one extra call, minute +20).
- M3 write location after the loop (or never) -> J11 red (the rejected value changes: arrest town instead of Yew).
- M4 write floor before the loop -> J14 red (latch fires under ground).
- M5 pre-halve 20 under Quickness / call with 10 -> J8 red (a second halving gives 5-minute steps: 233 calls, wake 08:00).
- M6 step other than 20 (e.g. 60 or 1) -> J1/J2/J5 red (minute offsets and the single-carry rule).
- M7 call housekeeping / `advance_turn` per iteration -> J10 red (food, HP, turn counter, Starving).
- M8 omit `rand` -> J11 red (zero draws); omit the cap's break semantics -> J15.
- M9 `hour < 8` or `<= 8` condition -> J4/J6 (never terminates or overshoots) and J3.
- M10 keys/floor cleared before the loop (visible only in J14) / `shadowlordHere` stale -> J12.
Boundary notes: the loop ends at the FIRST sample inside hour 8 (minute 0..19; 0..9 with Q); the test is on the hour byte only;
00:00-07:59 never crosses midnight; 08:00-23:59 always crosses exactly one; Quickness makes 144 the maximum call count.

---------------------------------------------------------------------------------------------------------------

## 7. Residual unknowns (exact)

1. No runtime witness. Everything is static. Evidence that would settle the loop end to end: a DOSBox breakpoint on
   CS:0x4F7C from TOWN 0x1328, counting calls and reading `[0x587E],[0x587F],[0x5881]` on exit; the table in section 6 predicts
   the counts (59 for 12:35, 66 for 10:00, 69 for 09:00).
2. K4F7C's tail `0x5194 call 0x71AA` is `lcall [0x5350]` with driver function 0x6C (hour/minute notice) on every call while
   `[0x5893]` is 1..0x20 (always true in the jail). I treated it as presentation; the driver routine itself (a .DRV, not in the
   tree as source) was not disassembled, so a pacing or state side effect inside the driver is not excluded. The ports have no
   equivalent and no fixture can observe it.
3. `[0x588C]` (u8, set to 0x0E at CMDS 0x0505 by the camp "Party rested!" path, tested `<1` at CMDS 0x03EA/0x044C, decremented by
   K4F7C 0x4FD7 once per hour crossed) is modelled by neither port anywhere (camp.ts ~370-378 declares it). The jail wait would
   subtract up to 24 from it (saturating), so the first camp after a jail would be allowed again; the fix above does NOT restore
   that (pre-existing, separate gap; needs its own row).
4. The inn's wind/animation RNG per iteration (K3AE6 -> K5910 -> K2F62/K4552/K51A0) is proved by static call chain and the
   `[0x58A4]` writer, not measured; it is the declared wind-rand divergence and is unchanged by this item.
5. Not traced: how the guard "(T)alk demand" reaches TOWN 0x13D6 (`0x13CF call K7AE2` -> TALK 0x0000 returns non-zero) -- it is
   inside npc_engine, so it shares the 0x1642 Time-Stop gate; irrelevant to the clock loop.
6. The T-Deck passes `c.sky == nullptr` to every command (assigned only in quest_driver.cpp:51 and only with a moon table), so on
   hardware the moon latch is not refreshed by `advance_clock` at all today; the local copy with location 4 in section 4 is a host/test
   concern. Whether hardware latches phases elsewhere was not examined.
7. Device pacing: the 1988 loop runs 25-144 driver notices in a row with no delay; the ports emit only MapChanged. No presentation
   consequence derived; out of scope.
8. JAIL-A/B left open and unchanged: the Refuge wake (BLCKTHRN 0x0C2A-0x0C3D: `advance_clock(9)` until hour 6 after
   `[0x588E]=[0x587A]=0`, then `[0x58A6]=[0x58A7]=0`) has the same direct-set shape in `partyRefuge` (blackthorn.ts ~553-554) -- same
   defect class, belongs with D-83/D-84; not verified here beyond B's and A's reads.

---------------------------------------------------------------------------------------------------------------

## 8. How this reconciliation resolved each disagreement (evidence in this session)

| point | A | B | resolved | by |
|---|---|---|---|---|
| beep_ticks (K3AE6) RNG | "1 animation frame, no RNG" | calls K5910 -> wind rolls | B right | disasm 0x3AE6-0x3B18, K5910 closure, `[0x58A4]` writers |
| torch/light byte identity | 58a7 = light, 58a6 = torch | opposite | B right (no behaviour effect) | writers CMDS 0x0DD6, SJOG 0x1A05, CAST2 0x08F0; tail floors |
| Time-Stop hang | real, cap needed | unreachable | B right; keep a defensive cap | 0x1642 gate, jump scan |
| quest_parity pins the jail | yes, 12 rows | no fixture pins it | A right; **10** rows (A miscounted, B missed the pin) | ran TS rows (jq/) |
| floor during the loop | keep old floor until 0x1337 | native "already 0" | A right (gates K4A84) | 0x1332-0x1337 vs 0x5153-0x5161 |
| inn ordering defect | yes | yes | agreed, confirmed (0x0200-0x0282 after the loop) | disasm |
| bases | brief wrong | brief wrong | confirmed independently | prologue-landing fit |
