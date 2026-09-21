# Alpha 2 — Consolidated Physical T-Deck Checklist

**Created:** Batch 18 (2026-09-21) · **Supersedes** the per-batch phase lists in
[`GAMEPLAY_INTEGRATION_AUDIT.md`](GAMEPLAY_INTEGRATION_AUDIT.md) §16 for *execution* purposes.
§16 stays as the archaeology — it records which batch owed which step and why.
**This file is the single list to run in one device session.**

> **Nothing in this file has been executed.** No physical T-Deck was available
> during Batch 18. Every row's result column reads `UNTESTED`. Do not infer a
> PASS from a host-suite green: the whole point of these rows is that they are
> the checks host evidence cannot make.

## Session setup

| Item | Value |
|---|---|
| Firmware image | `native/targets/tdeck/build-batch18/openu5_tdeck.bin` |
| **SD resource pack** | **NO REFRESH REQUIRED.** Batch 18 changed no resource file. The last pack change was Batch 9C (`openu5-alpha1-resources.bin`, 2,039,545 B, payload CRC32 `0x2065ad91`, SHA-256 `434cd664…b4ea`). If the card already boots a Batch 9C-or-later image, leave it alone. |
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
| H-01 | Card per setup table | Power on | `IDENTITY … match=1`, then the identity screen, then the title | boots to frontend | — | UNTESTED |
| H-02 | Title | wait 20 s, then any key | attract demo runs, then the menu | — | — | UNTESTED |
| H-03 | Menu | `U` Introduction, page through; `A`; `R`; `S` (change brightness + trackball, Back) | each screen renders; settings persist across a power cycle | settings file written | Back returns to menu | UNTESTED |
| H-04 | Menu | `C` Create New Character → name, gender, full questionnaire | enters gameplay as the new identity | `objects_` empty, party is the new identity | — | UNTESTED |
| H-05 | Power-cycle after H-04 | `J` Journey Onward | the created save loads | position/party restored | — | UNTESTED |
| H-06 | In world | Mic **short** press | Cancel/back | **no `0` appears in any text field** | — | UNTESTED |
| H-07 | In world | Mic **hold ≈1.1 s** | `INPUT_HOLD … emitted=movement-toggle state=ON` | Movement Mode ON | — | UNTESTED |
| H-08 | Movement Mode ON | WASD; then `Y` → type `wasd` | WASD moves; inside the Yell text row the **literal characters** appear | — | — | UNTESTED |
| H-09 | In world | Trackball all four directions; `Alt+M` open and close | movement in four directions; System Menu opens and closes | — | `Alt+M` again closes | UNTESTED |

---

## Group 2 — Y-series input and magic closeout (Batch 16)

*The three checks Batch 16 left owed. Firmware only.*

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-10 | World, party ≥2, an active player set via `1`–`6` | **`Symbol` held + Mic pressed and released** (do **not** hold past 1.1 s) | the `Set Active Plr:` echo, resolving to *no* active member | `active_character` returns to `255` | — | UNTESTED |
| H-11 | Immediately after H-10 | plain Mic short press, then a plain 1.1 s hold | short = Cancel as before; hold = Movement Mode toggle as before | unchanged contract | — | UNTESTED |
| H-12 | World, on foot, a caster with Vas Rel Por mixed and ≥ its mana | `C` → `Vas Rel Por` | the prompt **`To phase:`** appears | none yet | any non-`1`–`8` key, and the device Cancel, **fail silently** — no banner, no teleport | UNTESTED |
| H-13 | At the H-12 prompt | a digit `1`–`8` | the ceremony flash, then the party is **elsewhere**, with **no** `Success!`/`Failed!` banner | moonstone teleport applied | — | UNTESTED |
| H-14 | **Aboard a ship or skiff**, same caster | `C` → `Vas Rel Por` | **the `To phase:` prompt must not appear at all** — the ship check precedes the print (`CAST.OVL 0x0cf6`) | spell/mana still spent per the reference, no teleport | — | UNTESTED |
| H-15 | World, carrying a **Rel Hur** scroll (count noted) | `U` → Rel Hur → **cancel** the `Direction?` prompt | the direction row appears and closes | **the scroll count is DOWN by one** — the reference consumes at selection, before the getdir | this *is* the cancel path | UNTESTED |
| H-16 | World, carrying a **skull key** (count noted) | `U` → skull key → **cancel** the `Direction?` prompt | same shape | **the key count is DOWN by one** | this *is* the cancel path | UNTESTED |

