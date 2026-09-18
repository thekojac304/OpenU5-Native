# Alpha 2.0 forensic loot, modal UI, and action-feedback pass

## Outcome

This pass fixes the exact-coordinate Troll chest handoff, removes misleading Ready `x0` labels, moves Z/Stats into the compact modal system, adds visible consumable results and semantic audio hooks, and reproduces the original DOS magic ceremony as an asynchronous viewport palette inversion. It does not add fuzzy chest lookup, a second audio backend, or a new UI architecture.

## Chest root cause and authoritative path

Roaming outdoor combat retains the defeated outdoor enemy coordinate in `OutdoorServices::victory_latch`, so its victory callback can insert a `QuestObject` at that coordinate. Troll toll refusal starts combat directly and never uses that latch. The direct teardown fallback formerly inserted a surviving combat chest at the player's return coordinate. The combat renderer showed the chest at the defeated Troll's combat cell, while exploration `Open` correctly searched the adjacent blocked Troll coordinate. Thus location and floor matched, but world X/Y did not: chest identity was lost between the Troll movement trigger and direct encounter teardown.

The attempted blocked movement coordinate is now captured with the toll prompt, copied into `CombatState` when the direct Troll encounter begins, and used with the encounter's exact location/floor during teardown. `Open` remains an exact target lookup. No nearest-object, fuzzy coordinate, or automatic targeting behavior was added.

The device trace now covers the entire chain with exact values:

- `LOOT_CREATE` records actor/definition, direct versus roaming encounter, encoded loot/trap/content, combat cell, and captured world origin.
- `CHEST_INSERT` records source, map identity, world coordinate, object index/type/flags/trap/content, and collection counts.
- `CHEST_PRE_TEARDOWN`, `CHEST_WORLD`, and `CHEST_POST_COMBAT` show survival across ownership transfer.
- `CHEST_RENDER` records world coordinate, wrapped/bounded screen transform, tile, and authoritative object index.
- `OPEN_REQUEST` records player identity, direction, exact target, and transform.
- Every object at that exact target is logged as `OPEN_OBJECT`; `OPEN_LOOKUP` records candidate count/chosen index/openability, and `OPEN_RESULT` records the core result and explicit reason.

Multiple loot ownership paths remain distinct:

| Path | Authoritative ownership after combat |
|---|---|
| Troll/direct encounter chest | Promoted `QuestObject` at captured blocking coordinate during direct teardown |
| Roaming outdoor chest | Promoted `QuestObject` by the existing victory latch at defeated roaming-enemy coordinate |
| Trapped/untrapped chest | Same paths above; encoded trap bit and contents are preserved |
| Authored map chest | Existing authored/hydrated `QuestObject`; unchanged |
| Corpse/body and non-chest remains | Combat-local presentation only; not made world-openable |
| Dungeon combat loot | Combat/dungeon object-layer rules; world promotion remains deliberately disabled |

Focused regression coverage proves that a trapped direct-encounter chest is inserted at the adjacent captured coordinate, exact-direction `Open` finds it, and the ordinary authored-chest path still opens.

## Ready and Stats

`ready_items()` was not enumerating every legal definition. Its source was and remains authoritative equipment inventory plus the character's currently equipped items. An equipped item can legitimately have zero units in the carried inventory because the equipped copy has already left the inventory count. The device formatter treated every quantity other than one as `xN`, producing misleading `x0` rows. Owned items now show their actual quantity; an equipped-only item is explicitly labeled `[equipped]`. Legal-but-unowned and unequipped definitions remain absent. Existing core equipment tests continue to cover inventory transfer, legality, slots, two-handed/offhand rules, and re-equipping.

The Ready header overflow came from combining slot and current equipment into one bounded line. The selector now uses separate `Slot:` and `Current:` rows, with bounded item-name truncation only when needed.

Z/Stats previously selected a party member and appended a one-line data dump to transcript history. It now keeps the party selector live as a dedicated `Z / Stats` detail panel with member position/name, class, status, HP, MP, STR/DEX/INT, level/XP, and equipped weapon/armor. Cursor movement changes the selected member and shared party highlight; Mic Back closes the modal. Stats rows are explicitly excluded from transcript rendering.

## Structured-data presentation audit

| State | Classification | Result |
|---|---|---|
| Dialogue, command results, combat results, item/spell results | A — transcript/history | Retained as chronological results |
| Ready item list, Use inventory, spell list, shop/service choices, party choices | B — selector | Shared compact selector frame, spacing, highlight, and footer |
| Z/Stats and Ready current-equipment detail | C — stats/detail panel | Structured bounded rows; never appended as transcript data |
| Direction, combat aim, spell/item target | D — active prompt | Existing target mode retained and cleared on resolution/cancel |
| Available controls | E — context footer | Existing `|` glyph and bounded action strings retained |
| Shop quantities, donations, authored text entry | F — numeric/text input | Existing dedicated input modes retained |
| Use/spell selected-item detail | B/C within selector | Existing selector detail row; result moves to history after resolution |
| Special-item choices | B when bounded, D when directional, A for result | Existing typed routing retained |

No bounded gameplay selection was moved back into the transcript.

## Consumable, spell, visual, and audio feedback

Successful potion and scroll paths now emit `Used <authoritative display name>.` followed by the existing effect/result text (or `No effect!` when no authored result exists). Selectors close through the existing synchronous modal contract before command execution; prompts and party highlight therefore restore through the established base-mode logic.

