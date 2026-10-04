# D-82 FINAL (reconciler): the missing INIT.OOL underworld objects

Reconciler of D82-A (binary-first), D82-B (ports-first) and D82-C (cross-check). READ-ONLY: nothing under
`C:/Dev/OpenU5-Native` was edited, built or run (no cmake/ninja/ctest/idf; `python re/tools/callers_banda.py` was run once as a
negative control, it only reads). Own scratch under `findings/d82f/`: `dz.py` (annotating 16-bit disassembler), `basefit.py`
(brute-force overlay base fit), `census.py` (by-band caller census), `oolstr.py` (every `.OOL`/`.GAM` string reference),
`scratchrefs.py`/`ptrrefs.py` (operand scans), `k3a74.py` (write-site windows).

Conventions: `FILE.OVL:0xNNNN` = overlay FILE offset (what `re/tools/dis16.py` prints); `K:0xNNNN` = `ULTIMA.EXE` image offset with the
0x800-byte MZ header skipped (the notes' CS:IP); `DS:0xNNNN` = `DATA.OVL` file offset - 0x10. Overlay near call to the kernel:
`K = (printed_target + base) & 0xFFFF`. Every instruction quoted below was disassembled by me in this session; where I rely on a
report without re-reading the code it says so.

---------------------------------------------------------------------------------------------------------------------------

## 1 Verdict

### 1.1 What the original does

1. **New game.** Two independent writers leave `SAVED.OOL = 256 x 0x00 ++ INIT.OOL` (0x200 bytes) and `SAVED.GAM = INIT.GAM` patched
   with the identity: `FONT.OVL:0x0de7-0x0e3d` (Create Character) and the Ultima IV transfer `INTRO.OVL:0x1363-0x1e08`. `INIT.OOL`
   is a raw 256-byte image of the live object table (`DS:0x5C5A`, 32 records x 8 B) and it fills the **UNDER** block only; the BRIT
   block is zero-filled by `rep stosb` (`FONT:0x0e11-0x0e1d`, `INTRO:0x1ddc-0x1de8`). Nothing else in any binary references
   `INIT.OOL` (string/operand census, 2.2).
2. **Journey Onward** (`INTRO.OVL:0x0f26-0x0f89`) reads `SAVED.OOL` and writes its halves to the working files `BRIT.OOL` (first 0x100) and
   `UNDER.OOL` (second 0x100). `BRIT.OOL`/`UNDER.OOL` on disk are therefore **run-time scratch, not distribution data**. The live table
   of a loaded game comes from `SAVED.GAM` (window `DS:0x55A6..0x6606`, table = +0x6B4), never from the `.OOL` files.
3. The five records are **ordinary 8-byte table records** in slots 23..27 of the UNDER block, byte-exact
   (`INIT.OOL` file offset = 8 x slot): slot 23 `29 29 0e f2 ff 00 00 00` (tile byte 0x29 = sprite 0x129 SkiffRight, (14,242));
   slots 24-27 `1e 1e 67 e2 ff 00 00 00`, `1e 1e 69 e3 ff ..`, `1e 1e 6b e3 ff ..`, `1e 1e 6c e1 ff ..` (tile byte 0x1e = sprite 0x11e
   DeadBody at (103,226), (105,227), (107,227), (108,225)); floor byte 0xFF; +5/+6/+7 = 0. No transformation anywhere between file
   and table.
4. They enter the live table only through a **raw 0x100-byte read of `UNDER.OOL` into `DS:0x5C5A`**, at exactly three read sites:
   the kernel main loop after any town/dungeon session (`K:0x012d-0x013c`, an unconditional read of `world_filename(g_floor)`, which names `UNDER.OOL` for every exit that leaves `g_floor = 0xFF`),
   the moongate when old and new location are both 0 (`K:0x4889-0x4898`), and the waterfall at (54,138) (`OUTSUBS.OVL:0x0529-0x0538`).
   The **whirlpool** (`MAINOUT.OVL:0x12b2`) sets `g_floor = 0xFF` without any `.OOL` access, so the Britannia table stays live under
   `g_floor = 0xFF` (static result; consequences in 2.6).
5. A save never parks the live table: `CAST2.OVL:0x10fe` writes `SAVED.GAM` (live window) and `SAVED.OOL` = on-disk `BRIT.OOL ++ UNDER.OOL`
   (the active world's block is stale). So the objects survive wherever they currently live (live table if the party is outdoors in
   that world, else the parked block). Every load site is a whole-table copy that also replaces slot 0, i.e. `g_hull`/`g_skiffs`
   (`DS:0x5C5F`/`0x5C61`).
6. **Behaviour of the records** (all re-read by me, 2.7): they are objects by class (`MAINOUT:0x105c` says 0 for tile bytes < 0x80), never
   moved or despawned; drawn as sprites `0x100+byte` over a **zeroed terrain cell**; a body blocks every party/monster; the skiff is
   **enterable on foot** (object byte 0x29 is in the allow list `[0x24,0x2c)` and the composited terrain value 0 passes the foot bitmap,
   bit 0 of `DS:0x54d4` is clear) and boardable (`CMDS:0x0898-0x08b5`, record consumed, `g_hull`/`g_skiffs` untouched); neither can be
   picked up (`SJOG:0x196a-0x197d`), opened (`SJOG:0x1181`, chest = tile 1) or remains-searched (`SJOG:0x0a50`, only 0x1f).

### 1.2 Who is wrong

* **Reference (TypeScript): wrong by omission.** No seed anywhere; `init.ool` is only the write template. Also wrong: the extractor's
  provenance of `init.ool` (`extractor/src/pipeline.ts:419-428` builds it from the run-time files `BRIT.OOL ++ UNDER.OOL`; the faithful
  source is `zeros(256) ++ INIT.OOL`, byte-identical today), and comments (`game.ts:3434-3442`, `game.ts:6964`, `saveNative.ts:509,954-955`, see 3.4).
* **Native: wrong by omission**, same shape (`alpha_runtime.cpp:3229-3240`); the PC bridge needs two extensions (bodies, export slot rule) once
  new journeys carry the seeds.
* **Notes/tools wrong:** `re/notes/moonstone-loc-y-pozo-doom.md` 2.4 (says the `world_filename` callers are two writes; they are seven,
  four of them in the kernel, because its census used the broken base table) and 2.5 (`BRIT.OOL` "carries slot 0 at (86,107)": that is a
  played session's scratch; this tree's file is 256 zeros); `re/notes/intro.md:162-163` and `oracle-pending-sweep.md:89-90` (DS offsets of
  `SAVED/BRIT/UNDER.OOL/UNDER.DAT` shifted by one entry, real: 0x323f/0x3249/0x3252/0x325c/0x3266); `re/tools/callers_banda.py` `BASES`
  (wrong for 14 of its 22 entries; it finds **4 of the 17** `write_whole_file` call sites, measured); the task brief's base table (wrong for
  OUTSUBS, DUNGEON, COMSUBS, SHOPPES3, LOOKOBJ, DNGLOOK, CAST, COMBAT, INTRO, BLCKTHRN); `re/tools/thunks.py --bases` (prints the minimum
  thunk target, not a load base).
* **Investigators:** A and C concluded the new-game skiff is "unobtainable on foot" because terrain 3 is foot-blocked. That is wrong
  (the movement code tests the composited, zeroed terrain byte, 2.7); B was right. A's "Option A" rationale (the cell must keep the
  terrain's walkability) is consequently void. B's suggestion to put the data into `createNewGame` (or new `ExtractedInitialState` fields) was rejected (3.2).

### 1.3 Recommended representation (needs the lead's sign-off, section 7 item 8)

* skiff -> **persistent terrain override** `0:255:14:242 = 0x129` (the reference's own "Class C" channel, declared there "su equivalente declarado de un registro de objeto" at `game.ts:4929`;
  the channel `(X)-it` already uses (`game.ts:5007`) and the channel the A4-SAVE3 bridge reads and writes; boarding already clears it (`game.ts:4971`), with no `g_hull` side effect). It must NOT be a `ship` object (two real hazards, 3.5 and 4.4). Cost: slot 23 is not occupied (declared).
* four bodies -> `prop` pool objects `{location 0, floor 255, tile 286, slot 24..27}` (the representation both ports already use for the
  interior `.NPC` corpse).
* seeded at the **new-journey seam only**, never in `createNewGame`/`createBaseGame`, the import path or Continue/Load; data derived from
  the 40 bytes `INIT.OOL[0xb8..0xdf]` through one classifier; no RNG, no text.

Confidence: HIGH for sections 2 (every address re-disassembled, three positive controls); HIGH for the port status; MEDIUM for
the representation (a judgement, with the cost stated).

