# Generic dialogue/conversation translation

Shop handoffs are now implemented when caller-owned `ShopServices` is attached;
see [SHOPS.md](SHOPS.md). Without that service the historical deferred contract
below remains unchanged for Shop/Guard. QuestEnd now invokes native Faulinei
theft using the existing physical-placement value, emits its message when
applicable, and closes successfully. See [QUESTS.md](QUESTS.md). Guard quest
behavior remains deferred.

The authoritative implementation is the TypeScript in this repository. This
batch ports its generic TLK interpreter, effect application and semantic caller
behavior. It does not replace the TypeScript with an interpretation of DOS/Redux
comments. In particular, two differences between comments and executable code
are deliberately retained below.

## Source map

| TypeScript reference | Native implementation |
| --- | --- |
| `game/src/core/dialogue/conversation.ts`: ScriptItem, ScriptLine, QA, TalkLabel, TalkScript | `dialogue.h`: borrowed `TalkItem`, `TalkLine`, `TalkQA`, `TalkLabel`, `TalkScript` |
| `registry.ts`: masterForLocation, TalkScriptRegistry.get/constructor | `talk_master`, `talk_script_for`, `talk_catalog_lookup` (last duplicate record wins) |
| conversation.ts: foldChar, stristrIndex, keywordMatches, buildQuestionAnswers, getQuestionKey | `talk_keyword_matches`, `Conversation::matches`, `answer`, `interest` |
| splitIntoSections, processSections, processSection | `Conversation::line`, `run_line` |
| script_, processConversationLine, processAnswer, labelLoop | `Conversation::run`, `label_jump`, `run_label`; explicit resumable phases |
| flushLine, beginSpeech, abandonSpeech, endSpeech, insertLatinAt | corresponding native speech/output methods |
| start, input, answersToPartyName, metAvatar, ended | `Conversation::start`, `input`, `met_avatar`, `ended` |
| `dialogue/effects.ts`: applyDialogueEffect, applyGiveItem, applyBeggarAlmsKarma | `dialogue_effects.cpp`: `apply_dialogue_effect` |
| `party.ts`: effectiveName, avatarName, partyEffectiveNames, joinByName, rosterJoinSwap | orchestration's name construction, existing `party_members`, effect's roster swap |
| `game.ts`: talkTarget, talkScriptFor, tryTalkPossessed | `execute_dialogue_command`, registry lookup and message routing |
| game.ts: markNpcMet, npcKnowsAvatar, rollTalkSelfIntro | shared `GameState::npc_met`, live `OriginalRng` with trace observation |
| game.ts: despawnJoinedNpc, townNpcDeadBitSet; npc/manager.ts: clearSlot | `dialogue_despawn`, shared `npc_dead` and existing `NpcActors` |
| npc/manager.ts: arrestAlarm | `dialogue_alarm`, sorted slot traversal with identical draws |
| `ui/talk-console.ts`: start, input, render, end — semantic portion only | `DialogueSession`, `Delivery::render`, command integration and QuestEnd handoff |
| `main.ts`: startTalk merchant/guard dispatch | explicit Shop/Guard handoffs before invoking their deferred mechanics |

Relative names in the table refer to `game/src/core/` unless otherwise stated.
The parser remains `extractor/src/parsers/tlk.ts`; native code consumes its
structured result, not a second TLK parser. All production sources use the shared
`sources.cmake` list for host and ESP-IDF.

## Text contract

* Text is borrowed UTF-16, matching JavaScript **code units**, including isolated
  surrogates. The interpreter is not restricted to ASCII: the TS matcher masks
  each code unit with `0x7f`, then applies `c > 0x60 ? c & 0x5f : c`. Every one of
  the 65,536 possible leading code units is parity-tested. No Unicode library,
  locale-sensitive C case conversion or UTF-8 byte approximation is used.
* Matching uses the first substring occurrence with `start += matched + 1` on
  failure. That occurrence is accepted only at offset zero or after literal
  U+0020. A rejected occurrence does not cause a search for a later occurrence.
  There is no trailing boundary and no four-character keyword truncation.
* Key registration trims ECMAScript whitespace; key identity is case-sensitive
  and first insertion wins. Lookup order is name/job/work/bye, special THANK and
  profanity behavior, then the record's QAs. Aliases use the same matcher and
  preserve per-key precedence. The name prefix applies only when the whole
  untrimmed response lowercases to `name`.
* Empty/whitespace **interest** means bye. Empty **label response** repeats the
  question, adding the exact retry text. Empty **AskName** is an unsuccessful
  name response. AskName matches the first four trimmed code units of any party
  member's effective name. A present empty party-name list differs from absence.
