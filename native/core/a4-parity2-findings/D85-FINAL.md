# D-85 FINAL (reconciled) -- the location-29 (Stonegate) trapdoor

Reconciler's note. I read D85-A.md and D85-B.md, then re-disassembled everything myself in this session
(`re/tools/dis16.py`, capstone 16-bit; scratch scripts `findings/d85_fin_*.py`, `findings/ovdis.py`). Nothing in the
repository was touched. Evidence grades: BINARY = disassembled here, instructions quoted; PORT/NOTE = read from source.

Conventions: `TOWN 0xNNNN` = file offset in TOWN.OVL (base 0x81D0, kernel address = `(target + 0x81D0) & 0xFFFF`);
`K 0xNNNN` = ULTIMA.EXE image offset (MZ header skipped, = CS:IP of the notes); `DS 0xNNNN` = data-segment offset
(DATA.OVL file offset = DS + 0x10). Pascal-style calls: first pushed argument is the first parameter, callee pops.

---------------------------------------------------------------------------------------------------------------

## 1. Verdict

1. **D-85 is real and exactly as the ledger says.** The Stonegate kill loop (TOWN 0x0ff9-0x103a) is bounded by the BYTE
   `[0x585b]` = party size, re-read each iteration, unsigned compare. It writes, for roster index `0 .. party_size-1`
   only: HP word (`rec+0x10`) := 0 and status byte (`rec+0x0b`) := 0x44 ('D'). Inn companions (index >= party_size) are
   not read, not written, get no sound and no panel redraw. Both ports kill the whole roster:
   native `quest_world.cpp:234` (`character_count`), reference `game.ts:2720` (`state.characters`). On a stock save this
   is the 16-record roster (INIT.GAM / SAVED.GAM in `original/u5/ultima5` both hold 16 records with party size 3), so a
   default game loses 13 never-recruited or inn-parked characters in one stroke. No test sees it because every test has
   roster == party (`trapdoor-fall.test.ts:82`, `a3_03_sfx_runtime_test.cpp:143`).
2. **Larger finding, confirmed by me in the binary, not in the ledger (both investigators found it independently):**
   BEFORE the `cmp [0x5893],0x1d` test at TOWN 0x0f96 every trapdoor (all locations, every lap of a chained fall, and
   location 29 too) executes `K 0x5910` (viewport redraw + wind roll) and `K 0x2aa8` (`party_random_damage`:
   `rand(1,8)` per living party member). Both ports omit both and both say the branch has "no RNG" (`game.ts:2674`,
   `turn.ts:549`). That claim is false for the binary. The kill tail itself (0x0fa0-0x103a) draws nothing.
3. **Which artefact is wrong:** the reference and the native port are both wrong (roster bound; missing prologue).
   `re/notes/loops.md` s.2.0 (line 114-115) is RIGHT about the prologue but mislabels the tile ("Tile de dano 0x8C (Blackthorn
   exec) / 0xBC / 0x8F ... @0x0F8A/0x10BA") and the port header comment `turn.ts:459` copied the incomplete version
   (lists 0xBC/0x8F only, and its 0x0F8A bullet was never implemented). The comments `game.ts:2674`, `game.ts:2719`
   ("TODO el roster (no solo al party)", citing 0x0ff6-0x1037 which in fact loops over `[0x585b]`) and `turn.ts:549`
   are wrong. I did not re-read `re/notes/tpk-112-acta.md` / `tpk-113-acta.md` end to end; the lava fill (0x0fd6) and the 0x5c5a wipe (0x0fea) they describe match what I disassembled.
   `re/disasm/*.asm` (cited by many notes) does not exist in the tree.
4. **Tooling/brief correction (affects other batches):** BLCKTHRN.OVL loads at **0xA290**, not 0xE63E. `callers_banda.py`
   BASES and the brief carry 0xE63E; every BLCKTHRN caller is missed by the stock census. See 2.0.
5. After the kill nothing returns to the caller as "game over": the turn finishes, housekeeping and the NPC tail run once,
   the loop head TOWN 0x1436 sees `K 0x39fc == -1` and runs the Refuge (thunk K 0x7a5e -> BLCKTHRN 0x0910).
   Revive covers party_size only (the ports' `resolve_refuge` / `partyRefuge` already do, so after the fix a living
   companion at an inn stays alive and the dead-companion defect disappears).

---------------------------------------------------------------------------------------------------------------

## 2. Binary facts

### 2.0 Controls run first

* Overlay arithmetic: TOWN 0x0f2a `call 0xffff9ec2` -> `(0x9ec2+0x81d0)&0xffff` = K 0x2092 (prologue `55 8b ec 56 57 1e`, the
  `rand(lo,hi)`: ror-3 xor LCG at DS 0x5420; `bx=[bp+6]`=lo (first push), `cx=[bp+4]`=hi, `ret 4`). TOWN 0x0f8a
  `call 0xffffd740` -> K 0x5910; 0x0f8d `0xffffa8d8` -> K 0x2aa8; 0x0f57 `0xffffc232` -> K 0x4402; 0x1021 `0xffffa730` ->
  K 0x2900; 0x101e `0xffffa06c` -> K 0x223c; 0x0fbb `0xffffa112` -> K 0x22e2; 0x0fc6 `0xffff9ef8` -> K 0x20c8; 0x0fd3
  `0xffffa13e` -> K 0x230e; 0x10d0 `0xffffa918` -> K 0x2ae8; 0x1436 `0xffffb82c` -> K 0x39fc. Every one lands on `55 8b ec`.
  Prologue-hit rate with base 0x81D0 over all TOWN.OVL near calls: 144 of 178 (the rest are overlay-local or data).
* Positive control for the by-band census: `callers_banda.py 2092` lists TOWN 0x0f2a and 0x108d (the wake draw and the swamp draw I
  read) among 12 TOWN sites.
