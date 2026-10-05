# Alpha 2.0 frontend and New Journey foundation

Status: host-built and host-tested on 2026-09-17. The new screens have not been physically validated on a T-Deck Plus.

## Authoritative behavior traced

The implementation was derived from the repository's reverse-engineered Ultima V sources and extracted data rather than from a new launcher design:

- `re/notes/intro.md` and `re/disasm/INTRO.OVL.asm` define the title controller, six original commands, input, timeout, 21-scene introduction, acknowledgements, and Journey gate.
- `re/notes/gypsy.md`, `game/assets/questions.json`, and `FONT.OVL` analysis define character creation.
- `game/assets/intro-pics.png`, `intro-pics.json`, and `intro-scenes.json` contain extracted original title art, subtitle fire frames, and Summoning text.
- `game/src/skin/fiel/sky.ts`, `game/src/skin/fiel/skin.ts`, and `re/notes/cielo-176-acta.md` define celestial and wind presentation.

### Original startup flow

INTRO.OVL loads TITLE.BIT, BRITISH.BIT, and BRITISH.PTH. Four figures walk paths from `(44,68)`, `(64,94)`, `(143,78)`, and `(167,105)` while the Ultima V title composition is shown. The title's idle logo also uses the animated `ultima:1..4` fire subtitle frames.

The original menu is an ornamental six-row menu with this exact action table:

| Key | Original label | Original action |
| --- | --- | --- |
| J | Journey Onward | Load SAVED.GAM/OOL and enter play; reject an empty Avatar name. |
| C | Create New Character | Run the FONT.OVL gypsy creation sequence. |
| T | Transfer from Ultima IV | Import and convert an Ultima IV `party.sav`. |
| U | Ultima V Introduction | Play the 21-scene “The Summoning” sequence. |
| A | Acknowledgements | Show STARTSC.16 with the curtain reveal. |
| R | Return to the View | Run the attract/demo view, then return to the menu. |

Original navigation maps keys 1/3 to previous and 2/4 to next, wraps over all six rows, accepts Enter or Space, and supports direct `JCTUAR` hotkeys. Its wait loop runs for 200 ticks; timeout dispatches `R`, so inactivity returns to the View instead of starting a game. Menu music is serviced during the wait loop. Audio is deliberately not implemented in this milestone.

“The Summoning” is a 21-scene INTRO.OVL sequence using STORY.DAT, STORY1-6.16, TEXT.16, and proportional text. Acknowledgements uses STARTSC.16 and a center-out curtain. Journey Onward prints the original no-active-game guidance if the first party record has an empty name. Transfer is a substantial import/conversion flow and is explicitly deferred here.

## Implemented state machine

`FrontendSession` is platform-independent and separate from gameplay `GameState`:

```text
Title -> IntroAnimation -> AttractDemo -> MainMenu
                                   ^         |
                                   |         +-> Continue -> EnterGame / Error
                                   |         +-> NewJourney -> CharacterCreation
                                   |         |                       |
                                   |         |                 initial save -> EnterGame
                                   |         +-> Load -> EnterGame / Error
                                   |         +-> Settings -> MainMenu
                                   |         +-> Credits -> MainMenu
                                   +---------+-> Return to the View
```

The boot title holds for 1.4 seconds, its lead animation for 4.2 seconds, and the gameplay preview for 12 seconds before the menu. Menu inactivity enters the preview after 12 seconds, an approximation of the original 200-tick idle dispatch. Any input returns from the preview cleanly. Presentation timers never advance the game clock, consume gameplay RNG, or write a save. These boot timings and the abbreviated title composition are explicitly PARTIAL, not a claim of witness-level title parity.

The original `JCTUAR` actions and labels remain the first six rows. `Load Game` and `Settings` are port extensions after those rows. `Developer` is a ninth row only in developer builds and only when enabled in persistent settings. Transfer remains visible in its original position and reports that it is deferred rather than silently removing the original feature.

## Title, introduction, and attract presentation

The menu uses the exact extracted `ultima:0` title image and cycles all four extracted `ultima:1..4` subtitle-fire frames. This keeps the screen immediately identifiable as Ultima V rather than as a generic launcher.

The Introduction action traverses all 21 authoritative scenes, using the extracted STORY.DAT narrative text. A key advances each waiting scene; the fabricated five-second timer was removed. Scene zero's exact non-wait cadence and the six story illustration compositions remain a visual/timing fidelity gap.

