# OpenU5 T-Deck Plus Alpha 1

This is the first T-Deck build that runs the native OpenU5 game state and
`UiSession` controller instead of the Milestone-5 movement prototype. It was
built, packaged, and host-tested without access to a physical T-Deck Plus.
**Hardware success has not been claimed or established.**

## Files

| File | Size | SHA-256 |
| --- | ---: | --- |
| `native/targets/tdeck/build-core/launcher/OpenU5-TDeck-Alpha1-Launcher.bin` | 634,848 bytes | `e72213fdcda560088206f529afb566630c916ba5b15686afbf2f9fee5fcbea09` |
| `native/assets/openu5-assets.bin` | 132,284 bytes | `6eb001ed2a7729e683896998f693d02aeaf1327f3cddd661d1c726ef7414e188` |
| `native/assets/openu5-alpha1-resources.bin` | 1,227,341 bytes | `c1758446b4d446cd8aabc586b408b7d388501c1471278eb4bfa3bb0c6fc9b988` |

The firmware is a normal ESP32-S3 application image beginning at offset zero.
Launcher needs an app allocation of at least 655,360 bytes (640 KiB, including
64 KiB alignment).

## Install and SD-card layout

1. Generate the two asset packs from a legally obtained local Ultima V data
   extraction with `npm run pack:native` and `npm run pack:alpha1`. Game data is
   deliberately not embedded in the firmware.
2. Copy the two generated packs to the paths below on a FAT-formatted microSD.
3. Copy the Launcher binary anywhere Launcher can browse on that card.
4. In Launcher, choose **SD**, select
   `OpenU5-TDeck-Alpha1-Launcher.bin`, and install it into an app allocation of
   at least 640 KiB.
5. Leave the SD card inserted when starting OpenU5.

```text
/
|-- OpenU5-TDeck-Alpha1-Launcher.bin       (location is not significant)
`-- ultima5/
    |-- openu5-assets.bin
    |-- openu5-alpha1-resources.bin
    `-- saves/                             (created automatically)
```

`openu5-alpha1-resources.bin` is the versioned `OU5A1RES` 1.1 pack. It contains
the native initial state, world and small maps, dungeons, NPC schedules,
objects/world tables, compact dialogue, combat maps/enemies, shops, shrine and
Look data, the coordinate-indexed decoded/raw sign table, plus retained extracted
JSON inputs. Its header, directory, payload, and every required entry are
CRC-validated during startup. Firmware that requires 1.1 rejects a stale 1.0
pack rather than exposing LOOK2's `*` sign sentinel.

## Controls

- Trackball: move in the world; navigate menus, pickers, targets, and the
  developer menu.
- Shift + trackball up/down: page through long transcript text.
- Enter: confirm a prompt or selection. Space passes a turn where applicable.
- Backspace: delete during text/number entry; otherwise go back.
- `$`: cancel the current prompt/menu.
- Keyboard letters: Ultima commands during exploration and context-specific
  commands in combat, dungeons, dialogue, and shops.
- Numbers and text are consumed by their active prompt and do not leak into
  gameplay commands. Symbol+number directional movement is not supported.
- Alt+D: developer menu. Alt+S: save. Alt+L: load.

The exploration command set is Attack, Board, Cast, Enter, Fire, Get, Hole Up,
Ignite, Jimmy, Klimb, Look, Mix, New Order, Open, Push, Ready, Search, Talk,
Use, View, X-it, Yell, Z-stats, and Pass. Direction, party, item, equipment,
spell, target, text, number, and Yes/No prompts are presented through the real
`UiSession` state machine.

## Screen and UI modes

Alpha 1 uses the existing tile renderer in a 176x176 game viewport with a
readable transcript/status area. It renders outdoor and small-map exploration,
a compact dungeon view, and combat arenas. Implemented overlays cover command
echo/status, transcript paging, text/numeric/Yes-No prompts, party selection,
inventory/equipment/spell/target selection, dialogue, shop flows, combat,
dungeon actions, shrine prompts, and the developer menu. Correctness and
readability take priority over animation or decorative layout.

## Developer menu

Developer tools are built into this image with
`OPENU5_ENABLE_DEVELOPER_TOOLS=ON`. Press Alt+D, use the trackball to navigate,
Enter to select, and Backspace or `$` to go back. Numeric edit fields also
accept keyboard digits.

The menu exposes Teleport/Map Picker, Party, Stats, Inventory, Equipment,
Reagents, Quest/Progression, Time, Transport, NPC/Dungeon State, and
Shortcuts/Presets. Presets include MaxedParty, StockedInventory, Combat,
Dungeon, Shrine, Quest, Transport, Endgame, LowHealthStatus, and SaveLoad.
Presets establish legitimate native state; use Teleport/Map Picker and the
normal command appropriate to that state to enter the desired scene.

## Save and load

Alt+S captures complete native state with the existing GAM, OOL, and JSON
envelope codecs. It writes a new two-slot generation to temporary files,
flushes and closes each file, reads it back to verify size and CRC, renames the
three payloads, and commits the generation marker last. The preceding complete
generation is retained. Alt+L validates both generations and loads the newest
complete one, falling back automatically when the newest is incomplete or
corrupt.

