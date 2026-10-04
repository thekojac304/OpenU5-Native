# MANI-FINAL -- "Not dead!" (DS 0x953c) and In Mani Corp (spell, scroll, arena): reconciled

Reconciler: In Mani Corp. Inputs: MANI-A.md, MANI-B.md. Everything in sections 1-2 marked "(re-disassembled)" was
disassembled again in this session with `re/tools/dis16.py` (capstone); my own scripts (fit, census, RNG closure) are in
`.../scratchpad/findings/mani_f/` (`census_f.py` base fit, `census2_f.py` census, `rngclosure.py`, `show.py`). Nothing in
the repository was edited, nothing was built or run.

---------------------------------------------------------------------------------------------------

## 0. Reconciliation log

### 0.1 Where A and B disagreed or left a hole, and how each was settled in the binary

| # | Point | A | B | Settled by |
|---|---|---|---|---|
| 1 | Wording of the verdict | spell: no, scroll: yes + "Failed!" | same | Both identical in substance; re-disassembled (2.2-2.5). |
| 2 | Cancelled "On who:" on the DEVICE: refund? | not mentioned | native runtime refunds scroll / charge / MP | Confirmed in code: `alpha_runtime.cpp:1395` opens `PartySelection/UseTarget` and only dispatches `UseItem` after the pick; `AlphaRuntime::modal` (1375) clears `pending_use_item_` on cancel and dispatches nothing. So on the device a cancelled scroll pick does NOT spend the scroll; binary spends it at CAST.OVL 0x11ec before the picker. Separate divergence, not part of the text fix (section 7.4). |
| 3 | Refuge (BLCKTHRN 0x0b95, mode 0xFF): can a LIVING member reach `resurrect_apply` and print "Not dead!"? | open | open | **Settled for the MAINOUT and TOWN routes** (2.7): the Refuge is entered only when kernel 0x39fc returns -1, which is only true when no member among the first `[0x585b]` is 'G' or 'P' or 'S'. The Refuge loop runs over the same `[0x585b]` members. So every member it touches is 'D' (the only other status the game writes) and "Not dead!" is unreachable there. DUNGEON route: shares the 0x39fc census (DUNGEON.OVL 0x0d32, 0x0fa8) but I did not trace `[bp-0xe]` into 0x1014; "probable" only. |
| 4 | Turn cost in town and dungeon | MAINOUT only | MAINOUT only | Town now traced (2.6): `U`/`C` result 1 -> TOWN 0x159a..0x15d4 -> kernel 0x39fc (party-alive check) -> `advance_clock(1)`-style call `0x4f7c` with arg 1. Dungeon consumer: result stored in `[bp-2]` at DUNGEON 0x0771 and returned at 0x070a; the consumer of that return value NOT traced. |
| 5 | Is the quoted brief's overlay base table usable? | CAST 0xbf80, COMBAT 0xa290 etc. (brief wrong) | same | **Re-fitted independently** (2.0): the brief's CAST 0xA8D8, COMBAT 0xBFEC, BLCKTHRN 0xE63E, DUNGEON 0xE1E0, COMSUBS 0x85FE are all wrong for those files. The shipped `re/tools/callers_banda.py 0x7ef6` prints ONLY `SHOPPES.OVL 0x16f5` (verified): it misses three of the four callers. |
| 6 | "No RNG draw" | none in the routines | none in the routines | True for the routines themselves. **Qualification both missed:** the "On who:" picker waits through the common key poll `0x266c`, which (when no key is pending and `g_location` < 0x21 or > 0x7f) calls `0x5910`, whose callees `0x4552` and `0x2f62` contain RNG calls (2.8). That idle hook is the same for every key wait in the game, is modelled by no port in any prompt, and is not specific to this item. |
| 7 | Ceremony (CAST2:0x0000) | plays only on a successful revive: 8 (spell), 6 (scroll) | same | Re-disassembled (2.2): CAST2:0x0000 takes its index at `[bp+4]` (`cmp word [bp+4],9`); the call at 0x06d4 is reached only on the success path. Adjacent divergence, listed in 7.3, not conflated with the text fix. |

No material disagreement between A and B on the core claim. A and B's only differences were omissions (B: device cancel
refund and the ceremony table detail; A: arena turn caveat wording). Both flagged the same unresolved items.

### 0.2 The three agreed claims I tried hardest to refute

1. **"The spell pushes flag 0, the scroll flag 1, and `Not dead!` is gated on flag != 0."** Re-disassembled
   CAST2.OVL 0x05e0-0x0616, CAST.OVL 0x10ec-0x10fc and 0x12d8-0x12f7. Caller stack layout independently confirmed: the
   routine indexes the roster with `[bp+6]<<5` and the healer pushes `idx` then `0xff` (SHOPPES 0x16f1), so `[bp+4]` is the
   LAST pushed argument = the flag. Both arms push `idx` first. Confirmed, no refutation.
2. **"A living target on the scroll prints `Not dead!` and THEN `Failed!`."** The second line comes from a different
   routine: `[bp-2]` of the scroll reader is overwritten with the return of `0x05e0` (0) at 0x12f1; the reader returns
   `[bp-2]` at 0x1350; (U)se does `mov [bp-0xa],ax` at 0x1819 then `jmp 0x1b8a`; `cmp [bp-0xa],0 / jne` at 0x1b8a-0x1b8e falls
   into `push 0x4a7b / call print` at 0x1b90-0x1b94. Checked that no other instruction between 0x1792 and 0x1b8a on this
   path writes `[bp-0xa]` (the scroll case enters via `push [bp-0x12]; call 0x11de` at 0x1813-0x1816 and goes straight to 0x1819).
   Confirmed, and the spell is different: its tail (0x11a6-0x11d6) is the Cast tail, `[bp-6]` is written only at 0xdc4
   and 0x1138 (arm 46, not 42). Confirmed.
