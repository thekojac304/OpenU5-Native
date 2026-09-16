# Platform-independent persistence

The authoritative format is the current TypeScript implementation, not the DOS
format documentation. `persistence.h` exposes byte/document operations; it has
no paths, browser storage, clocks, random slot IDs, ESP-IDF, or FATFS dependency.
No TypeScript runtime was changed. Hardware controls and target code are unchanged.

## Source map

| Reference | Native implementation |
| --- | --- |
| `saveNative.ts:parseSaveWindow`, character/bitmap helpers | `persistence.cpp:import_native` (window decoding followed by reference import rules) |
| `saveNative.ts:exportNativeSave`, `extractSidecar` | `export_native`, `export_native_state` |
| `saveNative.ts:writeEnemyTable`, native world-object readers/writers | private object/enemy table helpers |
| `saveNative.ts:buildNativeOol` | `build_ool` |
| `u5gam.ts:emptySidecar`, `isSaneSidecar`, `readU5gamEnvelope` | `empty_sidecar`, `sane_sidecar`, `read_envelope` |
| Existing `.u5gam` marker + JSON envelope shape | `write_envelope` |
| `persistence.ts:importNativeSaveFiles` | `import_save`, plus `load_native_state` for the live native projection |
| `state.ts:serialize`, `deserialize`, `assertValidState`, optional defaults/migrations | `save_json.cpp`, `deserialize`, `load_state`, `save_state` |
| `save-keys.ts:SaveMeta`; `persistence.ts:listSaves`, autosave pointer rule | Metadata JSON objects, `most_recent_slot`, `autosave_slot` |
| `npc/manager.ts:enterMap(restore=true)`, `syncWalkToState` | `restore_npc_walk`, `capture_npc_walk` |
| Future SD power-loss recovery, explicitly requested for this batch | `Generation`, `save_crc32`, `complete_generation`, `select_generation` (new adapter contract, not claimed as TS parity) |

## Compatibility details

* `.GAM` export copies exactly 4,192 template bytes and patches fields explicitly.
  Short templates/imports fail. Extra input bytes are tolerated; they are not
  copied into the native window. Unmodeled bytes, unused roster records, name
  tails, character byte 0x18, moonstone location semantics, possession-byte
  preservation, object mover bytes, and word-seal low bits follow the reference.
* Fields use explicit little-endian encoding. The turn byte saturates at 255;
  byte 0x2e6 is independent and survives. NPC grids are MSB-first in the file.
  Doom bits OR into the dead bitmap, not an independent replacement word.
* Nine name bytes are supported, including a completely nonterminated native
  name. `CharacterState` now reserves a tenth byte for the C++ terminator.
* Object placement reserves slots before enemies; world objects allocate
  descending 31..1, while enemies allocate ascending 1..23. Non-owned actors
  survive. Native actor recognition is exact, not animation-frame-masked.
* `.OOL` is exactly 512 bytes. A missing/short template becomes zeros; an interior
  save retains the template; overworld/underworld patch only the selected block.
  **The reference has no `.OOL` import path.** This implementation does not invent
  one or recover world state from the inactive OOL block.
* Explicit sidecar JSON wins over an envelope. Bad/missing envelopes fall back
  to native-only state. Invalid explicit JSON fails instead of falling back.
  Envelope validation deliberately checks only the reference's shallow shape:
  sidecar version is not validated, and metadata is not required. Version 1 is
  required for the envelope and for full JSON GameState saves.
* Native imports do not run JSON save migrations. JSON imports remove old
  search:13/14/15 flags, retired treasury state and only the specified treasury
  overrides; optional defaults and absence-significant keys match the reference.
  Undefined properties are omitted in encoded JSON, as with JSON.stringify.
* JSON strings retain UTF-16 code units, escaped lone surrogates and unknown
  fields. Text decoding uses replacement UTF-8; envelopes strip a leading BOM.
  Object key ordering and numeric spelling need not be byte-identical JSON;
  logical JSON equality is checked. Native GAM/OOL equality is byte-exact.

