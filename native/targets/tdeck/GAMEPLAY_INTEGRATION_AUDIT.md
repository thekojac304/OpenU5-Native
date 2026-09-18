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

The **core is in far better shape than the device integration**. 56 of 57 host tests pass, including deep byte-level parity suites for combat, commands, magic, items, shops, dialogue, dungeons, travel and persistence. The failures are almost entirely in the **glue layer**: `native/targets/tdeck/main/alpha_runtime.cpp` (1375 lines), `native/core/src/ui_session.cpp`, `native/core/src/presentation.cpp`, and the **asset pack**.

That glue layer has **zero automated test coverage**. Nothing in the repository instantiates `AlphaRuntime`. The on-device "smoke tests" are data-presence probes and isolated `UiSession` probes with a spy dispatcher — they never exercise the adapter that consumes the intents. Every defect below lives in exactly the blind spot that testing strategy creates.

### Largest risks, in order

1. **R-01 — Shop and Dialogue UI modes are destroyed on every keypress.** `AlphaRuntime::synchronize_after_debug()` (misnamed; it runs on *every* gameplay input, not just debug actions) unconditionally forces `base_mode` to Combat/Dungeon/Exploration. `UiMode::Shop` and `UiMode::Dialogue` are non-modal, so they are overwritten immediately after the core sets them. This single line plausibly breaks every shop, every inn, every tavern and every conversation after the first keystroke. *This is the highest-value single fix in the audit.*
2. **R-05 — The dungeon has no art.** The perspective slice atlases (`DNG1/2/3.16`) and feature art (`ITEMS.16`) that the reference compositor blits are **not packed into the native asset file at all**. `native/tools/u5pack/alpha1.ts` packs dungeon *cell maps* only. `render_dungeon_view()` is a hand-rolled wireframe of `dungeon_line`/`dungeon_rect` calls. ANCHOR 4 cannot be fixed by tuning the renderer; the asset pipeline must be extended first.
3. **R-19 — Ships cannot sail.** `(Y)ell` on the T-Deck always opens a word prompt. The reference branches to `yellSails()` when aboard a frigate *before* prompting. `CommandKind::YellSails` has no device route at all. All naval travel is blocked.
4. **R-07/R-08 — The endgame (U)se chain is unreachable,** and the one quest item that *is* listed is bound to the wrong id: the picker offers item **18 labelled "Grapple"**, but id 18 is the **Amulet of Lord British** in the authoritative ZSTATS item table. Crown, Sceptre, Moonstones, Shards, Plans, Watch and Badge are absent from the picker entirely.
5. **R-06 — `Ready` is rejected in combat and in dungeons** while the UI happily offers it: a textbook success-without-effect.
6. **R-09/R-10 — Five modal responses and both NPC-initiated events are silently discarded**, disabling Blackthorn, the Britannia guards, fountains, and every shopkeeper/guard that starts the conversation.

### Anchor verdicts

| Anchor | Verdict | Root cause status |
|---|---|---|
| **1 — stale blue/relic symbol after loot removed** | **RED** | **Root-caused.** Two independent mechanisms: (a) native promotes unopened arena chests to world objects that the reference never creates (**failing test, reproducible**); (b) the world presentation renders a chest object at raw `o.tile` = **1** = deep-water terrain (blue) instead of sprite `0x101`. |
| **2 — loose-loot icons don't match identity** | **RED** | **Root-caused.** The combat and world loot tile equations are correct (`0x100+id`, verified [EXEC]). The defect is **layer ordering**: `compose_world_presentation` paints *every* quest object last-wins, so a chest/prop/ship object overdraws a loot icon in the same cell with a raw terrain tile. |
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
| `n` | New Order | **Y** | Two-stage party selection → `NewOrder`. `SetActivePlayer` unreachable (§5). Y-20 |
| `o` | Open | **Y** | Troll chest Open preserved as working. Y-07 |
| `p` | Push | **Y** | Y-07 |
| `r` | Ready | **G**(world) / **R**(combat+dungeon) | R-06 |
| `s` | Search | **Y** | Trap reporting preserved as working. Y-07 |
| `t` | Talk | **Y** | Player-initiated works; NPC-initiated does not. R-10 |
| `u` | Use item | **R** | Picker id space wrong and incomplete. R-07, R-08 |
| `v` | View gem | **R** | ANCHOR 3. R-17 |
| `x` | X-it (Disembark) | **Y** | Y-07 |
| `y` | Yell | **R** | No frigate branch → sails unreachable. R-19 |
| `z` | Z-stats | **Y** | Preserved as working; party highlight preserved. Y-07 |
| space/Enter | Pass | **G** | Two independent routes, both echo. |
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

See the full Resource/Item matrix in §11. Summary: **gold/food/gems/keys/torches/reagents/equipment G**, **potions/scrolls Y**, **quest items and tools R**.

### F. Loose loot / chests / remains

