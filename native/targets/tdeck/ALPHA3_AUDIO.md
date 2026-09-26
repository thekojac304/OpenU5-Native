# Alpha 3 — Audio (A3-01 architecture, A3-02 PC-speaker synthesizer, A3-03 remaining SFX)

**Status: A3-03 COMPLETE (the remaining gameplay SFX, the ambient proximity sounds, combat victory and the scene cues). Not an Alpha 3 release.** The device now plays 62 of the 73 cue ids, synthesized from the 1988 binary's own primitive parameters; the 11 that stay silent are each classified, with the reason, in §16.18. Every PC-speaker call site of the shipped binaries — 121 found by the census, 5 by hand — is classified in a table the tests enforce (§16.10). There is no music playback yet (A3-04). A3-01 (sections 1–14) is the architecture, §15 is A3-02, and §16 is A3-03.

This document is the audio track's reference. It records what the original does, what the community music patch adds, how the port tells the two apart, and the contracts later batches must keep.

## 0. The rule in one table

| The user's DOS files | Sound effects | Music | What Settings shows |
|---|---|---|---|
| **Stock** (unpatched *Ultima V* DOS) | Supported. The effects are the original's PC-speaker sounds, synthesized from the original's own parameters: 22 since A3-02 (§15.5), 62 of the 73 cue ids since A3-03 (§16). No asset is needed. | **None.** The 1988 game has no music. | `Music Volume: Unavailable`, footer *Stock DOS game files have no music* |
| **Supported music patch** (Exodus Project *Ultima V Upgrade* 1.0) | The same. | **Enabled** (playback arrives in A3-04). | `Music Volume: 80%`, adjustable |
| **Incomplete patch** (some of its files) | The same. | None. It is never guessed. | `Unavailable`, *Music patch files are incomplete* |
| **Unknown music variant** (another driver or foreign XMI files) | The same. | None. It is never guessed. | `Unavailable`, *Unsupported music patch variant* |
| No audio pack on the card, or a stale/corrupt one | The same. | None. | `Unavailable`, *No audio pack: npm run pack:audio* or *Audio pack stale or corrupt: rebuild* |

In every row the game is fully playable. None of these states changes a save, the game pack or any gameplay rule.

## 1. Baseline (Phase A)

- HEAD `4b3257f4` = tag `alpha2-batch55-release` (annotated tag object `a2dce734`). The tree was clean.
- The Alpha 2 firmware source is `211c676a` (`alpha2-batch54-rc1`).
  - Launcher SHA-256 `ff3dfe19…5828`.
  - `0xd68a0` = 878,752 B, which leaves 169,824 B free in the 1 MiB app partition.
- Fresh host build and **serial ctest: 123/123 passed in 116.19 s.** The only warning is the known w64devkit `stl_uninitialized.h` false positive. Logs: `native/core/a3-01-baseline-host-build.log`, `native/core/a3-01-baseline-ctest.log`.
- Game resource pack:
  - `openu5-alpha1-resources.bin`, format OU5A1RES 2.0, 42 named entries.
  - 2,041,466 B, payload CRC `0x26f75ae6`, SHA-256 `a48abdbf…b379b`.
  - The firmware locks on the exact size and CRC (`alpha_resources.h`).
  - This format has no optional sections and no flags, and until now it had no written specification. It is defined by `native/tools/u5pack/alpha1.ts` and the reader in `alpha_resources.cpp`.
- Settings:
  - `openu5::FrontendSettings` is saved as `/sd/ultima5/settings.json`, JSON with version 1, through `AlphaSettingsService`.
  - It already stored `soundVolume` and `musicVolume` (0–100, default 80). They had been reserved and inert since Alpha 2 (ledger D-3 / Y-03). There is no NVS.
- Audio before A3-01: none.
  - No I2S code, no audio pins, and `esp_driver_i2s` was not required.
  - The core already emitted about 45 semantic `GameEventKind::Sfx` cues, with 31 distinct ids. The device dropped all of them; the only trace was one log line for Refuge beats.

## 2. T-Deck Plus audio hardware (Phase B)

