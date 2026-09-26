# Alpha 2 — Preservation vs. Modernization Ledger

**Created:** Batch 18 (2026-09-21) · **Scope:** the native T-Deck port, `native/`
**Companion documents:** [`GAMEPLAY_INTEGRATION_AUDIT.md`](GAMEPLAY_INTEGRATION_AUDIT.md) (the finding matrix),
[`../../../docs/FIDELITY-CONTRACT.md`](../../../docs/FIDELITY-CONTRACT.md) (how "faithful" is decided),
[`../../../docs/bugs-del-original.md`](../../../docs/bugs-del-original.md) (the register of the *original's* defects).

## Why this file exists

The fidelity contract and the bug register both answer the question *"what did the
1988 binary do, and do we clone it?"*. Neither answers the reciprocal question:
**where does this port knowingly differ from the reference, and why?**

Those differences accumulated across Batches 1–18 for three unrelated reasons, and
until now they were scattered across per-batch write-ups where nobody could tell a
deliberate adaptation from unfinished work. This ledger separates them so that a
future preservation-profile effort can restore original behaviour cleanly, and so
that a tester never files an intentional difference as a defect.

**Batch 18 changed none of these.** Making them explicit is the whole deliverable.

**Batch 21B added D-19 through D-22** from the original-behaviour gap sweep. All four are unresolved divergences in the §4 sense — gaps, not choices — and none was fixed in that batch; only H-148 was.

**Batch 26 changed no row.** H-115 was a defect, not a divergence: 1988 saves underground and resumes in place (`CAST2.OVL:0x10FE`, no location gate), and the port now does too. The session rides the A-14 sidecar as `gameState.dungeon`, native-only, because the TypeScript reference does not save it. The Rel Tym toggle resets on load, as `DUNGEON 0x0E40` does. D-23 (H-164) is now reproduced on host for the dungeon session and stays open.

**Batch 27 resolved D-23** (H-164): `Alt+L` is the same 1988 load as Continue Latest and now ends in the same `synchronize_loaded_world()`. It was a native routing defect, not a divergence, so no §2/§3 row changed.

**Batch 28 changed no row** (H-166). 1988 writes and reads the save window as one piece (`CAST2.OVL:0x10FE`, `INTRO.OVL:0x0EB4`), dungeon map and object register included. A-14's two-slot generation gate now matches that: a generation whose `"dungeon"` or `"worldObjects"` sidecar the runtime would refuse is refused whole and the older generation loads, instead of loading with that part silently dropped. A native defect in the A-14 mechanism, not a divergence. Legal-but-unusual payloads are still accepted verbatim. The one new domain rule (a dungeon session's id must be a dungeon, 33–40) is recorded with its evidence in the audit.

**Batch 29 resolved two thirds of D-19 and added D-24 and D-25.** The bed snap (H-154) and the "Thrown out of bed!" probe (H-155) were native wiring defects, now bound to the Batch 24 reposition primitive and to one NPC-plus-object occupancy helper. The snap follows the binary (`TOWN.OVL:0x1694`: reposition from the live schedule, stuck counter kept), not the TypeScript reference, which *rebuilds* the list from the `.NPC` file. That is a knowing native-vs-reference difference in the binary's favour; no corpus tracks it, and it is recorded in the audit as a REFERENCE-PORT DIFFERENCE. D-19's third clause (the watchman walking through the fire) was wrong: the device never posts a watch, so no guard walks. That gap is now D-24 (H-167). D-25 (H-168) is a gap both ports share. H-169 (the pre-sleep NPC passes) is observed but not adjudicated, so it has no row yet.
**Batch 32 resolved D-24 on host and added D-27.** H-167 now offers the original optional watch when at least two members are `G`/`P`, validates a chosen guard as `G`, and feeds the guard's CampFire south start and identity into the walk collision test. Six mutations are killed. Device presentation awaits Phase 6Y. The separate CMDS camp loop also calls world and housekeeping kernels that native camping omits; that pre-existing gap is D-27/H-171, queued without a production change.
The "toggle candidate" column records how isolated each difference already is —
**Batch 33 resolved D-25/H-168 on host.** Cannon hits now run the original immediate NPC clear-slot lifecycle in both ports, using the original type gate for the persistent dead bit. The old cannon archaeology note was corrected: kernel `0x7AF4` clears the object record; it is not an HP/damage roll. The five mutation cases are killed. No device-only observation remains for H-168; Phases 6T–6Y retain their earlier assignments.
it is an assessment, not a commitment, and **no toggle is implemented**.

**Batch 51 re-adjudicated D-10 and added D-39 through D-43** (scripted-scene pacing). The original paces every staged scene with `delay` 0x20fa and `run_n_frames` 0x3ae6 (real BIOS ticks, exact), `tone_sweep` 0x2192 (blocks for a2 samples even with sound off; host-dependent) and `fx_tile_fizzle_in` 0x1068 (no timer). The device honoured only tick waits and only inside a pacer, so the Camp apparition (D-39) and Blackthorn's sweep/circle/siren beats (D-10) had no dwell; both are host fixed. The sweep hold uses the project's established calibration (25,806 samples/s, `speaker.ts`), the fast-host floor below both 1988 witnesses. Four scene-local gaps are only classified: D-40 shrine/Codex key waits, D-41 ritual inversion, D-42 Refuge cadence and getkey, D-43 the Blackthorn sacrifice burst.

**Batch 52 (Alpha 2 readiness audit) reconciled every row and added D-44 through D-54.** Phases 6T–7D passed on hardware: D-19, D-20, D-23, D-24, D-29, D-31–D-37 and D-39 are hardware-confirmed. D-38 is confirmed for the avatar; the scheduled-NPC 1988 pose is still unresolved. The new rows are native **device-wiring defects**, not choices. They were found by checking every core service hook against what `AlphaRuntime` binds. D-44 (Words of Power), D-45 (moonstones; its D-48 gate visibility and D-53 prompt ride with it), D-46 (ENDMSG endgame) and D-49 (shop ship/horse, the Batch 20 H-146 FAIL) are **Alpha 2 release blockers**. D-47 (karma text), D-50–D-52 and D-54 are non-blocking. The per-row status table is §6. No production code changed.

