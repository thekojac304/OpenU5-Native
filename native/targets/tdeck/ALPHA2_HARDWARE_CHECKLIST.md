# Alpha 2 — Consolidated Physical T-Deck Checklist

**Created:** Batch 18 (2026-09-21) · **Supersedes** the per-batch phase lists in
[`GAMEPLAY_INTEGRATION_AUDIT.md`](GAMEPLAY_INTEGRATION_AUDIT.md) §16 for *execution* purposes.
§16 stays as the archaeology — it records which batch owed which step and why.
**This file is the single list to run in one device session.**

> **Nothing in this file has been executed.** No physical T-Deck was available
> during Batch 18 or Batch 19. Every row's result column reads `UNTESTED`. Do not infer a
> PASS from a host-suite green: the whole point of these rows is that they are
> the checks host evidence cannot make.
>
> **Batch 52 correction:** this banner was true at Batch 18/19 and is kept as history. Batch 20 ran 117 rows on hardware, and Phases 6T–7D followed. The authoritative status is the "Alpha 2 Readiness — Batch 52" section at the end of this file.

## Session setup

| Item | Value |
|---|---|
| Firmware image | **The current validation image is the Launcher named in the annotated tag of the batch that owns the phase you are running** (for Phase 7D: tag `alpha2-batch51-scene-pacing`). *Batch 51 correction: this row used to name `build-batch19`; it was never updated, and in the first Phase 7B/7C session a stale Batch 43 image was flashed instead of Batch 48.* Every batch packages the same filename, `OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, so the filename proves nothing. *Batch 52: no new image. The next phase, 7E, will name the Batch 53 tag, and that batch changes the SD pack as well.* *Batch 53A: Phase 7E continues on the Batch 53A image (tag `alpha2-batch53a-ending-terminal`); the SD pack is the Batch 53 one, unchanged.* *Batch 55: Alpha 2 final is the RC1 image byte for byte (`OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`, SHA-256 `ff3dfe19…5828`). Its `Git` line reads `211c676a1dca`, the commit of `alpha2-batch54-rc1`, NOT the commit of the release tag `alpha2-batch55-release`, which is a docs-only commit on top of it.* |
| **Firmware identity gate** | **Before recording ANY physical result, read `Git <hash>` on the boot identity screen and confirm it equals the first 12 hex digits of the commit the owning tag points to** (`git rev-list -n1 <tag>`); write that hash into the result cell. On disk the same check is `grep -a -o <hash> <image>`. A result recorded against a different hash certifies nothing about the phase and must be struck, not reinterpreted. |
| **SD resource pack** | **NO REFRESH REQUIRED.** Neither Batch 18 nor Batch 19 changed any resource file. The last pack change was Batch 9C (`openu5-alpha1-resources.bin`, 2,039,545 B, payload CRC32 `0x2065ad91`, SHA-256 `434cd664…b4ea`). If the card already boots a Batch 9C-or-later image, leave it alone. |
| If the card is older than 9C | `npm run pack:alpha1`, then copy `native/assets/openu5-alpha1-resources.bin` over `<SD>:\ultima5\openu5-alpha1-resources.bin`. **Do not reformat**; saves and settings are separate files. A stale card stops at the identity screen with `match=0` — that is the gate working. |
| Developer tools | Required for most rows (`Alt+D`). Built in when `OPENU5_ENABLE_DEVELOPER_TOOLS=ON`. |
| Serial log | Capture it for the whole session. Several rows name the exact log line that proves them. |

**Global pass criteria** (from §16, unchanged): no phase ends in a mode that
accepts no input; no command reports success without a visible authoritative
change; the log contains no `UNRESOLVED_NAME`, no
`Command failed status=invalid context` for a command the UI offered, and no
`PRESENTATION_DISPATCH` whose source contradicts `UI_MODE`.

**Expected non-defects.** Before filing anything, check
[`ALPHA2_PRESERVATION_LEDGER.md`](ALPHA2_PRESERVATION_LEDGER.md) §2–§3. The
recurring false alarms are: no box frame around a Z-stats list, `M`/`F` instead
of `♂`/`♀`, unabbreviated item names, canonical names instead of rune sigils, no
Pocket Watch row, an enemy standing on a black cell in a dungeon room, a *green*
In Nox Grav field, and In \*Grav seeding **no** field in a combat arena.

---

## Group 1 — Boot, identity and the input contract

*Regression guard. Do not skip; every later row depends on it.*

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-01 | Card per setup table | Power on | `IDENTITY … match=1`, then the identity screen, then the title | boots to frontend | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-02 | Title | wait 20 s, then any key | attract demo runs, then the menu | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-03 | Menu | `U` Introduction, page through; `A`; `R`; `S` (change brightness + trackball, Back) | each screen renders; settings persist across a power cycle | settings file written | Back returns to menu | PASS (Batch 20, hardware, `a357e28c…`) |
| H-04 | Menu | `C` Create New Character → name, gender, full questionnaire | enters gameplay as the new identity | `objects_` empty, party is the new identity | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-05 | Power-cycle after H-04 | `J` Journey Onward | the created save loads | position/party restored | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-06 | In world | Mic **short** press | Cancel/back | **no `0` appears in any text field** | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-07 | In world | Mic **hold ≈1.1 s** | `INPUT_HOLD … emitted=movement-toggle state=ON` | Movement Mode ON | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-08 | Movement Mode ON | WASD; then `Y` → type `wasd` | WASD moves; inside the Yell text row the **literal characters** appear | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-09 | In world | Trackball all four directions; `Alt+M` open and close | movement in four directions; System Menu opens and closes | — | `Alt+M` again closes | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 2 — Y-series input and magic closeout (Batch 16)

*The three checks Batch 16 left owed. Firmware only.*

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-10 | World, party ≥2, an active player set via `1`–`6` | **`Symbol` held + Mic pressed and released** (do **not** hold past 1.1 s) | the `Set Active Plr:` echo, resolving to *no* active member | `active_character` returns to `255` | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-11 | Immediately after H-10 | plain Mic short press, then a plain 1.1 s hold | short = Cancel as before; hold = Movement Mode toggle as before | unchanged contract | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-12 | World, on foot, a caster with Vas Rel Por mixed and ≥ its mana | `C` → `Vas Rel Por` | the prompt **`To phase:`** appears | none yet | any non-`1`–`8` key, and the device Cancel, **fail silently** — no banner, no teleport | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Screen shows `Aim: empty (-1,-1)` with a `Move\|Confirm\|Mic Back` footer instead of `To phase:`. Root cause (proven): `AlphaRuntime::overlay()` (`alpha_runtime.cpp:1489`) unconditionally renders the generic combat-reticle overlay for ANY non-`Fire` `UiMode::TargetSelection` request, clobbering the real prompt text set by `begin_target(...,"To phase:",...)`. Confirmed NOT cosmetic-only for GatePhase specifically: a bare `2` and `e` (no Sym) both produced `"Failed!"` with no ceremony flash at all, even though `ui_session.cpp:665-673` correctly accepts `'1'`-`'8'` for `GatePhase` — the digit never reaches that handler, pointing to a T-Deck key-routing gap upstream, not yet localized. **Blast radius confirmed wider in H-15:** the same `overlay()` clobbering also hides the plain world `Direction?` getdir text (harmless there, since Move+Confirm is the correct interaction for a spatial direction pick — only `GatePhase`'s non-spatial digit entry is functionally broken by it). Deferred to Batch 21 (isolated to targeting-prompt presentation + this one spell's input path, does not block the rest of the checklist) **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-C).** |
| H-13 | At the H-12 prompt | a digit `1`–`8` | the ceremony flash, then the party is **elsewhere**, with **no** `Success!`/`Failed!` banner | moonstone teleport applied | — | **FAIL — same defect as H-12.** Cannot be exercised; the phase digit never registers, so the ceremony/teleport never fires **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-C).** |
| H-14 | **Aboard a ship or skiff**, same caster | `C` → `Vas Rel Por` | **the `To phase:` prompt must not appear at all** — the ship check precedes the print (`CAST.OVL 0x0cf6`) | spell/mana still spent per the reference, no teleport | — | PASS (Batch 20, hardware, `a357e28c…`) — immediate silent `Failed!`, no prompt shown at all, matching the aboard-ship abort path |
| H-15 | World, carrying a **Rel Hur** scroll (count noted) | `U` → Rel Hur → **cancel** the `Direction?` prompt | the direction row appears and closes | **the scroll count is DOWN by one** — the reference consumes at selection, before the getdir | this *is* the cancel path | **PASS for the wind-change/consume-at-selection behavior (Batch 20, hardware, `a357e28c…`)** — wind changed correctly to match the selected direction. **FAIL (presentation-only, same defect as H-12):** no `Direction?` text appears in the transcript, only the generic `Aim: empty (x,y)` overlay — functionally harmless here since Move+Confirm is the right interaction for a direction pick. Enemies observed spawning near the party after use are **not related to Rel Hur** — `outdoor_turn()`'s `has_spawn`/`SpawnRoll` random-encounter check (`turn.h:48-55`) runs on every outdoor turn via the same shared `turn()` path any turn-consuming command goes through; Rel Hur's own effect code (`magic.cpp:176-181`) only touches wind. Not a defect |
| H-16 | World, carrying a **skull key** (count noted) | `U` → skull key → **cancel** the `Direction?` prompt | same shape | **the key count is DOWN by one** | this *is* the cancel path | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 3 — The wishing well (Batch 18 / R-26, R-34)

*New in Batch 18. Firmware only; the card is unchanged.*

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-17 | Stand beside a well (tile 161) with gold > 0 | `L` + the direction of the well | transcript reads **`a well.`** and the prompt **`Drop a coin?`** | none | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-18 | At the H-17 prompt | `N` | the transcript echoes **`No`** and the prompt closes | no gold spent | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-19 | Re-open the prompt | **Mic short press (Cancel)** | identical to H-18 — it echoes **`No`** and closes. Before Batch 18 the key was simply ignored and the prompt stayed open | no gold spent | this *is* the cancel path | PASS (Batch 20, hardware, `a357e28c…`) |
| H-20 | Re-open the prompt, gold > 0 | `Y` | echoes **`Yes`**, then the wish row opens with the prompt **`Thy wish?`** (not "What dost thou wish?") and **the prompt text is visible** — R-34 | 1 gold spent on submitting the wish | Cancel closes the wish row | PASS (Batch 20, hardware, `a357e28c…`) |
| H-21 | Developer → set gold to **0**, then re-open the prompt | `Y` | echoes **`Yes`** and **stops** — no wish row, and **no** `Thou hast no coin!` (that line is a fabrication) | nothing | — | PASS (Batch 20, hardware, `a357e28c…`) — echoes `Yes` and stops, matching expectations |
| H-22 | At Paws or Empath Abbey, gold > 0 | `L` at the well → `Y` → type `Horse` | `Poof!` and a horse appears on the adjacent walkable cell | 1 gold spent, horse placed | — | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Tester confirmed testing at Paws (location 22, correctly gated) and got `No effect...`. Root cause (proven): `look.cpp:49`'s wish-word match is raw case-sensitive `wish.find(u"Horse")` — no case folding. This codebase's own established pattern for identical keyword matching (`quest.cpp:31-33`'s `contains()`, used for shrine mantras/NPC keywords) uppercases both sides first, matching the original's documented `and 0x5f` case-insensitive fold (`re/notes/shrines.md:146`). `look.cpp` never applies this folding, so any capitalization other than the exact literal fails silently with `No effect...`. Narrow, isolated, deferred to Batch 21 |
| H-23 | **R-34 general check.** Any chained modal: shrine `Visit?` → `Virtue?` → `Mantra?`; a Blackthorn interrogation round; `U` a potion → `On whom?` | run each | **every follow-up prompt shows its text.** Before Batch 18 the second modal in any chain rendered with an empty prompt row | — | each cancels normally | PASS for the wishing-well/potion chains tested so far (Batch 20, hardware, `a357e28c…`). Shrine `Visit?→Virtue?→Mantra?` and the Blackthorn interrogation round not yet reached — will be covered in the dungeon/Blackthorn leg |

---

## Group 4 — Overworld, town and NPCs

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-24 | Debug → Teleport → Britannia | move around | day/night, wind bar, moon marks all change | clock advances | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-25 | Teleport → Britain | observe | NPCs visible and moving on schedule | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-26 | Beside an NPC | `T` → name / job / bye | **the conversation survives more than one keypress** (R-01) | — | Mic exits cleanly | PASS (Batch 20, hardware, `a357e28c…`) |
| H-27 | Town | `L` at a sign; `S`earch; `O`pen a door; `J`immy a locked door; `K`limb; `P`ush furniture | each reports its own authoritative result | as each command implies | direction prompts cancel free | PASS (Batch 20, hardware, `a357e28c…`) |
| H-28 | Town | enter a blacksmith, buy an item, back out one level at a time | the world view and Exploration verbs return (R-01) | gold/inventory change | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-29 | Repeat H-28 | at an inn (Rest), a healer (Heal), a tavern (Rations + Rumour), a reagent shop | same | same | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-30 | ~~Lord British's castle basement, Gorn's brazier~~ **CHECKLIST DEFECT (Batch 20) — corrected location: the Palace of Blackthorn's basement/jail (`smallmaps.json` location 18, floor -1), where Gorn is imprisoned, per `GAMEPLAY_INTEGRATION_AUDIT.md` R-33 and R-32's own location note. Not Lord British's castle** | `S`earch the brazier | the keys are **findable** (R-33: the basement floor byte fix) | keys granted | — | BLOCKED — tester searched the wrong location (LB castle's own basement storeroom, not Blackthorn's jail) per the checklist's incorrect original wording. Needs retest at the corrected location |
| H-148 | **ADJUDICATED IN BATCH 21B — SOFTWARE FIXED, HARDWARE RETEST REQUIRED.** Lord British's Castle, location 17, **basement (floor -1)**: the skull-key vault, chests at (16,21) (17,22) (13,23), beds at (13,19) and (16,19) | Loot all three chests, `(G)et` the loot off the floor, then `(H)`ole up **on a basement bed** for the minimum 1 hour, then re-loot | All three chests reappear at those exact cells, untrapped, openable again, with the skull-key door still open. Repeatable with no cooldown | chests re-seeded | — | **CONFIRMED ORIGINAL — FIXED.** Derived from the 1988 binaries this batch, not from the community report: `CMDS.OVL:0x0552 cmd_camp_holeup` — reachable only on a bed tile `0xAB` (`ULTIMA.EXE:0x32b9`) — calls `TOWN.OVL:0x1694 town_populate_npcs` at `0x0677` on **every 10-minute tick** of its sleep loop. That one routine zeroes all 31 interior object slots and re-places every `.NPC` slot with a non-zero type; a chest is type 1 and is re-seeded with contents `0x1E`, untrapped, with no persistence bit consulted (`TOWN.OVL:0x1795`), because `open_chest_world` records nothing when you loot one. The authored vault is real `CASTLE.NPC` data (slots 23/24/25). The report was right in every clause, including the skull key: the door is a **map tile** the reset never touches, relocked only by a map reload — which is what a floor change does (`town_use_ladder` `0x052e`). Native ran only the NPC half of that routine; the object half is now wired into the same per-tick hook (`commands.cpp`, `CommandKind::Rest`, bed path). Pinned by `batch21b_chest_reset` (38 checks, 6 mutations dead). **Retest: audit Phase 6P** |
| H-31 | A shrine | Visit → Virtue → Donate | the sequence completes and **the game is still playable afterwards** (R-01, Y-25) | shrine bitmap updated | — | PASS (Batch 20, hardware, `a357e28c…`) — donated 5 cycles at Shrine of Justice, charged exactly 500gp (`shrine.cpp:151`'s `n*100` formula) and printed `ALAKAZAM!` (`shrine.cpp:68`, the genuine success line), game remained playable afterward |

---

## Group 5 — Combat, loot and the Batch 2 anchors

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-32 | Debug → Max Party, Max Resources, Equip Best Gear | trigger an overworld encounter | an arena opens | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-33 | In combat | move; `A`ttack with the reticle; `F`ire; `C`ast a combat spell | each resolves with a visible result; the `F`ire projectile flies cell by cell as a sub-cell dot, never a tile write (Y-04) | HP/ammo change | reticle cancels free | PASS (Batch 20, hardware, `a357e28c…`) |
| H-34 | In combat | `R` → select a weapon | **the weapon actually changes, and the character then attacks with it** (R-06 + the `CombatActor` cache resync) | equipment changed | closing the picker spends the combat action | PASS (Batch 20, hardware, `a357e28c…`) |
| H-35 | Win the fight | observe | **the arena stays open** | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-36 | In the open arena | `O`pen the chest, `S`earch it, `G` + direction repeatedly to zero | each `G` names an item | pile drains to 0 | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-37 | **ANCHOR 1b** — after H-36 | observe the emptied cell | **plain arena floor — no blue square, no leftover symbol** (R-02) | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-38 | **ANCHOR 2** — during H-36 | before each `G`, note the visible icon | **the message names the same item the icon showed** (R-04, two-layer composition) | — | — | PASS (Batch 20, hardware, `a357e28c…`). Initial attempt was confounded by the "Combat Test Setup" preset's Max Resources maxing several categories to cap, legitimately triggering `Nothing to get!` (see `combat.cpp:1389-1399`/`loot.cpp:116-155` — not a defect). Retested with resources below cap: items are attainable and icon/message match confirmed |
| H-39 | **ANCHOR 1a** — exit via Mic | look at the encounter coordinate on the overworld | **no blue tile-1 cell** (R-03, `gameplay_parity` mismatch 59) | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-40 | After H-39 | save, reload | **loot left behind is still there** (R-14) | `objects_` restored | — | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 6 — Items, magic and the views

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-41 | Carrying a spread of tools | `U`se | **every owned tool listed with the correct name; "Grapple" absent; the Amulet present** (R-07/R-08). Pocket Watch absent is expected | — | Mic closes the picker | PASS (Batch 20, hardware, `a357e28c…`) |
| H-42 | Party with a healthy member | `U` a **Blue** potion on them | exactly `Item: Potion` and **no second line** — in particular **not** `No effect!` (R-21) | potion spent | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-43 | Poison a member (Green potion), then | `U` a **Red** potion on them | `Item: Potion` then `Poison cured!` — this is the control for H-42 | poison cleared | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-44 | During H-43 | watch the screen | **the flash/inversion happens BEFORE the `Poison cured!` line** (`CAST.OVL 0x139b` precedes `0x13a1`) | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-45 | Any potion | `U` it and **cancel** the "On who" picker | `Item: Potion`, the count **down by one**, and **no flash** — the original's own behaviour | potion spent | this *is* the cancel path | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Potion count does NOT decrease on cancel. Root cause (proven): `alpha_runtime.cpp:910`'s modal-cancel handler for `UiRequestId::UseTarget`/`Inventory` only resets `pending_use_item_=-1` and never calls `command(c)`, so `world_magic.cpp:25`'s unconditional `consume_potion()` (confirmed correct — decrements before checking the target) never runs. Direction-based use-items (skull key, item 17) route through a different, working cancel path (see H-15/H-16 passing). Same class of bug as Y-31 (Batch 16), in the sibling PartySelection-cancel path that fix didn't reach. Narrow, isolated, deferred to Batch 21 |
| H-46 | Carrying all eight scrolls | `U` each in turn | each echoes the bare word `Scroll`, then that scroll's own line (`Light!`, `Wind change!`, `Protection!`, `Negate magic!`, `View!`, `Summon Daemon!` + `Not here!`, `Resurrection!`, `Negate time!`). **No scroll echoes its own name** (R-21) | scroll spent | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-47 | In combat | `U` a scroll, then a potion | same rules as H-42/H-46 inside the arena | — | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-48 | World, at a locked door | cast **An Sanct**; then **In Por** on the overworld; then **An Ex Por** at a town door | each raises `Direction?`, echoes the direction, and applies (R-11) | door/position change | Cancel refunds nothing — the reference consumes first | **PASS (Batch 5)** |
| H-49 | Carrying gems | `V` | **photograph.** A legible map, the gem count decremented, and closing charges exactly one turn (`VIEW_EFFECT`/`VIEW_RESULT`) — ANCHOR 3 / R-17 | gem spent, 1 turn | any key closes | PASS (Batch 20, hardware, `a357e28c…`) |
| H-50 | At a crystal ball, party of 2+ all `G`/`P`, **no** active player set | `L` at it | **REWRITTEN IN BATCH 19 — R-25 is fixed; this row now flips an observation instead of recording a defect.** A `Player: ` roster picker opens (NOT the old `Peer into it?` yes/no, which is deleted), and **no** `Thou dost see` line appears | none yet | — | PASS (Batch 20, hardware, `a357e28c…`) |
| H-141 | Continuing H-50 | pick a member with **high** INT | `Strange vision!` and the 32x32 gem map opens. **Photograph.** The **gem count must NOT change** and closing must charge **no** turn (the `dec [g_gems]` lives in the `(V)` case, outside this route) | gems unchanged, no turn | any key closes | PASS (Batch 20, hardware, `a357e28c…`). Code-verified: `look.cpp:16-19`'s high-INT branch only ever emits `GemView`, never touches HP — the party's low HP visible in the retest screenshots predates this and is unrelated (see H-142) |
| H-142 | At a crystal ball, a member with **low** INT | `L`, pick that member | `Death vision!` and **exactly 1 HP** off **that** member — not member 0, and not the active player if they differ | 1 HP | — | PASS (Batch 20, hardware) — retested with a healthy party member; earlier attempt was confounded by pre-existing low HP, not a defect |
| H-143 | At a crystal ball, party where one member is dead/`D` | `L`, pick the **disabled** member | `Disabled!` and the `Player: ` prompt **comes back** — a re-ask, not a rejection and not a silent no-op. Then cancel: `None!`, no vision, no HP change | none | Mic-short = cancel -> `None!` | PASS (Batch 20, hardware) — retested with a party of 2+ eligible members; earlier attempt was confounded by having only one eligible member, not a defect |
| H-144 | At a crystal ball with an **active player** set (arrow), or with exactly **one** `G`/`P` member left | `L` at it | **no prompt at all** — the vision fires immediately on that member. With **zero** `G`/`P` members: `None!` and nothing else | per branch | — | PASS (Batch 20, hardware, `a357e28c…`) — this is exactly what was observed in the H-143/H-145 attempts: single eligible member, no prompt, vision fired immediately |
| H-145 | Party of 2+, **no** active player | `S`earch a direction, then `C`ast | **both** raise the same `Player: ` picker. `S` asks **after** the direction; `C` asks **before** the spell list. Cancelling either prints `None!` and abandons the command (no search result, no spell menu). With an active player set, **neither** asks | per command | `None!` | PASS (Batch 20, hardware) — retested with a healthy party of 2+ |
| H-51 | Cast Wis An Ylem, or read an In Quas Wis scroll | observe | the map reveals for ~1.1 s and input is swallowed for the same window (R-12) | 1 turn | — | INCONCLUSIVE — likely test-location confound (Batch 20, hardware, `a357e28c…`). Tester saw the ceremony flash and cast confirmation but no visible map change. Code-verified real: `alpha_runtime.cpp:396` arms a timed window, and `presentation.cpp:76-77` force-fills the entire visibility buffer to "visible" during that window, overriding normal line-of-sight — a real, meaningful effect, but only visible where sightlines are normally obstructed. Tested near an open area (Lycaeum) where LOS may already be unobstructed, so there was nothing extra to reveal. Needs retest somewhere with normally blocked sightlines (forest/hills) |
| H-52 | `U`se the Spyglass at night | observe | the zodiac view draws stars/signs/Shadowlord lines and closes on any key, charging no turn (R-13). *Expected residual: the view is clipped by the 9 px strips — D-12* | none | any key | PASS (Batch 20, hardware, `a357e28c…`) |
| H-53 | Any party | `M`ix a spell | the spell list opens and the mix resolves. *Expected residual: quantity is always 1 — D-6* **(A3-HF10: superseded — the reagents are now marked by hand and the quantity asked; Phase H-213)** | reagents spent | — | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 7 — The (Z)-stats page family (Batch 14 / R-22)

*Setup: ≥3 members with deliberately different stats; some gold/food/keys/gems/torches; a few reagents; a couple of mixed spells; ≥8 distinct armaments; one member fully equipped, one with nothing equipped.*

| # | Input | Expected visible result | Result |
|---|---|---|---|
| H-54 | `Z` | the **member picker**, with the roster highlight — `select_player` runs first | PASS (Batch 20, hardware, `a357e28c…`) |
| H-55 | move to member 2, Enter | the panel title becomes **that member's name**; body is their **Stats** page: `M`/`F`, `Lv-N`, class; the centred health word; `Str=`/`HP:`, `Int=`/`HM:`, `Dex=`/`Ex:`, `Magic:` | PASS (Batch 20, hardware, `a357e28c…`) |
| H-56 | `1`, `2`, `3` | each shows **its own member's distinct numbers**. `HM` is max HP and `Ex` is experience | PASS (Batch 20, hardware, `a357e28c…`) |
| H-57 | page right once | `Arms` page, equipped items **by real name**. The unequipped member reads `(None ready)`, not six blank rows | PASS (Batch 20, hardware, `a357e28c…`) |
| H-58 | read the Arms page of the fully-equipped member | every line a genuine item name. **Nothing may read `Equipment 12` or `Item 30`.** A helm named as a shield means the Batch 14 off-by-one is back | PASS (Batch 20, hardware, `a357e28c…`) |
| H-59 | page right past the last member's Arms; also press `0` from any page | the **`Equipment`** page: `Food:`, `Gold:`, then `Keys.......`, `Gems.......`, `Torches....`. A `Grapple` line only when one is carried | PASS (Batch 20, hardware, `a357e28c…`) |
| H-60 | page right four more times | **Reagents**, **Spells**, **Items**, **Armaments**, in that order. Each row is a 2-digit count, `-`, then the name | PASS (Batch 20, hardware, `a357e28c…`) |
| H-61 | inspect the lists | a reagent owned **none** of is **absent**; an empty list reads `(None owned!)` | PASS (Batch 20, hardware, `a357e28c…`) |
| H-62 | the **Items** page | carried scrolls/potions appear **with** counts; regalia/Shard/Spyglass/Sextant/Black Badge/Wooden Box appear **without** a number | PASS (Batch 20, hardware, `a357e28c…`) |
| H-63 | **Armaments** with >7 items: press down, then left/right | the detail line reads `1-7 of N` with `v`; down scrolls by one and the marker becomes `^ v`; **left/right still change page, not scroll** | **FAIL — device-only rendering defect (Batch 20, hardware, `a357e28c…`).** Line reads `1-7 of N ?` instead of `1-7 of N ^ v`; deterministic, every attempt, survives reopening. Root cause (code inspection, not yet fixed): `native/targets/tdeck/main/tdeck_board.cpp`'s bitmap font switch has a `case 'v':` glyph but no `case '^':`; the caret falls to `default:`, which intentionally renders the same bitmap as `'?'`. Cosmetic only, does not block other rows — deferred to Batch 21 per Phase 20G (doesn't meet the "blocks large part of remaining validation" bar for an in-batch fix) |
| H-64 | page right from Armaments; page left from member 1's Stats | wraps to member 1's Stats / to Armaments. **Never a blank page** between the last member and Provisions | PASS (Batch 20, hardware, `a357e28c…`) |
| H-65 | page around the ring twice, then press `i`, `p`, `k`, `e`, `s` | the modal stays open; **nothing happens** — no torch lit, no gem spent, no prompt armed behind the panel | PASS (Batch 20, hardware, `a357e28c…`) |
| H-66 | watch the clock and viewport during H-65 | the avatar does **not** move and the day/time does **not** advance | PASS (Batch 20, hardware, `a357e28c…`) |
| H-67 | press **Space**; then repeat and press **Mic** | the sheet closes and the world HUD returns; the **very next** trackball nudge moves the avatar and advances one turn | PASS (Batch 20, hardware, `a357e28c…`) |
| H-68 | press `Z` again | it opens the **picker**, not the last page; confirming opens a **Stats** page with no leftover scroll position. Repeat five times — nothing drifts | PASS (Batch 20, hardware, `a357e28c…`) |
| H-69 | Developer → poison or kill a member, then view their Stats | the centred health line reads **`Poisoned`** / **`Dead`**; the rest renders normally | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 8 — Transport

| # | Setup | Input | Expected | Result |
|---|---|---|---|---|
| H-70 | Debug → Transport → Ship | `B`oard, then `Y`ell | **HOIST/FURL, not a word prompt** (R-19); the ship sails with the wind | PASS (Batch 20, hardware, `a357e28c…`) |
| H-71 | Aboard | `X`-it; board a horse, a carpet, a skiff | the avatar sprite changes each time | PASS (Batch 20, hardware, `a357e28c…`) |
| H-72 | Aboard a ship | save, reload | transport mode and hull survive | PASS (Batch 20, hardware, `a357e28c…`) |
| H-146 | **New in Batch 20 — no prior row covered this.** Shipwright or Horse Seller, gold ≥ price | `Y` to confirm the purchase | The ship/skiff/horse is granted, gold spent, transport placed at the dock/stable | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** `N` (decline) works; `Y` (confirm) does **nothing** — no message, no gold spent, no transport granted. Root cause (proven): `shop_orchestration.cpp`'s `ShipDeal` confirm handler (line 467-470) and `horse()` handler (line 285-288) both require the `v.ship`/`v.horse` and `v.reserve` callback hooks; `alpha_runtime.cpp:220-229` wires `shop_services_.record_present/record/tile/occupied/plate/hour_tiles/wake_npcs` but never assigns `.ship`, `.horse`, or `.reserve`, so they're null on this build and the confirm silently hits `CommandStatus::Unsupported`. Device-only adapter wiring gap; player-facing effect is that no shop can sell a ship, skiff, or horse. Workaround for the rest of this session: Debug → Presets → "Transport Test Setup" bypasses the shop. Deferred to Batch 21 (doesn't block remaining validation given the workaround) **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-D).** |
| H-147 | **New in Batch 20 — expected non-defect, recorded to save future testers the investigation.** Aboard a ship, sails down | `H` (Hole up), enter any hours value, Enter | Hull increases by a random 1-3 (capped at 99); the clock advances by a **fixed 25 minutes regardless of the hours entered** — the "Hours (1-9)?" prompt (`ui_session.cpp:794`) is asked unconditionally for `H` in every context and this ship-repair path (`rest.cpp:232-240`) simply ignores the value. **Confirmed matches code exactly on hardware — not a bug.** Do not mistake this for a way to skip to daytime; it never was one |

---

## Group 9 — The dungeon (the big one)

*Setup for all: Debug → Teleport → Dungeon, and a **lit** torch unless the row says otherwise.*

### 9a — Presentation and authored art (R-05, Batches 9/9C)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-73 | Deceit floor 0, standard entry | `DUNGEON_SESSION_STATE active=1` **and** a usable view. **Photograph it.** Textured masonry with baked floor speckle and ceiling — **not** flat wedges, **not** outlines. Wireframe geometry means a stale card, not a code fault | PASS (Batch 20, hardware, `a357e28c…`) |
| H-74 | Torch **out**, no light spell | the viewport is **entirely black**. Ignite: the corridor appears **on that keypress** | PASS (Batch 20, hardware, `a357e28c…`) |
| H-75 | Read the strips | above the viewport: **`L1`**, not blank sky. Below: **`Dir:` + the facing**, not `Wind: --` | PASS (Batch 20, hardware, `a357e28c…`) |
| H-76 | Turn left then right (trackball, or `A`/`D` with Movement Mode **ON**) | the lower band changes on each turn | PASS (Batch 20, hardware, `a357e28c…`) |
| H-77 | Stand so a door or room entrance is **two cells ahead** down an open corridor | the corridor **stops** at it and shows a dead end with a door panel. Then stand **on** the door: the two nearest side slices drop away | PASS (Batch 20, hardware, `a357e28c…`) — confirmed via H-81 exercising the same scenario |
| H-78 | Stand so a ladder or chest is **one or two cells ahead** | it is **visible from there**, not only underfoot | INCONCLUSIVE — interrupted by a soft-lock (see H-149) before the visibility-at-distance check itself was confirmed. Needs retest |
| H-79 | Let a wandering monster approach | drawn from **three cells out**; a ceiling-lurking type appears **high in the frame** | PASS (Batch 20, hardware, `a357e28c…`) |
| H-80 | Look at the depth rings | the four rings **abut** with no seams or black gaps (left edge 16→96, right 96→176). The only black in a lit view is the vanishing point | PASS (Batch 20, hardware) |
| H-81 | Repeat H-77 looking at the art | a **dead end** and a **door** are visibly different images; the front wall reads as one symmetric piece with no overlap or gap at the centre column | PASS (Batch 20, hardware, `a357e28c…`) |
| H-82 | Find a **passage**, an **alcove** and a **side door** on side walls | all three visually distinct from plain masonry and from each other, each on the **correct side** — walk past and confirm | PASS (Batch 20, hardware, `a357e28c…`) |
| H-83 | Visit dungeons of different variants (1/4/5 → DNG3 grey; 6/7 → DNG2 red; rest → DNG1 olive) | the wall appearance changes, **on entry**, not after a step | PASS (Batch 20, hardware, `a357e28c…`) |
| H-84 | Repeat H-79 against the art | a recognisable sprite with clean edges — **no** opaque rectangle around it, **no** holes punched through its dark interior | UNTESTED |
| H-149 | **New in Batch 20 — CRITICAL, confirmed reproducible 3× across 2 dungeons.** Deceit floor 8 (displayed) = internal floor index 7 | Opened and looted a chest, then attempted to move | **CONFIRMED on hardware (Batch 20, `a357e28c…`):** all four cardinal directions reported `Blocked!` from the same cell; turning was fully responsive, recovered only via Debug → Teleport. See H-150/H-151 for two further reproductions with a strong shared root-cause lead **SUPERSEDED by Batch 21A (`9e1dd6e6`+): ADJUDICATED ORIGINAL BEHAVIOUR - HARDWARE RETEST REQUIRED.** Not a defect. Deceit's only chest cell in all eight floors is floor index 7 (5,5), cell `0x41`, whose four cardinal neighbours in DUNGEON.DAT are wall/wall/wall plus the **unrevealed secret door** at (5,4) - and Deceit floor 6 (5,5) is a **pit trap** (`0x69`), which is how a party lands there without having revealed that door. `Blocked!` in all four directions with turning still responsive is the authored 1988 outcome; the way out is **(S)earch**, not movement. Pinned end-to-end on the production path by `batch21a_dungeon_room_regression` RED-8 (R8-1..R8-4), which asserts BOTH the four-way block and that Search reveals the door and the party walks out. Retest per audit Phase 6M step 7. (This also resolves H-78's INCONCLUSIVE: it was interrupted by an authored alcove, not a bug.) |
| H-150 | **New in Batch 20 — CRITICAL.** Wrong dungeon, walked through a door into a room | Attempted further movement/commands | **CONFIRMED on hardware (Batch 20, `a357e28c…`).** Transcript: `...Advance, Entering room..., Blocked! x6`. Screenshot shows a top-down arena-style view (purple/green field, no enemies) with only the active character (Shamino) able to act — **the turn never passes to the other two party members.** Party fully healthy (240/240 HP, all status `G`) — not a combat-loss artifact **SUPERSEDED by Batch 21A (`9e1dd6e6`+): ADJUDICATED ORIGINAL BEHAVIOUR - HARDWARE RETEST REQUIRED.** Not a defect. The one-character turn loop is **Set Active Player** - COMBAT:0x063E @0666-067f auto-passes every player whose turn comes up while `g_active_char` names a different, still-living member (`call 0xda86; ret`), cloned in `Engine::current()` and in the reference port's `skipsForActiveChar()`. Leaving combat restores per-member commands, exactly as observed. `Blocked!` inside an arena comes from `Engine::move()`, not the dungeon: a chosen character hemmed in by their own party and a wall reports it in every direction they try. Reproduced on the production path by `batch21a_dungeon_room_regression` RED-6: with `active_character == 255` all three healthy members are scheduled; after the dungeon digit key chooses member 2, exactly one is, and the arena stays live. Mutation M5 proves the guard has teeth (`combat_parity` + R6-3). Retest per audit Phase 6M steps 1-3. |
| H-151 | **New in Batch 20 — CRITICAL, total unrecoverable freeze.** Destard, after retreating from a lost fight (`BATTLE IS LOST!`), continued moving and entered another room | Attempted further movement/commands | **CONFIRMED on hardware (Batch 20, `a357e28c…`): total freeze.** Nothing responds — not movement, not `Alt+M`, not Mic. Screenshot shows a fully-rendered combat arena (torches, chests, field tiles, 2 enemy sprites) frozen mid-frame, transcript's last line is `Entering room...`. **Requires a physical power cycle to recover — no software escape path exists.** **SUPERSEDED by Batch 21A (`9e1dd6e6`+): SOFTWARE FIXED - HARDWARE RETEST REQUIRED.** This was the family's one real defect, and it was **not** the room-number collision. `combat_growth_reserve()` returned `max(count,63)+1` = 64 whenever any actor in the arena was an enemy with ability bit `0x1000` (divide-on-hit), while `AlphaRuntime` owns `kCombatActors`(22) + 32 overflow = **54** slots - so `capacity - count < reserve` was permanently true and **every** combat command, the player's and the runtime's own `CombatEnemyStep` beat, returned `NeedsActorStorage` and did nothing. `Engine::current()` then kept naming the same enemy, `combat_ai_turn()` stayed true, and `handle()` pushed every keypress into `combat_input_queue_` instead of the session: a fully rendered arena answering neither movement, `Alt+M` nor Mic, with `combat_.ended` never set so teardown never ran. Only `Alt+D`/Save/Load (DeviceShortcuts, handled above the queue) would still have answered. Two shipped definitions carry the bit - def 24 **Slime** (`0x1100`) and def 30 **Gargoyle** (`0x9000`) - and seven authored room boards place them: Deceit r0/r7, **Despise/Destard r9**, Shame r2/r12/r14, Hythloth r13, plus any encounter rolling either. Fixed in `combat.cpp` (`combat_growth_reserve()` now states real per-action growth) and `combat_magic.inc` (`Engine::spawn()` refuses past capacity, matching the original's fixed 32-slot table at DS:0xBA14). RED-to-GREEN in `batch21a_dungeon_room_regression` RED-7 (R7-3/4/5); mutation M1 restores the defect and they go RED again. Retest per audit Phase 6M steps 4-6. |
| H-152 | **New in Batch 20 — related turn-order defect, non-fatal.** A working (non-frozen) dungeon room combat encounter | Take actions across multiple turns | **CONFIRMED on hardware (Batch 20, `a357e28c…`):** only the first character to act gets turns for the entire room visit — the other two party members never get a turn while inside the room. On leaving the room, the other two characters' turns suddenly become available. Likely the same actor-cycling defect family as H-150/H-151, observed here without a freeze — valuable supporting evidence that the dungeon-room combat turn queue does not advance correctly. Location of this observation (corridor ambush vs. authored room) not yet confirmed **SUPERSEDED by Batch 21A (`9e1dd6e6`+): ADJUDICATED ORIGINAL BEHAVIOUR - HARDWARE RETEST REQUIRED.** Same adjudication as H-150: Set Active Player's auto-pass (COMBAT:0x063E @0666-067f). It is not an actor-cycling defect and it is not room-specific - it applies to any combat while a member is chosen, which is why leaving the room appeared to restore the other members' turns. `0` returns the party to party mode. Covered by `batch21a_dungeon_room_regression` RED-6 and RED-2 (the `active_character == 255` control, where all three members ARE scheduled). Retest per audit Phase 6M steps 1-2. |
| — | **Root-cause lead for H-149/H-150/H-151 (unconfirmed mechanism, needs adjudication before any fix — but the trigger condition is now confirmed)** | `dungeon.cpp:125-129`'s `room()`: `if ((c>>4)==10 \|\| dungeon_room_cleared(...)) msg("Entering room..."); else event(DungeonEventKind::Room,...)` — the no-event message-only branch | **Confirmed why fresh rooms aren't safe:** `dungeon_mark_room()` (`dungeon.cpp:224-232`) keys the "cleared" bitmap purely by `(dungeon, room-number & 15)` — no floor, no X/Y. Room numbers are a 4-bit field reused across a dungeon's 8 floors by construction, so clearing/losing any one room marks every other room sharing that number — anywhere in the dungeon — as "already cleared," even on a genuine first visit. This is why the tester couldn't tell whether they'd "been there before": they hadn't, but the shared room-number made no difference. Every other cell-entry outcome in `dungeon.cpp` emits a `DungeonEventKind` event to drive a presentation transition; this is the one branch that doesn't, and all three reproductions' transcripts end on that exact message. The mechanism from "no event emitted" to "stuck one-actor turn loop" (Wrong) vs. "total freeze" (Destard) is still untraced. **Top priority for Batch 21 — no safe workaround exists**, since room-number collision means even a genuinely fresh room can trigger this **SUPERSEDED by Batch 21A (`9e1dd6e6`+): the lead's FACT is right and its CONCLUSION is wrong.** The 4-bit room-number collision is **ORIGINAL** - `cleared_bit()` is a byte-exact clone of DNGLOOK 0x0844 @0x088c-0x08c9 / 0x08d4 (14 bytes = 7 dungeons x 16 rooms, keyed by dungeon + a 4-bit room number only, collapsing even Deceit-equals-Despise), mirrored in `game/src/core/dungeon/dungeon.ts` `dungeonClearedBitIndex()`, with the six-room `ROOM_CLEAR_EXEMPT` table from DATA.OVL 0x384a cloned too. Entering an already-cleared room printing "Entering room..." and placing no monsters is likewise original (DUNGEON 0x0000:0x0008 unconditional; COMBAT 0xB94 places nothing). **None of it was changed**, and `batch21a_dungeon_room_regression` RED-1/RED-3 now pin it (mutation M6 makes `dungeon_parity`, `dungeon_combat_regression` and R3-2/3/4 all fail). The message-only branch is not what froze the device: see H-151 for the real cause (`combat_growth_reserve()`). |

### 9b — Dungeon runtime and input (Batch 9B)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-85 | Deceit, torch lit | turn left, turn right, advance, back up **with the trackball**: facing changes on each turn, the cell changes on each move. Then Enter = **Turn Around**: the `Dir:` band flips 180°, it does **not** pass the turn | **PASS (Batch 9B)** |
| H-86 | Torch burnt out or entered without one | the viewport is black; press `I` — **a torch lights and the corridor appears on that keypress**. Before 9B this answered `"What?"` | **PASS (Batch 9B)** |
| H-87 | Still dark | turn left twice, advance once: the viewport stays black but the `Dir:` band still changes. Relight with `I`: the corridor shown is the one those commands moved you to | PASS (Batch 20, hardware, `a357e28c…`) |
| H-88 | A cell with a ladder **up and down** | `K` shows `Klimb-U/D-`; answering down goes one level **deeper**. On a one-way ladder `K` resolves with **no prompt** | **PASS (Batch 9B)** |
| H-89 | Any corridor cell | `S` shows `Dir-`; answering **down** (Here) searches the cell **underfoot**. `H` opens camp hours; `D` drinks or answers `"No fountain here."`; a digit sets the active player | PASS (Batch 20, hardware, `a357e28c…`) |
| H-90 | Standing in Deceit, reached **from another named map** (Serpent's Hold is the reported case) | the right-hand location strip reads **`Deceit`**; leaving returns it to the surface name. Check one more dungeon | PASS (Batch 20, hardware, `a357e28c…`) |

### 9c — Dungeon fields and rooms (Batches 12/12B)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-91 | Destard, a corridor cell whose **forward** cell is plain empty floor, level-4+ caster with mana | cast `In Flam Grav`: the log reads `Cast` (not `Failed!`) **and** a field is visible ahead — dense short horizontal **bright green** sparkle strokes. It shimmers on every redraw; that is the original's per-redraw re-roll | PASS (Batch 20, hardware, `a357e28c…`) |
| H-92 | After H-91 | **only** the cell directly ahead is affected — not the sides, not the cell beyond | PASS (Batch 20, hardware, `a357e28c…`) |
| H-93 | After H-91 | turn away and back: the field is still there. Pass several turns: unchanged (there is no field duration) | PASS (Batch 20, hardware, `a357e28c…`) |
| H-94 | Repeat H-91 with `In Nox Grav`, `In Zu Grav`, `In Sanct Grav` | each seeds its own one-cell field, each visible. `In Zu Grav`/`In Flam Grav` draw **EGA 10 bright green**; `In Nox Grav`/`In Sanct Grav` **EGA 9 bright blue**. **Only two colours across four spells is correct** | PASS (Batch 20, hardware, `a357e28c…`) |
| H-95 | Face a wall or an occupied cell | cast `In Flam Grav`: **`Failed!`**, nothing drawn, nothing changed | PASS (Batch 20, hardware, `a357e28c…`) |
| H-96 | Walk **into** an In Flam Grav field; then into an In Zu Grav field | `Fire!!` and HP loss, field **still there**. `Sleep spell!`, members may sleep, field **disappears** — a sleep field consumes itself, fire does not | PASS (Batch 20, hardware, `a357e28c…`) |
| H-97 | Walk the corridors of **Wrong** (30 authored fields) or **Covetous** (25) | authored magic fields appear at cells you never cast into | PASS (Batch 20, hardware, `a357e28c…`) |
| H-98 | Enter **Destard**, step into a **room** cell (high nibble `F` or `A`) | the board you fight on is a **Destard** board, not Wrong's of the same number | UNTESTED |
| H-99 | Enter **Doom**, step into a room cell | a fight **starts**. A command-failed/invalid-context line instead means the arena lookup regressed — capture the log | UNTESTED |
| H-100 | Any dungeon room | an enemy standing on a **black** cell, possibly outside the wall boundary, **is correct** — 50 of the 128 shipped `.CBT` boards author slots on `BlackSquare`. **Negative gate: do not file it** | UNTESTED |
| H-101 | Deceit room 4 (poison `0xE8`), Deceit room 2 (energy `0xEB`), Doom room 9 (poison) | field tiles visible from the first frame; a member standing on poison and passing the turn is poisoned; energy tiles **block** and never damage; `An Grav` on a field cell = `Success!`, on an empty cell = `Failed!` | UNTESTED |

### 9d — Combat field magic in the arena (Batch 12) — *negative gate*

> In combat, In \*Grav builds **no** wall. `CAST.OVL 0x004c`'s combat branch throws a spell weapon through the ordinary attack dispatcher and seeds no tile. This is the original's behaviour, cloned on purpose (`docs/bugs-del-original.md` §2.9). **Do not file the absence as a defect.**

| # | Setup | Expected | Result |
|---|---|---|---|
| H-102 | Any fight, level-4+ caster with ≥4 MP | cast `In Flam Grav`, aim at an **enemy**: the reticle opens on the caster's cell; ceremony flash; an ordinary attack result (hit naming the enemy, up to **21** damage, or a miss). **No fire tile anywhere.** Aim at a clear square: flash, nothing else, spell and mana spent, turn passed. **That is a PASS** | UNTESTED |
| H-103 | After H-102 | advance 2–3 turns: still no field, and none arrives late | UNTESTED |
| H-104 | Same gesture with `In Nox Grav` | identical shape, damage up to **18**, no poison tile | UNTESTED |
| H-105 | Same gesture with `In Zu Grav` aimed at an enemy | spell spent, mana −4, turn passes, enemy loses **exactly zero** HP. This is ticket #91 and it is a **PASS** | UNTESTED |
| H-106 | Same fight | cast `In Bet Xen` and `Kal Xen Corp` | both still summon as before — up to four swarm actors from one cast, one daemon that may turn on you | UNTESTED |

### 9e — Dungeon movement, transitions and the magic ceremony

| # | Setup | Expected | Result |
|---|---|---|---|
| H-107 | Movement Mode ON | `W` advances, `S` backs up, `A`/`D` turn. Walk into a wall: the blocked response | PASS (Batch 20, hardware, `a357e28c…`) |
| H-108 | Find stairs | `K`limb down: the depth readout changes and the top band reads **`L2` on the descent itself** | PASS (Batch 20, hardware, `a357e28c…`) |
| H-109 | Walk into a pit; walk into a field | the damage message and the feature art | PASS (Batch 20, hardware, `a357e28c…`) |
| H-110 | Corridor | `S`earch, `O`pen, `G`et, `J`immy each report their own result | PASS (Batch 20, hardware, `a357e28c…`) |
| H-111 | Corridor | cast `Uus Por` / `Des Por`; then `R`eady — **it applies and charges no turn** (R-06) | PASS (Batch 20, hardware, `a357e28c…`) |
| H-112 | Corridor | cast an ordinary spell (e.g. `In Lor`): **the same flash/inversion the overworld gives**. Then cast `Grav Por` or `Vas Flam` as the negative control — weapon-spells stay silent, above ground and below | PASS (Batch 20, hardware, `a357e28c…`) |
| H-113 | Trigger a dungeon encounter and win | **the return is to the same cell and facing** (Y-22) | PASS (Batch 20, hardware, `a357e28c…`) — tested via a corridor ambush, not a room encounter |
| H-114 | In a dungeon | `Alt+M` → close; `Alt+D` → Back | **the dungeon view returns both times** | PASS (Batch 20, hardware, `a357e28c…`) |
| H-115 | In a dungeon | save, reload | position, facing, revealed cells, wanderer **and any field you cast** resume exactly (R-15; fields ride in `DungeonState::cells`) | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Save reports success; reloading (same session, no power-cycle) resumes an older/prior save instead. Reproducible specifically in dungeons — never observed in the overworld. Root cause (proven architectural gap, exact trigger for this symptom not yet confirmed): `alpha_save.cpp`'s `candidate()` (used as the save's own "semantic-validation" self-check, line ~178) and `restore_candidate()` (the core of `load()`) both only call `restore_gameplay`/`restore_terrain`/`restore_npc_walk` — **neither ever calls `restore_dungeon`**. Dungeon state is restored as a separate step afterward (`alpha_runtime.cpp:2166`), and its failure just logs `DUNGEON_RESTORE_FAILED` and silently resets `dungeon_={}` with no player-visible indication. This means both the save-time and load-time integrity checks are blind to dungeon-payload corruption — a save/load can be reported fully successful while the dungeon-specific data is invalid. Needs live log confirmation (`DUNGEON_RESTORE_FAILED` at the moment of reload) to nail the exact mechanism, but the architectural gap itself is confirmed by code inspection. High priority for Batch 21 alongside H-149/H-150/H-151 **Batch 26: HOST FIXED / DEVICE RETEST PENDING (audit Phase 6U).** The cause was not the save self-check: the sidecar exporter never wrote the `"dungeon"` object (`persistence.cpp` `extras[]`), so every dungeon save loaded at the entrance on the surface. Saving underground is allowed in 1988 (`CAST2.OVL:0x10FE`, no location gate) and resumes in place. The Rel Tym toggle is 0 after a load (`DUNGEON 0x0E40`, not saved). Use the System Menu load (`Alt+L` is H-164). See `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 26" **Batch 52: HARDWARE PASS — Phase 6U.** |
| H-116 | Descend past floor 7 | the Underworld transition | PASS (Batch 20, hardware, `a357e28c…`) |
| H-117 | Walk out at the level-1 entrance | the surface world view and Exploration verbs return | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 10 — Quest, Blackthorn and persistence

