# Native developer state API

This extends the semantic map-picker port with the remaining platform-independent
OpenU5 developer controls. It does not add a T-Deck menu or change device input,
display, board, or rendering code.

## Original web inventory

The authoritative mutation facade is `game/src/debug/debugApi.ts`. The browser
registry in `game/src/debug/registry.ts` exposes:

- three one-click shortcuts: Maximize all, Best equipment, and Full max party;
- party size/selection, name, gender, class, status, HP/MP, level, experience,
  attributes, inn months, party membership, and all six equipped slots;
- food, gold, keys, gems, torches, skull keys, carpets, karma, and grapple;
- calendar, clock, wind, transport, turns, and torch duration;
- the six web special-item flags, three shards, three Lord British artifacts,
  shrine and Shadowlord numeric state, named/raw quest flags, and moonstones;
- stock quantities for equipment, spells, scrolls, potions, and reagents.

`game/src/debug/shortcuts.ts` contains the deterministic shortcut behavior and
caps. `game/src/debug/saveEditorSections.ts` adds raw transport/ship state,
temporary spell fields, NPC dead/met matrices, the seven-by-sixteen cleared
dungeon-room bitmap, and Clear wandering enemy pool. The save editor explicitly
excludes editing complex live world-object pools, open-door overrides, NPC walk
machines, map overrides, and the reagent-day vector. The original panel has no
arbitrary combat/enemy spawner, no endgame-completion button, and no named
test-state preset system; those are not hidden browser behaviors to port.

## Native API

`include/openu5/debug_developer.h` and `src/debug_developer.cpp` operate directly
on `GameState`, `TurnState`, `QuestState`, the NPC/dungeon bitmaps, and the live
`OutdoorServices` enemy owner. They expose:

- validated character numeric/text/equipment edits and heal, status-clear, or
  explicit revive operations;
- validated resources, inventory quantities, supported special items, shards,
  artifacts, known quest flags, shrine state, clock, transport, and runtime
  turn fields;
- NPC dead/met flags, cleared dungeon rooms, Shadowlord positions, and live
  overworld-enemy clearing;
- exact native forms of Maximize all, Best equipment, Full max party, and Kill
  Shadowlords.

Best equipment derives each slot from the supplied native combat attack/defense
tables, uses the higher item ID to break ties, observes the two-hand exclusion,
and guarantees at least one stocked copy. If tables are absent it is a safe
no-op, matching the web shortcut. All non-teleport operations are deterministic
and leave the shared RNG seed unchanged.

Validation extracts the browser field bounds into the core API: attributes
1..30, model counters and item quantities 0..99, gold/food/experience 0..9999,
the six-month calendar, legal class/status/gender values, real roster bounds,
and valid inventory/NPC/dungeon indices. Invalid requests do not partially
mutate state. Party size cannot exceed the real native roster and active member
is either in-party or the 0xff sentinel.

## Presets

The native-only preset layer composes the ported controls without creating a
second state or inventing a session:

- `MaxedParty`: fills real contiguous roster members, maximizes them, and uses
  live combat tables for equipment; it does not stock unrelated resources.
- `StockedInventory`: all inventory arrays, reagents, gold/resources, supported
  possession flags, artifacts, and shards.
- `Combat`: maxed/stocked/equipped party with member zero active; it deliberately
  does not fabricate a `CombatState` or enemy encounter.
- `Dungeon`: stocked tools, maximum torch duration, clean room-clear bitmap, and
  all eight Word-of-Power flags; the existing map-picker/normal command enters a
  dungeon.
- `Shrine`: full karma/gold, all shrines visited, and one pending shrine quest.
- `Quest`: all shards, Words of Power, and shrine visits without killing the
  Shadowlords or marking the game won.
- `Transport`: a coherent live ship state across `GameState` and `TurnState`.
- `Endgame`: equipped possessions, all Shadowlords dead, in-Doom state, wooden
  box, and explicitly not-yet-won state.
- `LowHealthStatus`: deterministic poison/sleep/charm/death recovery cases.
- `SaveLoad`: distinctive valid values across core, turn, quest, NPC, and
  dungeon-room owners for exercising the existing serialization adapters.

The original browser has no Save/Load action in its debug panel. The native
SaveLoad preset prepares representative saveable state; actual persistence
continues through `save_state`, `load_state`, and the existing storage adapter.

## Unsupported fields

- Pocket watch possession has no native field: native watch use is always
  available, matching the current core behavior. Requests report `Unsupported`.
- Moonstones are accepted as caller-provided transition data but are not owned
  by native `GameState`, so the API does not create a shadow moonstone array.
- Arbitrary string quest flags are reduced to the native `QuestFlag` enum; all
  progression flags currently consumed by native core are editable.
- The web editor intentionally has no placed-world-object editor, NPC walk-path
  editor, open-door editor, map-override editor, or arbitrary encounter spawner.
  Native likewise does not synthesize these complex owners.
- Combat and dungeon presets prepare state only. Starting a real encounter or
  entering a dungeon remains the responsibility of the normal core APIs and the
  already-ported map picker.

## Build control

`OPENU5_ENABLE_DEVELOPER_TOOLS` remains `OFF` by default. When off, neither
developer implementation is present in `openu5_core`; host tests compile the
sources privately. When on, both are available to a future frontend. Static
archive and section garbage collection remove unreferenced developer functions,
so merely enabling the option does not make gameplay invoke them.

For the ESP32-S3 build, `debug_developer.cpp` is 6,567 bytes of object text and
zero bytes of object data/BSS. Together with the previously measured 3,114-byte
map-picker object, the developer objects contain 9,681 bytes of text before
link-time garbage collection. With no frontend reference, both the enabled and
disabled firmware currently link to the same 342,168-byte payload (342,288-byte
padded application image), so the measured linked-image delta is zero. The API
adds no fields to `GameState`, `TurnState`, or `QuestState`, making the persistent
and live state-size impact zero.
