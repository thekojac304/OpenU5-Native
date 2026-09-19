# Alpha 2.0 Batch 4 — Root-Cause & Behavioral Adjudication (READ-ONLY pass)

**Date:** 2026-09-19
**Scope:** R-09 (dropped modal responses) and R-10 (NPC-initiated Talk / Shop) only.
**Status:** adjudication only. No source file was modified, no test was added, no commit was made.

**Authoritative build verified:** `native/core/build-batch1-control` (g++ / w64devkit / Ninja,
`OPENU5_ENABLE_DEVELOPER_TOOLS=ON`) — `ctest` reports **61 total, 60 pass, 1 fail**. The sole
failure is `#39 gameplay_parity` (the pre-existing R-21 / mismatch-2034 case), unrelated to
Batch 4.

**Reproduction harnesses** (session scratchpad, not in the tree):
`scratchpad/b4/repro.cpp` — current behaviour; `scratchpad/b4/preview.cpp` — proposed shape.
Both link the authoritative `libopenu5_core.a`.

---

## A. Executive adjudication

**The audit is directionally right on R-10 and substantially wrong on R-09.**

Four findings change the shape of the batch:

1. **R-09 is not "the answer evaporates" — it is a hard softlock.** `commands.cpp:628` gates
   *every* command with `AwaitingResponse` while any `BlackthornSession` flag is live
   (`shrine>=0 || password || tribute || arrest`). Because no `BlackthornAction` is ever
   dispatched, the flag is never cleared. Reproduced: after answering a guard tribute prompt,
   `Pass` returns `AwaitingResponse` forever. The player is locked out of the whole game, not
   merely denied one effect. Severity is understated in the ledger.

2. **The five flows do *not* share one root cause.** Four of them (Blackthorn, GuardPassword,
   GuardTribute, GuardArrest) share exactly one: a missing
   `UiRequestId → CommandKind::BlackthornAction` translation. **FountainDrink is a different
   defect entirely** — the reference fountain is not a yes/no prompt at all. It is
   `pickMember("Who will drink?")` with pure flavour text and **no command, no state change,
   no turn**. Native opens `begin_yes_no(FountainDrink,"Drink?")`, and the strings
   `"a gurgling fountain!"`, `"Refreshing..."`, `"Incapacitated!"`, `"None!"` do not exist
   anywhere in `native/`. Adding a `modal()` branch "following the WellDrop pattern" would
   produce a wrong interaction, not a fix.

3. **Both analogies the audit cites as correct are themselves broken.**
   - `CrystalBall`: reference uses `pickCommandChar` (a character picker), not Y/N.
     `alpha_runtime.cpp:644` dispatches `CommandKind::CrystalBall` with the default
     `Command::member == -1`, and `look.cpp:39` rejects that outright. **Executed: zero events,
     zero HP change — always `Rejected`.**
   - `WellDrop`: the "No" answer dispatches nothing (reference calls `dropCoin(false)`, which
     prints `"No\n"`), and native passes `cancel_means_no=false` where the reference uses
     `yesno-esc` (ESC = No). It "works" on Yes only because `member == -1` happens to be truthy
     in `look.cpp:37`.

   `TrollToll` and `WellWish` are the only two genuinely correct precedents.

4. **R-10 is accurate, and the fix is smaller than expected.** `CommandKind::BeginConversation`
   already matches on `n.schedule.slot == cmd.member` with no adjacency or floor test
   (`dialogue_orchestration.cpp:132`), **and already routes `dialog 0x81..0x88` into
   `begin_shop()` via `DialogueHandoff::Shop`** (`:139`). Executed: one
   `BeginConversation(member=slot)` serves both Talk *and* Shop initiation, yields correct
   sessions and correct Batch-1 return modes. No new command, no new session entry point.

---

## B. R-09 per-flow evidence table

