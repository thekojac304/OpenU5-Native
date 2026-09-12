# Music ↔ location mapping of the Exodus Project MIDI patch (MID.DRV) — derived from the binary

Date: 2026-09-12. Scope: the patched DOS Ultima V in `original/u5/ultima5/` (ULTIMA.EXE 2001-08-20,
`mid.drv` 823 B, INTRO/ENDGAME/FONT/TOWN/MAINOUT/DUNGEON overlays dated 2001).

Status: the analysis below is read-only reverse engineering of the patched binary. OpenU5's runtime
mapping was brought in line with it on 2026-09-12 — see section 5.

Tooling: capstone (16-bit) via a 10-line script; no oracle/DOSBox run was needed because the whole
decision procedure lives as straight-line code inside `mid.drv` itself. `re/tools/oracle.py` is absent
from this checkout (only referenced from notes; it was macOS-only anyway).

## 0. TL;DR

* The song for the generic selector 0x00 is NOT chosen in ULTIMA.EXE. **It is computed inside
  `mid.drv` at file offset 0x016d** from four game globals read through DS=BX (BX = game data segment,
  passed by the kernel wrapper): `g_location` [0x5893], `g_floor` [0x5895], `g_transport_tile`
  [0x587c] and `g_cmb_victory_flag` [0x58a3]. It is a range/switch, not a table.
* The music driver has its own far pointer, `cs:[0x0ded]` in ULTIMA.EXE (NOT [0x5350], which is the
  video driver). The patch reused the body of `detect_video_adapter` (0x0de0..0x0e93) for its glue code.
* Song IDs 0x00..0x0f are exactly the order of `Files.txt`; the filename table is in `mid.drv` at 0x20.

## 1. Call chain

```
ULTIMA.EXE 0x00ad  main init: call 0x0e36
  0x0e36: call 0x0df1 (load MID.DRV: push 0x5340 = DS ptr to "MID.DRV" in DATA.OVL, the old Tandy
                       slot; call 0x0fae = driver loader; segment -> cs:[0x0def])
          call 0x7a2e (stub#2 -> INTRO.OVL 0x0986 intro_main_controller)   ; main menu / intro
          call 0x0e1d (selector 0x0f: enable location-driven music)

ULTIMA.EXE 0x1b38 poll_key_blink_cursor (the universal key poll):
  0x1b5b: call 0x0e0e            ; was `call 0x1d5e kernel_getkey` in the unpatched game
  0x0e0e: push bx; push dx; mov bx,ds; xor dx,dx; call 0x0e03; pop dx; pop bx; call 0x1d5e; ret
  0x0e03: mov cs:[0x0ded],dx ; lcall cs:[0x0ded]          ; DX = selector, BX = game DS
```
So on EVERY key poll the driver's selector 0 runs with BX = the game's data segment. The only direct
caller of `kernel_getkey` 0x1d5e left in the EXE is 0x0e19 (inside the wrapper). FONT.OVL (3 sites)
and INTRO.OVL (1 site) also call 0x0e0e.

### 1.1 MID.DRV dispatch table (file offset 0, `E9 rel16` x 10, selector = 3 x index)
| sel | body | effect |
|---|---|---|
| 0x00 | 0x2a9 | DS=drv; `call 0x16d` (compute song from location, BX=game DS); `call 0x267` (play-if-changed) |
| 0x03 | 0x2e2 | stop (`int 66h` AX=0x705, cur=0xff) + FREEZE ([0x11f]=0) |
| 0x06 | 0x2b7 | `call 0x223` = intro range table on BL; play |
| 0x09 | 0x2c5 | FREEZE; AL=0 U5THEME; play |
| 0x0c | 0x2d5 | AL=0x0f AMIGA; play (mode flag untouched) |
| 0x0f | 0x2ee | ENABLE location mode ([0x11f]=1) |
| 0x12 | 0x2f7 | FREEZE; AL=4 STONES; play |
| 0x15 | 0x307 | FREEZE; AL=0x0d REUNION; play; then cur=[0x11e]:=0x0e so that the next sel 0x1b only (re)starts RULEBRIT once Reunion has finished (`0x267`: same song => `0x27c` checks status via AX=0x70c and restarts only if BL!=1) |
| 0x18 | 0x31c | `call 0x24a` = endgame range table on BL; play |
| 0x1b | 0x32a | AL=0x0e RULEBRIT; play |

Driver state (DS = CS-0x10, so [0x11e] is file offset 0x1e): `[0x11e]` current song (0xff = none),
`[0x11f]` mode flag: 1 = compute from location on each sel-0 call, 0 = frozen (keep current).