## Live-state ownership and semantic hooks

`load_state` and `load_native_state` validate/decode into a document and then
restore the existing GameState and TurnState. Failure leaves live state and the
retained document unchanged. `save_state` and `export_native_state` capture those
same live structs into a copy of the retained document. There is no second
gameplay state model, raw struct serialization, or hidden template/global state.

The retained document is the save representation. It carries fields not owned
by these live structs: artifact/shard/moonstone data, journal, explored data,
quest flags, map overrides, door timers, world enemies/objects, and extra runtime
fields. Codecs decode/re-encode these fields rather than dropping them. Existing
service owners must capture their changes into that document before saving and
restore their state from it after loading; this is the application wiring seam.
Do not keep using a stale retained document after mutating those service-owned
fields. Begin a new game document from the authoritative INIT.GAM/template or
full initial-state JSON; a bare default C++ GameState is not a complete save.

GameState fields captured directly include the roster/equipment/pack, logical
equipment length, resources, position/time/turns, transport, ship residue,
dungeon room bitmap, NPC met/dead grids, generic item flags and worn crown.
TurnState captures clock latch, spell timers, wind, drunkenness, reagent/skull
tree days and Shadowlord locations. Negative phase values represent absence.

For NPC restoration, first initialize actors with static schedules using the
existing `enter_npc_map`, then apply `restore_npc_walk`. The saved slot list is
authoritative: absent slots are removed, duplicate records use the last value,
path buffers truncate/pad to 32 bytes. With the fidelity gate open, absent/null
walk data clears the live list. Capture walk data before save; sidecar inclusion
uses the explicit gate argument, matching the reference's gate.

The document codec accepts a broader domain than bounded C++ gameplay structs.
`NativeDomain` prevents silent narrowing: e.g. >16 characters, >9-byte names,
non-byte character values, unsupported coordinates, nonintegral counters,
oversized inventories or grids. Fixed native spell/scroll/potion/reagent arrays
require their canonical lengths. Such documents can still be decoded and
round-tripped as documents without loading into live gameplay.

### Deliberately absent from persisted state

These are reference persistence boundaries, not invented sidecar extensions:

* `Game.liveRng` is outside TS GameState. Loading does not reseed the native RNG;
  its current seed is preserved. Exact RNG continuation across process restart
  is **not supplied by the reference save formats**.
* Active CombatState/CombatContext, DungeonState (including facing/revealed map/
  wanderer), Conversation/DialogueSession, command cadence/awaiting responses,
  travel scratch and volatile terrain are runtime session objects. They are not
  in TS serialization. Their consequences in party/world/NPC/dungeon-clear state
  persist where the reference supports them. This does not claim mid-combat,
  mid-conversation or active dungeon-session resume.
* Native bare GAM+sidecar intentionally loses some full-JSON state: for example
  unsaturated turns, wornCrown and drunkTurns are not sidecar fields. Full JSON
  preserves them. Native actor sidecars override lossy native reconstruction.

No new sidecar fields were introduced for these native-only session structures.

## Logical generations and storage boundary

The browser reference sorts metadata newest-first and loads that slot; a corrupt
newest body throws. `most_recent_slot` preserves this policy, with stable ties.
Autosave has three slots and advances its pointer only after successful commit.
Browser writes, quota exceptions, thumbnail/index rollback, and localStorage
enumeration remain platform code and are not implemented here.

The separately requested SD generation contract is a descriptor, not a new
on-disk save format. A generation contains a sequence number, required-section
flags, borrowed payload buffers, CRC32 values, commit state and a cross-file
identity verdict supplied by the adapter. The core recomputes all supplied
payload checksums, validates lengths/sidecar/envelope, and chooses the greatest
complete committed sequence (stable first entry on ties). Incomplete, corrupt,
mixed-identity or uncommitted generations cannot supersede the previous one.
No fallback from a broken sidecar to native-only is allowed during generation
validation, unlike deliberate native-only import.