Return to the View and idle timeout display a live, animated 11x11 gameplay rendering produced by the normal native map renderer. It is presentation-only: it reads the initialized template state, uses wall-clock animation ticks, and never dispatches commands or mutates the user's live/save state.

Acknowledgements uses the exact extracted 288x137 STARTSC.16 panel. The center-out curtain raster effect remains a visual-fidelity follow-up.

## New Journey semantics

The implementation follows FONT.OVL rather than exposing arbitrary sliders:

1. Ask “By what name shalt thou be known?” and accept at most eight characters. An empty name cannot proceed.
2. Accept original `M` or `F` selection, stored as GAM values `0x0b` and `0x0c`.
3. Start from INIT.GAM Avatar stats STR/DEX/INT = 15/15/15.
4. Run the original eight-virtue elimination bracket in rounds 4 + 2 + 1, producing seven questions.
5. Use all 28 extracted QUESTION.DAT pair questions and the original Honesty, Compassion, Valor, Justice, Sacrifice, Honor, Spirituality, and Humility stat tables.
6. Add the winning virtue's stat contribution after each answer.
7. Write final INT and current MP to the accumulated INT, DEX to accumulated DEX, and STR to `max(accumulated STR, 20)`.

The menu seeds the original recurrence with the INTRO clock hash, the bracket consumes that stream, and the resulting seed becomes the initial gameplay RNG state. Host tests inject a timestamp and use seed zero only for deterministic golden vectors. Attract rendering remains read-only and consumes no RNG.

Only name, gender, STR, DEX, INT, and current MP are replaced in INIT.GAM. Class remains Avatar (`A`); level, experience, HP/max HP, equipment, inventory, party members, position, time, quest flags, and all other initial semantics remain the authoritative INIT.GAM values. This also preserves starting equipment and inventory without duplicating them in frontend code.

## Initial save, Continue, and Load

Completing the seventh answer reloads the authoritative INIT.GAM template, applies the new identity, clears volatile runtime command/enemy/terrain state, synchronizes the world, and immediately writes a normal GAM/OOL/JSON generation through `AlphaSaveService` before entering play. The creation-only writer preserves INIT.GAM's inactive object-window bytes and seeds SAVED.OOL from the exact 512-byte INIT.OOL resource; byte-level golden tests prove that only the original identity/stat fields differ in the first GAM.

Save discovery inspects both existing two-generation commit slots. It validates their commit metadata and existing GAM/OOL/JSON integrity using the shared persistence functions. It does not implement a second save parser.

- Journey Onward/Continue selects the newest complete valid generation with the existing recovery rules.
- Load Game exposes both physical recovery generations and loads the selected valid generation.
- Empty slots are labeled empty, committed but invalid slots are labeled corrupt, and failed Continue/Load actions return to a useful frontend error.
- A failed new save leaves the prior committed generation intact and does not enter gameplay.

## Settings

Settings live independently at `/sd/ultima5/settings.json`, written through a temporary file and rename. Version-one fields are:

- display brightness, 10-100%;
- default Movement Mode;
- trackball response: deliberate, normal, or fast;
- UI size preference, reserved until alternate layout metrics can be applied safely;
- developer entry visibility, additionally gated by build capability;
- future sound volume;
- future music volume;
- future touch-control enablement.

Movement Mode and trackball response are applied to the existing semantic input adapter. Defaults preserve the Alpha 1.4 behavior. Brightness is persisted as backend state in this foundation; hardware backlight application is left for physical calibration. Audio is not implemented.

## 320x240 gameplay layout

```text
+----------------------------+---------------------+
| 12-cell sun/moon sky band  | location / mode     |
| +------------------------+ | party HP/status     |
| |                        | |                     |
| | 176x176 game viewport  | | running transcript  |
| |                        | |                     |
| +------------------------+ | current prompt      |
| day/time + wind direction  | current input       |
|                            |                     |
+----------------------------+---------------------+
```

The command prompt and current text/number input now terminate the running right-hand transcript instead of occupying a detached terminal box. Dialogue, shops, inventory, combat messages, and prompts continue through the shared `UiSession` transcript and wrapping pipeline. The right panel shows all active party members; the lower-left strip shows authoritative day/time and wind. The unused native-only “future context actions” label was removed.

Future touch controls must produce existing `UiAction` values. No parallel command path was introduced.

### Celestial and wind indicators

The HUD reads only authoritative `GameState` and `TurnState`:

