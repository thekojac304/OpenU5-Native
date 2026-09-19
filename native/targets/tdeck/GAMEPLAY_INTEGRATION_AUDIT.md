# OpenU5-Native — Whole-Project Gameplay Integration Audit

**Target:** LilyGO T-Deck Plus · ESP32-S3 · ESP-IDF 6.1 · 16 MiB flash · 8 MiB PSRAM · 320x240 ST7789 · physical keyboard · trackball · GT911 · microSD
**Scope:** end-to-end playability, not code presence.
**Method:** input→adapter→UI→core→state→renderer→feedback→mode-return→persistence tracing, static reachability sweeps, reference (`game/src`, `re/`, `extractor/`) adjudication, host test-suite execution, and one purpose-built core probe.
**Commit audited:** `c18f5b64` (branch `main`, clean tree).

Evidence classes used below:
- **[EXEC]** proven by running code during this audit
- **[REF]** proven against the authoritative TypeScript/DOS reference in this repo
- **[STATIC]** proven by unambiguous control-flow reading
- **[UNPROVEN]** requires physical device evidence

---

## 1. EXECUTIVE SUMMARY

### Overall integration health: **NOT PLAYABLE END-TO-END**

The **core is in far better shape than the device integration**. At the original audit baseline, 56 of 57 host tests pass, including deep byte-level parity suites for combat, commands, magic, items, shops, dialogue, dungeons, travel and persistence. **Post-Batch-1, the host suite was 58 total, 57 pass, with only the pre-existing `gameplay_parity` mismatch 59 (R-02/R-03/R-04) failing.** **Post-Batch-2, R-02/R-03/R-04 are GREEN and mismatch 59 is fixed.** The host suite is still 58 total, 57 pass, but the sole failure is now a *different*, newly-exposed mismatch — **`gameplay_parity` mismatch 2034 (R-21)**, a scroll-use event/message/SFX divergence that was simply hidden behind mismatch 59 until now. Mismatch 2034 was reproduced byte-for-byte on the untouched pre-Batch-2 baseline with the Batch 2 changes removed, confirming it is pre-existing and unrelated to R-02/R-03/R-04; it is tracked separately (§3 R-21) and was **not** fixed in Batch 2. The remaining failures are almost entirely in the **glue layer**: `native/targets/tdeck/main/alpha_runtime.cpp` (1375 lines), `native/core/src/ui_session.cpp`, `native/core/src/presentation.cpp`, and the **asset pack**.

`AlphaRuntime` itself still has no host coverage — nothing in the repository instantiates it directly, and the on-device "smoke tests" are data-presence probes and isolated `UiSession` probes with a spy dispatcher that never exercise the adapter that consumes the intents. **Batch 1 added `ui_mode_regression`, an ESP-free seam (`ui_mode_policy.h`) that host-tests mode arbitration and `UiSession` mode ownership (28/28 GREEN)** — narrowing, but not closing, that blind spot. Most defects below still live in the parts of the blind spot that seam does not cover.

### Largest risks, in order

1. ~~**R-01 — Shop and Dialogue UI modes are destroyed on every keypress.**~~ **RESOLVED in Batch 1.** `AlphaRuntime::synchronize_after_debug()`'s mode arbitration was split out and now preserves `Shop`/`Dialogue`/`ShrineSpecial` instead of unconditionally forcing `base_mode` to Combat/Dungeon/Exploration. See §3 R-01 and §14 Batch 1 for root cause, fix, and test evidence.
2. **R-05 — The dungeon has no art.** The perspective slice atlases (`DNG1/2/3.16`) and feature art (`ITEMS.16`) that the reference compositor blits are **not packed into the native asset file at all**. `native/tools/u5pack/alpha1.ts` packs dungeon *cell maps* only. `render_dungeon_view()` is a hand-rolled wireframe of `dungeon_line`/`dungeon_rect` calls. ANCHOR 4 cannot be fixed by tuning the renderer; the asset pipeline must be extended first.
3. ~~**R-19 — Ships cannot sail.**~~ **RESOLVED in Batch 3.** `handle_exploration` now branches to `CommandKind::YellSails` when the party is aboard a frigate outside the Underworld, exactly as the reference's `yell()` dispatcher does, before the word prompt. See §3 R-19 and §14 Batch 3.
4. ~~**R-07/R-08 — The endgame (U)se chain is unreachable.**~~ **RESOLVED in Batch 3.** `usable_item_display_name()` now has exactly one interpretation — the real canonical id — and the shared picker seam gates a row per canonical id from its authoritative possession owner. Grapple is gone from the picker (Klimb-only). Pocket Watch (35) remains the one deliberate omission: no field anywhere backs it. See §3 R-07/R-08 and §14 Batch 3.
5. ~~**R-06 — `Ready` is rejected in combat and in dungeons.**~~ **RESOLVED in Batch 3.** Ready now routes in all three contexts, passes `battle = c.combat` to `equip_item()`, and refreshes the acting player's `CombatActor` equipment cache after a successful in-combat change. See §3 R-06 and §14 Batch 3.
6. **R-09/R-10 — Five modal responses and both NPC-initiated events are silently discarded**, disabling Blackthorn, the Britannia guards, fountains, and every shopkeeper/guard that starts the conversation.

### Anchor verdicts

| Anchor | Verdict | Root cause status |
|---|---|---|
| **1 — stale blue/relic symbol after loot removed** | **GREEN — RESOLVED (Batch 2), hardware validation not yet performed** | **Root-caused and fixed.** Two mechanisms, both repaired: (a) native promoted unopened arena chests to world objects the reference never creates — both promotion sites (`combat.cpp::finish_encounter_combat`, `outdoor.cpp::outdoor_start` `victory_latch`) removed (R-03); (b) the invalid promoted objects carried raw `o.tile` = **1** = deep-water terrain (blue) instead of sprite `0x101` — this defect disappears as a consequence of (a), since legitimate stationary chest QuestObjects already store the pre-offset sprite tile correctly (R-02). `gameplay_parity` mismatch 59, the reproducible failing test for this anchor, is now GREEN. Physical device confirmation is the one remaining step — see §16 Phase 3 steps 19–22. |
| **2 — loose-loot icons don't match identity** | **GREEN — RESOLVED (Batch 2), hardware validation not yet performed** | **Root-caused and fixed.** The combat and world loot tile equations were already correct (`0x100+id`, verified [EXEC]). The defect was **layer ordering**: `compose_world_presentation` painted *every* quest object in one unified last-write-wins pass, so a chest/prop/ship object could overdraw a loot icon sharing its cell. Repaired (R-04) by splitting composition into the reference's two layers — stationary/non-loot objects resolved first-match-per-cell, then loot/search resolved last-match-per-cell (LIFO) and painted unconditionally on top, so a stationary object can never mask loot again. Covered by `presentation_regression`'s new R04-OVERLAY (deliberately defeats insertion-order luck) and R04-LIFO cases. Physical device confirmation is the one remaining step — see §16 Phase 3 step 21. |
| **3 — View Gem appears to do nothing** | **RED** | **Partially root-caused.** Command, gem decrement, deferred turn and a renderer dispatch all exist and are wired. The **presentation itself is fabricated**: `render_world_gem_view` colours cells by `tile&3`/`tile&7`/`tile&15` bit tests, which is not a terrain classification — the output is noise, and the top/bottom 9 px are overdrawn by the sky/wind bars. Needs one device capture to separate "renders noise" from "renders nothing". |
| **4 — dungeon 3D broken/unusable** | **RED** | **Root-caused.** Required art is not in the asset pack (above). Controls are, however, already correct — see §12. |

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
| Journey Onward | menu 0 | `service_frontend_intent` → `save_.load` | `load_native_state` + `synchronize_loaded_world` | gameplay rebind | `frontend`, `persistence` | ✗ | **R** | Load does not restore/clear `objects_`. R-14 |
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
| `c` | Cast | **R** | World direction/unit targets never prompted. R-11 |
| `e` | Enter | **Y** | Dungeon entry logs exist; town/dungeon entry unproven. Y-07 |
| `f` | Fire | **Y** | Reticle + Confirm path in `handle_modal`; `CellProjectile` has no renderer. Y-04 |
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
| `v` | View gem | **R** | ANCHOR 3. R-17 |
| `x` | X-it (Disembark) | **Y** | Y-07 |
| `y` | Yell | **G** | R-19 **GREEN** (Batch 3) — frigate branch dispatches `YellSails`; word-of-power Yell unchanged (Y-24 both branches covered). |
| `z` | Z-stats | **R** | Member picker and party highlight work; the Stats/Arms/Provisions/Reagents/Spells/Items/Armaments page family does not exist — selecting a member just reopens the picker. R-22 |
| space/Enter | Pass | **G** | Two independent routes, both echo. |
| `0`–`9` | Set Active Player / harpsichord note | **G** at the core/`UiSession` level; **device-partial** on T-Deck hardware | Y-20, R-20 **GREEN** (Batch 3) — digits dispatch `SetActivePlayer` with the literal digit; at the harpsichord they are intercepted first and dispatch `HarpsichordNote`. On T-Deck hardware, digits `1`-`9` (and Cancel, via short-press) are physically reachable, but literal `'0'` — the clear-active-player route — is not: the only matrix position resolving to `'0'` is the physical Mic key, which `UiInputAdapter::translate()` unconditionally intercepts for short=Cancel/long=Movement-Mode before any character dispatch. Tracked as **Y-29** (device-only reachability gap, open). |
| any other | `"X-What?"` | **G** | Matches reference unknown-key echo. |