| # | Setup | Input | Expected | Result |
|---|---|---|---|---|
| H-118 | Debug → Quest → grant a shard | `U`se it in a Flame room | the shard's effect fires (R-08) | **FAIL — CRITICAL, confirmed production defect (Batch 20, hardware, `a357e28c…`).** Used Debug → Certification → "Flame/Shard Test" (grants all 3 shards, teleports to the verified Empath Abbey (15,3) floor-1 ritual cell), then `U`sed a shard. Only the generic "Use item" echo appeared — no ritual text at all, even though `cast_shard_into_flame()` (`quest.cpp:89-107`) is unconditional: it always writes a header line first regardless of position match, so at minimum 1-2 lines of flavor text should always print. **Afterward, movement became permanently silent — no `Blocked!`, no echo, nothing — and did NOT recover after teleporting elsewhere or loading a save.** `Alt+M`/`Alt+D` (System Menu/Developer) still respond normally, ruling out a total device hang; teleport and load commands are themselves still processed (the destination/reload happens) but movement remains dead afterward regardless. Since Load only restores `GameState`/`TurnState`/quest data (confirmed via `alpha_save.cpp` review, see H-115) and never touches UI session mode, and this survives both teleport and load, the stuck state most likely lives in UI-session/input-mode state (e.g. a leftover `TargetSelection`-style mode silently swallowing movement input) rather than corrupted save data. **Narrowed further:** `L`ook and `Z`-stats both work normally (Look correctly resolves different directions per trackball/WASD input, confirming directional input hardware is fine) — only the world **Move** command path specifically is affected, producing no response at all (not even `Blocked!`). This rules out a broad UI/input freeze; it's a targeted lock on one command kind. Recovered via power cycle (teleport/reload were insufified). Root cause not yet isolated — needs dedicated investigation. **Top priority for Batch 21 alongside H-149/H-150/H-151/H-115** **Batch 25: HOST FIXED / DEVICE RETEST PENDING (audit Phase 6T).** Root cause was not the ritual: a question the core was waiting on (e.g. "Leave this place?") was on screen when `Alt+D` was pressed; `UiSession::set_base_mode()` overwrote the Developer menu's return register, so closing the menu dropped the question while `awaiting_exit` stayed set and every world command (Use, Move, Look's core half) was refused silently. See `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 25" **Batch 52: HARDWARE PASS — Phase 6T.** |
| H-119 | At a dungeon entrance | `Y`ell a word of power | the quake and the flag toggle | UNTESTED **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E-B).** |
| H-120 | Blackthorn's palace, capture sequence | run the whole ceremony | the interrogation asks its questions, each with a visible **`Your response?`** row (see H-23), and the scene reads as a distinct scene, not the ordinary Palace lobby (R-32) | UNTESTED |
| H-121 | During H-120, a companion is executed | observe the active character | **the previously-set active character is NOT re-pointed** — if it named the executed companion or anyone after them in the marching order, it now names someone else. That is `BLCKTHRN 0x03ae-0x04d4`'s own behaviour. **Do not file it** | UNTESTED |
| H-122 | During H-120 | watch the pacing | **RE-ADJUDICATED — Batch 51** (see Phase 7D-B below): the tick pacing is byte-exact; the materialization sweep, the holy-circle frame and the sacrifice siren had **zero dwell** and are host fixed; the "missing teleport-in" was the circle frame being staged and replaced inside one pump, so it never reached the screen (its LFSR texture is still not modelled). *Original row text:* **KNOWN OPEN — D-10/Y-32.** The scene may feel faster and more collapsed than the original. **If you can record video against original footage, that recording is the missing evidence.** Capture it rather than filing a ticket | Consistent with the known-open D-10/Y-32 pacing issue (fast, as expected — not filed separately). **Additional new observation (Batch 20):** the animation for Blackthorn teleporting in appears to be missing entirely, distinct from the general pacing issue. Recorded for Batch 21 follow-up, not yet root-caused |
| H-123 | Falsehood / Abbey chain | run it | the chain completes and its quest objects register | UNTESTED |
| H-124 | Overworld, town, dungeon, aboard a ship, mid-quest | save and reload in each | after each: position, party, inventory, equipment, time, transport, world objects, hidden/search objects, and the UI mode and renderer all match | PASS for overworld/town/ship contexts (Batch 20, hardware, `a357e28c…`). Dungeon context not independently retested — presumed to share H-115's confirmed defect given the identical save/reload mechanism **Batch 26:** the dungeon context shared H-115's defect and is fixed with it; world objects were already persisted (R-14) and are now pinned by `batch26_dungeon_save` L1/L2. Dungeon context: **DEVICE RETEST PENDING (audit Phase 6U)** **Batch 52: dungeon context HARDWARE PASS — Phase 6U.** |

---

## Group 11 — reachability and feedback spot-checks

*Added in Batch 18 after the reconciliation found resolved findings with no device row of their own. Short rows, runnable alongside the groups above.*

| # | Setup | Input | Expected | Audit ID | Result |
|---|---|---|---|---|---|
| H-125 | At the harpsichord (Lord British's castle / a tavern) | press digits `0`–`9` | each plays its note; the digits are intercepted **before** Set Active Player at the instrument, and only there | R-20 | PASS mechanically (Batch 20, hardware, `a357e28c…`) — digit interception confirmed correct (does not fall through to Set Active Player at the instrument). Actual note **audio** not verifiable — audio is not yet wired into this build, a known gap, not a regression |
| H-126 | World, party ≥2, **away** from a harpsichord | press `1`…`6` | `Set Active Plr:` and the named member becomes active. A digit beyond the party size is bounded, not honoured | Y-20 | PASS (Batch 20, hardware, `a357e28c…`) |
| H-127 | Inside a selection or target modal | `Alt+M` → close | the System Menu opens over the modal and, on close, the **same modal is still open and still answerable** | Y-27 | PASS (Batch 20, hardware) |
| H-128 | Town, on a bed | walk onto it | the auto-sleep turn fires from the town turn handler — no player-visible command, no `What?` | Y-26 | UNTESTED |
| H-129 | From the frontend menu | item 7, Developer | gameplay opens on whatever `INIT.GAM` state booted, and the Developer screen is usable | Y-18 | UNTESTED |
| H-130 | Long transcript (fight or a long conversation) | `Shift+Up` several times, then let new text arrive | the view **stays** where you scrolled it; it does **not** yank to newest. `Shift+Down` to the bottom re-enables auto-follow | R-31 (enhancement E-1) | PASS (Batch 20, hardware) — works, tester notes it may benefit from UX tweaks in the future (not a functional defect) |
| H-131 | Debug → preset **Combat** | apply it | it does **not** silently engage Set Active Player, and other party members are **not** auto-passed | R-24 | PASS (Batch 20, hardware, `a357e28c…`) — verified during Leg B's "Combat Test Setup" preset application |
| H-132 | Debug → Default Entrance at a location with a basement (e.g. Lord British's castle) | enter | you arrive on the **ground floor**, not the basement — the ordinal-vs-signed-z fix | R-27 | PASS (Batch 20, hardware, `a357e28c…`) |
| H-133 | Debug menu, all pages | browse | every setter shows a **label**, not a raw ordinal, and the current value is readable without moving the cursor onto it. Special Items and Quest Items are reachable and named | R-28, R-29 | PASS (Batch 20, hardware) |
| H-134 | Debug → any preset | select it | the effect list is **disclosed** before applying and a confirm gate is required. The Transport preset does **not** silently rig HMS Cape | R-30 | PASS (Batch 20, hardware, `a357e28c…`) — verified during Leg B's "Combat Test Setup" preset application (effect list disclosed, confirm gate present) |
| H-135 | Trigger an earthquake event (Yell a word of power at a dungeon entrance, H-119) | observe | the **Quake** shake renders | Y-04 | UNTESTED **Batch 53: reachable again (RB-2 fixed); observed in Phase 7E-B.** |
| H-136 | Combat: land a killing area-effect blow, or step a poisoned member through a turn | observe | **CellExplosion** draws on the cell; **PoisonTick** flashes the poisoned member's roster row by inversion, not by a tile write | Y-04 | UNTESTED |
| H-137 | Enter a Refuge; separately, let a troll ambush you at a bridge | observe | the **Refuge** and **TrollSneak** narrative scenes both pace out as sequences with readable text, not as a single flashed frame | Y-04 | PASS for TrollSneak (Batch 20, hardware) — confirmed via a bridge ambush. Refuge half not yet tested **Batch 53: Refuge half host-proven on the device runtime; Phase 7E-E.** |
| H-138 | Any combat where a summoned or field effect kills the last enemy while a member is poisoned | observe | the sprite on the cell stays put for the whole choreography (`under_tile`), and no committed state change is deferred past it | Y-04 (#243) | UNTESTED |
| H-139 | Combat, a member with **nothing** in hand | `R`eady | **KNOWN OPEN — D-8/Y-28.** Today a disabled `(None available)` picker opens instead of the action being charged immediately. Record what you see; do not file | Y-28 | UNTESTED |
| H-140 | Combat, `R`eady a ring that vanishes on use | observe | **KNOWN OPEN — D-8/Y-28.** `Ring vanishes!` does not close the picker early. Record; do not file | Y-28 | UNTESTED |

---

## Result tally (Batch 20 close-out)

Original 145 rows, plus 7 new rows discovered/added during Batch 20 (H-146–H-152) = **152 total rows**.

| Group | Rows | PASS | FAIL | BLOCKED | INCONCLUSIVE | UNTESTED |
|---|---|---|---|---|---|---|
| 1 — boot / input | 9 | 9 | 0 | 0 | 0 | 0 |
| 2 — Y-series closeout | 7 | 4 | 3 | 0 | 0 | 0 |
| 3 — wishing well | 7 | 6 | 1 | 0 | 0 | 0 |
| 4 — overworld / town (+ H-148) | 9 | 7 | 0 | 1 | 1 | 0 |
| 5 — combat / loot | 9 | 9 | 0 | 0 | 0 | 0 |
| 6 — items / magic / views | 18 | 16 | 1 | 0 | 1 | 0 |
| 7 — Z-stats | 16 | 16 | 0 | 0 | 0 | 0 |
| 8 — transport (+ H-146, H-147) | 5 | 4 | 1 | 0 | 0 | 0 |
| 9 — dungeon (+ H-149–H-152) | 49 | 33 | 5 | 0 | 1 | 10 |
| 10 — quest / persistence | 7 | 4 | 1 | 0 | 0 | 2 (H-119, H-123) |
| 11 — reachability / feedback spot-checks | 16 | 9 | 0 | 0 | 0 | 7 |
| **Total** | **152** | **117** | **12** | **1** | **3** | **19** |

**Rows still UNTESTED (19):** H-84; H-98–H-106 (9 rows, deliberately deferred — freeze-family defect, no safe workaround exists); H-119, H-123 (directions given, not yet run); H-128, H-129, H-135, H-136, H-138, H-139, H-140 (Group 11 spot-checks not reached this session).

**Rows BLOCKED (1):** H-30 (checklist itself had the wrong location; corrected, not yet retested at the real location).

**Rows INCONCLUSIVE (3):** H-51 (likely test-location confound, needs retest with blocked sightlines), H-78 (interrupted by the H-149 soft-lock before confirmed), H-148 (real community-reported original-game behavior, not yet adjudicated against the 1988 disassembly).

**Alpha 2 cannot be declared validated until this table has no UNTESTED/BLOCKED/INCONCLUSIVE rows and every FAIL is resolved.**

---

## Batch 21A addendum (`9e1dd6e6`+, firmware `0xd20c0`) — the dungeon room-entry family

The Batch 20 tally above is left exactly as it was recorded; this addendum supersedes only the four rows named in it. **Nothing here is a PASS.** Hardware status changes only when the user flashes the Batch 21A firmware and physically retests (audit **Phase 6M**).

| Row | Batch 20 status | Batch 21A disposition |
|---|---|---|
| H-149 | FAIL (CRITICAL) | **ADJUDICATED ORIGINAL — hardware retest required.** Authored pit-fed chest alcove; the exit is (S)earch. No production change. |
| H-150 | FAIL (CRITICAL) | **ADJUDICATED ORIGINAL — hardware retest required.** Set Active Player auto-pass (COMBAT:0x063E). No production change. |
| H-151 | FAIL (CRITICAL) | **SOFTWARE FIXED — hardware retest required.** `combat_growth_reserve()` demanded 64 free actor slots out of 54 whenever a Slime or Gargoyle was in the arena. |
| H-152 | FAIL | **ADJUDICATED ORIGINAL — hardware retest required.** Same as H-150. |

**Consequence for the deferred rows:** H-98–H-106 (the nine room-combat rows Batch 20 refused to run because the freeze had no safe workaround) are now believed safe, but only **after** Phase 6M passes. Do not resume the rest of the 152-row checklist before that micro-gate is green.

**Still open and untouched this batch:** H-118 (shard ritual / permanent movement lock, CRITICAL), H-115 (dungeon save/load), H-12/H-13, H-22, H-45, H-63, H-146, H-122, H-148.

**Host suite:** 89/89 from a clean build (88 before, +1 for the new `batch21a_dungeon_room_regression`). **Firmware:** `openu5_tdeck.bin` `0xd20c0` (860,352 bytes), `0x2df40` (188,224 / 18%) free, zero project warnings. **SD resource pack unchanged; no SD recopy required.**

---

## Batch 21A.1 addendum (`236cfa34`+, firmware unchanged at `0xd20c0`) — the Deceit L1 → Klimb Down → L8 report

Raised during the Phase 6M micro-retest, **before it could complete**. The Batch 20 tally and the Batch 21A addendum above are left exactly as recorded; this addendum adds one row and supersedes nothing.

**Verdict: NOT A DEFECT — no production code changed.** The full derivation is in `GAMEPLAY_INTEGRATION_AUDIT.md` § *Batch 21A.1*.

### The observation, preserved verbatim

1. Teleported/entered **Deceit** · 2. Was on displayed **L1** · 3. The only apparent route was a ladder · 4. Used `K` / Klimb **Down** · 5. This immediately entered a top-down dungeon room combat board with **Water Serpent** enemies · 6. User fled / lost the room encounter · 7. On return to the dungeon 3D view, the HUD now showed **Deceit L8** · 8. The corridor had many doors and clearly was not the expected immediate lower level from L1 · 9. User then used Developer teleport to go back to the beginning of Deceit · 10. Teleport succeeded, but the dungeon now looked different from how it looked at the beginning of the test.

Photographs: the arena after `Klimb- Down!` / `Entering room...`, and the returned 3D view showing `Deceit`, `L8`, `BATTLE IS LOST!`, `Back up`, `Blocked!`.

| Row | Status | Disposition |
|---|---|---|
| H-153 | **New in Batch 21A.1** — Deceit, displayed L1, `K`/Klimb Down → immediate Sea Serpent room → fled → 3D view returned reading **L8** on an unfamiliar door-heavy corridor; a later developer teleport back to the Deceit entrance looked different from the start of the session | **ADJUDICATED ORIGINAL — hardware retest required.** Deceit floor 0 **(1,3)** is authored `0x60`, a **Trap** cell, and every trap cell is down-klimbable (`caps()` `t == 6`, DUNGEON:0x1E79-0x1E8B; `dungeon.ts` `klimbCaps()`). `(K)`Down steps to floor 1, whose (1,3) is `0x69` — a pit trap — so `enter()` runs the pit chain (DUNGEON:0x0A4C, `pitFall()`), which keeps falling while it lands on another pit. Deceit (1,3) is `0x61` on floors 2–6: **six falls, floor 1 → 7.** The chain stops on floor 7 (1,3) = `0xFA`, **room 10**, and opens its fight. `DUNGEON.CBT` #10 places **four sprite-`0x88`** units; `initialize_combat` resolves `(0x88 - 0x40)/4` = def **18 = Sea Serpent** — the reporter's "Water Serpent". Room 10's board has **no in-arena klimb/grate tile**, so leaving sets `escape_floor_delta = 0` and `dungeon_combat_return()` moves no floor: the party stays on floor 7, and `L8` is the correct rendering of index 7. **There is no wrap:** the stored floor byte is `0x07`, and the command path's own `pos.floor > 7` gate passes. The post-teleport difference is also real and intended — a same-dungeon developer teleport **preserves facing** and does not re-run `dungeon_load`, and the pit chain **permanently** rewrites each pit it consumes (`0x60 \| (cur & 8)`), so a second Klimb Down at (1,3) now stops on floor 1. Pinned end-to-end on the production path by `dungeon_combat_regression` **B21A1-0 … B21A1-6** (7 cases, 45 checks). Retest per audit **Phase 6M.1**. |

### Retest gate

Run audit **Phase 6M.1** (10 short steps) *before* resuming Phase 6M or the rest of the 152-row checklist. **No reflash is required** — the Batch 21A firmware already on the device is the firmware this adjudication was made against.

**Host suite:** 89/89 from a clean build, 0 fail, 0 skipped (`dungeon_combat_regression` grows from 124 to 169 checks). **Firmware:** unchanged from Batch 21A — `openu5_tdeck.bin` `0xd20c0` (860,352 bytes), zero project warnings. **SD resource pack unchanged; no SD recopy required.**

**Still open and untouched this batch:** H-118 (shard ritual / permanent movement lock, CRITICAL), H-115 (dungeon save/load), H-12/H-13, H-22, H-45, H-63, H-146, H-122, H-148.

---

## Batch 21B — H-148 adjudication + Original Behavior Gap Sweep (2026-09-22)

**H-148 is closed as a software fix and needs a hardware retest.** The community report was confirmed clause by clause against the shipped 1988 binaries; the full derivation, the RED/GREEN evidence and the six-mutation proof are in `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 21B". The row above is updated in place.