**Batch 53 fixed the four release blockers in software; the hardware retest is Phase 7E.** D-44 (Words of Power), D-45 (the runtime moonstone owner, hydrated from and captured into GAM `0x28a–0x2a9` with no new save field), D-46 (ENDMSG.DAT and `end_record`) and D-49 (shop `ship` / `horse` / `reserve` on core's own transport owner) are bound by shared production binders that `initialize()` and the host fixture both call; D-48's gate visibility (static tile `0xDC`) and D-53's `To phase:` line ride with D-45; D-47 now shows the six KARMA.DAT records. Three SD-pack sections were added (ENDMSG, KARMA, Words of Power). Each row reads **SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E)**; none is struck before the device run. Still open and unchanged: D-48's transit animation and the `0xDC` rise, D-54 (endgame cinematic), D-50 – D-52, D-40 – D-43.

**Batch 53A (2026-09-26) — the state after the ending.** Phase 7E-A run 1 on the Batch 53 image passed the absorption, `VICTORY!`, the ENDMSG text and input responsiveness, then left the party in Doom's enclosed final cell accepting movement. The shipped ENDGAME.OVL never returns to the dungeon (both endings finish in a loop inside the overlay), so this was a native defect, **D-55** (Outcome B), not an Alpha 3 presentation gap: the device now enters a terminal `UiMode::Ending` on game-won. **A-15** records the device's terminal-state adaptations. Two findings from the same trace are recorded, not fixed: **D-56** (the Y/N box questions are the player's in the original) and **D-57** (two `victory` token lines in the transcript). D-46 stays SOFTWARE FIXED — HARDWARE RETEST PENDING until 7E-A′ passes.

---

## 1. Reference-faithful behaviour (the default)

Everything not listed in sections 2–4 is intended to reproduce Ultima V exactly,
including its bugs where `FIDELITY-CONTRACT.md` REGLA 1 applies.

This is not a list because it is the overwhelming majority of the port, and it is
not asserted by prose: it is asserted by the parity corpora, which compare native
against the TypeScript reference case by case on every host run.

| Corpus | Size | What it pins |
|---|---|---|
| `gameplay_parity` | 5,058 sequences | whole-command outcomes through `dispatch_world_command` |
| `quest_parity` | 5,377 cases | quest/scripted state machines (incl. 2 reference non-terminating observations) |
| `magic_parity` | 25,088 cases | `cast_spell()` tracked fields |
| `combat_parity`, `advanced_combat_parity`, `command_parity`, `item_parity`, `turn_parity`, `travel_parity`, `shop_parity`, `dialogue_parity`, `dungeon_parity`, `dungeon_flow_parity`, `movement_flow_parity`, `transport_flow_parity`, `world_flow_parity`, `persistence_parity` | — | their named domains |
| `typescript_*_drift` (14 checks) | — | that every generated table still matches its generator |

A difference that any of these would catch is a **defect**, not a ledger entry.
Nothing in sections 2–4 moves a field those corpora track.

---

## 2. Intentional native / T-Deck adaptations

Forced by the hardware: a 320×240 ST7789, a 5×7 ASCII font, a physical keyboard
with no ESC and no numeric row, a trackball, and no DOS text console.

| # | Adaptation | Reference behaviour | Cause | Evidence | Toggle candidate |
|---|---|---|---|---|---|
| A-1 | Mic key short press = Cancel/ESC; hold ≈1.1 s = Movement Mode toggle | DOS ESC key | The T-Deck matrix has no ESC | `input_regression`, `ui_input_adapter.cpp` | No — it *is* the ESC affordance |
| A-2 | `Symbol`+Mic = literal `'0'` | plain `0` key | `kSymbol[0][6]` is the only matrix cell producing `'0'`, and A-1 owns that key | Y-29, Batch 16; `input_test.cpp` | No |
| A-3 | Movement Mode: WASD drives movement/direction contexts, literal in text entry | arrow keys / numpad | No arrow cluster on the T-Deck | `is_movement_context()`, smoke 7 | No |
| A-4 | Trackball is the 4-way direction and cursor device, with a settings-driven debounce (25–300 %) | arrow keys | hardware | `InputController::normalize` | No |
| A-5 | `Alt+M` System Menu, `Alt+D`/`S`/`L` developer/save/load | DOS-era menus and hotkeys | device shell affordances | `DeviceShortcut` | No |
| A-6 | Dungeon HUD uses the 9 px top/bottom strips for `L<n>` and `Dir:` | the DOS status panel | the strips exist because the viewport blit reserves them; dropping them would waste 18 of 176 rows | R-05, Batch 9 | Possible, low value |
| A-7 | Dungeon `(M)` is an alias of Cast; `(R)eady`, `(U)se` and `(Z)`-stats are shared into the dungeon context | unconfirmed for Mix; the others are reference dungeon commands | deliberate shared UI affordance, recorded at `ui_session.cpp` | Y-15 — see §4, the *Mix underground* question is unresolved | Yes, isolated to one `switch` arm |
| A-8 | `.` and Confirm both mean Turn Around in the dungeon | the DOS turn-around key | key availability | `ui_session.cpp:866`, `:911` | Yes |
| A-9 | No IBM.CH box frame around a Z-stats list | `draw_list_frame` 0x045e paints a 15×10 frame from glyphs 0x10/0x11/0x13–0x17 | the 5×7 face is ASCII-only, and the panel already carries its own rules in that place | Batch 14 residual 1 | Yes — one draw call |
| A-10 | Sex renders `M`/`F`, not `♂`/`♀` | CP437 0x0b/0x0c | `tdeck_board.cpp`'s `glyph()` is an ASCII switch. `GameState::gender` still stores the binary's own 0x0b/0x0c | Batch 14 residual 2 | Yes — the data is intact, only the glyph table is missing |
| A-11 | Item names are unabbreviated (`Spider Silk`, not `Sp. Silk`) | 14-cell abbreviated name tables | the T-Deck row is wider; the item SET, ORDER and counts are reproduced exactly | Batch 14 residual 3 | Yes — a second name table |
| A-12 | Z-stats Items page prints canonical names, not RUNES.CH sigils | `print_list_row`'s `*`/`!`/`(` branches draw a pictogram | ASCII face | Batch 14 residual 4 | Yes |
| A-13 | Asset pack + CRC/SHA identity gate blocks boot on a mismatched SD card | no such concept | the card and firmware can drift independently | `packs_match`, Batch 9C | No — a safety gate |
| A-14 | Persistence is a JSON sidecar over a two-slot generation commit | a single `.GAM`/`SAVED.GAM` pair | flash wear, partial-write recovery, DMA headroom | §8, `persistence_parity` | No |
| A-15 | After game-won the device is in the terminal `UiMode::Ending`: game keys do nothing, Shift+Up/Down page the ending, the System Menu stays (Load, Settings, Developer, **Return to Title**), Save is refused, and one System line says `The quest is complete. Alt+M: System Menu` | ENDGAME.OVL's terminal loop (`0x04f9` / `0x0ac9`): in the GOG build any key restores the video mode and quits to DOS (`0x0b0b` → `exit()`); the 1988 build (`ff 46 fe eb fb`) reads no key | The device shows the whole ending as one transcript, so a stray key must not throw it away; Return to Title is the device's program exit; a save would load back into the sealed cell | Batch 53A; `batch53a_ending_terminal` | Possible (bind "any key → title" once the Alpha 3 presenter paces the pages) |

---

## 3. Intentional enhancements

Knowingly preferred over the original. All are additive: none removes or alters a
reference behaviour.

| # | Enhancement | Original | Rationale | Evidence | Toggle candidate |
|---|---|---|---|---|---|
| E-1 | Transcript scrollback: `Shift+Up`/`Shift+Down` review with auto-follow that does **not** yank the view back on new text | no scrollback | a 320×240 transcript is short; losing a message to an auto-scroll is unrecoverable | R-31, Batch 4.5C | Yes — `preserve_scroll_on_growth()` is one function |
| E-2 | Developer tools: map/teleport picker, labelled setters, presets with a disclosed effect list and a confirm gate, deterministic Certification workflow | none | the port cannot be validated on a device without them | R-27–R-30, Batch 4.5A; gated behind `OPENU5_ENABLE_DEVELOPER_TOOLS` | Already a compile-time toggle |
| E-3 | Dungeon `(D)rink` away from a fountain answers `"No fountain here."` instead of silence | silence | inherited from the TypeScript port's declared QoL shortcut | `ui_session.cpp` `case 'd'` | Yes — one branch |
| E-4 | Settings surface (brightness, trackball debounce, UI size, movement mode) persisted separately from the save | none | device ergonomics | `AlphaSettingsService` | No |

**Batch 31 / H-165 clarification to E-2.** The 1988 `TOWN.OVL:0x0798–0x07b4` exit question blocks its command loop until Y/N/Esc, so ordinary `MAINOUT.OVL:0x0790` dungeon entry cannot run under that question. Developer teleport is an E-2 native enhancement with no original equivalent. It now refuses a dungeon destination while `awaiting_exit` is pending, leaving the question answerable; after the answer it uses the normal dungeon loader. This is a native-only preservation policy, proven with the shipped dungeon pack and raw-menu runtime test `batch31_h165_dungeon_entry` (11/11). Other Developer destinations keep their established Batch 25 behavior. H-115 save/resume remains distinct and unchanged.

**Not enhancements, despite looking like them:** the Blackthorn capture scene
presentation (R-32) and the dungeon authored art (Batch 9C) are *fidelity* work —
they move the port toward the original, not away from it. Their open questions are
in §4 (D-10) and in the hardware checklist, not here.

---

## 4. Unresolved divergences

Differences whose status is **not yet decided**. Each needs either a reference
answer or a product decision; none is scheduled in Alpha 2 unless marked.

| # | Divergence | Kind | Why unresolved | Audit ID |
|---|---|---|---|---|
| D-1 | WASD does not navigate the frontend menus (trackball and hotkeys only) | product decision | nobody has decided whether Movement Mode should reach menus | Y-01 |
| D-2 | Trackball centre click (GPIO 0) is unbound | product decision | should it be Confirm? | Y-02 |
| D-3 | Audio: `GameEventKind::Sfx` unconsumed; `sound_volume`/`music_volume` settings are reserved and inert | out of Alpha 2 scope | declared out of scope; the dangling settings are the only loose end | Y-03 |
| D-4 | Mix is unreachable underground | reference question | is Mix legal in a dungeon in the original? | Y-15, A-7 |
| D-5 | Acknowledgements is one hard-coded line, not the reference credit scroll | incomplete | low priority | Y-17 |
| D-6 | Mix quantity is hard-coded to 1 (`c.hours = 1`) | incomplete | the reference lets the player choose a batch size; needs a numeric modal | Y-19 |
| D-7 | Dungeon keyboard movement requires Movement Mode ON (no `w`/`d` fallback) | product decision | probably correct, but it has never been stated as a contract | Y-23 |
| D-8 | Combat Ready picker: empty-handed opens a disabled `"(None available)"` picker instead of charging immediately; `ItemResult::vanished` does not close the picker early | presentation | does not affect action cost; never adjudicated | Y-28 |
| D-9 | World cast raises the getdir **before** `world_magic()`, the reference consumes first and prompts after | ordering | the fused `world_magic()` call makes the reference order awkward; deliberately not fixed | Y-30 |
| D-10 | Blackthorn capture scene pacing feels collapsed against original footage. **RE-ADJUDICATED — Batch 51:** the tick pacing is byte-exact — `delay` 0x20fa and `run_n_frames` 0x3ae6 count real PIT ticks, so 55 ms is the original tick, not a TrollSneak loan. What collapsed were the non-tick waits: the materialization sweep ran with Blackthorn's cell already filled and no hold, the holy circle and Blackthorn were applied in one pump (the "missing teleport-in", H-122), and the sacrifice siren had no hold. All three **host fixed** (503 ms sweep over an empty cell, one-tick circle frame, 7,130 ms siren). | calibration — **residual only** | what remains open is the **true host duration** of the sweeps (B: the device holds the calibrated fast-host floor) and the fizzle texture (C: render-bound, no timer). *Original cell:* **no footage-derived timing evidence for this scene exists anywhere in the repository** (`re/notes/blackthorn-escena-324.md` §4 disclaims a witness; the TypeScript `PAUSE_UNIT_MS` is an admitted reuse of TrollSneak's calibration). Inventing a number is out of bounds | Y-32, H-122 |
| ~~D-11~~ | ~~Crystal ball opens a fabricated `"Peer into it?"` yes/no; the reference opens the kernel 0x4988 character picker, and the native command is consequently dead~~ **RESOLVED — Batch 19.** The fabricated prompt is deleted and `openu5::resolve_command_char()` (`native/core/include/openu5/command_char.h`) is the shared seam, used by the crystal ball, `(S)earch` and `(C)ast` alike. Row kept struck through rather than removed so the backlog's history stays readable | ~~production defect~~ **fixed** | — | R-25, §14 Batch 19 |
| D-12 | The zodiac view is still clipped by the 9 px sky/wind strips | presentation | the gem view got `full_square_viewport` in Batch 10; zodiac was out of that scope | §6, R-13 |
| D-13 | No `"Player: <name>"` / `"Status: "` console echo during Z-stats member selection | presentation | filed as a minor follow-up in Batch 14, never scheduled | Batch 14 residual 6 |
| D-14 | Digit shortcuts work inside the Z-stats page loop but not inside the member picker | scope | the picker's digit selection is a kernel behaviour outside the overlay; wiring it changes digit handling for **every** party picker | Batch 14 residual 7 |
| D-15 | Pocket Watch (extended id 0x23) has no row in the `(U)se` picker or the Z-stats Items page | missing state | no `GameState` field backs possession, so no row can be gated | R-08, Batch 14 residual 5 |
| D-16 | "Transfer from Ultima IV" is a notice, not a feature | explicit deferral | `frontend.cpp:88` | §2A |
| D-17 | `render_active_view()` (~45 lines) has no caller but is still linked into the firmware | dead code | superseded by `render_snapshot()`; confirmed still present in `openu5_tdeck.map` at Batch 18 | §5 |
| D-18 | `CommandKind::ShopAction`, `CommandKind::CombatEscape` and `CommandKind::Unready` are handled but have no producer | dead code | each is deliberate; re-verified at Batch 18 | §5 |
| ~~D-19~~ | ~~Bed hole-up on the device snaps no NPCs, can never print `"Thrown out of bed!"`, and lets the watchman walk through the campfire — `AlphaRuntime` wires `RestServices::snap_npcs`, `occupied` and `cell_free` to an empty lambda / constant `false` / constant `true`~~ **RESOLVED — Batch 29 (host; device retest Phase 6W).** `bind_rest_services()` binds the snap (`snap_npcs_to_schedule`, 1988's reposition) and the probe (`object_or_npc_at`, NPCs + objects). The watchman clause was wrong: no watch is ever posted on the device, so `cell_free` has no caller (see D-24). Row kept struck through, as D-11 | ~~incomplete~~ **fixed** (snap, probe); **reclassified** (`cell_free`) | host wiring never finished; the core side was complete | H-154, H-155, H-160 |
| ~~D-20~~ | ~~Bed hole-up runs no per-tick turn housekeeping and no day/night tile refresh~~ **RESOLVED ON HOST — Batch 30; device retest Phase 6X.** The native bed loop now calls shared `turn_housekeeping` once per tick and refreshes the existing terrain owner at 05:00/20:00, before NPC snap and ejection. TypeScript still omits both. | ~~incomplete~~ **host fixed** | 1988 calls kernel `0x2AE8` at `CMDS.OVL:0x0671` and `TOWN.OVL:0x0170` at `0x0664`, both inside the sleep loop; 24/24 shipped-pack runtime checks, 6/6 mutations, full suite 100/100 | H-156, H-157 |
| D-21 | Changing floors inside a small map does not reload the map record, so volatile terrain (an unmagicked skull-key door) survives a floor change | incomplete | 1988's `town_use_ladder` re-reads the record via `town_load_town_map(fresh=1)`; fixing it touches R-14 terrain persistence | H-158 |
| D-22 | Interior chests are seeded with contents byte `8` where the binary seeds `0x1E` | incomplete | Class-C guess for oracle hole O5, **closed from the binary in Batch 21B** (`TOWN.OVL:0x1795`); correcting it moves `gameplay_parity`/`quest_parity` and must land together with the TypeScript generator | H-159 |
| ~~D-23~~ | ~~`Alt+L` quick load does not go through `synchronize_loaded_world()`: the world-object pool is neither cleared nor restored from the save, the dungeon session is not restored, live scenes/fx are not cancelled~~ **RESOLVED — Batch 27.** The `Alt+L` arm now calls `synchronize_loaded_world()`, the one post-load finalization every other load route uses; `batch27_alt_load` compares it field by field with Continue Latest. Row kept struck through, as D-11 | ~~incomplete~~ **fixed** | found by code reading in Batch 24; reproduced on host in Batch 26 | H-164 |
| ~~D-24~~ | ~~Outdoor (H)ole up on the device never asks "Wilt thou set a watch?" / "Who will stand guard?": every camp is unwatched~~ **HOST FIXED — Batch 32; device Phase 6Y pending.** The optional prompt, one `G` guard choice, "None posted!" on invalid/cancel, CampFire south start, sleeper/fire collision and watched RNG path are live. | ~~incomplete~~ **host fixed** | `camp_watch_offer`/`camp_guard_choice`, existing UiSession modes and `RestServices::cell_free(ctx, guard, col, row)`; 35/35 shipped-pack/raw-key checks and six killed mutations | H-167 (supersedes H-160) |
| ~~D-25~~ | ~~A cannon-killed NPC kept walking until map re-entry~~ **HOST FIXED — Batch 33.** The hit immediately clears the live NPC slot in both ports; the original type-gated dead bit persists a person but allows a guard to return on re-entry. | ~~incomplete~~ **host fixed** | CMDS `0x0d47-0x0d82` → `0x3A74`, TOWN `0x011e`/`0x0052`/`0x00b0`; shipped Ararat/castle host 19/19, five killed mutations, gameplay parity | H-168 |
| ~~D-26~~ | ~~With Q active or expiring during bed sleep, the port uses `hours * 6` ticks and can stop before the original target hour~~ **HOST FIXED — Batch 35.** Bed sleep now uses the original wrapped target hour, including the 23 subtraction at midnight. Q remains a separate status byte/turn counter and may expire mid-sleep. | ~~incomplete~~ **host fixed** | CMDS `0x059e-0x05b0` target local, `0x063b` equality before tick; shipped raw-key RED 15 failures, GREEN 42/42, six killed mutations, 17/17 subset, 105/105 full host. No new physical phase | H-170 |
| ~~D-27~~ | ~~Camp omitted original five-minute world and survival housekeeping~~ **HOST FIXED / RECLASSIFIED — Batch 36.** The queued survival claim was false: CMDS `0x020a` is status draw `0x2900`, `0x031b` is delay `0x20fa(1)`, and Camp never calls `0x2ae8`. The real gaps were `0x5910` redraw wind/RNG before ring/clock, accepted-entry Q/T clear (`0x001c/0x001f`), and the hourly encounter roll after the next redraw/ring. Poison, meals, starvation and turn counter correctly do not advance in Camp. | ~~incomplete~~ **host fixed** | original CMDS/EXE bytes plus 24/24 shipped-pack raw-key checks; mutation and full-suite evidence in audit | H-171 |
| ~~D-28~~ | ~~Bed sleep skips the original 16 NPC passes and hostile pre-sleep abort~~ **HOST FIXED — Batch 34.** Full local NPC pass/redraw at the current hour, up to 16; AI 6/7 adjacency cancels before sleep. | ~~incomplete~~ **host fixed** | CMDS `0x05b4-0x05d4`; shipped Iolo's Hut/castle raw-key 21/21, eight killed mutations, full 104/104 | H-169 |
| ~~D-29~~ | ~~Original bed entry clears viewport rectangle after `Zzz` and before the first ten-minute tick; native lacked a matching fill~~ **HOST FIXED — Batch 37; Phase 6Z pending.** The renderer-only black map-window fill is synchronous, persists through bed ticks, and is restored by the next ordinary redraw. | ~~queued~~ **host fixed** | CMDS `0x0614-0x0624`; raw-key framebuffer RED 9/12, GREEN 12/12, eight mutations killed, 107/107 host | H-172 |
| ~~D-30~~ | ~~Camp from a nonzero minute ran twelve five-minute steps per requested hour instead of stopping at the original target hour~~ **HOST FIXED — Batch 38.** Camp now compares the live hour to the wrapped target before each step; 05:50 + one hour ends at 06:00 after two steps. | ~~incomplete, queued~~ **host fixed** | CMDS 0x0066-0x0079 and 0x01ee-0x01f8; raw-key RED 12/31, final 43/43, six killed mutations, 108/108 host; no new physical phase | H-173 |
| ~~D-31~~ | ~~Native omits intermediate bed status frames~~ **HOST FIXED - Batch 39; Phase 7A pending.** The panel shows S before Zzz, then post-housekeeping HP/clock each tick while the map stays black. The original row draw also clears an asleep active selection. | ~~queued~~ **host fixed** | CMDS `0x060a`/`0x0674` -> EXE `0x2900`/`0x2726`; framebuffer RED 9/17, GREEN 17/17, nine mutations killed, 109/109 host | H-174 |
| ~~D-32~~ | ~~Camp apparition member staging~~ **HOST FIXED — Batch 41.** Real Rest now mutates one eligible slot, waits for a key, then recalculates MP and redraws status before touching the next slot. RNG and final mechanics match Batch 40. | **host fixed; Phase 7B pending** | OUTSUBS 0x070b-0x090e; RED 0/1, focused 18/18, raw-key 9/9, nine killed mutations, affected 16/16, fresh 112/112 host. Separate visual gap H-176/D-33. | H-175 |
| ~~D-33~~ | ~~Native omits the standing actor and viewport XOR at each live apparition slot~~ **HOST FIXED — Batch 42.** Authored CampFire south cell, class standing tile and one 176x176 EGA XOR/repaint precede level/Hail; the panel waits for acknowledgement. | **host fixed; Phase 7B pending** | OUTSUBS/DATA raw log; framebuffer RED 5/7, GREEN 23/23, eleven killed mutations, affected 19/19, full 113/113 host | H-176 |
| ~~D-34~~ | ~~Native omits the CampFire party sleep scene before the gate~~ **HOST FIXED — Batch 43.** Scene, S/G/P panel and guard movement appear during accepted Camp on both gate outcomes; apparition remains separate. | **host fixed; Phase 7B pending** | CMDS 0x005e/0x010d, kernel 0x6880/0x6936; RED 6/11, GREEN 20/20, affected 19/19; original-byte and mutation logs under native/core | H-177 |
| ~~D-35~~ | ~~Camp scene entry omitted equipped-ring RNG~~ **HOST FIXED — Batch 44.** Each living 0x2a/0x2c wearer rolls expiry rand(0,15); a surviving non-S 0x2c actor then triggers a party-wide 0x400c regeneration pass. These calls interleave in roster order before sleep scene mounting. | **host fixed; Phase 7B pending** | ULTIMA.EXE 0x69e1-0x6b6e/0x6794/0x400c; RED 4/20, GREEN 20/20, 15 killed mutations, affected 21/21 | H-178 |
| ~~D-36~~ | ~~Camp ring-42 standing actors rendered as ordinary class tiles~~ **HOST FIXED — Batch 46.** A surviving Ring of Invisibility gives each non-S Camp actor visible outline tile 0x11d; the sleep routine overwrites eligible sleepers with 0x11e. Guard and P retain 0x11d until each live apparition wake writes a class standing tile; D leaves background. Ring expiry precedes mounting. | **host fixed; Phase 7B pending** | ULTIMA.EXE 0x6794/0x68ae, OUTSUBS 0x086d; RED 13/16, GREEN 16/16, nine killed mutations, unchanged RNG | H-179 |
| ~~D-37~~ | ~~Dialog-zero scheduled guard can occupy a bed and answer Look while native leaves the bed visually empty~~ **HOST FIXED — Batch 45; Phase 7C pending.** Guard visibility, identity, Look and occupancy are fixed. The old claim that upright bed pose is original is **unproven** (Batch 49). | **host fixed for visibility; original pose unresolved** | TOWN 0x1726, EXE 0x5394, LOOKOBJ 0x09c4; RED 32/33, focused 14/14; Batch 49 byte trace | H-180 |
| D-38 | Native world avatar keeps its fixed on-foot tile when the eligible original actor stands on bed head 0xab; original selector renders 0x11a via actor byte 0x1a + bank 0x100. Native scheduled NPCs still retain ordinary tiles; their **original** bed pose remains unresolved. | **avatar host fixed Batch 48; NPC adjudication open; device Phase 7C pending** | ULTIMA.EXE 0x51b8-0x5391 / 0x55fe; FONT.OVL 0x02e3; Batch 49 shipped-byte trace and castle census | H-181 |
| ~~D-39~~ | ~~The Camp apparition ran with every original wait at zero: each Camp event was rendered synchronously inside the command, so the chord that freezes the XOR frame, the chime, the arpeggio, the restore frames and the figure's appearance each lived one SPI transfer ("way too fast", Phase 7B run 2)~~ **HOST FIXED — Batch 51; Phase 7D-A pending.** `NarrativeScenePacer` paces the apparition from its first waiting event with the waits OUTSUBS spends: sweeps at the calibrated floor (materialize 387, arpeggio 1,162, chime 193, **chord 2,325 ms**), `run_n_frames` 55/165 ms, fizzle floor 55 ms; the getkeys stay the session's own. Input during a wait is swallowed, before the Batch 41 scene-key rule. | **host fixed; Phase 7D-A pending** | OUTSUBS `0x067b`–`0x08d9`; core RED 14/27 → 28/28, runtime RED 5/16 → 21/21, 12/12 mutations, full 120/120 | H-182 |
| D-40 | Shrine "Quest is ordained" and the Codex reading: the device drops every `ShrineKeyWait` outside the Blackthorn scene, so all the text after each altar/Codex getkey arrives at once | missing acknowledgement | the reference presents them (`ui/shrine-key-pacer.ts`); the device never ported it. Probe `native/core/batch51-shrine-keywait-probe.log`. Not fixed in Batch 51 | H-183 |
| D-41 | Donation "ALAKAZAM!", "WELL DONE!" and the Codex ceremony: `RitualInvert` has no device consumer (no viewport inversion), the 184,000 / 138,000-sample sweep holds are zero, and the Codex's three XOR pulses are absent (only its quakes play) | missing presentation beat + missing dwell | CAST2 `0x0bcd`/`0x0c41` XOR held through 920 sweeps then `run_n_frames(10)` `0x0d1a`; Codex XOR+shake ×3 `0x0dbd`–`0x0dee`; reference has `ui/ritual-invert.ts`. Not fixed in Batch 51 | H-184 |
| D-42 | Refuge cadence is Class C — 70 ms per `delay` unit plus 900/260 ms reading floors — where the original's `delay(n)` is exact 54.93 ms ticks; the karma speech holds on a timer where the original waits for a key; the sweeps, fizzles and dissolves are unheld | calibration + missing acknowledgement | the beat script is `quest_parity`-pinned (`quest_driver` serializes `delayUnits`), so the getkey must enter through the TypeScript reference first; changing only the device unit would make the speech faster, not right. BLCKTHRN `0x0910`–`0x0bfa`, getkey `0x0b3e` | H-185 |
| D-43 | Blackthorn sacrifice: the burst never reaches the device world-FX layer (the pacer releases `CellExplosion` to `UiSession`, not `consume_event`), and native orders it after the victim clears where the original fires it first | missing presentation beat + ordering | BLCKTHRN `0x041e` burst before `0x0421`–`0x0429` clear; not fixed in Batch 51 | H-186 |
| D-44 | Words of Power are never bound on the device: `QuestWorldServices::words` is empty and the SD pack has no table. Yell therefore never matches, and every dungeon seal stays closed (INIT.GAM `0x32a–0x331` are all `0x00`). No dungeon can be entered in normal play. | **native defect — RELEASE BLOCKER (RB-2)** | Device wiring never finished; the parity harness binds it itself. The fix needs a pack section for DATA.OVL DS `0x4502` (FALLAX … VERAMOCOR). | H-187 (re-opens Y-24) **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-B).** |
| D-45 | The runtime owns no moonstones (`QuestWorldServices::moonstones` unbound). Moongates never transit, Vas Rel Por always prints "Failed!", searching for or using a stone does nothing, and no save can ever record a moved stone. | **native defect — RELEASE BLOCKER (RB-3)** | GAM `0x28a–0x2a9` has no `GameState` field, and the adapter-side owner was never built | H-188 (root cause of H-12/H-13; re-opens Y-33) **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-C).** **Batch 53B: 7E-C run 1 — first transit and Vas Rel Por PASS; the "no return" step is the original's behaviour (H-195, Outcome A); the retest completes as 7E-C′.** |
| D-46 | The device has no ENDMSG.DAT (`end_record` unbound). The Wooden-Box victory is refused, and the final Doom arena's teardown is deferred forever, so only `Alt+D`/`Alt+S`/`Alt+L` answer. | **native defect — RELEASE BLOCKER (RB-1)** | The text was never packed | H-189 **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-A).** |
| D-47 | The Camp apparition and Refuge karma speech show one invented sentence instead of the six KARMA.DAT records | fabricated text; non-blocking | The text was never packed; `bind_rest_services` substitutes a constant | H-190 **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-E).** |
| D-48 | The night moongate tile `0xDC` is never composed at a buried stone (kernel `0x475a`), and the 1,648 ms transit (`0x48a8`, 15 × `delay(2)`) is absent | presentation. Gate visibility is **part of RB-3**; the transit animation is Alpha 3 | No native composition path; the reference uses `activeMoongates` | H-191 **Batch 53: gate visibility SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-C); the transit animation stays Alpha 3.** |
| D-49 | The shipwright/stable purchase confirm does nothing (`ShopServices::ship`/`horse`/`reserve` unbound → `Unsupported`) | **native defect — RELEASE BLOCKER (RB-4)** | Batch 20 hardware FAIL, never fixed; the host fixture copies the omission | H-146 **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-D).** |
| D-50 | The wishing well matches its six words case-sensitively; the original folds case with kernel `0x6f1e` stristr (`and 0x5f`), called from LOOKOBJ `0x00aa` | native **and** reference divergence; minor | The TypeScript note left it "open". Batch 52 proved it from bytes (`native/core/batch52-h22-wish-stristr-disasm.log`); fix the reference first | H-22 |
| D-51 | Cancelling a potion's target picker refunds the potion; the reference consumes it at selection | native divergence; player-favourable | The Use-target cancel path never dispatches | H-45 |
| D-52 | The Z-stats Armaments marker renders `^` as `?` | presentation | The 5×7 face has no `^` glyph | H-63 |
| D-53 | `AlphaRuntime::overlay` replaces every non-Fire TargetSelection prompt (`To phase:`, `Direction?`) with `Aim: empty (x,y)` | presentation. The `To phase:` half rides RB-3 | The reticle readout is not scoped to combat aim | H-12, H-15 **Batch 53: the `To phase:` half SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-C); `Direction?` (H-15) stays Alpha 3 UI.** |
| D-54 | The device ending is transcript text only: `endgame_script` false, `end_narration` unbound. There is no green scene, orb, dissolve, story pages, scroll or terminal freeze. | missing presentation; Alpha 3 | Reachable only once RB-1 is fixed | Y-07 **Batch 53A: the terminal *state* was not presentation and is split out as D-55; the cinematic itself stays Alpha 3.** |
| D-55 | After the ending the device returned to `UiMode::Dungeon` in Doom's enclosed final cell (floor 7, 5,7) and accepted movement; the original never leaves ENDGAME.OVL (`endgame_datestamp` has no `ret`; loops `0x04f9` / `0x0ac9`) | **native defect** (missing terminal state) | Batch 53's own test asserted "play continues" (EV7/EV8/EV10/EV11) | H-192 **Batch 53A: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-A′).** |
| D-56 | The ending's two box questions ("Didst thou bring my box?", "…Didst thou bring it?") are real Y/N prompts in the original (getkey loops `0x0852`, `0x088b`; victory needs `Y` **and** the box at `0x08b9`/`0x08c2`); native and the TypeScript reference answer them from the inventory | native **and** reference divergence | Part of the ending presenter the device does not have (D-54); 7E-A exercises only the box-and-`Y` route | H-193 — Alpha 3, with D-54 |
| D-57 | Two literal `victory` lines follow the report: `UiSession::consume` appends the `GameWon` and `Endgame` events' internal `ending` token to the transcript | fabricated device text; non-blocking | Found by the 53A transcript dump; outside the 53A stop condition | H-194 — queued for the next batch that touches the ending |
| D-58 | A party **standing** on a drawn moongate is not re-sent by it: native (`commands.cpp` `move()` → `CommandEffect::Moongate`) and the TypeScript reference (`Game.move` → `checkMoongate`) test the gate only after a successful step. The original tests the painted map cell under the party at the top of every outdoor-loop iteration (MAINOUT `0x0b00` → `0x48a8`), so it re-fires after a Pass or any non-moving command, and when night falls on a party standing on a buried stone | native **and** reference divergence; minor | With the phase unchanged the outcome is identical (the party is sent onto itself); it differs only if the phase changes while the party stands on a gate (midnight, nightfall). Found while adjudicating H-195; the parity fixtures pin the step-only trigger, so a fix starts in the reference | H-195 — queued; not changed in Batch 53B |
| D-59 | The device never binds `CommandContext::sky` (`SkyRefresh`), so the moon-phase latch is never refreshed (the parity driver `quest_driver.cpp:51` binds it). Device saves carry no valid latch (`-1`), so the gate phase is always recomputed from the day's DATA.OVL `0x1EEA` bytes | native device-binding divergence; minor | Correct for every same-day case (7E-C included). It cannot reproduce the original's "yesterday's phases until the next surface hour" window (`0x4a84`), and a save that carried a valid latch would freeze the gates. Found by the Batch 53B probe | — queued; not changed in Batch 53B |

