# D-83 / D-84 -- resurrection parity: FINAL reconciled report

Reconciler. Inputs: `D83D84-A.md` (binary-first), `D83D84-B.md` (ports-first). Everything marked "I derived" below was disassembled or executed by me in this session from `original/u5/ultima5/*` (ULTIMA.EXE, CAST2.OVL, CAST.OVL, SHOPPES.OVL, BLCKTHRN.OVL, TALK.OVL, TOWN.OVL, MAINOUT.OVL, ENDGAME.OVL, DATA.OVL) with `re/tools/dis16.py` (capstone), plus my own scripts under `findings/d83f/` (`f_lib.py`, `f_table.py`, `f_basefit.py`, `f_thunks.py`, `f_census.py`, `f_karma_scan.py`, `f_reach.py`, `f_path.py`, `f_emu.py` = an independent 8086 interpreter that executes the real CAST2 and kernel bytes, `f_sweep.py`, `f_table_out.py`). The repository was not touched (`git status` shows only the four pre-existing untracked logs). `re/disasm/*.asm` does not exist; nothing here relies on it.

---------------------------------------------------------------------------------------------

## 1. Verdict

### 1.1 What the original does

1. There is ONE resurrection routine: **CAST2.OVL file offset 0x05e0** (`resurrect_apply(member, mode)`, Pascal, `ret 4`, both args words, pushed member then mode). The kernel reaches it through the overlay stub **ULTIMA.EXE CS 0x7ef6** (`9a ec 02 2e 07 | 12 00 | ea c0 e7 00 00`: overlay id 0x12 = CAST2, `ljmp 0:0xe7c0`, 0xe7c0 - 0xe1e0 = 0x05e0). The notes' "kernel 0xdc66" is the RAW near-call operand seen from the 0xa290 band: (0xdc66 + 0xa290) & 0xffff = 0x7ef6. It is not a kernel address.
2. **Exactly four callers** (my census over ULTIMA.EXE and all 22 overlays, every E8/E9 at every offset, correct band bases, plus every alternative base as an over-reporting net, plus literal-word scans, plus an intra-CAST2 scan): CAST.OVL 0x10f3 (spell 42 In Mani Corp, mode 0), CAST.OVL 0x12ee (scroll 6 In Mani Corp, mode 1), SHOPPES.OVL 0x16f5 (healer Resurrect, mode 0xff), BLCKTHRN.OVL 0x0b95 (Refuge revive loop, mode 0xff). No other reference exists.
3. **The healer and the Refuge DO call it.** After the call each one overwrites HP with the member's RECOMPUTED maximum (`SHOPPES 0x16f8-0x1703`, `BLCKTHRN 0x0b98-0x0b9d`). The routine itself leaves HP = 1 (so the spell and the scroll end at HP 1).
4. The routine, for a member whose status byte is 'D' (0x44): status 'G' (0x47); HP word = 1; MP byte by class ('A' and 'M' = INT, 'B' = INT >> 1, every other class byte untouched); **if karma (unsigned byte at DS:0x5888) < 0x62 (98): XP = low 16 bits of ((int32)(signed)XP * karma / 100)** (signed 32-bit multiply and truncating signed divide); then, for EVERY karma (also >= 98): **level byte = 1 + bitlength(XP / 100)** (signed `idiv`, loop, no table, no cap) and **max HP word = 30 * level**. No RNG, no karma write. Not 'D': nothing changes, returns 0 (prints DS:0x953c "Not dead!\n" only when mode != 0). Member < 0: returns 0xffff, nothing happens.
5. Per path after the routine: spell and scroll: HP 1. Healer and Refuge: HP = new max HP (30 * level). Status 'G'. At the Refuge the routine sees the karma the party DIED with: the floor of 75 is applied at 0x0bfd, after the revive loop.

### 1.2 Who is wrong

* **Both ports (reference and native) are wrong for the healer and the Refuge; both are right (byte-exact for XP <= 32767) for In Mani Corp.** TS `healerHeal` (`game/src/core/shops/shops.ts:1238-1241`) sets status 'G' and HP 1. TS `partyRefuge` (`game/src/core/world/blackthorn.ts:535-543`) sets HP = the STORED max and status 'G' for every member (even a non-'D' one). Native `healer_heal` (`native/core/src/shops.cpp:226-230`) is the same as TS healer; native `resolve_refuge` (`native/core/src/quest_world.cpp:189`) the same as TS Refuge. D-83 and D-84 in `ALPHA2_PRESERVATION_LEDGER.md` (rows 228, 229) are exact, including "with the karma the party died with, before the floor of 75".
* **Notes that are wrong or misleading about the BINARY:**
  * `native/targets/tdeck/ALPHA4_UI.md` section 14.5, D-83 row (line 3334; the section 11.15 line 2955 "the healer's Resurrect ... do not run `resurrect_apply`" is ambiguous in the same way): "The healer's Resurrect sets HP 1 and skips `resurrect_apply` (SHOPPES `0x16ee`-`0x1703`)". It describes the PORT but cites the ORIGINAL's addresses; at those addresses the original CALLS the routine and sets HP = max. The brief's working hypothesis ("the healer sets HP 1 and skips resurrect_apply") is half wrong for the same reason. (The D-84 row there says "revives with HP = max, status 'G' only (... runs `resurrect_apply`)" which is right.)
  * `re/notes/death-resurrection-audit.md`: row 3b says the routine sets "hp = 1 (NOT maxHP) ... y luego MP por clase" and omits the experience cut, level and max HP recompute; its S3/4/5 verdict "partyRefuge: revive (currentHp:=maxHp, status 'G') ... exacto" is wrong (the original runs the routine and the HP it copies is the recomputed max).
  * `re/notes/blackthorn.md` (lines ~303, 360, 392) and `re/deliberate-divergences.md` (lines 623, 1093): "the revive STATUS byte is open / Class C / assumed canonical". It is closed by the binary: 0x47 'G' for a dead member, and the routine does far more than set a status byte (XP cut, level, max HP, MP).
  * `re/notes/shops.md` section 5: "R resurrect ... + HP=maxHP ... Coincide con el clon" is right about the binary and wrong about the TS clone (TS sets HP 1, no routine).
  * `ALPHA4_UI.md` line 2838 and the comment at `native/core/src/enhanced.cpp:310-313` (Revive Party cheat): "back as the game's own revivals leave them: 'G', HP to the maximum, MP by class ... with no experience cut". True of the ending's revive (ENDGAME 0x075a-0x0765) but false of the healer and the Refuge (they cut XP at karma < 98 and recompute level and max HP). Not a behaviour change (the cheat is an intentional enhancement); the comment and the doc sentence go stale after the fix.
  * The task brief's overlay-base table is wrong for 10 of the overlays it lists. Brief value -> true base (my landing test, 2.0): OUTSUBS 0x81D0 -> 0xA290; COMSUBS 0x85FE -> 0xE1E0; SHOPPES3 0xA5F6 -> 0xE1E0; LOOKOBJ 0xA444 -> 0xA290; DNGLOOK 0xA2B6 -> 0xA290; CAST 0xA8D8 -> 0xBF80; COMBAT 0xBFEC -> 0xA290; INTRO 0xCD3A -> 0x81C0; DUNGEON 0xE1E0 -> 0x81D0; BLCKTHRN 0xE63E -> 0xA290. Its TOWN/MAINOUT 0x81D0, NPC/SHOPPES 0xA290, TALK/CMDS/SJOG 0xBF80 and CAST2 0xE1E0 are right. `re/tools/callers_banda.py` has a wrong `BASES` table for 15 overlays (OUTSUBS, COMSUBS, TALK, SHOPPES2, SHOPPES3, LOOKOBJ, DNGLOOK, CAST, COMBAT, DUNGEON, BLCKTHRN, CAST2, ENDGAME, FLAMES, INTRO) and omits ZSTATS and FONT. With either table the Refuge and both CAST callers are invisible; the stock tool prints ONE hit (SHOPPES 0x16f5) for `0x7ef6` (I reproduced it).

### 1.3 Step 1 -- every point on which A and B differ or leave something open, and my settlement