- sun cell: `17 - hour`, visible only in cells 0-11, glyph `0x2a`;
- Felucca cell: `8 - hour`, add 24 only when below -12, glyph `0x30 + phase`;
- Trammel cell: `2 - hour`, add 24 only when below -12, glyph `0x30 + phase`;
- phase comes from the turn's latched phases, with the extracted daily phase table only as validation fallback;
- wind labels are exactly `Calm  Winds`, `North Winds`, `South Winds`, `East  Winds`, and `West  Winds`.

The bands are hidden in locations/floors where the original does not draw the surface sky. They redraw whenever the gameplay screen redraws, but rendering never changes time, moon phases, wind, RNG, or turns.

## Preservation and boundaries

The frontend consumes the same semantic actions as the established keyboard/trackball adapter. Mic/back mapping, Movement Mode, semantic WASD, keyboard transport fixes, debug menu behavior, teleport, shops, inventory naming, combat targeting, resource validation, and the smoke-test harness were not redesigned. Their native regression/parity tests remain enabled.

Deferred by milestone scope:

- Ultima IV character transfer;
- audio playback, even though future volume fields exist;
- final touch-button artwork and hit testing;
- physical brightness calibration and alternate UI-size metrics;
- device raster composition of the six Summoning illustration plates and the exact Acknowledgements curtain;
- a gameplay-to-title quit command, because the current gameplay command set has no authoritative return-to-title action.

## Automated verification

`native/core/tests/frontend_test.cpp` covers title/menu/idle/attract transitions, all 21 Introduction scene slots, original hotkeys, missing-save error, character creation, EnterGame, explicit load selection, settings persistence, INIT.GAM identity application, GAM/OOL/JSON export and reboot import, inventory preservation, generation fallback, and representative sun/moon/wind states.

The complete host suite passed: 53/53. This includes the existing movement, input, presentation, shops, dialogue, combat, persistence, travel, quest, dungeon, and debug regressions. The ESP-IDF 6.1 ESP32-S3 build also completed with `-Werror` and passed the 1 MiB application-partition size check.

## Build artifacts and measured impact

- Launcher image: `build-alpha20-final/launcher/OpenU5-TDeck-Alpha2.0.0-alpha1-Debug-Launcher.bin`
- Launcher SHA-256: `cc67d395d145e3e5927c70a92d3a946cdd9a21c27f7a22b445e8b6824592e365`
- App image size: 709,136 bytes; 339,440 bytes remain in the 1 MiB application partition.
- Alpha 1.4 alpha2 comparison: 692,576 bytes, so this firmware adds 16,560 bytes of flash.
- Resource pack: `native/assets/openu5-alpha1-resources.bin`, 1,605,980 bytes, SHA-256 `9c028f7dac268285e300f69ffb521df60174a0721f02fb92b3c9f3d6fcbb2ba5`.
- New frontend/save-seed PSRAM-backed payloads are 281,600 bytes of animated title frames, 11,160 bytes of introduction text, 6,135 bytes of questionnaire text, 78,912 bytes of credits art, and 512 bytes of INIT.OOL: 378,319 bytes total. Runtime free-RAM margins still require physical measurement; no physical RAM or timing claim is made.

## Physical test checklist

1. Copy the exact resource pack to `/ultima5/openu5-alpha1-resources.bin` on the SD card and launch the packaged binary.
2. Confirm the extracted Ultima V logo/fire animation, automatic gameplay preview, original six menu rows, and idle return to the View.
3. Navigate with trackball, arrows/WASD, Enter/Space, direct `JCTUARLS` keys, and Mic/back; verify Movement Mode behavior is unchanged after entering play.
4. Play/advance all 21 Introduction scenes and return to the menu; inspect wrapping and readability.
5. Create male and female characters with several questionnaire answer paths; power-cycle immediately after entering play and Continue.
6. Verify name, gender, STR/DEX/INT/MP, inventory, equipment, party, location, time, and quest defaults survive reboot.
7. Corrupt one recovery generation and verify Continue selects the other; inspect explicit Load labels and errors.
8. Change settings, power-cycle, and verify persistence plus trackball/Movement Mode application. Calibrate brightness separately.
9. Advance the real game clock through dawn/day/night and change sailing wind; compare the live celestial/wind band without extra turns.
10. Re-run Mic mapping, movement/WASD, keyboard transport, debug, teleport, shops, inventory names, combat targeting, resource mismatch, and smoke-test checks from Alpha 1.4.