---

## 5. How to use this ledger

* **Testers:** anything in §2 or §3 is expected. Do not file it. Anything in §4 is
  known; check the audit ID before reporting.
* **A future preservation profile:** §2 and §3 are the restore list. The "toggle
  candidate" column names the ones already isolated enough to switch without
  restructuring; A-1 through A-5 and A-13/A-14 are *not* restorable, because the
  hardware, not a preference, forced them.
* **A future batch:** §4 is the backlog. As of Batch 19 no entry in it was a live
  production defect — D-11, the only one, is fixed and struck through. **Batch 21B
  changed that:** D-19 through D-22 *are* live gaps against the 1988 binary, derived
  and cited, deliberately queued rather than fixed. Everything else remaining is a
  decision, a calibration, or scope.
* **On the word "intentional":** §2 and §3 are deliberate; **§4 is not**. An
  unresolved divergence is an open question about fidelity, not a choice, and the
  two must never be totalled together as "intentional divergences" — a summary row
  in `GAMEPLAY_INTEGRATION_AUDIT.md` did exactly that until Batch 19 corrected it.
  The split is 18 deliberate (14 + 4) and 22 unresolved, of which one is now closed.

**Batch 34 / H-169.** The original pre-sleep passes are restored only for accepted bed sleep. They run before status `S`, `Zzz`, and the first ten-minute tick, and can cancel when the final per-pass marker is hostile; a later talker may overwrite it. The 16/16 focused regression subset and 104/104 fresh host suite pass. H-172 records the separate original bed-entry viewport fill; H-170 and H-171 remain queued. No new hardware phase was allocated.