### 1.2 The location switch — `mid.drv` 0x016d (verbatim)
```
016d  pushf ; push bx ; push ds
0170  cmp byte [0x11f],0 ; jne 017d
0177  mov al,[0x11e] ; jmp 021f              ; frozen: keep current song
017d  mov ds,bx                              ; DS := game data segment
017f  xor ah,ah
0181  mov bl,[0x5893]                        ; g_location
0185  cmp bl,0xff ; jne 019d                 ; 0xff = combat sentinel (set by ULTIMA.EXE 0x5fb4)
018a  mov bh,[0x58a3]                        ;   g_cmb_victory_flag
018e  mov al,3                               ;   ENGGMNT  (Engagement and Melee)
0190  cmp bh,0 ; jne 0198 ; jmp 021f
0198  mov al,0 ; jmp 021f                    ;   victory announced -> U5THEME
019d  mov bh,[0x587c] ; and bh,0xf8          ; g_transport_tile family
01a4  cmp bh,0x20 ; jne 01ae
01a9  mov al,2 ; jmp 021f                    ; 0x20..0x27 = FRIGATE (sails up/down) -> HORNPIPE
01ae  cmp bl,0 ; jne 01c3
01b3  mov al,1                               ; loc 0: BRITLAND
01b5  mov bh,[0x5895] ; cmp bh,0 ; je 021f   ;   g_floor != 0 (0xff = Underworld) ->
01be  mov al,0x0a ; jmp 021f                 ;   WRLDBLW
01c3  cmp bl,8    ; ja 01cd ; mov al,8    ; jmp 021f   ; 0x01..0x08 -> TRNTLLA
01cd  cmp bl,0x0c ; ja 01d7 ; mov al,0x0c ; jmp 021f   ; 0x09..0x0c -> LADYNAN
01d7  cmp bl,0x10 ; ja 01e1 ; mov al,5    ; jmp 021f   ; 0x0d..0x10 -> GREYSON
01e1  cmp bl,0x11 ; ja 01eb ; mov al,7    ; jmp 021f   ; 0x11       -> MONARCH
01eb  cmp bl,0x12 ; ja 01f5 ; mov al,0x0b ; jmp 021f   ; 0x12       -> BLCKTHRN
01f5  cmp bl,0x18 ; ja 01ff ; mov al,5    ; jmp 021f   ; 0x13..0x18 -> GREYSON
01ff  cmp bl,0x1d ; ja 0209 ; mov al,0x0c ; jmp 021f   ; 0x19..0x1d -> LADYNAN
0209  cmp bl,0x20 ; ja 0213 ; mov al,6    ; jmp 021f   ; 0x1e..0x20 -> FANFARE
0213  cmp bl,0x28 ; ja 021f ; mov al,9    ; jmp 021f   ; 0x21..0x28 -> HALLS
021d  mov al,0xff                                      ; anything else (0x40 demo, 0x42 endgame, >=0x7f) -> stop
021f  pop ds ; pop bx ; popf ; ret
```
Order of precedence: frozen -> combat sentinel -> frigate -> location.

### 1.3 Intro / endgame range tables (BL supplied by the overlay)
* `0x223` (sel 0x06, INTRO.OVL 0x0adc, BL = [bp-8] story-page counter): BL 0..7 -> STONES(4);
  8..0x0e -> HALLS(9); 0x0f..0x15 -> GREYSON(5); else stop.
* `0x24a` (sel 0x18, ENDGAME.OVL 0x0aee, BL = [bp-6] scene index): 0..3 -> STONES(4); 4..7 -> LADYNAN(0x0c); else stop.

### 1.4 Every kernel/overlay entry into the glue
All verified by near-call arithmetic; overlay near-call bases: INTRO 0x81c0, TOWN/MAINOUT/DUNGEON
0x81d0, BLCKTHRN/COMBAT/ENDGAME 0xa290, CAST2 0xe1e0 (each base also maps the overlay's other calls
onto `kernel_getkey` 0x1d5e / `putchar` 0x16ba, which is the cross-check).

