# Batch 53A — what the original does after the final Doom absorption

Question from the Phase 7E-A hardware run (Batch 53 image): after "Avatar is absorbed!", `VICTORY!`
and the ENDMSG text, the T-Deck left the party in Doom's enclosed final cell with ordinary movement
still accepted. Is that the original's state, or a missing transition?

**Answer: the original never returns to the dungeon.** From game-won onward the machine is inside
ENDGAME.OVL `endgame_main` for good. No world command, movement, save or relocation exists after it.

Evidence: `re/tools/batch53a_endgame_terminal.py` (reads the shipped binaries in
`original/u5/ultima5`; output committed as `native/core/batch53a-original-endgame.log`).
Kernel targets use the ENDGAME slot: `fileoff_ULTIMA = (CS + 0xA290) & 0xFFFF`.

## 1. Entry

| step | where | bytes / effect |
|---|---|---|
| absorption arms the sentinel | SJOG.OVL `absorb` 0x1ea4, write at 0x1edc | `[0x58a0] = 0x4d` (see `endgame.md` §Trigger) |
| combat teardown checks it first | SJOG.OVL 0x2046 / DUNGEON.OVL 0x00cb | `cmp byte [0x58a0],0x4d ; jne` → `call` resolving to kernel **0x7c4a** (both, re-resolved by the tool with each module's own slot) |
| the one overlay-13 stub | ULTIMA.EXE 0x7c4a | `9a ec 02 2e 07 0d 00 ea d8 a8` = loader, overlay #13, `ljmp 0xa8d8` → ENDGAME.OVL **0x0648** |

The teardown is diverted *before* it restores the combat actors; the stub is a call that, as shown
below, never comes back.

## 2. endgame_main (0x0648) owns the screen and the keyboard

- 0x066f / 0x0681 read **MISCMAPS.DAT** (its own scene map, into DS 0xac64) and **ENDMSG.DAT**
  (0x3e8 bytes into DS 0xb21e). The dungeon view is gone; the throne-room scene replaces it.
- Ten `getkey_with_redraw` (kernel 0x266c) call sites: 0x0848 0x0852 0x088b 0x092c 0x0936 0x0940
  0x094a 0x0954 0x095e 0x0970 — every page waits for a key.
- **The two Y/N questions are real prompts.** 0x0852–0x0874 loops on getkey until `'Y'` or `'N'`
  ("Didst thou bring my box?"); an `N` leads to the second question and the same loop at
  0x088b–0x08ad. 0x08b9: answer `'Y'` **and** `g_wooden_box` (DS 0x57bf) ≠ 0 → victory branch;
  anything else → "I see..." / "Well then, pull up a chair." (stranded).
  *The TypeScript reference and the native auto-answer from the box (`sequence.ts` "La fija el
  inventario … NO un prompt"); the bytes say the player answers. Ledger D-55, Alpha 3 (it lives in
  the ending presenter, which the native does not have — D-54).*

## 3. Neither branch returns

- `endgame_main` [0x0648, 0x0aee) has exactly **one** `ret`, at **0x0aed**. The only path to it is
  0x0a70 `call endgame_datestamp (0x0326)` → 0x0a73 `jmp 0x0ae8`.
- `endgame_datestamp` [0x0326, 0x04fe) has **no `ret`** and no jump out of its body. After the
  proclamation and "Report now … to Lord British at Origin Systems!" (0x04f6) it falls into
  **0x04f9 `call 0x0b0b ; jmp 0x04f9`**.
- The stranded branch ends in **0x0ac9–0x0ae5**: `wander_sprite(1,3,4,5)` then `call 0x0b1f`,
  whose body is `call 0x0b0b ; jmp 0x0ac9` — it never reaches the `ret` either.

So the `ret` at 0x0aed is dead in practice, and the combat teardown that called the stub never
resumes. The party's dungeon position is never read again.

## 4. The terminal loops in this build, and in the 1988 build

In `original/u5/ultima5` (2,864 B, sha1 `8c94bb4e…`), 0x0b0b is:

```
0b0b call kernel 0x1d5e   ; non-blocking key poll (int 16h AH=01)
0b0e cmp al,0 ; je 0b18
0b12 call kernel 0x0878   ; restore_video_mode
0b15 call kernel 0x02f4   ; exit() -> 0x034c -> 0x0e2f: driver shutdown, int 21h AH=4Ch
0b18 mov dx,0x1b ; call kernel 0x0e03   ; sound-driver tick
```

A key pressed after the ending (either branch) **terminates the program to DOS**. The
`fanfarria-endgame-espectral.md` addendum quotes `original/u5/play/ENDGAME.OVL` @0x04f9 as
`ff 46 fe eb fb` (`inc [bp-2] ; jmp 0x04f9`): in that build the loop spins without reading the
keyboard — a frozen screen until reset. That copy is not in this tree; both builds agree on
everything that matters here: **terminal, no return, no game command.**

## 5. The ten questions

| # | question | original |
|---|---|---|
| 1 | remain in the final Doom map? | No map is shown: the scene map from MISCMAPS.DAT replaces it, and the dungeon loop never runs again. The stored position is irrelevant. |
| 2 | arena torn down? | The teardown is diverted by the sentinel before it restores anything; the arena is simply never returned to. |
| 3 | party relocated? | No. Nothing writes the party position; the game never resumes. |
| 4 | dedicated ending mode? | Yes: ENDGAME.OVL, with its own map, text file, key loops and terminal loop. |
| 5 | return to LB / surface / title / credits? | No. The throne scene is *inside* ENDGAME.OVL; there is no title or credits call. |
| 6 | movement ever resumes after game-won? | **Never.** |
| 7 | ending takes over permanently? | Yes. |
| 8 | acknowledgement loop after the final text? | Pages before it wait on getkey. After the final text: the 0x04f9 loop — a key exits to DOS (this build) or nothing reads keys (1988 build). |
| 9 | restart / title / new-game transition? | None inside the game. The program ends (or hangs); starting again is relaunching ULTIMA.EXE. |
| 10 | gameplay state vs presentation? | Gameplay: game-won, no further world input, no save, no relocation, the Y/N choice. Presentation: the green throne scene, orb/moongate, dissolve, story pages, scroll (D-54). |

## 6. Consequence for the native (Batch 53A, Outcome B)

The native prints the ending as transcript text (D-54) and then let the per-input resyncs put the
session back in `UiMode::Dungeon` — in a cell whose four walls are real data (Doom floor 7 (5,7),
room 15; Forward fails in every facing). Batch 53A adds the terminal `UiMode::Ending`: entered on
GameWon, not leavable by `set_base_mode()`, swallowing every game key; transcript paging, the
System Menu (Load, Settings, Developer, **Return to Title** = the device's program exit) stay;
Save is refused. The GOG build's "any key exits" is not bound to arbitrary keys, because the device
shows the whole ending as one transcript and a stray key would discard unread text.
