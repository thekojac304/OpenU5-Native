# Alpha 2.0 Device/UI Correction Pass

Status: implementation, host validation, ESP-IDF build, and packaging complete. The items explicitly marked **physical retest required** cannot be proven without running this image on the T-Deck and are not claimed as hardware-pass results.

## 1. SD DMA/free-space failure root cause

The supplied physical trace establishes an SDMMC DMA-allocation failure, not proven media exhaustion: `allocate_dma_buf: not enough mem` occurs before the disk I/O errors and `statvfs`/free-space failure. The code audit found two contributors on the same path:

- large runtime/session workspaces were retained in internal RAM, fragmenting the heap before the first New Journey persistence transaction;
- the asynchronous diagnostic logger could flush to the same FAT/SD device while save, validation, or free-space I/O was running.

The previous error path then presented the failed free-space query as if the card had insufficient capacity. That classification was incorrect.

## 2. DMA/internal heap measurements

Every save, load, slot inspection, settings read/write, free-space query, temporary write, and validation read now emits:

`SD_HEAP operation=... state=... free_internal=... free_dma=... largest_dma=... largest_internal=... requested_dma=...`

The firmware reserves 8,192 bytes of internal DMA-capable memory during early startup, releases it only for a serialized SD transaction, and restores it when the transaction ends. Sector-oriented operations report a 512-byte requested-DMA size. Exact before/after free-block figures must be collected from the corrected image on physical hardware; they were not present for this build in the supplied trace.

Final link-map summary:

- DIRAM: 109,250 / 341,760 bytes used (31.97%); 232,510 bytes remain.
- Flash code: 493,834 bytes.
- Flash data: 171,044 bytes.
- Total reported image content: 757,028 bytes; packaged binary: 757,152 bytes.

## 3. Persistence fix

- Reserve DMA headroom before resource/runtime allocations and release it around SD transactions.
- Serialize persistence and diagnostic-log FAT access with one storage mutex; serial capture continues and queued SD logs drain afterward.
- Move safe long-lived objects and persistence workspaces to PSRAM.
- Preserve directory creation, free-space validation, temporary GAM/OOL/JSON writes, CRC/readback validation, generation commit, and recovery semantics.
- Distinguish `SD free-space query I/O failed; space not established` from a successful query that proves insufficient capacity.

The complete New Journey -> commit -> Enter Game -> reboot -> Journey Onward cycle is **physical retest required**.

## 4. Startup/title transition fix

The second fallback-looking title was accidental exposure: startup `IntroAnimation` was omitted from the title-owned render classification, so the renderer temporarily selected the text fallback. Startup intro is now explicitly title-owned. The first coherent frame is assembled while the backlight is dark, and the title/art path remains active throughout the startup transition. Extracted title artwork is not scaled.

## 5. Frontend/gameplay renderer ownership fix

Frontend states return from the render path before any gameplay viewport, transcript, celestial, or wind update can run. Gameplay rendering is reachable only after the frontend is inactive. Debug-build assertions guard both ownership boundaries. Leaving either owner invalidates retained-region caches so stale dirty updates cannot survive a transition.

## 6. Menu-row shifting fix

Every menu row now reserves the same one-character cursor gutter. Selected and unselected rows use the same text origin; only the gutter character/color changes. Retained redraw clears the full fixed row rectangle before drawing the new cursor state.

## 7. HUD framing changes

The existing 176x176 viewport and right-side HUD architecture remain. The pass keeps the established viewport, party, world/status, and transcript boundaries, applies a consistent cyan frame language, and keeps the lower-left area outside the viewport available for later touch work. Readability remains the priority over decoration.

## 8. Sun/moon/wind cleanup

The existing celestial and wind implementations were retained. Updates are now gameplay-owner-only, cached strings avoid unchanged redraws, animated-cell restoration repaints a strip only when the animation intersects its row, and the strips are integrated with the viewport frame. The wind strip remains slim; its gameplay semantics are unchanged.

## 9. Movement Mode indicator location