Repository reverse-engineering of DOS `CAST2:0000`, including the MUSIC-PATCHED timing reference, shows that the common ceremony is not a generic whole-screen flash. It performs a noise lead-in, XOR-inverts the viewport interior by mapping EGA palette index `c` to `c ^ 15`, plays two mirrored tone sweeps, then applies the same XOR mapping to restore. Forty-one of 48 spells use this ceremony; the attack/line-effect families `{1, 13, 28, 37, 40, 44, 45}` do not. Vas Rel Por (46) performs its ceremony only after its original phase selection, which the native command does not yet model, so no speculative generic flash was attached. Potions use their color index `0..7`; supported scrolls use their original ceremony indices and location gates.

The timing is calibrated from the 25,806 Hz patched-DOS reference:

- noise lead: `(0x1f40 + 0x640 * index) * 1.5 / 25806` seconds;
- inverted/tone-sweep window: `2 * (0x2710 + 0xfa0 * index) / 25806` seconds.

The native renderer stores only start/end timestamps. It never sleeps or busy-waits. On phase transitions it redraws the 176x176 viewport, transforms exact palette pixels through `c ^ 15`, and performs a coherent normal redraw at expiry. HUD, selector, and transcript are not inverted. A host regression verifies all 16 colors and proves that applying the transform twice restores the original pixels; non-palette pixels remain unchanged.

No `AudioService` implementation is present in this tree. This pass therefore emits only semantic `Sfx` hooks: `spell-cast`, `potion-used`, `scroll-used`, and `invalid-magic`, plus the indexed `MagicCeremony` event. It does not create a speaker or competing audio backend. Spells excluded by the reference ceremony do not receive an invented generic hook.

## Performance, watchdog, and memory

Selector navigation retains row-level dirty redraw. Ready detail and Stats redraw only changed detail/row regions. Magic feedback is timestamp-driven and adds no task, polling loop, delay, turn, RNG draw, or gameplay mutation. Restoration is forced even if no other scene state changes.

Static ESP-IDF stack reports remain bounded: `AlphaRuntime::render` 2,144 bytes, `compose_selection_view` 320 bytes, `start_magic_ceremony` 48 bytes, `Board::show_alpha` 352 bytes, and `finish_encounter_combat` 112 bytes. Added persistent state is a few coordinate/timestamp/selection scalars and one extra 32-byte detail row; there is no new heap owner or PSRAM buffer. The 24 KiB main stack and input task/queue/watchdog configuration are unchanged. Local builds cannot certify physical watchdog behavior; the physical checklist retains that verification.

## Validation

- Warnings-as-errors host build: passed.
- Focused Ready/Stats/modal/action-feedback/magic/chest/Open tests: 6/6 passed.
- Full native/non-Node host suite: 31/31 passed, including equipment/items, combat, magic, shops/services, party/modal flow, debug tools, and input.
- Node/tsx reference generators: infrastructure-blocked before project code; Node 24.18.0 currently fails `os.userInfo()` with `uv_os_get_passwd ENOMEM`, even in a one-line standalone Node invocation.
- ESP-IDF 6.1 warnings-as-errors ESP32-S3 build: passed.
- App image: 784,720 bytes; 263,856 bytes free in the 1 MiB app partition (25%).
- `git diff --check`: passed; only repository-wide LF-to-CRLF notices were emitted.
- No device run was claimed. Serial timings, task-watchdog status, audio playback, and physical pixel restoration still require the retest below.

## Packaging

- Launcher: `C:\Dev\OpenU5-Native\native\targets\tdeck\build-core\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`
- Size: 784,720 bytes
- SHA-256: `2eff4222b32cdb68f9b871a42847d5a4837c4d1dcb30a10b915d64da45fc9345`
- Required Alpha resource pack: `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`
- Resource SHA-256 (unchanged): `4e1fc6cd2733806232dbbd0af3bd6767b1d2ca58db8b3d2ce2d13a1531e50050`
- Resource SD destination: `/ultima5/openu5-alpha1-resources.bin`
- Tile pack (unchanged): `C:\Dev\OpenU5-Native\native\assets\openu5-assets.bin`
- Tile SHA-256: `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188`
- Tile SD destination: `/ultima5/openu5-assets.bin`

## Physical retest checklist

1. Boot and confirm no task-watchdog warnings.
2. Press R: confirm no `x0` rows, readable separate Slot/Current rows, and an in-bounds selector.
3. Press U: use one potion and one scroll; confirm item name, result text, visual ceremony where applicable, and audio hook if the audio service is available.
4. Press Z: cycle members; confirm the dedicated stats panel, correct party highlight, and no transcript dump.
5. Cast an ordinary ceremonial spell; confirm unchanged spell selector/targeting, viewport XOR effect and clean restoration, and no stale prompt/highlight.
6. Kill a Troll and retain serial from `LOOT_CREATE` through `CHEST_POST_COMBAT`.
7. Stand adjacent and Open toward the visible chest; retain `OPEN_REQUEST`, all `OPEN_OBJECT` lines, `OPEN_LOOKUP`, and `OPEN_RESULT`; coordinates/location/floor must match exactly.
8. Open one ordinary authored chest and one roaming-combat chest if available.
9. Confirm corpse/non-chest remains and a dungeon encounter do not acquire erroneous world chests.
10. Recheck spell/shop/inn/tavern selectors, right border, `|` footer, Mic, WASD, trackball, and all text sizes.
