# Ultima V Native v0.4.0

Ultima V Native v0.4.0 is the first public pre-1.0 release of a complete, playable *Ultima V: Warriors of Destiny* for the LilyGO T-Deck Plus, from character creation to the ending, with a finished front end, real save management and optional conveniences. It is a pre-1.0 public release: the game is complete, with documented non-blocking discrepancies still under active preservation work (see *Known limitations*).

**Firmware:** `UltimaV-Native-v0.4.0-TDeck.bin` — 1,047,408 bytes
**SHA-256:** `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210`
**Firmware provenance.** v0.4.0 uses the exact hardware-validated firmware previously developed as RC5; the binary was not rebuilt for the public versioning cleanup, and the internal version string is retained because those exact bytes were physically validated. The boot identity screen reads `FW 4.0.0-alpha4-rc5-debug` and `Git 59c14b907399`. That is correct: the hardware-tested RC5 build is the exact firmware published as v0.4.0.

## You must supply your own Ultima V files

This project distributes **no** Ultima V game data, art, music, text or maps — the original game belongs to Electronic Arts. You need your own copy of the DOS game (for example the *Ultima 4+5+6* pack on GOG) and you generate the device's resource files from it.

**Music is asset-dependent.** The stock DOS game has **no music**, so a stock install plays all the sound effects (synthesized on the device) but no songs. Community music-patched game files (the Exodus *Ultima V Upgrade* 1.0 patch) can provide music; if you install that patch, the optional audio pack carries its songs. Without it the game is complete, just without music, and Settings tells you why.

## What's in v0.4.0

- **A new interface.** The "More Ultima V" look in the original's EGA colours and font, a centred title with credits, a console with the original's bullets and cursor, reverse-video lists, the moongate transit and the original's console echoes.
- **Save slots.** Three manual slots, each with a backup generation that is never overwritten by the one you just loaded; two-row slot pages marked CURRENT / LATEST / RECOVERED; Continue loads the slot you saved last.
- **PC save import / export.** Bring a DOS `SAVED.GAM` + `SAVED.OOL` into a slot, or export a slot back (title screen → **P**). Town NPC tables and dungeon saves are not carried across.
- **The ending.** The original's full ending sequence plays: throne room, the questions, the dissolve, the story pages and the final scroll.
- **Preservation and parity work.** Many fidelity fixes against the original game: the palace crown gate, the worn crown, Negate against enemy magic, the dungeon's command keys, the wishing well, and a cleanup sweep (D-82 – D-89) covering the naval OUCH, healer and Refuge resurrection, the Stonegate trapdoor, invalid dungeon digits, the food cap, the combat clock and the new-game underworld skiff and bodies.
- **Trackball.** Click toggles WASD movement mode; ten speed levels.
- **Developer menu cleanup.** Alt+D opens the Developer menu from anywhere; it no longer clutters the System Menu.
- **Cheats.** Party, Inventory and World cheats, plus God Mode (System Menu → Cheats).
- **Difficulty.** Original / Relaxed / Easy / Custom presets. **Original** is the default and plays the 1988 rules; nothing changes unless you choose it.
- **Keyboard backlight.** A brightness setting for the keyboard light.
- **More room for the game.** The Launcher app allocation was expanded (the firmware now fits a 1.25 MiB partition; Launcher sizes its own slot, 1,024 KiB for this image).
- **Dungeon music fix.** Dungeons now play the dungeon song, and the surface song returns when you leave (music-patched files only).
- **Final hotfix.** The healer's **R** now resurrects (it did nothing in the previous candidate), and the In Mani Corp scroll on a living target shows the right messages.

## Installing

**Supported hardware:** LilyGO T-Deck Plus (ESP32-S3, 16 MB flash, 8 MB PSRAM) with a FAT-formatted microSD card, running the T-Deck **Launcher**.

1. **Firmware.** Copy the `.bin` to the SD card (anywhere) and install it from Launcher: **SD** → select the file. Launcher sizes its own app slot from the image (1,024 KiB for this one).
2. **Resource files.** From the repository, with your Ultima V files in `original/u5/ultima5/`:
   - `npm install`
   - `npm run pack:native` → `native/assets/openu5-assets.bin` (needs `TILES.16`, `BRIT.DAT`, `DATA.OVL`, `INIT.GAM`, `DWELLING.DAT`)
   - `npm run pack:alpha1` → `native/assets/openu5-alpha1-resources.bin` (**2,266,819 bytes** for v0.4.0 — the firmware refuses any other resource pack)
   - optional music: `npm run pack:audio` → `native/assets/openu5-audio.bin`
   The exact file requirements and identities are in [`native/ASSETS.md`](native/ASSETS.md) and [`ALPHA4.md`](docs/history/alpha/ALPHA4.md) §4 – §5.
3. **SD layout:**
   ```text
   /
   |-- UltimaV-Native-v0.4.0-TDeck.bin   (location not significant)
   `-- ultima5/
       |-- openu5-assets.bin
       |-- openu5-alpha1-resources.bin
       |-- openu5-audio.bin      (optional; music capability)
       |-- settings.json         (created automatically)
       |-- saves/                (created automatically; slots 1-3)
       |-- import/               (optional: SAVED.GAM + SAVED.OOL to import)
       |-- export/               (created on export: slot1/ slot2/ slot3/)
       `-- logs/                 (created automatically)
   ```
4. **Saves and settings** live under `/ultima5/` (`saves/`, `settings.json`). Back up that folder before the first boot of any new image. Alpha 3 saves load as Slot 1.

Coming from the Alpha 3 development build: flash the image and copy the new 2,266,819-byte resource pack. Coming from an earlier development test image from the ending batch onwards: flash only.

## Known limitations

- Pre-1.0 release: the game is complete, but this is a Debug build (includes the Developer menu and verbose logging).
- Fourteen small known divergences from the original (ledger rows D-90 – D-103) are recorded and not fixed.
- Real heap/memory figures for v0.4.0 were not captured; no memory problem was observed.
- The PC save bridge was tested against the host and the device, not against a real DOS install.
- The naval cactus check was not run.
- Some host parity tests need numeric fixtures generated from your own Ultima V files (the repository ships none); without them those tests are skipped. See `native/core/README.md`, "Local map fixtures".
- Music requires music-patched game files and the optional audio pack.

## Relationship to OpenU5

Ultima V Native was built with substantial help from the OpenU5 codebase: OpenU5's TypeScript implementation was a major behavioral reference, and the original DOS executable was reverse-engineered to settle disagreements. OpenU5 is a separate browser/reference implementation; none of its browser, website or mobile features are part of this T-Deck release. Ultima V Native is not a straight port of OpenU5 and not an independent clean-room reconstruction. File names that still say `openu5` are legacy technical identifiers kept for compatibility.

## Reporting bugs

Open an issue on the project's GitHub page with: what you did, what you expected, the firmware identity (`FW …` / `Git …` from the Developer screen), and, where possible, your save. See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## For engineers

The detailed record — every batch, test, mutation proof, hardware report and the known-divergence ledger — is in [`ALPHA4.md`](docs/history/alpha/ALPHA4.md), [`native/targets/tdeck/ALPHA4_UI.md`](native/targets/tdeck/ALPHA4_UI.md) (§18 is the release record) and [`PROJECT_HISTORY.md`](docs/history/PROJECT_HISTORY.md).