| Stage | Status | Reason / ID |
|---|---|---|
| Chest creation in arena | **G** | `combat.cpp:467`, `:1178` — `combat_parity` green. |
| Trapped/untrapped, Search, trap resolution | **G** | Preserved as working; `chest_trap` parity green. |
| Loose-object spawn, LIFO stack, one-item-per-Get | **G** | `combat.cpp:1346` top-of-stack scan; preserved as intended. |
| Full-inventory retention (`apply_loot_grant` fails → pile kept) | **G** | Deliberate and correct: "award first, erase second". |
| Final removal → `pile_count` 0 | **G** | Confirmed. |
| Cell recomposition after removal | **R** | ANCHOR 1 (b). R-02, R-04 |
| Combat→world chest promotion | **R** | ANCHOR 1 (a) — reproducible test failure. R-03 |
| Loot icon identity | **R** | ANCHOR 2 — layer ordering. R-04 |
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
| **Ready in combat** | **R** | R-06 [EXEC] |
| **Ready in dungeon** | **R** | R-06 [EXEC] |
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
| Magic items (carpet/skull key/spyglass/sextant/watch/box) | **R** | Half absent, one mis-ided. R-07, R-08, R-13 |

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
| **Post-combat chest promotion** | **R** | R-03 — fails `gameplay_parity`. |
| Ready in combat | **R** | R-06 |
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
| `r` Ready | offered, then rejected | **R** — R-06 |
| `m` Mix | silently aliased to Cast | **Y** — Y-15 |
| Mic short = Back, Mic long = Movement Mode | unchanged in dungeon | **G** |

### M. Shops / services

| Aspect | Status | Reason / ID |
|---|---|---|
| Shop data, prices, quantities, all keeper types | **G** | `shop_parity`, `shop_adapters`, `shop_flow` green; `shop_orchestration.cpp` is 1150 lines of adjudicated logic. |
| Offer list cursor ↔ authoritative item id resolution | **G** | `shop_offering_at` bridge in `dispatch` is exactly right. |
| Hierarchical Back (deal → list → menu → exit) | **G** | `handle_shop` deal/list classification. |
| Rations quantity / rumour text sub-modals | **G** | `finish_modal` converts them to `ShopAction::Text`. |
| **Shop UI mode survival across keypresses** | **R** | R-01 — `UiMode::Shop` clobbered after every input. |
| Shop panel composition | **R** | `compose_shop_view` requires `mode()==Shop`; after R-01 it returns `nullptr`. |
| Shop exit → correct gameplay mode | **R** | `set_base_mode(pre_combat_mode_)` is a stale register. R-18 |
| Save/load after a purchase | **G** | Gold/equipment/reagents all round-trip. |

### N. NPCs / dialogue

| Aspect | Status | Reason / ID |
|---|---|---|
| Talk scripts, keywords, name/job/bye, rune output | **G** | `dialogue_parity`, `dialogue_adapter` green. |
| Player-initiated Talk + direction | **Y** | Route exists; device unproven and subject to R-01. Y-07 |
| Dialogue → quest state mutation | **G** | `dialogue_effects.cpp`; `quest_parity` green. |
| Schedules, visibility, movement, day/night | **G** | `npc_path`, `actors`, `enter_npc_map`; `turn_parity` green. |
| **NPC-initiated Talk** | **R** | R-10 |
| **NPC-initiated Shop** | **R** | R-10 |
| Guard password / tribute / arrest | **R** | R-09 |
| Blackthorn interrogation | **R** | R-09 |
| **Dialogue UI mode survival** | **R** | R-01 |
| Dialogue exit → correct mode | **R** | R-18 |
| NPC walk persistence | **G** | `capture_npc_walk`/`restore_npc_walk` with the fidelity gate. |

### O. Quests

| Aspect | Status | Reason / ID |
|---|---|---|
| Quest tables, flags, shrine bits, doom bits, shadowlords | **G** | `quest_parity` (1581-line `quest_case.inc`) green. |
| Word-of-power Yell at dungeon entrances | **Y** | Route works; ship branch missing (R-19) does not affect it. Y-24 |
| Shrines (visit/restore/donate) | **Y** | All three modals wired in `modal()`; `UiMode::ShrineSpecial` is dead in practice (§5). Y-25 |
| Search-based quest chains | **Y** | `quest_search.cpp` + `search_objects` fixtures; found objects are world objects → **lost on reload** (R-14). |
| **Shards → Flames ritual** | **R** | Shards 29–31 absent from the Use picker. R-08 |
| **Crown / Sceptre / Amulet** | **R** | R-07, R-08 |
| Blackthorn / Falsehood / Abbey | **R** | R-09 |
| Codex / endgame | **R** | Gated behind the Use chain. R-08 |
| HMS Cape plans | **R** | Item 33 absent. R-08 |
| Harpsichord melody (Cove passage) | **R** | `CommandKind::HarpsichordNote` has no route (§5). R-20 |
| Moonstones / moongates | **R** | Items 21–28 absent; `CommandKind::UseMoonstone` unreachable. R-08 |

### P. World objects / terrain / special cells

| Aspect | Status | Reason / ID |
|---|---|---|
| Doors, locked, secret, open-door timer | **G** | `CommandState::Door` + `CommandEffect::Doors`; `command_parity` green. |
| Signs | **G** | `look_sign` test + `resolve_look_sign`. |
| Terrain overrides (volatile/persistent/hourly/wipe) | **G** | `WorldTerrain` with a 4-layer `inspect()`; `capture_terrain`/`restore_terrain` round-trip. |
| Fields, traps, fireplaces, emitters, light flood | **G** | `presentation.cpp` `visibility()` + `kEmitters`; `presentation_regression` green. |
| Chests in the world | **R** | Rendered as terrain tile 1. R-02 |
| Quest objects / hidden items | **R** | Not serialized. R-14 |
| Beds / camp / auto-sleep | **Y** | `AutoSleep` is core-internal; `rest.cpp` covered by `travel_parity`. Y-26 |
| Ladders / stairs / bridges | **G** | Klimb + `resolve_world_step` speed classes. |

### Q. Transport