### D. View command family

| Sub-command | Status | Reason / ID |
|---|---|---|
| `V` gem — world | **R** | R-17. Core (decrement, defer turn, `AfterGemView`) matches `game.ts::view()` byte-for-byte [REF]. Presentation fabricated. |
| `V` gem — dungeon floor | **R** | `render_dungeon_gem_view` is an 8-neighbour flood over an 22×22 window; reference is the authored DNGLOOK 8×8 floor map. R-17 |
| `V` gem — no gems | **G** | `"You have none!\n"` + immediate turn. Matches DS 0xa266 and the bug-for-bug turn charge [REF]. |
| Crystal-ball "Strange vision!" gem view | **Y** | `look.cpp:40` sets `gem_from_crystal=true`; device correctly suppresses the deferred turn. Presentation shares R-17. |
| `MapReveal` (Wis An Ylem, In Quas Wis scroll, Death Vision) | **R** | Event emitted with `note=20` frames; **no consumer anywhere**. R-12 |
| `Zodiac` (Use Spyglass) | **R** | Event emitted; **no consumer**. R-13 |

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
| Save/load of world loot | **R** | `objects_` not serialized. R-14 |

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
| **World casting with a target** | **R** | R-11 — 6 spells consume charge + MP, then no-op. |
| Ceremony (invert + timing) | **G** | `start_magic_ceremony` calibrated to CAST2:0000; no-ceremony list matches. |
| `MagicEffect::Reveal`/`DeathVision` visual | **R** | R-12 |
| **Spell descriptions vs. actual effects** | **R** | R-16 — ≥10/48 summaries and 12/48 target labels contradict the effect [EXEC]+[REF]. |
| Save/load of spell quantities | **G** | `spellQuantities` round-trips. |

### I. Potions / scrolls / magic items

| Aspect | Status | Reason / ID |
|---|---|---|
| Potion ids 8–15, scroll ids 0–7 | **G** | Index space matches `world_magic.cpp` handlers exactly; `item_parity` green. |
| Potion target prompt (`UseTarget`) | **G** | `modal()` opens party selection for items 6 and 8–15 before dispatch. |
| Consumption on cancel | **G** | Deliberately reference-correct: "canceling target selection still loses it" (`inventory.h`). |
| Scroll effects | **Y** | `case 1` (wind) requires `cmd.has_direction`, which the device never supplies → Rel Hur scroll changes nothing. Y-21 |
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
| `c`, `u`, `z`, `v` | Cast, Use, Stats, View gem | **G** route / see R-11, R-17 |
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
| Blackthorn interrogation | **R** | R-09 |
| **Dialogue UI mode survival** | **G** | R-01 resolved (Batch 1). Note: the active prompt loop itself was always protected by `TextEntry` modal state; R-01's damage was to `base_mode` underneath it. |
| Dialogue exit → correct mode | **G** | Dialogue now owns `dialogue_return_mode_`. R-18 resolved (Batch 1). |
| NPC walk persistence | **G** | `capture_npc_walk`/`restore_npc_walk` with the fidelity gate. |

### O. Quests

| Aspect | Status | Reason / ID |
|---|---|---|
| Quest tables, flags, shrine bits, doom bits, shadowlords | **G** | `quest_parity` (1581-line `quest_case.inc`) green. |
| Word-of-power Yell at dungeon entrances | **Y** | Route works; unaffected by the R-19 ship branch added in Batch 3 (both branches now regression-covered — Y-24 discharged). Y-07 |
| Shrines (visit/restore/donate) | **Y** | All three modals wired in `modal()`; `UiMode::ShrineSpecial` lifecycle repaired in Batch 1 (§5). Remaining YELLOW is physical-device validation only. Y-25 |
| Search-based quest chains | **Y** | `quest_search.cpp` + `search_objects` fixtures; found objects are world objects → **lost on reload** (R-14). |
| Shards → Flames ritual | **G** reachability | R-08 resolved (Batch 3): shards 29–31 are Use-picker rows gated on `game.quest.shards[0..2]`. |
| Crown / Sceptre / Amulet | **G** reachability | R-07/R-08 resolved (Batch 3): ids 18/19/20 gated on `game.quest.artifacts[0..2]`. |
| Blackthorn / Falsehood / Abbey | **R** | R-09 |
| Codex / endgame | **Y** | The Use chain that gated it is reachable (R-08 resolved, Batch 3); the endgame itself is still unproven end-to-end. Y-07 |
| HMS Cape plans | **G** reachability | R-08 resolved (Batch 3): id 33 gated on `game.hms_cape`. |
| Harpsichord melody (Cove passage) | **G** | R-20 resolved (Batch 3): digit keys at the harpsichord dispatch `CommandKind::HarpsichordNote`. |
| Moonstones / moongates | **G** reachability / **R-14** persistence | R-08 resolved (Batch 3): ids 21–28 are rows whenever carried (`!buried`), so `CommandKind::UseMoonstone` is reachable. Burial state still does not survive save/load — R-14. |

### P. World objects / terrain / special cells

| Aspect | Status | Reason / ID |
|---|---|---|
| Doors, locked, secret, open-door timer | **G** | `CommandState::Door` + `CommandEffect::Doors`; `command_parity` green. |
| Signs | **G** | `look_sign` test + `resolve_look_sign`. |
| Terrain overrides (volatile/persistent/hourly/wipe) | **G** | `WorldTerrain` with a 4-layer `inspect()`; `capture_terrain`/`restore_terrain` round-trip. |
| Fields, traps, fireplaces, emitters, light flood | **G** | `presentation.cpp` `visibility()` + `kEmitters`; `presentation_regression` green. |
| Chests in the world | **G** — RESOLVED (Batch 2) | R-02 fixed. The invalid tile-1 world chests came solely from the R-03 promotion sites; legitimate stationary chest QuestObjects (`quest_world.cpp::hydrate_interior_objects`) already stored the correct pre-offset sprite tile (`0x101`) and needed no change. |
| Quest objects / hidden items | **R** | Not serialized. R-14 |
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
| Poison tick feedback | **Y** | `PoisonTick` event unconsumed. Y-04 |

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
| Save / load slots | **Y** | `service_system_menu_intent` → `synchronize_loaded_world`; inherits R-14/R-15. |
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

### R-05 — Dungeon perspective art is not in the asset pack (ANCHOR 4) · **SEVERITY 1**

`native/tools/u5pack/alpha1.ts:49` packs `dungeons.bin` — 8 records × 516 bytes = **cell maps only**. There is no DNG or ITEMS record in the pack, and `native/ASSETS.md` never mentions dungeon art.

`extractor/src/parsers/dngtiles.ts` **exists** and parses exactly what is needed (`parseDngView`, `parseItemsView`), and `game/src/skin/fiel/dungeon.ts` documents the authoritative compositor: 28 pre-drawn perspective slices per wall variant blitted at fixed X per depth with mirrored pairs, plus 20 half-feature images from ITEMS.16 for stairs/fountains/traps/chests, with floor speckle and ceiling **baked into each slice**.

What the device does instead (`native_renderer.cpp:352-411`): fills the top half `kDungeonCeiling` and the bottom half `kDungeonFloor`, scans forward for a wall, draws Bresenham lines for corridor edges, then `dungeon_rect`s a flat grey wall with mortar stripes. No doors, no variant, no features beyond a green/red 12×12 blob, no light model.

**This is why the view is "geometry partially present, textures wrong".** It cannot be fixed in the renderer alone.

**Fix shape (ordered):** 1) extend `alpha1.ts` with a `dngview.bin`/`itemsview.bin` record set using the existing `dngtiles.ts` parser; 2) add slice/feature readers to `asset_pack.cpp`; 3) replace `render_dungeon_view` with a slice blitter following `dungeon.ts`'s SIDE_X / depth-pair model; 4) wire `DungeonViewInfo.wallVariant` from the dungeon id.

**Evidence:** [STATIC] + [REF] + asset-pack manifest.

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

### R-25 — `CrystalBall` dispatches with `member == -1` and is always rejected · **SEVERITY: unassessed** · **RED — OPEN, newly discovered (Batch 4 adjudication), not fixed**

Reference `main.ts` uses `pickCommandChar` (a character picker) before viewing a crystal ball; native opens a `YesNo` and, on Yes, dispatches `CommandKind::CrystalBall` with the default, never-set `Command::member == -1`. `look.cpp:39` rejects that outright: **zero events, zero HP change, always `Rejected`**, regardless of player input. Discovered while adjudicating R-09 (the original audit cited this as a correctly-handled precedent to copy for the Blackthorn/guard fix; it is not). Left unfixed per Batch 4's explicit scope boundary. **Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (`modal()`, `CrystalBall` arm), `native/core/src/look.cpp:39`.

---

### R-26 — `WellDrop` "No" dispatches nothing and ESC does not mean No · **SEVERITY: unassessed** · **RED — OPEN, newly discovered (Batch 4 adjudication), not fixed**

Reference `dropCoin(false)` (the "No" answer) prints `"No\n"`; native's `WellDrop` "No" arm dispatches nothing at all — it only "works" on Yes because `Command::member == -1` happens to be truthy in `look.cpp:37`. The reference also treats ESC as No (`yesno-esc`); native passes `cancel_means_no=false`, so ESC is ignored instead. Discovered while adjudicating R-09 (same reason as R-25 — cited as a correct precedent, only half true). Left unfixed per Batch 4's explicit scope boundary. **Files:** `native/targets/tdeck/main/alpha_runtime.cpp:645`, `native/core/src/ui_session.cpp` (`WellDropPrompt` case).

