# Alpha 4 — UI track

Alpha 4 is a UI/presentation track on top of the released Alpha 3 (`alpha3-release`, `eb1bf5e7`). Each batch keeps its own section; a finding is recorded where it was made and not merged into an earlier batch's conclusion.

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
- **Latent (not changed, queued as a separate task).** The save target is chosen by sequence regardless of validity. If the newest generation is refused (torn files under an intact commit, or a failed post-write check after the commit rename), the next save overwrites the older, valid one. A failure of that save leaves no restorable generation. Test L8 shows the first half. The fix belongs to the storage batch that §2.4 describes.

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