When active, `MOVE MODE: ON` is persistent in the right-side status area at y=78. It is removed when Movement Mode is off and never occupies the reserved lower-left region.

## 10. Rapid-input architecture

Physical capture is decoupled from 80-150 ms renders. A dedicated FreeRTOS task pinned to core 0 samples trackball GPIO and keyboard interrupt/matrix state at a 1 ms service cadence, then writes ordered raw events to a bounded queue. The main/UI task drains as many as 64 events before scheduling one redraw. Raw events retain press/release state, modifiers, matrix coordinates, base/symbol codes, snapshots, and capture timestamps. Existing Mic short/hold and Movement Mode interpretation remain in the semantic layer.

## 11. Input queue metrics

- Capacity: 64 events.
- Payload: 2,048 bytes (`64 * sizeof(RawInputEvent)`, 32 bytes on this target), plus FreeRTOS queue metadata.
- Capture task: 4,096-byte stack, priority 4, core 0.
- Diagnostics: `INPUT_QUEUE depth/high_water/dropped/queued/consumed`, per-event queued/consumed records, and `INPUT_SERVICE render_block_us/high_us/queue_depth`.
- Queue overflow is explicit and counted; events are not silently coalesced.

Physical queue high-water and dropped counts are **physical retest required**.

## 12. Trackball cooldown root cause

The fixed same-direction acceptance window was the remaining late-path throttle. The percentage now configures that actual edge/debounce window directly. The GPIO task captures falling edges independently of rendering; the semantic layer only rejects a same-direction re-close inside the configured window. Raw, accepted, and suppressed counts are reported independently.

## 13. Trackball timing mapping

| Responsiveness | Same-direction minimum interval | Intended behavior |
|---:|---:|---|
| 25% | 48,000 us | deliberate/slow |
| 100% | 12,000 us | prior comfortable baseline |
| 200% | 6,000 us | fast |
| 300% | 4,000 us | fastest supported |

`TRACKBALL_SETTINGS` reports percentage, minimum interval, debounce, and acceleration; `TRACKBALL_INPUT` reports raw/accepted/suppressed totals.

## 14. In-game full Developer menu path

With Developer visibility enabled: **System Menu -> Developer / Debug**. This opens the same `UiDebugMenu` instance used by the frontend/direct shortcut. Opening it consumes no gameplay command, turn, RNG call, or time step. The exact interrupted `UiMode` is restored on exit; transcript, prompt, selections, and session data remain owned by the existing `UiSession`.

## 15. Text/UI-size setting restoration

Small, Medium, and Large are visible again. The value applies live and persists through the existing settings store. Large uses 2x text for fullscreen menus/settings/system menu and the gameplay transcript, with adjusted row/column limits. World tiles, map geometry, and extracted title art are never scaled. Space-constrained title-art menus and compact shop/developer panels intentionally clamp to the normal glyph size instead of clipping.

## 16. Title/in-game Settings unification

Both Settings entry points edit the same `FrontendSettings` instance and use the same persistence service. Both expose Brightness, Movement default, Trackball percentage, Text/UI size, and Developer visibility. Audio and touch controls were not added.

## 17. Shop conversational flow restoration

Talk/direction still targets the authoritative NPC and shop orchestration. Greeting, service choice, pauses, deal confirmation, full-inventory, and result states stay on the main transcript; the compact selector is not shown during conversational phases. Transactional UI appears only when the state machine asks for an offer, party member, quantity/name, or deal decision.

## 18. Shop selector design

The selector is a compact right-side handheld panel layered beside the still-visible gameplay viewport. It shows the current transaction heading/prompt, real offer/service rows from the shop state, price/value where supplied, selected row, and concise Confirm/Mic Back guidance. It reuses the existing selected offer ID rather than maintaining a parallel phone-style shop model.

## 19. Shop routing bug root cause

