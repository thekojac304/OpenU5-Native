# Quest/progression translation ledger — semantic core complete

The requested platform-independent quest/progression continuation is complete
within the semantic command/event contract below. This does not make the device
a playable alpha. Existing non-foot movement and bridge-toll command restrictions
remain platform-independent integration blockers. No T-Deck hardware, input,
display, UI or FATFS source was changed, and no TypeScript runtime was changed.

## Starting state and completed continuation

The repository began with an explicitly partial ledger: 4,249 quest observations,
2,033,537 cumulative. Existing rules, known state, shrine ceremonies, melody,
Faulinei theft/dialogue-end hook and save adapters were retained. The continuation
completed the following missing orchestration, reusing existing owners and engines:

| Reference behavior | Native implementation / connection |
| --- | --- |
| Legacy `destroyShadowlord` | `quest.cpp`: exact result/messages and death flag; preserves the shard |
| Underworld plot seeding | `hydrate_underworld_plot`: purge/reseed through the caller's world-object pool; grant and hydration preflight storage |
| Interior plot objects | Real NPC slot/type/schedule hydration; Crown, Sceptre, wooden box and carpet; taken flags prevent reappearance |
| In-world Yell | Names summon at the reference flame coordinates; adjacent Words of Power change flags, quakes and turn order |
| Shard use | Flame/identity gates, sound/quake/explosion order, death/doom/shard mutations and summoned-object removal |
| Get and Search progression | Search-object FIFO, loot LIFO and plot pickup; all 113 real search entries, key/equipment/HMS grants, reagent patches, moonstones, secret doors and chest-trap detection |
| Artifacts | Amulet/Badge spell markers, Crown, HMS plans, Sceptre world/dungeon/combat effects; existing box Use response retained |
| Harpsichord | Note event/matcher, passage overlay, real movement through the opened wall and wooden-box pickup |
| Moonstones | Borrowed existing transition owner, burial/acquisition and physical gate traversal including midnight closure and interior/underworld hydration |
| Blackthorn and guards | Capture/password/interrogation, sacrifice/roster compaction, destroyed shrine, karma/time/deposit, tribute/charity/arrest/alarm and hostile attack handoff |
| Urban Shadowlords | Physical placement, wither overlay with its separate day-seeded RNG, real NPC possession/fear and repeated-entry behavior |
| Scripted combat | Doom entrance and hostile NPCs use the existing generic combat engine with real enemy/arena resources; NPC removal and Sceptre reclamation stay ordered |
| Absorption / rescue | Painted north-cell trigger, absorbed status, early combat return, Lord British rescue, `GameWon` then `Endgame`; full victory/stranded text beats |
| Defeat and trapdoor consequences | Stonegate floor wipe/party death, generic trapdoor fall, combat/dungeon Refuge checks, full Refuge beats and resurrection mutation |
| Persistence | Existing QuestState gains search flag presence/value bits; HMS state uses GameState; retained-document and GAM/sidecar round trips preserve new and unknown flags |

Sources are the corresponding `game/src/core/quest`, `world`, `endgame`,
`combat`, `game.ts`, and existing native save adapters. The implementation lives
in `quest.cpp`, `shrine.cpp`, `quest_world.cpp`, `quest_search.cpp`, `blackthorn.cpp`
and existing command/world/dialogue/combat/dungeon/persistence files. There is
one live QuestState embedded in GameState, not a second quest state machine.

## Validation and counting

**5,377 quest observations: 5,375 completed cases and two bounded reference-loop
observations.** The continuation adds **1,128** to the previous partial pass.
The pre-quest total remains 2,029,288; the new cumulative total is **2,034,665**.
Each generated case/sequence counts once; its intermediate snapshots are not
added again to the cumulative total.

| Category | Cases/sequences |
| --- | ---: |
| World/NPC/combat quest sequences | 1,064 |
| Legacy destruction / plot grants | 48 / 48 |
| Ritual / summon / words rules | 192 / 256 / 72 |
| Rescue gating / playtime | 512 / 156 |
| Melody | 143 |
| Shrine donation / Codex / modes | 210 / 1,024 / 256 |
| Shrine check / restoration | 56 / 56 |
| Semantic shrine ceremony sequences | 68 |
| Theft | 1,152 |
| Retained-document / GAM-sidecar round trips | 40 / 24 |

`check-quests.ts` calls actual TypeScript Game methods and helpers, runs the same
inputs through `quest_driver`, and compares projected state, ordered semantic
events and final RNG after each action. Combat snapshots also compare actor
stats/status/counters, terrain, active actor, absorption and combat event streams.
Guard prompts compare charity/toll payloads. Endgame and Refuge compare their
complete emitted beat scripts. NPC snapshots compare slot/dialogue/AI changes;
they are not a full per-step NPC path trace. Existing NPC/path suites remain
separate. These are deterministic differential tests, not an exhaustive proof.

Real-asset integration includes **214 NPC sequences**, **82 encounter sequences**,
all eight shrine definitions and dungeon words, all 113 search entries, real
small maps, overworld, dungeon cells, INIT.GAM, NPC records, enemy tables, surface
CBT arenas and the final DUNGEON.CBT room (source array index 127, map index 111).
Surface arenas use their array identities; dungeon-map index duplicates are not
mistaken for surface arenas. Required assets are not synthesized or skipped.