---

## Group 3 — The wishing well (Batch 18 / R-26, R-34)

*New in Batch 18. Firmware only; the card is unchanged.*

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-17 | Stand beside a well (tile 161) with gold > 0 | `L` + the direction of the well | transcript reads **`a well.`** and the prompt **`Drop a coin?`** | none | — | UNTESTED |
| H-18 | At the H-17 prompt | `N` | the transcript echoes **`No`** and the prompt closes | no gold spent | — | UNTESTED |
| H-19 | Re-open the prompt | **Mic short press (Cancel)** | identical to H-18 — it echoes **`No`** and closes. Before Batch 18 the key was simply ignored and the prompt stayed open | no gold spent | this *is* the cancel path | UNTESTED |
| H-20 | Re-open the prompt, gold > 0 | `Y` | echoes **`Yes`**, then the wish row opens with the prompt **`Thy wish?`** (not "What dost thou wish?") and **the prompt text is visible** — R-34 | 1 gold spent on submitting the wish | Cancel closes the wish row | UNTESTED |
| H-21 | Developer → set gold to **0**, then re-open the prompt | `Y` | echoes **`Yes`** and **stops** — no wish row, and **no** `Thou hast no coin!` (that line is a fabrication) | nothing | — | UNTESTED |
| H-22 | At Paws or Empath Abbey, gold > 0 | `L` at the well → `Y` → type `Horse` | `Poof!` and a horse appears on the adjacent walkable cell | 1 gold spent, horse placed | — | UNTESTED |
| H-23 | **R-34 general check.** Any chained modal: shrine `Visit?` → `Virtue?` → `Mantra?`; a Blackthorn interrogation round; `U` a potion → `On whom?` | run each | **every follow-up prompt shows its text.** Before Batch 18 the second modal in any chain rendered with an empty prompt row | — | each cancels normally | UNTESTED |

---

## Group 4 — Overworld, town and NPCs

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-24 | Debug → Teleport → Britannia | move around | day/night, wind bar, moon marks all change | clock advances | — | UNTESTED |
| H-25 | Teleport → Britain | observe | NPCs visible and moving on schedule | — | — | UNTESTED |
| H-26 | Beside an NPC | `T` → name / job / bye | **the conversation survives more than one keypress** (R-01) | — | Mic exits cleanly | UNTESTED |
| H-27 | Town | `L` at a sign; `S`earch; `O`pen a door; `J`immy a locked door; `K`limb; `P`ush furniture | each reports its own authoritative result | as each command implies | direction prompts cancel free | UNTESTED |
| H-28 | Town | enter a blacksmith, buy an item, back out one level at a time | the world view and Exploration verbs return (R-01) | gold/inventory change | — | UNTESTED |
| H-29 | Repeat H-28 | at an inn (Rest), a healer (Heal), a tavern (Rations + Rumour), a reagent shop | same | same | — | UNTESTED |
| H-30 | Lord British's castle basement, Gorn's brazier | `S`earch | the keys are **findable** (R-33: the basement floor byte fix) | keys granted | — | UNTESTED |
| H-31 | A shrine | Visit → Virtue → Donate | the sequence completes and **the game is still playable afterwards** (R-01, Y-25) | shrine bitmap updated | — | UNTESTED |

---