| Aspect | Status | Reason / ID |
|---|---|---|
| Walking + speed classes (swamp/cactus/bridge) | **G** | `movement_flow_parity`. |
| Horse, carpet, skiff Board/X-it | **Y** | `transport_flow_parity` green; device unproven. Y-07 |
| Ship Board/X-it | **Y** | Same. |
| **Ship sails (hoist/furl)** | **R** | R-19 — blocks all ocean travel. |
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
| Resurrection (In Mani Corp, healer, ankh) | **G** | `apply_target_spell` + `shop_orchestration` healer flow — but healer reachability is subject to R-01. |
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
| Renderer rebind after debug | **G** | `synchronize_after_debug` — correct *for its named purpose*; the bug is that it also runs on ordinary input (R-01). |
| Max Party / Max Resources / Equip Best / Full Test Setup | **G** | Preserved. |
| On-device diagnostics (37 scenarios) | **Y** | Data-presence probes, not integration tests. Y-06 |
| Debug masking production defects | **R** | Debug teleport calls `set_base_mode(Exploration)` on the recovery path and `synchronize_after_debug` re-derives mode every input — which **hides** R-01's shrine/dialogue symptoms from debug-driven testing. R-01 note. |

### W. Frontend / device presentation

See the renderer/mode matrix in §10.

---

## 3. KNOWN RED FAILURES

### R-01 — `synchronize_after_debug()` destroys Shop / Dialogue / ShrineSpecial modes on every input · **SEVERITY 1**

**Symptom:** after the first keystroke in a shop or a conversation, the shop panel disappears and subsequent keys are interpreted as world commands (`b` → "Board", `s` → "Search-", `r` → "Ready").

**Path:**
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
`UiSession::set_base_mode` assigns `mode_` whenever `!is_modal(mode_)` (`ui_session.cpp:166`). `Shop`, `Dialogue` and `ShrineSpecial` are **not** in `is_modal()` (`ui_session.cpp:39`), so each is overwritten immediately after the core set it.

**Why it survives review:** the function is *named* for debug actions and its comment only discusses the debug picker, but it is the runtime's only per-input rebind of `context_.dungeon` / `context_.combat`, so it must keep running. `AlphaRuntime::command()` at `alpha_runtime.cpp:416` already demonstrates the correct pattern — it only ever *forces* Combat or Dungeon, never Exploration.

**Fix shape:** split the function. Keep the context/terrain rebind unconditional; make the `set_base_mode` call conditional on `ui_->base_mode()` not already being an owned session mode (`Shop`, `Dialogue`, `ShrineSpecial`), or simply mirror `command()`'s "only force Combat/Dungeon" rule and let `DungeonExited`/`CombatEnded`/`Shop Exited` own the return to Exploration.

**Evidence:** [STATIC], unambiguous. **Requires one device capture to confirm the user-visible symptom** (`SERVICE_ACTION` followed by `UI_MODE from=shop to=explore` in the same input's log block is the signature).

---

### R-02 — World chest objects render as deep-water terrain (ANCHOR 1b) · **SEVERITY 1**

`presentation.cpp:205`:
```cpp
place(o.x, o.y, (o.shadowlord || o.loot || o.search) ? o.tile + 256 : o.tile, ...)
```
A chest object has `chest=true` but `loot=false, search=false, shadowlord=false`, and `tile=1` (`combat.cpp:1665`, `world_commands.cpp`). So it is placed as **terrain tile 1 = deep water**, a solid blue cell, instead of sprite `0x101`.

**Reference [REF]:** `game.ts::lootRenderTiles()` (line 1327) renders **only** `kind==="loot"` and `kind==="search"` objects, both at `id + 0x100`, and explicitly notes that object tiles `0x101..0x10F` are `walkable:false` and must be entities, never composed terrain. Chests in the reference are map-override tiles, not entities.

**Fix shape:** either (a) restrict the quest-object entity layer to `loot|search|shadowlord` exactly as the reference does and write chests to the terrain override layer, or (b) add `chest` to the `+256` predicate. (a) is reference-faithful.

**Evidence:** [STATIC] + [REF].

---

### R-03 — Native promotes arena chests to world objects; the reference does not (ANCHOR 1a) · **SEVERITY 1**

**Reproducible failure, in the repo today:**
```
ctest -R gameplay_parity      →  Error: Gameplay mismatch 59
```
Sequence 59 is `move, pass, pass, fight×2000, quick×100, end` with seed 3 on tile 5. After the canonical victory exit:

| | `/worldObjects` |
|---|---|
| native (actual) | `[{location:0, floor:0, x:10, y:10, tile:1, kind:"chest", contents:20, trapped:false}]` |
| TypeScript reference (expected) | `[]` |

Source: `combat.cpp:1665` — the teardown loop walks every `unopened_chest(state, cell)` and `append`s a world `QuestObject`. The reference has no such promotion; unclaimed arena treasure is simply lost, which is why the victory arena stays open for looting.

Combined with R-02 this is exactly ANCHOR 1: **after looting everything, a blue tile-1 cell remains at the encounter coordinate.**

**Note:** `expected` here is generated by running the live TypeScript core at test time (`check-gameplay.ts` spawns `gameplay_driver` and compares against the TS run), so this is not a stale fixture. `typescript_dungeon_fixture_drift` and the other drift tests all pass, confirming fixture freshness.

**Evidence:** [EXEC] + [REF].

---

### R-04 — Quest-object layer overdraws loot icons (ANCHOR 2) · **SEVERITY 2**

The loot tile equations are **correct** — verified by probe:
```
loot_render(id=2 gold)=0x102   loot_render(id=8 gem)=0x108
loot_render(chest 1)=0x101     loot_render(trapped 129)=0x101   (&0x7f folds the trap bit — matches reference)
```
The defect is ordering. `compose_world_presentation` iterates **all** quest objects last-wins and places every one of them; a chest, prop, ship or torch object sharing a cell with a loot object will overdraw the loot sprite with a raw terrain tile. The reference builds a `Map` keyed by cell containing **only** loot/search entries, so a non-loot object can never mask a loot icon.

**Fix shape:** mirror the reference's per-cell top-of-stack map, restricted to the renderable kinds.

**Evidence:** [STATIC] + [REF].

---

### R-05 — Dungeon perspective art is not in the asset pack (ANCHOR 4) · **SEVERITY 1**

`native/tools/u5pack/alpha1.ts:49` packs `dungeons.bin` — 8 records × 516 bytes = **cell maps only**. There is no DNG or ITEMS record in the pack, and `native/ASSETS.md` never mentions dungeon art.

`extractor/src/parsers/dngtiles.ts` **exists** and parses exactly what is needed (`parseDngView`, `parseItemsView`), and `game/src/skin/fiel/dungeon.ts` documents the authoritative compositor: 28 pre-drawn perspective slices per wall variant blitted at fixed X per depth with mirrored pairs, plus 20 half-feature images from ITEMS.16 for stairs/fountains/traps/chests, with floor speckle and ceiling **baked into each slice**.

What the device does instead (`native_renderer.cpp:352-411`): fills the top half `kDungeonCeiling` and the bottom half `kDungeonFloor`, scans forward for a wall, draws Bresenham lines for corridor edges, then `dungeon_rect`s a flat grey wall with mortar stripes. No doors, no variant, no features beyond a green/red 12×12 blob, no light model.

**This is why the view is "geometry partially present, textures wrong".** It cannot be fixed in the renderer alone.

**Fix shape (ordered):** 1) extend `alpha1.ts` with a `dngview.bin`/`itemsview.bin` record set using the existing `dngtiles.ts` parser; 2) add slice/feature readers to `asset_pack.cpp`; 3) replace `render_dungeon_view` with a slice blitter following `dungeon.ts`'s SIDE_X / depth-pair model; 4) wire `DungeonViewInfo.wallVariant` from the dungeon id.

