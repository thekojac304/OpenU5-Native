# OpenU5 T-Deck Plus — Alpha 2 (Release Candidate 1)

**Status: ALPHA 2 RC READY — HARDWARE SMOKE PENDING.** Alpha 2 is released once the
short RC smoke test (Phase 8, below) passes on a device.

Alpha 2 runs the native OpenU5 port of *Ultima V: Warriors of Destiny* (Origin
Systems, 1988) on the LilyGO T-Deck Plus. The game can be played from character
creation to the ending. Its rules come from the original binary, and the
browser/TypeScript reference port is used as a cross-check. You need your own copy of
the original game data; nothing from it ships in the firmware.

## What Alpha 2 contains

- **The whole game loop on the device:**
  - Britannia and the Underworld, towns, castles and keeps;
  - the eight dungeons in the 3D view, with their rooms, traps and chests;
  - tactical combat, conversation, shops, magic, the shrines, the Codex, Blackthorn's palace;
  - the quest items, and the ending.
- **The quest's device services:**
  - Words of Power open the dungeon seals.
  - Moongates appear at night and transit by lunar phase.
  - Vas Rel Por asks `To phase:`.
  - Moonstones can be found, used, saved and loaded.
  - Shipwrights and stables sell ships, skiffs and horses.
  - The Refuge after a party wipe speaks the KARMA.DAT text.
  - The final Doom room runs the ENDMSG.DAT ending, then the original's terminal state: no game key resumes play, and Save is refused.
- **Rest.**
  - Bed sleep and Camp, with the watch and the day/night tiles.
  - The Camp apparition and staged level advancement.
  - Scripted scenes paced with the original's own waits.
- **Saving.**
  - Two-generation, CRC-verified saves.
  - Continue Latest from the title and from the System Menu.
  - `Alt+S` / `Alt+L` at any time.
  - Automatic fallback to the older generation when the newest is incomplete or corrupt.
- **Device frontend:**
  - title and New Journey character creation;
  - the System Menu (`Alt+M`: Save, Load / Save Management, Settings, Return to Title);
  - transcript scroll-back;
  - the Developer menu (`Alt+D`: teleport, party/stats/inventory editors, time, presets, on-device smoke tests).

## Supported hardware

- **LilyGO T-Deck Plus:** ESP32-S3, 16 MiB flash, 8 MiB octal PSRAM, 320×240 ST7789, keyboard, trackball.
- **Launcher:** installed as a Launcher app. It needs an app allocation of **at least 896 KiB** (the 878,752 B image rounded up to 64 KiB).
- **SD card:** a FAT-formatted microSD with the two resource packs (below). Leave it inserted while playing.

## Files

| File | Size | Identity |
|---|---:|---|
| `native/targets/tdeck/build-batch54/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin` | 878,752 B | SHA-256 and embedded `Git`: annotated tag **`alpha2-batch54-rc1`** |
| `/ultima5/openu5-alpha1-resources.bin` (SD) | 2,041,466 B | CRC32 `26f75ae6`, SHA-256 `a48abdbfc88eb5ab43453880a8ea029a1ea0684dfec2045f9dae941b31aa379b` |
| `/ultima5/openu5-assets.bin` (SD) | 132,284 B | CRC32 `933c9b82`, SHA-256 `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188` |

On boot, the identity screen shows `FW 2.0.0-alpha2-rc1-debug`, `Git <12 hex>`, `RES v2.0 2041466B CRC 26f75ae6` and `ASSET … 132284B CRC 933c9b82`. The firmware refuses to start with any other resource pack; a Batch 51 or older pack is rejected. The Launcher image is a Debug build: it includes the Developer menu and verbose serial and SD logging.

## Installing or updating

**Resource packs** (only if you do not have the Batch 53 pack already). Generate them from your own Ultima V data:
```bash
npm run extract
```
```bash
npm run pack:native
```
```bash
npm run pack:alpha1
```
Then copy both packs to `/ultima5/` on the SD card.

**Firmware.** Copy the RC1 Launcher image anywhere on the card, then install it from Launcher (**SD** → select the file).

**SD layout:**
```text
/
|-- OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin   (location not significant)
`-- ultima5/
    |-- openu5-assets.bin
    |-- openu5-alpha1-resources.bin
    |-- saves/     (created automatically)
    `-- logs/      (created automatically)
```

**Updating from Batch 53, 53A or 53B:** flash the RC1 image only. **The SD resource pack is unchanged; no SD recopy is required.**
**Updating from Batch 52 or earlier:** also replace `/ultima5/openu5-alpha1-resources.bin` with the 2,041,466 B pack above. Keep `openu5-assets.bin` and `saves/`.

## Save compatibility

