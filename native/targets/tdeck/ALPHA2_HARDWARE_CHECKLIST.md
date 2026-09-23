# Alpha 2 — Consolidated Physical T-Deck Checklist

**Created:** Batch 18 (2026-09-21) · **Supersedes** the per-batch phase lists in
[`GAMEPLAY_INTEGRATION_AUDIT.md`](GAMEPLAY_INTEGRATION_AUDIT.md) §16 for *execution* purposes.
§16 stays as the archaeology — it records which batch owed which step and why.
**This file is the single list to run in one device session.**

> **Nothing in this file has been executed.** No physical T-Deck was available
> during Batch 18 or Batch 19. Every row's result column reads `UNTESTED`. Do not infer a
> PASS from a host-suite green: the whole point of these rows is that they are
> the checks host evidence cannot make.

## Session setup

| Item | Value |
|---|---|
| Firmware image | `native/targets/tdeck/build-batch19/openu5_tdeck.bin` |
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
| H-12 | World, on foot, a caster with Vas Rel Por mixed and ≥ its mana | `C` → `Vas Rel Por` | the prompt **`To phase:`** appears | none yet | any non-`1`–`8` key, and the device Cancel, **fail silently** — no banner, no teleport | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Screen shows `Aim: empty (-1,-1)` with a `Move\|Confirm\|Mic Back` footer instead of `To phase:`. Root cause (proven): `AlphaRuntime::overlay()` (`alpha_runtime.cpp:1489`) unconditionally renders the generic combat-reticle overlay for ANY non-`Fire` `UiMode::TargetSelection` request, clobbering the real prompt text set by `begin_target(...,"To phase:",...)`. Confirmed NOT cosmetic-only for GatePhase specifically: a bare `2` and `e` (no Sym) both produced `"Failed!"` with no ceremony flash at all, even though `ui_session.cpp:665-673` correctly accepts `'1'`-`'8'` for `GatePhase` — the digit never reaches that handler, pointing to a T-Deck key-routing gap upstream, not yet localized. **Blast radius confirmed wider in H-15:** the same `overlay()` clobbering also hides the plain world `Direction?` getdir text (harmless there, since Move+Confirm is the correct interaction for a spatial direction pick — only `GatePhase`'s non-spatial digit entry is functionally broken by it). Deferred to Batch 21 (isolated to targeting-prompt presentation + this one spell's input path, does not block the rest of the checklist) |
| H-13 | At the H-12 prompt | a digit `1`–`8` | the ceremony flash, then the party is **elsewhere**, with **no** `Success!`/`Failed!` banner | moonstone teleport applied | — | **FAIL — same defect as H-12.** Cannot be exercised; the phase digit never registers, so the ceremony/teleport never fires |
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
| H-53 | Any party | `M`ix a spell | the spell list opens and the mix resolves. *Expected residual: quantity is always 1 — D-6* | reagents spent | — | PASS (Batch 20, hardware, `a357e28c…`) |

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
| H-146 | **New in Batch 20 — no prior row covered this.** Shipwright or Horse Seller, gold ≥ price | `Y` to confirm the purchase | The ship/skiff/horse is granted, gold spent, transport placed at the dock/stable | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** `N` (decline) works; `Y` (confirm) does **nothing** — no message, no gold spent, no transport granted. Root cause (proven): `shop_orchestration.cpp`'s `ShipDeal` confirm handler (line 467-470) and `horse()` handler (line 285-288) both require the `v.ship`/`v.horse` and `v.reserve` callback hooks; `alpha_runtime.cpp:220-229` wires `shop_services_.record_present/record/tile/occupied/plate/hour_tiles/wake_npcs` but never assigns `.ship`, `.horse`, or `.reserve`, so they're null on this build and the confirm silently hits `CommandStatus::Unsupported`. Device-only adapter wiring gap; player-facing effect is that no shop can sell a ship, skiff, or horse. Workaround for the rest of this session: Debug → Presets → "Transport Test Setup" bypasses the shop. Deferred to Batch 21 (doesn't block remaining validation given the workaround) |
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
| H-115 | In a dungeon | save, reload | position, facing, revealed cells, wanderer **and any field you cast** resume exactly (R-15; fields ride in `DungeonState::cells`) | **FAIL — confirmed production defect (Batch 20, hardware, `a357e28c…`).** Save reports success; reloading (same session, no power-cycle) resumes an older/prior save instead. Reproducible specifically in dungeons — never observed in the overworld. Root cause (proven architectural gap, exact trigger for this symptom not yet confirmed): `alpha_save.cpp`'s `candidate()` (used as the save's own "semantic-validation" self-check, line ~178) and `restore_candidate()` (the core of `load()`) both only call `restore_gameplay`/`restore_terrain`/`restore_npc_walk` — **neither ever calls `restore_dungeon`**. Dungeon state is restored as a separate step afterward (`alpha_runtime.cpp:2166`), and its failure just logs `DUNGEON_RESTORE_FAILED` and silently resets `dungeon_={}` with no player-visible indication. This means both the save-time and load-time integrity checks are blind to dungeon-payload corruption — a save/load can be reported fully successful while the dungeon-specific data is invalid. Needs live log confirmation (`DUNGEON_RESTORE_FAILED` at the moment of reload) to nail the exact mechanism, but the architectural gap itself is confirmed by code inspection. High priority for Batch 21 alongside H-149/H-150/H-151 **Batch 26: HOST FIXED / DEVICE RETEST PENDING (audit Phase 6U).** The cause was not the save self-check: the sidecar exporter never wrote the `"dungeon"` object (`persistence.cpp` `extras[]`), so every dungeon save loaded at the entrance on the surface. Saving underground is allowed in 1988 (`CAST2.OVL:0x10FE`, no location gate) and resumes in place. The Rel Tym toggle is 0 after a load (`DUNGEON 0x0E40`, not saved). Use the System Menu load (`Alt+L` is H-164). See `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 26" |
| H-116 | Descend past floor 7 | the Underworld transition | PASS (Batch 20, hardware, `a357e28c…`) |
| H-117 | Walk out at the level-1 entrance | the surface world view and Exploration verbs return | PASS (Batch 20, hardware, `a357e28c…`) |

---

## Group 10 — Quest, Blackthorn and persistence

| # | Setup | Input | Expected | Result |
|---|---|---|---|---|
| H-118 | Debug → Quest → grant a shard | `U`se it in a Flame room | the shard's effect fires (R-08) | **FAIL — CRITICAL, confirmed production defect (Batch 20, hardware, `a357e28c…`).** Used Debug → Certification → "Flame/Shard Test" (grants all 3 shards, teleports to the verified Empath Abbey (15,3) floor-1 ritual cell), then `U`sed a shard. Only the generic "Use item" echo appeared — no ritual text at all, even though `cast_shard_into_flame()` (`quest.cpp:89-107`) is unconditional: it always writes a header line first regardless of position match, so at minimum 1-2 lines of flavor text should always print. **Afterward, movement became permanently silent — no `Blocked!`, no echo, nothing — and did NOT recover after teleporting elsewhere or loading a save.** `Alt+M`/`Alt+D` (System Menu/Developer) still respond normally, ruling out a total device hang; teleport and load commands are themselves still processed (the destination/reload happens) but movement remains dead afterward regardless. Since Load only restores `GameState`/`TurnState`/quest data (confirmed via `alpha_save.cpp` review, see H-115) and never touches UI session mode, and this survives both teleport and load, the stuck state most likely lives in UI-session/input-mode state (e.g. a leftover `TargetSelection`-style mode silently swallowing movement input) rather than corrupted save data. **Narrowed further:** `L`ook and `Z`-stats both work normally (Look correctly resolves different directions per trackball/WASD input, confirming directional input hardware is fine) — only the world **Move** command path specifically is affected, producing no response at all (not even `Blocked!`). This rules out a broad UI/input freeze; it's a targeted lock on one command kind. Recovered via power cycle (teleport/reload were insufified). Root cause not yet isolated — needs dedicated investigation. **Top priority for Batch 21 alongside H-149/H-150/H-151/H-115** **Batch 25: HOST FIXED / DEVICE RETEST PENDING (audit Phase 6T).** Root cause was not the ritual: a question the core was waiting on (e.g. "Leave this place?") was on screen when `Alt+D` was pressed; `UiSession::set_base_mode()` overwrote the Developer menu's return register, so closing the menu dropped the question while `awaiting_exit` stayed set and every world command (Use, Move, Look's core half) was refused silently. See `GAMEPLAY_INTEGRATION_AUDIT.md` §"Batch 25" |
| H-119 | At a dungeon entrance | `Y`ell a word of power | the quake and the flag toggle | UNTESTED |
| H-120 | Blackthorn's palace, capture sequence | run the whole ceremony | the interrogation asks its questions, each with a visible **`Your response?`** row (see H-23), and the scene reads as a distinct scene, not the ordinary Palace lobby (R-32) | UNTESTED |
| H-121 | During H-120, a companion is executed | observe the active character | **the previously-set active character is NOT re-pointed** — if it named the executed companion or anyone after them in the marching order, it now names someone else. That is `BLCKTHRN 0x03ae-0x04d4`'s own behaviour. **Do not file it** | UNTESTED |
| H-122 | During H-120 | watch the pacing | **KNOWN OPEN — D-10/Y-32.** The scene may feel faster and more collapsed than the original. **If you can record video against original footage, that recording is the missing evidence.** Capture it rather than filing a ticket | Consistent with the known-open D-10/Y-32 pacing issue (fast, as expected — not filed separately). **Additional new observation (Batch 20):** the animation for Blackthorn teleporting in appears to be missing entirely, distinct from the general pacing issue. Recorded for Batch 21 follow-up, not yet root-caused |
| H-123 | Falsehood / Abbey chain | run it | the chain completes and its quest objects register | UNTESTED |
| H-124 | Overworld, town, dungeon, aboard a ship, mid-quest | save and reload in each | after each: position, party, inventory, equipment, time, transport, world objects, hidden/search objects, and the UI mode and renderer all match | PASS for overworld/town/ship contexts (Batch 20, hardware, `a357e28c…`). Dungeon context not independently retested — presumed to share H-115's confirmed defect given the identical save/reload mechanism **Batch 26:** the dungeon context shared H-115's defect and is fixed with it; world objects were already persisted (R-14) and are now pinned by `batch26_dungeon_save` L1/L2. Dungeon context: **DEVICE RETEST PENDING (audit Phase 6U)** |

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
| H-135 | Trigger an earthquake event (Yell a word of power at a dungeon entrance, H-119) | observe | the **Quake** shake renders | Y-04 | UNTESTED |
| H-136 | Combat: land a killing area-effect blow, or step a poisoned member through a turn | observe | **CellExplosion** draws on the cell; **PoisonTick** flashes the poisoned member's roster row by inversion, not by a tile write | Y-04 | UNTESTED |
| H-137 | Enter a Refuge; separately, let a troll ambush you at a bridge | observe | the **Refuge** and **TrollSneak** narrative scenes both pace out as sequences with readable text, not as a single flashed frame | Y-04 | PASS for TrollSneak (Batch 20, hardware) — confirmed via a bridge ambush. Refuge half not yet tested |
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
| H-156 | Bed hole-up runs **no** per-tick turn housekeeping. 1988 calls kernel `0x2AE8 kernel_turn_housekeeping` at `CMDS.OVL:0x0671` every ten minutes: poison 1 HP, meals at 06:00/12:00/18:00, `Starving!`, turn counter, Q/T spell expiry, regeneration-ring roll. Sleeping in the port costs no food, never starves you, never ticks poison and never regenerates. The TypeScript reference omits it too | **CONFIRMED MISSING (both ports) — queued, highest severity of the sweep.** Do not file |
| H-157 | Sleeping across 20:00 or 05:00 leaves the day/night tile overlay stale (drawbridge planks `0x48/0x49`, lamp `0x87`). 1988 calls `TOWN.OVL:0x0170 town_schedule_tile_refresh` from inside the sleep loop at `CMDS.OVL:0x0664` | **CONFIRMED MISSING (both ports) — queued.** Do not file |
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
| H-165 | A question left pending across a Developer teleport *into a dungeon* cannot be answered there (`Exit`/`DeclineExit` are refused by the dungeon context gate), so the lock could return on surfacing | **CONFIRMED BY CODE READING — queued, not reproduced.** Answer on-screen questions before opening the Developer menu |

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
