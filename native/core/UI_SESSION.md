# Portable UI/session controller

This layer is the presentation boundary for the first playable-alpha.  It does
not render, know browser events, own a device, or contain a T-Deck key code.
`UiSession` accepts semantic `UiAction` values, emits `UiIntent` values to an
owner, and consumes ordered `GameEvent` values through `event_sink()`.

## Web-flow audit

The authoritative web path was audited at `main.ts`, `ui/prompt-manager.ts`,
`ui/talk-console.ts`, `ui/shop-console.ts`, the faithful console scrollback,
combat/dungeon key reducers, Ready/Use/Mix/party pickers, shrine pacers, and the
developer registry/panel/map picker.

The portable behavior retained from that path is:

- one live modal consumes all input before the world/dungeon/combat router;
- text input echoes a bounded editable buffer, numeric input accepts digits
  only, Y/N ignores unrelated input, and cancellation is request-specific;
- dialogue lines remain ordered with their prompt and dialogue input is sent
  through `DialogueText`; shop input remains a typed `ShopInput`;
- exploration command letters are disabled in text, menus, shops and combat;
  directional commands retain their two-stage command/direction behavior;
- combat has its own move/pass/escape/attack-target routes; dungeon directions
  map to forward/back/turn, not overworld movement;
- command echo is a distinct transcript channel, while combat, dialogue, shop,
  quest/system text stay distinguishable for a frontend;
- Escape/Back unwinds the current modal or menu level before it can reach an
  enclosing gameplay mode.

DOM nodes, Pixi/canvas drawing, browser `KeyboardEvent`, touch coordinates,
animation clocks, audio, and shell/panel layout were deliberately not ported.

## API and modes

`include/openu5/ui_session.h` defines the frontend-neutral input vocabulary:
Direction, Character, Confirm, Cancel, Back, Next, Previous, PageUp/PageDown,
SelectIndex, TextInput, and DeleteCharacter.

Supported modes are Exploration, Dungeon, Combat, Dialogue, Shop,
Shrine/Special, TextEntry, NumericEntry, YesNo, PartySelection,
InventorySelection, EquipmentSelection, SpellSelection, TargetSelection, and
DebugMenu.  Selection data is a borrowed callback source so a 256-item inventory
does not have to be copied into session memory.

The intent callback is synchronous.  A production owner maps an intent to the
existing `execute_command`, `execute_shop`, shrine, or explicit developer API,
then feeds emitted events back through `event_sink()`.  The session never writes
`GameState`, `TurnState`, combat, dialogue, shop, or dungeon state itself.

## Transcript and paging

The owner supplies a ring of `UiTextBlock`.  Each block is 168 bytes on the host
and holds up to 159 UTF-8 bytes plus ordering/channel/continuation metadata.
Long messages are split into adjacent blocks.  History is strictly bounded and
oldest-first ordering is preserved across ring wrap.  `visible_lines()` wraps
to a configurable character-cell width and returns a configurable page from
the current scroll offset.  Pixel dimensions do not appear in this module.
The current prompt remains separately queryable even if its transcript block
has rolled out.

## Developer menu

When `OPENU5_ENABLE_DEVELOPER_TOOLS=ON`, `UiDebugMenu` adds an 11-category,
hierarchical controller over the already translated developer APIs:

1. Teleport / Map Picker (destination, floor, coordinates, standard entry)
2. Party
3. Stats
4. Inventory
5. Equipment
6. Reagents
7. Quest / Progression
8. Time
9. Transport
10. NPC / Dungeon State
11. Shortcuts / Presets

The model supports navigation, value editing, toggles, actions, back/cancel,
teleport application, all four shortcuts, and all ten deterministic presets.
The source is absent from the production source list and no session debug pointer
exists when the option is off.

## Host harness and tests

`ui_host_harness` accepts semantic commands on stdin.  It can switch gameplay
modes, walk, run a two-stage Talk and dialogue response, simulate a shop
transaction, open party/inventory/equipment/spell selectors, drive combat aim,
page the transcript, and (in a developer build) navigate teleport and preset
menus.  It is intentionally not a PC renderer.

`ui_session_tests` has 49 checks covering command routing, text isolation, yes/no, numeric input,
cancellation, selections, dialogue, shop, combat targeting, ordered/bounded
history (including wrapping across storage-block boundaries), paging,
prompt-derived commands, and gameplay mode switching. `ui_debug_menu_tests`
has 10 checks covering hierarchy navigation, numeric editing through the real
developer API, teleport application, Full Max Party, back/close, and
session/debug switching.

## Memory contract

Measured Windows x64 values are 424 bytes for `UiSession` without developer
tools (432 with its debug pointer), 160 bytes for `UiDebugMenu`, 168 bytes per
history block, and 104 bytes per temporary rendered line.  Example bounded
history storage is 1,344 / 2,688 / 5,376 / 10,752 bytes for 8 / 16 / 32 / 64
blocks.  The implementation calls no allocator.  Compiler `-fstack-usage`
measurement at `-Os` reports a largest UI-session frame of 296 bytes
(`append_utf16`) and a largest debug-menu frame of 120 bytes (`apply_action`).
The former includes the 160-byte UTF-16-to-UTF-8 conversion chunk. Rendered
page arrays belong to the caller and must not be placed in a small task stack.

The ESP32-S3 build compiles the release UI object to 9,874 bytes of text and
zero data/BSS. Until a device adapter references the controller, section
garbage collection removes it from the final image: the measured app binary is
unchanged at 342,288 bytes. The object size is therefore a conservative ceiling
for retaining every controller function; a thin adapter will normally retain
only the referenced function sections.

For the future device adapter, keep `UiSession` and the small debug model in
internal RAM.  Put a 32- or 64-block transcript ring, rendered glyph/cache data,
large selection catalogs, and visual assets in PSRAM.  The frontend should use
small reusable line buffers and never allocate a complete transcript or screen
on the app-task stack.

No T-Deck display, keyboard, trackball, touch, board, audio, or FATFS source is
part of this layer.