---------------------------------------------------------------------------------------------------------------------------

## 2 Binary facts

### 2.0 Instruments and positive controls (all passed)

| control | result |
|---|---|
| overlay bases, brute force over all 65536 candidates, score = near calls landing on a kernel `55 8b ec` prologue (`d82f/basefit.py`) | INTRO **0x81C0** 546/608 (file starts with a 0x10-byte header `d9 95 00 00 a3 95 ..`, thunk targets 0x8b46/0x85fe/0xa250 -> file 0x986/0x43e/0x2090 are prologues, verified); OUTSUBS **0xA290** 68/78; MAINOUT 0x81D0 182/264; TOWN 0x81D0 143/212; DUNGEON **0x81D0** 166/255; CMDS 0xBF80 210/259; SJOG 0xBF80 211/257; TALK 0xBF80 109/239; CAST **0xBF80** 154/315; CAST2 0xE1E0 183/204; FONT 0xE1E0 77/100; COMSUBS/SHOPPES2/SHOPPES3/ZSTATS 0xE1E0; LOOKOBJ/DNGLOOK/BLCKTHRN/COMBAT/ENDGAME/SHOPPES 0xA290 (183/212, 104/116, 129/152, 115/197, 114/174, 210/299). NPC is inconclusive (13/54). Second witness, independent of call statistics: `K:0x7a22 = lcall 0x72e,0x2ec ; id 0x0b ; ljmp 0:0xa5f8` and `0xa5f8 = 0xA290 + 0x368` where `OUTSUBS.OVL:0x0368` is the `world_filename` prologue; `0x7a3a -> 0x8ef2 = 0x81D0 + MAINOUT:0x0d22`; `0x7a16 -> 0x8ffe = 0x81D0 + DUNGEON:0x0e2e`. |
| census vs the shipped tool (`d82f/census.py` vs `re/tools/callers_banda.py 25d8`) | mine: **17** writers; shipped tool: 4 (kernel x2, MAINOUT x2). With the brief's table my census finds 9 (misses INTRO and OUTSUBS). The tool is blind to every OUTSUBS, INTRO, COMBAT, BLCKTHRN, LOOKOBJ, DNGLOOK, DUNGEON, COMSUBS, SHOPPES3 and CAST caller. |
| independent reproduction of known figures | `K:0x38e4` 11 callers, `K:0x3868` exactly ten call sites `0x38ef,0x3905,0x391d,0x3935,0x394d,0x3964,0x397b,0x3992,0x39a9,0x39bf` (matches `world/actorPool.ts`); `K:0x7a22` 6 external callers + the OUTSUBS-local call at `0x040c` = the seven of `re/notes/gfloor-146-acta.md`. |
| terrain decode | `UNDER.DAT` with `off=(y>>4)*4096+(x>>4)*256+(y&15)*16+(x&15)` matches the A/C claim (65536/65536 vs `underworld.json` per A and C, not re-run by me; I decoded the ten cells of 2.4 with it). |
| file bytes | `INIT.OOL`/`UNDER.OOL` md5 11736cdb..., `BRIT.OOL` 348a9791... (all zero), `SAVED.OOL` 2dc39b03... == `bytes(256)+INIT.OOL` (True), `game/assets/init.ool` sha256 d2ec1ae9f0472fcb... == `bytes(256)+INIT.OOL` (True), `INIT.GAM[0x6b4..0x7b3]` all zero, location 13, floor 0, x=y=15. |