**Evidence:** [STATIC] + [REF] + asset-pack manifest.

---

### R-06 — `Ready` is rejected in combat and in dungeons · **SEVERITY 2**

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

---

### R-07 — Use picker binds item id 18 to "Grapple"; id 18 is the Amulet of Lord British · **SEVERITY 2**

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

---

### R-08 — The endgame (U)se chain is unreachable from the T-Deck · **SEVERITY 1**

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

---

### R-09 — Five modal responses are silently discarded · **SEVERITY 2**

`UiSession::consume` opens these modals; `UiSession::finish_modal` does **not** convert them to commands; and `AlphaRuntime::modal()` has **no branch** for them. The player answers, and the answer evaporates.

| Event | Modal opened | Missing dispatch |
|---|---|---|
| `BlackthornPrompt` | `begin_text(Blackthorn)` | `CommandKind::BlackthornAction` (implemented, `commands.cpp:627`) |
| `GuardPasswordPrompt` | `begin_text(GuardPassword)` | guard password verification |
| `GuardTributePrompt` | `begin_yes_no(GuardTribute)` | tribute payment |
| `GuardArrestPrompt` | `begin_yes_no(GuardArrest)` | arrest submission |
| `FountainDrinkPrompt` | `begin_yes_no(FountainDrink)` | fountain drink effect |

Note the near-miss: `WellDrop` → `DropCoin`, `WellWish` → `MakeWish` and `CrystalBall` → `CrystalBall` **are** handled, and `TrollToll` is handled. `FountainDrink` sits in the same family and was simply missed.

**Evidence:** [STATIC].

---

### R-10 — NPC-initiated Talk and Shop never open · **SEVERITY 2**

`blackthorn.cpp:62/65` emits `GameEventKind::NpcInitiatesShop` / `NpcInitiatesTalk` with `e.npc` set, as a **request** for the frontend to start the session. Neither `UiSession::consume` nor `AlphaRuntime::consume_event` has a case for either. Consequence: `CommandKind::BeginConversation` (which takes `member = NPC slot`) is unreachable, and guards/merchants who hail the party do nothing.

**Evidence:** [STATIC] + unconsumed-event sweep.

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

### R-18 — `pre_combat_mode_` is one stale register serving three return paths · **SEVERITY 2**

`ui_session.cpp` writes `pre_combat_mode_` **only** on `CombatStarted`, but reads it on:
- `CombatEnded` (correct),
- `Dialogue::Ended` → `set_base_mode(pre_combat_mode_)` (`:785`),
- `Shop Exited` → `set_base_mode(pre_combat_mode_)` (`:838`).

Sequence that breaks it: fight in a dungeon (`pre_combat_mode_ = Dungeon`) → leave the dungeon → walk to a town → talk to an NPC → end the conversation → **`base_mode` becomes `Dungeon` on the surface.** Today R-01 masks this by re-deriving the mode on the next input; fixing R-01 without fixing R-18 will expose it.

**Fix shape:** give dialogue and shop their own return registers, or have both derive the return mode from the live context the way `command()` does.

---

### R-19 — `(Y)ell` has no frigate branch, so ships cannot sail · **SEVERITY 1**

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

---

### R-20 — Harpsichord melody unreachable · **SEVERITY 3**