| # | Point | A | B | Settled by me, from the binary |
|---|---|---|---|---|
| 1 | Healer member chooser's kernel target | kernel 0x2e8e | text says "calls kernel 0x8bfe" | **0x2e8e**. `SHOPPES 0x1397 call 0x8bfe` -> (0x8bfe + 0xa290) & 0xffff = 0x2e8e = `2bc0 50 e8e6fe c3` = `push 0; call 0x2d7a; ret`. 0x8bfe is the raw operand (B's slip; its own section 3 uses the same call). The chooser does not filter by status (digit key 1..party size, `cmp ax,[0x585b]`, kernel 0x2dd4-0x2de9). |
| 2 | Which overlays the brief/tool get wrong | brief 10, tool 15, tool omits ZSTATS/FONT | "12 (+TALK, CAST2)" | A is exact: I re-derived all bases by a landing test (2.0): brief wrong for the 10 it lists; tool wrong for 15 (14 with call sites; FLAMES has none) and omits ZSTATS, FONT. B's 12 mixes brief and tool errors (SHOPPES2, ENDGAME are only in the tool table). Non-material. |
| 3 | Refuge status handling of a member that is not 'D' | "harmless: all are 'D' at entry" | "binary leaves a non-'D' status alone; TS forces 'G'" | Both right: the routine writes status only inside the 'D' branch (0x0629); 0x0b98 copies HP unconditionally. All members are 'D' at entry (kernel 0x39fc returns -1 only when no member in 0..party_size-1 is 'G' (0x47), 'P' (0x50) or 'S' (0x53): I read 0x39fc 0x3a28-0x3a6a). So a mixed party is unreachable in 1988; the fix should still not write status for a non-'D' member (see 3/4). |
| 4 | shop_flow affected count | not computed | 658 rows / 98 sequences / shops 33-39 | **Confirmed.** B's baseline `flow.txt`/`helpers.txt` are byte-identical to the repo's (SHA-256 `35f2e933...d41f` and `547a72b8...eba9`); patched differs in exactly 658 / 130 lines. Independently: 7 healer shops x (variant v in 0..89, v%6 in {4,5}, member m in {0,1,5} with m < 1 + v%6 and (m+v)%4 == 3) = 7 x 14 = 98 sequences. |
| 5 | quest_parity delta | "Avatar currentMp 0 -> 20 only (predicted, not run)" | "15 of 5,377 cases, MP only (regenerated)" | **Confirmed.** I diffed B's saved `expected_base.json` vs `expected_patched.json`: 5,377 cases, 15 changed, the only differing field is `characters[0].currentMp` 0 -> 20 (in `steps[*].state` and final `state`); `inputs` identical. |
| 6 | Is "no RNG" established? | rests on census (A8.2: effect() and print not traced to leaves) | same | **Established statically**: recursive-descent closure from CAST2 0x05e0 = 16 functions (CAST2 0x0000 ceremony; kernel 0x2192, 0x0b86, 0x08e6, 0x0a70, 0x223c, 0x0496, 0x0442, 0x1850, 0x16ba, 0x1f77, 0x17f4, 0x1bf2, 0x1cee, 0x1f12), no call to kernel 0x2092 and no operand DS:0x5420/0x5422 (the LCG seed). Positive control: the same tool from SHOPPES 0x019a (the Falsehood gold drain) reaches kernel 0x2092 and the seed accesses. Same result for kernel 0x2900 (28 functions), 0x03a0, 0x2192 and SHOPPES 0x13b0. The only far calls in the closure are `lcall [0x5350]` (display-driver pointer). |
| 7 | DS:0xa9fa meaning | "redraw flag" (3 consumers) | same, 4 consumers | **Presentation only.** Scan of the operand `fa a9`: ~30 writers set it to 1 (CAST, CAST2, CMDS, COMBAT, DUNGEON, MAINOUT, SJOG, TOWN, kernel); the readers TOWN 0x0dd3, MAINOUT 0x05a3, DUNGEON 0x03de, COMBAT 0x06e2 do `cmp [0xa9fa],0 / je / call 0x2900 / mov [0xa9fa],0`. All four callers call 0x2900 themselves right after the routine, so it is redundant. |
| 8 | Healer shop index [0xb114] (A open item 5) | not traced | "town idx" | **Settled.** Written in TALK 0x0128-0x0159: `[0xb116] = npc - 0x81` (shop type), `[0xb114]` = position of the current location in the 16-byte row `DS 0x23ca + 16*type`. Type 6 -> SHOPPES 0x14f8 (healer, via TALK jump table at TALK 0x01ca, handler 0x01ba -> stub 0x7faa) has the row `[5, 6, 7, 21, 23, 30, 31]`; resurrect prices DS 0x3d96 words `200, 215, 225, 237, 247, 249, 262, 270` (8th unused). Equals TS `HEALING_TOWNES` (shops.ts:274) and `data.json resurrectPrices`. |
| 9 | Whether the ending's revive uses the routine | "not a caller" | "independent path" | Confirmed by my census (no ENDGAME hit) and by reading ENDGAME 0x073e-0x0792: `cmp [bx],0x44 / jne`, prints name + DS 0x849a " lives!\n", `mov byte [bx],0x47`, HP := word [maxHP pointer], no XP/level/MP change. Native `endgame_scene.cpp:216-232` matches. |
| 10 | ceremony effect(n) | inside routine, modes 0 -> 8, 1 -> 6 | same | Confirmed: `06be cmp word [bp+4],1 / jne 06ca / mov ax,6 / jmp 06d3`; `06ca cmp word [bp+4],0 / jne 06d7 / mov ax,8`; `06d3 push ax / call 0` (CAST2 0x0000). Mode 0xff skips it. |
| 11 | XP >= 0x8000 | "negative, level 1, max 30; unreachable" | same, with examples | Confirmed by execution (2.11). Unreachable: every XP writer caps at 9999 (COMBAT 0x193a/0x1a4d and CAST 0x097a push 0x270f into kernel 0x3f14 `add_word_capped`; COMBAT 0x1929-0x1936 targets record+0x14). |
| 12 | Dynamic (DOS) confirmation | none | none | Still none. See section 7. |

### 1.4 Step 2 -- adversarial attempts to refute the agreed claims

I tried to break the three most consequential agreed claims, plus the overlay base.

1. **"The callers are exactly these four" and the base.** I rebuilt the overlay table from the PLINK86 records (ULTIMA.EXE image 0x7780, 24 x 16 B: load_seg at +0x0a, end_seg +0x0c, relocs +0x08, name pointer +0x0e) and fitted bases by a landing test independent of both reports (every external E8 target + candidate base must land on `55 8b ec` or a loader thunk `9a ec 02 2e 07`). Result for the overlays that matter: SHOPPES 0xa290 (228/233 external calls land), BLCKTHRN 0xa290 (132/142), CAST 0xbf80 (270/283), CAST2 0xe1e0 (186/197); the brief's BLCKTHRN 0xe63e lands 0/142, CAST 0xa8d8 lands 1/283 (and the tool's CAST2 0xc29e lands 0/197). Census with those bases: the same four sites, nothing else, nothing in ULTIMA.EXE, no `9a`/`ea` far transfer to 0x7ef6, no data word 0x7ef6 or 0xe7c0 outside the stub's own `ljmp` (the byte pair `f6 7e` appears only inside other instructions: ULTIMA.EXE 0x19cf/0x19e0, INTRO 0x1b3a, CMDS 0x19b3, SHOPPES2 0x057b, FONT 0x0084), no intra-CAST2 call or jump into 0x05e0..0x06e8. Positive controls: (a) the census run on kernel 0x2900 (status redraw) finds 80 sites (9 in ULTIMA.EXE, 71 across 18 overlays), including the four callers' redraw calls (SHOPPES 0x1707, BLCKTHRN 0x0b9f, CAST 0x10f9, CAST 0x12f4) which I checked by hand (`e8 84 58` at 0x10f9 -> 0x10fc + 0x5884 = 0x6980; + 0xbf80 = 0x2900); (b) run on kernel 0x39fc it finds exactly the 12 sites B listed; (c) the stock tool misses three of the four callers (reproduced).
2. **"The healer and Refuge call the routine and then copy the RECOMPUTED max over HP."** Read directly. Healer: `16ee ff76fa push [bp-6]; 16f1 b8 ff00 mov ax,0xff; 16f4 push ax; 16f5 e8 6ec5 call 0xdc66(raw)->0x7ef6; 16f8 8b76fa mov si,[bp-6]; 16fb b105 / 16fd d3e6 shl si,5; 16ff 8b84ba55 mov ax,[si+0x55ba]; 1703 8984b855 mov [si+0x55b8],ax`. Refuge: `0b90 56 push si; 0b91 b8 ff00 mov ax,0xff; 0b94 50; 0b95 e8 ced0 call ->0x7ef6; 0b98 8b5ef0 mov bx,[bp-0x10]; 0b9b 8b07 mov ax,[bx]; 0b9d 8905 mov [di],ax` with di = 0x55b8 (+0x20 per member) and [bp-0x10] = 0x55ba (+0x20 per member). The max HP is read AFTER the call. The Refuge copy does not look at the routine's result.
3. **"The cut uses the karma the party died with; threshold 98; level recomputed at every karma."** (a) Karma timeline: a byte scan of every code file for the operand 0x5888 shows, inside party_refuge (BLCKTHRN 0x0910-0x0c65), only the read at 0x0b03 (speech index karma/20) and the floor at 0x0bfd-0x0c04; the only pointer pushes of 0x5888 in BLCKTHRN are at 0x0575 (the capture scene, another entry); the kernel contains no direct 0x5888 operand, so no kernel callee can write karma without a pointer argument. Karma is never written inside the routine either. (b) Threshold: `064e 803e8858 62 cmp byte [0x5888],0x62; 0653 7323 jae 0678` (unsigned). (c) Level: 0x067d-0x06b6 runs after the `jae` target 0x0678 for every karma. (d) I executed the real bytes of CAST2 0x05e0 plus the real kernel `lmul` 0x0442 and `ldiv` 0x0496 in my own interpreter (it decodes with capstone and implements flags, `mul`, `imul`, `div`, `idiv`, `cwd`, `neg`, `sbb`, `shr`, `rcr`, `xchg`...) and compared with a hand model and with the TS/native model: dense XP 0..9999 x 12 karmas (0,1,2,49,50,96,97,98,99,100,128,255) = 120,000 runs, 0 mismatches against BOTH models; karma 0..255 x 158 XP values (all of 0..129 and every level boundary +-1 up to 65535) = 40,448 runs, 0 mismatches against the binary-faithful model, and the port model differs ONLY for XP >= 32768; class byte sweep (A, B, M, F, T, Z, 0x00) x INT 0..255 x old MP {0,7,255} = 5,376 runs, 0 mismatches against both models; XP 32768..65535 step 17 x 6 karmas = 11,568 runs, 0 mismatches against the signed model.