Encounter coverage records four party escapes, five combat-to-Refuge sequences,
and six final-room absorption endings (three seeds, both wooden-box branches).
Seven sequences emit combat `VICTORY!`, including the six empty final rooms and
one normal town-guard victory. The latter and the three explicit defeat fixtures
assert that their intended outcome was actually reached. The real palace hostile
slots retain their extracted enemy definition, including summoning behavior.

Five real-overworld routes walk 40 steps into a shrine and finish its ceremony.
The castle passage test tries the closed wall, plays the melody, walks through,
picks up the real box object and checks that hydration does not replace it.
Real plot pickup also covers Stonegate's trapdoor wipe. These are subsystem-to-
subsystem replays; no uninterrupted new-game-to-ending campaign is claimed.

The theft rejection loop can fail to terminate (seed 20 with keys only is one
witness). Both implementations are observed through 65,536 draws using test-only
watchdogs. Production preserves the reference loop. Unicode ritual matching uses
the generated JavaScript uppercase table, including expansion and surrogate pairs;
the registered drift test protects the table.

## Resources, lifetime and persistence

QuestWorldServices borrows the existing object pool, tile/volatile overlay owner,
NPC data, moonstones, encounter context, string records and search/word tables.
`reserve(n)` guarantees subsequent appends without failure; erase compacts indices.
Callers must provide the relevant resources before executing a command. Missing
resources produce adapter status codes rather than invented assets. Get/Search
here cover quest acquisition and the documented priority branches, not every
untranslated generic world Open/Jimmy/Push/Get behavior.

Event text, NPC pointers and Endgame/Refuge scripts are synchronous borrowed views.
An asynchronous presentation owner must copy them during `emit`. Capture/refuge,
melody/passage and shrine pending sessions are transient as in the reference.
Known quest flags preserve absent versus false; unknown flags survive through the
retained save document. Existing GAM offsets and envelope/sidecar formats remain
unchanged. Missing transportTile restores the reference foot default (28).
Reference formats still do not persist live RNG, combat or dialogue sessions.

ESP measurements (bytes):

| Type | Current | Delta from partial quest pass |
| --- | ---: | ---: |
| GameState | 2,304 | +32 (2,272 → 2,304); +72 versus pre-quest 2,232 |
| QuestState | 68 | +28 (40 → 68) |
| CommandContext / Command | 128 / 32 | +8 / 0 |
| QuestWorldServices / QuestObject | 108 / 52 | New borrowed service/value views |
| BlackthornSession / ShrineSession | 8 / 4 | New / unchanged |
| EndgameScript / RefugeScript | 368 / 256 | Transient event descriptors |
| CombatState / GameEvent | 3,604 / 56 | Existing owners extended |
| Moonstone | 6 | Signed basement floor supported; caller-owned |

No separate quest scratch arena or duplicate world pool is allocated. Object
capacity remains caller-owned (four underworld spawns can require four reserved
records); generic combat actor/field overflow and NPC scratch retain their existing
contracts. The NPC raster is 1,056 bytes. General A* scratch remains 323,084 bytes,
external to these quest helpers. Do not place it on an internal-RAM task stack.
Text construction uses `std::string`/`std::u16string` heap storage proportional to
input/text length; Unicode uppercasing may triple code-unit length. Save JSON and
retained assets retain their existing heap requirements. There is no measured
constant whole-session heap peak. The Unicode table is 31,600 bytes of read-only
object data when linked, not live QuestState.

Largest measured Xtensa `-Os -fstack-usage` frame: **2,544 bytes**, `restore_core`
(+32 from the partial pass). Largest new quest orchestration frame: **608 bytes**,
`absorption_endgame`; combat spray: 912; dispatcher: 416; Get: 400; Refuge: 368.
These are individual compiler frames, not measured whole-call-chain peaks.

Final firmware: **342,288 bytes**, +32 from the partial 342,256-byte image,
+64 from the pre-quest 342,224-byte baseline. Linked image is 342,168 bytes.
Launcher allocation stays **393,216 bytes**. ESP-IDF build, size and Launcher
packaging pass without flashing. The current device slice strips unused quest
code, so this delta is not the cost of a fully wired alpha.

## Explicit gaps and alpha handoff

* No required quest asset skips. Three *long foot-route* cases are excluded:
  shrine indices 1 and 3 have no dry walkable approach in the chosen search;
  index 6 has reference coordinates that do not identify an overworld shrine
  tile. All eight shrine rules/ceremonies remain covered. This is not a claim
  that those shrines are unreachable by other reference travel mechanisms.
* Optional shrine/Blackthorn visual scene timelines, XOR/rune/viewport styling,
  animation and rendering are deferred with UI. The semantic no-scene reference
  branch is the ceremony/capture contract; visual-event-array parity is not
  claimed. Endgame/Refuge beat data is emitted, but no renderer is added.
* Two existing extractor optional tests skip: historical font-atlas comparison
  and the optional user seed-save round trip. Required quest sources are present.
* The subsequent [gameplay integration](GAMEPLAY.md) resolves the previously
  recorded non-foot Move, bridge 106/107, outdoor encounter ownership and generic
  world-command blockers. That ledger supplies the current alpha-readiness and
  resource contract; this document retains the completed quest-specific scope.
* Device wiring, UI/pickers, presentation, FATFS and hardware validation remain
  future work. No new-game-to-ending or on-device gameplay claim is made.

There is no outstanding gameplay translation item in the requested quest list
within this semantic contract. Quest completion must not be conflated with
completion of all platform-independent gameplay or permission to start device/UI
integration.
