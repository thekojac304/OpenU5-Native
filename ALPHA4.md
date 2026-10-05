# Ultima V Native — T-Deck Plus — Alpha 4 (released; RC5 is the release image)

**Status: ALPHA 4 RELEASED (2026-10-04).** Alpha 4 is release-ready, with **RC5 as the hardware-validated firmware image**. No release blockers. Nothing is pushed; no GitHub Release has been created.

| | |
|---|---|
| **Firmware** | `OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` — `FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399` (`59c14b907399be8e8796dcd84946a60c942731c1`, no `-dirty`) |
| **Size / free** | 1,047,408 B (`0xffb70`) / 263,312 B (20.1 %) free in the 1.25 MiB app partition; Launcher allocation 1,024 KiB (the next 64 KiB step is close — informational only) |
| **SHA-256** | `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210` (re-hashed at closeout, unchanged) |
| **Release tag** | `alpha4-release` (annotated) on the **final release-record commit** — a documentation-only commit after `e664c2d3` |
| **Firmware provenance** | the image was built from `59c14b907399`, **not** from the release-tag commit. The release commit changes only documents and logs, so it is code-identical to `59c14b90`; the image was **not** rebuilt (a rebuild would be a new, untested file). Flash the `.bin` above; its identity screen reads `4.0.0-alpha4-rc5-debug` / `59c14b907399`, by design |
| **Host suite** | 201 / 201, serial (148.2 s) on the release tree; the 39-test parity / preservation / golden / release-blocker subset 39 / 39 |
| **Hardware** | RC5 identity, the In Mani Corp scroll on a living target and the healer's `R` resurrection — **PASS** (user-reported, 2026-10-04); everything earlier in §2 below |

Readable release notes for GitHub: [`ALPHA4_RELEASE_NOTES.md`](ALPHA4_RELEASE_NOTES.md). The engineering record is `native/targets/tdeck/ALPHA4_UI.md` (§18 is the release record). The sections after this block were written as the candidates progressed; where they say "owed", "pending" or "candidate", **§2 below and `ALPHA4_UI.md` §18 supersede them (2026-10-04)**.

## Release history before the release (superseded 2026-10-04 — kept as written)
**RC5 hotfix (2026-10-04):** the user's RC4 hardware pass — R4-1, R4-2 (New Journey skiff + four bodies, D-82), R4-3 (save / power-cycle / Continue) and R4-6 (combat clock, D-88) **PASS**; R4-7 (naval cactus) not tested — found two failures (a written report, no serial evidence). **R4-5, the healer's `R` did nothing:** fixed — `UiSession`'s shop key map sent `Rest` for `r` everywhere but a tavern, so `ShopAction::Resurrect` could never come from a key; one line, pinned by a real-runtime test (`ALPHA4_UI.md` §17.2). **R4-4, the In Mani Corp scroll on a living target showed only `Failed!`:** investigated and **not reproduced** — every device route (overworld, towns, dungeon, paced, audio mounted, the use-then-cancel order) reads `Scroll` / `Resurrection!` / `Not dead!` / `Failed!`, and the RC4 ELF contains the code; the *spell* form reads exactly `Failed!`; no production change, route pinned (§17.3).
- **Candidate image (supersedes RC4):** `OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin`, `FW 4.0.0-alpha4-rc5-debug` — 1,047,408 B (`0xffb70`), SHA-256 `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210`, embedded `Git 59c14b907399` (no `-dirty`); 263,312 B (20.1 %) free in the 1.25 MiB app partition; flash `.text` −4 B vs RC4; host suite 201 / 201; guards GREEN; 19 / 19 mutants killed. RC4's artifact (`49e3962f…4414`) and record are preserved.
- **Owed:** the three-check retest only (`ALPHA4_UI.md` §17.7): identity, the In Mani Corp scroll on a living member, the healer's Resurrect. Not tagged or pushed.


**A4-PARITY2 (2026-10-04):** a parity-only cleanup after Alpha 4 (`native/targets/tdeck/ALPHA4_UI.md` §16). Fixed in the reference first and native second, each with RED-first tests and mutation proof: **D-89** (a naval OUCH hurts the whole party), **D-83 / D-84** (the healer and the Refuge run the shared resurrection routine; **changes Original on purpose**), **D-85** (the Stonegate trapdoor loop is bounded by the party size), **D-86** (an invalid dungeon digit passes no world turn), **D-87** (crops and plates cap food at 9999), **D-88** (combat advances the clock one minute per ten unit activations; **changes Original on purpose**), **D-82** (a New Journey seeds the underworld skiff and four bodies from INIT.OOL) and the In Mani Corp living-target text. Fourteen further divergences (D-90 … D-103) are recorded and **not** fixed. Host suite 200 / 200, `tsc` clean, the TypeScript FAIL set identical to the baseline, 110 / 110 mutants killed.
- **Candidate image (supersedes RC3):** `OpenU5-TDeck-Alpha4.0.0-alpha4-RC4-Debug-Launcher.bin`, `FW 4.0.0-alpha4-rc4-debug` — 1,047,408 B (`0xffb70`), SHA-256 `49e3962f367cb85650ae548463fefbf0ef2f4bbaf5d567679422a0d141674414`, embedded `Git 859f1605b974`; 263,312 B (20.1 %) free in the 1.25 MiB app partition; Launcher allocation 1,024 KiB (the next 64 KiB step is 1,169 B away). RC3 plus the fixes above; the SD pack is unchanged.
- **Not hardware-tested.** The minimal RC4 checklist is `ALPHA4_UI.md` §16.16; still owed from earlier candidates: the RC3 music retest (§15.10) and the Alpha 4 RC heap capture. Nothing is tagged or pushed.