* Yes/no are ordinary free-text answers; no new affirmative/negative parser was
  invented. Semantic DialogueYes/DialogueNo submit `yes`/`no`. Script keywords
  such as `y` and `n` determine accepted variants. `goodbye` is not a new alias.
  Health is not an implicit topic: it works only when a script defines it.
* The UI's 15-character cap, key echo, ESC clearing, keyboard, timed pauses and
  key waits are not native-core input restrictions. Pause/KeyWait remain output
  markers. EndConversation is programmatic teardown, not an ESC binding and not
  equivalent to typing bye while an NPC question is pending.
* Rune toggles preserve mixed segments on the same line. Translation/description
  composition/aliases/quote glyphs remain injected callbacks/data. Quotes close
  before trailing newlines and are abandoned on transfer opcodes exactly as in TS.
  This batch does not ship translated-language assets or a language selector.

The existing native roster stores eight-byte `.GAM` names. Recruitment uses
case-insensitive ASCII roster-name comparison, **not** DOS keyword folding;
all real recruitable names exercised are ASCII. Arbitrary Unicode roster names
remain outside the pre-existing native CharacterState representation. Player
response and TLK text retain the full UTF-16 comparator contract.

## Coroutine and mutation contract

Root progression, current sections/items, skip counter, active label, default
answer index and pending Interest/Name/Label are instance state. Labels replace
the current label iteratively; the reference's 16-jump guard resets after label
input. Script indexes/views are borrowed, not copied. Repeated `input` after end
or before start emits nothing. `bind` constructs a fresh conversation for another
NPC. The session and backing script must remain stationary/alive until teardown.

The interpreter emits a whole batch before the caller applies effects, as TS
does. Opening batches **do not apply effects**, even if they contain Gold,
JoinParty, Change or CallGuards. Input batches do apply them, in order. Another
reference quirk: JoinParty emits join and end, and ends the interpreter even when
the separate effect handler returns `ended:false` for a full party. Neither was
“fixed” in native or TypeScript.

A description branch may end the conversation but TypeScript still executes
the following opening presentation logic, including its self-introduction RNG
draw. Native preserves this. A known NPC consumes no introduction draw. Other
interpreter opcodes have no RNG. CallGuards draws once per non-guard actor, in
ascending slot order, including actors that subsequently fail the person gate.
No conversation command advances clock/world turns. Beggar alms can reset the
existing unsaturated `turns_since_start` after its >=100 gate.

Generic effects use the existing GameState pack and PartyState:

* Karma saturates at 0/99. Gold demand charges only sufficient funds, clamps a
  negative int32 demand to zero, and emits the exact insufficient-funds message.
  Beggar type 0x6c..0x6f applies the cooldown, +1 karma, and +2 more for zero gold.
* Equipment gifts below 0x40 saturate at 99. A..K gifts update food/gold, keys,
  gems, torches, grapple, carpets, sextant, spyglass, black badge and skull keys.
  Out-of-range codes remain no-ops. These are generic opcode effects, not quest
  progression inferred from prose.
  `GameState::equipment_count` preserves the logical TS array length (normally
  48) independently of 256 reserved native slots. Gifts never extend it. Existing
  inventory/combat/loot assignments extend this metadata when their TS assignment
  extends the array. Fixtures exercise lengths 0, 48, 64 and 256; unready then
  dialogue-gift integration verifies that extension remains visible.
* Recruitment preserves no-match/already/full/joined ordering, all messages,
  complete roster-record swaps, equipment and active-character index behavior.
  Successful/already recruitment applies the exact NPC dead-bit type gate and
  removes the runtime NPC. Reload combines shared npc_dead with supplied dead bits.
* AskName recognition writes npc_met after draining output. Alarm mutates existing
  NPC schedules/dialog numbers without ending the conversation. The native actor
  list is the runtime walk state; the serialized npcWalk mirror is persistence work.

## Semantic API, events and deferred boundaries

Attach caller-owned `DialogueServices`/`DialogueSession` to `CommandContext`.
Talk takes a semantic direction; BeginConversation uses `Command.member` as a
slot in the current location. DialogueText takes pointer+length UTF-16. DialogueYes,
DialogueNo and EndConversation require no hardware concept. An active session
blocks unrelated world commands. Absent services, combat/dungeon contexts,
invalid input pointers and a different GameState owner reject explicitly.

`GameEventKind::Dialogue` is a synchronous borrowed envelope for original
DialogueOutput, effect-result messages and native lifecycle/handoff metadata.
The TypeScript interpreter uses its separate DialogueOutput union for prompts,
lines and effects; GameEvent carries world messages and NPC-initiation notices.
The native envelope keeps that distinction. Output order, text, rune/segment flags,
pause markers, effects and prompts are compared directly. The orchestration
comparison separately projects actual TalkConsole messages/prompts, applies
actual Game/NpcManager methods, and compares state/actors/RNG after each action.
Native Ended/Handoff metadata is additional explicit API information, not a
claim that TypeScript emitted identically named events. Message rejections and
possessed-NPC messages use the existing GameEvent::Message variant.

