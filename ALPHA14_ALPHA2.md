# OpenU5 T-Deck Alpha 1.4.0 alpha2 diagnostic report

This correction build treats the supplied Alpha 1.4 serial trace as the
authoritative device record. It has been compiled, packaged, and host-tested,
but has not been flashed to a T-Deck Plus in this environment.

## Physical Mic identity

The physical Mic/0 key is bound to matrix `(0,6)` before base/Symbol character
translation. Base code `0x00` and Symbol code `0x30` therefore identify the
same semantic key. A short release emits Cancel; a 1.1-second hold toggles
movement mode; neither path can emit `0` or another gameplay character. The
former `(4,4)` assumption is removed.

The existing semantic Cancel hierarchy remains unchanged and is regression
tested as editor -> parent submenu -> Developer root -> gameplay.

## Developer renderer

The menu is now retained on screen. Entering it performs the one permitted
full draw. Later actions compare a cached `DeviceDebugScreen` and repaint only
changed opaque regions: old/new selection rows, a changed value row,
breadcrumb, position, or status. The world viewport is not recomposed while
the Developer menu is active. Teleport result text uses the same dirty path,
so no black frame is inserted.

The supplied physical trace contains 13 Developer action renders: 367,460 to
447,283 microseconds, averaging 407,685 microseconds. A normal selection move
in alpha2 transfers two 312x17 rows: 10,608 pixels / 21,216 bytes, whose 40 MHz
wire time is 4,244 microseconds before transaction overhead. The new
`DEBUG_RENDER` record reports `dirty_regions`, `pixels`, and actual `us`, so a
post-flash trace will provide the authoritative after-device latency. No
after-device timing is claimed by the host build.

## Keyboard transport correction

The prior driver requested a five-byte raw snapshot every 10 ms. The keyboard
C3 firmware scans 5x7 cells with a 1 ms settle per cell (about 35 ms per full
scan), and its raw-mode response supplies a five-byte snapshot without a raw
change interrupt. The trace also showed malformed high-bit snapshots followed
by zeros, which the old decoder accepted as state changes, and it retransmitted
the raw-mode command after a single read failure.

Alpha2 uses active-low INT falling edges when available plus a 45 ms fallback,
validates that only row bits 0..6 are present, and backs failed reads off from
60 to 250 ms. It preserves the last stable matrix, installs the first recovered
snapshot as a quiet baseline, and emits one resynchronization notification per
failure burst. Raw-mode command recovery starts only after eight consecutive
failures and is rate-limited to once per two seconds. Transient startup failure
keeps the device online for the same bounded recovery instead of disabling it.

## Command/corpse finding

The trace's `L -> What?` occurred while `ui=combat`; `CombatEnded` and the
combat-to-exploration mode transition appear later. This matches the reference
behavior: victory does not close the arena, dead enemies leave loot/corpse
tiles there, and combat has no Look command. No Giant Rat special case or new
combat Look behavior was added.

The actual transition hazard is corrected: `CombatEnded` now clears a stale
combat aim/selection modal before restoring exploration. Tests cover semantic
L/O/G/T/A and U routing in ordinary exploration and immediately after combat,
literal letters in text entry, and retained dead-enemy loot presentation.
Giant Rat identity, teleport semantics, and range-constrained melee targeting
are unchanged.

## Validation and artifact

- ESP-IDF 6.1 ESP32-S3 compile, link, image generation, and partition gate: pass
- Launcher image structural/checksum validation: pass
- Native, UI, input, debug, gameplay, parity, and fixture suite: 51/51 pass
- `git diff --check`: pass (line-ending notices only)

Launcher:

`native/targets/tdeck/build-alpha14-alpha2/launcher/OpenU5-TDeck-Alpha1.4.0-alpha2-Debug-Launcher.bin`

- Size: 671,600 bytes
- SHA-256: `468e29044c11142136488dffd9a82fff74ecd61046b83e2bca77957910f95fbe`
