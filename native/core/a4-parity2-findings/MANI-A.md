# MANI-A -- "Not dead!" (DS 0x953c) in In Mani Corp: spell, scroll, arena

Lens: binary first. Everything in section 2 and 3 was disassembled in this session
(capstone via re/tools/dis16.py plus my own annotating wrapper `mani_a/mdis.py`,
census `mani_a/census.py`, base fitting `mani_a/basefit.py`, all under
`.../scratchpad/findings/mani_a/`). Notes and ports were read only AFTER the binary
derivation was written down; section 4-6 are the comparison.

NOTE ON THE SHARED FOLDER: other investigators write into the same `findings/`
directory and overwrote files called `census.py`, `basecheck.py` and `dis.py`. My scripts
therefore live in `findings/mani_a/` (a file named `dis.py` also shadows the stdlib
`dis` module and breaks capstone's import; mine is `mdis.py`).

---------------------------------------------------------------------------------------

## 1. Verdict

1. DS 0x953c is `"Not dead!\n"` (10 bytes + NUL, DATA.OVL file offset 0x954c). It has
   exactly ONE referent in every binary: `CAST2.OVL:0x060b` (`mov ax,0x953c`), inside the
   resurrect routine `CAST2.OVL:0x05e0` (`resurrect_apply(idx, flag)`). The print is
   gated by the SECOND argument: `cmp word ptr [bp+4],0 / je` skips it when flag == 0.
2. The two In Mani Corp entries pass DIFFERENT flags:
   * spell (CAST.OVL `0x10ec`, jump-table entry 42): `push idx; push 0` -> flag **0** ->
     silent in `0x05e0`; the generic Cast tail prints `Failed!\n` (DS 0x4660) plus a
     glide tone. **The spell on a living target does NOT print "Not dead!"** -- the
     existing TS comment (main.ts 4448-4474) and the existing native behaviour
     (`Failed!`) are right.
   * scroll (CAST.OVL `0x12d8`, scroll 6): `push idx; push 1` -> flag **1** -> prints
     `Not dead!\n`, returns 0; the scroll function returns 0 and the (U)se epilogue
     `CAST.OVL:0x1b8a` then ALSO prints `Failed!\n` (DS 0x4a7b) and plays a glide tone.
     **The scroll on a living target prints "Not dead!" THEN "Failed!"** (the section
     11.15 claim is correct for the scroll, but incomplete: it omits the second line and
     the tone). Both ports print nothing at all for the scroll.
3. Resources on a living target: spell -- the mixed-spell count is decremented
   (`0x0ec8`), circle-8 MP is spent (`0x0ef8`) BEFORE the dispatch, and the effect
   ceremony does NOT play; scroll -- the scroll is consumed first thing (`0x11ec`),
   always, also on "Not here!" and on a cancelled picker. No RNG draw anywhere on either
   path. A world turn is taken on both (see 2.8, with a caveat).
4. Arena: **no, the arena does not have "Not dead!"**. There is no private COMBAT.OVL
   resurrect and COMBAT.OVL never references DS 0x953c or calls `0x05e0`. In the arena
   `g_location [0x5893]` is forced to 0xff, so In Mani Corp the SPELL stops at the
   location gate with `Not here!\n` (DS 0x462f) + glide, before any resource is spent,
   and the SCROLL prints `Resurrection!\n` then `Not here!\n` (DS 0x46e1) and the scroll
   is still consumed. Both ports already do this.
5. Fix needed (small): only the world scroll path, in both ports, plus the test/ledger
   edges in sections 6-7. Nothing else about In Mani Corp diverges in TEXT; two
   non-text adjacent divergences (ceremony, sound) are listed in section 8.

---------------------------------------------------------------------------------------

## 2. Binary facts

### 2.0 Tooling controls (positive controls and a warning about the base table)

* Overlay link bases were FIT, not trusted: for every overlay I counted how many of its
  external `E8` near calls land on a kernel `55 8b ec` prologue or an overlay-thunk
  (`9a ec 02 2e 07 ..`) for every candidate base 0x7000-0xffff
  (`mani_a/basefit.py`). Result (calls landing / calls):
  * CAST2.OVL 0xe1e0 = 186/216 (0xc29e = 0 of 205) -- the brief is right for CAST2.
  * CAST.OVL **0xbf80** = 270/323 (the thunk-table value 0xa8d8: 1 of 286). The brief's
    0xa8d8 is WRONG for CAST.OVL; 0xbf80 is right.
  * COMBAT.OVL **0xa290** = 160/210 (the brief's 0xbfec: 0). The brief is WRONG.
  * DUNGEON.OVL 0x81d0, COMSUBS 0xe1e0, SHOPPES 0xa290, BLCKTHRN 0xa290, TOWN/MAINOUT
    0x81d0 (brief's DUNGEON 0xe1e0 / COMSUBS 0x85fe / BLCKTHRN 0xe63e are wrong).
  A census built from the brief's table would silently miss callers. Mine uses the fitted
  bases (`mani_a/census.py` has an EMP table; it also scans the brief's value as a
  secondary pass).
* Thunk -> file mapping was checked the same way: thunk-table overlay id 18 = CAST2.OVL
  (15/16 thunk targets minus 0xe1e0 are prologues in CAST2.OVL), id 16 = CAST.OVL (targets
  0xcd3a/0xd712 minus 0xbf80 = 0x0dba and 0x1792, both prologues).
* Kernel print routine: CAST2's `call 0x3670` -> (0x3670+0xe1e0)&0xffff = **0x1850**,
  a `55 8b ec 83 ec 42` prologue; the census of 0x1850 finds 499 call sites in 8 binaries
  (positive control, run with the first-pass bases). RNG: kernel `0x2092` is the LCG (`mov ax,[0x5420]; add ax,0x9248;
  ror x3; xor 0x9248; add 0x11 ...`), 0x3aae = rand0, 0x3abe = rand30; first-pass census 197 call sites over 20 binaries.

### 2.1 The string

DATA.OVL file offset 0x954c (= DS 0x953c + 0x10): `4e 6f 74 20 64 65 61 64 21 0a 00`
= `"Not dead!\n"`. Neighbours: DS 0x9532 `"Oops...\n"`, DS 0x94f4 `"On who: "`,
DS 0x94fe `"None!"`. One occurrence of the text in DATA.OVL. A byte search for the immediate
word `3c 95` over every .OVL/.EXE finds exactly `CAST2.OVL:0x060b`
(`b8 3c 95 50 e8 5f 30`); CHOICE.EXE has an unrelated coincidence. COMBAT.OVL and
ULTIMA.EXE contain no reference (a LEA/computed form is not excluded by a byte search,
but the census in section 3 shows no call to the routine from either).

### 2.2 The resurrect routine, CAST2.OVL:0x05e0 (flat, ret 4 = two word args)

Stack (callee pops 4): `[bp+6]` = roster index (pushed first), `[bp+4]` = flag.

```
05e0 push bp / mov bp,sp / sub sp,8 / push si
05e7 cmp word [bp+6],0 ; 05eb jge 05f6      ; idx<0 -> [bp-6]=0xffff, jmp 06e1  (returns -1)
05f6 bx=[bp+6]<<5 ; 05fd cmp byte [bx+0x55b3],0x44 ; 0602 je 061a   ; status=='D' ?
0604 cmp word [bp+4],0 ; 0608 je 0611        ; flag==0 -> skip the print
060a mov ax,0x953c ; push ax ; 060e call 0x3670 (K 0x1850)   ; "Not dead!\n"
0611 mov word [bp-6],0 ; jmp 06e1            ; returns 0
061a ax=([bp+6]<<5)+0x55a8 ; [bp-2]=ax ; bx=ax
0629 mov byte [bx+0xb],0x47                  ; status 'G'
062d mov word [bx+0x10],1                    ; HP = 1
0632 al=[bx+0xa]: 0x41('A') / 0x4d('M') -> [bx+0xf]=[bx+0xe] (MP=INT)
                  0x42('B') -> [bx+0xf]=[bx+0xe]>>1 ; other classes: MP untouched
064e cmp byte [0x5888],0x62 ; jae 0678       ; karma < 98
0655..0675  [bx+0x14] = long_div( long_mul(sext(exp), zext(karma)), 100 )  (K 0x0442, 0x0496)
0678 [bp-8]=1 ; ax=[bx+0x14]/100 (cdq; idiv 100) ; dx=1 ; loop { if cx<=0 break; dx++; sar cx,1 }
06b3 byte [bx+0x16]=dl (level) ; word [bx+0x12]=30*level (mov ax,0x1e; imul dx)
06be cmp word [bp+4],1 ; jne 06ca ; ax=6 ; jmp 06d3
06ca cmp word [bp+4],0 ; jne 06d7 ; ax=8
06d3 push ax ; call 0 (CAST2:0x0000 = the ceremony/jingle routine) ; (other flag values: none)
06d7 word [bp-6]=1 ; 06dc byte [0xa9fa]=1 ; 06e1 ax=[bp-6] ; ret 4
```
Returns: -1 (idx<0), 0 (not dead), 1 (revived). The ceremony (jingle+flash, CAST2:0x0000
with 6 for flag 1, 8 for flag 0, none for any other flag value) plays ONLY on success.
No RNG call lies in 0x05e0-0x06e8; the two nearest RNG sites in CAST2 are 0x03eb and
0x05aa, both in the PRECEDING routine. The "On who" picker and the status redraw are
outside this routine (below).

### 2.3 Spell path -- In Mani Corp = spell index 42

* DATA.OVL spell-name table: `"In Mani Corp"` at DS 0x876; list order from `"In Lor"`
  (DS 0x6f9) as index 0 puts it at index 42 (e.g. `Vas Mani` 27, `Kal Xen Corp` 43).
* Cast command = CAST.OVL:0x0dba (exported by thunk 0x7e5a). Order of events:
  ```
  0dd5 K0x4988 (pick caster) ; 0de2 print DS 0x4603 "Spell name:\n:" ; 0de9 thunk 0x808e
        (CAST2:0x00de, spell-name entry) -> [bp-2]=idx ; -1 -> "None!\n"(0x4611) ; -2 -> "No effect!\n"
  0e0a [bp-8] = idx/6 + 1  (the circle = the MP cost) ; [0x588f]=circle
  0e1a LOCATION GATE (g_location = byte [0x5893]):
        ==0       -> test byte [idx+0x1c90],8
        >0x7f     -> test byte [idx+0x1c90],1          (0e2c cmp 0x7f / jbe 0e3e)
        else  (0x12/0x1d Absorbed! block at 0e3e-0e53)
        <0x21     -> test byte [idx+0x1c90],4 ; else test ...,2          (0e74-0e89)
        bit missing -> "Not here!\n" DS 0x462f (0e9b) + K0x43ae glide(0x32,1,0x7d0,0x320)
                       (0ea2-0eb2) ; jmp 0x11d9  -- NOTHING consumed
  0eb8 cmp byte [idx+0x57f0],0 -> "None mixed!\n" (0x463a) ; 0ec8 dec byte [idx+0x57f0]   <- mix count spent
  0ed3 MP=[caster*32+0x55b7] < circle -> "M.P. too low!\n" (0x4647), [bp-0xa]=0 -> tail
  0ef8 sub [caster*32+0x55b7],circle                                                 <- MP spent
  0efc [bp-0xa]=0xffff ; 0f01 level=[caster*32+0x55be] < circle -> [bp-0xa]=0 -> tail (Failed!)
  0f0f cmp idx,0x2f ; jbe ; 0f1a jmp cs:[bx-0x2f3a]    ; jump table at file 0x1146, 48 words, base 0xbf80
  ```
  Jump-table entry 42 = word 0xd06c -> file 0x10ec. DATA.OVL spell flag table
  (DS 0x1c90 = file 0x1ca0): byte[42] = **0x0e**. So bit 8 (g_location==0), bit 4
  (1..0x20), bit 2 (0x21..0x7f), but NOT bit 1 (g_location>0x7f = the arena).
  The bit for location >0x7f is set only by the combat entry (2.6).
* The handler:
  ```
  10ec call 0xffffc1aa  (K 0x812a = thunk id18 -> CAST2:0x009e  "On who:" picker)
  10ef push ax          (idx)         10f0 sub ax,ax ; 10f2 push ax   (flag = 0)
  10f3 call 0xffffbf76  (K 0x7ef6 = thunk id18 -> CAST2:0x05e0)
  10f6 mov [bp-0xa],ax                10f9 call K 0x2900 (status-panel redraw)
  10fc jmp 0x11a6
  ```
* The common tail:
  ```
  11a6 cmp [bp-0xa],1 ; jne 11b6 ; push 0x4656 ; call K0x1850      ; "Success!\n"
  11b6 cmp [bp-0xa],0 ; jne 11d6 ; push 0x4660 ; call K0x1850      ; "Failed!\n"
       push 0x320,0x7d0,1,0x32 ; call K0x43ae (glide, sound only)  (0x11d3)
  11d6 mov ax,[bp-6]   (= 1, set at 0x0dc4 and never rewritten)  -> returned to the caller
  ```
  Result -1 (picker cancelled, idx<0) prints nothing and still returns 1.
* CAST2:0x009e (the picker): `print DS 0x94f4 "On who: "` ; `call K0x2e8e` (= `push 0 ;
  call 0x2d7a`, the party-member selector: digits 1..[0x585b], ESC -> -1, no status
  test, dead members are selectable) ; idx<0 -> prints DS 0x94fe `"None!"`, else prints the
  roster NAME ([idx*32+0x55a8]); then K0x1f12 (text column) and if column != 0 a newline
  (K0x16ba(0x0a)). Returns idx or -1.
* Result for a LIVING target: idx>=0, status != 'D', flag 0 -> `0x05e0` returns 0 without
  printing -> tail prints `Failed!\n` + glide. Exact transcript outside the arena:
  `Cast...` / `Spell name:` ... / `On who: <Name>` / `Failed!`  (+ glide).
  For a DEAD target: ceremony(8), status panel redraw, `Success!\n`.

### 2.4 Scroll path -- scroll 6

* Use-item = CAST.OVL:0x1792 (exported thunk 0x7e42; "Item:" picker; `[bp-0xa]=1` at
  0x179a). For picker values 0..7 it does `push [bp-0x12]; call 0x11de` (0x1813-0x1816),
  then `[bp-0xa]=ax` (0x1819) and `jmp 0x1b8a`.
* Scroll reader CAST.OVL:0x11de:
  ```
  11e4 [bp-2]=1                                  <- default return = 1
  11ec dec byte [type+0x5820]                    <- scroll CONSUMED first, unconditionally
  11f0 print DS 0x466a "Scroll\n\n"
  1205 jmp cs:[bx-0x2d40]  (jump table file 0x1340): 6 -> 0x12d8
  12d8 print DS 0x46d2 "Resurrection!\n"
  12df cmp byte [0x5893],0x80 ; jae 12fa          ; >=0x80 (arena) -> "Not here!\n" DS 0x46e1 (12fa), [bp-2] stays 1
  12e6 call K0x812a (On who: picker) ; push ax ; mov ax,1 ; push ax        <- flag = 1
  12ee call K0x7ef6 (CAST2:0x05e0) ; 12f1 mov [bp-2],ax ; 12f4 call K0x2900 ; jmp 1350
  1350 mov ax,[bp-2] ; ... ret 2
  ```
* Use-item epilogue:
  ```
  1b8a cmp [bp-0xa],0 ; jne 1baa
  1b90 push 0x4a7b ; call K0x1850                        ; "Failed!\n" (DS 0x4a7b, a second copy)
  1b97 push 0x320,0x7d0,1,0x32 ; 1ba7 call K0x43ae       ; glide
  1baa epilogue
  ```
  `[bp-0xa]` is 1 by default and only the ring/potion/scroll branches overwrite it
  (confirmed by the existing note re/notes/use-merchants.md section 4, and by my own read
  of 0x1792-0x1b8a), so only a result of exactly 0 prints.
* Result table for scroll 6 (outside the arena): picker cancelled -> `0x05e0` returns -1 ->
  nothing after `Resurrection!` (+ `On who: None!`); living target -> `Not dead!` then
  `Failed!` + glide; dead target -> ceremony(6), redraw, nothing printed after the picker
  line. Exact living-target transcript:
  `Use item` / `Item: ... Scroll` / `Resurrection!` / `On who: <Name>` / `Not dead!` / `Failed!`.
* In the arena (g_location 0xff): `Scroll` / `Resurrection!` / `Not here!`; scroll consumed;
  `[bp-2]`=1 so no `Failed!`.

### 2.5 Turn cost

* Overworld (U) and (C): kernel command dispatcher 0x3178 (callers MAINOUT 0x0c00, TOWN
  0x158f, DUNGEON 0x07a3) starts with `[bp-2]=1` (0x317e) and returns it. (U) is
  `3413 call K0x7e42 ; jmp 31ee` -- it does not touch `[bp-2]`, so it returns 1; (C) is
  `3249 call K0x7e5a ; jmp 3231` -> `[bp-2]=ax` = the cast routine's return, which on the
  In Mani Corp path is `[bp-6]` = 1. MAINOUT 0x0c30: `cmp [bp-8],0 ; jne` -> world turn
  (`push 2 ; call K0x4f7c`, advance_clock(2)) for a nonzero return. So the original spends
  a world turn on: spell success, spell failed, spell cancelled-picker, scroll in all three
  outcomes. Caveat: I read only the MAINOUT (location 0) branch of the consumer; the TOWN
  and DUNGEON consumers of the same return were not traced. The ports document
  "UseItem does not consume a turn" (commands.cpp 1074-1076, use-merchants.md 4); that is a
  SEPARATE question and I did not adjudicate it -- do not change it under this item.
* Arena: Cast = COMBAT.OVL 0x8f0 ("Cast...\n" DS 0x6df6): after the `0x80` roster-flag gate
  `[bp-2]=1` then, once past the sleep/paralysis check (thunk 0x7de2), `0x923 [bp-2]=0`,
  then `0x95e call K0x7e5a` (the SAME CAST.OVL:0x0dba). The loop end (0xb56) re-prompts the
  same character when `[bp-2]!=0` and ends the turn when it is 0, so every cast attempt
  that gets past 0x91b spends the turn, including "Not here!". Use item = COMBAT.OVL
  0x544 helper with kind 5 (`0x59e call K0x7e42`) -> `sub ax,ax` (0x5b0): turn spent.

### 2.6 Where g_location becomes 0xff (why the arena is "combat" for every gate)

* Kernel combat entry `ULTIMA.EXE:0x5f86` (callers: ULTIMA.EXE 0x0e5d, 0x0e69, 0x6347 and
  DUNGEON.OVL 0x0c53, 0x1db5): `0x5fa8 [0x5894]=[0x5893]` (saves the real location),
  `0x5fb4 mov byte [0x5893],0xff`; restore `0x6091 [0x5893]=[0x5894]` (0x6094). The
  dungeon-room arena `DUNGEON.OVL:0x0000` ("Entering room...") does the same
  (`0x00a8 [0x5893]=0xff`, restore `0x00d8`/`0x0109`). COMBAT.OVL itself never writes
  [0x5893] and reads the SAVED location from [0x5894] for its own Absorbed! test (0x0936).
* Hence in every arena: the cast gate tests bit 1 (spell 42 = 0x0e has no bit 1) and the
  scroll's `cmp [0x5893],0x80` is true.

### 2.7 The other callers of CAST2:0x05e0 (flag values)

* SHOPPES.OVL 0x16f5 (healer "Resurrect"): `push [bp-6] ; mov ax,0xff ; push ax`; guarded
  by its OWN status test `0x16b5 cmp byte [bx+0x55b3],0x44 ; je 16bf ; jmp 15d4`
  (not-dead is handled in the shop, so `Not dead!` is unreachable there). Flag 0xff: no
  ceremony.
* BLCKTHRN.OVL 0x0b95 (`party_refuge`, function 0x0910, entry thunk 0x7a5e): loop over the
  party `push si ; mov ax,0xff ; push ax ; call K0x7ef6`, then restores HP from max. Flag
  0xff is nonzero, so a member with status != 'D' would print `Not dead!\n`; I saw no status
  test in 0x0b54-0x0bb3. The Refuge is the party-wipe scene so in play everyone is dead; I
  did NOT trace whether the entry can be reached with a living member (see section 8).

### 2.8 Mini-glossary for the implementer

DS 0x953c "Not dead!\n" (CAST2:0x060a). DS 0x4660 / 0x4a7b "Failed!\n". DS 0x4656
"Success!\n". DS 0x46d2 "Resurrection!\n". DS 0x46e1 "Not here!\n" (scroll 6, arena).
DS 0x462f "Not here!\n" (cast gate). DS 0x466a "Scroll\n\n". DS 0x94f4 "On who: ". DS 0x94fe
"None!". Roster record = 32 bytes at DS 0x55a8: +0x0a class, +0x0b status, +0x0e INT, +0x0f MP,
+0x10 HP word, +0x12 max-HP word, +0x14 EXP word, +0x16 level. Karma byte = [0x5888].

---------------------------------------------------------------------------------------

## 3. Callers census (by band, fitted bases, over-reporting direction)

Method: `mani_a/census.py` scans every `E8`/`E9` rel16 in ULTIMA.EXE and every overlay
at EVERY byte offset and resolves with the fitted base (EMP) and, separately, the brief's
base (ALT, over-reports). Positive control: 0x1850 (about 500 sites), 0x7e5a and 0x7e42 (the
COMBAT/EXE callers verified by disassembly), 0x2092/0x3aae/0x3abe RNG (about 200 sites).

| target | callers found | relevance |
|---|---|---|
| kernel thunk 0x7ef6 (CAST2:0x05e0 resurrect) | CAST.OVL 0x10f3 (spell 42); CAST.OVL 0x12ee (scroll 6); SHOPPES.OVL 0x16f5 (healer); BLCKTHRN.OVL 0x0b95 (Refuge). ULTIMA.EXE: none. COMBAT.OVL: none. Intra-CAST2 calls to 0x05e0: none. | all four read in context, all real calls |
| word `c0 e7` (0xe7c0, the thunk's jump target) anywhere | only the thunk record itself (ULTIMA.EXE 0x7efe) | no indirect/table dispatch |
| DS 0x953c (immediate `3c 95`) | CAST2.OVL 0x060b only | the one print site |
| CAST.OVL:0x11de (scroll reader) | CAST.OVL 0x1816 only (not exported by any thunk) | 1 |
| CAST.OVL:0x1792 (Use item) via thunk 0x7e42 | ULTIMA.EXE 0x3413 (kernel (U)); COMBAT.OVL 0x059e (arena (U), kind 5) | both relevant |
| CAST.OVL:0x0dba (Cast) via thunk 0x7e5a | ULTIMA.EXE 0x3249 (kernel (C)); COMBAT.OVL 0x095e (arena (C)) | both relevant |
| kernel 0x3178 (command dispatcher that owns (U)/(C)) | MAINOUT 0x0c00, TOWN 0x158f, DUNGEON 0x07a3 | the three world loops |
| kernel 0x5f86 (arena entry, sets 0xff) | ULTIMA.EXE 0x0e5d, 0x0e69, 0x6347; DUNGEON.OVL 0x0c53, 0x1db5 | + DUNGEON:0x0000 sets 0xff by itself |
| RNG 0x2092/0x3aae/0x3abe inside the touched bodies | none in CAST.OVL 0x10ec-0x10fc, 0x11a6-0x11d8, 0x11de-0x1356, CAST2 0x05e0-0x06e8, 0x009e-0x00dc, 0x0000-0x009c; kernel 0x2900 and 0x2d7a ranges clean (nearest sites 0x2acf, 0x2f70) | 0 draws, but see section 8 |

---------------------------------------------------------------------------------------

## 4. Reference (TypeScript) status

Compared against section 2:

* `game/src/core/magic/cast.ts` `castSpell` (about 340-410): spell 42 -> `{kind:"resurrect"}`
  (line 293). Location gate by `TIME_PERMITTED_BITS[42]=0x0e` (tables.ts 19-24) with
  `requiredTimeBit` -> in the arena "Not here!", `consumed:false`. Matches 0x0e1a-0x0eb2.
  Mix count before MP (0x0ec8), MP spend, level gate "Failed!" -- all match.
* `cast.ts:545-562 applyResurrect`: status 'D' only, 'G', HP 1, A/M MP=INT, B INT>>1,
  karma<98 -> `floor(exp*karma/100)`, level loop, max HP 30*level. Matches 0x05fd-0x06bb.
* `main.ts:4448-4474` spell on foot: `pickCastTarget -> hud.message(applyResurrect ? "Success!" :
  "Failed!")`. **Correct per the binary** (the comment block at 4449-4470 reproduces the same
  derivation: flag 0 silent, tail Success!/Failed!). No glide tone (see section 8).
  `main.ts:4612-4621` dungeon cast: same. `main.ts:3316-3345` arena target-spell branch is
  unreachable for In Mani Corp (castSpell already returned "Not here!").
* `useScroll.ts:90-96` `readScroll` case 6: pushes `"Resurrection!"` (DS 0x46d2), arena
  (`location>=0x80`) pushes `"Not here!"` (DS 0x46e1), else followup `{kind:"resurrect"}`.
  Correct for the dead-target and arena paths.
* **DIVERGENCE** `main.ts:4879-4883` (scroll branch, `fu.kind === "resurrect"`):
  ```
  pickCastTarget((i) => { applyResurrect(game.state.characters[i]!, game.state.karma); });
  ```
  The boolean is discarded and the comment says "sin Success/Failed". For a living target
  the binary prints `Not dead!` and `Failed!` (+ glide); this prints nothing. (Dead target and
  cancelled picker are correct.)
* `main.ts:4996-5010 applyCombatScroll` (arena scroll): `Scroll`, `Resurrection!`, `Not here!`;
  matches 2.4 (arena).
* The tools that mirror the reference: `native/core/tools/check-gameplay.ts:75` replays the
  scroll in TS for the `gameplay_parity` test and has the same silent
  `applyResurrect(...)` (line 75, `r.followup.kind==='resurrect'&&...`).

## 5. Native status

* `native/core/src/world_magic.cpp:35` (world scroll 6, also used inside dungeons):
  `case 6:say("Resurrection!");if(auto *p=target())apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand);break;`
  **DIVERGENCE**: result of `apply_target_spell` ignored; no "Not dead!", no "Failed!", no
  failure Sfx. Scroll consumption (line 27) and `Scroll` echo (line 27) are right. The
  `ceremony(...)` index table at line 37 (`indices[8]={0,-1,2,3,4,-1,-1,7}`) has no ceremony
  for 6 (the binary plays CAST2:0(6) only on success; see section 8).
  `target()` (line 14) returns null for member outside the roster, which equals the
  binary's idx<0 (silent). The device picker for scroll 6 is
  `native/targets/tdeck/main/alpha_runtime.cpp:1395` (`PartySelection/UseTarget`, all
  members selectable, dead included).
* `world_magic.cpp:64` spell effects: `say(apply_target_spell(...)?"Success!":"Failed!")` for
  Mani/FullHeal/Cure/Awaken/Resurrect: correct for In Mani Corp (living -> "Failed!").
  `world_magic.cpp:47` emits the spell ceremony `ceremony(circle,"spell-cast")` (circle 8 for
  spell 42) BEFORE the target is picked and regardless of the result; the binary plays it only
  on a successful revive (section 8).
* `native/core/src/magic.cpp:231-248` `apply_target_spell(Resurrect)`: same rule as the
  binary (status 'D', G, HP 1, A/M/B MP, karma<98 exp cut as `int(exp)*karma/100`, level loop,
  30*level). Matches 0x05fd-0x06bb for all exp that fit a signed word.
  `magic.cpp:150-203 cast_spell`: absorbed block, location bit (`ctx.combat||>=128 ->1; 0 ->8;
  <=32 ->4; else 2`), mix, MP, level -- same order as the binary. `magic_tables.inc:45`:
  `{"In_Mani_Corp",...,8,159,14}` (circle 8, time bits 14 = 0x0e). So the arena cast returns
  "Not here!" (not consumed).
* `native/core/src/combat.cpp:1160` (arena scroll 6): `say("Resurrection!");say("Not here!");`
  correct. `combat.cpp:1068-1098 combat_cast`: In Mani Corp never reaches the Resurrect branch
  (cast_spell refuses first); that branch (1085-1092) is dead for kind==Resurrect.
* `native/core/src/commands.cpp:1074-1076`: "(U)se ... do not consume a turn" -- unrelated,
  see 2.5.
* `sfx_inventory.cpp:126` (CAST.OVL 0x11d3) and `:133` (CAST.OVL 0x1ba7) list the two failure
  glides as `I::None / EvidenceUnknown` ("adjacent 'Failed!' of one spell: not mapped");
  my disassembly shows 0x11d3 = the Cast tail's `Failed!` glide and 0x1ba7 = the (U)se
  epilogue's `Failed!` glide (which the scroll path reaches).

## 6. Fixtures, corpora and tests that pin it, and what would change

Native:
* `native/core/tools/check-gameplay.ts` -- test `gameplay_parity` (CMake 3006). Line 111:
  `for item 0..15, seed 0..15: characters[0].status = ['G','P','S','D'][seed%4]`, member 0
  (or -1 every 5th seed): scroll 6 is exercised on LIVING targets. The expected stream is
  built by the TS replica at line 75 (silent). If native is fixed and the replica is not,
  seeds with status G/P/S on item 6 FAIL; both must change in the same commit (the replica
  is where the reference behaviour is stated for this test). Line 119 (arena twin) is
  already "Not here!" and must not change.
* `native/core/tests/batch13_test.cpp` `group_a_world_scroll_echo` (284-291): iterates all
  eight scrolls with a living member 0; asserts only first line == "Scroll" and no message
  starting with "Used ". Will keep passing; it will now see 4 messages for scroll 6. Add the
  new assertions there (section 7). The same file's arena group (kArenaScrollCeremony) is
  unaffected.
* `native/core/tests/item_parity_test.cpp` case 19 (`UseItem`, item `a`) and
  `fixtures/items-coverage.json`/`items.txt` (from `tools/generate-item-fixtures.ts`): the
  generator uses `scrollQuantities: Array(8).fill(0)` (line 32) so scroll 6 is not
  exercised there; no change expected. Confirm by running the suite, I did not run it.
* `native/core/tests/a4_enh2_preservation_test.cpp` B4 (`resurrection_golden`): pins
  `apply_target_spell(Resurrect)` at several karmas (the golden `kResurrection =
  0xbe90d6bb09040045`). The fix does not touch `apply_target_spell`, so this golden stays.
* `native/core/tests/a3_03_sfx_inventory_test.cpp` + `src/sfx_inventory.cpp:126,133`: if a
  failure cue is added, the two rows move from `EvidenceUnknown` to Implemented and the
  inventory counts the test pins change. Leave alone if the batch is text-only.
* Not affected: `magic.txt`/`magic_parity_test` (spell effect fixtures from
  `tools/generate-magic-fixtures.ts` use `applyResurrect` directly, line 35),
  `combat-*.json` fixtures (arena "Not here!" already the binary's).

TypeScript:
* `game/tests/use-scroll.test.ts:79-88`: pins `readScroll(...,6,0)` messages `["Resurrection!"]`
  + followup resurrect, and the arena pair. Unchanged if the new lines are printed by the
  caller (main.ts), changed if they are moved into `readScroll`.
* `game/tests/cast-onwho-consumidores.test.ts:151-175`: source-text asserts -- the Cast
  resurrect branch must NOT contain "Not dead!"/"Resurrection!" (it does not) and
  `USE_SCROLL` (the scroll branch source) must NOT contain the literal
  `'"Success!" : "Failed!"'` and must contain `fu.kind === "resurrect"` and `applyResurrect`.
  A fix has to avoid writing the ternary `? "Success!" : "Failed!"` in the scroll branch.
* `game/tests/fixtures/approved-strings.json`: has `"Resurrection!"` (line 667) and
  `"Failed!"` (417), no `"Not dead!"` entry; the anti-fabrication gates
  (`string-manifest`, `display-consts-censo`, `i18n-manifest` -- I did not run them) will want
  one: key `"Not dead!"`, citation DS 0x953c, CAST2.OVL:0x060a, flag-1 only (scroll).
* `game/src/core/__parity__/magic-run.ts` -- spell effects only; unaffected.
* Doc debris to refresh when fixed: `native/targets/tdeck/ALPHA4_UI.md` 2961 and 3342
  (the section 11.15 claim; refine to "scroll, plus Failed!"), `re/notes/potions-scrolls.md`
  row 6 ("selChar + applyResurrect + sfx"), `re/notes/magic.md` 6 (says
  `mode!=0: "Not dead!"` -- correct), `re/ledger/frontier*.json` 0x0605-0x060a text.

## 7. Proposed fix and boundary test cases

Fix (both ports, scroll only):
* Native `world_magic.cpp` case 6: after `say("Resurrection!")`, if `target()` exists: if
  `p->status != 'D'` -> `say("Not dead!")` then `say("Failed!")` (in that order, no other
  state touched); else apply as today. Member outside the roster -> nothing more. Do NOT print
  "Not dead!" from the Cast path (line 64) and do not add it to the arena (combat.cpp 1160).
* TS `main.ts` 4879-4883: use the return of `applyResurrect`; on false emit `"Not dead!"`
  then `"Failed!"` (not as a `? "Success!" : "Failed!"` ternary; success prints nothing).
  Mirror in `check-gameplay.ts:75` (the replica), add the approved-strings entry.
* Optional, separate decision: the failure glide `K0x43ae(0x32,1,0x7d0,0x320)` that follows
  `Failed!` in both tails (section 8).

Boundary cases (native `batch13_test` World fixture, steady_rand draw counter; TS equivalent):
1. scroll 6, target 'G': messages == `Scroll`, `Resurrection!`, `Not dead!`, `Failed!`; scroll
   qty 2->1; character record byte-identical; RNG draws 0.
2. scroll 6, target 'P' and 'S': same stream (the test is status != 'D').
3. scroll 6, target 'D': `Scroll`, `Resurrection!` only; revived G/HP 1/MP by class/exp cut by
   karma/level/maxHP 30*lvl (existing B4 golden covers the maths); NO "Not dead!", NO
   "Failed!"; qty 2->1.
4. scroll 6, member -1 (picker cancelled) and member >= roster: `Scroll`, `Resurrection!`; qty
   2->1; no "Not dead!"/"Failed!".
5. scroll 6, g_location >= 0x80 / arena: `Scroll`, `Resurrection!`, `Not here!`; qty -1; no
   "Not dead!", no "Failed!" (existing arena tests + gameplay_parity line 119).
6. spell 42 (In Mani Corp) on a living target, outside the arena, MP >= 8, level >= 8, mixed
   >= 1: stream ends `Failed!`; NO "Not dead!"; mixed -1, MP -8; character unchanged (pins
   that nobody "fixes" the spell by mistake).
7. spell 42 in the arena: `Not here!`; mixed and MP untouched.
8. spell 42, caster level < 8: `Failed!`, MP and mix spent, no target prompt effect (existing).
9. Order check: "Not dead!" appears before "Failed!", both after "Resurrection!".

---------------------------------------------------------------------------------------

## 8. Unresolved / not established

* Turn cost of (U)se/(C)ast in towns and dungeons: I read only the MAINOUT consumer of the
  command-dispatcher return (nonzero -> `advance_clock(2)`). The TOWN (0x159a-0x15b6) and
  DUNGEON (0x07a3) consumers, and whether the ports' "(U)se spends no turn" is a deliberate
  adjudicated deviation, are not settled here. Needed: disassemble TOWN.OVL 0x159a-0x16a0 and
  DUNGEON.OVL 0x07a3-0x0840.
* Failure sound: the glide `K0x43ae(0x32,1,0x7d0,0x320)` follows `Failed!` in the Cast tail
  (CAST.OVL 0x11d3) and the (U)se epilogue (0x1ba7), and follows the cast gate's `Not here!`
  (0x0eb2). It is sound only (no RNG, the ports pin it as unmapped). Whether to add it is a
  presentation/audio decision (A3 audio batch rows `EvidenceUnknown`).
* Ceremony (CAST2:0x0000) on success: the binary plays it from inside `0x05e0` -- arg 8 for the
  spell, 6 for the scroll -- only when a revive happens, after the status change and before
  `Success!`. Native emits the spell ceremony at cast time (world_magic.cpp:47) even for a
  living target or a cancelled picker, and plays nothing for the scroll success (index 6 is -1
  in both ports; TS ceremony.ts 123 says "sin ceremonia" because the handler body has none --
  the ceremony lives in the callee). Not a text issue; flag for the audio/presentation owner
  and do not conflate it with this fix.
* BLCKTHRN.OVL:0x0b95 (Refuge) calls the routine with flag 0xff on every roster slot without a
  status test (0x0b54-0x0bb3). If the Refuge can be entered with a member alive, `Not dead!`
  would print there. Not traced (the notes call it the party-wipe scene; D-84 in ALPHA4_UI.md
  11.15 already records that the port's Refuge does not run `resurrect_apply`). Evidence that
  would settle it: the callers of thunk 0x7a5e (BLCKTHRN:0x0910) and what precedes each.
* Transitive RNG: no RNG call site inside the bodies listed in section 3, and the kernel
  ranges for 0x2900/0x2d7a contain none, but I did not walk the entire call graph below
  K0x1850/K0x2e8e/K0x16ba. A census of every kernel function reachable from them would close it.
* Exp truncation: the binary does `sext(exp)*zext(karma)/100` as signed 32-bit and stores the
  low word; native `int(exp)*karma/100` and TS `floor(exp*karma/100)` agree for exp 0..9999
  (the game's cap); out-of-range exp is not exercised anywhere.
* The ports' dungeon scroll path passes `pos.dungeon` (0x21..0x7f band) as the location; the
  binary's gate is `>=0x80` only, so a dungeon scroll behaves like the world one (not a
  difference I could find).
