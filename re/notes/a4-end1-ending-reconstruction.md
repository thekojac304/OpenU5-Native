# A4-END1 — the ending, reconstructed for the native port

What the original does from the final absorption to its last screen, at the level the native
sequencer (`native/core/src/endgame_scene.cpp`) clones it, and what the T-Deck adapts. Builds on
`endgame.md` (Task 3.12: functions, playtime, the scroll), `endgame-derivation.md` (the GAP list:
the recolor §A, the gate and orb §GAP 4, the page layout §C, the fizzle §F) and
`batch53a-endgame-terminal.md` (entry, the Y/N prompts, neither branch returns). No ENDMSG.DAT /
END.DAT text is reproduced here: the records stay in the user's files and the pack.

**Sources.** The shipped install `original/u5/ultima5` = the 1988 game with the Exodus Project
*Ultima V Upgrade* 1.0 applied in place (`ALPHA3_AUDIO.md` §5): `ENDGAME.OVL` (patched: the 1988
2,800 B overlay plus the patch's tail at 0x0aee), `ULTIMA.EXE`, `DATA.OVL`, `EGA.DRV`, `FONT.OVL`,
`MISCMAPS.DAT`, `ENDMSG.DAT`, `END.DAT`, `ENDTEXT.16`, `ENDSC.16`, `END1.16`–`END3.16`, `IBM.CH`,
`RUNES.CH`, `mid.drv`. `re/tools/a4_end1_endgame_listing.py` prints the whole overlay with every near
call resolved (local names; kernel targets through the overlay-13 slot, `(target + 0xA290) & 0xFFFF`)
and every DS operand named — run it locally; its output quotes EA code and text and is never committed.
**No DOSBox or other runtime oracle was available**: everything below is static evidence (the bytes),
cross-checked against the two video witnesses of `endgame-witness-20260721.md`.

## 1. Trigger

SJOG `absorb` (0x1ea4) writes the sentinel `[0x58a0] = 0x4d` when the Avatar steps under the trapped
soul; the combat teardown tests it first (SJOG 0x2046, DUNGEON 0x00cb) and calls the overlay-13 stub
ULTIMA.EXE 0x7c4a → ENDGAME.OVL `endgame_main` 0x0648, which never returns. The native trigger is the
same moment: the core's absorption (`quest_world.cpp` `absorption_endgame`) sets game-won and, on the
device (`QuestWorldServices::endgame_presenter`), leaves the narration to the presenter; the GameWon
event mounts `openu5::EndgameScene` (`AlphaRuntime::start_endgame`). Parity drivers keep the
presenter off and the TypeScript reference's one-shot narration (quest_parity is unchanged).

## 2. endgame_main, step by step (→ native `Pc` state)

| original | what it does | native |
|---|---|---|
| 0x0650–0x06b7 | patch 0x0aff: mid.drv selector **0x15** (Joyous Reunion; the driver records Rule Britannia as current); status panel redrawn; MISCMAPS.DAT[0x210] (11 rows × 16) into the scene map (stride 0x20); EGA.DRV fn36 ax=4 recolours 22 tiles in the tileset (§4); ENDMSG.DAT into DS 0xb21e | `Entry` (music Reunion), `start()` copies the room |
| 0x06bb–0x06f6 | actor table (32 × 8 B at 0x5c5a) cleared; Lord British slot 31, tile 0x7c, at (5,8); `run_n_frames(0x28)` | `Entry` → frames(40) |
| 0x06f9–0x070a | `move_sprite_toward(31, 5, 3)` until there; each step `sprite_step_redraw` 0x04fe | `LbWalk` + the step sub-sequence |
| 0x070c–0x081c | each roster member in order: if dead — `put_char('\n')`, the name, `" lives!\n"` (DS), the viewport XOR (8,8)–(0xb7,0xb7) with 15, `tone_sweep(1,0x1388,0x9c40,1,0x2260)`, status `'G'` and HP = max (0x075a–0x0765, shown at 0x0792's panel redraw); the class tile (`byte[0x1ade + strchr("AMBFDTPRS", class)]`: M 0x40, B 0x44, F 0x48, else 0x4c) at the mirror (5,9); one frame; walk to the lineup DATA 0x3e5a/0x3e60 = cols {5,4,6,3,5,7}, rows {5,6,6,7,7,7} | `MemberNext`, `MemberRevived`, `MemberPlace`, `MemberWalk` |
| 0x082c–0x0848 | `run_n_frames(0x28)`; ENDMSG 0x00, the name at DS 0x55a8 (roster slot 0), `"!\"\n\n"`; getkey | `Greeting` → Key |
| 0x084b–0x08b0 | ENDMSG 0x01; Y/N loop 0x0852 (getkey upper-cases via kernel 0x2032; anything but Y/N is read again); echo `"Yes\n\n"` / `"No\n\n"`; only **N** asks ENDMSG 0x02 and loops again at 0x088b | `BoxQuestion`, `FirstAnswer`, `SecondAnswer` → YesNo |
| 0x08b9 | answer `'Y'` **and** `g_wooden_box` (DS 0x57bf) → victory (0x08cc), else 0x0a76 | `Branch` |
| 0x08cc–0x091e | 8 frames; the Avatar (slot 0) to (5,4) and back to (5,5); 4 frames; the box (slot 6, tile 0x0e) at (5,4); ENDMSG 0x03; 40 frames | `AvatarForward`, `AvatarBack`, `BoxPlaced` |
| 0x0925–0x095e | `"\n\nHe says:\n\n"`; getkey; ENDMSG 0x04–0x08, a getkey after each | `HeSays`, `Speech` |
| 0x0961–0x0987 | ENDMSG 0x09 ("FOLLOW!"); the orb (tile 0x08) in slot 6; getkey; `tone_sweep(1,0x2710,0xc350,1,0x1450)` | `Follow`, `Orb` |
| 0x098a–0x09b2 | slot 6 cleared; 0xdc into the scene map at (5,4); gate stage `[0x5887]` 1..15 one frame each, then 16 (whole) for 4 frames | `GateOpen`, `GateRise` |
| 0x09b5–0x0a24 | Lord British into the gate, gone; each member into the gate, gone, one frame each | `LbIntoGate`, `MembersIntoGate` |
| 0x0a27–0x0a37 | stages 15..1 | `GateClose` |
| 0x0a39–0x0a6a | `blit_tile(0x44, 5, 4)` over the gate cell (the map keeps 0xdc); the back buffer cleared; `story_screens` 0x0000 fizzles to it | `Teardown` → Dissolve |
| 0x0000–0x01e6 | six story pages (§6), a key wait each; the patch's poll passes BL = page + 1 to selector **0x18** | `Story`, `StoryNext` → Key ×6 |
| 0x01ed–0x0225, 0x0326 | ENDSC.16 at (40,0) on black; `endgame_datestamp` (§7); the 1988 loop 0x04f9 forever (the patch: selector **0x1b**, then any key → DOS) | `StoryNext` → Scroll, Forever |
| 0x0a76–0x0a8f | `"\"I see...\n"`; 40 frames; ENDMSG 0x0a; the Avatar one row up | `ISee`, `Chair` |
| 0x0a92–0x0ac4 | rounds of: slot 2 → (8,6), Lord British → (4,1) (the bed), slot 0 → (8,4), until nobody moves | `StrandedWalks` |
| 0x0ac9–0x0ae2 | forever: `wander_sprite` 1, 3, 4, 5, a frame each (the patch selects 0x1b each pass) | `Wander` (phase Stranded) |

`sprite_step_redraw` 0x04fe = `run_n_frames(2)`, the footstep (kernel 0x433e:
`noise_burst(0x3e8,0x19,1)`, `delay(0x14)`, `noise_burst(0x5dc,0x19,1)` = 555 speaker samples),
`run_n_frames(3)`. `move_sprite_toward` 0x0510 takes one step on the longer axis (rows when
|dcol| < |drow|) and returns 0 when inactive or there. `wander_sprite` 0x05a2: a coin, then up to 8
random directions until one is visible floor (an actor's own cell reads 0 in the visible buffer, so
nobody walks into anyone).

## 3. The text

Eleven ENDMSG.DAT records, printed in record order through `print_string` 0x1850 into the console
(one stream: every print continues the line the last one left), interleaved with the DATA.OVL strings
above. Victory: `[lives…]` 0x00 name `!"` 0x01 Yes 0x03 He says: 0x04 … 0x09. Stranded: … 0x01 No
0x02 No / Yes (no box) → `"I see...` 0x0a. Each record is printed once. The device adds nothing to
the stream: the GameWon / Endgame events' internal token is no line (D-57).

## 4. The throne room

MISCMAPS.DAT record 0 at 0x210, 11 rows of 16 bytes (11 used). The compositor (0x5394 / 0x56ac)
copies the room into the visible buffer every frame, then draws slots 31 down to 0 (a lower slot
wins a shared cell), each through the town pose selector 0x51b8, whose gate admits tiles 0x1c,
0x12–0x15, 0x28–0x2b and 0x40–0x7f: on the bed 0xab → 0x1a; on a mirror 0x9d/0x9e → 0x3c + r;
on chair 0x92 → (table 0x9a/0x9c south ? 0x34 + r : 0x32); on chair 0x90 → (table 0x9b/0x9c north ?
0x38 + r : 0x30); 0x91/0x93 → 0x30 + (tile & 3); 0x84 → 0x60 + r; 0x85 → 0x64 + r; r = rand(0,3)
(0x51a0). Standing south of a mirror lights the reflection 0x9e (0x532c). Drawn tiles are tile | 0x100.

**The green scene is a tileset recolor** (EGA.DRV fn36 ax=4, `endgame-derivation.md` §A): the
low-nibble LUT 0x2d4d `{0,5,4,4,2,1,2,7,8,0xc,0xc,0xc,0xa,9,0xe,0xf}` (and its high-nibble twin)
applied in place to 22 tiles in this order: 0x44 0x5c 0x5d 0x90 0x92 0x94 0x96 0x9b 0xab 0xac 0xaf
0xb0 0xb1 0xbf 0xdc 0x108 0x10e 0x11a 0x138 0x139 0x13a 0x13b. The torch flicker (fn32) animates the
mutated tileset.

**The partial gate** (0x56e6 → 0x1112 → EGA.DRV 0x24d6): while `[0x5887]` is 1..15 and no actor
stands on (5,4), tile slot 0x116 is composed as the floor 0x44 with its bottom `stage` rows replaced
by the top `stage` rows of 0xdc, both from the recolored set, blitted opaque.

## 5. The dissolve

EGA.DRV fn34 (SEL 0x66, entry 0x256b, carry clear; `endgame-derivation.md` §F): a Galois LFSR over the
rectangle's pixel indices, taps from cs:0x254d by bit width {3, 6, 0xc, 0x14, 0x30, 0x60, 0xb8, 0x110,
0x240, 0x500, 0xca0, 0x1b00, 0x3500, 0x6000, 0xb400}, seed 1, x = state % w, y = state / w
(indices past the rectangle skipped), (0,0) last. 320 × 200 uses 0xb400; the first pixel is (1,0).
In the endgame the sound/delay gate cs:[0x253d] is 0, so the loop runs at the machine's speed: no
timer. The only measure is the witness video, ~2.5 s for the 64,000 pixels — the device's rate is
25,600 px/s (class C).

## 6. The six story pages

`story_screens` 0x0000 draws each page on black: page 0's headlines ("The" ENDTEXT 0 at (216,0),
"Homecoming" ENDTEXT 4 at (152,28)), page 3's ("Dream" ENDTEXT 5 at (224,0), "The" at (176,0)); then
the art (opaque, drawn after, so page 0's art covers Homecoming's first 15 columns); then END.DAT's
text through FONT.OVL 0x0000's justified engine: space 5 (DS 0x5154), glyph widths DS 0x50ca, advance
width + 1, leading 9, glyphs at y ≥ 0xc0 clipped, `{` an indent of 15, `_` a soft hyphen, the first
line at the caller's pen, the column (band) re-chosen at every line, extra space distributed by idiv.
Per page (DATA 0x3da6–0x3e06): text at END.DAT 0, 424, 956, 1530, 2280, 2932; bands, cut rows and
pens as `check-endgame-pages.ts` L1 prints them. The scroll page: ENDSC.16 at (40,0) on black.