---

### R-11 — Six world spells consume charge and MP, then do nothing · **SEVERITY 2**

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

---

### R-12 — `MapReveal` has no renderer · **SEVERITY 3**

Emitted by `world_magic.cpp` for `MagicEffect::DeathVision` (Wis An Ylem) and by the In Quas Wis scroll, with `e.note = 20` animation frames. Unconsumed by both `UiSession` and `AlphaRuntime`. The spell succeeds, prints its message, charges a turn, and reveals nothing.

### R-13 — `Zodiac` has no renderer · **SEVERITY 3**

`Use Spyglass` is one of the six items the picker *does* offer. The core emits `GameEventKind::Zodiac`; nothing consumes it. The reference builds a `ZodiacView` (`game/src/core/world/zodiac-view.ts`).

---

### R-14 — World objects are never serialized, and never cleared on load · **SEVERITY 1**

`AlphaRuntime::objects_` (`std::vector<QuestObject>`) holds combat-promoted chests, `(S)earch`-revealed items, shard spawns, spilled world loot, ships and props. Grep across `save_core.cpp`, `gameplay_save.cpp` and `alpha_save.cpp` finds **no reference to it**.

Two consequences:
1. **Loss:** everything the player uncovered or dropped is gone after a power cycle.
2. **Leak:** `synchronize_loaded_world()` (`alpha_runtime.cpp:1284`) clears `actors_`, `dungeon_` and `combat_` but **not** `objects_` — loading save B while save A's world objects are live leaves A's chests and quest items in B's world. (The new-journey path at `:1308` does clear it, correctly.)

**Fix shape:** add a `worldObjects` array to the sidecar (`gameplay_save.cpp` already owns `mapOverrides`/`openDoors`/`overworldEnemies` and is the natural home), plus an `objects_.clear()` in `synchronize_loaded_world`.

**Evidence:** [STATIC].

---

### R-15 — `DungeonState` is not serialized · **SEVERITY 2**

Only `dungeonRoomsCleared` persists. `DungeonState` (`active`, `pos.dungeon/floor/x/y/facing`, the 512-byte mutable cell copy, the 64-byte `revealed` bitmap, the wanderer) is absent. `synchronize_loaded_world` does `dungeon_ = {}`, so a save taken inside a dungeon reloads at the **surface return position** with the dungeon session gone — silently. Reveal progress and field/trap mutations are lost.

Whether U5 permits saving inside a dungeon at all is a reference question worth settling before fixing; if it does not, this becomes an explicit N/A plus a save-time refusal message.

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

---

### R-17 — View Gem presentation is fabricated (ANCHOR 3) · **SEVERITY 2**

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

### R-21 — `gameplay_parity` scroll-use event/message/SFX divergence, newly exposed after Batch 2 · **SEVERITY: unassessed** · **RED — OPEN, newly exposed**

**Discovery context:** Batch 2 fixed `gameplay_parity` mismatch 59 (R-03). Fixing it allowed the same test to run further and reach a *different* failure, previously hidden:
```
ctest -R gameplay_parity      →  Error: Gameplay mismatch 2034
```
Observed divergence (from the generated mismatch artifact) is in scroll-use event output — example: native emits message text `"Scroll"` where the reference emits `"Used Vas Lor Scroll."`, and the reference's expected event stream includes an `sfx: "scroll-used"` event and an `unknown`-kind event that native's actual output does not produce.

**Critical scoping evidence:** this mismatch was reproduced **byte-for-byte** on the untouched pre-Batch-2 baseline (`63eeac3b`), with the Batch 2 production changes stashed out. Therefore:
- it is **pre-existing**, not introduced by Batch 2;
- it was **masked** by the earlier-failing mismatch 59, which halted the test before sequence 2034 was ever reached;
- it is **not caused by, and is out of scope for, R-02/R-03/R-04**.

**No root cause has been investigated yet.** This finding is intentionally narrow and evidence-only: [EXEC] `gameplay_parity` reaches mismatch 2034 after Batch 2; [EXEC] the identical mismatch reproduces on the untouched `63eeac3b` baseline. Do not assume a cause (scroll message table, missing `sfx` event consumer, event-shape difference, or something else) until investigated. Do not fold this into R-16 (spell description/label mismatches) unless a future investigation proves they share a cause — R-16 is about spell *descriptions*, not scroll-use *event output*, and the two have not been shown to be related.

**Fix shape:** not yet determined — requires its own investigation batch (see §14 Batch 12).

**Evidence:** [EXEC] only, both directions (present after Batch 2; present on the untouched baseline).

---

### R-22 — Native Z-stats status/inventory pages missing · **SEVERITY 2**

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

**Fix shape:** not yet designed — full implementation is deferred to its own future batch (see §14 Batch 13). This entry is evidence-only; do not implement in the same pass that files this finding.

**Evidence:** [STATIC] — `ui_session.cpp` `case 'z'` and the `OpenStatusSelection`/`UiRequestId::Status` handling; [REF] — reference `(Z)` page family listed above.

---

### R-23 — Blackthorn sacrifice can leave blank roster gaps · **SEVERITY 3**

**Discovery context:** pre-Batch-4 hardware testing traced `sacrificeFirstCompanion()` (reference) against `blackthorn.cpp::sacrifice()` (native) while investigating an unrelated single-character-combat report (see R-24/DebugPreset::Combat, which turned out to be the actual cause of that report — this finding is separate and lower priority).

**Reference behavior:** `sacrificeFirstCompanion()` performs full roster compaction — `characters.splice(victim, 1)` removes the sacrificed member and shifts every later slot down, then `characters[15] = sacrificedRecord` places the sacrificed record's remnant at the fixed final roster slot.

**Native divergence:** `blackthorn.cpp::sacrifice()` only shifts entries within the old `character_count`, then unconditionally forces `character_count = 16`. Because the shift does not extend into the newly-exposed range `[old character_count, 16)`, and `character_count` is forced to 16 regardless, this can expose blank/default-constructed roster slots inside `[0, character_count)` that were never part of a compaction shift.

**Potential downstream effects:** any code that iterates `[0, character_count)` and assumes every slot holds a real (if possibly dead) character — including the Developer fill helpers (`fillable_party`/`maximize_party` in `debug_developer.cpp`, which stop at the first blank `name[0] == 0` slot) — may see a native-only blank gap that the reference roster never produces.

**Scope note:** this is **not** the cause of the user's ordinary one-character-in-combat report; that was `DebugPreset::Combat` silently engaging Set Active Player (R-24, fixed in the pre-Batch-4 Developer-tool cleanup). R-23 is a separate, lower-priority fidelity bug and is not fixed in this pass.

**Fix shape:** not yet designed — mirror the reference's `splice`+fixed-slot-15 compaction exactly. Deferred to a future batch.

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

## 4. YELLOW / UNPROVEN AREAS