| | **BlackthornPrompt** | **GuardPasswordPrompt** | **GuardTributePrompt** | **GuardArrestPrompt** | **FountainDrinkPrompt** |
|---|---|---|---|---|---|
| **Reference behavior** | `main.ts:2706` `askText(question + "\n\nYour response?\n:", max 0xE)` → `submitInterrogationResponse(text)`. Mantra match destroys the shrine (`karma−5`, sacrifice if `living>1`); miss advances round (≤3) and re-prompts; `round≥1 && living>1` costs 2 clock units. | `main.ts:2718` `askText(GUARD_PASSWORD_CHALLENGE, max 0xE)` → `submitGuardPassword(text)`. `guardDemand` truncates to 4 chars, compares `"IMPE"`. Hit → `"Pass, friend!"`, **grants nothing**, re-challenges next turn. Miss → `runCapture()` (TOWN 0x12ae). | `main.ts:2501` `hud.message` then `type:"yesno"` → `resolveGuardTribute(yes)`. `guardDemand` debits `10 × living` (Minoc loc 5: `gold − gold/2`). Paid → `party-changed`, no text. Refused **or accepted with insufficient gold** → `ret 1` → arrest prompt. | `main.ts:2553` `hud.message('\n"Thou art under arrest!"\n\n"Wilt thou come quietly?"\n\n:')`, `type:"yesno"` → `resolveGuardArrest(yes)`. Y → `guardArrestJail`: loc 4 (Yew), (25,4), keys 0, hour 8; echo baked into core. N → `"Then defend thyself, rogue!"` + `arrestAlarm` + `hostileNpcAttack(npcSlot)`. | `main.ts:2668` `hud.message("a gurgling fountain!")` then **`pickMember("Who will drink?")`**. Chosen member `'D'`/`'S'` → `"Incapacitated!"`, else `"Refreshing..."`. **Pure flavour: no HP, no state, no turn, no core call.** |
| **Native current behavior** | `ui_session.cpp:945` `enter_shrine_mode(); begin_text(Blackthorn, e.text, 14)`. Answer → `ModalResponse` → `AlphaRuntime::modal` has no branch → **discarded**. `blackthorn.shrine` stays ≥0 → every command `AwaitingResponse`. | `ui_session.cpp:946` `begin_text(GuardPassword, e.text, 14)`. Prompt text is **byte-exact** (from `blackthorn.cpp:24`). Answer → discarded. `blackthorn.password` stays true → softlock. ESC closes the modal and dispatches nothing → **softlock with the prompt gone**. | `ui_session.cpp:947` `begin_yes_no(GuardTribute,"Pay tribute?",false)`. `e.note` (the toll, or `−1` for Minoc charity) is **ignored**. Answer → discarded; gold unchanged; `tribute` stays true → softlock. | `ui_session.cpp:948` `begin_yes_no(GuardArrest,"Go quietly?",false)`. Answer → discarded; no jail, no combat; `arrest` stays true → softlock. | `ui_session.cpp:952` `begin_yes_no(FountainDrink,"Drink?",false)`. Answer → discarded. No prompt text, no roster pick, no flavour strings anywhere in the tree. No softlock (no core machine). |
| **Exact divergence point** | `alpha_runtime.cpp:628-649` — no `UiRequestId::Blackthorn` branch | same, no `GuardPassword` branch | same, no `GuardTribute` branch | same, no `GuardArrest` branch | **`ui_session.cpp:952` — wrong modal kind**, *and* no effect owner. Divergence begins one step earlier than the other four. |
| **Answer mapping** | text → `BlackthornAction::Answer`, `cmd.text/text_length`; `member` unused | text → `BlackthornAction::Password`, `cmd.text/text_length`; `member` unused (the loc-18 branch of `guard_demand` ignores `agree`) | yes/no → `BlackthornAction::Tribute`, `cmd.member = yes?1:0` | yes/no → `BlackthornAction::Arrest`, `cmd.member = yes?1:0` | member index → flavour string by `characters[m].status`; **no CommandKind exists or is needed** |
| **Cancel behavior** | **Cancel ≠ abort.** Reference `askText.cancel → onText("")`; an empty response is a mantra miss and advances the interrogation. Native currently dispatches `accepted=false`, which `modal()` drops. | **Cancel ≠ abort.** `onText("")` → `guardDemand("")` → `ret 1` → **capture**. Native's ESC strands the machine — worse than the accepted path. | **Not cancellable.** Reference `type:"yesno"` ignores ESC. Native `cancel_means_no=false` already matches — **verified MATCH**. | **Not cancellable.** Same. Native already matches. | **Cancel = "None!"**, no effect. Native has no cancel path because it has the wrong widget. |
| **Required retained context** | None from the UI. `BlackthornSession{shrine,round,living}` is core-owned and survives. Only the *request identity* must survive prompt→answer. | None. `BlackthornSession::password` is core-owned. | None. `npc_slot` is captured by `talk_guard` into `BlackthornSession::npc_slot` (`blackthorn.cpp:26`) and carried to the arrest escalation by the core. The UI must retain **nothing**. | None. `npc_slot` already stored (`blackthorn.cpp:32` re-finds the actor by slot for `town_attack_commit`). | The chosen **member index**, which needs the roster — i.e. `AlphaRuntime`-owned `selections_`. |
| **Likely owning layer** | `UiSession::finish_modal` (+ cancel route) | `UiSession::finish_modal` (+ cancel route) | `UiSession::finish_modal` | `UiSession::finish_modal` | `UiSession::consume` raises `OpenPartySelection`; `AlphaRuntime::modal` resolves the member and prints — mirroring `main.ts` |