`play_harpsichord` (`quest_world.cpp:116`) and `CommandKind::HarpsichordNote` are fully implemented, including the Cove secret-passage trigger at location 17 floor 2. No device route constructs the command — the note digits are never captured.

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
| Y-20 | `SetActivePlayer` | `N` is bound to New Order (two-stage swap); the reference also supports selecting an active member | Adjudicate the reference key binding |
| Y-21 | Rel Hur scroll | `world_magic.cpp` `case 1` needs `cmd.has_direction`; the device never supplies one for scroll use | Same class as R-11, smaller blast radius |
| Y-22 | Dungeon→combat→dungeon return | `dungeon_combat_return` handles floor delta, escape border and facing; covered by `dungeon_flow_parity` at core level only | Device round-trip test |
| Y-23 | Dungeon keyboard movement with Movement Mode off | No `w`/`d` fallback; only trackball moves | Probably acceptable, but state it as a deliberate contract |
| Y-24 | Word-of-power Yell | Works today, but will be affected by the R-19 fix | Regression-test both branches together |
| Y-25 | Shrines | All three modals dispatch correctly; `UiMode::ShrineSpecial` is set and then immediately discarded (§5) | Device pass at a shrine |
| Y-26 | Beds / auto-sleep | `CommandKind::AutoSleep` is core-internal (`commands.cpp:1163`), reached through `townAutoSleepTurn` | Confirm it is genuinely internal-only |
| Y-27 | System menu over an open modal | Menu is handled before gameplay routing and does not touch `ui_->mode()` | Device round-trip from inside a selection/target modal |

---

## 5. DEAD / UNREACHABLE PATHS

### `CommandKind` values with no player route

| Value | Verdict |
|---|---|
| `Unready` | **Dead but harmless** — `equip_item` toggles, so unequipping works |
| `CombatEscape` | **Dead** — only `CombatEscapeQuick` is bound; confirm the slow-escape variant is intentionally unused |
| `CombatAttackCancel` | **Deliberately dead** — documented at `ui_session.cpp:486`; removing it consumed a turn, which is wrong for the handheld contract. Keep. |
| `CombatYield` | **Dead** — no route anywhere |
| `ShopAction` | **Dead** — shops go through `UiIntentKind::Shop` → `execute_shop`. Candidate for deletion. |
| `EnterDungeon` | Reachable only from `commands.cpp:561` (world `Enter`) and `debug_map_picker.cpp:323`. Correct — not player-constructed. |
| `BeginConversation` | **Dead** — blocked by R-10 |
| `BlackthornAction` | **Dead** — blocked by R-09 |
| `UseMoonstone` | **Dead** — blocked by R-08 |
| `HarpsichordNote` | **Dead** — R-20 |
| `YellSails` | **Dead** — R-19 |
| `SetActivePlayer` | **Dead** — Y-20 |
| `AutoSleep` | Core-internal. Fine. |

### `UiMode` values

| Mode | Entry | Exit | Verdict |
|---|---|---|---|
| `ShrineSpecial` | 4 shrine/Blackthorn events | **none** — `handle_input`'s switch has no case, falling to `default: return false` | **Dead in practice.** The mode is set, immediately shadowed by the modal it accompanies, and then wiped by `synchronize_after_debug` on the next input. Were R-01 fixed naively, this becomes a **hard soft-lock**: no input is accepted and only `Alt+M` escapes. **Fix R-01 and ShrineSpecial together.** |
| `Dialogue` | `DialogueOutputKind::Prompt` | `Ended` → `pre_combat_mode_` | Live, but R-01 + R-18 |
| `Shop` | Shop events | `Exited`/`Closed` → `pre_combat_mode_` | Live, but R-01 + R-18 |
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

`usable_names` has 32 consecutive `nullptr` entries (indices 3–31 and 33, 35, 36) — the `UNRESOLVED_NAME` logging path in `open_selection` exists precisely to catch this and currently cannot fire because those rows are never added. See R-08.

---

## 6. DEVICE-INTEGRATION RISKS (T-Deck specific)

| Risk | Detail |
|---|---|
| **The device glue is the untested half** | 1375 lines of `alpha_runtime.cpp` + 1082 of `tdeck_board.cpp` + 569 of `tdeck_input.cpp` ≈ 3000 lines with no host test. Every SEVERITY-1 finding lives here or in the presentation/asset layer. |
| **Mode ownership is split three ways** | `UiSession::consume` sets base mode from events; `AlphaRuntime::command()` forces Combat/Dungeon; `synchronize_after_debug` forces Combat/Dungeon/Exploration. Three writers, no single owner — the direct cause of R-01 and R-18. |
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

| Test | Status |
|---|---|
| `gameplay_parity` | **FAILING** — mismatch 59, R-03. 56/57 otherwise pass. |

### False-confidence tests

