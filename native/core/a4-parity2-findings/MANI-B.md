# MANI-B -- In Mani Corp on a LIVING target ("Not dead!", DS 0x953c)

Lens: ports first, then the binary independently. READ-ONLY investigation. Nothing in the repository was edited and no build or test was run. Everything labelled "binary" below was disassembled in this session with `re/tools/dis16.py` (capstone). Scratch scripts are in this folder (`census.py`, `rangecalls.py`, `basescan.py`).

---------------------------------------------------------------------------------------------------

## 1 Verdict

The claim in ALPHA4_UI.md section 11.15 is **half right**.

| Path | What the 1988 binary does on a LIVING target | Claim |
|---|---|---|
| **Scroll** (U)se -> In Mani Corp (index 6) | prints `Not dead!\n` (DS 0x953c), then the (U)se tail prints `Failed!\n` (DS 0x4a7b) and plays a glide tone. The scroll is already consumed. A turn is taken. No RNG draw. | CONFIRMED (and "Failed!" + tone follow, which the claim does not mention) |
| **Spell** (C)ast -> In Mani Corp (idx 42) | `resurrect_apply` is called with mode **0**, which makes it SILENT. The Cast tail prints `Failed!\n` (DS 0x4660) + the same glide tone. Spell charge and 8 MP are spent. A turn is taken. No RNG draw. NO "Not dead!". | REFUTED for the spell. The brief's "the resurrection spell, and its scroll" is wrong for the spell. |
| **Arena** (COMBAT.OVL) | The arena cannot reach `resurrect_apply` at all. Spell 42's time mask is 0x0e (no combat bit): `Not here!\n` + glide tone, nothing consumed. Scroll 6 in the arena: `Resurrection!\n` `Not here!\n`, scroll consumed. No "Not dead!" anywhere in COMBAT.OVL. | The arena does NOT have the message. |

The single discriminant is the mode word `[bp+4]` of `resurrect_apply` (CAST2.OVL 0x05e0): the spell pushes 0, the scroll pushes 1, the healer and the Refuge push 0xFF. "Not dead!" is printed only when `mode != 0` and the status byte is not 'D'.

DS 0x953c has exactly ONE referencing instruction in the whole install (`mov ax,0x953c` at CAST2.OVL 0x060a; a byte scan of every OVL and ULTIMA.EXE for the literal `3c 95` finds nothing else).

Both ports are wrong about the scroll on a living target (they print nothing after `Resurrection!`). Both ports are right about the spell (no "Not dead!", `Failed!` printed) -- the TypeScript reference even has a test pinning that (`game/tests/cast-onwho-consumidores.test.ts`).

Adjacent divergences found while doing the work, NOT part of the "Not dead!" fix but in the same scenario (section 5.4 and 8):
- the spell's ceremony (`CAST2:0x0000`, index 8) plays in both ports BEFORE and regardless of the apply; the binary plays it only inside `resurrect_apply`, on success. On a living target the ports flash; the original does not;
- on a dead target the scroll should play ceremony index 6 (inside `resurrect_apply`); both ports have "no ceremony" for scroll 6;
- cancelling the "On who:" prompt: the binary has already spent the scroll / spell charge / MP; the T-Deck runtime refunds all three.

---------------------------------------------------------------------------------------------------

## 2 Binary facts

### 2.0 Tooling controls and a warning about the tooling

Positive controls run first:
- The thunk table reproduces a known entry: `re/tools/thunks.py` lists `thunk 0x7ef6` -> ovl 18, target 0xe7c0 = base 0xE1E0 + 0x05E0, i.e. CAST2.OVL 0x05E0 = `resurrect_apply`. At CAST2.OVL 0x05e0 the prologue `55 8b ec 83 ec 08` is there and the function prints DS 0x953c. This also matches the note chain in `re/notes/cast-dispatch-48-brazos.md` (arm 42).
- RNG control: scanning CAST.OVL 0x4a0..0x5c0 for near calls that resolve to kernel 0x2092 (rng_rand_range) finds `04c6 call raw 6112 -> kernel 2092`, which is the Kal Xen `rand_range` the notes cite. So `rangecalls.py` does resolve RNG calls.

**Overlay load bases, measured** (script `basescan.py`: for each candidate base, count near calls whose resolved target is a kernel `55 8b ec` prologue or a thunk start `9a ec 02 2e 07`):

| file | measured best base | hits | note |
|---|---|---|---|
| CAST.OVL | **0xBF80** | 270 / next best 124 | NOT 0xA8D8 (brief, and `re/tools/callers_banda.py` BASES) |
| CAST2.OVL | **0xE1E0** | 186 / next 69 | NOT 0xC29E (the `callers_banda.py` BASES) |
| SHOPPES.OVL | 0xA290 | 228 | agrees |
| COMBAT.OVL | **0xA290** | 160 of 210 calls; **0 of 210** at 0xBFEC and at 0xBF80 | the brief's 0xBFEC does NOT fit COMBAT.OVL |
| BLCKTHRN.OVL | **0xA290** | 132 of 159; **0 of 159** at 0xE63E and 0xE1E0 | the brief's 0xE63E does NOT fit BLCKTHRN.OVL |

Consequence for the implementer: `re/tools/callers_banda.py 0x7ef6` prints ONLY `SHOPPES.OVL 0x16f5`. It misses both CAST.OVL callers (wrong CAST base) and the BLCKTHRN.OVL caller (wrong base). My own census (`census.py`, every OVL tried at every base in the thunk-table set plus 0; over-reports, never misses) is in section 3. A hit of `census.py` at a base that does not fit the file (I listed those) is spurious and was discarded.