3. **"Exactly four callers of thunk 0x7ef6; COMBAT.OVL never reaches the routine or the string; the arena gates it out."**
   Own base fit for every overlay (2.0), own census over EVERY E8/E9 at every byte offset of ULTIMA.EXE and all 22 overlays,
   run both with the fitted base and with every candidate base from the brief and the thunk table. Positive controls
   run. Same four callers, no more. The arena premise (`g_location` = 0xff) re-read at ULTIMA.EXE 0x5fa8-0x5fb4 and the
   restore at 0x6091-0x6094. Confirmed.

---------------------------------------------------------------------------------------------------

## 1. Verdict

* The section 11.15 line in `native/targets/tdeck/ALPHA4_UI.md` (2961, 3342) says "In Mani Corp's **scroll** on a living
  target should print `Not dead!` (DS 0x953c)". It is CORRECT but INCOMPLETE. The task brief ("the resurrection spell, and
  its scroll") is WRONG about the spell.
* **Scroll** (U)se -> In Mani Corp (scroll index 6), living target (any status byte != 'D'): the original prints
  `Resurrection!` (already ported), then `On who: <name>`, then `Not dead!` (DS 0x953c), then, from the (U)se epilogue,
  `Failed!` (DS 0x4a7b) and plays a glide tone. The scroll was consumed first (0x11ec). A turn is taken (overworld: yes,
  traced; town: yes, traced; dungeon: not traced). Zero RNG draws in the routines.
* **Spell** (C)ast -> In Mani Corp (index 42), living target: `resurrect_apply` is called with flag 0 and is SILENT. The Cast
  tail prints `Failed!` (DS 0x4660) plus the same glide tone. No `Not dead!`. Mixed-spell count and 8 MP are already spent.
  Both ports already print `Failed!` here (TS, native) and have a TS test pinning that no "Not dead!" appears.
* **Arena**: no. COMBAT.OVL has no private resurrect, never references DS 0x953c, never calls thunk 0x7ef6. In the arena
  `g_location` = 0xff, so the spell stops at the location gate with `Not here!` (nothing consumed) and the scroll prints
  `Resurrection!` then `Not here!` and is still consumed. Both ports already do exactly that.
* **Wrong things**: (a) TS reference `main.ts` scroll branch (discards the boolean, prints nothing); (b) native
  `world_magic.cpp` case 6 (same); (c) the replica in `native/core/tools/check-gameplay.ts` line 75 (same, and is the oracle
  of the `gameplay_parity` test); (d) the notes `re/notes/potions-scrolls.md` row 6 ("selChar + applyResurrect + sfx",
  incomplete) and the section 11.15 sentence; (e) `callers_banda.py` BASES. `re/notes/magic.md` line 160 (`mode!=0: "Not
  dead!"`) is correct.
* Not wrong, do not touch: `applyResurrect` / `apply_target_spell(Resurrect)` bodies, `useScroll.ts` `readScroll`, the spell
  paths, the arena scroll path, `combat.cpp` 1160.

---------------------------------------------------------------------------------------------------

## 2. Binary facts (all re-disassembled in this session unless marked)

### 2.0 Tooling controls and overlay bases

Own fit (`mani_f/census_f.py`): for every candidate base 0x7000..0xFFFE step 2, count E8 near calls whose resolved target
is a kernel `55 8b ec` prologue or a thunk record start (`9a ec 02 2e 07`). Best base / calls landing of calls:

| overlay | base | landing / calls | next best |
|---|---|---|---|
| CAST.OVL | **0xBF80** | 270 / 323 | 124 (brief's 0xA8D8 is not near the top) |
| CAST2.OVL | **0xE1E0** | 186 / 216 | 69 |
| COMBAT.OVL | **0xA290** | 160 / 210 | 78 (brief's 0xBFEC: not near the top) |
| BLCKTHRN.OVL | **0xA290** | 132 / 159 | 60 (brief's 0xE63E: not near) |
| SHOPPES.OVL | 0xA290 | 228 / 301 | 115 |
| TOWN.OVL, MAINOUT.OVL | 0x81D0 | 151 / 216, 197 / 270 | |
| DUNGEON.OVL | 0x81D0 | 187 / 263 | (brief's 0xE1E0 wrong) |
| COMSUBS.OVL | 0xE1E0 | 95 / 146 | (brief's 0x85FE wrong) |

Direct landing checks (all by hand): CAST.OVL `0x10f3 call 0xffffbf76` -> (0xbf76+0xBF80)&0xFFFF = **0x7ef6** (thunk record
`ULTIMA.EXE 0x7ef6: 9a ec 02 2e 07 | 12 00 | ea c0 e7 00 00` = overlay id 0x12, jump 0xe7c0; 0xe7c0 - 0xE1E0 = 0x05e0 =
CAST2.OVL prologue `55 8b ec 83 ec 08 56`). `0x58d0` -> (0x58d0+0xBF80)=0x1850 print (prologue `55 8b ec 83 ec 42 57 56`),
`0x842e` -> 0x43ae glide (`55 8b ec 83 ec 08 57 56`), `0x6980` -> 0x2900 (`55 8b ec 83 ec 02 56 b8`),
`call 0xffffc1aa` -> 0x812a (thunk -> CAST2:0x009e). CAST jump tables: spell table words at file 0x1146
(`jmp cs:[bx-0x2f3a]`), scroll table at file 0x1340 (`jmp cs:[bx-0x2d40]`); with base 0xBF80 these give entry 42 =
0xd06c -> file **0x10ec**, scroll 6 = 0xd258 -> file **0x12d8**. Both are what the notes say, which independently
validates the base.

Positive controls for the census (`mani_f/census2_f.py`): `0x7e5a` -> COMBAT.OVL 0x095e and ULTIMA.EXE 0x3249 only;
`0x7e42` -> COMBAT.OVL 0x059e and ULTIMA.EXE 0x3413 only; print `0x1850` -> 1014 call sites over all files (fit bases).
`re/tools/callers_banda.py 0x7ef6` printed only `SHOPPES.OVL 0x16f5 call 0xdc66`: the shipped tool is wrong for CAST,
CAST2, COMBAT, BLCKTHRN.

### 2.1 Strings (DATA.OVL file offset = DS + 0x10)

`0x953c` `"Not dead!\n"` (10 bytes + NUL, file 0x954c); `0x94f4` `"On who: "`; `0x94fe` `"None!"`; `0x46d2`
`"Resurrection!\n"`; `0x46e1` `"Not here!\n"` (scroll 6 arena); `0x462f` `"Not here!\n"` (Cast gate); `0x466a`
`"Scroll\n\n"`; `0x4a7b` `"Failed!\n"` ((U)se tail); `0x4656` `"Success!\n"`; `0x4660` `"Failed!\n"` (Cast tail); `0x4603`
`"Spell name:\n:"`; `0x463a` `"None mixed!\n"`; `0x4647` `"M.P. too low!\n"`; `0xa24c` and `0x6e42` `"Use item\n\n"`;
`0x48b1` `"Item: "`; `0xa142` `"Cast...\n"`. Spell-name list at DS 0x6f9: index 42 = `In Mani Co(rp)`; flag table at DS 0x1c90
(file 0x1ca0): byte[42] = **0x0e**. One immediate reference to 0x953c in the whole install: `CAST2.OVL 0x060a b8 3c 95`.

### 2.2 `resurrect_apply` = CAST2.OVL:0x05e0 (`ret 4`; `[bp+6]` = roster index, `[bp+4]` = flag word)

```
05e7 837e0600  cmp word ptr [bp+6],0 ; 05eb 7d09 jge 05f6        ; idx<0 -> [bp-6]=0xffff, jmp 06e1 (returns -1)
05f6 8b5e06 b105 d3e3  bx=[bp+6]<<5
05fd 80bfb35544 cmp byte ptr [bx+0x55b3],0x44 ; 0602 7416 je 061a  ; status byte == 'D' (byte compare, only 'D')
0604 837e0400  cmp word ptr [bp+4],0 ; 0608 7407 je 0611          ; flag == 0 -> SILENT (word compare)
060a b83c95    mov ax,0x953c ; 060d 50 push ax ; 060e e85f30 call 0x3670   ; print_string("Not dead!\n")  (0x3670+0xE1E0 = 0x1850)
0611 c746fa0000 mov word ptr [bp-6],0 ; 0616 e9c800 jmp 06e1       ; result 0
061a..  dead path: [bx+0xb]=0x47 'G'; [bx+0x10]=1 (HP); class [bx+0xa]: 0x41/0x4d -> [bx+0xf]=[bx+0xe]; 0x42 -> [bx+0xf]=[bx+0xe]>>1;
        if karma byte [0x5888] < 0x62 (064e cmp / 0653 jae 0678) -> [bx+0x14] = long_div(long_mul(sext(exp), zext(karma)), 100) (kernel 0x2262 / 0x22b6)
        level loop: [bp-8]=1; cx=exp/100 (cdq; idiv 100); while(cx>0){dx++; cx>>=1 (sar)}; [bx+0x16]=dl; [bx+0x12]=0x1e*dx
06be 837e0401 cmp word ptr [bp+4],1 ; jne 06ca ; ax=6 ; jmp 06d3   ; flag 1 -> ceremony index 6
06ca 837e0400 cmp word ptr [bp+4],0 ; jne 06d7 ; 06d0 ax=8        ; flag 0 -> index 8 ; flag 0xff -> none
06d3 50 push ax ; 06d4 e829f9 call 0x0000                          ; CAST2:0x0000 ceremony(index) -- SUCCESS PATH ONLY
06d7 c746fa0100 mov word ptr [bp-6],1 ; 06dc c606faa901 mov byte [0xa9fa],1 ; 06e1 mov ax,[bp-6] ; ret 4
```
Returns -1 (idx<0), 0 (not dead), 1 (revived). No RNG call, no clock call and no other state write on the not-dead
path (only the print). The level word loop, 30*level and MP rules match TS and native for every exp that fits a signed
word (both ports treat exp as unsigned; difference only above 32767, unreachable).

### 2.3 Callers of `resurrect_apply` (thunk 0x7ef6) and the flag each pushes -- exhaustive

| caller | pushes | flag | note |
|---|---|---|---|
| CAST.OVL 0x10f3 (spell arm 42) | `10ec call 0xffffc1aa; 10ef push ax; 10f0 sub ax,ax; 10f2 push ax; 10f3 call 0xffffbf76` | **0** | then `10f6 mov [bp-0xa],ax; 10f9 call 0x6980; 10fc jmp 0x11a6` |
| CAST.OVL 0x12ee (scroll 6) | `12e6 call 0xffffc1aa; 12e9 push ax; 12ea mov ax,1; 12ed push ax; 12ee call 0xffffbf76` | **1** | then `12f1 mov [bp-2],ax; 12f4 call 0x6980; 12f7 jmp 0x1350` |
| SHOPPES.OVL 0x16f5 (healer) | `push [bp-6]; mov ax,0xff; push ax` | 0xff | guarded by its own `cmp byte [bx+0x55b3],0x44 / je` at 0x16b5; "Not dead!" unreachable |
| BLCKTHRN.OVL 0x0b95 (Refuge) | `0b90 push si; 0b91 mov ax,0xff; 0b94 push ax; 0b95 call 0xffffdc66` | 0xff | see 2.7 |

ULTIMA.EXE, COMBAT.OVL and CAST2.OVL (intra-overlay) contain no call; no `0xe7c0`/`0x7ef6` data word anywhere except
the thunk record itself. Other relevant censuses (same tool, fitted bases): CAST.OVL 0x11de scroll reader: CAST.OVL
0x1816 only; thunk 0x7e42 (Use item): ULTIMA.EXE 0x3413 and COMBAT.OVL 0x059e; thunk 0x7e5a (Cast): ULTIMA.EXE 0x3249 and
COMBAT.OVL 0x095e; kernel 0x3178 (command dispatcher): MAINOUT 0x0c00, TOWN 0x158f, DUNGEON 0x07a3; kernel 0x5f86
(arena entry): ULTIMA.EXE 0x0e5d, 0x0e69 (+0x6347 per A, not re-checked) and DUNGEON.OVL 0x0c53, 0x1db5 (A, not
re-checked); DUNGEON.OVL 0x0000 sets 0xff by itself (A, not re-checked).

### 2.4 Spell path (CAST.OVL 0x0dba, thunk 0x7e5a)

Entry: `0dc1 mov ax,1; 0dc4 mov [bp-6],ax; 0dc7 mov [0x588f],al; ...; 0dcf [bp-0xc]=1; 0dd2 [bp-0xa]=1`. Caster select
`0dd5 call 0x8a08` (K 0x4988). `Spell name:` then CAST2:0x00de (thunk 0x808e). Circle: `0e0a..0e14: [bp-8] = idx/6 + 1`
(signed idiv by 6) = 8 for 42. Location gate 0x0e1a-0x0e95: `[0x5893]==0` -> `test [bx+0x1c90],8`; `>0x7f` -> `test ..,1`;
`==0x12 (with [0x57b4]==0) or ==0x1d` -> "Absorbed!" block (0x0e53..0x0e71, tone_sweep 0x6212 -> K 0x2192, jmp 0x11d9);
`<0x21` -> bit 4; else bit 2. Flag 0x0e has bits 8,4,2 and not 1. Refusal 0x0e9b: print 0x462f `Not here!`, glide
`push 0x320,0x7d0,1,0x32; call 0x842e` (0x0ea2-0x0eb2), `jmp 0x11d9`: **nothing consumed**. Then: `0eb8 cmp byte [idx+0x57f0],0`
-> "None mixed!" else `0ec8 dec byte [idx+0x57f0]` (mixed count spent); `0ed3` MP byte `[caster<<5+0x55b7]` < circle ->
"M.P. too low!" (0x4647), `[bp-0xa]=0`, jmp 0x11a6; `0ef8 sub [si+0x55b7],al` (MP spent); `0efc [bp-0xa]=0xffff`; level byte
(+0x16, `0x55be`) < circle -> `[bp-0xa]=0` (tail "Failed!"); `0f0f..0f1a` bounds `cmp ax,0x2f` and `jmp cs:[bx-0x2f3a]`.
Arm 42 = 2.3 row 1. **Tail** 0x11a6: `cmp [bp-0xa],1 / jne` -> `push 0x4656; call 0x58d0` ("Success!"); 0x11b6 `cmp [bp-0xa],0 /
jne 0x11d6` -> `push 0x4660; call 0x58d0` ("Failed!") then the glide 0x11c3-0x11d3; 0x11d6 `mov ax,[bp-6]` (= 1; the only writers
of `[bp-6]` in 0x0dba-0x11de are 0x0dc4 and 0x1138, the latter belongs to arm 46 at 0x112e). Result -1 (cancelled picker)
prints nothing and the routine still returns 1. Living target transcript: `Cast...` / `Spell name:` / `On who: <Name>` /
`Failed!` + glide; mixed -1, MP -8, no ceremony. The `jmp 0x11d9` refusal exits return the glide's AX, not `[bp-6]` (the
turn consequence of a REFUSED cast is outside this item).

### 2.5 Scroll path (CAST.OVL 0x1792 (U)se -> 0x11de scroll reader)

(U)se 0x1792: `179a [bp-0xa]=1`; picker; `1813 push [bp-0x12]; 1816 call 0x11de; 1819 mov [bp-0xa],ax; 181c jmp 0x1b8a`. Reader:
`11e4 [bp-2]=1; 11ec dec byte [bx+0x5820]` (scroll consumed first, unconditionally) `; 11f0 print 0x466a "Scroll\n\n"`;
`11f7 cmp ax,7 / ja 0x1350`; `1205 jmp cs:[bx-0x2d40]`. Scroll 6 = 0x12d8: `print 0x46d2; 12df cmp byte [0x5893],0x80; 12e4 jae 0x12fa`;
`12e6 call 0xffffc1aa` (CAST2:0x009e: prints `On who: `, K 0x4cae picker, name at `[idx<<5+0x55a8]` or `None!` on -1, conditional LF);
push idx; push 1; `call 0xffffbf76`; `[bp-2]=ax`; `call 0x6980`; `jmp 0x1350` (`mov ax,[bp-2]; ret 2`). Arena: `12fa mov ax,0x46e1;
jmp 0x1289; 1289 push ax; 128a call 0x58d0; 128d jmp 0x1350` (result stays 1). (U)se epilogue 0x1b8a: `cmp word [bp-0xa],0; jne 0x1baa;
push 0x4a7b; call 0x58d0; push 0x320; push 0x7d0; push 1; push 0x32; call 0x842e` (glide 0x1b97-0x1ba7). Result table:

| situation | printed after `Resurrection!` | scroll | result |
|---|---|---|---|
| living target (status != 'D') | `On who: <Name>` `Not dead!` then `Failed!` + glide | consumed | 0 |
| dead ('D') | `On who: <Name>`, ceremony(6), panel redraw, nothing else | consumed | 1 |
| picker cancelled | `On who: None!` only | consumed | -1 (no Failed!) |
| arena (`[0x5893]` >= 0x80) | `Not here!` (no picker) | consumed | 1 (no Failed!) |

### 2.6 Turn cost

Kernel dispatcher 0x3178 starts `317e mov word [bp-2],1`. Arm `U` 0x340c: `push 0xa24c; call 0x1850; 3413 call 0x7e42; 3416 jmp 0x31ee`
(does not touch `[bp-2]`, returns 1). Arm `C` 0x3242: `push 0xa142; call 0x1850; 3249 call 0x7e5a; 324c jmp 0x3231; 3231 mov [bp-2],ax`
(returns the cast routine's `[bp-6]` = 1 on the In Mani Corp path). MAINOUT 0x0c0f-0x0c3d: `mov [bp-8],ax; ... cmp byte [0x5893],0 / je 0x0c30;
0x0c30 cmp word [bp-8],0 / jne 0x0c39; 0x0c39 push 2; call 0xffffcdac` (= K 0x4f7c). TOWN 0x159a-0x15d4: `mov [bp-0xa],ax; cmp [bp-0xa],3`;
location==0 branch; `cmp [bp-0xa],0 / jne`; `call 0xffffb82c` (= K 0x39fc, party alive/dead check) `/ inc ax / jne 0x15c8`; `push 1; call 0xffffcdac`
(K 0x4f7c with 1). Net: spell (any outcome) and scroll (any outcome) each spend a world turn on foot and in a town (as long as the party is
not wiped, which cannot be true during a cast). Dungeon consumer not traced. The ports document that (U)se takes no turn (`commands.cpp`
1074-1076, `re/notes/use-merchants.md`); that is a separate question and is NOT part of this fix.

### 2.7 Refuge (flag 0xff) -- is "Not dead!" reachable?

BLCKTHRN.OVL 0x0b54-0x0bb1: loop `si = 0 .. [0x585b]-1`, `push si; mov ax,0xff; push ax; call 0xffffdc66`, then `0b98 mov bx,[bp-0x10]
(0x55ba + 0x20*si); mov ax,[bx]; mov [di],ax` (HP = max HP, `di` = 0x55b8 + 0x20*si). Entry: MAINOUT 0x0aa2 `call 0xffffb82c` (K 0x39fc) ->
`[bp-6]`; `==1` message branch; `==-1` (0x0ac2) -> ... `0x0af0 call 0xffff8cac` (= stub K 0x0e7c -> `call 0x7a5e` -> BLCKTHRN 0x0910 via thunk
`0x7a5e -> target 0xaba0 = 0xa290+0x910`). TOWN 0x1436 `call K 0x39fc; cmp ax,1; ...; cmp [bp-8],-1; jne; jmp 0x1862 -> 0x1865 call 0xfffff88e`
(= thunk 0x7a5e). Kernel 0x39fc: loops `[0x585b]` members; status 'G' (0x47) or 'P' (0x50) -> returns 0 at once; counts 'S' (0x53); end:
count != 0 -> returns 1, else returns **-1**. So the Refuge is entered only when no member is G/P/S, i.e. all are 'D'. `Not dead!` is not
reachable there (with the game's four status bytes). DUNGEON.OVL 0x1014 (`cmp [bp-0xe],0; jge 0x1017; call 0x8cac`) is a third entry whose
value probably comes from the same 0x39fc call at DUNGEON 0x0fa8: not traced.

### 2.8 RNG

No RNG call (K 0x2092 rand core, 0x3aae, 0x3abe) in CAST2 0x05e0-0x06e8, CAST2 0x009e-0x00dc, CAST2 0x0000-0x009c range used here, CAST.OVL
0x10ec-0x10fc, 0x11a6-0x11d8, 0x11de-0x1356, 0x1b8a-0x1baf. `rngclosure.py` (direct-near-call closure inside the kernel, over-reporting):
print K 0x1850 (5 functions), glide K 0x43ae (3), redraw K 0x2900 (17), putchar K 0x16ba, K 0x1f12: zero RNG calls. The picker K 0x2e8e (58 functions
by the over-reporting closure) reaches RNG callers through exactly one chain: `0x2e8e -> 0x2d7a (0x2e91) -> 0x266c (0x2dca) -> 0x5910 (0x269a) -> 0x4552
(0x5941) / 0x2f62 (0x5944)`. `0x266c` is the common key poll: `267f call 0x1b38; call 0x2032; or si,si; jne 0x269d; ...; 268c cmp [0x5893],0x21 /
jb; 2693 cmp ..,0x7f / jbe 0x269d; 269a call 0x5910`. That idle hook runs while the picker waits for a key when `g_location` < 0x21 or > 0x7f, and is
identical for every prompt of the game. It is not modelled by either port for any prompt, so it does not belong to this item; the correct statement is
"0 RNG draws from the resurrect/scroll/tail code; the shared key-wait idle hook is out of scope".

### 2.9 Arena

Arena Cast: COMBAT.OVL 0x095e -> thunk 0x7e5a = the SAME CAST.OVL 0x0dba. Arena Use: COMBAT.OVL 0x059e -> thunk 0x7e42 = the SAME CAST.OVL 0x1792.
ULTIMA.EXE 0x5f86 (arena entry): `5fa8 mov al,[0x5893]; 5fab mov [0x5894],al; 5fb4 mov byte [0x5893],0xff`; restore `6091 mov al,[0x5894];
6094 mov [0x5893],al`. So in the arena the Cast gate tests bit 1 (flag 0x0e: clear -> `Not here!` + glide, nothing consumed) and the scroll reader's
`cmp [0x5893],0x80` is true. COMBAT.OVL byte-scan for DS 0x953c: none; census of thunk 0x7ef6: none.

---------------------------------------------------------------------------------------------------

## 3. Reference (TypeScript): exact change needed

Only the world/dungeon (U)se scroll branch. All line numbers are the current working tree.

* `game/src/main.ts` **4879-4883** (inside the `a.kind === "scroll"` block that starts at 4856):
  ```
  } else if (fu.kind === "resurrect") {
    // In Mani Corp: elige PJ y resucita (sin Success/Failed; el eco es "Resurrection!").
    pickCastTarget((i) => {
      applyResurrect(game.state.characters[i]!, game.state.karma);
    });
  }
  ```
  Change the callback to use the boolean and, on `false`, emit two separate rows, in this order:
  `if (!applyResurrect(game.state.characters[i]!, game.state.karma)) { hud.message("Not dead!"); hud.message("Failed!"); }`
  (a success prints nothing, as today). Correct the comment (4880): the scroll reader pushes flag 1 (CAST.OVL 0x12ea), so `Not dead!`
  (DS 0x953c, CAST2.OVL 0x060a) is printed by `resurrect_apply`, and `Failed!` (DS 0x4a7b) by the (U)se epilogue CAST.OVL 0x1b90 because the
  reader returns 0. Constraints from existing tests: the new code must NOT contain the text `"Success!" : "Failed!"` (see section 5), and the block must
  keep `fu.kind === "resurrect"` and `applyResurrect`. A cancelled picker already calls the cancel callback only (`pickers.ts` 199-209: `None!` appended,
  callback not run): correct, no "Failed!" (the binary returns -1).
* Do NOT change `useScroll.ts` 90-96 (`readScroll` case 6): the target is not known there, and its tests pin `["Resurrection!"]`.
* Do NOT change `cast.ts` 545-561 `applyResurrect` (state transition correct; its boolean already encodes `status === 'D'`). Do NOT change the Cast branches
  (`main.ts` 4448-4474, 4612-4621, 3316-3345): they print `Success!`/`Failed!` and no `Not dead!`, which is the binary.
* `game/tests/fixtures/approved-strings.json`: add key `"Not dead!"` with a `[D]` citation (`DS 0x953c file 0x954c, CAST2.OVL 0x060a, flag != 0 only; scroll
  CAST.OVL 0x12ea`). The string extractor (`game/tools/extract-user-strings.mjs`) treats `hud.message(...)` as a sink, so `string-manifest.test.ts` goes red
  without it. `"Failed!"` is already there (line 417). (I did not run the guards.)
* Mirror replica: `native/core/tools/check-gameplay.ts` **line 75** (`else if(r.followup.kind==='resurrect'&&s.characters[a.member])applyResurrect(s.characters[a.member],s.karma);`)
  must become `...&&s.characters[a.member]){if(!applyResurrect(s.characters[a.member],s.karma)){msg('Not dead!');msg('Failed!');}}`, keeping the following
  `else if(r.followup.kind==='reveal')reveal();` and `cue(...)` unchanged (scroll 6 has no ceremony index in both ports, see 7.3).

## 4. Native: exact change needed

* `native/core/src/world_magic.cpp` **line 35**:
  `case 6:say("Resurrection!");if(auto *p=target())apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand);break;`
  -> `case 6:say("Resurrection!");if(auto *p=target()){if(!apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand)){say("Not dead!");say("Failed!");}}break;`
  `apply_target_spell(Resurrect)` returns false exactly when `status != 'D'` and touches nothing then (`magic.cpp` 231-247), so the boolean is the faithful
  discriminant (byte compare against 0x44, like the binary); do not add a second status test. Text without a trailing `\n` (the ports' convention).
  `target()` (line 14) already returns null for `member < 0` or `>= character_count`, which equals the binary's idx < 0 (silent). Line 27 (consume first) and
  line 37 (`indices[8]`, ceremony index -1 for 6, so no ceremony/Sfx event on the failure path) stay untouched. This function is the single native producer for
  world, town (the `commands.cpp:1078` route) and dungeon (`dungeon_orchestration.cpp:158-160`) scrolls, so one edit covers all three.
* Do NOT touch `combat.cpp` 1160 (`case 6:say("Resurrection!");say("Not here!");break;` is the binary), `magic.cpp`, `world_magic.cpp:64` (spell; correct), nor
  the failure sound: emit no Sfx (the glides at CAST.OVL 0x11d3 / 0x1ba7 stay `EvidenceUnknown` in `sfx_inventory.cpp:126,133`; adding one is a separate audio
  decision; `invalid-magic` is in `nativeOnlyAudioHooks`, so even that would not fail `gameplay_parity`, but it is not part of this fix).
* Device wiring needs no change for the text (`alpha_runtime.cpp:1395-1396`, `1626` list every party member enabled, dead included).

---------------------------------------------------------------------------------------------------

## 5. Fixtures, corpora and tests

**Must change in the same commit (or RED)**
* `native/core/tools/check-gameplay.ts` line 75 (the TS replica, see 3): test `gameplay_parity` (CMake line 3006) compares native vs this replica live (no committed
  fixture). Line 111 runs scroll item 6 (`for item 0..15, seed 0..15`, `characters[0].status = ['G','P','S','D'][seed%4]`, member 0, or -1 every 5th seed): seeds with
  G/P/S and member 0 gain exactly the two messages `Not dead!`, `Failed!` after `Resurrection!`; 'D' and member -1 unchanged. Native and replica must change together.
  Event kinds are compared with `isDeepStrictEqual` after filtering native-only sfx ids.
* `game/tests/fixtures/approved-strings.json` (add "Not dead!").
* Docs to refresh: `native/targets/tdeck/ALPHA4_UI.md` 2961 and 3342 (say scroll AND `Failed!` + tone, not the spell); `re/notes/potions-scrolls.md` row 43
  (6 = selChar + resurrect_apply flag 1: `Not dead!` + `Failed!`); `ALPHA2_HARDWARE_CHECKLIST.md` H-46 and `GAMEPLAY_INTEGRATION_AUDIT.md` ~4154 transcript
  (a living pick now shows `Resurrection!` / `Not dead!` / `Failed!`).

**Must stay byte-identical (and will, if the fix is at the caller layer)**
* `native/core/fixtures/magic.txt` + `magic_parity_test` + `tools/generate-magic-fixtures.ts` + `game/src/core/__parity__/magic-run.ts:163`: they drive
  `cast_spell` and `apply_target_spell` / `applyResurrect` directly (booleans, state, RNG). Do NOT turn the boolean into a tri-state or move the message
  into `applyResurrect`/`apply_target_spell`.
* `native/core/tests/a4_enh2_preservation_test.cpp` B4 (`kResurrection = 0xbe90d6bb09040045`) pins `apply_target_spell` only.
* `fixtures/items.txt`, `commands.txt`, `items-coverage.json`, `commands-coverage.json`, `combat-*.json`, `advanced-combat*`: no scroll-6 / Cast rows or arena only.
* `game/tests/use-scroll.test.ts` 79-88 (`readScroll(...,6,0)` = `["Resurrection!"]` + followup; arena pair): keep.
* `game/tests/cast-onwho-consumidores.test.ts` 151-167 (the CAST branches contain `"Success!" : "Failed!"`, `applyResurrect(`, `pickCastTarget(`, and not `Not dead!` /
  `Resurrection!`) and 171-175 (`USE_SCROLL` = source between `hud.messageAppend("Scroll")` and `if (a.kind === "potion") {` must contain `fu.kind === "resurrect"` and
  `applyResurrect` and must NOT contain `"Success!" : "Failed!"`): these constrain the shape of the TS edit, they do not need editing. Note that the test comment at 169-170
  ("sin Success!/Failed!") becomes slightly stale wording; the assertion is still true.
* `native/core/tests/batch13_test.cpp`: A2 (284-291, all eight scrolls, living member 0: first line `Scroll`, no `Used ` message) and D1 (419-426, ceremony index
  == `kWorldScrollCeremony[i]`, = -1 for 6 with the living test member) stay green; E3/C1 arena groups unaffected. They do not assert the trailing messages, so a
  RED test must be ADDED (6).
* `native/core/tests/batch5_test.cpp` (`cast.item = 6` there is SPELL 6, not scroll 6) and `batch18_well_ceremony_test.cpp` (UseTarget flow) do not touch it.
* `a3_03_sfx_inventory_test` + `sfx_inventory.cpp` 126/133: unchanged for a text-only fix.
* No fixture, corpus, recorded footage or capture in the repository exercises a living target for either path (`videocap` takes use a dead Shamino and are
  port recordings). So nothing in the tree is an oracle for this behaviour except the binary.

---------------------------------------------------------------------------------------------------

## 6. Test plan

All host tests below use the batch13 `World` fixture style (`World::use(item, member)`, `trace()` = `m:text|s:id|c:index`) with a counting `Rand` instead of
`steady_rand()` for the draw count; TS equivalents go in `game/tests/` with a `hud` spy over the main.ts branch, or at the `check-gameplay.ts` level.

RED-first (fails today, green after the fix):
1. World, scroll 6, member 0 status 'G': trace == `m:Scroll|m:Resurrection!|m:Not dead!|m:Failed!` exactly (no `s:`, no `c:`); `scroll_quantities[6]` 2 -> 1;
   character record byte-identical (HP, MP, exp, level, max HP, status, class, INT); RNG draws == 0.
2. Same for 'P' and for 'S', and for status bytes that are not G/P/S/D (e.g. 0x00, 'X'): all non-'D' print the two lines (the binary compares only against 0x44).
3. Dungeon route (location 0x21..0x7f in `dungeon_orchestration` -> `world_magic`) and a town location: identical trace.
4. TS: the branch with a 'G' target emits `["Not dead!","Failed!"]` rows after the picker callback; `gameplay_parity` seeds 0..15 for item 6 go red on the native side
   (or TS side) until both change together.

Pins that must stay green (guards; they pass before and after, they catch the WRONG fix):
5. 'D' target: trace == `m:Scroll|m:Resurrection!` (no `Not dead!`, `Failed!`, `Success!`, no ceremony); status 'G', HP 1; karma boundary: exp 1000 with karma 97, 98, 99,
   0 (cut only when karma < 98, i.e. byte compare `cmp [0x5888],0x62 / jae`), exp 0 -> level 1, max HP 30; classes A/M MP = INT, B = INT>>1, other untouched.
6. Cancelled/absent target (`member = -1`, `member >= character_count`): `Scroll`, `Resurrection!` only; scroll 2 -> 1 (binary: consumed first, 0x11ec).
7. Arena scroll (combat path, `combat_use_consumable`): `Scroll`, `Resurrection!`, `Not here!`; consumed; no `Not dead!`/`Failed!`.
8. Spell 42 on a living target outside the arena with mixed >= 1, MP >= 8, level >= 8: messages end `Failed!` with NO `Not dead!`; mixed -1; MP -8; character unchanged
   (note: the ports emit the circle-8 ceremony here before the apply, the binary does not; assert current behaviour and name the divergence in the test comment).
9. Spell 42 in the arena: `Not here!`, mixed and MP untouched. Spell 42 with caster level < 8: `Failed!`, mix and MP spent (existing).
10. Order assertion: `Resurrection!` < `Not dead!` < `Failed!` (swap guards).

Mutations the suite must kill: (a) print only `Failed!`; (b) print `Not dead!` only; (c) swap the two lines; (d) print on 'D'; (e) treat only 'G' as not-dead (P/S must print);
(f) print when `member < 0`; (g) move the message into `apply_target_spell` / `applyResurrect` (kills `magic.txt` and TS generator, so it must be rejected by the fixtures,
not by a new test); (h) also print `Not dead!` on the Cast path (case 8 kills it); (i) print in the arena (case 7); (j) refund the scroll on a non-'D' target (cases 1-2 check
the quantity); (k) draw RNG on the failure path.

Boundary values: status 'D' (0x44) vs 0x43/0x45; karma 97/98 (0x62); exp 0, 99, 100, 199, 200, 9999; member -1, 0, last, last+1.

---------------------------------------------------------------------------------------------------

## 7. Residual unknowns and adjacent divergences (do not fold into this fix)

7.1 **Dungeon turn consumer.** `U`/`C` returns 1 through DUNGEON.OVL 0x07a3 -> 0x0771 `[bp-2]=ax` -> 0x070a `mov ax,[bp-2]` and the function's `ret 2`; the caller that
turns it into a clock tick was not traced. Needed: callers of the DUNGEON.OVL function that ends at 0x07de and what they do with AX (`push 2`/`push 1` + K 0x4f7c).
Overworld (`push 2; call 0xffffcdac`, K 0x4f7c) and town (`push 1; call 0xffffcdac`) are traced. I did not decode K 0x4f7c itself (the note calls it advance_clock).

7.2 **Refuge via the DUNGEON route.** Probably guarded by the same K 0x39fc call (DUNGEON.OVL 0x0fa8 vs 0x1014 `[bp-0xe]`), not traced. For MAINOUT and TOWN it is settled
(all members 'D').

7.3 **Adjacent, real, binary-proven, not text: ceremony and sound.** (i) `resurrect_apply` plays CAST2:0x0000 itself, index 6 for the scroll and 8 for the spell, only on a
successful revive (0x06be-0x06d4). Both ports claim scroll 6 has "sin ceremonia" (`ceremony.ts:123`, `world_magic.cpp:37` `indices[6]=-1`, `batch13_test` `kWorldScrollCeremony`) because
the CAST.OVL handler body 0x12d8-0x12f7 has no call; the callee does. The ports also play the spell's circle-8 ceremony BEFORE and regardless of the result (`world_magic.cpp:47`,
`main.ts` generic `emitCastCeremony`), including on a living target or a cancelled picker. Settling the presentation side needs original footage or an emulated run of CAST2:0x0000
(index 6 vs 8); whether the scroll's flash is visible on the original is not established by the binary alone (the code path is certain, the visual effect is not measured here).
(ii) The glide `K 0x43ae(0x32,1,0x7d0,0x320)` follows every `Failed!` of both tails (0x11d3, 0x1ba7) and `Not here!` of the Cast gate (0x0eb2); audio decision, tracked as
`EvidenceUnknown` in `sfx_inventory.cpp`.

7.4 **Adjacent, real: device cancel refunds.** On the T-Deck a cancelled "On who:" pick spends nothing (scroll, mixed-spell charge, 8 MP), and the picker opens before the Cast gates
(`alpha_runtime.cpp` 1395, 1528-1533, modal 1375); the binary consumes the scroll at 0x11ec / the charge at 0x0ec8 and the MP at 0x0ef8 before the picker and evaluates every gate first.
(By code reading only; no run.)

7.5 **"On who: <Name>" echo.** The TS reference prints it (`pickers.ts` 199-209, cmd-strings 286-289); native never prints it (device `PartySelection`). Not touched by this item.

7.6 **Native (U)se takes no turn** (`commands.cpp` 1074-1076) while the binary does (2.6). Separate item; the MANI fix must not rely on it.

7.7 **Exp width.** The binary treats exp as a signed word (`cdq` + long mul/div), both ports as unsigned; identical for every reachable value (cap 9999).

7.8 **Idle RNG in the key wait** (2.8): `0x5910 -> 0x4552 / 0x2f62` draw RNG while a prompt waits for a key outside 0x21..0x7f. Not decoded (conditions, exact draws). Unmodelled by every
port for every prompt; affects exact RNG-stream parity of any prompt in the original, not this item.

7.9 Not re-checked by me (taken from A): ULTIMA.EXE 0x6347 and DUNGEON.OVL 0x0c53 / 0x1db5 as extra callers of the arena entry K 0x5f86, and DUNGEON.OVL 0x0000's own 0xff write at 0x00a8;
they only matter for "which arena entries have `g_location` = 0xff", and ULTIMA.EXE 0x0e5d/0x0e69 plus the K 0x5f86 body itself were re-read.