| Test | Why it over-reports |
|---|---|
| `ui_session_tests` | Uses a `Spy` dispatcher that records `UiIntent`s and never executes them. Proves `UiSession` routing; proves **nothing** about `AlphaRuntime::dispatch`, `modal()`, `open_selection` or `synchronize_after_debug` — i.e. exactly where R-01, R-06, R-07, R-08, R-09, R-11 live. |
| `device_smoke_tests` (37 scenarios) | Cases 3,4,5,9,10,12,13,15,18,19,20,21,24,26,27,28,29 are **data-presence assertions** ("is the table non-empty", "is the name non-null"). Cases 2,6,7,8,11,17,22,25 are `UiSession`-with-spy probes. Case 16 composes a snapshot but asserts only a hash. **No scenario validates a rendered frame, a mode round-trip, or a full input→state→presentation chain.** |
| `presentation_regression` | Asserts snapshot composition; never checks that the chosen tile is the *right* tile for the object kind — which is why R-02 and R-04 survived. |
| `combat_loot_open_regression` | Covers Open→pile→Get authoritative mutation. Does **not** assert that the cell stops rendering loot, nor that no world object is created on exit — R-01/R-03's blind spot. |
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
| Special items (crown/sceptre/amulet/badge/box/spyglass/sextant/grapple) | ✓ | ✓ | n/a (picker missing — R-08) | **G** storage / **R** use |
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
| `Combat` | `context_.combat && combat_.initialized` | `render_snapshot(compose_combat_presentation)` | combat commands | quick escape | **G** logic / **R-04** icons |
| `Shop` | `shop_.phase != Closed` | world/combat viewport + `DeviceShopView` overlay | `execute_shop` | hierarchical | **R-01** |
| `Dialogue` | `dialogue_` session | viewport + transcript | dialogue commands | `EndConversation` | **R-01** |
| `ShrineSpecial` | shrine/Blackthorn session | viewport only | **nothing** | **nowhere** | **Dead** (§5) |
| `TextEntry` / `NumericEntry` / `YesNo` | request-specific | viewport + prompt | `finish_modal` | `return_mode_` | **G** |
| `PartySelection` / `InventorySelection` / `EquipmentSelection` / `SpellSelection` | none | viewport + `DeviceSelectionView` | `modal()` | `return_mode_` | **G** routing / **R-07/R-08** content |
| `TargetSelection` | combat for aim; world for Fire | viewport + reticle (`snapshot.target_*`) | direct command | `return_mode_` | **G** |
| `DebugMenu` | developer build | `DeviceDebugScreen` (viewport suppressed) | `UiDebugMenu` | `debug_return_mode_` | **G** |
| *gem view overlay* | `gem_view_active_` | `render_world_gem_view` / `render_dungeon_gem_view` | any key closes | prior mode + `AfterGemView` | **R-17** |

**Presentation source selection** (`alpha_runtime.cpp` `render()`) is a clean single decision — `combat ? combat : dungeon ? dungeon3d : world`, overridden by the gem view. It logs `PRESENTATION_DISPATCH` every frame. This part is well built; the problems are the *contents* of two of the branches and the *mode* that selects them.

**Impossible combinations found:** none in the renderer. The only contradictions are mode-vs-context (`UiMode::Shop` with `base_mode == Exploration` after R-01; `UiMode::ShrineSpecial` with no handler).

---

## 11. RESOURCE / ITEM MATRIX

| Resource | Get | Use | Ready | View | Save | Status |
|---|---|---|---|---|---|---|
| Gold | ✓ | n/a | n/a | Z-stats | ✓ | **G** |
| Food | ✓ | n/a (auto-consumed) | n/a | Z-stats | ✓ | **G** |
| Gems | ✓ | n/a | n/a | **`V`** | ✓ | **R-17** |
| Keys | ✓ | via Jimmy/Open | n/a | Z-stats | ✓ | **G** |
| Torches | ✓ | `I`gnite | n/a | Z-stats | ✓ | **G** |
| Reagents ×8 | shop | `M`ix | n/a | Z-stats | ✓ | **Y-19** |
| Equipment ×48 | ✓ / shop | n/a | **`R`** | Z-stats | ✓ | **G** world / **R-06** combat+dungeon |
| Armour / helmets / shields | ✓ | n/a | ✓ | ✓ | ✓ | **G** |
| Weapons / ammo | ✓ | n/a | ✓ (ammo checked) | ✓ | ✓ | **G** |
| Potions ×8 (ids 8–15) | ✓ | ✓ + party target | n/a | picker | ✓ | **G** |
| Scrolls ×8 (ids 0–7) | ✓ | ✓ | n/a | picker | ✓ | **Y-21** (Rel Hur needs a direction) |
| Spells ×48 | `M`ix | `C`ast | n/a | picker + summary | ✓ | **R-11**, **R-16** |
| Magic Carpet (16) | quest | ✓ | n/a | picker | ✓ | **Y** |
| Skull Key (17) | quest | ✓ | n/a | picker | ✓ | **Y** |
| **Amulet (18)** | quest | **mislabelled "Grapple"** | n/a | picker | ✓ | **R-07** |
| **Crown (19)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| **Sceptre (20)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| **Moonstones (21–28)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| **Shards (29–31)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| Spyglass (32) | quest | ✓ route | n/a | picker | ✓ | **R-13** (no zodiac view) |
| **Plans (33)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| Sextant (34) | quest | ✓ | n/a | picker | ✓ | **G** |
| **Watch (35)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| **Badge (36)** | quest | **absent** | n/a | **absent** | ✓ | **R-08** |
| Wooden Box (37) | quest | ✓ ("How?") | n/a | picker | ✓ | **G** |
| Grapple | quest | **should not be a Use item** | n/a | — | ✓ | **R-07** |
| Loose loot piles | ✓ LIFO | n/a | n/a | rendered `0x100+id` | **✗** | **R-14** |
| World chests | Open→piles | n/a | n/a | **tile 1 = blue** | **✗** | **R-02**, **R-14** |

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
| — | **R-06** | `Ready` offered and rejected |
| — | **R-17** | Dungeon gem view is a synthetic flood fill |

**Bottom line:** the dungeon's *logic* is in good shape and its *controls are already correct*. The unusable view is an **asset-pipeline gap**, not a control or state bug. Do not "fix" the controls.

---

## 13. QUEST / CONTENT COVERAGE REPORT