The world guard-interception producer (`checkGuardTribute`'s
`npc-initiates-talk`/`npc-initiates-shop`) remains at the existing
CommandEffect::Capture/Tribute service seams. This batch supplies BeginConversation
for a selected NPC; it does not implement the surrounding guard/capture/hostile
NPC arbitration or automatically select a speaker during a world turn. Those
world events must be routed by that future adapter to this semantic entry point.

Handoffs never fabricate success:

| Handoff | Deferred owner |
| --- | --- |
| Shop, dialog 0x81..0x88 | Full shop dispatch, including shopIsOpen **before** mounted-merchant refusal, inventory/prices/transactions |
| Guard, dialog 0xff in Blackthorn | Quest-specific password/challenge and consequences |
| QuestEnd, every normal/programmatic script close | Implemented: Game.faulineiTheftOnTalkEnd through native apply_faulinei_theft |

For unresolved Shop/Guard, an absent/false handler records `session.deferred`
and returns Unsupported. QuestEnd is built in: handoff, theft mutation/message,
then Ended; it does not call the external handler. An installed Shop/Guard
handler must return true only when it has actually handled
the path or established that its behavior is inapplicable. Its emitted events
remain ordered and counted. The recorded effect envelope is also an attachment
point for later quest observers; no quest conditions were embedded in dialogue.

Still deferred: quest progression/narrative conditions beyond interpreted TLK
branching, shops, persistence/application wiring, save mirrors, SD asset loading,
browser/board input, rendering, audio, and UI pacing. Raw structured interpreter
behavior is complete within the native state domain above. This is not a claim
of exhaustive narrative reachability or of a playable device conversation UI.

## Assets and parity evidence

Authoritative inputs are `original/u5/ultima5/TOWNE.TLK`, `DWELLING.TLK`,
`CASTLE.TLK`, `KEEP.TLK`, plus `DATA.OVL`'s compressed-word table. The existing
`extractor/src/pipeline.ts` produces `game/assets/talk/towne.json`,
`dwelling.json`, `castle.json`, `keep.json`. Run the existing root extraction
command when those assets are absent:

```powershell
npm run extract -- --skip-tiles
node --import tsx native/core/tools/generate-dialogue-fixtures.ts
node node_modules/typescript/bin/tsc -p native/core/tools/tsconfig.json
```

The generator deep-compares each JSON against fresh `parseTlkFile` output using
`extractCompressedWords`. It fails on absent/stale assets; it does not fabricate
content or silently skip. Binary replay fixtures and coverage live in ignored
`native/core/build-dialogue/`. Original dialogue text is not checked into source.
`--check` compares deterministic fixture bytes; coverage instrumentation observes
the reference's processSection without altering its control flow or output.

**250,226 new / 1,241,551 total** parity snapshots:

| Domain | New snapshots |
| --- | ---: |
| Interpreter, real TLK and synthetic edge sequences, exhaustive UTF-16 prefixes, aliases/translation | 237,746 |
| Actual Game/TalkConsole/NpcManager orchestration, including repeats | 7,124 |
| Direct generic effects/counters/roster outcomes | 1,260 |
| Slot-ordered guard alarms and RNG | 4,096 |

All **135 real records** and **313 labels** are exercised. There are 913 isolated
label/input pairs; these deliberately enter an original label via a test job
redirect instead of claiming narrative reachability. Instrumentation observes
**2,023 distinct nonempty topic/answer/label branch lines**, of which **1,623**
are entered without a label redirect. Empty lines are excluded from that coverage
metric. The interpreter processes 516,191 outputs in its sequence corpus.

State digests include exact UTF-16 output/segment bytes, effects, result messages,
met/ended, inventory, gold, karma, roster ordering/equipment, RNG and turn counter.
Orchestration adds active-session status, clock, NPC met/dead bitmaps, full actor
order, schedules, dialog numbers and walk state. Following responses exercise
pending prompt and branch continuation; adapter checks assert native pending
states and explicit rejection/handoff results. The original dialogue fixtures
mark the quest-end seam; the added quest suite tests theft behavior separately,
and the adapter check now expects the built-in QuestEnd path to close successfully.

Validation, missing optional save evidence, firmware and memory measurements are
in [VALIDATION.md](VALIDATION.md). None of the TypeScript runtime, source assets,
extractor behavior, device input bindings or target source configuration changed.
