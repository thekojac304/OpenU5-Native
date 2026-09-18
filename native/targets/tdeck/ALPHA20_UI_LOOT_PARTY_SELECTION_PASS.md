# Alpha 2.0 UI consistency, loot correctness, and party selection pass

## Modal presentation audit

| UI state | Classification | Previous device presentation | Result |
|---|---|---|---|
| Transcript and dialogue history | A — transcript/history | Retained log | Preserved; modal option rows are not appended here. |
| Ready member and equipment | B — compact list selector | Equipment selection existed in `UiSession`, but only the spell mode was admitted to the device compact-selector renderer, so it fell through to the log/footer presentation. | Party member (when required) and compatible equipment use the shared compact selector. |
| Use item and party target | B — compact list selector | Inventory selection had the same spell-only renderer fallthrough. Potion/scroll selector values were also reversed. | Item and party target use the shared compact selector; item IDs now match the authoritative Use table. |
| Party/order/status member | B — compact list selector | Shared selection state, log-style device fallback | Shared compact selector with immediate selected-row highlight. |
| Spell and spell party target | B — compact list selector | Compact selector | Preserved; party-target spells now use the same selected-party mapping outside combat too. |
| Shop/service/member choice | B — compact list selector | Compact shop selector | Preserved. |
| Direction, combat aim, and world target | C — active target/prompt | Active target state and context footer | Preserved. |
| Valid controls | D — context action footer | Green footer; ASCII `|` fell through the bitmap font to `?`. | Preserved; `|` now has an explicit renderer glyph. |
| Yes/No | E — confirmation | Prompt plus Y/N footer | Preserved. |
| Quantity and authored text | F — text/numeric input | Dedicated input state plus footer | Preserved. |
| Developer menu | B — bounded list selector | Dedicated retained developer screen | Preserved; intentionally not folded into gameplay selector infrastructure. |

No bounded gameplay selection mode remains dependent on the transcript overlay. `UiSession` remains the selection source of truth; the device builds `DeviceSelectionView` for Party, Inventory, Equipment, and Spell modes.

## Root causes and corrections

1. Ready and Use already opened typed `UiSession` selection modes. `AlphaRuntime::compose_selection_view()` rejected every mode except `SpellSelection`, which caused Ready and Use to use the legacy right-panel/log fallback. The composer now accepts every typed selection mode.
2. Use item options encoded potion indices as `0..7` and scrolls as `8..15`, opposite the authoritative Use command table. The mappings are now scroll `0..7`, potion `8..15`. Party-target and direction-target handoffs still resolve through `CommandKind::UseItem`.
3. Ready now selects a party member when needed, then derives compatible inventory through `ready_items()` and executes `CommandKind::Ready`; equipment rules are not duplicated.
4. Party highlighting comes from the live `UiSession` cursor while a party selector is active, the current combat actor during combat, and the authoritative active character otherwise. `>` marks selection and `*` can distinguish the combat actor.
5. The main right frame previously ended on physical pixel 319 with no safety margin. Major gameplay/shop/selector frames now end at pixel 318; compile-time bounds checks enforce a maximum endpoint of 319. Small white line caps add restrained hierarchy without changing the 176x176 viewport.
6. The footer used ASCII `|`, but the 5x7 bitmap switch had no `|` case and deliberately rendered every unsupported byte as `?`. A native `|` glyph now exists; no encoding-dependent separator is used.
7. Roaming outdoor combat installed a victory callback that promoted combat chests into `QuestWorldServices`. Troll toll refusal and other direct encounter starts bypassed `outdoor_start()`, so they had no callback: the combat grid rendered the chest, then teardown discarded its identity and world Open correctly found nothing. Combat teardown now promotes any still-unclaimed non-dungeon encounter chest at the exact return map/location/floor/coordinate before destroying combat state. The outdoor callback still wins for roaming enemies and clears its cells, preventing duplicates.
8. Dungeon combat remains governed by dungeon/combat object-layer rules; fallback world promotion is deliberately disabled while `CommandContext::dungeon` is active. Corpses and non-chest splats remain combat presentation state, not world-openable objects.

## Device diagnostics

The serial path now records `LOOT_CREATE`, `CHEST_INSERT`, `CHEST_WORLD`, `CHEST_RENDER`, `OPEN_REQUEST`, every `OPEN_OBJECT` at the exact target coordinate, `OPEN_LOOKUP`, and `OPEN_RESULT`, including location, floor, coordinates, object index/type, trap data, contents, and tile.

## Performance and memory

Selector rows retain row-level dirty redraw. Moving a selector redraws the old/new rows and changed detail/context text. Party rows are small fixed retained regions; no gameplay full-screen redraw was introduced. Added persistent state is four small scalar fields plus one byte in the selector view; no new heap allocation, task, or stack buffer was added. The 24 KiB main task stack, PSRAM owners, input task, queue, Mic semantics, WASD, and trackball timing are unchanged.

## Validation and packaging

- Warnings-as-errors host build: passed.
- Native/non-Node host suite: 31/31 passed, including UI session, input, frontend/HUD geometry, presentation/Open integration, combat, magic, shops/services, and developer tools.
- Full configured suite: native tests passed; 22 Node/tsx fixture checks could not start because Node `uv_os_get_passwd` returned host `ENOMEM` before test code ran.
- ESP-IDF 6.1 ESP32-S3 warnings-as-errors build: passed; app size 779,680 bytes with 268,896 bytes free in the 1 MiB app partition.
- Static stack report: `AlphaRuntime::render` 2,144 bytes, `AlphaRuntime::command` 480 bytes, `Board::show_alpha` 352 bytes, `finish_encounter_combat` 112 bytes.
- `git diff --check`: passed (repository-wide line-ending notices only).
- Launcher: `C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-final\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`, 779,680 bytes, SHA-256 `895f7099a7d5eee72d295390a1149af75f4f4ee06307fb3ab8a964e701330cbe`.
- Resource pack: `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`, 1,840,139 bytes, SHA-256 `4e1fc6cd2733806232dbbd0af3bd6767b1d2ca58db8b3d2ce2d13a1531e50050`; SD destination `/ultima5/openu5-alpha1-resources.bin`.
- Tile pack unchanged: `C:\Dev\OpenU5-Native\native\assets\openu5-assets.bin`, 132,284 bytes, SHA-256 `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188`; SD destination `/ultima5/openu5-assets.bin`.