**Batch 35 / H-170.** Original Q storage is `g_time_spell='Q'` plus a 30-turn counter; it is not the bed target. The bed target is a local wrapped hour with the original subtract-23 quirk, compared at the loop head before each ten-minute advance. Native's fixed `hours * 6` loop could stop early or late while Q changed the clock rate. The target-hour loop restores that behavior without changing shared Q expiry or the Batch 30/34 tick and NPC ordering. Focused 42/42, six mutations killed, 17/17 regression subset and 105/105 fresh host suite; no new hardware phase. H-171 and H-172 remain queued.

**Batch 36 / H-171.** Direct original-byte call-target recovery overturned D-27's survival-housekeeping premise. Camp cancels Q/T on entry; each five-minute step redraws (including wind RNG), rolls regeneration rings, advances the clock, waits one timer tick and optionally moves the guard, with an hourly encounter check after the next redraw/ring. No poison/food/starvation/turn housekeeping or ordinary NPC pass occurs. Native now restores the missing state/RNG and ordering. H-172 remains queued; separate H-173/D-30 records nonzero-minute Camp duration and is unfixed; no new physical phase.

**Batch 37 / H-172.** Original CMDS copy-fills the map window black after `Zzz`, before the first tick; the fill changes no map or NPC state. Native now writes that rectangle synchronously through the Board and restores it on the next ordinary redraw. Host RED 9/12, GREEN 12/12, eight mutations killed, 19/19 affected subset, and 107/107 full suite. Phase 6Z checks actual TFT presentation. H-173/D-30 was not touched. The separate interim roster-refresh omission is queued as H-174/D-31.