Kernel addresses resolved this way (kernel = (raw target + base) & 0xFFFF):
- print_string: kernel 0x1850 (CAST2 raw 0x3670 at 0xE1E0; CAST.OVL raw 0x58d0 at 0xBF80; SHOPPES/BLCKTHRN/COMBAT raw 0x75c0 at 0xA290). I disassembled the head of 0x1850 (reads a NUL-terminated string at `[bp+4]`).
- kernel 0x16ba = putchar (head: `cmp dl,0x0a` ...); kernel 0x1f12 returns `[[0x539a]+4]` (a window flag byte); kernel 0x43ae = `glide(start,end,step,total)` (note `re/notes/audio-diff-calibration.md`), prologue `55 8b ec 83 ec 08` seen.
- kernel 0x2900 = status-panel redraw (CAST.OVL raw 0x6980). Not decoded further, UI only.

### 2.1 The strings (DATA.OVL, file offset = DS offset + 0x10)

| DS | file off | bytes | meaning |
|---|---|---|---|
| **0x953c** | 0x954c | `"Not dead!\n\0"` (read: `b'Not dead!\n'`) | printed by resurrect_apply, mode != 0 only |
| 0x94f4 | 0x9504 | `"On who: \0"` | CAST2 0x009e prompt |
| 0x94fe | 0x950e | `"None!\0"` | printed in place of the name when the picker is cancelled |
| 0x46d2 | 0x46e2 | `"Resurrection!\n\0"` | scroll 6 header |
| 0x46e1 | 0x46f1 | `"Not here!\n\0"` | scroll 6 in the arena |
| 0x466a | 0x467a | `"Scroll\n\n\0"` | (U)se scroll category word |
| 0x4a7b | 0x4a8b | `"Failed!\n\0"` | (U)se tail |
| 0x4656 / 0x4660 | | `"Success!\n"` / `"Failed!\n"` | Cast tail (two distinct copies of "Failed!") |
| 0x462f | 0x463f | `"Not here!\n\0"` | Cast window gate |
| 0x4647 | | `"M.P. too low!\n\0"` | Cast MP gate |
| 0x463a | | `"None mixed!\n\0"` | Cast mixed-count gate |

### 2.2 `resurrect_apply` = CAST2.OVL 0x05e0, `ret 4`, stdcall-ish: `[bp+6]` = slot, `[bp+4]` = mode

Disassembly (CAST2.OVL file offsets = CS offsets inside the overlay):

```
05e0 55 / 05e1 8bec / 05e3 83ec08 / 05e6 56
05e7 837e0600        cmp word ptr [bp+6],0            ; slot (signed word)
05eb 7d09            jge 0x5f6
05ed c746faffff      mov word ptr [bp-6],0xffff       ; result = -1 (cancelled picker)
05f2 e9ec00          jmp 0x6e1
05f6 8b5e06 / b105 / d3e3                              ; bx = slot << 5  (record size 0x20)
05fd 80bfb3554 4     cmp byte ptr [bx+0x55b3],0x44    ; status byte (record+0x0b) == 'D' (0x44) ?
0602 7416            je 0x61a                         ; dead -> go resurrect
0604 837e0400        cmp word ptr [bp+4],0            ; mode (word)
0608 7407            je 0x611                         ; mode == 0 -> silent
060a b83c95          mov ax,0x953c
060d 50
060e e85f30          call 0x3670                      ; print_string("Not dead!\n")
0611 c746fa0000      mov word ptr [bp-6],0            ; result = 0
0616 e9c800          jmp 0x6e1
```

Decisions: the compare is byte-exact ('D' only; 'G','P','S' and anything else -> not dead); the mode compare is on the full WORD (the callers push 0, 1 or the word 0x00FF). No RNG call, no clock call and no other state write happens on the not-dead path. Result is 0 (not -1).

Dead path (for completeness, boundary tests need it):
```
061a-0627  bx = (slot<<5) + 0x55a8 ; [bp-2] = bx            ; record base 0x55a8 + slot*0x20
0629 c6470b47        mov byte [bx+0x0b],0x47          ; status = 'G'
062d c747100100      mov word [bx+0x10],1             ; HP (+0x10) = 1
0632-0648            class byte [bx+0x0a]: 'A'(0x41) or 'M'(0x4d): MP(+0x0f) = byte [bx+0x0e] (INT)
                                              'B'(0x42): MP = INT >> 1 ; otherwise MP untouched
064e 803e885862      cmp byte [0x5888],0x62 ; jae 0x678    ; karma >= 98 -> no cut
0655-066f            exp(word +0x14) = exp * karma / 100  (long mul 0x2262, long div 0x22b6; karma byte zero-extended)
0678-06a7            [bp-8]=1; cx = exp/100 (signed idiv by 0x64); loop dx=1; while(cx>0){dx++; cx>>=1 (sar)}
06b3                 byte [bx+0x16] = dl                  ; level
06b6-06bb            word [bx+0x12] = 0x1e * dx           ; max HP
06be 837e0401 / 75 06 / b80600 / eb 0a    mode == 1 -> push 6
06ca 837e0400 / 75 07 / b80800            mode == 0 -> push 8      ; mode 0xFF -> neither
06d4 e829f9          call 0x0000                      ; CAST2:0x0000 ceremony(6 or 8), SUCCESS PATH ONLY
06d7 c746fa0100      mov word [bp-6],1                ; result = 1
06dc c606faa901      mov byte [0xa9fa],1              ; dirty flag
06e1 mov ax,[bp-6] ... ret 4
```
No `rand` call anywhere in 0x05e0..0x06e8 (control: `rangecalls.py CAST2.OVL 0xe1e0 0x5e0 0x6ec` lists only the print, the long mul/div and the ceremony call; one false "call raw b38a" at 0x69c is the second byte of `d1 e8`, a byte-scan artefact).

### 2.3 The four callers and the mode each pushes