## 7. The scroll (`endgame_datestamp` 0x0326)

Window 0 (40 × 25 cells of 8 × 8, opaque, IBM.CH font 0, RUNES.CH font 1), cursor (0,1), 0xfd
toggles reverse video, 0xfc centres each line at `(avail − last) / 2` = (40 − len) / 2. Rows 1–6, 8,
10–14 the proclamation in reverse on the parchment (day and month as ordinals, the year spelled
`spell_cardinal(y/100) Hundred` / `spell_cardinal(y%100)`, the Avatar's name), rows 16–17 in runes,
then 0xfd off and the report rows 21–23 (`Report now, …`, the playtime from 139/4/5 on a 13 × 28
calendar with borrows, `to Lord British at Origin Systems!`).

## 8. Timing

`run_n_frames(n)` = n × 55 ms (the 18.2 Hz tick, `scene_timing.h`). Footstep 555 samples → 21 ms at
the device's 25,806 Hz tone_sweep rate; one step = 110 + 21 + 165 = 296 ms. Revive sweep 0x9c40
samples = 1,550 ms; orb sweep 0xc350 = 1,937 ms. Lord British waits 40 frames (2.2 s) before walking;
40 frames before the greeting, after the box, after "I see...". Every getkey waits for ever.

## 9. Audio

- **Stock 1988 assets: no music.** Speaker sounds only: the footstep per step, the revive sweep per
  revived member, the orb sweep. The device plays them as `MoveStep` and `EndgameOrb` (param 1 / 0).
- **The Exodus patch** (`mid.drv`, SHA pinned by A3-04): selector **0x15** plays Joyous Reunion and
  sets Rule Britannia as current, so the driver's poll starts Rule Britannia when Reunion ends
  (0x0307–0x0314, 0x027c); **0x18** maps the story page through the table at 0x24a (scenes 0–3
  Stones, 4–7 Dream of Lady Nan); **0x1b** asks for Rule Britannia, which waits for a Reunion still
  playing (0x032a → 0x0267). The device plays these only when the supported patch's songs are in the
  audio pack; the chain is timed from the parsed song length (`AudioBackend::music_length_ms`).

## 10. The last screen

1988: the victory loop 0x04f9 and the stranded wander 0x0ac9 never end and read no key. The patch:
any key on the scroll exits to DOS. The device keeps the frozen screen (the scroll for ever; the
stranded room wandering for ever); only the System Menu answers (Load leaves; Return to Title
leaves); Save is refused from game-won on (Batch 53A); the device line `The quest is complete.
Alt+M: System Menu` appears once the stranded wander begins (A-15) and never over the scroll.

## 11. Device adaptations (documented, not invented art)

1. The 320 × 200 pages sit at y 20 of the 240-row panel, black bands above and below; the fizzle runs
   over the picture (fn34, 320 × 200) and over the bands as their own 320 × 40 rectangle, kept in step.
2. The seven full-screen pages are pre-composed by the pack builder (`alpha1-endgame.ts`, the FONT.OVL
   port above) from the user's own files and shipped in the SD resource pack (`endgame-pages.bin`,
   7 × 32,000 B 4bpp; `endgame-room.bin`, 121 B) — flash stays untouched; the scroll's text is drawn
   at run time from the live date and name.