**Batch 38 / H-173.** Original Camp stores a target hour modulo 24 and exits at the loop head when the live hour first equals it. The start minute is not a target: 12:05 + one hour ends 13:00 after eleven five-minute steps, while 12:01 first enters the target hour at 13:01 after twelve. A 23:55 + one-hour Camp ends 00:00 after one step; multi-hour midnight wraps retain one encounter check per intervening hour after redraw/ring. Native's fixed twelve steps per hour overran nonzero-minute starts. The target-hour loop restores the stop rule and retains final-step guard movement and Batch 36 per-step ordering. Raw-key RED 12/31, final 43/43, six killed mutations, 9/9 affected regressions, and 108/108 full host suite. No new physical phase. H-174/D-31 remains queued.

**Batch 39 / H-174.** Direct original-byte tracing resolves both bed calls to the resident status renderer: once after G-to-S but before Zzz and black fill, then after housekeeping on every ten-minute tick before NPC snap/ejection. It draws six party rows and lower status fields, clears a sleeping active selection, and never repaints the map. The last sleeping frame persists through wake until ordinary redraw. Native now presents these synchronous panel frames without disturbing the Batch 37 black viewport or Batch 30/34/35 sleep order. Focused RED 9/17, GREEN 17/17, nine mutations killed, 20/20 affected regression and 109/109 full host. Phase 7A remains for physical TFT output; 6T-6Z unchanged. Batch 40 advancement remains untouched.