| ID | Area | Why unproven | Missing evidence |
|---|---|---|---|
| Y-01 | Movement Mode in frontend menus | `mode_before` is forced to `InventorySelection`/`TextEntry` for the frontend (`handle()`), and `is_movement_context` excludes selections | Decide whether WASD should navigate the title menu; today only the trackball and hotkeys work |
| Y-02 | Trackball centre click | GPIO 0 not wired in `tdeck_input.cpp` | Decide whether click = Confirm |
| Y-03 | Audio | `GameEventKind::Sfx` unconsumed; `sound_volume`/`music_volume` settings are reserved | Declared out of scope for Alpha 2.0 — treat as N/A but note the dangling settings |
| Y-04 | Feedback VFX | `Quake`, `CellExplosion`, `CellProjectile`, `Refuge`, `TrollSneak`, `PoisonTick` all unconsumed | Six missing feedback channels; `MagicCeremony` **is** consumed and works, so the pattern exists |
| Y-05 | `AlphaRuntime` coverage | No host test instantiates it (ESP-IDF headers) | Highest-leverage test gap in the project — see §7 |
| Y-06 | Device smoke tests | 37 scenarios, overwhelmingly data-presence assertions + `UiSession`-with-spy probes | None validate a renderer output or a full route |
| Y-07 | Broad device presentation | No automated device evidence for any gameplay screen | Physical certification pass — see §16 |
| Y-15 | Dungeon `m` key | Aliased to Cast with echo "Cast"; Mix unreachable in dungeons | Confirm reference behaviour for Mix underground |
| Y-17 | Acknowledgements | One hardcoded line vs. the reference credit sequence | Low priority |
| Y-18 | Developer entry from frontend | Calls `frontend_.enter_game()` directly, entering gameplay on whatever INIT.GAM state was loaded at boot | Confirm this is the intended debug affordance |
| Y-19 | Mix quantity | `c.hours = 1` hardcoded in `modal()`; reference lets the player choose a batch size | Add a numeric modal |
| Y-20 | `SetActivePlayer` | **GREEN — RESOLVED (Batch 3) at the core/`UiSession` level.** The reference binds the digit keys `0`-`9` (kernel `0x4080`, via MAINOUT `0xc06` / TOWN `0xe34`), not `N`; `N` stays New Order. `handle_exploration` now echoes `"Set Active Plr:"` and dispatches `CommandKind::SetActivePlayer` with the **literal** digit (`'2'` -> `command.member = 2`), because the core handler performs its own `member - 1` exactly as the kernel takes `key - '1'`. Digit range, party validity and the `None!`/`Invalid!` outcomes stay entirely in that core handler — no new selection modal. At the harpsichord the digit is intercepted first (R-20). The core/session route accepts a literal `'0'` (clear active player) exactly like any other digit; **on T-Deck hardware specifically**, digits `1`-`9` reach this route, but `'0'` cannot, because the only matrix position that resolves to `'0'` is the physical Mic key, which the device input adapter intercepts unconditionally for short=Cancel/long=Movement-Mode before any digit is ever produced. That device-only gap is tracked separately as **Y-29**. | Covered by `batch3_group_a_test.cpp` A4/A5 (core/session level); Y-29 covers the T-Deck hardware reachability gap |
| Y-21 | Rel Hur scroll | `world_magic.cpp` `case 1` needs `cmd.has_direction`; the device never supplies one for scroll use | Same class as R-11, smaller blast radius |
| Y-22 | Dungeon→combat→dungeon return | `dungeon_combat_return` handles floor delta, escape border and facing; covered by `dungeon_flow_parity` at core level only | Device round-trip test |
| Y-23 | Dungeon keyboard movement with Movement Mode off | No `w`/`d` fallback; only trackball moves | Probably acceptable, but state it as a deliberate contract |
| Y-24 | Word-of-power Yell | **GREEN — DISCHARGED (Batch 3).** Both branches are regression-tested together: `batch3_group_a` A1 (frigate → `YellSails`, no modal), A2 (non-frigate → `YellText`), A3 (word path → `CommandKind::Yell`). | — |
| Y-25 | Shrines | All three modals dispatch correctly; `UiMode::ShrineSpecial` lifecycle (entry, capture, return) repaired in Batch 1 (§5) — no known mode defect remains | Device pass at a shrine (physical-device validation only) |
| Y-26 | Beds / auto-sleep | `CommandKind::AutoSleep` is core-internal (`commands.cpp:1163`), reached through `townAutoSleepTurn` | Confirm it is genuinely internal-only |
| Y-28 | Combat Ready picker close timing | Two presentation gaps that do **not** affect action cost (see section 3 R-06): the empty-handed case opens a disabled `"(None available)"` picker instead of charging immediately without one, and `ItemResult::vanished` (`"Ring vanishes!"`) does not close the picker early the way the reference does | Close the picker from `AlphaRuntime::modal()` on `vanished`, and short-circuit the empty-handed open |
| Y-27 | System menu over an open modal | Menu is handled before gameplay routing and does not touch `ui_->mode()` | Device round-trip from inside a selection/target modal |
| Y-29 | Physical "clear active player" (digit `0`) is hardware-unreachable on T-Deck | **OPEN.** `SetActivePlayer` with `member=0` (clears `active_character` back to `255`, `commands.cpp:747`) requires the literal character `'0'`. On T-Deck the only matrix position that resolves to `'0'` (`kSymbol[0][6]`, per `keyboard_matrix.cpp`) is the physical Mic/0 key, and `UiInputAdapter::translate()` intercepts that exact `column==kMicrophoneKeyColumn && row==kMicrophoneKeyRow` position unconditionally, before any modifier check, to implement short-press=Cancel / long-press=Movement-Mode-toggle. A literal `'0'` therefore never reaches `handle_exploration`'s digit switch on this hardware. Digits `1`-`9` (Set Active Player members 1-6, and Cancel already covered by short-press) remain reachable. See the pre-Batch-4 hardware investigation below for the surveyed alternatives and the recommended (not yet implemented) route. | Implement the recommended Sym+Mic/0 route (see below) once approved, or accept the gap and document it as a deliberate handheld-contract limitation |

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
| `BeginConversation` | **Dead** — blocked by R-10 |
| `BlackthornAction` | **Dead** — blocked by R-09 |
| `UseMoonstone` | **Live** — R-08 resolved (Batch 3): moonstone rows 21-28 are in the Use picker whenever carried |
| `HarpsichordNote` | **Live** — R-20 resolved (Batch 3): digit keys at the harpsichord |
| `YellSails` | **Live** — R-19 resolved (Batch 3): `(Y)ell` aboard a frigate |
| `SetActivePlayer` | **Live at the core/`UiSession` level** — Y-20 resolved (Batch 3): literal digits `0`-`9`. On T-Deck hardware, digits `1`-`9` are physically reachable; literal `0` remains blocked by the Mic/0 special handling — tracked as **Y-29**. |
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
`Sfx` — Y-03. `PoisonTick`, `Quake`, `CellExplosion`, `CellProjectile`, `Refuge`, `TrollSneak` — Y-04.
`NpcInitiatesTalk`, `NpcInitiatesShop` — R-10. `Zodiac` — R-13. `MapReveal` — R-12.

### `DungeonAction` values

`TurnAround` and `Drink` have no producer in `ui_session.cpp`, `alpha_runtime.cpp` or `dungeon_orchestration.cpp`. `MagicUp`/`MagicDown`/`Tick` are correctly core-internal (produced by the Cast handler).

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
| **Viewport blit clips 9 px top and bottom** | `tdeck_board.cpp:628` deliberately reserves those rows for the sky and wind bars. Correct for the world view; wrong for the dungeon view and the gem view, which are full-square compositions. |
| **Dirty-region cache keyed only on `viewport_crc32`** | Sound, but it means any renderer that produces a constant image (e.g. an all-black dungeon frame) will suppress its own redraw. Worth a `force` on presentation-source changes — `dungeon_presentation_pending_` already does this for dungeon entry. |
| **Double `render()` per loop iteration** | `main.cpp` calls `runtime.render(board)` twice per pass (once for `input_dirty`, once unconditionally). Harmless today because `dirty_` gates it, but it doubles the animation-tick path cost. |
| **PSRAM budget** | A* 323 KB + transcript 15 KB + viewport 62 KB + creation canvas 97 KB + tile cache + 8 dungeon arenas. Adding DNG/ITEMS slice atlases (R-05) needs a budget check before implementation. |
| **24 KiB main stack, dedicated input task, keyboard recovery** | All preserved and working. Do not disturb. |
| **SD DMA headroom guard** | `AlphaSaveService::reserve_dma_headroom` + `SdHeadroomGuard` is a good pattern; save/load is the riskiest device operation and it is well defended. |
| **Asset/firmware identity gate** | `packs_match` blocks startup on CRC/SHA mismatch. Excellent — keep it when the asset pack version bumps for R-05. |

---

## 7. TEST-GAP REPORT

### Currently failing

Post-Batch-1 host suite was: **58 total, 57 pass, 1 fail** (`gameplay_parity`, mismatch 59, R-03).

**Post-Batch-2 host suite is: 58 total, 57 pass, 1 fail.** Mismatch 59 is fixed. `gameplay_parity` still fails, but now at a *different* location:

| Test | Status |
|---|---|
| `gameplay_parity` | **FAILING** — mismatch 2034, **R-21** (newly exposed, pre-existing, unrelated to Batch 2 — see §3 R-21). Mismatch 59 (R-03) is **fixed** and no longer the failure. Only failure; all other 57 tests, including `ui_mode_regression` (28/28 GREEN) and the Batch 2 regressions (`presentation_regression`, `combat_loot_open_regression`, `combat_escape_regression`, `direct_troll_handoff_regression`), pass. |

**Post-Batch-4 addendum (superseded by the harness-correction pass below):** the authoritative suite grew from 61 to **63** registered tests (`batch4_group_a`, `batch4_group_b`). Initial result: 63 total, 60 pass, 3 fail — `gameplay_parity` mismatch 2034 (R-21, unchanged) plus one internal defect each in `batch4_group_a` (its A8 case, 4 checks) and `batch4_group_b` (one check in B5), both test-authoring defects, not production regressions (see R-09 above for the original analysis).