## Group 5 — Combat, loot and the Batch 2 anchors

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-32 | Debug → Max Party, Max Resources, Equip Best Gear | trigger an overworld encounter | an arena opens | — | — | UNTESTED |
| H-33 | In combat | move; `A`ttack with the reticle; `F`ire; `C`ast a combat spell | each resolves with a visible result; the `F`ire projectile flies cell by cell as a sub-cell dot, never a tile write (Y-04) | HP/ammo change | reticle cancels free | UNTESTED |
| H-34 | In combat | `R` → select a weapon | **the weapon actually changes, and the character then attacks with it** (R-06 + the `CombatActor` cache resync) | equipment changed | closing the picker spends the combat action | UNTESTED |
| H-35 | Win the fight | observe | **the arena stays open** | — | — | UNTESTED |
| H-36 | In the open arena | `O`pen the chest, `S`earch it, `G` + direction repeatedly to zero | each `G` names an item | pile drains to 0 | — | UNTESTED |
| H-37 | **ANCHOR 1b** — after H-36 | observe the emptied cell | **plain arena floor — no blue square, no leftover symbol** (R-02) | — | — | UNTESTED |
| H-38 | **ANCHOR 2** — during H-36 | before each `G`, note the visible icon | **the message names the same item the icon showed** (R-04, two-layer composition) | — | — | UNTESTED |
| H-39 | **ANCHOR 1a** — exit via Mic | look at the encounter coordinate on the overworld | **no blue tile-1 cell** (R-03, `gameplay_parity` mismatch 59) | — | — | UNTESTED |
| H-40 | After H-39 | save, reload | **loot left behind is still there** (R-14) | `objects_` restored | — | UNTESTED |

---

## Group 6 — Items, magic and the views

| # | Setup | Input | Expected visible result | Expected state change | Cancel path | Result |
|---|---|---|---|---|---|---|
| H-41 | Carrying a spread of tools | `U`se | **every owned tool listed with the correct name; "Grapple" absent; the Amulet present** (R-07/R-08). Pocket Watch absent is expected | — | Mic closes the picker | UNTESTED |
| H-42 | Party with a healthy member | `U` a **Blue** potion on them | exactly `Item: Potion` and **no second line** — in particular **not** `No effect!` (R-21) | potion spent | — | UNTESTED |
| H-43 | Poison a member (Green potion), then | `U` a **Red** potion on them | `Item: Potion` then `Poison cured!` — this is the control for H-42 | poison cleared | — | UNTESTED |
| H-44 | During H-43 | watch the screen | **the flash/inversion happens BEFORE the `Poison cured!` line** (`CAST.OVL 0x139b` precedes `0x13a1`) | — | — | UNTESTED |
| H-45 | Any potion | `U` it and **cancel** the "On who" picker | `Item: Potion`, the count **down by one**, and **no flash** — the original's own behaviour | potion spent | this *is* the cancel path | UNTESTED |
| H-46 | Carrying all eight scrolls | `U` each in turn | each echoes the bare word `Scroll`, then that scroll's own line (`Light!`, `Wind change!`, `Protection!`, `Negate magic!`, `View!`, `Summon Daemon!` + `Not here!`, `Resurrection!`, `Negate time!`). **No scroll echoes its own name** (R-21) | scroll spent | — | UNTESTED |
| H-47 | In combat | `U` a scroll, then a potion | same rules as H-42/H-46 inside the arena | — | — | UNTESTED |
| H-48 | World, at a locked door | cast **An Sanct**; then **In Por** on the overworld; then **An Ex Por** at a town door | each raises `Direction?`, echoes the direction, and applies (R-11) | door/position change | Cancel refunds nothing — the reference consumes first | **PASS (Batch 5)** |
| H-49 | Carrying gems | `V` | **photograph.** A legible map, the gem count decremented, and closing charges exactly one turn (`VIEW_EFFECT`/`VIEW_RESULT`) — ANCHOR 3 / R-17 | gem spent, 1 turn | any key closes | UNTESTED |
| H-50 | At a crystal ball | `L` at it | **KNOWN OPEN — D-11/R-25.** Today a fabricated `Peer into it?` yes/no appears and answering Yes does nothing at all. **Record what you see; do not file a new ticket** | none | — | UNTESTED |
| H-51 | Cast Wis An Ylem, or read an In Quas Wis scroll | observe | the map reveals for ~1.1 s and input is swallowed for the same window (R-12) | 1 turn | — | UNTESTED |
| H-52 | `U`se the Spyglass at night | observe | the zodiac view draws stars/signs/Shadowlord lines and closes on any key, charging no turn (R-13). *Expected residual: the view is clipped by the 9 px strips — D-12* | none | any key | UNTESTED |
| H-53 | Any party | `M`ix a spell | the spell list opens and the mix resolves. *Expected residual: quantity is always 1 — D-6* | reagents spent | — | UNTESTED |

---

## Group 7 — The (Z)-stats page family (Batch 14 / R-22)

*Setup: ≥3 members with deliberately different stats; some gold/food/keys/gems/torches; a few reagents; a couple of mixed spells; ≥8 distinct armaments; one member fully equipped, one with nothing equipped.*