**Batch 40 / advancement audit.** The original evaluates XP only inside the successful Camp apparition (CMDS 0x04e7-0x0502 to OUTSUBS 0x0658). It derives level as one plus the number of positive right shifts of XP/100, updates every eligible live roster slot in order, sets max/current HP to 30 times level, and draws once per changed member for a capped +1 STR/DEX/INT. Ordinary earned XP is capped at 9999, so normal play tops out at L8. Dead members skip level evaluation but receive class MP recalculation; the apparition separately heals and cures every live member. Bed sleep, ordinary turns and XP awards do not advance levels. Native progression matches, with 21/21 command-path checks, eight killed mutations, 15/15 affected regressions and a fresh 110/110 full host suite; no production source changed. The direct original-byte log and complete findings are in the Batch 40 section of GAMEPLAY_INTEGRATION_AUDIT.md. D-32/H-175 records the separate missing per-member scene staging and acknowledgment; its opt-in host probe fails on unchanged production. No firmware or hardware test was performed. Phase 7B is reserved after that presentation fix; existing phases retain their meanings.

**Batch 41 / H-175.** Original OUTSUBS advances one live eligible member after its wake/chime, prints Hail, and waits for one unfiltered key. MP and status-panel redraw follow the key, before the next slot or its RNG draw. Dead and unchanged slots consume no stat draw or Hail/key wait. Karma speech has a separate key wait, then disappearance and Camp completion. Native now preserves this intermediate state with a transient continuation; device save/load cannot interrupt it, and it is never serialized. RED 0/1 became focused 18/18 and shipped-pack raw-key 9/9, with nine mutations killed, 16/16 affected regressions and fresh 112/112 full host suite. The clean app is 871,424 bytes, +784 from Batch 40; no flash. H-176/D-33 is the distinct, still absent actor wake tile and viewport XOR flash. Phase 7B remains pending; 6T–6Z and 7A are unchanged.

**Batch 42 / H-176.** The shipped OUTSUBS span changes both actor animation frames from sleeping tile 0x11e to class standing tiles (M 0x140, B 0x144, F 0x148, other 0x14c) at the authored CampFire south cell. A single white EGA XOR of the inclusive (8,8)-(183,183) viewport follows chime; post-chord compositor redraw restores it before level/Hail. Native presents those transient frames through a viewport-only Board path, leaving world position and Batch 41 deferred MP/panel draw untouched. Raw-key framebuffer RED 5/7 became 23/23 GREEN; eleven mutations were killed; affected regression 19/19; fresh full host suite 113/113. The clean app is 872,432 bytes (+1,008 from Batch 41); no flash. Phase 7B remains untested. The distinct pre-apparition Camp sleep-scene omission is H-177/D-34, queued without a new physical phase.