**RC3 hotfix (2026-10-03):** the RC2 hardware session found that a dungeon played the surface's music (every way in, not only Developer teleport). Fixed in one block of the music selector; host-verified (192 / 192); **not yet hardware-tested**.
- **Candidate image:** `OpenU5-TDeck-Alpha4.0.0-alpha4-RC3-Debug-Launcher.bin`, `FW 4.0.0-alpha4-rc3-debug` — 1,046,288 B (`0xff710`), SHA-256 `9fd7fc8b5669c206efdc43d9588fe845c9c63f233c74e54c4e89f15803c9d542`, embedded `Git 6b6ab8a4c378` (no `-dirty`); 264,432 B (20.2 %) free in the 1.25 MiB app partition. RC2 plus that one fix (`ALPHA4_UI.md` §15).
- **Still owed:** the §15.10 retest on this image, and the Alpha 4 RC heap capture (none exists). User-reported on RC2 (not itemised, nothing captured): SAVE2 / UI3 slots, SAVE3 import / export, power-cycle Continue and smoke acceptable; A4-PARITY1 **PASS** — D-81, D-79, D-50 and D-78 (D-78: the cast executed and was not absorbed; the literal `Success!` text was not observed on a full-health target). Not RC-ready until the two items above.
- The RC2 text below is kept as written; RC2 (`608eb700…1e95`) is **superseded by RC3**.

**Status: ALPHA 4 RC2 BUILT — NOT YET RC-READY: ONE REQUIRED HARDWARE SESSION OWED (2026-10-02).** Software, preservation and build are complete; known parity divergences are tracked separately and do not gate it. Not a release.
- **Candidate image:** `OpenU5-TDeck-Alpha4.0.0-alpha4-RC2-Debug-Launcher.bin`, `FW 4.0.0-alpha4-rc2-debug` — 1,046,240 B (`0xff6e0`), SHA-256 `608eb7005dcc88cab6ed8c020e3ad7050c9a711406650492407c7bb760801e95`, embedded `Git 24c666e1ba13`; 264,480 B (20.2 %) free in the 1.25 MiB app partition; Launcher allocation 1,024 KiB.
- **What it is:** A4-POLISH3's firmware with a new version line. Everything Alpha 4 set out to do is implemented, host-verified and committed (`native/targets/tdeck/ALPHA4_UI.md` §14.4).
- **What is still owed:** one hardware session on this image (`ALPHA4_UI.md` §14.7): A4-PARITY1's gameplay fixes, the save-slot pages (A4-SAVE2 / A4-UI3), the PC save import / export on the device (A4-SAVE3) and the RC heap capture. Those were never run on a T-Deck. If they pass, the project convention (Alpha 2, Alpha 3) is to promote this exact image unchanged.
- **RC1** (`aae348ac`, `FW 4.0.0-alpha4-rc1-debug`, SHA-256 `67100a51…145a`) is superseded: its session never ran, and RC2 carries everything since.
- Alpha 3 (tag `alpha3-release`, [`ALPHA3.md`](ALPHA3.md)) was the current release *(superseded 2026-10-04: Alpha 4 is now the current release)*.

## 1. What Alpha 4 is

Alpha 3's complete game with a finished front end, real save management, the original ending, a parity sweep, and optional conveniences that leave the 1988 rules alone unless chosen:
- **Presentation (A4-UI1, UI2, UI4/PRES1):** the "More Ultima V" chrome in the original's EGA palette and IBM.CH font; the title's credits; the console with bullets, turn rows and the wave cursor; reverse-video lists; the moongate transit; the original's console echoes.
- **Saves (A4-SAVE1, SAVE2, UI3):** three manual slots, each a two-generation pair that never overwrites the generation Continue restored; two-row slot pages with CURRENT / LATEST / RECOVERED.
- **PC save transfer (A4-SAVE3):** import a DOS `SAVED.GAM` + `SAVED.OOL` into a slot, export a slot back (title → **P**).
- **The ending (A4-END1):** ENDGAME.OVL played in full: the throne room, the box questions, the dissolve, the six story pages, the final scroll.
- **Parity (A4-PARITY1):** the palace crown gate, the worn crown as time spell 0x1c, Negate against enemy magic, the dungeon's command keys, the well's wish.
- **Enhancements (A4-ENH1, ENH2, POLISH3):** the trackball click toggles WASD Mode; trackball speed levels; System Menu **Difficulty** (Original / Relaxed / Easy / Custom) and **Cheats** (Party / Inventory / World); Alt+D opens the Developer menu (no longer a menu row); a Keyboard Backlight setting.