**Turn cost.** None of the four Blackthorn flows consumes a world turn. `execute()` returns at
`commands.cpp:627` before the turn machinery, matching the reference (`applyEvents` of a
core-returned event list). The only clock movement is the interrogation's internal
`advance_clock(...,2,...)` in `blackthorn.cpp:40`, which is reference behavior.

---

## C. R-10 event-flow evidence

### NpcInitiatesTalk

| | |
|---|---|
| **Producer** | `blackthorn.cpp:65`, inside `blackthorn_turn_effect(CommandEffect::Tribute)`, reached from the town turn tail `commands.cpp:288`. Gated by `adjacent()` (manhattan 1, ai>3), `location != 18`, `dialog != 255`, `dialog < 128`, and a live `talk_script_for(registry, loc, dialog)`. |
| **Payload semantics** | `e.npc` is `const NpcActor *` — a **borrowed pointer into `c.actors->actors[]`**, documented as "borrowed identity for synchronous delivery" (`movement.h:56`). Not a persistent ID. `schedule.slot` inside it is the stable identity. |
| **Current consumer path** | `AlphaRuntime::consume_event` (`alpha_runtime.cpp:253`) forwards to `ui_->consume(e)`; `UiSession::consume`'s switch falls to `default: break`. |
| **Exact drop point** | `ui_session.cpp:979` (`default: break;`) and the absence of any branch in `alpha_runtime.cpp:253-279`. |
| **Correct production initiation path** | `CommandKind::BeginConversation` with `cmd.member = npc->schedule.slot`. `dialogue_orchestration.cpp:132` matches on slot alone (no adjacency, no floor), which is exactly what an NPC-initiated hail needs. **Executed:** `BeginConversation(member=5)` → `AwaitingResponse`, `session.active=1`, `base_mode=Dialogue`. |
| **Mode/return consequences** | `DialogueOutputKind::Prompt` captures `dialogue_return_mode_ = world_return_mode(base_mode_)` **before** `set_base_mode(Dialogue)` (`ui_session.cpp:864`); `DialogueEventKind::Ended` restores it. Verified end-to-end: Exploration → Dialogue → Exploration. Batch 1's R-18 register already covers it. |

### NpcInitiatesShop

| | |
|---|---|
| **Producer** | `blackthorn.cpp:62`, same tail. Gated by `dialog ∈ [128,252]`, `shop_is_open(times,hour)`, and the mounted-merchant refusal `(transport_tile&252)==16 && dialog!=131`. |
| **Payload semantics** | Identical: borrowed `NpcActor *`; `schedule.slot` is the identity. |
| **Current consumer path / drop point** | Identical — `ui_session.cpp:979` default arm; no `AlphaRuntime` branch. |
| **Correct production initiation path** | **The same `BeginConversation(member=slot)`.** `dialogue_orchestration.cpp:139` sends `dialog 0x81..0x88` to `DialogueHandoff::Shop` → `begin_shop(c, npc)` *before* binding any TLK script, so no greeting leaks. **Executed:** `BeginConversation(member=6)` → `shop.phase=3 (Greeting)`, `base_mode=Shop`. |
| **Mode/return consequences** | `UiSession::consume(Shop)` captures `shop_return_mode_ = world_return_mode(base_mode_)` on first entry only, and restores it on `ShopEventKind::Exited`/`Closed`. Batch 1 R-01/R-18 already covers it. |