* String control: DS 0x2768 -> DATA.OVL file 0x2778 = `A TRAPDOOR!\n\0`; DS 0x2780 = `Burning!\n\0`; DS 0x2652 word table =
  {0x2626 "TOWNE.DAT", 0x2630 "DWELLING.DAT", 0x263d "CASTLE.DAT", 0x2648 "KEEP.DAT"}.
* **BLCKTHRN base = 0xA290** (A and B both right, the brief wrong). Scan of every `E8` rel16 in BLCKTHRN.OVL: prologue-hit rate
  with base 0xA290 = 129/136; 0xE63E = 0/125; 0xA89E = 0/136; 0x81D0 = 0/88. The descriptor table at ULTIMA.EXE image 0x7780 (16-byte
  records, word +4 = size in paragraphs, word +0xA = load segment) row 8 = size 0x00c7 paragraphs (3184 B = BLCKTHRN.OVL), segment 0x0a29.
  Thunk K 0x7a5e = `lcall 0x72e:0x2ec ; db 08 00 ; ljmp 0:0xaba0`; 0xaba0 - 0xa290 = 0x910; BLCKTHRN 0x0910 = `55 8b ec 83 ec 12 57 56`;
  BLCKTHRN 0x091c `call 0x828e` -> K 0x251e. (Other bases in the brief were not re-derived for this item; my own best-fit scan
  `d85_fin_census.py` is only a heuristic for DUNGEON/COMSUBS etc. and I do not rely on it for any claim.)
  Stock `callers_banda.py` therefore misses BLCKTHRN callers; for D-85's routines I re-ran with 0xA290 (see 2.9): BLCKTHRN has
  no caller of K 0x2aa8, K 0x2ae8, K 0x39fc or TOWN post_turn (it has eight K 0x5910 callers, irrelevant here).

### 2.1 Roster / party layout (BINARY + stock-save evidence)

* Records: DS 0x55a8 + 0x20*i, 16 records (0x55a8 + 16*0x20 = 0x57a8 = the food word). Fields proven by this very code:
  status byte `rec+0x0b` (`[si+0x55b3]`, si = i<<5), HP word `rec+0x10` (`[si+0x55b8]`), and `rec+0x1f` (`[si+0x55c7]`).
* INIT.GAM and SAVED.GAM (4192 B, game window starts at DS 0x55A6): record i at file offset 2 + 0x20*i; byte 0x2B5 = DS 0x585b = **3**;
  records 0-2 have `+0x1f` = 0, records 3-15 `+0x1f` = 255, all 'G' with HP > 0. So `[0x585b]` is the party count (3), the roster is 16.
* Writers of DS 0x585b: exhaustive byte-pattern scan of ULTIMA.EXE and every overlay (`fe 06 5b 58` inc, `fe 0e 5b 58` dec,
  `a2 5b 58`, `c6 06 5b 58`, `88 /r 5b 58`): TALK.OVL 0x0912 `inc` (after the join swap: TALK 0x08c8-0x0910 zeroes `[rec+0x1f]`,
  copies the record to a stack temp, and swaps it into index `[0x585b]` = first slot past the party), SHOPPES3.OVL 0x083a `inc`,
  SHOPPES3.OVL 0x0472 `dec`, BLCKTHRN.OVL 0x04d4 `dec`. Everything else is the bulk save read. So party = records `0..[0x585b]-1`,
  non-party = `[0x585b]..15`.
* Every party loop uses it as bound: K 0x39fc (3a0e), K 0x2ae8 (2afa, 2b4d), K 0x2aa8 (2ab7), TOWN post_turn wake loop (0f0f, 0f3b).

### 2.2 `post_turn` TOWN 0x0f02 (kernel 0x90d2), in order (all quoted from my disassembly)

```
0f02 push bp / mov bp,sp / sub sp,0xc / push di / push si            ; locals [bp-2]=fell flag [bp-4]=index/ramp [bp-6]=saved transport tile [bp-8]=tile
0f0a mov word [bp-4],0
0f0f mov al,[0x585b] / sub ah,ah / or ax,ax / je 0x0f48              ; party size 0 -> skip wake loop
0f18 mov si,0x55b3 ; 0f1b mov di,[bp-4]
0f1e cmp byte [si],0x53 / jne 0x0f35                                 ; 'S'
0f23 sub ax,ax / push ax / mov ax,0xf / push ax / call K 0x2092       ; rand(0,15)  -- ONE draw per sleeping member, index order
0f2d cmp ax,0xf / jne 0x0f35 / mov byte [si],0x47                    ; wakes only on 15
0f35 add si,0x20 / inc di / mov ax,di / mov cl,[0x585b] / sub ch,ch / cmp ax,cx / jb 0x0f1e
0f45 mov [bp-4],di
0f48 mov word [bp-2],0                                                ; <-- loop head (a rescan re-enters HERE, not at 0f0a)
0f4d mov al,[0x5896] / sub ah,ah / push ax                           ; first push  = x
0f53 mov al,[0x5897] / push ax                                       ; second push = y
0f57 call K 0x4402                                                    ; tile_ptr
0f5a mov bx,ax / mov al,[bx] / sub ah,ah / mov [bp-8],ax
0f63 cmp ax,0x8c / je 0x0f6b / jmp 0x1050                            ; TRIGGER: tile byte (zero-extended) == 0x8c (140)
0f6b mov al,[0x587c] / and al,0xfe / cmp al,0x14 / jne 0x0f77 / jmp 0x1050   ; transport tile 0x14/0x15 (carpet) flies over: no message, no effect
0f77 mov ax,0x2768 / push ax / call K 0x1850                         ; print "A TRAPDOOR!\n"
0f7e mov al,[0x587c] / sub ah,ah / mov [bp-6],ax                     ; save transport tile
0f86 mov [0x587c],ah                                                  ; (ah = 0) transport tile := 0 for the redraw
0f8a call K 0x5910                                                    ; PROLOGUE 1: viewport redraw incl. wind roll
0f8d call K 0x2aa8                                                    ; PROLOGUE 2: party_random_damage
0f90 mov al,[bp-6] / mov [0x587c],al                                  ; restore transport tile
0f96 cmp byte [0x5893],0x1d / je 0x0fa0 / jmp 0x103c                 ; location == 29 ?  -> kill / fall
```
`K 0x4402` for `1 <= [0x5893] <= 0x7f` (4420 `cmp [0x5893],0`, 447e..44a8): both arguments checked `0..0x1f`
(`jl`/`jg` -> constant 0x6a07), result `0x6608 + (a2<<5) + a1` with `a1=[bp+6]` = first push = x and `a2=[bp+4]` = second push = y.
So x = `[0x5896]`, y = `[0x5897]`; tile = byte at DS 0x6608 + y*32 + x. `cmp ax,0x8c` at 0x0f63 is the only `3d 8c 00` in TOWN.OVL.

