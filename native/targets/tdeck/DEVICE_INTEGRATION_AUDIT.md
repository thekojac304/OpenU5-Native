# T-Deck device-integration audit report

Audit target: Alpha 1.4.0-alpha2 diagnostic firmware, 2026-09-17.

## 1. Smoke-test architecture

The device runner binds the loaded authoritative resource pack, `CommandContext`, `UiSession`,
device input adapter, and presentation layer. Isolated fixtures prevent diagnostic probes from
mutating the live game. It runs one scenario per render-loop pass, writes compact state/UI/
presentation hashes, and streams results to SD without retaining a full report in RAM.

## 2. Total scenarios

45 device scenarios in 15 groups, plus the existing 52-test host/reference suite.

## 3. Subsystems covered

Overworld, local maps, dialogue, shops/inns, inventory/equipment, combat, dungeons, shrines and
special prompts, transport, quest/progression, persistence, device input, developer tools,
resources, and presentation.

## 4. Automated totals

- Host/reference: **52 PASS, 0 FAIL**. The 30 native/non-tsx cases passed in the initial run; the
  22 TypeScript/reference cases passed when rerun outside a Windows sandbox restriction that made
  Node's `os.userInfo()` fail before test startup.
- ESP32-S3 target build: **PASS**, warnings-as-errors enabled.
- Focused authoritative sign/enemy resource tests: **6 PASS, 0 FAIL**.
- On-device smoke runner: **45 scenarios implemented; physical Run All result pending**. It must not
  be reported as a device PASS until the packaged image is run on a T-Deck with its SD resources.

## 5. Integration defects discovered

Movement-mode WASD was gated by broad base modes rather than the active session's semantic input
contract. Shop state executed but lacked visible phase prompts and item/service presentation. Shop
list cursors were also forwarded as item IDs. Shop Back always ended the service instead of using
the service's Cancel hierarchy. Inventory/equipment labels were composed from internal IDs. The
loaded resource report was copied before pack loading, leaving its counts stale for diagnostics.

## 6. Integration defects fixed

All defects above were fixed at the adapter/session/presentation boundaries. No shop economics,
combat rules, sign records, or other authoritative gameplay semantics were changed.

## 7. WASD directional-prompt fix

`UiSession::accepts_direction_input()` is now the semantic gate. With Movement Mode enabled, W/A/S/D
become North/West/South/East `UiAction::Direction` values anywhere the current mode accepts a
direction. Literal text and numeric entry remain characters. Regression coverage compares the
trackball/WASD path in exploration, dungeon, combat, target prompts, selections, and literal entry.

## 8. Shop/inn root cause and fix

The service emitted correct shop states and results, but the frontend did not turn phases into
visible prompts/choices and rendered no authoritative offer. `UiSession` now presents the shop and
keeper, phase-specific choices, list/deal/numeric/text prompts, results, Cancel hierarchy, and exit.
The frontend resolves list rows through `shop_offering_at`, then sends the authoritative item/member
ID. It renders the real name, price, and quantity for blacksmith/item, reagent/food/guild,
ship/service, tavern, healer, and inn paths.

## 9. Inventory/equipment naming root cause and fix

The runtime formatted `Equipment <id>` and similar labels directly. A central display-name resolver
now maps save/resource IDs to names extracted from authoritative DATA.OVL/reference tables. It is
used by equipment, spells, scrolls, potions, usable inventory, reagents, and shop presentation.
Unknown IDs are disabled and logged as `UNRESOLVED_NAME`; they are never silently presented as a
plausible generic item.

## 10. Generic-name leakage audit

All 48 equipment and 48 spell IDs, eight reagents, eight potions, and eight scrolls resolve. Named
usable/special items, guild goods, ship services, wine, party members, resource-backed enemies,
NPC dialogue identities, shops/keepers, and teleport/location destinations follow their existing
authoritative name sources. Representative parity asserts include equipment 4 `Spiked Helm`, 16
`Mystic Armour`, 38 `Silver Sword`, reagent 7 `Mandrake`, and spell 47 `An Tym`. Production UI
search found no remaining `Equipment/Item/Object/Spell/Reagent/NPC/Enemy/Shop/Location <id>`
formatters. Numeric indices remain only in explicitly developer-facing state editors.

## 11. Look/sign audit

No new sign-data defect was found. Look resolves map/floor/coordinate metadata through the sign
resource and emits the normal `Thou dost see` prefix plus decoded sign text. Tests cover Iolo's-hut
coordinates, overworld signs, a small-map law sign, and a shipwright sign, and assert that the `*`
sentinel never reaches transcript lines. The smoke runner additionally rejects blank sampled sign
records.

## 12. Post-combat routing

`CombatEnded` clears combat modal/target state and restores the exploration base mode. Host tests
confirm that L re-enters the normal semantic Look direction path rather than leaking a raw character;
the shortcut matrix also covers O/G/U/T/A. No fake combat Look command was added.

## 13. Single-letter shortcuts

The handheld shortcuts remain. Recognized letters dispatch the same `CommandKind`/selection path
used by authoritative commands; text and numeric modes retain literal input. Board/disembark,
dungeon, combat, inventory, spell, and exploration routing remain separately context-aware.

## 14. Still requiring physical validation

Mic and key feel, trackball responsiveness, visual flashing/tearing, font and wrapping appearance,
animation quality, marker/reticle visibility, shop/inventory readability, and overall screen layout.
The user-confirmed Alpha 1.4.0-alpha2 hardware results remain physically validated; new audit fixes
and the smoke runner are target-built/host-validated but not yet physically certified.

## 15. Estimated manual certification time

Approximately **20 minutes**, with a 15–30 minute target window.

## 16. Flash/RAM/PSRAM impact

The application image is 692,576 bytes (`0xA9160`), leaving 356,000 bytes (`0x56EA0`, 34%) in the
1 MiB app partition. Relative to the pre-audit packaged image (671,600 bytes), growth is 20,976
bytes. `DeviceSmokeTests` is 224 bytes; it retains only borrowed environment pointers, counters,
and two short status buffers. Selection labels grew by 1 KiB total. No new PSRAM allocation or
persistent heap-owned report was added.

## 17. Launcher binary

`C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha14-alpha2\launcher\OpenU5-TDeck-Alpha1.4.0-alpha2-Debug-Launcher.bin`

## 18. Smoke-test SD log

`/ultima5/logs/smoke-tests.log`

## 19. Short physical retest

Run All diagnostics; test Talk/Open with trackball and Movement Mode WASD; verify `wasd` remains
literal in dialogue; inspect an inn and item shop through purchase/Cancel/exit; inspect R/Use/Cast
names; finish one combat and immediately try L/O/G/U/T/A; open/Back/teleport in Developer tools;
scan for flashing, clipping, weak reticles, and layout instability.

## 20. Future roadmap

See `native/targets/tdeck/FRONTEND_ROADMAP.md` for the original-style intro/menu, settings, main UI,
future semantic touch buttons, and audio architecture. Those features were intentionally not
implemented in this audit.