**Emitter/consumer range mismatch (theoretical; flag only).** `blackthorn.cpp:59` emits
`NpcInitiatesShop` for `dialog 128..252`, but `BeginConversation` only hands off `0x81..0x88`.
Outside that window native would fall through to `talk_script_for` → `"Funny, no response!"`,
where the reference (`SHOP_TYPES[dlg]` undefined) prints nothing. The reference documents the
same wider gate and states no factory NPC falls outside `0x81..0x88`; unreachable today, but the
future consumer should gate on `0x81..0x88` to stay faithful.

---

## D. UiSession vs AlphaRuntime adjudication

**The actual rule the codebase establishes** (not the one the audit assumes):

- `UiSession::finish_modal` (`ui_session.cpp:400-431`) translates a modal answer into a
  `Command` **when the translation is a total function of `(UiRequestId, UiModalValue)` with no
  runtime-owned data**. Precedents: `TownExit → Exit/DeclineExit`,
  `Dialogue → DialogueText/Yes/No/EndConversation`, `RestHours → Rest(hours)/RestCancel`,
  `YellText → Yell(text)`, `Shop → ShopAction`.
- `AlphaRuntime::modal` (`alpha_runtime.cpp:628-649`) translates **when runtime-owned data is
  required**: the `selections_[]` id table, `pending_order_from_ / pending_ready_member_ /
  pending_use_item_ / pending_combat_spell_`, `status_member_`, the multi-step `shrine_virtue_`
  buffer, `active_member(game_)`.

`TrollToll`, `WellDrop`, `WellWish` and `CrystalBall` sit in `AlphaRuntime::modal` while needing
no runtime data. They are **exceptions, not the rule** — and two of the four are defective
(§A.3), which is decisive evidence against copying them.

### R-09 ruling

**Blackthorn / GuardPassword / GuardTribute / GuardArrest → `UiSession::finish_modal`.**
Each is a pure `(request, answer) → Command{BlackthornAction, item=action, member=agree, text=…}`
map; the whole surviving context lives in the core's `BlackthornSession`. This matches the
`TownExit`/`RestHours`/`YellText` precedent exactly, and it puts the translation inside
`openu5_core`, where the RED tests can drive **production code** with no new seam and no mirror.
The cancel semantics (§B) force the same conclusion: `cancel_modal` (`ui_session.cpp:355`)
currently dispatches `accepted=false` with the text buffer already cleared, and
`AlphaRuntime::modal` early-returns on `!accepted`. The two text prompts must resolve-on-cancel
with an empty string, which is a `UiSession` routing change regardless of where the `Command` is
built.

**FountainDrink → split, mirroring the reference.** `UiSession::consume` must raise
`UiIntentKind::OpenPartySelection` with a fountain request id instead of `begin_yes_no`;
`AlphaRuntime::modal` resolves `selections_[index].value` and emits the flavour line, because the
effect reads `characters[m].status` and `UiSession` is presentation-only and does not own
`GameState`. This is precisely what `main.ts` does.

### R-10 ruling

**`AlphaRuntime::consume_event`, deferred — not `UiSession`.**

1. `UiSession::consume` never issues a gameplay command in the entire file. Every command it
   emits comes from `handle_input()` (a user action) or `finish_modal()` (a modal answer).
   `NpcInitiates*` is neither: it is the core asking the frontend to run another command.
   Routing it through `UiSession` would invent a new responsibility for a class whose header
   says "presentation state only".
2. The reference draws the line in the same place: `main.ts`'s `applyEvents` calls
   `talkConsole.start(...)` / `startShopConsole(...)`. `AlphaRuntime` *is* `main.ts` here.
3. `UiSession` cannot defer anyway — its `dispatch()` is synchronous, so consuming there has the
   identical re-entrancy problem with none of the ownership benefit.

**It must be deferred, not immediate.** These events are emitted *inside*
`dispatch_world_command`, so an immediate `AlphaRuntime::command()` re-enters the core while the
outer command is on the stack, and nests all of `AlphaRuntime::command`'s pre/post
instrumentation plus `finish_combat_if_needed()` / `schedule_combat()`. The re-entrant call was
measured and the *core* tolerates it (R-10-C: outer `Pass=Success`, inner `AwaitingResponse`,
session opened) — because `blackthorn_turn_effect` returns `true` and `turn()` bails immediately
after the emit. That is a coincidence of one call site, not a contract. The safe drain point is
the tail of `AlphaRuntime::handle()`, next to `synchronize_after_debug(...)`, after
`ui_->handle_input()` has fully unwound — the same shape as the existing `gem_view_active_`
deferral.

