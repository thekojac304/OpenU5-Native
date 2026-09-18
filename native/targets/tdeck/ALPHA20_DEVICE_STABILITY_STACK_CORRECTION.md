# Alpha 2.0 device-stability / stack correction

Date: 2026-09-17

This pass treats the supplied physical trace as authoritative. It preserves the Alpha 2.0 frontend, HUD, System Menu, Settings, Return-to-the-View, persistence, input, gameplay, debug, smoke-test, and resource-validation behavior. Physical acceptance remains pending on the T-Deck; no claim below substitutes host or static analysis for that retest.

## 1. Largest stack consumers

Compiler stack-usage reports were enabled for the T-Deck component. The important before/after frames are:

| Function | Before | After | Change |
| --- | ---: | ---: | ---: |
| `DeviceSmokeTests::run` | 5,904 | 5,904 | unchanged; largest remaining frame and an explicit diagnostic action |
| `AlphaSaveService::save` | 5,520 | 1,104 | -4,416 |
| save `candidate` validation | 4,944 | 288 | -4,656 |
| save `restore_candidate` | 4,864 | 160 | -4,704 |
| `AlphaRuntime::render` | 3,792 | 2,128 | -1,664 |
| `AlphaSaveService::inspect` | 2,688 | 64 | -2,624 |
| `save::restore_core` | 2,608 | 2,608 | unchanged core decoder |
| `Board::show_alpha` | 2,304 | 368 | -1,936 |
| `enter_npc_map` | 2,192 | 2,192 | unchanged core operation |
| `Board::show_frontend` | 1,504 | 1,520 | +16 for the authentic creation-art branch |
| `AlphaResourcePack::load` | 1,392 | 1,392 | unchanged |

The former startup chain was `app_main -> AlphaRuntime::initialize -> AlphaSaveService::inspect -> candidate -> load_native_state -> restore_core`. Its nested static frames accounted for roughly 11 KiB before library/RTOS overhead, consistent with the physically measured 11,664-byte high-water use and 624-byte margin.

The former New Journey commit chain added a 5,520-byte save frame and then nested the 4,944-byte validator and 2,608-byte core restore. That path could exceed the 12,288-byte task allocation even without unrelated corruption. The corrected nested save/validation frames are approximately 5 KiB before library/RTOS overhead.

The corrected frontend render chain is approximately `app_main` 544 + `AlphaRuntime::render` 2,128 + `Board::show_frontend` 1,520, plus draw helpers. The corrected gameplay HUD chain substitutes the 368-byte `show_alpha` frame. System Menu and Settings use the same bounded frontend view owner.

## 2. Root cause of the 624-byte margin

The 12,288-byte main task combined several large automatic objects in nested calls:

- save inspection owned decoded game/turn/command/world/NPC/JSON state on the stack;
- save validation and transactional restore created additional copies in nested frames;
- render owned a `PresentationSnapshot` and frontend/layout objects as automatic variables;
- gameplay HUD composition owned a full rendered-transcript line array;
- debug rendering owned a `DeviceDebugScreen` locally.

Startup save inspection exercised the deepest of these chains before the first idle frontend frame. The physical 624-byte result therefore reflects real task exhaustion, not a keyboard or display symptom.

## 3. Objects and buffers moved off stack

- A single 10,080-byte, main-task-owned persistence workspace now lives in PSRAM. It owns candidate generations, validation/transaction state, GAM, OOL, JSON, and commit scratch.
- `PresentationSnapshot` and `FrontendView` are persistent `AlphaRuntime` members.
- `DeviceDebugScreen` is a 600-byte PSRAM object.
- The 19-element `UiRenderedLine` array (1,976 bytes) is persistent `Board` scratch.
- The 320 x 152 character-creation composition surface is a 97,280-byte PSRAM buffer.
- The original creation sprites are loaded directly into a 217,746-byte PSRAM resource blob.

No framebuffer, resource blob, save image, or decoded save-state aggregate is intentionally allocated on the main-task stack.

## 4. New main-task stack size

`CONFIG_ESP_MAIN_TASK_STACK_SIZE` and its generated compatibility alias are 24,576 bytes. This adds 12,288 bytes of internal RAM over the physically failing build.

## 5. Expected worst-case stack margin

Using only the larger task allocation and making no credit for the frame reductions, the prior physical high-water use of 11,664 bytes implies a conservative 12,912-byte margin. The corrected startup chain also removes about 7 KiB of nested adapter frames, so its expected margin is higher still. The largest remaining individual frame is the explicitly invoked 5,904-byte smoke harness.

The firmware logs byte-valued high-water marks at initialization, frontend intents, every New Journey save stage, after the first render, and each five-second heartbeat. The required >=4 KiB margins must still be confirmed on hardware for every requested route.

## 6. Developer reboot root cause

Developer was not actually constructing a New Journey, but the shared frontend-intent handler mislabeled every intent as `CHAR_CREATE` and then unconditionally performed a two-slot semantic save inspection after opening Developer tools. That inspection entered the deepest startup validation chain with only 624 bytes of historical margin and overflowed the main task.

Developer now has a dedicated early path: it opens the debug menu, does not call character-creation completion, does not inspect or write storage, and emits `DEVELOPER_ENTRY storage_touched=0 new_journey_touched=0`. Generic routing is logged as `FRONTEND_INTENT`; `CHAR_CREATE` is reserved for actual creation progress.

## 7. Character-creation completion result

The existing name, gender, questionnaire, final derivation, INIT construction, and EnterGame flow is retained. Creation progress logs include the answered-question count and stack margin. Final creation now reaches the reduced-stack persistence path.