### Retest gate

Run audit **Phase 6P** (8 short steps in Lord British's Castle basement). **A reflash is required** — the fix is in core logic, so the Batch 21A.3 firmware on the device does not carry it. **The SD resource pack is unchanged; no SD recopy is required.**

### New rows queued by the sweep — *classification only, none of these was fixed*

| Row | Behaviour | Status |
|---|---|---|
| H-154 | Bed hole-up does not snap NPCs to their schedule **on the device**: `alpha_runtime.cpp:232` wires `RestServices::snap_npcs` to an empty lambda. This is the *sibling half* of `TOWN.OVL:0x1694`, the routine H-148 restored — the core now always runs the object half, but the NPC half is host wiring and was deliberately left for its own batch | **CONFIRMED MISSING ORIGINAL BEHAVIOR — queued.** Do not file |
| H-155 | `"Thrown out of bed!"` can never fire on hardware: `RestServices::occupied` is wired to a constant `false`, so the `CMDS.OVL:0x0688` occupancy probe (kernel `0x368E`) always answers no | **CONFIRMED MISSING — queued.** Do not file |
| H-156 | Bed hole-up must run kernel `0x2AE8 kernel_turn_housekeeping` at `CMDS.OVL:0x0671` every ten-minute tick: poison, meals, starvation, turn counter, Q/T expiry, regeneration. The TypeScript reference omits it | **HOST FIXED — Batch 30 / DEVICE RETEST PENDING** (Phase 6X) |
| H-157 | Sleeping across 20:00 or 05:00 must refresh the town's day/night overlay *inside the tick*, before housekeeping/NPC snap, via `TOWN.OVL:0x0170` at `CMDS.OVL:0x0664`. The TypeScript reference omits it | **HOST FIXED — Batch 30 / DEVICE RETEST PENDING** (Phase 6X) |
| H-158 | Changing floors inside a small map does not reload the map record. 1988's `town_use_ladder` (`TOWN.OVL:0x052e`) calls `town_load_town_map(fresh=1)`, which re-reads the 0x400-byte record for the new floor **and** re-seeds the object register. Consequence a tester will see: a skull-key-unmagicked door stays open across a floor change where 1988 relocks it | **CONFIRMED MISSING (both ports) — queued.** Expected during Phase 6P step 8; do not file |
| H-159 | Interior chests are seeded with contents byte `8`; the binary seeds `0x1E` (`TOWN.OVL:0x1795`). Closes oracle hole **O5**. Affects every interior chest's loot at map entry, not just after a reset, so fixing it will move `gameplay_parity`/`quest_parity` and must be done together with the TypeScript side | **CONFIRMED MISSING — queued.** Do not file |
| H-160 | Outdoor camp: the watchman walks through the campfire and through sleeping members, because `RestServices::cell_free` is a constant `true` on the device | **CONFIRMED MISSING — queued, cosmetic.** Do not file |

**Host suite:** 91/91 from a clean build, 0 fail, 0 skipped — the prior 90 plus `batch21b_chest_reset`. No parity corpus moved. **SD resource pack unchanged.**

**Still open and untouched this batch:** H-118 (shard ritual / permanent movement lock, CRITICAL), H-115 (dungeon save/load), H-12/H-13, H-22, H-45, H-63, H-146, H-122, plus the seven rows queued above.

## Batch 24 — state/reload parity (H-161, H-162, H-163)

All three are one 1988 routine, `TOWN.OVL:0x0408` (the floor loader), reached with argument 1 from stairs/ladders and argument 0 from a load and from the end of a town fight. Full derivation, RED/GREEN and the seven-mutation proof: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 24".

| Row | Behaviour | Status |
|---|---|---|
| H-161 | Changing floors repositions every NPC of the location to its current schedule cell (`0x0408(1)` → `0x1694`, `0x1841-0x1856`), on every floor, from the live schedule | **SOFTWARE FIXED — HARDWARE RETEST REQUIRED** (Phase 6S steps 1–3) |
| H-162 | An open door is closed after a load (`0x11F0(fresh=0)` → `0x0408(0)` zeroes `[0x594f]`); chests, NPC positions and inventory load as saved | **SOFTWARE FIXED — HARDWARE RETEST REQUIRED** (Phase 6S steps 4–5) |
| H-163 | After a town fight (`0x09BC` → `0x0408(0)`) transient terrain is re-read: a skull-keyed vault door is magically locked again. The open door (`COMBAT 0x0bcf`), NPC positions and chests already matched | **CONFIRMED DIVERGENCE — CORRECTED** (transient terrain); verified native match for the rest (Phase 6S steps 6–7) |
| H-164 | `Alt+L` quick load skips `synchronize_loaded_world()`: the world-object pool is neither cleared nor restored, the dungeon session is not restored, live scenes are not cancelled (System Menu / frontend loads do all of that) | **CONFIRMED BY CODE READING — queued, not reproduced.** Use the System Menu load for testing |

**Retest gate:** audit **Phase 6S**. **A reflash is required** (core and runtime changes). **The SD resource pack is unchanged; no SD recopy is required.**

**Host suite:** 94/94 from a clean build, 0 fail, 0 skipped — the prior 93 plus `batch24_reload_parity`. `commands.txt`/`travel.txt` regenerated from the corrected reference (token-level proof in the audit).

## Batch 25 — H-118 shard ritual / world Move lock

The ritual was never defective (CAST `0x15b4` always prints its header; the native port matches it on every exit path). The lock was a native Developer-overlay defect: a core-owned question pending as `Alt+D` was pressed got dropped when the menu closed, leaving `CommandState::awaiting_exit` (or `awaiting_troll`, or a Blackthorn flag) set, so every world command was refused silently until a power cycle. Full derivation, RED/GREEN and the five-mutation proof: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 25".

| Row | Behaviour | Status |
|---|---|---|
| H-118 | After `Alt+D` → Back, the interrupted question returns; a shard Use prints its text; Move/Look/Z-stats answer immediately after, with no power cycle | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6T) |
| H-165 | A question left pending across a Developer teleport *into a dungeon* cannot be answered there (`Exit`/`DeclineExit` are refused by the dungeon context gate), so the lock could return on surfacing | **SUPERSEDED by Batch 31: HOST FIXED.** The real shipped-data runtime reproduced the lock (RED 5/8); Developer now refuses that dungeon teleport until the question is answered (GREEN 11/11). No new physical phase required. |

**Retest gate:** audit **Phase 6T** (9 short steps). **A reflash is required** (core change). **The SD resource pack is unchanged; no SD recopy is required.**

**Host suite:** 95/95 from a clean build, 0 fail, 0 skipped — the prior 94 plus `batch25_shard_ritual`. No fixture moved.

**Still open:** H-115 (dungeon save/load) — the next planned batch.

## Batch 26 — H-115 dungeon save/load

Saving underground is allowed in 1988: `Q` reaches `CAST2.OVL:0x10FE` through the shared kernel dispatcher, with no location gate. The whole save window is written, including the dungeon registers and the 512-byte map, and a load resumes in the dungeon. The native port captured the session (Batch 6) but its sidecar exporter never wrote it, so every dungeon save loaded at the entrance on the surface. Full derivation, RED/GREEN and the seven-mutation proof: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 26".

| Row | Behaviour | Status |
|---|---|---|
| H-115 | Save in a dungeon, reload (System Menu or title Continue): same dungeon, level, cell and facing, with the map as saved (sprung traps stay sprung). The first input after the load acts in the dungeon | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6U part A) |
| H-124 (dungeon context) | As above; world objects (opened chests, spilled loot) load exactly as saved, with no refill, duplicate or stale pre-load object | **DEVICE RETEST PENDING** (Phase 6U parts A and B) |
| H-164 | `Alt+L` underground restores `GameState` but not the dungeon session — now reproduced on host (`batch26_dungeon_save` observation Q) | **CONFIRMED — queued, not fixed.** Use the System Menu load |
| H-166 | The save's own semantic self-check (`alpha_save.cpp` `candidate()`) does not validate the dungeon or object payload, so a corrupt payload is not rejected in favour of the older generation | **CONFIRMED BY CODE READING — queued.** Only a corrupted card reaches it |

**Retest gate:** audit **Phase 6U** (part A: dungeon save, 10 steps; part B: loose loot, 7 steps). **A reflash is required** (core and runtime changes). **The SD resource pack is unchanged; no SD recopy is required.** Existing saves stay loadable; a dungeon save made on older firmware still loads at the entrance, because that is all it contains.

**Host suite:** 96/96, 0 fail, 0 skipped, from a clean build — the prior 95 plus `batch26_dungeon_save`. No fixture moved.

**Still open:** H-154–H-160, H-164, H-165, H-166. Hardware verification of H-115 awaits the user's Phase 6U report.

## Batch 27 — H-164 `Alt+L` quick load

`Alt+L` and System Menu → Load / Save Management → Continue Latest read the same generation through the same `AlphaSaveService::load`. Only Continue Latest then ran `synchronize_loaded_world()`. The `Alt+L` arm ran its own partial copy, which never restored the dungeon session or the loose-object pool. It now calls the same helper. Full derivation, the side-by-side call flow, RED/GREEN, the five-mutation proof and the field-by-field cross-path comparison: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 27".

| Row | Behaviour | Status |
|---|---|---|
| H-164 | `Alt+L` of a dungeon save puts the party back on the saved level, cell and facing with the saved map, at once, and the next input acts in the dungeon. `Alt+L` of a surface save behaves as before. Opened chests and loot come back exactly as saved. A missing or corrupt save changes nothing | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6V, 8 steps) |
| H-166 | unchanged by this batch | **CONFIRMED BY CODE READING — queued** (Batch 28) |
| H-118 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6T), separate |

**Retest gate:** audit **Phase 6V**. **A reflash is required** (runtime change). **The SD resource pack is unchanged; no SD recopy is required.** No save-format change: every existing save loads as before, now identically through both routes.

**Host suite:** 97/97, 0 fail, 0 skipped, from a clean build — the prior 96 plus `batch27_alt_load`. No fixture moved.

**Still open:** H-154–H-157, H-160, H-165, H-166. Hardware verification of H-115 (Phase 6U), H-118 (Phase 6T) and H-164 (Phase 6V) awaits the user's reports.

## Batch 28 — H-166 save-generation validation

A save generation is now accepted or refused whole. Before, the generation gate (`candidate()`/`restore_candidate()`) checked the CRCs, the parse and the gameplay/terrain/NPC data, but not the `"dungeon"` or `"worldObjects"` sidecar. Those were decoded only after the load had committed, and dropped on error. A well-formed but invalid newest generation therefore won over a good older one and loaded with the session or the pool silently missing. The gate now runs both decoders before anything is committed, in one place (`alpha_save_generation.cpp` `stage_generation()`) that every load route, the Generation rows and the save self-check share. Full trace, invariants, RED/GREEN, six-mutation proof, atomicity and cross-path coverage: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 28".

