# OpenU5 T-Deck Plus — Alpha 3 (released)

**Status: ALPHA 3 RELEASED (2026-09-28).** The hardware-tested RC1 image is the final Alpha 3 firmware, byte for byte.
- **Release image:** `OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin` — 988,320 B (`0xf14a0`), 60,256 B (5.7 %) free in the 1 MiB app partition, SHA-256 `4b5b9d1f5cb5c14fc2628da6c5ce2befb18f4c81daaa5bfd04821e1ee63353b1`, `FW 3.0.0-alpha3-rc1-debug`, embedded `Git f85575b96f05`.
- **Promoted unchanged** (Option A, as Alpha 2's Batch 55): no rebuild, `PROJECT_VER` untouched, no second file name. The `rc1` in the version string is kept on purpose: the physical validation applies to this exact file, and a rebuild would be a new, untested image.
- **Tags:** `alpha3-rc1` is the source commit `f85575b96f05bd7250ef41347f858bfc954e276b` (its evidence commit is `0f15e8736b3b37b2266297c044e197275b0373d9`); **`alpha3-release`** is the closeout commit, which changes documents and logs only. Excluding `*.md` and `*.log`, its diff against `alpha3-rc1` is empty.
- **Validation:** host **162 / 162** serial on the release tree (134.68 s); `tsc --noEmit` clean and the `game/` vitest FAIL set equal to the A3-HF10 baseline's 97 (RC1 tree; `game/` is unchanged since); the three image guards GREEN (RC1 image). **Phase A3-RC1: PASS** on the device (§9). **RC1 heap gate: PASSED — A3-04H deferred** (`native/targets/tdeck/ALPHA3_AUDIO.md` §37).
- **Release blockers: none.** Everything §2 lists is deferred past Alpha 3, and none of it locks the game, loses state or hides a control.
- Alpha 2 (tag `alpha2-batch55-release`, [`ALPHA2.md`](ALPHA2.md)) is the previous release.

> *History:* drafted in the Alpha 3 pre-RC reconciliation (2026-09-28); the RC1 batch replaced its status with "RC1 — hardware smoke pending"; the release closeout (2026-09-28) replaced that with the release status, recorded the Phase A3-RC1 result in §9 and §10, and updated the rows that named the pending gate (§2 A3-04H, §11, §12). The RC1 wording is superseded and remains in tag `alpha3-rc1` and the git history.

## 1. What Alpha 3 is

Alpha 3 is Alpha 2's complete game — character creation to the ending, on the LilyGO T-Deck Plus — with three additions:
- **sound**: the original's PC-speaker effects, and music when the user's DOS files carry the supported community music patch;
- **a faster, steadier device**: rendering, audio real-time, watchdog and storage-heap work;
- **presentation and gameplay parity fixes** found on hardware during Alpha 3: HF1 to HF10.

Its rules still come from the original binary, with the browser/TypeScript reference port as a cross-check. You need your own copy of the original game data; nothing from it ships in the firmware or in the repository.

## 2. Scope

**Alpha 3 includes:**
- the completed audio track: sound effects, audio controls, and music (§3, §4);
- **patched-DOS music**: the Exodus Project *Ultima V Upgrade* 1.0, detected from the files themselves;
- **stock-DOS behaviour**: the 1988 game has no music, so none is played or synthesised;
- the audio controls: SFX Volume, Music Volume, and the session mutes;
- the performance, render and watchdog fixes of A3-04A to A3-04G;
- the gameplay and presentation parity work through A3-HF10, Mix parity included;
- the current functional ending (Alpha 2's: ENDMSG text, then the terminal state);
- the current device UI (Alpha 2's frontend, plus the Alpha 3 rows and diagnostics).

**Deferred until after Alpha 3 (the final list).** These are known and catalogued, not forgotten. None of them locks the game, loses state or hides a control. The release does not fix any of them.

| Item | What is missing | Record |
|---|---|---|
| H-209 / D-67 | The shard ritual's seven bursts: 1988 holds each for 174 ms with a noise burst; the device flashes 60 ms on / off, silently. | ledger D-67; `ALPHA3_AUDIO.md` §34.12 |
| H-211 / D-68 | The Refuge's stage content: when the Avatar appears, and the final stage showing the Avatar alone. The cadence itself is A3-HF9. | ledger D-68; §35.12 |
| H-212 / D-69 | On a static viewport the device shake is one 2 px drop held for the shake, not eight pulses (every quake). | ledger D-69; §35.12 |
| D-54 | The ending cinematic and paged presentation. The ending is transcript text over the Doom view. | ledger D-54 |
| D-56 (H-193) | The ending's two box questions are answered for the player; in 1988 they are real Y/N prompts. | ledger D-56 |
| D-57 (H-194) | Two literal `victory` lines follow the ending's report text. Still applicable. | ledger D-57 |
| D-71 | Mix presentation residue: the 10-tick wait after "Mixing...", the "Mix Reagents" echo, the console footer, the typed spell name. | ledger D-71; §36.6 |
| UI / frontend | The redesign and polish track (HUD, Settings, the Ready picker D-8, the `^` glyph D-52, the `Direction?` overlay H-15). No mockups exist yet. | `ALPHA2.md` "Alpha 3 handoff" §2 |
| A3-04H | Storage import count and PSRAM routing of the save document. **Deferred:** the RC heap capture fired no trigger it could not explain (`ALPHA3_AUDIO.md` §37), so it did not start. | `ALPHA3_AUDIO.md` §28.18, §28.20, §37 |
| Other classified items | D-48 moongate transit animation; D-58 / D-59 (moongate); D-50 (H-22), D-51 (H-45); D-38's NPC sleeping pose (needs an original witness); D-10 / D-39 sweep-duration residuals; D-60 (the fanfare does not hold the game — the A3-01 rule); the open decisions and questions D-1, D-2, D-4, D-5, D-7, D-9, D-12 – D-16; hygiene D-17 / D-18; §19.17 F-3 (host-only synth rates). | ledger §4 |

## 3. What is new since Alpha 2

**Sound effects** (A3-01 – A3-03, A3-05; `native/targets/tdeck/ALPHA3_AUDIO.md` §1–§16, §29)
- The T-Deck's I2S speaker, driven by a PC-speaker synthesizer that emulates the original's own primitives from their loop bodies.
- 62 of the 73 cue ids play: footsteps and bumps, dungeon and world cues, combat hits and the victory fanfare, the spell ceremony, the shrine and shard ladders, the quake rumble, Blackthorn's siren, the Refuge's thunder and slumber melody, the moongate, sceptre and Shadowlord sweeps, the harpsichord, the fountain, waterfall and grandfather clock. The 11 silent ids and every silent call site carry their reason (§16.18).
- No asset is needed: stock DOS files get every effect.

**Music** (A3-04, A3-04A; §17, §18)
- The Exodus Project *Ultima V Upgrade* 1.0's 16 XMI songs, played through a from-scratch OPL2 emulator, switched at the boundaries the patch driver itself uses (key poll, load, Ending, Camp).
- The songs come from the user's own patched files, through the optional SD audio pack (§5).

**Audio controls** (A3-01, A3-05; §11, §29)
- System Menu › Settings: **SFX Volume** and **Music Volume**, 0–100 % in 10 % steps, kept in `settings.json`. One square law for both channels; 0 % is exact silence and stops the synth. The curve is final.
- **`Alt+Shift+M`** / **`Alt+Shift+S`** mute and restore music / effects for the session. The configured volume is never written, a reboot starts unmuted, the Settings rows read `NN% (muted)`, and editing a row unmutes it.
- With stock files `Music Volume` reads `Unavailable` with the reason, and `Alt+Shift+M` answers `Music unavailable: …`.

**Performance and platform** (A3-04A – A3-04G; §18–§28)
- The music stutter: a FreeRTOS mutex taken on every synth table read (a function-local static), removed; the per-sample audio path runs from IRAM.
- SD diagnostic logging is **off by default**: the SD card shares the TFT's SPI bus, and the log's bursts caused 0.8–1.5 s stalls. A Developer switch turns it on for a session.
- The renderer yields instead of sleeping to the next tick, and an idle-service guard keeps core 0's task watchdog fed (A3-04E.1).
- A cheaper renderer: on hardware, composition 38.5 → 10.1 ms per frame, full-screen TFT max 148.8 → 120.1 ms, `idle0` gap max 102.9 → 37.6 ms (H-200).
- The System Menu opens in ≈ 180 ms instead of ≈ 820 ms, and keeps no save document afterwards (H-202).
- Developer › Diagnostics: the audio / render performance report, live stats, and the A/B probes, on the Developer screen.

**Gameplay and presentation parity** (A3-HF1 – A3-HF10, A3-05)

| Batch | The original's behaviour, now on the device |
|---|---|
| A3-HF1 (D-61) | The arena `Get` takes an item even when its counter is full. |
| A3-HF2 (D-62) | A grandfather clock strikes only when the game hour changes. |
| A3-HF3 (D-63) | Every combat hit draws the star on the struck cell and, for a party member, inverts its roster row for 174 ms. |
| A3-05 (D-64) | A kill plays one hit burst, not two. |
| A3-HF4 (D-65) | A successful load leaves no prompt, picker, view or pending question from the game it replaced. |
| A3-HF5 (D-66) | Conversations keep their script pauses and key waits (Chuckles' song, Blackthorn's speech). |
| A3-HF6 (D-40) | The shrine rite and the Codex wait for a key at each of the original's getkeys. |
| A3-HF7 (D-41) | "WELL DONE!" and "ALAKAZAM!" turn the map to its negative; the Codex ceremony flashes it three times. |
| A3-HF8 (D-43) | Blackthorn's sacrifice shows its burst on the victim's cell before the victim goes. |
| A3-HF9 (D-42) | The Refuge keeps the original's cadence, and Lord British's karma speech waits for a key. |
| A3-HF10 (D-6, D-70) | `M`ix: the player marks the reagents and answers **How much?**; a wrong set is spent and springs the trap. |

**Save / load reliability** (A3-HF4, A3-04G)
- A load always lands in plain play; a failed load changes nothing.
- The save shell releases each slot before reading the next and keeps nothing after a save, load or inspect; a Save made inside the menu updates its Load page.
- The save format is unchanged (below, §9).

## 4. Stock and music-patched DOS files

| The user's DOS files | Sound effects | Music | Settings shows |
|---|---|---|---|
| **Stock** (unpatched *Ultima V* DOS) | all of them, synthesized; no asset needed | **none** — the 1988 game has no music | `Music Volume: Unavailable`, *Stock DOS game files have no music* |
| **Supported patch** (Exodus *Ultima V Upgrade* 1.0) | the same | **the patch's 16 songs** | `Music Volume: 80%`, adjustable |
| Incomplete patch | the same | none, never guessed | `Unavailable`, *Music patch files are incomplete* |
| Unknown music variant | the same | none, never guessed | `Unavailable`, *Unsupported music patch variant* |
| No, stale or corrupt audio pack | the same | none | `Unavailable`, *No audio pack: npm run pack:audio* or *Audio pack stale or corrupt: rebuild* |

In every row the game is fully playable; none changes a save, the game packs or a gameplay rule. Detection reads the files, never their names (`ALPHA3_AUDIO.md` §6).

## 5. Files

| File | Size | Identity |
|---|---:|---|
| `native/targets/tdeck/build-a3-rc1-post/launcher/OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin` — **the Alpha 3 release image** | 988,320 B (`0xf14a0`; identical in size to A3-HF10's) | SHA-256 `4b5b9d1f5cb5c14fc2628da6c5ce2befb18f4c81daaa5bfd04821e1ee63353b1`; embedded `Git f85575b96f05` (the source commit `f85575b96f05bd7250ef41347f858bfc954e276b`, tag **`alpha3-rc1`**; release tag **`alpha3-release`**) |
| `/ultima5/openu5-alpha1-resources.bin` (SD) | 2,041,466 B | CRC32 `26f75ae6`, SHA-256 `a48abdbfc88eb5ab43453880a8ea029a1ea0684dfec2045f9dae941b31aa379b` — unchanged since Batch 53 |
| `/ultima5/openu5-assets.bin` (SD) | 132,284 B | CRC32 `933c9b82`, SHA-256 `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188` — unchanged |
| `/ultima5/openu5-audio.bin` (SD, **optional**) | 56,148 B patched / 112 B stock | OU5AUDIO 1.0, derived locally by `npm run pack:audio`, never committed; its CRCs are checked at boot |

The release image keeps the RC1 name, which follows `PROJECT_VER` through `package_launcher.py`: `3.0.0-alpha3-rc1-debug` → `OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin`. The `Git` it embeds is the `alpha3-rc1` commit, not the release tag's. On boot the identity screen shows:
- `FW 3.0.0-alpha3-rc1-debug`
- `Git f85575b96f05`
- `RES v2.0 2041466B CRC 26f75ae6`
- `ASSET … 132284B CRC 933c9b82`

The firmware refuses to start with any other resource pack. The image is a Debug build: it includes the Developer menu and verbose serial logging. SD diagnostic logging is off unless the Developer switch turns it on.

## 6. Installing or updating

- **Launcher allocation: the full 1 MiB (1,048,576 B).** The release image is 988,320 B; rounded up to 64 KiB that is 1,048,576 B. Alpha 2 needed 896 KiB, so a Launcher slot sized for Alpha 2 is too small. (The packager's `App partition minimum` line reads **1,048,576 B**.)
- **Firmware:** copy the Launcher image anywhere on the card, then install it from Launcher (**SD** → select the file).
- **Game packs:** unchanged since Batch 53. Build them only if you do not have them: `npm run extract`, `npm run pack:native`, `npm run pack:alpha1`, then copy both to `/ultima5/`.
- **Music (optional):** only with the Exodus *Ultima V Upgrade* 1.0 installed over your DOS files. Extract from that install, run `npm run pack:audio` (or `npm run pack:audio -- --source <dir> --output <file>`), and copy `native/assets/openu5-audio.bin` to `/ultima5/openu5-audio.bin`. Without it, or with stock files, the game is complete and silent of music.

**SD layout:**
```text
/
|-- OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin   (location not significant)
`-- ultima5/
    |-- openu5-assets.bin
    |-- openu5-alpha1-resources.bin
    |-- openu5-audio.bin      (optional; music capability)
    |-- settings.json         (created automatically)
    |-- saves/                (created automatically)
    `-- logs/                 (created automatically)
```

| Coming from | What to do |
|---|---|
| Alpha 2 (`Git 211c676a1dca`) or any Alpha 3 development image | Flash the Launcher image, in a 1 MiB slot. **No game-pack recopy.** Add `openu5-audio.bin` if you want music. Keep `saves/` and `settings.json`. |
| Batch 52 or earlier | As `ALPHA2.md` says: replace the resource pack with the 2,041,466 B one, then as above. |

## 7. Save compatibility

- Saves live in `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`. No Alpha 3 batch changed the save format: `persistence.cpp` is unchanged since `alpha2-batch55-release`, and A3-04G changed only how the shell inspects the slots. Alpha 2 saves load.
- `settings.json` keeps version 1. Alpha 2's already held both volume keys (80 / 80), so no migration is needed.
- A save made after the ending opens straight into the ending, as in Alpha 2.

## 8. Controls (quick reference)

| Input | Action |
|---|---|
| Trackball | move; navigate menus, pickers and targets |
| Shift + trackball up/down | page the transcript |
| Letters | Ultima commands (Talk, Look, Get, Klimb, Search, Mix, Cast, Use, Yell, Hole up, Z-stats, …) |
| Enter / Space / Backspace | confirm / pass a turn / back |
| `Enter: continue` on the status line | a key wait of the original's (conversation, shrine, Codex, Refuge): any key continues and does nothing else |
| Mix's **Reagents:** panel | Enter or Space mark / unmark, `M` mixes, Mic cancels; **How much?** takes a number, Mic erases it, Enter answers |
| Mic key, short press | Cancel |
| Mic key, hold about 1 s | toggle movement mode |
| `Alt+M` | System Menu (Settings: SFX Volume, Music Volume) |
| `Alt+S` / `Alt+L` | save / load (latest) |
| `Alt+Shift+M` / `Alt+Shift+S` | mute / restore music / sound effects (this session only) |
| `Alt+D` | Developer menu |

## 9. Validation

**Host.** At A3-HF10: **162 / 162**, serial, 152.76 s, in a fresh build; the one known w64devkit warning. Every Alpha 3 fix was proven RED-first on the unmodified tree and by mutation (A3-HF10: 54 / 71 RED, 28 / 28 mutations killed). TypeScript: `tsc --noEmit` clean; the whole `game/` vitest run has the same 97 pre-existing, environment-bound failures as its baseline (`native/core/a3-hf10-ts-base-fails.txt`), compared as a FAIL set. **RC1 tree (2026-09-28):** fresh `native/core/build-a3-rc1`, built in 1 m 48 s with the one known w64devkit warning; **162 / 162**, serial, **139.25 s**, 0 skipped. `tsc --noEmit` clean. The whole `game/` vitest run: 7,728 tests, 7,525 passed, **97 failed — the same 97** as `a3-hf10-ts-base-fails.txt`, compared as a FAIL set (`native/core/a3-rc1-ts-compare.log`). Eighteen suites fail to load (11 "Invalid or unexpected token", 7 `ENOENT` on git-ignored `original/` or `re/` data); the baseline file never listed suites, and `game/`, `re/` and `extractor/` are byte-identical to tag `alpha3-hf10-mix-parity`, so none is new.

**Release tree (closeout, 2026-09-28).** No source, test or build changed after RC1 (the diff against `alpha3-rc1`, excluding `*.md` and `*.log`, is empty). The existing `native/core/build-a3-rc1` was run again, serially and without a rebuild: **162 / 162, 134.68 s** (`native/core/a3-release-ctest.log`). No firmware was rebuilt.

**Hardware.** Every phase is in `native/targets/tdeck/ALPHA2_HARDWARE_CHECKLIST.md`.

| Check | Covers | Status |
|---|---|---|
| A3-01, A3-02 device checks | speaker, test tone, volumes, the first 22 effects, the harpsichord | **PASS** (2026-09-26) |
| A3-04E.1 soak and probes | watchdog, render pacing, music 80 % / 0 % | **PASS** (2026-09-27) |
| H-197 | arena Get with a full pack (A3-HF1) | **PASS** |
| H-198 | grandfather clock and fountain (A3-HF2) | **PASS** |
| H-200 | render correctness and speed (A3-04F) | **PASS** |
| H-201 | combat hit feedback (A3-HF3) | **PASS** |
| H-202 | System Menu inspection, save / load (A3-04G); heap watch item open | **PASS** |
| H-205 | conversation pauses (A3-HF5) | **PASS** |
| H-206 | shrine and Codex key waits (A3-HF6) | **PASS** |
| H-207 | ritual negative and Codex pulses (A3-HF7) | **PASS** |
| H-208 | Blackthorn sacrifice burst (A3-HF8) | **PASS** |
| **H-203** | mute shortcuts, Settings, the kill burst (A3-05) | **PASS** (A3-HF10 image, 2026-09-28) |
| **H-204** | a load leaves no prompt (A3-HF4) | **PASS** (A3-HF10 image, 2026-09-28) |
| **H-210** | Refuge cadence and key wait (A3-HF9) | **PASS** (A3-HF10 image, 2026-09-28) |
| **H-213** | Mix reagent picker and quantity (A3-HF10) | **PASS** (A3-HF10 image, 2026-09-28) |
| **Phase A3-RC1** | the RC smoke and the RC heap capture, on the RC1 image (`FW 3.0.0-alpha3-rc1-debug`, `Git f85575b96f05`) | **PASS** (2026-09-28); heap gate **PASSED** — A3-04H deferred |

H-199 (an optional serial check of one log line, A3-HF2.1) is withdrawn: not owed for Alpha 3.

**Phase A3-RC1 (2026-09-28): PASS.** On the exact release image: `FW 3.0.0-alpha3-rc1-debug`, `Git f85575b96f05`. The user ran the RC smoke and reported it PASS; the serial capture (`native/targets/tdeck/a3-rc1-hw-capture.log`, summarised by `a3-rc1-hw-summary.log`) shows:
- **Health:** watchdog / crash / reboot lines 0; storage error lines 0; error-level (`E`) lines 0.
- **Audio:** 71 / 71 `AUDIO_PERF` windows clean (`missed=0 underruns=0 hw_underruns=0 runaway=0 failures=0`); a 13-cue fight with a killing blow and a victory.
- **System Menu:** 13 opens, 180–190 ms from key to first frame (median 180), no slowdown; 15 cached `SAVE_INSPECT` at 82.4–83.1 ms with 0 B retained in every window; internal heap flat across the repeated opens.
- **Save / load:** a gameplay save, a load of it and a New Journey, no error.
- **Input:** queue `dropped=0`, `queued=836`, `consumed=836`. Three recoverable keyboard-controller read errors occurred (below); the recovery worked and no input was lost.
- **Heap gate: PASSED — A3-04H deferred.** No leak (§10, `ALPHA3_AUDIO.md` §37).
- **Capture caveat:** the PowerShell tee lost 7.4 s – 99.9 s of the boot, so the `IDENTITY` and `AUDIO_PACK` lines are not in the file; the identity above is the user's report of the device's identity screen.

**Keyboard recovery (evidence, not a defect).** The controller returned `ESP_ERR_INVALID_RESPONSE` on 3 of 10,248 reads, in two incidents (two reads in a row at 186.56 s, one at 195.36 s). `TDeckInput` logged `KEYBOARD_ERROR`, ran `KEYBOARD_RECOVER` to a usable baseline both times (`errors=3 recoveries=2`), and `INPUT_RESYNC` abandoned only the held gestures. No lock, no mode corruption, no lost or duplicated input. It is recorded as a successful recovery; it has no blocker ID.

## 10. Firmware and resources

| | A3-HF10 (code baseline) | RC1 = the Alpha 3 release image |
|---|---|---|
| Version | `3.0.0-alpha3-dev-a3-hf10-debug` | `3.0.0-alpha3-rc1-debug` |
| Image | 988,320 B (`0xf14a0`), SHA-256 `c14d96ff7b3b96983a2823cf54db69f094b84fa0837e6afed17b20ba1f5b8a79`, `Git 028269fec0c5` | 988,320 B (`0xf14a0`), **+0 B** vs A3-HF10, SHA-256 `4b5b9d1f5cb5c14fc2628da6c5ce2befb18f4c81daaa5bfd04821e1ee63353b1`, `Git f85575b96f05` (post-commit image; tag `alpha3-rc1`) |
| App partition free (1 MiB) | 60,256 B (5.7 %) | 60,256 B (5.7 %) |
| Launcher allocation | 1,048,576 B | 1,048,576 B (the whole 1 MiB partition) |
| Flash `.text` / `.rodata` | 669,770 B / 219,460 B | 669,770 B / 219,460 B (diff 0; the per-object-file diff is empty) |
| Internal RAM (DIRAM `.data` / `.bss`) | 21,627 B / 51,968 B — unchanged by A3-HF9 and A3-HF10 | 21,627 B / 51,968 B (unchanged) |
| IRAM | 16,384 B, full; the audio hot path is checked by `a3_04b_iram_check.py` | 16,384 B, full (unchanged) |
| PSRAM | unchanged by A3-HF9 and A3-HF10 | unchanged (no static PSRAM section moved; the section diff is empty) |
| Image guards | `a3_04f_image_check.py`, `a3_04b_iram_check.py`, `a3_04a_hotpath_check.py`: GREEN | GREEN, all three (pre-commit and post-commit images) |

**Heap.** The RC1 heap capture found no leak (`native/targets/tdeck/a3-rc1-heap-adjudication.log`, `ALPHA3_AUDIO.md` §37):
- §28.16 trigger 2 fired once, on a gameplay save (`196,419 → 144,283`, −52,136 B). `capture_save_document()` rewrites the live save document in place, and the document stays alive after a save by design; a later load or New Journey replaces it. Internal + PSRAM returned to within 4,664 B of the first heartbeat.
- Trigger 3 fired (largest internal block 30,720 B): above the 23,552 B floor accepted at A3-04G, and the DMA headroom was restored in every window. Trigger 5 did not fire (the 376 B and 168 B minima sit inside storage windows). Trigger 6 did not fire (`.data`, `.bss`, IRAM equal to A3-HF10). Trigger 7 is this capture. Workspace and inspect windows retained 0 B.

**Size tripwires.**
- The image must stay ≤ 1,048,576 B: the partition is 1 MiB, and the Launcher allocation is already the whole partition.
- An RC changes only `PROJECT_VER` and the embedded `Git`. Its image must match A3-HF10's size to within the longer version string. The Alpha 2 precedent: 98 bytes differed between the Batch 53A image and RC1. A larger difference means the RC carries code, and it stops.
- `ALPHA3_AUDIO.md` §28.16 trigger 6: no batch adds ≥ 1 KiB of internal `.data` / `.bss` or IRAM without an offset.

## 11. Known differences

The deferred items of §2 are the known differences. These points are not bugs:
- **Music exists only with the patch.** The 1988 game has none; stock files stay silent of music by design.
- **SFX Volume is an output gain, not the 1988 sound flag.** At 0 % the game keeps its sound-ON branches (the blindfold drags) and the sound-OFF lute.
- **Audio never holds the game** (the A3-01 rule, D-60): the victory fanfare plays while play continues. The original's silent waits are kept silently.
- **Stepping back onto the gate you arrived by keeps you there.** That is the original's behaviour.
- **Debug logging is kept on purpose**, on serial. SD logging is off until the Developer switch turns it on.
- **Free internal RAM moves with the save document.** A save rewrites the live document in place, so free internal RAM can change by tens of KB across a save and comes back when a load or New Journey replaces the document. The RC heap capture found no leak (`ALPHA3_AUDIO.md` §37); the placement / fragmentation watch stays a known trait, and its remedy, A3-04H, is post-Alpha-3 work.

Every knowing difference from the original is in [`native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md`](native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md). The original's own defects, which the port keeps, are in [`docs/bugs-del-original.md`](docs/bugs-del-original.md).

**Not reopened for Alpha 3** (decisions, recorded so they are not re-litigated):
- Audio is closed. Music is closed. The volume curve is final.
- A3-04 performance work is closed. A3-04H is deferred: the RC heap capture did not call for it.
- UI redesign is deferred. The full D-54 ending cinematic is deferred.
- H-209, H-211, H-212 and D-71 are deferred.
- Mix gameplay behaviour is fixed (A3-HF10).
- No additional gameplay parity audit is required before Alpha 3.

## 12. Release verification checklist (the RC1 batch)

A mechanical batch: no code, test or behaviour change. Stop at the first step that does not hold.

1. **Hardware gate.** H-210, H-213, H-203 and H-204 are recorded **PASS** in `ALPHA2_HARDWARE_CHECKLIST.md`, each with the `FW` / `Git` it ran on. A FAIL is a hotfix batch, not an RC.
2. **Clean tree.** `git status` clean on `main` (the pre-RC reconciliation committed, the hardware results committed). Record HEAD and the latest tag.
3. **Version.** `native/targets/tdeck/CMakeLists.txt`: `set(PROJECT_VER "3.0.0-alpha3-rc1-debug")`, with its comment updated the way Batch 54's was. It is the only non-documentation change. Confirm the name: `package_launcher.py` maps it to `OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin`.
4. **Fresh host build.** A new directory, `native/core/build-a3-rc1`: `-G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENU5_ENABLE_DEVELOPER_TOOLS=ON -DNODE_EXECUTABLE=…`. The only warning allowed is the known w64devkit `-Wstringop-overflow`.
5. **Serial host suite.** `ctest` without `-j`: **162 / 162**. Nothing else runs on the machine meanwhile (no ESP-IDF build). Keep the log.
6. **TypeScript.** `tsc --noEmit` clean. The whole `game/` vitest run: its FAIL set equals `native/core/a3-hf10-ts-base-fails.txt` (97). Compare the set, never the total.
7. **Firmware build.** From an ESP-IDF 6.1 shell (`export.ps1`), a fresh directory, `--no-ccache`: `idf.py --no-ccache -B build-a3-rc1 reconfigure`, then `ninja -C build-a3-rc1 -j 4 all`. Zero project warnings.
8. **Size and resources.** `python -m esp_idf_size build-a3-rc1/openu5_tdeck.map`, with a `--diff` against the A3-HF10 map. Every section is equal to A3-HF10's except the bytes of the version string (§10's tripwires); `.data`, `.bss`, IRAM and PSRAM are unchanged. Do not run `idf.py size` on a `--no-ccache` directory, and never point `idf.py` at an old build directory.
9. **Image guards.** `a3_04f_image_check.py`, `a3_04b_iram_check.py`, `a3_04a_hotpath_check.py` on the new ELF: GREEN.
10. **Commit.** One commit: the version bump, the evidence logs, and the documents with everything that is known before the image exists (this file's status "RC1 READY — HARDWARE SMOKE PENDING", the checklist's Phase A3-RC1 made concrete, the audit's and ledger's RC status, `LAUNCHER.md`'s RC1 row, `README.md`). As in Batch 54, the Files table says "SHA-256 and embedded `Git`: annotated tag".
11. **Rebuild from the commit.** The firmware embeds `git rev-parse --short=12 HEAD` at configure time, so build again in a fresh directory (`build-a3-rc1-post`), `reconfigure` first. Rerun steps 8 and 9 on it.
12. **Launcher packaging.** `python package_launcher.py --build-dir build-a3-rc1-post`. It validates the header, the segment checksum and the appended SHA-256. Record `App partition minimum`.
13. **Embedded version.** The packager's `Firmware version` line reads `3.0.0-alpha3-rc1-debug`. The identity screen will show `FW 3.0.0-alpha3-rc1-debug`.
14. **Embedded Git.** The image's `Git` equals the step-10 commit's `git rev-parse --short=12 HEAD`.
15. **SHA-256.** Record it from the packager, and check it with a second hash of the Launcher file.
16. **Annotated tag.** Tag the step-10 commit, for example `alpha3-rc1`, following `alpha2-batch54-rc1`: the image path, size, free bytes, Launcher allocation, SHA-256, `Git`, `FW`, the packs (unchanged), the host totals, and "Next: Phase A3-RC1". The documents' "(RC1: fill in)" fields are answered by the tag. A post-commit logs commit follows if the evidence logs need one, as every Alpha 3 batch did.
17. **No push, no flash** unless the user asks.
18. **Physical RC validation:** Phase A3-RC1 in `ALPHA2_HARDWARE_CHECKLIST.md`, on this exact image (check `FW` and `Git` first).
19. **RC heap capture** (`ALPHA3_AUDIO.md` §28.16 trigger 7): the serial capture from power-on, taken in the same Phase A3-RC1 session, summarised with `native/core/tools/a3_04g_hw_closeout.py`. A trigger fired by placement only (internal + PSRAM sum steady) is recorded and does not block. A trigger it cannot explain opens A3-04H before the release.

**Release, after Phase A3-RC1 passes:** promote the RC1 image byte for byte, as Batch 55 did. The release commit changes documents and logs only; it gets its own annotated tag, and no new image is built.

**Release closeout (done, 2026-09-28).** Phase A3-RC1 passed and the heap gate passed, so the RC1 image was promoted unchanged: the file's SHA-256 (`4b5b9d1f5cb5c14fc2628da6c5ce2befb18f4c81daaa5bfd04821e1ee63353b1`), size (988,320 B), `FW` and embedded `Git` were re-read from the file, not from a build. The closeout commit adds the evidence (`a3-rc1-hw-capture.log`, `a3-rc1-hw-summary.log`, `a3-rc1-heap-adjudication.log`, `native/core/a3-release-ctest.log`) and updates documents only. Its annotated tag is `alpha3-release`, on the closeout commit, the way `alpha2-batch55-release` sits on Batch 55's; `alpha3-rc1` stays on the source commit.