---

## E. Reproduction results

Harness: `scratchpad/b4/repro.cpp`, linked against the authoritative `libopenu5_core.a`.
`World::modal()` and `World::consume_event()` are verbatim transcriptions of the current
`AlphaRuntime` bodies; every trigger goes through the real `execute_command()` town-turn tail.
Fixture additions were limited to what the core's own context guards require (a 32×32 walkable
floor, `npc_scratch`, a one-entry `TalkRegistry`) — no private state was patched.

| # | Setup | Input sequence | Reference expected | Current native | Divergence point | Status |
|---|---|---|---|---|---|---|
| R-09-A | Town loc 2, guard type 112 / dialog 0xFF / ai 4 adjacent | `Pass` → `'y'` | gold 500→480, `tribute` cleared, world playable | prompt opens (`YesNo`/`GuardTribute`) ✓; **gold 500→500, `tribute=1`, next `Pass`=`AwaitingResponse`** | `alpha_runtime.cpp:628` | **RED** |
| R-09-A′ | same | `Pass` → `ESC` | ESC ignored, prompt stays live | ESC ignored, prompt stays live | — | **PASS** (already faithful) |
| R-09-B | Town loc 2, guard type 112 / ai 6 | `Pass` → `'y'` | loc 4, (25,4), keys 0, hour 8 | unchanged at loc 2; `arrest=1`; softlocked | `alpha_runtime.cpp:628` | **RED** |
| R-09-B′ | same | `Pass` → `'n'` | defend-thyself + alarm + combat | nothing dispatched | same | **RED** |
| R-09-C | Palace loc 18, badge worn (`time_spell=0x1d`) | `Pass` → `I M P E` ⏎ | `"Pass, friend!"` | prompt text **byte-exact** ✓; no command; `password=1`; next `Pass`=`AwaitingResponse` | `alpha_runtime.cpp:628` | **RED** |
| R-09-C′ | same | `Pass` → `ESC` | `onText("")` → failed password → capture | modal closes, **nothing dispatched, `password=1`** — stranded | `ui_session.cpp:355` + `:628` | **RED** |
| R-09-D | `BlackthornSession{shrine=0,round=0,living=2}`, `BlackthornPrompt` delivered | `M U` ⏎ | round advances or shrine resolves | `TextEntry`/`ShrineSpecial` ✓; no command; next `Pass`=`AwaitingResponse` | `alpha_runtime.cpp:628` | **RED** |
| R-09-E | `FountainDrinkPrompt` delivered | `'y'` | `"a gurgling fountain!"` + roster pick + `Refreshing...` | `YesNo` `"Drink?"`; answer reaches the dispatcher but nothing happens | **`ui_session.cpp:952`** (wrong widget) | **RED** |
| R-09-F | `CrystalBallPrompt` delivered | `'y'` | `pickCommandChar` then vision / 1 HP | `YesNo`; dispatches `CrystalBall` with `member=−1`; **0 events, 0 HP change** → `Rejected` | `alpha_runtime.cpp:644` + `look.cpp:39` | **RED** (out-of-scope observation) |
| R-09-G | `WellDropPrompt` delivered | `'n'` / `ESC` | `dropCoin(false)` → `"No\n"`; ESC = No | No → nothing dispatched; ESC → ignored (`cancel_means_no=false`) | `alpha_runtime.cpp:645`, `ui_session.cpp:951` | **RED** (out-of-scope observation) |
| R-10-A | Town loc 2, dialog 9 / ai 4 adjacent, script registered | `Pass` | conversation opens | `NpcInitiatesTalk` emitted, `e.npc` valid (slot 5, still `&actors[0]`), aiType already degraded 4→1; **`talk.active=0`, mode Exploration**. Control `BeginConversation(5)` → `AwaitingResponse`, active, `base=Dialogue` | `ui_session.cpp:979` / `alpha_runtime.cpp:253` | **RED** |
| R-10-B | Town loc 2, dialog 0x81, shop open | `Pass` | shop console opens | `NpcInitiatesShop` emitted, slot 6; **`shop.phase=Closed`**. Control `BeginConversation(6)` → `phase=3`, `base=Shop` | same | **RED** |
| R-10-C | as R-10-A, consumer calls `BeginConversation` **inside** `consume_event` | `Pass` | — | outer `Pass=Success`, inner `AwaitingResponse`, session opens | — | tolerated, but see §D |

