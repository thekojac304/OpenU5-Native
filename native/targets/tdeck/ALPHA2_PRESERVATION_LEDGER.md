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
The "toggle candidate" column records how isolated each difference already is —
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
| D-11 | Crystal ball opens a fabricated `"Peer into it?"` yes/no; the reference opens the kernel 0x4988 character picker, and the native command is consequently dead | **production defect** | the faithful fix needs `resolve_command_char` as a shared seam, also owed to `(S)earch` and `(C)ast` | R-25 |
| D-12 | The zodiac view is still clipped by the 9 px sky/wind strips | presentation | the gem view got `full_square_viewport` in Batch 10; zodiac was out of that scope | §6, R-13 |
| D-13 | No `"Player: <name>"` / `"Status: "` console echo during Z-stats member selection | presentation | filed as a minor follow-up in Batch 14, never scheduled | Batch 14 residual 6 |
| D-14 | Digit shortcuts work inside the Z-stats page loop but not inside the member picker | scope | the picker's digit selection is a kernel behaviour outside the overlay; wiring it changes digit handling for **every** party picker | Batch 14 residual 7 |
| D-15 | Pocket Watch (extended id 0x23) has no row in the `(U)se` picker or the Z-stats Items page | missing state | no `GameState` field backs possession, so no row can be gated | R-08, Batch 14 residual 5 |
| D-16 | "Transfer from Ultima IV" is a notice, not a feature | explicit deferral | `frontend.cpp:88` | §2A |
| D-17 | `render_active_view()` (~45 lines) has no caller but is still linked into the firmware | dead code | superseded by `render_snapshot()`; confirmed still present in `openu5_tdeck.map` at Batch 18 | §5 |
| D-18 | `CommandKind::ShopAction`, `CommandKind::CombatEscape` and `CommandKind::Unready` are handled but have no producer | dead code | each is deliberate; re-verified at Batch 18 | §5 |

---

## 5. How to use this ledger

* **Testers:** anything in §2 or §3 is expected. Do not file it. Anything in §4 is
  known; check the audit ID before reporting.
* **A future preservation profile:** §2 and §3 are the restore list. The "toggle
  candidate" column names the ones already isolated enough to switch without
  restructuring; A-1 through A-5 and A-13/A-14 are *not* restorable, because the
  hardware, not a preference, forced them.
* **A future batch:** §4 is the backlog. D-11 is the only entry that is a live
  production defect; the rest are decisions, calibration, or scope.
