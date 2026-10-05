# Alpha 2.0 alpha2 hardware-truth frontend correction

Build identity: `OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

This pass is deliberately limited to the T-Deck Plus frontend, attract presentation,
character creation, keyboard-path regression coverage, and diagnostic evidence. It does
not add audio, touch controls, U4 transfer, gameplay work, or final reference-fidelity
polish.

## Findings and corrections

1. **Title/menu flicker root cause**

   The runtime requested a frontend render on every title-animation tick (275 ms), and
   `Board::show_frontend()` began every render with a 320x240 black fill before redrawing
   the title and all static menu text. Attract mode did the same every 55 ms. The LCD
   therefore received a real intermediate black frame and repeated static regions.

2. **Frontend renderer correction**

   The board now caches frontend state, view kind, title resource, menu rows, footer, and
   attract frame. The first frontend draw is the only full-screen clear. A title animation
   tick transfers only the changing 288x49 fire strip. A menu move redraws the old and new
   280x9 rows. A name edit redraws its single 280x9 row. State transitions clear and
   replace only the affected title/body regions. No update clears a region and presents it
   separately from the replacement content.

3. **Return-to-the-View clipping/layout root cause**

   The former attract branch drew a 176x144 preview at `(72,60)` but ignored the frontend
   information lines. It did not implement the reference title-plus-scene composition and
   left no deliberate 320x240 safe-area allocation for labels/status text. The replacement
   layout retains the original 320x110 title resource, draws a framed 100x98 scene at
   `(8,119)`, and reserves the right-hand 194x98 area for unwrapped, bounded scene/status
   lines. The footer remains within `y=228..238`.

4. **Attract animation root cause**

   The previous code regenerated the same live-game snapshot and changed only the terrain
   animation frame; the Avatar's position never changed. The replacement uses a private,
   deterministic eight-step route and four presentation frames, dirties the attract
   viewport on each demo step, and never advances or mutates gameplay state, RNG, saves,
   settings, or the world. This is a **working animated demo**, logged with
   `exact_reference=0`; it is not a claim of exact original scripted-View fidelity.

5. **Name-entry freeze root cause**

   Two concrete frontend defects were present. First, each accepted character caused the
   synchronous full-screen clear/redraw described above on the single input/render loop.
   Second, the frontend always called the shared adapter as a direction-accepting
   `InventorySelection` screen. With Movement Mode enabled, `W/A/S/D` could become
   directions rather than name characters, and Backspace became Back rather than delete.
   The frontend now selects `TextEntry` only during name entry, never accepts semantic
   directions, and renders only accepted state mutations. The eight-byte buffer already
   had a correct length guard and NUL termination; tests now cover eight characters,
   overflow rejection, and backspace editing.

   The debug build records raw edges, emitted actions, state/buffer before and after input,
   render begin/end, poll I/O time, heap/PSRAM, and stack margin. A physical retest remains
   the authority for excluding an additional device-only electrical or transport cause.

6. **Gender-screen freeze root cause**

   Gender entry inherited the same direction-accepting frontend adapter declaration and
   full-screen render path. The corrected non-direction frontend path leaves character
   input live, maps `M/F` to `0x0b/0x0c`, visibly advances to the questionnaire, and maps
   Mic to Cancel/Back. There is no save or settings write at this transition and no
   retained `TextEntry` declaration after the name phase.

7. **Keyboard transport regression result**

   No frontend-specific keyboard loop exists. Frontend and gameplay both use
   `InputHardware::poll()` followed by `UiInputAdapter`. The Alpha 1.4 transport behavior
   remains intact: interrupt-assisted polling with a 45 ms conservative fallback,
   malformed-snapshot rejection, preserved stable state across read faults, and raw-mode
   recovery only after eight failures with a two-second retry bound. Host regression tests
   confirm that frontend text entry bypasses Movement Mode while gameplay semantics remain
   unchanged. Transport faults are rate-limited under `KEYBOARD_ERROR`.

8. **Render transfer comparison**

   These are deterministic pixel-transfer counts from the old and corrected paths, not
   invented physical microsecond measurements:

   | Operation | alpha1 path | alpha2 corrected path |
   |---|---:|---:|
   | Title animation | about 137,720 px | 14,112 px |
   | Menu selection | about 137,720 px | 5,040 px |
   | Name character echo | about 95,760 px | 2,520 px |
   | Attract frame | about 116,064 px | 9,800 px |

   The alpha2 log records the real-device `render_us`, transferred pixels, dirty-region
   count, full-redraw flag, screen-clear flag, and cumulative state-reconstruction count
   for the requested six operations. Exact before/after wall-clock timings require flashing
   the old and new images to the same physical unit; host build timings are not substituted.

9. **RAM/PSRAM/stack impact**

   The change adds approximately 1.4 KiB of fixed board-side frontend cache plus small
   runtime counters. It adds no PSRAM allocation and no per-frame heap allocation. Every
   frontend render log includes free internal RAM, free PSRAM, and current task stack
   margin. The ESP-IDF image is 721,168 bytes and leaves 326,384 bytes (31%) in the 1 MiB
   application partition.

10. **Launcher**

    `native/targets/tdeck/build-alpha20-alpha2c/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`

    SHA-256: `da952f99731615a030775459a8ddd0de3cc3c7394c7683ee25e7f029b22d658b`

11. **SD diagnostic log**

    Card path: `/ultima5/logs/alpha20-frontend-debug.log`

    Mounted firmware path: `/sd/ultima5/logs/alpha20-frontend-debug.log`

    The logger writes asynchronously through its bounded queue, flushes periodically and
    after important state/character/keyboard transitions, and mirrors concise records to
    serial. Relevant prefixes are `FRONTEND_STATE`, `FRONTEND_INPUT`, `FRONTEND_RENDER`,
    `ATTRACT_TICK`, `ATTRACT_FRAME`, `CHAR_CREATE`, `CHAR_NAME`, `CHAR_GENDER`,
    `KEYBOARD_ERROR`, and `METRICS`.

12. **Short physical retest**

    1. Boot and leave the title/menu idle for 20 seconds: the menu must remain stable while
       the fire strip animates, with no black flash.
    2. Move the menu selection several times and open Return to the View: verify bounded
       labels, no clipped footer, and obvious Avatar movement through multiple demo steps.
    3. Start New Character with Movement Mode both off and on. Enter an eight-character
       name containing `W`, `A`, `S`, and `D`; backspace and re-enter one character.
    4. Select `M`, repeat with `F`, answer several questionnaire prompts, and verify Mic
       Cancel/Back where offered.
    5. Inspect serial and the SD log. Reject the build for a `KEYBOARD_ERROR` storm, stack
       margin collapse, per-character full redraw, or multi-hundred-millisecond name echo.
       Confirm save files are first created only after final New Journey completion.

## Verification completed before packaging

- Host tests: 53/53 passed.
- ESP32-S3 ESP-IDF 6.1 build with warnings-as-errors: passed.
- Launcher package structure and copied application image: validated.
- Physical render time, heap/PSRAM, stack, and keyboard-transport acceptance: intentionally
  left to the hardware checklist above; the build now supplies the evidence needed to make
  that decision without guessing.