**Original stays original.** With Difficulty **Original**, no cheat and no World toggle — the default for every new journey and every older save — the game is the recreated 1988 game: goldens recorded before each enhancement reproduce bit for bit, and the parity corpus is unchanged (`ALPHA4_UI.md` §10.9, §11.10).

## 2. Hardware validation so far

| Track | Device result |
|---|---|
| A4-UI2, A4-SAVE1, A4-END1, A4-UI4/PRES1 | **PASS** (`ALPHA4_UI.md` §2.8, §3.6, §7.19, §8.22.8) |
| A4-ENH1 (trackball), A4-POLISH3 (keyboard light) | **PASS** (the user, 2026-10-02, §14.2) |
| A4-ENH2 | cheats **PASS**; difficulty rules **accepted as implemented**, balance adjustable later (§14.2) |
| A4-PARITY1, A4-SAVE2, A4-UI3, A4-SAVE3, the heap capture | **not yet run** — the RC2 session (§14.7) |
| A4-SAVE3's real-DOS round trip | optional; without it the PC bridge is **host-validated only** |

**Superseding note (2026-10-04, the release closeout).** The "not yet run" row above is closed by the user's reports, recorded in full in `ALPHA4_UI.md` §18: **SAVE2, UI3, SAVE3 (device import / export), power-cycle Continue and ordinary smoke — user-reported PASS; PARITY1 — PASS** (D-78: the cast was not `Absorbed!`; the literal `Success!` was not seen on a full-health target; D-79, D-81, D-50 PASS); **RC3 dungeon-music retest — PASS** (no serial capture); **RC4** — R4-1, R4-2, R4-3, R4-6 PASS, R4-4 and R4-5 FAIL as originally reported, R4-7 not tested; **RC5** — identity, the In Mani Corp scroll on a living target and the healer's `R` **PASS**. **The RC heap capture was not run; the user waived it for the Alpha 4 release; no heap defect was observed; no heap numbers exist and none are claimed.** It is not a release blocker. A4-SAVE3's real-DOS round trip stays optional and unrun: the PC bridge is validated on the host and on the device's import / export, not against a DOS install.

## 3. Known differences carried forward

Documented, non-blocking, and not Alpha 4 work (`ALPHA4_UI.md` §14.5, ledger §4): the healer and the Refuge skip the 1988 resurrection penalty (D-83, D-84); the location-29 trapdoor takes the whole roster (D-85); a dungeon digit key on an invalid member passes a turn (D-86); crops can push food past 9,999 (D-87); combat does not advance the clock (D-88); the naval OUCH's damage routine is to be adjudicated (D-89); a new game lacks INIT.OOL's underworld skiff and bodies (D-82); and the older minor rows. Deferred by decision: a softer death penalty (waits on D-83 / D-84), Advanced Cheats, the credits curtain, story plates and title figures, WASD in the frontend menus (D-1).
**Superseding note (2026-10-04).** D-82 – D-89 above were **fixed in A4-PARITY2** (and shipped in RC4 / RC5), so the list is historical. The healer and the Refuge now run the shared resurrection routine (D-83, D-84), the trapdoor loop is bounded (D-85), an invalid dungeon digit passes no turn (D-86), food caps at 9,999 (D-87), combat advances the clock (D-88), the naval OUCH hurts the whole party (D-89) and a New Journey seeds the underworld skiff and bodies (D-82). Open and **not** Alpha 4 work: D-90 – D-103 (recorded in the ledger, none fixed; none is marked release-critical) and the older minor rows.

## 4. Installing or updating

- **Firmware:** copy the Launcher image to the card and install it from Launcher (**SD** → the file). Launcher sizes its own partition from the image (rounded up to 64 KiB, §5); the ESP-IDF partition table is not used (`native/targets/tdeck/LAUNCHER.md`).
- **Game packs — changed since Alpha 3.** Alpha 4 needs the A4-END1 resource pack: `/ultima5/openu5-alpha1-resources.bin`, **2,266,819 B**, CRC32 `5c0d175d`, SHA-256 `85b38994eea674e48338d744091c6b895b9507a286bb38bcc84231131feed01e` (`npm run pack:alpha1`). The tiles pack (`openu5-assets.bin`, 132,284 B, CRC32 `933c9b82`) and the optional audio pack are unchanged. The firmware refuses any other resource pack.
- **Back up `ultima5/`** (saves and settings) before the first boot of a new image.