The device adapter mapped every non-list Confirm to `ShopAction::Continue`. Greeting, menu, and deal states require `ShopAction::Confirm`, so physical Confirm advanced no authoritative state in those phases. Confirm is now state-aware: lists select the highlighted item/member, pause/full states continue, and conversational/deal states confirm. Mic Back maps deal states to Decline and other states to the orchestration's normal back/cancel behavior. No duplicate shop state machine was introduced.

## 20. Test results

- Warnings-as-errors host build: pass.
- Focused frontend, UI session, developer menu, shop adapters, shop flow, and input regression suites: pass.
- Full native/non-Node host suite: 30/30 pass, 0 failures.
- ESP-IDF 6.1 warnings-as-errors T-Deck build: pass.
- Application image: 0xB8DA0 bytes; 0x47260 bytes (28%) remain in the 1 MiB application partition.
- `git diff --check`: pass apart from existing line-ending notices.
- The Node/TypeScript parity runner could not start because the installed Node 24 runtime returned `uv_os_get_passwd: ENOMEM`; no product assertion failed, but that runner is not counted as passed.

Host tests cover state ownership/routing, settings, shop semantics, and input policy. LCD pixel timing, SDMMC allocation behavior, hardware tap capture, and full physical shop coverage remain device tests.

## 21. Internal RAM/PSRAM/stack impact

- Internal DMA reserve: +8,192 bytes while idle; deliberately freed during SD work and restored afterward.
- Input queue payload: +2,048 bytes internal, plus queue metadata.
- Input capture task stack: +4,096 bytes internal.
- UI session (464 bytes), developer menu (192 bytes), save scratch, and dungeon arenas use PSRAM where safe instead of fragmenting internal RAM.
- Main task remains 24 KiB.
- Static stack frames from the final target build: `AlphaRuntime::render` 2,128 bytes; `AlphaSaveService::save` 1,120; `Board::show_frontend` 1,520; `Board::show_alpha` 432; input capture task 64; input service 176.
- Final linked DIRAM headroom is 232,510 bytes. Runtime free/largest-block values are logged on-device because link-map headroom is not equivalent to live heap contiguity.

## 22. Exact launcher path

`C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-correction\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

- Size: 757,152 bytes.
- SHA-256: `03A61CFECE9C3C3B93E8C4E47C007E46376911540C741DE1A0AD5EE33F10299B`.

Required resource pack:

`C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`

- Size: 1,824,829 bytes.
- SHA-256: `530286B2BF2A82A20E6279F3B6261B546B800AD2A749CBF3B97B263F85051AA4`.

## 23. Short physical retest checklist

1. Boot from power-off: confirm one coherent art-led title sequence, stable menu X positions, and no celestial/wind pixels on frontend screens.
2. Clear/rename the save directory, complete New Journey, and retain serial plus `/ultima5/logs/alpha20-frontend-debug.log`; confirm every persistence stage reaches generation commit and Enter Game with no DMA allocation error.
3. Reboot and use Journey Onward; compare created name, gender, stats, inventory, position, time, and generation identity.
4. During save, inspect `SD_HEAP`: record free internal/DMA and largest blocks before reserve release, during the SD window, and after reserve restoration.
5. In Movement Mode, tap rapid alternating and repeated WASD while forcing redraws; confirm order, no missed normal-speed taps, no duplicate storm, and `INPUT_QUEUE dropped=0`.
6. Test trackball at 25/100/200/300%; confirm independently registered fast swipes and compare raw/accepted/suppressed counters.
7. Open **System Menu -> Developer / Debug**, exercise one read-only category, exit, and verify the exact prior modal/session resumes without a turn or time change.
8. Change UI size from title Settings and in-game Settings, reboot, and verify the same persisted value plus bounded Large layouts.
9. Test a weapon/bow shop, inn, provisions shop, and one service shop: greeting first, selector only for transactions, selection/Confirm acts on the highlighted offer, Mic backs one logical level, and exit returns to exploration.
10. Confirm the Movement Mode indicator occupies only the right status area and the lower-left future-touch region remains clear.

Out of scope and intentionally unchanged: audio, final touch buttons, Ultima IV transfer, and unrelated gameplay features.