| caller | address | pushes | mode | meaning |
|---|---|---|---|---|
| Spell dispatcher arm 42 | CAST.OVL 0x10ec-0x10fc | `call 0xffffc1aa` (stub 0x812a -> CAST2 0x009e "On who:"); `push ax`; `sub ax,ax; push ax`; `call 0xffffbf76` (stub 0x7ef6) | **0** | `10f6 mov [bp-0xa],ax`, `10f9 call 0x6980` (panel), `10fc jmp 0x11a6` (Cast tail) |
| Scroll 6 | CAST.OVL 0x12d8-0x12f7 | `push 0x46d2; call print`; `cmp byte [0x5893],0x80; jae 0x12fa`; `call 0xffffc1aa; push ax; mov ax,1; push ax; call 0xffffbf76` | **1** | `12f1 mov [bp-2],ax`; `12f4 call 0x6980`; `12f7 jmp 0x1350` |
| Healer (Resurrect) | SHOPPES.OVL 0x16f1-0x16f5 | `push [bp-6]; mov ax,0xff; push ax; call 0xffffdc66` | **0xFF** | member chosen at 0x16a1; `cmp byte [bx+0x55b3],0x44; je 0x16bf` else `jmp 0x15d4`: a living member never reaches the call |
| Refuge | BLCKTHRN.OVL 0x0b90-0x0b95 | `push si; mov ax,0xff; push ax; call 0xffffdc66` | **0xFF** | loop `si = 0 .. [0x585b]` (party size) 0xb54-0xbb1; I found NO status filter before the call; whether every member is already 'D' on entry is not established (section 8) |

`0xffffbf76` in CAST.OVL = (0xbf76 + 0xBF80) & 0xFFFF = 0x7ef6 (thunk). `0xffffdc66` in SHOPPES/BLCKTHRN = (0xdc66 + 0xA290) & 0xFFFF = 0x7ef6.

### 2.4 Path A: the spell, CAST.OVL 0x0dba `cast_command_dispatch` (then arm 42)

Order (CAST.OVL file offsets):
1. 0x0dc1-0x0dd2: `[bp-6]=1; [bp-0xc]=1; [bp-0xa]=1`; DS 0x588f/0x5890 = 1.
2. 0x0dd5 `call 0x8a08` (caster select, `[bp-4]`); `or ax,ax / jge` else `jmp 0x11d9`.
3. 0x0de2 print "Spell name:" (0x4603); 0x0de9 `call 0xffffc10e` -> spell index `[bp-2]`; -1 -> "None!" 0x4611; -2 -> "No effect!" 0x4618.
4. 0x0e0a-0x0e17: `[bp-8] = idx/6 + 1` (signed idiv by 6) = circle = **8 for idx 42** (42/6 = 7, +1).
5. Window gate 0x0e1a-0x0e8e. Mask byte = DATA.OVL DS 0x1c90 + idx. Measured: **DS 0x1c90[42] = 0x0e** (file offset 0x1ca0+42; full table dumped: idx 42 -> 0xe). Bits: `loc==0 -> bit 8 (0x0e21 test ...,8)`; `loc > 0x7f -> bit 1 (0x0e36 test ...,1)`; `0x12 with [0x57b4]==0` or `0x1d` -> "Absorbed!" (0x0e53); `loc < 0x21 -> bit 4`; else bit 2. So In Mani Corp is castable in the overworld (8), towns (4), dungeons (2) and NOT in the arena (1 clear). Failure path 0x0e9b: print "Not here!\n" (0x462f), `glide(0x32, 1, 0x7d0, 0x320)` (0x0ea2-0x0eb2, `call 0x842e` = kernel 0x43ae), `jmp 0x11d9`. **Nothing is consumed on this gate.**
6. 0x0eb8-0x0ec8: `cmp byte [idx+0x57f0],0` -> "None mixed!" (0x463a) else `dec byte [idx+0x57f0]`: **the mixed-spell charge is spent here** (before the MP and level gates).
7. 0x0ecc-0x0eea: MP byte `[caster*0x20 + 0x55b7]` vs circle: `cmp ax,[bp-8]; jae 0x0eee`; else print "M.P. too low!" (0x4647) and `[bp-0xa]=0 -> 0x11a6` (tail prints "Failed!").
8. 0x0eee-0x0f0a: **`sub [si+0x55b7],al` -> MP -= 8 (circle)**; `[bp-0xa]=0xffff`; level byte `[si+0x55be]` (record+0x16) vs circle: `cmp ax,[bp-8]; jb 0x0ee5` -> `[bp-0xa]=0`, tail ("Failed!", no prompt, no target).
9. 0x0f0c-0x0f1a: `cmp ax,0x2f; jbe` then `jmp word ptr cs:[bx-0x2f3a]`, jump table at 0x1146 (48 words, CS offsets).
10. Arm 42 (table word, file offset 0x10ec): see section 2.3.
11. Tail 0x11a6-0x11dc: `cmp [bp-0xa],1; jne 0x11b6` -> print DS 0x4656 "Success!\n"; `0x11b6 cmp [bp-0xa],0; jne 0x11d6` -> print DS 0x4660 "Failed!\n" then `glide(0x32,1,0x7d0,0x320)` (0x11c3-0x11d3); `0x11d6 mov ax,[bp-6]` = **1** -> return.

So for a living target: result 0 -> `Failed!\n` + glide. For a dead target: result 1 -> `Success!\n`, and the ceremony(8) played inside resurrect_apply. For a cancelled picker: result -1 -> the tail prints NOTHING (neither compare matches), mana and charge already spent.

### 2.5 Path B: the scroll, CAST.OVL (U)se handler 0x1792 -> scroll reader 0x11de

(U)se handler 0x1792: `[bp-0xa]=1` (0x179a); item picker; id 0..7 -> `push id; call 0x11de` (0x1813-0x1816, the ONLY caller of 0x11de: census `kernel 0xd15e`); `mov [bp-0xa],ax` (0x1819); `jmp 0x1b8a`. Common tail 0x1b8a-0x1baf: `cmp [bp-0xa],0; jne 0x1baa` else print DS 0x4a7b "Failed!\n" + `glide(0x32,1,0x7d0,0x320)` (0x1b97-0x1ba7).

