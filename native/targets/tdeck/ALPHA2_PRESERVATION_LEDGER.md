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
| D-10 | Blackthorn capture scene pacing feels collapsed against original footage | calibration | **no footage-derived timing evidence for this scene exists anywhere in the repository** (`re/notes/blackthorn-escena-324.md` §4 disclaims a witness; the TypeScript `PAUSE_UNIT_MS` is an admitted reuse of TrollSneak's calibration). Inventing a number is out of bounds | Y-32 |
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
| ~~D-29~~ | ~~Original bed entry clears viewport rectangle after `Zzz` and before the first ten-minute tick; native lacked a matching fill~~ **HOST FIXED — Batch 37; Phase 6Z pending.** The renderer-only black map-window fill is synchronous, persists through bed ticks, and is restored by the next ordinary redraw. | ~~queued~~ **host fixed** | CMDS `0x0614-0x0624`; raw-key framebuffer RED 9/12, GREEN 12/12, seven mutations killed, 107/107 host | H-172 |
| ~~D-30~~ | ~~Camp from a nonzero minute ran twelve five-minute steps per requested hour instead of stopping at the original target hour~~ **HOST FIXED — Batch 38.** Camp now compares the live hour to the wrapped target before each step; 05:50 + one hour ends at 06:00 after two steps. | ~~incomplete, queued~~ **host fixed** | CMDS 0x0066-0x0079 and 0x01ee-0x01f8; raw-key RED 12/31, final 43/43, six killed mutations, 108/108 host; no new physical phase | H-173 |
| ~~D-31~~ | ~~Native omits intermediate bed status frames~~ **HOST FIXED - Batch 39; Phase 7A pending.** The panel shows S before Zzz, then post-housekeeping HP/clock each tick while the map stays black. The original row draw also clears an asleep active selection. | ~~queued~~ **host fixed** | CMDS `0x060a`/`0x0674` -> EXE `0x2900`/`0x2726`; framebuffer RED 9/17, GREEN 17/17, nine mutations killed, 109/109 host | H-174 |
| D-32 | Camp apparition member staging: native applies all roster changes before emitting the per-member chimes and Hail messages, without the original per-member key holds or panel redraw | confirmed incomplete presentation | OUTSUBS 0x07fb-0x090e interleaves state, actor wake, chime/flash, conditional message/getkey and status redraw per live slot. Batch 40 opt-in host probe is RED 0/1. Core advancement remains correct and independently tested. Queue staged UI work; Phase 7B is reserved for device observation after a fix. | H-175 |

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

**Batch 37 / H-172.** Original CMDS copy-fills the map window black after `Zzz`, before the first tick; the fill changes no map or NPC state. Native now writes that rectangle synchronously through the Board and restores it on the next ordinary redraw. Host RED 9/12, GREEN 12/12, seven mutations killed, 19/19 affected subset, and 107/107 full suite. Phase 6Z checks actual TFT presentation. H-173/D-30 was not touched. The separate interim roster-refresh omission is queued as H-174/D-31.

**Batch 38 / H-173.** Original Camp stores a target hour modulo 24 and exits at the loop head when the live hour first equals it. The start minute is not a target: 12:05 + one hour ends 13:00 after eleven five-minute steps, while 12:01 first enters the target hour at 13:01 after twelve. A 23:55 + one-hour Camp ends 00:00 after one step; multi-hour midnight wraps retain one encounter check per intervening hour after redraw/ring. Native's fixed twelve steps per hour overran nonzero-minute starts. The target-hour loop restores the stop rule and retains final-step guard movement and Batch 36 per-step ordering. Raw-key RED 12/31, final 43/43, six killed mutations, 9/9 affected regressions, and 108/108 full host suite. No new physical phase. H-174/D-31 remains queued.

**Batch 39 / H-174.** Direct original-byte tracing resolves both bed calls to the resident status renderer: once after G-to-S but before Zzz and black fill, then after housekeeping on every ten-minute tick before NPC snap/ejection. It draws six party rows and lower status fields, clears a sleeping active selection, and never repaints the map. The last sleeping frame persists through wake until ordinary redraw. Native now presents these synchronous panel frames without disturbing the Batch 37 black viewport or Batch 30/34/35 sleep order. Focused RED 9/17, GREEN 17/17, nine mutations killed, 20/20 affected regression and 109/109 full host. Phase 7A remains for physical TFT output; 6T-6Z unchanged. Batch 40 advancement remains untouched.

**Batch 40 / advancement audit.** The original evaluates XP only inside the successful Camp apparition (CMDS 0x04e7-0x0502 to OUTSUBS 0x0658). It derives level as one plus the number of positive right shifts of XP/100, updates every eligible live roster slot in order, sets max/current HP to 30 times level, and draws once per changed member for a capped +1 STR/DEX/INT. Ordinary earned XP is capped at 9999, so normal play tops out at L8. Dead members skip level evaluation but receive class MP recalculation; the apparition separately heals and cures every live member. Bed sleep, ordinary turns and XP awards do not advance levels. Native progression matches, with 21/21 command-path checks, eight killed mutations, 15/15 affected regressions and a fresh 110/110 full host suite; no production source changed. The direct original-byte log and complete findings are in the Batch 40 section of GAMEPLAY_INTEGRATION_AUDIT.md. D-32/H-175 records the separate missing per-member scene staging and acknowledgment; its opt-in host probe fails on unchanged production. No firmware or hardware test was performed. Phase 7B is reserved after that presentation fix; existing phases retain their meanings.