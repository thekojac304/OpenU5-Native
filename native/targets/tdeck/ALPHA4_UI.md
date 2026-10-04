# Alpha 4 — UI track

Alpha 4 is a UI/presentation track on top of the released Alpha 3 (`alpha3-release`, `eb1bf5e7`). Each batch keeps its own section; a finding is recorded where it was made and not merged into an earlier batch's conclusion.

> **ALPHA 4 RELEASED (2026-10-04) — read §18 first; it supersedes every "owed", "pending" and "candidate" below.** RC5 (`FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399`, SHA-256 `363a8fda…7210`) is the hardware-validated firmware image: the RC5 retest (identity, the In Mani Corp scroll on a living target, the healer's `R`) **PASSED**. RC3's music retest PASSED; the RC heap capture was waived by the user; PARITY1's checks PASSED; the RC4 failures (R4-4, R4-5) are fixed and retested. The paragraphs below are kept as written, as the history of how the release was reached.
>
> **RC5 hotfix (2026-10-04) — read §17 first.** The user's RC4 hardware pass (R4-1/2/3/6 PASS, R4-7 not tested) failed two items: the healer's `R` did nothing (fixed: the shop key map never produced `Resurrect`; §17.2) and a living target of the In Mani Corp scroll showed only `Failed!` (investigated, **not reproduced**, route now pinned by real-runtime tests; §17.3). The candidate image is now **RC5** (`4.0.0-alpha4-rc5-debug`, §17.6; `Git 59c14b907399`, SHA-256 `363a8fda…7210`): host suite 201 / 201, guards GREEN, **three-check retest owed** (§17.7). RC4's record below is unchanged.
>
> **A4-PARITY2 (2026-10-04) — read §16 first.** The post-Alpha-4 parity cleanup fixed D-82, D-83, D-84, D-85, D-86, D-87, D-88 and D-89 (reference first, native second, RED-first, 110 / 110 mutants killed), the In Mani Corp living-target text, and corrected the jail / inn note; fourteen newly found divergences (D-90 … D-103) are recorded, not fixed (§16.13). The candidate image is now **RC4** (`4.0.0-alpha4-rc4-debug`, §16.15; `Git 859f1605b974`): host suite 200 / 200, guards GREEN, **not yet hardware-tested** (minimal checklist §16.16). RC3's record (§15) is unchanged.
>
> **RC3 hotfix (2026-10-03) — read §15 first.** The RC2 hardware session found that a dungeon played the surface's music (every entry, not only Developer teleport): fixed in one block of `sync_music()`, host-verified (192 / 192), **not yet hardware-tested**. RC2 (§14.9) is superseded by RC3 (`4.0.0-alpha4-rc3-debug`); §14's records are kept as written.
>
> **Current state (A4-CLOSE1, 2026-10-02) — read §14 first.** Alpha 4's implementation is complete and host-verified (every track, §14.4). The final candidate image is **RC2** (`4.0.0-alpha4-rc2-debug`, A4-POLISH3's source with a new version line; image in §14.9). Known parity divergences (D-82 – D-89 and the older open rows) are tracked separately (§14.5) and are not unfinished Alpha 4 work. What remains before Alpha 4 can be called RC-ready is **one hardware session on the RC2 image** (§14.7): the PARITY1 fixes, the SAVE2 / UI3 slot pages, SAVE3 on the device and the RC heap capture have never been run on a T-Deck. Every "pending" in §2 – §13 is the status as that batch wrote it; §14.3 classifies each one.

## 1. UI Batch 1 (A4-UI1) — the "More Ultima V" chrome

Three commits: `4c8f8a17` (A, IBM.CH in the resource pack), `aebd9af0` (B, the chrome), `9ab2564e` (C, goldens). The code comments of UI1 already point at this file, but UI1 did not write its section. Its record is those three commit messages and the `native/core/a4-ui1-*.log` evidence logs.

In short:
- IBM.CH travels in the pack as `ibm.ch`. The pack identity lock is 2,042,554 B, CRC `0x9c10874f`, 43 entries.
- The frame uses the original's EGA palette.
- The roster and status box follow `roster.ts`.
- The band captions are IBM.CH.
- Menus select in reverse video.
- The screen goldens are in `a4_ui1_goldens.h`.

UI2 keeps all of this. The one UI1 golden it changes is the System Menu screen (§2.6).

## 2. UI Batch 2 (A4-UI2) — title, death music, save/load

A single presentation/flow batch in four commits:

| Commit | Subtrack |
|---|---|
| `8aa930e3` | A. Title-screen polish |
| `d81733f4` | B. Death/resurrection music lifecycle |
| `5749694c` | C. Save/load presentation |
| E (this document) | Goldens, docs, evidence, version `4.0.0-alpha4-ui2-debug` |

D, the multi-slot implementation, was **not** done; §2.4 explains why and gives the design. No gameplay rule, save format, resource pack or audio pack changed.

### 2.1 A — Title screen

**Findings (before editing).**
- **One draw path.** `FrontendSession::view()` (`core/src/frontend.cpp`) gives Title and the startup intro (page 0) the same Generic view:
  - two lines, "Lord British presents" and "Copyright 1988 Lord British";
  - footer "Press a key".

  `AlphaRuntime::render()` passes the art (`intro_title`: four 320×110 frames; fire frame `(tick/5)&3`) to `Board::show_frontend()`. That one call draws everything:
  - the art at (0,4) 320×110;
  - the lines through `draw_generic_body()`;
  - the footer in the common footer row.
- **Separately drawn?** Yes and no. The credits and the prompt are separate `draw_text_box` calls, but in the same call and the same frame, with no timing of their own.
- **Font and scale.** The Board's built-in compact 5×7 glyphs, in 6-px cells, scale 1. `ui_size` does not apply, because the view is not a sized menu.
- **Coordinates.**
  - The lines are **left-aligned** at x=20: boxes (20,116,280,9) and (20,127,280,9), white. The ink spans x 20..181. Nothing is centred.
  - "Press a key" is the grey (`#AAAAAA`) footer at (8,228), bottom-left. It sits 94 px below the credits and apart from them.
- **Animation.** A fire frame resends only the 288×49 strip at (16,65). Neither the body nor the footer is resent.
- **Timings** (`frontend.cpp`): Title for 1,400 ms, then the startup intro until 5,600 ms, then the attract demo. Any key on the Title opens the main menu.
- **Coverage.** No host test captured the Title's pixels. The UI1 `main-menu` golden goes straight to the menu.

**Change.**
- The Title (and the startup intro) is `FrontendViewKind::TitleCredits`. The value is appended to the enum; strings, states and timings are unchanged.
- The Board draws that kind as a centred block in the game's own **IBM.CH 8×8**, the UI1 chrome font. That is 8-px cells and 8 rows, against 6-px cells and 7 rows before: a modest size increase.

| | Before | After |
|---|---|---|
| "Lord British presents" | 5×7, x=20, y=116, white | IBM.CH, centred x=76, y=130, white |
| "Copyright 1988 Lord British" | 5×7, x=20, y=127, white | IBM.CH, centred x=52, y=144, white |
| "Press a key" | 5×7 footer, x=8, y=228, grey | IBM.CH, centred x=116, y=184, grey; the footer row stays blank |
| Gap under the art (ends y=113) | 2 px | 16 px |
| Line pitch / credits → prompt / clear to bottom | 11 / 94 / 5 px | 14 / 32 / 48 px |

- The block is redrawn only when the screen or its text changes, so an animation frame still sends the strip alone.
- Constants: `kTitleCreditsY=130`, `kTitleCreditsStep=14`, `kTitlePromptY=184` (`device_ui_views.h`).
- The art is untouched.

### 2.2 B — Death / resurrection music

**Hardware defect.** When the party dies in battle while music plays, the battle music plays on through the whole death and resurrection.

**Reference.**
- **The patch author** (`original/u5/ultima5/History.txt`) made sure that during Death and the Blackthorn capture "no music should be playing".
- **The driver** (`re/notes/music-location-mapping.md` §1.4):
  - TOWN `0x1862`, MAINOUT `0x0af0` and DUNGEON `0x1014` issue selector `0x03` (stop, and freeze the location rule) before `party_refuge` (BLCKTHRN `0x0910`).
  - They re-enable the location rule after it.
  - The next key poll, at the castle prompt (`0x1b5b` → `0x0e0e`, selector 0 before `kernel_getkey`), derives Lord British's castle (0x11) → **The Missing Monarch** (song 0x07).
- **The web reference** (`game/src/main.ts` `runRefugeScene`): `music.play("silence")` first; `resumeMusic()` right after `resolveRefuge()`.
- **Death has no music of its own.** It is silence from the Refuge's first beat to the resurrection, then the castle's song at once.

**The native path, traced.**

| Step | Where it runs | Music re-derived before A4-UI2? |
|---|---|---|
| The troll's blow kills the last standing member | `render()` → `service_combat()` → `CombatEnemyStep` | no |
| The hit cue holds, then the teardown | `render()` → `finish_combat_if_needed()` → `finish_encounter_combat()` → "BATTLE IS LOST!" → `check_refuge()` → `GameEventKind::Refuge` | no |
| The Refuge beats; the karma getkey; its key | `render()` → `service_narrative_scene()`; the scene's input rule returns early | no |
| `resolve_refuge()`: revived, at the castle | `render()` → `service_narrative_scene()` | no |
| The first ordinary key after waking | `handle()`'s last line | yes — Castle → Monarch |

**Root cause: a state transition that nothing re-derives, not a missing cue.**
- `AlphaRuntime::sync_music()` already maps the Refuge to Silence (ALPHA3_AUDIO.md §17.6). But it runs only on key polls (§17.7).
- A battle lost to an enemy's blow reaches the Refuge and leaves it inside `render()`. So Engagement and Melee played until the first key after the resurrection.
- The out-of-combat Refuge (a Space, A3-HF9's path) got the stop from that key. It then stayed silent at the castle until the next key.
- Escapes and the victory exit are torn down in the input path, so they already re-derive the music.

**Fix** (`alpha_runtime.cpp`, two calls): the Refuge event's intake calls `sync_music()` (Silence, whatever raised it), and so does `resolve_refuge()`'s completion (Monarch at the castle, no key needed). Mutes, volumes, SFX and pacing are not touched.

| | Before | After |
|---|---|---|
| Blow → first Refuge beat | Engagement plays on | stop (Silence) at the beat's frame, before "An unending darkness engulfs thee..." |
| Every beat, the getkey and its key | Engagement | silence |
| Resurrection resolves (castle) | Engagement until the next ordinary key | The Missing Monarch at that frame, started once |
| Music muted (Alt+Shift+M) | context stays Combat; unmuting at the castle plays Engagement | context Silence → Castle; unmuting plays Monarch |
| Music Volume 0 %, raised after | Monarch (Settings keys re-derive) but context Combat until then | context Castle; raising plays Monarch |
| World-path Refuge | stop on the Space; silent at the castle until a key | stop on the Space; Monarch at the resolve |
| SFX (slumber, thunder ×2, revival), scene timing | — | unchanged (getkey 15,985 ms after the blow; resolve 3,990 ms after the key, with music, muted, and at 0 %) |

- The doc claim in ALPHA3_AUDIO.md §17.7 ("called from exactly three places") and §35.4 ("the Refuge's silence starts with the scene's first beat") assumed an input drove the change. A correction note there points here.
- **Not examined here:** the end of the Blackthorn capture scene, which also ends in `render()`. Its silence is the pacer's own rule (§17.6 case 1). Nothing was reported, and nothing was changed.

### 2.3 C — Save / load presentation

**The architecture, inspected.**
- **Two physical generations of ONE journey:**
  - `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`;
  - the commit is 32 B: magic `0x31533555`, version 1, sequence, three CRC-32s.
- **Every save writes the slot `(newest+1)&1`**, which is always the older generation. The order is temps, CRC readback, renames, the commit last, then a post-write semantic check.
- **Continue / Alt+L** choose the newest complete generation by sequence (`select_generation`). They fall back to the other if the newest is refused by the CRC gate or the semantic gate (`stage_generation`).
- **"Load slot"** restores one generation and has no fallback.
- **The redundancy is recovery for the same journey**: the backup is always the previous save. There are no player-facing slots.
- **Save identity.** None beyond magic and version:
  - `identity_matches` is hard-wired true;
  - nothing ties a save to the resource pack, a character or a journey.
- **Cost per generation:** GAM 4,192 + OOL 512 + commit 32 + sidecar (369 B in the host fixture, about 2.5 KB on a played card), about 12 sectors. A save needs more than 32 KiB free.
- **Timings (H-202):** save about 2.1 s; load 0.8–1.05 s; cold list 785 ms; cached list about 83 ms.
- **The menus exposed the physical layer.**
  - Rows were "Generation 1: Avery" and "Generation 2: corrupt", by slot index. Which row was newer flipped with every save.
  - The only metadata was the leader's name. Saving said nothing about what it overwrites.
  - A refused Generation row in the System Menu did nothing, silently.
  - "Create New Character" silently makes the current journey the backup.
- **Latent (not changed, queued as a separate task).** The save target is chosen by sequence regardless of validity. If the newest generation is refused (torn files under an intact commit, or a failed post-write check after the commit rename), the next save overwrites the older, valid one. A failure of that save leaves no restorable generation. Test L8 shows the first half. The fix belongs to the storage batch that §2.4 describes. *(Fixed in A4-SAVE1, §3.)*

**The new presentation.** There is no storage change: file names, the commit record, generation choice, fallback and the gate are untouched.

| Screen | Before | After |
|---|---|---|
| System Menu rows | Resume · Save · Load / Save Management · Settings · (Developer / Debug) · Return to Title | Resume · **Save Game** · **Load Game** · Settings · (Developer / Debug) · Return to Title |
| System Menu footer | "Alt+M or Mic: resume" on every row | per row: Save Game "Saves now; the previous save becomes the backup"; Load Game "The latest save and its backup"; Return to Title "Unsaved progress will be lost"; the others as before |
| After Save Game | nothing in the menu (the transcript line is hidden behind it) | footer "Saved." / "Saved. The previous save is now the backup." / "Save failed. Your previous save is kept." until the next key (the transcript lines are unchanged) |
| Load page | ">Load / Save Management<": Continue Latest · Generation 1: *name* · Generation 2: *name*/corrupt/empty | ">Load Game<", subtitle "Each save keeps the one before as a backup": **Latest: *leader*, *place*** · **Backup: *leader*, *place*** (or "damaged", "no save yet", "none yet") |
| Load page footer | "Enter loads; Mic returns" | the selected row: "4-5-139 13:05, party of 2. Enter loads" · "Damaged: Enter loads the backup instead" · "Damaged: this backup cannot be loaded" · "Each save keeps the one before as backup" · "No save on this card yet" |
| Picking a damaged or missing backup | nothing (System Menu); "No valid save in this slot" (title) | "That backup is damaged and cannot load" / "No backup save yet" |
| Title main menu, on Create New Character | the hotkey help | "New game: your current save becomes the backup" (only when a valid save exists) |
| Journey Onward | Continue Latest · Recovery / Load Previous | subtitle "Latest save: *leader*, *place*" (or "Latest save damaged; Continue uses the backup" / "No saved journey on this card") · **Continue** · **Load Game** |
| Title Load page | ">Recovery / Load Previous<", Generation rows; Back → main menu | the same Load Game list as the System Menu; Back → Journey Onward |

- **Latest is Continue.** It carries Continue Latest's semantics and fallback, so every existing "Load page, Enter" route loads exactly what it loaded.
- **Backup is `load_slot`** of the older generation.
- **Row order is by commit sequence**, whichever physical slot holds each generation. A refused generation keeps its sequence in the list (`alpha_save.cpp` inspect, the memory stub), so it can still be ordered.
- **Where the metadata comes from.** `summarize_candidate()` fills the place, the game date/time and the party size from the generation the gate staged:
  - the place is the HUD's caption; a dungeon session names the dungeon;
  - nothing is read from the live game.
- **Width limits.** Rows are cut to 36 cells (Large text, `kSaveRowChars`). Footers are at most 50.
- **Shell fix.** The shell's body clear started at y=20 and cut the last glyph rows of a subtitle (y=15..22). No shell page had a subtitle before. It now starts at y=23.

### 2.4 D — Multiple manual save slots: deferred

*(Implemented by A4-SAVE2, §4, to this design: slot 1 is the existing pair, one sequence across the card. Hardware validation pending.)*

**Judgement: materially invasive for a UI batch.** The two-generation pair is the recovery mechanism of one journey, and repurposing it would remove recovery. Real slots need a pair each, which touches:
- **The storage service:** path naming, the target rule (including the latent defect in §2.3), the InspectCache (sized `[2]`), `load_slot`, and `restore_newest` over N pairs.
- **The `FrontendSaveSlot[2]` shape**, threaded through 12 call sites: the frontend, the System Menu, the runtime, both host stubs, and five tests.
- **The semantics of Alt+S, New Journey and Continue**, which need a "current slot" and a global order.
- **The cold list cost.** It triples, from 785 ms toward about 2.4 s on the device, unless the list stops staging whole generations.

The presentation cleanup above stands on its own. Its Latest/Backup vocabulary is the one a slotted catalogue would show per slot.

**Follow-up design (its own storage batch).**
1. **Prerequisite:** the target-rule fix. Write over the generation that is not the newest *valid* one, and keep the sequence monotonic.
2. **Layout.** Logical slots 1–3, each an A/B pair:
   - `alpha1-s{2,3}-g{0,1}.*`;
   - **slot 1 is the existing `alpha1-g{0,1}.*`** (no migration: an Alpha 3 or UI1/UI2 card is slot 1, byte for byte);
   - the commit record v1 is unchanged, and the slot is named by the file name.
3. **Order across slots.** One global sequence: next = max over every commit + 1. "Continue" is the highest sequence among valid generations on the card. Each slot's own Latest/Backup is as in §2.3.
4. **List cost.** Per slot, stage only the newest generation, and the backup only if that is refused: about 3 × 390 ms cold, cached as now by commit fields. Or add a summary record to a v2 commit so the list never stages a document.
5. **UI.**
   - Load Game lists Slot 1–3, each with its leader, place and date. Recovery (the backup) shows only when a slot's latest is damaged.
   - Save Game asks which slot, and confirms an overwrite of a different journey.
   - Alt+S saves to the current journey's slot.
   - New Journey takes an empty slot or asks which to overwrite.
6. **API.** A `SaveCatalog` (N × 2 summaries) replaces `FrontendSaveSlot[2]`; `load_slot(slot, generation)`.
7. **Storage cost.** 6 generations × about 5–7 KB plus temporaries is at most about 45 KB; the 32 KiB free-space rule stands.
8. **Downgrade.** An older firmware reads only `alpha1-g{0,1}` (slot 1) and ignores the rest.
9. **Tests.**
   - Over the real `alpha_save.cpp` (sd_shims, fault injection): an Alpha 3 card lists as slot 1 and loads identically; writing slot 2 never touches slot 1's files; a power cut (fault) in slot 2 leaves slots 1 and 3 intact; Continue picks the global newest.
   - The memory stub grows to N pairs.

### 2.5 Migration and compatibility

- **No on-card format changed.** Saves written by Alpha 3, UI1 or UI2 are the same files and the same commit record. Each firmware reads the others'.
- **`FrontendSaveSlot` lives only in RAM.** It grew from about 32 to about 64 B, in the InspectCache (PSRAM workspace) and the menus.
- **Unchanged:** `settings.json`, the resource pack (still the UI1 pack, 2,042,554 B, CRC `0x9c10874f`) and the audio pack. No SD card change is needed from UI1.
- **Test identity checks.** Tests that picked a generation by slot row now pick it by age (§2.6). The generation chosen by every Continue / Alt+L route is unchanged.

### 2.6 Tests, RED-first, mutations, suite, firmware

New ctests share `host_tests/a4_ui2_harness.h`: the real `AlphaRuntime`, the real `tdeck_board.cpp` over the fake ST7789 (untimed), the virtual clock, a recording audio backend and the two-slot memory card. The build defines them through `openu5_a4_ui2_runtime()` in `native/core/CMakeLists.txt`.

| Test | Checks | RED-first (unmodified production) | After |
|---|---|---|---|
| `a4_ui2_title_runtime` | T1–T10: the art unchanged, IBM.CH lines centred, prompt placed, nothing stray, spacing, strip-only animation, 1,400 / 5,600 ms timings, intro identical, key → menu | 5/10 (T2–T6 RED) | 10/10 |
| `a4_ui2_death_music_runtime` | D0–D10, W1–W2: the troll arena, the blow inside `render()`, paced scenes, the real audio pack; mute, 0 %, timing, the world path | 6/13 (D2, D3, D4, D6, D7, D9, W2 RED) | 13/13 |
| `a4_ui2_save_menu_runtime` | M1–M5, L1–L10, F1–F5 (+F2b), Z1 | 2/22 (only L4 / L7, Continue unchanged, GREEN) | 22/22 |

- **RED-first driver:** `native/core/tools/a4_ui2_red_first.py <build> A|B|C [--dump dir]`. It swaps in the production files of the commit before each subtrack (converted to the checkout's line endings), builds, runs the test and restores. Logs: `a4-ui2-{A,B,C}-red-first.log`, `a4-ui2-red-first-rerun.log`.
- **Mutations:** `native/core/tools/a4_ui2_mutation_check.py` runs 22 mutants (A1–A6, B1–B3, C1–C13; C7 against the real `alpha_save.cpp` through `a3_04g_storage_runtime`). Result: **22 / 22 killed, 0 survived, 0 invalid; restored build GREEN** (`a4-ui2-mutation.log`). C1 (Latest by physical slot) survives L1, because the newer save happens to be in slot 0 there; L5, after a third save, kills it.
- **Intentional expectation updates:**
  - `frontend_test` (the Load flow's rows by age, plus a damaged-backup case);
  - `batch28_save_validation` (`menu_load_row` = Latest/Backup; F3: a refused latest now falls back, whole; F3b: a refused backup does nothing);
  - `a3_04g_storage_runtime` (rows compared through `format_save_row`; K5/K6 by age; V3 corrupts the backup);
  - `a4_ui1_chrome_runtime` G1: the `system-menu` screen golden re-recorded (new labels), the other seven states unchanged.
- **Host suite:** baseline 163/163 (141.6 s). After A 164/164 (131.9 s); B 165/165 (125.0 s); C 165/166 (only the golden above); E **166/166** (131.3 s, `a4-ui2-E-ctest.log`). There are three new targets.
- **Firmware** (`idf.py -B build-a4-ui2 reconfigure`, `ninja -j 4`): built first time, zero project warnings. The binary is `0xf27d0` (993,232 B), +2,720 B against UI1 (`0xf1d30`): `.text` +1,840, `.rodata` +880, internal `.bss` +256 (the larger save summaries in the runtime's menus and the menu notice). 55,344 B (5 %) of the 1 MiB slot is free. IRAM and `.data` are unchanged (`a4-ui2-fw-size-diff.log`, against `build-a4-ui1`).

### 2.7 Hardware validation (A4-UI2 image; SD packs unchanged from UI1)

The boot screen must show `FW 4.0.0-alpha4-ui2-debug` and the Git of the image being tested. If it does not, stop: every Launcher image shares one file name, so check the `FW` line before judging anything.

1. **Title (2 min).**
   - Return to Title (Alt+M → Return to Title).
   - Check that the credits sit centred under the art in the game font, and that "Press a key" is centred below them with nothing in the bottom-left corner.
   - The fire animates, and the text does not flicker.
   - After about 1.4 s the same screen stays, then after about 5.6 s the attract demo starts. A key on the Title opens the menu.
2. **Death music (5 min).**
   - Get into a fight with music on (Music Volume above 0, not muted), then let the party die.
   - The battle music stops as the screen reads "An unending darkness engulfs thee...". No music plays through the whole scene, including at Lord British's speech and its key.
   - As the party wakes in the castle, The Missing Monarch starts **without a key**.
   - The slumber melody, thunder and revival sounds play as before.
3. **Death music, muted (3 min).**
   - Alt+Shift+M before a fight, then die.
   - There is no music throughout.
   - At the castle, Alt+Shift+M plays The Missing Monarch, not the battle music.
4. **Save/Load (5 min).**
   - Alt+M: the rows read Save Game and Load Game. Moving the cursor changes the footer: Save Game tells you the previous save becomes the backup; Return to Title warns that unsaved progress is lost.
   - Save Game: the footer reads "Saved. The previous save is now the backup."
   - Walk elsewhere, save again, then open Load Game. It shows **Latest** (here) and **Backup** (the previous place). The footer gives the date, time and party.
   - Load the Backup, then use Load Game → Latest.
   - Return to Title, then Journey Onward: the subtitle names the latest save, and Load Game lists the same two rows. Mic from Load Game returns to Journey Onward.
   - On the title menu, highlighting Create New Character shows the backup warning.
5. **Regression (3 min).**
   - Alt+S and Alt+L still work.
   - Power-cycle, then Continue: the latest save loads.
   - The Settings page is unchanged.
   - The System Menu's Settings and Developer rows are unchanged.

**PASS** requires steps 1–5, with no crash, lock or watchdog. **Report back:** the `FW`/`Git` lines and PASS/FAIL per step.

### 2.8 Hardware result and closeout (A4-UI2)

**PASS on the T-Deck.** Tested image: `FW 4.0.0-alpha4-ui2-debug`, Git `9105950ef13d`, 993,232 B (`0xf27d0`), Launcher SHA-256 `1c5787610282a553f03b0af5857630d984dddaf203d6a05f83639b3136c69896`. SD packs were the UI1 packs, unchanged.

| Step | Result |
|---|---|
| 1. Title: credits/prompt presentation | PASS |
| 1. Main-menu reverse-video selection | PASS |
| 2. Death/resurrection music lifecycle | PASS |
| 2. SFX during resurrection | PASS |
| 3. Muted / 0 % music behaviour | PASS |
| 4. Latest / Backup save-load presentation | PASS |
| 4. Loading the Backup; save rotation | PASS |
| 4. Create New Character warning | PASS |
| 4. System Menu / Load Management presentation | PASS |
| 5. Power-cycle Continue | PASS |
| 5. Settings persistence and text sizes | PASS |
| 5. General gameplay / UI regression | PASS |

No new hardware issue was observed. No crash, lock or watchdog.

Closeout checks on the tagged tree: the serial host suite is **166/166** (144.6 s, `a4-ui2-closeout-ctest.log`); the tree had no source changes since `9105950e`; the resource-pack identity lock is unchanged since UI1 (`kExpectedAlphaResourceSize` 2,042,554 B, CRC `0x9c10874f`; no diff to `alpha_resources.*` since `9ab2564e`); the image embeds `4.0.0-alpha4-ui2-debug` and Git `9105950ef13d`. Post-commit build/package evidence: `a4-ui2-postcommit-{configure,build,package}.log`. The earlier UI1 batch C logs (`a4-ui1-C-*.log`) were committed separately as historical evidence.

Next known engineering item: a latent save-target weakness. A save made after the newest generation is corrupted can overwrite the only good generation.
*(A4-SAVE1, §3, fixed it.)*

## 3. A4-SAVE1 — recovery hardening

A storage fix, not a UI batch. It is scoped to one defect: the one §2.3 recorded as latent and §2.4 made step 1 of any slot work. Out of scope and unchanged: the two-generation design, manual save slots (§2.4 stays deferred), the file names, the commit record, the transaction order, the load gate, generation choice on load, and every menu.

**Baseline.** HEAD `c0ece5a4` on `main`, clean tree. Serial host suite 166/166 (182.5 s, `native/core/a4-save1-baseline-ctest.log`).

### 3.1 The defect

After Continue fell back from a refused newest generation to the older one, the next save wrote over that older generation, the only valid one.

The exact sequence (test group C, over the real `alpha_save.cpp`):
1. Slot 1 holds sequence 1 (valid). Slot 0 holds sequence 2.
2. Slot 0's sidecar is torn under its intact commit (or re-sealed with content the semantic gate refuses).
3. Continue refuses slot 0 and restores slot 1, which is correct.
4. Save. On HEAD the target was `(2 + 1) & 1` = **slot 1**:
   - temps, CRC readback;
   - the gam/ool/json renames replace slot 1's files;
   - the commit is written last.

Recorded pre-fix results (`a4-save1-red-first.log`):
- **When every stage succeeded:** the card ended as slot 0 = refused sequence 2, slot 1 = the new sequence 3. There was one valid generation and no backup, and the refused one was never replaced.
- **When any stage from the first rename to the commit rename failed (D3–D5):** slot 1 held new files under its old commit, and slot 0 was still torn. **Neither generation loaded.** A reboot's Continue found nothing.
- **When the temp write or readback failed (D1, D2):** the save failed harmlessly, but the retry in the same session overwrote slot 1 (D1r, D2r).

The same rule also broke in two more cases:
- **A post-write self-check that fails after the commit.** It leaves a committed-but-refused newest generation, which is the same state (D6).
- **Sequences that no longer follow the slots.** `(newest + 1) & 1` then writes over the NEWEST generation (G1, G2, G3).

**Root cause.** In `AlphaSaveService::save()` the target came from the commit records alone: `newest = max(sequence of every readable commit)`, then `slot = (newest + 1) & 1`. A commit record says nothing about whether its files are intact or its content passes the gate.

The error came from two assumptions:
- the highest sequence is the generation to keep;
- sequence parity names its slot.

Both hold only while every generation is valid and every save succeeds. The transaction itself is sound: it protects the other slot perfectly. The target selection pointed that protection at the wrong slot.

This is not a validation or commit defect. The gate refused the right generation, and load chose the right fallback.

### 3.2 The invariant

> From the first rename of a save until its generation is written, read back, committed and accepted by the post-write check, the generation that Continue would restore is on the card byte for byte and loadable.

A save's target holds no loadable generation between its first rename and its commit, so the invariant fixes the target:
- **The generation Continue restores is never the target.** That is the newest generation by sequence that the gate accepts.
- **The newest is accepted:** keep it and write the other slot. This is the normal rotation.
- **The newest is refused:** it is expendable. Write over it and keep the older generation (Continue's fallback).
- **Sequence:** the new sequence is the newest sequence + 1, so the new generation is always the newest. Sequences are `uint64_t` and compared with `>`. On a tie, the lower slot is the newest, exactly as `openu5::save::select_generation` picks.
- **Slot identity and parity are never assumed.** A physical slot says nothing about age or validity.

### 3.3 The fix

The files changed:
- **`main/alpha_save_generation.{h,cpp}`** (the storage-independent half) adds three functions:
  - `newest_committed_slot()`;
  - `choose_save_target(present, commits, newest_accepted)`, which returns `{slot, sequence, keep}`;
  - `files_match_commit()`.
- **`main/alpha_save.cpp`** `save()`. Before capturing or encoding anything, it:
  - reads both commit records;
  - asks `generation_accepted()` about the newest;
  - takes the target from `choose_save_target()`;
  - logs `SAVE_TARGET newest= accepted= slot= keep= sequence=`.

  The rest of the transaction is untouched and in the same order: temps with fflush+fsync, CRC readback, the three renames, the commit temp and rename last, the post-write semantic check, and the save-list update.
- **`generation_accepted()`:**
  - It reads the newest slot's three files and checks their CRC-32s against its commit on every save.
  - It then runs the full gate (`verify_candidate`) unless the save list (InspectCache) holds an accepted summary for this exact commit. Same sequence and CRCs, plus matching file CRCs, means the same bytes the gate accepted, so re-staging would repeat a known answer.
  - A refused slot is dropped from the list.
  - Everything it stages is released before the save builds its own document.

  A stale list entry cannot protect a damaged generation, because the CRC check runs every time. Test R2 has the list naming the damaged generation as valid.
- **`host_tests/host_stubs/alpha_save_memory_host_stub.cpp`** used its own copy of the old rule. It now calls the same `choose_save_target()`, with the gate as `newest_accepted`. It also gains the seam `host_memory_save_damage_older_for_test()`.

The state is derived from the card each time: no flag, no remembered "last loaded slot", and no format change. The commit record (v1), file names and sidecar are byte-identical in layout. Older firmware reads every card this one writes, and the reverse.

**What it costs.**
- **A save's first card reads:** one generation's commit records plus its three files (about 7–10 KB).
- **A cold save** (after a reboot with no menu opened, or with the newest refused) also runs one gate verify. That is the same work as one slot of a cold list, about 0.39 s on the device by H-202's 785 ms for two.
- **A warm save** (after the title's list or an earlier save) skips it.
- **The small-heap peak** of a cold save is +32 B over a warm one (H1: 384,963 vs 384,931 host bytes). The verify runs and is released before the export, so its document never coexists with the save's own. Retention is unchanged (H2).

### 3.4 Tests

`a4_save1_recovery_runtime` (new ctest). It runs the real `alpha_save.cpp` over the fake SD card (`sd_shims`, as `a3_04g_storage_runtime`) and the real `AlphaRuntime`. A fresh service object models a reboot.

| Group | What it proves |
|---|---|
| A1–A4 | Five saves on a blank card: sequence k in slot k&1, the other slot byte for byte, a reboot's Continue restores each save, both listed valid. |
| B1–B2 | Torn newest, and newest refused by the semantic gate: Continue restores the older generation (unchanged behaviour). |
| Ca1–Ca5, Cb1–Cb5 | The defect, torn and semantically refused. Continue falls back; Save keeps the valid slot byte for byte; the refused slot is replaced as sequence 3 and accepted; a reboot loads the new save; two valid generations again. |
| D1–D6 (+ D1r–D6r) | Faults during that save: temp write, CRC readback, gam rename, json rename, commit rename, and a post-write check that fails after the commit. Each save reports failure, the valid slot is byte for byte and Continue recovers. A retry in the same session succeeds without touching the generation it loaded. |
| E1–E3 | After the recovery save: the next saves alternate slot 1, slot 0, and after a reboot slot 1 again. The sequences are 4, 5, 6. |
| F1–F3 | Both refused: Continue refuses and leaves the live game untouched. A save then succeeds. With a refused newest and no older commit, the save succeeds and loads. |
| G1–G4 | Sequence 7 in slot 0 with 4 in slot 1; `0x100000001` vs 2; a tie (9/9, Continue takes slot 0 and the save keeps it); a refused newest in slot 1. |
| R1–R4 | Through the runtime's own service with a warm list: Alt+S ×2, menu, the newest torn behind the service, Alt+L, Alt+S. The 701 generation is kept although the list still named the torn one valid. Repeated with the menu opened after the damage. |
| H1–H2 | Heap: cold vs warm save peak and retention. |

- **RED-first:** `native/core/tools/a4_save1_red_first.py build-a4-save1` swaps HEAD's `alpha_save.cpp` and `alpha_save_generation.{h,cpp}` into the tree. The file is converted to the checkout's line endings, then built, run and restored.
  - **On HEAD's production: 19/45 GREEN, 26 RED** (`a4-save1-red-first.log`). A, B, F1, F2, R1, H and the D1/D2 first saves are GREEN; every C, D-rename, retry, E, G and R2–R4 check is RED.
  - **After the fix: 45/45.**
- **Intentional expectation update:** `a4_ui2_save_menu_runtime` L8 recorded the defect: "Saving now replaces the damaged generation? No: it replaces the OLDER one". The Backup row then read "damaged".
  - The new check L8a asserts the fix: after the save, Latest is 15:20 and Backup is the 13:05 generation Continue fell back to, valid.
  - L8 keeps its purpose (a damaged backup is listed and refused) by damaging the backup directly with the new stub seam.
  - That test is now 23/23; it was 22.
- **Unchanged and GREEN:** `a3_04g_storage_runtime` (including its K7–K9 failed-save cases and the R peak bounds), `batch28_save_validation`, `batch24`–`batch27`, `a3_hf4_load_transient_runtime`, `batch53*`.
- **Host suite (serial):** **167/167** (160.7 s, `a4-save1-ctest.log`). That is 166 plus the new target, with zero project warnings.

### 3.5 Firmware

- **Build:** `idf.py -B build-a4-save1 reconfigure`, then `ninja -j 4`. It built first time, with zero project warnings. `PROJECT_VER` is `4.0.0-alpha4-save1-debug`.
- **Size:**
  - binary `0xf2af0` (994,032 B), **+800 B** against A4-UI2 (`0xf27d0`);
  - 54,544 B (5 %) of the 1 MiB slot is free;
  - flash only: `.text` +708, `.rodata` +96;
  - IRAM, DIRAM, `.data` and `.bss` are unchanged (`a4-save1-fw-size-diff.log`, against `build-a4-ui2`).
- **Post-commit image:** after the commit, `idf.py reconfigure` and a rebuild embed the commit's Git hash (`a4-save1-postcommit-*.log`).

### 3.6 Hardware validation (PASS)

Status: **software-complete, hardware PASS / COMPLETE.** The SD packs are unchanged from UI1/UI2.

The physical T-Deck validation passed every planned check (§3.7) on the image `4.0.0-alpha4-save1-debug`, embedded Git `5193a81f`, 994,032 B (`0xf2af0`), SHA-256 `a1e4d613bae4c0ed9ea48b7ef39af1af8adc143b9029239e0272925c1f9ad1fa`:

- normal two-generation Latest/Backup save and load, both generations loading;
- deliberate corruption of the newest generation (its generation JSON deleted);
- damaged-newest detection, and Continue's fallback to the older valid generation at the right place;
- a Save Game immediately after the fallback: the list showed the new save as Latest and the recovered generation as a healthy Backup (not reported damaged), and loading the Backup restored the recovered pre-save state;
- a full power cycle: Continue loaded the newly committed generation and Backup still loaded the protected prior one;
- a further ordinary save: Latest/Backup rotation was back to normal, with no persistent recovery-mode behaviour;
- no crash, lockup or watchdog.

**Significance.** After Continue falls back from a rejected newest generation to an older valid one, the next save does not overwrite that only known-good generation. The rejected generation is expendable, the recovered one stays as Backup, and normal rotation resumes. No serial log was supplied with the result, so none is recorded here.

**Closeout.** The closeout commit is documentation and evidence only. It was not built into, or tested as, firmware; the hardware-tested image embeds `5193a81f`. Closeout checks: the serial host suite is **167/167** (`a4-save1-closeout-ctest.log`); the retained post-commit build/package evidence is `a4-save1-postcommit-{configure,build,package}.log`. No production code changed and nothing was flashed at closeout.

To protect the real save, the test works on a copy of the card's saves and restores them at the end. The steps are in §3.7.

### 3.7 Hardware checklist (A4-SAVE1 image)

0. **Protect the real save.**
   - With the T-Deck off, put the SD card in a PC.
   - Copy the whole folder `ultima5/saves/` to the PC (for example `saves-before-a4-save1/`).
   - Put the card back. Flash the image.
   - The boot screen must show `FW 4.0.0-alpha4-save1-debug` and the Git hash of the A4-SAVE1 commit. Stop if it does not.
1. **Ordinary save/load.**
   - Continue. Walk somewhere recognisable (place **X**), then Alt+M → Save Game. The footer reads "Saved. The previous save is now the backup."
   - Walk to a different place **Y**. Save Game again.
   - Load Game shows **Latest: …, Y** and **Backup: …, X**. Load the Backup (you are at X), then Load Game → Latest (you are at Y).
2. **Damage the newest generation (on the PC).**
   - Power off and put the card in the PC.
   - In `ultima5/saves/` find the pair whose files are newest by modified time: `alpha1-g0.*` or `alpha1-g1.*`. Call its digit **N**.
   - Delete only `alpha1-gN.json`. Keep `alpha1-gN.commit`, `.gam` and `.ool`.
   - Put the card back.
3. **Fallback.**
   - Boot, then Journey Onward. The subtitle reads "Latest save damaged; Continue uses the backup". Load Game shows **Latest: damaged**.
   - Continue. You are at **X**.
4. **The A4-SAVE1 case: save after the fallback.**
   - Walk to a third place **Z**, then Save Game.
   - Open Load Game. It must show **Latest: …, Z** and **Backup: …, X**, **not "Backup: damaged"**. The old firmware overwrote X here.
   - Load the Backup: you are at X.
5. **Reboot and reload.**
   - Power-cycle, then Continue. You are at **Z**.
   - Load Game → Backup: you are at **X**.
6. **Rotation resumes.**
   - Continue (you are at Z). Walk to **W** and Save Game.
   - Load Game shows **Latest: W** and **Backup: Z**.
   - Optional, on the PC: `alpha1-gN.json` exists again, and both pairs have four files.
7. **Restore the real save.**
   - Power off.
   - On the PC, delete `ultima5/saves/` on the card and copy `saves-before-a4-save1/` back as `ultima5/saves/`.
   - Boot and Continue: your own journey loads.

**PASS** requires steps 1–6 as written, with no crash, lock or watchdog. **Report back:** the `FW`/`Git` lines and PASS/FAIL per step. If a serial log is attached, each save prints `SAVE_TARGET newest=<slot> accepted=<0|1> slot=<written> keep=<kept>`. At step 4 expect `accepted=0 slot=N`.

## 4. A4-SAVE2 — manual save slots

**Status: software complete. Hardware validation PENDING (§4.11). Not closed.** No commit or tag has been made. That waits for the hardware review, as in every Alpha 4 batch so far.

This batch implements the design of §2.4. The storage and recovery rules of A4-SAVE1 (§3) are unchanged; they now run once per slot.

**Baseline.** HEAD `09cc4600` on `main`, clean tree. Serial host suite 167/167 (139.2 s, `native/core/a4-save2-baseline-ctest.log`). The docs matched the code. The roadmap has no separate "manual load" task (§2.4 is one design, and §3 kept it deferred), so the load half is in scope.

### 4.1 What the card held before, and what Continue did

- **One journey, two generations:** `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`, each with a 32-byte v1 commit record (magic `0x31533555`, version 1, a 64-bit sequence, three CRC-32s).
- **Save (A4-SAVE1):**
  1. Read both commits.
  2. Ask the gate about the newest (`generation_accepted`: file CRCs every time, plus the full gate unless the InspectCache knows that commit).
  3. `choose_save_target()` picks the target. The write order is temps+fsync, CRC readback, three renames, the commit last, then the post-write gate.
- **Continue / Alt+L / the "Latest" row** = `load()`: the newest generation the gate accepts, else the other. **"Backup"** = `load_slot(physical generation)`.
- **The New Journey save** went into the same pair, so the journey it replaced became the Backup.

### 4.2 The architecture: three logical slots, one A/B pair each

| Player sees | Physical files (in `/ultima5/saves/`) |
|---|---|
| Slot 1 | `alpha1-g0.*`, `alpha1-g1.*` — **the pre-SAVE2 pair, unchanged** |
| Slot 2 | `alpha1-s2-g0.*`, `alpha1-s2-g1.*` |
| Slot 3 | `alpha1-s3-g0.*`, `alpha1-s3-g1.*` |

Each generation has `gam`, `ool`, `json` and `commit`, plus `.tmp` names during a save. No name is a prefix of another slot's. The longest path, `/sd/ultima5/saves/alpha1-s3-g1.commit.tmp`, is 41 characters. Format details:
- The commit record (v1), the GAM/OOL/sidecar encodings and the save directory are byte-identical in layout.
- The slot is named by the file name only.

**One sequence across the card.**
- A save's sequence is above every commit on the card and above its own pair's newest: `choose_save_target(..., card_newest)`. Its default of 0 is exactly A4-SAVE1's rule.
- So "the slot saved last" is the slot whose newest commit has the highest sequence.

**The same save transaction for every slot.**
- `AlphaSaveService::save(..., slot)` runs the A4-SAVE1 transaction unchanged, on the chosen slot's pair: the same stages, order and `SAVE_TARGET` rule (the newest accepted generation is kept; a refused newest is the one replaced).
- It reads the other slots' commit records (32 B each) and nothing else of them. It writes only the chosen slot's files.
- `slot = -1` means "the slot holding the card's newest commit; on a blank card, Slot 1". That is where every pre-SAVE2 save went.

**Loading.**
- `load_slot(slot)` restores that slot's newest accepted generation, else the one before it. This is `restore_newest()` over its pair, the same A4-SAVE1 selection and gate.
- Continue is `load()` (§4.4).

**The player never sees A/B.**
- The menus list slots.
- A slot whose newest generation is refused is listed as "Last save damaged; Enter loads the one before". That is the only time the recovery generation shows.
- The UI2 Backup row is gone (§4.7).

API changes:
- `alpha_save.h`: `kSlots`, `save(..., int slot = -1)`, `load_slot(logical slot)`, `inspect_catalog(FrontendSaveCatalog&)`, `inspect_generations(slot, [2])` (`inspect()` = Slot 1's, kept for the storage tests), `last_slot()`.
- `frontend.h`: `FrontendSaveCatalog` / `SaveSlotStatus {Empty, Saved, Recovered, Damaged}`, `continue_slot()`, `first_empty_slot()`, `slot_loadable()`, `format_slot_row()`, `format_slot_detail()`. These replace `FrontendSaveSlot[2]` in the menus.

### 4.3 Migration and compatibility

**There is no migration step, by construction.**
- The pre-SAVE2 pair **is** Slot 1. Nothing is copied, renamed, converted or rewritten on boot, listing or load.
- A card from Alpha 3, UI1, UI2 or SAVE1 is therefore Slot 1 byte for byte.
- Repeated boots cannot remigrate or duplicate anything (test M3 counts zero writes, renames and unlinks over three boots).
- There is no "old versus new structure" to reconcile: the old structure is one of the new slots.

**Downgrade.**
- An older firmware reads only `alpha1-g{0,1}`, i.e. Slot 1, and ignores `alpha1-s2-*` and `alpha1-s3-*`.
- A SAVE2 save into Slot 1 is exactly what an older firmware reads as its newest (test M6, through an independent legacy reader).

**Downgrade, save, upgrade.**
- The older firmware numbers its Slot 1 save from Slot 1's own newest, so it can equal or trail a Slot 2/3 sequence.
- The tie goes deterministically to the lower slot (M7).
- If it trails, Continue names the other slot. Nothing is lost: every slot still loads.

**Unchanged:** `settings.json`, the resource pack (UI1's, 2,042,554 B, CRC `0x9c10874f`) and the audio pack. **No SD-card change is needed.**

### 4.4 Continue, Alt+S / Alt+L, New Journey

**Continue** (title Continue):
1. Order the slots by their newest commit's sequence, newest first; a tie goes to the lower slot.
2. Restore each in turn as `load_slot` does (its newest accepted generation, else the one before), until one restores.

What this means in practice:
- It is **not** "Slot 1". It resumes the journey saved most recently, whichever slot holds it (tests S1–S3, R5).
- A slot whose latest save is damaged restores the save before it, not another slot's (S4).
- Continue moves on to the next most recent slot only when a slot has nothing loadable (S5).
- On a pre-SAVE2 card this is the pre-SAVE2 load exactly.
- The title shows the slot: "Latest save: *leader*, *place*" and "Enter continues Slot N; Mic returns".

**The journey's slot.** `last_slot()` is the slot of the last successful save or load. It is RAM only, never on the card.
- **Alt+S** saves the live journey into its own slot, without a question. This is the quick save, as before, and the save it replaces becomes that slot's hidden recovery copy.
- **Alt+L** reloads the journey's slot, not Continue's (R2).
- With no journey slot yet (e.g. a Developer entry), Alt+S uses the slot holding the card's newest save (a blank card: Slot 1), as every pre-SAVE2 save did, and Alt+L is Continue.

**Create New Character** takes the lowest empty slot, without a question. The main-menu footer names it ("New journey: saved in empty Slot 2").
- With every slot in use it lists the slots ("Every slot is in use. Choose one to replace").
- Then it asks **"Replace Slot N?"**, with **No, keep it** first. No and Mic return, and nothing is written until the character is complete.
- Until A4-SAVE2 a new journey always demoted the current save to the Backup.

### 4.5 The menus

| Screen | A4-UI2 | A4-SAVE2 |
|---|---|---|
| System Menu rows | Resume · Save Game · Load Game · Settings · (Developer) · Return to Title | unchanged |
| Save Game / Load Game footers | "Saves now; the previous save becomes the backup" / "The latest save and its backup" | "Choose a slot to save this journey in" / "Choose a saved journey to load" |
| Save Game | saved at once | **Save Game** page: "Slot 1: *leader*, *place*" / "Slot 2: empty" / "Slot 3: damaged". The cursor starts on the journey's slot (none: the first empty). An empty slot saves at once. |
| Occupied slot | — | **"Overwrite Slot N?"**, subtitle the slot's row, rows **No, keep it** (selected) / Yes, overwrite. No: back on the Save page, "Slot N kept". Back: the Save page. Yes: saves. |
| After a save | "Saved." / "Saved. The previous save is now the backup." | "Saved in Slot N." / "Save failed. Your previous save is kept." |
| Load Game | Latest / Backup | Slots 1–3, the cursor on the journey's slot (none: Continue's). Footer: "4-5-139 13:05, party of 2. Enter loads" · "Last save damaged; Enter loads the one before" · "Damaged: this slot cannot be loaded" · "Empty slot". Enter on an empty or damaged slot: "Slot N is empty" / "Slot N is damaged and cannot load", and nothing loads. |
| Journey Onward | subtitle names the latest save | the same, and the footer names Continue's slot |
| Title Load Game | Latest / Backup | the same slot list; Back returns to Journey Onward |
| Create New Character footer | "New game: your current save becomes the backup" | "New journey: saved in empty Slot N" / "Slots full: you choose one to replace" |

- Rows are still cut to 36 cells; every footer fits 50 (Z1, Z2).
- The renderer is unchanged: the pages are ordinary menu views. No golden changed. `a4_ui1_chrome_runtime` G1's System Menu root is identical.
- The Save page's footer uses the save wording ("Enter replaces it").

### 4.6 Building the slot list: what it reads

`inspect_catalog()`, per slot:
1. Read its two commit records (32 B each; an empty slot costs two failed opens).
2. The **newest** generation: its summary from the save list when the list holds an accepted summary for that exact commit; otherwise read and gate it.
3. The one before it **only when the newest is refused**.

Each staged document is released before the next generation is read, so a list never holds two documents. The status is exactly what `load_slot` would do. A save already records its own generation in the list, so the menu after a save costs the commit reads alone. Measured on the host over the real `alpha_save.cpp` (tests H1–H4):

| Card | Opens (failed) | Bytes read | Generations staged |
|---|---|---|---|
| Pre-SAVE2 shape (Slot 1 pair only), cold, A4-SAVE2 list | 5 (4) | 5,138 | 1 |
| The same card, the pre-SAVE2 list (both generations) | 8 | 10,212 | 2 |
| Three slots × two generations, cold | 15 (0) | 15,414 | 3 (the newest of each) |
| The same, warm (unchanged card) | 6 | 192 | 0 |

- **Small-heap peak.** The cold three-slot list peaks at one staged document's worth (378,751 host bytes, equal to a single pair's verify) and keeps 32 B.
- **What to expect on the device (estimate, not measured).**
  - Cold: H-202 measured about 390 ms per staged generation, so an existing one-journey card lists in about half the pre-SAVE2 cold time. A full three-slot card costs about 3 × 390 ms, once per boot.
  - Warm: six commit reads (the pre-SAVE2 warm list was two, about 83 ms).
  - Each open logs one `SAVE_CATALOG slot1=… slot2=… slot3=… staged= bytes= commit_us= verify_us= total_us=` line on serial (§4.11 asks for it).
- **Validation is not weakened for speed.** Every load re-verifies; a stale list can never make a damaged generation load (a3_04g V3).

### 4.7 Tests

**New ctest `a4_save2_slots_runtime`: 65 checks.** It runs the real `alpha_save.cpp` over the fake SD card (sd_shims, as `a4_save1_recovery_runtime`) and the real `AlphaRuntime`. The layout names are written out in the test, never taken from production. A fresh service is a reboot.

| Group | What it proves |
|---|---|
| E1–E3 | A card with no directory or an empty one lists three empty slots (six failed commit opens, no writes); Continue and every slot load refuse and change nothing. |
| I1–I7 | Distinct states (gold, karma, position, clock) saved to Slots 1, 2, 3: each save creates only its own four files, the other slots stay byte for byte, and each slot restores its own state after a reboot. A second Slot 2 save rotates only Slot 2's pair (sequence 4 = the card's newest + 1) and keeps its previous generation valid. |
| S1–S6 | Continue restores the slot saved last (not Slot 1). A torn latest in that slot restores its previous save, not a newer-numbered other slot. A wholly damaged slot passes Continue to the next most recent. A tie goes to the lower slot, in the list and in the load. |
| O1–O5 | Through the real System Menu: an empty slot saves without a question. "Overwrite Slot 2?" starts on No. No and Back perform zero writes, renames or unlinks, and the whole card is byte for byte (closing the menu on the question leaves every save byte for byte). Yes replaces the slot. The replaced save is that slot's fallback. |
| D1–D7 (+D1r–D6r) | Slot 2's newest torn (the A4-SAVE1 case), then each fault stage of the save: temp write, CRC readback, gam rename, json rename, commit rename, and a post-write check that fails after the commit. The save fails. Slot 2's previous save is byte for byte and restores. The bad candidate is not listed. Slots 1 and 3 are byte for byte and restore their own. The retry succeeds. D7: a failed first save into an empty slot leaves it listed empty. |
| G1–G13 | Inside one slot: both valid; newer torn; older torn; GAM truncated; commit missing, bad magic, unknown version, truncated; semantically refused; sequence order against file order; equal sequences; both refused (nothing restores, live game untouched); saving into a wholly damaged slot. Each also checks the list's status and Slot 1. |
| M1–M7 | A pre-SAVE2 card written by an **independent legacy writer**, not `AlphaSaveService`. It lists as Slot 1, and Continue and Slot 1 restore it with zero writes, renames and unlinks, over three more boots. Its own fallback works. A Slot 2 save leaves the old pair byte for byte and still the newest for an independent older-firmware reader. A save naming no slot rotates the old pair only. After a downgrade, the tie goes to Slot 1. |
| R1–R7 | The runtime: Slot 2 loaded, then Alt+S saves Slot 2 (not the newest Slot 3). Alt+L reloads Slot 2 when Continue would pick Slot 3. Save Game to Slot 1 moves the journey. Create New Character through the title fills the empty Slot 3 alone; Continue after a reboot restores it (leader Nova). With every slot in use, "Replace Slot 1?" starts on No and No writes nothing; Yes replaces Slot 1 alone. |
| H1–H4 | The list's opens, bytes and small-heap peak (§4.6). |

**SAVE1 regression.**
- `a4_save1_recovery_runtime` is **unchanged**: not one line of the test was edited. It passes **45/45** over Slot 1, so every A4-SAVE1 guarantee holds for the pre-SAVE2 pair.
- `a3_04g_storage_runtime`, `batch28_save_validation`, `batch24`–`batch27`, `a3_hf4_load_transient_runtime` and `batch53*` are green; the changes to them are listed below.

**No RED-first run.** The tested API (`inspect_catalog`, `save(slot)`, the slot menus) does not exist at HEAD, so the new tests cannot build against it. The proof is the mutation driver instead:
- `native/core/tools/a4_save2_mutation_check.py <build> [ids]` runs **25 mutants** against `a4_save2_slots_runtime`, plus `a4_save1_recovery_runtime`, `a4_ui2_save_menu_runtime`, `a3_04g_storage_runtime` and `frontend_tests` where each is the natural witness.
- **Result: 25 / 25 killed, 0 survived, 0 invalid; restored build GREEN** (`native/core/a4-save2-mutation.log`). It covers every class the brief names:

| Class | Mutants |
|---|---|
| Wrong-slot selection | S1 (all slots write Slot 1's names), S2 (Slots 2/3 misnumbered), S3 (the requested slot ignored), S19 (Alt+S ignores the journey slot), S20 (Alt+L reloads Continue), S23 (the menu's slot dropped), S24 (the save list keyed by Slot 1) |
| Destroyed fallback | S5 (a refused newest overwrites the older), S7 (no in-slot fallback on load) |
| Migration choosing the wrong source | S17 (the old pair not Slot 1), S18 (a no-slot save on a blank card goes to Slot 3) |
| Overwrite-No modifying the slot | S12 (the question starts on Yes), S13 (No saves), S14 (an occupied slot saved without asking) |
| Invalid generation accepted | S8 (the list skips the gate), S11 (a semantically refused generation passes verify) |
| Other | S4 (no card-wide sequence), S6 (Continue oldest first), S9 (the list never looks behind a refused newest), S10 (the list stages the backups: the slow-SD cost), S15/S16 (an empty slot offered for loading, System Menu / title), S21/S22 (New Journey ignores the empty slot / asks with Yes first), S25 (the menu is not given its own save) |

- The first run had two survivors, S16 and S25. Neither was a production defect: no runtime check pressed Enter on the title's empty slot, and none re-read the Load page after an in-menu save. `a4_ui2_save_menu_runtime` gained F3b and M11, and the driver gained the `frontend_tests` and `a3_04g` witnesses. On that run S1–S3 had also killed the test through an exception; a missing file is now a RED, not a crash.

**Intentional expectation updates.** The tests changed only where the menus changed:
- **`a4_ui2_save_menu_runtime`:** rewritten for the slot presentation; it keeps UI2's M/L/F/Z purposes. 23 → 33 checks: M1–M11, L1–L11, F1–F7 with F2b and F3b, Z1–Z2. It includes the SAVE1 case inside a slot (L6–L8).
- **The menu-save helpers:** a3_01, a3_02 and a3_03 runtime, and batch24, 26, 27, 28, 53 and 53a. Save Game now opens the Save page: Enter picks the journey's slot, and an occupied slot asks, answered Yes. What each helper saves is unchanged.
- **The Load page refuses an empty or damaged slot itself** (footer notice), where it used to attempt the load and print "No valid save". The "nothing changed" half of each check is kept:
  - `batch27` X1c/X2c;
  - `a3_hf4` L7.1m;
  - `batch28` G1/G4.
  
  Alt+L and the title still attempt the load and print it.
- **`batch28`:** the OLD oracle is now a card holding OLD alone (the Backup row that loaded it is gone). The physical-generation constants are renamed (`kOldGen`/`kNewGen`). F3 is Slot 1 with its newest refused: listed so, and the load falls back, OLD whole. F3b: a refused older generation stays hidden, and Slot 1 loads NEW.
- **`a3_04g_storage_runtime`:**
  - `load_page()` reads Slot 1's row and footer.
  - K3a, K5a, K6, K7, K8 and K10 compare with a cold `inspect_catalog()`, through the runtime's service (`save_service_for_test()`).
  - K5a: a deleted newest shows the older generation.
  - K6: "Last save damaged; Enter loads the one before".
  - K10: the clock is changed before the in-menu save, and the question is answered.
  - V3: both generations are corrupted, since a slot load falls back.
  - K1 (two 32-byte reads per open on a one-slot card) is unchanged.
- **`frontend_test`:** the Load flow by slot; Recovered and Damaged; the New Journey slot choice.
- **Signature only:** `a3_01_audio_contract`, `a3_05_audio_mute`, `a4_ui1_chrome_runtime` (`SystemMenuSession::open(settings, catalog)`).
- **The memory stub** (`alpha_save_memory_host_stub.cpp`) holds three pairs and calls the same production target, gate and restore functions. Its seams act on the journey slot (the one with the card's newest commit), so a one-journey test sees what it saw.

### 4.8 Host results

**Serial suite: 168/168** (137.1 s, `native/core/a4-save2-ctest.log`). That is 167 plus the new `a4_save2_slots_runtime`. Zero project warnings; the one GCC `stl_uninitialized.h` false positive predates Batch 19.

| Test | Result |
|---|---|
| `a4_save2_slots_runtime` (focused) | 65 / 65 |
| `a4_save1_recovery_runtime` (A4-SAVE1, unchanged) | 45 / 45 |
| `a4_ui2_save_menu_runtime` | 33 / 33 |
| `a3_04g_storage_runtime`, `batch28_save_validation`, `batch24`–`27`, `a3_hf4_load_transient_runtime`, `batch53*`, `frontend` | green |

### 4.9 Firmware

- **Build:** `idf.py -B build-a4-save2 reconfigure`, then `ninja -j 4`. It built the first time, with zero project warnings. `PROJECT_VER` is `4.0.0-alpha4-save2-debug` (`native/core/a4-save2-fw-{configure,build}.log`).
- **Size:**
  - binary `0xf3980` (997,760 B), **+3,728 B** against A4-SAVE1 (`0xf2af0`);
  - 50,816 B (5 %) of the 1 MiB slot is free. ESP-IDF's "nearly full" notice has shown since UI2.
- **By section:** `.text` +2,864, `.rodata` +864, internal `.bss` **+272** (the menus' three-slot lists replacing two generation summaries). IRAM and `.data` are unchanged (`a4-save2-fw-size-diff.log`, against `build-a4-save1`).
- **PSRAM:** the save workspace grows by about 0.6 KB for the per-slot save list and commit records (host 64-bit build: 11,888 B).
- **Card:** a full card holds 6 generations × about 5–7 KB. The 32 KiB free-space rule is unchanged.
- **Built after the batch commit:** as always, the hardware image must be rebuilt after the commit so it embeds its Git hash (`idf.py reconfigure`).

### 4.10 Limitations and notes

1. **Device timing is estimated, not measured.** Host counts prove the list reads one generation per slot. The device's cold and warm list times need the `SAVE_CATALOG` serial line from §4.11.
2. **A replaced slot's hidden copy.** After Create New Character replaces a slot, that slot's recovery generation is the replaced journey until the next save into the slot.
3. **Downgrade, save, upgrade** can make Continue name another slot first (§4.3). Nothing is lost.
4. **Continue's last resort.** When the most recent slot has nothing loadable, Continue restores the next most recent slot, which may be another journey. The title footer names the slot.
5. **Loading keeps its old behaviour.** A menu load still does not ask about unsaved progress, as before.
6. **Out of scope:** named saves, timestamps beyond the game date shown, more slots, and backup selection.

### 4.11 Hardware checklist (A4-SAVE2 image)

Do the steps in order. Each needs only the T-Deck, except steps 0, 2, 8 and 11, which need the SD card in a PC. The places A–D can be anywhere you will recognise. Z-stats (the party's gold and food) is a second check of each state.

0. **Protect your real save.** With the T-Deck off, copy the SD card's whole `ultima5/saves/` folder to the PC (for example `saves-before-a4-save2/`). Flash the image. The boot screen must show `FW 4.0.0-alpha4-save2-debug` and the Git line of the image you were given (built after the A4-SAVE2 commit). **Stop if it does not.**
1. **Your existing save is Slot 1 (migration).**
   - Journey Onward: the subtitle names your journey, and the footer reads "Enter continues Slot 1".
   - Load Game: **Slot 1** shows your leader and place; Slots 2 and 3 read "empty".
   - Continue: your journey loads where you left it.
2. **No migration writes (PC).**
   - Power off and put the card in the PC. `ultima5/saves/` still holds only `alpha1-g0.*` and `alpha1-g1.*`, eight files, with the same dates as the backup from step 0.
   - Put the card back.
3. **Empty behaviour.**
   - Power off. On the PC, rename `ultima5/saves` to `ultima5/saves-hold`, then put the card back and boot.
   - Journey Onward reads "No saved journey on this card".
   - Load Game shows Slot 1, 2 and 3 all "empty". Enter on one says "Slot 1 is empty" and nothing loads.
   - Main menu: highlight Create New Character. The footer reads "New journey: saved in empty Slot 1".
   - Create a character (any name). In the game, Alt+M → Save Game: Slot 1 shows the new leader, and Slots 2 and 3 read "empty".
4. **Three distinct saves.**
   - Walk to place **A** and note Z-stats. Save Game, Slot 1, Enter: the question **"Overwrite Slot 1?"** appears with **No, keep it** highlighted. Press Down, then Enter on Yes. The footer reads "Saved in Slot 1."
   - Walk to place **B**, note Z-stats, then Save Game, move to Slot 2, Enter. It saves at once, with no question: "Saved in Slot 2."
   - Walk to place **C**, note Z-stats, then Save Game, Slot 3, Enter: "Saved in Slot 3."
5. **Restore.**
   - Load Game shows three rows with their places; each row's footer gives its date and time.
   - Load Slot 1: you are at A with A's Z-stats. Load Slot 2: B. Load Slot 3: C.
6. **Overwrite NO.**
   - Load Slot 2 (you are at B). Walk to a new place **D**.
   - Save Game: the cursor is on Slot 2. Press Enter. At "Overwrite Slot 2?" press Enter on **No, keep it**. The footer reads "Slot 2 kept".
   - Press Enter again, then Mic at the question: you are back on the Save page. Close the menu.
   - Load Game → Slot 2: you are back at **B**, not D.
7. **Overwrite YES.**
   - Walk to D again. Save Game → Slot 2 → Enter → Down → **Yes, overwrite** → Enter: "Saved in Slot 2."
   - Load Slot 1: A. Then Slot 2: **D**.
8. **Continue.**
   - Alt+M → Return to Title → Journey Onward. The subtitle names D, and the footer reads "Enter continues Slot 2". Continue: you are at D.
   - Load Slot 3 (C), walk a few steps, press **Alt+S** ("Save complete"), and remember where you stand (**C'**).
   - Return to Title. The footer reads "Enter continues Slot 3". Continue: you are at C'.
   - Load Slot 1 (A), walk a few steps, then **Alt+L**: you are back at A (Slot 1), not at the newest save (Slot 3).
9. **Power cycle.**
   - Power off and on. Journey Onward: the footer names Slot 3. Continue: C'.
   - Load Game: the three rows are as before, and each slot loads its own state (A, D, C').
10. **Every slot in use.**
    - Main menu: highlight Create New Character. The footer reads "Slots full: you choose one to replace". Press Enter: the list "Every slot is in use. Choose one to replace".
    - Pick Slot 1. The question **"Replace Slot 1?"** has **No, keep it** highlighted. Press Enter (No): back on the list. Press Mic: the main menu.
    - Journey Onward → Load Game → Slot 1: still A.
11. **Optional: recovery inside a slot (PC).**
    - Power off. In `ultima5/saves/`, find the newest `alpha1-s2-g0.*` / `alpha1-s2-g1.*` pair by modified time (that is D). Delete only its `.json`.
    - Boot. Load Game: Slot 2's footer reads "Last save damaged; Enter loads the one before". Enter: you are at **B**.
    - Slots 1 and 3 still load A and C'.
12. **Restore your real save.**
    - Power off. On the PC, delete `ultima5/saves` (the test saves) and rename `ultima5/saves-hold` back to `ultima5/saves`. The copy from step 0 is the spare.
    - Boot and Continue: your own journey loads as Slot 1.

**PASS** requires steps 0–10, plus 11 if you do it, with no crash, lock or watchdog.

**Report back:**
- the `FW`/`Git` lines;
- PASS/FAIL per step;
- if you have a serial log, the `SAVE_CATALOG … total_us=` line of the first System Menu open after boot at step 9, and of a second open. Those are the cold and warm list times on a full card.

### 4.12 Status

| Axis | State |
|---|---|
| Software (host suite, focused, SAVE1, mutations, firmware build) | **complete** |
| Hardware validation (§4.11) | **pending** |
| Closed (commit, post-commit image, hardware PASS, closeout) | **no** |

*A4-CLOSE1 (2026-10-02): committed in `d6457ab1` (with A4-END1); still no hardware result of its own. Its checks are part 4 of the RC2 session (§14.7).*

## 5. A4-SAVE3 — original PC save import / export

The preservation goal: a save written by the original PC/DOS Ultima V can be brought into a Native slot, and a Native slot can be written back out as files the DOS game loads. This section is the compatibility contract. §5.1–§5.5 are the investigation, done before any production code changed; §5.6 onward is what was built on it.

**Baseline.** The working tree is A4-SAVE2 (§4, uncommitted). A fresh serial suite on it: **168/168**, 135.69 s (`native/core/a4-save3-baseline-ctest.log`).

### 5.1 The original PC save set (evidence, not the file extension)

The 1988 game writes a save in exactly one place, the Quit & Save handler `CAST2.OVL:0x10FE` (`re/notes/save-window-writer.md` §1, `re/notes/gfloor-146-acta.md` §2):

- `0x1185-0x1194`: **`SAVED.GAM`**, one `write` of `0x1060` = **4192 bytes** starting at `DS:0x55A6`. The file is a verbatim dump of the kernel's live-state window `[0x55A6, 0x6606)`; nothing is normalised between the "Y" and the dump.
- `0x1197`: **`SAVED.OOL`**, **512 bytes** from `0xB21E`: the BRIT block (256 B) followed by the UNDER block (256 B). Just before, `0x113E-0x1157` read the on-disk `UNDER.OOL` and `BRIT.OOL` into those buffers. The `.OOL` files are where the world the party is **not** in parks its 32×8-byte object table; the live table of the current world is inside the window (`DS:0x5C5A` = `SAVED.GAM` +0x6B4).
- Journey Onward (`INTRO.OVL:0x0f26-0x0f89`) reads `SAVED.OOL` back and splits it into `BRIT.OOL`/`UNDER.OOL`; it loads `SAVED.GAM` verbatim into the window.

**Accepted source files:** `SAVED.GAM` (exactly 4192 B) and `SAVED.OOL` (exactly 512 B), from the directory the DOS game runs in. Both are required: the DOS game always writes both, and Journey Onward reads both. `BRIT.OOL`/`UNDER.OOL` are scratch files the game regenerates from `SAVED.OOL`; they are not part of a save. `INIT.GAM`/`INIT.OOL` are the new-game templates, not saves.

**Fixture.** `original/u5/ultima5/SAVED.GAM` + `SAVED.OOL` (dated 2021-09-25, git-ignored like every original file) is a genuine DOS save that the Native writer never touched: the Avatar "Kojac", Shamino and Iolo in Lord British's Castle (location 17, floor 0, (15,20)), 9,200 gold, year 139 month 4 day 14 03:39, wind West, and 31 NPC slots live in the window's NPC tables.

### 5.2 What a Native save generation holds

A slot generation is four files (`alpha1-…g<N>.{gam,ool,json,commit}`, §4.2).

**`.gam` — the original format, rebuilt from INIT.GAM.**
- *Format:* the same 4192-byte `SAVED.GAM` layout (`docs/formats/tlk-npc-dataovl-gam.md` §4). No envelope, no Native bytes in unused space.
- *How it is written:* `export_native_state()` (`native/core/src/save_core.cpp` → `persistence.cpp export_native`) copies a **template** and patches the fields the codec models: the 16 character records, the scalars (food … torchTurns), clock, position, inventory arrays, moonstones, LB artifacts, shards, special items, quest bytes (`0x322-0x332`, word seals `0x32A`, doom bits `0x624`), the NPC dead/met bitmaps (`0x5B4`/`0x634`), the party/vehicle record `0x6B4-0x6BB`, and — outdoors — the monster slots of the object table. The template is **always INIT.GAM** (`alpha_runtime.cpp`: every `save_.save(…, resources_.initial_gam, …)`).
- *Measured* (scratch probe, recorded in `native/core/a4-save3-investigation.log`): the real DOS `SAVED.GAM`, imported with no sidecar and exported again:
  - with the DOS file itself as the template: **0 of 4192 bytes differ.** Every field the codec reads, it writes back exactly;
  - with INIT.GAM as the template (what Native does): **750 bytes differ.** They are the unmodelled bytes. In this town save almost all are the town's NPC tables `0x6BC-0x1017` (object slots, the live schedule `0x7B8`, NPC records `0x9B8`, path buffers `0xBB8`, type bytes `0xFF8`; layouts in `re/notes/npc.md` §0.2/§0.4), plus `0x2D2` (combat scratch), `0x2E1` (moongate animation), `0x2E9-0x2EC` (combat flags, **wind**), `0x2FF` (light radius) and `0x3A9-0x3B2` (the open-door tracker, **sail direction**, drunk timer, shadowlord-here).
- *Conclusion:* byte-compatible **format**, not byte-identical **content**. Everything Native needs to resume is in `.gam` + `.json` together; the `.gam` alone lacks the sidecar-only state of §5.3.

**`.ool` — the original format, the active world only.**
- *Original role:* the parked object tables of both worlds (§5.1).
- *Native role:* `build_ool()` copies the pack's `init.ool` (= an empty BRIT block ++ `INIT.OOL`, byte-identical to the `SAVED.OOL` the 1988 new game seeds, `FONT.OVL:0x0de7`) and, only when the party is outdoors, writes the current world's block: slot 0 (vehicle tile, x, y, floor, hull, skiffs) and the monsters. **No Native load ever reads it** (only its size and CRC are checked).
- *Measured:* with the DOS `SAVED.OOL` as template the rebuild is **0 of 512** bytes off; with `init.ool` it is 25 off for this save. (The fixture's `SAVED.OOL` happens to be the new-game one.)
- *Conclusion:* byte-compatible format; enough, with `.gam`, for DOS to load. It never carries a parked vehicle (§5.3, `worldObjects`).

**`.commit` — Native save management only.** 32 bytes, written raw (`AlphaSaveCommit`): magic `0x31533555`, version 1, a 64-bit card-wide sequence, and the CRC-32 of each of the three files (plus 4 uninitialised padding bytes). It is written after the three files are renamed into place and is never needed from, or given to, the PC.

**`.json` — the Native sidecar.** `{"version":1, "qol":{…}, "gameState":{…}}`, the keys of `persistence.cpp`'s whitelist. A sidecar-less import (`load_native_state(gam, nullptr, …)`, exactly how every New Journey loads INIT.GAM) starts from `empty_sidecar()`. §5.3 classifies every key.

### 5.3 The sidecar, field by field

Classes: **1** canonical (needed to resume, not recoverable from the original files) · **2** derived (reconstructable from the `.gam`/`.ool`, the game's own data files, or by a rule the runtime already runs) · **3** Native-only (no DOS equivalent) · **4** cache · **5** save-system metadata. "Codec" = Native's `.gam` codec already carries it. "B*n*" = carried by the SAVE3 bridge (§5.5).

| Key | Class | 1988 home | Import from a PC save | Export to a PC save |
|---|---|---|---|---|
| `version` | 5 | — | written as 1 | not written |
| `qol.journal` | 3 | — (retired reference feature, no reader) | `[]` (`empty_sidecar`) | omitted |
| `qol.explored`, `qol.treasuryLoot` | 3 | — (legacy; never written by Native) | absent | omitted |
| `gameState.transport` | 2 | derived from `g_transport_tile` `0x2D6` | **B9** `transport_mode(tile)` (the empty sidecar's `"foot"` was wrong aboard a vehicle) | `0x2D6` (codec) |
| `questFlags["search:N"]` | 2 | "found once" bitmap `0x2B6` (15 B; `SJOG:0x0514`, bit `1<<(N&7)` at `+N>>3`) | **B1**, N = 0…112 except 13/14/15 (gated separately) | **B1** |
| `questFlags["word-spoken:33…40"]` | 2 | bit 7 of `0x32A+i` | codec | codec |
| `questFlags["shadowlord-dead:…"]` | 2 | `0x322-0x324` ≥ 0x80 | codec | codec |
| `questFlags["in-doom"]` | 3 | none identified (set on entering Doom) | absent | omitted |
| `questFlags["game-won"]` | 3 | none (1988 never saves after the win; Native refuses to) | absent | omitted |
| `openDoors` | 2 | `0x3A9-0x3AC`, but every 1988 load zeroes it (`TOWN:0x0408`→`0x041d`), as Native's load does | `[]` | omitted (DOS clears it on load) |
| `mapOverrides` (location 0, horse/skiff tiles) | 2 | object-table records | **B10** from the tables | **B10** into the tables |
| `mapOverrides` (everything else) | 3 | none: 1988 re-reads every map from disk (`DS:0x6608` is outside the window) | absent | omitted |
| `skullTreeFoundDay` | 2 | `0x20C` (`[0x57b2]`) | **B6** | **B6** |
| `reagentPatchFoundDay` | 2 | `0x2B2-0x2B4` | **B7** | **B7** |
| `overworldEnemies` | 2 | object-table slots 1–23 of the live world | codec (outdoors); `[]` in a town (Native clears them on entry) | codec |
| `worldObjects`: frigates at location 0 | 2 | object-table records (live table or parked `.OOL` block) | **B10** | **B10** |
| `worldObjects`: interior chests/props/plot items | 2 | the town's NPC slots; re-seeded from the `.NPC` file on every map entry (`TOWN:0x1694`) | derived: fresh map entry (§5.6) | omitted (DOS re-seeds on entry) |
| `worldObjects`: underworld plot items | 2 | re-created on arrival | derived: `hydrate_underworld_plot` | omitted |
| `worldObjects`: search loot, summoned shadowlord | 3 | would be object records; not bridged | none | omitted |
| `lightSpellMins` | 2 | `0x300` | **B3** | **B3** |
| `timeSpell`, `timeSpellTurns` | 2 | `0x2D4`, `0x2E8` (0 = none) | **B8** | **B8** |
| `wind` | 2 | `0x2EC` (0 calm … 4 west) | **B2** | **B2** |
| `windDriftCtr` | 2 | `0x2DD` | **B4** | **B4** |
| `sailDir` | 2 | `0x3AF` | **B5** | **B5** |
| `shipHull`, `shipSkiffs` | 2 | party record `0x6B9`/`0x6BB` | codec outdoors; in a town the runtime default (hull 99) — the value is replaced from the ship's own record when boarding | codec |
| `hmsCapeToggle` | 3 | `DS:0xA524`, outside the window: 1988 never saves it | 0 | omitted |
| `shadowlordLocs`, `shadowlordSummoned`, `shadowlordDoomBits` | 2 | `0x322`, `0x325`, `0x624` | codec | codec |
| `dungeon` | 1 (as Native stores it) | `g_dng_map 0x3B4`, facing `0x105D`, the wanderer's object record | **not bridged: a save inside a dungeon is refused** | **refused** |
| `npcWalk` | 2 | the window's NPC tables `0x7B8-0x105B` | derived: fresh map entry at the saved hour (§5.6) | omitted: not bridged (§5.5 L1) |

Captured into the live document but persisted **nowhere** by Native, before and after SAVE3: `wornCrown`, `drunkTurns` (`0x3B1`), `chunkOrigin` (render-only). Neither direction carries them.

**Is the JSON fundamentally required?** No. For a PC save it is reconstructable: every class-2 key comes from the `.gam`/`.ool`/data files or a runtime rule, and every class-3/5 key has a documented default. Only `dungeon` (class 1 as stored) has no bridge, and it only exists inside a dungeon.

### 5.4 Two latent Native codec gaps found on the way

Both were measured with a synthetic outdoor variant of the fixture (party at (0x50,0x60), a frigate record in object slot 30):
1. **Sidecar-less vehicles are refused.** `import_native` turns a vehicle record into a reference-shaped entry `{"kind":"ship",…}` with no `plotZ`/`plot`/`ship` fields; `validate_world_objects` rejects it (`NativeDomain`), so the gate would refuse the whole generation and the runtime restore would empty the pool.
2. **Native's own vehicles never reach its `.gam`.** `object_table()` (ported from the reference) writes only entries whose `kind` is `ship`/`horse`; the pool is captured with a `ship` flag and no `kind`. A Native save outdoors next to its frigate has no frigate record in its `.gam`.

Neither matters to Native→Native saves (the sidecar carries the pool). Both matter across the PC boundary; the bridge (B10) handles them at the bridge, and the Native codec is not changed (its bytes are pinned by `persistence_parity`).

### 5.5 Compatibility determination: **Case C**

The `.gam`/`.ool` formats are original and the codec is exact for what it models (Case A would hold for those fields alone). But some representable state lives only in the sidecar, and some Native state has no DOS equivalent — **Case C**: import with documented defaults; export everything representable that the bridge can prove; name what is lost.

**The bridge** (`openu5::save::pc`, `native/core/src/pc_save.cpp`), on top of the unchanged codec:
- **B1** search-found bitmap ↔ `search:N`
- **B2** wind · **B3** light-spell minutes · **B4** wind-drift counter · **B5** sail direction · **B6** skull-tree day · **B7** reagent-patch days · **B8** time spell + turns — each a single byte whose cell the reference itself documents (`game/src/core/state.ts`, `SAVE_OPTIONAL_DEFAULTS`: "+0x2EC … verificado", "+0x300 … verificado", `[0x57b2]` ⇒ +0x20C, `[0x5858-0x585A]` ⇒ +0x2B2..0x2B4) and whose raw value the runtime already uses unchanged.
- **B9** transport mode from the vehicle tile (import).
- **B10** vehicles: frigates (`0x20-0x27`) ↔ the pool (`ship=true`, tile = 256 + byte, hull `+5`, skiffs `+7`); horses (`0x10/0x11`) and skiffs (`0x28-0x2B`) ↔ location-0 `mapOverrides` (how Native leaves them when the party dismounts). The live table (`.gam` +0x6B4) when the party is in that world outdoors, else the parked `.OOL` block (BRIT = surface, UNDER = underworld).

**Not bridged (the boundary; each is refused or named, never guessed):**
- **L1 Town NPC and object tables.** Native's `NpcActor` mirrors the 1988 record fields, but no codec between them has been proven against the binary, and the tables cross-link (`+0x0C objIdx` into the object table). *Import:* the imported journey resumes as DOS would after leaving and re-entering the map — NPCs at their posts for the saved hour, chests/props re-seeded (the same derivation `TOWN.OVL:0x11F0` runs with `fresh=1`, whose content the D1 act measured as faithful, `re/notes/npc-carga-partida-fresh-gate.md`). *Export:* the tables are INIT.GAM's (all zero); DOS loads the town with nobody in it until the party re-enters it (`fresh=0`), exactly the D1 observation of a port-written save in DOSBox.
- **L2 Dungeon saves.** Refused both ways with a clear message: Native keeps the session only in the sidecar and no DOS dungeon save has been available to prove a bridge.
- **L3 Parked monsters.** The `.OOL` blocks' monsters are not imported (Native clears monsters on entering a map); an export in a town leaves the new-game blocks.
- **L4 Carpets left standing, search-loot piles, a summoned Shadowlord** are not bridged (unverified encodings); the export manifest names them when present.
- **L5 Unmodelled bytes** (combat scratch, moongate animation, light radius, the door tracker, camp-heal cooldown `0x2E6`, drunk timer `0x3B1`, shadowlord-here `0x3B2`, and `0x3B0` = DS:0x5956, which the RE ledger has not named): export writes INIT.GAM's values; the 1988 game recomputes or resets most of them on load.

### 5.6 Import: what the player does and what runs

**Where.** Title → main menu → **PC Save Transfer** (the row before Developer; hotkey **P**) → **Import the PC save into a slot**. The files go in the SD card's `ultima5/import/` folder, named as DOS names them: `SAVED.GAM` and `SAVED.OOL`. The folder is the player's: nothing ever writes to it or moves anything out of it. The managed `ultima5/saves/` directory never holds raw PC files.

**What runs** (`AlphaRuntime::import_pc_save`, `alpha_runtime.cpp`; storage in `alpha_save.cpp`; conversion in `native/core/src/pc_save.cpp`):
1. **The page looks at the folder** (`read_pc_import`, `stat` of both names). A file of the wrong size is reported by its size and never read into memory. The page's subtitle says what it found: `PC save: Kojac, Lord British's Castle`, or why not.
2. **The check** (`pc::check_original`): exactly 4192 + 512 bytes; party size 1–6; each party record a character (printable name, gender `0x0B/0x0C`, class A/B/F/M, status G/P/S/D/C); a calendar date and time; location ≤ 40 with a floor and position the map can hold; wind and sail direction 0–4. A dungeon (33–40) is refused here (L2). INIT.GAM itself fails (its Avatar has no name).
3. **The slot.** An empty slot imports at once. An occupied or damaged slot asks **"Replace Slot N?"**; files imported before ask **"Import it again?"** — either way **No, keep it** is selected first, and No or Back writes nothing ("Import cancelled. Slot N is unchanged").
4. **The conversion** (`pc::import_original`): the codec reads `SAVED.GAM` with no sidecar, the NPC fidelity gate on — exactly how every New Journey reads INIT.GAM — then the bridge completes the document (B1–B10), then `restore_core` projects it into the game and turn state. Game, turn and document are assigned only if all of that succeeds.
5. **The live owners** (the title owns no live game, so this is what New Journey does): the runtime-only owners are cleared, then **`restore_gameplay` and `restore_terrain`** run over the document — what the generation gate restores for every load; an outdoor PC save has monsters, and the bridge's horses and skiffs are terrain cells — then `synchronize_loaded_world()`, the load's own sync.
6. **Native-only state for runtime operation (L1):** in a town, the fresh map entry — `hydrate_interior_objects` and `enter_npc_map` at the saved hour, the derivation `TOWN.OVL:0x11F0` runs with `fresh=1`. In the underworld outdoors, `hydrate_underworld_plot` (as the falls do).
7. **The write: `AlphaSaveService::save(..., slot)`**, the unchanged A4-SAVE1/SAVE2 transaction: target choice (the slot's accepted generation is kept), temps + fsync, CRC readback, three renames, the **commit record last**, then the post-write gate re-reads the generation from the card. The sequence is card-wide. The sidecar written is the capture of the live owners, so it is deterministic from the PC files and the game data.
8. **Loadable on its own:** the slot list is re-read from the card and must show the slot **Saved**. Only then is success reported ("Imported into Slot N. The PC files are kept"), and the marker `ultima5/pc-import.txt` (`gam=<crc32> ool=<crc32> slot=<n>`, outside both the import folder and `saves/`) records which files went where.

**Idempotency.** The source is never changed, and the page says it is kept. Importing again is always a deliberate choice: the page names the slot the same files went to ("PC save: Kojac, Lord British's Castle (in Slot 2)"), and even an empty slot then asks "Import it again?" first. Different files (other CRCs) are not affected by the marker.

**The imported slot is an ordinary slot.** Its generation, commit, CRCs, sequence and the save list's memory are the ones any save makes; Continue, Alt+S / Alt+L, the A/B rotation and the fallback treat it like every other slot (tests R1–R2). Its `.gam` is Native's own (rebuilt from INIT.GAM, §5.2), so the PC file's unmodelled bytes are not carried even before the first Native save.

**Errors** (each leaves the source files and the destination slot as they were): `No PC save in /ultima5/import` · `PC save incomplete: SAVED.GAM is missing` / `…SAVED.OOL is missing` · `SAVED.GAM is not 4192 bytes` / `SAVED.OOL is not 512 bytes` · `SAVED.GAM: party size is not 1-6` / `…party records are not valid` / `…game clock is not valid` / `…position is not valid` / `…wind/sail bytes not valid` · `Dungeon saves cannot be transferred` · `SAVED.GAM could not be read` (the codec) · `The PC save could not be read from SD` · `Import cancelled. Slot N is unchanged` · `Import failed. Slot N is unchanged` (an SD write failure or the post-write check; the slot's previous generation still loads) · `Imported save failed its check`.

### 5.7 Export: what the player does and what runs

**Where.** PC Save Transfer → **Export a slot as a PC save** → a slot → Enter. The files appear in `ultima5/export/slot<N>/`: `SAVED.GAM`, `SAVED.OOL` and `EXPORT.TXT`. Nothing else is written there (no `.json`, `.commit`, slot or sequence data).

**What runs** (`AlphaSaveService::export_pc_save`):
1. **The generation `load_slot()` restores**: the slot's newest generation the gate accepts, else the one before it, each read and verified by `verify_candidate` (one staged document at a time). A refused generation is never exported, even when the menu still offered the slot (the list's memory is keyed on commit records, A4-SAVE2 §4.6).
2. **Not in a dungeon** (`pc::exportable`, L2).
3. **The files:** `SAVED.GAM` = the generation's own `.gam` with B1–B8 written and B10's vehicles placed; `SAVED.OOL` = the generation's own `.ool` with B10's parked vehicles. The two tables written hold exactly the vehicles Native holds (the template's own vehicle records are removed first).
4. **Written safely:** each file beside its final name, read back, then renamed over it; a failure removes the temp and leaves any earlier export as it was.
5. **Checked:** both files read back from the card byte for byte, `check_original` accepts them, and Native's own codec reads them (`import_original`).
6. **The slot is only read.** Its files, commit and the save list's memory are unchanged.

**`EXPORT.TXT`** says which slot and save it came from and how to use it, how many vehicles were written, and what was **not** carried: the people and chests of the current town (L1), unplaced vehicles, carpets, items on the ground, a summoned Shadowlord (L4), and the Native-only state.

**Errors:** `Slot N is empty` · `Slot N cannot be loaded; nothing to export` (the menu) · `Slot N has no loadable save` (the export's own gate) · `Dungeon saves cannot be transferred` · `Export failed: SD card write error` · `Export failed its check. Nothing was replaced`.

### 5.8 Native-only state: the policy

| State | On import | On export |
|---|---|---|
| Slot, A/B generations, commit record, sequence | made by the ordinary save transaction | never written |
| Settings (`settings.json`) | untouched | never written |
| `hmsCapeToggle` (runtime only in 1988) | 0 | omitted |
| `openDoors` | none | omitted (every 1988 load clears the tracker) |
| `qol.journal` / `explored` / `treasuryLoot` | `[]` / absent | omitted |
| `in-doom`, `game-won` | absent | omitted |
| Town NPC walk state, interior objects | derived by the fresh map entry (L1) | omitted (L1) |
| Dungeon session | refused (L2) | refused (L2) |
| Search-loot piles, summoned Shadowlord, standing carpets | none | counted in `EXPORT.TXT` (L4) |
| `wornCrown`, `drunkTurns`, `chunkOrigin` | not persisted by Native at all, in any direction | — |

Nothing is encoded into unused original bytes: the bridge writes only the ten 1988 cells of B1–B8 and whole object records (B10), each with the meaning the DOS game gives it.

**Enhanced Mode.** SAVE3 depends on no enhanced UI feature. Any future enhanced-only persistent state belongs in the sidecar under its own key: the bridge never reads or writes sidecar keys other than the ones in §5.3, so such state cannot reach an exported file, and it takes its documented default when a PC save is imported (or re-imported after a DOS round trip). Nothing in gameplay changed in this batch.

### 5.9 Round-trip limitations (what a DOS round trip loses, on purpose)

- **Town NPCs and objects (L1).** An imported town save resumes as DOS would after leaving and re-entering the map: NPCs at their posts for the hour (mid-walk positions snap), chests re-seeded, a guard alarm's rewritten schedules forgotten. An exported town save loads in DOS with nobody in the town until the party leaves and re-enters.
- **Dungeons (L2).** Leave the dungeon before saving on either side.
- **Parked monsters (L3)** are not carried either way; vehicles are.
- **Carpets left standing, items on the ground, a summoned Shadowlord (L4).**
- **Unmodelled bytes (L5):** combat scratch, moongate animation, light radius, the door tracker, the camp-heal cooldown `0x2E6`, the drunk timer `0x3B1`, shadowlord-here `0x3B2`, the unnamed `0x3B0` (DS:0x5956), each record's `+0x18`. Export writes INIT.GAM's value; the DOS game recomputes or resets most of them on load.
- **A frigate's hull while the party is in a town** is imported as the runtime default (99); the value is replaced from the ship's own record when boarding.
- **Vehicles may move to another object record** (slot); tile, position, hull and skiffs are kept.
- **The new-game underworld skiff.** The 1988 new game parks a skiff at (14,242) in UNDER.OOL. An imported DOS journey keeps it (as a Native terrain cell) and exports it back once. A journey started on Native never had it, because Native does not read `.ool` (§5.10, item 4).

### 5.10 Findings outside the bridge (not changed here)

1. The codec's vehicle gap (§5.4): handled at the bridge; the codec is pinned by `persistence_parity` and was not changed.
2. **The place caption says "Britannia" in the underworld.** `hud_location_caption()` treats `floor < 0` as the underworld, but Native keeps the outdoor underworld as floor 255 (the falls, the codec, `build_ool`). The HUD strip and the save list pass it unchanged; `dungeon_input_test` D-LOC-1e tests only -1. The PC page converts `0xFF` to -1 itself. Not examined on hardware; queued separately.
3. **`wornCrown` and `drunkTurns` are captured into the document but persisted nowhere** (neither a `.gam` field nor the sidecar whitelist). The reference documents the drunk timer as reset by the town loader, but that write (`TOWN:0x1218`) is inside the `fresh=1` block the 1988 load skips (`npc-carga-partida-fresh-gate.md` §1). Not examined further.
4. **Native journeys lack the new-game underworld skiff** (Native never reads INIT.OOL). Not examined further.

### 5.11 Tests

New ctest targets (the suite goes from 168 to **170**):

| Target | What |
|---|---|
| `a4_save3_pc_bridge_runtime` | the REAL `alpha_save.cpp` over the fake SD card, the real `AlphaRuntime` and title screen, and the genuine DOS save; its variants (outdoors, vehicles, search bits, time spell, dungeon, damage) made by patching documented offsets, never by the Native writer. The byte offsets it reads are written out from the format document and the RE notes, not taken from the code under test. **57 checks** in groups P (checks and codec), J (import through the title), F (failures at every stage), X (export), T (the round trip, outdoors too), R (recovery afterwards), H (card I/O and heap). |
| `a4_save3_pc_reference` | the **independent reader**: `native/core/tools/check-pc-save.ts` runs the test above with `--emit`, then reads the emitted DOS fixture, the exported round trip and the vehicle layouts with the TypeScript reference's own `SAVED.GAM` parser (`importNativeSave`), and SAVED.OOL records and the sidecar-only cells by their documented offsets. **10 checks.** |

The brief's list, and where each is:

| Required | Checks |
|---|---|
| JSON reconstruction (no JSON in; generated fields correct; the load valid) | P4–P8, J7, J8 |
| Missing original files: rejected, no destination changes | J1, J2 |
| Corrupt / incompatible original rejected | P2, P3, J3, J4 |
| Slot independence (import into Slot 2; 1 and 3 unchanged) | J10, J11 |
| Occupied slot, No: destination unchanged | J12 |
| Interrupted import: old destination recoverable | F1–F4 (every stage of the slot transaction, and the post-write check) |
| Successful import becomes active | J5, J8, J9, J13, R1 |
| Original → Native state parity | P4, J8, TS1, TS2 |
| Native → original export matches the format | X1–X3, TS3–TS5 |
| Round trip, representable state survives | T1–T6, TS4–TS10 |
| Native-only state: defaults on import, omitted on export | J7, T4, X3, P14 |
| Recovery afterwards follows A/B rules | R1, R2 |
| Idempotency (the same files again) | J14, J15, J17 |
| Source files never modified | J6, J16 |
| Exporter never exports a damaged / refused generation | X4, X5, X5b |

**RED evidence.** The bridge is new API, so its tests cannot run against the pre-SAVE3 tree; the house rule's other route applies — every guard validated by mutating production code (§5.13). The pre-existing behaviours the bridge corrects are pinned RED in the suite itself: P9 (the codec alone refuses a DOS vehicle) and the investigation probes (`native/core/a4-save3-investigation.log`: 750 bytes lost to the INIT.GAM template, no frigate in Native's `.gam`). One defect was found by the independent reader during the work, not by the C++ checks: the outdoor import at first skipped `restore_gameplay`/`restore_terrain` (horses, skiffs and monsters were dropped); T5/T6 were added and mutant P25 re-creates that defect.

### 5.12 Host results

All runs serial, `native/core/build-a4-save3` (host toolchain as every batch).

| Run | Result | Log |
|---|---|---|
| Baseline (A4-SAVE2 tree, before any change) | **168 / 168**, 135.69 s | `native/core/a4-save3-baseline-ctest.log` |
| After the production changes, before the test updates | 166 / 168: `frontend` (Up from the top now wraps to row 8, Developer) and `a4_ui1_chrome_runtime` G1 (the main-menu golden has the new row) — the two expected, by design | `a4-save3-wip-ctest.log` |
| SAVE3 focused | `a4_save3_pc_bridge_runtime` **57 / 57**; `check-pc-save.ts` **10 / 10** | `a4-save3-focused.log` |
| SAVE2 regression | `a4_save2_slots_runtime` **65 / 65**; `a4_ui2_save_menu_runtime` **33 / 33** | `a4-save3-save2-regression.log` |
| SAVE1 recovery regression | `a4_save1_recovery_runtime` **45 / 45**; `a3_04g_storage_runtime` **44 / 44** | `a4-save3-save1-regression.log` |
| Full suite | **170 / 170**, 140.47 s | `a4-save3-ctest.log` |
| **Final re-run** (2026-09-30 evening, the final tree after the mutation pass restored every file; host build from that tree, 0 warnings) | full suite **170 / 170**, 177.90 s; focused `a4_save3_pc_bridge_runtime` **57 / 57** + `check-pc-save.ts` **10 / 10**; SAVE2 `a4_save2_slots_runtime` **65 / 65** + `a4_ui2_save_menu_runtime` **33 / 33**; SAVE1 `a4_save1_recovery_runtime` **45 / 45** + `a3_04g_storage_runtime` **44 / 44** | `a4-save3-final-{build,ctest,focused,save2-regression,save1-regression}.log` |

The two test updates: `frontend_test.cpp` line 42 (row 7 → 8, commented) and the A4-UI1 main-menu golden, re-recorded with `--record` (only that hash changed; its provenance comment says why). The other seven goldens are byte for byte.

Measured on host (H1–H2): an import reads the two PC files with at most 2 files open; an export's small-block heap peak is a load's (562,528 B vs 562,560 B: one staged document at a time) and keeps 0 B afterwards. Menu inspection cost is unchanged: the PC page reads the import folder (two `stat`s and 4.7 KB) only when opened, and its slot list is the warm catalog (commit reads alone).

### 5.13 Mutations

`native/core/tools/a4_save3_mutation_check.py <build> [ids]`: 30 mutants, each one or more edits of production code, built and run against `a4_save3_pc_bridge_runtime` (and, for the ones marked, the TypeScript reader too). **Final pass: 30 killed, 0 survived, 0 invalid; restored build GREEN** (`native/core/a4-save3-mutation.log`).

| Brief's class | Mutant(s) | Killed by |
|---|---|---|
| incomplete original set accepted | P1 (a missing SAVED.OOL read as an empty block) | J2 |
| incorrect JSON / default reconstruction | P2 (wind), P3 (transport), P4 (search bitmap one bit off) | P5/P7/J7/T3, P8, P6/T3 |
| wrong destination slot | P5 | J10, J11, J15, J17, T3, R1, R2 |
| source files modified | P6 (the marker written over `import/SAVED.GAM`) | J6, J16 and the J group |
| destination destroyed before the new generation validates | P7 (the save writes over the generation it keeps) | J13, F2, F3, X4, X5, R1, R2 |
| Native-only metadata in the original payload | P8 (the sidecar in the export folder), P9 (a U5PARTIDA envelope on SAVED.GAM) | X1; X1, X3, T1, T6 |
| exporter choosing the wrong / failing generation | P10 (older first), P11 (a refused generation exported) | T1-T3; X4, X5 |
| incompatible source accepted | P12 (dungeon), P13 (any party size), P14 (a longer SAVED.GAM) | P3, J4 |

The others: B10 import off (P15), export off (P16), the template's vehicles kept (P17, the new-game skiff twice), B1 export writing the gated entries (P18), B8 export dropping the time spell (P19), the overwrite question starting on Yes (P20), an occupied slot imported over without asking (P21), files imported before imported again without asking (P22), the marker matching any files (P23), no fresh map entry (P24), **no outdoor restore before the sync (P25, the RED-first of the defect the independent reader found)**, export changing the slot (P26), a dungeon document exported (P27), the wrong export folder (P28), the page not told where the files went (P29), a short read imported anyway (P30).

The first pass (`a4-save3-mutation-pass1.log`) had 27 killed, 1 survived and 2 invalid. P15 and P27 did not build (a mutant that removes a function's or variable's only use is refused by `-Werror`; they now empty the loop and keep a `(void)` use). **P29 survived**: every test left the page before importing again, so the stale-status case was untested. J17 (Import again at once, on the same page) was added, and P29 is killed by it (`a4-save3-mutation-pass1-rerun.log`, then the final pass).

### 5.14 Firmware

ESP-IDF 6.1, `native/targets/tdeck/build-a4-save3` (`idf.py reconfigure`, then `ninja -j 4 all`), first attempt clean, no project warnings (`native/core/a4-save3-fw-{configure,build}.log`).

- **Image `0xf72f0` (1,012,464 B)**: +14,704 B against A4-SAVE2's `0xf3980`; **36,112 B (3 %) of the 1 MiB app partition free** (was 50,816 B). `native/core/a4-save3-fw-size-diff.log`.
- Flash `.text` +11,552 (`pc_save.cpp` +4,169, `alpha_save.cpp` +3,134, `alpha_runtime.cpp` +2,375, `frontend.cpp` +1,764), `.rodata` +3,152 (the messages and `EXPORT.TXT` text). Internal `.bss` +48 B; IRAM and `.data` unchanged; no new static buffers (the PC files are read into two heap vectors, 4.7 KB, released at once).
- The headroom is now 3 %; later batches should expect to budget flash.
- **Identity:** `PROJECT_VER` is `4.0.0-alpha4-save3-debug` (was `…-save2-debug`). Nothing is committed, so the embedded Git line still names `09cc4600` (as the A4-SAVE2 build does): the version string is what tells the two images apart. The image built here is `native/targets/tdeck/build-a4-save3/openu5_tdeck.bin`, SHA-256 `a4f58e3fe3cf40c1f8881fa74dce2ef920d277162f295e951d1fbb2e73a4ca0c`. After a commit, rebuild (`idf.py reconfigure`, then the build) so the image names its commit. *(Superseded by the final image below.)*

**The final image (the one to flash).** Built after every code, test and doc change, from scratch in a new directory: `idf.py --no-ccache -B build-a4-save3-final reconfigure`, then `ninja -C build-a4-save3-final -j 4 all` (1179/1179, first attempt clean, no project warnings), then `python package_launcher.py --build-dir build-a4-save3-final` (`native/core/a4-save3-final-{fw-configure,fw-build,package,fw-size-diff}.log`).
- **Identity change** (`main/CMakeLists.txt`): an image configured from uncommitted tracked changes now embeds `<hash>-dirty`, so it cannot pass for a build of HEAD. Untracked files (evidence logs) do not count; a clean committed tree embeds the bare hash as before. `tools/a3_04g_hw_closeout.py`'s `git=(\S+)` parser accepts the suffix.
- **Launcher file:** `native/targets/tdeck/build-a4-save3-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-save3-Debug-Launcher.bin`, byte-identical to `build-a4-save3-final/openu5_tdeck.bin`. That one file is all the Launcher needs (an application image with no bootloader or partition table merged in, as for every earlier batch).
- `FW 4.0.0-alpha4-save3-debug`, **`Git 09cc460052ee-dirty`** (HEAD `09cc4600` plus the uncommitted A4-SAVE2 + A4-SAVE3 tree).
- **1,012,480 B (`0xf7300`)**, SHA-256 `45d2863ac96cad54f6123e9cbb966d08c91bde2c911bb3ecc4f000e0ea6c80d8`. **36,096 B (3 %) of the 1 MiB app partition free.** Launcher allocation 1,048,576 B.
- The delta against A4-SAVE2 is +14,720 B: the earlier SAVE3 build's +14,704, plus 16 B of `.rodata` for the `-dirty` suffix. `.text` is +11,552 and internal `.bss` +48, both the same as the earlier build.

### 5.15 Manual PC/DOS validation (after the software checks)

Do not mark real PC compatibility PASS until part B's round trip succeeds in a real DOS Ultima V. No emulated-DOS run was possible in this batch (the DOSBox-X oracle `re/tools/oracle.py` is not in the tree and DOSBox-X is not installed on the build machine), so nothing here has yet been loaded by the original executable.

**Optional desktop pre-check (no T-Deck).** With a folder holding a DOS `SAVED.GAM` + `SAVED.OOL` (your own, or `original/u5/ultima5/`):

```
native/core/build-a4-save3/a4_save3_pc_bridge_runtime.exe native/assets/openu5-alpha1-resources.bin <that folder> --emit <out folder>
```

`<out folder>/roundtrip-SAVED.GAM` and `roundtrip-SAVED.OOL` are that save imported, changed (1234 gold, one step east, wind South, search item 20 found, a frigate docked at (58,107)), saved and exported through the same code as the device. Renamed to `SAVED.GAM`/`SAVED.OOL`, they can be tried in DOSBox or DOS before the hardware run. (The run also executes the whole test group; its checks assume the Lord British's Castle fixture, so with another save some checks may report RED while the files are still written.)

**0. Before anything**
- Power off the T-Deck. On a PC, copy the SD card's whole `ultima5/` folder somewhere safe (it holds `saves/`, `settings.json` and the resource packs). Restore it at the end if anything looks wrong.
- Back up the DOS Ultima V installation directory the same way (every `SAVED.*`, `BRIT.OOL`, `UNDER.OOL`).
- Flash the A4-SAVE3 image (§5.14, `OpenU5-TDeck-Alpha4.0.0-alpha4-save3-Debug-Launcher.bin`, SHA-256 `45d2863a…`). The boot screen must show `FW 4.0.0-alpha4-save3-debug` and `Git 09cc460052ee-dirty` (or, if the tree was committed and the image rebuilt, that commit's hash). **Stop if it does not.**

**A. Import a real PC save**
1. In DOS Ultima V, load (Journey Onward) a save you know, **outside any dungeon** — ideally one standing next to a docked ship, or one in a town. Before quitting, write down: location and coordinates (e.g. with a sextant), each party member's name, level, HP, and STR/DEX/INT, gold, food, keys, gems, a few inventory counts, the date and time, the wind direction, and any ship you own and where it is.
2. Quit & Save in DOS (`Q`, then `Y`). Copy `SAVED.GAM` and `SAVED.OOL` from the Ultima V directory to the SD card as `ultima5/import/SAVED.GAM` and `ultima5/import/SAVED.OOL` (create the `import` folder).
3. Boot the T-Deck. Main menu → **PC Save Transfer** (or press **P**). The subtitle must read **"PC save: <your Avatar>, <place>"**.
4. **Import the PC save into a slot** → pick an **empty** slot → Enter. Footer: "Imported into Slot N. The PC files are kept".
5. Check the SD card later: `ultima5/import/` still holds your two files, unchanged.
6. Main menu → Journey Onward → Load Game → the slot. Check everything you wrote down in step 1. In a town, the townsfolk are at their posts for the hour (they will not be exactly where DOS showed them); chests are back. A docked ship is where you left it; board it and check its hull.
7. Save normally (Alt+S, or System Menu → Save Game → the same slot).
8. Power-cycle. Load the slot again and check the same things.
9. PC Save Transfer again: the subtitle now ends **"(in Slot N)"**; Import → an empty slot asks **"Import it again?"** with **No, keep it** selected. Press Enter (No): nothing changes.

**B. Export to DOS**
1. Load the slot. Change something easy to recognise: spend or earn gold to an odd amount (e.g. buy one ration), walk to a remembered spot outdoors, and note the new values.
2. Save normally.
3. Main menu → PC Save Transfer → **Export a slot as a PC save** → the slot → Enter. Footer: "Slot N written to /ultima5/export/slotN".
4. On the PC: `ultima5/export/slotN/` holds `SAVED.GAM` (4192 bytes), `SAVED.OOL` (512 bytes) and `EXPORT.TXT`. Read `EXPORT.TXT`.
5. In the (backed-up) DOS Ultima V directory, replace `SAVED.GAM` and `SAVED.OOL` with the exported ones. Delete `BRIT.OOL` and `UNDER.OOL` if present (Journey Onward rewrites them from `SAVED.OOL`).
6. Launch DOS Ultima V → Journey Onward. Check the values from step 1, the party, the inventory, the date and the wind. If you exported in a town, the town is empty until you leave and re-enter it (L1): do that and check the townsfolk return.
7. Walk a few steps, then Quit & Save (`Q`, `Y`). The game must save and exit normally; start it again and Journey Onward once more.

**C. Re-import (optional, the gold standard)**
1. Copy the DOS-resaved `SAVED.GAM`/`SAVED.OOL` into `ultima5/import/` (replacing the earlier ones).
2. PC Save Transfer: the subtitle shows the new state (no "(in Slot N)": these are different files). Import into **another** slot.
3. Load it and check the same values as B.6.

**PASS** needs A, B and (if done) C with no crash, lock, watchdog reset or DOS error. **Report back:** the `FW`/`Git` lines, PASS/FAIL per step, anything in the DOS game that looked different from the Native state (with the `EXPORT.TXT` text), and, if you have a serial log, the `PC_IMPORT` and `PC_EXPORT` lines.

### 5.16 Status

| Axis | State |
|---|---|
| Investigation (§5.1–§5.5) | done |
| Software (focused, SAVE2/SAVE1 regressions, full suite, mutations, firmware build) | **complete** — 170/170, focused 57/57 + 10/10, SAVE2 65/65, SAVE1 45/45, mutations 30/30, firmware `0xf72f0`; final re-run 170/170, final image `0xf7300` (`Git 09cc460052ee-dirty`, SHA-256 `45d2863a…`) |
| Hardware / real DOS validation (§5.15) | **pending** |
| Closed (commit, image, PASS, closeout) | **no** — nothing committed or tagged, as for A4-SAVE2 |

*2026-09-30 (A4-UI3):* the real DOS round trip may be run on the A4-UI3 image (§6.13, checklist §6.14 B). UI3 does not touch the bridge, the import/export paths or serialization; only the PC pickers' rows changed.

*A4-CLOSE1 (2026-10-02): committed in `d6457ab1`; never run on a device. The device import / export is part 5 of the RC2 session (§14.7); the real-DOS round trip stays optional, and without it the release notes say "PC bridge host-validated only".*

## 6. A4-UI3 — save / load UX

A compact presentation pass over the A4-SAVE1/SAVE2/SAVE3 save system. **The save architecture is unchanged:** three player-visible logical slots, each backed by two hidden recovery generations; Continue's order and fallback (§4.4); manual Load never crosses to another slot; the PC Save Transfer bridge and its flow (§5); no new files, metadata, serialization or dirty-state tracking. The audit's own axis is `GAMEPLAY_INTEGRATION_AUDIT.md`, "Alpha 4 A4-UI3".

### 6.1 Baseline

`main` at `09cc4600` with A4-SAVE2 + A4-SAVE3 present and uncommitted. Host suite **170 / 170** (149.47 s, `native/core/build-a4-ui3`, `native/core/a4-ui3-baseline-{configure,build,ctest}.log`). Firmware: the A4-SAVE3 final image, `0xf7300` (1,012,480 B), 36,096 B (3 %) free. `PROJECT_VER` `4.0.0-alpha4-save3-debug`. The before screens are `native/core/a4-ui3-shots/before/*.png` (the A4-UI2 harness's `--dump`).

### 6.2 What was wrong (before)

1. A slot was one cramped line, `Slot 1: Avery, Iolo's Hut`, cut at 36 cells: a long name and place lost the place's end (`Slot 1: Shamino12, Lord British's Ca`).
2. Nothing marked the slot of the journey being played; only the cursor's start hinted at it.
3. A recovered slot (newest generation refused, the one before loads) looked exactly like a healthy one; only the selected row's footer said so.
4. `empty` / `damaged` sat in the name's position in lower case, like a name.
5. The confirm pages repeated the whole row (`Slot 1: Avery, …`) under a title that already named the slot.
6. After an in-game save the System Menu stayed open on "Saved in Slot N." — the brief wants the save to return to the game.
7. A load that fell back to the save before the newest said nothing.
8. A Load that failed in game (the slot damaged after the menu listed it) wrote "No valid save" to the transcript the open menu covers, and left the page listing the slot as loadable.

### 6.3 The slot rows (one formatter for every page)

`format_slot_rows()` / `list_slots()` (`native/core/src/frontend.cpp`) build every slot list: title Load Game, System Menu Save Game and Load Game, New Journey's replace list, and PC Save Transfer's import and export pickers. A slot is **two rows**, each at most 36 cells (what Large text fits in the menu's 304 px):

```
  Slot 1  Kojac                         <- leader (8 letters, the game's limit)
          Lord British's Castle         <- the place a load restores (grey)
  Slot 2  Avery     CURRENT, RECOVERED  <- the tag column
          Britannia
  Slot 3  EMPTY                         <- no metadata
```

- **Status words** in capitals so they never read as a name: `EMPTY`; `DAMAGED` (nothing in the slot loads, neither generation) with the detail row `Cannot be loaded`.
- **Tags** in one column after the 8-letter name field: `CURRENT` (in game) or `LATEST` (title), and `RECOVERED`; both → `CURRENT, RECOVERED` (36 cells with an 8-letter name).
- The name and place shown are those of **what a load restores** (for a recovered slot, the save before the newest), from the save list's existing summary (`summarize_candidate`, cached per commit). Date, time and party stay in the selected row's footer (A4-UI2 wording, unchanged).
- The core view keeps **one line per slot** (`lines[i]`, `selected_line` = the slot) and adds `FrontendView::details[i]`; only the device renderer (`tdeck_board.cpp` `show_frontend`) expands a slot page into two rows, draws the detail row in the footers' grey, and puts **both rows** of the selected slot in reverse video (the retained redraw restores both rows of the old selection). Every other page is drawn exactly as before.
- Not shown: level (not in the summary; it would need a new field); no timestamp beyond the in-game date the footer already shows.

### 6.4 Current journey and Continue

- **`CURRENT`** (System Menu Save and Load pages) marks `AlphaSaveService::last_slot()`: the slot the running journey was last loaded from or saved to — the slot Alt+S / Alt+L use and the pages start on. It means "this journey's slot", **not** "the card matches the running game"; Return to Title still says "Unsaved progress will be lost". Load Slot 2 → Slot 2 is CURRENT; save into Slot 1 → Slot 1 is CURRENT; a New Journey's slot is CURRENT. With no journey slot nothing is marked.
- **`LATEST`** (title Load Game, New Journey's replace list, PC import and export pickers) marks `continue_slot()`: the slot Continue restores. The title has no running journey, so it marks Continue's target instead.
- **Journey Onward** names slot and journey: subtitle `Latest: Slot 2, Iolo, Britannia` (was `Latest save: Iolo, Britannia`); footer unchanged, `Enter continues Slot 2; Mic returns`, or, when that slot is recovered, `Latest save damaged; Enter loads the one before` (the subtitle then names the save that loads). Continue is still one key with no question and no extra storage work. The main menu is unchanged.

### 6.5 Save, Load, feedback

| Flow | Behaviour |
|---|---|
| Save → empty slot | saves at once; **the menu closes**, transcript `Save complete: Slot N` |
| Save → occupied (or damaged) slot | `Overwrite Slot N?`, subtitle **that slot's save** (`Kojac, Lord British's Castle` / `Damaged save`), rows `No, keep it` (**selected**) / `Yes, overwrite`. No → back on the Save page on that slot, `Slot N kept`. Back or the Mic → back on the Save page, even with Yes highlighted. Yes → saves and closes. |
| Save fails | the menu stays open, re-lists the card and says what is true: `Save failed. Slot N keeps its last save` or `Save failed. Nothing was saved in Slot N` (transcript `Save failed; prior kept`, as before) |
| Load → healthy slot | loads, closes, `Load complete` (no question) |
| Load → recovered slot | loads the save before the newest: `Load complete`, then **`Recovered previous save (Slot N)`** (also after Continue and Alt+L when they fall back) |
| Load → empty slot | `Slot N is empty`, stays on the page, opens no file |
| Load → damaged slot | `Slot N is damaged and cannot load`, nothing loads |
| Load → slot that fails as it loads | nothing else is loaded in its place; the page re-lists the card (`DAMAGED`) and says `Slot N could not be loaded. Nothing changed` |

The recovery flag comes from the commit records the load already read (`restore_slot`'s `older`; for Continue also "it passed over the newest slot") — no extra I/O. Recovery stays one step. Unsaved-progress warnings are unchanged: the runtime has no dirty tracking and none was added.

### 6.6 PC Save Transfer

Unchanged in place (the title menu's eighth row, `P`), flow and semantics: Import / Export, `Replace Slot N?` and `Import it again?` with No first, the Mic cancelling, the import and export paths, the bridge. Only the import and export pickers use the shared rows (with `LATEST`), and the import question's subtitle names the slot's save instead of repeating its row.

### 6.7 Title menu

Not changed: the eight rows (nine with Developer) fit under the art; PC Save Transfer and Developer stay last. The main-menu golden (`a4_ui1_chrome_runtime` G1) is byte for byte.

### 6.8 Files

- Core: `native/core/include/openu5/frontend.h` (`format_slot_rows`, `format_slot_identity`, `list_slots`, the tags, `FrontendView::details`; `format_slot_row` removed), `native/core/src/frontend.cpp`, `native/core/src/system_menu.cpp`.
- Device: `main/tdeck_board.{h,cpp}` (two rows a slot, the selection span, the grey detail row), `main/alpha_runtime.{h,cpp}` (a save returns to the game, the failure notices, `announce_recovered_load`), `main/alpha_save.{h,cpp}` (`last_load_recovered()`), `CMakeLists.txt` (`PROJECT_VER`).
- Host stub: `host_tests/host_stubs/alpha_save_memory_host_stub.cpp` mirrors `last_load_recovered()`.
- Tests and tools: new `native/core/tests/a4_ui3_slot_rows_test.cpp`, `host_tests/a4_ui3_save_ux_runtime_test.cpp`, `native/core/tools/a4_ui3_mutation_check.py`; `native/core/CMakeLists.txt` registers both tests.

### 6.9 Existing tests changed on purpose

| Test | Why |
|---|---|
| `frontend_test.cpp` (load / damaged flows) | the rows' new text and `details`, the LATEST / RECOVERED tags, the Journey Onward subtitle and footer |
| `a4_ui2_save_menu_runtime` M3–M11, L1, L6, L7, L9, L11, F2, F3, Z1 | two-row text and tags; a save returns to the game (M4/M9 read the transcript, M10 checks the next open); L7 also checks the recovery line; Z1: a 9-letter test name keeps 8 letters and the place is no longer cut |
| `a4_save2_slots_runtime` O1, O4 | `Save complete: Slot N` with the menu closed, instead of the old footer |
| `a3_04g_storage_runtime` K10 and its `slot1_page` / `load_page` helpers | the row is two rows (CURRENT on Slot 1); the in-menu save closes the menu, so K10 reads the next open's Load page |
| `batch28_save_validation` F3b | the row prefix `Slot 1  ` with no DAMAGED / RECOVERED tag (stronger than before) |
| `a3_01_audio_runtime` H1 | its inline menu save no longer presses Alt+M afterwards, and each of its two runs starts from a blank card (the transcript now names the slot, which differed only because both runs shared one host card) |
| `menu_save()` helpers in `a3_02`, `a3_03`, `batch24`, `batch26`, `batch27`, `batch28`, `batch53`, `batch53a` | close the menu only if it is still open (Alt+M would re-open it); batch24's `menu_load` re-opens the menu |
| `tools/a4_save2_mutation_check.py` S23, S25 | re-anchored on the same code; S25 (a failed save's re-listing) is now killed by `a4_ui3_save_ux_runtime` S10 |

No golden hash changed.

### 6.10 New tests

**`a4_ui3_slot_rows`** (core, 40 checks): R the rows (three occupied, occupied + empty, an empty slot with a leftover summary, the longest name with both tags and the longest place, an over-long name, marker on one slot only, RECOVERED, DAMAGED, the confirm identity); S Save (CURRENT on the journey's slot and staying there when the cursor moves, an empty slot saves at once, the question names that slot and its save with No selected, No / Back / the Mic with Yes highlighted cancel, Yes saves that slot, a damaged slot asks too); L Load (rows and tags, empty refused, wraparound, the recovered footer, a recovered slot loads that slot, damaged refused, every Load intent is exactly the selected slot, the Mic returns); T the title (Journey Onward names Continue's slot and journey, Continue is one ContinueLatest, the recovered case, LATEST on the title Load page, wraparound, the Mic, a blank card); P PC Save Transfer (P reaches it, import list on the first empty slot, the occupied question on No, the Mic cancels, `Import it again?` on No, export on Continue's slot, an empty export refused).

**`a4_ui3_save_ux_runtime`** (device, 31 checks): the REAL `AlphaRuntime` on the REAL `tdeck_board.cpp` over the fake ST7789 (the A4-UI2 harness) with the REAL `alpha_save.cpp` over the fake SD card. P the panel (both rows of the selected slot in reverse video, the others plain; the place row grey; moving the selection restores both old rows; wraparound; Large text fits, ink right edge x = 311, rows end above y = 101). S saving (S1 three EMPTY on a blank card; S2 an empty slot saves and returns to the game, its four files alone; S4 leader and place per slot with CURRENT on the journey's slot; S5 `Overwrite Slot 1?` naming `Kojac, Iolo's Hut`, No selected; S6 No, the Mic and Back with Yes highlighted open no file and change no byte; S7 Yes saves Slot 1 and leaves Slot 2 byte for byte; S8 CURRENT follows the save; S9 a failed save's two notices; S10 the menu a failed save leaves open lists the card as it now is). L loading (L1 a healthy slot loads directly; L2 Load Slot 2 → Slot 2 alone CURRENT; L3 an empty slot refuses and opens no file; L4 the newest damaged → RECOVERED, showing the older save's leader; L5 Enter loads it and says `Recovered previous save (Slot 1)`; L6 both damaged → `DAMAGED`, nothing loads; L7 a slot damaged after the page opened loads nothing else, the page re-lists it). C the title (Journey Onward names Continue's slot; LATEST on the title Load page; Continue in one key; Continue over a recovered slot says so; New Journey's slot becomes CURRENT). X PC Save Transfer with the genuine DOS save (reachable by P; the import picker's rows; `Replace Slot 1?` on No, the Mic cancels, every byte kept; the export picker). W a warm Save page open reads the commit records alone (4 opens, 128 B).

Harness note: the host fixture has no creation canvas (`initialize()` allocates it on the device), so the creation quiz cannot be drawn on the host Board; C5 types its keys without drawing frames between them.

**RED evidence.** The rows, tags and `details` are new API, so the new tests cannot compile against the pre-UI3 tree; the house rule's other route applies — every guard validated by mutating production code (§6.11). The production change turned five existing tests RED before they were updated (`frontend`, `batch28_save_validation`, `a3_01_audio_runtime`, `a4_ui2_save_menu_runtime` 20/33, `a4_save2_slots_runtime` 63/65: the old text and the old stay-open save), each updated as listed in §6.9.

### 6.11 Mutations

`native/core/tools/a4_ui3_mutation_check.py <build> [ids]`: 25 mutants of production code, built and run against `a4_ui3_slot_rows`, `a4_ui3_save_ux_runtime` and (where marked) `a4_ui2_save_menu_runtime` (`native/core/a4-ui3-mutation.log`).

| Brief's class | Mutant | Killed by |
|---|---|---|
| wrong current-slot marker | U1 (CURRENT follows the cursor), U2 (LATEST always Slot 1) | S1b, L6; S1, L4 · T4; C2 |
| destructive prompt defaulting Yes | U3 | S3–S9 (core), S5–S9 (device) |
| empty Load accepted | U4 | L2; L3 |
| manual Load cross-falling | U5 (a failed slot load falls back to Continue) | L7 |
| recovered slot shown as healthy | U6 | R3, R6, L1, T4; L4 |
| wrong slot named in the overwrite prompt | U7 (title), U8 (subtitle) | S3, S9; S5 |
| Back / Mic confirming the destructive action | U9 (Back), U10 (the Mic with Yes highlighted) | S5–S7; S6, S7 |
| metadata from the wrong slot | U11 | R1–R7, S1, L1, T4, P1; S4, L2, L4, C2, X2, … |

The others: a damaged slot shown EMPTY (U12), an empty row with leftover metadata (U13), Journey Onward naming the wrong slot (U14), a save not returning to the game (U15), a recovered load not announced (U16), every load claiming recovery (U17), Continue never reporting a fallback (U18), the panel selecting one row (U19), the retained redraw forgetting the second row (U20), the place row drawn white (U21), an untruthful failure notice (U22), a failed load leaving the page stale (U23), the PC picker without LATEST (U24), a failed save leaving the open menu stale (U25).

The first pass had 24 mutants, all killed. Re-anchoring SAVE2's S25 showed its case (a failed save's re-listing) was no longer exercised by any test once a successful save closes the menu: S10 was added, U25 mirrors S25, and both are killed by it (`a4-ui3-mutation-u25.log`, `a4-ui3-save2-mutation-recheck.log`, which also re-runs SAVE2's S12–S16 and S23 against the UI3 tree: all killed).

**Final pass (the final test set): 25 killed, 0 survived, 0 invalid; restored build GREEN** (`native/core/a4-ui3-mutation.log`).

### 6.12 Host results

All runs serial, `native/core/build-a4-ui3` (the usual host toolchain), 0 project warnings.

| Run | Result | Log |
|---|---|---|
| Baseline (A4-SAVE3 tree) | **170 / 170**, 149.47 s | `a4-ui3-baseline-ctest.log` |
| After the production change, before the test updates | 165 / 170: the five tests of §6.10's RED evidence | (console; recorded here) |
| UI3 focused | `a4_ui3_slot_rows` **40 / 40**; `a4_ui3_save_ux_runtime` **31 / 31** | `a4-ui3-focused.log` |
| SAVE3 regression | `a4_save3_pc_bridge_runtime` **57 / 57**; `check-pc-save.ts` **10 / 10** | `a4-ui3-save3-regression.log` |
| SAVE2 regression | `a4_save2_slots_runtime` **65 / 65** | `a4-ui3-save2-regression.log` |
| SAVE1 regression | `a4_save1_recovery_runtime` **45 / 45**; `a3_04g_storage_runtime` **44 / 44** | `a4-ui3-save1-regression.log` |
| UI1 / UI2 regression | `a4_ui1_chrome_runtime` **30 / 30** (all eight goldens byte for byte); `a4_ui2_save_menu_runtime` **33 / 33**; `a4_ui2_title_runtime` **10 / 10**; `a4_ui2_death_music_runtime` **13 / 13** | `a4-ui3-ui1-ui2-regression.log` |
| Full suite | **172 / 172** (170 + `a4_ui3_slot_rows` + `a4_ui3_save_ux_runtime`), 153.64 s | `a4-ui3-ctest.log` |

**Performance.** No storage path changed: the menus still read the SAVE2 save list (`inspect_catalog`, cached per commit record). A warm Save page open reads the commit records alone (W1: 4 opens, 128 B on the test card); an empty-slot Load opens no file (L3); No / Back / the Mic on the question open no file (S6). Cold opens are SAVE2's (§4.6), unchanged. Two places now read the list where they did not before, both after a failure only: a failed save (as before UI3, but no longer after a success — one warm read fewer per save) and a failed Load.

### 6.13 Firmware

An interim build (`native/targets/tdeck/build-a4-ui3`, `native/core/a4-ui3-fw-{configure,build,size-diff}.log`) caught one firmware-only `-Werror=format-truncation` (the Journey Onward subtitle copied one 96-byte buffer into another; the identity is at most 34 characters, so it is now `%.40s`), then built clean.

**The final image (the one to flash).** Built after every code, test and doc change, from scratch in a new directory: `idf.py --no-ccache -B build-a4-ui3-final reconfigure`, then `ninja -C build-a4-ui3-final -j 4 all` (1179/1179, first attempt clean; no project warnings — the only warning line is ESP-IDF's "smallest app partition is nearly full (3 % free)" notice), then `python package_launcher.py --build-dir build-a4-ui3-final` (`native/core/a4-ui3-final-{idf-export,fw-configure,fw-build,package,fw-size-diff}.log`).

- **Launcher file:** `native/targets/tdeck/build-a4-ui3-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui3-Debug-Launcher.bin`, byte-identical to `build-a4-ui3-final/openu5_tdeck.bin`. That one file is all the Launcher needs (an application image; no bootloader or partition table merged in).
- **SHA-256** `33a7eb5974af30413d89a4c13e32fbf0cc5a571aeaffb323a85feb261e36642b`.
- Embedded (read back from the image's app descriptor and strings): **`FW 4.0.0-alpha4-ui3-debug`**, **`Git 09cc460052ee-dirty`** (HEAD `09cc4600` plus the uncommitted A4-SAVE2 + A4-SAVE3 + A4-UI3 tree; a clean committed tree would embed the bare hash).
- **1,014,544 B (`0xf7b10`)**: **+2,064 B** against A4-SAVE3's final `0xf7300` (1,012,480 B). **34,032 B (3.25 %) of the 1 MiB app partition free** (was 36,096 B).
- Flash `.text` +1,868, `.rodata` +192, internal `.bss` +16 (the Board cache's selection span, padded); IRAM, `.data` and PSRAM unchanged. Per symbol: the renderer's two-row expansion in `show_frontend` ~+0.9 KB, `format_slot_rows` / `format_slot_identity` / `list_slots` +586 B (the old `format_slot_row` −182 B), `service_system_menu_intent` +181 B, `restore_slot` +105 B net, `announce_recovered_load` +52 B; no new assets, strings tables or pages.

### 6.14 Hardware validation checklist (A4-UI3 image; also finishes A4-SAVE3's DOS round trip)

**0. Before anything**
- Power off. On a PC copy the SD card's whole `ultima5/` folder somewhere safe (`saves/`, `settings.json`, packs). Back up the DOS Ultima V directory the same way (every `SAVED.*`, `BRIT.OOL`, `UNDER.OOL`).
- Flash `OpenU5-TDeck-Alpha4.0.0-alpha4-ui3-Debug-Launcher.bin` (§6.13). The boot screen must show `FW 4.0.0-alpha4-ui3-debug` and the Git line of §6.13. **Stop if it does not.**

**A. UI3 save / load UX**
1. **Boot.** Title, then the main menu: eight rows (nine with Developer), PC Save Transfer below Settings; no clipping.
2. **Continue target.** Journey Onward: the subtitle reads `Latest: Slot N, <leader>, <place>` and the footer `Enter continues Slot N; Mic returns`. Note N.
3. **Title Load Game.** Down → Load Game: every slot is two rows (`Slot N  <leader>` over the place in grey); slot N carries `LATEST`; the cursor starts on it; empty slots read `EMPTY` with no second-row text. Up/Down wrap. Enter on an EMPTY slot: footer `Slot N is empty`, nothing happens. Mic → back to Journey Onward on Load Game.
4. **Load each slot** from the title (Load Game → slot → Enter): the game opens at that slot's place with that leader; no question.
5. **Current-slot marker.** In game, Alt+M → Load Game: the slot just loaded carries `CURRENT` (and only it). Mic.
6. **Save Game, empty slot.** Alt+M → Save Game → an EMPTY slot → Enter: the menu closes at once and the log says `Save complete: Slot N`. Alt+M → Save Game: that slot now shows your leader and place and is `CURRENT`.
7. **Overwrite, No default.** Save Game → an occupied slot → Enter: `Overwrite Slot N?`, the subtitle names **that** slot's leader and place, `No, keep it` highlighted. Enter → back on Save Game, footer `Slot N kept`.
8. **Back / Mic cancel.** Enter again, Down to `Yes, overwrite`, then press the **Mic**: back on Save Game, nothing saved. Again with **Backspace/Back**: the same. Alt+M on the question closes the menu without saving.
9. **Overwrite Yes.** Enter, Down, Enter: the menu closes, `Save complete: Slot N`.
10. **Load each slot in game** (Alt+M → Load Game → slot → Enter): `Load complete`; CURRENT moves to that slot.
11. **Recovered slot (if practical).** With a PC and a backup: in `ultima5/saves/`, damage the newest generation of a slot that has two (e.g. truncate its newest `.gam` — the generation whose `.commit` is newest; Slot 1 is `alpha1-g0/g1`, Slot 2 `alpha1-s2-g0/g1`, Slot 3 `alpha1-s3-g0/g1`). Boot: Load Game shows that slot tagged `RECOVERED` with the leader/place of the save before; footer `Last save damaged; Enter loads the one before`. Enter: `Load complete`, then `Recovered previous save (Slot N)`. Restore the backup afterwards.
12. **Unusable slot (if practical).** Damage both generations of one slot: it reads `DAMAGED` / `Cannot be loaded` (never EMPTY); Enter: `Slot N is damaged and cannot load`, nothing loads, no other slot loads. Restore the backup.
13. **Text size.** Settings → Text / UI: Large. Save Game and Load Game: both rows of every slot fit, nothing clipped at the right edge, the selection covers both rows. Back to Medium.
14. **PC Save Transfer layout.** Title → PC Save Transfer (P): Import and Export pickers show the same two-row slots with `LATEST`; Import → occupied slot → `Replace Slot N?` with No highlighted; Mic → `Import cancelled. Slot N is unchanged`.
15. **Feel.** Scrolling the slot pages: no flicker beyond the rows that change, no lag, no stale highlight on the row you left.

**B. SAVE3 real DOS round trip** (§5.15 in full; summary)
1. In DOS Ultima V load a known save **outside any dungeon**; write down location, party (names, levels, HP, STR/DEX/INT), gold, food, keys, gems, some inventory, date/time, wind, ships. Quit & Save (`Q`, `Y`).
2. Copy `SAVED.GAM` + `SAVED.OOL` to the SD card as `ultima5/import/`.
3. T-Deck: PC Save Transfer → subtitle `PC save: <Avatar>, <place>` → Import → an **EMPTY** slot → `Imported into Slot N. The PC files are kept`.
4. Journey Onward → Load Game → that slot: verify everything from step 1 (townsfolk at their posts for the hour, not exactly where DOS showed them).
5. Save in Native (Alt+S, or Save Game → that slot → Yes). Power-cycle. Load it again and verify again.
6. Change something recognisable (odd gold amount, a remembered spot outdoors); save.
7. PC Save Transfer → Export → the slot → `Slot N written to /ultima5/export/slotN`.
8. Copy `export/slotN/SAVED.GAM` + `SAVED.OOL` into the DOS directory (delete `BRIT.OOL`/`UNDER.OOL` if present).
9. DOS Ultima V → Journey Onward: verify the changed state from step 6.
10. Walk a few steps, Quit & Save in DOS; start DOS again and Journey Onward: it loads.
11. Optional: copy the DOS-resaved files back to `ultima5/import/` and import into another slot; verify.

**Do not mark SAVE3 real PC compatibility PASS until step B.9–B.10 succeed in real DOS.** Report: the `FW`/`Git` lines, PASS/FAIL per step, photos of any clipped or odd screen, and (if you have serial) the `SAVE_CATALOG`, `PC_IMPORT`, `PC_EXPORT` lines.

### 6.15 Known issues left out of scope

- Underworld place caption: a Native save in the underworld is listed `Britannia` (`hud_location_caption()` tests `floor < 0`; Native keeps the underworld as floor 255). The same function captions the in-game HUD, so the fix is not isolated to the save UI; already queued as A4-SAVE3 audit item 7. Not changed.
- Town NPC round trips, dungeon PC transfer refusal, parked-vehicle codec gaps, worn crown / drunk timer persistence, the missing Native underworld skiff, broader serialization gaps (A4-SAVE3 §5.9–§5.10): not touched.
- A torn file under an **unchanged** commit record keeps the runtime's save-list memory (SAVE2 / A3-04G design: the list trusts a commit it verified); the slot is then listed healthy until a load falls back, which now says `Recovered previous save`. Unchanged.

### 6.16 Status

| Axis | State |
|---|---|
| Software (focused, SAVE3/SAVE2/SAVE1/UI1/UI2 regressions, full suite, mutations, firmware build) | **complete** — 172/172, UI3 40/40 + 31/31, SAVE3 57/57 + 10/10, SAVE2 65/65, SAVE1 45/45 + 44/44, UI1 30/30, UI2 33/33 + 10/10 + 13/13, mutations 25/25; final image `0xf7b10` (`Git 09cc460052ee-dirty`, SHA-256 `33a7eb59…`) |
| Hardware (§6.14 A) | **pending** |
| A4-SAVE3 real DOS round trip (§6.14 B, §5.15) | **pending** |
| Closed (commit, tag, PASS) | **no** — nothing committed or tagged, as for A4-SAVE2 / A4-SAVE3 |

*A4-CLOSE1 (2026-10-02): committed in `d6457ab1`; still no hardware result of its own (the slot rows were seen during the END1 and UI4 sessions, but never recorded, §8.21). Part 4 of the RC2 session (§14.7).*

## 7. A4-END1 — the full ending

The original PC/DOS ending, played on the device: the throne room, Lord British's walk, the revivals, the box questions, the speech, the orb and the moongate, the dissolve, the six story pages, the scroll — or the stranded room — and the frozen last screen. Built on the hardware-validated, uncommitted A4-SAVE1/SAVE2/SAVE3/UI3 tree; nothing of the save system changed. The reconstruction with every address is `re/notes/a4-end1-ending-reconstruction.md`; the audit's own axis is `GAMEPLAY_INTEGRATION_AUDIT.md`, "Alpha 4 A4-END1".

### 7.1 Baseline

`main` at `09cc4600` with A4-SAVE2 + A4-SAVE3 + A4-UI3 present and uncommitted. Host suite **172 / 172** (146.01 s, `native/core/build-a4-end1`, `native/core/a4-end1-baseline-{configure,build,ctest}.log`). Firmware: the A4-UI3 final image `0xf7b10` (1,014,544 B), 34,032 B free. `PROJECT_VER` `4.0.0-alpha4-ui3-debug`. Before A4-END1 the device ending was Batch 53's one-shot transcript (the TypeScript reference's rescue narration) followed by Batch 53A's terminal Ending — ledger D-54 (no presenter), D-56 (the box questions answered from the inventory), D-57 (two `victory` lines).

### 7.2 Investigation first

The investigation report (trigger, control flow, pages, text sources, visuals, input, timing, audio, final behaviour, native gaps, the plan) was delivered before any production code; it found **no reverse-engineering blocker**. Evidence: the shipped binaries read directly (`re/tools/a4_end1_endgame_listing.py` prints all of ENDGAME.OVL with calls resolved — its output quotes EA code and text, so it stays local), cross-checked with the earlier notes (`endgame.md`, `endgame-derivation.md`, `batch53a-endgame-terminal.md`) and the two video witnesses. **No DOSBox or runtime oracle was available**, so no direct runtime parity is claimed: the evidence is static, and §7.16 is the manual DOS comparison checklist.

### 7.3 The original ending, in brief

- **Trigger.** SJOG `absorb` (0x1ea4) arms `[0x58a0] = 0x4d`; the combat teardown (SJOG 0x2046 / DUNGEON 0x00cb) calls the overlay-13 stub ULTIMA.EXE 0x7c4a → ENDGAME.OVL `endgame_main` 0x0648, which never returns.
- **Throne room.** MISCMAPS.DAT[0x210], recoloured green in the tileset itself (EGA.DRV fn36 ax=4, 22 tiles); Lord British (slot 31) at (5,8) for 40 frames, then five steps to (5,3); each roster member in turn — revived first if dead (`<name> lives!`, the viewport XOR, the 1,550 ms sweep, then `G` at full HP) — appears in the mirror (5,9) and walks to the lineup {5,4,6,3,5,7} × {5,6,6,7,7,7}.
- **Text.** ENDMSG.DAT's 11 records in record order through `print_string`, with the DATA.OVL strings (`" lives!"`, `"!\""`, `"Yes"`, `"No"`, `"He says:"`, `"\"I see..."`) between them.
- **Input.** Ten `getkey_with_redraw` sites, any key each; two real Y/N loops (0x0852, 0x088b) that read again on anything but Y/N; victory needs **Y and the box** (0x08b9).
- **Victory.** The Avatar steps forward and back, the box at (5,4), ENDMSG 0x03, `He says:`, five speech records (a key each), `FOLLOW!` (0x09) with the orb, the orb's sweep, the red moongate rising in 16 stages, Lord British then each member into it, the gate closing, the floor blitted back; `story_screens` fizzles the screen to black (EGA.DRV fn34) and shows **six story pages** (a key each); then ENDSC.16 with `endgame_datestamp`'s proclamation, runes and report — and the 1988 loop 0x04f9 for ever (the Exodus patch: any key exits to DOS).
- **Stranded.** `"I see..."`, 40 frames, ENDMSG 0x0a, member 2 to the table, Lord British to his bed, the Avatar to the other chair, then the wander loop 0x0ac9 for ever.
- **Timing.** `run_n_frames(n)` = n × 55 ms; a step 296 ms (2 frames, the 21 ms footstep, 3 frames); sweeps 1,550 / 1,937 ms; the fizzle has no timer (≈ 2.5 s on the witness).
- **Audio.** Stock 1988: speaker only — the footsteps and the two sweeps; **no music**. The Exodus patch's `mid.drv`: 0x15 Joyous Reunion (then Rule Britannia by itself), 0x18 per story page (Stones for 1–3, Dream of Lady Nan for 4–6), 0x1b Rule Britannia (waits for a Reunion still playing).

### 7.4 Native implementation

| Layer | What | Where |
|---|---|---|
| Core sequencer | `openu5::EndgameScene`: `endgame_main` as explicit states (`Pc`), each ending in a wait the device satisfies — Frames, Hold (a sweep, a footstep), Key, YesNo, Dissolve, Forever; a sink for console text, sound cues and music selectors; `compose()` builds the throne room as a `PresentationSnapshot` (slots 31..0 through the pose selector, the reflection, the partial gate, `endgame_recolor`); the 40 × 25 scroll grid (`endgame_datestamp`, number words, playtime); `EndgameFizzle` (fn34's LFSR). No device code, no allocation; the revive writes the live roster as 0x075a does | `native/core/{include/openu5,src}/endgame_scene.{h,cpp}` |
| Trigger | `QuestWorldServices::endgame_presenter`: on the device the absorption sets game-won (`rescue_lord_british`) without the reference's narration; parity drivers leave it off (quest_parity unchanged) | `quest_world.{h,cpp}` |
| D-57 | the GameWon / Endgame events' internal token is no longer printed | `ui_session.cpp` |
| Device presenter | `start_endgame` on GameWon; `service_endgame` every frame: the scene clock (≤ 250 ms a frame; it stands under the System Menu and the Developer screen and resumes where it stopped), timed waits from exact due times, the fizzle's pace, the A-15 line once the stranded wander begins; the throne room is a presentation source that outranks the others (`endgame-scene`); the story pages and the scroll replace the game screen; keys go to the scene's getkeys only; music selectors and sound cues; `stop_endgame` on Load, Return to Title and a Developer un-win | `main/alpha_runtime.{h,cpp}` |
| Board | `show_endgame_page` (a 4bpp 320 × 200 page at y 20 on black, with the scroll's opaque 8 × 8 cells — IBM.CH / RUNES.CH, reverse video — over it; drawn once per page); the dissolve's capture (every pixel of one full repaint mirrored into a PSRAM copy as it is sent), `fizzle_endgame` (the next pixels of fn34's order blacked out, the bands with them), the copy released after | `main/tdeck_board.{h,cpp}` |
| Renderer | fn36's LUT applied to the listed tiles before any animation pass; the partial gate cell | `main/native_renderer.cpp` |
| Pack | `endgame-room.bin` (121 B) and `endgame-pages.bin` (the six story pages and the scroll's backdrop, 7 × 32,000 B 4bpp + 16 B header), composed at pack time from the user's own END.DAT / ENDTEXT.16 / END1–3.16 / ENDSC.16 / FONT.OVL tables by a port of FONT.OVL's justified renderer; checked by `check-endgame-pages.ts` | `native/tools/u5pack/alpha1-endgame.ts`, `alpha1.ts`; `main/alpha_resources.{h,cpp}` |
| Audio | `EndgameOrb` (param 1 the revive sweep, 0 the orb's); music through the existing contexts (Reunion, EndgameStones, EndgameLadyNan, Finale); the 0x15 chain timed from the song's parsed length (`AudioBackend::music_length_ms`, the device's from the XMI's last tick) | `sfx_synth.cpp`, `sfx_inventory.cpp`, `audio.h`, `main/tdeck_audio.h` |

**SD resource pack:** 2,042,554 → **2,266,819 B** (+224,265 B: the two entries and their table rows), CRC `0x5c0d175d`, SHA-256 `85b38994eea674e48338d744091c6b895b9507a286bb38bcc84231131feed01e`. **The firmware's identity lock requires this pack** (A-13): it must be copied to the SD card with the image. The audio pack is unchanged.

Found and fixed on the way (each pinned by a test): a Developer un-win (Preset: Endgame) left the scene running and eating input (Batch 53A TI3); a dissolve with no frame to capture waited for ever (now skipped, logged, like a capture without memory); after a long pause at a getkey the following walk collapsed to nothing, because the next wait was timed from when the getkey began (W4); closing the System Menu let the scene catch up 250 ms (W5).

### 7.5 Adaptations (documented; no invented art)

1. The 320 × 200 pages sit at y 20 of the 240-row panel on black bands; the fizzle covers the picture (fn34 over 320 × 200, exactly the original's order) and the bands as their own 320 × 40 rectangle in step with it.
2. The seven full-screen pages are composed once at pack time (flash is the hard constraint); the scroll's text is drawn live (date, name, playtime).
3. The fizzle's start frame is the Board's own full repaint mirrored into PSRAM (153,600 B, only during the dissolve). Without the memory, or without a frame, the fizzle is skipped and logged.
4. The fizzle's rate is 25,600 px/s (the witness's ≈ 2.5 s): the original has no timer there.
5. The getkeys wear the device cue (`Enter: continue`, `Y / N`) on the status line, as every paced scene does since Batch 51 / A3-HF5.
6. The scene clock stands under the System Menu and the Developer screen; one frame moves it at most 250 ms.
7. The last screen stays (1988's freeze); the Exodus patch's "any key → DOS" has no device meaning (A-15): the System Menu's Load and Return to Title leave.

### 7.6 Input

| Where | Key | Effect |
|---|---|---|
| a getkey (the status line says `Enter: continue` in the throne room; each story page, which shows no cue, as in 1988) | any key: letter, digit, Space, Enter, **Mic** (ESC), **Backspace** | ends that getkey only — the ending goes on; Mic and Backspace never abort it |
| a box question (`Y / N`) | Y / N, either case | the answer; every other key is read again (0x0852 / 0x088b) |
| no getkey open (walks, sweeps, the gate, the dissolve) | any key | nothing — dropped, never kept for the next getkey (no BIOS type-ahead) |
| anywhere in the ending | the trackball's roll | nothing (not a key on the T-Deck); Shift+roll pages the console in the throne room, as in every paced scene |
| anywhere | a held key | one key: the matrix sends one press; the release is no key |
| the scroll / the stranded room | any key | nothing |
| anywhere | Alt+S | `Save unavailable` (Batch 53A) |
| anywhere | Alt+M | the System Menu (the scene's clock stands): Load leaves, Return to Title leaves, Settings |
| anywhere | Alt+D | the Developer screen (the clock stands); a Developer un-win stops the scene |

### 7.7 Timing (the scene clock; exact due times, observed within one 5 ms frame)

| Beat | Duration |
|---|---|
| before Lord British walks; before the greeting; after the box; after `I see...` | 40 frames = 2,200 ms each |
| one step (anyone) | 110 + 21 + 165 = 296 ms; the footstep at 110 ms |
| a revival (XOR held) | 1,550 ms, then the healed roster |
| the orb's sweep | 1,937 ms |
| the gate | 1 frame per stage 1–15, the whole gate 4 frames, 1 frame per stage closing |
| the fizzle | 64,000 px at 25,600 px/s = 2,500 ms |
| a getkey, a page, the scroll | for ever |

The ending's timing is the same with music, muted, and with no audio pack at all (M12, M13).

### 7.8 Audio

- **Stock assets (no music patch): no music is played**, none is invented. The sounds are the original's speaker cues: a footstep per step, the revive sweep per revival, the orb's sweep.
- **With the supported Exodus patch's songs in the audio pack:** Joyous Reunion from the absorption; Rule Britannia follows by itself when it ends (even while a getkey waits); Stones for story pages 1–3, Dream of Lady Nan for 4–6; Rule Britannia on the scroll — and on the stranded wander, after a Reunion still playing has ended.
- The existing controls hold: Alt+Shift+M / Alt+Shift+S silence the music / the sounds for the session (unmuting on the scroll starts its song), Music Volume / SFX Volume as set.

### 7.9 Files

- Core: new `include/openu5/endgame_scene.h`, `src/endgame_scene.cpp` (`sources.cmake`); `presentation.h` (`endgame_recolor`, the gate fields); `quest_world.{h,cpp}` (`endgame_presenter`); `ui_session.cpp` (D-57); `audio.h` (`music_length_ms`); `sfx_synth.cpp`, `sfx_inventory.cpp` (`EndgameOrb`).
- Device: `main/alpha_runtime.{h,cpp}`, `main/tdeck_board.{h,cpp}`, `main/native_renderer.cpp`, `main/alpha_resources.{h,cpp}`, `main/tdeck_audio.h`, `main/CMakeLists.txt` (`endgame_scene.cpp` built `-Os`, §7.14), `CMakeLists.txt` (`PROJECT_VER`).
- Pack tools: new `native/tools/u5pack/alpha1-endgame.ts`, `check-endgame-pages.ts`; `alpha1.ts` (the two entries).
- Host fixture and stubs: `host_tests/alpha_runtime_host_fixture.cpp` (the scene and the pack's two entries before `bind_quest_services`), `host_stubs/tdeck_board_host_stub.cpp`, `host_stubs/batch37_board_capture_stub.cpp` (the Board's four new calls), `host_tests/a4_ui2_harness.h` (opt-in dungeon arenas, null by default).
- Tests and tools: new `native/core/tests/a4_end1_endgame_scene_test.cpp`, `host_tests/a4_end1_ending_runtime_test.cpp`, `native/core/tools/a4_end1_mutation_check.py`, `re/tools/a4_end1_endgame_listing.py`; `native/core/CMakeLists.txt` registers the three tests.
- Notes: new `re/notes/a4-end1-ending-reconstruction.md`.

### 7.10 Existing tests changed on purpose

| Test | Why |
|---|---|
| `batch53_release_blockers` EV5, EV6, ES3, V0b | they read the one-shot transcript; they now play the ending through its getkeys (`drive_ending`) — EV6 reads the report off the scroll, and the console must not repeat it. EV8 (no input wedge, paging) is unchanged and passes again once the ending has been played (the console then holds enough to page) |
| `batch53a_ending_terminal` R5, TV0/TS0 (and the TV/TS checks after them) | the same: R5 reads the report off the scroll; the terminal checks run on the ending's last screen |
| `a3_02_sfx_synth` E12 | 63 supported cues, not 62: `EndgameOrb` has its program |
| `a3_03_sfx_inventory` I1 | `EndgameOrb` left the silent set (ten ids, not eleven) and is Implemented |
| `a3_04e_pacing` S2 | five draw loops pause through the policy's cadence, not four (`show_endgame_page`) |

No golden hash changed (the eight A4-UI1 goldens are byte for byte).

### 7.11 New tests

**`a4_end1_endgame_scene`** (core, 50 checks, over the user's ENDMSG.DAT / MISCMAPS.DAT): S0 the room; E the entry, Lord British's 40 frames and walk, a step's frames / footstep / frames, the mirror, the revive (print, sweep, hold, roster), the lineup and the tie rule of `move_sprite_toward`, the greeting; Q the box questions (Y, N then Y/N, other keys read again, case folding); V the victory branch (box, speech order, orb, the 16 gate stages, into the gate, the floor blitted back, the dissolve, six pages with selector page + 1, the scroll); S the datestamp (rows, centring, reverse, runes, ordinals / cardinals, the 13 × 28 playtime and its borrows); N the stranded branch (`I see...`, the walks, the bed pose, the wander, Yes without the box); C the composition (pose selector, reflection, partial gate, recolor flag and LUT); F the fizzle (fn34's order, (0,0) last, every pixel once).

**`a4_end1_ending_runtime`** (device, 56 checks): the REAL `AlphaRuntime` on the REAL `tdeck_board.cpp` over the fake ST7789, from the Phase 7E-A route (Developer Preset: Endgame, Doom L6 (4,7), the pit, four steps north) with a party of four, three of them fallen. T the trigger (the scene mounts; no ENDMSG at the absorption; no internal token); V the room on the viewport (MISCMAPS from the pack, the recolor on exactly the listed tiles — LUT and list held independently by the test —, the viewport on the panel, the XOR during a revival, the gate at stage 5); W the waits (40 frames, 296 ms steps with keys pressed between them, the 1,550 ms holds, two minutes at a getkey, the step cadence after a 30 s pause at the box question, the System Menu stopping the clock); K the keys (no type-ahead, the cue, a held key, Y/N only, Mic and Backspace as keys, Alt+S refused); E the console's whole text in order, from the pack, once each; D the dissolve (capture == panel, 25,601 pixels after 1 s exactly in fn34's order with the bands, the last at 2,500 ms, the copy released); P the six pages and the scroll pixel for pixel, each drawn once; M the music (Reunion → Rule Britannia by itself, Stones / Lady Nan, Rule Britannia on the scroll, waiting for the Reunion in the stranded room) and the sounds (21 footsteps, the revive sweeps with param 1, the orb's with param 0), the mutes, the same clock muted and with no audio pack; F the scroll for ten minutes, keys doing nothing, the System Menu over it, Load leaving; S the stranded room (No, No; the walks; the A-15 line once; the wander; Return to Title) and Yes without the box; N a pack without the ending keeps the parity path.

**`a4_end1_endgame_pages`** (TypeScript, 11 checks): L1 the six pages' layout from DATA.OVL; J1–J2 the justification; C1–C3 the clip and the coverage (every printable byte drawn once, in order; C3 lays a text three pages long on page 0's layout, whose leading puts a line at exactly y 0xc0, and no glyph of it is drawn); O1–O2 the order of drawing; G1 the seven screens' goldens; G2 the entries' sizes; G3 the shipped pack holds exactly this build.

**RED evidence.** The scene and the presenter are new API, so the new tests cannot run against the pre-END1 tree. Mutant **E0** is the pre-END1 device in one edit (the presenter off): it turns the new runtime test RED (T1, T3, S1, S2, S4, M8–M13, F5, F6) and the updated Batch 53 / 53A checks RED (EV6, R5). The production change turned five existing tests RED before they were updated (§7.10).

### 7.12 Mutations

`native/core/tools/a4_end1_mutation_check.py <build> [ids | --anchors]`: **55 mutants** of production code — the scene, the runtime presenter, the Board, the renderer, `ui_session.cpp`, `quest_world.cpp` and the TypeScript composer — each built and run against `a4_end1_endgame_scene` (scene), `a4_end1_ending_runtime` (runtime), `batch53_release_blockers` (b53), `batch53a_ending_terminal` (b53a) and `check-endgame-pages.ts` (pages).

| Brief's class | Mutants | Killed by |
|---|---|---|
| the pre-END1 device / the trigger | E0 presenter off (Batch 53's dump); E1 presenter **and** narration | runtime T1, T3, S1, S2, S4, M8–M13, F5, F6; b53 EV6; b53a R5 · runtime T3; b53 EV5, EV6 |
| text: order, once, nothing invented | E2 the D-57 token back; E3 the speech from ENDMSG 0x05; E4 the A-15 line over the overlay | runtime T3, F0, S3 · scene V3–V9, runtime E2 · runtime F0, S3 |
| the throne room | V1 no recolor; V2 a wrong LUT entry; V3 the renderer ignores it; V4 no XOR; V5 the gate from the bottom of 0xdc; V6 Lord British a row up; V7 the lineup; V8 the tie rule; V9 the bed pose; V10 14 gate stages | runtime V1, V4, V7, D1 (V1–V3; V2 also scene C4) · runtime V4 · runtime V7 · scene E2–E5, runtime V3, W1, … · scene E10, E13 · scene E13 · scene N3 · scene V6–V14 |
| timing | W1 32 frames; W2 the roster healed at the print; W3 waits timed from the getkey's start; W4 the clock runs under the System Menu | scene E2, E3, runtime W1 · runtime W2 · runtime W4 · runtime W5 |
| one held key never skips several screens; no type-ahead | K1 the trackball ends a getkey; K2 a key hurries a timed wait | runtime W3 · runtime W1, W2, M1, M2, M12, M13 |
| the box questions | K3 any key answers; K4 no case folding; K7 Yes without the box wins | scene Q2, Q3, runtime K3, … · scene Q3, Q4, S2, runtime …, b53 EV5, EV6, ES3, V0b · scene N5, N6, runtime S4 |
| Mic / Back must not abort; no saving mid-ending | K5 the Mic aborts; K6 Alt+S saves | runtime K3, K4, … · runtime K5, F6; b53a TV6–TV8, TV12, TS6–TS8, TS12 |
| the dissolve | D1 always skipped; D2 the capture not a full repaint; D3 32,000 px/s; D4 a wrong tap; D5 the bands never fizzle; D6 the copy never released | runtime D1–D3 · D1, D2 · D2, D3 · scene F1, runtime D2, D3, P1, … · runtime D2 · runtime D3 |
| the pages and the scroll | P1 the picture at y 0; P2 transparent cells; P3 redrawn every frame; P4 the centring; P5 "Twentyieth"; P6 a 30-day borrow | runtime D2, P1, P3, F2, F3 · P3, F2, F3 · P2, F1 · scene S1, S2 · scene S2 · scene S2, S3 |
| music: only the patch's, at its selectors | M1 no Reunion → Rule Britannia chain; M2 0x1b cuts the Reunion; M3 the story music by page, not page + 1 | runtime M4, M9 · runtime M8, M9 · scene V12, runtime M6 |
| sounds | M4 no footsteps; M5 the revive plays the orb's sweep; M6 no orb sweep | scene E4, runtime M1, M3 · runtime M2 · scene V5, runtime M5 |
| no silent return to play; ways out | F1 a key on the scroll ends it; F2 Load leaves the overlay running; F3 Return to Title leaves it running; F4 a Developer un-win keeps it; F5 no wander | scene V14, runtime F2, F3 · runtime F4 · runtime F7 · b53a TI3 · scene N6, runtime F5 |
| the TypeScript composer | T1 rounding instead of idiv; T2 an 8-row leading; T3 the art before the headlines; T4 the clip at y 200; T5 no soft hyphen; T6 the scroll at (0,0) | pages G1, G3 · C3, G1, G3 · O1, G1, G3 · C3 · C2, G1, G3 · O2, G1, G3 |

**The first pass** (`a4-end1-mutation-pass1.log`, on the test set before the last guards): 50 killed, 5 survived — E2 (the D-57 check read the Message channel only), E4 (the A-15 count started after the absorption's own input), V8 (no test walked a tie), F2 (Load's own stop was masked by the un-win stop unless the loaded save is won), T4 (the real pages never reach y 0xc0). Each got a guard: T3 / F0 / S3 read every channel for the token and count from before the absorption; core E13 walks members 1 and 3 through their ties; runtime F4 loads a won save on the scroll; pages C3 lays an over-long text on page 0's layout, whose leading puts a line at exactly y 0xc0. The five re-run killed (`a4-end1-mutation-survivors-recheck.log`; T4 needed page 0's layout — page 1's leading skips the window). A second guard was also made independent: the runtime test held the production recolor LUT as its oracle, so V2 was killed by the core test alone; it now holds its own copy.

**Final pass (the final test set): 55 killed, 0 survived, 0 invalid; restored build GREEN** (`native/core/a4-end1-mutation.log`).

### 7.13 Host results

All runs serial, `native/core/build-a4-end1` (the usual host toolchain), 0 project warnings (`a4-end1-build.log`).

| Run | Result | Log |
|---|---|---|
| Baseline (A4-UI3 tree) | **172 / 172**, 146.01 s | `a4-end1-baseline-ctest.log` |
| After the production change, before the test updates | 168 / 173: the five tests of §7.10 (`batch53_release_blockers` EV5, EV6, EV8, ES3, V0b; `batch53a_ending_terminal` R5, TV0, TV5, TS0, TS5 and TI3 — TI3 was the un-win defect of §7.4; `a3_02_sfx_synth` E12; `a3_03_sfx_inventory` I1; `a3_04e_pacing` S2) | (console; recorded here) |
| END1 focused | `a4_end1_endgame_scene` **50 / 50**; `a4_end1_ending_runtime` **56 / 56**; `check-endgame-pages.ts` **11 / 11** | `a4-end1-focused.log` |
| SAVE3 regression | `a4_save3_pc_bridge_runtime` **57 / 57**; `check-pc-save.ts` **10 / 10** | `a4-end1-save3-regression.log` |
| SAVE2 regression | `a4_save2_slots_runtime` **65 / 65** | `a4-end1-save2-regression.log` |
| SAVE1 regression | `a4_save1_recovery_runtime` **45 / 45**; `a3_04g_storage_runtime` **44 / 44** | `a4-end1-save1-regression.log` |
| UI1 / UI2 / UI3 regression | `a4_ui1_chrome_runtime` **30 / 30** (all eight goldens byte for byte); `a4_ui2_title_runtime` **10 / 10**; `a4_ui2_death_music_runtime` **13 / 13**; `a4_ui2_save_menu_runtime` **33 / 33**; `a4_ui3_slot_rows` **40 / 40**; `a4_ui3_save_ux_runtime` **31 / 31** | `a4-end1-ui-regression.log` |
| Full suite | **175 / 175** (172 + `a4_end1_endgame_scene` + `a4_end1_ending_runtime` + `a4_end1_endgame_pages`), 151.10 s | `a4-end1-ctest.log` |

**Rendered screens.** No golden changed. The new pixel checks compare the panel with what the pack and the fonts say it must be (P1, P3, D1, D2, V1, V2, V4, V7), not with recorded hashes; the seven screens' own goldens (G1) were recorded after the screens were looked at (`check-endgame-pages.ts --png`), and the runtime test's `--dump <dir>` writes the throne room, the revive's XOR, the gate, the captured frame, the fizzle half way, each page, the scroll and the stranded room for a human look.

### 7.14 Firmware

**The investigation (flash is the hard constraint).** An interim build (`native/targets/tdeck/build-a4-end1`, `native/core/a4-end1-fw-{configure,build}.log`) built clean on the first attempt at **`0xfade0` (1,027,552 B), +13,008 B** over A4-UI3 — 21,024 B free, more than "a few KB", so it was taken apart by object and symbol (`esp_idf_size --files` / `--archive-details libmain.a`, against `build-a4-ui3-final`):

| Where | Growth | What |
|---|---|---|
| `endgame_scene.cpp` (new) | +5,992 B | the state machine `advance` (1.6 KB), `compose` (0.7 KB), the scroll (0.5 KB), the original's number words and scroll lines (1.1 KB of strings), put_char / wander / step / fizzle |
| `alpha_runtime.cpp` | +3,344 B | `service_endgame` (0.9 KB), the page / capture / presentation-source paths in `render` (0.85 KB), the getkey routing, start / stop, the music selectors, the sound cues |
| `tdeck_board.cpp` | +1,131 B | `show_endgame_page`, `fizzle_endgame`, the capture mirror |
| merged strings (`stdio_vfs.c.obj`) | +1,152 B | the `ENDGAME_*` serial lines the hardware checklist reads, the A-15 line |
| `alpha_resources.cpp`, `native_renderer.cpp`, `sfx_synth.cpp`, `main.cpp` | +372, +320, +241, +278 B | the two pack entries' load and checks; the recolor and the partial gate; the orb's sweep program; the runtime object's new members |

No accidental bloat (no new library, no table, no duplicated asset); the 224 KB of screens were already moved to the SD pack, which is what lets the feature fit at all. One reduction was applied: the sequencer is cold code (once a playthrough; per frame only a 121-cell compose and, during the dissolve, the fizzle's LFSR), so `endgame_scene.cpp` alone is built `-Os` (`main/CMakeLists.txt`; code generation only, the host tests run the same source): **−856 B**. The rest is the feature.

**The final image (the one to flash).** Built after every code, test and doc change, from scratch in a new directory: `idf.py --no-ccache -B build-a4-end1-final reconfigure`, then `ninja -C build-a4-end1-final -j 4 all` (1180/1180, first attempt clean; no project warnings — the only warning lines are ESP-IDF's "smallest app partition is nearly full (2 % free)" notice and its five `component_validation.cmake` notices), then `python package_launcher.py --build-dir build-a4-end1-final` (`native/core/a4-end1-final-{idf-export,fw-configure,fw-build,package,identity,fw-size-diff}.log`).

- **Launcher file:** `native/targets/tdeck/build-a4-end1-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-end1-Debug-Launcher.bin`, byte-identical to `build-a4-end1-final/openu5_tdeck.bin` — an application image, no bootloader or partition table merged in.
- **SHA-256** `4a167f960cf17f762e0209c35513976955e8693e6308f19628ca921707cfa1e1`.
- Embedded (read back from the image's app descriptor and strings, `a4-end1-final-identity.log`): **`FW 4.0.0-alpha4-end1-debug`**, **`Git 09cc460052ee-dirty`** (HEAD `09cc4600` plus the uncommitted A4-SAVE2 + A4-SAVE3 + A4-UI3 + A4-END1 tree), ESP-IDF v6.1.
- **1,026,688 B (`0xfaa80`)**: **+12,144 B** against A4-UI3's final `0xf7b10` (1,014,544 B). **21,888 B (2.09 %) of the 1 MiB app partition free** (was 34,032 B).
- Flash `.text` +10,372, `.rodata` +1,752; internal `.bss` +160 (the runtime object's ending state: the scene pointer, ENDMSG pointers, two fizzles, the scene clock), `.data` +32; IRAM unchanged. PSRAM: +224,137 B resident (the pack's two entries, loaded at boot with the pack), the scene object (~2.3 KB), and **153,600 B only during the dissolve** (the panel copy, freed after; without it the fizzle is skipped).

### 7.15 Hardware validation checklist (A4-END1 image) — run 2026-10-01: PASS (§7.19); B.2 revival not physically tested

**0. Before anything**
- Power off. On a PC back up the SD card's whole `ultima5/` folder (`saves/`, `settings.json`, both packs). Keep the old `openu5-alpha1-resources.bin`: the A4-UI3 firmware needs it if you go back.
- Copy the **new resource pack** `native/assets/openu5-alpha1-resources.bin` (2,266,819 B, SHA-256 `85b38994eea674e48338d744091c6b895b9507a286bb38bcc84231131feed01e`) to `/ultima5/openu5-alpha1-resources.bin`. The audio pack stays as it is.
- Flash `OpenU5-TDeck-Alpha4.0.0-alpha4-end1-Debug-Launcher.bin` (§7.14; SHA-256 `4a167f96…a1e1`). The boot screen must show **`FW 4.0.0-alpha4-end1-debug`** and **`Git 09cc460052ee-dirty`**. **Stop if it does not.** A pack refusal at boot means the pack copy is not the new one.

**A. Reaching the ending (the Phase 7E-A route)**
1. Continue (or New Journey). Alt+D → Developer → Shortcuts → **Preset: Endgame**, then Party → **Party size 1** for the quickest run (as in 7E-A). For a second run, Party size 4: the throne room then shows the lineup of four (in the arena every member makes the walk). A revival needs a companion already dead in the party (e.g. a save where one fell): the Developer has no kill shortcut.
2. Alt+S once **before** the pit, so a pre-ending save exists (Load later returns to it).
3. Developer → Teleport → Doom, Floor Level 6, X 4, Y 7, default entrance Off. Face East, step forward: the final room. Walk forward four times: `Avatar is absorbed!` (then the arena's `VICTORY!`, ledger D-72).

**B. Victory, page by page** (answer **Y**)
1. The viewport becomes the **green throne room** (the floor, the chairs and bed, the torches recoloured; the torches still flicker). Lord British stands at the bottom centre ~2 s, then walks up five cells, a footstep each. With the patch's music: **Joyous Reunion** starts at once.
2. Each companion appears in the mirror at the bottom and walks to the lineup in front of Lord British; a **dead** one first: `<name> lives!`, the viewport flashes inverted for ~1.5 s with a rising sweep, the roster shows the member healed.
3. ~2 s later: the greeting with the Avatar's name; status line `Enter: continue`. Wait a minute: nothing moves on; then any key.
4. The box question; status line `Y / N`. Press Space, Enter, other letters, Mic, Backspace: nothing. (Optional: N → the second question; Y there.) **Y**: `Yes`; the Avatar steps forward and back; the box appears; the box text.
5. `He says:` then five pages of Lord British's speech, one key each (try the **Mic** on one: it turns the page, it does not leave the ending), then `"FOLLOW!" cries Lord British`, the orb appears. Key: the orb's sweep, the **red moongate** rises cell-high, Lord British and then each member walk into it and vanish, it sinks, the floor returns.
6. The whole screen **dissolves to black** in ~2.5 s, random pixels, including the bands above and below where the pages will sit.
7. **Six story pages**, each waiting for a key: The Homecoming (the stones), the house, the night, The Dream, the choice, the gate. Music: Stones for the first three, Dream of Lady Nan for the last three. The text is justified in its columns, nothing clipped at the bottom, the art over the headline's start on page 1.
8. The **scroll**: `Be it known that on` … the date in words, `<Avatar> the Avatar`, `saved the life …`, the two rune lines, then the report rows below the parchment with the playtime. Music: Rule Britannia.
9. Wait several minutes: nothing changes. Keys and the trackball: nothing. Alt+S: `Save unavailable` (the log is under the scroll; check after leaving).
10. Alt+M: the System Menu over the scroll; Alt+M again: the scroll comes back intact.
11. Alt+L (or System Menu → Load Game): the pre-ending save loads; the game plays again.

**C. Input feel**
1. At any getkey, hold a key a few seconds: only one page turns.
2. While Lord British or a member walks, press keys: nothing hurries; the next getkey still waits for a fresh key.
3. Roll the trackball at a getkey: nothing.
4. During the gate rising, Alt+M: the scene pauses; close the menu: it continues from the same point, no jump.

**D. Stranded** (from the pre-ending save)
1. Same route; at the box question answer **N**, then **N** again: `No` twice, `"I see...`, then `Well then, pull up a chair…` (ENDMSG 0x0a).
2. A companion walks to the table, Lord British to his bed (the sleeping pose), the Avatar to the other chair; the others wander about the floor for ever, never onto anyone.
3. The status line / log shows `The quest is complete. Alt+M: System Menu` once.
4. Keys do nothing; Alt+M → **Return to Title**: the title screen; Continue loads the last save (pre-ending).
5. Optional: Y **without** the box (drop it with the Developer first): `Yes`, then `"I see...` — the stranded room.

**E. Audio**
1. With the patch's songs (Music Volume above 0 %): Reunion → Rule Britannia by itself if you wait long enough at a getkey; the page music and the scroll's as in B.7–B.8.
2. Alt+Shift+M during the ending: the music stops and stays off through pages and scroll; Alt+Shift+M on the scroll: Rule Britannia starts. Alt+Shift+S: footsteps and sweeps silent.
3. SFX Volume and Music Volume settings are obeyed.
4. Without the music patch (or with music unavailable): **no music anywhere in the ending**; the footsteps and the two sweeps still sound.

**F. Final state and power**
1. Power-cycle on the scroll: Continue loads the last save, never the ending (the ending is never saved).
2. Report: the `FW` / `Git` lines, PASS/FAIL per step, photos of any odd screen (especially the pages and the scroll), and (with serial) the `ENDGAME_*` lines.

### 7.16 Manual DOS comparison checklist (optional; no runtime oracle was available)

In DOSBox (or real DOS) with the same install (`original/u5/ultima5`, the Exodus patch applied) and a save in or near the final room with the wooden box (a DOS save of your own; the PC Save Transfer bridge refuses dungeon saves, A4-SAVE3):
1. **Order and text.** Compare the console text line by line with the T-Deck's: the revivals, the greeting, the two questions, the box text, `He says:`, the five speech pages, FOLLOW!; and on the stranded route `I see...` and the chair text.
2. **Prompts.** At the box question DOS ignores every key but Y / N (either case); the T-Deck must match.
3. **Timing.** Lord British's ~2.2 s wait and ~0.3 s steps (at DOSBox's default cycles the 18.2 Hz tick holds; the speaker sweeps depend on the machine); the dissolve's length depends on DOSBox's speed (the T-Deck uses ~2.5 s).
4. **The room.** The green recolour of floor, chairs, bed and torches; the mirror reflection; the gate rising from the bottom of its cell.
5. **Pages.** The six pages' art, headlines and justified text line breaks; the scroll's rows, centring and runes.
6. **Music** (patched install only): Reunion, Rule Britannia after it, Stones / Lady Nan per page, Rule Britannia at the scroll.
7. **End.** The patched DOS build exits to DOS on a key at the scroll (the T-Deck stays; A-15); the stranded room wanders for ever in both.

### 7.17 Known issues left out of scope

- `VICTORY!` (the native arena's line when the absorption ends combat) precedes the ending; the original goes from `absorb` straight to the overlay. Pinned by Batch 53A R3; a combat change — ledger **D-72**, queued.
- The TypeScript reference's rescue narration (`rescue_events`, pinned by quest_parity) is not ENDMSG's text or order; the device no longer uses it — ledger **D-73**, queued as a reference fix (fix the reference at the pinned layer, then regenerate).
- *(A4-UI4/PRES1, §8.5 and §8.8: D-72 fixed — the arena's "ended" line is no longer printed and Doom's cell latches silently; D-73's narration half fixed in the reference and the parity path. The reference's auto-answered box questions stay queued.)*
- Developer has no per-member "kill" shortcut, so the hardware route shows a revival only with an already-fallen companion.
- The real Joyous Reunion length comes from the parsed XMI on the device; the host checks the chain with a declared length.

### 7.18 Status

| Axis | State |
|---|---|
| Investigation (before code) | **delivered; no RE blocker**; static evidence only (no DOSBox) — `re/notes/a4-end1-ending-reconstruction.md` |
| Software (focused, SAVE3/SAVE2/SAVE1/UI1/UI2/UI3 regressions, full suite, mutations, firmware build) | **PASS** — 175/175; END1 50/50 + 56/56 + 11/11; SAVE3 57/57 + 10/10, SAVE2 65/65, SAVE1 45/45 + 44/44, UI1 30/30 (goldens unchanged), UI2 10/10 + 13/13 + 33/33, UI3 40/40 + 31/31; mutations 55/55; final image `0xfaa80` (`FW 4.0.0-alpha4-end1-debug`, `Git 09cc460052ee-dirty`, SHA-256 `4a167f96…a1e1`), 21,888 B free |
| SD resource pack | recopied for the hardware run (2,266,819 B, SHA-256 `85b38994…d01e`); audio pack unchanged |
| Hardware (§7.15) | **PASS** (§7.19), with one case **not physically exercised**: the dead-member revival |
| Revival of a dead companion | **automated PASS** (core E7–E9, runtime W2 / V4 / M2 / E1, mutants W2 / M5 / V4 killed); **NOT PHYSICALLY TESTED** on the T-Deck — untested, not failed |
| DOS comparison (§7.16) | optional; not run |
| Queued from END1 | D-72 (`VICTORY!` before the ending), D-73 (the reference's rescue narration) — out of scope, still queued |
| Closed | **yes** — commit and annotated tag `alpha4-end1-hardware-validated` (§7.19) |

### 7.19 Hardware result and closeout (2026-10-01)

The user ran §7.15 on the T-Deck with the final image of §7.14 — `OpenU5-TDeck-Alpha4.0.0-alpha4-end1-Debug-Launcher.bin`, `FW 4.0.0-alpha4-end1-debug`, `Git 09cc460052ee-dirty`, SHA-256 `4a167f960cf17f762e0209c35513976955e8693e6308f19628ca921707cfa1e1`, 1,026,688 B — over the new resource pack (2,266,819 B, SHA-256 `85b38994eea674e48338d744091c6b895b9507a286bb38bcc84231131feed01e`).

**Result: PASS.**

| Checklist area | Hardware |
|---|---|
| The victory route (A, B): trigger, throne-room choreography (Lord British's walk, the lineup), the box / orb / moongate sequence | **PASS** |
| The box questions: Y / N interaction | **PASS** |
| The dissolve | **PASS** |
| All six story pages | **PASS** |
| The final scroll | **PASS** |
| Music and SFX behaviour (E) | **PASS** |
| The stranded branch (D) | **PASS** |
| System Menu behaviour; Save refused during and after the ending | **PASS** |
| Load and Return to Title tear the ending down | **PASS** |
| Input and pacing (C) | **PASS** (correct on hardware) |
| **A dead companion's revival** (B.2: `<name> lives!`, the XOR, the sweep, the healed roster) | **NOT PHYSICALLY TESTED** — no dead companion was at hand (the Developer menu has no kill shortcut, §7.17). Untested on hardware, **not failed**. Its evidence is automated only: core E7–E9 (the print, the 1,550 ms hold, `G` at full HP), runtime W2 (three revivals, roster healed after the hold), V4 (the XOR on the composed viewport), M2 (the revive sweep, param 1), E1 (the `lives!` lines in order); mutants W2, M5 and V4 killed. |

**Closeout runs** on the committed tree (no production or test logic changed since §7.13, so no mutation pass was repeated): END1 focused 50/50, 56/56, 11/11 and the serial full suite (`native/core/a4-end1-closeout-{focused,ctest}.log`). The post-commit build (`native/core/a4-end1-postcommit-{configure,build,package}.log`, directory `build-a4-end1-postcommit`) checks that the committed tree builds to the same image apart from its embedded Git line; **the tested image remains the §7.14 one** (`Git 09cc460052ee-dirty`), and that is the one the tag records. Results: closeout focused **50 / 50, 56 / 56, 11 / 11**; serial full suite **175 / 175** (143.90 s). The post-commit image (`Git d6457ab10589`, SHA-256 `5855fd5171767f73f5e08ac950996fdd86511edde26ff8afa38fdfeb4a1c7aa2`, 1,026,688 B) has every section the same size as the tested one; its bytes differ only by the embedded Git line, whose different length shifts the layout. It was not flashed and is not the validated image.

D-72 and D-73 stay queued, out of A4-END1's scope.

## 8. A4-UI4/PRES1 — frontend and presentation finalization

One batch, two halves. **UI4** fixes the frontend and the gameplay screen where they were inconsistent or broken. **PRES1** restores small original-game presentation behaviour, derived from the binaries and still missing or approximate on the device. This includes D-72 and D-73, the two items END1 left queued.

No save format, gameplay rule, resource pack or audio pack changed. The A4-END1 cinematic is untouched apart from one thing: D-72 removes the stray `VICTORY!` that preceded it.

Two subjective changes were mocked up and **not** implemented, pending the user's choice (§8.10).

### 8.1 Baseline

- `main` at `e53741b2`, the annotated tag `alpha4-end1-hardware-validated`. The tree was clean.
- Host build `native/core/build-a4-ui4`. Serial suite **175 / 175** (156.73 s; `native/core/a4-ui4-baseline-{configure,build,ctest}.log`). The only warning is the long-standing `stl_uninitialized.h` false positive.
- Firmware baseline: END1's tested image, `0xfaa80` (1,026,688 B), with **21,888 B free**.

### 8.2 Investigation (before any production change)

The candidate list was built from these sources:
- the Alpha 3 deferred list (`ALPHA3.md` §2);
- the Alpha 2 handoff (`ALPHA2.md`);
- the ledger's open rows;
- the audit's Alpha 4 sections;
- ALPHA4_UI §2–§7 and their "left out of scope" lists;
- the frontend document (`ALPHA20_FRONTEND_NEW_GAME.md`);
- a host screen survey: the real runtime and Board, every frontend page, the gameplay screen at all three text sizes, pickers, the Developer screen.

The reference port's skin was read for the console conventions; the binary was read wherever the rule was in question. Two findings came from the survey, not from any document:
- **Small text was garbled.**
- **A lost battle printed `BATTLE IS LOST!` three times.**

| # | Candidate | Source | Disposition |
|---|---|---|---|
| F1 | The main-menu footer names `J C T U A R`; S and P are also hotkeys | survey | **fixed** (§8.3) |
| F2 | Boot splash "Alpha 2.0", diagnostics "Alpha 2.0 Debug", identity "OpenU5 HARDWARE-TRUTH … Starting diagnostic runtime" | survey | **fixed** (§8.3) |
| F3 | Small text drops each glyph's last column and bottom row ("Load" → "Lcac") | survey | **fixed** (§8.3; mockup shown) |
| F4 | "Mic saves" (title) vs "Mic returns" (System Menu) on the same Settings rows; "Developer / Debug" vs "Developer" | survey | **fixed** (§8.3) |
| — | D-5 Acknowledgements | ledger | **obsolete**: the original STARTSC.16 panel has shown since Alpha 2.0; the curtain reveal is queued |
| H1 | D-53's `Direction?` half (H-15): a world getdir showed `Aim: empty (-1,-1)` | ledger, audit | **fixed** (§8.4) |
| H2 | The underworld captioned "Britannia" (HUD, save rows, PC import line) | A4-SAVE3 item 7, A4-UI3 §6.15 | **fixed** (§8.4) |
| H3 | D-12: the zodiac view clipped by the strips | ledger | **fixed** (§8.4) |
| H4 | D-52: `^` drawn as `?` | ledger | **fixed** (§8.4) |
| H5 | In-game pickers and shops select with a green `>`; every menu since UI1 uses reverse video, as do the original's pickers (0x2a28) | survey | **subjective: mockup, awaiting a decision** (§8.10) |
| H6 | The HUD's `HH:MM` clock is not the original's | web skin F-F | **kept** (A4-UI1's choice); recorded as E-7 |
| P1 | D-72 `VICTORY!` before the ending; **new**: `BATTLE IS LOST!` ×3 / `VICTORY!` ×2 (D-74) | ledger, survey | **fixed** (§8.5) |
| P2 | Console echoes against DATA.OVL: `Look-` then `north` on its own row; `Cast`, `Ready`, `Z-stats`, `Klimb` printed twice, `Mix`, the arena's `Get`/`Open`/`Search` | survey, reference `CMD_STRINGS` | **fixed** (D-75, §8.5); the rest of the echo family is queued |
| P3 | D-48: the moongate transit, and the gates' rise and fall | ledger | **fixed** (§8.7) |
| P4 | D-67: the shard ritual's bursts (60/60 ms, silent) | ledger | **fixed** (§8.6) |
| P5 | D-69: one 2 px drop for a whole shake over a still view | ledger | **fixed** (§8.6) |
| P6 | D-68: the Refuge stage's content | ledger | **fixed** (§8.6; device presentation only) |
| P7 | D-71: Mix's echo and its 10-tick wait | ledger | **fixed** (§8.5); the picker legend and the spell list are declared (A-17) |
| P8 | The console's echo bullet `►`, a blank row before each command, bottom anchoring, the flame-wave wait cursor (IBM.CH 0x05–0x08) | reference `skin/fiel` (`consoleLinesToRows`, `CONSOLE_CURSOR_WAVE`), `getkey-cursor-derivacion.md` | **subjective adaptation to the 5×7 transcript: mockup, awaiting a decision** (§8.10) |
| D-73 | The reference's rescue narration | ledger | **narration half fixed** (§8.8); the reference's auto-answered box questions (D-56's reference half) stay queued |
| — | D-8, D-13 (with the echo family), D-14, D-15, D-16, D-38's NPC pose, D-10/D-39 residuals, D-58/D-59, D-50/D-51, D-60, D-1/D-2/D-4/D-7/D-9, D-17/D-18; the intro's six story plates, the credits curtain, the title's walking figures | ledger, `ALPHA20_FRONTEND_NEW_GAME.md` | **queued**, each for its reason (§8.11) |

### 8.3 Frontend (UI4)

- **F1. Hotkey footer.**
  - The main menu takes `JCTUARSPD` (`FrontendSession::handle`), but its footer named `J C T U A R`.
  - It now reads `Select: arrows / Enter / J C T U A R S P`, plus ` D` when the Developer row is shown.
  - The UI1 `main-menu` golden is re-recorded for this footer alone; the other seven are byte for byte.
- **F2. Boot screens name the image's release.**
  - `firmware_release_label()` (`device_ui_views.h`, static strings, no buffer) maps `PROJECT_VER`'s major to "Alpha N". `main.cpp` hands it to `Board::set_release_label()` before `initialize_display()`.
  - The splash shows "Alpha 4". The diagnostics screen shows the same label.
  - The identity screen's title is "OpenU5-TDeck" and its last line "Starting".
  - The `FW` / `Git` / `RES` / `ASSET` lines every hardware check reads are unchanged.
- **F3. Small text.**
  - The 5×7 face drawn into Small's 4×6 glyph sampled column `col*5/4` and row `row*7/6`, so it never read column 4 or row 6: "o" became "c", "n" became "r".
  - Small now merges the middle column pair (2,3) and the middle row pair (3,4) by OR, and keeps every stroke.
  - Same metrics and cell, no font table (`draw_text_box_metrics`). Medium (1:1) and Large (up-scaled) are unchanged.
  - Mockup: `C:\dev\ui4-scratch\mock-small-text.png` (not in the repo).
- **F4. One wording.**
  - Both Settings pages say `Left/right changes; Mic saves`; Mic leaves either page and persists it.
  - The System Menu's developer row is "Developer", as on the title menu.
- **Unchanged:** the title, main menu, Journey Onward, Load/Save, PC Save Transfer, confirmation and error pages. Their layouts were reviewed and found consistent with UI1–UI3.

### 8.4 The gameplay screen (UI4)

- **H1 (D-53 / H-15).**
  - A getdir reads only a direction (kernel 0x35EC). This covers Look-, Talk-, Attack-, Open-, Get-, Search-, Jimmy-, Push- and Klimb- in the world, and Get-/Open-/Search- in the arena.
  - Its status line now reads `Direction?`. The aim readout stays the arena Attack's own.
- **H2.**
  - `hud_location_caption()` tested `floor < 0`, but Native keeps the outdoor underworld as floor 255. It now treats both 255 and −1 as the underworld.
  - This fixes the HUD caption, the save rows' place and the PC import line together, since they share the function.
- **H3 (D-12).** The spyglass's zodiac view is drawn full-square, as the gem view is (R-17 / Y-14).
- **H4 (D-52).** `^` is a glyph of the 5×7 face. Z-stats marks the readied item with it.
- **E-7 (recorded, unchanged).** The status box's `HH:MM` clock. It is not the original's, but it gives the handheld player a clock.

### 8.5 Console feedback (PRES1)

- **D-72 and D-74: the arena's "ended" event is never printed.**
  - `combat.cpp`'s `end()` emits a `CombatEventKind::Ended` line, "VICTORY!" or "BATTLE IS LOST!", once or twice per end. `UiSession::append_combat_event` printed it.
  - So a defeat showed `BATTLE IS LOST!` three times, the third being `finish_encounter_combat`'s own line, and a won battle printed `VICTORY!` again as the party walked out.
  - The original prints the defeat once (COMBAT 0x0cda) and leaves silently with the victory flag (0x0cd3). The reference's `combatOut` prints only messages, hits and echoes.
  - Doom's cell has no enemy at entry, so its latch is silent (COMBAT 0x0bb2–0x0bc0). The absorption therefore reaches the ending with no `VICTORY!` (D-72).
  - The core event is unchanged, so combat parity is unaffected.
- **D-75: the dispatcher's own echoes** (DATA.OVL, the kernel table DS 0xa134–0xa28c; the arena's copies in COMBAT.OVL).
  - **Direction word.** getdir 0x35EC prints the direction word on the command's own row: "Look-North" (DS 0xa2a6 "North", 0xa2ae "South", 0xa2bc "East", 0xa2b6 "West"). This applies to Look, Open, Get, Search, Jimmy, Push, Klimb and Attack, and to the arena's Get-/Open-/Search-.
  - **Talk and Fire are left alone.** They are not shown to reach 0x35EC and keep their old row.
  - **Corrected strings:**
    - `Cast...` (0xa142);
    - `Ready...` plus a blank row (0xa1f0 `"Ready...\n\n"`);
    - `Use item` plus a blank row (0xa24c);
    - `Z-stats...` (0xa28c);
    - `Klimb-` (0xa1a0) once, where its direction question used to echo the token "klimb" as a second row;
    - the arena's `Get-` (DS 0x6e14), `Open-` (0x6e22) and `Search-` (0x6e3a).
- **D-71 (part).**
  - `M` echoes `Mix Reagents` plus a blank row (DS 0xa1b4).
  - "Done!", or the trap, follows "Mixing..." after 10 ticks: CMDS 0x1b88–0x1b9c runs `delay(10)` or `run_n_frames(10)`, 550 ms. Keys are swallowed, as in every timed beat (A3-HF7's `DialoguePacer` Effect, keyed on the core's own "Mixing..." literal).
- **Declared (A-17).** The spell is picked from a list, and the picker's legend is on its context bar instead of the three console lines.

### 8.6 Scene beats (PRES1)

- **D-67: the shard ritual** (CAST 0x16e1–0x16fa, seven calls of `explosion_fx_at_cell` 0x3522; read again this batch).
  - Each burst is tile 0 held for `noise_burst(0x7d0, 0xbb8, 0xa)`: 174 ms, `kBlackthornBurstSamples`, A3-HF8's burst. It then plays the CombatHit program as it lands, followed by `viewport_redraw`.
  - The redraw has no timer, so the device holds the cleared cell for one tick (the render-bound floor of Batch 51).
  - Before: 60 ms on, 60 ms off, silent, and only 4 of the 7 bursts visible.
  - `WorldFxLayer::take_burst_starts()` tells the runtime when each burst begins.
- **D-69: the quake.** An animation-only frame redraws only the animated cells. Each change of the quake offset (a pulse down, its rest) is now a full frame, so a shake shows all its pulses: 16 changes per shake.
- **D-68: the Refuge stage** (BLCKTHRN `party_refuge`; read again this batch).
  - After the darkness line, 0x098f–0x09ca clear every object, so the stage is empty.
  - 0x09f5–0x0a07 put slot 0 (0x1c) at (5,5) right after "But thy slumber is disturbed!" (and before its sweeps). The pacer marks that beat (`places_avatar`: the beat with the six slumber sweeps), and `refuge_scene_figures(…, avatar_placed)` draws the Avatar from then on.
  - The last stage (0x0bc4–0x0be7) blacks the window and blits 0x11c at (5,5): the Avatar alone.
  - Device presentation only: the scene names that `quest_parity` pins are unchanged. `[0x587c]` = 0x1e during the first `delay(10)` is still not modelled.

### 8.7 The moongates (D-48)

**The original** (ULTIMA.EXE and EGA.DRV; read this batch):

- **0x475a, `kernel_moongate_render`.**
  - At night (hour ≥ 20 or < 5, 0x4767–0x4773) each compositor pass raises the cosmetic counter `[0x5887]` (0x3ef0, capped at 16).
  - By day it sinks (0x3f36), and at 0 the cell is grass again (0x4798).
  - It writes the gate tile (0xdc) **into the map buffer** on every buried stone's cell. 0xdc is a light source, so a standing gate lights its surroundings.
- **0x56e6 → 0x1112 → EGA.DRV fn 0x60 (0x24d6).** A gate cell at stage 1..15 is tile slot 0x116, composed as follows:
  - **grass** (tile 5) — or the floor 0x44 when `[0x5893]` == 0xff, i.e. the ending;
  - with its bottom *n* rows replaced by the **top** *n* rows of 0xdc.
- **0x266c → 0x5910.** The idle getkey redraws outdoors every tick, so a rise is 16 ticks (~880 ms).
- **0x48a8, `kernel_moongate_enter`** (the party on a gate):
  1. `run_n_frames(1)`.
  2. The activation sweep (0x48e5, tone_sweep count 0x7530: 1,162 ms at the calibrated floor).
  3. `fx_tile_fizzle_in(0xdc)` over the party.
  4. `run_n_frames(1)`.
  5. The gate closes over the cell, 0x1112(15 → 1) with `delay(2)` each: 15 × 2 ticks = **1,648 ms**.
  6. Grass.
  7. At 00:00–00:09 no teleport, otherwise the teleport (0x47f4). The closing leaves `[0x5887]` = 0, so the destination's gate rises from 0.

**The device:**
- The runtime keeps the counter: one stage per 55 ms tick on the surface.
- `compose_world_presentation(…, moongate_stage)` places a gate while the counter is above 0, sets the frame's partial stage, and makes the sampler see a standing gate as 0xdc for the light pass, so the gate glows.
- The renderer composes every 0xdc cell at 1..15 as grass plus the gate's top rows. The ending's partial gate is the same code with its floor 0x44.
- The core's own `moongate` cue is emitted on the origin before the teleport, and it starts the transit presenter:
  - the origin stays on screen;
  - its centre is the party, then the gate, then the closing stages over grass;
  - the destination appears after ~2,980 ms;
  - no key acts until the transit's clock has run, as in 0x48a8.

**Declared:**
- The counter is not saved (SAVED.GAM +0x2E1). A load settles it: 16 at night, 0 by day.
- The fizzle is held one tick, the render-bound floor.
- The sweep's hold is the calibrated floor.

### 8.8 D-73 — the reference's rescue narration

- **The change.** On the absorption path (the only one the game takes), `rescueLordBritish` (`game/src/core/quest/lordbritish.ts`) and native `rescue_events` (the parity drivers' path) now print nothing. ENDGAME.OVL prints ENDMSG in its own order, and the reference's script (`buildEndgameScript`) carries it.
- **Removed text** (the port's own narration): the connector lines "Bearing amulet, crown and sceptre…" and "Lord British rises…", plus the console copies of ENDMSG and of the scroll.
- **Kept.** The direct callers' gated path (tests and debug) is unchanged. The device never used either path; its presenter plays the ending.
- **Proof** (fix the reference at the pinned layer):
  - With the reference changed alone, `quest_parity` failed at the first absorption case (`Quest mismatch 4809`).
  - With both changed it passes, together with `quest_case_table_drift`.
  - A token diff of the native driver over all 5,377 inputs: 18 rows change (the absorption cases), and only by losing message events. Their state, seed and every other event are identical (`a4-ui4-d73-quest-diff.log`).
  - The reference's vitest: 7,728 tests, 97 failing, the same failing set before and after (`a4-ui4-d73-vitest.log`).
- **Still queued:** the reference answers the box questions from the inventory (D-56's reference half), which needs an interactive web script.

### 8.9 Device adaptations and rows

| Row | What |
|---|---|
| A-17 (new) | Mix: the spell from a list, the reagent picker's legend on its context bar |
| E-7 (new, records A4-UI1) | The status box's `HH:MM` clock |
| D-48 | The moongate counter is not saved (settled on load); the fizzle is a one-tick floor; the sweep is the calibrated floor |
| D-67 | The redraw between two bursts is a one-tick floor |
| D-75 | Talk / Fire keep their own direction row (not shown to reach 0x35EC) |

### 8.10 Subjective changes (mocked up; decided 2026-10-01, implemented in §8.20)

*The user chose variant B for the console (the bullet, a blank row before each command, bottom anchoring, the flame-wave cursor) and reverse video for the pickers and shops. Both are implemented in §8.20. The proposal as it was mocked up:*

1. **The console's original look, adapted to the 5×7 transcript.** All of it is derived from the reference skin and the getkey derivation:
   - the echo bullet `►` (the original's two-tone full-cell triangle) on every command row;
   - a blank row before each command (the LF getkey prints);
   - bottom anchoring;
   - the flame-wave cursor (IBM.CH 0x05–0x08) at every key wait: on a fresh `►` row when a command is awaited, at the end of the echo row in a getdir.

   The mockup is `C:\dev\ui4-scratch\mock-console.png`: today / variant B (blank rows) / variant C (dense). Cost estimate: ~1–1.5 KB of flash, one animated cell.
2. **In-game picker and shop selection in reverse video,** as every menu since A4-UI1 and as the original's 0x2a28 picker cursor. The mockup is `C:\dev\ui4-scratch\mock-picker.png`.

### 8.11 Queued (not this batch)

- **The rest of the console-echo family (D-75, D-13):**
  - Talk/Fire's direction word;
  - the dungeon's `Look...` / `Search...` / `Dir-` rows;
  - `Player: <name>` / `Item: Done` and the inline Yes/No appends;
  - the arena's `Attack-Aim!`.
- **D-56's reference half** (the interactive box questions in the web script).
- **The credits panel's curtain reveal, the intro's six story plates, the title's walking figures.** Each needs its own derivation and pack composition.
- **D-8** (the Ready picker's empty-handed path; needs 0x0f2e and touches action cost).
- **D-38's NPC bed pose** (needs a witness).
- **D-10 / D-39** sweep residuals (machine-dependent).
- **D-58 / D-59** (moongate gameplay).
- **D-50 / D-51** (gameplay).
- **D-60** (a decision).
- **D-1, D-2, D-4, D-7, D-9, D-14, D-15, D-16, D-17, D-18.**
- **Hardware status of A4-SAVE2, SAVE3 and UI3.** §4.12, §5.16 and §6.16 still read "hardware pending". A4-END1's run was on that tree, but no per-batch result was recorded. Checked against each batch's own records at the user's request (§8.21): none records a hardware PASS, so all three stay pending.
- **`speaker_segment_frames` in flash** on the per-sample audio path (`a3_04b_iram_check` RED since at least A4-END1; §8.17).
- **The console cursor at the other prompts** (yes / no, text, number, pickers; A-18) and **the strip logs' anchoring** in shops and pickers, beyond the bullet.

### 8.12 Files

- **Core:**
  - `include/openu5/ui_session.h`, `src/ui_session.cpp` (the ended line, getdir's word, the echoes, Klimb; §8.20: the console layout, `UiTextCommand`, `answer_echo`, the arena's single Cast);
  - `src/frontend.cpp`, `src/system_menu.cpp`;
  - `include/openu5/world_fx.h`, `src/world_fx.cpp` (the burst and its sound);
  - `include/openu5/narrative_scene.h`, `src/narrative_scene.cpp` (the Refuge stage);
  - `include/openu5/presentation.h`, `src/presentation.cpp` (the gate stage, the glow);
  - `include/openu5/quest_world.h`, `src/quest_world.cpp` (`moongate_night`, `moongate_stone_at`, D-73).
- **Device:**
  - `main/alpha_runtime.{h,cpp}` (Direction?, the zodiac square, the quake frames, the burst sound, the Mix hold, the Refuge's Avatar, the moongate counter and transit; §8.20: `configure_session()`, `console_ready()`, the cursor's phase);
  - `main/tdeck_board.{h,cpp}` (the release label, Small, `^`; §8.20: the bullet cell, bottom anchoring, the wave cursor and its still-frame animation, the reverse-video lists);
  - `main/device_ui_views.h` (`firmware_release_label`);
  - `main/location_names.h`, `main/native_renderer.cpp` (the world partial gate), `main/main.cpp`;
  - `CMakeLists.txt` (`PROJECT_VER` 4.0.0-alpha4-ui4-debug).
- **Reference:** `game/src/core/quest/lordbritish.ts`.
- **Tests and tools:**
  - new `host_tests/a4_ui4_presentation_runtime_test.cpp`, `host_tests/a4_ui4_moongate_runtime_test.cpp`, `host_tests/a4_ui4_console_runtime_test.cpp` (§8.20);
  - `host_tests/alpha_runtime_host_fixture.cpp` calls the production `configure_session()`; the two host Board stubs gain `animate_console_cursor()`;
  - `native/core/tools/a4_ui4_red_first.py`, `native/core/tools/a4_ui4_mutation_check.py`;
  - `native/core/CMakeLists.txt` registers the three tests.

### 8.13 Existing tests changed on purpose

| Test | Why |
|---|---|
| `ui_session` (check 153) | `Ready...` and its blank row (DS 0xa1f0) |
| `batch25_shard_ritual` R9 / Y7 / S12 | `Look-North` on one row |
| `batch53a_ending_terminal` R3 | it pinned the native `VICTORY!` at the absorption; it now pins its absence (D-72) |
| `a4_end1_ending_runtime` E1, E2, S1, S4, N1 | E1, E2, S1 and S4 tolerated a leading `VICTORY!` and now require none. N1 (a pack without the ending, which the identity lock refuses on the device): the parity path prints no narration (D-73) |
| `a3_01_audio_contract` F6 | the System Menu's Settings footer wording |
| `a4_ui2_save_menu_runtime` F1, `a4_ui3_save_ux_runtime` C1 | the main-menu footer |
| `a3_hf9_refuge_cadence_runtime` N1.3, N1.10 | N1.3: the darkness stage is all black, with no Avatar yet (was ≥ 98 of 99). N1.10: the peal's shake is 32 viewport changes, not one drop and one restore (H-212) |
| `batch7b` B1, B7–B9, B16, E9 (+ RF0) | the 174 ms burst and its redraw; the last Refuge stage is the Avatar alone; an empty void before the slumber beat |
| `batch53_release_blockers`, `batch53b_moongate_return` (their `step()`) | after a gate fires, the harness waits out the ~3 s transit, as a player does |
| `a4_ui1_chrome_runtime` G1 (golden) | `main-menu` re-recorded for the footer; the other seven byte for byte |
| `a3_04f_render_runtime` G1 (golden) | Re-recorded for the console text. Same 297 states and 2,766 render calls; with the console region masked, every state equals the UI1 recording (`a4-ui4-census-masked.log`) |
| `a3_04f_render_runtime` G1 again, T2, the window classes (§8.20) | G1 re-recorded for the console package: 301 states over the same 2,766 calls; masked, all 297 equal the pre-console recording (`a4-ui4-console-golden-proof.log`). The census gives the cursor's one cell its own `cursor` class, so the transcript rules (T1–T5) still count rows; T2's one row now holds because the echo takes the prompt row's line (comment updated) |
| `a4_ui1_chrome_runtime` G1 again (§8.20) | the four gameplay states: only the prompt row's bullet and cursor changed, 42 pixels at x 184–195, y 232–239 (`a4-ui4-console-golden-proof.log`) |
| `a3_hf10_mix_parity_runtime` N1.4, N1.5 and its cell helper (§8.20) | the cursor is the reverse-video bar, not a green `>`; a cell is "lit" by ink against its row's own background |
| `a3_hf6_shrine_key_wait_runtime` N2.5 (§8.20) | a 50 ms pause before a second roll the same way: the trackball's 12 ms debounce swallowed it once the console frame got shorter than the old frame's ~22 ms of modelled bus time |
| `a4_ui4_presentation_runtime` (`transcript_row`) (§8.20) | finds a row on the bottom-anchored console |

### 8.14 New tests and RED-first

- **`a4_ui4_presentation_runtime`** (29 checks): F1, F1b, F2, F2b, F3, F3b, F4, F4b, H1, H1b, H2, H2b, H3, H4, E1, E1b, E1c, E5, E2, E3, E3b, E4, E4b, S1, S1b, S2, S3, S3b, S3c.
  - It runs on the real runtime and Board over the fake panel, with the real audio pack.
  - The oracle holds its own glyph bytes.
- **`a4_ui4_moongate_runtime`** (11 checks): A0–A3 (the rise and fall, stage by stage, read off the composed cell and the panel against the cell's own day and night captures), T1–T6 (the transit's phases, timing and key hold), N1 (midnight).
- **RED-first** (`native/core/tools/a4_ui4_red_first.py`): every changed production file is swapped for its `e53741b2` blob, in the checkout's line endings, in a build with `-DA4_UI4_HEAD_API`, which compiles out the two checks that need new API and counts them RED.
  - **Presentation: 3 / 29**; the GREEN three are the controls F1b, F3b and S3b.
  - **Moongate: 3 / 11**; the controls are A0, T1 and T6.
  - Log: `a4-ui4-red-first.log`.
- **`a4_ui4_console_runtime`** (19 checks, §8.20): C1–C13 with C6b, C8b and C11b, and P1–P3. **RED-first 1 / 19** both against the pre-console tree (the tool's new `--snapshot`) and against `e53741b2`; the GREEN one is the control C3 (no stray blue). Log: `a4-ui4-console-red-first.log`.

### 8.15 Mutations

`native/core/tools/a4_ui4_mutation_check.py <build> [ids]` runs 33 mutants of production code, built and run against the new tests and the existing witnesses (`batch53a`, `a4_end1_ending_runtime`, `a3_hf9`, `batch7b`, `batch25`, and `quest_parity` through ctest).

The mutants by class:
- the frontend: F1–F4b;
- the screen: H1–H4;
- the console: E1–E9;
- the scenes: S1–S3c;
- the moongates: M1–M7, including M4 = the pre-A4-UI4 device;
- D-73: R1.

**First pass:** 32 killed, 1 invalid (S3 left a parameter unused under `-Werror`; rewritten). The restored build was RED on one expectation, `a4_end1_ending_runtime` N1, which D-73 legitimately changes; N1 was updated (§8.13) and END1 became R1's second witness. S3 and R1 were re-run: both killed, restored GREEN (`a4-ui4-mutation-rerun.log`).

**§8.20 adds 16 mutants** (C1–C13, P1–P3, witness `a4_ui4_console_runtime`). The full pass over all 49 (`a4-ui4-mutation-console.log`): 48 killed, 1 survived, 0 invalid, restored build GREEN. The survivor, C10 (the wave never moves on a still frame), lived because C6's room has animated cells whose frames moved the wave anyway; C6b (a still screen) was added and kills it (`a4-ui4-mutation-console-c10.log`). **49 / 49 killed.**

**Earlier final pass (the test set before §8.20): 33 / 33 killed, 0 survived, 0 invalid; restored build GREEN.** That is 32 in the full pass (`native/core/a4-ui4-mutation.log`), plus M6: its anchor had moved when the transit's key hold became time-based, and the re-anchored mutant was killed by T5 (`a4-ui4-mutation-m6.log`).

### 8.16 Host results

| Run | Result | Log |
|---|---|---|
| Baseline (`e53741b2`) | **175 / 175**, 156.73 s | `a4-ui4-baseline-ctest.log` |
| First full run after the change | 166 / 176, the ten updated on purpose (§8.13) | `a4-ui4-pass1-ctest.log` |
| After the moongate subtrack | 175 / 177: `batch53_release_blockers` and `batch53b_moongate_return` stepped through the new transit's key hold; their harnesses now wait it out (§8.13), and the hold became time-based (`moongate_transit_active()`) so it ends on its own clock even when no frame runs | (console) |
| UI4 focused | `a4_ui4_presentation_runtime` **29 / 29**; `a4_ui4_moongate_runtime` **11 / 11** | `a4-ui4-ctest.log` |
| END1 | `a4_end1_ending_runtime` **56 / 56**, `a4_end1_endgame_scene` 50 / 50, `a4_end1_endgame_pages` 11 / 11, `batch53_release_blockers`, `batch53a_ending_terminal` | `a4-ui4-ctest.log` |
| UI1 / UI2 / UI3 / SAVE regressions | `a4_ui1_chrome_runtime` 30 / 30 (one golden re-recorded), `a4_ui2_*`, `a4_ui3_*`, `a4_save1/2/3_*`, `a3_04g_storage_runtime`, `batch28_save_validation`: green | `a4-ui4-ctest.log` |
| D-73 | `quest_parity` and `quest_case_table_drift` green; the reference's vitest FAIL set unchanged (97 / 7,728) | `a4-ui4-d73-*.log` |
| Full suite | **177 / 177**, serial, 162.26 s | `a4-ui4-ctest.log` |
| §8.20 first run | 174 / 177 before the host fixture ran the device's binder (a copy of `initialize()` never turned the layout on, so only `a3_hf10_mix` saw the lists); 174 / 177 after, the failures being the two goldens, the census's transcript rules, `a3_hf6` N2.5 and two row lookups (§8.13) | (console) |
| §8.20 focused | `a4_ui4_console_runtime` **19 / 19**; presentation 29 / 29, moongate 11 / 11, END1 56 / 56, `a3_hf10_mix` 71 / 71, UI1 30 / 30, A3-04F 17 / 17, `a3_hf6` 48 / 48 | `a4-ui4-console-focused.log` |
| **Full suite (final tree)** | **178 / 178**, serial, 164.06 s; host build 0 warnings | `a4-ui4-console-{build,ctest}.log` |

### 8.17 Firmware

**Final image (§8.20, the image to flash).** Built from scratch in `build-a4-ui4-final` (`idf.py --no-ccache -B build-a4-ui4-final reconfigure`, `ninja -j 4 all` 1180 / 1180, `package_launcher.py`; `a4-ui4-final-{idf-export,fw-configure,fw-build,package}.log`): no compiler warnings, only ESP-IDF's "nearly full" notice.
- **Launcher:** `native/targets/tdeck/build-a4-ui4-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-Debug-Launcher.bin`, byte-identical to `build-a4-ui4-final/openu5_tdeck.bin`.
- **SHA-256** `09f97efe46421dd20846f7ea72f8c1b04541eb919bb0b7b774273442de2f08d3`; `FW 4.0.0-alpha4-ui4-debug`, `Git e53741b23fbc-dirty`, ESP-IDF v6.1 (`a4-ui4-final-image-identity.log`).
- **Size: 1,032,384 B (`0xfc0c0`)**: +3,248 B over the first UI4 image below, +5,696 B over A4-END1's. **16,192 B of the 1 MiB app partition free.** Partitions unchanged.
- **By section** against the first UI4 image: flash `.text` +2,956, `.rodata` +272; internal `.data` +16 (the Board's cursor state); IRAM, `.bss` and PSRAM unchanged (`a4-ui4-final-fw-size-diff-vs-{ui4,end1}.log`, `a4-ui4-final-fw-size-archives.log`).
- **ELF checks** (`a4-ui4-final-elf-checks.log`): `a3_04a_hotpath_check` GREEN. `a3_04b_iram_check` is RED on one flash function in the per-sample path, `speaker_segment_frames`, **in this image, the first UI4 image and the hardware-validated A4-END1 image alike**: it predates this batch, which touches no audio code. Not fixed here; queued (§8.11).

**The first UI4 image (superseded; not to be flashed).** From scratch in a new directory:
- `idf.py --no-ccache -B build-a4-ui4 reconfigure`;
- `ninja -C build-a4-ui4 -j 4 all`: 1180/1180, first attempt;
- `python package_launcher.py --build-dir build-a4-ui4`.

There are no project warnings. The only warning lines are ESP-IDF's "smallest app partition is nearly full (2 % free)" notice and its `component_validation` notices (`a4-ui4-fw-{configure,build}.log`, `a4-ui4-package.log`).

**The image.**
- **Launcher:** `native/targets/tdeck/build-a4-ui4/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-Debug-Launcher.bin`, byte-identical to `build-a4-ui4/openu5_tdeck.bin`.
- **SHA-256** `0f7155863c5794066aa43059154b13d74796c9c01b442644beea9bc5a5764ed2`.
- **Embedded** (read back): `FW 4.0.0-alpha4-ui4-debug`, `Git e53741b23fbc-dirty`, ESP-IDF v6.1 (`a4-ui4-final-identity.log`).
- **Size:** **1,029,136 B (`0xfb410`), +2,448 B** against A4-END1's `0xfaa80`. **19,440 B (2 %) of the 1 MiB app partition free** (was 21,888 B).
- **By section:** flash `.text` +2,392, `.rodata` +48; internal `.bss` +80 (the moongate counter and transit, the quake's drawn offset, the Refuge flag, the burst counter); IRAM, `.data` and PSRAM unchanged (`a4-ui4-fw-size-diff.log`).

**SD packs:**
- **Resource pack unchanged:** A4-END1's, 2,266,819 B, SHA-256 `85b38994eea674e48338d744091c6b895b9507a286bb38bcc84231131feed01e`. No diff to the pack tools or the loader, so **no SD recopy**.
- **Tile pack unchanged** (132,284 B, SHA-256 `6eb001ed…e188`).
- **Audio pack unchanged** (local, 56,148 B, SHA-256 `28c1533b…a6d3`).

### 8.18 Hardware checklist (A4-UI4 image)

Everything below cannot be proven on the host; every route was checked through the same host path the Developer menu uses:
- the teleport coordinates through `apply_debug_teleport`, as the moongate and underworld tests do;
- the Flame/Shard Certification through Batch 25's real menu route;
- the ending through A4-END1's route.

Saves are not touched unless a step says so.

0. **Flash.** Flash `build-a4-ui4-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-Debug-Launcher.bin` (SHA-256 `09f97efe…08d3`, 1,032,384 B; §8.17). The SD card stays as for A4-END1 (no pack changed).
1. **Boot.** The splash reads **Alpha 4** (not Alpha 2.0). The identity screen is titled **OpenU5-TDeck** and shows `FW 4.0.0-alpha4-ui4-debug` and `Git e53741b23fbc-dirty`. **Stop if not.**
2. **Title.**
   - The main menu's footer reads `Select: arrows / Enter / J C T U A R S P`.
   - **S** opens Settings; its footer reads `Left/right changes; Mic saves`.
   - Set **Text / UI: Small**: every row's letters are whole ("Load Game", "Movement default", "Brightness"); nothing reads as "Lcac". Back to Medium. Mic.
3. **In game (Continue).**
   - Alt+M → Settings: the same footer. Set Small; the console text is legible. Back to Medium.
   - Press **L** then roll up: one row reads **`Look-North`**. While it waits, the status line reads **`Direction?`** (not `Aim: empty (-1,-1)`).
   - **C** → `Cast...` (Mic out). **R** → `Ready...` then a blank row (Mic). **U** → `Use item` then a blank row (Mic). **Z** → `Z-stats...` (Mic).
3b. **The console (§8.20).** Right after Continue (Medium text):
   - The newest text sits at the **bottom** of the console. Every command row starts with a **blue ► with a white edge**, and has an **empty row above it**. Messages ("Thou dost see…") have no ►.
   - The last row is an empty ► with a **small white diagonal wave right after it, moving** (about nine steps a second).
   - **Space** (Pass): `►Pass` appears where the empty ► was, with a new empty row and ► + wave below it.
   - **L**: the wave now blinks right after `►Look-` and there is no ► row below it. Roll up: `►Look-North`, the result, and the ► + wave return.
   - **T**, roll up: `►Talk-` then the direction on its own row, **without** a ► and without an empty row between them.
   - Take ten steps; then **Shift+Up**: the view pages back and **no wave** shows; **Shift+Down**: it returns.
   - Alt+M → Settings → Text **Small**, then **Large**: the ► and the wave fill their cells at each size. Back to Medium.
   - *The console's height (the hardware run's finding) is described in §8.22.3 and retested in §8.22.6.*
3c. **The lists (§8.20).** With two or more members (Alt+D → Party size 3 if needed), **R**: in "Ready Whom?" the chosen row is a **white bar with black text** (no green `>`). Roll down and up: the bar moves and leaves no white lines behind. Mic. The other pickers (U, Z's member, M's reagents) and any shop's offers select the same way.
4. **Underworld caption.** Alt+D → Teleport → Underworld, Default Entrance **On** (126,20). The status box's caption reads **Underworld**. (Return with a Britannia teleport.)
5. **The spyglass.**
   - Alt+D → Shortcuts → **Preset: Stocked inventory** (it grants the spyglass and the reagents).
   - Time → Hour **22**. On the Britannia surface, **U** → Spyglass.
   - The zodiac fills the whole square, including the top and bottom rows: no blue bands across it.
6. **The caret (H-63 / D-52).** It needs a member with **more than seven** owned armament entries. **Z** → the member → page right to the **Armaments** inventory page. The range/detail line reads `1-7 of N` with `v`. Roll **down**: the list scrolls by one and the marker reads **`^ v`** (it read `? v`). **Left/right** still change the page, not the scroll.
   - *Corrected after the hardware run (§8.22): the first wording ("Ready a weapon … the readied item is marked `^`") pointed at the wrong place. H-63's `^` is the scroll marker. **Hardware 2026-10-01: PASS.***
7. **Mix.**
   - **M** → the first spell → mark its reagents → **M** → How much? **1** → Enter.
   - The rows read `Mix Reagents`, a blank row, `Mixing...`, then about half a second later `Done!`. Keys pressed in that half second do nothing. During that half second there is **no ► row and no wave**; both return after `Done!`.
8. **Moongates.**
   - Alt+D → Teleport → Britannia, **X 96, Y 104**, Default Entrance **Off**.
   - Time → Hour **12**: two cells north is grass.
   - Time → Hour **21**: the gate **rises out of the ground** in about a second, step by step, and lights its cell.
   - Hour **12** again: it **sinks** in about a second. Hour **21** again.
   - Step north once (96,103), then north onto the gate. The moongate sweep sounds and the party stands on the gate for ~1.2 s. The gate covers the party, then **sinks into the ground in ~1.6 s** (15 steps), leaving grass. The destination appears with its gate rising under and around the party, about 3 s in total.
   - Press keys during it: nothing moves.
   - Optional: Time → Hour **0**, Minute **5**, on the gate (step off and back on): the same animation, and the party stays.
9. **Shard ritual** (corrected route, §8.22.2: the first run's save evidently had Astaroth destroyed).
   - **Astaroth alive:** Alt+D → Quest / World → **Quest flag**: Enter, roll to `1` (it reads `shadowlord-dead:hatred`), **Enter**; the row must still read `shadowlord-dead:hatred`. Roll down to **Toggle quest flag** (it reads Yes): Enter, roll to **No**, **Enter**; the row must read No. Only then Mic/Back out.
   - *A Developer value applies only on **Enter**: Mic while it is open cancels it. Do not Alt+L after these rows: the save brings Astaroth's flag back (§8.22.8).*
   - **Hour:** Alt+D → Time → **Hour**: Enter, `12`, **Enter**. (At 05–06 and 19–20 o'clock a monk steps onto the ritual cell once the party leaves it.)
   - Alt+D → Certification → **Flame/Shard Test** → Run. The party stands **on** the ritual cell, Empath Abbey F1 (15,3); the Flame is the cell directly north.
   - Step **south** once, onto (15,4). It is a door; that is expected.
   - **Y**, `ASTAROTH`, Enter. **No text** follows: the 1988 summons is silent (CMDS 0x1030). **Astaroth appears on the Flame**, two cells north. (`No effect!` here means Astaroth is still marked destroyed.)
   - Step **north** once: back on (15,3), Astaroth directly north.
   - **U** → Shard of Hatred: the header and `...and cast it into the Flame of Love!`. After the shakes, **seven** bursts on Astaroth's cell, each held a visible moment with a **crackle**, with a brief gap between them. Then `The doom of the Shadowlord Astaroth is wrought!`; Astaroth is gone and the shard leaves the inventory.
10. **Refuge and quake** (A3-HF9's route).
    - Alt+S first, if you want to return.
    - Alt+D → Party size 1, Preset: **Low health/status**; Space until the Refuge.
    - "An unending darkness engulfs thee..." → the view is **entirely black**.
    - "But thy slumber is disturbed!" → **the Avatar appears** in the centre.
    - "There is a peal of thunder!" → the black stage **visibly jolts several times** for ~2 s (was a single drop).
    - At "Vertigo..." the last stage is **the Avatar alone** (no ghosts, no apparition). Then the castle as before.
    - **Defeat line** (optional; A4-UI2's death-music route): with the same low-health party, attack a monster and lose. **`BATTLE IS LOST!` appears once** (it appeared three times). And a won fight shows `VICTORY!` once, not again as the party walks off the board.
    - **Arena cast** (optional, in any fight): on a member's turn, **C** and choose a spell: **one** `►Cast...` row (it printed twice). While enemies move, no ► + wave shows.
11. **The ending** (A4-END1 route A: Preset Endgame, Party size 1, Doom L6 (4,7), east into the pit, north ×4).
    - After `Avatar is absorbed!` there is **no `VICTORY!`**; the throne room's text follows.
    - Nothing else in the ending needs re-checking. Alt+L back.
12. **Report:** the `FW` / `Git` lines, PASS/FAIL per step, photos of anything odd (Small text, the moongate's stages, the zodiac, the console's ► and wave, a picker bar).

*Host-validated routes:* steps 3b–3c run on the host as `a4_ui4_console_runtime` C1–C11b and P1–P2 (the same keys and Settings route); the shop list (P3) and the arena's cast (C13) are checked below the menu route (the Board's shop view, the arena session), so their steps say "any shop" / "any fight" rather than a Developer route.

### 8.19 Status

| Axis | State |
|---|---|
| Investigation and plan | delivered before production code (§8.2) |
| Objective UI4 / PRES1 fixes | **software complete** |
| Subjective changes (§8.10) | **decided (variant B, reverse video) and implemented**, §8.20: software complete |
| Host suite | **178 / 178** serial; focused 19 / 19, 29 / 29, 11 / 11; RED-first as above; mutations **49 / 49**. Hardware follow-up (§8.22): still **178 / 178** (151.66 s); console 34 / 34, shard route 51 / 51; mutations **6 / 6** |
| Firmware | first image `0xfc0c0` (1,032,384 B), 16,192 B free, SHA-256 `09f97efe…08d3`. **Follow-up image** `0xfc150` (1,032,528 B), **16,048 B free**: Launcher `build-a4-ui4-hf1/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-hf1-Debug-Launcher.bin`, SHA-256 `1d57efce…3a44` (§8.22.5); packs unchanged (no SD recopy) |
| Hardware (§8.18) | **first run 2026-10-01: PASS, except two follow-ups** — step 9's route was stale (a precondition, not a defect) and the console used 12 of its 19 rows (fixed). Caret (step 6) **PASS**. Retest of §8.22.6 **PASS** (2026-10-01, §8.22.8) |
| Closed (commit, tag) | **committed** after the retest PASS (§8.22.8), without A4-PARITY1; not tagged or pushed |

### 8.20 The console package and the reverse-video lists (decided 2026-10-01)

The user chose variant B for the console (§8.10) and reverse video for the in-game lists. Both are presentation only: no save, rule, pack or core command output changed. The transcript's stored text is the same; only how the device lays it out and draws it.

**The console, as the original does it and as the device now draws it:**

| Original | Device |
|---|---|
| `getkey_with_redraw` (0x266c) opens a command with a line feed and the bullet; the echo lands after the bullet (`►Look-North`) | a command's first row starts with the bullet, after a blank row |
| the bullet is a cell-filling two-tone ►: the frame's blue, a white edge on its slopes and tip (the reference's skin.ts `BULLET_BLUE` / `BULLET_WHITE`, calibrated to a DOS capture) | the same 8 × 8 masks, stretched end to end over the device's cell: 6 × 8 Medium, 5 × 7 Small, 8 × 10 Large |
| the console window (INTRO.OVL `set_text_window` index 2) scrolls up from its last row | the rows are anchored at the bottom |
| `poll_key_blink_cursor` (0x1b38) draws IBM.CH glyph `[0x5390]` + phase at the text position on each pass of 0x266c (one tick and a map redraw per pass; ~100 ms in the video, `re/notes/getkey-cursor-derivacion.md`) | IBM.CH 0x05–0x08, read from the pack's font, cut to the cell's width, one glyph every two ticks (110 ms): after the live bullet row when a command is awaited, after the echo in a getdir |

**Where it lives:**
- **Core** (`UiSession`): `set_console_layout()`, `set_console_ready()`, `console_cursor()` and the `UiTextCommand` flag. The wrapped lines carry the blank rows and the live prompt row, so Shift+Up / Shift+Down page by exactly what is drawn. The layout is off by default; a plain session's lines are unchanged.
- **Which echoes start a command:** `command_echo()` (a key's own echo), the walk echo and the arena's echo events. Answers use the new `answer_echo()` and get no bullet and no blank row: Talk / Fire's direction word, Klimb's Up / Down, Ahead / Here / Left / Right.
- **Device:** `AlphaRuntime::configure_session()` turns the layout on. It is one binder shared by `initialize()` and the host fixture: the fixture's copy of `initialize()` had hidden the layout from every runtime test until the first run showed it.
- **Readiness:** `console_ready()` shows the prompt row and cursor only when the original would be in getkey. Nothing may be holding the keyboard: a paced scene; the dialogue pacer (TLK pauses, Mix's 10 ticks, rite effects); a map reveal or a moongate transit; the gem, zodiac or Z-stats views; a ritual effect, world effect or quake; an enemy's turn.
- **Board:** the bullet is cell 0x10 of a row, in the world console and in the shop and picker logs. The cursor is one cell drawn on its own: a full frame places it and an animation frame moves it. A still frame redraws only that cell when the phase moves; the A3-04F census counts it as its own `cursor` class, so its transcript rules are unchanged.
- **Found on the way:** the arena's (C)ast printed `Cast...` twice: the key's own echo (renamed from "Cast" in §8.5) and `combat_cast()`'s `Cast...\n` (the combat parity pin). The key no longer echoes (the ledger's D-75 note).

**The lists:**
- The party, inventory, equipment, spell and reagent pickers and the shop offers now select with kernel 0x2a28's reverse-video bar, as every menu has since A4-UI1, not a green `>`.
- The bar is white across the panel's 134 px row and the row's whole 14 px pitch. The text stays where it was, and a row the bar leaves is black again.
- The Developer screen keeps its `>`: it is not a game screen.

**Adaptations (ledger A-18):**
- the masks and glyphs are fitted to the device's cells;
- the cadence is two ticks;
- the cursor blinks only at command waits and getdir. The original also blinks it at yes / no, text, number and picker prompts; on the device their keys are shown in the context bar or the picker panel.

**Rows:** D-76 (the console) and D-77 (the lists) are fixed, hardware PASS 2026-10-01 (§8.22.8); A-18 is new; D-75 has a note for the arena's Cast.

**Evidence:**
- `a4_ui4_console_runtime`: 19 / 19 on the final tree; RED-first 1 / 19 against both bases (the GREEN one is the control C3).
- The golden proof (`a4-ui4-console-golden-proof.log`).
- Mutations C1–C13 and P1–P3: all killed.
- Full suite 178 / 178.
- Firmware: +3,248 B, 16,192 B free (§8.17).
- Hardware: steps 3b, 3c and the additions to 7 and 10 (§8.18).

### 8.21 Hardware status of A4-SAVE2, A4-SAVE3 and A4-UI3 (checked 2026-10-01)

At the user's request each batch's own records were checked for a hardware PASS:
- **A4-SAVE2:** §4.11 is a checklist with no result; §4.12 reads "pending". Its logs are host and firmware logs only. The commit that carried it (`d6457ab1`, A4-END1's) says their records are "their own sections, unchanged here", and the PASS in `e53741b2` is A4-END1's own.
- **A4-SAVE3:** §5.15 / §5.16 "pending"; no DOS round trip recorded.
- **A4-UI3:** §6.14 / §6.16 "pending".

None records a PASS, so all three stay **pending**; the A4-END1 run on that tree is not counted for them. (§7's opening line calls that tree "hardware-validated"; of the four, only A4-SAVE1, §3.6, has its own PASS.)

### 8.22 Hardware follow-up (2026-10-01)

The user ran §8.18 on the A4-UI4 image (`FW 4.0.0-alpha4-ui4-debug`).
- **Every step tested passed except two follow-ups:** step 9 (the shard ritual) and the console's height.
- **Step 6 (the caret) passed** once found in the right place: H-63's `^` is the Armaments page's scroll marker, not a mark on the readied item.

This is a hardware follow-up, not a feature batch. Nothing else changed: the moongates, the ending, save/load, the Refuge, the quake, the ritual's timing and the console's other rules are untouched.

#### 8.22.1 The caret (H-63 / D-52)

- **Hardware PASS.** The `^` renders on the Armaments inventory page when a list of more than seven entries is scrolled.
- The checklist's wording was wrong; step 6 now describes H-63's own case (`ALPHA2_HARDWARE_CHECKLIST.md` H-63):
  - Z-stats, page to Armaments, more than seven entries;
  - scroll down: `^ v`, not `? v`;
  - left/right still change the page.
- No production change.

#### 8.22.2 The shard ritual: a stale checklist, not a defect

**What the hardware showed:**
- Certification placed the party on the ritual cell.
- One step south: a door.
- `Y` `ASTAROTH`: "did nothing".
- Back north, the Shard of Hatred printed only `...and cast it into the Flame of Love!`. No ritual effects.

**What the code and the binary say:**

| Question | Answer |
|---|---|
| Where the Certification puts the party | `apply_debug_certification(FlameShard)`: Empath Abbey (location 31), **floor 1, (15,3)**, by an explicit-coordinate teleport. It grants the three shards and leaves Shadowlord progression untouched (PART 9, by design). The party starts **on** the ritual cell. Facing is irrelevant (the town has none). |
| The ritual's gates (CAST 0x15b4 → `cast_shard_into_flame()`) | the party on (15,3), location 31, floor 1 (else `No effect!`); then the tile at (x, y−1) is a Shadowlord, 0xFC (else silence after the Flame line); then the summoned index is the shard's, 1 (else silence) |
| How the Shadowlord gets there (CMDS 0x1030 → `yell_in_world()`) | Yell the name **in the Flame room**; it lands at **(x, y−2)**. Gates: location 30–32; the name matches; y ≥ 2; **that Shadowlord alive** (0x1076, `[0x58c8+idx] != 0xff` = the port's `!quest_flag(HatredDead)`); none already present. Success is **silent**. |
| So where the Yell happens | **one cell south of the ritual cell, (15,4)**: Astaroth appears on the Flame (15,2); one step north puts him directly above the ritual cell (`re/notes/shadowlord-ritual.md`, "Flujo completo") |
| Was south → Yell → north stale? | **No.** It is the binary's own sequence, and Batch 25's S9–S10 pass it on the host. What the checklist omitted were two **preconditions**. |
| Does the production ritual work? | **Yes**, through the device's own keys and Developer rows: `batch25_shard_ritual` A4–A8 (below). |

**The cause of the hardware result (inferred; the save itself was not read):** Astaroth was already destroyed in the save.
- A4-END1's route A applies **Preset: Endgame**, which sets all three `shadowlord-dead` flags. The Certification does not clear them.
- With Astaroth dead, the original refuses the summons with `No effect!` (CMDS 0x1076). The shard then prints the Flame line and stops (CAST 0x16c1: no 0xFC above).
- That is exactly the hardware result, and the only state found that produces it.
- The other candidate, the hour, gives a different result. At 05–06 and 19–20 o'clock a monk steps onto (15,3) behind the party: the step north is `Blocked!` and the shard prints `No effect!`.
- The photographed party (six maxed members, F:9993, G:9999) is consistent with the Developer presets.

**Host proof** (`batch25_shard_ritual`, new block A, 9 checks, all through the Developer menu's rows and the device's keys):
- **A1–A3** reproduce the hardware result:
  - Preset: Endgame → Certification → south → Yell: `No effect!`, the Flame empty;
  - north → Use Hatred: the header and the Flame line only, the shard kept.
- **A4–A8** pass the corrected route:
  - Quest / World: Quest flag 1, Toggle 0; Time: Hour 12;
  - Certification on (15,3); south; a silent Yell;
  - the composed view shows the Shadowlord (tile 252 + 256) on the Flame; north, with him directly above;
  - Use Hatred: doom wrought, shard consumed, `HatredDead` set, Astaroth gone;
  - Move answers.
- **A9** is the hour control: at Hour 6 the step north is `Blocked!`.
- Result: 51 / 51 GREEN; no production code changed, so no RED-first or mutation.
- Step 9 of §8.18 now carries the corrected route.

#### 8.22.3 The console's height

**Measured:**
- The console is the rectangle under the status box's rule, x 184–318, **y 88 to the glass (240)**. With a context bar up it ends at y 215.
- The Board lays it out from `world_transcript_geometry()`:

| Text | Cell | Rows (no bar) | Bottom | Rows (bar) | Before |
|---|---|---|---|---|---|
| Small | 5 × 7 | **21** = (240 − 88) / 7 | y 235 | **18** | 12 (19 laid out) |
| Medium | 6 × 8 | **19** = 152 / 8 | y 240 | **15** | 12 |
| Large | 8 × 10 | **15** = 152 / 10 | y 238 | **12** | 12 |

**Why it showed 12:**
- `UiSession::visible_lines()` returns at most `config_.page_rows`.
- The device built its session with `{11, 12, 63}`: page_rows **12**, from before the 15–21-row layout.
- So a full console drew 12 lines. Bottom anchoring put them at the bottom and left the top 3–9 rows black. That is the photo: 12 rows of 8 px, y 144–240, under 56 px of black.
- **A second symptom of the same cap:** Shift+Up pages by the Board's row count (19 at Medium, `set_transcript_view_metrics`) while only 12 rows were shown. Each page skipped 7 lines.
- Small had a cap of its own: the Board's row arrays (`kAlphaTranscriptLines`) held 19, the 8 px cell's count, so Small could not reach its 21 rows.

**The fix** (no layout redesign; the viewport, status box and bars do not move):
- `kConsoleMaxRows` (`device_ui_views.h`) = (240 − 88) / Small's 7 px = **21**. It sizes the Board's rows (`kAlphaTranscriptLines`).
- `AlphaRuntime::session_config()` sets page_rows to it. So the Board's own geometry decides what is drawn: 21 / 19 / 15 rows, or 18 / 15 / 12 above a context bar.
- The construction is one binder, `AlphaRuntime::construct_session()`. `initialize()` and the host fixture both call it; the fixture had its own copy of `{11, 12, 63}`.
- Everything else is as it was:
  - bottom anchoring;
  - the bullet and its blank row;
  - the wave cursor, and no cursor while scrolled back;
  - Shift+Up / Shift+Down, which now page by exactly the rows drawn.

**Host proof** (`a4_ui4_console_runtime`, new block H, 15 checks, real Board over the fake ST7789, per text size):
- **H1:** a full console fills every row from y 88, each row inked exactly where its line has text.
- **H2:** after the System Menu's full repaint, no console window starts above y 88 or runs past the console's bottom.
- **H3:** the live prompt row is the last row; its bullet and the wave cursor fit inside it.
- **H4:** Shift+Up moves back exactly the rows drawn, full and cursorless; Shift+Down brings the cursor back to the last row.
- **H5:** with the context bar up (`Look-`), the rows stop above y 215 and the cursor follows `Look-` on the last row.

**Results:**
- **RED-first** (`tools/a4_ui4_hf1_red_first.py`, the follow-up's files swapped for their pre-follow-up copies): **25 / 34**. All nine REDs are H checks. The GREEN H checks are the controls: H2 Medium / Large, H3, H5 Large (whose 12 rows equalled the old cap). Log: `a4-ui4-hf1-console-red-first.log`.
- **GREEN 34 / 34** (`a4-ui4-hf1-console-green.log`).
- **Mutations:** 6 / 6 killed (`tools/a4_ui4_hf1_mutation_check.py`, `a4-ui4-hf1-mutation.log`):
  - page_rows 12, 19, or one short;
  - the cap from Medium's metrics;
  - the Board's rows back to 19;
  - the shared constructor ignoring `session_config()`.

#### 8.22.4 Goldens

- **`a4_ui1_goldens.h` is unchanged:** its screens hold fewer than 12 lines.
- **`a3_04f_panel_goldens.h` moved** (G1: first difference at state 39).
- **The proof** (`a4-ui4-hf1-golden-proof.log`): each panel state was hashed with the console rectangle (x 184–318, y 88–239) blanked. The pre-follow-up and the follow-up trees give the same **297 masked states, 0 differing**.
  - The old files still pass the old golden, 17 / 17.
  - 255 of the 301 unmasked states differ (39–300: from there the transcript holds more than 12 lines).
- **Re-recorded twice:** the two files are byte-identical (SHA-256 `F59CBE3A…60E9`). The hashes went under the file's provenance header; G1 GREEN, 17 / 17.

#### 8.22.5 Firmware and packs

- **Version:** `PROJECT_VER` is now `4.0.0-alpha4-ui4-hf1-debug`. The retest image must never share a name or an `FW` line with the first image: both embed `Git e53741b23fbc-dirty`.
- **Build:** clean, `build-a4-ui4-hf1` (`idf.py --no-ccache reconfigure`, then `ninja -j 4`), first attempt.
- **Launcher:** `native/targets/tdeck/build-a4-ui4-hf1/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-hf1-Debug-Launcher.bin`. Byte-identical to `openu5_tdeck.bin`; embeds `4.0.0-alpha4-ui4-hf1-debug` and `e53741b23fbc-dirty`.
- **SHA-256** `1d57efcefec3eec385a36b753f1e57a4d4825d3b03ab3726f30ab14737be3a44`.
- **Size:** **1,032,528 B (`0xfc150`), +144 B** against the first A4-UI4 image. **16,048 B (2 %) of the 1 MiB app partition free.**
- **By section** (`a4-ui4-hf1-fw-size-diff.log`): flash `.text` −56; internal **`.data` +208**. That is the Board's two extra cached rows: the Board is a static object, so its members are internal `.data`. IRAM and PSRAM are unchanged.
- **Stack:** `Board::render()`'s row buffer grows by two rows on the game thread's stack (about 200 B).
- **ELF guards** (`a4-ui4-hf1-elf-checks.log`): hot path GREEN. The IRAM check's one RED is the pre-existing `speaker_segment_frames`, unchanged since A4-END1.
- **Packs unchanged, so no SD recopy:**
  - resource pack 2,266,819 B, SHA-256 `85b38994…d01e`;
  - tile pack 132,284 B, `6eb001ed…e188`;
  - audio pack 56,148 B, `28c1533b…a6d3`.

#### 8.22.6 Mini retest (the follow-up image)

0. **Flash** `build-a4-ui4-hf1/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-hf1-Debug-Launcher.bin` (SHA-256 `1d57efce…3a44`). Keep the SD card as it is. The identity screen must read **`FW 4.0.0-alpha4-ui4-hf1-debug`**; **stop if it reads `…-ui4-debug`.**
1. **The console's height** (Continue, Medium text):
   - Press **Space** about 15 times. The console fills the **whole black area** from just under the status box down to the bottom edge: **19 rows**, no black band above them. The newest `►` + wave is still on the last row.
   - **Shift+Up:** the previous page, full, with no wave. Press it again if the history allows. **Shift+Down** back to the bottom: the wave returns on the last row.
   - **L** (the context bar opens): the rows stop above the bar, and the wave follows `►Look-` on the last row above it. Roll a direction to finish.
   - Alt+M → Settings → Text **Small**: about **21** rows, filling to the bottom. **Large:** about **15** rows. At each size the `►` and the wave fit their cells, and nothing overlaps the status box or the bars. Back to **Medium**.
2. **The shard ritual:** before changing it, note what **Toggle quest flag** reads with Quest flag `1` selected: `1` confirms §8.22.2's cause; `0` means Astaroth was alive and the first run failed for another reason (report it). Then §8.18 step 9 as now written (Astaroth alive, Hour 12, Certification, south, a silent Yell with Astaroth appearing on the Flame, north, Use Hatred). Expect the seven bursts and `The doom of the Shadowlord Astaroth is wrought!`. *Each Developer value applies only on **Enter**; Mic while it is open cancels it, and an Alt+L after the rows restores the save's flag (§8.22.8).*
3. **Optional:** Developer → Diagnostics → Audio/render stats: the internal heap figures look as before. The image's internal `.data` grew by 208 B.
4. **Report:** the `FW` line, PASS/FAIL for 1 and 2, and a photo of a full Medium console.

#### 8.22.7 Status

| Axis | State |
|---|---|
| Caret (step 6, H-63 / D-52) | **hardware PASS** (2026-10-01) |
| Shard ritual (step 9) | checklist corrected twice (the precondition, §8.22.2; Enter, §8.22.8); production unchanged; **hardware PASS** (2026-10-01): D-67's seven bursts seen |
| Console height | **software fixed**; RED-first 25 / 34 → GREEN 34 / 34; mutations 6 / 6; **hardware PASS** (2026-10-01) |
| Suite | **178 / 178** serial (151.66 s, `a4-ui4-hf1-ctest.log`); closeout 178 / 178 (157.86 s, `a4-ui4-closeout-ctest.log`) |
| Firmware | `0xfc150`, 16,048 B free; SHA-256 `1d57efce…3a44` |
| A4-UI4/PRES1 | **closed**: hardware PASS (§8.22.8); committed without A4-PARITY1; not tagged or pushed |

#### 8.22.8 Retest result (2026-10-01): PASS

The user ran §8.22.6 on the follow-up image: `FW 4.0.0-alpha4-ui4-hf1-debug`, `Git e53741b23fbc-dirty`, SHA-256 `1d57efcefec3eec385a36b753f1e57a4d4825d3b03ab3726f30ab14737be3a44` (re-hashed unchanged at the closeout).

- **Step 1, the console's height: PASS.**
- **Step 2, the shard ritual: PASS, on the second attempt.** Astaroth answered the Yell and appeared on the Flame; Use Hatred played the seven bursts, then `The doom of the Shadowlord Astaroth is wrought!`. D-67 is now seen on hardware.
- The PASS was reported in writing; the photo of the finished ritual did not reach the session.

**The first attempt printed `No effect!`. Cause: the checklist's setup, not the firmware.**
- What the device showed: Toggle quest flag read Yes and was set to No; Certification, south, `Y` `astaroth`: `No effect!`. The transcript holds a `Load complete` (an Alt+L) before the step south.
- A Developer value applies only on **Enter** (`UiDebugMenu::handle_input`: Confirm calls `apply_value()`; Cancel / Back leave the edit and drop the value). §8.18 step 9 read "set it to `1` … set it to `0`. Mic/Back out", which allows Mic while a value is still open.
- **Host probe** (a temporary block in `batch25_shard_ritual_test.cpp`, removed afterwards; the file is byte-identical to the tested tree and `batch25_shard_ritual` passes). It drove the device's keys through `AlphaRuntime::handle()` from Preset: Endgame, then Hour 12, Certification, south, `Y` `astaroth` (`a4-ui4-hf1-retest-probe.log`):

| Developer rows | Astaroth at the Yell | Yell |
|---|---|---|
| Enter on both (one member; six members) | alive | silent: Astaroth summoned |
| Quest flag left with Mic (it stays at flag 0, Faulinei, which also reads Yes; the toggle revives Faulinei) | destroyed | `No effect!` |
| Toggle left with Mic | destroyed | `No effect!` |
| Enter on both, then Alt+L | destroyed (the save's flag) | `No effect!` |

- The Yell's name match folds case, as the original does, so a lower-case `astaroth` is fine.
- Which of the three failing sequences ran on the device was not determined. The second attempt, with Enter on both rows, passed.
- §8.18 step 9 and §8.22.6 step 2 now say Enter and warn against an Alt+L. No production change.

**Rows** (the ledger). The first run's record (§8.22: every step tested passed except the two follow-ups) and this retest move the rows that the **required** §8.18 steps exercise to hardware PASS:
- D-12 (step 5);
- D-48 (step 8);
- D-53's `Direction?` half and D-75's fixed echoes (step 3);
- D-67 (step 9, this retest);
- D-68 and D-69 (step 10);
- D-71's echo and wait (step 7);
- D-72 (step 11);
- D-76 (step 3b and this retest's console height) and D-77 (step 3c).

D-52 was already PASS. Two items stay **hardware pending**, because their steps were optional and not reported: D-74 (step 10's defeat / victory lines) and the arena's single `Cast...` (step 10's arena cast, D-75's note).

**Closeout.**
- The tree is A4-UI4/PRES1 + hf1 alone. A4-PARITY1, developed on top of it and still uncommitted, was set aside first.
- The tree was verified byte-identical to the copy preserved before PARITY1 began: 100 files, plus the tracked diff.
- Suite: **178 / 178** serial on a fresh `build-a4-ui4-closeout` (157.86 s, `a4-ui4-closeout-ctest.log`), and again after these records (146.84 s, `a4-ui4-closeout-final-ctest.log`). No project warnings: the one warning line is w64devkit's `stl_uninitialized.h` false positive.

## 9. A4-PARITY1 — release-readiness parity sweep

A final parity audit before an Alpha 4 release candidate, then the fixes the user ruled release-blocking. Investigation first (no production change), then reference-first fixes. The audit's own axis is `GAMEPLAY_INTEGRATION_AUDIT.md`, "Alpha 4 A4-PARITY1"; the ledger rows are D-50, D-78 – D-82, A-19, A-20.

### 9.1 Baseline

- `main` at `e53741b2` (tag `alpha4-end1-hardware-validated`) **plus the uncommitted A4-UI4/PRES1 + hf1 tree** (45 tracked files changed, 55 untracked). That tree is still hardware-retest pending (§8.22.6); its image is untouched: `build-a4-ui4-hf1/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-ui4-hf1-Debug-Launcher.bin`, 1,032,528 B, SHA-256 `1d57efcefec3eec385a36b753f1e57a4d4825d3b03ab3726f30ab14737be3a44` (re-hashed before and after this batch). Every PARITY1 build uses new directories.
- A copy of the UI4/hf1 working tree (its 45 changed and 55 new files) was kept outside the repository before any edit, so the UI4 commit can still be made without PARITY1 (`C:\dev\parity1-scratch\ui4-hf1-baseline`; the patch `baseline-tracked.patch`).
- Host: a fresh build of that tree, serial ctest **178 / 178** (157.16 s).
- Packs: resource 2,266,819 B `85b38994…d01e`, tiles 132,284 B `6eb001ed…`, audio 56,148 B `28c1533b…`.

### 9.2 The audit and the user's decisions

The investigation report (2026-10-01) reconciled the ledger, this document, the audit, the hardware checklist and the Alpha 2/3 records, and proposed the scope below. The user decided:

| # | Item | Decision |
|---|---|---|
| 1 | NEW-1, the palace crown gate (D-78) | **RC blocker — fix** |
| 2 | P1b, Use crown's time-spell write (D-79) | **include**, if the bytes confirm it (they do, §9.4) |
| 3 | NEW-2, the dungeon's command keys (D-81) | **RC blocker — fix** M, N and the B / E / P / T / Y strings as one pass |
| 4 | NEW-3, `speaker_segment_frames` in flash | **RC blocker — smallest targeted change** |
| 5 | D-50, the well's case-folding | include only if small and self-contained (it is, §9.8) |
| 6 | the credits curtain, the intro's story plates, the title's walking figures | **deferred past Alpha 4** |
| 7 | D-60 | a documented deliberate row (A-19) |
| 8 | D-1, D-2 | stay deferred |
| 9 | D-7 | documented as the intended Movement Mode contract (A-20) |
| 10 | the RC build | stays Debug, with the Developer menu |
| 11 | NEW-4, the underworld skiff | classify; fix only if a tiny, sourced omission (it is not, §9.8) |
| 12 | push | nothing |
| 13 | the UI4/hf1 artifact | untouched; UI4/PRES1 stays hardware-retest pending |

### 9.3 NEW-1 — the palace gate reads possession (D-78)

**The binary** (independently re-derived this batch; every overlay and ULTIMA.EXE scanned for instructions on `[0x57b3..0x57b5]`):

| Address | Instruction | Meaning |
|---|---|---|
| SJOG 0x16e6 | `mov byte [0x57b4], 0xff` | the **only** writer: the Get of the crown |
| CAST 0x0e45 | `cmp byte [0x57b4], 0` / `je 0x0e53` ("Absorbed!") | the out-of-combat gate in location 0x12 |
| COMBAT 0x092f | `cmp byte [0x57b4], 0` / `jne 0x095e` | the arena's twin (after 0x0928's `'N'` test) |
| ZSTATS 0x09e1 | `mov al, [0x57b4]` | the inventory display |

`[0x57b4]` is SAVED.GAM +0x20E (the window starts at DS 0x55A6): `lbArtifacts.crown` in the reference, `quest.artifacts[1]` in native — the crown's **possession**. Both ports gated on a separate "worn" flag (`wornCrown` / `worn_crown`) that only (U)se set and no save carried: a party that picked the crown up in the palace still had its spells absorbed until it "wore" it, and again after any load.

**The fix.** `castAbsorbedOutOfCombat` / `combatCastAbsorbed` (reference) and `cast_spell` / `combat_cast` (native) read possession. The parity drivers (`generate-magic-fixtures.ts`, `generate-advanced-combat-fixtures.ts`, `magic_parity_test`, `advanced_combat_parity_test`) now feed possession from the same expression the worn flag used, so the NEW-1 change alone leaves both corpora byte-identical; the RED proof is native against the corrected drivers (§9.12).

### 9.4 P1b — the worn crown is time spell 0x1c (D-79)

**The binary:** CAST 0x193e prints "Crown", pushes 0x1c to the toggle 0x1764 (if `[0x587a]` already holds it: "Removed!", `[0x587a]` = `[0x588e]` = 0, `[0xa9fa]` = 1) and otherwise prints "Thou dost don the Crown of Lord British...", pushes 0x1c / 0xff / 9 and calls 0xc132 — with CAST's load base 0xBF80, kernel 0x80b2, an overlay thunk (load overlay 0x12, `ljmp 0:0xead8`) to CAST2 0x08f8, `set_time_spell(anim, turns, status)`: `[0x587a]` = 0x1c, `[0x588e]` = 0xff, then the animation 9. The amulet (0x1908, value 0x0e) and the badge (0x1b47, 0x1d) use the same byte; the reference had already corrected the badge the same way.

**The fix.** `useCrown` (reference) and `use_quest_item` (native, the crown joins the amulet / badge arm) toggle `timeSpell` 0x1c with 255 turns. The worn crown therefore replaces Quickness, Negate or any other time effect, is removed by a second Use, and is saved and loaded with the time spell (SAVED.GAM 0x2D4 / 0x2E8). `wornCrown` / `worn_crown` stay in the state for old saves but are no longer written. The animation 9 is not drawn (as for the amulet and badge).

### 9.5 P1c — Negate and the worn crown stop enemy magic (D-80)

Proving P1b found the time-spell byte's readers: 0x1c is read in three places, each beside Negate's 'N', and neither port had any of them:

| Site | Instructions | Effect with `'N'` or 0x1c |
|---|---|---|
| COMBAT 0x0185–0x019b | after the 50 % roll (0x017a), `test word [type*2 + 0x153c], 0x8000` (LE 0x8000 = the clone's `rangedMagic`, `re/notes/combat.md` §11) | a magic projectile is **not fired** (0x019d: the roll's own "no shot") |
| COMBAT 0x0f27–0x0f3b | `test word [...0x153c], 0x2000` (teleport) | no teleport and no `rand(0,3)`; the creature moves normally (0x0fab) |
| COMSUBS 0x0112–0x011e | before any special (`re/notes/comsubs-ataque-jugador.md` §19, where 0x1c was "unnamed") | no possession, invisibility or daemon, **zero draws** |

**The fix.** `negatesEnemyMagic()` (reference) / `negates_enemy_magic()` (native) and the three gates, at the same points of `enemyAttack`, `enemyMove` and `enemySpecial` / `enemy_attack`, `enemy_move` and `enemy_special`. Negate magic (In An, the spell and the scroll) now does what the original's does to a mage or a daemon; the worn crown does the same.

**The corpus.** The default combat corpus mounts none of the gated abilities and no time spell. `generate-combat-fixtures.ts --negate` (new; default and real-arena modes unchanged, `--check` byte-identical) mounts exactly them — rangedMagic, teleport, possess, invisibility, daemon — under no time spell, 'N' and 0x1c, and fails if a gated ability fires under 'N' or 0x1c (`NEGATE_FORBIDDEN`) or if none fires without one. `combat_parity_tests ... --negate` replays it draw by draw: `fixtures/combat-negate.txt`, 56,160 rows (ctest `combat_negate_parity`, drift `typescript_combat_negate_fixture_drift`).

### 9.6 NEW-2 — the dungeon's command keys (D-81, closes D-4)

**The binary** (`re/notes/dungeon-dispatch-gates.md` §3, re-read; the strings re-read from DATA.OVL by offset): DUNGEON.OVL's `sub_06C4` handles only movement, `5`, the digits, Enter / `.` and two control keys, and hands every other key to the shared kernel dispatcher (0x07a0 → 0x3178). M (CMDS 0x1AD8) and N (CMDS 0x0DDC) are therefore the overworld's handlers, ungated underground; the refusals print:

| Key | Original output (DATA.OVL) | Turn |
|---|---|---|
| B | `Board ` (DS 0xa13a) + `\nNot here!\n` (0x4252) | yes |
| E | `Enter what?\n` (0xa156) | yes |
| F | `Fire-` (0xa164) + `What?\n` (0x42e4) | yes |
| P | `Push\nNot here!\n` (0xa1d4) | no |
| T | `Talk-Funny, no response!\n` (0xa22c) | yes |
| X | `X-it ` (0xa280) + `what?\n` (0x4368) | yes |
| Y | `Yell what?` + a word, then `\nNo effect!\n` (CMDS 0x14ac, DS 0x453a) | no |

**The device before:** M opened **Cast** (A-7's "alias"), and every other key here printed the kernel default "What?" with no turn. The core already accepted Mix and New order underground (`commands.cpp` handles both before the dungeon gate).

**The fix** (`UiSession::handle_dungeon`, plus one core branch): M = the overworld's Mix ("Mix Reagents", the spell list for a mix, the reagent picker); N = New order's party picker; B / E / F / T / X print their row(s) and spend the dungeon's cast turn (`DungeonAction::Tick`: the clock, the dungeon's tick, the Refuge check — the reference's `dungeonSpellTurn`); P prints and spends nothing; Y asks for its word and the core answers through `yell_in_world()`'s own "\nNo effect!\n" branch (a new pass-through before the dungeon gate). D keeps the E-3 "Drink"; Q keeps the device shell (A-5). A-7 loses its Mix clause.

### 9.7 NEW-3 — the IRAM guard

`a3_04b_iram_check` was GREEN on Alpha 3's release image and RED on the A4-END1, A4-UI4 and hf1 images: `speaker_segment_frames` (16 instructions), called by `SpeakerVoice::begin_segment` (IRAM) on the audio task, had become an out-of-line flash function. One entry in `main/audio_iram.lf` places it in IRAM again; `a3_04b_perf` gains **I1b** (the entry is present). The ELF guard on the new image: §9.16.

### 9.8 D-50 (fixed) and NEW-4 (D-82, deferred)

**D-50.** The well matches with kernel 0x6f1e (LOOKOBJ 0x00aa; `native/core/batch52-h22-wish-stristr-disasm.log`). The clone, in the reference (`kernelStristr`) and native (`look.cpp`), is exact: each byte loses bit 7 (`and 0x7f7f`) and is folded with `and 0x5f` when above 0x60; a needle longer than the haystack is −1; and after a mismatch the start advances by the characters already matched plus one, so the original misses an overlapping match ("HHorse" does not contain "Horse") — kept. Small and self-contained: one function a port, no other text input changed. `horse` and `HORSE` now work as `Horse` did (the T-Deck types lower case).

**NEW-4 → D-82, deferred.** INIT.OOL / UNDER.OOL seed five outdoor objects in the underworld: slot 23, a `SkiffRight` (0x29) at (14,242); slots 24–27, four `DeadBody` (0x1e) beside the Amulet's cell (105,225) (`re/notes/moonstone-loc-y-pozo-doom.md` §2.5; BRIT.OOL seeds nothing). Neither port places them: both read `init.ool` only as the template for **writing** SAVED.OOL. Not fixed: how the original reads `.OOL` into the actor table is not derived (the same note, §2.4), the skiff's Native representation is open (A4-SAVE3 carries it as a terrain cell), and the change would move new-game state in both ports and their fixtures. Not a blocker: a skiff also travels with a ship, and the bodies are decor.

### 9.9 Deliberate rows and the ledger

- **A-19** (was D-60): audio never paces the game; the fanfare plays while play continues.
- **A-20** (was D-7): dungeon keyboard movement is the Movement Mode contract.
- **A-7** revised (no Mix alias); **A-9 / A-10 / A-12** keep their behaviour, with the stale "ASCII-only" cause corrected.
- **Stale rows struck from their own records** (no hardware status raised): D-17 (the function is in the map's *discarded* sections — dead source, not image bytes), D-21, D-22 (7E-G accepted), D-44 – D-46, D-49 (7E PASS), D-47 (7E-E accepted), D-55 (7E-A′ PASS), D-53's `To phase:` half (7E-C PASS).
- Not changed: A4-SAVE2 / SAVE3 / UI3 stay hardware pending (§8.21). The UI4/PRES1 rows keep their own status: hardware PASS at UI4's closeout (§8.22.8), except D-74.

### 9.10 Files

- **Reference** (`game/src/core/`): `magic/cast.ts`, `combat/combat.ts`, `endgame/use-tools.ts`, `world/wishingwell.ts`, `world/blackthorn.ts` (`TIME_SPELL_CROWN`); `game/src/main.ts` (the arena gate's caller).
- **Reference tests** (`game/tests/`): `cast-absorbed-gate`, `use-tools`, `shrines`, `combat-spells`.
- **Native core:** `src/magic.cpp`, `src/combat.cpp`, `src/combat_magic.inc`, `src/quest_world.cpp`, `src/commands.cpp`, `src/look.cpp`, `src/ui_session.cpp`.
- **Device:** `main/audio_iram.lf`; `CMakeLists.txt` (`PROJECT_VER` `4.0.0-alpha4-parity1-debug`).
- **Corpora and drivers:** `tools/generate-combat-fixtures.ts` (`--negate`), `tools/generate-magic-fixtures.ts`, `tools/generate-advanced-combat-fixtures.ts`; `fixtures/advanced-combat.txt` (+ coverage) regenerated, `fixtures/combat-negate.txt` (+ coverage) new; `tests/combat_parity_test.cpp` (`--negate`), `tests/magic_parity_test.cpp`, `tests/advanced_combat_parity_test.cpp`.
- **Tests:** `targets/tdeck/host_tests/a4_parity1_runtime_test.cpp` (new), `tests/a3_04b_perf_test.cpp` (I1b), `tests/ui_session_test.cpp`; `native/core/CMakeLists.txt` (3 new ctest entries).
- **Tools:** `tools/a4_parity1_red_first.py`, `tools/a4_parity1_mutation_check.py`.
- **Docs:** this section, the ledger, the audit.

### 9.11 Existing tests changed on purpose

- `ui_session_test.cpp`: dungeon `m` asserted the Cast alias (`UiRequestId::Spell`); now Mix (`UiRequestId::Custom`).
- `game/tests/cast-absorbed-gate.test.ts`: the palace case set `wornCrown`; it now sets `lbArtifacts.crown` (the reference's FAIL set showed this one case and no other).
- `magic_parity_test` / `advanced_combat_parity_test` and their generators: the crown input is possession.

### 9.12 New tests and RED-first

| Test | What | RED-first | GREEN |
|---|---|---|---|
| `a4_parity1_runtime` (real runtime and Board, raw keys) | C1–C4 the crown (a cast in the palace with the crown carried, the control without it, Use writes 0x1c permanent and replaces Quickness, it survives Alt+S / Alt+L, a second Use removes it); D1–D4 the dungeon keys (M, N, the six refusals with their turn cost, Y); W1–W4 the well (`horse`, `HORSE`, `HHorse` no match, the `Horse` control) | **2 / 18** against the pre-PARITY1 production (`tools/a4_parity1_red_first.py`; the two GREEN are the controls C2 and W4) | **18 / 18** |
| `magic_parity`, `advanced_combat_parity` (drivers feed possession) | NEW-1 | RED on the unfixed native | GREEN |
| `combat_negate_parity` (new corpus) | P1c, draw by draw | RED on the unfixed native | GREEN |
| `a3_04b_perf` I1b | NEW-3 | RED (only I1b) | GREEN |
| reference vitest (new cases in 4 files) | possession, 0x1c, `negatesEnemyMagic`, `kernelStristr` | **6 / 90 RED** on HEAD's reference (`a4-parity1-ts-red-first.log`) | 90 / 90 |

### 9.13 Parity corpus diffs (`native/core/a4-parity1-corpus-diff.log`)

| Corpus | Change | Proof |
|---|---|---|
| `magic.txt` (25,088) | none | `--check` byte-identical |
| `combat.txt` (97,344) | none | `--check` byte-identical |
| `advanced-combat.txt` (125,440) | 2,794 rows | every changed row is a Negate scenario — variant 18 (`timeSpell 'N'`) or spell 32 (In An); **0 rows outside**, 0 scenario keys changed |
| `combat-negate.txt` (new, 56,160) | vs a corpus generated from the **unfixed** reference: 16,256 rows | only 'N' and 0x1c rows change (none of the no-spell rows); gated-ability texts under 'N' / 0x1c: 86 / 87 → 0 / 0, without a spell 213 → 213 |
| live `gameplay_parity` (4,802 driver rows) | 9 rows | 4 wishes (D-50), 4 combats reading scroll 3 (In An → 'N', D-80), 1 Use crown in combat (D-79 / D-80) |
| live `quest_parity` (5,377) | 6 rows | all Use crown: only `timeSpell` (→ 0x1c) and `timeSpellTurns` (→ 255) |

### 9.14 Mutations

`tools/a4_parity1_mutation_check.py`: 31 mutants, each one production edit (24 native, 7 in the reference). A native mutant is rebuilt and checked against `magic_parity`, `advanced_combat_parity`, `combat_parity`, `combat_negate_parity`, `ui_session`, `a4_parity1_runtime`, `gameplay_parity`, `quest_parity` and `a3_04b_perf`; a reference mutant against the touched vitest suites, the four corpus generators under `--check` and the live `gameplay_parity` / `quest_parity`. **31 / 31 killed**, 0 survived, 0 invalid; the restored tree GREEN on both sides (`native/core/a4-parity1-mutation.log`).

| Fix | Mutants | Killed by |
|---|---|---|
| NEW-1 | N1 the world gate reads the worn flag; N2 the arena gate does; N3 the palace absorbs whatever is carried | `magic_parity` + runtime C1; `advanced_combat_parity`; `magic_parity` + C1 |
| P1b | N4 Use crown toggles the flag again; N5 the worn crown lasts 20 turns; N6 it writes the amulet's 0x0e | runtime C3 / C4 + live `gameplay_parity` / `quest_parity`; runtime + `quest_parity`; runtime |
| P1c | N7 / N9 / N10 each gate removed; N8 the projectile gate before the roll; N11 / N12 only 'N' / only 0x1c | `combat_negate_parity` (N7–N10), `advanced_combat_parity`, live `gameplay_parity` |
| NEW-2 | N13 M is Cast again; N14 N is "What?"; N15 / N20 B / F without the turn; N16 P with one; N17 / N19 / N21 T / X / E's strings; N18 the core refuses Yell | `a4_parity1_runtime` (and `ui_session` for N13) |
| D-50 | N22 case-sensitive again; N23 a plain substring search (the skip dropped) | runtime W1 / W2 + live `gameplay_parity`; runtime W3 |
| NEW-3 | N24 the fragment entry removed | `a3_04b_perf` I1b |
| reference | T1 the world gate on `wornCrown`; T2 Use crown sets the flag; T3 only 'N'; T4 / T5 / T6 each gate removed; T7 case-sensitive | vitest + `generate-magic --check`; vitest + live parity; the negate / advanced corpora `--check` + live parity; vitest + live parity |

Not mutated, declared: two details of 0x6f1e cannot be observed on the device — the fold's threshold (0x60 itself) and the bit-7 strip — because the six needles are letters and the keyboard types ASCII; the reference's unit test pins both.

### 9.15 Host results

| Run | Result | Log |
|---|---|---|
| Baseline (a fresh build of the UI4/hf1 tree, outside the repository) | **178 / 178**, 157.16 s | `C:\dev\parity1-scratch\ctest.log` (scratch; numbers recorded here) |
| RED-first, parity drivers against the unfixed native | `magic_parity`, `advanced_combat_parity`, `combat_negate_parity` RED; `combat_parity` (control) GREEN | `a4-parity1-red-p1-parity.log` |
| GREEN, the same set + the four drift checks | 8 / 8 | `a4-parity1-green-p1-parity.log` |
| RED-first, `a4_parity1_runtime` (pre-PARITY1 production) | **2 / 18** (the controls C2, W4) | `a4-parity1-red-first.log` |
| GREEN, `a4_parity1_runtime` | **18 / 18** | `a4-parity1-runtime-green.log` |
| `a3_04b_perf` I1b | RED → GREEN | `a4-parity1-red-new3-i1b.log`, `a4-parity1-green-new3-i1b.log` |
| First full run | 180 / 181: `ui_session` (the dungeon M assertion, changed on purpose, §9.11) | `a4-parity1-ctest-pass1.log` |
| **Full suite, final tree** | **181 / 181**, serial, 158.26 s; host build 0 warnings. 181 = 178 + `combat_negate_parity`, `typescript_combat_negate_fixture_drift`, `a4_parity1_runtime`. The SAVE (A4-SAVE1/2/3, A4-UI3, A3-04G, Batch 28), combat (`combat_parity`, `advanced_combat_parity`, real arenas, A3-HF1/HF3), dungeon, UI (A4-UI1 – UI4), END1 and audio (A3-01 – A3-05) groups are inside it, all GREEN | `a4-parity1-ctest.log` |
| Reference | `tsc --noEmit` clean; vitest 7,735 tests (+7), assertion-level FAIL set **identical** to the A3-HF10 baseline (97); reference RED-first 6 / 90 → 90 / 90 on the touched files | `a4-parity1-vitest.log`, `a4-parity1-ts-red-first.log` |

### 9.16 Firmware and packs

Built from scratch in a new directory, `build-a4-parity1` (`idf.py --no-ccache -B build-a4-parity1 reconfigure`, `ninja -C build-a4-parity1 -j 4 all` 1180 / 1180 at the first attempt, `python package_launcher.py --build-dir build-a4-parity1`; `native/core/a4-parity1-{idf-export,fw-configure,fw-build,package}.log`). No project warnings: the only warning lines are ESP-IDF's "nearly full (1 % free)" notice and its `component_validation` notices.

- **Launcher:** `native/targets/tdeck/build-a4-parity1/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-parity1-Debug-Launcher.bin`, byte-identical to `build-a4-parity1/openu5_tdeck.bin`.
- **SHA-256** `72fc1aeebf37db8ca5761cb24fc62ae5689683db8bc462ccc3bf40bd4a1dd306`.
- **Embedded** (read back): `FW 4.0.0-alpha4-parity1-debug`, `Git e53741b23fbc-dirty` (`a4-parity1-image-identity.log`).
- **Size: 1,033,232 B (`0xfc410`), +704 B** against the hf1 image (`0xfc150`), inside the batch's ~1 KB budget. **15,344 B (1.5 %) of the 1 MiB app partition free** (was 16,048 B).
- **By section** against the hf1 image (`a4-parity1-fw-size-diff.log`): flash `.text` +600, `.rodata` +64; DIRAM `.text` +36 (`speaker_segment_frames`, now in IRAM); internal `.data` / `.bss`, IRAM and PSRAM unchanged.
- **ELF guards** (`a4-parity1-elf-checks.log`): `a3_04a_hotpath_check` GREEN; **`a3_04b_iram_check` GREEN** again (only the three by-design per-event / one-time functions stay in flash); `a3_04f_image_check` GREEN.
- **Packs unchanged, no SD recopy:** resource 2,266,819 B `85b38994…d01e`; tiles 132,284 B `6eb001ed…`; audio 56,148 B `28c1533b…`. No save-format change: the worn crown rides the existing time-spell fields (SAVED.GAM 0x2D4 / 0x2E8); cross-image loading was not tested.
- **The hf1 image is untouched:** `1d57efcefec3eec385a36b753f1e57a4d4825d3b03ab3726f30ab14737be3a44`, re-hashed after this build.
- **Not flashed.**

### 9.17 What remains before an Alpha 4 RC

**No known software blocker remains** for an Alpha 4 RC: the four gameplay defects the audit found (D-78 – D-81) and the IRAM regression are fixed and host-proven; D-50 is fixed; D-82 is a queued, non-blocking divergence.

What stands between this tree and an RC:

1. ~~**The UI4/PRES1 hf1 retest** (§8.22.6, on the hf1 image as planned), then UI4/PRES1's own commit and closeout.~~ **Done 2026-10-01:** retest PASS (§8.22.8); UI4/PRES1 committed alone as `48bd39ef`; PARITY1 restored on top and verified (§9.20).
2. **PARITY1's commit** (software-only, hardware pending).
3. **The RC image** built after both commits (`4.0.0-alpha4-rc1-debug`, a clean committed tree so `Git` names the commit), and its RC record (an `ALPHA4.md`, the Launcher table).
4. **One consolidated hardware session** on that image (§9.18).

Hardware-validation debt it settles (none of it is raised here without evidence):

| Item | Class | Where |
|---|---|---|
| A4-SAVE2 (§4.11), A4-UI3 (§6.14 A) | tested indirectly (Continue, Alt+S / Alt+L and the slot rows ran in the END1 and UI4 sessions) but never recorded | RC part 4 |
| A4-SAVE3 on the device (§6.14 B steps 1–7) | never tested | RC part 5 |
| A4-SAVE3's real-DOS round trip (§5.15 B / C) | never tested; needs a DOS install | RC part 5, optional — without it the RC notes must say the PC bridge is host-validated only |
| UI4/PRES1: the console height, D-67's bursts (and the shard route) | **hardware PASS** 2026-10-01 (§8.22.8) | closed |
| UI4/PRES1's other rows (D-12, D-48, D-53, D-68, D-69, D-71, D-72, D-74, D-75, D-76, D-77) | recorded hardware PASS at UI4's closeout (§8.22.8), except D-74 and the arena's single `Cast...` (optional steps, not reported) | RC part 2, optional |
| PARITY1: D-50, D-78 – D-81 | never tested | RC part 3 |
| A4-END1's dead-companion revival | not physically tested (no Developer kill) | optional; automated evidence stands |
| The Alpha 4 heap capture | owed: internal `.data` + `.bss` grew 1,104 B across Alpha 4 (A3 trigger 6, cumulative) and an RC needs its capture (trigger 7) | RC part 6 |
| SAVE2's `SAVE_CATALOG` cold / warm times | never measured | RC part 4 (serial) |

Category D (no physical run owed): H-51, H-84, H-128, H-129, H-136 / H-138 (superseded by H-201), H-139 / H-140 (known D-8), H-123 (certified by `quest_parity`).

### 9.18 The consolidated Alpha 4 RC hardware checklist (proposed)

One session, about 2 hours (plus an optional DOS round trip), on **the RC image** (built after the UI4/PRES1 and PARITY1 commits). Parts 1 and 3 can also be run on this batch's image if PARITY1 is to be seen before the RC is built. Every Developer value applies only on **Enter**; Mic while it is open cancels it (§8.22.8). Serial capture from power-on through part 6 (`python -m esp_idf_monitor -p COMx -b 115200 --no-reset 2>&1 | Tee-Object -FilePath a4-rc1-capture.log`; afterwards `python native/core/tools/a3_04g_hw_closeout.py a4-rc1-capture.log`).

**0. Before anything.** Copy the SD card's whole `ultima5/` folder to a PC (`saves/`, `settings.json`, the packs) and keep it. The packs stay as for A4-END1 (resource `85b38994…`, tiles `6eb001ed…`, audio `28c1533b…`); nothing to recopy. Flash the RC image. The identity screen must show its `FW` and `Git`, `RES … 2266819B` and `ASSET … 132284B`. **Stop if not.**

**1. Boot and title.** Splash "Alpha 4"; the main menu's footer names `J C T U A R S P`; Small text legible; Journey Onward → Continue loads your journey with its music.

**2. A4-UI4/PRES1.** Nothing owed: the hf1 retest passed (§8.22.8). Optional, D-74: with a low-health party (Alt+D → Party size 1, Preset: Low health/status), lose a fight: `BATTLE IS LOST!` appears **once**; win one: `VICTORY!` once, not again as the party walks off the board. In any fight, **C** on a member's turn: **one** `►Cast...` row.

**3. A4-PARITY1.**
- 3.1 **The crown carried, not worn** (D-78). Alt+D → Quest Items → **Crown of Lord British: On**. Alt+D → Inventory → Inventory index **4** → Spell quantity **3** (Mani). Stats → Current MP **30**. Teleport → **Palace of Blackthorn**, default entrance On. **C** → Mani → the Avatar: `Success!` — **not** `Absorbed!`.
- 3.2 **Control.** Quest Items → Crown **Off**; **C** → Mani: `Absorbed!`, no MP spent. Crown back **On**.
- 3.3 **Wearing it** (D-79). **U** → Crown: `Thou dost don the Crown of Lord British...`. **Alt+S**, walk a few steps, **Alt+L**. **U** → Crown: `Removed!` (it was still worn after the load). **U** → Crown again to wear it.
- 3.4 **Optional, Negate in a fight** (D-80): with the crown worn, or after casting In An, fight a spell-casting monster: it fires no magic missile; a teleporting one does not teleport. (Draw-by-draw host evidence: `combat-negate.txt`.)
- 3.5 **The dungeon keys** (D-81). Teleport → **Deceit**, default entrance On. Note the HUD clock.
  - **M**: `Mix Reagents` and the spell list (not `Cast...`). Mic.
  - **N**: the party picker (New order). Mic.
  - **B**: `Board`, `Not here!`. **E**: `Enter what?`. **F**: `Fire-What?`. **T**: `Talk-Funny, no response!`. **X**: `X-it what?` — each advances the clock.
  - **P**: `Push`, `Not here!` — the clock does not move.
  - **Y**: `Yell what?`; type `ABC`, Enter: `No effect!`; the clock does not move.
- 3.6 **The well** (D-50). Teleport → **Paws**, X **4**, Y **21**, default entrance **Off** (the well is the cell to the west). **L**, roll west: `Drop a coin?` → **Y** → `Thy wish?` → type `horse` (lower case), Enter: `Poof!` and a horse appears east of the party; gold −1.

**4. A4-SAVE2 + A4-UI3** (from §4.11 and §6.14 A, condensed; the card backup from step 0 is the safety net).
- 4.1 Journey Onward: `Latest: Slot N, …`; Load Game: two-row slots, `LATEST` on N, empty slots `EMPTY`.
- 4.2 Three places A, B, C saved into Slots 1, 2, 3 (an occupied slot asks `Overwrite Slot N?` with **No, keep it** first; No → `Slot N kept`; Yes → the menu closes, `Save complete: Slot N`).
- 4.3 Load each slot in game: the place and party match; `CURRENT` follows the load.
- 4.4 Return to Title: the footer names the slot saved last; Continue loads it. Alt+S then Alt+L stay in the current slot.
- 4.5 Power-cycle: the same three rows; each loads its own state.
- 4.6 Create New Character with every slot used: `Replace Slot N?`, No → back to the list, nothing changed.
- 4.7 Text Large: two rows per slot, nothing clipped; back to Medium.
- 4.8 Optional (PC): delete the newest `.json` of one slot's pair: the row reads `RECOVERED`, Enter loads the save before, `Recovered previous save (Slot N)`.
- 4.9 Serial: the first and a second System Menu open's `SAVE_CATALOG … total_us=` (cold / warm).

**5. A4-SAVE3, the PC bridge** (§6.14 B).
- 5.1 With DOS `SAVED.GAM` + `SAVED.OOL` (your own, or `original/u5/ultima5/`) in `ultima5/import/`: title → **P** → `PC save: <Avatar>, <place>` → Import into an EMPTY slot → `Imported into Slot N. The PC files are kept`. Load it: party, place, gold, food, date and any ship as in DOS (townsfolk at their posts for the hour). Save, power-cycle, load again.
- 5.2 Change something (odd gold), save, PC Save Transfer → Export → `Slot N written to /ultima5/export/slotN`; on the PC the folder holds `SAVED.GAM` (4,192 B), `SAVED.OOL` (512 B), `EXPORT.TXT`.
- 5.3 Optional, the gold standard: those files in a real DOS Ultima V → Journey Onward shows the changed state; Quit & Save works. Without it, the RC notes say "PC bridge host-validated only".

**6. Smoke and heap** (as Phase A3-RC1, condensed): walk / bump with music; Settings volumes and the two mutes; one fight to victory (hit cues, one kill burst, the fanfare); Mix with How much? 1; Alt+S / Alt+L; the System Menu ten times; New Journey and back to Continue; Developer → Diagnostics → Audio/render stats `und=0 hw=0 miss=0`. **Stop the capture.**

**7. Power-cycle Continue**: the identity as in step 0; Continue loads; input works. Restore the card from step 0 if wanted.

**8. Optional: the END1 revival** — only if a companion has fallen in play and stays dead into the Doom route (§7.15 B.2; the Developer has no kill shortcut, and Preset: Endgame's Max Party touches the roster, so take the route without it). Otherwise its automated evidence stands.

**PASS** needs parts 1–7 with no crash, reset, watchdog, lock, stale-resource refusal or input-mode corruption; Save, Load and Continue restore the saved state; music and effects continuous; the heap capture explained (A3 §28.16 triggers; a placement-only trigger does not block). **Report:** the `FW` / `Git` lines, PASS / FAIL per step, the capture, photos of anything odd.

### 9.19 Status

| Axis | State |
|---|---|
| Investigation and plan | delivered before production code (the PARITY1 report); decisions §9.2 |
| NEW-1 / D-78, P1b / D-79, P1c / D-80, NEW-2 / D-81, D-50 | **software fixed**, reference first, host-proven |
| NEW-3 (IRAM guard) | **fixed**: `a3_04b_iram_check` GREEN on the image |
| NEW-4 / D-82 | classified, **deferred** (§9.8) |
| Ledger | A-19, A-20 added; A-7 revised; stale rows reconciled; D-82 queued |
| Host | **181 / 181** serial; runtime 2 / 18 → 18 / 18; mutations **31 / 31**; corpora diffs exactly the predicted rows; reference vitest FAIL set identical |
| Firmware | `0xfc410` (1,033,232 B), **15,344 B free**, SHA-256 `72fc1aee…d306`, `FW 4.0.0-alpha4-parity1-debug`; packs unchanged |
| Hardware | **pending** — the consolidated RC session (§9.18) |
| A4-UI4/PRES1 | **closed**: hf1 retest PASS (§8.22.8); committed alone as `48bd39ef`; its hf1 image untouched |
| Closed (commit, tag, push, flash) | **committed** on top of `48bd39ef` after the revalidation of §9.20; not tagged, pushed or flashed |

*A4-CLOSE1 (2026-10-02): the §9.18 session was never run on the RC1 image (`aae348ac`, SHA-256 `67100a51…145a`). RC1 is superseded as the hardware candidate by RC2 (§14.9), which carries everything since (A4-ENH1, A4-ENH2, A4-FLASH1, A4-POLISH3). §14.7 is §9.18 retargeted to RC2, with the parts already settled struck. D-50 and D-78 – D-81 stay hardware pending until that session.*

### 9.20 Separation from A4-UI4/PRES1 and revalidation (2026-10-01)

PARITY1 was developed on the uncommitted UI4/PRES1 + hf1 tree. UI4/PRES1 had to be committed on its own first, after its hardware retest. This is how the two were split and joined again.

**Preserved before anything moved.** The finished PARITY1 tree (153 changed or new files) was saved three ways, outside the repository:
- a tar with a SHA-256 manifest;
- a byte-checked directory copy;
- a snapshot commit on the local branch `preserve/a4-parity1-wip`.

`parity1-only.patch` was checked against it: all 38 sections match, and the live tree was exactly the UI4 baseline + those 38 files + 23 PARITY1 logs.

**UI4/PRES1 alone.**
- The tree went back to the copy kept before PARITY1 began. The check was byte-for-byte: the same 100 files, the tracked diff identical, the untracked hashes equal.
- The 28 PARITY1-only files were moved out, not deleted.
- Suite 178 / 178 serial, then the hardware retest (§8.22.8).
- UI4/PRES1 committed as `48bd39ef`, with no PARITY1 file in it.

**PARITY1 restored on `48bd39ef`.**
- 150 files copied back from the preserved copy; each is byte-identical to the finished PARITY1 tree.
- The three shared documents are PARITY1's own, with UI4's closeout edits re-applied by the same script that made them. Proof: applied to the UI4 baseline, that script reproduces the committed closeout documents byte for byte, and the restored documents differ from PARITY1's by exactly those edits.
- The change set against `48bd39ef` is exactly PARITY1: 38 patch files + 23 logs.
- This section and the RC lines above are the only new text.

**Revalidation on the restored tree.**
- **Host:** **181 / 181** serial, 158.53 s, on a fresh `build-a4-parity1-commit` (0 project warnings; `a4-parity1-commit-ctest.log`), and again after these records (148.24 s, `a4-parity1-commit-final-ctest.log`). That includes `a4_parity1_runtime`, `combat_negate_parity`, `a3_04b_perf`, `magic_parity`, `advanced_combat_parity`, `gameplay_parity`, `quest_parity` and every drift test.
- **Reference:** `tsc --noEmit` clean; the four touched test files 90 / 90; full vitest 7,735, assertion-level FAIL set **identical** to this batch's recorded run (97, all pre-existing) (`a4-parity1-commit-vitest.log`).
- **Corpora:**
  - `magic.txt` and `combat.txt` are unchanged against `48bd39ef`;
  - `advanced-combat.txt` and `combat-negate.txt` are byte-identical to the batch's, so §9.13's row analysis stands;
  - the four combat / magic drift tests are GREEN.
- **Mutations:** not re-run. The restored production and test files are byte-identical to the ones the 31 / 31 pass ran against.
- **Firmware:** a fresh `--no-ccache` build of the restored tree, `build-a4-parity1-precommit` (`a4-parity1-precommit-*.log`):
  - 1,033,232 B (`0xfc410`), 15,344 B free; the section diff against `build-a4-parity1` is empty;
  - 85 bytes differ from that image: the `Git` string (`48bd39effb0b-dirty`), the build times and the SHA;
  - all three ELF guards GREEN;
  - packs unchanged (`85b38994…`, `6eb001ed…`, `28c1533b…`);
  - the hf1 and the first PARITY1 images re-hashed unchanged.
- **NEW-4 / D-82:** still deferred and queued (§9.8).

## 10. A4-ENH1 — trackball, Developer entry, cheats and difficulty (2026-10-01)

One integrated enhancement pass on top of the Alpha 4 RC1 commit (`aae348ac`), in seven commits. **Status: implemented and host-verified; firmware-build verified; hardware validation PENDING; every tuning value PROVISIONAL.** Nothing here is declared closed until the device run of §10.13.

| Commit | Part |
|---|---|
| `fef1f9fe` | 0. The Original-mode preservation goldens, recorded on the unmodified tree |
| `07a26014` | 1. Trackball click = instant WASD toggle; trackball instrumentation |
| `7ee18683` | 2. Trackball speed levels (a pulse accumulator) |
| `cf669d36` | 3. The Developer menu leaves the ordinary menus; Alt+D everywhere |
| `ae9c3eb9` | 4. Player cheats |
| `abe4f14d` | 5. Difficulty presets and scaled XP |
| (this commit) | 6. Preservation and mutation evidence, documentation, version `4.0.0-alpha4-enh1-debug` |

### 10.1 Baseline

- `main` at `aae348ac` (Alpha 4 RC1, hardware session §9.18 pending). The tree was clean except the eight untracked `native/core/a4-rc1-*.log` files, which belong to the RC hardware session; they were left untouched and are still untracked.
- Fresh host build `build-a4-enh1-baseline`: **181 / 181** serial, 157.25 s (`a4-enh1-baseline-ctest.log`).
- The RC1 image (`build-a4-rc1`, SHA-256 `67100a51…145a`) was not rebuilt or touched.

### 10.2 The trackball pipeline, traced before any change

| Question | Answer (pre-A4-ENH1 code) |
|---|---|
| Where do X/Y deltas come from? | There are none. The ball has four direction pins (up GPIO 3, down 15, left 1, right 2); each pulses once per few degrees of roll. The press switch (GPIO 0) was never read. |
| How often sampled / reported? | `tdeck_input.cpp`: a falling-edge GPIO interrupt wakes the input task (priority 4, core 0, 1-tick = 10 ms fallback poll), which samples the pins and queues **one `RawInputEvent` per falling edge**, timestamped (µs), into a 64-deep queue. The game thread drains it every loop. |
| Accumulated? | No. |
| Does each raw event move? | Yes: every pulse that passed the filter was one `Direction` action — one move, one turn, one cursor row. |
| Where was "sensitivity"? | `InputController::normalize` (device semantic layer): a **same-direction minimum gap** of 1,200,000 / percent µs — 48 ms at 25 %, 12 ms at 100 %, 4 ms at 300 %. |
| Threshold / deadzone | None beyond that gap. |
| Acceleration | None. |
| Repeat timing | None; the pulses themselves drive everything. |
| Per report or per distance? | Per report (per pulse). |
| Diagonals | Each axis's pulses step independently: a staircase at the full pulse rate. |
| WASD | Keyboard W/A/S/D become the same `Direction` actions in movement contexts (exploration, dungeon, combat, targeting, direction-accepting pickers); untouched by the trackball filter. Toggled only by a 1.1 s Mic hold. |

**Why every setting felt fast.** The percentage only capped the rate. Any roll slower than about 21 pulses a second stepped on *every* pulse at *every* setting, 25 % included. A flick also queued its pulses: every one was a move, so the party kept moving after the ball stopped. The fix had to change the model, not the numbers.

### 10.3 The click (commit 1)

- `tdeck_input.cpp` reads GPIO 0 (`kTrackballClick`: LilyGO's `BOARD_BOOT_PIN`, Meshtastic's `TB_PRESS`) on **both** edges and queues `RawInputKind::TrackballClick` (appended) with Pressed / Released.
- `TrackballClickFilter` (`input_controller.cpp`): a press is accepted **on its edge** — no hold, no delay, no double-click. A press within 30 ms of a release is contact bounce; a second toggle needs 150 ms since the last; a held press never repeats.
- `UiInputAdapter::translate` toggles Movement (WASD) Mode and returns `DeviceShortcut::MovementModeToggled`, leaving the caller's action untouched (a default `UiAction` is Confirm, so even "clearing" it would read as Enter).
- `AlphaRuntime::handle_input_event` handles the toggle beside the A3-05 mutes, before any screen routes input: never a Confirm, a menu action, a scene key or a command. Feedback: **"WASD Mode: ON" / "WASD Mode: OFF"** in the transcript (the Mic hold now says the same); the HUD's `MOVE` marker as before.
- Roll pulses within **60 ms** of either click edge are dropped (the ball rocking under the finger), and a partial step is forgotten.
- The live toggle is never written to `settings.json`; "Movement default" stays the Settings row's boot value. (The title used to save the toggle and then reload the frontend's old copy, so the live mode reverted at once — removed.)

### 10.4 The speed model (commit 2)

`InputController::normalize` now: click guard → same-direction bounce window → **step gap** (pulses inside it are *dropped*, not banked: when the ball stops, the party stops) → a **per-axis accumulator**: N pulses on one axis make one step, the remainder is kept; a pulse the other way clears the count (no unwinding); **400 ms** without a pulse on that axis clears it too (a tiny roll moves nothing). Axes count separately, so a diagonal alternates its two directions. No acceleration.

The Settings row (title and System Menu) is **"Trackball speed: N/10"**; its footer says "Left/right: 1 slow - 10 fast; Mic saves". The table, `kTrackballLevels` — **PROVISIONAL, to be tuned on hardware**:

| Speed | Pulses / step | Step gap | Bounce window | Note |
|---|---|---|---|---|
| 1 | 8 | 250 ms | 4 ms | slowest |
| 2 | 6 | 200 ms | 4 ms | |
| 3 | 5 | 160 ms | 4 ms | |
| 4 | 4 | 130 ms | 4 ms | |
| **5** | **3** | **100 ms** | 4 ms | **default** |
| 6 | 2 | 80 ms | 4 ms | |
| 7 | 1 | 50 ms | 12 ms | close to the old 25 % minimum |
| 8 | 1 | 30 ms | 12 ms | |
| 9 | 1 | 20 ms | 12 ms | |
| 10 | 1 | none | 12 ms | exactly the old 100 % path |

Host measurement of the model (`input_regression` S7, one second of pulses every 10 ms): steps 3 / 4 / 5 / 7 / 9 / 11 / 17 / 25 / 50 / 50 for speeds 1–10 — monotone, and low / medium / high clearly apart. A slow roll (24 pulses, one every 120 ms): 2 / 3 / 4 / 5 / 8 / 12 / 24 / 24 / 24 / 24 steps — speed 1 steps once per ten pulses (eight, plus the two its 250 ms gap drops), speeds 7–10 on every pulse. On the device the pulses per roll are what the Developer report shows (§10.13 B).

**Settings.** `settings.json` gains the optional key `trackballSpeed` (1..10). A file without it — every file written before A4-ENH1 — takes the default **whatever its old percentage said** (the model changed; an old 25 % would otherwise map to the same too-fast feel). `trackballResponsiveness` is still written, unchanged, so an older firmware still reads the file. The version stays 1.

**Host fixture.** Every older host test drives one raw pulse per intended step — the old contract, which is speed 10 now. The fixture sets speed 10 before `load_device_settings()`; tests that want the device default set it through the device's own Settings row (`a4_enh1_runtime` S1–S7). Tests that boot from a literal `settings.json` say speed 10 explicitly. That is the one place the host differs from a fresh device, and it is pinned by its own tests.

**Diagnostics** (commit 1, extended in 2): Developer > Diagnostics > **"Trackball stats (live)"** (inserted directly above "Probe: SD diag logging"; every older row keeps its place counted from the end) shows the speed's row, pulses and steps per direction, drops by reason (bounce, step gap, after-click), cleared partial steps (idle, reversal) and the count held now on each axis, a same-direction gap histogram (<4 … ≥128 ms), clicks (pressed / toggled / bounce) and the last six rolls ("R9>3 180 ms, min gap 8 ms, speed 5" = nine pulses right, three steps). Reading it starts a new window. One `TRACKBALL_GESTURE` serial line per finished roll; no per-pulse logging (the existing `INPUT_TRACE` lines are unchanged).

### 10.5 The Developer entry (commit 3)

- Gone from ordinary navigation: the title's "Developer" row, the System Menu's "Developer" row, and both Settings pages' "Developer: Visible/Hidden" switch — whatever an old `developerToolsVisible` says (still read and written for compatibility; nothing shows it).
- **Alt+D** opens the Developer menu in the game, as before, and now also on the title, the startup intro, the attract loop and the main menu (`FrontendSession::request_developer_tools()`: the same `OpenDeveloperTools` intent the hidden row raised, so its no-storage entry is kept). The main menu loses its 'D' hotkey. Every Developer function, diagnostic and preset is unchanged; no player cheat is in it.

### 10.6 The cheats (commit 4)

System Menu > **Cheats** (after Settings and Difficulty; Return to Title stays last): **God Mode** (toggle), **Heal Party**, **Cure Party**, **Add Gold** (left/right: +10 / +100 / +1000), **Max Gold**. The footer says what a row does, then the result; an applied cheat is also printed in the transcript; the page stays open. The subtitle says whether this journey has used one.

Architecture (`openu5/enhanced.h`, `enhanced.cpp`):
- `EnhancedState` on `GameState`: `god_mode`, `cheats_used` (a bit per cheat ever applied — **support metadata only**, nothing reads it in play) and `difficulty` (§10.7).
- `apply_cheat()` is the **one entry point**: each cheat changes the game there, through the fields the game's own writers use. `CheatKind` is append-only (its value is its bit), so restore MP, revive, food, keys, torches, gems, reagents, equipment, teleport, no encounters / hunger / poison and quest items are each a case.
- Guards: Heal / Cure **refused in combat** (the arena holds its own HP copies and writes them back at the end); gold never passes **9999** (every gold writer's cap) and is never lowered (a Developer preset can leave more); a negative or absurd amount clamps; the dead stay dead (revive is not a cheat yet); Cure cures poison and sleep.
- **God Mode** is one predicate, `party_damage_blocked()`, asked by the five party HP-loss sites: combat `damage()`, `apply_damage()` (poison, starvation, fire and lava, quakes, traps, the Look sun), the chest trap, the ladder fall and the waterfall. The RNG draws still happen; only the HP write is skipped. **Unchanged under God Mode:** status deaths (an inn's poisoned sleeper), scripted deaths (the location-29 trapdoor), the poison tick's flash and sound (it ticks, it takes nothing).

### 10.7 The difficulty (commit 5)

A rules layer on top of the recreated game: plain numbers per preset (`GameplayRules`, `kGameplayRules` in `enhanced.cpp` — **one table to retune**), applied at five central hooks. No enemy, item or original table is edited.

| Hook (site) | Original | Relaxed | Easy |
|---|---|---|---|
| incoming damage — an enemy's hit on the party (combat `damage()`) | 100 % | **85 %** | **65 %** |
| outgoing damage — the party's hit on an enemy (combat `damage()`) | 100 % | 100 % | 100 % |
| XP per kill (combat `kill()`, the one award site) | 100 % | **150 %** | **200 %** |
| poison: 1 HP on every Nth turn (turn housekeeping) | every turn | **every 4th** | **every 10th** |
| meals that eat at 06 / 12 / 18 (turn housekeeping) | all | **75 %** | **50 %** |
| passed overworld spawn rolls that spawn (`world()`) | all | **90 %** | **75 %** |

All values are **PROVISIONAL** starting points from the brief's ranges; XP is not above 2× by default.

- **Rounding:** half up; a positive value never scales to 0 (a hit stays a hit, a kill is worth at least 1 XP) — with today's table that clamp is never reached (no preset is under 50 %), so it is defensive. COMBAT's kill-outright value 99 is kept, and scaling never manufactures it (a scaled 99 becomes 98).
- **XP:** the 1988 award `(hp >> 2) + 1` → the preset's share → the 9999 cap and the arena tally. Applied once.
- **Poison, meals, encounters use no new state.** Poison is thinned by the saved turn count (`turns_since_start % N`), meals by the calendar's meal number (Bresenham: exactly p of the meals, evenly spread, the same ones after any load), encounters by a fixed hash of the turn number (no RNG draw). Nothing can drift across save / load, rest, map changes or combat; a dialogue that resets the turn counter (the 100-turn effect) only moves the phase.
- **Scope, by design:** incoming scaling is enemy hits in the arena only — arena fields, starvation, traps, hazards and the ship's hull are unchanged. Dungeon wanderers and fixed rooms are unchanged (the encounter share is the overworld spawn gate). Death and resurrection penalties are unchanged.
- **UI:** System Menu > **Difficulty** (after Settings): Original / Relaxed / Easy, the cursor on the journey's own; each row's footer states what it changes ("Hits 85% XP 150% Food 75% Poison 1/4 Fights 90%"); Enter switches from the next turn on (footer + transcript "Difficulty: Easy"). A difficulty is not a cheat (no bit).

### 10.8 Save and settings

- **Per journey, in the save:** the state document gains `"enhanced": {"difficulty": "relaxed", "godMode": true, "cheatsUsed": 9}` **only when it differs from the defaults** (`save_core.cpp`), carried in the sidecar via `persistence.cpp` `extras[]` (the sidecar is a whitelist: a key missing from it is dropped at every save). Absent = Original, God Mode off, no cheats — every older save, every PC import (§5's rule: the bridge never reads or writes sidecar keys outside §5.3) and every journey that never touched them. A malformed value takes its default rather than refusing the save. Not in the `.GAM`.
- **Device-wide, in `settings.json`:** only the trackball speed (§10.4).

### 10.9 Preservation (Original + no cheats)

- **Goldens recorded on the unmodified tree before any change** (commit 0, `a4-enh1-golden-head.log`), using only pre-A4-ENH1 interfaces:
  - `a4_enh1_preservation`: 3,000 housekeeping turns (2,400 poison ticks, 45 meals, starvation, the regeneration ring), 4,000 spawn-gate rolls, a whole fight on the real combat engine (126 HP of enemy hits, 2 kills, 22 XP), and a default journey's saved document byte for byte plus its load-and-save-again;
  - `a4_enh1_preservation_runtime`: a 260-step night walk on the real overworld through the real `AlphaRuntime` (the live `world()` turn: 3 fights, poison) and Alt+S / Alt+L.
  
  After every later commit — every hook in place — both reproduce the recorded hashes **bit for bit**.
- `a4_enh1_rules` R1: every hook is the identity at Original over damage −5..300, XP, and 20,050 turns of poison, meals and spawns.
- The whole existing parity corpus (`gameplay_parity`, `quest_parity`, `combat_parity`, `advanced_combat_parity`, `magic_parity`, `turn_parity`, `travel_parity`, `persistence_parity`, the TypeScript drift tests) passes unchanged in every commit's suite.
- Mutations R1–R5 (a non-identity Original row) turn the goldens RED (§10.11).
- *Correction (A4-ENH2, 2026-10-01): the A1 golden never starves — its food never reaches 0 — so starvation was not covered here; A4-ENH2's B1 golden covers it (§11.10).*

### 10.10 Tests

- New: `a4_enh1_preservation`, `a4_enh1_preservation_runtime` (commit 0), `a4_enh1_runtime` (W1–W11 click, T1–T2 report, S1–S7 speed, D1–D5 Developer entry, C1–C11 cheats, R1–R6 difficulty incl. the live encounter thinning in lockstep with Original), `a4_enh1_rules` (C1–C15, P1–P6, R1–R11); `input_regression` gains the click (C1–C7), instrumentation and speed (S1–S7) blocks.
- **Changed on purpose:** the Diagnostics row count (`ui_debug_menu`, `a3_04e_pacing` U1/U2, `a3_04e_pacing_runtime` navigation); the trackball row and settings document (`frontend`, `a3_01_audio_contract` F5/F7/F10); the tests that drive one pulse per step now say speed 10 (`dungeon_input`, `dungeon_combat`, `a3_01/a3_03/a3_05/a4_ui2_death_music` settings documents, the host fixture); the Developer rows (`frontend`, `a4_ui4_presentation` F4b); the System Menu root (`a4_ui2_save_menu` M1/M2); the A4-UI1 goldens — settings ×2 (twice) and system-menu (twice), each with a pixel proof that only the intended rows changed (`a4-enh1-p{2,3,4,5}-golden-proof.log`, `tools/a4_enh1_png_diff.py`).
- **Found and fixed (pre-existing):** since A3-01 (`2f218808`) a `//` comment in the middle of a `frontend_test` line had silenced every System Menu check after it on that line; they run again (proven: a wrong expectation now fails at line 141).

### 10.11 Mutations

`tools/a4_enh1_mutation_check.py <build> [ids]` — 39 mutants, **39 killed**, 0 survived, 0 invalid (`a4-enh1-mutation.log`: 37 killed and 2 INVALID through the driver's own anchors -- one left a stray comment, one deleted a parameter's only use under `-Werror=unused-parameter`; `a4-enh1-mutation-rerun.log`: those two, fixed, killed). They cover the click (not handled, bounce re-toggling, routed as a key, no feedback, no jiggle guard), the speed (no accumulation, no step gap, no idle or reversal reset, the key not read back, the default row changed), the Developer entry (the row back, Alt+D dead on the title), the cheats (God Mode missing at apply_damage / combat / the chest trap, Heal in combat, the gold cap and floor, no cheats-used bit, the key off the sidecar whitelist or written for an Original journey, a load dropping God Mode, the page's Enter dead) and the difficulty (five non-identity Original rows, each hook removed, XP scaled twice, the meal thinning never skipping, the difficulty outside "is default", a load ignoring it, the page's Enter dead). Two notes: a 1 % change to Original's XP or hit share rounds away inside the golden fight (R1 and R3 were killed by `a4_enh1_rules` R1's identity sweep, not by the golden); and a "never scale a hit to 0" mutant is equivalent with today's table (no preset is under 50 %), so the clamp is defensive and was left out.

### 10.12 Host and firmware

- Serial suite per commit: 184 / 184 (commits 1–3), 185 / 185 (4–5), 185 / 185 on the final tree (161.31 s, `a4-enh1-final-ctest.log`; 0 project warnings).
- Firmware (1 MiB app partition):

| Image | Size | Free |
|---|---|---|
| RC1 (`aae348ac`) | `0xfc410` (1,033,232 B) | 15,344 B |
| commit 1 | `0xfd0c0` (1,036,480 B) | 12,096 B |
| commit 2 | `0xfd450` (1,037,392 B) | 11,184 B |
| commit 3 | `0xfd3a0` (1,037,216 B) | 11,360 B |
| commit 4 | `0xfdc60` (1,039,456 B) | 9,120 B |
| commit 5 | `0xfe4a0` (1,041,568 B) | 7,008 B |
| | commit 6, pre-commit (`4.0.0-alpha4-enh1-debug`) | `0xfe4a0` (1,041,568 B) | 7,008 B | |

  The pass costs about 8.3 KB; **7 KB of the partition remain**, worth knowing before the next feature. Packs unchanged (no SD recopy). Section diff against RC1 (`a4-enh1-p6-fw-size-diff.log`): flash `.text` +6,004 B, `.rodata` +2,336 B, internal `.bss` +464 B (the trackball counters and the six-roll ring inside the input adapter -- an RC heap capture should account for it), `.data` and IRAM unchanged. The three ELF guards are GREEN on it (`a4-enh1-p6-elf-checks.log`). The image to flash is built **after** this commit from a fresh `--no-ccache` directory, so its `Git` line names the commit; its path and SHA-256 go in `native/core/a4-enh1-image-*.log`, left untracked like RC1's until the hardware result is recorded.

### 10.13 Hardware checklist (A4-ENH1 image) — PENDING

On the ENH1 image (`native/targets/tdeck/build-a4-enh1-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-enh1-Debug-Launcher.bin`, identity screen `FW 4.0.0-alpha4-enh1-debug` and `Git <this commit>`; packs as for RC1, nothing to recopy). Back up the card's `ultima5/` first. Developer values apply on Enter. Report PASS / FAIL per line and anything odd; the tuning lines want an opinion, not a PASS.

**A. Trackball click**
1. In the world, click once: "WASD Mode: ON" at once, `MOVE` on the HUD, the party does not move, nothing opens.
2. Click again: "WASD Mode: OFF" at once.
3. Ten quick clicks (about 3 a second): exactly ten lines alternating ON / OFF — never two for one press.
4. Press and hold 3 s: one toggle, at the press, none at the release.
5. Click in the System Menu, a shop, a Look "Direction?" prompt, Z-stats and the title: no row selected, no prompt answered or cancelled, no command; the mode still toggles.
6. Click firmly while resting a finger on the ball: no step.

**B. Trackball speed** (Settings > "Trackball speed: N/10"; default 5). For each of speeds **1, 3, 5, 7, 10**: a tiny roll (a nudge), a normal roll, a fast flick, horizontal, vertical, diagonal, in the world and in a menu. Then Developer > Diagnostics > "Trackball stats (live)" once per speed and note the pulses-per-roll and the gap histogram.
7. Tiny roll: no move at speeds 1–6.
8. Normal roll: steady, not jumpy; the speeds feel clearly different.
9. Fast flick: a short burst, and nothing more once the ball stops (no "catch-up" moves).
10. Diagonal: alternating steps, no runaway in one axis.
11. Menus: the cursor follows a deliberate roll at every speed (say which speed you would choose).
12. Speed 10 feels like the RC1 image at 100 %; speed 7 like its 25 %.
13. Reboot: the chosen speed is kept. A card from before this image boots at speed 5.
14. WASD ON at speed 1: W/A/S/D still move at once, one step per key.

**C. Developer entry**
15. No "Developer" in the title menu, the System Menu or either Settings page.
16. Alt+D in the game opens the Developer menu; Alt+D on the title opens it too.

**D. Cheats** (System Menu > Cheats)
17. God Mode On; let monsters and a poison field hit the party: no HP lost; Off again: damage returns.
18. Heal Party with a hurt party: full HP. Cure Party with a poisoned member: cured.
19. Add Gold: +100, then right to +1000, Enter. Max Gold: 9999. At 9999 Add Gold says it is full.
20. During a fight: Heal Party says "Not during combat".
21. God Mode On, Alt+S, power-cycle, Continue: still On; the Cheats page says the journey has used cheats.

**E. Difficulty** (System Menu > Difficulty)
22. Original on an old save: everything as RC1 (a fight, a poisoned walk, a day's meals).
23. Relaxed / Easy: enemy hits visibly smaller; a kill's XP ×1.5 / ×2 (Z-stats before and after one kill).
24. Easy: a poisoned member loses 1 HP per 10 steps (Relaxed: per 4).
25. Easy: food drops at about half the rate over a game day (Relaxed: three quarters).
26. Easy at night in open country: noticeably fewer monsters appear (opinion).
27. Alt+S, power-cycle, Continue: the difficulty is kept; a save from RC1 loads as Original.

**PASS** needs A, C, D and the mechanical lines of B and E with no crash, lock, reset or input-mode corruption; the tuning opinions (B 8–12, E 23–26) decide the numbers of a follow-up, they do not fail this run.

### 10.14 Deferred and open

- **Tuning** of the speed table and the difficulty presets: hardware and play.
- More cheats (the list in §10.6) and per-hook toggles: the framework is ready, none is implemented.
- Difficulty for the dungeon wanderer, arena fields, traps, starvation and death penalties: deliberately out of scope (§10.7).
- God Mode does not stop status or scripted deaths (§10.6).
- Alt+S / Alt+L on the title still act as Enter on the selected row (a default `UiAction` is Confirm). Observed while tracing the click, pre-existing, not changed here.
- The RC1 hardware session (§9.18) is still owed; this image supersedes RC1 for testing only if the user decides so.

### 10.15 Status

| Axis | State |
|---|---|
| Click, speed, Developer entry, cheats, difficulty | **implemented, host-verified** |
| Original-mode preservation | **host-verified**: pre-change goldens bit-identical; parity corpus unchanged |
| Firmware | **build-verified** (pre-commit `0xfe4a0`, 7,008 B free, ELF guards GREEN; the flashable image is built after this commit) |
| Tuning values | **PROVISIONAL** |
| Hardware | **PENDING** (§10.13) |
| Commit / tag / push | committed; not tagged, not pushed, not flashed |

*Update (A4-ENH2, 2026-10-01): the user reports that the A4-ENH1 image passed real-device testing and that the trackball feels substantially better (§11.1); no per-line results for §10.13 were given.*

*Update (A4-CLOSE1, 2026-10-02): hardware **PASS**, recorded in §14.2. The click toggles WASD Mode at once; the speed model feels substantially better and the trackball's behaviour is acceptable, so `kTrackballLevels` is accepted for Alpha 4 as it stands (still adjustable later). The cheats work on hardware; the difficulty presets are accepted as implemented (superseded by A4-ENH2's rows). The PENDING in the §10.13 heading and the table above is the status as this batch wrote it.*

## 11. A4-ENH2 — Custom difficulty, tuned presets, dungeon and starvation rules, more cheats (2026-10-01)

The follow-up to A4-ENH1 (§10), on top of its last commit (`5e0a16eb`), in eight commits. It extends the enhancement framework without touching the Original game. **Status: implemented and host-verified; firmware-build verified; hardware validation PENDING; every tuning value PROVISIONAL; subjective balance not final.** Nothing here is declared closed until the device run of §11.14.

| Commit | Part |
|---|---|
| `ad3d8258` | 0. Original-mode preservation goldens for the new hook sites, recorded on the unmodified tree |
| `3a8ea537` | 1. Custom difficulty: one precedence function, a choice table, the journey's own values |
| `d774ae54` | 2. Easy retuned: outgoing damage 120 %, overworld encounters 65 % |
| `a4810fdb` | 3. Dungeon wanderers and starvation severity: two central hooks, after the 1988 draws |
| `48241296` | 4. Party and inventory cheats in groups; God Mode at the naval OUCH |
| `6361bedd` | 5. World cheats (No Hunger, No Poison Damage, Disable Random Encounters) and their precedence |
| `c8e873d9` | 6. Review fix: Revive Party raises HP and MP but never lowers them |
| (this commit) | 7. Mutation and review evidence, documentation, version `4.0.0-alpha4-enh2-debug` |

### 11.1 Baseline

- `main` at `5e0a16eb` (A4-ENH1 commit 6). The A4-ENH1 commits and their records (§10, ledger E-8 – E-11, the audit's A4-ENH1 section) were present. The tree was clean except fourteen untracked logs (`native/core/a4-rc1-*.log`, `native/core/a4-enh1-image-*.log`), which belong to the RC1 and A4-ENH1 hardware sessions; they were left untouched and are still untracked.
- **A4-ENH1 on hardware (user report, 2026-10-01):** the A4-ENH1 image passed the full host suite and real-device testing, and the trackball now feels substantially better. No per-line results for §10.13 were given; they are recorded here as reported, not line by line.
- Fresh host build `build-a4-enh2-baseline`: **185 / 185** serial, 193.80 s (`a4-enh2-baseline-ctest.log`; 0 project warnings).
- Firmware at HEAD rebuilt in a fresh `build-a4-enh2`: `0xfe4a0` (1,041,568 B), **7,008 B free** in the 1 MiB app partition — the A4-ENH1 image's size exactly (`a4-enh2-p0-fw-build.log`). Flash was treated as the binding constraint throughout (§11.13).

### 11.2 Investigation (before any production change)

Nine read-only investigations (starvation, death and resurrection, dungeon wanderers, the cheat data model, the menu and its flash cost, the save format, the existing tests and mutation tooling, firmware sizing, the combat hooks), the four feasibility claims each re-read by an independent skeptic against the code and the 1988 binaries (`re/tools/dis16.py`). The answers that decided the design:

| Question | Answer | Hook? |
|---|---|---|
| Where does starvation happen? | Only in turn housekeeping: `turn.cpp` (ULTIMA.EXE `0x2B5D`: food 0 at an hour change → "Starving!" and `rand(1,8)` per member through `party_random_damage`, `0x2AA8`). All four 1988 housekeeping callers (TOWN `0x10D0`, MAINOUT `0x0CD3`, DUNGEON `0x0E22`, CMDS `0x0671`) reach it; camp, inn, jail and combat never do. No consequence beyond HP (and death). | **Yes** — at that call site, after the draw; fire, quakes and the cactus share `party_random_damage` and must not change |
| How are dungeon wanderers spawned? | No per-turn roll. One wanderer per session, re-armed by `dungeon_respawn()` (DUNGEON `0x0134`) at entry, a floor change, a pit fall and after a corridor fight (`0x0F0F`, `0x1CDB`, `0x0B73`, `0x0C64`, `0x1DE3`); eight failed placement tries already leave it dormant. Fixed rooms go through the Room event and the authored arena catalog, never through it. | **Yes** — after its last draw; a refused re-arm is the existing dormant record |
| What is the death / resurrection penalty? | The 1988 game's only one is `resurrect_apply`'s experience cut (CAST2 `0x05e0`: karma < 98 → exp × karma / 100, then level and max HP recomputed). Its four callers are In Mani Corp (spell and scroll), the healer and the Refuge. The port applies it on In Mani Corp only: the healer sets HP 1 with no cut, the Refuge sets HP to the old maximum with no cut — both pinned by the TypeScript reference's fixtures. Death costs no gold, items or stats. | **No clean hook** — deferred (§11.7) |
| Which encounters does the overworld rate thin? | Only new roaming monsters (`outdoor_tick`'s spawn block, the one place a monster joins the list), on the surface and the Underworld; never dungeons, towns or scripted fights. | already central |
| Is all outgoing damage hooked? | Yes for every party → enemy blow and spell (`damage()`); not arena fields, Fear / Repel / Polymorph (direct writes) or the ship's cannon — unchanged by design. | already central |

### 11.3 The rules architecture and precedence

- **One precedence function**, `effective_rule()` (`enhanced.cpp`): every hook reads its one field through it, so there is no second code path.

  ```
  1988 rule  →  difficulty (a preset's fixed row, or the journey's Custom values)
             →  World cheats (No Hunger, No Poison Damage, Disable Random Encounters)
             →  God Mode, at every party HP write (party_damage_blocked)
  ```

  At Original with no cheat every field is the 1988 value and every hook returns its input unchanged.
- `GameplayRules`: eight `uint16_t` fields (one member-pointer type for the Custom page): enemy damage, player damage, XP, overworld encounters, poison interval (0 = never), hunger, dungeon encounters, starvation. `kOriginalRules` is the identity row.
- `kRuleChoices`: one row per field — label, field, discrete values (ascending: left lowers, right raises), optional names — **append-only**, because a value's place in the save's `"custom"` array is its field.
- Hooks (unchanged sites from A4-ENH1 plus two): combat `damage()` (incoming, outgoing), combat `kill()` (XP), turn housekeeping (poison, meals, starvation), `world()` (overworld spawns), `dungeon_respawn()` (wanderers). Poison, meals and encounters use existing saved counters (turn count, calendar) and a fixed hash — no new RNG stream; the dungeon hash is salted with the floor because a fight and an entry do not advance the turn.

### 11.4 The presets (PROVISIONAL)

| Field | Original | Relaxed | Easy |
|---|---|---|---|
| Enemy damage (incoming) | 100 % | **85 %** | **65 %** |
| Player damage (outgoing) | 100 % | 100 % | **120 %** (was 100 %) |
| XP | 1.0x | **1.5x** | **2.0x** |
| Overworld encounters | 100 % | **90 %** | **65 %** (was 75 %) |
| Poison | every turn | **every 4th** | **every 10th** |
| Food (meals that eat) | all | **75 %** | **50 %** |
| Dungeon encounters (wanderer re-arms placed) | all | **90 %** | **65 %** |
| Starvation | Original (`rand(1,8)`) | **Reduced** (half, rounded up) | **Minimal** (a quarter, at least 1) |
| Death / resurrection | unchanged | unchanged | unchanged (§11.7) |

Every preset value is one of the Custom choices (`a4_enh2_rules` K2), so Custom can reproduce any preset. Rounding as A4-ENH1: half up, a positive value never scaled to 0, COMBAT's kill-outright 99 kept and never manufactured. The largest non-99 blow in the game is 30: 120 % reaches 36, Custom's 150 % reaches 45 — no field or cap is near. XP is scaled once at the one award site, before the 9999 cap.

### 11.5 Custom

| Row (Custom page) | Choices | Default |
|---|---|---|
| Enemy damage | 50 / 65 / 75 / 85 / 100 % | 100 % |
| Player damage | 100 / 110 / 120 / 135 / 150 % | 100 % |
| XP rate | 1.0 / 1.5 / 2.0 / 2.5 / 3.0x | 1.0x |
| Overworld encounters | 25 / 50 / 65 / 75 / 90 / 100 % | 100 % |
| Poison | Off / Light (every 10th turn) / Reduced (every 4th) / Original (every turn) | Original |
| Hunger | Off / 25 % / 50 % / 75 % / Original | Original |
| Dungeon encounters | 25 / 50 / 65 / 75 / 90 / 100 % | 100 % |
| Starvation | Off / Minimal (¼) / Reduced (½) / Original | Original |

- **Preset / Custom relationship.** Original, Relaxed and Easy are fixed rows; Custom is the journey's own values (`EnhancedState::custom`, Original's by default). Choosing a preset never writes them: **Easy → Custom → Original → Custom gives the same Custom values back** (`a4_enh2_rules` K9, `a4_enh2_runtime` U5). A fresh Custom starts at Original's values (the simplest predictable choice; seeding it from the last preset was rejected as an implicit mutation).
- **UI.** System Menu > Difficulty: Original / Relaxed / Easy / Custom and, under them, the selected row's actual values (`"  Enemy damage: 65%"`, … — twelve lines, the page's capacity). Enter on a preset uses it (from the next turn); Enter on Custom uses Custom and opens **Custom Difficulty**, a row per value, left/right applied at once (no transcript line per edit; only a change of difficulty is announced). Back returns to the Custom row. With a World cheat on, the subtitle says "World cheats take precedence".

### 11.6 Dungeon encounters and starvation

- **Dungeon (`rules_wanderer_allowed`).** After all of `dungeon_respawn()`'s 1988 draws (bank, up to eight cells, the hidden roll) the difficulty decides whether the rolled wanderer is placed; a refused one gets exactly the dormant record (type 255, bank 0, nowhere) that eight failed tries leave and that every part of the game already handles. Original: every re-arm placed; Original's draw sequence is untouched (`a4_enh2_preservation` B2 and the runtime's Deceit walk bit-identical). A non-Original refusal changes the later draws (a dormant wanderer does not walk) — the same trade-off A4-ENH1 accepted for overworld spawns. Fixed rooms, the Doom entrance and every scripted fight never ask.
- **Starvation (`rules_starvation_damage`).** `party_random_damage(g, rand, starvation)` gains a flag passed only by the starvation site: the `rand(1,8)` is still drawn per member, only the HP loss is scaled (Reduced ½, Minimal ¼, at least 1; Off: none, and no "Starving!"). The same draws at every severity when no one dies (`a4_enh2_rules` S1).

### 11.7 Death and resurrection — deferred

There is no clean central hook in the port today. The one 1988 penalty (the experience cut in `resurrect_apply`) is applied by the port only on In Mani Corp; the healer (SHOPPES `0x16ee`–`0x1703`) and the Refuge (BLCKTHRN `0x0b90`–`0x0b9d`) skip it, and set HP differently (D-83, D-84 below). A "softer penalty" hook would therefore act on one spell and leave the two common paths — where a softer penalty matters — unchanged. What it would take: (1) a fidelity fix first — one shared `resurrect_apply(CharacterState&, karma)` called by the healer and the Refuge, fixed in the TypeScript reference at the layer its fixtures pin, `--check`, regenerate, token-diff proof — which **changes Original on purpose** and so cannot ride with this batch; (2) then one hook in that function (an effective karma, ≥ 98 meaning no cut). No random numbers are drawn on any of these paths, so the draw order is not at risk. No death-penalty field was added to `GameplayRules` or the save.

### 11.8 The cheats

System Menu > **Cheats** now lists groups; each group page is the A4-ENH1 Cheats page made generic (an order table — `CheatKind` stays append-only, its value is its save bit — a help line per cheat, `cheat_name` a table).

| Group | Cheat | Semantics |
|---|---|---|
| **Party** | God Mode | (A4-ENH1) toggle; no party member loses HP. A4-ENH2 adds the naval OUCH (a skiff or ship blocked by a cactus: `rand(1,8)` on the active member), a sixth HP-loss site A4-ENH1 missed, and the inn's poisoned sleeper |
| | Heal Party / Cure Party | (A4-ENH1) unchanged; refused in combat |
| | **Restore MP** | the game's class rule (the inn, camping, `resurrect_apply` CAST2 `0x0632`): Avatar and mages INT, bards INT ÷ 2; other classes have no MP. Only raises; skips the dead and anyone not in the party. Allowed in combat (the arena reads MP from the roster) |
| | **Revive Party** | the dead in the party only, back as the game's own revivals leave them: `'G'`, HP to the maximum, MP by class, **no experience cut**; a record with no maximum takes `resurrect_apply`'s 30 × level; HP and MP are raised, never lowered (a Developer edit can leave more — the review's finding, commit 6). Refused in combat (the arena seats no dead member and writes its own HP and status back); living members untouched |
| **Inventory** | Add Gold / Max Gold | (A4-ENH1) unchanged |
| | **Max Food** | 9999 (every food writer's cap) |
| | **Max Keys / Max Torches / Max Gems** | 99 (the byte counters' cap) |
| | **Give Reagents** | each of the eight below 99 to 99; no quest item, skull key, carpet, potion, scroll or equipment touched |
| **World** | **No Hunger** | toggle; no meal eats and no one starves (also at food 0) |
| | **No Poison Damage** | toggle; poison takes no HP (no tick); the poisoned status stays; the inn no longer kills a poisoned sleeper |
| | **Disable Random Encounters** | toggle; overworld and dungeon: no spawn passes, no re-arm places a wanderer; roaming monsters are cleared at the next world turn (the list holds only random spawns), a placed wanderer goes dormant at the next dungeon tick, a camp sleeps through its 1/64 ambush (both draws made). Not touched: fixed rooms, the bridge troll's toll (a fixed map feature), the Doom entrance, Shadowlords, guards, scripted fights, and any fight the player starts |

No cheat lowers a value already above its cap (crops can push food to 10000, a Developer preset can leave more), wraps a field, or touches quest items. No teleport, equipment or quest-item cheat (left for an Advanced Cheats feature). Every applied cheat sets its `cheats_used` bit (support metadata only). The Developer tools are unchanged and stay off the ordinary menus.

**Changed on purpose from A4-ENH1:** God Mode now also covers the naval OUCH and the inn's poisoned sleeper (§10.6 had recorded the latter as an exception). Status deaths other than poison at the inn, scripted deaths (the location-29 trapdoor) and a Polymorph aimed at a party member are still outside God Mode.

### 11.9 Save and persistence

The per-journey `"enhanced"` sidecar object (still written only when something differs from the defaults; still carried by `persistence.cpp`'s `extras[]` whitelist; still never read or written by the PC save bridge):

```json
"enhanced": {"difficulty": "custom", "godMode": false, "cheatsUsed": 28672,
             "toggles": 28672, "custom": [75, 120, 300, 65, 10, 0, 25, 50]}
```

- `"difficulty"` gains `"custom"` (with a `static_assert` that the name table covers every difficulty — the A4-ENH1 table would otherwise have been read past its end).
- `"toggles"`: the World cheats that are on (their `cheat_bit`s), **only when one is on**; a load drops any other bit; a malformed value is none.
- `"custom"`: the eight Custom values in field order, **only when they differ from Original's** — whatever the difficulty, so they survive a preset. Read field by field: a missing, extra, foreign or non-numeric entry keeps that field's Original value; only an array is read (a red-first test found that `Json::at()` would index an object's members). Append-only: a six-value array written by commit 1 still loads.
- **Defaults and older saves:** no key — every save before A4-ENH1, every A4-ENH1 save without enhanced state, every PC import — is Original, no cheat, no toggle, Custom at Original's values. An A4-ENH1-shaped object loads and saves back to the same bytes (`a4_enh2_preservation` B5). A journey that never touched any of it saves the very document it saved before A4-ENH1 (`a4_enh1_preservation` A4).
- **Downgrade:** an A4-ENH2 save opened by A4-ENH1 firmware loads (unknown keys ignored); Custom plays as Original and the Custom values and toggles are dropped at its next save.
- **PC export isolation:** the `.GAM` of a Custom journey with every toggle on is byte-identical to the same journey on Original (`a4_enh2_rules` P4); the sidecar never leaves the card.
- One shape choice, recorded by the review: Custom values left changed while playing Original are saved (so they come back with Custom); stepping them back to Original's values removes the key again.

### 11.10 Preservation (Original + no cheat + no toggle)

- **New goldens recorded on the unmodified tree before any change** (commit 0, `a4-enh2-golden-head.log`), using only pre-A4-ENH2 interfaces, for the four gaps A4-ENH1's goldens never reached:
  - `a4_enh2_preservation`: B1 starvation (2,400 turns at food 0: 739 "Starving!" hours, 44 deaths; and `party_random_damage`'s other callers), B2 dungeon wanderers (2,000 re-arms and a 6,000-action walk with 9 ambushes), B3 camp (40 sleeps, 4 ambushed), B4 death and resurrection (In Mani Corp at seven karmas, the healer, the inn's poisoned sleeper at every inn), B5 A4-ENH1-shaped save documents;
  - `a4_enh2_preservation_runtime`: a starving daytime walk on the real overworld, then Deceit by its entrance and 900 inputs through the live dungeon turn (re-arms, walks, two corridor fights, meals and starvation underground), then Alt+S / Alt+L.
- After every later commit both, and A4-ENH1's two goldens, reproduce their recorded hashes **bit for bit**; the whole parity corpus passes unchanged.
- **Correction to §10.9 (found by this batch's investigation):** A4-ENH1's A1 golden never starves — its food never reaches 0 — although §10.9 and its source comment say it covers starvation. B1 covers it now; §10.9's text is left as written, with this note.

### 11.11 Tests

- New: `a4_enh2_preservation`, `a4_enh2_preservation_runtime` (commit 0); `a4_enh2_rules` — K1–K10 (Custom: choices, presets as points, precedence, identity at defaults, every value of every field through its hook, steps, row text, Poison / Hunger Off, the preset/Custom relationship, the default), T1–T5 (presets; outgoing damage through a real fight: 18→22/27, 16→19/24, 13→16/20 at Original → Easy 120 % / Custom 150 %, same draws; rounding and edges; XP 1.0–3.0x, a real kill 63 → 158 at 2.5x, the 9999 cap; spawn share 100000 / 90108 / 65102), W1–W4 (wanderer share 3998 / 3628 / 2670 / 1027 re-arms placed at Original / Relaxed / Easy / Custom 25 %, after identical draws; the dormant record; rooms; determinism across a save), S1–S2 (starvation 4001 / 2236 / 1239 / 0 HP over 300 hours, the same draws; per-draw rounding, fire / quake / cactus unscaled), X1–X7 (each new cheat, guards, caps, quest items, the naval OUCH through a real Move; X7 Revive never lowering MP, from the review), Z1–Z11 (precedence at every hook and site), P1–P4 (persistence); `a4_enh2_runtime` — U1–U9 (the Difficulty and Custom pages through the device, in play, the save, a power cycle, a starving walk with Starvation Off, 24 real entries into Deceit: 15 wanderers at Original, 2 at Custom 25 %), Y1–Y5 (the cheat groups, each new cheat from the device, the save's mark, Revive refused in a real troll fight), V1–V5 (the World page; the subtitle; a poisoned, starving walk; Alt+S / Alt+L / power cycle; a night circuit on Easy: 8 fights, and with Disable Random Encounters the roaming troll gone after one step and no fight).
- **RED first:** each production commit's new checks were shown RED against the code without the change (`a4-enh2-p{2,3,4,5,6}-red-first.log`; commit 1's P3 caught a real defect on its first run — the reader accepted an object as the Custom array — and commit 6's X7 was RED against commit 5's Revive).
- **Changed on purpose:** `a4_enh1_runtime` R1 (the Difficulty page's four rows and values), C2 / C4 / C5 / C6 (the cheat groups' navigation); `a4_enh1_rules` R2 (the Easy row) and R9 (Easy 64000–66000). No golden was re-recorded: the System Menu root and the screens the A4-UI1 goldens capture did not change.

### 11.12 Mutations

`tools/a4_enh2_mutation_check.py <build> [ids] [--anchors]` — 49 mutants, **49 killed**, 0 survived (`a4-enh2-mutation.log`: 47 killed and 1 INVALID through the driver's own text — mutant D4 put a `(void)` on the guarded line and tripped `-Werror=misleading-indentation`; `a4-enh2-mutation-rerun.log`: D4, fixed, killed; `a4-enh2-mutation-review.log`: after commit 6, X1–X9 again and the new X10, all killed; the restored build GREEN after each). They cover:
- **Original** (O1–O7): five non-identity Original rows — enemy and player damage 99 %, poison every 2nd turn, dungeon wanderers 90 %, starvation 50 % — and the precedence's difficulty layer read wrongly (every preset reading the Custom values; Custom reading Original's row). Poison turns every golden RED (A4-ENH1 A1 / P3 / P4, A4-ENH2 B1 / B2 and the runtime walk); dungeon 90 % turns B2 RED, starvation 50 % B1 and the runtime's starving walk; a 1 % damage change rounds away inside the golden fights (as A4-ENH1 found) and is killed by the identity sweeps (`a4_enh1_rules` R1, `a4_enh2_rules` K4).
- **Presets** (E1–E5): Easy's 120 % and 65 % back to A4-ENH1's values, Easy's dungeon share and Relaxed's starvation at Original, the outgoing hook removed.
- **Custom** (C1–C11): never written, written at Original's values, read ignored, a foreign value accepted, an object read as the array, `enhanced_is_default` ignoring the values, the `"custom"` name, a page edit not sent, the runtime dropping the values, Back to the root, a value wrapping instead of holding.
- **Dungeon and starvation** (D1–D6): the re-arm hook removed; the decision moved before the hidden roll (a refused re-arm then skips a 1988 draw — W1's identical-draws check kills it); the starvation flag not passed; every `party_random_damage` caller scaled; "Starving!" said when off; Off scaled to 1.
- **Precedence** (P1–P10): No Hunger without starvation, Disable Random Encounters sparing the dungeon, No Poison Damage overridden by the difficulty, the roaming list, the dungeon tick, the camp, the inn, the toggles not saved, a stray bit kept, `enhanced_is_default` ignoring the toggles.
- **Cheats** (X1–X10): Restore MP lowering a value, Revive in combat or on the living, the Max cheats lowering a value, a cap of 100, Give Reagents filling the skull keys, the naval OUCH unguarded, a group row applying its index's cheat, a World row showing God Mode's state, Revive lowering MP.

**Review.** Two adversarial review passes, each finding re-read by a skeptic told to refute it: commit 1 (three lenses: preservation, save, menu) and commits 2–5 (four lenses: preservation and RNG determinism, the cheats, the encounter sites, the menu and save). Commit 1's three findings were refuted as defects — they are recorded as design notes above (Custom values saved while on Original; the downgrade path; A4-ENH1's stale mutation anchors). Commits 2–5 gave one finding that stood: Revive Party set a revived member's MP to the class value even when it was higher (reachable only after a Developer edit or an edited import), against the batch's never-lower rule — fixed in commit 6 with its red-first test (X7) and mutant (X10). The preservation, encounter and menu / save lenses found nothing.

A4-ENH1's driver (`tools/a4_enh1_mutation_check.py`) is historical evidence for its own tree: six of its anchors (R1–R5, R14) no longer match after commit 1 rewrote those lines; the A4-ENH2 driver re-covers those defect classes (O1–O7, C6, P10) against today's code.

### 11.13 Host and firmware

- Serial suite per commit: 187 / 187 (commit 0), 189 / 189 (commits 1–6: 143.62, 145.73, 147.09, 154.83, 150.45, 156.22 s); commit 6's run is the final tree's (commit 7 changes no host-built file); 0 project warnings.
- Firmware (1 MiB app partition), each commit built in `build-a4-enh2`:

| Image | Size | Free | Δ |
|---|---|---|---|
| A4-ENH1 (`5e0a16eb`) | `0xfe4a0` (1,041,568 B) | 7,008 B | |
| commit 1 (Custom; first build, before the two reductions below) | `0xfed10` (1,043,728 B) | 4,848 B | +2,160 |
| commit 1 (as committed) | `0xfe820` (1,042,464 B) | 6,112 B | +896 |
| commit 2 | `0xfe820` | 6,112 B | 0 |
| commit 3 | `0xfe9b0` (1,042,864 B) | 5,712 B | +400 |
| commit 4 | `0xfee90` (1,044,112 B) | 4,464 B | +1,248 |
| commit 5 | `0xff180` (1,044,864 B) | 3,712 B | +752 |
| commit 6, and the pre-commit image of commit 7 (`4.0.0-alpha4-enh2-debug`) | `0xff180` (1,044,864 B) | 3,712 B | 0 |

- **Size work, measured.** Commit 1's first build cost 2,160 B. Two behaviour-preserving reductions brought it to 896 B: every hook reads one field (`effective_rule`) instead of copying the whole rules struct, `kOriginalRules` is `inline constexpr` (one copy, not one per translation unit), and `enhanced.cpp` and `system_menu.cpp` — cold code, like `endgame_scene.cpp` (§7.14) — are built `-Os` on the device (`main/CMakeLists.txt`; code generation only, the host tests run the same source). A 32-bit form of the meal rule was tried and reverted: a state document may carry any int32 year, and the 64-bit form cannot overflow.
- **A4-ENH2 costs 3,296 B; 3,712 B (0.35 %) of the partition remain.** That is low. The next feature must free space first. Measured, behaviour-preserving candidates (none applied here — each touches another subsystem and wants its own batch): `-Os` on the other cold save / persistence / frontend objects (`save_core` and `persistence` 14.5 KB `.text` each, `alpha_save` 12.5 KB, `frontend` 8.3 KB: about 10–14 % of them); the `upper()::mappings` case table (31,600 B, about 17 KB smaller with a compact encoding, generator and drift test to change); the `Board` and input objects' `.data` (about 7.2 KB of mostly zero initialisers that could live in `.bss`). Not candidates: diagnostics, preservation tests, the Developer tools, save compatibility.
- Section diff against the A4-ENH1 image (`a4-enh2-p6-fw-size-diff.log`): flash `.text` +1,556 B, `.rodata` +1,744 B (the choice and help tables and their strings), internal `.bss` +160 B (the System Menu's ninth line buffer, 96 B, and `EnhancedState`'s growth inside the runtime's game states), `.data` and IRAM unchanged. The three ELF guards are GREEN on it (`a4-enh2-p6-elf-checks.log`: no lock / allocation / log on the per-sample audio path; that path in IRAM; the image check). The image to flash is built **after** the last commit from a fresh `--no-ccache` directory, so its `Git` line names the commit; its path and SHA-256 go in `native/core/a4-enh2-image-*.log`, left untracked like A4-ENH1's until the hardware result.

### 11.14 Hardware checklist (A4-ENH2 image) — PENDING

On the A4-ENH2 image (`native/targets/tdeck/build-a4-enh2-final/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-enh2-Debug-Launcher.bin`, built after this commit; identity screen `FW 4.0.0-alpha4-enh2-debug` and `Git <this commit>`; its SHA-256 in `native/core/a4-enh2-image-identity.log`). **Stop if the identity screen does not say so.** Back up the card's `ultima5/` first; packs as for A4-ENH1 (nothing to recopy). Developer values apply on Enter. Report PASS / FAIL per line; the balance lines want an opinion, not a PASS. Set the state each line needs with the Developer rows first (food, a dead companion, a poisoned member, the hour): the device runs on your own save.

**A. Difficulty**
1. Original on an older save (A4-ENH1 or RC1): everything as before — a fight, a poisoned walk, a day's meals, a starving hour, a dungeon floor.
2. Relaxed / Easy / Custom: the Difficulty page shows each row's values under the four rows as the cursor moves; Enter changes the subtitle's "Now:".
3. Easy, a fight: your blows visibly larger (Z-stats or the damage line), enemy blows smaller; a kill's XP ×2 (Z-stats before / after).
4. Easy at night in open country: noticeably fewer monsters than Original (opinion).
5. Custom XP 3.0x: one kill gives three times Original's XP. Custom Poison Off / Light / Reduced: a poisoned member loses no HP / 1 per 10 steps / 1 per 4.
6. Custom Hunger Off: food does not drop over a game day; 50 %: about half.
7. Dungeon: Easy or Custom 25 %: across several floor changes / entries, the wanderer appears clearly less often than on Original (opinion); a fixed room still fights.
8. Starvation: food 0 (Developer), walk across an hour: Original "Starving!" and 1–8 HP each; Custom Minimal 1–2; Off: no line, no HP.

**B. Custom**
9. Change every row once (left and right; each holds at its ends); Back; the Difficulty page's Custom row shows the new values.
10. Alt+S (or Save Game), power off, power on, Continue: Difficulty says Custom and the values are back.
11. Choose Easy, then Original, then Custom again: the Custom values are the ones you left.

**C. Cheats** (System Menu > Cheats: Party / Inventory / World)
12. Restore MP (Avatar or a mage with MP spent): MP back to INT; a fighter unchanged.
13. Revive Party (a dead companion — Developer): 'G' with full HP; during a fight it says "Not during combat".
14. Max Food 9999, Max Keys / Torches / Gems 99 each, Give Reagents 99 of each; Z-stats shows them; a second press says "already full".
15. No Hunger: a game day, food unchanged; with food 0 (Developer), no "Starving!".
16. No Poison Damage: a poisoned member walks without losing HP and stays poisoned; resting at an inn while poisoned: survives.
17. Disable Random Encounters: monsters on screen vanish after a step; none appears at night in open country; in a dungeon no wanderer; camping is never ambushed; the bridge troll and dungeon rooms still happen.
18. Save, power-cycle, Continue: the World toggles are still on (the World page shows them); the Difficulty subtitle says "World cheats take precedence".

**D. Precedence**
19. Easy + Disable Random Encounters: no monster at night.
20. Easy + No Poison Damage: no poison HP loss (Easy alone: 1 per 10 steps).
21. Custom Hunger Off (no cheat): no food eaten; Custom Starvation Original still starves at food 0 — No Hunger stops that too.
22. God Mode + Easy in a fight: no HP lost, your blows still Easy's.

**PASS** needs A1, B, C and D with no crash, lock, reset or input-mode corruption; the balance opinions (A3–A8) decide a later tuning, they do not fail this run.

### 11.15 Found in passing (recorded, not fixed)

The investigation found these on its way; each is recorded where it belongs, none is fixed by this batch (Original must not change here):
- **D-83** the healer's Resurrect and **D-84** the Refuge do not run `resurrect_apply` (§11.7).
- **D-85** the location-29 trapdoor kills the whole roster (inn companions included), not the party (TOWN `0x0ff9`–`0x103a` loops over the party size).
- **D-86** a digit key in a dungeon on an invalid member runs a stray overworld turn (`commands.cpp`; DUNGEON `0x07c8`–`0x07d1` passes no turn).
- **D-87** picking crops outside combat adds food with no cap (SJOG `counter_add(food, 1, 9999)`; the arena twin caps).
- **D-88** the 1988 combat advances the clock a minute every ten actions (COMBAT `0x0C64`–`0x0C76`); neither port does, so a fight that crosses an hour boundary no longer swallows it (meal and starvation timing after combat).
- **D-89** the naval OUCH takes `rand(1,8)` from the active member directly; the notes (`re/notes/cactus-ouch-acta.md`) and the reference say the original calls `party_random_damage` — to adjudicate against the binary.
- Also: In Mani Corp's scroll on a living target should print "Not dead!" (DS `0x953c`); `re/notes/kernel-survival.md` §293–295 says meals and hunger run during the jail / inn wait (a byte scan shows no housekeeping there); God Mode does not cover a Polymorph aimed at a party member (a player's own act).

### 11.16 Deferred and open

- **Death / resurrection softening** (§11.7): needs the D-83 / D-84 fidelity fix first.
- **Tuning** of every preset and choice: hardware and play.
- **Flash:** 3,712 B left; the next feature needs a size batch first (§11.13). *(A4-FLASH1, §12: the app partition is now 1.25 MiB, 265,856 B free; Launcher installs are unaffected.)*
- Advanced Cheats (teleport, equipment, quest items): not in this batch, by the brief.
- The bridge troll is not a "random encounter" here (a fixed map feature with a random trigger); a separate toggle could cover it if wanted.
- The A4-ENH1 hardware checklist (§10.13) was reported passed as a whole; its tuning opinions were not itemised.

### 11.17 Status

| Axis | State |
|---|---|
| Custom difficulty, presets, dungeon and starvation rules, the cheats, precedence | **implemented, host-verified** |
| Death / resurrection softening | **deferred** (no clean hook; §11.7) |
| Original-mode preservation | **host-verified**: A4-ENH1 and A4-ENH2 goldens bit-identical; parity corpus unchanged |
| Firmware | **build-verified** (pre-commit `0xff180`, 1,044,864 B, 3,712 B free, ELF guards GREEN; the flashable image is built after this commit) |
| Tuning values | **PROVISIONAL**; balance not final |
| Hardware | **PENDING** (§11.14) |
| Commit / tag / push | committed; not tagged, not pushed, not flashed |

*Update (A4-CLOSE1, 2026-10-02): hardware **accepted**, recorded in §14.2. The cheat system works on hardware. The user is comfortable proceeding with the difficulty implementation as it is, so the presets and the Custom choices are **accepted for Alpha 4**; exhaustive subjective balance testing is not an Alpha 4 gate, and every value stays adjustable in a later version (`kGameplayRules`, `kRuleChoices`). No per-line results for §11.14 were given. The PENDING and PROVISIONAL above are the status as this batch wrote it.*

## 12. A4-FLASH1 — flash budget and the app partition (2026-10-02)

A4-ENH2 left 3,712 B of the 1 MiB app partition. Before the next feature (keyboard backlight), this batch traced where the ceiling comes from, how Launcher uses it, and made the smallest change the evidence supports. No gameplay, save, pack or runtime code changed; the image's memory sections are byte-for-byte A4-ENH2's.

### 12.1 Baseline

- HEAD `dc61bc64` (A4-ENH2 (7)); the untracked A4-ENH1 / A4-ENH2 / RC1 image logs in `native/core/` were left alone.
- Image `0xff180` (1,044,864 B), SHA-256 `d310fa64…8e09` (§11.13). ESP-IDF's `check_sizes.py`: "Smallest app partition is 0x100000 bytes. 0xe80 bytes (0%) free." (`a4-enh2-image-build.log`).
- Build: `idf.py --no-ccache -B <dir> reconfigure` then `ninja -C <dir> -j 4 all`, packaged by `python package_launcher.py --build-dir <dir>`.
- Partition source: `sdkconfig.defaults` had `CONFIG_PARTITION_TABLE_SINGLE_APP=y`, and the git-ignored local `sdkconfig` agreed. No partition CSV in the repo.

### 12.2 Where the old limit came from

There was one cause: **ESP-IDF's default partition table.** `CONFIG_PARTITION_TABLE_SINGLE_APP` selects `components/partition_table/partitions_singleapp.csv` from ESP-IDF (`nvs 0x6000`, `phy_init 0x1000`, `factory 1M`). ESP-IDF's Kconfig describes it as "the default partition table, designed to fit into a 2MB or larger flash with a single 1MB app partition". It has been in `sdkconfig.defaults` since the Milestone 2 bring-up (`11e32392`, a 298 KB image) and was never revisited.

None of these set it: no OTA (no `otadata`, no `ota_*`), no filesystem partition, no Launcher rule, no packaging rule. The searches covered partition CSVs, `sdkconfig*`, CMake, packaging, and hard-coded `0x100000` / `1048576` in `*.py`, `*.cmake`, `CMakeLists.txt` and `*.ts`. `package_launcher.py` only *printed* "App partition minimum" (the image rounded up to 64 KiB). It never compared that figure with anything. The "1 MiB slot / allocation" wording in `ALPHA3.md`, `LAUNCHER.md` and the checklists described the build's partition. It was never a Launcher limit.

### 12.3 Flash layout from the repository

Standalone layout (what `idf.py flash` writes), from the built `partition-table.bin`:

| Region | Offset | Size | Size (B / KiB) | Notes |
|---|---|---|---|---|
| Bootloader | `0x0` | `0x8000` region | 32,768 / 32 | `bootloader.bin` `0x5850` (22,608 B), `0x27b0` free |
| Partition table | `0x8000` | `0x1000` | 4,096 / 4 | 3,072 B binary + MD5 |
| `nvs` | `0x9000` | `0x6000` | 24,576 / 24 | unused by OpenU5 (no `nvs_flash` component, no NVS calls) |
| `phy_init` | `0xf000` | `0x1000` | 4,096 / 4 | unused (no radio) |
| `factory` (before) | `0x10000` | `0x100000` | 1,048,576 / 1,024 (1 MiB) | ends `0x110000` |
| **`factory` (A4-FLASH1)** | `0x10000` | **`0x140000`** | **1,310,720 / 1,280 (1.25 MiB)** | ends `0x150000` |
| Unallocated (before / after) | `0x110000` / `0x150000` | to 16 MB | 15,663,104 B (14.94 MiB) / 15,400,960 B (14.69 MiB) | no data partition |

- **Configured flash:** `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`. The comment's basis is LilyGO's part number (ESP32-S3FN16R8).
- **Not proven by the repository:** the chip on the tester's T-Deck. `main.cpp` logs `flash=%luMiB` from `esp_flash_get_physical_size()` at boot. No committed serial capture contains that line (the hardware captures start after boot). The new layout ends at 1.3 MiB, so it fits any part of 2 MB or more. The result does not depend on the 16 MB assumption.
- **The device's real table is Launcher's, not this one.** It is not in the repository (§12.4).

### 12.4 How Launcher installs OpenU5

Launcher (bmorcelli/Launcher) is not vendored. The project's packaging was validated against its SD installer (`LAUNCHER.md` at `11e32392` cites `src/sd_functions.cpp` and `src/partition_install_layout.cpp`). Those files were read at upstream `cd392f28` (main, 2026-10-01):

- **The SD file is the bare app image, stored as-is.** On the SD card each firmware costs exactly its file size (1,044,864 B), unpadded. `package_launcher.py` copies `openu5_tdeck.bin` byte for byte, merges no bootloader or partition table, and refuses an image that looks merged.
- **Launcher never reads this project's partition table.** `updateFromSD()` reads 16 bytes at file offset `0x8000`. Without the `AA 50 01` partition-table magic, the file is a plain app image. `effectiveSdAppSize()` measures it by walking its segments to the checksum and appended SHA-256, and `installFromSdDynamic()` installs it.
- **Launcher creates the app partition from the image size.** `launcherSelectInstallLayout()` needs `alignUp(image, LAUNCHER_APP_PARTITION_ALIGNMENT)` with `LAUNCHER_APP_PARTITION_ALIGNMENT = 0x10000` (`partition_table_model.h`). `launcherPartitionCreateOtaApp()` adds an OTA app entry of exactly that size in free space. Launcher then writes its generated table (`launcherPartitionWriteGeneratedTable`), sets the OTA boot entry, and streams the image from SD in 4 KiB chunks.
- **Space is dynamic, not fixed slots.** Each installed firmware is its own OTA partition, sized to its image. When free space runs out, Launcher offers to reuse an existing non-running OTA app at least as large ("Use X partition"), or to repartition one plus adjacent free space. It never touches the running partition (`launcherPartitionIsReplaceableApp`).
- **Consequences:**
  1. The ESP-IDF factory size changes nothing Launcher stores, sizes or flashes.
  2. What OpenU5 costs a Launcher device is the image rounded up to 64 KiB: today 1,048,576 B in flash and 1,044,864 B on the SD card.
  3. Growing the image by 3,713 B moves the Launcher partition to 1,114,112 B (1,088 KiB) whatever the partition table says. That is the real compactness line.
- **Unresolved:** the Launcher *version* on the tester's device is not recorded. The dynamic installer is upstream at least since May 2026 (the oldest commit returned for `partition_install_layout.cpp`, `a1f2f9e412`, 2026-05-20). The project cited it from Milestone 2. An older fixed-scheme Launcher would install into its own preset app partition. That still would not read this table, but its preset size would then be a hard limit. Every image so far (up to 1,044,864 B) installed on the device, so its partition holds at least 1,048,576 B.

### 12.5 Is the limit artificial?

Yes. It is the configured size of ESP-IDF's stock table, nothing more. On a Launcher device it is not even the install limit. It is only the build's hard stop (`check_sizes.py` / `app_check_size`) and the layout of a standalone `idf.py flash`. Valid sizes: an app partition's offset must be `0x10000`-aligned and its size `0x1000`-aligned without secure boot (`gen_esp32part.py`). Launcher rounds to `0x10000`, so the candidates below are 64 KiB multiples.

### 12.6 Options compared

Current image 1,044,864 B. "Launcher cost" means per installed copy in flash / per file on the SD card.

| Option | App capacity | Free now | Launcher cost | Migration | Risk | Saves / settings |
|---|---|---|---|---|---|---|
| Keep 1 MiB, optimise only | 1,048,576 (`0x100000`) | 3,712 B (0.35 %) | 1 MiB / 1,044,864 B | none | every feature starts with a size batch; one slip fails the build | unaffected |
| 1.125 MiB | 1,179,648 (`0x120000`) | 134,784 B (11.4 %) | same | Launcher: none; standalone: new table | the 128 KiB review line trips after 3.7 KB | unaffected |
| **1.25 MiB (chosen)** | **1,310,720 (`0x140000`)** | **265,856 B (20.3 %)** | **same** | Launcher: none; standalone: new table | low | unaffected |
| 1.5 MiB | 1,572,864 (`0x180000`) | 528,000 B (33.6 %) | same | same | weakens the ceiling's role as a brake | unaffected |
| 2 MiB | 2,097,152 (`0x200000`) | 1,052,288 B (50.2 %) | same | same | no brake for many releases | unaffected |

The Launcher cost is the same in every row: it follows the image, not the partition. The choice therefore trades only the build's brake against the risk of a forced size batch.

### 12.7 Headroom policy (since A4-FLASH1)

- **Warn below 128 KiB free** in the app partition (image > 1,179,648 B): the next feature gets a size review first.
- **Fail below 64 KiB free** (image > 1,245,184 B): the build and the packager stop. To go further, free space or raise the partition deliberately, recorded here.
- **Watch the Launcher step.** Each 64 KiB boundary costs 64 KiB of device flash per installed copy. The packager and the build print the distance to the next step, and a release names its Launcher allocation.
- **Never raise the partition to fit one feature.** Use the measured candidates in §12.8 first.
- Recent growth: A3 RC1 → A4-ENH2 was +56,544 B over ten Alpha 4 batches (≈ 5.7 KB each; the largest, A4-SAVE3, +14.7 KB). That leaves ≈ 20 such batches before the warning line and ≈ 35 before the stop. The keyboard-backlight feature (a GPIO/PWM control and a settings row) is expected to cost a few KB.
- Why 1.25 MiB and not less: 1.125 MiB satisfies the 128 KiB review line by only 3.7 KB, so the next batch would trip it. 1.1875 MiB (`0x130000`) would also work, but it buys nothing on a Launcher device, and 1.25 MiB is the round value, 20 % free now.

### 12.8 Size audit (A4-ENH2 map; findings only, nothing applied)

- **Totals:** flash `.text` 714,522 B, `.rodata` 230,956 B; `libmain.a` is 514 KB of the code.
- **Strings: 146,232 B.** The linker's merged string pool (4,752 strings). The map charges it to `stdio_vfs.c.obj` (`.rodata.esp_stdio_register.str1.4`, "size before relaxing 0x51"); it is not ESP-IDF's console. Of it, 65,154 B is 1,030 `ESP_LOG` format strings. That includes 12,360 B of the `"X (%lu) %s: "` prefix that Log V1 compiles into every format, and 25,850 B of the project's own `KEY=value` serial traces (310 strings, the longest `AUDIO_PERF`, `DEBUG_TELEPORT`, `RENDER_PERF`, `U5OBJ …`). These are diagnostics and stay. `CONFIG_LOG_VERSION_2` would drop the per-format prefix (≈ 12 KB, **unmeasured**, changes log plumbing; its own batch).
- **Build-wide optimisation is `-Og`** (`CONFIG_COMPILER_OPTIMIZATION_DEBUG`, ESP-IDF's default). `-Os` is the largest lever, but it changes code generation everywhere, including timing on the render and audio paths. It is an architectural change with a hardware retest, not a quick win. The per-file `-Os` on cold objects (`endgame_scene`, `enhanced`, `system_menu`) remains the pattern.
- **Largest symbols:** `upper()::mappings` 31,600 B (`.rodata`; about 17 KB smaller with a compact encoding, §11.13); `openu5::execute` 13.1 KB; `AlphaRuntime::render` 10.4 KB; `AlphaResourcePack::load` 10.2 KB; `Board::show_alpha` 8.5 KB; `DeviceSmokeTests::run` 6.9 KB (diagnostic, kept).
- **Library weight:** `stdio`/fatfs/sdmmc/SPI/I2C/I2S drivers are each needed; libc 28 KB; `libstdc++` 3.9 KB (no exceptions, no RTTI); `esp_err_msg_table` 1.8 KB.
- **No duplicate tables found** beyond what §11.13 already fixed (`kOriginalRules` `inline constexpr`). No `__FILE__` paths in the pool; assertion expressions from FreeRTOS headers are present (`CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE`), a few hundred bytes.
- **Measured, behaviour-preserving candidates still open (§11.13):** `-Os` on the cold save / persistence / frontend objects (≈ 10–14 % of their ~50 KB `.text`); the `upper()` table encoding; `Board` / input `.data` initialisers to `.bss` (≈ 7.2 KB of internal RAM, not flash).

### 12.9 Changes

- `native/targets/tdeck/partitions.csv` (new): `nvs` `0x9000`/`0x6000` and `phy_init` `0xf000`/`0x1000` unchanged, `factory` `0x10000`/`0x140000`.
- `sdkconfig.defaults`: `CONFIG_PARTITION_TABLE_CUSTOM=y`, `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`, replacing `CONFIG_PARTITION_TABLE_SINGLE_APP=y`. **The local `sdkconfig` is git-ignored and set the choice explicitly, so it was edited by hand** (the same three lines; a pre-edit copy was kept outside the tree). A fresh checkout gets the new table from the defaults.
- `check_app_budget.py` (new): reads the built image and `partition-table.bin`, prints free space and the Launcher allocation, warns below 128 KiB free, exits 1 below 64 KiB. `CMakeLists.txt` runs it after every build (`openu5_app_budget`, `ALL`).
- `package_launcher.py`: prints the same budget lines and refuses an image that fails the budget. The packaged file is still the byte-identical app image.

### 12.10 Verification

- Fresh `build-a4-flash1` (`--no-ccache` reconfigure + `ninja -j 4`), first attempt clean. `check_sizes.py`: "binary size 0xff180 bytes. Smallest app partition is 0x140000 bytes. 0x40e80 bytes (20%) free." The budget line reads 265,856 B free (20.3 %) and Launcher allocation 1,048,576 B. `esp_idf_size --diff` against `build-a4-enh2-final`: every section equal. The pre-commit image differs from A4-ENH2's only through its `Git dc61bc64d5b1-dirty` string, whose shift moves the merged string pool.
- `partition-table.bin` decoded: `nvs 0x9000 0x6000`, `phy_init 0xf000 0x1000`, `factory 0x10000 0x140000`, end marker.
- Guard RED first (`a4-flash1-budget-red-first.log`): against the A4-ENH2 build's 1 MiB table, `check_app_budget.py` exits 1 and `package_launcher.py` refuses the image. Against the new table both pass. The boundaries (1,179,648 ok / 1,179,649 warn / 1,245,184 warn / 1,245,185 fail at `0x140000`) are recorded.
- ELF guards GREEN (`a4-flash1-fw-elf-checks.log`). Packaging OK (`a4-flash1-fw-package.log`). Host suite 189 / 189, serial, 144.10 s (`a4-flash1-host-ctest.log`; `a3_04b_perf` reads `sdkconfig.defaults` and still passes).
- No script assumed the old size: no hard-coded `0x100000` / `1048576` in any build or packaging script.

### 12.11 Migration and data

- **Launcher installs (the supported path): nothing changes.** The Launcher file is the same kind of bare image of the same size, and Launcher sizes its own partition. No Launcher update, no erase. The partition table this batch changed is never sent to the device.
- **Standalone `idf.py flash`** (already discouraged in `LAUNCHER.md`: it replaces Launcher's table): it writes the new table. `nvs` and `phy_init` keep their offsets and sizes, and `factory` grows into flash that was unallocated, so nothing previously allocated is moved. It does replace whatever table the device had, as before. On a Launcher device that means Launcher's table: Launcher and every installed firmware would have to be reinstalled. This is unchanged by A4-FLASH1.
- **NVS / settings:** OpenU5 uses no NVS. Settings (`settings.json`) and saves live on the SD card (`/ultima5/…`). No partition change reaches them. No OTA metadata exists to change.
- **Image to flash:** none new. The firmware the device runs is A4-ENH2's: the A4-ENH2 image stays the hardware candidate (§11.14), and `PROJECT_VER` is unchanged. A post-commit build of this batch would differ only in its `Git` line.

### 12.12 Status

| Axis | State |
|---|---|
| Source of the 1 MiB limit | **proven**: ESP-IDF stock single-app table via `sdkconfig.defaults` |
| Launcher behaviour | **source-verified** at upstream `cd392f28`; installed Launcher version **unknown** |
| Physical flash size | **assumed** 16 MB (part number); not in any committed capture; the layout does not depend on it |
| Partition | 1.25 MiB factory, build-verified; budget guard RED-first and GREEN |
| Hardware | nothing to run for this batch on the Launcher path. Optional: read the boot log's `flash=` line once |

## 13. A4-POLISH3 — keyboard backlight setting (2026-10-02)

A device preference: a Settings row that sets the T-Deck keyboard's backlight. Nothing in gameplay, saves, difficulty, cheats or preservation changes.

### 13.1 Baseline

HEAD `1b59e9a8` (A4-FLASH1 logs on `a07cae3e`, on A4-ENH2 `dc61bc64`). Host suite 189 / 189, serial, 147.35 s (`native/core/a4-polish3-baseline-ctest.log`). The keyboard driver is `main/tdeck_input.cpp` (`InputHardware`): ESP-IDF `i2c_master`, I2C0, SDA 18 / SCL 8 / INT 46, keyboard at `0x55`. Its capture task is the only I2C user. It sends one command, raw mode `0x03`, at init and after a bus recovery, and reads five matrix bytes. No backlight command was sent anywhere. `kTftBacklight` (GPIO 42) is the display's light, a different part.

### 13.2 The control path (verified)

Source: LilyGO `T-Deck/examples/Keyboard_ESP32C3/Keyboard_ESP32C3.ino` at master `12f12f8c` (2025-06-20), and its parent revision before `9d15775e` (2024-12-25).

- The light belongs to the keyboard's **ESP32-C3**, not to the S3. Pin 9, LEDC channel 0, 1 kHz, **8-bit** duty.
- Commands, written to `0x55`:
  - `0x01 <duty>`: set the duty now, **0..255**, 0 = off. It also sets the sketch's on/off state.
  - `0x02 <duty>`: Alt+B's duty while the set duty is 0. Kept only if above 30. Default 127.
  - `0x03` / `0x04`: raw / key mode (raw is what OpenU5 uses).
- `case 0x01` has no `break`. It falls into `case 0x02`, which calls `Wire.read()` again. A two-byte frame reads −1 there and changes nothing. A third byte would become Alt+B's duty. **The frame is exactly two bytes.**
- **No persistence, no read-back.** The sketch boots dark (`KB_BRIGHTNESS_BOOT_DUTY 0`) and keeps no copy across a reset. A raw-mode read returns the matrix only. The S3 can set the light but never query it.
- **Alt+B** is handled by the C3 itself, in raw mode too (its `loop()` runs whatever the mode). It toggles between 0 and the set duty (or the `0x02` duty if the set duty is 0).
- **Firmware generations.** Before 2024-12-25 the sketch had no I2C commands at all: Alt+B toggled the pin on/off, and the S3 could not control the light. Raw mode (2025-06-12) is newer than `0x01`/`0x02` in the same file. So **every keyboard OpenU5 can read in raw mode supports arbitrary 0..255 duty**. A binary-only firmware cannot run OpenU5's keyboard at all. The installed sketch revision is still not observable (`DEBUG51.md`: an ACK is not a capability), so §13.9 checks it on hardware.

### 13.3 Levels and mapping

| Level | Duty | Note |
|---|---|---|
| Off | 0 | the C3's own boot state; the default |
| Low | 32 | just above the sketch's 30 floor for Alt+B |
| Medium | 127 | the sketch's own Alt+B default |
| High | 191 | 75 % |
| Max | 255 | 100 % |

Discrete levels, not a numeric slider, to match the other named rows (Text / UI). The duties are **provisional**: LED brightness is not linear in duty, so Low may need tuning on the device. They live only in `main/keyboard_backlight.h` (`kKeyboardBacklightDuty`). The core knows level names, never duties.

### 13.4 Settings UI

Both Settings pages (title and System Menu) get a seventh row, appended after Music Volume: **`Keyboard Backlight: Off|Low|Medium|High|Max`**. Left/right (and Enter) step it and cycle like Text / UI: Right from Max is Off, Left from Off is Max. Footer: `Left/right: Off to Max; Mic saves`. If no keyboard answered at boot, the row still edits and saves, and the footer reads `No keyboard found; saved for next boot`. That state survives Return to Title, as music availability does.

### 13.5 Application

- Each edit applies **at once**: every accepted Settings key already runs `AlphaRuntime::apply_device_settings()`, which now calls `sync_keyboard_light()`. That hands the level to the bound sink **only if it differs from the last one sent**. The cursor, other rows and the save send nothing.
- `main.cpp` binds the sink after `initialize()` (`attach_keyboard_light`): `InputHardware::set_keyboard_backlight(level)` only stores the level in an atomic. The **capture task** sends it between matrix reads: `0x01 <duty>`, one 10 ms-timeout transmit, never while the bus is recovering. The game thread never touches I2C, so input and the light never race.
- Policy (`KeyboardBacklightPolicy`, pure and host-tested): a level is written once. A failed write is retried on the next passes, **3 attempts at most**, then left alone until the level changes. After a recovery re-enters raw mode, the level is written again (a C3 that reset is dark). No write floods the bus, and a failure never blocks input or boot.
- Display brightness (`board.set_brightness`, GPIO 42) is untouched.

### 13.6 Persistence

`settings.json` key `"keyboardBacklight"`: an integer 0..4, always written, optional on read (like `"trackballSpeed"`). An older file without the key loads every other value and takes **Off**, which is exactly what those cards had (OpenU5 never lit the keyboard, and the C3 boots dark). A non-integer or out-of-range value reads Off and the rest of the document still loads. An older firmware ignores the key. Never in save data.

Boot: settings load in `initialize()`, then `attach_keyboard_light` sends the saved level once, Off included. Off also turns off a light left on by Alt+B across an S3-only restart.

### 13.7 Alt+B policy

OpenU5 already drops Alt+B (`ui_input_adapter.cpp`: Alt chords other than M / D / S / L produce nothing), so the chord reaches only the C3. **The chosen policy is to leave the firmware shortcut alone**:

- Alt+B stays a quick toggle: off, and back on at the Settings level (or at the sketch's 127 if the Settings level is Off).
- OpenU5 does not intercept it and does not try to mirror it. The C3 has no read-back, and matching its edge detection from our sampled matrix could drift.
- The Settings row is the saved preference. It is re-applied at boot, on every change and after a keyboard recovery. So an Alt+B toggle lasts until one of those, and the row can show a level the light is not at.
- OpenU5 sends no `0x02`. Alt+B from Off keeps the sketch's 127 (= Medium).

### 13.8 Files, tests, size

- `main/keyboard_backlight.h` (new: protocol constants, duties, frame, policy). `tdeck_input.{h,cpp}`: the atomic, the write in `service_once()`, re-arm after raw mode. `alpha_runtime.{h,cpp}`: `KeyboardLightSink`, `attach_keyboard_light()`, `sync_keyboard_light()`. `main.cpp`: the bind. Core: `FrontendSettings::keyboard_backlight`, the names and step in `frontend.h`, the codec, both menus (`SettingsRow::kKeyboardLightRow`). `PROJECT_VER` `4.0.0-alpha4-polish3-debug`.
- New host targets: `a4_polish3_keyboard_light` (20 checks: levels/duties/frame, JSON round trip, old-card migration, bad values, the write policy, both menus) and `a4_polish3_keyboard_light_runtime` (10 checks on the real `AlphaRuntime` with a recorder sink: boot applies the saved level once, an older card or no card sends Off, each change is sent at once and only once, Mic saves `"keyboardBacklight"`, a reboot re-applies it, wrap, no-keyboard footer).
- Changed on purpose (the row count and default document): `frontend_test` (7 rows), `a3_01_audio_contract` F5/F7 (7 rows) and F10 (the default document gains `"keyboardBacklight":0`), `a4_enh1_runtime` D1 and `a4_ui4_presentation_runtime` F4b (7 rows), and two A4-UI1 goldens, `settings` `0x914e832c2718da41` and `settings-large` `0x2e0ea57d2967bfd7`. Only the new row's pixels differ (`a4-polish3-golden-proof.log`); the other six states are identical.
- The test found one defect before commit: `FrontendSession::start()` rebuilt the session and dropped "no keyboard" on Return to Title. Fixed (M7).
- Mutations: `tools/a4_polish3_mutation_check.py <build> [ids]`, **17 / 17 killed** (`a4-polish3-mutation.log`). `tdeck_input.cpp` is device-only. Its policy is the mutated header, but the I2C call itself is covered only by the hardware run.
- Host suite **191 / 191**, serial, 170.28 s (`a4-polish3-host-ctest.log`).
- Firmware (`build-a4-polish3`, first attempt clean): **`0xff6f0` (1,046,256 B)**, +1,392 B against A4-FLASH1's `0xff180` (1,044,864 B). `.text` +944, `.rodata` +432, internal `.bss` +32, `.data` +16; IRAM unchanged. **264,464 B (20.2 %) free** in the 1.25 MiB partition. The Launcher allocation is still 1,024 KiB, with the next 64 KiB step 2,321 B away. ELF guards GREEN (`a4-polish3-fw-elf-checks.log`).

### 13.9 Hardware checklist (PENDING — nothing here is validated on a device)

Use a card with the A4-POLISH3 image. In System Menu > Settings > Keyboard Backlight:

1. **Off**: keyboard dark.
2. **Low**: dim but visible.
3. **Medium**: brighter. It should match what Alt+B gave before this batch.
4. **High**: brighter still.
5. **Max**: brightest.
6. Each step changes the light **at once**, before Mic saves.
7. At every level, type a few keys and a command. Input is normal: no drops, no repeats.
8. Pick a level, Mic, power-cycle. The light comes back at that level once the game is up (dark until then). Also test a reboot without a power cut.
9. The TFT brightness does not change when the keyboard level changes.
10. Alt+B at a non-Off level: the light goes off, and Alt+B again restores that level. Alt+B with the row at Off gives the sketch's 127. Re-opening Settings and changing the row takes over again.
11. If no level changes the light: note `KEYBOARD_LIGHT level=… result=…` in the serial log. `ESP_OK` with no light means the installed keyboard sketch predates the `0x01` command (§13.2). Input is unaffected either way.

### 13.10 Status

| Axis | State |
|---|---|
| Protocol | **source-verified** (LilyGO sketch `12f12f8c`); installed keyboard sketch revision **unknown** |
| Range | 0..255 PWM; five levels exposed |
| Host | 191 / 191, mutations 17 / 17 |
| Firmware | built, guards GREEN, 264,464 B free |
| Hardware | **PENDING** (§13.9) |
| Deferred | duty tuning after the device run; syncing an Alt+B toggle back into the row (no read-back exists) |

*Update (A4-CLOSE1, 2026-10-02): hardware **PASS**, recorded in §14.2 — §13.9 steps 1–10 as reported (step 11 is a fallback diagnostic, not needed: the light responded). The duties are accepted as they stand; no tuning was asked for. The PENDING above is the status as this batch wrote it.*

### 13.11 Image for the hardware run

Post-commit build of `abba9c0f` (`a4-polish3-postcommit-*.log`): `native/targets/tdeck/build-a4-polish3/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-polish3-Debug-Launcher.bin`, **1,046,240 B (`0xff6e0`)**, SHA-256 `401541093fe645de03d4ff89ebb555b004f741cd47e4ac58fe08f342b717b6ca`, `FW 4.0.0-alpha4-polish3-debug`, `Git abba9c0ff137`. It is 16 B smaller than the pre-commit build only because its Git string lost `-dirty`. 264,480 B free; Launcher allocation 1,024 KiB (next step 2,337 B away). No SD pack change: the existing A4-END1 pack is the one to use. Not tagged; hardware PENDING (§13.9).

## 14. A4-CLOSE1 — Alpha 4 closeout and the final candidate image (2026-10-02)

A reconciliation batch: it records the hardware results reported since A4-ENH1, classifies every open status line in the Alpha 4 record, separates Alpha 4's own work from the parity divergences it found, and builds one clean final candidate. **No production code changed**: the only source edit is `PROJECT_VER` (`native/targets/tdeck/CMakeLists.txt`). No parity row was fixed, no completed batch reopened.

### 14.1 Baseline

- `main` at **`b242735f`** (A4-POLISH3's post-commit logs, on `abba9c0f`). Present, in order: A4-ENH1 (`fef1f9fe`..`5e0a16eb`), A4-ENH2 (`ad3d8258`..`dc61bc64`), A4-FLASH1 (`a07cae3e`, `1b59e9a8`), A4-POLISH3 (`abba9c0f`, `b242735f`). Tags: `alpha4-ui2-hardware-validated`, `alpha4-end1-hardware-validated`; nothing pushed.
- Untracked: twenty image logs left for their hardware results — `native/core/a4-rc1-*.log` (8), `a4-enh1-image-*.log` (6), `a4-enh2-image-*.log` (6). No other untracked file. They are committed by this batch (§14.10): the A4-ENH1 / A4-ENH2 results are now recorded, and RC1 is superseded.
- Latest flashable image before this batch: A4-POLISH3's, `build-a4-polish3/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-polish3-Debug-Launcher.bin`, 1,046,240 B (`0xff6e0`), SHA-256 `40154109…b6ca`, `FW 4.0.0-alpha4-polish3-debug`, `Git abba9c0ff137` (§13.11).

### 14.2 Hardware results recorded (the user's report, 2026-10-02)

Recorded as reported. Nothing beyond the report is claimed; where a checklist line was not itemised, the table says so. The images' `FW` / `Git` lines were not reported.

**A4-ENH1 (§10.13), the trackball: PASS.**

| Item | Result |
|---|---|
| A. The click toggles WASD Mode at once | **PASS** |
| B. Movement with the new speed model | **PASS** — substantially better; `kTrackballLevels` accepted as it stands |
| Trackball behaviour overall | **PASS** — acceptable on hardware |
| A.3–A.6, B.7–B.14 line by line | not itemised; covered by the two statements above and the whole-image PASS of 2026-10-01 (§11.1) |
| C (Developer entry), D (cheats), E (presets) | the whole-image PASS of 2026-10-01, not itemised; D is also covered by A4-ENH2's cheat PASS below; E is superseded by A4-ENH2's accepted rules |

**A4-ENH2 (§11.14): accepted.**

| Item | Result |
|---|---|
| C. The cheat system | **PASS** — works on hardware (not itemised per cheat) |
| A, B, D. Difficulty, Custom, precedence | **ACCEPTED as implemented** — the user is comfortable proceeding with the current difficulty implementation; exhaustive subjective balance testing is not an Alpha 4 gate |
| Tuning values (`kGameplayRules`, `kRuleChoices`) | **accepted for Alpha 4**; balance stays adjustable in a later version |

**A4-POLISH3 (§13.9), the keyboard backlight: PASS.**

| Step | Result |
|---|---|
| 1–5. Off / Low / Medium / High / Max | **PASS** — every level works |
| 6. Applied at once | **PASS** |
| 7. Keyboard input normal at every level | **PASS** |
| 8. Persistence | **PASS** (reported as "persistence works"; the power-cut and plain-reboot variants were not itemised) |
| 9. TFT brightness unaffected | **PASS** |
| 10. Alt+B | **PASS** — acceptable |
| 11. Serial fallback | not needed (the light responded) |

The duties (0 / 32 / 127 / 191 / 255) are accepted; no tuning was asked for.

### 14.3 Reconciliation: every open status line, classified

Searched across this file, `ALPHA2_PRESERVATION_LEDGER.md`, `GAMEPLAY_INTEGRATION_AUDIT.md`, `ALPHA2_HARDWARE_CHECKLIST.md`, `LAUNCHER.md`, `MOVEMENT_INPUT.md`, `ALPHA3_AUDIO.md` and `PROJECT_HISTORY.md` for: pending, unrun, untested, not tested, hardware pending, retest pending, TODO, FIXME, open, deferred, unresolved, not validated, owed, provisional, RC1 hardware. Classes: **(1)** now PASS / closed; **(2)** intentionally deferred; **(3)** known parity divergence; **(4)** still genuinely untested; **(5)** historical wording, kept as written. A historical line is never rewritten; where its status moved, a dated note was added beside it.

| Hit | Where | Class | Now |
|---|---|---|---|
| A4-ENH1 "hardware PENDING", "PROVISIONAL" | §10 opening, §10.4, §10.13, §10.15; ledger A4-ENH1 paragraph; audit A4-ENH1 rows; `MOVEMENT_INPUT.md` | **1** | PASS (§14.2); table accepted; notes added |
| A4-ENH2 "hardware PENDING", "PROVISIONAL", "balance not final" | §11 opening, §11.4, §11.14, §11.17; ledger A4-ENH2 paragraph; audit A4-ENH2 rows | **1** | cheats PASS, rules accepted (§14.2); notes added |
| A4-POLISH3 "PENDING", provisional duties | §13.3, §13.9, §13.10, §13.11 | **1** | PASS (§14.2); note added |
| §10.14 "more cheats … none is implemented"; dungeon / starvation difficulty out of scope | §10.14 | **1** | done by A4-ENH2 (E-12, E-13) |
| §11.16 "Flash: 3,712 B left" | §11.13, §11.16 | **1** | closed by A4-FLASH1 (already annotated) |
| §11.16 "A4-ENH1's checklist was reported passed as a whole" | §11.16 | **1** | recorded, §14.2 |
| §9.17 items 1–3 (UI4 retest, PARITY1's commit, the RC image) | §9.17 | **1** | done: `48bd39ef`, `544d7dd3`, RC1 `aae348ac`; RC1 now superseded by RC2 |
| "UI4/PRES1 stays hardware-retest pending" | §9 baseline, §9.2 row 13; audit A4-PARITY1 intro | **1** (wording **5**) | PASS §8.22.8; kept as written |
| D-53 "the `Direction?` half stays … HARDWARE PENDING" | ledger D-53, last column | **1** — stale | PASS §8.22.8; note added, row struck |
| D-76 "retest pending" | ledger D-76 | **1** — stale | PASS §8.22.8; note added |
| D-2 "Trackball centre click (GPIO 0) is unbound — should it be Confirm?" | ledger D-2; §8.11; §9.2 row 8 | **1** — stale | answered by A4-ENH1: the click is the Movement Mode toggle (E-8), hardware PASS; note added, row struck. D-1 stays a deferred decision |
| `speaker_segment_frames` in flash (`a3_04b_iram_check` RED) | §8.11, §8.17 | **1** | fixed by A4-PARITY1 (NEW-3); GREEN on every image since |
| D-4 (Mix underground) | ledger D-4 | **1** in software | answered by D-81; its hardware check rides with D-81 (class 4) |
| D-50, D-78, D-79, D-80, D-81 "SOFTWARE FIXED — HARDWARE PENDING" | §9.19; ledger; audit A4-PARITY1 rows | **4** | RC2 session part 3 (§14.7) |
| A4-SAVE2 "hardware validation pending" | §2.4 note, §4 opening, §4.12, §8.11, §8.21, §9.9 | **4** | RC2 session part 4; note added at §4.12 |
| A4-UI3 "pending" | §6.14 A, §6.16; audit A4-UI3 | **4** | RC2 session part 4; note added at §6.16 |
| A4-SAVE3 on the device "never tested" | §5.16, §6.14 B, §9.17; audit A4-SAVE3 | **4** | RC2 session part 5; note added at §5.16 |
| A4-SAVE3 real-DOS round trip | §5.15, §6.14 B, §9.18 5.3 | **4**, optional | optional; without it the notes say "PC bridge host-validated only" |
| The Alpha 4 RC heap capture "owed" | §9.17 | **4** | RC2 session part 6 (required) |
| `SAVE_CATALOG` cold / warm times "never measured" | §9.17 | **4** | RC2 session part 4.9 (serial) |
| D-74 and the arena's single `Cast...` | §8.22.8; ledger D-74; audit A4-UI4 rows | **4**, optional | optional step of the RC2 session; non-blocking (console text only) |
| A4-END1's dead-companion revival "NOT PHYSICALLY TESTED" | §7.15, §7.18, §7.19; ledger D-54 / D-57 | **4**, optional | optional; its automated evidence stands |
| Launcher version on the device; the boot log's `flash=` line | §12.4, §12.12 | **4**, optional | read once from the RC2 session's serial capture |
| Death / resurrection softening | §11.7, §11.16; audit A4-ENH2 item 4 | **2** | waits on D-83 / D-84 |
| Advanced Cheats (teleport, equipment, quest items); a bridge-troll toggle | §11.16 | **2** | not Alpha 4 |
| Difficulty for arena fields, traps, hazards, the ship's hull; God Mode and scripted / status deaths and a Polymorph aimed at the party | §10.7, §10.14, §11.8 | **2** | deliberate scope |
| Syncing Alt+B back into the Settings row | §13.10 | **2** | no read-back exists |
| The credits curtain, the intro's story plates, the title's walking figures | §8.11, §9.2 row 6 | **2** | deferred past Alpha 4 by the user |
| D-1 (WASD in the frontend menus) | ledger D-1, §9.2 row 8 | **2** | the user's deferred decision |
| A Release (non-Debug) build | §9.2 row 10 | **2** | the RC stays Debug with the Developer menu, by the user's decision |
| `-Os` / `CONFIG_LOG_VERSION_2` / table-encoding size candidates | §11.13, §12.8 | **2** | not needed (the free space of §14.9) |
| D-82 – D-89, and the older open rows | ledger §4; §11.15; §9.8 | **3** | §14.5 |
| Alt+S / Alt+L on the title act as Enter on the selected row | §10.14; audit A4-ENH1 item 7 | **3** (native routing quirk, pre-existing) | §14.5 |
| §2.4 "deferred"; §3 "§2.4 stays deferred" | §2.4, §3 | **5** | implemented by A4-SAVE2 (§4) |
| "Nothing committed or tagged" for SAVE2 / SAVE3 / UI3 | §4, §5.16, §6.16 | **5** | committed in `d6457ab1` (with A4-END1); notes added |
| "pending the user's choice (§8.10)" | §8 opening | **5** | decided and implemented, §8.20 |
| The hf1 image's pre-retest wording | §8.19, §8.22 | **5** | retest PASS §8.22.8 |
| "RC1 … hardware session §9.18 pending" | §10.1, §10.14, §11.1; `CMakeLists.txt` comment | **5** | never run; superseded by RC2 (§14.7) |
| §9.18 "proposed" | §9.18 | **5** | retargeted to RC2 as §14.7 |
| Ledger batch paragraphs and totals for Alpha 2 / 3 (Phases 6Y – 7E "pending", H-197 / H-198 "pending", "Alpha 3 RC1 … smoke pending"); ledger "Totals after A4-END1 / A4-UI4 / A4-PARITY1" | ledger | **5** | each later PASS is recorded in its own phase (`ALPHA3.md` §9, `ALPHA2_HARDWARE_CHECKLIST.md`); new "Totals after A4-CLOSE1" added |
| D-66 (not struck; its own text records hardware PASS H-205) | ledger | **5** | Alpha 3 row; content already closed |
| Audit A4-UI2 "Hardware checks … PENDING" | audit | **5** | already annotated PASS |
| TODO / FIXME | the three Alpha 4 documents | — | no Alpha 4 hit; the audit's hits are the Batch 18 / Batch 52 sweeps reporting none |

`ALPHA2_HARDWARE_CHECKLIST.md` holds no Alpha 4 phase: Alpha 4's checklists live in this file (§2.7, §3.7, §4.11, §5.15, §6.14, §7.15, §8.18, §9.18, §10.13, §11.14, §13.9 and §14.7). `PROJECT_HISTORY.md` ends at the Alpha 3 release; an Alpha 4 chapter belongs with the Alpha 4 release, as §20 did for Alpha 3.

### 14.4 Alpha 4 implementation status, by track

Implementation and parity are separate axes here: a track is complete when its own scope is built, tested and committed; the divergences it found are listed in §14.5, not counted against it.

| Track | Implementation | Host | Hardware | Record |
|---|---|---|---|---|
| A4-UI1 chrome | complete (`4c8f8a17`..`9ab2564e`) | goldens, 163 | no separate record; on every later device run (A4-UI2's general UI PASS, §2.8) | §1 |
| A4-UI2 title, death music, save menus | complete | 166 | **PASS** (§2.8; tag `alpha4-ui2-hardware-validated`) | §2 |
| A4-SAVE1 recovery hardening | complete | 167 | **PASS** (§3.6) | §3 |
| A4-SAVE2 manual slots | complete (`d6457ab1`) | 168, mutations 25 / 25 | **untested** (RC2 part 4) | §4 |
| A4-SAVE3 PC save bridge | complete (`d6457ab1`) | 170, mutations 30 / 30 | **untested** on the device (RC2 part 5); DOS round trip optional | §5 |
| A4-UI3 save / load UX | complete (`d6457ab1`) | 172, mutations 25 / 25 | **untested** (RC2 part 4) | §6 |
| A4-END1 ending | complete | 175, mutations 55 / 55 | **PASS** (§7.19; tag `alpha4-end1-hardware-validated`); the revival optional | §7 |
| A4-UI4/PRES1 + hf1 | complete (`48bd39ef`) | 178, mutations 49 / 49 + 6 / 6 | **PASS** (§8.22.8); D-74 optional | §8 |
| A4-PARITY1 | complete (`544d7dd3`) | 181, mutations 31 / 31 | **untested** (RC2 part 3) | §9 |
| RC1 identity | built (`aae348ac`, SHA-256 `67100a51…145a`) | 181 | never run; **superseded** by RC2 | §9.17 – §9.19 |
| A4-ENH1 | complete | 185, mutations 39 / 39 | **PASS** (§14.2) | §10 |
| A4-ENH2 | complete | 189, mutations 49 / 49 | cheats **PASS**, rules **accepted** (§14.2) | §11 |
| A4-FLASH1 | complete | 189 | nothing to run on the Launcher path | §12 |
| A4-POLISH3 | complete | 191, mutations 17 / 17 | **PASS** (§14.2) | §13 |

**No Alpha 4 implementation item is open.** What is open is hardware evidence for four committed tracks (SAVE2, SAVE3, UI3, PARITY1) and the RC heap capture, all in one session (§14.7).

### 14.5 Known parity divergences carried forward (not Alpha 4 work)

Each is documented in the ledger with its evidence, is classified a divergence (not a completed item), and is **not** release-critical: none locks the game, loses or corrupts state, or hides a control, and no project record marks any of them a blocker. None was fixed here.

| Row | What | Class | Note |
|---|---|---|---|
| D-83 | The healer's Resurrect sets HP 1 and skips `resurrect_apply` (SHOPPES `0x16ee`–`0x1703`) | native **and** reference | fix the reference first (`shops.ts`, `shop_parity`'s 4,320 healer rows); changes Original |
| D-84 | The Refuge revives with HP = max, status 'G' only (BLCKTHRN `0x0b90`–`0x0b9d` runs `resurrect_apply`) | native **and** reference | with D-83; unblocks the deferred death-penalty option |
| D-85 | The location-29 trapdoor kills the whole roster, not the party (TOWN `0x0ff9`–`0x103a`) | native **and** reference | recoverable (an inn companion can be raised) |
| D-86 | A dungeon digit key naming an invalid member runs an overworld turn (DUNGEON `0x07c8`–`0x07d1` passes none) | native | found by code read; not yet run |
| D-87 | Crops outside combat add food uncapped (SJOG `0x1A44` / `0x1AB2`: `counter_add(food, 1, 9999)`) | native **and** reference | visible after Max Food |
| D-88 | 1988 combat advances the clock a minute per ten actions (COMBAT `0x0C64`–`0x0C76`); neither port does | native **and** reference | meal / starvation timing after a fight |
| D-89 | The naval OUCH takes `rand(1,8)` from the active member; the notes and the reference say `party_random_damage` | **to adjudicate** against the binary | the notes or the ports are wrong |
| D-82 | A new game lacks INIT.OOL's underworld skiff and four bodies | native **and** reference | the `.OOL` read site is not derived |
| also §11.15 | In Mani Corp's scroll on a living target should print "Not dead!" (DS `0x953c`); `re/notes/kernel-survival.md` §293–295 claims housekeeping during the jail / inn wait (a byte scan shows none) | text; notes | small |
| older | D-8, D-9, D-10 / D-39 residuals, D-13 / D-75's rest of the echo family, D-14, D-15, D-16, D-18, D-38's NPC pose, D-51, D-56's reference half, D-58, D-59; the title's Alt+S / Alt+L acting as Enter | as in the ledger | non-blocking since Alpha 2 / 3 / A4-UI4 |

**Recommended next parity batch (A4-PARITY2 or Alpha 5's first), in order:**
1. **D-89** — adjudicate the OUCH routine against the binary first (cheap, and it decides whether the notes or both ports move).
2. **D-83 + D-84** — one shared `resurrect_apply` for the healer and the Refuge, reference first at the layer `shop_parity` / `quest_parity` pin; then the deferred death-penalty difficulty hook becomes possible.
3. **D-85** — the trapdoor over the party size, not the roster.
4. **D-86** — run the digit key in a dungeon, then drop the stray turn.
5. **D-87** — cap outdoor crops at 9999.
6. **D-88** — the combat clock (touches housekeeping timing after every fight; widest corpus impact, so last).
7. D-82 when the `.OOL` read site is derived; the §11.15 text items alongside.

### 14.6 Intentionally deferred (not divergences, not open work)

Death / resurrection softening (waits on D-83 / D-84); Advanced Cheats; a bridge-troll toggle; difficulty for arena fields, traps and hazards; syncing Alt+B into the row; the credits curtain, story plates and title figures (post-Alpha 4, the user's decision); D-1 (the user's decision); a Release build (the RC stays Debug); the size candidates of §12.8; A3-04H (Alpha 3's deferred heap item).

### 14.7 The remaining hardware session (§9.18, retargeted to RC2)

One session on **the RC2 image** (§14.9), about 2 hours, with a serial capture from power-on through part 6 (`python -m esp_idf_monitor -p COMx -b 115200 --no-reset 2>&1 | Tee-Object -FilePath a4-rc2-capture.log`; afterwards `python native/core/tools/a3_04g_hw_closeout.py a4-rc2-capture.log`). Every Developer value applies only on **Enter**; Mic while a value is open cancels it. The device runs on your own save, so set each step's state with the Developer rows first.

- **0. Before anything.** Back up the card's whole `ultima5/` folder. Packs unchanged since A4-END1 (resource `85b38994…`, tiles `6eb001ed…`, audio `28c1533b…`): nothing to recopy. The identity screen must show `FW 4.0.0-alpha4-rc2-debug`, the `Git` of §14.9, `RES … 2266819B`, `ASSET … 132284B`. **Stop if not.**
- **1. Boot and title** — as §9.18 part 1.
- **2. A4-UI4/PRES1** — nothing owed. Optional: D-74 and the arena's single `Cast...` (§9.18 part 2).
- **3. A4-PARITY1 — required:** §9.18 3.1, 3.2, 3.3 (D-78, D-79), 3.5 (D-81, with D-4), 3.6 (D-50). 3.4 (D-80) optional.
- **4. A4-SAVE2 + A4-UI3 — required:** §9.18 4.1–4.7 and 4.9; 4.8 optional.
- **5. A4-SAVE3 — required:** §9.18 5.1–5.2 (import and export on the device). 5.3 (real DOS) optional.
- **6. Smoke and heap — required:** §9.18 part 6, plus the boot log's `flash=` line (§12.12). The capture is the Alpha 4 RC heap capture (A3 §28.16 trigger 7).
- **7. Power-cycle Continue** — as §9.18 part 7.
- **8. Optional:** the END1 revival (§9.18 part 8).
- **Not owed:** A4-ENH1, A4-ENH2 and A4-POLISH3 (§14.2); RC2 carries exactly their code.

**PASS** needs parts 1 and 3–7 with no crash, reset, watchdog, lock, stale-resource refusal or input-mode corruption; Save, Load and Continue restore the saved state; the heap capture explained (a placement-only trigger does not block). **Report:** the `FW` / `Git` lines, PASS / FAIL per step, the capture.

### 14.8 Final host test baseline

- Fresh host tree `native/core/build-a4-close1` (`a4-close1-host-{configure,build}.log`): 1,576 build steps; the one known w64devkit `stl_uninitialized.h` false positive, 0 project warnings.
- **Full suite: 191 / 191, serial, 164.44 s** (`a4-close1-host-ctest.log`), run after this record's documentation edits. The code is A4-POLISH3's: no production or test file changed since `abba9c0f`, and the one source edit (`PROJECT_VER`) is firmware-only.
- **Preservation, goldens and parity, run again on their own: 54 / 54, serial, 104.81 s** (`a4-close1-preservation-ctest.log`): the Original-mode goldens (`a4_enh1_preservation`, `a4_enh1_preservation_runtime`, `a4_enh2_preservation`, `a4_enh2_preservation_runtime`), the screen goldens (`a4_ui1_chrome_runtime`, `a3_04f_render_runtime`), every `*_parity` corpus (`gameplay_parity`, `quest_parity`, `combat_parity`, `combat_negate_parity`, `advanced_combat_parity`, `magic_parity`, `persistence_parity`, `turn_parity`, `travel_parity`, …) and every TypeScript drift test.
- Not re-run, on purpose: the mutation campaigns (no production or test file changed since each batch's own pass; A4-POLISH3's 17 / 17 is the latest) and the `game/` vitest run (`game/` is unchanged since A4-PARITY1, whose FAIL set was identical to its baseline's 97).

### 14.9 The final candidate (RC2)

Built after commit `24c666e1` (this record and `PROJECT_VER`) from a fresh `--no-ccache` directory: `idf.py --no-ccache -B build-a4-rc2 reconfigure`, then `ninja -C build-a4-rc2 -j 4 all`, first attempt clean, 0 project warnings (`native/core/a4-rc2-fw-{configure,build}.log`); packaged by `python package_launcher.py --build-dir build-a4-rc2` (`a4-rc2-package.log`).

| | |
|---|---|
| **File** | `native/targets/tdeck/build-a4-rc2/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC2-Debug-Launcher.bin` (byte-identical to `build-a4-rc2/openu5_tdeck.bin`) |
| **Size** | **1,046,240 B (`0xff6e0`)** — A4-POLISH3's size exactly |
| **SHA-256** | **`608eb7005dcc88cab6ed8c020e3ad7050c9a711406650492407c7bb760801e95`** |
| **Identity** | `FW 4.0.0-alpha4-rc2-debug`, **`Git 24c666e1ba13`** (HEAD at build; no tracked change; no `-dirty` in the image) |
| **App partition** | 1,310,720 B (`0x140000`, 1.25 MiB): **264,480 B (20.2 %) free**; `check_app_budget.py` OK (above the 128 KiB warning line) |
| **Launcher allocation** | 1,048,576 B (1,024 KiB); the next 64 KiB step is 2,337 B away (the packager's figure) |
| **Firmware guards** | `a3_04a_hotpath_check` **GREEN**, `a3_04b_iram_check` **GREEN**, `a3_04f_image_check` **GREEN**, `check_app_budget` OK (`a4-rc2-elf-checks.log`) |
| **Sections vs A4-POLISH3** | every section equal (`a4-rc2-fw-size-diff.log`): only the version string and the `Git` line differ |
| **Sections vs Alpha 3 RC1** (the release) | flash `.text` +45,696, `.rodata` +11,912; IRAM `.text` +36; internal `.bss` +1,488, `.data` +288 — **+1,776 B internal RAM across Alpha 4** (`a4-rc2-fw-size-vs-a3-rc1.log`), the figure the RC2 heap capture must account for (§14.7 part 6) |
| **Packs** | unchanged since A4-END1 — resource 2,266,819 B `85b38994…d01e`, tiles 132,284 B `6eb001ed…e188`, audio 56,148 B `28c1533b…a6d3`: no SD recopy from any image since A4-END1 |
| **Earlier images** | re-hashed after the build, all unchanged: RC1 `67100a51…145a`, A4-ENH1 `ea8ce9d5…fcee`, A4-ENH2 `d310fa64…8e09`, A4-POLISH3 `40154109…b6ca`, UI4-hf1 `1d57efce…3a44`, PARITY1 `72fc1aee…d306`, Alpha 3 RC1 `4b5b9d1f…53b1` (`a4-rc2-identity.log`) |

Not flashed, not tagged. This is the image for the §14.7 session.

### 14.10 Evidence and commits

- Commit 1 (this record, `PROJECT_VER` = `4.0.0-alpha4-rc2-debug`, the ledger / audit / Launcher / release-notes reconciliation, and the twenty earlier image logs: RC1's eight, A4-ENH1's six, A4-ENH2's six).
- Commit 2: the RC2 build, packaging, guard and size logs (`native/core/a4-rc2-*.log`), §14.9 / §14.11, and the RC2 rows of `ALPHA4.md` and `LAUNCHER.md`.
- Host logs: `native/core/a4-close1-host-{configure,build,ctest}.log`, `a4-close1-preservation-ctest.log`.
- Earlier images untouched: RC1 `67100a51…145a`, A4-ENH1 `ea8ce9d5…ffcee`, A4-ENH2 `d310fa64…8e09`, A4-POLISH3 `40154109…b6ca` (re-hashed after the RC2 build, `a4-rc2-identity.log`).
- Not tagged, not pushed, not flashed.

### 14.11 Release readiness

**Is Alpha 4 ready to be treated as a release candidate, with remaining parity items tracked separately? Not yet — and parity is not the reason.** Every software, preservation and build criterion is met, and the parity divergences are rightly tracked separately. What stops it is hardware evidence: required Alpha 4 checklist items have never been run on a device. RC2 is the image built to close exactly that gap; it becomes RC-ready when the §14.7 session passes, with no further code change expected.

- **Implementation:** complete. Every Alpha 4 track is built, host-verified and committed (§14.4); nothing in Alpha 4's own scope is unfinished.
- **Host:** 191 / 191 serial in a fresh build; the preservation goldens, screen goldens, parity corpora and drift tests 54 / 54; 0 project warnings (§14.8).
- **Preservation:** Original with no cheat and no toggle reproduces the A4-ENH1 / A4-ENH2 goldens bit for bit; the parity corpora are unchanged.
- **Firmware:** one clean image from a fresh directory, no `-dirty`, named by its commit, all guards GREEN, 20.2 % of the partition free, identical in every section to the A4-POLISH3 image the user ran (§14.9).
- **Divergences:** D-82 – D-89 and the older rows are documented, classified and non-blocking (§14.5). They are the next parity batch, not Alpha 4 blockers.
- **Hardware — the one open gate.** Required Alpha 4 checklist items have **never been run on a device**: A4-SAVE2 (§4.11) and A4-UI3 (§6.14 A) — the slot pages; A4-SAVE3's import and export on the device (§6.14 B); A4-PARITY1's fixes D-50, D-78, D-79, D-81 (§9.18 part 3); and the Alpha 4 RC heap capture (§9.18 part 6). They are untested, not failed: no defect is known in any of them, and their host evidence is complete. Under this batch's rule they keep Alpha 4 from being called RC-ready, and certainly from release, until §14.7 passes. If it passes, the project's convention is to promote this exact file unchanged.

**Recommended next step:** run the §14.7 session on the RC2 image (about 2 hours, serial capture on). On PASS, a docs-only batch records it and declares RC2 the Alpha 4 release candidate, then the release, unchanged (as Batch 55 and Alpha 3 did). A FAIL is fixed in its own batch. The parity batch of §14.5 follows, separately, and does not gate it.

### 14.12 Status

| Axis | State |
|---|---|
| Alpha 4 implementation | **complete**: every track built, host-verified and committed (§14.4) |
| Hardware results since A4-ENH1 | **recorded** (§14.2): A4-ENH1 PASS, A4-ENH2 cheats PASS / rules accepted, A4-POLISH3 PASS |
| Record | **reconciled** (§14.3); stale D-2 / D-53 / D-76 closed; historical wording kept |
| Parity divergences | **tracked separately** (§14.5); none blocking; none fixed here |
| Host | **191 / 191** serial (164.44 s); preservation / goldens / parity 54 / 54 |
| Final candidate | **RC2**, `0xff6e0` (1,046,240 B), SHA-256 `608eb700…1e95`, `Git 24c666e1ba13`, 264,480 B free; guards GREEN (§14.9) |
| Release readiness | **not yet RC-ready**: software, preservation and build complete; required hardware checks never run (§14.11). RC2 is the candidate image for that session |
| Hardware still owed | one RC2 session (§14.7): PARITY1, SAVE2 / UI3, SAVE3 on the device, the heap capture |
| Commit / tag / push | committed; not tagged, not pushed, not flashed |

## 15. A4-RC3 hotfix — the music a dungeon plays (2026-10-03)

Found on hardware during the RC2 session, while setting up the dungeon part of §9.18 part 3. A hotfix: nothing outside the music selector's input changed.

### 15.1 Baseline

HEAD `f5dfd6e5` (A4-CLOSE1 RC2 build evidence), tree clean; `PROJECT_VER` `4.0.0-alpha4-rc2-debug`; the A4-CLOSE1 records (§14) and A4-POLISH3's production tree present. Relevant tests green before any edit (`a3_04_music_runtime`, `a3_01_audio_*`, `a3_05_*`, `a4_ui2_death_music_runtime`, `a3_04a/b`, `a3_hf9_*`: 13 / 13).

### 15.2 The hardware observation (RC2, recorded as reported)

Developer teleport into Deceit left the source location's music playing indefinitely. Normal dungeon actions did not correct it. A battle room correctly started the combat song, and leaving the combat restored the stale source music. Leaving the dungeon normally restored the right overworld music; Developer-teleporting back into the dungeon left that overworld music playing again. Separate from A4-PARITY1's D-81: the dungeon command-key behaviour itself passed on hardware.

### 15.3 Root cause

It is **not** a Developer-teleport defect, and not a missing `sync_music()` call (unlike the Refuge defect of §2.2). The tail of `AlphaRuntime::handle()` already calls `sync_music()` after a teleport (`synchronize_after_debug`, then the tail). What it computed was wrong:

- `sync_music()` built its `LocationMusicInput` from `game_.position` alone;
- a dungeon session never rewrites `GameState::position` — it keeps the **surface return cell** by design (`dungeon_input_test` D-LOC-2c; `EnterDungeon` in `dungeon_orchestration.cpp` sets `DungeonState` and leaves `position` alone), and the HUD caption already reads the session instead (`hud_dungeon_bands`);
- so an underground party "stood" at its entry cell: Britannia gives Britannic Lands, a castle gives The Missing Monarch, and the dungeon range (locations 0x21 – 0x28 → Halls of Doom) was never reached;
- combat does not save a music context: `pos.in_combat` wins while the arena is up, and afterwards the **same derivation** runs again, so the battle exit restored the stale surface song for the same reason.

Why nothing caught it: A3-04's location test G5 ("a dungeon (loc 0x21) → Halls of Doom") sets `position.map = 0x21` by hand — a state the game never produces — so the pure rule was proven and the wiring was not (the "test builds the state instead of reaching it" class).

Scope: **every** way into a dungeon (normal Enter, Developer teleport, loading a dungeon save), not only the Developer route. The user's report only exercised the Developer route; the new test's N-series shows the normal entry fails identically on RC2.

### 15.4 Normal path vs Developer teleport

| Path | Position the selector read (RC2) | Music |
|---|---|---|
| surface → dungeon, normal (E) | surface cell, stale | surface song (wrong) |
| surface → dungeon, Developer | surface cell, stale | surface song (wrong) |
| dungeon → surface, normal (klimb) | `exit_dungeon` writes the surface position | correct |
| dungeon → surface, Developer | the destination | correct |
| town / castle / underworld teleport | the destination | correct |
| load of a dungeon save | the surface cell of the save | surface song (wrong) |
| dungeon combat exit | the surface cell | surface song (wrong) |

Developer teleport takes the same `handle()` tail as every key; no teleport-specific music path exists or was added.

### 15.5 The fix

One block in `AlphaRuntime::sync_music()` (`alpha_runtime.cpp`): when `hud_dungeon_bands(dungeon_, context_.dungeon)` is active, the location is the session's own dungeon id (33 – 40) and the transport tile is ignored (a dungeon has no ship; a Developer teleport off a frigate must not carry the ship's song in). Everything else is untouched: the priority list, the combat / victory branch, the de-duplication in `AudioService::sync_music()` (a repeated request never restarts a song), mute and volume 0 %. No hard-coded Deceit / dungeon selector; the destination is read by `music_context_for_location` like any other.

### 15.6 Tests

New `a4_rc3_dungeon_music_runtime` (34 checks; the A4-UI2 harness: real `AlphaRuntime` on the real Board, the real audio pack, a recording backend; **raw keys only, including the Developer menu**):

| Series | Covers |
|---|---|
| N | normal Enter into Deceit, ordinary actions, Alt+S, climb out, Alt+L back into the dungeon save |
| T | Developer teleport surface → dungeon, dungeon → Britannia / Underworld, → castle → city → Britannia, back into the dungeon; exactly one start, while the menu is still open |
| C | **the hardware sequence**: surface song → teleport → Halls of Doom → real Deceit room fight (Engagement) → leave by the board edge → Halls of Doom, never the surface song |
| S | Deceit → Deceit → Despise starts nothing (same effective context); a frigate source |
| A | muted (silent, context follows, unmute plays the destination); Music Volume 0 % (same, raised from the System Menu); a stock pack (no `start_music` / `stop_music`, context still follows) |

- **RED-first, against the unmodified RC2 `alpha_runtime.cpp`: 18 / 34, 16 RED** — N2, N3, N5, T2, T3, T5, C1, C4, C5, S1, S3, A1 – A4, A6: every step of the hardware report, plus the normal entry and the load.
- **GREEN with the fix: 34 / 34.**
- Mutation: removing only the `transport_tile = 0` guard fails S3 alone (33 / 34).
- The test's Britannia teleport enters at explicit grass coordinates: the picker's default entrance for Britannia is the sea at (0,0), which it refuses (existing picker behaviour, not changed here).

### 15.7 Regressions and host suite

- Audio set unchanged and green: `a3_01_audio_contract`, `a3_01_audio_runtime`, `a3_04_music_runtime` (G5 still passes: the pure rule is unchanged), `a3_04b_perf_runtime`, `a3_05_audio_mute`, `a3_05_audio_controls`, `a4_ui2_death_music_runtime`, `a3_hf9_refuge_cadence_runtime`, `a4_end1_ending_runtime`.
- **Full suite: 192 / 192, serial, 156.06 s** (`native/core/a4-rc3-host-ctest.log`; host tree `build-a4-close1` rebuilt, 146 steps, 0 warnings, 0 errors). 191 + the new target. No expectation was changed.
- Not run: the mutation campaigns of earlier batches (the only production file touched is `alpha_runtime.cpp`, one block) and the `game/` vitest run (`game/` unchanged).

### 15.8 What did not change

Normal dungeon exit, combat music selection, death / Refuge music, the ending, save / load formats, SFX, mute and volume behaviour, gameplay and parity rules. The change turns the dungeon's song on where the selector's own table always said it should be (ALPHA3_AUDIO.md §17: "Dungeon entry/exit — a location range change"; checklist E "Enter any dungeon: Halls of Doom plays").

### 15.9 Hardware status

**The pre-fix observation is recorded in §15.2. The fix is NOT hardware-tested; do not mark it PASS until the RC3 image is.** User-reported acceptance (not itemised; no serial timing or heap values were captured): A4-SAVE2 / A4-UI3 slot behaviour, A4-SAVE3 device import / export, power-cycle Continue and general smoke. The real-DOS SAVE3 round trip stays optional.

- **Alpha 4 RC heap capture: none exists** in the tree (only Alpha 3's `a3-rc1-hw-capture.log`). It remains the one open evidence item. After flashing RC3: `python -m esp_idf_monitor -p COMx -b 115200 --no-reset 2>&1 | Tee-Object -FilePath a4-rc3-capture.log`, run §14.7 part 6 (walk / bump with music, volumes and mutes, one fight, Mix, Alt+S / Alt+L, System Menu x10, New Journey and Continue, Developer > Diagnostics > Audio/render stats `und=0 hw=0 miss=0`), stop, then `python native/core/tools/a3_04g_hw_closeout.py a4-rc3-capture.log`.
- **A4-PARITY1 hardware, user-reported (2026-10-03; on the RC2 image, before the music fix; not itemised, nothing captured):** **PASS** for D-81 (dungeon command keys), D-79 (the worn crown), D-50 (the well's case-folding) and D-78 (the palace crown gate), with one nuance on D-78: the Mani cast **executed and was not absorbed** (the defect was `Absorbed!` for a party carrying the crown), but the literal `Success!` text of §9.18 3.1 was **not observed on a full-health target**. The fix under test is the possession gate, and an executed, unabsorbed cast is what it changes; the exact result text was not captured, so it is not claimed. Recorded as PASS with that caveat. This is a user report, distinct from per-line serial evidence.

### 15.10 Targeted hardware retest

1. Start on the surface with identifiable surface / location music.
2. Developer teleport > Deceit: dungeon music begins immediately.
3. Several ordinary dungeon actions: it stays.
4. Enter a dungeon battle room: combat music starts.
5. Leave the combat: dungeon music returns, not the old surface track.
6. Leave the dungeon normally: the correct surface music starts.
7. Developer teleport back into the dungeon: dungeon music starts immediately again.
8. Repeat one teleport while music is muted (Alt+Shift+M): silence stays; unmute gives the destination's music.
9. Repeat at Music Volume 0 %: raising the volume gives the destination's music.

Also worth one try: (E)nter Deceit from its mouth in Britannia — the same defect, no Developer menu.

### 15.11 Candidate

RC2 (§14.9) is **superseded by RC3** (`4.0.0-alpha4-rc3-debug`): RC2 plus this one fix. RC2's records are unchanged. The RC3 image identity is in §15.12.

**Release readiness, as of this amendment.** §14.11 named four open hardware items. By the user's reports they now stand as: A4-SAVE2 / A4-UI3 slots, A4-SAVE3 device import / export, power-cycle Continue and smoke — **user-reported acceptable**; A4-PARITY1 D-81, D-79, D-50 — **user-reported PASS**; D-78 — **user-reported PASS with the `Success!`-text nuance of §15.9**. What is left before Alpha 4 can be called RC-ready: **(1)** the §15.10 retest of the music fix on the RC3 image (the fix is not hardware-tested), and **(2)** the Alpha 4 RC heap capture, which does not exist and which no report replaces. The PARITY1 and SAVE results were obtained on RC2; RC3 differs from RC2 by one selector block (flash `.text` +44 B), so they are not repeated unless the retest shows a regression. The distinction between per-line captured evidence and user-reported acceptance is kept: no serial timing or heap value is claimed for any of them. Still not RC-ready until (1) and (2).

### 15.12 The RC3 candidate image

Built after commit `6b6ab8a4` (the fix, the test, `PROJECT_VER`, §15) from a fresh `--no-ccache` directory: `idf.py --no-ccache -B build-a4-rc3 reconfigure`, then `ninja -C build-a4-rc3 -j 4 all`, first attempt clean (1,182 steps), 0 project warnings (`native/core/a4-rc3-fw-{configure,build}.log`); packaged by `python package_launcher.py --build-dir build-a4-rc3` (`a4-rc3-package.log`). A pre-commit build of the same tree (`build-a4-rc3-pre`, `a4-rc3-pre-fw-*.log`) hit one GCC internal compiler error (segmentation fault) inside ESP-IDF's own `esp_lcd_panel_rgb.c` on its first attempt — the known third-party toolchain flake (`a4-rc3-pre-fw-build-attempt1-gcc-segfault.log`); resuming the same directory completed. That image is not the candidate.

| | |
|---|---|
| **File** | `native/targets/tdeck/build-a4-rc3/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC3-Debug-Launcher.bin` (byte-identical to `build-a4-rc3/openu5_tdeck.bin`) |
| **Size** | **1,046,288 B (`0xff710`)** — RC2 + 48 B |
| **SHA-256** | **`9fd7fc8b5669c206efdc43d9588fe845c9c63f233c74e54c4e89f15803c9d542`** |
| **Identity** | `FW 4.0.0-alpha4-rc3-debug`, **`Git 6b6ab8a4c378`** (HEAD `6b6ab8a4c3787bd1d6f1781858a37c2f4b0c6c11`; no tracked change; no `-dirty` anywhere in the image) |
| **App partition** | 1,310,720 B (`0x140000`, 1.25 MiB): **264,432 B (20.2 %) free**; `check_app_budget.py` OK |
| **Launcher allocation** | 1,048,576 B (1,024 KiB); the next 64 KiB step is 2,289 B away |
| **Firmware guards** | `a3_04a_hotpath_check` **GREEN**, `a3_04b_iram_check` **GREEN**, `a3_04f_image_check` **GREEN**, `check_app_budget` OK (`a4-rc3-elf-checks.log`, all exit 0) |
| **Sections vs RC2** | flash `.text` +44 B only; `.rodata`, IRAM, internal `.data` / `.bss` and PSRAM unchanged (`a4-rc3-fw-size-diff.log`) — the +1,776 B internal RAM across Alpha 4 that the heap capture must account for (§14.9) is unchanged |
| **Packs** | unchanged since A4-END1: no SD recopy from RC2 |
| **Earlier images** | re-hashed after the build, all unchanged: RC2 `608eb700…1e95`, RC1 `67100a51…145a`, A4-POLISH3 `40154109…b6ca` (`a4-rc3-identity.log`) |

Not flashed, not tagged, not pushed. **This is the image for the §15.10 retest**; if it passes, RC3 replaces RC2 as the image for the remaining §14.7 items (the RC heap capture, and the PARITY1 crown / well steps not yet recorded, §15.9).

### 15.13 Evidence and commits

- Commit 1 `6b6ab8a4`: the fix, `a4_rc3_dungeon_music_runtime` and its CMake target, `PROJECT_VER`, §15, the host logs (`native/core/a4-rc3-{host-build,host-ctest,red-first,green}.log`).
- Commit 2: the RC3 build, packaging, guard, size and identity logs (`native/core/a4-rc3-{fw-*,pre-fw-*,elf-checks,pre-elf-checks,package,identity}.log`), §15.12 – §15.13, the RC3 rows of `ALPHA4.md` and `LAUNCHER.md`.
- Not tagged, not pushed, not flashed.

## 16. A4-PARITY2 — the post-Alpha-4 parity cleanup (2026-10-03)

One consolidated parity batch for the divergences carried forward in §14.5: D-89, D-83 / D-84, D-85, D-86, D-87, D-88, D-82 and the two small §11.15 items. **Parity only**: no feature, cheat, difficulty option or presentation change. Each fix is proven from the 1988 binary first, made in the TypeScript reference first where the reference is wrong, then in native, with RED-first tests, a mutation campaign and a token-level account of every corpus that moves. Order is §14.5's own: D-89, D-83 / D-84, D-85, D-86, D-87, D-88, D-82, then the small items. One logical commit each; nothing is pushed.

### 16.1 Baseline (before any production change)

| | |
|---|---|
| **HEAD** | `1783256f2af85136c183434388cbab98262b440e` (Alpha 4 RC3 records; RC3 image `4.0.0-alpha4-rc3-debug`, `Git 6b6ab8a4c378`, SHA-256 `9fd7fc8b…d542`) |
| **Tags at HEAD** | none (the Alpha 4 tags are `alpha4-ui2-hardware-validated`, `alpha4-end1-hardware-validated`; RC2 / RC3 are untagged by decision) |
| **Working tree** | clean except the untracked `native/core/a4-release-ctest.log` — a **truncated** earlier release-ctest run (it stops at test 182 of 192). Left untouched, as instructed; it is not evidence of anything |
| **Host suite** | fresh configure + build in `native/core/build-a4-parity2-baseline` (1,596 steps, 1 warning line: the known GCC 16 `-Wstringop-overflow` false positive in w64devkit's own header), then serial `ctest`: **192 / 192 passed, 265.93 s** (`native/core/a4-parity2-baseline-{configure,build,ctest}.log`) |
| **TypeScript** | `tsc --noEmit` clean (`a4-parity2-baseline-tsc.log`); the vitest baseline is in §16.2 |
| **Release candidate** | RC3 is the current Alpha 4 candidate (§15.12); its hardware status is unchanged by this batch until a new image is built and run (§16.12) |

The baseline build directory is kept pristine at HEAD so that every RED-first claim below can be re-run against it.

### 16.2 TypeScript baseline

`tsc --noEmit` clean. The whole `game/` vitest run (`npx vitest run`, 7,735 tests: 7,532 passed, **97 failed**, 106 pending) has the **identical FAIL set** to the saved A3-HF10 baseline (97 assertion-level failures, the environment-bound set the toolchain memory warns about; `a4-parity2-baseline-ts-compare.log`, set saved as `native/core/a4-parity2-baseline-ts-fails.txt`) plus the same 18 suites that fail to *load* (a missing git-ignored input or an unparsable generated file). Every later TypeScript comparison in this section compares **that set**, never a total.

### 16.3 The investigation (read-only, before any change)

Nine reconciled derivations, one per item, sit in `native/core/a4-parity2-findings/` (`*-FINAL.md`, with the two or three independent blind reports each was reconciled from, `*-A/B/C.md`). Every binary fact in this section was re-disassembled by the reconciler of its item with `re/tools/dis16.py`, with positive controls, and the three most consequential agreed claims of each were attacked directly. They are the evidence basis of §16.4 onward; each item's section below quotes only what its fix depends on.

**Tooling correction found by all nine.** The overlay near-call base table in `re/tools/callers_banda.py` — and the table quoted in this batch's brief — is **wrong for 10 to 15 overlays**. Fitted by the kernel-prologue landing test (and confirmed by the kernel thunk targets), the bases are: TOWN, MAINOUT, **DUNGEON 0x81D0**; INTRO 0x81C0; **OUTSUBS**, NPC, SHOPPES, **COMBAT**, **BLCKTHRN**, ENDGAME, LOOKOBJ, DNGLOOK 0xA290; CMDS, SJOG, TALK, **CAST 0xBF80**; CAST2, ZSTATS, FONT, **COMSUBS, SHOPPES2, SHOPPES3 0xE1E0**. The stock tool therefore misses most callers of a kernel routine (`callers_banda.py 0x7ef6` prints one of the four callers of `resurrect_apply`). The tool is **not changed** by this batch (it is not production code and the brief said to record, not fold in); every census below was redone with fitted bases. Recorded as D-97 (§16.13).

**Evidence provenance caveat (D-82).** The tree's `original/` set is *The Exodus Project "Ultima V Upgrade 1.0" (2001)*: `FONT`, `INTRO`, `MAINOUT`, `TOWN`, `DUNGEON`, `ENDGAME` and `ULTIMA.EXE` are the patched files (`Files.txt`, `History.txt`: music changes only), `OUTSUBS`, `CAST2`, `CMDS`, `SJOG` are the 1996 originals. Everything below that depends on those files is a derivation from the Upgrade's code; `History.txt` documents no save or object-table change, so it is almost certainly the 1988 logic, but a pristine-1988 diff is impossible here.

### 16.4 Scope discipline: what this batch does and does not fix

The investigations found, besides the scoped items, further divergences. **None is fixed here** (they would widen the batch, move other corpora, or need a decision); each is recorded in §16.13 as its own row with its evidence, and none changes what an Original-mode player sees unless its own batch is run. The only exceptions are changes a scoped item *directly requires*, listed per item.

### 16.5 D-89 — the naval "OUCH!" (fixed)

**Question.** Native rolled `rand(1,8)` on the active member for a skiff or ship bumping a cactus; the notes and the reference said the original calls `party_random_damage`. Adjudicated against the binary (`a4-parity2-findings/D89-FINAL.md`, two blind derivations reconciled).

**What the original does.** MAINOUT.OVL has **one** blocked-move tail (`0x0312–0x0347`), shared by foot, horse, carpet, skiff and a rowed frigate: `0x0322` prints `Blocked!` (always), `0x0329 cmp [bp-6],0x2f` (the terrain word of the destination), `0x032f` prints `OUCH!`, **`0x0336 call 0xffffa8d8` = kernel `0x2AA8` `party_random_damage()`** (no arguments), else `0x033c` the beep (mutually exclusive); then `K:1B16` flushes the keyboard and the function returns 0 (no clock tick). `K:2AA8` draws **one `rand(1,8)` per slot `i < min(party_size, 6)` whose status is not `'D'`**, in slot order, each draw immediately followed by that member's `apply_damage` (`K:2A52`: `HP -= n`, death at `HP <= 0` — HP equal to the damage kills — writes HP 0 and `'D'` and clears the active character if it was the one that died, then `K:2900` redraws the party panel). `'S'` and `'P'` are damaged and keep their status. There is **no separate naval routine**: "naval" is a skiff (`0x28–0x2B`) or a sails-down frigate (`0x24–0x27`, which also prints `Rowing!` first) pushing into terrain `0x2F`; a ship under sail never reaches OUCH (it takes `COLLISION!` / `BREAKING UP!` with `rand(1,30)` hull damage). From seed `0x0C4C` the draw stream is 4, 8, 3, 8, 5, 3. (Not reachable in the stock map: all 14 Britannia cacti are ringed by desert, UNDER.DAT has none; the fix is for parity, not for a reachable bug.)

**Who was wrong.** The notes (`re/notes/cactus-ouch-acta.md`, `kernel-survival.md`) were **right**. The TypeScript reference was half wrong (its foot path and `partyRandomDamage` were right; `resolveNavalStep` rolled once on the active member, member 0 when none was selected, with no `'D'`, no active-member reset, no skipping of the dead). Native was wrong in `Runner::naval_step` only (its foot path was right). The ledger's D-89 described the defect correctly but called it "to adjudicate"; it is now measured.

**Fix.** Reference first (`game/src/core/game.ts`, `resolveNavalStep`): the inline roll becomes `partyRandomDamage(this.state, this.rand)` followed by one `party-changed` event (the panel redraw inside `K:2A52`; the foot path already emitted it). Native (`native/core/src/commands.cpp`, `Runner::naval_step`): the inline roll becomes `party_random_damage(c.game, rand)` plus `PartyChanged` — exactly the foot path's two calls. God Mode keeps its contract (the draws are made; only the HP write is skipped), so A4-ENH2's X6 is unchanged; its mutant X7 was retargeted (the line it anchored on no longer exists).

**Evidence.**
- RED first: `game/tests/a4-parity2-d89-naval-ouch.test.ts` **11 of 12 RED** against the unfixed reference (the control was green); `a4_parity2_d89_naval_ouch` **9 of 10 RED** against unfixed native (N8, the controls, green) — `a4-parity2-d89-ts-red.log`, `a4-parity2-d89-red.log`. Both → GREEN (12/12, 10/10, `a4-parity2-d89-green.log`).
- The tests separate the three failure shapes: *active member only* (HP vector and final seed of a 3-member party), *whole party* (the same, plus `party_size` bounds, six members, `active = 0xFF`), and *wrong draw count / order* (a dead member is skipped **without** a draw — slot 2 takes the second draw; `HP == damage` kills; the dying active member clears the active index). Controls: grass is `Blocked!` + beep with no OUCH and no draw; a sailing ship into a cactus never reaches OUCH; naval and foot give identical HP vectors from the same party and seed.
- **Corpus.** `fixtures/movement-flow.txt` is the only corpus that sees this path (one-member parties, so the helper swap itself changes **zero** bytes — `generate-movement-flow-fixtures.ts --check` passes with the helper alone). Adding the `party-changed` event drifts it (control: `movement flow drift` thrown before regeneration); the regenerated file differs from HEAD in **exactly 56 rows / 187 hash tokens of 257,544** (the 187 naval-OUCH snapshots of transports 32–43; no row changes width; `a4-parity2-d89-corpus-diff.log`) — each changed token is the event-list hash gaining `party-changed` after `message:OUCH!`; seeds, HP and positions are unchanged. Every other corpus is byte-identical (`commands.txt` and `transport-flow.txt` have foot rows or board/exit only).
- Native: serial suite **193 / 193** (146 s; +1 target); `tsc --noEmit` clean; the whole `game/` vitest FAIL set **identical** to the baseline (97 + 18 load failures; 7,747 tests, +12).
- Mutations (`tools/a4_parity2_mutation_check.py D89`, `a4-parity2-d89-mutation.log`): **14 / 14 killed** (9 native, 5 reference: active-member-only roll, no panel event, draw-before-skip, loop bound 5, roster instead of party, God Mode before the draws, an extra beep, `HP < damage`, a dying active member left selected), the restored tree GREEN; the retargeted A4-ENH2 mutant X7 is KILLED by X6 (`a4-parity2-d89-enh2-x7.log`).

**Not changed (adjacent, recorded in §16.13).** The ports still run a naval turn (clock minutes, wind roll, poison tick) after a *blocked* rowed step; the binary does not (`MAINOUT 0x0C30` skips `advance_clock` on a 0 return) — D-92. A naval cactus inside a town is layer-ungated in both ports; the binary (`TOWN 0x083A`) prints `Blocked!` and beeps there — D-93. The tests therefore never assert the clock, and pin the seed as "damage draws, then the turn" (measured on a grass obstacle).

### 16.6 D-83 + D-84 — the healer and the Refuge run `resurrect_apply` (fixed; changes Original on purpose)

**Question.** The notes (and §14.5) said the original's healer and Refuge run the resurrection routine and the ports do not. Re-derived independently (`D83D84-FINAL.md`; the reconciler also executed the real CAST2 and kernel bytes in its own 8086 interpreter over 160,000+ inputs, 0 mismatches against the arithmetic below).

**What the original does.** One routine, **CAST2.OVL `0x05e0`** `resurrect_apply(member, mode)` (kernel stub `0x7ef6`), with **exactly four callers**: CAST `0x10f3` (In Mani Corp spell, mode 0), CAST `0x12ee` (In Mani Corp scroll, mode 1), **SHOPPES `0x16f5`** (healer, mode `0xff`), **BLCKTHRN `0x0b95`** (Refuge, mode `0xff`). For a member whose status byte is `'D'`: status `'G'`; HP = 1; MP by class (`'A'`, `'M'` = INT, `'B'` = INT >> 1, every other class byte untouched); **if karma (unsigned byte `DS:0x5888`) < 98 (`0x62`) the experience becomes the low 16 bits of `trunc(XP × karma / 100)`**; then, **for every karma**, level = 1 + bitlength(`XP / 100`) (signed `idiv`, a loop, no table, no cap) and max HP = 30 × level. No RNG, no karma write. A non-`'D'` member: nothing happens (it prints `Not dead!` when mode ≠ 0). The spell and the scroll stop there (HP 1). **The healer (`0x16f8–0x1703`) and the Refuge (`0x0b98–0x0b9d`) then copy the member's NEW max HP over HP** — the Refuge's copy is unconditional (even for a non-`'D'` member) and writes no status — and the Refuge runs with the karma the party **died with**: the floor of 75 is applied at `0x0bfd`, after the loop. So the §14.5 wording "the healer sets HP 1 and skips `resurrect_apply` (SHOPPES `0x16ee–0x1703`)" described the *port* but cited the *original's* addresses; at those addresses the original calls the routine.

**Who was wrong.** Both ports, for the healer and the Refuge; both were already byte-exact for In Mani Corp (XP ≤ 32,767 — XP is a signed word in the routine, unreachable above the in-game cap of 9,999; recorded, not modelled). Notes corrected with dated superseding paragraphs: `re/notes/death-resurrection-audit.md` (its "exact" verdict for the Refuge and its omission of the XP cut / level / max HP), `re/notes/blackthorn.md` and `re/deliberate-divergences.md` (the revive byte was never "open": `'G'`), `re/notes/shops.md` §5, and the `Revive Party` cheat's comment (the ending's revival has no XP cut; the cheat deliberately follows it, not the healer).

**Fix.** Reference first: `shops.ts` `healerHeal` and `blackthorn.ts` `partyRefuge` call the existing byte-exact `applyResurrect` (only for a `'D'` member in the Refuge) and then set `currentHp = maxHp`. Native: the Resurrect body of `apply_target_spell` is extracted into one shared `resurrect_apply(CharacterState&, uint8_t karma)` (`magic.cpp` / `magic.h`), used by the spell, `healer_heal` and `resolve_refuge`. Order inside the healer is the binary's (the `'D'` gate, the payment, the Falsehood drain, then the routine). The cheat, the ending's revival and the In Mani Corp paths are untouched.

**Evidence.**
- RED first: `a4-parity2-d83-d84-resurrect.test.ts` **14 of 16 RED** on the unfixed reference (the two controls green); `a4_parity2_d83_d84_resurrect` **7 of 8 RED** on unfixed native; both GREEN after (16/16, 8/8). The tests carry an independent model of the binary's arithmetic and check **198 karma × XP cells** (karma 0, 1, 50, 74, 75, 96, 97, 98, 99, 100, 255 × XP at every level boundary ±1 and 0, 9999) through the healer **and** the Refuge, plus the worked rows of the report (INIT.GAM's Avatar at karma 75 → XP 112, L2/60, HP 60, MP 15; the Refuge's A / B / M rows; death karma 74 cuts XP 100 to 74 — the floor comes after the loop; XP 450 with a stored level 2 becomes L4/120 at karma 99; XP 0 drops a L5/150 member to L1/30), the threshold (97 cuts, 98 does not), truncation, MP by class, party-only revival, a synthetic mixed party, and the **agreement** test: the spell, the healer and the Refuge leave identical status / XP / level / max HP / MP and differ only in HP (1 versus the new max).
- **Corpora** (control first: `--check` on the unmodified reference reproduced the stored digests byte for byte; on the patched reference it drifted, as it must). `shop_parity`'s `helpers.txt` (generated, git-ignored): **130 rows of 70,472 / 390 of 550,720 tokens** change (only the three character-digest columns; sha-256 `547a72b8…eba9` → `14ad4555…2d8b`); `shop_flow`'s `flow.txt`: **658 rows (98 healer-walk sequences, shops 33–39) / 658 tokens** (`35f2e933…d41f` → `b450fc72…a02a`); both digests equal the ones the investigation predicted from an independent regeneration, and every other row is byte-identical (`a4-parity2-d83-corpus-diff.log`). `quest_parity` is live: native-before vs native-after over the same 5,377 inputs differs in **15 cases, and only in `characters[].currentMp`** (39 step states + 15 finals; the Avatar's MP 0 → 20; `a4-parity2-d83-quest-diff.log`) and `ctest quest_parity` passes with the reference. `gameplay_parity`, `magic_parity`, `command_parity` and every other corpus are unchanged.
- Existing expectations that moved, each with its reason: TS `shops.test.ts` (the healer now leaves the Avatar at XP 112 / HP 60 / MP 15, gold 280); native `batch7b` E38 (a member with XP 0 returns at level 1 / max 30 / HP 30, not the stored 40); `a4_enh2_preservation` B4 — the golden is **re-recorded for the healer block only and the old value is still reproduced**: the test now re-creates the pre-D-83 healer result around the same call and the old hash `0xbe90d6bb09040045` comes out, which proves the In Mani Corp and inn blocks are bit-identical to pre-A4-ENH2; the new value is `0x6d09cbe2f90a0714`.
- Suite **194 / 194** serial (132 s); `tsc --noEmit` clean; the whole `game/` FAIL set **identical** to the baseline (7,763 tests, +28 over the baseline).
- Mutations (`a4_parity2_mutation_check.py D83`): **22 / 22 killed** (14 native, 8 reference: the healer or the Refuge skipping the routine, HP left at 1, HP copied before the routine, the floor of 75 before the loop, the threshold at 97 / 99, level recomputed only when the cut ran, max HP not recomputed, rounding, a bard's MP, status forced on a living member, HP set only for revived members, roster instead of party, a level loop off by one), restored tree GREEN (`a4-parity2-d83-mutation-part{1,2}.log`).

**Deferred death-penalty option.** §14.6 deferred the "soft death penalty" difficulty feature until this fidelity fix. The hook now exists (one shared routine, three callers); the option is **not** added here (the brief: parity only).

**Not changed (adjacent, §16.13).** XP as a signed word (unreachable); the In Mani Corp casting ceremony belongs inside the routine on success only (effect 8 for the spell, 6 for the scroll), where the ports play a generic cast ceremony (D-101); a cancelled "On who" pick on the device refunds what the binary has already spent (D-101).

### 16.7 D-85 — the location-29 trapdoor kills the party, not the roster (fixed)

**What the original does.** `post_turn` (TOWN.OVL `0x0f02`) fires on a standing test: any turn-consuming town command with the tile under the party equal to `0x8c` and the transport tile not `0x14`/`0x15` (the carpet flies over). The kill branch is taken only for location 29 (Stonegate; KEEP.DAT record 6, ring of `0x8c` around (15,15)); every other trapdoor location falls one floor. The kill loop `0x0ff9–0x103a`: `0ff9 mov al,[0x585b] / or ax,ax / jne / jmp 0x10c7`, then per member `100b mov word [si],0` (HP), `100f mov byte [di],0x44` (`'D'`), a noise burst (`0x1012`) and the panel redraw (`0x1021`), `i++`, and `cmp [bp-4], [0x585b] / jb` — **the bound is the byte `[0x585b]` = party size, re-read each pass, unsigned**. It touches roster indices `0 .. party_size-1` only, every one regardless of status (an already dead member is rewritten to HP 0; asleep and poisoned members die), with no cap at 6; companions at an inn (index ≥ party size) are not read, not written, get no sound and no redraw. Before it, the branch zeroes the whole 32×32 map to lava (`0x8F`), blanks the viewport and zeroes the 32-slot actor table; the kill tail draws no RNG. After it nothing is "game over": the turn finishes, the kernel party check returns −1 and the (party-bounded) Refuge runs. INIT.GAM and SAVED.GAM hold **16** roster records with party size **3**: the old behaviour killed 13 never-recruited or inn-parked characters in one stroke (the D-85 note's "recoverable" understates it).

**Who was wrong.** The reference (`game.ts` `stonegateLavaWipe`, looping `state.characters`; its comment cited `0x0ff6–0x1037` for a roster loop that in fact loops over `[0x585b]`) and native (`quest_world.cpp` `quest_trapdoor`, `character_count`) — and the device's trapdoor audio (`alpha_runtime.cpp`, one noise burst per `character_count`; the comment above it already said "[0x585b] members"). Nothing saw it: every test had roster == party.

**Fix.** Reference first: the wipe iterates `characters.slice(0, partySize)`. Native: the loop bound is `party_size` (clamped to `character_count`, which guards a short synthetic roster or a corrupt `party_size`; the original has no clamp). Device: the trapdoor fall's burst count is `party_size`. The write stays direct (HP and status only; God Mode does not apply — the original is a scripted write, §11.7), the lava wipe, the object erase and both events are untouched.

**Evidence.**
- RED first: `a4-parity2-d85-trapdoor-party.test.ts` **5 of 7 RED** on the unfixed reference (2 controls green), later 8 tests with a synthetic "no cap at six" case; `a4_parity2_d85_trapdoor_party` **5 of 8 RED** on unfixed native (D2 — the stock save shape — shows **16 dead, 0 alive**, want 3 / 13); `a3_03_sfx_runtime` **W1b RED** (burst count 5, want 2). All GREEN after (8/8, 8/8, 47/47). The tests cover: a party of 2 in a roster of 5 (inn companions byte-identical, including an asleep and a poisoned one), the stock shape (party 3 / roster 16), party == roster, party 6 of 16 (exactly six), party 1 of 3, no status filter inside the party ('S', 'P', a dead member with HP 7) with an outside 'S' untouched, only HP and status written, party size 0 / party larger than the roster / 17 over 16 (clamped, no out-of-bounds write), the lava wipe + object erase + one `MapChanged` + one `PartyChanged` + `PartyKilled`, and the control that another location kills nobody.
- **Corpora.** `turns.json` / `turns.inc` (the only corpus with trapdoor tiles; its probe hook kills nobody) is **byte-identical**: `generate-turn-fixtures.ts --check` passes with the fix (`a4-parity2-d85-corpus-diff.log`). No stored fixture changes at all. `command_parity`, `movement-flow` and the transport / world-flow corpora do not reach the branch.
- Suite **195 / 195** serial (146 s); `tsc` clean; the whole `game/` FAIL set **identical** to the baseline (7,771 tests).
- Mutations (`a4_parity2_mutation_check.py D85`): **10 / 10 killed** — roster loop, party + 1, party − 1, an already dead member skipped, a cap at six, the device counting the roster (native ×6), and the same four shapes in the reference. The first run left the reference's "cap at six" mutant alive (a party never exceeds six, so only a synthetic party size above six can tell); the test above was added and the mutant re-run: killed (`a4-parity2-d85-mutation.log`).

**Not changed — a larger finding recorded as D-90.** Both investigators found, and the reconciler confirmed in the binary, that *before* the `cmp [0x5893],0x1d` test every trapdoor (all locations, every lap of a chained fall, and location 29 too) first runs `K:0x5910` (viewport redraw with the wind roll) and `K:0x2AA8` `party_random_damage` (`rand(1,8)` per living member): in the original, **stepping on any trapdoor hurts the whole party**, and a fall costs HP at every floor. Both ports omit both and say the branch has "no RNG" (the reference docblock is corrected here with a dated note; behaviour is not). It is outside D-85's wording (loop bounds, indices touched, the roster), it moves the `turns` corpus (72 live rows and up to 504 with successors) and it needs its own batch; the exact plan is in `D85-FINAL.md` §3b / §4c.

### 16.8 D-86 — a dungeon digit naming an invalid member passes no world turn (fixed)

**What the original does.** In a dungeon every key `0x30..0x39` goes to DUNGEON.OVL `0x06c4` (arm `0x07bc–0x07d6`), which calls the shared kernel set-active routine `K:0x4080` and then **overwrites its return value with 0** (`0x07ce mov [bp-2],ax` immediately followed by `0x07d1 mov word ptr [bp-2],0`). The kernel returns 1 only for "Invalid!" (an index ≥ `[0x585b]`, a member whose status byte is `'D'` or `'S'`); the dungeon discards it, so its loop skips exactly one thing: the per-turn block `0x0c76` (the sleeper-wake `rand(0,0x3f)` rolls, the wanderer step, tile effects, the status redraw and `K:0x2ae8` housekeeping). The digit path itself draws no RNG. `'0'`, a valid member, an invalid index, a dead and an asleep member: the same, no world turn. The **overworld** (MAINOUT `0x0c0c` → `0x0c39 advance_clock(2)`) and the **town** (TOWN `0x0ef3` → `0x15d4 advance_clock(1)`) do *not* discard the 1: there an invalid digit costs a turn. The dungeon's forced 0 is the only one of the three.

**Who was wrong.** Native, **large**: `commands.cpp` ended the invalid branch with `if(!g.position.map.location)r.turn();`. A dungeon session never writes `position.map` (it stays 0 = the surface), so a full *outdoor* turn ran on the surface tile under the entrance — the wind roll, two minutes, the hazard roll, housekeeping, `turns_since_start++`, doors, actors, the spawn gate, the Refuge check (the first RED run: every invalid digit "turns 0→1, seed changed, clock 12:00→12:02"). The reference, **small**: the same gate (`position.location === 0`, true underground) reached only `runContextTurn`'s dungeon tail — a door-timer tick and a spurious Refuge check, no clock, no RNG — invisible to every fixture. `re/notes/dungeon.md` and the `survival.ts` docblock misattribute `[bp-0xe]` (it is the kernel party-state at `0x0fab`, not the dispatcher's result) and `game.ts` called the dungeon case "not derived".

**Fix (the smallest routing change).** Native: the discriminator becomes `c.dungeon` (`if(!c.dungeon&&!g.position.map.location)r.turn();`) — the flag `Ignite` and `ViewGem` already use. Reference: `&& !this.dungeonState`. The overworld keeps its turn, the town keeps none, combat is another arm, `'0'` and the valid branch are untouched.

**Evidence.**
- RED first: `a4_parity2_d86_dungeon_digit_runtime` (the REAL `AlphaRuntime`, real Board, raw keys: enter Deceit with `(E)nter`, then digits) **4 of 9 RED** — the four invalid-digit cases, each with its measured damage; GREEN 9/9 after. It checks, per digit, the full state (clock, `turns_since_start`, the live RNG seed, food, every member's HP and status, the dungeon cell and the surface position): a valid digit changes only the active member; an invalid one (beyond the party, a dead member, a sleeping member) changes nothing at all; `'0'` clears the active member and costs no turn; controls — a valid digit on the surface costs none and the overworld's invalid digit still costs the turn (clock +2, one world turn, a new seed). `a4-parity2-d86-dungeon-digit.test.ts` (reference, with a spy on `runContextTurn`): 2 of 6 RED on the unfixed reference (one by an exception: the stray turn reached the Refuge check with a party of zero), GREEN 6/6; controls: the overworld still passes a turn, the town (still) none.
- `gameplay_parity` (the live TypeScript-versus-native corpus; its rows have no dungeon-digit case): **68 rows appended** after the last existing row (so no earlier sequence index moves; 4,802 → 4,870): digits 0, 1, 2, 3, 4, 7, 9 × statuses G / P / S / D × surface and underworld-entered, time spells `T` / `Q`, a party of six with digits 6–9, and inn companions beyond the party. RED on the unfixed native (first mismatch is the first invalid row, sequence 4,805), GREEN after; the TypeScript fix needs no corpus change (its stray residue is not projected).
- No stored fixture changes (nothing exercised a dungeon digit). Suite **196 / 196** serial (155 s); `tsc` clean; the whole `game/` FAIL set **identical** to the baseline (7,777 tests).
- Mutations (`a4_parity2_mutation_check.py D86`): **7 / 7 killed** — native: the stray turn back (`!c.dungeon` removed), no invalid digit passing a turn anywhere (the overworld loses its turn), the town gaining one, a sleeping member accepted; reference: the same three routing shapes; restored tree GREEN (`a4-parity2-d86-mutation.log`).

**Not changed — two adjacent divergences recorded (§16.13).** (D-91) The 1-minute clock tick `advance_clock(1)` at `0x0f2f` is **not** gated on the dispatcher result: it runs once per loop iteration before the next key, for every key and every return value (`T`: none, `Q`: every second iteration) — so "no turn" is true of the *world turn*, but a dungeon digit still costs the loop minute, like every other no-turn dungeon key (Z, Look, Attack, ...); the ports charge clock and housekeeping *before* a consumed command instead. A class change touching many dungeon fixtures; it needs its own batch. (D-94) An invalid digit **in a town** costs a 1-minute town turn in the binary; the ports (and `game.test.ts`, and the location-1 `gameplay_parity` rows) give none.

### 16.9 D-87 — crops and table plates cap food at 9999 (fixed)

**What the original does.** Every food-adding branch of SJOG.OVL's `Get` (`0x18CE`) ends in kernel `counter_add(&food, 1, 9999)` (ULTIMA.EXE `0x3F14`, called as `call 0x7f94`): the **crop** `0x2D` and the **plate** `0x9A` at `0x1A50`, the plates `0x9B` and `0x9C` at `0x1ABE` — four tiles, not only "crops". `counter_add` has **no failure path**: it computes `s = int16(old + 1)` and stores 9999 when `s >= 9999` (a *signed* compare), else `old + 1` — so 9998 → 9999, 9999 → 9999 and **10000 up to 32766 → 9999: it clamps down; it neither leaves a larger value alone nor refuses** (only `old >= 32767` leaves the plain-`min` family: 32767 → 0x8000, 65535 → 0; unreachable in play, a save import or hand edit only, and not modelled). At the cap nothing else changes: the tile rewrite (`0x2D → 0x2C`, plates → `0x95`/the other half), the redraw mark, the text (`Crops picked!` / `Mmmmm...!`) and the karma decrement (only when karma > 0) all run before or independently of the call, and no instruction after it reads its result: **a crop picked at food 9999 is still consumed, still prints, still costs a karma point and the turn; food stays 9999.** There is **no separate arena routine** in COMBAT.OVL: the arena `G` key runs the same SJOG routine through the kernel thunk `0x7E06` with the location byte at `0xFF`; the ports' arena twin (`combat.cpp`, `combat.ts`) is a duplicate and was already capped. The ledger's "crops" wording understated the scope, and its "(Max Food (10,000))" was wrong — Max Food sets 9999; 10,000 was reachable only by picking a crop at 9999.

**Who was wrong.** `re/notes/cmds.md` (+1 with cap 9999, and that the clone's `food++` lacks the cap) was **right and never acted on**. The reference: two bare `food++` (`game.ts`: the plates' `stealFood` and the wheat branch). Native: one statement, `++c.game.food` in `quest_search.cpp`, which serves the crop **and** all three plates. Both ports' arena twins were right.

**Fix.** Reference first: both `food++` become `addWordCapped(food, 1)` (the helper every other TS food writer uses, `min(9999, v + 1)` — exact for 0..32766). Native: `food = uint16_t(std::min<int32_t>(9999, int32_t(food) + 1))` — the arena twin's own expression; no `if(food<9999)` guard (that would leave 10000+ alone, which is wrong) and no refusal. Tile, text, karma, events, turn and RNG are untouched.

**Evidence.**
- RED first (absolute expectations, because **no stored corpus can see this**: `gameplay_parity` compares native with the TypeScript `Game` live, and with both ports uncapped it agrees): `a4_parity2_d87_crop_food_cap` **3 of 5 RED** (a crop at 9999 → 10000, 10000 → 10001, 12345 → 12346; the three plates likewise; the karma/turn case at the cap) with the control and the arena twin green; the reference test `a4-parity2-d87-crop-cap.test.ts` **2 of 6 RED**; both GREEN after. Covered: a crop at food 0 / 9997 / 9998 / 9999 / 10000 / 12345 / 32766 → 1 / 9998 / 9999 / 9999 / 9999 / 9999 / 9999; every valid direction of the three plates at 9998 / 9999 / 10000; the refusals (a plate from the wrong side) leave food, tile, karma and the turn untouched at 9999 and 10000; at the cap the crop is still consumed, still prints, karma 50 → 49 (karma 0 stays 0) and the turn passes; below the cap nothing changed; and, for native, the **arena twin** at 9998 / 9999 / 10000 (crop and plate: it caps — there was no native test of it).
- **Cross-port rows.** `gameplay_parity` gains **64 rows** appended after the last row (food 9997 / 9998 / 9999 / 10000 × tiles 45 / 154 / 155 / 156 × four directions, two gets each, 4,870 → 4,934). They cannot go RED against the binary — only when one port is fixed first, which is the point of fixing the reference first: with the TypeScript capped and native not, `gameplay_parity` failed at sequence 4,902, the first 9,999-food row, and passes after the native fix.
- No stored fixture changes. Suite **197 / 197** serial (157 s); `tsc` clean; the whole `game/` FAIL set **identical** to the baseline (7,783 tests).
- Mutations (`a4_parity2_mutation_check.py D87`): **10 / 10 killed** — native: no cap, a value above the cap left alone, a cap of 10000, a cap only at exactly 9999, the crop refused at the cap, the karma skipped at the cap; reference: the wheat branch uncapped or leaving a larger value alone, the plates uncapped or capped at 10000; restored tree GREEN (`a4-parity2-d87-mutation.log`).

**Not changed (adjacent, recorded as D-98).** The same helper serves the *object* pickup (SJOG `0x14EA`, the food/gold piles): `loot.cpp` guards with `if(g.food<9999)`, which leaves a value above the cap alone where the original clamps it to 9999 (gold likewise). It only differs for food or gold already above 9999 and is not a crop. The `Max Food` cheat deliberately never lowers a value (an enhancement, `enhanced.cpp`); its comment no longer claims crops reach 10000.

### 16.10 D-88 — the combat clock: one minute per ten unit activations (fixed; changes Original on purpose)

**What the original does.** `COMBAT.OVL`'s round loop (file `0x0B94`) counts every **unit activation** in a DS *byte* `[0x5882]` (`inc byte [0x5882]` at `0x0C64`). The compare is `cmp byte [0x5882],0xA / jne` — **equality, 8-bit wrap**: at the 10th the byte is zeroed *before* the call and **`advance_clock(1)`** (kernel `0x4F7C`) runs at `0x0C76`. It is the plain clock routine: not a world turn, no housekeeping. It runs at the **start** of the 10th activation — after the unit's countdown was reloaded, before the AI / human turn routine is entered — so before any of that unit's text, prompt, sound or RNG draw. What counts: every unit that survives four skip tests (an empty slot, a gone slot, a party member whose roster status is `'D'` — swept with no turn —, a unit standing on tile `0x84`/`0x85`) and whose countdown reaches 0 on this pass, **regardless of side**, control, sleep, charm, Time Stop, Quickness skips, the auto-pass of a non-active party member and every activation after "VICTORY!" (a victory does not end the fight). **`[0x5882]` is not combat-local**: nothing initialises it at combat entry or exit; it lies inside the 0x1060-byte `SAVED.GAM` window at file offset **`0x2DC`**, loaded and saved verbatim by DOS file I/O (exactly three instructions in the program reference it), so it survives fights, saves and loads and returns to 0 only by reaching 10 (or a fresh INIT.GAM). Inside an arena `[0x5893]` (g_location) is **`0xFF`**: the sky strip / moon latch and the clock hook are skipped, and the midnight Shadowlord re-roll (`rand(1,8)`, drawn from the fight's own stream) excludes no town. `advance_clock(1)` in a fight: minute + 1 (one carry), torch and light-spell minutes − 1, the hourly countdown, hour / day / month / year rollover; it does **not** touch food, HP, status or the turn counter — those are housekeeping (kernel `0x2AE8`, exactly four call sites, all in world loops). **No meal, starvation damage or poison tick ever runs inside a fight.** A crossed hour is charged at the next housekeeping **iff the last nonzero advance before it is the one that crossed it** (the `prev_hour` flank): after an overworld / town / ship / camp fight the next consumed turn's `advance_clock(2|1)` overwrites `prev_hour` first, so the crossing is **swallowed**.

**Who was wrong.** Both ports: the TypeScript `Combat` counted activations (`actionCount`, whose own comment said "1 minuto de juego cada 10 acciones") and nothing read it; native's `action_count` likewise. Neither had the tick, the persisted byte, the `0xFF` location, or the 8-bit equality. `re/notes/combat.md` reads the instructions right but implies a combat-local counter (it is save state) and has the exit conditions of `0x0CCA` / `0x0CE8` swapped; `kernel-survival.md` ("2 callers") is superseded (21 sites). The §14.5 wording "touches housekeeping timing after every fight, widest corpus impact" overstated it: only the arena tick is new; the swallow emerges from the existing flank logic; only the live `gameplay_parity` and `quest_parity` and four runtime goldens move.

**Fix.** Reference first: `GameState.combatClock` (`state.ts`), `Combat.tickCombatClock()` right after the activation count (equality, 8-bit wrap, reset before the call, `advanceClock(state, 1, <the fight's rng>, undefined, 0xFF)`; inert without `state.time`, which is what keeps every fixture generator's time-less state byte-identical), `advanceClock` / `relocateShadowlordsAtMidnight` gain an optional location for the midnight exclusion, and `saveNative.ts` reads / writes `+0x2DC`. Native: `TurnState::combat_clock`, `Engine::tick_combat_clock()` right after `++s.action_count` in `current()` (the fight's stream, location `0xFF`, no sky), `advance_clock`'s optional `party_location`, and the byte through the codec (`persistence.cpp` decode / encode, `save_core.cpp`; the PC bridge needs no code of its own — it reads and writes the original file through that codec, so the counter crosses it, and a mutant that removed its own lines survived because there were none to remove). **Document convention:** the counter is written to the document only when non-zero, and the encoder writes an explicit 0 when it is absent (so a template file's byte can never leak): with that, every Original document that has seen no ten-activation fight is byte-identical to the pre-D-88 one (the A4-ENH1 / ENH2 saved-document goldens are untouched) and `capture(restore(document)) == document` holds.

**Evidence.**
- RED first: `a4-parity2-d88-combat-clock.test.ts` **11 of 12 RED** (the green one is the new optional-location parameter itself), later 13 tests; `a4_parity2_d88_combat_clock` **7 of 7 RED** on unfixed native; both GREEN after (13/13; 9/9 native). Covered: 9 activations no minute / the 10th +1, counter 0, torch −1, **tick at the start of the activation** (before the unit acts); 11 / 19 / 20 / 21 / 30; the counter equals the activations mod 10 over 45; **it survives a fight** (9, then the next fight's first activation ticks; a loaded 7 ticks after 3); equality with 8-bit wrap (a loaded 255 wraps with no tick and ticks on the 11th; a loaded 12 ticks only at the 254th); minute and hour rollover with `prev_hour` kept; 100 activations at food 0 with a poisoned member change no food / HP / status / turn counter; the 06:00 swallow versus the 05:58 charge (via the real `advance_clock(2)` + housekeeping); Time Stop (the tick only snapshots `prev_hour`) and Quickness (1, not 0); the midnight re-roll draws from the fight's stream and its outcome **does not depend on the live town** (1..8); an enemy's activation counts like a player's; the byte round-trips through `SAVED.GAM +0x2DC` (reference and native), the encoder writes an explicit 0 over a template byte of 99, and the **PC bridge** carries it (an original `SAVED.GAM` with `+0x2DC = 7` imports as 7 and exports as 7).
- **Live parity.** `gameplay_parity` and `quest_parity` compare the two ports; both tick, so both keep passing, and `combatClock` is added to both projections so a one-sided counter bug turns them red (the first run with the field exposed a round-trip invariant — a zero omitted on capture but default-filled on load — which drove the document convention above).
- **Corpora.** Every stored fixture generator (`combat`, `combat-negate`, `advanced-combat`, `magic`, `dungeon-flow`, `world-flow`, `transport`, `turn`, ...) is **byte-identical**: their states have no `time`, so the hook is inert (`--check` drift tests all pass); `persistence_parity` passes unchanged (the byte is a document key only when non-zero).
- **Preservation goldens moved on purpose (4 runtime, 0 static).** A4-ENH1's night walk (`P3`, `P4`) and A4-ENH2's starving walk, dungeon walk and save (`W2`, `D3`, `S1`) walk through fights, and fights advance the clock now; they are re-recorded with the old value kept in a comment (`a4-parity2-d88-preservation-runtime-diff.log`: the HEAD binary and the new tree side by side). The ENH1 walk keeps its fights / combat steps / poisoned HP / turns (3 / 207 / 378 / 26) — only the hash, which holds the clock minute, moves. The ENH2 walk's timing shifted (the hourly starvation draws follow the clock) and with it the stream the dungeon section inherited: that scenario no longer met its wanderer (`D2` red), so its dungeon section is re-seeded (`rng.seed(3)`: seeds 3 and 5 ambush twice, as the original did). The static goldens (`a4_enh1_preservation`, `a4_enh2_preservation`, saved documents included) are untouched.
- Suite **198 / 198** serial (166 s); `tsc` clean; the whole `game/` FAIL set **identical** to the baseline (7,796 tests). One reference guard moved: `save-editor-completeness.test.ts` requires every new `GameState` key to be classified by the save editor, so `combatClock` is a covered row ("Combat clock counter", 0-255) and a `RuntimeNumberField`.
- Mutations (`a4_parity2_mutation_check.py D88`): **20 / 20 killed** — native (13): the tick at 9 / 11 activations, a modulo test for the equality, two minutes, housekeeping run by the tick, the live location excluded, the live stream instead of the fight's, only the player's activations counted, a reset at combat entry, the `.GAM` byte never written, an absent key leaving the template byte, a load not reading the counter, a restore ignoring it; reference (7): the tick at 9, no 8-bit wrap, two minutes, the live location excluded, never written to `SAVED.GAM`, never read back, the count removed; restored tree GREEN (`a4-parity2-d88-mutation.log`). The first campaign left three native mutants alive and drove three corrections: the codec-level "explicit 0 over a template byte" test (the PC bridge's own export used to mask that mutant), and the removal of two PC-bridge edits that turned out to be redundant (the codec already carries the byte, which is why the bridge needs no code of its own).

**Not changed — recorded (§16.13).** (D-99) The tick makes two older order divergences observable: the **bridge troll** (the original runs troll → housekeeping, the ports housekeeping → troll) and the **dungeon** (the original runs head `advance_clock(1)` → command → fight → housekeeping; the ports charge clock and housekeeping before the command), so a crossed hour is charged / swallowed differently there; and the optional **post-fight `advance_clock(0)`** (kernel `0x6356`: it only refreshes the moon latch / 12-hour dial when a flank is pending) is not modelled. (D-100) The scheduler's `0x84`/`0x85` skip (a unit on the stocks / manacles never acts and never counts) is a binary rule neither port models; porting it would move the `real_arena_*` corpora. The dungeon camp's ambush (mode 6) charge/swallow is **unknown** (the Hole-up command's return was not decoded).

### 16.11 D-82 — a New Journey seeds the underworld skiff and four bodies from INIT.OOL (fixed; changes Original on purpose)

**What the original does** (`D82-FINAL.md`; two independent binary derivations plus a cross-check agree). A new game's `SAVED.OOL` is `zeros(256) ++ INIT.OOL` (0x200 bytes): `FONT.OVL 0x0de7-0x0e3d` (Create Character) and the Ultima IV transfer (`INTRO.OVL 0x1363-0x1e08`) both write it. `INIT.OOL` is the raw 256-byte image of the live object table (`DS:0x5C5A`, 32 records of 8 bytes; +0 tile byte, +2 x, +3 y, +4 floor, +5 hull, +7 skiffs) and fills the **UNDER** block only; the BRIT block is zero. The shipped file holds five records, all at floor `0xFF`: slot 23 a **skiff** (`0x29`) at (14,242) and slots 24-27 four **bodies** (`0x1e`) at (103,226), (105,227), (107,227), (108,225) beside the Amulet's cell. They reach the live table through the whole-table `UNDER.OOL` reads (the kernel main loop after any town/dungeon session, the moongate, the waterfall); a save carries them in `SAVED.OOL` or the live window, wherever they currently are.

**Who was wrong.** Both ports, by omission: `init.ool` was only the *write template*, nothing read it, so every Native/Reference journey started with an empty underworld table (no skiff, no bodies). Also wrong: the extractor built `init.ool` from `BRIT.OOL ++ UNDER.OOL` — run-time scratch files ("Journey Onward" rewrites them from `SAVED.OOL`), not distribution data. The bytes are identical for today's install; the *source* was wrong, and a played install would have leaked its parked tables into every new journey.

**Provenance caveat (stated, not hidden).** The `original/u5/ultima5` set in this tree is the Exodus "Upgrade 1.0" (2001) build, not a 1988 floppy. The derivation is static: the two writers, the three readers and the five records come from that build's binaries and `INIT.OOL`. Nothing in the old docs contradicts it, but no 1988 `INIT.OOL` was available to compare, so a 1988-vs-2001 difference in the five records cannot be excluded. The seed is therefore driven by the shipped file's bytes (not by five hard-coded literals) and every claim above is pinned by an asset-gated drift test.

**Representation (the port's own, nothing invented).** Skiff / horse / carpet records become the **Class C terrain override** `0:<floor>:x:y = 0x100 + byte` (what a dismounted skiff already is; boarding erases it and touches no `g_hull`); frigates would be `ship` pool objects with their hull and skiffs; the bodies (`0x1e` / `0x1f`) become `prop` pool objects with their slot — the same representation as an interior corpse. A record whose +4 disagrees with its block is skipped (the original leaves it invisible: `K:0x368e` and the compositor compare +4 with `g_floor`), slot 0 (the party's own vehicle) is ignored, an unknown class byte is reported and **not placed**. No RNG, no text. The seed runs **only at the New Journey seam** — never inside `createNewGame` (the parity harnesses build states with it), never on load, continue or import.

**Fix.** Reference first: `seedNewJourneyUnderworld(state, ool)` (`state.ts`) called by `main.ts` right after `applyGypsyCreation` from `/assets/init.ool`; the extractor's `buildInitOol(INIT.OOL, BRIT, UNDER)` makes `init.ool = zeros(256) ++ INIT.OOL` (the scratch pair is only the fallback when INIT.OOL is absent). Native second: `seed_new_journey_underworld(ool, length, services, terrain)` (`quest_world.cpp`; all-or-nothing — the pool reservation is taken first, a refusal returns false with nothing placed) called by `AlphaRuntime`'s `CreateInitialSave` branch after the clean reconstruction and before the first save; a refusal fails the journey with the existing "Initial save could not be created".

**Evidence.**
- RED first: `a4-parity2-d82-underworld-seed.test.ts` **6 of 8 RED** against the no-op scaffold (`native/core/a4-parity2-d82-ts-red.log`), 9 tests GREEN after (the ninth is the asset-gated drift lock: `game/assets/init.ool == zeros(256) ++ INIT.OOL`, and the five records are the ones above). Native: the `a4_parity2_d82_runtime` runtime test (real `AlphaRuntime`, real `alpha_save.cpp` on the fake card; **12 of 12**) — N1 the pack's `initial_ool` is `zeros ++ INIT.OOL` with exactly the five documented records; N2 four body props at (0,255), tile `0x11e`, slots 24-27 at the documented cells and no ship; N3 one persistent override `(0,255)(14,242) = 0x129` (the only persistent underworld cell; the effective tile reads `0x129`); N4 Journey Onward restores exactly that with no duplicates; N5 **Load Game of a pre-journey save seeds nothing**; N6 a refused reservation places nothing; N7 floor-mismatch / slot 0 / unknown class / short image place nothing, a frigate keeps its hull and skiffs, a horse / carpet / skiff are overrides, a block-0 record seeds on floor 0. Extractor: `init-ool.test.ts` (3 tests incl. the negative control that played scratch files never leak in).
- **Corpora.** None moves: the seed lives at the New Journey seam, which no fixture, corpus or parity driver crosses (`createNewGame`, the native driver and every generator are untouched; `--check` drift tests pass, `gameplay_parity` / `quest_parity` unchanged). No preservation golden moves.
- Suite **199 / 199** serial (163 s); `tsc` clean; the whole `game/` FAIL set **identical** to the baseline (97 assertion-level fails, identical sets); extractor suite 235 passed / 2 skipped (was 232 before the 3 new tests), its `tsc` shows only the 3 errors that exist at HEAD.
- Mutations (`a4_parity2_mutation_check.py D82`): **16 / 16 killed** — native (11): the seed never called, a load seeding too, a body on floor 0, a body that is not a prop, a transient skiff, the floor-vs-block check removed, a refused reservation ignored, slot 0 seeded, a skiff tile off by one, a frigate losing its hull, an unknown class placed (two mutants first failed to compile and were rewritten — INVALID is not killed); reference (5): the floor check removed, the skiff on floor 0, slot 0 seeded, an unknown class placed, a frigate losing its hull. Restored tree GREEN both sides.

**Not changed — recorded (§16.13).** (1) **PC bridge, import:** an original `SAVED.GAM/OOL` carries the four bodies (they are in every original new-game table); `read_vehicles` only reads vehicles, so an imported save loses the bodies. Reading `0x1e/0x1f` into props is a small, coherent extension, left out because A4-SAVE3's bridge corpus (P10 / T5 / T6 / P17) would need re-deriving. (2) **PC bridge, export:** `place_vehicle` fills free records from slot 31 downward; a table that has the original's slots 24-27 occupied by bodies and enough skiffs would run out, and a future export of Native's body props must keep slots 28..31 for vehicles. A fresh Native journey exports its seeded skiff as a record at the top of the table rather than at slot 23 — a harmless re-slotting, the data being the same. (3) The reference-side comments that still call `init.ool` BRIT/UNDER (`game.ts` ~3434-3442 and ~6964, `saveNative.ts` ~509 and ~954) and the notes' DS offsets (`re/notes/intro.md:162-163`, `oracle-pending-sweep.md:89-90`, `moonstone-loc-y-pozo-doom.md` 2.4/2.5) are listed in `D82-FINAL.md` 3.4; only the behaviour was changed here.

### 16.12 Small item — In Mani Corp's scroll on a living target prints "Not dead!" then "Failed!" (fixed; text only)

**What the original does** (`MANI-FINAL.md`, re-disassembled; the §11.15 line was *correct but incomplete*, and the brief's "the spell" was wrong). The scroll reader `CAST.OVL 0x11de` arm 6 (`0x12d8`) prints `Resurrection!`, runs the "On who:" picker (`CAST2.OVL 0x009e`) and calls `resurrect_apply` (`CAST2.OVL 0x05e0`) with flag **1** (`12ea mov ax,1`). That routine compares the status **byte** against `0x44` only; for any other byte with a non-zero flag it prints **`Not dead!`** (DS `0x953c`, `060a mov ax,0x953c`) and returns 0. The reader returns 0, so the (U)se epilogue (`CAST.OVL 0x1b8a-0x1b94`) prints **`Failed!`** (DS `0x4a7b`) and a glide tone. Transcript: `Scroll` / `Resurrection!` / `On who: <Name>` / `Not dead!` / `Failed!`. The scroll is consumed first (`0x11ec`); no RNG draw. A dead target is revived silently; a cancelled picker (`None!`) adds nothing; in the arena the scroll prints `Resurrection!` then `Not here!` (no picker, no `Failed!`). The **spell** (flag 0) is silent and its Cast tail prints only `Failed!` — both ports already did exactly that and are not touched.

**Who was wrong.** Both ports discarded the boolean: TS `main.ts` (scroll branch) and native `world_magic.cpp` case 6 printed nothing after `Resurrection!`; the `check-gameplay.ts` replica (the oracle of the live `gameplay_parity`) copied the TS shape. `re/notes/potions-scrolls.md` row 6 said "selChar + applyResurrect + sfx" (incomplete) — dated correction added.

**Fix.** Reference first, at the caller layer (the state transition `applyResurrect` / `apply_target_spell` and the magic corpus are untouched — their boolean already encodes `status == 'D'`): `main.ts` prints `Not dead!` then `Failed!` as two rows when `applyResurrect` returns false; the replica gets the same two `msg()` calls; the approved-strings key `"Not dead!"` gets its `[D]` citation. Native: `world_magic.cpp` case 6 says the same two lines on a false return. This is the single native producer for the world, town and dungeon scrolls, so one edit covers all three. No sound is added (the glides at `0x11d3` / `0x1ba7` stay `EvidenceUnknown` in `sfx_inventory.cpp`: an audio decision, not part of this text fix).

**Evidence.**
- RED first: `a4_parity2_mani_not_dead` **10 of 29** checks RED on the unfixed native (`native/core/a4-parity2-mani-red.log`: seven status bytes G/P/S/X/0/0x43/0x45 and three locations); `a4-parity2-mani-not-dead.test.ts` **3 of 5** RED (`a4-parity2-mani-ts-red.log`), the two green ones being the pins (`applyResurrect` is false for every status byte but `'D'`; the spell branch and `readScroll(6)` stay without `Not dead!`). Both GREEN after (29/29; 5/5). Controls: `'D'` revives silently (G, HP 1), 0x43 / 0x45 are *not* dead, no target (`member` -1 / past the roster) prints only `Scroll` / `Resurrection!`, the other seven scrolls never print it, no RNG draw anywhere on the failure path.
- **Live parity.** `gameplay_parity` runs scroll 6 on G/P/S/D members in the replica and in native: both changed together, still identical.
- **Corpora.** None moves: `magic.txt`, `items.txt`, `commands.txt` and every combat corpus are byte-identical (`generate-magic-fixtures.ts --check` passes); no preservation golden moves (`a4_enh2_preservation` B4 pins `apply_target_spell` only).
- Suite **200 / 200** serial (166 s); `tsc` clean; the `game/` FAIL set **identical** to the baseline (97 assertion-level fails; 7,810 tests).
- Mutations (`a4_parity2_mutation_check.py MANI`): **11 / 11 killed** — native (7): only `Failed!`, only `Not dead!`, the two lines swapped, printed on a *dead* target instead, only a Good target counted as not dead (P/S/other must print), printed with no target, an RNG draw on the failure path; reference (4): `Failed!` only, swapped, printed on a dead target, the replica printing nothing. (T-mutants also ran `gameplay_parity` against the native binary left from the previous N-mutant; their kill is by `vitest`, listed first, and the final restored-tree run rebuilds both sides GREEN.)

**Not changed — recorded (§16.13).** (U)se takes no turn in the ports (`commands.cpp` 1074-1076) while the original spends a world turn on foot and in a town for the scroll and the spell alike (`MAINOUT 0x0c39`, `TOWN 0x159a-0x15d4`; the dungeon consumer is untraced). The spell's circle-8 ceremony runs before the apply in the ports and not at all on a living target in the binary (`CAST2.OVL 0x06d4` is on the success path only). The `Failed!` glide is unmodelled audio.

### 16.12.1 Small item — the jail / inn housekeeping note (documentation only; no production change)

`re/notes/kernel-survival.md` §5.5 claimed the jail / inn wait applies meals and hunger at each hour change. `JAIL-FINAL.md` (two independent derivations, a closure test and a caller census) shows the note is **wrong and the ports are right**: the jail loop (`TOWN.OVL 0x1324-0x1330`) calls exactly `advance_clock(0x14)` per round, the inn night is a different loop (`SHOPPES3.OVL 0x01B5-0x01F4`: 12 x `advance_clock(5)`, `beep_ticks(1)`, ring sweep, redraw, `advance_clock(9)`, until `g_hour == 6`), and the per-turn housekeeping (`K:2AE8`: meals at hours 6/12/18, `Starving!`, poison, the turn counter, the time-spell countdown, ring regeneration) has exactly four call sites in the install (`TOWN 0x10D0`, `MAINOUT 0x0CD3`, `DUNGEON 0x0E22`, `CMDS 0x0671`), none of them reachable from either wait. What *does* run it every ten-minute step is the bed hole-up (`CMDS 0x0552 -> 0x0671`), the probable origin of the sentence. Both ports already omit it from the jail and the inn, so there is nothing to fix there: the note got a dated three-part correction (the loop shapes, the four callers, and the pointer to D-95 below). No code, test or corpus changed; this is the documentation half of the same item, in its own commit. The jail's **clock** divergence the derivation found on the way (the ports assign `hour = 8; minute = 0`; the original loops `advance_clock(20)` until the hour is 8) is a real production divergence, recorded in §16.13, **not** fixed here.

### 16.13 Newly discovered divergences (recorded, NOT fixed; each is its own batch)

Every row came out of an A4-PARITY2 derivation with binary evidence in the cited `*-FINAL.md`; none is fixed here, none moves a corpus, and none changes what an Original-mode player sees until its own batch is run. "Class" says what a fix would cost.

| ID | Where | What the original does | What the ports do | Class / note |
|---|---|---|---|---|
| D-90 | Stonegate trapdoor (location 29), every trapdoor and every lap of a chained fall | Before the `cmp [0x5893],0x1d` test the loop first runs `K:0x5910` (viewport redraw with the wind roll) and `K:0x2AA8` `party_random_damage` (`rand(1,8)` per living member): stepping on any trapdoor hurts the party first | Fall without the damage / wind draws (D-85 fixed only the lethal loop bound) | RNG-stream change in every trapdoor row of `turn`/`command` corpora; `D85-FINAL.md` §2 |
| D-91 | Dungeon command loop (`DUNGEON.OVL 0x0f2f`) | `advance_clock(1)` once per loop iteration **after** the key, for every key and every return value (`T` none, `Q` every second iteration); housekeeping only when the command consumed a turn | Clock **and** housekeeping charged *before* a consumed command (`dungeon_orchestration.cpp:191-193`, `dungeon-cmds.ts:326`); no-turn keys (Z, Look, Attack, turning, a digit) cost nothing | Class change touching many dungeon fixtures; native `advance_turn` would need a clock-only helper. D-86 closed only the stray *world turn*. `D86-FINAL.md` §7.2 |
| D-92 | **A blocked rowed step** (a by-product of the D-89 reconciliation) | `MAINOUT 0x0C30` skips `advance_clock` on a 0 return: a blocked naval step passes no time | The ports still run a naval turn (clock minutes, wind roll, poison tick) after a blocked rowed step | Movement-flow corpus rows; `D89-FINAL.md` |
| D-93 | Naval cactus inside a town | `TOWN 0x083A` prints `Blocked!` and beeps | Layer-ungated in both ports (the OUCH path runs) | No shipped data reaches it (the only town map with cacti has no navigable tile) |
| D-94 | Invalid digit **in a town** | Costs a 1-minute town turn (`TOWN 0x15b6-0x15d4`) | No turn (ports, `game.test.ts`, the location-1 `gameplay_parity` rows) | Small; moves `gameplay_parity` rows. `D86-FINAL.md` §7.3 |
| D-95 | Jail wait after an arrest (`TOWN.OVL 0x1324-0x1330`) | `while (g_hour != 8) advance_clock(20)` (test first) | TS `guardArrestJail` and native `blackthorn_action` assign `hour = 8; minute = 0` (copied native <- TS, `batch4_group_a_test.cpp` A3) | Wake minute is `start_minute mod 20` (mod 10 under Quickness) not 0; the calendar day advances for an arrest after 08:xx and not for 00:00-07:59; the midnight Shadowlord re-roll and its RNG draws, torch and light-spell minutes, month/year rollover and `monthsAtInn` ageing are skipped. `JAIL-FINAL.md` §1.3 |
| D-96 | Inn night (`SHOPPES3.OVL 0x0200-0x0282`) | The wake-up refill and poison death run **after** the 12-step night | Both ports run them **before** | Visible only as RNG: a poisoned Ring-of-Regeneration bearer draws `rand(0,7)` once per night iteration in the original, none in the ports. `JAIL-FINAL.md` §1.4 |
| D-97 | Tooling: `re/tools/callers_banda.py` (a by-product of every derivation) | — | `BASES` is wrong for 10-15 of 22 overlays (DUNGEON must be 0x81D0, OUTSUBS 0xA290, ...): the tool finds 4 of 17 `write_whole_file` call sites and 1 of the 4 callers of `resurrect_apply`; the brief's own table was wrong the same way; `thunks.py --bases` prints a minimum | Not production code, **not changed** here. Fix = the fitted table in §16.3; any census made with the stock tool must be redone. |
| D-98 | `loot.cpp` object pickup (SJOG `0x14EA`, the food/gold piles) | The shared counter helper clamps **down** to 9999 | `if(g.food<9999)` leaves a value already above 9999 alone (gold likewise) | Differs only for food/gold already above 9999 (an import or edit); the `Max Food` cheat deliberately never lowers. `D87-FINAL.md` |
| D-99 | Bridge troll and dungeon order, made observable by D-88's tick | Troll → housekeeping; the dungeon: head `advance_clock(1)` → command → fight → housekeeping; the post-fight `advance_clock(0)` (`0x6356`, moon latch / 12-hour dial) | The reverse order; no post-fight `advance_clock(0)` | A crossed hour is charged / swallowed differently there. `D88-FINAL.md` |
| D-100 | Combat scheduler `0x84`/`0x85` (unit on the stocks / manacles) | Never acts and never counts toward the clock | Not modelled | Porting it would move the `real_arena_*` corpora. The dungeon camp's ambush (mode 6) charge/swallow is **unknown** (the Hole-up return was not decoded). |
| D-101 | In Mani Corp (spell and scroll), ceremony and turn | The circle-8 / circle-6 ceremony is played **inside** `resurrect_apply` (`CAST2.OVL 0x06d4`) on **success only**; (U)se and (C)ast spend a world turn on foot and in a town for every outcome (`MAINOUT 0x0c39`, `TOWN 0x159a-0x15d4`; the dungeon consumer is untraced); a cancelled "On who" pick on the device refunds what the binary already spent (the scroll, the charge, the MP) | A generic cast ceremony at cast time, even on a living target; (U)se takes no turn (`commands.cpp` 1074-1076); the scroll has no ceremony index | Text fixed in §16.12; the rest is a presentation-and-turn decision. The `Failed!` glide is unmodelled audio (`EvidenceUnknown`). `MANI-FINAL.md` §7 |
| D-102 | PC bridge, import (from D-82) | An original `SAVED.GAM/OOL` carries the four bodies (`0x1e`) in the UNDER table | `read_vehicles` reads vehicles only: an imported save loses the bodies | Small coherent extension (read `0x1e/0x1f` into props, keep slots 28..31 for exported vehicles in `place_vehicle`); needs A4-SAVE3's bridge corpus (P10 / T5 / T6 / P17) re-derived. `ALPHA4_UI.md` §16.11 |
| D-103 | D-82 provenance and stale text | The five records, the writers and readers of INIT.OOL | `game.ts` ~3434-3442 and ~6964, `saveNative.ts` ~509 and ~954 still call `init.ool` BRIT/UNDER; `re/notes/intro.md:162-163` and `oracle-pending-sweep.md:89-90` have DS offsets shifted by one entry (real: `0x323f`/`0x3249`/`0x3252`/`0x325c`/`0x3266`); `re/notes/moonstone-loc-y-pozo-doom.md` §2.4-2.5 undercounts the `world_filename` callers (seven, four in the kernel) and calls a played session's scratch BRIT.OOL "carrying slot 0 at (86,107)" | Comment / note corrections only. `D82-FINAL.md` §3.4. The Upgrade 1.0 provenance caveat of §16.3 applies to D-82 and its records. |

**Smaller observations (no row yet).** The bed hole-up in the TypeScript `camp.ts bedSleepStep` still omits the per-step housekeeping and the `0x7A9A` tile call that native has (ledger D-20's residual); the ports' XP is unsigned where the binary's is a signed word (unreachable: writers cap at 9999, only an import could reach 32768; likewise food ≥ 32767 under D-87); the kernel key poll `0x266c` calls the idle tick `0x5910` (locations < 0x21 or > 0x7f), which can reach `rand` and the wind: real-time idle RNG during pickers and Y/N prompts is unmodelled by both ports; the Stonegate wipe zeroes all 32 object slots (live townspeople included) where the ports erase persisted objects at (location, floor) only — unobservable while the Refuge relocates in the same command; the whole-party-asleep loop (`DUNGEON 0x0fd0 jne 0xf93`) prints "Zzzzzz..." per pass and was not chased. Every statement here is static: **no derivation was confirmed in a DOS run**.

### 16.14 Mutation summary

`python native/core/tools/a4_parity2_mutation_check.py <build>` (every group, one pass over the final tree, `native/core/a4-parity2-final-mutation.log`): **110 / 110 killed, restored tree GREEN for all eight groups** (native and reference). Per group: D-89 14, D-83/D-84 22, D-85 10, D-86 7, D-87 10, D-88 20, D-82 16, In Mani Corp 11. Mutants that did not compile were rewritten, never counted as killed (D-82: two; the tool reports them INVALID). The final sweep also re-proved the per-item runs recorded in §16.5-§16.12. Reference mutants (T*) edit `game/src/core` or `main.ts` and run vitest, the group's corpus generators with `--check` and the live `gameplay_parity` / `quest_parity`; native mutants (N*) rebuild and run the group's ctest set. (A T-mutant's ctest leg can run on the binary left by the previous N-mutant; every T-kill recorded here is a vitest or `--check` kill, and each group's restored-tree run rebuilds both sides first.)

### 16.15 The RC4 candidate and the firmware

**Production changed, so the identity moved:** `PROJECT_VER` `4.0.0-alpha4-rc3-debug` → **`4.0.0-alpha4-rc4-debug`** (commit `859f1605`, which also carries the final verification logs). RC3's record (§15) is unchanged and stays the image for the music retest it was built for; RC4 is RC3 plus the A4-PARITY2 fixes, so it never shares a name or an FW line with it. **No RC4 hardware PASS is claimed.** Nothing was flashed, tagged or pushed.

Final verification on the committed tree (HEAD `859f1605`, then the ledger / doc commit):
- **Host:** fresh configure (`-DCMAKE_BUILD_TYPE=Release -DOPENU5_ENABLE_DEVELOPER_TOOLS=ON`, `build-a4-parity2-final`; a first attempt without the developer-tools option failed to link `debug_developer_tests` and was discarded, `a4-parity2-final-build.log` is the clean one), 1,649 steps, project warnings 0 after a `-Wnarrowing` in the D-85 test was fixed; **serial `ctest` 200 / 200, 160.9 s**. 192 baseline + 8: `a4_parity2_{d89_naval_ouch, d83_d84_resurrect, d85_trapdoor_party, d86_dungeon_digit_runtime, d87_crop_food_cap, d88_combat_clock, d82_runtime, mani_not_dead}`. Every corpus drift test, preservation golden and live parity test is inside that count and green.
- **TypeScript:** `tsc --noEmit` clean; the `game/` vitest FAIL set **identical** to the baseline (97 assertion-level fails, 7,810 tests; the 18 suites that cannot load are the same environment-bound ones); the extractor suite 235 passed / 2 skipped.
- **Firmware:** fresh `--no-ccache` ESP-IDF 6.1 (`idf.py --no-ccache -B build-a4-rc4 reconfigure`, `ninja -C build-a4-rc4 -j 4 all`; `a4-parity2-fw-{configure,build}.log`), 1,182 steps, **first attempt clean, 0 project warnings**, run alone (no `ctest` during the build). Guards (`a4-parity2-elf-checks.log`): `a3_04a_hotpath_check` **GREEN**, `a3_04b_iram_check` **GREEN**, `a3_04f_image_check` **GREEN**, `check_app_budget` OK.

| | |
|---|---|
| **File** | `native/targets/tdeck/build-a4-rc4/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC4-Debug-Launcher.bin` (byte-identical to `build-a4-rc4/openu5_tdeck.bin`) |
| **Size** | **1,047,408 B (`0xffb70`)** — RC3 + 1,120 B |
| **SHA-256** | **`49e3962f367cb85650ae548463fefbf0ef2f4bbaf5d567679422a0d141674414`** |
| **Identity** | `FW 4.0.0-alpha4-rc4-debug`, **`Git 859f1605b974`** (HEAD `859f1605b974c0d0d3c2fe38eea281f53ea3877f`; the only `-dirty` strings in the image are the unrelated `value-dirty` / `input-dirty` log tags) |
| **App partition** | 1,310,720 B (`0x140000`, 1.25 MiB): **263,312 B (20.1 %) free**; `check_app_budget.py` OK |
| **Launcher allocation** | **1,048,576 B (1,024 KiB)**, the image rounded up to 64 KiB; the next 64 KiB step is **1,169 B away** (RC3: 2,289 B). One more small fix would move the Launcher's own OTA slot to 1,152 KiB: still well inside the 1.25 MiB partition, but it is now the margin to watch |
| **Sections vs RC3** | flash `.text` **+1,068 B**, `.rodata` **+48 B**; IRAM, DIRAM (`.text` / `.bss` / `.data`) and PSRAM **unchanged** (`a4-parity2-fw-size-diff.log`, `a4-parity2-identity.log`) |
| **Packs** | the SD resource pack is **unchanged** (2,266,819 B, SHA-256 `85b38994…9507a2` as in A4-END1): `init.ool` was already `zeros ++ INIT.OOL` byte for byte, only its *source* in the extractor changed — no SD recopy from RC3 |
| **Earlier images** | re-hashed after the build, all unchanged: RC3 `9fd7fc8b…d542`, RC2 `608eb700…1e95`, RC1 `67100a51…145a` |

### 16.16 The minimal hardware checklist for RC4

Only what the host genuinely cannot show (the real TFT, keys, SD card and Board). Everything else is a host test above. Flash RC4, keep the SD pack. **PASS column intentionally empty.**

| # | Do | Expect | Why hardware | PASS |
|---|---|---|---|---|
| R4-1 | Boot, open the Developer screen | `FW 4.0.0-alpha4-rc4-debug` and `Git 859f1605b974` | the image identity (RC3's retest used a different hash) | |
| R4-2 | Title → New Journey, finish the gypsy quiz. Developer teleport to the Underworld at (105,227) (near the Amulet cell); look around; step into a body | four dead-body sprites at (103,226), (105,227), (107,227), (108,225), each **blocks** the party | the real sprite draw and collision of a seeded `prop` (D-82) | |
| R4-3 | Developer teleport to the Underworld at (14,242): a skiff sprite is there; walk onto / `B`oard it; save (Alt+S), power-cycle, **Journey Onward**, go back | a boardable skiff; after the reboot the bodies and the skiff (unless boarded) are where they were, not duplicated | the Class C override and the prop slots through the real SD save / load (D-82, N4) | |
| R4-4 | (U)se **In Mani Corp** scroll on a **living** member; then again and cancel at "On who" | `Scroll` / `Resurrection!` / `On who: <Name>` / `Not dead!` / `Failed!`; the cancel prints `None!` and nothing else; both consume the scroll | the device picker flow and the console lines (the text is host-proven) | |
| R4-5 | Healer (Resurrect) on a dead member of a **Level ≥ 2 / karma < 98** save | the member returns with the class's MP and the reduced experience, HP = the new maximum, not 1 | D-83 changes Original on purpose; a visible, shop-dialogue outcome | |
| R4-6 | Fight a long enough combat (≥ 10 unit activations) near the end of an hour (e.g. 11:55), wins or flee | the panel clock advances by minutes during the fight (about one per ten activations) | D-88 changes Original on purpose; the TFT clock box | |
| R4-7 | Row a ship into a blocked cactus cell (Developer: teleport a ship beside the Underworld/overworld cactus ring if one is reachable; otherwise record "not reachable") | `OUCH!` and **every** living member takes damage | D-89; no shipped data guarantees a cactus (§16.5), so this one may be unreachable | |

**Still owed from earlier candidates, not repeated here:** the RC3 dungeon-music retest (§15.10), the Alpha 4 RC heap capture (§14.7: after the RC4 flash, the same `esp_idf_monitor` capture; the seed adds at most four small pool objects and one terrain cell), and the §14.7 SAVE2 / UI3 / SAVE3 / PARITY1 items the user has reported accepted but not itemised. D-85, D-86 and D-87 have no hardware case: they are fully determined by host evidence (D-86 by a real-`AlphaRuntime` test; D-85 by its core test plus the real-runtime audio test W1b; D-87 by its core test and the live parity corpora).

### 16.17 Remaining parity debt, and where A4-PARITY2 stands

**Closed in software (hardware pending):** D-82, D-83, D-84, D-85, D-86, D-87, D-88, D-89 — all eight, with RED-first tests, mutation proof (110 / 110) and every corpus diff explained; D-82 carries the Upgrade-1.0 provenance caveat (§16.3, §16.11) and is closed for the New Journey seam, with the bridge's import side recorded as D-102. The two small items are done (In Mani Corp's living-target text, §16.12; the jail / inn note, §16.12.1, documentation only). **D-82 is therefore *not* the sole unresolved item: nothing in scope is unresolved.**

**Open, recorded, not fixed (§16.13; ledger rows D-90 … D-103):** the two derivation by-products the brief asked to keep out of production — D-92 (a blocked rowed step still ticks the clock) and D-97 (`callers_banda.py` overlay bases) — plus D-90 (every trapdoor's damage and wind roll), D-91 (the dungeon loop-minute class), D-93, D-94 (town invalid digit), D-95 (the jail clock), D-96 (the inn wake-up order), D-98 (loot cap above 9999), D-99 (bridge-troll / dungeon order), D-100 (scheduler skip), D-101 (In Mani Corp ceremony, turn and cancel-refund), D-102 (PC import drops the bodies), D-103 (stale comments and notes). Biggest in cost: D-91 (many dungeon fixtures) and D-90 / D-92 / D-95 (RNG streams and corpora). Every statement is **static**: no derivation was confirmed in a DOS run.

**State for the next step.** RC4 is built, verified and ready for the minimal hardware pass above; it is not RC-ready by that measure until the pass is run (and the owed RC3 retest and RC heap capture are recorded). Nothing is tagged or pushed; the publication recommendation is to **hold** until R4-1 … R4-6 pass, because D-83, D-84 and D-88 change Original behaviour on purpose and have never been seen on the device.

---

## 17. The RC4 hardware report and the RC5 hotfix (2026-10-04)

A targeted follow-up, not another parity sweep. RC4 (§16.15) was flashed and the minimal checklist (§16.16) run by the user; two items failed. This section records the report exactly, the investigation, the one production fix, and the three-step retest. §16 and the RC4 record are unchanged.

### 17.1 RC4 hardware result (user report, 2026-10-04)

A **written user hardware report** — no serial capture, no photograph, no log was supplied, and none is claimed. Image: `FW 4.0.0-alpha4-rc4-debug`, `Git 859f1605b974`, SHA-256 `49e3962f…4414`.

| # | Result | What the user reported |
|---|---|---|
| R4-1 | **PASS** | identity as expected |
| R4-2 | **PASS** | New Journey: the underworld skiff and the four bodies (D-82) |
| R4-3 | **PASS** | save / power-cycle / Journey Onward |
| R4-4 | **FAIL** | (U)se In Mani Corp scroll on a **living** member showed only `Failed!`; `Not dead!` was missing |
| R4-5 | **FAIL** | in a working healer shop `H` (Heal) and `C` (Cure) worked; **`R` (Resurrect) did nothing** |
| R4-6 | **PASS** | combat clock (D-88) |
| R4-7 | **NOT TESTED** | naval cactus; optional, unreachable on shipped data (§16.5) |

D-82 and D-88 are therefore hardware-confirmed. Nothing else of PARITY2 is reopened; D-90 … D-103 stay recorded, not fixed.

### 17.2 R4-5 — the healer's `R` did nothing: root cause and fix

**Root cause (a UI key map, not the shop or the resurrection).** `UiSession::handle_shop` (`ui_session.cpp`) turned a raw `r` into `ShopAction::Rations` at a Barkeeper and **`ShopAction::Rest` everywhere else**. The healer's prompt accepts `Heal`, `Cure` and **`Resurrect`** (`shop_orchestration.cpp`, `HealerNeed` / healer `Menu`); `ShopAction::Resurrect` appeared nowhere else in the tree, so **no key could ever produce it**. A `Rest` at a healer is not a legal action in any healer phase and is dropped without a word — exactly "R does nothing". `h` and `c` were mapped, which is why Heal and Cure worked. The context bar (`H Heal|C Cure|R Raise`) and the prompt (`H Heal  C Cure  R Resurrect`) advertised a key the map never honoured.

**Why nothing caught it.** Every healer test — `shop_flow` (98 sequences), `shop_parity`, the PARITY2 `a4_parity2_d83_d84_resurrect` — drives `ShopAction::Resurrect` **directly** into `execute_shop`; none sends the player's `r`. The UiSession key map and the shop service were each right on their own side of an untested seam (the same class as *Host fixture copies hide device wiring* / *Parity harnesses bind what the device does not*). The bug is older than PARITY2: `r` was never Resurrect on the device (the PARITY2 change, `resurrect_apply`, is innocent and is reached once the key is routed).

**Fix (the key map, one line):** `case 'r'` → `Rations` at a Barkeeper, **`Resurrect` at a Healer** (SHOPPES 0x16b5: the original's R is Resurrect), `Rest` otherwise (the innkeeper's `R Rest` is untouched). No `if (key == 'R')` was added at any other layer.

### 17.3 R4-4 — the In Mani Corp scroll on a living member: investigated, **not reproduced**

The whole device path was traced and then driven for real: `(U)se` → item picker → In Mani Corp Scroll (id 6) → "Use on whom?" `PartySelection` → `AlphaRuntime::modal` (`UseItem`, `member`) → `commands.cpp` / `dungeon_orchestration.cpp` → `world_magic` case 6 → `Message` events → `UiSession::consume` → transcript → the console the Board paints.

- **The native core emits both lines** (`world_magic.cpp`: `Resurrection!`, then on `!apply_target_spell(... Resurrect ...)` `Not dead!` and `Failed!`), and nothing between the core and the transcript rewrites, merges or drops them.
- **The shipped RC4 binary has it:** in `openu5_tdeck.elf` the only reference to the `"Not dead!"` literal (`0x3c0cbddc`) is an `l32r` at `0x42076d24` **inside `world_magic`**, right after the `apply_target_spell` call and immediately followed by the `"Failed!"` literal and two calls to the `say` lambda. The flashed code is the PARITY2 code.
- **The parity test was not the device route** — the PARITY2 test (`a4_parity2_mani_not_dead`) calls `world_magic()` with a hand-built `Command` — so a real-runtime test was added (§17.4, group **U**). **It is GREEN against RC4 production, not RED:** with raw keys through the real `AlphaRuntime` and Board, the living target reads `Use item` / `Scroll` / `Resurrection!` / `Not dead!` / `Failed!` on the overworld, in two towns, inside a dungeon, with scenes paced, with the audio pack mounted, and in the checklist's literal *use, then cancel at "On who"* order (and the reverse); the rendered console (a PNG dump) shows both lines; a cancel prints neither and spends nothing; a dead target is revived silently.
- **What does read exactly "Failed!" only** is the **spell** form: `(C)ast` In Mani Corp on a living member prints `Cast...` / `Failed!` — correct, the binary's spell is silent and prints only the Cast tail's `Failed!` (`MANI-FINAL.md` §1; test U7). That is precisely the reported symptom, and the Developer screen's *Spells* and *Scrolls* rows share one index field and sit one row apart. **This is a hypothesis about what happened on the device, not a finding;** nothing in the report can distinguish it from an unknown device-only cause.
- The §16.16 R4-4 row also listed an `On who: <Name>` line. **The native port has never printed it** (`MANI-FINAL.md` §7.5; the device uses a `PartySelection` modal instead), so that expectation was wrong; RC5-2 below drops it.

**Verdict for R4-4: no production defect was found, none was changed.** The route is now pinned by 16 real-runtime checks so a regression cannot pass unseen, and RC5-2 is written so that a second failure is self-diagnosing (it asks for the whole console, whether `Cast...` is above `Failed!`, and whether the scroll count dropped). If RC5-2 fails the same way **with `Scroll` / `Resurrection!` above it and a scroll spent**, that is a genuine device-only defect and the Alpha 4 publication blocker it was reported as.

### 17.4 Do the two failures share a cause? No

R4-5 is a missing key-map entry in `UiSession`'s shop mapping; R4-4 involves a different mode (the item and party pickers), a different command (`UseItem`) and no key-map defect. Target-selection routing, member-status filtering, the modal-to-core conversion and transcript handling were each exercised end to end for both and behave. They were kept separate, with no shared abstraction.

### 17.5 Tests and mutation proof

- **`a4_rc5_hotfix_runtime`** (`targets/tdeck/host_tests/a4_rc5_hotfix_runtime_test.cpp`; REAL `AlphaRuntime`, real `tdeck_board.cpp` over the fake ST7789, the `a4_ui2` harness; 37 checks; arguments: the resource pack and the audio pack). Group **U** — the (U)se route (above). Group **H** — the healer, reached by *Talking to East Britanny's keeper* with real keys: `H0` the shop opens; `H2a/b` **H** heals and **C** cures and each charges; `H1` **R opens the member list** like H and C; `H3` the dead companion is selectable, the deal quotes a price, and Y raises it — at **karma 50** XP 1000 → 500, level 4, max HP 120, HP = 120, and at **karma 99** no cut, level 5, max HP 150 (the binary-derived rule, computed independently in the test, with the quoted price charged exactly, the living Avatar and Shamino byte-identical, the shop still open); `H5` a living member is refused ("Thou hast no need of this art!"), nothing charged; `H6` **Mic** from the list and from the deal returns to "Anything else?" charging nothing and the shop still answers; `H7` too little gold raises nobody; `H8` the controls: `R` still asks to **Rest** at an innkeeper and for **Rations** at a tavern.
- **RED against RC4 production** (`a4-rc5-red-vs-rc4.log`, the final test with only the one-line fix reverted): `H1/50`, `H1/99` (R opens no list: the phase stays at the Heal/Cure/Resurrect prompt), `H6`, `H6b` — 27 checks ran, 4 RED. GREEN with the fix: **37 / 37**. Group U is GREEN on both (§17.3).
- **Mutation** (`native/core/tools/a4_rc5_mutation_check.py`, logs `a4-rc5-mutation.log`, `a4-rc5-mutation-rerun.log`): **19 / 19 killed**, restored tree GREEN. H1 `R` is Rest again (RC4) · H2 R is Cure · H3 R is Heal · H4 R is Resurrect in every shop (the inn loses Rest) · H5 the Barkeeper loses Rations · H6 the service maps Resurrect to Cure · H7 the deal skips `resurrect_apply` · H8 a fixed karma 99 · H9 HP left at 1 · H10 a living member is raised · H11 the raise is free · M1 only `Failed!` · M2 only `Not dead!` · M3 swapped · M4 printed for a dead target · M5 scroll not consumed · M6 the device skips the "Use on whom?" pick · M7 the pick always goes to member 0 · M8 the spell form also prints `Not dead!`. The first pass left three survivors — H4, H5 (the inn / tavern `R` were not pinned through real keys: gap, closed by `H8`) and an *equivalent* mutant (a cancelled pick dispatches nothing, so a stale pending item has no consumer; replaced by M8) — all three then killed.
- **Host suite:** serial `ctest` **201 / 201** (`a4-rc5-ctest.log`, 148.2 s; 200 + `a4_rc5_hotfix_runtime`). The TypeScript reference and the corpora were **not touched** (no reference code changed), so no vitest or generator re-run was needed.

### 17.6 The RC5 candidate and the firmware

**Production changed after RC4, so the identity moved:** `PROJECT_VER` `4.0.0-alpha4-rc4-debug` → **`4.0.0-alpha4-rc5-debug`** (commit `59c14b90`, after the fix `e7d4162f`). **RC4's image, filename, SHA and §16 record are preserved unchanged** (re-hashed after the RC5 build: `49e3962f…4414`; RC3 `9fd7fc8b…d542`; RC2 `608eb700…1e95` — `a4-rc5-identity.log`).

Fresh `--no-ccache` ESP-IDF 6.1 (`idf.py --no-ccache -B build-a4-rc5 reconfigure`, `ninja -C build-a4-rc5 -j 4 all`), 1,182 steps, run alone (no `ctest` during the build), **first attempt clean, 0 warnings** (`a4-rc5-fw-{configure,build}.log`). Guards (`a4-rc5-elf-checks.log`): `a3_04a_hotpath_check` **GREEN**, `a3_04b_iram_check` **GREEN**, `a3_04f_image_check` **GREEN**, `check_app_budget` OK.

| | |
|---|---|
| **File** | `native/targets/tdeck/build-a4-rc5/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` (byte-identical to `build-a4-rc5/openu5_tdeck.bin`) |
| **Size** | **1,047,408 B (`0xffb70`)** — the same size as RC4 |
| **SHA-256** | **`363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210`** |
| **Identity** | `FW 4.0.0-alpha4-rc5-debug`, **`Git 59c14b907399`** (HEAD `59c14b907399be8e8796dcd84946a60c942731c1`, **no `-dirty`**: the only `-dirty` strings in the image are the unrelated `value-dirty` / `input-dirty` log tags) |
| **App partition** | 1,310,720 B (`0x140000`): **263,312 B (20.1 %) free**; `check_app_budget.py` OK |
| **Launcher allocation** | 1,048,576 B (1,024 KiB); the next 64 KiB step is still **1,169 B away** |
| **Sections vs RC4** | flash `.text` **−4 B**; everything else unchanged (`a4-rc5-fw-size-diff.log`) |
| **Packs** | the SD resource pack and the audio pack are unchanged — no SD recopy |

### 17.7 The RC5 hardware retest — three checks, nothing else

Flash RC5, keep the SD pack. Do **not** repeat the underworld objects, save / Continue, combat clock, music, crown, well, UI, cheats or the ending; the naval cactus stays optional and unrun.

| # | Do | Expect | PASS |
|---|---|---|---|
| RC5-1 | Boot, open the Developer screen | `FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399` | |
| RC5-2 | Give the party **one In Mani Corp *Scroll*** (Developer menu (Alt+D) → Inventory: set the item index to **6**, and set the **Scrolls** quantity — not Spells). On the map: **(U)se** → *In Mani Corp Scroll* → pick a **living** member | the console reads, in order, `Scroll` / `Resurrection!` / **`Not dead!`** / **`Failed!`**; the member is unchanged; the scroll count drops by one. **If you see only `Failed!`, report the whole console** — in particular whether `Cast...` is above it (the spell was used) or `Scroll` / `Resurrection!` are, and whether the scroll count dropped | |
| RC5-3 | A dead companion, **karma below 98** (e.g. 50), a companion with a few hundred XP, enough gold (the quote at East Britanny is 237 gp). Enter a **healer**, press **R**, choose the dead companion, answer **Y** | the member list opens; the companion returns **alive** (`G`); **XP is cut to karma %** (1000 XP at karma 50 → 500), level and max HP follow the new XP (level = 1 + bit-length of XP/100; max HP = 30 × level), HP = the new max; gold falls by the quoted price; "Anything else?" **Y** and **H** still work | |

### 17.8 Does anything besides the retest remain before publication?

No **new** blocker comes out of this investigation: the healer defect is fixed and pinned, and the In Mani Corp route is verified correct on every path the host can drive. **If RC5-1 … RC5-3 pass, nothing in this follow-up blocks Alpha 4 publication**, and the known divergences D-90 … D-103 do not gate it. Two caveats stay honest: (1) **R4-4 is unexplained** — it passes the retest or it becomes a real, device-only defect (§17.3); (2) the items the project's own records already list as owed from earlier candidates — the RC3 dungeon-music retest (§15.10) and the Alpha 4 RC heap capture (§14.7) — are not part of this hotfix and were not re-asked of the user; they are prior bookkeeping, not defects found here. Nothing is tagged or pushed.

## 18. The Alpha 4 release (2026-10-04)

Documentation-only closeout. No production code, test, golden or fixture changed; the only commit after `e664c2d3` (the RC5 records) is the release-record commit, which carries the annotated tag `alpha4-release`.

### 18.1 The record

| Field | Value |
|---|---|
| Baseline at closeout | `main` at `e664c2d3`, tree clean apart from one untracked `native/core/a4-release-ctest.log` (an earlier 192 / 192 run of the RC2 tree; not part of this record), 47 commits ahead of `origin/main`, nothing pushed |
| Release tag | `alpha4-release` — annotated, on the final release-record commit (the commit that carries this section). It follows the convention of `alpha3-release`; the earlier `alpha4-ui2-hardware-validated` / `alpha4-end1-hardware-validated` tags are per-batch hardware tags and stay |
| **Firmware provenance** | **`59c14b907399be8e8796dcd84946a60c942731c1`** (`59c14b90`, "candidate identity 4.0.0-alpha4-rc5-debug"; the production fix is `e7d4162f`). The release-tag commit is a documents-only descendant and **did not produce the binary** |
| Artifact | `native/targets/tdeck/build-a4-rc5/launcher/OpenU5-TDeck-Alpha4.0.0-alpha4-RC5-Debug-Launcher.bin` |
| SHA-256 | `363a8fdae6eb714058a4545bad880e05c4d52919e9afd41de8d61e45cfa67210` (re-hashed 2026-10-04) |
| Identity | `FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399` (each string appears once in the file; no `-dirty`) |
| Size / free | 1,047,408 B (`0xffb70`) / 263,312 B (20.1 %) free in the 1.25 MiB app partition |
| Launcher allocation | 1,024 KiB (the next 64 KiB boundary is close — informational only; `check_app_budget` OK at RC5) |
| Host suite | **201 / 201**, serial, 148.2 s, fresh `ctest` on the release tree (`ninja: no work to do`: the build dir was current) |
| Release subset | `ctest -R "parity\|preserv\|golden\|release_blockers\|rc5"` **39 / 39** (23.8 s) |
| Mutation / guards | unchanged from RC5: 19 / 19 RC5 mutants killed, PARITY2's 110 / 110; firmware guards GREEN at the RC5 build (§17.6) |

**The two identities are different on purpose.** The tag names the repository's final release state; the firmware's `Git` names the commit it was compiled from. `git diff 59c14b90 alpha4-release` touches only `*.md` and `*.log` files (the RC5 records in `e664c2d3` and this closeout). A user who flashes the release `.bin` sees `59c14b907399`, and every claim about that image is checkable against it — the same principle as Alpha 2's Batch 55 and Alpha 3's `alpha3-release` (`PROJECT_HISTORY.md` §20.3).

### 18.2 Hardware validation — what the user reported

| Track | Result |
|---|---|
| A4-UI2, A4-SAVE1, A4-END1, A4-UI4/PRES1 | PASS (§2.8, §3.6, §7.19, §8.22.8) |
| A4-ENH1 (trackball, Developer entry, cheats and difficulty) | PASS |
| A4-ENH2 cheats | PASS |
| A4-ENH2 rules / tuning | accepted as implemented (balance adjustable later) |
| A4-POLISH3 keyboard backlight | PASS |
| A4-SAVE2 manual slot flow | PASS — user-reported |
| A4-UI3 save / load flow | PASS — user-reported |
| A4-SAVE3 import / export on the device | PASS — user-reported |
| Power-cycle Continue | PASS — user-reported |
| Ordinary smoke | PASS — user-reported |
| A4-PARITY1 D-78 crown possession gate | PASS — the literal `Success!` was not observed on a full-health target, but the gate behaviour passed: the spell was not `Absorbed!` |
| A4-PARITY1 D-79 worn-crown persistence | PASS |
| A4-PARITY1 D-81 dungeon commands | PASS |
| A4-PARITY1 D-50 wishing well | PASS |
| **RC3 dungeon-music retest (§15.10)** | **PASS** — normal dungeon entry, Developer dungeon teleport, ordinary dungeon actions, combat round trip, normal exit restores the surface music, teleport back restores the dungeon music, mute, Music Volume 0. No serial capture was obtained |
| **RC4 pass** | R4-1 identity PASS · R4-2 New Journey underworld skiff + bodies PASS (D-82) · R4-3 save / power-cycle / Continue PASS · **R4-4 In Mani Corp living-target scroll FAIL as originally reported** · **R4-5 healer `R` FAIL** · R4-6 combat clock PASS (D-88) · R4-7 naval cactus NOT TESTED (optional) |
| **RC5 retest** | RC5 identity **PASS** · In Mani Corp scroll on a living target **PASS** (the expected console behaviour on the device) · healer `R` resurrection **PASS** (after the key-routing fix) |
| **RC heap capture (§14.7)** | **NOT RUN — waived by the user for the Alpha 4 release.** No heap defect was observed. No heap figure exists for Alpha 4 and none may be quoted; this is not a release blocker |
| A4-SAVE3 real-DOS round trip | optional, never run — the PC bridge is validated on the host and on the device's import / export only |

All device results are the user's reports; none carries a serial capture or an itemised per-line table beyond what is written here. RC5 is hardware-validated and **no further broad regression is required**. The naval cactus (R4-7) stays optional and unrun and does not block publication.

### 18.3 What was stale, and what supersedes it

| Stale statement (kept where it was written) | Now |
|---|---|
| "RC3's music retest is still owed" (§15.10, `ALPHA4.md`) | PASS (§18.2) |
| "the RC heap capture is still required / none exists" (§14.7, §14.9, §17.8) | waived by the user; not run; not a blocker |
| "PARITY1's checks are unrun" (§9, §14.3, §14.7) | D-50, D-78, D-79, D-81 PASS |
| "the RC4 failures still block publication" (§17.3, §17.8) | R4-5 fixed and retested PASS; R4-4 retested PASS on RC5 (the reported failure was **not reproduced on the host**, so its original cause remains unexplained; §17.3's caveat is closed by the device result) |
| "RC5 hardware is pending / three-check retest owed" (§17.6, §17.7) | the retest was run: PASS |
| "SAVE2 / UI3 / SAVE3 hardware pending" (§4.12, §5.16, §6.16, §14.7) | user-reported PASS |

### 18.4 PARITY2 final status and the RC4 → RC5 history

PARITY2 closed its scoped debt: **D-82, D-83, D-84, D-85, D-86, D-87, D-88, D-89**, plus the In Mani Corp living-target text and the corrected jail / inn housekeeping note (§16). Hardware evidence: D-82 (R4-2) and D-88 (R4-6) PASS on RC4; D-83 / D-84's healer path PASS on RC5. RC4's two failures: **R4-5** — the healer's `R` did nothing because `UiSession`'s shop key map sent `Rest` for `r` everywhere but a tavern (a one-line fix, `e7d4162f`, pinned by 37 real-runtime checks, RED against RC4); **R4-4** — the scroll on a living target showed only `Failed!` on the device, not reproduced on any host route, pinned by 16 checks and then PASSED on the RC5 image. RC1 – RC4 stay as history (`aae348ac`, `24c666e1`, `6b6ab8a4`, `859f1605`; SHAs `67100a51…145a`, `608eb700…1e95`, `9fd7fc8b…d542`, `49e3962f…4414`); RC5 supersedes them.

### 18.5 Known, non-blocking, future work

- **D-90 – D-103** (fourteen post-PARITY2 divergences, ledger rows; none fixed and none marked release-critical).
- The older open ledger rows and the deferred items: a softer death penalty, Advanced Cheats, the credits curtain, the intro's story plates and title figures, WASD in the frontend menus (D-1).
- Optional, unrun checks: the naval cactus (R4-7), the A4-SAVE3 real-DOS round trip, D-74 and the arena's single `Cast...`.
- An RC heap capture, if the project later wants Alpha 4 heap numbers.
- Packaging: the Launcher allocation is 1,024 KiB with 263,312 B free in the app partition; the next 64 KiB boundary is close, so the next feature batch should run `check_app_budget` early.
- Hygiene found by the publication audit (§18.6).

### 18.6 Publication-readiness audit (read-only)

Nothing was deleted or changed. Verdict: **safe and sensible to make public once the owner has decided the first item below.**

- **Clean:** `original/`, `game/assets`, `reference/`, `logs/`, the espejo-tour saves and routes and `node_modules` are git-ignored and **zero files under `original/` are tracked**; no `.gam` / `.ool` / `.ovl` / `.dat` / `.tlk` / `.exe` / `.wav` / `.mid` / `.mp3` / `.ogg` is tracked; no SD pack, audio pack or Launcher image is tracked (the one tracked `.bin` is the 112-byte `a3-01-audio-stock.bin` capability fixture); no secret-shaped assignment (`api_key`, `password`, `token` set to a long literal) turns up in tracked text; `NOTICE` and `README` state that no Ultima V data is distributed.
- **Decide first — `native/core/fixtures/fixed-maps.txt` (≈80 KB) and `dungeon-maps.txt` (≈14 KB):** these are numeric tile-index dumps that look like Ultima V's fixed-location and dungeon maps, extracted from the owner's game files. They are the one place original map content may be redistributed, which contradicts the README's "no maps" statement. They are inputs to the host parity tests. Options: keep (if the owner judges transformed tile indices acceptable), or git-ignore them and regenerate on the host from the user's own files. Not changed here because that alters test inputs.
- **Hygiene — `native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md`:** one historical paragraph cites a developer-machine path (`<home>\.platformio\…`). About 220 tracked build / test logs (`native/core/*.log`, `native/targets/tdeck/*.log`) also carry the same user name in absolute paths. **Resolved in the publication-hygiene pass:** the user-home and repository-root prefixes in those logs and in the human-written Markdown were normalised to `<home>` / `<repo>` (or repository-relative); no hashes, timestamps, results or offsets were touched, and the logs stay as the evidence record. The one untracked file, `native/core/a4-release-ctest.log`, is a stale scratch log and is not committed.
- **Size:** the tracked fixtures are large (`items.txt` 42 MB, `advanced-combat.txt` 31 MB, …; 4,874 tracked files in total) — a heavy clone, not a hazard. No `build-*` directory is tracked.
- **Public-facing docs:** the root `README.md` describes the TypeScript browser port and said nothing about the T-Deck; it now links to `ALPHA4.md` and `ALPHA4_RELEASE_NOTES.md`.

### 18.7 Verdict

**Alpha 4 is release-ready, with RC5 as the hardware-validated firmware image.** Nothing was pushed; no GitHub Release was created. After the owner's inspection: `git push origin main alpha4-release`, then a GitHub Release carrying the `.bin`, its SHA-256 and `ALPHA4_RELEASE_NOTES.md`.