| Quest system | Core | Player route | Status |
|---|---|---|---|
| Word-of-power Yell at dungeon entrances | ✓ `yell_word_of_power` | ✓ `Y` | **Y-24** |
| Shadowlord summoning in Flame rooms | ✓ `summon_shadowlord` | ✓ `Y` | **Y** |
| **Shard ritual (Falsehood/Hatred/Cowardice)** | ✓ `cast_shard_into_flame` + `apply_shard_destruction` | **✗ no Use route** | **R-08** |
| **Crown / Sceptre / Amulet** | ✓ `use_quest_item` | **✗ / mislabelled** | **R-07, R-08** |
| Sceptre force-field dissolution (world + dungeon) | ✓ both branches | **✗** | **R-08** |
| **Blackthorn interrogation** | ✓ `blackthorn.cpp`, `BlackthornAction` | **✗ modal discarded** | **R-09** |
| **Guard password / tribute / arrest** | ✓ `talk_guard` + 3 prompts | **✗ modals discarded** | **R-09** |
| Shrines (visit / restore / donate / quest bits) | ✓ `shrine.cpp` | ✓ all 3 modals wired | **Y-25** |
| Search-revealed quest objects | ✓ `quest_search.cpp` + fixtures | ✓ `S` | **R-14** (lost on reload) |
| Lord British progression / karma | ✓ `dialogue_effects` | ✓ Talk | **Y** |
| **Harpsichord passage (Cove)** | ✓ `play_harpsichord` | **✗** | **R-20** |
| **Moongates / moonstones** | ✓ `use_moonstone`, `transitions.cpp` | **✗** | **R-08** |
| **HMS Cape** | ✓ id 33 | **✗** | **R-08** |
| Codex / endgame script | ✓ `EndgameScript`, `GameWon`/`Endgame` events appended to transcript | gated behind the above | **R-08** |
| Underworld | ✓ `exit_dungeon(true)` | ✓ | **G** logic |
| Refuge / camp scenes | ✓ `RefugeScript` | event unconsumed | **Y-04** |

`quest_parity` passes on a 1581-line adjudicated case table — the quest **logic** is well built. The problem is that roughly half the quest *verbs* have no player route, so **the game cannot currently be completed.**

---

## 14. PRIORITIZED REPAIR PLAN

Small, independently testable batches, in dependency order. Each batch ends at a physical test.

---

### Batch 1 — Mode ownership · **unblocks all of shops, dialogue, shrines** · risk: medium
**IDs:** R-01, R-18, `ShrineSpecial` (§5)
**Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (`synchronize_after_debug`, rename it), `native/core/src/ui_session.cpp` (`consume`, `handle_input`, return registers)
**Work:** split context-rebind from mode-rebind; give dialogue and shop their own return registers; give `ShrineSpecial` either a real `handle_input` case or delete the mode and stop setting it. **R-01 and ShrineSpecial must land together** — fixing R-01 alone converts a silent bug into a soft-lock.
**Physical test:** enter a blacksmith, buy an item, back out through every level, confirm the world view and Exploration verbs return. Talk to an NPC, exit, confirm the same. Visit a shrine, donate, confirm recovery.
**Model:** Opus-level. This is the subtle one; three writers must become one owner.

---

### Batch 2 — Loot & chest presentation (ANCHORS 1 + 2) · risk: low
**IDs:** R-02, R-03, R-04
**Files:** `native/core/src/presentation.cpp` (`compose_world_presentation` quest-object layer), `native/core/src/combat.cpp:1665` (promotion)
**Work:** restrict the entity layer to `loot|search|shadowlord` with a per-cell top-of-stack map mirroring `lootRenderTiles()`; route chests to the terrain-override layer; remove or gate the post-combat chest promotion so `gameplay_parity` passes.
**Verify:** `ctest -R gameplay_parity` must go green — this batch has a hard pass/fail gate already in the repo.
**Physical test:** troll chest → Open → Search → Get to zero → confirm the cell shows plain terrain, no blue square; confirm each icon matches the next `G` result.
**Model:** Sonnet/Codex is sufficient — the target behaviour is fully specified by the reference and by a failing test.

---

### Batch 3 — Reachability: Use picker, Yell sails, Ready context · risk: low
**IDs:** R-06, R-07, R-08, R-19, R-20, Y-20
**Files:** `native/core/src/display_names.cpp` (`usable_names`), `native/targets/tdeck/main/alpha_runtime.cpp` (`open_selection`), `native/core/src/ui_session.cpp` (`handle_exploration` case `'y'`), `native/core/src/commands.cpp:660-662` (routing predicates)
**Work:** name all 13 usable items; build the picker from possession flags; drop the grapple row; add the frigate branch to `(Y)ell`; add `Ready`/`Unready` to the dungeon and combat routing lists and pass `battle = c.combat`.
**Physical test:** Debug → Full Test Setup → `U` and confirm every owned tool appears with the right name; board a ship and hoist sails; `R` in combat and in a dungeon and confirm the weapon actually changes.
**Model:** Sonnet/Codex. Mechanical, but the item-id table must be transcribed from `use-tools.ts:7` exactly.

---

### Batch 4 — Dropped modal responses and NPC-initiated sessions · risk: low
**IDs:** R-09, R-10
**Files:** `native/targets/tdeck/main/alpha_runtime.cpp` (`modal`), `native/core/src/ui_session.cpp` (`consume`)
**Work:** add five `modal()` branches (Blackthorn, GuardPassword, GuardTribute, GuardArrest, FountainDrink) following the existing `WellDrop`/`WellWish` pattern; consume `NpcInitiatesTalk` → `BeginConversation` with the NPC slot, and `NpcInitiatesShop` → shop entry.
**Depends on:** Batch 1 (dialogue/shop modes must survive first).
**Physical test:** walk past a Britannia guard at night and answer the password prompt; drink from a fountain; approach a shopkeeper and let them hail you.
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