Scroll reader 0x11de (`ret 2`, `[bp+4]` = scroll index 0..7, `[bp-2]` result initialised to 1 at 0x11e4):
```
11e9 8b5e04 ; 11ec fe8f2058  dec byte [bx+0x5820]        ; scroll count, UNCONDITIONAL, before anything
11f0 b86a46 ; call print     ; "Scroll\n\n" (DS 0x466a)
11f7-1202   cmp idx,7 ; ja 0x1350 ; add ax,ax ; xchg bx,ax ; 1205 jmp word cs:[bx-0x2d40]   ; table at file 0x1340
```
Jump table @0x1340, read: idx0 0x120a, 1 0x1222, 2 0x124a, 3 0x1264, 4 0x1278, 5 0x12b4, **6 0x12d8**, 7 0x1300.

Handler 6 (0x12d8):
```
12d8 b8d246 ; push ; call print         ; "Resurrection!\n" (0x46d2)  -- printed BEFORE the prompt and before any gate
12df 803e935880  cmp byte [0x5893],0x80 ; 12e4 jae 0x12fa
12e6 call 0xffffc1aa                    ; "On who: " + name (CAST2:0x009e)  -> ax = slot or -1
12e9 push ax ; 12ea mov ax,1 ; push ; 12ee call 0xffffbf76   ; resurrect_apply(slot, mode=1)
12f1 mov [bp-2],ax ; 12f4 call 0x6980 ; 12f7 jmp 0x1350
12fa b8e146 ; jmp 0x1289 -> 128a call print ("Not here!\n") ; 128d jmp 0x1350      ; arena
1350 mov ax,[bp-2] ... ret 2
```
Resulting scroll-on-living-target transcript (exact order): `Scroll\n\n` -> `Resurrection!\n` -> `On who: ` + member name (line break only if the window flag at `[[0x539a]+4]` is set: CAST2 0x00c8-0x00d3 `call kernel 0x1f12; or ax,ax; je; push 0xa; call 0x16ba` = putchar('\n')) -> `Not dead!\n` -> (panel redraw) -> `Failed!\n` + glide. Scroll count decremented at 0x11ec, i.e. BEFORE the prompt. [bp-2] = 0 -> returned; (U)se tail prints "Failed!".

Return/turn: ULTIMA.EXE command dispatcher (0x3178) initialises `[bp-2]=1` (0x317e); 'U' arm 0x340c: `push 0xa24c; print ("Use item"); call 0x7e42 (thunk -> CAST.OVL 0x1792); jmp 0x31ee` -> returns `[bp-2]` = 1: the overlay's return is DISCARDED (`re/notes/command-dispatch.md` section 6-bis: only A, B, C, E, K, Y propagate). 'C' arm 0x3242: print "Cast..." (0xa142), `call 0x7e5a`, `jmp 0x3231` -> `[bp-2]` = the dispatcher's AX = `[bp-6]` = 1 (arm 42 never writes `[bp-6]`). The overworld loop MAINOUT 0x0c0f-0x0c36 does `mov [bp-8],ax; ... cmp [bp-8],0; jne 0x0c39 (world tick); jmp 0x0d14 (skip)`. **So both the spell and the scroll take a turn, living or dead target, cancelled or not.** (The world-tick body at 0x0c39 starts `push 2; call 0xffffcdac` = kernel 0x4f7c, not decoded here; the town and dungeon loops use their own tails, TOWN `[bp-0xa]=1`-style, not decoded.)