The host frontend/developer/gameplay suite passes, the complete target firmware builds, and the stack analysis removes the proven overflow. Physical completion through EnterGame is pending the checklist below.

## 8. New Journey persistence result

GAM/OOL/JSON encoding, atomic temporary writes, CRC readback, renames, commit-last semantics, semantic validation, generation fallback, and detailed `NEWGAME_SAVE` stage diagnostics remain intact. The adapter now reuses its PSRAM workspace, avoids a redundant second native-state decode during slot inspection, and logs the stack margin at each commit stage.

All 53 host tests pass, including `persistence_parity`, frontend, gameplay entry, and recovery-related paths. Target compilation and packaging pass. The physical SD transaction is deliberately reported as pending rather than inferred from host results.

## 9. Keyboard/input findings

The Alpha 1.4 transport remains unchanged: raw-mode command, INT-assisted reads, 45 ms compatibility polling, bounded I2C timeouts, backoff, resynchronization, and modifier-edge preservation are retained. Enter remains the existing matrix-to-Confirm mapping.

The input service now records a rate-limited `INPUT_SERVICE_GAP` when the single main loop goes unserviced for >=75 ms, including the worst observed gap. This separates render/main-loop starvation from `ESP_ERR_INVALID_RESPONSE`; the latter still drives the existing transport recovery. The event is copied to the SD diagnostic log.

## 10. Startup-display correction

Display initialization now draws a complete boot title/version/status frame while the backlight is off, then enables the backlight. Existing first-frame cache invalidation and full-clears are retained for runtime identity, title/intro, View/attract, and menu layout transitions. This removes exposure of controller-clear or partially initialized content without redesigning the frontend.

## 11. Character-creation art

The prior build did not contain the original creation artwork. This pass adds all 11 existing `create:0` through `create:10` sprites from the repository's extracted original `intro-pics.png` / atlas data (the reference CREATE/FONT artwork), packed as `creation-sprites.bin`. The quiz composition uses the original braziers and virtue symbols with the dilemma text at the foot. Name/gender retain the original title artwork. No replacement art was invented.

## 12. Render-latency improvements

- Developer entry no longer performs a synchronous two-slot SD semantic scan.
- Slot inspection no longer decodes the selected native state a second time merely to obtain the display name.
- Presentation, frontend-view, transcript-line, and debug-screen scratch is reused rather than rebuilt as large automatic state.
- Character artwork is converted once by the packer and loaded in device-native RGB565/alpha form; render does not read or convert it from SD.
- `INPUT_SERVICE_GAP` gives physical evidence for the remaining 100-210 ms stalls, while the existing per-render high-water timing remains in `FRONTEND_RENDER`/`METRICS`.

The normal dirty-region renderer, title animation, attract visuals, and transition correctness are preserved.

## 13. Internal RAM / PSRAM impact

Measured type/owner deltas against the physically traced Alpha 2.0 build:

- internal main-task reservation: +12,288 bytes;
- persistent runtime owner: 19,704 -> 21,688 bytes (+1,984);
- persistent board owner: 4,028 -> 6,732 bytes (+2,704);
- total new internal reservation/static ownership: +16,976 bytes;
- new PSRAM ownership: 325,706 bytes total: 217,746 creation sprite blob + 97,280 creation canvas + 10,080 save workspace + 600 debug render scratch.

The save workspace is allocated on first persistence inspection/use and logs its exact size. The matching SD resource pack is 1,824,829 bytes with SHA-256 `530286b2bf2a82a20e6279f3b6261b546b800ad2a749cbf3b97b263f85051aa4`.

## 14. Exact Launcher path

`C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-correction\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

- size: 743,648 bytes
- SHA-256: `68b1aba27a772feb00b2339a6ba74d2b3f59331cc8dc957a6a76be3fc1c919e5`
- ESP-IDF: 6.1.0
- matching resource source: `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`
- resource SD destination: `/ultima5/openu5-alpha1-resources.bin`

## 15. Physical retest checklist

1. Copy the matching resource pack to `/ultima5/openu5-alpha1-resources.bin`, flash the Launcher image, and capture serial plus `/ultima5/logs/alpha20-frontend-debug.log`.
2. Cold boot through title, intro, View/attract, and menu; confirm every visible transition is a complete frame and all reported stack margins remain >=4,096 bytes.
3. Open Settings, change/restore one setting, return to the View, and revisit the menu; check render times and `INPUT_SERVICE_GAP` events.
4. Select Developer; require `DEVELOPER_ENTRY storage_touched=0 new_journey_touched=0`, no `CHAR_CREATE intent=developer`, no storage stages, no reset, and >=4 KiB stack margin.
5. Run New Journey through name, gender, every dilemma, original creation artwork, final derivation, every `NEWGAME_SAVE` stage, commit, and EnterGame; require no reset and >=4 KiB throughout.
6. Exercise the polished HUD, System Menu, in-game Settings, Continue, and explicit Load; verify the reserved touch strip and current presentation remain intact.
7. Type Enter, modifiers, and semantic movement during title/attract updates. Correlate any `ESP_ERR_INVALID_RESPONSE` with `INPUT_SERVICE_GAP` instead of assuming one causes the other.

## Build validation

- ESP32-S3 full firmware build: pass; binary 743,648 bytes, 29% of the smallest 1 MiB app partition free.
- Host build with warnings-as-errors: pass.
- CTest: 53/53 pass (frontend, Settings/session, Developer, presentation, input, gameplay, persistence, and parity coverage).
- `git diff --check`: pass (only pre-existing line-ending advisory output).