### Batch 11 — Test infrastructure · risk: low · **do this in parallel with Batch 1**
**IDs:** Y-05, Y-06, and everything in §15
**Files:** new `native/targets/tdeck/host_tests/runtime_integration_test.cpp` + a thin `AlphaRuntime` seam
**Work:** see §15. Without this, every batch above is verified only by hand.
**Model:** Opus-level for the seam design (extracting the routing/mode logic from ESP-IDF dependencies); Sonnet for the tests themselves.

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
| 9 | **A removed loose item no longer renders in that cell** | `presentation_regression` — catches ANCHOR 1 |
| 10 | **An empty loot stack renders the underlying terrain** | `presentation_regression` — catches ANCHOR 1 |
| 11 | **The visible loot tile equals `0x100 + (next Get result id)`** | `presentation_regression` — catches ANCHOR 2 |
| 12 | **No world object renders with a tile < 0x100** | `presentation_regression` — catches R-02 in one line |
| 13 | Every equipment slot holds ≤ 1 item | `item_parity` |
| 14 | Every modal `UiRequestId` opened by `consume` is handled by `finish_modal` **or** `AlphaRuntime::modal` | runtime — **catches R-09** |
| 15 | Every `GameEventKind` has a consumer or an explicit `IGNORED` entry | runtime — catches R-10, R-12, R-13 |
| 16 | Every `View` resource handler is reachable from the UI | runtime |
| 17 | Every dungeon id 33–40 loads valid map data | already smoke 18; promote to host |
| 18 | Every spell has a target label consistent with its `target_type` | `magic_parity` — **catches R-16** (12 failures today) |
| 19 | Every spell summary equals `MagicDefinitions.json::SimpleDescription` | new drift test — catches R-16 |
| 20 | Every serialized field round-trips, and **every live owner is serialized** | `persistence_driver` — **catches R-14, R-15** |
| 21 | Every `CommandKind` is either produced by a UI route or annotated core-internal | static test — catches R-19, R-20, and the §5 list |
| 22 | `Ready` succeeds in world, combat and dungeon | `command_parity` — **catches R-06** (probe already written) |

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
14. `Z`-stats: page through Provisions and Stats; confirm the party highlight.
15. **[R-01] Enter a blacksmith. Buy an item. Back out one level at a time. Confirm the world view and Exploration verbs return.** Repeat at an inn (Rest), a healer (Heal), a tavern (Rations + Rumour) and a reagent shop.

### Phase 3 — Combat and victory loot (7 min)
16. Debug → Max Party, Max Resources, Equip Best Gear.
17. Trigger an overworld encounter. Move, `A`ttack with the reticle, `F`ire, `C`ast a combat spell.
18. **[R-06] Press `R` in combat, select a weapon. Confirm the weapon actually changes** (today it will not).
19. Win. **Confirm the arena stays open.** `O`pen the chest, `S`earch it, `G` + direction repeatedly to zero.
20. **[ANCHOR 1] Confirm the cell shows plain arena floor — no blue square, no leftover symbol.**
21. **[ANCHOR 2] Before each `G`, note the visible icon; confirm the message names the same item.**
22. Mic (Back) → canonical victory exit. **[ANCHOR 1] Confirm no blue tile-1 cell at the encounter coordinate on the overworld.**
23. Save, reload. **[R-14] Confirm any loot left behind is still there.**

### Phase 4 — Items, magic, view (6 min)
24. `U`se: **[R-07/R-08]** confirm every owned tool is listed with the correct name; confirm "Grapple" is absent and the Amulet is present.
25. `U`se a potion on a party member. `U`se a scroll.
26. `C`ast Mani on a companion. `M`ix a spell.
27. **[R-11]** Cast An Sanct in the world at a locked door. Confirm it does something and does not silently eat the charge.
28. **[ANCHOR 3]** `V` with gems. **Photograph the screen.** Confirm the gem count decremented, a legible map appeared, and closing it charged exactly one turn (`VIEW_EFFECT` / `VIEW_RESULT` in the log).

### Phase 5 — Transport (4 min)
29. Debug → Transport → Ship. `B`oard. **[R-19] `Y`ell → confirm HOIST/FURL, not a word prompt.** Sail with the wind.
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
39. `C`ast Uus Por / Des Por. **[R-06] `R`eady in the dungeon.**
40. Trigger a dungeon encounter. Win. **Confirm the return is to the same cell and facing.**
41. `Alt+M` System Menu → close. `Alt+D` Developer → Back. **Confirm the dungeon view returns both times.**
42. **[R-15]** Save inside the dungeon, reload, and record exactly what happens.
43. Descend past floor 7 → Underworld. Confirm the transition.
44. Walk out at the level-1 entrance → surface. Confirm the world view and Exploration verbs.

### Phase 7 — Quest-critical interaction (3 min)
45. Debug → Quest → grant a shard. **[R-08] `U`se it in a Flame room.** (Today it will not be listed.)
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

**Test run:** `ctest` in `native/core/build-alpha20-host`, 57 tests, **56 passed / 1 failed** (`gameplay_parity`, 61.9 s). Mismatch artifact retained at `native/core/build-gameplay/mismatch.json`.

**Probe:** a throwaway program linked against `libopenu5_core.a` verified R-06 and R-16 directly. It lives in the session scratchpad, **not** in the repo — it is an audit instrument, not a test. Its assertions are folded into the proposed invariants 18 and 22 in §15, which is where they belong.

**Working tree:** unchanged apart from this document. No production code was modified during the audit.