The sources are LilyGO's own board files, not generic ESP32-S3 knowledge:
- [`examples/UnitTest/utilities.h`](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/UnitTest/utilities.h)
- [`examples/SimpleTone/SimpleTone.ino`](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/SimpleTone/SimpleTone.ino)
- the [T-Deck Plus wiki](https://wiki.lilygo.cc/products/t-deck-series/t-deck-plus/) pin table

Every pin the repository already defines matches `utilities.h`.

| Item | Finding |
|---|---|
| Speaker path | An I2S-input speaker amplifier. **TX only**: `BOARD_I2S_BCK` GPIO 7, `BOARD_I2S_WS` GPIO 5, `BOARD_I2S_DOUT` GPIO 6. There is no MCLK and no control bus. LilyGO's sources do not name the amplifier chip. It is commonly reported as a MAX98357A, but that is not verified here. Nothing in the design depends on the part number. |
| Power | The board's peripheral enable, GPIO 10. `Board` already drives it high at boot. |
| Microphones | ES7210 ADC on a **separate** I2S path (MCLK 48, LRCK 21, SCK 47, DIN 14). It is not used. |
| Sample format | LilyGO's tone example uses 16-bit PCM, one slot (`ONLY_LEFT`), Philips I2S, 8 kHz, and DMA of 8 × 64. Any standard rate works. The A3-01 backend uses **16 kHz, 16-bit, mono** (the one sample goes to both slots). |
| Volume | There is no hardware volume register (LilyGO's player example scales in software). **Volume is digital**: `apply_gain_q15()` scales the samples, saturating. |
| Drivers in the repo | None before A3-01. A3-01 adds `esp_driver_i2s` to `main` and uses the ESP-IDF 6.1 standard-mode API (`i2s_new_channel`, `i2s_channel_init_std_mode`). |
| Asynchrony | Yes. A dedicated `openu5-audio` task pinned to **core 1**; today no application task runs there, while the game loop and input capture are on core 0. The game thread only posts to a queue with timeout 0. The DMA uses `auto_clear_after_cb`, so an underrun sends silence, never a looping stale buffer. |
| Cost (A3-01 backend) | Created on first use only: 2 KiB DMA (4 × 256 frames × 2 B), a 3 KiB task stack and a 4-entry queue. An effect synthesizer (A3-02) adds tens of bytes of state. Music (A3-04) needs the XMI (≤ 7.3 KB per song) plus the 3.6 KB bank, and an OPL2 emulation. The TypeScript reference's own emulator is `game/src/ui/opl/chip.ts`. That is the one real CPU cost. It belongs on core 1 at 16–22 kHz, and A3-04 must measure it. |

## 3. Architecture

```
 game logic (core)                     GameEvent{Sfx, "move-blocked"}, scene cues
      │  semantic cue id only           (no Hz, no file names, no hardware)
      ▼
 AlphaRuntime::present_audio()         at PRESENTATION time: immediate events in
      │                                consume_event, paced ones in release_scene_event,
      ▼                                Refuge beats in narrative_beat
 openu5::AudioService (core)           channels, volumes, capability, dedupe, stats
      │  SfxRequest / MusicSong + Q15 gain
      ▼
 openu5::AudioBackend                  device: tdeck::TdeckAudioBackend (I2S, core 1)
                                       host:   recording backends in the tests
                                       none:   NullAudioBackend / nullptr = silence
 audio pack (SD, optional) ──► AudioPackInfo ──► MusicAvailability (service + Settings)
 settings.json ──► FrontendSettings.sound_volume / music_volume ──► service volumes
```

- The core emits semantic ids only. The native cue strings are one closed vocabulary, `openu5::SfxId` (`openu5/audio.h`):
  - the 52 ids of the reference catalogue, in `game/src/core/sfx.ts` order;
  - 4 native hook names;
  - 1 diagnostic id.
  - A test scans `native/core` and fails if any emitting line uses a cue outside it.
- `AudioService` never sees GameState, the clock or an RNG. The backend never calls back into the service or the game.
- The runtime hands a cue to the service **when it is presented**, never when it was emitted. A paced scene's cues therefore follow the scene pacer (proved by T20).
- `AlphaRuntime::configure_audio(pack, backend)` is the single seam:
  - `main.cpp` calls it after the identity gate;
  - host tests call it after the fixture.
- Two new shared binders close a known trap: the host fixture used to *copy* production wiring.
  - `load_device_settings()` (settings.json → input + audio);
  - `bind_developer_diagnostics()`;
  - `initialize()` and the fixture both call them.

## 4. Original SFX / tone inventory (Phase C)

**Primary evidence:** a fresh census of the shipped binaries, `re/tools/a3_01_sound_census.py` → `native/core/a3-01-sound-census.log`. It uses Batch 51's overlay-base mechanics and every call site of the six PC-speaker primitives.

| Primitive (kernel) | Sites (this tree) | `re/notes/sfx-catalog.md` §2 | Parameters | Mute branch |
|---|---:|---:|---|---|
| `tone_sweep` 0x2192 | 44 | 44 | (inc, delay, count, start, step). The pitch comes from `inc`; `count` is the duration. | **0x21c4 runs the same loop with the gate closed**, so it blocks just as long with sound off. |
| `noise_burst` 0x223c | 25 | 25 | (step, dur, band). It has a local PRNG `[0x545c]` and **never touches `g_rng`**. | same loop, silent |
| `glide` 0x43ae | 31 | 27 as totalled; its per-overlay rows sum to 30 + 1 = 31 | (start, end, step, total) set_tone ramp | delays kept |
| `set_tone` 0x22e2 / `stop` 0x230e | 8 / 5 | 8 / 5 | PIT ch2 divisor `0x1234DE / f` | gate stays closed |
| `beep` 0x22c0 | 8 | 2 (before §10/§11) | (freq, dur) | `delay` kept |
| **All** | **121** | 111 | | |

The beep sites are:
- kernel 0x42a1;
- FONT 0x0403;
- MAINOUT 0x0344 and TOWN 0x0849 (the wall bump, §10);
- SJOG 0x1c1a / 0x1d59 (the same `beep(0xa5,0xc8)`);
- SJOG 0x1f5a / 0x1f62 (not attributed).

The catalogue's kernel beep at 0x429c is not found by the linear sweep (an alignment gap), so the true count is at least 9. The sound flag is `[0xa9ce]`. With it at 0, every primitive still runs its timing loop.

**Semantic families.** Each is one `SfxId`. The parameters are in `sfx-catalog.md` §6 and in the `audio.cpp` table comments. "Native" is what `native/core` emits today.

| Family | Original site(s) | Timing relation | Native emits | Rebuildable from parameters? |
|---|---|---|---|---|
| Combat hit / heavy / damage / defeat | kernel 0x35de, 0x35c9, 0x2a68, 0x2fe3 (NB) | inline, short | `combat-damage` (dungeon traps); the arena hits are **not emitted yet** | yes |
| Victory fanfare | kernel 0x4368 (TS ×4), COMBAT 0x0d02 | blocks about 1.7 s; the reference pauses the arena for it | `victory-fanfare` (shard) | yes |
| Spell ceremony (spell-cast / potion-used / scroll-used + `MagicCeremony(i)`) | CAST2 0x0000: NB lead, then 2 mirrored sweeps with `count = 0x2710 + 0xfa0·i` | the inverted-viewport window **is** the sweeps. Already modelled in `start_magic_ceremony` (async). | yes (native hook names) | yes (tables 0x4af6…) |
| Per-spell zap / line spray / time spell | CAST/CAST2 | inline | not yet | yes |
| Shrines: donation, ordained melody, well done | CAST2 0x0bc0–0x0e80 | inline; the quake follows | yes (3) | yes |
| Sceptre, moongate, quake, waterfall | kernel 0x6221, 0x48e5; screen-shake; OUTSUBS 0x0492 | moongate transit stages are `delay(2)` ticks (exact) | yes (4) | yes (quake is class C) |
| Shadowlord announce drone | TOWN 0x11e9, TS count 60000 (about 2.3 s) | blocks | yes | yes |
| Harpsichord / instrument note | TOWN 0x0e6d, note table DATA.OVL `[0x2746]` | per key | yes (`note` = digit); silent on device (H-125) | yes |
| Healer jingle (shop) | SHOPPES 0x13b0, TS ×6 | inline | not yet | yes |
| Cannon, escape, absorbed, ring vanishes, torch borrowed | CMDS 0x09d5 / 0x18ac, SJOG 0x1f08 / 0x1a21, ZSTATS 0x0e42 (GL) | inline | cannon, ring, torch | yes |
| Footstep, wall bump, search fail | kernel 0x433e (NB ×2); MAINOUT 0x0344 / TOWN 0x0849 beep | per step | `move-step`, `move-blocked` | yes |
| Dungeon trap / zap / field afflict / fail | kernel 0x2fe3; DUNGEON 0x04b9, 0x099e / 0x0a30, 0x1cfb | per event | yes (4) | yes |
| Ambient: fountain, waterfall, clock tick/tock/chime, Codex hum | kernel `ambient_sfx_tick` 0x4102 (proximity) | idle repaint cadence | not yet | yes (the Codex condition is not derived) |
| **Camp apparition**: materialize, arpeggio, heal chime, chord | OUTSUBS 0x067b / 0x0698 / 0x0896 / 0x08c1 | **the scene's holds (Batch 51)**: 387 / 1162 / 193 / 2325 ms at 25,806 samples/s | yes (4), paced | yes (table 0x3a26) |
| **Blackthorn** materialize sweep, sacrifice siren | BLCKTHRN 0x083f, 0x03d0–0x040f | **scene holds** (503 ms; siren 7.1 s) in `BlackthornScenePacer` | beat fields only (`BlackthornSfx`), not routed yet | yes |
| Refuge thunder | BLCKTHRN party_refuge | narrative beats | `refuge-thunder` (beats) | yes |
| Intro thunder / chime / summon, title fizzle / crackle | FONT 0x03ca / 0x088d | intro scenes | `IntroViewFrame` flags, not routed | yes |
| Bard song (Codex / lute) | tables 0x6a48 / 0x6a34 | Camp sleep loop | not yet | yes |
| Endgame orb / fanfare | ENDGAME 0x078f / 0x0987 | ending scenes | not yet (D-54) | yes |
| Flash-border chirp | kernel 0x3072, pitch from `rand_range` | **consumes `g_rng`** | not emitted | the RNG draw belongs to the game step, not to audio |

**Silence branches whose timing mattered** (Phase C / H). These are already handled; A3 must keep them:
1. `tone_sweep` blocks **with sound off** (mute branch 0x21c4). The device reproduces those waits silently in the scene pacers (D-39 / D-10, Batch 51). **Audio completion must never become the pacer again.**
2. Sound-flag gameplay branches (Batch 51 §13.5):
   - with `[0xa9ce] = 0` the original drags the blindfolded party 3 times instead of 18;
   - with `[0xa9ce] = 0` it skips the bard's lute.
   - The device keeps the sound-ON drags and the sound-OFF lute.
   - **`SFX Volume` is an output gain. It is NOT the 1988 sound flag.** Setting it to 0 mutes the output and takes neither branch (see §10).
3. `fx_tile_fizzle_in` has no timer (class C, a one-tick floor) and no sound. `delay` / `run_n_frames` are real ticks (exact).

**Reconstructible vs asset.** Every effect above is fully described by the original's parameters and needs **no asset**. What is not derivable statically is timbre and absolute Hz: the gate is bit-banged at a CPU-calibrated rate. The project's one calibration, `SPEAKER_SAMPLE_RATE_HZ = 24000 / 0.93 = 25,806` (`game/src/skin/fiel/speaker.ts`), is already the scene-timing constant (`scene_timing.h`). A3-02 renders with it, so rendered durations equal the paced holds **by construction**. They are never used to drive them.

## 5. The community music patch (Phase D)

The development install `original/u5/ultima5/` is the **Exodus Project *Ultima V Upgrade*, Release 1.0** (Michael C. Maggio / "Voyager Dragon", 2001-08-21), applied in place:
- the patched `ULTIMA.EXE` is dated 2001-08-20;
- `DATA.OVL`'s driver slot reads `MID.DRV`.

The stock copies once kept under `original/u5/play` and `original/u5/ultima5/upgrade` are no longer in this tree. The full derivation of the selector is `re/notes/music-location-mapping.md`; this section records what the port needs.

**What the patch adds or changes** (from its `Files.txt`, checked against the tree):

| Kind | Files |
|---|---|
| Music data | 16 XMI songs (`U5THEME` … `AMIGA`), 780–7,246 B each, 51,598 B total. `setm.xmi` is MIDPAK's own test song. |
| Timbre bank | `FAT.OPL` (3,622 B, a Miles AIL Global Timbre Library: 181 two-operator timbres = 128 GM melodic + 53 percussion). `MIDPAK.AD` is the copy SETM installs (byte-identical here). |
| Song selector | `mid.drv` (823 B), the port's anchor. SHA-256 `d5a0d0c2ce93e782aa72fe8f340e4a406c4c43ac1e862bf95a9ed49e5b77b7c2`. |
| MIDPAK kit (DOS only) | `*MIDPAK.COM`, `*.ADV` / `*.ADD`, `SETM.EXE`, `U5.CFG`, `u5cfg.exe` |
| Patched code (DOS only) | `ULTIMA.EXE`, `INTRO` / `FONT` / `MAINOUT` / `TOWN` / `DUNGEON` / `ENDGAME.OVL`, `ULTIMA5.COM` |
| `DATA.OVL` | `u5data.exe` rewrites the unused Tandy driver name at fileoff 0x5350: `T1K.DRV` → `MID.DRV`. This is 3 bytes, and it is the only change to that file. |

**The format** is XMI (Miles eXtended MIDI, IFF):
```
FORM:XDIR { INFO(u16 = 1) }
CAT :XMID { FORM:XMID { TIMB, EVNT } }
```
- Every song has exactly one sequence.
- `EVNT` uses 120 Hz ticks, note-on events carry their duration, and each song has 1–2 tempo metas.
- Songs last 11.9 s (Reunion) to 144.8 s (Stones) at 120 Hz.
- It is played on OPL FM with `FAT.OPL`: the reference's AdLib/SB route, and the one `game/src/ui/opl/` reproduces.

**Does the port need the patched code?** No. The selection logic lives entirely in `mid.drv` 0x016d, and the port re-implements it as a pure function: `openu5::music_context_for_location()`, tested for every `g_location`. The port needs:
1. the song data (XMI);
2. the timbre bank;
3. **proof that the install's selector is the one it copied**, which is the `mid.drv` hash.

The patched overlays and EXE are DOS glue: key-poll hooks, scene selectors and a Ctrl-E exit.

**Context mapping** (the driver's switch, then the scripted selectors):

| Context | Song |
|---|---|
| Britannia / Underworld | Britannic Lands / Worlds Below |
| Aboard a frigate (tiles 0x20–0x27, wins over the location) | Cap'n Johne's Hornpipe |
| Cities of Virtue 0x01–0x08 | Villager Tarantella |
| Lighthouses 0x09–0x0c, keeps 0x19–0x1d | Dream of Lady Nan |
| Huts 0x0d–0x10, villages 0x13–0x18 | Greyson's Tale |
| Lord British's castle 0x11 | The Missing Monarch |
| Blackthorn's palace 0x12 | Lord Blackthorn |
| Lycaeum / Empath Abbey / Serpent's Hold 0x1e–0x20 | Fanfare for the Virtuous |
| Dungeons 0x21–0x28 | Halls of Doom |
| Combat | Engagement and Melee, then the Ultima V Theme after `VICTORY!` |
| Title menu | Theme |
| Character creation | Amiga Theme |
| Shrines, Camp / hole-up | Stones (frozen) |
| Intro pages | Stones / Halls / Greyson by page range |
| Endgame | Stones / Lady Nan by scene, then Joyous Reunion, then Rule Britannia |
| Blackthorn capture, death / Refuge | **silence** |
| Demo, anything else | stop |

**Looping and transitions.**
- No song carries AIL loop controllers (CC 116/117; checked for all 16).
- The driver restarts the *same* song when it has finished, on each key poll (`0x267` → `0x27c`, status `int 66h` AX=0x70c).
- A different song replaces the current one at once. There is no crossfade in DOS.
- The same song is never restarted while it plays (`cmp al,[0x11e]`); several contexts share songs.
- Reunion → Rule Britannia is a chain: `sel 0x15` pre-sets the current song so that `sel 0x1b` starts Rule Britannia only after Reunion ends.

**Priorities:**
1. frozen / scripted scene
2. combat sentinel
3. frigate
4. location

The patch's own known issue: on the title demo the Theme does not loop, because the demo polls keys inside the EGA driver.

**Detection outcomes.** The following are PATCH DETECTED (supported), STOCK ASSETS, and UNKNOWN / UNSUPPORTED VARIANT, and the rules in §6 decide between them.

## 6. Capability detection (Phase E)

The detector is `native/tools/u5pack/audio-capability.ts`, run by the packer.

**Evidence** (never file names alone):
1. **`mid.drv`'s SHA-256** must equal Exodus 1.0's. This pins the selection logic the port implements. Any other driver is an unknown variant, whatever its name. One changed selector byte makes it unknown (test R4).
2. **The songs** are the files named by `mid.drv`'s *own* table (pointers at 0x20). Each must be a structurally valid single-sequence XMI that the reference converter also accepts.
3. **`FAT.OPL`** must parse as a Miles timbre library, using the reference `parseMilesOplBank`.
4. **Informational only:** `DATA.OVL`'s slot (`T1K.DRV` / `MID.DRV` / other). It proves only that `u5data.exe` once ran. It is recorded, and it decides nothing.

Files are found case-insensitively in the game directory, then under `upgrade/` (the extractor's rule).

| Evidence | Result |
|---|---|
| `mid.drv` present, hash ≠ Exodus 1.0 | `unknown-music-variant` |
| no `mid.drv`, but any Exodus song file or `FAT.OPL` | `incomplete-music-patch` |
| no `mid.drv`, no Exodus song or bank, but foreign `*.XMI` (MIDPAK's `SETM.XMI` excepted) | `unknown-music-variant` |
| nothing of the above | `stock-no-music` |
| Exodus `mid.drv` + 16 valid songs + valid bank | `supported-music-patch` |
| Exodus `mid.drv`, anything missing or invalid | `incomplete-music-patch` |

Unknown is never treated as stock. An incomplete or unknown install gets **no** music data in its pack.

## 7. Resource-pack architecture (Phase F)

**Decision: a separate, optional SD audio pack, `/ultima5/openu5-audio.bin` (OU5AUDIO 1.0), built locally by `npm run pack:audio`. The game pack is not changed.**

The deciding measurement is `native/core/a3-01-stock-vs-patched-extract.log`:
- a stock-shaped copy of the install (the patch's files removed and the 3 `DATA.OVL` bytes restored) and the patched install give **byte-identical inputs for every entry of the game pack**;
- only `music/*` and the extractor's `manifest.json` differ, and neither is packed;
- the patched re-extraction also equals the files the Alpha 2 pack was built from.

So the Alpha 2 game pack is independent of the asset flavour today. Recording music capability *inside* it would make the two valid installs build different packs, and the exact size + CRC lock would refuse one of them.

| Option | Verdict |
|---|---|
| A. Inside `openu5-alpha1-resources.bin` | Rejected. It breaks the stock/patched identity property above, forces every user to rebuild and recopy the 2 MB pack, and would need the identity lock redesigned. |
| B. Loose audio files on SD | Rejected. The firmware would scan and validate arbitrary files at boot, and there is no single identity. |
| C. A manifest pointing at copied sources | Rejected. Two artifacts to keep in step, with the same scanning problem. |
| **D. Separate derived audio pack (chosen)** | Optional, small (stock 112 B; patched 56,148 B), CRC-sealed, and one file read at boot. The game pack's identity is untouched: **the byte-identical Alpha 2 pack still boots**. |

How option D does on each criterion:
- **Size:** 56 KB beside a 2 MB pack.
- **Streaming and seeks:** none needed. XMI is event data, so a song is loaded whole (≤ 7.3 KB).
- **RAM:** A3-01 reads the pack into a temporary buffer, validates every CRC, and frees it. A3-04 keeps the ~55 KB of payload in PSRAM.
- **Stale safety:** the major version is pinned. Stale, corrupt or inconsistent packs are refused, and refusal means *no music*, never a boot block.
- **User update:** one command and one extra file. It is not needed for stock installs, which simply get "no audio pack".

**Licensing and source-asset policy.**
- The repository contains **no game or music bytes**. It holds only:
  - parsers;
  - the format;
  - the `mid.drv` hash;
  - a 112-byte stock fixture: header plus flags, byte-identical to what a real stock install produces.
- Packs are derived on the user's machine from the user's own files, into the git-ignored `native/assets/`, and they are never committed.
- The patch's XMI and `FAT.OPL` are copied **verbatim**, and only when the install really has the complete supported patch.

## 8. OU5AUDIO 1.0 format

The full layout is in `native/core/include/openu5/audio_pack.h`. In summary:
- **Header:** a 32-byte header like OU5A1RES's, with magic `OU5AUDIO`, the version, sizes, and CRCs of the payload and of the table.
- **Table:** 48-byte entries: name, kind, id, offset, length, CRC.
- **Entries:**
  - `capability` (kind 0), always present, 32 bytes;
  - with the supported patch only: `song-00` … `song-0f` (kind 1, id = the driver's song id) and `timbre-bank-FAT.OPL` (kind 2).

**The firmware check** (`inspect_audio_pack`, pure):
- magic, major version, geometry, exact size, table CRC, payload CRC and every entry CRC;
- a Supported claim stands **only** if all 16 songs pass `validate_xmi` and the bank passes `validate_timbre_bank`;
- a stock / incomplete / unknown record carrying music data is Inconsistent.

## 9. Audio service API (Phase G)

This is `openu5/audio.h`: `AudioService` over `AudioBackend`.

| Call | Policy |
|---|---|
| `play_sfx(SfxId, param)` | Submitted immediately, in emission order, carrying the SFX gain. Volume 0 or no backend: dropped (counted as muted). A backend refusal is counted and **never retried**. |
| `stop_sfx()` | Forwarded. |
| `play_music(MusicContext)` | Context → song (`song_for_context`, the patch's table). Nothing reaches the backend unless music is Available. The same song is never restarted. `Silence` stops it. The context is remembered even while music is unavailable. |
| `stop_music()` | Selector 0x03: stop, and the context becomes Silence. |
| `set_sfx_volume` / `set_music_volume` | 0–100. Only the changed channel's gain is sent. Music volume 0 stops the song but keeps the context; raising the volume starts it again. |
| `has_music()`, `music_availability()`, `current_music_context()`, `current_song()`, `stats()` | Read only. |
| `flush_for_load()` | Stops SFX; music is untouched. |

**Channel policy.**
- **SFX:** one channel. Requests keep their emission order. The original speaker was monophonic and every primitive blocked, so it never overlapped two effects. A3-02's device mixer must play SFX **sequentially** (FIFO, bounded; overflow drops the oldest pending effect, never the one playing).
- **Music:** one song at a time. There is no crossfade: the DOS driver switches instantly. A short fade is a possible A3-05 polish, not part of the contract.
- **No ducking, and SFX never interrupt music.** In the patched original the MIDI card and the PC speaker were separate hardware, and both sounded at once.

**Scenes, load, menus, Developer, Ending.**
- **Scene transitions:** the context is re-derived from the game (A3-04), and the same song continues.
- **Save / load:** a load flushes SFX (`synchronize_loaded_world`). A3-04 re-derives music from the loaded world.
- **System Menu / Developer overlay:** no game cues arrive, music keeps playing, and volume changes apply live.
- **Ending:** it follows the endgame table, then Reunion, then Rule Britannia, in the ending presenter (A3-04 with D-54).
- **Blackthorn capture and death / Refuge:** silence.

**Backend contract:** every call returns without waiting for hardware, and the backend never calls the service or the game.

## 10. Non-blocking timing contract (Phase H)

| Claim | Proof |
|---|---|
| Playback cannot block the game loop | `audio.cpp` contains no loop-until, delay, sleep, wait or RTOS primitive (source check S20). The device backend's game-thread entry is `xQueueSend(…, 0)` (S20). I2S writes run only on the core-1 audio task. |
| Missing hardware or assets cannot stall gameplay | No backend: silent (S20). A refusing backend is asked once per request (S20, G20). A missing / stale / corrupt pack gives `NoAudioPack` / `AudioPackInvalid` (P4, L1). The audio pack is read after the game-pack identity gate and is never part of it (I4). |
| SFX cannot change the RNG order | G18: position, clock, food, gold, HP, **RNG state**, command count and transcript are identical silent / recording / refusing, **and** identical to a run whose Sfx events never reach the runtime. Mutation M18 (an RNG draw in the audio path) is killed by that check. |
| Music cannot change input timing | Music has no input path. The service holds no clock, and T19 shows identical input and scene timelines. |
| Stopping or restarting audio on load does not mutate state | H1: after save → walk → Alt+L, the loaded world is identical with or without audio. |
| Audio failures degrade to silence | S20 and G20 (refusing), P4 (bad packs), L1 (foreign file). |
| Scene timing stays scene timing | T19: the paced Camp apparition's 5 ms timeline and every frame timestamp are **identical** with no audio, a recording backend, a refusing backend, and SFX muted. T20: the cues reach the backend when the pacer releases them (5 / 395 / 1670 / 1865 ms), never inside the command. |
| The 1988 sound flag is not the volume | `SFX Volume` is an output gain only. At 0 % the Batch 51 sound-flag branches do not change: sound-ON drags, sound-OFF lute. |

## 11. Settings (Phases I and J)

**Rows**, in the System Menu (Alt+M → Settings) and in the title Settings. Developer stays last, and on the title screen it exists only in developer builds.
1. Brightness
2. Movement default
3. Trackball
4. Text / UI
5. **SFX Volume**
6. **Music Volume**
7. Developer

| | SFX Volume | Music Volume |
|---|---|---|
| Label / value | `SFX Volume: 80%` | `Music Volume: 80%`, or `Music Volume: Unavailable` |
| Keys | Trackball Left −10, Right +10, Enter +10 (the Brightness convention) | the same, **only while music is available** |
| Range | 0–100 %, step 10, clamped with no wrap; 0 = mute | the same |
| Default | 80 % | 80 % |
| Applied | live, to the SFX channel only | live, to the music channel only |
| Persisted | on leaving Settings and on closing the menu, in `/sd/ultima5/settings.json` `soundVolume` (the existing key) | `musicVolume` (the existing key) |
| Without music | fully usable (it drives the test tone now, and the effects from A3-02) | the row says Unavailable, adjust keys do nothing, and the footer gives the reason |

**Missing-music UX.** The row stays visible and reads `Unavailable`. When it is selected, the footer names the reason (§0). This fits the existing Settings conventions: rows are plain text, there is no grey, and the footer already carries hints. It also gives better feedback than a hidden row, and it is not a dead control.

**Persistence.**
- Both values are device settings. They are never in a save; save/load, New Journey and Return to Title keep them.
- They are stored even while music is unavailable. Installing a supported pack later finds the old preference live (S7). Removing it makes the row unavailable again without losing the value.
- **Migration:** none needed. Every Alpha 2 `settings.json` already has both keys (80/80), and the document version stays 1 (F10).
- **Corrupt or missing file:** the document is refused whole and the defaults stand (F9, S9), as before.
- Off-step values from a hand edit are kept and step deterministically (85 → 95 → 100).

## 12. Tests and evidence (Phases K and L)

**New ctest targets (3):**

| Target | Checks | Covers |
|---|---:|---|
| `a3_01_audio_contract` | 57 | Vocabulary V1–V4:<br>• drift against `sfx.ts` and `music.ts`;<br>• a scan proving every emitted cue maps.<br>Selector M1–M3, exhaustively over `g_location`.<br>Service S11–S20.<br>Pack reader P1–P5:<br>• the committed stock fixture;<br>• the real pack;<br>• crafted corrupt, stale and inconsistent packs.<br>Settings rows and codec F5–F10. |
| `a3_01_audio_runtime` | 37 | The real `AlphaRuntime` with raw keys and a recording backend:<br>• loader L1;<br>• routing and state invariance G;<br>• Camp timing invariance T;<br>• Settings rows and `settings.json` reboot S;<br>• Developer tone D;<br>• load flush H;<br>• identity gate I. |
| `a3_01_audio_capability` (node) | 26 | Detector and writer:<br>• synthetic installs C1–C14 (no game bytes);<br>• read-only views of the real install R1–R5. |

- **Matrix mapping (the brief's 1–20):**

  | Brief item | Checks |
  |---|---|
  | 1 | C1–C3, R2 |
  | 2 | C8, R1, P2 |
  | 3 | C5, C6, C9–C11, R3, R5 |
  | 4 | P4, I4, `batch53_release_blockers` P4–P6 unchanged |
  | 5 | F5, S5 |
  | 6 | F6, S6 |
  | 7 | F7, S7 |
  | 8 | F8, S8 |
  | 9 | F9, S9 |
  | 10 | F10, S10 |
  | 11 | S11, G11 |
  | 12 | S12 |
  | 13 | S13 |
  | 14 | S14 |
  | 15 | S15 |
  | 16 | S16, T16 |
  | 17 | S17 |
  | 18 | S18, G18, T18, H1 |
  | 19 | T19 |
  | 20 | S20, T20, G20 |

- **Host backends:**
  - `NullAudioBackend` (core) is silent.
  - The tests' `Recorder` backends log every SFX id, parameter and gain, every music song and gain, start / stop order, and channel gains with virtual-clock timestamps.
  - Capability state is visible through `AudioService::music_availability()` / `AlphaRuntime::audio_pack()`.
- **Mutation validation:** `native/core/tools/a3_01_mutation_check.py` → `native/core/a3-01-mutation.log`. 21 one-line mutations of production code; each must turn at least one A3-01 check RED. Two survivors of the first pass (M8 payload CRC, M18 RNG draw) exposed two test gaps. Both were closed (P3 appended bytes; the G18 no-cue oracle), and the second pass kills all 21.
- **Existing guards changed** (the row sets grew; nothing else changed):
  - `frontend_test` System Menu Settings `line_count` 5 → 7;
  - `ui_debug_menu_test` Diagnostics rows 16 → 17.

## 13. Hardware proof of concept (Phase M) and device check

**Developer → Diagnostics → `Audio test tone (SFX)`** (the last row) plays one short two-note square-wave chime: 880 Hz for 120 ms, then 1320 Hz for 180 ms, with 3 ms edges. It goes through the SFX channel, so it follows `SFX Volume`, and at 0 % it is not sent at all. A System line reports the result.
- The I2S channel and the task exist only after the first tone.
- A bring-up failure is logged (`AUDIO_BACKEND failed <step>: <err>`) and leaves the game silent.

**Hardware result (reported by the user before A3-02, 2026-09-26): PASS.** The A3-01 image booted; the test tone plays through the speaker; SFX Volume changes its loudness and 0 % mutes; both volumes survive a reboot; Music Volume is adjustable with the supported music-patched asset set; gameplay stayed stable with the backend up. The levels will want tuning: that is A3-05, not a defect. A3-01 device check, as it was run:
1. Boot with **no** `openu5-audio.bin`:
   - the log shows `AUDIO_PACK … state=missing`;
   - Settings shows `SFX Volume: 80%`, and `Music Volume: Unavailable` with footer *No audio pack: npm run pack:audio*.
2. Copy the **stock** pack, `npm run pack:audio -- --source <stock dir> --output <file>`:
   - Music reads Unavailable, *Stock DOS game files have no music*.
3. Copy the **patched** pack (`npm run pack:audio`, 56,148 B):
   - `Music Volume: 80%` is adjustable. It is inaudible until A3-04.
4. Developer → Diagnostics → Audio test tone:
   - a short chime from the speaker;
   - the System line reads `Audio test: tone sent, SFX 80%`.
5. Set SFX Volume to 30 % and repeat: a quieter tone. At 0 %: no tone, and the line says *silent*.
6. Reboot: both volumes are kept.
7. Save / load / Return to Title: both volumes are kept, and gameplay is unchanged.
8. The game pack identity screen is unchanged: `RES … 2041466B CRC 26f75ae6`.

## 14. Next audio batches (Phase O)

| Batch | Scope | Why this split |
|---|---|---|
| **A3-02 — speaker synthesizer + first gameplay SFX** | Port the six PC-speaker primitives (`tone_sweep`, `noise_burst` with its local PRNG, `beep`, `set_tone` / `stop`, `glide`) into a device mixer at 16 kHz on the core-1 task, with the 25,806 samples/s calibration and the sequential FIFO SFX policy. Render the families whose native emission already exists and is not paced: move-step / move-blocked, dungeon-*, cannon, moongate, quake, sceptre, ring, torch, instrument-note (closes H-125), shrine cues. Hardware-validate the speaker and volume. | The primitives are small and fully derived. Everything after this depends on them. |
| **A3-03 — broad SFX hookup** | Emit what the core does not emit yet: arena combat hits / damage / defeat / escape / victory fanfare; the spell-ceremony pairing (MagicCeremony index → CAST2 0x0000 tables); healer jingle; ambient proximity (fountain / waterfall / clock); Blackthorn beat cues; intro and title cues; bard song. Audit each against `sfx-catalog.md` and give each its guard. | This is mostly core emission and adjudication work, not audio code. |
| **A3-04 — music playback** | Port the reference's XMI sequencer, voice allocator, bank reader and OPL2 emulator (`game/src/ui/opl/`) to the core-1 task. Keep the audio pack's songs and bank in PSRAM. Wire `music_context_for_location` and the scripted selectors (title, creation, shrine / camp freeze, combat / victory, Blackthorn / death silence, intro / endgame tables, the Reunion → Rule Britannia chain). Measure CPU. | This is the largest piece. It needs A3-02's mixer, and a CPU budget measured on hardware. |
| **A3-05 — audio polish and hardware validation** | Mixer headroom and clipping, optional music fade, SFX / music balance, a battery and CPU soak, Settings hardware sign-off, the preservation-ledger rows (D-3 closure), and a possible strict "1988 sound-off" profile decision (the sound-flag branches). | Decisions that need the other three on hardware first. |

## 15. A3-02 — the PC-speaker synthesizer, the first gameplay sounds and the harpsichord

### 15.1 Baseline (Phase A)

| Item | Value |
|---|---|
| Tree | HEAD `2f218808` = tag `alpha3-a3-01-audio-architecture`, clean |
| A3-01 firmware | `3.0.0-alpha3-dev-a3-01-debug`, `0xdde10` = 908,816 B, embedded `Git 2f21880857ca`, Launcher SHA-256 `4909aa6f…c17c` |
| Host suite | fresh build `native/core/build-a3-02-baseline`, **serial ctest 126 / 126 in 115.79 s**; the only build warning is the known w64devkit `stl_uninitialized.h` false positive (`native/core/a3-02-baseline-*.log`) |
| Game / resource packs | unchanged since Alpha 2: `openu5-alpha1-resources.bin` 2,041,466 B, CRC `0x26f75ae6` |
| Audio pack | optional `openu5-audio.bin`, OU5AUDIO 1.0, music capability only (patched 56,148 B) |
| Service / backend | `AudioService` (A3-01) → `TdeckAudioBackend`, which rendered only `DiagnosticTone` and declined every gameplay cue |
| Settings | SFX Volume / Music Volume rows, 0–100 % step 10, default 80, `settings.json` `soundVolume` / `musicVolume` |
| Cue catalogue | 57 `SfxId`s; the core emits 31 distinct cue ids (A3-01 V4) |
| A3-01 hardware | **PASS** (§13): tone audible, SFX Volume scales, 0 % mutes, persistence, Music row, stability. Calibration not final → A3-05 |

### 15.2 The primitives, re-read from the bytes (Phase B)

Every primitive body was disassembled again for this batch (`re/tools/dis16.py --exe`), and every call site the A3-02 cues use was re-read with its pushes (`re/tools/a3_02_cue_sites.py` → `native/core/a3-02-cue-sites.log`, 22 sites, each landing on the claimed primitive). C argument order is a0 = the LAST push; the reference writes push order.

| Primitive | Entry (C args) | What the loop does | Pitch | Length | Mute branch |
|---|---|---|---|---|---|
| **tone sweep** | `tone_sweep` 0x2192 (a0 bx step, a1 bx start, a2 count, a3 delay, a4 inc) | PIT ch2 = 0x3c (a 19.9 kHz carrier the cone averages); `dx = 0, bx = start`; count times: `dx += inc; gate = dx > bx` (`cmp dx,bx; ja` 0x21f8, **unsigned**); `bx += step`; spin a3 × C/24 | inc / 65536 × 25,806 Hz, constant | count × a3 sweep samples | 0x21c4: the same loop, gate closed — it blocks just as long |
| **noise burst** | `noise_burst` 0x223c (a0 band, a1 dur, a2 step) | gate on; **do** { `s = PRNG(s)`; PIT = 0x1234DE / (100 + s mod (band − 99)); `acc += step`; spin step × C>>4 } **while** `acc < dur` (signed) | each draw in [100, band], **closed** | ceil(dur / step) iterations (≥ 1) × 1.5 samples × step | same loop, silent |
| **fixed tone** | `beep` 0x22c0 (a0 dur, a1 freq) = `set_tone` 0x22e2 + `delay` 0x20c8(dur, 1) + `stop` 0x230e | one PIT square | 1193182 / floor(1193182 / freq) | dur × 24 samples | the delay is kept |
| **glide** | `glide` 0x43ae (a0 total, a1 step, a2 end, a3 start) | `inc = trunc16((end − start) × step) / total`; `si = start, di = 0`; **while** `di < total` (signed `jl`) { set_tone(si); delay(step, 1); si += inc; di += step } | a staircase; the nominal end is **never written**: cannon 1000→«200» ends at 233 Hz | ceil(total / step) × step × 24 samples | delays kept |
| **stop / silence** | `stop` 0x230e; `delay` 0x20c8 (a0 count, a1 shift) | gate bits 0/1 cleared; count × C >> table[shift]; shift 1 → table `[0x5427]` = 0 | — | count × 24 samples | — |
| **repeated beep** | two or more `beep` calls | e.g. `combat-reject` (SJOG 0x1f52/0x1f5d), not wired in A3-02 | — | — | — |
| **multi-tone sequence** | a loop of `tone_sweep` calls | the arpeggio (6), the ceremony (NB + 2), the mirror (18 NB), later the ordained melody / healer jingle | per call | sum | — |
| **scene chime / arpeggio** | the Camp apparition's four sites (OUTSUBS 0x067b / 0x0698 / 0x0896 / 0x08c1), Blackthorn 0x083f | tone sweeps | per call | per call | — |

- **The noise PRNG** is the word `[0x545c]`: `((s + 0x9248) ror 3 ^ 0x9248) + 0x11`. It is shared across calls and never touches `g_rng`. Its initial value is read from the DS image: DATA.OVL fileoff 0x546c = **`0x7664`**.
- **Calibration:** one sweep sample = 1 / 25,806 s (`speaker.ts` `DELAY_UNIT_MS` = 0.93 ms from DOSBox-X captures; `scene_timing.h` `kToneSweepSamplesPerSecond`). The synthesizer uses the pacers' **integer** 25,806 (the reference's 24000/0.93 = 25,806.45 differs by 17 ppm) and a `static_assert` ties the two constants together. Durations are held in half samples so that noise_burst's 1.5-sample unit stays exact; frames = floor(half samples × 16000 / 51,612) per segment.
- **Which duration the sound follows:** its own loop length (the original's). The paced scenes hold for the same number of samples because both are derived from one constant — never because one waits for the other (§15.7). Scene timing itself was recovered separately in Batch 51 and is unchanged.
- **Aliasing at 16 kHz:** A3-02 sweep fundamentals are 1.0–3.5 kHz (the highest, the ceremony at idx 0, 3,469 Hz); PIT tones 165 Hz–2.5 kHz; noise draws reach 20 kHz (`dungeon-zap`) — above Nyquist. Every primitive is rendered at 4× (64 kHz) and box-filtered to 16 kHz, and the noise path then passes the cone low-pass (2.1 kHz). What still folds is the 1-bit gate's own upper harmonics, which is part of the PC-speaker character; the real cone rolled off above ~5 kHz anyway. No clipping: see §15.3.

**Where A3-02 departs from the TypeScript reference** — each time toward the bytes, none of it in a layer a fixture pins (§15.11 R1):

| Item | Reference (`speaker.ts`) | A3-02 | Why |
|---|---|---|---|
| tone_sweep timbre | a 50 % square; start/step "→AV" | **the duty is modelled**: gate = dx > bx per iteration, bx swept by step | the loop body (0x21f8); the mirrored start/step pairs of the ceremony and the healer jingle are duty mirrors |
| PIT quantisation | noise only (`pitHz`) | every PIT tone: beeps, glides, noise | 0x22f2 divides the same way for set_tone |
| noise iterations | floor(dur / step), capped at 512 | ceil(dur / step), no cap | the do-while at 0x22aa; equal for every A3-02 cue (all exact multiples) |
| PRNG seed | 0x1234, arbitrary | 0x7664 | the DS image |
| calibration | 25,806.45 | 25,806 | the pacers' constant (17 ppm) |

### 15.3 The synthesizer (Phase C)

**Layers** (all pure, in the portable core: `native/core/include/openu5/sfx_synth.h`, `src/sfx_synth.cpp`; no heap, no RTOS, no clock, no GameState, no `g_rng`):
1. `compile_sfx(SfxId, param, SpeakerProgram&)` — the cue table (§15.5). A program is ≤ 24 segments; a segment is ONE primitive call holding the binary's own arguments.
2. `SpeakerVoice` — renders a program by **emulating each primitive's loop**:
   - PIT tones (beep, glide steps): a 50 % square from a 32-bit phase accumulator at the fine rate, bipolar (DC-free);
   - tone_sweep: the literal 1-bit PWM, one gate decision per original iteration, minus the expected duty so the output stays centred however far the duty sweeps;
   - noise: a unipolar gate (the cone is only pushed), a 20 Hz DC block and the 2.1 kHz cone low-pass, as the reference measured — one pole each where the reference uses two-pole biquads (the noise timbre is class C there too);
   - 4× oversampling (64 kHz) and a box filter to 16 kHz; integer arithmetic only, so host and device render the same samples.
3. `SfxPlayer` — one monophonic voice plus a pending FIFO, the policy of §15.4, the persistent PRNG word, and the transport epoch.

| Decision | Choice | Reason |
|---|---|---|
| Sample rate | 16 kHz, 16-bit mono | A3-01's hardware-validated I2S setup; ample for fundamentals ≤ 3.5 kHz |
| Waveform | square / 1-bit PWM, not band-limited sines | the speaker was a gate; do not modernise the timbre |
| Amplitude | a 50 % square peaks at 8,192 (−12 dBFS), the A3-01 tone's level; an extreme duty can reach 2× = 16,384 | 6 dB of headroom; nothing clips at unity (Y6) |
| Envelope | linear 4 ms ramps at every tone / sweep / glide segment edge, 0.25 ms for noise (the reference's EDGE_S / NOISE_EDGE_S); a 2 ms release on a cancel or a preemption | no click, and noise keeps its dry attack |
| Sweep interpolation | none: one gate decision per original iteration | the original has no interpolation either |
| Gain | applied once, at render time, from the live SFX channel gain, saturating | a Settings change is heard within one 16 ms chunk (Y8) |

**Device** (`native/targets/tdeck/main/tdeck_audio.{h,cpp}`):
- The **game thread** only: checks `sfx_supported(id)` (a cue without an A3-02 program is declined there and never costs a queue slot), posts `{request, epoch}` to a 16-entry FreeRTOS queue with a **0 timeout** (full = declined and counted), bumps an atomic flush epoch on `stop_sfx()`, or stores a gain. It never waits.
- The **audio task** (core 1, 4 KiB stack, priority 3, created on the first accepted cue) owns the `SfxPlayer`. It blocks on the queue while silent; otherwise it syncs the epoch, drains the queue into the player, renders 256-frame (16 ms) chunks at the live gain and blocks only in `i2s_channel_write` (200 ms timeout). When the player goes idle it writes two silent chunks and disables the channel. It never spins, so it cannot starve core 1's idle-task watchdog.
- **Memory:** the backend object is 784 B (static; it holds the player, one program and the FIFO), the task stack grew 3 → 4 KiB, the queue is 16 × 16 B. The synthesizer's code is about 4.5 KB.

### 15.4 Channel and priority policy (Phase D)

One voice, like the one speaker gate. The original never overlapped two effects because every primitive blocked; the device keeps that order without blocking anything.

| Class | Cues | Rule |
|---|---|---|
| **Diagnostic** | the Developer test tone | preempts everything and flushes the FIFO |
| **Scene** | apparition ×4, Blackthorn materialize (and later: Refuge, intro, title, bard, endgame) | the **latest wins**: it cuts whatever plays at its class or below (2 ms fade) and drops the lower-class cues still queued — the pacer is the clock, so audio follows it |
| **Spell** | the ceremony | FIFO |
| **Combat** | hit / heavy / defeat / damage | FIFO |
| **Instrument** | harpsichord notes | FIFO, **never coalesced**: a repeated key is a repeated note |
| **Ordinary** | world feedback (footstep, wall bump, glides, dungeon noise) | FIFO |

- **FIFO:** 8 pending. An **overflow drops the oldest pending** request, never the one playing (the A3-01 contract). Eight was chosen by a test: five instant harpsichord keys plus the step after them overflowed a 4-deep queue and lost a note, which the original (15-key BIOS type-ahead) never did. At most ~1.2 s of notes can lag.
- **Repeats:** a request identical to the newest pending one (same id and parameter) is coalesced — holding a direction into a wall queues at most one more bump.
- **Twenty requests at once** (Y10): distinct ones — the first plays, the newest 8 wait, the 11 oldest are dropped; identical ones — one plays, one waits, 18 coalesce. None of it blocks the producer.
- **Unknown or unaudited ids** are declined at the backend and counted as refused by the service (never retried).

### 15.5 The first gameplay sounds (Phase E)

**22 gameplay cues + the diagnostic tone** render in A3-02. Every one is emitted by the core already, or — for the four combat cues and the ceremony — derived at presentation from an event the core already emits, exactly as the reference's `sfxForCombatEvent` / ceremony do. No gameplay event was added.

| Brief item | Cue (`SfxId`) | Site and pushes | Program | Length |
|---|---|---|---|---|
| 1 error beep | `move-blocked` | MAINOUT 0x0344 / TOWN 0x0849: `beep(0xa5, 0xc8)` | 165 Hz PIT tone | 186 ms |
| 2 movement | `move-step` | kernel 0x433e: NB(1, 0x19, 0x3e8); delay(0x14, 1); NB(1, 0x19, 0x5dc) | two 1.5 ms clicks, 18.6 ms apart | 21 ms |
| — world | `torch-borrowed`, `dungeon-fail` | SJOG 0x1a21, DUNGEON 0x1cfb: glide(0x320→0x7d0, 1, 0x32) | 800→1976 Hz, 50 steps | 46.5 ms |
| — world | `ring-vanishes` | ZSTATS 0x0e42: glide(0x4b0→0x7d0, 1, 0x28) | 1200→1980 Hz | 37 ms |
| — world | `cannon-fire` | CMDS 0x09d5: glide(0x3e8→0xc8, 5, 0x12c) | 1000→233 Hz, 60 steps | 279 ms |
| — world | `waterfall-fall` | OUTSUBS 0x0492: glide(0x9c4→0x320, 1, 0x12c) | 2500→1005 Hz | 279 ms |
| — world | `dungeon-trap` | kernel 0x2fe3: NB(0x28, 0xbb8, 0x1f4) | 75 draws ≤ 500 Hz | 174 ms |
| — world | `dungeon-zap` | DUNGEON 0x04b9: NB(1, 0x1f4, 0x4e20) | 500 draws ≤ 20 kHz | 29 ms |
| — world | `field-afflict` | DUNGEON 0x099e: NB(1, 0x32, 0xdac) | 50 draws ≤ 3.5 kHz | 3 ms |
| — world | `mirror-break` | TOWN 0x0a69–0x0a80: si = 0x7d0 … <0x4e20 step 0x3e8, NB(0x28, 0x78, si) | 18 bursts, band 2000 → 19000 | 125 ms |
| 3 combat | `combat-hit` | kernel 0x35de: NB(0xa, 0xbb8, 0x7d0) — a hit on an **enemy** | 300 draws ≤ 2 kHz | 174 ms |
| 3 combat | `combat-hit-heavy` | kernel 0x35c9: NB(0x28, 0xbb8, 0x1f4) — a hit on a **party member** | 75 draws ≤ 500 Hz | 174 ms |
| 3 combat | `combat-defeat` | kernel 0x2fe3 (the 0x2fd0 burst) — a death | 75 draws ≤ 500 Hz | 174 ms |
| 3 combat | `combat-damage` | kernel 0x2a68: NB(0xa, 0x640, 0x7d0) — dungeon trap damage | 160 draws ≤ 2 kHz | 93 ms |
| 4 spell | `time-spell` (the ceremony) | CAST2 0x0000(idx < 9): NB(0x320, 0x1f40 + 0x640·i, 0x2bc) then tone_sweep(inc[i], 1, 0x2710 + 0xfa0·i, up[i], +step[i]) and the mirror (down[i], −step[i]); tables DATA.OVL 0x4af6 / 0x4b08 / 0x4b1a / 0x4b2c | a low burble, then one pitch whose timbre sweeps out and back | 1.24 s (i = 0) … 4.46 s (i = 8) |
| 5 Camp | `apparition-materialize` | OUTSUBS 0x067b: TS(0xa3c, 1, 0x2710, 0x9c4, 6) | 1032 Hz, duty 96 % → 5 % | 387.5 ms |
| 5 Camp | `apparition-arpeggio` | OUTSUBS 0x0683–0x06a2: TS([0x3a26 + 2k], 1, 0x1388, 0xc8, 0xd) × 6 | 1032 ×3, 1457, 1536, 1638 Hz | 1162.5 ms |
| 5 Camp | `apparition-heal-chime` | OUTSUBS 0x0896: TS(0x157c, 1, 0x1388, 0xc8, 0xd) | 2166 Hz | 194 ms |
| 5 Camp | `apparition-chord` | OUTSUBS 0x08c1: TS(0x157c, 1, 0xea60, 0x9c4, 1) | 2166 Hz, duty 96 % → 5 % | 2325 ms |
| 6 Blackthorn | `blackthorn-materialize` | BLCKTHRN 0x083f: TS(0xaf0, 1, 0x32c8, 0x64, 5) | 1103 Hz | 504 ms |
| — harpsichord | `instrument-note` | TOWN 0x0e6d: TS(note[digit], 1, 0xfa0, 0x4e20, 0xfffc) | §15.6 | 155 ms |
| — device | `diagnostic-tone` | class D (the A3-01 chime) | 880 + 1320 Hz | 300 ms |

- **Item 7, a UI / selection cue: none.** The 1988 game makes no sound in its menus, and none was invented.
- **How they reach the synthesizer:**
  - `present_audio()` (A3-01) for every `Sfx` cue, at presentation time;
  - **new:** a `MagicCeremony(index)` event plays `time-spell(index)`. The `spell-cast` / `potion-used` / `scroll-used` cues before it are markers with no program; `invalid-magic` has no adjudicated sound;
  - **new:** a `Combat` event `Attacked` with `hit > 0` plays `combat-hit`, or `combat-hit-heavy` when the target is a party member (`member != 255`); `Died` plays `combat-defeat`; a miss is silent;
  - **new:** the Blackthorn pacer's beat cue (`BlackthornSfx`) is handed to a cue sink at the instant the beat is applied — bound in the shared `bind_scene_pacers()`, so the host fixture runs the device's wiring. `ShardSweep` is routed but still declined.
- **Declined until A3-03**, with the reason each needs its own adjudication: `quake` (class C tri-band, and its sync with the shake), `moongate` / `sceptre` / `shadowlord-announce` (long sweeps inside presentations not yet reworked), `shard-sweep` / `victory-fanfare` (the reference pauses the game for them: `BLOCKING_CUES` #206/#212), the shrine cues (inside the shrine pacer), the arena `combat-escape` / `combat-absorbed` / `combat-reject` / `VICTORY!` / arena `Borrowed!` derivations, `refuge-thunder` (class C), ambient, bard song, intro/title, endgame.

### 15.6 The harpsichord (Phase F)

- **Trigger:** in any small map, while the tile immediately SOUTH of the party is 0x8D (the party sits on the chair facing it), a digit key goes to TOWN 0x0e34 instead of the command dispatcher: note = digit, then `tone_sweep(note[digit], 1, 4000, 20000, −4)` @0x0e6d, then the 13-note matcher (`6 7 8 9 8 7 8 7 6 7 6 5 3` opens the passage on LB castle floor 2 — unchanged gameplay). No turn passes. The original uses **only the speaker**, never a music driver.
- **Notes** (DATA.OVL 0x2746, digit-indexed):

  | digit | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 |
  |---|---|---|---|---|---|---|---|---|---|---|
  | inc | 0x0c2c | 0x0da9 | 0x0f56 | 0x103f | 0x123c | 0x1478 | 0x16fa | 0x1857 | 0x1b53 | 0x1eab |
  | Hz | 1227 | 1377 | 1546 | 1638 | 1838 | 2063 | 2316 | 2454 | 2754 | 3092 |

  Digits 1–9 rise; 0 is the highest key (the tenth). Each note is 4000 samples = **155 ms**, its duty widening 69 % → 94 %.
- **Overlap:** never. The note blocks in the original and keys wait in the BIOS buffer, so notes play one after another, each in full. The device queues them (FIFO, never coalesced) and input never waits.
- **Stuck notes:** impossible by construction (every program is finite; the longest wait is ~1.2 s of queued notes). The System Menu does not cut a phrase; a load or Return to Title cuts it in 2 ms.
- SFX Volume applies; 0 % is exact silence. **H-125 is fixed on the host; its hardware confirmation is §15.14 D.**

### 15.7 Scene-timing safety (Phase G)

`a3_02_sfx_runtime` runs the paced **Camp apparition** and the paced **Blackthorn** entry segment through the real `AlphaRuntime`, on the virtual clock, under six audio setups: no audio, the synthesizer, SFX muted (from `settings.json`), a failing backend (declines everything), **stalled** audio (never rendered — every sound "lasts forever") and **racing** audio (rendered 100× ahead — every sound "ends at once").

- **T19 (Camp) and T21 (Blackthorn):** every 5 ms sample (pacer state, XOR/inversion, released steps, frame count), every presented frame's timestamp and the final game state are **identical** under all six.
- The apparition cues reach the synthesizer when the pacer releases them (5 / 395 / 1670 / 1865 ms); the Blackthorn sweep sounds at its beat and the next beat follows **503 ms** later — the pacer's hold, not the sound's.
- Mutations M6 (the Camp pump held while audio refuses) and M7 (the Blackthorn pump shifted by submitted sounds) are both killed by these checks.

### 15.8 Load and mode safety (Phase H)

| Route | Result |
|---|---|
| Alt+L | `flush_for_load` (A3-01): the playing cue fades in 2 ms, the queued ones are dropped, silence after (M1) |
| System Menu Load | the same (M2) |
| Title Continue | Return to Title flushes, and the load flushes again (M3) |
| **Return to Title** | **new:** `audio_.stop_sfx()`. The title replaces the world, like a load; before A3-02 a queued effect could still sound over the title (M4; mutation M9) |
| Developer menu | the test tone preempts the playing cue; the path is healthy and idle after (M5) |
| Camp | paced cues, §15.7 |
| Ending | the cue playing when the game is won ends on its own; cues still play; Return to Title from the Ending flushes (M6) |

- **Stale effects:** a flush is an **epoch**. A request posted before it is dropped by the audio task as stale, even if it was still in the FreeRTOS queue (Y10; mutation M22).
- **No dangling references:** the backend holds only copied requests (id, parameter, gain); it never calls the service, a scene or the game.

### 15.9 Volume (Phase I)

- Gain = volume² × 32767 / 10⁴ (A3-01): 0 % → 0, 10 % → 327, 50 % → 8,191, 80 % → 20,970, 100 % → 32,767.
- Measured on the chord's peak: 0 / 148 / 3,726 / 9,541 / 14,908 — strictly monotonic (Y8).
- 0 % is exact digital zero (Y7), and the service does not even submit (R2).
- 100 % and any gain above it clamp to unity; no integer overflow (Y8).
- A change mid-tone is heard from the next 16 ms chunk, phase unbroken (Y8).
- The gain is applied once (Y8; mutation M13), and only to the SFX channel (A3-01 S14; mutation M14).
- **Hardware loudness curve calibration deferred to A3-05.**

### 15.10 The audio pack (Phase J)

**SFX need no asset.** The effects are synthesized from code and the binary's parameters, so stock and patched users get the same sounds, and a missing, stale or corrupt `openu5-audio.bin` changes nothing about them: `main.cpp` attaches the backend whatever the pack's state. Nothing was added to OU5AUDIO. The optional pack remains music capability only (A3-04).

### 15.11 Tests (Phase K)

**New ctest targets (3):**

| Target | Checks | Covers |
|---|---:|---|
| `a3_02_sfx_synth` | 50 | primitives measured from rendered PCM (Y1–Y10), cue mapping (E11–E14), harpsichord (H15–H18), the TypeScript reference row by row (R1), no blocking primitive (N1) |
| `a3_02_sfx_runtime` | 34 | the real `AlphaRuntime` by raw keys: routing (R), harpsichord (P), combat / ceremony (C), Camp and Blackthorn timing (T), load / mode (M), persisted volume (S25) |
| `a3_02_sfx_reference_drift` (node) | 1 | `native/core/tools/generate-sfx-fixtures.ts --check`: the reference catalogue as 81 segments, `native/core/fixtures/a3-02-sfx-reference.txt` |

| Brief item | Checks |
|---|---|
| 1 fixed tone | Y1 (1000.15 Hz beep, 165 Hz wall bump, a sweep's inc/65536 × 25806) |
| 2 sweep direction and endpoints | Y2 (duty narrows as bx rises, widens as it falls; the pitch holds on both legs) |
| 3 glide | Y3 (staircase, effective end, falling pitch, total ≤ 0 is mute) |
| 4 noise bounds | Y4 (PRNG verbatim, the closed interval reached at both ends, bounded output, persistent state, no game RNG) |
| 5 duration | Y5 (frames from the calibration; scene cues match their holds within 1 ms; the ceremony lead equals the inverted-viewport lead) |
| 6 amplitude clamp | Y6 |
| 7 0 % mute | Y7, R2 |
| 8 max-volume clamp | Y8 |
| 9 cancellation | Y9, H17 |
| 10 queue overflow | Y10 |
| 11 id → primitive | E11, R1 |
| 12 unknown id | E12 |
| 13 repeated cue | E13, H18 |
| 14 scene priority | E14 |
| 15 note mapping | H15 |
| 16 pitch ordering | H16, P4 |
| 17 cancellation | H17, P5 |
| 18 rapid notes | H18, P1–P3 |
| 19 Camp audio on/off | T19 |
| 20 Blackthorn audio on/off | T21 |
| 21 backend failure | T19 / T21 (broken), T16 |
| 22 load clears stale SFX | M1–M3, Y10 (epoch) |
| 23 Return to Title | M3, M4 |
| 24 Ending | M6 |
| 25 persisted volume on real effects | S25 |

- **Existing guards edited** (source shape only; each still guards the same contract):
  - `a3_01_audio_contract` V4 skips `sfx_synth.{h,cpp}` like `audio.{h,cpp}` (its class-name strings sit on `SfxClass` lines);
  - S20 accepts any `xQueueSend(queue_, &x, 0)` (the device now posts a `{request, epoch}` command).
- **RED evidence** is by mutation (§15.12): the synthesizer and its guards are new. The runtime wiring A3-02 added is shown RED by reverting it to A3-01's behaviour: M9 (Return to Title), M23 (Blackthorn cue), M24 (ceremony), M25 (combat side).

### 15.12 Mutations (Phase L)

`native/core/tools/a3_02_mutation_check.py` → `native/core/a3-02-mutation.log`: 26 one-line production mutations (synthesizer 16, runtime 7, service 2, device 1). Each must turn at least one check RED.

- **First pass: 25 killed.**
  - **M20 survived** (harpsichord notes coalesced like ordinary cues): no test pressed the same digit twice in a row. Closed by H18 "the same digit three times fast is three notes".
  - **M22** died only by a compile error (`-Werror` on the unused parameter), a weak kill. The mutation was rewritten to compile.
- **Second pass: 26 / 26 killed**, restored build and all four suites green.

| # | Mutation | Killed by |
|---|---|---|
| M1 | sweep direction reversed | Y2 |
| M2 / M3 | wrong harpsichord / ceremony table | H15, H16, P4, E11, R1 |
| M4 / M5 | 0 % not silent (render / service) | Y7, R2 |
| M6 / M7 | Camp / Blackthorn scene waits on audio | T19 / T21 |
| M8 / M9 | load / Return to Title keeps SFX | M1–M4 |
| M10 | harpsichord note index + 1 | H15, H16, P4 |
| M11 / M12 | overflow refuses the producer / the device producer waits | Y10 / A3-01 S20 |
| M13 | gain applied twice | Y8 |
| M14 | SFX volume touches the music channel | A3-01 S14 |
| M15 | a cancelled tone keeps generating | Y9, H17, M1 |
| M16 | the duty is not modelled | Y2 |
| M17 | noise interval open | Y4 |
| M18 | durations off the calibration | Y5, R1, P3 … |
| M19 | an unaudited cue accepted | E12 |
| M20 | notes coalesced | H18 |
| M21 | a scene cue does not preempt | E14 |
| M22 | the epoch ignored | Y10 |
| M23 / M24 / M25 | Blackthorn cue / ceremony / combat side not routed | T21 / C2 / C1 |
| M26 | the PRNG re-seeded per burst | Y4 |

### 15.13 Firmware (Phase M)

- Pre-commit build `native/targets/tdeck/build-a3-02` (`native/targets/tdeck/a3-02-firmware-build.log`): ESP-IDF 6.1, **zero project warnings** under `-Werror`.
- `0xdf4b0` = **914,608 B**, **+5,792 B** against A3-01's 908,816; **133,968 B (13 %)** of the 1 MiB app partition free.
- Audio memory: backend object 784 B static; task stack +1 KiB (4 KiB); queue 16 × 16 B; synthesizer code about 4.5 KB.
- Two first attempts died inside ESP-IDF's own sources (an assembler rejecting `.tbyte` in `mcpwm_oper.c`, then a GCC internal segfault in `esp_lcd_panel_rgb.c`) — different third-party files each time, no project file involved; the build completed at `ninja -j 4`. The post-commit image is built from a fresh directory; its path, SHA-256 and embedded `Git` are in the annotated tag.
- Not flashed.

### 15.14 Hardware test (Phase N) — A3-02 device check

Copy the Launcher image (tag message). SD card: unchanged; `openu5-audio.bin` optional.

- **A. Boot.** The identity screen reads `FW 3.0.0-alpha3-dev-a3-02-debug` and the `Git` hash of the tag; `RES 2041466B CRC 26f75ae6` as before.
- **B. Basic SFX** (SFX Volume 80 %). Confirm each is heard and that they differ:
  1. Walk on the overworld: a soft double click on every step.
  2. Walk into a wall, a mountain or a table: a low buzz of ~0.2 s.
  3. A fight: a hit on a monster and a hit on the party are two different noise bursts; a kill is a low burst; a miss is silent.
  4. Cast any ceremonial spell (a healing or light spell), or drink a potion: a low burble while the screen is normal, then one whistling pitch whose timbre sweeps while the viewport is inverted.
- **C. Volume.** Repeat B.2 at 100 %, 50 % and 0 %: loudness falls with each step, and 0 % is silent.
- **D. Harpsichord** (Lord British's castle, floor 2: sit on the chair at (17,17) facing the harpsichord south; Developer > Teleport works). Press 1 to 9 and 0: ten rising pitches, 0 the highest. Type a quick run: every note plays in order, the keys never stall, nothing rings on. Open Alt+M mid-run and close it: no stuck tone. Optional: 6 7 8 9 8 7 8 7 6 7 6 5 3 opens the passage as before.
- **E. Camp apparition** (a member with enough experience to level, then (H)ole up and camp): the rising materialize tone, the six-note arpeggio, the heal chime and the long chord. The scene must feel paced exactly as in Alpha 2 — sound neither hurries nor holds it.
- **F. Load.** Start the harpsichord run (or the Camp chord) and press Alt+L straight away: silence at once after "Load complete", and nothing from the old game plays after it.
- Not in this check: music (A3-04), final loudness (A3-05).

**Result (reported by the user before A3-03, 2026-09-26): PASS.** The image boots; footsteps, the wall bump, the arena hits and the spell ceremony are audible; SFX Volume scales them and 0 % mutes; the harpsichord plays in pitch order (H-125 closed); the Camp apparition cues play with no pacing change; Alt+L flushes what plays and what waits. Carried forward: the tone and loudness character (partly the T-Deck's small speaker) to A3-05; **the troll-fight victory chime and the fountain were missing** — A3-03 (§16.4, §16.5).

### 15.15 Next batches (Phase P)

| Batch | Scope | Why this split |
|---|---|---|
| **A3-03 — remaining gameplay SFX / event hookup** | Programs and routing for the declined cues, each adjudicated against its site: moongate, sceptre, shadowlord, shrine donation / ordained / well-done, quake (class C, synchronised with the shake), `shard-sweep` / `victory-fanfare` **with the reference's blocking pauses decided on their own axis** (`BLOCKING_CUES`), the arena message derivations (escape, absorbed, reject, VICTORY!, Borrowed!), ambient proximity (fountain, waterfall, clock), healer jingle, bard song, Refuge thunder, intro / title / endgame. Close D-3's SFX half. | The primitives and the policy exist; what is left is adjudication and routing, cue by cue. |
| **A3-04 — music playback from supported patched assets** | The XMI sequencer, the OPL2 emulation and the `FAT.OPL` bank on the core-1 task, mixed after the SFX voice; `music_context_for_location` and the scripted selectors; CPU and memory measured on hardware. | Needs the audio task and the mixer point A3-02 built, and a measured CPU budget. |
| **A3-05 — volume curve, polish and audio hardware sign-off** | The loudness curve on the real speaker (the user's A3-01 note), SFX / music balance, headroom with both channels, optional music fade, a battery and CPU soak, the strict "1988 sound-off" profile decision, and the ledger rows. | Calibration needs every sound on hardware first. |

## 16. A3-03 — the remaining gameplay SFX, the ambient cues and the scene audio

### 16.1 Baseline (Phase A)

| Item | Value |
|---|---|
| Tree | HEAD `bc65972b` = tag `alpha3-a3-02-sfx-synth`, clean |
| A3-02 firmware | `3.0.0-alpha3-dev-a3-02-debug`, `0xdf4b0` = 914,608 B, 133,968 B (13 %) free, embedded `Git bc65972b8362`, Launcher SHA-256 `2ee83b26…5706` |
| Host suite | fresh build `native/core/build-a3-03-baseline`, **serial ctest 129 / 129 in 111.29 s** (`native/core/a3-03-baseline-*.log`). The first build attempt died inside GCC's own `<compare>` header with a one-bit corruption (`uno2dered`); the header on disk is intact and the resumed build completed — toolchain flakiness, like A3-02's ESP-IDF crashes |
| Synth / service / backend | A3-02's `SfxPlayer` behind `AudioService` → `TdeckAudioBackend` (core-1 task, 16-entry queue) |
| Cue catalogue | 57 `SfxId`s, 23 with a program (22 gameplay + the diagnostic tone); the core emits 31 distinct cue strings |
| **A3-02 hardware** | **PASS** (the user's report): boots; footsteps, the wall bump, combat hits, the spell ceremony and the harpsichord (pitch order correct) are audible; SFX Volume works down to 0 % mute; the Camp apparition cues play with no pacing change; Alt+L flushes. Carried forward: loudness / tone character to A3-05 (partly the small speaker); **the troll-fight victory chime and the fountain are missing** — both closed here (§16.4, §16.5) |

### 16.2 The census (Phase B)

Primary evidence is the binaries, read three ways:
- `re/tools/a3_01_sound_census.py` → `native/core/a3-01-sound-census.log`: every call of the six primitives, **121 sites**.
- `re/tools/a3_03_cue_sites.py` → `native/core/a3-03-cue-sites.log` (new): the pushes of every A3-03 site as whole ranges, because several cues are loops whose arguments only make sense with the loop head and tail (the shard / shrine / Blackthorn ladders, the ORDAINED and Refuge tables, the quake's four passes, the fanfare).
- For each site, the message printed next to it: a site is **"adjacent"** to a message when the `print_ds` and the speaker call sit in one basic block, print first. That test is what turned 16 hints into attributions (the combat, ship, theft, wish and trapdoor rows below); a hint without adjacency stayed `EvidenceUnknown`.

Five calls the linear census cannot see are classified by hand: kernel **0x429c** (the clock's *tock*, an alignment gap), **0x42c4** (the fountain enters the waterfall's `noise_burst` call at 0x42bd), **SJOG 0x1d32** (an arena step calls `sfx_footstep` 0x433e, a wrapper, not a primitive), and **EGA.DRV 0x269f / 0x29c5** (the title's dissolve and crackle; the driver has its own `noise_burst`).

**The whole table is code** — `openu5::sfx_sites()` in `native/core/src/sfx_inventory.cpp`, one row per call with its cue, status and one line of evidence. **126 sites: 95 Implemented** (4 of them a primitive's own `set_tone` / `stop`), **22 EvidenceUnknown, 4 NoNativeEvent, 5 DeferredPresentation, 0 DeferredMusic** (no speaker site is music; the Exodus songs are A3-04's and never enter this table).

The A3-03 rows (every one re-read for this batch; `a0` is the last push):

| Family | Site(s) | Program | Native trigger |
|---|---|---|---|
| **Victory fanfare** | COMBAT 0x0cf6 prints `\nVICTORY!\n`, 0x0cfd sets `g_cmb_victory_flag`, **0x0d02** calls kernel 0x4368; CAST 0x1759 after "… is wrought!" | 0x4368: 3 × TS(0x11f8, 1, 0x2a30, 0x12c, 6) + TS(0x17d4, 1, 0x5460, 0x12c, 3) — 1810 Hz × 3, then 2401 Hz; 2.09 s | the combat latch's `VICTORY!` (a `CombatEventKind::Message`); the shard ritual's `victory-fanfare` cue |
| **Shard sweep / Blackthorn siren** | CAST 0x15dd–0x162a; BLCKTHRN 0x03d0–0x040f (the same call) | 2 × 460 calls TS(0xa50, 1, 0xc8, si, 0), si = 0x7d0 → 0x61a8 by 0x32 and back: one pitch (1032 Hz) whose duty walks out and back; 7.13 s | the ritual's `shard-sweep` cue; the Blackthorn pacer's `ShardSweep` beat (A3-02 routed it, the synthesizer declined it) |
| **Shrine: ALAKAZAM / WELL DONE** | CAST2 0x0bd0–0x0c0f / 0x0c44–0x0c83 | the same two 460-call loops with (0xa8c, 0xc8) / (0xc1c, 0x96); 7.13 s / 5.35 s | `shrine-donation` / `shrine-well-done` |
| **Shrine: ORDAINED** | CAST2 0x0ac6–0x0b02 | 7 × TS from the four DS tables 0x4be6 / 0x4bf4 / 0x4c02 / 0x4c10 | `shrine-ordained` |
| **Quake** (harpsichord, Word of Power, WELL DONE, Codex, shard ritual, underworld EARTHQUAKE) | kernel **0x3072** (`screen_shake_rumble`; TOWN 0x0ea3, CMDS 0x12f9, CAST2 0x0c88, CAST 0x169d/0x16a0/0x16a3, MAINOUT 0x0a7d) | 8 passes over the viewport edges, each step `set_tone(rand_range(0x13, 0x96))`, `stop` at 0x316e — see §16.6 | the **`Quake` event**, one rumble per shake |
| **Refuge thunder** | BLCKTHRN 0x0acc / 0x0acf: two calls of the same 0x3072 | one rumble per peal | the Refuge's two `refuge-thunder` beats |
| **Refuge slumber** | BLCKTHRN 0x0a0d–0x0a49 after "But thy slumber is disturbed!" | 6 × TS from DS 0x3720 / 0x372c / 0x3738 / 0x3744; 10.1 s | that Refuge line |
| **Refuge revival** | BLCKTHRN 0x0b5d–0x0bb0 after "Strange words are intoned." | per member i: TS(0x8e30 / (i + 7), 1, 0x7530, 0x7d0, 2) — 0x03a0 is a 32-bit unsigned divide | that line; the party size |
| **Moongate** | kernel 0x48a8: tile under the party == 0xdc → `run_n_frames(1)`, TS(0x170c, 1, 0x7530, 0x7d0, 2) @0x48e5 | 2.3 kHz, 1.16 s | `moongate` (emitted since Alpha 2) |
| **Sceptre (wielding)** | CAST 0x197b–0x198f after "Wielding the Sceptre of Lord British…"; NB(10, 3000, 2000) @0x19e0 per dissolved field | TS(0x1450, 1, 0xc350, 0x1388, 1), 1.94 s | `sceptre` |
| **Sceptre reclaimed** | kernel 0x6209–0x6221 after "The Sceptre is reclaimed!" | TS(0xfd2, 1, 0xfde8, 1, 1), 2.52 s | that message |
| **Shadowlord drone** | TOWN 0x11d5–0x11e9 | TS(0x19c8, 1, 0xea60, 0x7d0, 1), 2.33 s | `shadowlord-announce` |
| **Healer jingle** | SHOPPES 0x13b0–0x1469 (callers: the Cure / Heal / Resurrect branches) | six TS, three mirrored pairs | a successful healer `Shop` result ("It is done.") |
| **Arena: escape / absorbed / grazed / dragged under** | SJOG 0x1c37 & CMDS 0x18ac / SJOG 0x1f08 / COMSUBS 0x0352 / 0x03d6 | GL(1200 → 2000, 1, 40) | `Escape!` / `… is absorbed!` / `… grazed!` / `… dragged under!` |
| **Arena: passes out / possessed** | SJOG 0x2218 / COMSUBS 0x01b1 | TS(0xc1c, 1, 0x7530, 0x3e8, 2) | `… passes out!` / `… possessed!` |
| **Arena: gates in a daemon** | COMSUBS 0x02cb | TS(0xac8, 1, 0x1388, 0x3e8, 0xf) | `… gates in a daemon!` |
| **Arena: ARGH! / regurgitated** | COMBAT 0x07f1 / 0x1cbf | NB(40, 3000, 500) / NB(1, 7000, 600) | `ARGH!` / `… regurgitated!` |
| **Arena: food stolen** | COMBAT 0x03b6 | GL(800 → 2000, 1, 50) | `A … stole some food!` |
| **Arena: blocked / same exit** | SJOG 0x1d59 / 0x1c1a | beep(0xa5, 0xc8) — the wall bump | `Blocked!` / `All must use the same exit!` |
| **Arena: footstep** | SJOG 0x1d32 → 0x433e | `move-step` | a party member's `Moved` event |
| **Arena / world: Absorbed! (magic)** | CAST 0x0e6e, COMBAT 0x0958 | TS(0x2648, 1, 0x6d60, 0x3e8, 2) = `spell-zap` | `Absorbed!` |
| **Plague!** | SJOG 0x0237 (`search_remains_outcome`) | NB(40, 3000, 500) = `search-fail` | the arena's `\nThou dost find\nPlague!` |
| **Ship collision** | MAINOUT 0x0300 after "COLLISION!" (not after "Docked!") | NB(100, 2000, 300) | `COLLISION!` |
| **Ship sinking / whirlpool** | MAINOUT 0x113b (sunk with no skiff, before "DROWNING!!!") / 0x12a6 after "WHIRLPOOL!" | GL(660 → 150, 40, 7800), 7.25 s | `DROWNING!!!` / `\nWHIRLPOOL!\n` |
| **Theft detected** | TALK 0x11a8 after "Something was stolen!" | GL(800 → 2000, 1, 50) | that message |
| **Wish granted** | LOOKOBJ 0x0129 after "\|Poof!\|" (the wishing well) | NB(10, 3000, 2000) | `\nPoof!\n` (the potion's `Poof!` is not it) |
| **Shard at the wrong flame** | CAST 0x166d after "\|\|No effect!\|" | GL(800 → 2000, 1, 50) | `\n\nNo effect!\n` (printed by exactly one core site; test D2) |
| **Location-29 trapdoor** | TOWN 0x0f96 `cmp [0x5893], 0x1d`; 0x0fb3–0x0fd3 `set_tone(v)`, `delay(0x28, 1)`, v = 1000 … 251; 0x101e NB(40, 3000, 500) per member as each dies | a 28 s falling tone, then a burst per member | `A TRAPDOOR!` while in location 29 — the original's own branch condition |
| **Ambient** (§16.4) | kernel 0x4102 | fountain NB(10, 30, 25000); waterfall NB(20, 60, 10000); tick / tock beep(3000 / 2000, 3); chime TS(0xc2c, 1, 0x7d0, 0x4e20, −10) | the runtime's ambient ticker |
| **Intro** | FONT 0x03ca / 0x0403 / 0x088d | NB(20, 60, 10000); beep(3000 or 2000, 3); NB(1, 1200, 4000) | the device's `IntroViewFrame` thunder / chime / summon flags, as the frame is shown |
| (already A3-02) | kernel 0x6a3c "A ring has vanished!" | GL(1200 → 2000, 1, 40) | `ring-vanishes`, emitted since Alpha 2 |

### 16.3 Emitted / audible / silent (Phase C)

Before A3-03:
1. **Emitted and audible (A3-02):** move-step, move-blocked, the dungeon cues, torch, ring, cannon, waterfall fall, mirror, the arena hit / heavy / defeat (derived), the ceremony (derived), the harpsichord, the four apparition cues, the Blackthorn materialize.
2. **Emitted but silent:** moongate, quake, sceptre, shard-sweep, victory-fanfare (shard), shadowlord-announce, the three shrine cues, refuge-thunder, the Blackthorn siren beat, and the four magic markers (spell-cast / potion-used / scroll-used / invalid-magic).
3. **Original sound, not emitted or derived:** the arena VICTORY! fanfare, escape / absorbed / blocked / footstep and the eight other arena messages, the healer jingle, the ambient proximity sounds, the attract demo's three cues, and the world messages of §16.2.

After A3-03: **set 2 is empty except the four markers** (their sound is the ceremony event's, by design), and **set 3 is closed wherever the original's trigger exists natively**. `a3_03_sfx_inventory` I4 fails if the core emits a cue the device does not play; `a3_03_sfx_runtime` W4 presents every supported cue through the real runtime and fails if one is dropped.

**No new core emission.** Every A3-03 sound is derived at presentation time from an event the core already emits (a message, a combat event, a `Quake`, a shop result, a Refuge line, an intro frame flag). This is deliberate: `quest_parity` serializes the Refuge beats and every Sfx event against the TypeScript reference, so a new core cue would have moved a pinned fixture. The derivation keys are the core's own literals, and `a3_03_sfx_inventory` D1 fails if any key stops being printed by the core.

### 16.4 The fountain and the other ambient sounds (Phase D)

**Where it comes from.** `ambient_sfx_tick` 0x4102 is called only by `viewport_redraw` 0x5910 (at 0x5a1a). The key wait `getkey_with_redraw` 0x266c calls 0x5910 once per pass (0x269a) — but only when `g_location < 0x21` or `> 0x7f`, i.e. never in a dungeon — and each pass spends `delay(1)` = one 55 ms BIOS tick while no key is down (0x1b38 → 0x20fa). 0x5910 skips 0x4102 under An Tym (`[0x587a] == 'T'`). (Correction: `ambient-audio-audit.md` §2 names 0x1070 as the key wait; 0x1068 is `fx_tile_fizzle_in` — `wind-rand-decision.md` had already said so. A DOSBox-X capture of the waterfall measured 54.9 ms between bursts, `audio-diff-calibration.md` §7.2.)

**What it does** (`openu5/ambient_sfx.h`, pure):
- scan the 11 × 11 window around the party (the arena's own cells, centre (5,5), in combat), x outer, y inner; keep the **nearest** object of a sounding class (squared distance strictly below the best so far, which starts at 0x33, so the far corner still counts; a tie goes to the first in scan order);
- classes: 1 clock (0xfa / 0xfb), 2 waterfall (0xd4–0xd7), 3 **fountain (0xd8–0xdb)**; the bellows, the moongate and every sprite are silent; class 4 (a bard sprite) is not ported (§16.7);
- sound: fountain and waterfall on **every** tick; the clock ticks at phase 0 and tocks at phase 4; while `[0x5884]` is non-zero it strikes instead (the chime TS), and `[0x5884]` runs down at phases 0 / 4 **whatever the class** (0x430e is outside the switch);
- the phase `[0x6a34]` is `(phase + 1) & 7` per call; `[0x5884]` is re-armed to the 12-hour clock (midnight → 12) by `advance_clock` (0x5164–0x5183), which the device reads as "the game clock moved".

**Runtime** (`AlphaRuntime::service_ambient`, called from the gameplay render loop): one 0x4102 call per 55 ms tick while the original would be waiting in 0x266c — the world, a town or any arena (a dungeon room's too: an arena runs at `g_location` ≥ 0x80), **not** a dungeon corridor, a paced scene (Camp, Refuge, TrollSneak, Blackthorn), the Ending, the Developer menu, An Tym, a map reveal or a gem / zodiac view. The System Menu and the title never reach it (they own the render path). The window is the **raw** terrain (`WorldTerrain::effective`, with the hour / persistent / transient overrides), never the light-censored view, so a fountain burbles at night too. A new world (load, System Menu Load, title Continue, Return to Title) starts both counters over.

| Question | Answer |
|---|---|
| Trigger | the nearest sounding object within the 11 × 11 window |
| One-shot or recurring | recurring: one burble per 55 ms tick; each is NB(10, 30, 25000) = 3 draws × 15 sweep samples = **1.7 ms**, a click |
| Movement / time | the original ticks it on every redraw — the idle key wait and each move's redraw; the device ticks it every 55 ms while eligible, so walking keeps it going |
| Range | 11 × 11 window (the far corner, distance 50, counts); leaving it stops it within one tick |
| Several objects | the nearest wins; there is one sound per tick |
| Stuck tone | impossible: every ambient program is 1.7–77 ms long and never queued |
| Volume / mute | the SFX channel gain; at 0 % the service never submits (F6) |

**Hardware-visible behaviour:** standing beside the fountain in location 1 at (6,25), the host counts **20 burbles in 1.1 s, exactly 55 ms apart**, each ending (F1–F3); a real clock in location 2 ticks and tocks once each per 440 ms and, after a step moves the clock, strikes the hour (12 at noon) before ticking again (C1–C2).

### 16.5 Combat victory and leaving the arena (Phase E)

- **When it fires:** COMBAT 0x0cf6–0x0d02 prints `\nVICTORY!\n`, sets `g_cmb_victory_flag` and calls the fanfare, **only if the flag was still 0** — once per arena. The defeat branch (0x0cda, `\nBATTLE IS LOST!`) calls nothing. Every arena uses it: overworld and underworld encounters, the troll bridge, camp ambushes, dungeon rooms (one engine, `combat.cpp`, reached through `start_encounter_combat` / `start_fixed_combat`); arenas that start already won (the six empty final rooms) never print it and stay silent, as in the original.
- **Fanfare, not beeps:** four tone sweeps, two pitches: 1810 Hz × 3, then 2401 Hz twice as long (measured from the rendered PCM, `a3_03_sfx_inventory` P1).
- **Native trigger:** the latch's `VICTORY!` line (`CombatEventKind::Message`). The engine prints a second `VICTORY!` as the `Ended` line when the party finally walks off the arena; that line can repeat and also fires for arenas that start won, so **it never sounds** (`sfx_for_combat_text(…, ended = true)` → none; mutation M5).
- **Leaving the arena:** walking a member off the edge (`Escape!`, SJOG 0x1c37) and the escape command (`Escape!`, CMDS 0x18ac) play GL(1200 → 2000, 1, 40). `Leave!` (no enemy left) has no adjacent speaker call and is silent.
- **The troll case (the user's report):** the toll refusal's own entry, `start_encounter_combat(enemy 41)`, through the real runtime: one fanfare, heard in full (V0–V2); an ordinary arena the same (V3); a lost battle and an escape have no fanfare (V4).
- **No delay:** the original's fanfare blocks for 2.09 s and then flushes the keyboard buffer (0x0d05 → kernel 0x1b16). **The device reproduces neither**: audio never paces, per the A3-01 contract. V5 runs the troll victory under six audio setups and exploration returns at the same instant (16.305 s) with identical state in all of them. The un-reproduced 2.09 s hold and key flush are a presentation-timing divergence, recorded in the ledger; the TypeScript reference *does* pause for it (`BLOCKING_CUES`) — a decision this batch does not take.

### 16.6 Moongate, quake, shrines, ritual (Phase F)

- **Moongate** plays when the core already emits `moongate` (stepping onto an active gate). The transit animation stays out of scope.
- **The quake is kernel 0x3072.** Its eight passes redraw the viewport's edges in strips (`call 0x71ca` / `0x0ace` / `0x7200`), and after each strip write a new PIT count: `set_tone(rand_range(0x13, 0x96))`, i.e. 19–150. The gate stays open from the first write to the `stop` at 0x316e. `set_tone` writes the count only (0x22f2–0x22fe, no control word), so in mode 3 each write takes effect at the next half-cycle: **the sound is a square whose every half-cycle is 1 / (2v) s for a freshly drawn v** — a random low rumble (the witness measured a peak near 106 Hz). The synthesizer's new `Rumble` primitive emulates exactly that; the draw is `rand_range` 0x2092's own arithmetic (the `[0x545c]` step, `& 0x7fff`, `lo + v % (hi − lo + 1)`, closed interval — R2).
  - **Class C, declared:** the loop has no timer (its pace is the strip blits), so the length is the shake's measured window, 8 × 117 ms = 936 ms (`presentation.h`; a `static_assert` ties the two); and the draws come from the **game** RNG `[0x5420]` inside the render path, which the port never consumes for presentation (a registered deliberate divergence, `oracle-flash-rng.md`), so the rumble draws from its own word: the law is exact, the sequence is not.
  - **One rumble per shake.** The rumble is keyed to the `Quake` event, not the `quake` cue: the shard ritual shakes three times (CAST 0x169d / 0x16a0 / 0x16a3) on one cue, the Codex three times on three cues, and every other shake once. The cue is kept (the reference pins it) and sounds nothing on its own (W4, S1, S2; mutation M11).
  - The TypeScript reference renders the quake as a tri-band noise burst calibrated to the witness; A3-03 departs toward the bytes.
- **Shrines.** ALAKAZAM, WELL DONE and ORDAINED play as the shrine emits them (S3). WELL DONE is followed by its shake; in the binary the sweeps and the shake are serialized, and the device's FIFO plays them in that order (sweeps 5.35 s, then the rumble — S1). **The visible divergences stay as they were:** the device still shakes at once and draws no inverted viewport during the sweeps (H-184), and the shrine's key waits are still dropped (H-183). Audio was added without touching those scenes; their timing, frames and state are identical under all six audio setups (S4).
- **Shard ritual:** the sweep (7.13 s), three rumbles, the fanfare — the binary's order, FIFO (S2); a shard held at the wrong flame plays its GL(800 → 2000) "No effect!".

### 16.7 Bard and lute (Phase G)

- **Camp lute:** the watch's bard plays only with the sound flag on (`[0xa9ce]`, CMDS 0x0123): `run_n_frames(0x34)`, 52 redraws, each advancing the sprite and one note of the 53-index melody through 0x4102 class 4. Batch 51 kept the **sound-off** branch on the device (§4, §10): no lute scene, no drag variation. **SFX Volume is not the sound flag** and never selects a branch — `a3_03_sfx_inventory` I5 fails if any gameplay module reads `sound_volume` or emits `bard-song`.
- Playing the lute would mean mounting the lute scene — a pacing change this batch must not make. **Status: DeferredPresentation**, with the note derived and ready: melody DS 0x6a48, frequencies DS 0x6a34, TS(freq, 1, 0x7d0, 0x4e20, −10) per non-rest index (`sfx-bardo-adjudicacion.md`).
- **Class 4 elsewhere** (a bard sprite 0x15C–0x15F in view, e.g. a tavern bard): the gate reads the sprite layer `[0xac64]` with the terrain layer `[0xab02]` empty; the device composes layers differently. Same status.
- **The harpsichord** is unchanged from A3-02 (TOWN 0x0e34; ten notes, H-125 hardware-confirmed). Its quake is now audible (Q1).

### 16.8 Blackthorn and the quest scenes (Phase H)

- **Materialize:** A3-02 (unchanged).
- **Sacrifice siren:** BLCKTHRN 0x03d0–0x040f is the shard ritual's ladder, bit for bit; the pacer already held its 7.13 s and handed the beat to the cue sink, and the synthesizer now plays it (B1). The next beat still follows **the pacer's** hold under all six setups (B2).
- **Refuge:** the two peals (each one 0x3072 call), the slumber melody and the revival tones now sound at their lines; the Refuge's timeline, frames and state are identical under all six setups (R1–R2). The Refuge's own cadence remains class C (H-185).
- **Explosion / "fanfare":** the sacrifice's explosion (H-186) has no speaker call of its own in the binary; the shard ritual's fanfare is §16.6.

### 16.9 Intro, title and endgame (Phase I)

- **Attract demo** (FONT.OVL's scene engine, which INTRO shares): the device already runs it and already set `thunder` / `chime` / `summon` flags on the frames that carry them; A3-03 routes them as each frame is shown. The chime is 3000 on the gate's first frame and 2000 on its fifth (FONT 0x03e8–0x0403). 90 s of attract: thunder ×4, chime ×8, summon ×1 (W3). The host fixture now binds the View data as `initialize()` does, so the demo runs in host tests.
- **Title dissolve / subtitle crackle** (EGA.DRV 0x269f / 0x29c5): the device's title has neither effect. DeferredPresentation.
- **Endgame** (ENDGAME 0x078f "… lives!", 0x0987 the orb): the cinematic is deferred (D-54). DeferredPresentation. No `victory` transcript line triggers a sound: only the combat latch's does.
- **No music** is started anywhere.

### 16.10 The completeness guard (Phase J)

`openu5/sfx_inventory.h` holds two tables and `a3_03_sfx_inventory` enforces them:

| Guard | Fails when |
|---|---|
| I1 | an id is `Implemented` without a program, or has a program without being `Implemented`; an unsupported id has no reason; the silent set is not exactly the eleven of §16.18 |
| I2 | a census call site (121, parsed from the committed census log) has no row, or a row claims a census site the census does not have; the five hand-found calls are missing |
| I3 | an `Implemented` site names a cue with no program; an implemented original cue has no site behind it |
| I4 | a cue literal the core emits maps to a silent id (other than the four magic markers) |
| I5 | the core emits `bard-song`, or a gameplay module reads `sound_volume` |
| D1 / D2 | a derivation key stops being printed by the core; a message the binary follows with no sound (`Docked!`, `BATTLE IS LOST!`, the Ended `VICTORY!`, `Leave!`, the potion's `Poof!`, door-jimmy `Key broke!`) would sound |
| W4 (runtime) | the runtime drops any supported cue |

Statuses: `Implemented`, `DeferredMusic`, `DeferredPresentation`, `EvidenceUnknown`, `IntentionallySilent`, and **`NoNativeEvent`** — added because four original sites are neither unknown nor deferred: their trigger simply does not exist in the native core ("Magic absorbed!", "A shadowlord appears", the dungeon teleport ring, the chest-object jimmy).

### 16.11 Ambient and repeat policy (Phase K)

| Rule | Why |
|---|---|
| **Ambient** is a new lowest class | it recurs every 55 ms; losing one tick is inaudible |
| an ambient tick **never queues**: a busy voice (or a non-empty queue) skips it (`SfxAdmit::Skipped`) | the original only reached 0x4102 when nothing else was sounding — every other primitive blocked |
| **any** other cue cuts a playing ambient tick (2 ms fade) and starts | ambience never delays a step, a hit or a scene cue |
| a scene cue skips ambience like everything else; ambience never preempts anything | Q1, M3 |
| **only a held key's step / bump coalesces** (A3-02 coalesced every identical repeat) | three shakes, three Stonegate drones, one damage burst per member are three real calls in the binary; A3-02's rule played two of three (Q4, S2; mutation M12) |
| the Shadowlord drone moved from Scene to Ordinary | it is no scene's beat; Stonegate's three announces must each play, FIFO |
| the fanfare is Combat (FIFO), the shard / shrine ladders Spell (FIFO), the Refuge cues Scene (latest wins) | the binary's order where the game is not paced; the pacer's clock where it is |

40 ticks beside a fountain: 40 bursts, each ending, nothing piling up (Q3). Walking past it: 8 ticks skipped during the step and bump sounds, none queued, no overflow (F5).

### 16.12 Scene and timing safety (Phase L)

Six audio setups, as in A3-02: none, the synthesizer, SFX 0 %, a failing backend, stalled audio (never rendered) and racing audio (rendered 100× ahead). Every sample of the scene state, every frame timestamp and the final game state must be identical.

| Scene | Check | Result |
|---|---|---|
| Camp apparition | A3-02 T19 (still run) | identical |
| Blackthorn entry | A3-02 T21 | identical |
| **Blackthorn sacrifice** (siren now audible) | B2 | identical; the next beat 7.13 s after the siren in every setup |
| **Refuge** (thunder, slumber, revival) | R2 | identical |
| **Shrine WELL DONE** (sweeps + shake) | S4 | identical |
| **Quake** (the harpsichord melody) | Q2 | identical |
| **Combat victory** (troll arena) | V5 | identical; exploration returns at 16.305 s in all six |

"Queue full" is covered by Q3/F5 (no overflow) and A3-02 Y10; "audio task delayed" is the stalled setup.

### 16.13 Load and mode safety (Phase M)

| Route | Result |
|---|---|
| Alt+L beside a fountain | silent within 2 ms; no burble in the loaded world (M1) |
| System Menu Load | the same (M2) |
| System Menu open | no ambient tick while open; the fountain resumes when it closes (M3) |
| Return to Title | silent; no ambience on the title (M4) |
| Title Continue | nothing from the old world survives (M5) |
| Alt+L during the fanfare | faded within 2 ms, nothing queued survives (M6) |
| Developer teleport | the ambience follows the new position within one tick (F4) |
| Dungeon corridor | no ambient tick at all (M7) |
| Arena (a dungeon room's included) | ticks, reading the arena's own cells around (5,5) (F7) |
| Ending | no ambience (M8) |
| Alt+L with strikes pending | the loaded world only ticks (M9): a load resets `[0x6a34]` / `[0x5884]` |

### 16.14 Tests (Phase N)

**New ctest targets (2):**

| Target | Checks | Covers |
|---|---:|---|
| `a3_03_sfx_inventory` | 48 | I inventory / completeness; D derivations; P programs against the binary's constants; R the rumble and the ladders measured from PCM; A the ambient scan / ticker; Q the policy |
| `a3_03_sfx_runtime` | 46 | the real `AlphaRuntime`: F fountain (and an arena's), C clock, V victory (troll + ordinary + lost + escape + six-setup timing), Q quake, S shrines / ritual, B Blackthorn sacrifice, R Refuge, W world / shop / intro / no dropped cue, M load / mode |

| Brief item | Checks |
|---|---|
| 1 fountain cue emitted | F1, P5, A3 |
| 2 repeat cadence | F2, A3 |
| 3 no stuck sound | F3, P5, Q3 |
| 4 low priority / preemption | Q1, Q2, F5 |
| 5 mute | F6 |
| 6 troll victory | V0, V1 |
| 7 ordinary victory | V3 |
| 8 exactly once | V1, V2, D2 |
| 9 not on failed / aborted combat | V4, D2 |
| 10 no gameplay delay | V5 |
| 11 moongate | P4, W4 |
| 12 quake | Q1, R1–R3 |
| 13 no timing change | Q2 |
| 14 shrine cue | S1, S3, P3 |
| 15 ritual cue | S2, P2, R4 |
| 16 presentation timing unchanged | S4, B2, R2 |
| 17 bard note order | DeferredPresentation (§16.7); the harpsichord's order is A3-02 H16 / P4 |
| 18 sound-off semantics separate from volume | I5; A3-02 T19 (Camp identical at 0 % and 80 %) |
| 19 Blackthorn remaining cue | B1, B2 |
| 20 final victory cue | not original-backed on the device (ENDGAME cinematic deferred); the shard ritual's fanfare S2 |
| 21 every supported id has a mapping | I1, I3 |
| 22 every unsupported id classified | I1, I2 |
| 23 runtime drops no supported cue | W4, I4 |
| 24 ambience flushes on load / title | M1–M5, M9 |
| 25 fanfare flushes safely | M6 |

**Existing guards edited** (the contract each guards is unchanged):
- `a3_02_sfx_synth` E12: the supported count 23 → 62, and the "undeclared id" example `moongate` → `bard-song` (moongate is now wired).
- `alpha_runtime_host_fixture.cpp` binds the attract demo's View data as `initialize()` does.

### 16.15 Mutations (Phase O)

`native/core/tools/a3_03_mutation_check.py` → `native/core/a3-03-mutation.log`: 24 one-line production mutations, each run against the A3-03 suites and the A3-02 / A3-01 audio suites.

- **First pass** (`native/core/a3-03-mutation-first-pass.log`, 24 mutations): 21 killed, **3 survived**, and M5 died only by `-Werror`.
  - **M13** (the dungeon rule dropped) survived because the rule was stated twice — as the UI mode and as the dungeon context — and the context test also silenced arena fights *inside* dungeons, where the original's `g_location` is ≥ 0x80 and 0x266c does redraw. The gate was corrected to the binary's rule (a corridor is silent, any arena ticks; audit finding F10), and F7 plus the new M26 guard the arena side.
  - **M21** (the healer's type test dropped) survived because the test's other shop used other words; W2 now presents "It is done." from another counter and a healer result that is not the service (new M25 guards the second test).
  - **M24** (the attract thunder not routed) survived because W3 summed the three flags; it now requires each, and the 2000 Hz chime.
- **Second pass** (`native/core/a3-03-mutation.log`, 26 mutations): **26 killed, 0 survived**; M5 was still a compile kill (a self-comparison), so it was rewritten to `(void)ended;` and re-run alone (`native/core/a3-03-mutation-m5-rerun.log`): **killed by D2 and V1–V3 / V5**. Every restored build and all five audio suites green.

| # | Mutation | Killed by |
|---|---|---|
| M1 | the fountain cue never emitted | A3, F1–F4 |
| M2 | ambience on every render, not per 55 ms tick | F1, F2, C1 |
| M3 | ambience preempts a scene / gameplay cue | Q1, F5 |
| M4 | the troll (arena) victory cue omitted | D1, V1–V3, V5 |
| M5 | the victory cue fires twice (the Ended line sounds) | D2, V1–V3, V5 (re-run) |
| M6 | the quake sound drives its shake | Q2, S4 |
| M7 | ritual audio controls the scene pacing | R2, A3-02 T19 |
| M8 | a load leaves the ambience state running | M9 |
| M9 | an unsupported id accepted (bard-song) | I1, A3-02 E12 |
| M10 | the runtime drops a supported cue (moongate) | W4 |
| M11 | one rumble per cue instead of per shake | Q1, S1, S2, W4 |
| M12 | every repeat coalesces (the A3-02 rule) | Q4 |
| M13 | ambience in a dungeon corridor | M7 |
| M14 | both ladder legs rise | P2, P3, R4 |
| M15 | the fanfare's last note at the first pitch | P1 |
| M16 | the trapdoor ramp everywhere | W1 |
| M17 | the clock never re-armed | C2, M9 |
| M18 | the scan keeps the last of equal distances | A2 |
| M19 | one revival tone whatever the party | R1 |
| M20 | the escape glide omitted | D1, V4 |
| M21 | the healer jingle on any shop's result | W2 |
| M22 | the rumble a flat tone | R1 |
| M23 | the chimes run down only beside a clock | A4 |
| M24 | the attract thunder not routed | W3 |
| M25 | the healer jingle on any healer result | W2 |
| M26 | no ambience in an arena | F7 |

### 16.16 Firmware (Phase P)

- Pre-commit build `native/targets/tdeck/build-a3-03` (`native/targets/tdeck/a3-03-firmware-configure.log`, `a3-03-firmware-build.log`): ESP-IDF 6.1, `idf.py reconfigure` then `ninja -j 4`, first attempt clean, **zero project warnings** under `-Werror`.
- `0xe0a40` = **920,128 B**, **+5,520 B** against A3-02's 914,608; **128,448 B (12 %)** of the 1 MiB app partition free.
- Audio memory: the backend object 784 → **912 B** (+128 B: two ladder fields in each segment of the two 24-segment program copies, and the rumble's state). The queue (16 × 16 B), the audio task (4 KiB stack, core 1, priority 3) and the DMA are unchanged. Each ambient tick costs the game thread 121 terrain reads and at most one 16-byte post with a zero timeout.
- The post-commit image is built in a fresh directory without ccache; its path, SHA-256 and embedded `Git` are in the annotated tag. Not flashed.

### 16.17 Hardware test (Phase Q) — A3-03 device check

Copy the Launcher image named in the annotated tag. SD card unchanged; `openu5-audio.bin` optional (no SFX needs it). SFX Volume 80 % unless a step says otherwise.

- **A. Boot.** The identity screen reads `FW 3.0.0-alpha3-dev-a3-03-debug` and the tag's `Git` hash; `RES 2041466B CRC 26f75ae6` as before.
- **B. Fountain.** Developer > Teleport to Moonglow (location 1), X=6, Y=25 — beside the fountain. Stand still: a soft, fast crackle ("burble") about 18 times a second. Walk 6 cells away: it stops. Walk back: it returns. It never turns into a held tone. (Optional: a clock — location 2, X=13 Y=2 — ticks and tocks; take a step and it strikes the hour.)
- **C. Combat victory.** Fight trolls (a bridge toll refused, or any encounter): when the last enemy falls, **one** fanfare — three equal notes and a higher, longer fourth — then walk off the arena: no second fanfare. Losing a fight or escaping plays no fanfare (escaping plays a short rising glide).
- **D. Quake / Word of Power.** At Deceit's entrance (Britannia, beside it; the known FALLAX setup), (Y)ell the word: the screen shakes with a low random rumble lasting the shake (~0.9 s).
- **E. Shrine / ritual.** Any shrine: donate (ALAKAZAM, a long whistling tone whose timbre sweeps up and down, ~7 s), or complete a quest (WELL DONE, ~5 s, then the rumble). Or the harpsichord's 6 7 8 9 8 7 8 7 6 7 6 5 3 on LB castle floor 2: the passage opens with the rumble.
- **F. Bard / lute.** Not in this build: the Camp keeps the 1988 sound-off lute branch (§16.7). Nothing to check.
- **G. Load flush.** Beside the fountain, press Alt+L into a save elsewhere: silence at once, no burble after "Load complete". Win a fight and press Alt+L during the fanfare: it stops at once.
- **H. Volume.** Beside the fountain: 100 %, 50 %, 0 % — quieter each step, silent at 0 %.
- Not in this check: music (A3-04), final loudness and tone (A3-05).

### 16.18 Remaining SFX (Phase S)

**Cue ids that stay silent (11), each classified:**

| Id | Status | Why |
|---|---|---|
| `bard-song` | DeferredPresentation | the Camp lute is the sound-on branch of a scene the device runs in its sound-off form (Batch 51); the tavern bard needs the sprite-layer gate (§16.7) |
| `title-fizzle`, `title-crackle` | DeferredPresentation | the device title has no dissolve and no subtitle crackle |
| `endgame-orb` | DeferredPresentation | the endgame cinematic is deferred (D-54) |
| `line-spray` | EvidenceUnknown | CAST 0x1f60: the fan's lead and a crackle drawn from the game RNG per painted pixel; no native event marks the fan, and its owning spells are not mapped |
| `combat-reject` | EvidenceUnknown | SJOG 0x1f26's two beeps: which native refusals are the funnel's callers is not established (the device's `What?` is UI text) |
| `invalid-magic` | EvidenceUnknown | the failure glides are per-spell CAST branches (0x0eb2 / 0x11d3 / 0x1325 / 0x1ba7); none maps to the native generic failure |
| `cast-spell` | IntentionallySilent | the reference's disputed name for the 0x4368 fanfare; never emitted (casting sounds through the ceremony) |
| `spell-cast`, `potion-used`, `scroll-used` | IntentionallySilent | markers: the `MagicCeremony` event that follows them plays CAST2 0x0000 |

**Sites with no sound on the device (31 of 126):** the 22 `EvidenceUnknown` rows (glides and sweeps in CAST / CAST2 / COMBAT / COMSUBS / MAINOUT / DUNGEON with no adjacent message, the kernel 0x350a cell impact, the line spells, the reject funnel), 4 `NoNativeEvent` ("Magic absorbed!", "A shadowlord appears", the dungeon teleport ring, the chest-object jimmy) and 5 `DeferredPresentation` (class 4, the endgame, the title). Each row in `sfx_inventory.cpp` names its site and the reason. Closing the `EvidenceUnknown` rows is attribution work — tracing each routine to its caller — not audio work.

**Departures from the TypeScript reference** (each toward the bytes; none in a layer a fixture pins):

| Item | Reference | A3-03 | Why |
|---|---|---|---|
| `sceptre` | TS(0xfd2, 1, 65000, 1, 1) | TS(0x1450, 1, 50000, 5000, 1) (+ NB per dissolved field) | the reference used kernel 0x6221's pushes ("The Sceptre is reclaimed!"); the native cue follows "Wielding the Sceptre…", which is CAST 0x198f. 0x6221 is now its own cue |
| `quake` | tri-band noise, calibrated | the 0x3072 rumble (random half-cycles 19–150 Hz) | §16.6 |
| `shard-sweep` & the shrine ladders | one tone, the duty not modelled | 2 × 460 calls with the duty walking | A3-02's duty model, applied per call |
| repeats | — | only a held key's step / bump coalesces | §16.11 |
| victory / shard fanfare | pauses the game and flushes keys (`BLOCKING_CUES`) | never pauses | the A3-01 contract; recorded as a timing divergence |

### 16.19 Next batches

| Batch | Scope |
|---|---|
| **A3-04 — music playback from supported patched assets** | the XMI sequencer, OPL2 emulation and the `FAT.OPL` bank on the core-1 task, mixed after the SFX voice; `music_context_for_location` and the scripted selectors; the combat → victory song switch after `VICTORY!`; CPU and memory measured on hardware. Nothing in A3-03 constrains it: the SFX voice is one channel and music is the other. |
| **A3-05 — loudness / tone balance and full audio hardware sign-off** | the loudness curve on the real speaker, SFX / music balance and headroom, the tone character the user noted (partly the small speaker), a battery and CPU soak, the strict "1988 sound-off" profile decision (which would also decide the lute, §16.7), and the ledger rows (D-3). |
| (separate, not audio) | attribution of the 22 `EvidenceUnknown` sites; the presentation batch H-183–H-186 (shrine key waits, the ritual inversion, the Refuge cadence, the sacrifice burst), the endgame cinematic (D-54). |
