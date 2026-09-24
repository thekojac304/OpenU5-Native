# OpenU5-Native — Whole-Project Gameplay Integration Audit

**Target:** LilyGO T-Deck Plus · ESP32-S3 · ESP-IDF 6.1 · 16 MiB flash · 8 MiB PSRAM · 320x240 ST7789 · physical keyboard · trackball · GT911 · microSD
**Scope:** end-to-end playability, not code presence.
**Method:** input→adapter→UI→core→state→renderer→feedback→mode-return→persistence tracing, static reachability sweeps, reference (`game/src`, `re/`, `extractor/`) adjudication, host test-suite execution, and one purpose-built core probe.
**Commit audited:** `c18f5b64` (branch `main`, clean tree).
**Last comprehensive reconciliation:** **Batch 18**, from `83b2ed56` — see §14 Batch 18 for the authoritative current state, and the two documents it produced:
[`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md) (the one device list to run) and
[`ALPHA2_PRESERVATION_LEDGER.md`](ALPHA2_PRESERVATION_LEDGER.md) (every knowing divergence from the reference).

> ### CURRENT STATE (Batch 18) — read this before any status claim below
>
> **Alpha 2: HOST-CLEAN — HARDWARE VALIDATION REMAINS.**
>
> | | |
> |---|---|
> | Host suite | **86 total · 86 pass · 0 fail · 0 skipped** · 0 project warnings |
> | `gameplay_parity` | PASS, 5,058 sequences · `quest_parity` PASS, 5,377 cases |
> | Firmware | PASS · `0xd1b30` = 858,928 B · 189,648 B (18 %) free · 0 project warnings · **SD pack unchanged** |
> | Open production defects | **0** — R-25 closed in Batch 19; no new production defect was found |
> | Unresolved reference questions | **4** — Y-15, Y-28, Y-30, Y-32 |
> | Missing features / undecided | **5** — Y-01, Y-02, Y-06, Y-17, Y-19 |
> | Out of Alpha 2 scope | **2** — Y-03 (audio), the TypeScript-side R-16 residual |
> | Findings implemented but hardware-unverified | **33** (Batch 19 adds R-25) |
> | Findings verified **on hardware** | **1** — R-11 |
> | Catalogued divergences | **36** — **18 deliberate** (14 T-Deck adaptations + 4 enhancements) and **18 UNRESOLVED**. *Corrected in Batch 19: this row was headed "Intentional divergences", which described all 36 as intentional. Only the 18 deliberate ones are. An unresolved divergence is an open question about fidelity, not a decision — the two must not be merged. `ALPHA2_PRESERVATION_LEDGER.md` always kept them in separate sections (§2/§3 vs §4); only this summary label was wrong.* |
> | Hardware rows | **145 total — 4 PASS · 0 FAIL · 141 UNTESTED** (Batch 19 added H-141 – H-145 for the picker; see §14 Batch 19 and the checklist) |
>
> Per-identifier evidence for all of the above is the reconciliation table at §14 Batch 18 §3b.
>
> **Every "RED"/"OPEN"/"currently failing" statement below is historical unless its own heading says otherwise.** Sections 1–13 were written at the `c18f5b64` baseline and are preserved for their reasoning; §14's per-batch write-ups are the running correction, and §14 Batch 18 is the reconciled present.

Evidence classes used below:
- **[EXEC]** proven by running code during this audit
- **[REF]** proven against the authoritative TypeScript/DOS reference in this repo
- **[STATIC]** proven by unambiguous control-flow reading
- **[UNPROVEN]** requires physical device evidence

---

## 1. EXECUTIVE SUMMARY

### Overall integration health: ~~**NOT PLAYABLE END-TO-END**~~ → **HOST-CLEAN, HARDWARE-UNVALIDATED** (Batch 18)

> **This verdict is the `c18f5b64` baseline verdict and is superseded.** It was true when written: at that commit the glue layer dropped modal responses, destroyed shop/dialogue modes on every keypress, could not sail, could not Ready in combat, had no dungeon art and no Z-stats pages. Batches 1–18 closed all of it. The reconciled verdict is **HOST-CLEAN — HARDWARE VALIDATION REMAINS**: every system below is either host-verified, verified on hardware, or listed as one of the 136 outstanding device rows. The original text is kept because the *reasoning* under each heading is still the derivation for the fixes that followed.

The **core is in far better shape than the device integration**. At the original audit baseline, 56 of 57 host tests pass, including deep byte-level parity suites for combat, commands, magic, items, shops, dialogue, dungeons, travel and persistence. **Post-Batch-1, the host suite was 58 total, 57 pass, with only the pre-existing `gameplay_parity` mismatch 59 (R-02/R-03/R-04) failing.** **Post-Batch-2, R-02/R-03/R-04 are GREEN and mismatch 59 is fixed.** The host suite is still 58 total, 57 pass, but the sole failure is now a *different*, newly-exposed mismatch — **`gameplay_parity` mismatch 2034 (R-21)**, hidden behind mismatch 59 until then. **Batch 13 adjudicated and fixed it**: "2034" is a sequence index, not a byte offset, and it was a confirmed native defect — the (U)se scroll/potion readers echoed a fabricated `Used <name>.` line where `CAST.OVL 0x11f0`/`0x136e` print the bare category word, plus an invented `No effect!` fallback. `gameplay_parity` now passes. Mismatch 2034 was reproduced byte-for-byte on the untouched pre-Batch-2 baseline with the Batch 2 changes removed, confirming it is pre-existing and unrelated to R-02/R-03/R-04; it is tracked separately (§3 R-21) and was **not** fixed in Batch 2. **Batch 17 retired the other half of the "known baseline failures" convention.** From Batch 8 to Batch 16 every write-up below compared against two expected failures and set them aside. Batch 13 fixed the first (`gameplay_parity` mismatch 2034). Batch 17 adjudicated the second — `quest_parity`'s `STATUS_ACCESS_VIOLATION` / exit `3221225477` — and it was **not** the "MinGW/w64devkit environment finding" eight batches recorded it as: it was a `setjmp`/`longjmp` escape in the host harness invoking undefined behaviour against the Win64 SEH ABI, reproducing 100 % of the time and hiding **zero** parity divergence (the full 5377-case comparison was run to completion before the fix and matched the reference on every case). **The host suite is now 85 total, 85 pass, 0 fail — the first fully green run in this audit's recorded history.** Any failure a future batch sees is a real one, and must be treated as such. See §4 Y-34 and §14 Batch 17. **Batch 18 re-measured this from a clean build directory and confirms it (85/85/0 on the unmodified Batch 17 tree), then closed at 86/86/0 with its own new target — the first batch to both open and close fully green.**

The remaining failures are almost entirely in the **glue layer**: `native/targets/tdeck/main/alpha_runtime.cpp` (1375 lines), `native/core/src/ui_session.cpp`, `native/core/src/presentation.cpp`, and the **asset pack**.

`AlphaRuntime` itself still has no host coverage — nothing in the repository instantiates it directly, and the on-device "smoke tests" are data-presence probes and isolated `UiSession` probes with a spy dispatcher that never exercise the adapter that consumes the intents. **Batch 1 added `ui_mode_regression`, an ESP-free seam (`ui_mode_policy.h`) that host-tests mode arbitration and `UiSession` mode ownership (28/28 GREEN)** — narrowing, but not closing, that blind spot. Most defects below still live in the parts of the blind spot that seam does not cover.

**Batch 14 added a second host-driven `AlphaRuntime` suite.** `batch14_zstats_runtime` uses the same Batch 11 seam to drive the new `(Z)`-stats modal entirely through production routing — raw input, `UiInputAdapter`, `UiSession`, `dispatch()` — so the modal's input isolation and turn accounting are asserted against the real `handle()` rather than a mirror. See §14 Batch 14.

**Batch 11 changed the first sentence of this paragraph.** `AlphaRuntime` is now directly host-instantiated and host-driven — the real, unmodified `alpha_runtime.cpp` links into a new host executable (`alpha_runtime_integration_regression`) via harmless ESP-IDF/Board/storage substitutes, and 22 RED→GREEN assertions drive its actual `handle()`/`dispatch()`/`command()` across five representative command classes. This closes the *integration-testability* gap that made every earlier batch's "hand-mirrored" host coverage (see `dungeon_combat_test.cpp`'s and `dungeon_input_test.cpp`'s own file-header warnings that they *copy* `AlphaRuntime`'s per-input tail rather than exercise it) unable to catch a routing/turn-accounting bug in `AlphaRuntime` itself. It does **not** retroactively resolve any Y/R item below — see §14 Batch 11 for exactly what is and is not now covered.

### Largest risks, in order

1. ~~**R-01 — Shop and Dialogue UI modes are destroyed on every keypress.**~~ **RESOLVED in Batch 1.** `AlphaRuntime::synchronize_after_debug()`'s mode arbitration was split out and now preserves `Shop`/`Dialogue`/`ShrineSpecial` instead of unconditionally forcing `base_mode` to Combat/Dungeon/Exploration. See §3 R-01 and §14 Batch 1 for root cause, fix, and test evidence.
2. **R-05 — The dungeon** (ANCHOR 4). **Batch 9 resolved the presentation LOGIC; Batch 9B resolved the dungeon RUNTIME; Batch 9C packed and painted the authored ART.** Batch 9 proved and fixed eight semantic presentation defects behind a new portable seam, `openu5::plan_dungeon_view()`. Batch 9B then fixed what the first hardware session exposed: the dungeon key map was a **subset** of the reference dispatcher’s — `(I)gnite` above all, so a party could not make light from inside a dark dungeon — and the HUD location caption read the stale surface map id instead of the dungeon’s. Batch 9C then closed the asset half: `DNG1/2/3.16`, `ITEMS.16` and `MON0-7.16` are packed at their native 4bpp+mask (193,864 B of authored pixels in 195,768 B of container), resident in PSRAM, and blitted by `render_dungeon_view()` through a second portable seam, `openu5::dungeon_art_blits()`. The pack identity changed, so **the SD card must be rewritten together with the firmware** (§14 Batch 9C). R-05's remaining status turns on the final physical dungeon session; see §3 R-05 and §14 Batches 9 / 9B / 9C.
3. ~~**R-19 — Ships cannot sail.**~~ **RESOLVED in Batch 3.** `handle_exploration` now branches to `CommandKind::YellSails` when the party is aboard a frigate outside the Underworld, exactly as the reference's `yell()` dispatcher does, before the word prompt. See §3 R-19 and §14 Batch 3.
4. ~~**R-07/R-08 — The endgame (U)se chain is unreachable.**~~ **RESOLVED in Batch 3.** `usable_item_display_name()` now has exactly one interpretation — the real canonical id — and the shared picker seam gates a row per canonical id from its authoritative possession owner. Grapple is gone from the picker (Klimb-only). Pocket Watch (35) remains the one deliberate omission: no field anywhere backs it. See §3 R-07/R-08 and §14 Batch 3.
5. ~~**R-06 — `Ready` is rejected in combat and in dungeons.**~~ **RESOLVED in Batch 3.** Ready now routes in all three contexts, passes `battle = c.combat` to `equip_item()`, and refreshes the acting player's `CombatActor` equipment cache after a successful in-combat change. See §3 R-06 and §14 Batch 3.
6. ~~**R-09/R-10 — Five modal responses and both NPC-initiated events are silently discarded**, disabling Blackthorn, the Britannia guards, fountains, and every shopkeeper/guard that starts the conversation.~~ **RESOLVED in Batch 4.** Left unstruck for four batches; corrected in Batch 18. See §3 R-09/R-10 and §5, whose `BeginConversation`/`BlackthornAction` rows carried the same stale reading.

**The remaining risk list, reconciled at Batch 19, is shorter:**

1. **141 of 145 hardware rows have never been run** ([`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md)). This is now the **only** Alpha 2 blocker.
2. ~~**R-25 — the crystal ball is 100 % non-functional** and its prompt is fabricated.~~ **RESOLVED in Batch 19.** Kernel `0x4988` is now the shared seam `openu5/command_char.h`, and the crystal ball, `(S)earch` and `(C)ast` all resolve through it. Host-verified; device rows H-50 and H-141 – H-145 remain. See §14 Batch 19.
3. **Y-32 — the Blackthorn scene's pacing has no reference witness.** Not fixable until someone records original footage; see the ledger D-10. This is a missing *oracle*, not a known defect.

### Anchor verdicts

| Anchor | Verdict | Root cause status |
|---|---|---|
| **1 — stale blue/relic symbol after loot removed** | **GREEN — RESOLVED (Batch 2), hardware validation not yet performed** | **Root-caused and fixed.** Two mechanisms, both repaired: (a) native promoted unopened arena chests to world objects the reference never creates — both promotion sites (`combat.cpp::finish_encounter_combat`, `outdoor.cpp::outdoor_start` `victory_latch`) removed (R-03); (b) the invalid promoted objects carried raw `o.tile` = **1** = deep-water terrain (blue) instead of sprite `0x101` — this defect disappears as a consequence of (a), since legitimate stationary chest QuestObjects already store the pre-offset sprite tile correctly (R-02). `gameplay_parity` mismatch 59, the reproducible failing test for this anchor, is now GREEN. Physical device confirmation is the one remaining step — see §16 Phase 3 steps 19–22. |
| **2 — loose-loot icons don't match identity** | **GREEN — RESOLVED (Batch 2), hardware validation not yet performed** | **Root-caused and fixed.** The combat and world loot tile equations were already correct (`0x100+id`, verified [EXEC]). The defect was **layer ordering**: `compose_world_presentation` painted *every* quest object in one unified last-write-wins pass, so a chest/prop/ship object could overdraw a loot icon sharing its cell. Repaired (R-04) by splitting composition into the reference's two layers — stationary/non-loot objects resolved first-match-per-cell, then loot/search resolved last-match-per-cell (LIFO) and painted unconditionally on top, so a stationary object can never mask loot again. Covered by `presentation_regression`'s new R04-OVERLAY (deliberately defeats insertion-order luck) and R04-LIFO cases. Physical device confirmation is the one remaining step — see §16 Phase 3 step 21. |
| **3 — View Gem appears to do nothing** | **RED** | **Partially root-caused.** Command, gem decrement, deferred turn and a renderer dispatch all exist and are wired. The **presentation itself is fabricated**: `render_world_gem_view` colours cells by `tile&3`/`tile&7`/`tile&15` bit tests, which is not a terrain classification — the output is noise, and the top/bottom 9 px are overdrawn by the sky/wind bars. Needs one device capture to separate "renders noise" from "renders nothing". |
| **4 — dungeon 3D broken/unusable** | **YELLOW — logic resolved (Batch 9), art still missing** | **Root-caused, and split.** Two independent causes, not one: (a) the *presentation logic* was wrong in eight provable ways — no light gate, the movement blocker reused as the sight rule, no door/room/alcove/passage classification, features and the wanderer drawn only at arm's length, no wall variant, and two 9 px strips of blank sky and `Wind: --` covering 18 of 176 viewport rows. All eight are fixed and covered by `dungeon_view_regression` (Batch 9). (b) the *authored art* is still not in the asset pack, so the device draws the reference's own packless placeholders. Controls were already correct and remain untouched — see §12. |

### What is genuinely solid

Movement, world commands, combat mechanics, the turn/clock model, magic *mechanics*, shops/dialogue/quests *as core libraries*, the frontend state machine, save/load of `GameState`/`TurnState`, the input controller, Mic short/long semantics, and Movement Mode. The reference-fidelity work is real and deep. The gap is that a large fraction of it is not wired to a player.

---

## 2. MASTER COVERAGE MATRIX

Status keys: **G**=GREEN, **Y**=YELLOW, **R**=RED, **N**=N/A-intentional.
"Auto" = automated coverage that exercises the *player route*, not just the core function.
Full per-column detail (state, presentation, save, reason, files, next action) for every non-GREEN row is in §3–§4; the matrix carries the verdict and the pointer.

### A. Frontend / game start

| System | Entry | Device route | Core path | Presentation | Auto | Device | Status | Reason / ID |
|---|---|---|---|---|---|---|---|---|
| Title / attract | any key, 20 s idle | `AlphaRuntime::handle` → `frontend_.handle` | `FrontendSession::handle` | `FrontendView` + `render_intro_view` | `frontend_tests` | ✗ | **Y** | State machine proven; art path unproven on device. Y-07 |
| Main menu (8 items) | hotkeys `JCTUARSD`, trackball, Enter | same | `activate_menu` | `FrontendViewKind::Menu` | `frontend_tests` | ✗ | **Y** | All 8 routes exist [STATIC]. Movement Mode WASD does **not** navigate here — Y-01 |
| Journey Onward | menu 0 | `service_frontend_intent` → `save_.load` | `load_native_state` + `synchronize_loaded_world` | gameplay rebind | `frontend`, `persistence` | ✗ | **G** | Load restores/clears `objects_` and `dungeon_`. R-14/R-15 resolved (Batch 6) |
| Create New Character | menu 1 | creation phases → `CreateInitialSave` | `apply_new_journey_identity`, `preserve_new_journey_template_bytes` | creation canvas | `frontend_tests` | ✗ | **Y** | Reset path at `alpha_runtime.cpp:1308` *does* clear `objects_`/`dungeon_`/`combat_` correctly. Unproven on device. Y-07 |
| Transfer from Ultima IV | menu 2 | notice only | — | — | — | — | **N** | Explicit "Ultima IV transfer is deferred" (`frontend.cpp:88`). Intentional. |
| Introduction | menu 3 | `IntroAnimation`, 21 pages | `IntroViewPlayer` | `render_intro_view` | `frontend_tests` | ✗ | **Y** | Y-07 |
| Acknowledgements | menu 4 | `Credits` | — | `FrontendViewKind::Credits` | `frontend_tests` | ✗ | **Y** | Single hardcoded credit line vs. reference scroll. Y-17 |
| Return to the View | menu 5 | `AttractDemo` | `IntroViewPlayer` | `render_intro_view` | `frontend_tests` | ✗ | **Y** | Y-07 |
| Settings | menu 6 | `Settings` + `PersistSettings` | `AlphaSettingsService` | settings view | `frontend_tests` | ✗ | **Y** | Y-07 |
| Developer menu (from frontend) | menu 7 | `OpenDeveloperTools` → `enter_game()` + `open_debug_menu()` | `UiDebugMenu` | `DeviceDebugScreen` | `ui_debug_menu` | partial | **Y** | Enters gameplay with whatever `game_` was loaded at boot from INIT.GAM. Y-18 |
| Frontend → gameplay handoff | any of the above | `frontend_.active()` gate in `handle`/`render` | — | `assert(!frontend_.active() && !system_menu_.active())` | — | ✗ | **Y** | Ownership assert exists — good design. Unproven. Y-07 |

### B. Input system

| System | Route | Status | Reason / ID |
|---|---|---|---|
| Keyboard matrix + recovery | `tdeck_input.cpp` → `RawInputEvent` | **G** | `input_regression` (421 lines) covers matrix, resync, modifiers. Preserved per instruction. |
| Mic/0 `(0,6)` short = Cancel | `UiInputAdapter::translate` | **G** | [EXEC] `input_test`, smoke 34/35. Never emits printable `0`. |
| Mic/0 long ~1.1 s = Movement Mode | `UiInputAdapter::update` + `translate` | **G** | Threshold fires from the pump, not only on release. Preserved. |
| Movement Mode WASD (gameplay/direction prompts) | `is_movement_context` | **G** | Covers Exploration, Dungeon, Combat, TargetSelection, plus any `accepts_direction_input()` mode. |
| Literal WASD in text/numeric entry | `movement_mode_active` returns false for modals | **G** | smoke 7. |
| Trackball 4-way + debounce | `InputController::normalize` | **G** | Debounce is settings-driven (25–300 %). |
| Trackball centre click (GPIO 0) | — | **Y** | Not bound anywhere. Y-02 |
| `Alt+M` System Menu | `UiActionKind::SystemMenu` | **Y** | Handled before gameplay routing in every mode. Unproven. Y-07 |
| `Alt+D/S/L` developer/save/load | `DeviceShortcut` | **Y** | Y-07 |
| GT911 touch | — | **N** | `FrontendSettings::touch_controls` reserved; lower-left HUD region intentionally blank (`hud.h:kHudTouch*`). Intentional. |
| Movement Mode in frontend menus | `mode_before = InventorySelection` | **Y** | `is_movement_context(InventorySelection,false)` = false → WASD inert in menus. Y-01 |

### C. World / exploration commands

All routes are `UiSession::handle_exploration` → `UiIntent` → `AlphaRuntime::dispatch` → `dispatch_world_command`.

| Key | Verb | Status | Reason / ID |
|---|---|---|---|
| dir | Move | **Y** | `world_flow_parity`, `movement_flow_parity` green; device presentation unproven. Y-07 |
| `a` | Attack | **Y** | Direction prompt → `CommandKind::Attack`. Y-07 |
| `b` | Board | **Y** | `transport_flow_parity` green. Y-07 |
| `c` | Cast | **G** — RESOLVED (Batch 5), hardware-certified | R-11. World An Sanct / In Por / An Ex Por now open the ordinary world getdir; hardware checkpoint GREEN — all three passed on the physical T-Deck. |
| `e` | Enter | **Y** | Dungeon entry logs exist; town/dungeon entry unproven. Y-07 |
| `f` | Fire | **Y** | Reticle + Confirm path in `handle_modal`. `CellProjectile` **now has a renderer** -- Y-04 GREEN (Batch 7B): one flight per shot, origin (0,0) for a broadside and the cannon's own cell on foot, interpolated cell by cell at 55 ms/cell and drawn as a sub-cell dot, never a tile write. The remaining **Y** is the unproven *device* reticle presentation, Y-07, not the projectile. |
| `g` | Get | **G** | Directional, LIFO, authoritative increment — preserved as working. World loot icon = `o.tile+256` matches reference [REF]. |
| `h` | Hole up (Rest) | **Y** | `RestHours` → numeric → `Rest`/`RestCancel` via `finish_modal`. Y-07 |
| `i` | Ignite | **Y** | Dungeon variant gated separately at `commands.cpp:752`. Y-07 |
| `j` | Jimmy | **Y** | Y-07 |
| `k` | Klimb | **Y** | `KlimbCancel` on modal cancel is wired correctly. Y-07 |
| `l` | Look | **Y** | `look_sign` test green. Y-07 |
| `m` | Mix | **Y** | Opens spell list with `UiRequestId::Custom`; qty hardcoded to 1, reagent mask from definition. Y-19 |
| `n` | New Order | **Y** | Two-stage party selection → `NewOrder`. Y-07. (`SetActivePlayer` is now reachable on the digit keys — Y-20 **GREEN**, Batch 3.) |
| `o` | Open | **Y** | Troll chest Open preserved as working. Y-07 |
| `p` | Push | **Y** | Y-07 |
| `r` | Ready | **G** (world, dungeon, combat) | R-06 **GREEN** (Batch 3) |
| `s` | Search | **Y** | Trap reporting preserved as working. Y-07 |
| `t` | Talk | **Y** | Player-initiated works; NPC-initiated does not. R-10 |
| `u` | Use item | **G** (picker) | R-07, R-08 **GREEN** (Batch 3) — canonical real ids, complete owned set, no Grapple. Pocket Watch 35 still excluded (no backing state). |
| `v` | View gem | **G** — host+firmware RESOLVED (Batch 10); hardware visual check pending | ANCHOR 3. R-17/Y-14 |
| `x` | X-it (Disembark) | **Y** | Y-07 |
| `y` | Yell | **G** | R-19 **GREEN** (Batch 3) — frigate branch dispatches `YellSails`; word-of-power Yell unchanged (Y-24 both branches covered). |
| `z` | Z-stats | **G** | Member picker (`select_player` 0x0000) then the full 17-slot page axis: per-member Stats and Arms, Provisions, and the Reagents/Spells/Items/Armaments lists, with the binary's circular ring, its `0`–`6` jumps, its 7-row list paging and its Space/ESC-only exit. **R-22 RESOLVED (Batch 14)** |
| space/Enter | Pass | **G** | Two independent routes, both echo. |
| `0`–`9` | Set Active Player / harpsichord note | **G** — full reachability, host+firmware (Batch 16) | Y-20, R-20 **GREEN** (Batch 3) — digits dispatch `SetActivePlayer` with the literal digit; at the harpsichord they are intercepted first and dispatch `HarpsichordNote`. **Y-29 GREEN — RESOLVED (Batch 16):** on T-Deck hardware, digits `1`-`9` (and Cancel, via short-press) were always physically reachable, but literal `'0'` — the clear-active-player route — was not: the only matrix position resolving to `'0'` is the physical Mic key, which `UiInputAdapter::translate()` unconditionally intercepted for short=Cancel/long=Movement-Mode before any modifier check. Fixed by teaching that one branch to check `raw.modifiers.symbol` first: a Symbol-held Mic/0 press now bypasses the short/long special case and emits the literal character `'0'`, which flows through the same digit path every other key already uses. A plain (unmodified) press is byte-for-byte unchanged. See §14 Batch 16. |
| any other | `"X-What?"` | **G** | Matches reference unknown-key echo. |

### D. View command family

| Sub-command | Status | Reason / ID |
|---|---|---|
| `V` gem — world | **G** — host+firmware RESOLVED (Batch 10) | R-17/Y-14. Core (decrement, defer turn, `AfterGemView`) matches `game.ts::view()` byte-for-byte [REF]. Presentation now `build_world_gem_view()` (native/core, host-tested): reference `GEM_CATEGORY` classification, chunk-origin-anchored/full-town window, full 32×32 square. Hardware visual check pending. |
| `V` gem — dungeon floor | **G** — host+firmware RESOLVED (Batch 10) | `build_dungeon_gem_view()` (native/core, host-tested) is the same 8-neighbour flood over the 22×22 display DNGLOOK performs (reference `buildGemView`'s dungeon branch), now blocker-correct (a *revealed* secret door still blocks the gem's flood, unlike movement passability). R-17. Hardware visual check pending. |
| `V` gem — no gems | **G** | `"You have none!\n"` + immediate turn. Matches DS 0xa266 and the bug-for-bug turn charge [REF]. |
| Crystal-ball "Strange vision!" gem view | **G** — RESOLVED (Batch 19) | **Corrected in Batch 19; this row read R in Batch 18 and the R was right then.** Both halves now work. Presentation was already done (`look.cpp` sets `gem_from_crystal`, the device suppresses the deferred turn, sharing R-17/Y-14's renderer). The route is now live: the fabricated `"Peer into it?"` yes/no is deleted and the kernel `0x4988` picker resolves the member — directly when one is active or exactly one is eligible, `"None!"` when none is, the `"Player: "` roster prompt when two or more are, with the `"Disabled!"` re-ask and `"None!"` on cancel. R-25 — closed; device rows H-50 and H-141 – H-145. |
| `MapReveal` (Wis An Ylem, In Quas Wis scroll, Death Vision) | **G** — RESOLVED (Batch 7) | `compose_world_presentation`'s `reveal_all` bypasses the light/wall censorship for `note*PAUSE_UNIT_MS` (1100ms) wall-clock; the device swallows input for the same window, matching the reference's modal `revealViewport`/`cancelMapReveal`. R-12 |
| `Zodiac` (Use Spyglass) | **G** — RESOLVED (Batch 7) | `render_zodiac_view` draws the already-computed `ZodiacView` (stars/signs/Shadowlord lines); closes on any key like View Gem, charging no turn. R-13 |

### E. Inventory / items

See the full Resource/Item matrix in §11. Summary: **gold/food/gems/keys/torches/reagents/equipment G**, **potions/scrolls Y**, **quest items and tools G — RESOLVED (Batch 3)** (R-07, R-08; see §3).

### F. Loose loot / chests / remains

| Stage | Status | Reason / ID |
|---|---|---|
| Chest creation in arena | **G** | `combat.cpp:467`, `:1178` — `combat_parity` green. |
| Trapped/untrapped, Search, trap resolution | **G** | Preserved as working; `chest_trap` parity green. |
| Loose-object spawn, LIFO stack, one-item-per-Get | **G** | `combat.cpp:1346` top-of-stack scan; preserved as intended. |
| Full-inventory retention (`apply_loot_grant` fails → pile kept) | **G** | Deliberate and correct: "award first, erase second". |
| Final removal → `pile_count` 0 | **G** | Confirmed. |
| Cell recomposition after removal | **G** — RESOLVED (Batch 2) | ANCHOR 1 (b). R-02 fixed — no chest-tile-1 producer remains. |
| Combat→world chest promotion | **G** — RESOLVED (Batch 2) | ANCHOR 1 (a) — `gameplay_parity` mismatch 59 fixed. R-03: both promotion sites removed. |
| Loot icon identity | **G** — RESOLVED (Batch 2) | ANCHOR 2 — two-layer composition. R-04 fixed. |
| Corpse/blood decor persistence (`loot[]` 30/31) | **G** | Reference-correct: `TILE_CORPSE`/`TILE_BLOOD` persist in `lootLayer` [REF]. |
| Save/load of world loot | **G** | `objects_` serialized/restored via the sidecar. R-14 resolved (Batch 6) |

### G. Ready / equipment

| Aspect | Status | Reason / ID |
|---|---|---|
| Member selection (party >1, combat actor, solo) | **G** | Three-way branch at `alpha_runtime.cpp` `OpenEquipmentSelection` is correct. |
| Item list = `ready_items()` with qty + equipped flag | **G** | `item_parity` green; `UNRESOLVED_NAME` logging is a good defect trap. |
| Equip / replace / toggle-unequip | **G** | `equip_item` toggles (`inventory.cpp:95`), so Unready is covered without `CommandKind::Unready`. |
| Slot rules, two-handed, rings/amulets, ammo | **G** | `item_parity`, `advanced_combat_parity`. |
| List refresh after equip | **G** | `modal()` re-opens the selection from just-mutated fields — correct pattern. |
| **Ready in combat** | **G** — RESOLVED (Batch 3) | R-06. Routes through `CommandKind::Ready`, `equip_item(..., battle=true)` (armour lock preserved), then `resync_player_equipment()` refreshes the actor's cached weapons/attack/range/defense. The combat action is charged **once per `R` interaction, at picker close** (`UiSession::cancel_modal()` -> `CommandKind::CombatYield`), matching `closeAndEndTurn`/`playerReady()`. Presentation gaps: **Y-28**. |
| **Ready in dungeon** | **G** — RESOLVED (Batch 3) | R-06. Ordinary free action: `battle=false`, no armour lock, no turn charged. |
| Save/load of equipment | **G** | `save_core.cpp` round-trips all six slots. |

### H. Magic (48 spells)

| Aspect | Status | Reason / ID |
|---|---|---|
| Spell table, circles, reagents, time gates | **G** | Generated from `MagicDefinitions.json`; `magic_parity` green. |
| Mix | **Y** | Quantity hardcoded to 1. Y-19 |
| Combat casting (self / party / aim / direction) | **G** | `cast_selected_spell` covers `selectedCombatPlayer`, `castingCombatPlayer`, `MapPosition`, `MapUnit`, `direction` — **but only when `context_.combat`**. |
| Dungeon casting (Uus Por / Des Por / fields / dispel) | **G** | `dungeon_orchestration.cpp:160+` routes to `MagicUp`/`MagicDown`/`Tick`. |
| **Combat field spells (In Flam / Nox / Zu / Sanct Grav)** | **G** — ADJUDICATED (Batch 12), **no production change** | Hardware report "casts, flashes, leaves no field" is the **reference's own behavior**, cloned on purpose (`docs/bugs-del-original.md` §2.9, ticket #91). `CAST.OVL 0x004c` branches on `g_location >= 0x80`: the **combat** arm seeds no field at all, it sets a spell weapon from `DS:0x4592` and calls the same `COMSUBS:0x0c52` dispatcher Grav Por / Vas Flam / Xen Corp use; the **dungeon** arm is the only reader of the field-tile table `DS:0x4596`. See the Batch 12 write-up in §14 for the six-axis evidence (dispatch, arena state, render, gameplay effect, lifetime, hardware). |
| **Dungeon field spells — 3D VISIBILITY (In Flam / Nox / Zu / Sanct Grav)** | **G** — RESOLVED (Batch 12B), production fix | The dungeon arm always wrote the cell correctly (`dungeon_orchestration.cpp`, one cell, `0x82/0x81/0x80/0x83`, bit 3 kept) and the write always survived turning, ticking and stepping — but **nothing was drawn**. `dungeon_art_blits()` returns 0 for cell kind 8 because ITEMS.16 has no field image; the original draws it with the procedural sparkle subsystem `magic_field_sparkle_drawer` @0x127e, which the port had never carried. Added as `openu5::dungeon_field_spark()` (tables DS 0x2e42/0x2e4a/0x2e52/0x2e5a, colours 0x1292 + `add ax,8`) and painted by `render_dungeon_view`. Also restores the **55 authored** fields DUNGEON.DAT ships in Wrong (30) and Covetous (25), invisible since Batch 9C. |
| **Dungeon ROOM arena identity** | **G** — RESOLVED (Batch 12B), production fix | The alpha pack stored combatmaps.json's **per-territory** index (britannia 0..15, then dungeon 0..111) while `dungeon_encounter()` addresses an arena by the **global** index `dungeon_room_map()` produces (16..127). Every dungeon room resolved to the **next dungeon's** room, and Doom's 112..127 matched nothing at all (`MissingMap`). Retagged at load in `alpha_resources.cpp`; proven against the shipped pack by `batch12b_hardware_regression`. |
| **World casting with a target** | **G** — RESOLVED (Batch 5), hardware-certified | R-11 — **three** spells (An Sanct / In Por / An Ex Por), not six, were the defect; they now prompt for a direction and apply. An Ylem / An Grav have no world effect in the reference either. In Ex Por was believed to be a fourth member of that group too, but Batch 8 (R-16) found that belief itself is stale — the reference's own In Ex Por world handler calls the Skull Key's door-unlock worker (re/notes/magic.md, corrected 2026-08-07) and is simply un-fixed on both ports; wiring its getdir/tile-mutation is a tracked follow-up, not part of this row's Batch 5 fix. Hardware checkpoint GREEN — An Sanct (locked door), In Por (world blink) and An Ex Por (door) all passed on the physical T-Deck on the first attempt. World spell targeting presents a directional prompt, not a combat-style target reticle — confirmed correct per the reference `doCast` getdir behavior, not a defect. |
| Ceremony (invert + timing) | **G** | `start_magic_ceremony` calibrated to CAST2:0000; no-ceremony list matches. |
| `MagicEffect::Reveal`/`DeathVision` visual | **G** — RESOLVED (Batch 7) | R-12 |
| **Spell descriptions vs. actual effects** | **G** — RESOLVED (Batch 8) | R-16 — adjudicated, not rubber-stamped: 9/10 flagged summaries and 1/12 flagged target labels were confirmed defects and fixed; the rest were a stale audit premise (target_type is the unreliable field, not the label) or, for In Ex Por, a stale *reference* premise later corrected by RE. See Resolution below. |
| Save/load of spell quantities | **G** | `spellQuantities` round-trips. |

### I. Potions / scrolls / magic items

| Aspect | Status | Reason / ID |
|---|---|---|
| Potion ids 8–15, scroll ids 0–7 | **G** | Index space matches `world_magic.cpp` handlers exactly; `item_parity` green. |
| (U)se consumable ECHO and ceremony | **G** — RESOLVED (Batch 13), production fix | Was R-21 / `gameplay_parity` mismatch 2034. Both readers echoed a fabricated `Used <name>.` line where `CAST.OVL 0x11f0`/`0x136e` print the bare category word (DS `0x466a` / `0x4706`), and the drinker printed an invented `No effect!` where the reference prints nothing for a result with no authored DS line. Fixed at all mouths; the potion ceremony also moved back ahead of the reroll (`0x139b` before `0x13a1`) and dungeon casts now raise the ceremony they had always skipped. `batch13` + `gameplay_parity` (now passing). |
| Potion target prompt (`UseTarget`) | **G** | `modal()` opens party selection for items 6 and 8–15 before dispatch. |
| Consumption on cancel | **G** | Deliberately reference-correct: "canceling target selection still loses it" (`inventory.h`). |
| Scroll effects | **G** — DISCHARGED (Batch 5); cancel-path refund **RESOLVED (Batch 16)** | Y-21's premise was **stale**: `AlphaRuntime::modal()`'s Inventory branch already opens `begin_target(UseTarget,"Direction?")` for scroll id 1, so the device *does* supply a direction and Rel Hur changes the wind. Proven host-side by `batch5` B4 + C6. **Y-31 GREEN — RESOLVED (Batch 16):** cancelling that getdir used to dispatch nothing, refunding the scroll (and the skull key, which shares the route) where the reference has already consumed it at item-selection time, before the getdir ever opens. `UiSession`'s `TargetSelection` cancel arm now dispatches the pending `UseItem` with `has_direction=false` on cancel, the same pattern the `Cast` arm already used for R-11/Y-21's world-getdir ordering. See §14 Batch 16. |
| Magic items (carpet/skull key/spyglass/sextant/watch/box) | **G** picker | R-07/R-08 resolved (Batch 3): all state-backed tools listed by real canonical id. Watch (35) still absent — no backing state. Spyglass zodiac view still **R-13**. |

### J. Combat

| Aspect | Status | Reason / ID |
|---|---|---|
| Entry: direct, random, dungeon, scripted | **G** | `combat_parity`, `advanced_combat_parity`, `direct_troll_handoff` green. |
| Initiative/scheduling, enemy AI beat (400 ms) | **G** | `service_combat` + `schedule_combat`; queues player input during AI turns. |
| Move / attack / aim reticle / ranged / spells / items | **G** | `handle_combat` + `handle_modal` TargetSelection. |
| Attack cancel leaves Aim without consuming a turn | **G** | Deliberate handheld contract, documented in `ui_session.cpp:486`. |
| Flee (Back/Cancel → `CombatEscapeQuick`) | **G** | `combat_escape_regression` green. |
| **Victory arena stays open for looting** | **G** | `victory=true` without `ended=true` (`combat.cpp:255-278`); `end()` only via the canonical exit. Preserved. |
| Canonical victory exit → teardown | **G** | `finish_combat_if_needed` runs `finish_encounter_combat`, then rebinds base mode to Dungeon/Exploration. |
| Return to world | **Y** | `log_combat_world_restore` + tile save/restore exist; unproven on device. Y-07 |
| Return to dungeon cell/facing | **Y** | `dungeon_combat_return` handles floor delta, escape border, facing. Unproven. Y-22 |
| **Post-combat chest promotion** | **G** — RESOLVED (Batch 2) | R-03 — both promotion sites removed; `gameplay_parity` mismatch 59 fixed. Unclaimed arena treasure is now correctly lost on exit, matching the reference. |
| Ready in combat | **G** — RESOLVED (Batch 3) | R-06 |
| Combat save/load | **N** | Not serialized; the reference does not save mid-combat. Intentional. |

### K. Dungeons

Full report in §12. Summary: **data G, controls G, geometry Y, art R, HUD Y, save R**.

### L. Dungeon control semantics

| Control | Mapping | Status |
|---|---|---|
| Movement Mode `W` | `Direction::North` → `DungeonAction::Forward` | **G** [STATIC] |
| Movement Mode `S` | `South` → `Back` | **G** |
| Movement Mode `A` | `West` → `Left` (turn) | **G** |
| Movement Mode `D` | `East` → `Right` (turn) | **G** |
| Trackball up/down/left/right | same four | **G** |
| No strafing invented | confirmed — `DungeonAction` has no strafe | **G** |
| Movement Mode **off** | `a`=Attack, `s`=Search, `w`/`d`="What?" — no keyboard movement | **Y** | Y-23 |
| `o`/`g`/`j`/`s`/`k`/`space` | Open/Get/Jimmy/Search/Klimb/Pass | **G** |
| `c`, `u`, `z`, `v` | Cast, Use, Stats, View gem | **G** route / R-11 RESOLVED (Batch 5), see R-17 |
| `r` Ready | offered and applied | **G** — R-06 RESOLVED (Batch 3) |
| `m` Mix | silently aliased to Cast | **Y** — Y-15 |
| Mic short = Back, Mic long = Movement Mode | unchanged in dungeon | **G** |

### M. Shops / services

| Aspect | Status | Reason / ID |
|---|---|---|
| Shop data, prices, quantities, all keeper types | **G** | `shop_parity`, `shop_adapters`, `shop_flow` green; `shop_orchestration.cpp` is 1150 lines of adjudicated logic. |
| Offer list cursor ↔ authoritative item id resolution | **G** | `shop_offering_at` bridge in `dispatch` is exactly right. |
| Hierarchical Back (deal → list → menu → exit) | **G** | `handle_shop` deal/list classification. |
| Rations quantity / rumour text sub-modals | **G** | `finish_modal` converts them to `ShopAction::Text`. |
| **Shop UI mode survival across keypresses** | **G** | R-01 resolved (Batch 1) — `UiMode::Shop` now preserved via `ui_mode_policy.h` arbitration. |
| Shop panel composition | **G** | `compose_shop_view` requires `mode()==Shop`; no longer clobbered after R-01 fix. |
| Shop exit → correct gameplay mode | **G** | Shop now owns `shop_return_mode_`, captured before session-mode overwrite. R-18 resolved (Batch 1). |
| Save/load after a purchase | **G** | Gold/equipment/reagents all round-trip. |

### N. NPCs / dialogue

| Aspect | Status | Reason / ID |
|---|---|---|
| Talk scripts, keywords, name/job/bye, rune output | **G** | `dialogue_parity`, `dialogue_adapter` green. |
| Player-initiated Talk + direction | **Y** | Route exists; R-01 mode ownership is resolved, but the route remains physically unproven on device. Y-07 |
| Dialogue → quest state mutation | **G** | `dialogue_effects.cpp`; `quest_parity` green. |
| Schedules, visibility, movement, day/night | **G** | `npc_path`, `actors`, `enter_npc_map`; `turn_parity` green. |
| **NPC-initiated Talk** | **R** | R-10 |
| **NPC-initiated Shop** | **R** | R-10 |
| Guard password / tribute / arrest | **R** | R-09 |
| Blackthorn interrogation | **R** | R-09. Its roster mutation is a separate axis and is now **G**: **R-23 resolved (Batch 15)** — the sacrifice compacts all sixteen records, parks the executed companion in slot 15 with `partyStatus 0x7f`, and (faithfully) re-indexes nothing. The row's own R-09-era verdict is unchanged. |
| **Dialogue UI mode survival** | **G** | R-01 resolved (Batch 1). Note: the active prompt loop itself was always protected by `TextEntry` modal state; R-01's damage was to `base_mode` underneath it. |
| Dialogue exit → correct mode | **G** | Dialogue now owns `dialogue_return_mode_`. R-18 resolved (Batch 1). |
| NPC walk persistence | **G** | `capture_npc_walk`/`restore_npc_walk` with the fidelity gate. |

### O. Quests

| Aspect | Status | Reason / ID |
|---|---|---|
| Quest tables, flags, shrine bits, doom bits, shadowlords | **G** | `quest_parity` (1581-line `quest_case.inc`) green. |
| Word-of-power Yell at dungeon entrances | **Y** | Route works; unaffected by the R-19 ship branch added in Batch 3 (both branches now regression-covered — Y-24 discharged). Y-07 |
| Shrines (visit/restore/donate) | **Y** | All three modals wired in `modal()`; `UiMode::ShrineSpecial` lifecycle repaired in Batch 1 (§5). Remaining YELLOW is physical-device validation only. Y-25 |
| Search-based quest chains | **Y** | `quest_search.cpp` + `search_objects` fixtures; found objects are world objects → now survive reload (R-14 resolved, Batch 6). |
| Shards → Flames ritual | **G** reachability | R-08 resolved (Batch 3): shards 29–31 are Use-picker rows gated on `game.quest.shards[0..2]`. |
| Crown / Sceptre / Amulet | **G** reachability | R-07/R-08 resolved (Batch 3): ids 18/19/20 gated on `game.quest.artifacts[0..2]`. |
| Blackthorn / Falsehood / Abbey | **R** | R-09. Blackthorn's sacrifice roster compaction specifically is **G** (R-23 resolved, Batch 15); capture-scene pacing (**Y-32**, still open — see §4, Batch 16 investigated it and found no footage evidence to fix it against) and the end-to-end ceremony hardware walk are not covered by that. (Batch 16 correction: **Y-31** was never a Blackthorn finding — it is the unrelated Rel Hur/skull-key `(U)se` cancel-refund, resolved in Batch 16; several earlier write-ups below grouped "Y-31/Y-32" together as if both were Blackthorn pacing, which was a mis-association, not a shared root cause.) |
| Codex / endgame | **Y** | The Use chain that gated it is reachable (R-08 resolved, Batch 3); the endgame itself is still unproven end-to-end. Y-07 |
| HMS Cape plans | **G** reachability | R-08 resolved (Batch 3): id 33 gated on `game.hms_cape`. |
| Harpsichord melody (Cove passage) | **G** | R-20 resolved (Batch 3): digit keys at the harpsichord dispatch `CommandKind::HarpsichordNote`. |
| Moonstones / moongates | **G** | R-08 resolved (Batch 3): ids 21–28 are rows whenever carried (`!buried`), so `CommandKind::UseMoonstone` is reachable. Burial state (`buried`/`location`) was never part of `objects_`/R-14 — it lives in core `GameState` and already survived save/load; a revealed-but-not-yet-`(G)et`-ed ground marker is a `QuestObject` and is covered by the R-14 fix (Batch 6). Stale "Burial state does not survive save/load" wording corrected. |

### P. World objects / terrain / special cells

| Aspect | Status | Reason / ID |
|---|---|---|
| Doors, locked, secret, open-door timer | **G** | `CommandState::Door` + `CommandEffect::Doors`; `command_parity` green. |
| Signs | **G** | `look_sign` test + `resolve_look_sign`. |
| Terrain overrides (volatile/persistent/hourly/wipe) | **G** | `WorldTerrain` with a 4-layer `inspect()`; `capture_terrain`/`restore_terrain` round-trip. |
| Fields, traps, fireplaces, emitters, light flood | **G** | `presentation.cpp` `visibility()` + `kEmitters`; `presentation_regression` green. |
| Chests in the world | **G** — RESOLVED (Batch 2) | R-02 fixed. The invalid tile-1 world chests came solely from the R-03 promotion sites; legitimate stationary chest QuestObjects (`quest_world.cpp::hydrate_interior_objects`) already stored the correct pre-offset sprite tile (`0x101`) and needed no change. |
| Quest objects / hidden items | **G** | Serialized/restored via the sidecar. R-14 resolved (Batch 6) |
| Beds / camp / auto-sleep | **Y** | `AutoSleep` is core-internal; `rest.cpp` covered by `travel_parity`. Y-26 |
| Ladders / stairs / bridges | **G** | Klimb + `resolve_world_step` speed classes. |

### Q. Transport

| Aspect | Status | Reason / ID |
|---|---|---|
| Walking + speed classes (swamp/cactus/bridge) | **G** | `movement_flow_parity`. |
| Horse, carpet, skiff Board/X-it | **Y** | `transport_flow_parity` green; device unproven. Y-07 |
| Ship Board/X-it | **Y** | Same. |
| Ship sails (hoist/furl) | **G** | R-19 resolved (Batch 3): `(Y)ell` aboard a frigate dispatches `CommandKind::YellSails`. |
| Wind + drift | **G** | `turn.cpp`; wind bar in HUD. |
| Transport tile → avatar sprite | **G** | `turn_.transport_tile+0x100` in `render()`. |
| Combat while mounted/aboard | **G** | `advanced_combat_parity`. |
| Save/load of transport | **G** | `transport`, `transportTile`, `shipHull`, `shipSkiffs`, `sailDir`, `hmsCape` all round-trip. |

### R. Time / world simulation

| Aspect | Status | Reason / ID |
|---|---|---|
| Turns, clock, minutes-per-action, dungeon minute cost | **G** | `turn_parity` green; dungeon uses its own loop per `game.ts` [REF]. |
| Day/night light ramp, torch, light spell | **G** | `presentation_light_level` matches the reference ramp table. |
| Food consumption, poison, healing, rest | **G** | `travel_parity`. |
| Moon phases (Trammel/Felucca) + sky bar | **G** | `hud_world_state` + `moon_phases` resource. |
| Encounter timing | **G** | `outdoor.cpp` + `scripted_encounter.cpp`. |
| **Debug actions must not advance time** | **G** | `UiDebugMenu` mutates state directly; `debug_developer`/`debug_map_picker` tests assert no turn consumption. Teleport goes through `execute_dungeon_command` for a *real* session — correct. |
| Poison tick feedback | **G** — RESOLVED (Batch 7B) | `PoisonTick` is consumed by `PoisonFlashPacer` (`openu5/poison_tick.h`): one 93 ms blip per poisoned member in ascending SLOT order, each inverting that member's roster row (the binary's shared `0x2a28`), the flash switching off after the last. Not modal -- it swallows no input and defers no turn, because the tick fires on every step. Y-04. |

### S. Death / resurrection / party state

| Aspect | Status | Reason / ID |
|---|---|---|
| HP, death, status letters (G/P/S/D/C) | **G** | `combat_parity`, `item_parity`. |
| Party wipe / defeat | **G** | `combat.cpp` `end()` with `victory=false`. |
| Resurrection (In Mani Corp, healer, ankh) | **G** | `apply_target_spell` + `shop_orchestration` healer flow. Healer reachability was subject to R-01; R-01 is GREEN — RESOLVED (Batch 1), so this no longer applies. |
| Active member / combat actor identity | **G** | `active_member()` + `current_combat_actor()`. |
| Save/load of party status | **G** | `partyStatus`, `status`, `currentHp`, `monthsAtInn` round-trip. |

### T. Save / load

See §8. Summary: **`GameState`/`TurnState` G**, **`CommandState`/outdoor/terrain G**, **world objects R**, **dungeon session R**.

### U. System menu / settings

| Aspect | Status | Reason / ID |
|---|---|---|
| Open/close from any gameplay mode | **G** | Checked before gameplay routing in `handle()`; restores settings on close. |
| Save / load slots | **G** | `service_system_menu_intent` → `synchronize_loaded_world`; R-14/R-15 resolved (Batch 6). |
| Brightness, trackball sensitivity, text size, Movement Mode | **G** | Persisted through `AlphaSettingsService`. |
| Return to the originating gameplay mode | **G** | System menu does not touch `ui_->mode()`. |
| Modal preservation across menu open/close | **Y** | Not tested; a modal open underneath should survive. Y-27 |

### V. Developer tools

| Aspect | Status | Reason / ID |
|---|---|---|
| 12 categories, editing, confirmation | **G** | `ui_debug_menu` + `debug_developer` tests. |
| **Dungeon teleport creates a real session** | **G** | `debug_map_picker.cpp:323` builds `CommandKind::EnterDungeon` and calls `execute_dungeon_command` — the production path, not a fake. |
| No turn/RNG consumption | **G** | Asserted by `debug_developer_test`. |
| Renderer rebind after debug | **G** | `synchronize_after_debug` — correct *for its named purpose*. It also runs on ordinary input; that was R-01, which is GREEN — RESOLVED (Batch 1) (Batch 1's mode arbitration now preserves session-owned modes during ordinary per-input synchronization). |
| Max Party / Max Resources / Equip Best / Full Test Setup | **G** | Preserved. |
| On-device diagnostics (37 scenarios) | **Y** | Data-presence probes, not integration tests. Y-06 |
| Debug masking production defects | **G** — resolved (Batch 1) | Batch 1's mode arbitration now preserves session-owned modes (`Shop`/`Dialogue`/`ShrineSpecial`) during ordinary per-input synchronization, so the debug-driven masking of R-01's symptoms no longer applies. Debug teleport's own recovery path still separately calls `set_base_mode(Exploration)` — that debug-specific recovery behavior is unchanged and correct for its purpose. |

### W. Frontend / device presentation

See the renderer/mode matrix in §10.

---

## 3. KNOWN RED FAILURES

### R-01 — `synchronize_after_debug()` destroyed Shop / ShrineSpecial modes on every input · **SEVERITY 1** · **GREEN — RESOLVED (Batch 1)**

**Symptom (as originally observed):** after the first keystroke in a shop, the shop panel disappeared and subsequent keys were interpreted as world commands (`b` → "Board", `s` → "Search-", `r` → "Ready"). The active **Dialogue** prompt loop was not destroyed by this defect — it is protected by `TextEntry` modal state — but `base_mode` was still corrupted underneath it, and `ShrineSpecial` had no other exit (see the `ShrineSpecial` note below).

**Root cause:** `AlphaRuntime::synchronize_after_debug()` ran after ordinary gameplay processing on *every* input, not just debug actions, and unconditionally forced `base_mode` to Combat/Dungeon/Exploration:
```
AlphaRuntime::handle()                      alpha_runtime.cpp:857
  ui_->handle_input(action)
    → dispatch → execute_shop / dialogue
      → GameEventKind::Shop / Dialogue
        → UiSession::consume → set_base_mode(UiMode::Shop)   ui_session.cpp:817/785
  synchronize_after_debug(before, dungeon_before)            alpha_runtime.cpp:857
    ui_->set_base_mode(combat ? Combat : dungeon ? Dungeon : Exploration)
                                                             alpha_runtime.cpp:697
```
`UiSession::set_base_mode` assigns `mode_` whenever `!is_modal(mode_)` (`ui_session.cpp:166`). `Shop`, `Dialogue` and `ShrineSpecial` are **not** in `is_modal()` (`ui_session.cpp:39`), so session-owned state was overwritten on the very same input that entered it. Shop was the direct user-visible casualty; `ShrineSpecial` depended on this same clobber as its only effective exit (see the `ShrineSpecial` fix below).

**Implemented fix:** per-input mode arbitration now preserves `Shop` / `Dialogue` / `ShrineSpecial` whenever no authoritative Combat/Dungeon state overrides them, using the shared ESP-free `ui_mode_policy.h` seam (`native/targets/tdeck/main/ui_mode_policy.h`, new). The context/terrain rebind (`context_.dungeon`/`context_.combat`) still runs unconditionally; only the `set_base_mode` write is now conditional, mirroring `AlphaRuntime::command()`'s existing "only force Combat/Dungeon" rule.

**Related writer noted during Batch 1:** `finish_combat_if_needed()` also writes `base_mode` (rebinding to Dungeon/Exploration on the canonical victory exit). It was inspected and left unchanged — it is redundant-but-harmless after the ownership fix, since it only fires on the combat-teardown path and agrees with the new arbitration. See §6 for the full writer inventory.

**Evidence:** [STATIC] root cause; [EXEC] fix — `ui_mode_regression` 28/28 GREEN, host suite 57/58 (only unrelated `gameplay_parity` mismatch 59 remains), T-Deck ESP-IDF build PASS. Hardware flash not performed.

---

### R-02 — World chest objects render as deep-water terrain (ANCHOR 1b) · **SEVERITY 1** · **GREEN — RESOLVED (Batch 2)**

**Symptom (as originally observed):** `presentation.cpp:205`:
```cpp
place(o.x, o.y, (o.shadowlord || o.loot || o.search) ? o.tile + 256 : o.tile, ...)
```
A chest object has `chest=true` but `loot=false, search=false, shadowlord=false`, and `tile=1` (`combat.cpp:1665`, `world_commands.cpp`). So it was placed as **terrain tile 1 = deep water**, a solid blue cell, instead of sprite `0x101`.

**Reference [REF]:** `game.ts::lootRenderTiles()` (line 1327) renders **only** `kind==="loot"` and `kind==="search"` objects, both at `id + 0x100`, and explicitly notes that object tiles `0x101..0x10F` are `walkable:false` and must be entities, never composed terrain. Chests in the reference are map-override tiles, not entities, and their stored `WorldObject.tile` is already pre-offset into sprite-bank space at creation time (`hydrateInteriorObjects`: `tile = p.type + 0x100`).

**Corrected root cause (Batch 2 investigation):** this was **not** fundamentally a renderer-predicate bug. Every legitimate stationary chest/prop/ship producer in the native codebase already followed the reference's pre-offset convention correctly (`quest_world.cpp::hydrate_interior_objects`: `o.tile = n.type+256`; the pirate-ship prize: `o.prize.tile=292`). The only producers emitting a *raw, un-offset* `chest.tile=1` were the two invalid post-combat promotion sites documented under R-03. The rendering predicate in `presentation.cpp` and the equivalent terrain-composition predicate in `quest_world.cpp::quest_world_tile` were both faithfully rendering a bad input, not misclassifying a good one.

**Implemented fix:** none required in the renderer. Removing R-03's two invalid promotion sites removes the only producer of a bad-tile chest object; once nothing creates `chest.tile=1`, the deep-water manifestation cannot occur. **No chest-specific `+256` special case was added to `presentation.cpp`, and `quest_world_tile()` was not modified** — both were confirmed correct as-is.

**Evidence:** [STATIC] + [REF] root cause; [EXEC] fix — `presentation_regression`'s existing stationary sprite-bank tile assertion (a `torch` object built with `tile=0x101`, asserting it renders unchanged) already proves the correct convention and required no new coverage. `gameplay_parity` mismatch 59 (the reproducible manifestation of this defect combined with R-03) is GREEN. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**.

---

### R-03 — Native promotes arena chests to world objects; the reference does not (ANCHOR 1a) · **SEVERITY 1** · **GREEN — RESOLVED (Batch 2)**

**Reproducible failure (as originally observed):**
```
ctest -R gameplay_parity      →  Error: Gameplay mismatch 59
```
Sequence 59 is `move, pass, pass, fight×2000, quick×100, end` with seed 3 on tile 5. After the canonical victory exit:

| | `/worldObjects` |
|---|---|
| native (actual, pre-Batch-2) | `[{location:0, floor:0, x:10, y:10, tile:1, kind:"chest", contents:20, trapped:false}]` |
| TypeScript reference (expected) | `[]` |

**Root cause — corrected from the original audit wording: TWO promotion sites, not one.** The original audit cited only `combat.cpp:1665`. The Batch 2 investigation found a **second, independent, functionally identical** promotion site that the audit missed:
1. `native/core/src/combat.cpp` — `finish_encounter_combat()`. The teardown loop walked every `unopened_chest(state, cell)` and `append`ed a world `QuestObject{tile=1, ...}`. This is the direct/scripted-encounter path (no `OutdoorServices` owner — Troll toll refusal, camp ambushes).
2. `native/core/src/outdoor.cpp` — `outdoor_start()`'s `victory_latch`. The identical promotion, for encounters that *do* have an `OutdoorServices` owner (ordinary roaming-monster encounters) — **this is the path `gameplay_parity` sequence 59 actually exercises.**

The reference has no such promotion on either path; unclaimed arena treasure is simply lost on exit (`game.ts` `collectSpoils()` is a stats-only counter, never a `worldObjects.push`), which is why the victory arena stays open for looting.

Combined with R-02 this was exactly ANCHOR 1: **after looting everything, a blue tile-1 cell remained at the encounter coordinate.**

**Note:** `expected` here is generated by running the live TypeScript core at test time (`check-gameplay.ts` spawns `gameplay_driver` and compares against the TS run), so this is not a stale fixture. `typescript_dungeon_fixture_drift` and the other drift tests all pass, confirming fixture freshness.

**Implemented fix:** both promotion sites removed in their entirety (no replacement persistence path added — unclaimed arena treasure is simply lost, as the reference does). The pirate-ship prize promotion in `outdoor.cpp`'s `victory_latch` is unrelated and was left untouched. Two other pre-existing tests that encoded the old (buggy) promotion as their expected behavior — `combat_escape_regression` and `direct_troll_handoff_regression` — were corrected to the reference-faithful expectation as a direct consequence of this fix; their substantive escape/teardown/terrain-restoration coverage is unchanged.

**Evidence:** [EXEC] + [REF] root cause; [EXEC] fix — `gameplay_parity` mismatch 59 is **fixed**: the run now proceeds past sequence 59 entirely. `combat_loot_open_regression` (R03-DIRECT), `presentation_regression` (R03-OUTDOOR), `combat_escape_regression`, and `direct_troll_handoff_regression` are all GREEN. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**.

**Note:** `combat_cell_to_world` (the arena-relative rotation helper the removed promotion sites used) currently has no remaining production caller. This is noted for awareness only — it is **not** a required cleanup and Batch 2 deliberately did not remove or refactor it, to keep scope minimal.

---

### R-04 — Quest-object layer overdraws loot icons (ANCHOR 2) · **SEVERITY 2** · **GREEN — RESOLVED (Batch 2)**

The loot tile equations were already **correct** — verified by probe:
```
loot_render(id=2 gold)=0x102   loot_render(id=8 gem)=0x108
loot_render(chest 1)=0x101     loot_render(trapped 129)=0x101   (&0x7f folds the trap bit — matches reference)
```
**Root cause (as originally observed):** the defect was ordering. `compose_world_presentation` iterated **all** quest objects last-wins and placed every one of them; a chest, prop, ship or torch object sharing a cell with a loot object could overdraw the loot sprite with a raw terrain tile. The reference builds a `Map` keyed by cell containing **only** loot/search entries, resolved last-match/LIFO and painted after a separate first-match stationary-object layer, so a non-loot object can never mask a loot icon.

**Implemented fix:** `compose_world_presentation` now composes two reference-faithful layers instead of one unified pass:
- **Layer 1 (stationary/non-loot: chest/prop/ship/torch/plot/shadowlord)** — first matching object per cell wins, mirroring `game.ts tileAt()`'s `Array.find`.
- **Layer 2 (loot/search only)** — last matching object per cell wins (LIFO/top-of-stack), always painted *after* layer 1, mirroring `game.ts lootRenderTiles()`, so loose loot or a search find can never be masked by a co-located stationary object regardless of `QuestWorldServices` append order.

No `QuestObject` storage/layout change was made; `quest_world_tile()` was left untouched (it already implements correct first-match semantics for its own separate consumers, and the accepted RED tests did not require changing it).

**Evidence:** [STATIC] + [REF] root cause; [EXEC] fix — `presentation_regression`'s new R04-OVERLAY case (deliberately appends the loot object first and the stationary object second, defeating today's-then accidental insertion-order success, and asserts the loot sprite still wins) and R04-LIFO case (two loot objects in one cell, asserts the last-appended one is visible) are both GREEN. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**.

---

### R-05 — Dungeon presentation (ANCHOR 4) · **SEVERITY 1** · **YELLOW — presentation LOGIC resolved (Batch 9); dungeon RUNTIME resolved (Batch 9B); authored ART packed and painted (Batch 9C); awaiting the final physical dungeon session**

> **Batch 9D note (scope, not a verdict change).** The hardware session after Batch 9C confirmed the authored art, navigation, interactions and per-dungeon wall variants as **good**, and reported dead controls in combat entered *from* a dungeon. That is not a presentation defect: the renderer's own source arbitration makes `combat_source` outrank `dungeon_source`, so a dungeon-origin fight draws the combat scene and the dungeon art is not on that path at all. It is filed as a **combat-transition integration** issue (§14 Batch 9D) and the Batch 9C art verdict is **unchanged**. The Batch 9E report (a fled combat room returning the party into a sealed cell) is likewise not presentation — it was adjudicated as **reference-faithful**, with the authored escape sitting on the combat board rather than in the maze, and the authored-art evidence is again **not** downgraded. See §14 Batch 9E. One genuine R-05 **part 2** follow-up did come out of it: Batch 9B's two dungeon prompt mirrors had no production caller, so `Klimb-U/D-` and `Will you drink?` were unreachable on hardware even though the host suite proved them. Fixed in Batch 9D; §16 steps **33j and 33k must be re-judged**, not carried over as confirmed.

`native/tools/u5pack/alpha1.ts:49` packs `dungeons.bin` — 8 records × 516 bytes = **cell maps only**. There is no DNG or ITEMS record in the pack, and `native/ASSETS.md` never mentions dungeon art.

`extractor/src/parsers/dngtiles.ts` **exists** and parses exactly what is needed (`parseDngView`, `parseItemsView`), and `game/src/skin/fiel/dungeon.ts` documents the authoritative compositor: 28 pre-drawn perspective slices per wall variant blitted at fixed X per depth with mirrored pairs, plus 20 half-feature images from ITEMS.16 for stairs/fountains/traps/chests, with floor speckle and ceiling **baked into each slice**.

What the device does instead (`native_renderer.cpp:352-411`): fills the top half `kDungeonCeiling` and the bottom half `kDungeonFloor`, scans forward for a wall, draws Bresenham lines for corridor edges, then `dungeon_rect`s a flat grey wall with mortar stripes. No doors, no variant, no features beyond a green/red 12×12 blob, no light model.

**This is why the view is "geometry partially present, textures wrong".** It cannot be fixed in the renderer alone.

**Fix shape (ordered):** 1) extend `alpha1.ts` with a `dngview.bin`/`itemsview.bin` record set using the existing `dngtiles.ts` parser; 2) add slice/feature readers to `asset_pack.cpp`; 3) replace `render_dungeon_view` with a slice blitter following `dungeon.ts`'s SIDE_X / depth-pair model; 4) wire `DungeonViewInfo.wallVariant` from the dungeon id.

**Evidence:** [STATIC] + [REF] + asset-pack manifest.

#### Resolution, part 1 of 3 — presentation LOGIC (Batch 9) · **GREEN**

Batch 9 was verification-first: nothing was changed until the current device logic had been moved into a host-testable seam and made to fail. The whole of `render_dungeon_view()`'s decision-making was ported verbatim into a new core module and `dungeon_view_regression` was written against the reference; that port failed **38 assertions**, each traceable to a named routine. Nine findings, one of them a false alarm:

| # | Required behaviour | Native before Batch 9 | Evidence | Verdict |
|---|---|---|---|---|
| 1 | Torus wrap: the ray marches unwrapped, the map is read `&7`, so the corridor continues across the border | already correct (`&7` inside the cell read) | `coreview.ts dungeonView().add()`; `native_renderer.cpp` | **CORRECT** — §12 row 8's "wraps around and reports a wall from behind" concern is **withdrawn**: the wrap is what the original does, deliberately |
| 2 | Light gate: `lit = torch_turns > 0 \|\| light_spell_minutes > 0`; unlit ⇒ the raycast is skipped and the viewport is **black** | no light gate at all — the corridor was always fully lit | `light.ts visibleDepth()`; DUNGEON:0x1AD6-0x1AE4; and `dungeon.cpp`'s own (S)earch, which already applies exactly this double gate before printing `"darkness."` | **DEFECT** |
| 3 | Sight blocker = kind ≥ 0xa, **plus** a *revealed* secret door | the **movement** blocker (11 / 12 / unrevealed 13) was reused for sight | `dungeon.ts blocksView()` vs `isPassable()` | **DEFECT** — a door, a room or a rooms-broke cell ahead was see-through, and a found secret door stopped being drawn at all |
| 4 | Front wall classified: Wall/SecretDoor → 8, SpecialWall → 0x18, rooms-broke/door/room → 12 (a dead end **with a door**); a revealed secret door is re-read as a normal door first | one flat grey rect for every case | `frontBase()`; fn_150a @0x150a, table 0x2e80 | **DEFECT** |
| 5 | Side slice classified by the neighbour: <0xa → 0x10 open passage, 0xc → 0x14 alcove, {0xa,0xe,0xf} → 4 side door, else 0 plain wall; **both** sides always emitted, the right one mirrored, at table 0x2e62's X | a single undifferentiated edge line, drawn only when the *movement* blocker said "wall" | `sideBase()`; fn_1682 @0x1682 | **DEFECT** |
| 6 | Standing on a normal door suppresses ring 0's two side slices | always emitted | driver @0x1b1e | **DEFECT** |
| 7 | Features (kinds 1–8) at **every** depth 0–3, painted far→near, after the corridor; a trap only with `(sub & 7) == 0`; a magic field has no ITEMS.16 art | only the cell underfoot, as one 12×12 blob, with no trap gate | `planDungeonView()` features loop; `featureBlits()`; fn_1952 @0x197b | **DEFECT** — a ladder or chest one cell ahead was invisible |
| 8 | The wanderer at depth 1–3, compared with the `&7` wrap; `hidden` means the **ceiling row** | depth 1 only, and `hidden` suppressed it entirely | `planDungeonView()` monster; `api.ts` ("ceiling … NO es invisibilidad"); tables 0x2E2A / 0x2E32 | **DEFECT** ×2 |
| 9 | `wallVariant(location)` reaches the view | derived correctly but **only** inside (S)earch's message switch; the view never saw it | `wallVariant()` (`dungeon.ts:1768`) vs `dungeon.cpp`'s inlined copy | **DEFECT** |
| 10 | Dungeon bands: top `L1`..`L8`, bottom `Dir:` + facing right-justified in 7 | a blank sky strip and `Wind: --` covering 18 of 176 viewport rows | `skin.ts:1696-97`; `dungeonLevelLabel` / `dungeonDirLabel`; dng_draw_panel @0x01D2 | **DEFECT** — §12 rows 12/13. A turn in place in a symmetric corridor produced **no on-screen change whatsoever** |
| 11 | Every dungeon mutation is visible on the frame that produced it | already correct — `AlphaRuntime::dispatch` sets `dirty_` on every input, `dungeon_presentation_pending_` forces a full redraw on entry/teleport, and the board's cache key is a CRC over the real pixels | `alpha_runtime.cpp`; `dungeon_report()` | **CORRECT** — no stale-frame defect exists |
| 12 | Authored slice/feature textures | **not in the asset pack** | `alpha1.ts`; `native/ASSETS.md` | **DEFECT — still OPEN, see part 2** |

**Fix.** A new portable module, `native/core/src/dungeon_view.cpp` + `include/openu5/dungeon_view.h`, implements the original's `dng_draw_view` driver as a pure function: `plan_dungeon_view(GameState, TurnState, DungeonState) -> DungeonViewPlan`, a deterministic list of `Side` / `Front` / `Feature` / `Monster` ops carrying slice piece codes, the table-0x2e62 X positions, the mirror flag, and the wall variant. `dungeon_visible_depth()`, `dungeon_wall_variant()`, `dungeon_view_blocks()`, `dungeon_side_slice_base()`, `dungeon_front_slice_base()` and `dungeon_feature_drawable()` are exported beside it. `native_renderer.cpp`'s `render_dungeon_view()` now only **paints** that plan — it no longer re-derives geometry, sight or contents from the map, so the picture cannot disagree with the core about what the party is looking at. `dungeon.cpp`'s (S)earch switch now calls the shared `dungeon_wall_variant()` instead of its inlined copy, so the stalactite/caved-in/skeleton message and the wall texture cannot drift apart.

**Deliberately NOT changed.** The movement blocker (`dungeon.cpp`'s `type == 11 || 12 || unrevealed 13`) is reference-correct and was left exactly as it was; `dungeon_view_blocks()` is a separate, second rule. No canonical gameplay rule was touched to make presentation easier, and the dungeon controls — already correct per §12 — were not touched either.

**Platform divergence, documented.** The original paints its two dungeon bands in the 8 px frame margin *outside* the 176×176 viewport (`frame.ts VIEWPORT = {x:8,y:8}`). The T-Deck's frame is 2 px, so there is nowhere outside to put them; `hud_dungeon_bands()` supplies the original's **content** and `tdeck_board.cpp` draws it over the same two 9 px strips the overworld uses for sky and wind. Position diverges, information does not — and the strips were already overdrawing the viewport before Batch 9, carrying nothing.

**Placeholder honesty.** With no slice atlas the device paints the reference's **own** packless fallbacks (`drawSidePlaceholder` / `drawFrontPlaceholder` / `drawContents`' primitive branch), including its `ringBox()` geometry, which is derived from the real `SIDE_X` table — the four hand-tuned constant arrays that used to define the corridor are gone. The wall variant tints the placeholder rather than selecting an atlas; that tint is a stand-in, to be replaced by atlas selection when part 2 lands.

**Coverage.** `native/core/tests/dungeon_view_test.cpp` (`dungeon_view_regression`), 13 blocks: D1 light gate, D2 sight-vs-movement, D3 front classification, D4 side classification, D5 standing on a door, D6 feature depths and painter order, D7 trap/field gates, D8 wanderer depth/bank/ceiling/floor, D9 wall variant for all eight dungeons, D10 orientation, D11 cone depth, D12 bands, D13 **mutation → visible state in one step** — D13 drives the real `dungeon_action()` and asserts the plan already reflects a turn, a step, a Klimb between levels, a (S)earch that reveals an adjacent secret door, and a sprung bomb trap that clears the cell underfoot, with no second input.

**Verification.** RED **38 failing assertions** against the ported current logic; GREEN all pass. Full host `ctest` **69/71** — the only failures are the two known unrelated ones, `gameplay_parity` (R-21, still mismatch **2034**, unchanged) and the `quest_parity` GCC/w64devkit `STATUS_ACCESS_VIOLATION`; the baseline before any edit was 68/70 with the same two. T-Deck ESP-IDF build: **PASS**, `build-batch9-dungeon-presentation/openu5_tdeck.bin`, **0xcf150 (848,720) bytes**, 19% of the 1 MiB app partition free, zero warnings (+2,624 bytes over Batch 8B's 0xce310, as expected for a new module and a classified painter). Hardware flash: **not performed this batch** — see §16 for the checklist.

#### Resolution, part 2 of 3 — dungeon RUNTIME (Batch 9B) · **GREEN**

Batch 9 shipped without a hardware flash. The physical T-Deck Plus session that followed it reported two defects that no host test could have caught, because neither lives in the code Batch 9 touched:

> *"after entering a dungeon, normal dungeon commands became non-functional — still true after teleporting out, lighting a torch and re-entering"*, and *"while physically inside **Deceit** the location strip read **Serpent's Hold**; the `L#` and `Dir:` bands were correct."*

Batch 9B investigated both from the input edge inward and found two independent, pre-existing defects. **Neither was introduced by Batch 9** — `git show f3a2f0ec` touches `dungeon_view.{h,cpp}`, `hud.{h,cpp}`, `dungeon.cpp`, `native_renderer.{h,cpp}`, `tdeck_board.{h,cpp}` and two call sites in `alpha_runtime.cpp`, and nothing whatsoever in the input path. Batch 9 made them *visible*: before it, the corridor was lit unconditionally and the two strips carried no dungeon information, so neither a dead key nor a wrong caption had anything to contradict.

##### Finding A — the dungeon key map was a subset of the reference's, and three core capabilities had no input path at all

`UiSession::handle_dungeon()` is the only thing a physical key ever reaches in a corridor. Compared against the reference's own DUNGEON dispatcher (`game/src/main.ts`, `handleDungeonKey` + the command table below it, mirroring jump table 0x3178 / default 0x34D8), it was missing six commands and mis-bound a seventh:

| Key | Reference | Native before Batch 9B | Core support that could not be reached | Verdict |
|---|---|---|---|---|
| `I` | `ignite` (CMDS.OVL 0x0D98) | `"What?"` | `commands.cpp:752` has a dedicated `c.dungeon && CommandKind::Ignite` arm that seeds `ignite_torch()` with `dungeon_context->state.pos.dungeon` | **DEFECT — the critical one.** A party that ran out of light *inside* a dungeon could not make light. This is precisely the loop the physical tester hit: dark corridor → `I` → `"What?"` → leave, ignite outside, come back |
| Enter / `.` | `turnAround` | Enter → **Pass**; `.` → `"What?"` | `DungeonAction::TurnAround`, `dungeon.cpp:501` | **DEFECT** — `TurnAround` had **zero** references outside its own `case`. On a T-Deck, whose keyboard has no arrow keys and whose only direction input is a four-way trackball, an about-face was unreachable |
| `D` | `drink` (declared QoL shortcut; on a fountain it first asks "Will you drink?", DS 0x7700) | `"What?"` | `DungeonAction::Drink`, `dungeon.cpp:342` | **DEFECT** — likewise zero references |
| `H` | `startCamp()` (kernel 0x3C9A branch loc>=0x21) | `"What?"` | `commands.cpp:759` `dungeon_camp` exists for exactly this, and had no caller | **DEFECT** |
| `K` on an up **and** down cell | opens `"Klimb-U/D-"` (0x6cba) and waits | resolved silently, always **up** | `dungeon_klimb_choice()` (`dungeon.h:77`) had **no caller anywhere in the tree** | **DEFECT** — the resolver prefers up whenever up exists, so a party could never **descend** from a ladder-up-and-down cell |
| `S` | asks `"Dir-"` (SJOG 0x0672): up Ahead / down Here / left Left / right Right | dispatched immediately, always Ahead | `dungeon_action()`'s `dir` parameter (0/1/2/3) has carried this since the port landed; nothing ever supplied it | **DEFECT** — three of the four targets unreachable |
| digits | Set Active Plr (DUNGEON 0x07bc, return forced to 0 = no turn) | `"What?"` | `CommandKind::SetActivePlayer` | **DEFECT** |
| `W` | `"W-What?"` (0x3450) | plain `"What?"` | — | **DEFECT** (cosmetic) |

**Why no host test caught it.** `dungeon_view_regression` proves `plan_dungeon_view()` and never presses a key. `dungeon_parity` and `dungeon_flow_parity` call `dungeon_action()` and `execute_dungeon_command()` **directly**. Nothing anywhere asserted that the UI router could *reach* each core action — the one link in the chain a physical key actually depends on. §12's "the controls are already correct" row was a **false negative**: it was read off `dungeon_action()`'s completeness, not off the dispatcher's.

**Fix.** `UiSession::handle_dungeon()` now implements the reference's full dispatcher. Enter/`.` are `TurnAround`; Space stays Pass. `I`/`D`/`H`/digits are routed. `K` consults the new prompt mirror and opens the reference's U/D prompt only when the cell genuinely offers both; `S` always opens the `Dir-` prompt. Both prompts reuse the existing `TargetSelection` modal and deliver their answer in `dungeon_action()`'s own `dir` parameter (`Command::hours`), never as a compass direction — the corridor's geometry is relative to the party's facing. A key outside a prompt's set is ignored and the prompt stays open, reproducing the original's `getkey` loop (1eca-1ece-1eac); the abort is the original's own "Pass" (DS 0x84ec for `Dir-`, 0x6cc6 for Klimb, i.e. `dir == 2`), so even the abort is one core call rather than a UI-invented no-op.

Two predicates UiSession cannot derive (it owns no dungeon state) arrive through `set_dungeon_prompt_context()` / `refresh_dungeon_context()` — the same narrow-mirror contract as `set_sail_context()` (R-19) and `set_harpsichord_active()` (R-20). `dungeon_action()` re-checks both authoritatively (`"No fountain here."`, and Klimb's own up/down caps), so a stale mirror can only mis-route a **prompt**, never mis-apply an **action**.

**Not changed:** no `DungeonAction` semantics, no movement blocker, no sight rule, no light gate, and nothing in `dungeon.cpp` at all. This is a routing fix.

##### Finding B — the location caption read the surface map id, which a dungeon session deliberately never updates

`tdeck_board.cpp:759` composed the caption from `game.position.map.location` alone. A dungeon session does **not** rewrite `GameState::position`: the surface map id and coordinate are the RETURN context and must survive the whole descent (`openu5::exit_dungeon()` is what consumes them). So while the party is underground the caption names wherever it happened to be last.

That reproduces the report exactly. `location_display_name()` indexes 32 to `"Serpent's Hold"` and 33 to `"Deceit"`; a tester who reached Deceit **from** Serpent's Hold left `position.map.location == 32` behind, and the strip kept printing it. It is not an off-by-one and not a cache: the caption has no cache at all and is redrawn on every full render — it was simply reading the wrong source. That also explains why the `L#` and `Dir:` bands were right: `hud_dungeon_bands()` reads the authoritative `DungeonState`.

**Fix.** `HudDungeonBands` now carries `dungeon_id` (straight from `DungeonState::pos.dungeon`), and the caption goes through a new host-portable helper, `tdeck::hud_location_caption()` in `main/location_names.h`, which prefers the dungeon identity whenever a session is mounted. Caption and bands therefore share one source and cannot disagree about which dungeon is on screen. Dungeons already occupied 33..40 in the same display table, so all eight resolve with no new data.

##### Coverage

`native/targets/tdeck/host_tests/dungeon_input_test.cpp` gives `dungeon_input_regression`, **65 checks**. Unlike the two existing dungeon suites it drives the **physical device path** end to end:

```
RawInputEvent (matrix code / trackball kind)
  -> tdeck::UiInputAdapter::translate()      main/ui_input_adapter.cpp
  -> UiSession::handle_input() -> handle_dungeon() / handle_modal()
  -> UiIntent -> dispatch_world_command() -> execute_dungeon_command()
  -> dungeon_action() -> DungeonState mutation
  -> tdeck::resolve_synchronized_base_mode()  main/ui_mode_policy.h
```

`AlphaRuntime` still cannot be host-compiled, so its per-input tail is exercised through the same `ui_mode_policy.h` seam it calls, exactly as `ui_mode_test.cpp` does. Blocks: **D-IN-1** movement/turning, **D-IN-2** reference command coverage (13 checks), **D-IN-3** Klimb reaches both directions, **D-IN-4** the `Dir-` choice, **D-IN-5** darkness suppresses sight but **not** input dispatch (including igniting *from* the dark), **D-IN-6** entry/exit/re-entry leave no modal owning the keyboard, **D-LOC-1** all eight dungeon names plus the exact Serpent's-Hold-stale case, **D-LOC-2** surface to dungeon to surface transitions.

**Verification.** RED **24 of 65 failing** against a `git worktree` at Batch 9 (`f3a2f0ec`) carrying only the additive API surface as shims, so the RED isolates the behaviour and not the compile; GREEN **0 of 65**. `dungeon_view_regression` is **unchanged** and still passes — nothing Batch 9 proved was revised. Full host `ctest` **70/72**, the two failures being the known unrelated `gameplay_parity` (R-21, mismatch **2034**, unchanged) and `quest_parity` (GCC/w64devkit `STATUS_ACCESS_VIOLATION`); the pre-edit baseline this batch measured was 69/71 with the same two. T-Deck ESP-IDF build: **PASS**, `build-batch9b-input/openu5_tdeck.bin`, **0xcf540 (849,216) bytes**, 19% free, zero compiler warnings, **+1,008 bytes** over Batch 9.

> **Correction to the Batch 9 record.** Batch 9 reported its binary as "0xcf150 (848,720) bytes". The hex is right and the decimal is not: `0xcf150` is **848,208**. This batch rebuilt `f3a2f0ec` unchanged and measured 848,208 B on disk. All deltas here are against that figure.

**Physical test:** the Phase 6B block of §16, steps 33g-33l. **Not performed this batch** — no hardware in this session. Batch 9B's brief makes that checkpoint a hard gate before the authored-art work, so part 3 was deliberately not started; see §14.

**Still open after Batch 9B:** `(L)ook` in a corridor. The reference's dungeon `L` opens its own `Dir-` prompt and chains the fountain-drink beat (`DNGLOOK 0x012f`); the native core has no `DungeonAction::Look` at all, so wiring a key would have meant inventing gameplay rather than reaching an existing rule. Left out deliberately, recorded here rather than silently.

#### Resolved, part 3 of 3 — the ASSET pipeline · **Batch 9C**

This was the original R-05 finding, and it is now closed on the host side. `native/tools/u5pack/alpha1.ts` packed `dungeons.bin` — 8 records × 516 bytes = **cell maps only** — and nothing else about a dungeon's appearance. Batch 9C added five ordinary TOC entries carrying the authored art itself, reusing the accepted extractor parsers rather than writing new ones, and replaced the wireframe painting with a blitter that consumes them.

**What is packed** (`native/tools/u5pack/alpha1-dungeon-art.ts`, container `OU5DART1`):

| Entry | Source | Banks × images | Payload | Entry bytes | Entry CRC32 |
|---|---|---|---|---|---|
| `dungeon-dng1.art` | `DNG1.16` | 1 × 28 | 54,448 B 4bpp | 54,800 | `0x088b567b` |
| `dungeon-dng2.art` | `DNG2.16` | 1 × 28 | 54,448 B 4bpp | 54,800 | `0x98754b71` |
| `dungeon-dng3.art` | `DNG3.16` | 1 × 28 | 54,448 B 4bpp | 54,800 | `0x1631de3b` |
| `dungeon-items.art` | `ITEMS.16` | 1 × 20 | 8,160 B 4bpp + 2,040 B mask | 10,456 | `0xf97191f7` |
| `dungeon-mon.art` | `MON0-7.16` | 8 × 6 | 16,256 B 4bpp + 4,064 B mask | 20,912 | `0xfa91efa5` |
| **Total** | | **146 images** | **193,864 B authored** | **195,768** | |

The images are stored in the originals' **own** 4bpp indexed encoding plus the containers' own 1bpp AND-masks — nothing is pre-expanded to RGB565. That is half the bytes, and it keeps the packed data byte-identical to the source files, which is what makes the identity check below meaningful. An index becomes a colour only at blit time, through `openu5::kDungeonEgaRgb565`. Transparency is the mask, never colour-0 keying: the open chest's black interior is opaque, and keying would punch a hole through it.

**Identity.** `native/core/tools/check-dungeon-art-identity.ts` (ctest `typescript_dungeon_art_identity`) compares dimensions, indexed-pixel CRC32 and mask CRC32 for one image of every semantic class across **three independent readings** — the accepted extractor parsers reading the raw file, the packer's own reader, and the bytes in the generated pack read back through the container the device reads — and re-derives the pixels a fourth way, from the extractor's decoded RGBA, so a nibble-order or stride mistake cannot hide. It also rebuilds every entry and requires the result to equal the pack byte for byte, which is what makes the pack's identity constants describe a reproducible build. With the user's original data absent it reports SKIP and passes, the same convention the extractor catalog's optional stages use.

**Pack identity changed, as approved.** 34 entries / 1,843,457 B / payload CRC `0x2b1449f4` → **39 entries / 2,039,545 B / payload CRC `0x2065ad91`** (SHA-256 `434cd664…b4ea`), a delta of **+196,088 B** (195,768 B of art plus five 64-byte TOC records). `kExpectedAlphaResourceSize`, `kExpectedAlphaResourceCrc32` and `kExpectedAlphaResourceSha256` were updated together, and the five entry names were added to `AlphaResourcePack::open()`'s `required[]` so a pre-9C card is rejected **by name** and not only by the size/CRC lock. Verified in both directions against the retained old pack: old → `firmware_match=false` (device shows the identity screen and refuses to start), new → `firmware_match=true`. The entry bound in `alpha_resource_limits.h` is 64 and was **not** raised; 39 fits. The pack format was **not** version-bumped: a named-entry TOC needs none for new entries.

**Runtime residency and cache.** `tdeck::DungeonArtCache` reads all five entries once, inside `AlphaRuntime::initialize()`, while the pack is still open — `main.cpp` closes it immediately afterwards, which is also why the device performs **no SD access for art at any point during play**: a dungeon transition can neither stall nor fail on the card. Resident cost is **195,768 B of PSRAM** (the five entry blobs, indexed in place) plus ≈1.8 KB of internal-RAM descriptor tables, which stay internal for the same reason `alpha_resources.h` gives for its own pointer tables: the painter walks them every redraw.

All three wall variants are resident, not one. The party can enter any of the eight dungeons in a single session, so over a session all three genuinely *are* what is needed; the alternative — one resident variant reloaded on entry — would save ≈109 KB of a part with megabytes spare in exchange for SD I/O inside a dungeon transition. The trade was made deliberately in favour of the quieter runtime. The consequence is that the cache is a **selector, not a loader**: `select()` repoints the wall surface array and reads nothing, so first entry, turning, stepping, changing level, leaving, re-entering and switching to a different-variant dungeon all cost **zero reloads**. Its identity is `openu5::DungeonArtCacheKey` — the wall bank alone — which `dungeon_art_regression` proves is stable under turn/move/level/light changes and changes only with the variant. It also proves that the shortcut `alpha_runtime.cpp` uses to build the key (straight from the dungeon number, rather than building a whole plan to read one field) agrees with the plan for **every** dungeon.

**Degradation.** A missing, truncated or tampered art entry fails `openu5::dungeon_art_parse()` — which validates the container's shape and every pixel/mask span **before** indexing anything, so a half-indexed bank is impossible — the cache stays unready, and the corridor paints **black** rather than substitute geometry. A bad pack is therefore visibly a bad pack, not a crash and not a silent fallback.

**Fix shape (unchanged, now with the seam in place):** 1) extend `alpha1.ts` with `dngview.bin`/`itemsview.bin` using the existing `dngtiles.ts` parser; 2) add slice/feature readers to `asset_pack.cpp`; 3) replace the placeholder painting inside `render_dungeon_view` with a slice blitter — **the plan it consumes already carries the piece code, the X, the mirror flag and the variant**, so this is now a paint-only change; 4) port `featureBlits()`' anchor tables (`FEAT_Y_LADDER_UP`, `FEAT_Y_FLOOR`, `FEAT_Y_TOP_HORIZON`) for the ITEMS.16 half-images.

**Evidence:** [STATIC] + [REF] + asset-pack manifest + [EXEC] the byte measurement above.

---

### R-06 — `Ready` is rejected in combat and in dungeons · **SEVERITY 2** · **GREEN — RESOLVED (Batch 3)**

Probe output [EXEC]:
```
PROBE1 Ready-in-combat   status=InvalidContext  equipped_weapon=0
PROBE2 Ready-in-dungeon  status=InvalidContext  equipped_weapon=0
PROBE3 Ready-in-world    status=CoreError       equipped_weapon=0   (control: reaches the real handler)
```
`commands.cpp:660` routes only `EnterDungeon | DungeonCommand | Cast | UseItem` into `execute_dungeon_command`, and `:662` routes only the `CombatMove..Cast` / `CombatKlimb..CombatSearch` ranges into combat. `Ready` is in neither list, so it falls into the blanket gate at `:758` (`if (c.combat || (c.dungeon && !dungeon_camp) || ...) → InvalidContext`).

Meanwhile `UiSession::handle_combat` case `'r'` and `handle_dungeon` case `'r'` both open the equipment selector, and `AlphaRuntime::dispatch`'s `OpenEquipmentSelection` branch has explicit combat-actor handling. The player selects an item and **nothing happens** — no message, no state change. `inventory.h::equip_item` even has a `battle` parameter for exactly this case that is never reached.

**Fix shape:** add `Ready`/`Unready` to the dungeon and combat routing predicates and pass `battle = c.combat` into `equip_item`.

**Evidence:** [EXEC].

#### Resolution (Batch 3)

Reference-correct semantics, one context at a time:

| Context | Routing | `equip_item` `battle` | Armour lock (ids 9-15) | Turn/action cost | Actor cache |
|---|---|---|---|---|---|
| World / town | unchanged | `false` | no | free action (unchanged) | n/a |
| **Dungeon corridor** | now legal | `false` | **no** - a dungeon is not a battle | **free action, no turn charged** | n/a |
| **Combat arena** | now legal | `true` | **yes** - preserved | see note below | **refreshed** |

`commands.cpp`'s blanket context gate now carries an explicit `ready_anywhere` exemption for `CommandKind::Ready` on three of its clauses (`c.combat`, `c.dungeon && !dungeon_camp`, and the `location 33-40` clause - the range the binary writes into `g_location` on dungeon entry). Everything else about the gate is untouched. The `battle` flag is `c.combat` and nothing else, so a dungeon corridor Ready keeps passing `false` and can still change armour.

**Explicit `CombatActor` refresh after a successful combat Ready.** `equip_item()` is deliberately left CombatState-unaware - inventory does not learn about combat. Instead the Ready **handler** orchestrates two steps, exactly as the reference's `game.readyItem()` does (`equipItem()` then `combat.syncPlayerEquip()`):

```
CommandKind::Ready
  -> equip_item(game, member, item, rand, /*battle=*/c.combat)
  -> if ok && c.combat && c.combat_context:
         resync_player_equipment(*c.combat_context, member)
```

`resync_player_equipment()` (`combat.cpp`, declared in `combat.h`) refreshes **only** the equipment-derived cache: `weapon_count`, `weapons[]`, `attack`, `range`, `defense`. HP, position, status, the enemy flag, initiative/`counter` and every other dynamic field are untouched, matching the reference's "no toca HP ni iniciativa". Member to actor mapping is by the actor's own `member` field, **not** by array index: arena construction skips dead members and enemies share the array, so index equality does not hold.

To make initial construction and mid-combat refresh incapable of drifting, the weapon-cache computation was factored into one internal helper, `load_equipment_cache()`, now called by both `initialize_combat()` and `resync_player_equipment()`. It mirrors the reference's `characterWeapons()` (helmet, weapon, shield in order; skip slot 255 and any item whose attack-table entry is <= 0; empty result collapses to the generic `CombatWeapon` sentinel).

**Turn/action cost in combat - implemented at the picker-interaction layer (R-06 correction pass).**

Authoritative reference control flow, traced end-to-end rather than inferred from comments:

| Moment | Reference | Charges? |
|---|---|---|
| `R` pressed in the arena | `main.ts:3347` -> `hud.echo(CMD_STRINGS.ready)` -> `openCombatReadyPicker(cur.charIdx)` | no |
| Picker opens (`hud.message(READY_UI.item)`) | — | no |
| No equippable rows | early return: `"Thou art empty-handed!"`, picker never opens, then `combatOut(cb.playerReady())` | **yes, once** |
| Cursor move (`act.kind === "move"`) | `publish()` | no |
| Equip succeeds (`act.kind === "equip"`) | `game.readyItem(charIdx, id, true)`, then `publish()` — **picker stays open** | **no** |
| Equip rejected (armour lock / hands / strength / ammo) | `readyRejectLines(r.message)`, then `publish()` — **picker stays open** | **no** |
| `"Ring vanishes!"` (`r.vanished`) | `closeAndEndTurn(false)` — closes without `"Done"` | **yes, once** |
| ESC / Done (`act.kind === "close"`) | `closeAndEndTurn(true)` — closes with `"Done"` | **yes, once** |

`closeAndEndTurn` tears the picker down (`view.setReadyPicker(null)`, `prompts.current = null`, `refreshAwaiting()`) and *then* calls `combatOut(cb.playerReady())`. `CombatSession.playerReady()` (`game/src/core/combat/combat.ts:3102`) is `requirePlayerTurn()` + `advanceTurn()` and nothing else — body-identical to `playerYieldTurn()` (`:3085`). `readyPickerKey` -> `itemPageKey` (`game/src/core/itemPageController.ts:48`) maps Enter/Space to `enter` (-> `equip`) and Escape to `close`, so **ESC is not a free cancel in combat**: it charges, unlike the (U)se picker's ESC, which prints `"None!"` and costs nothing (`main.ts:5119`).

So the cost is **exactly one combat action per `R` interaction**, at close, independent of how many rows were inspected, equipped or refused, and independent of whether anything changed.

The overworld/town/dungeon path is a different function: `doReady()` -> `pickMember()` -> `openReadyPicker()` (`main.ts:4643`), whose `close()` is silent and calls no turn API at all, and whose member-selection ESC prints `"None!"` and returns. Ready is a **free action** there.

**Ownership layer.** The cost is a property of the *picker interaction*, not of an equip, so it is charged in `UiSession::cancel_modal()` — the one place a Ready picker terminally closes in this port. `equip_item()` remains completely unaware of combat and of action cost, and the per-equip `CommandKind::Ready` handler charges nothing:

- an equip leaves through `finish_modal()` (accepted), and `AlphaRuntime::modal()` then reopens the selector — a fresh `begin_selection()`, never a close, so no charge;
- the terminal close (`Cancel`/`Back`) leaves through `cancel_modal()`, which dispatches the cancelled `ModalResponse` first and then a `CommandKind::CombatYield` command — the same teardown-then-spend order as `closeAndEndTurn`;
- the charge is gated on `request == UiRequestId::Equipment && return_mode_ == UiMode::Combat`, so world and dungeon-corridor Ready stay free and the member-selection step (`UiRequestId::EquipmentMember`, which `AlphaRuntime` skips in combat anyway) is never charged.

`CombatYield` is the port's existing silent advance-the-turn action (`combat.cpp`: `if (action == CombatAction::Yield) { e.advance(); return CombatResult::Ok; }`, behind the same `if (!player(*a)) return;` guard that `requirePlayerTurn()` provides), i.e. the exact counterpart of `playerReady()`. It was previously listed as a dead `CommandKind` in section 5; it now has this one route.

**Two narrow presentational divergences remain — tracked as Y-28, cost is unaffected:**

1. **Empty-handed.** The reference does not open the picker and charges immediately. This port always opens the selection (`AlphaRuntime::open_selection()` inserts a disabled `"(None available)"` row), so the charge arrives when the player dismisses it. Still exactly one action per `R`, but it costs the player one extra keypress and a picker flash.
2. **`"Ring vanishes!"`.** `equip_item()` does model `ItemResult::vanished` (`inventory.cpp:146`), but `AlphaRuntime::modal()`'s Equipment branch reopens the selector unconditionally instead of closing early on it. The reference closes the picker at that moment. Again the charge is still exactly one, at the eventual close.

Neither changes how many combat actions an `R` interaction costs, which is why R-06 is GREEN; both are real presentation gaps and are named rather than left implicit.

`CommandKind::Unready` was **not** touched: it has no device route (`equip_item` toggles, so unequipping already works through Ready) and is out of this batch's scope. See section 5.

**Regression coverage:** `native/core/tests/batch3_group_c_test.cpp`, 37 checks.

*Core/command layer (real `execute_command()` path):* C1 world Ready guard, C2 dungeon-corridor Ready (success, no turn charged), C3 combat Ready routing, C4 combat Ready equipment/cache invariant, C4 characterization (a *direct* `equip_item()` call still leaves the cache stale - proving the resync belongs to the Ready orchestration, not to `equip_item`), C5 combat armour lock, C6 dungeon != battle.

*Picker-interaction layer (real `UiSession::handle_input()` -> `dispatch()` path with a spy dispatcher):* C7 world Ready close charges nothing, C8 dungeon-corridor Ready close charges nothing, C9 combat Ready charges exactly once and only at close, with the teardown dispatched before the turn is spent, C10 three equips inside one interaction charge nothing and the terminal close still charges exactly once (no double-charge from repeated selection callbacks), C11 a rejected equip charges nothing at the attempt and the interaction still costs exactly one, plus an empty-handed picker that dispatches nothing on Confirm and charges exactly once on dismissal, C12 cancelling the Ready member selection is free.

---

### R-07 — Use picker binds item id 18 to "Grapple"; id 18 is the Amulet of Lord British · **SEVERITY 2** · **GREEN — RESOLVED (Batch 3)**

`alpha_runtime.cpp` `open_selection`, InventorySelection branch:
```cpp
if (game_.grapple) add(18, openu5::usable_item_display_name(2));   // "Grapple"
```
Authoritative id space [REF] — `game/src/core/endgame/use-tools.ts:7`, transcribed from the ZSTATS extended item table at `0xB9EE` and the CAST.OVL jump table at `0x185d`:
```
0x10 Carpet · 0x11 Skull Key · 0x12 Amulet · 0x13 Crown · 0x14 Sceptre ·
0x1d/1e/1f Shards · 0x20 Spyglass · 0x21 Plans · 0x22 Sextant · 0x23 Watch · 0x24 Badge · 0x25 Box
```
`use_quest_item` agrees (`quest_world.cpp:84`: `id==18 → Amulet`, toggling `time_spell` `\x0e`).

So selecting "Grapple" **toggles the Amulet of Lord British's status effect** — presentation says one thing, state does another.

Additionally, the Grapple is not a `(U)se` item in Ultima V at all: it is consumed by `K)limb` over mountains (`game.ts::klimbGrapple`, CMDS 0x1C20). It should not be in the picker.

**Fix shape:** drop the grapple row; add id 18 gated on the amulet possession flag with the correct name.

**Evidence:** [REF] + [STATIC].

#### Resolution (Batch 3)

`display_names.cpp::usable_names` is now indexed by the **real canonical id and nothing else**. The former alternate "offset" reading (slot 0 = Carpet, 1 = Skull Key, 2 = Grapple) is gone - there is exactly one interpretation of the index, so an offset bug cannot recur. Ids 0-15 are deliberately null (the scroll/potion ranges, named by `scroll_display_name()`/`potion_display_name()`), and id 18 is `"Amulet of Lord British"`.

**Grapple remains a Klimb-only item.** It is not an entry of the (U)se extended-item table at all; it is consumed by `commands.cpp`'s Klimb handler and `dungeon.cpp`'s pit logic. `UsableItemPickerInput` still carries a `grapple` field so a caller can pass one whole possession picture, but the seam **never reads it**, and this is stated at the field.

---

### R-08 — The endgame (U)se chain is unreachable from the T-Deck · **SEVERITY 1** · **GREEN — RESOLVED (Batch 3)**

The picker offers ids **16, 17, 18(wrong), 32, 34, 37**. The core implements **16, 17, 18, 19, 20, 21–28, 29–31, 32, 33, 34, 35, 36, 37**.

Missing, with the quest each one blocks:

| id | Item | Blocks |
|---|---|---|
| 19 | Crown of Lord British | Blackthorn absorption immunity, endgame |
| 20 | Sceptre of Lord British | Force-field dissolution (world **and** dungeon) |
| 21–28 | Moonstones | Moongate travel, `use_moonstone`, `CommandKind::UseMoonstone` |
| 29–31 | Shards of Falsehood/Hatred/Cowardice | **The shard-into-flame ritual — the game's central quest** |
| 33 | HMS Cape plans | Ship rigging |
| 35 | Pocket Watch | (cosmetic) |
| 36 | Black Badge | Blackthorn infiltration |

`display_names.cpp::usable_names` also has `nullptr` at every one of those indices, so even if a row were added it would render as `"Unresolved id N"` and be disabled.

**Fix shape:** one table extension in `display_names.cpp` + one predicate list in `open_selection` driven by `game_.specialItems`/`shards`/`crown`/`sceptre`/`hmsCape`/`blackBadge` (all of which already exist and already round-trip through save).

**Evidence:** [STATIC] + [REF].

#### Resolution (Batch 3)

Row selection lives in one shared, ESP-free seam - `openu5::usable_item_picker_rows()` (`native/core/include/openu5/inventory_picker.h`, `native/core/src/inventory_picker.cpp`) - called by **both** `AlphaRuntime::open_selection()` and the host Group B test, so device and host cannot diverge. Its input was widened from "the six flags the old code happened to read" to the real possession gates. Row order follows the reference's `buildUseRows()` (`game/src/core/usePicker.ts`).

Canonical id -> authoritative possession owner -> display name:

| id (hex) | Item | Possession owner | Display name |
|---|---|---|---|
| 16 (0x10) | Magic Carpet | `GameState::magic_carpets` | Magic Carpet |
| 17 (0x11) | Skull Key | `GameState::skull_keys` | Skull Key |
| 18 (0x12) | Amulet of Lord British | `game.quest.artifacts[0]` | Amulet of Lord British |
| 19 (0x13) | Crown of Lord British | `game.quest.artifacts[1]` | Crown of Lord British |
| 20 (0x14) | Sceptre of Lord British | `game.quest.artifacts[2]` | Sceptre of Lord British |
| 21-28 (0x15-0x1c) | Moonstones, phase = id - 21 | `QuestWorldServices::moonstones[phase].buried == false` | Moonstone (generic) |
| 29 (0x1d) | Shard of Falsehood | `game.quest.shards[0]` | Shard of Falsehood |
| 30 (0x1e) | Shard of Hatred | `game.quest.shards[1]` | Shard of Hatred |
| 31 (0x1f) | Shard of Cowardice | `game.quest.shards[2]` | Shard of Cowardice |
| 32 (0x20) | Spyglass | `GameState::spyglass` | Spyglass |
| 33 (0x21) | HMS Cape plans | `GameState::hms_cape` | HMS Cape Plans |
| 34 (0x22) | Sextant | `GameState::sextant` | Sextant |
| **35 (0x23)** | **Pocket Watch** | **none - intentionally excluded** | **none (null)** |
| 36 (0x24) | Black Badge | `GameState::black_badge` | Black Badge |
| 37 (0x25) | Wooden Box | `GameState::wooden_box` | Wooden Box |

`kUsableItemPickerMaxRows` was raised from 6 to **21** - the exact size of the full owned set - so no valid row is ever truncated.

Notes on the three gates that needed adjudication:

- **Moonstones (21-28)** have no scalar `GameState` field. Possession is `QuestWorldServices::moonstones[phase].buried == false` - "owned" means "carried, not buried" - which is precisely how the reference gates the same rows (`buildUseRows()` pushes one row per `!m.buried` moonstone) and how `use_moonstone()` already reads them. The picker reads **current** ownership only. **Moonstone persistence across save/load remains R-14 and is untouched by this batch.**
- **HMS Cape plans (33)** is gated on `game.hms_cape`. That bit *is* possession: `quest_world.cpp`'s `apply_search_grant(id==4, quality==255)` sets it on (G)et, and the reference's `useHmsCape()` states its own Use-time write is a no-op because the pickup already wrote `0xFF`. There is no separate "rigged" ownership flag in either implementation.
- **Black Badge (36)** is gated on `game.black_badge`, which is possession. `time_spell` is the separate Use-*time* effect and is not an ownership gate.

**Pocket Watch (id 35) is still deliberately excluded.** No field in `GameState`, `QuestState` or `QuestWorldServices` backs it - re-confirmed this batch across `state.h`, `debug_developer.cpp` and `save_core.cpp` - so no row can be gated on possessing it, and its display-name slot stays null so an ungated row can never render. Inventing a backing field was explicitly out of scope. **Open follow-up.**

**Regression coverage:** `native/core/tests/batch3_group_b_test.cpp` - B1 real-id names, B1 Pocket-Watch characterization, B2 id 18 != Grapple (and no Grapple row at all), B3 full-set completeness including moonstones, B4 picker row id == the id later dispatched as `CommandKind::UseItem`, B5 no unresolved/null names for any visible row, B6 Grapple stays Klimb-only.

---

### R-09 — Five modal responses are silently discarded · **SEVERITY 2 (understated — see below)** · **GREEN — RESOLVED (Batch 4)**

`UiSession::consume` opens these modals; `UiSession::finish_modal` does **not** convert them to commands; and `AlphaRuntime::modal()` has **no branch** for them. The player answers, and the answer evaporates.

| Event | Modal opened | Missing dispatch |
|---|---|---|
| `BlackthornPrompt` | `begin_text(Blackthorn)` | `CommandKind::BlackthornAction` (implemented, `commands.cpp:627`) |
| `GuardPasswordPrompt` | `begin_text(GuardPassword)` | guard password verification |
| `GuardTributePrompt` | `begin_yes_no(GuardTribute)` | tribute payment |
| `GuardArrestPrompt` | `begin_yes_no(GuardArrest)` | arrest submission |
| `FountainDrinkPrompt` | `begin_yes_no(FountainDrink)` | fountain drink effect |

**Correction (Batch 4 adjudication, `ALPHA20_BATCH4_ADJUDICATION.md`):** the "near-miss" claim below was **wrong on two of its four counts**, and the severity was **understated**. `commands.cpp:628` gates *every* command with `AwaitingResponse` while any `BlackthornSession` flag (`shrine>=0 || password || tribute || arrest`) is live; since nothing ever dispatched a `BlackthornAction`, the four Blackthorn/guard flows were not merely "the answer evaporates" but a **hard, permanent softlock** — one bad answer near a guard ended the playthrough. `FountainDrink` is not part of the same family at all: the reference (`main.ts:2668`) is `pickMember("Who will drink?")`, pure flavour text with no HP/state/turn/command, not a yes/no prompt. Of the family this section held up as the correct pattern to copy, only `TrollToll` and `WellWish` were actually correct; `CrystalBall` dispatches with `member == -1` and is **always** `Rejected` (`look.cpp:39`), and `WellDrop`'s "No" answer dispatches nothing and its ESC does not mean No. Both are tracked below as R-25/R-26, discovered but explicitly out of scope for Batch 4.

#### Resolution (Batch 4)

Split by actual root cause, per the adjudication's ruling (§D):

- **Blackthorn / GuardPassword / GuardTribute / GuardArrest → `UiSession::finish_modal`.** Each is a pure `(UiRequestId, UiModalValue) -> Command{BlackthornAction, item=action, member=agree, text=...}` map — the same shape as the existing `TownExit`/`RestHours`/`YellText` arms — because all persistent state (`BlackthornSession`) is already core-owned; no Blackthorn state moved into `UiSession`. **Cancel semantics matter**: the reference's `askText.cancel -> onText("")` means cancelling the two text prompts is **not** dismissal — it is a failing answer that still advances/clears the machine. `UiSession::cancel_modal()` now routes `Blackthorn`/`GuardPassword` through the same `BlackthornAction` translation with an empty string instead of the bare `accepted=false` `ModalResponse` the dispatcher used to drop; `GuardTribute`/`GuardArrest` are correctly left non-cancellable (`cancel_means_no=false`, unchanged), matching the reference's `type:"yesno"`.
- **FountainDrink → split, mirroring `main.ts`.** `UiSession::consume` no longer opens a `YesNo "Drink?"` modal; it prints `"a gurgling fountain!"` and raises `UiIntentKind::OpenPartySelection` with the existing `UiRequestId::FountainDrink` (no new request id needed), prompt `"Who will drink?"`. `AlphaRuntime::modal()` resolves the picked member's `characters[m].status` and prints the flavour line via a new tiny, pure, ESP-free helper `openu5::fountain_drink_result(char status, bool cancelled)` (`native/core/include/openu5/look.h` / `src/look.cpp`): `'D'`/`'S'` → `"Incapacitated!"`, otherwise `"Refreshing..."`, cancel → `"None!"`. No `CommandKind`, no HP/party mutation, no turn — exactly the reference's pure-flavour contract.
- **Chained-modal ownership** (e.g. Tribute refusal chaining into the Arrest prompt) needed no new mode-stack: `finish_modal` already restores `mode_ = return_mode_` *before* dispatching, so a modal the dispatched command re-arms captures the correct (Exploration) return register — the existing Batch 1 mechanism, unchanged.
- **Presentation fidelity** (in scope, narrowly): the guard-arrest prompt now announces `"Thou art under arrest!"` before asking `"Wilt thou come quietly?"` (was the abbreviated `"Go quietly?"` with no announcement), matching `main.ts:2553`. The Blackthorn interrogation's `"\n\nYour response?"` suffix (`main.ts:2706`) was **initially added to the core event text in `blackthorn.cpp`, which broke `quest_parity`** (mismatch 4821: the reference's world-flow fixture compares the raw event text, and that suffix is the reference's own `askText` widget composing its display, not part of the semantic event) — corrected to compose the suffix in `UiSession`'s own prompt text only, presentation-side, leaving the core event untouched. Tribute prompt wording/amount (`e.note`) was left unchanged: no exact reference string is evidenced in the adjudication, and inventing one would be an unverified guess, not a fidelity fix — left for follow-up.

**Regression coverage:** `native/core/tests/batch4_group_a_test.cpp` (A1–A10) drives the real town-turn trigger → real `UiSession` modal machinery → real `execute_command()` for all four Blackthorn/guard flows (gold debit, arrest-jail mutation, alarm escalation, password pass/cancel-as-empty) and the fountain party-picker shape; A10 is the new pure-helper coverage.

**Two RED assertions were found to be test-authoring defects, independently re-verified, and corrected (not weakened — a harness-correction pass, `native/core/tests/batch4_group_a_test.cpp` / `batch4_group_b_test.cpp`):**
- **A8** (originally `blackthorn_machine_never_softlocks_the_world`) armed a `BlackthornSession` flag directly and called `Pass` **without ever dispatching the resolving `BlackthornAction`**, then asserted the result was not `AwaitingResponse`. That assertion was the `commands.cpp:628` gate working *correctly* (the world should stay blocked while a prompt is genuinely unresolved), not the real Batch 4 defect. **Corrected** (renamed `machine_never_softlocks_after_real_resolution`) to drive the real trigger and real modal answer for all four families, THEN issue `Pass` and assert it is not `AwaitingResponse`: GuardTribute/GuardArrest/GuardPassword resolve in one real round trip (reusing A1/A3/A5's setups); Blackthorn's multi-round interrogation needed a minimal, content-free `ShrineServices` (a fixed placeholder string per `record()` index — no MISCMSG/virtue/mantra content, matching the established host-suite convention of not carrying that content, `host_tests/ui_mode_test.cpp`'s SHRINE-1/2 note) plus `living=1` so the real `blackthorn_action()` state machine concludes (`s.shrine=-1`) on the first answer instead of *correctly* advancing to another still-open round. All 4 sub-cases now pass.
- **B5**'s third check asserted `!dialogue_session.active` after routing the real `NpcInitiatesTalk` event through the identical `UiSession::consume()` path B1 requires to reach `BeginConversation` and activate the session — both B1's default `ctx.events = ui.event_sink()` and B5's own tap (its comment said it forwards "exactly like `ui.event_sink()` would") call the same production `UiSession::consume()`, which (per this section's resolution above) dispatches `BeginConversation` immediately and unconditionally for that event. Independently re-derived and empirically re-confirmed (B1 green, that check red) before correcting. The deeper cause is a genuine architectural-layer mismatch: production never lets `UiSession` see `NpcInitiatesTalk`/`NpcInitiatesShop` at all — `AlphaRuntime::consume_event()` intercepts both *before* forwarding to `UiSession`, and its pending register is private `AlphaRuntime` state (ESP-only, no extracted host-testable seam, and none was invented solely for this test, per instruction). **Corrected**: B5 no longer forwards to `UiSession` at all (that path is already proven end-to-end by B1–B4); its tap only isolates identity capture/survival (unchanged, already-sound checks 1–2), and its third check now closes the loop honestly — the re-resolved `(slot, location)` identity, dispatched as `BeginConversation{member=slot}` exactly as the deferred drain would use it, opens a real dialogue session naming the same actor.

**Evidence:** [STATIC] + [EXEC] (`batch4_group_a_tests` 32/32, `batch4_group_b_tests` 12/12, `quest_parity`).

---

### R-10 — NPC-initiated Talk and Shop never open · **SEVERITY 2** · **GREEN — RESOLVED (Batch 4)**

`blackthorn.cpp:62/65` emits `GameEventKind::NpcInitiatesShop` / `NpcInitiatesTalk` with `e.npc` set, as a **request** for the frontend to start the session. Neither `UiSession::consume` nor `AlphaRuntime::consume_event` has a case for either. Consequence: `CommandKind::BeginConversation` (which takes `member = NPC slot`) is unreachable, and guards/merchants who hail the party do nothing.

#### Resolution (Batch 4)

The fix is smaller than the original audit assumed: `CommandKind::BeginConversation` already matches an NPC by `schedule.slot` alone (`dialogue_orchestration.cpp:132`, no adjacency/floor test) and already routes `dialog 0x81..0x88` into `begin_shop()` via `DialogueHandoff::Shop` (`:139`) — Talk and Shop initiation are the **same** entry point; no `BeginShop` command or second shop-construction path was invented.

`UiSession::consume` now has a case for both `NpcInitiatesTalk` and `NpcInitiatesShop`: it copies only `e.npc->schedule.slot` (the borrowed `NpcActor*` is never retained) and dispatches `BeginConversation{member=slot}`, exactly mirroring `finish_modal`'s existing `command()` calls. `native/core/tests/batch4_group_b_test.cpp` (B1–B4) drives this end-to-end through the real town turn and confirms identical `ShopPhase`/type/NPC-identity/mode-ownership between NPC-initiated and ordinary player-triggered `Talk` for the same NPC (B4), and correct base-mode Dialogue/Exploration round-tripping (B2, reusing Batch 1's R-18 register unchanged).

**Production re-entrancy (device layer only).** These events fire from inside `blackthorn_turn_effect`, itself reached from the still-live outer town-turn command. Immediately dispatching `BeginConversation` from `AlphaRuntime::consume_event()` would re-enter `AlphaRuntime::command()` — nesting its pre/post instrumentation and `finish_combat_if_needed()`/`schedule_combat()` inside itself. The core tolerates the nested call (measured), but it is not the intended architecture, so `AlphaRuntime::consume_event()` now intercepts `NpcInitiatesTalk`/`NpcInitiatesShop` **before** forwarding to `UiSession::consume()` (which would otherwise dispatch immediately, since it has no re-entrancy concern of its own — see `UiSession`'s own header contract, unchanged, "presentation state only" for every other path): it copies `(schedule.slot, location)` into a small pending register and returns without touching `UiSession`. `AlphaRuntime::handle()` drains it once the outer input has fully unwound, beside the existing `synchronize_after_debug(...)` call — the same shape as the pre-existing `gem_view_active_` deferral. The drain re-resolves the NPC by `(slot, location)` (never the stale pointer), rejects a stale/relocated capture, requires `UiSession` to be back in plain Exploration, and — since the emitter's dialog window (`0x80..0xFC`) is wider than the shop handoff's actual support (`0x81..0x88`) — silently drops an out-of-range `NpcInitiatesShop` rather than inventing a fallback shop or falling through to `"Funny, no response!"`.

**Evidence:** [STATIC] + [EXEC] (`batch4_group_b_tests` B1–B5).

---

### R-25 — `CrystalBall` dispatches with `member == -1` and is always rejected · **SEVERITY 2** · **GREEN — RESOLVED (Batch 19), production fix; hardware validation pending (H-50, H-141 – H-145)**

> **Batch 19 closed this.** The kernel `0x4988` picker now exists once, as `native/core/include/openu5/command_char.h`, and the crystal ball, `(S)earch` and `(C)ast` all resolve through it. Full write-up, reference derivation, RED counts and mutation table: §14 Batch 19. The Batch 18 text below is preserved because it is the derivation the fix was built from; its "deferred" verdict is superseded.

Reference `main.ts` uses `pickCommandChar` (a character picker) before viewing a crystal ball; native opens a `YesNo` and, on Yes, dispatches `CommandKind::CrystalBall` with the default, never-set `Command::member == -1`. `look.cpp:39` rejects that outright: **zero events, zero HP change, always `Rejected`**, regardless of player input. Discovered while adjudicating R-09 (the original audit cited this as a correctly-handled precedent to copy for the Blackthorn/guard fix; it is not). Left unfixed per Batch 4's explicit scope boundary. **Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (`modal()`, `CrystalBall` arm), `native/core/src/look.cpp:39`.

#### Batch 18 re-adjudication — confirmed live, and severity raised

Re-verified against the current tree, not the Batch 4 note. `look.cpp`'s guard is now `if(cmd.member<0||cmd.member>=g.party.character_count) return CommandStatus::Rejected;` and `alpha_runtime.cpp:888` still reads `else if(i.request==...::CrystalBall&&i.value.yes){c.kind=...::CrystalBall;command(c);}` with `c.member` untouched. **The crystal ball is 100 % non-functional on device**, which is a severity-2 dead feature, not "unassessed". Two separate divergences sit on top of each other:

1. the **prompt is fabricated** — `ui_session.cpp:1201` raises `begin_yes_no(CrystalBall,"Peer into it?")`, and `LOOKOBJ cmd_look 0x09ea`'s case `0x29` raises **no yes/no at all**: it calls the command character picker (`call 0xffffa6f8` → kernel `0x4988`) directly, and only then rolls the die at `0x09f6`;
2. the **member is never chosen**, so the already-correct core arm can never run.

#### Why Batch 18 did NOT fix it

The faithful fix is not "pass `active_member(game_)`". `pickCommandChar` (`game/src/ui/pickers.ts:92`, annotated instruction-by-instruction against kernel `0x4988`) is a four-branch resolver: active character valid → use it directly (`@0x49b2`); exactly one eligible `'G'`/`'P'` → auto-select (`@0x49fa`); zero eligible → `"None!"` (DS `0xa3da`, printed by the common epilogue at `@0x4a5f`); two or more → the `"Player: "` prompt (DS `0xa3c4`) with the chosen name echoed on the same row, a `"Disabled!"` re-ask loop for a member whose status byte is neither `'G'` nor `'P'` (`@0x4a4e`), and `"None!"` on cancel.

Native implements **none** of it. `search_world()` takes a `searcher` parameter and every caller passes `-1`, falling back to "active, else member 0" (`quest_search.cpp:67`); `pickCaster` delegates to the same kernel routine in the reference, so `(C)ast` owes it too. Building it correctly means a new shared core seam plus a new `UiRequestId` plus re-pointing `(S)earch` and `(C)ast` at it — **three systems, architectural, and it changes the perceiver of every chest trap check**. That is squarely inside Batch 18's "defer instead" list, and a partial implementation would be a guess dressed as a fix.

**Scoped follow-up (Batch 19 candidate, precisely bounded):**
1. Add `openu5::resolve_command_char()` to the core as an ESP-free seam reproducing kernel `0x4988`'s four branches and its three strings, with the `"Disabled!"` re-ask loop.
2. Add `UiRequestId::CommandChar` and route `CrystalBallPrompt` through it instead of `begin_yes_no`, deleting the fabricated `"Peer into it?"`.
3. Re-point `search_world()`'s `searcher` and `cast_selected_spell()`'s caster at the same seam; `gameplay_parity` is the regression oracle for the trap-check perceiver change.
4. Device row H-50 in the consolidated checklist already records the current broken behaviour so the fix has an observation to flip.

---

### R-26 — `WellDrop` "No" dispatches nothing and ESC does not mean No · **SEVERITY 2** · **GREEN — RESOLVED (Batch 18), production fix**

Reference `dropCoin(false)` (the "No" answer) prints `"No\n"`; native's `WellDrop` "No" arm dispatches nothing at all — it only "works" on Yes because `Command::member == -1` happens to be truthy in `look.cpp:37`. The reference also treats ESC as No (`yesno-esc`); native passes `cancel_means_no=false`, so ESC is ignored instead. Discovered while adjudicating R-09 (same reason as R-25 — cited as a correct precedent, only half true). Left unfixed per Batch 4's explicit scope boundary. **Files:** `native/targets/tdeck/main/alpha_runtime.cpp:645`, `native/core/src/ui_session.cpp` (`WellDropPrompt` case).

#### Fix (Batch 18) — RESOLVED, and two adjacent fabrications closed with it

Adjudicated against `LOOKOBJ.OVL` via `game/src` before any edit. The **core was always correct**: `look.cpp:37` is `say(cmd.member?"Yes\n":"No\n"); if(cmd.member&&g.gold>0) emit(WellWishPrompt);`, byte-for-byte `game.ts::dropCoin()` — the Yes echo precedes the gold check (`0x006e` before `0x0075`), and a Yes with no gold returns **silently** (there is no `"Thou hast no coin!"` in the binary). `gameplay_parity` drives that arm through its `coin` op and has always passed it. Everything below is the **glue** that decides what the core is handed, which no host test in the project reached until this batch.

Four separate divergences, all confirmed RED first (see §14 Batch 18 for the run):

1. **The "No" answer dispatched nothing.** `alpha_runtime.cpp`'s arm was gated on `&&i.value.yes`. Now it dispatches on both answers and carries the answer in `Command::member` (`c.member=i.value.yes?1:0`) — the exact shape the `TrollToll` arm one line above already uses. The Yes path is semantically unchanged (`-1` and `1` are both truthy in the core's test).
2. **ESC was ignored.** The reference prompt type is `yesno-esc` (`main.ts` `well-drop-prompt`). `begin_yes_no(...,false)` → `begin_yes_no(...,true)`.
3. **The object description was missing.** `LOOKOBJ 0x0048` special-cases the well *before* the generic "Thou dost see" and prints description and prompt in one print — `hud.message("a well.\n\nDrop a coin?")`, DATA.OVL DS `0x720c`. Native raised only the prompt. Added `append(UiTextChannel::Message,"a well.")`, the identical shape the `FountainDrinkPrompt` arm four lines below already uses for DS `0x729c`.
4. **The wish row's prompt was fabricated.** Native printed `"What dost thou wish?"`; the literal is DS `0x722c` = `"\nThy wish?"` (`WELL_UI.wish`, printed by `LOOKOBJ 0x007f`, with the reference's own note that it deliberately carries **no** `:` cursor). The 12-character limit was already right.

Fixing (4) exposed **R-34** below, which had to be fixed for the corrected prompt to be visible at all.

**Tests:** new `batch18_well_ceremony` (19 checks) drives the whole ceremony through production routing — real `RawInputEvent`s into the real `UiInputAdapter`, the real `UiSession`, and the real, unmodified `alpha_runtime.cpp`. **RED 16/5 → GREEN 19/0.** Four mutations, each caught and reverted. **Files:** `native/core/src/ui_session.cpp`, `native/targets/tdeck/main/alpha_runtime.cpp`. **Hardware:** checklist rows H-17 – H-22.

---

### R-34 — `finish_modal()` wipes the prompt of any modal its own dispatch armed · **SEVERITY 2** · **GREEN — RESOLVED (Batch 18), production fix**

Found while fixing R-26 item (4): the corrected `"Thy wish?"` string was in the source and the wish row was open in `UiMode::TextEntry`, but `UiSession::prompt()` returned the **empty string**.

**Root cause.** `UiSession::finish_modal()` ends with
```
    dispatch(i);
    input_[0] = 0; input_length_ = 0; prompt_[0] = 0;
```
The teardown runs **after** the dispatch, and an answer is allowed to arm the next modal *synchronously inside* that dispatch — every one of them reaching `enter_modal()`, which sets `mode_` **and** `prompt_`. The unconditional clear then wiped the new prompt while leaving the new mode, so the follow-up modal rendered with an empty prompt row. `cancel_modal()` does not have the bug: it clears **before** dispatching.

**Blast radius — this was never well-specific.** Every chained modal in the project was affected, and the chain runs through `AlphaRuntime::modal()` as often as through `UiSession::consume()`:

| Chain | Follow-up prompt that was blank |
|---|---|
| Well: Yes → `WellWishPrompt` | `Thy wish?` |
| `(U)se` picker → Rel Hur scroll / skull key | `Direction?` (`begin_target`, `alpha_runtime.cpp` Inventory arm) |
| `(U)se` picker → potion/consumable | `On whom?` party picker |
| Shrine: `Visit?` → | `Virtue?`, then `Mantra?` |
| Blackthorn interrogation: each answer re-emits the prompt | `Your response?` — the very row Batch 4 added for prompt fidelity |
| `(R)eady`: member picked → | the equipment picker's prompt |

**Fix.** One guard, keyed on the fact that `enter_modal()` only ever records a **non-modal** `return_mode_` — so after `mode_ = return_mode_` runs earlier in `finish_modal()`, `is_modal(mode_)` is an exact test for "the dispatch armed something new":
```
    if (!is_modal(mode_)) { input_[0] = 0; input_length_ = 0; prompt_[0] = 0; }
```

**Tests:** `batch18_well_ceremony` group W6 pins the invariant through a **second, independent** chain (the Rel Hur `Direction?` prompt) so no future change can fix the well without fixing the mechanism. **RED** against unmodified code (W4-3 and W6-3 both failing), **GREEN** after. Mutation M1 (clear unconditionally again) turns exactly those two RED. Full suite 86/86 confirms no chain relied on the old clearing. **File:** `native/core/src/ui_session.cpp`. **Hardware:** checklist row H-23.

---

### R-11 — World spells that need a direction consume charge and MP, then do nothing · **SEVERITY 2** · **GREEN — RESOLVED (Batch 5), hardware-certified**

`AlphaRuntime::cast_selected_spell` only opens a target/direction prompt when `context_.combat` is true:
```cpp
if (context_.combat && (strstr(target,"MapPosition") || strstr(target,"MapUnit") || strcmp(target,"direction")==0))
```
Outside combat it falls through to `command(c)` with `has_direction=false` and no target. `cast_spell()` has already decremented `spell_quantities` and MP by then (`magic.cpp:76-79`).

| Spell | `TimePermitted` | Target | World result today |
|---|---|---|---|
| An Sanct (6) | anytime | selectedMapUnit | `"Cancelled."`, charge+MP gone |
| An Ex Por (25) | anytime | selectedMapPosition | `"Cancelled."`, charge+MP gone |
| In Por (17) | anytime | direction | **silent**, charge+MP gone |
| An Ylem (5) | anytime | selectedMapUnit | **silent**, charge+MP gone |
| An Grav (18) | anytime | selectedMapUnit | **silent**, charge+MP gone |
| In Ex Por (26) | anytime | SelectedMapPosition | **silent**, charge+MP gone |

`world_magic.cpp` is written to handle these correctly *given a direction* (`if (!cmd.has_direction) { say("Cancelled."); return {}; }` at the Seal/Disarm branch, `if (fx==Blink && cmd.has_direction)` at the Blink branch).

**Fix shape:** drop the `context_.combat &&` guard and use `UiSession::begin_target` with a direction request in the world, mirroring `direction_request()`.

**Evidence:** [STATIC] + table dump.

#### Adjudication (Batch 5) — the table above is wrong about three of its six rows

Only **three** world casts take a direction. The reference's world dispatcher
(`game/src/main.ts`, `doCast`) arms a getdir for exactly three effect
descriptors — `sealDoor` (An Ex Por 25), `disarmOrOpen` (An Sanct 6) and
`blink` (In Por 17) — and falls off the end of its `else if` chain for
everything else. `world_magic.cpp` agrees independently: its only pre-flight
target guards name items **6, 25 and 17** (`world_magic.cpp:43-44`).

| Spell | Effect (`kEffects`) | Reference `doCast` branch | Batch 5 verdict |
|---|---|---|---|
| An Sanct (6) | `Disarm` | `disarmOrOpen` → getdir | **R-11, fixed** |
| In Por (17) | `Blink` | `blink` → getdir | **R-11, fixed** |
| An Ex Por (25) | `Seal` | `sealDoor` → getdir | **R-11, fixed** |
| An Ylem (5) | `Poof` | *no branch* | **Not R-11.** Consumes and does nothing, silently, in the reference too. No prompt is owed. |
| An Grav (18) | `Dispel` | *no branch* outside the dungeon | **Not R-11.** Its world-map dispel is unimplemented in the reference port as well; a getdir here would be fabricated. |
| In Ex Por (26) | `Animation` | *no branch* | **Not R-11.** The original binary's #26 is getdir + animation only (it stopped opening doors; skull doors are `(U)se Skull Key`). The reference port implements neither the getdir nor the animation. The missing animation is R-12/Batch 7 territory, not targeting. |

Adding a prompt for An Ylem / An Grav / In Ex Por would **fabricate** UX the
original does not have, so Batch 5 deliberately does not. That is why the fix
keys on the **effect kind**, not on `target_type` — `target_type` over-selects
here, and R-16 already shows it is unreliable metadata.

**Charge ordering — the audit's Batch 5 "Care" note had it backwards.** The
plan said "cancelling must not consume". The reference consumes **first**:
`castSpell()` runs, decrements the charge and the mana and fires the ceremony,
and *only then* is `pendingCastDoor` / `pendingCastUnlock` / `pendingCastBlink`
armed and the getdir entered (`main.ts`, `doCast` tail). Cancelling the getdir
prints `"Cancelled."` (seal/unlock) or nothing at all (blink) and **refunds
nothing**. That is bug-for-bug original behaviour and the fix preserves it: the
existing `UiSession` cancel arm already dispatches the Cast with
`has_direction=false`, which lands on `world_magic.cpp:55`
(`if(!cmd.has_direction){say("Cancelled.");return {};}`) — charge gone, terrain
untouched — exactly matching the reference. No refund logic was added.

#### Root cause (Batch 5) — two defects, both required

1. **`AlphaRuntime::cast_selected_spell`** gated the prompt on
   `context_.combat`, so a world cast never opened one and dispatched straight
   through with `has_direction=false`.
2. **`UiSession::handle_modal`** treated *every* `CommandKind::Cast` sitting in
   `TargetSelection` as the combat aim **reticle**: a direction press walked
   `combat_x`/`combat_y` and dispatched nothing, and Confirm dispatched with
   `has_target` — never `has_direction`. `world_magic()` reads `has_direction`
   **only**, so removing the `context_.combat &&` guard alone would still have
   produced a no-op. Fixing (1) without (2) would have looked like a fix and
   changed nothing on hardware.

#### Resolution (Batch 5)

- **New seam `openu5::cast_target_prompt(SpellId, in_combat, in_dungeon)`**
  (`native/core/include/openu5/magic.h`, `native/core/src/magic.cpp`) returns
  `CombatReticle` / `WorldDirection` / `None`. Combat keeps today's predicate
  byte for byte. The world arm selects on `kEffects[id].kind ∈ {Seal, Disarm,
  Blink}`. The dungeon arm returns `None`: the reference's `doDungeonCast`
  resolves An Sanct against the party's dungeon **facing**
  (`applyAnSanctOpenChest`), has no seal or blink branch, and the dungeon
  command path never reaches `world_magic`. Extracted rather than left inline
  because `cast_selected_spell` is behind ESP-IDF headers (Y-05) — the Batch 3
  `usable_item_picker_rows` precedent.
- **`AlphaRuntime::cast_selected_spell`** now switches on that seam and, for
  `WorldDirection`, opens the ordinary world getdir
  (`begin_target(UiRequestId::Direction, "Direction?", c, -1, -1)`) — the same
  call shape Talk/Open/Push/Klimb and the Rel Hur scroll already use.
- **`UiSession::handle_modal`** now gates the Cast reticle (both the Direction
  and the Confirm arms) on `request_ == UiRequestId::Target`, which is what
  combat uses. A Cast on `UiRequestId::Direction` therefore takes the shared
  world-getdir path: one press dispatches with `has_direction`, echoing the
  direction like every other world getdir. Combat is bit-identical.

**RED → GREEN:** `native/core/tests/batch5_test.cpp`, registered as ctest
`batch5`. Pre-fix **30/34** — the four failures were A1 ×3 (the three world
spells answered `None`) and B1 (a direction at the world getdir dispatched
nothing). Post-fix **34/34**. The guards that were green before and after pin
what must *not* change: A2 (no fabricated prompt for An Ylem / An Grav / In Ex
Por), A3 (combat reticle selection unchanged), A4 (no dungeon getdir), B2
(combat aim walk + Confirm unchanged), B3/C3/C5 (a cancelled world cast still
spends charge and mana — the reference ordering).

**Full suite after the fix:** 69 tests, 68 pass, 1 fail — `gameplay_parity`
mismatch **2034** / R-21, the sole known expected failure, untouched by this
batch.

**Hardware checkpoint: GREEN.** An Sanct on a locked door, In Por blink in
world gameplay, and An Ex Por on a door were all run on the physical T-Deck
and **passed**, each on the first test attempt. Batch 5 is now fully
certified in software and on physical hardware. World spell targeting
presents a directional prompt (getdir), not a combat-style target reticle —
this matches the reference and is not a defect.

**Known divergence recorded, not fixed (Y-30):** `world_magic()` fuses
"consume the spell" and "apply the direction" into one call, so the native
prompt necessarily comes *before* consumption while the reference consumes
first. Net state is identical for both the success and the cancel path (the
cancel still dispatches), but the *message/ceremony ordering* differs, and a
cast that would fail its gate (`"Not here!"`, `"None mixed!"`, `"M.P. too
low!"`, `"Absorbed!"`) raises a spurious getdir first where the reference
prints the failure immediately and never prompts. Closing that needs
`world_magic` split into cast + apply — a core API change outside Batch 5.

---

### R-12 — `MapReveal` has no renderer · **SEVERITY 3** · **GREEN — RESOLVED (Batch 7)**

Emitted by `world_magic.cpp` for `MagicEffect::DeathVision` (Wis An Ylem) and by the In Quas Wis scroll, with `e.note = 20` animation frames. Unconsumed by both `UiSession` and `AlphaRuntime`. The spell succeeds, prints its message, charges a turn, and reveals nothing.

#### Resolution (Batch 7)

**Adjudicated against the reference first** (`game/src/skin/coreview.ts`'s `revealViewport`/the `visibleWindow` bypass, `game/src/main.ts`'s `runMapReveal`/`cancelMapReveal`), not assumed from the audit wording. The reference comment for `revealViewport` explicitly rejects the naive shape ("no es `computeVisibleWindow(∞)`: el flood no cruza muros y dejaría a oscuras los recintos sellados que el original SÍ enseña") -- a flood re-run with a higher light value still stops at walls, so the fix is not "raise the light level," it is "skip the censorship bitmap entirely for the window." The effect is real-time (`Date.now()+ms`), not turn-gated, and is world-only (the reference's own guard: `if (game.combat || game.dungeonState) return null`).

**Implemented:** `compose_world_presentation` (`native/core/include/openu5/presentation.h`, `native/core/src/presentation.cpp`) gained a `reveal_all` parameter (defaulted `false`, so every existing call site is unaffected); `visibility()` now fills every cell of the window `1` and returns immediately when it is set, instead of running the light-radius/sight-blocking flood. This is a pure, host-testable function -- it never touches `GameState`/`TurnState`, so the wall-clock timer stays where the reference keeps it: the caller. `AlphaRuntime` (`native/targets/tdeck/main/alpha_runtime.{h,cpp}`) arms `map_reveal_end_us_` on `GameEventKind::MapReveal` (`e.note * PAUSE_UNIT_MS` = 20*55 = 1100ms, mirroring `DEATH_VISION_FRAMES*PAUSE_UNIT_MS` exactly), swallows input while it is armed (`"revealing traga el input"`), and passes the live `map_reveal_active` flag into `compose_world_presentation` every render; the view "re-censors itself on expiry" on the following real render, with no explicit close command, matching the reference precisely.

**Tests:** `native/core/tests/presentation_test.cpp` (ctest `presentation_regression`) -- a new case forces night (`hour=0`, the same fixture the pre-existing "night visibility/fog censors distant cells" case already uses) and confirms `reveal_all=true` shows the real terrain at a cell the ordinary night flood hides, that every cell of the window is marked visible, and that the very next ordinary call (without the flag) censors again -- no state leaks between calls. Confirmed **RED** (compile failure -- the `reveal_all` parameter and the assertions using it did not exist yet) by reverting only `presentation.h`/`presentation.cpp` via `git stash` while keeping the new test, then **GREEN** after restoring the fix.

**Not host-tested, by necessity:** the wall-clock timer and the input-swallow live in `AlphaRuntime`, which cannot be host-built (ESP-IDF headers -- the same gap Y-05 already documents). This mirrors the existing, already-untested precedent for `GemView`/`MagicCeremony`'s own device-side timing in the same file. Physical-device confirmation is outstanding (no hardware in this session).

### R-13 — `Zodiac` has no renderer · **SEVERITY 3** · **GREEN — RESOLVED (Batch 7)**

`Use Spyglass` is one of the six items the picker *does* offer. The core emits `GameEventKind::Zodiac`; nothing consumes it. The reference builds a `ZodiacView` (`game/src/core/world/zodiac-view.ts`).

#### Resolution (Batch 7)

**Adjudicated against the reference first**: `look.cpp`'s `emit_zodiac` already computes the full `ZodiacView` (80 background stars, 8 zodiac signs rotated by calendar day, the Shadowlord-city connecting line) -- correctly, and already exercised indirectly by the existing parity/drift harness (`gameplay_driver.cpp` reads `troll_sneak`/other event payloads the same way; no production change was needed on that side). The audit's own framing ("no renderer") is exactly right: the entire gap is presentation. `game/src/main.ts`'s dismissal comment settles the interaction contract precisely: "vista de zodíaco... modal cosmético que se cierra con CUALQUIER tecla... No cobra turno propio (el (U)se ya lo gestionó)" -- the identical shape as View Gem's any-key close, except it never charges a deferred turn (the `(U)se` command already spent it).

**Implemented:** `render_zodiac_view` (`native/targets/tdeck/main/native_renderer.{h,cpp}`) plots the 80 stars and 8 sign markers (with a connecting line only when `has_line` -- the one dimension of the zodiac that is not purely cosmetic, per the reference: it marks which city currently holds a Shadowlord) directly onto the 176x176 viewport; the star/sign coordinates `look.cpp` already emits are in the same viewport-relative pixel space as the reference's own map-window-relative `plot()` calls, so no rescaling is needed. `AlphaRuntime` copies the borrowed `ZodiacView` payload into a persistent member on `GameEventKind::Zodiac` (the payload is only valid for the synchronous emit, like every other `GameEvent` pointer field), gates it with `zodiac_view_active_` exactly like `gem_view_active_` (closes on any key, `shortcut==DeviceShortcut::None`), but -- matching the reference -- dispatches no command and charges no turn on close.

**Not host-tested, by necessity:** both the pixel-plotting renderer and the device-side flag/copy live in ESP-only files (`native_renderer.cpp` needs `esp_err.h`; `alpha_runtime.cpp` needs ESP-IDF headers), so no host seam exists for this specific fix -- the same already-documented gap as `render_world_gem_view`/`GemView` in the same files (Y-05). The core-side `ZodiacView` computation this renderer consumes was already correct and already covered indirectly; nothing there changed. Physical-device confirmation is outstanding (no hardware in this session).

---

### R-14 — World objects are never serialized, and never cleared on load · **SEVERITY 1** · **GREEN — RESOLVED (Batch 6)**

`AlphaRuntime::objects_` (`std::vector<QuestObject>`) holds combat-promoted chests, `(S)earch`-revealed items, shard spawns, spilled world loot, ships and props. Grep across `save_core.cpp`, `gameplay_save.cpp` and `alpha_save.cpp` finds **no reference to it**.

Two consequences:
1. **Loss:** everything the player uncovered or dropped is gone after a power cycle.
2. **Leak:** `synchronize_loaded_world()` (`alpha_runtime.cpp:1284`) clears `actors_`, `dungeon_` and `combat_` but **not** `objects_` — loading save B while save A's world objects are live leaves A's chests and quest items in B's world. (The new-journey path at `:1308` does clear it, correctly.)

**Fix shape:** add a `worldObjects` array to the sidecar (`gameplay_save.cpp` already owns `mapOverrides`/`openDoors`/`overworldEnemies` and is the natural home), plus an `objects_.clear()` in `synchronize_loaded_world`.

**Evidence:** [STATIC].

#### Resolution (Batch 6)

**Adjudicated against the reference first** (`game/src/core/state.ts` — the `WorldObject`/`worldObjects` doc; `game/src/core/saveNative.ts`'s `SaveSidecar`; `game/src/core/persistence.ts`'s `deserialize`), not assumed from the audit wording. Two things the reference settles that the original write-up didn't know yet:
- Interior objects (chest/prop/loot/plot **in a town/castle**) are already correctly handled: they're **regenerated/discarded per map-entry transition** (`hydrate_interior_objects`/`discard_interior_objects`, already wired to `load_small_map`/`exit_to_overworld`), not part of what a save captures. The reference states this explicitly (`state.ts:108-111`). Nothing here needed a production change.
- A **load is not a transition**. `deserialize` (`persistence.ts:788-810`) restores `worldObjects` **verbatim** with no re-hydration pass, defaulting to `[]` only when the field is absent (a save predating the field). So whatever was live at save time — chest mid-loot, an in-flight search reveal, a docked ship — round-trips exactly, interior or not. Whole-vector capture (not a filtered/derived subset) is therefore the *correct* shape, not an ephemeral-state overreach.

**Implemented:** `capture_world_objects`/`restore_world_objects` (`native/core/include/openu5/gameplay_save.h`, `native/core/src/gameplay_save.cpp`) capture/restore the entire `objects_` pool through the existing `QuestWorldServices` callback seam (`count`/`read`/`reserve`/`append`) — no new coupling to `AlphaRuntime` internals. `synchronize_loaded_world()` (`alpha_runtime.cpp`) now does `objects_.clear();` unconditionally before restoring, so a domain-invalid or absent sidecar field degrades to an empty pool (safe default) rather than leaving stale objects from a previous world live, and a validation failure never partially populates the pool (validate-then-commit, matching `restore_gameplay`'s existing contract). `AlphaSaveService::save()` (`alpha_save.cpp`) captures via the same `c.quest_world` seam already threaded through `CommandContext`.

**Backward compatibility:** an older save with no `worldObjects` key restores to an empty pool, not a crash (`restore_world_objects` returns `Error::None` for an absent field).

**Tests:** `native/core/tests/gameplay_integration_test.cpp` (ctest `gameplay_integration`) — round-trip of a chest + an Underworld plot item through capture/encode/parse/restore; a contaminated pool (simulating save B's objects still live) does not survive the clear-then-restore sequence; a domain-invalid entry (out-of-range tile) and an absent field both leave the pool empty rather than partially populated or crashing.

**Not fixed here, and not claimed:** moonstone *burial* state (the `buried`/`location` flag) was never part of `objects_` — it lives in core `GameState`/the native `.gam` window and was already correctly persisted before this batch (see the moonstone-persistence correction below).

---

### R-15 — `DungeonState` is not serialized · **SEVERITY 2** · **GREEN — RESOLVED (Batch 6)**

Only `dungeonRoomsCleared` persists. `DungeonState` (`active`, `pos.dungeon/floor/x/y/facing`, the 512-byte mutable cell copy, the 64-byte `revealed` bitmap, the wanderer) is absent. `synchronize_loaded_world` does `dungeon_ = {}`, so a save taken inside a dungeon reloads at the **surface return position** with the dungeon session gone — silently. Reveal progress and field/trap mutations are lost.

Whether U5 permits saving inside a dungeon at all is a reference question worth settling before fixing; if it does not, this becomes an explicit N/A plus a save-time refusal message.

#### Resolution (Batch 6)

**Adjudicated first, per the audit's own instruction to settle this before picking A or B.** The authentic DOS behavior (`re/notes/save-window-writer.md`, `re/notes/dungeon-map-buffers.md`, `re/notes/combat-dungeons.md`, cross-checked against `game/src/core/saveNative.ts:923-926`) is: **saving underground is permitted, unconditionally** — the original (Q)uit&Save handler is one raw `AH=0x40` dump of the live 4192-byte DGROUP window, with no location/mode check anywhere in that path. Because the DOS engine keeps a single shared position register for every context, that same dump naturally captures the true in-dungeon location/floor/x/y/facing and the dungeon map-reveal buffer, all inside the same window. **Option A (serialize and restore `DungeonState`)** is therefore the reference-correct shape, not Option B (refuse to save underground) — confirmed by an on-disk 15-file corpus sample showing live in-dungeon coordinates in captured `.GAM`s. (Ironically, the reference *TypeScript* engine doesn't model this fidelity either — `dungeonState` lives outside its serialized `GameState`, a self-declared "QoL, not fidelity" gap in `persistence.ts`/`savepanel.ts` — but the DOS behavior, not the TS port's shortcut, is what an alpha implementation should match, and Option A is the only shape consistent with it.)

**Implemented:** `capture_dungeon`/`restore_dungeon` (`gameplay_save.h`/`.cpp`) round-trip the full live session — `pos` (dungeon/floor/x/y/facing), `quickness_toggle`, the 512-byte `cells` copy, the 64-byte `revealed` bitmap, and the wanderer — through the JSON sidecar (not new native `.gam` byte offsets: the port's own `dungeon_`/game-position split, unlike DOS's single register, means the surface-return `game_.position` and the live dungeon session are already two different owners, and the sidecar is where every other owner not covered by fixed `.gam` offsets already lives). `AlphaSaveService::save()` captures from `c.dungeon_context->state` (already threaded through `CommandContext`, no new parameter). `synchronize_loaded_world()` restores into `dungeon_`, falling back to an inactive session (`dungeon_ = {}`) on a domain-invalid or absent document — the common case (a surface save) resolves this way by design, not as an error path.

**Memory-safety-motivated validation, not just domain hygiene:** `pos.floor`/`pos.x`/`pos.y` are validated to `[0,7]` and `facing` to `[0,3]` — tighter than their `uint8_t` storage width — because they are **unguarded array indices** downstream: `dungeon.cpp`'s `offset(f,x,y) = f*64+y*8+x` indexes the 512-byte cell grid with no bounds check, and `quest_world.cpp`'s facing-delta lookup indexes a 4-entry table the same way. A corrupt or hand-edited sidecar with an out-of-range value is rejected (`Error::NativeDomain`) rather than read out of bounds.

**Backward compatibility:** a save with no `dungeon` key (the overwhelming common case — any surface save) restores to an inactive session directly; this is not a degraded/error path.

**Tests:** `native/core/tests/gameplay_integration_test.cpp` (ctest `gameplay_integration`) — a full session (mutated cells, revealed bitmap, an active wanderer) round-trips exactly through capture/encode/parse/restore; an out-of-range `facing` (which would index the direction-delta table out of bounds) is rejected without mutating the live session, and the caller's own fallback (mirroring `synchronize_loaded_world`) clears it; an absent `dungeon` key resets a stale live session to inactive directly.

**Not fixed here:** the underlying `DungeonEncounters`/combat-arena wiring for a restored session is unchanged — this batch persists the *session*, it doesn't add new dungeon combat behavior.

**Correction (Batch 26, 2026-09-22).** This resolution never reached the SD card. `export_native()` (`persistence.cpp`) copies only the sidecar keys named in its `extras` list, and `"dungeon"` was not on it, so `capture_dungeon()`'s object was dropped on every save and every dungeon save loaded at the entrance on the surface — the H-115 hardware FAIL. The test above round-tripped the in-memory JSON document only, never `export_native_state`/`load_native_state`, so it could not see the drop. Two further corrections: `quickness_toggle` is not part of the 1988 save (DUNGEON `0x0E40` zeroes it on session entry), and a load must also re-derive the dungeon context and UI mode. See §"Batch 26".

---

### R-16 — Spell descriptions contradict spell effects · **SEVERITY 3**

`spell_target_label()` contradicts `SpellDef::target_type` for **12 of 48** spells [EXEC]:
```
id 5  An Ylem      selectedMapUnit      → label "Direction"
id 6  An Sanct     selectedMapUnit      → label "Direction"
id 7  An Xen Corp  selectedMapUnit      → label "Direction"
id 13 Vas Flam     selectedMapUnit      → label "Direction"
id 18 An Grav      selectedMapUnit      → label "Direction"
id 23 Wis Quas     selectedMapUnit      → label "Direction"
id 25 An Ex Por    selectedMapPosition  → label "Direction"
id 26 In Ex Por    SelectedMapPosition  → label "Direction"
id 28 In Zu        selectedMapUnit      → label "Direction"
id 34 An Xen Ex    selectedMapUnit      → label "Direction"
id 38 In Quas Xen  selectedMapUnit      → label "Direction"
id 47 An Tym       noSelection          → label "Direction"
```
`spell_effect_summary()` contradicts the authoritative `MagicDefinitions.json::SimpleDescription` for at least these [REF]:

| id | Spell | Native summary | Reference |
|---|---|---|---|
| 5 | An Ylem | "Teleports a map creature" | makes objects vanish |
| 9 | In Wis | "View a gem map" | reveals caster's location |
| 26 | In Ex Por | "Animates an object" | **unlocks magical locks** |
| 28 | In Zu | "Makes a creature invisible" | **puts enemies to sleep** |
| 31 | Quas An Wis | "Negates magic" | **charms multiple enemies** |
| 32 | In An | "Shows nearby deaths" | negates magic |
| 36 | Sanct Lor | "Protects a companion" | **invisibility** |
| 40 | In Nox Hur | "Fear attack" | **blasts foes w/poison** |
| 41 | In Quas Corp | "Puts creatures to sleep" | **causes fear** |
| 47 | An Tym | "Slows a creature" | **stops passage of time** |

Two of these are more than a label problem — **`kEffects[26] = Animation` for In Ex Por (should unlock) and `kEffects[28] = Line` for In Zu (should sleep)** look like genuine effect-table mis-assignments and need adjudication against `re/` before any edit.

`SpellId::Nox` (index 48) is a related hazard: `spell_definition()` accepts index 48 but `kEffects`, `kSummaries` and `kTargets` have only 48 entries (0–47). It is unreachable from the UI (`open_selection` loops `i<48`) but is an out-of-bounds read waiting for a future caller.

#### Resolution (Batch 8)

Adjudicated per-item against `MagicDefinitions.json`, `re/` disasm evidence, and the TypeScript reference (`game/src/core/magic/{cast,tables,spells}.ts`), in that priority order but with RE overriding the JSON wherever the two conflicted and RE had a byte-verified answer. **Nothing here changes `cast_spell()`'s tracked numeric fields** — confirmed by re-running `magic_parity` (25,088 cases, byte-identical before/after) — so this is a display/classification-metadata fix, not a gameplay-behavior fix, except where noted for In Ex Por below.

**`kTargets` (12 flagged) — 11 stale audit premise, 1 confirmed defect.** The audit's implicit assumption — the label should mirror `SpellDef::target_type` — is itself wrong for 11 of the 12: `target_type` is the unreliable field (already established by the Batch 5 comment this file cites at line ~742), and `"Direction"` correctly names the real input mechanism for An Ylem/An Sanct/An Xen Corp/Vas Flam/An Grav/Wis Quas/An Ex Por/In Ex Por/An Xen Ex/In Quas Xen (all combat-reticle spells, aimed with direction presses) and In Zu (a line spell that literally calls `getdir`, confirmed in `re/notes/fx-lineaoe-negate-derivation.md` §1: `1f87 push actor ; call getdir`). An Ex Por and An Sanct additionally raise a real *world* getdir (Batch 5). The one confirmed defect is **An Tym (#47)**: `target_type` is `noSelection` and its effect is a pure global time-stop (`TimeStatus,84,10`, no direction ever consumed — `re/notes/fx-lineaoe-negate-derivation.md` §2) — it should read `"World"`, exactly like its `noSelection`+`TimeStatus` siblings Quas An Wis and In An, not `"Direction"`. **Fixed.**

**`kSummaries` ("at least" 10 flagged) — 9 confirmed defects, 1 stale audit premise.** `kSummaries`/`kTargets` were hand-typed in commit `c18f5b64` without cross-checking `MagicDefinitions.json`, and nine entries describe a *different* spell's effect outright (not a paraphrase difference — a wrong one): An Ylem (5, "teleports" vs. its own `Poof`/vanish effect), In Wis (9, "gem map" vs. reveals location), In Zu (28, "invisible" vs. sleep — corroborated by RE: line-spell mode 1 is a saving throw + `0xffffa92e` "dormir"), Quas An Wis (31) and In An (32) (their summaries were swapped relative to their own `TIME_STATUS.confusion`/`.negate` effects in `tables.ts`), Sanct Lor (36, "protects" vs. `invisibilitySelf`), In Nox Hur (40) and In Quas Corp (41) (also swapped — mode 2 of the line-spell dispatch is confirmed poison, `massFear` is confirmed fear), and An Tym (47, "slows" vs. its actual global time-stop). All nine corrected to match `MagicDefinitions.json::SimpleDescription` (adapted to the table's existing sentence-case/verb-phrase style). **Fixed.**

In Ex Por (26) is the one flagged summary that is **not** a native defect, but the reasoning is not "target_type is unreliable" — it needed its own re-derivation, below.

**`kEffects[28]` (In Zu, "should sleep") — stale audit premise, not touched.** `re/notes/fx-lineaoe-negate-derivation.md` §3c ("Modo 1 In Zu") shows the line-spell dispatcher already applies a saving throw, a per-monster-type immunity check, and then `0xffffa92e` ("dormir"/sleep) to every cell the spray registers. `Line` is the correct top-level classification — it names the *delivery mechanism* (a getdir'd spray, shared with In Nox Hur/In Flam Hur/In Vas Grav Corp), not the per-hit status; `kEffects[28].value == 1` already encodes "mode 1 = sleep" and matches the reference 1:1 (`cast.ts` case 28: `{kind:"lineAoe",mode:1,len:2}`). Re-typing this to a bare "sleep" kind would *break* the shared line-spray classification for no gain — `kSummaries[28]` (fixed above, now "Puts enemies to sleep") already carries the correct player-facing meaning.

**`kEffects[26]` (In Ex Por, "should unlock") — confirmed defect, RE evidence reverses the earlier belief cited by this very file.** The original derivation this audit and `magic.cpp`'s own Batch-5 comment relied on ("getdir + animation, NADA MÁS — does not touch doors") was a mis-read of the callee and was **retracted 2026-08-07** in `re/notes/magic.md` (search "CORREGIDO 2026-08-07") after a second RE lane opened the callee `CAST2.OVL:0x0768 magic_door_open_worker` and found In Ex Por's world branch (`CAST:0x1026`) calls it directly — **the identical worker the Skull Key uses** (`CAST:0x18dd`, `re/notes/skullkey-alcanzabilidad.md`). `content-audit.md`'s own retraction states it plainly: *"el manual tenía razón y el port omite la mecánica"* (the manual was right and the port omits the mechanic). `MagicDefinitions.json::SimpleDescription = "unlocks magical locks"` for this spell was therefore correct all along, not stale.

Fixed at the classification level: added `MagicEffect::Unlock` (`magic.h`) and reclassified `kEffects[26]` from `Animation` to `Unlock` (`generate-magic-fixtures.ts`'s `effects[26]`, regenerating `magic_tables.inc` — this table is generator output, not hand-editable; see the generator's inline citation). `kSummaries[26]` corrected to "Unlocks magical locks" to match. **The tile mutation itself is deliberately NOT wired in this batch** — `world_magic.cpp` already has the analogous transform (its `Disarm` branch, used by An Sanct) and `commands.cpp`'s Skull Key handler has the exact 151/152→184/186 transform to copy, but doing so safely means: a new pre-flight mutable-terrain guard, confirming it does *not* fall through to An Sanct's unrelated trapped-chest fallback, and deciding In Ex Por's still-unmeasured combat-context behavior (its time-window mask is town+combat, `0x05`) — real engineering, not a metadata fix, and exactly the kind of thing `re/notes/content-audit.md`'s own retraction defers ("Se arregla APARTE"). `cast_target_prompt(InExPor, false, false)` therefore still returns `None` today, same as before; `batch5_test.cpp`'s A2 guard is updated to assert that with the corrected rationale instead of the old, now-disproven one, so a future fix has an honest test to flip. **Tracked as an open follow-up, not resolved by this batch** (see "remaining OPEN items" below).

**`SpellId::Nox` (index 48) "out-of-bounds read" — stale audit premise, refuted by direct inspection, no fix needed.** `spell_effect_summary()`/`spell_target_label()` already guard `unsigned(id) < 48` and return `nullptr` for id 48 (`magic.cpp`); this was true before this batch too. Separately, `kEffects` (`magic_tables.inc`) has **49** entries, not 48 — it silently already covers index 48 (`{MagicEffect::Animation}`, i.e. Nox is a no-op, matching its `MagicDefinitions.json` entry `"Type":"none","TimePermitted":"never"`). The audit's "only 48 entries" claim was simply a miscount. No production change; a bounds-safety regression lock was added instead (see Tests).

**Tests:** `native/core/tests/display_names_test.cpp` gained 11 semantic (substring, not literal-table-duplication) assertions pinning the 9 `kSummaries` fixes + the 1 `kTargets` fix, plus two `cast_spell()`-driven regression locks proving `kEffects[26]==Unlock` and `kEffects[28].kind==Line` so a future edit can't silently flip either based on the raw (unreliable) `target_type`/`SimpleDescription` text alone. RED confirmed by reverting `magic.cpp`/`magic_tables.inc` to their pre-batch content (keeping the additive `MagicEffect::Unlock` enum member so the suite still compiled) and re-running: all 11 new checks failed with the expected old/wrong values; the `kEffects[28]==Line` lock passed both before and after (it was never wrong). `native/core/tests/batch5_test.cpp`'s comments were corrected to state the In Ex Por rationale accurately; its assertions are unchanged (native behavior for that spell is unchanged).

**Verification:** `display_names` (0 failures, up from the pre-fix RED of 11), `batch5` (34/34, unchanged), `magic_parity` (25,088/25,088, byte-identical fixture before and after — confirms the `kEffects[26]` reclassification moves no tracked field), `command_parity`/`combat_parity`/`dungeon_parity`/`advanced_combat_parity` (all green, unaffected), `typescript_magic_fixture_drift` (green after regenerating `magic_tables.inc` from the updated generator — this table is generated, not hand-authored; the first pass at this fix hand-edited the `.inc` file directly and correctly tripped this drift check). Full host suite: **70 total, 68 pass, 2 failures** — `gameplay_parity` (R-21, pre-existing, unrelated, confirmed still failing at the identical "Gameplay mismatch 2034") and `quest_parity`, which crashes (`STATUS_ACCESS_VIOLATION`, exit `3221225477`) when built and run with this session's available MinGW/w64devkit GCC toolchain. This is a **pre-existing, unrelated environment finding, not a Batch 8 regression**: it reproduces identically on a completely clean, unmodified tree (verified before any Batch 8 edit, with and without `OPENU5_ENABLE_DEVELOPER_TOOLS`), while the project's own `build-zig` directory (zig-cc toolchain, the tool's hardcoded default driver path) does not crash — it runs to completion and hits an unrelated stale-fixture mismatch instead, consistent with that directory simply predating recent commits. Rebuilding the full suite under `build-zig` to get a clean apples-to-apples 70/69/1 was not completed this batch (`native/core/src/debug_labels.cpp` fails a `-Werror -Wstring-concatenation` check under zig's clang that GCC accepts — also pre-existing, also unrelated to R-16). Recommend a dedicated environment-parity investigation; not spawned as a numbered finding here since it touches build tooling, not gameplay.

**Firmware:** fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-batch8-spell-metadata/openu5_tdeck.bin`, **0xce1f0 (844,272) bytes**, 19% of the 1 MiB app partition free — byte-identical size to Batch 7B (the changes are metadata text + one enum value; no new code paths compiled in). Clean build, no new warnings.

**Audit-list correction:** this file's own line ~171 (World casting with a target) and the Batch 5-era comment in `magic.cpp` both stated "An Ylem, An Grav and In Ex Por have no world effect in the reference either" — corrected to drop In Ex Por from that group per the finding above.

**Remaining OPEN items (not this batch):**
- ~~Wiring In Ex Por's real door-unlock world effect...~~ **RESOLVED (Batch 8B)** natively — see below. The TypeScript side (`game/src/core/magic/cast.ts` case 26, `content-audit.md` PENDIENTE(3)) still implements the disproven "no-op" belief and remains a separate, out-of-scope follow-up (it touches `game/src`, not the native T-Deck port).
- R-21 (`gameplay_parity` mismatch 2034) — untouched per this batch's baseline, as instructed.
- Y-32 — untouched per this batch's baseline, as instructed.
- The GCC/w64devkit `quest_parity` crash and the zig-toolchain `debug_labels.cpp` `-Wstring-concatenation` build failure — newly observed this batch, pre-existing, environment/tooling issues unrelated to spell metadata; flagged for separate investigation, not adjudicated as audit rows here.

---

### R-17 — View Gem presentation is fabricated (ANCHOR 3) · **SEVERITY 2** · **GREEN (host+firmware) — RESOLVED (Batch 10); HARDWARE VISUAL CHECK PENDING**

The **mechanics are byte-correct** against `game.ts::view()` [REF]: echo before the gate, `"You have none!\n"` with an immediate turn when `gems==0`, decrement-before-paint, turn **deferred** to `AfterGemView` on close (`commands.cpp:754`, `alpha_runtime.cpp:262-268`, `:792-796`). The crystal-ball variant correctly suppresses the deferred turn via `gem_from_crystal`.

The **presentation is not derived from anything**:
```cpp
// render_world_gem_view, native_renderer.cpp:474
const uint16_t color = (tile & 3) == 3 ? 0x001f : (tile & 7) == 0 ? 0x07e0
                     : (tile & 15) < 4 ? 0x7be0 : 0x8410;
```
These are bit tests on raw tile ids, not a terrain classification — the output is a 32×32 grid of essentially arbitrary colours. The reference (`buildGemView` + `ui/viewgem.ts`) builds a descriptor with one *category* per tile and paints it with the EGA palette.

Secondary defect: `Board::show_alpha` blits the viewport only for rows `[kHudSkyBarH, 176 - kHudWindBarH)` (`tdeck_board.cpp:628-631`) and then overdraws the sky and wind bars. The gem map loses its top and bottom 9 px and gains two HUD strips across it.

**Why "appears to do nothing" is plausible without a code bug:** noise that looks like static, clipped and straddled by two HUD bars, on a 176 px square, reads as "nothing happened" — especially since the world view underneath is also mostly green. **One device screenshot with `VIEW_EFFECT`/`VIEW_RESULT` in the log settles it.** Status stays RED per the audit contract until that proof exists.

#### Resolution (Batch 10) — also closes Y-14

**Root cause confirmed against the reference.** `buildGemView` (`game/src/core/world/gem-view.ts`) classifies every overworld/town tile through a static per-tile CATEGORY table (`GEM_CATEGORY`, `game/src/skin/fiel/gemmap-overworld.ts`, 17 categories 0-16, ported byte-for-byte from LOOKOBJ's `byte[tile+0x1d1a]`) before painting, and anchors its 32×32 window to a **chunk origin** quantized to 16 (`initChunkOrigin`, MAINOUT 0x0019) — the original never centres the overworld gem on the party, and a town's gem is the whole fixed 32×32 map at (0,0), never scrolled. The native implementation did neither: it bit-tested raw tile ids for colour and always centred a `center-16` window regardless of map kind, which for a town near a map edge would additionally have silently skipped (left black) any cell outside `[0, geometry.width)` — a second, previously undocumented instance of the Y-14 clipping defect, at the content level rather than the HUD-strip level.

**Implemented fix — new portable semantic layer** (`native/core/include/openu5/gem_view.h`, `native/core/src/gem_view.cpp`, host-tested by `gem_view_regression`; ESP-free, no device/pixel code):
- `gem_terrain_category(tile)` — the ported `GEM_CATEGORY` table (`gem_category.inc`, generated from and drift-checked against `gemmap-overworld.ts` by `gem_category_table_drift`).
- `gem_chunk_origin(x,y)` — the entry-formula chunk anchor (`initChunkOrigin`). The original also keeps this origin hysteretically while walking (`scrollChunkOrigin`); reproducing that exactly would require new persistent overworld-movement state, which is out of this batch's scope (preserving overworld movement semantics unchanged) and which the reference itself treats as an accepted simplification when no persisted origin is fresh. The stateless entry formula already reproduces the essential, tested property: anchored to a 16-cell block, never centred.
- `build_world_gem_view(map, party)` — large (wrapping) maps use the chunk-origin window with toroidal wrap; small maps (towns/castles) return the **whole** fixed 32×32 map at (0,0). Always populates all 1024 cells (Y-14).
- `build_dungeon_gem_view(dungeon)` — the same 22×22 8-connected flood fill DNGLOOK performs, moved out of `native_renderer.cpp` into the host-tested core. This also fixes a second bug found while porting it: the old flood used `dungeon_wall()`, a **movement**-passability helper that treats a secret door (0xd) as passable once `revealed`; the gem view must never consult discovery state (`DUNGEON_GEM_BLOCKERS = {0xb,0xc,0xd}`, unconditionally) — a *revealed* secret door still blocked movement in this codebase already, but would have incorrectly stopped blocking the *gem's* flood, letting the gem see further than the reference does. `gem_view_test.cpp` case F3 drives this exact scenario.

**Rendering** (`native/targets/tdeck/main/native_renderer.cpp`): `render_world_gem_view`/`render_dungeon_gem_view` now only paint — they consume the `GemView` the core builds and colour each cell through the live EGA palette (`PresentationTileCache::palette`, the same source every other view uses), by the reference's own per-category/per-type colour, not a raw-tile bit test. Micro-patterns (dots/lines/frames within a cell) are simplified to a solid fill — a disclosed Class-C simplification; every colour is still the original's own EGA index for that category/type, not an invented one. The marker is now painted at `GemView::marker_{x,y}`, not a hardcoded (16,16) — required once the window stopped being unconditionally centred.

**Clipping (Y-14, HUD-strip half)** (`native/targets/tdeck/main/tdeck_board.{h,cpp}`, `alpha_runtime.cpp`): `Board::show_alpha` gained a `full_square_viewport` parameter, set from `gem_view_active_`. When true, the viewport blit covers the full 176 rows and the sky/wind strips are not drawn at all, instead of always reserving rows `[0,9)`/`[167,176)` for them and overdrawing the gem square — exactly the secondary defect this finding originally reported. Entering/leaving the mode forces both the viewport and sky-bar caches so neither a stale bar fragment nor a stale clipped rectangle survives the transition either way. The zodiac view is unchanged (still clipped) — it is not R-17/Y-14 and R-13 already marks it GREEN; touching it is out of this batch's scope.

**Evidence:**
- [REF] `game/src/core/world/gem-view.ts` (`buildGemView`), `game/src/core/world/chunk-origin.ts` (`initChunkOrigin`), `game/src/skin/fiel/gemmap-overworld.ts` (`GEM_CATEGORY`, `drawCell`), `game/src/skin/fiel/gemmap.ts` (dungeon `GLYPH`/`WALL_DENSE`/`FOUNTAIN_COLOR`/`FIELD_STRIPES`).
- [EXEC] **RED → GREEN**: `gem_view_regression` (new, 6 groups A-F: terrain classification, mixed-map full-square + non-centred town marker, N/S/E/W + row/col orientation, marker-doesn't-alter-terrain, full-square/no-clipping for both variants, wall/door/room/revealed-secret-door blocker semantics + overworld-edge wrap) and `gem_category_table_drift` (new, guards the ported table against the reference) both pass. Full host suite **77 total, 75 pass, 2 fail** — the only failures are the pre-existing `gameplay_parity` mismatch **2034** (R-21, untouched) and a newly-*surfaced* (not newly-*introduced*) `frontend` struct-padding `memcmp` sensitivity, reproduced byte-for-byte on the untouched pre-Batch-10 tree with only an unrelated compiler-warning fix applied — see §14 Batch 10 for the isolation evidence. Pre-Batch-10 baseline (as originally compiled): 75 total, 73 pass, 2 fail (`gameplay_parity`, `quest_parity`); `quest_parity`'s known environment crash did not reproduce this run.
- [BUILD] T-Deck `idf.py build` (`build-batch10`): **PASS**. Total image 852,916 bytes; app partition 19% free (`0x2fbd0` / `0x100000`); DIRAM 33.92% used.
- [HARDWARE] **Not yet performed.** Host + firmware evidence proves the semantic layer is reference-faithful and the production integration compiles; only a physical `V` on real glass (overworld, town and dungeon) remains to confirm the pixels actually land as painted — see the Batch 10 hardware checklist in §14.

---

### R-18 — `pre_combat_mode_` was one stale register serving three return paths · **SEVERITY 2** · **GREEN — RESOLVED (Batch 1)**

**Root cause:** `ui_session.cpp` wrote `pre_combat_mode_` **only** on `CombatStarted`, but read it on:
- `CombatEnded` (correct),
- `Dialogue::Ended` → `set_base_mode(pre_combat_mode_)` (`:785`),
- `Shop Exited` → `set_base_mode(pre_combat_mode_)` (`:838`).

Breaking sequence: fight in a dungeon (`pre_combat_mode_ = Dungeon`) → leave the dungeon → walk to a town → talk to an NPC → end the conversation → `base_mode` became `Dungeon` on the surface. R-01 previously masked this by re-deriving the mode on the next input; fixing R-01 without fixing R-18 would have exposed it.

**Implemented fix:** Combat keeps its own `pre_combat_mode_`. Shop now owns `shop_return_mode_`. Dialogue now owns `dialogue_return_mode_`. Each return mode is captured before the session-mode overwrite and reduced to the underlying world mode where necessary.

**Evidence:** [STATIC] root cause; [EXEC] fix — `ui_mode_regression` 28/28 GREEN, host suite 57/58 (only unrelated `gameplay_parity` mismatch 59 remains), T-Deck ESP-IDF build PASS. Hardware flash not performed.

---

### R-19 — `(Y)ell` has no frigate branch, so ships cannot sail · **SEVERITY 1** · **GREEN — RESOLVED (Batch 3)**

Reference dispatcher [REF], `game.ts:5054`:
```ts
yell(): GameEvent[] {
  const transport = this.state.transportTile ?? 0x1c;
  if (isFrigate(transport) && this.state.position.location < 0x80) return this.yellSails();
  return [{ kind: "yell-word-prompt" }];
}
```
Native device route, `ui_session.cpp:590`:
```cpp
case 'y': command_echo("Yell"); begin_text(UiRequestId::YellText,"Yell what?",15); return true;
```
— unconditional word prompt. `finish_modal` converts it to `CommandKind::Yell` → `yell_in_world()`, which handles words of power and shadowlord summoning and **never touches sails**. `CommandKind::YellSails` is fully implemented (`commands.cpp:940`: FURL/HOIST, `sync_transport ±4`, clears `sail_dir`, charges a turn, emits `MapChanged`) and has **zero callers**.

Consequence: the ship can be boarded but the sails can never be hoisted — **all wind-driven ocean travel is blocked**, which gates Serpent's Hold, Buccaneer's Den, the Isle of the Avatar and the endgame.

**Fix shape:** two lines in `handle_exploration` — branch on the transport tile before the text prompt, exactly as the reference does. The device has `turn_.transport_tile` available via the same `UiSession` that already tracks shop phase.

#### Resolution (Batch 3)

`handle_exploration`'s `'y'` case now branches before the word prompt:

```cpp
case 'y':
    command_echo("Yell");
    if (sail_context_frigate_ && sail_context_location_ok_) { c.kind = CommandKind::YellSails; break; }
    begin_text(UiRequestId::YellText, "Yell what?", 15); return true;
```

`UiSession` does not own `GameState`/`TurnState`, so the two predicates are **mirrored in** from the owner through the narrow context path, exactly as `set_shop_offer_count()` already works. The Batch-3 RED-test seam `set_sail_context(frigate_aboard, location_allows_sails)` was **promoted into a real production-fed path**: `AlphaRuntime::refresh_session_context()` writes both values from authoritative runtime state (`(turn_.transport_tile & 0xf8) == 0x20` and `game_.position.map.location < 0x80`) immediately before every input is routed, so tests can never supply something production does not. The mirror only *routes*; `CommandKind::YellSails` re-checks the same two predicates authoritatively in `commands.cpp`, so a stale mirror could at worst mis-route, never mis-apply.

It is a state-driven HOIST/FURL toggle: no new choice modal, no new UI. Frigate tiles are `0x20-0x27`; `location >= 0x80` (Underworld) excludes sails behaviour.

**Y-24 discharged:** the word-of-power branch is unchanged and covered alongside the sails branch — `batch3_group_a_test.cpp` A1 (frigate -> `YellSails`, no `YellText` modal), A2 (non-frigate still opens `YellText`), A3 (the word path still reaches `CommandKind::Yell`).

**Evidence:** [REF] + [STATIC] + [EXEC].

---

### R-20 — Harpsichord melody unreachable · **SEVERITY 3** · **GREEN — RESOLVED (Batch 3)**

`play_harpsichord` (`quest_world.cpp:116`) and `CommandKind::HarpsichordNote` are fully implemented, including the Cove secret-passage trigger at location 17 floor 2. No device route constructed the command — the note digits were never captured.

#### Resolution (Batch 3)

`handle_exploration` now routes the digit keys `'0'`-`'9'`, in the reference's own order (`main.ts`): the harpsichord intercept is tested **first**, and when it fires the digit dispatches `CommandKind::HarpsichordNote` with the literal note digit and never reaches the set-active-player arm. Outside that context the digit is a `SetActivePlayer` (Y-20 below).

Whether the intercept is active is a second narrow runtime/session context mirror, `set_harpsichord_active()`, fed by `AlphaRuntime::refresh_session_context()` from the authoritative rules — the same predicate as the reference's `Game.harpsichordSeated()`:

- not in combat, not in a dungeon;
- small map only (`position.map.location != 0`, so never the overworld or the Underworld);
- the harpsichord tile **141 / 0x8D** immediately **south** of the party (the chair is north of the instrument).

No modal was invented, and digit interception is **not** broadened beyond that context: outside it every digit still routes to `SetActivePlayer`.

The existing melody implementation (`advance_melody` / `play_harpsichord`, the 13-note sequence `6 7 8 9 8 7 8 7 6 7 6 5 3` and the Cove passage-open mutation at location 17 floor 2) is **unchanged**; `batch3_group_a_test.cpp` A6 drives the whole melody through `execute_command()` as a GREEN guard.

**Regression coverage:** A5 (digit at the harpsichord dispatches `HarpsichordNote`, not `SetActivePlayer`), A6 (core melody GREEN guard).

---

### R-21 — `gameplay_parity` mismatch 2034: fabricated (U)se-consumable echo · **SEVERITY 2** · **GREEN — RESOLVED (Batch 13), production fix**

**What 2034 actually was.** Not a byte offset. `check-gameplay.ts` throws `Gameplay mismatch ${i}` where `i` is the **sequence index**, so 2034 is scripted sequence #2034 — the very first `(U)se` of a scroll in the whole fixture (`{kind:'use',item:0,member:-1,dir:0}`, seed 0, Vas Lor on a town map). Every sequence starts from a fresh `initial()` state, so nothing upstream feeds it: 2034 is the FIRST divergence, not downstream fallout. Sequences 0–2033 are simply the blocks that never touch a scroll or a potion.

**The audit's previous account was backwards and is replaced.** The old row said "native emits `Scroll` where the reference emits `Used Vas Lor Scroll.`, and the reference's expected stream includes an `sfx` event and an `unknown`-kind event that native does not produce." The regenerated artifact says the opposite on every point:

```
ACTUAL   (native)    [message "Used Vas Lor Scroll."] [message "Light!"] [sfx "scroll-used"] [unknown ""]
EXPECTED (reference) [message "Scroll"]               [message "Light!"]
```

**Reference rule.** `re/notes/potions-scrolls.md`, derived from `re/disasm/CAST.OVL.asm`: the scroll reader at `0x11de` does `11ec dec byte[bx+0x5820]` (consume, always, first), then `11f0 print "Scroll\n\n"` (DS `0x466a`), and only then jumps to the per-scroll handler, which prints its own DS line. The potion drinker at `0x135a` does the same with `136e print "Potion\n"` (DS `0x4706`). Neither reader ever echoes the item's own name — the (U)se picker closes without echoing it, unlike (R)eady. `game/src/main.ts` states the identical rule at all three of its consumable mouths (`hud.messageAppend("Scroll")` / `hud.messageAppend("Potion")`, each citing its DS offset), including the arena mouth `applyCombatScroll`. Second rule, same sites: a handler with no authored DS line prints **nothing** — Blue always, Yellow when it healed 0, Red/Green/Orange on a failed precondition, White on a successful reveal. `"No effect!"` (DS `0x46ec`) is An Tym's location gate, not a generic fallback; no potion handler reaches it.

**Root cause — confirmed native behavioural defect (classification 1).** Both fabrications were introduced deliberately by the Alpha-20 forensic action-feedback pass, whose own write-up records the intent verbatim: *"Successful potion and scroll paths now emit `Used <authoritative display name>.` followed by the existing effect/result text (or `No effect!` when no authored result exists)"* (`ALPHA20_FORENSIC_LOOT_MODAL_ACTION_FEEDBACK_PASS.md`). That was a device-UI decision applied to the **core** command layer, where it contradicts the binary. The same write-up records why it went undetected: *"Node/tsx reference generators: infrastructure-blocked before project code"* — the parity suite never ran in that session. It has been latent ever since, masked until Batch 2 fixed mismatch 59 and let the test run far enough to reach 2034.

**Two further defects of the same family, uncovered while proving it and inseparable from it** — the parity fixture cannot encode one reference rule that holds at two mouths and not the third:

- **Potion ceremony ordering.** `CAST.OVL 0x135a` lists `136e` print · `1375` selChar · `1394` exit-if-cancelled · **`139b` anim(colour)** · `13a8` reroll · handler's DS line. The ceremony precedes the reroll, so its index is the colour **asked for**. Native emitted it *after* the effect line, at both the world and the arena mouth. `main.ts` pins the reference order at both (`applyCombatPotion`, and the first statement of the `pickCastTarget` callback).
- **Dungeon casts raised no ceremony at all.** There is one cast dispatcher in the binary (the 48-entry jump table at `CAST.OVL 0x0f1a`), so a dungeon cast runs the same handlers and reaches `CAST2:0x0000` with the same circle index; `main.ts` emits it at all three cast mouths through one `emitCastCeremony` helper. Native re-implements the cast head a third time in `dungeon_orchestration.cpp`, and that copy was silent — the same spell was ceremonial outdoors and mute underground.

**What was NOT a defect.** The `MagicCeremony` indices and gates were already reference-correct: native's scroll table `{0,-,2,3,4,-,-,7}` (world) and `{0,-,2,3,-,-,-,7}` (arena, In Quas Wis gated by `0x127f` before the ceremony), potion index = colour, cast index = circle, and the seven `SPELLS_WITHOUT_CEREMONY` `{1,13,37,28,40,44,45}` all match `game/src/core/magic/ceremony.ts`, which derives the whole jump table handler by handler. They showed up as `unknown ""` because of a **driver** gap: `gameplay_driver.cpp` had no name for `GameEventKind::MagicCeremony` and dropped its index. The four `Sfx` ids (`spell-cast`, `scroll-used`, `potion-used`, `invalid-magic`) are native-invented semantic hooks with no reference counterpart anywhere — the Alpha-20 pass says so outright (*"No `AudioService` implementation is present in this tree. This pass therefore emits only semantic `Sfx` hooks"*).

**Correction.** `native/core/src/world_magic.cpp` (drop the `used()` helper; echo `"Scroll"` / `"Potion"`; print nothing when the result carries no authored line; raise the potion ceremony before the reroll), `native/core/src/combat.cpp` (`combat_use_consumable` — the same four changes at the arena mouth), `native/core/src/dungeon_orchestration.cpp` (raise the circle-indexed ceremony on a successful dungeon cast, and the failure hook its two sibling mouths already emit). No name table was touched: `scroll_display_name` / `potion_display_name` still return the picker labels, asserted as a control (A3).

**Harness corrections — coverage ADDED, not removed.** `gameplay_driver.cpp` now serialises `MagicCeremony` as `magic-ceremony` **carrying its index**, and `check-gameplay.ts` models the reference ceremony at all four magic mouths from `ceremony.ts`'s derived tables, so roughly 640 sequences now assert a ceremony index and gating the suite previously could not see at all. The four native-only audio hooks are normalised out of the native side by a named, commented filter, since no reference model has ever contained them; every `Sfx` the reference core really does raise (`dungeon-zap`, `dungeon-fail`, `dungeon-trap`, `field-afflict`, `combat-damage`) stays compared.

**RED proof.** New `batch13` CTest (`native/core/tests/batch13_test.cpp`, groups A–F): **58 failing checks** against unmodified production code, **0** after. Group E (ceremony order) was added separately and went RED at **2** failures with E3 passing as a control; group F (dungeon ceremony) went RED at **7** with F3 passing as a control. `action_feedback_regression` was **asserting the fabrication** — `check(has_text(events,"Used "+scroll_display_name(0)+"."))`, plus a `potion_messages>=2` check that only ever passed because of the invented fallback. It was **corrected, not weakened**: re-pointed at the DS rule and extended with a positive case proving an authored line is still printed.

**Mutation proof** — each applied to a clean tree, rebuilt, measured, then reverted:

| mutation | `batch13` | `action_feedback` | `gameplay_parity` |
|---|---|---|---|
| M1 restore the world scroll name echo | 18 fail | fail | **mismatch 2034** — the original R-21 signature |
| M2 restore the `No effect!` fallback | 4 fail | — | mismatch 2163 |
| M3 move the potion ceremony back after the effect line | 1 fail | — | mismatch 2179 |
| M4 drop the dungeon cast ceremony again | 2 fail | — | mismatch 2808 |
| M5 change one ceremony index (In Quas Wis 4 → 5) | 1 fail | — | mismatch 2098 |
| M6 refabricate the arena potion echo | 17 fail | — | mismatch 3144 |

M5 is the load-bearing one: a single-digit change to an index the parity suite could not previously see now fails it.

**Evidence:** [EXEC] `gameplay_parity` **PASSES**, 5058/5058 sequences with every outcome witness present; `batch13` 0 failures; full suite 81 total / 80 pass / 1 fail; six mutations. [STATIC] `re/notes/potions-scrolls.md` (the CAST.OVL `0x11de` / `0x135a` listings), `game/src/core/magic/ceremony.ts`, `game/src/main.ts` (scroll 4841, potion 4866, arena potion 4941, arena scroll 4972, dungeon cast 4528), `ALPHA20_FORENSIC_LOOT_MODAL_ACTION_FEEDBACK_PASS.md`.

---

### R-22 — Native Z-stats status/inventory pages missing · **SEVERITY 2** · **GREEN — RESOLVED (Batch 14)**

**Discovery context:** pre-Batch-4 hardware testing walked the reference `(Z)` command family and compared it against the native T-Deck implementation. This was missed by the original audit — Batch 3's Use-picker reachability work (R-07/R-08) covers the `(U)se` item picker only and does not substitute for `(Z)` status pages, which are a distinct, unimplemented UI axis.

**Reference `(Z)` page family:** the reference presents a member-select screen, then, per selected member, a multi-page status flow:
- Stats page (attributes, level, exp, hp/mp)
- Arms page (equipped weapon/armor/shield/ring/amulet)
- Provisions page (food, gold, torches, gems, keys)
- Reagents page
- Spells page (known spells / spellbook contents)
- Items page — including quest/special possessions: Amulet, Crown, Sceptre, Shards, Moonstones, HMS Cape plans, Spyglass, Sextant, Black Badge, Wooden Box
- Armaments page (full weapon/armor inventory)

**Native limitation:** `case 'z'` in `ui_session.cpp` (`handle_exploration`) only dispatches `UiIntentKind::OpenStatusSelection` with `UiRequestId::Status`, which opens the initial party-member picker. Selecting a member does not advance to any Stats/Arms/Provisions/Reagents/Spells/Items/Armaments page — it merely reopens or repositions the same member picker. There is no real status-page axis backing `(Z)` today, so none of the quest/special items listed above (which the reference explicitly surfaces under `(Z)`'s Items page) are ever shown to the player through this command.

**Fix (Batch 14) — RESOLVED.** The page family was implemented against the binary, not against the bullet list above (which was written from memory and got two things wrong: the odd per-member page is **Arms only** — `draw_arms_page` 0x02a8 paints the six equipment slots and no spellbook — and list `0xe` is **Spells**, the mixture counts at `0x57f0`, not a second item page). See §14 Batch 14 for the full write-up, the proven page family and the residuals.

**Evidence:** [STATIC] — `ui_session.cpp` `case 'z'` and the `OpenStatusSelection`/`UiRequestId::Status` handling; [REF] — `re/notes/zstats.md` (ZSTATS.OVL function map, record layout, the DS 0x1a7e/0x1aae tables) and `re/notes/ztats-layout.md` (the 17-slot page axis §2, per-page field order §1/§8, `render_item_list` §3–§5, the key matrix §1b/§8.5); [TEST] — `batch14_zstats_model` (97 checks) and `batch14_zstats_runtime` (54 checks).

---

### R-23 — Blackthorn sacrifice roster compaction stopped at the roster length · **SEVERITY 3** · **GREEN — RESOLVED (Batch 15)**

**Discovery context:** pre-Batch-4 hardware testing traced `sacrificeFirstCompanion()` (reference) against `blackthorn.cpp::sacrifice()` (native) while investigating an unrelated single-character-combat report (see R-24/DebugPreset::Combat, which turned out to be the actual cause of that report — this finding is separate and lower priority).

**Reference behavior:** `sacrificeFirstCompanion()` performs full roster compaction — `characters.splice(victim, 1)` removes the sacrificed member and shifts every later slot down, then `characters[15] = sacrificedRecord` places the sacrificed record's remnant at the fixed final roster slot.

**Native divergence:** `blackthorn.cpp::sacrifice()` only shifts entries within the old `character_count`, then unconditionally forces `character_count = 16`. Because the shift does not extend into the newly-exposed range `[old character_count, 16)`, and `character_count` is forced to 16 regardless, this can expose blank/default-constructed roster slots inside `[0, character_count)` that were never part of a compaction shift.

**Potential downstream effects:** any code that iterates `[0, character_count)` and assumes every slot holds a real (if possibly dead) character — including the Developer fill helpers (`fillable_party`/`maximize_party` in `debug_developer.cpp`, which stop at the first blank `name[0] == 0` slot) — may see a native-only blank gap that the reference roster never produces.

**Scope note:** this is **not** the cause of the user's ordinary one-character-in-combat report; that was `DebugPreset::Combat` silently engaging Set Active Player (R-24, fixed in the pre-Batch-4 Developer-tool cleanup). R-23 is a separate, lower-priority fidelity bug and is not fixed in this pass.

**Adjudication (Batch 15) — the finding is REAL but was mis-described on two counts.** The original entry was written from the TypeScript port; Batch 15 re-derived it from the binary first.

- **The reference rule, from `BLCKTHRN.OVL 0x0438` `sacrifice_member`** (`re/notes/blackthorn.md` 3.3; the full instruction trace in `re/notes/blackthorn-cota-party-size.md` 1; the slot-15 arithmetic independently re-measured in `re/notes/cabeza-264b-acta.md` 4.1): the victim is the **second LIVING member** (`044c cmp byte [si],'D' / inc cx / cmp cx,2`) scanned with `g_party_size` as the **only** bound (`043d`; there is no `cmp si,6` in this family, unlike the chest traps). Its whole 32-byte record is copied to a local (`046c`, `repne movsw cx=0x10`); `0487 cmp ax,0xf / jge` **skips the compaction entirely when the victim already is slot 15**; `04ab-04c0` shifts `record[i] = record[i+1]` **until `si == 0x57a8`** — `0x55a8 + 512`, the end of **all sixteen records**, *not* the party bound; `04c2-04cd` copies the parked record to `DS:0x5788` (slot 15); `04cf` writes `[0x57A7] = 0x7f` (record `+0x1F` = `partyStatus`); and `04d4 dec [g_party_size]` runs **last and exactly once**.
- **Correction 1 — the artefact is a DUPLICATE, not a blank.** Native's shift stopped at the old `character_count` and then forced `character_count = 16`, so slot `old_count-1` kept a **stale copy of its own previous occupant** (the roster's last record appeared twice), and slots `[old_count, 15)` were never shifted at all. "Blank roster gaps" described the untouched tail correctly but named the immediate defect wrongly.
- **Correction 2 — the defect was NOT reachable on the device.** Every `.GAM` produces exactly sixteen records (`persistence.cpp` `import_native`, `for (i = 0; i < 16)`), and with `character_count == 16` the old loop bound and the reference bound are the same expression — native was already byte-for-byte correct on the shipped roster shape, and on every `quest_parity` Blackthorn scenario, which builds a full 16-record roster (`tools/check-quests.ts:174`). That is why no parity suite ever caught it. The divergence was reachable only through rosters **shorter than sixteen records**, which `save::deserialize`/`restore_core` accept (`persistence.cpp:521`, `save_core.cpp:278` allow any 1..16) — JSON-only states and host fixtures. The finding is therefore **latent on hardware and live on the host/JSON surface**, not the device-visible bug the original severity implied.
- **Correction 3 — `character_count = 16` was never the defect.** It is the faithful mirror of the reference's fixed sixteen-record table, and of the port's own `characters[15] = rec` (which forces `characters.length` to 16 with holes in between; a hole reads `?.status !== "D"`, i.e. *living*, exactly as native's default-constructed record does).

**Index/reference rule — adjudicated and deliberately NOT "fixed".** The census of persistent roster indexes (`re/notes/party-contiguidad-acta.md` 3) finds exactly one: `activeCharacter` / `g_active_char`. `guardIdx`/`buyerIdx`/`casterIdx`/`memberIdx` are single-command parameters that do not survive the command. **`sacrifice_member` does not re-index it.** The re-indexing block is `SHOPPES3 0x03dd-0x0400`, which belongs to the inn **(L)eave** (native `inn_leave`, `shops.cpp:275-278`, already faithful), and `BLCKTHRN 0x03ae-0x04d4` has no counterpart — the same declared asymmetry the inn **(P)ickup** already carries. So after a sacrifice the active-character index keeps its numeric value and, if it pointed at or past the victim, now addresses a **different** character. That is the original's behaviour; it is cloned bug-for-bug and pinned by tests from both sides (B2/B3 assert the sacrifice leaves it alone, B4 asserts `inn_leave` still re-indexes), so a later "consistency" refactor cannot quietly unify the two routines.

**Fix (Batch 15) — RESOLVED.** One bound: the compaction loop in `blackthorn.cpp` now runs to `kRosterCapacity` instead of `g.party.character_count`, matching `04ab-04c0`'s run to the end of the table. The victim selection, the slot-15 park, the `0x7f` `partyStatus`, the `party_size` decrement and its ordering are unchanged — they were already correct. The routine was lifted out of the file's anonymous namespace and declared in `openu5/blackthorn.h` as `sacrifice_first_companion()` (the reference's own name) so the mutation is testable at a production seam; its two call sites are unchanged. **No shared party-deletion primitive was introduced**: `inn_leave`/`inn_pickup` are a different asm body with a different active-character rule, and merging them would destroy the asymmetry above.

**Still open after this batch:** nothing on roster compaction. Broader Blackthorn ceremony fidelity is **not** marked GREEN by this batch — the capture/interrogation presentation and pacing work (Y-31/Y-32) and the consolidated hardware validation of the whole sequence remain open, and `§2`'s Blackthorn rows keep their existing R-09-era verdicts.

**Evidence:** [REF] `BLCKTHRN.OVL 0x0438` via `re/notes/blackthorn.md` 3.3, `re/notes/blackthorn-cota-party-size.md` 1, `re/notes/cabeza-264b-acta.md` 4.1, `re/notes/party-contiguidad-acta.md` 2.2/2.3/3; [REF-2] `sacrificeFirstCompanion()` and `rosterLeaveCompact()` (`game/src/core/party.ts`); [STATIC] `native/core/src/blackthorn.cpp` `sacrifice_first_companion()`, `native/core/src/shops.cpp` `inn_leave()`; [TEST] `batch15_sacrifice_roster` (75 checks, groups A–F).

**Evidence:** [REF] `sacrificeFirstCompanion()`; [STATIC] `native/core/src/blackthorn.cpp` `sacrifice()`.

---

### R-24 — `DebugPreset::Combat` silently engaged Set Active Player, auto-passing every other party member · **SEVERITY 1** · **GREEN — RESOLVED (pre-Batch-4 Developer-tool cleanup)**

**Discovery context:** pre-Batch-4 hardware testing of the Developer `Combat` preset found that after applying it and entering combat, only one party member (member 0 / the Avatar) ever received a manual turn; every other party CombatActor was created correctly but was silently auto-passed every cycle.

**Root cause:** `apply_debug_preset(..., DebugPreset::Combat)` (`native/core/src/debug_developer.cpp`) set `g.party.active_character = 0` after maximizing the party. `active_character != 255` is not inert debug metadata — it is the live state read by the real Set Active Player mechanic (`commands.cpp` `CommandKind::SetActivePlayer`, and the scheduling check in `combat.cpp`'s `Engine::current()`: `if (player(a) && g.party.active_character != 255 && a.member != g.party.active_character) ... skip = true`). Setting it to `0` is functionally identical to the player manually pressing `1` (Set Active Player, member 0) before combat — every other living player actor is auto-passed for the rest of the encounter.

**Fix:** `DebugPreset::Combat` no longer touches `active_character` away from the unrestricted sentinel (`255`); it now explicitly (re)asserts `255` after preparing the maxed/geared/stocked multi-member state, so the preset produces a good combat test party without engaging Set Active Player. The real Set Active Player command and combat scheduling logic are unchanged.

**Regression coverage:** `debug_developer_test.cpp` — (1) the existing preset check now asserts `active_character == 255` after `DebugPreset::Combat`; (2) a new RED-before-GREEN regression builds a real multi-member arena via `initialize_combat`, cycles turns through the real combat scheduling path (`current_combat_actor`/`combat_action`), and asserts more than one distinct party member receives a turn. Verified to fail (RED) against the pre-fix source with the message "combat preset leaves every party member reachable for manual turns, not just one," and to pass (GREEN) after the fix.

**Evidence:** [EXEC] RED-before-GREEN regression in `debug_developer_test.cpp`; [STATIC] `combat.cpp` `Engine::current()` skip logic; [STATIC] `commands.cpp` `SetActivePlayer` handler.

---

### R-27 — Developer Default Entrance resolved the floor-list ordinal instead of signed z, opening basement-having locations into their basement · **SEVERITY 2** · **GREEN — RESOLVED (Batch 4.5A-1)**

**Discovery context:** RED characterization tests (`debug_map_picker_test.cpp` T3/T4/T6, `ui_debug_menu_test.cpp` T1/T2/T5) written against the Developer Teleport screen found that selecting a small-map destination and applying "Use default entrance" did not reliably land at the canonical standard-entry cell for locations that author a basement below their ground floor.

**Root cause:** `UiDebugMenu::apply_action()` (`native/core/src/ui_debug_menu.cpp`) resolved the Teleport screen's floor field by calling `debug_floor_at(context_, destination, floor_)`, where `floor_` is a *list ordinal* into that destination's ascending-sorted floor set, not a signed z value. Selecting a new destination reset `floor_` to ordinal `0` (`apply_value`, `cursor_==0` case). For a location whose floors are sorted `{-1, 0, 1, 2, 3}` (a basement below ground level, e.g. Blackthorn location 18 or Serpent's Hold location 32), ordinal `0` is the *basement* (`z=-1`), not signed `z=0` — so Default Entrance silently opened directly into the basement instead of the intended ground-floor standard entrance. `debug_floor_at` itself was correct (it genuinely returns the ordinal's floor, sorted); the defect was the UI layer treating "reset ordinal" as equivalent to "canonical entrance floor," which is only true for locations with no floor below `z=0`. The underlying `debug_map_picker.cpp`/`apply_debug_teleport` API was unaffected — it has always applied whatever floor a caller passes, exactly like `DebugApi.goToLocation`; basement access via an explicit request was never broken.

**Fix:**
- `native/core/include/openu5/transitions.h` now exposes the canonical small-map arrival cell as shared constants (`kSmallMapEntryX = 15`, `kSmallMapEntryY = 30`, `kSmallMapEntryFloor = 0`), matching normal gameplay's `load_small_map`/`party.ts` entry rule. No per-location entrance table was introduced.
- `native/core/src/transitions.cpp` (`load_small_map`) and `native/core/src/debug_map_picker.cpp` (`apply_debug_teleport`'s standard-entry x/y resolution) now read from those constants instead of repeating the literal `15, 30`.
- `UiDebugMenu::apply_action()` (`native/core/src/ui_debug_menu.cpp`) now resolves the Teleport-screen floor differently depending on `standard_entry_`: when the destination is a small map and "Use default entrance" is on, the request floor is the canonical `kSmallMapEntryFloor` (signed `z=0`) directly, never the ordinal. When `standard_entry_` is off (explicit manual placement) or the destination is a dungeon/large map, floor resolution is unchanged — still ordinal-driven through `debug_floor_at`, so manual basement selection (`z=-1`) and all other manual floor choices continue to work exactly as before.
- New debug-certification safety contract: `apply_debug_teleport` now refuses a **standard-entry** teleport whose resolved destination cell is confirmed impassable, returning a new `DebugTeleportStatus::ImpassableDestination` with no `GameState` mutation, no `MapChanged` event and no reload side effects (`native/core/src/debug_map_picker.cpp`). `validate_debug_teleport` is unchanged and still reports `Applied` with `passability_known`/`passable` metadata so callers can inspect a cell before choosing to apply it — only `apply_debug_teleport` enforces the refusal. An **explicit manual coordinate** (`standard_entry == false`) is never refused this way, even onto a known-impassable cell (e.g. Blackthorn's prison or Serpent's Hold's Flame of Courage) — it still applies and reports `passable == false`, preserving free tester placement.
- `UiDebugMenuView` (`native/core/include/openu5/ui_debug_menu.h`) gained `teleport_passability_known`/`teleport_passable`, carrying the authoritative `DebugTeleportResult` passability metadata into the view, because `last_teleport_status` alone cannot distinguish "Applied onto a walkable cell" from "Applied onto an explicit impassable cell" (both are `DebugTeleportStatus::Applied` by design — Part 3 above). Device status presentation (`native/targets/tdeck/main/alpha_runtime.cpp`, `teleport_result_label`) now shows `Applied`, `Applied (impassable)`, or `Impassable destination` instead of a bare `Applied` in all three cases; `teleport_status_name`'s table also gained the new `ImpassableDestination` entry.
- Scope: Developer-tool teleport resolution and presentation only. Normal small-map gameplay entry (`load_small_map`) behavior is unchanged except for now reading the same named constants instead of repeating literals.

**Affected locations (basement-having small maps where Default Entrance previously mis-resolved):** Yew, Lord British's Castle, Palace of Blackthorn, Serpent's Hold.

**Regression coverage (RED→GREEN):**
- `debug_map_picker_test.cpp`: T6 characterizes that `debug_floor_at` ordinal `0` is the basement, not signed `z=0`, for Lord British's Castle. T3 drives a synthetic single-floor location (99) with an impassable standard-entry cell through `validate_debug_teleport` (still reports `Applied`/`passable=false`) and `apply_debug_teleport` (now refuses, status `!= Applied`, zero mutation/events/reloads). T4 confirms an explicit manual coordinate onto the same impassable cell still applies and relocates the party.
- `ui_debug_menu_test.cpp`: T1/T2 drive the real Teleport screen for Blackthorn and Serpent's Hold (each authoring a `z=-1` basement with a void standard-entry cell and a walkable `z=0` standard-entry cell) and confirm Default Entrance now lands at `location/floor==0, x=15, y=30`, not the basement. T5 drives an explicit walkable vs. explicit impassable teleport through the same fixture and confirms `UiDebugMenuView.teleport_passability_known`/`teleport_passable` now distinguish them (this replaced a self-contradictory placeholder assertion in the original RED test that compared `last_teleport_status` for both cases, which can never be true since both legitimately report `Applied`).
- All prior debug-tool coverage (`debug_developer_test`, `debug_map_picker`, `ui_debug_menu`, `ui_mode_regression`) and transition/world-flow tests touched by the shared-constant extraction remain green.

**Final suite result:** `native/core/build-batch1-control` — **63 total, 62 pass, 1 fail**, sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `debug_map_picker` (62/62) and `ui_debug_menu` (23/23) are fully GREEN; no Developer-tool test failures remain.

**Evidence:** [EXEC] `debug_map_picker_tests.exe` 62/62, `ui_debug_menu_tests.exe` 23/23, full `ctest` 62/63 (sole R-21); [STATIC] `transitions.h`/`transitions.cpp`/`debug_map_picker.cpp`/`ui_debug_menu.cpp`/`ui_debug_menu.h`/`alpha_runtime.cpp` diffs.

---

### R-28 — Developer UI: raw ordinals, cursor-only value visibility, and triplicated label tables · **SEVERITY 3 (Developer-tool only)** · **GREEN — RESOLVED (Batch 4.5A-2)**

**Discovery context:** A design audit of the Developer debug UI ahead of the Special Items / Certification work (Batch 4.5A-3+) found the view-model underneath it too weak to build on: values were only ever shown for the currently selected row, several rows displayed meaningless raw ordinals (the Quest Item selector showed bare `0`-`5`, not the item's name or its real gameplay id), and category/item label tables were duplicated between `native/core/src/ui_debug_menu.cpp` and `native/targets/tdeck/main/alpha_runtime.cpp` with no shared source of truth.

**Root cause:**
- `AlphaRuntime::debug_screen()` only ever formatted a value for `index == cursor && v.editable` — every other visible row rendered as a bare label, so a tester scanning eight stat rows saw only the one under the cursor.
- `UiDebugMenu` had no notion of "this row's current value" independent of cursor position; `item_edit_range()` and `item_name()` were both hard-keyed to `cursor_`.
- `UiDebugMenu::category_name()`/`item_name()` (long-form category names, e.g. `"Teleport / Map Picker"`) and `alpha_runtime.cpp`'s `kDebugCategories`/`kTeleportItems`/.../`kDiagnosticItems` (short-form, device-facing) were two independently-maintained copies of the same vocabulary, plus a third `DebugList`/`debug_list()` indirection layer that only existed to paper over the mismatch. `teleport_status_name`, `teleport_result_label`, and `debug_status_name` were also device-local, and the last of the three rendered `DebugStatus::Unsupported` as the misleading `"Unavailable"`.
- The Quest Item selector displayed `DebugQuestItem`'s raw enum ordinal (`0`-`5`) with no path to either a human name or the real gameplay item id (29-31 for the shards, 18-20 for the regalia) — the exact ordinal/id conflation this batch was chartered to eliminate before Special Items work could build on it safely.

**Fix:**
- New core-owned label seam: `native/core/include/openu5/debug_labels.h` / `src/debug_labels.cpp`. ESP-free, host-testable, zero heap allocation, no RTTI, static/constexpr tables throughout. Owns: root category names (`debug_root_category_name`, statically asserted 1:1 against `UiDebugCategory::Count`), Diagnostics group names (mirrors `device_smoke_tests.cpp`'s own group table — see below), `DebugQuestItem` → `{name, canonical_id}` metadata, `DebugStatus`/`DebugTeleportStatus` presentation text (`"Unsupported"`, not `"Unavailable"`), the composed teleport-result label (status + passability), transport-mode names, equipment-slot names, and two small formatters (signed-z floor label, character-name-with-fallback).
- New `DebugRowValue` descriptor (`native/core/include/openu5/ui_debug_menu.h`): `{Kind, value, text, canonical_id}` with `Kind ∈ {None, Integer, Boolean, Text, TextWithId, Unsupported}`. `UiDebugMenu` gained `row_label(size_t)`/`row_value(size_t)`, both keyed by an explicit row index rather than `cursor_`, so any visible row can be described independently of cursor position. `item_edit_range()` was refactored to take an explicit `row` parameter (same behavior, no longer implicitly `cursor_`-only) so `row_value()` can reuse it for arbitrary rows. When the queried row is the one currently under edit, `row_value()` substitutes the live (uncommitted) edit value before formatting — a strict improvement over the old device-side special-casing, which only did this for two of the Teleport screen's six rows (destination name, on/off) and left every other in-progress edit (Transport Mode, Equipment item id, …) showing a bare number while being edited.
- `AlphaRuntime::debug_screen()` (`native/targets/tdeck/main/alpha_runtime.cpp`) no longer owns any label table. It iterates the visible row window and asks `UiDebugMenu` for `row_label(index)`/`row_value(index)`, formatting strictly by `DebugRowValue::Kind` (`Integer` → number, `Boolean` → Yes/No, `Text`/`TextWithId` → name (+ `[canonical_id]`), `Unsupported` → the literal word, `None` → nothing). `kDebugCategories`, `kTeleportItems` … `kDiagnosticItems`, the `DebugList` struct, `debug_list()`, and the device-local `teleport_status_name`/`teleport_result_label`/`debug_status_name` were deleted outright; the two remaining teleport-status log call sites now call `openu5::debug_teleport_status_name()` directly.
- `device_smoke_tests.cpp`'s own 15-entry group-name table was deliberately **left in place** rather than folded into `debug_labels.h`: it is tightly coupled to that file's own `Scenario`/group internals, and merging it would pull device smoke-test structure into core for no behavioral gain. This is the one documented remaining duplication (`debug_labels.cpp`'s `kDebugDiagnosticGroupNames` must be kept textually in sync with `device_smoke_tests.cpp`'s `groups[]` by hand) — explicitly permitted by this batch's own scope guard ("if this... drags device diagnostics internals into core dependencies, leave that duplication documented").
- Root category names were unified onto the existing device-facing short forms (`"Teleport"`, `"Quest"`, `"NPC / Dungeon"`, `"Presets"`, …) rather than the old core-only long forms (`"Teleport / Map Picker"`, `"Quest / Progression"`, `"Shortcuts / Presets"`) — shorter, T-Deck-appropriate, and now the single form both breadcrumb and root-menu rows render. No test asserted the old long forms.
- Scope: presentation/label/view-model only. No setter semantics, preset effects, category count, navigation shape, teleport semantics, or turn/RNG behavior changed. Special Items, the Quest/Special category split, and Certification presets remain unimplemented — deliberately out of scope for this batch.

**Regression coverage (RED→GREEN):**
- New `debug_labels_test.cpp` (L1-L6): exact quest-item name/canonical-id pairs for all six `DebugQuestItem` values; transport-mode names; equipment-slot names (actual code order — Helmet/Armor/Weapon/Shield/Ring/Amulet, not the illustrative order in the batch brief); `DebugStatus::Unsupported` → `"Unsupported"`; root category count/uniqueness; and an explicit ordinal-vs-canonical-id spot check (`ShardFalsehood` ordinal `0` → canonical id `29`; `Amulet`/`Crown`/`Sceptre` ordinals `3`/`4`/`5` → canonical ids `18`/`19`/`20`) — the exact confusion this batch exists to prevent from recurring. 124 checks total (also covers diagnostic-group table, floor-label formatting, character-label fallback, and teleport-result-label composition).
- `ui_debug_menu_test.cpp` extended with U1-U5: U1 confirms the Quest Item row now reports `Shard of Falsehood [29]` (`TextWithId`), not a raw ordinal. U2 is a table-driven walk of every row in all 12 current categories asserting `Kind != None` for every stateful row and `Kind == None` only for the real action rows (Teleport now, Heal/Clear/Revive party, the four Equipment shortcuts, Kill Shadowlords, Clear overworld enemies, and the wholly-action Shortcuts/Presets and Diagnostics categories). U3 moves the cursor away from a row and confirms `row_value()` for the now-unselected row is unchanged — the direct regression guard for the old cursor-only presentation bug. U4 confirms the Character row renders the party member's real name (`"Avatar"`) and falls back safely (`"Character 1"`) for a blank roster slot. U5 confirms the Transport Mode row renders `"Foot"`/`"Horse"`, not a raw integer, including while the row is mid-edit. 146 checks total (up from 23 pre-batch).
- `DeviceDebugScreen`/`AlphaRuntime` remain outside host-test reach (ESP-IDF headers, per Y-05) — no device-format seam was added for this batch, per its own guidance not to build a large seam just to reach it. `row_value()`'s per-row, cursor-independent correctness (verified above) is the primary acceptance target the batch specified for this gap.

**Final suite result:** `native/core/build-batch1-control` — **64 total, 63 pass, 1 fail**, sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `debug_labels` (124/124, new), `ui_debug_menu` (146/146), `debug_map_picker` (62/62), and `debug_developer` (46/46) are fully GREEN.

**Evidence:** [EXEC] `debug_labels_tests.exe` 124/124, `ui_debug_menu_tests.exe` 146/146, `debug_map_picker_tests.exe` 62/62, `debug_developer_tests.exe` 46/46, full `ctest` 63/64 (sole R-21); [STATIC] `debug_labels.h`/`debug_labels.cpp` (new), `ui_debug_menu.h`/`ui_debug_menu.cpp`/`alpha_runtime.cpp` diffs.

---

### R-29 — Developer UI: Special Items unreachable, Quest Items a raw selector, and several already-implemented setters unreachable · **SEVERITY 3 (Developer-tool only)** · **GREEN — RESOLVED (Batch 4.5A-3)**

**Discovery context:** the Batch 4.5A design audit that produced R-28's label/view-model seam also found that several existing, safe `debug_developer` capabilities had no menu path at all: `debug_set_special_item` (Grapple/Spyglass/HMS Cape/Sextant/Pocket Watch/Black Badge/Wooden Box), `DebugQuestNumber::ShadowlordSummoned`, and `DebugResource::Karma`/`TorchTurns`. Separately, `UiDebugCategory::QuestProgression`'s item rows were still the pre-R-28 raw selector/value pair (`"Quest item"` + `"Toggle quest item"`, cursor-indexed 0-5) rather than named rows, and the NPC/Dungeon page offered dungeon slot `0..7` when only `0..6` are valid (`debug_set_dungeon_room_cleared` rejects `dungeon>=7`).

**Root cause:**
- `debug_set_special_item()` (Part of R-28's predecessor batch) was implemented and tested at the API level (`debug_developer_test.cpp`) but never wired into `UiDebugMenu` — there was no `UiDebugCategory` row that could reach it.
- `UiDebugCategory::QuestProgression` conflated two distinct state domains (item possession vs. world/progression numerics) in one category, and its item rows were ordinal-indexed rather than named — the same "meaningless raw ordinal" class of defect R-28 fixed for the Quest Item *label*, still present in the *value* half of that row pair.
- `DebugQuestNumber::ShadowlordSummoned` and `DebugResource::Karma`/`TorchTurns` were fully implemented and validated setters with no `UiDebugMenu` row reaching them — an oversight, not a deliberate scope exclusion (unlike `ActiveCharacter`, which stays excluded per R-24's finding that it is a live gameplay register).
- `NpcDungeonState` row 4 offered `dungeon_slot_` in `[0, 7]` while only 7 dungeons (slots `0..6`) exist; slot 7 was always rejected by `debug_set_dungeon_room_cleared`'s own `dungeon>=7` guard, silently discarding the tester's selection with no menu-level indication it was invalid.

**Fix:**
- New `UiDebugCategory::SpecialItems` (7 explicit rows: Grapple, Spyglass, HMS Cape Plans `[33]`, Sextant `[34]`, Pocket Watch `[35]`, Black Badge `[36]`, Wooden Box `[37]`) and `UiDebugCategory::QuestItems` (6 explicit rows: the three shards and three Lord British artifacts, each `"Name [canonical_id]"`) replace `QuestProgression`'s raw item selector. Both categories fire their mutation directly on a single Confirm/SelectIndex — no numeric edit dialog — via new `UiDebugMenu::quest_item_value()`/`special_item_value()` helpers that read possession straight from `GameState` and are shared between presentation (`row_value()`) and mutation (`apply_action()`) so the two can never drift apart. Grapple's label carries `canonical_id = -1` (Klimb-only, never a Use-item id — the exact R-07 confusion) via a new `debug_special_item_label()` table in `debug_labels.h`/`.cpp`, alongside a small `debug_format_item_label()` formatter ("Name `[id]`" / "Name") shared by both new categories.
- Pocket Watch (`DebugSpecialItem::PocketWatch`) is deliberately kept visible rather than hidden: its row reports `DebugRowValue::Kind::Unsupported`, and Confirm calls the real `debug_set_special_item()`, which returns `DebugStatus::Unsupported` (rendered "Result: Unsupported" by the existing device-generic status path) without mutating `GameState` — no fabricated backing state was added.
- **Black Badge semantic boundary (the batch's critical invariant):** the Special Items toggle mutates `GameState::black_badge` (possession) only, through the existing `debug_set_special_item()` setter — it never touches `TurnState::time_spell`. That field, and the "Badge worn!" wear-state transition it drives (id 36 in `quest_world.cpp`'s `use_quest_item()`, which also gates the Palace guard/password behavior), remains reachable only through the real `(U)se` picker, unchanged by this batch. `native/core/tests/debug_developer_test.cpp` proves both halves against the real functions: the debug toggle leaves `time_spell` at `0`, `usable_item_picker_rows()` subsequently lists id 36 once owned, and a direct call to `use_quest_item(ctx, 36, {})` is what sets `time_spell == '\x1d'`. `ui_debug_menu_test.cpp`'s S2 drives the same boundary through real `UiDebugMenu` input (Next to the row, Confirm, Confirm again), proving the toggle is a true on/off switch and not "badge currently worn."
- `UiDebugCategory::QuestWorld` retains `QuestProgression`'s non-item rows (Quest Flag selector/toggle, Shrine Quest/Visited bitmaps, Shadowlord Doom Bits, Kill Shadowlords) and gains one new row, **Shadowlord Summoned**, wired to the pre-existing `DebugQuestNumber::ShadowlordSummoned` setter.
- **Karma** and **Torch Turns** were added to the `Inventory` category (rows 7-8, between Magic Carpets and the equipment/spell/scroll/potion quantity rows, which shifted from indices 7-11 to 9-13), each wired to the pre-existing `DebugResource::Karma`/`TorchTurns` setter. `ActiveCharacter` remains deliberately unexposed (R-24).
- `NpcDungeonState`'s dungeon slot row is now bounded `[0, 6]`, matching the seven real dungeons and the setter's own guard, so the menu never offers an always-invalid selection.
- Root category table (`debug_labels.h`'s `kDebugRootCategoryNames`, statically asserted against `UiDebugCategory::Count`) grew from 12 to 14 entries: `"Quest Items"` and `"Special Items"` were inserted after `"Reagents"`, and `"Quest"` was renamed `"Quest / World"` — the enum itself keeps every other category's ordinal unchanged (`Teleport`..`Reagents` untouched; `Time`..`Diagnostics` each shift by exactly two slots) to minimize churn, per this batch's own guidance.
- **Deferred:** `debug_set_shrine_destroyed()` is not wired in this pass. A clean UI needs a virtue name per shrine index (0-7); the only existing name table (`virtue_name()`) lives in `frontend.h`, a title-screen/character-creation header with no architectural relationship to the Developer debug-menu label seam — pulling it in would blur that boundary for a single lookup. This is a small, well-scoped follow-up (a two-line virtue-name accessor belongs in a neutral home such as `quest.h` or `debug_labels.h` itself), deliberately left out rather than rushed, per the batch's explicit permission to defer it without blocking Special Items.
- Scope: Developer-menu wiring and label metadata only. No setter validation, preset behavior, teleport semantics, or non-Developer gameplay code changed. Certification setups and the Blackthorn capture/MISCMSG integration remain untouched, as directed.

**Regression coverage (RED→GREEN):**
- `debug_labels_test.cpp` gained S4-equivalent coverage: exact name + canonical id for all seven `DebugSpecialItem` values (including Grapple's `-1`), `debug_format_item_label()`'s two formats, the new 14-entry root category count, and the three new category names at their exact indices. 170 checks total (up from 124).
- `ui_debug_menu_test.cpp` gained S1-S9: S1 (Special Items category + exact row labels), S2 (Black Badge toggle on/off through real menu input, `time_spell` unchanged both ways), S3 (Pocket Watch reports `Unsupported` and leaves `GameState` byte-identical, verified via `memcmp`), S5 (Quest Items are six named rows, no raw selector), S6 (a shard and an artifact toggle through `UiDebugMenu`), S7 (Shadowlord Summoned reachable and reaches the real setter), S8 (Karma/Torch Turns reachable and reach the real setter), S9 (dungeon slot max excludes the always-invalid slot 7). U1 and U2 were updated in place to reflect the new category shapes (Quest Items/Special Items rows report real Boolean/Unsupported state, never `Kind::None`, even though Confirm fires immediately). 196 checks total (up from 146).
- `debug_developer_test.cpp` gained the Black Badge semantic-boundary regression described above (possession-only debug toggle, Use-picker visibility, real `use_quest_item()` wear-state transition) plus a `memcmp`-verified Pocket Watch no-mutation check. 50 checks total (up from 46).
- `debug_map_picker_test.cpp` (62/62) and `batch3_group_b_test.cpp`/`ui_mode_regression` (unaffected by this batch, re-run as regression guards) remain fully GREEN.

**Final suite result:** this batch's host verification ran in a temporary GCC 13 / CMake / Make toolchain (the project's normal Zig-based host compiler and the Node/`tsx` TypeScript parity harness were not available in this sandboxed session). `openu5_core` and every reachable test target compiled clean; the four Developer-tool suites are `debug_labels` **170/170**, `ui_debug_menu` **196/196**, `debug_developer` **50/50**, `debug_map_picker` **62/62**. A further 35 non-Developer-tool host tests (all `*_parity`/`*_flow_parity`/`*_regression` targets, `ui_mode_regression`, `batch3_group_a`/`b`/`c`) ran and passed **35/35** with zero regressions. Three tests (`frontend`, `shop_parity`, `dialogue_parity`) and one build target (`shop_flow_tests`) could not be exercised because their binary/generated fixture assets (`game/assets/init.gam`, `build-shops/`, `build-dialogue/dialogue.bin`) were intentionally not copied into the minimal verification tree — an artifact of this session's environment, not a code defect. `batch4_group_a_test.cpp`/`batch4_group_b_test.cpp` (both untouched by this batch) failed to *compile* only under GCC 13's stricter C++17 aggregate-initialization rules (parenthesized aggregate-init, accepted by the project's normal Clang-based Zig compiler, is a C++20 feature under strict GCC) — a pre-existing cross-toolchain gap unrelated to this batch. The Node-based TypeScript parity/drift tests (including `gameplay_parity`, the sole authoritative-suite failure at mismatch 2034/R-21) were not registered at all, since Node was not installed in the verification container, and so were not re-executed this pass. Given the touched files are limited to `debug_labels.h/.cpp`, `ui_debug_menu.h/.cpp`, and their three test files, none of which any TypeScript fixture or drift check depends on, the authoritative `native/core/build-batch1-control` suite is expected to remain **64 total, 63 pass, 1 fail** (sole `gameplay_parity` mismatch 2034/R-21, unrelated, pre-existing, unchanged) — but this expectation has not been directly re-confirmed by running that exact suite in this session, and should be spot-checked with a normal `cmake --build` + `ctest` pass in the project's usual environment before this is treated as certified.

**Evidence:** [EXEC] `debug_labels_tests` 170/170, `ui_debug_menu_tests` 196/196, `debug_developer_tests` 50/50, `debug_map_picker_tests` 62/62, plus 35/35 across the remaining non-Node host suite (GCC 13 verification build); [STATIC] `debug_labels.h`/`debug_labels.cpp`, `ui_debug_menu.h`/`ui_debug_menu.cpp` diffs; no `alpha_runtime.cpp`/device-side changes were required (device presentation is already fully generic over `DebugRowValue::Kind`, per R-28).

---

### R-30 — Developer presets were opaque (no disclosed effect list, no confirm gate), Transport preset silently rigged HMS Cape, and there was no deterministic Certification workflow · **SEVERITY 3 (Developer-tool only)** · **GREEN — RESOLVED (Batch 4.5A-4)**

**Discovery context:** the Batch 4.5A design audit that produced R-28/R-29's label/view-model seam and Special Items/Quest Items wiring flagged two remaining problems ahead of hardware certification: (1) every `DebugPreset` applied on a single Confirm with zero disclosure of what it actually mutated, including several fields (`active_character`, `hms_cape`, `shrine_quest`, `time_spell`, progression flags, the dungeon-room bitmap) that change real gameplay-visible state, not just scratch test scaffolding; (2) `DebugPreset::Transport` set `GameState::hms_cape = true` as a side effect of "prepare a coherent transport test," silently rigging real ship movement timing (`commands.cpp:382-389`) for every tester who ran it, whether or not they were testing HMS Cape; (3) there was no repeatable, deterministic setup for the five hardware scenarios testers actually run by hand every time (ship/sails, dungeon entry, Blackthorn badge possession, flame/shard, shop/NPC schedule) — each required manually chasing several menu pages in the right order.

**Root cause:**
- `UiDebugMenu::action_requires_confirmation()` gated confirmation on exactly two rows (`Equipment`'s Full Test Setup, `ShortcutsPresets`'s Full Test Setup shortcut) — the ten presets reachable from `ShortcutsPresets` rows 5-14 applied immediately on `SelectIndex`/`Confirm`, with no intervening disclosure step and no metadata to disclose even if there had been one.
- `apply_debug_preset()`'s `DebugPreset::Transport` case (`debug_developer.cpp`) set `g.hms_cape = true` and `c.turn.hms_cape_toggle = 1` unconditionally. Unlike `StockedInventory`, which carries an explicit in-code "quest-neutral tools only" contract deliberately excluding HMS Cape/Black Badge/Wooden Box/shards, nothing in Transport's code, tests, or prior audit entries tied it specifically to HMS Cape behavior — it was scope creep from "prepare a ship" to "prepare a rigged ship," never adjudicated.
- No `UiDebugCategory` or core API existed for a named, repeatable hardware-certification scenario; testers composed the same five setups by hand from Teleport/Special Items/Quest Items/Inventory/Transport each time, with no guarantee of hitting the exact same deterministic state twice.

**Fix:**
- New core-owned effect-sheet metadata seam: `DebugEffectSheet {display_name, effects[], effect_count}` in `debug_labels.h`/`.cpp`, with `debug_preset_info(DebugPreset)` and `debug_certification_info(DebugCertification)` accessors, following R-28's established pattern (static/constexpr tables, zero heap, ESP-free, single source of truth). Each of the ten presets and five new Certification setups got a concise, static effect-line list; any mutation to a live gameplay register or meaningful progression state — `active_character`, `hms_cape`/Transport mode, `shrine_quest`/`shrine_visited`, `time_spell`, Word-of-Power flags, the dungeon-room-cleared bitmap, the Shadowlord-dead flags, `InDoom`/`GameWon`, and (newly, for the Shop/NPC Certification setup) the clock — carries an explicit `"LIVE: "` line prefix. `StockedInventory` deliberately carries none (it is quest-neutral by contract, now also by disclosure).
- One reusable confirm/effect-sheet mechanism (PART 13): `UiDebugMenuView` gained `confirming_sheet` (non-null for a preset/Certification row), and `UiDebugMenu::action_requires_confirmation()` now also gates every preset row (`ShortcutsPresets` cursor ≥ 5) and every Certification row. `enter_or_apply()`/`effect_sheet_for_current_row()` populate it; Confirm applies, Cancel/Back discards with zero mutation — the exact "Enter = Apply / Back = Cancel, no accidental apply while navigating" contract the batch specified, reusing the existing `confirming_`/two-step-Confirm machinery rather than a new UI framework. The one pre-existing plain-text confirmation (`ShortcutsPresets`'s Full Test Setup shortcut, a `DebugShortcut` not a `DebugPreset`) is untouched and keeps its old simple string. `AlphaRuntime::debug_screen()` (`native/targets/tdeck/main/alpha_runtime.cpp`) renders the sheet by repurposing the existing row-grid/status presentation structures exactly as directed — no new screen, no device-owned wording: the header goes in `status`, each effect line becomes one `rows[]` entry (capped to `kDebugScreenRows`=9, informational — `selected_row` is set one past the last valid index so nothing highlights), the existing generic row/status renderer in `tdeck_board.cpp` needed no changes at all.
- **Transport preset HMS Cape adjudication (PART 11):** `DebugPreset::Transport` no longer sets `g.hms_cape`. RED characterization (`debug_developer_test.cpp`'s pre-existing "coherent ship/transport preset" check, run against the unmodified source) confirmed the prior behavior; no comment, test, or audit entry justified coupling a generic transport preset to the HMS Cape speed bonus, so the preferred outcome from the batch brief was adopted as-is: generic Transport preset means normal transport, HMS Cape is tested on its own (Special Items, or the new Ship/Sails Certification setup, which also never grants it). `turn.hms_cape_toggle` — inert without `hms_cape` true (`commands.cpp:382`) — is now reset to `0` rather than left at its old, now-misleading `1`.
- **SaveLoad preset live-effect adjudication (PART 12):** the active Quickness effect (`turn.time_spell = 'Q'`, `spell_turns = 42`) is kept, not removed — it is a deliberate persistence fixture exercising round-trip save/load of a live temporary effect, which `debug_developer_test.cpp`'s existing save/load-round-trip assertion depends on. It is now disclosed with an explicit `"LIVE: "` line rather than left silent.
- New top-level `UiDebugCategory::Certification` (appended after `Diagnostics`, so every prior category's numeric index is unchanged) with exactly five rows, each composing **only** existing debug setters and the existing `apply_debug_teleport()` API (new `apply_debug_certification()` in `debug_developer.cpp`/`.h`) — never a session/internal-state patch:
  - **Ship / Sails Test:** Transport = Ship, sane Hull (50)/Skiffs (2)/Wind/Sail Direction; HMS Cape deliberately never granted; teleport to Britain's default entrance (the smallest existing deterministic route to a transport-capable location — no compile-time-known water-adjacent overworld coordinate exists in the debug-map APIs, so per the batch's own fallback guidance this uses the same standard-entry small-map mechanism every other setup uses). Player still presses Yell (Board is inapplicable — transport is already Ship, exactly like the existing Transport preset's own composition style).
  - **Dungeon Test:** composes `DebugPreset::Dungeon` verbatim, then teleports to Deceit (location 33) floor 0 via the real `EnterDungeon` command path (the same production path the Teleport category's own dungeon destinations already use) — `DungeonState` is never patched directly.
  - **Blackthorn Badge Test:** `debug_set_special_item(BlackBadge, true)` only, teleport to the Palace of Blackthorn (location 18) default entrance; `time_spell` is never touched. Effect sheet explicitly discloses "Does NOT wear the badge" and the still-open Blackthorn-capture/MISCMSG integration issue (informational only, not addressed by this batch).
  - **Flame / Shard Test:** grants all three shards, leaves `ShadowlordSummoned`/doom-bits untouched, teleports to Serpent's Hold (location 32) — chosen as the cleanest existing debug-teleport representation, since its Default Entrance floor resolution is the exact subject of Batch 4.5A-1's T2 regression.
  - **Shop / NPC Test:** Gold = 9999, clock = 12:00 (disclosed `LIVE:`, since it directly gates NPC/shop schedules — the entire point of the setup), teleport to Britain's default entrance. No dialogue/shop session is ever touched (none exists in `CommandContext` for this setup to reach).
  - None of the five bypasses required player input: every setup stops at deterministic state + teleport, leaving the real verb (Yell, Use, Talk, dungeon movement) for the tester.
- Ten preset display names moved from a locally-duplicated `"Preset: X"` string array in `ui_debug_menu.cpp` to `debug_preset_info(...).display_name` (single source of truth, matching R-28's category-name precedent) — e.g. `"Preset: Combat"` is now `"Combat Test Setup"`, matching the batch's own worked example verbatim.
- Scope: Developer-menu preset/Certification wiring, effect-sheet metadata, and the Transport/SaveLoad live-effect adjudications above only. No non-Developer gameplay code changed; Blackthorn MISCMSG/capture text (separate 4.5B issue) was not touched.

**Regression coverage (RED→GREEN):**
- `debug_labels_test.cpp` gained P1 (every `DebugPreset` has a non-empty display name and ≥1 non-null effect line, iterated over all ten values), P2 (Combat/Shrine/Transport/Endgame/SaveLoad each disclose ≥1 `"LIVE: "` line; `StockedInventory` discloses none), and C1's metadata half (`DebugCertification::Count == 5`, exact names, non-empty effect lines, explicit "does NOT grant/wear" disclosure lines for Ship/Sails and Blackthorn Badge). Root category count assertion updated 14→15 (`"Certification"` at index 14). 308 checks total (up from 170).
- `ui_debug_menu_test.cpp` gained P4/P5 (driving the real Combat-preset row through `UiDebugMenu`: selecting it opens the sheet without mutating state; a second Confirm applies it; Cancel instead leaves state untouched and returns cleanly to the row list) and C1/C2 (Certification category + row labels; the Blackthorn Badge setup driven entirely through `UiDebugMenu`, asserting `black_badge==true`, `time_spell` unchanged, `location/floor/x/y` at the Palace default entrance, and `context_.blackthorn == nullptr` throughout). U2's category table gained a `Certification` entry (5/5 action rows, `Kind::None`). 222 checks total (up from 196).
- `debug_developer_test.cpp` gained P6 (the Transport-preset HMS Cape RED→GREEN pair: the pre-existing "coherent ship/transport preset" check was first observed failing against the *new* source with the *old* assertion — i.e. proving the removal took effect — then updated to assert `!hms_cape && hms_cape_toggle==0`) plus C3/C4/C5/C6, exercising `apply_debug_certification()` directly against a new `WorldData`/`DungeonContext` fixture (Britain/Blackthorn/Serpent's Hold small maps, eight dungeons with an authored floor-0 ladder entry matching Deceit's real shape): C5 (Flame/Shard grants exactly the three shards, leaves `summoned`/`doom_bits` at their untouched defaults, teleports to Serpent's Hold), C3 (Shop/NPC sets Gold/clock, teleports to Britain), C6 (Ship/Sails leaves `hms_cape` false, teleports to Britain), C4 (Dungeon composes the preset's torch/bitmap/flag mutations and confirms a genuine `DungeonState.active` session at Deceit floor 0, not a patched struct). 67 checks total (up from 50).
- `debug_map_picker_test.cpp` (62/62, untouched by this batch) re-run as a regression guard, unaffected.

**Final suite result:** `native/core/build-batch1-control` (w64devkit GCC 16.2.0 / Ninja) — **64 total, 63 pass, 1 fail**, sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `debug_labels` (308/308), `ui_debug_menu` (222/222), `debug_developer` (67/67), and `debug_map_picker` (62/62) are fully GREEN; every other registered host test (`frontend`, `shop_*`, `dialogue_*`, all `*_parity`/`*_flow_parity`/`*_regression`, `batch3_*`, `batch4_*`, and every `typescript_*_drift` Node/tsx parity check) re-ran and passed with zero regressions from this batch.

**Firmware:** `native/targets/tdeck/build-batch45a4-green` (ESP-IDF 6.1, `idf.py set-target esp32s3` + `idf.py build`) — clean build, zero compiler warnings or errors across the full tree and specifically across the four touched files (`debug_developer.cpp`, `debug_labels.cpp`, `ui_debug_menu.cpp`, `alpha_runtime.cpp`, force-recompiled and re-verified individually). `openu5_tdeck.bin` is 820,912 bytes (`0xc86b0`); the app partition (`0x100000`) has `0x37950` bytes (22%) free.

**Evidence:** [EXEC] `debug_labels_tests.exe` 308/308, `ui_debug_menu_tests.exe` 222/222, `debug_developer_tests.exe` 67/67, `debug_map_picker_tests.exe` 62/62, full `ctest` 63/64 (sole R-21), ESP-IDF `idf.py build` clean (0 warnings/errors); [STATIC] `debug_developer.h`/`.cpp`, `debug_labels.h`/`.cpp`, `ui_debug_menu.h`/`.cpp`, `alpha_runtime.cpp` diffs. Hardware flash/physical micro-check: **not performed in this session** (no physical device attached) — software checkpoint only; see MANUAL_CERTIFICATION.md for the steps a tester should run against real hardware before treating this batch as hardware-verified.

#### R-30 correction pass — Ship/Sails and Flame/Shard Certification setups did not actually satisfy their own hardware-test contract

A design review of the freshly-landed R-30 work found two of the five Certification setups fell short of "the tester still performs the real verb and it actually works":

- **Ship/Sails:** the setup wrote `GameState::transport = Ship` but never `TurnState::transport_tile`, which defaults to `0x1c` (Foot, `turn.h`). `CommandKind::YellSails` (`commands.cpp:949-961`) — the command a real Yell dispatches to while aboard a frigate — gates purely on `(turn.transport_tile & 0xf8) == 0x20` (the frigate tile range) and `position.map.location < 0x80`; it never reads `GameState::transport` or terrain at all. The device layer's own pre-routing mirror (`AlphaRuntime::refresh_session_context()`, `alpha_runtime.cpp`) checks the identical `transport_tile` condition before even dispatching `'y'`. So on the setup as originally shipped, a tester pressing Yell would still see the ordinary word-of-power prompt, never the sails toggle — `GameState::transport == Ship` alone does nothing for this command. **Fix:** the setup now also calls `debug_set_runtime_number(turn, TransportTile, 0x24)` — the exact "hoisted frigate" value real boarding assigns (`transport.cpp`'s `board_transport` ship branch) and this same file's own `DebugPreset::Transport` and `world_flow_adapter_test.cpp`'s real-boarding fixture already use, reused rather than invented. Separately investigated and found *not* to exist anywhere in this repository: a concrete, sourced real-map "open water" (x,y) coordinate (checked `transport_flow_parity_test.cpp`, `world_flow_adapter_test.cpp`, `debug_map_picker_test.cpp`, and every doc under `native/targets/tdeck`) — every ship-adjacent coordinate found is a synthetic test-harness fixture, not extracted overworld terrain, and the real functional gate above is location-content-independent. The teleport target therefore stays Britain's already-evidenced default entrance; what was actually broken, and is now fixed, is state (`transport_tile`), not destination.
- **Flame/Shard:** the setup granted all three shards but teleported to Serpent's Hold's *Default Entrance* (its town-square door) — not a verified flame/ritual position. The real check, `cast_shard_into_flame()` (`quest.cpp:89-107`, invoked from `quest_world.cpp:103` with the party's live `g.position`), requires the party standing exactly at `x==15, y==flame_y[i], location==30+i, floor==flame_floor[i]` (`flame_y[]={9,3,16}`, `flame_floor[]={2,1,-1}`) when the shard is Used — no town's default entrance coincides with any of the three. **Fix:** the setup now uses an *explicit* manual-coordinate teleport (not standard-entry) to Empath Abbey (location 31), floor 1, `(15,3)` — the exact, sourced cell for `i=1`. Per this same table, that chain is the **Hatred shard / Astaroth / Flame of Love** ritual, not Falsehood as this batch's design brief assumed (the Falsehood chain is the Lycaeum, location 30, `(15,9,2)`); the label is corrected here rather than silently perpetuated, cross-checked independently against `re/notes/shadowlord-ritual.md` and `game/src/momentos/defs.ts`'s `FLAME_X/Y/LOCATION/FLOOR` tables, which agree exactly.
- Scope: these two Certification setups' composition and effect-sheet wording only. No other preset, Certification setup, or non-Developer gameplay code changed.

**Regression coverage (RED→GREEN):** `debug_developer_test.cpp`'s C5 (Flame/Shard) was first observed failing dynamically against the corrected source with the *old* assertion (`location==32`) — proving the coordinate actually changed — then updated to assert the verified Empath Abbey cell (`location==31, floor==1, x==15, y==3`). C6 (Ship/Sails) gained two new assertions verified RED→GREEN by temporarily reverting just the `TransportTile` setter and rebuilding: with it removed, `t.transport_tile == 0x24` failed exactly as predicted (check 64); restored, both it and a direct assertion of `CommandKind::YellSails`' literal gate condition (`(transport_tile & 0xf8) == 0x20 && location < 0x80`) pass. `debug_developer_tests.exe` grew from 67/67 to **69/69**. `debug_labels_tests.exe` grew from 308/308 to **309/309** (a new disclosed `LIVE:` line for the Ship/Sails transport-tile mutation). `ui_debug_menu_tests.exe` (222/222) and `debug_map_picker_tests.exe` (62/62) are unaffected regression guards.

**Corrected suite result:** `native/core/build-batch1-control` — **64 total, 63 pass, 1 fail**, sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged).

**Corrected firmware:** `native/targets/tdeck/build-batch45a4-corrected` — clean build, 0 warnings/errors. `openu5_tdeck.bin` is 821,152 bytes (`0xc87a0`); app partition free space `0x37860` bytes (22%).

**Evidence:** [EXEC] `debug_developer_tests.exe` 69/69 (was 67), `debug_labels_tests.exe` 309/309 (was 308), `ui_debug_menu_tests.exe` 222/222, `debug_map_picker_tests.exe` 62/62, full `ctest` 63/64 (sole R-21), ESP-IDF `idf.py build` clean; [STATIC] `debug_developer.cpp` (`kLocationEmpathAbbey`/`kShipSailsTransportTile` constants and comments, `apply_debug_certification`'s `ShipSails`/`FlameShard` cases), `debug_labels.cpp` (effect-sheet wording), `debug_developer_test.cpp` diffs. Hardware flash/physical micro-check: **not performed in this session** (no physical device attached).

---

### R-31 — Transcript auto-scroll forcibly reset the player's view to newest text on every new event, defeating `Shift+Up`/`Shift+Down` review · **SEVERITY 2** · **GREEN — RESOLVED (Batch 4.5C)**

**Discovery context:** hardware retest after Batch 4.5B's presentation fix (Gorn's jail dialogue, Blackthorn's interrogation) found the existing `Shift+Up`/`Shift+Down` transcript paging (`UiSession::handle_input`'s `PageUp`/`PageDown`, present since before this batch) practically unusable during any live conversation: the player could page up, but the very next transcript event — even their own command echo — silently snapped the view back to the newest line.

**Root cause:** `UiSession::push_block()` (`native/core/src/ui_session.cpp`) unconditionally set `scroll_lines_ = 0` on every new block, and the coalesced combat "Blocked! xN" repeat-update path did the same. This is correct *only* when the player is already following the newest text (auto-follow); it silently discarded any manual scroll position the instant any new text arrived, which during Gorn's or Blackthorn's dialogue is essentially every keypress. A secondary defect: `PageUp`/`PageDown` and `wrapped_line_count()`'s default column width used `UiSessionConfig`'s fixed constructor values (`wrap_columns`, `page_rows` — set once at `AlphaRuntime` construction) instead of the actual on-screen row/column count, which varies with the text-size setting and with whether a context bar is reserving space at the bottom of the panel — so even a successful page-up moved by the wrong amount relative to what was rendered.

**Fix:** `push_block()` no longer resets `scroll_lines_`; a new `preserve_scroll_on_growth()` helper (called from `append()`, `append_utf16()`, and the "Blocked!" coalesce path) leaves the offset alone when already at 0 (auto-follow keeps working, T4) and otherwise advances it by exactly the number of new wrapped lines added, so a scrolled-away view holds the same lines on screen until the player pages back down themselves (T3) — a `PageDown` to offset 0 is what restores auto-follow (T2), matching the "chat scrollback" contract the task specified. Because `PageUp`/`PageDown` are handled at the very top of `handle_input()` before any modal/mode dispatch, this was already safe for an active `TextEntry` modal underneath (T5) and dispatches no gameplay command (T1); no change was needed there. Separately, `UiSession::set_transcript_view_metrics(columns, rows)` is a new narrow mirror (same shape as the existing `set_sail_context()`/`set_harpsichord_active()`) that `AlphaRuntime::refresh_session_context()` now refreshes immediately before every key is routed, using a shared `world_transcript_geometry()` helper (`tdeck_board.h`) so the mirror can never drift from what `Board::render()` actually draws for the current text-size/context-bar state; `wrap_columns`/`page_rows` remain the fallback when unset, so every existing host test is unaffected. This is entirely generic transcript-history behavior — no Blackthorn-specific code was added.

**Regression coverage (RED→GREEN):** seven new cases (T1-T7) added to `native/core/tests/ui_session_test.cpp`, driven through real event paths (a real multi-line dialogue conversation, and a real long `BlackthornPrompt` narrative), confirmed RED against the pre-fix source (T3's "position stays put while new text arrives" assertion failed at the exact reset line) and GREEN after. `ui_session_tests` grew from 261 to **271** checks, all passing.

**Corrected suite result:** authoritative `ctest` — **66 total, 65 pass, 1 fail**, sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged) — no change from the baseline going into this batch.

**Corrected firmware:** `native/targets/tdeck/build-batch45c-transcript` — clean build, 0 warnings/errors from any project file. `openu5_tdeck.bin` is 822,160 bytes (`0xc8b90`), up from the prior recorded `0xc8a50` (+320 bytes); app partition free space `0x37470` bytes (22%, unchanged). Hardware flash was not performed in this session (no physical device attached).

**Evidence:** [EXEC] `ui_session_tests.exe` 271/271, full `ctest` 65/66 (sole R-21), ESP-IDF `idf.py build` clean; [STATIC] `ui_session.h`/`.cpp` (`preserve_scroll_on_growth`, `set_transcript_view_metrics`, `wrapped_line_count`/`PageUp`/`PageDown` mirror fallback), `tdeck_board.h`/`.cpp` (`world_transcript_geometry`, `kShopLogRows`/`kSelectorLogRows` hoisted to shared constants), `alpha_runtime.cpp` (`refresh_session_context()`'s new geometry mirror).

### R-32 — Blackthorn capture/interrogation has no distinct scene presentation; the party visibly stays in the ordinary Palace lobby throughout · **SEVERITY 2 (fidelity, not a functional defect)** · **GREEN — RESOLVED (Batch 4.5D)**

**Discovery context:** the user, familiar with the original DOS game, reports the T-Deck's Blackthorn capture/interrogation/punishment/jail sequence is presented very differently from the original: Blackthorn is never visibly staged, interrogation text appears over the ordinary lobby view, the punishment has no distinct visual staging, and jail relocation happens with no transition. This is a **read-only fidelity audit** per this batch's scope; no scene-presentation code was written.

**Findings (phase-by-phase, original/reference vs current T-Deck):**

| Phase | Original DOS (`re/notes/blackthorn.md`, `re/notes/blackthorn-escena-324.md`) | TypeScript reference (`game/src/core/world/blackthorn-capture.ts`, `blackthorn-scene.ts`, `game/src/ui/blackthorn-scene-pacer.ts`) | Native core (`native/core/src/blackthorn.cpp`) | Current T-Deck runtime | Missing behavior |
|---|---|---|---|---|---|
| Blindfold/blackout | Real viewport blackout (`set_color(0)`+`fill_rect`) after "subdued and blindfolded" text | `buildBlackoutIntroScript` beat with a `blackout:true` flag | Plain `Message` text only, no state/visual change | Ordinary world view unchanged | Any blackout/transition cue |
| Throne room + staged actors | Private 11×11 room loaded from MISCMAPS.DAT (`g_location=0xFF`); party seated, 2 guards scripted in, Blackthorn "fizzles in" via a bytecode VM (`anim_vm`) | `BlackthornScenePacer` mounts a dedicated `BlackthornSceneView` (grid+figures) that governs the mode like other scenes, built from VM-derived beat scripts | No scene/actor/tile struct exists anywhere; `GameEventKind` has no scene-kind event at all | `AlphaRuntime::render()`'s presentation dispatch recognizes only `combat`/`dungeon3d`/`world` — Blackthorn always renders via ordinary `compose_world_presentation` | A 4th presentation source; a native scene/beat event type; the MISCMAPS scene grid asset (packed for neither engine's asset list on the native side — see below) |
| Interrogation Q&A | Prints + `getkey` waits, hourglass tile visibly draining per round | Prints + `shrine-key-wait` + `buildHourglassScript` tile patch | `BlackthornPrompt` text event only, 4-round loop, no tile mutation | `UiSession::consume()` calls `enter_shrine_mode()` + `begin_text()`; underlying view stays "world" (Batch 4.5B already fixed the narrative/prompt split, but not the view) | Hourglass visual, any non-text pacing |
| Punishment/pendulum | Torture-table tile plot, siren SFX, `explosion_fx_at_cell` visual burst at the victim's cell | `buildSacrificeScript` + a `cell-explosion` scene event | `sacrifice()` mutates `GameState` roster directly; only three `Message` strings emitted, no FX event of any kind | No FX rendered — same lobby view throughout | An FX/scene event; native has no event type to carry one |
| Jail relocation | Deterministic `(10,7)`, location 18, floor `0xFF`→basement (asm-verified) | `finishCaptureDeposit` — identical coordinates | `deposit()`: `c.game.position={{10,7},{18,-1}}` (hardcoded, matches) | `GameEventKind::MapChanged`+`PartyChanged`; ordinary redraw of floor −1 as a plain world floor | Any transition/fade marking the jump; jail floor renders as just another floor of the same building |

**Semantic vs presentation-only, for scoping:** the *semantic* events (mantra match/mismatch, `sacrifice()`'s roster mutation, `deposit()`'s position write, karma/quest-flag effects) are unchanged and reference-faithful in native `blackthorn.cpp` and already covered by `batch45b`/`batch4_group_a`/`quest_parity`. Everything found missing above — blackout, staged actors, the throne-room map, hourglass/pendulum tile animation, the sacrifice FX burst, and a jail-entry transition — is **presentation-only staging**, not a core-state gap. `game/src/skin/coreview.ts:912`'s own comment ("manda sobre el modo, como las demás escenas") confirms even the reference treats this as a rendering-layer concern layered over the same core engine, not a different set of game rules.

**Assets:** the Palace of Blackthorn's basement/jail floor (`smallmaps.json` location 18, floor `-1`) is already packed for the T-Deck (`native/tools/u5pack/alpha1.ts`'s `smallMaps()` packs every floor unconditionally) and resolvable by `get_active_map()` — so jail *placement* already works, it simply has no distinct visual identity from the lobby (same top-down world rendering, just a different floor). The original's private throne-room scene asset (the TS reference's `shrine-scene.json`, produced by `extractor/src/parsers/shrine-scene.ts`) is **not referenced by any native packer at all** — confirmed by exhaustive search of `native/tools/u5pack/`, `native/core/include/`, and `native/core/src/` for "shrine-scene"/"ShrineScene"/"MISCMAPS": zero hits. A faithful native scene would need this asset (or an equivalent hand-authored native representation) packed for the first time.

**Fix (Batch 4.5D) — the smallest faithful scene system for this one sequence, and nothing else.**

**Reconstructed reference sequence (the phase table this pass was built from).** Sources: `re/notes/blackthorn-escena-324.md` (a complete static derivation of `anim_vm` 0x00be and the five scripts, with the bytecode quoted), `re/notes/blackthorn.md`, and the TypeScript port of exactly that material in `game/src/core/world/blackthorn-scene.ts` / `blackthorn-capture.ts` / `game/src/ui/blackthorn-scene-pacer.ts`. Beat counts below are the expansion of the cited bytecode, not estimates.

| Phase | Semantic state | Player-visible scene | Actors/assets | Input state | Exit condition |
|---|---|---|---|---|---|
| 1. Capture trigger | `blackthorn_turn_effect(Capture)` — hostile Palace guard adjacent, no Badge. `pick_interrogation_shrine` chooses the shrine; no position write yet | "subdued and blindfolded!" print (0x0652), then the viewport blacks out (0x0676 `set_color(0)` + 0x0689 `fill_rect(8,8,183,183)`) | none — blackout only | swallowed (the binary is inside its own routine) | pause(2) + 5 × `delay_ticks_int1c(5)` drags = 27 units (6 beats) |
| 2. Drag | none | "Strong guards drag thee away!" (0x06b0), still blacked out | none | swallowed | 18 further drags (0x06b9-0x06e0) |
| 3. Throne-room mount | none | The room is loaded (MISCMAPS.DAT record 0, `g_location=0xFF`) and the party is seated in its manacles: seat table DS 0x1F0A/0x1F42/0x1F48, sprite by class letter DS 0x701A → DS 0x1ADE. Then "chained and manacled!" (0x07dc) | 11×11 room grid; party sprites 0x140/0x144/0x148/0x14C | swallowed | mount beat pause(0x10), then pause(0x32) (0x07df) (19 + 1 beats) |
| 4. Guards enter | none | "Footsteps!" (0x07ea), `beep_delay(8)`, both guards written in at the south door and marched by **anim_vm 0x3702** to (1,9)/(9,9) | Guard1 0x170 ×2 | swallowed | script end, pause(8) |
| 5. Blackthorn materializes | none | `tone_sweep` 0x082b, the holy circle 0x116 at (5,5), the LFSR fizzle 0x1068, then Blackthorn1 0x178 (0x0863) | Blackthorn 0x178 at (5,5) | swallowed | pause(8) (17 beats for phases 4-5 together) |
| 6. Greeting | none | Blackthorn's greeting with the Avatar's name (0x087f-0x0891) | room stays up | **getkey 0x0894** | a key |
| 7. Guard release feint | none | "GUARD! Release this good …" (0x0897), then **anim_vm 0x370e**: guard A leaves (1,9) and walks to (1,5), towards the Avatar | guard A moves | swallowed during the script | script end (7 beats) |
| 8. "Wait!" | none | MISCMSG rec11 (0x08c6) | room stays up | **getkey 0x08cd** | a key |
| 9. Interrogation round | `blackthorn_action(Answer)`; `s.round` 0..3 | the question over the **still-visible** throne room | room + full cast | `BlackthornPrompt` → text entry, prompt `"Your response?"` | a typed answer |
| 10. First wrong answer, party>1 | `warned` armed | rec7, then **anim_vm 0x36da** (0x0523): guard A marches the companion from (7,5) to the torture table (5,7), the object is cleared and the cell becomes 0x82; guard B plants the full hourglass 0xE9 at (5,9). Then rec8 + the companion's name + ` die!" ` | torture table, hourglass | swallowed, then **getkey 0x053f** | script end (30 beats) + a key |
| 11. Later wrong answers | `advance_clock(2)` per failed round 1-3 | the sand falls: (5,9) → 0xEB (round 1, 0x05da) / 0xE8 (round 2, 0x05e2) | one tile patch | prompt again | next question |
| 12a. Betrayal (mantra given) | shrine destroyed, karma −5, companion sacrificed when party>1 | rec5 inside `sacrifice_member(0)`, pause(10), the mirrored siren sweeps, `explosion_fx_at_cell` at slot 1's **last** coordinates, the table left as 0x80 | explosion FX | **getkey 0x0510** | 3 beats + a key |
| 12b. Dungeon (Avatar alone, fails) | no sacrifice | rec10 | — | **getkey 0x0510** | a key |
| 12c. Pendulum (4th round, party>1) | companion sacrificed | rec4, the same sacrifice staging, "… is sliced in half! ", **getkey 0x04f6**, rec6 | explosion FX | a key | — |
| 13. Exit | `deposit()`: keys=0, (10,7), location 18, floor −1 | 12a/12b: **anim_vm 0x369e** — the west secret door opens to floor, guard A escorts the Avatar out to the cell corridor, the door shuts, Blackthorn and both guards leave by the south door, six fading footsteps (41 beats). 12c: **anim_vm 0x3716** — Blackthorn alone walks out, but only while slot 8 is still standing (the 0x08d9 gate) (8 beats) | — | swallowed | the script's dismount; the deposit (0x08e7) restores the screen |

**Implemented architecture.** Semantics stayed in core and drawing stayed in presentation, as the batch required; `blackthorn.cpp`'s rules — mantra matching, `sacrifice()`, `deposit()`, karma, quest flags, the clock penalty — are **byte-for-byte unchanged**, and the only edits to that file interleave new *additive* events between the same prints it already emitted.

- **New core module `native/core/src/blackthorn_scene.cpp` + `include/openu5/blackthorn_scene.h`** — a 1:1 native mirror of the reference's `blackthorn-scene.ts`: the seat/class tables, the tile constants, a small `runAnim` equivalent, and the nine script builders (blackout intro, throne mount, chained pause, entry, guard release, warning, hourglass, sacrifice, finale, Blackthorn exit). Pure: no `GameState`, no RNG, no clock. Each beat carries the **complete** cast snapshot after it, so no presenter needs a VM of its own.
- **New `GameEventKind::BlackthornScene`** with a `const BlackthornSceneScript *blackthorn_scene` payload on `GameEvent` (`movement.h`), borrowed for the synchronous delivery exactly like `endgame`/`refuge`/`troll_sneak`. The existing `ShrineKeyWait` and `CellExplosion` kinds carry the getkey points and the sacrifice burst — no new event kinds were needed for those.
- **New `BlackthornSceneServices` on `CommandContext`** (`capture_tiles`, a live `BlackthornSceneState`, a per-segment script scratch). This is the **discriminant**: with it absent — or with `capture_tiles` null — `blackthorn_action` emits precisely the text-only stream it emitted before this batch, which is why every parity fixture, scripted quest and pure harness is byte-identical (the same gate the reference uses via `ctx.captureTiles`).
- **New `openu5::BlackthornScenePacer`** (same header/TU), the native counterpart of `ui/blackthorn-scene-pacer.ts`. `blackthorn.cpp` still emits the whole turn synchronously; the pacer takes ownership from the first scene event onward and releases it beat by beat through an `EventSink` — normally `UiSession`'s — in the original order. It copies every borrowed payload (notably message text, which is a `std::string` temporary in `blackthorn.cpp`'s emitter) into caller-supplied storage.
- **New `compose_blackthorn_presentation()`** (`presentation.cpp`'s header, implemented beside the scripts) bakes the scene into the ordinary `PresentationSnapshot`, so the existing rasterizer draws it with **no new drawing code at all**: the blindfold is a window of `kPresentationHidden`, which `render_snapshot` already leaves black, and the throne room is the grid with the cast composited on top. `tdeck_board` is untouched; the device layer never sees a scene struct, let alone a gameplay internal.
- **Device (`alpha_runtime.cpp`)** — a fourth presentation source in `render()`'s dispatch (`blackthorn-scene`, ahead of `combat`/`dungeon3d`/`world`, since a cutscene must outrank the lobby it replaces); `consume_event()` offers every event to the pacer before `UiSession`; `service_blackthorn_scene()` pumps it once per frame off the same 55 ms unit the tile animator uses; the scene's queue, text arena, working grid and script scratch are PSRAM like every other large buffer; a load arriving mid-scene cancels it (`synchronize_loaded_world()`).

**Pacing behaviour (and the one deliberate device adaptation).** `frames` are run-n-frames units at `PAUSE_UNIT_MS = 55` — the reference's own calibration, not an invented delay; the capture intro therefore runs ~13.6 s of scripted staging, which is the binary's own pause budget (27 + 106 + 50 + 42 + 23 units). Automatic advance happens exactly where the reference auto-advances, and the five `getkey_with_redraw` points wait for a key exactly where the binary waits. Input is **swallowed** while a segment is running — the same rule the reference pacer and the shrine rite use — with two exceptions: transcript paging (Shift+Up/Down), which touches no scene state and dispatches nothing, and Confirm at a getkey point. The one adaptation is an **overlay cue** (`"Enter: continue"`) at the getkey points: a DOS player simply pressed a key, but on the handheld an uncued wait looks like a freeze. It is a device affordance, not transcript text, so the narrative corpus is unchanged.

**Known inherited nuance, declared:** the hourglass tiles 0xE8-0xEB fall inside the shared animator's 232-239 `TileCycle` group, so the drained frame the escalation writes is cycled by the same tile animation that drives every other animated tile in the port. The semantic patch is applied at the reference point either way; this is the shared animator's existing classification, identical in the TypeScript reference, and was deliberately **not** special-cased here.

**Assets / resource pack.** The throne room (`shrine-scene.json` key `capture`, the extractor's MISCMAPS.DAT record 0) had never been packed for the native engine — confirmed by the 4.5C audit above. It is now packed through the normal pipeline as a new `blackthorn-scene.bin` entry in `native/tools/u5pack/alpha1.ts` (cols, rows, then cols×rows `int16` tiles, row-major), added to `AlphaResourcePack::load()`'s required-entry list and validated for exact 11×11 shape before use. Nothing is embedded as a literal in device code. The pack went from 1,843,143 to **1,843,457 bytes** (+314: a 64-byte TOC entry plus 8 + 242 bytes of payload) and from 33 to 34 entries — still inside the 64-entry bound raised in Batch 4.5B. `kExpectedAlphaResourceSize`/`Crc32`/`Sha256` were updated together. **An SD recopy is required this time.**

**Regression coverage (RED→GREEN).** New native test `native/core/tests/blackthorn_scene_test.cpp`, ctest target `blackthorn_scene`, **57/57**. Every case drives the real entry points (`blackthorn_turn_effect(Capture)`, `blackthorn_action(Answer)`) through a real `UiSession` behind a real pacer, and reads the throne room out of the **real packed resource file** rather than a private literal — so a packer regression fails here instead of on hardware. T1 scene begins on capture, the composed presentation stops being the world, and no gameplay position is rewritten to draw it; T2 beat order (blackout → drags → mount); T3 Blackthorn staged at (5,5) with tile 0x178, guards at (1,5)/(9,9), the party in the two manacles with their class sprites; T4 pacing — exactly one line released before the blindfold beats, the drag line withheld for the full 27 units; T5 the prompt is live, `"Your response?"` exact, scene still mounted, prior narrative still reachable by scrolling; T6 the deterministic wrong answer reaching the warning consequence with the companion on the table and the hourglass planted; T7 victim staging with the roster mutation at the same reference point and the explosion aimed at the retained coordinates; T8 clean exit to (10,7)/loc 18/floor −1 with the pacer idle; T9 paging mid-scene changing nothing else; T10 the immediate-subdual branch untouched; T11 the packed resource. Confirmed **RED** before the fix by reverting only `native/core/src/blackthorn.cpp` to its pre-batch state while keeping the new module compiled: checks 6, 7, 9, 12, 13, 14 and 15 failed (no scene on the air, the seven-line burst, no blackout, no mount) — exactly the reported hardware defect — then GREEN once restored.

**Scope discipline.** **R-23 (Blackthorn sacrifice roster compaction) was deliberately not touched.** `sacrifice()` is byte-for-byte unchanged and T7 asserts the roster mutation still happens at exactly the same point with the same effect; the scene only *shows* it. *(Forward pointer, added in Batch 15: that routine is now named `sacrifice_first_companion()` and R-23 was resolved there. This paragraph's claim about the Batch 4.5D pass is unchanged and still accurate for that pass.)* R-14 (world-object save/load persistence) is likewise untouched. No unrelated cutscene was redesigned: the shrine rite, the endgame and the refuge paths are unchanged, and `RefugeScript`'s pre-existing emitted-but-unconsumed dead path is still open and still out of scope.

### R-33 — `searchObjects` basement entries compare the raw DATA.OVL floor byte (255) against the runtime floor convention (−1), so 5 basement search spots — including Gorn's brazier keys — can never be found · **SEVERITY 2** · **GREEN — RESOLVED (Batch 4.5C.1)**

**Discovery context:** hardware finding — Gorn (Blackthorn jail) tells the player he hid keys in the brazier; searching the brazier returns "Nothing of note" instead of granting keys.

**Root cause:** the reference `searchObjects` table entry for the brazier (`game/assets/data.json`, index 13: `{id:7, quality:9, location:18, floor:255, x:8, y:6}`, meaning 9 regular keys) stores `floor` as the raw, unnormalized DATA.OVL byte (`0xFF`/255) extracted by `extractor/src/parsers/dataovl.ts`. But every runtime representation of that same basement uses the signed convention **`-1`**, not 255: `smallmaps.json`'s location-18 floor list is `[-1,0,1,2,3]`; the reference's own `blackthornCaptureDeposit()` (`game/src/core/world/blackthorn.ts`) sets `position.floor = -1` with a comment documenting the exact `0xff→-1` mapping; native `blackthorn.cpp`'s `deposit()` likewise hardcodes `-1`. A `normZ`-style byte-255→`-1` alias already exists for NPC schedule floors (`game/src/core/npc/manager.ts`) and for the Look command's basement signage (`native/core/include/openu5/look.h`'s documented alias), but **no equivalent alias is applied before the search-object floor comparison**, in either engine: `game/src/core/world/search.ts`'s `searchAt()` and native `native/core/src/quest_search.cpp`'s `search_at()` both compare the table's raw `255` directly against `position.map.floor` (which is `-1` for anyone standing in the jail), so the match can never succeed and the code falls through to "Nothing of note" every time.

**Classification: wrong coordinates/floor** — specifically a floor-representation mismatch (raw extracted byte vs. runtime signed convention), **not** data missing from the native pack, **not** a missing prerequisite flag (Gorn's dialogue — `game/assets/talk/castle.json` npcIndex 11 — sets no quest flag when he mentions the brazier; the reference design is unconditionally findable once you know to look, gated only by "currently hold 0 keys and the target cell is unoccupied," which is itself correctly implemented at `quest_search.cpp:10` but unreachable behind the floor check), **not** wrong Search target-cell semantics (Search already correctly resolves the direction-targeted cell before the table lookup), and **not** the already-tracked R-14 (R-14 is about placed/found search objects not surviving save/load — an orthogonal persistence gap that presumes the search already succeeded). Confirmed present **identically** in the TypeScript reference and the native pack — `native/tools/u5pack/alpha1.ts`'s `worldTables()` copies `data.json`'s `searchObjects` verbatim, so this is a pre-existing reference-engine defect the native port faithfully inherited, not something the port introduced. Four other basement entries share the identical defect: index 46/47 (Yew) and 73/74 (Lord British's Castle) — none of the Grand Tour end-to-end fixtures exercise a Search while standing on floor `-1` against one of these five entries (`game/e2e/grandtour/ch08-yew.spec.ts` explicitly notes the basement search case is skipped as unreachable in that chain), so this was never caught by existing test coverage on either side.

**Fix (Batch 4.5C.1):** normalized at the one shared comparison boundary in each engine, with an explicit signed-conversion rule rather than a scattered `255 || -1` special case: `decode_authored_floor(location, raw)` (native, `quest_world.h`/`quest_search.cpp`) and `decodeAuthoredFloor(location, raw)` (TypeScript, `game/src/core/world/search.ts`, exported for direct testing) both decode `0x00..0x7F` to themselves and `0x80..0xFF` to their two's-complement negative (`0xFF -> -1`) — but **only when `location !== 0`**. That location guard is the fix's critical correctness detail, confirmed by tracing `types.h`'s own documented convention ("Small-map basement floors may be -1; large-map underworld is explicitly 255") and `world.cpp`'s `get_active_map()` (`id.location==0` selects Underworld precisely when `id.floor==255`): for the WORLD map, floor 255 is itself the correct, already-meaningful runtime floor for the Underworld and must never be decoded, or every Underworld search object (data.json indices 0-11, all `location:0`) would silently stop matching. `search_at()`/`searchAt()` now decode the table entry's floor before comparing to the party's real position; native's `isFindable`-equivalent gate logic was unaffected (its "cell occupied" check already used the current real position floor, never the table's raw value) but TypeScript's `isFindable`/`cellFree()` needed the identical decode applied to the `occupiedAt` callback's floor argument, since it does read the table entry's raw floor directly.

**Root cause confirmed exactly** per this batch's read-first requirement: `search_at()` (`quest_search.cpp:9`, pre-fix) and `searchAt()` (`search.ts:358`, pre-fix) were the sole two places `SearchObject::floor` / `SearchObject.floor` was ever compared against a runtime floor — confirmed by exhaustive grep of both engines before any code was written.

**Both engines fixed together, deliberately:** a native-only fix was evaluated and rejected. `native/core/tools/check-quests.ts`'s own `searchObjects` parity sweep (line 170) sets the scripted party's position floor to the table's *raw* value (e.g. `255`, not `-1`) for every entry, and `game/tests/search.test.ts` had seven existing assertions that queried the Blackthorn jail with the same raw `255`. Fixing only one engine would have made native and the live TypeScript reference disagree on these exact inputs, breaking `quest_parity`/`gameplay_parity`'s dynamic (not golden-file) equality check. Fixing both identically keeps every existing scripted scenario self-consistent (both sides now agree, even where a scenario still queries a floor that never occurs in real play) while genuinely correcting the one scenario that does occur in real play: querying with the true runtime floor `-1`. `game/tests/search.test.ts`'s seven Blackthorn-jail query sites were updated from the raw `255` to the real `-1` to match (the raw-value assertion on the *stored table entry* itself, `toMatchObject({..., floor: 255, ...})`, is untouched — the data is still authored as 255; only how it's interpreted at the comparison changed). A new test was added confirming the WORLD-map Underworld convention (`location:0, floor:255`) is untouched by the fix.

**Regression coverage (RED→GREEN):** a new native test, `native/core/tests/quest_search_test.cpp` (15 checks: the `decode_authored_floor` helper directly; Gorn's brazier keys found at the real floor `-1` and confirmed *not* found at the raw `255`; a second independently-affected basement entry (Yew, index 46); an ordinary floor-0 entry proven unaffected; and the WORLD-map floor-255-is-Underworld invariant in both directions), registered as ctest target `quest_search`. Confirmed RED against the pre-fix source (temporarily reverted via `git stash`) — failed exactly at "T1: Gorn's brazier is found at the real runtime floor -1" — then GREEN (15/15) after restoring the fix. `game/tests/search.test.ts` gained one new test (Underworld convention guard) and is **25/25** (was 24/24); the full TypeScript `vitest` suite shows the same ~98 pre-existing, unrelated failures (sandbox symlink permissions, a hardcoded-path lint check, and similar environment-only issues — none touching `search`/`quest`/`blackthorn`) both before and after this change, confirmed by inspecting every failing test name.

**Corrected suite result:** authoritative `ctest` — **67 total, 66 pass, 1 fail** (up from 66 total, 65 pass, 1 fail — the +1/+1 is the new `quest_search` target; the sole failure remains `gameplay_parity` mismatch 2034, R-21, unrelated, pre-existing, unchanged). `quest_parity` (23 s) and `gameplay_parity`'s only failure staying at mismatch 2034 both confirm the dual-engine fix introduced no native/reference divergence anywhere in the existing scripted suites.

**Corrected firmware:** `native/targets/tdeck/build-batch45c1-search-floor` — clean build, 0 warnings/errors from any project file. `openu5_tdeck.bin` is 822,208 bytes (`0xc8bc0`), up from the prior recorded 822,160 bytes (`0xc8b90`, +48 bytes); app partition free space `0x37440` bytes (22%, unchanged). Hardware flash was not performed in this session (no physical device attached).

**Scope discipline:** R-14 (world objects never serialized/cleared on save-load) is a separate, unrelated persistence gap and is explicitly **not** touched or marked resolved by this fix. R-32 (Blackthorn scene presentation fidelity) remains open, unimplemented, and scoped for a future 4.5D pass — nothing in this fix changes the Blackthorn capture/interrogation/punishment staging, only the brazier's own search-floor comparison.

## 4. YELLOW / UNPROVEN AREAS

| ID | Area | Why unproven | Missing evidence |
|---|---|---|---|
| Y-01 | Movement Mode in frontend menus | `mode_before` is forced to `InventorySelection`/`TextEntry` for the frontend (`handle()`), and `is_movement_context` excludes selections | Decide whether WASD should navigate the title menu; today only the trackball and hotkeys work |
| Y-02 | Trackball centre click | GPIO 0 not wired in `tdeck_input.cpp` | Decide whether click = Confirm |
| Y-03 | Audio | `GameEventKind::Sfx` unconsumed; `sound_volume`/`music_volume` settings are reserved | Declared out of scope for Alpha 2.0 — treat as N/A but note the dangling settings |
| Y-04 | Feedback VFX | **GREEN — RESOLVED (Batch 7B).** All six channels now have reference-faithful consumers: `Quake` (Batch 7) and, in Batch 7B, `CellExplosion`, `CellProjectile`, `PoisonTick`, `Refuge` and `TrollSneak`. Per-channel behaviour is recorded in the Batch 7B section of §14, not summarised as "renderers added". | Closed. The three primitives introduced are `WorldFxLayer` (shared by `CellExplosion`/`CellProjectile` -- they ARE one primitive in the reference), `PoisonFlashPacer` + `roster_invert_row` (the shared row-inversion the picker cursor and combat hit also belong to), and `NarrativeScenePacer` (the one sequencer `Refuge` and `TrollSneak` share). The `#243` ordering question is answered the way the reference answers it -- `under_tile` holds the sprite on the cell for the whole choreography, and no committed state mutation is deferred. **Remaining exposure is device-side only** (Y-05/Y-07): the pacers and the layer are host-tested, the pixel work in `alpha_runtime.cpp`/`native_renderer.cpp`/`tdeck_board.cpp` is not, and no channel has been observed on physical hardware. |

#### Y-04 partial resolution (Quake) -- Batch 7 · *superseded: Y-04 is fully GREEN as of Batch 7B (see §14)*

**Adjudicated against the reference first** (`game/src/skin/fiel/quake.ts`'s witness-derived dynamics: 2px vertical amplitude, square wave, 8 pulses of a 117ms period (42ms down + 75ms rest); `game/src/skin/fiel/skin.ts`'s `applyTurnFx`, which collapses every `{kind:"quake"}` event of a turn into **one** `trigger(now, count*QUAKE_PULSES)` call via `planTurnPhase` -- a sustained shake, not N overlapping ones). Native already emits `GameEventKind::Quake` correctly in three places (`shrine.cpp`'s ritual, `quest_world.cpp`'s harpsichord passage and shard-destruction paths); the entire gap was the missing consumer, exactly as the audit's original framing said.

**Implemented:** `quake_offset_at(t_ms, pulses)` (`native/core/include/openu5/presentation.h`/`.cpp`) is a pure square-wave function matching the witness dynamics exactly, with `kQuakeAmplitudePx`/`kQuakePulses`/`kQuakePeriodMs` as named constants instead of magic numbers. `AlphaRuntime` (`alpha_runtime.{h,cpp}`) arms a wall-clock timer on `GameEventKind::Quake`; because `consume_event` sees events one at a time rather than as a pre-counted turn batch, a retrigger while a shake is still running **extends** the pulse count (`quake_pulses_ += kQuakePulses`) instead of resetting the clock, reconstructing the same total sustained duration the reference's batch-counting `applyTurnFx` produces for e.g. the three-Quake-events shard-destruction sequence (`quest_world.cpp:106`). `shift_viewport_vertically` (`native_renderer.{h,cpp}`) re-blits the already-rendered 11x11 game window the computed offset lower, clamping the vacated top rows to the window's own edge (Class-C, not asm-derived, but never a bare black bar); it runs as a post-process alongside the existing `magic_inverted` XOR post-process, so it applies uniformly over whichever presentation source (world/combat/dungeon/gem-view) rendered that frame, matching the reference's own skin-level overlay. `animation_only`'s periodic 55ms redraw pump now also runs while a quake is active (it previously only watched tile-animation visibility), because the shake oscillates within its own window and needs continuous re-rendering, not just a dirty flag on the start/stop transition.

**Tests:** `native/core/tests/presentation_test.cpp` (ctest `presentation_regression`) -- the waveform (silent before start, 42ms down phase, 117ms period, silent after 8 pulses, and the sustained 3x-shake case staying active past where a single default trigger would have stopped). Confirmed **RED** (compile failure) via the same `git stash` method as R-12 above, then **GREEN**.

**Not host-tested, by necessity:** the timer/extension logic and the pixel shift itself are ESP-only (`alpha_runtime.cpp`, `native_renderer.cpp`), the same already-documented gap as every other device-side timed effect in this project (Y-05). Physical-device confirmation is outstanding (no hardware in this session).

**Scope discipline:** `CellExplosion`, `CellProjectile`, `PoisonTick`, `Refuge` and `TrollSneak` were deliberately not touched in this pass -- see the table row above and §14 Batch 7 for why each needs more than a consumer wired onto an existing pattern.
| Y-05 | `AlphaRuntime` coverage | No host test instantiates it (ESP-IDF headers) | Highest-leverage test gap in the project — see §7 |
| Y-06 | Device smoke tests | 37 scenarios, overwhelmingly data-presence assertions + `UiSession`-with-spy probes | None validate a renderer output or a full route |
| Y-07 | Broad device presentation | No automated device evidence for any gameplay screen | Physical certification pass — see §16 |
| Y-15 | Dungeon `m` key | Aliased to Cast with echo "Cast"; Mix unreachable in dungeons | Confirm reference behaviour for Mix underground |
| Y-17 | Acknowledgements | One hardcoded line vs. the reference credit sequence | Low priority |
| Y-18 | Developer entry from frontend | Calls `frontend_.enter_game()` directly, entering gameplay on whatever INIT.GAM state was loaded at boot | Confirm this is the intended debug affordance |
| Y-19 | Mix quantity | `c.hours = 1` hardcoded in `modal()`; reference lets the player choose a batch size | Add a numeric modal |
| Y-20 | `SetActivePlayer` | **GREEN — RESOLVED (Batch 3) at the core/`UiSession` level.** The reference binds the digit keys `0`-`9` (kernel `0x4080`, via MAINOUT `0xc06` / TOWN `0xe34`), not `N`; `N` stays New Order. `handle_exploration` now echoes `"Set Active Plr:"` and dispatches `CommandKind::SetActivePlayer` with the **literal** digit (`'2'` -> `command.member = 2`), because the core handler performs its own `member - 1` exactly as the kernel takes `key - '1'`. Digit range, party validity and the `None!`/`Invalid!` outcomes stay entirely in that core handler — no new selection modal. At the harpsichord the digit is intercepted first (R-20). The core/session route accepts a literal `'0'` (clear active player) exactly like any other digit; **on T-Deck hardware specifically**, digits `1`-`9` reach this route, but `'0'` cannot, because the only matrix position that resolves to `'0'` is the physical Mic key, which the device input adapter intercepts unconditionally for short=Cancel/long=Movement-Mode before any digit is ever produced. That device-only gap is tracked separately as **Y-29**. | Covered by `batch3_group_a_test.cpp` A4/A5 (core/session level); Y-29 covers the T-Deck hardware reachability gap |
| Y-21 | Rel Hur scroll | **GREEN — DISCHARGED (Batch 5). The stated claim is false.** `AlphaRuntime::modal()`'s Inventory branch (`alpha_runtime.cpp:684`) already routes scroll id 1 (and skull key 17) through `ui_->begin_target(UiRequestId::UseTarget,"Direction?",c)`, and `UiSession`'s TargetSelection direction arm dispatches `UseItem` with `has_direction` set, so `world_magic.cpp` `case 1` sets the wind. Both halves are now pinned host-side: `batch5` B4 (the getdir dispatches `UseItem` + direction) and C6 (`world_magic` spends the scroll and sets `turn.wind`). No production change was needed for the stated defect. | Discharged; the remaining cancel-path divergence is tracked separately as **Y-31** |
| Y-22 | Dungeon→combat→dungeon return | `dungeon_combat_return` handles floor delta, escape border and facing; covered by `dungeon_flow_parity` at core level only | Device round-trip test |
| Y-23 | Dungeon keyboard movement with Movement Mode off | No `w`/`d` fallback; only trackball moves | Probably acceptable, but state it as a deliberate contract |
| Y-24 | Word-of-power Yell | **GREEN — DISCHARGED (Batch 3).** Both branches are regression-tested together: `batch3_group_a` A1 (frigate → `YellSails`, no modal), A2 (non-frigate → `YellText`), A3 (word path → `CommandKind::Yell`). | — |
| Y-25 | Shrines | All three modals dispatch correctly; `UiMode::ShrineSpecial` lifecycle (entry, capture, return) repaired in Batch 1 (§5) — no known mode defect remains | Device pass at a shrine (physical-device validation only) |
| Y-26 | Beds / auto-sleep | `CommandKind::AutoSleep` is core-internal (`commands.cpp:1163`), reached through `townAutoSleepTurn` | Confirm it is genuinely internal-only |
| Y-28 | Combat Ready picker close timing | Two presentation gaps that do **not** affect action cost (see section 3 R-06): the empty-handed case opens a disabled `"(None available)"` picker instead of charging immediately without one, and `ItemResult::vanished` (`"Ring vanishes!"`) does not close the picker early the way the reference does | Close the picker from `AlphaRuntime::modal()` on `vanished`, and short-circuit the empty-handed open |
| Y-27 | System menu over an open modal | Menu is handled before gameplay routing and does not touch `ui_->mode()` | Device round-trip from inside a selection/target modal |
| Y-29 | Physical "clear active player" (digit `0`) is hardware-unreachable on T-Deck | **GREEN — RESOLVED (Batch 16).** `SetActivePlayer` with `member=0` (clears `active_character` back to `255`, `commands.cpp:747`) requires the literal character `'0'`. On T-Deck the only matrix position that resolves to `'0'` (`kSymbol[0][6]`, per `keyboard_matrix.cpp`) is the physical Mic/0 key, and `UiInputAdapter::translate()` intercepted that exact `column==kMicrophoneKeyColumn && row==kMicrophoneKeyRow` position unconditionally, before any modifier check, to implement short-press=Cancel / long-press=Movement-Mode-toggle. A literal `'0'` therefore never reached `handle_exploration`'s digit switch on this hardware. **Fix:** the Mic/0 branch now checks `raw.modifiers.symbol` first (`ui_input_adapter.cpp`); a Symbol-held press bypasses the short/long state machine entirely and emits `UiActionKind::Character,'0'`, which flows through the ordinary digit path every other key already uses (including the harpsichord note-0 interception, R-20/Y-20). A plain (unmodified) Mic/0 press keeps its exact existing short=Cancel/long=Movement-Mode contract — only the Symbol-held edge of the same key changes. This is the pre-existing "recommended, not yet implemented" route below, now implemented as recommended. | Device confirmation that Symbol+Mic/0 physically produces the clear-active-player prompt, and that a plain Mic/0 press is unchanged — see §16 hardware checklist |
| Y-30 | World cast prompt/consumption ordering under the fused `world_magic()` | **OPEN — newly discovered (Batch 5 adjudication), deliberately not fixed.** `world_magic()` performs `cast_spell()` (charge + mana + ceremony) and the direction-dependent effect in **one** call, so the native getdir must be raised *before* the call, while the reference consumes first and prompts after (`main.ts` `doCast` → `pendingCastDoor`/`pendingCastUnlock`/`pendingCastBlink`). Net state matches on both the success and the cancel path (a cancelled world cast still dispatches, so the charge is still spent — `batch5` B3/C3/C5), so this is a **presentation-ordering** divergence, not a state divergence: the result message and the `MagicCeremony` flash land after the getdir instead of before it, and a cast that would fail its own gate (`"Not here!"`, `"None mixed!"`, `"M.P. too low!"`, `"Absorbed!"`) raises a spurious getdir first where the reference prints the failure immediately and never prompts. Most visible on An Ex Por outdoors and In Por in town (both `"Not here!"`). | Split `world_magic` into a cast step and an apply-direction step, then have `AlphaRuntime` dispatch the cast, read the effect, and only then prompt. Core API change — out of Batch 5 scope. |
| Y-31 | Cancelling the Rel Hur (and skull key) `(U)se` getdir refunds the item | **GREEN — RESOLVED (Batch 16).** The reference consumes at item-selection time, *before* the getdir: `readScroll()` runs and prints its messages, then `pendingScrollWind` is armed (`main.ts` `doCast`-adjacent `(U)se` branch), and cancelling leaves the scroll spent with the wind unchanged; the skull key is explicitly the same (`main.ts`: "la llave YA se decrementó al seleccionar el item (0x18c4 va ANTES del getdir)"). Native dispatched nothing on cancel — `UiSession`'s TargetSelection cancel arm sent a bare `ModalResponse{accepted=false}` for a `UseItem` pending command — so ESC at the Rel Hur prompt **refunded the scroll**. **Fix:** the seam turned out to live in `UiSession::handle_modal`'s TargetSelection cancel arm (host-testable, not `AlphaRuntime` as originally guessed — Batch 11 had already made `AlphaRuntime` host-testable too, but the actual pending state lives in `UiSession`), alongside the existing `Cast` cancel arm: a new `use_item` case dispatches the pending `UseItem` with `has_direction=false`, which `world_magic.cpp` already consumes unconditionally on `cmd.item` range for both scroll ids 0–7 and the skull key (17) — no `world_magic.cpp` change was needed, only the dispatch decision. | Discharged |
| Y-32 | Scripted-event temporal / pacing parity | **OPEN — investigated and reclassified (Batch 16); still insufficiently evidenced for a numeric fix.** A manual comparison against original Ultima V footage found that native's **Blackthorn capture scene appears substantially faster and more collapsed than the original**. Batch 16 confirmed by direct archaeology that **no footage-derived timing evidence exists anywhere for this scene**: `re/notes/blackthorn-escena-324.md` §4 explicitly disclaims a witness for the capture scene's cadence ("sin testigo propio para esta escena"), and the TypeScript port's `PAUSE_UNIT_MS` (`game/src/skin/world-fx.ts:58`) is an admitted reuse of TrollSneak's calibration, not an independent Blackthorn measurement. Every frame count in `blackthorn_scene.cpp`'s script builders is the disassembled bytecode's own literal pause argument (cited instruction-by-instruction against `re/notes/blackthorn-escena-324.md` §2), so the numbers are not invented, but they were never checked against real footage for *this* scene the way Y-04's Quake channel was. Batch 16 also found and ruled out one candidate root cause: `BlackthornScenePacer` (unlike `NarrativeScenePacer`) enforces no reading-floor on `Message`/`Forward`/`Prompt` steps — but tracing the actual interleaving in `blackthorn.cpp` (the `event(...);build_*_script(...);emit_scene(...)` sequence) shows every message is already followed by either a genuine blocking `key_wait` or a frame-bearing beat, so this asymmetry does not currently manifest as a dropped or flashed message; it is recorded as an observation, not a diagnosis. **Batch 4.5D (R-32)**, which built the scene's presentation from nothing, explicitly did not attempt any independent pacing calibration either — it reused the same generic `kPresentationUnitMs`/`PAUSE_UNIT_MS`=55ms convention verbatim. | A frame-by-frame video comparison against original DOS footage, scene by scene, measuring the per-beat dwell each one actually holds rather than assuming the 55ms/tick DOS-timer convention transfers unmodified. `BlackthornScenePacer::resume_at_ms()`/`released_steps()` already expose the shape `batch7b_test.cpp`'s D8/E23-style assertions use for Refuge/TrollSneak (`blackthorn_scene_test.cpp`'s T4 already pins *relative* ordering); once real dwell numbers exist, the same absolute-ms assertion style can be added. Do not invent floor values for the Message/Forward/Prompt asymmetry without footage evidence, even though the architecture would support it cheaply. |
| Y-33 | Vas Rel Por (spell 46) never raises its phase-gate ceremony | **GREEN — RESOLVED (Batch 16).** `CAST.OVL 0x0cf0` prints "To phase:", reads the key, and only after the exact `'1'..'8'` gate (`0x0d1d`/`0x0d23`) pushes the literal index 8 (`0x0d2d`) into `CAST2:0x0000`; the three early exits (aboard ship at `0x0cf6`, any non-`'1'..'8'` key) skip both the ceremony and the teleport. The native command modelled none of this — no prompt, no gate, and `Command.hours` (reserved for the phase) defaulted to 0, a *valid* phase, so casting the spell silently teleported to moonstone 0 with no keypress at all. **Fix, four layers deep, mirroring `game/src/main.ts`'s `castGateTravel`:** (1) `cast_target_prompt()` gained a `CastTargetPrompt::WorldPhase` case and an `aboard_ship` parameter (ship gates the prompt itself, matching 0x0cf6 firing before the print at 0x0cff); (2) `UiSession`'s `TargetSelection` mode gained a `Character` arm for the new `UiRequestId::GatePhase` request — exactly `'1'-'8'` sets the phase (0-7), anything else (including Cancel, via the existing `cast` arm) leaves the pre-armed `-1` sentinel; (3) `world_magic.cpp`'s `Gate` branch now fires the ceremony (literal index 8) only when a valid phase was chosen **and** the party is not aboard ship — checked independently of the UI, since a caller that supplies `hours` directly (the parity fixture) must get the same answer; (4) `commands.cpp`'s pre-existing (already-correct, unmodified) `phase<0`/ship/bounds gate and silent-success `moonstone_teleport` call now receive a real phase instead of an always-valid stale default. `dungeon_orchestration.cpp` needed **no** change: Vas Rel Por is peace-time-only (`time_bits` excludes both combat and dungeon), so `cast_spell()`'s own generic time-window gate already rejects it underground with "Not here!" before any Gate-specific code runs — proven as a control, not assumed. `check-gameplay.ts` (native's own copy of the parity fixture) was missing the ceremony cue in its `gateTravel` branch entirely — a real gap in the fixture, not the reference — and was corrected alongside the fix; its line-130 scenario (`hours` ∈ {-1,0,3,7,8} × six transport tiles) caught a native off-by-one (no upper bound on the valid phase range) that the new host tests had missed. `gameplay_parity` passes. |
| Y-34 | `quest_parity` crashes the native driver with `STATUS_ACCESS_VIOLATION` (exit `3221225477`) | **GREEN — RESOLVED (Batch 17). Host-harness undefined behaviour, not a production defect and not environment flakiness.** Carried as "baseline noise" from Batch 8 through Batch 16. The driver dies on input line 3452 (`{"op":"theft","here":0,"seed":20}`, `keys=2`) — one of exactly two of the 1152 `theft` cases in which Faulinei's rejection-sampling re-roll (TALK.OVL `0x11c7`, `re/notes/shadowlord-urbano-acta.md` §2.3) does not terminate against the port's deterministic `OriginalRng`. Both parity sides bound the *observation* with a 65536-draw watchdog; the native side escaped it with `setjmp`/`longjmp` written inline in `quest_driver.cpp`'s `main()`. On `x86_64-w64-mingw32` with `__SEH__`, `setjmp(b)` expands to `_setjmp(b, __builtin_frame_address(0))` because GCC 16 dropped `__builtin_sponentry`, and GCC's frame base is not the Win64 SEH establisher frame — measured at `-O2`: TargetFrame `0x5AD89FFD30` against establisher frame `0x5AD89F62A0`, off by `main`'s ~39 KB frame. `ntdll!RtlUnwindEx` therefore never matches, walks past `main` and off the top of the thread stack into `MEM_RESERVE` pages, and the resulting AV recurses inside exception dispatch until the process dies. At `-O0` the two values coincide exactly and the identical source survives, which is why it read as flaky for eight batches. **Adjudication: zero parity divergence was hiding behind it** — the full 5377-case comparison was run to completion on an `-O0` driver *before* any change and matched the TypeScript reference on every case. **Fix is harness-only**: the escape moved to a shared seam (`native/core/tests/quest_theft_watchdog.h`) and became a C++ exception, the same escape `check-quests.ts` uses. No production translation unit touched; firmware byte-identical. | New `batch17_theft_watchdog` (33 checks) pins the budget, the untouched-state contract, the gate, the linear cascade, the exact non-terminating set, and — group D — that the escape unwinds rather than jumps, so a return to `longjmp` turns red cleanly instead of crashing. Host suite is now **85/85, zero failures**. See §14 Batch 17. |

**Y-29 investigation detail — surveyed candidates for a device-reachable "clear active player" route:**

| Candidate | Hardware/input distinguishable? | Conflicts with existing action? | Preserves short=Cancel / long=Movement Mode? | Complexity |
|---|---|---|---|---|
| Shift+0 (shift + physical Mic key) | **No** — the Mic-key branch in `ui_input_adapter.cpp::translate()` matches on `column`/`row` alone and returns before `raw.modifiers` is ever inspected, for both the pressed and released edges | N/A — unreachable | N/A — unreachable | N/A |
| Alt+0 (alt + physical Mic key) | **No** — same reason: the position match short-circuits before the `raw.modifiers.alt` branch further down in the same function is ever reached | N/A — unreachable | N/A — unreachable | N/A |
| Symbol/"Fn" layer + physical Mic key (**preferred — see recommendation below**) | **Yes, with a small code change** — today the position check in `UiInputAdapter::translate()` fires unconditionally and never inspects `raw.modifiers.symbol`, so this chord is currently swallowed by the same short/long Cancel/Movement-Mode branch as a bare press; but `raw.modifiers.symbol` is already carried on every `RawInputEvent`, so the branch can be taught to check it first | **No** — a *plain* Mic/0 press (no Symbol) keeps exactly its current short=Cancel / long=Movement-Mode behavior unchanged; only the Symbol-held case would newly diverge, and that combination is not bound to anything today | **Yes for the unmodified key** — the short/long contract is untouched for a plain press; only the Symbol-held edge of the same physical key gains new behavior | **Low** — confined to `ui_input_adapter.cpp`'s Mic-key branch: when `raw.modifiers.symbol` is set, bypass the short/long Cancel/Movement-Mode special case entirely and instead emit `action.kind = UiActionKind::Character, action.character = '0'`, which flows through the *existing, already-correct* `case '0'` digit path in `ui_session.cpp::handle_exploration` verbatim (including the harpsichord-note-0 interception, exactly as a real `'0'` press elsewhere would) — and also restores the keyboard's normal Symbol-layer numeric-`0` behavior anywhere a literal `0` is genuinely needed, not just for Set Active Player |
| Alt + a different digit-producing key (e.g. the symbol-layer key that yields `'1'`, column 0 row 1) | **Yes** — that position is untouched by the Mic-key special case; `raw.modifiers.alt` and `raw.column`/`raw.row` are both already carried on every `RawInputEvent` | **No** — today `raw.modifiers.alt` combined with any key other than `m`/`d`/`s`/`l` falls through `UiInputAdapter::translate()`'s alt branch and returns `false` (dropped, no-op); this exact chord is currently dead input | **Yes** — entirely independent code path from the Mic-key branch | **Low**, but **not preferred** — it works, but it repurposes an unrelated key/modifier combination that has no relationship to the digit `0` at all, whereas Sym+Mic/0 is the position that was always meant to produce `'0'` in the first place |
| A dedicated system/context-menu command (e.g. exposed from the `(N)ew Order` party-selection screen) | Yes, trivially — any UI affordance can carry it | Would need new UI; `(N)` today only performs the reference's two-stage swap-order flow, which has no Set-Active-Player affordance in the reference either | Yes — unrelated code path | **Higher** — no reference precedent for a menu-driven active-player clear; inventing one risks diverging from reference behaviour, and it still requires a rendering/interaction affordance that does not exist today |

**Recommendation (NOT IMPLEMENTED YET — pending approval):** teach the Mic/0 branch in `UiInputAdapter::translate()` to check `raw.modifiers.symbol` before applying its short/long special case. A **plain** physical Mic/0 press keeps its current, unchanged contract: short press = Cancel, long press = Movement Mode toggle. A **Sym + physical Mic/0** press should instead bypass the short/long handling entirely, emit the literal character `'0'`, and flow through the existing normal character path — the same `case '0'` digit route every other digit already uses. This makes `SetActivePlayer(member=0)` / clear-active-player reachable on hardware, and also restores the keyboard's expected Symbol-layer numeric-`0` behavior anywhere else a literal `0` is genuinely needed. This supersedes the previously-recommended Alt+&lt;other-key&gt; workaround (still listed above for completeness): Sym+Mic/0 is the position the hardware's own symbol layer was already designed to produce `'0'` from, so it is the correct fix rather than a repurposed unrelated chord. As with the rest of Y-29, this is documented as a recommendation only — **not implemented in this pass** — pending explicit approval before implementation.

---

## 5. DEAD / UNREACHABLE PATHS

### `CommandKind` values with no player route

| Value | Verdict |
|---|---|
| `Unready` | **Dead but harmless** — `equip_item` toggles, so unequipping works. Intentionally left alone by Batch 3's R-06 fix. |
| `CombatEscape` | **Dead** — only `CombatEscapeQuick` is bound; confirm the slow-escape variant is intentionally unused |
| `CombatAttackCancel` | **Deliberately dead** — documented at `ui_session.cpp:486`; removing it consumed a turn, which is wrong for the handheld contract. Keep. |
| `CombatYield` | **Live** — R-06 correction pass: `UiSession::cancel_modal()` dispatches it to spend the combat action when a combat (R)eady picker closes, the counterpart of the reference's `playerReady()` |
| `ShopAction` | **Dead** — shops go through `UiIntentKind::Shop` → `execute_shop`. Candidate for deletion. |
| `EnterDungeon` | Reachable only from `commands.cpp:561` (world `Enter`) and `debug_map_picker.cpp:323`. Correct — not player-constructed. |
| `BeginConversation` | **STALE ENTRY — corrected in Batch 18. Live.** R-10 was resolved in Batch 4; the value has had producers ever since at `ui_session.cpp:1228` (`NpcInitiatesTalk` → `Command{BeginConversation, member=npc slot}`) and `alpha_runtime.cpp:1064`. The "blocked by R-10" reading survived four batches after its cause was fixed. |
| `BlackthornAction` | **STALE ENTRY — corrected in Batch 18. Live.** R-09 was resolved in Batch 4; produced at `ui_session.cpp:499/504/509/514` (Tribute, Arrest, Answer, Password) and handled at `commands.cpp:627`. |
| `UseMoonstone` | **Live** — R-08 resolved (Batch 3): moonstone rows 21-28 are in the Use picker whenever carried |
| `HarpsichordNote` | **Live** — R-20 resolved (Batch 3): digit keys at the harpsichord |
| `YellSails` | **Live** — R-19 resolved (Batch 3): `(Y)ell` aboard a frigate |
| `SetActivePlayer` | **Live, fully reachable on T-Deck hardware** — Y-20 resolved (Batch 3): literal digits `0`-`9`. Digits `1`-`9` were always physically reachable; literal `0` was blocked by the Mic/0 special handling — **Y-29 resolved (Batch 16)** via Symbol+Mic/0. |
| `AutoSleep` | Core-internal. Fine. |

### `UiMode` values

| Mode | Entry | Exit | Verdict |
|---|---|---|---|
| `ShrineSpecial` | 4 shrine/Blackthorn events | **GREEN — RESOLVED (Batch 1).** `ShrineSpecial` now captures `shrine_return_mode_` on entry. After a shrine modal resolves, `UiSession` settles back to that originating world mode only if no new shrine modal was synchronously re-armed. No arbitrary-key dismissal behavior was added. | Previously dead in practice — the mode was set, immediately shadowed by the modal it accompanied, and depended entirely on the R-01 clobber as its only escape. Fixed alongside R-01 per the audit's own instruction not to fix R-01 in isolation. |
| `Dialogue` | `DialogueOutputKind::Prompt` | `Ended` → `dialogue_return_mode_` | Live — R-01 + R-18 resolved (Batch 1) |
| `Shop` | Shop events | `Exited`/`Closed` → `shop_return_mode_` | Live — R-01 + R-18 resolved (Batch 1) |
| all others | — | — | Reachable |

### `GameEventKind` values with no consumer

`Moved`, `MapChanged`, `PartyChanged` — harmless (the device sets `dirty_` unconditionally and recomposes from authoritative state each frame).
`Sfx` — Y-03 (still unconsumed; the narrative scenes' cues are logged semantically, not synthesized).
`PoisonTick`, `CellExplosion`, `CellProjectile`, `Refuge`, `TrollSneak` — **all now consumed (Batch 7B)**; Y-04 GREEN. See §14 Batch 7B.
`NpcInitiatesTalk`, `NpcInitiatesShop` — R-10. `Quake`, `Zodiac` (R-13) and `MapReveal` (R-12) now have consumers -- see §14 Batch 7.

### `DungeonAction` values

~~`TurnAround` and `Drink` have no producer in `ui_session.cpp`, `alpha_runtime.cpp` or `dungeon_orchestration.cpp`.~~ **STALE ENTRY — corrected in Batch 18. Both live since Batch 9B.** `TurnAround` is produced at `ui_session.cpp:866` (Confirm in the dungeon) and `:911` (the `.` key); `Drink` at `:479` (the `Will you drink?` answer) and `:906` (the `d` key, which answers `"No fountain here."` off a fountain). `MagicUp`/`MagicDown`/`Tick` are correctly core-internal (produced by the Cast handler).

**Re-verified live at Batch 18 and still accurate:** `ShopAction` (handled at `commands.cpp:629`, no producer — shops route through `UiIntentKind::Shop`), `CommandKind::CombatEscape` (no reference anywhere but the enum; only `CombatEscapeQuick` is bound, at `ui_session.cpp:941`), `Unready` (handled at `commands.cpp:1114`, no producer — `equip_item` toggles). All three are deliberate and harmless; see the preservation ledger D-18.

### Renderer functions

`render_active_view()` (`native_renderer.cpp:299`) has no caller in `alpha_runtime.cpp` — superseded by `render_snapshot()`. Dead code; ~45 lines.

### Display-name tables

**Stale as of Batch 3 — corrected.** `usable_names` (`native/core/src/display_names.cpp`) is now indexed by the real canonical (U)se item id and has only 17 `nullptr` entries, all deliberate: indices 0–15 (the scroll/potion id ranges, named instead through `scroll_display_name()`/`potion_display_name()`) and index 35 / 0x23 (Pocket Watch — no `GameState` field backs possession, so the row is intentionally kept unresolvable rather than rendered ungated; see R-08). Every other id in 16–37 (the full R-08 owned-item table) now has a real name. The `UNRESOLVED_NAME` logging path in `open_selection` exists to catch a regression of this, not a currently-live gap. See R-07/R-08.

---

## 6. DEVICE-INTEGRATION RISKS (T-Deck specific)

| Risk | Detail |
|---|---|
| **The device glue is the untested half** | 1375 lines of `alpha_runtime.cpp` + 1082 of `tdeck_board.cpp` + 569 of `tdeck_input.cpp` ≈ 3000 lines with no host test. Every SEVERITY-1 finding lives here or in the presentation/asset layer. |
| **Mode ownership is split four ways** | `UiSession::consume` sets base mode from events; `AlphaRuntime::command()` forces Combat/Dungeon; `synchronize_after_debug` forced Combat/Dungeon/Exploration (root cause of R-01/R-18, **resolved in Batch 1** via the shared `ui_mode_policy.h` seam); `finish_combat_if_needed()` also rebinds base mode on the combat-teardown path (identified during Batch 1, left unchanged — redundant-but-harmless after the ownership fix). |
| **Viewport blit clips 9 px top and bottom** | `tdeck_board.cpp:628` deliberately reserves those rows for the sky and wind bars. Correct for the world view. The dungeon3d view repurposes the same two strips for its own level/facing bands (R-05, intentional, Batch 9) rather than dropping them. **RESOLVED for the gem view (Y-14, Batch 10):** `show_alpha`'s new `full_square_viewport` parameter (set from `gem_view_active_`) skips the strips entirely and blits the full 176 rows instead of overdrawing them across the gem square. The zodiac view is unchanged — still clipped, out of this batch's scope (R-13 already GREEN). |
| **Dirty-region cache keyed only on `viewport_crc32`** | Sound, but it means any renderer that produces a constant image (e.g. an all-black dungeon frame) will suppress its own redraw. Worth a `force` on presentation-source changes — `dungeon_presentation_pending_` already does this for dungeon entry. |
| **Double `render()` per loop iteration** | `main.cpp` calls `runtime.render(board)` twice per pass (once for `input_dirty`, once unconditionally). Harmless today because `dirty_` gates it, but it doubles the animation-tick path cost. |
| **PSRAM budget** | A* 323 KB + transcript 15 KB + viewport 62 KB + creation canvas 97 KB + tile cache + 8 dungeon arenas. The DNG/ITEMS budget check is **done** (Batch 9): 217,792 B per DNG variant as RGB565 (653,376 B for all three) + 34,680 B for ITEMS.16; ≈163 KB for all three at 4 bpp. Only one variant is ever resident, so ≈213 KB + 34 KB. Affordable as a cache; a streaming reader is not forced. |
| **24 KiB main stack, dedicated input task, keyboard recovery** | All preserved and working. Do not disturb. |
| **SD DMA headroom guard** | `AlphaSaveService::reserve_dma_headroom` + `SdHeadroomGuard` is a good pattern; save/load is the riskiest device operation and it is well defended. |
| **Asset/firmware identity gate** | `packs_match` blocks startup on CRC/SHA mismatch. It did its job in Batch 9C: adding the dungeon art changed the pack identity (1,843,457 → 2,039,545 B, CRC `0x2b1449f4` → `0x2065ad91`), so **a pre-9C card is correctly refused** and firmware and card must be updated together. The format itself was not bumped — a named-entry TOC needs no version change for new entries — and the gate was neither weakened nor bypassed. |

---

## 7. TEST-GAP REPORT

### Currently failing — **NOTHING. (Batch 18, re-measured from a clean build directory.)**

> **Read this before the history below.** Every "currently failing" statement in the rest of this section is **archaeology**, kept for its reasoning and superseded by this box. As of Batch 18 the authoritative host suite is **86 total, 86 pass, 0 fail, 0 skipped**, measured twice: **85/85/0** from a clean `build-batch18` on the unmodified Batch 17 tree (the pre-edit baseline), and **86/86/0** after Batch 18's own fix and its new `batch18_well_ceremony` target.
>
> | Measure | Batch 18 baseline | Batch 18 final |
> |---|---|---|
> | registered tests | 85 | 86 |
> | pass / fail / skipped | 85 / 0 / 0 | 86 / 0 / 0 |
> | `gameplay_parity` | PASS, 5,058 sequences | PASS, 5,058 sequences |
> | `quest_parity` | PASS, 5,377 cases, `nonterminatingObservations: 2` | PASS, same |
> | project compiler warnings | 0 | 0 |
>
> The one warning the clean baseline build emits is **not** a project warning: it is GCC 16's `-Wstringop-overflow` firing inside `<bits/stl_uninitialized.h>` on a `std::vector::insert` inlined from `tests/command_parity_test.cpp:240` — test-harness code, a known false-positive shape for that diagnostic, and not present in any translation unit that ships.
>
> **The "compare against the known baseline failures" convention is retired and must not come back.** It was retired in two halves — `gameplay_parity` in Batch 13 (R-21) and `quest_parity` in Batch 17 (Y-34) — and Batch 18 is the first batch to open *and* close on a fully green suite. Any failure a future batch sees is real and is caused by the change under test.

#### Historical record

Post-Batch-1 host suite was: **58 total, 57 pass, 1 fail** (`gameplay_parity`, mismatch 59, R-03).

**Post-Batch-2 host suite is: 58 total, 57 pass, 1 fail.** Mismatch 59 is fixed. `gameplay_parity` still fails, but now at a *different* location:

| Test | Status |
|---|---|
| `gameplay_parity` | **PASSING** since Batch 13 (R-21 resolved — the fabricated (U)se-consumable echo; see §3 R-21 and §14 Batch 13). Historical entry follows. — was: **FAILING** — mismatch 2034, **R-21** (newly exposed, pre-existing, unrelated to Batch 2 — see §3 R-21). Mismatch 59 (R-03) is **fixed** and no longer the failure. Only failure; all other 57 tests, including `ui_mode_regression` (28/28 GREEN) and the Batch 2 regressions (`presentation_regression`, `combat_loot_open_regression`, `combat_escape_regression`, `direct_troll_handoff_regression`), pass. |

**Post-Batch-4 addendum (superseded by the harness-correction pass below):** the authoritative suite grew from 61 to **63** registered tests (`batch4_group_a`, `batch4_group_b`). Initial result: 63 total, 60 pass, 3 fail — `gameplay_parity` mismatch 2034 (R-21, unchanged) plus one internal defect each in `batch4_group_a` (its A8 case, 4 checks) and `batch4_group_b` (one check in B5), both test-authoring defects, not production regressions (see R-09 above for the original analysis).

**Post-Batch-4 harness-correction pass:** A8 and B5 were independently re-derived from the production semantics and test code (not assumed correct from the prior report), confirmed to be the test-authoring defects described above, and corrected — not weakened, not deleted — to test the real invariants: A8 now drives a real trigger + real modal answer for each of the four Blackthorn/guard families before asserting `Pass` is unblocked (Blackthorn's case adds a minimal content-free `ShrineServices` fixture so its real state machine can conclude on host); B5 now respects the `AlphaRuntime`-vs-host-layer boundary — it no longer routes through `UiSession::consume()` (which B1–B4 already prove end-to-end) and instead closes the identity-lifetime loop by dispatching the re-resolved `(slot, location)` identity as a real `BeginConversation` and confirming it opens the correct session. No production code changed in this pass. Result: **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). No Batch 4 test failures remain.

**Post-Batch-9 host suite is: 71 total, 69 pass, 2 fail.** The suite grew by one registered test, `dungeon_view_regression`. Both failures are the known unrelated pair — `gameplay_parity` (R-21, mismatch **2034**, unchanged index and content) and `quest_parity` (the GCC/w64devkit `STATUS_ACCESS_VIOLATION` environment crash, exit 3221225477). The pre-edit baseline captured before any Batch 9 change was **70 total, 68 pass, 2 fail**, the same two.

`ui_mode_regression` is new, narrow device-glue coverage added in Batch 1 — it host-tests mode arbitration and `UiSession` mode ownership via the `ui_mode_policy.h` seam. It does not make `AlphaRuntime` as a whole host-tested; see Y-05 below.

### False-confidence tests

| Test | Why it over-reports |
|---|---|
| `ui_session_tests` | Uses a `Spy` dispatcher that records `UiIntent`s and never executes them. Proves `UiSession` routing; proves **nothing** about `AlphaRuntime::dispatch`, `modal()`, `open_selection` or `synchronize_after_debug` — i.e. exactly where R-01, R-06, R-07, R-08, R-09, R-11 live. **Partly mitigated (Batch 3):** the Use-picker row-selection logic was extracted into the ESP-free `openu5::usable_item_picker_rows()` seam that `AlphaRuntime::open_selection()` and `batch3_group_b_tests` now both call, so that slice of `open_selection` is host-testable. `batch3_group_a_tests` drives the real `UiSession::handle_input()` -> `dispatch()` path for the sails/digit/harpsichord routes; `batch3_group_c_tests` drives the real `execute_command()` path for Ready in all three contexts. **Further mitigated (Batch 5):** the R-11 slice of `cast_selected_spell` — which prompt a (C)ast owes the player in a given context — was extracted the same way into `openu5::cast_target_prompt()`, and `batch5_tests` drives that seam plus the real `UiSession` TargetSelection routing and the real `world_magic()` consumption ordering. |
| `device_smoke_tests` (37 scenarios) | Cases 3,4,5,9,10,12,13,15,18,19,20,21,24,26,27,28,29 are **data-presence assertions** ("is the table non-empty", "is the name non-null"). Cases 2,6,7,8,11,17,22,25 are `UiSession`-with-spy probes. Case 16 composes a snapshot but asserts only a hash. **No scenario validates a rendered frame, a mode round-trip, or a full input→state→presentation chain.** |
| `presentation_regression` *(historical — gap closed in Batch 2)* | At the original audit baseline, asserted snapshot composition but never checked that the chosen tile was the *right* tile for the object kind, which is why R-02 and R-04 survived. Batch 2 added R03-OUTDOOR, R04-OVERLAY and R04-LIFO cases that close this gap; the test now asserts both the correct tile and the correct layering. |
| `combat_loot_open_regression` *(historical — gap closed in Batch 2)* | At the original audit baseline, covered Open→pile→Get authoritative mutation but did **not** assert that no world object is created on exit — R-01/R-03's blind spot. Batch 2 added the R03-DIRECT case that closes this gap. |
| `debug_developer_test`, `debug_map_picker_test` | Strong (they assert no turn/RNG consumption), but they exercise the debug path, which `synchronize_after_debug` *serves correctly*. They therefore cannot expose R-01. |
| `frontend_test` | State machine only; no view/art assertions. |
| *(dungeon view)* *(historical — gap closed in Batch 9)* | Until Batch 9 there was **no host coverage of dungeon presentation at all**: `render_dungeon_view()` lived in `native_renderer.cpp`, which pulls in `esp_err.h` and `asset_pack.h` and cannot be host-built, and every dungeon test asserted core *state* only. That is why eight presentation defects survived every prior batch. Closed by extracting the decisions into `openu5::plan_dungeon_view()` and covering them with `dungeon_view_regression`. |

### Genuinely strong tests — preserve

`combat_parity`, `advanced_combat_parity`, `command_parity`, `item_parity`, `magic_parity`, `turn_parity`, `travel_parity`, `shop_parity`, `dialogue_parity`, `quest_parity`, `dungeon_parity`, `dungeon_flow_parity`, `dungeon_view_regression`, `movement_flow_parity`, `transport_flow_parity`, `world_flow_parity`, the `typescript_*_drift` fixture-freshness suite, and `input_regression`. These are the reason the core is trustworthy.

---

## 8. SAVE / LOAD COVERAGE REPORT

| Domain | Serialized | Restored | UI rebinds | Status |
|---|---|---|---|---|
| Party (6 chars × 18 fields) | ✓ | ✓ | ✓ | **G** |
| Position / map / floor | ✓ | ✓ | ✓ | **G** |
| Time, turns, moon phases | ✓ | ✓ | ✓ | **G** |
| Gold/food/keys/gems/torches/skull keys/carpets | ✓ | ✓ | ✓ | **G** |
| Equipment / potions / scrolls / reagents / spells | ✓ | ✓ | ✓ | **G** |
| Transport (mode, tile, hull, skiffs, sail dir, HMS Cape) | ✓ | ✓ | ✓ | **G** |
| Quest flags, shrine bitmaps, doom bits, shadowlords, shards | ✓ | ✓ | ✓ | **G** |
| NPC dead/met bitmaps + NPC walk state | ✓ | ✓ | ✓ (`enter_npc_map` then overlay) | **G** |
| Special items (crown/sceptre/amulet/badge/box/spyglass/sextant/grapple) | ✓ | ✓ | picker (grapple deliberately excluded — Klimb-only) | **G** storage / **G** use (R-07/R-08 resolved, Batch 3) |
| Door timer, town/outdoor turn phases, awaiting-exit | ✓ (`gameplay_save`) | ✓ | ✓ | **G** |
| Terrain overrides (`mapOverrides`, `openDoors`) | ✓ | ✓ | ✓ | **G** |
| Overworld enemies | ✓ | ✓ | ✓ | **G** |
| **World objects (`objects_`)** | ✓ | ✓ | cleared-then-restored, no leak | R-14 resolved (Batch 6) |
| **Dungeon session** | ✓ | restored if active | full session (pos/cells/revealed/wanderer) resumes | R-15 resolved (Batch 6) |
| Combat session | ✗ | cleared | ✓ | **N** (matches reference) |
| Dialogue / shop / shrine / Blackthorn sessions | ✗ | cleared | ✓ | **N** (session-only, correct) |
| Settings (brightness, movement mode, trackball, UI size) | ✓ (separate `settings.json`) | ✓ | ✓ | **G** |
| **Persistent dungeon field spells** (In Flam/Nox/Zu/Sanct Grav) | ✓ | ✓ | n/a (re-derived from cells) | **G** — reconciled in Batch 18 |
| **Hidden / search objects once found** | ✓ | ✓ | ✓ | **G** — reconciled in Batch 18 |
| Reagent-patch "found on day N" latch | ✓ (`save_core.cpp:191` `reagentPatchFoundDay`) | ✓ (`:381`) | n/a | **G** — reconciled in Batch 18 |
| Buried-moonstone state | ✓ (`persistence.cpp:308`) | ✓ (`:419`) | n/a | **G** — reconciled in Batch 18 |

**Batch 18 persistence reconciliation.** The four rows above were not in this table and were audited directly rather than assumed:

* **Dungeon field spells have no storage of their own and need none.** `dungeon_orchestration.cpp:176` writes the field straight into `d.cells[front]` (`cell = (cell&8) | 130/129/128/131` for Zu/Nox/Flam/Sanct). `DungeonState::cells[512]` is serialized in full by `save::capture_dungeon()` (`gameplay_save.cpp:90`) and validated element-by-element on restore (`restore_dungeon()` rejects any array that is not exactly 512 finite bytes in 0–255). A cast field therefore survives save/load for free, and so does every authored field — they are the same bytes. Device row H-115 checks it on glass.
* **Found search objects** become ordinary `QuestObject`s via `quest_search.cpp`'s `place()` lambda and ride `objects_`, which R-14 made persistent in Batch 6.
* Both remaining rows are plain `GameState`/`TurnState` fields with named serializers, cited above.

**No persistence axis was found lacking a production-path test.** `persistence_parity` drives the real `persistence_driver`; `gameplay_integration_test.cpp:87` round-trips `capture_dungeon` against a live `DungeonState`. **No new save format was introduced, and none is needed.**

**Transaction integrity: G.** Two-slot generation commit with per-file CRC, temp-file validation, atomic rename, `select_generation` recovery, and DMA headroom guarding. This is the best-engineered part of the device layer.

**Can save/load strand the player in an invalid mode?** Only via R-15 (dungeon), and even then `synchronize_after_debug` re-derives a valid mode on the next input. No known hard strand.

---

## 9. INPUT / COMMAND CONTEXT MATRIX

| Input | World | Combat | Dungeon | Selection modal | Target modal | Text entry | Numeric | Yes/No | Shop | Dialogue | Debug |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Trackball / arrow | Move | CombatMove | Fwd/Back/Turn | cursor ±1 | reticle / resolve | — | — | — | list cursor | — | cursor |
| WASD (Movement Mode **on**) | Move | CombatMove | Fwd/Back/Turn | cursor ±1 | reticle | **literal** | **literal** | **literal** | list cursor | **literal** | cursor |
| WASD (Movement Mode **off**) | `a`Attack `s`Search `w`/`d` "What?" | `a`Attack `s`Search | `a`Attack `s`Search | — | — | literal | digits only | `y`/`n` | shop keys | literal | — |
| Enter / Confirm | Pass | CombatPass | Pass | select | fire/cast/confirm | accept | accept+clamp | **yes** | select/confirm | — | enter/apply |
| Mic short (Cancel) | — | **CombatEscapeQuick** | — | cancel modal | cancel (Klimb→KlimbCancel, Cast→cancel_target, Attack→silent) | cancel/clear | cancel | yes-if-`cancel_means_no` | hierarchical back | EndConversation | back one level |
| Backspace | Back | Back | Back | cancel | cancel | **delete char** | **delete char** | — | back | — | back |
| Mic long (1.1 s) | toggle Movement Mode | toggle | toggle | toggle | toggle | toggle | toggle | toggle | toggle | toggle | toggle |
| `Alt+M` | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu | System Menu |
| `Alt+D` / `Alt+S` / `Alt+L` | debug / save / load | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Shift+up / Shift+down | transcript page | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Letter keys | 23 verbs | 8 verbs | 11 verbs | — | — | text | digits only | `y`/`n` | shop verbs + `a`–`z` row pick | text | — |

**Context-leak findings**
- `accepts_direction_input()` correctly includes `DebugMenu` and all selection modes, so Movement Mode navigates them — good.
- `Alt+M`/`Alt+D`/`Alt+S`/`Alt+L` are handled **before** gem-view close and before gameplay routing. Deliberate: `gem_view_active_ && shortcut == None` means a shortcut does not dismiss the gem view. Correct.
- Combat queues player input during enemy AI turns (`combat_input_queue_`, 8 deep) rather than dropping it. Good.
- **`is_movement_context` excludes `Shop`** even though `accepts_direction_input()` returns true for list phases — passed in as the `accepts_direction` argument, so it works. No leak.
- The `handle_combat` `Cancel/Back` → `CombatEscapeQuick` mapping is the canonical victory exit. Preserved.

---

## 10. RENDERER / MODE MATRIX

| UI mode | Authoritative context required | Renderer | Input target | Back returns to | Verdict |
|---|---|---|---|---|---|
| (frontend active) | `frontend_.active()` | `render_frontend` / `render_intro_view` / creation canvas | `FrontendSession` | menu | **G** — asserted exclusive |
| (system menu active) | `system_menu_.active()` | system-menu view | `SystemMenuSession` | gameplay mode | **G** — asserted exclusive |
| `Exploration` | none | `render_snapshot(compose_world_presentation)` | `dispatch_world_command` | — | **G** |
| `Dungeon` | `dungeon_.active && context_.dungeon` | `render_dungeon_view(plan_dungeon_view(...))` | `execute_dungeon_command` | `handle_dungeon()` → full reference dispatcher (Batch 9B) | **G** logic (Batch 9, `dungeon_view_regression`) / **G** input (Batch 9B, `dungeon_input_regression`) / **G** art (Batch 9C, `dungeon_art_regression` + `typescript_dungeon_art_identity`) |
| `Combat` | `context_.combat && combat_.initialized` | `render_snapshot(compose_combat_presentation)` | combat commands | quick escape | **G** logic / icons — R-04 resolved (Batch 2, `compose_world_presentation` two-layer fix) |
| `Shop` | `shop_.phase != Closed` | world/combat viewport + `DeviceShopView` overlay | `execute_shop` | hierarchical | **G** — R-01 resolved (Batch 1) |
| `Dialogue` | `dialogue_` session | viewport + transcript | dialogue commands | `EndConversation` | **G** — R-01 resolved (Batch 1) |
| `ShrineSpecial` | shrine/Blackthorn session | viewport only | modal resolution → `shrine_return_mode_` | originating world mode | **G** — resolved (Batch 1) (§5) |
| `TextEntry` / `NumericEntry` / `YesNo` | request-specific | viewport + prompt | `finish_modal` | `return_mode_` | **G** |
| `PartySelection` / `InventorySelection` / `EquipmentSelection` / `SpellSelection` | none | viewport + `DeviceSelectionView` | `modal()` | `return_mode_` | **G** routing / **G** content (R-07/R-08 resolved, Batch 3) |
| `TargetSelection` | combat for aim; world for Fire | viewport + reticle (`snapshot.target_*`) | direct command | `return_mode_` | **G** |
| `DebugMenu` | developer build | `DeviceDebugScreen` (viewport suppressed) | `UiDebugMenu` | `debug_return_mode_` | **G** |
| *gem view overlay* | `gem_view_active_` | `render_world_gem_view` / `render_dungeon_gem_view` | any key closes | prior mode + `AfterGemView` | **G** — R-17/Y-14 RESOLVED (Batch 10), hardware check pending |

**Presentation source selection** (`alpha_runtime.cpp` `render()`) is a clean single decision — `combat ? combat : dungeon ? dungeon3d : world`, overridden by the gem view. It logs `PRESENTATION_DISPATCH` every frame. This part is well built; the problems are the *contents* of two of the branches and the *mode* that selects them.

**Impossible combinations found:** none in the renderer. The previously identified Shop/base-mode and ShrineSpecial lifecycle contradictions were resolved in Batch 1.

---

## 11. RESOURCE / ITEM MATRIX

**Note on the `View` column (post-Batch-14):** "Z-stats (R-22)" below means the reference exposes this resource on a `(Z)` status page. **R-22 is RESOLVED (Batch 14)**: the page family exists and every row below is reachable on device, so those `View` entries now describe a route that works rather than a missing one. The `Status` column still separates the resource's own storage/use functionality from that presentation route. The one row that remains view-less is the Pocket Watch (extended id `0x23`), which no `GameState` field backs — see R-08 and the Batch 14 residuals.

| Resource | Get | Use | Ready | View | Save | Status |
|---|---|---|---|---|---|---|
| Gold | ✓ | n/a | n/a | Z-stats Provisions page | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Food | ✓ | n/a (auto-consumed) | n/a | Z-stats Provisions page | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Gems | ✓ | n/a | n/a | **`V`** | ✓ | **G** — R-17/Y-14 RESOLVED (Batch 10), hardware check pending |
| Keys | ✓ | via Jimmy/Open | n/a | Z-stats Provisions page | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Torches | ✓ | `I`gnite | n/a | Z-stats Provisions page | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Reagents ×8 | shop | `M`ix | n/a | Z-stats Reagents page | ✓ | **Y-19** storage/use / **G** view (R-22 resolved, Batch 14) |
| Equipment ×48 | ✓ / shop | n/a | **`R`** | Z-stats Armaments page | ✓ | **G** world, dungeon and combat (R-06 resolved, Batch 3) / **G** view (R-22 resolved, Batch 14 — which also corrected the off-by-one in `equipment_display_name`) |
| Armour / helmets / shields | ✓ | n/a | ✓ | Z-stats Arms + Armaments pages | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Weapons / ammo | ✓ | n/a | ✓ (ammo checked) | Z-stats Arms + Armaments pages | ✓ | **G** storage/use / **G** view (R-22 resolved, Batch 14) |
| Potions ×8 (ids 8–15) | ✓ | ✓ + party target | n/a | picker | ✓ | **G** |
| Scrolls ×8 (ids 0–7) | ✓ | ✓ | n/a | picker | ✓ | Y-21 **discharged (Batch 5)** — the Rel Hur getdir exists and works; cancel-path refund **resolved (Batch 16, Y-31)** |
| Spells ×48 | `M`ix | `C`ast | n/a | picker + summary; Z-stats Spells page | ✓ | R-11 **RESOLVED (Batch 5)**, **R-16 RESOLVED (Batch 8)** / **G** view (R-22 resolved, Batch 14) |
| Magic Carpet (16) | quest | ✓ | n/a | picker | ✓ | **Y** |
| Skull Key (17) | quest | ✓ | n/a | picker | ✓ | **Y** |
| Amulet (18) | quest | ✓ | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker — R-07 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Crown (19) | quest | ✓ | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Sceptre (20) | quest | ✓ | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Moonstones (21–28) | quest | ✓ | n/a | picker (gated on `!buried`); Z-stats Items page | ✓ | **G** picker — R-08 resolved (Batch 3); ground-marker persistence R-14 resolved (Batch 6); **G** Z-stats Items-page view (R-22 resolved, Batch 14 — the page reads the same `!buried` gate as the picker) |
| Shards (29–31) | quest | ✓ | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Spyglass (32) | quest | ✓ route | n/a | picker; Z-stats Items page | ✓ | **R-13** (no zodiac view) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Plans (33) | quest | ✓ | n/a | picker (gated on `hms_cape`); Z-stats Items page | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Sextant (34) | quest | ✓ | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| **Watch (35)** | **no owner anywhere** | **absent** | n/a | **absent** | ✓ | **OPEN** — deliberately excluded by Batch 3: no `GameState`/`QuestState`/`QuestWorldServices` field backs id 35, so no possession gate exists. Inventing one was out of scope. |
| Badge (36) | quest | ✓ | n/a | picker (gated on `black_badge`); Z-stats Items page | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Wooden Box (37) | quest | ✓ ("How?") | n/a | picker; Z-stats Items page | ✓ | **G** Use-picker / **G** Z-stats Items-page view (R-22 resolved, Batch 14) |
| Grapple | quest | **Klimb-only, never a Use item** | n/a | — | ✓ | **G** — R-07 resolved (Batch 3): removed from the Use picker entirely |
| Loose loot piles | ✓ LIFO | n/a | n/a | rendered `0x100+id` | **✓** | R-14 resolved (Batch 6) |
| World chests | Open→piles | n/a | n/a | correct sprite (was **tile 1 = blue**; R-02 resolved Batch 2) | **✓** | R-02 **G** / R-14 resolved (Batch 6) |

---

## 12. DUNGEON INTEGRATION REPORT

| Layer | Status | Detail |
|---|---|---|
| 1. Selection / load | **G** | `dungeon_load` via `EnterDungeon`; 8 dungeons validated by smoke 18 |
| 2. Dungeon id | **G** | `DungeonData::location` 33–40 |
| 3. Depth (0–7) | **G** | Bounds-checked in `run_dungeon_command` |
| 4. x/y (0–7) | **G** | Bounds-checked, `&7` wrap |
| 5. Facing | **G** | `DungeonFacing` N/E/S/W, validated `<4` |
| 6. Map data | **G** | 512 cells/dungeon from `dungeons.json`, packed and validated |
| 7. Cell decoding (wall/door/stair/pit/field) | **G** | High nibble = type, per DNGLOOK dict; `dungeon_cell`/`dungeon_wall` agree with the movement blocker |
| 8. Visibility | **G** — Batch 9 | Concern **withdrawn**: the torus wrap is what the original does on purpose (`coreview.ts dungeonView().add()` marches the ray unwrapped and reads the tile `&7` so the corridor continues across the border). Batch 9 added the missing piece instead — the **light gate**: with neither torch nor light spell the viewport is now black, as DUNGEON:0x1AD6 requires |
| 9. Geometry | **G** — Batch 9 | `plan_dungeon_view()` emits all four side families (wall 0–3, door 4–7, open passage 16–19, alcove 20–23) and all three front families (dead end 8–11, door front 12–15, special 25–27), classified by the neighbour/blocking cell, plus the standing-on-a-door ring-0 suppression. **Sight is now its own rule**, not the movement blocker: a door, a room and a revealed secret door all stop the view |
| 10. **Authored textures** | **G** — Batch 9C | **Packed and painted.** Five `dungeon-*.art` entries carry all 146 authored images at their native 4bpp+mask (193,864 B); `render_dungeon_view()` blits them through `dungeon_art_blits()`. The wireframe stand-in and its `ringBox()` geometry are **gone** |
| 11. Wall/floor/ceiling | **G** — Batch 9 + 9C | The variant reaches the view (`plan_dungeon_view()` carries `dungeon_wall_variant()`), and Batch 9C turned the **tint** into **atlas selection**: V1/V2/V3 select the DNG1 olive / DNG2 red / DNG3 grey banks. Floor speckle and ceiling are no longer flat fills — they are baked into each authored slice, as in the original |
| 12. Viewport composition | **G** — Batch 9 | 176×176 correct. The two 9 px strips still sit over the viewport (the T-Deck frame is 2 px, so unlike the original there is nowhere outside to put them — documented divergence), but they now carry the dungeon's **own** bands, `L1`..`L8` and `Dir:` + facing, instead of a blank sky and `Wind: --` |
| 13. HUD integration | **G** — Batch 9 | `hud_dungeon_bands()` (core, host-tested D12) supplies the level and facing captions; `tdeck_board.cpp` paints them in place of the sky/wind strips and folds them into the same cache signatures, so a Klimb or a turn repaints its strip on the frame that produced it |
| 14. Movement (fwd/back) | **G** | `DungeonAction::Forward`/`Back`; `dungeon_flow_parity` green |
| 15. Turning | **G** | `Left`/`Right` |
| 16. Backward movement | **G** | Distinct action, does not turn |
| 17. Blocked behaviour | **G** | Same `dungeon_wall` rule as the renderer — consistent |
| 18. Stairs / ladders | **G** — Batch 9 logic+placement / Batch 9C art | `dungeon.cpp` up/down with floor bounds → `ExitSurface`/`ExitUnderworld`. Now **drawn at every depth 0–3**, not only underfoot: a ladder two cells ahead used to be invisible |
| 19. Pits | **G** — Batch 9 logic+placement / Batch 9C art | Damage script path present; drawn at every depth, and the trap `(sub & 7) == 0` gate (fn_1952 @0x197b) is now honoured |
| 20. Fields / traps | **G** — Batch 9 logic+placement / Batch 9C art | Cast-created fields at `dungeon_orchestration.cpp:165`; a magic field is drawn by its own primitive coloured by `sub & 3` (it has no ITEMS.16 art in the original either) |
| 21. Objects | **G** — Batch 9 logic / Batch 9C art | Was: only the `here` cell, as one 12×12 green/red blob. Now every cone cell of kind 1–8 at depths 0–3, painted far→near after the corridor, each now as its authored ITEMS.16 half-image pair (ladder / chest / open chest / fountain / trap; a magic field has no ITEMS art in the original either and keeps its own primitive). The wanderer is drawn at depths 1–3 from its MON0-7 bank and a `hidden` one is drawn on the **ceiling row** instead of being suppressed |
| 22. Encounters | **G** | `DungeonEncounters` + `dungeon_arenas_` bound to all combat maps |
| 23. Dungeon combat | **G** | `start_room` / `start_corridor` with `corridor_cause` |
| 24. Combat return | **Y-22** | `dungeon_combat_return` handles floor delta, escape border and facing; core-tested only |
| 25. Dungeon exit | **G** | Walking out at the level-1 entry cell → `exit_dungeon(false)` → `DungeonExited` → `set_base_mode(Exploration)` |
| 26. Underworld transition | **G** | `exit_dungeon(true)` from floor ≥ 8 |
| 27. System menu round-trip | **Y-27** | No device proof |
| 28. Developer menu round-trip | **G** | `debug_return_mode_` restores Dungeon |
| 29. **Save/load in dungeon** | **G** | R-15 resolved (Batch 6): session (position/cells/revealed/wanderer) round-trips through the sidecar |
| 30. Movement Mode controls | **G** | W/S/A/D = forward/back/turn-left/turn-right — **already matches the requested target** |
| 31. Trackball | **G** | up/down/left/right = forward/back/turn-left/turn-right |
| 32. Keyboard (Movement Mode off) | **Y-23** | No movement keys; verbs only |
| — | R-06 **G** | `Ready` offered and applied (RESOLVED, Batch 3) |
| — | R-17 **G** | Dungeon gem view flood-fill moved to host-tested `build_dungeon_gem_view()` (RESOLVED, Batch 10); hardware check pending |

**Bottom line (revised again after Batch 9B):** the dungeon was **three** gaps, not one or two. The **presentation logic** was genuinely defective in eight provable ways (§3 R-05 part 1) and is now fixed and host-tested behind `plan_dungeon_view()`. The **controls** were **not** already correct — that earlier reading was a false negative, taken from `dungeon_action()`'s completeness rather than from the UI dispatcher that a physical key actually reaches; (I)gnite, (D)rink, (H)ole up, Turn Around, the Klimb U/D choice, the Search Dir- choice and the digit keys were all unreachable, and three core APIs had zero callers. Batch 9B fixed the routing (§3 R-05 part 2) and added `dungeon_input_regression`, which drives the real input path. Batch 9C closed the **asset-pipeline gap** (part 3): the authored art is packed with per-image CRC identity, resident in PSRAM, and painted through a second portable seam — leaving only the final physical dungeon session to confirm on glass.

---

## 13. QUEST / CONTENT COVERAGE REPORT

| Quest system | Core | Player route | Status |
|---|---|---|---|
| Word-of-power Yell at dungeon entrances | ✓ `yell_word_of_power` | ✓ `Y` (non-frigate branch) | **G** — Y-24 discharged (Batch 3) |
| Shadowlord summoning in Flame rooms | ✓ `summon_shadowlord` | ✓ `Y` | **Y** |
| Shard ritual (Falsehood/Hatred/Cowardice) | ✓ `cast_shard_into_flame` + `apply_shard_destruction` | ✓ Use picker ids 29–31 | **G** — R-08 resolved (Batch 3) |
| Crown / Sceptre / Amulet | ✓ `use_quest_item` | ✓ Use picker ids 18–20, correctly named | **G** — R-07/R-08 resolved (Batch 3) |
| Sceptre force-field dissolution (world + dungeon) | ✓ both branches | ✓ Use picker id 20 | **G** — R-08 resolved (Batch 3) |
| **Blackthorn interrogation** | ✓ `blackthorn.cpp`, `BlackthornAction` | **✗ modal discarded** | **R-09** |
| **Guard password / tribute / arrest** | ✓ `talk_guard` + 3 prompts | **✗ modals discarded** | **R-09** |
| Shrines (visit / restore / donate / quest bits) | ✓ `shrine.cpp` | ✓ all 3 modals wired | **Y-25** |
| Search-revealed quest objects | ✓ `quest_search.cpp` + fixtures | ✓ `S` | R-14 resolved (Batch 6) — survives reload |
| Lord British progression / karma | ✓ `dialogue_effects` | ✓ Talk | **Y** |
| Harpsichord passage (Cove) | ✓ `play_harpsichord` | ✓ digit keys while seated | **G** — R-20 resolved (Batch 3) |
| Moongates / moonstones | ✓ `use_moonstone`, `transitions.cpp` | ✓ Use picker ids 21–28 when carried | **G** — R-08 resolved (Batch 3); ground-marker persistence R-14 resolved (Batch 6) |
| HMS Cape | ✓ id 33 | ✓ Use picker id 33 | **G** — R-08 resolved (Batch 3) |
| Codex / endgame script | ✓ `EndgameScript`, `GameWon`/`Endgame` events appended to transcript | no longer gated — the Use chain above is reachable | **Y** — R-08 resolved (Batch 3); end-to-end endgame still unproven |
| Underworld | ✓ `exit_dungeon(true)` | ✓ | **G** logic |
| Refuge / camp scenes | ✓ `RefugeScript` | ✓ paced by `NarrativeScenePacer` | **Refuge: G** — Y-04 resolved (Batch 7B). The scene is modal, black-viewport, five accumulating phases, and `resolve_refuge` runs only after the last beat. **Camp is a separate, still-unwired script** and is not covered by Y-04's closure. |

`quest_parity` passes on a 1581-line adjudicated case table — the quest **logic** is well built. The problem is that roughly half the quest *verbs* have no player route, so **the game cannot currently be completed.**

---

## 14. PRIORITIZED REPAIR PLAN

Small, independently testable batches, in dependency order. Each batch ends at a physical test.

---

### Batch 1 — Mode ownership · **unblocks all of shops, dialogue, shrines** · risk: medium · **COMPLETED**
**IDs:** R-01 (GREEN), R-18 (GREEN), `ShrineSpecial` (§5, GREEN)
**Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (mode arbitration split out of `synchronize_after_debug`), `native/targets/tdeck/main/ui_mode_policy.h` (new, ESP-free arbitration seam), `native/core/src/ui_session.cpp` / `native/core/include/openu5/ui_session.h` (`consume`, `handle_input`, `shop_return_mode_`, `dialogue_return_mode_`, `shrine_return_mode_`), `native/core/tests/ui_session_test.cpp`, `native/targets/tdeck/host_tests/ui_mode_test.cpp` (new), `native/core/CMakeLists.txt`
**Work done:** context/terrain rebind (`context_.dungeon`/`context_.combat`) kept unconditional; the `set_base_mode` write now follows an explicit policy via `ui_mode_policy.h`: authoritative Combat overrides to Combat; authoritative Dungeon overrides to Dungeon; otherwise, if the current mode is `Shop`/`Dialogue`/`ShrineSpecial`, it is preserved; all other states resolve to Exploration. Combat, Shop and Dialogue each got their own return-mode register (`pre_combat_mode_`, `shop_return_mode_`, `dialogue_return_mode_`), captured before the session-mode overwrite. `ShrineSpecial` now captures `shrine_return_mode_` on entry and settles back to it once no new shrine modal is synchronously re-armed. `finish_combat_if_needed()` was inspected as an additional base-mode writer and left unchanged (redundant-but-harmless).
**Physical test:** enter a blacksmith, buy an item, back out through every level, confirm the world view and Exploration verbs return. Talk to an NPC, exit, confirm the same. Visit a shrine, donate, confirm recovery. **Not yet performed on hardware** — see test evidence below.
**Test evidence:** `ui_mode_regression` 28/28 GREEN. Full host suite: 58 total, 57 pass, 1 fail (`gameplay_parity` mismatch 59 — pre-existing, unrelated post-combat chest-promotion defect, R-02/R-03/R-04, untouched by this batch). T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**.
**Model:** Opus-level. This was the subtle one; four writers became one owner.

---

### Batch 2 — Loot & chest presentation (ANCHORS 1 + 2) · risk: low · **COMPLETED**
**IDs:** R-02 (GREEN), R-03 (GREEN), R-04 (GREEN)
**Files:** `native/core/src/presentation.cpp` (`compose_world_presentation` two-layer quest-object composition), `native/core/src/combat.cpp` (`finish_encounter_combat` — promotion removed), `native/core/src/outdoor.cpp` (`outdoor_start` `victory_latch` — promotion removed; a second invalid site the original audit did not identify), `native/core/tests/combat_loot_open_regression_test.cpp` (R03-DIRECT), `native/core/tests/presentation_test.cpp` (R03-OUTDOOR, R04-OVERLAY, R04-LIFO), `native/core/tests/combat_escape_regression_test.cpp` and `native/core/tests/direct_troll_handoff_test.cpp` (stale post-fix expectations corrected).
**Work done:** removed both post-combat unopened-chest promotion sites in their entirety (no replacement persistence path — unclaimed arena treasure is now correctly lost on exit, matching the reference); restructured `compose_world_presentation`'s quest-object loop into two passes — stationary/non-loot objects resolved first-match-per-cell, then loot/search resolved last-match-per-cell (LIFO) and painted unconditionally after layer 1. No renderer hack was added for R-02 (confirmed no chest-specific `+256` case was needed, `quest_world_tile()` untouched) and no `QuestObject` storage/layout refactor was performed.
**Verify:** `ctest -R gameplay_parity` — mismatch 59 is **fixed**; execution now proceeds well past sequence 59. (The run still fails, at a *different*, pre-existing, newly-exposed mismatch 2034 — tracked separately as **R-21**, confirmed unrelated to this batch; see §3 R-21 and §14 Batch 12.)
**Physical test:** troll chest → Open → Search → Get to zero → confirm the cell shows plain terrain, no blue square; confirm each icon matches the next `G` result. **Not yet performed on hardware.**
**Test evidence:** `combat_loot_open_regression` PASS (R03-DIRECT GREEN; consumed-chest guard and existing LIFO/full-inventory behavior unchanged and GREEN). `presentation_regression` PASS (R03-OUTDOOR, R04-OVERLAY, R04-LIFO GREEN; existing stationary sprite-bank tile convention GREEN). `combat_escape_regression` PASS (stale promoted-chest expectation corrected to reference behavior; escape/bridge-restoration coverage unchanged). `direct_troll_handoff_regression` PASS (same correction; combat-local Open and consumed-chest no-duplication coverage unchanged). `ui_mode_regression` PASS (Batch 1 untouched). Full host suite: 58 total, **57 pass, 1 fail** — sole failure is `gameplay_parity` at the newly-exposed mismatch 2034 (R-21), confirmed pre-existing and reproduced on the untouched `63eeac3b` baseline. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**.
**Model:** Sonnet/Codex is sufficient — the target behaviour is fully specified by the reference and by a failing test.

---

### Batch 3 — Reachability: Use picker, Yell sails, Ready context · risk: low · **COMPLETED**
**IDs:** R-06 (GREEN), R-07 (GREEN), R-08 (GREEN), R-19 (GREEN), R-20 (GREEN), Y-20 (GREEN)
**Files:** `native/core/src/display_names.cpp` (`usable_names` — real canonical ids only), `native/core/include/openu5/inventory_picker.h` + `native/core/src/inventory_picker.cpp` (shared ESP-free picker seam: widened possession input, `kUsableItemPickerMaxRows` 6 → 21), `native/targets/tdeck/main/alpha_runtime.{h,cpp}` (`open_selection` fills the widened seam input; new `refresh_session_context()` feeds the session's sail/harpsichord mirrors before every routed input), `native/core/include/openu5/ui_session.h` + `native/core/src/ui_session.cpp` (`handle_exploration` case `'y'` and the new digit cases; the two RED-test seams promoted to production-fed context mirrors), `native/core/src/commands.cpp` (context gate `ready_anywhere` exemption; Ready handler passes `battle = c.combat` and calls the resync), `native/core/include/openu5/combat.h` + `native/core/src/combat.cpp` (`load_equipment_cache()` shared by arena construction and the new `resync_player_equipment()`).

**Work done:**
- **R-07/R-08** — `usable_item_display_name()` now has exactly one index interpretation, the real canonical id; the alternate offset reading is gone. The picker seam gates one row per canonical id from its authoritative owner (`game.quest.artifacts[0..2]` for 18–20, `QuestWorldServices` moonstones `!buried` for 21–28, `game.quest.shards[0..2]` for 29–31, `game.hms_cape` for 33, `game.black_badge` for 36, plus the pre-existing 16/17/32/34/37). Rows carry the same real id later dispatched as `CommandKind::UseItem`, each resolves a real name, and none appears twice. **Grapple is removed from the Use picker entirely and remains Klimb-only.** **Pocket Watch (35) is intentionally still excluded** — no authoritative state owner exists and none was invented.
- **R-19** — `(Y)ell` aboard a frigate outside the Underworld dispatches `CommandKind::YellSails` (state-driven HOIST/FURL toggle, no modal); the word-of-power branch is unchanged. The context is a narrow mirror fed by `AlphaRuntime::refresh_session_context()` from `turn_.transport_tile` and `position.map.location`, and re-checked authoritatively by the command itself.
- **Y-20** — digits `0`–`9` dispatch `CommandKind::SetActivePlayer` with the literal digit; the core handler keeps its own `member - 1`, range/party validity and `None!`/`Invalid!` messages. No new modal.
- **R-20** — at the authored harpsichord context (small map, not combat, not dungeon, tile 141/0x8D immediately south) the digit is intercepted *before* `SetActivePlayer` and dispatches `CommandKind::HarpsichordNote` with the literal note digit. The existing melody/Cove-passage core implementation is unchanged and still GREEN. Digit interception is not broadened beyond that context.
- **R-06** — Ready is legal in world, dungeon corridor and combat. Dungeon Ready is an ordinary free action (`battle=false`, no armour lock, no turn charged). Combat Ready routes through `CommandKind::Ready`, calls `equip_item(..., battle=true)` (armour lock preserved) and, on success, calls **`resync_player_equipment()`** to refresh the acting player's `CombatActor` equipment-derived cache (`weapon_count`, `weapons[]`, `attack`, `range`, `defense`) from authoritative `GameState`. `equip_item()` itself is **not** coupled to combat state. Arena construction and the resync share one `load_equipment_cache()` helper so they cannot drift.
- **R-06 combat action cost (correction pass)** — the reference charges the acting combatant's turn **once per `R` interaction, at picker close** (`main.ts::openCombatReadyPicker` -> `closeAndEndTurn` -> `CombatSession.playerReady()` = `requirePlayerTurn()` + `advanceTurn()`), whether or not anything was equipped, and ESC is **not** a free cancel there. Implemented at the layer that owns that interaction: `UiSession::cancel_modal()` dispatches `CommandKind::CombatYield` after the cancelled `ModalResponse`, gated on `UiRequestId::Equipment` with `return_mode_ == UiMode::Combat`. Equips leave through `finish_modal()` and cost nothing, so inspecting/equipping/failing on any number of rows still costs exactly one action. World and dungeon Ready remain free; `equip_item()` stays unaware of combat and of action cost. Two presentational gaps remain (empty-handed picker, `"Ring vanishes!"` early close) — tracked as **Y-28**; neither changes the cost.

**Verify:** `ctest --test-dir native/core/build-batch1-control`.

**Physical test:** Debug → Full Test Setup → `U` and confirm every owned tool appears with the right name; board a ship and hoist sails; `R` in combat and in a dungeon and confirm the weapon actually changes; sit at the harpsichord and play `6 7 8 9 8 7 8 7 6 7 6 5 3`. **Not yet performed on hardware.**

**Test evidence:** `batch3_group_a` 19/19 GREEN, `batch3_group_b` 74/74 GREEN, `batch3_group_c` 37/37 GREEN (17 core/command-layer + 20 picker-interaction-layer checks after the R-06 action-cost correction pass). Targeted regression set all PASS: `ui_session`, `item_parity`, `command_parity`, `transport_flow_parity`, `quest_parity`, `ui_mode_regression`, `presentation_regression`, `combat_loot_open_regression`, `direct_troll_handoff_regression`, `world_flow_adapters`, `world_flow_parity`, `dungeon_parity`. Full host suite: **61 total, 60 pass, 1 fail** — the sole failure is `gameplay_parity`, still at **mismatch 2034** (R-21), byte-identical to the pre-batch baseline, with **no new earlier mismatch**. T-Deck ESP-IDF 6.1 build: **PASS** (`openu5_tdeck.bin` 0xc63f0 bytes, 23% of the app partition free; zero compiler warnings). Hardware flash: **not performed**.

**Known remaining, intentionally deferred:**
- **Pocket Watch id 35** — no authoritative state owner; excluded from the picker and from the name table.
- **Combat Ready picker presentation (Y-28)** — the action *cost* now matches the reference exactly (one per `R` interaction, at close). Two presentation gaps remain: the empty-handed case opens a disabled `"(None available)"` picker instead of charging without one, and `ItemResult::vanished` does not close the picker early. Neither affects cost. See section 3 R-06.
- **Moonstone persistence** — the picker reads current ownership only; save/load persistence stays **R-14**.
- **`syncPlayerEquip`'s invisibility rule** — the reference clears the invisible flag when ring 42 is *removed*. Not implemented here; out of the listed resync field set and uncovered by the batch contracts.
- **`CommandKind::Unready`** — still routeless by design (see section 5).
- **R-21 `gameplay_parity` mismatch 2034** — untouched and still OPEN.

**Model:** Sonnet/Codex. Mechanical, but the item-id table must be transcribed from `use-tools.ts:7` exactly.

---

### Batch 4 — Dropped modal responses and NPC-initiated sessions · risk: **medium (softlock + re-entrancy, not low)** · **COMPLETED**
**IDs:** R-09, R-10 (plus R-25/R-26 newly discovered, explicitly not fixed)
**Files:** `native/core/src/ui_session.cpp` (`finish_modal`, `cancel_modal`, `consume`), `native/core/include/openu5/look.h` + `src/look.cpp` (new `fountain_drink_result` helper), `native/targets/tdeck/main/alpha_runtime.{h,cpp}` (`consume_event`, new `drain_pending_npc_initiation`, `modal`, `open_selection`), `native/core/tests/batch4_group_a_test.cpp`, `native/core/tests/batch4_group_b_test.cpp`.
**Work actually done (differs from the original plan above, per `ALPHA20_BATCH4_ADJUDICATION.md`):** the original plan's "five `modal()` branches ... following `WellDrop`/`WellWish`" was wrong for four of the five — Blackthorn/GuardPassword/GuardTribute/GuardArrest are pure `(request, answer) -> Command` maps and belong in `UiSession::finish_modal`/`cancel_modal`, not `AlphaRuntime::modal()` (see R-09 resolution above); `FountainDrink` is not a `modal()` branch at all, it is a party-picker shape. NPC-initiated Talk/Shop reach `CommandKind::BeginConversation` via a new `UiSession::consume` case, but `AlphaRuntime` defers the actual dispatch past the outer command's return (see R-10 resolution above) rather than consuming it immediately.
**Depends on:** Batch 1 (dialogue/shop modes must survive first) — confirmed unaffected; Batch 1's return-mode registers (R-18) needed no changes.
**Physical test:** walk past a Britannia guard at night and answer/cancel the password prompt; refuse a tribute and confirm the arrest escalation; drink from a fountain; approach a shopkeeper and let them hail you. Deferred to the Batch 4 hardware checkpoint (not run this pass).
**Model:** Sonnet.

---

### Batch 5 — World spell targeting · risk: medium · **COMPLETED (fully certified in software and on physical T-Deck hardware)**
**IDs:** R-11 (**GREEN — RESOLVED**), Y-21 (**GREEN — DISCHARGED, premise was stale**)
**Files:** `native/core/include/openu5/magic.h` + `native/core/src/magic.cpp` (new `cast_target_prompt` seam), `native/core/src/ui_session.cpp` (`handle_modal` TargetSelection), `native/targets/tdeck/main/alpha_runtime.cpp` (`cast_selected_spell`), `native/core/tests/batch5_test.cpp` (new), `native/core/CMakeLists.txt`.
**Work actually done (differs from the plan above — see the R-11 adjudication in §3):**
- The "6 spells" in the original R-11 table is **3**. Only An Sanct (6), In Por (17) and An Ex Por (25) take a direction in the reference; An Ylem (5), An Grav (18) and In Ex Por (26) have **no world effect branch at all** and must not grow a fabricated prompt. The fix therefore keys on the **effect kind** (`Seal`/`Disarm`/`Blink`), not on `target_type` — which over-selects here and which R-16 already shows is unreliable.
- Removing the `context_.combat &&` guard was **necessary but not sufficient**. `UiSession::handle_modal` treated every `CommandKind::Cast` in `TargetSelection` as the combat aim *reticle*, so a direction press walked `combat_x/combat_y` and dispatched nothing, and Confirm set `has_target` — never `has_direction`, the only field `world_magic()` reads. Both arms are now gated on `request_ == UiRequestId::Target` (combat's request id), so a Cast on `UiRequestId::Direction` takes the shared world-getdir path.
- **The plan's "Care" note was backwards.** The reference consumes the charge and the mana **first** and prompts **after** (`main.ts` `doCast` → `pendingCastDoor`/`pendingCastUnlock`/`pendingCastBlink`), and cancelling refunds nothing. The existing `UiSession` cancel arm already dispatches the Cast with `has_direction=false`, which lands on `world_magic.cpp:55` and reproduces the reference exactly. **No refund logic was added**; three tests now hold that ordering in place.
- **Y-21 needed no production change.** `alpha_runtime.cpp:684` already routes scroll id 1 through `begin_target(UseTarget,"Direction?")`; the audit's "the device never supplies one" was stale. Proven host-side end to end (`batch5` B4 + C6).
**Newly discovered, deliberately not fixed:** **Y-30** (prompt/consumption ordering under the fused `world_magic()`), **Y-31** (cancelling the Rel Hur / skull-key `(U)se` getdir refunds the item, where the reference consumes).
**Tests:** `native/core/tests/batch5_test.cpp` → ctest `batch5`. RED **30/34** pre-fix (A1 ×3 + B1), GREEN **34/34** post-fix. Full suite **69 / 68 pass / 1 fail**, the fail being `gameplay_parity` mismatch **2034** / R-21 — unchanged from the pre-batch baseline.
**Firmware:** fresh ESP-IDF v6.1 build `build-batch5-world-targeting/openu5_tdeck.bin`, 829,536 bytes (0xca860), 21% of the 0x100000 app partition free.
**Physical test — GREEN, PASS.** An Sanct on a locked door, In Por blink (world gameplay), and An Ex Por on a door all passed on the physical T-Deck, each on the first test attempt. R-11 hardware validation: GREEN. Batch 5 status: fully certified in software + physical hardware.
**Model:** Opus-level — the consume-ordering interacts with the reference's bug-for-bug charge semantics.

---

### Batch 6 — Persistence gaps · risk: medium · **COMPLETED**
**IDs:** R-14 (**GREEN — RESOLVED**), R-15 (**GREEN — RESOLVED**)
**Files:** `native/core/include/openu5/gameplay_save.h`, `native/core/src/gameplay_save.cpp` (new `capture_world_objects`/`restore_world_objects`/`capture_dungeon`/`restore_dungeon`), `native/targets/tdeck/main/alpha_save.cpp` (`save()` captures via `c.quest_world`/`c.dungeon_context->state`), `native/targets/tdeck/main/alpha_runtime.cpp` (`synchronize_loaded_world`), `native/core/tests/gameplay_integration_test.cpp` (new coverage).
**Work actually done:**
- Adjudicated both items against the reference before touching production code (`game/src/core/state.ts`, `saveNative.ts`, `persistence.ts`; `re/notes/save-window-writer.md` + `dungeon-map-buffers.md` + `combat-dungeons.md`) — see the full adjudication in the R-14/R-15 Resolution subsections in §3. Two premises in the original write-up needed correcting in the process: interior objects (town/castle chests/props) were already correctly handled by the existing `hydrate_interior_objects`/`discard_interior_objects` transition hooks and needed no change; and the "moonstone burial state does not survive save/load" line was a mis-attribution — burial state is core `GameState`, already persisted, and was never part of `objects_`/R-14.
- R-14: whole-`objects_`-vector capture/restore through the existing `QuestWorldServices` callback seam, `objects_.clear()` unconditionally before restore in `synchronize_loaded_world` (fixes the cross-save leak), validate-then-commit (a domain-invalid or absent sidecar field degrades to an empty pool, never a partial one).
- R-15: **Option A** (serialize/restore `DungeonState`) confirmed as reference-correct — saving underground is authentically permitted in the DOS original, which captures live dungeon position/reveal state as a side effect of being one raw memory dump. The port's own `dungeon_`/`game_.position` split (unlike DOS's single register) means this lives in the JSON sidecar, not new `.gam` byte offsets. `pos.floor`/`x`/`y`/`facing` are validated to the exact ranges the engine indexes with (not just their storage width), because they are unguarded indices into the cell grid and the facing-delta table downstream.
**Tests:** `native/core/tests/gameplay_integration_test.cpp` → ctest `gameplay_integration`. RED = compile failure against pre-batch code (the API didn't exist yet); GREEN post-fix, 42 assertions. Full suite **69 / 68 pass / 1 fail**, the fail being `gameplay_parity` mismatch **2034** / R-21 — unchanged from the pre-batch baseline.
**Firmware:** incremental ESP-IDF v6.1 rebuild of `build-batch5-world-targeting/openu5_tdeck.bin`, 835,712 bytes (0xcc280, up from 829,024 / 0xca860 pre-batch), 20% of the 0x100000 app partition free; zero compiler warnings.
**Physical test:** **not performed** (no hardware access in this session) — the physical-test script from the plan (open a chest/walk away/save/reload; search a tree for the skull key/reload; save in a dungeon and observe) remains open for hardware validation.
**Model:** Sonnet.

---

### Batch 7 — Missing feedback renderers · risk: low → **actually mixed; adjudication corrected the plan** · **PARTIALLY COMPLETED**
**IDs:** R-12 (**GREEN — RESOLVED**), R-13 (**GREEN — RESOLVED**), Y-04 (**PARTIAL at the time of Batch 7** -- Quake resolved; `CellExplosion`/`CellProjectile`/`PoisonTick`/`Refuge`/`TrollSneak` were left open and are **now resolved in Batch 7B, below**)
**Files:** `native/core/include/openu5/presentation.h`, `native/core/src/presentation.cpp` (`reveal_all`, `quake_offset_at`), `native/core/tests/presentation_test.cpp`, `native/targets/tdeck/main/alpha_runtime.{h,cpp}` (`consume_event`, `render()`, `handle()`), `native/targets/tdeck/main/native_renderer.{h,cpp}` (`render_zodiac_view`, `shift_viewport_vertically`, `recompute_viewport_crc32`).
**Work actually done (differs from the plan above -- the original "risk: low" / "`start_magic_ceremony` is the proven template" framing was wrong for four of the six items, discovered during adjudication, per this batch's own read-first instruction):**
- **R-12 (MapReveal)** is not shaped like `MagicCeremony` at all. The reference's `revealViewport` is a *modal* (input-swallowing), *real-time* (not turn-gated) bypass of the world's light-radius/wall censorship -- not an async non-blocking overlay. The genuine fix (`compose_world_presentation`'s new `reveal_all` parameter) lives in **host-testable** `native/core`, not device-only code, which the original plan's file list did not anticipate.
- **R-13 (Zodiac)** *is* shaped like `GemView` (any-key-closes, no deferred turn charge here specifically) -- the audit's "no renderer" framing was exactly right, and the core-side `ZodiacView` computation (`look.cpp`) was already correct and untouched.
- **Y-04's `Quake`** genuinely fit the "timed, non-blocking" template, but even it needed a real reference-derived waveform (`quake_offset_at`, 2px/8-pulse/117ms square wave) plus a sustained-shake extension rule, not a bare timer copy of `start_magic_ceremony`.
- **Y-04's remaining five channels do not fit the template and were not forced into it.** `CellExplosion`/`CellProjectile` need a new per-cell temporary-tile compositor (plus an ordering question against the world-object mutation the reference's own `#243` note says already commits synchronously); `PoisonTick` needs a net-new party-roster row-invert renderer that does not exist anywhere in native; `Refuge`/`TrollSneak` each need a full modal scene pacer, the same class of work as the Blackthorn capture scene across Batches 4.5B-4.5D. Implementing any of these as a rushed "consume the event" bolt-on would have produced exactly the half-finished, non-reference-faithful result this program's own instructions warn against.
**Tests:** `native/core/tests/presentation_test.cpp` → ctest `presentation_regression`. RED (compile failure against pre-fix source, confirmed via `git stash` of only the two `presentation.{h,cpp}` files) → GREEN for both the `reveal_all` bypass and the `quake_offset_at` waveform. Full suite **69 / 68 pass / 1 fail**, the fail being `gameplay_parity` mismatch **2034** / R-21 — unchanged from the pre-batch baseline.
**Firmware:** fresh ESP-IDF 6.1 build `native/targets/tdeck/build-batch7-feedback-renderers/openu5_tdeck.bin`, 0xccad0 (836,816) bytes, 20% (0x33530 bytes) of the 1 MiB app partition free, zero compiler warnings from any project file. Hardware flash was not performed (no physical device attached to this session).
**Physical test:** not performed this session (no hardware). Outstanding: cast Wis An Ylem or read the In Quas Wis scroll and confirm the world reveals and re-censors on schedule without eating a keypress afterward; `(U)se` the spyglass at night and confirm the star field closes on any key with no turn charged; trigger a quake (harpsichord passage, or destroy a shard) and confirm the 11x11 window shakes without the HUD/frame moving.
**Model:** Sonnet.

---

### Batch 7B — Remaining Y-04 feedback channels · risk: medium, mixed scope · **COMPLETED**
**IDs:** Y-04 (**GREEN — RESOLVED**, all five remaining channels), Y-32 (**OPEN — newly observed, not addressed**)
**Files:** new `native/core/include/openu5/world_fx.h` + `src/world_fx.cpp`, `openu5/poison_tick.h` + `src/poison_tick.cpp`, `openu5/narrative_scene.h` + `src/narrative_scene.cpp`, `native/core/sources.cmake`; `openu5/ui_session.h` + `src/ui_session.cpp` (`append_continuation`); `native/targets/tdeck/main/alpha_runtime.{h,cpp}`, `native_renderer.{h,cpp}` (`paint_world_fx_dot`), `device_ui_views.h` (`DevicePartyHighlight::damage_flash`), `tdeck_board.{h,cpp}` (reverse-video roster row); tests `native/core/tests/batch7b_test.cpp` (new, ctest `batch7b`), `batch7b_red_test.cpp` (new, RED harness, not registered), `ui_session_test.cpp`.

**Adjudication first, per channel. The audit's own prior framing was corrected on one point:** it said the five channels "do not share one abstraction -- do not merge them just because one helper could technically touch all three shapes." That is right for two of the three groupings and **wrong for one**. In the reference, `CellExplosion` and `CellProjectile` are *literally one module and one live-fx list* (`game/src/skin/world-fx.ts`, one `WorldFxLayer`, one `paint()`), and `Refuge` and `TrollSneak` are *literally one shape* (an ordered beat queue with a per-beat dwell, modal input, and a deferred tail). Splitting either pair would have been the divergence. `PoisonTick` is genuinely its own primitive and was kept separate.

**Per-channel record.**

**`CellProjectile` — one flight per shot, sub-cell, presentation-only.** Reference: CMDS.OVL asks for the flight ONCE per shot, never once per cell (`push origX / origY / dstX / dstY / 1` at all three call sites: broadside-with-impact 0x0A45, broadside-without 0x0AD2, cannon-on-foot 0x0CFE). The origin differs by surface and this is not decoration: a broadside pushes `5 / 5` (0x0A48-0x0A49), the window centre, so the ball leaves the **ship** at offset (0,0); on foot the origin is `dir±1 + 5` (0x0BAD/0x0BB9) and is never rewritten in the loop, so it leaves the **cannon's** cell. Path length is Chebyshev, minimum 1. The frame cadence is **not derivable** — the flight sits behind a far call whose segment relocates on load (#311/#313) — so 55 ms/cell is borrowed from the already-calibrated combat projectile and is **declared Class C, on loan, with its own constant** (`kWorldFxProjectileMsPerCell`) so a future cannon witness can recalibrate it without touching combat's. The ball lives *between* cells, so it is a fractional-position dot, not a tile blit. Previous native behaviour: `f` raised the reticle and fired; the event was emitted and dropped, nothing was ever drawn, and no path or cadence logic existed anywhere in native. **Verdict: was missing entirely; now reference-faithful.** Simulation does **not** wait for it: the core already resolved the shot synchronously and the flight is drawn over the committed result, exactly as the reference does. No world tile is written to display it — `apply_world_fx` refuses a Dot op by construction, and the test asserts the composed window is byte-unchanged while a flight is live.

**`CellExplosion` — the same primitive, different shape.** Reference: `CAST.OVL 0x16f4` to `explosion_fx_at_cell` (`ULTIMA.EXE 0x3522`), `bursts` times over one cell — blit of tile 0 (`Explosion` in TileData) + noise burst + `viewport_redraw`. The count and the cell are derived (7 and `(party_x, party_y - 1)` for the shard ritual; 1 and slot 1's last cell for the Blackthorn sacrifice, 0x0414-0x041E); the inter-blit cadence is Class C, because it is not a readable constant — it is whatever `viewport_redraw` costs per frame. The blits **alternate** presence and absence, because the original repaints the viewport between them; painting seven back to back would be one tile held still for 420 ms. **The `#243` ordering question is answered the way the reference answers it, and the alternative was explicitly rejected:** `under_tile` (the raw slot byte, 0xFC in the ritual) is held on the cell for the fx's entire lifetime, including its lead and pause, so the Shadowlord is what the player sees under the burst even though `quest_world.cpp` has already erased the object synchronously. Deferring that mutation instead would have moved the ritual's lifetime into the core — a save or a power-off mid-scene would lose the deed, and every non-interactive consumer would stop committing. `lead_ms` is the native counterpart of the reference's `cerrarVentanaDeSacudida`: an effect that follows a shake must not start inside it, so the device passes the remainder of its own live quake window. Native has no audio catalogue, so the audio half of the reference's lead is not modelled and is declared, not faked. Previous native behaviour: event emitted, dropped, nothing drawn; the cell showed the Flame from the first frame. **Verdict: was missing, and the ordering defect the audit predicted was real; both fixed.**

**`PoisonTick` — the shared roster-row inversion.** Reference: `kernel_turn_housekeeping` 0x2AE8 walks the slots ASCENDING and calls `kernel_apply_damage(i,1)` per poisoned member; 0x2a52 is damage *and* presentation, in this order — invert the row (0x2a59), noise burst (0x2a68), un-invert (0x2a6e, an XOR, so the second call restores), *then* subtract the HP (0x2a7b). 0x2a28 is the **shared** row-inversion routine: the `select_party_member` picker cursor and the combat hit use the same one, which is why the reference publishes a row index rather than painting anything of its own, and why a precedence rule is needed (`roster_invert_row`: damage flash > Ztats cursor > picker cursor > combat actor). The blip is **93 ms exactly** — not an eyeballed number, but `samplesToMs(1600 x 1.5, 1)` at 24000/0.93 Hz, i.e. the rendered length of the very cue 0x2a68 programs. The original blocks, so N poisoned members are N complete consecutive sequences in slot order, never overlapping. **It is not modal** — it swallows no input and defers no turn, because the tick fires on every step. **One declared divergence, inherited from the reference and taken for the same reason:** native, like the TypeScript port, has already committed the HP subtraction in `turn.cpp` before the event is emitted, so the flash presents damage that has already landed rather than preceding it as at 0x2a7b. Deferring a committed HP write to a wall-clock presentation would desynchronise saves, reloads and non-interactive consumers; nothing the player observes changes. Previous native behaviour: event emitted and dropped; **no roster row-inversion primitive existed anywhere in native**, for the poison tick, the picker cursor or the combat hit. **Verdict: primitive was absent; now present, shared, and precedence-ordered.**

**`Refuge` — a staged modal scene, not a one-frame event.** Reference: `BLCKTHRN.OVL 0x0910 party_refuge`. Observable beats, in order: (1) the viewport goes **black** (0x0962) with the Avatar alone in the centre, and `An unending darkness engulfs thee...` prints; (2) `Thou hast found refuge.`; (3) `No evil lives here, only peace and darkness.`; (4) `But thy slumber is disturbed!`; (5) `Someone shouts`; (6) `"FORTIS FORTUNA AVENTARI"`; (7) the **left** spectral figure is blitted, tile 0x5e at (col2,row7) (0x0a70); (8) the **right** one, tile 0x5f at (col8,row7) (0x0aa2); (9) `There is a peal of thunder!`; (10) and (11) two thunder cues (0x0acc/0x0acf); (12) the cyan **apparition**, tile 0x174 at (col5,row2) (0x0ae9); (13) Lord British's karma speech, indexed by the karma **at death**; (14) `Strange words are intoned.`; (15) `Vertigo...`; (16) the transition flash (0x0bc4). Confirmed against video-M f042 (black viewport, Avatar alone) and f058 (apparition top-centre, both figures below, speech in quotes); authority `re/notes/death-resurrection-audit.md` section 3. Progression is **fixed presentation time, not input**: no beat waits on a key and there is no dismissal key; the scene ends by itself. Dwell = raw `delay` (0x7e6a) units x 70 ms plus a floor — 900 ms on beats that print a line so it can be read, 260 ms on the figure/sound transitions, which in the original are nearly immediate (the three numbers are the reference presenter's own, Class C, calibrated against video-M). Input is swallowed throughout. **State order is the point of the design and is preserved:** `check_refuge` mutates nothing and only latches `refuge_pending`; the roster keeps showing the **fallen** party for the whole scene; the stage comes down first and only then does `resolve_refuge` revive the party, floor karma at 75, set the clock to 6:00 and wake them in Lord British's castle. Previous native behaviour: `check_refuge` emitted the script, nobody consumed it, and the party was silently resurrected by the *next* turn's death check — no scene, no narration, no karma speech. **Verdict: the scene did not exist; now staged, paced and correctly ordered.**

**`TrollSneak` — the same sequencer, its own numbers.** Reference: MAINOUT 0x1c0e-0x1ca6. The pauses come from kernel 0x3AE6, which is **not** a beep but run-n-frames (n x [frame tick 0x5910 + delay 0x20fa(1)]) — a mute pause that keeps rendering. `pause(10)` after `Thou spieth trolls under the bridge!` (0x1c19), and `pause(5)` before **each** dot of `$ sneaks across...` (0x1c56-0x1c65); the third dot prints dry. One unit = one INT 1Ch tick ~ 55 ms, a derived tracing rather than a calibration, and it matches the aulddragon P02 26:20 witness (preamble to name ~ 700 ms, dot to dot ~ 200-300 ms). **The dots continue the line already on screen**, which is why `UiSession::append_continuation` was added — pushing each dot as its own transcript block would have read nothing like the reference. The rest of the turn (`Caught!` and the toll prompt, or nothing) is **deferred** until the beats run out; in the original all of it is synchronous inside the turn, so nothing legitimate can slip in between. Input is swallowed; nothing is mutated (the rolls already happened in the ambush). **Deliberately NOT assumed identical to Refuge:** it uses a different time unit (55 ms vs 70 ms), has no reading floors at all (which is what lets its dry beats coalesce, exactly as the reference's own drain loop does), mounts no viewport scene, and ends by releasing a deferred tail rather than by applying a mutation. Previous native behaviour: event emitted and dropped; the whole crossing was invisible, and `Caught!` + the toll prompt arrived instantly with no preamble. **Verdict: was missing entirely; now staged, with the turn correctly deferred behind it.**

**The shared sequencer.** `NarrativeScenePacer` (`openu5/narrative_scene.h`) is the one mechanism Refuge and TrollSneak share, and it is deliberately small: an ordered step queue (beats + deferred events) over caller-owned storage, a per-beat dwell in milliseconds, `enqueue()` taking ownership of the rest of the turn from the first scene event onward, `pump(now_ms, beat_sink, event_sink)` releasing only what is due, and `take_completion()` reporting the finished scene exactly once so the owner — not the pacer — applies any state mutation. It owns no game rules, no clock and no RNG; it never sleeps, never blocks and never spins. `set_paced(false)` drains synchronously, which is the escape hatch every automated harness needs and the same one the reference pacers have. `BlackthornScenePacer` was **not** generalised into it: its stage/slot model, room-patch format and key-wait states are specific to that scene, and folding them together would have been a bigger, riskier change than writing the 200 lines these two actually need.

**Pacing instrumentation (for Y-32).** The property Y-32 will need to measure is exactly the one this sequencer makes assertable: a beat becomes visible, and **cannot** be replaced before its dwell has elapsed. `resume_at_ms()`, `waiting()`, `released_steps()` and `queued_steps()` expose it, and `batch7b` D8/D9 and E23/E24/E26 already pin it for both scenes — including the negative half (no advance one millisecond early) and the "no state mutation while the scene is on screen" half. No tracing framework was built. All reference-derived timing constants are named and documented at their definition: `kSceneFrameUnitMs` (55, run-n-frames), `kRefugeUnitMs`/`kRefugeTextFloorMs`/`kRefugeSceneFloorMs` (70/900/260, Class C), `kWorldFxPauseUnitMs` (55, reused from the existing scene unit rather than re-invented), `kWorldFxExplosionBurstMs` (60, Class C), `kWorldFxProjectileMsPerCell` (55, Class C on loan), `kPoisonBlipMs` (93, derived from the cue the binary programs).

**Declared adjacent gap, NOT fixed and NOT a Batch 7B regression:** `resolve_refuge` sets the destination position directly instead of routing through `load_small_map`, so it emits no `ReloadEffect::EnterNpcs`/`ClearEnemies`/`RefreshHourTiles` and the party wakes in an unpopulated Lord British's castle. This predates Batch 7B — at HEAD the same `resolve_refuge` already ran, one turn later, from the next death check — but the staged scene now makes the arrival something the player watches rather than something that happens between turns, so it is worth recording. It is a core-rules gap, not one of the five presentation channels, and fixing it would touch `quest_parity`/`gameplay_parity` surface; out of scope for this batch.

**Tests:** `native/core/tests/batch7b_test.cpp` gives ctest `batch7b`, **264 checks GREEN**, covering all five channels plus the Batch 7 channels this batch must not regress (Quake's waveform, MapReveal's censorship bypass). `ui_session` grew from 271 to **277** checks for `append_continuation` (continuation, empty no-op, cross-channel degradation, orphan continuation).
**RED evidence:** `native/core/tests/batch7b_red_test.cpp`, built and run against a **`git worktree` of the untouched HEAD `722ed36f`**, states the three Y-04 claims that can be expressed against the pre-batch API and fails **7 of 7 at runtime** (not at compile time): R1a/R1b/R1c — the TrollSneak preamble, the continued dot line and the outcome never reach the transcript; R2a/R2b — the Refuge narration never reaches the transcript; R3a/R3b — a `CellExplosion` with `under_tile=252` puts neither the Shadowlord nor the burst on its cell. The remaining two channels have **no pre-fix seam at all** and their RED is therefore compile-level, which the RED file states explicitly rather than faking: nothing in pre-batch native publishes a roster-flash row (`PoisonTick`) or computes a flight path or cadence (`CellProjectile`), so there is no value a HEAD-compilable test could have read. Each RED claim is restated through the new mechanism in `batch7b_test.cpp` and is annotated there with the RED id it discharges (D5, D15, D16, E21, E28, B11, B12).
**Full suite:** **70 total, 69 pass, 1 fail** (up from 69/68/1 — the +1/+1 is the new `batch7b` target). The sole failure remains `gameplay_parity` mismatch **2034** / R-21 — unrelated, pre-existing, untouched, not rebaselined and not suppressed.
**Firmware:** fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-batch7b-feedback-scenes/openu5_tdeck.bin`, **0xce1f0 (844,784) bytes**, **19% (0x31e10 = 204,304 bytes) of the 1 MiB app partition free** (Batch 7 was 0xccad0 / 20%; the +5,920 bytes are the three new core modules and their device wiring). Bootloader 0x5850 bytes, 31% free. **Zero compiler warnings or errors from any project file**; the only warnings in the log are ESP-IDF's own `component_validation.cmake` notices, identical to the Batch 7 build's.
**Physical test — NOT performed (no hardware in this session).** The distinction matters and is stated plainly: the core primitives are **host verified** (264 checks), the firmware **builds clean**, and **hardware behaviour is unverified** for every one of the five channels, because all of the pixel work (`alpha_runtime.cpp`'s render wiring, `native_renderer.cpp`'s projectile dot, `tdeck_board.cpp`'s reverse-video roster row) is ESP-only and outside host coverage — the standing Y-05 gap. Outstanding physical checks: fire a broadside and a land cannon and watch the ball cross the cells; destroy a shard and confirm the Shadowlord stays visible under the seven bursts and that they flicker rather than holding; take poison damage with two or more poisoned members and confirm the rows invert one at a time in slot order; wipe the party and watch the refuge scene stage the black viewport, both figures and the apparition at the reference cadence before the castle appears; get ambushed at a troll bridge and confirm the dots land on one line at ~275 ms apart before `Caught!`.
**Model:** Opus.

---

### Batch 8 — Spell metadata adjudication · risk: low, but **research-heavy** · **GREEN — RESOLVED**
**IDs:** R-16
**Files:** `native/core/src/magic.cpp` (`kSummaries`, `kTargets`), `native/core/include/openu5/magic.h` (new `MagicEffect::Unlock`), `native/core/tools/generate-magic-fixtures.ts` (`effects[26]`, regenerates `magic_tables.inc`), `native/core/tests/{display_names_test.cpp,batch5_test.cpp}`
**Outcome:** blanket regeneration from `SpellDef::target_type` was rejected after adjudication — target_type is the unreliable field, not the label, for 11 of the 12 flagged `kTargets` entries. Per-item adjudication instead: 9 `kSummaries` + 1 `kTargets` (An Tym) confirmed defects and fixed; `kEffects[28]` (In Zu) confirmed correct as `Line`, not touched; `kEffects[26]` (In Ex Por) confirmed a genuine defect by a 2026-08-07 RE correction that reverses the belief this very file's Batch 5 section relied on, reclassified to a new `MagicEffect::Unlock` at the metadata level only — wiring the real door-unlock gameplay effect is deferred as a tracked follow-up (RE's own recommendation, "se arregla APARTE"). `SpellId::Nox`'s "out-of-bounds" claim was refuted by direct inspection (already guarded). See the Resolution subsection under R-16 above for the full adjudication table, evidence, and test results.
**Physical test:** not performed this batch (no hardware in this session); firmware builds clean and is unchanged in size beyond the metadata/enum diff.
**Model:** Opus.

---

### Batch 8B — In Ex Por runtime verification · risk: low · **GREEN — CONFIRMED DEFECT, FIXED**
**IDs:** follow-up to R-16 / Batch 8.
**Task:** determine whether native In Ex Por (Unlock) runtime behavior was already correct, given that prior physical T-Deck testing had plausibly exercised Magic Lock/Unlock successfully around Blackthorn; modify production code only if a real defect was proven.
**Trace:** `cast_target_prompt()` (`magic.cpp`) only armed `WorldDirection` for `Seal`/`Disarm`/`Blink` — `Unlock` fell through to `None`, so a world (C)ast of In Ex Por never set `has_direction`. `world_magic.cpp`'s `fx==MagicEffect::Seal||fx==MagicEffect::Disarm` branch had no `Unlock` sibling, and its pre-flight mutable-terrain guard named only items 6/25. `dungeon_orchestration.cpp`'s Cast dispatch has no Seal/Unlock branch either (confirmed this is reference-faithful for both spells — the RE-derived worker writes "el mapa vivo," the town/world coordinate grid, not a dungeon's per-cell `d.cells[]` state, so An Ex Por, already shipped, has no dungeon branch and needed none). `combat_cast()` (`combat.cpp`) has no Seal/Unlock handling either, and needs none: a locked door isn't a combat-representable target, matching An Ex Por's existing combat behavior. Net: casting In Ex Por in the world consumed the charge and MP, printed nothing, and left every tile untouched — a silent no-op, not the RE-confirmed unlock. This matched Batch 8's own explicit "Remaining OPEN items" entry, not a rediscovery.
**Verdict:** **CONFIRMED DEFECT** (not already correct, not accidentally correct). The task's premise that hardware testing "likely" exercised Magic Unlock successfully does not hold against the source: no path in `native/core` ever mutated a door tile for spell #26 before this batch.
**Fix (smallest reference-faithful change, reusing the existing Skull Key seam):**
- `magic.cpp`: `cast_target_prompt()` adds `MagicEffect::Unlock` to the `WorldDirection` group.
- `world_magic.cpp`: pre-flight guard extended to item 26; new `fx==MagicEffect::Unlock` branch (sibling to, not sharing, the Seal/Disarm block — deliberately **not** falling through to An Sanct's trapped-chest fallback) applies the exact 0x97→0xB8 / 0x98→0xBA transform `commands.cpp`'s Skull Key handler (`CommandKind::UseItem` case 17) already uses, gated by `q->volatile_tile`.
- `magic.h`/`magic.cpp` comments updated to stop describing the mutation as deferred.
- Dungeon (`dungeon_orchestration.cpp`) and combat (`combat.cpp`) dispatch untouched — both are correctly out of scope per the trace above.
**Tests:** `native/core/tests/batch5_test.cpp` — A1 moves In Ex Por into the direction-prompting group, A4 pins dungeon `None`, and five new C-series cases (C1v–C1z) exercise the real `world_magic()` Cast dispatch: unlocking both door-tile variants (0x97→0xB8, 0x98→0xBA), "No effect!" + charge-still-spent on an invalid target, the cancelled-getdir ordering guard (charge/mana spent, "Cancelled.", no terrain change), and a full An Ex Por → In Ex Por lock/unlock round trip on the same door. All new and pre-existing `batch5` assertions pass (46/46).
**Verification:** full host `ctest` **68/70** (identical to the pre-fix baseline captured before any edit) — the only failures are the pre-existing, unrelated `gameplay_parity` (still mismatch **2034**, unchanged index and content) and the `quest_parity` GCC/w64devkit environment crash (`STATUS_ACCESS_VIOLATION`), matching this task's own "known unrelated conditions." `magic_parity` and `typescript_magic_fixture_drift` unaffected (this batch touches no tracked numeric field). Confirmed by static trace that `gameplay_parity`'s scripted fixtures never cast item 26 at a reachable location against a door tile (the only door-targeting fixture list is `for(const item of [6,25])`; the all-items fixture at line 101 casts from the outdoor map, location 0, which In Ex Por's own `time_bits` mask (0x05 = town+combat) already rejects with "Not here!" before reaching the new branch, on both sides of this change) — so no hidden divergence exists beyond mismatch 2034.
**Firmware:** fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-batch8b-in-ex-por/openu5_tdeck.bin`, **0xce310 (846,096) bytes**, 19% of the 1 MiB app partition free (288 bytes larger than Batch 8's metadata-only build, expected — this batch compiles a real new branch). Clean build, zero warnings (full log re-checked for "warning"/"error", none found).
**Physical test:** not performed this batch (no hardware in this session).
**Model:** Sonnet 5.

---

### Batch 9 — Dungeon presentation (ANCHOR 4) · risk: **high** · **GREEN for the presentation LOGIC; the ASSET half stays OPEN**
**IDs:** R-05, plus §12 dungeon rows 8–13, 18–21.
**Scope note.** The batch brief that ran this work forbade a broad renderer rewrite, which is precisely what the asset half (pack-format bump + slice blitter) is. Batch 9 therefore took the half that the brief did ask for — "verify that dungeon state is presented correctly to the player", state → derived visible state → redraw — in full, and left the art. The two halves are separable, and doing the logic first was not wasted: the blitter consumes the plan this batch built.
**Method.** Verification-first. `render_dungeon_view()`'s decision-making was ported verbatim into a new host-testable core seam, then `dungeon_view_regression` was written against the reference and run against that port: **38 failing assertions**, each traceable to a named DUNGEON.OVL routine. Only then was production changed.
**Findings.** Eight proven presentation defects + one already-correct behaviour whose audit row was a false alarm (row 8's torus-wrap concern — the wrap is deliberate) + one already-correct redraw chain (every dungeon mutation is visible on the frame that produced it; `dirty_` is set on every input and the board's cache key is a CRC over the real pixels). Full matrix in §3 R-05.
**Files:** `native/core/include/openu5/dungeon_view.h` + `src/dungeon_view.cpp` (**new**), `native/core/src/dungeon.cpp` (shared `dungeon_wall_variant()`), `native/core/include/openu5/hud.h` + `src/hud.cpp` (`hud_dungeon_bands()`), `native/targets/tdeck/main/native_renderer.{h,cpp}` (paints the plan), `native/targets/tdeck/main/tdeck_board.{h,cpp}` (bands over the two strips), `native/targets/tdeck/main/alpha_runtime.cpp` (call sites), `native/core/tests/dungeon_view_test.cpp` (**new**), `native/core/{CMakeLists.txt,sources.cmake}`.
**Tests:** `dungeon_view_regression`, 13 blocks D1–D13, including D13's mutation→visibility drive through the real `dungeon_action()`.
**Verification:** RED 38 → GREEN 0. Full host `ctest` **69/71**, the two failures being the known unrelated `gameplay_parity` (R-21, mismatch **2034**, unchanged) and `quest_parity` (GCC/w64devkit `STATUS_ACCESS_VIOLATION`); pre-edit baseline was 68/70 with the same two.
**Firmware:** ESP-IDF 6.1, `build-batch9-dungeon-presentation/openu5_tdeck.bin`, **0xcf150 (848,720) bytes**, 19% free, zero warnings.
**Physical test:** the Phase 6 block of §16, steps 33a–33f below. Not performed this batch.
**Model:** Opus 5.

### Batch 9B — Dungeon RUNTIME (input + identity) · risk: **medium** · **GREEN; the authored-ART half is deferred to Batch 9C**
**IDs:** R-05 part 2 of 3, plus §12 dungeon controls row (previously a false negative).
**Trigger.** The first physical T-Deck session after Batch 9: dungeon commands non-functional, and the location strip reading `Serpent's Hold` inside Deceit.
**Method.** Input-edge-inward trace, then reference diff. A scratch host harness first reproduced the whole semantic path (trackball and matrix key through `UiInputAdapter`, `UiSession`, `dispatch_world_command`, `dungeon_action`) and showed it **working**, which ruled out a modal leak, a mode-arbitration bug, a darkness gate and a stale-dirty renderer. The defect was then found by diffing `handle_dungeon()` against the reference's own DUNGEON dispatcher: six commands missing, one mis-bound, and three core capabilities (`DungeonAction::TurnAround`, `DungeonAction::Drink`, `dungeon_klimb_choice()`) with **zero callers anywhere in the tree**.
**Findings.** Two independent pre-existing defects, neither introduced by Batch 9 (whose diff touches no input code): **(A)** the dungeon key map was a subset of the reference's — most consequentially `(I)gnite`, so a party could not make light from inside a dark dungeon; **(B)** the HUD location caption read `GameState::position.map.location`, which a dungeon session deliberately never updates because it is the surface RETURN context. Full matrix in §3 R-05 part 2.
**Files:** `native/core/src/ui_session.cpp` + `include/openu5/ui_session.h` (dungeon dispatcher, the two prompt mirrors, the `DungeonDrink` modal arm, the `DungeonCommand` `TargetSelection` resolution), `native/core/src/hud.cpp` + `include/openu5/hud.h` (`HudDungeonBands::dungeon_id`), `native/targets/tdeck/main/location_names.h` (**new** `hud_location_caption()`), `native/targets/tdeck/main/tdeck_board.cpp` (caption call site), `native/targets/tdeck/host_tests/dungeon_input_test.cpp` (**new**), `native/core/CMakeLists.txt`.
**Tests:** `dungeon_input_regression`, 65 checks across D-IN-1..6 and D-LOC-1..2, driving the **real** physical input path rather than calling `dungeon_action()` directly — the gap that let both defects through.
**Verification:** RED **24/65** against a worktree at `f3a2f0ec` with additive shims only; GREEN **0/65**. `dungeon_view_regression` unchanged and still passing. Full host `ctest` **70/72**; the two failures are the known unrelated `gameplay_parity` (R-21, mismatch **2034**, unchanged) and `quest_parity` (`STATUS_ACCESS_VIOLATION`). Baseline measured this batch: 69/71 with the same two.
**Firmware:** ESP-IDF 6.1, `build-batch9b-input/openu5_tdeck.bin`, **0xcf540 (849,216) bytes**, 19% free, zero compiler warnings, **+1,008 B** over Batch 9's 0xcf150 (**848,208** — see the decimal correction in §3 R-05 part 2).
**Physical test:** §16 Phase 6B, steps 33g-33l. **Not performed** — no hardware in this session.
**Scope note.** Batch 9B's brief makes a physical checkpoint a **hard gate** before authored-art integration ("If hardware input is still broken: STOP"). That checkpoint cannot be run from this session, so the art half was deliberately left unstarted rather than built on an unverified foundation. The read-only asset inventory the brief asks for *before* any encoding decision **was** completed, and its measurements are in Batch 9C below.
**Model:** Opus 5.

### Batch 9C — Authored dungeon ART · risk: high · **DONE**
**IDs:** R-05 part 3 of 3, §12 rows 10-11, 18-21.
**Files:** `native/tools/u5pack/alpha1-dungeon-art.ts` (new) + `alpha1.ts`; `native/core/include/openu5/dungeon_art.h` + `src/dungeon_art.cpp` (new); `native/core/tests/dungeon_art_test.cpp` (new); `native/core/tools/check-dungeon-art-identity.ts` (new); `native/targets/tdeck/main/dungeon_art_cache.{h,cpp}` (new); `native_renderer.{h,cpp}`, `alpha_resources.{h,cpp}`, `alpha_runtime.{h,cpp}`; both `CMakeLists.txt`.

**Asset inventory — measured in Batch 9B, re-verified byte for byte this batch:**

| Bank | Source | Images | Dimensions | 4bpp | +1bpp mask |
|---|---|---|---|---|---|
| Perspective | `DNG1.16` | 26 of 28 slots | h=164, w in {8,16,24,32,56,80} | 54,448 B | n/a |
| Perspective | `DNG2.16` | 26 of 28 | same | 54,448 B | n/a |
| Perspective | `DNG3.16` | 26 of 28 | same | 54,448 B | n/a |
| **DNG total** | | **78** | | **163,344 B** | |
| Features | `ITEMS.16` | 20 | 40x80, 24x56, 40x24, 24x32, 16x24, 16x16, 8x8 | 8,160 B | **10,200 B** |
| Wanderers | `MON0-7.16` | 48 | 24x66, 16x25, 8x6 | 16,256 B | **20,320 B** |
| **All authored dungeon art** | | **146** | | | **193,864 B** |

Every Batch 9B figure held exactly. The two empty DNG slots are 8 and 24 — the depth-0 entries of front bases 8 and 24, which a front op can never reach because the driver only emits one at si > 0.

**The authoritative mapping**, reconstructed from `DUNGEON.OVL` + the DATA.OVL tables and cross-checked against `game/src/skin/fiel/dungeon.ts` and the accepted container parsers. Every anchor below was **derived, not assumed**: the side-slice widths are exactly what makes the four rings of each side abut (left 16→96, right 96→176), and tables 0x2e72 `[56,72,80,88]` and 0x2E2A `[72,80,88]` are exactly `96 − width` for the near feature and wanderer images — which is why the resolver anchors centred pairs at `96 − w` rather than carrying a second table. `dungeon_art_regression` re-derives both from the dimension tables, so a bad edit cannot silently move the art.

| Plan op | Bank | Image | Destination | Flips | Order |
|---|---|---|---|---|---|
| Side, plain wall | DNG*n* | `0 + depth` | `x` from plan (0x2e62), `y=14`, natural size | right slice mirrored | near→far |
| Side, door | DNG*n* | `4 + depth` | same | same | |
| Side, open passage | DNG*n* | `16 + depth` | same | same | |
| Side, alcove (special wall) | DNG*n* | `20 + depth` | same | same | |
| Front, dead end | DNG*n* | `8 + depth` | **pair**: `96−w` normal, `96` mirrored | H only | at first blocker |
| Front, door | DNG*n* | `12 + depth` | same pair | H only | |
| Front, special wall | DNG*n* | `24 + depth` | same pair | H only | |
| Feature, ladder up | ITEMS | `0 + si` | pair at `96−w`, `y` = 0x2e82 `[15,39,71,87]` | H **+ V flip** | block 1 |
| Feature, ladder down | ITEMS | `0 + si` | pair, `y` = horizon 96 | H only | block 2 |
| Feature, fountain | ITEMS | `4 + si` | pair, `y` = horizon 96 | H only | |
| Feature, trap | ITEMS | `8 + si` | pair, `y` = 0x2e7a `[152,120,104,96]` | H only | gated on `(sub&7)==0` |
| Feature, closed chest | ITEMS | `12 + si` | same floor `y` | H only | |
| Feature, open chest | ITEMS | `16 + si` | same floor `y` | H only | |
| Feature, magic field | — | **none** | — | — | sparkle subsystem, no ITEMS art |
| Wanderer | MON*bank* | `frame*3 + depth−1` | pair at 0x2E2A, `y` = 0x2E32 row 0 | H only | far→near |
| Wanderer, ceiling | MON*bank* | same | same, `y` = 0x2E32 **row 1** | H only | |

`LadderUpDown` carries **both** feature blocks — the V-flipped up block first (`fn_1952` @0x1984), the normal down block second (@0x19ae) — which is why one op can expand to four blits.

**Masks and mirroring.** DNG slices are fully opaque (floor speckle and ceiling are baked in). ITEMS and MON carry the container's own 1bpp MSB-first AND-mask where **bit 1 = background**; transparency comes from it and never from colour-0 keying. A blit occupies `[x, x+w) × [y, y+h)`; `mirror` flips horizontally inside that box and `vflip` vertically, which is exactly what the binary's 0x8b7c / 0x8a2c primitives do.

**Paint order.** Black background → the plan's ops in order → each op's blits in order. The plan already emits corridor rings near→far with the front wall at the first blocker and contents far→near; expansion **preserves** that, which `dungeon_art_regression` A8 asserts against a real plan so nothing authored can reorder the picture behind the plan's back.

**Architecture.** Two portable seams, not one. `plan_dungeon_view()` (Batch 9) decides *what*; `dungeon_art_blits()` (new, `native/core/src/dungeon_art.cpp`) decides *which image, where, with which flips, in what order*; `render_dungeon_view()` now only **paints**. No classification, light gate, sight rule, ring suppression, feature eligibility or depth logic was moved into the renderer, and none was duplicated. The container parser `dungeon_art_parse()` also lives in core, so the bounds checking between a corrupt card and a wild pointer is host-testable. The wireframe stand-in — `dungeon_ring()`, `dungeon_feature_box()`, `dungeon_variant_wall()`, `dungeon_side_quad()` and the placeholder EGA constants — was removed as dead code; `dungeon_pixel/rect/line` survive because the gem and zodiac views still use them.

**`primitives` changed meaning** in the dungeon path: it now counts **blits**, not per-pixel writes. The old counter started at 30,976 and incremented per pixel, which an authored 176×176 repaint would overflow in a `uint16_t`; a blit count is both in range and the number the metrics line actually wants.

**Pack.** 34 entries / 1,843,457 B / CRC `0x2b1449f4` → **39 entries / 2,039,545 B / CRC `0x2065ad91`**, SHA-256 `434cd664b4b92472386e04f08012aee52932296e4c432ec0f1c2e0d26f63b4ea`; **+196,088 B**. Generation is deterministic (byte-identical on re-run) and reproducible from `original/u5/ultima5/` — the same repository convention `runes.ch` already uses — via the accepted extractor parsers. The packer **refuses to build** a pack whose extracted images disagree with the authored dimension tables that `dungeon_art.h` carries, so the two ends cannot drift apart silently.

**Memory.** 195,768 B PSRAM (the five entry blobs, indexed in place; no second copy, no RGB565 pre-expansion) + ≈1.8 KB internal-RAM descriptor tables. Read once during `AlphaRuntime::initialize()` while the pack is open; **zero SD access for art during play**, and no per-frame allocation of any kind.

**Tests added:** `dungeon_art_regression` (A1–A13: dimension invariants derived from the geometry, variant→bank, cache identity incl. the runtime shortcut's agreement with the plan for all eight dungeons, side/front/feature/wanderer mapping, paint order against a real plan, unlit view, missing catalog, the packed container and its corruption catalogue, surface lookup bounds, the EGA table) and `typescript_dungeon_art_identity` (golden extraction CRCs, three independent readings, plus byte-for-byte pack reproduction). Both were confirmed RED against deliberate defects before being accepted: a wrong feature Y table and a weakened container bounds check each produced a specific, named failure.

**Host suite:** 74 tests, **72 pass, 2 fail** — `gameplay_parity` mismatch **2034** (R-21, intentionally open) and `quest_parity` `3221225477` (pre-existing environment failure). Both signatures are byte-identical to the Batch 9B baseline; the suite grew by exactly the two new tests. All eight dungeon tests GREEN, including Batch 9's `dungeon_view_regression` and Batch 9B's `dungeon_input_regression`, **unchanged**.

**Firmware:** ESP-IDF 6.1, `build-batch9c-dungeon-art/openu5_tdeck.bin`, **0xcfe30 (851,504) bytes**, **18.7% free** of the 1,048,576 B app partition, zero compiler warnings, **+2,288 B** over Batch 9B's 0xcf540 (849,216). The art itself adds nothing to flash — it lives on the SD card.

**SD card:** the pack identity changed, so **the card must be rewritten with the firmware**. See §16 step 0c.
**Physical test:** §16 steps 33–33k against real textures — the meaningful final R-05 checkpoint.
**Model:** Opus 5.

### Batch 9D — Non-overworld combat TRANSITION · risk: medium · **GREEN for the three proven defects; the entry-side symptom is NOT reproduced in host — see "What this batch did not prove"**
**IDs:** combat-transition integration (new); R-05 part 2 follow-up (the two Batch 9B prompt mirrors); developer-tool safety (`DebugTeleportStatus::ActiveCombat`).
**Files:** `native/targets/tdeck/host_tests/dungeon_combat_test.cpp` (new); `native/core/include/openu5/combat.h`, `native/core/src/combat.cpp`, `native/core/src/debug_map_picker.cpp`; `native/targets/tdeck/main/alpha_runtime.cpp`, `main/ui_mode_policy.h`; `native/core/CMakeLists.txt`.

**Trigger.** The physical T-Deck session after Batch 9C. Authored dungeon art, navigation, fountains, interactions and per-dungeon wall variants were all confirmed good, and overworld combat unchanged. Three new reports: **(1)** combat started from a dungeon corridor opens the scene but normal combat controls do nothing; **(2)** a room reached by a ladder does the same; **(3)** opening developer options and teleporting out of that state freezes the game.

**Scope ruling (R-05).** Presentation is not implicated. The renderer's own source arbitration makes `combat_source` outrank `dungeon_source`, so a dungeon-origin fight draws the combat scene and the dungeon art is not on this path at all. Recorded as a **combat-transition integration** issue; the Batch 9C authored-art verdict is unchanged and is **not** downgraded.

**Defect A — developer teleport is not a combat exit (the freeze).** `apply_debug_teleport()` had **no `c.combat` guard**. The Britannia / Underworld / SmallMap arms clear `DungeonContext::state.active`, set `c.dungeon = false` and rewrite `GameState::position`, while `CommandContext::combat` and `CombatState::initialized` stay true. The result is a mounted arena over a world that no longer matches it: the renderer keeps choosing `combat_source`, the router keeps sending keys to `handle_combat()`, and `CombatState`'s return bookkeeping (`encounter_location`/`floor`, `loot_x/y`, the dungeon room latch) still describes a place that no longer exists. There is no reachable way back — which from the player's seat is a freeze. The same-dungeon floor/cell arm was equally unguarded and rewrote `DungeonState::pos` underneath a live arena. Only the *cross-dungeon* arm was covered, and only incidentally, because it routes through `execute_dungeon_command()`'s own combat guard.
**Policy chosen: option 2, refuse — and it is the project's existing convention, not a new one.** `DebugTeleportStatus::ActiveCombat` already existed, `debug_labels.cpp` already renders it as "Blocked by active combat", and `debug_map_picker_test` already pinned the cross-dungeon case. Batch 9D enforces it **uniformly, before any mutation**, in the same shape as the existing `ImpassableDestination` refusal. It is not a ban: finish or flee the fight and the identical request applies.

**Defect B — a stranded arena wedges the runtime.** `Engine::current()` returns null exactly when nobody can ever act again (`combat_over()` is true by its own definition), but `CombatState::ended` is set only by `Engine::end()`, which runs only from inside an action that *had* an actor. An arena reaching that state by any other route leaves the owner with no actor to arm an AI beat for, nothing queued, every combat command returning `Ok` having done nothing, and `finish_encounter_combat()` declining forever because `ended` is false — a combat scene that owns the screen and the keyboard and answers neither. That is the reported symptom exactly, and the runtime had no escape hatch: `AlphaRuntime::schedule_combat()` simply `return`ed on a null actor.
New owner-side helper `openu5::close_stranded_combat()` ends such an arena through the ordinary `end()` path; `AlphaRuntime::schedule_combat()` calls it from the combat service tick, logs `COMBAT_STRANDED`, and runs the normal teardown, so recovery costs **no keypress**. It is deliberately **not** inside `combat_action()`: the reference's own command path is a silent no-op in that state and `combat_parity` pins the trace. That was measured, not assumed — putting it in `combat_action()` broke `combat_parity` at row 11241 (`op=2 field=4 got=1 expected=0`, trace 825 vs 800), and moving it to the owner restored the byte-identical trace.

**Defect C — Batch 9B's two dungeon prompt mirrors had no production caller.** `UiSession::refresh_dungeon_context()` was called only by `dungeon_input_test.cpp`. Nothing in `AlphaRuntime` ever called it, so on hardware `dungeon_klimb_prompt()` and `dungeon_fountain_prompt()` were permanently **false**: `(K)limb` silently preferred **up** on an up-and-down ladder — the exact defect Batch 9B set out to fix, and a step (§16 33j) the 9B checkpoint recorded as confirmed — and `(D)rink` never asked "Will you drink?". A pure host/device drift: the host suite proved a rule the device never ran. Fixed by moving the publication into the shared ESP-free seam `tdeck::publish_dungeon_prompt_context()` in `ui_mode_policy.h`, beside `resolve_synchronized_base_mode()` and for the same stated reason, and calling it from `AlphaRuntime::refresh_session_context()` immediately before every key is routed — the same narrow-mirror contract as the sail (R-19) and harpsichord (R-20) mirrors already there. `dungeon_combat_regression` drives that seam, so the two cannot drift again.

**Encounter initiation — no defect; the hardware observation is correct.** Traced to `game/src/core/dungeon/dungeon.ts` (original `0x1D4A` attack / `0x0B7E` ambush) and matched against `native/core/src/dungeon.cpp`. There are exactly two triggers and neither is sight or adjacency: `DungeonAction::Attack` emits `Corridor(cause=attack)` only when the wanderer occupies the cell the party **faces** (otherwise "What?"), and `tick()` emits `Corridor(cause=ambush)` when the wanderer's own random walk **steps onto the party**, after printing "Attacked!"/"Attacked from the <dir>!" and turning the party to face it. **Requiring `A` is reference-correct and is left alone.** `dungeon_combat_regression` D9D-7 now pins the ambush half so it cannot silently regress.

**Transition matrix** — measured through the real input seam, not derived. `overworld` = `start_encounter_combat()`; `dungeon` = corridor `(A)ttack`; `room` = ladder into an authored room arena. All three agree on every field, which is the finding:

| State | Overworld | Dungeon wanderer | Combat room | Correct | Verdict |
|---|---|---|---|---|---|
| `UiSession::mode()` | Combat | Combat | Combat | Combat | agree |
| `UiSession::base_mode()` | Combat | Combat | Combat | Combat | agree |
| `pre_combat_mode_` | Exploration | Dungeon | Dungeon | source mode | agree |
| `CommandContext::combat` | 1 | 1 | 1 | 1 | agree |
| `CommandContext::combat_context` | set | set | set | set | agree |
| `CommandContext::dungeon` | 0 | 1 | 1 | source | agree |
| `DungeonState::active` | 0 | 1 | 1 | source | agree |
| `CombatState::room` | 0 | 0 | 1 | per path | agree |
| `corridor_cause` / `room_entry_valid` | n/a | 1 / false | -1 / true | per path | agree |
| active modal at entry | none | none | none | none | agree |
| scheduled actor | player | player | player | player | agree |
| queued inputs | 0 | 0 | 0 | 0 | agree |
| next key routed to | combat | combat | combat | combat | agree |
| first key after exit | world | dungeon | dungeon | source | agree |

**What this batch did not prove.** The entry-side symptom itself — "the scene opens and the controls are dead" — **could not be reproduced** in host against a faithful, instrumented mirror of `AlphaRuntime::handle()`, `::command()`, `::service_combat()`, `::schedule_combat()` and `::finish_combat_if_needed()`, driven from `RawInputEvent` through the real `UiInputAdapter`, `UiSession`, `dispatch_world_command()`, `execute_dungeon_command()`, `dungeon_action()`, `dungeon_encounter()` and `start_fixed_combat()`, with a six-member party, across all three entry paths. Positively **ruled out**: a stale dungeon modal surviving entry (D9D-8), a renderer/input mode split (the matrix above), an uninitialised combat session, an incomplete return context, a mis-set `pre_combat_mode_`, and inputs stuck in the AI queue with no beat. What remains and **is** fixed is the one production state that produces exactly that symptom and had no recovery — Defect B — plus the two integration gaps above. Whether the device reached Defect B's state by the route this batch could not construct is what the §16 Phase 6C checkpoint decides; if the controls are still dead after this image, the `COMBAT_STRANDED` and `PRESENTATION_DISPATCH` lines in the serial log now say which half is wrong.

**Tests added:** `dungeon_combat_regression` (`native/targets/tdeck/host_tests/dungeon_combat_test.cpp`), **59 checks**, D9D-1…D9D-11: the overworld reference path; corridor `(A)ttack`; ladder-into-room; ambush; the combat command set after a dungeon entry (aim reticle open **and** cancelled, move, pass); the return to dungeon for both corridor and room including the **first key after the fight**; dungeon-modal ownership across entry; the two prompt mirrors; developer teleport during combat; and stranded-arena recovery. RED before the fixes: **8 of 59** — D9D-9a/b (mirrors never published), D9D-10b/c/d (teleport tore the dungeon session down and moved the party under a live arena), D9D-11c/d/e (the stranded arena never closed and the dungeon never came back). GREEN after: **59/59**.

**Host suite:** 75 tests, **73 pass, 2 fail** — `gameplay_parity` mismatch **2034** (R-21, intentionally open) and `quest_parity` `3221225477` (pre-existing environment failure). Both signatures byte-identical to the Batch 9C baseline (74 tests, 72/2); the suite grew by exactly the one new test. All eight dungeon tests, both combat parity suites, `combat_escape_regression`, `combat_loot_open_regression`, `debug_map_picker`, `debug_developer`, `dungeon_input_regression`, `dungeon_view_regression` and `dungeon_art_regression` GREEN and **unchanged**.

**Firmware:** ESP-IDF 6.1, `build-batch9d-combat/openu5_tdeck.bin`, **0xcffb0 (851,888) bytes**, **19% free** of the 1,048,576 B app partition, zero compiler warnings, **+384 B** over Batch 9C's 0xcfe30 (851,504).

**SD card: unchanged.** No resource touched; the Batch 9C pack (2,039,545 B, CRC `0x2065ad91`) stays exactly as it is. Flash firmware only.
**Physical test:** §16 Phase 6C.
**Model:** Opus 5.

### Batch 9E — Dungeon combat-ROOM escape · risk: low · **GREEN — ADJUDICATION + COVERAGE, no gameplay change**
**IDs:** combat-transition integration (Batch 9D lane, part 2); the sealed-room divergence of `re/notes/salas-selladas-mazmorra.md` section 5.
**Files:** `native/core/src/dungeon.cpp` (comment only); `native/targets/tdeck/host_tests/dungeon_combat_test.cpp`; `native/core/CMakeLists.txt`.

> **This batch changes NO gameplay behaviour.** The only production edit is a comment. What it ships is the adjudication and the missing host coverage.

**Trigger.** The physical T-Deck session after Batch 9D. Dungeon combat controls, the fight itself, death/resurrection and explicit `A` initiation were all confirmed **good** — Batch 9D's three defects are closed. One new report: leaving a ladder-entered dungeon combat room by walking the party off the combat-board edge returned them into a sealed 1×1 dungeon cell.

**Scope ruling (R-05).** Presentation is not implicated and the Batch 9C authored-art verdict is **unchanged and not downgraded**. The renderer drew exactly what the cell it was handed said.

#### The headline finding

**Sealed dungeon combat rooms carry their own escape, and it is on the COMBAT BOARD, not in the maze.** A room's `.CBT` board can contain an in-arena (K)limb tile — `0xC8` up, `0xC9` down, or the room-gated `0x86` grate (`SJOG cmd_klimb_combat 0x1df4` + `test [g_unk_58a1],0x80`). Using it sets `CombatState::escape_floor_delta`, and `dungeon_combat_return()` applies that delta when the party fled. **That is the only room exit that moves the party in the dungeon.**

Walking off the board edge is also a valid room exit — in fact it is the *only generic* one, because ESC/quick-withdraw is refused inside a room (`"Escape-Not here!"`, `CMDS.OVL 0x1822`, checked **before** the victory gate at `0x183a`, so even a won room cannot be closed with it). The reference states the consequence outright: *"la única salida de una sala es vaciar el bando party del tablero ANDANDO"* (`COMBAT.OVL 0x0ca6-0x0cc7`). But the edge never repositions anybody: `dng_enter_room` saves `g_party_x/y` on entry (`DUNGEON.OVL 0x0084/0x008c`) and restores them on **both** exit branches (`0x00fa-0x0103`).

So: edge-walk out of an uncleared, sealed room ⇒ back in the sealed cell. **That is reference-faithful and is not a defect.**

#### The actual hardware reproduction — Deceit, not Doom

| | |
|---|---|
| Dungeon | **Deceit (33)** |
| Cell | **floor 1, (5,3)** — authored room 0, `0xF0` |
| Entry | the `0x20` **LadderDown** at Deceit floor 0 (5,3) |
| Combat board | array position **16** = `DUNGEON.CBT #0` |
| Board contents | **2 chests, 1 mimic, 11 slimes** |
| Authored escape | **`0xC8` up-ladder at board (5,2)** → `escape_floor_delta = −1` → back onto the same LadderDown cell |

The board contents are the discriminator: across all 112 dungeon boards only **two** carry both a chest (sprite 1) and slimes (sprite `0x40 + 24·4`) — Deceit room 0 and Shame room 2 — and Shame room 2's dungeon cell is not sealed. It is also the same *"slimes and a chest"* room this document's own §16 Phase 6C step 33u records the Batch 9D session using.

A first pass of this batch mis-identified the target as **Doom 40:2:(5,5) room 6**. That was wrong and is corrected here: Doom room 6 is the one ordinary sealed room with **no** in-arena escape at all, which makes it the worst possible example of the reported case rather than the case itself.

#### The authored census

Measured over the shipped `fixtures/dungeon-maps.txt` and `fixtures/fixed-maps.txt`, and re-run every time `dungeon_combat_regression` runs (B9E-0):

| | count |
|---|---|
| sealed room cells in `DUNGEON.DAT` (four wall neighbours, no secret door) | **14** |
| …carrying an in-arena `0xC8`/`0xC9`/`0x86` escape on their own board | **12** |
| …without one | **2**, both in Doom |

The two without are **Doom 40:2:(5,5) room 6** — the single *ordinary* sealed room the authored data leaves with no in-arena way out — and **Doom 40:7:(5,7) room 15**, which is board 127, **Lord British's endgame room**: it is reached by a *pit*, has no entry ladder to hand back, and must stay sealed by design. Setting the endgame room aside, the ordinary sealed rooms are **12 of 13**.

> **Correction to this batch's own first pass.** An earlier note in this lane reported *13 of 14*. That figure came from a different predicate (does the board's klimb direction *match* the entry ladder), which scored the endgame room as a vacuous match. The measured count of sealed rooms that actually carry an in-arena escape tile is **12 of 14**. The census is exercised, not asserted from a table.

#### Victory and flee are intentionally not equivalent

| | flee | victory |
|---|---|---|
| restored cell | room-entry cell | room-entry cell |
| authored cell after | `0xFn` — unchanged | `0xFn → 0xAn` (victory latch → `dungeon_mark_room()`) |
| room-cleared bit | unset | set |
| re-entry | **fights again** | walk-in, no fight |
| dungeon-side ladder-pair de-seal (`caps()`, `t == 10`) | not offered | offered |
| the way out | the board's own in-arena (K)limb tile | the cleared-room ladder pair |

Each outcome has its own exit and they are different exits. The reference pins the same asymmetry (`parejaDeEscaleraBajoSala` gates on `RoomsBroke`; `game/tests/salas-selladas-mazmorra.test.ts`, *"la pareja NO abre una sala SIN despejar"*).

#### Production verdict

**Native already matched the reference for the observed case.** Exercised against the reference core in-tree (`DungeonState` + `Combat` + `Game.endCombat`'s room arm) and against native on the same authored data: same restored floor/x/y, same `0xF` cell, same `Klimb-what?`, same `Blocked!`.

An earlier revision of this batch added a `DungeonState::room_return_*` provenance record that let `caps()` de-seal a **fled** room. It was investigated, measured, and **removed in full** — it was a parity divergence, and it hid the mechanism the player is actually meant to use. `dungeon.h`, `dungeon_orchestration.cpp` and `gameplay_save.cpp` are byte-identical to Batch 9D; `dungeon.cpp` carries a comment and no code change. **No gameplay parity change is retained from the mistaken fix.**

**Tests added:** `dungeon_combat_regression` grows **59 → 124 checks**, B9E-0…B9E-7, driven on **authored** data (CTest passes both fixtures) through the Batch 9D device path — `RawInputEvent → UiInputAdapter → UiSession → dispatch_world_command → execute_dungeon_command → dungeon_encounter → start_fixed_combat`, with the in-arena escape driven by the real `k` key (`CommandKind::CombatKlimb → CombatAction::Klimb`), not by poking `CombatState`.

| test | proves |
|---|---|
| B9E-0 | the 14/12/2 census, and the identification of Deceit room 0 from the board's own chests+slimes |
| B9E-1 | Deceit room 0 edge-walk → sealed `0xF0` return, `K` refuses, movement blocked — **asserted as correct** |
| B9E-2 | Deceit room 0 in-arena `0xC8` → `delta = −1`, back on the LadderDown, and the party walks out through the authored secret door |
| B9E-3 | Destard room 0 room-gated `0x86` grate → `delta = +1`, the opposite direction and the other tile |
| B9E-4 | victory → `0xF0 → 0xA0`, cleared bit, ladder-pair de-seal works, no refight |
| B9E-5 | a **fled** room fights again on re-entry |
| B9E-6 | an unentered sealed room with a ladder pair straddling it is still not klimbable — the guard against re-broadening `caps()` |
| B9E-7 | corridor/wanderer returns keep their own rules (ambush steps a cell, attack does not, wanderer re-armed) |

One fixture subtlety worth recording: Deceit floor 0 (5,3), the LadderDown, is reachable **only** through the authored `0xD0` secret door at (5,4). The regression marks it revealed, as a player would with (S)earch, so the fixture does not start the party somewhere no player could stand.

**Host suite:** 75 tests, **73 pass, 2 fail** — `gameplay_parity` mismatch **2034** (R-21, intentionally open, **untouched**) and `quest_parity` `3221225477` (pre-existing environment failure). Byte-identical to the Batch 9D baseline re-measured at the start of this batch. `dungeon_input_regression` 65/65, `dungeon_view_regression`, `dungeon_art_regression`, both combat parity suites, `combat_escape_regression`, `combat_loot_open_regression`, `debug_map_picker` 62, `debug_developer` 69, `gameplay_integration` 42 — all GREEN and unchanged. Batch 9D's D9D-1…D9D-11 (59 checks) run unchanged inside `dungeon_combat_regression`, including the developer-teleport refusal and stranded-arena recovery.

**Firmware:** ESP-IDF 6.1, `build-batch9e-final/openu5_tdeck.bin`, **0xcffb0 (851,888) bytes**, **19% free** of the 1,048,576 B app partition, **zero compiler warnings**, and **0 B delta** against Batch 9D's 0xcffb0 (851,888) — as expected, since no production code changed.

**SD card: unchanged.** No resource touched; the Batch 9C pack (2,039,545 B, CRC `0x2065ad91`) stays as it is. Flash firmware only.
**Physical test:** §16 Phase 6D.
**Model:** Opus 5.

### Batch 10 — View Gem presentation · risk: low · **DONE (host+firmware); hardware check pending**
**IDs:** R-17, Y-14
**Files:** `native/core/include/openu5/gem_view.h`, `native/core/src/gem_view.cpp` (new semantic layer), `native/core/src/gem_category.inc` (new, generated), `native/core/tools/generate-gem-category-table.ts` (new), `native/core/tests/gem_view_test.cpp` (new), `native/core/sources.cmake`, `native/core/CMakeLists.txt`; `native/targets/tdeck/main/native_renderer.{h,cpp}` (paint-only now), `tdeck_board.{h,cpp}` (`full_square_viewport`), `alpha_runtime.cpp` (call sites + `full_square_viewport`/palette plumbing).
**Work:** replace the bit-test colouring with a real terrain-category map derived from the same classification the reference's `buildGemView` uses; stop clipping the gem view. **Done after Batch 9**, sharing its "full-square presentation sources must not be HUD-clipped" fix.
**Physical test:** `V` with gems on the overworld and in a dungeon; confirm the map is legible and that closing it charges exactly one turn. See the Batch 10 hardware checklist below.
**Model:** Sonnet 5.

**Resolution:** see R-17's "Resolution (Batch 10)" write-up above for the full root-cause/fix/evidence account (also closes Y-14). Host suite RED→GREEN: `gem_view_regression` (new) and `gem_category_table_drift` (new); full suite **77 total, 75 pass, 2 fail** (pre-existing `gameplay_parity` R-21 mismatch 2034, and a `frontend` struct-padding `memcmp` sensitivity newly *surfaced* — not introduced — by this batch's from-scratch rebuild and reproduced identically on the untouched pre-Batch-10 tree; see the isolation evidence in R-17's write-up). Firmware `idf.py build` (`build-batch10/openu5_tdeck.bin`): **PASS**, total image **852,916 bytes** (+1,028 B over Batch 9E's 851,888 B — the new gem-view code), app partition **19% free**, DIRAM 33.92% used. Two unrelated pre-existing build-hygiene fixes (a `-Wsign-conversion` cast in `dungeon_art.cpp` and a `-Wstring-concatenation` parenthesization in `debug_labels.cpp`) were required to get any host build compiling at all in this environment's current toolchain — both are no-op for behavior, isolated and verified by rebuilding on an otherwise-untouched tree.

**Hardware verification checklist** (not yet performed — see §16 for the project's physical-test log format):
1. Activate/use the View Gem (`V`) in a known mixed-terrain overworld area (water, grass/forest, hills, road all visible if possible; a debug/developer teleport may establish the position).
2. Confirm the complete 32×32 square is visible with no clipped/cropped edge and no sky/wind bar text drawn across it.
3. Confirm the party marker is visible and lands on the correct cell (not necessarily centred — the overworld window is anchored to a 16-cell block, so the marker can sit anywhere in columns/rows 8-23).
4. Move north/south/east/west, re-open the gem, and confirm the displayed terrain shifts in the corresponding direction (a west step should reveal new terrain to the west edge, etc.).
5. Inspect several recognizable terrain features (coastline, a road, a hill range) against the actual overworld and confirm they read as visually distinct categories.
6. Confirm returning from the View Gem (any key) leaves normal controls/rendering intact and charges exactly one turn (no charge from the crystal-ball variant).
7. Enter a dungeon, use `V`, and confirm the connected-floor flood-fill is visible as a full square, walls/doors/rooms are distinguishable, and the party marker sits at the display's centre.
8. Confirm Batch 9's dungeon 3D corridor presentation still renders normally (unaffected by this batch).

---

### Batch 11 — AlphaRuntime integration testability · risk: low · **DONE (host+firmware)**
**IDs:** Y-05, Y-06; closes the integration-testability half of §15's "structural change" note (not the coverage rows §15 lists as depending on it — see Scope below)
**Files (new):** `native/targets/tdeck/host_tests/esp_shims/{esp_err,esp_check,esp_log,esp_timer,esp_heap_caps,esp_attr,esp_system}.h`, `esp_shims/freertos/{FreeRTOS,task}.h`; `host_tests/host_stubs/{tdeck_board_host_stub,alpha_save_host_stub,tdeck_input_host_stub,device_smoke_tests_host_stub}.cpp`; `host_tests/alpha_runtime_host_fixture.cpp`; `host_tests/alpha_runtime_integration_test.cpp`.
**Files (changed, both additive):** `native/targets/tdeck/main/alpha_runtime.h` (one new public method + a 1-field fixture struct + four trivial reference accessors, all under a "Batch 11 host-test seam" banner comment; zero lines changed or removed); `native/core/CMakeLists.txt` (one new `add_executable`/`add_test` block).
**`alpha_runtime.cpp` itself: unmodified. Not one line changed.**

**Design taken vs. §15's proposal.** §15 proposed extracting `dispatch`/`modal`/`open_selection`/`cast_selected_spell`/`synchronize_*`/`consume_event` (~500 lines) into a new ESP-free `GameplayController` class, with `AlphaRuntime` becoming glue over it. That was evaluated and **not** done: `command()` alone is ~600 lines with ESP_LOG/`esp_timer_get_time()` calls interleaved on nearly every line for hardware-debug tracing, and correctly separating those from the surrounding logic without behavior drift, across a file this dense with reference-parity timing comments (R-10, R-12, Y-04, #324/R-32), is a large, real rewrite with real regression risk — exactly what the batch's own instructions ("no broad architectural rewrite", "preserve semantics exactly") rule out attempting in one pass. Tracing confirmed the ESP-IDF/Board/storage dependency actually sits in only four places: `esp_log.h`/`esp_timer.h`/`esp_heap_caps.h`/`esp_check.h` (used for logging, perf metrics and `initialize()`'s PSRAM allocation — never for a gameplay decision), `Board` (used only by `render()`/`initialize()`, never by `handle()`/`dispatch()`/`command()`), and `AlphaSaveService`/`AlphaSettingsService`/`DeviceSmokeTests` (real SD/VFS I/O, reached from a few `handle()` branches — Save, Load, movement-mode-toggle settings-persist — that are not part of command routing). So instead of moving code, this batch supplies **harmless host substitutes for exactly those four dependency surfaces** (the shape the task spec calls "a hostable runtime adapter") and links `alpha_runtime.cpp` itself, byte-for-byte unchanged, into the new host target. Every ESP_LOG/`esp_timer_get_time()` call still runs — against a host stdout logger and a host monotonic clock instead of the real ones — so the same diagnostic trace lines this codebase already leans on for adjudication print during test runs.
**Why this satisfies the spec's requirements:** production `AlphaRuntime` calls through nothing new (the shims are transparent header substitutions, not a code path `alpha_runtime.cpp` branches on); host tests call the literal production `handle()`/`dispatch()`/`command()`, not a copy; there is no duplicated command switch and no gameplay behavior mirrored into the test file (contrast with `dungeon_combat_test.cpp`'s and `dungeon_input_test.cpp`'s own file-header admissions that they hand-copy `AlphaRuntime`'s per-input/per-command tail — Batch 11 is the first suite that does not); `tdeck_board.cpp`/`alpha_save.cpp`/`tdeck_input.cpp`/`device_smoke_tests.cpp` (the real device implementations) are untouched and still the only ones `main/CMakeLists.txt` builds into firmware — the stubs live only in `host_tests/host_stubs` and only this one CMake target links them.
**The one small, real addition to `alpha_runtime.h`:** `AlphaRuntime::attach_host_test_fixture()`, which wires the same `CommandContext`/`CombatContext`/`DungeonContext`/`UiSession` graph `initialize()` wires for production, sourcing world/NPC-scratch/transport-service data from a caller-supplied fixture and ordinary `new` instead of a loaded `AlphaResourcePack` and PSRAM. This is construction/dependency-injection, not gameplay logic — it contains no command dispatch, no `CommandKind` switch, and no behavior a test could get right or wrong independent of the production code it wires together.

**RED → GREEN.** Five representative command classes, all driven through `AlphaRuntime::handle()` with real `RawInputEvent`s (never a synthesized `UiIntent`/`Command`), 22 assertions total, all GREEN against the real routing:
- **A — immediate world command (Move):** trackball input reaches `handle()`, moves the player exactly one tile, charges exactly one turn.
- **B — direction-prompt command (Open):** `O` arms `UiMode::TargetSelection` with zero turns charged and zero mutation; a follow-up direction is accepted, closes the prompt back to `Exploration`, and (with nothing at the target tile) mutates nothing — production's own "nothing to open" outcome, not a test-invented one.
- **C — inventory/picker command (Use item):** `U` enters `UiMode::InventorySelection`; confirming a potion row arms a `PartySelection` "Use on whom?" sub-picker (discovered empirically — not assumed — while running this test: the picker enumerates potions/scrolls/`usable_item_picker_rows()`, never raw `equipment_quantities`); confirming the party target routes back through `UseItem`, which actually decremented the potion count.
- **D — modal/view command (View Gem):** `V` does not itself move the player and consumes exactly one gem on cast (also discovered empirically: the gem is spent synchronously on cast, not on close — the test's first RED run corrected an assumption it started with); any key, *including a movement key*, closes the view without also delivering that key as a move, and charges the deferred turn exactly once, on close.
- **E — illegal/no-op command (Klimb with nothing to climb):** the key is accepted by `handle()` but `dispatch_world_command` rejects it — zero turns charged, zero mutation.
- **F — turn accounting:** every class above asserts `GameState::turns_since_start` (`openu5/state.h`) directly, before and after, instead of trusting a side channel — this is the authoritative turn oracle and the same one a future runtime-routing bug (gameplay correct, `AlphaRuntime` over/under-charging) would have to falsify to hide.

Getting from a linked-but-crashing build to this GREEN state surfaced two real, narrow wiring bugs in the host fixture itself (not in `AlphaRuntime`): `commands.cpp`'s context gate unconditionally rejects every command when `context_.actors` is non-null but `context_.npc_scratch` is null (the fixture had left it null; production always binds both together) — Move was accordingly RED with `InvalidContext` until fixed. Both are documented inline in `alpha_runtime_host_fixture.cpp` at the exact line.

**Host suite:** baseline (this batch's start, commit `9ab63f11`) and after are both **78 total, 76 pass, 2 fail** — `gameplay_parity` (pre-existing R-21 mismatch 2034, untouched per instruction) and `quest_parity` (a native-driver crash reproduced byte-for-byte by building the *unmodified* HEAD tree in an isolated directory with this environment's current toolchain — confirmed pre-existing and unrelated to this batch, not investigated further per scope). The new `alpha_runtime_integration_regression` target is included in that 78/76 and is the only change to the pass count's composition (77→78 total, 75→76 pass at Batch 10's own numbers, adjusted here for the two pre-existing failures both being present at this batch's start already).
**Firmware:** `idf.py build` (`build-batch11/openu5_tdeck.bin`): **PASS**. Total image **852,528 bytes** (0xd0430), app partition **19% free** (0x2fbd0 of 0x100000) — 388 bytes *smaller* than Batch 10's 852,916 B, expected build-metadata/timestamp noise between separate `idf.py build` invocations of a source tree where `alpha_runtime.cpp` is byte-identical to Batch 10's (this batch changed zero lines in it). Bootloader 31% free, unchanged partition table.

**Scope — what this does and does not prove:**
1. **Does prove:** production `AlphaRuntime::handle()`/`dispatch()`/`command()` — the actual dispatcher device firmware runs — now executes on host, for the five classes above, with the same turn-accounting oracle a future regression would have to defeat.
2. **Does not prove:** any specific §2/§3 Y/R row is fixed. None of A–E touch Shop/Dialogue mode-survival (R-01, already covered by `ui_mode_regression`), the dungeon (R-05), NPC-initiated events (R-09/R-10), or the scroll-use mismatch (R-21) — those need their own scenarios in this same harness, not a re-read of this batch's five.
3. **Runtime bug classes now catchable that were not before:** (a) a routing bug where `dispatch_world_command` returns success/turns correctly but `AlphaRuntime`'s own tail charges/suppresses a turn incorrectly (the exact bug class §7's turn-accounting note anticipates, and the reason F asserts `turns_since_start` directly rather than trusting `ActionResult`); (b) a direction-prompt arm/close bug where `UiSession` state and `AlphaRuntime`'s own dirty/mode bookkeeping disagree; (c) a picker round-trip bug where a selection is accepted but never reaches the intended `CommandKind` (would have shown as C-4's potion count not decrementing); (d) a modal-input-swallow regression where a view/picker leaks an input through to gameplay instead of consuming it (D-3/D-5's explicit movement-key-as-close-key probe exists specifically to catch this).
4. **Still cannot be proven without hardware:** real pixel output (all rendering paths are compiled but never invoked — `render()` was not part of any test's call graph), physical Board/SPI/SD/GPIO behavior (harmlessly stubbed here, never executed for real), and anything timing-sensitive against a real ESP32-S3 clock.
5. **Materially helps R-21 (mismatch 2034)?** Likely somewhat, not decisively: R-21 is a scroll-use message/event/SFX divergence, and this seam is the first place a scroll-use *routed through real `AlphaRuntime` input* (as opposed to `dispatch_world_command` called directly, which is how the existing `gameplay_parity` harness reaches it) could be host-tested end to end. But R-21's own investigation (Batch 12) has not started, so whether the defect is even reachable from `AlphaRuntime`'s side of the boundary — rather than entirely inside `world_magic.cpp`/`commands.cpp`, which the existing parity harness already exercises directly — is unknown. This batch does not investigate that; Batch 12 still owns it.

---

### Batch 12 (combat-field magic lane) — In *Grav in the arena · risk: low · **GREEN — NOT A DEFECT, ADJUDICATED**

> ⚠ **Numbering collision, declared.** The plan entry immediately below also carries the number "Batch 12" (R-21, scroll-use divergence). That entry is **untouched and still NOT STARTED** — R-21 was explicitly deferred when this lane was opened. Two different lanes share the number; neither supersedes the other.

**IDs:** new hardware report (In Flam Grav / In Nox Grav / In Zu Grav "cast, flash, leave no persistent field on the battlefield"); ticket #91.
**Files:** **no production file changed.** `native/core/tests/batch12_combat_field_test.cpp` (new), `native/core/CMakeLists.txt` (registers it), this document.

> **This batch changes NO gameplay behaviour and NO production code.** What it ships is the adjudication and the missing host coverage — the Batch 9E shape.

#### The headline finding

**In combat, the four In\*Grav spells are not supposed to create a field.** `CAST.OVL cast_field_wall` 0x004c splits at `0054 cmp byte [g_location],0x80` / `jb`:

- **Dungeon (below 0x80).** Writes **one** field tile into the cell ahead, from the field-tile table `DS:0x4596` = `{0x82,0x81,0x80,0x83}`, guarded by `00b8 test ...,0xf7` (empty floor only) and preserving bit 3 (`00c6 al=[bp-8]&8` / `00ce or al,[bx+0x4596]`). **This is the only site in the whole disassembly that references `DS:0x4596`** — the census is "who reads the table", not "we saw no writes".
- **Combat (0x80 and above).** Seeds nothing. `00ef mov al,[bx+0x4592]` loads a spell-weapon id (`0x35/0x33/0x34/0x36` for args 0..3) and `0100 call 0xffffc14a` reaches `COMSUBS.OVL:0x0c52 attack_dispatch_by_reach` — the same destination, with the same two arguments, that `cmb_set_weapon_then_attack` uses for Grav Por / Vas Flam / Xen Corp. The chain was read end to end (`attack_dispatch_by_reach` → `player_ranged_attack` / `melee_strike_resolve` → `hit_roll` / `apply_damage_death_loot`) and none of the four routines writes a tile.

Damage comes from `attackValues[0x33..0x36]` = **18 · 0 · 21 · 0**. So In Zu Grav and In Sanct Grav spend the mixed spell, the mana and the turn and do **nothing at all**, and In Nox Grav / In Flam Grav are a dart, not a wall. Registered as an original defect and **deliberately cloned**: `docs/bugs-del-original.md` §2.9, `re/notes/field-spell-port.md`, and the live-binary witness `re/notes/field-grav-gate-testigo-20260808.md` (the `DS:0x1C90` masks for indices 14/15/16/20 read `0x03` = dungeon+combat only, taken from running RAM with In Lor as positive control).

⇒ **The reported symptom is the game working.** Implementing combat field creation would fabricate content EA never shipped, move the RNG stream, and break both `magic_parity` (25,088 cases) and the reference port's own guard (`game/tests/field-wall-dungeon.test.ts`).

#### Per-axis evidence (recorded separately, as required)

| Axis | Verdict | Evidence |
|---|---|---|
| **Spell dispatch correctness** | **G** | `magic_tables.inc` carries `{Field,53,0}/{Field,51,1}/{Field,52,2}/{Field,54,3}` for ids 14/15/16/20 — the `DS:0x4592` weapon ids. `combat_cast` (`combat.cpp:1058`) rewrites `Field -> Attack`, mirroring `combatCastEffect` (`game/src/core/combat/combat.ts:561`); `spell_flight` (`combat_magic.inc:78`) carries `{16,30,99,18,0,21,0}` for ids 48..54 — `attackValues` transcribed, zeros included. Reticle prompt: `cast_target_prompt` sees `selectedCombatMapPosition` -> `CombatReticle`. |
| **Arena-state creation** | **G — correctly absent in combat, present in the dungeon** | Combat: a cast into an arena that **already owns field storage and a live authored field** leaves `field_count` unchanged and the aimed cell empty. Dungeon: `dungeon_orchestration.cpp:169` writes exactly one cell to `0x82/0x81/0x80/0x83`, refuses a non-empty cell with `Failed!`, and preserves bit 3. |
| **Rendering** | **G** | `compose_combat_presentation` (`presentation.cpp:249`) paints every field slot's raw tile over the terrain. Proven through that seam, not through a renderer-only fixture. |
| **Gameplay effects** | **G** | `Engine::advance` implements `COMBAT:0x1b1e` whole: terrain magnitudes (`0x8F`/`0xBC` -> 100, `0x04` -> 50) then the object sweep (`0xE8` -> 50 poison, `0xEA` -> 100 damage `rand(0,10)`, `0xE9` -> 150 sleep). `0xEB` is absent from that table **and that is correct** — `field_blocks` implements `COMBAT:0x0000 @00a4`, so nobody can ever stand on it. An Grav in combat (`CAST2:0x07bc`) removes exactly one slot -> `Success!`, none -> `Failed!`. |
| **Lifetime** | **G — no duration exists** | There is no per-field counter anywhere in the reference (MAGIC.md: *"No new field duration exists"*). Fields persist for the whole encounter and leave only through An Grav or the Sceptre. Pinned over twelve alternating party/enemy turns. |
| **Hardware validation status** | **PENDING** | Software GREEN; the device checklist is in §16 below. **Not marked GREEN on the strength of the cast flash** — the flash is `GameEventKind::MagicCeremony`, which fires for every ceremonial spell and says nothing about the effect. |

#### Where arena fields actually come from

They are **authored `.CBT` units of family `0xE8`** — 26 of them in the whole corpus: **cm18** (Deceit r2, 12 x `0xEB`), **cm20** (Deceit r4, 8 x `0xE8`), **cm121** (Doom r9, 6 x `0xE8`). All three are dungeon rooms, so they reach the arena through `dungeon_encounter` -> `start_fixed_combat` -> `initialize_combat`'s fixed arm, which converts each `(sprite & 0xfc) == 0xe8` unit into a `CombatField` slot with its **raw** sprite as the tile. Census source: `re/notes/cargador-ticket-amplio-0xb4-0xe8-0x70.md` §6.

#### Non-blocking finding (not fixed — no behaviour depends on it)

`AlphaRuntime::init` allocates `combat_fields_` (32 x `CombatField`, PSRAM) and assigns `combat_.fields = combat_fields_` once at boot (`alpha_runtime.cpp:203/207`). `initialize_combat` reconstructs `CombatState` in place and restores only the **actor** and **pile** overflow pointers, so that assignment is dropped by the first arena and `CombatState::fields` is thereafter either `nullptr` (roaming combat) or `DungeonEncounters::fields` (the fixed path). The allocation is therefore **dead**. It is harmless today — nothing outside the fixed path ever inserts a field, and `field_count` is reset to 0 by the same reconstruction — and preserving the pointer would hand roaming arenas storage that nothing can fill. Recorded here rather than changed, because this batch touches no production code.

**Tests added:** `batch12_combat_field` (new CTest), nine blocks against the real production functions — A location gate (`cast_spell` refuses overworld/town with `Not here!`, spending neither spell nor mana; dungeon and combat both yield the `Field` descriptor with its `DS:0x4592` id), C1–C3 combat cast (resources spent, turn finished, **no field created**, damage inside 21/18/0), C7 invalid placement (empty-cell cast, and an opaque tile blocking the spell line), C1'/C6 authored-field seeding and twelve-turn persistence, C4 render through `compose_combat_presentation`, C5 the three magnitudes plus `0xEB` blocking-and-not-damaging, An Grav dispel (one slot, `Success!`/`Failed!`), the dungeon branch (one cell, correct tile per spell, bit 3 preserved, `0xf7` guard), and C8 summon regression (In Bet Xen swarm, at most four of index 31; Kal Xen Corp one daemon, index 38).

**RED proof.** A passing test proves nothing on its own, so the guard was verified by mutating production code both ways and reverting: (1) making `combat_cast` seed a field at the aim cell — the change this batch was asked to make — turns it **RED with 10 failures**; (2) replacing `spell_flight`'s transcribed zeros with real damage (`{...,18,25,21,25}`) turns it **RED with 1 failure**. Both mutations reverted; `git status` confirms `native/core/src/` unmodified.

**Host suite:** **79 total, 77 pass, 2 fail** — the identical two pre-existing failures the baseline run on this tree produced before any Batch 12 edit: `gameplay_parity` (R-21, *"Gameplay mismatch 2034"*, deferred by instruction) and `quest_parity` (`STATUS_ACCESS_VIOLATION`, exit `3221225477`, the MinGW/w64devkit environment finding already recorded in Batch 8's write-up). All magic and combat tests green, `magic_parity` and `typescript_magic_fixture_drift` included.

**Firmware:** `idf.py -B build-batch12 build` **PASS** — `openu5_tdeck.bin` **0xd0430 bytes**, byte-for-byte the same size as Batch 11, as expected from a batch that changes no production code. App partition 19% free.

---

### Batch 12B — Hardware discrepancy investigation (dungeon field visibility + combat spawn placement) · risk: medium · **GREEN — TWO SEPARATE ROOT CAUSES, BOTH FIXED; one authored-data finding GUARDED**

> ⚠ **This does NOT revise the Batch 12 verdict.** In \*Grav still seeds no arena field, and that is still the original's behaviour. The two findings below are about the **dungeon** field and about **which arena a dungeon room loads** — neither touches combat field magic. They are recorded on separate evidence axes, as required, and are not merged into the Batch 12 conclusion.

**IDs:** hardware observation A (Destard: successful `Cast`, no visible field in the 3D view); hardware observation B (enemy rendered in the black void beyond the wall).
**Files (production):** `native/core/include/openu5/dungeon_art.h`, `native/core/src/dungeon_art.cpp` (new `dungeon_field_spark()` plus its five reference tables), `native/targets/tdeck/main/native_renderer.cpp` (paints them), `native/targets/tdeck/main/alpha_resources.cpp` (one field retagged at load).
**Files (tests/build):** `native/targets/tdeck/host_tests/batch12b_hardware_test.cpp` (new), `native/core/tests/dungeon_art_test.cpp` (new block A14), `native/core/CMakeLists.txt`, this document.

#### Evidence axis 1 — dungeon field state MUTATION · **G, was never broken**

`execute_dungeon_command()`'s `MagicEffect::Field` arm writes **exactly one** cell — the one ahead by facing — to `0x82` / `0x81` / `0x80` / `0x83` for In Flam / In Nox / In Zu / In Sanct Grav, refuses a non-empty cell with `Failed!` (the `0xf7` guard) and preserves bit 3. The device's `Cast` log line was therefore telling the truth. Proved for all four spells through the real production entry point in `batch12b_hardware_regression` A1: no `Failed!`, the mixed spell drops 20 to 19, mana drops, the destination cell holds the reference tile, high nibble 8, and **exactly one** cell in the 512-cell floor set is non-zero.

#### Evidence axis 2 — dungeon field state PERSISTENCE · **G, was never broken**

The write is not lost by any refresh path. A1 drives, for each of the four spells: four `Right` turns back to North; two `TurnAround`s; five `Pass` ticks; and a full step-out / step-back cycle — and re-reads the cell after each. It survives all of them. `dungeon_load()` is the only thing that ever restores `DungeonState::cells`, and it runs on `EnterDungeon` alone. Liveness is proved the hard way as well: stepping into an In Flam Grav field reports `Fire!!` and takes hit points, and stepping into an In Zu Grav field reports `Sleep spell!` and **consumes the field**, which is the reference's own asymmetry (`dungeon.cpp`'s `field == 0` write-back).

#### Evidence axis 3 — dungeon field 3D RENDERING · **R to G, CONFIRMED NATIVE DEFECT, FIXED**

**This is observation A's root cause, and it is not cosmetic in the dismissive sense — it hid authored content.**

`plan_dungeon_view()` already emitted the `Feature` op for the field cell (cell kind 8 is inside its `LadderUp..MagicField` range), so the core knew. `dungeon_art_blits()` then returned **0** for it, with a correct comment: a magic field has no ITEMS.16 image. What the port never carried is what the original does *instead* — `DUNGEON.OVL magic_field_sparkle_drawer` @0x127e, reached from `feature_overlay_drawer_by_nibble` @0x19f6 when the tile's high nibble is 8. Its body (0x127e-0x1346, `ret 4`) draws `count[depth]` inclusive horizontal strokes inside a box, **two `rand_range` rolls per stroke, x before y**:

| Table | DS | Values (depth 0..3) |
|---|---|---|
| strokes | `0x2e52` | 300 · 100 · 50 · 15 |
| box low edge | `0x2e42` | 16 · 56 · 80 · 92 |
| box high edge | `0x2e4a` | 167 · 135 · 111 · 99 |
| stroke length | `0x2e5a` | 7 · 7 · 5 · 2 (inclusive, so width = len+1) |
| colour by `tile & 7` | `0x1292` + `add ax,8` @`0x12b7` | 10 · 9 · 10 · 9 (globals DS `0x13b6`/`0x13b4`/`0x13ae`/`0x13b2` = 2·1·2·1) — EGA 10 bright green, EGA 9 bright blue, so sleep+fire are green and poison+energy blue. Only two colours exist in 1988; a green fire field is correct. |

The depth/type split (tables index by **depth**, the colour switch conmutes on the **field type**) is settled by the caller's push order at `0x19f6` and independently by the tables' own 8-byte / four-word stride; both derivations are in `re/notes/dungeon-decor-mazmorra.md` sections 2 and 5 and section 6 ticket 4, and the accepted reference implements the same function as `fieldSparkRects` in `game/src/skin/fiel/dungeon-decor.ts`. The randomness is **render** randomness — the binary re-rolls the whole field on every corridor redraw, one per key poll, so its consumption is unbounded and wall-clock dependent. It is the sanctioned divergence class of `dungeon.md` 12.11 (the same one the wanderer's animation frame already uses) and it never touches `GameState::rng`.

**Fix:** `openu5::dungeon_field_spark(depth, sub, phase, index)` in `native/core` — pure, per-stroke, reproducible from its own coordinates so a host test can assert the picture while `phase` still re-rolls it per redraw on device — plus a paint branch in `render_dungeon_view` that maps the EGA index through `kDungeonEgaRgb565`. The renderer still decides nothing: the plan says *where*, `dungeon_field_spark` says *what*, and the painter only moves pixels, exactly as it does for the authored banks. It counts as **one** primitive, because it is one feature drawn by one subsystem.

**Blast radius beyond the reported symptom:** DUNGEON.DAT authors **55 magic-field cells** that have been invisible since Batch 9C — **30 in Wrong** and **25 in Covetous**, covering all four tile values. `batch12b_hardware_regression` A2b renders one of Wrong's own authored fields, from the shipped pack's cells, and compares the viewport with and against it.

#### Evidence axis 4 — combat actor AUTHORED placement · **G — REFERENCE-AUTHORED, NOT A DEFECT, GUARDED**

**This is observation B's appearance, and it is the original's own data.** Counting the shipped pack directly: **50 of the 128 `.CBT` boards place at least one non-empty unit slot on a `BlackSquare` (tile 255) cell — 403 slots in total.** They are not confined to the outer ring either; boards put units on black cells well inside the wall line. The procedural **corridor** arena does the same by construction: the monster-slot tables `DS 0x2476/0x2486/0x2496/0x24A6` include x or y of `0` and `10`, i.e. the ring **outside** the wall rows at 1 and 9, and `build_corridor_map` blacks out the outer row of any blind side (`0x097E`). `native/core/src/scripted_encounter.cpp` reproduces those tables, the carve widths (`open` 2..8, `doorway` 3..7) and the door plugs exactly as `buildCorridorCombatMap` does. Tile 255 is impassable to players and monsters alike (`kCombatTileFlags[255] == 4`), so nothing *walks* there — an actor on black was **placed** there, by the authored slot.

Asserted as a **guard** in `batch12b_hardware_regression` B1 (the census must stay non-zero), so a later "tidy-up" of the authored boards would be caught. **Production placement was not altered.**

#### Evidence axis 5 — combat actor RENDERER placement · **G for the transform; R to G for WHICH BOARD, CONFIRMED NATIVE DEFECT, FIXED**

The **transform is the identity** and was never the problem: `compose_combat_presentation` writes an actor at exactly `y*11 + x` (B1 pins `(0,0)` to index 0 and `(10,7)` to index 87), and the 11x11 arena is the 11x11 viewport, so there is no origin, offset or scale to get wrong.

**But the board being drawn was the wrong one.** `dungeon_encounter()` finds its arena by `arenas[i].map->index == room_map`, where `room_map` is the **global** catalog index `dungeon_room_map()` produces — `16 + order*16 + room`, i.e. 16..127 across the 16 britannia boards followed by 7x16 dungeon rooms (`DUNGEON.OVL 0x003a` / `DNGLOOK 0x0844`; `roomCombatMapIndex` in `game/src/core/dungeon/dungeon.ts`). The alpha resource pack, however, stores `combatmaps.json`'s **per-territory** index: britannia counts 0..15 and the dungeon rooms restart at 0..111. Measured against the shipped `openu5-alpha1-resources.bin`:

| Dungeon | room 0 global index | board it should load | board it actually loaded |
|---|---|---|---|
| Deceit (33) | 16 | 16 (Deceit r0) | **48** (Wrong r0) |
| Destard (35) | 32 | 32 (Destard r0) | **48** (Wrong r0) |
| Wrong (36) | 48 | 48 | **64** (Covetous r0) |
| Covetous (37) | 64 | 64 | **80** (Shame r0) |
| Shame (38) | 80 | 80 | **96** (Hythloth r0) |
| Hythloth (39) | 96 | 96 | **112** (Doom r0) |
| Doom (40) | 112 | 112 | **none — `MissingMap`** |

Every dungeon room therefore fought in the **next dungeon's** room, and Doom's rooms could not start at all. World combat was never affected: it indexes `CombatResources::maps` **positionally** (`encounter_preflight`, `combat.cpp`) and never reads this field, which is why the britannia half stayed correct and the defect hid. `dungeon_combat_regression` (Batch 9D/9E) did not catch it because it builds its arena by hand and assigns `map.index = dungeon_room_map(...)` itself, satisfying the contract the pack breaks.

**Fix:** one field, retagged where the pack is decoded (`alpha_resources.cpp`) — the pack's combat-map array **is** that global catalog, written in order by `native/tools/u5pack/alpha1.ts`, so the global index is the record's own position. Done at load rather than in the packer deliberately: **an already-flashed SD card needs no repack**, which is what makes the Phase 6F hardware check runnable on the existing card.

This finding compounds observation B rather than replacing axis 4: an enemy on a black cell is authored, but the tester was looking at a board from the wrong dungeon while seeing it.

#### What this batch did NOT do

- It did not revisit combat field magic. `batch12_combat_field` is unchanged and green.
- It did not alter any authored combat board, and it moved no actor.
- It did not touch R-21, as instructed.
- It did not change the packer, so `kExpectedAssetPack*` and the card's contents are untouched.

**Tests added/changed:** `batch12b_hardware_regression` (new CTest, `native/targets/tdeck/host_tests/batch12b_hardware_test.cpp`) — the first host suite that reads the **real shipped resource pack**, links the **production device renderer** and the **production pack loader**, and drives the production dungeon command path; blocks A1 (state + persistence + liveness), A2 (plan/art/paint, with a lit positive control and an unlit negative control), A2b (an **authored** Wrong field), B1 (authored-placement census + identity transform) and B2 (room-to-board identity for all seven room-bearing dungeons plus Doom). `dungeon_art_regression` gains **A14**: the five reference tables, `len+1` inclusive width, colour by `tile & 7` (and that bit 3 cannot reach the colour switch), every stroke inside its box, reproducibility from `(depth, sub, phase, index)`, re-rolling on a new phase, spread across the box, index/depth bounds, and that a field **still** emits no ITEMS.16 blit so the two subsystems cannot both draw it.

**RED to GREEN evidence.** The suite was written against unfixed production code and reported **38 failures**: 8 x `A2 RED` (four spells x "a field adds a primitive" / "the viewport differs"), 1 x `A2b RED` (the authored Wrong field), 28 x `B2 RED` (seven dungeons x four sampled rooms) and 1 x `B2 RED: Doom's room 0 resolves to a board at all`. Every A1 and B1 assertion passed in that same run — which is the point: the state and the transform were never broken. After the two production fixes: **0 failures**. The new A14 block was then mutation-checked both ways and reverted: `kDungeonFieldSparkLen[0] = 8` turns it RED (1201 failures), and collapsing the colour selector to `kDungeonFieldSparkColor[0]` turns it RED (930 failures).

**Host suite:** **80 total, 78 pass, 2 fail** — up one registered test from Batch 12's 79, and the **same two** pre-existing failures the pre-batch baseline run produced on this tree: `gameplay_parity` (R-21, *"Gameplay mismatch 2034"*, deferred by instruction) and `quest_parity` (`STATUS_ACCESS_VIOLATION`, exit `3221225477`, the MinGW/w64devkit environment finding from Batch 8). All magic, dungeon and combat suites green, `batch12_combat_field`, `dungeon_combat_regression`, `dungeon_parity`, `dungeon_art_regression`, `dungeon_view_regression`, `magic_parity`, `combat_parity` and every TypeScript drift check included.

**Firmware:** `idf.py -B build-batch12b build` **PASS** — `openu5_tdeck.bin` **0xd06d0 bytes**, `+0x2a0` (672 bytes) over Batch 11/12's `0xd0430`, which is the sparkle subsystem plus the retag. App partition **19% free**. No compiler warnings from project code.

---

### Batch 13 — R-21 adjudicated: the fabricated (U)se-consumable echo · risk: low · **GREEN — CONFIRMED NATIVE DEFECT, FIXED; two inseparable siblings fixed with it**
**IDs:** R-21 (was RED/OPEN since Batch 2) — now **GREEN**.

**Verdict: confirmed native behavioural defect.** Not a harness defect, not a bad reference expectation, not an initialisation or ordering artefact of the fixture, and not authored data. The parity fixture and the reference were right; native was wrong.

**"Mismatch 2034" decoded.** `check-gameplay.ts` throws `Gameplay mismatch ${i}` with `i` the **sequence index**, so 2034 is scripted sequence #2034, the first `(U)se` of a scroll anywhere in the fixture. Sequences are independent (`add()` builds a fresh `initial()` state each time) and the divergence is in that sequence's first and only step, so 2034 is the **first** divergence, with no earlier candidate to bisect toward. Sequences 0–2033 are the blocks that never touch a consumable.

**Causal chain.** `(U)se` scroll 0 → reference `CAST.OVL 0x11de` consumes the scroll and prints the category word `Scroll` (DS `0x466a`), then the handler prints `Light!` → native `world_magic()`'s `used()` helper printed `Used Vas Lor Scroll.` instead → first message of the step differs → mismatch 2034. The same helper, and a `"No effect!"` fallback for results with no authored DS line, were applied at the arena mouth too (`combat_use_consumable`).

**Provenance.** Both fabrications came from the Alpha-20 forensic action-feedback pass, which documents the intent verbatim and also documents that its Node/tsx reference generators were infrastructure-blocked and never ran. A device-UI choice landed in the core command layer and no parity run was there to catch it. It was invisible until Batch 2 fixed mismatch 59 and let `gameplay_parity` reach sequence 2034.

**Scope note — two inseparable siblings.** Closing R-21 forced the parity fixture to express the reference's magic-feedback rules, and those rules cannot be stated once and be true at only two of the three mouths. So two adjacent defects of the same family were corrected with it, and are reported here rather than folded into R-21's headline: (a) the **potion ceremony fired after the effect line**, where `CAST.OVL 0x139b` puts it before the reroll at `0x13a1`; (b) **dungeon casts raised no ceremony at all**, although the binary has a single cast dispatcher and `main.ts` emits it at all three cast mouths. Nothing else was touched.

**Production files changed:** `native/core/src/world_magic.cpp`, `native/core/src/combat.cpp`, `native/core/src/dungeon_orchestration.cpp`. Display-name tables untouched and asserted as a control.

**Test/harness files changed:** `native/core/tests/batch13_test.cpp` (new), `native/core/tests/gameplay_driver.cpp` (name `MagicCeremony`, carry its index — it was serialised as `unknown` with the index dropped), `native/core/tools/check-gameplay.ts` (model the reference ceremony at all four magic mouths from `ceremony.ts`'s derived tables; normalise out the four native-invented audio hooks), `native/core/tests/action_feedback_test.cpp` (**corrected** — it was asserting the fabrication), `native/core/CMakeLists.txt` (register `batch13`).

**RED → GREEN.** `batch13` **58 failing checks → 0**. Group E (ceremony order) RED **2**, group F (dungeon ceremony) RED **7**, each with its own passing control (E3, F3). Six mutations, each on a clean rebuilt tree and reverted, all caught by both the focused test and `gameplay_parity`; M1 reproduces the historical signature `Gameplay mismatch 2034` exactly, and M5 — one digit changed in a ceremony index — fails a parity check that did not exist before this batch.

**Full suite:** **81 total, 80 pass, 1 fail.** Baseline measured at the start of this batch on `a4f1c5a3`: 80 total, 78 pass, 2 fail. The +1/+1 is the new `batch13` target; `gameplay_parity` moved **fail → pass** and no other result changed. The single remaining failure is `quest_parity`, the MinGW/w64devkit `STATUS_ACCESS_VIOLATION` (exit `3221225477`) recorded in Batch 8 — present identically in this batch's own pre-edit baseline, not investigated, and not inseparable from R-21 in any way.

**Firmware:** T-Deck ESP-IDF 6.1 build **PASS** into `native/targets/tdeck/build-batch13`. `openu5_tdeck.bin` = `0xd06f0` (**853,744 bytes**), **+32 bytes** over Batch 12B's `0xd06d0` (853,712). App partition `0x2f910` = **194,832 bytes (19%) free**. **Zero warnings, zero errors.** Hardware flash not performed — see §16 Phase 6G.

**Newly exposed downstream parity issues:** none. `gameplay_parity` passes outright — it did not move to a different mismatch.

**Known gap left open, deliberately:** Vas Rel Por (spell 46) performs its ceremony only after its phase key (`CAST.OVL 0x0d31`, literal index 8). The native command does not model the phase gate, so neither native nor the fixture emits it, and the fixture excludes 46 explicitly rather than pretending to cover it. That is a pre-existing gap noted by the Alpha-20 pass, not a Batch 13 regression, and it is listed in §4 rather than fixed here.

---

### Batch 14 — Z-stats status/inventory pages · risk: medium, moderate scope · **GREEN — IMPLEMENTED AND VALIDATED** *(was numbered Batch 13; renumbered because Batch 13 went to R-21)*
**IDs:** R-22 (evidence-only since the pre-Batch-4 Developer-tool cleanup) — now **GREEN**.

#### The proven page family and navigation

Adjudicated from the binary first. `ZSTATS.OVL` overlay #23 and the DATA.OVL DGROUP tables it reads are the authority (`re/notes/zstats.md`, `re/notes/ztats-layout.md`); the TypeScript skin (`game/src/skin/fiel/ztats.ts`) corroborates and is cited where it refines, never where it differs.

**Selection runs FIRST.** `cmd_zstats` (0x0a3a) does not open a page: at 0x0a40 it calls `select_player` (0x0000), which prints `"Player: "` (DS 0x96b4) and enters the kernel picker. Only the member it returns opens a page, at axis slot `member*2`. Native already had this half — it is the only part of `(Z)` that worked.

**The axis is 17 slots (`[bp-2]`), not "seven pages":**

| Slot | Page | Routine | Banner |
|---|---|---|---|
| even 0,2,…,10 | **Stats** of member `idx>>1` | `draw_stat_page` 0x0082 | member name |
| odd 1,3,…,11 | **Arms** of member `idx>>1` | `draw_arms_page` 0x02a8 | member name |
| `0xc` | **Provisions** | 0x039c (headerless tail-call) | `Equipment` (DS 0x9724) |
| `0xd` | **Reagents** | `render_item_list` 0x06e8 | `Reagents` (0x97ac) |
| `0xe` | **Spells** | `render_item_list` | `Spells` (0x97b6) |
| `0xf` | **Items** (quest/special) | `render_item_list` | `Items` (0x97be) |
| `0x10` | **Armaments** | `render_item_list` | `Armaments` (0x97c4) |

The ring is **circular with four wrap gates** and never enters the unused window `[party*2, 0x0b]`: forward, `party*2-1 → 0xc` (0x0af7) and `0x10 → 0` (0x0b0a); backward, `0 → 0x10` (0x0ac6) and `0xc → party*2-1` (0x0aac).

**Two prior descriptions in this document were wrong and are corrected here.** (a) The odd page is **Arms only** — the asm at 0x02a8 paints the six equipment slots `+0x19..+0x1e` and nothing else; the "equipment + spellbook" reading was an earlier note's invention. (b) List `0xe` is **Spells** (the mixture counts at `0x57f0`, confirmed by the dumped name table at DS 0x19e2), not a second generic item page.

**Field-level facts pinned per page.** Stats (0x0082) prints the **level on the header line** (record `+0x16`) beside the sex glyph and class, the health word centred, then two columns: `Str=`/`  HP:`, `Int=`/`  HM:`, `Dex=`/`  Ex:` — where **`HM` is max HP (`+0x12`)** and **`Ex` is experience (`+0x14`)** — and `    Magic:` (current MP, `+0x0f`, the one 2-wide field that pads with SPACE, not `'0'`). Arms (0x02a8) walks `+0x19` helmet, `+0x1a` armour, `+0x1b` hand A, `+0x1c` hand B, `+0x1d` ring, `+0x1e` amulet, **skipping** `0xff` slots (`print_padded_string` 0x0278 returns 0), and prints `(None ready)` (0x9716) when all six are empty. Provisions (0x039c) prints `" Food: "`/`" Gold: "` (4-wide) then the dot-leader counters `" Keys......."`/`" Gems......."`/`" Torches...."` (2-wide), and the `" Grapple"` line only when one is carried. Lists (0x06e8) show **seven** content rows (the render loop stops at cursor row 8, the frame's bottom edge), filter to `qty>0` via `find_next_owned` 0x05a4, format each row as a 2-digit space-padded count + `'-'` (0x2d) + name, drop **both** number and separator when the stored byte is the `0xff` sentinel (0x062e), and print `(None owned!)` (0x9794) when nothing is owned.

**Key matrix.** Space (0x0a78) and ESC (0x0a81) are the **only** keys that close — navigating never closes. Arrows cycle the axis; inside a list that actually overflows, up/down **scroll** (0x081c/0x086c) and left/right leave to change page (0x0948), while a list that fits falls through to the axis (§8.5). PgUp/PgDn step a literal 7. `'1'`–`'6'` jump to a member's stats page bounded by `g_party_size` (0x0b12) and `'0'` jumps to Provisions (0x0b37). **`'z'` is not read inside the loop at all** — there is no `cmp` for it; it opens and never re-enters or closes.

#### What was built

A new ESP-free presentation seam, `native/core/include/openu5/zstats.h` + `src/zstats.cpp`, turns live `GameState` into a composed `ZStatsPage` (banner, up to eight row strings, list totals and scroll markers). It is presentation only: it writes nothing, reads no clock and draws no RNG — matching `cmd_zstats`, which consumes exactly **zero** rolls and charges **no** turn. The Items page deliberately **reuses `usable_item_picker_rows()`** (R-07/R-08's seam) for its flag/counter tail, because the original reads one extended table (0xB9EE) for both the `(Z)` Items page and the `(U)se` picker; that keeps the moonstone gate (`owned == !buried`) in a single place instead of forking it.

`AlphaRuntime` gained the modal itself: `open_zstats()` arms the axis when the picker confirms a member (this arm previously **reopened the very same picker**, which is exactly why `(Z)` had no pages), `handle_zstats_input()` is the key loop, and it is called from `handle()` **before any routing**, beside the gem and zodiac views — so a direction can never reach `dispatch_world_command()` as a Move and a command letter can never arm a prompt behind the modal.

Rendering reuses the existing compact-selector panel primitive (title band, two detail lines, eight rows, context bar) rather than adding a second text renderer: the original's page is sixteen cells wide and eight rows tall, which is what that panel already shows.

#### One narrow production defect this exposed, and fixed

`openu5::equipment_display_name()` was **off by one for all 48 ids**. Its table had been transcribed from `InventoryDetails.json`'s `Armament` map, which carries a phantom `[0] = "BareHands"` entry from the clone-era Redux data set; the reference itself compensates with `.slice(1)` (`game/src/ui/shop-console.ts`, and the rule is spelled out in `game/src/core/world/search.ts`'s `EQUIP_IDX_GLASS_SWORD` comment) and native did not. Three anchors **inside native itself** pin the correct alignment, independently of any name table:

* `equip_type_of(9..15) == 0x40` — the armour band `ZSTATS.OVL:0x0c94` locks in battle, so id 9 is an armour, not the Jewel Shield;
* `ammo_item_for()` maps 26/36 → 27 and 28 → 29 — Bow/Magic Bow → **Arrows**, Crossbow → **Quarrels** (0x0d0c);
* `is_thrown_weapon()` is `{16, 21, 22}` — Dagger/Spear/Throwing Axe (`COMSUBS:0x097c`).

So `equipment_display_name(16)` returned **"Mystic Armour"** for the Dagger and `(27)` returned **"Bow"** for the Arrows. Every consumer was showing the neighbouring item's name — the Ready picker, the blacksmith, the Developer equipment rows — and `(Z)`'s Arms and Armaments pages could not have been correct without fixing it. The fix is the table itself, re-seated onto the binary's own id space (the byte-exact DS 0x17f6 dump, `game/src/core/data/longEquipNames.json`), which also restores the missing 48th entry, **Ankh**, and corrects `"Amulet of Turning"` to the binary's `"Amulet/Turning"`. A `static_assert` now pins the table at exactly 48. This is the only production change outside the Z-stats path, and `display_names_test.cpp`'s three anchors — which had encoded the off-by-one — were re-seated with it.

#### Files changed

| File | Purpose |
|---|---|
| `native/core/include/openu5/zstats.h` | **new** — the ESP-free Z-stats presentation contract: page axis constants, `ZStatsPage`/`ZStatsList`, the axis and compose entry points |
| `native/core/src/zstats.cpp` | **new** — composes all four page kinds from live `GameState`; the axis ring and its four wrap gates |
| `native/core/src/display_names.cpp` | equipment name table re-seated onto the binary id space (+ `Ankh`, `Amulet/Turning`, `static_assert(48)`) |
| `native/core/sources.cmake` | registers `zstats.cpp` for host and ESP-IDF alike |
| `native/core/CMakeLists.txt` | registers `batch14_zstats_model` and `batch14_zstats_runtime` |
| `native/core/tests/batch14_zstats_model_test.cpp` | **new** — 97 checks over the page model |
| `native/core/tests/display_names_test.cpp` | the three off-by-one anchors re-seated, two added |
| `native/targets/tdeck/main/alpha_runtime.h` | the modal's state, its three methods, and the `zstats_active()`/`zstats_view()` host-test seam |
| `native/targets/tdeck/main/alpha_runtime.cpp` | `open_zstats()`, `handle_zstats_input()`, the pre-routing interception, and the panel composition |
| `native/targets/tdeck/host_tests/batch14_zstats_runtime_test.cpp` | **new** — 54 checks driving the real production routing |
| `native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md` | this write-up, the matrix rows, the §11 resource rows and the §16 Phase 6H checklist |

#### Tests and RED → GREEN evidence

Both suites were written **before** the implementation and run against an inert seam that reproduced pre-batch behaviour exactly (the modal never opened; `(Z)` still just reopened the picker):

* `batch14_zstats_model` — **96 checks, 86 failures** RED → **97 checks, 0 failures** GREEN (the 97th is a later placeholder sweep). The ten vacuous greens in the RED run were degenerate cases (an empty list is empty either way).
* `batch14_zstats_runtime` — **54 checks, 33 failures** RED → **54 checks, 0 failures** GREEN. The 21 vacuous greens there were "nothing happened" assertions that trivially held while no modal existed.

Because several of those runtime greens were vacuous, the isolation guards were **mutation-checked** as well: deleting the single pre-routing interception line in `handle()` (and nothing else) turns **27 of 54** checks RED — including every Z9 isolation assertion, both Z10 exit assertions and all four Z11 turn/RNG assertions. Reverted; back to 0 failures.

During the GREEN pass, fifteen row-spacing assertions were adjudicated against the DGROUP strings rather than forced to match: the trailing space in `" Food: "`/`" Gold: "` (DS 0x972e/0x9738) was a **production** bug and was fixed, while the stat-column and centring expectations were **test** errors — the two spaces before `HP:`/`HM:`/`Ex:` belong to the label strings themselves, and the centring width is 16 cells, because `draw_stat_page`'s second `set_text_window` is `(1, 0x18, 1, 0x27, 9)` = cols 24..39. (The faithful TypeScript skin narrows its own panel to 15 for roster alignment; that is its refinement, not the asm's.)

| Guard | Covered by |
|---|---|
| Z1 command routing | `batch14_zstats_runtime` Z1-0…Z1-6 |
| Z2 live character data | `batch14_zstats_model` Z2-1…Z2-14 |
| Z3 equipment naming | model Z3-1…Z3-20 (incl. the off-by-one anchors and a full placeholder sweep) |
| Z4 inventory counts | model Z4-1…Z4-5, Z4a-1…Z4a-11 |
| Z5 inventory ordering | model Z5-1…Z5-16 |
| Z6 zero counts | model Z6-1…Z6-5 |
| Z7 character navigation | runtime Z7-1…Z7-6 |
| Z8 page navigation | model Z8-1…Z8-8, Z8b-1…Z8b-8; runtime Z8-1…Z8-6, Z8b-1…Z8b-6 |
| Z9 modal input isolation | runtime Z9-1…Z9-7 |
| Z10 exit semantics | runtime Z10-1…Z10-6 |
| Z11 no turn/time mutation | runtime Z11-1…Z11-4 |
| Z12 abnormal status | model Z12-1…Z12-4 |
| Z13 party-size edge cases | model Z13-1…Z13-6; runtime Z13-1…Z13-8 |
| Z14 repeated lifecycle | runtime Z14-1…Z14-4 |

#### Residual differences from the original

Recorded honestly; none of them is a semantic difference.

1. **No IBM.CH box frame around a list.** `draw_list_frame` (0x045e) paints a 15x10 scroll frame from glyphs 0x10/0x11/0x13–0x17. The T-Deck's 5x7 face is ASCII-only and the compact-selector panel already carries its own cyan rules in the same place, so the frame is not drawn. Content, row count, ordering and paging are unchanged.
2. **Sex is `M`/`F`, not the CP437 `♂`/`♀`.** Same cause: `tdeck_board.cpp`'s `glyph()` is an ASCII switch. `GameState::gender` still holds the binary's own 0x0b/0x0c.
3. **Item names are unabbreviated.** The original's list column is 14 cells and its name tables are abbreviated to fit it (`"Sp. Silk"`, `"Sht. Sword"`, `"In Sanct G"`). The T-Deck row is wider, so every row names its item through native's existing canonical helper. The item SET, the ORDER and the quantity column — the authored facts — are reproduced exactly; the abbreviation is a column-width artifact.
4. **No rune sigils on the Items page.** `print_list_row`'s `*`/`!`/`(` branches switch to `RUNES.CH` to draw a scroll/potion/moonstone pictogram. Native prints the canonical name instead (`"In Sanct Scroll"`, `"Blue Potion"`, `"Moonstone"`), which carries the same information in an ASCII face.
5. **Pocket Watch (extended id 0x23) still has no row.** No `GameState` field backs it, so no row can be gated on possessing it — the same R-08 gap the `(U)se` picker has, unchanged by this batch.
6. **No `"Player: <name>"` / `"Status: "` console echo during selection.** The binary echoes the chosen name and a persistent `Status:` line with the wave cursor for the duration of the sheet. Native keeps its existing `"Z-stats"` command echo. Presentation only; filed as a minor follow-up, not fixed here.
7. **Digit shortcuts work inside the page loop, not inside the picker.** `'0'`–`'6'` are asm-derived for the page loop (0x0b12/0x0b37) and are implemented. The picker's own digit selection is *observed* behaviour of a kernel routine outside this overlay, and wiring it would mean changing digit handling for **every** party picker in the game (Ready whom, Use on whom, …) — out of scope here.

#### What this batch did NOT do

- It did not touch R-23 (Blackthorn roster compaction), Y-29, Y-31/Y-32 or the `quest_parity` harness crash; all four remain open and are unchanged.
- It did not reopen Batch 13's magic-ceremony work.
- It did not refactor the inventory system, the display-name infrastructure beyond the single off-by-one table, or the UI architecture.
- It did not change the resource pack, so the flashed SD card needs no repack.

**Host suite:** **83 total, 82 pass, 1 fail** — two registered tests up from Batch 13's 81, and the **only** failure is the same pre-existing one the pre-batch baseline run produced on this tree: `quest_parity` (`STATUS_ACCESS_VIOLATION`, exit `3221225477`, the MinGW/w64devkit environment finding from Batch 8). `gameplay_parity` remains **5058/5058 GREEN**. `display_names` passes with the corrected table, and every TypeScript drift check is green.

**Firmware:** `idf.py -B build-batch14 build` **PASS**. `openu5_tdeck.bin` = `0xd19e0` (**858,592 bytes**), **+4,848 bytes** over Batch 13's `0xd06f0` (853,744) — the new page model, the modal and the panel composition. App partition `0x2e620` = **189,984 bytes (18%) free** (was 19%). **Zero warnings from any project file** (the only log warnings are the five pre-existing ESP-IDF `component_validation.cmake` notices about `esp_wifi`/`wpa_supplicant` private includes). The resource pack is unchanged, so **no SD recopy is needed**. Hardware flash not performed in this session — the device checklist is §16 Phase 6H.

---

### Batch 15 — Blackthorn sacrifice roster compaction and party-index parity · risk: low, one-bound scope · **GREEN — IMPLEMENTED AND VALIDATED**
**IDs:** R-23 (evidence-only since the pre-Batch-4 Developer-tool cleanup) — now **GREEN**.

#### What the reference actually does

Adjudicated from the binary before anything was written, because the open finding had been recorded from the TypeScript port. `BLCKTHRN.OVL 0x0438` `sacrifice_member`, whose instruction trace is given in full in `re/notes/blackthorn-cota-party-size.md` §1 and summarised in `re/notes/blackthorn.md` §3.3, with the slot-15 arithmetic re-measured independently in `re/notes/cabeza-264b-acta.md` §4.1:

| step | asm | rule |
|---|---|---|
| bound | `0438` `al = g_party_size` · `or ax,ax / je 0x46c` | the **only** roster bound is `g_party_size`; `0` is an early-out. No `cmp si,6` — that belongs to the chest-trap family, and the two conventions are deliberately different |
| victim | `044c` `cmp byte [si],'D'` · `inc cx` · `cmp cx,2` | the **second LIVING member**. Not "slot 1", and not "never the Avatar": with slot 0 dead, living #1 is slot 1 and the victim is slot 2 |
| park | `046c` `repne movsw cx=0x10` | the **whole 32-byte record** is copied to a local first |
| gate | `0487` `cmp ax,0xf / jge` | a victim that already **is** slot 15 skips the compaction rather than reading `record[16]` |
| compaction | `04ab-04c0` `record[i] = record[i+1]` until `si == 0x57a8` | `0x55a8 + 512` — the end of **all sixteen records**, *not* the party bound |
| tail | `04c2-04cd` `record[15] = TEMP` (`DS:0x5788`) | the executed companion is **parked in slot 15**, and persists there in the save window |
| flag | `04cf` `byte [0x57A7] = 0x7f` | record `+0x1F` = `partyStatus`; `0x7f` also retires the `0x00` "in party" value |
| count | `04d4` `dec [g_party_size]` | **last**, and exactly once |

So, for the batch brief's own worked example: an active party `[Avatar, Shamino, Iolo, Dupre, Jaana]` sacrifices **Shamino**, not Iolo, because Shamino is living #2. Iolo is the victim only when Shamino is dead — and then the result is exactly `[Avatar, Shamino(D), Dupre, Jaana]`: the compaction moves **slots**, so a dead member keeps its place in the roster and is not skipped. The vacated final active slot is not zeroed and carries no sentinel; it receives whatever the next roster record held, because the shift simply continues through the non-party tail.

#### The native divergence, and the two things the open finding got wrong

Native's shift ran `for (j = i+1; j < g.party.character_count; ++j)` and then forced `character_count = 16`.

1. **The immediate artefact was a DUPLICATE, not a blank.** Slot `old_count-1` kept a stale copy of its own previous occupant — the roster's last record appeared twice — and `[old_count, 15)` was never shifted at all. The original entry's "blank roster gaps" described the untouched tail but named the defect wrongly.
2. **It was never reachable on the device.** Every `.GAM` yields sixteen records (`persistence.cpp` `import_native`), and at `character_count == 16` the old bound and the reference bound are the *same expression*. The shipped T-Deck roster was already correct, and so was every `quest_parity` Blackthorn scenario, which builds a full 16-record roster (`tools/check-quests.ts:174`) — which is exactly why no parity suite ever caught this. The divergence lived only on rosters shorter than sixteen records, which `save::deserialize`/`restore_core` accept. **Latent on hardware, live on the host/JSON surface.**
3. `character_count = 16` was **not** part of the defect: it mirrors the reference's fixed sixteen-record table and the port's own `characters[15] = rec`, which forces `characters.length` to 16 with holes between — and a hole reads `?.status !== "D"`, i.e. *living*, exactly as native's default-constructed record does.

#### The index rule — adjudicated, and deliberately left alone

The batch brief asks explicitly not to pick a policy. The reference's policy is **no adjustment at all**. The census of persistent roster indexes (`re/notes/party-contiguidad-acta.md` §3) finds exactly one — `activeCharacter`/`g_active_char` — and `sacrifice_member` does not touch it. The re-indexing block is `SHOPPES3 0x03dd-0x0400`, which belongs to the inn **(L)eave** (native `inn_leave`, `shops.cpp:275-278`, already faithful: clear to `0xff` if it *is* the departure, decrement if it is *after* it); `BLCKTHRN 0x03ae-0x04d4` has no counterpart, the same declared asymmetry the inn **(P)ickup** already carries.

Consequence, cloned bug-for-bug: after a sacrifice the active-character index keeps its numeric value, so an index that pointed **at** the victim now addresses the survivor that compacted into the slot, and an index **after** the victim now addresses the next character along. Group B pins this from both sides — B2/B3 assert the sacrifice leaves the index alone, B4 asserts `inn_leave` still re-indexes — so a later "make these consistent" pass cannot quietly unify two routines the original kept different.

#### The fix

One bound, in `native/core/src/blackthorn.cpp`: the compaction loop runs to `kRosterCapacity` instead of `g.party.character_count`, matching `04ab-04c0`'s run to the end of the table. Victim selection, the slot-15 park, the `0x7f` `partyStatus`, the `party_size` decrement and its ordering are unchanged — they were already correct.

The routine was lifted out of the file's anonymous namespace and declared in `openu5/blackthorn.h` as `sacrifice_first_companion()` (the reference's own name) so the mutation can be driven at a production seam rather than hand-mirrored in a test; its two call sites in `blackthorn_action()` are otherwise unchanged. **No shared party-deletion primitive was introduced.** `inn_leave`/`inn_pickup` are a different asm body with a different active-character rule and a different park slot; merging them would destroy the asymmetry above. No party container was redesigned, no bounds-checking was rewritten, and the save representation is untouched.

#### RED → GREEN

`batch15_sacrifice_roster` was written first and run against the **unmodified** body, exposed through the new seam with the defective bound still in place: **75 checks / 4 failures**, and the four are exactly R-23 — two from the direct seam (group A2, a six-record roster keeping a duplicated tail) and two from the **real scripted Blackthorn path** (E1's four-failed-round pendulum and E3's conceded mantra, both driven through `blackthorn_action()` rather than a helper). After the one-bound fix: **75 / 0**.

Because most of the suite was green before the fix — correctly, since those aspects were already reference-faithful — four mutations were run against production code and reverted, to prove no green is vacuous:

| mutation applied to production code | result |
|---|---|
| add the "intuitive" active-character re-index (copy `inn_leave`'s block) | **2 RED** — B2, B3 |
| park the victim at `characters[count-1]` (the inn's slot) instead of 15 | **3 RED** — A2 ×2, E1 |
| remove the compaction shift entirely | **17 RED** — A1–A6, B2/B3, C1/C3, E1/E3 |
| decrement `party_size` twice | **9 RED** — A1, A2, A3, A6, A7, D1, E1, E3, F1 |

#### Coverage — `native/core/tests/batch15_sacrifice_test.cpp`, 75 checks

| group | what it drives | brief |
|---|---|---|
| A (27) | the `sacrifice_first_companion()` seam directly: 16-record control, the short-roster defect, the worked `[Avatar, Shamino(D), Iolo, …]` example, earliest legal victim, dead-Avatar victim selection, last active member, the `0x0487` victim-is-slot-15 gate, the fewer-than-two-living no-op (byte-compared whole `PartyState`), and the `party_size == 0` early-out | C1, C2, C3 |
| B (7) | `activeCharacter` before / at / after the victim, plus the `inn_leave` contrast | C4, C5, C6 |
| C (9) | real `execute_command()` `SetActivePlayer` and `NewOrder` after the compaction — asserting the command acts on the **intended surviving character by name**, not merely on a numerically valid slot, and that the bound followed `party_size` down | C7 |
| D (7) | Batch 14's `(Z)`-stats page ring after the roster shrinks: `zstats_clamp_page` folds a stranded page back to the last live member's Arms page, and `compose_zstats_page` on the *unclamped* stale page yields an empty page through the `member_valid` gate rather than reading a dead slot | 15E |
| E (14) | the **real** scripted event: `blackthorn_action(Capture)` then four failed rounds (the pendulum), the conceded-mantra branch, the lone-Avatar pardon, and the ordering pin that the "… die!" threat names slot 1 **before** any compaction (`0x0531` prints `DS 0x55c8`) | C8 |
| F (4) | `capture_core`/`restore_core` round trip: all sixteen records in order, both counts, and the parked victim's `0x7f` | 15E |

The `(Z)`-stats check in group D also recorded one thing worth stating plainly: `AlphaRuntime::zstats_view()` composes from the **unclamped** `zstats_page_` (only `handle_zstats_input()` clamps). That is not a defect — the `member_valid` gate makes a stale page render as an empty page, never an out-of-range read — and it is not reachable in production anyway, because the modal intercepts input before any routing, so no gameplay command can shrink the roster while it is open. D3 pins the safe behaviour so it stays safe.

#### What this batch did NOT do

- It did **not** mark broader Blackthorn ceremony fidelity GREEN. The capture/interrogation presentation and pacing work (Y-31/Y-32) is untouched and still open, §2's two Blackthorn rows keep their existing R-09-era verdicts, and the sequence's hardware validation is still owed.
- It did not touch Y-29, Y-31/Y-32, Y-33 or the `quest_parity` harness crash — all remain open and unchanged.
- It did not unify the sacrifice and inn-leave compactions, redesign party storage, add bounds checks, or change the save format.
- It did not change the resource pack, so the flashed SD card needs no repack.

**Host suite:** **84 total, 83 pass, 1 fail** — one registered test up from Batch 14's 83 (the new `batch15_sacrifice_roster`), and the **only** failure is the same pre-existing one this batch's own pre-edit baseline run produced on this tree: `quest_parity` (`Native driver failed: 3221225477`, `STATUS_ACCESS_VIOLATION`, the MinGW/w64devkit environment finding from Batch 8). `gameplay_parity` remains **GREEN**. `blackthorn_scene`, `batch45b`, `batch4_group_a`/`_b`, `batch14_zstats_model`, `batch14_zstats_runtime`, `alpha_runtime_integration_regression`, `persistence_parity`, `shop_parity`, `ui_session` and every TypeScript drift check are green. The one compiler warning in the build log is a pre-existing `-Wstringop-overflow=` from libstdc++ inlining inside `command_parity_test.cpp`, present identically in the baseline build.

**Firmware:** `idf.py -B build-batch15 build` **PASS** — size and headroom in the closeout line below. Hardware flash was not performed in this session, and **no hardware step is owed by this batch**: the defect it fixes is unreachable from a `.GAM`-loaded device roster (all sixteen records), so there is nothing a tester could observe on device that the host suite does not already prove. The physical-device backlog stays consolidated into the later hardware batch.

---

### Batch 16 — Input / presentation cleanup: Y-29, Y-31, Y-33 fixed; Y-32 investigated and reclassified · risk: low-medium, four independent findings · **GREEN — THREE CONFIRMED NATIVE DEFECTS FIXED; ONE ADJUDICATED AS INSUFFICIENTLY EVIDENCED, NO CODE CHANGE**

**IDs:** Y-29 (**GREEN — RESOLVED**), Y-31 (**GREEN — RESOLVED**), Y-32 (**OPEN — investigated, reclassified, no fix**), Y-33 (**GREEN — RESOLVED**).

Scope was archaeology-first for all four findings, exactly as instructed: nothing in production code was touched until each item's current behaviour was traced end to end and adjudicated against the audit's own claim. Three were confirmed live defects with an exact, narrow fix; one (Y-32) turned out to have no reference evidence anywhere in the repository to fix it *against*, so it was left alone and the new archaeology was recorded instead of a fabricated fix.

#### Adjudication table

| ID | Classification | Root cause |
|---|---|---|
| Y-29 | Confirmed native defect, fixed | `UiInputAdapter::translate()`'s Mic/0 branch matched on matrix position alone and never consulted `raw.modifiers`, so the hardware's own Symbol-layer route to a literal `'0'` was unconditionally swallowed by the short=Cancel/long=Movement-Mode special case. |
| Y-31 | Confirmed native defect, fixed | `UiSession`'s `TargetSelection` cancel arm had no case for a pending `UseItem`, so cancelling the Rel Hur/skull-key getdir sent a bare `ModalResponse{accepted=false}` that dispatched nothing — refunding an item the reference had already consumed at selection time, before the getdir ever opened. |
| Y-32 | Investigated; reclassified from "unassessed" to "confirmed real observation, no footage evidence exists to fix it against" | No cause. Archaeology found zero footage-derived timing evidence for the Blackthorn capture scene anywhere in the repository — not in `re/notes` (which explicitly disclaims a witness for this scene) and not in the TypeScript port (which reuses TrollSneak's calibration wholesale). A candidate structural cause (no reading floor on Message steps) was found and ruled out by direct trace: it does not currently manifest. |
| Y-33 | Confirmed native defect, fixed | The native command modelled no phase gate at all for Vas Rel Por: no prompt, no `'1'-'8'` validation, and `Command.hours` (reserved for the phase) defaulted to 0 — a *valid* phase — so every cast silently teleported to moonstone 0 with zero player input. |

#### Y-29 — Sym+Mic/0

Traced end to end from `native/targets/tdeck/main/ui_input_adapter.cpp`'s Mic/0 branch (lines 64-88 pre-fix) through `keyboard_matrix.cpp` (confirming `kSymbol[0][6]=='0'` is the sole matrix cell producing the character `'0'`) to `ui_session.cpp`'s `handle_exploration` digit switch and `commands.cpp`'s `SetActivePlayer` handler (both already correct and unreachable only for lack of the input). The audit's own pre-Batch-4 investigation had already surveyed the alternatives and recommended Symbol+Mic/0 as the correct route; this batch implemented exactly that recommendation, unchanged.

**Fix.** `ui_input_adapter.cpp`'s Mic/0 branch now checks `raw.modifiers.symbol` before touching any of the short/long bookkeeping (`mic_down_`/`mic_pressed_us_`/`mic_long_hold_handled_`). A Symbol-held press emits `UiActionKind::Character,'0'` on the Pressed edge and swallows the paired Released edge; a plain press is byte-for-byte unchanged.

**RED → GREEN.** `native/targets/tdeck/host_tests/input_test.cpp` already contained a test block exercising Symbol+Mic/0 through `translate()` — it was pinning the *old* (buggy) behaviour (asserting Symbol+Mic/0 still yields `Cancel`). Updated to assert the correct `Character,'0'` result: confirmed **RED** against unmodified production code (exit 205, the new assertion), then **GREEN** after the fix. **Mutation:** the emitted character was changed from `'0'` to `'1'` — caught (RED, exit 205) — and reverted.

#### Y-31 — Rel Hur / skull-key cancel refund

Traced the cancel path in `ui_session.cpp`'s `TargetSelection` mode: `UseItem` was the one pending-command kind with no dedicated cancel arm, falling into the generic `ModalResponse{accepted=false}` branch that dispatches nothing. Traced consumption in `world_magic.cpp`: both the scroll (ids 0-7, line 27) and the skull key (item 17, line 18) decrement unconditionally on `cmd.kind==UseItem`, independent of `cmd.has_direction` — so the existing `Cast` cancel arm's exact pattern (dispatch with the direction/target unset) reproduces the reference precisely for both items with no `world_magic.cpp` change at all.

**Fix.** A new `use_item` case in the `TargetSelection` cancel arm, alongside the existing `klimb`/`attack`/`cast` cases: `cancelled.has_direction=false; command(cancelled);`.

**RED → GREEN.** New tests B5 (Rel Hur)/B6 (skull key) in `native/core/tests/batch5_test.cpp`, asserting a cancelled getdir still dispatches `UseItem`: **RED** against unmodified code (the generic `ModalResponse` arm dispatches nothing), **GREEN** after the fix. Two `world_magic()`-level control tests (C7/C8, already passing pre-fix) independently proved the consumption mechanics were never broken — only the dispatch decision was. **Mutation:** the `use_item` case body was replaced with a no-op — caught (2 failures), reverted.

#### Y-32 — scripted-event pacing (Blackthorn capture)

Investigated independently of Y-31 per instruction (they share nothing but adjacent audit IDs — see the §2 Blackthorn row correction above, which fixes a pre-existing mis-association in several historical batch write-ups that had bundled "Y-31/Y-32" together as if both were Blackthorn findings). Full archaeology across `narrative_scene.h`/`.cpp`, `blackthorn_scene.h`/`.cpp`, the Batch 4.5D commit (`29a69fb6`) that built the scene's presentation, `re/notes/blackthorn-escena-324.md`, and `game/src/core/world/blackthorn-scene.ts`/`game/src/ui/blackthorn-scene-pacer.ts`.

**Finding.** Every dwell value in `blackthorn_scene.cpp`'s script builders is the disassembled bytecode's own literal pause argument (cited instruction-by-instruction) — not invented. But the 55ms/tick conversion applied to those arguments has **never been checked against footage for this scene specifically**: `re/notes/blackthorn-escena-324.md` §4 explicitly states there is no witness for this scene's cadence, and the TypeScript port's `PAUSE_UNIT_MS` (`game/src/skin/world-fx.ts:58`) is an admitted reuse of TrollSneak's calibration, documented as such in its own comment. Batch 4.5D, which built the scene's presentation from nothing, reused the same generic convention and made no independent timing claim either.

A candidate architectural cause was found and directly ruled out: `BlackthornScenePacer` (unlike `NarrativeScenePacer`, which enforces `kRefugeTextFloorMs`/`kRefugeSceneFloorMs` reading floors on text beats) has no reading-floor mechanism for `Message`/`Forward`/`Prompt` steps. Tracing the actual `event(...)/build_*_script(...)/emit_scene(...)` interleaving in `blackthorn.cpp` shows every message is already followed by either a genuine blocking `key_wait` or a frame-bearing beat, so this asymmetry does not currently cause a dropped or flashed message — it is a real, evidenced structural gap, but not (on current evidence) the cause of the reported "collapsed" feel.

**Decision: no code change.** Per instruction, a defect is only fixed once confirmed with reference evidence; there is none here to fix against, and inventing a floor value or retuning `kPresentationUnitMs` without footage would be exactly the kind of unevidenced change the batch discipline rules out. The §4 table entry and the recommendation column now record what was actually learned, in place of the original "no cause has been investigated" note, so the next pass starts from real findings instead of a guess.

#### Y-33 — Vas Rel Por phase-gate

Traced `CAST.OVL 0x0cf0`'s full instruction sequence (`re/notes/vas-rel-por-341-derivacion.md`) against native's handling in `world_magic.cpp` (item 46 excluded from the generic ceremony but nothing raised the phase-specific one), `commands.cpp` (the phase/ship/bounds gate and `moonstone_teleport` call already existed, correctly, but nothing ever fed it a real phase — `Command.hours` defaulted to 0, a *valid* phase), and `dungeon_orchestration.cpp` (no `MagicEffect::Gate` handling at all). `game/src/main.ts`'s `castGateTravel` served as the byte-faithful, already-tested template.

**Fix, mirroring the reference exactly:**
1. `magic.cpp`'s `cast_target_prompt()` gained `CastTargetPrompt::WorldPhase` for `MagicEffect::Gate`, gated on a new `aboard_ship` parameter — the ship check (`CAST.OVL 0x0cf6`) fires *before* the "To phase:" print (`0x0cff`) in the reference, so the prompt itself must not open at sea, not merely fail after opening.
2. `ui_session.cpp`'s `TargetSelection` mode gained a `Character` arm for the new `UiRequestId::GatePhase` request: exactly `'1'-'8'` sets `cmd.hours` to the phase (0-7); any other key — and Cancel, via the pre-existing `cast` arm — leaves the `-1` sentinel the request is armed with.
3. `world_magic.cpp`'s `Gate` branch now raises the ceremony (literal index 8, `CAST.OVL 0x0d2d`) only when `cmd.hours` is a valid phase (0-7) **and** the party is not aboard ship — checked independently of the UI decision, not trusted to it, because a caller that supplies `hours` directly (the parity fixture) must reach the same answer regardless of entry path.
4. `commands.cpp`'s existing `phase<0`/ship/bounds gate and `moonstone_teleport` call needed **no change** — they were already correct and were simply never being fed a real phase before.
5. `native/targets/tdeck/main/alpha_runtime.cpp`'s `cast_selected_spell` now sets `c.hours=-1` unconditionally before any dispatch decision (not only inside the new `WorldPhase` case), closing a self-found gap where the aboard-ship path (which returns `CastTargetPrompt::None`, skipping the new case entirely) would otherwise have fallen through to `command(c)` with the stale default `hours=0` and fired the ceremony on a cast the ship should silently fail.
6. `dungeon_orchestration.cpp` needed **no** change, proven rather than assumed: Vas Rel Por's `time_bits` (14 = peace-only) excludes both combat and dungeon (`location>=128` shares bit 1 with combat), so `cast_spell()`'s own generic time-window gate already rejects it underground with "Not here!" before any Gate-specific code runs (control test C11).
7. `native/core/tools/check-gameplay.ts` (native's own copy of the parity fixture, not the `game/` port) was missing the ceremony cue in its `gateTravel` branch entirely — imported `VAS_REL_POR_PHASE_CEREMONY_INDEX` from `game/src/core/magic/ceremony.ts` and added `cue(...)` gated on the identical `hours∈[0,7] && !aboard_ship` condition used in native.

**RED → GREEN, in two rounds.** Round one: new tests D1-D4 (`cast_target_prompt` policy), E1-E3 (`UiSession` dispatch), C9-C11 (`world_magic` ceremony gating + dungeon control), G1-G2 (full `execute_command()` integration, including an actual moonstone teleport and its silent-success assertion) — all **RED** against unmodified production code, **GREEN** after the fix (67/67). Three mutations (ceremony fired unconditionally; the ship-check ternary inverted; the digit range widened to accept `'9'`) each caught the intended assertion and were reverted. Round two: running the **authoritative** `gameplay_parity` suite (not just the new unit tests) surfaced a real bug the unit tests had missed — the ceremony condition checked `cmd.hours>=0` but no upper bound, so `hours=8` (out of the valid 0-7 range, corresponding to a key that would have failed the reference's own `'1'..'8'` gate) still fired the ceremony. Caught by `check-gameplay.ts`'s pre-existing line-130 fixture scenario (`hours` ∈ {-1,0,3,7,8} × six transport tiles), mismatch 4514. Added test C10c pinned it **RED**, the upper-bound check (`cmd.hours<=7`) fixed it **GREEN**, and `gameplay_parity` was re-run to confirm (69/69 on `batch5`). This is exactly what the authoritative-suite-after-unit-tests step in the batch discipline is for.

#### Files changed

**Production:** `native/targets/tdeck/main/ui_input_adapter.cpp` (Y-29), `native/core/src/ui_session.cpp` (Y-31, Y-33), `native/core/include/openu5/ui_session.h` (Y-33: `UiRequestId::GatePhase`), `native/core/include/openu5/magic.h` (Y-33: `CastTargetPrompt::WorldPhase`), `native/core/src/magic.cpp` (Y-33), `native/core/src/world_magic.cpp` (Y-33), `native/targets/tdeck/main/alpha_runtime.cpp` (Y-33).

**Test/harness:** `native/targets/tdeck/host_tests/input_test.cpp` (Y-29, corrected — was asserting the old behaviour), `native/core/tests/batch5_test.cpp` (Y-31 B5/B6/C7/C8; Y-33 D1-D4/E1-E3/C9-C11/G1-G2), `native/core/tools/check-gameplay.ts` (Y-33 fixture gap, corrected).

**Audit:** this document — §2 (three rows), §4 (all four IDs), §5 (`SetActivePlayer`), §11 (Scrolls row), this section.

#### What this batch did NOT do

- It did not touch `quest_parity`'s MinGW crash (Batch 17's scope).
- It did not attempt a Y-32 fix. No reference timing evidence exists to fix it against; inventing numbers was explicitly out of scope for this batch's discipline.
- It did not change the resource pack, so the flashed SD card needs no repack.
- It did not touch Blackthorn's ceremony/roster code at all (R-23 and the broader capture-scene presentation are unrelated to any of this batch's four findings, despite Y-32's superficial adjacency).

**Host suite:** **84 total, 83 pass, 1 fail** — no change in registered-test count (no new `add_test` target; all new coverage extended existing `batch5`/`input_regression`), and the **only** failure is the same pre-existing baseline: `quest_parity` (`Native driver failed: 3221225477`, `STATUS_ACCESS_VIOLATION`, the MinGW/w64devkit finding from Batch 8), present identically in this batch's own pre-edit baseline run. `gameplay_parity` is **GREEN** (confirmed after the C10c upper-bound fix and a driver rebuild). `batch5` grew from 51 to 69 checks, all passing. Zero project-level compiler warnings in the host build.

**Firmware:** `idf.py -B build-batch16 build` **PASS**. `openu5_tdeck.bin` = `0xd1b10` = **858,896 bytes**, **+320** over Batch 15's `0xd19d0` (858,576), **189,680 bytes (18%) free** of the `0x100000` app partition, **zero project warnings** (the five in the log are the same pre-existing ESP-IDF `component_validation.cmake` notices as every prior batch). Hardware flash not performed. The resource pack is unchanged; the SD card needs no repack.

**Hardware-only validation still owed (device checklist, not performed in this session):**
1. Physical Symbol+Mic/0 press at an exploration prompt actually clears the active character (Y-29) — press-and-release, not held past the 1.1s Movement-Mode threshold.
2. A plain (unmodified) Mic/0 short press still cancels, and a 1.1s+ hold still toggles Movement Mode — confirming the fix changed nothing about the existing physical contract.
3. Casting Vas Rel Por on-device: the "To phase:" prompt appears, a digit key `1`-`8` teleports silently (no "Success!"/"Failed!" banner), any other key (including the device's own Cancel/ESC affordance) fails silently, and the prompt does not appear at all while aboard a ship or skiff (Y-33).
4. Using the Rel Hur scroll and the skull key on-device, cancelling the direction prompt each time, and confirming the item count actually decrements (Y-31) — the host suite proves the dispatch, but the visible inventory count is a device-only observation.

---

### Batch 17 — `quest_parity`'s `STATUS_ACCESS_VIOLATION` adjudicated · risk: low, one-seam scope · **GREEN — HOST-HARNESS DEFECT (UNDEFINED BEHAVIOUR), FIXED; NO PRODUCTION CODE AND NO PARITY SEMANTICS CHANGED**

From Batch 8 to Batch 16, every batch write-up in this section carried the same
line: `quest_parity` fails with `STATUS_ACCESS_VIOLATION` / exit `3221225477`,
"a MinGW/w64devkit environment finding", compared against and set aside as
baseline noise. It was never noise, it was never environmental in the sense that
word implied, and it was never a production defect. It was one line of the host
harness invoking undefined behaviour, and it reproduced 100 % of the time.

**The finding is registered as Y-34.**

#### What the crash actually was

`quest_parity` is `node --import tsx tools/check-quests.ts <quest_driver.exe>`:
the TypeScript reference generates 5377 cases, the native `quest_driver.exe`
replays them, and the two projections are compared. The driver died on **input
line 3452** — `{"op":"theft","here":0,"seed":20}` with `keys = 2` — after
writing 3450 output lines. Isolated, that single line crashes on its own, which
is what made the bisect immediate once the run was looked at instead of
compared.

That case is one of exactly **two** of the 1152 `theft` cases in which
Faulinei's theft loop **does not terminate**. TALK.OVL `0x11c7` is rejection
sampling: `rand(0,2)` picks keys/gems/torches and the three `je` at
`0x11e7`/`0x11fd`/`0x1209` jump **backwards** to `0x11c7` when the drawn
category is empty (`re/notes/shadowlord-urbano-acta.md` §2.3). The original
bounds nothing, and neither does the port — correctly. Against the port's own
deterministic `OriginalRng`, whose stream replaces the original's
`srand(rng_time_hash())` at `0x11AB` (a **declared parity ceiling**,
`re/notes/rng.md` "Techos de paridad"), seed 20 orbits a cycle that never yields
the stocked index. Both parity sides therefore *observe* the non-termination
under a draw budget instead of hanging: `check-quests.ts` throws after 65536
draws, and `quest_driver.cpp` used `setjmp`/`longjmp`.

The `longjmp` is the whole bug. On `x86_64-w64-mingw32` with `__SEH__`,
`<setjmp.h>` expands `setjmp(b)` to `_setjmp(b, __builtin_frame_address(0))`
whenever `__builtin_sponentry` is unavailable — and GCC 16, this toolchain's
compiler, no longer provides that builtin (verified by preprocessing: `c++ -E`
yields exactly that expansion, and `__has_builtin(__builtin_sponentry)` is
false). GCC's frame base is **not** the Win64 SEH establisher frame.
Instrumenting the driver with `RtlLookupFunctionEntry`/`RtlVirtualUnwind` at the
`longjmp` site measured both numbers directly:

| build | `jmp_buf.Frame` (the `longjmp` TargetFrame) | `main`'s SEH establisher frame | result |
|---|---|---|---|
| `-O2` | `0x5AD89FFD30` | `0x5AD89F62A0` | **no match** — off by `0x9A90`, `main`'s ~39 KB frame |
| `-O0` | `0xF8807FFE40` | `0xF8807FFE40` | **exact match**, unwind stops, watchdog works |

`msvcrt!longjmp` hands the mismatched value to `ntdll!RtlUnwindEx`, nothing ever
matches it, and the unwinder walks past `main`, past the CRT startup frames and
off the top of the thread stack. A vectored exception handler caught the first
fault precisely:

```
[B17] FIRST FAULT code=0xC0000005 addr=00007fffe3d95ae1 access=read target=0x8965009478
[B17] stack limit=0000008964ff0000 base=0000008965000000 rsp=0000008964ff50b0
[B17] faulting module: ntdll.dll+0x15AE1
[B17] target query=48 state=0x2000 protect=0x0          <- MEM_RESERVE, PAGE_NOACCESS
[B17]   frame[7]=ntdll.dll+0xCC67D                      <- RtlUnwindEx
[B17]   frame[8]=msvcrt.dll+0x7ADBB                     <- longjmp
[B17]   frame[9]=quest_driver.exe+0x1680                <- quest_driver.cpp:225
[B17]  frame[10]=quest_driver.exe+0xB9AD                <- openu5::Rand::operator() (turn.h:24)
[B17]  frame[11]=quest_driver.exe+0x1540E0              <- main (quest_driver.cpp:228)
```

The read target `0x8965009478` is `0x9478` **above** `StackBase`, in reserved
uncommitted pages; the access violation is then raised *inside* exception
dispatch and recurses until the process dies. That recursion is why the process
reported a bare `0xC0000005` with no usable diagnostic, and why eight batches
read it as environment flakiness.

**And it is why `-O0` "worked":** at `-O0` GCC keeps a frame pointer, the two
values coincide exactly, and the identical source survives. The defect is
optimisation-sensitive, not environment-sensitive — the "it doesn't crash under
`build-zig`" observation in Batch 8's write-up was the same coincidence wearing
a different hat, and the crash also vanishes under `gdb`, which was the final
reason nobody got a stack trace.

#### Classification

**Host-test defect, and specifically undefined behaviour.** [csetjmp.syn]/2
already forbids `longjmp` wherever replacing it with `throw`/`catch` would run a
non-trivial destructor; the harness carried a comment asserting the C++-level
side of that condition was satisfied ("theft has no C++ objects requiring
unwinding") and it was — but the Win64 SEH ABI breaks for a *different* reason
the comment never considered. Not a production defect, not corrupted fixture
data, and not a platform issue in the sense of "this host is broken": the same
harness would be equally wrong on any toolchain whose `setjmp` records GCC's
frame base.

#### The evidence that there is no production defect behind it

Before anything was changed, `quest_driver` was rebuilt at `-O0` — an
independent build where the `longjmp` happens to work — and the **full
`quest_parity` comparison was run to completion**: **5377 of 5377 cases match
the TypeScript reference**, `nonterminatingObservations: 2`, every coverage
counter as expected. So the eight-batch crash was hiding **zero** parity
divergence. `apply_faulinei_theft` is byte-faithful, the unbounded re-roll is
correct and stays unbounded, and **no parity expectation was weakened, no
assertion relaxed, no test skipped and no platform excluded**.

#### The fix

The `setjmp`/`longjmp` escape was lifted out of `main` into a shared test-support
seam, `native/core/tests/quest_theft_watchdog.h`, and re-expressed as a C++
exception — the same escape the TypeScript side uses. `apply_faulinei_theft`
holds only trivially destructible locals, so the unwind is well defined, and the
`try`/`catch` sits one frame below it. `quest_driver.cpp` now calls that seam
instead of open-coding a watchdog in `main`.

The seam also records whether the escape **unwound**: the tripping path parks a
scope guard in the callback frame, which a real unwind destroys and a jump would
skip. That turns "do not reach for `longjmp` here again" from a comment into an
assertion (group D below), so the next person who tries it gets a clean red test
instead of a 0xC0000005 eight batches later.

**Nothing in `native/core/src` or `native/core/include` was touched.** The
firmware is byte-identical.

#### Regression coverage — `batch17_theft_watchdog` (new, 33 checks)

Drives the same `openu5_test::observe_faulinei_theft()` seam `quest_driver.cpp`
itself calls, so the contract is pinned outside the 5377-case replay:

- **Group A** — the two reference non-terminating cases (input lines 3452 and
  3484) trip the watchdog, the tripping draw is the 65537th, the loot is empty,
  the stocked categories and gold are untouched, and the RNG orbit from seed 20
  closes back on 20 exactly as the reference records. Canaries prove control
  returned to the caller's frame and a second observation ran after the first.
- **Group B** — terminating observations are indistinguishable from an unwatched
  call: the keys branch still applies its floor-0 decrement, the `0x1187` gate
  consumes zero draws for `here != 0`, the `0x1210`/`0x1232`/`0x124a` linear
  cascade consumes zero draws and takes the highest non-empty slot, and the gold
  branch is exactly one `rand(1,15)` clamped at 0.
- **Group C** — re-walks the fixture's own `4 × 9 × 32` theft axis (1152 cases)
  and pins the non-terminating set **exactly** to the reference's two, so "the
  loop now always terminates" and "the loop now never terminates" both turn this
  red.
- **Group D** — the escape must unwind, not jump.

#### RED → GREEN

| stage | result |
|---|---|
| **Baseline** (Batch 16 tree, clean configure+build) | 84 total, 83 pass, **1 fail** — `quest_parity`, `Native driver failed: 3221225477` |
| **RED** — new test against the unmodified `setjmp`/`longjmp` escape | **exit 1, 33 checks, 2 failures**: group D1 ×2, *"the tripped watchdog unwinds the callback frame instead of jumping out of it"* — a clean assertion failure, not a crash |
| **GREEN** — same test against the exception escape | **exit 0, 33 checks, 0 failures** |
| `quest_driver.exe` on the two watchdog cases, directly | exit 0, both `{"nonterminating":true,"seed":20}`, inventory untouched |
| `quest_parity` alone | **PASS** (4.86 s) — first time in this project's recorded history |

#### Mutation check (false-green audit)

| mutation | expected to break | measured |
|---|---|---|
| **M1** — `quest.cpp`: replace the `0x11c7` re-roll with "fall through to the next stocked category" (the plausible "cleanup") | the non-terminating set and the watchdog | **9 of 33 failed** — A1 ×3, A2, A4 ×2, C1, D1 ×2 |
| **M2** — `quest.cpp`: widen the `0x1187` gate from `here != 0` to `here < 0` | the gate and the case census | **3 of 33 failed** — B2 (`here=1`, `here=2`), C1 |
| **M3** — restore the `setjmp`/`longjmp` escape | the unwind contract | **2 of 33 failed** — D1 ×2 (this is the RED row above) |

All three mutations were reverted; `quest.cpp` is byte-identical to `HEAD` and
the suite re-verified green afterwards.

**Host suite:** **85 total, 85 pass, 0 fail.** One registered test up from Batch
16's 84 (the new `batch17_theft_watchdog`), and `quest_parity` moved
**fail → pass**. No other result changed. **This is the first fully green host
suite in the audit's recorded history** — the "two known baseline failures"
convention that ran from Batch 8 through Batch 16 (`gameplay_parity` mismatch
2034, fixed in Batch 13; `quest_parity` `3221225477`, fixed here) is now retired.
Any failure in a future batch is a real one.

**Firmware:** `idf.py -B build-batch17 build` **PASS**. `openu5_tdeck.bin` =
`0xd1b10` = **858,896 bytes**, **±0 bytes** against Batch 16 — byte-identical, as
expected for a change confined to host test sources. `0x2e4f0` (189,680 bytes,
18 %) free of the `0x100000` app partition. The resource pack is unchanged; the
SD card needs no repack.

**Hardware:** **no device step is owed.** The change is confined to
`native/core/tests/` and `native/core/CMakeLists.txt`; no production translation
unit is touched, and the firmware binary is byte-identical to Batch 16's. There
is nothing on-device that could observe this batch.

**Files changed:** `native/core/tests/quest_theft_watchdog.h` (new seam),
`native/core/tests/batch17_theft_watchdog_test.cpp` (new test),
`native/core/tests/quest_driver.cpp` (calls the seam; `<csetjmp>` dropped),
`native/core/CMakeLists.txt` (registers the test).

**Audit:** this document — §14 (this section), §16 (Phase 6J). The Batch 8 and
Batch 10 write-ups' "environment finding" framing is superseded by this section
rather than rewritten, so the historical record of how the failure was read at
the time is preserved.

#### What this batch did NOT do

- It did not touch production code. Not one line under `native/core/src` or
  `native/core/include`.
- It did not change any parity expectation, weaken an assertion, skip a test,
  suppress a process error, catch an access violation or exclude a platform.
- It did not "fix" the unbounded theft re-roll. That loop is the original's
  (TALK.OVL `0x11c7`) and stays unbounded on both parity sides; only the
  observation of it is bounded, and group C now guards that.
- It did not start the comprehensive project audit or any Batch 18 work.

---

### Batch 18 — Comprehensive Alpha 2 audit and reconciliation · risk: low (audit batch; two small production fixes) · **GREEN — HOST-CLEAN; TWO CONFIRMED NATIVE DEFECTS FIXED; ONE DEFERRED WITH A SCOPED FOLLOW-UP**

**IDs:** R-26 (**GREEN — RESOLVED**), R-34 (**new, GREEN — RESOLVED**), R-25 (**OPEN — re-adjudicated, severity raised, deliberately deferred**). Plus four stale entries in this document corrected and four persistence axes reconciled.

This is the first whole-project audit since the parity, dungeon, magic, UI, Blackthorn, save/load and harness-cleanup batches. Its deliverable is not code — it is an authoritative, contradiction-free answer to *what remains before Alpha 2 can be declared complete*. Two new sibling documents carry the parts that were previously scattered:

* **[`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md)** — one deduplicated 140-row device list, executable in a single session. §16 below stays as the archaeology of which batch owed which step.
* **[`ALPHA2_PRESERVATION_LEDGER.md`](ALPHA2_PRESERVATION_LEDGER.md)** — every knowing divergence from the reference, split into T-Deck adaptations, deliberate enhancements, and unresolved divergences, with a toggle-feasibility assessment for a future preservation profile. **No enhancement was reverted and no toggle was implemented.**

---

#### 1. Baseline

Measured from a **clean** `native/core/build-batch18` on the unmodified Batch 17 tree, before any edit:

| | |
|---|---|
| HEAD at entry | `83b2ed56` — *Batch 17: adjudicate and fix the quest_parity host crash (Y-34)* |
| Latest tag at entry | `alpha2-batch17-quest-parity-crash` |
| Host suite | **85 total · 85 pass · 0 fail · 0 skipped** |
| `gameplay_parity` | PASS — 5,058 sequences |
| `quest_parity` | PASS — 5,377 cases, `nonterminatingObservations: 2` (the two reference non-terminating theft cases, expected) |
| Project compiler warnings | **0** |

**Batch 17's exit state, verified from git and source rather than from its own prose.** The commit touches exactly five files, four of them host build inputs (`tests/quest_theft_watchdog.h`, `tests/batch17_theft_watchdog_test.cpp`, `tests/quest_driver.cpp`, `CMakeLists.txt`) plus this document. No translation unit under `native/core/src`, `native/core/include` or `native/targets/tdeck/main` was modified — confirmed by `git show --stat`. The classification stands: the former `quest_parity` `STATUS_ACCESS_VIOLATION` was **host-harness undefined behaviour** (a `setjmp`/`longjmp` escape invalid against the Win64 SEH ABI), not production code, not parity semantics, and not environment flakiness. Batch 17 did build firmware and reported it byte-identical to Batch 16 at `0xd1b10`; Batch 18's own baseline size measurement agrees.

**One non-project warning exists and is recorded so it is not rediscovered as a finding.** The clean baseline build emits exactly one `-Wstringop-overflow` from GCC 16, inside `<bits/stl_uninitialized.h>`, on a `std::vector::insert` inlined from `native/core/tests/command_parity_test.cpp:240`. It is test-harness code, a known false-positive shape for that diagnostic, and absent from every translation unit that ships.

---

#### 2. Items closed during reconciliation

| ID | Was | Now | Evidence |
|---|---|---|---|
| **R-26** | RED — OPEN since Batch 4 | **GREEN — RESOLVED** | `batch18_well_ceremony` 19/19; RED 16/5 first; 4 mutations caught |
| **R-34** | not known to exist | **GREEN — RESOLVED** (found while fixing R-26) | same target, groups W4/W6; RED on two independent chains |

**R-26 — the wishing well.** Four divergences, all in the T-Deck glue; the core arm (`look.cpp:37`) was byte-correct throughout and `gameplay_parity` had always passed it. (1) the "No" answer dispatched nothing, so `dropCoin(false)`'s `"No"` echo never printed; (2) ESC was ignored although the reference prompt type is `yesno-esc`; (3) the object description `"a well."` (DS `0x720c`) was missing, although the fountain arm four lines away already does exactly this for DS `0x729c`; (4) the wish row printed a fabricated `"What dost thou wish?"` instead of DS `0x722c` `"Thy wish?"`. Full derivation in §3 R-26.

**R-34 — chained modals lost their prompt.** `UiSession::finish_modal()` cleared `prompt_`/`input_` **after** dispatching the answer, and an answer may arm the next modal synchronously inside that dispatch. The new modal's `mode_` survived and its prompt did not. This was **never well-specific**: the Rel Hur/skull-key `Direction?` row, the `(U)se` potion `On whom?` picker, the shrine `Virtue?`/`Mantra?` pair, the equipment picker, and — worst — Blackthorn's `Your response?` row, the very line Batch 4 added for prompt fidelity, were all rendering with a blank prompt. `cancel_modal()` never had the bug; it clears before dispatching. Fixed with one guard on `is_modal(mode_)`, which is exact here because `enter_modal()` only ever records a non-modal `return_mode_`. Full derivation in §3 R-34.

---

#### 3. Stale entries corrected

Four statements in this document were **true when written and false now**, and had survived between four and nine batches past their cause:

| Location | Stale claim | Corrected to |
|---|---|---|
| §5 `CommandKind` table | `BeginConversation` — "Dead — blocked by R-10" | **Live.** R-10 was fixed in Batch 4; producers at `ui_session.cpp:1228` and `alpha_runtime.cpp:1064` |
| §5 `CommandKind` table | `BlackthornAction` — "Dead — blocked by R-09" | **Live.** R-09 was fixed in Batch 4; producers at `ui_session.cpp:499/504/509/514` |
| §5 `DungeonAction` | "`TurnAround` and `Drink` have no producer" | **Both live since Batch 9B** — `:866`/`:911` and `:479`/`:906` |
| §7 "Currently failing" | a chain of superseded per-batch tallies ending at "71 total, 69 pass, 2 fail" | **86 / 86 / 0**, with the retired-convention warning at the top of the section |

**Re-verified and still accurate**, so left alone: `ShopAction`, `CombatEscape` and `Unready` are still handled with no producer; `render_active_view()` still has no caller and is still linked into the firmware (confirmed in `build/openu5_tdeck.map`).

---

#### 3b. The full reconciliation table

Every R- and Y- identifier in this document, classified in exactly one bucket, with the concrete evidence behind the classification. Built by re-reading the current source and re-running the suite, **not** by copying prior status lines — which is how §3b caught the four stale entries in §3a above.

Legend: **CH** = closed, host-verified · **CD** = closed, verified on hardware · **IH** = implemented, hardware verification still required · **OD** = open, real production defect · **OM** = open, missing feature/behaviour · **OE** = open, insufficient reference evidence · **INT** = intentional enhancement/divergence · **STALE** = superseded by later work · **OOS** = out of Alpha 2 scope.

| ID | Class | Evidence | Device row |
|---|---|---|---|
| R-01 shop/dialogue mode destroyed | **IH** | `ui_mode_regression` 28/28 via `ui_mode_policy.h` (Batch 1) | H-26, H-28, H-31 |
| R-02 chest renders as deep water | **IH** | `presentation_regression` R03-OUTDOOR (Batch 2) | H-37 |
| R-03 arena chests promoted to world objects | **IH** | `gameplay_parity` mismatch 59 GREEN; `combat_loot_open_regression` R03-DIRECT | H-39 |
| R-04 quest layer overdraws loot | **IH** | `presentation_regression` R04-OVERLAY, R04-LIFO | H-38 |
| R-05 dungeon presentation | **IH** | `dungeon_view_regression` (logic, Batch 9); `typescript_dungeon_art_identity` (art, Batch 9C) | H-73 – H-84 |
| R-06 Ready rejected in combat/dungeon | **IH** | `batch3_group_c` drives the real `execute_command()` in all three contexts | H-34, H-111 |
| R-07 Grapple bound to id 18 | **IH** | `batch3_group_b` over `usable_item_picker_rows()` | H-41 |
| R-08 endgame (U)se chain unreachable | **IH** | same seam; every owned id gated by its authoritative owner | H-41, H-118 |
| R-09 five modal responses discarded | **IH** | `batch4_group_a` 32/32 (real trigger + real answer per family) | H-31, H-120 |
| R-10 NPC-initiated Talk/Shop | **IH** | `batch4_group_b` 12/12 | H-25, H-26 |
| R-11 world spells consume then do nothing | **CD** | `batch5` + **all three checks passed on the physical T-Deck, first attempt (Batch 5)** | **H-48 PASS** |
| R-12 `MapReveal` has no renderer | **IH** | `presentation_regression` reveal window (Batch 7) | H-51 |
| R-13 `Zodiac` has no renderer | **IH** | Batch 7; the 9 px clipping residual is ledger D-12 | H-52 |
| R-14 world objects never serialized | **IH** | `persistence_parity`; sidecar round-trip (Batch 6) | H-40, H-124 |
| R-15 `DungeonState` not serialized | **IH** | `gameplay_integration_test.cpp:87` round-trips `capture_dungeon` | H-115 |
| R-16 spell metadata contradictions | **CH** | `display_names` 11 semantic assertions; `magic_parity` **25,088 cases byte-identical before and after**, proving no tracked field moved. Pure metadata — nothing to observe on glass | none owed |
| R-16 residual: the TypeScript port still implements the disproven In Ex Por no-op | **OOS** | touches `game/src`, not the native port (`content-audit.md` PENDIENTE(3)) | n/a |
| R-17 View Gem presentation fabricated | **IH** | `gem_view` host tests + `gem_category_table_drift` (Batch 10) | H-49 |
| R-18 `pre_combat_mode_` stale register | **CH** | `ui_mode_regression`; the three return paths are separately owned and asserted | covered by H-35 |
| R-19 ships cannot sail | **IH** | `batch3_group_a` A1/A2 | H-70 |
| R-20 harpsichord unreachable | **IH** | `batch3_group_a` A3 | **H-125** (added in Batch 18 — it had no row) |
| R-21 fabricated (U)se echo | **IH** | `gameplay_parity` mismatch 2034 GREEN (Batch 13) | H-42 – H-47 |
| R-22 Z-stats pages missing | **IH** | `batch14_zstats_model` 97/0, `batch14_zstats_runtime` 54/0 | H-54 – H-69 |
| R-23 sacrifice roster compaction | **CH** | `batch15_sacrifice_roster` 75/0 + 4 mutations. **No hardware step is owed**: every `.GAM` yields 16 records, at which count the old and reference bounds are the same expression | none owed, by proof |
| R-24 `DebugPreset::Combat` engaged Set Active Player | **IH** | pre-Batch-4 Developer cleanup | **H-131** (added in Batch 18) |
| **R-25 crystal ball always rejected** | **IH** | **CLOSED in Batch 19.** `openu5::resolve_command_char()` (kernel `0x4988`) is now a shared core seam; the crystal ball, `(S)earch` and `(C)ast` all resolve through it. RED 24/49 on the production-routing suite → GREEN 49/49, four mutations caught | **H-50** (rewritten to the fixed expectation) + **H-141 – H-145** |
| R-26 well "No" / ESC | **CH** *(+ IH for the visuals)* | `batch18_well_ceremony` 19/0, RED 16/5 first, 4 mutations | H-17 – H-22 |
| R-27 Developer Default Entrance ordinal vs signed z | **IH** | Batch 4.5A-1 | **H-132** (added in Batch 18) |
| R-28 Developer UI raw ordinals | **IH** | Batch 4.5A-2 | **H-133** (added in Batch 18) |
| R-29 Developer Special/Quest Items unreachable | **IH** | Batch 4.5A-3 | **H-133** (added in Batch 18) |
| R-30 Developer presets opaque | **IH** | Batch 4.5A-4 | **H-134** (added in Batch 18) |
| R-31 transcript auto-scroll defeated review | **IH** | Batch 4.5C; `preserve_scroll_on_growth()`. Also ledger **E-1**, a deliberate enhancement | **H-130** (added in Batch 18) |
| R-32 Blackthorn scene has no distinct presentation | **IH** | Batch 4.5D; `blackthorn_scene_test.cpp`. Its *pacing* is a separate, open question — Y-32 | H-120 |
| R-33 basement search floor byte 255 vs −1 | **IH** | Batch 4.5C.1 | H-30 |
| R-34 `finish_modal()` wipes chained prompts | **CH** *(+ IH for the visuals)* | `batch18_well_ceremony` W4/W6 on two independent chains; mutation M1 | H-23 |
| Y-01 WASD in frontend menus | **OM** | product decision, never taken. Ledger **D-1** | n/a until decided |
| Y-02 trackball centre click unbound | **OM** | GPIO 0 not wired. Ledger **D-2** | n/a until decided |
| Y-03 audio | **OOS** | declared out of Alpha 2 scope; `Sfx` unconsumed, volume settings inert. Ledger **D-3** | n/a |
| Y-04 feedback VFX | **IH** | all six channels consumed (Batches 7/7B); pacers and `WorldFxLayer` host-tested, the pixel work is not | H-33, **H-135 – H-138** (five of six channels had no row before Batch 18) |
| Y-05 `AlphaRuntime` has no host coverage | **STALE** | superseded by Batches 11, 14 and 18: three targets now link the real, unmodified `alpha_runtime.cpp` and drive it through `UiInputAdapter` → `UiSession` → `dispatch()`. The gap is narrowed to the pixel layer, which is Y-07 | n/a |
| Y-06 device smoke tests are data-presence probes | **OM** | still true; 37 scenarios, none validates a rendered frame or a full route. Superseded *in purpose* by the three AlphaRuntime targets | n/a |
| Y-07 broad device presentation unproven | **IH** | this is the umbrella for the whole checklist | all 136 UNTESTED rows |
| Y-14 gem view overdrawn by the strips | **IH** | resolved with R-17 via `full_square_viewport` (Batch 10) | H-49 |
| Y-15 Mix unreachable in dungeons | **OE** | no derivation exists for the dungeon dispatcher's `m` slot; native's alias is a declared shared affordance. Ledger **A-7**/**D-4** | n/a until derived |
| Y-17 Acknowledgements is one line | **OM** | low priority. Ledger **D-5** | n/a |
| Y-18 developer entry from the frontend | **IH** | enters on whatever `INIT.GAM` booted; confirmed intended | **H-129** (added in Batch 18) |
| Y-19 Mix quantity hard-coded to 1 | **OM** | needs a numeric modal. Ledger **D-6** | n/a |
| Y-20 `SetActivePlayer` unreachable | **IH** | `batch3_group_a`; digits carry the literal character | **H-126** (added in Batch 18) |
| Y-21 Rel Hur scroll claim | **STALE** | the claim was false when written; discharged in Batch 5 by direct reading of `alpha_runtime.cpp:684` | n/a |
| Y-22 dungeon→combat→dungeon return | **IH** | `dungeon_flow_parity` at core level | H-113 |
| Y-23 dungeon keyboard movement needs Movement Mode | **INT** | deliberate contract, now stated. Ledger **D-7** | H-107 exercises it |
| Y-24 word-of-power Yell | **CH** | `batch3_group_a` A3 | H-119 |
| Y-25 shrines | **IH** | mode lifecycle repaired in Batch 1; no known mode defect | H-31 |
| Y-26 beds / auto-sleep | **CH** | confirmed core-internal (`commands.cpp:1163` via `townAutoSleepTurn`), which was the question | **H-128** (added in Batch 18, opportunistic) |
| Y-27 System Menu over an open modal | **IH** | handled before gameplay routing and does not touch `ui_->mode()` | **H-127** (added in Batch 18) |
| Y-28 combat Ready picker close timing | **OE** | two presentation gaps, never adjudicated; no action-cost effect. Ledger **D-8** | **H-139, H-140** (record, do not file) |
| Y-29 Symbol+Mic/`0` | **IH** | `input_test.cpp` corrected from pinning the old behaviour, then RED→GREEN + mutation (Batch 16) | H-10, H-11 |
| Y-30 world cast prompt/consumption ordering | **OE** | a consequence of the fused `world_magic()`; no tracked-field divergence demonstrated. Ledger **D-9** | n/a |
| Y-31 Rel Hur / skull-key cancel refund | **IH** | `batch5` B5/B6 RED→GREEN, C7/C8 controls, mutation (Batch 16) | H-15, H-16 |
| Y-32 scripted-event pacing | **OE** | **no footage-derived timing evidence exists in this repository.** Batch 16 proved that by archaeology and correctly refused to invent a number. Ledger **D-10** | H-122 (record video if possible — that recording *is* the missing evidence) |
| Y-33 Vas Rel Por phase gate | **IH** | `batch5` D1-D4/E1-E3/C9-C11/G1-G2 + C10c (found by `gameplay_parity`, not by the unit tests), 3 mutations | H-12 – H-14 |
| Y-34 `quest_parity` access violation | **CH** | `batch17_theft_watchdog` 33/0, RED 2, 3 mutations; **harness-only, firmware byte-identical, so no device step exists to owe** | none owed, by proof |

**Totals:** 2 **CH**-only closures this batch plus 8 pre-existing **CH**; **1 CD** (R-11, the only hardware-verified finding in the project); **32 IH**; **1 OD** (R-25); **5 OM**; **5 OE**; **1 INT**; **2 STALE**; **2 OOS**.

**The single most important number in this table is CD = 1.** Thirty-two findings are implemented and believed correct on host evidence alone. That is why the hardware checklist, not the code, is the Alpha 2 blocker.

---

#### 4. Unresolved production defects

**One.** R-25 — the crystal ball.

Re-adjudicated against the current tree rather than the Batch 4 note, and its severity raised from "unassessed" to **2**: the feature is 100 % non-functional on device. `alpha_runtime.cpp:888` dispatches `CommandKind::CrystalBall` with `Command::member` never set, and `look.cpp:39` rejects any member outside `[0, character_count)` — zero events, zero HP change, every time. On top of that the `"Peer into it?"` yes/no is a native fabrication: `LOOKOBJ cmd_look 0x09ea` case `0x29` raises no prompt at all, it calls the command-character picker (kernel `0x4988`) directly.

**Deferred on purpose, against the batch's own fix policy.** The faithful fix is not "pass the active member". `pickCommandChar` is a four-branch resolver (active → direct; one eligible → auto; zero → `"None!"`; two or more → the `"Player: "` prompt with a `"Disabled!"` re-ask loop), native implements none of it, and the same kernel routine drives `(S)earch`'s trap-check perceiver and `(C)ast`'s caster. Three systems, architectural, and it moves an RNG-adjacent perceiver — exactly the "defer instead" profile. The scoped four-step follow-up is written out in §3 R-25, and device row H-50 records the current broken behaviour so a future fix has an observation to flip.

---

#### 5. Unresolved reference questions

| Question | Why it is open | ID |
|---|---|---|
| Is Mix legal underground? Native aliases dungeon `(M)` to Cast | no derivation attempted for the dungeon dispatcher's `m` slot | Y-15 |
| What is the true per-tick pause for the Blackthorn capture scene? | **no footage-derived timing evidence exists anywhere in the repository.** `re/notes/blackthorn-escena-324.md` §4 explicitly disclaims a witness for this scene, and the TypeScript `PAUSE_UNIT_MS` is an admitted reuse of TrollSneak's calibration. Batch 16 confirmed this by direct archaeology and correctly refused to invent a number | Y-32 |
| Should the world cast consume before prompting, as the reference does, rather than prompt before the fused `world_magic()` call? | the ordering is a consequence of the fused call, not an oversight; no divergence has been demonstrated in a tracked field | Y-30 |
| Does the empty-handed combat Ready picker charge without opening in the reference, and does `vanished` close it early? | never adjudicated; does not affect action cost | Y-28 |
*(Listed for completeness but classified **out of Alpha 2 scope**, not as a reference question: whether the reference's In Ex Por door-unlock should also be wired on the **TypeScript** side. It touches `game/src`, not the native port — R-16 residual, `content-audit.md` PENDIENTE(3).)*

---

#### 6. Intentional divergences

Now enumerated in one place for the first time: **[`ALPHA2_PRESERVATION_LEDGER.md`](ALPHA2_PRESERVATION_LEDGER.md)**. Counts: **14 T-Deck adaptations** (A-1 – A-14), **4 deliberate enhancements** (E-1 – E-4), **18 unresolved divergences** (D-1 – D-18, of which exactly one — D-11 — is a live production defect, R-25).

Nothing was reverted. The ledger's purpose is to make a future preservation profile possible, and it records per row whether the difference is already isolated enough to become a compatibility toggle. Seven adaptations and two enhancements are assessed as isolated; A-1 – A-5, A-13 and A-14 are **not restorable**, because hardware and not preference forced them.

---

#### 7. Hardware-only checks

**None were performed. No physical T-Deck was available in this session, and no hardware evidence is claimed.**

All still-relevant device steps from every prior batch are consolidated and deduplicated into **[`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md)**: **140 rows**, each with setup, exact input, expected visible result, expected state change, cancel-path expectation, SD-refresh requirement and a PASS/FAIL/UNTESTED field.

* **4 rows carry real PASS evidence** and are marked as such: H-48 (Batch 5's three world-cast checks) and H-85/H-86/H-88 (the Batch 9B dungeon-runtime gate).
* **136 rows are UNTESTED.**
* The three checks Batch 16 left outstanding are rows **H-10/H-11** (Symbol+Mic `0`), **H-12 – H-14** (Vas Rel Por), **H-15/H-16** (Rel Hur and skull-key cancel).
* Batch 18 adds **H-17 – H-23** for its own fixes (the well ceremony and the R-34 general chained-modal check).
* **Batch 18 also found sixteen resolved findings that owed a device check and had no row anywhere**, and added Group 11 (**H-125 – H-140**) for them. The reconciliation table at §3b is what surfaced them: R-20 (harpsichord), Y-20 (digits as Set Active Player), Y-27 (System Menu over a modal), Y-26, Y-18, R-31, R-24, R-27, R-28/R-29, R-30, Y-28's two record-only rows, and — the most substantial gap — **five of Y-04's six feedback channels**. Only `CellProjectile` had a row; Quake, CellExplosion, PoisonTick, Refuge and TrollSneak were all "GREEN — resolved (Batch 7B)" with nothing on any device list to confirm them. This is the clearest single justification for consolidating the checklist: the per-batch phase lists each covered their own batch and nobody had ever read them as one set.
* **SD refresh: not required.** No resource file changed. The last pack change was Batch 9C.

---

#### 8. Firmware

`idf.py -B build-batch18 build` — **PASS**, from a fresh build directory.

| | |
|---|---|
| `openu5_tdeck.bin` | `0xd1b30` = **858,928 bytes** |
| Delta vs Batch 16/17 (`0xd1b10`) | **+32 bytes** — the R-26/R-34 changes and the `"a well."` literal |
| App partition free | `0x2e4d0` = **189,648 bytes (18 %)** of `0x100000` |
| Project warnings | **0**. The 15 warning lines in the log are the five pre-existing ESP-IDF `component_validation.cmake` notices about `esp_wifi`/`wpa_supplicant` private includes, three lines each — third-party, not ours |
| SD resource pack | **unchanged; no recopy required** |

---

#### 9. Alpha 2 readiness

**HOST-CLEAN — HARDWARE VALIDATION REMAINS.**

The software half is finished to the limit of what host evidence can establish: 86/86 with both parity corpora green, zero project warnings on host and firmware, no TODO/FIXME/stub in any production translation unit, no platform-macro-guarded gameplay branch that host tests cannot reach, no gameplay text constructed in the device layer, and no direct `GameState` mutation outside the core seams (all four re-verified by sweep this batch — see "Phase 18D result" below).

It is **not** ALPHA 2 READY, and the blocker is nameable: **136 of 140 device rows have never been executed**, and one live production defect (R-25) is open with a deferred, scoped fix. Alpha 2 cannot be called validated until the checklist has no UNTESTED rows.

---

#### Phase 18D result — the search for untested production bypasses

Run as a sweep, not a sample. What it found:

| Sweep | Result |
|---|---|
| `TODO`/`FIXME`/`XXX`/`HACK`/`stub`/`temporary`/`placeholder` in `native/core/src`, `native/core/include`, `native/targets/tdeck/main` | **No actionable hit.** Every match is a descriptive comment (a "temporary per-cell override", a "temporary diagnostic file") or a variable name. No parked work, no commented-out behaviour |
| `#ifdef ESP*` / `#if defined(ESP*)` / `#ifdef CONFIG_*` gameplay branches | **None.** There is no gameplay code that host tests cannot reach for platform reasons |
| Gameplay text constructed in the device layer | **None.** The only string literals `AlphaRuntime` appends are on `UiTextChannel::System` (boot banner, "Load complete", "No valid save", "Developer tools disabled", "Shop selection unavailable"). The one flavour line it prints, the fountain result, comes from `openu5::fountain_drink_result()` in the core |
| Direct `GameState` mutation outside core seams | **None.** Every `game_.party`/`gold`/`position`/`time` reference in `alpha_runtime.cpp` is a read or a log argument |
| Duplicate command implementations | **None in production.** The duplication that exists is **test-side and self-declared**: `dungeon_combat_test.cpp` and `dungeon_input_test.cpp` mirror `AlphaRuntime`'s per-input tail and say so in their own file headers. Batch 11/14/18's host-linked `AlphaRuntime` targets are the answer to that, and they now cover three command families |
| Old stub paths reachable in production | **None found.** The dead-but-handled command kinds (§5) are unreachable, not stale shims |
| **Untested production seams** | **One, and it was the batch's fix.** The entire well ceremony — description, prompt type, answer dispatch, wish-row prompt — was reachable only through `AlphaRuntime`, and no host test touched it. That is precisely how R-26 survived fourteen batches while `gameplay_parity` drove the underlying core arm 5,058 sequences at a time. R-34 was hiding behind it |

**One test was added, and only because a real uncovered production seam existed.** No test was written to inflate coverage.

**One non-gameplay hygiene finding, recorded and deliberately not fixed.** A tracked 2,704-byte file literally named `-` sits at the repository root. It is a CMake compiler-detection preprocessor dump (`CXX-DetectStdlib.h` expanded against the zig libc++ headers) that was captured to a file named `-` instead of to stdout, and it was committed in `8a603f8f` ("Add native physical combat foundation"). It is inert — nothing reads it, no build references it — but it is tracked, it shows as modified in every `git status`, and its name breaks naive shell tooling (`head -c 400 -- -` reads stdin, not the file). Removing a tracked file is outside an audit batch's remit and has nothing to do with Alpha 2, so it is logged here rather than deleted. Suggested disposal: `git rm -- ./-` in any batch that is already touching build files.

---

#### Files changed

**Production (2):** `native/core/src/ui_session.cpp` (R-26 items 2–4, R-34), `native/targets/tdeck/main/alpha_runtime.cpp` (R-26 item 1).

**Test/harness (2):** `native/targets/tdeck/host_tests/batch18_well_ceremony_test.cpp` (new, 19 checks), `native/core/CMakeLists.txt` (the new target).

**Documentation (3):** `native/targets/tdeck/ALPHA2_HARDWARE_CHECKLIST.md` (new), `native/targets/tdeck/ALPHA2_PRESERVATION_LEDGER.md` (new), this document (§1, §3 R-25/R-26/R-34, §5, §7, §8, §14, §16).

#### RED → GREEN evidence

| Stage | Result |
|---|---|
| RED, unmodified production code (first run, 16 checks) | **5 failures** — W1-3 (`a well.` missing), W2-1 (No never dispatched), W3-1 and W3-2 (ESC ignored), W4-3 (fabricated wish prompt) |
| RED, after adding group W6 (19 checks) | **2 failures** — W4-3 and W6-3, both R-34, on two independent chains |
| GREEN, after the fix | **19 / 0** |
| Full suite | **86 total · 86 pass · 0 fail** |

| Mutation | Effect |
|---|---|
| M1 — clear `prompt_`/`input_` unconditionally again (defeat R-34's guard) | **2 RED** (W4-3, W6-3), reverted |
| M2 — restore the `&&i.value.yes` gate and drop `c.member` | **2 RED** (W2-1, W3-2), reverted |
| M3 — `cancel_means_no` back to `false` | **2 RED** (W3-1, W3-2), reverted |
| M4 — drop the `"a well."` description | **1 RED** (W1-3), reverted |

#### What this batch did NOT do

- It did not start Alpha 3, and it did not begin Batch 19.
- It did not fix R-25. The scoped follow-up is written down instead (§3 R-25), because a partial `pickCommandChar` would be a guess.
- It did not revert any intentional enhancement, and it did not implement any compatibility toggle — the ledger assesses feasibility only.
- It did not close a single hardware check from host evidence. The three Batch 16 checks and every other device row remain **UNTESTED**.
- It did not touch Y-32. No timing evidence has appeared since Batch 16 refused it, and none was invented here either.
- It did not change the resource pack, so the flashed SD card needs no repack.
- It did not modernize anything. Both production edits move the port **toward** the 1988 binary: one restores a missing echo and a missing description, the other stops a teardown from erasing a reference prompt.

---

### Batch 19 — R-25: the kernel `0x4988` command-character picker as a shared seam · risk: medium (one new core seam, three callers re-pointed) · **GREEN — THE LAST KNOWN HOST-SIDE ALPHA 2 PRODUCTION DEFECT IS CLOSED**

**Scope.** Close R-25 the way Batch 18 scoped it: recover kernel `0x4988`, expose it once as a production seam, prove it RED, and route the crystal ball plus any divergent `(S)earch`/`(C)ast` selection through it. No Alpha 3 work, no hardware checklist execution, no unrelated modernization.

#### 1. Baseline (Phase 19A)

Clean `native/core/build-batch19` on the unmodified Batch 18 tree `bb0067a1` (tag `alpha2-batch18-comprehensive-audit-r26-r34`), before any edit: **86 total, 86 pass, 0 fail, 0 skipped**; `gameplay_parity` PASS at 5,058 sequences; `quest_parity` PASS at 5,377 cases; **zero project compiler warnings** (the one warning line in the build log is inside `w64devkit`'s own `bits/stl_uninitialized.h`, a toolchain header, not project code). The declared entry state is confirmed.

#### 2. The reference contract (Phase 19B)

Primary authority is the 1988 disassembly, adjudicated in `re/notes/resolve-command-char-178c-acta.md`, which reads `ULTIMA.EXE:0x4988`–`0x4a83` (252 B, `ret` with no arguments) end to end, plus its sibling `select_party_member` `0x2d7a` and the `0x2e8e` wrapper. The caller derivations are `re/notes/bola-144-acta.md` §2 (crystal ball), `re/notes/cmds.md` §11 (Search) and `re/notes/cast-input.md` §9 (Cast). The TypeScript `game/src/ui/pickers.ts::pickCommandChar` was used only as corroboration; it cites the same offsets and it agrees.

**Picker mechanics — the four branches, in the binary's own order:**

| # | Site | Condition | Behaviour |
|---|---|---|---|
| 1 | `@0x4995` | `g_location > 0x80` (combat) | The member is the ACTING combatant: field `+3` of `g_combat_actor_records[g_cmb_actor]`. **No prompt.** |
| 2 | `@0x49b2` | `g_active_char != 0xFF` | That member, returned directly. **No prompt, and no eligibility test** — the compare is against `0xFF` and nothing else. |
| 3 | `@0x49dc`/`@0x49fa` | census of status `'G'`(0x47)/`'P'`(0x50) over the roster | `cmp [bp-6],1` / `jle 0x4a5f`: **one** eligible auto-selects; **zero** falls through to the epilogue still carrying the `0xFFFF` that `@0x4990` wrote. |
| 4 | `@0x4a02` | two or more eligible | Prints DS `0xa3c4` `"Player: "`, opens the roster select (`0x2e8e`→`0x2d7a`), echoes the chosen name on the same row (`@0x4a30`-`@0x4a3a`). |

Inside branch 4: a pick whose status is neither `'G'` nor `'P'` prints DS `0xa3ce` `"Disabled!"` at `@0x4a4e` and **re-asks** — `di` is still 0, so `@0x4a55`/`@0x4a57` jump back to the prompt, and the name is NOT echoed on that path. Leaving with `-1` — cancel, or branch 3's zero-eligible fall-through — prints DS `0xa3da` `"None!"` from the common epilogue at `@0x4a5f`. Input is **cursor/row-based** (reverse-video row highlight), not numeric and not directional, and there is no `">Select:<"` band. Selection mutates no state; the result is a member index, or `-1`.

**Caller behaviour, kept OUT of the seam.** `LOOKOBJ 0x09ea` calls the picker, then `0x09f0`'s `inc ax / jne` skips the entire tail on `-1`; the roll `rand_range(1,30)` is at `0x09f6`; `0x0a10`'s `ja` is **unsigned**, so the tie loses and INT must strictly exceed; the lose branch damages the picked member by 1 and the win branch opens `gem_view` **without** decrementing gems (the `dec [g_gems]` lives at `0x3428`, inside the `(V)` case). `SJOG 0x095c` asks for the direction at `0x097e` **before** calling the picker at `0x09a0`. `CAST.OVL:0x0dd5` calls the picker **before** the `"Spell name:"` prompt.

**Declared as NOT modelled, each with its adjudication:**
- The `-2` handling at `@0x4a6e` is **dead code for this caller** — the `0x2e8e` wrapper forces `select_party_member`'s argument to 0, and `-2` is only produced with a non-zero argument (acta §3). Not ported, by derivation, not by guess.
- The `0x80`-vs-`0x7f` threshold asymmetry against `select_party_member` is **real in the bytes but has no reachable discriminating value measured** (acta §2 measures a bound, not an absence, and says so). It only matters inside branch 1. Left alone in both directions, as the acta instructs.
- The `+2` bit-`0x80` test that `select_party_member` performs and `0x4988` omits is a real asymmetry, but the only combat caller performs it *for* the routine at `COMBAT.OVL:0x0909` before the call (acta §6.2), so it is not observable. Not modelled.

**Ambiguity declared.** The exact roster bound the census at `@0x49dc` sweeps is not stated in the notes. The seam uses `party_size` clamped to `character_count`, which is what `UiMode::PartySelection` lists and what the TypeScript picker sweeps. That choice is corroboration-led, not binary-led, and is marked as such in the header.

#### 3. What native actually did (Phase 19C)

| Caller | Before Batch 19 | Verdict |
|---|---|---|
| Crystal ball | `ui_session.cpp` raised a **fabricated** `"Peer into it?"` yes/no; `alpha_runtime.cpp` dispatched `CrystalBall` with `Command::member` at its never-set `-1`; `look.cpp` rejected that outright. **100 % non-functional.** | **Defect — fixed.** |
| `(S)earch` | The core's `search_world(…, searcher)` fallback is `searcher ?? (active == 0xff ? 0 : active)` — a **byte-exact clone of the reference core's own** `game.ts:3526`. But every native caller passed `-1`, and unlike the reference port no UI layer ever supplied a searcher. | **Core correct; the caller was divergent — fixed at the caller.** |
| `(C)ast` | `cast_selected_spell()` set `caster = active_member()`, i.e. "active, else member 0", after the spell menu. | **Divergent — fixed**, including the prompt ORDER. |
| Combat `(C)ast` | Also `active_member()`, i.e. the arrow-marked member rather than whose turn it is. | **Divergent (branch 1) — fixed.** |
| Dungeon-corridor `(S)earch` | `SJOG 0x0646` `search_dungeon`, a **different routine**, recorded as only partly ported. | **Not touched** — different reference routine, not this one. |
| Fountain "Who will drink?" | `pickMember` flavour text, a different routine (R-09 §A.2). | **Not touched.** Similar-looking UI, different reference. |
| `(R)eady` / `(Z)`-stats | Selector 1, the RAW picker `kernel 0x2d7a`, which has **no** active-character gate — `re/notes/player-select-selectors.md`, confirmed empirically in DOSBox on 2026-07-17. | **Not touched, and must not be** — they always ask. |

#### 4. The seam (Phase 19D)

New, ESP-free, production-owned, caller-agnostic: `native/core/include/openu5/command_char.h` + `native/core/src/command_char.cpp`.

```
bool               command_char_eligible(char status);                    // @0x49dc
CommandCharResult  resolve_command_char(const PartyState &);              // branches 2/3/4
bool               command_char_accepts(const PartyState &, int32_t);     // @0x4a2e
const char        *command_char_prompt/disabled/none();                   // DS 0xa3c4/0xa3ce/0xa3da
```

`CommandCharOutcome` is the explicit result type the scope asked for: `Resolved` (a member, no prompt owed), `Prompt` (branch 4), `None` (zero eligible — the reference-equivalent exceptional state). It takes `PartyState` directly rather than a copied projection, so the eligibility rule and the three strings exist **once** in the whole project.

**Where the branch decision runs, and why it is not inside `world_look()`.** The first implementation put the gate in the core, and `gameplay_parity` failed at sequence **895** — the fixture expects `look()` at a crystal ball to emit a bare `crystal-ball-prompt` even with `activeCharacter: 0`. That is correct and it is the reference's own layering: `game.ts:6465` returns `[{kind:"crystal-ball-prompt"}]` for **every** tile `0x29`, and `ui/pickers.ts` runs the picker, because in the binary the picker is **blocking and owns a modal**. The gate was moved to the caller/UI boundary to match, which also put all three callers on one code path. The failing fixture was treated as the oracle and was **not** edited.

#### 4b. Two deliberate deviations from Batch 18's four-step scope

Batch 18 wrote the scope before the seam existed; two steps were implemented differently, and the reasons are recorded rather than silently absorbed.

1. **Step 2 said "add `UiRequestId::CommandChar` and route `CrystalBallPrompt` through it".** What shipped keeps `UiRequestId::CrystalBall` and adds `SearchMember` and `CastMember` — one id per caller, not one shared id. A single shared id would have needed a "which caller is pending" discriminator in `AlphaRuntime` to know what to do with the answer, and it would have bought nothing: the picker's own rules (the branch decision, the `"Disabled!"` re-ask, the two `"None!"` exits) are already single-sourced in `command_char.h` and in the two `AlphaRuntime` helpers, so no picker logic is duplicated per caller. What differs per id is only the caller's own work after the pick, which is exactly what should differ.
2. **Step 1 said the seam should reproduce "the four branches".** It reproduces three. Branch 1 (combat) is deliberately excluded and the reason is in the header: the only reachable combat path takes `g_cmb_actor` directly, and modelling it in the seam would mean feeding a party-roster function a combat context it has no reason to own. Batch 19 *did* fix that branch — in the two places that were divergent, `cast_selected_spell()` and the `Target` arm, both of which used the arrow-marked active member instead of the acting combatant.

#### 5. RED proof (Phase 19E)

Two suites, because the defect straddles the core/glue boundary.

**`batch19_command_char` (core, 57 checks)** — drives the seam and `world_look()`. Against unmodified Batch 18 code it is **GREEN**, and that is declared rather than dressed up: after the layering correction above, everything it asserts about the core's crystal-ball tail was already correct — which is exactly what the audit always said ("the already-correct core arm"). It is a lock, not a RED proof, and it is validated by mutation instead (§6).

**`batch19_command_char_runtime` (49 checks)** — links and drives the **real, unmodified `alpha_runtime.cpp`** through raw `RawInputEvent`s, the real `UiInputAdapter` and the real `UiSession`, the same seam Batch 18's `batch18_well_ceremony` used. Against Batch 18 production code: **RED 24 / 49.**

| Group | RED at Batch 18 |
|---|---|
| C1 crystal ball opens the roster picker, not a yes/no | C1-1, C1-2 |
| C2/C3 a pick actually runs the vision, and damages exactly that member by 1 | C2-2, C2-3, C3-1, C3-2, C3-3 |
| C4 cancel prints `"None!"` | C4-1 |
| C5 active character resolves with no prompt | C5-2 |
| C6 single eligible auto-selects — and it is the ELIGIBLE one, not member 0 | C6-2, C6-3 |
| C7 zero eligible prints `"None!"` | C7-2 |
| C8 a disabled pick prints `"Disabled!"` and RE-ASKS | C8-1, C8-2, C8-4, C8-5 |
| C9/C11 `(S)earch` asks for its searcher, after the direction; cancel drops it | C9-1, C9-2, C11-1, C11-2 |
| C12/C14 `(C)ast` asks BEFORE the spell menu; cancel opens no menu | C12-1, C12-2, C12-3, C14-1 |

The four groups that stayed GREEN at Batch 18 are the ones that must not move: C10 (`(S)earch` with an active character does not ask), C13 (`(C)ast` likewise), C15 (the well and fountain prompts), C16 (an ordinary `(L)ook` keeps its `"Thou dost see"`) and C17 (the R-34 chained-modal prompt survives).

#### 6. Mutation validation (Phase 19E)

Four mutations of `command_char.cpp`, each built, run and reverted:

| # | Mutation | core RED | runtime RED |
|---|---|---|---|
| M1 | drop `'P'` (0x50) from the eligibility census | **6** | 0 |
| M2 | auto-select with two eligible (move the `jle` boundary) | **4** | **19** |
| M3 | gate the active-character branch on status (a test `@0x49b2` does NOT do) | **2** | 0 |
| M4 | zero eligible falls back to member 0 instead of `"None!"` | **2** | **2** |

M1 and M3 are invisible to the runtime suite because its fixtures never use a `'P'` member or a disabled active character; the core suite covers exactly those. Both suites together catch all four.

#### 7. The fix (Phase 19F)

**F1 — Crystal ball.** `look.cpp` keeps raising `CrystalBallPrompt` for every tile `0x29` (reference layering, `gameplay_parity`-pinned) and its tail is now the single `crystal_ball_vision()` helper, so the auto-resolved route and the answered-prompt route cannot drift. `ui_session.cpp`'s fabricated `"Peer into it?"` `begin_yes_no` is **deleted**; the arm now dispatches `OpenPartySelection` and lets the roster owner decide whether a modal opens at all. `alpha_runtime.cpp` resolves the branch, dispatches directly on `Resolved`, prints `"None!"` on `None`, opens the `"Player: "` picker on `Prompt`, applies the `"Disabled!"` re-ask on an ineligible pick, and prints `"None!"` on cancel. Verified end to end: correct prompt, correct selection, correct vision, 1 HP to the chosen member, gem view with `gem_from_crystal` so the device charges no gem and no `(V)` turn, correct cancel, no duplicated selection logic.

**F2 — `(S)earch`.** The core is untouched — its fallback is the reference core's own, and `quest_parity` drives it. `AlphaRuntime::command()` now resolves `Command::member` through the seam before dispatch, after the direction, matching `SJOG 0x097e`→`0x09a0`. This changes the perceiver of every chest trap check to the reference's; `gameplay_parity` and `quest_parity` are the oracle and both stayed green.

**F3 — `(C)ast`.** The caster gate runs **before** the spell menu (`CAST.OVL:0x0dd5` precedes the `"Spell name:"` prompt of `0x11d9`; the OCR corpus of 49 original Let's-Play routes has **70** `"Cast... Player: <name> Spell name:"` rows and **zero** in the other order). Combat and `(M)ix` keep the plain menu: combat is branch 1, and Mix is a different routine. Combat casting now takes the **acting combatant** as caster (branch 1) instead of the arrow-marked active member — a real bug, since it could spend the wrong character's magic points. Directional, world and dungeon spell *targeting* is untouched; only who casts changed.

#### 8. Verification (Phase 19G)

Focused: `batch19_command_char` **57/57**, `batch19_command_char_runtime` **49/49** (from RED 24/49). Regression: the crystal ball's success and cancel paths, `(S)earch`, `(C)ast`, spell targeting, modal chaining, prompt rendering, the untouched inventory/party selectors, the **R-34** chained-modal check (C17, an independent second copy of Batch 18's W6) and **R-26**'s well ceremony (C15-1/C15-2, plus the whole `batch18_well_ceremony` suite) all green. Authoritative suite: **88 total, 88 pass, 0 fail, 0 skipped**, `gameplay_parity` 5,058 and `quest_parity` 5,377 both PASS. **No newly excused "known failure" — there are none in this project and Batch 19 added none.**

#### 8b. Firmware and device impact (Phases 19I / 19J)

T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch19`. `openu5_tdeck.bin` is `0xd20a0` = **860,320 bytes**, **+1,392** on Batch 18's `0xd1b30` = 858,928, with **188,256 bytes (18 %)** of the app partition free. **Zero project warnings**; the five warning lines are the pre-existing ESP-IDF `component_validation.cmake` notices. The **resource pack is unchanged** — still the 2,039,545-byte Batch 9C `openu5-alpha1-resources.bin` — so **the SD card needs no repack**. That is the expected resource behaviour: this batch adds a core translation unit and rewires glue, and touches no asset.

Checklist impact, 140 rows → **145**:

* **H-50 rewritten.** It recorded R-25's broken behaviour ("a fabricated `Peer into it?` appears and Yes does nothing — record it, do not file a ticket"). It is now an expectation that can pass: the `Player: ` picker opens and no `Thou dost see` line appears.
* **H-141 – H-145 added**, because nothing in the existing 140 rows covered any of it: the win branch and its gem-count invariant (H-141), the lose branch's exactly-1-HP-to-the-chosen-member (H-142), the `"Disabled!"` re-ask (H-143), the three no-prompt branches and the zero-eligible `"None!"` (H-144), and the `(S)earch`/`(C)ast` prompt order (H-145).
* **No row was deduplicated.** The question was asked; the answer is that the picker had no coverage anywhere in the 140, which is consistent with R-25 having survived from the original baseline.

**141 of 145 rows are UNTESTED.** No hardware was run in this batch and none of the above is device evidence.

#### 9. What Batch 19 did NOT do

- It did not execute the hardware checklist, and it claims no hardware PASS.
- It did not begin Alpha 3.
- It did not touch `(R)eady`/`(Z)`-stats, the fountain drinker, the dungeon-corridor Search, or any selector that is not `0x4988`.
- It did not edit a parity fixture. When the core-side gate contradicted `gameplay_parity` sequence 895, the fixture won and the code moved.
- It did not model `0x4988`'s `-2` path, its threshold asymmetry or its `+2` bit test; each is declared above with the derivation that says why.
- It did not change the resource pack, so the flashed SD card needs no repack.

---

## 15. PROPOSED REGRESSION TESTS

### Invariants worth asserting (cheap, high value)

| # | Invariant | Where |
|---|---|---|
| 1 | Every gameplay `UiMode` maps to exactly one presentation source | runtime integration test |
| 2 | `combat`, `dungeon` and `world` are never simultaneously authoritative | assert in `render()` alongside the existing frontend/system-menu assert |
| 3 | `UiMode::Dungeon` ⇒ `dungeon_.active && context_.dungeon` | runtime |
| 4 | `UiMode::Combat` ⇒ `combat_.initialized && !combat_.ended` | runtime |
| 5 | **`UiMode::Shop` survives a full shop keypress round-trip** | runtime — **catches R-01** |
| 6 | **`UiMode::Dialogue` survives a dialogue keypress round-trip** | runtime — catches R-01 |
| 7 | Every `UiMode` reachable by `set_base_mode` has a `handle_input` case | static assert / test — catches `ShrineSpecial` |
| 8 | A successful pickup mutates exactly one inventory field | extend `combat_loot_open_regression` |
| 9 | A removed loose item no longer renders in that cell | `presentation_regression` — **ADOPTED (Batch 2)**, pre-existing coverage confirmed sufficient |
| 10 | An empty loot stack renders the underlying terrain | `presentation_regression` — **ADOPTED (Batch 2)**, pre-existing coverage confirmed sufficient |
| 11 | The visible loot tile equals `0x100 + (next Get result id)` | `presentation_regression` — **ADOPTED (Batch 2)** as R04-OVERLAY/R04-LIFO |
| 12 | ~~No world object renders with a tile < 0x100~~ | **REJECTED (Batch 2 investigation)** — this is a **false invariant**: legitimate `plot`-kind QuestObjects (crown, sceptre, shards, amulet) deliberately keep a raw tile below `0x100` (`quest_world.cpp:42`, `game.ts` `hydrateInteriorObjects`), so a blanket `tile>=0x100` check was explicitly not added. The narrower, correct invariant actually adopted is: *a stationary chest/prop-style object that reaches presentation with a sprite-bank tile renders that exact pre-offset tile unchanged* — already proven by `presentation_regression`'s existing `torch` case. |
| 13 | Every equipment slot holds ≤ 1 item | `item_parity` |
| 14 | Every modal `UiRequestId` opened by `consume` is handled by `finish_modal` **or** `AlphaRuntime::modal` | runtime — **catches R-09**. **Batch 19 note: this invariant would NOT have caught R-25.** `UiRequestId::CrystalBall` had a handler the whole time; the handler dispatched a command whose `member` was never set. The stronger form the crystal ball needed is row 23. |
| 15 | Every `GameEventKind` has a consumer or an explicit `IGNORED` entry | runtime — catches R-10; R-12/R-13/Quake now have consumers (Batch 7); `PoisonTick`/`CellExplosion`/`CellProjectile`/`Refuge`/`TrollSneak` remain open (Y-04) |
| 16 | Every `View` resource handler is reachable from the UI | runtime |
| 17 | Every dungeon id 33–40 loads valid map data | already smoke 18; promote to host |
| 18 | ~~Every spell has a target label consistent with its `target_type`~~ | **REJECTED (Batch 8) after adjudication** — `target_type` is the unreliable field for 11/12 flagged spells (the label correctly names the real getdir/reticle input instead); building this exact invariant would have forced 11 correct labels to become wrong. Replaced by 11 targeted semantic checks in `display_names_test.cpp` (R-16 RESOLVED) |
| 19 | ~~Every spell summary equals `MagicDefinitions.json::SimpleDescription`~~ | **REJECTED (Batch 8) after adjudication** — true for 9/10 flagged spells (fixed) but FALSE for In Ex Por, where the JSON's own claim was stale relative to RE-confirmed behavior until 2026-08-07; a blanket equality test would have baked that staleness in. Replaced by targeted semantic checks (R-16 RESOLVED) |
| 20 | Every serialized field round-trips, and **every live owner is serialized** | `persistence_driver` — **catches R-14, R-15** |
| 21 | Every `CommandKind` is either produced by a UI route or annotated core-internal | static test — R-19, R-20 and Y-20 are now routed (Batch 3). `CombatYield` is also live and routed: `UiSession::cancel_modal()` emits it, and `commands.cpp`'s generic `CombatMove..CombatEnemyStep` range dispatch (`static_cast<CombatAction>(int(cmd.kind) - int(CommandKind::CombatMove))`) carries it straight through to `combat.cpp`'s `CombatAction::Yield` handler — see `native/core/tests/batch3_group_c_test.cpp` C13. The remaining §5 dead/annotated-only list is `Unready`, `CombatEscape`, `ShopAction`, `BeginConversation` (R-10), `BlackthornAction` (R-09) |
| 23 | **Every `Command` field a core arm *rejects on* is actually set by every route that dispatches that command** | runtime — **would have caught R-25 fourteen batches earlier**. `look.cpp` rejected `CrystalBall` on `member < 0`, and the only route that dispatched it never assigned `member`. A dead feature with a live handler, a passing suite and a wired prompt. `batch19_command_char_runtime` now pins the crystal ball, `(S)earch` and `(C)ast` specifically; the general sweep is still open. |
| 22 | `Ready` succeeds in world, combat and dungeon | **DONE (Batch 3)** — `batch3_group_c` C1/C2/C3/C4 through the real `execute_command()` path, plus the combat `CombatActor` cache invariant |

### The structural change that makes most of these possible

**Superseded by Batch 11 — see its write-up in §14 for what was actually built.** This section originally proposed extracting `dispatch`/`modal`/`open_selection`/`cast_selected_spell`/`synchronize_*`/`consume_event` into a new ESP-free `GameplayController` class (~500 lines moved). Batch 11 evaluated that plan and took a different, lower-risk path instead: `alpha_runtime.cpp` is linked into a new host target **unmodified**, with ESP-IDF/Board/storage host substitutes standing in for its four actual device-dependency surfaces (logging/timing/allocation, `Board`, `AlphaSaveService`/`AlphaSettingsService`, `DeviceSmokeTests`) rather than moving any gameplay code out of it. The net effect — `AlphaRuntime`'s real dispatcher runs on host — is the same; the mechanism and risk profile are not. The rows above that name "runtime integration test" as their home now have a concrete harness (`native/targets/tdeck/host_tests/alpha_runtime_integration_test.cpp`) to be added to; only rows 3–6 (Move/Open/Use/View's adjacent invariants) have anything approximating coverage there yet, and even those are the specific assertions Batch 11 wrote, not a general sweep of this table — items 1, 2, 7–22 remain open.

---

## 16. PHYSICAL T-DECK CERTIFICATION PLAN

> **Superseded for execution by [`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md) (Batch 18).** That file is the deduplicated 140-row list to actually run, in one session, with a PASS/FAIL/UNTESTED field per row. **This section is kept as archaeology** — it records which batch owed which step and why, which the flat checklist deliberately does not. If the two ever disagree about an expectation, this section is the derivation and the checklist is the instruction; fix the checklist.
>
> **Current tally: 4 PASS, 0 FAIL, 136 UNTESTED.** The only hardware evidence on record anywhere in this project is Batch 5's three world-cast checks and Batch 9B's three dungeon-runtime checks.

Efficient broad-coverage pass using Developer tools. ~45 minutes. Each step names the log line that proves it.

### Phase 0 — Boot and frontend (5 min)
0c. **[Batch 9C — SD card, do this FIRST]** Batch 9C changed the resource pack, so firmware and card must be updated **together**. Rebuild the pack with `npm run pack:alpha1` and copy `native/assets/openu5-alpha1-resources.bin` over the existing `<SD>:\ultima5\openu5-alpha1-resources.bin`, replacing it. **Do not reformat the card** and do not delete anything else: saves and settings live in separate files and are untouched. Then flash `build-batch9c-dungeon-art/openu5_tdeck.bin`. The new pack is **2,039,545 bytes**, payload CRC32 `0x2065ad91`, SHA-256 `434cd664…b4ea`; `openu5-assets.bin` (the tile pack) is **unchanged** and must stay as it is. If the card is not updated the device will stop on the identity screen with `match=0` — that is the gate working, not a fault.
1. Boot. Confirm `IDENTITY ... match=1` and the identity screen.
2. Title → wait 20 s → attract demo → any key → menu.
3. Menu: `U` Introduction (page through), `A` Acknowledgements, `R` Return to View, `S` Settings (change brightness + trackball, Back to persist).
4. `C` Create New Character: name, gender, full questionnaire → gameplay. **Confirm `objects_` is empty and the party is the new identity.**
5. Back to frontend is not available — power-cycle, then `J` Journey Onward. Confirm the created save loads.

### Phase 1 — Input contract (3 min) · *regression guard, do not skip*
6. Mic short press → Cancel. **Confirm no `0` appears in any text field.**
7. Mic hold 1.1 s → `INPUT_HOLD ... emitted=movement-toggle state=ON`.
8. Movement Mode ON: WASD moves. Open Yell (`Y`) → type `wasd` → **confirm literal characters**.
9. Trackball in all four directions. `Alt+M` opens and closes the System Menu from the world.

### Phase 2 — Overworld and town (7 min)
10. Debug → Teleport → Britannia. Move; confirm day/night, wind bar, moon marks.
11. Teleport to **Britain**. Confirm NPCs are visible and moving on schedule.
12. `T`alk to an NPC: name / job / bye. **[R-01] Confirm the conversation survives more than one keypress.**
13. `L`ook at a sign. `S`earch. `O`pen a door. `J`immy a locked door. `K`limb. `P`ush furniture.
14. `Z`-stats: **R-22 implemented (Batch 14)** — `Z` opens the member picker, and confirming a member now opens the real page axis. Page through Stats, Arms, Provisions and the four lists and confirm the ring wraps; see §16 Phase 6H for the full checklist.
15. **[R-01] Enter a blacksmith. Buy an item. Back out one level at a time. Confirm the world view and Exploration verbs return.** Repeat at an inn (Rest), a healer (Heal), a tavern (Rations + Rumour) and a reagent shop.

### Phase 3 — Combat and victory loot (7 min)
16. Debug → Max Party, Max Resources, Equip Best Gear.
17. Trigger an overworld encounter. Move, `A`ttack with the reticle, `F`ire, `C`ast a combat spell.
18. **[R-06] Press `R` in combat, select a weapon. Confirm the weapon actually changes** — and that the character then *attacks with it* (the `CombatActor` cache resync). Fixed in Batch 3; hardware confirmation outstanding.
19. Win. **Confirm the arena stays open.** `O`pen the chest, `S`earch it, `G` + direction repeatedly to zero.
20. **[ANCHOR 1 — Batch 2 regression-validation] Confirm the cell shows plain arena floor — no blue square, no leftover symbol.** (Code/test-level fix is GREEN as of Batch 2; this step is the outstanding hardware confirmation, not a check for a known-open defect.)
21. **[ANCHOR 2 — Batch 2 regression-validation] Before each `G`, note the visible icon; confirm the message names the same item.** (Code/test-level fix is GREEN as of Batch 2; this step is the outstanding hardware confirmation.)
22. Mic (Back) → canonical victory exit. **[ANCHOR 1 — Batch 2 regression-validation] Confirm no blue tile-1 cell at the encounter coordinate on the overworld.** (`gameplay_parity` mismatch 59 is fixed at the code/test level; this step is the outstanding hardware confirmation, not a check for a known-open defect.)
23. Save, reload. **[R-14] Confirm any loot left behind is still there.** Fixed in Batch 6; hardware confirmation outstanding.

### Phase 4 — Items, magic, view (6 min)
24. `U`se: **[R-07/R-08]** confirm every owned tool is listed with the correct name; confirm "Grapple" is absent and the Amulet is present. Fixed in Batch 3; hardware confirmation outstanding. (Pocket Watch is expected to be absent — no backing state.)
25. `U`se a potion on a party member. `U`se a scroll.
26. `C`ast Mani on a companion. `M`ix a spell.
27. **[R-11 — Batch 5 hardware checkpoint, GREEN — PASS]** Cast **An Sanct** in the world at a locked door; cast **In Por** in world gameplay (overworld) and confirm the blink; cast **An Ex Por** at a door in a town. Each must raise a `Direction?` prompt, echo the direction, and apply. All three passed on the physical T-Deck, each on the first test attempt. Fully certified in software and on physical hardware.
28. **[ANCHOR 3]** `V` with gems. **Photograph the screen.** Confirm the gem count decremented, a legible map appeared, and closing it charged exactly one turn (`VIEW_EFFECT` / `VIEW_RESULT` in the log).

### Phase 5 — Transport (4 min)
29. Debug → Transport → Ship. `B`oard. **[R-19] `Y`ell → confirm HOIST/FURL, not a word prompt.** Sail with the wind. Fixed in Batch 3; hardware confirmation outstanding.
30. `X`-it. Board a horse, a carpet, a skiff. Confirm the avatar sprite changes each time.
31. Save, reload aboard the ship; confirm transport and hull survive.

### Phase 6 — Dungeon (10 min) · *the big one*
32. Debug → Teleport → Dungeon (Deceit), floor 0, standard entry. Confirm `DUNGEON_SESSION_STATE active=1` **and** a usable view.
33. **[ANCHOR 4] Photograph the view.** As of Batch 9C the authored Ultima V wall art **must be present**: textured masonry slices with baked floor speckle and ceiling, not flat wedges and not outlines. If you still see line/wireframe geometry, the card is stale — re-check step 0c before failing anything else.
33a. **[R-05 light gate]** With the torch OUT and no light spell running, stand anywhere in the corridor. **The viewport must be entirely black.** Ignite a torch: the corridor appears on that keypress.
33b. **[R-05 bands]** Read the strip above the viewport: it must say **`L1`**, not a blank sky. Read the strip below: **`Dir:` + the facing**, not `Wind: --`.
33c. **[R-05 turning]** Turn left then right **with the trackball** (or with `A`/`D` *only* after Movement Mode is ON — with it OFF, `A` is **Attack** and `D` is **Drink**, which is what made the first physical session read as “controls dead”). **The lower band must change on each turn.**
33d. **[R-05 doors]** Stand so a **door or a room entrance** is two cells ahead down an open corridor. The corridor must **stop** at it and show a dead end with a door panel — previously the view ran straight through as if the cell were open. Then stand ON the door: the two nearest side slices must drop away.
33e. **[R-05 features at depth]** Stand so a **ladder or a chest is one or two cells ahead**, not underfoot. **It must be visible from there** — previously only the cell underfoot ever drew anything.
33f. **[R-05 wanderer]** Let a wandering monster approach. It must be drawn from **three cells out**, not only when adjacent; if it is a ceiling-lurking type it must appear **high in the frame**, not vanish.
33m. **[R-05 authored walls — Batch 9C]** The corridor must be **textured art**, and the four depth rings must **abut** with no seams or black gaps between them (left edge 16→96, right 96→176). The only black in a lit view is the vanishing point beyond the light.
33n. **[R-05 authored front door]** Repeat 33d and look at the art, not just the geometry: a **dead end** and a **door** must be visibly different images, and the front wall must read as one symmetric piece — its two mirrored halves meet at the centre column and must not overlap or leave a gap.
33o. **[R-05 side geometry]** Find at least one **passage**, **alcove** and **side door** on a side wall. All three must be visually distinct from plain masonry and from each other, and each must appear on the **correct side** — walk past it and confirm it stays on the side the map says.
33p. **[R-05 wall variant]** Visit a dungeon whose variant differs (`dungeon_wall_variant()`: dungeons 1, 4, 5 → DNG3 grey; 6, 7 → DNG2 red; the rest → DNG1 olive). The wall **appearance must change**, and it must change on entry, not after a step.
33q. **[R-05 authored wanderer]** Repeat 33f against the art: the monster must be a recognisable **sprite** with clean edges against the corridor — no opaque rectangle around it (that would mean the AND-mask was lost) and no holes punched through its dark interior (that would mean colour-0 keying crept back in).
34. Movement Mode ON: `W` advances, `S` backs up, `A`/`D` turn. Walk into a wall; confirm the blocked response.
35. Trackball: same four.
36. Find stairs, `K`limb down. Confirm the depth readout and a new floor. **[R-05]** The top band must read `L2` on the descent itself.
37. Walk into a pit; walk into a field. Confirm the damage message and the feature art.
38. `S`earch, `O`pen, `G`et, `J`immy in the dungeon.
39. `C`ast Uus Por / Des Por. **[R-06] `R`eady in the dungeon** — confirm it applies and charges no turn. Fixed in Batch 3; hardware confirmation outstanding.
40. Trigger a dungeon encounter. Win. **Confirm the return is to the same cell and facing.**
41. `Alt+M` System Menu → close. `Alt+D` Developer → Back. **Confirm the dungeon view returns both times.**
42. **[R-15]** Save inside the dungeon, reload, and confirm the dungeon session (position, facing, revealed cells, wanderer) resumes exactly. Fixed in Batch 6; hardware confirmation outstanding.
43. Descend past floor 7 → Underworld. Confirm the transition.
44. Walk out at the level-1 entrance → surface. Confirm the world view and Exploration verbs.


### Phase 6B — Dungeon RUNTIME gate (Batch 9B, 5 min) · *run this before any Batch 9C art work*

*Superseded by Batch 9C:* flash `build-batch9c-dungeon-art/openu5_tdeck.bin` and update the card per step **0c** — 9C changed the pack, so the 9B card no longer boots. The steps below are unchanged and remain the input-routing gate; run them in the same session as 33m-33q.

33g. **[9B input — turning and movement]** Inside Deceit with a torch lit, turn left, turn right, advance, and back up **with the trackball**. Facing must change on each turn and the cell must change on each move — watch the `Dir:` band and the corridor together. Then press Enter: that is now **Turn Around**, so the `Dir:` band must flip 180°, *not* pass the turn.
33h. **[9B input — the critical one, Ignite from the dark]** Let the torch burn out, or teleport in without one. The viewport must be black (33a). Now press `I`. **A torch must light and the corridor must appear on that keypress.** Before Batch 9B this answered `"What?"` and the only way out of a dark dungeon was to leave it.
33i. **[9B input — dark dispatch]** While still dark, turn left twice and advance once. The viewport stays black, but the `Dir:` band must still change on each turn. Relight with `I`: the corridor that appears must be the one those commands moved you to. **Darkness suppresses sight, never input.**
33j. **[9B input — Klimb both ways]** Find a cell with a ladder **up and down**. Press `K`: the prompt `Klimb-U/D-` must appear. Answer **down** on the trackball. The top band must read one level deeper. Before Batch 9B this cell always went **up** and descending from it was impossible. On a one-way ladder `K` must still resolve with no prompt at all.
33k. **[9B input — Search direction]** Press `S`. The prompt `Dir-` must appear. Answer **down** (Here) and confirm the search reports the cell underfoot, not the one ahead. Also confirm `H` opens the camp hours prompt, `D` drinks (or answers "No fountain here."), and a digit key sets the active player.
33l. **[9B identity]** While standing in Deceit, read the right-hand location strip: it must say **`Deceit`**. Reach it *from* another named map (Serpent's Hold is the case that produced the original report) and confirm it still says `Deceit`, then leave and confirm the strip returns to the surface name. Check one more dungeon if convenient.

**Gate.** 33g, 33h and 33j were confirmed on hardware at the Batch 9B checkpoint. Re-run the whole 33g–33l run once on the 9C image — now that the corridor is authored art, 33g and 33i are judgeable in a way they were not against the wireframe. If any step fails, **stop and report it**: the art work sits directly on this path, and a routing regression would look like an art fault.

**Batch 9D correction to 33j and 33k.** Both steps were recorded as confirmed at the 9B checkpoint, but neither prompt could actually appear on that image: nothing in `AlphaRuntime` ever published the two mirrors they read (§14 Batch 9D, defect C). On the 9D image `Klimb-U/D-` and `Will you drink?` must genuinely appear. **Re-run 33j and 33k and judge them fresh.**

### Phase 6C — Non-overworld COMBAT TRANSITION gate (Batch 9D, 8 min) · *firmware only; the SD card is unchanged*

Flash `build-batch9d-combat/openu5_tdeck.bin`. **Do not touch the card** — Batch 9D changes no resource, and the Batch 9C pack is still the right one.

33r. **[control case — overworld]** Start one ordinary overworld fight. Attack, aim, pass and move must all work exactly as before. This is the known-good comparison; if it is wrong, stop here, because nothing below is interpretable.
33s. **[dungeon wanderer]** In a dungeon, find a wandering monster and step so it is in the cell **directly ahead**. Press `A`. *(Requiring `A` is correct — §14 Batch 9D, "Encounter initiation". Sight and adjacency never start a fight; only `A` while facing it, or the monster stepping onto you.)* The combat scene must open **and the controls must work on the first key**: `A` must open the aim reticle, the trackball must move the reticle and then the actor, Space/Enter must pass. Fight it to a conclusion.
33t. **[return to the dungeon]** When the fight ends, the dungeon view must come back by itself. The **first** key afterwards must be a dungeon command — no extra press to "wake it up". Check `Dir:` and `L#` are still right and the location strip still names the dungeon.
33u. **[combat room]** Take the ladder into the room that previously produced dead controls (slimes and a chest). Combat controls must work immediately, the chest must be openable with `O` + direction, and walking the party off the board edge must return you to the corridor with working input.
33v. **[developer teleport safety — the freeze case]** Start a dungeon fight, then open the developer menu (`Alt+D`) and attempt a teleport out. **The expected result is an explicit refusal reading "Blocked by active combat" — not a teleport, and above all not a freeze.** Back out of the menu: the fight must still be there and still playable. Finish or flee it, then repeat the same teleport — it must now work. Try both an out-of-dungeon destination (Britannia) and a same-dungeon floor change; both must refuse while the fight is live.
33w. **[stranded-arena guard]** If any fight ever *does* go unresponsive, do not power-cycle — capture the serial log and look for `COMBAT_STRANDED` and the surrounding `PRESENTATION_DISPATCH` lines. The runtime now closes that state by itself within one frame; if the log shows it firing, the recovery worked and the interesting question is what produced the state. If the controls are dead and `COMBAT_STRANDED` never appears, that is a different defect and the log will say which mode and which source owned the frame.

**Gate.** 33s, 33u and 33v are the three hardware reports this batch exists for. If 33s or 33u still shows dead controls, **stop and report the serial log** rather than re-opening the dungeon art: §14 Batch 9D records exactly what was ruled out and what the log lines now distinguish.

### Phase 6D — Dungeon combat-ROOM escape gate (Batch 9E, 6 min) · *firmware only; the SD card is unchanged*

Flash `build-batch9e-final/openu5_tdeck.bin`. **Do not touch the card** — Batch 9E changes no resource, and no gameplay behaviour either: these steps confirm the authored mechanism on glass and record the sealed return as **expected**.

The room is **Deceit, floor 1, (5,3)** — the slime-and-chest room. Reach it from Deceit floor 0 (5,3), the ladder cell behind the secret door, and (K)limb down.

33x. **[the authored escape — the step that matters]** Enter the room. **During combat**, find the **up-ladder tile inside the arena** (board cell (5,2), near the top of the cave), walk a party member onto it and press **`K`**. Expected: the fight ends through the Klimb, the party is back on **Deceit floor 0 (5,3)** — the ladder they came down — dungeon controls work on the first key, and the room is **still unfought**.
33y. **[edge-walk — reference behaviour, NOT a regression]** Enter the same room again and leave *without winning* by walking every conscious member off one shared board edge. Expected: the party is back on **Deceit floor 1 (5,3)**, the room is still uncleared, **all four sides are walls, movement is Blocked! and dungeon-side `K` answers "Klimb-what?"**. **Record this as PASS.** It is what the original does; the exit was the in-arena ladder in 33x. Do not file it as a defect.
33z. **[victory]** Enter and **win** the room, then leave by the board edge. Expected: the room is marked cleared, `K` on the cell now lifts the party back up to floor 0, and klimbing back down starts **no** fight.
33aa. **[the other tile, if convenient]** Any room whose arena shows a **grate** or a **down**-ladder: the same in-arena `K` must take the party **down** a floor instead of up. Destard floor 0 (3,1) — reached by klimbing **up** from floor 1 (3,1) — is the measured example.
33ab. **[developer teleport]** Unchanged re-check: during a dungeon fight the teleport must still refuse with "Blocked by active combat", must not freeze, and must work once the fight is over.

**Gate.** 33x is what this batch exists for. If the in-arena `K` does **not** move the party a floor, capture the serial log — that is a real defect. If 33y leaves the party sealed, that is **correct** and the batch passes.

### Phase 6E — Combat FIELD magic gate (Batch 12, 6 min) · *firmware only; the SD card is unchanged*

Flash `build-batch12/openu5_tdeck.bin`. **Do not touch the card** — Batch 12 changes no resource and no production code at all; this phase exists to confirm on glass what the software already proves, and above all to record the "missing fire field" report as **expected behaviour**.

> **Read this before running the steps.** In combat, In \*Grav does **not** build a wall. `CAST.OVL 0x004c`'s combat branch throws a *spell weapon* through the ordinary attack dispatcher — the same one Grav Por and Vas Flam use — and seeds no tile. The fire/poison/sleep/energy fields you are looking for only exist (a) as **authored** units on three dungeon-room boards, and (b) from casting In \*Grav **in a dungeon corridor**, not in an arena. In Zu Grav and In Sanct Grav additionally have `attackValues` of **zero**, so in combat they do nothing whatsoever. All of that is the original's behaviour, cloned on purpose (`docs/bugs-del-original.md` §2.9). **Do not file any of it as a defect.**

Setup: Developer menu → give the party mixed spells and mana (or mix them), and take a caster of level 4+ with at least 4 MP.

33ac. **[In Flam Grav in combat — the reported case]** Start any fight. Cast `In Flam Grav` (`c`, then the runic name). **Expect:** the aim reticle opens on the caster's cell. Move it onto an **enemy** and confirm. **Expect:** the ceremony flash, then an ordinary attack result — a hit line naming the enemy and up to **21** damage, or a miss line. **Expect NO fire tile anywhere, on the target cell or any other.** Now repeat aiming at a **clear square**: flash, nothing else, spell and mana still spent, turn passed. **That is a PASS.**
33ad. **[persistence check has nothing to persist]** Advance two or three turns after 33ac. **Expect:** still no field, and no delayed one appearing. The absence is the whole point; the check is that nothing *arrives* late either.
33ae. **[In Nox Grav in combat]** Same gesture. **Expect:** identical shape, damage up to **18**, no poison tile.
33af. **[In Zu Grav in combat — the zero case]** Same gesture, aimed at an enemy. **Expect:** the mixed spell is spent, mana drops by 4, the turn passes, and the enemy loses **exactly zero** hit points — a graze/miss line at most. No sleep field, no sleep. **This is ticket #91 and it is a PASS.**
33ag. **[where the field actually lives — the dungeon]** Leave combat, stand in a dungeon **corridor** facing an empty floor cell, and cast `In Flam Grav`. **Expect:** a field appears in the single cell **directly ahead** — one cell, not a wall, not a spread — and it stays there while you turn away and come back. Repeat facing a wall or an occupied cell: **expect `Failed!`** and no change. `In Nox Grav` / `In Zu Grav` / `In Sanct Grav` each seed their own colour in the same one-cell shape.
33ah. **[authored arena fields — the other source]** Enter **Deceit room 4** or **Doom room 9** (poison fields, `0xE8`) or **Deceit room 2** (energy, `0xEB`). **Expect:** the field tiles are **visible on the board from the first frame**. Standing a member on a poison field and passing the turn must poison them; the energy tiles must **block** movement and must never damage anyone. Cast `An Grav` on a field cell: **expect `Success!`** and that one tile gone. Cast it on an empty cell: **expect `Failed!`**.
33ai. **[summons — regression guard]** In the same fight, cast `In Bet Xen` (insect swarm) and `Kal Xen Corp` (daemon). **Expect:** both still summon exactly as before — up to four swarm actors from one cast, one daemon that may or may not turn on you. Nothing in this batch touched them.

**Gate.** The pass condition for 33ac–33af is *"no field, and that is correct"*. The real failures to watch for are the opposite ones: if 33ag produces **no** dungeon field, or 33ah shows an authored room with its fields **missing, invisible, or harmless**, capture the serial log — those would be genuine defects, and they are the two places fields are supposed to exist.

### Phase 6F — Dungeon FIELD visibility and dungeon ROOM identity (Batch 12B, 8 min) · *firmware only; the SD card is unchanged*

Flash `build-batch12b/openu5_tdeck.bin`. **Do not re-pack or re-write the card** — both Batch 12B fixes are firmware-side on purpose, and the second one (the arena retag) is done while decoding the pack precisely so an already-flashed card still works.

> **What changed since Phase 6E.** Phase 6E's gate said that if step 33ag produced **no** dungeon field, or 33ah showed an authored room with its fields **missing or invisible**, those would be genuine defects. Both happened on device, and both are now fixed. Phase 6E's combat expectations (33ac–33af: *no* field in an arena, and that is correct) are **unchanged** and are not re-run here.

#### Dungeon field visibility (observation A)

Setup: Developer menu — mixed spells and mana on a level 4+ caster, and a **lit** torch or In Lor. With no light the corridor is black by design (`DUNGEON:0x1AD6`) and this phase proves nothing.

33aj. **[the reported case]** Enter **Destard** and walk to a corridor cell whose **forward** cell is plain, traversable, empty floor. Cast `In Flam Grav`. **Expect:** the log line `Cast` (not `Failed!`), **and** a field now visible in the corridor — a dense field of short horizontal **bright green** sparkle strokes filling the cell directly ahead. It re-draws (shimmers) on every redraw; that motion is the original's own per-redraw re-roll, not a fault.
33ak. **[one cell, not a wall]** **Expect:** only the cell directly ahead is affected. The cells to either side and the cell beyond it are unchanged.
33al. **[turn away and back]** Turn left twice (or use TurnAround) so the field leaves the view, then turn back. **Expect:** the field is still there, in the same cell, still drawn. Then pass a few turns and look again. **Expect:** unchanged — there is no field duration in the reference.
33am. **[the other three spells]** Repeat 33aj with `In Nox Grav`, `In Zu Grav` and `In Sanct Grav`. **Expect:** each seeds its own one-cell field and each is visible. `In Zu Grav` and `In Flam Grav` draw in **EGA 10, bright green**; `In Nox Grav` and `In Sanct Grav` in **EGA 9, bright blue**. Only **two** colours across the four spells is correct and is not a bug — the 1988 palette globals for the four field types are 2·1·2·1, each +8 (`add ax,8` @`0x12b7`). A green fire field looks wrong and is right.
33an. **[the guard still refuses]** Face a **wall** or an already-occupied cell and cast `In Flam Grav`. **Expect:** `Failed!`, nothing drawn, nothing changed.
33ao. **[the field is real, not decoration]** Walk **forward into** the In Flam Grav field. **Expect:** `Fire!!` and hit-point loss, and the field is **still there** afterwards. Do the same with an `In Zu Grav` field: **expect** `Sleep spell!`, party members may fall asleep, and that field **disappears** — a sleep field consumes itself on entry and fire does not. Both are the reference's behaviour.
33ap. **[authored fields — the half nobody could see before]** Enter **Wrong** or **Covetous** and walk the corridors. **Expect:** magic fields appear in the corridor at authored cells you never cast anything into — Wrong ships 30 of them, Covetous 25. Before this batch they were invisible while still being able to sleep, poison or burn you on entry.

#### Dungeon room identity (observation B)

33aq. **[the wrong-room defect]** Enter **Destard** and walk into a **room** cell (high nibble `F` or `A` — the ladder-into-room and corridor-room entries Phase 6E's 33ah used). **Expect:** the board you fight on is a **Destard** board. Before this batch it was Wrong's board of the same room number.
33ar. **[Doom could not fight at all]** Enter **Doom** and step into a room cell. **Expect:** a fight starts. Before this batch the arena lookup returned `MissingMap` and the encounter could not begin — if you see a command-failed/invalid-context line instead of a fight, capture the serial log.
33as. **[the enemy in the void is EXPECTED]** In any dungeon room, you may see one or more enemies standing on **black** cells, including outside the visible wall boundary. **This is correct and must not be filed as a defect.** 50 of the 128 shipped 1988 `.CBT` boards author unit slots on `BlackSquare` — 403 slots in total — and the procedural corridor arena's own monster tables place slots on the ring outside the wall rows. The renderer does not move an actor: it draws it at exactly its own `(x, y)`. What *was* wrong was which board you were standing on (33aq), not where the enemy stood on it.
33at. **[Phase 6E regression]** Re-run 33ah on **Deceit room 4** (poison `0xE8`), **Deceit room 2** (energy `0xEB`) and **Doom room 9** (poison `0xE8`) — now that the room identity is fixed, these are the boards you will actually get. **Expect** exactly what 33ah expected: the field tiles visible from the first frame, poison fields poisoning, energy tiles blocking and never damaging, `An Grav` giving `Success!` on a field cell and `Failed!` on an empty one.

**Gate.** 33aj–33ap must each show a **visible** field; a `Cast` with nothing drawn is the exact defect this batch fixed and would mean a regression. 33aq and 33ar must produce the right dungeon's board and a startable Doom fight. 33as is a **negative** gate: an enemy on a black cell is a pass, not a finding.

### Phase 7 — Quest-critical interaction (3 min)
45. Debug → Quest → grant a shard. **[R-08] `U`se it in a Flame room.** Fixed in Batch 3; hardware confirmation outstanding.
46. `Y`ell a word of power at a dungeon entrance; confirm the quake and the flag toggle.
47. Visit a shrine: Visit → Virtue → Donate. **[R-01] Confirm the game is still playable afterwards.**

### Phase 8 — Persistence sweep (3 min)
48. Save and reload in: overworld, town, dungeon, aboard a ship, mid-quest. After each, confirm: position, party, inventory, equipment, time, transport, world objects, and that the UI mode and renderer match.

### Pass criteria
- Anchors 1–4 each produce a photograph showing correct behaviour.
- No phase ends with the game in a mode that accepts no input.
- No command reports success without a visible, authoritative change.
- The log contains no `UNRESOLVED_NAME`, no `Command failed status=invalid context` for a command the UI offered, and no `PRESENTATION_DISPATCH` whose source contradicts `UI_MODE`.

---

### Phase 6G — (U)se consumable echo and the magic ceremony (Batch 13, 6 min) · *firmware only; the SD card is unchanged*

Flash `build-batch13/openu5_tdeck.bin`. **Do not touch the card** — Batch 13 changes no resource. Every step below is a transcript/screen observation; nothing here needs a save.

Setup: Developer menu — give the party a few of each scroll and potion, mana and level 8 on a caster, and a **lit** torch before the dungeon steps.

33ar. **[the R-21 line itself]** Above ground, `(U)se` → **Vas Lor scroll**. The transcript must read `Item: Scroll` and then `Light!`. It must **not** read `Used Vas Lor Scroll.` — that fabricated line is what `gameplay_parity` mismatch 2034 was reporting for eleven batches. The viewport must invert and flash once (the ceremony).
33as. **[every scroll]** Repeat for the other seven. Each must echo the bare word `Scroll`, then that scroll's own line (`Wind change!`, `Protection!`, `Negate magic!`, `View!`, `Summon Daemon!` + `Not here!`, `Resurrection!`, `Negate time!`). No scroll may ever echo its own name.
33at. **[silent potions print nothing]** `(U)se` a **Blue** potion on a **healthy** member. Expect exactly `Item: Potion` and **no** second line — in particular **not** `No effect!`, which belongs to An Tym's location gate and to nothing else. Then put a member to sleep (Orange potion) and use Blue on them: still only `Potion`, and they wake.
33au. **[authored lines still print]** Poison a member (Green potion), then use **Red** on them. Expect `Item: Potion` then `Poison cured!`. This is the control for 33at — if it is missing, the fix went too far.
33av. **[the ceremony comes first]** Watch the screen during 33au. The flash/inversion must happen **before** the `Poison cured!` line appears, not after. `CAST.OVL 0x139b` fires the ceremony ahead of the reroll at `0x13a1`.
33aw. **[cancelling still spends it]** `(U)se` a potion and **cancel** the "On who" picker. Expect `Item: Potion`, the potion count **down by one**, and **no** flash. That is the original's own behaviour (`0x1394`/`0x1396`), not a bug.
33ax. **[arena mouth]** Start a fight, `(U)se` a scroll and then a potion from inside combat. Same rules: bare `Scroll` / `Potion`, no name echo, no invented `No effect!`.
33ay. **[the dungeon ceremony — new in this batch]** In a **dungeon corridor**, cast an ordinary spell (e.g. `In Lor`). **Expect the same flash/inversion the overworld gives**, which this build raises for the first time. Cast `Grav Por` or `Vas Flam` as the negative control: those are weapon-spells and must stay silent, above ground and below.

**Gate.** 33ar and 33at are the two R-21 observations. If either still shows the old text, the flashed image is not this build. 33au and 33av are the controls that prove the correction removed a fabrication rather than suppressing real output; if 33au prints nothing, stop and report — that would be over-correction, not R-21.

### Phase 6H — the (Z)-stats page family (Batch 14 / R-22, 8 min) · *firmware only; the SD card is unchanged*

Flash `build-batch14/openu5_tdeck.bin`. **Do not touch the card** — Batch 14 changes no resource.

Setup: Developer menu — a party of at least **three** members with **deliberately different** stats (so an aliased field is visible), some gold/food/keys/gems/torches, a few reagents, a couple of mixed spells, and at least **eight** distinct armaments so the Armaments list overflows its seven rows. Equip one member fully (helm, armour, weapon, shield, ring, amulet) and leave another with **nothing** equipped.

34a. **[Z opens the picker, not a page]** In ordinary overworld play press `Z`. The right panel must show the member picker with the roster highlight — `select_player` (0x0000) runs first, exactly as before this batch.
34b. **[confirming opens the sheet]** Move the picker to member 2 and press **Enter**. The panel title must become **that member's name** and the body must be their **Stats** page: `M`/`F`, `Lv-N`, class; the health word centred; then `Str=`/`HP:`, `Int=`/`HM:`, `Dex=`/`Ex:`, and `Magic:`. Before this batch this step merely reopened the picker — if it still does, the flashed image is not this build.
34c. **[the values are that member's]** Compare the numbers against the Developer values you set. `HM` is **max HP** and `Ex` is **experience** — not "magic points" and not the level. Press `1`, then `2`, then `3`: each must show its own member's distinct numbers, not the first member's.
34d. **[Arms page]** From a Stats page, page **right once**. Title stays the member's name, body shows `Arms` and the equipped items **by real name**. Page to the member you left unequipped: it must read `(None ready)`, not six blank rows.
34e. **[real item names, no placeholders]** On the Arms page of the fully-equipped member, read every line. Each must be a genuine item name (`Iron Helm`, `Plate Mail`, `Long Sword`, `Large Shield`, `Ring of Protection`, `Amulet/Turning`). **Nothing may read `Equipment 12`, `Item 30` or similar.** This step is also the visible check on the Batch 14 name-table correction — if a helm shows up named as a shield, or arrows as a bow, the off-by-one is back.
34f. **[Provisions]** Keep paging right past the last member's Arms page. The next page must be titled **`Equipment`** and list `Food:`, `Gold:`, then `Keys.......`, `Gems.......`, `Torches....`. A carried grapple adds a `Grapple` line; without one, that line must be absent entirely. Press `0` from any page to jump straight here.
34g. **[the four lists, in order]** Page right four more times: **Reagents**, **Spells**, **Items**, **Armaments** — in that order. Each row is a 2-digit count, `-`, then the item name.
34h. **[zero counts are absent, not zero]** A reagent you own **none** of must not appear at all. A list with nothing owned must read `(None owned!)`, not an empty box.
34i. **[quest items appear only when owned]** On the **Items** page, confirm the scrolls/potions you carry appear **with** counts, and that any Lord British regalia, Shard, Spyglass, Sextant, Black Badge or Wooden Box you hold appears **without** a number (those are stored `0xff`). Anything you do not own must be absent.
34j. **[list paging]** On **Armaments** with more than seven owned items, the detail line reads `1-7 of N` with a `v` marker. Press **down**: the window scrolls by one and the marker becomes `^ v`. **Left/right** must still change page from inside the list, not scroll it.
34k. **[the ring wraps both ways]** From Armaments, page **right** once: you must land back on **member 1's Stats** page. From member 1's Stats page, page **left** once: you must land on **Armaments**. Paging must **never** show a blank page between the last member and Provisions.
34l. **[navigating never closes]** Page around the whole ring at least twice. The modal must stay open the entire time — only Space and Mic close it.
34m. **[no movement, no turn]** Watch the clock and the viewport while doing 34l. The avatar must **not move** and the **day/time must not advance**. Then press `i`, `p`, `k`, `e`, `s` while the sheet is open: nothing may happen — no torch lit, no gem spent, no prompt armed behind the panel.
34n. **[exit restores gameplay immediately]** Press **Space**. The sheet closes and the normal world HUD returns. The **very next** trackball nudge must move the avatar and advance the clock by one turn. Repeat with the **Mic** key instead of Space — same result.
34o. **[reopen is sane]** Press `Z` again. It must open the **picker** (not the page you were last on), and confirming must open a **Stats** page with no leftover scroll position. Repeat the open/browse/close cycle five times; nothing may drift.
34p. **[abnormal condition]** *(if convenient)* Poison or kill a member via the Developer menu, then view their Stats page. The centred health line must read **`Poisoned`** / **`Dead`** and the rest of the page must still render normally.

**Gate.** 34b, 34e and 34n are the three that matter. 34b proves the page family exists at all (the R-22 symptom was that it did not); 34e proves the equipment names are seated on the right id space; 34n proves the modal hands input back cleanly. If 34m shows the clock advancing, stop and report — the pre-routing interception is not doing its job.

**Expected residuals — do NOT report these as defects** (see §14 Batch 14): a list has no drawn box frame, sex is `M`/`F` rather than `♂`/`♀`, item names are unabbreviated (`Spider Silk`, not `Sp. Silk`), the Items page shows canonical names rather than rune sigils, there is no Pocket Watch row, and there is no `Player: <name>` / `Status:` console echo during selection.

### Phase 6I — the Blackthorn sacrifice roster (Batch 15 / R-23) · **NO HARDWARE STEP OWED** · *firmware only; the SD card is unchanged*

Batch 15 deliberately adds **no** steps to this plan, and that is a finding rather than an omission.

R-23's divergence was only ever reachable on a roster holding **fewer than sixteen character records**. Every `.GAM` the device loads produces exactly sixteen (`persistence.cpp` `import_native`), so on hardware the pre-fix code and the reference computed the identical result. There is nothing a tester could see on a T-Deck that the host suite does not already prove, and inventing a device step for it would spend bench time to observe a no-op.

What *is* still owed for this sequence is unchanged and belongs to the other findings, not to R-23: the capture/interrogation presentation and pacing checks (Y-31/Y-32) and the end-to-end walk of the Blackthorn ceremony itself. Those stay in the consolidated hardware batch.

If a tester happens to run the Blackthorn ceremony while validating something else, one observation is worth recording because it is **correct** and looks wrong: after a companion is executed, the previously-set active character (`1`–`6`) is **not** re-pointed. If it named the executed companion, or anyone after them in the marching order, it now names a different party member. That is `BLCKTHRN 0x03ae-0x04d4`'s own behaviour — the inn (L)eave re-indexes, this routine does not — and it must **not** be filed as a defect. See §14 Batch 15.


### Phase 6J — `quest_parity`'s host crash (Batch 17 / Y-34) · **NO HARDWARE STEP OWED** · *nothing to flash; the firmware binary is byte-identical*

Batch 17 adds **no** steps to this plan, and unlike Phase 6I this is not even a
judgement call.

The batch changed four files, all of them host build inputs:
`native/core/tests/quest_theft_watchdog.h`,
`native/core/tests/batch17_theft_watchdog_test.cpp`,
`native/core/tests/quest_driver.cpp` and `native/core/CMakeLists.txt`. No
production translation unit under `native/core/src` or `native/core/include`
was touched, `quest_driver` is a host-only replay harness that never ships, and
`idf.py -B build-batch17 build` produced an `openu5_tdeck.bin` of `0xd1b10`
bytes — **the same size, from the same sources, as Batch 16's**. There is no
device-observable surface to check.

The one thing a tester should *not* do is re-run any Faulinei theft scenario
looking for a behaviour change. There isn't one: `apply_faulinei_theft` is
unmodified, the unbounded `0x11c7` re-roll is unmodified, and the full 5377-case
`quest_parity` comparison was run to completion **before** the fix (on an `-O0`
driver) specifically to prove the eight-batch crash was hiding zero divergence.
See §14 Batch 17.


### Phase 6K — the wishing-well ceremony and chained-modal prompts (Batch 18 / R-26, R-34) · *firmware only; the SD card is unchanged*

Flash `build-batch18/openu5_tdeck.bin`. **Do not touch the card** — Batch 18 changes no resource file.

The executable rows are **H-17 – H-23** in [`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md). In summary:

* **The well (R-26).** `L`ook at a well: the transcript must read `a well.` and the prompt `Drop a coin?`. `N` echoes **`No`**; **the Mic/Cancel key must do the same** — before this build it was simply ignored and the prompt stayed open. `Y` with gold echoes `Yes` and opens a row prompted **`Thy wish?`** (not "What dost thou wish?"). `Y` with **zero** gold echoes `Yes` and stops **silently** — there is no `Thou hast no coin!` in the binary, and printing one would be the regression.
* **Chained modals (R-34) — the general check, and the one that matters most.** Any modal whose answer opens another modal must now show the second modal's prompt text: the shrine's `Virtue?` then `Mantra?`, a Blackthorn interrogation round's `Your response?`, the Rel Hur/skull-key `Direction?` row, and the `(U)se`-a-potion `On whom?` picker. **Before this build every one of those rendered with an empty prompt row.** If any still does, the flashed image is not this build.

**Gate.** H-19 (Cancel means No) and H-23 (chained prompts visible) are the two observations that cannot be made any other way. H-21 is the over-correction control: if it prints *anything* after `Yes`, stop and report.

---

### Phase 6L — the command-character picker (Batch 19 / R-25) · *firmware only; the SD card is unchanged*

Flash `build-batch19/openu5_tdeck.bin`. **Do not touch the card** — Batch 19 changes no resource file.

The executable rows are **H-50** (rewritten) and **H-141 – H-145** in [`ALPHA2_HARDWARE_CHECKLIST.md`](ALPHA2_HARDWARE_CHECKLIST.md). In summary:

* **The crystal ball works at all (R-25).** `L`ook at a crystal ball with no active player and two or more healthy members: a **`Player: `** roster picker opens. The old `Peer into it?` yes/no is deleted — if it still appears, the flashed image is not this build. Picking a high-INT member gives `Strange vision!` and the 32×32 gem map; **the gem count must not change and closing must charge no turn**. A low-INT member gives `Death vision!` and **exactly 1 HP off that member** — not member 0, and not the active player if they differ.
* **The branches that must NOT ask.** With an active player set, or with exactly one `G`/`P` member left, the vision fires **immediately, with no prompt**. With zero `G`/`P` members the transcript reads **`None!`** and nothing else happens.
* **The re-ask.** Picking a dead/disabled member prints **`Disabled!`** and brings the **same `Player: ` prompt back**. That is a re-ask, not a rejection: if the prompt closes, or the vision runs on the dead member, that is the regression.
* **The other two callers.** `S`earch and `C`ast now raise the same picker — `S` **after** the direction, `C` **before** the spell list. Cancelling either prints `None!` and abandons the command. With an active player set, neither asks.

**Gate.** H-50 plus H-141 is the pair that proves R-25 is actually closed on device rather than merely on host. H-143 (the `Disabled!` re-ask) and H-145 (`S`/`C` order) are the two observations no host test can make, because they are about what the player sees and in what order.


---

## APPENDIX — Audit artifacts

**Test run (ORIGINAL AUDIT BASELINE RUN, pre-Batch-1):** `ctest` in `native/core/build-alpha20-host`, 57 tests, **56 passed / 1 failed** (`gameplay_parity`, 61.9 s). Mismatch artifact retained at `native/core/build-gameplay/mismatch.json`. This run predates Batch 1 and is preserved as historical evidence. At this point in the program, mismatch 2034 (R-21) had not yet been observed — it was unreachable behind mismatch 59 — and this record is preserved as-is rather than rewritten with knowledge that did not exist at the time.

**Status (post-Batch-1, historical):** 58 total, 57 pass, 1 fail — `gameplay_parity` failing at mismatch 59, plus the new `ui_mode_regression` suite (28/28 GREEN). See §14 Batch 1. As with the baseline run above, mismatch 2034 was not yet reachable or known at this point and this record is preserved unchanged.

**Historical status (post-Batch-2, superseded):** 58 total, **57 pass, 1 fail**. R-02, R-03 and R-04 are GREEN; `gameplay_parity` mismatch 59 is **fixed**. The sole remaining failure is `gameplay_parity` at **mismatch 2034 (R-21)** — a scroll-use event/message/SFX divergence that was masked by mismatch 59 until now, confirmed pre-existing (reproduces on the untouched `63eeac3b` baseline) and out of scope for Batch 2. `ui_mode_regression` (28/28 GREEN, Batch 1) and all Batch 2 regressions (`presentation_regression`, `combat_loot_open_regression`, `combat_escape_regression`, `direct_troll_handoff_regression`) pass. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**. See §14 Batch 2 and §3 R-21. This entry is preserved as historical record of the program's state at that point and is superseded by Batch 3 below; it is not rewritten with later knowledge.

**Status (post-Batch-3):** the host ctest suite is substantially larger than the post-Batch-2 snapshot above (61 registered tests in the authoritative `native/core/build-batch1-control` acceptance tree, up from 58), reflecting Batch 3's new `batch3_group_b`/`batch3_group_c` suites and the TypeScript drift/fixture tests. (A stale, non-authoritative `OPENU5_REAL_ARENAS=ON` build tree was briefly observed reporting 65 tests during this window; that count came from a build configuration different from the Batch acceptance tree and was never the authoritative figure — the number to cite for Batch acceptance is always the `build-batch1-control` count.) R-06 (`Ready` rejected in combat/dungeons), R-07 (Use-picker id 18 mis-binding), R-08 (endgame Use chain unreachable), R-19 (`(Y)ell` has no frigate branch) and R-20 (harpsichord melody unreachable) are all **GREEN — RESOLVED (Batch 3)**; see §3 for each. `CombatYield` is confirmed live and routed end-to-end through the real `execute_command()`/`combat.cpp` path, not just emitted by `UiSession` (§15 invariant 21; `native/core/tests/batch3_group_c_test.cpp` C13). The sole known failure remains `gameplay_parity` at mismatch 2034 (R-21, still RED/OPEN — untouched by Batch 3, tracked as Batch 12). Batch 3 did change production code (§14 Batch 3 file list), and T-Deck ESP-IDF 6.1 firmware was rebuilt and passed as part of that same Batch 3 validation: `openu5_tdeck.bin` was approximately `0xc63f0` bytes, with 23% of the app partition free and zero compiler warnings. Hardware flash was not performed at that Batch 3 validation point. See §3 R-06/R-07/R-08/R-19/R-20/R-21 and §14 Batch 3.

**Status (pre-Batch-4 Developer-tool cleanup, historical, superseded by Batch 4 below):** Batch 3 is committed and pushed to `main`. The work-in-progress on top of it at that point was a small, self-contained Developer-tool correction — **R-24** (`DebugPreset::Combat` no longer silently engages Set Active Player) — plus its regression coverage, three new evidence-only audit findings (R-22 Z-stats, R-23 Blackthorn roster gap, Y-29 physical clear-active-player hardware reachability), and this document's own text corrections. No gameplay behavior other than the R-24 Developer-preset fix changed. The authoritative `native/core/build-batch1-control` suite was **61 total, 60 pass, 1 fail** — the sole failure was `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing). T-Deck ESP-IDF firmware was rebuilt clean for this pass because `DebugPreset::Combat`'s production behavior changed. See §3 R-24 for the fix and evidence.

**Status (post-Batch-4 implementation, historical, superseded by the harness-correction pass below):** R-09 and R-10 became **GREEN — RESOLVED**; see §3 for the full analysis, including the correction of the original R-09 write-up (severity was understated; the `CrystalBall`/`WellDrop` "correctly handled" claims were wrong and are now tracked as R-25/R-26, discovered but explicitly left unfixed). The authoritative suite grew from 61 to **63** registered tests and, at that point, reported 63 total, 60 pass, 3 fail: `gameplay_parity` mismatch 2034 (R-21, unchanged, pre-existing) plus one internal defect each in `batch4_group_a` (its A8 case) and `batch4_group_b` (one check in B5) — both test-authoring defects, not production regressions (see §7 addendum and §3 R-09 as originally written). A pass mid-implementation briefly introduced a real regression — `quest_parity` mismatch 4821 — by placing the Blackthorn "Your response?" prompt suffix in the core event text instead of `UiSession`'s presentation layer; this was caught by the parity fixture itself, reverted, and re-implemented presentation-side. T-Deck ESP-IDF 6.1 firmware was rebuilt and **passed**: `openu5_tdeck.bin` grew from `0xc63f0` to `0xc67c0` bytes (+976 bytes), app partition free space `23%` → `22%`, zero compiler warnings.

**Status (post-Batch-4 harness-correction pass):** A8 and B5 were independently re-adjudicated from the production semantics and test code, confirmed as test-authoring defects (not assumed from the prior report), and corrected in `native/core/tests/batch4_group_a_test.cpp` / `batch4_group_b_test.cpp` only — no production code changed, so the firmware certification above remains valid unchanged. Both tests now prove the real invariants without weakening coverage: A8 drives a real trigger + real modal answer for all four Blackthorn/guard families before asserting the world unblocks (Blackthorn's case adds a minimal content-free `ShrineServices` test fixture so the real state machine can conclude on host); B5 no longer asserts a state production never produces (routing through `UiSession::consume()` while expecting the session to stay inactive) and instead proves the identity-lifetime invariant end-to-end: capture, survive, re-resolve without the borrowed pointer, and successfully open the correct session via `BeginConversation`. The authoritative suite now reports **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `batch4_group_a` is 32/32 and `batch4_group_b` is 12/12. See §3 R-09/R-10/R-25/R-26 and §14 Batch 4.

**Probe:** a throwaway program linked against `libopenu5_core.a` verified R-06 and R-16 directly. It lives in the session scratchpad, **not** in the repo — it is an audit instrument, not a test. Its R-06 assertions are now carried permanently by `native/core/tests/batch3_group_c_test.cpp` (invariant 22, §15); R-16's are now carried permanently by `native/core/tests/display_names_test.cpp` (Batch 8) instead of remaining probe-only.

**Working tree, historical (Batch 3, committed/pushed):** at the time of that document update, the tree contained the completed Batch 3 implementation and tests — production changes in `native/core/include/openu5/combat.h`, `native/core/include/openu5/inventory_picker.h`, `native/core/include/openu5/ui_session.h`, `native/core/src/combat.cpp`, `native/core/src/commands.cpp`, `native/core/src/display_names.cpp`, `native/core/src/inventory_picker.cpp`, `native/core/src/ui_session.cpp`, `native/targets/tdeck/main/alpha_runtime.cpp` and `native/targets/tdeck/main/alpha_runtime.h`; test changes in `native/core/tests/batch3_group_b_test.cpp` and `native/core/tests/batch3_group_c_test.cpp` (the latter now also carries the C13 real-routing `CombatYield` regression guard) — plus that document update. That Batch 3 tree has since been committed and pushed to `main`.

**Working tree (current, pre-Batch-4 Developer-tool cleanup, not committed):** on top of the committed/pushed Batch 3 baseline, the current uncommitted working tree contains only the small R-24 Developer-tool fix and its supporting changes: `native/core/src/debug_developer.cpp` (production fix), `native/core/tests/debug_developer_test.cpp` (regression coverage), `native/core/src/turn.cpp` (a documentation-only comment on the reference-faithful troll dexterity check; no logic changed), and this document (R-22/R-23/R-24/Y-29 findings, Batch 13 plan entry, and the text corrections in this section). No other production code was modified.

**Status (post-Batch-4.5A-1, Developer Teleport Default Entrance fix):** R-27 became **GREEN — RESOLVED**; see §3 R-27 for the full root cause, fix, and per-location evidence. The authoritative `native/core/build-batch1-control` suite reports **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `debug_map_picker` is 62/62 and `ui_debug_menu` is 23/23, both fully GREEN; no Developer-tool test failures remain. T-Deck ESP-IDF 6.1 firmware was rebuilt in a fresh tree (`native/targets/tdeck/build-batch45a1-green`) because this pass changes production/device-facing code, and **passed**: `openu5_tdeck.bin` is `0xc6840` (813,120) bytes, up from the prior recorded `0xc67c0` (+128 bytes), with 22% of the app partition free and zero compiler warnings (one transient GCC internal-compiler-error segfault in an unrelated ESP-IDF file, `esp_lcd_panel_rgb.c`, occurred on the first build attempt and cleared on retry — not caused by this pass's changes). Hardware flash was not performed. See §3 R-27 for the full analysis.

**Status (post-Batch-4.5A-2, Developer UI label/view-model seam):** this closing status paragraph was not recorded at the time (see R-28 in §3 for the full contemporaneous record of that batch's fix, regression coverage, and 64/63/1 host-suite result); it is not reconstructed retroactively here.

**Status (post-Batch-4.5A-3, Special Items + Developer capability wiring):** R-29 became **GREEN — RESOLVED**; see §3 R-29 for the full root cause, fix, and regression coverage. This session's host verification ran under a temporary GCC 13/CMake/Make toolchain and ESP-IDF 6.1 cloned fresh into `C:\Users\lawma\.platformio\packages\framework-espidf-6.1` (the project's normal Zig host compiler and pre-installed ESP-IDF/xtensa toolchain were not present in this sandboxed session and were provisioned during this pass). The four Developer-tool suites are `debug_labels` **170/170** (up from 124), `ui_debug_menu` **196/196** (up from 146), `debug_developer` **50/50** (up from 46), and `debug_map_picker` **62/62** (unchanged); a further 35 non-Developer-tool host tests passed **35/35** with zero regressions. The Node/TypeScript parity/drift harness (including `gameplay_parity`, the sole authoritative-suite failure at mismatch 2034/R-21) was not re-run in this session (Node was not installed in the verification environment) — the touched files (`debug_labels.h/.cpp`, `ui_debug_menu.h/.cpp`, and their three test files) have no TypeScript-fixture dependents, so the authoritative suite is expected to remain **64 total, 63 pass, 1 fail** (sole R-21, unchanged), but this has not been directly re-confirmed by running that exact suite and should be spot-checked in the project's normal environment. T-Deck ESP-IDF 6.1 firmware was rebuilt in a fresh tree (`native/targets/tdeck/build-batch45a3-green`) because this pass changes production/device-facing code, and **passed**: `openu5_tdeck.bin` is `0xd3760` (866,144) bytes, with 17% (182,432 bytes) of the 1 MiB app partition free and zero compiler warnings. Hardware flash was not performed. See §3 R-29 for the full analysis.

**Status (Batch 4.5B hardware correction — presentation/device seam, narrow, R-23 out of scope):** Batch 4.5B's resource wiring (MISCMSG/shrine records reaching `blackthorn_action`) is confirmed **GREEN** by `native/core/tests/batch45b_test.cpp`, unchanged in this pass. A subsequent hardware retest on real T-Deck surfaced two presentation-layer symptoms once the narrative started rendering: (1) the interrogation narrative/question text was concatenated into `UiSession`'s modal prompt line (`enter_modal`'s `kUiPromptBytes` = 96-byte `prompt_` buffer) instead of the transcript, so long questions were silently truncated by `copy_text`; and (2) the party appeared to remain visually in the pre-capture Palace lobby through the interrogation and its wrong-answer consequence. Root-cause tracing of (1) is direct and fixed here: `UiSession::consume()`'s `GameEventKind::BlackthornPrompt` case in `native/core/src/ui_session.cpp` now `append()`s the narrative to the transcript (`UiTextChannel::Quest`, matching `GuardArrestPrompt`'s established pattern) and calls `begin_text()` with only the short "Your response?" cue, with a new host-test case in `native/core/tests/ui_session_test.cpp` driving a question longer than 95 characters through the real event path and asserting both halves land where they should. `blackthorn.cpp`'s core state machine was not touched (matches `batch45b`/`batch4_group_a` A7's already-established, reference-faithful event ordering: no relocation is emitted before or during the interrogation itself — `deposit()` only runs at the immediate-subdual branch or after the interrogation resolves, exactly as `game/src/core/world/blackthorn-capture.ts`'s `finishCaptureDeposit` comment documents for the reference). For (2), tracing `UiSession::finish_modal()` → `AlphaRuntime::dispatch()`/`command()` → `blackthorn_action()`'s `deposit()` → `AlphaRuntime::handle()`'s unconditional `synchronize_after_debug()` (terrain refresh, `resolve_synchronized_base_mode()` rebind, forced `dirty_=true`) did not turn up a synchronization defect: `game_.position` is mutated synchronously before any render, and every input already forces a terrain refresh and full redraw regardless of which command ran. `game/assets/maps/smallmaps.json` also already carries a floor `-1` entry for location 18 (the capture's `deposit()` destination), so the render's `get_active_map()` lookup should resolve. No RED reproduced at this seam with the evidence available in this session, so nothing was changed in `AlphaRuntime`'s synchronization logic (rewriting an unverified hypothesis here was judged higher-risk than leaving it alone). One defensive, zero-behavior-change diagnostic was added: `AlphaRuntime::render()`'s existing silent `return ESP_FAIL` when `get_active_map()` finds no map for the current `{location,floor}` now logs `WORLD_MAP_MISSING` with the position first, so a hardware retest that still shows a stale scene will show in the device log exactly whether a missing/unbuilt map is the cause versus something else. **This session had no host C++ compiler available** (no MSVC, no MinGW g++/clang) — only the ESP-IDF 6.1 / xtensa-esp32s3 cross toolchain and CMake/Ninja already provisioned at `C:\esp\v6.1\esp-idf` and `C:\Users\lawma\.platformio\packages` were present. The `native/core` **host** test suite (`batch45b`, `ui_session`, the `batch4_group_*` suites, and the authoritative `native/core/build-batch1-control` ctest tree) links and runs as native x86_64 executables and therefore **could not be built or run in this session**; its counts are not verified here and must be run in the project's normal host build environment before this fix is trusted. The **T-Deck firmware**, which only needs the xtensa cross toolchain, was built fresh in this session into `native/targets/tdeck/build-batch45b-presentation-fix` using a real ESP-IDF 6.1/xtensa-esp32s3 toolchain found already provisioned on this machine (`C:\esp\v6.1\esp-idf`, `C:\Users\lawma\.platformio\packages\tool-cmake`/`tool-ninja`), and **passed**: `openu5_tdeck.bin` is `0xc8a50` (821,840) bytes, with 22% (0x375b0 bytes) of the 1 MiB app partition free, and zero compiler warnings from any project file (the only warnings in the log are five pre-existing, unrelated ESP-IDF `component_validation.cmake` notices about `esp_wifi`/`wpa_supplicant` private-include usage). Hardware flash was not performed (no physical device attached to this session). Files touched: `native/core/src/ui_session.cpp`, `native/core/tests/ui_session_test.cpp` (production fix + regression coverage, unverified — no host compiler), `native/targets/tdeck/main/alpha_runtime.cpp` (diagnostic logging only), and this document. R-23 (roster compaction) remains untouched and open, as scoped.

**Status (post-Batch-4.5C, scrollable gameplay transcript + Blackthorn/Gorn audits):** R-31 became **GREEN — RESOLVED**; see §3 R-31 for the full root cause (transcript auto-scroll forcibly resetting to newest on every event, plus a fixed-page-size mismatch with the actual on-screen text-size/context-bar geometry), fix, and regression coverage. This session had a full host toolchain available (`C:\Dev\TamaPoke\.build-tools\w64devkit` g++/ninja) and ran the complete authoritative `ctest` suite, unlike the two prior Batch 4.5B sessions that could not: **66 total, 65 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged), exactly matching the baseline going into this batch. `ui_session_tests` alone is **271/271** (up from 261), adding seven RED-then-GREEN cases (T1-T7) driven through real dialogue and `BlackthornPrompt` event paths, confirmed RED against the unfixed source before the fix landed. T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch45c-transcript` and **passed**: `openu5_tdeck.bin` is `0xc8b90` (822,160) bytes, up from the prior recorded `0xc8a50` (+320 bytes), with 22% (`0x37470` bytes) of the 1 MiB app partition free and zero compiler warnings from any project file. Hardware flash was not performed (no physical device attached). The resource pack (`native/assets/openu5-alpha1-resources.bin`, 1,843,143 bytes, SHA-256 `0ac80e41be1b2b3ae20966730cd775347c5ac3f2cfe4f1da8ad6eb8ee02c819b`) was **not** modified this session; no SD recopy is required. Separately, this session performed two **read-only** fidelity audits per its instructions, adding **R-32** (Blackthorn capture/interrogation has no distinct scene presentation on the T-Deck — confirmed a real, sourced divergence from the original DOS staging and from the TypeScript reference's own dedicated scene layer; phase-by-phase comparison and a scoped, unimplemented 4.5D proposal recorded in §3 R-32) and **R-33** (the brazier/Gorn "Nothing of note" hardware finding is a floor-representation bug — raw DATA.OVL byte 255 vs. the runtime `-1` basement convention — present identically in the TypeScript reference and the native port, affecting 5 known `searchObjects` entries; classified in §3 R-33 as a separate, previously-uncaught reference-engine defect, not fixed in this pass per the instruction to prefer report-only unless a fix is a provably one-line packing omission, which this is not). Files touched: `native/core/include/openu5/ui_session.h`, `native/core/src/ui_session.cpp`, `native/core/tests/ui_session_test.cpp`, `native/core/UI_SESSION.md`, `native/targets/tdeck/main/alpha_runtime.cpp`, `native/targets/tdeck/main/tdeck_board.cpp`, `native/targets/tdeck/main/tdeck_board.h`, and this document. No production code for R-32/R-33 was touched, as scoped — both remain open, tracked findings.

**Status (post-Batch-4.5C.1, basement search-object floor normalization):** R-33 became **GREEN — RESOLVED**; see §3 R-33 for the full root cause, the dual-engine fix (`decode_authored_floor`/`decodeAuthoredFloor`, a single shared comparison-boundary helper in each engine rather than a scattered `255 || -1` check, with an explicit two's-complement rule that leaves the WORLD map's own floor-255-means-Underworld convention untouched), and regression coverage. The authoritative `ctest` suite grew from 66 to **67** registered tests (the new `quest_search` target) and reports **67 total, 66 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged); `quest_parity` (23 s) stayed GREEN, confirming the native/TypeScript fix introduced no new engine divergence anywhere in the existing scripted quest suites. `quest_search_tests` is **15/15** (new), confirmed RED against the pre-fix source (via a temporary `git stash` of just the two native fix files) before the fix landed, then GREEN after. `game/tests/search.test.ts` grew from 24 to **25/25** (one new Underworld-convention guard; seven pre-existing assertions that queried the Blackthorn jail with the raw, unnormalized floor `255` were corrected to the real runtime floor `-1` to match the fix, since 255 was never an achievable real `GameState.position.floor` value there); the full TypeScript `vitest` suite's ~98 pre-existing failures (sandbox symlink permissions, a hardcoded-path lint check, and similar environment-only issues, none touching `search`/`quest`/`blackthorn`) are unchanged before and after. T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch45c1-search-floor` and **passed**: `openu5_tdeck.bin` is `0xc8bc0` (822,208) bytes, up from the prior recorded `0xc8b90` (+48 bytes), with 22% (`0x37440` bytes) of the 1 MiB app partition free and zero compiler warnings from any project file. Hardware flash was not performed (no physical device attached). The resource pack (`native/assets/openu5-alpha1-resources.bin`) was **not** modified — the fix normalizes the floor value at the comparison boundary inside `search_at()`/`searchAt()`, not the packed/authored data itself, so no re-pack or SD recopy is needed. R-14 (world-object save/load persistence) was explicitly **not** touched or marked resolved. R-32 (Blackthorn scene presentation fidelity) remains open and unimplemented, scoped for a future 4.5D pass, untouched by this fix. Files touched: `native/core/include/openu5/quest_world.h`, `native/core/src/quest_search.cpp`, `native/core/tests/quest_search_test.cpp` (new), `native/core/CMakeLists.txt`, `game/src/core/world/search.ts`, `game/tests/search.test.ts`, and this document.

**Status (post-Batch-4.5D, Blackthorn capture/interrogation scene presentation):** R-32 became **GREEN — RESOLVED**; see §3 R-32 for the reconstructed reference phase table, the exact native divergence, the implemented architecture, the pacing behaviour and the one declared fidelity compromise. The authoritative `ctest` suite grew from 67 to **68** registered tests (the new `blackthorn_scene` target) and reports **68 total, 67 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged, re-confirmed verbatim this session). Targeted suites: `blackthorn_scene` 57/57 (new), `batch4_group_a` 32/32, `batch4_group_b` 12/12, `batch45b` 28/28, `ui_session` 271/271, `input_regression` pass, `ui_mode_regression` 28/28, `presentation_regression` pass (snapshot=1100, actor-clock=1036), `quest_parity` GREEN (37 s), `quest_search` 15/15 — confirming the additive scene events introduced no native/TypeScript divergence in any existing scripted suite. The resource pack **changed**: `native/assets/openu5-alpha1-resources.bin` is now 1,843,457 bytes (was 1,843,143), 34 entries (was 33), payload CRC32 `0x2b1449f4`, SHA-256 `1a5b5a402ec9480ed8ee5963c7fbf1e04470f10cbfeedd5dfc69d9b9f50fa573`, because the throne-room grid (`blackthorn-scene.bin`) is packed for the first time — **an SD recopy to `<SD>:\ultima5\openu5-alpha1-resources.bin` is required before flashing.** T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch45d-blackthorn-scene`. Hardware flash was not performed in this session (no physical device attached); the hardware micro-check is the eleven-step sequence in this batch's instructions. R-23 (sacrifice roster compaction) and R-14 (world-object persistence) were explicitly **not** touched or marked resolved. Files touched: `native/core/include/openu5/blackthorn_scene.h` (new), `native/core/src/blackthorn_scene.cpp` (new), `native/core/include/openu5/movement.h`, `native/core/include/openu5/commands.h`, `native/core/include/openu5/blackthorn.h`, `native/core/src/blackthorn.cpp`, `native/core/src/ui_session.cpp`, `native/core/sources.cmake`, `native/core/CMakeLists.txt`, `native/core/tests/blackthorn_scene_test.cpp` (new), `native/tools/u5pack/alpha1.ts`, `native/targets/tdeck/main/alpha_resources.h`, `native/targets/tdeck/main/alpha_resources.cpp`, `native/targets/tdeck/main/alpha_runtime.h`, `native/targets/tdeck/main/alpha_runtime.cpp`, and this document. `native/assets/openu5-alpha1-resources.bin` was regenerated by `npm run pack:alpha1` but is `.gitignore`d (it is derived from the user's own original assets), so it is not part of the commit -- rebuild it locally and recopy it to the SD card, exactly as the pack has always been distributed.

**Status (post-Batch-7, missing feedback renderers, partial):** R-12 and R-13 became **GREEN — RESOLVED**; see §3 for each. Y-04 is **partially resolved**: `Quake` now has a real, reference-derived consumer, but `CellExplosion`, `CellProjectile`, `PoisonTick`, `Refuge` and `TrollSneak` remain open, each needing more than this batch's scope justified rushing (see the "Batch 7 continued" plan entry in §14 for the concrete reason per channel). The plan's original "risk: low" / single-template framing was corrected during adjudication before any production code was written, per this batch's own read-first instruction -- R-12 and Quake needed real, reference-derived fixes (a censorship bypass and a witness-calibrated waveform, respectively), not a bare copy of `start_magic_ceremony`. The authoritative `native/core/build-batch1-control` suite reports **69 total, 68 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged, re-confirmed this session). `presentation_regression` carries all new coverage (the `reveal_all` bypass and the `quake_offset_at` waveform), both confirmed RED (compile failure against pre-fix source, via `git stash` of only `presentation.h`/`presentation.cpp`) before GREEN. T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch7-feedback-renderers` and **passed**: `openu5_tdeck.bin` is `0xccad0` (836,816) bytes, with 20% (`0x33530` bytes) of the 1 MiB app partition free and zero compiler warnings from any project file. Hardware flash was not performed (no physical device attached to this session); the physical checkpoints from §14 Batch 7 remain outstanding. The resource pack was **not** modified this session; no SD recopy is required. Files touched: `native/core/include/openu5/presentation.h`, `native/core/src/presentation.cpp`, `native/core/tests/presentation_test.cpp`, `native/targets/tdeck/main/alpha_runtime.h`, `native/targets/tdeck/main/alpha_runtime.cpp`, `native/targets/tdeck/main/native_renderer.h`, `native/targets/tdeck/main/native_renderer.cpp`, and this document.

**Status (post-Batch-12, combat field magic — adjudicated, no production change):** The hardware report that In Flam Grav / In Nox Grav / In Zu Grav "cast, flash, and leave no persistent field on the battlefield" was traced to the reference before any code was touched, and it is **the original's own behaviour, deliberately cloned** — `CAST.OVL cast_field_wall` 0x004c seeds a field only in its DUNGEON branch (the sole reader of `DS:0x4596`); its COMBAT branch loads a spell-weapon id from `DS:0x4592` and calls `COMSUBS:0x0c52`, the same dispatcher Grav Por / Vas Flam / Xen Corp use, with `attackValues` 18 / 0 / 21 / 0. See §14 "Batch 12 (combat-field magic lane)" for the six-axis evidence and `docs/bugs-del-original.md` §2.9 / ticket #91. **No production file was modified**: the batch ships the adjudication plus the host coverage that was missing, `native/core/tests/batch12_combat_field_test.cpp` (new, registered as the `batch12_combat_field` CTest). Because a first-run-green guard proves nothing, it was validated by mutating production code twice and reverting — seeding a field in `combat_cast` turns it RED (10 failures) and giving In Zu Grav / In Sanct Grav real damage turns it RED (1 failure); `git status` confirms `native/core/src/` unmodified afterwards. The authoritative suite grew from 78 to **79** registered tests and reports **79 total, 77 pass, 2 fail** — the identical pair the pre-batch baseline run produced on this same tree: `gameplay_parity` (R-21, "Gameplay mismatch 2034", deferred by instruction and untouched) and `quest_parity` (`STATUS_ACCESS_VIOLATION`, exit `3221225477`, the MinGW/w64devkit environment finding already recorded in Batch 8's status). All magic and combat suites are green, `magic_parity`, `typescript_magic_fixture_drift`, `combat_parity` and `advanced_combat_parity` included. T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch12` and **passed**: `openu5_tdeck.bin` is `0xd0430` bytes with 19% of the 1 MiB app partition free — byte-for-byte the same size as Batch 11, as a batch that changes no production code must be. Hardware flash was not performed; the device checklist is §16 Phase 6E. One non-blocking finding recorded but **not** fixed (it changes no behaviour and this batch touches no production code): `AlphaRuntime::combat_fields_` is allocated and assigned once at boot, then orphaned by the first `initialize_combat`, which restores only the actor and pile overflow pointers. R-21 was **not** investigated, as instructed. Files touched: `native/core/tests/batch12_combat_field_test.cpp` (new), `native/core/CMakeLists.txt`, and this document.

**Status (post-Batch-14, R-22 implemented — the `(Z)`-stats page family):** R-22 — filed evidence-only during the pre-Batch-4 Developer-tool cleanup and open since — is **GREEN — RESOLVED**. Native `(Z)` had only the member picker: confirming a member reopened the very same picker, so no status or inventory page existed at all. The page family was adjudicated from `ZSTATS.OVL` and the DATA.OVL DGROUP before any code was written, and two descriptions previously recorded in this document were found wrong and corrected: the odd per-member page is **Arms only** (`draw_arms_page` 0x02a8 paints the six equipment slots and no spellbook), and list `0xe` is **Spells** (the mixture counts at `0x57f0`), not a second item page. What shipped is the real 17-slot axis of `cmd_zstats` (0x0a3a) — per-member Stats and Arms, Provisions, and the Reagents/Spells/Items/Armaments lists — with the binary's circular ring and its four wrap gates, its `'0'`–`'6'` jumps bounded by `g_party_size` (0x0b12/0x0b37), `render_item_list`'s seven content rows and its scroll-vs-page sub-loop (0x07d0), and Space/ESC as the **only** keys that close (0x0a78/0x0a81). The model lives in a new ESP-free seam, `openu5/zstats.h` + `src/zstats.cpp`, which writes nothing, reads no clock and draws no RNG — matching `cmd_zstats`, which consumes exactly zero rolls and charges no turn — and whose Items page **reuses `usable_item_picker_rows()`** because the original reads one extended table (0xB9EE) for both the `(Z)` Items page and the `(U)se` picker. `AlphaRuntime` owns the modal and intercepts input **before any routing**, beside the gem and zodiac views, so a direction can never reach `dispatch_world_command()` as a Move. Implementing it exposed one genuine production defect outside the Z-stats path, fixed narrowly: **`equipment_display_name()` was off by one for all 48 ids**, because its table had been transcribed from `InventoryDetails.json`'s `Armament` map with its phantom `[0] = "BareHands"` entry (the reference compensates with `.slice(1)`; native did not). Three anchors inside native itself pin the correct alignment — the armour band `equip_type_of(9..15) == 0x40`, `ammo_item_for()`'s 26/36→27 and 28→29, and `is_thrown_weapon()`'s `{16, 21, 22}` — so `equipment_display_name(16)` was returning "Mystic Armour" for the Dagger and `(27)` "Bow" for the Arrows, in the Ready picker and the blacksmith as well as here. The table is now re-seated onto the binary's own id space (the byte-exact DS 0x17f6 dump), restoring the missing 48th entry **Ankh**, with a `static_assert` pinning the count; `display_names_test.cpp`'s three anchors, which had encoded the off-by-one, were re-seated with it. Both new suites were written first and run RED against an inert seam that reproduced pre-batch behaviour exactly: `batch14_zstats_model` **96 checks / 86 failures → 97 / 0**, `batch14_zstats_runtime` **54 / 33 → 54 / 0**; because several runtime greens were vacuous while no modal existed, deleting the single pre-routing interception line in `handle()` was mutation-checked and turns **27 of 54** RED, then reverted. Fifteen row-spacing assertions were adjudicated against the DGROUP strings rather than forced to match — the trailing space in `" Food: "`/`" Gold: "` (0x972e/0x9738) was a production bug and was fixed, while the stat-column and 16-cell centring expectations were test errors. The authoritative suite went from **81 total / 80 pass / 1 fail** to **83 total / 82 pass / 1 fail** (the +2 is the two new targets); `gameplay_parity` remains 5058/5058 GREEN and the sole failure is still `quest_parity`'s MinGW/w64devkit `STATUS_ACCESS_VIOLATION` (exit `3221225477`) from Batch 8, present identically in this batch's own pre-edit baseline run. The resource pack is **unchanged**, so the flashed SD card needs no repack. T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch14`: `openu5_tdeck.bin` `0xd19e0` = **858,592 bytes**, **+4,848** over Batch 13, **189,984 bytes (18%) free**, **zero project warnings**. Hardware flash was not performed in this session; the device checklist is §16 Phase 6H, and the six expected presentation residuals (no drawn list frame, `M`/`F` instead of `♂`/`♀`, unabbreviated item names, canonical names instead of rune sigils, no Pocket Watch row, no `Player:`/`Status:` console echo) are listed there so a tester does not file them as defects. **R-23 (Blackthorn roster compaction), Y-29 (Mic/`0` input reachability), Y-31 and Y-32 (pacing/presentation) and the `quest_parity` harness crash were explicitly not touched and all remain open.**

**Status (post-Batch-15, R-23 resolved — the Blackthorn sacrifice roster compaction):** R-23 — filed evidence-only during the pre-Batch-4 Developer-tool cleanup and open ever since, explicitly skipped by Batches 4.5B, 4.5D and 14 — is **GREEN — RESOLVED**. It was adjudicated from `BLCKTHRN.OVL 0x0438` `sacrifice_member` before any code was written, and the open finding turned out to be **real but mis-described on three counts**, all corrected in §3 R-23 and §14 Batch 15. (1) The immediate artefact was a **duplicate**, not a blank: native's shift stopped at the old `character_count`, so the roster's last record appeared twice at slot `count-1` while `[count, 15)` was never compacted at all. (2) It was **never reachable on the device**: every `.GAM` yields exactly sixteen records (`persistence.cpp` `import_native`), and at `character_count == 16` the old bound and the reference bound `0x04ab-0x04c0`'s run-to-`si==0x57a8` are the same expression — which is also why no parity suite caught it, since `quest_parity`'s Blackthorn scenarios build a full 16-record roster (`tools/check-quests.ts:174`). The divergence was live only on rosters shorter than sixteen records, which `save::deserialize`/`restore_core` accept. (3) Forcing `character_count = 16` was **not** part of the defect — it mirrors the reference's fixed sixteen-record table and the port's own `characters[15] = rec`. The fix is one bound: the compaction loop now runs to `kRosterCapacity`. Victim selection (the **second living** member, not "slot 1" and not "never the Avatar"), the slot-15 park at `DS:0x5788`, the `0x7f` `partyStatus` byte at `[0x57A7]` and the single `dec [g_party_size]` were already correct and are unchanged. The batch also **adjudicated the index question and deliberately changed nothing**: the census of persistent roster indexes finds exactly one (`activeCharacter`/`g_active_char`), and `sacrifice_member` does **not** re-index it — the re-indexing block is `SHOPPES3 0x03dd-0x0400`, the inn **(L)eave** (native `inn_leave`, already faithful), and this body has no counterpart, the same declared asymmetry the inn **(P)ickup** carries. So after a sacrifice an active-character index that pointed at or past the victim now names a different member; that is the original's behaviour, cloned bug-for-bug and pinned from both sides so a later "consistency" pass cannot unify the two routines. **No shared party-deletion primitive was introduced** and no party container was redesigned. To make the mutation testable at a production seam the routine was lifted out of `blackthorn.cpp`'s anonymous namespace and declared as `sacrifice_first_companion()`; its two call sites are otherwise unchanged. The new suite was written first and run RED against the **unmodified** body through that seam: `batch15_sacrifice_roster` **75 checks / 4 failures → 75 / 0**, the four being exactly R-23 — two from the seam and two from the **real scripted path** (`blackthorn_action()`'s four-failed-round pendulum and its conceded-mantra branch). Because most of the suite was green before the fix (correctly — those aspects were already faithful), four production-code mutations were run and reverted to prove no green is vacuous: adding the "intuitive" active-character re-index turns **2** RED, parking the victim at `characters[count-1]` turns **3** RED, removing the compaction entirely turns **17** RED, and decrementing `party_size` twice turns **9** RED. The authoritative suite went from **83 total / 82 pass / 1 fail** to **84 total / 83 pass / 1 fail** (the +1 is the new target); `gameplay_parity` remains GREEN and the sole failure is still `quest_parity`'s MinGW/w64devkit `STATUS_ACCESS_VIOLATION` (`Native driver failed: 3221225477`) from Batch 8, present identically in this batch's own pre-edit baseline run on this tree. The resource pack is **unchanged**, so the flashed SD card needs no repack. T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch15`: `openu5_tdeck.bin` `0xd19d0` = **858,576 bytes**, **−16** against Batch 14's `0xd19e0` (858,592), **190,000 bytes (18%) free**, **zero project warnings** (the five in the log are the pre-existing ESP-IDF `component_validation.cmake` notices about `esp_wifi`/`wpa_supplicant` private includes). Hardware flash was not performed, and **§16 Phase 6I records that this batch owes no hardware step at all** — the defect is unreachable from a device roster, so there is nothing on a T-Deck to observe; the physical-device backlog stays consolidated into the later hardware batch. Files touched: `native/core/src/blackthorn.cpp` (the one-bound fix plus the seam), `native/core/include/openu5/blackthorn.h` (the declaration), `native/core/tests/batch15_sacrifice_test.cpp` (new, 75 checks), `native/core/CMakeLists.txt` (the new target), and this document. **Broader Blackthorn ceremony fidelity is NOT marked GREEN by this batch**: the capture/interrogation presentation and pacing work (Y-31/Y-32) is untouched, §2's two Blackthorn rows keep their existing R-09-era verdicts with only the roster axis annotated, and the sequence's end-to-end hardware walk is still owed. **Y-29, Y-31/Y-32, Y-33 and the `quest_parity` harness crash were explicitly not touched and all remain open.**


**Status (post-Batch-13, R-21 adjudicated and closed):** `gameplay_parity`'s "Gameplay mismatch 2034" — open since Batch 2 and deliberately preserved through ten intervening batches — is **resolved, and it was a confirmed native defect**, not a harness or reference-expectation defect. "2034" was never a byte offset: `check-gameplay.ts` throws `Gameplay mismatch ${i}` with `i` the **sequence index**, and #2034 is the first `(U)se` of a scroll in the fixture. Native's `world_magic()` printed `Used Vas Lor Scroll.` where `CAST.OVL 0x11f0` prints the bare category word `Scroll` (DS `0x466a`), and the potion drinker printed an invented `No effect!` where `0x136e`'s handler prints nothing at all when it has no authored DS line. Both were introduced knowingly by the Alpha-20 forensic action-feedback pass — a device-UI decision that landed in the **core** command layer — in a session whose own write-up records that the Node/tsx reference generators were infrastructure-blocked and never ran. Proving it forced the parity fixture to state the reference's magic-feedback rules, which exposed two inseparable siblings of the same family, both fixed and both reported on their own evidence axis: the **potion ceremony fired after the effect line** (the binary puts it at `0x139b`, ahead of the reroll at `0x13a1`) and **dungeon casts raised no ceremony at all** (there is one cast dispatcher, `CAST.OVL 0x0f1a`, and `main.ts` emits the ceremony at all three of its cast mouths). What was **not** wrong: every `MagicCeremony` index and gate already matched `game/src/core/magic/ceremony.ts`'s derived jump tables — they merely showed as `unknown` because `gameplay_driver.cpp` had no name for the event and dropped its index. Production files changed: `native/core/src/world_magic.cpp`, `native/core/src/combat.cpp`, `native/core/src/dungeon_orchestration.cpp`; no name table was touched. The harness gained coverage rather than losing it: the driver now serialises the ceremony **with its index**, the fixture models it at all four magic mouths from the derived tables (~640 sequences newly asserted), and `action_feedback_regression` — which had been **asserting the fabrication** — was corrected, not weakened. New `batch13` CTest: RED **58 failing checks** → GREEN **0**, with groups E and F independently RED at 2 and 7 and each carrying a passing control; six mutations on clean rebuilt trees all caught, M1 reproducing `Gameplay mismatch 2034` exactly and M5 failing parity on a one-digit ceremony-index change that the suite could not see before this batch. The authoritative suite went from 80 total / 78 pass / 2 fail to **81 total / 80 pass / 1 fail** — the +1/+1 is the new `batch13` target, `gameplay_parity` moved fail → pass, and nothing else changed; the sole remaining failure is `quest_parity`'s MinGW/w64devkit `STATUS_ACCESS_VIOLATION` (exit `3221225477`) from Batch 8, present identically in this batch's own pre-edit baseline. T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch13`: `openu5_tdeck.bin` `0xd06f0` = **853,744 bytes**, **+32** over Batch 12B, **194,832 bytes (19%) free**, **zero warnings**. Hardware flash not performed; the device checklist is §16 Phase 6G. No new downstream parity mismatch was exposed — `gameplay_parity` passes outright.

**Status (post-Batch-12B, hardware discrepancy investigation — two confirmed native defects, both fixed):** The device report that a Destard `In Flam Grav` logged `Cast` and showed nothing, and that an enemy stood in the black void beyond a combat wall, was traced to **two separate root causes plus one authored-data fact**, recorded on five separate evidence axes in §14 "Batch 12B" and deliberately **not** merged into the Batch 12 combat-field conclusion, which is unchanged. (1) **Dungeon field state and persistence were never broken** — the cast writes exactly one cell to the `DS:0x4596` tile, keeps bit 3, refuses a non-empty cell, survives turning, ticking and a step cycle, and fires `Fire!!` / `Sleep spell!` when walked into. (2) **The 3D renderer never drew it**: `dungeon_art_blits()` correctly returns 0 for cell kind 8 (ITEMS.16 has no field image) and the port had never carried what the original does instead, the procedural sparkle subsystem `magic_field_sparkle_drawer` @0x127e; it is now `openu5::dungeon_field_spark()` (tables DS `0x2e42`/`0x2e4a`/`0x2e52`/`0x2e5a`, colours from the `0x1292` switch plus `add ax,8`, derived in `re/notes/dungeon-decor-mazmorra.md` §§2/5/6·4 and matching the reference `fieldSparkRects`), painted by `render_dungeon_view`. That omission had also hidden the **55 authored** fields DUNGEON.DAT ships in Wrong (30) and Covetous (25) since Batch 9C. (3) **An enemy on a black cell is the original's own authored placement** — 50 of the 128 shipped `.CBT` boards do it, 403 slots in all, and `compose_combat_presentation` draws an actor at exactly `y*11 + x`, so nothing is displaced; asserted as a guard, not changed. (4) **But the board itself was the wrong one**: the alpha pack stores `combatmaps.json`'s per-territory index while `dungeon_encounter()` addresses arenas by the global index `dungeon_room_map()` produces, so every dungeon room loaded the **next dungeon's** room and Doom's 112..127 matched nothing at all (`MissingMap`, no fight); retagged at load in `alpha_resources.cpp`, which means **an already-flashed SD card needs no repack**. Production files changed: `native/core/include/openu5/dungeon_art.h`, `native/core/src/dungeon_art.cpp`, `native/targets/tdeck/main/native_renderer.cpp`, `native/targets/tdeck/main/alpha_resources.cpp`. The new `batch12b_hardware_regression` CTest — the first host suite to read the **real shipped resource pack** and link the **production device renderer** and **pack loader** — was written RED against unfixed production code (**38 failures**: 8 A2, 1 A2b, 29 B2; every A1 and B1 assertion already passing, which is the finding) and is **0 failures** after the fixes; `dungeon_art_regression`'s new A14 block was mutation-checked both ways and reverted (wrong stroke length → RED 1201; collapsed colour selector → RED 930). The authoritative suite grew from 79 to **80** registered tests and reports **80 total, 78 pass, 2 fail** — the same pre-existing pair as every batch since: `gameplay_parity` (R-21, "Gameplay mismatch 2034", deferred by instruction and untouched) and `quest_parity` (`STATUS_ACCESS_VIOLATION`, exit `3221225477`, the MinGW/w64devkit environment finding from Batch 8). All magic, dungeon and combat suites green, `batch12_combat_field` and `dungeon_combat_regression` included. T-Deck ESP-IDF 6.1 firmware was rebuilt fresh into `native/targets/tdeck/build-batch12b` and **passed**: `openu5_tdeck.bin` is `0xd06d0` bytes, `+0x2a0` over Batch 12's `0xd0430`, with 19% of the 1 MiB app partition free and no compiler warnings from project code. Hardware flash was not performed; the device checklist is §16 Phase 6F. R-21 was **not** investigated, as instructed.

**Status (post-Batch-18, COMPREHENSIVE RECONCILIATION — this paragraph supersedes every status paragraph above it):** Batch 18 is the first whole-project audit since the parity, dungeon, magic, UI, Blackthorn, save/load and harness-cleanup batches, and it is an evidence batch, not a feature batch. Baseline, from a **clean** `native/core/build-batch18` on the unmodified Batch 17 tree `83b2ed56` (tag `alpha2-batch17-quest-parity-crash`), before any edit: **85 total, 85 pass, 0 fail, 0 skipped**, `gameplay_parity` PASS at 5,058 sequences, `quest_parity` PASS at 5,377 cases with the expected `nonterminatingObservations: 2`, and zero project compiler warnings. Batch 17's exit state was verified from `git show --stat` and from source rather than from its own prose: the commit touches five files, four of them host build inputs, and no production translation unit — the "host-harness undefined behaviour, not production, not environment" classification stands. Reconciliation then closed **two** confirmed native defects and deferred **one**. **R-26** (the wishing well, open since Batch 4) was four separate divergences, all in the T-Deck glue while the core arm was byte-correct throughout: the "No" answer dispatched nothing, ESC was ignored although the reference prompt type is `yesno-esc`, the `"a well."` object description (DS `0x720c`) was missing, and the wish row printed a fabricated `"What dost thou wish?"` instead of DS `0x722c` `"Thy wish?"`. Fixing the last of those exposed **R-34**, new and broader: `UiSession::finish_modal()` cleared `prompt_`/`input_` **after** dispatching the answer, so any modal the answer armed synchronously kept its mode and lost its prompt — the Rel Hur/skull-key `Direction?` row, the `(U)se` `On whom?` picker, the shrine `Virtue?`/`Mantra?` pair, the equipment picker, and Blackthorn's `Your response?` row all rendered blank. One guard on `is_modal(mode_)` fixes it, exact because `enter_modal()` only ever records a non-modal `return_mode_`. **R-25** (the crystal ball) was re-adjudicated against the current tree, its severity raised from "unassessed" to **2** — the feature is 100 % non-functional and its `"Peer into it?"` prompt is a fabrication — and then **deliberately deferred**, because the faithful fix needs kernel `0x4988`'s four-branch command-character picker as a shared core seam that `(S)earch` and `(C)ast` also owe, which is architectural, multi-system and moves a trap-check perceiver. Its four-step scope is written out in §3 R-25 instead of guessed at. Four **stale entries** in this document were corrected: §5's `BeginConversation` and `BlackthornAction` rows still read "dead — blocked by R-10/R-09" four batches after Batch 4 fixed both; §5's `DungeonAction` note still read "`TurnAround` and `Drink` have no producer" nine batches after Batch 9B gave them producers; and §7's "Currently failing" still ended at a post-Batch-9 tally. §2's crystal-ball row, which read a flat **G** while R-25 said the route was dead, was split into resolved-presentation / unreachable-route. Four **persistence axes** absent from §8 were audited rather than assumed and all four are **G**: persistent dungeon field spells need no storage of their own because `dungeon_orchestration.cpp:176` writes them into `DungeonState::cells[]`, which `capture_dungeon()` serializes in full and `restore_dungeon()` validates element-by-element; found search objects ride `objects_` (R-14); and the reagent-patch day latch and buried-moonstone state have named serializers. No persistence axis lacks a production-path test, and no new save format was introduced. The **Phase 18D bypass sweep** came back clean on five of six axes — no `TODO`/`FIXME`/stub/parked work in any production translation unit, no `#ifdef ESP*`/`CONFIG_*`-guarded gameplay branch, no gameplay text constructed in the device layer (every `AlphaRuntime` literal is on `UiTextChannel::System`; the one flavour line comes from `openu5::fountain_drink_result()`), no direct `GameState` mutation outside the core seams, and no duplicate command implementation in production (the mirrors that exist are test-side and self-declared) — and found exactly **one** untested production seam, which was the well ceremony itself, reachable only through `AlphaRuntime`. That is how R-26 survived fourteen batches while `gameplay_parity` drove the underlying core arm 5,058 sequences at a time. **One** test was added, for that one seam: `batch18_well_ceremony`, 19 checks, driving raw `RawInputEvent`s through the real `UiInputAdapter`, the real `UiSession` and the real, unmodified `alpha_runtime.cpp`. **RED 16/5 → RED 19/2 (after group W6 pinned R-34 on a second, independent chain) → GREEN 19/0**; four mutations — defeating the R-34 guard (2 RED), restoring the Yes-only dispatch (2 RED), reverting `cancel_means_no` (2 RED), dropping the `"a well."` line (1 RED) — each caught and reverted. Authoritative suite: **86 total, 86 pass, 0 fail, 0 skipped**, `gameplay_parity` still 5,058 and `quest_parity` still 5,377/2, zero project warnings. T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch18`: `openu5_tdeck.bin` `0xd1b30` = **858,928 bytes**, **+32** against Batch 16/17's `0xd1b10`, **189,648 bytes (18 %) free**, **zero project warnings** (the 15 warning lines are the five pre-existing ESP-IDF `component_validation.cmake` notices, three lines each). The **resource pack is unchanged, so the SD card needs no repack**. Two new documents carry what was previously scattered: **`ALPHA2_HARDWARE_CHECKLIST.md`**, one deduplicated **140-row** device list executable in a single session, each row with setup, exact input, expected visible result, expected state change, cancel-path expectation, SD-refresh requirement and a PASS/FAIL/UNTESTED field; and **`ALPHA2_PRESERVATION_LEDGER.md`**, cataloguing **14** intentional T-Deck adaptations, **4** deliberate enhancements and **18** unresolved divergences, with a per-row assessment of whether each is already isolated enough to become a future compatibility toggle. **No enhancement was reverted and no toggle was implemented.** **No hardware was run.** No physical T-Deck was available; **136 of the 140 rows are UNTESTED**, and the only hardware evidence on record anywhere in this project remains Batch 5's three world-cast checks (row H-48) and Batch 9B's three dungeon-runtime checks (rows H-85/H-86/H-88). The three device checks Batch 16 left owed — Symbol+Mic/`0`, Vas Rel Por, and the Rel Hur/skull-key cancel — are rows H-10/H-11, H-12–H-14 and H-15/H-16, and they are still owed. Batches 16 and 17 added no appendix status paragraph of their own; their write-ups are §14 Batch 16 and §14 Batch 17, and nothing in them is contradicted here. **Alpha 2 state: HOST-CLEAN — HARDWARE VALIDATION REMAINS.** The software half is finished to the limit of what host evidence can establish. It is **not** ALPHA 2 READY: the blocker is the 136 unexecuted device rows, plus one open production defect (R-25) with a deferred, scoped fix. **Alpha 2 must not be called validated until the hardware checklist has no UNTESTED rows.** **Batch 19 was not started and no Alpha 3 work was begun.**

**Status (post-Batch-19 — this paragraph supersedes every status paragraph above it):** Batch 19 is a focused fidelity batch with one job: close **R-25**, the sole genuine production defect Batch 18's comprehensive audit left open. Baseline, from a **clean** `native/core/build-batch19` on the unmodified Batch 18 tree `bb0067a1` (tag `alpha2-batch18-comprehensive-audit-r26-r34`), before any edit: **86 total, 86 pass, 0 fail, 0 skipped**, `gameplay_parity` PASS at 5,058 sequences, `quest_parity` PASS at 5,377 cases with the expected `nonterminatingObservations: 2`. The declared entry state is confirmed. One warning line appears in a from-scratch host build, and it is **not** new and **not** project code: GCC 16 emits a `-Wstringop-overflow=` false positive inside `w64devkit`'s own `bits/stl_uninitialized.h` while inlining `std::vector::insert` for `command_parity_test.cpp`. It is present at the Batch 18 baseline too; production translation units compile clean under `-Wall -Wextra -Wconversion -Werror`. **R-25 is CLOSED.** The crystal ball was 100 % non-functional: `ui_session.cpp` raised a **fabricated** `"Peer into it?"` yes/no that the reference has no trace of, and `alpha_runtime.cpp` then dispatched `CommandKind::CrystalBall` with `Command::member` at its never-set `-1`, which `look.cpp` rejected outright — a dead feature with a live handler, a wired prompt and a fully green suite. The faithful fix, as Batch 18 scoped it, is kernel **`0x4988`** as a shared seam: `native/core/include/openu5/command_char.h` + `src/command_char.cpp`, derived from `re/notes/resolve-command-char-178c-acta.md` (the routine read whole, `ULTIMA.EXE:0x4988`-`0x4a83`) with the caller derivations in `bola-144-acta.md` §2, `cmds.md` §11 and `cast-input.md` §9. Three of its four branches live in the seam — active character returned directly with **no** eligibility test (`@0x49b2`), a single `'G'`/`'P'` member auto-selected (`@0x49fa`), zero eligible giving `"None!"` — plus the branch-4 post-pick gate (`@0x4a2e`) and the three DATA.OVL literals (`"Player: "`, `"Disabled!"`, `"None!"`). Branch 1 (combat) is deliberately **not** in the seam and the header says why; Batch 19 fixed it where it was actually divergent, in the two `AlphaRuntime` sites that used the arrow-marked active member instead of the acting combatant as caster. The `-2` path, the `0x80`/`0x7f` threshold asymmetry and the `+2` bit test are each declared unmodelled with the acta section that adjudicates them. **All three callers now resolve through the one seam.** The crystal ball's fabricated yes/no is deleted. `(S)earch` gets its searcher — the perceiver of every chest trap check — from the picker after the direction, matching `SJOG 0x097e`→`0x09a0`; its **core** was already correct and is untouched, because `search_world()`'s fallback is a byte-exact clone of the reference core's own and `quest_parity` drives it. `(C)ast` resolves its caster **before** the spell menu, matching `CAST.OVL:0x0dd5` and the OCR corpus of 49 original Let's-Play routes (70 `"Cast... Player: <name> Spell name:"` rows, zero the other way round). `(R)eady`/`(Z)`-stats are **not** touched: they call a different routine, the raw picker `kernel 0x2d7a`, which has no active-character gate and always asks — confirmed empirically in DOSBox on 2026-07-17. **A first implementation put the branch gate inside `world_look()` and `gameplay_parity` failed at sequence 895.** The fixture was treated as the oracle and the code moved: the reference core raises a bare `crystal-ball-prompt` for **every** tile `0x29` and the picker runs at the caller/UI boundary, because in the binary it is blocking and owns a modal. No fixture was edited. **Two** tests were added. `batch19_command_char` (core, **57** checks) drives the seam and `world_look()`; it is **green against unmodified Batch 18 code**, which is declared rather than dressed up — the core arm was always correct, exactly as the audit said — so it is a lock validated by mutation, not a RED proof. `batch19_command_char_runtime` (**49** checks) links and drives the **real, unmodified `alpha_runtime.cpp`** through raw `RawInputEvent`s: **RED 24/49 → GREEN 49/49**. Four mutations of the seam — dropping `'P'` from the census (6 core RED), moving the auto-single `jle` boundary (4 core / 19 runtime RED), gating the active branch on status (2 core RED), and falling back to member 0 instead of `"None!"` (2 core / 2 runtime RED) — were each caught and reverted. Authoritative suite: **88 total, 88 pass, 0 fail, 0 skipped**, `gameplay_parity` still 5,058 and `quest_parity` still 5,377/2, **no newly excused known failure** (this project has none and Batch 19 added none). T-Deck ESP-IDF 6.1 firmware **PASS** into `build-batch19`: `openu5_tdeck.bin` `0xd20a0` = **860,320 bytes**, **+1,392** against Batch 18's `0xd1b30` = 858,928, **188,256 bytes (18 %) free**, **zero project warnings** (the five warning lines are the pre-existing ESP-IDF `component_validation.cmake` notices). The **resource pack is unchanged** — `openu5-alpha1-resources.bin` is still the 2,039,545-byte Batch 9C pack — **so the SD card needs no repack**. The consolidated checklist grows from 140 to **145 rows**: H-50 is **rewritten** from "record this known-broken behaviour" to an expectation that can pass, and **H-141 – H-145** add the win branch and its gem-count invariant, the lose branch's exactly-1-HP-to-the-chosen-member, the `"Disabled!"` re-ask, the three no-prompt branches, and the `(S)earch`/`(C)ast` prompt order. **No row was deduplicated** — nothing in the existing 140 covered the picker. One taxonomy correction: §1's summary row read **"Intentional divergences — 36"** while itemising 14 adaptations, 4 enhancements and **18 unresolved**. Unresolved divergences are open questions about fidelity, not decisions, and must not be totalled with deliberate ones; the row is now "Catalogued divergences — 18 deliberate and 18 unresolved" and `ALPHA2_PRESERVATION_LEDGER.md` §5 states the rule. The ledger itself always kept them in separate sections; only the summary label was wrong. Ledger **D-11** is struck through as resolved, which leaves **no live production defect in the backlog**. **No hardware was run.** No physical T-Deck was available; **141 of the 145 rows are UNTESTED**, and the only hardware evidence on record anywhere in this project remains Batch 5's three world-cast checks (row H-48) and Batch 9B's three dungeon-runtime checks (H-85/H-86/H-88). **Alpha 2 state: HOST-CLEAN — FINAL HARDWARE VALIDATION REMAINS.** With R-25 closed and no new production defect found, the software half is finished to the limit of what host evidence can establish, and `build-batch19/openu5_tdeck.bin` is **one stable firmware candidate for a single comprehensive physical validation session**. It is **not** ALPHA 2 READY, and the blocker is now singular: the 141 unexecuted device rows. **Alpha 2 must not be called validated until the hardware checklist has no UNTESTED rows** — nothing above is device evidence. **The hardware checklist was not executed and no Alpha 3 work was begun.**

## Batch 20 Hardware Validation

**Source commit/tag:** `f5ca709e` / `alpha2-batch19-r25-character-picker` (verified clean — the only working-tree "modification" was a zero-byte tracked file's CRLF-normalization phantom diff, no real content change).

**Firmware identity:** `OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, 860,320 bytes, SHA-256 `a357e28c9993e4452ceb9d6b64325a12833d90b4a9d35dad973a22fabde8a526`, packaged via `package_launcher.py` from a from-scratch `build-batch20` (88/88 host suite green, `gameplay_parity` and `quest_parity` both PASS, zero project compiler warnings, byte-identical size to the Batch 19 artifact). **Manual Launcher installation confirmed by the tester.**

**Device/session notes:** One long continuous physical session on a LILYGO T-Deck, driven interactively leg by leg (boot/input → combat/loot/Z-stats → world magic/items/crystal-ball → wishing well/town/transport → the dungeon leg → quest/Blackthorn → remaining spot-checks). Several mid-session recoveries were required: two dungeon soft-locks recovered via Debug → Teleport, one total device freeze recovered only by physical power cycle, and one movement-specific lockup (surviving both teleport and save-reload) also recovered by power cycle. The tester operated with developer tools throughout and used Debug presets extensively for setup (Combat/Shrine/Transport Test Setup, Flame/Shard Test).

**Checklist scope:** 145 original rows + 7 new rows discovered this session (H-146–H-152) = **152 total rows**.

**Rows attempted:** 133. **Rows deliberately not attempted:** 19 (see below).

- **PASS: 117**
- **FAIL: 12** (rows carrying a confirmed production defect — several rows share the same underlying defect)
- **BLOCKED: 1** (H-30 — checklist itself named the wrong location)
- **INCONCLUSIVE: 3** (H-51, H-78, H-148 — need retest or further adjudication)
- **UNTESTED: 19** (H-84; H-98–H-106 deliberately deferred; H-119, H-123 not reached; H-128, H-129, H-135, H-136, H-138, H-139, H-140 not reached)

**Confirmed production defects (unique, not row count):**

1. **Vas Rel Por phase-prompt broken** (H-12/H-13, presentation half also affects H-15) — `AlphaRuntime::overlay()` unconditionally renders a generic combat-reticle readout for any non-`Fire` `TargetSelection` request, clobbering `GatePhase`'s `"To phase:"` text; separately, the phase digit itself never reaches the correctly-written `ui_session.cpp` handler, so even a bare `1`-`8` produces no ceremony. Two-layer bug: presentation clobbering (affects all `TargetSelection` prompts, harmless for spatial ones) plus an unlocalized input-routing gap specific to `GatePhase`.
2. **Potion-use cancel doesn't consume the item** (H-45) — `alpha_runtime.cpp`'s modal-cancel handler for the party-target picker never dispatches the pending command, so `world_magic.cpp`'s otherwise-correct unconditional `consume_potion()` never runs. Same defect family as the Batch 16 Y-31 fix, in a sibling cancel path that fix didn't reach.
3. **Wishing-well wish-word matching is case-sensitive** (H-22) — should uppercase-fold like this codebase's own established `contains()` pattern (used for shrine mantras/NPC keywords), per the documented original `and 0x5f` behavior; doesn't, so most capitalizations of "Horse" silently fail.
4. **Shop ship/skiff/horse purchases are completely non-functional** (H-146, new row) — `ShopServices::ship`/`::horse`/`::reserve` callback hooks are never wired on the T-Deck (`alpha_runtime.cpp`), so the shop's own null-check silently no-ops the purchase confirm.
5. **Z-stats Armaments scroll marker renders as `?`** (H-63) — the on-device bitmap font has no glyph case for `^`, falling to the same bitmap as `?`. Cosmetic only.
6. **Dungeon room-entry freeze family, CRITICAL** (H-149/H-150/H-151/H-152, all new rows) — confirmed reproducible 4× across 2 dungeons: a full unrecoverable device freeze in one case (Destard), a permanently-stuck-facing soft-lock in another (Deceit), and a stuck one-actor combat turn queue in a third and fourth (Wrong, and a plain authored-room encounter). Strong root-cause lead: `dungeon.cpp`'s `room()` has one branch (already-cleared rooms, and all type-`0xA` rooms) that prints a message and emits **no** transition event, unlike every other cell-entry outcome in the file. Confirmed why "avoid revisited rooms" isn't a safe workaround: the cleared-room bitmap is keyed only by `(dungeon, room-number & 15)` with no floor/X/Y, and room numbers are a 4-bit field reused across a dungeon's 8 floors by construction — clearing/losing one room marks every other room sharing that number as cleared, anywhere in the dungeon, even on a genuine first visit. Mechanism from "no event emitted" to the two different failure modes not yet traced. **No safe workaround exists for further room-entry testing.**
7. **Dungeon save/load reverts to a prior save** (H-115, also affects H-124's dungeon context) — confirmed architectural gap: both the save's own self-validation (`candidate()`) and the load's core restore (`restore_candidate()`) in `alpha_save.cpp` never call `restore_dungeon()`, so dungeon-payload corruption is invisible to both integrity checks; the actual dungeon restore happens as a separate, unguarded step in `alpha_runtime.cpp` whose failure is silent to the player.
8. **Shard-into-flame ritual produces no output and leaves world movement permanently dead** (H-118, CRITICAL) — using a shard at the verified ritual cell produced only the generic "Use item" echo (no ritual text at all, contrary to `cast_shard_into_flame()`'s unconditional header line), after which world movement specifically stopped responding — no `Blocked!`, nothing — surviving both teleport and save-reload, recovered only by power cycle. `Look` and `Z`-stats remained fully functional throughout, ruling out a broad freeze; root cause not isolated, but the precision (Move specifically, nothing else) rules out corrupted save data and points toward stuck UI-session/input-mode state.
9. **Blackthorn teleport-in animation appears to be missing** (H-122, new observation) — distinct from the already-known D-10/Y-32 pacing issue; not yet root-caused.

**Device-only integration defects:** #4 and #6 above are specifically T-Deck adapter/glue gaps (unwired callbacks, a UI-layer event-emission asymmetry), not core-logic defects; #2's cancel-dispatch gap is also T-Deck-adapter-side.

**Reference uncertainties (not adjudicated, no code change made):** H-148 — a real, community-documented 1988 exploit (a skull-key vault of chests at the bottom of Lord British's castle that regenerates loot after a minimal rest) with zero existing coverage anywhere in this project's disassembly notes or bug catalog. Needs a dedicated future adjudication pass against the actual binary before any classification.

**Checklist corrections:** H-30's setup location was wrong in the checklist itself (said Lord British's castle; the real location, per this document's own R-33/R-32 entries, is the Palace of Blackthorn's basement/jail) — corrected in place, not yet retested at the real location.

**Environmental/test-state issues found and resolved (not defects):** H-38 (loot-cap confound from the "Combat Test Setup" preset's Max Resources), H-142/H-143/H-145 (crystal-ball picker confound from a party left at 1 HP / dead / non-`G`/`P` status by earlier testing) — all three retested clean once the confound was understood and are now PASS. H-51 remains open pending a retest away from unobstructed sightlines.

**Presentation-only / non-blocking differences:** H-63's `?` glyph (item 5 above) and the `overlay()` clobbering's harmless half (item 1, for spatial `Direction?` prompts specifically).

**Confirmed non-defects, worth recording so future testers don't re-investigate:** the wind-change/enemy-spawn combination after using Rel Hur (H-15) — spawning is the ordinary outdoor random-encounter system, unrelated to Rel Hur's own effect code, which only touches wind; the ship "Hole up" fixed-25-minute clock advance regardless of entered hours (H-147, new row) — matches `rest.cpp`'s ship-repair path exactly, was never a way to skip to daytime; H-121's Blackthorn active-character non-repoint after an execution — authentic original behavior, explicitly not filed.

**Required follow-up work for Batch 21, in priority order:** (1) the dungeon room-entry freeze family (#6) — top priority, blocks 9 checklist rows entirely and has no safe workaround; (2) the shard/movement-lock defect (#8), possibly related to #6 but not confirmed; (3) the dungeon save/load integrity gap (#7); (4) the four narrower confirmed defects (#1-3, #5); (5) H-30 retest at the corrected location, H-51 retest with blocked sightlines, and the remaining 19 UNTESTED rows once #6 has a fix.

**Status (post-Batch-20 — this paragraph supersedes every status paragraph above it):** Batch 20 is the first physical-hardware validation pass of Alpha 2, executed interactively against `build-batch20`/`f5ca709e` (byte-identical to the Batch 19 artifact — no production code changed this batch, purely evidence-gathering). 133 of 152 checklist rows (145 original + 7 discovered this session) were attempted; 117 PASS, 12 FAIL, 1 BLOCKED, 3 INCONCLUSIVE, 19 UNTESTED. Batch 19's software-only optimism did not survive contact with real hardware: **eight confirmed production defects** were found, two of them CRITICAL (a dungeon room-entry defect family causing soft-locks and one total unrecoverable device freeze, and a shard-ritual interaction that permanently kills world movement until a power cycle), plus a save/load integrity gap specific to dungeons, and four narrower but real defects (Vas Rel Por's phase prompt, potion-cancel consumption, wishing-well case-sensitivity, and non-functional shop ship/horse purchases). One checklist row (H-30) had the wrong location documented and is corrected in place. One tester-reported behavior (H-148, a vault/chest respawn-on-rest exploit) is a real, previously-uncatalogued open reference question, deliberately not adjudicated this session per this project's rule against filing or fixing without disassembly-level verification. Three environmental test-state confounds (H-38, H-142/H-143/H-145) were investigated, understood, and resolved as non-defects on retest. The dungeon room-entry freeze family has **no known safe workaround** — room-number collisions in the cleared-room bitmap mean even a genuinely fresh room can trigger it — so the nine room-combat rows (H-98–H-106) were deliberately left UNTESTED rather than risk further device freezes, and this is the top priority for Batch 21. **Alpha 2 state: HARDWARE VALIDATION INCOMPLETE.** Substantial required rows remain unrun (19, plus 1 blocked and 3 inconclusive), and multiple confirmed production defects — two of them critical — block full sign-off. This is not a closeout batch: Batch 21 must fix the freeze family and the shard/movement-lock defect at minimum before hardware validation can be considered complete. **No Alpha 3 work was begun. No audio batch was begun. No preservation cleanup was begun.**

---

## Batch 21A — the dungeon room-entry freeze family (H-149 / H-150 / H-151 / H-152)

**Source commit/tag:** `9e1dd6e6` / `alpha2-batch20-hardware-validation` (verified clean — the only working-tree "modification" was the same zero-content CRLF phantom on the tracked `-` file Batch 20 recorded).

**Baseline before any edit:** a from-scratch `native/core/build-batch21a` reported **88 tests, 88 pass, 0 fail, 0 skipped**, and a from-scratch T-Deck firmware build produced `openu5_tdeck.bin` at `0xd20a0` (860,320 bytes) with zero project warnings — byte-for-byte the Batch 19/20 artifact. There was no baseline noise to excuse anything.

**Scope:** H-149, H-150, H-151, H-152 only. No production change was made for H-118, H-115, H-12/H-13, H-22, H-45, H-63, H-146, H-122 or H-148.

### The family is THREE separate things, and only ONE of them is a defect

Batch 20 filed all four rows as one "freeze family" behind one root-cause lead — `dungeon_mark_room()` keying the cleared-room bitmap by `(dungeon, room & 15)` with no floor. That fact is **correct and it is ORIGINAL**, so it is preserved, and it is not what froze the device. Driving the real production runtime from raw input separated the four observations cleanly.

#### 1. H-151 (Destard) — REAL DEFECT, CRITICAL. The divide-on-hit capacity dead end.

`combat_growth_reserve()` (`native/core/src/combat.cpp`) returned

```cpp
if (a.enemy && (a.enemy->abilities & 0x1000))
    return std::max<int32_t>(s.count, 63) + 1;   // = 64 for any ordinary arena
```

the moment ANY actor in the arena was an enemy carrying ability bit `0x1000` (divide-on-hit). Every entry point into the arena tests that value as

```cpp
if (actors.capacity() - count < combat_growth_reserve(state))   // + 4 on the cast paths
    return CombatResult::NeedsActorStorage;
```

and `AlphaRuntime` owns `kCombatActors` (22 inline) + 32 PSRAM overflow = **54 slots**. `54 - count` can never reach 64. So the moment a divider joined an arena, **every** combat command was refused and did nothing — the player's, and the runtime's own `CombatEnemyStep` beat.

The downstream chain is the hardware freeze, exactly:

1. `combat_action()` returns `NeedsActorStorage` before it ever calls `Engine::current()`, so nothing mutates and no message is emitted.
2. `Engine::current()` therefore keeps naming the same enemy actor forever.
3. `AlphaRuntime::combat_ai_turn()` stays true (`actor->member == 255`), so `service_combat()`'s 400 ms enemy beat fires, dispatches `CombatEnemyStep`, is refused, and changes nothing — forever.
4. `AlphaRuntime::handle()` reaches `else if (context_.combat && combat_ai_turn())` and pushes every translated action into `combat_input_queue_` (8 deep) instead of the session, where it is never drained (`service_combat()` only drains while `!combat_ai_turn()`).
5. `combat_.ended` is never set, so `finish_combat_if_needed()` never tears down and `close_stranded_combat()` never fires (`Engine::current()` does return an actor — that Batch 9D escape hatch only covers a *null* actor).

Result: a fully rendered arena that owns the screen and answers nothing — not movement, not `Alt+M` (a `UiActionKind::SystemMenu` action, queued and dropped), not the Mic key. `Alt+D` would still have answered, because Developer/Save/Load are `DeviceShortcut`s handled *above* the queue branch. Only a power cycle escapes. That is the H-151 report verbatim.

**Blast radius, measured, not estimated.** Two shipped enemy definitions carry `0x1000`, read byte-for-byte out of `native/assets/openu5-alpha1-resources.bin`: def **24 Slime** (`abilities = 0x1100`) and def **30 Gargoyle** (`0x9000`). Seven of the 112 authored dungeon room boards place one, censused from `native/core/fixtures/fixed-maps.txt` (the same authored data `dungeon_parity` reads):

| board | dungeon (by `dungeonOrderSkippingDespise`) | room | dividers |
|---|---|---|---|
| cm16 | Deceit | 0 | 11 Slimes |
| cm23 | Deceit | 7 | 13 Slimes |
| cm41 | Despise/Destard | 9 | 15 Slimes |
| cm82 | Shame | 2 | 8 Slimes |
| cm92 | Shame | 12 | 13 Slimes |
| cm94 | Shame | 14 | 10 Slimes |
| cm109 | Hythloth | 13 | 6 Gargoyles |

— plus any corridor or overworld encounter that rolls a Slime or Gargoyle. **Destard is on that list**, which is where H-151 froze. Entering any of those rooms on Batch 20 firmware bricks the session.

#### 2. H-150 / H-152 — NOT A DEFECT. Set Active Player.

`Engine::current()`'s skip arm — auto-pass any player whose turn comes up while `g_active_char` names a *different, still-living* member — is a faithful clone of **COMBAT:0x063E @0666-067f** (`call 0xda86; ret`), and of the reference port's `skipsForActiveChar()` in `game/src/core/combat/combat.ts`. With an active character chosen, only that character is interpelled each round; leaving combat restores per-member commands. That is precisely the hardware observation ("only Shamino could act… on leaving the room, the other two characters' turns suddenly became available"). Reproduced on the production path (RED-6): with `active_character == 255` all three healthy members are scheduled; after the dungeon digit key sets member 2, exactly one is, and the arena stays live. `Blocked!` inside an arena comes from `Engine::move()`, not from the dungeon — a chosen character hemmed in by their own party and a wall reports it for every direction they try. **No production change.**

#### 3. H-149 (Deceit L8) — NOT A DEFECT. An authored, pit-fed chest alcove.

Deceit's **only** chest cell across all eight floors is floor index 7 (displayed L8) at **(5,5)**, cell `0x41` (locked chest). Its four cardinal neighbours in `DUNGEON.DAT` are wall, wall, wall, and the **unrevealed secret door** at (5,4) — and Deceit floor 6 (5,5) is a **pit trap** (`0x69`), which is how a party lands there without ever having revealed that door. `dungeon_action`'s `Forward`/`Back` correctly reports `Blocked!` for an unrevealed type-13 cell, so all four directions are blocked and turning stays responsive: the authored 1988 outcome. The way out is **(S)earch**, not movement. Verified against `native/core/fixtures/dungeon-maps.txt`; the geometry is reproduced end-to-end in RED-8, which pins both halves — the four-way block is real, and Search still reveals the door and lets the party walk out. **No production change.** (This also explains H-78's INCONCLUSIVE: it was interrupted by an authored alcove, not by a bug.)

### The room-number collision: adjudicated ORIGINAL, preserved

`dungeon.cpp`'s `cleared_bit()` — `i = loc - 0x21; if (i >= 1) --i; bit = i*16 + (room & 15)` — is a byte-exact clone of **DNGLOOK 0x0844 @0x088c-0x08c9 / 0x08d4**, mirrored in `game/src/core/dungeon/dungeon.ts` `dungeonClearedBitIndex()`. The 1988 bitmap is 14 bytes = 7 dungeons × 16 rooms, keyed by dungeon and a **4-bit room number only** — no floor, no X/Y — and it even collapses Deceit≡Despise onto one slot. Room numbers ARE reused across a dungeon's eight floors by construction, and clearing one really does mark the others. The six-room `ROOM_CLEAR_EXEMPT` table (`DATA.OVL 0x384a`) is cloned too. Entering an already-cleared room prints `"Entering room..."` and places no monsters (`dungeon.ts onEnterCell`; DUNGEON 0x0000:0x0008 unconditional, COMBAT 0xB94 places nothing) — which is exactly what `room()`'s message-only branch does. **All of this is original behaviour and none of it was touched.** RED-1 and RED-3 pin it; mutation M6 proves they would catch a change.

### Production changes (2 files, core only — no T-Deck adapter change)

1. **`native/core/src/combat.cpp` — `combat_growth_reserve()`.** Now states the growth **one action** can cause, instead of a fabricated 64 that no storage could satisfy: the number of live divide-on-hit enemies on the board, plus one slot if any enemy carries the `abilities & 4` daemon gate. `divide()` adds at most one clone per damaged divider, and every area sweep snapshots `s.count` before it runs (`combat_magic.inc`'s `int count = s.count;`), so no clone created during an action can divide within that same action. Dead/fled/absorbed actors no longer count. The cast/consumable call sites keep their own `+ 4` summon budget (`combat_cast_effect` already rejects a Swarms `extra` outside 0..4, so four is the whole summon budget).

2. **`native/core/src/combat_magic.inc` — `Engine::spawn()`** now returns `CombatActor *` and refuses when `s.count >= s.actors.capacity()`, and its four callers no-op on null. This moves the roster ceiling to the one growth site instead of leaving it only in a per-action precondition. It is reference-faithful: the 1988 combatant table is a fixed **32 × 8 bytes at DS:0xBA14**, slots 0..5 party and 6..31 enemies (`re/notes/combat.md §1`), so a divide or summon with no free slot simply does not happen there — and native's 54 slots are strictly more generous, so no spawn the original would have made is ever refused. It also makes the previously unchecked `s.actors[s.count++]` on that line impossible to reach out of bounds whatever a caller does first. (The only other `s.count++` is in `initialize_combat()`, already bounded by party ≤ 6 plus `map.unit_count` ≤ 16 = `kCombatActors`.)

No renderer was touched, no dungeon/floor/room was special-cased, no room-cleared behaviour was disabled, no room encounter was removed, and no "reset everything on room entry" hammer was added.

### Test changes

- **NEW `native/targets/tdeck/host_tests/batch21a_dungeon_room_test.cpp`**, registered as the `batch21a_dungeon_room_regression` CTest (suite 88 → **89**). It drives the REAL, unmodified `alpha_runtime.cpp`: `RawInputEvent → UiInputAdapter → UiSession → AlphaRuntime::command() → dispatch_world_command → execute_dungeon_command → dungeon_action → DungeonEventKind::Room → dungeon_encounter → start_fixed_combat → initialize_combat → Engine::current()`. Eight cases, 38 checks — RED-1 (room-number collision preserved), RED-2 (all healthy members scheduled), RED-3 (cleared-room message-only transition), RED-4 (loss/retreat → klimb → second same-numbered room), RED-5 (teardown invariants), RED-6 (Set Active Player), RED-7 (the divider arena), RED-8 (the Deceit chest alcove).
- **`native/targets/tdeck/main/alpha_runtime.h` / `host_tests/alpha_runtime_host_fixture.cpp`** — the Batch 11 host-test seam gained the dungeon/room-combat resources production reads out of the SD pack at `initialize()` (dungeon data, room arenas, enemy definitions, the overworld location table) and the same PSRAM combat storage it allocates (32 overflow actors / 32 loot piles / 32 arena fields), plus read-only `dungeon_state()` / `combat_state()` / `command_context()` windows. Non-behavioural: a fixture that supplies none of it keeps the previous null/zero state exactly, so every pre-existing host test is unchanged. **This seam mattered**: before adding the combat storage the host measured capacity 22 instead of the device's 54, which would have made the verdict wrong.
- **`native/core/tests/advanced_combat_parity_test.cpp`** — the three appended **native adapter checks** (not reference snapshots; all 125,440 parity rows were and are green) were recalibrated. They used to lean on the reserve short-circuiting to an unsatisfiable 64 — the very defect. The insufficiency is now built honestly: variant 25 fills the board with 16 enemies (count 18 of the 22 inline slots) and every one divides, so one action could need 16 clones plus the cast path's 4 summon slots against 4 free slots. Strictly stronger than the old check, and mutation M2 proves it still has teeth.

### RED → GREEN evidence

Pre-fix, against unmodified production code (`native/core/batch21a-red.log`): **6 of 38 checks RED.**

- `RED-7 / R7-3` — `combat_growth_reserve` = **64** against `capacity 54, count 5`, unsatisfiable.
- `RED-7 / R7-4` — a single production `CombatPass` returned **status 7 (NeedsStorage)**.
- `RED-7 / R7-5` — `combat.current` stayed at **2 → 2**: the scheduler never advanced. The frozen arena, reproduced.
- (Three further REDs were harness artifacts corrected before the baseline was recorded — the enemy beat is paced off the real wall clock, so a tight key loop only fills `combat_input_queue_`; `Fixture::play_out()` lets that time actually pass.)

Post-fix (`native/core/batch21a-green-focused.log`): **38 checks, 0 failures.** The same `CombatPass` now reports `status=0 current=2->1 count=5 reserve=2 capacity=54`.

### Mutation proof

| # | Mutation | Caught by |
|---|---|---|
| M1 | restore `max(count, 63) + 1` for a divider — the original defect | `batch21a_dungeon_room_regression` **R7-3, R7-4, R7-5** |
| M2 | make the reserve always 0 | `advanced_combat_parity` (adapter check 1/2) |
| M3 | drop the additive daemon-gate slot | **nothing** — honestly reported below |
| M4 | remove `spawn()`'s capacity refusal | **nothing** — honestly reported below |
| M5 | drop the Set Active Player auto-pass | `combat_parity` **and** `batch21a` **R6-3** |
| M6 | make cleared rooms re-fight (break `room()`'s gate) | `dungeon_parity`, `dungeon_combat_regression`, `batch21a` **R3-2/3/4** |

M3 and M4 are **defence-in-depth, not load-bearing, and are not claimed as covered**: once the reserve is correct, the arithmetic guarantees headroom, so no public path can reach the growth site with a full roster. They are kept because an unchecked `s.actors[s.count++]` behind a now-thin computed margin is a buffer overflow waiting for the next caller, and trading a freeze for a possible memory stomp would be a bad bargain. M5 and M6 are recorded to prove the two **adjudications** are pinned, not merely asserted.

### Full regression suite

From a clean `native/core/build-batch21a-green`: **89 tests, 89 pass, 0 fail, 0 skipped** (`native/core/batch21a-green-ctest.log`). `gameplay_parity` PASS, `quest_parity` PASS, `dungeon_parity` PASS, `dungeon_combat_regression` PASS, `combat_parity` PASS, `advanced_combat_parity` PASS (125,440 snapshots + 3 adapter checks), `alpha_runtime_integration_regression` PASS, all TypeScript drift suites PASS. Zero project compiler warnings — the single warning line in the build log is the pre-existing GCC 16 `-Wstringop-overflow=` false positive inside w64devkit's own `bits/stl_uninitialized.h`, not project code.

### Firmware

From-scratch ESP-IDF 6.1 build into `native/targets/tdeck/build-batch21a`: `openu5_tdeck.bin` = **`0xd20c0` (860,352 bytes)**, **`0x2df40` (188,224 bytes, 18%) free** in the `0x100000` app partition. **+32 bytes** over Batch 19/20's `0xd20a0`. Zero errors, zero project warnings (the 15 `component_validation.cmake` notices are third-party). **Not flashed** — the user flashes manually.

**SD resource pack unchanged; no SD recopy required.** Nothing under `native/assets/` or `extractor/` was touched; the fix is core C++ only.

### Status

**SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** H-149, H-150, H-151 and H-152 do not become PASS until the user flashes this firmware and physically retests. H-150/H-152 and H-149 are additionally reclassified from "defect" to **adjudicated original behaviour**; their hardware rows remain open only so a tester can confirm the described original behaviour on the device.

**Status (post-Batch-21A — this paragraph supersedes every status paragraph above it):** Batch 21A fixed the one real defect in Batch 20's "freeze family" and adjudicated the other three observations as faithful 1988 behaviour. The device-freezing defect was **not** the room-number collision Batch 20 suspected — that is original and preserved — but `combat_growth_reserve()` demanding 64 free actor slots out of 54 whenever a Slime or a Gargoyle was in the arena, which refused every combat command forever and left the runtime holding the screen and the keyboard. Seven authored dungeon rooms (Deceit 0/7, Despise/Destard 9, Shame 2/12/14, Hythloth 13) plus any encounter rolling those two definitions were affected. The host suite is **89/89** from a clean build and the T-Deck firmware builds clean at `0xd20c0`. **Alpha 2 state: HARDWARE VALIDATION STILL INCOMPLETE.** The nine room-combat rows H-98–H-106 that Batch 20 deferred are now believed safe to run, but the Batch 21A micro-retest below must pass first. H-118 (shard/movement lock) and H-115 (dungeon save/load) remain open and are the next priorities; no work on them was begun this batch. **No Alpha 3 work, no audio batch, no preservation cleanup was begun.**

### Phase 6M — Batch 21A dungeon room-entry micro-gate · *firmware only; the SD card is unchanged*

Run this **before** resuming the rest of the 152-row checklist. Capture the serial log for all of it.

1. **Wrong** — reproduce the room entry that formerly produced the one-character turn loop. Press `0` first (Set Active Player → None). Expect every healthy party member to be interpelled in turn.
2. Press a digit (e.g. `2`) to choose one member and confirm only that member now acts — this is **original behaviour**, not a bug. Press `0` to return to party mode.
3. Enter a **second room with the same room number on a different floor**. Expect `"Entering room..."` with no combat if it is already marked cleared; expect movement out of the cell to keep working.
4. **Destard** — repeat the exact H-151 sequence: lose or retreat from a fight (`BATTLE IS LOST!`), keep moving, then enter another room. Expect the arena to open and **accept input**.
5. **Deceit room 0 or 7** (the Slime rooms) — enter one deliberately. Expect a playable arena: turns advance, enemies act, `divides!` may appear, and the fight can be won, lost or escaped.
6. Confirm `Alt+M`, the Mic key, movement and combat all stay responsive throughout, with **no power cycle needed**.
7. **Deceit L8** — revisit the H-149 area. Falling into the (5,5) chest alcove and finding all four directions `Blocked!` is **correct**; confirm **(S)earch** toward the wall reveals `"A hidden door!"` and the party can then walk out.

---

## Batch 21A.1 — the "Deceit L1 → Klimb Down → L8" report (`236cfa34`+)

**Verdict: NOT A DEFECT. No production code was changed. Every one of the reporter's six observations is authored Ultima V data behaving exactly as the 1988 binary does.**

### The hardware observation, preserved verbatim

After flashing Batch 21A and beginning the Phase 6M micro-retest, the user reported, in this order:

1. Teleported/entered **Deceit**
2. Was on displayed **L1**
3. The only apparent route was a ladder
4. Used `K` / Klimb **Down**
5. This immediately entered a top-down dungeon room combat board with **Water Serpent** enemies
6. User fled / lost the room encounter
7. On return to the dungeon 3D view, the HUD now showed **Deceit L8**
8. The corridor had many doors and clearly was not the expected immediate lower level from L1
9. User then used Developer teleport to go back to the beginning of Deceit
10. Teleport succeeded, but the dungeon now looked different from how it looked at the beginning of the test

Two photographs exist: the room/arena state after `Klimb- Down!` / `Entering room...`, and the returned 3D view showing `Deceit`, `L8`, `BATTLE IS LOST!`, `Back up`, `Blocked!`.

The user asked whether the dungeon is procedurally generated. **It is not** — and the answer to the report turned out to depend on exactly that fact.

### Root cause: Deceit's authored six-deep pit shaft at (1,3)

There is no floor corruption anywhere in this path. The party genuinely is on floor index **7**, and `L8` is the correct rendering of 7 (`hud.cpp`: `level = int(d.pos.floor) + 1`). What happened is a **pit shaft** authored into `DUNGEON.DAT`:

| Step | Mechanism | Reference |
|---|---|---|
| 1 | Deceit floor 0 **(1,3)** is authored `0x60` — CellType `Trap`, sub 0. **Every trap cell is down-klimbable**, so `(K)` resolves DOWN with no U/D prompt. | `dungeon.cpp` `caps()` `down = t == 2 \|\| t == 3 \|\| t == 6`, a clone of DUNGEON:0x1E79-0x1E8B; `dungeon.ts` `klimbCaps()` `cell.type === CellType.Trap` |
| 2 | `level()` steps to floor 1 and runs `enter()` on the **destination** cell. Deceit floor 1 (1,3) is `0x69` — a pit trap. | DUNGEON:0x1C6A `change_level`; `dungeon.ts` `klimb()` → `onEnterCell()` |
| 3 | `enter()` therefore runs the **pit chain**, which falls one floor per pit and **keeps falling** while it lands on another pit. Deceit (1,3) is `0x61` on floors 2, 3, 4, 5 and 6. **Six falls: floor 1 → 7.** | DUNGEON:0x0A4C; `dungeon.ts` `pitFall()` |
| 4 | The chain stops on floor 7 (1,3) = `0xFA` — **room 10**, uncleared — and its tail opens the room fight, the same gate `enter()` uses. | `pitFall()` tail, byte-identical to native |
| 5 | `DUNGEON.CBT` #10 places **four units with sprite `0x88`**. `initialize_combat` resolves a sprite to `(sprite - 0x40) / 4` = **18**, and def 18 is the **Sea Serpent**. That is the reporter's "Water Serpent". | `enemies.ts:34` — `def.tile = 0x140 + defIndex*4` ⇒ `0x88` → def 18 |
| 6 | Room 10's board carries **no in-arena klimb/grate tile**, so leaving it sets `escape_floor_delta = 0` and `dungeon_combat_return()` moves no floor at all. The party is left on floor 7 — **L8**. | `combat.cpp:1515`, `dungeon_orchestration.cpp:85` |

**The route is unique.** Of all 64 Deceit floor-0 cells, exactly one — (1,3) — reaches floor 7 in a single Klimb Down. The other two authored LadderDowns, (5,3) and (5,7), each move exactly one floor into rooms 0 and 1.

### Each observation, accounted for

| # | Observation | Explanation |
|---|---|---|
| 2 | displayed **L1** | floor index 0. `hud_dungeon_bands` clamps `level` into 1..8, so L1 can only mean floor 0. |
| 3 | "the only apparent route was a ladder" | Deceit's standard entry is floor 0 (1,1), the `0x10` entrance LadderUp. The shaft at (1,3) is two cells away. |
| 4–5 | Klimb Down → room immediately | Steps 1–4 above. The intervening `Pit Trap!` / `Falling...` / `...splat!` messages scroll past in the six-fall chain. |
| 5 | Water Serpent | Sprite `0x88` → enemy def 18, **Sea Serpent**, four of them. |
| 7 | **L8** on return | floor index 7 + 1. Verified on the host: the stored floor byte is `0x07`, not `0xFF`. |
| 8 | "many doors … not one level below L1" | Correct — it is **six** levels below. Floor 7 is a different, door-and-room-heavy floor. |
| 10 | "the dungeon looked different" | **Two real, intended reasons.** (a) A same-dungeon developer teleport deliberately **preserves facing** (`debug_map_picker.cpp`) and does **not** re-run `dungeon_load`, so the same entrance cell is rendered down a different axis than on first entry. (b) The shaft is genuinely **spent**: the pit chain rewrites every pit it consumes (`0x60 \| (cur & 8)`, the reference's `curSub & 0xf8`), so a second Klimb Down at (1,3) now stops on floor 1. The map really has changed — that is authored mechanics, not corruption. |
| — | `Back up` → `Blocked!` | Floor 7 (1,2) is authored wall. The landing cell is **not** sealed: (1,4) is corridor, so the party can walk out south. |

### The wraparound hypothesis, disproved

The report hypothesised a signed/unsigned slip, a `-1`/`0xff` sentinel or a wrap producing floor index 7. **It is not that**, and the new suite proves it two ways:

- **Directly:** the stored floor byte after the whole sequence is `7` (`0x07`), the command path's own `pos.floor > 7` gate passes, and dungeon movement still dispatches. A wrapped floor would make every dungeon command return `InvalidContext`.
- **By mutation (M3):** injecting exactly the hypothesised wrap — making the pit chain store `uint8_t(++f + 248)` so the floor byte becomes `255` — leaves the HUD **still reading `L8`**, because `hud.cpp` clamps `level > 8` to 8. Only `B21A1-3a/3b/3d` catch it. So the HUD reading alone can never distinguish a real floor 7 from a wrap; the new RED-C case is what does.

### Production changes

**None.** `native/core/src` and `native/targets/tdeck/main` are byte-for-byte identical to `236cfa34`.

### Coverage added

`dungeon_combat_regression` (`native/targets/tdeck/host_tests/dungeon_combat_test.cpp`) gains a **Batch 21A.1** section: 7 cases, 45 checks, taking the suite from 124 to **169 checks**. Every case drives the real device path — `RawInputEvent` → `tdeck::UiInputAdapter` → `UiSession` → `dispatch_world_command` → `execute_dungeon_command` → `dungeon_action` → the pit chain → `dungeon_encounter` → `start_fixed_combat` → `finish_encounter_combat` → `dungeon_combat_return` — against the **shipped** `dungeon-maps.txt` and `fixed-maps.txt` bytes.

| Case | What it pins |
|---|---|
| **B21A1-0** | The authored census: `0x60` at floor 0 (1,3), six chaining pits on floors 1–6, `0xFA` room 10 on floor 7, four sprite-`0x88` Sea Serpents on CBT #10, no in-arena escape tile, and that (1,3) is the **only** floor-0 cell reaching floor 7. |
| **B21A1-1** (RED-A) | The hardware sequence: HUD `L1` → Klimb Down → floor 7, six pits consumed, room 10 live, four def-18 Sea Serpents, HUD `L8`. |
| **B21A1-2** (RED-B) | Leaving without a victory keeps floor 7: `escape_floor_delta == 0`, the room-entry cell 33:7:(1,3) restored, room still uncleared, session intact. |
| **B21A1-3** (RED-C) | **No wrap.** Floor byte is exactly 7; `floor + 1 == 8` with nothing to clamp; movement still dispatches and is not `InvalidContext`; (1,4) is corridor and (1,2) is wall. |
| **B21A1-4** (RED-D) | Control: the (5,3) LadderDown still moves exactly one floor into room 0, HUD `L2`, no pit consumed anywhere. |
| **B21A1-5** (RED-E) | Control: room 0's authored in-arena `0xC8` Klimb still sets `escape_floor_delta = -1` and returns floor 1 → 0, HUD `L1`. |
| **B21A1-6** | Observation 10: the teleport lands on 33:0:(1,1) reading `L1`, **preserves facing**, leaves the six consumed pits consumed, and a second Klimb Down at (1,3) now stops on floor 1. |

### Mutation proof

Because there is no fix to revert, the four mutations instead inject the defects the report hypothesised — and the "obvious fixes" a future batch might be tempted to apply — proving every new case is load-bearing.

| # | Mutation | Kills |
|---|---|---|
| **M1** | `caps()` drops `t == 6`, so trap cells are no longer down-klimbable ("stop the pit being a ladder") | 12 checks |
| **M2** | The pit chain stops after one fall (`f < 8` → `f < 2`) | 11 checks |
| **M3** | The hypothesised wrap: the chain stores `uint8_t(++f + 248)`, floor byte → `255` | 6 checks — and **the HUD still reads `L8`**, which is the whole point |
| **M4** | `finish_encounter_combat` forces `delta = -1` into `dungeon_combat_return` | 17 checks, across D9D, B9E **and** B21A1 |

### Status

**SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** (Nothing needed fixing; the retest confirms the adjudication on the device.)

Host suite **89/89** from a clean build, 0 fail, 0 skipped, zero project warnings. Firmware unchanged from Batch 21A — see the checklist addendum. **SD resource pack unchanged; no SD recopy required.**

### Phase 6M.1 — Batch 21A.1 micro-retest · *no reflash needed if Batch 21A is already on the device*

The Batch 21A firmware already on the device is sufficient — no production code changed. Run this short list; do **not** resume the 152-row checklist until it passes.

1. Enter Deceit at the standard entry. Confirm the HUD reads **L1**.
2. Walk to **(1,3)** — the plain-pit cell — and press `K`, choosing **Down** if prompted.
3. Expect `Down!`, then a run of `Pit Trap!` / `Falling...` / `...splat!`, then `Entering room...`.
4. Expect the room board with **four Sea Serpents**.
5. Leave the room by walking off the board edge (`BATTLE IS LOST!`).
6. Confirm the 3D view returns reading **L8** — **this is correct**. Confirm the party is at (1,3) on floor 7.
7. Confirm movement and turning still respond. Walking **south** to (1,4) must work; north must report `Blocked!`.
8. Developer-teleport back to Deceit's standard entry. Confirm **L1**, cell (1,1), and that level/cell are stable.
9. Walk to (1,3) and press `K`/Down **again**. It must now stop on **L2** — the shaft is spent.
10. Repeat steps 1–9 once.

Serial capture is useful if convenient, but a clean visual/behavioural pass does not depend on it unless something unexpected recurs.

---

## Batch 21A.2 — adjacent dungeon-door perspective visibility (`bfc4e39e`+)

**Verdict: REFERENCE-FAITHFUL VISUAL QUIRK — NO FIX. No production code was changed.**

### The hardware observation

During the same Deceit L8 session reached through the authored pit shaft (see § *Batch 21A.1*), the user met a cluster of roughly three adjacent doors near the bottom of the floor. Facing one door head-on, another door occupied an immediately neighbouring cell — and **no part of the neighbour's edge or frame was visible**. The frontal door filled the corridor end and the neighbouring cell read as ordinary masonry. The user found it extremely disorienting and asked whether the native renderer was dropping a side-door slice the original would draw.

### The exact authored geometry

Deceit (33) floor 7 carries **six** authored room cells — and a room cell (`0xF`) is door-family for both the front (`frontBase` → 12) and the side (`sideBase` → 4) tables:

```
        x0    x1    x2    x3    x4    x5    x6    x7
  y3   Wall  r10   Wall  ....  ....  ....  ....  ....
  y4   r12   ....  Wall  ....  Wall  Secr  Wall  r11
  y5   r14   ....  Wall  Ladd  Wall  Ches  Wall  r13
  y6   Wall  Wall  Wall  Secr  Wall  Wall  Wall  ....
  y7   Wall  ....  Wall  ....  Wall  Ladd  Wall  r15
```

(1,3)=`0xFA` r10 · (0,4)=`0xFC` r12 · (7,4)=`0xFB` r11 · (0,5)=`0xFE` r14 · (7,5)=`0xFD` r13 · (7,7)=`0xFF` r15. Four of them — r11/r12/r13/r14 — form a **2×2 block straddling the x-wrap**, which is the "cluster of about three doors".

Exactly **five** standable viewpoints on the floor put a door directly ahead with another door beside it. The hardware view is **(1,4) facing West**: frontal door r12 at (0,4), r10 at (1,3) beside the **party**, r14 at (0,5) beside the **frontal door**.

### What the ORIGINAL emits

[REF-BIN] `dng_draw_view` @0x1a90, as recorded in `re/notes/dungeon3d-audit.md` §7.5:

> *"Marcha `si=0..3` … por celda llama `fn_150a(si)` (front: si `tile≥0xa0` dibuja muro de fondo y **PARA**) y, **si abierta**, `fn_1682` IZQ+DER"*

The front test runs **first** and **stops the march**; the left/right pair is emitted **only when the cell is open**. So the blocking depth never contributes side slices. [REF-TS] `planDungeonView()` breaks on `blocksView(cell)` before its own side pair — identical.

**The two adjacencies are therefore different questions, and only one of them was ever drawn:**

| Neighbour of… | Ring | Emitted? |
|---|---|---|
| the **party's own** cell | 0 | **yes** — a side door, `sideBase` 4 |
| the **frontal door** | 1 (the blocking depth) | **no** — the march has already stopped |

### The resulting pixels — no occlusion is involved

Authored slice widths (`dungeon_art.cpp kDungeonWallDims`) and the compositor's fixed X tables settle it arithmetically:

| Piece | Image | Geometry | Covers |
|---|---|---|---|
| ring-0 side door, left | 4 | x = 16, w = 24 | **16 … 40** |
| depth-1 front door | 13 | centred pair, w = 56, spans 96−w … 96+w | **40 … 152** |
| ring-0 side door, right | 4 | x = 152, w = 24, mirrored | **152 … 176** |

The three **abut exactly and never overlap**. So a door beside the party is drawn *and stays fully visible*; a door beside the frontal door produces **no op at all**. The invisibility is a **planning stop**, not an art or painter-order problem — which is why no amount of image-index or z-order work could reveal it without diverging from 1988.

The hardware plan, from the real production planner (`batch21a2-plan-dump.log`, party (1,4) facing West) is three ops and nothing else:

```
#  depth side   family              image destX width covers
0  0     left   side open passage   16    16    24    16..40
1  0     right  SIDE DOOR           4     152   24    152..176  (mirrored)
2  1     pair   FRONT door          13    40    56    40..152   (centred PAIR)
```

### Native vs reference

Driven op-for-op on the same authored cells, `plan_dungeon_view()` (native) and `planDungeonView()` (TypeScript reference) produce **identical** plans for all six viewpoints — the five door-adjacency ones and an open-corridor control. This is case **D** of the adjudication matrix: *both render the same confusing view → confirmed non-defect.*

The control is what proves the suppression is the blocker break and not a classification failure: from the very same cell **(1,4) facing South**, with the corridor open, the very same r14 at (0,5) **does** earn a ring-1 side-door slice (image 5 at x = 120).

### Why it is spatially confusing, and why that is authentic

The original's corridor is composed, not projected: each depth contributes a fixed-width pre-drawn slice, and a blocking cell is represented by its **face**, not by its surroundings. A door's face is opaque scenery — the geometry beside it lies behind the plane the player is looking at, and 1988 never had a piece for it. The result is that a T-junction of doors is indistinguishable from a single door in a wall until the party steps sideways. That is a real limitation of the 1988 raster and is preserved deliberately; adding a jamb, an outline or a partial side slice would be a modern perspective cue the original does not have.

**Guidance for hardware testing:** this view is authored. Seeing no hint of a door that is adjacent to the door you are facing is not a defect and should not be filed as one.

### Coverage added

`dungeon_view_regression` gains **D14**, driven against the shipped `dungeon-maps.txt` (the test now takes the fixture path, exactly as the parity suites do). D4 only ever proved side-door *classification* down an open corridor; nothing proved what the blocker break does to it.

D14 pins: the authored cluster identity; the hardware view's three-op plan; the frontal door image 13; the ring-0 side door emitted, mirrored, at its table X; **the blocking depth emitting no side slice**; the abutting-not-overlapping extents; the open-corridor control; and the other three cluster viewpoints.

`game/tests/b21a2-door-adjacency.test.ts` does the same against the reference planner, so the two can never silently drift.

### Mutation proof

| # | Mutation | Result |
|---|---|---|
| **M1** | emit the blocking depth's side slices before the break — *the exact "obvious fix" this report invites* | 5 D14 assertions RED. **Across the whole 89-test suite, `dungeon_view_regression` was the only failure** — before D14, nothing would have caught it |
| **M2** | drop `Room` from `dungeon_side_slice_base` | 4 RED: D4's classification case plus three D14 assertions, proving the *visible* half is load-bearing on authored data too |

### Status

Host suite **89/89** from a clean build, 0 fail, 0 skipped, zero project warnings. **No production change, so no firmware build and no hardware retest are required**; Batch 21A/21A.1 conclusions are untouched. **SD resource pack unchanged.**

## Batch 21A.3 — Set Active Player reachability during combat (H-153)

### The hardware observation

On the physical T-Deck:

- With Set Active Player engaged **before** a fight, only that party member receives manual combat turns.
- If the player clears the selection with Symbol+Mic/0 **before** combat, multi-member turn rotation works.
- Once combat begins, the selection can be neither cleared nor changed.
- So a player who forgets to clear it is locked to one character for the whole battle.

The scheduler half of that report is **not** a defect. `Engine::current()` (`native/core/src/combat.cpp:260-266`) auto-passes every party actor that is not `active_character` while a live actor with that member exists, which is COMBAT:0x0666-0x067f (`skipsForActiveChar`) exactly. The question this batch had to answer is the other half: **is Set Active Player supposed to be reachable from inside the arena?**

### Native routing before this batch

| Where | `1`-`9` | `0` | Symbol+Mic/0 on T-Deck |
|---|---|---|---|
| Exploration (`UiSession::handle_exploration`) | harpsichord intercept first, else `CommandKind::SetActivePlayer`, echo "Set Active Plr:" | same, member 0 -> "None!" | `UiInputAdapter` emits a literal `'0'` Character, so the ordinary digit route (Y-29) |
| Dungeon (`handle_dungeon`) | `SetActivePlayer` (DUNGEON 0x07bc-0x07d6) | same | same |
| **Combat (`handle_combat`)** | **no case — falls to `default:`, appends "What?" to the Combat channel** | **same** | **same: the chord produced a correct literal `'0'`, and `handle_combat` answered "What?"** |

So the failure was **two-deep**, and either layer alone would have been enough to cause it:

1. `handle_combat` **never constructed** `SetActivePlayer` — every digit was the unknown-key default.
2. Even if it had, `commands.cpp`'s overworld arm is gated `!c.combat`, so the command would have fallen through to the context gate at `commands.cpp:764` (`c.combat && !ready_anywhere` -> `InvalidContext`) and been rejected silently.

The T-Deck adapter was **not** at fault: `UiInputAdapter::translate` is mode-agnostic for the Symbol+Mic chord and already delivered a literal `'0'` in Combat. That matters, because a plain (unmodified) Mic press in Combat is `UiActionKind::Cancel` = `CombatEscapeQuick` — had the chord regressed to that path, trying to clear the selection would have attempted to **flee the battle**. C6 below now pins it.

### What the ORIGINAL does — COMBAT.OVL 0x063E

`re/notes/combat-commands.md` already had the dispatch row (`| 0-9 | 0x09ec/0x09fe | set-active | Set Active Plr (g_active_char) |`), and `re/notes/combate-hotfix-20260722.md` section 3 carries the full derivation — including the note that an earlier pass had the turn-cost semantics exactly **inverted**. The binary answer, key by key:

| Key | Chain | Behaviour | Turn cost |
|---|---|---|---|
| `0` | COMBAT @0x0aa2 -> @0x09ec | `g_active_char = 0xFF`; prints "Set active plr:\nNone!\n" (DS 0x6e66) | **CEDES the turn** — falls to the tail @0x0b56 with `[bp-2]=0`; @0x0b79-0x0b83 sees the key in `'0'..'6'`, skips the end-of-action housekeeping (SJOG 0x2012) and returns |
| `1`-`6` **valid** | @0x0aaa-0x0ab4 -> @0x09fe -> stub 0x7d46 -> **SJOG.OVL 0x1F7A** | prints "Set active plr:\n" (DS 0x8f3a) always, then `g_active_char = idx` + the NAME (COMSUBS 0x0094), ret 1 | **CEDES the turn** (same tail) |
| `1`-`6` **invalid** | SJOG @0x2000-0x2004, ret 0 | prints "Invalid!\n" (DS 0x8f4c); COMBAT @0x0a11 -> @0x0a41 sets `[bp-2]=1` and jumps to 0x06F1 | **FREE** — the SAME actor is re-prompted with its banner |
| `7`-`9` | default @0x0ab7 | "What?\n" (DS 0x6ee6), re-prompt without a banner | **FREE** |

Validity is judged **against the ARENA, not the roster** (SJOG @0x1f9a-0x1fcd sweeps the 32 combat slots for a party slot with that `charIdx` and rejects `flags & 0x2c` = asleep/gone). The strings are COMBAT.OVL's **own** copy: lower-case "Set active plr:", *not* the kernel's "Set Active Plr:" (DS 0xa396) that the overworld loop prints — this is precisely the byte-identical-twin trap `re/notes/combat-commands.md` warns about.

Answers to the five questions asked:

- **A. Available during combat?** **Yes** — `0` and `1`-`6` are live dispatcher entries.
- **B. What do digits do?** `1`-`6` select (or reject); `0` clears to 0xFF; `7`-`9` are "What?".
- **C. Can the restriction be cleared?** **Yes**, with `0`; it writes `g_active_char = 0xFF`; it **costs the current actor's turn**; and it applies immediately to scheduling — the @0x0666 gate reads the same global every round.
- **D. Can it be changed member-to-member?** **Yes**, with `1`-`6`, under the arena validity rule.
- **E. Does combat inherit the pre-combat state?** **Yes** — `g_active_char` is one global; combat neither resets nor re-reads it from anywhere else.

### Verdict

**`CONFIRMED REACHABILITY DEFECT — FIXED`.** The original allows both clearing and changing during combat; native allowed neither. This is not a gameplay trap in the original: the escape hatch exists, native simply had no route to it.

### Production changes (4 files)

| File | Change |
|---|---|
| `native/core/include/openu5/combat.h` | `CombatAction::SetActive`, appended at the **end** of the enum. The combat parity fixtures encode actions as `CommandKind`/`CombatAction` ordinals (`combat_parity_test.cpp`: `CommandKind(int(CommandKind::CombatMove) + op)`), so no existing value may shift. |
| `native/core/src/combat.cpp` | The handler, placed beside `CombatAction::Yield` and **ahead of `e.disabled()`**, because the reference dispatches the digit from the player turn loop before any action gate. Emits the Echo + result, applies the arena validity rule, and calls `e.advance()` on success (and on `0`) but not on rejection. Widened the action-bound check and added the `0..6` range guard. |
| `native/core/src/commands.cpp` | One new disjunct, `(c.combat && cmd.kind == CommandKind::SetActivePlayer)`, on the existing combat-command arm; the action mapping and the `combat_arg = cmd.member` line. The `!c.combat` overworld/dungeon arm is untouched. |
| `native/core/src/ui_session.cpp` | `handle_combat` gains `case '0' ... '6'` constructing the **same** `CommandKind::SetActivePlayer`. `'7'`-`'9'` deliberately stay with `default:` -> "What?". No `command_echo()`, because COMBAT.OVL prints its echo from inside the set-active routine, so `combat.cpp` emits it — the shape Pass and Ready already use. |

Explicitly **not** done, per the batch's own prohibitions: no auto-clear or reset of `active_character` at combat start, no menu, no T-Deck-only command, no weakening of the `Engine::current()` filter (C1 guards it), and no change to the Symbol+Mic/0 adapter contract (C6 guards it).

Two deliberate adjudications worth recording:

- **The `advance()` seam.** The reference's tail *skips* the end-of-action housekeeping (SJOG 0x2012 spell-turn decay). Native models the housekeeping-free advance as `Engine::advance()` — the same seam `CombatAction::Yield` and the combat (R)eady close already use, and the declared native equivalent of the port's `Combat.playerYieldTurn()`/`playerReady()` pair. Reusing it is the consistent choice; changing what `advance()` means is out of scope for this batch.
- **The invalid-case re-prompt.** The DOS re-prints the turn banner at 0x06F1. Native has no transcript banner — `CombatEventKind::Turn` carries no text and `UiSession::consume` ignores textless combat events — and `battle.current` still points at the same actor, so the HUD keeps showing them. Nothing is emitted, and nothing is missing.

### Test changes

One new target, **`batch21a3_combat_active_player`** (`native/core/tests/batch21a3_combat_active_player_test.cpp`, 23 checks). It links `ui_input_adapter.cpp` because the question is a routing one end to end, and it drives the **real** path throughout — `tdeck::UiInputAdapter` -> `UiSession::handle_combat` -> `dispatch` -> `execute_command` -> `combat_action` — with core events fed back into the same session's transcript. No spy stands in for routing anywhere.

| Case | Pins |
|---|---|
| **C1** | CONTROL, green before and after: the pre-combat active member is inherited and is the only one scheduled, over six rounds |
| **C2** | a digit in combat reaches `SetActivePlayer(member=2)` and is not "What?" |
| **C3** | `0` clears to 0xFF, prints the **lower-case** arena echo + "None!", and the rest of the party is prompted again in the same battle |
| **C4** | `2` re-points the selection, prints the chosen NAME, and only the new member is scheduled thereafter |
| **C5a/b** | a valid selection, and `0`, each cede the current actor's turn |
| **C5c** | an arena-invalid member prints "Invalid!", writes nothing, and costs **no** turn |
| **C5d** | `'7'`-`'9'` construct no command, print "What?", cost no turn |
| **C6** | Symbol+Mic in **Combat** still yields a literal `'0'` (not Cancel/Escape), fires no device shortcut, and clears the selection through the ordinary digit route |

### RED to GREEN evidence

Against unmodified production code: **14 of 23 checks RED** (`native/core/batch21a3-red.log`). The nine that passed are the controls — C1's two scheduler checks, C5c's "does not write" and "costs no turn", C5d's three, and C6's two adapter checks — which is exactly the shape the report predicts: the scheduler and the adapter were already right, the route did not exist. After the fix: **23/23 GREEN** (`native/core/batch21a3-green.log`).

### Mutation proof

| # | Mutation | Result |
|---|---|---|
| **M1** | drop `'0'` from the combat digit case, keeping `'1'`-`'6'` | **6 RED** — all four C3 checks, C5b, and C6's end-to-end check. The T-Deck clear route has its own guard |
| **M2** | a valid selection no longer calls `e.advance()` | **2 RED** — C5a's cost check and C4's "only the new member is scheduled" |
| **M3** | the rejected branch calls `e.advance()` | **1 RED** — C5c's cost check, the one the earlier reference derivation had inverted |
| **M4** | validate against the ROSTER instead of the arena — the "obvious" shortcut of reusing the overworld criterion | **3 RED** — all three C5c checks |
| **M5** | remove the `commands.cpp` combat routing disjunct (the defect itself) | **12 RED** |

### Full regression suite

**90/90 from a clean build**, 0 fail, 0 skipped (`native/core/batch21a3-verify-ctest.log`) — the prior 89 plus the new target. Targeted re-runs of `combat_parity`, `advanced_combat_parity`, `command_parity`, `dungeon_combat_regression` and `alpha_runtime_integration_regression` all pass; appending the action ordinal left every fixture trace byte-identical. One warning, the pre-existing w64devkit `stl_uninitialized.h` `-Wstringop-overflow=` false positive; zero project warnings.

### Firmware

Built clean with ESP-IDF 6.1: `openu5_tdeck.bin` = **0xd2230** (860,720 bytes), up 368 bytes from Batch 21A/21A.1/21A.2's 0xd20c0 (860,352) — the new combat handler and its strings. 18% of the app partition free. **Not flashed.** The five `component_validation.cmake` notices are third-party, as in every build.

### SD card

Unchanged. No resource pack, asset or fixture was touched.

### Status

`CONFIRMED REACHABILITY DEFECT — FIXED`. Batch 21A / 21A.1 / 21A.2 conclusions are untouched.

### Phase 6N — Batch 21A.3 Set Active Player in combat · *firmware only; the SD card is unchanged*

Flash the Batch 21A.3 firmware, then, with a party of three or more:

1. Outside combat, press Symbol+`w` (`1`) to select member 1. Confirm the echo "Set Active Plr:" and the member's name.
2. Start a multi-member fight. Confirm only member 1 is prompted — this is correct and unchanged.
3. **In combat**, press Symbol+`e` (`2`). Expect the echo "**Set active plr:**" (lower case — it is a different string from step 1) followed by member 2's name, and the current actor's turn to end immediately.
4. Confirm that from the next round on, only member 2 is prompted.
5. **In combat**, press Symbol+Mic (`0`). Expect "Set active plr:" + "None!", the turn to be ceded, and normal multi-member rotation to resume in the same battle. **It must not flee** — that is the plain-Mic path and the chord must not reach it.
6. In combat, select a member who is dead or asleep. Expect "Invalid!" and the **same** actor still prompted, with no turn lost.
7. In combat, press Symbol+`x` (`8`). Expect "What?" and no turn lost.

**Pass criteria:** steps 3-5 change the scheduling mid-battle; step 5 never escapes; step 6 costs nothing.

---

## Batch 21B — H-148 chest-respawn fidelity + Original Behavior Gap Sweep

**Scope:** H-148 only for production. Everything else below is classified and queued, not fixed. H-118 (shard ritual) and H-115 (dungeon save/load) were not touched.

### How this batch could read the 1988 binaries at all

Every `re/notes/*.md` cite of the form `re/disasm/CMDS.OVL.asm` refers to a listing that **is not in the tree** — `re/disasm/` does not exist, and `original/` is gitignored while the EA files sit there untracked. So this batch built the reading tools instead of trusting the notes:

| Tool | What it does |
|---|---|
| `re/tools/dis16.py` | 16-bit capstone front-end over any shipped module; `--exe` skips ULTIMA.EXE's MZ header so offsets match the notes' CS:IP convention |
| `re/tools/thunks.py` | dumps the kernel overlay-thunk table (11-byte `lcall 0x72e:0x2ec` + overlay id + `ljmp 0:target`) and derives each overlay's load base |
| `re/tools/callers_banda.py` | exhaustive **by-band** call-site census: every `E8`/`E9` rel16 at **every** offset in every module, resolved through the overlay base. Over-reports rather than misses, which is the safe direction for a universal claim |

Derived overlay bases (used throughout below): TOWN/OUTSUBS `0x81D0`, MAINOUT `0x8304`, NPC/TALK/SHOPPES `0xA290`, CMDS/SJOG `0xBF80`, COMBAT `0xBFEC`, DUNGEON `0xE1E0`. The first two reproduce `re/notes/npc-carga-partida-fresh-gate.md` §1 **byte for byte**, which is this batch's control that the tooling is reading the same bytes the notes read.

---

### H-148 — the reference behaviour, derived

**The trigger is not "rest nearby". It is (H)ole up on a bed, and it is one routine.**

1. `ULTIMA.EXE:0x329C` is the (H) handler. It prints `"Hole up- "` (DS `0xa170`), reads the tile under the party, and `0x32b9 cmp [bp-4],0xab / je 0x32c6` — **only tile `0xAB`, a bed**, reaches `cmd_camp_holeup`. Anything else prints `"Only in bed!\n"` (DS `0xa17a`). The by-band census finds exactly one caller of that thunk (`0x802E`), so this is the whole door.
2. `CMDS.OVL:0x0552 cmd_camp_holeup` prompts `"For how many hours? "` (DS `0x4209`), sleeps the roster `'G'→'S'`, prints `"Zzzzzzz...\n"`, blanks the viewport, and enters its clock loop `0x0634-0x068d`. Each iteration advances the clock by **ten minutes** (`0x0647 push 0x0a` to kernel `0x4F7C kernel_advance_clock`) — six iterations per game hour.
3. Inside that loop, `0x0677 call 0xffffbb0e` = kernel thunk `0x7A8E` = **`TOWN.OVL:0x1694 town_populate_npcs`**. `0x068d je 0x634` jumps back, so it runs on **every** tick.
4. `town_populate_npcs` is a single routine over a single 1988 table. `0x16a2-0x16b9` calls kernel `0x3A74 set_actor_record` with six zero fields for slots 1..31 — wiping the whole interior object register at DS `0x5C5A` — and zeroes every live NPC `objIdx` (`0x16ae mov word [di],0`, `di` walking `0x5F7A` by `0x10`). `0x16c9-0x171b` then re-places **every** `.NPC` slot whose type byte at `0x659E` is non-zero, via `TOWN.OVL:0x1726 town_npc_place`.
5. A chest is `.NPC` type **1**. `town_npc_place` at `0x178e cmp byte [bx+0x659e],1 / jne 0x179c` sends type 1 straight to `0x1795 mov word [bp-6],0x1e`, **skipping** the per-location killed bitmask at DS `0x28C2` that every other type is tested against. `[bp-6]` is pushed as the `+5` field of `set_actor_record` (push order verified against `0x3A74`'s `bp+6..bp+0x10` writes and `ret 0xe`), so the chest is recreated with contents byte **`0x1E`**, trap bit `0x80` clear.
6. Nothing anywhere records that a chest was opened. `SJOG.OVL:0x112C open_chest_world` at `0x11d6-0x11e1` calls the same `0x3A74` with six zeros — it **blanks the slot and writes no flag**.

**Answers to the batch's six questions:**

| | Answer | Citation |
|---|---|---|
| **A. What respawns** | The entire interior object register: every `.NPC` slot with a non-zero type — chests, inert props, plot artifacts — re-placed at its schedule position for the current hour. Not map tiles, not search objects, not dungeon or combat state | TOWN.OVL `0x1694`/`0x1726` |
| **B. Trigger** | `(H)`ole up on a bed tile `0xAB` inside a small map, once per 10-minute tick. **Not** outdoor camp (kernel `0x3C9A` never calls it), not an inn, not a save/load, not a map reload timer | census of thunk `0x7A8E` = 1 call site, CMDS `0x0677` |
| **C. Minimum timing** | The prompt accepts 1–9 hours; the reset fires on the **first ten-minute tick**, so the documented "minimum rest" is one hour and the refill is already done before it elapses | CMDS `0x0631` to `0x063b` to `0x0647` to `0x0677` |
| **D. Skull-key door** | **Stays unmagicked.** The reset writes only the object register; the door lives in the 0x400-byte map buffer at `0x6608`, which is re-read only by `town_load_town_map` `0x0408`. Changing floors (`town_use_ladder` `0x052e` to `0x0408` with `fresh=1`) re-reads it, **which is exactly why the community report says a second skull key is needed only after a floor change** | CAST `0x18f4` writes volatile terrain; TOWN `0x0408`/`0x052e` |
| **E. Contents** | **Deterministic and identical every time: `+5 = 0x1E`, untrapped.** No RNG, no reroll, no table lookup, no trap re-arm, no preserved jimmy state | TOWN `0x1795` |
| **F. Scope** | **Every interior small map** that runs the TOWN loop (town, castle, keep, dwelling) — *not* LB's castle specially. Not the overworld (no beds, and overworld objects persist in SAVED.OOL), not dungeons, not combat | the single call site plus the `0xAB` gate |

**The authored vault is real data, and it matches the hardware report exactly.** `CASTLE.NPC` record `(17-1)&7 = 0` carries type `0x01` in slots **23, 24, 25** at (16,21), (17,22), (13,23), all with `z = 0xFF` — the basement, runtime floor -1 — plus a type `0x1E` dead body in slot 28 at (9,9). The same basement map carries beds (`0xAB`) at **(13,19) and (16,19)**, four tiles from the chests, and the magically sealed doors `0x98` at (8,12)/(12,12)/(16,12) and `0x97` at (15,24) that a skull key unmagics. The only ladder out of that basement is at (12,7). Every clause of the community report is confirmed.

### H-148 — native root cause

The port splits the 1988 table in two: NPCs live in `NpcActors`, objects in the quest-object pool. `rest.cpp::bed_sleep_step` drove only the NPC half, through `RestServices::snap_npcs`; the object half (`hydrate_interior_objects`) was wired **only** to `ReloadEffect::HydrateInterior`, which `transitions.cpp::load_small_map` emits on map entry. So a looted vault stayed looted until the party left the location entirely.

The TypeScript reference port has the **same** split and the **same** omission — `game.ts::wakeSnapNpcs()` is `npcManager.enterMap(...)` and nothing else, hooked at `camp.ts:314` with a comment correctly citing `0x0677 -> TOWN 0x1694`. This is a **reference-port difference**: judged against DOS the reference is wrong here, so native was fixed against the binary, not against TypeScript.

### H-148 — RED proof

`native/core/tests/batch21b_chest_reset_test.cpp` (new ctest target `batch21b_chest_reset`), driven through the real `execute_command` / `hydrate_interior_objects` / `(O)pen` production paths with the authored `CASTLE.NPC` slots and the authored basement bed.

Against unmodified production code (`native/core/batch21b-red.log`):

```
batch21b check failed: H148-C ** all three chests are back after the minimum hole up **
exit=1
```

A and B passed — the vault hydrates with three chests at their authored cells, untrapped, and `(O)pen` empties it — and C failed at exactly the missing behaviour.

### H-148 — the fix

`native/core/src/commands.cpp`, the `CommandKind::Rest` case. For the **bed** path only, the host's `RestServices` is wrapped so the per-tick hook runs **both halves of `town_populate_npcs`**: the host's NPC snap, then `hydrate_interior_objects(c, location)`. `occupied` is forwarded through the same wrapper so the occupancy probe still reaches the host. Nothing else changed: no coordinates are hard-coded, no timer was invented, no chest system other than the interior `.NPC` register is touched, and `hydrate_interior_objects` is the *same* function map entry already used, so the reset inherits the authored contents, the taken-artifact gate and the discard-then-reseed order unchanged.

Camp, dungeon rest, ship repair, save/load and R-14 terrain persistence are untouched.

### H-148 — GREEN proof

`batch21b_chest_reset`: **38/38 checks pass.**

* **H148-A** three chests at (16,21)/(17,22)/(13,23) plus the slot-28 prop, all untrapped.
* **H148-B** `(O)pen` removes each chest and leaves the prop alone.
* **H148-C** one hour of hole-up = six ticks, and all three chests are back at their authored cells.
* **H148-D** three consecutive loot-then-rest cycles, each refilling — no cooldown, no once-only bit, no duplicate props.
* **H148-E** hole-up emits no `ClearTerrain`, `ResetDoors` or `HydrateInterior`, so an unmagicked skull-key door survives it.
* **H148-E2** a chest authored onto the bed cell throws the party out on the **first** tick — pinning that the reset runs *before* the `0x0688` occupancy probe of the same iteration, and that the probe still reaches the host.
* **H148-F1** camping outdoors refills nothing. **F2/F2b** a taken plot artifact is not resurrected while an untaken one is re-placed exactly once. **F3** a search find is never re-armed. **F4** un-collected floor loot **is** destroyed by the wipe half — faithful, and the reason the 1988 farm needs a `(G)et` before the rest.
* **H148-G** the refilled vault is pool state (openable again), and a redundant map-entry hydrate on top of a reset does not duplicate.

### H-148 — mutation proof

Each load-bearing part mutated, built, run, reverted (`native/core/batch21b-mutation-m*.log`):

| # | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | object half removed from the per-tick hook | **DEAD** | H148-C chests back |
| M2 | trigger inverted — repopulate on the camp path instead of the bed path | **DEAD** | H148-C chests back |
| M3 | wipe half removed (`discard_interior_objects` dropped from hydration) | **DEAD** | H148-C chests back (duplicates) |
| M4 | host NPC half dropped from the wrapper | **DEAD** | H148-C six ticks per hour |
| M5 | occupancy probe no longer forwarded to the host | **DEAD** | H148-E2 first tick ends the sleep |
| M6 | taken-artifact gate defeated | **DEAD** | H148-F2 taken plot artifact does not hydrate |

All six died. The suite was re-confirmed green after the last revert.

---

## Batch 21B — Original Behavior Gap Sweep

### Coverage, stated honestly

Categories **A (loot/economy)**, **B (doors/locks/terrain)** and **C (rest/sleep/camp)** were swept from the binary, instruction by instruction, because H-148 lands in them. Categories **D-J** were swept only by (i) the exhaustive by-band census — which proves nothing outside CMDS `0x0677` reaches the reset routine — and (ii) re-reading `docs/bugs-del-original.md`, `re/deliberate-divergences.md` and this audit's open rows. **They did not get their own binary pass, and no row below claims otherwise.** A future batch that wants D-J swept properly should budget for it rather than inherit this one's confidence.

### The matrix

| ID | Subsystem | Behaviour | DOS evidence | TypeScript | Native | Player-visible impact | Sev | Repro | Classification | Follow-up | HW test? |
|---|---|---|---|---|---|---|---|---|---|---|---|
| — | Loot | Interior chests refill on a bed hole-up | CMDS `0x0677` to TOWN `0x1694` | omits the object half | **fixed this batch** | the LB-basement farm works | — | 100 % | **FAITHFUL** (as of this batch) | H-148 closed | yes |
| — | Loot | Nothing else in the game refills chests: no dungeon, combat, overworld or shop path reaches the routine | by-band census: 1 call site for thunk `0x7A8E` | same | same | — | — | — | **FAITHFUL** | — | no |
| — | Loot | Un-collected floor loot is destroyed by the reset | the wipe half zeroes slots 1..31 | n/a | pinned by H148-F4 | forces a `(G)et` before the rest | — | 100 % | **FAITHFUL** | — | no |
| — | NPC | A killed NPC does not come back from the reset | `town_despawn_object` `0x00b0` zeroes the `.NPC` type byte at `0x659E` | modelled | `npc_dead` mask | — | — | — | **FAITHFUL** | — | no |
| — | Doors | Doors do **not** auto-close during a hole-up | the per-tick call list (`0x20FA`, `0x4F7C`, `0x4A84`, `0x2AE8`, `0x2900`, `0x1694`, `0x368E`) contains no door housekeeping | same | same | — | — | — | **FAITHFUL** | — | no |
| — | Doors | A skull-key-unmagicked door is volatile terrain, reverted only by a map reload | CAST `0x18f4`; TOWN `0x0408` re-reads the record | `setVolatileTerrain`, sealed in `doors.test.ts` | same | — | — | — | **FAITHFUL** (but see H-158) | — | no |
| **H-154** | Rest | Bed hole-up does not snap NPCs to their schedule **on the device** | CMDS `0x0677` to TOWN `0x1694`, NPC half | modelled | `alpha_runtime.cpp:232` wires `snap_npcs` to `[](void*){}` | NPCs stand still through a night's sleep | med | 100 % | **CONFIRMED MISSING** → **HOST FIXED in Batch 29** (`bind_rest_services()`: `snap_npcs_to_schedule`; `batch29_rest_wiring`). TS "modelled" is a rebuild, not 1988's reposition: see the Batch 29 row below | device retest Phase 6W | yes |
| **H-155** | Rest | "Thrown out of bed!" can never fire on the device | CMDS `0x0688` to kernel `0x368E kernel_object_at` | modelled (`objectOrNpcAt`) | `occupied` wired to constant `false` | a whole reference outcome is unreachable | med | 100 % | **CONFIRMED MISSING** → **HOST FIXED in Batch 29** (`occupied` → `object_or_npc_at`, NPCs + objects; `batch29_rest_wiring`) | device retest Phase 6W | yes |
| **H-156** | Rest | Bed hole-up runs no per-tick turn housekeeping | CMDS `0x0671` to kernel `0x2AE8 kernel_turn_housekeeping`: poison 1 HP, meals at 6/12/18, `Starving!`, turn counter, Q/T expiry, regeneration ring | **also omits it** | `bed_sleep_step` only advances the clock | sleeping costs no food, never starves, never ticks poison, never regenerates | **high** | 100 % | **CONFIRMED MISSING** (both ports) | own batch — moves survival fixtures | yes |
| **H-157** | Terrain | Hole-up does not run the day/night tile refresh | CMDS `0x0664` to TOWN `0x0170 town_schedule_tile_refresh` when the hour becomes 5 or 20 | omits it | `WorldTerrain::hourly` refreshed only on a town-turn hour change, map entry or klimb | sleep across 20:00/05:00 and the drawbridge-and-lamp overlay is stale until you leave | med | 100 % | **CONFIRMED MISSING** (both ports) | with H-156 | yes |
| **H-158** | Terrain | Changing floors inside a small map does not reload the map record | `town_use_ladder` `0x052e` to `town_load_town_map(fresh=1)` `0x0408` (re-reads the 0x400 record **and** calls `0x1694`) | not modelled | `klimb_ladder`/`apply_stair_step` emit only `RefreshHourTiles` | an unmagicked skull-key door survives a floor change when 1988 relocks it; the vault does not refill on a floor round-trip | med | 100 % | **CONFIRMED MISSING** (both ports) → **FIXED in Batch 23** (both ports; the chest refill on a floor change is part of it) | own batch — touches R-14 terrain persistence | yes |
| **H-159** | Loot | Interior chest contents byte is 8; the binary seeds `0x1E` | TOWN `0x1795` `mov word [bp-6],0x1e` to `+5` via kernel `0x3A74` | `INTERIOR_CHEST_CONTENTS = 8` | `o.contents = 8` | every interior chest's loot roll is off the authored value | med | 100 % | **CONFIRMED MISSING** — closes oracle hole **O5** (`re/notes/objects.md`) → **FIXED in Batch 23** (both ports) | own batch: will move `gameplay_parity`/`quest_parity`, needs the TS side regenerated in step | no |
| **H-160** | Camp | Outdoor camp guard walks through the fire and through sleepers | `camp_guard_walk` consumes `cell_free` | modelled (`campCellFree`) | `cell_free` wired to constant `true` | cosmetic on the device today | low | 100 % | **CONFIRMED MISSING** → **RECLASSIFIED in Batch 29: UNREACHABLE ON DEVICE.** The device never posts a watch (`member = -1`, `guard_start` unbound), so CMDS `0x0337` skips the walk and `cell_free` has no caller (mutation M5x survives). No watchman exists to walk through the fire; the real gap is H-167 | with H-167 | no |
| **H-161** | NPC | A floor change does not reposition NPCs to their schedule | `0x052E` → `0x0408(1)` → `0x1694`, NPC half `0x1841-0x1856` (every slot with a type byte, every floor, live schedule) | not modelled (`enterMap` only on map entry) | `reload_floor` emitted no NPC effect | NPCs stay mid-walk across stairs/ladders | med | 100 % | **CONFIRMED MISSING** (both ports) → **FIXED in Batch 24** (both ports; `snap_npcs_to_schedule` / `snapToSchedule`) | — | yes |
| **H-162** | Doors | An Open door survives a load | `0x00f7` → `0x11F0(fresh=0)` → `0x0408(0)`, `0x041d` zeroes `[0x594f]` | `main.ts` restored `openDoors` | load paths restored `CommandState::door` | a door open at save time is open after the load | low | 100 % | **CONFIRMED MISSING** (both ports) → **FIXED in Batch 24** (both load paths in each port) | — | yes |
| **H-163** | Terrain | Town-fight end does not re-read the floor | `0x09BC` → `0x6150` → `0xb0` → `0x0408(0)` (`0x09d9-0x09dc`), no branch | `endCombat` did not | `finish_encounter_combat` did not | a skull-keyed lock stays unlocked after a town fight; open door / NPCs / chests already matched | low | 100 % | **CONFIRMED DIVERGENCE** (transient terrain; both ports) → **FIXED in Batch 24** | — | yes |
| **H-164** | Save | `Alt+L` skips `synchronize_loaded_world()` | — (device routing) | n/a | object pool / dungeon session / scenes not reset by the `DeviceShortcut::Load` arm | stale world objects can survive a quick load | med | code reading | **CONFIRMED BY CODE READING — queued** (Batch 24) → reproduced on host (Batch 26) → **FIXED in Batch 27** (the arm calls `synchronize_loaded_world()`; `batch27_alt_load`) | — | yes |
| **H-166** | Save | The generation gate does not validate the `"dungeon"` / `"worldObjects"` sidecar | — (device persistence; one 1988 save window, `CAST2.OVL:0x10FE` / `INTRO.OVL:0x0EB4`) | n/a | `candidate()`/`restore_candidate()` checked CRC, parse, gameplay/terrain/NPC walk only; the two sidecar owners were decoded after the commit, falling back to no session / an empty pool | a well-formed but invalid newest generation wins over a good older one; the load is a mixture no save held | low | only a corrupted card | **CONFIRMED BY CODE READING** (Batch 26) → **reproduced on host and FIXED in Batch 28** (one gate, `stage_generation()`; `batch28_save_validation`) | — | no (host-certified) |
| — | Rest | `snap_npcs` is an NPC-only hook, so the object half of one binary routine is unmodelled | TOWN `0x1694` is one routine | `wakeSnapNpcs` to `npcManager.enterMap` only | **corrected this batch** | — | — | — | **REFERENCE-PORT DIFFERENCE** — native is now right and TypeScript is not; no parity fixture encodes it (91/91 green) | flag before any fixture regeneration | no |
| — | Rest | `bedSleepStep` omits `0x0671` and `0x0664` | as H-156/H-157 | omits | omits | — | — | — | **REFERENCE-PORT DIFFERENCE** compounding H-156/H-157 | with them | no |
| **H-170** | Rest | Bed sleep used a fixed tick count, missing the original target hour under Q and at midnight | CMDS `0x059e-0x05b0` stores `g_hour + digit`, subtracting **23** if above 23; `0x063b` compares the live hour before each ten-minute tick | fixed-count comparison only | **HOST FIXED Batch 35**: target-hour loop with original 23 subtraction | Q may prolong sleep after its expiry; original midnight quirk may add an hour | high | shipped castle raw-key boundary fixture | **HOST FIXED** (42/42 focused; six mutations killed; 105/105 host) | no new physical phase | no |
| — | NPC | Loading a save inside a town: the binary does **not** re-read the `.NPC` (`fresh = 0`) | TOWN `0x11FF/0x1203`; `ULTIMA.EXE:0x00F4` | re-derives | re-derives | ports show a populated town where 1988 shows an empty one | — | — | **DELIBERATE DIVERGENCE** | `re/notes/npc-carga-partida-fresh-gate.md` §6/§8, `re/deliberate-divergences.md` | no |
| — | NPC | The `+5` byte written for **non-chest** `.NPC` slots — `0xFF` or `0` from a per-location dword bitmask at DS `0x28C2` | TOWN `0x179c-0x17d4` | — | — | unknown | ? | — | **UNRESOLVED** — no note anywhere in the repo names DS `0x28C2`; chests bypass it, so H-148 is unaffected. **Batch 29, partly resolved:** a static DATA.OVL table (file `0x28D2`), one dword per `g_location`, **not** the dead bitmap (`0x5B56`); non-zero only at locations 4, 5, 28, 29 (and index 0). The meaning of `+5 = 0xFF` is still open; `0x368E` does not read `+5` | needs a DS-map pass | no |
| — | Rest | Whether the bed loop has any ambush/interruption roll beyond the occupancy probe | `0x0688` is the only gate decoded; kernel `0x2AE8`'s callees were **not** exhaustively walked | camp-only ambush | camp-only ambush | unknown | ? | — | **UNRESOLVED** — stated rather than assumed. **Batch 29:** inside the loop, still only `0x0688`; *before* it, `0x05b4-0x05d4` can cancel the sleep, see H-169 | with H-156 | no |
| **H-167** | Camp | The device camp never posts a watch | kernel `0x3e2c`, `0x3ea5-0x3ecd`: with ≥2 `G`/`P` members, ask watch and select one; only `G` may guard; invalid/cancel gives "None posted!" then an unwatched camp. Guard skips partial heal (`CMDS:0x0461`) and walks (`0x0337`) | comparison only | before Batch 32, hours went straight to `member=-1`; `guard_start` absent, `cell_free` lacked guard identity | no watch prompt; everyone healed; watched RNG path unreachable | med | 100 % | **HOST FIXED — Batch 32** (35/35 raw-key checks, six killed mutations; device Phase 6Y pending) | Phase 6Y | yes |
| **H-168** | NPC | A cannon-killed NPC stayed active until map re-entry | CMDS `0x0d47-0x0d82`: clear object (`0x3A74`), slot lookup TOWN `0x011e`, eligible dead bit `0x0052`, clear live slot `0x00b0` | `townNpcDeadBitSet` + `clearSlot` (Batch 33) | `dialogue_despawn` from cannon hit (Batch 33) | killed NPC is untargetable and absent from movement immediately; nonpersistent guards return only on re-entry | low | 100 % | **HOST FIXED** (Batch 33, shipped-pack RED/GREEN and parity) | no new physical phase | no |
| **H-169** | Rest | The pre-sleep NPC passes were unmodelled | CMDS `0x05b4-0x05d4`: at the current hour call NPC.OVL `0x0db4`, redraw (`0x5910`), test marker `0x61`, repeat up to 16 times; adjacent AI 6/7 arms `0x61`, later AI 4/5 talk can overwrite with `0x74` | omits | **HOST FIXED Batch 34**: full `tick_npcs` passes, redraw and hostile cancellation before bed begin | NPCs move/RNG advances before sleep; adjacent hostile can abort with no sleep tick | high | authored hostile fixture | **HOST FIXED** (21/21 raw-key checks; eight mutations killed; 104/104 full host) | no new physical phase | no |
| **H-172** | Rest presentation | Original bed-entry viewport fill was absent | CMDS `0x0614-0x0624`: color-zero copy fill (8,8)-(183,183), after status `S` and `Zzz`, before first tick; map stays black until return redraw | unadjudicated | **HOST FIXED Batch 37**: synchronous Board rectangle fill and cache invalidation | black map window through all bed ticks; no game-state mutation | low | accepted bed sleep | **HOST FIXED** (12/12 framebuffer checks; seven mutations killed; 107/107 host) | Phase 6Z TFT check | yes |
| **H-174** | Rest status presentation | Original refreshes party panel after `G→S` and during bed ticks; native does not present intermediate roster frames | CMDS `0x060a` and `0x0674` status draws, separate from viewport `0x0624` | unadjudicated | status byte is ordered, but synchronous Board panel refresh is absent | interim roster may show pre-sleep status | low | accepted bed sleep | **QUEUED, DISTINCT from H-172**; no Batch 37 fix | later presentation batch | no |
| **H-171** | Camp | The original redraw wind roll and Camp-entry Q/T clear were missing; the queued claim of per-step survival housekeeping was incorrect | CMDS `0x001c/0x001f` clear spell count/effect; `0x0204` calls viewport redraw `0x5910` (wind through `0x2f62`), `0x0207` ring `0x400c`, `0x020a` status draw `0x2900`; hourly encounter at `0x021d`; `0x0314` clock +5, `0x031b` delay `0x20fa(1)`, optional `0x4a84` sky/status display, then guard. No `0x2ae8` call | TypeScript comparison excluded for Camp only | **HOST FIXED Batch 36:** native clears Q/T at accepted Camp entry, calls wind before ring/clock each five-minute step, and checks encounter after the next-hour wind/ring | wind state and RNG/encounter order now match the original call path; poison, food, starvation, turn count and timed-turn decrements remain absent | high | original-byte disassembly plus shipped-pack runtime | **HOST FIXED** (see Batch 36 evidence below) | no new physical phase | no |
| **H-173** | Camp duration | Camp from a nonzero minute overran the original target hour | CMDS 0x0066-0x0079 stores (start hour + requested hours) modulo 24; 0x01ee-0x01f8 compares only the live hour at every loop head, before redraw. Starting minute is not saved as a target. | TypeScript comparison is not an oracle | **HOST FIXED Batch 38:** native now stops at the target hour after whole five-minute steps; the last step still moves the guard | 05:50 + 1 ends 06:00 after 2 steps rather than 06:50 after 12; wind/ring/guard and intervening encounter draws now have original counts | med | shipped original bytes plus raw-key runtime test | **HOST FIXED** (RED 12/31, final 43/43; six mutations killed; 108/108 host) | no device-specific phase | no |
| — | Rest | The TypeScript bed snap is a `.NPC` rebuild, not 1988's reposition | TOWN `0x1694` repositions type≠0 slots from the live schedule, stuck kept | `wakeSnapNpcs` → `npcManager.enterMap`: re-reads the file (dead bits honoured), resets stuck | `snap_npcs_to_schedule` (Batch 29) | TS only: an `0x00B0`-cleared guard returns after a sleep; an alarmed guard goes to its file post | — | 100 % | **REFERENCE-PORT DIFFERENCE** (Batch 29 mutation M2b = the TS shape, killed by A4/B2d/B2e). No fixture pins it | fix in TS only with a `--check` control | no |
| — | Ships | Frigate delivery price does not match and accepting does nothing | `bugs-del-original.md` §1.4 | pending | pending | — | — | — | **OUT OF ALPHA 2 SCOPE** | §1.4 | — |
| — | Dungeon | A dungeon chest on floor 0 hangs the game | §1.5 | pending | pending | — | — | — | **OUT OF ALPHA 2 SCOPE** | §1.5 | — |
| — | Quests | Wishing well's horse appears inside a wall | §1.7 | divergent | divergent | — | — | — | **OUT OF ALPHA 2 SCOPE** | §1.7 | — |
| — | Items | Skull Key inside a dungeon is spent and opens nothing | §1.9 | pending | pending | — | — | — | **OUT OF ALPHA 2 SCOPE** | §1.9 | — |
| — | Items | Passing at the Skull Key prompt spends it and blows up your own square | §1.10 | divergent | divergent | — | — | — | **OUT OF ALPHA 2 SCOPE** | §1.10 | — |

### Counts

| Classification | Rows |
|---|---|
| FAITHFUL | 6 |
| DELIBERATE DIVERGENCE | 2 |
| CONFIRMED MISSING ORIGINAL BEHAVIOR | 7 |
| REFERENCE-PORT DIFFERENCE | 2 |
| UNRESOLVED | 2 |
| OUT OF ALPHA 2 SCOPE | 5 |

**Nothing in the "confirmed missing" column was fixed.** H-148 is the only production change in this batch. H-154 is the *sibling half of the very routine H-148 restored* and was still left alone deliberately: the core now runs the object half for every host, while the NPC half remains a host-wiring gap whose fix moves NPCs on screen during sleep and deserves its own RED proof.

### Full regression suite

**91/91 from a clean, from-scratch build**, 0 fail, 0 skipped (`native/core/batch21b-final-ctest-rerun.log`) — the prior 90 plus `batch21b_chest_reset`. No parity corpus moved: `gameplay_parity`, `quest_parity` and all 14 `typescript_*_drift` checks pass unchanged, which says no fixture sequence exercises a bed hole-up in a location carrying `.NPC` objects. One warning, the pre-existing w64devkit `stl_uninitialized.h` `-Wstringop-overflow=` false positive; zero project warnings.

⚠ **One run is declared rather than buried.** The *first* clean-build `ctest` (`batch21b-final-ctest.log`) was launched while the ESP-IDF firmware build was saturating the machine, and `quest_parity` failed there. The failure is an **I/O artifact, not a mismatch**: `check-quests.ts` choked on a *truncated* driver stdout line — `"scrollQuantities":[0,0,0,0,0,0,0,0,"potionQuantities"`, a missing `]` mid-array — with `SyntaxError: Expected ',' or ']' after array element in JSON at position 759`. It is not the Batch 17 signature either (that was exit `3221225477`, `STATUS_ACCESS_VIOLATION`). A targeted re-run passed immediately, and the full suite re-run on an idle machine passed 91/91. Both logs are kept. The lesson is mine: do not run the two toolchains concurrently and then read the result as a verdict.

### Firmware

Built clean with ESP-IDF 6.1: `openu5_tdeck.bin` = **0xd22d0** (860,880 bytes), up **160 bytes** from Batch 21A.3's 0xd2230 (860,720) — the `RestServices` wrapper and its two thunks. `0x2dd30` (187,696 bytes, **18 %**) of the app partition free. Bootloader 0x5850, 31 % free. **0 errors, 0 project warnings**; the five `component_validation.cmake` notices are third-party, as in every build. **Not flashed.**

### SD card

Unchanged. No resource pack, asset or fixture was touched — the fix is core logic over data the pack already carries.

### Status

H-148: **SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** All other sweep rows are classification only. Batch 21A / 21A.1 / 21A.2 / 21A.3 conclusions are untouched.

### Phase 6P — Batch 21B H-148 chest reset · *firmware only; the SD card is unchanged*

Flash the Batch 21B firmware. Party needs at least one skull key.

1. Enter Lord British's Castle (location 17) and klimb **down** to the basement.
2. Unlock the vault normally: `(U)se` Skull Key at one of the magically sealed doors — `0x98` at (8,12), (12,12) or (16,12), or `0x97` at (15,24) — then `(O)pen` it.
3. `(O)pen` and empty the three chests at **(16,21), (17,22) and (13,23)**. `(G)et` the loot off the floor — the 1988 reset destroys anything left lying there.
4. Walk to the basement bed at **(16,19)** or **(13,19)** and `(H)`ole up for **1 hour** — the minimum the prompt accepts.
5. **Expected:** all three chests are back at those exact cells and can be opened again for loot.
6. **Expected:** the skull-key door is still open — **no second key is needed**.
7. Repeat steps 3-5 once. It must refill again; there is no cooldown.
8. Klimb **up** one floor and back down. Native will *not* relock the door here and 1988 would — that is **H-158**, already queued; note it but do not file it as new.

**Pass criteria:** steps 5-7. **Fail** if the chests stay empty, if they come back trapped, if the door relocks, or if a chest appears anywhere other than those three cells.

**Known-and-queued, do not file:** NPCs will not move while you sleep (H-154), "Thrown out of bed!" cannot fire (H-155), sleeping costs no food and does not tick poison (H-156), and sleeping past 20:00 leaves the drawbridge overlay stale (H-157). Serial is not required for a clean pass; capture it only if behaviour diverges.

## Batch 22 — Lord British's Castle basement chests missing on the T-Deck (diagnostic batch)

### The device observation

On the real T-Deck, newest firmware, brand-new save: Lord British's Castle basement (location 17, floor -1) loads and renders, but the three authored vault chests at **(16,21), (17,22), (13,23)** are absent and plain floor is drawn in their cells. Host probes from Batch 21B had already shown the core *can* hydrate them — but both probes supplied their own `QuestWorldServices` and their own reload callback, so neither ever ran `AlphaRuntime`'s object callbacks or `AlphaRuntime::command_reload()`.

### The two real runtime paths into the basement

**A. Walk in — (E)nter, then (K)limb down.**
`UiInputAdapter` → `UiSession` → `AlphaRuntime::command()` → `execute_command()` → `Runner::enter()` → `transitions.cpp::load_small_map(17)` emits, in order, `EnterNpcs`, `ClearEnemies`, `ResetDoors`, `ClearTerrain`, **`HydrateInterior`**, `RefreshHourTiles`, `UrbanEffects` → each goes to `Runner::transitions()` (`commands.cpp:294`), which **runs `hydrate_interior_objects(r.c, 17)` itself** (`commands.cpp:309`) and only then forwards the effect to the host (`AlphaRuntime::command_reload`). Hydration: select `context_.npc_data[16]` (= `resources_.npc_locations[16]`, from `npcs.bin`) → `quest_.reserve` = `AlphaRuntime::object_reserve` → `discard_interior_objects(17)` → per slot `quest_.append` = `AlphaRuntime::object_append` (`objects_.push_back`). All floors of the location are seeded at once, basement slots with `z=0xFF → floor -1`. `(K)limb` on the floor-0 ladder (tile 201) → `klimb_ladder(-1)` → `RefreshHourTiles` + `ContextTurn`; nothing touches the pool. First frame: `AlphaRuntime::render()` → `get_active_map(resources_.world, L17/F-1)` → `compose_world_presentation(context_, …)` → layer-1 object pass reads `quest_.count/read` = `objects_` → actor clock → world fx → `render_snapshot(tile_cache_, snapshot, …)`.

**B. Developer Teleport** (developer tools are compiled into the Alpha firmware: `OPENU5_ENABLE_DEVELOPER_TOOLS=ON`).
`UiDebugMenu::apply_action()` → `apply_debug_teleport(context_, r)` (`debug_map_picker.cpp`, SmallMap arm) sets the position, then calls the file-local `reload()` for `ResetDoors`, `EnterNpcs`, **`HydrateInterior`** — and that helper only forwards to `c.services.reload`, i.e. **straight to `AlphaRuntime::command_reload()`, bypassing `Runner::transitions()`**. `command_reload()` has arms for `EnterNpcs`, `ClearEnemies`, `ClearTerrain` and `RefreshHourTiles` only. **`HydrateInterior` is dropped on the floor.** NPCs appear (the device handles `EnterNpcs` itself), the pool stays empty, and the composer paints terrain.

### Root cause (proven on host, with the device's own runtime and data)

`debug_map_picker.cpp::reload()` treated `HydrateInterior` as a host effect. It is core-owned everywhere else: `Runner::transitions()` consumes it before notifying the host, and the reference recipe the picker claims to copy (`DebugApi.goToLocation` / `teleportSmallMap`, `game/src/debug/debugApi.ts:310-343`) calls `game.hydrateInteriorObjects(location)` directly. `debug_map_picker_test.cpp` only asserted that the effect was *emitted* to a recording host, and the Batch 21B probes consumed it in their own callback — so no test ever ran the device's consumer. **Classification: port defect in the device glue (a core effect with no device consumer), not reference behaviour and not authored data.**

### RED proof

New ctest target `batch22_basement_objects` (`native/targets/tdeck/host_tests/batch22_basement_objects_test.cpp`). It links the **real, unmodified `alpha_runtime.cpp`** (Batch 11 seam) and loads the **real shipped `native/assets/openu5-alpha1-resources.bin`** through the production `AlphaResourcePack` (payload CRC `0x2065ad91`), so the `.NPC` tables, castle floors, overworld and location table are the device's own bytes. Against the pre-fix picker: **17/23 GREEN, 6 RED** (`native/core/batch22-red.log`) — T1 (source table) and T2 (walk-in, hours 12 and 3) GREEN; **T3b/T3c/T3d RED on both Developer Teleport variants**, pool size 0, and the snapshot drawing tile **68 (floor)** on all three chest cells — the device symptom exactly.

### The fix (one production statement)

`debug_map_picker.cpp::reload()` now runs `hydrate_interior_objects(c, location)` for `HydrateInterior` when `c.quest_world` is bound, then forwards as before. `hydrate_interior_objects()` discards the location's interior objects before re-seeding, so a repeat teleport cannot stack a second vault (T3d). The walk-in path is untouched. `ResetDoors` has the same shape on this route (the device has no arm for it) — **not changed here**, queued below.

### GREEN and mutation proof

After the fix: **23/23 GREEN** (`batch22-green.log`). T1/T2 were green on first run, so they were mutation-validated against production code and reverted:
- **M1** — drop the `0xFF → -1` floor decode in `hydrate_interior_objects`: **12 RED** (T2b/T2e/T2f at both hours, T3b/c/d on both variants) (`batch22-mutation-m1.log`).
- **M2** — remove `Runner::transitions()`'s `HydrateInterior` consumer: **6 RED** (T2b/T2e/T2f at both hours) (`batch22-mutation-m2.log`).

### Other Phase E questions, answered separately

| Question | Answer | Evidence |
|---|---|---|
| Which heap backs `objects_`? | `std::vector` default allocator → `operator new` → `malloc`. With `CONFIG_SPIRAM_USE_MALLOC=y`, `SPIRAM_MALLOC_ALWAYSINTERNAL=4096`, a block of 4 KiB or less is tried in **internal RAM first**. Location 17 reserves 32 × 68 = 2,176 bytes. | `sdkconfig`; `sizeof(QuestObject)=68` from the host trace. The device heap is **not yet observed** — `U5OBJ RESERVE_DONE storage_heap=` reports it. |
| Does the PSRAM guard match the allocator? | **No.** `object_reserve()` gates on `heap_caps_get_free_size(PSRAM) >= need + 32 KiB`, but the storage is most likely internal. The guard is merely conservative; with ~7 MB of PSRAM it cannot plausibly fail. Left unchanged, as instructed. | code reading; `U5OBJ RESERVE … result=` on device |
| Can `reserve()` fail or throw? | The guard can return `false` → `hydrate_interior_objects` returns `false` **before** its discard, so nothing is cleared first. `CONFIG_COMPILER_CXX_EXCEPTIONS` is off, so a real allocation failure inside `vector::reserve` **aborts** (panic + reboot); it cannot fail silently. | `sdkconfig`; `quest_world.cpp` order |
| Is a hydration failure surfaced? | Walk-in: `NeedsStorage` → the existing `Command failed status=NeedsStorage` warning. Teleport: the picker still ignores the `bool`, but `U5OBJ POST_HYDRATE result=0 reason=…` now reports it. | code |
| Does later initialization wipe the pool? | Not on the walk-in path (T2e: intact after `(K)limb`). The only `objects_.clear()` calls are load/restore and New Journey, both now logged (`U5OBJ CLEAR site=…`). | T2e; code |
| Does save restore replace hydrated objects? | Restore is verbatim from the sidecar, chests included — **but** a save taken after a Developer Teleport under the old firmware captured the empty pool; loading it still shows no chests until the party re-enters the castle. | `gameplay_save.cpp:45-62` |
| Basement floor/z on device? | Same code, same bytes: slots 23/24/25 (and 28, the `(9,9)` control) are `z=255,255,255`, decoded to floor -1. | `U5OBJ SRC`/`HYDRATE` lines, T1b/T1c |

### What is proven vs. not

- **Proven:** the Developer Teleport route never hydrated interior objects on the device runtime; the fix restores them in the pool and in the composed snapshot, on the device's own data.
- **Strong hypothesis:** the hardware session reached the basement via Developer Teleport. Not established — the report does not say how the party got there.
- **Unproven:** that the walk-in route is also healthy **on hardware** (it is healthy on host with the real runtime and data). If the chests are still missing after this firmware, the `U5OBJ` trace will name the stage.

### Diagnostic instrumentation (temporary; remove after the hardware run)

All `U5OBJ` lines are gated on location 17 and the four tracked cells; every other location is silent. Core gains one behaviour-neutral observer, `QuestWorldServices::hydration_trace` (`InteriorHydrationTrace`: `begin`/`slot`/`end`), which the device binds in `initialize()` and the host fixture binds identically. Checkpoints: `SOURCE`/`SRC` (table selected, tracked slots with all three schedule positions), `RESERVE`/`RESERVE_DONE` (request, size/capacity, internal/PSRAM/default free + largest block, guard result, actual storage heap), `HYDRATE … ACCEPT|DROP reason=` (`empty-slot`, `npc-type-not-object`, `plot-item-already-taken`), `POST_HYDRATE result= reason=` (`ok`, `no-location`, `no-actor-owner`, `pool-services-missing`, `reserve-failed`, `no-source-table`) + `_OBJ` dump of the tracked cells on **any** floor, `RELOAD effect= hydrate_calls=` (the device being notified — if `hydrate_calls` did not move, nothing hydrated), `APPEND`/`ERASE`/`ERASE_BY_HYDRATION_DISCARD`/`WRITE`/`CLEAR site=`, `PRE_PRESENT` + `_OBJ` dump at the first basement snapshot, `PRESENT` per tracked cell (terrain, NPC tile, object presence/tile, final tile, visibility, which layer won) at entry and again the first time each cell enters the 11×11 window, and `RENDER` (the tile handed to `render_snapshot`).

### Queued, not fixed (same defect class)

- `debug_map_picker.cpp::reload()` forwards `ResetDoors` to a device consumer that has no arm for it: a Developer Teleport does not reset door timers on the T-Deck.
- `dungeon_orchestration.cpp::reload()` forwards `HydrateUnderworld` the same way on dungeon exit to the Underworld; `AlphaRuntime::command_reload()` has no arm for it, so the Underworld plot objects are not re-seeded on that route on the device.

### Full regression suite

Baseline before any edit, clean build (`native/core/build-batch22-baseline`): **91/91 serially** (`batch22-baseline-ctest-serial.log`). ⚠ Declared, not buried: the first baseline run used `ctest -j 6` and `gameplay_parity` died with **SEGFAULT** (`batch22-baseline-ctest.log`); it passed 3/3 in isolation and in the serial run, on unmodified code. A crash under load is not a parity mismatch, so it is queued for its own investigation rather than excused.

Final, from-scratch build (`native/core/build-batch22-final`): **92/92**, 0 fail (`batch22-final-ctest.log`) — the prior 91 plus `batch22_basement_objects`. No parity corpus moved. One warning, the pre-existing w64devkit `stl_uninitialized.h` `-Wstringop-overflow=` false positive; zero project warnings.

### Firmware

Built with ESP-IDF 6.1 into `native/targets/tdeck/build-batch22`: `openu5_tdeck.bin` = **0xd3890** (866,448 bytes), up 0x15c0 (5,568 bytes) from Batch 21B's 0xd22d0 — almost all of it the temporary `U5OBJ` format strings. `0x2c770` (17 %) of the app partition free; bootloader 0x5850, 31 % free. **0 errors, 0 warnings.** Launcher image: `build-batch22/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, SHA-256 `d0d0d32989f6eb4d6e60a4dbd6903fba09175b91fd60020b92d0a79989c5f545`. **Not flashed.** The first firmware attempt failed on three `-Werror=misleading-indentation` one-liners in the new trace code — the host targets compile `alpha_runtime.cpp` without `-Werror`, so only the firmware build catches that class.

### SD card

Unchanged.

### Status

Developer-Teleport route: **SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** Walk-in route: **HOST-VERIFIED, HARDWARE UNVERIFIED.** H-148 (Batch 21B chest reset) stays **HARDWARE RETEST REQUIRED** — its premise, that the chests are there to begin with, is itself unverified on hardware.

### Phase 6Q — Batch 22 basement chests · *firmware only; the SD card is unchanged*

Flash the Batch 22 firmware, start a New Journey, go to Lord British's Castle basement **however you normally do** and look at the vault. Capture every serial line containing `U5OBJ`. Pass: three chests drawn at (16,21), (17,22), (13,23).

## Batch 23 — Lord British's vault: chest loot and the floor-change reset (H-158, H-159)

**Scope:** the three authored basement chests of location 17 and the `0x97` vault door at (15,24). Production changes close **H-159** (chest contents byte) and **H-158** (a floor change must reload the floor). Nothing else was fixed; every other finding below is queued.

### The two hardware observations (Batch 22 firmware, confirmed by the user)

1. Repeatedly looting/resetting the vault yields only gold, torches and food.
2. Loot the chests, go upstairs, come back down: the chests stay looted and the magically locked vault door stays unlocked. Sleeping in the basement bed does refill the chests.

Batch 22's hydration fix is confirmed on hardware (the chests appear). It was not revisited.

### Baseline

HEAD `b6705142` (tag `alpha2-batch21b-original-behavior-sweep`). ⚠ **Batch 22 is not committed**: it exists only as working-tree changes (the firmware that was flashed). Batch 23 was built on top of that working tree; the Batch 22 diff was snapshotted before any edit. Clean from-scratch baseline (`native/core/build-batch23-baseline`), serial: **92/92, 0 fail** (`batch23-baseline-ctest.log`). No known-flaky test was hit; the Batch 22 `gameplay_parity` crash under `ctest -j 6` remains queued and was not reproduced serially.

### Original loot contract (recovered from the binaries)

Every citation below was re-read with `re/tools/dis16.py`; the tables were dumped from `DATA.OVL` (`fileoff = DS + 0x10`).

**When.** The contents byte is fixed when the chest is placed; the loot is rolled when the chest is **opened**.

- Placement: `TOWN.OVL:0x1726 town_npc_place`, type-1 branch `0x178e cmp byte [bx+0x659e],1` → `0x1795 mov word [bp-6],0x1e`, written as object byte `+5` by kernel `0x3A74` (verified: `[bp+6]` → `+5`). Untrapped (bit `0x80` clear). The byte is a constant: no RNG, no table, no location input.
- Open: `SJOG.OVL:0x112C open_chest_world` reads `+5` (`0x11cf mov al,[bx+0x5c5f]`) *before* blanking the slot (`0x11e1`), applies town karma (−2, floor 0), and if bit `0x80` is set prints `Trapped!` and runs kernel `0x2FD0`, masking the byte to `& 0x7f`. It then calls **`0x1040 loot_fixed(contents)`** and **`0x10B8 loot_random(contents)`**; if neither placed anything it prints `Chest empty!`.
- Kernel `rand(lo,hi)` is `ULTIMA.EXE:0x2092` (SJOG `call 0x6112` + base `0xBF80`): `state = ror3(state + 0x9248) ^ 0x9248 + 0x11`; result `lo + (state & 0x7fff) % (hi − lo + 1)`. Native `OriginalRng::next` is the same function.

**Stage 1 — `loot_fixed` (`0x1040`), rows `si = 7 … 0` (DS `0x4124` item / `0x412C` guard / `0x4134` max):**

```
if guard[si] > contents: skip, no roll                 (0x1083)
if guard[si] > rand(1,30): skip                          (0x1090/0x1099)
base = max[si] == 1 ? 1 : rand(1, max[si])               (0x109d/0x1050)
loot_place(item[si], base, contents)
```

**Stage 2 — `loot_random` (`0x10B8`), `contents/2 + 1` draws (DS `0x413C` item / `0x416C` guard, 48 rows):**

```
idx = rand(0,47)                                         (0x10db)
if guard[idx] > contents: skip, no roll                  (0x10e6)
if guard[idx] > rand(1,30): skip                         (0x10f2/0x10fb)
loot_place(item[idx], base = idx, contents)
```

**Quantity — `loot_place` (`0x0F88`):** id 1 (nested chest) → `rand(1, contents)` becomes the new chest's contents; id 2 (gold) → `rand(1, 3·contents)` (the `rand(1,90)` base is drawn and discarded); ids 3/4 (potion/scroll) → `base − 1` = the potion/scroll index; every other id → `base` (for equipment, the equipment index). Each piece takes a free actor slot (`call 0` = SJOG `find_free_actor_slot`, 31 → 1, shared with the location's NPCs and objects); a piece that finds no slot is rolled but not placed.

**Complete outcome table at contents `0x1E` (30):**

| Stage | RNG / range | Result (object id) | Quantity | P per Open |
|---|---|---|---|---|
| fixed row 7 | `rand(1,30) ≥ 7` | food (15) | `rand(1,2)` | 24/30 |
| fixed row 6 | `rand(1,30) ≥ 7` | torches (13) | `rand(1,2)` | 24/30 |
| fixed row 5 | `rand(1,30) ≥ 15` | gems (8) | `rand(1,2)` | 16/30 |
| fixed row 4 | `rand(1,30) ≥ 9` | keys (7) | `rand(1,2)` | 22/30 |
| fixed row 3 | `rand(1,30) ≥ 17` | scroll (4) | index `rand(1,8) − 1` | 14/30 |
| fixed row 2 | `rand(1,30) ≥ 17` | potion (3) | index `rand(1,8) − 1` | 14/30 |
| fixed row 1 | `rand(1,30) ≥ 3` | gold (2) | `rand(1,90)` (after a discarded `rand(1,90)`) | 28/30 |
| fixed row 0 | `rand(1,30) ≥ 25` | nested chest (1) | contents `rand(1,30)` (after a discarded `rand(1,10)`) | 6/30 |
| random ×16 | `rand(0,47)` = idx, then `rand(1,30) ≥ guard[idx]` | equipment id `item[idx]` ∈ {5 weapon, 6 shield, 9 helm, 10 ring, 11 armour, 12 amulet} | 1 piece of equipment index `idx` | per draw `1/48 × (31 − guard)/30` |

Equipment rows (index: guard) — helms 0–3: 10,10,15,20 · shields 4–8: 10,15,20,**28**,255 · armour 9–15: 15,15,20,20,20,24,255 · weapons 16–41: 5,10,10,10,10,10,10,10,15,15,15,10,15,10,20,20,20,20,20,255,23,23,23,255,255,255 · rings 42–44: 23,23,23 · amulets 45–47: 23,15,255. Names by index follow the project's equipment table (`longEquipNames.json`; `DATA.OVL` stores them through a shared-string pointer table, so the **index** is the binary fact).

**Equipment is possible — 41 of the 48 rows.** Never: 8 Jewel Shield, 15 Mystic Armour, 35 Sword of Chaos, 39 Glass Sword, 40 Jeweled Sword, 41 Mystic Sword, 47 Ankh (guard 255). Reachable high-value rows include 7 Magic Shield (guard 28), 36 Magic Bow, 37 Silver Sword, 38 Magic Axe, the three rings and the Amulet of Turning (guard 23). Expected equipment per Open, before the slot cap: **6.64** at contents 30.

**Generic, not location-specific.** The routines are the world-chest routines (combat chests use the same `0x112C` with the enemy's treasure rating as contents). What makes the vault what it is, is the constant `0x1E` given to every `.NPC` type-1 slot — and the three basement chests are the only type-1 `.NPC` slots in the game (`re/notes/npc-object-actors.md` census). Nothing in the chain reads the location except the town karma penalty and the trap-type band.

Independent oracle: `re/tools/chest_loot_oracle.py` transcribes `0x2092`/`0x1040`/`0x10B8`/`0x0F88` and reads the four tables out of `DATA.OVL` on every run. Every vector in the tests comes from it.

### Original reset contract (recovered from the binaries)

**Census.** `0x1694 town_populate_npcs` has exactly two callers: kernel thunk `0x7A8E` (only caller `CMDS.OVL:0x0677`, the bed hole-up loop — Batch 21B) **and `TOWN.OVL:0x051d`, a near call inside `0x0408`** (`0x0517 cmp word [bp+4],0 / je 0x520 / call 0x1694`). `0x0408` is the floor loader: it re-reads the floor's 0x400 bytes from the location's `.DAT` into the map buffer DS `0x6608` (`0x045c-0x046f`, file offset `(base + floor) << 10`), zeroes the open-door tracker (`0x041d mov byte [0x594f],0`), refreshes the hour tiles (`0x0508 call 0x170`) and only then, if its argument is non-zero, repopulates. It has four callers (exhaustive near-call scan of `TOWN.OVL`; it has no kernel thunk):

| Caller | Argument | What reaches it |
|---|---|---|
| `0x0574` in `0x052E stair_transition` | **1** | every stair step (`0x0835`, the movement handler) and every `(K)limb` of a ladder or grate (`0x0bd5`) |
| `0x1236` in `0x11F0 town_load_town_map` | its own `fresh` | map entry `TOWN 0x12d1` (1), Blackthorn exit `BLCKTHRN 0x0c5d` (1), moonstone teleport `ULTIMA.EXE 0x4876` (1), boot/Journey Onward `ULTIMA.EXE 0x00f7` (**0** when the save is inside a town — `npc-carga-partida-fresh-gate.md`) |
| `0x1044` | 1 | a scripted floor drop (fills the buffer with `0x8f`, wipes the object table, `dec [0x5895]`) — not part of this batch |
| `0x09dc` in `0x09bc` | 0 | after a town-NPC fight (`"Attacked!"` path) — map re-read, no repopulate; not part of this batch |

`0x1726` places an object **only when the schedule z equals the current floor** (`0x176c-0x1785`), so the register only ever holds the current floor's objects; `0x16a2-0x16b9` zeroes all 31 slots first, which also destroys uncollected floor loot.

**The door.** `0x97` is `MagicLockDoor`. A skull key (CAST `0x18f4`) writes `0xB8` into the live buffer; `(O)pen` then shows it open for four turns via the tracker `[0x594f]`. **Closed and magically locked are one stored state — the authored map byte `0x97`** — and it is restored by exactly one thing: a re-read of the floor by `0x0408`.

| Player action (original) | Chests reset | Door closes | Magic lock restored | Loot rerolls |
|---|---|---|---|---|
| Go upstairs (stairs or ladder), in the new floor | the new floor's objects are re-placed; the basement's are no longer in the register | — | — | — |
| **Return downstairs** | **yes** — `0x052E → 0x0408(1) → 0x1694`, contents `0x1E` | **yes** | **yes** — `0x0408` re-reads `0x97` | yes (every Open rolls from the fresh `0x1E`) |
| (H)ole up in the basement bed | **yes** — `CMDS 0x0677 → 0x1694` each 10-minute tick | no | **no** — `0x1694` never touches `0x6608`, and the hole-up has no path to `0x0408` | yes |
| Leave the castle and come back | **yes** — `town_load_town_map(1)` | yes | yes | yes |
| Ordinary turns / clock ticks | **no** — neither routine is reached | (only the 4-turn Open countdown) | no | — |
| Load a game saved in the basement | **no** — `fresh = 0`: no repopulate; the saved object register (DS `0x5C5A` is in the save window) comes back as it was | yes — `0x0408(0)` re-reads the floor and zeroes `[0x594f]` | yes | no |

**Correction to Batch 21B.** Batch 21B's answer table, row **B**, states the refill trigger is the bed hole-up only, from a census of kernel thunk `0x7A8E`. That census could not see `TOWN.OVL`'s own near call at `0x051d`: every stair step, ladder, map entry and moonstone arrival also re-seeds the interior objects. Batch 21B's row **D** and its H-158 row already described the floor-change reload correctly; row B is superseded here, not edited.

### Native behaviour and root causes — two independent defects

**H-159 — loot.** Native call chain on the device: `UiSession` → `AlphaRuntime::command` → `execute_command` → `world_interaction(Open)` → `open_chest` → `chest_loot(contents & 127)` → `QuestObject{loot}` appended per piece → `(G)et` → `apply_loot_grant`. `chest_loot`'s tables and control flow are **byte-identical to the original** (all five tables compared to `DATA.OVL`; call sequence proven by V1). The only divergence is its input: `hydrate_interior_objects` seeded every interior chest with `o.contents = 8`, inherited from the TypeScript reference's documented Class-C placeholder `INTERIOR_CHEST_CONTENTS = 8` (oracle hole O5). With contents 8, fixed rows with guards 25/17/17/15/9 are skipped without a roll, leaving only food, torches and gold, and `loot_random` makes 5 draws of which only index 16 (Dagger, guard 5) can succeed — about one Dagger per eleven chests. That is the hardware observation exactly. Not a table, RNG, item-id, category or inventory defect; `(G)et` of equipment already works (V6).

**H-158 — lifecycle.** `klimb_ladder` and `apply_stair_step` (`transitions.cpp`) emitted only `RefreshHourTiles`. The TypeScript reference had the same gap: both methods cite `0x052E → 0x0408` but implement only the hour-tile refresh. So a floor change never cleared the transient terrain layer holding the skull-keyed `0xB8`, never reset the open-door tracker, and never re-hydrated interior objects. The bed path was already right (Batch 21B), which is why sleeping refilled the chests while the stairs did not. The Batch 22 queue items (`ResetDoors`/`HydrateUnderworld` dropped on the Developer-Teleport and dungeon-exit routes) do **not** intersect: stair and ladder effects flow through `Runner::transitions()`, which consumes `ResetDoors`, `ClearTerrain` and `HydrateInterior` itself before notifying the device.

### Fixes (minimal)

| File | Change |
|---|---|
| `native/core/include/openu5/quest_world.h`, `src/quest_world.cpp` | `kInteriorChestContents = 0x1e`, cited to `TOWN 0x1795`; the hydrated chest uses it |
| `native/core/src/transitions.cpp`, `include/openu5/transitions.h` | `reload_floor()`: `ResetDoors`, `ClearTerrain` (and `volatile_terrain_wipe = false`), `HydrateInterior`, `RefreshHourTiles` — the same effects, in the same order, as `load_small_map`. `klimb_ladder` and `apply_stair_step` call it and now take the `TravelState&` that `load_small_map` already takes |
| `native/core/src/commands.cpp` | the two call sites pass `c.travel` |
| `game/src/core/game.ts` (reference) | `INTERIOR_CHEST_CONTENTS = 0x1e` (closes O5); `klimbLadder`/`applyStairStep` run the same `doors.reset` / `clearVolatileTerrain` / `hydrateInteriorObjects` sequence as its own `loadSmallMap`, before `refreshHourTiles` |
| `native/core/fixtures/commands.txt`, `fixtures/travel.txt` | regenerated from the corrected reference (see below) |
| `native/core/tools/generate-travel-fixtures.ts` | its minimal kind-0 mock gains no-op `clearVolatileTerrain`/`hydrateInteriorObjects` (the native side of that case passes no effect sink at all) |
| `native/core/tests/travel_parity_test.cpp` | the new `TravelState&` argument at three call sites |

Nothing is special-cased to location 17 and no timer was invented. The fix lives in the transition layer because that is where the reference draws the line: `command_parity` and `travel_parity` pin exactly which reload operations `Game.klimbLadder`/`applyStairStep` perform, so a native-only fix in `Runner` (tried first) moved `command_parity` row 4224 and was withdrawn in favour of correcting the reference at its own layer.

**Fixture regeneration, controlled.** Both generators were first run in `--check` mode against the *unmodified* reference: byte-identical (no drift). After the reference fix: `commands.txt` 112 rows changed, `travel.txt` 26 rows changed, nothing else. A token-level check proves every changed `commands.txt` row differs *only* by three records (`22 ResetDoors`, `23 ClearTerrain`, `24 HydrateInterior`) inserted immediately before the existing `RefreshHourTiles` record with its identical position and RNG seed, plus the grown length counters. Every changed `travel.txt` row is the same insertion (`2`,`3`,`4` before `5`) plus `volatile_terrain_wipe` 1 → 0 (the floor re-read clears the wipe, as `loadSmallMap` does). No RNG draw, event, position or state value moved in either corpus.

`quest_parity` (live against the reference) moved only in the expected place — sequence 4941, `world-flow` `{interior}` at location 17: `contents` 8 → 30 on the three chests — and is green again with the reference constant corrected. The reference's own unit suite has the identical failing set before and after (97 pre-existing failures: lints and undistributed material; `batch23-ts-vitest*.log`); the 13 files that exercise the changed code pass 228/228.

### Tests — `batch23_vault_parity` (45 checks, real `AlphaRuntime` + shipped resource pack, Batch 22 seam)

- **V1–V3 loot mechanism** (core `chest_loot` with a recording RNG): the exact 42-draw `(lo,hi)` sequence and final RNG state of oracle seed `0x0073`; pieces and final state of seeds `0x0073`/`0x00d0`/`0x0181`; every fixed row reachable with the original quantity rules and no roll for a guard over contents; all 48 equipment rows index by index (41 reachable, 7 never).
- **V4–V6 through the runtime**: hydrated chests carry `0x1E`; real `(O)pen` with seeded `game.rng` places exactly the oracle's pieces for all three chests (a Mace, Quarrels, Arrows, Flaming Oil, a potion, scrolls, keys, gems, a nested chest); real `(G)et` carries a Mace and scroll into the inventory. Against contents 8 the runtime produced exactly the oracle's contents-8 prediction for each seed, which also proves no stray draw precedes the loot roll.
- **L1/L2 lifecycle** by ladder and by stairs (all legs real `(K)limb` keys / `Move` commands): three chests back, contents `0x1E`, no duplicates, uncollected loot gone, and the refilled chest rolls anew.
- **D1/D2 door**: `(15,24)` authored and drawn `0x97`; skull key → `0xB8`; `(O)pen` → open; the first floor change clears the tracker; back downstairs it is `0x97`.
- **Negatives, each from the census:** L3 five `(P)asses` do not refill; L4/D4 leaving and re-entering refills and relocks (already correct); D3 a bed hole-up refills the chests but leaves the door `0xB8`.

Positions are placed by hand only to stand next to a chest (the vault is barrels around three floor cells), next to the door, or on a ladder; every state change is a real command.

**RED → GREEN.** Against pre-Batch-23 production (Batch 22 tree, `transitions`/`commands` at HEAD, contents 8): **28/45 GREEN, 17 RED** — V4b, V5a–c, V6b–d, L1d–f, L2d–f, D1d–e, D2d–e (`native/core/batch23-red.log`). After the fix: **45/45** (`batch23-green.log`).

**Mutation proof** (`native/core/batch23-mutation-m*.log`, each reverted):

| | Mutation | RED |
|---|---|---|
| M1 | `loot_random` loop disabled | 12 — V1a–d, V2b, V3a, V5a–c, V6b, L1f, L2f |
| M2 | contents back to 8 | 11 — V4b, V5a–c, V6b–d, L1d, L1f, L2d, L2f |
| M3 | floor change without `HydrateInterior` | 6 — L1d–f, L2d–f |
| M4 | floor change without `ClearTerrain` | 2 — D1e, D2e |
| M5 | floor change without `ResetDoors` | 3 — D1d, D1e, D2d |
| M6 | bed hole-up also clears terrain | 1 — D3b |

M6 first *survived* against an earlier D3 that opened the door before sleeping: the Open countdown overlay masked the tile underneath. D3 was tightened to unlock without opening and to require exactly `0xB8`; M6 then died.

### Regression sweep

Covered by the full suite and the new target: Batch 21B bed reset (38 checks), Batch 22 hydration, other chests (combat and dungeon chests use their own contents — `combat_loot_open_regression`, dungeon parity), inventory insertion (V6), doors (`command_parity`, `world_flow_*`), interior floor transitions (`travel_parity`, `movement_flow_*`), rest (`batch21b_chest_reset`), save/load (`persistence_*`), Developer Teleport (`debug_map_picker`, `batch22`). Nothing else moved.

### Queued, not fixed

- **H-161 — NPC half of `0x1694` on a floor change.** The original also snaps every NPC of the location to its schedule position (`0x1841-0x1856`) on every stair/ladder step. Neither port does; not reported on hardware. Same split as H-154.
- **H-162 — an Open door survives a load.** The original's load path `0x0408(0)` zeroes `[0x594f]` and re-reads the floor; native restores `openDoors` with its countdown (so does the reference). The **magic lock** already matches — `WorldTerrain::transient` is not saved.
- **H-163 — `0x09bc` post-fight reload.** After a town-NPC fight the original re-reads the floor without repopulating (doors relock, chests do not refill). Native behaviour not examined.
- The Batch 22 `U5OBJ` trace is still compiled in and still owed its removal.

### Full regression suite

From-scratch build (`native/core/build-batch23-final`), serial: **93/93, 0 fail** (`batch23-final-ctest.log`) — the prior 92 plus `batch23_vault_parity`. `command_parity` and `travel_parity` run against the regenerated fixtures; `gameplay_parity` and `quest_parity` run live against the corrected reference. One warning, the pre-existing w64devkit `stl_uninitialized.h` `-Wstringop-overflow=` false positive; zero project warnings.

### Firmware

ESP-IDF 6.1, `native/targets/tdeck/build-batch23`: `openu5_tdeck.bin` = **0xd38d0** (866,512 bytes), +0x40 over Batch 22; `0x2c730` (17 %) of the app partition free. **0 errors, 0 compiler warnings** (`batch23-firmware-build.log`). Launcher image (`package_launcher.py`): `build-batch23/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, SHA-256 `e4efc721ff717e70e28e79734d918233ad2ade80ecb418f4ad1da1b76fb923bf`. **Not flashed.** SD card unchanged.

**Correction (after the Batch 22/23 split) — the image to flash is a different file.** The image above was built before Batch 23 was committed, so it identifies itself as `FW b6705142ee31`: `main/CMakeLists.txt` stamps `git rev-parse --short=12 HEAD` at configure time. The final artifact was reconfigured and rebuilt at the Batch 23 commit: `build-batch23-split/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, SHA-256 `86bcc09ee2553f1d1a50d44a83d21647dd3cb0d2101a9c9be7a72b26e177a781`, `FW 93ef488b57bb`, same size `0xd38d0`, 0 errors, 0 warnings. The two images differ only in build timestamps, that embedded commit id and the image/descriptor hashes; the code is identical. **Flash the `build-batch23-split` image.**

### Status

H-158 and H-159: **SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** H-148 (Batch 21B bed reset) is unchanged and still owed its hardware pass; Phase 6R below covers it too.

### Phase 6R — Batch 23 vault loot and floor-change reset · *firmware only; the SD card is unchanged*

Flash the Batch 23 firmware, start a **New Journey**, and give the party a few skull keys. No serial capture is needed.

1. Enter Lord British's Castle normally and go down to the basement (the ladder at (1,1) or the stairs at (12,7)).
2. `(U)se` a skull key on the vault door at **(15,24)**, `(O)pen` it, and open the three chests.
3. **Expected:** the `Found:` lists are no longer only gold, torches and food. Over the three chests expect weapons, armour, shields, helms and often rings/amulets, plus potions, scrolls, keys, gems and sometimes a nested chest. (Each chest averages over six pieces of equipment before the slot limit.)
4. `(G)et` a few pieces, leave the rest on the floor, then go **up** one floor and straight back **down**.
5. **Expected:** all three chests are back and unopened; the loot left on the floor is gone; the door is **magically locked again** — another skull key is needed.
6. Unlock the door again (do not open it), loot the chests, `(H)`ole up 1 hour in the bed at (16,19).
7. **Expected:** the chests are back; the door is **still unlocked** (no second key).

**Pass:** steps 3, 5 and 7. **Fail** if step 3 still shows only gold/torches/food across several chests, if the chests or the lock do not come back in step 5, or if the bed relocks the door in step 7.

**Known and queued, do not file:** NPCs do not jump to their schedule positions when you change floors (H-161) or sleep (H-154).

## Batch 24 — state/reload parity: floor change, save/load, end of a town fight (H-161, H-162, H-163)

Scope: the three queued items that concern **when the 1988 game rebuilds or refreshes local world state**. Nothing else was fixed; newly noticed gaps are queued at the end.

### Baseline

HEAD `5adae63f` (tag `alpha2-batch23-vault-loot-floor-reset` at `93ef488b`), branch `main`, working tree clean. From-scratch build `native/core/build-batch24-baseline`, serial: **93/93 PASS, 0 fail, 0 skipped** (`batch24-baseline-ctest.log`); one warning, the known w64devkit `stl_uninitialized.h` `-Wstringop-overflow=` false positive. Controls on the unmodified reference: `generate-command-fixtures.ts --check` and `generate-travel-fixtures.ts --check` byte-identical (`batch24-fixture-check-control.log`); TypeScript unit suite 97 pre-existing failures (`batch24-ts-vitest-baseline.log(.fails)`).

### Original behaviour (recovered from the binaries)

Every citation was re-read with `re/tools/dis16.py`; call-site claims use `re/tools/callers_banda.py` (positive control: it finds `TOWN.OVL:0x09d0 → kernel 0x6150`, read independently) plus an in-module near-call scan of `TOWN.OVL`.

**One routine: `TOWN.OVL:0x0408`, the floor loader.** It zeroes the open-door tracker `[0x594f]` (`0x041d`), re-reads the floor's 0x400 bytes from the location's `.DAT` into the map buffer DS `0x6608` (`0x045c-0x046f`), rebuilds the tile caches, refreshes the hour tiles (`0x0508 call 0x170`) and **only if its argument is non-zero** calls `0x1694 town_populate_npcs` (`0x0517 cmp [bp+4],0` / `0x051d`). The map buffer lies outside the save window `0x55A6..0x6605`, so nothing written into it — an Open door, a skull-keyed `0x97 → 0xB8` — survives any re-read. Its four callers: `0x0574` (in `0x052E`, argument 1), `0x1236` (in `0x11F0`, its own `fresh`), `0x1044` (scripted floor drop, 1) and `0x09dc` (in `0x09BC`, **0**).

**`0x1694`, the NPC half** (the object half was Batch 23). `0x16c9-0x171b` walks slots 1..31 whose type byte (DS `0x659E`) is non-zero — `npc_clear_slot 0x00B0` zeroes that byte, so a slot cleared during the visit stays gone — and for each takes the period `schedule_index` (`NPC.OVL:0x12E0`, via kernel thunk `0x7B36`, `0x16d7`) picks for `g_hour` (`0x16d1`). `0x1726` places the slot's object only when the period's Z equals the current floor, but **`0x1841-0x1856` write X/Y/Z into the live record unconditionally**, then state = 1 (`0x1856`, again `0x16fc`), servedSlot = period (`0x1705`), pathIdx = −1 (`0x170c`). The stuck counter (DS `0x65C2`), the dialog byte and the schedule's AI bytes are never written. It reads the **live** schedule table DS `0x5D5E`, not the `.NPC` file: the guard alarm `0x958 → 0x85e` zeroes a guard's four times in that table (`0x0890-0x0896`, DS `0x5D6A`) and sets its AI to 7 (`0x08c2`), and a later `0x1694` honours that.

**`0x12E0 schedule_index`.** `d_i = (hour − t_i) & 0xff`; period 0; `d0 > d1` → 1; `> d2` → 2; `> d3` → 1 (without updating the minimum). Both ports implement exactly this (`time.ts scheduleIndex`, `time.cpp schedule_index`); `re/tools/npc_schedule_oracle.py` (new) transcribes it and reads the `.NPC` records itself.

**Load.** Journey Onward is `ULTIMA.EXE 0x00f7 → 0x11F0(fresh=0)` inside a town (`re/notes/npc-carga-partida-fresh-gate.md` §1): no `.NPC` read, no activation, and `0x0408(0)` — the door tracker is zeroed and the floor re-read, but the NPC band, the object register (DS `0x5C5A`), the schedules and the dead bits come back from the save window verbatim.

**End of a town fight.** Both town fights — the player's (A)ttack (`0x09e6 → 0x0b3a`) and a hostile NPC's `"Attacked!"` (`0x13fb → 0x1408`) — go through `0x09BC town_attack_engine_commit`, 17 instructions without a branch: dead bit (`0x52`), `enter_combat_vs_actor` (kernel `0x6150`, which returns only when the fight is over, won or fled), `npc_clear_slot` (`0xb0`), **`0x0408(0)`** (`0x09d9-0x09dc`), `0x2ae`. `0x6150`'s only callers are `TOWN 0x09d0` and `ENDGAME 0x0051`. The kernel combat mainline `0x5F86` copies the object register `0x5C5A → 0xA9FC` before the fight (`0x5fbe-0x5fd8`) and back after, and `COMBAT.OVL:0x0bcf mov byte [0x594f],0` zeroes the door tracker as every fight starts.

| Trigger | Floor re-read (`0x0408`) | Door tracker / transient terrain | NPC schedule reposition (`0x1694`) | Objects (chests) | Persistent state |
|---|---|---|---|---|---|
| stairs / ladder (`0x052E`) | yes, argument **1** | reset / reset | **yes**, every NPC of the location, every floor | wiped and re-placed (Batch 23) | untouched |
| load (`0x00f7 → 0x11F0(0)`) | yes, argument **0** | reset / reset (map buffer is not saved) | **no** — the saved NPC band loads verbatim | as saved | as saved |
| end of a town fight (`0x09BC`) | yes, argument **0** | reset (already at `0x0bcf`) / reset | **no** | as before the fight (`0x5F86`) | the attacked slot is cleared; persons get a dead bit |

**The three items are one mechanism** — `0x0408` — and differ only in its argument. The fixes stay separate because the three call sites are separate in both ports; nothing was abstracted beyond one new core primitive.

### Native behaviour before the fix

- **H-161 — CONFIRMED DIVERGENCE.** `klimb_ladder`/`apply_stair_step` → `reload_floor()` emitted `ResetDoors`, `ClearTerrain`, `HydrateInterior`, `RefreshHourTiles` and nothing for NPCs; a floor change left every NPC where its walk had taken it, mid-walk machine included. The TypeScript reference had the same gap (`klimbLadder`/`applyStairStep`; the only 0x1694 analogue in the port was `NpcManager.enterMap`, a full `.NPC` rebuild).
- **H-162 — CONFIRMED DIVERGENCE.** `capture_gameplay` saves `openDoors` with its countdown (correct: the 1988 save window holds `[0x594f]`), and `restore_gameplay` puts it back (correct as a deserializer — `gameplay_driver` round-trips every parity step through it). What was missing is the load *operation*: neither `AlphaRuntime::synchronize_loaded_world()` (System Menu / frontend loads) nor the `Alt+L` arm zeroed the tracker, so the door was still drawn open with 3 turns left after a load. The TypeScript reference did the same (`main.ts`, both load paths, `doors.restore`). The skull-key lock already matched — `WorldTerrain::transient` is not saved.
- **H-163 — CONFIRMED DIVERGENCE (transient terrain only).** `finish_encounter_combat` / `AlphaRuntime::finish_combat_if_needed` recomputed the hour tiles but never cleared transient terrain, so a skull-keyed `0xB8` stayed unlocked after a town fight where 1988 relocks it. **The open door already matched** (both ports zero the tracker at combat entry, `COMBAT 0x0bcf`), and so did "no NPC reposition" and "no chest refill". The TypeScript reference had the same gap (`endCombat`).

### Fixes

| File | Change |
|---|---|
| `native/core/include/openu5/actors.h`, `src/actors.cpp` | `snap_npcs_to_schedule(NpcActors&, location, hour)` — the NPC half of `0x1694`: position from the period, state 1, served period, path −1; stuck untouched; a reposition, not a rebuild |
| `native/core/include/openu5/transitions.h` | `ReloadEffect::SnapNpcs`, **appended** (fixtures record the ordinals) |
| `native/core/src/transitions.cpp` | `reload_floor()` emits `SnapNpcs` right after `HydrateInterior` (the two halves of `0x1694`) |
| `native/core/src/commands.cpp` | `Runner::transitions()` consumes `SnapNpcs` on `c.actors` — the table the device shares (`context_.actors = &actors_`), so no device arm is needed |
| `native/targets/tdeck/main/alpha_runtime.cpp` | `synchronize_loaded_world()` and the `Alt+L` arm zero `commands_.door.turns` after a successful load |
| `native/core/include/openu5/combat.h`, `src/quest_world.cpp`, `src/combat.cpp` | `CombatState::town_fight`, set by `town_attack_commit` (the native `0x09BC`) once its combat starts; `finish_encounter_combat` then does `0x0408(0)`: door tracker 0, terrain wipe off, `clear_residence()` + hour-tile `refresh()` — no objects, no NPCs |
| `game/src/core/npc/manager.ts` (reference) | `NpcManager.snapToSchedule` — same contract |
| `game/src/core/game.ts` (reference) | `klimbLadder`/`applyStairStep` call it after `hydrateInteriorObjects`; `townAttackCommit` sets `townFightReload` (cleared at every `startCombat`); `endCombat` performs the `0x0408(0)` re-read for it |
| `game/src/main.ts` (reference) | both load paths `doors.reset()` instead of `doors.restore(state.openDoors)` |
| `native/core/tools/generate-{command,travel}-fixtures.ts` | the mocks observe the new operation (command: `fx(31)` wrapping the real `snapToSchedule`; travel: `snapToSchedule: () => fx(11)`) |
| `native/core/fixtures/commands.txt`, `travel.txt` | regenerated from the corrected reference |

Not done, deliberately: `0x09BC`'s trailing `0x2ae` (the Shadowlord re-seed double-run, `re/notes/rng-186-acta.md` T6, queued as #197) and the bed hook's NPC half (H-154, device wiring) — `snap_npcs_to_schedule` is now the faithful primitive H-154 can use.

**Fixture regeneration, controlled** (`batch24-fixture-diff.log`, token level). `travel.txt`: 26 rows changed, every one differing **only** by `[10 11 loc floor x y]` records (1 or 2 per row), each inserted between `10 4` (HydrateInterior) and `10 5` (RefreshHourTiles) at the same position, output length +6 per record; input, result, position and drunk/wipe/Shadowlord fields identical. `commands.txt`: 992 of 13,824 rows changed, in 112 of 1,152 sequences; in **every** changed sequence the first changed row is a floor change, and all 112 floor-change rows carry exactly one `31` record right after `24` at the same position with the rest of the effect list unchanged. Rows after it differ in NPC state (the repositioned actors) and, through the NPC wander draws on the shared kernel RNG, in RNG seeds/draws, the regeneration roll (`c2.hp`, 27 rows) and wind (11 rows); no party-field change occurs without a preceding RNG-stream difference, and no event or status field changed. `gameplay_parity` and `quest_parity` (live against the corrected reference) stayed green without edits. TypeScript unit suite: identical failing set before and after (97; `batch24-ts-vitest*.log.fails`).

### Tests — `batch24_reload_parity` (47 checks, real `AlphaRuntime` + shipped pack)

Same seam as Batches 22/23, extended: `HostTestFixture::pack` copies the INIT.GAM/.OOL templates and the town-combat maps/enemies/tables into the members `initialize()` assigns and runs the New Journey's `load_native_state`; `host_stubs/alpha_save_memory_host_stub.cpp` replaces only the SD card — it runs `alpha_save.cpp`'s own chain (`capture_*` → `export_native_state` over INIT.GAM → `encode_json`; `load_native_state` → `restore_gameplay`/`restore_terrain`/`restore_npc_walk` into scratch copies, then commit). Every state change is a real command or key: `(E)nter`, `(K)limb`, `Move`, `(O)pen`, `(U)se` skull key, `(A)ttack`, `Alt+S`/`Alt+L`, `Alt+M` → Save / Continue Latest, and arena walk-offs through the runtime's wall-clock service loop. Positions are placed by hand only to stand next to a target, and NPCs are displaced by hand to model "where its walk took it". Expected cells come from `npc_schedule_oracle.py 17 12`.

- **N1/N2 (H-161)** — ladder down and stairs up in Lord British's Castle at noon: the landing floor's NPC, the one left behind and one on a third floor are each at the oracle cell with state 1, the period and path −1; every NPC off the landing floor stands where `0x12E0` puts it; no NPC added or removed; the stuck counter untouched.
- **N3** — two `(P)asses` do not reposition.
- **S1/S2 (H-162)** — `Alt+S`/`Alt+L`, and System Menu Save / Continue Latest: gold changed after the save comes back (the load really happened); the Open door (20,16) is closed with tracker 0; the skull-keyed lock is `0x97`; **persistent state survives** — the opened vault chest stays opened with its loot, the other two stay, a displaced NPC is still displaced (no `0x1694` on load), skull keys and karma as saved.
- **F1 (H-163)** — basement fight with slot 21 and a walk-off: the vault lock is `0x97` again; the door is closed (already by `0x0bcf`); no refill, no wipe; no NPC reposition.
- **F2** — a guard fight on floor 0: the alarm rewrites slot 1's live schedule (times 0, AI 7); the next ladder step repositions slot 1 **from the live schedule**, to (17,7,0), not the file's (17,28,0); the fought guard (family `0x70`, no dead bit) is **not** resurrected.

**RED → GREEN.** Against pre-Batch-24 production (the nine production files stashed, test seams kept): **35/47 GREEN, 12 RED** — N1c–f, N2c–f, S1e, S2e, F1d, F2d (`native/core/batch24-red.log`). After the fix: **47/47** (`batch24-green.log`). All green-before rows are characterizations or anti-fake guards; none carries a fix alone.

An earlier draft of F2 expected (17,28,0) and failed against the fixed code: the binary (`0x85e`) showed the alarm rewrites the live schedule, so the expectation was wrong, not the snap. It was corrected and now also proves the snap reads the live table.

**Mutation proof** (`native/core/batch24-mutation-m*.log`, each reverted; production restored byte-exact):

| | Mutation | RED |
|---|---|---|
| M1 | `reload_floor` without `SnapNpcs` | 9 — N1c–f, N2c–f, F2d; `command_parity` (row 4224) and `travel_parity` (case 19322) also fail |
| M2 | `SnapNpcs` as a blind `.NPC` rebuild (`enter_npc_map`) | 4 — N1h, N2h, F2d, F2e. `command_parity`/`travel_parity` **pass** under M2: only the runtime test catches a rebuild |
| M3 | `synchronize_loaded_world` without the door reset | 1 — S2e |
| M4 | `Alt+L` arm without the door reset | 1 — S1e |
| M5 | no post-fight re-read | 1 — F1d |
| M6 | over-reach: the post-fight re-read also repositions NPCs | 1 — F1g |
| M7 | over-reach: a load repositions NPCs | 1 — S2h |

### Full regression suite

From-scratch build (`native/core/build-batch24-final`), serial: **94/94, 0 fail, 0 skipped** (`batch24-final-ctest.log`) — the prior 93 plus `batch24_reload_parity`. One warning, the pre-existing w64devkit false positive; zero project warnings.

### Firmware

ESP-IDF 6.1, `native/targets/tdeck/build-batch24`: `openu5_tdeck.bin` = **0xd39a0** (866,720 bytes), +0xd0 over Batch 23; `0x2c660` (17 %) of the app partition free. **0 errors, 0 compiler warnings** (`batch24-firmware-build.log`). The image embeds the commit id at configure time, so the Launcher image is rebuilt (`idf.py reconfigure build`, `package_launcher.py`) **after** the Batch 24 commit; its path and SHA-256 are recorded in the annotated tag `alpha2-batch24-state-reload-parity`. **Not flashed.** SD card unchanged.

### Status

H-161: **SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** H-162: **SOFTWARE FIXED — HARDWARE RETEST REQUIRED.** H-163: **CONFIRMED DIVERGENCE — CORRECTED** for transient terrain (skull-keyed lock); **verified native match** for the open door, NPC positions and chests. Hardware retest: Phase 6S.

### Queued, not fixed

- **H-164 — `Alt+L` quick load bypasses `synchronize_loaded_world()`.** The `DeviceShortcut::Load` arm (`alpha_runtime.cpp`) re-enters the NPC map and refreshes terrain itself but does not clear and restore the world-object pool (`objects_.clear()` / `restore_world_objects`), restore the dungeon session, or cancel live scenes/fx — all of which the System Menu and frontend loads do. By code reading only; not reproduced (S1 saves and loads with an unchanged pool, so it cannot show the leak). Batch 24 added the door reset to both arms rather than rerouting `Alt+L`.
- H-154 (bed NPC half on the device) and #197 (`0x09BC → 0x2ae` Shadowlord re-seed) remain queued; see above.
- Documentation drift noticed, not edited: `ALPHA2_HARDWARE_CHECKLIST.md` rows H-158/H-159 and `ALPHA2_PRESERVATION_LEDGER.md` D-21/D-22 still read "queued", although Batch 23 fixed them in software.

### Phase 6S — Batch 24 floor change, load and town fight · *firmware only; the SD card is unchanged*

Flash the Batch 24 firmware (the image named in the tag). A New Journey with a few skull keys is enough; no serial capture is needed.

1. Enter Lord British's Castle. Note where two or three NPCs on the ground floor stand; wait (Pass) until some of them have walked somewhere else.
2. Go down the ladder at (1,1) and straight back up.
3. **Expected:** the NPCs you watched are back at their posts for the current hour (not where they had walked to). Guards at their stations, not mid-walk.
4. In the basement, `(O)pen` an ordinary door (e.g. the one at (20,16)) and immediately **Save** (`Alt+S` or System Menu), then **Load** (`Alt+L`, and once more via System Menu → Continue Latest).
5. **Expected:** after each load the door is **closed**. Everything else is as saved: an opened chest stays opened, gold/keys unchanged, NPCs where they were when you saved.
6. Unlock the vault door (15,24) with a skull key (do not open it), then attack an NPC in the basement (or let one attack you) and walk out of the arena.
7. **Expected:** after the fight the vault door is **magically locked again**; chests you had opened stay opened; NPCs are not moved.

**Pass:** steps 3, 5 and 7. **Fail** if NPCs stay mid-walk after a floor change, if an open door survives a load, or if the vault stays unlocked after a town fight.

**Known and queued, do not file:** NPCs do not jump to their schedule positions when you sleep (H-154); `Alt+L` may leak world objects from before the load (H-164).

## Batch 25 — H-118: the "shard ritual → permanent world Move lock"

Scope: H-118 only. H-115 (dungeon save/load) was not touched and remains the next planned batch. One neighbouring gap is queued at the end.

### The hardware observation (Batch 20, CRITICAL)

Developer → Certification → "Flame/Shard Test" (grants the three shards, teleports to Empath Abbey floor 1 (15,3)), then `(U)se` a shard. Only the generic `Use item` echo appeared — **no ritual text at all**. From then on a direction produced nothing (no `Blocked!`, no echo), across a Developer Teleport and a Load; `(L)ook` "resolved directions" and `(Z)`-stats, `Alt+M` and `Alt+D` still answered; only a power cycle recovered. Classified CRITICAL because it permanently removes the player's ability to move without a reboot and blocks preservation sign-off.

### Baseline

HEAD `caa6488a` (tag `alpha2-batch24-state-reload-parity`), branch `main`, working tree clean. From-scratch build `native/core/build-batch25-baseline`, serial: **94/94 PASS, 0 fail, 0 skipped** (`batch25-baseline-ctest.log`).

### Reference behaviour (recovered from the binaries)

Re-read with `re/tools/dis16.py`; strings decoded at `DATA.OVL` `DS+0x10`.

- **The ritual, `CAST.OVL` dispatcher `0x1a2c` → `0x15b4`** (`re/notes/shadowlord-ritual.md`, cross-checked): `idx = itemId − 0x1d`; step 1 prints `"Gem Shard\n\nThou dost hold above thee the evil Shard of "` + name **before any gate**; off the cell (`0x162f-0x1654`, tables DS `0x4882/0x4886/0x488a/0x488e`) → `"\n\nNo effect!\n"` and `ret`; on it → `"...and cast it into the Flame of "` + flame; then only if the tile north is `0xFC` and `[0x58cb] == idx` the doom (`0x1708`: Shadowlord gone, shard consumed, doom bit). **No `getdir`, no modal, no retained state**: control returns to the command loop on `ret`, so an immediate Move is ordinary. Native `cast_shard_into_flame` / `use_quest_item` already implement this exactly (Batch 25 section S below, GREEN on unmodified code).
- **The town-exit question, `TOWN.OVL:0x0798`**: pushes DS `0x2690` = `"\nDost thou wish to leave? "`, then `0x07ac-0x07b4` loops `call 0xa49c` (getkey) until the key is `Y` (0x59), `N` (0x4e) or ESC (0x1b). A closed loop: the 1988 game **cannot** return to its command loop with that question unanswered.

REFERENCE FACT: a shard Use always prints its header; nothing in it can disable movement. REFERENCE FACT: a pending "leave?" question always owns the keyboard until answered.

### Root cause

NATIVE OBSERVATION (host, real `AlphaRuntime`, real Developer menu, raw keys — `batch25-red.log`): the same route from a clean world does **not** lock (S1–S14 GREEN before the fix). The signature appears only when a core-owned question is pending as `Alt+D` is pressed:

1. The port splits `TOWN 0x07ac`'s synchronous loop into core state `CommandState::awaiting_exit` plus a UiSession yes/no modal; the modal is the **only** thing that can send `Exit`/`DeclineExit`.
2. `Alt+D` → `UiSession::open_debug_menu()` parks the modal in `debug_return_mode_`.
3. `AlphaRuntime::synchronize_after_debug()` runs after **every** input (the `Alt+D` keystroke included) and calls `ui_->set_base_mode(resolve_synchronized_base_mode(...))`. `UiSession::set_base_mode()`'s ordinary branch never replaces a modal (`else if (!is_modal(mode_)) mode_ = m;`), but its Developer branch did: `if (mode_ == UiMode::DebugMenu) debug_return_mode_ = m;` — unconditionally.
4. Closing the menu restored Exploration. The question vanished; `awaiting_exit` stayed `true`.
5. From then on `commands.cpp`'s gate `exit != c.commands.awaiting_exit` returned `InvalidContext` — silently — for **every** world command.

Why exactly this signature: the shard `UseItem` is refused before `use_quest_item` runs (hence no header, the one thing CAST `0x15b4` always prints); Move is refused before `Runner::move` emits its `WalkEcho`; `(L)ook`'s `"Look-"` and direction echoes are UiSession text, but its core half ("Thou dost see …") is refused too — the hardware note records Look "resolving directions", which is exactly what survives; `(Z)`-stats, `Alt+M`, `Alt+D`, the Developer Teleport and `Alt+L` are all device-level and never pass through `dispatch_world_command`. Neither a teleport nor a load touches `CommandState`, so the lock survived both; a power cycle rebuilt it. The same drop affects every core-owned question the overlay can interrupt: `awaiting_troll` ("Pay toll?"), Blackthorn's tribute/arrest/password flags.

Timing: the bad state exists **before** the ritual code executes (class A). The ritual is not involved; it was simply the first core command after the Certification, which is only reachable through `Alt+D`.

INFERENCE: which prompt was on the T-Deck screen when the tester pressed `Alt+D` is not recoverable from the Batch 20 notes. The deduction that it was *a* pending core question rests on three facts: (a) a silent shard Use is impossible unless `dispatch_world_command` refused it, since `pool()` is wired identically in `initialize()` and the fixture; (b) a refusal of Use and Move that survives teleport and load while the UI stays in Exploration can only be one of the `CommandState`/Blackthorn awaiting flags; (c) the only path in the tree that returns the UI to Exploration with such a flag still set is this register clobber, and the tester provably opened the Developer menu immediately before the Use.

### Fix (minimal)

| File | Change |
|---|---|
| `native/core/src/ui_session.cpp` | `set_base_mode()`: the Developer branch now writes `debug_return_mode_` only when it does not hold a modal — the same rule the ordinary branch already applies to `mode_`. One condition; comment cites H-118 and `TOWN 0x07ac` |

Why this layer: `UiSession` owns both the modal and the return register, and `open_debug_menu()` already intends "return to what I interrupted". `synchronize_after_debug()` is correct to publish authoritative Combat/Dungeon changes on every input and is left alone (U9 proves a Dungeon change made under the menu still wins). Not done, deliberately: no reset of `awaiting_exit` on menu close (mutation M5 below shows why that is a symptom fix), no change to Move, no global modal clear.

Behavioural consequence, by design: after `Alt+D` → Back the interrupted prompt is back on screen. If a Developer Teleport moved the party meanwhile, the question is answered where the party now stands — `Y` leaves that town (Y5), `N` stays (R5) — exactly like any other answer to the kernel's loop.

### Tests — `batch25_shard_ritual` (42 checks, real `AlphaRuntime` + shipped pack + real Developer menu)

Seam: Batch 24's, plus the Developer menu `initialize()` constructs (`alpha_runtime_host_fixture.cpp`: `new UiDebugMenu(context_)`, `attach_diagnostics`, `ui_->attach_debug_menu`; no existing host test sends `Alt+D`). Every step is a raw keyboard/trackball event through `AlphaRuntime::handle()`; Cancel is a real short Mic press/release; the only staging line stands the party on Lord British's Castle's west edge.

- **R1–R11** (the hardware route, answer N): castle edge → "Leave this place?" → `Alt+D` → Certification → Flame/Shard → Back, Back → **the question is back** (R3) → `N` clears `awaiting_exit` (R4) → `(U)se` Shard of Hatred prints the `0x15b4` header and "…Flame of Love!" (R6), no doom, shard kept (R7) → **immediate Move (15,3)→(15,4)** (R8) → Look reaches the core (R9) → Z-stats round trip (R10) → Move back (R11).
- **Y1–Y8** (answer Y): the party leaves the town it now stands in; overworld Move, Look, Z-stats answer.
- **S1–S14** (shard exit paths from a clean world): Mic inside the (U)se list (S2–S3); wrong shard for this flame → header + "No effect!" (S4), no direction prompt (S5 — reference has none), Move (S6); wrong cell → "No effect!" (S7–S8); full success — `(Y)ell ASTAROTH` one cell south (S9), step on, Shard of Hatred → "The doom of the Shadowlord Astaroth is wrought!", shard consumed, summon cleared (S10); immediate Move (S11), Look (S12), Z-stats (S13), Move (S14).
- **U1–U9** (the overlay against every core-owned question, raised through the production event sink): "Leave this place?", "Pay toll?", "Pay tribute?", "Wilt thou come quietly?", "Password?" each come back after `Alt+D` → Back (U1–U5); controls: Exploration → Exploration (U6); an armed `Look-` comes back and completes into a core Look (U7–U8); an authoritative Dungeon change made under the menu still wins (U9).

**RED → GREEN.** Unmodified production: **24/42 GREEN, 18 RED** — R3 R4 R6 R8 R9 R11, Y3–Y7, U1–U5, U7, U8 (`native/core/batch25-red.log`). The RED R-series is the hardware signature verbatim: R6's transcript holds only `Use item`; R8/R11 Move does nothing; R9 has only the `Look-`/direction echoes; **R10 Z-stats is GREEN** — selective, not a dead device. S1–S14 are GREEN before the fix: the ritual itself was never defective. After the fix: **42/42** (`batch25-green.log`).

**Mutation proof** (`native/core/batch25-mutations.log`; each applied, built, run and reverted; production restored byte-exact):

| | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | guard removed (the pre-fix write) | 24/42 | R3 R4 R6 R8 R9 R11 Y3–Y7 U1–U5 U7 U8 |
| M2 | guard reads the live `mode_` (always DebugMenu) instead of the parked mode | 24/42 | same 18 |
| M3 | guard protects yes/no questions only | 39/42 | U5 (password text entry), U7, U8 |
| M4 | guard protects every modal except TargetSelection | 40/42 | U7, U8 |
| M5 | no guard; clear `awaiting_exit` whenever the Developer menu closes (symptom reset) | 32/42 | R3 Y3 Y5 U1–U5 U7 U8 — Move works again under M5, but the question still vanishes and the troll/guard questions are still dropped |

### Full regression suite

From-scratch build (`native/core/build-batch25-final`), serial: **95/95, 0 fail, 0 skipped** (`batch25-final-ctest.log`) — the prior 94 plus `batch25_shard_ritual`. One warning, the pre-existing w64devkit `stl_uninitialized.h` false positive; zero project warnings. No fixture moved.

### Firmware

ESP-IDF 6.1, `native/targets/tdeck/build-batch25`: `openu5_tdeck.bin` = **0xd39a0** (866,720 bytes) — unchanged from Batch 24 (one added compare fits the existing alignment); `0x2c660` (17 %) of the app partition free; **0 errors, 0 compiler warnings** (`batch25-firmware-build.log`). The Launcher image is rebuilt after the commit (`idf.py reconfigure build`, `package_launcher.py`); its path and SHA-256 are in the annotated tag `alpha2-batch25-h118-shard-move-lock`. **Not flashed.** SD card unchanged.

### Status

H-118: **HOST FIXED / DEVICE RETEST PENDING** (Phase 6T). Root cause: native Developer-overlay defect (not a 1988 behaviour, not the ritual). Not HARDWARE VERIFIED until the user runs Phase 6T.

### Queued, not fixed

- **H-165 — a pending "Leave this place?" plus a Developer teleport *into a dungeon*.** After the fix the question comes back, but by code reading its answer is refused there: `Exit`/`DeclineExit` hit `commands.cpp`'s `c.dungeon && !dungeon_camp` context gate (`InvalidContext`), so `awaiting_exit` would survive until the party surfaces and then lock world commands. Requires a pending question **and** a dev-tool dungeon teleport; not reproduced (the host fixture carries no dungeon data, so the Dungeon Certification does not activate a dungeon there). Workaround for testers: answer any on-screen question before opening the Developer menu.

### Phase 6T — Batch 25 H-118 retest · *firmware only; the SD card is unchanged*

Flash the Batch 25 firmware (the image named in the tag). Any save in a town is fine. No serial capture is needed.

1. **Baseline:** in any town, walk a few steps with the trackball. Each step echoes a direction.
2. Walk into a town's outer edge until **"Leave this place?"** appears. **Do not answer it.**
3. Press `Alt+D` → Certification → **Flame / Shard Test** → Confirm → Run Certification, then Mic/Back until the Developer menu is closed.
4. **Expected:** "Leave this place?" is **back on screen**. Answer **N**.
5. `U`se → **Shard of Hatred**. **Expected:** "Gem Shard … Thou dost hold above thee the evil Shard of Hatred…" then "…and cast it into the Flame of Love!" (no doom line — no Shadowlord is present). A short sweep sound may play.
6. Immediately move **South** with the trackball. **Expected:** the party steps to (15,4) with the "South" echo.
7. `L`ook North. **Expected:** a description line ("Thou dost see …"), not just "Look- north".
8. `Z`-stats, open and close. Then move **North**. **Expected:** the party steps back onto the ritual cell.
9. `U`se → **Shard of Falsehood** here. **Expected:** the header, then "No effect!". Then `U`se and cancel the list with a short Mic press. Move once. **Expected:** the party moves both times.

**Pass:** steps 4–9 with **no power cycle**. **Fail** if the question does not return in step 4, if any Use prints only "Use item", or if a direction stops producing a step/echo.

**Known and queued, do not file:** H-165 (a question left pending across a Developer teleport *into a dungeon*); H-115 dungeon save/load (next batch).

## Batch 26 — H-115: save and load inside a dungeon

Scope: H-115 only, including the loose-world-object half of the save it depends on. Not touched: H-154–H-160, H-164 (`Alt+L`), H-165, presentation/audio. New findings are queued at the end.

### The hardware observation (Batch 20)

In a dungeon, Save reported success; a reload in the same session (no power cycle) resumed "an older/prior save instead". Never seen on the surface. Batch 20 blamed `candidate()`/`restore_candidate()` in `alpha_save.cpp` for not calling `restore_dungeon` (§14 item 7). That gap is real, but it is **not** the cause; see H-166 below.

### Baseline

HEAD `60581138` (tag `alpha2-batch25-h118-shard-move-lock`, which follows `alpha2-batch24-state-reload-parity`), branch `main`, working tree clean. From-scratch build `native/core/build-batch26-baseline`, serial: **95/95 PASS, 0 fail, 0 skipped** (`batch26-baseline-ctest.log`).

### Reference behaviour (recovered from the binaries)

Re-read with `re/tools/dis16.py` and `re/tools/thunks.py`. Strings decoded at `DATA.OVL` `DS+0x10`.

- **Saving underground is allowed.** `DUNGEON.OVL:0x06C4` (the dungeon key handler) handles only the arrows 1–4, `5`, `0x0B`, Enter/`.`, `^S`/`^V` and the digits. Every other key, `Q` included, falls through `0x07BC`/`0x07C0` to `0x07A0` → `call 0xafa8` = kernel `0x3178` (`kernel_cmd_dispatch`, base `0x81D0`). There `0x34C0 cmp ax,0x51` → `0x338C`: prints `"Quit:"` (DS `0xA1EA`), then `call 0x81AE`, a thunk to overlay 18 = `CAST2.OVL:0x10FE`. That routine asks `"\nSave game? "` (`0x9658`), prints `"Yes\nSaving...\n"` (`0x966A`), writes `SAVED.GAM` (`0x9698`) from `0x55A6` for `0x6606−0x55A6 = 0x1060` bytes in one `write_whole_file` (`0x1185-0x1194`), then the 512-byte `.OOL`, then `"Done.\n"` (`0x96AC`). **There is no `g_location` (DS `0x5893`) test anywhere on that path.** Control returns to the dungeon loop (`0x3396 jmp 0x31e9`). There is no refusal message, because nothing refuses.
- **What the dump holds.** The window `[0x55A6, 0x6606)` contains `g_location` `0x5893` (`0x21..0x28` underground), `g_floor` `0x5895`, x/y `0x5896`/`0x5897`, `g_dng_facing` `0x6603`, and the **whole** 8-floor map `g_dng_map` `0x595A` (512 B), which carries every sprung trap, opened chest and cast field. It also holds the rooms-cleared bits `0x58E0` and the object table `0x5C5A`, which holds the dungeon wanderer.
- **Loading.** `INTRO.OVL:0x0EB4` reads the window back verbatim. The kernel loop (`ULTIMA.EXE 0x00DB`: `g_location >= 0x21` → `0x0104`) re-enters the session at `DUNGEON.OVL:0x0E2E` with no file read, so the map is the saved one, not `DUNGEON.DAT`.
- **One thing is not carried.** `0x0E40 mov [bp-4],0` then `0x0F15 mov di,[bp-4]`: the Rel Tym every-other-turn toggle (`0x0F25 xor di,1`) is a local of the session loop, zeroed on every entry, a load included.
- **Loose objects.** The object register `0x5C5A` is inside the window and loads verbatim. A load goes `0x11F0(fresh=0)` → `0x0408(0)`, which never reaches `0x1694`, so the current map's objects (opened chests, spilled loot) come back exactly as saved (Batch 24, H-162).

REFERENCE FACT: saving in a dungeon is permitted and silent-path identical to the surface; reloading resumes in the dungeon at the saved floor, cell and facing, with the saved map. REFERENCE FACT: the Rel Tym toggle restarts at 0 after a load.

### Native behaviour before the fix

NATIVE OBSERVATION (host, real `AlphaRuntime`, shipped pack, real `(E)nter` and System Menu, `batch26-red.log`). A save made on Deceit floor 3 (L3) loaded back with `DUNGEON_RESTORE active=0`: the party stood on the overworld at the Deceit entrance, where the surface-return position always is, with the clock rolled back. That is the "older save" the hardware saw.

Root cause, one line: `persistence.cpp`'s sidecar exporter copies only the keys in `extras[]`, and `"dungeon"` was not among them. So `capture_dungeon()`'s object (Batch 6, R-15) never reached the card, and `restore_dungeon()` always found no key and cleanly returned "no session". Batch 6's test round-tripped the in-memory JSON only (see the correction under R-15). `"worldObjects"` **is** on the list, so the loose-object half was already persisted.

Two further gaps, found while proving the fix:
- `restore_dungeon()` restored `quickness_toggle`, which 1988 resets (`0x0E40`).
- The System Menu branch of `AlphaRuntime::handle()` (and the frontend Continue branch) returns before `synchronize_after_debug()`. After a load, `context_.dungeon` and the UI base mode therefore kept their **pre-load** values until the next input, and that first input was routed by the stale mode. Before the fix, this was visible as D3e: loading a surface save while underground left the Dungeon UI mode live over a dead session. With a restored session it would have been the inverse: the first trackball press after loading a dungeon save would go to the world instead of turning the party.

The `.GAM` of a dungeon save keeps the surface-return position (location 0, entrance x/y) and the INIT template's bytes at `0x3B4`. The live session rides the JSON sidecar, as Batch 6 chose and as ledger A-14 (sidecar persistence) already declares. Batch 26 does not change that layout.

### Fix (minimal)

| File | Change |
|---|---|
| `native/core/src/persistence.cpp` | `"dungeon"` appended to `extras[]` (exported to and imported from `sidecar.gameState`). Native-only key; the TypeScript reference keeps `dungeonState` outside its save, so its sidecars never carry it and `persistence_parity` is unaffected |
| `native/core/src/gameplay_save.cpp` | `capture_dungeon()` no longer writes `"quickness"`; `restore_dungeon()` no longer requires it and leaves the toggle 0 (`0x0E40`) |
| `native/targets/tdeck/main/alpha_runtime.cpp` | end of `synchronize_loaded_world()`: `context_.dungeon`/`context_.combat` and the UI base mode are derived from the restored owners by the same `resolve_synchronized_base_mode()` rule `synchronize_after_debug()` uses; `dungeon_presentation_pending_` set when a session was restored (the full redraw the entry path gets) |
| `native/core/tests/gameplay_integration_test.cpp` | the Batch 6 round-trip now expects the toggle at 0 |

Not done, deliberately: no new `.GAM` offsets, no save-format version bump, no refusal path, no change to the `Alt+L` arm (H-164), no change to `alpha_save.cpp`'s generation/semantic-validation logic (H-166).

**Save compatibility.**
- A pre-Batch-26 save, taken anywhere, has no `"dungeon"` key: it loads exactly as it did before (C4). A dungeon save made on older firmware still loads at the dungeon entrance on the surface, because that is all the file ever contained.
- A Batch-26 save carries `sidecar.gameState.dungeon`. Older firmware ignores the key, since its import copies only listed keys.
- The `"quickness"` field was never on disk, so dropping it breaks nothing.
- The `.GAM` bytes are unchanged for every save.

### Tests — `batch26_dungeon_save` (34 checks, real `AlphaRuntime` + shipped pack)

Seam: Batch 24's in-memory save generation (`alpha_save_memory_host_stub.cpp`, the same serialization chain as `alpha_save.cpp`), plus the pack's eight `DUNGEON.DAT` maps in the host fixture. Staging is limited to fixture setup, each marked in the source:
- a one-member party;
- standing on an entrance or ladder cell;
- Deceit's Word of Passage (entrances draw sealed, tile 223, until it is known — `quest_world_tile`).

Every behaviour under test goes through production input: `(E)nter`, `(K)limb`, the trackball, `Alt+M` menus. The one exception is `(O)pen`, which runs through `execute_command` as in Batch 24.

- **C1–C4** (storage chain, the calls `AlphaSaveService` makes): the sidecar carries `"dungeon"` (C1). `load_native_state` + `restore_dungeon` return the exact session: position, facing, 512 cells, reveal and wanderer (C2). The toggle is 0 after the load (C3). A sidecar without the key, i.e. a surface or pre-26 save, loads as "no session" with no error (C4).
- **D1** (same session): `(E)nter` Deceit → S,S,turn,E,E,turn,N onto the `0x61` at (3,2). The trap on L1 and the one under it on L2 both fire (`0x61→0x60`, DUNGEON `0x0A9F`); the party lands on L3 (3,2) facing North. System Menu Save prints "Save complete" (D1c). Then turn, move and let time pass, change gold, and System Menu Load. Checks: gold and clock come back (D1e); the whole session matches the snapshot (D1f); the sprung traps are still `0x60` (D1g — the map came from the save, not a `DUNGEON.DAT` re-read); the surface-return position is the entrance (D1h); dungeon context and UI mode are live at once (D1i).
- **D2** (power cycle): a fresh `AlphaRuntime` standing outside the castle, then System Menu → Continue Latest. The whole session comes back from storage alone (D2b). Dungeon UI mode is set the moment the load ends (D2c). The very next trackball press turns the party North→East in place (D2d) and does not touch the surface position (D2e).
- **D3** (surface control / stale key): save in the dungeon and reload it, so the retained document now holds `"dungeon"`. Then `(K)limb` out at (1,1), make a **surface** save, go back underground, and load. The session ends, mode is not Dungeon (D3e), and the party stands where the surface save was made (D3f).
- **L1** (loose objects, same session): Lord British's basement. Open chest (16,21) so its loot spills, then save. Open the other two chests, then load. The pool is the saved one entry for entry and field for field (L1d). The two chests are closed again, once each, and (16,21) is open with its loot, not refilled (L1e).
- **L2** (loose objects, power cycle, different pool live): a fresh runtime in the basement opens (13,23), then loads. The pool is exactly the saved one (L2c), with no stale pre-load loot and no duplicate chest (L2d).
- **Q** (observation only, not a check): `Alt+L` underground restores `GameState` but not the session — H-164, queued, printed as `INFO`.

**RED → GREEN.** Unmodified production: **23/34 GREEN, 11 RED** (`native/core/batch26-red.log`): C1 C2 C3 D1f D1g D1i D2b D2c D2d D3b D3e. All L checks were GREEN before the fix: R-14's `worldObjects` persistence works, and the L checks now pin it with post-save mutation and a power cycle. After the fix: **34/34** (`batch26-green.log`).

**Mutation proof** (`native/core/batch26-mutations.log`): each mutation was applied, built, run and reverted, and production was rebuilt at the end (34/34). A first pass was discarded: restoring the backups left source timestamps older than the mutated objects, so ninja kept stale objects and each mutation leaked into the next. The recorded pass touches every restored file.

| | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | `"dungeon"` dropped from `extras[]` (the pre-26 exporter) | 24/34 | C1 C2 C3 D1f D1g D1i D2b D2c D2d D3b |
| M2 | the Rel Tym toggle carried across the load (Batch 6 behaviour) | 33/34 | C3 |
| M3 | no context/base-mode resync at the end of `synchronize_loaded_world()` | 31/34 | D2c D2d D3e |
| M4 | `"worldObjects"` dropped from the sidecar (no pool persistence) | 30/34 | L1d L1e L2c L2d |
| M5 | `objects_.clear()` omitted on load (stale pre-load pool) | 30/34 | L1d L1e L2c L2d |
| M6 | an inactive session does not erase `"dungeon"` (stale key in a surface save) | 33/34 | D3e |
| M7 | position restored but not the map (cells left zero) | 28/34 | C2 D1f D1g D2b D3c D3e |

### Full regression suite

From-scratch build (`native/core/build-batch26-final`), serial: **96/96, 0 fail, 0 skipped** (`batch26-final-ctest.log`) — the prior 95 plus `batch26_dungeon_save`. One warning, the pre-existing w64devkit `stl_uninitialized.h` false positive; zero project warnings. `gameplay_integration` passes with its one updated expectation. No parity fixture moved: `persistence_parity`'s TypeScript-generated states never carry a `"dungeon"` key.

### Firmware

ESP-IDF 6.1, `native/targets/tdeck/build-batch26`: `openu5_tdeck.bin` = **0xd3970** (866,672 bytes), −0x30 against Batch 25 (the `"quickness"` key strings are gone); `0x2c690` (17 %) of the app partition free. **0 errors, 0 compiler warnings** (`batch26-firmware-build.log`, built before the commit). The image embeds the commit id at configure time, so the Launcher image is rebuilt (`idf.py reconfigure build`, `package_launcher.py`) **after** the Batch 26 commit; its path and SHA-256 are recorded in the annotated tag `alpha2-batch26-h115-dungeon-save-load`. **Not flashed.** SD card unchanged.

### Status

H-115: **HOST FIXED / DEVICE RETEST PENDING** (Phase 6U). Classification: native defect (serialization seam), not a 1988 behaviour. The 1988 contract (save allowed underground, resume in place) is now what the port does on the System Menu and frontend load paths. H-124's dungeon context rides the same retest. Not HARDWARE VERIFIED until the user runs Phase 6U.

### Queued, not fixed

- **H-164 (existing) — now reproduced on host.** `Alt+L` underground restores `GameState` but leaves the live dungeon session untouched (`batch26_dungeon_save` Q: "does NOT match the save"). The Batch 26 fix lives in `synchronize_loaded_world()`, which that arm skips. Use the System Menu load until H-164's batch.
- **H-166 — the save's semantic validation does not cover the dungeon session or the object pool.** `alpha_save.cpp`'s `candidate()` (the post-write self-check and the load-time generation pick) runs `load_native_state`, `restore_gameplay`, `restore_terrain` and `restore_npc_walk` only. A newest generation whose `"dungeon"` or `"worldObjects"` payload is domain-invalid is therefore selected; `synchronize_loaded_world()` then drops the session to the surface (log line `DUNGEON_RESTORE_FAILED`) or empties the pool, instead of falling back to the older generation. Not the cause of H-115, since the payload never existed, and not reachable by play, since only a corrupted card produces an invalid payload. Robustness, not parity. By code reading.

### Phase 6U — Batch 26 dungeon save/load and loose loot · *firmware only; the SD card is unchanged*

Flash the Batch 26 firmware (the image named in the tag). No serial capture is needed. **Use the System Menu for every save and load in this phase — not `Alt+S`/`Alt+L`** (`Alt+L` is H-164, queued). Before opening the Developer menu, make sure no question is on screen (H-165).

System Menu keys: `Alt+M` opens it. Trackball up/down moves the cursor, `Enter` selects, a short Mic press goes back or closes. Root items: Resume · **Save** · **Load / Save Management** · Settings · Developer / Debug · Return to Title. **Save** is one step down from the top. **Load / Save Management** is two steps down; inside it the first item is **Continue Latest**.

**A. Dungeon save**

1. Load or start any game, standing on the surface with nothing on screen. `Alt+M` → **Save** → `Enter`. Expect `Save complete`. Short Mic to close. (This surface save is used in step 11.)
2. `Alt+D` → **Certification** → **Dungeon Test** → Confirm → Run Certification, then Mic/Back until the Developer menu is closed. Expect the 3-D view of **Deceit**, the HUD showing **L1** and **Dir: South**, standing on the up ladder. (The certification only grants the Words of Passage and performs the ordinary `(E)nter`; from here on everything is normal play.)
3. With the trackball: **Up**, **Up** (two steps south), **Left** (HUD `Dir: East`), **Up**, **Up**, **Left** (HUD `Dir: North`), **Up**. Expect the pit-trap / falling messages and the HUD at **L3**, **Dir: North** (the trap under you on L2 fires too). *If a fight starts on the way, win or flee it and continue; if you cannot reach L3, save anywhere on L2 or deeper and note the HUD level, direction and view instead.*
4. Note exactly what the screen shows (level, direction, the corridor in front). `Alt+M` → **Save** → `Enter`. **Expect `Save complete`** — there is no refusal underground in the original either. Short Mic to close the menu.
5. Change things: trackball **Right** twice (HUD `Dir: South`), then **Up** once or twice if the way is open.
6. `Alt+M` → trackball Down twice to **Load / Save Management** → `Enter` → **Continue Latest** → `Enter`. **Expect `Load complete`, still in Deceit, HUD L3 / Dir: North, the same view as in step 4.** Not the overworld, not the entrance, not L1.
7. Immediately press trackball **Right** once. **Expect the HUD to read `Dir: East` at once** (the first input after the load turns the party; it does not walk on the overworld).
8. Power cycle the T-Deck. At the title screen choose **Continue**. **Expect Deceit, L3, Dir: North** (the step-4 save; step 7 was not saved).
9. `(L)ook` once to confirm the dungeon answers normally, then `Z`-stats open/close.
10. **Surface control:** `Alt+M` → **Load / Save Management**. The two **Generation** rows are the two newest saves: one is step 4 (the dungeon), the other step 1 (the surface). Load **each** in turn with `Enter`. **Expect:** the dungeon generation puts you in Deceit L3 / Dir: North; the surface generation puts you on the overworld where step 1 was saved, the HUD without L/Dir, and the first trackball press is an ordinary overworld move (a step, or `Blocked`).

**Pass A:** steps 4, 6, 7, 8 and 10 as stated. **Fail** if the save is refused or fails, if any load in step 6/8 lands on the overworld or at the Deceit entrance, if the level/direction differ from step 4, or if the first press after a load does nothing or moves on the overworld.

**B. Loose loot persistence**

1. Enter **Lord British's Castle**, go down the ladder at (1,1) to the basement.
2. Stand at (16,22) and `(O)pen` north: the chest at (16,21). Its contents spill onto the floor. Note what lies there.
3. `Alt+M` → **Save**. Short Mic to close.
4. `(O)pen` the chests at (17,22) and (13,23) too (standing at (17,23) and (13,24), opening north).
5. `Alt+M` → **Load / Save Management** → **Continue Latest**.
6. **Expect:** (16,21) still open with exactly the loot noted in step 2, not refilled, not doubled. (17,22) and (13,23) are **closed chests again**, one each, with **no loot** in front of them.
7. Power cycle → title **Continue**. Expect the same as step 6.

**Pass B:** steps 6 and 7. **Fail** if a chest refills, a loot pile is doubled or missing, or a chest opened after the save is still open after the load.

**Known and queued, do not file:** H-164 (`Alt+L` does not restore the dungeon or the object pool — use the System Menu); H-165 (a question left pending across a Developer teleport into a dungeon); H-166 (a corrupt dungeon/object payload is not rejected by the save's self-check).

## Batch 27 — H-164: `Alt+L` restores what Continue Latest restores

Scope: H-164 only. Not touched: H-154–H-160, H-165, H-166 (see the boundary check below), H-118's device retest, persistence formats, A-14, the renderer.

### Baseline

HEAD `0a2c1978` (tag `alpha2-batch26-h115-dungeon-save-load`), branch `main`, working tree clean. From-scratch build `native/core/build-batch27-baseline`, serial: **96/96 PASS, 0 fail, 0 skipped** (`batch27-baseline-ctest.log`).

Batch 26's evidence for H-164 was observation Q in `batch26_dungeon_save`: a printed `INFO`, not a check. It saved on Deceit L3 (3,2) facing North, turned twice, pressed `Alt+L` and printed "dungeon session does NOT match the save (saved facing 0, now 2)". That showed the live session survives `Alt+L`. It did not show that the session is never restored from disk, because the stale session happened to be in the same dungeon.

### What `Alt+L` is supposed to mean

1988 has one load. `INTRO.OVL 0x0EB4` reads the whole save window back, and the kernel loop (`ULTIMA.EXE 0x00DB`) resumes in whatever context it holds, the dungeon included (Batch 26). The device's `Alt+L` (`ui_input_adapter.cpp`: Alt + `l` → `DeviceShortcut::Load`, announced by the runtime as "Alt+L load") is a keyboard affordance for that same load of the newest generation. So is System Menu → Load / Save Management → Continue Latest. Neither is a lighter "quick" load in the original, so both must end in the same state.

1. **What is read from disk:** identical for both. Each calls `AlphaSaveService::load(context_, outdoor_, terrain_, actors_, retained_, ms)` (`alpha_save.cpp`): `candidate()` for both slots, `select_generation`, then `restore_candidate()`. That is transactional. `load_native_state` plus `restore_gameplay`/`restore_terrain`/`restore_npc_walk` run into scratch copies, and the live `GameState`, `TurnState`, `CommandState`, outdoor, terrain, actors and retained document are replaced only if all of them succeed. It falls back to the older generation. The dungeon session and the object pool are **not** restored here; they stay in the retained document (`gameState.dungeon`, `worldObjects`).
2. **What must be rebuilt afterwards:** everything `synchronize_loaded_world()` (`alpha_runtime.cpp`) does. That covers cancelling the scenes and fx, the `0x0408(0)` door reset, the NPC re-entry, the terrain refresh, clearing live combat, clearing and restoring the object pool, restoring the dungeon session (Rel Tym toggle at 0), re-deriving the context and base mode, and the dungeon redraw request.

### The two paths, side by side

| Step | System Menu → Continue Latest | `Alt+L` (Batch 26) |
|---|---|---|
| input | `handle()` → `system_menu_.active()` branch → `system_menu_.handle` → `service_system_menu_intent()` | `handle()` → past the modal gates (all require `shortcut==None`) → the `DeviceShortcut::Load` arm |
| read | `save_.load(...)` | `save_.load(...)`, the same call |
| on failure | "No valid save", nothing else | "No valid save", nothing else |
| scenes/fx/poison cancelled | ✓ | ✗ |
| `commands_.door.turns=0` (H-162) | ✓ | ✓ (a hand copy) |
| `actors_={}`, `enter_npc_map`, `restore_npc_walk` | ✓ | ✓ without the `actors_={}` |
| `terrain_.refresh` | ✓ | ✓ |
| `combat_.initialized=false` | ✓ | ✗ |
| `objects_.clear()` + `restore_world_objects` (R-14) | ✓ | **✗** |
| `restore_dungeon` (R-15 / H-115) | ✓ | **✗ ← first semantic divergence that matters** |
| `context_`/base mode from the restored owners | ✓ (Batch 26) | via the trailing `synchronize_after_debug()` |
| `dungeon_presentation_pending_` | ✓ | ✗ |
| after | `trace_direct_troll_save`, `system_menu_.close()`, "Load complete", return (no `synchronize_after_debug`) | `trace_direct_troll_save`, "Load complete", then the ordinary tail: `synchronize_after_debug()`, `drain_pending_npc_initiation()` |

The first divergence is at the first statement after a successful read. The System Menu calls the canonical helper; the `Alt+L` arm runs its own inline subset. The subset was written before R-14/R-15 existed and was extended piecemeal (Batch 24 added the door reset to both arms instead of routing `Alt+L` through the helper; D-23). It never gained the two sidecar-owned restores. Frontend Continue/LoadSlot and System Menu LoadLatest/LoadSlot all call `synchronize_loaded_world()`. `Alt+L` was the only load route that did not.

Root cause, one line: **the `Alt+L` arm duplicated part of `synchronize_loaded_world()` instead of calling it, and the duplicate never restored the dungeon session or the object pool.** Persistence itself was correct.

### Fix (minimal)

| File | Change |
|---|---|
| `native/targets/tdeck/main/alpha_runtime.cpp` | the `DeviceShortcut::Load` arm, on a successful `save_.load`, calls `synchronize_loaded_world()` (then the same `trace_direct_troll_save` it already had). The hand-copied door reset / NPC re-entry / terrain refresh lines are removed; the helper already does all three. A failed read still does nothing but print "No valid save" |

Why this is the minimal architectural fix: the canonical successful-load finalization already existed, and four of the five load routes already used it. Nothing new is abstracted. No format, no `alpha_save.cpp`, no core file, and no dungeon-specific branch is touched. The shortcut simply stops being a special case. The helper's own comment ("the System Menu branch returns before `synchronize_after_debug()`") remains true; `Alt+L` still runs that tail afterwards. It re-derives the context and base mode by the same rule from the same owners, so the result is idempotent, and it refreshes terrain again.

Host-only test seams, never linked into firmware (`alpha_save_memory_host_stub.cpp`): `host_memory_save_forget_for_test()` (no generation, i.e. a missing save) and `host_memory_save_damage_for_test()` (the stored sidecar cut in half, so `load_native_state`'s parse rejects it, i.e. a corrupt save).

### Tests — `batch27_alt_load` (37 checks, real `AlphaRuntime` + shipped pack)

This is the same seam and fixture as `batch26_dungeon_save`: the in-memory generation, real `(E)nter`/`(K)limb`/trackball, real `Alt+S`/`Alt+L` raw keys and real `Alt+M` menus. `(O)pen` goes through `execute_command` as in Batches 24 and 26. Rel Tym is staged by setting `TurnState::time_spell='Q'` and taking one dungeon turn.

- **X** (failure behaviour, run first while the store is empty). X1a–c: no save; `Alt+L` and Continue Latest both report "No valid save" and change no observable field. X2–X2c: a fresh runtime makes a real save, plays on, then the stored sidecar is damaged. Both routes reject it and synchronize nothing; gold stays 999 and the session and pool stay untouched.
- **S** (same session): save on Deceit L3 (3,2) N with the L1/L2 traps sprung. Turn, move, cast Rel Tym (toggle 1), then `Alt+L`. S2: "Load complete", gold and clock back. H164-A1: session active, Deceit. **H164-B1**: floor 2 (3,2) N with the saved map, reveal and wanderer. **H164-G**: the Rel Tym toggle is 0. H164-C1: dungeon context and Dungeon UI and base mode at once. **H164-C2**: the next trackball press turns the *restored* party N→E.
- **K** (leave, then quick load): `Alt+S` at the Deceit L1 ladder, `(K)limb` out to Britannia, `Alt+L`. **H164-A2**: underground at the saved cell, not at the surface entrance. **H164-C3**: in the dungeon loop at once.
- **P** (power cycle): a fresh runtime outside the castle, `Alt+L` of the L3 save. **H164-B2**: the whole session from storage alone. **H164-D**: the sprung traps are still `0x60` and the map/reveal/wanderer are the saved ones, not `DUNGEON.DAT`. **H164-C4/C5**: Dungeon mode at once; the next press turns the restored party.
- **E** (loose objects): Lord British's basement. Open (16,21), `Alt+S`, open the other two, `Alt+L`. **H164-E1/E2**: the pool is exactly the saved one; two chests are closed again, once each, and (16,21) is open with its loot. **H164-E3**: a fresh runtime with a different pool live, then `Alt+L`, keeps no pre-load object.
- **F** (surface). H164-F1: an ordinary surface save loads through `Alt+L` as before (position, gold, Exploration). **H164-F2**: `Alt+L` of a surface save while underground ends the live dungeon session.
- **Q** (cross-path equivalence, below).

**RED → GREEN.** Unmodified Batch 26 production (`alpha_runtime.cpp` from `0a2c1978`, with only the test, stub seams and CMake target added): **21/37 GREEN, 16 RED** (`native/core/batch27-red.log`). The REDs are H164-B1 G C2 A2 C3 B2 D C4 C5 E1 E2 E3 F2 and Q1 Q2 Q3. Every one is a dungeon-session or object-pool field that `synchronize_loaded_world()` restores and the arm did not. The controls were GREEN before the fix: the failure checks X*, S2 (the `.GAM` half always loaded), H164-F1 (surface), and H164-A1/C1. A1 and C1 are only GREEN before the fix because the *stale* session is still in Deceit; K and P show the real failure. After the fix: **37/37** (`batch27-green.log`).

**Mutation proof** (`native/core/batch27-mutations.log`). Each mutation was applied to the fixed source, built, run and reverted; every restore was `touch`ed so ninja rebuilt it. Production was rebuilt at the end: 37/37.

| | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | `Alt+L` arm without `synchronize_loaded_world()` | 21/37 | H164-B1 G C2 A2 C3 B2 D C4 C5 E1 E2 E3 F2, Q1 Q2 Q3 |
| M2 | the arm finalizes a **failed** read too (`if(ok)` dropped) | 33/37 | X1b X1c X2b X2c |
| M3 | `synchronize_loaded_world()` without `restore_dungeon` | 26/37 | H164-A1 B1 G C1 C2 A2 C3 B2 D C4 C5 |
| M4 | `synchronize_loaded_world()` without the pool clear + restore | 34/37 | H164-E1 E2 E3 |
| M5 | `synchronize_loaded_world()` without the context/base-mode resync | 36/37 | Q1 |

M5 is killed only by Q1. `Alt+L` itself still passes the C checks under M5 because its trailing `synchronize_after_debug()` re-derives the same values; the System Menu route has no such tail, which is why Batch 26's D2c/D2d/D3e pin it. A first mutation pass ran X1 and X2 in one runtime, and M2 survived X2 there: X1's failed-read finalization had already cleared the session, so X2 compared two surface states. The recorded test runs X2 in a fresh runtime, and M2 now dies at X2b/X2c.

### Cross-path equivalence

`Snapshot` (in the test) covers: the `GameState` position (map/floor/x/y), clock, gold/karma, dungeon active, id/level/cell/facing, map/reveal/wanderer, the Rel Tym toggle, the loose-object pool, command context dungeon/combat, live combat, UI mode and base mode, every NPC actor's x/y/z/state, and the open-door tracker. It is compared field by field.

- **Q1**: one dungeon save; two fresh runtimes, one `Alt+L` and one Continue Latest. Identical. Before the fix it differed in five fields: dungeon active; id/level/cell/facing; map/reveal/wanderer; context; UI mode.
- **Q2**: one castle-basement save (objects and NPCs); two fresh runtimes. Identical. Before the fix it differed in the pool.
- **Q3**: one runtime. Save, perturb, `Alt+L`, snapshot; perturb differently, Continue Latest, snapshot. Identical. Before the fix it differed in the dungeon position and the map.

**Intentional differences, none gameplay-visible:**
- The System Menu route closes the menu it was opened from; `Alt+L` never opened one.
- `Alt+L` then runs the ordinary input tail. `synchronize_after_debug()` is idempotent here: the same `resolve_synchronized_base_mode()` rule over the same owners, plus a second `terrain_.refresh` of the same `game_`. `drain_pending_npc_initiation()` has nothing to drain, since an initiation is queued and drained inside the same `handle()` call, and it drops any initiation for another location in any case.
- The frontend Continue path (title screen) also skips `trace_direct_troll_save`, which is a log line only.

### Failure behaviour

`save_.load` is unchanged and still transactional, and the arm still finalizes only when it returns `true`. X1/X2 show that a missing or corrupt save leaves every snapshot field unchanged through both routes. M2 shows those checks would catch a finalization on failure.

### H-166 boundary check

H-166 (the `candidate()` self-check does not validate `"dungeon"`/`"worldObjects"`, so a domain-invalid newest generation is selected rather than falling back) is **unchanged and still queued**. Its evidence is code reading (Batch 26 §"Queued"); no test demonstrates it, and none was added or altered. `alpha_save.cpp` and the generation logic are untouched, and the damage seam used here produces a *parse* failure, which `load_native_state` already rejects. That is the handled path, not H-166's domain-invalid-but-well-formed one. The refactor does not hide H-166; `DUNGEON_RESTORE_FAILED`/`WORLD_OBJECTS_RESTORE_FAILED` still log from the one helper. It does widen H-166's reach by one route. Before, `Alt+L` ignored the payload entirely and kept whatever session and pool were live. Now, like the other four routes, it applies the helper's fallback (no session, empty pool) to an invalid payload. That is the same outcome as Continue Latest, which is the point of this batch. The fix for choosing the older generation instead stays with Batch 28.

### Full regression suite

From-scratch build (`native/core/build-batch27-final`), serial: **97/97, 0 fail, 0 skipped** (`batch27-final-ctest.log`). That is the prior 96 plus `batch27_alt_load`. The build has one warning, the pre-existing w64devkit `stl_uninitialized.h` false positive; there are zero project warnings. `batch24_reload_parity` (S1, the `Alt+L` door reset now reached through the helper), `batch25_shard_ritual` (`Alt+L` leaves `CommandState` alone, as before) and `batch26_dungeon_save` all pass unchanged. Batch 26's observation Q now prints "dungeon session matches the save". No parity fixture moved.

### Firmware

ESP-IDF 6.1, from scratch in `native/targets/tdeck/build-batch27`: `openu5_tdeck.bin` = **0xd3920** (866,592 bytes), −0x50 against Batch 26 (the inline copy is gone); `0x2c6e0` (17 %) of the app partition is free. **0 errors, 0 compiler warnings** (`batch27-firmware-build.log`, built before the commit). The Launcher image is rebuilt (`idf.py reconfigure build`, `package_launcher.py`) **after** the Batch 27 commit so it embeds that commit. Its path and SHA-256 are recorded in the annotated tag `alpha2-batch27-h164-alt-load`. **Not flashed.** SD card unchanged.

### Status

H-164: **HOST FIXED / DEVICE RETEST PENDING** (Phase 6V). Classification: native defect (device routing), not a 1988 behaviour and not a reference-port difference. D-23 is resolved. H-118/shard movement: unchanged. It was fixed on host in Batch 25 and its device retest (Phase 6T) is still the user's, separate from this batch. H-115: unchanged by this batch (host fixed, Phase 6U).

**Still open:** H-154–H-160 (H-158/H-159 were fixed in Batch 23; H-154–H-157 and H-160 remain), H-165, H-166.

### Phase 6V — Batch 27 `Alt+L` quick load · *firmware only; the SD card is unchanged*

Flash the Batch 27 firmware (the image named in the tag). No serial capture is needed. Before opening the Developer menu, make sure no question is on screen (H-165).

1. On the surface with nothing on screen: `Alt+M` → **Save** → `Enter`. Expect `Save complete`. Short Mic to close. Note where you stand.
2. Walk three steps. Press `Alt+L`. **Expect `Load complete` and the party back where step 1 saved, with the overworld HUD.** Press one direction: an ordinary overworld step (or `Blocked`).
3. `Alt+D` → **Certification** → **Dungeon Test** → Confirm → Run Certification, then Mic/Back until the Developer menu is closed. Expect Deceit, HUD **L1**, **Dir: South**.
4. Trackball **Up**, **Up**, **Left**, **Up**, **Up**, **Left**, **Up**. Expect the trap messages and HUD **L3**, **Dir: North**. *If a fight intervenes, finish it; if L3 is out of reach, use any level ≥ L2 and note the HUD.*
5. Note the level, direction and view. `Alt+M` → **Save** → `Enter`. Expect `Save complete`. Short Mic to close.
6. Trackball **Right** twice (HUD `Dir: South`), then **Up** once if the way is open.
7. Press `Alt+L`. **Expect `Load complete` and, with no other key, the step-5 level, direction and view.** Not the overworld, not the entrance.
8. Trackball **Right** once. **Expect `Dir: East` at once**: the controls are live in the dungeon.

**Pass:** steps 2, 7 and 8 as stated. **Fail** if `Alt+L` lands on the overworld or at the Deceit entrance after step 7, keeps the step-6 facing, or needs an extra key before the view or controls update.

**Known and queued, do not file:** H-165 (a question left pending across a Developer teleport into a dungeon); H-166 (a corrupt dungeon/object payload is not rejected by the save's self-check).

## Batch 28 — H-166: a save generation is validated whole before it may replace the live world

Scope: H-166 only. Not touched: H-118 (Phase 6T), H-115 (Phase 6U), H-164 (Phase 6V), H-154–H-157, H-160, H-165, pacing, audio, the renderer, the Windows frontend, the save format, A-14.

### Baseline

HEAD `e5692ae0` (tag `alpha2-batch27-h164-alt-load`), branch `main`, working tree clean. From-scratch build `native/core/build-batch28-baseline`, serial: **97/97 PASS, 0 fail, 0 skipped** (`batch28-baseline-ctest.log`).

### What H-166 actually was

The Batch 26 hypothesis held, and it is now reproduced on host. `alpha_save.cpp` checked a generation's CRCs, its parse and three of its owners (gameplay, terrain, NPC walk) before letting it replace the live world. The other two sidecar owners, `"worldObjects"` (R-14) and `"dungeon"` (R-15/H-115), were decoded only **after** the commit, by `synchronize_loaded_world()`, whose fallback on a bad payload is "empty pool" / "no session". So a newest generation that was well-formed and CRC-consistent but carried a dungeon or object payload the runtime refuses:

- was listed **valid** in Load / Save Management;
- **won** generation selection over a good older generation;
- loaded as the newest generation's party, clock and gold with the dungeon session dropped (party on the surface) or the object pool emptied. No save ever held that mixture.

The same gap let the post-write self-check certify a save the loader would refuse ("Save complete"). Only a corrupted card, a foreign writer or a firmware bug produces such a payload; it is robustness, not parity.

### The flow before Batch 28 (traced, not taken from comments)

A generation is `alpha1-g<slot>.{gam,ool,json}` plus `alpha1-g<slot>.commit` = `{magic 0x31533555, version 1, sequence, CRC(gam), CRC(ool), CRC(json)}`, two slots, `slot = sequence & 1`.

| Step | Code | What it checks / does |
|---|---|---|
| save | `AlphaSaveService::save` | capture every owner into `retained_`, `export_native_state`, `build_ool`, `encode_json`; write three temps; CRC read-back; rename the three; write + rename the commit — **the generation now exists**; then `stage("semantic-validation", candidate(slot))`. A failed self-check prints "Save failed; prior kept" but the committed generation stays on the card as the newest; only the load's fallback made "prior kept" true |
| per-slot check | `candidate(slot)` | read commit + three files; `complete_generation` (committed, sizes, three CRCs, JSON parse, `sane_sidecar`); `load_native_state` into zeroed scratch; `restore_gameplay`; `restore_terrain`; `restore_npc_walk`. **Not** `worldObjects`, **not** `dungeon` |
| used by | `load` (fills both `Generation`s, result ignored), `load_slot` (must pass), `inspect` (the "valid"/"corrupt" rows), the save self-check | |
| choice | `select_generation` | highest sequence among `complete_generation` ones (CRC + parse only) |
| restore | `restore_candidate` | the same chain over **copies** of the live owners; commits `GameState`, `TurnState`, `CommandState`, outdoor, terrain, actors and the retained document only when all pass. On failure `load` drops that slot and selects again (one fallback) |
| after `true` | `synchronize_loaded_world()` (every route since Batch 27) | clears the pool, `restore_world_objects(retained_)` → on error `WORLD_OBJECTS_RESTORE_FAILED`, pool empty; `restore_dungeon(retained_)` → on error `DUNGEON_RESTORE_FAILED`, no session |

Parsed vs checked, per sidecar component, before the fix: the `.GAM` fields and `transport`/`questFlags` (`load_native_state`), `overworldEnemies`/`openDoors`/`chunkOrigin` (`restore_gameplay`), `mapOverrides` (`restore_terrain`) and `npcWalk` (`restore_npc_walk`) were checked at the gate. `worldObjects` and `dungeon` were only parsed there and checked after the commit.

Answers to the batch questions: `candidate()` validated CRC/parse/gameplay/terrain/NPC walk. A generation was "usable" once `restore_candidate` succeeded, i.e. before its two sidecar owners were looked at. Invalid `dungeon` and invalid `worldObjects` data could each make a newer generation win, and one bad component poisoned the load by being *dropped* rather than refused. The older generation was available and the fallback loop already existed; it was never asked, because nothing failed before the commit.

### The correct contract

- **A generation is all or nothing.** The 1988 save is one window (`CAST2.OVL:0x10FE` writes `0x55A6..0x6606` in one call; `INTRO.OVL:0x0EB4` reads it back whole). The dungeon registers, `g_dng_map` (`0x595A`) and the object register (`0x5C5A`) are inside it (Batch 26). Mixing one generation's party with an empty pool or a dropped session is a state the original can never be in; it is worse than the older, complete save.
- **Newest refused, older complete → the older loads, whole.** This is what A-14's two-slot commit is for, and `load`'s fallback loop already does it for gameplay/terrain/NPC failures. H-166 only extends the gate to the two owners it missed.
- **No usable generation → nothing changes.** Continue Latest and `Alt+L` print "No valid save"; the title screen shows its existing error. No live owner, pool or session is touched.
- **An explicit Generation row does not fall back.** The player named that generation: `inspect` lists a refused one as **corrupt** and the row does nothing (System Menu); the title Load page says "No valid save in this slot". Unchanged behaviour, now also applied to sidecar corruption.
- **Save self-check:** a save whose own sidecar would be refused reports "Save failed; prior kept", and the next load really does restore the prior generation.
- **No recovery UI, no repair, no sanitising.** A refused generation is left on the card untouched.

### The fix — one validation boundary

`alpha_save.cpp` needs ESP-IDF's VFS/FATFS and never built on host, so the host seam re-implemented its semantic chain, and no test could reach H-166. Batch 28 first split the storage-independent half **unchanged** into `alpha_save_generation.cpp` (`verify_candidate`, `restore_candidate`, `restore_newest`, and the one staging function under them), which both the firmware and the host stub compile. Batches 24–27 pass unchanged on the split (47/42/34/37). Then the gate:

```
stage_generation()            alpha_save_generation.cpp: the only step between "parsed" and "may replace live state"
  load_native_state            .GAM + sidecar parse
  restore_gameplay             enemies / doors / chunk origin
  restore_terrain              map overrides
  restore_npc_walk             NPC walk
  validate_world_objects       NEW: restore_world_objects' own decoder, no pool touched
  restore_dungeon -> scratch   NEW: into AlphaSaveStage::dungeon, never the live session
callers: verify_candidate  -> save self-check, load_slot, inspect, load's per-slot pass
         restore_candidate -> load_slot, restore_newest (Continue Latest, Alt+L, title Continue)
```

Every load route, the Generation rows and the self-check pass through it; none validates on its own. `synchronize_loaded_world()` is unchanged. Its two fallbacks stay as defence in depth. For a generation that passed the gate they cannot fire on content, because the same decoders already accepted the same document; the pool one can still fire if `object_reserve` finds no PSRAM, which is a memory condition, not a property of the save.

| File | Change |
|---|---|
| `native/targets/tdeck/main/alpha_save_generation.{h,cpp}` | new; the storage-independent half of `alpha_save.cpp`, moved unchanged, plus the two gate lines in `stage_generation()` |
| `native/targets/tdeck/main/alpha_save.cpp` | file I/O, timing, logging only; calls the functions above (`Commit`/`Candidate` become aliases, the scratch holds one `AlphaSaveStage`) |
| `native/targets/tdeck/main/CMakeLists.txt` | firmware compiles `alpha_save_generation.cpp` |
| `native/core/src/gameplay_save.cpp`, `include/openu5/gameplay_save.h` | `restore_world_objects`' per-entry rules moved into `decode_world_object` (same rules); new `validate_world_objects`; `restore_dungeon` accepts `dungeon` 33–40 instead of 0–255 |
| `native/targets/tdeck/host_tests/host_stubs/alpha_save_memory_host_stub.cpp` | two slots with commit + CRCs; `load`/`load_slot`/`inspect`/self-check call the production functions; new seam `host_memory_save_edit_for_test`; `damage` now tears the newest generation (CRC) |
| `native/targets/tdeck/host_tests/batch28_save_validation_test.cpp`, `native/core/CMakeLists.txt` | new ctest; the four existing memory-stub targets also compile the new TU |

### Invariants enforced

**`"dungeon"`** (`restore_dungeon`, run into scratch at the gate):

| Field | Rule | Why |
|---|---|---|
| key absent | valid: no session | a surface save, or any pre-Batch-26 save |
| key present | must be an object | |
| `dungeon` | integer **33–40** (new in Batch 28) | see below |
| `floor`, `x`, `y` | 0–7 | unguarded indices into the 8×8×8 grid (`dungeon.cpp` `offset()`) |
| `facing` | 0–3 | unguarded index into the 4-entry delta tables |
| `cells` | array of exactly 512 integers 0–255 | the grid is 512 bytes |
| `revealed` | array of exactly 64 integers 0–255 | 64 bytes |
| `wanderer` | object; `bank`, `type`, `x`, `y`, `floor`, `attr`, `prevX`, `prevY` integers 0–255; `hidden` bool | `uint8_t` storage |

The dungeon id rule is the only new domain rule. `pos.dungeon` is the session's location. Every producer of a live session gives it 33–40: `dungeon_load()` takes the id from the pack's eight `DUNGEON.DAT` maps, and `EnterDungeon` is only issued for `location_at()` 33–40, the underground `g_location` range `0x21..0x28` (Batch 26). The runtime keys four things off it: the room maps (`dungeon_room_map`), the rooms-cleared bits (`cleared_bit`), the exit position (`exit_dungeon`: `locations[dungeon-1]`) and the wall variant. Below 33 a room cell hands `dungeon_encounter` a negative map index, which it runs as a **corridor** fight; above 40 the index names no arena and the encounter is refused as an invalid context. `(K)limb`ing out lands at (0,0) (id 0 or ≥ 41, past the 40-entry location table) or at a town's entrance (ids 1–32). That is concrete misbehaviour, not an improbable value.

Deliberately **not** enforced (preservation): any cell value 0–255 (`P1` loads a floor of `0xFF` cells verbatim); any reveal byte; the wanderer's full `uint8_t` range (type 255 = none, x/y 255 = unset; every consumer already bounds-checks: `dungeon_cell`'s range guard, the movement wrap, `kDungeonMonBanks`, `enemy_def_count`); and no cross-check between the session and the `.GAM` surface position, because a Developer teleport into a dungeon legitimately leaves that position in a town.

**`"worldObjects"`** (`validate_world_objects` = `restore_world_objects`' rules, unchanged since R-14, now shared through `decode_world_object`): key absent → valid, empty pool; present → an array; each entry an object with `location`/`x`/`y` 0–255, `floor` −1–255, `tile` 0–2047, `plotZ` −1–255, `item` 0–8 (`PlotItem`, 8 = None), `plot`/`shadowlord`/`search`/`loot`/`chest`/`prop`/`trapped`/`ship`/`torch` bools, `itemId`/`quality`/`contents` 0–1023, `slot` −1–255, `hull`/`skiffs` 0–255. Not enforced: the count (bounded by `kMaxJsonBytes`; a failed `reserve` at sync is a memory condition, not a property of the save), coordinates against map size, and cross-object consistency. No new object rule was added: there was no evidence that any in-range value misbehaves.

**Compatibility.** No save a device ever wrote is refused. `capture_dungeon` runs only for a live session, and every session is 33–40. `capture_world_objects` has written all 22 fields since its one introducing commit (`94cda853`), with the same rules. Before R-14 the retained `worldObjects` could only come from `import_native`. `INIT.GAM` starts at location 13 with an empty object table, so `read_objects` (the reference's ship/horse entry format, which the native decoder does not accept) never ran on a New Journey, and every device sidecar has carried the key, which overrides the `.GAM` list on import. No format, `.GAM` or version change.

### Tests — `batch28_save_validation` (51 checks, real `AlphaRuntime` + shipped pack)

Seam: the memory stub now holds **two** generation slots with commit records and CRCs. Its `load`, `load_slot`, `inspect` and post-write self-check call the **production** `alpha_save_generation.cpp`, so CRC, parse, semantic gate, choice and fallback are the device's code. `host_memory_save_edit_for_test(newest|older, edit)` parses one generation's sidecar, applies an edit to `sidecar.gameState`, re-encodes it and **re-seals its CRC**. The result is exactly H-166's class: well-formed and CRC-consistent, with wrong content. Staging: a one-member party, Deceit's Word, and three marker loot objects appended through the runtime's own `QuestWorldServices`.

Every case starts from two real System Menu saves: **OLD** (seq 1: Deceit L1 (1,1) South, gold 111, pool {1}) and **NEW** (seq 2: Deceit L3 (3,2) North after the two traps, gold 222, pool {1,2}); then the live world moves on (turned East, gold 999, pool {1,2,3}). The oracles are each generation loaded on its own through its production Generation row in a fresh runtime (`O1`).

- **A** (control): NEW valid → Continue Latest restores NEW, field for field equal to NEW's own row.
- **B** (control, unreadable newest): NEW torn → listed corrupt, Continue Latest restores OLD whole. Already handled by the CRC before Batch 28.
- **C1–C6** (NEW's `dungeon` invalid, its `worldObjects` valid): level 8; 511 cells; facing 4; dungeon id 0; id 41; `wanderer.hidden` a number. Each checks that Continue Latest restores **OLD whole** (C*n*), that `inspect` lists NEW corrupt and OLD valid (C*n*i), and that NEW's *valid* pool was not kept beside OLD (C*n*m, the **mixed-corruption** check).
- **D1–D4** (NEW's `worldObjects` invalid, its `dungeon` valid): `item` 9; an entry that is a number; `floor` −2; the array replaced by an object. D*n*, D*n*i and D*n*m as above (D*n*m: NEW's valid floor-2 session not kept).
- **P1–P4** (valid but unusual, must load **NEW** verbatim): a floor of `0xFF` cells, all-`0xFF` reveal, a dormant wanderer at x 200 with bank 255, dungeon 40; an object at tile 2047 / slot, hull, skiffs 255 / floor −1 / contents 1023 / item 8; no `dungeon` key (surface or pre-26 save); no `worldObjects` key (pre-R-14 save).
- **F** (routes): `Alt+L` (F1d, F1o) and the title screen, System Menu → Return to Title → Journey Onward → Continue (F2d, F2o), each with a dungeon and an object corruption, restore OLD whole. F3: NEW's Generation row, once refused, loads nothing and changes nothing.
- **G** (no usable generation: NEW's dungeon and OLD's objects both invalid): Continue Latest (G1), `Alt+L` (G2) and title Continue (G3), live in Deceit with a session, pool {1,2,3}, gold 999, facing East. G4: live in Lord British's castle among its NPCs, gold 777. Every snapshot field is unchanged.
- **S** (self-check): a live object with `item` 9 is staged, then saved: "Save failed; prior kept" (S1); Continue Latest then restores NEW, not gold 555 with an emptied pool (S2).

**RED → GREEN.** Against the behaviour-preserving split with no gate (production semantics at `e5692ae0`): **10/51 GREEN, 41 RED** (`native/core/batch28-red.log`). The ten GREENs are the controls and preservation cases: O0 O1 A0 A1 B1 B2 P1–P4. Every C/D/F/G/S check is RED. Typical failures: C1 "got gold=222 session=0" (NEW's party, session dropped); C4 "gold=222 session=1 floor=2" (dungeon id 0 accepted); D1 "gold=222 pool=0" (pool emptied). The log carries 18 `DUNGEON_RESTORE_FAILED`/`WORLD_OBJECTS_RESTORE_FAILED` lines, the post-commit fallbacks firing. After the fix: **51/51** (`batch28-green.log`).

### Mutation proof (`native/core/batch28-mutations.log`)

A Python driver applied each mutation to production, rebuilt, ran the test, restored the file byte for byte and touched it (Batch 26's stale-object lesson); cmake was called by absolute path. After the last restore: 51/51.

| | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | the Batch 28 gate removed (both lines) | 10/51 | the exact RED set: C1–C6 (×3), D1–D4 (×3), F1d F1o F2d F2o F3, G1–G4, S1 S2 |
| M2 | dungeon validated, `worldObjects` not | 32/51 | D1–D4 (×3), F1o F2o, G1–G3, S1 S2 |
| M3 | `worldObjects` validated, dungeon not | 26/51 | C1–C6 (×3), F1d F2d F3, G1–G4 |
| M4 | no fallback: stop at the first refused generation | 26/51 | C*n*/C*n*m, D*n*/D*n*m, F1d F1o F2d F2o, S2 |
| M5 | a refused generation still commits its `GameState` | 47/51 | G1–G4 |
| M6 | dungeon id back to 0–255 | 45/51 | C4, C5 (×3 each) |

M1 reproducing the RED set exactly shows the RED was measuring this gate and nothing else in the split. M5 survives the fallback cases by design: there the older generation is committed over the leak. G is what pins atomicity.

### Atomicity

`restore_candidate` stages into copies (`AlphaSaveStage`) and moves them into the live owners only after `stage_generation()` returns true. The dungeon session and the pool are live only in `AlphaRuntime` and are touched only by `synchronize_loaded_world()`, which runs only after a `true`. So a refused generation cannot write anything. G1–G4 prove it end to end with recognisable live state in two contexts: position, clock, gold/karma, the dungeon session (id/level/cell/facing/map/reveal/wanderer), the pool, the command context, the UI base mode, every NPC actor's x/y/z/state (G4: the castle) and the open-door tracker, all unchanged after each route's failed load. M5 shows those checks catch a partial commit. With a fallback, the result is field-for-field equal to OLD's own row (C/D/F): nothing from the live state or from NEW survives (the markers and the C*n*m/D*n*m checks name the source of the pool and the session).

### Cross-path coverage

| Route | Entry | Function | Checks |
|---|---|---|---|
| System Menu → Continue Latest | `service_system_menu_intent` | `load` → `restore_newest` | A1 B2 C* D* P* G1 G4 S2 |
| `Alt+L` | `DeviceShortcut::Load` arm | `load` → `restore_newest` | F1d F1o G2 |
| Title → Journey Onward → Continue | `service_frontend_intent` | `load` → `restore_newest` | F2d F2o G3 |
| System Menu Generation row | `inspect` → `load_slot` | `verify_candidate` / `restore_candidate` | C*i D*i B1 F3, oracles O1 |
| Title Load page (slot) | `service_frontend_intent` | the same `inspect`/`load_slot` | not driven separately: same functions as the row |
| Save self-check | `save` | `verify_candidate` | S1 S2 |

No route validates on its own and none bypasses `stage_generation()`. Batch 27's post-load unification is untouched.

### Full regression suite

From-scratch build (`native/core/build-batch28-final`), serial: **98/98, 0 fail, 0 skipped** (`batch28-final-ctest.log`): the prior 97 plus `batch28_save_validation`. The build has one warning, the pre-existing w64devkit `stl_uninitialized.h` false positive; zero project warnings. Targeted before that: `batch28_save_validation` 51/51, `batch27_alt_load` 37/37, `batch26_dungeon_save` 34/34, `batch25_shard_ritual` 42/42, `batch24_reload_parity` 47/47. `gameplay_integration` (Batch 6's `restore_dungeon` round trip, dungeon 35) and `persistence_parity` also pass. Batch 27's X2 (a torn save) now fails at the CRC rather than the parse, since the stub stores CRCs as the card does; same outcome. No parity fixture moved.

### Firmware

ESP-IDF 6.1, from scratch in `native/targets/tdeck/build-batch28`: `openu5_tdeck.bin` = **0xd3a20** (866,848 bytes), +0x100 (256 bytes) against Batch 27's `0xd3920`: the new translation unit, `validate_world_objects` and the scratch `DungeonState`. `0x2c5e0` (181,728 bytes, 17 %) of the app partition is free; bootloader `0x5850`, 31 % free. **0 errors, 0 compiler warnings** (`batch28-firmware-build.log`, built before the commit; the five ESP-IDF `component_validation.cmake` notices are third-party). The `AlphaSaveScratch` PSRAM workspace grows by one `DungeonState` (about 600 bytes). The Launcher image is rebuilt (`idf.py reconfigure build`, `package_launcher.py`) **after** the Batch 28 commit so it embeds that commit; its path and SHA-256 are recorded in the annotated tag `alpha2-batch28-h166-save-validation`. **Not flashed.** SD card unchanged; no SD recopy.

### Status

H-166: **FIXED ON HOST** (`batch28_save_validation`). Classification: native defect (device persistence gate), not a 1988 behaviour and not a reference-port difference; robustness, reachable only from a corrupted card. **No new hardware phase:** everything H-166 decides happens in `alpha_save_generation.cpp`, which the host test runs as the device does, over the device's own sidecar bytes. The device-only parts (file I/O, temp+rename, CRC read-back, the PSRAM scratch) are unchanged, and the next flash exercises them in Phases 6T/6U/6V anyway. Staging a corrupt-but-CRC-valid generation on a real card would need a hand-edited file and a recomputed commit record: busywork that certifies nothing the host test does not.

Unchanged by this batch and still separate: H-118 (Phase 6T), H-115 (Phase 6U), H-164 (Phase 6V), all **HOST FIXED / DEVICE RETEST PENDING**.

**Still open:** H-154–H-157, H-160, H-165.

**Known and queued, do not file:** H-165 (a question left pending across a Developer teleport into a dungeon).

## Batch 29 — the device's rest services: H-154 bed NPC snap, H-155 "Thrown out of bed!", H-160 reclassified

Scope: H-154, H-155 and H-160 only. Not touched: H-156 (per-tick housekeeping in bed) and H-157 (the 05:00/20:00 tile refresh), both **reserved for Batch 30**; H-165; H-118 (Phase 6T), H-115 (Phase 6U), H-164 (Phase 6V); the TypeScript reference; the SD pack; the save format.

### Baseline

HEAD `f964df19` (tag `alpha2-batch28-h166-save-validation`), branch `main`, working tree clean. From-scratch build `native/core/build-b29-base`, serial: **98/98 PASS, 0 fail, 0 skipped** (`native/core/batch29-baseline-ctest.log`). GCC 16.2.0 and CMake 4.4.2 from w64devkit. One warning, the known w64devkit `stl_uninitialized.h` false positive.

### The 1988 bed hole-up, re-derived

Read from `original/u5/ultima5/*.OVL` with `re/tools/dis16.py`, not from the earlier notes. CMDS positive near calls are kernel calls too: `(target + 0xBF80) & 0xFFFF`.

| CMDS `0x0552`… | What it does |
|---|---|
| `0x055a` | "For how many hours? " (DS `0x4209`); one digit, `0` or space aborts |
| `0x059e-0x05b0` | target = `g_hour + n`, minus **23** if above 23 (the declared wake-hour divergence, `bugs-del-original.md` §1.3) |
| `0x05b4-0x05d4` | **up to 16 NPC passes** (`0xffffbb32` → stub `0x7ab2` → NPC.OVL `0x0db4`, then kernel `0x5910`); if `[0x65be] == 0x61` after a pass, **return without sleeping** (see H-169) |
| `0x05e6-0x0607` | every party `'G'` → `'S'` |
| `0x060d` | "Zzzzzzz...\n" (DS `0x421e`) |
| **loop** `0x0634` | `0x20FA` frame delay (not on the first entry) |
| `0x063b` | `g_hour == target` → leave |
| `0x0647` | `advance_clock(10)`, kernel `0x4F7C` |
| `0x064e-0x0664` | hour changed to 20 or 5 → `0xffffbb1a` (day/night tiles, **H-157**) |
| `0x066e` / `0x0671` / `0x0674` | kernel `0x4A84`; kernel **`0x2AE8` turn housekeeping (H-156)**; `0x2900` status panel |
| **`0x0677`** | `0xffffbb0e` → stub `0x7a8e` → **TOWN.OVL `0x1694`**, every tick |
| **`0x067a-0x0688`** | push `[0x5896]` x, `[0x5897]` y, `[0x5895]` g_floor; **kernel `0x368E` `find_object_at_xy`** |
| `0x068b` | zero → next tick; non-zero → `si = -1`, leave |
| `0x069d` | only when `si == -1`: **"Thrown out of bed!\n"** (DS `0x422a`) |
| `0x06a4-0x06e5` | both exits: `'S'` → `'G'`, restore `[0x587b]`, `inc [g_party_x]`, repaint flag, `inc [0x5c5c]` (slot 0's mirror of the same step), redraw |

`re/tools/callers_banda.py 0x7a8e` finds exactly one caller of the stub in every module, CMDS `0x0677` (the positive control: that is the site read above). Only the bed loop runs `0x1694` through the stub. The TypeScript comment that the inn shares it (`game.ts` `wakeSnapNpcs` docblock) is not supported by the census; the inn was not adjudicated here.

**H-154 — the snap.** `0x1694` runs once per 10-minute tick, inside the loop. It is one routine over the 32-slot actor table. `0x16a2-0x16b9` clears object-register slots 1..31 and every live `objIdx`; `0x16c9-0x171b` then walks slots 1..31 whose type byte (DS `0x659E`) is non-zero and, through `0x1726`, (a) places an object in the register **only if the slot's schedule z equals `g_floor`** (`0x176c`), and (b) for **every** such slot, on every floor, writes the period's X/Y/Z from the **live** schedule (DS `0x5D5E`, `+3/+6/+9`) into the live record (`0x1841-0x1853`), state 1 (`0x1856`), servedSlot = period (`0x1705`), pathIdx −1 (`0x170c`). The stuck counter (DS `0x65C2`) is not written. `0x1694` never reads the dead bitmap (DS `0x5B56`). A dead NPC is absent because its type byte is 0: the map loader despawns every slot `is_npc_dead` (TOWN `0x0000`) reports (`0x123c-0x124e` → `0x00B0`), and the kill path `0x09BC` marks the bit (`0x0052`) and despawns (`0x00B0`). A guard cleared by `0x00B0` without a dead bit is equally absent until the `.NPC` is re-read. The object half and the NPC half are one routine in 1988, but in the port they are separate services: the core already runs the object half from this hook (H-148, `hydrate_interior_objects`), and the device owns the NPC half.

**H-155 — the probe.** Kernel `0x368E` scans object-register slots 1..31 (slot 0 is the party's own record), compares `+2`/`+3`/`+4` with the three arguments (x, y, floor) and returns the matched slot's `+0` byte. The floor comparison is skipped only when `g_location > 0x7f` (`0x36c0`); the flags byte `+5` is not read. The cell tested is the **party's own cell** on the current floor. After `0x1694` the register holds that floor's NPCs **and** its objects, so both count. The probe runs after the tick's clock (`0x0647`), housekeeping (`0x0671`) and snap (`0x0677`). When it fires, the loop ends at once: the remaining hours are not slept, "Thrown out of bed!" prints, and the common epilogue still wakes everyone and steps the party one cell east (onto the RightBed; 264/264 LeftBeds have one there, `camp.ts` docblock). **Shipped data:** of the 32 locations, no `.NPC` object slot (types 1, 14, 27, 30, `0xB5`, `0xB6`) is ever authored onto a LeftBed. Many NPC periods are (Lord British's Castle alone: slots 1, 2, 5–14, 18–20, 26, 27, 29, 30). The object half of the probe is faithful but cannot fire on shipped data; every real "Thrown out of bed!" is an NPC coming to its bed.

**H-160 — the camp watch.** Outdoor camp is a different routine (CMDS `0x0000`). The guard walk (`0x0337-0x03e8`) starts with `cmp [bp+6], -1`: **with no guard posted, the whole walk is skipped and no random number is drawn.** With a guard: `rand(0,3)`, move only on 2; `rand(0,3)` for the direction; bounds 0..10 (kernel `0x6d82`, `0x038e`); then the free-cell test at `0x03a6`, stub `0x7d76` → overlay 7 `0x0000`. That test blocks on impassable terrain (the fire, tile 179), tile `0xFF`, an object-register occupant (some families excepted) or a live combat actor (the sleepers). TS `campCellFree` models the same set. The guard is chosen by kernel `0x3ea5-0x3eac`, "Who will stand guard? " (DS `0xa36e`), after "Wilt thou set a watch?". **On the device, (H)ole up asks only for hours** (`UiSession` `'h'` → `RestHours` → `Command::member = -1`), and `RestServices::guard_start` is unbound. So `camp()` always gets guard −1, the walk never runs, and `cell_free` has no caller on hardware. The Batch 21B row ("the watchman walks through the campfire") describes a watchman the device never posts. It is **reclassified**: the constant `cell_free` is unreachable wiring, not a player-visible defect, and the real gap is the missing watch (**H-167**, new). A faithful `cell_free` also needs the guard's index (the guard does not block itself, `campCellFree`), which `RestServices::cell_free(ctx, col, row)` does not carry. That is a core API change, which belongs with H-167.

### Side by side

| | DOS | TypeScript | native core | device before Batch 29 |
|---|---|---|---|---|
| per-tick NPC snap | `0x0677` → `0x1694`: reposition every live slot, every floor, live schedule, stuck kept | `snapNpcsToSchedule` → `wakeSnapNpcs` → `npcManager.enterMap`: a **rebuild** from the `.NPC` file (dead bits honoured) | `bed_sleep_step` calls `snap_npcs` every tick; the Rest arm adds `hydrate_interior_objects` | `snap_npcs = [](void*){}` |
| object half | same routine | not called from the bed (Batch 21B row) | `hydrate_interior_objects` every tick (H-148) | core-owned, ran |
| occupancy probe | `0x0688` → `0x368E` (x, y, g_floor), objects + NPCs | `objectOrNpcAt(x, y, floor)`: `worldObjects` + `npcManager.npcAt` | `bed_sleep_step` calls `occupied` after the snap, every tick | `occupied` → `false` |
| thrown out | message, loop ends, epilogue runs | same | same (`bed_sleep` breaks, `bed_sleep_end`) | unreachable |
| camp watch | "Wilt thou set a watch?" / "Who will stand guard?" | `campPrompt.ts`, `campGuardStartCell`, `campCellFree` | `camp(ctx, hours, member)`, `camp_guard_walk` | no prompt, `member = -1`, `guard_start` unbound, `cell_free` → `true` (never called) |

### Corrections to the Batch 21B text

- **H-160** is inaccurate as written. See above: reclassified to **UNREACHABLE ON DEVICE**, superseded by H-167.
- **H-154's "TypeScript: modelled"** is only half true. TS snaps on every tick, but by *rebuilding* the list from the `.NPC` file, not by 1988's reposition. That has three observable consequences, each pinned by a check below (mutation M2b = that shape): a guard cleared in a fight without a dead bit comes back (B2d); an alarmed guard goes to its file schedule instead of its live one (B2e); the stuck counter is reset (A4). **REFERENCE-PORT DIFFERENCE**, new row below. No parity fixture pins bed-snap NPC state (`item_parity` pins only the callback order and arguments), so native follows the binary here and the reference was not changed.
- The matrix row "`+5` byte from a per-location dword bitmask at DS `0x28C2`": DS `0x28C2` is a **static DATA.OVL table** (file offset `0x28D2`), one dword per location, indexed by `g_location`. It is **not** the dead bitmap (`0x5B56`). Non-zero at locations 4 (`0x00028000`), 5 (`0x00000002`), 28 (`0x000003F8`), 29 (`0x000001E0`), and at index 0 (`0x00000A3F`, unused by towns). What `+5 = 0xFF` means for those slots is still unresolved; it does not affect the probe (`+5` is not read).
- The matrix row "Whether the bed loop has any ambush/interruption roll beyond the occupancy probe": the per-tick call list is as recorded, but *before* the loop `0x05b4-0x05d4` runs up to 16 NPC passes that can cancel the sleep (**H-169**, new, not adjudicated).

### Root causes

- **H-154:** `AlphaRuntime::initialize()` bound `RestServices::snap_npcs` to `[](void*){}`. The core called it on every tick and got nothing. Native defect (device wiring); the core and the primitive (`snap_npcs_to_schedule`, Batch 24) were already right.
- **H-155:** `RestServices::occupied` was bound to `[](...){return false;}`. The core probed on every tick after the snap and was always told "empty". Native defect (device wiring). The device already had the right predicate, as `shop_services_.occupied` (actors + pool, current location/floor).
- **Why no test caught it:** the host fixture (`alpha_runtime_host_fixture.cpp`) re-declared the same three stubs lambda for lambda, and the core rest tests (`item_parity`, `batch21b_chest_reset`) bind their own recording callbacks. Nothing ran the device's binding.

### Fix (minimal)

| File | Change |
|---|---|
| `native/targets/tdeck/main/alpha_runtime.{h,cpp}` | new `bind_rest_services()`: the one place the device's `RestServices` are bound, called by `initialize()` (and by the host fixture). `snap_npcs` → `openu5::snap_npcs_to_schedule(actors_, location, hour)` for locations 1–32, the same primitive `ReloadEffect::SnapNpcs` uses for a floor change. `occupied` → new `object_or_npc_at(x, y, floor)`: `actors_` and `objects_` of the current location on that floor. `shop_services_.occupied` now calls the same helper with the current floor (identical behaviour), so there is one occupancy predicate. `cell_free` and `karma_record` unchanged |
| `native/targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp` | the fixture's copy of the rest stubs is replaced by a call to production's `bind_rest_services()` |
| `native/targets/tdeck/host_tests/batch29_rest_wiring_test.cpp`, `native/core/CMakeLists.txt` | new ctest `batch29_rest_wiring` |

The seam was moved first, with the three stubs carried over unchanged, so the RED run below measures production's binding with production's old behaviour. The object half is **not** run by the new callback. The core's Rest arm already runs `hydrate_interior_objects` from the same hook (H-148), and running it twice would double the pool work for nothing. Dead NPCs: `NpcActors` holds no dead or `0x00B0`-cleared slot (map entry filters the dead bitmap; kills erase the actor, but see H-168 for the cannon), and `snap_npcs_to_schedule` only moves what is there, exactly like `0x1694` over type-0 slots. The live schedule (`NpcActor::schedule`, rewritten by the alarm) is what it reads.

Not done, deliberately: H-156 and H-157 (Batch 30); no guard prompt, `guard_start` or `cell_free` change (H-160/H-167); no change to TypeScript or to any fixture.

### Tests — `batch29_rest_wiring` (26 checks, real `AlphaRuntime` + shipped pack)

Raw keys through `AlphaRuntime::handle()`: `e` to enter Lord British's Castle, `h`, a digit, `Enter`. The fixture binds production's `bind_rest_services()`. Expected NPC cells come from the test's own transcription of NPC.OVL `0x12E0` over the pack's `.NPC` tables, never from the port's `schedule_index`.

- **H154-C** C1: `context_.rest_services` is the runtime's own owner with all four callbacks bound. The proof that the tests reach production's callbacks is M1 below: mutating production turns them RED.
- **H154-A / H155-C** (quiet bed (9,7,0), 12:00 + 2 h; slot 1 displaced on floor 0, slot 5 on floor 1, both mid-walk with stuck 7): A1 the full 12 ticks to 14:00; **A2** slot 1 and **A3** slot 5 at their 14:00 cells, state 1, served = period, path −1; A4 stuck still 7; C2 no "Thrown out"; C3 everyone `'G'`, one step east.
- **H154-B1** (dead bit on slot 29 before entry): absent at entry, **absent after twelve snaps** (B1c).
- **H154-B2** ((A)ttack guard slot 2 at (13,28), walk off the arena; family `0x70`, so no dead bit): **slot 2 not resurrected** (B2d); **B2e** slot 1, whose live schedule the alarm rewrote (times 0, AI 7), snapped from the live schedule, not from the file.
- **H155-A** (slot 14's bed (12,10,0), 22:00 + 2 h; slot 14 is elsewhere at 22:00 and on the bed from 23:00): **A1** "Thrown out of bed!"; **A2** the sleep ended on the 23:00 tick; **A3** slot 14 is on the bed (snap before probe); A4 epilogue.
- **H155-B** (object occupant): a copy of the castle table whose first chest has period 0 = (9,7,0) from 13:00; rest from 12:50. **B1** the 13:00 tick's re-seed puts the chest on the bed and the probe throws. No shipped slot does this (see above). It is arranged because `0x368E` counts the object half too. The first RED run also recorded a device fact: an object already standing on the bed hides the `0xAB` tile, and (H)ole up then refuses with "Hole up- Only in bed!".
- **H160-R** (outdoor (H)ole up): the camp runs, and no watch prompt appears. This records today's device flow for H-167 and is expected to change with it.

**RED → GREEN.** Against the moved seam with the old stubs (production behaviour at `f964df19`): **19/26 GREEN, 7 RED** (`native/core/batch29-red.log`). RED: H154-A A2, A3; B2e; H155-A A1 (slept to 00:00), A2, A3 (slot 14 still at (9,22)); H155-B B1 (slept to 14:50 with the chest on the bed). The first RED run also exposed a test-setup error in H155-B (the chest was placed on the bed before (H)ole up, which refused); B was rearranged before any production change. After the fix: **26/26** (`batch29-green.log`).

### Mutation proof (`native/core/batch29-mutations.log`)

Driver as in Batch 28: one mutation of `alpha_runtime.cpp`, touch, rebuild through cmake by absolute path, run, restore byte for byte, touch. After the last restore: 26/26, and the file compares identical to the fixed source.

| | Mutation | Result | Killed by |
|---|---|---|---|
| M1 | `snap_npcs` back to a no-op | 20/26 | H154 A2 A3, B2e; H155-A A1 A2 A3 (no snap → slot 14 never reaches its bed) |
| M2a | snap → `.NPC` rebuild ignoring the dead bitmap | 22/26 | A4, **B1c**, **B2d**, B2e |
| M2b | snap → rebuild **with** the dead bitmap (the TS `wakeSnapNpcs` shape) | 23/26 | A4, **B2d**, B2e |
| M3 | `occupied` back to constant `false` | 23/26 | H155 A1 A2, B1 |
| M4a | occupancy blind to objects | 25/26 | H155 **B1** |
| M4b | occupancy blind to NPCs | 24/26 | H155 **A1 A2** |
| M5x | `cell_free` made fatal (`std::abort()`) | **26/26, survived** | nothing, by design: the device never calls it (H-160) |
| M6x | `occupied` over-constrained to constant `true` | 18/26 | **C2**, and every check that needs a full rest (A1–A3, B1b, B2c) |
| M7 | snap only the party's floor | 25/26 | A3 |

Every assertion that was already GREEN before the fix and guards new behaviour is killed by at least one mutation: A4 (M2a, M2b), C2 (M6x), B1c (M2a), B2d (M2a, M2b). C3/A4-of-H155 pin the unchanged epilogue. The prompt's M5 ("restore `cell_free` to constant true") and M6 ("over-constrain `cell_free`") do not apply as written: `cell_free` was never changed, and M5x shows it has no caller. M6x is the same over-constraint aimed at the predicate that does run.

### Regression

Targeted, in `native/core/build-b29` (`batch29-targeted-ctest.log`), serial, all PASS: `batch29_rest_wiring` 26/26, `batch21b_chest_reset` 38, `batch22_basement_objects` 23/23, `batch23_vault_parity` 45/45, `batch24_reload_parity` 47/47, `batch25_shard_ritual` 42/42, `batch26_dungeon_save` 34/34, `batch27_alt_load` 37/37, `batch28_save_validation` 51/51, `item_parity`, `command_parity`, `travel_parity`, `gameplay_integration`, `gameplay_parity`, `quest_parity`, `persistence_parity`, `alpha_runtime_integration_regression`. Every earlier runtime batch keeps its exact check count. H-148's chest reset (21B/22/23) is unchanged: the object half still runs once per tick from the core. Batch 24's floor/load/fight semantics are unchanged: the same primitive, a different caller. No parity fixture moved.

### Full regression suite

From-scratch build (`native/core/build-b29-final`), serial: **99/99, 0 fail, 0 skipped** (`native/core/batch29-final-ctest.log`): the prior 98 plus `batch29_rest_wiring`. One warning, the pre-existing w64devkit `stl_uninitialized.h` false positive; zero project warnings (`batch29-final-host-build.log`).

### Firmware

ESP-IDF 6.1, from scratch in `native/targets/tdeck/build-batch29`: `openu5_tdeck.bin` = **0xd3a70** (866,928 bytes), **+0x50 (80 bytes)** against Batch 28's `0xd3a20`: two real callbacks and the shared `object_or_npc_at()` in place of two empty lambdas. `0x2c590` (181,648 bytes, 17 %) of the app partition is free; bootloader `0x5850`, 31 % free. **0 errors, 0 compiler warnings** (`batch29-firmware-build.log`, built before the commit; the five ESP-IDF `component_validation.cmake` notices are third-party). The Launcher image is rebuilt (`idf.py reconfigure build`, `package_launcher.py`) **after** the Batch 29 commit so it embeds that commit; its path and SHA-256 are recorded in the annotated tag `alpha2-batch29-rest-wiring`. **Not flashed.** SD card unchanged; no SD recopy.

### Status

- **H-154: HOST FIXED / DEVICE RETEST PENDING** (Phase 6W). Native defect (device wiring). The bed snap is now 1988's reposition: live schedule, every floor, stuck kept, dead and `0x00B0`-cleared slots stay gone.
- **H-155: HOST FIXED / DEVICE RETEST PENDING** (Phase 6W). Native defect (device wiring). "Thrown out of bed!" fires on the tick an NPC (or, in principle, an object) lands on the party's cell.
- **H-160: RECLASSIFIED — UNREACHABLE ON DEVICE.** No change. The device posts no camp watch, so the guard walk and `cell_free` never run (M5x). The player-visible gap is H-167.
- **H-156, H-157: OPEN, reserved for Batch 30.** Per-tick housekeeping (`0x2AE8` at `0x0671`) and the 05:00/20:00 tile refresh (`0x0664`) are still absent from the bed loop in both ports. Note for Batch 30: in 1988 the tick that throws you out has already run its housekeeping (`0x0671` precedes `0x0677`/`0x0688`).
- **H-165: OPEN**, untouched.
- **H-118 (Phase 6T), H-115 (Phase 6U), H-164 (Phase 6V):** unchanged, **HOST FIXED / DEVICE RETEST PENDING**.

Hardware validation is warranted for H-154/H-155, but only as a smoke test. The code that decides both runs on host exactly as on the device (the runtime, the pack, the raw-key route). What only the device shows is the player's view: an NPC visibly in the bed, the message, the clock. Phase 6W is one short pass.

### Queued, not fixed (found during this batch's archaeology)

- **H-167 — the device camp never posts a watch.** 1988 asks "Wilt thou set a watch?" then "Who will stand guard? " (kernel `0x3ea5-0x3eac`, `select_party_member`; a member not in `'G'` gives "None posted!"). The guard is excluded from the partial heal (`0x0461`), walks the camp (`0x0337-0x03e8`, 1–2 draws per 5-minute step) and is drawn in the scene. The device asks only for hours (`Command::member = -1`, `guard_start` unbound), so every camp is an unwatched camp: everyone heals and **the RNG stream differs from a watched camp**. The TypeScript reference has the whole flow (`campPrompt.ts`, `campGuardStartCell`, `campCellFree`). Closing it needs the prompt, `guard_start` from the CampFire arena's south starts, and a `cell_free` that knows the guard's index (a `RestServices` signature change). Missing original behaviour, device only. Medium.
- **H-168 — an NPC killed by a ship's cannon keeps walking.** 1988's hit (CMDS `0x0d47-0x0d82`) clears the object (`0x3A74`), finds its slot (TOWN `0x011e`), marks it dead (`0x0052`) and despawns it (`0x00b0`). Both ports only set the dead bit and karma (`commands.cpp` cannon arm; `game.ts` "(1) Ocupante") and leave the actor in `NpcActors`/`NpcManager` until the next map entry. The bed snap now moves such an actor like any other, which is harmless because it was never removed. Both ports, low.
- **H-169 — the pre-sleep NPC passes are unmodelled.** Before "Zzzzzzz...", CMDS `0x05b4-0x05d4` runs up to 16 NPC passes at the current hour (NPC.OVL `0x0db4` + kernel `0x5910`) and returns without sleeping if a pass leaves `[0x65be] == 0x61`. Neither port does this. What sets `0x61` (an NPC reaching the party?) was **not adjudicated**. Both ports; severity unknown until it is.
- **TypeScript bed snap is a rebuild** (REFERENCE-PORT DIFFERENCE, see Corrections). To fix in the reference only with a `--check` control, if a fixture ever pins it.

### Phase 6W — Batch 29 bed rest · *firmware only; the SD card is unchanged*

Flash the Batch 29 firmware (the image named in the tag). No serial capture is needed. Close the Developer menu with nothing on screen before each (H)ole up (H-165).

1. `Alt+D` → **Time** → Hour **22**, Minute **0**. Then **Teleport** → Destination **Lord British's Castle**, Floor **0**, X **12**, Y **10**, Use default entrance **off** → **Teleport**. Close the menu. You stand on the left half of a bed.
2. Press `h`, `2`, `Enter`. **Expect** "Zzzzzzz...", then **"Thrown out of bed!"** with the clock at **23:00** (not 00:00). The party stands one cell east, on the right half of the bed, awake; **an NPC is now lying on the left half**, where you slept.
3. `Alt+D` → **Time** → Hour **12**, Minute **0**. **Teleport** → Lord British's Castle, Floor **0**, X **9**, Y **7**. Close the menu. Note two or three NPCs in view, then Pass until some have walked.
4. Press `h`, `2`, `Enter`. **Expect** "Zzzzzzz..." and **no** "Thrown out": the clock reaches **14:00**, the party steps one cell east, and the NPCs you watched are no longer where they had walked to. They have jumped to their 14:00 posts.

**Pass:** steps 2 and 4 as stated. **Fail** if step 2 sleeps to 00:00 with no message or leaves the bed empty, if step 4 throws you out, or if NPCs stand exactly where they were before the sleep.

**Known and queued, do not file:** sleeping costs no food, ticks no poison and does not regenerate (H-156, Batch 30); the drawbridge and lamps do not change when a sleep crosses 05:00/20:00 (H-157, Batch 30); outdoor (H)ole up never asks "Wilt thou set a watch?" (H-167); H-165.

## Batch 30 — H-156 / H-157, bed-sleep housekeeping and hour tiles

**Baseline.** `e1ee763a` and annotated `alpha2-batch29-rest-wiring`; clean tree, 99/99 host, Batch 29 launcher 866,928 bytes (`0xd3a70`), SHA-256 `73a199b3204578cfe7fc5fb6a0d5471bdc04a3cd6251f6875d5ae2532327fc2b`.

**1988 order, not inferred from the TypeScript port.** The preserved CMDS.OVL `0x0634-0x068d` walk in the Batch 29 audit gives: loop-target check (`0x063b`); `advance_clock(10)` (`0x0647`); on an hour change to **20 or 5**, call the TOWN.OVL `0x0170` tile transform (`0x064e-0x0664`); kernel `0x4A84`, then `kernel_turn_housekeeping` `0x2AE8` (`0x0671`), status panel (`0x0674`); TOWN `0x1694` NPC/object snap (`0x0677`); occupancy probe (`0x0688`); ejection or next tick. `re/notes/kernel-survival.md` §2 reads the `0x2AE8` body: poisoned awake members lose 1 HP; sleepers and dead members do not eat; an hour change with zero food announces starvation and damages the party, otherwise 06/12/18 consume food for eaters; the saturating turn counter increments; finite Q/T spell turns decrement and clear the effect at zero; regeneration rings roll last. TOWN `0x0170` is the previously decoded gate/lamp/drawbridge transform (`re/notes/aud-relojes-acta.md`, `re/notes/sueltos-b-174-acta.md`). Thus the ejecting tick has *already* run the refresh when applicable and housekeeping, and no further tick follows. Kernel `0x4A84` and the status repaint are outside these two issues; their full presentation effects were not re-derived here.

**Native defect and fix.** Before Batch 30, `bed_sleep_step` advanced the clock and went straight to the snap/probe; the TypeScript reference omits both calls too. `rest.cpp` now uses the existing `WorldTerrain::refresh` only on the two changed-hour boundaries and the existing `turn_housekeeping` once after that, before the snap/probe. `RestContext` carries the command's terrain/world owners; `commands.cpp` passes them to the bed path. The housekeeping result is sent through the same PoisonTick/message event kinds as a normal town turn. No survival logic or terrain transform was copied into rest. `bed_sleep_begin`/`end`, outdoor camp, the snap/probe and UI were unchanged.

**RED → GREEN.** `batch30_sleep_parity` uses the shipped resource pack, real `AlphaRuntime`, raw `h`/digit/Enter keys, actual castle bed and lamp cells. Against unchanged Batch 29 production, 13/22 checks passed and 9 failed (`native/core/batch30-red.log`): six-tick turn/poison count, Q expiry, ejecting-tick housekeeping, starvation, and both boundary tile states failed. Two added meal checks then joined the final 24-check suite. With the production fix, 24/24 pass (`batch30-green.log`). The terrain observer reads the **effective shipped-map tile at the per-tick NPC-snap seam**; merely checking the final tile would be misleading because `AlphaRuntime::synchronize_after_debug()` refreshes it after the input completes. A marker inserted after the first ordinary tick proves no per-tick refresh. The 05:00 and 20:00 cases each show the changed tile on the *first* tick at that hour, before snap. The Q case pins expiry after two housekeeping calls and poison on all six; the ejecting case pins exactly one housekeeping call.

**Mutation proof.** `batch30-mutations.log`: six of six killed — skip housekeeping (7 RED); double it (5); skip only the ejecting tick (1); omit 05:00 (1); omit 20:00 (1); refresh every tick (1). Production restored, 24/24 GREEN. The older TypeScript-derived `item_parity` bed rows cannot remain byte-for-byte oracles for the two omitted 1988 calls. Its **304 affected bed rows** now assert a native tick/turn/early-ejection invariant (with the Q-duration mismatch explicitly separated as H-170); the other **38,424** rows retain byte-for-byte comparison, while `batch30_sleep_parity` supplies the original-derived state checks for this batch. The older chest-reset fixture now supplies food and HP so starvation does not turn a chest test into a survival scenario. Neither change weakens the new assertions.

**Regression and remaining verification.** Fresh host build with the Batch 29 Release/developer-tools settings: **100/100 PASS** (one new CTest, `batch30-final-ctest.log`); relevant focused/parity subset **6/6 PASS** (`batch30-targeted-ctest.log`). The firmware is built separately; no device was flashed and no SD card was touched. H-156/H-157 are **HOST FIXED / DEVICE RETEST PENDING**, Phase **6X**; Phase 6W was already reserved for Batch 29. Host tile ordering is stronger than a post-sleep device screenshot, because the runtime refreshes after input. Physical 6X is therefore a smoke check of presentation and survival state, with the in-loop order established on host.

**Queued, out of scope — H-170.** CMDS `0x063b` stops when `g_hour` reaches the target hour, whereas native `bed_sleep` runs `hours * 6` ticks. Q can halve a clock advance and expire partway through the sleep, so native may end before the original target hour (the Batch 30 Q fixture ends at 12:50 after six ticks from 12:00). The TypeScript path has the same fixed-count shape. This is separate from the required per-tick housekeeping and was left unchanged. H-165 and all other queued rows remain untouched.

**Boundary of this proof.** The original `0x588B` counter saturates at 255 (`kernel-survival.md` §2), while the existing native `turns_since_start` is wider and `turn_housekeeping` increments it without that cap. Batch 30 proves one call per sleep tick from ordinary low counter values and reuses the normal-turn implementation. Whether the wider native representation is an intentional state-model translation or a distinct preservation defect requires separate adjudication; this batch did not change that shared routine.

**Firmware build.** ESP-IDF 6.1, clean `build-batch30`: `openu5_tdeck.bin` **867,120 bytes (`0xd3b30`)**, +192 bytes (`0xc0`) against Batch 29; **`0x2c4d0` bytes** remain in the 1 MiB app partition. Build completed with no compiler warnings or errors (`native/targets/tdeck/batch30-firmware-build.log`). The final Launcher package is made after the source/evidence commit so its embedded firmware commit ID matches the Batch 30 commit; its hash is recorded in the annotated tag. No flash or SD write.

## Batch 31 — H-165 pending town-exit question across Developer dungeon teleport

**Baseline and exact question.** Batch 30 HEAD `bff6608e3bfc986c7f270638fb7daa0fb40c1005`, annotated tag `alpha2-batch30-sleep-housekeeping-refresh`, clean tree, Launcher 867,120 bytes (`0xd3b30`), SHA-256 `2941eaf9fc0619f8a187c1f1f7fea5755580a76dfd2b29ce5b294d7f22d7cc3f`. Batch 30 reported 100/100 host PASS. H-165 is the Batch 25 queued question at line 5081: a pending **"Leave this place?"** survives `Alt+D`, then Developer teleports *into* a dungeon. The modal returns, but `Exit`/`DeclineExit` is refused by the dungeon context gate, leaving `awaiting_exit` latched. It was only code-read in Batch 25 because that fixture supplied no dungeon data. It is not a general dungeon-loader or save/resume question.

**1988 call path.** `TOWN.OVL:0x0798` prints the exit question and `0x07ac–0x07b4` loops on `getkey` until Y/N/Esc (Batch 25 source walk, `batch25_shard_ritual_test.cpp`). The main command dispatcher cannot receive `(E)nter` while this loop waits. `re/notes/dungeon-map-buffers.md` §2.1 closes the ordinary dungeon-entry call chain: kernel `0x3178` dispatches E to `MAINOUT.OVL:0x08de` → `0x0790`; `0x086d–0x0887` reads that dungeon's **512 bytes** from `DUNGEON.DAT` into `g_dng_map` and sets `g_location` `0x21..0x28`; `0x088f–0x08c9` selects floor, position and facing. `DUNGEON.OVL:0x0e2e` then enters the session and `DNGLOOK:0x093a` applies cleared-room markers. The same note's §2.4 shows `INTRO:0x0eb4` restores the saved 512-byte map and `ULTIMA.EXE:0x00db/0x010b` resumes the dungeon without another file read; level changes (`DUNGEON:0x1c6a`) also do not reload it. The original has **no Developer menu** and therefore no transition from an unanswered town prompt into a dungeon. The preservation invariant proven here is that the original blocking question must be answered before an ordinary dungeon-entry command can run. Refusing the native-only teleport until then is an explicit Developer policy, not an invented 1988 teleport rule.

**Native pre-Batch-31 path and defect.** `UiSession::open_debug_menu()` parks the yes/no modal (Batch 25), and the Developer Teleport row calls `apply_debug_teleport()`. Its Dungeon arm invokes the real `EnterDungeon` command, which calls `dungeon_load()` and emits `DungeonEntered`; `synchronize_after_debug()` rebinds the runtime to a live dungeon. The modal remains parked. On closing Developer it returns, but the `Exit`/`DeclineExit` command falls through `commands.cpp`'s `c.dungeon && !dungeon_camp` gate and is refused before `awaiting_exit` can be cleared. This is a real native-only defect, not a failure of `dungeon_load()` or H-115 save restoration.

On a new dungeon entry, native `dungeon_load()` copies the 512 authored cells, selects entrance floor/cell/facing, clears reveal data and the quickness toggle, resets and respawns its wanderer, and applies cleared-room bits. A same-dungeon Developer relocation changes only floor/x/y and preserves the live cell mutations and facing (Batch 21A.1). A saved dungeon session is restored through H-115's sidecar path, without calling `dungeon_load()` or losing its live cell mutations. H-165's refusal happens before all of these transition choices, so it neither resets dungeon data nor changes save/resume behavior.

**Adequate fixture and RED.** `batch31_h165_dungeon_entry` attaches the shipped pack's `AlphaResourceOwners::dungeons` and `report.dungeon_count` to the existing real `AlphaRuntime` host fixture. The prior Batch 25 test omitted those two fields, so its Certification could not activate a dungeon. No dungeon bytes or expected state are hand-authored: `alpha_resources.cpp` reads each `dungeons.bin` record (from `DUNGEON.DAT`) into `DungeonData`, and the test uses the actual Developer menu with raw keys and the real command/loader. On unchanged Batch 30 production, the focused run was **5/8 GREEN, 3 RED** (`native/core/batch31-red.log`): teleport wrongly activated Deceit, mutated the session, and the returned N answer did not clear `awaiting_exit`. The log records `DEBUG_TELEPORT ... result=Applied ... dungeon_after=active1` while that latch was pending.

**Smallest fix and GREEN.** `apply_debug_teleport()` now returns `PendingQuestion` only for a Dungeon destination while `awaiting_exit` is true, before calling `EnterDungeon`. The status is labelled **"Answer pending question"** in the existing Developer result row. The prompt, world location, RNG, turn and dungeon session remain untouched. After N clears the question, the same menu entry succeeds. The final focused suite is **11/11 GREEN** (`batch31-green.log`): it compares the resulting Deceit id/floor/cell/facing and all 512 authentic cells with ordinary `(E)nter`, confirms dungeon controls, and keeps a pending-question small-map teleport as a Batch 25 policy control. H-115 dungeon save/load code was not touched.

**Mutation proof.** Two production-source mutations were built, run and reverted. M1 disabled the new guard: **8/11 GREEN**, H165-2/3/5 RED (`batch31-mutation-m1.log`). M2 guarded *all* Developer destinations instead of only Dungeon: **10/11 GREEN**, H165-11 RED (`batch31-mutation-m2.log`). The restored production source again passes **11/11**. Mutation executables are build-directory artifacts only.

**Regression and hardware boundary.** The fresh targeted run passed **11/11 CTest targets**: H-165, debug map picker, gameplay integration, dungeon flow/parity/combat, travel parity, and Batches 25–28 prompt/save regressions (`batch31-targeted-ctest.log`). The host path uses the same runtime, pack, menu and raw-key dispatcher as the device, so H-165 needs no new physical observation or checklist phase. Existing Phases 6T/6U/6V/6W/6X remain pending independently. H-167–H-170 remain out of scope; no new unrelated defect was adjudicated.

**Full host regression.** Fresh clean-first **Release** build with developer tools on (`build-batch31-final`), matching Batch 30's build settings: **101/101 PASS**, 0 fail, 0 skipped (`native/core/batch31-final-ctest.log`), one new CTest over Batch 30's 100. The sandboxed Windows Node 24 `os.userInfo()` call failed before any TypeScript-based test could start; the final run used a build-directory-only `NODE_OPTIONS` preload that supplies the existing `USERNAME`/`USERPROFILE` only when that OS lookup throws. It did not change repository source, fixtures, or test expectations. All 24 affected Node-based tests then passed along with the 77 native tests.

**Firmware.** Clean ESP-IDF 6.1 T-Deck build (`native/targets/tdeck/build-batch31`) completed with `openu5_tdeck.bin` **867,184 bytes (`0xd3b70`)**, **+64 bytes** against Batch 30 and `0x2c490` bytes free in the 1 MiB app partition (`batch31-firmware-build.log`). The installed cross-compiler could not launch under the Windows filesystem sandbox (access denied), so the build ran with the toolchain's required access; no flash command was run. The Launcher is repackaged after the Batch 31 commit so its embedded commit id is current. The SD pack and save format are unchanged.

## Batch 32 — H-167 camp watch flow and narrow core rest API

**Baseline and exact question.** HEAD and the dereferenced annotated `alpha2-batch31-h165-dungeon-entry` tag were both `6e3da69150e3bd4c9ba519107549a12cdb421cf4`; the working tree was clean. The Batch 31 Launcher was 867,184 bytes with SHA-256 `e9a43529ff7040ad7e029fc6ca2d9699e4d99c6159848ff89da14a9013bc911a`. The 101-target host suite reran **101/101 PASS** using the same build-directory-only Node `os.userInfo()` fallback required by Batch 31's sandbox. H-167 is audit row D-24's *missing optional watch after outdoor (H)ole up hours*: the device always supplied guard −1, had no `guard_start`, and its `cell_free` callback could not identify the guard. It is distinct from H-160's previously imagined walking watchman and from the bed-sleep rows H-156/H-157.

**Original call path (1988 bytes, not the reference port).** The kernel Camp route `ULTIMA.EXE:0x3c9a-0x3eef` performs the location/transport checks and asks `"For how many hours? (1-9)"` at `0x3da6`. Zero/Space exits without sleeping. `0x3e06-0x3e24` counts party members whose status is `G` or `P`; only a count of at least two reaches `0x3e2c`'s `"Wilt thou set a watch?"`. N goes straight to the unwatched sleep. Y prints `"Who will stand guard?"` (`0x3ea5`) and calls the shared `select_party_member` (`0x3eac`). The **single** chosen index is accepted only if its roster status is `G` (`0x3ec6`); a `P`/`S`/`D` choice or picker escape becomes guard −1 and prints `"None posted!"` (`0x3ecd`) **without retry or abort**. The guard is a transient camp argument (`[bp-6]`, then CMDS `[bp+6]`), not save state. With fewer than two `G`/`P` members no watch question appears. A no-watch choice does not consume watch RNG.

The selected guard's formation cell comes from the **authored CampFire arena's south starts**, copied by kernel `0x60ec` and read by `0x6936`; this is not the static default table at DATA.OVL `0x1724`. `CMDS.OVL:0x0337-0x03e8` skips the entire walk for guard −1. Otherwise every five-minute loop opportunity rolls `rand(0,3)`; a 2 causes a direction draw, followed by bounds/passability/occupied-cell checks. The fire tile and living sleepers block movement; the guard does not block itself. At the wake helper `0x0461`, the guard is excluded from the partial HP/MP pass; on apparition the later live-member full heal is independent of watch. The existing one-per-crossed-hour ambush can interrupt before wake and therefore before partial healing. Original scene rendering is separate from the core guard cell and remains outside this H-167 API change.

| Path | Time and shared work | Distinct outcome |
|---|---|---|
| Normal command turn | `advance_clock` then kernel turn housekeeping/world path | Ordinary movement/encounter state |
| Bed sleep (`CMDS:0x0552`) | Ten-minute ticks; Batch 30 calls day/night refresh and housekeeping before NPC snap/bed occupant probe | `G` sleepers become `S`, can be thrown out, then wake one cell east; no watch picker |
| Camp (`kernel:0x3c9a` → `CMDS:0x0000`) | Five-minute steps to the target hour; arena/guard path and per-hour ambush; partial heal/apparition only after uninterrupted sleep | Optional watch, no bed ejection or bed NPC snap; guard walks and is excluded from partial heal |
| Camp watch transition | Hours → optional Y/N → one roster pick or cancel → Camp | Prompt and pick consume no game time or RNG; the resulting guard controls walk draws and heal exclusion |

**Core API and production change.** `RestServices::cell_free` now takes `(context, guard, col, row)`, the smallest missing identity needed to ignore the guard's own cell while rejecting sleeper cells. `camp_watch_offer` reuses `camp_context` and `camp_watch_count` to publish the original ≥2 gate without duplicating eligibility in the T-Deck UI. `camp_guard_choice` applies the kernel's `G` check only for a real watch choice (`Command::watch_requested`); low-level callers that already pass a resolved guard keep their existing `Rest` contract, as the item parity fixture requires. `UiSession` carries the hours through existing YesNo and PartySelection modes, dispatches one Rest after the response, and clears its pending hours on every completion path. `AlphaRuntime::bind_rest_services` reads the shipped arena's south starts and tests arena passability and sleeper occupancy. The existing `camp_sleep_step`, heal, encounter and clock routines are reused. Bed sleep, dungeon save/resume and pending-question code were not changed.

**RED, GREEN and RNG proof.** A new shipped-pack `batch32_camp_watch` CTest drives real `AlphaRuntime::handle()` raw keys. Before production edits it logged **5/17 GREEN, 12 RED** (`native/core/batch32-red.log`): hours immediately completed Camp, so the watch question, picker, cancel and guard effects were absent. The final test is **35/35 GREEN** (`batch32-final-focused.log`), including the original camp heading/hours text, zero/Space/Escape hours cancellation, G+P count, invalid P choice, picker escape, N, one-member bypass, return to Exploration, watcher heal exclusion, CampFire south cell, sleeper/fire collisions and one completion. Seeded `OriginalRng(1)` is replayed against every production draw trace; prompt/picker consume none, watched sleep has per-step `rand(0,3)` draws before heal/gate, and declining watch has none. This detects RNG insertion or reordering on the watch path while preserving the established live stream. The old Batch 29 H-160 observation was updated to assert the offered prompt and N's unwatched route.

**Mutation proof.** Six production-source mutations were built and reverted; all were killed by the committed test: bypass watch offer (15 RED), accept poisoned watcher (2), skip guard walk (2), use east instead of south formation (1), suppress "None posted!" (2), and run wake twice (2). The six raw logs and build logs are `native/core/batch32-mutation-*.log` / `*-build.log`; `batch32-post-mutation-green.log` proves the restored source. No mutation-only production change or throwaway script is committed.

**Regression and device boundary.** From the fresh Release build, the targeted rest/gameplay/combat set passed **9/9 CTests**, input/combat set **7/7**, and item parity plus rest set **4/4**. The final host suite passed **102/102**, zero fail or skip (Batch 31: 101/101). The clean ESP-IDF 6.1 build, incrementally rebuilt after the final prompt edit, produced **869,024 bytes (`0xd42a0`)**, **+1,840** against Batch 31, with `0x2bd60` free in the 1 MiB app partition (`native/targets/tdeck/batch32-firmware-build.log`). The final commit-stamped Launcher SHA-256 is in the annotated Batch 32 tag. No flash or SD repack was performed. The next unused physical label is **Phase 6Y**, assigned in `ALPHA2_HARDWARE_CHECKLIST.md` to observe the watch prompt, picker, cancel and return on the handheld; 6T/6U/6V/6W/6X remain pending independently.

**Separate queued finding.** Tracing the full camp loop exposed **H-171 / D-27**, not a reason to widen H-167. Batch 36 corrected this queued interpretation from the original bytes: `0x5910` is viewport redraw (including a gated wind roll), `0x2900` draws status, and `0x20fa(1)` delays one timer tick. The actual H-171 gameplay gaps were the missing redraw wind state/RNG, missing Q/T clear on accepted Camp entry, and the encounter roll placed before rather than after the next-hour redraw/ring. Camp does not call `0x2ae8` turn housekeeping. See the Batch 36 section and raw disassembly log. H-168, H-169 and H-170 were not changed.

## Batch 33 — H-168 cannon-killed NPC lifecycle

**Baseline and exact question.** HEAD `c6a5ae14aa5c3f7d0d6ec2b701b84d9c20c256f0`, annotated `alpha2-batch32-camp-watch`, clean tree, and the Batch 32 Launcher (869,024 bytes; SHA-256 `5bf7adb9a93f6802e8496f3a6bd37251963a94b05e33ac71bbaa5ad718dc1608`) matched the requested exit state. The unchanged 102-target host suite passed 102/102 with Batch 32's build-directory-only Node `os.userInfo()` shim. An initial unshimmed run failed 24 Node tests before they began with the same sandbox `uv_os_get_passwd` error; its raw log is retained. H-168/D-25 asks why a cannon-killed NPC kept participating in local movement, collision and targeting until map re-entry.

**1988 call path, corrected.** CMDS.OVL `0x0B16` scans adjacent cannon tiles `0xB4..0xB7`, follows the cannon's facing up to four impact cells, and calls kernel `0x7782` for an occupant. On an NPC hit, `0x0D47-0x0D82` calls kernel `0x7AF4` = `set_actor_record` (`0x3A74`) with zero fields to clear the struck object record, then TOWN `0x011e` resolves its NPC slot, `0x0052` sets the dead bitmap only for eligible type families, and `0x00B0` clears the live slot. The latter writes type byte zero and clears live/schedule/object fields; NPC.OVL `0x0DB4` and the bed snap `0x1694` only walk nonzero-type slots. The 32-slot table remains allocated; this is an inactive slot, not a new corpse. Neither the visible cannon branch nor `0x7AF4` computes HP damage: the earlier `re/notes/cannon-fire.md` identification of `0x7AF4` as an opaque damage roll was wrong and is corrected. An eligible person stays dead when the `.NPC` data is reread; a type `0x70` guard lacks a persistent bit, yet is still absent immediately and may return only on re-entry. The original display/object clear and logical slot clear are distinct operations.

**Native and reference defects; fix.** Before Batch 33, native `commands.cpp` set `npc_dead` and karma but kept the `NpcActor` in `NpcActors`. The ordinary `tick_npcs` pass, `npc_occupied`, later cannon targeting, and `snap_npcs_to_schedule` could still reach it. Map entry's `enter_npc_map` filtered the dead bit and hid this failure for a person. Native now copies the struck actor and calls the already-used `dialogue_despawn`, which applies the original type gate and removes the live actor immediately. No map-reload, ordinary movement, combat-death, or renderer code changed. The TypeScript cannon branch had the same H-168 omission. Its scoped parity correction calls its existing `townNpcDeadBitSet` and `NpcManager.clearSlot`; no fixture expectations were edited.

**RED/GREEN and gameplay observables.** The new `batch33_cannon_npc_death` CTest links production `AlphaRuntime`, dispatches raw `F`/trackball/Confirm input, and loads the shipped resource pack. The main witness uses Ararat's authored north cannon at `(12,11)` and authored NPC slot 1 (type `0x40`), placed in its first impact cell as if it walked there. The final RED against unchanged Batch 32 cannon code is **12/19**, seven failures (`native/core/batch33-red.log`): after death the actor still occupies/blocks the cell, a second shot charges karma again, a normal Pass leaves it in the update list, and the bed schedule update actually moves the dead slot to `(15,14)`. Map re-entry then masks it. The corrected path is **19/19 GREEN** (`batch33-final-focused.log`): immediate removal, no second target or world corpse, no later movement, and reload preserves the person-dead state. An empty cannon line leaves the ordinary actor and karma intact. A second shipped scenario uses Lord British's Castle floor-2 south cannon `(15,26)` and authored guard slot 27 (type `0x70`): it despawns immediately without a dead bit, and returns on map re-entry. The focused TypeScript cannon tests are **17/17**.

**Mutation proof.** Five temporary production mutations were built, run and restored. All were killed: old dead-bit-only behavior (seven RED on the final 19-check test); remove actor without the dead bit (K1/K6); kill an actor on a miss (M2/M3); create a world corpse object (K3b); and persist the type-`0x70` guard dead bit (G4/G5). Raw test and build logs are `native/core/batch33-mutation-*.log` / `*-build.log`; `batch33-post-mutation-final-build.log` and the final focused log prove restored source. No mutation source or throwaway program is committed.

**Regression and parity.** The focused cannon/movement/reload/combat subset passed **18/18 CTests** (`batch33-targeted-ctest.log`). The first fresh full suite exposed one real case-4671 H-168 difference: native's `npcs=[]` versus the old TypeScript live slot; the other 102 targets passed. After the scoped TypeScript fix, `gameplay_parity` passed 1/1 and the final fresh Release/developer-tools host suite passed **103/103**, zero failures or skips (`batch33-final-ctest.log`), one new CTest over Batch 32. No other fixture or death path was changed.

**Firmware and hardware.** Clean ESP-IDF 6.1 T-Deck build: `openu5_tdeck.bin` **869,008 bytes (`0xd4290`)**, **−16 bytes** from Batch 32, with `0x2bd70` bytes free in the 1 MiB app partition (`native/targets/tdeck/batch33-firmware-build.log`). No project compiler warnings or errors; five existing ESP-IDF component-validation notices. The commit-stamped Launcher is packaged after commit and its SHA-256 is recorded in the annotated Batch 33 tag. No hardware was flashed or checked, and the SD resource pack and save format did not change. This core lifecycle is exercised by the same production raw-input/runtime path on host, so H-168 needs no new physical phase. Pending 6T, 6U, 6V, 6W, 6X and 6Y retain their assignments. H-169, H-170 and H-171 remain queued and untouched; no separate H-number was opened.

## Batch 34 — H-169 pre-sleep NPC loop parity

**Baseline.** HEAD `e7f4b27acb3f75a507bbb696867374e0bcda1bdd`, annotated `alpha2-batch33-cannon-npc-death`, clean tree. The Batch 33 Launcher was 869,008 bytes (`0xd4290`) with SHA-256 `303823aed8067d09e1559e18e9659e5b61d78c19947b2843547d71bb2068d380`. Its built host suite passed 103/103 (`native/core/batch34-baseline-ctest.log`).

**1988 call path and ordering.** The local-bed dispatcher first requires tile `0xAB`; CMDS.OVL `0x0552` asks for one hour digit, with 0/Space leaving before any NPC pass. `0x059e-0x05b0` chooses the target hour. `0x05b4-0x05d4` then repeats up to **16 full** NPC.OVL `0x0db4` passes with the unchanged current `g_hour` argument. Each pass first clears marker `[0x65be]` in its prologue (`0x0dc1`), processes live slots in ascending index order, then CMDS calls kernel `0x5910` viewport redraw and tests the resulting marker. NPC.OVL `0x06e4:0x0723-0x07be` sets marker `0x61` (`'a'`) for an orthogonally adjacent AI type 6/7 actor before that actor's movement/RNG; the pass finishes, redraws, and CMDS returns immediately only if the final marker is `0x61`. An adjacent AI 4/5 NPC with a dialogue number writes `0x74` (`'t'`); because slots run in ascending order, a later talker can overwrite an earlier attack marker, while a later hostile can overwrite talk. A mover that becomes adjacent in pass N can therefore cancel on pass N+1 if the final marker remains `0x61`. Eligible movement branches can draw RNG; an idle/ineligible slot need not. There is no clock, terrain refresh, turn housekeeping, or bed occupancy probe in these passes. Movement can change the live bed-cell occupancy, but the occupancy/ejection decision belongs to the later per-tick snap and probe, not this loop.

If all 16 passes complete, CMDS `0x05e6-0x0607` changes party status `G` to `S`, `0x060a` refreshes status, `0x060d` prints `Zzzzzzz...`, `0x0614-0x0624` performs the bed-entry viewport fill, then `0x063b` checks the target hour. The first ten-minute advance is only at `0x0647`; a 05:00/20:00 crossing refreshes terrain (`0x0664`); kernel turn housekeeping follows at `0x0671`, then status refresh (`0x0674`), the distinct per-tick NPC/object **snap** (`0x0677` → TOWN `0x1694`), and the bed-cell occupancy probe (`0x0688`). An occupied cell ends the tick loop with "Thrown out of bed!"; both exits wake status `S` to `G` and step the party east. Camp has its own CMDS `0x0000` five-minute loop and no call to this pre-sleep site. Ordinary town command turns call NPC.OVL `0x0db4` through TOWN `0x166e`, once and under a separate result gate. The pre-sleep pass is neither those normal turns nor the per-tick bed snap.

**Defect and fix.** Batch 33 native called `bed_sleep` directly after accepted Rest, so no pre-sleep NPC update, redraw, or hostile cancellation occurred. `commands.cpp` now runs the existing full `tick_npcs` 16 times at bed entry, with the original redraw-after-pass and abort gate. A small optional output from the existing NPC pass reports the original shared action marker at the AI fast path, preserving later-slot overwrite and the no-movement return on an armed marker. No normal command turn, time advance, housekeeping or terrain refresh was added. Camp is gated out. `bed_sleep_begin` now changes status to `S` before the message, matching `0x05e6` before `0x060d`. A Batch 29 snap test's fabricated odd `path_index=3` was corrected to valid `2` so the newly reached movement path can process that fixture (`batch34-legacy-fixture-red.log` preserves its six-failure witness).

**Executable evidence.** New `batch34_pre_sleep_npc` drives production `AlphaRuntime` through raw Enter/H/1/Enter keys and the shipped resource pack. Iolo's Hut has a real LeftBed at (12,14) and authored ground-floor AI-6 slot 1: placed adjacent as a live walked position, the unchanged Batch 33 path gave **3/6 GREEN, 3 RED** (`batch34-red.log`) by sleeping six ticks instead of cancelling. GREEN is **21/21** (`batch34-final-focused.log`): immediate cancellation after one pass; a hostile that moves into adjacency cancels after two; a synthetic same-floor placement of two authored Yew records proves the later adjacent talker overwrites an earlier hostile marker for all 16 passes (moving it away restores one-pass cancellation); this control does not claim the basement hostile naturally reaches that floor; an authored castle NPC's in-progress corridor step appears on the *first* pre-sleep redraw; uninterrupted sleep gives exactly 16 redraws before `Zzz`, no early clock/housekeeping, then the six existing bed ticks and east-step exit. The seeded castle roster records 74 pre-sleep movement RNG draws, first range `[0,255]`; this checks the call count and slot-order consumption. Camp and zero-hour cancel show no pre-sleep redraw.

**Mutation and regression.** Eight temporary production mutations were built, tested and restored against the final 21-check fixture: omit all passes (11 focused failures), one pass (five), seventeen passes (three), redraw before movement (two), skip movement (nine), ignore hostile marker (six), apply passes to Camp (one), and suppress the later talk-marker overwrite (one). Raw logs: `native/core/batch34-mutation-*.log` and `*-build.log`; restored build: `batch34-post-mutation-build.log`. The bed/Camp/NPC/movement/command/turn regression subset passed **16/16** (`batch34-regression-ctest.log`), including Batch 29's corrected valid-state fixture. The fresh Release/developer-tools full host build passed **104/104**, zero fail or skip (`batch34-final-ctest.log`).

**Presentation boundary and remaining work.** The pre-sleep NPC loop itself writes actor movement and the action marker; it does not write a party/NPC sprite or map tile. The party's `G`→`S` state follows the loop and is now ordered before `Zzz`. The distinct `0x0614-0x0624` set-color/fill-rectangle calls clear the original viewport during bed entry; native has no equivalent scene-specific fill. This is queued separately as **H-172**, with no Batch 34 rendering change. H-170 (target-hour mismatch) and H-171 (Camp housekeeping) remain queued and untouched. No hardware was flashed; 6T, 6U, 6V, 6W, 6X and 6Y remain pending, and H-169 needs no new device phase because the real raw-input/runtime path is covered on host.

**Firmware.** Clean ESP-IDF 6.1 T-Deck build: `openu5_tdeck.bin` **869,360 bytes (`0xd43f0`)**, **+352 bytes** against Batch 33; `0x2bc10` bytes free in the 1 MiB app partition (`native/targets/tdeck/batch34-firmware-build.log` clean build; `batch34-firmware-final-build.log` final incremental build). Launcher packaging is performed after the Batch 34 commit so its embedded revision and SHA-256 match the annotated tag. The SD pack and save format are unchanged.

## Batch 35 — H-170 Q sleep target-hour parity

**Baseline.** HEAD `8839caa18c106e2c44f52665864200fa681cc5c7`, annotated `alpha2-batch34-pre-sleep-npc-loop`, clean tree. The Batch 34 Launcher is 869,360 bytes (`0xd43f0`) and SHA-256 `7c4a06b30ee2cb40b3602621c0ab2a29922a0587d009b7ebb8bb10ea06875b73`; its untouched host build reran 104/104 PASS.

**Original representation and mechanism.** Q is Rel Tym Quickness, not a sleep deadline. CAST2.OVL `0x08f8` writes `'Q'` to `g_time_spell` (DS `0x587a`) and 30 turns to `g_time_spell_turns` (DS `0x588e`); the native `TimeStatus` spell metadata likewise writes `Q/30`. Kernel `0x4f8d` halves each requested clock advance with a minimum of one minute; kernel turn housekeeping `0x2bae-0x2bc2` decrements the separate counter and clears the status at zero on ordinary turns and on every bed tick. The bed target is instead a **16-bit local hour** `[bp-6]`: shipped CMDS.OVL bytes at `0x059e-0x05b0` add the one-digit requested hours to the current hour and subtract `0x17` (**23**) once if the sum exceeds 23. At `0x062e` the local moves to `si`; `0x063b-0x0645` zero-extends the *live* hour and exits on equality, **before** `0x0647` advances ten requested minutes. Minutes, date, Q's remaining turns, and elapsed tick count are not compared. The raw shipped CMDS bytes and SHA-256 are preserved in `native/core/batch35-original-cmds-bytes.log`. The live clock wraps 23→00 through kernel `0x4fe6`, separately from the target's 23 subtraction. Thus 23:50 + one requested hour targets **01**, and the target 00 is unreachable for accepted one-digit 1–9 hour requests. The first tick entering the target hour completes terrain refresh, shared housekeeping (possibly expiring Q), NPC snap and occupancy/ejection; the next loop-head comparison exits if not ejected. Camp uses its distinct five-minute loop and was not changed.

**Pre-Batch-35 defect.** Native `bed_sleep` ran exactly `hours * 6` ten-minute ticks. With Q active for two turns, 12:00 + one hour ended at **12:50** after six ticks; the original keeps ticking until **13:00**, seven ticks. At 12:50 the old loop overslept to 13:40 after six ticks; the original reaches 13:00 after two Q ticks. The older TypeScript bed implementation is comparison evidence only. Native now computes the original wrapped target once and loops until `g_hour` equals it. The 16 pre-sleep NPC passes still precede status `S` and `Zzz`; the target test follows `Zzz` and precedes the first tick. Each tick still orders clock → 05:00/20:00 terrain refresh → shared housekeeping/Q expiry → NPC snap → occupancy/ejection. No Q storage, normal-turn semantics, T effect, Camp behavior, viewport presentation, or ejection logic changed. The deliberate-divergence table above is corrected for native; TypeScript still differs.

**Executable evidence.** The new `batch35_q_sleep_target` uses the shipped resource pack, real `AlphaRuntime`, raw Enter and (H)ole-up keys, a scheduled empty castle bed, and observes housekeeping state at every real NPC snap. Against unchanged Batch 34 production its RED log has **14/29 green, 15 red** across Q mid-sleep, exact boundary, expiry on target, 23→00, and nonzero minutes (`native/core/batch35-red-ctest.log`). After the target-loop correction, the expanded fixture is **42/42 GREEN**, covering Q expiration during sleep, on the target tick, on an ejecting tick, at 20:00 terrain refresh, and live 23→00 with target 01 (`batch35-green-ctest.log`). The Batch 30 test's obsolete Q/fixed-tick expectations and TypeScript-derived item-parity bed invariant were updated to preserve target-hour, one-housekeeping-per-snap, and early-ejection checks. Six temporary mutations were all killed: current hour (25 RED), target +1 (18), target −1 (25), subtract 24 at midnight (3), fixed six ticks (18), and less-than-hour comparison (4). Their build/test logs are `native/core/batch35-mutation-*.log` and `*-build.log`; the original source was restored and rebuilt (`batch35-post-mutation-build.log`).

**Regression.** The focused target is one new CTest. The 17-target bed/Camp/Q/T/turn/runtime subset passed 17/17 (`native/core/batch35-regression-ctest.log`), and the fresh Release/developer-tools complete host suite passed **105/105** (`batch35-final-ctest.log`) versus Batch 34's 104/104. H-171 Camp housekeeping and H-172 bed-entry viewport fill remain queued and unchanged. The host test exercises production input, runtime and resource paths; H-170 has no remaining device-specific question and no new hardware phase is allocated. Existing 6T–6Y checks remain pending. No flash was performed.

**Firmware.** A clean ESP-IDF 6.1 T-Deck build (`native/targets/tdeck/build-batch35`) completed with no project compiler errors or warnings: `openu5_tdeck.bin` **869,376 bytes (`0xd4400`)**, **+16 bytes** over Batch 34, with `0x2bc00` bytes free in the 1 MiB app partition (`native/targets/tdeck/batch35-firmware-build.log`). The commit-stamped Launcher is packaged after the source/evidence commit; its SHA-256 is recorded in the annotated Batch 35 tag. No flash or SD change.

## Batch 36 — H-171 Camp redraw and spell-clear parity

**Baseline and corrected question.** Batch 35 HEAD `4536c16ac0dae495ad6bcf47c558310579ebb1d8`, annotated `alpha2-batch35-q-sleep-target`, clean tree, 105/105 host, and Launcher 869,376 bytes (`0xd4400`), SHA-256 `91f2719a44970688cdf3a5c04d0cc46ac6e8e8f42e326732c43da048db26c601` were verified before production changes (`native/core/batch36-baseline-ctest.log`). H-171/D-27 originally asked whether Camp's five-minute loop omitted per-step world and survival housekeeping. Direct disassembly of `original/u5/ultima5/CMDS.OVL` and `ULTIMA.EXE` (`native/core/batch36-original-camp-bytes.log`) corrects the three misnamed calls in the Batch 32 queue: `0x5910` is `viewport_redraw`, whose `0x5944` calls `maybe_change_wind 0x2f62` when the redraw latch is enabled; `0x2900` draws the status panel; `0x20fa(1)` waits one INT 1Ch tick. Camp never calls `kernel_turn_housekeeping 0x2ae8`. The missing gameplay effects were wind/RNG, Q/T cancellation at entry, and the hourly encounter's relative draw order. The shipped TypeScript Camp flow is comparison evidence only.

**Original order.** The accepted command validates location and hours and offers the watch with at least two `G`/`P` party members (Batch 32). `Y` enters the guard picker; only `G` posts, while invalid or cancelled selection prints `None posted!`; `N` proceeds unwatched. No clock or RNG is spent in these prompts. CMDS `0x001c/0x001f` then clears the finite time-spell counter and Q/T effect, before setting the target hour at `0x0066` and forming the Camp scene and guard. After `Zzzzzz...`, the loop head `0x01ee` exits immediately if target hour is reached. Otherwise each iteration runs `0x5910` redraw (one wind `rand(0,63)` under its active-map latch, with additional wind-choice draws on a hit), `0x400c` ring regeneration (one `rand(0,7)` per eligible equipped member), and `0x2900` status draw. On a changed hour *before* the next clock advance, `0x021d` makes the one-in-64 encounter check. A hit reseeds enemy selection from the DOS wall clock, prints `Ambushed!`, and returns into combat without this iteration's clock/guard step or completion heal. A miss updates the prior-hour marker, advances the clock five minutes (`0x0314`), waits one timer tick (`0x031b`), optionally redraws local sky/time status through `0x4a84` (`0x0329`), and runs the posted guard's movement roll (`0x0340`). At an exact-hour start, N hours contain 12N five-minute steps and N−1 encounter checks. Completion runs the partial heal and one apparition gate, then returns to command state; no extra housekeeping pass runs at start or end. The guard changes only its own movement/occupancy, heal eligibility and RNG draws after the clock step.

**Included effects and absent effects.** Each Camp step advances the game clock and its clock-owned torch/light, calendar and lunar state; redraw can change wind; the regeneration ring rolls before the clock; the guard can move after it; the encounter check occurs at an hourly boundary before the next five-minute clock step. Camp entry clears Q/T once, so they do not expire through turn counts during Camp. `0x4a84` reads hour/moon glyphs and paints the local sky/status display; it is not an NPC schedule pass. Camp has no poison damage, meal deduction, starvation damage, `turns_since_start` increment, Q/T turn decrement, ordinary NPC pass/snap, bed ejection/occupancy probe, or bed-specific 05:00/20:00 terrain refresh. Actor/terrain animation inside `0x5910` is redraw presentation, not a normal command turn.

| Effect | Normal consumed command turn | Bed sleep ten-minute tick | Camp five-minute step |
|---|---|---|---|
| Clock | context cost (typically town 1, outdoor 2 minutes) | +10 minutes | +5 minutes; Q/T cleared at Camp entry |
| Terrain/display | context/world rendering | 05:00/20:00 terrain transform; status display | redraw animation/status and optional `0x4a84` sky display; no bed terrain transform |
| Survival and turn count | `0x2ae8` once | `0x2ae8` once after clock | no `0x2ae8` |
| Q/T | finite counter decrements in housekeeping | finite counter decrements after clock | cleared once at entry; no per-step decrement |
| Food, poison, starvation | housekeeping | housekeeping | absent |
| Regeneration ring | housekeeping tail | housekeeping tail | explicit `0x400c` before clock, once per step |
| NPC/guard | context NPC/world phases | 16 entry passes, then schedule snap and occupancy/ejection each tick | no ordinary NPC pass; posted guard alone moves after clock |
| Encounters | context spawn/hostile checks | hostile entry abort and occupancy ejection; no Camp ambush roll | one hourly Camp ambush roll after next redraw/ring, before clock (N−1 per N exact-start hours) |

**Native correction and proof.** Before Batch 36 `camp_sleep_step` advanced the clock, swept rings, and moved the guard without the redraw wind roll; `camp` retained Q/T; its hourly encounter roll came after the prior hour's twelfth guard step. The correction calls existing `maybe_change_wind` before ring/clock, clears Q/T on accepted Camp entry, and moves the existing encounter gate to after the next hour's wind/ring but before its clock/guard. Watch selection, guard cell/collision, heal exclusion, duration and N−1 encounter count remain intact. No bed loop or broad housekeeping routine is reused. The shipped-pack raw-key fixture against *unchanged Batch 35 production* was **12/21 GREEN, nine RED** (`batch36-red-full.log`); an added hourly-order assertion was RED against the interim build (`batch36-encounter-red.log`). The final fixture is **24/24 GREEN** from the clean certified build (`batch36-certified-focused.log`). It checks watched/unwatched cadence, the 06:00 meal boundary without food/poison/turn changes, pre-clock wind and ring draws, post-clock guard draw, actual wind-state change, Q/T cancellation, and a two-hour encounter draw boundary. The item parity tool defers 432 TypeScript Camp rows (38,728 total rows), and gameplay parity removes 256 TypeScript Camp scenarios (4,802 remaining sequences); all non-Camp comparisons remain active. The shipped-pack original-backed fixture is the Camp oracle.

**Mutation and regression.** Eight temporary production mutations were all killed by the final 24-check original-backed fixture: omit wind (15/24), half cadence (18/24), wind after clock (22/24), extra final wind (17/24), keep Q/T (20/24), add broad turn housekeeping (15/24), draw RNG without updating wind (22/24), and encounter before the next redraw/ring (23/24). Raw `native/core/batch36-mutation-*.log` and matching build logs are retained. The source was restored and rebuilt (`batch36-post-mutation-build.log`, **24/24 GREEN**). The 18-target Camp/watch/bed/Q/T/survival/NPC/encounter/runtime subset passed **18/18** (`batch36-certified-regression-ctest.log`). The clean Release/developer-tools complete host suite passed **106/106**, one new CTest over Batch 35 (`batch36-certified-ctest.log`; clean configure/build logs `batch36-certified-configure.log` / `batch36-certified-build.log`). The clean ESP-IDF 6.1 image was **869,456 bytes (`0xd4450`)**, **+80 bytes** versus Batch 35, with `0x2bbb0` free in the 1 MiB app partition (`native/targets/tdeck/batch36-firmware-build.log`). The commit-stamped Launcher is packaged after commit and its SHA-256 is recorded in the annotated Batch 36 tag. No hardware was flashed; 6T–6Y retain their assignments. H-172 remains out of scope.

**Separate queued H-173 / D-30: Camp duration from a nonzero minute.** The original computes a target *hour* once (`CMDS:0x0066-0x0079`) and tests equality at every loop head (`0x01ee-0x01f8`). The native `for (h < hours)` plus twelve steps per hour instead always spends 60 game minutes per requested hour. For a 05:50 one-hour Camp, the original reaches the target at 06:00 after two steps; native ends at 06:50 after twelve. This affects duration and hence total wind/ring/guard RNG, separate from H-171's behavior and order *within* each step. The two original byte ranges are preserved in `batch36-original-camp-bytes.log`; native `camp()` and `camp_sleep_step` show the fixed twelve-step loop. No H-173 production code or test expectation was changed in Batch 36. H-172 is still queued independently.

## Batch 37 — H-172 bed-entry viewport fill

**Baseline.** Before any production edit, HEAD was `72d4f082752b4a0ed886b2ffa4aeb96a2f982996`, tag `alpha2-batch36-camp-housekeeping` was annotated, and the working tree was clean. Batch 36's recorded host suite was **106/106**. Its Launcher measured **869,456 bytes (`0xd4450`)** and SHA-256 `e6ffb95d2052ae3c60792217e2506400c5b3102033ccbf150fcd3a5f850682f1` by direct file hash.

**Original visual and order.** Raw shipped `CMDS.OVL` bytes and SHA-256 are preserved at `native/core/batch37-original-bed-bytes.log`; call targets and driver mode are cross-checked with `re/notes/cama-apagon-296.md` and `re/notes/barrido-gfx-consumidores.md`. After at most sixteen pre-sleep NPC passes and the hostile cancellation gate, CMDS `0x05e6-0x0607` changes eligible party members `G→S`, `0x060a` refreshes the status panel, and `0x060d` prints `Zzzzzzz...`. Then `0x0614` sets graphics color **0** via kernel `0x0a70`, and `0x0624` calls kernel `0x0aa6` to copy-fill the inclusive rectangle **(8,8)–(183,183)**: all **176×176** original map-window pixels, in EGA black. The bordering frame, sky and wind strips, transcript and party panel lie outside that rectangle. The driver uses copy mode for `0x0aa6`, unlike its distinct XOR rectangle operation. This is a renderer write, not an animation, sprite/bed tile replacement, NPC update, actor suppression, or game-state mutation.

The target-hour comparison at `0x063b` follows the fill; the first ten-minute advance is `0x0647`. A 05:00/20:00 crossing refreshes terrain at `0x0664`; housekeeping and status at `0x0671/0x0674`, NPC/object snap at `0x0677`, and occupancy/ejection at `0x0688` retain their Batch 30/34/35 order. No sleep-loop call redraws the map window, so black persists through every tick, including an ejecting tick. Common exit wakes `S→G`, steps east, and the next ordinary redraw restores the map. The fill changes no clock, status, actor, schedule, bed tile, occupancy or RNG. NPC bed/sleep presentation uses the separate NPC pass and schedule snap; the fill covers the entire viewport, regardless of any NPC in it. Camp does not reach this site.

**Native deficiency and correction.** Batch 36 assigned `S` before `Zzz` but displayed the prior map throughout the synchronous sleep command. `bed_sleep_begin` now emits a raster-only `BedViewportFill` immediately after `Zzz`. `AlphaRuntime` presents it synchronously on the Board while the command is still on the input stack. `Board::fill_bed_viewport()` uses its existing hardware `fill_rect` with black. Native sky/wind captions occupy the top and bottom nine rows *inside* its 176×176 viewport, so the physical map-image rectangle is (4,13), size 176×158; that preserves their original out-of-fill behavior and the frame. Device and host share `bed_viewport_rect()`. Invalidating only the viewport cache makes the next normal render restore map pixels, even if the old and new map CRC match. Sleep timing, NPC scheduling, occupancy, housekeeping and Camp behavior were not changed. Item parity filters only this raster-only event while retaining all semantic event and state comparisons.

**Executable evidence.** New CTest `batch37_bed_viewport` uses the shipped pack, real AlphaRuntime, raw Enter/H/1/Enter input, an authored castle bed, deterministic tile-cache pixels and a capture Board. It reads every map-image pixel at the first and every later NPC snap; checks that `Zzz` precedes the fill; proves black persists through six ticks and wake with sky/frame unaffected; checks `G→S→G`, six turns and the east-step epilogue; and verifies ordinary post-command render restores the map. Camp and Iolo's Hut hostile cancellation remain unfilled. Against unchanged Batch 36 production the RED log was **9/12 green, 3 red** (`batch37-red.log`); corrected code is **12/12 green** (`batch37-green.log`, `batch37-post-mutation.log`). Seven temporary mutations were killed and restored: omit fill (3 failures), before Zzz (1), before sleeping status (1), after first tick (2), apply to Camp (1), one-pixel-short rectangle (2), and normal redraw instead of fill (3). Raw `batch37-mutation-*.log` and matching build logs are retained; `batch37-post-mutation-build.log` proves restoration.

**Regression and firmware.** The affected bed/Camp/party/renderer/runtime subset passed **19/19** (`batch37-regression-ctest.log`), and the fresh developer-tools host build passed **107/107** versus Batch 36's 106/106 (`batch37-final-configure.log`, `batch37-final-build.log`, `batch37-final-ctest.log`). A clean ESP-IDF 6.1 build produced `openu5_tdeck.bin` **869,888 bytes (`0xd4600`)**, **+432 bytes** over Batch 36 and `0x2ba00` bytes free in the 1 MiB app partition (`native/targets/tdeck/batch37-firmware-build.log`). The commit-stamped Launcher is packaged after commit. No flash was performed; existing 6T–6Y remain pending. **Phase 6Z** is queued because the host Board is a framebuffer capture stub and cannot observe the actual TFT/SPI transfer. H-173/D-30 remains out of scope and unfixed.

**Separate finding.** Original `0x060a` and `0x0674` draw the party status panel after `G→S` and during ticks. Native already mutates the status byte in the right order, but its synchronous sleep path does not present those intermediate roster frames. This distinct presentation gap is **H-174/D-31**, queued without a Batch 37 correction.

## Batch 38 — H-173 Camp duration at nonzero minutes

**Baseline and original mechanism.** Batch 37 HEAD ad3b1d00f34a208e84fb85c507eea1c9df01b910, annotated alpha2-batch37-bed-entry-viewport, clean working tree, 107/107 rerun host tests (native/core/batch38-baseline-ctest.log), and the Batch 37 Launcher at 869,888 bytes with SHA-256 94ee00da7fc01d30d3125393cfdb0a3de101e901bdcd749dbd1bb2863c8ae8c3 were verified before production edits. The shipped original CMDS.OVL bytes and hash remain in native/core/batch36-original-camp-bytes.log. At 0x0066-0x0079 the requested hours are added to the current hour in a local word and 24 is subtracted once when the sum exceeds 23. No minute is copied into the target. The loop-head 0x01ee-0x01f8 compares that target word to the live hour and exits before redraw when equal. Each unfinished iteration redraws/wind, sweeps rings, performs an encounter check if the hour changed since the prior iteration, advances five minutes, and moves a posted guard. Thus the final clock/guard step happens, but there is no final extra redraw, ring sweep, or encounter check after the target hour is reached. This Camp wrap subtracts 24; it differs from bed sleep's separate subtract-23 behavior.

| Start and request | Original completion | Five-minute steps | Encounter checks on uninterrupted Camp |
|---|---:|---:|---:|
| 12:00 + 1 hour | 13:00 | 12 | 0 |
| 12:05 + 1 hour | 13:00 | 11 | 0 |
| 12:55 + 1 hour | 13:00 | 1 | 0 |
| 05:50 + 1 hour | 06:00 | 2 | 0 |
| 23:55 + 1 hour | 00:00 | 1 | 0 |
| 23:05 + 2 hours | 01:00 | 23 | 1, at the intervening 00:00 hour |
| 12:05 + 2 hours | 14:00 | 23 | 1, at the intervening 13:00 hour |
| 12:01 + 1 hour | 13:01 | 12 | 0 |

The 12:01 case shows that a five-minute step can first enter the target hour at minute 01: completion is hour-only, with no minute alignment or exact-elapsed-time target. Clock wrap is applied by the existing clock helper; the Camp target wraps modulo 24. The hourly encounter check occurs after that iteration's redraw/ring and only when the target hour has not ended Camp.

**Native mismatch and correction.** Before this batch, camp() made one call to a twelve-step camp_sleep_step() per requested hour, so 05:50 + 1 ended 06:50 after twelve steps, consuming ten extra wind and ring sweeps plus any guard draws. The original target-hour rule was known as D-30/H-173 from Batch 36 but intentionally left unfixed there. The production change computes the wrapped target once, checks it at each loop head, and executes a single existing-order step until reached. A saved previous hour keeps the encounter check on intervening hour transitions. Accepted-entry Q/T clear, watch selection, guard validation/movement, healing, redraw/wind and ring order, hourly encounter RNG, and bed sleep are unchanged.

**Executable and mutation evidence.** New CTest batch38_camp_duration loads the shipped pack and enters Camp through raw keyboard events in AlphaRuntime. It uses a fixed original RNG seed and ring draws to count every five-minute step; it checks clock endpoints, wind plus encounter draw totals, pre-clock ring timing, no ordinary turn/food progress, and the watched guard draw after the final clock step. The initial unchanged-production RED was 12/31 (native/core/batch38-red.log); the expanded final fixture is 43/43 GREEN (batch38-final-focused.log). Six temporary source mutations were killed and restored: fixed twelve-step hours (13/38), preserving the start minute (13/38), one step early (8/38), an extra final step (9/38), wrong midnight modulo 23 (30/38), and skipping the final guard (37/38). Raw mutation and matching build logs, batch38-mutations.log, and the restored build/test logs are retained under native/core. The additional off-grid 12:01 witness was added after the six-mutation run; it passed in the final fixture.

**Regression and firmware.** The affected Camp/watch/guard/encounter/bed/runtime subset passed 9/9 (batch38-regression-ctest.log). The fresh developer-tools full host suite passed 108/108, one new CTest over Batch 37 (batch38-red-configure.log, batch38-final-build.log, batch38-final-ctest.log). A clean ESP-IDF 6.1 firmware build was attempted; a parallel compilation of an unchanged ESP-IDF LCD source hit an internal compiler error, then a two-job retry completed (native/targets/tdeck/batch38-firmware-build.log and batch38-firmware-retry.log). The image is 869,856 bytes (0xd45e0), 32 bytes smaller than Batch 37, with 0x2ba20 bytes free in the 1 MiB app partition. The final commit-stamped Launcher is packaged after commit; its SHA-256 is recorded in the annotated tag. No flash or physical check was performed. Host execution covers the production clock and RNG path completely, so no new hardware phase is needed. Existing pending 6T through 6Z retain their assignments. H-174 intermediate party-panel refresh remains queued and untouched; no other defect was opened.