**Batch 43 / H-177.** Shipped ULTIMA.EXE/CMDS/OUTSUBS bytes place CampFire party actors, sleepable S status and guard motion before the apparition gate. P remains standing; dead cells are empty. The scene appears on missed gates and successful gates without level gain, then yields to the already restored H-175/H-176 staging on success. Native mounts the existing CampFire viewport during Camp, refreshes the panel and restores the world on miss or ambush without changing normal no-ring RNG order. Raw-key RED 6/11 became GREEN 20/20; nine targeted mutations were killed (including one fixture tightened after a guard-move survivor), affected regressions 19/19 and fresh full host suite 114/114. The clean app is 872,928 bytes (+496 against Batch 42), with 0x2ae20 bytes free; no flash. Phase 7B remains untested alongside 6T–6Z and 7A. H-178/D-35 separately queues the original equipped-ring scene-entry expiry draw.

**Batch 44 / H-178.** Shipped kernel bytes establish two interleaved scene-entry ring behaviors. A living wearer of Invisibility (42) or Regeneration (44) gets one rand(0,15) expiry roll; 11 removes the ring. Every surviving, non-S Regeneration wearer then triggers the party-wide 0x400c pass, one rand(0,7) for each living Regeneration wearer, with +1 HP on 7. This is before CampFire sleep mounting and separate from the once-per-five-minute Camp pass. Poisoned members, guards and full-HP members participate; dead slots do not. A pre-existing S wearer receives expiry but does not trigger its own pass. Native now follows this byte order, including immediate healing and downstream wind, encounter, apparition and advancement shifts. Rejected Camp has no scene-entry rolls; accepted Camp retains them if later ambushed. H-179/D-36 queues the separate standing Invisibility-ring actor visual gap. No new hardware phase is allocated; Phase 7B retains the Camp scene and cue scope, while 6T–6Z and 7A remain pending. No hardware was flashed.
Batch 44 validation: final focused 20/20, affected regression 21/21, fresh full host 115/115, 15/15 targeted mutations killed. Clean firmware app 873,152 bytes (+224 versus Batch 43), 0x2ad40 bytes free. See the Batch 44 audit section and native/core/batch44-final-mutation-summary.log for exact seed and mutation evidence.

**Batch 45 / H-180.** Physical nighttime bed observations separated into an authentic upright named occupant and an invisible dialog-zero guard. Original castle tiles 0x15c/0x170 overlay the underlying 0xab bed, without a town-sleep tile or hidden flag. Native's presentation dialog filter hid the guard while Look and collision still found it. The one-line correction, shipped-pack RED/GREEN fixture and four killed mutations close the host defect. H-179/D-36 remains the separately queued Camp ring-42 gap. Phase 7C reserves device confirmation.

Batch 45 validation: focused 14/14, affected 17/17, fresh full host 116/116, four targeted mutations killed and restored. Clean ESP-IDF app 873,136 bytes (−16 against Batch 44), 0x2ad50 bytes free. No flash.

**Batch 46 / H-179.** Original Camp uses a per-member visible 0x11d outline for surviving Ring of Invisibility wearers who remain standing. Sleepers still use 0x11e; dead cells expose terrain. The posted guard and poisoned P member keep the outline through Camp redraws. Status rows, fire and clock are unchanged. On successful apparition, each live member receives the class standing tile and XOR flash, with ring equipment retained. Native previously painted class tiles for standing wearers. A Camp-only tile selection fixes that without changing Batch 44 RNG. The shipped-pack fixture was RED 13/16 and GREEN 16/16; nine mutations were killed. Phase 7B absorbs physical review; no new phase or flash.
Batch 46 validation: 16/16 focused, 22/22 affected, 117/117 fresh full host, and 9/9 targeted mutations killed. Clean ESP-IDF app 873,184 bytes (+48 against Batch 45), 0x2ad20 bytes free. No flash.

**Batch 47 / H-181 / D-38.** Full shipped EGA tile survey identifies 0x11a (282) as the actual person-in-bed sprite, distinct from Camp 0x11e and the 0xab/0xac static bed. Original EXE 0x5356 branches from bed terrain 0xab to actor byte 0x1a; FONT.OVL adds the actor bank 0x100. Ordinary NPC type >=0x40 bypasses this pose selector, so Batch 45 remains correct. The native world avatar still uses its fixed on-foot tile over a bed head; **H-181/D-38 is open and queued for a focused presentation batch**, with no production change here. All 512 source tiles, a tight candidate sheet, castle context, and the 264-bed shipped map/NPC census are in `re/verified/batch47-bed-tiles`. Parser subset 37/37 and fresh host suite 117/117 pass. No new physical phase or flash.

**Batch 48 / H-181 / D-38.** The center world actor now selects 0x11a when its original-admitted byte meets live 0xab terrain, without changing actor or terrain state. Exact opcode ranges are 0x1c, 0x12-0x15, 0x28-0x2b and 0x40-0x7f; 0x1d/0x1e and inactive/dead render paths take precedence outside the selector. The original-byte trace also corrects the Batch 47 claim that >=0x40 categorically bypasses the selector. This conflicts with the earlier upright scheduled-NPC observation, so the native NPC loop remains unchanged and the historical caller distinction is explicitly unresolved. Shipped castle RED 24/27 became final GREEN 40/40; eight mutations killed, affected 17/17, full host 118/118. Phase 7C adds avatar on/off-bed observation. No device was flashed.

Batch 48 final firmware: 873,344 bytes (0xd5380), +160 against Batch 46, 0x2ac80 bytes free; final affected 17/17 and host 118/118. Commit-stamped Launcher SHA-256 is recorded in the annotated Batch 48 tag.

**Batch 49 — scheduled NPC bed-pose adjudication (unresolved).** Fresh opcode assertions and shipped castle data are in `native/core/batch49-original-npc-bed-trace.log` (regenerator `re/tools/batch49_npc_bed_trace.py`). At 23:45, castle slot 13's authored 0x5c and slot 1's 0x70 target (9,7,0) and (17,7,0); both corresponding CASTLE.DAT heads are 0xab. TOWN 0x17d9 passes the type unchanged to EXE 0x3a74; ordinary EXE 0x55f8 calls the same 0x51b8 selector, whose 0x40-0x7f branch would choose 0x1a **if** the live DS:6608 cell is still 0xab. No original live DS:6608 value or rendered frame was captured. Batch 45's upright observation was on native hardware, so it cannot establish the 1988 pose. Outcome D applies: native production remains unchanged; no new defect ID or RED test is warranted. Obtain a paired original DOSBox-X memory/framebuffer trace of the exact castle slot at 23:45, including both map-entry and in-place schedule transition if possible. Phase 7C remains reserved and cannot settle the original rule from native hardware alone. No flash.

---

## 6. Batch 52 reconciliation (2026-09-25)