| Row | Behaviour | Status |
|---|---|---|
| H-166 | A newest save whose dungeon or loose-object data is well-formed but invalid is refused whole: Continue Latest, `Alt+L` and title Continue load the older generation, complete; with no usable generation they report it and change nothing. The refused generation is listed **corrupt** in Load / Save Management. Unusual but legal data still loads verbatim. A save whose own data would be refused reports "Save failed; prior kept" | **FIXED ON HOST** — no device phase (host-certified; audit §"Batch 28" Status) |
| H-118 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6T), separate |
| H-115 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6U), separate |
| H-164 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6V), separate |

**Retest gate:** no new phase. The Batch 28 image contains runtime and core changes, so flash it (the image named in the tag `alpha2-batch28-h166-save-validation`) before running Phases 6T, 6U and 6V; their save/load steps also exercise the unchanged SD half of this code. **The SD resource pack is unchanged; no SD recopy is required.** No save-format change, and no save any firmware has written is refused.

**Host suite:** 98/98, 0 fail, 0 skipped, from a clean build: the prior 97 plus `batch28_save_validation` (51 checks). No fixture moved.

**Still open:** H-154–H-157, H-160, H-165. Hardware verification of H-115 (Phase 6U), H-118 (Phase 6T) and H-164 (Phase 6V) awaits the user's reports.

## Batch 29 — rest services: H-154 bed NPC snap, H-155 "Thrown out of bed!", H-160 reclassified

The device bound two of the three rest callbacks to stubs, so the core's correct bed loop was fed nothing: NPCs never moved while the party slept, and nobody could ever throw it out of bed. Both callbacks are now bound in one production function, `AlphaRuntime::bind_rest_services()`, which the host fixture also calls. The snap uses the Batch 24 reposition primitive. The probe uses one occupancy helper that counts NPCs and objects on the party's floor, as kernel `0x368E` does. H-160 was re-derived and is **not** a device defect as written: the device never posts a camp watch, so no guard walks and `cell_free` is never called. The real gap is H-167. Full derivation, side-by-side call flow, RED/GREEN and the nine-mutation proof: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 29".

| Row | Behaviour | Status |
|---|---|---|
| H-154 | While the party sleeps in a bed, every NPC of the location is moved to its post for the current hour on every 10-minute tick, on every floor, from its live (alarm-rewritten) schedule. Killed NPCs and guards slain in a fight do not come back | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6W) |
| H-155 | If an NPC (or object) lands on the party's bed cell after a tick's snap: "Thrown out of bed!", the sleep ends on that tick, the party wakes one step east. A free bed still gives the full rest | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6W) |
| H-160 | Camp watchman collision | **RECLASSIFIED — UNREACHABLE ON DEVICE** (no watch is ever posted; superseded by H-167). No change |
| H-167 | Outdoor (H)ole up never asks "Wilt thou set a watch?" / "Who will stand guard?": every device camp is unwatched | **CONFIRMED MISSING — queued.** Do not file |
| H-168 | Cannon-killed NPC immediately leaves movement, collision and targeting; person remains dead on re-entry, guard may return | **HOST FIXED — Batch 33.** No new device phase; shipped-pack raw-key test covers the production path. |
| H-169 | 1988 runs up to 16 NPC passes before "Zzzzzzz..." that can cancel the sleep; neither port does | **OBSERVED, NOT ADJUDICATED — queued.** Do not file |
| H-118 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6T), separate |
| H-115 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6U), separate |
| H-164 | unchanged by this batch | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6V), separate |