Result: nothing that A and B agree on was refuted; two wording errors found (items 1 and 2 of table 1.3). Confidence: **high** for the routine, the four callers, the call-site sequences, the HP overwrite order and the arithmetic; **medium-high** that nothing observable happens in the unmodelled screen/sound leaves (the print routine and the ceremony were stubbed in every emulation; reachability shows no RNG).

---------------------------------------------------------------------------------------------

## 2. Binary facts

### 2.0 Tooling, bases, controls

* **Overlay table** (PLINK86, ULTIMA.EXE image 0x7780): overlay id = 1-based index into the record table and into the thunk records' id byte (id 18 = CAST2.OVL load_seg 0x0e1e, id 8 = BLCKTHRN load_seg 0x0a29, id 12 = SHOPPES 0x0a29, id 16 = CAST 0x0bf8). Near-call base = load_seg * 16 (INTRO and DATA carry a 0x10 relocation header: INTRO 0x81c0). Five bands: 0x81d0 (TOWN, MAINOUT, DUNGEON), 0xa290 (FLAMES, NPC, COMBAT, BLCKTHRN, LOOKOBJ, DNGLOOK, OUTSUBS, SHOPPES, ENDGAME), 0xbf80 (SJOG, CMDS, CAST, TALK), 0xe1e0 (CAST2, ZSTATS, COMSUBS, SHOPPES2, SHOPPES3, FONT), INTRO 0x81c0. This agrees with `re/notes/overlay-load-layout.md` section 1, which I read after fitting.
* Brief vs truth for the overlays this item touches: brief BLCKTHRN 0xE63E (true 0xA290), CAST 0xA8D8 (true 0xBF80); the brief's CAST2 0xE1E0 and TALK 0xBF80 are right. `thunks.py --bases` prints the lowest thunk entry per id, which is not a load base.
* **Resolution rule used:** in overlay O, `E8 rel16` at file offset i has target t = (i + 3 + rel) & 0xffff; if t < filesize it is intra-overlay (file offset t), otherwise kernel address = (t + base) & 0xffff; a kernel address that is the start of a thunk record resolves to (overlay id, entry - that overlay's base).
* Stub controls: `0x7ef6 -> (18, 0xe7c0) = CAST2 0x05e0` (prologue `55 8b ec 83 ec 08 56`); `0x7a5e -> BLCKTHRN 0x0910`; `0x812a -> CAST2 0x009e` ("On who: "); `0x8106 -> CAST2 0x0000` (ceremony); only ONE thunk targets 0xe7c0.

### 2.1 The routine (CAST2.OVL file offsets; kernel offsets are CS)

```
05e0 55                  push bp
05e1 8bec                mov bp,sp
05e3 83ec08              sub sp,8                     ; [bp-2]=record ptr [bp-4]=scratch [bp-6]=result [bp-8]=level
05e6 56                  push si
05e7 837e0600            cmp word [bp+6],0            ; member (pushed first)
05eb 7d09                jge 05f6
05ed c746faffff          mov word [bp-6],0xffff       ; member < 0: return -1, no side effect, no text
05f2 e9ec00              jmp 06e1
05f6 8b5e06 b105 d3e3    mov bx,[bp+6] / mov cl,5 / shl bx,cl
05fd 80bfb35544          cmp byte [bx+0x55b3],0x44    ; status == 'D'? (0x55a8 + 0x0b)
0602 7416                je 061a
0604 837e0400            cmp word [bp+4],0            ; mode (pushed second) == 0 ?
0608 7407                je 0611
060a b83c95 50 e85f30    push 0x953c ; call 0x3670    ; kernel 0x1850 print_string "Not dead!\n" (mode != 0 only)
0611 c746fa0000          mov word [bp-6],0            ; return 0
0616 e9c800              jmp 06e1
061a 8b4606 b105 d3e0 05a855   ax = (member << 5) + 0x55a8
0624 8946fe  8bd8        mov [bp-2],ax / mov bx,ax
0629 c6470b47            mov byte [bx+0x0b],0x47      ; status = 'G'
062d c747100100          mov word [bx+0x10],1         ; HP = 1
0632 8a470a 2ae4         mov al,[bx+0x0a] / sub ah,ah ; class
0637 3d4100 740a         cmp ax,'A' / je 0646
063c 3d4200 7453         cmp ax,'B' / je 0694
0641 3d4d00 7508         cmp ax,'M' / jne 064e
0646 8bf3 8a440e         mov si,bx / mov al,[si+0x0e] ; INT
064b 88470f              mov [bx+0x0f],al             ; MP = INT   (A, M)
0694 8bf3 8a440e 2ae4 d1e8 ebac  B: al=INT, ax>>1 (shr), jmp 064b    ; MP = INT >> 1
064e 803e885862          cmp byte [0x5888],0x62       ; karma (unsigned byte)
0653 7323                jae 0678                     ; karma >= 98: NO experience cut
0655 b86400 99 52 50     mov ax,100 / cwd / push dx / push ax          ; long 100 (pushed first)
065b a08858 2ae4 2bc9 51 50    karma zero-extended to a long, pushed
0664 8b4714 99 52 50     mov ax,[bx+0x14] / cwd / push dx / push ax    ; XP, SIGN-extended, pushed last
066a e8f51b              call 0x2262 -> kernel 0x0442  lmul (ret 8; dx:ax = low 32 bits of XP*karma)
066d 52 50 e8441c        push dx / push ax / call 0x22b6 -> kernel 0x0496 ldiv (ret 8; dividend = top, divisor = the long 100 below)
0672 8b5efe 894714       mov bx,[bp-2] / mov [bx+0x14],ax            ; XP = LOW 16 bits of the quotient
0678 c746f80100          mov word [bp-8],1
067d 8b5efe 8b4714 99 b96400 f7f9   bx=rec; ax=XP; cwd; mov cx,100; idiv cx      ; signed 16-bit quotient
0689 8946fc 8b56f8 8bc8 eb10        [bp-4]=ax; dx=[bp-8]; cx=ax; jmp 06a3
06a0 42 d1f9             inc dx / sar cx,1
06a3 0bc9 7ff9           or cx,cx / jg 06a0           ; while (cx > 0) { level++; cx >>= 1 }
06a7 8956f8 894efc       [bp-8]=dx / [bp-4]=cx
06ad 8b5efe 8a46f8 884716      mov bx,[bp-2] / mov al,[bp-8] / mov [bx+0x16],al     ; LEVEL byte
06b6 b81e00 f7ea 894712        mov ax,0x1e / imul dx / mov [bx+0x12],ax             ; MAX HP word = 30*level
06be 837e0401 7506 b80600 eb0a cmp word [bp+4],1 / jne 06ca / mov ax,6 / jmp 06d3
06ca 837e0400 7507 b80800      cmp word [bp+4],0 / jne 06d7 / mov ax,8
06d3 50 e829f9           push ax / call 0x0000        ; CAST2 ceremony effect(n): modes 0 and 1 only
06d7 c746fa0100          mov word [bp-6],1            ; return 1
06dc c606faa901          mov byte [0xa9fa],1          ; status-panel-dirty flag
06e1 8b46fa 5e 8be5 5d c20400  ax=[bp-6]; pop si; mov sp,bp; pop bp; ret 4
```
Kernel helpers, verified by reading: `0x0442` `lmul`: short path when both high words are zero (`mov ax,[bp+6]; mov bx,[bp+0xa]; or bx,ax; ... jne` then `mul bx`), otherwise the cross-product long path, low 32 bits, `ret 8`. `0x0496` `ldiv`: records the signs in DI (`inc di` per negative operand), negates negatives, divides (short path `div cx` twice when the divisor's high word is 0, which is the case for 100), `dec di / jne` then negates the result when exactly one operand was negative; `ret 8`, result in dx:ax. So truncation toward zero, signed.

Record layout (32 bytes at DS:0x55a8 + 0x20*slot): +0x0a class, +0x0b status, +0x0e INT (byte), +0x0f MP (byte), +0x10 HP (word), +0x12 max HP (word), +0x14 XP (word), +0x16 level (byte). DS:0x5888 karma (byte), DS:0x585b party size (byte), DS:0x57aa gold (word), DS:0x5893 location (byte).

Write set of the routine (executed): +0x0b, +0x0f (A, B, M only), +0x10/11, +0x12/13, +0x14/15 (only if karma < 98), +0x16, and DS:0xa9fa. Nothing else.

### 2.2 Arithmetic, widths, signedness (all verified by execution)

* Karma: unsigned byte, `jae`: karma 0..97 cut, 98..255 no cut. Real range 0..99.
* XP' = low16( trunc( (int32)(int16)XP * karma / 100 ) ). For XP <= 32767: floor(XP * karma / 100), no overflow possible (max 9999 * 97). The 16-bit store is the only truncation.
* Level = 1 + number of loop iterations of `while (q > 0) { q >>= 1 }` with q = (int16)XP / 100 (signed `idiv`): q 0 -> 1; 1 -> 2; 2..3 -> 3; 4..7 -> 4; 8..15 -> 5; 16..31 -> 6; 32..63 -> 7; 64..127 -> 8; 128..255 -> 9; 256..327 -> 10. XP thresholds: 100, 200, 400, 800, 1600, 3200, 6400 (level 8), 12800 (9), 25600 (10). NO table, NO cap at 8; the in-game XP ceiling 9999 keeps it <= 8.
* Max HP = 30 * level (word, `imul` low word). It REPLACES the old max (up or down). HP is NOT clamped by the routine (HP is simply 1).
* MP: 'A' (0x41), 'M' (0x4d): MP = INT (byte). 'B' (0x42): MP = INT >> 1 (word shift of the zero-extended INT, low byte stored: INT 17 -> 8, 18 -> 9, 1 -> 0, 255 -> 127). Any other class byte: MP untouched (never zeroed).
* Mutation order: status, HP, MP, [XP], level, max HP, ceremony, return, flag.

### 2.3 The four call sites

**Spell 42 In Mani Corp (CAST.OVL 0x10ec, mode 0):**
```
10ec e8bbb0      call 0xffffc1aa  -> stub 0x812a -> CAST2 0x009e "On who: " (DS 0x94f4; "None!" DS 0x94fe) ; returns member or -1
10ef 50          push ax          ; member
10f0 2bc0 50     sub ax,ax / push ax          ; mode 0
10f3 e880ae      call 0xffffbf76 -> 0x7ef6
10f6 8946f6      mov [bp-0xa],ax
10f9 e88458      call 0x6980 -> kernel 0x2900 (status redraw)
10fc e9a700      jmp 0x11a6 : cmp [bp-0xa],1 / jne -> print DS 0x4656 "Success!\n" ; ==0 -> DS 0x4660 "Failed!\n" + glide (call 0x842e, args 0x32,1,0x7d0,0x320) ; -1 -> nothing
```
Location gate (CAST 0x0e1a-0x0e8e): location 0 tests bit 8, location > 0x7f (combat) bit 1, location < 0x21 bit 4, else bit 2 of `DS 0x1c90[42]` = **0x0e**: castable outdoors, in towns/castles, in dungeons; NOT in combat. So the combat branches of both ports (TS main.ts:3325, native combat.cpp:1086-1105) are unreachable for spell 42.

**Scroll 6 In Mani Corp (CAST.OVL reader 0x11de -> arm 0x12d8, mode 1):** `11ec dec byte [bx+0x5820]` (scroll consumed FIRST), print DS 0x466a "Scroll\n\n", print DS 0x46d2 "Resurrection!\n", `12df cmp byte [0x5893],0x80 / jae 12fa` (combat: DS 0x46e1 "Not here!\n"), else `12e6 call "On who"; push ax; push 1; 12ee call 0x7ef6; 12f1 mov [bp-2],ax; 12f4 call 0x6980`; result returned to the use-item tail (CAST 0x1b8a: `cmp word [bp-0xa],0 / jne 1baa` else print DS 0x4a7b "Failed!\n" + glide). A living target therefore prints "Not dead!\n" (routine) then "Failed!\n" + glide; a cancelled picker (-1) prints nothing; success prints nothing more.

**Healer 'R' branch (SHOPPES.OVL function 0x14f8, branch 0x169a-0x170a, mode 0xff):**
```
169a b87e81 50 e81f5f   print DS 0x817e "Resurrect"
16a1 e8d8fc             call 0x137c : if byte [0x585b]==1 -> member 0 (no prompt) else print DS 0x805a "\n\n\"Who needs my aid?\" " + call 0x8bfe (= kernel 0x2e8e), -1 prints DS 0x8072 "No one"
16a4 8946fa / 16a7 3dffff / 16aa 7503 / 16ac e9f9fe   [bp-6] = member ; -1 -> back to the menu 0x15a8
16b5 80bfb35544 / 16ba 7403 / 16bc e915ff   status != 'D' -> 0x15d4: print DS 0x3d5a "\n\n\"Thou hast no need of this art!\"\nsays $." (call 0x26), menu. NOTHING charged, routine not called.
16bf..16d1 print DS 0x8188 "\n\n\"", DS 0x818c "I can raise this unfortunate person from ", DS 0x81b6 "the dead "
16d4 8b1e14b1 d1e3 8b87963d a318b1    price = word [DS 0x3d96 + 2*[0xb114]] -> [0xb118]
16e1 e886fd             call 0x146a (pay) ; 16e4 or ax,ax / je 16eb ; non-zero (declined) -> menu
16eb e8c2fc             call 0x13b0 (jingle + XOR flash, sound only)
16ee ff76fa b8ff00 50 e86ec5   push [bp-6] ; push 0xff ; call ->0x7ef6
16f8 8b76fa b105 d3e6 8b84ba55 8984b855   HP word := max HP word (recomputed); return value ignored
1707 e8666f / 170a e99bfe   call 0x2900 ; jmp 0x15a8 (menu: DS 0x81c8 "\n\n\"Is there any other way in which I may\n" + DS 0x81f2 "aid thee?\" ")
```
Pay routine 0x146a: prints DS 0x807a `for % gold.\n\nWilt thou\npay?" `, reads a key until 'Y' (0x59, echo DS 0x8098 "Yes") or 'N' (0x4e, echo DS 0x809c "No", returns 1 = declined). On 'Y': `cmp [0x57aa],[0xb118] / jge ok` (signed); if gold < price: refuse unless (price <= 100 AND location [0x5893] == 7) -> else print shoppe record 0x23ab (via 0x17a), return 1; at 0x14da, if not declined and gold >= price: `sub [0x57aa],price` then `call 0x19a` (post_purchase_gold_rand: if byte [0x5958]==0 then `rand(1,64)` -> gold reduced, floored at 0; 1 RNG draw, ONLY there); returns 0. Location 5 makes Cure and Heal free (`cmp [0x5893],5` at 0x15e5 and 0x1655, then jump to the jingle) but there is NO such test in the 'R' branch. Location 7 charity needs price <= 100 and the minimum resurrect price is 200, so it never applies. Karma is not referenced anywhere in SHOPPES.OVL. Order: pay -> Falsehood drain (0 or 1 draw) -> jingle -> routine -> HP := max.

**Refuge (BLCKTHRN.OVL party_refuge, function 0x0910, stub 0x7a5e; revive loop 0x0b4f-0x0bb3, mode 0xff):**
```
0b03 a08858 2ae4 b114 f6f1 2ae4 8946fe    index = karma / 20 (death karma; KARMA.DAT record, speech 0x0b11-0x0b3e + getkey 0x83dc)
0b41 print DS 0x71cc "\n\nStrange words are intoned." ; 0b4c delay(4)
0b4f c746fa0000 / 0b54 a05b58 2ae4 / 0b59 0bc0 / 0b5b 7459   party size byte; 0 -> skip to 0bb6
0b5d bfb855 / 0b60 c746f0ba55     di = 0x55b8 (HP of slot 0), [bp-0x10] = 0x55ba (max HP of slot 0)
0b65 loop: si = [bp-6]; tone = 0x8e30 / (si+7) (kernel 0x03a0, unsigned long divide, a PITCH not HP); tone_sweep via thunk 0x7f02 (kernel 0x2192)
0b90 56 / 0b91 b8ff00 50 / 0b95 e8ced0   push si ; push 0xff ; call ->0x7ef6
0b98 8b5ef0 / 0b9b 8b07 / 0b9d 8905      ax = word [[bp-0x10]] (max HP, read AFTER the call) ; word [di] = ax   (unconditional)
0b9f e8ce7a  call 0x8670 -> kernel 0x2900 ; 0ba2 add di,0x20 ; add [bp-0x10],0x20 ; inc si ; 0baa a05b58 (party size RE-READ) ; 0baf 3bf0 / 0bb1 72b5   si < party size -> loop
0bb6.. print DS 0x71ea "\n\nVertigo...\n" ; delay(4)
0bfd 803e88584b / 0c02 7305 / 0c04 c60688584b   if karma < 0x4b then karma = 0x4b          ; FLOOR 75, AFTER the loop
0c09.. location 0x11, floor 1, (10,10), [0x587c]=0x1c, clock advanced to 6:00 (advance_clock(9) loop until [0x587f]==6), food 0 -> 0x3f
```
Members revived: slots 0 .. party_size-1 (NOT the roster); the routine is called for every one of them, a non-'D' member would only print "Not dead!\n" and still get HP := max. Entry: the three result-is-negative sites after kernel 0x39fc. TOWN: `1436 call 0x39fc`, `1456 cmp [bp-8],-1 / jne`, `145c jmp 0x1862`, `1862: call 0x8c56 ; 1865 call 0xfffff88e` = stub 0x7a5e. MAINOUT: `0aa2 call 0x39fc`, `0ac2 cmp [bp-6],-1 / jne 0b00`, (`0ac8 cmp [0xa9bd],1` only decides a prompt loop), `0af0 call 0x8cac` = kernel trampoline 0x0e7c (`call 0xe26 / call 0x7a5e / call 0xe1d`). DUNGEON: `0fa8 call 0x39fc` (result in [bp-0xe]), `100e cmp [bp-0xe],0 / jge 1017`, `1014 call 0x8cac` (same trampoline). Kernel 0x39fc returns 0 if any member 0..party_size-1 is 'G' or 'P', else 1 if any is 'S', else -1 (also -1 for party size 0) -> so every member is 'D' when the loop runs.

### 2.4 Differences between the four paths

| | Spell 42 | Scroll 6 | Healer 'R' | Refuge |
|---|---|---|---|---|
| mode word | 0 | 1 | 0xff | 0xff |
| who | "On who: " picker | same | chooser 0x137c (auto member 0 if party size 1) | slots 0..party_size-1 |
| precondition before the routine | window flags, mixed count, MP, level (all consumed before the prompt) | scroll consumed first, location < 0x80 | status 'D' (else "Thou hast no need of this art!", nothing charged) | none beyond 0x39fc == -1 |
| payment | 1 mixed spell + circle MP | 1 scroll | DS 0x3d96[idx] gold, + 1 rand(1,64) only with Falsehood present (before the routine) | none |
| not 'D' inside the routine | silent, 0 -> "Failed!" + glide | "Not dead!" , 0 -> "Failed!" + glide | unreachable (pre-checked) | unreachable (all 'D') |
| routine mutation | status G, HP 1, MP, XP cut if karma < 98, level, max HP | same | same | same (karma = death karma) |
| ceremony inside routine | effect(8), success only | effect(6), success only | none | none |
| HP after | 1 | 1 | max HP = 30*level | max HP = 30*level |
| status after | G | G | G | G |
| text after | "Success!" / "Failed!" / nothing if cancelled | nothing on success | menu loop | "Vertigo..." scene |
| karma written | no | no | no | floor 75 AFTER the loop |
| RNG | 0 | 0 | 0 in the routine; 1 draw in the payment with Falsehood | 0 |
| gold | no | no | price | no |

### 2.5 Census tables (all callers listed; relevance stated)

* Stub 0x7ef6 (`CAST2:0x05e0`): **BLCKTHRN 0x0b95 (raw 0xdc66, base 0xa290), SHOPPES 0x16f5 (raw 0xdc66, base 0xa290), CAST 0x10f3 and 0x12ee (raw 0xbf76, base 0xbf80)**. All relevant. ULTIMA.EXE: none. ALT-base hits: none. No intra-CAST2 caller.
* Stub 0x7a5e (`BLCKTHRN:0x0910`, the Refuge): ULTIMA.EXE 0x0e7f (trampoline 0x0e7c, callers MAINOUT 0x0af0 and DUNGEON 0x1014) and TOWN 0x1865 (trampoline 0x1862, entered by TOWN 0x145c `jmp`). (INTRO shows ALT hits under a base INTRO does not use; ignored.) Stub 0x7abe is the Blackthorn capture, not this item.
* Kernel 0x39fc (the Refuge trigger): TOWN 0x126b, 0x12c0, 0x1436, 0x15bf; MAINOUT 0x0aa2, 0x1158, 0x1b4b; DUNGEON 0x0d32, 0x0fa8; LOOKOBJ 0x038a; CMDS 0x1bfd; TALK 0x0116. Only the negative-result tests at TOWN 0x1456, MAINOUT 0x0ac2 and DUNGEON 0x100e lead to the Refuge.
* Other dead -> alive transitions: ENDGAME 0x073e-0x0792 (direct, no routine). Every other `mov byte [..],0x47` is S -> G or P -> G behind a status compare (B's scan; I spot-checked COMBAT: no 0x47 store into a status byte).

### 2.6 Strings (DS offset; file offset = DS + 0x10), verified in DATA.OVL

0x953c "Not dead!\n"; 0x94f4 "On who: "; 0x94fe "None!"; 0x4656 "Success!\n"; 0x4660 "Failed!\n"; 0x4a7b "Failed!\n"; 0x46d2 "Resurrection!\n"; 0x46e1 / 0x462f "Not here!\n"; 0x466a "Scroll\n\n"; 0x71cc "\n\nStrange words are intoned."; 0x71ea "\n\nVertigo...\n"; 0x817e "Resurrect"; 0x8188 "\n\n\""; 0x818c "I can raise this unfortunate person from "; 0x81b6 "the dead "; 0x805a "\n\n\"Who needs my aid?\" "; 0x8072 "No one"; 0x807a "for % gold.\n\nWilt thou\npay?\" "; 0x8098 "Yes"; 0x809c "No"; 0x3d5a "\n\n\"Thou hast no need of this art!\"\nsays $."; 0x81c8 + 0x81f2 the menu question. Tables: resurrect prices DS 0x3d96 words 200, 215, 225, 237, 247, 249, 262, 270; heal DS 0x3d86 bytes 35..70; cure DS 0x3d8e bytes 20, 25, 30, 35, 40, 15, 10, 1. Spell window flags DS 0x1c90 (48 bytes), entry 42 = 0x0e.

### 2.7 Boundary table (computed by executing the real bytes; class F, INT 20, old level 5, old max 150; cells "XP' / level / max HP"; status G; HP is then 1 for spell/scroll and the max HP shown for healer/Refuge; MP untouched for class F)

| XP in | k=0 | k=1 | k=50 | k=74 | k=75 | k=96 | k=97 | k=98 | k=99 | k=100 | k=255 |
|---:|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 |
| 1 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 0 / L1 / 30 | 1 / L1 / 30 | 1 / L1 / 30 | 1 / L1 / 30 | 1 / L1 / 30 |
| 99 | 0 / L1 / 30 | 0 / L1 / 30 | 49 / L1 / 30 | 73 / L1 / 30 | 74 / L1 / 30 | 95 / L1 / 30 | 96 / L1 / 30 | 99 / L1 / 30 | 99 / L1 / 30 | 99 / L1 / 30 | 99 / L1 / 30 |
| 100 | 0 / L1 / 30 | 1 / L1 / 30 | 50 / L1 / 30 | 74 / L1 / 30 | 75 / L1 / 30 | 96 / L1 / 30 | 97 / L1 / 30 | 100 / L2 / 60 | 100 / L2 / 60 | 100 / L2 / 60 | 100 / L2 / 60 |
| 101 | 0 / L1 / 30 | 1 / L1 / 30 | 50 / L1 / 30 | 74 / L1 / 30 | 75 / L1 / 30 | 96 / L1 / 30 | 97 / L1 / 30 | 101 / L2 / 60 | 101 / L2 / 60 | 101 / L2 / 60 | 101 / L2 / 60 |
| 199 | 0 / L1 / 30 | 1 / L1 / 30 | 99 / L1 / 30 | 147 / L2 / 60 | 149 / L2 / 60 | 191 / L2 / 60 | 193 / L2 / 60 | 199 / L2 / 60 | 199 / L2 / 60 | 199 / L2 / 60 | 199 / L2 / 60 |
| 200 | 0 / L1 / 30 | 2 / L1 / 30 | 100 / L2 / 60 | 148 / L2 / 60 | 150 / L2 / 60 | 192 / L2 / 60 | 194 / L2 / 60 | 200 / L3 / 90 | 200 / L3 / 90 | 200 / L3 / 90 | 200 / L3 / 90 |
| 399 | 0 / L1 / 30 | 3 / L1 / 30 | 199 / L2 / 60 | 295 / L3 / 90 | 299 / L3 / 90 | 383 / L3 / 90 | 387 / L3 / 90 | 399 / L3 / 90 | 399 / L3 / 90 | 399 / L3 / 90 | 399 / L3 / 90 |
| 400 | 0 / L1 / 30 | 4 / L1 / 30 | 200 / L3 / 90 | 296 / L3 / 90 | 300 / L3 / 90 | 384 / L3 / 90 | 388 / L3 / 90 | 400 / L4 / 120 | 400 / L4 / 120 | 400 / L4 / 120 | 400 / L4 / 120 |
| 799 | 0 / L1 / 30 | 7 / L1 / 30 | 399 / L3 / 90 | 591 / L4 / 120 | 599 / L4 / 120 | 767 / L4 / 120 | 775 / L4 / 120 | 799 / L4 / 120 | 799 / L4 / 120 | 799 / L4 / 120 | 799 / L4 / 120 |
| 800 | 0 / L1 / 30 | 8 / L1 / 30 | 400 / L4 / 120 | 592 / L4 / 120 | 600 / L4 / 120 | 768 / L4 / 120 | 776 / L4 / 120 | 800 / L5 / 150 | 800 / L5 / 150 | 800 / L5 / 150 | 800 / L5 / 150 |
| 1599 | 0 / L1 / 30 | 15 / L1 / 30 | 799 / L4 / 120 | 1183 / L5 / 150 | 1199 / L5 / 150 | 1535 / L5 / 150 | 1551 / L5 / 150 | 1599 / L5 / 150 | 1599 / L5 / 150 | 1599 / L5 / 150 | 1599 / L5 / 150 |
| 1600 | 0 / L1 / 30 | 16 / L1 / 30 | 800 / L5 / 150 | 1184 / L5 / 150 | 1200 / L5 / 150 | 1536 / L5 / 150 | 1552 / L5 / 150 | 1600 / L6 / 180 | 1600 / L6 / 180 | 1600 / L6 / 180 | 1600 / L6 / 180 |
| 3199 | 0 / L1 / 30 | 31 / L1 / 30 | 1599 / L5 / 150 | 2367 / L6 / 180 | 2399 / L6 / 180 | 3071 / L6 / 180 | 3103 / L6 / 180 | 3199 / L6 / 180 | 3199 / L6 / 180 | 3199 / L6 / 180 | 3199 / L6 / 180 |
| 3200 | 0 / L1 / 30 | 32 / L1 / 30 | 1600 / L6 / 180 | 2368 / L6 / 180 | 2400 / L6 / 180 | 3072 / L6 / 180 | 3104 / L6 / 180 | 3200 / L7 / 210 | 3200 / L7 / 210 | 3200 / L7 / 210 | 3200 / L7 / 210 |
| 6399 | 0 / L1 / 30 | 63 / L1 / 30 | 3199 / L6 / 180 | 4735 / L7 / 210 | 4799 / L7 / 210 | 6143 / L7 / 210 | 6207 / L7 / 210 | 6399 / L7 / 210 | 6399 / L7 / 210 | 6399 / L7 / 210 | 6399 / L7 / 210 |
| 6400 | 0 / L1 / 30 | 64 / L1 / 30 | 3200 / L7 / 210 | 4736 / L7 / 210 | 4800 / L7 / 210 | 6144 / L7 / 210 | 6208 / L7 / 210 | 6400 / L8 / 240 | 6400 / L8 / 240 | 6400 / L8 / 240 | 6400 / L8 / 240 |
| 9999 | 0 / L1 / 30 | 99 / L1 / 30 | 4999 / L7 / 210 | 7399 / L8 / 240 | 7499 / L8 / 240 | 9599 / L8 / 240 | 9699 / L8 / 240 | 9999 / L8 / 240 | 9999 / L8 / 240 | 9999 / L8 / 240 | 9999 / L8 / 240 |

(The 144 cells of B's table all agree with this one; A's tables agree on the cells I compared.) Reading: threshold = 98 (karma 97 cuts, 98 does not; 100 and 255 behave as >= 98). The cut can cross a level boundary downward even at karma 97 (XP 100 -> 97, level 2 -> 1). Truncation, not rounding: XP 1 at karma 97 -> 0; the Avatar of INIT.GAM (XP 150) at karma 75 -> 112 (112.5 truncates). Level and max HP are recomputed at karma >= 98 too: XP 450 with a stored level 2 becomes level 4 / max 120 at karma 99 (an unclaimed level-up is granted; an inflated level is taken away).

**A member at XP 0:** XP stays 0 for EVERY karma (0 * k / 100 = 0) and every mode; level becomes 1 and max HP 30 regardless of the previous level or max HP (a level-5 / 150 member with XP 0 drops to level 1 / 30); HP = 1 (spell, scroll) or 30 (healer, Refuge); status G; MP by class. **A member already at the minimum** (XP 0, level 1, max HP 30) is a fixed point: it changes only in status, HP and MP. The routine has no floor, clamp or minimum-XP-for-level; karma 0 sends ANY member to XP 0, level 1, max HP 30.

**MP by class (karma 99, XP 500; executed):** A INT 15 -> 15; M INT 22 -> 22; A/M INT 255 -> 255; B INT 17 -> 8; B INT 18 -> 9; B INT 1 -> 0; B INT 255 -> 127; F, 'T', 0x00 or any other class byte: MP unchanged (old 0 stays 0, old 5 stays 5).

**Worked end-to-end rows (executed routine + the caller's HP copy):** Refuge, party of 3, karma 50: A INT 20 XP 0 -> XP 0 / L1 / 30 / HP 30 / MP 20; B INT 17 XP 250 (old L3/90, MP 3) -> 125 / L2 / 60 / HP 60 / MP 8; M INT 22 XP 800 (old L5/150, MP 2) -> 400 / L4 / 120 / HP 120 / MP 22; karma afterwards 75. Death karma 74, F XP 100 -> XP 74 / L1 / 30 / HP 30 (the same member at 75 -> 75). Karma 99, F XP 450 stored L2/60 -> XP 450 / L4 / 120 / HP 120. Karma 98 / 97, F XP 100 -> 100 / L2 / 60 / HP 60 and 97 / L1 / 30 / HP 30. Healer, INIT.GAM Avatar (A, INT 15, XP 150, L2/60, MP 0) at karma 75 -> XP 112 / L2 / 60 / HP 60 / MP 15; at karma 99 -> XP 150 / L2 / 60 / HP 60 / MP 15. Non-'D' members in mode 0xff (synthetic): status G, P or S, return 0, print DS 0x953c, nothing changed except that the CALLER then sets HP := max (a 'P' member with HP 40 / max 90 ends at HP 90, status 'P').

**Out of range (unreachable in play, executed for the record):** XP is a signed word to the routine. XP 32767 karma 50 -> 16383 / L9 / 270; XP 32768 karma 50 -> 49152 / L1 / 30; XP 40000 karma 50 -> 52768 / L1 / 30; XP 65535 karma 50 -> 0 / L1 / 30; XP 40000 karma 99 -> 40000 / L1 / 30 (unchanged XP, level forced to 1). The TS and native copies treat XP as unsigned and differ for every XP >= 32768 and karma > 0 or >= 98 (e.g. XP 32768 karma 50 -> 16384 / L9 / 270).

---------------------------------------------------------------------------------------------

## 3. Reference (TypeScript): exact change

Files (both CRLF): `game/src/core/shops/shops.ts`, `game/src/core/world/blackthorn.ts`. `applyResurrect` (`game/src/core/magic/cast.ts:545-561`) and `resurrectionLevel` / `resurrectionMaxHp` (`magic/tables.ts:130-143`) are already byte-exact and MUST NOT change (the spell and the scroll leave HP 1, and `magic.test.ts:642`, `cast-onwho-consumidores.test.ts:186` pin that). No import cycle: the transitive import closure of `cast.ts` is 19 files and contains neither `shops.ts` nor `blackthorn.ts` (checked).

1. **`shops/shops.ts`, `healerHeal` (lines 1205-1243).** Add `import { applyResurrect } from "../magic/cast.js";` with the other imports (after line 29, `import { rosterLeaveCompact, rosterPickupInsert } from "../party.js";`). Replace the else branch at 1238-1241
   ```ts
   } else {
     rec.status = "G";
     rec.currentHp = 1;
   }
   ```
   by
   ```ts
   } else {
     applyResurrect(rec, state.karma); // SHOPPES 0x16f5 -> CAST2 0x05e0, mode 0xff: status G, HP 1, MP, cut, level, max HP
     rec.currentHp = rec.maxHp;        // SHOPPES 0x16ff-0x1703: HP := the RECOMPUTED maximum
   }
   ```
   `needs` (status 'D', line ~1215), the gold test and `state.gold -= price` stay as they are and stay before it (binary order: 'D' test 0x16b5, payment 0x14e9, drain 0x14ed, jingle, routine). No RNG. Update the doc comment at 1197-1203 ("'D' -> 'G' with 1 HP") to say what the routine does. Call sites of `healerHeal` need no change: `ui/shop-console.ts:688` (location-5 free Cure/Heal only, never Resurrect), `:730` (paid; `drainOnPurchase` follows, compatible because the routine draws nothing), `:739` (charity, never reached for Resurrect), `:2183`/`:2186` (legacy phases).
2. **`world/blackthorn.ts`, `partyRefuge` (lines 528-563).** Add the same import after line 24. Replace the loop body at 538-542
   ```ts
   const c = state.characters[i];
   if (!c) continue;
   if (c.status === "D") revived++;
   c.currentHp = c.maxHp;
   c.status = "G";
   ```
   by
   ```ts
   const c = state.characters[i];
   if (!c) continue;
   if (c.status === "D") {
     revived++;
     applyResurrect(c, state.karma); // BLCKTHRN 0x0b95 -> CAST2 0x05e0, mode 0xff; karma = the karma the party died with
   }
   c.currentHp = c.maxHp;            // 0x0b98-0x0b9d: UNCONDITIONAL, reads max HP after the call
   ```
   i.e. the unconditional `c.status = "G"` goes away (the routine writes status only for a 'D' member). Keep `state.karma < REFUGE_KARMA_FLOOR` (line 544) AFTER the loop (0x0bfd). Keep `revived` as the count of 'D' members at entry (= the routine's successes). Update the doc comments at 505-526 and 531-538 that call the status byte "open": the binary settles it. `Game.resolveRefuge` (game.ts:6436) and `checkRefuge`/`buildRefugeScript` (6362-6443) need no change: the speech index already uses `state.karma` before `partyRefuge` (6400), and nothing between `checkRefuge` and `resolveRefuge` writes karma.
3. **Optional, NOT parity-required (decide separately):** model the signed XP word in `applyResurrect` (`const s = (exp << 16) >> 16` for the product and the level quotient, low-16 store). Unreachable in play; `magic_parity` uses exp 1000 only, so it would not move any corpus. Default recommendation: do not, record it as a documented unreachable difference.
4. **Presentation neighbours (same routine, separate decision, no corpus moves except where noted):** scroll on a living target should print "Not dead!\n" then "Failed!\n" + glide (TS `main.ts:4879-4883`); the casting ceremony belongs inside the routine on success only (effect 8 for the spell, 6 for the scroll) instead of the cast-time generic ceremony (`magic/ceremony.ts`, `SCROLL_CEREMONY_INDEX[6] = null`).

---------------------------------------------------------------------------------------------

## 4. Native: exact change

1. **`native/core/src/magic.cpp` (LF) and `native/core/include/openu5/magic.h:159`.** Extract the Resurrect body at `magic.cpp:231-247` into a free function declared next to `apply_target_spell`:
   ```cpp
   // A4-PARITY2 (D-83/D-84). CAST2.OVL 0x05e0, the part all four callers share. 'D' -> 'G', HP 1,
   // MP by class (A/M = INT, B = INT >> 1, others untouched), experience cut to exp * karma / 100
   // (truncating, 16-bit store) when karma < 98, then level = 1 + bitlength(exp / 100) and max HP =
   // 30 * level, for every karma. Returns false and touches nothing for any other status. No RNG.
   // The spell and the scroll stop here (HP 1); the healer (SHOPPES 0x16f8) and the Refuge
   // (BLCKTHRN 0x0b98) then set HP = max HP.
   bool resurrect_apply(CharacterState &, uint8_t karma);
   ```
   with the body unchanged (same casts, `-Wall -Wextra -Wconversion -Werror` is already satisfied by the existing lines), and `case MagicEffect::Resurrect: return resurrect_apply(p, karma);` in `apply_target_spell`. `magic.cpp`, `shops.cpp` and `quest_world.cpp` are in one source list (`native/core/sources.cmake` lines 37, 41, 72) shared by the host library and the firmware (`targets/tdeck/main/CMakeLists.txt` includes it), so no link change is needed.
2. **`native/core/src/shops.cpp` (LF), `healer_heal` (210-232).** Add `#include "openu5/magic.h"`. Keep line 223 (`pay(g, price);`) and replace lines 224-230 (`if (Heal) ... else { c.status='G'; if (Resurrect) c.current_hp = 1; }`) by
   ```cpp
   pay(g, price);
   if (service == HealerService::Heal)
       c.current_hp = c.max_hp;
   else if (service == HealerService::Cure)
       c.status = 'G';
   else {
       resurrect_apply(c, g.karma);  // SHOPPES 0x16f5 -> CAST2 0x05e0 (mode 0xff)
       c.current_hp = c.max_hp;      // 0x16f8-0x1703: the recomputed maximum
   }
   ```
   `g.karma` is `uint8_t` (state.h:46). All native healer paths go through this one function: `shop_orchestration.cpp:328` (location-5 free Cure/Heal only), `:455` (HealerDeal), `:858` and `:861` (legacy healer with the location-7 retry). Unchanged: `needs`, the gold test, `gold_fits`, `pay`.
3. **`native/core/src/quest_world.cpp` (CRLF), `resolve_refuge` (185-195), line 189.** Add `#include "openu5/magic.h"` if it is not reached transitively. Replace the loop body
   ```cpp
   {auto &ch=g.party.characters[i];ch.current_hp=ch.max_hp;ch.status='G';}
   ```
   by
   ```cpp
   {auto &ch=g.party.characters[i];resurrect_apply(ch,g.karma);ch.current_hp=ch.max_hp;}
   ```
   Keep `if(g.karma<75)g.karma=75;` (line 190) AFTER the loop. The device calls this same function (`native/targets/tdeck/main/alpha_runtime.cpp:779`); there is no device-only copy. The a3_hf9 mutation anchors (`a3_hf9_mutation_check.py` M14 anchors in `check_refuge`, M15 in `alpha_runtime.cpp`) do not touch line 189.
4. **Stale text after the fix:** the comment at `enhanced.cpp:310-313` and `ALPHA4_UI.md` line 2838 (see 1.2); ledger rows D-83 / D-84 and the section 14.5 / 11.15 rows move to fixed; the `a4_enh2_preservation` header ("Original is bit for bit pre-A4-ENH2") needs a sentence that this item changes Original on purpose (ALPHA4_UI.md section 11.7 already says so).

---------------------------------------------------------------------------------------------

## 5. Parity fixtures and corpora

Order the project's own rule requires (ALPHA4_UI.md 11.7): change the TS reference at the layer the fixtures pin, run `--check` (CONTROL: drift MUST appear in the three places below), regenerate, token-diff proof, then move native.

**Must change (all generated, git-ignored `native/core/build*/`; counts measured by regenerating in a scratch mirror whose unpatched baseline is byte-identical to the repo's files):**

| Corpus | Pins | Change |
|---|---|---|
| `shop_parity` (`native/core/build-shops/helpers.txt` <- `tools/generate-shop-fixtures.ts` op 9; native `tests/shop_parity_test.cpp:55`; drift test `typescript_shop_fixture_drift`, CMakeLists 3018) | `S.healerHeal(g, i, ['heal','cure','resurrect'][k], price)`: v 0..59 x member {-1,0,1,5,15,16} x service x price {0,10,100,9999} = **4,320 rows** (1,440 resurrect); state karma 50, class `['A','B','M','F'][i%4]`, INT `[0,15,33,34,50][v%5]`, exp i, level 2, max 30+i, HP 10+i, MP i, status `"GPSD"[(i+v)%4]`, gold `[0,1,10,100,9998,9999][v%6]`; the digest hashes every field of all 16 characters | **130 rows** of 70,472 change (rows with member 0/1/5/15, status 'D', gold >= price; by price: 60 + 40 + 25 + 5; I recomputed this by hand). Each goes from {G, HP 1, exp i, level 2, max 30+i, MP i} to {G, exp floor(i/2), level 1, max 30, HP 30, MP by class}. Reference result: helpers.txt SHA-256 `14ad4555a89a21d473368e1cd1a71de111f3f566bdce4da0701d474309d42d8b` (baseline `547a72b8ef9d20c65b6b743a9fd4ae1cc6eda4185ead717077d2f64b0d43eba9`). |
| `shop_flow` (`build-shops/flow.txt` <- `tools/generate-shop-flow-fixtures.ts` line 83, healer walk `[confirm, resurrect, member m, confirm, confirm, resurrect, member m, decline, cancel]` for m in {0,1,5}; native `tests/shop_flow_test.cpp`; drift `typescript_shop_flow_drift`) | the real `ShopConsole` over `healerHeal` | **658 of 584,406 rows, 98 of 77,607 sequences**, shops 33-39 (the 7 healers), only variants with gold >= 200 and member 'D' (my independent count: 14 (v,m) x 7 shops = 98). flow.txt SHA-256 after: `b450fc72b9639a4b4a4deb6ed7144f7c6a74810c24dda1e73c742196f724a02a` (before `35f2e93320b5c9eac3e79706b445008ab7dc8376d724932cf2830fff8d05d41f`). An exact implementation of section 3 must reproduce both digests (the hashes cover every character field). |
| `quest_parity` (live compare `tools/check-quests.ts` TS vs native `tests/quest_driver.cpp`; line 200: 6 karmas [0,19,20,74,75,99] x food {0,10} = 12 `refuge-check` x3 + `refuge-resolve`; line 243: 3 combat-defeat seeds ending in `refuge-resolve`; party = one Avatar class A, INT 20, MP 0, XP 0, level 1, max 30) | `characters` is in the projection | **15 of 5,377 cases** (39 step states + 15 finals): the ONLY changed field is `characters[0].currentMp` 0 -> 20. XP stays 0, level 1, max 30, HP 30, status G for every karma (the karma is irrelevant at XP 0). TS and native MUST move together or the live compare fails. |
| `a4_enh2_preservation` golden B4 (`native/core/tests/a4_enh2_preservation_test.cpp:291-336`, `kResurrection = 0xbe90d6bb09040045` at line 397, check at 408) | In Mani Corp x 7 karmas + `healer_heal(g,1,Resurrect,400)` twice + inn | the hash changes (healer block); re-record it with a stated reason. The In Mani Corp block (298-314) and the inn block are unchanged. |
| `batch7b_test.cpp` E37/E38 (lines 459-466, 560-562) | `resolve_refuge` over two 'D' members, `max_hp = 40`, XP 0, zero class/level, karma 40; asserts `current_hp == 40` | becomes 30 (XP 0 -> level 1, max 30): change the expected value (or give the members XP). |
| TS `game/tests/shops.test.ts:536-550` ("resurrect revive con 1 HP") | `freshState()` (INIT.GAM: Avatar A, INT 15, XP 150, L2/60, MP 0, karma 75, 3 members) | expected after: status G, exp 112, level 2, max 60, currentHp 60, currentMp 15 (gold 500 -> 300). |

**Must stay byte-identical (and are, by my analysis and B's regeneration):** every other `helpers.txt` row (70,342) and every other `flow.txt` row (583,748); the other 5,362 quest cases; `gameplay_parity` (0 of 4,802 expectations change: `check-gameplay.ts` has no `refuge-resolve`/`resolveRefuge`, its `resurrect` effect calls `applyResurrect` only); `magic_parity` (`fixtures/magic.txt`, committed; `generate-magic-fixtures.ts:35` uses exp 1000 and karma variant*3 through `applyResurrect`, which does not change); `advanced_combat_parity`; `command_parity` (`checkRefuge` is a stub); movement/transport/dungeon/item/combat flows; `blackthorn-run.ts` `refuge` scenario (returns revived/foodRefilled/karma/position/clock/allAlive, all unchanged) and `re/parity/blackthorn/refuge-*.json`, `re/parity/shops/healer.json` (DOSBox specs, prices only / revived-allAlive); In Mani Corp tests (`magic.test.ts:642-670`, `cast-onwho-consumidores.test.ts:186-192`); the ending (`endgame_scene` does not call the routine); the `Revive Party` cheat tests (`a4_enh2_rules_test.cpp` X3).
**Must stay green without edits (they assert only status 'G', HP == max, karma, location, `revived`):** TS `blackthorn.test.ts` (refuge), `espejo-es-momentos.test.ts:301-320`, `refuge-live.test.ts`, `refuge-pestillo-wipe.test.ts`, `capture-live.test.ts:294`; native `a3_hf9_refuge_cadence_test.cpp` C6.3, `a3_hf9_refuge_cadence_runtime_test.cpp` N4.2 (429), `batch53_release_blockers_test.cpp` K3, `a4_ui2_death_music_runtime_test.cpp`. Run the device host tests anyway.

---------------------------------------------------------------------------------------------

## 6. Test plan

**RED-first (write against the UNFIXED code; each must fail for the stated reason):**
* TS `shops.test.ts`: healer Resurrect on the INIT.GAM Avatar at karma 75, gold 500, price 200: expect status G, exp 112, level 2, max 60, HP 60, MP 15, gold 300. RED now: HP 1, exp 150, MP 0.
* TS and native, table-driven per path (healer and Refuge), class F and the A/B/M classes: karma {0, 1, 74, 75, 97, 98, 99, 100} x XP {0, 1, 99, 100, 199, 200, 450, 9999} from the section 2.7 table (all 144 table cells are executable truth): XP', level, max HP, HP = max HP, status G, MP by class (A INT 15 -> 15, M INT 22 -> 22, B INT 17 -> 8, B INT 255 -> 127, F unchanged).
* Refuge scenarios: (a) party of 3 'D', karma 50, A XP 0 / B XP 250 / M XP 800 -> section 2.7 worked rows, karma afterwards 75; (b) death karma 74, XP 100 member -> XP 74 (a mutant that floors first gives 75); (c) karma 99, XP 450, stored level 2/60 -> level 4, max 120, HP 120; (d) XP 0 member with stored L5/150 -> level 1, max 30, HP 30; (e) roster of 6, party size 3: members 3..5 untouched; (f) a synthetic mixed party ('D' XP 300 L3, 'P' HP 40 max 90): the 'D' member is resurrected and counted in `revived`, the 'P' member keeps status 'P' and gets HP 90.
* Agreement test (native): for the same record and karma, `resurrect_apply` through the spell, the healer and the Refuge leaves identical status/XP/level/max HP/MP and differs only in HP (1 vs max).
* No-RNG test: the RNG seed (digest already hashes it in shop_parity; assert it in the Refuge/healer unit tests) is unchanged across the call; healer order with Falsehood present stays pay -> drain -> resurrect.
* After the fix: `--check` control (typescript_shop_fixture_drift, typescript_shop_flow_drift and quest_parity MUST fail on the TS-only change; if any passes the fix is not reaching it), regenerate, token-diff proof (130 / 658 rows (98 sequences) / 15 cases, MP only), regenerated digests equal section 5's, then native: ctest `shop_parity`, `shop_flow`, `quest_parity`, `a4_enh2_preservation` (re-record B4), `batch7b` (E38), `magic_parity`, `a3_hf9_refuge_cadence`, `a3_hf9_refuge_cadence_runtime`, `a4_ui2_death_music_runtime`, `batch53_release_blockers`, `gameplay_parity`, `command_parity`.

**Mutations the suite must kill (one at a time):**

| # | Mutant | Killed by |
|---|---|---|
| M1 | healer/Refuge skip the routine (today's code) | every new row |
| M2 | HP set to 1 after the routine (or the routine sets HP = max, which would break the spell/scroll HP 1) | healer HP = 60 (Avatar row); In Mani Corp tests keep HP 1 |
| M3 | HP copied BEFORE the routine (old max) | B XP 250 L3/90 -> must be 60, M XP 800 L5/150 -> 120 |
| M4 | Refuge floors karma before the loop | death karma 74, XP 100 -> 74 not 75 |
| M5 | threshold off by one (`< 99`, `<= 98`, `< 97`, `<= 97`) | karma 97 and 98 at XP 100 (97 vs 100) and XP 199 (193 vs 199); karma 99 and 100 rows |
| M6 | level/max HP recomputed only when the cut ran | karma 99, XP 450, stored L2 -> L4/120 |
| M7 | max HP not recomputed or `30 * old level` | XP 0 with stored L5/150 -> 30 |
| M8 | rounding instead of truncation (or ceil) | XP 150 karma 75 -> 112 (112.5); XP 1 karma 97 -> 0 |
| M9 | MP rule: B uses INT (not INT >> 1), F zeroed, A/M untouched | B INT 17 -> 8; F old MP 5 stays 5; A INT 15 -> 15 |
| M10 | Refuge writes status 'G' for a non-'D' member | mixed party: 'P' member stays 'P' |
| M11 | Refuge HP := max only for revived members | mixed party: 'P' member HP 40 -> 90 |
| M12 | Refuge loops the roster instead of party_size | roster 6 / party 3 |
| M13 | a draw appears (RNG seed moves) on either path | seed assertion |
| M14 | healer charges karma or gold twice / changes price | existing shop_parity gold rows |
| M15 | `level` loop off by one (`> 1`, cap 8, table lookup) | XP 100 (L2), 12800/25600 if the test builds them (L9/L10; unreachable in play but a free boundary) |

Boundary rows to include exactly as computed: karma {0, 1, 97 (threshold - 1), 98 (threshold), 99 (threshold + 1), 100, 255}; XP around every level boundary (99/100, 199/200, 399/400, 799/800, 1599/1600, 3199/3200, 6399/6400, 9999); XP 0; the fixed point (XP 0, L1, max 30).

---------------------------------------------------------------------------------------------

## 7. Residual unknowns (exact)

1. **No dynamic witness.** Nothing was run in DOS/DOSBox. The routine, the four call sites and the HP overwrites are established statically and by executing the real bytes with print and ceremony stubbed. A DOSBox-X breakpoint at CAST2 0x05e0 (or In Mani Corp on a dead member in a real SAVED.GAM, or the healer's Resurrect, or a wiped party) would settle any residual doubt; the evidence that would falsify my reading is a different HP after the healer/Refuge than the post-routine max HP.
2. **Screen and sound leaves not modelled:** `print_string` (kernel 0x1850), the ceremony CAST2 0x0000 and the display driver pointer `lcall [0x5350]` were not executed; only their RNG-freedom (static closure) and their position in the sequence are established. The tone/flash timing of the healer jingle and the Refuge sweeps is out of scope (already modelled by other items).
3. **Idle RNG during key waits is real in 1988 and unmodelled by both ports:** kernel getkey 0x266c calls the idle tick 0x5910 (locations < 0x21 and > 0x7f) which can reach kernel 0x4552 and `rand` (and 0x2f62 wind changes); the healer's chooser (0x2e8e -> 0x2d7a -> 0x266c) and Y/N prompts therefore sit on the real-time idle stream. This is a pre-existing, documented real-time divergence (`re/deliberate-divergences.md` around lines 1207-1305), not part of D-83/D-84; it cannot change the routine, the Refuge loop or the payment drain, but it means a DOS capture of the healer dialogue cannot be compared draw-for-draw.
4. **XP >= 32768** (signed word) is derived and executed but unreachable through play (XP writers cap at 9999; the only door is an imported SAVED.GAM or a corrupt record via the PC save bridge). Whether to model it is a scope decision (section 3.3); evidence to settle "can an import produce it": none needed for parity.
5. **Whether any caller passes a mode other than 0, 1, 0xff:** the four sites are exhaustive and there is no indirect call, so none does; only 0 (silent, ceremony 8), 1 ("Not dead!", ceremony 6) and 0xff ("Not dead!", no ceremony) exist.
6. **Presentation items bundled in the same routine and NOT part of D-83/D-84** (decision, not binary question): scroll on a living target ("Not dead!" then "Failed!" + glide, scroll still consumed); ceremony effect 8/6 inside the routine on success only versus the ports' cast-time generic ceremony; ceremony missing for the scroll.
7. **`flow.txt` and `quest` counts come from B's scratch regeneration**, not from my own re-run of the generators: I verified that B's baseline equals the repo's files byte for byte, that B's patch is exactly section 3, that the 98-sequence count is reproduced by an independent derivation, and the quest diff from B's saved expectations; I did not run `node --import tsx` over the generators myself (read-only brief). The implementer's own regeneration will confirm the digests in section 5.
8. **The stale-note list in 1.2** is based on the passages I read (ALPHA4_UI.md 2769, 2827, 2838, 3334-3335; death-resurrection-audit.md; blackthorn.md ~299-306, 358-362, 390-394; shops.md 93-99; deliberate-divergences.md 621-625); other notes that quote "kernel 0xdc66" may exist (`re/notes/routine-census.json` was not read).