A future adapter must assign identities/checksums, write all temporary payloads,
flush, validate the descriptor, commit its metadata last, and retain the previous
committed generation. It owns open/read/write/rename/flush/fsync/directories and
the on-disk transaction manifest. This batch does not specify or write a FATFS
manifest, and makes no claim that FAT rename alone is power-loss atomic.

## Memory and device integration limits

* GAM buffer: **4,192 B**; OOL: **512 B**; JSON input/checked output:
  **262,144 B**; complete `.u5gam`: at most **266,347 B** (GAM + 11-byte marker + JSON).
* JSON parsing/writing is bounded to **8,192 values and nesting depth 32**.
  JSON import and checked writers return capacity errors, not truncated data.
  An oversized envelope is classified as bad within the bounded native domain.
* Buffers/documents are caller-owned or allocated dynamically, never app_main
  stack globals introduced by this batch. `write_json` is the low-level trusted
  document formatter; use checked `encode_json` or semantic writers at the storage
  boundary. Direct programmatically constructed documents must honor the limits.
* The DOM is not a streaming decoder or a fixed scratch arena. On ESP each Json
  node is 64 B (host 88 B), so 8,192 live node objects alone cost 512 KiB before
  vector spare capacity, strings, keys and allocator overhead. A conservative
  container-storage allowance is roughly **2.5 MiB per maximum-sized parsed
  document**, excluding allocator bookkeeping. Parsing/capture/export can hold
  multiple documents and output buffers simultaneously; there is **no fixed
  total heap-scratch guarantee**. Do not equate the 256 KiB file limit with RAM use.
* ESP `-Os -fstack-usage`: largest new individual frame **2,416 B**
  (`restore_core`); import_native 496 B; NPC restore 288 B; recursive parser
  frame 144 B per level. These are individual frames, not total call-chain or
  task-stack high-water measurements. A dedicated storage task and measured
  stack budget are required; do not invoke worst-case nested parsing on app_main.
* GameState **2,200 -> 2,232 B (+32)** on host and ESP; PartyState 524 -> 556 B;
  CharacterState 32 -> 34 B. TurnState remains 72 B. Generation descriptor is
  72 B host / 48 B ESP. Json root is 88 B host / 64 B ESP, plus allocations.
* Future device integration needs a PSRAM allocation policy or streaming/arena
  codec for large saves, allocation-failure handling (IDF uses no exceptions),
  measured live peak RAM/stack, service-owner capture/restore, transaction
  storage and reboot/power-cut tests. None is claimed by a linker-stripped build.

## Validation and real fixture provenance

Run `node --import tsx native/core/tools/check-persistence.ts` after the host build.
The driver reads/writes ignored NDJSON files under `build-persistence`; reference
results come from unchanged TS functions. No original asset/save bytes are added
to tracked fixtures. CTest includes this runner in `persistence_parity`.

**619 compatibility scenarios** plus **11 native contract scenarios** (eight
generation, two parser capacity, one API scenario with eight assertions) pass.
They cover every native actor tile byte, field extrema, short rosters, unknown
bytes, OOL blocks, sidecar precedence, migration/version failures, JSON syntax/
Unicode, NPC restore and semantic load/save. Native-produced GAM/envelopes are
read back by TS. Two real sources (`original/u5/ultima5/INIT.GAM`, `SAVED.GAM`)
each complete three cycles: **six real round trips**, zero available-source skips.

`ad01.gam` is the first recorded checkpoint of the **Alex Diener / ESPEJO-2 AD
tour corpus**, whose Avatar is Barnabas; it is not INIT.GAM and cannot be replaced
with a synthetic save. The exact required path is
`game/e2e/espejo-tour/saves/ad01.gam`, relative to this repository. The two blocked
tests are in `game/tests/blackthorn-trono-nombre.test.ts`, lines 89 and 203,
reading it at lines 91 and 205. Obtain the user-owned recorded checkpoint using
the existing AD corpus convention (`game/e2e/espejo-tour/README.md`, ESPEJO-2).
It remains absent; no file was fabricated and no failing test was disabled.

See VALIDATION.md for full host, TypeScript, extractor and firmware results.