Corrected base table for the batch (replace the brief's): see Appendix A.

Provenance caveat (B 2.0, confirmed by `original/u5/ultima5/Files.txt`, `History.txt` and the file dates): the tree holds **The Exodus Project "Ultima V Upgrade
1.0" (2001)** set. `Files.txt` lists INTRO/FONT/MAINOUT/TOWN/DUNGEON/ENDGAME.OVL and `ULTIMA.EXE` as upgrade code; their dates are 2001-08-05..2001-08-20, while OUTSUBS/CAST2/CMDS/SJOG.OVL
carry 1996-12-25. The new-game writers (`FONT`, `INTRO`) and two swap sites (`MAINOUT`, the kernel) are in the patched set; `History.txt` documents music changes only (title/intro/endgame/creation songs, CTRL-E exit, the MID.DRV name in `DATA.OVL`) and says nothing
about saves or `.OOL`. A diff against a pristine 1988 build is impossible here (section 7 item 1).

### 2.1 File primitives (`ULTIMA.EXE`)

```
256e read_file(name=[bp+0xa], buf=[bp+8], len=[bp+6], offset=[bp+4])   ret 8   (callers push: name, buf, len, offset)
    25a1 call 0x1eac (disk select) ; 25ac..25b6: push [bp+0xa],[bp+8],[bp+6],di(=offset) ; call 0x7234 ; loop while ax==0
7234 int 21h: 7245 mov ah,3dh (open, mode 2) ; 725d xor al,al ; 725f mov ah,42h (lseek from start, only if offset!=0, cx=0 dx=offset)
     7265 mov cx,[bp+6] ; or cx,cx ; jne 726f ; mov cx,0xffff ; 726f mov dx,[bp+8] ; 7272 mov ah,3fh (read) ; 727b close ; ret 8
25d8 write_whole(len=[bp+4], buf=[bp+6], name=[bp+8])  ret 6  -> 2631 call 0x7296
7296 72a1 mov ah,3ch (create/truncate, cx=0) ; 72a9 mov cx,[bp+4] ; mov dx,[bp+6] ; 72af mov ah,40h ; close
```
No transformation, no zero fill, no filtering: a ".OOL read" is "open, read 0x100 bytes at offset 0 into DS:0x5C5A". `K:0x251e` is a floppy-era volume selector
(irrelevant data-wise).

### 2.2 Every `.OOL` string reference (closure)

`oolstr.py` (raw scan of every 16-bit operand equal to a DS string offset, all 24 binaries): `INIT.OOL` DS:0x334e **only** at `INTRO:0x1363`, DS:0xa0c2 **only** at
`FONT:0x0de7`. `SAVED.OOL`: INTRO 0x0f26, 0x1dea; FONT 0x0e1f; CAST2 0x1197. `BRIT.OOL`: INTRO 0x0f3f; OUTSUBS 0x051a (+0x0378 via `[bp-2]`); CAST2 0x1157.
`UNDER.OOL`: INTRO 0x0f4e, 0x0f7d; OUTSUBS 0x0529, 0x054d (+0x037f); CAST2 0x113e, 0x116f. Two further raw hits (`CMDS:0x1669`, `COMBAT:0x0654`) are operand-byte
coincidences (`a2 96 58 = mov [0x5896],al`), checked by disassembly. No pointer table exists.

Census (my `census.py`, verified bases, positive controls above):

* `K:0x256e` read_file, **47** callers: BLCKTHRN 0x0715 0x0727 0x0b2d; CAST2 0x0ef6 0x0f09 **0x114d 0x1166**; DUNGEON 0x007b; ENDGAME 0x016b 0x066f 0x0681; FONT 0x0507 0x0a9d 0x0b53 0x0c5a 0x0d5c **0x0df6**;
  INTRO 0x0321 0x0b3d 0x0ec6 **0x0f35** 0x1055 0x122f 0x1360 **0x1372**; LOOKOBJ 0x001e 0x0031 0x06c5 0x06d8 0x08ed 0x092f; MAINOUT 0x0884; NPC 0x0063 0x0078 0x008d; OUTSUBS 0x0103 **0x0538** 0x0950;
  SHOPPES 0x018c 0x0685 0x1084; TALK 0x12a6 0x12f1; TOWN 0x046f; ULTIMA **0x013c 0x4898** 0x6104. **Relevant (read an `.OOL`): the eight in bold.** All others name a data file.
  (A counted 45; B and C 47; 47 is right.)
* `K:0x25d8` write_whole, **17** callers: CAST2 0x117b 0x1194 0x11a3; FONT 0x0e2b 0x0e3d; INTRO 0x0f4b 0x0f5a 0x0f89 0x1df6 0x1e08; MAINOUT 0x0863 0x0aed; OUTSUBS 0x0418 0x0526 0x0559; ULTIMA 0x016b 0x482f.
  14 write an `.OOL`, three write `SAVED.GAM` (CAST2 0x1194, FONT 0x0e3d, INTRO 0x1e08).
* `K:0x7a22` world_filename (thunk to `OUTSUBS:0x0368`): ULTIMA 0x012d(R) 0x015f(W) 0x4823(W) 0x4889(R); MAINOUT 0x0857(W) 0x0ae1(W); + OUTSUBS-local `0x040c`(W). All 7 relevant.
  MAINOUT 0x0ae1 is the **party-death** path (A labelled it "step onto a town tile"): `MAINOUT:0x0aa2 call K:0x39fc ; cmp ax,-1 ; jne` and `K:0x39fc` returns -1 when no roster
  status byte is 'G'/'P'/'S' (`K:0x3a26-0x3a6a`); it then parks and calls `K:0x0e7c`. Immaterial to D-82.

### 2.3 New game, Journey Onward, Save (quoted)

```
FONT.OVL (base 0xE1E0), create_character_main 0x0b0a:
0b41 mov ax,0xa060 ("INIT.GAM")  ; buf 0x55a6 ; len 0x6606-0x55a6 = 0x1060 ; 0b53 call K:256e     -> window := INIT.GAM
 ...questionnaire (QUESTION.DAT is read INTO 0xB21E, FONT:0x0a85/0x0aa3; every other 0xB21E use is before 0x0de7) ...
0de7 mov ax,0xa0c2 ("INIT.OOL") ; 0deb mov ax,0xb31e ; 0def mov ax,0x100 ; 0df3 sub ax,ax ; 0df6 call K:256e   read(INIT.OOL, 0xB31E, 0x100, 0)
0e0a K:251e(3) ; 0e11 mov cx,0x100 ; 0e14 mov di,0xb21e ; 0e17 mov ax,ds ; 0e19 mov es,ax ; 0e1b sub al,al ; 0e1d repne stosb   zero 0xB21E..0xB31D
0e1f mov ax,0xa0cc ("SAVED.OOL") ; 0e23 mov ax,0xb21e ; 0e27 mov ax,0x200 ; 0e2b call K:25d8    write_whole(SAVED.OOL, 0xB21E, 0x200) = zeros(256) ++ INIT.OOL
0e2e "SAVED.GAM" ; 0x55a6 ; 0x1060 ; 0e3d call K:25d8
INTRO.OVL (base 0x81C0), Ultima IV transfer 0x132a:  1360 read INIT.GAM ; 1363 mov ax,0x334e ("INIT.OOL") ; 1367 0xb31e ; 136b 0x100 ; 1372 call K:256e
   1ddc mov cx,0x100 ; 1ddf mov di,0xb21e ; 1de8 repne stosb ; 1dea "SAVED.OOL"(0x3641) ; 1dee 0xb21e ; 1df2 0x200 ; 1df6 call K:25d8 ; 1e08 SAVED.GAM
INTRO.OVL Journey Onward 0x0e7c..:
0ec6 read(SAVED.GAM, 0x55a6, 0x1060) ; 0f26 mov ax,0x323f ("SAVED.OOL") ; 0xb21e ; 0x200 ; 0f35 call K:256e
0f3f "BRIT.OOL"(0x3249) 0xb21e 0x100 ; 0f4b call K:25d8 ; 0f4e "UNDER.OOL"(0x3252) 0xb31e 0x100 ; 0f5a call K:25d8
0f5d cmp [0x5893],0 ; jne 0f8c ; 0f64 cmp [0x5895],0 ; je 0f8c ; 0f6b..0f89 floppy-era re-write of UNDER.OOL (same data)
CAST2.OVL (base 0xE1E0), Save 0x10fe (only caller K:0x3393 via thunk 0x81ae, census-verified):
113e UNDER.OOL(0x967a) 0xb31e 0x100 0 ; 114d call K:256e ; 1157 BRIT.OOL(0x9684) 0xb21e 0x100 0 ; 1166 call K:256e ; 1169 cmp [bp-4],1 ; je 117e ; 116f UNDER.OOL write (floppy mirror)
1185 SAVED.GAM(0x9698) 0x55a6 0x1060 ; 1194 call K:25d8 ; 1197 SAVED.OOL(0x96a2) 0xb21e 0x200 ; 11a3 call K:25d8
```
U4-transfer buffer integrity (B and C left it open): settled statically. The transfer body `INTRO:0x1375-0x1ddc` calls only the kernel routines
`16ba 1850 1a3e 1b94 1bf2 1c22 1dda 1e38 1eac 2032 251e` (no thunk call, no far call) and INTRO-local functions; the local call graph from `INTRO:0x132a` reaches only `INTRO:0x1016`, `0x12ea`, `0x1e22`
(the `party.sav` reads at INTRO:0x1055/0x122f go to DS:0xBC88 and the DS:0xBB1C area). A scan of every operand in all 24 binaries for DS:0xB21E..0xB49E finds no kernel reference at all except the
pointer init `K:0x00a1 mov [0xb11c],0xb21e`, read only by NPC.OVL (`ptrrefs.py b11c`); inside INTRO the range is referenced only from `INTRO:0x014e` (STORY.DAT function), `0x0986` (Journey Onward) and `0x132a` itself,
and neither of the first two is reachable from the transfer. So `INIT.OOL` read at 0x1372 reaches the write at 0x1df6 intact (static proof, no runtime witness).

### 2.4 The live table and its records

`DS:0x5C5A..0x5D59`, 32 x 8 bytes = `SAVED.GAM + 0x6B4`. Writer `K:0x3a74(+0,+1,+2,+3,+4,+5,slot)` `ret 0xe` (`3a7f..3aa5`: `[si+0x5c5a..0x5c5f]` from `[bp+0x10..+6]`; never +6/+7).

| byte | meaning (evidence) |
|---|---|
| +0 | tile byte, sprite = +0 + 0x100 (blitter `K:0x56e1 add ah,1`); class selector; free iff 0 (`SJOG:0x0012 cmp byte [bx+0x5c5a],0`) |
| +1 | live frame, drawn by the compositor (`K:0x5525-0x5534`, `0x55f8`); = +0 when placed |
| +2,+3 | x, y (`K:0x368e` compares the byte, zero-extended, with the word argument; `0x36ad-0x36bb`) |
| +4 | floor byte: 0 Britannia, 0xFF underworld; must equal `g_floor` for `g_location <= 0x7f` (`K:0x36c0-0x36d2`, compositor `0x549a-0x54a0`) |
| +5 | hull (ships, `CMDS:0x08dc`), contents (chests), plot-item index (`OUTSUBS:0x05d1`) |
| +6 | not derived (zero in the seeds) |
| +7 | skiffs aboard a ship (`CMDS:0x08fe`) |

Slot 0 is the party's own vehicle record (`K:0x53a6-0x53bf` mirrors `[0x5896],[0x5897],[0x5895],[0x587c]` into it each frame); `g_hull` = `DS:0x5C5F` (+5), `g_skiffs` =
`DS:0x5C61` (+7) (`CMDS:0x08f4`, `0x0936`, `0x0fb5-0x1023`).

`K:0x368e find_object_at_xy(x=[bp+8], y=[bp+6], floor=[bp+4])` `ret 6`: slots 1..31 ascending (`mov si,0x5c62 ... cmp si,0x5d5a`), match x byte, y byte, floor byte (skipped when
`g_location > 0x7f`), returns the +0 byte (0 = none, slot 32 in `[0x5876]`). It never tests +0 != 0. `K:0x3702` is the descending twin.

Terrain under the records (my decode): skiff cell (14,242) = 0x03 WaterCoast; its 4-neighbours (13,242)=0x31, (15,242)=0x02, (14,241)=0x02, (14,243)=0x30; bodies
(103,226) (105,227) (107,227) (108,225) = 0x05 Grass; Amulet cell (105,225) = 0x30.

### 2.5 Slot roles

* `K:0x3868(lo,hi,near)` `ret 6`: scans slots **1..23** only (`3870 mov cx,1 ... 38cf cmp cx,0x18 ; jl 3881`), accepts `lo <= +0 <= hi` (unsigned `jb`/`ja`), never 0xB5 (`388f`), and with `near != 0`
  only records outside the 11x11 window (`38a1-38b7`). `K:0x38e4` calls it ten times: (0,0,0), (1,0xf,1), (0x80,0xff,1), (0x10,0x11,1), (0x30,0x7f,1), (1,0xf,0), (0x80,0xff,0), (0x10,0x11,0),
  (0x30,0x7f,0), (0,0xff,0). Tile bytes 0x1e and 0x29 match only the tenth call. Callers of `K:0x38e4` (11): CMDS 0x0ff4 (X-it), 0x10c0; LOOKOBJ 0x012c; MAINOUT 0x07fd, 0x0d33, 0x1021, 0x1bbd; SJOG 0x0422,
  0x05bd; TOWN 0x0314, 0x1785. A skiff in slot 23 therefore removes the last slot of the 23-slot monster pool; slots 24-27 are outside it.
* `SJOG.OVL:0x0000 find_free_actor_slot`: `mov dx,0x20 ; dec dx ; js ; cmp byte [bx+0x5c5a],0 ; jne` = 31 down to **0**, first free; 0 is ambiguous with "none" (only caller `SJOG:0x0fcc`, loot_place).
* `OUTSUBS.OVL:0x0566` (stub `K:0x7b96`, only caller `MAINOUT:0x0076`, inside the outdoor re-init `MAINOUT:0x0000`; gate `0x056e cmp [0x5895],0 ; je`, i.e. `g_floor != 0`): amulet slot 28 `[0x5d3a]=[0x5d3b]=0xb7`,
  `[0x5d3c]=0x69`, `[0x5d3d]=0xe1`, `[0x5d3e]=0xff`, `[0x5d3f]=0xf3` when `[0x57b3]==0`; slots 29..31 (`0x5d42+8*si`) `0xb4`, x=`[0x3a06+si]` (c0 82 b0), y=`[0x3a0a+si]` (50 41 b8), +4=0xff,
  +5=`[0x3a0e+si]` (f0 f1 f2) when `[si+0x57b6]==0` and `[si+0x58c8] < 0x80`. **No occupancy check**: slots 28-31 of the underworld table are rewritten at every outdoor (re)start while the plot items are
  untaken (all four in a new game). It never touches 23-27.

### 2.6 Every table swap (re-read by me)

| event | site | action |
|---|---|---|
| leave a town/dungeon | `K:0x00db-0x0116`, **`0x012d call 0x7a22 ; 0x0130 push ax ; mov ax,0x5c5a ; mov ax,0x100 ; sub ax,ax ; 0x013c call 0x256e`** | `L := file world(g_floor)`; then if outdoors and `g_floor != 0` a floppy-era re-write (`0x015f..0x016b`). Reached after the town branch (`0xf7-0x102`) and the dungeon branch (`0x104-0x111`); `g_floor` was set by the exit |
| exit setters of `g_floor` | `TOWN:0x07c5-0x07e1` (`g_location==0x19` -> 0xFF "Underworld!" else 0 "Britannia!"), `DUNGEON:0x1d25-0x1d36` (`[0x5895]!=0` -> 0xFF) | decide which file the line above reads |
| (E)nter town | `OUTSUBS:0x040c-0x0418` (`call 0x368 ; push 0x5c5a ; push 0x100 ; call K:25d8`), then `0x0420 g_location=idx+1`, `0x0423 g_floor=0`, `0x0428 x=0xf`, `0x042d y=0x1e` | park `world(g_floor)` |
| (E)nter dungeon | `MAINOUT:0x0857-0x0863`, then DUNGEON.DAT read `0x0884`, `0x088f cmp [0x5895],0 ; je 0x8b4 ; cmp al,0x28 ; je` else `g_floor=7`; else `g_floor=0` | park `world(g_floor)` |
| party death | `MAINOUT:0x0ae1-0x0aed` then `K:0x0e7c` | park |
| moongate (`K:0x47f4`, callers `K:0x4977`, `CAST:0x0d3a`) | `0x480a cmp [0x5893],0 ; jne` -> `0x4823 call 0x7a22 ... 0x482f call K:25d8` (park OLD floor); `0x4841-0x4856` set location/x/y/**floor from `[bx+0x5848]`**; `0x487c-0x4887` both 0 -> `0x4889 call 0x7a22 ... 0x4898 call K:256e` (read NEW floor), `0x489b call 0x7b7e` | same-world gate = park then re-read the same file (net zero); Britannia<->underworld = table REPLACED |
| waterfall (`OUTSUBS:0x0458`, stub `K:0x7b42`, callers `MAINOUT:0x05bb`, `0x0d0e`) | `0x0500 cmp [0x5896],0x36 ; 0x0507 cmp [0x5897],0x8a`; `0x050e` "Falling into underworld!!\n" (DS:0x39c3); `0x0515 mov [0x5895],0xff`; `0x051a..0x0526` write `BRIT.OOL`(0x39de) from 0x5c5a; `0x0529..0x0538` **read `UNDER.OOL`(0x39e7) into 0x5c5a**; `0x053b..0x0559` floppy mirror; `0x055c call K:7b7e` | x,y stay (54,138); the damage loop's `rand` draws (`0x04b6-0x04f3`) all precede `0x0500` |
| whirlpool (`MAINOUT:0x1248`, only caller `MAINOUT:0x1399`) | `0x12b2 mov [0x5895],0xff ; 0x12b7 [0x5896]=0x22 ; 0x12bc [0x5897]=0x12 ; 0x12c1 call 0x0` (`MAINOUT:0` = outdoor re-init) | **no file access**; census: no `.OOL` reader/writer is reachable from here |
| combat | `K:0x5f9f-0x60db` copies the table to `DS:0xA9FC` and back (A/B, not re-read by me) | not an `.OOL` |
| enter a small map | `TOWN:0x0fea-0x0ff4 mov cx,0x100 ; mov di,0x5c5a ; ... al=0 ; repne stosb`; `TOWN:0x120a-0x1216` zeroes +0 of slots 1..31 | this is the memset the ports' comments attribute to `MAINOUT:0x0857` (that address is the park write) |
| save | `CAST2:0x113e-0x11a3` | never parks (2.3) |

Routine census: `K:0x47f4` callers 2 (above); `MAINOUT:0x0000` via thunk `K:0x7b7e` 3 (`CAST:0x0740`, `OUTSUBS:0x055c`, `ULTIMA:0x489b`) + local `MAINOUT:0x0d29`, `0x12c1`; waterfall thunk 2;
enter thunk `K:0x7bba` 1 (`MAINOUT:0x097e`); seed thunk `K:0x7b96` 1; `K:0x7bde` (is_monster) 1 (`CMDS:0x09fa`); compositor `K:0x5394` 3 (`ULTIMA:0x5a0d`, `CAST:0x0798`, `CAST2:0x04aa`); blitter `K:0x56ac` 4;
all relevant to D-82 except `CAST:0x0740` (same-world blink, no `g_floor` change).

Consequences (static, no runtime witness): (a) a whirlpool-first entry leaves Britannia's table live under `g_floor=0xFF`, the compositor and `K:0x368e` ignore its `+4==0` records, and the next park
(`world_filename()` now says `UNDER.OOL`) overwrites `UNDER.OOL` with that table: the seeds are lost in that sequence. (b) Every swap replaces slot 0, hence `g_hull`/`g_skiffs` are per-world
(UNDER slot 0 is zero in a new game). (c) Wandering monsters are parked with their world's table.

### 2.7 Behaviour of the records (the disagreement, settled)

**The disagreement.** A and C: the foot party cannot step onto the skiff cell because the cell's terrain (0x03) is blocked in the foot bitmap `DS:0x54d4`. B: the movement test reads the *composited*
buffer, which the painter zeroed. B is right:

1. Movement `MAINOUT:0x01fe` (`ret 4`, args dy=[bp+4], dx=[bp+6]): `0x0236 call K:368e(g_x+dx, g_y+dy, g_floor)`; if an object byte is found `[bp-2]=0` and it is re-opened only by the allow list
   `0x0245-0x0283` (transport `>=0x30` or `<0x20`: object in `[0x24,0x2c)`, or 0x1b, or `(b&0xfe)==0x10`; transport `0x28..0x2f`: object in `[0x24,0x28)`; transport `0x20..0x27`: never). Then
   **`0x0288 mov si,[bp+4] ; shl si,5 ; 0x028f mov bx,[bp+6] ; 0x0292 mov al,[bx+si-0x5459]`** reads the cell at `0xABA7 + 32*dy + dx`, and `0x02a1-0x02a8` calls `K:0x2c4c(transport, that byte)` unless `[bp-2]==0`.
2. `0xABA7 = 0xAB02 + 5*32 + 5`: the centre of the 11x11 view buffer at `DS:0xAB02` (32-wide rows) that the redraw fills per cell with `K:0x4402` (`K:0x59d1-0x59dc`) and then hands to the compositor
   (`K:0x5a0d call 0x5394`, then `0x5a10 call 0x56ac` blit). The buffer persists until the next redraw, so at the next keypress it is the composite.
3. The compositor (`K:0x5394`, slots 31 down to 0, slot 0 mirrored first) skips `+0==0`, `+1==0`, `+4 != g_floor`, cells outside the window (`(x-(g_x-5))` and `(y-(g_y-5))` unsigned `<= 10`), cells holding 0xFF or 0x87.
   Decal classes (`(+0&0xfc)==0xe8`, `+0==0x1e`, `+0==0x1f`, and `+1` in {0x1d,0x1e}, `0x54f1-0x5552`): sprite layer `[bx-0x539c]` (16-wide, base `0xAC64`) := the **+1 byte**, terrain layer `[bx-0x54fe]` := **0** (`0x5534`, `0x5542`;
   a cell already 0 is skipped, `0x5513`). Every other class goes through `K:0x51b8(tile=+1, ...)`.
   For tile bytes `0x28..0x2b` (the skiff): `K:0x51b8:0x51d7-0x51e6` jumps to `0x51e9`, which fetches the real terrain with `K:0x4402`; for terrain 3 (not 0xec/0x0a/0x57/0x6a/0x6b/0x92/0x84/0x85/0x90/0x91; the
   `0x532c` probe of a neighbouring cell against 0x9d fails: none of the four neighbours of (14,242) is 0x9d, they are 0x02, 0x02, 0x30, 0x31) control reaches `0x5370`: sprite layer := tile, **terrain layer := 0** (`0x5388 mov byte [bx+si-0x54fe],0`).
   The blitter draws a cell whose terrain layer is 0 as sprite `0x100 + sprite byte` (`K:0x56ca-0x56e4 ... add ah,1`).
4. `K:0x2c4c(transport=[bp+6], terrain=[bp+4])`: class = `DS:0x54f4[transport>>2]`, jump table `cs:0x2d60`. Foot (0x1c/0x1d, index 7) = class 0 -> `0x2c6a` -> `K:0x2bd4`: blocked iff bit `0x80>>(t&7)` of
   `DS:0x54d4[t>>3]` is set. `DS:0x54d4` = `70 0c 00 28 01 f3 00 bd ...`: byte 0 = 0x70, so tiles 1, 2, 3 are blocked and **tile 0 is not**. (TileData.json agrees: tile 0 "Explosion", `IsWalking_Passable true`.)
   `K:0x2bd4:0x2c05-0x2c20` then only forces "blocked" for non-foot, non-0x4x transports on terrain 0x90..0x93.
5. Therefore: a foot party at (13,242) or (14,243) can step onto (14,242); a body (0x1e: below 0x24, not 0x1b, `&0xfe`=0x1e) is "Blocked!\n" (`MAINOUT:0x0322`, DS:0x29ae) for foot, horse, skiff and frigate;
   the skiff blocks a skiff or ship party. Monsters never enter an object cell (`MAINOUT:0x1482-0x14c5`: `K:0x2c4c` on the raw map, then `K:0x3702` != 0 -> 0).
6. (B)oard `CMDS:0x07f6`: `0x07fc-0x0814` "Not here!\n" for locations 0x21..0x28; `0x0818-0x0826 call K:368e(g_x,g_y,g_floor)` (the party's own cell), `0x0829 [bp-0xa]=obj`, `0x082c [bp-4]=[0x5876]`;
   skiff branch `0x0898 and al,0xfc ; cmp al,0x28 ; jne 0x8b8 ; 0x08a1 call 0x06ee` (gate: `[0x587c]` must be 0x1c/0x1d else "\nOn foot\n" DS:0x423e), `0x08ab` "skiff\n" (DS:0x4275), `0x08b2 mov al,[bp-0xa] ; jmp 0x875`
   (`[0x587c] := obj byte`, facing kept, no +2 as for the horse), epilogue `0x093e..0x0949 call K:3a74(slot,0,0,0,0,0,0)`, `0x094c or [0x24e6],2`. Only the frigate branch `0x08c4-0x0939` copies hull/skiffs
   (`0x08dc`, `0x08f4`, `0x08fe`, `0x0936`). A non-vehicle object at the party's cell -> `0x0954` "What?\n" (DS:0x42bf).
7. (G)et `SJOG:0x18ce` (inline scan `0x1933-0x19bd`): accepts `+0 < 0x10`, `==0x19`, `==0x1b`, `(+0&0xfc)==0xb4`; 0x1e and 0x29 fall through. (O)pen `SJOG:0x112c`: only `+0==1` (chest) ("Nothing to open!" DS:0x8b6c). (S)earch
   `SJOG:0x095c`: chest scan `dx==1`, then `K:0x3702 ; cmp ax,0x1f ; jne 0xa6a` -> only 0x1f ("moldy corpse") runs `search_remains_outcome` (`SJOG:0x01f2`); a 0x1e body is skipped and the search falls to the
   terrain table keyed on the raw map byte (`0x0a6a-0x0ae8`, stump 0x2b etc.). (L)ook `LOOKOBJ:0x099c-0x09de call K:368e` (re-read), then `LOOK2.DAT` word at `0x200+2*byte` (B's reading of `0x06a4`, not re-read; the table and strings are mine): 0x1e -> "a corpse" (offset 2983), 0x29 -> "a skiff" (2999); the ports' `look2.json` indexes by tile (286 -> "a corpse", 297 -> "a skiff").
8. Removal paths classified (all `K:0x3a74` callers, 25): board (`CMDS:0x0949`, only the boarded record), outdoor cannon (`CMDS:0x0a92`, behind `K:0x7bde` = `MAINOUT:0x105c`: 0x2c-0x2f -> 1, `<0x80` -> 0, 0xb4-0xb7 -> 0, 0xe8-0xeb -> 0,
   other `>=0x80` -> 1, so monsters only; `CMDS:0x0962` needs transport 0x20..0x27), world-turn despawn (`MAINOUT:0x1b1f`, monsters, `(byte)(x-[0x589b])>0x1f`), Open (`SJOG:0x11e1`, chest), remains search (`SJOG:0x0212`, 0x1f),
   whirlpool-class spell (`CAST2:0x08c1`, `(+0&0xfc)==0xe8`), town cannon (`CMDS:0x0d52`, small maps only: the function `0xaea` calls `0x962` for `g_location==0`), Get-plot removal (`SJOG:0x178b`, plot classes). Writers, not removers:
   `CMDS:0x1016` (X-it), `0x10e4`, `LOOKOBJ:0x014e`, `MAINOUT:0x1040`, `0x1bdb`, `SJOG:0x043c`, `0x05e7`, `0x0fe9`, `CAST2:0x094e`. Indoor-only, not classified by me: `BLCKTHRN:0x06f0 0x0809 0x0821 0x0854 0x0875 0x09c3`, `TOWN:0x16ab 0x17f5`.
   Conclusion: no outdoor code path found that removes a 0x1e record; the skiff leaves only by boarding.
9. Reachability by foot (static, 4-neighbour, wrap at 256, foot bitmap): the skiff's neighbourhood is an isolated pocket of **6 foot cells** {(12,241),(12,242),(13,241),(13,242),(13,243),(14,243)} (tiles
   0x0f 0x0f 0x35 0x31 0x0f 0x30); the bodies lie in the 1641-cell pocket that holds the Amulet cell and is not connected to the Falls arrival (54,138) (a 1-cell region). So on foot neither is reachable from the Falls; teleports and spells were not analysed.

### 2.8 Strings and constants for the port work

`"skiff\n"` DS:0x4275; `"horse\n"` 0x4266; `"carpet\n"` 0x426d; `"Ship\n"` 0x427c; `"\nOn foot\n"` 0x423e; `"What?\n"` 0x42bf; `"\nNot here!\n"` 0x4252; `"Blocked!\n"` 0x29ae; `"Falling into underworld!!\n"` 0x39c3;
`"\nWHIRLPOOL!\n"` 0x6b04. Tile numbers in the ports' space: skiff 0x129 = 297 (`TileData`: walking true, horse true, carpet/skiff/boat false); DeadBody 0x11e = 286 (all passability flags false). RNG: none (the seed, the
swap and the board draw nothing; the Falls' per-member draws precede the swap).

---------------------------------------------------------------------------------------------------------------------------

## 3 Reference (TypeScript): exact change needed

Facts of the current code (re-read): `createNewGame` `state.ts:520` -> `createBaseGame` `:526-575` (no `worldObjects`, no `mapOverrides`); `applyGypsyCreation` `:503-512` (documented as FONT.OVL create_character_main 0x0b0a);
`SAVE_OPTIONAL_DEFAULTS.worldObjects` `:737`, `deserialize` `:810`; real new-game site `main.ts:906-908` (`if (result.action === "create") { applyGypsyCreation(game.state, result.creation); ...}`); `Game` constructor `game.ts:1025` (`state ?? createNewGame(init)`);
the same `createNewGame` feeds `importNativeSave` (`saveNative.ts:1065`), `__parity__/run.ts:126`, `__parity__/combat-run.ts:235`, `momentos/compone.ts:175`, `walkthrough/recorrido.ts:91`.

Changes, in order (RED tests first, section 6):

1. **`game/src/core/state.ts`** (after `applyGypsyCreation`, line ~512): a pure `seedNewJourneyUnderworld(state: GameState, ool: Uint8Array): SeedReport`. Read the UNDER block (`ool[0x100..0x1ff]`), for slot 1..31 with `+0 != 0` (the BRIT block, bytes 0x000-0x0ff, is parsed with
   floor 0 by the same loop and is empty in a new game): classify `(b&0xfc)==0x28` skiff, `(b&0xfe)==0x10` horse, `b==0x1b` carpet -> terrain override `mapOverrides["0:<+4>:<x>:<y>"] = 0x100+b` (the existing Class C channel);
   `(b&0xf8)==0x20` frigate -> a `ship` world object `{tile:0x100+b, hull:+5, skiffs:+7, slot:i}` (as `readNativeWorldObjects`/`read_vehicles` do; none exists in the new-game data);
   `b==0x1e||b==0x1f` -> `worldObjects.push({location:0, floor:+4, x, y, tile:0x100+b, kind:"prop", slot:i})`; any other byte -> reported in `SeedReport.unknown`, not guessed, not placed. A record whose `+4` disagrees with its block is
   reported and skipped (the original keeps it invisible: `K:0x368e` and the compositor test `+4==g_floor`). Draws no RNG, prints nothing, does not touch `shipHull`/`shipSkiffs`. For the new-game bytes this yields exactly one override
   `"0:255:14:242" = 0x129` and four props at slots 24..27 (no `hull`/`skiffs` anywhere). The native classifier (4.1) is the same function by the same rules.
2. **`game/src/main.ts:906-908`**: call it right after `applyGypsyCreation(game.state, result.creation)` with the bytes of `/assets/init.ool` (`ui/savepanel.ts:145-149` already fetches it lazily; absent asset = no seed, like today).
   NOT in `createNewGame`/`createBaseGame` (`state.ts:520-575`), `deserialize`/`SAVE_OPTIONAL_DEFAULTS` (`:737`, `:810`: it would re-seed on every load), `importNativeSave` (`saveNative.ts:1065`), `hydrateUnderworldPlot` (`game.ts:5797-5820`: it must not
   re-derive, a boarded skiff must stay gone), Continue/Load. The `?nointro`/demo path (`createNewGame(init)` without a creation, per the comment at `party.ts:214`) stays seedless unless the lead wants it.
3. **`extractor/src/pipeline.ts:415-427`** and `extractor/src/assets-catalog.ts:94`: build `init.ool` as `zeros(0x100) ++ INIT.OOL` (assert `INIT.OOL.length == 256`, output 512), keep `BRIT.OOL`/`UNDER.OOL` out of it (optionally assert they match as a consistency log).
   Output bytes are unchanged today (sha256 d2ec1ae9f0472fcb...), so no asset regeneration is visible; the catalog label becomes `"INIT.OOL (BRIT block = zeros)"`. Fix the comments in `ui/savepanel.ts:94-95,145`.
4. Comments only: `game.ts:3434-3442` ("neto cero" holds for same-world gates; Britannia<->underworld replaces the table, `K:0x4823`/`0x4889` use different files); `game.ts:6964`, `saveNative.ts:509`, `saveNative.ts:954-955` (the memset is `TOWN:0x0fea-0x0ff4`, not `MAINOUT:0x0857`).
5. Not required for D-82 (adjacent, record them): `writeNativeWorldObjects` `saveNative.ts:564-598` and `readNativeWorldObjects` `:750-782` are floor-blind and classify skiffs as `ship`; `buildNativeOol` `:813-846` erases slots 1..23 of the party's block (an invention: the original does not refresh the active
   block); `Game.board()` `game.ts:4938-4941` copies hull/skiffs for any `kind:"ship"`. With the recommended representation none of them is exercised by the seeds; with a `ship`-object skiff all three become live defects (4.4).

What changes in reference state: `state.worldObjects` gains 4 entries, `state.mapOverrides` gains 1 key, in a journey created through the intro and nowhere else; the sidecar JSON carries them; the plain `.gam` live-table bytes do not change (neither encoder writes props or terrain cells).

## 4 Native: exact change needed

Facts (re-read): `AlphaRuntime::...FrontendIntentKind::CreateInitialSave` `alpha_runtime.cpp:3229-3240`: `load_native_state(initial_gam...)`, runtime owners cleared (`:3232-3236`: `terrain_.persistent.clear()` at 3234, `objects_.clear()` at 3236), `apply_new_journey_identity` + `synchronize_loaded_world()` (3237),
`save_.save(... initial_gam ... initial_ool ..., ms, true, intent.slot)` (3240). Boot at `:211` loads INIT.GAM for the title session only. The host fixture re-declares the load (`host_tests/alpha_runtime_host_fixture.cpp:116-117,160`); several tests carry their own copy of the New-Journey document
(`a3_04g_storage_runtime_test.cpp:396-405`, `a4_save1_recovery_runtime_test.cpp:151-162`, `a4_save2_slots_runtime_test.cpp:292-302`, `a4_save3_pc_bridge_runtime_test.cpp:324-346`, `batch26_dungeon_save_test.cpp:226-240`). The PC import path clears the same owners at `:3296-3297` ("as New Journey clears them") and must not seed.

1. **`native/core/src/quest_world.cpp`** (beside `hydrate_underworld_plot_impl`, `:16-29`) + declaration in whichever of `include/openu5/quest_world.h` / `world_terrain.h` avoids the include cycle (`world_terrain.cpp` already includes `quest_world.h`): one production function
   `bool seed_new_journey_underworld(const uint8_t *ool, size_t len, QuestWorldServices &, WorldTerrain &)` with the same classifier as 3.1 (`QuestObject{location 0, floor +4, x, y, tile 286, prop true, slot i}`;
   skiff -> `terrain.set({0,255}, 14, 242, 297, true, "new-journey.seed")` through the persistent layer). Reserve first (`s.reserve(n)`, the pool's contract) and return false (-> `NeedsStorage`/"Initial save could not be created") if it refuses; draws no RNG.
2. **`alpha_runtime.cpp`**: call it in the CreateInitialSave branch after `objects_.clear()` (3236) and before `save_.save` (3240), with `resources_.initial_ool/size` (required to be 512 B by `alpha_resources.cpp:177`).
   The host test twins above stay seedless on purpose except the new test, which must drive the real branch (as `a4_save2_slots_runtime_test.cpp:746-830`) or call the extracted function; do not re-declare the seeding in the fixture (memory rule: host fixture copies hide device wiring).
3. **PC bridge** (`core/src/pc_save.cpp`, ALPHA4_UI.md §5), required once new journeys carry the seeds:
   * `read_vehicles` `:64-83`: also read non-vehicle `0x1e/0x1f` records of both tables (`table_for` `:99-107` already picks live table vs parked block exactly as 2.6) into `prop` pool objects with their slot, add `ImportReport.bodies`;
     the skiff stays a terrain cell (unchanged: P10 `skiffs == 2`, `0:255:14:242 == 0x129`).
   * `complete_export` `:229-300`: first clear the template's body records together with the vehicles (`:261-270`), then write the pool's floor-255/0 props at their own slot (24..27) into `table_for(floor,...)`; otherwise a Native journey exported while the party is outdoors in the underworld (live table = the codec's
     table, which encodes no props) shows no bodies to the original, and the first park overwrites `UNDER.OOL` without them.
   * `place_vehicle` `:85-97`: **never place a record in slots 28..31 of the floor-255 table** (`OUTSUBS:0x0566` rewrites them at every outdoor start while the plot items are untaken, no occupancy check). With the seeded skiff cell present, today's loop (31 down) would write it to slot 31 and the
     original would destroy it at the first underworld entry; skipping 28..31 lands it in slot 23 (24-27 hold the bodies), its original slot.
   * the comment `:259-262` ("a Native journey only has [the skiff] if it was imported") becomes false.
4. `commands.cpp:1013-1068` / `world_terrain.cpp:41` need NO change with the terrain-cell skiff (`ship_at` returns false when no pool object is at the cell, so `commands.cpp:1022-1027` copies nothing; `board_transport` `transport.cpp:57-64` does not touch hull/skiffs for a skiff).
   They WOULD be wrong for a pool `ship` skiff with `hull=0,skiffs=0` (`ship_at` copies zeros onto `g_hull`/`g_skiffs`), as would `persistence.cpp:142-175 object_table` (floor-blind `p[4] = party floor`).
5. Not required (adjacent, record them): `object_table` floor leak and `prop`s absent from `.gam`; `ship()` (`persistence.cpp:98`) classifying skiffs as ships on a DOS-less `.gam` import; `exit_to_overworld` (`transitions.cpp:68-76`) always floor 0 (original: `g_location==0x19` -> 0xFF).

What changes in native state: `objects_` +4, `terrain_.persistent` +1 per new journey, in the sidecar (`gameplay_save.cpp:17,46-55`) of the new slot (`capture_terrain` writes `"0:255:14:242"`, `decode_world_object` accepts slot/prop/floor 255); `.gam`/`.ool` bytes of a new-journey save are unchanged (`build_ool` returns the template for `location != 0`;
`frontend_test.cpp:166` stays green); device cost a few hundred bytes of JSON and 4 PSRAM objects.

## 5 Fixtures and corpora

### 5.1 Must stay byte-identical (the layer choice is what keeps them so)

| pin | why unchanged |
|---|---|
| `native/core/fixtures/*` (foundation, turns, travel, transport, transport-flow, movement-flow, commands, items, magic, combat, dungeon*, ...) and their `--check` drift tests (`typescript_*_drift`, `movement_flow_drift`) | built from hand-made states; only `generate-fixtures.ts:115,120` calls `createNewGame(init)` and compares scalar columns; the seed is outside it |
| `game/src/core/__parity__/*` runners (`run.ts:126`, `combat-run.ts:235` use `createNewGame` and then set position, possibly underworld) | a seed inside `createNewGame` WOULD enter their states: the reason for the new-journey seam |
| `persistence_parity` (`check-persistence.ts` vs `persistence_driver.cpp`) | codec untouched (items 3.5 / 4.5 are not required) |
| `frontend` ctest (`tests/frontend_test.cpp:162-166`, args `game/assets/init.gam`, `game/assets/init.ool`): `build_ool(retained, init_ool) == init_ool` for the New-Journey document | location 13, template returned; also the best witness that the new-game `.ool` is `zeros ++ INIT.OOL` |
| `quest_parity`, `quest_case_table_drift` | `hydrate_underworld_plot` untouched, props are not `plot` |
| `device_smoke_tests.cpp` cases 31/32 (CRC of the new-game exports) | `empty_sidecar` has no objects |
| `game/tests/save-native.test.ts:250-255` ANTI-DRIFT sha256 (`createNewGame(canonicalInit())`, location 13) and `:257-312` (`buildNativeOol`: template kept `:271-287`, underworld party block `:288-300`) | codec untouched |
| `a4_save3_pc_bridge_runtime` P10/T5/T6, `a4_save3_pc_reference` TS7-TS10, mutation `a4_save3_mutation_check.py` P17 ("template's own vehicles kept = the new-game skiff twice") | the skiff stays a terrain cell and the vehicle-only clear stays |
| `game/assets/init.ool` (512 B, sha256 d2ec1ae9f0472fcb...), `native/tools/u5pack/alpha1.ts:481`, `alpha_resources.cpp:177` | extractor change reproduces the same bytes |

### 5.2 Must change, and why

| pin | change |
|---|---|
| `a4_save3_pc_bridge_runtime_test.cpp` P10 (`:549-556`: `objs.size() == 2`) and T5 (`:1177`) | if the bridge imports the bodies as pool props (3/4 item 3): `objs.size()` 2 -> 6 and `ImportReport` gains a body count; `cells == 3`, `r4.skiffs == 2` stay. T6's `table_vehicles` multiset ignores 0x1e, so it needs a sibling that counts bodies in and out |
| `check-pc-save.ts` TS8/TS9 (`:134-135`) | extend `vehicles()` or add a body-count check; the vehicle assertions stay |
| `ALPHA4_UI.md` 5.9 (line 914), 5.10 item 4 (921), 9.8 (2296), 14.5 D-82 row (3341), `ALPHA2_PRESERVATION_LEDGER.md:235`, `re/notes/moonstone-loc-y-pozo-doom.md` 2.4/2.5 | the read site is now derived; the statements "Native never has the skiff" / "read site not derived" become false |
| `pc_save.cpp:259-262` comment | see 4.3 |
| new deliberate-divergence rows | whirlpool-first loses the seeds in the original but not in the ports; slot 23 not occupied by the skiff; `g_hull`/`g_skiffs` per-world in the original; per-world parked monsters; carpet party passes a skiff cell in the original (class 2 on terrain 0) but not in the ports (`TileData` 297 `IsCarpet_Passable false`) |
| tooling | `re/tools/callers_banda.py` `BASES` (Appendix A); `re/tools/thunks.py --bases` label |

Unaffected by construction but to re-check after landing (not run by me): e2e sealed saves (`e2e/espejo-tour/saves` 51 `.gam`, `e2e/grandtour/saves` 18, `re/tools/*sellos*.mjs`) only if a flow creates the game through the main.ts "create" action; host tests that start through the frontend's New Journey and then assert an empty pool
(`a3_hf1_arena_loot_test.cpp:640` uses `attach_host_test_fixture`, i.e. NOT the CreateInitialSave branch, so it is unaffected; `a4_save2_slots_runtime_test.cpp:746-830`, `a4_ui2_save_menu_runtime_test.cpp:394`, `a4_ui3_save_ux_runtime_test.cpp:453-465` do use it and assert slot rows/footers, not pool contents).

## 6 Test plan

RED-first order: write the reference tests, watch them fail against today's code (no seeds), implement 3, then the native tests (RED against today's native), implement 4; no fixture regeneration is expected (if a generated corpus changes, the seed leaked into `createNewGame`).

### 6.1 Reference (vitest, `game/tests/new-journey-underworld-seed.test.ts`, names suggestive)

* **T1 derivation.** `seedNewJourneyUnderworld(createNewGame(canonicalInit()), oolBytes)` with `oolBytes = zeros(256) ++ INIT.OOL` built from a committed 40-byte literal (`INIT.OOL[0xb8..0xdf]`, so the test does not depend on the gitignored `game/assets`): exactly
  `mapOverrides == {"0:255:14:242": 297}` and four `prop` objects `(location 0, floor 255, tile 286, slot 24..27)` at (103,226) (105,227) (107,227) (108,225); nothing at floor 0; no `hull`/`skiffs`; the game RNG state is unchanged. Asset-gated drift test: the same bytes equal `game/assets/init.ool[0x1b8..0x1df]` and `original/.../INIT.OOL[0xb8..0xdf]` when present.
* **T2 layer.** `createNewGame(init)` still returns no `worldObjects`/`mapOverrides`; `importNativeSave` of the DOS fixture (`original/u5/ultima5/SAVED.GAM` + sidecar-less) gains none; `Game` built from `createNewGame` has none; only the creation seam seeds.
* **T3 walk/board.** Seeded state, position floor 255 at (13,242): a step east succeeds with no "Blocked!", position (14,242); `board()` -> messages `"skiff"`, `transportTile 0x29`, `shipHull`/`shipSkiffs` unchanged (set non-zero sentinels first), the override gone (cell reverts to terrain 0x03), a second `board()` -> "What?"; `exitVehicle` re-drops at the party cell. Foot party stepping into each body cell -> "Blocked!" (also from a horse, a skiff and a frigate transport); stepping onto (105,225) works; (G)et/(O)pen/(S)earch toward a body do nothing special; (L)ook says "a corpse" (286) and "a skiff" (297).
* **T4 persistence.** save/load round trip in the underworld keeps 4 props and the cell; after boarding, save+load shows no cell and re-entry through the Falls (`game.ts:2147-2152`), moongate (`:3434-3436`) and dungeon exit does not re-create it; `hydrateUnderworldPlot` (`:5797-5820`) leaves props and the cell (filters `kind==="plot"`); `discardInteriorObjects(13)` leaves location-0 props.
* **T5 pool.** `composeWorldPool` contains slots 24..27 for the props; `findFreeActorSlot` with 28..31 occupied by the plot reseed returns 23 in the reference (the binary's answer is 22 because slot 23 holds the skiff: declared divergence, assert the declared value).
* **T6 classifier boundaries.** byte 0x77 in the UNDER block -> reported, not placed; record with `+4 == 0` inside the UNDER block -> reported, skipped; records in slot 0 ignored; 0x1f body accepted as a prop; 0x28/0x2a/0x2b skiff variants -> override with `0x100+b`; frigate 0x24 with +5 = 99, +7 = 2 -> a `ship` object carrying that hull/skiffs (not a prop, not dropped).
* **T7 extractor.** `init.ool` built from `INIT.OOL` equals the committed asset (sha256 d2ec1ae9f0472fcb...) and **differs** when `BRIT.OOL`/`UNDER.OOL` in the input folder are replaced by different content (negative control for the provenance defect).

### 6.2 Native (new `a4_parity2_d82_runtime` host test + CMake entry beside `a4_save3_pc_bridge_runtime`, `CMakeLists.txt` ~2063-2110)

* N1-N3 as T1/T2/T6 through the real CreateInitialSave branch (drive the frontend like `a4_save2_slots_runtime_test.cpp:746-830`), reading `h.rt->objects_for_test()` and `command_context_for_test().terrain->persistent`: exactly 4 props + 1 cell, `QuestObject` fields as above; the seeds are not present after Continue of a save made after boarding.
* N4 walk/board/look in the real command path (`commands.cpp:1013-1068`): same sentinels on `ship_hull`/`ship_skiffs`.
* N5 bridge: (a) import of the fixture `SAVED.GAM`+`SAVED.OOL` (UNDER block = INIT.OOL) yields the same 4 props + cell as a New Journey; (b) export of a new journey with the party outdoors in the underworld has the four bodies in the live table at slots 24..27 and the skiff NOT in 28..31 (it lands in 23); export from Britannia keeps them in the UNDER block; (c) export -> import -> export is idempotent (no doubling); (d) P17's mutation still goes RED.
* N6 sidecar: `capture_terrain` emits `"0:255:14:242":297`, `capture_world_objects` the four props; a load restores them and does not re-seed; heap/reserve: the pool refuses -> the New Journey reports failure and writes nothing.
* N7 unchanged-bytes controls: `frontend` ctest, device smoke 31/32, `persistence_parity`.

### 6.3 Mutations the RED tests must kill (`native/core/tools/a4_parity2_d82_mutation_check.py`, reference counterpart in the TS suite)

| mutant | killed by |
|---|---|
| seed inside `createBaseGame`/`createNewGame` | T2, `__parity__` and `importNativeSave` controls |
| prop tile 30 (0x1e) instead of 286 | T3 (cell walkable, `LeftDesert2`) |
| floor 0 or -1 instead of 255 | T1, T3 |
| skiff as `ship` object with hull 0 / skiffs 0 | T3 sentinel check (native: N4); also a Britannia export containing a 0x29 record in the live table |
| re-seed on load/Continue | T4 / N1 |
| slot 23..26 instead of 24..27 | T1, T5 |
| unknown class silently dropped | T6 |
| `hydrateUnderworldPlot` purge broadened to `floor===0xff` | T4 |
| bridge `place_vehicle` allowing 28..31 | N5(b) |
| bridge export clearing the template bodies without rewriting them | N5(b) |
| extractor deriving `init.ool` from `BRIT.OOL ++ UNDER.OOL` | T7 negative control |

Boundary values to include: x,y as bytes (y=242 and 226/227/225 are < 255, no wrap; do not sign-extend `242`), floor byte 0xFF vs interior floor -1 (`quest_world.cpp:46`, `n.z==255 ? -1`), slots 23/24/27/28, cell (105,225) (Amulet, not a body, plot item coexists), neighbours (13,242) (14,243) (15,242) (14,241).

## 7 Residual unknowns (exact)

1. **1988 vs Upgrade 1.0.** Only the 2001-patched set is in the tree; `FONT`, `INTRO`, `MAINOUT`, `ULTIMA.EXE` (and TOWN/DUNGEON/ENDGAME) are patched files and `History.txt` documents music changes only, so the `.OOL` logic is probably the 1988 logic, but that is an inference. The OUTSUBS/CAST2/CMDS/SJOG sites (1996-12-25) are not in the patched set. Settle: diff pristine 1988 `ULTIMA.EXE`, `FONT.OVL`, `INTRO.OVL`, `MAINOUT.OVL` over 2.1-2.6.
2. **Run-time witnesses missing** (all static): whirlpool-first loss of the seeds (2.6a), per-world `g_hull`/`g_skiffs` (2.6b), foot step from (13,242)/(14,243) onto the skiff and "skiff" boarding (2.7), "Blocked!" at a body. Settle: DOSBox from a new game (breakpoints on `K:0x25d8`, watch `0x5C5F`, step into the cell).
3. **Practical reachability of the skiff.** Enterable and boardable once adjacent, but the 6-cell foot pocket (2.7.9) is cut off from the Falls arrival by the 4-neighbour foot bitmap. Not analysed: moongate arrivals from buried moonstones (destinations come from the `.GAM` window tables `DS:0x5830-0x5848`, zero in INIT.GAM), (K)limb (adds `IsKlimable` tiles), Blink/Gate spells (`CAST:0x0740`, `K:0x7b7e`). Settle: reachability search over those movers or a DOSBox walk.
4. **Unclassified `K:0x3a74` callers**: `BLCKTHRN:0x06f0 0x0809 0x0821 0x0854 0x0875 0x09c3`, `TOWN:0x16ab 0x17f5` (indoor overlays), and the callers of `K:0x368e` outside the verified set (MAINOUT 0x075b, CMDS 0x0688 0x16bf 0x172e, CAST 0x16be, SJOG 0x03f7 0x056d 0x059a 0x0e35, SHOPPES 0x0819, TALK 0x045e 0x048a, TOWN 0x069c 0x0a9a) and of `K:0x3702` (TOWN 0x0d86, NPC 0x0c32, SJOG 0x1e52). None can run outdoors in the underworld as far as their overlay/context shows; per-site slot provenance would close it.
5. **`LOOKOBJ` Look flow** for an object hit was read only to the `K:0x368e` call and the `LOOK2.DAT` strings, not through `0x06a4`; the prefix "Thou dost see" (DS:0x751c) is B's reading.
6. **Pixel-level draw order** of slot 0 over a record in the same cell, and the decal branch's behaviour when two decals share a cell (`0x5513` skips the second): not needed for the seeds, not verified by emulation.
7. **Adjacent divergences recorded, not decided**: the per-world monster table (the ports keep one `overworldEnemies`), `g_hull`/`g_skiffs` per world, town 0x19 exit floor, carpet-vs-skiff-cell passability, slot-23 occupancy. The lead decides whether each becomes a deliberate row.
8. **Representation decision** (1.3): terrain-cell skiff (recommended) vs a slotted object. The object route needs, at minimum, a board gate on frigate tiles in `Game.board()`/`commands.cpp:1022-1027`, floor-filtered encoders (`saveNative.ts:564`, `persistence.cpp:142`), and a decision on `ship()` classification; it buys only slot-23 occupancy.
9. `NPC.OVL` base is inconclusive by call statistics (13/54); A took 0xA290 from thunk matches. Not used here.

---------------------------------------------------------------------------------------------------------------------------

## Appendix A Corrected overlay base table (brief and `callers_banda.py` replaced)

Verified by brute force over all bases (my `basefit.py`) and, for OUTSUBS/MAINOUT/DUNGEON/INTRO, by the kernel thunk targets.

| overlay | base | brief / `callers_banda.py` | overlay | base | brief / `callers_banda.py` |
|---|---|---|---|---|---|
| TOWN, MAINOUT | 0x81D0 | ok | CAST | **0xBF80** | 0xA8D8 |
| DUNGEON | **0x81D0** | 0xE1E0 | CAST2, FONT, ZSTATS | 0xE1E0 | CAST2 0xC29E in the tool |
| INTRO (file has a 0x10-byte header) | **0x81C0** | 0xCD3A | COMSUBS | **0xE1E0** | 0x85FE |
| OUTSUBS | **0xA290** | 0x81D0 | SHOPPES2, SHOPPES3 | **0xE1E0** | 0xA89E / 0xA5F6 |
| NPC, SHOPPES | 0xA290 | ok | LOOKOBJ | **0xA290** | 0xA444 |
| TALK, CMDS, SJOG | 0xBF80 | tool says TALK 0xA290 | DNGLOOK | **0xA290** | 0xA2B6 |
| COMBAT, BLCKTHRN, ENDGAME | **0xA290** | 0xBFEC / 0xE63E / 0xE84C | FLAMES | not fitted (1 call) | 0xEA94 |

## Appendix B Disagreement ledger

| point | A | B | C | resolved by |
|---|---|---|---|---|
| foot party can enter the skiff cell | no (terrain 3 blocked) | yes (composited terrain 0) | no | 2.7 items 1-5: B |
| skiff representation | object (hazards listed), lead decides | terrain cell | terrain cell | 1.3 / 2.7.5 / 4.4 |
| insertion layer | new-journey seam | `createNewGame` or new `ExtractedInitialState` fields | never in `createNewGame` | 3.2: new-journey seam |
| `K:0x256e` callers | 45 | 47 | 47 | 47 (census) |
| `MAINOUT:0x0ae1` context | town tile | not stated | party death | C (2.2) |
| `INTRO` U4 buffer integrity | n/a | open | open | settled statically (2.3) |
| bodies removable? | "bodies are decor" | decor | "no code removes a 0x1E record" | 2.7.8 classification (strengthened) |
| ports' memset attribution | wrong site (TOWN) | not stated | not stated | A confirmed (2.6) |
| everything else (read sites, record bytes, whirlpool, save, bases) | agreed | agreed | agreed | re-derived, no refutation |
