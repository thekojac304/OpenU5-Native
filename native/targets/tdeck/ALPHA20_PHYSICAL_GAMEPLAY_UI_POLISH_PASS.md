# Alpha 2.0 physical gameplay/UI correction and polish

## Root causes and corrections

- Shop text was sent through a partial formatter that expanded only keeper,
  shop, and time-of-day (`$`, `#`, `@`). Price, item, quantity, and rumor-place
  tokens (`%`, `&`, `^`, `*`) were copied literally, and `ShopResult` messages
  bypassed that formatter. `ShopRun` now performs one synchronous authoritative
  expansion before event delivery, including compatible `%s` item and `%d`/`%u`
  price records. Item names come from the shared DATA.OVL-derived display-name
  layer; prices and quantities come from the live `ShopSession`.
- The conversation-first state machine is unchanged. Greetings and authored
  results remain transcript events; compact lists are still adapters used only
  for concrete item/member/spell selection.
- Modal instructions were stored in `UiSession::prompt()` and the device
  renderer treated prompt/overlay state as extra transcript rows. A reusable
  two-row context action bar now owns live status and valid actions for shops,
  inns, selectors, target modes, yes/no, and bounded input. It occupies y=215
  through y=238, below a dedicated separator; transcript layout is capped above
  y=215 and fully reflowed on entry/exit.
- Combat aim was renderer overlay text injected into transcript layout. It is
  now live context state (`Aim: <name> (x,y)`) and is cleared with target mode.
  Attack/casting still dispatch through the existing command and magic engines.
- Small and Medium both used the same 5x7 renderer. Large used 2x horizontal
  and 1x vertical scaling, producing stretched glyphs. The new mappings are
  Small 4x6 in a 5x7 cell, Medium 5x7 in a 6x8 cell, and Large 7x9 in an 8x10
  cell. Size changes invalidate the complete right gameplay panel and recompute
  wrapping/row geometry. Title Settings, in-game Settings, and the System Menu
  use the same mapping; Settings displays a live sample.
- `Blocked!` was emitted once per rejected player movement command. Enemy AI
  path probing uses a silent path, and the transcript was not duplicating the
  event. Physical bursts therefore represent consecutive queued movement
  attempts. Consecutive equivalent messages are presentation-aggregated as
  `Blocked! xN`; combat turns, AI, pathfinding, RNG, and outcomes are unchanged.
  `COMBAT_TEXT` logs include the current actor and generated/presented totals.
- Combat loot byte bit 7 is trapped-chest metadata. Presentation previously did
  `encoded + 0x100`, so trapped chest 129 became resource tile `0x181` (the blue
  creature-like image) instead of chest tile `0x101`. Presentation now masks
  metadata first: `(encoded & 0x7f) + 0x100`. Body 30 remains `0x11e`; generic
  monster splat 31 remains `0x11f`. `CORPSE_RENDER` logs actor, definition,
  encoded remains, resolved tile, and resource class.
- Post-combat chest routing was already correct in the authoritative world and
  UI paths. Regression coverage verifies combat `Open`, CombatEnded restoration,
  an explicit direction request, and exact adjacent chest identity; no
  nearest-chest behavior was added.
- Spell selection already handed off to the authoritative spell/target state.
  Its compact selector was retained and moved onto the same typography and
  context-bar presentation.
- Developer Teleport now labels `Use default entrance`, initializes it On when
  opening Teleport or selecting a new destination, and still permits Off.

## Validation

- Warnings-as-errors native host build: pass.
- Focused shop/UI/presentation/input/magic/combat tests: pass.
- Full native, non-Node host suite: 30/30 pass.
- ESP-IDF 6.1 ESP32-S3 warnings-as-errors build: pass. App image 772,000 bytes;
  276,576 bytes (26%) remain in the 1 MiB app partition.
- `git diff --check`: pass (repository line-ending warnings only).
- Stack reports: `AlphaRuntime::render` 2,128 bytes; `Board::show_alpha` 352
  bytes; largest new formatter frame 864 bytes. Main task remains 24 KiB.
  No PSRAM allocation was added. Persistent internal state adds one context-bar
  model/cache plus small Blocked counters; the viewport/transcript owners and
  input task architecture are unchanged.

No device was connected for this pass, so watchdog and visual conclusions still
require the physical checklist. The watchdog configuration was not disabled or
lengthened, and the build contains the existing stack/input/render diagnostics.

## Packaged image

- Launcher: `C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-final\launcher\OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin`
- Size: 772,000 bytes
- SHA-256: `12b986b81b234075605bc57a5548f8e068f395dc099c3220ac5625c023e8df36`
- SD resource pack: `/sd/ultima5/openu5-alpha1-resources.bin`
- Source file: `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`
- Resource SHA-256: `4e1fc6cd2733806232dbbd0af3bd6767b1d2ca58db8b3d2ce2d13a1531e50050`
- SD tile pack: `/sd/ultima5/openu5-assets.bin`
- Tile-pack SHA-256: `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188`

The resource packs were not rebuilt by this pass.
