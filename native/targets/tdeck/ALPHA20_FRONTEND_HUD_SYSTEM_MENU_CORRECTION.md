# Alpha 2.0 frontend, save, View, HUD, and System Menu correction

This report describes the packaged host-validated correction build. No new behavior in this pass has been physically validated on a T-Deck yet.

## 1. Settings flicker root cause and fix

The Settings state used the generic frontend `body_changed` path. Every cursor move or value change cleared the entire 320-pixel-wide body to black and then redrew every row, producing a large intermediate blank region on the LCD.

Settings now uses the retained frontend-row renderer shared with stable menus. Entry may redraw the complete Settings body once. A value change redraws one 280x9 row (2,520 pixels); moving the selection redraws the old and new rows (5,040 pixels). It never clears the screen between the erase and replacement operations. Runtime diagnostics are:

`SETTINGS_RENDER reason=... full_redraw=... dirty_regions=... pixels=... us=...`

## 2. Exact original Return-to-the-View behavior found

`INTRO.OVL` calls the `FONT.OVL` scene engine for Return to the View. It is not Lord British's room and is not the separate `BRITISH.PTH` riders-over-title attract. The authoritative data is `MISCMAPS.DAT[704:]`: four 19x4 tile maps at offsets 704-1215 and a 655-byte script at offsets 1216-1870. The last byte is RESTART, so the script loops until input.

The scenes and original titles are:

1. `The Summoning`: the Avatar's studio, including bed, plant, door, and mirror; the Avatar walks east, the mirror changes 0x9e/0x9d, the door changes, and the actor returns.
2. `The Journey`: field/path exterior; the Avatar walks to a moongate at (13,1), enters, and the gate falls.
3. `The Arrival`: water/coast exterior; a moongate at (9,1), Avatar traversal, three tile-0xfc Shadowlords converging, a five-step summon beam, the summoned figure, then dissolves and departure.
4. `The Welcoming`: rock dwelling with bed, table, kitchen, seated/bed actors, and choreographed entrances/exits.

The script uses the 16 proven PLACE, ERASE, MOVE, TICK, MGRISE, MGFALL, SCENE, ANIM7, ANIM8, RESTART, SETTILE, SUMMON, CLEAR, WALK, LOOP, and NEXT opcodes. WALK consumes seven 55 ms scene frames. MGRISE/MGFALL use 15 partial-gate frames and their two-frame event tail.

## 3. View/demo implementation

The placeholder circling characters were replaced by a deterministic core interpreter and native renderer. The packed resource contains the four maps and exact 655-byte script. Rendering uses the original terrain and mobile tile banks, a column curtain, existing animated-water/fire/tile treatments, Bayer pixel dissolves, partial moongate rise/fall, and the five-step summon beam. Any key returns to the menu.

The interpreter owns only demo-local terrain, actor slots, program counter, loop counter, phase, and frame state. It has no references to `GameState`, `TurnState`, RNG, saves, settings, NPCs, clocks, or recovery generations.

## 4. Initial-save failure root cause

The SD FAT mount permitted only three simultaneous files. At character-creation completion, the Alpha resource pack, tile resource pack, and SD diagnostic logger could already occupy all three FATFS descriptors. Opening the first new-save temporary file then failed with `EMFILE` (errno 24).

## 5. Exact failing persistence stage

The pre-fix failure was the first generation payload open: `write-gam-temp`, before CRC validation or commit creation. The diagnostic build now shows the precise stage and errno instead of only `Initial save could not be created`.

## 6. Filesystem and path corrections

- The FAT mount descriptor allowance is now six.
- Both read-only resource packs are explicitly closed after their validated contents have been copied/cached in PSRAM.
- Save code consistently uses the mounted VFS root `/sd/ultima5/saves`; user-visible SD-card paths remain `/ultima5/...`.
- `/sd/ultima5` and `/sd/ultima5/saves` are created and verified as directories on a clean card.
- Free space is checked before encoding/writes.
- GAM, OOL, and JSON are written to temporary files, flushed, fsynced, CRC-read back, renamed, and followed by a temporary commit record and final commit rename.
- A post-commit semantic reload validates the complete generation; validation was not weakened and no prior generation is required.
- Every stage logs `NEWGAME_SAVE stage=... result=... esp_err=... errno=... path=...`.

## 7. Journey Onward and recovery-load UX

The top-level Load Game peer was removed. Journey Onward now opens `Continue Latest` and `Recovery / Load Previous`; the latter exposes the two validated generations. The in-game `Load / Save Management` page provides the same latest/recovery distinction.

## 8. Gameplay HUD layout

- Gameplay viewport: `(4,4)` through `(179,179)`, 176x176.
- Upper-right party block: `(184,4)`, 136x48, six 8-pixel rows.
- Right world-status block: `(184,52)`, 136x32, four 8-pixel rows.
- Right running log: `(184,88)`, 136x152.
- Four-pixel gutters separate the viewport, reserved strip, and right column.

## 9. Reserved touch-region bounds

The future touch region is exactly `x=4..179, y=184..239` (176x56). No viewport status, coordinates, day/time, wind, mode, transcript, or other persistent text is drawn there. Future touch handlers must emit the existing semantic `UiAction`/`SystemMenu` actions.