RNG: none. No rand call in the dispatcher 0x0dba-0x11de up to arm 42 (CAST.OVL scan: 0 hits), none in the scroll reader 0x11de-0x135a (0 hits), none in resurrect_apply. (The (U)se handler has one `rand_range` at 0x1899 on another item's branch; irrelevant.)

### 2.6 Path C: the arena (COMBAT.OVL base 0xA290)

- Arena Cast: `COMBAT.OVL 0x095e call 0xffffdbca` -> (0xdbca + 0xA290) = **0x7e5a** = the SAME shared dispatcher thunk (CAST.OVL 0x0dba). Census for 0x7e5a: COMBAT.OVL 0x095e and ULTIMA.EXE 0x3249 only. Therefore gate step 5 applies with `loc > 0x7f` (bit 1); mask 0x0e has no bit 1: "Not here!" and nothing consumed.
- Arena (U)se: `COMBAT.OVL 0x544` is the arena's shared context-command function (`push [bp+6]` verb string, print; `test byte [actor*8 - 0x45ea],0x80` else "Can't!" 0x6d98 and return 1; then `switch [bp+4]` 0..5 -> thunks 0x7e06 Get, 0x7e12 Jimmy, 0x7e1e Open, 0x7e4e Ready, 0x7e2a Search, **0x7e42 Use** (0x059e `call 0xffffdbb2`); every dispatched case returns AX = 0 via `sub ax,ax` at 0x5b0). Use is entered at 0x09ac..0x09ba (`push 0x6e42 "Use item\n\n"; push 5; jmp 0x970; call 0x544`). Census for 0x7e42: COMBAT.OVL 0x059e and ULTIMA.EXE 0x3413. So the arena's scroll is the SAME 0x11de reader with `[0x5893] >= 0x80`: scroll 6 prints `Resurrection!\n` then `Not here!\n`, no target prompt, scroll consumed (0x11ec), result stays 1 (no "Failed!").
- DS 0x953c is not referenced from COMBAT.OVL (byte scan). resurrect_apply is unreachable from the arena.
- Not settled: how the arena loop converts the AX = 0 return of 0x544 into a turn charge (0x07ba common tail `cmp [bp-4],0 ...`). It does not matter for this item (the arena path never reaches the target, only a "Not here!" with a consumed scroll), see section 8.

---------------------------------------------------------------------------------------------------

## 3 Callers census

All by `census.py` (`E8/E9 rel16` at every offset in every OVL under every base in the thunk-table set, plus ULTIMA.EXE at base 0; over-reports, never misses). Positive control: the scan finds the known SHOPPES 0x16f5 call and the CAST.OVL calls that I also read by hand.

**resurrect_apply (kernel thunk 0x7ef6 -> CAST2.OVL 0x05e0):**

| hit | base that fits the file | verdict |
|---|---|---|
| CAST.OVL 0x10f3 (base 0xBF80) | yes | **spell** (mode 0), relevant |
| CAST.OVL 0x12ee (base 0xBF80) | yes | **scroll** (mode 1), relevant |
| SHOPPES.OVL 0x16f5 (base 0xA290) | yes | healer (mode 0xFF), only dead members reach it, not relevant to "Not dead!" |
| BLCKTHRN.OVL 0x0b95 (base 0xA290; measured base fits, see 2.0) | yes | Refuge (mode 0xFF), loop over the party, status filter not found |
| ULTIMA.EXE | none | no direct caller |

Local calls inside CAST2.OVL to 0x05e0 (kernel 0xe7c0): none. Literal references to 0x7ef6 / 0xe7c0 / 0x05e0 as data or immediates: none that are real (the 0x05e0 hits are all `d3 e0 05 xx` = `shl ax,cl; add ax,imm`; the 0x7ef6 hits in CMDS/FONT/INTRO/SHOPPES2/EXE are inside unrelated instructions: `or si,si; jle`, `cmp ...`). No indirect call table. Total: **four real callers**, matching ALPHA4_UI.md line 2769.

**CAST.OVL 0x0dba cast dispatcher (thunk 0x7e5a):** COMBAT.OVL 0x095e (arena Cast), ULTIMA.EXE 0x3249 ('C' key). Only these. **CAST.OVL 0x1792 (U)se (thunk 0x7e42):** COMBAT.OVL 0x059e (arena Use), ULTIMA.EXE 0x3413 ('U' key). **Scroll reader 0x11de:** CAST.OVL 0x1816 only. **DS 0x953c:** CAST2.OVL 0x060a only. TOWN/DUNGEON do not have their own Use/Cast: the TOWN/MAINOUT/DUNGEON loops all hand non-control keys to the kernel dispatcher 0x3178 (`re/notes/command-dispatch.md` section 5), so town and dungeon scrolls behave exactly like the overworld ones apart from the location tests.

---------------------------------------------------------------------------------------------------

## 4 Reference (TypeScript) status

- `game/src/core/magic/cast.ts:545` `applyResurrect(target, karma): boolean` -- matches `resurrect_apply`'s dead path (status, HP 1, MP by class, karma cut, level loop, max HP 30 x level); returns false (no state change, no RNG) when `status !== "D"`. Correct as the state transition. `cast.ts:292` (`case 42`) returns `{ kind: "resurrect" }`; `cast.ts:540` header comment cites `CAST2:0x05e0`.
- `game/src/core/useScroll.ts:90-97` `readScroll`, case 6: pushes `"Resurrection!"` (DS 0x46d2) and for `location >= 0x80` also `"Not here!"` (DS 0x46e1), else returns `followup: { kind: "resurrect" }`. Correct as far as it goes. It has no message for the not-dead outcome because the target is picked later, in `main.ts`.
- `game/src/main.ts:4856-4885` (world scroll): decrements the scroll first (4859), prints messages, `emitCeremony(scrollCeremonyIndex(idx))` (4874, index null for 6), then for `fu.kind === "resurrect"` (4879-4883) `pickCastTarget((i) => { applyResurrect(game.state.characters[i], game.state.karma); })` with the comment "sin Success/Failed; el eco es Resurrection!". **Divergence:** the boolean result is discarded, so a living target gets no "Not dead!" and no "Failed!" (and no tone). A cancelled picker is handled by `pickCastTarget` (scroll already spent, matches the binary).
- `game/src/main.ts:4996-5018` `applyCombatScroll` (arena): decrements, prints "Scroll" + `readScroll(.., 0x80)` messages (`Resurrection!`, `Not here!`), `combatOut(cb.playerCast(null, null))` (turn). Matches the binary. (Its header comment claims "Vía (U)se el juego nunca está en combate" in `useScroll.ts` line 15; the binary shows COMBAT.OVL 0x059e calls the same Use thunk, so that comment is wrong, harmless here.)
- `game/src/main.ts:4448-4474` (overworld Cast), `4612-4621` (dungeon Cast), `3316-3340` (arena Cast, effectively dead code for In Mani Corp because `castSpell` refuses it with "Not here!" via mask 0x0e): `pickCastTarget(... hud.message(applyResurrect(m, karma) ? "Success!" : "Failed!"))`. Correct for the spell (matches tail 0x11a6). The comment block at 4449-4470 independently derives the mode-0/mode-1 split and says "Not dead!" must NOT appear in the Cast path. **Divergences (adjacent):** the generic `if (r.ok) emitCastCeremony(def.index)` (4422 overworld, 4550 dungeon, 3270 combat) fires the circle-8 ceremony before the apply, on a living target too; no "invalid-magic"-style tone follows "Failed!" (the binary plays `glide(0x32,1,0x7d0,0x320)`).
- `game/src/core/magic/ceremony.ts:123`, `:140` `SCROLL_CEREMONY_INDEX = [0,null,2,3,4,null,null,7]`: scroll 6 = null ("sin ceremonia"). The note rests on CAST.OVL 0x12d8-0x12f7 containing no `call 0xffffc186`; true, but `resurrect_apply` (the callee) calls `CAST2:0x0000` itself with index 6 (0x06c4-0x06d4) on success. See section 8.
- Parity harness: `game/src/core/__parity__/magic-run.ts:162` `case "resurrect": applyResurrect(target, state.karma)` feeds `generate-magic-fixtures.ts`.

## 5 Native status

### 5.1 Scroll 6, world / town / dungeon: `native/core/src/world_magic.cpp`

- Line 27: `if(g.scroll_quantities[cmd.item]>0){--g.scroll_quantities[cmd.item];}say("Scroll");` (consume-first, matches 0x11ec; the `>0` guard is native-only and harmless).
- **Line 35:** `case 6:say("Resurrection!");if(auto *p=target())apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand);break;` -- the bool is discarded. **Divergence:** a living target gets neither "Not dead!" nor "Failed!" nor a tone.
- Line 37: `static constexpr int8_t indices[8]={0,-1,2,3,4,-1,-1,7};` -> no ceremony for scroll 6, and the `ceremony(...,"scroll-used")` emission is skipped.
- No `location >= 0x80` branch in this function: the arena has its own switch (below). `UseItem` for ids < 16 reaches this through `commands.cpp:1074-1075` (world) and `dungeon_orchestration.cpp:158-160` (dungeon); both ignore `WorldCommandResult::turn` for UseItem (see 5.5).
- Device wiring `native/targets/tdeck/main/alpha_runtime.cpp:1395-1396`: outside combat, item 6 (and potions 8-15) open `PartySelection` with `UiRequestId::UseTarget`; the accepted pick re-dispatches `UseItem` with `c.member`. Cancel: `AlphaRuntime::modal` (line 1375) only clears `pending_use_item_` and dispatches nothing: **the scroll is NOT spent on a cancelled picker** (binary and TypeScript spend it).

### 5.2 Scroll 6, arena: `native/core/src/combat.cpp`

- Line 1160: `case 6:say("Resurrection!");say("Not here!");break;` after `--c.game.scroll_quantities[item]` (line ~1156) and `say("Scroll")`. Matches the binary exactly (consumed, no prompt, no "Failed!"). Line 1162 `indices[8]={0,-1,2,3,-1,-1,-1,7}`. `e.advance()` charges the combat turn (binary: not decoded, section 8).

### 5.3 Spell, all contexts: `native/core/src/magic.cpp` + `world_magic.cpp` + `dungeon_orchestration.cpp` + `combat.cpp`

- `magic_tables.inc:45` In_Mani_Corp: target `"selectedCombatPlayer"`, circle 8, time_bits 14. `magic.cpp:160-172` `cast_spell`: absorbed test, window test `time_bits & bit` (bit 1 in combat or location >= 128, 8 outdoors, 4 for location <= 32, else 2) -> "Not here!" (matches mask 0x0e), `None mixed!`, `--q`, MP check (`consumed=true` "M.P. too low!"), `current_mp -= circle`, level check (`consumed=true`, ""), then the effect. Same gate order and same consumption points as 0x0e1a-0x0f0a. Resurrect returns `{true,true,"",fx}` with no RNG (control: only Food/Summon/Wind/Mani draw).
- `magic.cpp:207-250` `apply_target_spell(..., Resurrect)`: state transition equals 0x061a-0x06bb (`status != 'D'` -> false; `char class` 'A'/'M'/'B' rules; `karma < 98` cut with `int(p.exp) * karma / 100` on a uint16 exp; level loop; max HP 30 x level). No RNG.
- `world_magic.cpp:64` (and `dungeon_orchestration.cpp:182`, `combat.cpp:1086-1093`): `say(apply_target_spell(...) ? "Success!" : "Failed!")`. Correct text for the spell (no "Not dead!"). **Adjacent divergences:** (i) `world_magic.cpp:47` `ceremonial` includes idx 42, so `ceremony(circle=8,"spell-cast")` is emitted BEFORE the apply and for a living target; binary: ceremony(8) only on success inside resurrect_apply; (ii) no failure tone event after the apply's "Failed!" (the `invalid-magic` Sfx is emitted only on `!cast.ok`, line 46; audio.h:44 says invalid-magic "has no adjudicated original sound" yet the binary shows `glide(0x32,1,0x7d0,0x320)` at CAST.OVL 0x11c3-0x11d3 and 0x0ea2-0x0eb2 and 0x1b97-0x1ba7).
- Device wiring `alpha_runtime.cpp:1528-1533` + `1408`: for target type `"selectedCombatPlayer"` the picker opens BEFORE `cast_spell` is evaluated (also in the world and in the arena), so the player is asked "who" before "Not here!" / "None mixed!" / the level gate; the binary evaluates all gates first and only then prompts (0x0e1a-0x0f0a, then the arm's `call 0xffffc1aa`). Cancel of that picker (`modal`, line 1375: `pending_combat_spell_=-1`) dispatches nothing: **no spell charge and no MP are spent**, while the binary has already spent both (steps 6 and 8) and prints nothing (result -1).

### 5.4 "On who:" echo

Native never prints `"On who: "` (DS 0x94f4) or the member name for In Mani Corp; the picker is a device PartySelection. The new "Not dead!" line is therefore the only text between "Resurrection!" and "Failed!" in native; section 2.5 gives the exact original transcript for the record.

### 5.5 Turn

`WorldCommandResult::turn` defaults false (`world_commands.h:5-6`); `world_magic` returns `{}` for scrolls and target spells, `commands.cpp:1074-1075` stores only `a.status` for UseItem, and `commands.cpp:946-950` calls `r.turn()` for Cast only `if(action.turn)`. As read, the world Use and world Cast of this item therefore tick no turn, whereas the binary ticks one (section 2.5). `re/notes/use-merchants.md:52` records this as known for the TypeScript port ("El (U)se no rueda turno en el port"). I did not execute anything to confirm; the dungeon Cast does tick (`dungeon_orchestration.cpp:178` `run_dungeon_command(tick)`). This is a separate item and the MANI fix must not rely on it.

---------------------------------------------------------------------------------------------------

## 6 Fixtures and corpora that pin it, and what would change

**Would NOT change if the fix is made at the message layer (recommended):**
- `native/core/fixtures/magic.txt` + `native/core/tests/magic_parity_test.cpp` (ctest `magic_parity`) + `native/core/tools/generate-magic-fixtures.ts` (ctest `typescript_magic_fixture_drift`) + `game/src/core/__parity__/magic-run.ts:162`: they call `cast_spell` and `apply_target_spell` / `applyResurrect` directly over statuses "GPSD" and compare the boolean, HP/MP/level/exp/status and the RNG draw stream. They contain no message text for the apply. **Keep `apply_target_spell` / `applyResurrect` signatures and bodies unchanged** (do not turn the bool into a tri-state; add the message in the callers). Otherwise the magic fixture, the C++ test and the TS generator all need regeneration.
- `native/core/fixtures/items.txt` / `generate-item-fixtures.ts` only uses `UseItem` ids 34, 35, 37 (line 129).
- `native/core/fixtures/commands.txt` has no Cast/Use rows.

**Would change or must be extended:**
- `native/core/tests/batch13_test.cpp` (ctest `batch13`): the world fixture's member 0 is LIVING (`status='G'`, line ~156) and `use(i,0)` runs scroll 6 for i = 6. A2 (lines ~283-291) checks only first line "Scroll" and "no name echoed" -- stays green. D1 (lines ~397-402) asserts `ceremony_index(ev) == kWorldScrollCeremony[6] == -1` for that living member -- stays green for the "Not dead!" fix (no ceremony on failure). It would go red only if a ceremony were added to the success path AND the test member were dead. E3/E1/E2 do not touch scroll 6. `action_feedback_test.cpp` (scroll 0 only), `batch5_test.cpp` (scrolls 1 and 17) do not touch it.
- `game/tests/use-scroll.test.ts:79-90` pins `readScroll(.., 6, ..).messages` = `["Resurrection!"]` and `["Resurrection!","Not here!"]`, followup `{ kind: "resurrect" }`. Unchanged if the "Not dead!" is emitted by `main.ts` after the picker (the target is not known inside `readScroll`).
- `game/tests/cast-onwho-consumidores.test.ts`: lines ~151-170 assert the CAST branch of `main.ts` does NOT contain "Not dead!" or "Resurrection!" and DOES contain `'"Success!" : "Failed!"'`, `applyResurrect(`, `pickCastTarget(` -- these must stay true (correct per the binary). Lines ~172-180 assert that the `USE_SCROLL` block (source between `hud.messageAppend("Scroll")` and `if (a.kind === "potion") {`) contains `fu.kind === "resurrect"` and `applyResurrect` and does NOT contain the text `"Success!" : "Failed!"` -- a scroll fix must print "Failed!" with its own call (for example `hud.message("Failed!")`), not that ternary, or the test goes red.
- `game/tests/fixtures/approved-strings.json` (consumed by `game/tests/espejo-3h-colisiones.test.ts`, and the i18n/census guards `display-consts-censo.test.ts`): it has "Resurrection!" (line 667), "Success!" (733), "Failed!" (417), but NO "Not dead!" entry. A new `hud.message("Not dead!")` needs an entry with the citation "DS 0x953c (file 0x954c) `Not dead!\n`, CAST2.OVL 0x060a, mode != 0 (scroll CAST.OVL 0x12ee)". Which of those guards actually fail on a missing key I did not run.
- `game/tests/videocap-gate.test.ts` / `game/tools/videocap/*` (`resurreccion-hechizo-shader`: needs "Resurrection!" and "On who:"): these are port recordings (`origen: port`/`sidecar`), not original footage; the recorded take uses a DEAD Shamino, so the living-target text is not exercised. Not a witness.
- No fixture, corpus or recorded original footage in the repo exercises a living target for either path. The `re/notes` claims that touch it are `magic.md:160` ("mode!=0: Not dead!"; correct, cites the non-existent `re/disasm/CAST2.OVL.asm`) and `diff-ocr-masivo.md:39` (lists "Not dead!" as a string in the combat/cast text pool; the pool is the DATA.OVL neighbourhood at DS 0x953c, not evidence that it is printed in combat).

## 7 Proposed fix and boundary test cases

### 7.1 The fix (minimum, parity-only)

TypeScript reference (fix first, at the layer the fixtures pin: the `main.ts` scroll branch, not `applyResurrect`):
1. In `main.ts` world scroll (4879-4883), inside the picker callback: `const m = chars[i]; if (m.status !== "D") { hud.message("Not dead!"); hud.message("Failed!"); /* + failure tone cue */ } else { applyResurrect(m, karma); }`. Use a distinct call for "Failed!" (see the `USE_SCROLL` guard above). Do not touch `readScroll` (target unknown).
2. Add `"Not dead!"` to `approved-strings.json`.
3. Leave the Cast branches (4448, 4612, 3316) as they are: they are correct (no "Not dead!").

Native, mirroring it at the same layer:
1. `world_magic.cpp:35`: `case 6: say("Resurrection!"); if(auto *p=target()){ if(p->status!='D'){ say("Not dead!"); fail_tail=true; } else apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand); } break;` and after the switch, if `fail_tail`: `say("Failed!")` and emit the failure tone (reuse the `invalid-magic` Sfx hook unless the audio owner adjudicates the glide). Keep the `indices[8]` line as is for this item.
2. Nothing in `combat.cpp` (arena already correct).
3. Keep `apply_target_spell` as is.

Facts the implementer needs: text must be exactly `Not dead!` (the DATA string has a trailing `\n`; the ports pass strings without it); order is `Resurrection!`, `Not dead!`, `Failed!`; zero RNG draws; scroll consumed; no HP/MP/exp/status change; result code -1 (cancelled picker) prints neither "Not dead!" nor "Failed!".

### 7.2 Boundary test cases (each should be a RED-first host test; I have not run any)

1. **Scroll, living member, world:** statuses 'G', 'P', 'S' each: transcript `Scroll`, `Resurrection!`, `Not dead!`, `Failed!`; `scroll_quantities[6]` decremented once; member bytes unchanged (HP, MP, exp, level, max HP, status); RNG stream untouched (0 draws); no `scroll-used` Sfx and no MagicCeremony event; one failure tone event.
2. **Scroll, dead member ('D' only):** transcript `Scroll`, `Resurrection!` then no "Not dead!"/"Failed!"/"Success!"; HP 1, status 'G'; karma cut when karma < 98 (test karma 97, 98, 99, 0 with exp 1000 and exp 0); class MP rule A/M = INT, B = INT>>1, other = untouched; level = bitlength(exp/100)+1; max HP = 30 x level, including a record whose exp becomes 0 (level 1, max HP 30). Ceremony: assert the CURRENT behaviour (none) with a comment that the binary has ceremony(6) here (section 8) until that is adjudicated.
3. **Scroll, status bytes other than 'D'** (for example 0x00, 'X'): treated as not dead.
4. **Scroll, cancelled picker (slot -1):** `Scroll`, `Resurrection!` only; scroll spent; NO "Failed!" and no tone. On the device this needs the runtime-level test (`AlphaRuntime` cancel currently refunds -- adjacent divergence, section 5.1).
5. **Scroll in the arena (location >= 0x80):** `Scroll`, `Resurrection!`, `Not here!`; scroll spent; no prompt; no "Failed!" (already true; pin it).
6. **Spell, living member (world and dungeon):** no "Not dead!"; transcript ends `Failed!`; 1 mixed-spell charge and 8 MP spent; no change to the member; 0 RNG draws; a turn is due in the binary (see 5.5 before asserting it in native).
7. **Spell, arena:** `Not here!`, no charge, no MP spent (already true: `time_bits` 14).
8. **Spell, caster level < 8 or MP < 8:** `Failed!` (after "M.P. too low!" for the MP case) with NO target prompt (binary evaluates the gates first; native prompts first -- adjacent).
9. **Guard against the other modes if D-83/D-84 later share one function:** the shared `resurrect_apply(slot, mode)` must take the mode (0 spell: silent, ceremony 8; 1 scroll: "Not dead!", ceremony 6; 0xFF healer/Refuge: "Not dead!" printed on a non-dead target, no ceremony).

## 8 Unresolved (what is not established, and what would settle it)

1. **Ceremony on the scroll's success.** The binary calls `CAST2:0x0000` with index 6 from `resurrect_apply` (0x06c4-0x06d4, mode 1). Both ports and `re/notes/ceremonia-de-conjuro-cast2-0000.md` / `ceremony.ts:123` say scroll 6 has no ceremony, citing only the absence of a call in CAST.OVL 0x12d8-0x12f7; the note's "corpus negative" (section 7) is about original footage showing no flash. I did not see that footage. To settle: frame-diff of an original-footage scroll resurrection (viewport XOR inversion window of 2 x sweep at index 6), or run CAST2 0x0000 index 6 under an emulator. Same question for the spell: the ports flash before the apply regardless of outcome; the binary flashes only on success, at 0x06d4 with index 8.
2. **Refuge (mode 0xFF).** BLCKTHRN 0x0b68-0x0b95 calls `resurrect_apply` for every member `si = 0 .. [0x585b]` with no status test that I found, so a LIVING member in the Refuge loop would print `Not dead!\n`. Unknown: whether the scene guarantees every member is 'D' (a total party wipe) and whether that console row is visible. This is D-84 territory; evidence needed: the code that sets status 'D' on entry (search the BLCKTHRN 0x0910 function for writes of 0x44 to record+0x0b) or an original-footage capture.
3. **Turn accounting in the arena** for the scroll and the spell gate failure: `COMBAT.OVL 0x544` returns AX = 0 (or 1 on "Can't!"), and the shared dispatcher's `jmp 0x11d9` path returns whatever AX held. How the arena loop (0x063e / 0x07ba) turns that into a combat turn is not decoded. It does not change this item's text but matters for the arena turn parity. Evidence: read 0x07ba-0x0b56 for the use of `[bp-2]` and `[bp-4]`.
4. **World-tick body and the town/dungeon loops' use of the 'U' return.** Shown for the overworld only (MAINOUT 0x0c30 `cmp [bp-8],0`). I assumed the town and dungeon loops treat the 1 the same way, as `re/notes/command-dispatch.md` section 6-bis says, without disassembling TOWN and DUNGEON.
5. **Picker details**: the member selector (kernel 0x2e8e -> 0x2d7a with argument 0) was read only as far as the digit path (`'1'..'6'` bounded by the party size at DS 0x585b); the arrow-key cursor branch and the -1 return on cancel were not decoded. `CAST2:0x009e` prints "None!" for a negative return and the name otherwise (0x00b3-0x00c5), then a LF only when the window flag is set (kernel 0x1f12) -- the condition under which that flag is set was not traced.
6. **Native turn behaviour** (5.5) and the device-runtime cancel refunds (5.1, 5.3) are by code reading only; I was not allowed to run the host tests or build. A single RED test per case in 7.2 settles them.
7. Brief-versus-measurement conflict on overlay bases (COMBAT.OVL and BLCKTHRN.OVL measured at 0xA290, not 0xBFEC / 0xE63E). The `callers_banda.py` table is wrong for CAST, CAST2, COMBAT and BLCKTHRN; whoever runs it for this routine will miss three of the four callers.