| # | Input | Expected visible result | Result |
|---|---|---|---|
| H-54 | `Z` | the **member picker**, with the roster highlight — `select_player` runs first | UNTESTED |
| H-55 | move to member 2, Enter | the panel title becomes **that member's name**; body is their **Stats** page: `M`/`F`, `Lv-N`, class; the centred health word; `Str=`/`HP:`, `Int=`/`HM:`, `Dex=`/`Ex:`, `Magic:` | UNTESTED |
| H-56 | `1`, `2`, `3` | each shows **its own member's distinct numbers**. `HM` is max HP and `Ex` is experience | UNTESTED |
| H-57 | page right once | `Arms` page, equipped items **by real name**. The unequipped member reads `(None ready)`, not six blank rows | UNTESTED |
| H-58 | read the Arms page of the fully-equipped member | every line a genuine item name. **Nothing may read `Equipment 12` or `Item 30`.** A helm named as a shield means the Batch 14 off-by-one is back | UNTESTED |
| H-59 | page right past the last member's Arms; also press `0` from any page | the **`Equipment`** page: `Food:`, `Gold:`, then `Keys.......`, `Gems.......`, `Torches....`. A `Grapple` line only when one is carried | UNTESTED |
| H-60 | page right four more times | **Reagents**, **Spells**, **Items**, **Armaments**, in that order. Each row is a 2-digit count, `-`, then the name | UNTESTED |
| H-61 | inspect the lists | a reagent owned **none** of is **absent**; an empty list reads `(None owned!)` | UNTESTED |
| H-62 | the **Items** page | carried scrolls/potions appear **with** counts; regalia/Shard/Spyglass/Sextant/Black Badge/Wooden Box appear **without** a number | UNTESTED |
| H-63 | **Armaments** with >7 items: press down, then left/right | the detail line reads `1-7 of N` with `v`; down scrolls by one and the marker becomes `^ v`; **left/right still change page, not scroll** | UNTESTED |
| H-64 | page right from Armaments; page left from member 1's Stats | wraps to member 1's Stats / to Armaments. **Never a blank page** between the last member and Provisions | UNTESTED |
| H-65 | page around the ring twice, then press `i`, `p`, `k`, `e`, `s` | the modal stays open; **nothing happens** — no torch lit, no gem spent, no prompt armed behind the panel | UNTESTED |
| H-66 | watch the clock and viewport during H-65 | the avatar does **not** move and the day/time does **not** advance | UNTESTED |
| H-67 | press **Space**; then repeat and press **Mic** | the sheet closes and the world HUD returns; the **very next** trackball nudge moves the avatar and advances one turn | UNTESTED |
| H-68 | press `Z` again | it opens the **picker**, not the last page; confirming opens a **Stats** page with no leftover scroll position. Repeat five times — nothing drifts | UNTESTED |
| H-69 | Developer → poison or kill a member, then view their Stats | the centred health line reads **`Poisoned`** / **`Dead`**; the rest renders normally | UNTESTED |

---

## Group 8 — Transport

| # | Setup | Input | Expected | Result |
|---|---|---|---|---|
| H-70 | Debug → Transport → Ship | `B`oard, then `Y`ell | **HOIST/FURL, not a word prompt** (R-19); the ship sails with the wind | UNTESTED |
| H-71 | Aboard | `X`-it; board a horse, a carpet, a skiff | the avatar sprite changes each time | UNTESTED |
| H-72 | Aboard a ship | save, reload | transport mode and hull survive | UNTESTED |

---

## Group 9 — The dungeon (the big one)

*Setup for all: Debug → Teleport → Dungeon, and a **lit** torch unless the row says otherwise.*

