# Ultima V Native — T-Deck Plus — Alpha 2

**Status: ALPHA 2 RELEASED / CLOSED (2026-09-26, Batch 55).**
- Release tag: `alpha2-batch55-release`.
- The release firmware is the hardware-validated **RC1** image, promoted byte for byte. Its source commit is `211c676a` (`alpha2-batch54-rc1`).
- The RC smoke test passed on a device, steps 1–8.

> *History:* this file was first published in Batch 54 as the RC1 notes, with the status "ALPHA 2 RC READY — HARDWARE SMOKE PENDING". That status is superseded by the Phase 8 PASS recorded in Batch 55.

## What Alpha 2 is

Alpha 2 runs the native Ultima V Native port of *Ultima V: Warriors of Destiny* (Origin Systems, 1988) on the LilyGO T-Deck Plus. The game can be played from character creation to the ending. Its rules come from the original binary, and the browser/TypeScript reference port is used as a cross-check. You need your own copy of the original game data; nothing from it ships in the firmware.

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

- **LilyGO T-Deck Plus:** ESP32-S3, 16 MiB flash, 8 MiB octal PSRAM, 320×240 ST7789, keyboard, trackball. This is the only supported device.
- **Launcher:** the firmware is installed as a Launcher app. It needs an app allocation of **at least 896 KiB** (the 878,752 B image rounded up to 64 KiB).
- **SD card:** a FAT-formatted microSD with the two resource packs (below). Leave it inserted while playing.

## Files

| File | Size | Identity |
|---|---:|---|
| `native/targets/tdeck/build-batch54/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin` | 878,752 B | SHA-256 `ff3dfe193973547db2648c5f486aa381086505eb80b5f2dadec7432194fb5828` |
| `/ultima5/openu5-alpha1-resources.bin` (SD) | 2,041,466 B | CRC32 `26f75ae6`, SHA-256 `a48abdbfc88eb5ab43453880a8ea029a1ea0684dfec2045f9dae941b31aa379b` |
| `/ultima5/openu5-assets.bin` (SD) | 132,284 B | CRC32 `933c9b82`, SHA-256 `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188` |

**The Alpha 2 release image keeps its RC1 name and identity.** It is the exact file that passed the hardware smoke. On boot, the identity screen shows:
- `FW 2.0.0-alpha2-rc1-debug`
- `Git 211c676a1dca`
- `RES v2.0 2041466B CRC 26f75ae6`
- `ASSET … 132284B CRC 933c9b82`

The firmware refuses to start with any other resource pack; a Batch 51 or older pack is rejected. The image is a Debug build: it includes the Developer menu and verbose serial and SD logging.

## Installing or updating

**Resource packs.** You only need to build them if you do not already have the Batch 53 pack. Generate them from your own Ultima V data:
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

**Firmware.** Copy the Launcher image anywhere on the card, then install it from Launcher (**SD** → select the file).

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

| Coming from | What to do |
|---|---|
| **Alpha 2 RC1** (`Git 211c676a1dca`) | **Nothing.** You already run the release image. |
| Batch 53, 53A or 53B | Flash the Launcher image. **No SD recopy**: the resource pack is unchanged. |
| Batch 52 or earlier | Flash the Launcher image **and** replace `/ultima5/openu5-alpha1-resources.bin` with the 2,041,466 B pack above. Keep `openu5-assets.bin` and `saves/`. |

## Save compatibility

- Saves live in `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`. The release changes no save code or format, so saves from earlier Alpha 2 images load.
- A save made after the ending (a won game) opens straight into the ending, which is correct. To play on, Continue a save from before the ending, or use Return to Title.

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

## Validation

- **Host suite: 123 / 123.** A fresh serial run on the release tree, which includes the quest and gameplay parity corpora. The release-blocker, ending, moongate, persistence and stale-pack checks were all green.
- **Physical validation: complete.**
  - Phases 6Q, 6T – 7D passed on hardware, including beds, sleep, Camp, advancement, scene pacing, dungeon save and `Alt+L`.
  - Phase 7E covered the ending and its terminal state, a Word of Power, moongate and Vas Rel Por, buying transport, and the Destard room.
- **RC smoke (Phase 8): PASS**, on the release image itself:
  1. boot identity;
  2. basic input and overlays;
  3. save / `Alt+L`;
  4. power-cycle Continue;
  5. a dungeon;
  6. combat;
  7. Camp;
  8. the Refuge scene.

  There was no crash, lock, stale-resource refusal or input-mode corruption.

Details:
- [`docs/hardware/ALPHA2_HARDWARE_CHECKLIST.md`](../../hardware/ALPHA2_HARDWARE_CHECKLIST.md) (Batch 54 and 55 sections);
- [`native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md`](../../../native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md) (§14 "Batch 55").

## Known issues (non-blocking)

None of these locks the game, loses state or hides a control.