| site | glue | meaning |
|---|---|---|
| EXE 0x00ad main init | 0x0e36 | load driver; run intro; ENABLE |
| EXE 0x1b5b poll_key_blink_cursor | 0x0e0e | refresh (sel 0) + getkey |
| INTRO.OVL 0x0ad0 | sel 0x09 | main menu: U5THEME (frozen) |
| INTRO.OVL 0x0adc | sel 0x06 | story pages: table 0x223 |
| INTRO.OVL 0x0aed / 0x0af9 | sel 0x0c | character creation: AMIGA |
| EXE 0x3e7c (kernel_camp_holeup, arena type 6) | 0x0e66 | STONES frozen; arena runner 0x5f86; ENABLE |
| EXE 0x3ee2 -> 0x6360 -> 0x6373 (camp variant, arena type 4) | 0x0e5a | STONES frozen; 0x5f86; ENABLE |
| EXE 0x6069 inside arena runner 0x5f86, right before COMBAT.OVL 0x0b94 combat_main_loop | 0x0e49 | ENABLE (so the loc 0xff branch is live during combat) |
| MAINOUT.OVL 0x0968 (8-entry shrine loop in mainout_cmd_enter) | 0x0e50 | STONES frozen; CAST2.OVL 0x0e76 (inside shrine_visit); ENABLE |
| TOWN.OVL 0x12ca (guarded by `cmp g_location,0x12`) | 0x0e72 | STOP; BLCKTHRN.OVL 0x060e blackthorn_capture; ENABLE |
| TOWN.OVL 0x1862 / MAINOUT.OVL 0x0af0 / DUNGEON.OVL 0x1014 | 0x0e26+stub 0x7a5e / 0x0e7c | STOP; BLCKTHRN.OVL 0x0910 party_refuge (death); ENABLE |
| ENDGAME.OVL 0x0aee / 0x0aff / 0x0b18 | sel 0x18 / 0x15 / 0x1b | endgame table; REUNION -> RULEBRIT chain; RULEBRIT |
| EXE 0x0e2f | stop + `int 21h/4c` | Ctrl-E exit paths |

Ordinary combat (DUNGEON.OVL 0x0c53 dng_ambush, 0x1db5 dng_attack, EXE 0x6347) calls 0x5f86
directly, i.e. in location mode -> g_location=0xff -> ENGGMNT, then U5THEME once `g_cmb_victory_flag`
is set (COMBAT.OVL 0x0bb9/0x0cfd; cleared at EXE 0x6064 just before the ENABLE at 0x6069). Camp: the
arena runner branches on type (0x5fda/0x602e); with type 4/6 it runs 0x8076 (camp scene) under frozen
STONES and only reaches the ENABLE + combat loop if an ambush fires (return != 0).

## 2. Song-ID table (mid.drv 0x20 pointer table -> names at 0x40; identical to Files.txt order)
| id | file | title (Files.txt) | confidence |
|---|---|---|---|
| 0x00 | U5THEME.XMI | Ultima V Theme | binary |
| 0x01 | BRITLAND.XMI | Britannic Lands | binary |
| 0x02 | HORNPIPE.XMI | Cap'n Johne's Hornpipe | binary |
| 0x03 | ENGGMNT.XMI | Engagement and Melee | binary |
| 0x04 | STONES.XMI | Stones | binary |
| 0x05 | GREYSON.XMI | Greyson's Tale | binary |
| 0x06 | FANFARE.XMI | Fanfare for the Virtuous | binary |
| 0x07 | MONARCH.XMI | The Missing Monarch | binary |
| 0x08 | TRNTLLA.XMI | Villager Tarantella | binary |
| 0x09 | HALLS.XMI | Halls of Doom | binary |
| 0x0a | WRLDBLW.XMI | Worlds Below | binary |
| 0x0b | BLCKTHRN.XMI | Lord Blackthorn | binary |
| 0x0c | LADYNAN.XMI | Dream of Lady Nan | binary |
| 0x0d | REUNION.XMI | Joyous Reunion | binary |
| 0x0e | RULEBRIT.XMI | Rule Britannia | binary |
| 0x0f | AMIGA.XMI | Amiga Theme | binary |

Play routine 0x0ff: `[0x11e]=al; bx=[0x120+al*2]; int 66h AX=0x70d (load XMI file named at CX:BX);
int 66h AX=0x702 BX=0 (play sequence 0)`; al=0xff is a no-op. Titles come from Files.txt (the driver
only stores file names).

## 3. Location -> song
All rows BINARY-DERIVED from section 1.2; names from `game/src/core/location-display.ts`; the
Cities-of-Virtue row is additionally README-derived.