Every row checked against the Phase 6T–7D hardware results and the Batch 52 audit (`GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 52"). Earlier cells are kept as written; this table is the current status.

| Row(s) | Current status | Kind |
|---|---|---|
| A-1 … A-14 | unchanged — required hardware adaptations (A-13's identity gate will also stop a stale card once Batch 53 changes the pack) | required hardware divergence |
| E-1 … E-4 | unchanged — deliberate enhancements (scrollback, Developer tools, fountain line, settings) | deliberate UX modernization already present |
| D-1, D-2, D-7 | open product decisions | unresolved (decision) |
| D-3 | audio out of scope; see also the audio-driven items below | Alpha 3 audio |
| D-4, D-9 | open reference questions (Mix underground; world getdir order) | unresolved original behaviour |
| D-5, D-6, D-12, D-13, D-14, D-15, D-16 | incomplete / presentation / deferral | Alpha 3 |
| D-8 | Ready picker presentation (H-139/H-140 never run) | Alpha 3 UI |
| D-10 | **fix hardware-confirmed (7D-B PASS, Batch 51 image).** Residual: true 1988 sweep durations (class B, host-dependent) and fizzle texture (class C) | future strict-preservation-profile candidate |
| D-11, D-23 | resolved (D-23: **HW PASS 6V**) | obsolete historical note |
| D-17, D-18 | dead code, harmless | hygiene |
| D-19 | resolved — **HW PASS 6W** (functional) | obsolete historical note |
| D-20 | resolved — **HW PASS 6X** | obsolete historical note |
| D-21, D-22 | **host fixed — Batch 23** (H-158 floor-change reload; H-159 chest contents `0x1E`, with the TypeScript generator); this ledger never struck them. Device observation owed (Phase 7E) | resolved on host |
| D-24 | resolved — **HW PASS 6Y** | obsolete historical note |
| D-25 … D-28, D-30 | host fixed, no device question (B33–B36, B38) | resolved on host |
| D-29 | resolved — **HW PASS 6Z** | obsolete historical note |
| D-31 | resolved — **HW PASS 7A** | obsolete historical note |
| D-32 … D-36 | resolved — **HW PASS 7B run 2** (logic/visual, Batch 48 image) and **7D-A** (pacing, Batch 51 image) | obsolete historical note |
| D-37 | guard visibility / Look / occupancy — **HW PASS 7C run 2** | resolved; the pose question lives in D-38 |
| D-38 | controlled-actor bed pose — **HW PASS 7C run 2**. **Named/scheduled NPC 1988 sleeping pose still UNRESOLVED** (Batch 49 Outcome D: needs a paired original memory/framebuffer witness) | unresolved original behaviour |
| D-39 | resolved — **HW PASS 7D-A**. Residual: sweep holds are the calibrated fast-host floor (25,806 samples/s); 1988 hosts held the chord 1.18×–1.96× longer — **machine-dependent**, a preservation-profile choice | future strict-preservation-profile candidate |
| D-40 … D-43 | classified, not fixed (H-183 … H-186); **not Alpha 2 blockers** (no lock, no state) | known missing original presentation → Alpha 3 scene fidelity |
| D-44, D-45 (+ D-48 gate visibility, D-53 `To phase:`), D-46, D-49 | **ALPHA 2 RELEASE BLOCKERS RB-2, RB-3, RB-1, RB-4** | native defects (device wiring) |
| D-47 | fabricated karma text, non-blocking; ride Batch 53 (same pack section as D-46) | native defect (text) |
| D-48 transit animation, D-54 | missing original presentation | Alpha 3 presentation |
| D-50, D-51, D-52 | minor divergences | Alpha 3 fidelity / UI |

**Scripted-scene pacing decisions (Batch 51), restated.** Tick waits (`delay` 0x20fa, `run_n_frames` 0x3ae6) are exact. `tone_sweep` 0x2192 holds use the calibrated floor, which depends on the machine. Fizzles are held one tick without their texture. Getkeys stay real key waits. Confirmed on hardware by 7D.

**Missing audio-driven behaviour.**
- There is no audio (D-3).
- The original's `tone_sweep` blocks even with sound off; the device reproduces those waits silently (D-39/D-10).
- The sound-flag branches (the original drags the blindfolded party 3 times instead of 18 with sound OFF, and skips the bard's lute) keep the device's sound-ON drags and sound-OFF lute (Batch 51 §13.5).
- The harpsichord notes are silent (H-125).

**Batch 53 status.** D-44, D-45, D-46, D-49 (the four release blockers), D-47, D-48 gate visibility and D-53 `To phase:`: **SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E)**. Unchanged: D-48 transit animation, D-54, D-50 – D-52, D-40 – D-43 and every row above.

**Batch 53A status.** D-55 (terminal state after the ending): **SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-A′)**. New and open: D-56 (Alpha 3, with D-54), D-57 (queued). New deliberate row A-15. D-46's retest moves from 7E-A to 7E-A′ (run 1 passed its absorption/ending half on the Batch 53 image).

**Batch 53B status.** Phase 7E-C run 1: first moongate transit PASS, destination gate drawn, **stepping back onto the destination gate did not transport the party back**, Vas Rel Por PASS; Test C held. Adjudicated **Outcome A** — the original has no return link: every gate leads to the stone of the current moon phase, which is the one the party just arrived on (ULTIMA.EXE `0x4962`–`0x4977`, `0x47f4`). D-45 / D-48 (gate visibility) / D-53 are unchanged in status: **SOFTWARE FIXED — HARDWARE RETEST PENDING**, now completed by **7E-C′** (revised criteria, no reflash). No production change. New open rows: D-58, D-59 (both minor, queued).

**Batch 54 status (Alpha 2 RC1).** Phase 7E is complete; see the checklist's Batch 54 section.
- **HARDWARE PASS:**
  - D-44 (7E-B);
  - D-45 with D-48 gate visibility and D-53 `To phase:` (7E-C, return step adjudicated original in 53B);
  - D-46 and D-55 (7E-A′);
  - D-49 (7E-D).
- **Accepted:**
  - D-47: the Refuge was witnessed on the device, and its KARMA.DAT text is host-certified;
  - D-21 and D-22: 7E-G, previously physically confirmed.
- **No row changed kind, and no row was fixed in Batch 54.**
- **Still open and non-blocking:**
  - D-56, D-57, D-58 and D-59;
  - the D-48 transit animation and D-54;
  - D-40 – D-43, D-50 – D-52, D-38's NPC pose;
  - the Batch 52 open decisions, questions and Alpha 3 items.

  The Alpha 2 release notes (`ALPHA2.md`, repository root) list them for testers.

**Batch 55 status (Alpha 2 released / closed).** The Phase 8 RC1 hardware smoke passed on the device: steps 1–8 PASS on `Git 211c676a1dca`, `RES … CRC 26f75ae6`, `ASSET … CRC 933c9b82` (checklist, Batch 55 section).
- RC1 is promoted byte for byte to the final Alpha 2 release, tag `alpha2-batch55-release`.
- No hardware regression was found. Alpha 2 hardware validation is complete.
- **No row changed kind or status, and no row was fixed in Batch 55.** Every row still open after Batch 54 is non-blocking and carries its milestone into Alpha 3 (`ALPHA2.md`, "Alpha 3 handoff").

**Totals after Batch 55.** As after Batch 54:
- 19 deliberate;
- no software-fixed row awaiting hardware;
- the same open, non-blocking rows.

**Totals after Batch 54.**
- 19 deliberate (unchanged).
- Software-fixed rows awaiting hardware: **none**.
- Open, non-blocking: D-56 and D-57 (ending); D-58 and D-59 (moongate); plus everything listed as open after Batch 52 except the release blockers, all of which are now closed.

**Totals after Batch 53B.** 19 deliberate (unchanged). Software-fixed rows awaiting Phase 7E: as after Batch 53A, with D-45's retest moved from 7E-C to 7E-C′. New open rows: D-58, D-59 (queued), in addition to D-56 (Alpha 3) and D-57 (queued).

**Totals after Batch 53A.** 19 deliberate (A-1 … A-15, E-1 … E-4). Software-fixed rows awaiting Phase 7E: D-44, D-45 (with D-48 gate / D-53 prompt), D-46 and D-55 (7E-A′), D-47, D-49. New open rows: D-56 (Alpha 3), D-57 (queued).

**Totals after Batch 53.** 18 deliberate (A-1 … A-14, E-1 … E-4). Software-fixed rows awaiting Phase 7E: D-44, D-45 (with D-48 gate / D-53 prompt), D-46, D-47, D-49. Open decisions and questions and the Alpha 3 items are as listed for Batch 52 below.

**Totals after Batch 52.** 18 deliberate (A-1 … A-14, E-1 … E-4). Unresolved or queued rows open at Batch 52:
- 4 release blockers (D-44, D-45 with D-48/D-53, D-46, D-49);
- 1 fabricated-text defect (D-47);
- open decisions and questions (D-1, D-2, D-4, D-7, D-9, the D-38 NPC pose, the D-10/D-39 residuals);
- Alpha 3 items (D-3, D-5, D-6, D-8, D-12 … D-16, D-40 … D-43, D-48 animation, D-50 … D-52, D-54).

Deliberate and unresolved rows are still never totalled together.