Files are stored as `/ultima5/saves/alpha1-g{0,1}.{gam,ool,json,commit}`.
C++ structs are never serialized directly.

## 15-30 minute physical smoke test

Keep a serial log for the whole run so the boot, timing, memory, stack, and SD
metrics can be reviewed.

1. **Boot (2 min):** install from Launcher, start the app, and confirm both
   packs validate, the world viewport appears, and no low-RAM/low-stack warning
   is logged.
2. **World and location (2 min):** move several tiles with the trackball; use
   Alt+D -> Teleport/Map Picker to choose a town, then `E` to enter and `K` or
   the appropriate exit route to return.
3. **Dialogue and text (2 min):** face an NPC, press `T`, answer a free-text
   keyword, verify typing does not move or invoke commands, and leave dialogue.
4. **Shop (2 min):** teleport to a shop, use `T`, exercise both Buy and Sell,
   navigate an offer and party selection, and decline/leave cleanly.
5. **Inventory/equipment (2 min):** apply StockedInventory, use `U`, `R`, and
   `Z`; select an item and equipment entry and verify status/transcript updates.
6. **Combat and magic (3 min):** apply Combat, trigger the nearby encounter,
   move/target/attack, cast a spell, and confirm combat messages and arena
   redraws.
7. **World interaction (2 min):** use Look, Search, Open/Get, and Push on
   suitable nearby tiles or objects; verify direction prompts and turn results.
8. **Dungeon (2 min):** apply Dungeon, teleport to a dungeon entrance, enter,
   move between cells/floors, perform one dungeon action, and exit.
9. **Transport (2 min):** apply Transport; Board, move, and X-it with at least
   one available transport type.
10. **Shrine/quest (2 min):** apply Shrine or Quest, teleport to the matching
    site, exercise Yes/No plus virtue/mantra or numeric donation prompts, and
    inspect quest state in the developer menu.
11. **Save/recovery (4 min):** apply SaveLoad, note position/stats/inventory,
    press Alt+S, make visible changes, reboot, press Alt+L, and verify the noted
    state. If practical, interrupt or corrupt only the newest generation and
    verify fallback to the previous committed generation.
12. **Debug reachability (1 min):** reopen Alt+D, teleport once, apply
    LowHealthStatus, and confirm Back returns through every menu level.

## Instrumentation and memory placement

The ESP-IDF static report records 407,306 bytes of flash code, 143,892 bytes of
flash data, and 89,386/341,760 bytes of DIRAM (26.15%; 252,374 bytes unassigned
statically). The application image is 634,848 bytes and leaves 413,728 bytes
in the 1 MiB standalone build partition.

The main task stack is 12,288 bytes. Explicit PSRAM owners include the
323,084-byte general A* scratch arena, 16,128-byte 96-block transcript, and
61,952-byte viewport. Resource owners consume approximately 0.54 MiB more;
combat overflow storage, the currently expanded dialogue script, resource
caches, and persistence buffers are also external-RAM allocations. The
4,096-byte allocation threshold routes large standard-library allocations to
PSRAM while 49,152 bytes of internal RAM is reserved for DMA/internal-only
needs. `UiSession`, `UiDebugMenu`, hot state, and DMA-sensitive driver memory
remain internal.

At boot and every five seconds, the firmware logs free internal heap, free
PSRAM, main-task stack high-water margin, render and command high-water times,
and transcript use. It warns below 32 KiB free internal RAM or 4 KiB stack
margin. Resource/dialogue PSRAM allocations, every render, command timing,
save/load duration, and SD errors are also logged. Actual runtime free-memory,
stack, and timing values require the physical device and are not reported as
measured here.

## Known Alpha 1 limitations and first hardware checks

- No physical-device run has been performed for this integration. First check
  boot/resource validation, shared-SPI SD/display behavior, trackball and raw
  keyboard edges, free PSRAM/internal RAM, and the logged stack margin.
- Touch fallback was deferred because adding an unverified touch path would
  raise input/I2C risk. Trackball is the primary control.
- Audio is explicitly deferred and is not an Alpha 1 blocker.
- Horse-seller completion still needs the device runtime's world occupancy and
  horse-tile callbacks. Other native shop paths, including buy/sell, are wired.
- Original shrine/Codex narrative records and some endgame narration/power-word
  providers are not yet present in the compact resource owners; the native
  quest/progression state and debug presets are wired, but those missing
  resource-dependent endpoints can report `InvalidContext` rather than invent
  dialogue.
- Rest uses the native state transition with a generic karma result message
  because extracted `KARMA.DAT` prose is not in this pack.
- The renderer and input mappings prioritize function over polish. Combat,
  dungeon, long dialogue, picker scrolling, and save/load under simultaneous
  SD/display use deserve the first focused hardware pass.

These limitations define the Alpha 1 stop point; audio work, visual polish, HD
or isometric rendering, and unrelated ports are intentionally outside it.
