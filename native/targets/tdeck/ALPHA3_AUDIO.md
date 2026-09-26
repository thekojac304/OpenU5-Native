# Alpha 3 — Audio architecture (A3-01)

**Status: A3-01 COMPLETE (architecture, capability detection, volume settings, tests, one diagnostic tone). Not an Alpha 3 release.** Gameplay is still silent on the device: every semantic sound cue now reaches an audio service, but the device backend renders only the Developer test tone until A3-02.

This document is the audio track's reference. It records what the original does, what the community music patch adds, how the port tells the two apart, and the contracts later batches must keep.

## 0. The rule in one table

| The user's DOS files | Sound effects | Music | What Settings shows |
|---|---|---|---|
| **Stock** (unpatched *Ultima V* DOS) | Supported. The effects are the original's PC-speaker sounds, synthesized from the original's own parameters (A3-02/A3-03). No asset is needed. | **None.** The 1988 game has no music. | `Music Volume: Unavailable`, footer *Stock DOS game files have no music* |
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

**Not yet run on hardware.** A3-01 device check:
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