| Coming from | What to do |
|---|---|
| Any Alpha 4 image from A4-END1 on | Flash only; no pack recopy. |
| Alpha 3, or an Alpha 4 image before A4-END1 | Flash, and copy the 2,266,819 B resource pack. |

## 5. Files

| File | Size | Identity |
|---|---:|---|
| `native/targets/tdeck/build-a4-rc2/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC2-Debug-Launcher.bin` — **the candidate** | 1,046,240 B (`0xff6e0`) | SHA-256 `608eb7005dcc88cab6ed8c020e3ad7050c9a711406650492407c7bb760801e95`; `FW 4.0.0-alpha4-rc2-debug`, `Git 24c666e1ba13` (the commit `24c666e1ba13d455b367878c0b594438f809071f`) |
| `native/targets/tdeck/build-a4-rc5/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` — **the Alpha 4 release image (2026-10-04; supersedes the RC2 row above)** | 1,047,408 B (`0xffb70`) | SHA-256 `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210`; `FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399` (the commit `59c14b907399be8e8796dcd84946a60c942731c1`); 263,312 B free |
| `/ultima5/openu5-alpha1-resources.bin` | 2,266,819 B | CRC32 `5c0d175d`, SHA-256 `85b38994…d01e` |
| `/ultima5/openu5-assets.bin` | 132,284 B | CRC32 `933c9b82`, SHA-256 `6eb001ed…e188` |
| `/ultima5/openu5-audio.bin` (optional) | 56,148 B patched | unchanged since Alpha 3 |

The image is a Debug build with the Developer menu (Alt+D), by decision (`ALPHA4_UI.md` §9.2).

## 6. Saves and settings

- **Slots:** Slot 1 is the pre-Alpha-4 pair `alpha1-g{0,1}.*` (no migration; Alpha 3 saves load as Slot 1); Slots 2 and 3 are `alpha1-s{2,3}-g{0,1}.*`. Continue loads the slot saved last; Alt+S / Alt+L use the current slot.
- **Enhanced state** (difficulty, Custom values, God Mode, World toggles, the cheats-used mark) is kept in the save's sidecar only when it differs from the defaults; an older save is Original with no cheat. A PC export never carries it.
- **`settings.json`** stays version 1 and gains two optional keys: `trackballSpeed` (1..10, default 5) and `keyboardBacklight` (0..4, default Off). An older file loads.
- **PC transfer:** import reads `/ultima5/import/SAVED.{GAM,OOL}` (never written); export writes `/ultima5/export/slotN/`. Town NPC tables and dungeon saves are not bridged (`ALPHA4_UI.md` §5.9).

## 7. Controls added in Alpha 4

| Input | Action |
|---|---|
| Trackball click | toggle Movement (WASD) Mode at once |
| Settings → Trackball speed 1–10 / Keyboard Backlight Off–Max | device preferences, saved in `settings.json` |
| `Alt+M` → Difficulty / Cheats | the optional rules and conveniences |
| `Alt+D` | Developer menu (in the game and on the title screens; no longer a menu row) |
| Title → **P** | PC Save Transfer |
| Keyboard `Alt+B` | the keyboard's own light toggle (handled by the keyboard; the Settings level is re-applied at boot and on change) |

## 8. Validation

- **Host:** **191 / 191**, serial, 164.44 s, in a fresh build (`native/core/a4-close1-host-ctest.log`); the preservation goldens, screen goldens, parity corpora and drift tests 54 / 54 on their own. Every Alpha 4 batch was proven RED-first and by mutation (`ALPHA4_UI.md`, each batch's mutation section).
- **Firmware guards:** `a3_04a_hotpath_check`, `a3_04b_iram_check`, `a3_04f_image_check` GREEN; `check_app_budget` OK; every memory section equal to the A4-POLISH3 image the user ran (`ALPHA4_UI.md` §14.9).
- **Hardware:** §2 above; the remaining session is `ALPHA4_UI.md` §14.7.

- **Release verification (2026-10-04):** host suite **201 / 201**, serial, 148.2 s, on the release tree (`native/core/build-a4-parity2-final`, no rebuild needed: `ninja: no work to do`); the parity / preservation / golden / release-blocker subset (`ctest -R "parity|preserv|golden|release_blockers|rc5"`) **39 / 39**; the RC5 image re-hashed to `363a8fda…7210` and its strings `4.0.0-alpha4-rc5-debug` and `59c14b907399` present once each, no `-dirty`. Hardware: §2 above and `ALPHA4_UI.md` §18. Not run, by decision: the RC heap capture (waived).