**Retest gate:** audit **Phase 6W** (4 short steps in Lord British's Castle). **A reflash is required** (runtime change): the image named in the tag `alpha2-batch29-rest-wiring`. The same image serves Phases 6T, 6U and 6V. **The SD resource pack is unchanged; no SD recopy is required.** No save-format change.

**Host suite:** 99/99, 0 fail, 0 skipped, from a clean build: the prior 98 plus `batch29_rest_wiring` (26 checks). No fixture moved.

**At Batch 29 exit, still open:** H-156 and H-157 (**reserved for Batch 30**), H-165, H-167, H-168, H-169. Hardware verification of H-115 (Phase 6U), H-118 (Phase 6T), H-164 (Phase 6V) and H-154/H-155 (Phase 6W) awaits the user's reports.

## Batch 30 — H-156 / H-157 sleep ticks and day/night tiles

The 1988 bed loop advances ten minutes, refreshes town terrain if the resulting hour is 05:00 or 20:00, runs kernel housekeeping, then snaps NPCs and checks the bed occupant. The native loop omitted housekeeping and the in-loop refresh; both are now wired to existing routines. The shipped-pack/raw-key host test is 24/24 GREEN, six meaningful mutations are killed, and the fresh complete host suite is **100/100 PASS** (99 prior plus `batch30_sleep_parity`). Full evidence and the separate H-170 Q-duration finding are in `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 30".

| Row | Behaviour | Status |
|---|---|---|
| H-156 | One normal housekeeping call per ten-minute bed tick, including the tick that ejects the party; poison, meals, starvation and Q expiry use the existing kernel-derived turn routine | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6X) |
| H-157 | The shipped-map lamp-adjacent terrain changes at 05:00/20:00 inside the boundary tick, before NPC snap; no refresh on ordinary ticks | **HOST FIXED / DEVICE RETEST PENDING** (Phase 6X) |
| H-170 | Q can make a fixed `hours * 6` native sleep stop short of the original target hour; CMDS stores a wrapped target hour with its subtract-23 midnight quirk | **HOST FIXED — Batch 35; no new physical phase** (42/42 raw-key runtime, six mutations killed, 105/105 full host) |

**Retest gate:** Phase **6X** below, after a later device flash. Phase 6W is already assigned to Batch 29; Phases 6T, 6U and 6V remain pending. The SD pack and save format are unchanged. No hardware was flashed or SD card modified in Batch 30.

### Phase 6X — Batch 30 bed survival and day/night · *PENDING* → **PASS (Batch 52 reconciliation)**

Use the Batch 30 firmware. Close Developer before each sleep and keep enough HP to survive starvation. These steps check device presentation and state. The host observer establishes the exact in-loop terrain ordering; a final device screenshot alone cannot, because the runtime also refreshes terrain after input.

1. Developer → Time **12:50**, Food **0**, heal the party; teleport to **Lord British's Castle**, floor **0**, bed **(9,7)**. Close Developer, press `h`, `1`, Enter. Expect `Starving!` when the clock crosses 13:00 and lower HP on waking. Record clock and HP.
2. Developer → Time **05:50**, Food **5**, heal the party, set one member's Status to **P**, leave the others **G**; teleport to a free castle bed. Press `h`, `1`, Enter. Expect one HP of poison damage per completed sleep tick on P and food reduced by **one** at 06:00 (the G members are asleep during the meal). Record before/after HP, food and clock. If an occupant ejects the party, its final tick must still damage P.
3. Developer → Time **19:50**, teleport to a free castle bed, `h`, `1`, Enter; inspect the castle lamp at **(15,10)** and the tile south at **(15,11)**. Repeat from **04:50** on a free bed. Expect the night overlay after 20:00 and the day tile after 05:00. Host evidence pins that the change occurred during the first boundary tick, before NPC snap.

**Status:** 6X not performed *(at Batch 30; **Batch 52: HARDWARE PASS**, in the 6T–7C block)*. Do not use a Q-duration observation to judge H-156; H-170 is separate and queued. H-165 and all other queued issues remain out of scope.

## Batch 31 — H-165 pending question / Developer dungeon teleport

The original town-exit Y/N/Esc loop blocks the command dispatcher; an ordinary dungeon `(E)nter` cannot occur while it is pending. The Developer menu is native-only. Before this batch it entered a real Deceit session under the parked question; the returning N answer was refused underground, leaving the core latch set. The shipped-pack, raw-key host test reproduces this and proves the fix: a dungeon teleport is refused with **"Answer pending question"**, the question remains answerable, and the same teleport works after answering. Normal `(E)nter` and the successful Developer route yield the same authored Deceit entry state and 512 map cells. The focused suite is 11/11; the two mutation cases are killed. Full evidence: `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 31".

| Row | Behavior | Status |
|---|---|---|
| H-165 | With "Leave this place?" pending, Developer → Teleport → Deceit refuses the move, leaves the town question on return, and N clears it; a retry enters Deceit with normal dungeon controls | **HOST FIXED — no new physical phase required** |

No device flash or hardware check was performed for Batch 31. The production fix is in the shared Developer path exercised on host with authentic dungeon data, and H-165 has no remaining device-only uncertainty. The existing physical checks 6T, 6U, 6V, 6W and 6X remain pending. H-167–H-170 remain queued and untouched.

The fresh Release host suite adds one CTest and passes **101/101**. The clean T-Deck firmware build is **867,184 bytes (`0xd3b70`)**, +64 bytes against Batch 30; the SD pack and save format are unchanged.

## Batch 32 — H-167 camp watch / core rest API

The 1988 Camp route asks for a watch only when at least two party members are `G` or `P`. Y opens "Who will stand guard?"; only a member in `G` posts a watch. A disabled choice or picker cancellation prints "None posted!" and continues unwatched. N continues unwatched without that message. The guard starts at the CampFire arena's south formation cell, cannot walk through the fire or sleepers, skips the partial heal, and consumes watch RNG during each five-minute step. Before Batch 32 the device went directly from hours to an unwatched camp because no guard was passed and the rest service had neither a start cell nor a guard-aware occupancy callback.

The shipped-pack raw-key test is **35/35 GREEN**; six mutations are killed. The fresh Release host suite is **102/102 PASS**, versus Batch 31's 101/101. The T-Deck firmware is **869,024 bytes (`0xd42a0`)**, **+1,840 bytes**; its final commit-stamped Launcher SHA-256 is recorded in the Batch 32 annotated tag. The SD pack and save format are unchanged. No hardware was flashed.

| Row | Behavior | Status |
|---|---|---|
| H-167 | Optional watch prompt, one valid `G` guard, invalid/cancel fallback, guard walk and RNG | **HOST FIXED / DEVICE RETEST PENDING — Phase 6Y** |
| H-171 | Camp entry Q/T clear, per-step redraw wind, and hourly encounter RNG order; no survival turn housekeeping | **HOST FIXED — Batch 36; no new device phase** |

### Phase 6Y — Batch 32 camp watch · *PENDING* → **PASS (Batch 52 reconciliation)**

Use the Batch 32 Launcher after the existing flash plan. Keep the SD resource pack as is. The host test proves the underlying choice and walk mechanics; this physical check proves the handheld prompt, picker and return presentation.

1. On outdoor land, on foot, with Avatar and Iolo in the party and both at status `G`, note HP and clock. Press `h`, `1`, Enter. Expect **"Wilt thou set a watch?"** before time advances.
2. Answer `Y`. Expect **"Who will stand guard?"**; select Iolo (`2`). Expect the one-hour camp to finish at the next hour and return to ordinary movement controls. If no apparition occurs, Iolo receives no partial camp heal while the other eligible member does. An apparition can fully heal everyone; repeat if it occurs.
3. Repeat from the same outdoor state, answer `Y`, then cancel the guard picker. Expect **"None posted!"**, followed by normal camp completion and ordinary movement controls. Answer `N` on a third camp: no guard picker and no "None posted!".

**Pass:** all prompt/choice paths and the normal command return appear in that order. **Fail:** hours immediately start the camp with two eligible members, the picker refuses a valid `G` member, cancellation abandons camp, or input remains trapped after waking. This phase remains limited to watch presentation; Batch 36 resolved H-171 on the host. Pending phases **6T, 6U, 6V, 6W, 6X** keep their existing meanings.

## Batch 33 — H-168 cannon NPC lifecycle

The 1988 hit immediately clears the NPC's live slot after the eligible dead-bit setter. Batch 33 now does that through the existing native despawn routine and the corresponding TypeScript clear-slot routine. The shipped Ararat/castle raw-key host test is 19/19; five lifecycle mutations are killed; the fresh full host suite is 103/103. A killed person cannot move, block or take a second cannon hit before reload. An authored type-`0x70` guard despawns immediately and can return on re-entry because the original setter does not persist its dead bit.

**Hardware status:** no device check or flash was performed for Batch 33, and no new phase was allocated. Host exercises the actual T-Deck command and NPC update path; H-168 has no remaining device-specific question. Phases **6T, 6U, 6V, 6W, 6X and 6Y** remain pending with their existing meanings. H-169, H-170 and H-171 remain out of scope.

## Batch 34 — H-169 pre-sleep NPC loop

The original accepted bed command runs up to 16 full NPC passes at the current hour before `Zzzzzzz...` or the first ten-minute tick; an adjacent AI-6/7 hostile stops sleep after that pass and its redraw if no later talk NPC overwrites the shared marker. The production raw-key shipped-map host test passes **21/21**: one-pass and two-pass cancellation, visible first-pass NPC movement, exactly 16 uninterrupted passes, clock/housekeeping separation, Camp and zero-hour controls. Eight mutations were killed; the regression subset is 16/16 and the fresh full host suite is 104/104. H-172 separately queues the original viewport fill after `Zzz`. H-170 and H-171 are unchanged.

**Hardware status:** no flash or device check was performed, and no new phase was allocated for H-169. The production T-Deck input and runtime path is exercised on host with authentic local maps. Pending phases **6T, 6U, 6V, 6W, 6X and 6Y** retain their existing meanings. The Batch 34 Launcher is a build artifact, not a flash instruction.

## Batch 35 — H-170 Q sleep target hour

The original compares a local target hour before each bed tick; Q halves clock advances until its separate turn counter expires. Native now stops at that original target, including the 23 subtraction when the requested hour crosses midnight. The shipped-pack raw-key host fixture passes 42/42 and the fresh full host suite passes 105/105. Terrain refresh, housekeeping, NPC snap and ejection still run on the target tick. Host evidence fully proves this logic, so no new device phase is allocated. **No flash or physical check was performed.** Pending phases **6T, 6U, 6V, 6W, 6X and 6Y** keep their existing assignments; H-171 and H-172 remain queued.

The clean Batch 35 ESP-IDF 6.1 image is **869,376 bytes (`0xd4400`)**, +16 bytes against Batch 34 (`native/targets/tdeck/batch35-firmware-build.log`). The Launcher is packaged only after the commit so its embedded revision matches the annotated tag; it is a build artifact, not a flash instruction.

## Batch 36 — H-171 Camp redraw and spell clearing

Original-byte disassembly corrects the Batch 32 H-171 queue: `0x2900` is status drawing, `0x20fa(1)` is a timer delay, and Camp never calls turn housekeeping `0x2ae8`. Accepted Camp clears Q/T and its counter; each five-minute iteration redraws (including wind RNG), rolls regeneration rings, then advances the clock, with guard movement afterward. The hourly encounter roll follows the next-hour redraw and ring sweep. Poison, meals, starvation and turn counter do not progress while camping. The raw-key shipped-pack fixture and host regressions exercise the production runtime path. The focused fixture is 24/24, eight mutations are killed, the affected subset is 18/18, and the clean full host suite is 106/106. Item parity defers 432 Camp comparison rows and gameplay parity retains 4,802 non-Camp sequences; the original-backed fixture owns Camp. The clean ESP-IDF 6.1 image is **869,456 bytes (`0xd4450`)**, **+80 bytes** from Batch 35 (`batch36-firmware-build.log`). There is no H-171 device-specific question, so no new phase is assigned. **No hardware check or flash was performed.** Pending **6T, 6U, 6V, 6W, 6X and 6Y** retain their meanings. H-172 is untouched.

**Separate queue:** H-173/D-30 records nonzero-minute Camp duration (original target-hour stop versus native fixed twelve steps per hour). It was not changed in Batch 36. H-172 bed-entry presentation is also still queued.

## Batch 37 — H-172 bed-entry viewport fill

Host raw-key framebuffer proof establishes a synchronous black copy fill of the map-image rectangle after `Zzzzzzz...`, before the first ten-minute tick. It persists through all bed ticks and restores on the next ordinary redraw. The 12/12 focused checks, seven killed mutations, 19/19 affected subset and 107/107 fresh full suite passed. The clean ESP-IDF 6.1 image is **869,888 bytes (`0xd4600`)**, **+432 bytes** from Batch 36 (`batch37-firmware-build.log`). The host capture Board does not exercise the TFT/SPI transfer, so the physical pixel result remains unobserved. No check or flash was performed. Existing phases **6T, 6U, 6V, 6W, 6X and 6Y** retain their assignments. H-173 remains queued separately; H-174 records the distinct intermediate roster refresh omission.

### Phase 6Z — Batch 37 bed-entry map blackout · *PENDING* → **PASS (Batch 52 reconciliation)**

| # | Setup | Input | Expected visible result | Expected state change | Result |
|---|---|---|---|---|---|
| 6Z | On an empty LeftBed in Lord British's Castle, with a clock visible and one awake `G` party member | `H` → `1` → Enter; observe immediately after `Zzzzzzz...` and until wake | The **map image** becomes solid black at once, including bed, party and NPC pixels; cyan frame, sky/wind captions, transcript and party panel remain visible. The map stays black across sleep updates and is restored by the ordinary wake redraw, with no stale black cells after the party steps east. | Clock reaches the original target hour; the party wakes one cell east. The interim roster status is tracked separately by H-174. | **UNTESTED** → **Batch 52: HARDWARE PASS** (6T–7C block; see Batch 52) |

Use the Batch 37 commit-stamped Launcher when this phase is eventually run. Do not infer Phase 6Z PASS from the host framebuffer stub. The SD pack is unchanged; no SD recopy is required.

## Batch 38 — H-173 Camp target-hour duration

The 1988 Camp loop stores (start hour + requested hours) modulo 24 and tests the live hour before every five-minute iteration. It does not preserve the start minute as a target. Thus 05:50 + one hour ends 06:00 after two steps, 23:55 + one hour ends 00:00 after one, and 12:01 + one hour first reaches the target hour at 13:01 after twelve. The final step moves a posted guard; no extra redraw, ring or encounter draw follows it. Native previously ran twelve steps per requested hour. The Batch 38 target-hour correction passes the shipped-pack raw-key 43/43 focused checks, six killed mutations, 9/9 affected regressions and 108/108 full host tests. A clean ESP-IDF 6.1 build completed at **869,856 bytes (0xd45e0)**, **32 bytes smaller** than Batch 37, after retrying an unrelated ESP-IDF compiler crash with two build jobs. The host fixture exercises the real runtime clock, wind/ring RNG, intervening encounter check, and final guard movement, leaving no device-specific H-173 question. **No new hardware phase is allocated, and no flash or physical check was performed.** Pending 6T, 6U, 6V, 6W, 6X, 6Y and 6Z keep their existing meanings. H-174 interim party-panel refresh remains queued separately.

## Batch 39 - H-174 intermediate bed status-panel refresh

Original CMDS calls the status renderer after eligible G-to-S, before Zzz and black fill, then once per ten-minute tick after housekeeping and before NPC snap/ejection. It presents S at entry; tick frames update HP/status and clock. A sleeping active selection is cleared. The final sleeping frame lasts until ordinary redraw after wake. Native now draws only the right status region synchronously. Raw-key host framebuffer RED was 9/17; corrected 17/17, with nine mutations killed, 20/20 affected regressions and 109/109 full host tests. A clean ESP-IDF 6.1 firmware build hit an internal compiler error in unchanged ESP-IDF LCD code, then completed on a one-job retry (batch39-firmware-build.log and batch39-firmware-retry.log). The image is **870,640 bytes (0xd48f0)**, **+784 bytes** from Batch 38, with 0x2b710 bytes free in the 1 MiB app partition. The commit-stamped Launcher is packaged after commit; its SHA-256 is recorded in the annotated tag. The host Board does not exercise physical TFT/SPI glyph transfer. **No hardware check or flash was performed.** Pending 6T-6Z keep their meanings.

### Phase 7A - Batch 39 bed status-panel text - *PENDING* → **PASS (Batch 52 reconciliation)**

| # | Setup | Input | Expected visible result | Expected state change | Result |
|---|---|---|---|---|---|
| 7A | Empty LeftBed in Lord British's Castle; one selected awake G member, a poisoned second member with visible HP, and the clock | H -> 1 -> Enter; observe before Zzz, through six sleep ticks, and at the next ordinary redraw | The intermediate panel first shows S with no active selection while the map is still visible. Zzz and black fill follow. Each tick updates right-panel HP/clock while the map remains black. The sleeping panel persists through wake, then ordinary redraw restores G and the map. | Poison HP loses one per tick; clock reaches 13:00; party wakes and steps east. | **UNTESTED** → **Batch 52: HARDWARE PASS** (6T–7C block; see Batch 52) |

Use the Batch 39 commit-stamped Launcher when this phase is eventually run. Phase 6Z remains the separate map-blackout TFT check. The SD pack is unchanged.

## Batch 40 — advancement audit

Original-byte tracing and a real Rest-command host test establish that level evaluation occurs only in the successful Camp apparition. It does not run on XP award, an ordinary turn or bed sleep. The existing native progression formula, HP/stat/MP mutations and roster RNG order pass 21/21 focused checks and eight targeted mutations; the affected subset passes 15/15 and the fresh full host suite passes 110/110. No production logic changed, no firmware was built for this audit, and no hardware was flashed or tested. Existing pending phases 6T-6Z and 7A are unchanged.

### Phase 7B — Camp apparition staging and visuals — *RESERVED, UNTESTED* → **run 2 logic/visual PASS; pacing fixed B51, PASS in 7D (Batch 52)**

Batches 41–43 host-corrected H-175/D-32, H-176/D-33 and H-177/D-34. Accepted outdoor Camp now shows the CampFire sleep scene before the apparition gate, including when it misses. Sleepable non-guard members lie on tile 0x11e and show S in the panel; a posted guard and poisoned P member remain standing, with guard movement visible. The panel clock updates during Camp. A successful gate then materializes the figure over the fire, wakes each live actor in roster order, XORs the full 176x176 window once after each chime, and restores the viewport before level/Hail. Advancement panel updates follow each eligible member's acknowledgement. Host tests confirm pixels and ordering; physical TFT pulse duration belongs to this reserved phase. Phase 7B remains untested.

| # | Setup | Input | Expected visible result after Batch 43 host correction | Expected state change | Result |
|---|---|---|---|---|---|
| 7B | Outdoor on foot with a visible roster: a sleepable ring-42 wearer, a poisoned ring-42 wearer, an optional posted ring-42 guard, and a dead-cell control. Prepare repeatable missed and successful gates; give live members advancement XP on success. | Camp on both gate outcomes. Observe the first CampFire frame, a guard-move redraw, then each successful apparition cue and acknowledgement. Optionally use a seed where the ring expires at entry. | The ring wearer who sleeps uses 0x11e; a surviving ring-42 P/guard is a visible 0x11d outline from mount through redraw; the dead cell exposes terrain. An expired ring leaves an ordinary standing class actor. Fire, clock and S/P/G/D panel remain visible. A successful gate materializes the apparition, then each live actor receives its class standing tile before the full-window XOR/restore and Hail. | Entry expiry and all later RNG, guard motion and advancement retain the host-proven sequence; surviving rings remain equipped after apparition wake. | **Run 1, image `Git c1226a0af4c3` (Batch 43): INVALID — stale firmware, certifies nothing.** **Run 2, image `Git 0bbdbf5c86f5` (Batch 48): visual/logic staging PASS** (scene, sleepers/standing, ring-42 outline, guard, apparition, full-window inversion/restore, Hail/advancement, exit); **pacing FAIL — "way too fast", every beat had near-zero dwell** → H-182/D-39, host fixed Batch 51; retest the pacing in **Phase 7D-A**. H-175–H-179 host fixed. |

## Batch 41 — staged advancement host closeout

The original Camp apparition processes live members in roster order after wake/chime/chord. A changed member receives level/HP/stat mutation and Hail before an unfiltered getkey; MP and status-panel redraw follow the key, then the next slot and its RNG draw. Dead and unchanged slots have no Hail/key wait or stat draw. Karma speech has a separate key wait, then disappearance and Camp completion. Native H-175 now holds at each point. Core intermediate-state checks pass 18/18; shipped-pack raw-key checks pass 9/9; nine mutations were killed; affected regression passes 16/16 and the fresh full host suite passes 112/112. The clean production app is 871,424 bytes (+784 against Batch 40). Save/load/menu shortcuts are scene keys, so no half-advanced save is possible. No hardware was flashed or tested. Phase 7B stays reserved, and H-176/D-33's distinct standing-actor/XOR visual gap must be addressed before its full visual expectation can pass. Phases 6T–6Z and 7A remain pending.

## Batch 42 — standing actor and viewport XOR host closeout

Original OUTSUBS writes a class standing frame into the CampFire actor's two animation frames at its south formation cell, chimes, XORs white over the complete 176x176 game window once, holds through the long chord, and redraws to normal before level/Hail. Dead slots skip it; live slots without a level change still receive it. Native now draws the apparition arena and per-slot full-window frames without moving the world party or refreshing the status panel early. The raw-key framebuffer check is 23/23, eleven mutations were killed, the affected subset is 19/19 and the fresh full host suite is 113/113. The clean firmware app is 872,432 bytes, +1,008 from Batch 41. No hardware was flashed or tested. Phase 7B remains **RESERVED, UNTESTED** for the standing sprite, inverted frame and subjective pulse dwell on the TFT; phases 6T–6Z and 7A keep their pending assignments. H-177/D-34 is the separate, unfixed Camp sleep-scene omission.

## Batch 43 — pre-apparition Camp sleep-scene host closeout

Shipped-byte tracing places the CampFire party scene, sleeping status and guard movement before the 25% gate. The real raw-key framebuffer fixture was RED 6/11 on Batch 42 and GREEN 20/20 after correction. It covers the missed gate, a successful gate with no level gain, the poisoned standing exception, status panel, central fire, actor placement and guard motion. The affected host subset passes 19/19 and the fresh full host suite passes 114/114; nine targeted mutations were killed. The clean app is 872,928 bytes (+496 from Batch 42). Original-byte and mutation evidence is under native/core with the batch43 prefix. H-178/D-35 separately queues equipped-ring expiry at scene entry. Phase 7B absorbs the physical view check; no new phase is allocated. Phase 7B and pending 6T–6Z/7A remain untested; no device was flashed.

## Batch 44 — equipped-ring Camp scene-entry host closeout

The original CampFire actor builder first rolls expiry for each living wearer of ring 42 or 44. A result of 11 removes that ring with its vanish cue. A surviving, non-sleeping ring-44 actor then runs the party-wide regeneration helper before the next actor's expiry roll. This can immediately heal an injured member and always shifts later RNG, even at full HP. Dead slots do not roll; poisoned members and posted guards do, while an already sleeping member can receive another actor's regeneration pass without triggering its own. Native now follows that order before the Batch 43 sleep scene. The shipped-pack host fixture is RED 4/20 against Batch 43 and GREEN 20/20 after correction; fifteen mutations are killed. Phase 7B retains the Camp scene, expiry cue, and subsequent apparition presentation. H-179/D-36 separately queues the standing ring-42 invisible actor rendering before that phase can fully close. No new physical phase is allocated. Phases 6T–6Z, 7A, and 7B remain pending and untested; no device was flashed.
Final host evidence: focused 20/20, affected CTest subset 21/21, full fresh host suite 115/115, and 15/15 mutations killed. Clean app size is 873,152 bytes (+224 against Batch 43). No physical testing or flash occurred.

## Batch 45 — H-180/D-37 scheduled NPC bed presentation

The physical 23:45 report included an upright named bed occupant and a visually empty bed whose Look result was a guard. Shipped original code and CASTLE.NPC show the upright pose is original; native's dialog-zero actor filter caused the empty guard bed. Host correction is complete. H-179/D-36 remains the separate Camp ring-42 visual gap. No hardware was flashed or checked. Existing 6T–6Z, 7A and 7B checks remain pending.

### Phase 7C — nighttime NPC beds and silent guard — *RESERVED, UNTESTED* → **run 2 PASS (Batch 52 reconciliation)**

| # | Setup | Input | Expected visible result | Expected logical result | Result |
|---|---|---|---|---|---|
| 7C | Lord British's Castle after 23:00; named slot 13's (9,7,0) bed and guard slot 1's (17,7,0) bed. | View both, Look east at the guard from (16,7), then cross 06:00 or leave/re-enter after wake. | Named occupant shows original 0x15c upright graphic; guard shows 0x170 above its bed. After the guard leaves, its 0xab bed is exposed. No invisible logical occupant or lingering sleep tile. | Look identifies “a guard”; the guard blocks its cell and remains in its ordinary actor slot. | **Run 1, image `Git c1226a0af4c3` (Batch 43): INVALID — stale firmware** (its "invisible guard / non-reclining avatar" matched pre-Batch-45/48 code exactly; Batch 50 was cancelled for this reason with no production change). **Run 2, image `Git 0bbdbf5c86f5` (Batch 48): authoritative criteria PASS** — controlled actor reclines on the 0xab head, silent guards visible, Look/collision/schedule correct. The exact named-NPC 1988 sleeping pose stays qualified by Batch 49 and is not a pass/fail criterion. |

Batch 45 host evidence: focused 14/14, affected 17/17, fresh full suite 116/116 and four killed mutations. Clean app 873,136 bytes (−16 from Batch 44). Phase 7C remains untested; no flash or physical check occurred.

## Batch 46 — Camp Ring of Invisibility host closeout

Original Camp shows a surviving ring-42 guard or poisoned P member as a visible outline sprite 0x11d from scene mount through sleep-scene redraw. A ring wearer who sleeps still uses tile 0x11e; a dead member leaves the background visible. Expiry before mount leaves the ordinary standing class tile. Status rows, fire and clock remain visible. On apparition, each live actor changes to its class standing tile before the XOR flash and Hail; the ring remains equipped. Host framebuffer RED 13/16 became GREEN 16/16 with nine killed mutations and original RNG preserved. Existing Phase 7B already covers this exact Camp and apparition scene, so its future physical check now includes a surviving ring-42 guard/P, sleeper, and optional expired-ring control. Phase 7C remains the separate NPC-bed check; 6T–6Z and 7A are unchanged. All remain untested. No hardware was flashed.

Batch 46 validation: 16/16 focused, 22/22 affected, 117/117 fresh host, nine killed mutations; clean firmware 873,184 bytes (+48 against Batch 45). Phase 7B remains reserved and untested. No flash or physical check occurred.

**Batch 48 extension to Phase 7C (H-181/D-38; still UNTESTED).** In the same castle visit, use an empty authored 0xab head such as (9,7) while its scheduled occupant is away. Walk the controlled on-foot actor onto that head: the next world draw should show the 0x11a reclining-in-blue/white-bed tile. Step onto 0xac or off the bed: the ordinary 0x11c actor should return immediately, without leaving/re-entering the map. Then retain the existing nighttime 0x15c sleeper, 0x170 silent guard, Look and occupancy controls. This extends reserved Phase 7C; no new phase, flash or physical result is recorded.

**Batch 49 qualification of Phase 7C — still RESERVED, UNTESTED.** The 7C native named-NPC 0x15c and guard 0x170 expectations above describe the unchanged Batch 48 build, not a verified 1988 pose. Fresh original bytes show normal castle types 0x5c/0x70 reaching the shared bed selector, while the live original map cell and rendered frame remain unmeasured. Resolve the original pose from a paired original memory/framebuffer witness before using a 7C NPC pose result as a preservation pass/fail criterion. Guard visibility, Look/occupancy, exposed bed on departure, and controlled-actor 0x11a checks remain on the existing 7C row. No new phase, flash or physical result.

## Batch 51 — scripted-scene pacing (H-182 / D-39; H-122 re-adjudicated)

The physical Phase 7B run on the confirmed Batch 48 image showed every Camp apparition visual correct and the whole sequence "way too fast". The cause is shared: the device honoured an original wait only inside a scene pacer and only when it was a BIOS-tick primitive (`delay`/`run_n_frames`). Waits the original spends in `tone_sweep` (which blocks even with sound off) or a fizzle were zero, and the Camp apparition had no pacer at all. Batch 51 paces the apparition with the original's own waits and gives the Blackthorn materialization, holy-circle frame and sacrifice siren their holds. TrollSneak, which uses only tick waits and already passed on hardware (H-137), is the unchanged control. Host evidence: focused 28/28 + 21/21 on a virtual clock, 12/12 mutations killed, fresh full host suite 120/120. Firmware 874,864 B, +1,520 vs Batch 48. **SD resource pack unchanged; no SD recopy required.** Not flashed.

### Phase 7D — scripted-scene pacing on the TFT — *PENDING* → **PASS (Batch 52)**

**Gate first:** the boot identity screen must read `Git <first 12 hex of the alpha2-batch51-scene-pacing commit>` (the tag message states it). Film the screen if you can — a phone at 60 fps resolves every hold below; 120/240 fps resolves the 55 ms circle frame. Durations are the device's: **A** = exact original ticks, **B** = the calibrated floor of a host-dependent 1988 wait (1988 machines held it longer, up to the stated witness), **D** = a one-tick minimum so a transitional frame is visible at all.

| # | Setup | Input | Expected visible result | Pass criterion | Result |
|---|---|---|---|---|---|
| 7D-A | As 7B: outdoor on foot, at least two live members — one at level 1 with ≥100 XP (it will level), one without. Keep a save to retry the 25 % apparition gate. | H → hours → Enter → no watch. During the first inverted pulse press **Alt+S, Alt+M, Mic, Alt+D, Alt+L** once each. At the first Hail press **Alt+S**; at the karma speech press any key. | "An apparition!", then the sleeping camp holds **≈1.5 s** (B ≥1.55 s: materialize + six-note arpeggio) before the figure appears over the fire; the figure alone for a moment (D); the member stands **≈¼ s** (A 55 ms + B 193 ms chime); then the **whole 176×176 viewport inverts and stays inverted ≈2.3 s** (B ≥2.325 s; 1988 hosts held 2.75–4.57 s); it restores and **≥165 ms** later (A) the Hail appears and waits. The keys pressed during the inversion do **nothing** — no "Save complete", no menu, no Developer overlay, no load, no skipped beat. At the Hail, Alt+S advances (Batch 41) and never saves. Each further member gets its own full pulse; a non-levelling member's pulse is followed by ≈0.4 s of normal scene. The karma speech waits for a key; then the figure vanishes and the camp ends. | Every inversion visibly holds **at least 2 s**; nothing between "An apparition!" and the first Hail reads as instantaneous; dwell keys have no effect; order identical to 7B run 2. Serial: `CAMP_SCENE_PACED begin=apparition-materialize wait_ms=387`, `CAMP_SCENE_INPUT … effect=swallowed` per dwell key, `NARRATIVE_SCENE_END scene=3`. | **HARDWARE PASS — Batch 51 image** (user, Batch 52 prompt): staging and dwell perceptible, no longer too fast. The dwell-key swallow was not reported separately; it is host-certified. |
| 7D-B | H-120: Blackthorn's palace capture; a party of three or more living members if you want the sacrifice. | Run the capture; answer the interrogation (a wrong answer reaches the warning; a correct mantra at the right point reaches the sacrifice). | After the two guards take their posts behind the prisoners and pause, **Blackthorn's cell stays empty ≈½ s** (B ≥503 ms materialize sweep), then the **holy circle (0x116) is shown as its own frame** (D ≥55 ms — brief but distinct; the LFSR dissolve is still not modelled, so it cuts), then Blackthorn stands ≈0.44 s (A, 8 ticks) before speaking. Prompts stay readable; Enter continues. Sacrifice: after the pause the **victim stays frozen on the table ≈7 s** (B ≥7.13 s siren), then goes dark and the table is empty. | The circle frame is seen (video) and the materialization pause and siren hold are clearly present. **Not a regression:** no explosion burst is drawn on the victim — queued H-186. | **HARDWARE PASS — Batch 51 image**: pacing and materialization much better. The circle frame was not reported separately; it is host-certified. H-186 is still queued. |
| 7D-C | H-137: a bridge troll ambush (control — this mechanism was not changed). | Walk onto the bridge until the trolls spot you. | Unchanged from Batch 20: "Thou spieth trolls under the bridge!", ≈550 ms (A 10 ticks), "$ sneaks across" with dots ≈275 ms (A 5 ticks) apart. | Same as the Batch 20 PASS; no new delay anywhere else. | **HARDWARE PASS — Batch 51 image**: the TrollSneak control is unchanged. |

**Queued by the Batch 51 audit — classification only, none fixed; do not file as regressions:**

| # | Scene | Observation to expect on the device | Original | Ledger |
|---|---|---|---|---|
| ~~H-183~~ | Shrine "Quest is ordained" and the Codex reading | ~~All the text after each altar/Codex key wait appears at once; no key is waited for.~~ **HOST FIXED — A3-HF6** (`ALPHA3_AUDIO.md` §32): each of the eleven getkeys now holds the rest of the rite until a key. Device check: **Phase H-206**. | CAST2 getkeys `0x0a9b`/`0x0abc`, `0x0d2b`…`0x0e5b` (base `0xE1E0` → kernel `0x266c`) | D-40 |
| ~~H-184~~ | Donation "ALAKAZAM!", "WELL DONE!", Codex ceremony | ~~No viewport inversion at all; the Codex shows its three quakes but not its three XOR pulses.~~ **HOST FIXED — A3-HF7** (`ALPHA3_AUDIO.md` §33): the map viewport is XORed with 15 through the sweeps (and WELL DONE's shake) and restored with the reward line; the Codex XORs 4, 11, 15 over its three shakes and restores at the next getkey. Device check: **Phase H-207**. | `rect_XOR` `0x0bcd`/`0x0c41` held through 920 sweeps, then `run_n_frames(10)`; Codex XOR+shake ×3 `0x0dbd`–`0x0dee` | D-41 |
| ~~H-185~~ | Refuge | ~~Cadence is Class C (70 ms/unit + reading floors); the karma speech holds on a timer instead of waiting for a key.~~ **HOST FIXED — A3-HF9** (`ALPHA3_AUDIO.md` §35): every line and figure now waits exactly what `party_refuge` blocks in (ticks, the 10 s slumber melody, two shakes, one-tick fizzle floors), and Lord British's karma speech waits for a key. Device check: **Phase H-210**. | exact `delay(n)` ticks; `getkey` `0x0b3e` | D-42 |
| ~~H-186~~ | Blackthorn sacrifice | ~~No explosion burst is drawn on the victim.~~ **HOST FIXED — A3-HF8** (`ALPHA3_AUDIO.md` §34): after the siren, tile 0 (the explosion star) covers the victim's cell for 174 ms with the kernel's noise burst, and only then does the victim go dark. Device check: **Phase H-208**. | burst `0x041e` (→ `0x3522`: blit tile 0, `noise_burst(0x7d0,0xbb8,0xa)`, redraw) fires **before** the victim clears (`0x0421`) | D-43 |

## Alpha 2 Readiness — Batch 52 (2026-09-25)

**Verdict: ALPHA 2 NOT READY FOR RELEASE CANDIDATE.** Four release blockers were found by audit and by a device-shape probe, not by a hardware run: RB-1 H-189, RB-2 H-187, RB-3 H-188/H-191 and RB-4 H-146. The full evidence, the blocker criteria and the reconciliation table are in `GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 52". **No production code, firmware or SD pack changed in Batch 52.** The runtime baseline is still the Batch 51 image: 874,864 B, `Git 045cb092b81b`, Launcher SHA-256 `5323b0dd83251875a287beec8f165097dc134ab20ab0f5114ee4ee8bb9bede6d`.

### Physical phases — authoritative status

| Phase | Result | Image / note |
|---|---|---|
| 6T, 6U, 6V, 6X, 6Y, 6Z, 7A | **HARDWARE PASS** | User report of the 6T–7C block (2026-09-25). No hash was recorded, because the gate did not exist yet. The session's 7B/7C half was later identified as `Git c1226a0af4c3` (Batch 43), which contains every fix these phases test. The later changes on their paths (B44–B51) are presentation that 7B/7C run 2 and 7D exercised again. |
| 6W | **HARDWARE PASS (functional)** | Snap, occupancy, "Blocked!", Look and ejection all worked. The bed *visuals* from that session came from the stale image; see 7C run 2. |
| 7B | Run 1 **INVALID** (B43). Run 2 **logic/visual PASS, pacing FAIL** (`Git 0bbdbf5c86f5`, B48). | The pacing fix is H-182 (B51), which passed in 7D-A. |
| 7C | Run 1 **INVALID**. Run 2 **authoritative criteria PASS** (B48). | The named-NPC 1988 pose is still an evidence question (Batch 49), not a criterion. |
| 7D | **HARDWARE PASS** on the Batch 51 image, confirmed by the user | A: Camp pacing dramatically improved, dwell perceptible. B: Blackthorn pacing and materialization much better. C: TrollSneak unchanged. The dwell-key swallow (7D-A) and the one-tick circle frame on video (7D-B) were not reported separately; both are host-certified. |
| 6Q | **HARDWARE PASS** | The user confirmed the basement chest hydration in the Batch 23 prompt (2026-09-22). The same report saw the H-148 bed reset working. |
| 6M, 6M.1, 6N, 6P, 6R, 6S | **no recorded result** | Host-certified. The items still owed become steps in Phase 7E (below). |

### Rows still marked FAIL / BLOCKED / INCONCLUSIVE / UNTESTED — current disposition

The historical result cells above are left as recorded. Category numbers follow the audit: **1** superseded, **2** hardware pass, **3** host-certified with no device question left, **5** original quirk preserved, **6** evidence gap (non-blocking), **7** future / Alpha 3, **8** release blocker.

| Row | Batch 20 result | Now | Where it was settled / what remains |
|---|---|---|---|
| H-12, H-13 | FAIL | **8** | Root-caused in Batch 52: the device owns no moonstones, so every phase fails (H-188 / RB-3). The `To phase:` text is also overdrawn by the `Aim:` overlay (D-53). Fix in Batch 53, then Phase 7E. → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-C)** |
| H-15 (text half) | FAIL (presentation) | 7 | The same overlay. Harmless. |
| H-22 | FAIL | 7 | The original folds case (kernel `0x6f1e`, proven this batch); native does not. Easter egg. D-50. |
| H-23 | PASS (partial) | 2 | Blackthorn prompts were readable in 7D-B; the shrine chain passed in H-31. |
| H-30 | BLOCKED | 3 | R-33 host fix. Never run at the corrected location: Phase 7E. |
| H-45 | FAIL | 7 | Still refunds the potion on cancel. Player-favourable. D-51. |
| H-51 | INCONCLUSIVE | 6 | Code-verified; the location confounded the result. |
| H-63 | FAIL | 7 | The `^` glyph is still missing. Cosmetic. D-52. |
| H-78 | INCONCLUSIVE | 1 | Superseded by the H-149 adjudication; H-77/H-81 passed. |
| H-84 | UNTESTED | 6 | `dungeon_art_regression`. |
| H-98, H-99, H-101 | UNTESTED | 3 | Batch 12B on the production path. |
| H-100 | UNTESTED (negative gate) | 5 | 50 of the 128 shipped `.CBT` boards author slots on BlackSquare. |
| H-102 – H-105 | UNTESTED | 5 | The In \*Grav negative gate is original (`bugs-del-original.md` §2.9). |
| H-106 | UNTESTED | 3 | Combat and magic parity. |
| H-115 | FAIL | 2 | B26; 6U PASS. |
| H-118 | FAIL CRITICAL | 2 | B25; 6T PASS. |
| H-119 | UNTESTED | **8** | Cannot work on the device: Words of Power are unbound (H-187 / RB-2). → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-B)** |
| H-120 | UNTESTED | 2 | 7D-B (B51). |
| H-121 | UNTESTED | 5 | Original; do not file. |
| H-122 | KNOWN OPEN | 2 | 7D-B PASS. The residual class-B/C timing is category 6. The sacrifice burst is H-186 (7). |
| H-123 | UNTESTED | 6 | Reachable in normal play only after RB-2 is fixed: Phase 7E. |
| H-124 (dungeon) | PENDING | 2 | 6U PASS. |
| H-128, H-129 | UNTESTED | 6 | Internal auto-sleep (7C walked onto beds with no stray command); Developer-only. |
| H-135 | UNTESTED | 6 | Its only normal trigger is H-119: Phase 7E. |
| H-136, H-138 | UNTESTED | 6 | Y-04 host. |
| H-137 (Refuge half) | UNTESTED | 6 | **Never device-run, and it is the party-wipe recovery path: Phase 7E (required).** |
| H-139, H-140 | UNTESTED (known open) | 6 | D-8. Alpha 3. |
| H-146 | FAIL | **8** | Still unbound: `ShopServices::ship` / `horse` / `reserve` (RB-4). → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-D)** |
| H-148 | INCONCLUSIVE | 1 | B21B fix; the reset was seen on the device (Batch 23 report). |
| H-149, H-150, H-152 | FAIL CRITICAL | 5 | Original behaviour; H-150/H-152 were seen behaving as adjudicated (21A.3 report). |
| H-151 | FAIL CRITICAL | 3 | Core fix B21A (RED-7). The device observation is owed: Phase 7E. |

### New rows — found by Batch 52 (code, bytes and the device-shape probe; no hardware run)

| Row | Behaviour on the Batch 51 device | Status |
|---|---|---|
| H-187 | Yell a Word of Power at a dungeon entrance → always "No effect!", and the seal never opens. INIT.GAM starts all eight dungeons sealed, so no dungeon can be entered without Developer teleport. | **RELEASE BLOCKER (RB-2)** → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-B)** |
| H-188 | Moongates never transit, Vas Rel Por always prints "Failed!", and searching for or using moonstones does nothing. The runtime owns no moonstones. | **RELEASE BLOCKER (RB-3)** → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-C)** |
| H-189 | Final Doom battle with the Wooden Box: the victory ending is refused, and the arena never tears down (only `Alt+D` / `Alt+S` / `Alt+L` answer). | **RELEASE BLOCKER (RB-1)** → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-A)** |
| H-190 | The Camp apparition and Refuge karma speech show an invented sentence instead of the KARMA.DAT text. | non-blocking; ride Batch 53 → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-E)** |
| H-191 | No moongate tile is drawn at night. | **RELEASE BLOCKER (part of RB-3)** → **Batch 53: SOFTWARE FIXED — HARDWARE RETEST PENDING (7E-C)** |

### Reconciled disposition (Batch 52)

Exactly **37** of the 152 Batch 20 rows have a result cell that reads FAIL, BLOCKED, INCONCLUSIVE or UNTESTED: 12 FAIL, 1 BLOCKED, 3 INCONCLUSIVE and 21 UNTESTED. They are dispositioned as follows:

| Category | Rows | Count |
|---|---|---|
| **2** — hardware PASS since | H-115, H-118, H-120 | 3 |
| **1** — superseded | H-78, H-148 | 2 |
| **3** — host-certified | H-30, H-98, H-99, H-101, H-106, H-151 | 6 |
| **5** — original preserved | H-100, H-102 – H-105, H-121, H-149, H-150, H-152 | 9 |
| **6** — evidence gap (non-blocking) | H-51, H-84, H-123, H-128, H-129, H-135, H-136, H-138, H-139, H-140 | 10 |
| **7** — Alpha 3 | H-22, H-45, H-63 | 3 |
| **8** — **release blocker** | **H-12, H-13, H-119, H-146** | **4** |

The partial halves of PASS rows are dispositioned separately: H-15 text (7), H-23 Blackthorn/shrine prompts (2), H-122 (2), H-124 dungeon context (2), H-137 Refuge (6, required in 7E). The other 115 Batch 20 PASS rows stand.

*Note on the Batch 20 tally.* Its Group 10 line (4 PASS / 1 FAIL / 2 UNTESTED) does not match the cells. H-120 and H-121 read UNTESTED, and H-122 has an observation rather than a PASS. The tally is left as recorded; this table supersedes it.

### Phase 7E — *RESERVED for the Batch 53 image* → **written in the Batch 53 section below**

Batch 53 must write the exact steps against its own image, including the `Git` hash gate and the **SD-pack recopy**; Words of Power, ENDMSG and KARMA need new pack sections. Minimum content:

1. Yell FALLAX next to Deceit's entrance: quake, seal opens, ordinary `(E)nter` works (H-119, H-135, H-187).
2. A night moongate is visible and transits. Vas Rel Por shows `To phase:`, and a digit transits (H-188, H-191, H-12, H-13).
3. Buy a skiff or ship and a horse (H-146).
4. Developer Endgame preset → Doom's final room → absorption with the Wooden Box → the victory text appears and the arena closes (H-189).
5. The Camp karma speech is KARMA.DAT text (H-190).
6. Refuge after a party wipe (H-137 Refuge half).
7. Owed observations: a Slime / Gargoyle room in Destard (H-151); the vault floor-change reset and the door closing after a load (H-158, H-161, H-162); Gorn's brazier at the corrected location (H-30).

RC packaging and smoke are **Batch 54**, after 7E passes.

## Batch 53 — release-blocker wiring / data closeout (2026-09-26)

**RB-1 … RB-4: SOFTWARE FIXED — HARDWARE RETEST PENDING (Phase 7E below).** Evidence: `GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 53"; host target `batch53_release_blockers` (95 checks, 22 / 22 mutations killed). Nothing is marked PASS before this phase runs.

| Row | Batch 53 status | 7E step |
|---|---|---|
| H-189 (RB-1) | SOFTWARE FIXED — HARDWARE RETEST PENDING. **Run 1 (Batch 53 image): absorption, `VICTORY!`, ENDMSG, input PASS; post-ending state NOT ACCEPTED (H-192) — see Batch 53A** | A → **A′** |
| H-187 (RB-2), H-119, H-135 | SOFTWARE FIXED — HARDWARE RETEST PENDING | B |
| H-188, H-191 (RB-3), H-12, H-13 | SOFTWARE FIXED — HARDWARE RETEST PENDING. **Run 1: first transit PASS, destination gate drawn, stepping back onto it did not transport the party back (H-195), Vas Rel Por PASS; Test C held — adjudicated by Batch 53B: the original's behaviour, no fix** | C → **C′** |
| H-146 (RB-4) | SOFTWARE FIXED — HARDWARE RETEST PENDING | D |
| H-190, H-137 (Refuge half) | SOFTWARE FIXED — HARDWARE RETEST PENDING | E |
| H-151 | host-certified (B21A); device observation owed | F |
| H-158, H-161, H-162 | host-certified (B23/B24); device observation owed | G |
| H-30 | host-certified (B4.5C.1); device observation owed | H |

### Phase 7E — Batch 53 release-blocker retest · *firmware AND SD pack change*

Every coordinate below was executed on the host through the same Developer call the menu makes (`batch53_release_blockers` section V; the log prints them). `Alt+D` opens Developer; `Alt+M` the System Menu. In Developer → Teleport, set **Use default entrance: Off** before typing X / Y. Floors of a dungeon read "Level N" in that menu; the in-dungeon HUD shows N+1.

**0. Setup and identity gate**
1. On the SD card, replace **only** `/ultima5/openu5-alpha1-resources.bin` with the Batch 53 file: **2,041,466 bytes**, SHA-256 `a48abdbfc88eb5ab43453880a8ea029a1ea0684dfec2045f9dae941b31aa379b`. Do not reformat; keep the saves folder and `openu5-assets.bin`.
2. Flash the Batch 53 Launcher image (path and SHA-256 in tag `alpha2-batch53-release-blockers`).
3. Boot. The identity screen must show `Git` = the first 12 hex digits of the Batch 53 commit (the tag names it), `RES v2.0 2041466B CRC 26f75ae6`, and the game must start. *Optional negative check:* with the old Batch 51 pack on the card the device must **refuse to start** (identity mismatch; the log names `endmsg-records.bin` as missing).
4. Continue a save or start a New Journey. Existing saves stay compatible.

**A. The ending (H-189)** — about 2 minutes

> **Batch 53A:** run 1 on this image passed steps 1–5 (absorption, `VICTORY!`, ENDMSG, input responsive). **Step 6 and the "play continues" note below are superseded**: the original never returns to the dungeon after the ending. Run **7E-A′** (Batch 53A section at the end) instead; B–H below are unchanged.
1. `Alt+D` → Shortcuts → **Preset: Endgame**. Then Party → **Party size 1** (a full party also works, but every member must make the walk in step 5).
2. Teleport → Destination **Doom**, Floor **Level 6**, X **4**, Y **7** → Teleport.
3. Turn with the trackball until the party faces **East**, then move forward once. *Expected:* "Pit Trap!", "Falling...", "...splat!", and the final room's arena opens: a trapped soul above a mirror at the top centre.
4. *Expected:* the arena accepts moves.
5. Move the Avatar **North 4 times**, to the cell straight under the soul. *Expected:* "Avatar is absorbed!"; the arena closes; the ending prints: "Lord British carefully opens the box...", "\"FOLLOW!\" cries Lord British...", the date proclamation, "THE QUEST OF THE AVATAR IS FOREVER" and "Report now, thy Quest compleat in ...".
6. *Expected:* the dungeon view is back; turning left/right responds; `Alt+S` prints "Save complete"; `Alt+M` opens and closes the menu.
- **Pass:** 5 and 6. **Fail:** the arena stays drawn, no ending text, or keys only reach the Developer/save shortcuts.
- *Known, do not file:* the ending is transcript text and play continues — the cinematic (green scene, orb, story pages) is Alpha 3 (D-54).

**B. A Word of Power (H-119, H-135, H-187)** — about 1 minute
1. Teleport → **Britannia**, X **240**, Y **74** (one cell south of Deceit's entrance).
2. Move **North**. *Expected:* nothing happens — the entrance is sealed.
3. `Y` (Yell), type **FALLAX**, Enter. *Expected:* "A word of power is uttered", a screen quake, and **no** "No effect!".
4. Move **North** onto the entrance, press `E`. *Expected:* the party enters Deceit — no Developer tool involved.
5. *Optional:* `Alt+S`, then `Alt+L`, then leave and walk back: the seal stays open. Yelling **VILIS** there instead quakes but prints "No effect!".
- **Pass:** 3 and 4.

**C. Moongate and Vas Rel Por (H-188, H-191, H-12, H-13)** — about 3 minutes

> **Batch 53B:** run 1 passed steps 2, 3, 5 and 6; stepping back onto the destination gate did not take the party back to 96,102 and the test was held. That is the original's behaviour (every gate leads to the stone the moons select, which is the one you just arrived on). The pass criteria are completed by **7E-C′** (Batch 53B section at the end); steps 1–6 below are unchanged.
1. Teleport → **Britannia**, X **96**, Y **103**. Then Time → **Hour 21**.
2. Close Developer. *Expected:* a **moongate** is drawn one cell **North** (96,102). At hour 12 it must be absent.
3. Move **North** onto it. *Expected:* the view jumps to another moongate site (which one depends on the day's Trammel phase). No transit animation yet — that is Alpha 3.
4. Stats → Character: the Avatar → **Level 8**, **Current MP 99**. Inventory → Inventory index **46** → Spell quantity **1** (the row names Vas Rel Por).
5. Press `C` (if asked "Player:", choose the Avatar), pick **Vas Rel Por**. *Expected:* the status line reads **`To phase:`** (not "Aim: empty").
6. Press **3**. *Expected:* the ceremony flash, then the party stands at 38,224 (the Jhelom gate). No "Failed!". Digits map 1 → 224,133, 2 → 96,102, 3 → 38,224, 4 → 50,37, 5 → 166,19, 6 → 104,194, 7 → 23,126, 8 → 187,167 while the stones are still buried in their new-game places.
- **Pass:** 2, 3, 5 and 6.

**D. Buying transport (H-146)** — about 3 minutes
1. Inventory → **Gold 5000**. Time → **Hour 9**.
2. Teleport → **East Britanny**, Floor Ground Floor, X **8**, Y **10** (next to Master Hawkins of The Oaken Oar at 7,10).
3. `T` (Talk) **West**. At "May I help thee?" press `Y`; press `A` (frigate) or `B` (skiff); at the price press `Y`. *Expected:* "She awaits thee at the dock!" and the gold goes down. At "anything else" press `N` until the shop closes.
4. Teleport → **Britannia**, X **79**, Y **109** (the East Britanny dock). *Expected:* the ship is there. Press `B`. *Expected:* the party is aboard.
5. Horse: Time → **Hour 9**; Teleport → **Paws**, Ground Floor, X **7**, Y **18** (north of the horse seller at 7,19). `T` **South**, `Y` at the greeting, `Y` at the price. *Expected:* the gold goes down and a horse appears beside the party. Step onto it, press `B`: mounted.
6. *Control:* answering `N` at the price spends nothing and places nothing.
- **Pass:** 3, 4 and 5.

**E. Refuge after a party wipe (H-137, H-190)** — about 2 minutes
1. Party → **Party size 1**. Shortcuts → **Preset: Low health/status** (the Avatar is poisoned at 1 HP). Inventory → **Karma 50**. Close Developer.
2. Press **Space** (Pass) until the poison kills the Avatar. *Expected:* "An unending darkness engulfs thee...", "Thou hast found refuge.", the apparition, and a **KARMA.DAT** speech — at karma 50 it begins "It is within thee to attain great power, O seeker..." (never "Rest well, Avatar").
3. *Expected:* the party wakes in Lord British's castle, healed.
- **Pass:** 2 and 3.

**F. A Slime / Gargoyle room (H-151)** — about 2 minutes
1. Teleport → **Destard**, Floor **Level 6**, X **3**, Y **2**. Turn to face **North**, move forward. *Expected:* room 9's arena opens with Slimes / Gargoyles.
2. Fight or pass a dozen turns. *Expected:* turns advance, enemies act ("divides!" may appear), `Alt+M` and the Mic key answer. No freeze, no power cycle.
- **Pass:** 2.

**G. Vault reset and door-after-load (H-158, H-161, H-162)** — about 4 minutes
1. Inventory → **Skull keys 3**. Enter Lord British's castle (Teleport → Lord British's Castle, default entrance On) and take the ladder at (1,1) down to the basement.
2. `U`se a skull key on the vault door at **(15,24)**, open it, open the three chests; `G`et a few pieces and leave the rest.
3. Climb up one floor and straight back down. *Expected:* the three chests are back unopened, the loose loot is gone, the door is magically locked again.
4. `O`pen an ordinary basement door (e.g. at (20,16)), then `Alt+S` and `Alt+L`. *Expected:* the door is **closed** after the load; everything else is as saved.
- **Pass:** 3 and 4.

**H. Gorn's brazier (H-30)** — about 1 minute
1. Inventory → **Keys 0**. Teleport → **Palace of Blackthorn**, Floor **Basement**, X **8**, Y **7**.
2. `S`earch **North** (the brazier at 8,6). *Expected:* the keys are found (not "Nothing of note").
3. `G`et **North**. *Expected:* the party now holds **9 keys**.
- **Pass:** 2 and 3.

**Report back:** the `Git` hash seen on the identity screen, then PASS / FAIL per step A–H with any text that differed. RC packaging and smoke are **Batch 54**, only after 7E passes.

## Batch 53A — the state after the final Doom absorption (2026-09-26)

### Phase 7E-A, run 1 — Batch 53 image (`Git 1d7135e9bcb0`, pack `RES v2.0 2041466B CRC 26f75ae6`)

| Step | Result |
|---|---|
| 1–4 setup, pit, arena | PASS |
| 5 absorption ("Avatar is absorbed!") | **PASS** |
| 5 `VICTORY!` | **PASS** |
| 5 ENDMSG ending text | **PASS** |
| input responsive | **PASS** |
| 6 afterwards | the party stands in Doom's enclosed final cell (floor 7, 5,7); movement commands are accepted and go nowhere |

**Test A: NOT ACCEPTED.** Step 6's expectation ("the dungeon view is back; turning responds; `Alt+S` saves") was Batch 53's own assumption, not the original's. Batch 53A read the shipped ENDGAME.OVL: after game-won the original never returns to the dungeon — both endings finish in a loop inside ENDGAME.OVL (`0x04f9`, `0x0ac9`) that accepts no game command (in the GOG build a key quits the program). Adjudication **Outcome B**: the device was missing that terminal state. Evidence: `GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 53A", `re/notes/batch53a-endgame-terminal.md`. Step 6 and the "play continues" note of Test A above are superseded by 7E-A′ below; B–H are unchanged and still to be run.

### Phase 7E-A′ — Test A rerun on the Batch 53A image · *firmware changes, SD pack does not*

**0. Identity gate.** Keep the Batch 53 SD pack (**no recopy**). Flash the Batch 53A Launcher (path and SHA-256 in tag `alpha2-batch53a-ending-terminal`). The identity screen must show `Git` = the first 12 hex digits of the Batch 53A commit and `RES v2.0 2041466B CRC 26f75ae6`.

**Only if the game you continue is the `Alt+S` save made at step 6 of run 1:** it now opens straight into the ending ("The quest is complete. Alt+M: System Menu") — that is correct, not a hang. Before step 1: `Alt+D` → Shortcuts → **Dungeon Test Setup** (Preset: Dungeon; it re-arms the final room), then Teleport → **Britannia**, X **90**, Y **100** (leave Doom so the floor is re-read).

1. `Alt+D` → Shortcuts → **Endgame Test Setup** (Preset: Endgame). Party → **Party size 1**.
2. Teleport → **Doom**, Floor **Level 6**, X **4**, Y **7**, Use default entrance **Off** → Teleport.
3. Face **East**, move forward once: "Pit Trap!", "Falling...", "...splat!", the final arena.
4. **North ×4.** *Expected, unchanged from run 1:* "Avatar is absorbed!", `VICTORY!`, the ending text through "Report now, thy Quest compleat in … to Lord British at Origin Systems!".
5. *Expected, new:* right after it, one System line **"The quest is complete. Alt+M: System Menu"**. The Doom view stays on screen (the cinematic is Alpha 3, D-54).
6. Press forward, turn left/right, Enter, Space, `K`, Mic. *Expected:* **nothing happens** — no step, no turn, no "Blocked!", no new text. This is the original's terminal state, not a wedge.
7. **Shift+Up**, then **Shift+Down**. *Expected:* the transcript scrolls back through the ending and returns.
8. `Alt+S`. *Expected:* **"Save unavailable: the quest is complete"**; no "Save complete". Then `Alt+M` → **Save** → `Alt+M` to close: the transcript shows the same refusal, and step 6 still holds.
9. `Alt+M` → Load / Save Management → **Continue Latest**. *Expected:* "Load complete". A save made before the ending plays normally from the very first key; the run-1 `Alt+S` save (made after the Batch 53 ending) opens into the ending again, which is also correct. *(Return to Title is the other way out — the device's equivalent of the original quitting to DOS.)*

- **Pass:** 4, 5, 6, 8 and 9. **Fail:** any step or turn after the ending, "Save complete" after the ending, or no response to Shift+Up/`Alt+M`.
- *Known, do not file:* two lines reading `victory` after the report (H-194, queued); the ending is transcript text with the Doom view behind it, and the box question is answered for you (D-54 / D-56, Alpha 3).

**Report back:** the `Git` hash, then PASS / FAIL for 7E-A′ steps 4–9. Do not run B–H yet.

## Batch 53B — the moongate return trip (2026-09-26)

### Phase 7E-C, run 1 — as reported

| Step | Result |
|---|---|
| 1 Teleport Britannia 96,103, Time Hour 21 | done |
| 2 a moongate drawn one cell North (96,102) | **PASS** |
| 3 move North onto it: the party is transported | **PASS** (first transit) |
| — at the destination | the moongate is still visibly present |
| — step back onto that visible destination gate | **the party was not transported back** |
| 5 Vas Rel Por: `To phase:` | **PASS** |
| 6 digit `3`: travels correctly; moonstone state and gate visibility correct | **PASS** |

**Test C held** pending adjudication of the return step (H-195).

**Adjudication (Batch 53B): Outcome A — the device is right; no firmware change.** In the original a moongate does not lead back to the gate you came from. Every gate sends the party to the stone of the phase the moons show *now* (Trammel's phase from 20:00, Felucca's after midnight; ULTIMA.EXE `0x4962`–`0x4977`), and during the same phase that is the stone the party just arrived on. Stepping back onto the destination gate therefore transits the party onto itself. On the original the gate closes and re-opens around the party; the device has no transit animation yet (D-48, Alpha 3), so nothing visible happens. Evidence: `GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 53B", `re/notes/batch53b-moongate-return.md`, host target `batch53b_moongate_return` (24 checks; 8 / 8 mutations killed).

### Phase 7E-C′ — the revised Test C · *no reflash, no SD change*

Run it on whatever Batch 53 / 53A image is on the device (53B changed no firmware and no SD pack). Party size 1 is simplest.

1. `Alt+D` → Teleport → **Britannia**, X **96**, Y **103** (Use default entrance **Off**). Time → **Day 5**, **Hour 21**, **Minute 0**.
2. Close Developer. *Expected:* a moongate one cell **North** (96,102).
3. Move **North** onto it. *Expected:* the party stands on another gate at **50,37** — Day 5's Trammel phase is 3. (For another day: Days 1–28 → phase 0 0 1 2 3 4 5 6 7 0 0 1 2 3 4 5 6 7 0 0 1 2 3 4 5 6 7 0; phase 0 → 224,133 · 1 → 96,102 · 2 → 38,224 · 3 → 50,37 · 4 → 166,19 · 5 → 104,194 · 6 → 23,126 · 7 → 187,167, i.e. Vas Rel Por digit phase+1. On a day whose phase is 1 the gate sends the party onto itself.)
4. Step **off** the gate one cell in any walkable direction. *Expected:* an ordinary step; the gate is now drawn on the cell the party left (while the party stands on it, the party sprite covers it).
5. Step **back onto** it. *Expected:* **the party stays on that same gate** — no move to 96,102, no `Failed!`, no prompt. This is correct: at this hour every gate leads to this stone. Repeat 4–5 from a different side: same result.
6. *Expected:* the next move off the gate works normally (no frozen input).
7. **The return trip.** `Alt+D` → Time → **Day 3**, Hour **21**, Minute **0** (Day 3's Trammel phase is 1 = the Britain stone). Close Developer. Step off the gate and back on. *Expected:* the party is at **96,102**, the Britain gate.
8. Steps 4–6 of the original Test C (Vas Rel Por, `To phase:`, digit `3` → 38,224) — already PASS in run 1; repeat only if convenient. *Optional:* from any gate, Vas Rel Por digit **2** returns to 96,102 at once.

- **Pass:** 2, 3, 5, 6 and 7 (plus run 1's Vas Rel Por PASS). **Fail:** step 5 lands anywhere other than the gate the party stands on, any `Failed!` from a gate at hour 21, input refused after a gate, or step 7 not at 96,102. *(If step 7 leaves the party on the 50,37 gate, report it together with the Day shown: that would be a moon-phase latch carried in the save, which the device never refreshes — D-59 — not the gate.)*
- *Known, do not file:* no gate close/open animation, so steps 3 and 5 are instantaneous and step 5 looks like "nothing happened" (D-48, Alpha 3). Standing on a gate and pressing Space does not re-fire it on the device; the original re-fires it (to the same stone at the same hour) — declared divergence D-58, no visible difference here.

**Report back:** the `Git` hash, then PASS / FAIL for 7E-C′ steps 2–7.

## Batch 54 — Phase 7E closeout and the Alpha 2 release candidate (2026-09-26)

### Phase 7E — final reconciliation (user report after Batch 53B)

This table records the results as the user reported them. The run-1 cells above (7E-A run 1, 7E-C run 1) and the stale-image history from earlier phases stay as written.

| Test | Rows | Image | Result | Notes |
|---|---|---|---|---|
| **7E-A′** ending terminal state | H-189 (RB-1), H-192 / D-55, D-46 | Batch 53A (`Git 5fe1ac5335a1`) | **HARDWARE PASS** | Absorption, `VICTORY!` and the ENDMSG text appear. The device then stays in the terminal Ending, and ordinary dungeon controls no longer resume. Save is refused. Loading a pre-ending save leaves the Ending. Run 1 on the Batch 53 image is kept above: its post-ending state was NOT ACCEPTED, and Batch 53A fixed it. |
| **7E-B** Word of Power | H-187 (RB-2), H-119, H-135, D-44 | Batch 53 / 53A | **HARDWARE PASS** | The sealed Deceit entrance blocks. Yelling `FALLAX` opens it with the quake. Ordinary `(E)nter` works afterwards. |
| **7E-C** moongate / Vas Rel Por | H-188, H-191 (RB-3), H-12, H-13, D-45, D-48 (gate), D-53 | Batch 53 / 53A | **HARDWARE PASS (after the Batch 53B adjudication)** | Run 1 passed four checks: the gate is drawn at the night moonstone, the first transit works, Vas Rel Por shows `To phase:`, and phase travel works. The "no return through the destination gate" observation (H-195) is the original's behaviour, not a defect (Batch 53B, Outcome A). The user accepted C on that basis. The revised 7E-C′ steps 4–7 were not reported separately. |
| **7E-D** transport shop | H-146 (RB-4), D-49 | Batch 53 / 53A | **HARDWARE PASS** | Ship/skiff purchase deducts the gold, places the vessel, and boarding works. Horse purchase and mounting work. |
| **7E-E** Refuge | H-137 (Refuge half), H-190, D-47 | — | **ACCEPTED — previously witnessed** | The user has seen the party-wipe Refuge path return the party to Lord British's castle. That the speech comes from KARMA.DAT is host-certified (`batch53_release_blockers` B2/K). No hardware run was forced just to prove where the string comes from. |
| **7E-F** Destard Slime / Gargoyle room | H-151 | Batch 53 / 53A | **HARDWARE PASS** | The authored room opens, turns continue, and controls respond. No actor-storage freeze. |
| **7E-G** vault reset / door after load | H-158, H-159, H-161, H-162, H-148, D-21, D-22 | earlier images | **ACCEPTED — previously physically confirmed** | Not rerun: nothing after Batch 24 changed that subsystem. |
| **7E-H** Gorn's brazier keys | H-30 | earlier images | **ACCEPTED — previously physically confirmed** | Not rerun: nothing after Batch 4.5C.1 changed that path. |

**Phase 7E: COMPLETE.** RB-1 … RB-4 are **HARDWARE PASS**. No Phase 7E row is still pending.

The following history is preserved, not rewritten:
- The stale-firmware runs: Batch 50's device ran the Batch 43 image, and 7B/7C run 1 are INVALID.
- The Batch 53A correction: the original never leaves ENDGAME.OVL after game-won, so "play continues" was wrong.
- The Batch 53B moongate adjudication: a gate leads to the current phase's stone, and there is no return link.

### Phase 8 — Alpha 2 RC1 hardware smoke · *new firmware; SD pack unchanged* · about 10–15 minutes · *PENDING* → **PASS (Batch 55)**

This smoke test looks for catastrophic regressions only; it is not another validation campaign. Compared with the Batch 53A image that passed Phase 7E, the RC1 image changes only the version string and the embedded `Git` hash. The annotated tag `alpha2-batch54-rc1` gives its path, SHA-256 and `Git` hash. `Alt+D` opens Developer and `Alt+M` the System Menu. A short Mic press is Cancel.

**1. Boot / identity (1 min)**
1. **Do not recopy the SD pack.** `/ultima5/openu5-alpha1-resources.bin` stays the Batch 53 file (2,041,466 B). `/ultima5/openu5-assets.bin` and `/ultima5/saves/` stay as they are.
2. In Launcher, install `OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`.
3. Boot. The identity screen is held for about 2 s, so a photo helps. It must read:
   - **`FW 2.0.0-alpha2-rc1-debug`**
   - **`Git <first 12 hex of the tag's commit>`**
   - **`RES v2.0 2041466B CRC 26f75ae6`**
   - **`ASSET … 132284B CRC 933c9b82`**

   The title screen must follow. A resource-mismatch refusal is a FAIL: report it with the photo.

**2. Basic input (2 min)** — Journey Onward → **Continue Latest** (or start a New Journey).
1. Move a few cells with the trackball. `L`ook in a direction. `T`alk to someone nearby, type `NAME`, then `BYE`. Open `Z` stats, then press Mic to back out. *Expected:* typed text never moves the party.
2. Hold Mic for about a second: movement mode toggles. Hold it again to toggle it back.
3. `Alt+M` opens the System Menu; Mic or `Alt+M` closes it. `Alt+D` opens Developer; Mic backs out to play.

**3. Save / load (1 min)**
1. Note the position, then press `Alt+S`. *Expected:* "Save complete".
2. Walk 5 or more cells away.
3. Press `Alt+L`. *Expected:* "Load complete", the party is back at the saved cell, and the next key moves normally.

**4. Power-cycle Continue (1 min)**
1. Switch the device off and on. The identity screen must read as in step 1.
2. Journey Onward → **Continue Latest**. *Expected:* the step-3 save loads at the same position, and input works.

**5. Dungeon (2 min)**
1. `Alt+D` → Shortcuts → **Preset: Dungeon**. Teleport → **Britannia**, X **240**, Y **74**, Use default entrance **Off**. Close Developer, move **North** onto Deceit's entrance, and press `E`.
2. Move and turn several times in the 3D view. *Expected:* the view and the HUD's level readout stay correct.
3. Press `Alt+L` to load the step-3 save; loading out of a dungeon is the check. *Expected:* the surface appears and the next key moves normally.

**6. Combat (2–3 min)**
1. `Alt+D` → Shortcuts → **Preset: Combat** (maxed party, best gear). Walk the Britannia wilderness until an encounter opens an arena. For a deterministic route, use the 7E-F Destard room instead: Teleport → **Destard**, Level **6**, X **3**, Y **2**, face **North**, then forward.
2. Take several turns: move, attack, and let each member act.
3. End the fight normally: win it, or leave an outdoor arena by the edge. *Expected:* the map returns and the next key plays. If the Destard fight drags on, press `Alt+L` to get out, and note that you did.

**7. Rest (1 min)**
1. Outdoors on foot, press `H` (Hole up / Camp), choose a few hours, press Enter, and post no watch. *Expected:* the camp scene draws, the clock advances, and the party wakes. The status panel and map come back, and the next key plays.
2. If the apparition appears, it counts as step 8's scene: let it run, and answer the Hail and the karma speech.

**8. Scripted scene (1–2 min)** — the Refuge (the 7E-E route, deterministic)
1. Press `Alt+S` first. Then `Alt+D` → Party → **Party size 1**, then Shortcuts → **Preset: Low health/status**, and close Developer.
2. Press Space (Pass) until the poison kills the Avatar. *Expected:*
   - "An unending darkness engulfs thee..." and "Thou hast found refuge." appear.
   - The apparition and a karma speech follow, paced rather than all at once.
   - The party wakes in Lord British's castle, healed, and input works.
3. To get the full party back, press `Alt+L` to load the save from step 8.1.

**9. Ending mode — not run.** `batch53a_ending_terminal` (44/44) certifies it on the RC tree. Batch 54 changes no ending code, and 7E-A′ passed on the device.

**10. Transport / moongate — optional.** Run it only if convenient: 7E-C and 7E-D passed, and Batch 54 changes neither.

- **PASS requires every step above, and all of these:**
  - no crash, reset or watchdog;
  - no lock: every screen answers the trackball, `Alt+M` or Mic;
  - no stale-resource refusal;
  - no input-mode corruption: typing never moves the party, and movement never types;
  - Save, Load and Continue restore the saved state;
  - the dungeon, combat and rest loops return to play.
- **FAIL:** report the step, what the screen showed, the `Git` line and, if you have it, `/ultima5/logs/` from the SD card.
- *Known, do not file:* the "Known issues" list in `ALPHA2.md` at the repository root. It covers no audio, no moongate or ending animation, the two `victory` lines after the ending text (H-194), D-58 / D-59, and the Alpha 3 scene-fidelity rows H-183 – H-186.

**Report back:** the `FW` and `Git` lines, then PASS / FAIL for steps 1–8.

## Batch 55 — Phase 8 result and the Alpha 2 release (2026-09-26)

### Phase 8 — Alpha 2 RC1 hardware smoke · **PASS**

The user ran the complete Phase 8 smoke above on the device, on the RC1 image, and reported every step PASS. The "HARDWARE SMOKE PENDING" wording in the Batch 54 section is superseded by this result; it stays as written.

**Boot identity, as read on the device:**
- `FW 2.0.0-alpha2-rc1-debug`
- `Git 211c676a1dca` (the `alpha2-batch54-rc1` commit)
- `RES v2.0 2041466B CRC 26f75ae6`
- `ASSET … 132284B CRC 933c9b82`

The packs matched the firmware. The image is `OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`, SHA-256 `ff3dfe193973547db2648c5f486aa381086505eb80b5f2dadec7432194fb5828`.

| Step | Covers | Result |
|---|---|---|
| 1 Boot / identity | FW, Git, RES and ASSET lines; packs match | **PASS** |
| 2 Basic input | movement, Look, Talk, Z-stats, Mic, movement-mode toggle, System Menu, Developer | **PASS** |
| 3 Save / `Alt+L` | save, walk away, load; exact saved state; input normal afterwards | **PASS** |
| 4 Power-cycle Continue | full power off/on, Continue Latest, saved state, input normal | **PASS** |
| 5 Dungeon | Deceit entry, dungeon turns, load back out; no mode or input corruption | **PASS** |
| 6 Combat | encounter, several combat turns, normal exit, control restored | **PASS** |
| 7 Rest / Camp | camp scene, clock advance, return to play | **PASS** |
| 8 Scripted scene | party-wipe route, the Refuge scene, waking in Lord British's castle, input afterwards | **PASS** |

The PASS criteria all held: no crash, reset or watchdog; no lock; no stale-resource refusal; no mixed input mode; Save, Load and Continue restored the saved state; the dungeon, combat, rest and scripted-scene loops returned to play. Steps 9 (ending) and 10 (transport / moongate) were not run, as the smoke specified; Phase 7E and the host suite cover them.

**No hardware regression was found. Alpha 2 hardware validation is COMPLETE.**

### Alpha 2 released

The tested RC1 image is promoted **byte for byte** to the final Alpha 2 release (tag `alpha2-batch55-release`; see `GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Batch 55"). No new firmware was built, so no further flash and no SD change are needed. A device that passed Phase 8 is already running Alpha 2 final.

No hardware phase remains open for Alpha 2. The next physical checks belong to Alpha 3 work.

## Alpha 3 A3-HF1 — the troll-encounter reward chest (2026-09-26)

### Observation that opened it (Alpha 3 A3-04B image)

A random troll encounter by the bridge near Britain, won normally. A chest appeared in the arena and opened, but every `Get` toward it in the expected adjacent direction answered **"Nothing to get!"**. The user reported it was not the scripted TrollSneak scene. Not audio-related.

**Host finding** (`GAMEPLAY_INTEGRATION_AUDIT.md` §14 "Alpha 3 A3-HF1"): the loot was on the chest's cell and the Get found it, but refused it because its counter was already full (99, or 9999 for gold and food). The 1988 Get has no such refusal: it takes the item, names it, and the full counter stays full. The Developer "Stocked inventory" and "Combat" presets fill every counter a chest can hold, so with either one every item was refused. Because the Get takes the top of the stack, one full counter blocked the whole pile.

### Phase H-197 — arena chest Get with a full pack · *new firmware; SD pack unchanged* · about 10 minutes · **PASS (2026-09-27, physical T-Deck; recorded in A3-HF2.1)**

Flash the A3-HF1 Launcher image (its path and SHA-256 are in tag `alpha3-hf1-arena-loot`). The boot screen should show `FW 3.0.0-alpha3-dev-a3-hf1-debug` and that tag's `Git`.

1. **Fill the pack.** `Alt+D` → Shortcuts / Presets → **Preset: Stocked inventory** → confirm. This leaves Dexterity alone, so the trolls can still catch the party. *Optional:* Resources → Gold → `9000`, so the gold pickup shows a visible change.
2. **Go to the bridge.** `Alt+D` → Teleport → Destination Britannia, X `76`, Y `120` → Teleport. This is the grass just south of the bridge at (76,119), south-west of Britain.
3. **Meet trolls.** Walk north onto the bridge and back south, over and over. About one crossing in eight wakes the trolls. If someone fails to sneak across, **"Pay toll?"** appears: press **N**.
   - A maxed party (DEX 30, from Maxed party / Combat / Full Test Setup) is never caught. In that case walk the overworld until any monster attacks instead. Every monster's chest goes through the same Get.
4. **Win the fight.** A kill leaves a chest about half the time. If none appears, leave with Back and repeat step 3.
5. **Open it.** Stand next to the chest with the member whose turn it is and press **O** + the direction. You should see "Found:" and a list.
6. **Get the loot.** Press **G** + the same direction, repeatedly. The turn passes after each command, so in a larger party aim from the member whose turn it is, and they must be next to the chest.
7. **Check the pack.** `Z` → Provisions: full counters are still 99 / 9999. With the optional 9000 gold, the gold has gone up by the amount the Get named.

**PASS:**
- Each Get names one item ("38 gold!", "1 key!", a weapon's name…), and the next item shows on the cell.
- **No "Nothing to get!" while loot lies on the cell.**
- After the last item the cell is empty. One more Get there now answers "Nothing to get!", which is correct.
- Leaving with Back afterwards returns to the overworld normally.

**FAIL:** report the step, what the screen showed, the `FW` / `Git` lines, and the `COMBAT_GET_REQUEST` / `COMBAT_GET_CANDIDATE` / `COMBAT_GET_RESULT` lines from `/ultima5/logs/` if you have them. **If it fails with an ordinary, not-maxed pack, that is a different defect. Say so.**

*Known, do not file:* a save taken inside the arena loads back to the overworld without the arena. That was already true before this hotfix and is not this report.

## Alpha 3 A3-HF2 — ambient SFX parity: the grandfather clock (2026-09-27)

### Observation that opened it (Alpha 3 A3-04E.1 image)

Beside a grandfather clock, every move was followed by a short strike-like beep, and then the normal tick-tock. Separately, and unconfirmed: on an earlier image a fountain seemed to burble only around movement. In the latest tests it sounded normal.

**Host finding** (`ALPHA3_AUDIO.md` §24): the 1988 clock strikes only when the game hour changes (`advance_clock` 0x514a). The device re-armed the strike on every minute, so on every step. Fixed. The fountain was not reproduced: standing still, it burbles continuously in the host model of the device loop. No change.

### Phase H-198 — grandfather clock and fountain · *new firmware; SD pack unchanged* · about 5 minutes · **PASS (2026-09-27, physical T-Deck; recorded in A3-HF2.1)**

Flash the A3-HF2 Launcher image (its path and SHA-256 are in tag `alpha3-hf2-ambient-clock`). The boot screen should show `FW 3.0.0-alpha3-dev-a3-hf2-debug` and that tag's `Git`. Keep SFX Volume above 0 %.

1. **Stand near a clock.** `Alt+D` → Teleport → Small map, location 2, X `13`, Y `2`. Stand still for 5 s: tick, tock (about two a second), with no strike.
2. **Move several steps** near it, pausing between them. **No strike-like beep after any move**; the tick-tock just continues.
3. **Stand still again** for 5 s: the tick-tock plays normally.
4. **A real strike, if feasible.** Keep stepping beside the clock until the hour on the status line turns (each step is one minute). At the turn the clock strikes that hour on a 12-hour dial (three at 15:00, twelve at noon), then ticks again. The following steps strike nothing.
5. **Fountain, standing still.** Teleport → Small map, location 1, X `6`, Y `25`. Stand still for 20–30 s without touching anything, and note whether the soft fast burble continues the whole time.

**PASS:** steps 1–3 as described, a strike only at an hour change (step 4, if done), and the fountain continuous while standing still.

**FAIL:** report the step, the time shown and the `FW` / `Git` lines. A strike after an ordinary step is a failure. A fountain that goes quiet while standing still is a new observation: say after how long, and whether the Developer SD diag log was on.

## Alpha 3 A3-HF2.1 — cleanup: `PRESENTATION_DISPATCH` on change only (2026-09-27)

H-197 and H-198 above both **PASSED** on the physical T-Deck (2026-09-27). A3-HF2.1 changes only when one serial log line is written (and the TypeScript skin, which is not on the device). `ALPHA3_AUDIO.md` §25.

### Phase H-199 — serial log check · *new firmware; SD pack unchanged* · about 5 minutes · **OPTIONAL, PENDING**

Flash the A3-HF2.1 Launcher image (its path and SHA-256 are in tag `alpha3-hf2-1-cleanup`). The boot screen should show `FW 3.0.0-alpha3-dev-a3-hf2-1-debug` and that tag's `Git`. Attach a serial monitor.

1. **Stand still** in the overworld near water for 30 s. **No `PRESENTATION_DISPATCH` line** appears after the first one for that screen.
2. **Walk** 10 steps. No `PRESENTATION_DISPATCH` lines.
3. **Open Z-stats and close it.** One `PRESENTATION_DISPATCH ui=party …` line, then one `ui=explore …` line.
4. **A fight, if one comes.** Entering it logs one `… source=combat` line; the fight itself logs none; leaving it logs one `source=world` line.
5. **The clock** (location 2, X 13, Y 2): as in H-198 — no strike after steps inside the hour.

**PASS:** the line appears only at a change of screen, mode or source; the game and its sounds behave as on the A3-HF2 image. **FAIL:** a `PRESENTATION_DISPATCH` line repeated with no change between, a change with no line, or any gameplay / audio difference from the A3-HF2 image — report it with the `FW` / `Git` lines.

## Alpha 3 A3-04F — render / TFT efficiency (2026-09-27)

A3-04F changes how the Board sends pixels (whole rows share one SPI transaction; adjacent animated cells are one window; frame lines are one or two transactions, not one per row), redraws a party / status / transcript row only when its text, colour or reverse video changed, and computes the viewport checksum from a table instead of bit by bit. On the host the panel is pixel-identical to the A3-HF2.1 Board after every one of 2,766 render calls (`ALPHA3_AUDIO.md` §26). Nothing the game does, plays or saves changes.

### Phase H-200 — render correctness and speed · *new firmware; SD pack unchanged* · about 15 minutes · **PASS (2026-09-27, physical T-Deck; recorded in the A3-04F hardware closeout)**

Flash the A3-04F Launcher image (its path and SHA-256 are in tag `alpha3-a3-04f-render-efficiency`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-04f-debug` and that tag's `Git`; if not, stop. Use the music-patched assets, Music 80 %, SFX 80 %, *Probe: SD diag logging* off (the boot default). Serial is optional; if attached, search the capture for `task_wdt` afterwards.

1. **Start a window.** `Alt+D` › Diagnostics › *Audio/render stats (live)* (two up) › Enter › Enter › Back › Back. Leaving the menu repaints the whole screen: both frames, all party rows, location, clock, the transcript — nothing missing.
2. **The coast.** `Alt+D` › Teleport › Britannia X `111`, Y `22` (grass with water on most of the screen). Stand 30 s. Every water cell animates; no cell frozen, misplaced, striped or half-drawn; the top (sky) and bottom (wind) strips unharmed.
3. **Walk 60 s** along the coast and inland, in straight lines and in zig-zags. The map moves one tile per step with no missing rows or tiles. The transcript scrolls correctly: no stale, duplicated or missing line, repeated lines ("West", "West", …) included. The clock advances.
4. **Pass 15 times** (Space): 15 `Pass` lines scroll up; the newest is at the bottom.
5. **Z open / close ×3, Developer menu open / close ×3.** Each close redraws the whole right panel: party rows, location, clock, transcript and frame lines complete, no ghost of the menu.
6. **A fight, if one comes:** a hit member's row flashes in reverse video and returns to normal; HP numbers update; entering and leaving the arena repaint cleanly.
7. **Read the window:** `Alt+D` › Diagnostics › *Audio/render stats (live)* › Enter. Photograph every page (or keep the `A3C_PERF` / `A3E_PACE` lines).

Reference, A3-04E.1 image (11-minute soak, `ALPHA3_AUDIO.md` §23.10): compose avg **38.5 ms**, tiles max 37.4 ms, full-screen TFT max 148.8 ms, viewport-frame TFT avg 51.9–55.2 ms, animation-frame (`oth`) TFT avg 12.9–17.7 ms, walking step median 87.4 ms (serial `render … us`), `idle0 gap` max 102.9 ms, `forced=0`, `und=0 hw=0 miss=0`.

**PASS:** steps 1–6 as described (no missing, stale, ghosted or misplaced pixels anywhere); compose avg **≤ 26 ms**; full-screen TFT max below 148.8 ms; `und=0 hw=0 miss=0` (0–1 at a song switch), music and SFX continuous and on time; `idle0 gap` max < 250 ms; no `task_wdt`, crash or reboot.

**FAIL:** any visual defect (photograph it, say which screen and what you did just before), compose avg above 26 ms, underruns, a watchdog line or a reboot. Report it with the `FW` / `Git` lines and the report pages.

**Result (2026-09-27): PASS.** Image `FW 3.0.0-alpha3-dev-a3-04f-debug`, `Git dcea95390676`. The evidence is the live report's final window (213.7 s, 3,630 frames, Music 80 %), transcribed in `a3-04f-hw-h200-report.log`. No serial capture. `ALPHA3_AUDIO.md` §26.17.
- Steps 1–5 as described: no map corruption, no frozen or half-drawn water, no stale, duplicated or missing transcript line, menus closed cleanly, no ghosting. No crash, reboot or watchdog; music and SFX normal.
- Compose avg **10.1 ms** (was 38.5; −73.8 %). Tiles max 9.9 ms (was 37.4; −73.5 %).
- Full-screen TFT max **120.1 ms** (was 148.8; −19.3 %). Viewport-frame TFT avg 33.5 ms (was 51.9; −35.5 %). Animation-frame TFT avg 7.6 ms (was 12.9–17.7; −41 to −57 %).
- `idle0` gap max **37.6 ms** (was 102.9; −63.5 %), `forced=0`. `und=0 hw=0 miss=0`.
- Step 6: the fight worked, HP updated, and the arena repainted cleanly. **Its reverse-video clause does not apply.** The device has never flashed a hit member's row: the combat hit was never wired to the roster inversion, before or after A3-04F. The step restated an intent written in code comments. That clause is now **H-201 / D-63** below, a parity defect. It is not an H-200 failure, and A3-04F did not cause it.
- Internal heap minimum 2,200 B (was 200–340 B): a watch item, not closed (`ALPHA3_AUDIO.md` §26.17.8).

## Alpha 3 A3-HF3 — combat hit feedback (D-63) (2026-09-27)

**Observation (H-200, A3-04F image):** in combat, when a party member took damage, it was not obvious which member was hit. There was no name flash or other cue. The only signs were the red "`<name>` hit!" line, the HP number and the hit sound.

**What the original does** (ULTIMA.EXE 0x3564, `ALPHA3_AUDIO.md` §27.2): every hit draws a star (tile 0) over the struck combatant's cell. If a party member was hit, its roster row also goes into reverse video. Both last as long as the hit's noise burst, about 174 ms, then the screen is restored. The cue comes before the result ("hit!", "killed!"). A3-HF3 does the same, without holding the game.

### Phase H-201 — combat hit feedback · *new firmware; SD pack unchanged* · about 10 minutes · **PASS (2026-09-27)**

Flash the A3-HF3 Launcher image (its path and SHA-256 are in tag `alpha3-hf3-combat-hit-feedback`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf3-debug` and that tag's `Git`; if not, stop. SFX 80 %, any music setting.

1. **Get into a fight** with at least two members in the party. A roaming monster is enough. The trolls by the bridge are reliable: `Alt+D` → Teleport → Britannia X `76`, Y `120`, then walk north onto the bridge and back until trolls appear (as in H-197 step 3; press **N** at "Pay toll?"). Do not use a maxed party (DEX 30): nothing catches it.
2. **Let a member be hit.** Pass (Space) with a member standing next to a monster. When a monster hits, watch the right panel. **Only the struck member's row** turns to reverse video (filled bar, dark letters), for about a fifth of a second, and then returns to normal. At the same moment a **star** (red outline, yellow body, white centre) covers that member's cell in the arena, and the hit sound plays. Then the "`<name>` hit!" line and the new HP stay.
3. **Hit a monster.** Attack an adjacent monster (`A` + direction). When the blow lands, the star covers **the monster's** cell for about a fifth of a second. **No party row** changes. A miss shows nothing.
4. **Several hits.** Fight on until two different members are hit in the same round, or one member twice. Each hit shows its own flash on the right row, one after the other. A second hit on the same row shows as two separate flashes.
5. **Death (if it happens).** A killing blow on a member still flashes that row and cell first; then the row shows the member dead (`D`), not inverted.
6. **After the fight.** Leave the arena as usual. No row stays inverted, no star is left on the map, and the game moves normally.

**PASS:** steps 2–6 as described. The flashes are clearly visible and on the right row and cell. The hit sounds are the same as on the A3-04F image, and music does not stutter. No `task_wdt`, crash, reboot or visual corruption.

**FAIL:** a flash on the wrong row or cell, a row left inverted, no flash at all, a flash with no hit, or anything from the PASS list missing. Report it with the `FW` / `Git` lines and, if possible, a photo or video of the moment.

*Known, not this check:* the star is drawn on the **cell**, so for a fraction of a second the struck combatant is hidden under it; that is the original's opaque blit. A monster's death still plays its own sound after the hit (A3-03's `CombatDefeat`, recorded for review in `ALPHA3_AUDIO.md` §27.12).

**Result (2026-09-27): PASS. D-63 is hardware-validated and closed.**
- **Image:** the A3-04G image, which carries A3-HF3's combat code unchanged: `FW 3.0.0-alpha3-dev-a3-04g-debug`, `Git c8fee48beda2`.
- **Evidence:** one serial capture (`a3-04g-hw-h201-h202.log`) summarised by `native/core/tools/a3_04g_hw_closeout.py` (`a3-04g-hw-summary.log`), and the user's own look at the screen. Full write-up: `ALPHA3_AUDIO.md` §28.21.2.
- **Steps 1–4:** one fight with two trolls at the bridge and three party members.
  - 8 cues: 5 on trolls (`row=-1`), 3 on party members (`row=0` once, `row=2` twice), every party row equal to the struck member's roster slot.
  - Each cue drew exactly two frames, the star / row and then the restore. The restore frame was logged 200–220 ms after the cue.
  - 3 of 8 player attacks drew no cue (no miss line in the log, so consistent with misses).
  - The user saw the right row and cell each time.
- **Killing blow:** `COMBAT_HIT_CUE target=5 cell=4,2 row=-1`, then `ENEMY_ID phase=death actor=5` on the same tick.
- **Step 6:** `COMBAT_END reason=victory/all-hostiles-gone`, `normal-victory-exit`, the bridge tile restored unchanged, `UI_MODE from=combat to=explore`. No row stayed inverted, and walking resumed.
- **Audio:** every window in the fight `missed=0 underruns=0 hw_underruns=0`. No `task_wdt`, crash or reboot.
- **Not exercised** (caveats, still host-proven): step 5, because no member died; and two cues queued at once (every cue `queued=1`).
- **Unrelated:** one keyboard read error during targeting (`ESP_ERR_INVALID_RESPONSE`), recovered in 40 ms with a resync; 1 in 5,720 reads.

### Phase H-202 — System Menu save inspection and the storage heap · *new firmware; SD pack unchanged* · about 20 minutes · **PASS (2026-09-27); heap watch item OPEN**

Flash the A3-04G Launcher image (its path and SHA-256 are in tag `alpha3-a3-04g-storage-inspect`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-04g-debug` and that tag's `Git`; if not, stop. Music 80 %, SFX 80 %, SD diag logging off (the default). **Keep a serial monitor attached from power-on to the end** and send the whole capture: this check is read from the `SAVE_INSPECT`, `SD_HEAP`, `METRICS heartbeat` and `SYS_PERF` lines (`ALPHA3_AUDIO.md` §28). The card should hold two save generations; if it holds fewer, make them in step 3 with two saves.

The A3-04G image contains A3-HF3's combat code unchanged, so H-201 can be run on this same flash; if so, record this image's `FW` / `Git` in H-201's result.

1. **Boot to the title screen.** The two generations are listed correctly. Serial: one `SAVE_INSPECT slot0=verified slot1=verified …` line (this first look reads and checks both), and the `SD_HEAP operation=dma-reserve-restored state=save-inspect` line after it. Note that line's `free_internal` and `largest_internal`: the internal heap should be back near its pre-window value (the previous image showed 46,571 B free with a 7,552 B largest block here).
2. **Continue.** The game loads. Note the `load generation=… time=… ms` line and the `SD_HEAP … state=load-latest` line after it.
3. **Walk for about 30 s**, then note one heartbeat: `METRICS heartbeat internal=…` (call it *B*). If the card had fewer than two generations, press `Alt+S` twice now.
4. **First System Menu open (`Alt+M`).** Judge by eye how long the menu takes to appear; the previous image took ~0.8 s. Serial: `SAVE_INSPECT slot0=cached slot1=cached bytes=64 … total_us=…` (both `cached` if nothing changed since the title), no `INPUT_SERVICE render_block_us` line near 720,000, and `SYSTEM_MENU_RENDER … us=…` (about 100 ms, unchanged). The `dma-reserve-restored` line shows `free_internal` ≈ *B* and `largest_internal` 49,152 (the previous image showed ≈ 77 KB and 23,552 after this first open).
5. **The Load page.** `Down`, `Down`, `Enter`: *Generation 1* / *Generation 2* name the right characters, and neither says `empty` or `corrupt` wrongly. Back out and close with **Mic**.
6. **Twenty open / close cycles** (`Alt+M`, then **Mic**). Every open is quick; every `SAVE_INSPECT` line says `cached`. The heartbeats before and after stay at *B* (± a few bytes; no step down, no slope).
7. **Save from inside the menu.** `Alt+M`, `Down`, `Enter` (Save): "Save complete". Without closing, `Down`, `Enter` (Load / Save Management): the page lists the generation just written (the previous image listed the card as it was when the menu opened). Serial: the `save generation=… time=… ms` line, then `SAVE_INSPECT … cached … cached`.
8. **Load a generation.** On that page choose a generation and `Enter`: "Load complete", and the game is the one saved.
9. **Shortcuts.** Walk a little; `Alt+S` ("Save complete"); walk; `Alt+L` ("Load complete", back where you saved). Then one more `Alt+M` / Mic: `cached`, `cached`.
10. **Music and SFX** keep playing through all of the above; the Developer report (Alt+D → Diagnostics → Audio/render stats) shows `und=0 hw=0 miss=0`.
11. **Heap summary.** In the same report note internal heap now / min and PSRAM now / min. The **min** may still read a few hundred bytes to a few KB: a first look at the card (step 1), a load and a save each build a large document for a moment (§28.16). That alone is not a failure.
12. **Search the capture** for `allocate_dma_buf`, `dma-reserve-restore-failed`, `LOW INTERNAL RAM`, `SAVE_SCRATCH allocation failed`, `sdmmc`, `diskio`, `task_wdt`, `Guru`, `abort`.

**PASS:** steps 1–10 as described; every `dma-reserve-restored` line after a menu open shows `free_internal` within 4 KiB of the `release-reserved-dma` line before it and `largest_internal` of at least 32 KiB; the heartbeats do not fall across step 6; step 12 finds nothing; no crash, reboot, watchdog, or visual corruption.

**FAIL:** a wrong, stale, `empty` or `corrupt` entry for a good generation; an open that is not clearly faster than before; `free_internal` staying ≈ 150 KB lower after a menu open; any line from step 12; a save or load that fails. Send the capture with the `FW` / `Git` lines.

*Record for §28 in any case:* the `SAVE_INSPECT` lines of steps 1, 4 and 6 (`commit_us`, `read_us`, `verify_us`, `total_us`), every `SD_HEAP … dma-reserve-restored` line, the load time of step 2, and the step-11 figures. They replace §28.4's inferred split with a measured one.

**Result (2026-09-27): PASS for the menu's responsiveness, the save list, repeated-open stability and save / load. The heap watch item stays OPEN.**
- **Image:** `FW 3.0.0-alpha3-dev-a3-04g-debug`, `Git c8fee48beda2`.
- **Evidence:** one serial capture from the resource check (31.3 s) to 258.0 s (`a3-04g-hw-h201-h202.log`; the boot lines before 31.3 s are missing), summarised in `a3-04g-hw-summary.log`. Music 90 %, SFX 30 %, SD diag logging off. Full write-up: `ALPHA3_AUDIO.md` §28.21.
- **Step 1:** `SAVE_INSPECT slot0=verified slot1=verified bytes=14534 commit_us=81246 read_us=363022 verify_us=328111 total_us=785608`. The window restored 74,363 B, its own release value, with a 58,368 B largest block. The previous image showed 46,571 / 7,552 B here.
- **Step 2:** `load generation=32 slot=0 time=1051 ms status=0`.
- **Step 3:** *B* = 243,503 B, and 243,255 B just before the first open.
- **Step 4:**
  - `SAVE_INSPECT slot0=cached slot1=cached bytes=64 commit_us=81875 read_us=0 verify_us=0 total_us=83132`.
  - Input blocked 85 ms, where it was 724 ms.
  - Menu frame 99.5 ms.
  - Key → menu frame 180 ms, where it was 820 ms.
  - Restored 243,255 B (= *B*), largest 51,200 B.
- **Step 5:** from the user's report; the log shows only that the menu actions were accepted.
- **Step 6:** 25 opens in all, 23 of them in a row. Every one `cached / cached`, 82.8–83.2 ms, key → frame 180 ms. Heartbeats flat at 93,831 B; every window kept 0 B.
- **Step 7:**
  - `save generation=33 slot=1 time=2106 ms`, every stage `ok`.
  - The next `SAVE_INSPECT` was `cached / cached`: the save stored its own slot.
  - The page's contents are from the user's report.
- **Step 8:** `load generation=33 slot=1 time=843 ms status=0`. A later Quit → Continue loaded generation 33 too (815 ms).
- **Step 9:** not exercised (no Alt+S / Alt+L outside the menu in the capture).
- **Step 10:** 41 / 41 audio windows `missed=0 underruns=0 hw_underruns=0`, across 205.7 s.
- **Step 11:**
  - `SYS_PERF` at the end: internal 139,295 B, `heap_int_min` 316 B, PSRAM 6,009,164 B.
  - The minimum was set inside storage intervals (the boot windows; the in-menu Save).
- **Step 12:** nothing. No `task_wdt`, crash, reboot or error-level line.
- **PASS criteria:**
  - Every `dma-reserve-restored` line after a menu open is within 4 KiB of its release value: **met, 0 B on all 29**.
  - Largest internal block ≥ 32 KiB: **not met on 23 opens (23,552 B)**. The Continue before them set it (51,200 → 23,552 B), and no open changed it.
  - The heartbeats did not fall across step 6. Step 12 found nothing, and there was no crash.
- **Heap (the reason the watch item stays open):**
  - Both Continues after boot moved the live document into internal RAM: −149,424 and −47,192 B, with free PSRAM rising by the same amounts ± 28 B. This fires §28.16 triggers 2 and 3.
  - It is **placement, not a leak**: total free memory ended 41,140 B higher than at the start, and later windows raised the largest block again (32,768 → 47,104 → 36,864 B).
  - Classified as an **allocator placement / fragmentation watch — recoverable, not a leak** (`ALPHA3_AUDIO.md` §28.21.7).

## Alpha 3 A3-05 — audio finalization: session mutes and the kill burst (2026-09-27)

**What changed** (`ALPHA3_AUDIO.md` §29):
- **Alt+Shift+M** toggles music off and on, and **Alt+Shift+S** does the same for sound effects. A mute lasts for the session only. It never changes the volume set in Settings, and a reboot starts unmuted.
- A kill in combat now plays **one** hit sound, as the original does. The second burst it used to play came from the chest-trap routine.
- The volume curves are unchanged: the user already judged them good on the device.

### Phase H-203 — mute shortcuts and the kill sound · *new firmware; SD pack unchanged* · about 5 minutes · **PENDING**

Flash the A3-05 Launcher image (its path and SHA-256 are in tag `alpha3-a3-05-audio-finalization`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-05-debug` and that tag's `Git`; if they differ, stop. Use the music-patched card. No serial capture is needed.

1. **Set odd volumes.** `Alt+M` → Settings: Music Volume **70%**, SFX Volume **40%**. Close the menu, then walk a few steps to hear both.
2. **`Alt+Shift+M`.** The music stops, and the transcript says `Music muted.`. Footsteps and other effects still sound.
3. **`Alt+Shift+M` again.** The music comes back at the same loudness as before (the song restarts from its beginning), and the transcript says `Music restored.`.
4. **`Alt+Shift+S`.** Effects stop and the music keeps playing; the transcript says `SFX muted.`. Walk into a wall: no bump sound.
5. **`Alt+Shift+S` again.** Effects come back at the same loudness; the transcript says `SFX restored.`.
6. **Settings.** Mute the music (`Alt+Shift+M`), then `Alt+M` → Settings. The rows read `SFX Volume: 40%` and `Music Volume: 70% (muted)`. Press Right on the Music row: it reads `Music Volume: 80%`, and the music plays again. Close the menu.
7. **Kill one enemy.** In any fight, land a killing blow. You hear **one** hit burst for it, not two.
8. **Controls.** `Alt+M` still opens the System Menu, and `Alt+S` still says "Save complete".

**PASS:** steps 2–8 as described. Music and effects never mute each other, and a restore comes back at the configured level, not louder. No stutter, `task_wdt`, crash or reboot.

**FAIL:** a mute that silences the wrong channel or both; a restore at a different loudness; a Settings value that changed after a mute; `Alt+Shift+M` opening the menu or `Alt+Shift+S` saving; two bursts on a kill. Report it with the `FW` / `Git` lines.

*Not in this check:* a reboot. The mute is session-only by design (host test M9). If you do reboot, the device must start unmuted at 80 % music.

### Phase H-204 — a load leaves no prompt behind · *new firmware; SD pack unchanged* · about 3 minutes · **PENDING**

Flash the A3-HF4 Launcher image (its path and SHA-256 are in tag `alpha3-hf4-load-transient-reset`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf4-debug` and that tag's `Git`; if they differ, stop. Any card with a save works. A serial capture is optional: each load prints one `LOAD_TRANSIENT_RESET ui=<before>-><after>` line.

1. **Save.** Stand somewhere open and press `Alt+S` ("Save complete").
2. **Mix, then load from the menu.** Press `M`: the Mix list opens. Press `Alt+M` → *Load / Save Management* → *Continue Latest*.
   - The game is back at the saved place **with no spell list on screen**, and the status line shows no prompt. Walk one step: it walks. Mic is not needed.
3. **The same with `Alt+L`.** Press `M` again, then `Alt+L`: same result as step 2.
4. **A direction prompt.** Press `L` (Look); it asks for a direction. Press `Alt+L`. After "Load complete", a trackball move **walks**; no "Thou dost see" appears.
5. **A yes/no.** Enter a town and walk out through its edge until it asks "Leave this place?". Do not answer; press `Alt+L`. You are back at the saved place with no question on screen, and the next moves and commands work.
6. **A picker (optional).** Press `R` (Ready), then `Alt+L`: the picker is gone.
7. **A failed load keeps the prompt (optional).** Only with a card that holds no valid save (an empty *Generation* row cannot be chosen, so there is no other way to make a load fail): press `M`, then `Alt+L`. It says "No valid save" and the Mix list is **still open**; Mic closes it as before. The host covers this case (L7).
8. Throughout: no crash, `task_wdt` or reboot; no leftover highlight, inverted row or picker panel; music and effects normal.

**PASS:** steps 2–5 return straight to normal play with nothing left open, and the next key acts in the loaded game; step 7 (if run) keeps the prompt.

**FAIL:** a spell list, direction prompt, yes/no or picker still on screen after "Load complete"; a key after the load that answers the old prompt; commands refused after the load until Mic or a reboot; a failed load that closes the prompt. Report it with the `FW` / `Git` lines (and the serial `LOAD_TRANSIENT_RESET` line if captured).

### Phase H-205 — conversations keep their pauses · *new firmware; SD pack unchanged* · about 5 minutes · **PASS** (the user, on the A3-HF5 image, reported 2026-09-27; recorded by A3-HF6)

Flash the A3-HF5 Launcher image (its path and SHA-256 are in tag `alpha3-hf5-dialogue-pacing`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf5-debug` and that tag's `Git`; if they differ, stop. The image carries A3-05 and A3-HF4 unchanged, so H-203 and H-204 can be run on it too. A serial capture is optional: each paused speech prints `DIALOGUE_PAUSE begin=timed|key` and `DIALOGUE_PAUSE end`.

**Chuckles (mandatory).**
1. Enter Lord British's castle at daytime and find **Chuckles**, the bouncing jester, on the ground floor.
2. Talk to him (`T` + direction). At "Your interest?" type `ENTE` and Enter.
   - "Ho eyo he hum!" appears **alone**. About **1.5 s** later the second "Ho eyo he hum!", then the third, then "Bounce, bounce, bounce, bounce!", then "Didst thou enjoy that?", each about 1.5 s after the one before (about 6 s in all). Only then does the "You respond-" prompt return. Answer `Y`: "I thought thou might!" appears at once.
3. Ask `ENTE` again and press any key (a letter, space or the trackball) while a verse is waiting: the next verse appears **at once**, and the key does nothing else (nothing is typed, the Avatar does not move, the conversation does not end, Mic included).
4. Ask `WELC`. "Welcome, welcome, welcome." appears and the status line shows **`Enter: continue`**. Nothing more appears however long you wait. Each key shows the next part (four keys in all), ending with "...That's ME!" and "Your interest?".

**Blackthorn (optional; mandatory only if the host evidence is questioned).**
5. In Blackthorn's palace, talk to **Blackthorn** in his throne room. "You see the Dark Lord himself!" is followed about 1.5 s later by the rest of his greeting.
6. If he asks "Wilt thou be staying with us long?", answer `N`: "I beg to differ!", then about 1.5 s later "So very kind of thee to deliver thyself unto me!", then about 1.5 s later "Prepare now to meet thy fate!". The map returns only after that last line. (His guards then come for the Avatar and the capture scene follows; that scene was already paced and is not part of this check.)

**Regression.**
7. **Combat stays immediate.** Fight anything: every combat line appears as it happens, with no pause.
8. **Menus and loads.** During Chuckles' `ENTE`: press `Alt+M` and wait five seconds. Nothing new appears behind the menu; after closing it the rest of the song follows. Then press `Alt+S`, ask `ENTE` again, and press `Alt+L` in the middle of the song: the song stops, "Load complete", and the next key is an ordinary command. No stuck pause, no garbled or doubled transcript lines, no audio stutter.
9. Throughout: no crash, `task_wdt` or reboot; music and effects normal.

**PASS:** steps 2–4 and 7–8 as described (and 5–6 if run).

**FAIL:** a routine that still appears all at once; a verse that waits much longer than about 2 s with no key; a key that is typed into "Your interest?", moves the Avatar or ends the conversation during a pause; a `WELC` part that appears without a key; lines out of order, missing or doubled; combat text that waits; a pause still running after a load. Report it with the `FW` / `Git` lines (and the serial `DIALOGUE_PAUSE` lines if captured).

### Phase H-206 — the shrine and the Codex wait for a key · *new firmware; SD pack unchanged* · about 8 minutes · **PENDING**

Flash the A3-HF6 Launcher image (its path and SHA-256 are in tag `alpha3-hf6-shrine-key-waits`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf6-debug` and that tag's `Git`; if they differ, stop. The image carries A3-05, A3-HF4 and A3-HF5 unchanged. A serial capture is optional: each getkey prints `DIALOGUE_PAUSE begin=key`, and each key that ends one prints `DIALOGUE_PAUSE_INPUT ... effect=key-wait-ended ... gameplay_command=none`.

**What changed** (`ALPHA3_AUDIO.md` §32): the altar and the Codex now stop at each of the original's key waits, like a conversation's KeyWait. The text before a wait is shown, the status line reads `Enter: continue`, and the next part appears only after a key. That key does nothing else.

**Setup.** Start a **New Journey**, or load a save in which no shrine has been visited yet. Do part D last, because its preset marks every shrine visited.

**A. The altar (mandatory).**
1. `Alt+D` → Teleport → **Britannia**, X **233**, Y **66** (the Shrine of Honesty) → Teleport. Leave the Developer menu.
2. Press `E`. After the approach text, answer `Y` to "Visit?", then type `HONESTY` + Enter at "Virtue?" and `AHM` + Enter at "Mantra?".
3. "The Altar speaks and a Quest is ordained!" appears **alone**, and the status line shows **`Enter: continue`**. Wait 10 seconds: nothing more appears.
4. Press `A`, as if still typing. The lesson appears ("'Tis now thy sacred Quest to go unto the Codex and learn …"). The cue stays. No `A` is typed anywhere and no prompt opens.
5. Roll the trackball left once. "Return again when thy Quest is done!" appears, the cue goes, and **the Avatar does not move**.
6. Roll the trackball again: the Avatar moves normally.

**B. The Codex (mandatory).**
7. Teleport → **Britannia**, X **233**, Y **233** (the Codex). Press `E`. "Enter the Shrine of the Codex!" and "The Codex of Ultimate Wisdom lies before thee..." appear, then `Enter: continue`.
8. Press `E` four times, about a second apart. Each press shows exactly one more part: "The book is open to the page thou dost seek!", "Upon the hallowed page thou dost read:", the Honesty page in quotes ("A dishonest life brings unto thee temporary gain, …"), and then nothing new (the fourth `E` only clears the cue). The Codex is **not** entered again: "Enter the Shrine of the Codex!" appears only once.

**C. Menu and load (mandatory).**
9. Still on the Codex: `Alt+S`. Press `E` (the reading starts again). Press `Alt+M`, wait 5 seconds, and close the menu with `Alt+M`. Nothing new appeared behind the menu, and `Enter: continue` is still shown. Press Space: "The book is open…" appears, and nothing after it.
10. Press `Alt+L`. "Load complete" appears, the cue is gone, and no more Codex text appears. Roll the trackball: the Avatar moves normally.

**D. The ceremony (optional; about 2 minutes).**
11. `Alt+D` → Shortcuts → **Preset: Shrine** (confirm "Shrine Test Setup"). Go back to the Codex (step 7) and press `E`.
12. The ceremony takes nine keys. The fourth key brings the three quakes and "A STRANGE WIND CAUSES THE PAGE TO TURN!". The next four each bring one page: "Thou dost read:" with the first rune page, then one rune page per key. The ninth key only clears the cue. (On the A3-HF6 image the viewport inversion around the quakes is absent: do not file it. On the A3-HF7 image it is H-207: the fourth key's section arrives about 2.9 s later, after three coloured flashes, and keys pressed during them are ignored.)

**Regression.**
13. Talk to anyone whose speech has a KeyWait (Chuckles, `WELC`, H-205 step 4): it still waits for a key. Fight anything: combat text stays immediate.
14. Throughout: no crash, `task_wdt` or reboot; music and effects normal.

**PASS:** steps 3–10 as described (and 12 if run).

**FAIL:**
- any of the rite's text appears without its key, or a part waits without showing `Enter: continue`;
- a wait ends by itself;
- one key shows two parts;
- the key that ends a wait types a letter, opens a prompt, moves the Avatar or enters the Codex again;
- text is released behind the System Menu;
- a wait or its text survives the load;
- "WELL DONE!" or the donation prompt starts waiting for a key.

Report a failure with the `FW` / `Git` lines, and the serial `DIALOGUE_PAUSE` lines if captured.

### Phase H-207 — the ritual negative and the Codex's pulses · *new firmware; SD pack unchanged* · about 5 minutes · **PASS** (the user, on the A3-HF7 image, reported 2026-09-28; recorded by A3-HF8)

Flash the A3-HF7 Launcher image (its path and SHA-256 are in tag `alpha3-hf7-ritual-inversion`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf7-debug` and that tag's `Git`; if they differ, stop. The image carries A3-HF6 unchanged (H-206 may be run on it too; see its step 12). A serial capture is optional: each change of the map prints `RITUAL_FX kind=… mask=…`, each held effect `DIALOGUE_PAUSE begin=effect`, and each key pressed inside one `DIALOGUE_PAUSE_INPUT … effect=ritual-effect-swallowed … gameplay_command=none`.

**What changed** (`ALPHA3_AUDIO.md` §33): the map viewport — only the map, never the side panels, the text or the sky / wind strips — turns to its colour negative during "WELL DONE!" and "ALAKAZAM!", and flashes three times during the Codex ceremony, each flash with its own quake. Nothing reads a key while an effect runs: a key pressed then is ignored.

The colours below are the EGA index XOR the original uses: under the full negative black grass turns **white** and green specks **light magenta**; the Codex's first flash turns black **red**, its second **light cyan**, its third is the full negative.

**Setup (one preset for everything).** `Alt+D` → Shortcuts → **Preset: Shrine** (confirm "Shrine Test Setup"): every shrine visited, the Quest of Honesty in hand. Run A, then B, then C, in that order.

**A. The Codex's three pulses (mandatory, about 1 minute).**
1. `Alt+D` → Teleport → **Britannia**, X **233**, Y **233** (the Codex) → Teleport. Leave the Developer menu. Press `E`.
2. Press Space three times, a second apart: each press shows one more part ("The book is open…", "Upon the hallowed page…", the Honesty page in quotes) and the map stays normal. *(H-183 sanity: one key, one part, `Enter: continue` each time.)*
3. Press Space a fourth time and watch the map. While it flashes, press `E` once and roll the trackball once:
   - at once: the map **flashes red** (black → red) and shakes;
   - about **1 s** later: it turns **light cyan** and shakes again;
   - about **1 s** later: the **full negative** (black → white) and a third shake;
   - about **2.8 s** after the key: "A STRANGE WIND CAUSES THE PAGE TO TURN!" appears, and within a blink the map is **normal** again and `Enter: continue` shows.
4. The `E` and the trackball did **nothing**: no second "Enter the Shrine of the Codex!", no move, and "Thou dost read:" has **not** appeared; the page still waits for its own key.
5. Press Space: "Thou dost read:" and the first rune page appear, nothing more. Four more Space presses finish the pages (the last only clears the cue).

**B. WELL DONE's negative (mandatory, about 1 minute).**
6. Teleport → **Britannia**, X **233**, Y **66** (the Shrine of Honesty). Press `E`, `Y` to "Visit?", `HONESTY` + Enter, `AHM` + Enter.
7. "WELL DONE!" appears and **in the same instant the map turns negative** (white where it was black), with the rising-and-falling sweep sound. The side panels and the text stay normal.
8. After about **5.3 s** the screen shakes (about 1 s), still negative.
9. At the end of the shake "Intelligence +1" appears and **the map returns to normal at the same moment**. About half a second later the game accepts input again.
10. During steps 7–8 press `E` once: nothing happens (no "Enter what?", no new rite).

**C. The donation (optional, about 30 s).**
11. Still on the altar: `E`, `Y`, `HONESTY` + Enter, `AHM` + Enter. At "How many cycles?" press `1` + Enter: "ALAKAZAM!" and the negative at once, with the sweeps, for about **7 s**, **no shake**; then the map is normal.

**D. Menu and load (mandatory, about 1 minute).**
12. `Alt+S` on the altar. Donate again (step 11). While the map is negative press `Alt+M`, wait 3 s, close with `Alt+M`: the map is negative again and finishes normally.
13. Donate again and, while the map is negative, press `Alt+L`: "Load complete", the map is **normal at once**, and nothing of the rite appears afterwards.

**Regression.** Talk to Chuckles or anyone with a KeyWait (H-205 step 4): still waits for a key. Throughout: no crash, `task_wdt` or reboot; music and effects normal.

**PASS:** steps 3, 4, 5, 7, 8, 9, 10, 12 and 13 as described (and 11 if run).

**FAIL:**
- no negative / no flash; the side panels or the text invert; the whole screen inverts;
- the negative stays after "Intelligence +1", after "A STRANGE WIND…", after the load or after leaving the shrine;
- "Intelligence +1" appears together with "WELL DONE!" (before the sweeps and the shake);
- the Codex's three flashes come without their quakes, or "A STRANGE WIND…" appears before the third;
- a key pressed during an effect does something, or makes "Thou dost read:" appear without another key;
- one key shows two parts.

Report a failure with the `FW` / `Git` lines, and the serial `RITUAL_FX` / `DIALOGUE_PAUSE` lines if captured.

### Phase H-208 — the Blackthorn sacrifice burst · *new firmware; SD pack unchanged* · about 5 minutes · **PASS** (the user, on the A3-HF8 image, reported 2026-09-28; recorded by A3-HF9)

Flash the A3-HF8 Launcher image (its path and SHA-256 are in tag `alpha3-hf8-sacrifice-burst`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf8-debug` and that tag's `Git`; if they differ, stop. The image carries A3-HF7 unchanged. A serial capture is optional: the burst prints `BLACKTHORN_BURST cell=(5,7) tile=0 hold_ms=174` and `SFX_CUE id=combat-hit source=blackthorn-scene`; every key pressed during the scene prints `BLACKTHORN_SCENE_INPUT … effect=swallowed … gameplay_command=none`.

**What changed** (`ALPHA3_AUDIO.md` §34): at the end of the sacrifice siren, the explosion star (tile 0, the same star the combat hit shows) covers **one cell, the victim's**, for about a sixth of a second, with a short noise crack (the same sound as hitting an enemy in combat). Only then does the victim disappear. Nothing shakes, nothing inverts, and no key does anything during it.

**Setup (about 1 minute).** Any game (New Journey is fine). `Alt+D`:
1. Shortcuts → **Preset: Maxed party** (six members; the victim is the second living one, normally Shamino).
2. Time → **Hour 12**, **Minute 0** (the guards stand where the steps below expect them at noon).
3. Teleport → Destination **Palace of Blackthorn**, Floor **Ground Floor**, X **13**, Y **25**, Use default entrance **Off** → Teleport. Leave the Developer menu.
4. `Alt+S`.

**A. The pendulum (mandatory, about 2 minutes).**
5. Press Space. The guard north of you at (13,24) captures the party ("Thou art subdued and blindfolded!") and the capture scene runs as before. *If Space alone does not start it, stand beside any palace guard and press Space again.* Press Enter at each `Enter: continue`.
6. At each `Your response?` type **X** + Enter, four times (Enter at every `Enter: continue` between them). After the first wrong answer the companion is dragged to the table in front of Blackthorn and the hourglass is set down; that is unchanged.
7. After the fourth answer: the pendulum line; about half a second later the rising-and-falling siren, about **7 s**, the companion's body frozen on the table. During the siren press `E` once and roll the trackball once.
8. **At the end of the siren: the star covers the table cell, and only that cell, for a blink (about 0.17 s), with a short crack. Then the table is empty and "Shamino is sliced in half!" appears** (the name is your second member's), with `Enter: continue`.
9. The `E` and the trackball did **nothing**: no "Enter what?", no move, and nothing past "sliced in half!" has appeared. One Enter shows the next line, as before.

**B. The betrayal (optional, about 1 minute).**
10. `Alt+L`, Space, and at the first `Your response?` type **AHM** + Enter (the question is about Honesty). After the merciful-death line and the same siren, the star covers the companion's **seat** (two cells right of the centre, not the table) for the same blink, and then the seat is empty.

**C. Menu and load (mandatory, about 1 minute).**
11. `Alt+L`, run step 5 and step 6 again. During the siren press `Alt+M`, wait 3 s, close with `Alt+M`: the scene continues; the star still appears **once**, on the table, before the table empties, and "sliced in half!" follows it.
12. `Alt+L`, run steps 5–6 once more, and during the siren press `Alt+L`: "Load complete", the ordinary palace with the whole party, **no star** and no scene afterwards.

**Regression.** The capture's own pacing is unchanged (the empty cell before Blackthorn appears, the holy-circle frame, the two getkeys; H-120 / 7D-B). Throughout: no crash, `task_wdt` or reboot; music and effects normal.

**PASS:** steps 8, 9, 11 and 12 as described (and 10 if run).

**FAIL:**
- no star at all; the star on the wrong cell, on more than one cell, or over the whole viewport; a colour negative or a shake;
- the victim disappears **before** or **with** the star, or "sliced in half!" appears before it;
- the star stays after "sliced in half!", after the load or after leaving the palace;
- two stars, or a star that lasts clearly longer than a blink (about 1 s or more);
- a key pressed during the scene does something, or answers the `Enter: continue` after "sliced in half!".

Report a failure with the `FW` / `Git` lines, and the serial `BLACKTHORN_BURST` / `BLACKTHORN_SCENE` lines if captured.

### Phase H-210 — the Refuge's cadence and its karma key wait · *new firmware; SD pack unchanged* · about 5 minutes · **PENDING**

Flash the A3-HF9 Launcher image (its path and SHA-256 are in tag `alpha3-hf9-refuge-cadence`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf9-debug` and that tag's `Git`; if they differ, stop. The image carries A3-HF8 unchanged. A serial capture is optional: each figure prints `REFUGE_SCENE phase=…`, each peal `QUAKE pulses=8`, the key that ends the wait `NARRATIVE_SCENE_INPUT … effect=key-wait-ended … gameplay_command=none`, every other key `… effect=swallowed …`, and the end `REFUGE_RESOLVE`.

**What changed** (`ALPHA3_AUDIO.md` §35): the Refuge (the scene after the whole party dies) now keeps the original's own timing. It starts about half a second **before** the first line; "But thy slumber is disturbed!" stays alone for about **10 s** (its melody); the peal of thunder shakes the map; and Lord British's quoted speech **waits for a key** (`Enter: continue`) instead of leaving by itself. Any other key during the scene is ignored.

**Setup (about 1 minute).** Any game (New Journey is fine), outdoors on foot. Sound on if you can (the melody and the thunder help), but it is not required.
1. `Alt+S`.
2. `Alt+D` → Party → **Party size 1**; then Shortcuts → **Preset: Low health/status** (the Avatar is poisoned at 1 HP). Close the Developer menu.

**A. The cadence and the key wait (mandatory, about 2 minutes).** Press Space (Pass) once at a time until the poison kills the Avatar (usually one or two presses). From the Space that raises the scene, with a stopwatch or a phone video if you can:
3. For about **½ s** nothing new: the map is still drawn and the music stops. Then "An unending darkness engulfs thee..." and, at once, the map goes **black** but for the Avatar in the middle, followed immediately by "Thou hast found refuge."
4. About **¾ s** later: "No evil lives here, only peace and darkness." About **1½ s** later: "But thy slumber is disturbed!" (and its slow six-note melody).
5. **About 10 s** of nothing new (the melody plays out). Then "Someone shouts" and "FORTIS FORTUNA AVENTARI" together.
6. About ⅓ s later the left figure, ¼ s later the right one, ¼ s later "There is a peal of thunder!": the map **drops a couple of pixels** (the shake) with two rumbles for about **2 s**, then the cyan apparition appears at the top and, a blink later, Lord British's speech **in quotes**.
7. **Wait at least 10 s.** The speech stays; `Enter: continue` is on the status line; "Strange words are intoned." does **not** appear.
8. While it waits press `Alt+M`, wait 3 s, roll the trackball in the menu, close with `Alt+M`: still waiting, nothing new.
9. Press **one** key (Space). "Strange words are intoned." appears at once (with a rising tone), then about **1½ s** later "Vertigo...", then a moment later the party wakes in Lord British's castle, healed.
10. Step 9's key did nothing else: no command text, no prompt, the party did not step. The next key after waking plays normally.

**B. Keys during the scene (mandatory, 30 s).** `Alt+L`, repeat step 2 and the Space presses. During the 10 s melody press Space and `E` several times: nothing happens, and the speech of step 6 still **waits** for its own key (the early presses were not kept). End it with Enter.

**C. Load at the wait (mandatory, 30 s).** `Alt+L`, step 2, Space until the speech waits; press `Alt+L`: "Load complete", the ordinary map with the living Avatar, **no** "Strange words", no black map, no `Enter: continue`. Then repeat step 2 and Space: the **whole** Refuge plays again from its first half-second (it does not jump straight to the castle).

**Regression.** A TLK KeyWait (H-205), a shrine getkey (H-206), WELL DONE's negative (H-207) and the Blackthorn burst (H-208) are unchanged. Throughout: no crash, `task_wdt` or reboot.

**PASS:** steps 3–10, B and C as described (timings within roughly ±½ s by eye; the order and the key wait are what matter).

**FAIL:**
- the darkness line appears in the same instant as the Space, or the map goes black before it;
- "Someone shouts" follows "But thy slumber is disturbed!" after only a second or so;
- the speech leaves by itself, or "Strange words are intoned." appears without a key;
- the key that ends the wait also moves the party, types, opens a prompt, or skips "Vertigo..."'s pause;
- a key pressed before the speech ends it as soon as it appears;
- the castle appears before "Vertigo...", or the Refuge replays (or the party is revived) after the load.

Report a failure with the `FW` / `Git` lines, and the serial `REFUGE_SCENE` / `NARRATIVE_SCENE_INPUT` lines if captured.

### Phase H-213 — Mix: mark the reagents, answer "How much?" · *new firmware; SD pack unchanged* · about 5 minutes · **PENDING**

Flash the A3-HF10 Launcher image (its path and SHA-256 are in tag `alpha3-hf10-mix-parity`). The boot screen must show `FW 3.0.0-alpha3-dev-a3-hf10-debug` and that tag's `Git`; if they differ, stop. The image carries A3-HF9 unchanged, so this phase can share a session with H-210, H-203 and H-204.

**What changed** (`ALPHA3_AUDIO.md` §36, D-6 / D-70): after choosing a spell, `M`ix no longer picks the reagents or mixes one by itself. A **Reagents:** list opens with nothing marked; you mark the reagents yourself (Enter or Space), press `M`, and answer **How much?** with a number. A wrong set of reagents is spent and sets off a trap, exactly as in 1988.

**Setup (about 1 minute).** Any game, outdoors on foot (not in a dungeon: there `M` is Cast).
1. `Alt+S`.
2. `Alt+D` → **Reagents**: Reagent index `1` (Ginseng) → Quantity `9`; Reagent index `3` (Spider Silk) → Quantity `9`. Close the Developer menu.

**A. The picker (mandatory, 1 minute).**
3. Press `M`: the **Mix Spell** list. Scroll to **Mani** and note its count N (`Mani xN`; a bare `Mani` means 0 or 1 — then expect `x3` or `x4` in step 7). Press Enter.
4. A **Reagents:** panel opens: one row per reagent you own, each with a two-digit count (`09   Ginseng`, `09   Spider Silk`, …), **no row marked**, and `Mix: Mani` above the list. The context bar reads `Enter Mark|M Mix|Mic`. Nothing has been spent.
5. Move to Ginseng and press Enter: a `*` appears between its count and name (`09 * Ginseng`). Press Enter again: it goes; once more: it is back. Move to Spider Silk and press **Space**: `*`. The cursor stops at the first and last rows (it does not wrap).

**B. Quantity 3 (mandatory, 1 minute).**
6. Press `M`: the panel closes and the status line asks `How much?`. Type `3` (the line shows `How much? >3`) and press Enter: "Mixing..." then "Done!".
7. Press `M` again: the list shows **Mani x(N+3)** (capped at 99). Enter on Mani: the panel shows Ginseng **06** and Spider Silk **06** (3 × each). Keep the panel open for C.

**C. A wrong recipe (mandatory, 30 s).** Mark **only Ginseng**, `M`, `1`, Enter: "Mixing...", **no** "Done!", then a trap line (`ACID!`, `POISON!`, `BOMB!` or `GAS!`; a member may lose HP or be poisoned). `M` + Enter on Mani: Ginseng is **05**, Spider Silk still **06**, and the Mani count did not change.

**D. Too many (mandatory, 30 s).** In that panel mark Ginseng and Spider Silk, `M`, type `9`, Enter: "Insufficient reagents!" and `How much?` again, with nothing spent. Press **Mic**: the typed digits vanish but the question stays. Press Enter on the empty answer: the question closes, nothing mixed, nothing spent.

**E. Cancel (mandatory, 20 s).** `M`, Mani, mark Ginseng, press **Mic**: the panel closes silently; the counts are unchanged. `M`, Mani again: **nothing is marked**. Mic to close.

**F. Load (optional, 30 s).** `M`, Mani, mark both, `M`, type `2`, then `Alt+L`: "Load complete", no panel, no `How much?`; the counts are the saved ones (Setup step 1). Press `2` and Enter: nothing is mixed (Enter is Pass).

**Regression.** Cast (`C`) still lists only mixed spells and casts; Hole up (`H`) still asks its hours and Mic still cancels it. Throughout: no crash, `task_wdt` or reboot.

**PASS:** steps 3–7 and C–E as described.

**FAIL:**
- choosing the spell mixes at once, or the panel opens with the recipe already marked;
- a mix spends 1 of each reagent or adds 1 to the spell whatever the answer;
- a wrong or incomplete set of marks gives "Done!" or a charge;
- "Insufficient reagents!" spends anything, or does not ask again;
- Mic at the panel or at `How much?` spends anything;
- a key typed in the panel or at `How much?` moves the party, opens a command or appears in the text;
- a panel or question survives the load.

Report a failure with the `FW` / `Git` lines.