**GREEN-shape preview** (`scratchpad/b4/preview.cpp`, same fixture, proposed translation +
deferred drain): **all 12 expectations met.** Tribute Yes → −20gp, softlock cleared, mode
Exploration. Tribute No → arrest prompt chains cleanly (`YesNo`, `base` still Exploration) → Yes
→ exact `guardArrestJail` mutation (loc 4, 25/4, keys 0, hour 8) → Exploration. Password IMPE →
consumed, softlock cleared. Password ESC-as-empty → consumed, world never stranded. Deferred
`BeginConversation(slot)` → Dialogue and Shop sessions with correct return modes.

**Not reproducible on the host, and why:** the *full* Blackthorn interrogation scene needs the 12
MISCMSG records the device supplies through `ShrineServices::record`; without them
`blackthorn_action` returns `InvalidContext` before printing. R-09-D therefore arms
`BlackthornSession` directly and delivers the real `BlackthornPrompt` event — a fixture, not an
end-to-end repro. Everything else above is end-to-end through `execute_command`.

---

## F. Proposed RED regression seams

No tests written. Proposed home: **`native/core/tests/batch4_group_a_test.cpp`** (R-09) and
**`native/core/tests/batch4_group_b_test.cpp`** (R-10), registered with `add_test` exactly like
`batch3_group_a/b/c`. Both link only `openu5_core`; the `DeviceWorld` fixture pattern from
`native/targets/tdeck/host_tests/ui_mode_test.cpp` transfers directly (it already builds a real
`CommandContext` with dialogue/shop/shrine services).

### Group A — R-09

The four Blackthorn flows are **not** one parameterized case; each asserts a different state
mutation. `FountainDrink` is a separate shape.

| ID | Production path exercised | Proves | Why it is RED today | Over-mock check | New seam? |
|---|---|---|---|---|---|
| **A1** `guard_tribute_yes_debits_and_clears` | real town turn → `blackthorn_turn_effect` → `UiSession::handle_input('y')` → `finish_modal` → `execute_command` | `gold −= 10×living`; `tribute` cleared | no branch → gold unchanged, flag set | drives the real trigger and the real core; nothing stubbed | no |
| **A2** `guard_tribute_no_escalates_to_arrest` | same | refusal arms `GuardArrestPrompt` **and** the chained modal re-arms with `return_mode = Exploration` | no dispatch → no escalation | asserts UiSession registers, not just an enum | no |
| **A3** `guard_arrest_yes_jails_exactly` | same, hostile guard ai 6 | loc 4, (25,4), keys 0, hour 8 — the four-field `guardArrestJail` tuple | no dispatch → party unmoved | exact reference mutation, not "a command was seen" | no |
| **A4** `guard_arrest_no_consumes_machine` | same | `arrest` cleared; defend-thyself path taken | flag stays set | — | no |
| **A5** `guard_password_impe_passes` | palace loc 18 + badge | `password` cleared; `"Pass, friend!"` | no dispatch | uses the real byte-exact challenge from `blackthorn.cpp:24` | no |
| **A6** `guard_password_cancel_submits_empty` | same, `UiActionKind::Cancel` | cancel resolves with `""` → escalation, machine cleared | cancel dispatches `accepted=false` and is dropped | this is the distinct cancel semantic §B demands | no |
| **A7** `blackthorn_answer_advances_interrogation` | `BlackthornPrompt` event + armed session | `round` advances / shrine resolves; `base_mode` returns through `shrine_return_mode_` | no dispatch | asserts the Batch-1 shrine register survives the loop | no |
| **A8** `blackthorn_machine_never_softlocks_the_world` | all four, one assertion each | a following `Pass` is **not** `AwaitingResponse` | all four currently strand `commands.cpp:628` | this is the real user-visible bug | no |
| **A9** `fountain_opens_party_selection` | `FountainDrinkPrompt` → `UiSession::consume` | mode is `PartySelection`, prompt `"Who will drink?"`, an `OpenPartySelection` intent is raised | currently `YesNo` `"Drink?"` | pure `UiSession` production call | needs the new `UiRequestId` |
| **A10** `fountain_status_text_and_cancel` | member resolution + flavour | `'D'`/`'S'` → `Incapacitated!`, else `Refreshing...`, cancel → `None!`, **HP and turn unchanged** | strings do not exist | see §G uncertainty — may need an ESP-free seam | likely |