- **No audio or music.** The original's sound-driven pauses are reproduced silently.
- **No moongate transit or gate animation.** Stepping onto a gate moves you instantly.
- **No endgame cinematic or paged ending.** The ending is transcript text over the Doom view.
- **After the ending, two literal `victory` lines** follow the report text (H-194). The ending's two box questions are answered for you (H-193).
- **Small scene-fidelity gaps** (H-183 – H-186):
  - shrine and Codex text arrives without its key waits;
  - the donation and Codex screen inversions are missing;
  - the Refuge speech runs on a timer instead of waiting for a key;
  - Blackthorn's sacrifice has no burst.
- **The Ready picker** opens, disabled, with an empty hand (H-139/H-140).
- **Other small fidelity items:**
  - the wishing well is case-sensitive (H-22);
  - cancelling a potion's target refunds it (H-45);
  - the Z-stats `^` marker shows `?` (H-63);
  - non-combat `Direction?` prompts show the aim readout (H-15).
- **Unresolved original question:** the exact 1988 sleeping pose of named/scheduled NPCs.
- **Debug logging is kept on purpose.** The image logs input and object traces to serial and SD.
- **Not a bug:** stepping back onto the gate you arrived by keeps you there. This is the original's behaviour: every gate leads to the current lunar phase's stone.

Every knowing difference from the original is listed, with its reason, in [`docs/hardware/ALPHA2_PRESERVATION_LEDGER.md`](../../hardware/ALPHA2_PRESERVATION_LEDGER.md). The original's own defects, which the port reproduces on purpose, are in [`docs/bugs-del-original.md`](../../bugs-del-original.md).

## Alpha 3 handoff

Alpha 2 is closed. The Alpha 3 tracks below are defined. Each starts only when it is asked for. **Status (A3-02, 2026-09-26):** the audio track is in progress. The UI/frontend and presentation tracks have not started.

*Pre-RC reconciliation (2026-09-28): this handoff is superseded as a status. The audio track is closed, and the presentation track's H-183 – H-186 were done in A3-HF6 – A3-HF9. The Alpha 3 scope, what it defers (the UI track, D-54, D-56, D-57 and the rest) and its status are in [`ALPHA3.md`](ALPHA3.md). The lists below stay as written.*

**1. Audio / music** — **A3-01 done** (tag `alpha3-a3-01-audio-architecture`). This is architecture, not an Alpha 3 release. The reference is [`native/targets/tdeck/ALPHA3_AUDIO.md`](../../../native/targets/tdeck/ALPHA3_AUDIO.md).
- Done in A3-01:
  - the T-Deck I2S speaker path;
  - the semantic SFX vocabulary and the audio service;
  - the original SFX inventory, including the silent waits (D-3, D-10 / D-39) and the sound-flag branches;
  - the music-patch archaeology and capability detection: **stock DOS files have no music**, and only the Exodus *Ultima V Upgrade* 1.0 patch enables it;
  - the optional SD audio pack `openu5-audio.bin` (`npm run pack:audio`);
  - the **SFX Volume** / **Music Volume** Settings rows;
  - a Developer test tone.
- **A3-02 done** (tag `alpha3-a3-02-sfx-synth`; `ALPHA3_AUDIO.md` §15): the PC-speaker synthesizer (the original's primitives emulated from their loop bodies), 22 gameplay sounds — footstep, wall bump, dungeon and world cues, arena hits, the spell ceremony, the Camp apparition, Blackthorn's materialization — and the audible harpsichord (H-125, host-verified; its device check is §15.14).
- Next:
  - A3-03: the remaining SFX and scene cues (moongate, quake, shrines, shard ritual with the blocking-pause decision, ambient, bard song, intro / title / endgame);
  - A3-04: music playback and contexts;
  - A3-05: audio polish and hardware validation.

**2. UI / frontend**
- Audit the main game window and HUD.
- Audit the Settings menu.
- **Produce mockups before any code change.**
- Keep the layout constraints that gameplay depends on: the 176×176 game window, the status panel and the transcript.
- The Ready picker (D-8, H-139 / H-140), the `^` glyph (H-63), and the `Direction?` / aim overlay (H-15).

**3. Presentation fidelity**
- The moongate transit / gate animation (D-48).
- The endgame cinematic and a paged ending (D-54).
- The scene-fidelity rows H-183 – H-186 (D-40 – D-43).
- H-194's `victory` lines, and the real Y/N box prompts (H-193 / D-56, D-57).
- The remaining small items:
  - D-58 and D-59 (moongate);
  - H-22 and H-45;
  - the D-38 sleeping-pose question, which needs an original witness.

**Also carried into Alpha 3:**
- the open product decisions and reference questions in the ledger (D-1, D-2, D-4, D-5 – D-7, D-9, D-12 – D-16);
- hygiene: removing the log-only diagnostics from a release build, and the dead code in D-17 / D-18.