| g_location | name(s) | class | id | track |
|---|---|---|---|---|
| 0x00, g_floor==0 | Britannia surface | overworld | 0x01 | Britannic Lands |
| 0x00, g_floor!=0 (0xff) | Underworld | overworld | 0x0a | Worlds Below |
| any non-0xff loc, transport tile 0x20..0x27 | aboard a frigate | override | 0x02 | Cap'n Johne's Hornpipe |
| 0x01..0x08 | Moonglow, Britain, Jhelom, Yew, Minoc, Trinsic, Skara Brae, New Magincia | Cities of Virtue | 0x08 | Villager Tarantella |
| 0x09..0x0c | Fogsbane, Stormcrow, Greyhaven, Waveguide | lighthouses | 0x0c | Dream of Lady Nan |
| 0x0d..0x10 | Iolo's Hut, Sutek's Hut, Sin'Vraal's Hut, Grendel's Hut | huts | 0x05 | Greyson's Tale |
| 0x11 | Lord British's Castle | castle | 0x07 | The Missing Monarch |
| 0x12 | Palace of Blackthorn | castle | 0x0b | Lord Blackthorn |
| 0x13..0x18 | West/North/East Britanny, Paws, Cove, Buccaneer's Den | villages | 0x05 | Greyson's Tale |
| 0x19..0x1d | Ararat, Bordermarch, Farthing, Windemere, Stonegate | keeps | 0x0c | Dream of Lady Nan |
| 0x1e..0x20 | The Lycaeum, Empath Abbey, Serpent's Hold | keeps of the principles | 0x06 | Fanfare for the Virtuous |
| 0x21..0x28 | Deceit ... Doom | dungeons | 0x09 | Halls of Doom |
| 0xff, victory flag 0 | combat | scripted | 0x03 | Engagement and Melee |
| 0xff, victory flag 1 | combat, after VICTORY! | scripted | 0x00 | Ultima V Theme |
| 0x40 demo, 0x42 endgame, >=0x7f, others | - | default | 0xff | stop (only when not frozen) |

Scripted/frozen overrides (section 1.4): main menu U5THEME; intro pages table 0x223; character
creation AMIGA; shrines STONES; camp/hole-up STONES (ambush -> combat rules); Blackthorn capture and
death scenes silent; endgame table 0x24a then REUNION -> RULEBRIT.

Unresolved / not claimed: nothing in the location domain 0x00..0x28 is unresolved. Nothing here is
empirically observed (no DOSBox run); every value is read from the driver's own code. Skiff
(0x28..0x2b), horse and carpet have no music override (only the frigate family 0x20..0x27 is tested).

## 4. Reproduce
```
pip install capstone
python - <<'PY'
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
d=open('original/u5/ultima5/mid.drv','rb').read()
for i in Cs(CS_ARCH_X86,CS_MODE_16).disasm(d[0x16d:0x223],0x16d): print(hex(i.address),i.mnemonic,i.op_str)
PY
```
ULTIMA.EXE code offset = file offset - 0x800 (MZ header 128 paragraphs). Overlay near-call target =
(overlay offset + 3 + rel16 + base) & 0xffff with the bases listed in section 1.4.

## 5. How OpenU5 implements it (applied 2026-09-12)

The rule is represented exactly as the driver does it.

* `game/src/ui/music.ts` — `songForLocation({location, floor, transportTile, inCombat, combatVictory})`
  is the switch of section 1.2, branch for branch and in the same order; `contextForLocation` is the
  same switch returning a named context. `introPageContext` / `endgameSceneContext` are the two range
  tables (0x223 / 0x24a). `SONG_TRACK` is the driver's 16-entry pointer table, indexed by song id.
* Frozen mode mirrors the driver's `[0x11f]`: a scripted context takes ownership of the music and
  `playLocation` becomes a no-op until `resumeLocation()` (the driver's selector 0x0f).
* De-duplication is by SONG, not by context — the driver's `cmp al,[0x11e]`. It matters because
  distinct contexts share a song (hut/village both Greyson's Tale; lighthouse/keep both Lady Nan), so
  a context-keyed check would restart the same track when walking from one to the other.
* `game/src/main.ts` wires the scenes to the selectors of section 1.4: menu/intro pages/creation
  (polled from the intro's phase and story-page counters, the way the original polls on each keypress),
  shrine, camp, Blackthorn capture and death (silence), combat and the victory latch, and the endgame.
* Tracks are emitted by song name, in song-id order (`extractor/src/audio/tracklist.ts`,
  `extractor/src/assets-catalog.ts`). The old context-named files were renamed because three of those
  names were wrong: `castle.mid` was Rule Britannia, `tavern.mid` was the Hornpipe, `dungeon.mid`/
  `combat.mid`/`overworld.mid`/`underworld.mid` were merely redundant. Re-run `npm run extract`.

What changed audibly: Stones is no longer the theme of 28 locations (it is shrine/camp/intro/endgame
only, and it appears in NO branch of the location switch); Rule Britannia is the endgame close, not
the castle; The Missing Monarch is the castle; the Hornpipe is the frigate, not taverns; lighthouses
and keeps get Lady Nan; huts and villages get Greyson's Tale; Lycaeum, Abbey and Serpent's Hold get
Fanfare; combat victory switches to the Ultima V Theme; and the Amiga Theme finally plays, in
character creation, as the patch README says it should.

Still not derived, and labelled as such in the code: which beat of the port's endgame corresponds to
each scene of the original (the clone's script has no counter equivalent to the `[bp-6]` that indexes
table 0x24a), so the port reproduces the ORDER — scene table, then Joyous Reunion, then Rule Britannia
— rather than the indices.