3. The dissolve's start frame is captured by mirroring the Board's own full repaint into a PSRAM copy
   (no read-back from the panel); without the memory or without a frame the fizzle is skipped, logged.
4. Keys: a getkey ends on any key (Mic = ESC, Backspace = BS are keys, as at 0x266c); the box
   questions take only Y / N, case-folded; the trackball's roll is no key; a key pressed while no
   getkey is open is dropped (no BIOS type-ahead), so one press ends at most one wait. The getkeys
   wear the device cue `Enter: continue` / `Y / N` on the status line (as the other paced scenes).
5. The scene clock stands under the System Menu and the Developer screen and resumes where it
   stopped; a frame never moves it more than 250 ms.
6. The pose RNG is drawn per presented frame; the wander's RNG is seeded from the game's RNG.

## 12. Queued, not changed (A4-END1 scope)

- `VICTORY!` (the native arena's own line as the absorption ends combat) precedes the ending's text;
  the original jumps from `absorb` straight to the overlay. Pinned by Batch 53A R3 — a combat change,
  queued (ledger D-72).
- The TypeScript reference's rescue narration (`rescue_events`) is not ENDMSG's order or text; the
  device no longer uses it, quest_parity still pins it. Queued as a reference fix (ledger D-73).
- The Exodus patch's "any key exits to DOS" on the scroll is not reproduced (there is no DOS; A-15).