### 9a — Presentation and authored art (R-05, Batches 9/9C)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-73 | Deceit floor 0, standard entry | `DUNGEON_SESSION_STATE active=1` **and** a usable view. **Photograph it.** Textured masonry with baked floor speckle and ceiling — **not** flat wedges, **not** outlines. Wireframe geometry means a stale card, not a code fault | UNTESTED |
| H-74 | Torch **out**, no light spell | the viewport is **entirely black**. Ignite: the corridor appears **on that keypress** | UNTESTED |
| H-75 | Read the strips | above the viewport: **`L1`**, not blank sky. Below: **`Dir:` + the facing**, not `Wind: --` | UNTESTED |
| H-76 | Turn left then right (trackball, or `A`/`D` with Movement Mode **ON**) | the lower band changes on each turn | UNTESTED |
| H-77 | Stand so a door or room entrance is **two cells ahead** down an open corridor | the corridor **stops** at it and shows a dead end with a door panel. Then stand **on** the door: the two nearest side slices drop away | UNTESTED |
| H-78 | Stand so a ladder or chest is **one or two cells ahead** | it is **visible from there**, not only underfoot | UNTESTED |
| H-79 | Let a wandering monster approach | drawn from **three cells out**; a ceiling-lurking type appears **high in the frame** | UNTESTED |
| H-80 | Look at the depth rings | the four rings **abut** with no seams or black gaps (left edge 16→96, right 96→176). The only black in a lit view is the vanishing point | UNTESTED |
| H-81 | Repeat H-77 looking at the art | a **dead end** and a **door** are visibly different images; the front wall reads as one symmetric piece with no overlap or gap at the centre column | UNTESTED |
| H-82 | Find a **passage**, an **alcove** and a **side door** on side walls | all three visually distinct from plain masonry and from each other, each on the **correct side** — walk past and confirm | UNTESTED |
| H-83 | Visit dungeons of different variants (1/4/5 → DNG3 grey; 6/7 → DNG2 red; rest → DNG1 olive) | the wall appearance changes, **on entry**, not after a step | UNTESTED |
| H-84 | Repeat H-79 against the art | a recognisable sprite with clean edges — **no** opaque rectangle around it, **no** holes punched through its dark interior | UNTESTED |

