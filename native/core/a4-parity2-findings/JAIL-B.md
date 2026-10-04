# ITEM JAIL (A4-PARITY2, ledger §11.15) - report JAIL-B (ports-first lens)

All addresses below were disassembled in this session from `original/u5/ultima5/*` with `re/tools/dis16.py`
and my own annotator/census scripts in this folder (`dis2.py`, `census.py`, `bestbase.py`, `basetest.py`,
`cg.py`, `jailsim.py`). Notation: `CS:` = ULTIMA.EXE image offset (post-MZ-header, the notes' CS:IP);
overlay addresses are FILE offsets of the .OVL; "kernel 0xNNNN" is the resolved resident target.
DS strings: DATA.OVL file offset = DS + 0x10.

---------------------------------------------------------------------------------------------------

## 0. Tooling warning that affects every census (read first)

The brief's overlay-base table and `re/tools/callers_banda.py` `BASES` are WRONG for several overlays, so
`callers_banda.py` silently misses callers there ("never misses" is false). I measured, for every overlay,
the fraction of its near calls that land on a `55 8b ec` prologue in the resident image under each candidate
base (`bestbase.py`). Winning bases (hits/total resolved-into-kernel calls):

| overlay(s) | verified near-call base | evidence (hits/total) | table in brief / callers_banda.py |
|---|---|---|---|
| TOWN, MAINOUT, DUNGEON | 0x81D0 | 144/151, 182/189, 166/169 | DUNGEON 0xE1E0 (wrong) |
| OUTSUBS, NPC, SHOPPES, COMBAT, LOOKOBJ, DNGLOOK, BLCKTHRN, ENDGAME | 0xA290 | 68/69, 13/16, 210/214, 115/119, 183/190, 104/107, 129/133, 114/122 | COMBAT 0xBFEC, LOOKOBJ 0xA444, DNGLOOK 0xA2B6, BLCKTHRN 0xE63E, ENDGAME 0xE84C (all wrong) |
| CMDS, SJOG, TALK, CAST | 0xBF80 | 210/231, 212/220, 109/115, 154/163 | ok |
| SHOPPES2, **SHOPPES3**, COMSUBS, CAST2, ZSTATS, FONT | 0xE1E0 | 128/130, **85/89**, 82/86, 183/192, 149/169, 78/84 | SHOPPES3 0xA5F6 and COMSUBS 0x85FE and SHOPPES2 0xA89E (wrong) |
| INTRO | 0x81C0 | 546/555 | ok-ish (0xCD3A wrong) |

(`re/notes/overlay-load-layout.md` section 1 already has the right table; the brief and `callers_banda.py` do not.)
SHOPPES3 matters here: with 0xA5F6 zero of its 29 resolved calls land on prologues, with 0xE1E0 85 of 89 do.
I therefore used my own `census.py` (same algorithm as `callers_banda.py`, corrected bases, E8 and E9).

Positive controls that passed:
* Housekeeping census (below) reproduces exactly the four sites the notes list (TOWN 0x10D0, MAINOUT 0x0CD3,
  DUNGEON 0x0E22, CMDS 0x0671). `callers_banda.py` finds only three of them (misses DUNGEON 0x0E22 because of its
  DUNGEON base) - this is the proof that the original tool under-reports.
* advance_clock census reproduces the note's TOWN list (0x0511, 0x1328, 0x15D4) and the inn loop in SHOPPES3.
* Re-disassembly of advance_clock (CS:0x4F7C) matches `re/notes/kernel-survival.md` section 1 and
  `re/notes/reloj-advance-clock.md` line for line.

---------------------------------------------------------------------------------------------------

## 1. Verdict

**The note is wrong, AND production also diverges - but not in the way the note claims.**

1. `re/notes/kernel-survival.md` lines 293-295 ("Carcel/posada TOWN:0x1324: bucles de advance_clock(20) hasta la
   hora objetivo ... con comidas/hambre aplicandose en cada cambio de hora") is FALSE on three counts:
   a. The jail wait loop (TOWN 0x1324-0x1330) calls ONLY `advance_clock(0x14)`. No housekeeping (kernel 0x2AE8),
      no meals, no starvation, no poison tick, no ring regen, no redraw, no delay, no sound.
   b. The inn is not TOWN 0x1324. The inn night is SHOPPES3 0x01B5-0x01F4: 12 x `advance_clock(5)`, then
      `{beep_ticks(1); ring_regen; status redraw; advance_clock(9); tile transform at hour 20/5}` until `g_hour == 6`.
      Also no housekeeping, and its cadence is 5/9 minutes, not 20.
   c. `advance_clock` itself never does meals/hunger; those live only in `turn_housekeeping` (0x2AE8), and the full
      census of its callers is the four loop sites above.
   So meals/hunger do NOT run during the jail wait or the inn wait. The ports (no housekeeping there) already match
   the binary on that question. ALPHA4_UI.md's "byte scan shows no housekeeping there" is correct.
2. The one place the original DOES run housekeeping during a "sleep" is the bed hole-up (kernel `H` on a bed ->
   CMDS 0x0552, loop 0x0634-0x068D, housekeeping at 0x0671 every 10 minutes). Native fixed it (ledger D-20, Batch 30);
   the TypeScript reference still omits it. The notes `cama-241-acta.md` line 29 and `cama-apagon-296.md` section 2 label
   0x0671 "repintado de panel" - that label is wrong, 0x0671 is `turn_housekeeping`. This is probably where the
   "meals run while sleeping" idea comes from; it is true of the bed, false of jail and inn.
3. Production DOES diverge in the jail, because both ports skip the loop and write the result:
   `hour = 8; minute = 0` (only if hour != 8). The real loop (via advance_clock) additionally does, per call:
   minute/day/month/year advance, the midnight Shadowlord relocation (RNG draws), the end-of-month resets and
   `monthsAtInn` ageing, torch and light-spell minute burn, `g_prev_hour` snapshot, the hourly countdown byte 0x588C,
   the moon-phase latch. Visible: wake minute is `start_minute mod 20` (not 0), the day advances when the party was
   arrested after 08:xx, the RNG stream advances at midnight, a lit torch burns out. Size: small, deterministic,
   exactly modellable (no real-time component), no fixture pins the wrong values.
4. The inn night in both ports matches the binary except one ordering defect (poisoned ring-bearers draw RNG in the
   binary) and the declared unmodelled redraw/wind rolls (section 2.D).

Confidence: high for all binary facts (static derivation, every address quoted); medium that nothing else reaches
housekeeping (4 call sites found by byte scan, no function-pointer evidence; see section 8).

---------------------------------------------------------------------------------------------------

## 2. Binary facts

### 2.A The jail (arrest) - TOWN.OVL 0x12AE, base 0x81D0

Entry (census, section 3): sole caller `TOWN 0x13D6 call 0x12AE`, inside npc_engine TOWN 0x1352, whose sole caller
is `TOWN 0x1683 call 0x1352` in the town main loop.

```
12ae: push bp / mov bp,sp / sub sp,4
12b4: mov word [bp-4],0                      ; return value 0
12b9: cmp byte [0x5893],0x12                 ; g_location == 0x12 ? (Blackthorn's Palace)
12be: jne 0x12d8
12c0: call 0xffffb82c                        ; -> kernel 0x39fc (first-awake-member probe)
12c3: or ax,ax / 12c5: jge 0x12ca / 12c7: jmp 0x134b   ; palace branch: no prompt, no jail wait
12ca: call 0x8ca2 ; 12cd: mov ax,1 ; push ax ; call 0x11f0   ; town reload (thunk to TOWN 0x11f0)
12d8: push 0x27e2 ; call 0xffff9680          ; print DS 0x27e2 "\n\"Thou art under arrest!\"\n\n"
12df: push 0x27fe ; call 0xffff9680          ; print DS 0x27fe "\"Wilt thou come quietly?\"\n\n:"
12e6: call 0xffffa49c -> kernel 0x266c       ; getkey (uppercase)
12e9: mov [bp-2],al ; cmp al,'N' je 12f4 ; cmp al,'Y' jne 12e6   ; only Y/N accepted
12f4: cmp byte [bp-2],'Y' ; jne 0x133c
--- Y branch ---
12fa: push 0x281b ; print    DS 0x281b "Yes\n\nThe guard strikes thee unconscious!\n"
1301: sub ax,ax ; push ax ; 1304: call 0x88a0 -> kernel 0x0a70   ; set_color(0) (stores [0x52da]); NO fill_rect
1307: push 0x2845 ; print    DS 0x2845 "\nThou dost awaken to...\n"
130e: mov byte [0x5893],4            ; g_location = 4 (Yew)    <-- BEFORE the loop
1313: mov byte [0x5896],0x19         ; g_party_x = 25
1318: mov byte [0x5897],4            ; g_party_y = 4
131d: mov byte [0x24e6],1            ; redraw flag
1322: jmp 0x132b                     ; ENTER AT THE TEST
1324: mov ax,0x14 ; 1327: push ax ; 1328: call 0xffffcdac -> kernel 0x4f7c     ; advance_clock(20)
132b: cmp byte [0x587f],8            ; g_hour == 8 ?  (byte compare, unsigned equality)
1330: jne 0x1324
1332: sub al,al ; 1334: mov [0x57ac],al   ; keys = 0         <-- AFTER the loop
1337: mov [0x5895],al                      ; g_floor = 0      <-- AFTER the loop
133a: jmp 0x12cd                     ; push 1 ; call 0x11f0 (reload town), ret 0
--- N branch ---
133c: push 0x285e ; print DS 0x285e "No\n\n\"Then defend thyself, rogue!\"\n"
1343: call 0x958 (alarm) ; 1346: mov word [bp-4],1 ; 134b: ret [bp-4]
```
Verified resolutions: `0xcdac + 0x81D0 = 0x14F7C -> 0x4F7C` advance_clock; `0x88a0 + 0x81D0 -> 0x0A70`
(same primitive CMDS 0x0617 reaches as `0x4af0 + 0xBF80`); `0xa49c + 0x81D0 -> 0x266C` getkey (same as CMDS 0x0570).
`[0x587F]` = g_hour (advance_clock 0x4FE2 `inc byte [0x587f]`, 0x4FE6 `cmp ..,0x17`).

Properties that matter to a port:
* Loop = `while (g_hour != 8) advance_clock(20);` test FIRST (0x1322 `jmp 0x132b`). If the hour is already 8 no call is
  made at all: minute, prev_hour, torch, day are untouched.
* Per iteration the ONLY callee is `advance_clock`. I disassembled 0x1322-0x1337 and the loop body 0x1324-0x1330: there
  is no other call. In particular no `call -> 0x2AE8`, no `0x5910` redraw, no `0x20FA` delay, no `0x3AE6` beep.
* `g_location` is already 4 and `g_party_x/y` already (25,4) during the loop; `g_keys` and `g_floor` are cleared only after.
* Time Stop: the arrest can only start when `[0x587A] != 'T'`: TOWN 0x1642 `cmp byte [0x587a],0x54 / je 0x1686` jumps
  over `call 0x1352` at 0x1683 (the only path to it passes 0x1642/0x165f). So the 'T' hang (below) is unreachable.
  Under Quickness `'Q'` (0x51) the NPC pass runs only on alternate turns (0x1649-0x165d) but the arrest, once reached,
  runs with the halved step (below).
* Exit result of the jail loop (derived by simulation of 0x4F7C, `jailsim.py`, byte-faithful carry order):
  wake minute = `start_minute mod 20` (mod 10 with Q); hour 8; day+1 iff start hour > 8 (day/month/year cascade as 0x4FD2-0x509A);
  e.g. 07-04-139 12:35 -> 08-04-139 08:15 (59 calls, 1 midnight); 23:55 -> next day 08:15 (25 calls);
  00:00 -> same day 08:00 (24 calls, 0 midnights); 07:59 -> 08:19 (1 call); 08:40 -> unchanged (0 calls);
  09:00 -> next day 08:00 (69 calls); 139-13-28 20:05 -> 140-01-01 08:05 (36 calls).

### 2.B What advance_clock (CS:0x4F7C, ret 2, 0x4F7C-0x519C) does per call (re-derived; every line read)

```
4f84 cmp word [bp+4],0 ; jne 4f8d ; jmp 0x50a1              n==0 -> tail only (not used here)
4f8d cmp byte [0x587a],0x51 ; jne 4fa0 ; sar word [bp+4],1 ; if 0 -> inc  ('Q': n>>=1, min 1; 20 -> 10)
4fa0 mov al,[0x587f] ; mov [0x5880],al                      g_prev_hour = g_hour   (EVERY call)
4fa6 cmp byte [0x587a],0x54 ; je 4fc8                       'T': skips minute add AND both sub-saturating (hang if looped)
4fb0 add byte [0x5881],al                                   minute += n  (byte add)
4fb4 push 0x58a7 ; push n ; call 0x3f36                     torch minutes   -= n  (u8 saturating at 0: 3f43 jbe -> 0)
4fbe push 0x58a6 ; push n ; call 0x3f36                     light-spell min -= n  (same)
4fc8 cmp byte [0x5881],0x3b ; ja 4fd2 ; jmp 50a1            minute > 59 ?
4fd2 sub byte [0x5881],0x3c ; call 0x3f36(0x588c,1) ; inc byte [0x587f]      hour++ (+ 0x588C u8 countdown -1)
4fe6 cmp byte [0x587f],0x17 ; ja 4ff0 ; jmp 50a1
4ff0 mov byte [0x587f],0                                    midnight block:
4ffa..504f  for i in 0..2: if byte[0x58c8+i] < 0x80: loop { di = rand_range(1,8) [call 0x2092 push 1; push 8];
            if di == byte[0x5893] (g_location) di=0; if any of the 3 slots == di di=0; } until di != 0 ; byte[0x58c8+i]=di
5051 inc byte [0x587e] (g_day) ; cmp 0x1c ; jbe 509e ; month-end: [0x585a]=[0x5859]=[0x5858]=0,[0x57b2]=0,[0x587e]=1,[0x5959]=0 ;
     roster +0x17 (monthsAtInn) < 0x19 -> ++ for 16 records ; inc [0x587d] g_month ; >0xd -> 1 ; inc word [0x5874] g_year
509e call 0x2900 (status redraw)
50a1..5145 light recompute (reads hour, minute, [0x5893]==0x19, [0x5895]>0x7f; [0x58a6],[0x58a7] floors; sets [0x24e6])
514a cmp [0x5880],[0x587f] ; je 5186                          hour changed vs the snapshot:
5153 if [0x5893] < 0x21 and [0x5895] < 0x80: call 0x4a84     (sky / moon-phase display)
5164..5183 [0x5884] = 12-hour value of g_hour
5186 if [0x5893] != 0 and < 0x21: call 0x71aa                (lcall [0x5350]=0x6c driver notice with hour/minute; presentation)
```
Only residents reached: 0x3F36 (counter), 0x2900 (status redraw), 0x4A84, 0x71AA, 0x2092 (rand). It never calls 0x2AE8.

`turn_housekeeping` CS:0x2AE8 (read in full, 0x2AE8-0x2BD1): poison 1 HP (0x2B40 -> 0x2A52) for 'P', dead-active-member
reset (0x2B25), "Starving!" DS 0x54C8 + 0x2AA8 on hour change with `word [0x57a8]==0`, meals via 0x3F54 at hours 6/12/18,
`g_prev_hour = g_hour` (0x2B99), `counter_add_u8(0x588B,1,0xFF)` turn count, `[0x588E]` time-spell countdown
(0x2BAE-0x2BC7), ring regen 0x400C. None of that runs in the jail.

### 2.C Why "meals run in jail" cannot be true even indirectly

Hour change inside the loop: advance_clock sets `[0x5880] = g_hour` on every call, so after the loop `[0x5880] == 7`
and `g_hour == 8` (the last call always crosses 7 -> 8 unless the loop did not run). The arrest happens in the town
loop's NPC pass (0x1683), AFTER that turn's `advance_clock(1)` (0x15D4) and post-turn/housekeeping (0x15EC -> 0x10D0). The
NEXT turn begins with `advance_clock(1)` (0x15D4), which re-snapshots `[0x5880] = 8`, so no hour-change flank is ever
seen by housekeeping for the 12-ish skipped hours: no meal at 12/18/6, no starvation, even with food 0.

### 2.D The inn night - SHOPPES3.OVL 0x0072 (R) ... 0x02A3, base 0xE1E0 (verified, see section 0)

Resolutions: `0x6d9c -> 0x4F7C` advance_clock; `0x5906 -> 0x3AE6` beep_ticks; `0x5e2c -> 0x400C` ring regen; `0x4720 -> 0x2900`
status redraw; `0x7730 -> 0x5910` viewport redraw; `0xffff98ba -> 0x7A9A` thunk -> TOWN 0x0170 (hour tile transform);
`0xffff98ae -> 0x7A8E` thunk -> TOWN 0x1694 (NPC snap); `0x3670 -> 0x1850` print.

```
0172 mov byte [0x24e6],1 ; 0177 [bp-2] = [0x587b] (active member) ; 0182..01a5 for i<[0x585b]: status 'G'(0x47) -> 'S'(0x53)
01a8 call 0x2900 ; 01ab call 0x5910 (one viewport redraw) ; 01b2 print DS 0x4e44 "Zzzzzz....\n\n"
01b5 mov [bp-8],0xc ; mov si,0xc
01bd  push 5 ; call advance_clock ; dec si ; je 0x1ef ; jmp 0x1bd            12 x advance_clock(5) (each separately halved by Q)
01ca  push 1 ; call 0x3ae6 (beep_ticks 1) ; 01d1 call 0x400c ring regen ; 01d4 call 0x2900 ;
01d7  push 9 ; call advance_clock ; 01de cmp [0x587f],0x14 je 0x1ec ; cmp [0x587f],5 jne 0x1ef ; 01ec call 0x7a9a
01ef  cmp byte [0x587f],6 ; jne 0x1ca                                      test-first loop, exits on g_hour == 6
01f6 print DS 0x4e51 "Morning!\n" ; 01fd call 0x7a8e (NPC snap)
0200..0280 for i<[0x585b]: if status != 'D': HP = maxHP; MP by class (A/M: INT; B: INT>>1);
           if status == 'P': status='D', HP=0, print name + DS 0x4e5b ; else if 'S' -> 'G'
0285 [0x587b] = [bp-2] ; 028b inc byte [0x5896] (step out of bed) ; [0x24e6]=1 ; 0294 call 0x2900
```
There is NO call to 0x2AE8 anywhere in SHOPPES3 (census), and no call to it inside 0x01B5-0x01F4. The loop calls
beep_ticks, ring regen (RNG: `rand_range(0,7)` per living ring-0x2C bearer, 0x4043), status redraw, advance_clock, tile transform.
`beep_ticks` CS:0x3AE6 (read): if `[0x58a4] != 0` and n > 0: n times `{call 0x5910; call 0x20FA(1)}`.
`0x5910` = viewport_redraw; I confirmed by call-graph (`cg.py`) it reaches rand: `0x2F62 maybe_change_wind` (`rand(0,63)`, plus
rand(0,4)/rand(0,255) on a 1/64 hit) and `0x4552`, both gated by `[0x5891] != 0` (latch cleared only by 'T'). So the original inn
loop consumes wind RNG every iteration while `[0x58a4] != 0`. That is the already-declared "delays animados" divergence
(`re/notes/wind-rand-decision.md` section 3 table, Class 3); NOT part of this item, but the TS docblock that calls beep_ticks
"sin RNG" (shops.ts ~line 435) is inaccurate.

ORDERING DEFECT visible in the listing: in the binary the poison death (`status P -> D`, 0x023F) and the HP/MP refill happen
AFTER the wait loop (0x0200-0x0280). Ring regen (0x400C) skips only status 'D' (0x4032 `cmp byte [si],0x44`), so a POISONED
Ring-of-Regeneration bearer still draws `rand(0,7)` once per loop iteration in the original.

### 2.E The bed hole-up (for completeness; this is the loop that DOES run housekeeping) - CMDS.OVL 0x0552, base 0xBF80

Kernel command `H`: `ULTIMA.EXE CS:0x3288`: if g_location == 0 or > 0x20 -> camp 0x3C9A (no housekeeping caller); else tile under party
must be 0xAB (`cmp word [bp-4],0xab` at 0x32B9, else DS 0xA17A "Only in bed!\n") then `call 0x802E` = CMDS 0x0552 (DS 0xA170 "Hole up- ").
```
062e si = target hour [bp-6] (computed 0x059e-0x05b0: hour + digit - 0x30; if > 0x17: -= 0x17)
0631 jmp 0x63b ; 0634 push 1 ; call 0x617a -> 0x20FA delay(1) ; 063b cmp si,[g_hour] ; je 0x692   test-first, exit on hour == target
0647 push 0xa ; call -> 0x4F7C advance_clock(10)
0650..0664 if hour changed and (hour == 0x14 or hour == 5): call 0xffffbb1a -> 0x7A9A tile transform
066e call -> 0x4A84 ; 0671 call 0x6b68 -> 0x2AE8 turn_housekeeping ; 0674 call 0x6980 -> 0x2900 ; 0677 call -> 0x7A8E NPC snap
067a..068d find_object_at_xy(0x368E) on the party tile ; or ax,ax ; je 0x634 ; else si=-1 -> "Thrown out of bed!" DS 0x422A
```

---------------------------------------------------------------------------------------------------

## 3. Callers census (corrected bases; `census.py`; E8 and E9)

### 3.1 kernel_turn_housekeeping 0x2AE8 (complete)
| site | note |
|---|---|
| TOWN 0x10D0 (raw 0xA918) | town main loop post-turn |
| MAINOUT 0x0CD3 (raw 0xA918) | overworld main loop |
| DUNGEON 0x0E22 (raw 0xA918) | dungeon loop (callers_banda.py MISSES this one: wrong base) |
| CMDS 0x0671 (raw 0x6B68) | **bed hole-up loop** - the only "sleep" that runs it |
No ULTIMA.EXE caller (so camp 0x3C9A is not one), none in SHOPPES3, COMBAT, TOWN 0x1324, BLCKTHRN, SJOG, CAST2, OUTSUBS.
A raw-word scan for the 16-bit value 0x2AE8 (function-pointer/jump-table evidence) found only mid-instruction coincidences in the three
ULTIMA.EXE hits I examined (0x1E8D, 0x4B9D, 0x6972); I did not individually examine the overlay hits. Relevant: only the four above.

### 3.2 kernel_advance_clock 0x4F7C (complete) and relevance to this item
| site | what | relevant? |
|---|---|---|
| TOWN 0x1328 | **jail wait, advance_clock(0x14)** | YES (the item) |
| TOWN 0x15D4 | town main loop, advance_clock(1) per action | no |
| TOWN 0x0511 | n=0 light recompute at map init | no |
| SHOPPES3 0x01C1 / 0x01DB | **inn night**: 12 x 5, then 9-minute loop | YES (the "inn wait") |
| CMDS 0x064B | bed hole-up 10-minute step | related (runs housekeeping) |
| CMDS 0x0318 | `advance_clock(5)` in an unrelated CMDS command (preceded by `[bp-0x1e] = g_hour`; kernel-survival 5.5 attributes 5 x advance_clock(5) to ship repair; not verified here) | no |
| BLCKTHRN 0x0C2E | **Refuge wake loop** `advance_clock(9)` until hour==6, after `[0x587A]=0` | sibling (same class as the jail shortcut; D-84 area) |
| BLCKTHRN 0x05B4 | palace puzzle, advance_clock(2) | no |
| COMBAT 0x0C76 | advance_clock(1) per ten actions (D-88) | no |
| DUNGEON 0x0F2F | dungeon step cost | no |
| MAINOUT 0x0006, 0x0066, 0x0462, 0x068C, 0x0A56, 0x0C3D | overworld costs | no |
| OUTSUBS 0x0993 | after beep_ticks(1), advance_clock(0) (n=0) | no |
| CAST2 0x10F1 | advance_clock(0x10) in a spell | no |
| ULTIMA.EXE 0x3CFC (5), 0x6356 (0) | camp / light recompute | no |

### 3.3 The jail itself
* TOWN 0x12AE (arrest): sole caller TOWN 0x13D6 (npc_engine 0x1352). No thunk to TOWN 0x12AE exists in the stub table.
* TOWN 0x1352 (npc_engine): sole caller TOWN 0x1683, guarded by `[0x587A] != 'T'` (0x1642-0x1647).
* The 0x1324 loop has exactly one entry (0x1322 `jmp 0x132B`, from the Y branch) and one exit (0x1330 fall-through to 0x1332).
* Inside the Palace (g_location 0x12) the same function takes the 0x12C0 branch: no prompt, no cell, no wait loop.
UNRESOLVED (not needed for the fix): how the (T)alk guard-demand route (TS comment "por las DOS vias") reaches 0x13D6 - the census shows no
direct caller other than npc_engine, so it must come back through the town loop; I did not trace it.

---------------------------------------------------------------------------------------------------

## 4. Reference (TypeScript) status - game/src/core/**

| piece | file : lines | status vs binary |
|---|---|---|
| jail mutation | `world/blackthorn.ts:741-757` `guardArrestJail(state)` | sets loc 4 / (0x19,4) / floor 0 / keys 0 / `shadowlordHere`, then `if (hour !== 8) { hour = 8; minute = 0 }` (lines 752-755, doc lines 733-738 declare "minute=0 canonico, dia no modelado") |
| constants | `blackthorn.ts:711-716` `LOC_YEW`, `JAIL_X`, `JAIL_Y`, `JAIL_WAKE_HOUR` | correct values |
| caller | `world/guard-encounters.ts:485-504` `resolveGuardArrest`, call at 494 | fine; `GuardCtx` (63-...) has `rand` but NO sky context |
| ctx build | `game.ts:5604-5630` `guardCtx()`; `game.ts:5649` `resolveGuardArrest` | no `sky` injected |
| clock | `world/survival.ts:298-... advanceClock(state, minutes, rand?, sky?)` | complete port of 0x4F7C (Q, T, torch, light, midnight relocation 204 `relocateShadowlordsAtMidnight`, month-end resets, monthsAtInn, latch 271) - exactly what the loop needs |
| bed hole-up | `world/camp.ts:246-326` `bedSleep` (`hours*BED_STEPS_PER_HOUR` for-loop), `bedSleepStep` 311-326 | OMITS 0x0671 housekeeping and the 0x0664/0x066E calls, and uses a count loop instead of the target-hour test-first loop (ledger D-20 "TypeScript still omits both") |
| inn night | `shops/shops.ts:461-482` `innNightPass`; rest `383-418` `innRest`; console `ui/shop-console.ts:2357-2392`; `game.ts:4844` `innSleepUntilMorning` | loop shape, steps (12x5, 9), hour-6 exit, ring regen, tile transform: faithful. Defect: `innRest` runs P->D and the refill BEFORE the night loop (binary: after); doc says beep is "sin RNG" |
| housekeeping | `world/survival.ts:419` `turnHousekeeping` | faithful; NOT called by jail/inn (correct) |

Differences from the binary for the jail (TS):
1. No loop: no per-step `advance_clock(20)`; wake minute forced to 0 instead of `start_minute mod 20`.
2. `state.time.day/month/year` never advance (arrest after 08:xx should add a day; 23:xx-07:xx same day).
3. No midnight Shadowlord relocation, so no `rand_range(1,8)` draws (>=1 per slot with value < 0x80, retry loop, excluding
   g_location == 4 and the three slots) - RNG stream is short by those draws whenever start hour > 8.
4. Torch and light-spell minutes are not burned (59 calls x 20 for a 12:35 start; torch u8 saturates to 0).
5. `g_prev_hour`, `0x588C`, month-end resets, `monthsAtInn` ageing, moon-phase latch: skipped.
6. Order: the binary sets location BEFORE the loop and keys/floor AFTER; irrelevant today, load-bearing once the loop exists
   (see section 7, gotchas).

---------------------------------------------------------------------------------------------------

## 5. Native status - native/core/src/**

| piece | file : line | status |
|---|---|---|
| jail mutation | `blackthorn.cpp:61` (`blackthorn_action`, `BlackthornAction::Arrest`, `agree` branch) | one-liner: `g.position={{25,4},{4,0}}; g.keys=0; c.travel.shadowlord_here=-1; if(g.time.hour!=8){g.time.hour=8;g.time.minute=0;}` - same shortcut as TS |
| clock | `turn.cpp:25-70` `advance_clock(g, s, minutes, rand*, sky*)` | faithful (Q, T, torch `torch_turns`, light spell, midnight relocation, month-end, ageing, latch); one deliberate single carry per level (matches 0x4FC8-0x509A) |
| context | `commands.h:124,133` `CommandContext::turn`, `::sky` (a `SkyRefresh` with an int `location`) | available at the call site; `blackthorn_action` already has `Rand rand` |
| bed hole-up | `rest.cpp:304-390` `bed_sleep*` | FAITHFUL (target hour with -23, test-first, advance_clock(10), tile refresh at 5/20, `turn_housekeeping` per step at 329, snap, thrown-out gate) |
| inn night | `shops.cpp:329-345` `inn_night_pass`, `inn_rest` 233-263, orchestration `shop_orchestration.cpp:492-509` | loop faithful; same P->D-before-loop ordering defect (`inn_rest` is called at 497 before `inn_night_pass` at 503); beep/wind rolls not modelled (declared) |
| housekeeping | `turn.cpp:79-110` | not called by jail/inn (correct) |

Same six jail differences as TS (the native copy was written to mirror the TS `guardArrestJail`; test comment at
`batch4_group_a_test.cpp:208-211` says "exact reference guardArrestJail mutation").

---------------------------------------------------------------------------------------------------

## 6. Fixtures, corpora, tests that pin it - and what would change

Searched: `native/core/fixtures/*`, `native/core/build-shops/*`, `native/core/tools/generate-*-fixtures.ts`, `native/core/CMakeLists.txt`,
`game/src/core/__parity__`, `game/tools`, `game/tests`, `game/e2e`. No generated fixture or corpus contains the arrest/jail path
(no `guardArrestJail`/`resolveGuardArrest`/"dost awaken" hit in any fixture, generator, or `__parity__`).

Jail pins (all weak, none pins minute/day/RNG):
* `game/tests/guard-arrest-live.test.ts:258-276`: start `{year 139, month 4, day 7, hour 12, minute 35}` (line 78); asserts only `hour == 8`
  (line 274), location/x/y/floor/keys, gold. Would stay green; should gain `minute == 15`, `day == 8`.
* `game/tests/shadowlord-physical-flag.test.ts:75-90` calls `guardArrestJail(s)` DIRECTLY with one argument (hour 21): the signature must stay
  callable as `guardArrestJail(state)` (make `rand`/`sky` optional) or this test breaks; with `rand` absent advanceClock skips the relocation
  (`if (rand) ...`, survival.ts ~348) - good, that test stays valid.
* `game/tests/npc-hostil-ataca.test.ts:250-270` (Y/N branches, no clock assertions).
* `game/e2e/grandtour/ch08-yew.spec.ts:237-241` uses "hour != 8 and not at (25,4)" as a NO-JAIL control; unaffected.
* Native `native/core/tests/batch4_group_a_test.cpp:194-219` A3 (`GuardWorld(2)` starts hour 10, minute 0): asserts location 4, (25,4), keys 0, hour 8.
  With a real loop it still passes (10:00 -> 08:00 next day), loop terminates (time_spell is 0 in that harness). A4 (No branch) unaffected.
* `native/core/tests/frontend_test.cpp`, `quest_driver.cpp` mention the arrest event plumbing, not the clock.

Inn/bed pins:
* Inn helpers pinned individually: `native/core/build-shops/helpers.txt` via `shop_parity_test.cpp` ops 10 (`inn_rest`) and 15 (`inn_night_pass`);
  generated by `generate-shop-fixtures.ts`, drift test `typescript_shop_fixture_drift` (CMakeLists 3018). The P->D ordering fix is at the CALL SITE
  (console / orchestration), so these helper rows do not move unless `innRest`/`inn_rest` are split.
* Inn flow: `generate-shop-flow-fixtures.ts` (line 37 binds `Game.prototype.innSleepUntilMorning`, line 84 drives the Inn rest/leave/pickup flows) ->
  `typescript_shop_flow_drift`. A poisoned ring-bearer only matters if a flow row has that state; check before regenerating.
* Bed: `native/core/tests/item_parity_test.cpp:437-441` states "The TypeScript generator omits CMDS:0x0671 housekeeping and uses a fixed tick count";
  rows op 10/22 are checked by a native tick invariant instead of equality (`original_bed_cases`), generator `generate-item-fixtures.ts:62,85`
  calls `C.bedSleep`. Fixing the TS bed would change those generated rows (then the special case can be dropped).

What would change if the jail fix is applied: only the jail path - wake minute (`start mod 20`), day/month/year, torch/light spell, Shadowlord
slots + RNG stream position afterwards, `prev_hour`. No parity fixture row changes (none pins it); the item needs NEW tests (section 7).

---------------------------------------------------------------------------------------------------

## 7. Proposed fix and boundary tests

Scope decision for the batch: (a) documentation correction (mandatory), (b) jail loop port (small, deterministic, recommended),
(c) optional sibling clean-ups listed last. Do NOT add housekeeping to jail or inn.

### 7.1 Documentation
* `re/notes/kernel-survival.md` lines 293-295: replace with: jail = TOWN 0x1324 `while (g_hour != 8) advance_clock(20)`, no housekeeping;
  inn = SHOPPES3 0x01B5-0x01F4 (12x5 then 9-min loop to hour 6, beep+ring regen, no housekeeping); bed hole-up = CMDS 0x0634-0x068D, advance_clock(10) +
  housekeeping 0x0671 (+ tile transform 0x0664 at 20/5, 0x066E sky, NPC snap, bed gate); meals/hunger are only in housekeeping.
* `re/notes/cama-241-acta.md` line 29 and `cama-apagon-296.md` section 2 table: `0x0671` is `turn_housekeeping` (0x2AE8), not "repintado de panel".
* `ALPHA4_UI.md` 2767 (still true for camp/inn/jail/combat but name CMDS 0x0671 as the bed loop), 2961 and 3342 (mark the item resolved: note wrong,
  jail loop shortcut recorded as a new/renamed D-row), ledger D-20 stays RESOLVED (native).
* Record the overlay-base errors (brief, `callers_banda.py` `BASES`): SHOPPES3/SHOPPES2/COMSUBS/CAST2 = 0xE1E0, COMBAT/LOOKOBJ/DNGLOOK/BLCKTHRN/ENDGAME = 0xA290, DUNGEON = 0x81D0.

### 7.2 Code (reference first, then native; fixture drift check)
TS:
```
guardArrestJail(state, rand?, sky?): 
   position.location = 4; x = 0x19; y = 4;                      // 0x130e-0x1318, BEFORE the loop
   for (n = 0; state.time.hour !== 8 && n < CAP; n++) advanceClock(state, 20, rand, sky);   // 0x1322-0x1330, test FIRST
   keys = 0; floor = 0;                                          // 0x1332-0x1337, AFTER
   shadowlordHere = computeShadowlordHere(state);                // unchanged (post-reload)
```
CAP follows the inn precedent (`INN_NIGHT_MAX_STEPS = 1000`); the binary has none but the 'T' hang is unreachable (section 2.A), worst real case Q: 144 calls.
Remove the `JAIL_WAKE_HOUR` shortcut block. Add `rand`/`sky` to `GuardCtx` and feed them from `Game.guardCtx()` (`rand: this.rand` already exists).
Native: in `blackthorn_action`'s Arrest/agree branch: set `g.position` first, then
`while (g.time.hour != 8 && n++ < 1000) advance_clock(g, c.turn, 20, &rand, sky)`, then `g.keys = 0` (floor is already 0 from the position init), keep
`shadowlord_here = -1`.

GOTCHAS (these are what a quick port gets wrong):
1. Location must be 4 BEFORE the loop: midnight relocation rejects `di == g_location` (0x5011), the moon-phase refresh is gated by `location < 0x21`, and
   `skyRefreshCtx.location` is read lazily from the live position. In TS `Game.skyRefreshCtx` is a getter evaluated when called, so build/evaluate the sky
   context AFTER the position write (pass a thunk, not a pre-built object). In native `SkyRefresh::location` is a plain int copied by the caller
   (`commands.cpp:214,268,391` copy `c.sky`): take a local copy of `*c.sky` and set `.location = 4` before the loop, or the gate sees the origin town.
2. Test-first: if hour is already 8 make ZERO calls (minute, prev_hour, torch untouched).
3. Quickness: step 20 is halved inside advance_clock (do not pre-halve); wake minute = `start mod 10` then.
4. RNG: draws occur only inside the midnight block; `rand` must be the live stream (the TS clock skips relocation when `rand` is absent; native requires
   `s.has_shadowlords`). Draw order relative to text: none (no text or sound inside the loop; the two prints and `set_color(0)` come first).
5. Do not call housekeeping, redraws, delay, wind roll or any beep: the original loop has none.
6. `prev_hour` ends at 7 (last call crosses 7->8); the next town turn re-snapshots, so do not "fix" it.
7. Keep `guardArrestJail(state)` callable with one argument (`shadowlord-physical-flag.test.ts:85`).

### 7.3 Boundary test cases (new; RED first; all with time_spell none unless stated; arrest origin location != 4)
| id | start (y-m-d h:m) | expected |
|---|---|---|
| J1 | 139-04-07 12:35 | 139-04-08 08:15; 59 advance_clock(20) calls; 1 midnight block |
| J2 | 139-04-07 07:59 | 139-04-07 08:19; 1 call; 0 RNG draws |
| J3 | 139-04-07 08:40 | unchanged 08:40 (0 calls, prev_hour/torch untouched) |
| J4 | 139-04-07 23:55 | 139-04-08 08:15; 25 calls |
| J5 | 139-04-07 00:00 | 139-04-07 08:00; 24 calls; 0 midnights |
| J6 | 139-04-07 09:00 | 139-04-08 08:00; 69 calls |
| J7 | 139-13-28 20:05 | 140-01-01 08:05; `monthsAtInn < 25` incremented; `[0x585A,0x5859,0x5858,0x57B2,0x5959]` cleared |
| J8 | J1 with time_spell 'Q' | 139-04-08 08:05; 117 calls |
| J9 | J1 with torch 255, light spell 50 | both 0 after the wait |
| J10 | J1 with food 0, a 'P' member, a Ring of Regeneration bearer, `[0x588B]`/`timeSpellTurns` set | food, HP, status, turn counter, time-spell turns UNCHANGED; no "Starving!"; ring draws == 0 (no housekeeping) |
| J11 | J1 with Shadowlord slots < 0x80, rand script forcing a first draw of 4 | the draw is rejected (g_location == 4 during the loop), retry consumed; count of draws recorded |
| J12 | any start != 8 | `prev_hour == 7` after; location 4, (25,4), keys 0, floor 0, `shadowlordHere == -1` |
| J13 | direct `guardArrestJail(state)` with no rand | no throw, calendar advances, no relocation (matches `shadowlord-physical-flag`) |
Inn (only if the optional inn fix is taken): I1 poisoned ring-bearer, count `rand(0,7)` draws == number of loop iterations (binary) vs 0 (today).

### 7.4 Optional siblings (not required for the jail verdict)
* Inn ordering: run the member refill/P->D after `inn_night_pass` (binary 0x0200-0x0280); both ports; changes RNG only for a poisoned ring-bearer.
* TS bed hole-up: add 0x0671 housekeeping, the target-hour loop and the 0x0664/0x066E calls (native already has them, D-20); then drop the special case in `item_parity_test.cpp:437`.
* Refuge (BLCKTHRN 0x0C2A-0x0C36 `advance_clock(9)` until hour == 6, after `[0x587A]=0`): same shortcut class as the jail (`partyRefuge` sets hour 6 / minute 0, blackthorn.ts:553-554); belongs with D-83/D-84.

---------------------------------------------------------------------------------------------------

## 8. Unresolved / evidence that would settle it

1. No DOSBox/runtime witness was run; everything is static derivation. A runtime trace (breakpoint on 0x4F7C from TOWN 0x1328, count calls and final
   minute/day) would confirm section 2.A's simulation end to end. The simulation assumes the binary's single-carry order (hour at >59 min, day at >23 h),
   which I read in 0x4FC8-0x509A.
2. Housekeeping reachability is a byte scan plus a call-site census (4 sites, matching the notes). Not excluded: an indirect call through a runtime-built
   pointer. No pointer-table evidence found for 0x2AE8 (3 ULTIMA.EXE word hits were mid-instruction; overlay hits not individually inspected).
3. Meaning of `[0x588C]` (u8 countdown decremented per hour in advance_clock) is unknown; harmless to the fix (advance_clock handles it).
4. `0x71AA` (`lcall [0x5350]` with function 0x6C, hour/minute) is some driver notification; treated as presentation, not verified.
5. How the (T)alk guard-demand route reaches TOWN 0x13D6 is not traced (census shows only the npc_engine caller). Not needed for the loop.
6. `[0x58A4]` (gate of beep_ticks/viewport_redraw) semantics: wind-rand-decision.md calls it "map view active"; I did not re-derive it. It only affects the
   already-declared inn wind-roll divergence.
7. Whether the original ever lets a party be arrested with a Time-stop NPC pass: shown unreachable by TOWN 0x1642-0x1647; the same guard does not exist for the
   Refuge (it clears `[0x587A]` itself at 0x0C25).