**Post-Batch-4 harness-correction pass:** A8 and B5 were independently re-derived from the production semantics and test code (not assumed correct from the prior report), confirmed to be the test-authoring defects described above, and corrected — not weakened, not deleted — to test the real invariants: A8 now drives a real trigger + real modal answer for each of the four Blackthorn/guard families before asserting `Pass` is unblocked (Blackthorn's case adds a minimal content-free `ShrineServices` fixture so its real state machine can conclude on host); B5 now respects the `AlphaRuntime`-vs-host-layer boundary — it no longer routes through `UiSession::consume()` (which B1–B4 already prove end-to-end) and instead closes the identity-lifetime loop by dispatching the re-resolved `(slot, location)` identity as a real `BeginConversation` and confirming it opens the correct session. No production code changed in this pass. Result: **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). No Batch 4 test failures remain.

`ui_mode_regression` is new, narrow device-glue coverage added in Batch 1 — it host-tests mode arbitration and `UiSession` mode ownership via the `ui_mode_policy.h` seam. It does not make `AlphaRuntime` as a whole host-tested; see Y-05 below.

### False-confidence tests

| Test | Why it over-reports |
|---|---|
| `ui_session_tests` | Uses a `Spy` dispatcher that records `UiIntent`s and never executes them. Proves `UiSession` routing; proves **nothing** about `AlphaRuntime::dispatch`, `modal()`, `open_selection` or `synchronize_after_debug` — i.e. exactly where R-01, R-06, R-07, R-08, R-09, R-11 live. **Partly mitigated (Batch 3):** the Use-picker row-selection logic was extracted into the ESP-free `openu5::usable_item_picker_rows()` seam that `AlphaRuntime::open_selection()` and `batch3_group_b_tests` now both call, so that slice of `open_selection` is host-testable. `batch3_group_a_tests` drives the real `UiSession::handle_input()` -> `dispatch()` path for the sails/digit/harpsichord routes; `batch3_group_c_tests` drives the real `execute_command()` path for Ready in all three contexts. |
| `device_smoke_tests` (37 scenarios) | Cases 3,4,5,9,10,12,13,15,18,19,20,21,24,26,27,28,29 are **data-presence assertions** ("is the table non-empty", "is the name non-null"). Cases 2,6,7,8,11,17,22,25 are `UiSession`-with-spy probes. Case 16 composes a snapshot but asserts only a hash. **No scenario validates a rendered frame, a mode round-trip, or a full input→state→presentation chain.** |
| `presentation_regression` *(historical — gap closed in Batch 2)* | At the original audit baseline, asserted snapshot composition but never checked that the chosen tile was the *right* tile for the object kind, which is why R-02 and R-04 survived. Batch 2 added R03-OUTDOOR, R04-OVERLAY and R04-LIFO cases that close this gap; the test now asserts both the correct tile and the correct layering. |
| `combat_loot_open_regression` *(historical — gap closed in Batch 2)* | At the original audit baseline, covered Open→pile→Get authoritative mutation but did **not** assert that no world object is created on exit — R-01/R-03's blind spot. Batch 2 added the R03-DIRECT case that closes this gap. |
| `debug_developer_test`, `debug_map_picker_test` | Strong (they assert no turn/RNG consumption), but they exercise the debug path, which `synchronize_after_debug` *serves correctly*. They therefore cannot expose R-01. |
| `frontend_test` | State machine only; no view/art assertions. |

### Genuinely strong tests — preserve

`combat_parity`, `advanced_combat_parity`, `command_parity`, `item_parity`, `magic_parity`, `turn_parity`, `travel_parity`, `shop_parity`, `dialogue_parity`, `quest_parity`, `dungeon_parity`, `dungeon_flow_parity`, `movement_flow_parity`, `transport_flow_parity`, `world_flow_parity`, the `typescript_*_drift` fixture-freshness suite, and `input_regression`. These are the reason the core is trustworthy.

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
| **World objects (`objects_`)** | ✗ | ✗ | leaks across loads | **R-14** |
| **Dungeon session** | ✗ | cleared | base mode re-derived next input | **R-15** |
| Combat session | ✗ | cleared | ✓ | **N** (matches reference) |
| Dialogue / shop / shrine / Blackthorn sessions | ✗ | cleared | ✓ | **N** (session-only, correct) |
| Settings (brightness, movement mode, trackball, UI size) | ✓ (separate `settings.json`) | ✓ | ✓ | **G** |

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
| `Dungeon` | `dungeon_.active && context_.dungeon` | `render_dungeon_view` | `execute_dungeon_command` | — | **R-05** (art) |
| `Combat` | `context_.combat && combat_.initialized` | `render_snapshot(compose_combat_presentation)` | combat commands | quick escape | **G** logic / icons — R-04 resolved (Batch 2, `compose_world_presentation` two-layer fix) |
| `Shop` | `shop_.phase != Closed` | world/combat viewport + `DeviceShopView` overlay | `execute_shop` | hierarchical | **G** — R-01 resolved (Batch 1) |
| `Dialogue` | `dialogue_` session | viewport + transcript | dialogue commands | `EndConversation` | **G** — R-01 resolved (Batch 1) |
| `ShrineSpecial` | shrine/Blackthorn session | viewport only | modal resolution → `shrine_return_mode_` | originating world mode | **G** — resolved (Batch 1) (§5) |
| `TextEntry` / `NumericEntry` / `YesNo` | request-specific | viewport + prompt | `finish_modal` | `return_mode_` | **G** |
| `PartySelection` / `InventorySelection` / `EquipmentSelection` / `SpellSelection` | none | viewport + `DeviceSelectionView` | `modal()` | `return_mode_` | **G** routing / **G** content (R-07/R-08 resolved, Batch 3) |
| `TargetSelection` | combat for aim; world for Fire | viewport + reticle (`snapshot.target_*`) | direct command | `return_mode_` | **G** |
| `DebugMenu` | developer build | `DeviceDebugScreen` (viewport suppressed) | `UiDebugMenu` | `debug_return_mode_` | **G** |
| *gem view overlay* | `gem_view_active_` | `render_world_gem_view` / `render_dungeon_gem_view` | any key closes | prior mode + `AfterGemView` | **R-17** |

**Presentation source selection** (`alpha_runtime.cpp` `render()`) is a clean single decision — `combat ? combat : dungeon ? dungeon3d : world`, overridden by the gem view. It logs `PRESENTATION_DISPATCH` every frame. This part is well built; the problems are the *contents* of two of the branches and the *mode* that selects them.

**Impossible combinations found:** none in the renderer. The previously identified Shop/base-mode and ShrineSpecial lifecycle contradictions were resolved in Batch 1.

---

## 11. RESOURCE / ITEM MATRIX

**Note on the `View` column (post-R-22):** "Z-stats (R-22)" below means the reference exposes this resource on a `(Z)` status page, but native `(Z)` currently implements only the member picker — see R-22. The `Status` column separates the resource's own storage/use functionality (which is fine) from that missing presentation route.

| Resource | Get | Use | Ready | View | Save | Status |
|---|---|---|---|---|---|---|
| Gold | ✓ | n/a | n/a | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Food | ✓ | n/a (auto-consumed) | n/a | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Gems | ✓ | n/a | n/a | **`V`** | ✓ | **R-17** |
| Keys | ✓ | via Jimmy/Open | n/a | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Torches | ✓ | `I`gnite | n/a | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Reagents ×8 | shop | `M`ix | n/a | Z-stats (R-22) | ✓ | **Y-19** storage/use / R-22 view |
| Equipment ×48 | ✓ / shop | n/a | **`R`** | Z-stats (R-22) | ✓ | **G** world, dungeon and combat (R-06 resolved, Batch 3) / R-22 view |
| Armour / helmets / shields | ✓ | n/a | ✓ | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Weapons / ammo | ✓ | n/a | ✓ (ammo checked) | Z-stats (R-22) | ✓ | **G** storage/use / R-22 view |
| Potions ×8 (ids 8–15) | ✓ | ✓ + party target | n/a | picker | ✓ | **G** |
| Scrolls ×8 (ids 0–7) | ✓ | ✓ | n/a | picker | ✓ | **Y-21** (Rel Hur needs a direction) |
| Spells ×48 | `M`ix | `C`ast | n/a | picker + summary; Z-stats Spells page (R-22) | ✓ | **R-11**, **R-16** / R-22 view |
| Magic Carpet (16) | quest | ✓ | n/a | picker | ✓ | **Y** |
| Skull Key (17) | quest | ✓ | n/a | picker | ✓ | **Y** |
| Amulet (18) | quest | ✓ | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-07 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Crown (19) | quest | ✓ | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Sceptre (20) | quest | ✓ | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Moonstones (21–28) | quest | ✓ | n/a | picker (gated on `!buried`); Z-stats Items page (R-22) | ✓ | **G** picker — R-08 resolved (Batch 3); persistence still **R-14**; R-22 Z-stats Items-page view still missing |
| Shards (29–31) | quest | ✓ | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Spyglass (32) | quest | ✓ route | n/a | picker; Z-stats Items page (R-22) | ✓ | **R-13** (no zodiac view) / R-22 Z-stats Items-page view still missing |
| Plans (33) | quest | ✓ | n/a | picker (gated on `hms_cape`); Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Sextant (34) | quest | ✓ | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker / R-22 Z-stats Items-page view still missing |
| **Watch (35)** | **no owner anywhere** | **absent** | n/a | **absent** | ✓ | **OPEN** — deliberately excluded by Batch 3: no `GameState`/`QuestState`/`QuestWorldServices` field backs id 35, so no possession gate exists. Inventing one was out of scope. |
| Badge (36) | quest | ✓ | n/a | picker (gated on `black_badge`); Z-stats Items page (R-22) | ✓ | **G** Use-picker — R-08 resolved (Batch 3) / R-22 Z-stats Items-page view still missing |
| Wooden Box (37) | quest | ✓ ("How?") | n/a | picker; Z-stats Items page (R-22) | ✓ | **G** Use-picker / R-22 Z-stats Items-page view still missing |
| Grapple | quest | **Klimb-only, never a Use item** | n/a | — | ✓ | **G** — R-07 resolved (Batch 3): removed from the Use picker entirely |
| Loose loot piles | ✓ LIFO | n/a | n/a | rendered `0x100+id` | **✗** | **R-14** |
| World chests | Open→piles | n/a | n/a | correct sprite (was **tile 1 = blue**; R-02 resolved Batch 2) | **✗** | R-02 **G** / **R-14** |

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
| 8. Visibility | **Y** | `dungeon_wall` on the forward scan uses unmasked coordinates then `&7` inside — consistent, but the 4-deep scan can wrap around the 8×8 torus and report a wall from "behind" |
| 9. Geometry | **Y** | Corridor depth + two side tests. No door/alcove/passage distinction (reference has 4 slice families: wall 0–3, door 4–7, open passage 16–19, alcove 20–23) |
| 10. **Authored textures** | **R-05** | **Not in the asset pack** |
| 11. Wall/floor/ceiling | **R-05** | 5 hardcoded RGB565 constants vs. baked-in speckle+ceiling per slice; no per-dungeon wall variant (`DUNGEON:0x0e7b` selects DNG1 olive / DNG2 red / DNG3 grey) |
| 12. Viewport composition | **Y** | 176×176 correct, but the top/bottom 9 px are overdrawn by HUD bars that do not belong over a dungeon view |
| 13. HUD integration | **Y** | `hud_world_state(..., dungeon_active=true)` hides sky/wind marks; the bars themselves still paint |
| 14. Movement (fwd/back) | **G** | `DungeonAction::Forward`/`Back`; `dungeon_flow_parity` green |
| 15. Turning | **G** | `Left`/`Right` |
| 16. Backward movement | **G** | Distinct action, does not turn |
| 17. Blocked behaviour | **G** | Same `dungeon_wall` rule as the renderer — consistent |
| 18. Stairs / ladders | **G** logic / **R-05** art | `dungeon.cpp:174-181` up/down with floor bounds → `ExitSurface`/`ExitUnderworld` |
| 19. Pits | **G** logic / **R-05** art | Damage script path present |
| 20. Fields / traps | **G** logic / **R-05** art | Cast-created fields at `dungeon_orchestration.cpp:165` |
| 21. Objects | **Y** | `here` cell feature drawn as a 12×12 green/red blob |
| 22. Encounters | **G** | `DungeonEncounters` + `dungeon_arenas_` bound to all combat maps |
| 23. Dungeon combat | **G** | `start_room` / `start_corridor` with `corridor_cause` |
| 24. Combat return | **Y-22** | `dungeon_combat_return` handles floor delta, escape border and facing; core-tested only |
| 25. Dungeon exit | **G** | Walking out at the level-1 entry cell → `exit_dungeon(false)` → `DungeonExited` → `set_base_mode(Exploration)` |
| 26. Underworld transition | **G** | `exit_dungeon(true)` from floor ≥ 8 |
| 27. System menu round-trip | **Y-27** | No device proof |
| 28. Developer menu round-trip | **G** | `debug_return_mode_` restores Dungeon |
| 29. **Save/load in dungeon** | **R-15** | Session dropped silently |
| 30. Movement Mode controls | **G** | W/S/A/D = forward/back/turn-left/turn-right — **already matches the requested target** |
| 31. Trackball | **G** | up/down/left/right = forward/back/turn-left/turn-right |
| 32. Keyboard (Movement Mode off) | **Y-23** | No movement keys; verbs only |
| — | R-06 **G** | `Ready` offered and applied (RESOLVED, Batch 3) |
| — | **R-17** | Dungeon gem view is a synthetic flood fill |

**Bottom line:** the dungeon's *logic* is in good shape and its *controls are already correct*. The unusable view is an **asset-pipeline gap**, not a control or state bug. Do not "fix" the controls.

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
| Search-revealed quest objects | ✓ `quest_search.cpp` + fixtures | ✓ `S` | **R-14** (lost on reload) |
| Lord British progression / karma | ✓ `dialogue_effects` | ✓ Talk | **Y** |
| Harpsichord passage (Cove) | ✓ `play_harpsichord` | ✓ digit keys while seated | **G** — R-20 resolved (Batch 3) |
| Moongates / moonstones | ✓ `use_moonstone`, `transitions.cpp` | ✓ Use picker ids 21–28 when carried | **G** reachability — R-08 resolved (Batch 3); persistence still **R-14** |
| HMS Cape | ✓ id 33 | ✓ Use picker id 33 | **G** — R-08 resolved (Batch 3) |
| Codex / endgame script | ✓ `EndgameScript`, `GameWon`/`Endgame` events appended to transcript | no longer gated — the Use chain above is reachable | **Y** — R-08 resolved (Batch 3); end-to-end endgame still unproven |
| Underworld | ✓ `exit_dungeon(true)` | ✓ | **G** logic |
| Refuge / camp scenes | ✓ `RefugeScript` | event unconsumed | **Y-04** |

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

### Batch 5 — World spell targeting · risk: medium
**IDs:** R-11, Y-21
**Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (`cast_selected_spell`)
**Work:** remove the `context_.combat &&` guard; open a direction/target prompt in the world for `direction`, `selectedMapUnit`, `selectedMapPosition`; route the Rel Hur scroll through the same prompt.
**Care:** `cast_spell` consumes the charge *before* the effect runs. The prompt must come **before** dispatch (it does — `begin_target` holds a `pending_command_`), so cancelling must not consume. Verify against `magic_parity`.
**Physical test:** An Sanct on a locked door; In Por blink; An Ex Por on a door.
**Model:** Opus-level — the consume-ordering interacts with the reference's bug-for-bug charge semantics.

---

### Batch 6 — Persistence gaps · risk: medium
**IDs:** R-14, R-15
**Files:** `native/core/src/gameplay_save.cpp`, `native/core/include/openu5/gameplay_save.h`, `native/targets/tdeck/main/alpha_runtime.cpp` (`synchronize_loaded_world`)
**Work:** add a `worldObjects` sidecar array with capture/restore; add `objects_.clear()` on load; decide and then implement the dungeon-save policy (serialize `DungeonState`, or refuse to save underground with a reference-appropriate message).
**Physical test:** open a chest, walk away, save, reload, confirm the loose loot is still there; search a tree for the skull key, reload, confirm; save in a dungeon and observe the documented behaviour.
**Model:** Sonnet, with an explicit reference decision from the user on the dungeon-save question.

---

### Batch 7 — Missing feedback renderers · risk: low
**IDs:** R-12, R-13, Y-04
**Files:** `native/targets/tdeck/main/native_renderer.cpp`, `alpha_runtime.cpp` (`consume_event`)
**Work:** consume `MapReveal`, `Zodiac`, `Quake`, `CellExplosion`, `CellProjectile`, `PoisonTick`, `Refuge`, `TrollSneak`. `start_magic_ceremony` is the proven template for a timed, async, non-blocking effect.
**Physical test:** cast Wis An Ylem; use the spyglass; destroy a shard.
**Model:** Sonnet.

---

### Batch 8 — Spell metadata adjudication · risk: low, but **research-heavy**
**IDs:** R-16
**Files:** `native/core/src/magic.cpp` (`kSummaries`, `kTargets`), possibly `magic_tables.inc` (`kEffects[26]`, `kEffects[28]`)
**Work:** regenerate both description tables from `MagicDefinitions.json::SimpleDescription` and from `SpellDef::target_type` rather than maintaining them by hand. **Separately** adjudicate In Ex Por (Animation vs. unlock) and In Zu (Line vs. sleep) against `re/disasm` before touching `kEffects` — these change behaviour, not just text. Also bound `SpellId::Nox`.
**Physical test:** open the spell picker and read every entry against the reference.
**Model:** Opus-level for the two effect-table questions; the table regeneration itself is mechanical.

---

### Batch 9 — Dungeon presentation (ANCHOR 4) · risk: **high**, largest scope
**IDs:** R-05, plus dungeon rows 8–12, 21
**Files:** `extractor/src/parsers/dngtiles.ts` (exists, reuse), `native/tools/u5pack/alpha1.ts`, `native/ASSETS.md`, `native/targets/tdeck/main/asset_pack.cpp`, `native/targets/tdeck/main/native_renderer.cpp`
**Work, in order:**
1. PSRAM budget for the slice atlas (28 slices × 3 variants + 20 features) — do this **first**; it may force a streaming reader instead of a cache.
2. Extend the pack format (version bump; the `packs_match` gate will correctly block old cards).
3. Slice/feature readers in `asset_pack.cpp`.
4. Replace `render_dungeon_view` with the reference's blitter: `SIDE_X[side][depth]` at native height 164 / Y=14, left normal + right mirrored, 4 abutting rings 16→96→176, back wall as a mirrored pair with the `0x60` override, features as mirrored half-images from ITEMS.16.
5. Wall variant from the dungeon id.
6. Stop overdrawing the sky/wind bars in the dungeon and gem-view presentation sources.
**Physical test:** the full dungeon block of §16.
**Model:** Opus-level, and worth its own session. This is the single largest remaining piece of work in the project.

---

### Batch 10 — View Gem presentation · risk: low
**IDs:** R-17, Y-14
**Files:** `native/targets/tdeck/main/native_renderer.cpp`, `tdeck_board.cpp` (bar overdraw)
**Work:** replace the bit-test colouring with a real terrain-category map derived from the same classification the reference's `buildGemView` uses; stop clipping the gem view. **Do this after Batch 9** so both share the "full-square presentation sources must not be HUD-clipped" fix.
**Physical test:** `V` with gems on the overworld and in a dungeon; confirm the map is legible and that closing it charges exactly one turn.
**Model:** Sonnet.

---

### Batch 11 — Test infrastructure · risk: low
**IDs:** Y-05, Y-06, and everything in §15
**Files:** new `native/targets/tdeck/host_tests/runtime_integration_test.cpp` + a thin `AlphaRuntime` seam
**Work:** see §15. Without this, every batch above is verified only by hand.
**Model:** Opus-level for the seam design (extracting the routing/mode logic from ESP-IDF dependencies); Sonnet for the tests themselves.

---

### Batch 12 — Scroll-use event/message/SFX divergence (R-21) · risk: **unknown — investigation required** · **NOT STARTED**
**IDs:** R-21 (RED, OPEN, newly exposed)
**Files:** unknown — not yet investigated. Likely candidates given the observed symptom (scroll-use message text and missing `sfx`/`unknown` events) are the native scroll-use handler and its message/event tables, but this is a guess, not a finding, and must not be treated as one.
**Work:** first, root-cause `gameplay_parity` mismatch 2034 the same way R-02/R-03/R-04 were root-caused in Batch 2 — trace the exact native code path for the failing sequence, inspect the generated mismatch artifact, and adjudicate against the authoritative reference before proposing a fix. Do not assume this shares a cause with R-16 (spell description/label mismatches) — R-16 concerns spell descriptions, not scroll-use event output — unless investigation proves otherwise.
**Verify:** `ctest -R gameplay_parity` must go green with **no new mismatch surfacing** past 2034, the same hard pass/fail gate pattern used for Batch 2.
**Physical test:** not yet determined pending root cause.
**Model:** Sonnet/Opus for investigation, depending on what the root cause turns out to be — undetermined until the investigation phase runs.

---

### Batch 13 — Z-stats status/inventory pages · risk: medium, moderate scope
**IDs:** R-22
**Files:** `native/core/src/ui_session.cpp` (`case 'z'` / `UiIntentKind::OpenStatusSelection`), `native/targets/tdeck/main/alpha_runtime.cpp` (member-select consumption), `native/targets/tdeck/main/native_renderer.cpp` (new page rendering)
**Work:** design and add the real per-member status-page axis behind the existing member picker: Stats, Arms, Provisions, Reagents, Spells, Items (including the quest/special possessions — Amulet, Crown, Sceptre, Shards, Moonstones, HMS Cape plans, Spyglass, Sextant, Black Badge, Wooden Box), and Armaments, matching the reference page-by-page. This is a self-contained, moderate-scope UI addition; it does not block or depend on Batch 9-12.
**Physical test:** `Z`, select each member, page through all seven pages, confirm quest/special items appear exactly when owned.
**Model:** Sonnet for the mechanical per-page wiring; a short design pass first to settle page navigation (which key pages forward/back, whether it nests under `OpenStatusSelection` or is a new `UiMode`).

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
| 14 | Every modal `UiRequestId` opened by `consume` is handled by `finish_modal` **or** `AlphaRuntime::modal` | runtime — **catches R-09** |
| 15 | Every `GameEventKind` has a consumer or an explicit `IGNORED` entry | runtime — catches R-10, R-12, R-13 |
| 16 | Every `View` resource handler is reachable from the UI | runtime |
| 17 | Every dungeon id 33–40 loads valid map data | already smoke 18; promote to host |
| 18 | Every spell has a target label consistent with its `target_type` | `magic_parity` — **catches R-16** (12 failures today) |
| 19 | Every spell summary equals `MagicDefinitions.json::SimpleDescription` | new drift test — catches R-16 |
| 20 | Every serialized field round-trips, and **every live owner is serialized** | `persistence_driver` — **catches R-14, R-15** |
| 21 | Every `CommandKind` is either produced by a UI route or annotated core-internal | static test — R-19, R-20 and Y-20 are now routed (Batch 3). `CombatYield` is also live and routed: `UiSession::cancel_modal()` emits it, and `commands.cpp`'s generic `CombatMove..CombatEnemyStep` range dispatch (`static_cast<CombatAction>(int(cmd.kind) - int(CommandKind::CombatMove))`) carries it straight through to `combat.cpp`'s `CombatAction::Yield` handler — see `native/core/tests/batch3_group_c_test.cpp` C13. The remaining §5 dead/annotated-only list is `Unready`, `CombatEscape`, `ShopAction`, `BeginConversation` (R-10), `BlackthornAction` (R-09) |
| 22 | `Ready` succeeds in world, combat and dungeon | **DONE (Batch 3)** — `batch3_group_c` C1/C2/C3/C4 through the real `execute_command()` path, plus the combat `CombatActor` cache invariant |

### The structural change that makes most of these possible

`AlphaRuntime` cannot be host-tested because it includes `esp_*` headers and `tdeck_board.h`. Extract the **routing and mode logic** — `dispatch`, `modal`, `open_selection`, `cast_selected_spell`, `synchronize_*`, `consume_event` — into a `GameplayController` class in `native/core/` that takes the owners by reference and has no ESP dependency. `AlphaRuntime` then becomes board/render/storage glue over it. That one refactor moves roughly 500 lines of the highest-risk code into the existing, well-served host test harness.

**Scope note:** this is a real refactor, so it is proposed, not performed, and it is Batch 11 rather than a prerequisite.

---

## 16. PHYSICAL T-DECK CERTIFICATION PLAN

Efficient broad-coverage pass using Developer tools. ~45 minutes. Each step names the log line that proves it.

### Phase 0 — Boot and frontend (5 min)
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
14. `Z`-stats: **expected/blocking on R-22** — native does not yet implement the Stats/Arms/Provisions/Reagents/Spells/Items/Armaments page family, so this step cannot page through Provisions and Stats today. Current expected behavior: `Z` opens the member picker; selecting a member reopens/repositions the same picker with no further page. Confirm only the party highlight and picker reachability; do not expect Provisions/Stats pages until R-22 is implemented (§14 Batch 13).
15. **[R-01] Enter a blacksmith. Buy an item. Back out one level at a time. Confirm the world view and Exploration verbs return.** Repeat at an inn (Rest), a healer (Heal), a tavern (Rations + Rumour) and a reagent shop.

### Phase 3 — Combat and victory loot (7 min)
16. Debug → Max Party, Max Resources, Equip Best Gear.
17. Trigger an overworld encounter. Move, `A`ttack with the reticle, `F`ire, `C`ast a combat spell.
18. **[R-06] Press `R` in combat, select a weapon. Confirm the weapon actually changes** — and that the character then *attacks with it* (the `CombatActor` cache resync). Fixed in Batch 3; hardware confirmation outstanding.
19. Win. **Confirm the arena stays open.** `O`pen the chest, `S`earch it, `G` + direction repeatedly to zero.
20. **[ANCHOR 1 — Batch 2 regression-validation] Confirm the cell shows plain arena floor — no blue square, no leftover symbol.** (Code/test-level fix is GREEN as of Batch 2; this step is the outstanding hardware confirmation, not a check for a known-open defect.)
21. **[ANCHOR 2 — Batch 2 regression-validation] Before each `G`, note the visible icon; confirm the message names the same item.** (Code/test-level fix is GREEN as of Batch 2; this step is the outstanding hardware confirmation.)
22. Mic (Back) → canonical victory exit. **[ANCHOR 1 — Batch 2 regression-validation] Confirm no blue tile-1 cell at the encounter coordinate on the overworld.** (`gameplay_parity` mismatch 59 is fixed at the code/test level; this step is the outstanding hardware confirmation, not a check for a known-open defect.)
23. Save, reload. **[R-14] Confirm any loot left behind is still there.**

### Phase 4 — Items, magic, view (6 min)
24. `U`se: **[R-07/R-08]** confirm every owned tool is listed with the correct name; confirm "Grapple" is absent and the Amulet is present. Fixed in Batch 3; hardware confirmation outstanding. (Pocket Watch is expected to be absent — no backing state.)
25. `U`se a potion on a party member. `U`se a scroll.
26. `C`ast Mani on a companion. `M`ix a spell.
27. **[R-11]** Cast An Sanct in the world at a locked door. Confirm it does something and does not silently eat the charge.
28. **[ANCHOR 3]** `V` with gems. **Photograph the screen.** Confirm the gem count decremented, a legible map appeared, and closing it charged exactly one turn (`VIEW_EFFECT` / `VIEW_RESULT` in the log).

### Phase 5 — Transport (4 min)
29. Debug → Transport → Ship. `B`oard. **[R-19] `Y`ell → confirm HOIST/FURL, not a word prompt.** Sail with the wind. Fixed in Batch 3; hardware confirmation outstanding.
30. `X`-it. Board a horse, a carpet, a skiff. Confirm the avatar sprite changes each time.
31. Save, reload aboard the ship; confirm transport and hull survive.

### Phase 6 — Dungeon (10 min) · *the big one*
32. Debug → Teleport → Dungeon (Deceit), floor 0, standard entry. Confirm `DUNGEON_SESSION_STATE active=1` **and** a usable view.
33. **[ANCHOR 4] Photograph the view.** Confirm authored wall texture, correct variant colour, and that doors read differently from walls.
34. Movement Mode ON: `W` advances, `S` backs up, `A`/`D` turn. Walk into a wall; confirm the blocked response.
35. Trackball: same four.
36. Find stairs, `K`limb down. Confirm the depth readout and a new floor.
37. Walk into a pit; walk into a field. Confirm the damage message and the feature art.
38. `S`earch, `O`pen, `G`et, `J`immy in the dungeon.
39. `C`ast Uus Por / Des Por. **[R-06] `R`eady in the dungeon** — confirm it applies and charges no turn. Fixed in Batch 3; hardware confirmation outstanding.
40. Trigger a dungeon encounter. Win. **Confirm the return is to the same cell and facing.**
41. `Alt+M` System Menu → close. `Alt+D` Developer → Back. **Confirm the dungeon view returns both times.**
42. **[R-15]** Save inside the dungeon, reload, and record exactly what happens.
43. Descend past floor 7 → Underworld. Confirm the transition.
44. Walk out at the level-1 entrance → surface. Confirm the world view and Exploration verbs.

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

## APPENDIX — Audit artifacts

**Test run (ORIGINAL AUDIT BASELINE RUN, pre-Batch-1):** `ctest` in `native/core/build-alpha20-host`, 57 tests, **56 passed / 1 failed** (`gameplay_parity`, 61.9 s). Mismatch artifact retained at `native/core/build-gameplay/mismatch.json`. This run predates Batch 1 and is preserved as historical evidence. At this point in the program, mismatch 2034 (R-21) had not yet been observed — it was unreachable behind mismatch 59 — and this record is preserved as-is rather than rewritten with knowledge that did not exist at the time.

**Status (post-Batch-1, historical):** 58 total, 57 pass, 1 fail — `gameplay_parity` failing at mismatch 59, plus the new `ui_mode_regression` suite (28/28 GREEN). See §14 Batch 1. As with the baseline run above, mismatch 2034 was not yet reachable or known at this point and this record is preserved unchanged.

**Historical status (post-Batch-2, superseded):** 58 total, **57 pass, 1 fail**. R-02, R-03 and R-04 are GREEN; `gameplay_parity` mismatch 59 is **fixed**. The sole remaining failure is `gameplay_parity` at **mismatch 2034 (R-21)** — a scroll-use event/message/SFX divergence that was masked by mismatch 59 until now, confirmed pre-existing (reproduces on the untouched `63eeac3b` baseline) and out of scope for Batch 2. `ui_mode_regression` (28/28 GREEN, Batch 1) and all Batch 2 regressions (`presentation_regression`, `combat_loot_open_regression`, `combat_escape_regression`, `direct_troll_handoff_regression`) pass. T-Deck ESP-IDF build: PASS. Hardware flash: **not performed**. See §14 Batch 2 and §3 R-21. This entry is preserved as historical record of the program's state at that point and is superseded by Batch 3 below; it is not rewritten with later knowledge.

**Status (post-Batch-3):** the host ctest suite is substantially larger than the post-Batch-2 snapshot above (61 registered tests in the authoritative `native/core/build-batch1-control` acceptance tree, up from 58), reflecting Batch 3's new `batch3_group_b`/`batch3_group_c` suites and the TypeScript drift/fixture tests. (A stale, non-authoritative `OPENU5_REAL_ARENAS=ON` build tree was briefly observed reporting 65 tests during this window; that count came from a build configuration different from the Batch acceptance tree and was never the authoritative figure — the number to cite for Batch acceptance is always the `build-batch1-control` count.) R-06 (`Ready` rejected in combat/dungeons), R-07 (Use-picker id 18 mis-binding), R-08 (endgame Use chain unreachable), R-19 (`(Y)ell` has no frigate branch) and R-20 (harpsichord melody unreachable) are all **GREEN — RESOLVED (Batch 3)**; see §3 for each. `CombatYield` is confirmed live and routed end-to-end through the real `execute_command()`/`combat.cpp` path, not just emitted by `UiSession` (§15 invariant 21; `native/core/tests/batch3_group_c_test.cpp` C13). The sole known failure remains `gameplay_parity` at mismatch 2034 (R-21, still RED/OPEN — untouched by Batch 3, tracked as Batch 12). Batch 3 did change production code (§14 Batch 3 file list), and T-Deck ESP-IDF 6.1 firmware was rebuilt and passed as part of that same Batch 3 validation: `openu5_tdeck.bin` was approximately `0xc63f0` bytes, with 23% of the app partition free and zero compiler warnings. Hardware flash was not performed at that Batch 3 validation point. See §3 R-06/R-07/R-08/R-19/R-20/R-21 and §14 Batch 3.

**Status (pre-Batch-4 Developer-tool cleanup, historical, superseded by Batch 4 below):** Batch 3 is committed and pushed to `main`. The work-in-progress on top of it at that point was a small, self-contained Developer-tool correction — **R-24** (`DebugPreset::Combat` no longer silently engages Set Active Player) — plus its regression coverage, three new evidence-only audit findings (R-22 Z-stats, R-23 Blackthorn roster gap, Y-29 physical clear-active-player hardware reachability), and this document's own text corrections. No gameplay behavior other than the R-24 Developer-preset fix changed. The authoritative `native/core/build-batch1-control` suite was **61 total, 60 pass, 1 fail** — the sole failure was `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing). T-Deck ESP-IDF firmware was rebuilt clean for this pass because `DebugPreset::Combat`'s production behavior changed. See §3 R-24 for the fix and evidence.

**Status (post-Batch-4 implementation, historical, superseded by the harness-correction pass below):** R-09 and R-10 became **GREEN — RESOLVED**; see §3 for the full analysis, including the correction of the original R-09 write-up (severity was understated; the `CrystalBall`/`WellDrop` "correctly handled" claims were wrong and are now tracked as R-25/R-26, discovered but explicitly left unfixed). The authoritative suite grew from 61 to **63** registered tests and, at that point, reported 63 total, 60 pass, 3 fail: `gameplay_parity` mismatch 2034 (R-21, unchanged, pre-existing) plus one internal defect each in `batch4_group_a` (its A8 case) and `batch4_group_b` (one check in B5) — both test-authoring defects, not production regressions (see §7 addendum and §3 R-09 as originally written). A pass mid-implementation briefly introduced a real regression — `quest_parity` mismatch 4821 — by placing the Blackthorn "Your response?" prompt suffix in the core event text instead of `UiSession`'s presentation layer; this was caught by the parity fixture itself, reverted, and re-implemented presentation-side. T-Deck ESP-IDF 6.1 firmware was rebuilt and **passed**: `openu5_tdeck.bin` grew from `0xc63f0` to `0xc67c0` bytes (+976 bytes), app partition free space `23%` → `22%`, zero compiler warnings.

**Status (post-Batch-4 harness-correction pass):** A8 and B5 were independently re-adjudicated from the production semantics and test code, confirmed as test-authoring defects (not assumed from the prior report), and corrected in `native/core/tests/batch4_group_a_test.cpp` / `batch4_group_b_test.cpp` only — no production code changed, so the firmware certification above remains valid unchanged. Both tests now prove the real invariants without weakening coverage: A8 drives a real trigger + real modal answer for all four Blackthorn/guard families before asserting the world unblocks (Blackthorn's case adds a minimal content-free `ShrineServices` test fixture so the real state machine can conclude on host); B5 no longer asserts a state production never produces (routing through `UiSession::consume()` while expecting the session to stay inactive) and instead proves the identity-lifetime invariant end-to-end: capture, survive, re-resolve without the borrowed pointer, and successfully open the correct session via `BeginConversation`. The authoritative suite now reports **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `batch4_group_a` is 32/32 and `batch4_group_b` is 12/12. See §3 R-09/R-10/R-25/R-26 and §14 Batch 4.

**Probe:** a throwaway program linked against `libopenu5_core.a` verified R-06 and R-16 directly. It lives in the session scratchpad, **not** in the repo — it is an audit instrument, not a test. Its R-06 assertions are now carried permanently by `native/core/tests/batch3_group_c_test.cpp` (invariant 22, §15); R-16 remains probe-only.

**Working tree, historical (Batch 3, committed/pushed):** at the time of that document update, the tree contained the completed Batch 3 implementation and tests — production changes in `native/core/include/openu5/combat.h`, `native/core/include/openu5/inventory_picker.h`, `native/core/include/openu5/ui_session.h`, `native/core/src/combat.cpp`, `native/core/src/commands.cpp`, `native/core/src/display_names.cpp`, `native/core/src/inventory_picker.cpp`, `native/core/src/ui_session.cpp`, `native/targets/tdeck/main/alpha_runtime.cpp` and `native/targets/tdeck/main/alpha_runtime.h`; test changes in `native/core/tests/batch3_group_b_test.cpp` and `native/core/tests/batch3_group_c_test.cpp` (the latter now also carries the C13 real-routing `CombatYield` regression guard) — plus that document update. That Batch 3 tree has since been committed and pushed to `main`.

**Working tree (current, pre-Batch-4 Developer-tool cleanup, not committed):** on top of the committed/pushed Batch 3 baseline, the current uncommitted working tree contains only the small R-24 Developer-tool fix and its supporting changes: `native/core/src/debug_developer.cpp` (production fix), `native/core/tests/debug_developer_test.cpp` (regression coverage), `native/core/src/turn.cpp` (a documentation-only comment on the reference-faithful troll dexterity check; no logic changed), and this document (R-22/R-23/R-24/Y-29 findings, Batch 13 plan entry, and the text corrections in this section). No other production code was modified.

**Status (post-Batch-4.5A-1, Developer Teleport Default Entrance fix):** R-27 became **GREEN — RESOLVED**; see §3 R-27 for the full root cause, fix, and per-location evidence. The authoritative `native/core/build-batch1-control` suite reports **63 total, 62 pass, 1 fail** — sole failure `gameplay_parity` mismatch 2034 (R-21, unrelated, pre-existing, unchanged). `debug_map_picker` is 62/62 and `ui_debug_menu` is 23/23, both fully GREEN; no Developer-tool test failures remain. T-Deck ESP-IDF 6.1 firmware was rebuilt in a fresh tree (`native/targets/tdeck/build-batch45a1-green`) because this pass changes production/device-facing code, and **passed**: `openu5_tdeck.bin` is `0xc6840` (813,120) bytes, up from the prior recorded `0xc67c0` (+128 bytes), with 22% of the app partition free and zero compiler warnings (one transient GCC internal-compiler-error segfault in an unrelated ESP-IDF file, `esp_lcd_panel_rgb.c`, occurred on the first build attempt and cleared on retry — not caused by this pass's changes). Hardware flash was not performed. See §3 R-27 for the full analysis.