### Group B — R-10

| ID | Production path | Proves | Why RED | Over-mock check | New seam? |
|---|---|---|---|---|---|
| **B1** `npc_initiates_talk_reaches_begin_conversation` | town turn → `NpcInitiatesTalk` → consumer → `execute_command(BeginConversation)` | `DialogueSession::active`, and `session.npc.schedule.slot == e.npc->schedule.slot` | no consumer | asserts **NPC identity**, not that the enum was seen | the deferred-drain register (see §G) |
| **B2** `npc_initiates_talk_owns_and_returns_mode` | same + `EndConversation` | `base_mode` Exploration → Dialogue → Exploration | unreachable | guards Batch 1 R-18 | no |
| **B3** `npc_initiates_shop_opens_the_shop_session` | town turn, dialog 0x81 | `shop.phase != Closed`, `shop_return_mode_` correct | no consumer | proves the shared `DialogueHandoff::Shop` route, not a parallel one | no |
| **B4** `npc_initiated_shop_uses_the_same_entry_as_player_talk` | player `Talk` vs NPC-initiated, same NPC | identical `ShopPhase` and `shop_type` | one arm is unreachable | forbids a second shop entry point | no |
| **B5** `npc_initiation_survives_the_emitting_turn` | drain after `execute_command` returns | the copied slot still resolves to the same actor; a stale `location` is rejected | n/a (new invariant) | encodes the identity-lifetime ruling | no |

**Explicitly rejected as over-mocked:** any test that asserts only
"`GameEventKind::NpcInitiatesTalk` was emitted", and any test that drives a test-local copy of
`AlphaRuntime::modal` for the four Blackthorn flows — that is exactly why the translation belongs
in `finish_modal`.

---

## G. Minimal future GREEN change shape

No code. Smallest shape that the preview measured as sufficient:

1. **`UiSession::finish_modal`** — four `else if` arms mapping
   `{Blackthorn, GuardPassword, GuardTribute, GuardArrest}` to `UiIntentKind::Command` with
   `CommandKind::BlackthornAction`, `item = BlackthornAction{Answer, Password, Tribute, Arrest}`,
   `member = yes?1:0`, and `text/text_length` for the two text prompts. Same table shape as the
   existing `TownExit`/`RestHours` arms.
2. **`UiSession::cancel_modal`** — route `Blackthorn` and `GuardPassword` through the same
   translation with an empty string instead of the bare `accepted=false` `ModalResponse`.
   (`GuardTribute`/`GuardArrest` need nothing: `cancel_means_no=false` already makes them
   uncancellable, correctly.)
3. **`UiSession::consume`** — replace `FountainDrinkPrompt`'s `begin_yes_no` with an
   `OpenPartySelection` intent under a new `UiRequestId`; add the `"a gurgling fountain!"` line.
4. **`AlphaRuntime::modal`** — one arm resolving the fountain member and emitting
   `Refreshing...` / `Incapacitated!`; `cancel_modal` prints `None!`. No turn, no HP.
5. **`AlphaRuntime`** — a small pending register `{kind, slot, location}` written by
   `consume_event` on `NpcInitiates*` (copying `schedule.slot`, never `e.npc`), drained at the
   tail of `handle()` beside `synchronize_after_debug(...)` as
   `command(BeginConversation{member=slot})`, guarded by an unchanged `position.map.location` and
   by the UI not already owning a session. Gate `NpcInitiatesShop` on `dialog ∈ [0x81,0x88]`.

**Stated uncertainties.**

- *A10's home.* Whether the fountain flavour text should live in `AlphaRuntime`
  (reference-faithful, but host-untestable without a Batch-3-style ESP-free seam) or become a
  tiny core `CommandKind`/`look.cpp` branch (host-testable, but invents a command the reference
  does not have). Evidence currently favours the reference shape plus a small ESP-free helper
  taking `(status, cancelled) → const char*`. **Not resolved.**
- *B1's drain register.* Whether the deferred initiation belongs on `AlphaRuntime` (mirror-only,
  needs a seam for the host test) or as a tiny ESP-free `tdeck::` helper like
  `resolve_synchronized_base_mode`. Batch 3's precedent favours the helper. **Not resolved, but
  does not block RED design** — B1–B5 can be written against the core path first.
