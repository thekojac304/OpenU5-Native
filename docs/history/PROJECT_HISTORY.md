# Ultima V Native — Project History

*From the first commit to the Alpha 3 release, reconstructed from the repository itself.*

> **Status of this document.** Historical project record. It covers the repository from its first commit (`1225f9ac`, 2026-08-25) through the Alpha 3 closeout commit `eb1bf5e7` (tag `alpha3-release`, 2026-09-28). It is a historical document: nothing here changes code, tests, versions or tags, and no defect listed as deferred is reopened by being described.

## How to read this document

**Evidence.** Git history is the authoritative ordering. Everything else is used for what happened *inside* the commits: the 90 annotated tags (most carry the image size, SHA-256, host-suite total and hardware status), `ALPHA1.md` … `ALPHA3.md`, the committed evidence logs, `re/notes/`, and four large working documents abbreviated here as **AUDIT** (`GAMEPLAY_INTEGRATION_AUDIT.md`), **LEDGER** (`ALPHA2_PRESERVATION_LEDGER.md`), **CHECKLIST** (`ALPHA2_HARDWARE_CHECKLIST.md`) and **AUDIO** (`native/targets/tdeck/ALPHA3_AUDIO.md`).

**Before trusting any number here.**

- *Four kinds of number.* A **recorded** value is stated by a tag, commit body or document. A **measured** value was read directly for this history (a file size, a SHA-256, a `git diff`, a count of `add_test` lines). A **computed** value is arithmetic on recorded values. An **unresolved** discrepancy is one the repository cannot settle. Measured and computed values are labelled; discrepancies are listed in Appendix B.
- *Dates* are commit author dates. The three upstream commits are stamped +0200, everything after −0400. Where a dated status line in a document contradicts git, git wins (Appendix B).
- *Test counts are not one series.* Native CTest, the imported upstream project's counts and the TypeScript `vitest` run are kept apart (§16).
- *"Hardware PASS" usually means "the user reported it".* From A3-04E.1 on there is often a serial capture; there is rarely a wall-clock time. Where the image the tester ran is known it is named.
- *Who wrote the code.* The committer on 149 of the 152 commits is `thekojac304`; the other three are the upstream imports. Trailers credit Claude models as co-authors on 112 commits (Claude Sonnet 5, Opus 5, Opus 5.5, Sonnet 5.5). The forty without a trailer are the three imports, `2485950e` and the eight native commits through `c18f5b64`, the eight audit and early-batch commits through `5e1b14d1`, and the twenty Batch 30–49 commits. The repository says nothing more about the division of labour.
- *Alpha 2 and Alpha 3.* "Alpha 2.0" names the pre-audit development builds of §5.3; "Alpha 2" as a release is the state tagged `alpha2-batch55-release`. Batch numbers (1–55) belong to the Alpha 2 track. "Alpha 3" is the A3-nn sub-phases and A3-HFn hotfixes through `alpha3-release`.

## 1. The project in one page

Ultima V Native (formerly published as OpenU5-Native) is a native C++ implementation of *Ultima V: Warriors of Destiny* (Origin Systems, 1988), built to run on the LilyGO **T-Deck Plus** (ESP32-S3, 16 MiB flash, 8 MiB octal PSRAM, a 320×240 ST7789 panel, a physical keyboard and a trackball). It reads the user's own copy of the original game data from an SD card; the repository and the firmware contain no game data.

The repository sits on top of an imported upstream project, a TypeScript/PixiJS browser port whose method was to disassemble `ULTIMA.EXE` and its 24 overlays and cite the assembly for every rule. The native project inherited that method (and 516 reverse-engineering notes) and turned it on what the browser port had not needed: what the *device* runs, how long the original *waits*, and what it *sounds* like. The binary decides: the TypeScript reference is a cross-check (`ALPHA3.md` §1), and when native, the reference and the binary disagree, native — or the reference — is corrected, never the binary.

In the 34 days between the first commit and the last, the repository went through these phases (commit dates):

| Phase | Dates | What it was |
|---|---|---|
| Upstream import | 08-25 → 08-26 | Three commits importing a public browser port of Ultima V. |
| Web-side "Enhanced" work | 09-12 → 09-14 | Two commits on the imported web code by the later committer. No native code (§3.2). |
| Bring-up and Alpha 1.x | 09-14 → 09-18 | From `11e32392` (09-14 23:12), the first native commit: display, SD, assets, renderer, core, UI session, Developer menu, in a handful of very large commits. |
| The audit and Alpha 2 batches 1–19 | 09-18 → 09-21 | A whole-project audit finds the device glue layer untested; batches repair it. |
| Alpha 2 hardware era, batches 20–49 | 09-22 → 09-24 | First consolidated hardware pass; save/reload repairs; a long Camp/bed/sleep parity chain. (Batch 50, 09-25, was cancelled without a commit.) |
| Alpha 2 release | 09-25 → 09-26 | A readiness audit says NOT READY; four blockers are bound; RC1 is hardware-smoked and promoted byte for byte. |
| Alpha 3 audio and performance | 09-26 → 09-27 | Sound effects, music from a community patch, and seven performance sub-batches. |
| Alpha 3 hotfixes and closeout | 09-26 → 09-28 | Ten parity hotfixes found on hardware, a pre-RC reconciliation, RC1, release. |

Alpha 2 shipped on 2026-09-26 (`alpha2-batch55-release`), Alpha 3 two days later (`alpha3-release`). Of the 152 commits, 140 — and all 90 tags (64 `alpha2-*`, 26 `alpha3-*`) — fall in the eleven days from 2026-09-18 to 2026-09-28 (measured).

## 2. Milestone timeline

Host counts are native CTest totals unless marked. "Firmware" is the Launcher image size as the tag or document states it. "Hardware" is what the record says was physically observed.

| Date | Milestone / tag | Key result | Host tests | Firmware | Hardware status |
|---|---|---|---|---|---|
| 08-25 | `1225f9ac` upstream public release | Browser port imported: 2,183 files | upstream README claims 239 parity tests | — | — |
| 08-26 | `ad1a9dc1`, `291736a4` upstream syncs | README claim rises to 276; commit bodies quote 5,080 and 4,965 upstream unit tests | (upstream) | — | — |
| 09-12, 09-14 | `0315510f`, `2485950e` web-side commits | Browser "Enhanced" mode, OPL audio lane; no native files | (TypeScript only) | — | — |
| 09-14 | `11e32392` Milestone 2 — **first native commit** | T-Deck display + microSD bring-up | — | 298,064 B | M1 "tested by the user" per the doc; no M1 code in git |
| 09-14/15 | `5b4a8e91`, `1a593e0a` Milestones 3, 4 | Native asset pack; static renderer | 3 extractor tests (pack) | — | display/SD verified by the user (doc); colours "await visual verification" |
| 09-15 | `8a603f8f`, `fd2a2036` | Portable core, movement/input, combat, dialogue, dungeon, magic, persistence | 14/14 → 35/35 (`VALIDATION.md`) | M5.1: 339,728 B package | M5: "unverified"; M5.1 physical logs found a stack overflow |
| 09-18 | `c18f5b64` "Alpha 2.0 implementation" | UI session, frontend, Developer menu, save service; Alpha 1.x/2.0 docs land | 49/49 (docs) / 61 `add_test` lines | Alpha 2.0 docs: 634,848 → ~785 KB | user traces cited; new builds "not validated" |
| 09-18 | `cfd42139` / `alpha2-integration-audit-baseline` | Audit: "NOT PLAYABLE END-TO-END", glue layer untested | 57 tests, 56 pass, 1 fail | ≈780 KB | — |
| 09-21 | `83b2ed56` Batch 17 | Host suite first fully green | 85/85 | 858,896 B | Batch 5 and 9B gates passed on device |
| 09-21 | `f5ca709e` Batch 19 | Audit era's last batch; 145-row checklist exists | 88/88 | 860,320 B | 4 of 145 rows PASS |
| 09-22 | `9e1dd6e6` Batch 20 | First consolidated hardware pass: 117 PASS, 12 FAIL, 1 BLOCKED, 3 INCONCLUSIVE, 19 UNTESTED (of 152) | 88/88 | 860,320 B | first full physical session |
| 09-24 | `0bbdbf5c` Batch 48 | Last Camp/bed batch with code | 118/118 | 873,344 B | 6T–7C via a stale image (§8.3) |
| 09-25 | `0a3c1af9` Batch 52 | Readiness audit: NOT READY, four blockers | 120/120 | 874,864 B (B51) | Phase 7D PASS |
| 09-26 | `alpha2-batch54-rc1` | Alpha 2 RC1 | 123/123 | 878,752 B, 169,824 B free | smoke pending |
| 09-26 | `alpha2-batch55-release` | **Alpha 2 released**, RC1 promoted byte for byte | 123/123 | 878,752 B | Phase 8 smoke PASS (8 of 8 steps) |
| 09-26 | `alpha3-a3-01-audio-architecture` | Audio architecture; stock DOS has no music | 126/126 | 908,816 B | test tone PASS |
| 09-26 | `alpha3-a3-04a-audio-realtime` | Music stutter root-caused (a mutex in a function-local static) | 134/134 | 954,848 B | "mostly smooth" |
| 09-27 | `alpha3-a3-04f-render-efficiency` | Render efficiency: compose 38.5 → 10.1 ms | 146/146 | 977,888 B | H-200 PASS |
| 09-27 | `alpha3-a3-04g-storage-inspect` | System Menu opens in ≈180 ms instead of ≈820 ms | 149/149 | 980,672 B | H-201, H-202 PASS |
| 09-27/28 | `alpha3-hf5-dialogue-pacing` … `alpha3-hf10-mix-parity` | Dialogue pacing, shrine, Codex, sacrifice, Refuge, Mix parity | 154 → 162 | 985,488 → 988,320 B | H-205…H-213 PASS |
| 09-28 | `alpha3-rc1` (`f85575b9`) | Alpha 3 RC1 | 162/162 | 988,320 B, 60,256 B free | smoke + heap capture pending |
| 09-28 | `alpha3-release` (`eb1bf5e7`) | **Alpha 3 released**, RC1 promoted unchanged | 162/162 | 988,320 B | RC smoke PASS, heap gate PASSED |

## 3. Origin

### 3.1 What the repository started as

| Commit | Date | Author | Content |
|---|---|---|---|
| `1225f9ac` | 2026-08-25 19:43 +0200 | `kokoima` | "OpenU5: public release — byte-exact browser port of Ultima V, verified against the original binary". 2,183 files, 755,630 insertions, no body. |
| `ad1a9dc1` | 2026-08-26 00:09 +0200 | `OpenU5` (noreply@openu5.org) | "Sync from upstream": a visual comparison harness, an NPC load-fidelity gate, eight adjudicated reds. 42 files. |
| `291736a4` | 2026-08-26 09:34 +0200 | `OpenU5` | "Sync from upstream": data-segment strings extracted into a runtime asset, four fidelity fixes, refreshed docs. 73 files. |

At `1225f9ac` the tree was already `game/` (1,254 files, the TypeScript/PixiJS engine and its tests), `re/` (782 files: 516 reverse-engineering notes, mostly in Spanish, a parity harness and 48 tools), `extractor/`, `demo-byo/` and `docs/`. Its README describes a byte-exact port whose rules were "re-derived from the binary itself": every rule citing an offset (for example `COMBAT:0x194A`), a coverage ledger claiming 202,800 of 202,800 bytes justified, a differential parity harness against an independent model, and a DOSBox-X oracle. Its headline "239 automated parity tests" rises to "276" at `291736a4`; `docs/methodology.md` still says 239. Neither is a native count. Its golden rule — "no gameplay change is accepted without an assembly citation or a parity scenario. If the port contradicts the derived rule, the port is wrong" — is the rule the native batches apply (§7.3).

### 3.2 When native work began: imported, preparatory and native commits

A previous summary of this history placed the start of native T-Deck work on 2026-09-12. That is wrong. The repository supports one statement: **the first native T-Deck commit is `11e32392`, "Add T-Deck Plus display and SD bring-up", authored 2026-09-14 23:12:27 −0400** (03:12 UTC on 09-15). It is the first commit that touches `native/` (ten files there plus `.gitignore`, +925 lines: `tdeck_board.cpp`, `tdeck_pins.h`, `main.cpp`, `LAUNCHER.md`, `package_launcher.py`, `sdkconfig.defaults` and build files), and the first to introduce the strings "T-Deck" or "ESP32" (measured with `git log -S`). The commits before it fall into two classes, defined by what they touch:

| Class | Definition used here | Commits |
|---|---|---|
| **Imported / upstream** | Authored by someone other than the later committer, stamped +0200, arriving as a complete project | `1225f9ac`, `ad1a9dc1`, `291736a4` (08-25 → 08-26) |
| **Preparatory (web-side)** | Authored by `thekojac304`, touching only `game/`, `extractor/`, `docs/`, `re/notes/`, `.claude/` and `test-results/`; no native file | `0315510f` (2026-09-12 19:01), `2485950e` (2026-09-14 21:19) |
| **Native T-Deck implementation** | Adds or changes files under `native/` for the device or its portable core | `11e32392` (2026-09-14 23:12) onward |

`0315510f` (message in Spanish) is a self-described "work-in-progress snapshot … not a logical unit" of the web project's `enhanced-mode` branch: a mobile "Enhanced" chrome, contextual tap on NPCs, spell and settings panels, and an OPL audio lane ("7375 unit green; 91 red are pre-existing Windows-environment failures"). The OPL synthesizer it adds under `game/src/ui/opl/` is later ported to C++ for the T-Deck's music (§12.3). `2485950e` (subject "Update", empty body) is more web-side work, with two test-result screenshots. The word "deck" in both refers to the browser's portrait touch *deck* (`installPortraitDeckDom`, `setDeckSheet`), not the T-Deck.

The local reflog adds one non-committed fact. At 21:22:00 on 2026-09-14 the checkout was cloned from a repository named `OpenU5-Enhanced` under the committer's account (its `main` equalled `291736a4`); three seconds later it moved to `enhanced-mode` (`2485950e`), and at 21:22:18 to a new branch **`esp32-tdeck-port`**, still at `2485950e`. That branch name is the earliest trace of T-Deck intent, 110 minutes before the first native commit; it is reflog state, not history. The remote-tracking ref `enhanced/esp32-tdeck-port` points at `4d77fe77` (09-15 17:40), so the port's first day was also pushed to `OpenU5-Enhanced`; `origin` is a separate repository, `OpenU5-Native`. The T-Deck project thus began as a fork of the browser port, whose code (the "TypeScript reference") remained in the tree as a cross-check throughout.

### 3.3 What the evidence does not establish

