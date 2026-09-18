# T-Deck deterministic smoke tests

The device runner validates the integration path from authoritative resources and native core
through `UiSession`, the device input adapter, `PresentationSnapshot`, and the T-Deck frontend.
It is intentionally not a replacement for the deeper host/reference parity suite.

## Running on the device

1. Insert the normal game/resource SD card.
2. Open `Developer > Diagnostics / Smoke Tests`.
3. Choose `Run All`, or choose one subsystem group.
4. The screen shows the group, current scenario, completed/pass/fail counts, and first failure.
5. Read the complete report from `/ultima5/logs/smoke-tests.log` on the SD card.

No serial connection is required. Each render-loop pass executes at most one short scenario, and
the runner keeps only counters, the current scenario, and the first failure in RAM. It opens the
log only for a short append, avoiding a large in-memory report and long synchronous write burst.

## Architecture

The runner binds borrowed references to the live `CommandContext`, loaded `AlphaResourceOwners`,
resource-pack reports, and `UiSession`. Scenarios either inspect those live owners or create a
small isolated state/session fixture so diagnostics cannot alter the player's current game. Each
result includes an exact reason plus compact FNV-1a hashes for relevant authoritative state,
UI storage, or presentation state.

The 45 scenarios are split evenly across 15 groups:

| Group | Scenarios |
| --- | --- |
| Overworld | movement/passability; visibility/terrain snapshot; Look/sign/direction prompt |
| Local Maps | entry/exit resources; doors/object overlays; local Look/boundaries |
| Dialogue | Talk direction; literal text/keyword isolation; transcript wrap/Cancel |
| Shops / Inns | shop identities/types; real offer names/prices/row mapping; numeric/yes-no/Back/exit |
| Inventory / Equipment | equipment parity; usable/reagent/spell names; generic-ID scan |
| Combat | arena/enemy identity; target/active marker; victory cleanup/shortcuts |
| Dungeons | floors; corridor/fixed encounters; loot/exit plumbing |
| Shrines / Special | authoritative shrine text; special prompts; isolated quest mutation |
| Transport | transport domain; board/disembark; direction/collision services |
| Quest / Progression | search/special fixtures; Shadowlord/shards; endgame flag plumbing |
| Persistence | JSON round trip; GAM/OOL generation; corrupt-newest recovery selection |
| Device Input | trackball/Mic; Movement Mode WASD directions; text/numeric/shortcut isolation |
| Debug Tools | open/Back; teleport/presets; isolated value edit/gameplay restoration |
| Resources | pack identity; map/dialogue/shop/sign payloads; enemy/NPC/location names |
| Presentation | viewport/animation hash; transcript/shop/inventory UI; reticle/active marker |

## Interpreting results

`PASS` means the deterministic condition passed on that exact firmware and loaded resource pack.
`FAIL` includes a concise reason and the three optional hashes. A device run is still required
before calling the device harness physically validated; a successful target build or host test
does not establish that result.