- Saves live in `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`. RC1 changes no save code or format, so saves from earlier Alpha 2 images load.
- A save made after the ending (a won game) opens straight into the ending, which is correct. Continue a save from before the ending to play on, or use Return to Title.
- Moonstone positions travel in the save's own GAM bytes, so no new field was added.

## Controls (quick reference)

| Input | Action |
|---|---|
| Trackball | move; navigate menus, pickers and targets |
| Shift + trackball up/down | page the transcript |
| Letters | Ultima commands (Talk, Look, Get, Klimb, Search, Cast, Use, Yell, Hole up, Z-stats, …) |
| Enter / Space / Backspace | confirm / pass a turn / back |
| Mic key, short press | Cancel |
| Mic key, hold about 1 s | toggle movement mode |
| `Alt+M` | System Menu |
| `Alt+S` / `Alt+L` | save / load (latest) |
| `Alt+D` | Developer menu |

## Physical validation scope

- **Phases 6Q, 6T – 7D:** hardware PASS, including beds, sleep, Camp, advancement, scene pacing, dungeon save and `Alt+L`.
- **Phase 7E** passed on the Batch 53 / 53A images:
  - the ending and its terminal state;
  - a Word of Power;
  - moongate and Vas Rel Por;
  - buying transport;
  - the Destard Slime/Gargoyle room.
- **Accepted as previously witnessed:** the Refuge, the vault reset and door-after-load, and Gorn's brazier.
- **RC1 itself** changes only its version string and embedded `Git` hash relative to the 7E image; the **Phase 8 smoke** confirms it on a device.

Details: [`native/targets/tdeck/ALPHA2_HARDWARE_CHECKLIST.md`](native/targets/tdeck/ALPHA2_HARDWARE_CHECKLIST.md) (Batch 54 section).

The host suite gives 123 / 123 on the RC tree. The quest and gameplay parity corpora are part of it; audit trail: [`native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md`](native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md).

## Phase 8 — RC1 smoke test (about 10–15 minutes)

The exact steps are in the checklist, Batch 54 → Phase 8:
1. boot identity;
2. basic input;
3. save / load;
4. power-cycle Continue;
5. a dungeon;
6. one combat;
7. Camp;
8. the Refuge scene.

The ending and transport/moongate are covered by Phase 7E and the host suite.

## Known issues (non-blocking)

None of these locks the game, loses state or hides a control.

- **No audio or music.** The original's sound-driven pauses are reproduced silently.
- **No animations yet** for the moongate transit (a gate you step onto moves you instantly), the endgame cinematic, or the paged ending screens. The ending is transcript text over the Doom view.
- **After the ending, two literal `victory` lines** follow the report text (H-194). The ending's two box questions are answered for you (H-193).
- **Stepping back onto the gate you arrived by keeps you there.** This is the original's behaviour, not a bug: every gate leads to the current lunar phase's stone. Use a later night or Vas Rel Por to return (H-195).
- **A party standing on a gate is not re-sent** when the phase changes underneath it (D-58). The device never refreshes the moon-phase latch, and recomputes the phase from the day instead (D-59). Both are correct in ordinary play.
- **Scene fidelity gaps:**
  - shrine and Codex text arrives without its key waits (H-183);
  - donation and Codex inversions are missing (H-184);
  - the Refuge speech runs on a timer instead of waiting for a key (H-185);
  - no burst is drawn at Blackthorn's sacrifice (H-186).
- **Small divergences:**
  - the wishing well is case-sensitive (H-22);
  - cancelling a potion's target refunds it (H-45);
  - the Z-stats `^` marker shows `?` (H-63);
  - Ready with an empty hand opens a disabled picker (H-139/H-140);
  - non-combat `Direction?` prompts show the aim readout (H-15).
- **Unresolved original question:** the exact 1988 sleeping pose of named/scheduled NPCs.

Every knowing difference from the original is listed, with its reason, in [`native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md`](native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md). The original's own defects, which the port reproduces on purpose, are in [`docs/bugs-del-original.md`](docs/bugs-del-original.md).

## Deferred to Alpha 3

- Audio and music.
- UI rework and polish.
- The moongate transit / gate animation.
- The endgame cinematic, a paged ending screen, and the real Y/N box questions.
- Scene fidelity H-183 – H-186.
- H-22 wish case-insensitivity; H-45 potion cancel; H-63 caret glyph.
- The Ready picker (D-8).
- The named-NPC sleeping-pose evidence question (D-38).
- H-194's `victory` lines; D-58; D-59.
- The remaining open product decisions, reference questions and presentation rows in the ledger (D-1 – D-7, D-9, D-12 – D-16).
- Hygiene: the log-only diagnostics still compiled into the Debug image, and the dead code in D-17 / D-18.
