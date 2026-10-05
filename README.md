# Ultima V Native

**A native implementation of *Ultima V: Warriors of Destiny* for the LilyGO T-Deck, focused on preservation, portability and optional modern quality-of-life enhancements.**

| | |
|---|---|
| **Current public milestone** | Alpha 4 (tag `alpha4-release`, 2026-10-04), a pre-1.0 public release |
| **Platform** | LilyGO T-Deck Plus (ESP32-S3), installed through the T-Deck Launcher |
| **Firmware** | Download from [GitHub Releases](https://github.com/thekojac304/OpenU5-Native/releases) |
| **Game data** | Bring your own copy of Ultima V; nothing from the original game is distributed |

Alpha 4 is complete enough to play from character creation through the ending. This repository (also known as OpenU5 Native) additionally contains the original OpenU5 browser port, described [below](#the-browser-port-openu5).

## Screenshots

<!-- Screenshots will be added here. Planned, non-violent where possible:
     title screen, overworld, dungeon, save/load slots, Settings / Difficulty, Cheats (Enhanced mode). -->

*Screenshots are not in the repository yet.* Photos or captures of the title screen, overworld, dungeon, save/load UI, Settings / Difficulty and Cheats screens are planned.

## What this project is

This is **not an emulator**. It is a native implementation (a port) of the game: a portable gameplay core with platform-specific input and presentation layers. The T-Deck target drives the core with the device's keyboard and trackball, draws to its display and reads and writes the SD card. The rules in the core were re-derived from the original DOS executable and are checked against it.

There are two goals, kept separate:

1. **Preservation.** Reproduce the original DOS game's behavior as closely as practical.
2. **Optional enhancement.** Offer modern conveniences for a handheld, clearly separated from the 1988 behavior.

## Original mode and Enhanced mode

**Original** is the preservation-focused mode and the default. It aims to reproduce the DOS game's behavior, including parity fixes derived from reverse engineering and comparison work. With Difficulty set to **Original**, no cheats and no World toggles, the game plays the recreated 1988 rules; the regression goldens recorded before each enhancement still reproduce bit for bit.

**Enhanced** is a set of optional conveniences. They are not presented as authentic 1988 behavior:

- Difficulty presets (Original / Relaxed / Easy) and a Custom difficulty
- Cheats (Party, Inventory, World, God Mode)
- Improved trackball behavior (click toggles WASD movement mode, ten speed levels)
- Save-slot UX (slot pages, backups, recovery marks)
- Keyboard backlight control
- A handheld-oriented interface

Not every presentation change can be switched off: the device front end, save management and Developer-menu layout are part of the T-Deck build in both modes. The rules-affecting options (difficulty, cheats, World toggles) are what Original mode leaves untouched.

## Alpha 4 highlights

- Native T-Deck gameplay from character creation to the ending, including the original's full ending sequence
- A new front end in the original's EGA colours and font: centred title with credits, console, reverse-video lists, moongate transit
- Three manual save slots, each with a backup generation; save recovery; Continue loads the slot you saved last
- PC save import / export for the original `SAVED.GAM` + `SAVED.OOL`
- Trackball redesign: click-to-toggle WASD mode and ten speed levels
- Difficulty presets, Custom difficulty and cheats
- Keyboard backlight setting
- Cleaner Developer menu (Alt+D)
- A larger flash layout (1.25 MiB app partition) with room to grow
- Optional music when community music-patched game files are supplied, including a dungeon-music fix
- A broad preservation / parity cleanup against the original: palace crown gate, worn crown, Negate against enemy magic, dungeon command keys, the wishing well, the combat clock, healer and Refuge resurrection, new-game underworld skiff and bodies, and more

Full list: [`ALPHA4_RELEASE_NOTES.md`](ALPHA4_RELEASE_NOTES.md). Engineering history: [`ALPHA4.md`](ALPHA4.md) and [`native/targets/tdeck/ALPHA4_UI.md`](native/targets/tdeck/ALPHA4_UI.md).

## How it was built: AI-assisted development

This project was developed heavily with AI coding agents, or informally, "vibe coded." AI wrote a substantial amount of the implementation. Its output was not accepted blindly: changes were repeatedly checked against the original DOS executable, a reference implementation, parity corpora, regression and golden tests, mutation testing, clean firmware builds and a physical T-Deck.

Confidence in the result comes from that evidence, not from who or what wrote the code. For Alpha 4 that includes:

- 201 / 201 host tests passing on the release tree
- extensive mutation testing of the parity fixes (for example 110 / 110 mutants killed in the final parity batch)
- binary-level reverse-engineering notes for the original executable and overlays
- repeated testing on physical T-Deck hardware
- a hardware-validated firmware image, RC5, promoted byte for byte as the Alpha 4 release

This is not a claim of formal verification.

## Development and validation process

Typical work on a behavior follows this loop:

1. Identify a behavior or discrepancy.
2. Inspect the original DOS binary when needed.
3. Define the expected behavior against the reference.
4. Write RED-first tests that fail for the right reason.
5. Implement the native fix.
6. Run the parity, golden and preservation tests.
7. Run mutation / adversarial testing to prove the tests actually bite.
8. Build the firmware and run the build guards.
9. Validate on physical hardware.
10. Document the evidence and the release candidate.

Known discrepancies are tracked rather than hidden. See the [preservation ledger](native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md), the [gameplay integration audit](native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md), the [deliberate divergences](re/deliberate-divergences.md) and the [fidelity docs](docs/FIDELITY.md).

## Platform and hardware

The supported native platform is the **LilyGO T-Deck Plus (ESP32-S3, 16 MB flash, 8 MB PSRAM)**. The firmware uses the device's keyboard, trackball, 320x240 display and microSD card, and installs through the [Launcher](native/targets/tdeck/LAUNCHER.md). See [`native/targets/tdeck/README.md`](native/targets/tdeck/README.md) for the hardware basis and build notes.

## Installing (overview)

1. Download the current firmware (`OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin`) from [GitHub Releases](https://github.com/thekojac304/OpenU5-Native/releases).
2. Install it on the T-Deck through the Launcher's SD installer.
3. Supply your own Ultima V files and generate the SD resource packs from them on your own machine (`npm run pack:native`, `npm run pack:alpha1`, optionally `npm run pack:audio`).
4. Put the packs under `/ultima5/` on a FAT-formatted microSD card.

The original game's data, maps and assets are not distributed by this project, and the generated packs and local test fixtures derived from your files are not committed. Exact file requirements, sizes and the SD layout: [`ALPHA4_RELEASE_NOTES.md`](ALPHA4_RELEASE_NOTES.md), [`native/ASSETS.md`](native/ASSETS.md).

## Music

The stock DOS Ultima V has **no music**, and this project does not synthesize music that was not in the original. Sound effects are produced on the device. Users with community music-patched Ultima V files (the Exodus *Ultima V Upgrade* patch) can generate an optional audio pack and get music. Music controls are capability-aware: with no music assets present the game stays faithful to stock DOS, and Settings explains why. No music assets are distributed here.

## Saves and PC compatibility

Alpha 4 has multiple native save slots, Continue, save recovery and PC save import / export.

PC save import/export is supported, with a few documented edge cases around transient town state, dungeon saves, and certain world objects:

- Town NPC, chest and other transient state may reset or be re-seeded across a DOS / native round trip.
- Dungeon saves are intentionally refused for transfer; leave the dungeon first.
- Some transient world objects, such as dropped items, parked monsters and carpets, are not carried.

Ordinary party, inventory and quest progress is what the bridge is for. The full technical list is in [`ALPHA4_UI.md` §5](native/targets/tdeck/ALPHA4_UI.md) (A4-SAVE3, §5.9 for round-trip limitations).

Import and export were tested on the host and on the T-Deck's own import / export. A full Native → DOS → DOS save → Native round trip against a real DOS install was not run and is not claimed.

## Project status

- **Current milestone:** Alpha 4, a pre-1.0 public release
- **Release tag:** `alpha4-release`
- **Firmware:** RC5, hardware-validated and promoted unchanged as the release image
- **Host suite:** 201 / 201 passing
- **Playable:** the game is substantially complete and playable through the ending

This is not a claim of perfect parity. Known, non-blocking discrepancies remain documented in the ledgers above.

## Roadmap

**Next major goal: Windows.** The T-Deck work has matured the native core enough that the next major project is a Windows port. The intended direction:

- keep a fully faithful **Original** mode
- keep the optional **Enhanced** mode
- adapt controls and UI for the desktop
- reuse the native core where practical
- continue preservation and parity cleanup

No dates are promised. Broader portability to other platforms is a possible long-term direction, nothing more. The older browser-port roadmap is in [`ROADMAP.md`](ROADMAP.md).

## Known limitations

- Pre-1.0 software; the Alpha 4 firmware is a Debug build that includes the Developer menu.
- Some documented, non-blocking parity differences remain.
- Some rare edge cases have no physical-hardware coverage, and real heap figures for Alpha 4 were not captured (no memory problem was observed).
- You must supply your own game files.
- PC save transfer has the edge cases listed above.
- Music depends on supplied assets.

Exhaustive detail: [`ALPHA4_RELEASE_NOTES.md`](ALPHA4_RELEASE_NOTES.md) (Known limitations) and [`ALPHA4.md`](ALPHA4.md).

## Reporting bugs and contributing

A useful bug report includes:

- the release / version and, if available, the firmware identity (`FW …` / `Git …` on the boot or Developer screen)
- Original or Enhanced mode, and the Difficulty / cheat settings if they were changed
- your save state and the steps to reproduce, with screenshots or logs when useful
- whether it reproduces after a reload or power cycle

Use the issue templates in [`.github/ISSUE_TEMPLATE`](.github/ISSUE_TEMPLATE) and read [`CONTRIBUTING.md`](CONTRIBUTING.md). The project's rule: no gameplay change is accepted without provenance (an assembly citation or a parity scenario).

## The browser port (OpenU5)

The repository also holds the original **OpenU5**, a byte-exact browser port of the game (TypeScript + PixiJS) whose rules were re-derived from the binary, with the assembly cited next to the code. Highlights:

- the `ULTIMA.EXE` kernel and its 24 overlays were disassembled, and the coverage ledger justifies 202,800 / 202,800 bytes
- hundreds of parity tests compare the re-derived model with the engine, plus runtime checks against the real binary in headless DOSBox-X
- the original add/rotate/xor RNG is reimplemented bit-exactly
- a full-game "Grand Tour" playthrough test

Run it locally with your own files in `original/u5/ultima5/`:

```bash
npm install
npm run extract        # or: npm run extract -- --src /path/to/your/ultima5
npm run dev
```

Controls: [`docs/controls.md`](docs/controls.md). Method: [`docs/methodology.md`](docs/methodology.md). A browser "bring your own files" demo is at [openu5.org](https://openu5.org); extraction runs client-side and nothing is uploaded.

Some host parity tests (`native/core`) read local fixtures generated from your own game files; they are gitignored and skipped when absent. See "Local map fixtures" in [`native/core/README.md`](native/core/README.md).

## Credits

- Origin Systems and the original Ultima V creators
- The OpenU5 reference lineage: the TypeScript port in this repository, which serves as the reference implementation
- [Ultima5Redux](https://github.com/bradhannah/Ultima5Redux) (MIT), source of tile metadata; see [`NOTICE`](NOTICE)
- LilyGO and the T-Deck ecosystem and reference code
- bmorcelli's Launcher, the SD installer used for firmware installation
- The community Exodus *Ultima V Upgrade* music patch (not distributed here)

## Copyright and assets

Ultima V is copyrighted by its respective rights holders. Ultima is a registered trademark of Electronic Arts; this project is not affiliated with or endorsed by EA. The repository contains **no game data, art, music, text or maps** from Ultima V. You must provide your own legally obtained copy, and local fixtures and resource packs derived from it are generated on your machine and never committed. If you are a rights holder with concerns, please open an issue.

Code is licensed under GPL-3.0; see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