- **Motivation.** No committed file before `11e32392` mentions the T-Deck, LilyGO or the ESP32, and the first native document is a technical bring-up note. (The upstream ROADMAP's mobile touch deck is a browser feature.)
- **The relationship between the upstream author and the committer.** The upstream commits are authored `kokoima` and `OpenU5`; every later commit is `thekojac304`. The tree does not say whether these are one person.
- **Anything between 2026-08-26 and 2026-09-12**, a 17-day gap with no commits. The two web-side commits were already on the remote when this checkout was cloned (`2485950e` three minutes before), so they were made in another working copy.
- **Milestone 1.** Milestone 2's documents say it was "tested successfully by the user through the bmorcelli Launcher"; no commit contains it.
- **A working directory name.** `native/core/VALIDATION.md` gives a reproduction command under `OpenU5-TDeck`, suggesting — an inference — that the work began outside this checkout.

## 4. Architecture evolution

The final architecture was not planned in advance; most layers were added because something failed on hardware or in an audit.

| Piece | First in git | Why it appeared | What changed later |
|---|---|---|---|
| **Reference implementation** (`game/`, TypeScript) | `1225f9ac` | Upstream import. | A cross-check, itself corrected several times when the binary disagreed (§7.4). |
| **Reverse-engineering corpus** (`re/`) | `1225f9ac` | Upstream: 516 notes, 202,800-byte coverage ledger. | Native-era tools because the cited listings were missing (§7.2). |
| **T-Deck board layer** (`tdeck_board.cpp`, SPI display, SD, Launcher packaging) | `11e32392`, 09-14 | Milestone 2. | Runs on the host over a fake ST7789 from A3-04E (`4269266a`). |
| **Asset pack** (`openu5-assets.bin`, OU5PACK) | `5b4a8e91`, 09-14 | Game data reaches the device only through a derived, CRC-checked pack; nothing copyrighted enters git. | Unchanged (132,284 B) from Alpha 1 to Alpha 3. |
| **Static renderer** (11×11 tiles, 176×176 viewport, PSRAM buffer) | `1a593e0a`, 09-15 | Milestone 4. | A3-04F cut composition time by 74% (§12.8). |
| **Portable core** (`native/core`, ESP-free C++17) | `8a603f8f`, 09-15 | The same rules on device and host, checked against fixtures generated from TypeScript. | Grows through `fd2a2036` and every batch after. |
| **Input** (keyboard matrix, trackball, Mic key) | `8a603f8f` | The only inputs. | Mic key corrected in Alpha 1.4; controller recovery and a 1 ms capture task in Alpha 2.0 (§15.9). |
| **Host tests** | `8a603f8f` (14 `add_test` lines) | Core parity against fixtures. | 61 lines at `c18f5b64`, 127 at `4b3257f4`, 166 at `eb1bf5e7` (measured); §16. |
| **UI session** (`UiSession`) | `c18f5b64`, 09-18 | The boundary between raw input and the core. | Modal ownership rewritten in Batch 1; `reset_after_load` in A3-HF4. |
| **Developer menu** | `c18f5b64` | A port of the web debug API: teleport, presets, editors. | The main validation tool; also a lock-up cause in Batch 25 (§15.3). |
| **Frontend** (`FrontendSession`) | `c18f5b64` | Title, New Journey, Continue/Load/Settings. | System Menu (`Alt+M`), whose slow open drove A3-04G. |
| **Saves** (`alpha_save.cpp`, two-generation, CRC) | `c18f5b64` | Alpha 2.0 needed to save. | Batches 26 and 28; inspect cache in A3-04G. |
| **Resource pack** (`openu5-alpha1-resources.bin`) | `c18f5b64` | Dungeons, NPCs, dialogue, combat maps, shops. | 1.23 MB → 2.04 MB at Batch 53. |
| **Real `AlphaRuntime` on host** | `3c2d437a`, Batch 11 | No test instantiated the device glue (§6). | Every batch from 14 on depends on it. |
| **Scene pacers** (`NarrativeScenePacer`, `scene_timing.h`) | Batch 7B → Batch 51 | Scripted scenes ran "way too fast". | Extended by HF5 (`DialoguePacer`), HF7, HF8, HF9. |
| **Audio** (I2S backend, PC-speaker synth, OPL2 music) | `2f218808`, A3-01 | Alpha 2 shipped silent. | 62 of 73 cue ids play; music only with a user patch (§12). |
| **Developer-screen diagnostics** | `11015be6`, A3-04B | A transcript report was invisible. | The standard way to measure on the device. |
| **Image guards, RED-first scripts** | A3-04A → A3-04F | Defects host tests cannot see. | Run on every batch from HF4 on. |

Three structural moves were decisions rather than accretion.

**The core/glue split.** From the first native commit a portable core (rules, no ESP-IDF) was separated from device glue (`AlphaRuntime`, `UiSession`, presentation, storage). The audit's central finding (2026-09-18) was that this left the glue unprotected: nothing instantiated `AlphaRuntime`. Batch 11 did not extract a `GameplayController` as proposed — `command()` alone was about 600 lines with logging interleaved — but linked the real, unmodified `alpha_runtime.cpp` into a host target through shims for four dependency surfaces. That seam is why the rest of the history is possible.

**"Bind it once."** Batch 52 found that the parity harnesses bound gameplay hooks the device never bound (§10.2). Batch 53's production binders (`bind_quest_services()`, `bind_shop_services()`) are called by both `AlphaRuntime::initialize()` and the host fixture, and source-scanning tests fail if either assigns a hook outside a binder. The pattern began with `bind_rest_services()` (Batch 29) and recurred in HF5, HF6 and HF8.

**Real hardware models on the host.** A3-04E ran the real `tdeck_board.cpp` over a fake ST7789 and SPI timing model; A3-04G ran the real `alpha_save.cpp` over a fake SD card with fault injection and a heap census. Render pacing and storage footprint moved from "device only" to "reproducible on the host", though device numbers still had to be measured (§8.4).

## 5. Bring-up and the Alpha 1.x era (2026-09-14 → 2026-09-18)

### 5.1 The milestone series

There are no git tags before 2026-09-18; each milestone is known by its `PROJECT_VER` and README:

| Milestone | Commit | PROJECT_VER | What it proved |
|---|---|---|---|
| M2 | `11e32392`, 09-14 23:12 | `0.2.0`, "display + microSD" | ST7789 and SD share SPI2 and both work. 298,064 B image for the bmorcelli Launcher. |
| M3 | `5b4a8e91`, 09-14 23:44 | `0.3.0` | A derived pack (OU5PACK v1: palette, 512 tiles, a 256×256 map) is read and CRC-checked on the device. |
| M4 | `1a593e0a`, 09-15 00:20 | `0.4.0` | Britannia renders; a colour-order fix (`MADCTL` 0x68 → 0x60); "still require physical-device visual verification". |
| M5 / M5.1 | `8a603f8f` (09-15 17:37) | `0.5.1-input-final` | Trackball and keyboard movement on the packed map. |

`8a603f8f` ("Add native physical combat foundation") is a bulk drop of 1,755 files (+535,270 lines), 1,650 of them accidental build output under `build-m5/`, deleted three minutes later by `4d77fe77`. It also carries the core's first 72 files: movement, combat, commands, items, rest and travel. `fd2a2036` ("Add native dialogue dungeon magic and persistence systems") undersells its content the same way. That is the shape of the whole pre-audit era: a few huge commits whose messages name one topic.

Milestone 5.1 is the first recorded device debugging. `DEBUG51.md` and `STACK51.md`: physical logs confirmed a main-task stack overflow (3,584-byte stack, about 584 bytes free at minimum); `app_main`'s frame went from 768 to 288 bytes, a CRC routine's from 1,056 to 48, and the stack to 12,288 bytes. `DEBUG51.md` also withdraws an earlier claim that shared debounce was "the exact lockup cause".

### 5.2 Alpha 1 to 1.4.0-alpha2

All Alpha documents were committed together in `c18f5b64`, "Update Alpha 2.0 implementation" (2026-09-18 16:46, 200 files, +59,669/−545, empty body), so git gives no order among `ALPHA1.md`, `ALPHA12.md`, `ALPHA13.md`, `ALPHA14.md`, `ALPHA14_ALPHA2.md` and the eleven `ALPHA20_*` reports. `ALPHA14.md` embeds "current build: 4d50203e9c68", `c18f5b64`'s parent, so this work was built from a tree of 2026-09-15 and committed three days later. `UiSession`, the Developer menu, `AlphaRuntime`, the frontend and the save service all first appear in `c18f5b64`; no Alpha 1.x firmware was separately committed.

- **Alpha 1**: "the first T-Deck build that runs the native game state and `UiSession` controller … built, packaged, and host-tested without access to a physical T-Deck Plus. Hardware success has not been claimed." Launcher 634,848 B; packs of 132,284 B (assets) and 1,227,341 B (resources). Playable *on paper*, with a Developer menu on `Alt+D` and ten presets. Missing: touch, audio, horse-seller completion, some shrine and endgame narration.
- **Alpha 1.2**: a combat freeze — the core refuses player commands while an enemy is the current combatant, and the device, unlike the browser, had no combat pacer to issue `CombatEnemyStep`. It mapped the Mic key to matrix (4,4). 654,960 B; 27/27 native tests.
- **Alpha 1.3**: "why Mic required Symbol" (key identity inferred from the layer-resolved character); Giant Rat and Giant Spider mis-rendered because a monster-name pool indexed by definition omits definitions 8, 9, 42 and 43. 658,384 B; 50/50.
- **Alpha 1.4**: a "HARDWARE-TRUTH diagnostic pass" opening with a retraction — "(4,4) … was only a vendor-table assumption"; a physical trace put Mic/0 at (0,6). 667,216 B; 51/51.
- **Alpha 1.4.0-alpha2**: (0,6) applied (short press Cancel, 1.1 s hold toggles movement mode); a 10 ms keyboard poll replaced by interrupt-edge reads plus a 45 ms fallback, since a full controller scan takes about 35 ms. 671,600 B. `DEVICE_INTEGRATION_AUDIT.md` (dated 2026-09-17) adds a 45-scenario on-device smoke list.

Every Alpha 1.x document says its build was not flashed "in this environment", while 1.4 and the 2.0 reports rest on "supplied physical traces": the agent side had no device, the tester did. Only `ALPHA14.md` received a header note after the Mic retraction.

### 5.3 Alpha 2.0 (`ALPHA20_*`)

The eleven undated `ALPHA20_*` reports (order inferred from image sizes) are device corrections, each rooted in a supplied trace:

- **Frontend and New Game.** A title with the original six menu items; New Journey follows the gypsy questionnaire (8 virtues, a 4+2+1 bracket, 28 questions, from FONT.OVL). 709,136 B; 53/53; "not physically validated".
- **Flicker.** Each 275 ms title tick began with a full black fill; dirty-region redraw cut it from 137,720 to 14,112 pixels.
- **Initial-save failure.** FATFS allowed three open files; two packs and the logger filled them, so `write-gam-temp` failed with `EMFILE`. Six files, packs closed after caching.
- **Stack.** 624 bytes of margin at startup; stack 12,288 → 24,576 bytes, a 10,080-byte PSRAM persistence workspace.
- **Keyboard freeze.** A transient I2C read failure became permanent because the failed bus state was retained. Staged recovery (retry, bus reset, one raw-mode re-entry) — the mechanism that absorbed three read errors in the RC1 capture (§13.5).
- **SD failure.** `allocate_dma_buf: not enough mem` from a fragmented internal heap: an 8,192-byte DMA reserve, a storage mutex, a 1 ms input capture task with a 64-event queue. The first appearance of the heap concern closed at RC1 (§13).
- **A fabrication in the making.** A "forensic" pass added a "Used <name>." echo and an invented "No effect!"; its reference generators were "infrastructure-blocked and never ran", and the invention lived until Batch 13 (§7.4).

By `c18f5b64` the images had grown from 634,848 B to 772,000–784,720 B; `LAUNCHER.md` recorded 779,680 B.

### 5.4 Naming

Milestones gave way to "Alpha 1" and its point releases, those to "Alpha 2.0" (`2.0.0-alpha1`, then `2.0.0-alpha2-debug`), and once the audit began "Batch N" replaced version numbers. `c18f5b64`'s subject says "Alpha 2.0" while the audit committed 93 minutes later calls the same tree "NOT PLAYABLE END-TO-END".

## 6. The integration audit and Alpha 2 batches 1–19 (2026-09-18 → 2026-09-21)

### 6.1 Why the audit existed

`cfd42139` (2026-09-18 18:19, tagged `alpha2-integration-audit-baseline` four minutes later) adds the 1,221-line AUDIT. Its thesis: the core "is in far better shape than the device integration". The host suite had 57 tests, 56 passing (the failure was `gameplay_parity`); the layer between raw input and the core (`alpha_runtime.cpp`, `ui_session.cpp`, `presentation.cpp`, the asset pack) had "zero automated test coverage". Four "anchor" symptoms motivated it — a stale tile after loot is removed, loose-loot icons that did not match their identity, View Gem appearing to do nothing, a broken dungeon 3D view; that they came from hardware use is an inference, since the reports are not in the repository. The audit is unsigned and reads like an agent session's product (a "throwaway program … in the session scratchpad", a "Model: Opus-level / Sonnet" line per batch); git attributes it only to the committer.

### 6.2 How it worked

The method: "input→adapter→UI→core→state→renderer→feedback→mode-return→persistence tracing, static reachability sweeps, reference adjudication, host test-suite execution, and one purpose-built core probe", with evidence tagged [EXEC], [REF], [STATIC] or [UNPROVEN]. The static reachability census — every `CommandKind`, `UiMode`, `GameEventKind` and `DungeonAction` checked for a producer and a consumer — was the most productive tool: `TurnAround` and `Drink` had no producer, six event kinds (Quake, CellExplosion among them) no consumer, and a `usable_names` table 32 consecutive nulls. Those became R-08, R-10, R-12, R-13, R-19, R-20 and Y-04.

**The tracking systems** used for the rest of the record:

| Prefix | Meaning | Where it lives |
|---|---|---|
| **R-nn** | Red: a known failure with root cause, fix and evidence. R-01…R-20 at baseline, R-21…R-34 later; status appended to the heading. | AUDIT §3 |
| **Y-nn** | Yellow: behaviour not proven or adjudicated. Y-01…Y-27. | AUDIT §4 |
| **H-nn** | Hardware-checklist row. Created in Batch 18 ("one deduplicated 140-row device list"); from Batch 20, H-146 up also named hardware findings, later queued defects — one namespace, three uses (Appendix B). | CHECKLIST |
| **D-nn** | A knowing divergence from the original, with Kind and disposition; A-nn are T-Deck adaptations, E-nn enhancements. Created in Batch 18 (D-1…D-18). | LEDGER §4 |
| **RB-n**, **K1…K11** | Release blockers and blocker criteria, Batch 52 (§18.2). | AUDIT Batch 52 |

### 6.3 The batches

The repair plan listed Batches 1–11; the numbering did not survive (Batch 12 was used twice, the planned R-21 batch became 13). Batches 1–4 and the "4.5" series have no tags.

- **Batch 1 (`63eeac3b`): three writers became one owner.** `synchronize_after_debug()` overwrote Shop and ShrineSpecial modes on the very input that entered them; a new ESP-free `ui_mode_policy.h` with separate return-mode registers replaced it. A naive fix would have soft-locked `ShrineSpecial`, which had no `handle_input` case, so both landed together.
- **Batch 2 (`0a45b673`):** there were two chest-promotion sites, not one. Once `gameplay_parity` mismatch 59 was fixed the test ran on and failed at 2034 — a masked native fabrication (Batch 13).
- **Batch 3 (`c054d289` and two others):** the Use picker rebuilt on canonical ids, Ready made available in combat and dungeon, digits routed to SetActivePlayer or HarpsichordNote. 812,016 B.
- **Batch 4 (`420418f6`): R-09 was a softlock**, not a discarded answer: `commands.cpp` gated every command with `AwaitingResponse` while a `BlackthornSession` flag was live, and no `BlackthornAction` was ever dispatched (`ALPHA20_BATCH4_ADJUDICATION.md`). Crystal-ball and wishing-well analogues (R-25, R-26) followed in Batches 19 and 18.
- **Batches 4.5A–D (09-19):** a floor-list ordinal used as a signed z in Developer teleport; `ShrineServices` never wired (`InvalidContext` on hardware); a 33-entry pack rejected by a stale 32-entry bound; transcript auto-scroll reset on every event; basement search objects hidden by a floor byte of 255 against a runtime −1 — shared with the TypeScript reference and fixed in both; Blackthorn's capture scene ported as a paced scene.
- **Batch 5 (`d67cb2c0`):** the plan was wrong twice — the reference consumes a spell's charge before prompting, and removing the combat guard was "necessary but not sufficient" because `UiSession::handle_modal` treated every target-selection `Cast` as the combat reticle. The tag records the first hardware pass: three world casts "PASS on the physical T-Deck, first attempt".
- **Batches 6–8B:** world objects and dungeon sessions persisted (saving underground is authentic: DOS saves one raw memory dump); map-reveal and zodiac renderers; a spell-metadata audit that found 11 of 12 flagged targeting entries stale; In Ex Por's unlock wired.
- **Batches 9–9E (09-20):** the dungeon series (§15.2).
- **Batches 10–15:** View Gem; combat field magic (In Grav seeds no combat field — original behaviour) and a per-territory combat-map index; the fabricated (U)se echo (`c512781b`, §7.4); the full Z-stats pages plus an off-by-one in `equipment_display_name` for all 48 ids; a sacrifice-roster bug the audit had mis-described on three counts, unreachable on the device.
- **Batch 17 (`83b2ed56`): the `quest_parity` crash.** Carried for nine batches as "a MinGW/w64devkit environment finding", it was harness undefined behaviour: an inline `setjmp`/`longjmp` watchdog escape on a toolchain where GCC no longer provides `__builtin_sponentry`, the frames disagreeing by `main`'s ~39 KB frame at `-O2` and coinciding at `-O0` — which is why it looked like noise. Trigger: two of 1,152 theft cases in which a re-roll in `TALK.OVL 0x11c7` never terminates. A full 5,377-case `-O0` run had matched the reference on every case, so nothing was hidden. Harness-only, firmware byte-identical: **85/85, "the first fully green run in the audit's recorded history"**.
- **Batch 18 (`bb0067a1`):** the reconciliation that created the checklist and the ledger, fixed R-26 and R-34, and deferred R-25 for want of a shared seam. **Batch 19 (`f5ca709e`)** built it — the kernel `0x4988–0x4a83` command-character picker shared by Look, Search and Cast; a first version that gated `world_look()` failed `gameplay_parity` at sequence 895, and the code, not the fixture, moved.

### 6.4 What the audit era established

By Batch 19: 88/88, 860,320 B with 188,256 B free, and 145 checklist rows of which four had a PASS (H-48, H-85, H-86, H-88). Batch 18: "No hardware evidence is claimed or inferred." A method had appeared — RED first, GREEN, then mutation (§16.3). The record also keeps the audit's mistakes about itself: R-09 was harder than stated, R-11's "six spells" were three, R-16's flagged targets were mostly a stale premise, and R-23 was real but unreachable on the device. Several tags of this era print decimal sizes that disagree with their own hex (Appendix B).

## 7. Reverse engineering

### 7.1 What the corpus is and where it came from

The `re/` tree arrived whole in the first commit: 516 notes (most in Spanish, named things like `acampada-apagon-303.md`), a coverage ledger (`re/COVERAGE.md`) assigning all 202,800 bytes of `ULTIMA.EXE`, its 24 overlays and `DATA.OVL` to code, data or inert segments (835 segments, closed 2026-07-11 per its header), a parity harness and 48 tools. `docs/methodology.md` states the method: every rule cited by overlay and offset, a DOSBox-X oracle that compares state byte for byte, and an independent model run against the engine; `re/deliberate-divergences.md` classifies what is knowingly not identical.

The native project's contribution was the *reading* of that binary for questions the browser port had not needed: how long does the original wait, what does it sound like, what does it do on a machine with no sound card? At `alpha3-release` `re/notes/` holds 527 files (measured): 516 imported, six from the upstream syncs, two from the web-side commits, and three native-era notes (`batch53a-endgame-terminal.md`, `batch53b-moongate-return.md`, `mix-hf10-command-parity.md`); eleven existing notes were corrected in place after `11e32392`. Most native findings live in AUDIT, AUDIO and eighteen `native/core/batch*-original-*.log` files, each the output of a script that asserts the shipped bytes.

### 7.2 The listings that were not there

Upstream notes cite disassembly as `re/disasm/CMDS.OVL.asm`. Batch 21B (`b6705142`, 2026-09-22) records that it "**is not in the tree** — `re/disasm/` does not exist", and the `npm run re:disasm` that `docs/methodology.md` names is defined in no tracked `package.json`. So the native project built its own: in Batch 21B `re/tools/dis16.py` (16-bit capstone over the shipped binaries, offsets matching the notes' CS:IP), `thunks.py` (the overlay thunk table and load bases) and `callers_banda.py` (an exhaustive by-band census of relative calls); then independent `DATA.OVL` oracles for chest loot and NPC schedules (Batches 23–24), bed-pose scripts (45–49), a census of 216 timing call sites (51), assertion scripts for the ending and the moongate loop (53A–B), and speaker and data-segment censuses (A3-01 → HF2). The first tools' control was that the derived `TOWN`/`OUTSUBS` bases reproduced `npc-carga-partida-fresh-gate.md` §1 byte for byte; the Batch 51 census accepts the base that resolves the most near calls onto known kernel entries.

**The overlay base table was wrong three times.** A near call inside an overlay resolves to the kernel only after rebasing, and the Batch 21B table did not survive later evidence:

- `MAINOUT` was listed at `0x8304` (overlay 3's base). Batch 53B found `0x81d0`; the old value "missed every MAINOUT caller". The audit (not the commit body) says earlier "only caller" claims that relied on it "should be re-run"; no later record says they were.
- `TALK` was listed at `0xA290`. A3-HF5 found that its kernel calls resolve through `0xBF80`; through `0xA290` the KeyWait call lands mid-function. The upstream note `audio-202.md` had already used `0xbf80`, so the repository contradicted itself before the tool was written.
- `CAST2` was listed at `0xC29E` (by both `thunks.py --bases` and `callers_banda.py`). A3-HF6 found `0xE1E0`, "the same class of table error that A3-HF5 found for TALK".

Only `MAINOUT` was fixed. **At `alpha3-release`, `callers_banda.py` still lists `'TALK.OVL': 0xa290` and `'CAST2.OVL': 0xc29e`** (measured from the tagged blob). Both are recorded as "tooling finding, not changed" and "queued as RE-tooling follow-up work (not a game defect)" (`AUDIO §32.12`), but neither is on `ALPHA3.md`'s deferred list; §19.2 carries them as known residue. The record does not say whether HF5's derivation first started from the wrong value.

### 7.3 Method in practice

Visible from Batch 21B onward:

1. Read the routine with the project's own disassembler; record offsets and bytes (many logs record SHA-256 of the raw bytes, e.g. `batch35-original-cmds-bytes.log`).
2. Check the reading with an oracle the fix does not share — a positive control, a second tool, or a script asserting expected instructions (`batch53b_moongate_return.py` asserts 22).
3. Only then compare with native and the TypeScript reference and decide which is wrong.
4. Fix at the layer the parity fixtures pin. When a fixture must move, regenerate it under a `--check` control and prove by token diff that only the intended rows moved (Batch 23: `quest_parity` moved at exactly sequence 4941, seeded contents 8 → 30).
5. Where the bytes do not settle a question, say so and keep native flagged rather than invented.

### 7.4 Where the binary overruled an assumption

Upstream had set the pattern: "Blocked by wall!" does not exist in the binary (only "Blocked!", and the ranged attack `COMSUBS 0x0A68` does no line-of-sight check), and a census (`fabricacion-147-acta.md`) compared 848 user-facing strings against 28,411 printable strings in the original files after `"Here thou art!"` was found to be invented. The native-era cases:

| When | Assumption | What the binary said | Consequence |
|---|---|---|---|
| Batch 13, `c512781b` | (U)se on a consumable prints `Used <name>.`, else "No effect!" | `CAST.OVL 0x11de/0x11f0` print only "Scroll" (DS `0x466a`), `0x135a/0x136e` only "Potion" (DS `0x4706`); the potion ceremony at `0x139b` precedes the reroll at `0x13a1` | The Alpha 2.0 invention removed; `action_feedback_test` corrected, not weakened; `gameplay_parity` 5,058/5,058 |
| Batches 21B/23 | Chest refill is bed-only; vault chests seed contents 8 (the reference's "declared placeholder") | Every floor change runs `TOWN.OVL 0x052E → 0x408 → 0x1694`; the seed is `0x1E` at `TOWN 0x1795` | Batch 23 corrected 21B's census, which counted only the kernel thunk; 41 of 48 equipment rows become reachable |
| Batches 45 → 49 | The upright bed pose is original; NPC types ≥ 0x40 bypass the selector | The selector at `0x51bf–0x51e9` admits `0x40–0x7f` | Claim withdrawn (§15.5) |
| Batch 53A | The ending auto-answers its two questions | Two real Y/N loops at `ENDGAME.OVL 0x0852`, `0x088b` | D-56, deferred |
| A3-HF1 | "Full inventory keeps the pile" ("deliberate and correct") | `SJOG 0x1458` saturates and always clears | Invented in Batch 2; withdrawn (§14.1) |
| A3-HF2 | The clock strike re-arms every turn | `advance_clock 0x4f7c` skips the re-arm at `0x514a` unless the hour changed | Note, TypeScript and native all corrected (§14.1) |
| A3-05 | A kill's second burst, "0x2fe3, a death" | `0x2fe3` is the chest trap `0x2fd0` | Removed (§15.10) |

When native, reference and binary disagreed, the binary decided; the reference was corrected when it shared the error (Batch 23, HF2.1, HF9, HF10) and left alone when nothing pinned it.

### 7.5 Recovering time, sound and effect

Batch 51 (`045cb092`, 2026-09-25) moved from *what* the original does to *how long*. The tester had reported the Camp apparition "way too fast"; the batch inventoried every staged scene and graded the timing evidence A (exact duration proven), B (proven primitive, host-dependent duration), C (proven order, unresolved duration), D (native compromise) or E (guess). Four primitives carry all the waits:

- `delay(n)` at `0x20fa`: real BIOS ticks (54.9254 ms, via an INT 1Ch handler at `0x2159`; PIT channel 0 is never reprogrammed). Class A.
- `run_n_frames(n)` at `0x3ae6`: n ticks, compositor between. Class A.
- `tone_sweep` at `0x2192`: a2 "samples", each 1/1000 tick by design; its mute branch runs the same loop, so **it blocks even with sound off**. Class B.
- `fx_tile_fizzle_in` at `0x1068`: 256 blits, no timer. Class C.

Root cause, a "mixed" case: the device re-imposed the original's wait only where a scene pacer existed and only for tick primitives; waits spent inside `tone_sweep` or a fizzle were zero on a device with no working speaker, and the Camp apparition had no pacer at all. The fix, `scene_timing.h/.cpp` and `NarrativeScenePacer`, holds each chime 193 ms, the chord 2,325 ms and the restore 165 ms at a calibration of 25,806 samples per second — a fast-host floor, not a measurement; two 1988 witnesses bracket the chord from above at about 2.75 s and 4.567 s. The audio track reused the discipline (§12): the speaker's PWM loop, its noise LFSR (seeded `0x7664`) and the 174 ms hit burst were read from loop bodies.

The addresses behind later findings are given with each story rather than repeated here: the Camp apparition (§15.6), the ending (§10.3), the moongate loop (§15.8), the hit cue (§14.1), TALK (§14.2), the shrine and Codex (§14.3–§14.4), Blackthorn (§14.5), the Refuge (§14.6), Mix (§14.7).

### 7.6 What the corpus could not settle

- **No original rendered witness.** Batch 49 ran the original under a bundled DOSBox-X 2024.03.01 with a patched save; the screenshot hotkey captured nothing ("Screen report: Method 'None'"). The scheduled-NPC bed pose stayed unresolved (D-38).
- **The 1988 `ENDGAME.OVL` is second-hand.** The tree's copy is the GOG build (loops poll the keyboard and exit to DOS on a key); the 1988 variant, which spins, is quoted from `fanfarria-endgame-espectral.md`. Both are terminal.
- **An unexplained figure.** The Lord Fenton corpus measured the moongate sweep (a2 = 30,000) at about 200 ms or less, which no linear model of `tone_sweep` explains.
- **The real 1988 sweep duration** is host-dependent; 25,806 samples per second is a floor, and the record says so.

## 8. Hardware validation methodology

### 8.1 Why host tests were never enough

The parity suite could be green while the device was wrong: Batch 9D's host suite "proved a rule the device never ran" (§15.2); Batch 52's drivers bound hooks the device left unbound (§10.2); the Alpha 2.0 failures — a DMA allocation, a 3,584-byte stack, a three-file FATFS limit — have no host equivalent (§5.3); and A3-04A's audio path was an inline byte test on x86 but a FreeRTOS mutex 1.7 million times a second on the ESP32 (§12.4).

### 8.2 The device workflow

`package_launcher.py` validates the ESP32-S3 app header, segment checksum and appended SHA-256 of `openu5_tdeck.bin` and copies it byte for byte; it never flashes. The user copies the Launcher image to the SD card and installs it from the bmorcelli Launcher ("The user flashes manually"), with the game packs derived locally from the user's own files in `/ultima5/`. The tester follows a checklist phase, using the Developer menu (`Alt+D`) to teleport or set up presets, and records results as row text. From A3-04E.1, timing phases also capture a serial log (115,200 baud) committed with a script-generated summary.

### 8.3 The identity gate

Every Launcher image since Alpha 2.0.0 shared one file name until Batch 54. That caused the project's clearest process failure.

- **Batch 23 (`5adae63f`):** the first Batch 23 image identified itself as `FW b6705142ee31`, because the firmware stamps `git rev-parse HEAD` at configure time and the image was built before the commit. A correction commit named the flashable image.
- **Batch 50 (no commit, no tag, no document of its own):** in the Batch 50 prompt of 2026-09-25 the tester reported a bed-render defect "physically reproduced". The device was running a Batch 43 image (boot identity `Git c1226a0af4c3`), not Batch 48; the failure "is fully explained by pre-Batch-45/48 code". Batch 50 was cancelled. It is known only from the Batch 51 section of AUDIT: "Batch 50 made no production change, no commit and no tag … `build-batch50` is only an ignored scratch build dir" (the directory is no longer on disk). AUDIT calls it "a testing-process lesson, not a production defect". **Batch 51** added the checklist's mandatory "firmware identity gate": read the boot screen's `Git <hash>` and compare it with the owning tag's commit before recording anything. Phases 6T–7A, run without a recorded hash, were accepted afterwards on an argument (the Batch 43 image contained the Batch 25–39 fixes).
- **Batch 54 (`211c676a`):** the Launcher file name follows `PROJECT_VER`, "because a shared or ambiguous file name is how a stale image reached the device in Batch 50".
- **A3-04G:** captures begin with an `IDENTITY` line (`firmware=3.0.0-alpha3-dev-a3-04g-debug git=c8fee48beda2`).
- **RC1:** a PowerShell `Tee-Object` lost 7.4–99.9 s of the boot, so the RC1 identity rests on the user's report of the identity screen, as the record says.

### 8.4 The division of evidence

| Kind of evidence | What it proves | What it cannot | Examples |
|---|---|---|---|
| **Host parity** (`gameplay_parity`, `quest_parity`, `command_parity`, `magic_parity`) | The core computes what the binary computes, RNG stream included: 5,058 sequences, 5,377 quest cases, 25,088 magic cases. | That the device binds or presents it. | Batch 13 (caught); Batch 52 (hidden). |
| **Device-runtime host tests** (real `AlphaRuntime`, Board, `alpha_save.cpp`) | The glue drives the core from raw keys; pixels reach the panel. | Real timing, heap placement, bus contention; the device took 1.29–1.31× the modelled transfer time (A3-04F). | Batches 21A–31; A3-04E on. |
| **Physical observation** | What the tester sees, hears and cannot escape. | A cause. | H-149 to H-152: three different explanations (§15.1). |
| **Serial telemetry** | Cadence, stalls, heap, watchdog, audio windows. | A subjective verdict. | `AUDIO_PERF`, `SYS_PERF`, `SAVE_INSPECT`, `SD_HEAP`, `KEYBOARD_RECOVER`. |
| **Image guards** | Properties of the linked ELF. | Behaviour. | `a3_04a_hotpath_check.py`, `a3_04b_iram_check.py`, `a3_04f_image_check.py`. |

### 8.5 How the recording evolved

Batch 20 (§9.1) set the practice of prefixing a result with the image's SHA-256 prefix ("PASS (Batch 20, hardware, `a357e28c…`)"; that is the 860,320 B image's SHA-256, not a commit); its serial capture is not cited. In Batches 21–49 almost every batch ends "host fixed, device retest pending", and results arrive, untimed, in the next batch. Batch 51 added the identity gate and struck stale results. A3-04E.1 (`29c5b7a7`) committed the first serial captures (`a3-04e1-hw-soak.log`, 10,487 lines); A3-04G (`04d977c5`) added a closeout script (`a3_04g_hw_closeout.py`, 343 lines) that turns a capture into a numbered summary; RC1 committed capture, summary and a heap adjudication with the release.

Several PASS results came from a *later* image carrying the same code: H-201 (HF3) on the A3-04G image; H-203 (A3-05), H-204 (HF4), H-210 (HF9) and H-213 (HF10) together in one 20-minute session on the A3-HF10 image (`Git 028269fec0c5`). H-206's FW and Git were "not reported". The RC1 checks were written to run against the exact release image.

## 9. Alpha 2: hardware findings and the parity sweep (Batches 20–49, 2026-09-22 → 2026-09-24)

From Batch 20 (`9e1dd6e6`) through Batch 49 (`a76f94f1`), 36 commits changed 746 files (+193,208 lines), but production source (`native/core/src`, `native/targets/tdeck/main`) accounts for only 22 files, +1,270/−205 (measured); the rest is tests, logs, fixtures, documents and tooling. Batches 20–29 have full commit bodies; Batches 30–49 have subject lines only, and their evidence lives in the tags and AUDIT.

### 9.1 Batch 20: the first consolidated pass

The Batch 5, 9B–9E and 12B tags already cite physical results, but Batch 20 (2026-09-22 02:07) is the first pass through the whole checklist, in one long session on the 860,320 B image: boot and input, combat, magic, the wishing well, towns and transport, dungeons, the quest. Of 152 rows (145 plus H-146…H-152 added that session): 117 PASS, 12 FAIL, 1 BLOCKED, 3 INCONCLUSIVE, 19 UNTESTED. AUDIT lists "eight confirmed production defects, two CRITICAL": the Vas Rel Por phase prompt, potion-cancel not consuming, wishing-well case sensitivity, ship and horse purchases doing nothing (H-146), the Z-stats `^` glyph rendering as `?`, a dungeon room-entry freeze family (H-149–H-152), dungeon save/load reverting (H-115), and a shard-ritual lock that left Move dead (H-118). The freeze and the Move lock needed a power cycle.

Batch 20's root-cause leads were mostly wrong, and later batches said so: the freeze was blamed on a missing transition event and the 4-bit room-number collision (the collision is original; neither was the cause, §15.1); H-115 on a restore path (§15.4); H-118 on a stuck UI mode (a UI-session cause, but a different mechanism, §15.3). The tester is unnamed ("the tester", "the user").

### 9.2 The freeze family, the vault and floor changes (21A–23)

- **21A (`236cfa34`): three observations, one defect.** The Destard freeze (H-151) was a capacity check that refused every combat command; the Deceit L8 "Blocked!" (H-149) and the Set Active Player auto-pass (H-150/H-152) were authored (§15.1).
- **21A.1, 21A.2:** the Deceit L1 → L8 report was a six-deep pit shaft, and adjacent-door invisibility was authored; both were adjudicated by proving native and reference identical.
- **21A.3:** Set Active Player was unreachable during combat (a two-deep reachability gap); `CombatAction::SetActive` was *appended* to the enum, because parity fixtures encode ordinals.
- **21B (`b6705142`), 22, 23:** chest refill on bed hole-up (H-148); basement chests missing after a Developer teleport (the core-owned `HydrateInterior` effect reached a device handler with no arm for it); vault loot seeded with the wrong contents value. Batch 23 (`93ef488b`) corrected 21B's census (§7.4); its first image misidentified itself (§8.3).

### 9.3 State, reload and save (24–31)

Each batch used a real `AlphaRuntime`, the shipped pack and raw key events, RED then GREEN then mutation. Every bug was in device glue or routing, not in an original behaviour.

| Batch | Row | Root cause |
|---|---|---|
| 24 (`caa6488a`) | H-161/162/163 | One 1988 routine, `TOWN.OVL 0x0408`, is the floor loader for stairs, ladders, Journey Onward and post-town-fight; native ran a subset. |
| 25 (`60581138`) | H-118 | The Developer overlay overwrote a parked question (§15.3). |
| 26 (`0a2c1978`) | H-115 | `"dungeon"` was not in the sidecar whitelist (§15.4). |
| 27 (`e5692ae0`) | H-164 | `Alt+L` ran a hand-copied subset of the post-load synchronisation Continue Latest ran in full. |
| 28 (`f964df19`) | H-166 | A CRC-valid newest save with an invalid sidecar beat a good older one, and the post-write self-check certified it. |
| 29 (`e1ee763a`) | H-154/155 | The device bound the rest services to an empty lambda and a constant `false`; the host fixture "had its own copy of the stubs, so no test ever ran production's binding" — the start of "bind it once" (§4). |
| 30 (`bff6608e`) | H-156/157 | Bed sleep omitted the per-tick housekeeping and day/night refresh. |
| 31 (`6e3da691`) | H-165 | A pending "Leave this place?" plus a Developer teleport into a dungeon latched `awaiting_exit`. |

### 9.4 The Camp / bed / sleep / advancement chain (32–49)

Eighteen consecutive batches, three (40, 47, 49) with no production change, re-read the same few routines: `CMDS.OVL 0x0552` (bed hole-up), `CMDS 0x0000` (Camp), `OUTSUBS 0x0658` (`camp_results`) and kernel `0x6936` (the Camp scene). The chain grew because each batch's reading named the *next* omitted call:

| Row queued | By | What it was |
|---|---|---|
| H-167, H-168, H-169 | Batch 29 | camp watch, cannon-killed NPC, pre-sleep NPC passes |
| H-170 | Batch 30 | Q sleep target hour |
| H-171 | Batch 32 | camp housekeeping (later reclassified) |
| H-172 | Batch 34 | bed-entry viewport fill |
| H-173, H-174 | Batches 36, 37 | camp duration from a nonzero minute; intermediate status refresh |
| H-175 … H-179 | Batches 40–44 | staged advancement, standing actor and XOR flash, pre-apparition scene, ring RNG, ring-42 outline |
| H-180, H-181 | Batches 45, 47 | NPC bed visibility (from a physical observation), dynamic bed pose |

The chain corrected itself. Batch 36 found that Batch 32 had mislabelled three Camp calls — `0x5910` is the viewport redraw (it gates the wind roll), `0x2900` the status draw, `0x20fa(1)` a one-tick delay — so Camp never calls `kernel_turn_housekeeping 0x2ae8` and the "queued survival claim was false" (LEDGER D-27). Three upstream notes were corrected in place; `cannon-fire.md` had called `0x7AF4` an HP roll, but it is `set_actor_record 0x3A74` with zero fields. The stories are in §15.5–§15.7. Firmware grew from 860,320 B to 873,344 B (+13,024 B) and the suite from 88 to 118 tests.

### 9.5 Batch 50: the batch that was cancelled

Batch numbers run 49, 51. There is no Batch 50 commit, tag or document (measured: no tag matches `*50*`, no commit subject names Batch 50). The session was cancelled when its "physically reproduced" bed defect proved to be a stale image; the story, and its one durable result, Batch 51's identity gate, are in §8.3.

## 10. The Alpha 2 release (Batches 51–55, 2026-09-25 → 2026-09-26)

### 10.1 Pacing (Batch 51)

Phase 7B on the confirmed Batch 48 image showed every apparition visual correct but "way too fast". Batch 51 (`045cb092`, 2026-09-25 21:01) is the timing archaeology of §7.5: core tests RED 14/27 → GREEN 28/28, runtime 5/16 → 21/21, 12/12 mutations, 120/120, 874,864 B. Phase 7D: Camp pacing "dramatically improved", Blackthorn "much better", the untouched TrollSneak control unchanged. The batch *classified but did not fix* four scene-local gaps that became Alpha 3 hotfixes: shrine and Codex key waits (H-183), the ritual inversion (H-184), Refuge cadence and getkey (H-185), the sacrifice burst (H-186).

### 10.2 The readiness audit (Batch 52): NOT READY

Batch 52 (`0a3c1af9`, 22:00) changed documents only: "ALPHA 2 NOT READY FOR RELEASE CANDIDATE". It folded the unrun phases (6M, 6M.1, 6N, 6P, 6R, 6S) into a Phase 7E and swept every hook in every `*Services` and `*Context` struct for an assignment in device or core source. The parity drivers bound the missing hooks themselves; the device bound none:

| ID | What was missing | Effect on the device |
|---|---|---|
| RB-1 (H-189/D-46) | `end_record` and `ENDMSG.DAT` | The Wooden Box victory was refused; the final arena teardown was deferred forever. |
| RB-2 (H-187/D-44) | `words` (Words of Power) | Yell never matched; all eight dungeon seals stayed closed. |
| RB-3 (H-188/D-45) | the moonstone owner | Gates never transit and are never drawn; Vas Rel Por always "Failed!". |
| RB-4 (H-146/D-49) | `ShopServices::ship/horse/reserve` | Buying a ship, skiff or horse was a silent no-op — failed on hardware in Batch 20, then dropped out of tracking after 21B. |

A probe compiled the real `quest_driver.cpp` with each hook removed as the device leaves it: of 5,377 cases, control 0 divergent, device shape 84. Batch 52 also defined K1–K11 (§18.2). It is the clearest single example in the repository of what host parity cannot see.

### 10.3 Binding and the two hardware corrections (53, 53A, 53B)

- **Batch 53 (`1d7135e9`, 23:02):** one production binder per service family (§4) and three new SD-pack sections (`endmsg-records.bin` 838 B, `karma-records.bin` 793 B, `words-of-power.bin` 98 B); the pack grew from 2,039,545 B / 39 entries to 2,041,466 B / 42 entries (payload CRC `0x26f75ae6`), and the firmware refuses a pack without them. Moonstones are hydrated from the loaded GAM and captured back (`0x28a…0x2a9`) with no format change. `batch53_release_blockers` was 40 GREEN, 42 RED of 82 on the Batch 52 sources; 22/22 mutations (a first-pass survivor, an unpersisted Word-of-Power seal, added two checks). 121/121, 877,728 B.
- **Batch 53A (`5fe1ac53`, 23:52): the ending.** Phase 7E-A passed the absorption, VICTORY!, the ENDMSG text and input — and then the party stood in Doom's final cell and *accepted movement*. "Play continues" had been Batch 53's own assumption. The bytes say the ending never returns: `ENDGAME.OVL endgame_main 0x0648` has ten getkey sites, and `endgame_datestamp 0x0326–0x04fe` has one `ret`, reachable only through a call that never returns and loops at `0x04f9`. New `UiMode::Ending`: game keys swallowed, Save refused ("Save unavailable: the quest is complete"), System and Developer menus reachable. Batch 53's tag, which describes the earlier state, was not edited. 44 checks (RED 26 of 39), 15/15 mutations, 122/122, 878,752 B.
- **Batch 53B (`a0a064fe`, 09-26 00:36): the moongate return.** Stepping back onto the arrival gate did not return the party; **Outcome A: the original's behaviour, no production change** (§15.8). 24 checks, 8/8 mutations, 123/123.

### 10.4 Release candidate 1 (Batch 54)

Batch 54 (`211c676a`, 01:25; tag `alpha2-batch54-rc1` at 01:30) declared "ALPHA 2 READY FOR RELEASE CANDIDATE", 0 blockers. It changed only identity: `PROJECT_VER` `2.0.0-alpha2-debug` → `2.0.0-alpha2-rc1-debug`, and a Launcher name derived from it (`OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`); the source diff against 53A is `CMakeLists.txt` (+4/−1) and `package_launcher.py` (+16/−1). The image is 878,752 B, as 53A, with 169,824 B free. The tag records **98 bytes differing from the 53A image** (version string, Git, times, SHA); a `cmp -l` of the two files still on disk also counts 98 (measured). SHA-256 `ff3dfe193973547db2648c5f486aa381086505eb80b5f2dadec7432194fb5828` (recorded and measured), embedded Git `211c676a1dca`. The Launcher allocation was at least 917,504 B (896 KiB), up from 768 KiB for the Alpha 2.0.0 development image.

### 10.5 The smoke test and the promotion (Batch 55)

Phase 8 was "catastrophic regressions only", ten to fifteen minutes on the RC1 image: boot identity, input, save and `Alt+L`, power-cycle Continue, a dungeon, combat, Camp, the Refuge. Steps 9 (ending) and 10 (transport) were deliberately not run, being covered by Phase 7E and the host suite. All eight passed, at an unrecorded time between 01:30 and 01:56.

Batch 55 (`4b3257f4`, 01:56; tag `alpha2-batch55-release` at 01:58) chose **"Option A": promote RC1 byte for byte.** A rebuild would be a new, untested image differing in version string, Git, build time and SHA; "no convention requires a version string without `rc`"; and a renamed copy would disagree with its own identity screen, in a project where "a shared or ambiguous file name is how a stale image reached the device in Batch 50". So the released firmware shows `FW 2.0.0-alpha2-rc1-debug`, `Git 211c676a1dca`, "kept on purpose". `git diff alpha2-batch54-rc1 alpha2-batch55-release` touches ten files, all `*.md` or `*.log` (measured; +2,867/−74): the release is code-identical to RC1. Host 123/123 (134.3 s pre-commit; 101.4 s on the committed tree).

Alpha 3 reused the precedent wholesale, down to treating Batch 54's 98-byte difference as its own RC's size tripwire (a larger difference "means the RC carries code, and it stops") and placing its tags the same way (§20.3).

### 10.6 The state at release

`ALPHA2.md` lists what the release includes — the full game from character creation to the ending; the quest's device services (Words of Power, moongates, moonstones, shipwrights and stables, the Refuge, the ENDMSG ending); bed sleep and Camp with the watch and day/night tiles; two-generation CRC-verified saves; title, New Journey, System Menu, transcript scroll-back, Developer menu — and what it does not: "no audio or music", no moongate transit animation, no ending cinematic, two literal `victory` lines after the ending, the four scene gaps H-183–H-186, a disabled Ready picker, and the unresolved 1988 sleeping pose of named NPCs.

## 11. From Alpha 2 to Alpha 3: the handoff and what it became

`ALPHA2.md`'s "Alpha 3 handoff" defined three tracks, "Each starts only when it is asked for":

1. **Audio / music** — backend and service architecture; the original's sound events including silent waits; music contexts, volume, the harpsichord (H-125), scene cues. Planned as A3-01 architecture, A3-02 synthesiser, A3-03 remaining effects, A3-04 music, A3-05 polish and hardware validation.
2. **UI / frontend** — audit the game window, HUD and Settings; "produce mockups before any code change"; the Ready picker (D-8), the `^` glyph (H-63), the `Direction?` overlay (H-15).
3. **Presentation fidelity** — moongate transit (D-48), the ending cinematic (D-54), H-183–H-186, the ending's Y/N prompts (D-56) and `victory` lines (D-57), and smaller items.

| Planned | What happened |
|---|---|
| Audio A3-01…A3-05 | Done, but A3-04 (music) produced a stutter, and seven performance sub-batches (A3-04A–G) followed on the audio path, renderer, SD card and heap (§12, §13). |
| — (unplanned) | Ten hotfixes (A3-HF1…HF10), all found by playing on the device (§14). |
| UI / frontend | **Not started.** "No mockups exist yet" is still in `ALPHA3.md` at release. |
| Presentation fidelity | Partly done by hotfixes: H-183…H-186 became HF6–HF9; the Mix command (D-6, and D-70 found later) HF10. The rest stayed deferred. |

Alpha 3 was planned as a feature track and executed as a stabilisation track: unplanned performance and storage work, parity fixes discovered by playing, then a deliberate closing of scope (§18). `ALPHA3.md` summarises it as Alpha 2's game plus "sound", "a faster, steadier device" and "presentation and gameplay parity fixes found on hardware".

## 12. Alpha 3: audio, music and the performance work it opened (2026-09-26 → 2026-09-27)

The track ran from 03:31 on 2026-09-26 (A3-01) to 21:57 on 2026-09-27 (A3-05), about 42 hours. It produced sound effects, music, and — as its largest side effect — the render, SD and idle-service work that made the device smooth. Host totals per sub-phase are in §16.2 and image sizes in §17.1.

### 12.1 The starting facts

**Stock DOS *Ultima V* has no music.** Every level of the record says so (`AUDIO §0`, the A3-01 commit body, ledger E-6, `ALPHA3.md` §11). The static audit behind it, upstream's `re/notes/audio-profile-1988.md`, found that all four `.DRV` files are *video* drivers (`T1K.DRV` is Tandy video, refuting a hypothesis that music lived in Tandy sound); that across `ULTIMA.EXE`, the overlays and the drivers the only audio ports written are `0x42` (14 `out`s) and `0x61` (19), the PC speaker's PIT and gate; and that there is no AdLib, Sound Blaster, MPU-401 or Tandy access. The only global sound flag, `[0xa9ce]`, is the speaker's. The original's melodies are one-shot monophonic speaker sequences: Iolo's song, the Camp apparition arpeggio, the harpsichord.

**Music exists only through a community patch the user may have installed**: the Exodus Project *Ultima V Upgrade* Release 1.0 (2001-08-21), applied in place under `original/u5/ultima5/`. It adds 16 XMI songs (51,598 B in all), `FAT.OPL` (a Miles AIL bank of 181 two-operator timbres), `mid.drv` (the song selector), a MIDPAK kit and patched code, and a three-byte `DATA.OVL` edit that turns `T1K.DRV` into `MID.DRV`. It plays through OPL FM; songs run 11.9–144.8 s; peak polyphony (17 notes) exceeds an OPL2's 9 voices.

**What the port does.** It reimplements `mid.drv`'s selector (`0x016d`) as `music_context_for_location()`. Capability is **detected from the files, never from names** (`mid.drv`'s SHA-256, the songs its table names, `FAT.OPL` as a Miles bank; "unknown is never treated as stock") and travels in a separate optional SD file, `/ultima5/openu5-audio.bin`, because stock and patched installs produce byte-identical game-pack inputs and the game pack's size and CRC are locked. The repository holds no music bytes; with stock files the music backend is never reached, and sound effects, synthesised from the original's own speaker parameters, need no asset. Music is a deliberate *addition*, ledger E-6 — recorded late, at the pre-RC reconciliation. Knowingly not done: crossfades, the ending's scene chain (D-54), OPL3.

### 12.2 A3-01 to A3-03: sound effects (2026-09-26)

**A3-01** (`2f218808`, 03:31) was architecture only. The core already emitted about 45 `Sfx` events (31 distinct ids) and the device dropped them all; there was no I2S code. The speaker is a TX-only I2S amplifier on GPIO 7/5/6. A census of the original found 121 primitive call sites (later 126). Two decisions: `SfxVolume` is an output gain and **not the 1988 sound flag** (branches that depend on `[0xa9ce]` — the blindfold drag, the bard's lute — keep their sound-ON and sound-OFF forms regardless of volume), and audio completion "must never become the pacer again" (§12.10). 21/21 mutations; +30,064 B. Hardware: the test tone played, volumes scaled, 0% muted.

**A3-02** (`bc65972b`, 12:47) emulated the six speaker primitives from their loop bodies rather than copy the TypeScript model: the timbre is `tone_sweep`'s literal one-bit PWM loop, the noise PRNG is the word `[0x545c]` seeded `0x7664`, and one integer, 25,806 samples per second, calibrates everything. It departed from the reference three times "toward the bytes" (a duty-swept gate, not a 50% square; seed `0x7664`, not `0x1234`; 25,806, not 25,806.45). A four-deep pending queue lost notes on fast harpsichord keys and became eight. Two ESP-IDF builds died inside third-party sources (`.tbyte` in `mcpwm_oper.c`, a GCC segfault in `esp_lcd_panel_rgb.c`); a resumed `ninja -j 4` completed. Scene-timing safety was proved by running the paced Camp and Blackthorn timelines under six audio setups (none, synth, muted, failing, stalled, rendered 100× ahead) with identical frame timestamps. 22 cues and the harpsichord (closing H-125); 26/26 mutations.

**A3-03** (`76d4dd43`, 14:08) classified all 126 sites: 95 implemented, 22 evidence-unknown, 4 with no native event, 5 deferred presentation. **62 of the 73 cue ids play; 11 stay silent, each with a stated reason.** Ambient sounds (fountain, waterfall, the clock's tick and tock) come from `ambient_sfx_tick 0x4102`; the quake is a new `Rumble` primitive; `a3_03_sfx_inventory` fails if the core emits a cue the device does not play. One claim was wrong and pinned by its own test — the clock strike re-arming whenever the clock moves — refuted the next day by A3-HF2 (§14.1).

### 12.3 A3-04: music arrives, and stutters

A3-04 (`3bbe6716`, 17:39; its tag points at the version-bump commit `db6aa437`) ported the browser reference's OPL emulator, voice allocator, bank reader and sequencer into `music_synth.cpp`, with a direct XMI parser and a context switcher following the driver's priority order. "Software complete, host-validated"; no T-Deck was available; projected cost an *estimate* of 20–40% of core 1 and under 40 KB. 13/13 mutations. Image 944,784 B (`0xe6a90`; the tag's decimal "945,808" is a slip, §12.11). The first hardware result, on `Git db6aa437dc56`: the right songs, "extremely stuttery".

### 12.4 A3-04A: the mutex in the function-local static

A3-04A (`e48abb8d`, 19:02, 83 minutes later) found the cause by reading the linked image rather than measuring the device. `music_synth.cpp` read its ROM tables through `tables()`, which held a function-local `static const OplTables`. The firmware is built with `-mdisable-hardware-atomics`, so GCC cannot inline the "already constructed?" acquire-load and calls `__cxa_guard_acquire` on every access — which ESP-IDF implements as `xSemaphoreTake`/`xSemaphoreGive` on one global mutex. `tables()` runs twice per sounding operator per sample, and all nine channels sound almost all the time: a 16 ms block made 14,400–27,000 guarded reads (28,656 at most), about 1.7 million mutex round trips a second on the Theme; even at 0.5 µs each, 13.5 ms of every 16 ms. The producer ran below real time, so the DMA played each correct block and then silence: "recognisable but broken up". On x86 the same source compiles to an inline byte test, so no host test could see it.

The proof was static: `a3_04a_hotpath_check.py` walks the ELF call graph and finds `generate → Operator::sample → wave_atten/expo → tables → __cxa_guard_acquire`, the only hot-path guard among seven guard sites — RED on A3-04's image, GREEN on A3-04A's. The fix moved the tables to namespace scope (bit-identical output, fingerprint `6a7ff3d5727df2c6`), built that file alone at `-O2 -ffp-contract=off`, parsed all 16 songs once at boot (21,529 events, about 172 KB of PSRAM), kept the chip and allocator in place so a song switch allocates nothing, and redesigned the DMA ring as 8 × 128 frames primed before enable; two sequencing defects went with it (the channel enabled before any audio was written; an idle drain that cut about 32 ms off every sound). 22/22 mutations.

What was never confirmed: the predicted `task_wdt … IDLE1` every five seconds was never reported, and the device benchmark was never seen, because its report was invisible (§12.5). The root cause rests on static analysis, host counts, a model — and the fact that the fix worked: music "mostly smooth", though "the overworld lagged".

### 12.5 A3-04B and A3-04C: an invisible diagnostic and a wrong theory

A3-04B (`11015be6`, 20:54) opened with a new report: with music on the overworld was slow with a "tearing-like effect", a fight with two snakes "a little staticky", and at Music Volume 0% the overworld "MUCH better". And the Developer benchmark ran its 47 seconds and showed nothing.

- **The invisible diagnostic.** The report went to the gameplay transcript, which the Developer screen covers; the 135 px (22-column) panel wrapped about 21 lines into about 50 rows, leaving the last two in view. The A3-04A measurement had been lost. The report now replaces the Developer screen's rows until dismissed, tested through the real runtime with raw keys.
- **The hypothesis.** The synth ran from flash through 16 KB of ICache and 32 KB of DCache shared by both cores, contending with the renderer on core 0. Response: a cheaper synth (bit-for-bit, 254 → 155 Xtensa instructions per channel-sample) and every per-sample function in IRAM (`audio_iram.lf`, `-fno-jump-tables`), proved by `a3_04b_iram_check.py` (RED on A3-04A: 25 flash functions on the path). IRAM +5,692 B; the internal heap starts 7,472 B later. 33/33 mutations.
- **Left open.** A clip counter showed no clip is possible at default volumes; no later document says the combat static went away. The IRAM change was never A/B-tested against A3-04A on hardware, so whether it helped is not established.

A3-04C (`2a53128a`, 22:48) is headed "OUTCOME C: INSTRUMENTED; THE CAUSE IS NARROWED, NOT PROVEN", and refused to change production code: "a production 'fix' now would be a guess, which the batch rules forbid." Music 0% removes three things at once (synth, I2S DMA and interrupt, audio-task wake-ups), so it added a *synth-bypass probe* (song, channel and DMA cadence kept, synth skipped) and per-frame TFT and SD-burst instrumentation. The result nobody predicted:

| Run | audio CPU | CPU0 | frame max | TFT max | SD-log burst max |
|---|---|---|---|---|---|
| music 80% | ~41% | ~79% | ~1175 ms | ~1137 ms | ~1159 ms |
| synth bypassed | ~2% | ~62% | ~871 ms | ~833 ms | ~848 ms |
| music 0% | ~1% | ~59% | ~1504 ms | ~1465 ms | ~1500 ms |

The 0.8–1.5 s stalls were **independent of music** — the worst run had none — and each frame maximum sat within 4–23 ms of the SD-log burst maximum. (Approximate, "read off the Developer report"; the image on which the lag was first seen is not recorded, and the user's "much better" at 0% is not reconciled beyond the timing coincidence.)

### 12.6 A3-04D: the SD card and the TFT share a bus

A3-04D (`5cd8fd0b`, 23:51) traced the mechanism from the code. SPI2 carries both the ST7789 (CS GPIO 12, 40 MHz) and the SD card (CS GPIO 39, 800 kHz). ESP-IDF's `sdspi_host_start_command` holds the bus (`spi_device_acquire_bus(portMAX_DELAY)`) for the whole command, busy time included. One log flush is a single 8 × 512 B write: at least 41 ms of clocking in one bus hold, before the card's programming time. An idle-priority logger wrote about 0.4–0.5 KB per step and flushed its 4 KiB buffer every few seconds; while it wrote, no TFT row could move.

**Decision (Option A): diagnostic SD logging is off by default**, with a Developer switch for one session. Buffering was rejected ("whatever the buffering, a 4 KiB write at 800 kHz holds the TFT's bus for ≥ 41 ms"), and deleting the logger was rejected because checklists name it. The SD log was also less durable than it looked: FATFS writes a file's size only on sync or close. With logging off the second-long stalls were gone; a maximum of about 400–445 ms and a viewport average of about 122 ms remained, with and without the synth.

### 12.7 A3-04E and E.1: the tick sleeps, and the watchdog regression

What remained was a deterministic, tick-quantised repaint. A3-04E (`4269266a`, 09-27 01:19) found every draw loop sleeping `vTaskDelay(1)` (to the next 10 ms tick) every 16 rows or 32 fill chunks: 9 sleeps per walking step, 37 (7 + 9 + 7 + 5 + 5 + 1 + 1 + 1 + 1) for the full repaint on leaving the Developer menu — which opens every measurement window and explained the identical maxima. This was the first run of the *real* `tdeck_board.cpp` on the host over a fake ST7789: modelled 391.7 ms against the device's 404–408 ms. The sleep had arrived with the Alpha 2.0 import without comment or test, and for the candidate reasons (watchdog, input, audio, DMA) "the history does not say".

The fix yielded (`taskYIELD()`) at the same points (modelled repaint 113.8 ms) and gave the main loop — whose `vTaskDelay(pdMS_TO_TICKS(5))` was 0 ticks at 100 Hz — a real one-tick queue wait. **It broke something.** A pre-test run tripped the task watchdog on IDLE0 after about 71 s with `main` running; the A3-04E image "is not hardware-valid". Its design table had said "a draw cannot starve IDLE0", an argument from source never measured, later marked "wrong as a guarantee".

**A3-04E.1** (`cb423a69`, 02:19) found that the game thread no longer blocked in four places (the queue peek, the loop-spin branch, console output busy-waiting for USB FIFO room, the draws), and that the old sixteen-row sleep had implicitly fed the watchdog nine times per step. It added an `IdleServiceGuard`: a second core-0 idle hook counts idle-loop passes, and if the count has not moved for 200 ms (25× under the watchdog) the game thread sleeps one tick, at most three. It neither disables nor extends the watchdog. The closeout (`29c5b7a7`, 12:39) recorded two captures: 550 s of default pacing with no `task_wdt`, `idle0` gap maximum 102.9 ms, `forced=0` on every heartbeat, audio clean; and a probe run in which deliberate loop-spins drove the guard 262 times, the gap peaking at 223.9 ms. **The 71 s trip was never reproduced on hardware in default pacing** — "the guard was proven by Stage 2 instead". The same capture recorded, "not acted on", the two observations that start §13: the System Menu took about 0.72–0.75 s to open, and the internal-heap low-water mark had fallen to 200–344 B.

### 12.8 A3-04F: render efficiency

A3-04F (`dcea9539`, 15:40) measured first. Composition averaged 38.5 ms of every drawn frame, with or without music, and about 90% of the rasteriser was a bit-by-bit CRC-32 of the viewport: 4.71 million instructions per frame at `-Og`. Four changes, all host-proven pixel for pixel: a table-driven CRC (a `constexpr` table at namespace scope, "never a function-local static, applying the A3-04A lesson", −88%); whole pixel rows per SPI transaction up to the bus's 640 B; party, status and transcript rows drawn only on change; retained panel rows. Internal `.data` −1,808 B.

The hardware result (H-200, `Git dcea95390676`, 213.7 s, 3,630 frames) is the project's cleanest before/after:

| Metric | A3-04E.1 | A3-04F | Change |
|---|---|---|---|
| composition, average | 38.5 ms | 10.1 ms | −73.8% |
| full-screen TFT, max | 148.8 ms | 120.1 ms | −19.3% |
| viewport-frame TFT, average | 51.9 ms | 33.5 ms | −35.5% |
| `idle0` gap, max | 102.9 ms | 37.6 ms | −63.5% |
| frame, average | 61.9 ms | 19.9 ms | −67.9% |
| pixel transactions per frame | 257.5 | 47.6 | −81.5% |
| audio underruns / misses | 0 / 0 | 0 / 0 | unchanged |

Row fill took 25.0 µs with audio idle and 25.1 µs busy: "the shared-cache coupling … is not measurable in the renderer any more". The host model got the repaint ratio right (−18% modelled, −19% measured) but under-predicted the step gains (−24% against −35%). Two side outcomes: the heap low-water mark rose to 2,200 B, which the `.data` change "did not explain", so it became a watch item and led to A3-04G; and H-200's step 6, expecting a hit member's row to flash, "describes behaviour the device never had" — it became A3-HF3.

### 12.9 A3-05: audio finalised

A3-05 (`4264f34e`, 21:57) accepted the volume curve on the user's device verdict — one square law for both channels, `gain = v² · 32767 / 10000` (10% −40 dB, 50% −12 dB, 80% −3.9 dB) — with no production change. It added session mutes (`Alt+Shift+M`, `Alt+Shift+S`), finding on the way that Alt chords compared `ascii_lower(code)` and ignored Shift, so those keys had opened the System Menu and saved. And it removed the kill's second burst (D-64, §15.10). 20/20 mutations.

### 12.10 The rule that audio never holds the game

The original *blocks* in its sound routines — `tone_sweep` waits even with sound off; the victory fanfare blocks 2.09 s and flushes the keyboard — yet a late audio task must not stall gameplay. The A3-01 rule: durations are derived from the original's constants and used as *scene holds* by the pacers; audio renders alongside and never gates them, and the timelines are identical under six audio setups by test. Consequences: ledger D-60 (the fanfare plays while play continues, "a decision, not a defect"), and a constraint A3-04E had to respect — services scheduled relative to "now" (scene pacers, the enemy beat, the poison flash, Camp) keep the old spin, because a late step would push every later step back.

### 12.11 The A3-04 image size, resolved

The A3-04 tag and AUDIO both print "0xe6a90 = 945,808 B (+25,680 B vs A3-03's 920,128 B); 103,280 B" free, and the commit body repeats the "+25,680 B". The hex and decimal disagree: `0xe6a90` is 944,784 (945,808 is `0xe6e90`). The build logs (`a3-04-postcommit-firmware-build.log`, `a3-04-firmware-retry.log`) print `0xe6a90` with `0x19570` (103,792 B) free, and the post-commit Launcher on disk is 944,784 B (measured). So the correct values are 944,784 B, +24,656 B, 103,792 B free. The earlier draft called the 1,024 B "unexplained"; it is the same kind of hex-to-decimal slip as the Alpha 2 tags (§6.4), and A3-04A's documented "+9,040 B" inherits it (computed: +10,064 B).

## 13. Storage, the System Menu and the heap

### 13.1 Where the problem was first seen

The two symptoms were first recorded at the A3-04E.1 closeout (§12.7; `AUDIO §23.10.5`). The concern was older: the Alpha 2.0 `allocate_dma_buf: not enough mem` failure (§5.3) had already named large workspaces retained in internal RAM, with two remedies — an 8 KiB `SdHeadroomGuard` and long-lived objects moved to PSRAM.

From the committed soak log (`AUDIO §28.2`): the first System Menu open took from the key edge at 299,268 ms to the first menu frame at 300,088 ms — **about 820 ms**, of which 720 ms was the SD window and 98.5 ms the full-screen frame. Free internal heap went from 227,903 B (largest DMA block 49,152 B) to **77,411 B (largest block 23,552 B)** and stayed at 77,355–77,411 B for the rest of the run: −150,492 B internal and −84,132 B PSRAM, flat, "a plateau, not a slope". The low-water mark read 200 B.

### 13.2 The mechanism

`Alt+M` ran a save inspection inside the headroom guard. For each of two slots it read the commit record (32 B), the GAM (4,192 B), the OOL (512 B) and the sidecar, then ran `verify_candidate`: `complete_generation` (three CRC-32s, parse the sidecar, `import_save(GAM)` into a discarded whole document) and `stage_generation` (a second import, then restore and validate). Per open, on the host's fake card: 8 file opens, 10,210 bytes, 4 imports, 7,812 allocations.

The imported document is **2,717 nodes and 3,885 allocations** — 266,058 B in small blocks on the 64-bit host; on the device a 32-bit *estimate* of **about 193 KB in 303 blocks, every one ≤ 4 KiB**, exactly the size that `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096` sends to internal RAM first. One verify peaks at 1.42× the document; a second slot's verify on a stage still holding the first peaked at **2.43×** (about 469 KB) against roughly 236 KB of free internal RAM. Every cold verify filled internal RAM and the 48 KiB DMA pool. The double import predated Batch 28; `c18f5b64` already had it.

Two facts made the reading subtle, and both were written down before A3-04G began: `heap_int_min` is the **sum of each internal region's own lifetime minimum** (the DMA pool included), so a low value means every region came near full *at some time*, not that the heap ran out; and card time is display time, because the card shares SPI2 with the panel (§12.6).

### 13.3 A3-04G

A3-04G (`c8fee48b`, 2026-09-27 19:05) "measured first", with the first host build of the *real* `alpha_save.cpp`: `host_tests/sd_shims/` renames stdio and POSIX calls in a force-included header (production compiles verbatim), a fake card counts every open, read, write, rename and stat and injects faults by path, and a heap shim counts by capability. A 795-line test (44 checks: census, retention, 100 open/close cycles, the save list, failure paths, save/load correctness) drove the real runtime; 12 of 44 RED first; 17/17 mutations after one equivalent mutant (inspect already released inside its loop) was removed. An apparent 2,040 B "leak" over 100 cycles was `std::vector` doubling in the test's own record of the heap.

The changes: `verify_candidate` empties the stage *before* importing, so a verify never holds two documents; `save`, `load` and `inspect` release the whole workspace on every exit, and inspect releases each slot before reading the next; the save list is cached in the existing PSRAM workspace, keyed per slot by the commit record's fields; a Save made inside the menu now updates its Load page. In passing: `AlphaSaveCommit` has four trailing padding bytes that `save()` writes uninitialised, so a first `memcmp`-based cache missed one slot every time. **Nothing was moved to PSRAM**; +1,760 B of flash.

Host-side before/after (`a3-04g-red.log`, `a3-04g-green.log`; "no device improvement is claimed before H-202"):

| Operation | Before | After |
|---|---|---|
| open, unchanged card: files / bytes | 8 / 10,210 | 2 / 64 |
| open: allocations | 7,809 | 11 |
| cold inspect: kept after return | 295,038 B | 32 B |
| cold inspect: small-byte peak | 645,688 B (2.43×) | 378,749 B (1.42×) |
| Continue Latest: small bytes alive at a card read | 268,054 | 2,366 |
| save: kept after return | 295,774 B | 32 B |

### 13.4 The hardware closeout

`04d977c5` (21:24) recorded H-201 and H-202 from one serial capture (`a3-04g-hw-h201-h202.log`, 3,877 lines; the boot before 31.3 s is missing), on image `Git c8fee48beda2`:

| Item | Before (A3-04E.1) | After (A3-04G) |
|---|---|---|
| key edge to menu frame | ≈ 820 ms | **180 ms on every open** (−78%) |
| input blocked (`render_block_us`) | 724,017 µs | 84,966–85,544 µs |
| cached inspect, `total_us` | — | **82.8 / 83.0 / 83.2 ms** (min/median/max), 28 of them |
| bytes kept after a window | 150,492 B on the first open | **0 B in all 29 windows** |
| heartbeat internal free over 23 opens | flat plateau at 77 KB | 93,831 B on all 10, flat to the byte |
| cold boot inspect | — | 785,608 µs (card 444 ms, CPU 328 ms) |
| loads | ≈ 1,057 ms | 1,051 / 890 / 843 / 815 ms |

The misses are recorded: a cached open was predicted at 45–60 ms (measured 83), key-to-frame at ≈150 ms (measured 180), and the old window's split at ≈280 ms card / ≈440 ms CPU (measured cold: 444 / 328) — "card I/O was underestimated and the imports overestimated". The ranking was right.

H-202 passed *with an unmet clause*: its criterion required a largest internal block of at least 32 KiB after a menu open, and 23,552 B was measured. The document argues the opens did not cause it (the Continue before them had set it); the verdict was PASS and the heap watch stayed **open**, reclassified "allocator placement / fragmentation watch — recoverable, not a leak".

**The heap watch rules.** Seven re-escalation triggers were written with the implementation (`§28.16`, `c8fee48b`): (1) any DMA-allocation, low-internal-RAM, scratch, SD or save I/O error; (2) a storage window whose restored `free_internal` is more than 4 KiB below its release value; (3) `largest_internal` below 32 KiB; (4) the heartbeat falling across 20 or more menu opens; (5) a new `heap_int_min` low outside a storage window; (6) a batch adding ≥ 1 KiB of internal `.data`/`.bss`/IRAM without an offset; (7) an Alpha 3 release candidate, which requires the capture. Triggers 2 and 3 fired on this capture (both on Continues). The closeout commit then **added a reading rule** (`§28.21.7`): read a trigger-2 hit with the PSRAM figure; "if internal + PSRAM falls by more than 4 KiB across the window, that is retention: escalate at once. If the sum is conserved, it is placement: record it." The 23,552 B floor became the reference for the next capture.

### 13.5 The RC1 capture

The RC1 phase ran the candidate image (`FW 3.0.0-alpha3-rc1-debug`, `Git f85575b96f05`) with a 6,318-line capture whose boot (7.4–99.9 s) a PowerShell `Tee-Object` lost, taking `IDENTITY`, `AUDIO_PACK` and the boot inspect with it; the identity is the user's report of the device screen. Results: watchdog, crash and reboot lines 0; storage errors 0; 71 of 71 audio windows clean; 13 System Menu opens at 180–190 ms (median 180); 15 cached inspections at 82.4–83.1 ms retaining 0 B; save/load and a New Journey without error.

**A keyboard-controller incident.** `ESP_ERR_INVALID_RESPONSE` on 3 of 10,248 reads, in two incidents (two in a row at 186.56 s, one at 195.36 s). `KEYBOARD_RECOVER` ran the staged recovery (bus reset, raw mode once, baseline) to a usable state both times, `INPUT_RESYNC` abandoned only held gestures, and `INPUT_QUEUE` reported `dropped=0 queued=836 consumed=836`. It is recorded as a successful recovery with no blocker ID. The A3-04G capture had one such error in 5,720 reads.

### 13.6 The live-save-document finding, and why A3-04H was deferred

A3-04G had proposed A3-04H — remove the redundant imports (`verify_candidate` imports the GAM twice, Continue Latest seven times, and a save deep-copies the live document) "at the pinned core layer (7 → 2 on Continue, 2 → 1 per verify)" and route the document to PSRAM — gated to start only "if the RC heap capture fires a §28.16 trigger it cannot explain".

The RC1 capture fired two. **Trigger 3**: largest internal block 30,720 B in 13 windows, above the accepted 23,552 B floor. **Trigger 2**, once, on the gameplay save: internal free fell from 196,419 B to 144,283 B, **−52,136 B**, with PSRAM unchanged. `AUDIO §37.3` (`a3-rc1-heap-adjudication.log`):

| Step | internal | PSRAM | internal + PSRAM |
|---|---:|---:|---:|
| first heartbeat, 102.21 s | 55,923 | 6,081,064 | 6,136,987 |
| after the boot Continue | 198,599 | 5,937,432 | 6,136,031 |
| pre-save plateau, 187.31 s | 196,419 | 5,937,432 | 6,133,851 |
| **after the gameplay save** | **144,283** | 5,937,432 | 6,081,715 (−52,136) |
| after loading that save | 160,271 | 5,932,400 | 6,092,671 (+10,956) |
| after a New Journey, 440.97 s | 168,223 | 5,964,100 | 6,132,323 (+39,652) |

**It is not a leak.** `AlphaRuntime::retained_` is the *live save document*. A save's `capture_save_document()` re-captures the gameplay, terrain, NPC-walk, world-object, moonstone and dungeon sections into it **in place**; `export_native_state` then deep-copies it, and that transient copy drives the low `heap_int_min` values (376 B in a load, 168 B in the save, both inside storage windows). The live document stays alive by design until a later load or New Journey replaces it, and the sum recovered in two steps, each time it was replaced ("a retained allocation cannot come back"), ending 4,664 B below its first value with no slope. The finding also explained A3-04G's "recorded, not explained" +39,120 B on a save: the same rewrite in the other direction.

**The rule was amended at the gate.** Under the `04d977c5` reading rule, a −52,136 B move across a window "is retention: escalate at once". `§37.3` says the rule "assumed that a save conserves the sum. It does not", and replaces it for saves: a load, settings or inspection window is still judged by its own before and after, but **a save window is judged by recovery** — the sum before the save against the sum after the next clean load or New Journey, plus the absence of a downward slope across repeated warm saves. Git establishes only this: the reading rule was committed on 2026-09-27 (`04d977c5`), and the capture, the adjudication and the amendment first appear together in `eb1bf5e7`, so git cannot order the amendment against the reading of the capture. The document argues the amendment in full; the residual −4,664 B is called "live state" but not itemised.

Verdict: **"RC1 HEAP GATE PASSED — A3-04H DEFERRED."** The heap watch item "is closed for Alpha 3"; the placement trait remains "a known characteristic, not a release risk". Beyond the misses already noted, the record also keeps a tool label ("falling") in the RC1 summary that could be mistaken for trigger 4 (which needs 20 opens), and A3-HF4's discovery, during the storage hardware run, that a pending prompt survived a load (§14.1).

## 14. The hotfix series (A3-HF1 → A3-HF10)

The hotfixes were not consecutive: HF1 landed between A3-04B and A3-04C, HF2 and HF2.1 between A3-04E.1 and A3-04F, HF3 after A3-04F, HF4 after A3-05, and HF5–HF10 in one run from 23:25 on 2026-09-27 to 12:15 on 2026-09-28. The common shape: the user notices on the T-Deck something the original did not do; the batch reads the binary, finds the native difference, fixes it at the layer the parity fixtures pin, proves it RED-first and by mutation, and records a hardware check. From A3-04A on, every code commit is followed within minutes by an evidence-only "post-commit firmware build logs" commit, built fresh with `--no-ccache`, showing a zero size diff against the pre-commit image and the image guards green; the tag names that post-commit image, because the image embeds its own commit hash. Image sizes and section deltas are in §17.1.

### 14.1 HF1 to HF4 in brief

- **HF1 (`d1465ed7`, 09-26 21:46, D-61/H-197).** After a troll fight by the Britain bridge, every Get toward the reward chest answered "Nothing to get!". The original's Get (`SJOG 0x1458`) saturates a counter and always clears the pile; there is no "pack full" test. Native refused when a counter was at its cap — a rule invented in Batch 2 and listed in AUDIT as "deliberate and correct: award first, erase second", with no citation. The Developer presets cap every counter, which is why they exposed it (an inference: the device's counters were not captured). RED 6/26; 12/12 mutations, after a first pass left six multi-line anchors unapplied because the checkout is CRLF and the driver was made to follow each file's line endings. H-197 PASS on 2026-09-27 (image not recorded). Written up in AUDIT, not AUDIO.
- **HF2 (`3494752f`, 09-27 13:05, D-62/H-198) and HF2.1 (`94993d3d`, 14:24).** Beside a grandfather clock every move was followed by a strike. The original re-arms the strike only when the hour changes (`advance_clock 0x4f7c`, compare at `0x514a`). The wrong model came from `ambient-audio-audit.md` §5.1, which had flagged itself as odd and asked for a witness, and both the TypeScript skin and A3-03's own test pinned it. A new census tool found every instruction naming the clock's data-segment address (its positive control found exactly the three known references). The fix is one line: the re-arm key is year/month/day/hour. The reported fountain "thinning" was not reproduced. HF2.1 corrected the TypeScript skin — "brought into line with the bytes and with native, never the other way round"; no fixture pinned it — and made `PRESENTATION_DISPATCH` log on change only, after it was found to fill 24–31% of serial lines. RED 3/10 (HF2); 6/9 TypeScript and 1/7 native (HF2.1).
- **HF3 (`c3738572`, 17:50, D-63/H-201).** In H-200's fight the tester could not tell who was hit. `kernel_combat_hit_flash 0x3564` blits tile 0 (the explosion star) on the struck cell, XORs the member's roster row, plays a noise burst for 174.38 ms (9,000 half-samples), and only then applies the strike, so a killing blow is cued before the death. Native had the roster inversion (Batch 7B) but only the poison tick produced it. New `CombatHitCuePacer`; RED 13/24; 19/19 mutations; one render golden deliberately widened to the states that visibly carry a cue. H-201 passed on the A3-04G image, not an HF3 build.
- **HF4 (`cfd4ab3e`, 22:30, D-65/H-204).** A Mix picker opened before an in-menu Load survived the load, and Enter then mixed from the *old* game's list in the *new* game — and not only Mix (L2–L5 were RED too). The original loads only at start-up, where no prompt exists; TypeScript's `applyLoadedState` drops every prompt, view and pending command. New `reset_transient_after_load()` hands the UI to `UiSession::reset_after_load()`, answers and charges nothing for the old game, and logs `LOAD_TRANSIENT_RESET`; a failed load never reaches it. RED 36/85; 29/29 mutations. H-204 passed on 2026-09-28 on the A3-HF10 image; the HF4 image was apparently never reported separately.

### 14.2 HF5 — TALK dialogue pacing (`a8c2f676`, 09-27 23:25; D-66/H-205)

Chuckles' routine in Lord British's castle and Blackthorn's speech arrived "in one frame"; the original lets them unfold. (HF5's write-up says "all at once", not "too fast"; the latter is Batch 51's phrase for the Camp apparition.)

**Original.** No typewriter: the interpreter prints through `TALK 0x0574` without delay, so the unit is one script section. `0x83` **Pause** (`TALK 0x0f92–0x0fb3`) is `run_n_frames(28)` with a key exit — 28 ticks, 1,538 ms, the key consumed. `0x8F` **KeyWait** (`0x1010`) is `getkey_with_redraw 0x266c`: any key, no timeout. Nothing after a pause runs until it ends. 116 of the 135 TLK scripts carry 166 Pauses and 225 KeyWaits. Reading TALK's kernel calls needed base `0xBF80`, not the tool's `0xA290` (§7.2).

**Native.** `Conversation::flush()` recorded the opcode on every line (`DialogueOutput::pause`, even hashed by `dialogue_parity`), but nothing downstream read it; all nine rows of Chuckles' routine were in the transcript at t = 0–5 ms.

**Fix.** New `DialoguePacer`, a presentation-only queue owning no rule. The hold sits at the *front* of `consume_event()`: a paused line goes out and starts a hold (Timed, or Key until a key); every later event of the turn is queued in order; on release the queue drains through the unchanged `route_event()` up to the next paused line. **Nothing is dropped**: an event the pacer cannot own first releases everything queued, in order, then is handed back (`DIALOGUE_PAUSE collapsed`). `kTalkPauseMs` = `run_n_frames_ms(28)` = 1,540 ms (the reference's 1,538 recorded as drift). During a pause any key ends it and does nothing else — Mic included. The System Menu blocks releases; a successful load cancels the queue. Storage: 64 steps of 116 B plus a 4 KiB text arena, **11,520 B of PSRAM**. A latent defect surfaced: an NPC approach queued in the same turn was dropped by the drain, and now waits for the last line.

**Evidence.** 50 checks on the real runtime and Board with the real TLK corpus (pauses at 1,544 / 3,086 / 4,630 / 6,173 ms); RED 23/50; 26/26 mutations, after a survivor (a pause-ending key also reaching the game) was killed by a Mic-key check — in the Dialogue base mode only Mic acts. H-205 PASS on 2026-09-27 (image not recorded). The image pushed the Launcher allocation past 960 KiB (§17.2).

**Residue.** The original runs an effect (gold, karma, a guard call) *after* its pause; native applies it when the command runs and only the text waits. The `Enter: continue` cue on a KeyWait is a native modernisation.

### 14.3 HF6 — the shrine rite and the Codex wait for a key (`da249b38`, 09-28 00:07; D-40/H-183 → H-206)

The mantra's "The Altar speaks and a Quest is ordained!", the Codex lesson and the Codex's pages arrived in one frame — Batch 51's H-183.

**Original.** Every `E8` in `CAST2` resolved through base `0xE1E0` gives 15 calls to `0x266c`: **eleven are the rite** (`0x0a9b`, `0x0abc`, and nine in the Codex handler `0x0d24`); four are not waits (the donation's digit loop, Quit and Save's Y/N). In `0x0966–0x0e76` there is no flush, `delay` or `run_n_frames`: one key ends one getkey. "WELL DONE!" has no getkey. The tools' `0xC29E` put the getkey call mid-instruction (§7.2). The RE note was also corrected: `0x2032` is a `toupper`, not the poll.

**Native and fix.** `shrine.cpp` emitted a bare `ShrineKeyWait` at seven `d.wait()` sites (eleven getkeys at run time) and nothing consumed it. `paced_event_pause()` is HF5's `dialogue_event_pause()` plus `ShrineKeyWait → Key`; Blackthorn's capture scene emits the same marker for its own getkeys, so a gate keeps it away from the dialogue pacer. `bind_shrine_services()` was extracted: the host fixture had redeclared those hooks with a lookup returning `nullptr`, so on the host the ordained branch and every Codex page were `InvalidContext`. No new allocation.

**Evidence.** 48 runtime and 15 pacer checks, RED 19/48 and 8/15; 22/22 mutations. A harness bug surfaced: the fixture never copied the pack's title art, so the first real-Board test to reach the title segfaulted intermittently — never under gdb. H-206 PASS "reported by 2026-09-28"; "the image's FW / Git were not reported".

**Residue.** The Codex's visited bit is set when Enter is handled, not after the third getkey (a save inside those getkeys records it one early).

### 14.4 HF7 — the ritual's viewport negative and the Codex pulses (`c76b9ef5`, 09-28 00:48; D-41/H-184 → H-207)

"ALAKAZAM!", "WELL DONE!" and the Codex ceremony showed no inversion; everything after them arrived in the key's frame, and the Codex's three shakes merged into one.

**Original.** `CAST2 0x0bbc–0x0d1a`. The rectangle (8,8)–(0xb7,0xb7) is the 176×176 viewport; `0x0b86` sets carry so the EGA driver (fn21) programs its graphics controller to XOR each pixel's *palette index* with the `set_color` value — not a palette swap, and not the panels. From `INTRO.OVL 0x09f4/0x09fa`: the rite XORs 15; the Codex XORs 4, 15, 4, and because the shake never calls the compositor the pulses accumulate to 4, 11, 15. Holds: the donation's sweeps 184,000 samples (7,130 ms at the floor), WELL DONE's 138,000 (5,347 ms), the restore `run_n_frames(10)` 550 ms; no key is read and the world does not tick in between. The TypeScript `ritual-invert.ts` printed reward lines at once and never wired the Codex bracket; the binary won.

**Fix.** New `RitualFx` (a viewport XOR mask, a pending negative, a Codex pulse count) and an **Effect** state in `DialoguePacer`: a timed hold no key ends. The Codex's three `Quake` events carry their colour in the existing `note` field, so `quest_parity` is untouched. Production +251/−17; PSRAM unchanged.

**Evidence.** 48 runtime checks reading panel GRAM (fixture tiles use palette entry i = i·0x1111, so a frame's XOR mask is the mode of index^index) plus 23 model checks; RED 25/48 and 12/23; 24/24 mutations. One HF6 expectation was corrected: "A STRANGE WIND…" prints after the three shakes. H-207 PASS on 2026-09-28 on the HF7 image (hour not recorded).

**Declared divergences.** Keys inside an effect are swallowed rather than kept as DOS typeahead; the shake is the device's 936 ms; the device's sky and wind strips inside the rectangle are not inverted; the hypothesis that `[0x13ae]` is 0 stays open.

### 14.5 HF8 — the Blackthorn sacrifice burst (`602fa171`, 09-28 01:43; D-43/H-186 → H-208)

After the siren the victim simply went dark; no explosion was drawn.

**Original.** `BLCKTHRN sacrifice_member 0x03ae`: text; `run_n_frames(10)` (550 ms, victim drawn); the siren as 2 × 460 `tone_sweep`s (7,130 ms); one call to `explosion_fx_at_cell` (`0x3522`) at `0x041e`; and only *then* the victim's tiles are zeroed (`0x0421`). The burst is tile 0 with `noise_burst(0x7d0,0xbb8,0xa)` — 4,500 samples, 174 ms, the same arguments as an enemy being hit — and because the scene sets `[0x5893] = 0xff` it lands on the scene cell (5,7) or the companion's seat (7,5).

**Native and fix.** `blackthorn.cpp` emitted the burst after the whole script; the pacer released it after the victim's clear to `UiSession`, which ignores it — wrong order and never drawn. The host fixture nulled `capture_tiles` and the script, so the throne room could not even be staged. A burst beat now sits between siren and clear in `build_sacrifice_script()` (`kBlackthornBurstSamples`, 4,500), mapped to the existing combat-hit noise program; `bind_blackthorn_scene()` was extracted and the fixture stages the throne room.

**Evidence.** 51 runtime checks: the burst exists in three cases (pendulum, betrayal at once, betrayal after a warning), on the binary's cell, tile 0 pixel for pixel, starting 7,685 ms in (want 7,680 ± 11) and lasting 175 ms (want 174 ± 11), victim dark only after. RED 33/43; 15/15 mutations. H-208 PASS on the HF8 image.

**New residue — H-209 / D-67.** The *shard ritual* calls the same `0x3522` seven times (`CAST 0x16e1–0x16fa`): seven 174 ms holds with bursts. The device paints 60 ms on / 60 ms off, silently.

### 14.6 HF9 — the Refuge's cadence and its karma getkey (`4802262f`, 09-28 10:19; D-42/H-185 → H-210)

The party-wipe Refuge ran on an invented "Class C" clock (70 ms per raw delay unit plus 900/260 ms reading floors): the 10-second slumber held 900 ms, the darkness dissolve stretched to 1,600 ms, and Lord British's karma speech left by itself after about 1.5 s where the original waits for a key.

**Original.** `BLCKTHRN party_refuge 0x0910`, seventeen beats: `delay(10)` (550 ms) before *any* line; "An unending darkness engulfs thee…" over a black viewport; `delay(14)` after "Thou hast found refuge."; `delay(28)` after "No evil lives here"; six sweeps of 260,000 samples (10,075 ms) for "thy slumber is disturbed"; figures fizzling in at (2,7) and (8,7) with `delay(4)`; "a peal of thunder" with two 936 ms shakes; the apparition; the karma speech ending in `getkey_with_redraw 0x0b3e` (no timeout — though a key pressed during a busy loop waits in the BIOS buffer and satisfies it at once); then one 1,162 ms revival tone per member. The holds before the key sum to about 15.8 s (computed). The TypeScript reference had a `delay(10)` in the wrong place, three delays no instruction backs, and no getkey; it was fixed first, and HEAD's `game.ts` against the new native script fails `quest_parity` at mismatch 5147.

**Load reset.** A load during the scene cancelled the pacer but left `refuge_pending` set, so the next wipe would resolve with no scene and no speech; `reset_transient_after_load()` now clears it. A save during the getkey records the fallen party, since the original cannot save inside a getkey.

**Fix and evidence.** `RefugeBeat` gains `key_wait` (pinned by parity) and the device's holds (`sweep_samples`, `fizzle`, `shake`); `NarrativeScenePacer` gains `awaiting_key()`/`advance_key()`; the Class-C constants are removed. 43 core and 61 runtime checks, RED 30/43 and 28 on the runtime (recorded both as "28/37" and "61, 28 RED" — the runtime test stops early where HEAD never reaches a getkey); 23/23 mutations (two first-pass mutants did not compile). H-210 was run on the **A3-HF10 image** in the one-flash session.

**New residue.** **H-211 / D-68:** the stage content — in 1988 `0x098f–0x09ca` hides the window and clears every object, the Avatar is placed only at `0x09f5–0x0a07`, and the final black fill leaves the Avatar `0x11c` alone; native shows the Avatar from the darkness line and keeps the figures through "Vertigo". **H-212 / D-69:** the device's shake pushes whole-viewport pixels only on full frames, so over a static viewport it reads as **one 2 px drop held for the shake**, not eight pulses — pre-existing for every quake, found by a runtime test.

### 14.7 HF10 — Mix parity (`028269fe`, 09-28 12:15; D-6/D-70/H-213)

The device's `Custom` arm filled the reagent mask from the spell's own recipe and dispatched at once with quantity 1: no picker, no "How much?", no "No reagents owned!", and no way to choose a wrong reagent, so the chest trap was unreachable (D-70). D-6 had recorded only the quantity half; D-70 was found by the 2026-09-28 reconciliation. H-53 had passed on Batch 20 hardware with "expected residual: quantity is always 1".

**Original.** `CMDS 0x1ae0` sums the eight counts (none → "No reagents owned!"); `0x1b06` asks "For what spell?". The picker at `0x18be` lists only reagents with a non-zero count, in id order; the mask starts at 0; **RETURN or Space toggles**; `M` returns the mask; ESC returns −1 (no message); every other key is re-read. "How much?" is the kernel getnum (`0x3b9e`): at most two characters, a sign only first, backspace deletes, and **ESC erases the buffer and keeps reading — there is no cancel; only RETURN leaves**. A marked reagent with fewer than n (unsigned) prints "Insufficient reagents!" and re-asks; `n ≤ 0` ends silently; mask 0 prints "Nothing to mix!". Then n is **subtracted from every marked reagent before the recipe test**; an exact match gains n charges (capped at 99), anything else springs the chest trap `0x2fd0` on the first conscious member. An extra, a missing and a wrong reagent are all "wrong": spent, no charge, the trap.

**Fix.** The TypeScript picker already matched; four getnum details did not (ESC and backspace-on-empty cancelled, a leading sign was refused, a negative answer impossible) and were fixed in the reference too. Native gained two request ids appended after `Custom` (`MixReagents`, `MixQuantity`) and a pure core predicate `mix_quantity_short()` that is `0x1a70`'s test; the core Mix command is unchanged, and only a valid answer reaches it. Load reset clears a pending Mix.

**Evidence.** 71 runtime checks, **54 RED** on the unmodified tree (four of the 17 GREEN pass "by coincidence" through HEAD's own auto-mix of In Lor × 1), 8 of 22 TypeScript tests failing; **28/28 mutations** (one invalid first-pass mutant tripped `-Werror=array-bounds`). **162/162**. H-213 PASS on 2026-09-28 on the HF10 image, with H-204, H-203 and H-210 in the same session.

**Residue — D-71.** The 10-tick wait after "Mixing…", the "Mix Reagents" echo, the three console lines before the picker, and a spell picked from a list rather than typed. AUDIO §36 calls HF10 "the last gameplay-code batch before the Alpha 3 RC".

Each of the last three hotfixes found residue while deriving its own fix — HF8 D-67, HF9 D-68 and D-69, HF10 D-71 — which is part of why §18.4 happened.

## 15. Notable debugging stories

The defects whose debugging is worth keeping as stories. Five told in full elsewhere are only indexed here: the mutex in a function-local static (§12.4), the invisible diagnostic and the SD/TFT bus (§12.5–§12.6), the watchdog regression (§12.7), the System Menu latency (§13) and the live save document (§13.6).

### 15.1 The Destard freeze and the Deceit pit chain (Batch 21A, 21A.1)

Batch 20's "room-entry freeze family" turned out to be three different things. The real defect (H-151, Destard) was arithmetic: `combat_growth_reserve()` returned `max(count, 63) + 1 = 64` whenever any actor had the divide-on-hit ability bit, and every caller tested `capacity − count < reserve` against 54 actor slots. The test was permanently true, every combat command returned `NeedsActorStorage`, and nothing happened. Only `Alt+D`, Save and Load still answered, and only a power cycle escaped. Two shipped monsters carry the bit (Slime, Gargoyle); seven of the 112 authored rooms place them. The fix states the growth one action can cause and makes `spawn()` refuse past capacity — faithful, because the 1988 combatant table is a fixed 32×8.

The other two were "authored". H-149 was Deceit level 8 (5,5), the only chest cell, whose neighbours are three walls and a secret door at (5,4); a party arrives there by falling through a pit chain and "Blocked!" in all four directions is correct, the exit being (S)earch. H-150/H-152, the Set Active Player auto-pass, was original turn order. **The Batch 21A.1 report** (CHECKLIST row H-153; the audit's 21A.3 heading and the 21A.3 tag use H-153 for a different fix, see Appendix B) was the user's report — with photographs — of stepping Klimb Down from Deceit L1 and arriving on L8 among Sea Serpents. The batch proved it was a six-deep pit shaft: floor 0 (1,3) is a trap cell (down-klimbable), floor 1 (1,3) is a pit, `enter()` runs the pit chain (`DUNGEON 0x0A4C`) and keeps falling while it lands on pits, six falls in all, stopping at floor 7 (1,3), room 10, where four sea-serpent sprites are placed and no in-arena klimb tile exists. A "wraparound" hypothesis was tested and disproved: the stored floor byte was `0x07`, not `0xFF`, though the HUD clamps to 8 and could not tell them apart. No production code changed; the test grew from 124 to 169 checks so that the next person cannot mistake a wrap for a fall.

### 15.2 The dungeon series (Batches 9–9E)

Each fix exposed the next layer. Batch 9 fixed eight presentation defects (no light gate, movement rules reused as sight rules, no front/side classification, features drawn only underfoot). The first physical session after it (9B) found that the dungeon key map was a subset of the reference: Ignite answered "What?", so a dark dungeon could not be lit; three routes had no callers; and the HUD caption read "Serpent's Hold" inside Deceit. Batch 9C packed the authored art. The next physical session (9D) found that 9B's own prompt mirrors had no production caller — "the host suite proved a rule the device never ran" — plus a freeze after teleporting out of a fight. Batch 9E concluded that a reported "sealed 1×1 cell" was faithful (a census found 12 of 14 sealed rooms carry an in-arena escape tile), and corrected the batch's own earlier mis-identification of the room. 9E's commit body refers to a "previous Batch 9E commit" that added a return mechanism and was "removed in full"; `git log -S` finds only one commit, so that earlier revision was never committed.

### 15.3 The "Move lock" that was not the ritual (Batch 25, H-118)

Symptom: Use on a shard printed only "Use item", then every direction was silent, across teleport and load; Look, Z-stats, `Alt+D` and `Alt+M` still worked; only a power cycle recovered. It looked like a broken shard ritual. The batch ruled that out first: the original prints its header before any gate and native matched it on every path, so a silent Use meant the dispatcher had *refused* it. The gate that refuses every world command is `exit != awaiting_exit`. In 1988 a "Dost thou wish to leave?" question loops on a getkey, so the command loop cannot run with the question open; the port splits that loop into a flag plus a UI modal. `Alt+D` parks the modal in a return register, and `set_base_mode()`'s Developer branch overwrote that register unconditionally, unlike its ordinary branch. Closing the menu dropped the question and left the flag set, and the same applied to "Pay toll?" and Blackthorn's prompts. One condition in `ui_session.cpp` fixed it. Which prompt was on screen when the tester pressed `Alt+D` "is not recoverable from the Batch 20 notes"; the deduction rests on three code facts. H-165 (Batch 31) was the dungeon-teleport sibling; the same shape reappeared in HF4.

### 15.4 Dungeon saves that never reached the card (Batch 26, H-115)

After a dungeon save, a load reverted to an earlier one. Batch 20 blamed a restore path. The real cause was that `export_native()` copies only the keys in `persistence.cpp`'s `extras[]` whitelist, and `"dungeon"` was not one — so every dungeon save loaded at the entrance on the surface. Batch 6's test had round-tripped the in-memory JSON, which contains the key, and so had passed. It took 20 batches to surface. A test that stops at the layer before the whitelist cannot see the whitelist.

### 15.5 The bed pose and its disposition (Batches 45–50)

A physical observation at about 23:45 — an upright occupant standing in a bed while another bed looked empty and Look identified a guard — began the arc. Batch 45 found a real native defect (`compose_world_presentation` required `schedule.dialog != 0`, so a silent guard was present but never drawn) and made the stronger claim that the *upright pose* was original. Batch 47 rendered all 512 `TILES.16` tiles and found tile `0x11a`, a person reclining in a bed; it traced the terrain-sensitive pose selector at `ULTIMA.EXE 0x51b8–0x5391` and counted 264 bed-head cells in 29 locations, then asserted that ordinary NPC types ≥ 0x40 bypass the selector. Batch 48 re-read the opcodes and found the selector admits actor bytes `0x40–0x7f` after all; it fixed only the controlled actor. Batch 49 traced the shipped bytes for the two authored NPCs, showed the inputs are consistent with `0x11a`, and tried to run the original under DOSBox-X with a patched save; the run produced no capture (§7.6). Its verdict: the guard-visibility fix stands; the claim that the upright pose is original "is withdrawn pending an original witness". D-38 was still open at release and is on the Alpha 3 deferred list. Batch 50, in the middle of the arc, was cancelled because the "physically reproduced" bed defect was a stale image (§8.3). The arc contains a reversal (47 → 48), a withdrawal (45 → 49), an experiment that failed, a process failure, and an honest "unresolved".

### 15.6 Camp, the watch and the apparition (Batches 32, 40–44, 46)

The Camp watch (H-167, Batch 32): the original asks "For how many hours? (1–9)", and the watch prompt appears only if at least two members are in status G or P (good or poisoned); a guard is valid only if status G, otherwise "None posted!" with no retry; the guard cell comes from the authored CampFire arena. The device never posted a watch, so an earlier row (H-160, "watchman walks through the fire") was reclassified as unreachable. Advancement (Batch 40): an audit that changed nothing, because native matched — `camp_results` reaches the level-byte write only on a successful 25% apparition gate — but found a presentation gap; Batch 41 staged advancement per member with one unfiltered key per Hail. The apparition (Batches 42–46, kernel `0x6936`, `0x6a13`): `OUTSUBS 0x06b9` materialises tile `0x174`; class standing tiles come from a `DATA.OVL` table at DS `0x1ade`; white index 15 is XORed over the 176×176 window once per live slot; the pre-apparition scene mounts on *both* gate outcomes; each living ring wearer rolls `rand(0,15)` and an 11 clears the ring; a surviving ring-42 wearer is drawn as an outline (render byte `0x1d`, invisible flag `0x10`, compositor adds bank `0x100`), not omitted and not XORed. Batch 44's RNG-order requirement — rolls interleave per member in roster order, before the scene mounts — is the kind of detail only a parity fixture would catch.

### 15.7 The cannon-killed NPC that would not despawn (Batch 33, H-168)

`CMDS 0x0B16` scans cannon tiles `0xB4–0xB7`; on an NPC hit `0x0D47–0x0D82` call `0x7AF4` and then three town routines that set the dead bit (for eligible types) and clear the slot. Native kept the actor in `NpcActors`; the TypeScript cannon branch had the same omission. The batch also corrected the note that had claimed `0x7AF4` was an HP roll (§9.4). Test 19/19 (RED 12/19), five mutations.

### 15.8 The moongate that would not return (Batch 53B, H-195)

After a first transit, stepping back onto the gate did nothing. Three rival explanations were on the table — a guard that skips the just-arrived gate, a broken transit, a visibility mismatch — and the audit rejected all three by reading `MAINOUT 0x0b00`: the outdoor loop calls the moongate check (`0x48a8`) at the top of every iteration, the destination is a function of the clock alone (`0x4962`), and there is no return trip. Same phase, same stone, so stepping onto the arrival gate transits onto itself; in Alpha 2 that reads as "nothing happened" only because the transit animation (D-48) does not exist. The finding stands in the Alpha 3 known differences as "not a bug". It also produced two ledger rows (D-58: a party standing on a gate is not re-fired; D-59: the moon-phase latch is never refreshed on the device), and it exposed the wrong `MAINOUT` base in `callers_banda.py` (§7.2).

### 15.9 Input: the Mic key, the keyboard controller and the starved idle task

Three different failures shared the word "input". **The Mic key** was mapped to matrix (4,4) on the strength of a vendor character table (Alpha 1.2, 1.3), then retracted in Alpha 1.4 after a physical trace put it at (0,6) (§5.2). **The keyboard controller** returned transient I2C errors that, in Alpha 2.0, became permanent because the failed bus state was retained; staged recovery fixed it, and the RC1 capture recorded three errors in 10,248 reads recovered without loss (§13.5). **The starved idle task** was A3-04E's watchdog regression (§12.7). A related 75 ms `INPUT_SERVICE_GAP` diagnostic, added in Alpha 2.0, separated main-loop starvation from controller errors.

### 15.10 The chest trap that sounded like a death (A3-02 → A3-05)

A3-02 attributed a "combat defeat" sound to `0x2fe3`, from an early catalogue pass that a later note (`cmds.md` §8) had already corrected but that was copied into A3-02 anyway. `0x2fe3` is the first call of the chest trap `0x2fd0`. So a kill played two 174 ms bursts. A3-05 removed the second (D-64): the original's kill sounds only `0x3564`'s hit burst, since the strike and the " killed!" branch make no speaker call. The TypeScript reference has the same misattribution; it was recorded and not changed.

## 16. How automated coverage grew

### 16.1 Harness generations

Counts from different harnesses must not be added. In the order they appeared:

| Harness | First in git | What it is |
|---|---|---|
| Upstream parity and unit tests | `1225f9ac` | The imported project's model-versus-TypeScript parity suite (README: 239, later 276) and vitest unit tests (5,080 and 4,965 in commit bodies). Never combined with native counts. |
| Native CTest (host) | `8a603f8f` | 14 `add_test` lines (measured), e.g. `command_parity`, plus TypeScript fixture-drift tests through Node. |
| Differential drivers | `c18f5b64` | `gameplay_parity` (5,058 sequences), `quest_parity` (5,377 cases plus 2 bounded non-terminating observations), later `magic_parity` (25,088 cases). |
| Real device code on host | `3c2d437a` (Batch 11), `4269266a` (A3-04E), `c8fee48b` (A3-04G) | `AlphaRuntime` over ESP-IDF shims; `tdeck_board.cpp` over a fake ST7789 and SPI timing model; `alpha_save.cpp` over a fake SD card with fault injection and a heap census (§4). |
| Scripted mutation checks | `6edbb7de`, Batch 40 | Earlier mutations were manual (Batch 13 six, Batch 15 four). |
| RED-first scripts | `dcea9539`, A3-04F | Apply only the minimum edits needed to link and show the new tests fail on the unmodified tree. |
| Firmware image guards | `e48abb8d`, A3-04A | ELF checks: no hot-path guard, IRAM-resident audio path, image sections. |

### 16.2 The native host suite

Native CTest totals, run serially from Batch 22 on (the first `ctest -j 6` baseline had a `gameplay_parity` segfault on unmodified code). Sources are `VALIDATION.md`, commit bodies and tags; totals from Batch 17 on were re-read from every tag for this history.

| Date | Point | Tests | Note |
|---|---|---|---|
| 09-15 | `8a603f8f` (`VALIDATION.md` history) | 4 → 6 → 8 → 10 → 12 → **14** | foundation 4/4; world-turn 6/6 (22,877 generated parity cases, 11 extractor tests); travel 8/8; commands 10/10; items 12/12; combat 14/14. All first committed in `8a603f8f`; their dates are not recorded. |
| 09-15 | `fd2a2036` (`VALIDATION.md`) | 22 → 31 → 34 → **35** | combat magic, dungeon/world, dialogue, persistence |
| 09-18 | `c18f5b64` | **49** (`VALIDATION.md`, dated 09-16) | intermediate 42 and 44; 61 `add_test` lines; the Alpha 1.x/2.0 documents in the same commit give 27, 50, 51, 53 (Appendix B) |
| 09-18 | audit baseline | 57 (56 pass) | `gameplay_parity` mismatch 59 |
| 09-19 | Batch 4 | 63 (62 pass) | mismatch 2034 (R-21) |
| 09-20 | Batches 7–10 | 69–77 | two failures: `gameplay_parity` and the `quest_parity` crash |
| 09-21 | Batch 13 | 81 (80 pass) | `gameplay_parity` 5,058/5,058 |
| 09-21 | Batch 17 | **85 (85 pass)** | "first fully green run"; the "two known baseline failures" convention retired |
| 09-21 | Batch 19 | 88 | |
| 09-22 | Batches 20–27 | 88 → 97 | about one CTest per batch from 21A |
| 09-23 | Batches 28–40 | 98 → 110 | |
| 09-24 | Batches 41–49 | 112 → 118 | 47 and 49 add none |
| 09-25 | Batches 51–53A | 120 → 122 | |
| 09-26 | **Alpha 2 release** | **123** | 134.3 s pre-commit; 101.4 s on the committed tree |
| 09-26 | A3-01 → A3-04D, with HF1 | 126 → 141 | A3-01 126, A3-02 129, A3-03 131, A3-04 133, A3-04A 134, A3-04B 136, HF1 137, A3-04C 139, A3-04D 141 |
| 09-27 | A3-04E → HF5 | 143 → 154 | A3-04E/E.1 143, HF2 144, HF2.1 145, A3-04F 146, HF3 148, A3-04G 149, A3-05 151, HF4 152, HF5 154 |
| 09-28 | HF6 → HF10 | 156 → **162** | 156, 158, 159, 161, 162 |
| 09-28 | **RC1 and release** | **162** | 139.25 s at RC1; 134.68 s rerun at release, no rebuild (`a3-release-ctest.log`) |

Serial runtimes rose from about 107 s at A3-01 to about 153 s at HF9 and HF10. `add_test` lines exceed the CTest total (127 at the Alpha 2 release, 166 at the Alpha 3 release, measured) because they include Node-conditional tests.

### 16.3 RED-first and mutation

**Every fix was proven RED first** on the unmodified tree, usually in a worktree, and reported as a ratio — Batch 13's 58 → 0, Batch 24's 35/47, and in Alpha 3 from HF1's 6/26 to HF10's 54/71 (§14). Where a batch's test could not be RED (Batch 19's `batch19_command_char` is "a lock validated by mutation"), the batch said so. **Every fix was then mutated**: from about Batch 21A.3, 5–15 mutants per batch; in Alpha 3, 10–50, every count "all killed" after first-pass survivors, invalid mutants and equivalent mutants were disclosed (A3-04E.1: 50 of 50, 49 on the first pass). Mutation testing mostly found faults in the *tests*: the HF5 Mic-key survivor (§14.2), the A3-04G equivalent mutant (§13.3), two HF9 mutants that did not compile, the Batch 53 survivor that added two checks (§10.3), and HF1's driver silently skipping CRLF anchors (§14.1).

### 16.4 Differential corpora and TypeScript

The parity corpora are the suite's strongest part and stable in size across the history: `gameplay_parity` 5,058 sequences (about 640 newly asserted after Batch 13), `quest_parity` 5,377 cases, 25,088 magic cases; `VALIDATION.md` counts 2,076,501 cumulative parity observations. Their limit is the one Batch 52 measured (§10.2).

The reference's own tests were touched only when a fix reached the reference. At release `tsc --noEmit` is clean and the whole `game/` vitest run has 7,728 tests, 7,525 passed and **97 failed — the same 97** as the A3-HF10 baseline (`native/core/a3-hf10-ts-base-fails.txt`), compared as a FAIL set, never as a total. Eighteen suites fail to load (11 "Invalid or unexpected token", 7 `ENOENT` on git-ignored `original/` or `re/` data). The record calls the 97 "environment-bound" and "recorded, not investigated".

## 17. Firmware and resource growth

Through A4-ENH2 the app partition was 1 MiB (1,048,576 B), ESP-IDF's stock single-app table; A4-FLASH1 (2026-10-02, `native/targets/tdeck/ALPHA4_UI.md` section 12) made it 1.25 MiB with a project `partitions.csv`, and that changes no Launcher install (Launcher sizes its own app partition from the image). The rows below keep the 1 MiB arithmetic of their time. All sizes are the Launcher image as the tag or document records it; "free" is 1,048,576 minus the size. Where the record does not state a figure, it is left blank. Deltas are against the row above.

### 17.1 The main line

| Point | Image (B) | Free (B) | Delta | Note |
|---|---|---|---|---|
| Milestone 2, `11e32392` | 298,064 | — | | bring-up |
| Milestone 5.1 | 339,728 | — | | |
| Alpha 1 | 634,848 | 413,728 | | first `UiSession` build; 26.15% DIRAM |
| Alpha 1.4 alpha2 | 671,600 | | | |
| Alpha 2.0 development builds | 692,576 → 784,720 | | | order inferred |
| Batch 3 | 812,016 | | | |
| Batch 19 | 860,320 | 188,256 | | |
| Batch 48 | 873,344 | | | last Camp/bed batch with code |
| Batch 51 | 874,864 | 173,712 | +1,520 | scene pacing |
| Batch 53 | 877,728 | 170,848 | +2,864 | release blockers |
| **Alpha 2 RC1 = release** | **878,752** | **169,824 (16%)** | +0 vs 53A | 98 bytes differ from 53A |
| A3-01 | 908,816 | 139,760 | +30,064 | audio architecture |
| A3-02 | 914,608 | 133,968 | +5,792 | |
| A3-03 | 920,128 | 128,448 | +5,520 | |
| A3-04 | 944,784 | 103,792 | +24,656 | music; the tag's "945,808" is a slip (§12.11) |
| A3-04A | 954,848 | 93,728 | +10,064 | documents say +9,040 (slipped base) |
| A3-04B | 963,088 | 85,488 | +8,240 | IRAM +5,692 |
| HF1 | 963,152 | 85,424 | +64 | |
| A3-04C | 970,128 | 78,448 | +6,976 | |
| A3-04D | 972,256 | 76,320 | +2,128 | |
| A3-04E | 976,336 | 72,240 | +4,080 | |
| A3-04E.1 | 977,392 | 71,184 | +1,056 | |
| HF2 | 977,360 | 71,216 | −32 | |
| HF2.1 | 977,408 | 71,168 | +48 | |
| A3-04F | 977,888 | 70,688 | +480 | `.data` −1,808 |
| HF3 | 978,912 | 69,664 | +1,024 | |
| A3-04G | 980,672 | 67,904 | +1,760 | |
| A3-05 | 981,616 | 66,960 | +944 | |
| HF4 | 982,432 | 66,144 | +816 | |
| **HF5** | **985,488** | **63,088** | +3,056 | + 11,520 B PSRAM; internal `.bss` +64 |
| **HF6** | **985,600** | **62,976** | +112 | flash `.text` only |
| **HF7** | **986,592** | **61,984** | +992 | flash only |
| **HF8** | **986,944** | **61,632** | +352 | `.bss` +16 |
| **HF9** | **987,216** | **61,360** | +272 | all sections unchanged |
| **HF10** | **988,320** | **60,256 (5.7%)** | +1,104 | |
| **RC1 = release** | **988,320** | **60,256 (5.7%)** | +0 vs HF10 | 104 bytes differ from HF10 |

Sizes for HF1, HF2, HF2.1 and HF3 are from their tags; the deltas and the free column in rows without a stated figure are computed for this history. Between Alpha 2 and the release the image grew by 109,568 B (computed) and free space fell from 169,824 B to 60,256 B. The largest single increments are audio ones (A3-01, +30,064; A3-04, +24,656) and the smallest are the hotfixes: HF6–HF10 together cost +2,832 B, of which HF7–HF10 are +2,720 B (computed).

### 17.2 Memory and partition facts

- **Launcher allocation.** Alpha 2's RC1 needed at least 917,504 B (896 KiB); the Alpha 2.0.0 development image needed 768 KiB. Alpha 3's RC1 needs "1,048,576 B (the whole 1 MiB partition; Alpha 2 needed 896 KiB)". The packager rounds up to 64 KiB. Computed for this history: an image over 917,504 B no longer fits 896 KiB (crossed at A3-03), and an image over 983,040 B needs the whole 1 MiB (crossed at HF5; HF4's 982,432 B still fit 960 KiB). The tripwire is `ALPHA3.md` §10: the image must stay ≤ 1,048,576 B.
- **Audio's static cost.** A3-04A: `.dram0.bss` +3,008 B. A3-04B: `.iram0.text` +5,692 B (the audio hot path moved into IRAM), `.bss` +1,456 B, and the internal heap begins 7,472 B later. PSRAM: the 16-song library is about 172 KB (21,529 events), and the HF5 dialogue pacer is 11,520 B.
- **Final sections.** `.text` 669,770 B; `.rodata` 219,460 B; internal `.data` 21,627 B; `.bss` 51,968 B; IRAM 16,384 B, full; PSRAM unchanged since HF6. "IRAM full" is the RC1 tag's phrase; `AUDIO` also quotes `.iram0.text` totals, so the terminology is not uniform.
- **Trigger 6** of the heap watch rules (a batch adding ≥ 1 KiB of internal `.data`, `.bss` or IRAM needs an offset) is the size discipline for internal RAM; later batches report their section deltas against it. The one recorded offset is A3-04F's: a first build had added 272 B of internal `.data`, and the batch's −1,808 B change more than offset it.

## 18. How scope control evolved

The history shows the project finding out, repeatedly, that the original had more behaviour than the port. The question that dominated the last two days was when to stop.

### 18.1 The early posture

In the audit and hardware-era batches the working rule was implicit: a difference from the original was a defect, and a defect was a row. Rows were cheap, and many Batch 21B–49 investigations queued the next (§9.4). The first *classification* is the ledger's, created in Batch 18: divergences (D-nn), intentional adaptations (A-nn), intentional enhancements (E-nn). Batch 19 corrected a Batch 18 summary that had counted 18 unresolved divergences as intentional; the ledger's standing rule, quoted again in the pre-RC reconciliation, is that "deliberate and unresolved rows are still never totalled together".

### 18.2 The first finite gate: Batch 52

The Batch 52 readiness audit (2026-09-25) defined eleven blocker criteria, **K1–K11**: a hard lock or freeze; save corruption or loss; impossible main-quest progression; impossible core transport, combat or dungeon progression; a wedged input path; a command the UI offers that can never execute; unreadable required gameplay from a renderer failure; a persistent wrong-context mode; a deterministic crash; a broken resource-identity gate; normal play silently mutating the wrong state. It named what is *not* a blocker "unless severity proves otherwise" — cosmetic mismatches, host-dependent timing, deferred audio or UI polish, original quirks, documented deliberate divergences, minor player-favourable divergences — and sorted the hardware rows into nine reconciliation categories (superseded, hardware pass, host-certified, deliberate divergence, original quirk, evidence gap, future version, release blocker, duplicate). The verdict went from NOT READY, four blockers, to READY about three and a half hours later on the commit clock. The criteria were why the answer was finite.

### 18.3 The Alpha 3 growth

Highest H- and D- identifiers anywhere in the checklist and ledger at each tag (computed for this history from the files at each tag):

| Point | Highest H | Highest D |
|---|---:|---:|
| `alpha2-batch52-readiness-audit` | 191 | 54 |
| `alpha2-batch55-release` | 195 | 59 |
| A3-HF1 | 197 | 61 |
| A3-HF3 | 201 | 63 |
| A3-05 | 203 | 64 |
| A3-HF4 | 204 | 65 |
| A3-HF6 | 206 | 66 |
| A3-HF8 | 208 | 67 |
| A3-HF9 | 210 | 69 |
| A3-HF10 | 213 | 71 |
| `91b43913`, `e89c25d7`, `eb1bf5e7` | **213** | **71** |

Between A3-05 and HF10 (about sixteen hours) H rose 203 → 213 and D 64 → 71. The H- namespace served both hardware checks (H-206, H-207, H-208, H-210, H-213) and queued defects (H-209, H-211, H-212).

### 18.4 The turn

The decision trail (2026-09-28) is thin in prose and clear in artefacts:

1. **HF8 (01:43).** Its "Next" line is an open menu: H-208, then H-185, "H-209, or A3-04H".
2. **HF9 (10:19).** Its "Next" line asks for "a hard Alpha 3 remaining-work reconciliation before any further defect work" — the first recorded turn toward a finite list.
3. **Between 10:19 and 12:15.** The reconciliation found D-70 (the Mix auto-recipe). The session itself — what was asked, by whom — is not in the repository; only its outputs are. The audit's HF10 block ends: "Then Alpha 3 RC preparation / tracker reconciliation — no further gameplay defect batch."
4. **`91b43913` (13:15).** A new `ALPHA3.md`, "ALPHA 3 PRE-RC — NOT RELEASED": a deferred set (§19.1) and a "Not reopened for Alpha 3 (decisions, recorded so they are not re-litigated)" list, ending "no additional gameplay parity audit is required before Alpha 3". Its gates were mechanical: four pending hardware checks; "a FAIL is a hotfix batch, not an RC"; size tripwires (image ≤ 1 MiB; an RC changes only `PROJECT_VER` and Git; ≥ 1 KiB of internal RAM needs an offset); A3-04H only on an unexplained trigger; a 19-step RC procedure (§20.2).
5. **`e89c25d7` (20:37).** The four checks pass in one 20-minute session on the HF10 image: "No pre-RC hardware gate is pending."
6. **`f85575b9` (20:52).** RC1: "0 open release blockers; every remaining item is deferred and non-blocking".
7. **`eb1bf5e7` (21:49).** The deferral list becomes "the final list"; "the release does not fix any of them". Nothing seen in the RC capture received an H- or D- identifier; the keyboard read errors were recovery evidence with "no blocker ID".

**What the committed record does and does not formalise.** A search of the tree at `alpha3-release`, and of the untracked and ignored non-build files in the working copy, found neither of two things one might expect:

- *A rule to stop following new identifiers.* The committed project record does not formalise one. It contains narrower things: from Batch 55 on, write-ups report "no new ID was allocated" as a per-batch fact; the reconciliation's "no further gameplay defect batch" and "no additional gameplay parity audit is required"; and identifiers that stop at H-213 and D-71 from the reconciliation through the release.
- *A five-way classification* (release blocker / functional defect / presentation difference / tooling issue / deferred parity residue). The committed record does not state one. The nearest are K1–K11 and Batch 52's nine categories, the ledger's *Kind* column ("native defect", "calibration + missing sound", "presentation (device)", "missing / extra presentation", "known missing original presentation"), the deferred lists, and the consistent label "tooling finding, not changed".

Whether either existed as a working convention in the sessions that produced the commits is outside what the repository can show; this history makes no claim either way.

## 19. What was consciously deferred from Alpha 3, and known technical residue

### 19.1 The release's deferred list

None of these locks the game, loses state or hides a control. They are catalogued in the ledger and `ALPHA3.md`; the release does not fix them, and this section does not reopen them.

| Item | What it broadly represents |
|---|---|
| **H-209 / D-67** | The shard ritual's seven bursts: 174 ms holds with noise in the original, a silent 60 ms flash on the device (§14.5). |
| **H-211 / D-68** | The Refuge's *stage content*: when the Avatar appears, and the final Avatar-only stage. HF9 fixed the cadence, not the staging. |
| **H-212 / D-69** | A shake over a static viewport reads as one 2 px drop, not eight pulses — the Board's frame model, every quake, predating Alpha 3. |
| **D-54** | The ending cinematic and paged presentation; the ending is transcript text over the Doom view, then the terminal state. |
| **D-56 (H-193)**, **D-57 (H-194)** | The ending's two box questions are answered for the player (real Y/N prompts in 1988); two literal `victory` lines follow its report. |
| **D-71** | Mix presentation residue (§14.7). |
| **A3-04H** | Removing redundant save-document imports at the pinned core layer and routing the document to PSRAM. Not started: the heap adjudication explained both triggers (§13.6). |
| **UI / frontend redesign** | HUD, Settings, the Ready picker (D-8), the `^` glyph (D-52), the `Direction?` overlay (H-15). No mockups exist. |
| **Moongate transit animation** | D-48, with D-58 (a party standing on a gate is not re-fired) and D-59 (the moon-phase latch is not refreshed on the device). |
| **Other classified items** | D-50 (H-22, wishing-well case — the binary folds case), D-51 (H-45, cancelled potion refunds), D-38 (scheduled NPCs' sleeping pose, needing an original witness), D-10/D-39 sweep residuals, D-60 (the fanfare does not hold the game, deliberately), open decisions D-1, D-2, D-4, D-5, D-7, D-9, D-12–D-16, hygiene D-17/D-18, and F-3 (a host-only resampling limit). |

### 19.2 Known technical residue outside the deferred list

Recorded in AUDIT or AUDIO as "not changed", but not on `ALPHA3.md`'s deferred list. None affects the firmware.

| Item | State at `alpha3-release` | Where recorded |
|---|---|---|
| **TALK overlay base** in `re/tools/callers_banda.py` | `'TALK.OVL': 0xa290`; TALK's kernel calls resolve through `0xBF80` (measured) | AUDIT HF5 row ("tooling finding, not changed"); `AUDIO §31` |
| **CAST2 overlay base** in `callers_banda.py` and `thunks.py --bases` | `'CAST2.OVL': 0xc29e`; the calls resolve through `0xE1E0`; a census through the tool "misses every CAST2 caller" (measured) | AUDIT HF6 row; `AUDIO §32.12` ("queued as RE-tooling follow-up work (not a game defect)") |
| **OUTSUBS / BLCKTHRN bases** | The tool lists `0x81d0` and `0xe63e`; the Batch 51 census text gives `0xA290` for both; not reconciled | Appendix B |
| **"Only caller" claims resting on the old MAINOUT base** | AUDIT says they "should be re-run"; no record says they were | AUDIT Batch 53B |
| **`re/disasm/` and `npm run re:disasm`** | Cited by upstream notes, one native test and `docs/methodology.md`; neither exists | §7.2 |
| **Batch 22's `U5OBJ` serial diagnostics** | "To be removed after the hardware run"; 17 lines still mention it in `alpha_runtime.cpp` (measured) | AUDIT Batch 22 |
| **97 TypeScript test failures** | Environment-bound, "recorded, not investigated", compared as a fixed set | §16.4 |

## 20. The Alpha 3 release

### 20.1 The record

| Field | Value |
|---|---|
| Final closeout commit | `eb1bf5e7` (2026-09-28 21:49:47 −0400) |
| Release tag | `alpha3-release` (annotated, 21:50:02) |
| RC tag | `alpha3-rc1` (annotated, 20:55:06) |
| RC source commit | `f85575b96f05bd7250ef41347f858bfc954e276b` (the tag's target) |
| RC evidence commit | `0f15e8736b3b37b2266297c044e197275b0373d9` |
| Artifact | `OpenU5-TDeck-Alpha3.0.0-alpha3-RC1-Debug-Launcher.bin` |
| SHA-256 | `4b5b9d1f5cb5c14fc2628da6c5ce2befb18f4c81daaa5bfd04821e1ee63353b1` |
| Embedded firmware / Git | `FW 3.0.0-alpha3-rc1-debug` / `f85575b96f05` |
| Size / free | 988,320 B (`0xf14a0`) / 60,256 B (5.7%) in the 1 MiB app partition |
| Launcher allocation | 1,048,576 B (the whole partition) |
| Host suite | 162/162, serial (139.25 s at RC1; 134.68 s on the release tree, no rebuild) |
| TypeScript | `tsc --noEmit` clean; 7,728 tests, the same 97-failure set as A3-HF10 |
| Hardware | Phase A3-RC1 PASS; pre-RC checks H-203, H-204, H-210, H-213 PASS on the A3-HF10 image |
| Heap gate | PASSED — A3-04H deferred |
| Promotion | RC1 promoted unchanged ("Option A, as Alpha 2's Batch 55") |

**Measured for this history.** The file at the path in `ALPHA3.md` (`native/targets/tdeck/build-a3-rc1-post/launcher/…`) is 988,320 B; its SHA-256 equals the value above and in the `alpha3-rc1` tag; it contains the strings `3.0.0-alpha3-rc1-debug` and `f85575b96f05`; and `cmp -l` against the A3-HF10 post-commit image counts 104 differing bytes, matching the tag ("104 bytes differ from the A3-HF10 image … as Alpha 2's RC1 differed by 98"). `git diff alpha3-rc1 alpha3-release` touches 20 files, all `*.md` or `*.log` (the RC evidence commit plus the closeout): the release is code-identical to RC1. From `alpha3-hf10-mix-parity` to `alpha3-rc1`, the only non-document changes are `CMakeLists.txt` (+7/−3, the `PROJECT_VER`) and a host-side comparison script, `native/core/tools/a3_rc1_ts_compare.py`.

### 20.2 The RC1 phase

The RC checklist is 19 steps, "a mechanical batch: no code, test or behaviour change. Stop at the first step that does not hold": a clean tree; the version bump as the only non-documentation change; a fresh host build (`build-a3-rc1`, 1 m 48 s, the one known w64devkit warning); serial CTest 162/162 with nothing else running; TypeScript compared *as a set*; a fresh ESP-IDF build with `--no-ccache`; a size diff against A3-HF10 with every section equal; the three image guards; one commit; a rebuild from that commit (the image embeds its commit hash at configure time); packaging with header, checksum and SHA-256 validation; the embedded version and Git checked against the commit; the SHA-256 recorded and checked with a second hash; an annotated tag with the identity and "Next: Phase A3-RC1"; no push and no flash unless asked; physical validation on that exact image; the heap capture in the same session (§13.5–§13.6).

### 20.3 Why promoting the exact tested artifact mattered

The Batch 23 self-misidentification, the Batch 50 stale image, the Batch 54 file-name rule (§8.3) and the Alpha 2 promotion's reasons (§10.5) point to one principle: hardware validation is a statement about a *file*. A rebuild changes the version string, the Git, the timestamps and the SHA, and adds nothing a tester can use. So the released Alpha 3 keeps `rc1` in its version string, "on purpose: the physical validation applies to this exact file"; `alpha3-rc1` stays on the source commit, and the release tag sits on the documents-and-logs closeout "the way `alpha2-batch55-release` sits on Batch 55's". A user who flashes the release sees `FW 3.0.0-alpha3-rc1-debug` and `Git f85575b96f05`, and every claim in the record about that image is checkable against those two strings.

## 20A. The Alpha 4 release

*(Added 2026-10-04; numbered 20A so the Alpha 3 chapters keep their numbers. The engineering record is `native/targets/tdeck/ALPHA4_UI.md` §18.)*

| Field | Value |
|---|---|
| Release tag | `alpha4-release` (annotated), on the final documents-only closeout commit after `e664c2d3` |
| Firmware provenance | `59c14b907399be8e8796dcd84946a60c942731c1` (`alpha4` RC5; the fix is `e7d4162f`) — **not** the tag's commit |
| Artifact | `OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin`, 1,047,408 B, SHA-256 `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210` |
| Embedded identity | `FW 4.0.0-alpha4-rc5-debug` / `Git 59c14b907399` |
| Free / allocation | 263,312 B in the 1.25 MiB app partition / Launcher 1,024 KiB |
| Host suite | 201 / 201, serial (148.2 s); parity / preservation / golden subset 39 / 39 |
| Hardware | RC5 retest PASS; RC3 music PASS; PARITY1 PASS; SAVE2 / UI3 / SAVE3 / Continue / smoke user-reported PASS; heap capture **waived** |
| Promotion | RC5 promoted unchanged, as Alpha 2's Batch 55 and Alpha 3 |

**How Alpha 4 got here.** Alpha 4 ran between Alpha 3's release (2026-09-28) and 2026-10-04: a UI track (chrome, title, saves, console, PC save bridge, the ending), a parity sweep, three enhancement batches (trackball, cheats, difficulty, keyboard light), then RC1 – RC5. The release candidates repeated the Alpha 3 lesson: hardware validation is a statement about a *file*. RC2's session exposed the dungeon-music defect (RC3); RC4's session, after PARITY2's eight fixes, exposed the healer's missing `R` key and an unreproduced In Mani Corp scroll report (RC5); the RC5 image passed its three-check retest and was promoted unchanged. The RC heap capture was never run; the user waived it, and the record states plainly that no Alpha 4 heap figures exist. The publication audit's one open question — whether two map fixtures should stay in the public tree — is recorded in `ALPHA4_UI.md` §18.6 and left to the owner.

## 21. Method and lessons

**The working method**, as the record documents it (each item points to where it is shown in action):

- **Evidence first**: read the bytes with the project's own tools, record offsets, confirm with a control the fix does not share (§7.3); the audit's [EXEC]/[REF]/[STATIC]/[UNPROVEN] tags and Batch 51's A–E timing classes make the habit explicit.
- **The fixture is the oracle**: when a parity fixture disagrees, move the native code, not the fixture (Batch 19 sequence 895; HF9 mismatch 5147).
- **RED first, GREEN, then mutation**, with survivors and invalid mutants disclosed (§16.3).
- **Measure first**: A3-04C refused a guess; A3-04F found the CRC before touching the renderer; A3-04G built the real storage code on the host first.
- **Physical hardware, a serial log, a named image** (§8.3, §8.5); **bind once and test the binding** (§4, §10.3).
- **Finite gates**: K1–K11, size tripwires, heap triggers, a mechanical RC (§18); **exact artifact identity**, never rebuilding what was tested (§20.3).
- **Receipts and ledgers**: annotated tags as batch receipts; a ledger so that a knowing divergence is a decision, not a rediscovery (the original's own bugs, which the port keeps, are in `docs/bugs-del-original.md`); corrections made in place with history kept.

**Lessons the record supports.**

- **Host parity is necessary, not sufficient.** Green parity coexisted with an unbound device layer (Batch 52), a prompt mirror nobody called (Batch 9D), a whitelist a JSON-only test could not see (Batch 26), and a compiler flag that turned a table read into a mutex round trip (A3-04A).
- **Embedded storage can dominate the experience, and it is a placement and bus problem before it is an I/O problem.** The Alpha 2.0 DMA failure, the SD log that stalled the screen for a second (A3-04D) and the 820 ms System Menu (A3-04G) are one story told three times.
- **Aggregate free heap hides placement.** `heap_int_min` is a sum of per-region minima; 150 KB of "missing" internal RAM was a live document a later load would replace (§13.6).
- **Presentation timing can be functional behaviour.** A pause holds every effect behind it, a getkey blocks the world, and `tone_sweep` blocks even with sound off; zero-time scenes were a bug.
- **Preserve exact release artifacts**, because a shared file name delivered a stale image and cancelled a batch (§8.3).
- **Device input buses need recovery paths**: a transient I2C error was permanent until staged recovery existed, then three errors in 10,248 reads were absorbed without loss.
- **The tools that read the evidence are evidence too.** The cited listings did not exist, the overlay-base table was wrong three times (two entries still are, §19.2), and a diagnostic sat invisible under a screen.
- **Release scope needs explicit boundaries.** Without a finite blocker test each fix found the next row; the last day of Alpha 3 shows a list, gates, and an identifier count that stopped.
- **Say when you don't know.** Batch 49's "unresolved" and A3-04C's "narrowed, not proven" are among the most useful sentences in the repository.

## Appendix A. Principal sources

| Kind | Source | Used for |
|---|---|---|
| History | `git log --reverse --date=iso --format=fuller` (152 commits through `alpha3-release`); `git for-each-ref refs/tags` (90 annotated tags); `git reflog` (the 2026-09-14 clone and branch creation) | ordering, dates, tag receipts (image, SHA-256, host totals, hardware status) |
| Release docs | `ALPHA2.md`, `ALPHA3.md`, `native/targets/tdeck/LAUNCHER.md`, `native/targets/tdeck/README.md` | release scope, files, validation, handoff |
| Early docs | `ALPHA1.md`, `ALPHA12.md`, `ALPHA13.md`, `ALPHA14.md`, `ALPHA14_ALPHA2.md`, `docs/history/tdeck/ALPHA20_*.md`, `MILESTONE5.md`, `DEBUG51.md`, `STACK51.md`, `DEVICE_INTEGRATION_AUDIT.md`, `native/core/VALIDATION.md` | milestone and Alpha 1.x era |
| Audit | `native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md` (1,221 lines at `cfd42139`; 8,489 at `alpha3-release`), `ALPHA20_BATCH4_ADJUDICATION.md` | audit method, batches 1–55, Alpha 3 hotfix blocks |
| Ledger and checklist | `ALPHA2_PRESERVATION_LEDGER.md` (D-, A-, E- rows), `ALPHA2_HARDWARE_CHECKLIST.md` (H- rows, phases, identity gate) | classification, hardware results |
| Audio | `native/targets/tdeck/ALPHA3_AUDIO.md` (5,335 lines) §1–§37 | A3-01…A3-05, HF2–HF10, storage and heap |
| RE | `re/notes/` (527 files at release: 516 imported, 6 from upstream syncs, 2 from web-side commits, 3 native-era), `re/tools/`, `native/core/batch*-original-*.log`, `docs/methodology.md`, `re/COVERAGE.md` | reverse-engineering |
| Evidence logs | `a3-04-postcommit-firmware-build.log`, `a3-04e1-hw-soak.log`, `a3-04g-hw-h201-h202.log`, `a3-04g-hw-summary.log`, `a3-rc1-hw-capture.log`, `a3-rc1-hw-summary.log`, `a3-rc1-heap-adjudication.log`, `native/core/a3-release-ctest.log`, `native/core/a3-04g-{red,green}.log` | hardware and heap evidence |
| Measured for this history | the Alpha 3 RC1 Launcher file (size, SHA-256, embedded strings, `cmp` against HF10); the Alpha 2 RC1 file (SHA-256, `cmp` against 53A); the A3-04 post-commit file (944,784 B); `git diff` of both promotions; `callers_banda.py` at `alpha3-release`; `add_test` counts at five commits; `re/notes` counts; trailer and committer counts; tag counts by prefix and date; the files touched by `0315510f` and `2485950e`; a text search of the tree and the working copy for a five-way classification or an ID-freeze rule | figures marked "measured" or "computed for this history" |

## Appendix B. Contradictions, ambiguities and open questions found while writing

Grouped by kind. None has been resolved by editing the source documents. Each entry says whether the value is recorded, measured, computed or unresolved; where this history has established a value, it says how.

**Dates and provenance**

1. *Resolved (measured):* native T-Deck work began with `11e32392` on 2026-09-14 23:12 −0400, not on 2026-09-12 as a previous summary said. `0315510f` (09-12) and `2485950e` (09-14 21:19) are web-side commits with no native file (§3.2). The only earlier trace of T-Deck intent is a local reflog entry creating `esp32-tdeck-port` at 21:22:18.
2. No commit exists between 2026-08-26 and 2026-09-12; the web-side commits were made in another working copy (`2485950e` three minutes before the local clone). Milestone 1 has no commit.
3. All `ALPHA1`…`ALPHA20_*` documents and the native UI/runtime layer were committed together in `c18f5b64` on 2026-09-18; their order is inferred. `ALPHA14.md` names `4d50203e9c68` (09-15) as its current build; `VALIDATION.md` in the same commit is dated 2026-09-16.
4. Local `main` is 69 commits ahead of `origin/main` (`c943eb06`, Batch 41) (measured). Nothing after Batch 41 has been pushed to `origin`.
5. Hardware results rarely carry a time (unrecorded: the Phase 8 smoke's time, H-207's hour, H-205's and H-206's images, the RC1 capture's wall-clock). Every dev tag says "Not flashed", meaning the agent did not flash; the user later flashed HF7, HF8 and HF10.

**Numbers**

7. *Resolved (measured):* A3-04's tag and AUDIO print "0xe6a90 = 945,808 B" (+25,680 B, 103,280 B free); the commit body repeats the +25,680 B. The recorded hex, the build logs and the file on disk give 944,784 B (+24,656 B, 103,792 B free); 945,808 is `0xe6e90`. The earlier draft called the 1,024 B "unexplained"; it is a hex-to-decimal slip, and A3-04A's documented "+9,040 B" inherits it (computed: +10,064 B) (§12.11).
8. *Recorded, partly unresolved:* several Alpha 2 tags print decimal sizes that disagree with their own hex (Batches 6, 7, 7B, 8B, 9). Batch 9's "(848,720)" for `0xcf150` is 848,208, corrected in Batch 9B's text. Batch 11's tag and AUDIT give 852,528 B beside `0xd0430`, which is 853,040 (AUDIT's `0x2fbd0` free agrees with the hex). The images were not re-measured for this history.
9. *Unresolved:* native test counts from different sources conflict at the edges. `VALIDATION.md` gives 4 → 14 at `8a603f8f`, 22 → 35 at `fd2a2036`, and 42, 44, 49 at `c18f5b64`; the Alpha 1.x/2.0 documents in the same `c18f5b64` give 27 (Alpha 1.2), 50 (1.3), 51 (1.4) and 53 (2.0); the audit gives 57; `add_test` lines at `c18f5b64` number 61 (measured). Alpha 1.2's 27 is below `fd2a2036`'s 35 though built later; the definitions are not stated. *Corrected:* the earlier draft called 6/6 the earliest native total; `VALIDATION.md` records 4/4 before it. The upstream 276 and 239 are not native counts.
10. HF9's runtime RED is "28/37" in its section and "61, 28 RED" in the commit body; one run that stopped early.
11. Batch 20's tag and commit say "eight" confirmed defects; AUDIT lists nine items (the ninth, H-122, flagged as an observation). Batch 20's group tally table does not match its cells; Batch 52 supersedes it.
12. The Codex XOR sites are `0x0dbd/0x0dd4/0x0deb` in the HF7 commit body and the ledger, `0x0dac/0x0dc3/0x0dda` in `AUDIO §33.2`; probably different instruction anchors.
13. HF5's Pause is 1,540 ms native against the reference's 1,538 ms (recorded as drift).
14. *Corrected (computed):* the earlier draft said "the last five hotfixes together cost +2,720 B". HF7–HF10 cost +2,720 B; HF6–HF10 cost +2,832 B.
15. *Corrected:* the earlier draft's "H-149 to H-152 froze the device; four different causes" disagreed with its own §15.1. The four rows have three explanations — one real capacity defect (H-151) and two authored behaviours (H-149; H-150/H-152) — and only H-151 was a freeze.

**Identifiers**

16. H-153 names the Deceit L1→L8 report in the checklist row (added in Batch 21A.1) and Batch 52's reconciliation table, and the Set Active Player fix in AUDIT's "Batch 21A.3 … (H-153)" heading and the `alpha2-batch21a3-combat-active-player` tag. *Corrected (measured):* the 21A.3 commit body does not use H-153, as the earlier draft said it did. More generally the H- namespace names checklist rows, hardware findings from H-146 up, and queued defects (H-209, H-211, H-212).
17. D-55 and D-56 are swapped in `re/notes/batch53a-endgame-terminal.md` §2 against AUDIT and the ledger. "GOG build" is used for the tree's `ENDGAME.OVL` without being defined.
18. Batch numbering: Batch 12 was used twice, the planned R-21 batch became 13, Batch 14 "was numbered Batch 13; renumbered". There is no Batch 50 commit, tag or document; Phase letter "6O" is skipped.

**Tooling**

19. `docs/methodology.md`'s `npm run re:disasm` does not exist, nor does the `re/disasm/*.asm` that upstream notes and one native test cite.
20. `callers_banda.py` at `alpha3-release` still lists `TALK.OVL: 0xa290` and `CAST2.OVL: 0xc29e`, both recorded as wrong (§19.2). It lists `BLCKTHRN.OVL` at `0xe63e` and `OUTSUBS.OVL` at `0x81d0`, while the Batch 51 census text gives `OUTSUBS/BLCKTHRN` at `0xA290`; that census used a "best-resolving base" heuristic, and the two are not reconciled.
21. Batch 22's temporary `U5OBJ` diagnostics, "to be removed after the hardware run", are still in `alpha_runtime.cpp` at release.
22. The Batch 53 tag message still describes the pre-53A "play continues" behaviour; Batch 52's description of the arena input path was corrected by Batch 53 without being edited.

**Judgement calls recorded in the documents**

23. H-202 passed with one unmet clause (largest internal block ≥ 32 KiB; measured 23,552 B), on the argument that the opens did not cause it.
24. The heap reading rule written at A3-04G closeout (`04d977c5`: "if internal + PSRAM falls by more than 4 KiB across the window … escalate at once") would have escalated the RC1 save window (−52,136 B). The RC1 capture, its adjudication and the rule's amendment ("a save window is judged by recovery") first appear together in `eb1bf5e7`; git cannot order them within that commit. The amendment is argued in full; the residual −4,664 B is called "live state" but not itemised.
25. The A3-04D music-on and music-0% averages (frame 101.3 versus 79.2 ms) were left unreconciled; the later A3-04E.1 soak went the other way, attributed to scenery.
26. The Batch 47 → 48 → 49 reversal on NPC pose; "Outcome D" in Batch 49's title is never defined in its text.
27. Forty commits carry no co-author trailer, so their authoring model, if any, is not recorded (the list is in "How to read").
28. The committed record does not formalise a five-way classification or a rule to stop following new identifiers (§18.4). That is a statement about the repository, not about the sessions that produced it.

**What was not verified.** This document was assembled from the repository's own text plus the measurements listed in Appendix A. No build or test was run, and no firmware was flashed.

## Addendum (2026-10-05): public version normalised to v0.4.0

The development milestone Alpha 4 was publicly normalised to **v0.4.0**. `alpha4-release` remains as a historical tag; `v0.4.0` is the public release tag and the GitHub Release is titled "Ultima V Native v0.4.0". The firmware bytes were not changed: the public file `UltimaV-Native-v0.4.0-TDeck.bin` is the RC5 image (SHA-256 `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210`), and its RC5 internal identity is retained as provenance. Earlier entries are not rewritten.

## Addendum: OpenU5 and Ultima V Native are separate implementations

OpenU5 and Ultima V Native share repository history and reverse-engineering/reference material, but they are separate implementations. From the Ultima V Native public rebrand onward, browser-specific features and roadmap items are tracked separately (see [`OPENU5_BROWSER_ROADMAP.md`](../browser/OPENU5_BROWSER_ROADMAP.md) and [`ROADMAP.md`](../../ROADMAP.md)). Earlier entries are not rewritten.