## 10. Transcript font metrics

The visible bitmap glyph is 5x7 pixels in a 6x8 character cell. The right column fits 22 characters per line and 19 visible transcript rows. Word-aware wrapping remains in `UiSession`; the prompt and current input consume the final one or two rows of the same log rather than a detached terminal widget.

## 11. Party/status layout

Up to six active members fit in six compact rows. Each row contains an active marker, party order, seven-character name, current/max HP, and one-character Ultima condition. The current member is green and prefixed with `>`.

## 12. World status layout

The four right-side rows contain meaningful location name, `Day` plus HH:MM, a 12-cell sun/moon sky track plus Felucca/Trammel phases, and wind direction. This reuses the already validated game/turn/moon state and changes no mechanics.

## 13. Debug information removed

Raw map coordinates and internal modes such as EXPLORE/TARGET/COMBAT are absent from normal play. They remain available through Developer diagnostics. Target prompts still show target-specific information when it is part of the active interaction.

## 14. In-game System Menu behavior

The menu contains Resume, Save, Load / Save Management, Settings, and Return to Title. It intercepts semantic input before gameplay command routing, so opening, navigation, setting changes, and Resume consume no turn and advance no game clock or RNG. It is an overlay session separate from `UiSession`, preserving active prompts, request ID, typed text, transcript, and selection state. Resume redraws gameplay from that unchanged state.

## 15. System Menu keyboard shortcut

`Alt+M` emits the semantic `UiActionKind::SystemMenu`. Mic short press remains Cancel/Back and closes the System Menu at its root without being repurposed.

## 16. Settings integration

Title and in-game Settings use the same `FrontendSettings` object and `AlphaSettingsService` JSON backend. The shown controls are brightness, Movement Mode preference, trackball response, and Developer visibility in the developer build. Reserved UI-size and unimplemented audio/music/touch controls are not presented as functional. Brightness is applied on GPIO 42 with 5 kHz, 10-bit LEDC PWM; movement and trackball changes update the existing input adapter.

## 17. Test results

- Final native/core/Alpha regression suite: 53/53 passed, including frontend, save/persistence parity, shops, dialogue, inventory, transport, dungeon, combat, movement, presentation, input, quests, and all fixture drift checks.
- Extractor/reference suite: 31 files passed; 232 passed, 2 skipped.
- Targeted INTRO/gypsy/native-save suite: 4 files and 134 tests passed.
- Focused new assertions cover one-row Settings mutation, all four View scenes and View effects, View state isolation, Journey/recovery UX, new-journey GAM/OOL/JSON semantics and reboot decode, HUD bounds/metrics, System Menu state/RNG hashes, exact `UiSession` prompt/input restoration, and Alt+M/Mic semantics.
- ESP-IDF v6.1 warnings-as-errors build passed. The device smoke harness compiled into the image.
- Partition check passed. Launcher packaging passed. `git diff --check` passed.

## 18. Flash, RAM, and PSRAM impact

Against `build-alpha20-alpha2c`, the final map delta is +15,592 image bytes: +11,716 flash code, +2,560 flash data, and +2,620 DIRAM (including +1,304 BSS and +836 data). The final LEDC-linked image is 736,768 bytes and leaves 0x4c200 bytes (30%) of the 1 MiB app partition free.

The exact View resource adds 975 bytes in PSRAM (16-byte header, 304 map bytes, 655 script bytes). Resource pack files are closed after caching, recovering two FATFS descriptors. No large framebuffer was added: the existing 176x176 RGB565 viewport buffer is reused for the 304x64 View band.

## 19. Launcher package

`C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-correction\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

Final packaged size: 736,768 bytes. SHA-256: `096f52fc413798ef5f0b39ce53752905d044c1e5667b0c7bec49e2206d287758`.

The matching SD resource pack is `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin` (1,607,019 bytes; SHA-256 `6519dca0347ad56843893272f1e9a50033011ba952fbdd5a17a1201301feb6a5`).

## 20. Short physical retest checklist

1. Install the Launcher image and copy the matching resource pack to `/ultima5/openu5-alpha1-resources.bin`.
2. Open Settings; sweep every shown value and confirm only the affected row changes, with no black flash. Confirm brightness changes visibly.
3. Run Return to the View through all four titled scenes; verify curtain, studio/mirror, both moongates, Shadowlord dissolves/summon beam, rock dwelling, loop, and any-key return.
4. On a card with no `/ultima5/saves`, create a character. Confirm GAM/OOL/JSON/commit files appear, gameplay starts, reboot, and Journey Onward restores the same character/state.
5. Confirm Journey Onward exposes latest plus recovery one level down and no top-level Load Game exists.
6. Inspect the HUD with six members and during dialogue/combat: reserved lower-left strip empty, meaningful location/world status on the right, no raw coordinates/mode, readable 22x19 running log, typed input at its bottom.
7. Open `Alt+M` during an unfinished prompt; Resume with Mic and confirm typed text/prompt remain and no turn/time/RNG-visible event occurred. Exercise Save, recovery load, Settings, and Return to Title.