### 2.3 The location-29 branch, 0x0fa0-0x10c7 (exact)

```
0fa0 sub ax,ax / push ax / call K 0x0a70                 ; set_color(0) (black)
0fa6 mov ax,8 / push ax / push ax / mov ax,0xb7 / push ax / push ax / call K 0x0aa6   ; rect (8,8)-(0xb7,0xb7): the 11x11 viewport, instant fill
0fb3 mov word [bp-4],0x3e8
0fb8   push [bp-4] / call K 0x22e2                        ; set_tone(freq)   (gated on word [0xa9ce], the sound flag)
0fbe   mov ax,1 / push ax / mov ax,0x28 / push ax / call K 0x20c8   ; delay(1,0x28)   (NOT gated by the sound flag)
0fc9   dec word [bp-4] / cmp word [bp-4],0xfa / jg 0x0fb8           ; signed: 1000 down to 251 = 750 iterations
0fd3 call K 0x230e                                        ; speaker off
0fd6 mov cx,0x400 / mov di,0x6608 / mov ax,ds / mov es,ax / mov ax,0x8f / repne stosb   ; whole 32x32 local map := 0x8F (143, lava)
0fe5 mov byte [0x24e6],1                                  ; full viewport redraw flag
0fea mov cx,0x100 / mov di,0x5c5a / mov ax,ds / sub ax,ax / repne stosb  ; 256 B at DS 0x5c5a := 0 (32 actor/object slots x 8 B)
0ff6 mov [bp-4],ax                                        ; i := 0   (ax == 0)
0ff9 mov al,[0x585b] / sub ah,ah / or ax,ax / jne 0x1005 / jmp 0x10c7      ; party size 0 -> done
1005 mov si,0x55b8 / mov di,0x55b3                        ; HP word / status byte of member 0
100b mov word [si],0                                      ; HP := 0   (16-bit store)
100f mov byte [di],0x44                                   ; status := 'D'
1012 mov ax,0x28 / push ax / mov ax,0xbb8 / push ax / mov ax,0x1f4 / push ax / call K 0x223c   ; noise_burst(step 0x28, total 0xbb8, hi 0x1f4)
1021 call K 0x2900                                        ; stat-panel redraw
1024 add si,0x20 / add di,0x20 / inc word [bp-4]
102d mov al,[0x585b] / sub ah,ah / cmp [bp-4],ax / jb 0x103a   ; UNSIGNED word compare, bound re-read each pass
1037 jmp 0x10c7 ; 103a jmp 0x100b (back edge)
10c7 cmp word [bp-2],0 / je 0x10d0 / jmp 0x0f48           ; [bp-2] is still 0 on this path: NO rescan
10d0 call K 0x2ae8 ; ret                                  ; housekeeping, once
```
* Bound variable: `byte [0x585b]`, zero-extended, `jb` (unsigned). **Not** the roster; **no cap at 6 or 16** in this loop
  (the 6 cap is in K 0x2aa8, K 0x2ae8's bound is also `[0x585b]` only). Body runs before the first compare; the pre-check at
  0ff9 handles an empty party.
* Indices touched: `0 .. [0x585b]-1`, in order. Per touched member: ONLY word `[0x55b8+0x20*i]`=0 and byte `[0x55b3+0x20*i]`=0x44.
  No other field (max HP, MP, exp, level, equipment, `+0x1f`) is written. `[0x587b]` (active character) is not touched in the loop.
* No status filter: 'G', 'P', 'S' and already-'D' members are all rewritten (a dead member with non-zero HP ends at 0);
  every touched member, including an already-dead one, still gets the burst and the panel redraw.
* Non-party roster members (indices `>= [0x585b]`): untouched.
* RNG in 0x0fa0-0x103a: **none**. I re-ran a recursive-descent closure (`d85_fin_reach.py`, direct near calls inside ULTIMA.EXE) from
  K 0x0a70, 0x0aa6, 0x22e2, 0x20c8, 0x230e, 0x223c, 0x2900, 0x2a28: none reaches K 0x2092. Controls: the same scan from K 0x2aa8 and
  K 0x2f62 DOES reach it. (`0x0a70/0x0aa6` end in `lcall [0x5350]` into the display driver, outside the game's RNG.) The noise
  burst pitch comes from the private LCG at DS 0x545c (K 0x2255-0x2265), not DS 0x5420.

### 2.4 Non-29 branch (for the boundary tests)

```
103c dec byte [0x5895]                    ; floor byte, no bounds check: 0 -> 0xFF (z = -1)
1040 mov ax,1 / push ax / call 0x0408     ; TOWN local-map loader, arg 1 = reposition (NPC schedule); reads KEEP/TOWNE/... record [0x1e19+loc]+floor
1047 mov word [bp-2],1 / jmp 0x10c7       ; -> 0x10c7 jne -> jmp 0x0f48 : re-read the tile on the new floor (the wake loop is NOT repeated)
```
Every lap repeats the whole prologue (message, 0x5910, 0x2aa8). Housekeeping 0x10d0 runs once, after the chain.

### 2.5 The prologue callees (this is what both ports omit)

K 0x2aa8 `party_random_damage` (prologue `55 8b ec 83 ec 04 57 56`), quoted:
```
2ab0 sub si,si / mov di,0x55b3
2ab5 mov ax,si / mov cl,[0x585b] / sub ch,ch / cmp ax,cx / jae 0x2ad6     ; slot >= party size: skip
2ac1 cmp byte [di],0x44 / je 0x2ad6                                         ; ONLY 'D' is skipped ('S','P','G' are hurt)
2ac6 push si / mov ax,1 / push ax / mov ax,8 / push ax / call K 0x2092     ; rand(1,8)
2ad2 push ax / call K 0x2a52                                               ; apply_damage(i, dmg)
2ad6 add di,0x20 / inc si / cmp si,6 / jl 0x2ab5                           ; always 6 iterations
```
K 0x2a52(i,dmg): `call 0x2a28(i)` (invert roster row), `noise_burst(0xa,0x640,0x7d0)` (K 0x223c: 160 iterations, pitch 100..2000),
`call 0x2a28(i)`, `HP -= dmg` (word), `if HP <= 0 (signed jg skips)`: HP := 0, status := 0x44, and if `[0x587b] == i` then `[0x587b] := 0xff`;
then `call 0x2900`. No game-RNG inside.

K 0x5910 (prologue `55 8b ec 83 ec 0a 57 56`): `[0x545e]=0xff`; `if [0x587a]==0x54 ('T') [0x5891]=0`; `if [0x58a4]==0` skip to 5a1d
(**[0x58a4] = 1 in towns**: the only writers are TOWN 0x121d `mov byte [0x58a4],1` and MAINOUT 0x0011); if `[0x5891]==0` skip wind+animation;
else (unless `[0x5891]==0xff`) `call 0x4552` (actor/tile animation bytecode interpreter), then **`call 0x2f62`**, then (loc < 0x80)
0x475a / 0x70a6, redraw (`[0x24e6]`), 5a1d `[0x5891]=1`.
K 0x2f62 `maybe_change_wind`: `rand(0,63)`; nonzero -> return; zero -> loop { `si = rand(0,4)`; if `si == 0`: `rand(0,255) >= 0xc0` (signed `jge`) accepts, else re-roll } then `K 0x2e96(si)`.
The ports' `maybeChangeWind` / `maybe_change_wind` (turn.ts / turn.cpp:112-118) match 0x2f62 draw for draw.
K 0x4552 contains `rand` calls at K 0x4625 (0,255; `jge 0x80`), 0x466d (0,255; `jge 0x40`), 0x469f (0,255; `jge 0xc0`) for slots whose tile byte >= 0x34 and that
pass further gates; the ports do not model them anywhere (Class-3 declared divergence, `re/notes/wind-rand-decision.md`) and the existing Burning path
(TOWN 0x10ba) has the same residue. Unmodelled, identical to the Burning treatment.

Other 0x0f8a/0x0f8d twins: TOWN 0x10ba/0x10c4 (Burning!, tiles 0xbc and 0x8f; there the order is wind, message, damage), TOWN 0x10e4 (inside a different
thunk-entered routine at 0x10da, string DS 0x278a "Begone,..."): not trapdoor-related.

### 2.6 RNG order of a trapdoor turn in a town (all locations; location 29 stops after step 4)

1. (before post_turn) per-key wind in `town_read_command` (TOWN 0x0dd0, already modelled as site "wind"), clock advance (K 0x4f7c, 0x15d4).
2. wake loop: one `rand(0,15)` per 'S' member among `0..party_size-1`, index order.
3. `K 0x5910` (0x0f8a): the wind roll(s) (none if time spell 'T'; the other gates are always true here; plus the unmodelled 0x4552 draws).
4. `K 0x2aa8` (0x0f8d): one `rand(1,8)` per non-'D' member among `0..min(party_size,6)-1`, index order; dying members become 'D' immediately.
5. Non-29: `dec floor`, reload, rescan -> repeat 3-4 on the new floor. Location 29: kill (no draws).
6. housekeeping 0x10d0 (K 0x2ae8) once; NPC tail.

### 2.7 What happens after (location 29)

* `ret` to the town command loop. The caller `TOWN 0x15ec call 0xf02` (single call site) is preceded by `K 0x4f7c(1)` clock advance (0x15d4) and by the
  pre-turn gate 0x15bf `call K 0x39fc / inc ax / jne` (a fully-dead party never reaches post_turn). After post_turn: 0x160d-0x161c copy x,y,floor into slot 0
  of the just-zeroed table (`[0x5c5c]`,`[0x5c5d]`,`[0x5c5e]`), the NPC tail runs (gates 0x1642/0x1649), then 0x1686 -> `jmp 0x142c` -> loop head.
* Loop head 0x1436 `call K 0x39fc`: returns 0 on the first 'G'/'P' among the first `[0x585b]` members, else 1 if some 'S' was counted, else -1.
  After the kill all are 'D' -> -1 -> 0x1456 `cmp [bp-8],-1 / jne` -> `jmp 0x1862`: `call K 0x0e26` (driver fn 3), `call K 0x7a5e` (BLCKTHRN 0x0910 `party_refuge`),
  `call K 0x0e1d`, `jmp 0x145f`. No game-over screen, no stay-dead state. The Refuge is the same routine a combat wipe uses.
* Housekeeping at 0x10d0 (K 0x2ae8) runs on the dead party: loop 2b0b-2b55 over `[0x585b]` members; for status 'D' with index == `[0x587b]`
  (2b14-2b25) it sets `[0x587b] := 0xff`. So after the TPK the original has no active character selected.

### 2.8 Text, timing, sound (all of it)

* Text: `A TRAPDOOR!\n` (DS 0x2768) once at 0x0f77, before any effect. Nothing printed in the kill path. (Refuge prints its own lines.)
* Screen: transport tile zeroed for the 0x5910 redraw (so the walker is shown), then viewport filled black (instant; EGA.DRV service 0x3f read by B only, presentation),
  lava written to the buffer with the viewport already black, never displayed.
* Delays: `K 0x20c8(sel,n)` = `ax = [0x5356]; cx = word [sel + 0x5426]` (**`bx = [bp+6]`, i.e. `sel` as an UNSCALED byte index**; B's "[0x5426+2*arg]" is wrong);
  `if cx != 0: ax >>= cl`; then `n` times `[0x5422]=ax; do dec [0x5422] while != 0`. The table at DS 0x5426 reads (image bytes, never written by code: the only
  `26 54` operand sites are K 0x20d4 read, 0x20df/0x20ec on 0x5424) `00 00 00 00 01 00 01 00 02 00 02 00`, so for `sel=1` the word at 0x5427 is 0: **no shift**.
  `[0x5356]` is calibrated in K 0x11b4-0x120b (iterations counted during one INT 1Ch tick, times 0x12, divided by 0x2ee).
* Work per piece, in units of N = `[0x5356]` decrement iterations: ramp = 750 x 0x28 x N = 30000 N; each kill burst = 75 x 0x28 x (N>>4) ~ 187.5 N (1/160 of the ramp);
  each prologue damage burst = 160 x 0xa x (N>>4) = 100 N (1/300 of the ramp). The noise-burst waits are NOT gated by the sound flag (only the speaker enable and set_tone are).
  Absolute seconds cannot be fixed from the bytes (depends on N and the cycle cost of two different loops); the repo's "28 s" for the ramp is secondary.
* SFX in order: prologue damage bursts (1 per living member, 0x2a68), ramp 1000..251 (set_tone, K 0x22e2), speaker off, kill burst per PARTY member (0x101e).

### 2.9 Location, tile, callers

* Location 29: TOWN 0x0408 loader file = word table DS 0x2652 indexed `(loc-1)>>3` = 3 -> KEEP.DAT; record = `byte[DS 0x1e19+loc]` + signed floor = **6** (+0), and
  `[0x1e19+30]` = 7, so Stonegate has exactly one map. (KEEP.DAT row 4 of its 8 locations 25..32: Ararat 0, Bordermarch 2, Farthing 4, Windemere 5, **Stonegate 6**, Lycaeum 7,
  Empath Abbey 10, Serpent's Hold 14; names from `native/targets/tdeck/main/location_names.h:19` and `game/assets/maps/smallmaps.json` id 29, floors [0]; the binary has no name table.)
* KEEP.DAT bytes 0x1800-0x1bff (record 6): tile 0x8c at exactly (14,14),(15,14),(16,14),(14,15),(16,15),(14,16),(15,16),(16,16); the centre (15,15) is 0x44.
  All shipped 0x8c: Yew (TOWNE record 7) (20,1),(25,8); Palace of Blackthorn (CASTLE records 6-9, 45/36/30/36 cells); Stonegate (KEEP 6); Serpent's Hold (KEEP 14) (13,4).
  Only location 29 fires the kill branch; the others have a floor below.
* It is a STANDING test, not a step test: any turn-consuming town command (post_turn is reached after K 0x4f7c(1) when K 0x39fc != -1) with the byte under (x,y) == 0x8c,
  `[0x587c]&0xfe != 0x14` (carpet exempt; a horse 0x12/0x13 is NOT exempt).
* Dead / sleeping members: the trigger inspects no member. 'S' members may wake first (wake loop) then take prologue damage then become 'D'. Dead members are skipped by the
  prologue damage but rewritten by the kill. A party with no 'G'/'P'/'S' never reaches post_turn (0x15bf); "all asleep" returns +1 and still lets it fire.
* **Census (by band, positive control run; stock tool plus my all-bases run `d85_fin_census.py`, BLCKTHRN at 0xA290):**
  * post_turn K 0x90d2: **TOWN 0x15ec only**; no thunk targets it (`thunks.py` list, plus raw scan for the word 0x90d2: ULTIMA.EXE 0x2f28/0x3688 are `eb d2 90` coincidences).
  * K 0x2aa8: ULTIMA.EXE 0x2b74, 0x304f; **TOWN 0x0f8d**, 0x10c4, 0x10e4; MAINOUT 0x0336, 0x0a80, 0x1155, 0x1160; CMDS 0x0d91 (my all-bases run adds spurious candidates in
    overlays whose base I did not fix; none in BLCKTHRN). Relevant: TOWN 0x0f8d.
  * K 0x5910: ULTIMA.EXE 7 sites, TOWN 0x053a, 0x0c2c, 0x0dd0, **0x0f8a**, 0x10ba, 0x1272, 0x1376, MAINOUT 9, SHOPPES 1, CMDS 10, SJOG 6, BLCKTHRN 8 (0x13c,0x64f,0x93f,0x9cf,0xa0a,0xa89,0xabb,0xb00). Relevant: TOWN 0x0f8a (and its twin 0x10ba).
  * K 0x39fc: TOWN 0x126b, 0x12c0, **0x1436**, **0x15bf**; MAINOUT 0x0aa2, 0x1158, 0x1b4b; CMDS 0x1bfd. K 0x2ae8: TOWN **0x10d0**, MAINOUT 0x0cd3, CMDS 0x0671.
  * K 0x7a5e (Refuge thunk): ULTIMA.EXE 0x0e7f (wrapper K 0x0e7c), TOWN 0x1865. String DS 0x2768: TOWN 0x0f77 only. `cmp byte [0x5893],0x1d`: TOWN 0x0f96 (this), 0x1253/0x1275 (Shadowlord text), CAST 0x0e4c/0x1300.
  * `3d 8c 00` compares: TOWN 0x0f63 (trigger), TALK 0x0dd0 and 0x10c6 (dialogue code, not read here; not on this path).

---------------------------------------------------------------------------------------------------------------

## 3. Reference (TypeScript) -- exact change

All line numbers are in the files as they stand now.

**3a. D-85 proper** -- `game/src/core/game.ts`, `stonegateLavaWipe` (def at 2708), the loop at **2719-2723**:
```ts
// 0x0ff6-0x1037: HP 0 y estado 'D' a TODO el roster (no solo al party).
for (const ch of this.state.characters) { ch.currentHp = 0; ch.status = "D"; }
```
becomes a loop over the party only (`this.state.characters.slice(0, this.state.partySize)`, which clamps to the array length by construction; the same idiom is
already used at `game.ts:2138`), and the comment at 2719 must be reworded (the cited range loops over `[0x585b]`). Also reword the doc block 2657-2675
(bullet "TODO el roster ... stride 0x20") and `trapdoor-fall.test.ts:175/178`. Keep the 0x8F wipe (2714), the `worldObjects` filter (2717) and `party-changed` event as they are.
No 6 cap (the original has none). Keep the direct write (not `applyDamage`): the original is a scripted write (God Mode does not apply, ALPHA4_UI.md:2605).

**3b. Prologue (needs an owner decision: widens D-85; I recommend recording it as its own D row, but the fix is a handful of lines)** --
`game/src/core/world/loops/turn.ts`, `townTurn`, the trapdoor branch **550-557** (the pinned layer; the hook is in `game.ts:2606`/`2676` and must not do it):
```ts
if (esTrampilla && !enAlfombra(state) && ctx.onTrapdoor) {
  messages.push(TRAPDOOR_MESSAGE);                                  // 0x0f77
  if (state.timeSpell !== "T") { setSite(<wind site>); maybeChangeWind(state, rand); }   // 0x0f8a (K 0x5910)
  setSite(<damage site>); partyRandomDamage(state, rand);           // 0x0f8d (K 0x2aa8)
  const desenlace = ctx.onTrapdoor();
  ...
```
Order is message, wind, damage, then the hook. It applies to every lap and to the `"none"` outcome too. Reuse of `survival.ts:408 partyRandomDamage`
(faithful to K 0x2aa8: bound `partySize && i<6`, skip 'D', one `rand(1,8)` per living member). Comments to fix: `turn.ts:459` (add 0x8C to the extra-wind line), `turn.ts:549`
("Sin RNG en toda la rama"), `game.ts:2674` ("NO hay una sola tirada de RNG"). The `MAX_TRAPDOOR_FALLS = 8` guard (turn.ts:63) is a port guard the binary does not have; harmless, leave.
Site labels: either reuse `damageTick`/`burn` (no schema change) or append two new names (e.g. `trapdoorWind`, `trapdoorFall`) to `sites` in `generate-turn-fixtures.ts:15`
AND `names[]` in `turn_parity_test.cpp:43` (ids are indices; appending renumbers nothing). This is a choice, not a binary fact; new labels let a mutation separate the two branches.

**3c. Adjacent (optional, only reachable now through this TPK):** `survival.ts:425-431` documents the K 0x2b14-0x2b25 housekeeping clear of a dead active character as
"unreachable today"; the Stonegate TPK writes 'D' directly, so it IS reachable after D-85's kill: the original ends with `[0x587b] = 0xff`.

---------------------------------------------------------------------------------------------------------------

## 4. Native (C++) -- exact change

**4a. D-85 proper** -- `native/core/src/quest_world.cpp:234` (inside `quest_trapdoor`, 227-240):
`for(int i=0;i<g.party.character_count;++i)` -> `for(int i=0;i<g.party.party_size && i<g.party.character_count;++i)`
(`party_size` is `int32_t`, `character_count` `uint8_t`, array capacity `kRosterCapacity = 16`, `state.h:7,18-22`; the clamp guards a short synthetic roster and
a corrupt party_size > 16: the original has no clamp). Leave 229-233 and 235 as they are.

**4b. Device audio** -- `native/targets/tdeck/main/alpha_runtime.cpp:3804`: `int32_t(game_.party.character_count)` -> `int32_t(game_.party.party_size)`
(the comment at 3801-3802 already says "([0x585b] members)"). `sfx_synth.cpp:446-451` (`clamp_index(param,6)`, one `speaker_noise(0x28,0xbb8,0x1f4)` per member) is already correct and param-driven.

**4c. Prologue** -- `native/core/src/turn.cpp`, `town_turn`, the trapdoor branch **211-214**:
```cpp
message(r,TurnMessage::Trapdoor);
if (s.time_spell != 'T') { t.site = <wind>; maybe_change_wind(s,rand); }   // 0x0f8a
t.site = <damage>; party_random_damage(g,rand);                             // 0x0f8d
if (ctx.on_trapdoor(ctx.hazard_context) == TrapdoorOutcome::Fell && lap < 8) continue;
break;
```
`party_random_damage` (turn.cpp:71-78) already mirrors K 0x2aa8 (bound `party_size && i<6`, skip 'D', short-roster draws kept to match TS) and routes HP loss through `apply_damage`
(God Mode blocks the write after the draw, exactly as for Burning at 216-220). `quest_trapdoor` itself needs no change for the prologue.

**Native device notes (not binary facts):** `quest_trapdoor` returns `None` without killing when `pool(s)` or `s->volatile_tile` is missing (230); the production device wires both
(`alpha_runtime.cpp:3608` per B), the bare host harnesses do not. `commands.cpp:277` binds `on_trapdoor` only when `c.quest_world`; `commands.cpp:861-866` refuses (atomic `Unsupported`) a tile-140
turn without a quest world. `check_refuge` (quest_world.cpp:197) and `resolve_refuge` (189) are already party_size-bounded.

---------------------------------------------------------------------------------------------------------------

## 5. Fixtures and corpora

I counted directly in `native/core/fixtures/turns.json` (15,608 rows, `rows[].kind==0`, `input[2]==mode`, `input[1]==step`, `input[4]==tile`).

| artefact | what it pins | D-85 alone (3a/4a/4b) | with the prologue (3b/4c) |
|---|---|---|---|
| `native/core/fixtures/turns.json` + `turns.inc` (generator `native/core/tools/generate-turn-fixtures.ts`, hook at line 73, `typescript_turn_fixture_drift` CMake check with `--check`; replay `native/core/tests/turn_parity_test.cpp:66-72`, CTest `turn_parity`) | town turns with a trapdoor tile: hook outcomes `fell`/`none`/`tpk` by `seed%4`, `tpk` mutates nothing; coverage keys `trapdoorGuard` 18, `trapdoorNone` 18, `trapdoorTpkHook` 18 | **byte-identical** (the probe hook kills nobody) | **changes**: the live rows are the step-9 rows (tile list `[5,4,143,188,140][step%5]`, odd steps only get the hook) with a non-carpet transport tile: **72 rows** (6 town scenarios x 24 seeds = 144 step-9 rows, but the 72 whose `transportTile` is 0x15 are carpet-exempt and unchanged); the rows 10..15 of the same sequences consume the changed state through the "continuous turn input" equality, so up to **72 x 7 = 504 rows** are regenerated (state HP, seed, wind; trace sites between `message` and `hook`). Coverage keys unchanged. Regenerate from the FIXED TS (`--check` control, token-diff proof: only those rows differ). Do not hand-edit the C++ side first. The other 15,104 rows must stay byte-identical |
| `game/tests/trapdoor-fall.test.ts` | 161-199 TPK: map to 0x8F, floor unchanged, **175-182 "TODO el roster"** with `characters:[2]`, `partySize:2` (line 82), 184-191 no rescan, 193-198 control. Both green before and after D-85: they do not discriminate roster from party | reword 175; add the roster>party test | fall tests stay green (HP 50 absorbs 1..8) |
| `native/core/tests/command_parity_test.cpp:300-323` (modes 1-2) | tile 140 without `quest_world` -> atomic `Unsupported` | unchanged | unchanged (branch not entered) |
| `native/core/tools/generate-movement-flow-fixtures.ts:12,20` | skips tile 140 (`townTrapdoorFall:()=>false`) | unchanged | unchanged |
| `game/src/core/__parity__/loops-run.ts:101`, `master-run.ts:169` | pass `tileUnderParty`, never `onTrapdoor` | unchanged | unchanged |
| `native/core/tests/a3_03_sfx_inventory_test.cpp:449-462`, `sfx_inventory.cpp:222-224`, `sfx_synth.cpp:446-451` | ramp 1000..251 (750), one burst per `param` | unchanged | unchanged |
| `native/targets/tdeck/host_tests/a3_03_sfx_runtime_test.cpp:143,805-813` | location 29: `TrapdoorFall` with `param==2` (`character_count = party_size = 2`) | still passes; ADD a roster>party case asserting param == party_size | unchanged |
| `native/core/tools/a3_03_mutation_check.py` M16 | the location-29 condition of the cue | unchanged | unchanged |
| native test of `quest_trapdoor`/`PartyKilled` | **none exists** (only the probe in `turn_parity_test.cpp`) | new test required | new test |
| docs | `ALPHA4_UI.md:2956, 3336, 3348`, `ALPHA2_PRESERVATION_LEDGER.md:230`; "no RNG in the branch": `game.ts:2674`, `turn.ts:549`, `native/core/WORLD_TURNS.md` trapdoor lines, `native/core/VALIDATION.md:923-926`; `re/notes/loops.md:114-115` (relabel 0x8C as the trapdoor, and add the Stonegate branch) | update D-85 row | add a D row for the prologue, reword the "RNG-free" lines |

---------------------------------------------------------------------------------------------------------------

## 6. Test plan

RED-first means: write each test against the CURRENT code and watch it fail for the stated reason before touching the fix.

**D-85 (TS and native, same inputs):**
1. party 2, roster 5, all 'G' with HP > 0 and `partyStatus != 0`/inn byte set on 2..4: after the Stonegate ring-tile turn chars 0,1 = HP 0/'D'; chars 2-4 byte-identical in every field.
   RED today: chars 2-4 die. Mutation M1 "bound back to the roster" must fail it; M2 "off by one (party_size+1)" must fail it (char 2 touched).
2. party 6, roster 16: 0-5 dead, 6-15 untouched; party == roster == 6: all dead (no under-kill). Mutation M3 "cap at 3/at party_size-1" fails the second half.
3. party 1, roster 3: member 0 dead, companions alive; the Refuge fires the same command and revives member 0 only; companions' state unchanged (`resolve_refuge`/`partyRefuge` are party-bounded).
4. Pre-existing statuses: 'D' with HP 7, 'S', 'P' inside the party all end HP 0/'D' (the 'D'-with-HP case kills mutation M4 "skip already-dead"); a non-party 'S' or 'P' is untouched and draws no wake roll.
5. Defensive: party_size 0 -> no writes, no crash; party_size > character_count (synthetic short roster, e.g. party 6 / count 2) and party_size 17 -> clamped, no out-of-bounds write (native: the array is 16).
6. Carpet 0x14 and 0x15 (no message, no change); horse 0x12/0x13 and foot 0x1c fire; transport tile unchanged afterwards. Non-29 (Yew floor 0 at (20,1); Blackthorn; Serpent's Hold (13,4)) with the same roster: nobody dies, floor decrements.
7. After the kill: whole map 0x8F, objects at (location,floor) erased, `refuge` raised in the SAME command, no rescan (the exactly-one-message check at `trapdoor-fall.test.ts:184`).
8. Native device (host test): `TrapdoorFall` param == party_size with roster 5 / party 2 (RED today: 5). Mutation: restore `character_count` in alpha_runtime.cpp:3804 must fail it.
9. Stock-data check: a new game from the shipped INIT.GAM (party 3, roster 16) must leave records 3-15 alive after the TPK (RED today: all 16 'D').

**Prologue (if taken):** RED-first against a stub that records the RNG trace:
10. Draw order in Stonegate with 3 living members and 1 sleeper before it: wake `rand(0,15)`, wind `rand(0,63)` (+ the 1/64 tail), `rand(1,8)` x3 in index order, nothing after the kill; housekeeping draws unchanged.
11. A 'D' member in the party draws no damage roll; an 'S' and a 'P' member do. Time spell 'T': no wind draw, damage draws remain; 'Q' behaves as no spell.
12. Non-29 fall: floor-1, each living member loses 1..8, one message + wind + damage per lap; a chained fall repeats all three; `"none"` hook outcome still consumed the prologue.
13. A party killed by the fall damage still takes the kill pass (kill draws nothing); God Mode: draws still happen, only the HP write is skipped, the kill is a direct write and still kills.
14. Regenerate `turns.json/inc` from the fixed TS; assert the diff is limited to the 72 live step-9 rows and their 432 successors (mutation M5: swap wind and damage order, M6: damage before the message, M7: prologue only at 29, M8: prologue skipped on the `none` outcome must each break at least one corpus row).

---------------------------------------------------------------------------------------------------------------

## 7. Residual unknowns (exact)

1. **Wall-clock seconds** of the ramp and bursts: depend on `[0x5356]` (calibration) and the cycle ratio of the `dec mem/jne` loop to the calibration loop; only the RELATIVE work (2.8) is derived. Settle with a DOSBox-X trace of the PIT writes (port 0x42) timestamped against INT 1Ch.
2. **K 0x4552 animation interpreter inside K 0x5910**: has RNG at K 0x4625/0x466d/0x469f for live slots with tile >= 0x34 passing the gates at 0x45d9-0x461e. Which Stonegate slots are live at 0x0f8a (the table is zeroed only at 0x0fea) and so how many extra draws occur is not derived. Same unmodelled residue as the Burning path; settle with a memory dump of DS 0x5c5a at the trapdoor tick or by decoding Stonegate's KEEP.NPC/object placement against the interpreter gates.
3. **Scope of the 0x5c5a wipe**: the binary zeroes all 32 slots (including live townspeople); ports erase persisted objects at (location,floor) only. Unobservable today because the Refuge relocates in the same command; would matter only if the Refuge were deferred. Settle with a memory dump after the TPK.
4. **Display-driver fill** at 0x0aa6 was read only in EGA.DRV by B (B2: service 0x3f plain scanline fill); CGA/HER/T1K variants not read. Presentation only. I did not re-verify this one.
5. **K 0x0e26 / K 0x0e1d** (driver functions 3 / 0xf around the Refuge) not decoded; irrelevant to D-85.
6. **The meaning of `[0x587b]`** is inferred (reset-to-0xff rules in K 0x2a52/0x2ae8; ports call it `activeCharacter`); the housekeeping clear after the TPK (2.7) is binary-proven, its user-visible effect is not examined.
7. **`TALK.OVL 0x0dd0 / 0x10c6 cmp ax,0x8c`** were not read; they are dialogue code, not on this path.
8. **Dynamic confirmation:** everything above is static disassembly of the shipped binaries; no emulator run was made. The roster/party semantics were additionally cross-checked on INIT.GAM / SAVED.GAM bytes.
9. **Corpora outside the names I grepped:** a test that walks the real Stonegate map onto a ring tile without using the words trapdoor/PartyKilled would not have shown up; the greps over `native/core/tests`, `native/targets/tdeck/host_tests`, `game/tests`, `game/e2e` found none.

## Reconciliation log (A vs B)

* Agreed and re-verified by me: loop bound `[0x585b]`, indices, HP/status result, untouched non-party members, text, trigger tile (ring around (15,15) of KEEP record 6) and location, no-RNG kill tail, prologue at 0x0f8a/0x0f8d,
  Refuge afterwards, single post_turn caller, BLCKTHRN base 0xA290 (brief wrong), both port sites and the audio count.
* Disagreed, settled in the binary: (a) `delay` shift index: A right (unscaled byte index, `mov bx,[bp+6]; mov cx,[bx+0x5426]` at K 0x20d4), B's `2*arg` wrong (same value for sel=1: zero); (b) fixture impact: A "144 rows", B "every odd step row" -> the data says
  72 live rows (carpet transport 0x15 exempts the other 72) and up to 504 with successors; (c) noise-burst argument notation: A's order (push order step,total,hi) is the binary's; B listed it reversed; (d) trace-site naming: a design choice (3b).
* Left unresolved by both and by me: items 1-4 above.