### 9b — Dungeon runtime and input (Batch 9B)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-85 | Deceit, torch lit | turn left, turn right, advance, back up **with the trackball**: facing changes on each turn, the cell changes on each move. Then Enter = **Turn Around**: the `Dir:` band flips 180°, it does **not** pass the turn | **PASS (Batch 9B)** |
| H-86 | Torch burnt out or entered without one | the viewport is black; press `I` — **a torch lights and the corridor appears on that keypress**. Before 9B this answered `"What?"` | **PASS (Batch 9B)** |
| H-87 | Still dark | turn left twice, advance once: the viewport stays black but the `Dir:` band still changes. Relight with `I`: the corridor shown is the one those commands moved you to | UNTESTED |
| H-88 | A cell with a ladder **up and down** | `K` shows `Klimb-U/D-`; answering down goes one level **deeper**. On a one-way ladder `K` resolves with **no prompt** | **PASS (Batch 9B)** |
| H-89 | Any corridor cell | `S` shows `Dir-`; answering **down** (Here) searches the cell **underfoot**. `H` opens camp hours; `D` drinks or answers `"No fountain here."`; a digit sets the active player | UNTESTED |
| H-90 | Standing in Deceit, reached **from another named map** (Serpent's Hold is the reported case) | the right-hand location strip reads **`Deceit`**; leaving returns it to the surface name. Check one more dungeon | UNTESTED |

### 9c — Dungeon fields and rooms (Batches 12/12B)

| # | Setup | Expected | Result |
|---|---|---|---|
| H-91 | Destard, a corridor cell whose **forward** cell is plain empty floor, level-4+ caster with mana | cast `In Flam Grav`: the log reads `Cast` (not `Failed!`) **and** a field is visible ahead — dense short horizontal **bright green** sparkle strokes. It shimmers on every redraw; that is the original's per-redraw re-roll | UNTESTED |
| H-92 | After H-91 | **only** the cell directly ahead is affected — not the sides, not the cell beyond | UNTESTED |
| H-93 | After H-91 | turn away and back: the field is still there. Pass several turns: unchanged (there is no field duration) | UNTESTED |
| H-94 | Repeat H-91 with `In Nox Grav`, `In Zu Grav`, `In Sanct Grav` | each seeds its own one-cell field, each visible. `In Zu Grav`/`In Flam Grav` draw **EGA 10 bright green**; `In Nox Grav`/`In Sanct Grav` **EGA 9 bright blue**. **Only two colours across four spells is correct** | UNTESTED |
| H-95 | Face a wall or an occupied cell | cast `In Flam Grav`: **`Failed!`**, nothing drawn, nothing changed | UNTESTED |
| H-96 | Walk **into** an In Flam Grav field; then into an In Zu Grav field | `Fire!!` and HP loss, field **still there**. `Sleep spell!`, members may sleep, field **disappears** — a sleep field consumes itself, fire does not | UNTESTED |
| H-97 | Walk the corridors of **Wrong** (30 authored fields) or **Covetous** (25) | authored magic fields appear at cells you never cast into | UNTESTED |
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
| H-107 | Movement Mode ON | `W` advances, `S` backs up, `A`/`D` turn. Walk into a wall: the blocked response | UNTESTED |
| H-108 | Find stairs | `K`limb down: the depth readout changes and the top band reads **`L2` on the descent itself** | UNTESTED |
| H-109 | Walk into a pit; walk into a field | the damage message and the feature art | UNTESTED |
| H-110 | Corridor | `S`earch, `O`pen, `G`et, `J`immy each report their own result | UNTESTED |
| H-111 | Corridor | cast `Uus Por` / `Des Por`; then `R`eady — **it applies and charges no turn** (R-06) | UNTESTED |
| H-112 | Corridor | cast an ordinary spell (e.g. `In Lor`): **the same flash/inversion the overworld gives**. Then cast `Grav Por` or `Vas Flam` as the negative control — weapon-spells stay silent, above ground and below | UNTESTED |
| H-113 | Trigger a dungeon encounter and win | **the return is to the same cell and facing** (Y-22) | UNTESTED |
| H-114 | In a dungeon | `Alt+M` → close; `Alt+D` → Back | **the dungeon view returns both times** | UNTESTED |
| H-115 | In a dungeon | save, reload | position, facing, revealed cells, wanderer **and any field you cast** resume exactly (R-15; fields ride in `DungeonState::cells`) | UNTESTED |
| H-116 | Descend past floor 7 | the Underworld transition | UNTESTED |
| H-117 | Walk out at the level-1 entrance | the surface world view and Exploration verbs return | UNTESTED |

---

## Group 10 — Quest, Blackthorn and persistence

| # | Setup | Input | Expected | Result |
|---|---|---|---|---|
| H-118 | Debug → Quest → grant a shard | `U`se it in a Flame room | the shard's effect fires (R-08) | UNTESTED |
| H-119 | At a dungeon entrance | `Y`ell a word of power | the quake and the flag toggle | UNTESTED |
| H-120 | Blackthorn's palace, capture sequence | run the whole ceremony | the interrogation asks its questions, each with a visible **`Your response?`** row (see H-23), and the scene reads as a distinct scene, not the ordinary Palace lobby (R-32) | UNTESTED |
| H-121 | During H-120, a companion is executed | observe the active character | **the previously-set active character is NOT re-pointed** — if it named the executed companion or anyone after them in the marching order, it now names someone else. That is `BLCKTHRN 0x03ae-0x04d4`'s own behaviour. **Do not file it** | UNTESTED |
| H-122 | During H-120 | watch the pacing | **KNOWN OPEN — D-10/Y-32.** The scene may feel faster and more collapsed than the original. **If you can record video against original footage, that recording is the missing evidence.** Capture it rather than filing a ticket | UNTESTED |
| H-123 | Falsehood / Abbey chain | run it | the chain completes and its quest objects register | UNTESTED |
| H-124 | Overworld, town, dungeon, aboard a ship, mid-quest | save and reload in each | after each: position, party, inventory, equipment, time, transport, world objects, hidden/search objects, and the UI mode and renderer all match | UNTESTED |

---

## Group 11 — reachability and feedback spot-checks

*Added in Batch 18 after the reconciliation found resolved findings with no device row of their own. Short rows, runnable alongside the groups above.*

| # | Setup | Input | Expected | Audit ID | Result |
|---|---|---|---|---|---|
| H-125 | At the harpsichord (Lord British's castle / a tavern) | press digits `0`–`9` | each plays its note; the digits are intercepted **before** Set Active Player at the instrument, and only there | R-20 | UNTESTED |
| H-126 | World, party ≥2, **away** from a harpsichord | press `1`…`6` | `Set Active Plr:` and the named member becomes active. A digit beyond the party size is bounded, not honoured | Y-20 | UNTESTED |
| H-127 | Inside a selection or target modal | `Alt+M` → close | the System Menu opens over the modal and, on close, the **same modal is still open and still answerable** | Y-27 | UNTESTED |
| H-128 | Town, on a bed | walk onto it | the auto-sleep turn fires from the town turn handler — no player-visible command, no `What?` | Y-26 | UNTESTED |
| H-129 | From the frontend menu | item 7, Developer | gameplay opens on whatever `INIT.GAM` state booted, and the Developer screen is usable | Y-18 | UNTESTED |
| H-130 | Long transcript (fight or a long conversation) | `Shift+Up` several times, then let new text arrive | the view **stays** where you scrolled it; it does **not** yank to newest. `Shift+Down` to the bottom re-enables auto-follow | R-31 (enhancement E-1) | UNTESTED |
| H-131 | Debug → preset **Combat** | apply it | it does **not** silently engage Set Active Player, and other party members are **not** auto-passed | R-24 | UNTESTED |
| H-132 | Debug → Default Entrance at a location with a basement (e.g. Lord British's castle) | enter | you arrive on the **ground floor**, not the basement — the ordinal-vs-signed-z fix | R-27 | UNTESTED |
| H-133 | Debug menu, all pages | browse | every setter shows a **label**, not a raw ordinal, and the current value is readable without moving the cursor onto it. Special Items and Quest Items are reachable and named | R-28, R-29 | UNTESTED |
| H-134 | Debug → any preset | select it | the effect list is **disclosed** before applying and a confirm gate is required. The Transport preset does **not** silently rig HMS Cape | R-30 | UNTESTED |
| H-135 | Trigger an earthquake event (Yell a word of power at a dungeon entrance, H-119) | observe | the **Quake** shake renders | Y-04 | UNTESTED |
| H-136 | Combat: land a killing area-effect blow, or step a poisoned member through a turn | observe | **CellExplosion** draws on the cell; **PoisonTick** flashes the poisoned member's roster row by inversion, not by a tile write | Y-04 | UNTESTED |
| H-137 | Enter a Refuge; separately, let a troll ambush you at a bridge | observe | the **Refuge** and **TrollSneak** narrative scenes both pace out as sequences with readable text, not as a single flashed frame | Y-04 | UNTESTED |
| H-138 | Any combat where a summoned or field effect kills the last enemy while a member is poisoned | observe | the sprite on the cell stays put for the whole choreography (`under_tile`), and no committed state change is deferred past it | Y-04 (#243) | UNTESTED |
| H-139 | Combat, a member with **nothing** in hand | `R`eady | **KNOWN OPEN — D-8/Y-28.** Today a disabled `(None available)` picker opens instead of the action being charged immediately. Record what you see; do not file | Y-28 | UNTESTED |
| H-140 | Combat, `R`eady a ring that vanishes on use | observe | **KNOWN OPEN — D-8/Y-28.** `Ring vanishes!` does not close the picker early. Record; do not file | Y-28 | UNTESTED |

---

## Result tally

| Group | Rows | PASS | FAIL | UNTESTED |
|---|---|---|---|---|
| 1 — boot / input | 9 | 0 | 0 | 9 |
| 2 — Y-series closeout | 7 | 0 | 0 | 7 |
| 3 — wishing well | 7 | 0 | 0 | 7 |
| 4 — overworld / town | 8 | 0 | 0 | 8 |
| 5 — combat / loot | 9 | 0 | 0 | 9 |
| 6 — items / magic / views | 13 | 1 | 0 | 12 |
| 7 — Z-stats | 16 | 0 | 0 | 16 |
| 8 — transport | 3 | 0 | 0 | 3 |
| 9 — dungeon | 45 | 3 | 0 | 42 |
| 10 — quest / persistence | 7 | 0 | 0 | 7 |
| 11 — reachability / feedback spot-checks | 16 | 0 | 0 | 16 |
| **Total** | **140** | **4** | **0** | **136** |

The four PASS rows are the only hardware evidence on record: H-48 (Batch 5's
three world-cast checks, all passed first attempt) and H-85/H-86/H-88 (the Batch
9B dungeon-runtime gate). Everything else is owed.

**Alpha 2 cannot be declared validated until this table has no UNTESTED rows.**