- Prompt-text fidelity for tribute (`e.note` toll / Minoc charity) and arrest
  (`"Thou art under arrest!"`). Real divergences, but presentation, separable from the dispatch
  fix. **Recommend explicitly scoping them in or out before implementation.**

---

## H. Scope / risk notes

**Batch 1 regression risk: low, and measured.** `finish_modal` sets `mode_ = return_mode_`
*before* `dispatch()`, so a chained prompt raised by the dispatched command re-enters
`enter_modal` from a non-modal mode and captures the correct return register. The preview
exercised the worst chain (tribute → escalation → arrest → jail → MapChanged) and ended at
Exploration. `settle_shrine_after_modal()` correctly no-ops when a nested prompt re-armed, and
correctly settles when the ceremony ends. The `GuardPassword → Capture → BlackthornPrompt →
enter_shrine_mode()` path captures `shrine_return_mode_ = Exploration` because the guard prompts
(unlike `BlackthornPrompt`) deliberately do **not** enter `ShrineSpecial` — that asymmetry is
correct and must be preserved. R-10's Dialogue/Shop return registers were verified end-to-end.

**NPC identity lifetime.** `e.npc` is a borrowed pointer valid only for synchronous delivery. It
stays valid through the emitting turn because `blackthorn_turn_effect` returns `true` and
`turn()` bails immediately, but `blackthorn.cpp:53` compacts `c.actors->actors[]` on another
branch and `dialogue_despawn` can remove actors — so the pointer must never be stored. Copy
`schedule.slot` **and** `location`, and validate the location before draining. A drain deferred
past the next world turn is unsafe (the NPC may move, despawn, or a different actor may occupy
the slot after a map change); the proposed same-input drain avoids this. Adjacent to R-23 roster
compaction but does **not** require touching it.

**Cancellation risk.** The single highest-risk item is `GuardPassword`'s ESC. Today it closes the
prompt and leaves `BlackthornSession::password` set — a softlock with no visible prompt. A fix
that only adds the accepted branch would leave this path *worse* than the others. A6 must be in
the RED set.

**Audit assumptions found false.**

- "Five modal responses are silently discarded" — they are discarded *and* four of them softlock
  the world. Understated.
- "`FountainDrink` sits in the same family and was simply missed" — **false.** Different widget,
  no command, no turn; the reference is a party picker with flavour text.
- "`CrystalBall` … **are** handled" — **false.** Dispatched with `member = −1`, always
  `Rejected`.
- "`WellDrop → DropCoin` … handled" — **half false.** Yes works by accident (`−1` truthy); No
  dispatches nothing; ESC should mean No and does not.
- "add five `modal()` branches following the existing `WellDrop`/`WellWish` pattern" — the wrong
  layer for four of them and the wrong shape for the fifth.
- "`NpcInitiatesShop` → shop entry" implies a separate path; it is the **same**
  `BeginConversation`.
- Batch 4 risk rating "low" — the softlock and the re-entrancy constraint put it at **medium**.

**Must be answered before implementation:** (1) fountain effect owner — `AlphaRuntime` +
ESP-free helper vs. a core branch; (2) whether prompt-text fidelity (tribute toll/charity, arrest
wording, Blackthorn `"Your response?"` suffix) is in scope for Batch 4 or deferred.

**Unrelated defects observed, untouched:** the `CrystalBall` `member=−1` rejection and the
`WellDrop` No/ESC gaps (both R-09-adjacent, both in `alpha_runtime.cpp:644-645` /
`ui_session.cpp:951`); the `0x80..0xFC` vs `0x81..0x88` emitter/consumer window. Recorded only.

---

## I. Final recommendation

**READY FOR BATCH 4 RED TEST IMPLEMENTATION**

The two open questions (fountain effect owner, prompt-text fidelity scope) are GREEN-implementation
choices, not RED blockers: A1–A9 and B1–B5 can be authored against production paths now, and
A10's assertion text is fixed by the reference regardless of where the code lands.

Recommend widening the batch by two lines to cover the `CrystalBall` `member` and `WellDrop`
No/ESC defects, since they live in the same five lines the R-09 fix edits and the audit currently
cites them as the model to copy.
