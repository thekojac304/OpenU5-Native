# Alpha 3 — Audio (A3-01 architecture, A3-02 PC-speaker synthesizer, A3-03 remaining SFX, A3-04 music playback, A3-04A real-time playback, A3-04B render contention, A3-04C contention map, A3-04D SD-log isolation, A3-04E render pacing, A3-04E.1 idle service)

**Status (A3-04E.1 hardware closeout, 2026-09-27): WATCHDOG REGRESSION FIXED ON HARDWARE — RENDERER PACING HARDWARE-VALIDATED — A3-04E / A3-04E.1 CLOSED.** Two serial captures of the A3-04E.1 image (§23.10, committed as `a3-04e1-hw-soak.log` and `a3-04e1-hw-probes.log`) contain no `task_wdt`, crash or reboot. The first covers 11 min of uptime in default pacing, with music at 80 % and then 0 %, walking, town changes, menus and 3 min standing still. Over that whole run core 0's idle loop never went more than 102.9 ms without a pass, the guard never had to act (`forced=0`), and audio stayed at `und=0 hw=0 miss=0`. All 110 heartbeats `hb=13`–`122` are present. In the second capture, the legacy probes reproduced A3-04D exactly: menu-exit repaint 441–442 ms, walking steps ~155 ms, TFT max 408.7 ms. Switching back to the yield pacing brought steps to ~88 ms and the repaint to ~184 ms. The deliberately starving loop-spin phases made the guard act 262 times, and it held the idle gap at ≤ 223.9 ms. The §22.10 matrix was abbreviated (§23.10.4). Next: the ambient-SFX parity batch (§23.11).

**Status as A3-04E.1 wrote it: TASK-WATCHDOG REGRESSION FIXED ON THE HOST — HARDWARE VALIDATION PENDING (watchdog soak first).** A pre-test run of the A3-04E image tripped the task watchdog on IDLE0 after ~71 s, with `main` running. A yield never gives core 0 to the lower-priority idle task, and A3-04E's assumption that the TFT rows' own waits let it finish a pass was never a guarantee. The model reproduces the trip: 6.1 s without an idle pass. A3-04E.1 (§23) keeps A3-04E's pacing and adds a guard that watches core 0's idle loop through a second idle hook (the same pass that feeds the watchdog). It blocks the game thread for one tick only when that loop has not run for 200 ms. In the model the longest gap is then 200.9 ms, costing 4.3 % in the worst case and nothing where the idle loop already runs. The watchdog is untouched. The §22.10 matrix waits for the §23.8 soak.

**Status as A3-04E wrote it: RENDERER TICK SLEEPS AND MAIN-LOOP SPIN REMOVED — HOST-PROVEN WITH THE REAL BOARD CODE — HARDWARE VALIDATION PENDING.** With SD logging off, A3-04D's device runs still showed a ~400–445 ms frame/TFT maximum and a ~122 ms viewport average, the same with and without the synth. A3-04E (§22) found that neither is a stall. Every draw loop slept to the next 10 ms tick every 16 rows. A walking step crossed 9 of those sleeps, and the full-screen repaint on leaving the Developer menu, which opens every measurement window, crossed 37. The real `tdeck_board.cpp`, built for the host over a fake ST7789, reproduces both counts; its model gives 391.7 ms for the repaint, against the device's 404–408 ms. The draw loops now yield at the same points instead of sleeping (modelled repaint 113.8 ms, step 27.7 ms; byte-identical panel stream). The main loop, whose `vTaskDelay(pdMS_TO_TICKS(5))` was 0 ticks, now blocks on the input queue for one tick when nothing relative-timed is running. Each half is behind its own Developer probe (*Probe: legacy TFT pacing* / *legacy loop spin*), so the device can measure it alone. Not hardware-validated until the §22.10 runs pass.

**Status as A3-04D wrote it: SD DIAGNOSTIC LOGGING OFF BY DEFAULT — MECHANISM PROVEN FROM THE SOURCE — HARDWARE CONFIRMATION PENDING.** The A3-04C runs showed 0.8–1.5 s frame/TFT stalls with the synth bypassed and with music off, each within a few ms of the SD-log writer's longest burst. A3-04D (§21) traced why. The SD card shares SPI2 with the TFT, and ESP-IDF's sdspi driver holds that bus for a whole card command, the card's busy time included, at 800 kHz. The diagnostic SD log is therefore now **off at boot**, one Developer keypress (*Probe: SD diag logging*) turns it on for a session, and serial logging is unchanged. Every report and `A3C_PERF` line says `sdlog ON/OFF`, and slow TFT transactions are attributed to SD-log bursts (`insd=`). The ON/OFF device runs of §21.7 are pending, and the batch is not hardware-validated until they pass.

**Status as A3-04C wrote it: CONTENTION MAP INSTRUMENTED — CAUSE NARROWED, NOT PROVEN — HARDWARE EVIDENCE PENDING (Outcome C).** The user still sees the map lag with music on and much less at Music Volume 0 %. A3-04C (§20) found that 0 % turns off three things at once: the synth, the I2S DMA/interrupt on core 0, and the audio task's wakes. It adds the measurements that separate them: a per-frame TFT split (row building / SPI / tick yields), a row-level test of what core 1 was doing while each TFT row was built and sent, SD-log bus bursts, the audio task's own work per block, and a one-line `A3C_PERF` heartbeat. It also adds a Developer probe, *Probe: synth bypass*, that keeps the song, channel and cadence but skips the synth. No gameplay, audio output or renderer behaviour changed. The next step is the §20.10 hardware runs; their result picks A3-04D (§20.11).

**Status as A3-04B wrote it: MUSIC SMOOTHNESS IMPROVED — RENDER PERFORMANCE RETEST PENDING.** On the A3-04A image music played smoothly but the overworld lagged with music on and was much better at Music Volume 0 %, and the Developer audio benchmark showed no results. A3-04B (§19): the synth — which at 0 % does not run at all — executed from flash through the instruction and data caches both cores share, contending with the renderer on the other core; its per-sample path is now bit-exactly ~39 % cheaper per channel and runs from IRAM (proved on the linked image). The benchmark's results, the game thread's frame counters and FreeRTOS per-core/per-task CPU are one report on the Developer screen that stays until dismissed. The retest image exists (§19.15–19.16). Not an Alpha 3 release.

**Status as A3-04A wrote it: A3-04 music on hardware — PLAYBACK FUNCTIONAL — SMOOTHNESS RETEST PENDING.** The A3-04 image played the right songs but extremely stuttery; the cause was a FreeRTOS mutex taken on every synth table read (a function-local static under `-mdisable-hardware-atomics`), fixed in A3-04A together with a per-file `-O2`, a primed/drained DMA ring, boot-time song parsing and on-device performance counters (§18). The retest image exists (§18.17–18.18). Not an Alpha 3 release.

**A3-04 status as it was written:** SOFTWARE COMPLETE, HARDWARE VALIDATION PENDING (music playback from the supported community patch). The device now decodes and plays the Exodus Project *Ultima V Upgrade* 1.0's 16 XMI songs through a from-scratch OPL2 emulator, driven by `AlphaRuntime::sync_music()` at the same boundaries the patch driver itself re-derives its selector (every key poll, load, Ending, Camp). Host-validated end to end against the real patch corpus (67 + 18 = 85 new checks, §17.14) and by mutation (§17.15); the ESP-IDF firmware builds clean. Only the physical loudness/tone balance and an audible confirmation on the T-Deck are pending (§17.17), because the user was away from the device for this batch — A3-03's own hardware retest is carried forward alongside it (§17.17, item K). A3-01 (sections 1–14) is the architecture, §15 is A3-02, §16 is A3-03, and §17 is A3-04.

This document is the audio track's reference. It records what the original does, what the community music patch adds, how the port tells the two apart, and the contracts later batches must keep.

## 0. The rule in one table

| The user's DOS files | Sound effects | Music | What Settings shows |
|---|---|---|---|
| **Stock** (unpatched *Ultima V* DOS) | Supported. The effects are the original's PC-speaker sounds, synthesized from the original's own parameters: 22 since A3-02 (§15.5), 62 of the 73 cue ids since A3-03 (§16). No asset is needed. | **None.** The 1988 game has no music. | `Music Volume: Unavailable`, footer *Stock DOS game files have no music* |
| **Supported music patch** (Exodus Project *Ultima V Upgrade* 1.0) | The same. | **Enabled** (§17); smooth real-time playback fixed in A3-04A (§18); its contention with rendering addressed in A3-04B (§19); the lag that remains with music on is instrumented in A3-04C (§20), hardware evidence pending. Loudness/tone balance is A3-05. | `Music Volume: 80%`, adjustable |
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

## 17. A3-04 — music playback from the supported patch

The user was away from the T-Deck for this batch. Everything below is **host-first and host-validated**; §17.17 is the test plan to run on hardware together with A3-03's carried-forward retest, next time the two are together.

### 17.1 Baseline (Phase A)

- HEAD `76d4dd43` = tag `alpha3-a3-03-remaining-sfx`, tree clean.
- Fresh host build and **serial ctest: 131/131 passed in 126.24 s** (`native/core/a3-04-baseline-configure.log`, `a3-04-baseline-build.log`, `a3-04-baseline-ctest.log`), taken with `music_synth.{h,cpp}` stashed out so the number is genuinely A3-03's, not A3-04 code sitting inert in the tree.
- Confirmed already in place: the whole A3-01 plumbing for music — `MusicContext`, `MusicSong`, `music_context_for_location`, `AudioService::play_music/stop_music`, the `AudioBackend::start_music/stop_music` seam, the OU5AUDIO pack reader and its capability record — with **zero call sites**: `grep` found `play_music`/`music_context_for_location` used nowhere outside `audio.{h,cpp}` and their own tests. `TdeckAudioBackend::start_music` was `{ return false; }` (A3-04 was its own TODO comment). `alpha_audio.cpp`'s loader read and validated the whole pack and then freed it, with a comment naming exactly this batch as the one that would keep the bytes.
- The real local audio pack already exists (git-ignored, from an earlier `npm run pack:audio`): `native/assets/openu5-audio.bin`, 56,148 B, state Valid, capability Supported, 16/16 songs valid, bank valid. It was used throughout this batch's host tests and never touched or regenerated.

### 17.2 The patch's music format (Phase B) — primary evidence: the files themselves, plus the reference already derived from them

This batch's real head start: the project's own browser-side "reference" port (`game/src/ui/opl/{bank,voices,chip,sequencer}.ts`, `game/src/ui/music.ts`, `extractor/src/audio/xmi2midi.ts`) had **already done the derivation work** for the music patch — the same relationship the gameplay engine has to its TypeScript oracle, just for audio. `re/notes/music-location-mapping.md` records how: byte-level measurement of the 16 XMI files and `FAT.OPL`, corpus-wide statistics (event counts, channel usage, controller usage, simultaneous-voice peaks), and the driver's own selector disassembled from `mid.drv`. None of that needed re-deriving; it needed **porting and re-verifying against the actual patch files**, which is what §17.3 (parse) and §17.14 (host tests against the real corpus) do.

**XMI (IFF), measured on the real 16 songs:**
- `FORM:XDIR{INFO(u16=1)} CAT:XMID{FORM:XMID{[TIMB] EVNT}}` — one sequence per file, always.
- `EVNT` ticks are **fixed at 120/second**. Bytes < 0x80 accumulate as delay; a Note On (`0x9n`) carries its **duration as a trailing VLQ** and is the only source of note-offs — the corpus contains **zero literal `0x8n` events** (confirmed again on this batch's own parse of all 16 songs, `a3_04_music_synth` test suite, §17.14).
- Controllers seen: 1, 7, 10, 11, 32, 64, 91, 93, 119, 121; of those, 7/10/11/64/121 change anything an OPL2 can represent (the rest are accepted and ignored, on purpose).
- 103 pitch-bend events across the corpus; peak of **17 simultaneous notes** (an OPL2 has 9 voices — voice stealing is the *normal* case for parts of this corpus, not an edge case).
- Durations: **11.9 s (Reunion) … 144.8 s (Stones)** — reproduced exactly by this batch's own decode of the real files (§17.14 L4).
- No song carries an AIL loop controller (CC 116/117): the patch's own "restart on the next key poll" behaviour (§5) is the only loop semantic that exists.

**`FAT.OPL` (Miles AIL Global Timbre Library), measured:** an index of 6-byte records (`u8 patch, u8 bank, u32 offset`) terminated by `u16 0xFFFF`, each offset pointing at a 14-byte two-operator block (`u16 size=14, u8 fixedNote, 5B modulator, 1B 0xC0, 5B carrier`). 181 timbres = 128 melodic (GM 0–127) + 53 percussion (GM drum notes 35–87). The corpus never uses MIDI channel 9, so none of the percussion timbres is ever struck — they are still parsed and playable, because leaving a hole a byte-correct reader would otherwise fill is a defect, not a shortcut (`bank.ts`'s own stated reason, ported verbatim; `music_synth.cpp` `MilesOplBank::load`).

**This batch's audit against the files, done fresh (not assumed from the reference's notes):** `native/core/tools/a3_04_music_probe.cpp` (an ad hoc dev tool, not a ctest target) decoded all 16 real songs end to end and printed event counts, durations, peak and RMS. Every duration matched the documented range exactly; no song clipped; every song produced audible, non-trivial PCM. The formal proof of the same claims is `a3_04_music_synth`'s L-series (§17.14), which is what actually gates the suite.

### 17.3 Decoder/synth architecture (Phase C)

**Decision: port the reference's OPL2/OPL3 emulator, voice allocator and bank reader almost line for line into the portable core (`native/core/include/openu5/music_synth.h`, `src/music_synth.cpp`), the same file pair pattern as `sfx_synth.{h,cpp}`.** Host and device compile the identical implementation (`sources.cmake` lists it once, for both).

**Provenance and license, audited before porting anything:** `game/src/ui/opl/chip.ts`'s own header is explicit and this batch re-checked it against the code, not just the comment: it is an **original emulator written from the OPL's documented, publicly known behaviour** (the attenuation domain, envelope-phase state machine, waveform-table shapes) — **not** a port of DBOPL, Nuked-OPL, or any other third-party emulator. Its two ROM tables (`LOG_SIN`, `EXP`) are *generated* from closed-form formulas with checkable anchors (`LOG_SIN[0]=2137`, `LOG_SIN[255]=0`, `EXP[0]=0`), not copied data. One named influence — `MOD_SCALE`'s ratio, credited to DBOPL in a comment — is a single tuning constant affecting brightness only, not code. **Conclusion: no third-party code is reused, so no third-party license applies; this is the project's own license, same as every other file.** Nothing was "ported and hoped": every anchor the reference documents was re-verified in the C++ port (`a3_04_music_synth` C1–C4).

**One deliberate architectural departure from the reference, made and verified, not assumed:** the reference goes XMI → Standard MIDI File (`extractor/src/audio/xmi2midi.ts`) → re-parse (`opl/sequencer.ts parseSmf`). This port **skips the MIDI round trip**: `parse_xmi_events()` walks the XMI `EVNT` chunk directly into a flat, time-sorted event list. This changes nothing observable, because `xmi2midi.ts` always bakes in a **fixed** tempo (500,000 µs/quarter, division 60 = 120 XMI ticks/second) and explicitly ignores every tempo meta in the source ("as xmi2mid/wildmidi do", its own comment) — `parseSmf`/`eventSampleTimes` then read that fixed tempo straight back. Hardcoding `kXmiTicksPerSecond = 120` here is the identical number, reached without ever serializing to MIDI bytes and re-parsing them — one fewer allocation, one fewer format, and one fewer place a bug could hide. Proved, not asserted: `a3_04_music_synth` X1–X5 exercise the direct parser against hand-built synthetic XMI bytes (note-on/duration expansion, tempo-meta drop, EOT, the true-maximum-tick rule for `end_tick`), and L1–L6 run it against the real 16-song corpus.

**Layers**, each ported from one reference file, kept in the same order:

| Layer | Reference | Port | What it does |
|---|---|---|---|
| Bank | `bank.ts` | `MilesOplBank` | `FAT.OPL` bytes → 181 fixed-size `OplTimbre` records; a program the bank lacks falls back to melodic 0 (heard, not silenced) |
| Chip | `chip.ts` | `OplEmulator` | register writes → PCM, at the OPL's native 49,716 Hz; integer log-domain throughout except the final float mix + clamp (chip.ts is identical: JS numbers hold exact integers until the last step) |
| Voices | `voices.ts` | `OplVoiceAllocator` | MIDI-ish events → register writes, via a **sink callback** (no array/object per event — the same GC-avoidance reasoning the reference gives, ported to C++'s equivalent problem: allocation, not garbage-collection pauses) |
| Sequencer | `sequencer.ts` | `MusicSongPlayer` | one MIDI-ish track + one bank → continuous PCM at any output rate, with the reference's exact seamless-loop rule (§17.10) |
| XMI parse | `xmi2midi.ts`'s `EVNT`→MIDI half | `parse_xmi_events` | direct, per §17.3's departure |

**Device-only choice: OPL2 (9 voices), not the reference's default OPL3 (18).** The browser reference defaults to OPL3 *specifically to avoid the voice stealing* a real AdLib card would have produced on this corpus's 17-simultaneous-note passages. On the device this is inverted: **OPL2 is the CPU-appropriate choice (§17.4) and it is also the more period-accurate one** — a real 1988–2001 AdLib card was 9-voice OPL2, and the patch's own `Files.txt` targets "AdLib/SoundBlaster". Voice stealing under OPL2 is not a defect; it is what contemporary listeners actually heard. `OplChipKind` still supports OPL3 (host tests exercise both, C5/C6), so nothing was removed — only the device's default changed. This is recorded as a knowing choice, not a limitation to fix later.

**Ownership and the render() contract**, matching `sfx_synth.h`'s house style precisely: `MusicSongPlayer::render(int16_t*, frames, output_rate_hz, gain_q15)` applies the gain **once, saturating**, exactly like `SfxPlayer::render`. Internally, the chip always generates at its native 49,716 Hz into a small resident buffer (grown once, to the caller's first frame count, and never regrown for the same frame count — see §17.4's allocation proof), then linear-interpolated down to whatever rate the caller asked for (16 kHz on the device). `MusicSongPlayer` owns its `OplVoiceAllocator` via `std::unique_ptr`, rebuilt once per **song change** (not per frame): a one-time, off-the-audio-task-hot-path allocation, the same class of cost `parse_xmi_events` already pays once per song.

### 17.4 CPU / memory budget (Phase D) — estimated, hardware measurement is A3-05/A3-04's carried-forward hardware pass

No hardware is available this batch, so this section states the estimate, the reasoning, and exactly what A3-05 (or the deferred A3-04 hardware pass) must measure to confirm or correct it — per the brief's own instruction not to block host work on this.

- **The one real cost is the chip's own native rate, not the output rate.** `OplEmulator::generate()` always runs at 49,716 Hz internally, independent of the 16 kHz the device resamples down to (halving the output rate does not halve the chip's own per-second work — the OPL must be stepped at its real clock for correct pitch/envelope timing, the same constraint every OPL emulator has, including DOSBox's and the reference's own).
- **Per-sample cost, OPL2 (9 channels, 2 operators each):** per active channel, two `advance_envelope` calls (cheap: a modulo/shift compare, often an early return once `eg_state==Off`), a feedback shift, two `Operator::sample` calls (a waveform-table lookup + an `expo()` table lookup, both O(1)), and an add into the mix. **Rough order of magnitude: ~9 channels × ~2 operators × ~15–20 integer ops = 300–400 machine-level operations per chip sample**, before the compiler's own optimization. At 49,716 samples/second that is **15–20 million operations/second**, entirely on core 1's dedicated audio task (core 0 runs the game loop and input capture; nothing else runs on core 1 today, per A3-01 §2).
- **Xtensa LX7 @ 240 MHz, dual issue:** treating the estimate as 3–5 cycles per "operation" (integer table lookups and shifts, not single-cycle on this pipeline), that is roughly **20–40 % of one core's cycle budget** — a real, non-trivial load, but one with headroom: core 1 is otherwise idle, and the existing SFX synthesizer (A3-02) already shares that same task and budget without contention (§17.6's mixing design keeps them on one task, one I2S write per chunk — no new task, no new core claimed).
- **Floating point:** kept only where the reference's own math is genuinely fractional (the final per-sample mix scale and the resampling interpolation). The ESP32-S3 has a hardware single-precision FPU, so this is not a fixed-point-vs-float risk the way it would be on a plain ESP32 or an FPU-less MCU.
- **RAM, measured (not estimated) from this batch's own types:**
  - `MilesOplBank`: `sizeof(OplTimbre) × 256` fixed slots ≈ 4.5 KB, parsed once at boot from the retained pack payload, never reallocated.
  - `OplVoiceAllocator`: 16 channel-state structs + up to 18 voice structs, all fixed-size, no heap inside it — only the `unique_ptr` that OWNS it is heap, one allocation per song change.
  - `MusicTrack`: one `std::vector<MusicEvent>` (8 bytes/event), sized once at song-load. Measured on the real corpus: the largest song (Stones) parses to well under 3,000 events (≈24 KB); the smallest (Reunion) to under 300.
  - `MusicSongPlayer`'s chip-rate buffer: sized once to the caller's first `render()` frame count × the chip/output rate ratio (~3.1× at 16 kHz), then never regrown for a steady chunk size (proved by `a3_04_music_synth` M6: 200 `render()` calls at the device's own 256-frame chunk size allocate **zero** bytes after the first).
  - Total additional resident RAM for music, once a song is playing: **under 40 KB**, comfortably inside the T-Deck's PSRAM the audio pack payload (≤ 56 KB) is already read into (A3-01 §7).
  - Audio task stack: raised 4096 → **6144 B** (`tdeck_audio.h`) for the OPL synth's local state; still small next to the T-Deck's PSRAM/internal-RAM budget (A3-01 §2's numbers).
- **If the estimate is wrong (mitigation, not yet needed):** the simplest acceptable fallback, if OPL2 at 49,716 Hz turns out too expensive on core 1 alongside SFX, is capping active voices below 9 (the voice allocator's `pick_voice` already steals gracefully — capping is a one-line change, not a redesign) or lowering the chip's internal generation cadence for silent channels (`OplEmulator::is_silent()` already exists and is cheap to check per block). Neither is implemented now, because there is nothing to tune against without hardware; both are documented here so A3-05 does not have to rediscover them.

### 17.5 MusicContext → track map (Phase E)

The context table is **unchanged from A3-01** (§5's table, `audio.cpp`'s `kContexts`) — this batch adds no new contexts, per its own instruction not to invent ones the patch does not use. What A3-04 adds is the **two selector-range functions the table's scripted contexts need and A3-01 had not yet written**, ported from `game/src/ui/music.ts` `introPageContext`/`endgameSceneContext`:

| Function | Range | Context |
|---|---|---|
| `intro_page_music_context(page)` | 0x00–0x07 | `IntroStones` |
| | 0x08–0x0e | `IntroHalls` |
| | 0x0f–0x15 | `IntroGreyson` |
| | else | `Silence` |
| `endgame_scene_music_context(scene)` | 0x00–0x03 | `EndgameStones` |
| | 0x04–0x07 | `EndgameLadyNan` |
| | else | `Silence` |

Both are pure, exhaustively tested over every `uint8_t` value (`a3_04_music_synth` I1/I2), and both are killed by an off-by-one mutation each (M9/M10, §17.15). Their runtime hookup (`AlphaRuntime::sync_music()`'s frontend branch calling `intro_page_music_context`) is wired; the **endgame's own scene-by-scene stepping is not** (§17.20 — that is presentation/cinematic work outside this batch's scope, and the doc says so plainly rather than half-wiring it).

### 17.6 Music state machine (Phase F)

No new state was added. `AlphaRuntime::sync_music()` re-derives the context from **state that already exists** — the same principle the patch driver itself follows (it re-derives on every key poll rather than being told when to change), ported as a priority list rather than invented as a flag:

1. the Blackthorn capture/sacrifice cutscene (`blackthorn_pacer_.mounted()`) → **Silence** — its only scene is that one; ordinary palace visits never mount it, so this cannot misfire on the location-based `BlackthornPalace` context;
2. the Refuge dream (`narrative_pacer_` mounted, scene `Refuge`) → **Silence**;
3. Camp / hole-up (`camp_scene_active_`, or the pacer mounted with scene `Camp`/`TrollSneak`) → **Camp** (Stones), frozen — the location switch simply is not consulted while this is true, which is the entire "freeze" mechanic; no separate frozen flag exists or is needed;
4. the terminal Ending (`ui_->ending_active()`) → **Finale** (Rule Britannia) — a deliberate, documented simplification: the original's own Stones → Lady Nan → Reunion → Rule Britannia chain steps through `ENDGAME.OVL`'s scene table, which this port does not animate (D-54, explicitly out of scope for A3-04 — see §17.20). Finale is the correct **static** choice for "the quest is complete", not a guess at scenes the build cannot step through;
5. the shrine meditation screen (`ui_->base_mode()==ShrineSpecial`) → **Shrine** (Stones);
6. the frontend (`frontend_.active()`), by `FrontendState`: `CharacterCreation`→Creation, `IntroAnimation`→`intro_page_music_context(intro_frame_.scene)`, `EnterGame`→falls through to gameplay, everything else (Title/AttractDemo/MainMenu/NewJourney/Continue/Load/Settings/Credits/Error) → **Title**;
7. otherwise, gameplay: `music_context_for_location({location, floor, transport_tile, in_combat: base_mode()==Combat, combat_victory: combat_.victory})` — the driver's own switch, unchanged since A3-01.

**Deliberately not a case:** the System Menu, gem/zodiac views, and modal key-waits. These are overlays on whatever already plays; the original's own selector never touches music for them either, so `sync_music()` is simply never called from those early-return paths in `handle()` — silence by construction, not by an explicit "don't touch" branch.

**Fade/cut:** none, on purpose. The DOS driver switches instantly (§5: "There is no crossfade in DOS"); `AudioService::sync_music()` (unchanged since A3-01) stops the old song and starts the new one, no ramp. A short fade remains a possible A3-05 polish, never invented here.

### 17.7 Gameplay context hookup (Phase G)

`sync_music()` is called from exactly three places in `AlphaRuntime`, chosen to mirror the driver's own "every key poll" behaviour rather than adding new call sites for their own sake:

| Call site | Covers |
|---|---|
| `handle()`'s frontend branch, right before its `return true` | Title, Character Creation, Intro pages, and every frontend transition (New Journey, Continue, Load) that completes within one input |
| `handle()`'s System Menu branch, right after `service_system_menu_intent()` | System Menu Load (which can complete and close the menu within the SAME keystroke that opened the Load prompt, bypassing the function's final line) |
| `handle()`'s own final line (every ordinary gameplay input reaches this unless an earlier, deliberately silent branch returns first — §17.6) | movement/location changes, Camp, Alt+L / Alt+S, combat routing, the Ending sync that already runs earlier in the same call (`synchronize_after_debug`) |
| `configure_audio()`, once, at boot | starts the correct track immediately, with no key poll needed (§17.14 G1) |

No `GameEvent` was added. Every signal `sync_music()` reads (`game_.position`, `turn_.transport_tile`, `combat_.victory`, `ui_->base_mode()`/`ending_active()`, `frontend_.state()`, the scene pacers' `mounted()`/`scene()`) already existed before this batch.

### 17.8 Load / mode / restart safety (Phase H)

| Route | Result |
|---|---|
| Alt+L | the loaded position/floor/combat state is live by the time `sync_music()` runs at `handle()`'s end; the new context's song starts, the old one stops (no crossfade, §17.6) |
| System Menu Load | the same, via the dedicated call site in the System Menu branch (§17.7) — needed precisely because that branch otherwise returns before the function's final line |
| Title Continue / New Journey | `service_frontend_intent()` performs the load/reset, then `sync_music()` (in the same handle() call) derives the post-load context — Title's own Theme never lingers into gameplay |
| Return to Title | the frontend transition is processed and `sync_music()` runs before returning; Title's song starts, whatever was playing in-game (dungeon, combat, Camp) stops |
| Developer teleport | reaches the ordinary gameplay path's final `sync_music()` call, same as any other position change |
| Dungeon entry/exit | a location range change, handled by `music_context_for_location` exactly like any other location boundary |
| Combat entry/exit | `ui_->base_mode()` flips to/from `Combat`; the very next `sync_music()` call (the same input that started/ended combat) reflects it |
| Ending | `ending_active()` is already synced earlier in the SAME `handle()` call (`synchronize_after_debug`→`synchronize_ending`, both pre-existing), so `sync_music()` sees it consistently |

**No double-start:** `AudioService::play_music` (unchanged) de-dupes by **song**, not context — calling `sync_music()` on every poll costs nothing when nothing changed (`a3_04_music_runtime` G8). **No stale track after load, no queue leak:** the device backend's `apply_music_command` always calls `music_player_.stop()` before replacing `music_track_`, so the player never holds a pointer into a freed track, even for the span of one function call (§17.9).

### 17.9 SFX + music mixing (Phase I)

**Decision: one audio task, two independent players, summed and saturated.** `TdeckAudioBackend` (device-only; not host-tested — its logic is a thin, low-risk queue/mix loop over the already-proven `SfxPlayer` and `MusicSongPlayer`) now owns both:

- **Transport:** SFX keeps its existing 16-entry FreeRTOS queue (posted with a 0 timeout, unchanged). Music gets a **length-1** queue and `xQueueOverwrite` — the game thread only ever cares about the *latest* wanted song/stop, never a backlog of them, so "latest wins" is the correct policy and it never blocks or fails.
- **The audio task's loop:** each pass, drains the SFX queue (as before) and the music queue (new, non-blocking), applies any pending music command (`apply_music_command`: parses the new song's XMI once, off the game thread, replacing the resident `MusicTrack`), then renders one 256-frame (16 ms) chunk of **each** channel at its own live gain and sums them with a saturating add — exactly two independent hardware paths mixing in the air, which is what the patched original's MIDI card and PC speaker actually did (§9's existing "No ducking" contract, unchanged).
- **Idle behaviour, changed on purpose:** the task used to block indefinitely (`portMAX_DELAY`) on the SFX queue whenever SFX was idle. With continuous music, that would make a `start_music()` posted to the *separate* music queue invisible until something else woke the SFX wait. The task now blocks for a **bounded** 20 ms when both channels are silent, and polls the music queue every pass regardless — silence-to-music latency is at most 20 ms, imperceptible, and nothing spins.
- **No shared-volume bug:** SFX Volume and Music Volume remain two independent atomics (`sfx_gain_`, `music_gain_`), each applied inside its own player's `render()` call, exactly mirroring `SfxPlayer`'s existing contract. `a3_04_music_runtime` VOL1–VOL3 prove Music Volume 0/50/100 % reach the backend correctly (`gain_music`); A3-01's own S-series (unchanged) already proves SFX Volume never touches the music channel and vice versa, since `AudioService::set_sfx_volume`/`set_music_volume` were not modified.
- **Ownership safety:** `apply_music_command` always calls `music_player_.stop()` **before** replacing the resident `MusicTrack` (never after) — `MusicSongPlayer` holds a raw pointer into that track for as long as it is active, so stopping first removes the pointer before the object it points at is freed. There is no window, not even within a single function, where a dangling reference could be read.

### 17.10 Stock-asset behaviour (Phase J)

Unchanged from A3-01, and re-confirmed rather than assumed: `has_music()` is false for a stock pack, `AudioService::play_music`'s existing gate means the backend's `start_music`/`stop_music` are **never called at all**, regardless of how often `sync_music()` runs or how many contexts it passes through. `a3_04_music_runtime` STOCK1/STOCK2 drive a stock pack through location changes and a full Camp entry and assert **zero** `start_music`/`stop_music` calls. SFX is unaffected (a stock pack was already SFX-capable since A3-02; nothing here touches that path). Music Volume's row-unavailable behaviour is A3-01's `format_music_volume_row`, untouched.

### 17.11 Unknown / incomplete patch behaviour (Phase K)

Unchanged from A3-01's pack reader (`inspect_audio_pack`, `validate_xmi`, `validate_timbre_bank`) — this batch did not modify the validation rules, only added an optional `AudioPackPayload*` out-parameter so a caller that already trusts a Valid+Supported pack can get pointers to its song/bank bytes **from the same walk that validated them**, instead of re-scanning. An incomplete or unknown-variant pack is `Inconsistent`/`AudioPackInvalid` exactly as before: no music data is ever surfaced for it (the payload out-parameter is only filled when the pack is Valid and Supported — §`audio_pack.h`'s own contract), SFX is unaffected, and the reason string is unchanged.

### 17.12 Looping / end-of-track (Phase L)

Ported unchanged from `sequencer.ts`'s own measured rule (§5, §17.2): the patch restarts the same song on every key poll once it ends, with **no** AIL loop points in any of the 16 songs. `MusicSongPlayer::fill_chip`'s loop branch does the same thing the reference does and for the same measured reason — rebobinar (rewind) at the last event, **without** an `allNotesOff()`: the previous lap's still-releasing voices keep sounding into the new lap's attack (the voice allocator's own `pick_voice` already prefers unkeyed voices, so this self-resolves), which is the seamless splice the composed music actually has. A non-looping player (used only by the host tests to prove `ended()`; the device always loops) instead runs a fixed 3-second tail past the last event before reporting `ended()`, so a release is never cut mid-decay.

**Verified, not assumed:** `a3_04_music_synth` M4/M5 render a synthetic looping track in small, device-realistic chunks (not one giant call — §17.15's M5 mutation showed why that distinction matters) across 6 real seconds — six times the track's own 1-second length — and confirm it never reports `ended()` and keeps producing audible peaks in every subsequent lap. `a3_04_music_probe`'s manual run of the real 16-song corpus (§17.2) additionally confirms no duplicated first event and no audible discontinuity at the measured event-time boundaries.

### 17.13 Timing-safety result (Phase M)

- **No allocation in the per-sample path:** `OplEmulator::generate()` is source-scanned for `new`, `malloc`, `push_back`, `std::vector`, `.resize`, `make_unique` (N2/N3) and contains none. The ONE-TIME allocations this batch's design accepts (a `MusicTrack`'s event vector at song-load; a `MusicSongPlayer`'s chip-rate buffer, sized once to the caller's steady frame count) are proved to happen **exactly once per song**, never per frame: M6 runs 200 `render()` calls at a fixed 256-frame chunk after one warm-up call and asserts a global allocation counter stays at zero.
- **No RTOS primitive, no sleep, no blocking wait** in `music_synth.cpp` (N1, the same class of source scan as `audio.cpp`'s existing S20 and `sfx_synth.cpp`'s N1).
- **Determinism:** two fresh `MusicSongPlayer`s fed the identical track render byte-for-byte identical PCM (M1/M2) — a prerequisite for the real-corpus fingerprint (§17.14 L6) to mean anything across re-runs.
- **Music has no input path and holds no clock**, unchanged from A3-01's own claim (§10) — this batch added no field to `AudioService` that could contest it, and `sync_music()` only ever *reads* existing runtime state, never advances anything.
- **Re-run and still identical:** the existing paced-scene timing proofs (`a3_01_audio_runtime`'s T19/T20, the Camp/Blackthorn timelines under six audio setups) were re-run after every change in this batch (they are part of the full 133-test suite, §17.14) and remain green — nothing in this batch touches scene pacing, only the *music context* that plays alongside it.

### 17.14 Host / golden tests (Phase N)

Two new ctest targets, both required against the real local audio pack (the same convention A3-01/02/03 already established for `native/assets/openu5-audio.bin`):

| Target | Checks | Covers |
|---|---:|---|
| `a3_04_music_synth` | 67 | **B** (FAT.OPL reader, 8): melodic/percussion lookup, the melodic-0 fallback (proved against a bank ordered so the naive "return the first entry" bug cannot pass by coincidence), rejection of a truncated index and a non-2-operator block. **C** (chip ROM anchors, 8): `LOG_SIN`/`EXP`/`expo` anchors, OPL2 vs OPL3 channel counts, chip silence. **V** (voice allocator, 9): key-on/off register bits, A440→Block 4, velocity-0-as-note-off, 9-voice OPL2 saturation and the 10th note's steal, the sustain pedal holding then releasing, pitch bend rewriting F-Number, a drum note the bank lacks staying silent. **X** (direct XMI parse, 8): a synthetic well-formed XMI, note-on/delayed-note-off expansion, tempo-meta drop, the true-maximum-tick `end_tick` rule, rejection of a missing EVNT chunk / an unrecognised status byte / a truncated note-on. **M** (song player, 11): determinism, non-looping `ended()`, looping across 6 real seconds with no silence and no `ended()`, zero per-frame allocation, gain 0 = silence, `stop()`/post-stop silence. **I** (2): the two new context-range functions, exhaustive over every `uint8_t`. **N** (9): the non-blocking / no-hot-path-allocation source scans. **L** (6): the REAL 16-song corpus — parses, duration range, every song reaches `ended()`, every song is audible, none clips, and a combined PCM fingerprint is printed for future regression. |
| `a3_04_music_runtime` | 18 | The real `AlphaRuntime`, raw keys, a recording backend: boot plays the right track before any key poll; the location switch (Britannia/Underworld/City/Dungeon/Castle/Blackthorn/frigate) reaches the backend correctly; the same context never re-issues `start_music`, a real change always does exactly once; Camp freezes the location switch; the terminal Ending plays Rule Britannia; Music Volume 0/50/100 % reach `gain_music`; a stock pack issues zero `start_music`/`stop_music` calls across every scenario above; a source-scan proof that `sync_music()` reads the real combat base-mode and victory flag (the one branch no test here can safely reach without a live arena — `music_context_for_location`'s combat/victory arithmetic itself is already exhaustively proved by A3-01's M1–M3). |

**Full suite: 133/133 passed, serial, 118.72 s** (`native/core/a3-04-final-host-build.log`, `a3-04-finalctest.log`) — 131 (A3-03 baseline) + 2 new targets, zero regressions anywhere, including the pre-existing A3-01/02/03 audio suites (37/34/46 checks respectively, all still green after `alpha_runtime.cpp`'s `sync_music()` wiring).

### 17.15 Mutation results (Phase O)

`native/core/tools/a3_04_mutation_check.py` → `native/core/a3-04-mutation.log`: **13 one-line mutations of production code; all 13 killed, 0 survivors**, after two rounds of hardening the tests themselves (the honest process, kept rather than smoothed over):

- Two mutations (M1, an absent-program fallback; M8, an unrecognised status byte) initially **survived** because the test's own synthetic bank/byte happened to make the mutated and correct code produce the same result for that specific input — not a flaw in the mutation, a blind spot in the fixture. Both fixed by choosing a fixture that actually exercises the distinguishing code path (a bank ordered so timbres_[0] ≠ melodic-0; a status byte whose high nibble has no other handler).
- One mutation (M5, forcing the loop branch dead) survived a **first fix attempt** for a subtler reason: a single giant `render()` call synthesizes its entire request in one inner pass before ever re-checking the "has the track ended" condition, so the bug was invisible to a test that rendered 6 seconds in one call. Fixed by rendering in small, device-realistic chunks (matching how the real audio task actually calls it) — which is also a better test on its own merits, independent of this mutation.
- Two mutations (M3, M5) initially caused a **build failure** (a switch-case value out of range; an unused parameter) rather than exercising behaviour — not a real kill. Rewritten to mutate a value read at runtime instead of a declaration, so the kill is functional.

The other 9 killed cleanly on the first pass: the melodic-0 fallback (B4b), velocity-0-as-note-off (V3), the sustain pedal (V6/V7), `end_tick`'s true-maximum rule (X5, plus two real-corpus checks), the render() allocation-jitter regression (M6), OPL2/OPL3 channel counts swapped (C5/C6), the intro/endgame context-table boundaries (I1/I2), the frigate sentinel losing its signal (G7), the Camp freeze being skipped (CAMP1), and the victory flag being dropped from `sync_music()` (WIRE1, a source-scan kill — the honest limit of what a mutation test can prove without a live combat arena, stated rather than hidden).

### 17.16 Audio-pack changes (Phase P)

**None to the wire format.** `OU5AUDIO` stays version 1.0; the packer (`native/tools/u5pack/audio.ts`) is unchanged. The only change is on the **reading** side, and it is additive: `inspect_audio_pack()` gained an optional `AudioPackPayload*` out-parameter (default `nullptr`, so every existing call site is untouched) that returns pointers to the song/bank bytes it already found while validating — no second scan, no format change. `alpha_audio.cpp`'s `load_audio_pack_info` gained a matching optional `RetainedAudioPayload*` (again default `nullptr`) that keeps the pack's bytes resident (instead of freeing them, as before) **only** when the pack is Valid and Supported; every other outcome still frees everything, exactly as A3-01 designed it.

- Stock pack size: **112 B**, unchanged.
- Patched pack size: **56,148 B**, unchanged.
- Format version: **1.0**, unchanged — no regeneration needed for any existing pack on any card.
- Capability metadata: unchanged.
- **The user does not need to regenerate or re-copy `openu5-audio.bin`.** The existing file already carries everything this batch plays; only the firmware needed rebuilding.

### 17.17 Firmware build (Phase Q)

Fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-a3-04` (`native/targets/tdeck/a3-04-firmware-build.log`, `a3-04-firmware-retry.log`). The first `idf.py build` compiled every one of this batch's changed files clean — including `music_synth.cpp`'s full OPL2 synth for the xtensa target and `tdeck_audio.cpp`'s new mixing loop — then died inside the **bootloader subproject's own build**, no line of project code involved: the same class of transient ESP-IDF/toolchain flakiness the project has hit before (host-and-firmware-toolchain notes on fresh-build-dir crashes in third-party files). `idf.py reconfigure` + `ninja -j 4 all` (the documented workaround) completed clean on the retry.

- **Zero project warnings** under `-Werror` in both runs — the only warning lines in either log are the five stock `component_validation.cmake` notices every build emits (third-party, not project code).
- **Size:** `openu5_tdeck.bin` = **0xe6a90 = 945,808 B**, **+25,680 B against A3-03's 920,128 B**. **0x19570 = 103,280 B (10 %) free** in the 1 MiB app partition (down from A3-03's 12 %).
- **What grew:** the OPL2 emulator, voice allocator, bank reader and XMI parser (`music_synth.cpp`, new), `TdeckAudioBackend`'s music command queue and mixing loop (`tdeck_audio.cpp`), and the two new context-range functions (`audio.cpp`). Nothing in the game/resource packs changed (§17.16).
- **RAM/stack/CPU:** §17.4's estimates stand; the audio task's stack was raised 4096 → 6144 B in this build. Not measured on hardware this batch (no device available) — A3-05/the carried-forward hardware pass measures it for real.
- **Flash pressure:** materially changed (10 % free vs 12 %), but not critically — there is still comfortable headroom, and nothing in this batch's design (fixed-size bank/voice arrays, no new large tables) suggests future audio batches would consume flash at a similar rate.
- Not flashed. The post-commit image (built after the commit, in a fresh directory without ccache, per project convention) is what the annotated tag names, with its own SHA-256 and embedded `Git`.

### 17.18 Hardware test plan (Phase R) — for later, together with A3-03's carried-forward retest

The user is away from the device; nothing below has been run. Copy the Launcher image named in this batch's annotated tag onto the T-Deck's SD card structure as usual (`/sd/ultima5/…`), keep the existing `openu5-audio.bin` (§17.16: no regeneration needed).

**A. Patched assets, boot.** Boot with the existing supported `openu5-audio.bin`. The identity screen reads `FW 3.0.0-alpha3-dev-a3-04-debug` and the tag's `Git` hash; `AUDIO_PACK … capability=supported-music-patch` in the log. Settings shows `Music Volume: 80%`, adjustable.

**B. Title.** At the title screen (before New Journey / Continue), the Ultima V Theme should be audible.

**C. Overworld.** Journey Onward / Continue into Britannia: Britannic Lands plays. Descend to the Underworld (a moongate or a dungeon's lower reach): Worlds Below plays.

**D. Town.** Enter any City of Virtue: Villager Tarantella starts **once** on entry, and does not restart while walking around inside the same town.

**E. Dungeon.** Enter any dungeon: Halls of Doom plays.

**F. Combat.** Start a fight: Engagement and Melee plays. Win it: the Ultima V Theme plays (the driver's own victory switch, §5); leave the arena back to the prior location: that location's own track resumes (not a restart of the pre-combat track from its beginning, unless the driver's own dedupe-by-song rule says otherwise — see §17.8's "no double-start").

**G. SFX overlay.** While music plays, take a step (footstep SFX) and win/lose a fight (combat SFX): both the music and the SFX should be audible together, neither muting or replacing the other.

**H. Volume.** Music Volume 100 % → 50 % → 0 %: progressively quieter, then silent, while SFX Volume is left alone and SFX loudness is unaffected. Then the reverse: change SFX Volume and confirm the music's loudness is unaffected.

**I. Load.** Save in one context (e.g. a dungeon), walk into a different one (e.g. overworld), then load the save: the overworld track stops and the dungeon track resumes — not a restart of the overworld track, not both playing at once.

**J. Stock pack (optional).** Boot with a stock (unpatched) `openu5-audio.bin`, or none at all: Settings reads `Music Volume: Unavailable` with the documented reason; no music plays anywhere; SFX is unaffected.

**K. Carried forward from A3-03 (§16.17), to run in the same session:** the fountain's proximity crackle, the troll-fight victory fanfare, the Word of Power quake rumble, and Alt+L's load flush. A3-03's software is unchanged by this batch; only its hardware confirmation is still outstanding, for the same reason as A3-04's (§0's header).

### 17.19 Documentation (Phase S)

This file (`ALPHA3_AUDIO.md` §17, and the header/status line at the top). The audit ledger entry (batch record) is this report itself; A3-03's hardware line is recorded as **SOFTWARE COMPLETE — HARDWARE RETEST PENDING**, not re-run, because the user is away from the device.

### 17.20 Next batch (Phase T) — not started

*(A3-04A was inserted before it after the first hardware run showed stutter — §18.)* **A3-05 — hardware music validation, loudness/tone balance, SFX/music balance, final audio polish and sign-off.** At minimum: run §17.18's plan together with A3-03's carried-forward retest; measure the OPL2 synth's actual CPU cost on-device against §17.4's estimate and adjust the mitigation there only if the measurement says to; tune absolute loudness and the SFX/music balance; decide whether a short crossfade is worth adding (§17.6 leaves the DOS-exact instant switch as the default); the endgame's own scene-by-scene music chain (Stones → Lady Nan → Reunion → Rule Britannia) waits on the endgame cinematic itself (D-54), which is explicitly not this batch's or A3-05's scope — a presentation batch, not an audio one.

## 18. A3-04A — the music stutter: real-time audio performance

**Hardware observation that opened this batch** (the A3-04 image, `FW 3.0.0-alpha3-dev-a3-04-debug`, `Git db6aa437dc56`, Launcher SHA-256 `ee3bc904…5c6c`, flashed to the T-Deck Plus by the user): music plays, the right songs are recognisable, asset detection works — **but playback is extremely stuttery.** Classified at the start as *music logic: functional; real-time playback: FAIL*.

**Status: PLAYBACK FUNCTIONAL — SMOOTHNESS RETEST PENDING.** The fix below is software-complete and host-validated; the retest image exists (§18.17). Nothing here is a hardware measurement yet: every device number in this section is either a deterministic count or static analysis of the linked image, and says so. The retest's Developer → Diagnostics → **Audio performance** run (§18.14) measures the real ones — including the root cause itself.

### 18.1 Baseline (Phase A)

- HEAD `f6d611cf` (A3-04's packaging-record commit); tag `alpha3-a3-04-music-playback` = `db6aa437`, the image on the device. Tree clean.
- Fresh host build and **serial ctest: 133/133 passed in 120.27 s**, the one known w64devkit `stl_uninitialized.h` warning only (`native/core/a3-04a-baseline-{configure,build,ctest}.log`).
- A3-04's audio configuration, read from the source: 16 kHz mono 16-bit I2S; **4 DMA descriptors × 256 frames** (16 ms each, 64 ms total); the driver's free-descriptor queue holds 3; `auto_clear_after_cb`; one audio task, **priority 3, pinned to core 1, 6,144 B stack**; SFX command queue 16, music command queue 1 (overwrite); 256-frame render chunk; the synth compiled at the project-wide **`-Og`**; `CONFIG_FREERTOS_HZ=100`; task watchdog 5 s, idle tasks of both cores watched, **log only (no panic)**.

### 18.2 The device audio pipeline, traced (Phase B)

| Stage | Context / core / priority | Blocking | Chunk | Buffers | Allocation | Sync |
|---|---|---|---|---|---|---|
| `AudioService` → `play_sfx` / `start_music` / `set_gain` | game thread, core 0, main task prio 1 | never (zero-timeout send, `xQueueOverwrite`, atomics) | one request | FreeRTOS queues | none | queues, atomics |
| Command intake | audio task, core 1, prio 3 | ≤ 20 ms queue wait only while silent | — | — | **A3-04: an XMI parse per song switch** (vector + stable sort) | queues |
| XMI sequencer (`fill_chip`: event dispatch → voice allocator → register writes) | audio task | no | per event | player-owned | none | — |
| OPL2 synth (`OplEmulator::generate`) | audio task | no | **~795 chip samples per 16 ms block, at 49,716 Hz** | chip-rate float buffer | none after the first render | **A3-04: a mutex per table read (§18.3)** |
| Resample to 16 kHz, gain, SFX render, saturating mix | audio task | no | 256 frames | three 512 B arrays **on the task stack** | none | — |
| `i2s_channel_write` | audio task | waits for a finished descriptor (200 ms timeout) | 512 B = one descriptor | driver-owned DMA buffers | none | driver queue |
| DMA → I2S → amplifier | hardware; EOF interrupt on **core 0** (allocated where `ensure_started()` ran) | never waits: an unrefilled descriptor plays the silence auto-clear left | 16 ms | 4 descriptors | — | — |

**Where PCM is produced relative to when I2S needs it:** the write only returns once the DMA has freed a descriptor, so a steady stream renders each block right after a descriptor frees and hands it over while the other three (48 ms) play — render-ahead was already up to a full 64 ms ring. **The ring was not too shallow; it was never refilled fast enough** (§18.3).

Two A3-04 sequencing defects found on the way, independent of the stutter (both fixed, §18.8): the channel was **enabled before any audio was written** (`i2s_channel_enable` restarts the DMA at descriptor 0 and plays whatever each descriptor still holds — 64 ms of silence, or the unplayed tail of the previous sound), and the idle drain wrote **two** silent blocks into a **four**-deep ring before `i2s_channel_disable`, cutting the last ~32 ms of every sound (which the next enable then replayed).

### 18.3 Root cause: a FreeRTOS mutex on every synth table read

`music_synth.cpp` read its two ROM tables (`LOG_SIN`, `EXP`) through `tables()`, a **function-local `static const OplTables t`**. The firmware is compiled with **`-mdisable-hardware-atomics`**, so GCC cannot emit the inline acquire-load that normally tests "already constructed?" (its `is_atomic_expensive_p` path): the guard condition becomes constant and **`__cxa_guard_acquire` is called on every access**. ESP-IDF's `components/cxx/cxx_guards.cpp` implements it as `xTaskGetSchedulerState()` + **`xSemaphoreTake` + `xSemaphoreGive` on one global mutex** — its own comment assumes "the compiler must generate code to check if the first byte … is non-zero", which this toolchain configuration does not.

- **Where, proved on the linked image** (`native/targets/tdeck/a3_04a_hotpath_check.py` → `a3-04a-hotpath-a3-04-image.log`): `generate → Operator::sample → wave_atten / expo → tables → __cxa_guard_acquire`. It is the **only** hot-path guard in the whole A3-04 firmware (7 call sites in all; the other 6 are one-time statics in `app_main`). At `-O2` it would still be there: a scratch `-O2` build of the A3-04 source still emits 10 guard calls.
- **How often, measured on the real corpus** (deterministic host counts, instrumented scratch copy of the synth; §18.4): `tables()` runs twice per sounding operator per chip sample. On the patch's songs **all 9 OPL2 channels sound almost all the time** (releases are long), so a 16 ms block makes **14,400–27,000 guarded reads on average and up to 28,656** (9 channels × 2 operators × 2 reads × 796 chip samples) — **about 1.7 million mutex round trips a second** for the Ultima V Theme.
- **Why that breaks real time:** even at an optimistic 0.5 µs per round trip (two spinlocked critical sections, flash-resident code), the guard alone is 13.5 ms of every 16 ms block, before the synth does any work; at 1–2 µs it is 27–54 ms. The producer ran well below real time, so the DMA played each (correct) block and then silence — *recognisable but broken up*, exactly the report. **Hardware confirmation:** the retest's benchmark times 4,000 guarded reads against 4,000 plain ones on the device and prints `A3-04 guard N ns/read = X ms/8 ms blk` for the channels it just measured (§18.14).
- **Why no host test could see it:** on x86 the same source compiles to an inline byte test; the host suite ran the A3-04 synth at ~300 ns per output sample.
- **A second symptom it predicts:** a producer that never catches up never blocks in `i2s_channel_write`, so A3-04's priority-3 audio task never slept and **IDLE1 never ran** — its serial log should show `task_wdt: … IDLE1 (CPU 1)` every 5 s while music played (the watchdog here logs, it does not reset).

**The fix** (`music_synth.cpp`): the tables are a **namespace-scope object** built by the startup constructors; `tables()` returns it. No guard exists any more — the image check is GREEN (`a3-04a-hotpath-a3-04a-image.log`: `generate()` now reaches three functions, none of them a guard, lock, allocation, log or delay). **Synth output is bit-identical:** the real-corpus PCM fingerprint is `6a7ff3d5727df2c6` before and after (`a3_04_music_synth` L6).

### 18.4 Render timing (Phase C/D evidence available without the device)

Real-corpus workload per 16 ms block (the synth at 256 frames, `a3_04a` scratch probe; counts, not times):

| Song | Events | Avg sounding OPL channels | Guarded reads per block, avg / max |
|---|---:|---:|---:|
| Ultima V Theme | 3,178 | 8.76 | 27,010 / 28,656 |
| Rule Britannia | 966 | 8.53 | 26,448 / 28,656 |
| Lord Blackthorn | 1,796 | 8.60 | 25,966 / 28,656 |
| Stones | 1,536 | 8.93 | 22,737 / 28,277 |
| Halls of Doom (lightest) | 572 | 4.72 | 14,422 / 28,656 |

The **Theme** is the worst case (most sounding operators on average, densest event bursts — up to 69 events in one block) and is the benchmark's track. The whole corpus is 21,529 events.

**Chunk size does not change synth cost** (host, `-O2`, 60 s of the Theme, best of 5): 64 / 128 / 256 / 512 / 1024-frame renders cost 0.996 / 0.999 / 1.000 / 1.002 / 1.001 × the 256-frame cost per sample — the work is per chip sample, the per-call overhead is noise. Output differs between chunk sizes in 1–4 of 960,000 samples by exactly 1 LSB (the resampler's `double` position rounds differently when blocks split elsewhere). So block size is purely a latency/tolerance choice (§18.7).

Static per-sample path in the linked image: A3-04's `generate()` made **8 out-of-line calls per channel** (advance_envelope ×2, phase_inc ×2, sample ×2 → wave_atten → tables → guard, expo → tables → guard). A3-04A's `-O2` synth inlines `phase_inc`, `wave_atten`, `expo` and `tables`; `generate()` calls only `advance_envelope`, `sample` and `advance_clocks`.

### 18.5 Instrumentation (Phase C)

`openu5::AudioPerfCounters` (`audio_stream.h`) — accumulated by the audio task, never logged from it; the task publishes a copy every 16 blocks (128 ms) under a spinlock, and the game thread reads that copy. One window from the last reset:

| Counter | Meaning |
|---|---|
| render min / avg / max / p95 / p99 | one block: music synth + SFX synth + mix (histogram, 1 % of a block per bucket, up to 2 blocks) |
| music avg / max | the music synth alone |
| block duration | 8,000 µs (§18.7) |
| sched (period) avg / max | interval between two consecutive deliveries — the audio task's own schedule |
| write avg / max | `i2s_channel_write`: the copy plus the wait for a free descriptor (the wait is the healthy case) |
| underruns (writer) | writes that found the ring already empty (one per dry episode) |
| hw underruns (driver) | the driver's `on_send_q_ovf`: descriptors the DMA played with nothing new in them (one per silent block) |
| fill min / max / avg | blocks not yet finished playing, sampled just before each write: 8 = full, 0 = dry; "min buffered" = fill min × 8 ms |
| missed deadlines | blocks that took longer to render than they last |
| voices max / avg, OPL channels max / avg | MIDI voices keyed or held; OPL channels still sounding (the cost driver) |
| CPU | render time over the window |
| stack min | the audio task's stack high-water mark |
| SFX submitted / with music / queue max / pending max | cue traffic while music plays |
| runaway yields, write/enable failures, music switches | §18.12 |

Heap activity: the pump allocates nothing (host-proved, §18.13); the benchmark reports the internal/PSRAM heap change over its run.

### 18.6 OPL performance: `-O2` for `music_synth.cpp` only (Phase J)

Removing the guard leaves the synth at the project's `-Og`, where every operator step is an out-of-line call chain; with all 9 channels sounding (§18.4) that is the difference between comfortable headroom and running close to real time. `main/CMakeLists.txt` now builds **this one file** with `-O2 -ffp-contract=off` (verified last on its command line, after the project's `-Og`). `-O2` enables no fast-math and `-ffp-contract=off` forbids fused multiply-add, so the float arithmetic is the same IEEE sequence as at `-Og`: **what plays is unchanged.** No math was rewritten, no voice capped, no waveform simplified, no rate lowered.

**Not changed, measured and left for evidence:** the resampler's per-output-sample `double` arithmetic is software floating point on the ESP32-S3 (single-precision FPU only) — about 8 soft-double operations per output sample, a few percent of a core by static count. Changing it would move output bits; the benchmark's `music max` / `CPU` will say whether A3-05 needs to.

**Sample rate (Phase K):** not evaluated — 16 kHz stays. Neither precondition held: the stall was a throughput bug in the synth's glue, not the synth's inherent cost.

### 18.7 Buffering architecture and DMA configuration (Phases F, G, H)

**The DMA descriptor ring is the render-ahead PCM ring**, and the pump keeps it full: producer = the audio task, consumer = the I2S DMA, one descriptor = one block. A second software ring in front of it cannot raise stall tolerance (a stalled core cannot move PCM from a software ring into DMA either) and would only add SFX latency, so none was added. What the batch asked a ring buffer to give — render-ahead, stall tolerance, measured fill, underrun detection, bounded switch/volume latency — is now provided and measured on this ring (§18.5, §18.13).

| | A3-04 | A3-04A |
|---|---|---|
| Descriptors × frames | 4 × 256 (16 ms) | **8 × 128 (8 ms)** |
| Audio ahead of the speaker | 64 ms | **64 ms** (unchanged) |
| DMA buffer memory | 2,048 B | **2,048 B** (unchanged; +4 descriptors ≈ +48 B) |
| Stall tolerance at 50 % synth load (whole ring − one block's render) | ~56 ms | **~60 ms** (host-measured: 59 ms) |
| New SFX / music heard after | ≤ 64 + 16 ms | **≤ 64 + 8 ms** (host-measured switch: 64.0 ms) |
| DMA interrupts / task wake-ups | 62.5 / s | 125 / s (the driver's short EOF ISR, on core 0) |

Why 128-frame blocks: at the **same latency and memory** as the hardware-proven A3-02..A3-04 ring, halving the block halves one block's render time (tolerance + ~4 ms) and halves the command pickup delay (−8 ms worst case). Chunk size costs nothing in synth throughput (§18.4). Writes are one full descriptor each (256 B every 8 ms) — not tiny, not excessive. The driver buffers exactly the ring; nothing else sits between the write and the speaker.

**Designed tolerance: one stall of up to ≈ 64 ms − one block's render** (59 ms at 50 % load, host-measured by sweeping the stall 1 ms at a time; R6). Recovery after a longer stall is automatic and exact (R5: 100 ms → 6 silent blocks, detected by both counters, then the same blocks in the same order). No other application task runs on core 1 (the IDF IPC task only runs briefly on request), no code in `main/` writes internal flash at run time (which would stall both cores' caches), and A3-02's SFX on the same 64 ms ring passed on hardware with no dropouts reported (§15.14) — so tens of milliseconds of margin is expected to be plenty; the retest's `sched max` / `min buffered` will say. If it is not, `kAudioRingBlocks` is one constant (and latency grows with it).

### 18.8 Ring sequencing (the pump's state machine)

`openu5::AudioRingPump` (`audio_stream.{h,cpp}`) — **production code, driven identically by the device task and the host tests**:

- **Off** — nothing to play, channel off; the task blocks on its queue (≤ 20 ms).
- **Priming** — channel off; render `kAudioRingBlocks` blocks back to back and `i2s_channel_preload_data` every descriptor, **then** enable: playback starts with a full ring of fresh audio (no 64 ms lead-in, no stale replay). Start latency is eight renders back to back (cheap for SFX alone; with music playing the channel is normally already running).
- **Running** — per step: render one block, write it (the write waiting for the DMA is what paces the task). After **one whole ring of silence** in a row the last real block has provably played (the write of block *j* returns only when block *j* − 8 has finished), so the channel turns off without cutting anything.

Host proof against A3-04's own sequencing (mutation **L1** re-creates it inside the pump: enable first, two-block drain) — `a3-04a-mutation.log`: R1 (no lead-in), R2 (stale replay), R3 (cut tail), R4 (dry blocks) go RED.

### 18.9 Hot-path audit (Phase I)

| Item on the audio task | A3-04 | A3-04A |
|---|---|---|
| Mutex per synth table read | yes (§18.3) | **none** (image check GREEN) |
| XMI parse at a song switch | parse + vector + stable sort on the audio task | **none**: `MusicLibrary` parses all 16 songs and the bank **once at boot** (21,529 events, ~172 KB, PSRAM; the boot log prints `songs=16/16 events=21529 parse_us=…`) |
| Heap at a song switch | `unique_ptr<OplVoiceAllocator>` + the parse | **none**: the allocator is held in place (`std::optional`); host-proved zero allocations over 3,000 blocks with four song starts (R16) |
| Stack | `run()` 1,600 B (three blocks on the stack) + `MusicSongPlayer::start()` 1,680 B (an `OplEmulator` temporary) | `run()` 64 B, `start()` 48 B: blocks are pump members, the chip is rebuilt in place (`-fstack-usage`) |
| Logging | error paths only | none in `run()` (host scan S4); counters are published, the game thread logs them |
| File / SD access, string work, format conversion, context detection | none | none |
| Locks | the guard mutex | one spinlock copy per 128 ms (publishing the window) |

### 18.10 SFX + music (Phase L)

Unchanged in policy: one block, both players, each at its own live gain, **summed and saturated** — no ducking, no pausing music for SFX, one write per block. Proved against an independent oracle (R17: 600 blocks of the Theme with cues equal `MusicSongPlayer` + `SfxPlayer` rendered directly and summed) and channel by channel (R12: SFX Volume 0 leaves the music bit-identical; Music Volume 0 leaves the SFX alone). SFX render cost is small next to the synth (one PWM voice at 16 kHz); the benchmark's second phase adds a cue every 100 ms on top of the worst-case song.

### 18.11 Priority / core affinity (Phase M)

Audited, **not changed** — no evidence of starvation: audio task priority 3 on core 1 (the only application task there; `openu5-input` is priority 4 on core 0, the game loop is the main task on core 0, the SD-log writer is idle priority, the esp_timer task is on core 0). The I2S EOF interrupt is allocated on core 0 (where `ensure_started()` first runs); the DMA keeps playing through any interrupt latency shorter than the ring, and the driver-side `on_send_q_ovf` counter would expose anything longer. If the retest shows `sched max` spikes, moving the channel's bring-up onto the audio task (interrupt on core 1) is the next lever.

### 18.12 Scheduler / watchdog safety (Phase N)

- The task blocks every block in `i2s_channel_write` while running and in a bounded queue wait while silent, so IDLE1 runs.
- **Runaway guard:** a write issued while the ring still has a free descriptor cannot wait for the DMA; after `kRunawayWrites` (32) such writes in a row the producer is slower than real time, and the pump returns "yield" — the task sleeps one tick so IDLE1 (and the task watchdog) still run. A3-04's synth lived in exactly that state (§18.3). Host proof R9: a producer at 150 % of real time underruns continuously, both detectors count it, and the task yields 12 times in 400 blocks.
- Nothing on the game thread waits for audio; the perf window is a spinlock copy of ~200 B.
- A failed enable leaves the path silent and off and yields (R14).

### 18.13 Host tests and RED/GREEN (Phases O, Q)

New target **`a3_04a_audio_stream`** (`native/core/tests/a3_04a_audio_stream_test.cpp`, **58 checks**). It drives the production pump against **`VirtualI2sRing`, a model of the ESP-IDF 6.1 `i2s_std` TX path written from the driver source** (`i2s_common.c`: free-descriptor queue of `desc_num − 1`, drop-oldest + `on_send_q_ovf` on overflow, auto-clear after the callback, preload semantics, enable from descriptor 0 with whatever the buffers hold, disable keeping contents), with ground truth the driver cannot see (which block every descriptor held when it played). Producer timing is modelled (render time + injected stalls); nothing asserts a desktop's speed.

- **H** (7): no function-local static in any file the audio task runs; `tables()` guard-free; the firmware's per-file `-O2 -ffp-contract=off`; `audio_stream.cpp` free of RTOS calls, locks, logs and allocation.
- **L** (5): the library parses all 16 songs + the 181-timbre bank once; each track equals a direct parse; 21,529 events; null/bankless payloads are silent.
- **P** (7): the counters' arithmetic (min/avg/max, p95/p99, missed deadlines, fill/write/period, CPU, reset) and the legacy-guard estimate.
- **R** (29): prime-then-enable, no stale replay, drain-before-off, zero underruns at 50 % load, exact playback order; **stalls of 5 / 10 / 20 / 40 ms: no underrun and the stream is the reference block for block; 100 ms: underrun detected by both counters and exact recovery**; tolerance sweep (59 ms); 30 random 0–40 ms stalls + render jitter through music, SFX and a switch: no underrun; a 3-descriptor ring *does* underrun at 40 ms; a 150 % producer underruns, is detected and yields; switch → the new song's first block next, audible within 64 ms, nothing of the old song through the player; volume per block, never retroactive; SFX overlay = saturating sum; stop → exactly one ring of silence → off → sleeping; enable failure; flush epoch; the oracle (R17); **zero allocations over 3,000 blocks with SFX and four song starts**.
- **B** (6): the benchmark timeline and its report lines. **S** (4): the device backend runs this pump, with this DMA geometry, the driver callbacks registered, and no parse/log in its loop.

**RED first.** (1) The new test pointed at A3-04's sources (`git show HEAD:` copies) goes **7 RED**: H1 names the exact line `static const OplTables t;`, H2, H3 (no `-O2`), S1–S4 (`native/core/a3-04a-red-scan-against-a3-04.log`). (2) The image check is **RED on the A3-04 ELF** (the guard chain) and **GREEN on the A3-04A ELF**. (3) Mutation L1 reproduces A3-04's ring sequencing and goes RED (§18.8). (4) R9 is the throughput failure itself, reproduced in the model: a producer slower than real time underruns without end. The *subjective* "sounds bad" is not used anywhere as a RED.

### 18.14 The Developer diagnostics (Phases D, E, P)

Developer (Alt+D) → Diagnostics, two new rows just above **Audio test tone (SFX)**, which stays last:

- **Audio performance** — the benchmark. Measures A3-04's guard on this device (4,000 guarded vs 4,000 plain reads, ~10 ms), notes the free heap, forces the **Ultima V Theme** (the worst case, §18.4), then: 2 s settle → **30 s music alone** (the idle test: no SFX, no movement; `sync_music()` is held off so nothing changes the song) → **15 s music + one cue every 100 ms** (step / hit / bump / heavy hit) → restores the game's own music. It never waits: `render()` advances it every frame, in any mode. Output, in the transcript and as `AUDIO_PERF_REPORT` log lines:

  ```
  Audio perf, music alone (idle):
  Ultima V Theme 29.9s
  missed 0  underrun 0  hw 0
  render avg X p99 X max X
  block 8.0ms  music max X  CPU X%
  sched max X  min buffered X ms
  voices max X  OPL ch avg X max X
  SFX 0 (0 w/music) q0 p0
  stack min X B  runaway 0  fail 0
  Audio perf, music + SFX:
  … (same eight lines)
  A3-04 guard N ns/read = X ms/8 ms blk
  heap change: internal X B, PSRAM X B
  ```
- **Audio stats (live)** — the window since the last read (then a new one starts): for the load matrix, read it after walking, fighting, opening menus. The 5 s heartbeat also logs the running window as one `AUDIO_PERF …` line (serial / SD log).

Load matrix (Phase E) — how each case is measured on the retest: **1** music only = the benchmark's first phase; **5** rapid SFX = its second phase; **2** movement, **3** footsteps, **4** combat SFX, **6** menus/rendering = *Audio stats (live)* read right after doing each for ~20 s. Every row records missed / underruns / min buffered / render max / sched max / CPU.

### 18.15 Timing and gameplay regression (Phase R)

The device-side change sits entirely behind the unchanged `AudioBackend` seam: the game thread's calls are the same zero-timeout posts, and nothing in the pump reads game state or a game clock. The existing timing proofs — Camp, Blackthorn, Refuge, the quake, shrine and combat-victory timelines under the none / synth / muted / broken / stalled / racing backends (`a3_01_audio_runtime` T19–T21, `a3_02_sfx_runtime`, `a3_03_sfx_runtime`, `batch51_*` pacing) — all pass unchanged in the full suite (§18.16), as does `a3_04_music_runtime` (track selection, switches, Camp freeze, Ending, volume, stock pack). Audio failure stays presentation-only: a failed enable is silence (R14), the game never learns of it.

### 18.16 Audio regression, mutations and the full suite (Phases S, T)

Audio regression, all green in the full suite: stock pack — SFX yes, music never reaches the backend (`a3_04_music_runtime` STOCK1/2); patched pack — every song plays (library 16/16, L1); unknown/incomplete pack — no music data surfaced (A3-01 P-series, unchanged); Music Volume and SFX Volume independent (R11, R12, A3-01 S-series); SFX over music (R12, R17); transitions and loops (R10, `a3_04_music_synth` M4/M5); load/Return-to-Title flush (R15, A3-02 M4/M5). The Developer menu grew two rows (`ui_debug_menu_test` U2 updated: 17 → 19 Diagnostics rows; the test tone stays last, so every runtime test that reaches it is unchanged).

**Mutations** — `native/core/tools/a3_04a_mutation_check.py` → `native/core/a3-04a-mutation.log`. **22 mutations, 22 killed, 0 survived — every one by a failing check** (the first pass had one build failure and one test crash counted as kills: M8 did not compile, and M3 crashed the test on an unguarded cross-run comparison; M8 was rewritten to use its parameter, R10/R11 were bounds-checked, and the whole set re-run):

| # | Mutation | Killed by |
|---|---|---|
| M1 | render-ahead off: enable after one preloaded block | R1, R4 |
| M2 | ring too shallow: 3 descriptors | R5 (20, 40 ms), R6, R7 |
| M3 | producer fails to refill: every 50th block rendered, never written | R4, R5, R17 … (15 RED) |
| M4 | consumer skips a block: two renders per write | R17, R10 |
| M5 | SFX overwrite the music instead of summing | R17, R12 |
| M6 | a switch keeps the old track's buffered chip samples | R10 |
| M7 | a switch is ignored while a song plays | R10, R16 |
| M8 | a volume update lost: any non-zero Music Volume at unity | R17 (the oracle) |
| M9 | SFX gain applied to the music | R17, R12 |
| M10 | writer-side underrun detection removed | R5 (100 ms), R9 |
| M11 | driver-side underrun counter dropped | R5, R8, R9 |
| M12 | fill never re-based after an underrun | R9 |
| M13 | runaway guard never yields | R9 |
| M14 | A3-04's two-block drain | R3, R13 |
| L1 | A3-04's sequencing: enable first + two-block drain | 19 RED: R1, R2, R3, R4, R5, R17 … |
| M15 | tables back behind a function-local static | H1, H2 |
| M16 | the synth back at `-Og` | H3 |
| M17 | the driver overflow callback not registered | S3 |
| M18 | the chip buffer regrows mid-stream | R16, `a3_04_music_synth` M6 |
| M19 | benchmark cue cadence drifts with the frame rate | B2 |
| M20 | a render exactly one block long counted as missed | P3 |
| M21 | percentile at the bucket's lower edge | P2 |

**Full suite: 134/134 passed, serial, 117.44 s** (`native/core/a3-04a-final-ctest.log`) — A3-04's 133 plus `a3_04a_audio_stream`; the host build's only warning is the known w64devkit `stl_uninitialized.h` false positive.

### 18.17 Firmware (Phase U)

Fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-a3-04a` (`a3-04a-firmware-configure.log`, `-build.log`, `-retry.log`, `-retry2.log`; final pre-commit rebuild with the version string `a3-04a-precommit-*.log`). The first two attempts stopped on firmware-only `-Werror` diagnostics the host build does not raise — `misleading-indentation` in `alpha_runtime.cpp`'s heartbeat line and `format-truncation` in the report formatter — both fixed in the source, not suppressed.

- **Zero project warnings** (the five stock `component_validation.cmake` notices only).
- **Size: `0xe91e0` = 954,848 B, +9,040 B** against A3-04's 945,808 B; **`0x16e20` = 93,728 B (9 %) free** in the 1 MiB app partition.
- **Static RAM:** `.dram0.bss` 46,816 → 49,824 B (**+3,008 B**: the pump, the library's bank and track table and the counters live in the static backend object); `.dram0.data` unchanged; `.iram0.text` **+256 B** (the two I2S callbacks). Code: `.flash.text` +8,052 B, `.flash.rodata` +1,764 B.
- **DMA / ring:** **8 descriptors × 128 frames = 2,048 B** of DMA buffers (unchanged from 4 × 256), **64 ms rendered ahead**, final chunk **128 frames (8 ms)**.
- **Heap:** the song library, 21,529 events × 8 B ≈ **172 KB**, allocated once at boot (PSRAM, except Reunion's 2.3 KB track, under the 4 KiB internal-allocation threshold); nothing is allocated after boot on the audio path.
- **Audio task stack:** 6,144 B (unchanged); its deepest frames are now ~1 KB (A3-04: ~3.4 KB, §18.9). The benchmark reports the real high-water mark.
- **Image check** (`a3_04a_hotpath_check.py`): **GREEN** on this image (`a3-04a-hotpath-a3-04a-image.log`), RED on A3-04's (`a3-04a-hotpath-a3-04-image.log`).
- Not flashed. The post-commit image (a fresh directory, `--no-ccache`) is the one the annotated tag names, with its SHA-256 and embedded `Git`.

### 18.18 Hardware retest (Phase V)

Copy the Launcher image named in tag `alpha3-a3-04a-audio-realtime` as usual; keep the existing `openu5-audio.bin` (no regeneration). The identity screen reads `FW 3.0.0-alpha3-dev-a3-04a-debug`.

1. Flash; boot. Serial (optional): `AUDIO_BACKEND music library songs=16/16 events=21529 parse_us=…` and, at the first sound, `ring=8x128 frames (64 ms ahead)`.
2. At the title (Ultima V Theme) or on the overworld (Britannic Lands), **let the music play idle 20–30 s. Is it smooth?**
3. Walk continuously for ~20 s (footsteps), bump a wall a few times, trigger a few other cues.
4. Enter a fight and fight a few rounds.
5. **Does the music stay smooth throughout?** Do movement and keys respond as before?
6. Developer (Alt+D) → Diagnostics → **Audio performance**; wait ~47 s without touching anything; photograph or copy the lines it prints.
7. Optional (the load matrix): Diagnostics → **Audio stats (live)** once to start a window, then walk ~20 s → read it; fight ~20 s → read it; open and close menus ~20 s → read it.

**Pass:** no audible recurring stutter; `underrun 0  hw 0` (or near zero) in the benchmark; `missed 0`; no input or gameplay degradation; music + SFX stable. Record the benchmark's numbers here. **Do not** start loudness/tone/balance tuning — that is A3-05.

### 18.19 Documentation and status

This section; the header and §0 above; `LAUNCHER.md`'s A3-04A row. **A3-04 music hardware status: PLAYBACK FUNCTIONAL — SMOOTHNESS RETEST PENDING** until the user validates the A3-04A image.

## 19. A3-04B — music CPU contention, overworld render lag, and the invisible diagnostic

**Hardware observation that opened this batch** (the A3-04A image, `FW 3.0.0-alpha3-dev-a3-04a-debug`, `Git e48abb8d735b`, Launcher SHA-256 `2ba8bb93…83e7a`, flashed to the T-Deck Plus by the user): the catastrophic stutter is gone and music plays mostly smoothly — **but with music on, the overworld is noticeably slower**: map refreshes lag and there is a tearing-like effect; in a fight with two snakes the audio is "a little staticky". **With Music Volume = 0 % the overworld is MUCH better.** And Developer → Diagnostics → Audio performance runs its 47 s and plays its material, but **no result appears on screen** afterwards.

**Status: MUSIC SMOOTHNESS IMPROVED — RENDER PERFORMANCE RETEST PENDING.** Everything below is software-complete and host-validated; the retest image exists (§19.15). No number in this section is a device measurement: the device numbers are either deterministic host counts or static analysis of the linked image, and say so. The retest's new report (§19.3) measures the real ones — per core and per task.

### 19.1 Baseline (Phase A)

- HEAD `5c6cf905` (A3-04A's post-commit logs); tag `alpha3-a3-04a-audio-realtime` = `e48abb8d`, the image on the device (embedded `Git e48abb8d735b`, 954,848 B, 93,728 B free). Tree clean.
- Fresh host build and **serial ctest: 134/134 passed in 116.76 s**, the one known w64devkit `stl_uninitialized.h` warning only (`native/core/a3-04b-baseline-{configure,build,ctest}.log`).
- Read from the source and `sdkconfig`:

| Item | A3-04A |
|---|---|
| Audio task | `openu5-audio`, priority 3, **pinned to core 1**, 6,144 B stack; created on the first sound by `ensure_started()` (game thread) |
| Game / render task | ESP-IDF's `main` task (`app_main`'s loop), priority 1, **pinned to core 0** (`CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0`), 24 KiB stack. Loop: drain input → `handle()` → `render()` → `vTaskDelay(pdMS_TO_TICKS(5))`, which is **0 ticks** at `CONFIG_FREERTOS_HZ=100` (a yield) |
| Other tasks | `openu5-input` prio 4 core 0; `alpha20-sd-log` idle priority, unpinned; `esp_timer` core 0; IPC per core; `Tmr Svc` unpinned |
| Music synth | OPL2, 9 channels, **49,716 Hz** chip rate, in `music_synth.cpp` at `-O2 -ffp-contract=off`, executed **from flash** (`0x4202xxxx`) |
| Output | 16 kHz mono 16-bit I2S; **8 × 128-frame DMA ring (64 ms)**, one block = 8 ms; the I2S EOF interrupt on core 0 |
| Caches | ICache **16 KB**, DCache 32 KB — **both shared by the two cores**; QIO flash 80 MHz and octal PSRAM 80 MHz behind one MSPI controller |
| Diagnostic | Developer row → `audio_perf_start` → `AudioBenchmark` ticked in `render()` → `report_audio_perf()` → `ui_->append(System, …)` + `ESP_LOGI` |
| Music Volume 0 % | `AudioService::sync_music()`: volume 0 ⇒ `stop_music` (§19.6) |

### 19.2 Why the diagnostic's results were invisible (Phase B)

The benchmark ran and its counters were correct; **its results went to the one place the user could not see.** `report_audio_perf()` appended ~21 lines to the gameplay transcript (`UiTextChannel::System`), and:

1. **The Developer screen covers the transcript.** The benchmark is started from Developer → Diagnostics, and the user waits there; that screen draws only its own menu rows and a status line — and the Diagnostics status line kept showing the *smoke test's* `Results: /sd/…` path, which looks like an answer.
2. **After leaving the menu, the lines had scrolled away.** The transcript panel is 135 px wide — 22 columns at the small text size — and 19 rows tall (fewer at larger sizes). The report's lines are 30–45 characters, so 21 lines wrap to ~50 rows, and only the **last two** (the A3-04 guard price and the heap change) were in view; the measurements themselves were above them, reachable only by paging back with Shift+Up — nothing said so.
3. Refusals ("no audio output", "already running") went to the same hidden transcript.

Serial `AUDIO_PERF_REPORT` lines were written, but the device retest is done without a serial monitor.

**Fix:** the results are a **report that replaces the Developer screen's rows until it is dismissed** (§19.3). The runtime test proves it through the real `AlphaRuntime` with raw keys and the Developer screen `Board::show_alpha` actually receives (`a3_04b_perf_runtime` D0–D10), and mutation **D1** — A3-04A's reporting re-created in the current code (lines to the transcript, nothing on the Developer screen) — turns those checks RED (§19.13).

### 19.3 The AUDIO / RENDER PERF report (Phases B, P)

Developer (Alt+D) → Diagnostics, the two rows above **Audio test tone (SFX)** (renamed; still 19 rows, the test tone still last):

- **Audio/render performance** — the A3-04A benchmark (Theme, 2 s settle, 30 s music alone, 15 s music + a cue every 100 ms). While it runs, the Diagnostics status line counts: `Audio perf 12/47s music alone: keep still`. When it ends, the report opens.
- **Audio/render stats (live)** — opens the report for the window **since the last read** (or boot), then starts a new one. This is the control-matrix tool (§19.12).

The report sits on the Developer screen — breadcrumb `Developer > Perf report`, 9 rows per page, position `1-9/32` — **until dismissed**: Up/Down (trackball) scroll a line, PageUp/PageDown a page, **Enter or Back closes it**; leaving the Developer menu dismisses it too. If the benchmark ends while the player has left the menu (walking during it is allowed — that is how to measure render under music), one transcript line says `Perf report ready: Alt+D shows it`, and Alt+D opens it first. Every line also goes to serial / the SD log as `PERF_REPORT …`; the 5 s heartbeat adds `RENDER_PERF …` and `SYS_PERF …` lines.

The benchmark's report (the format exactly as `format_perf_report` writes it; the values are illustrative):

```
AUDIO/RENDER PERF  benchmark
-- Music alone (idle): Ultima V Theme --
window 30.0s  blocks 3750  block 8.0 ms
render avg 3.21 p99 4.10 max 5.02 ms          <- one 8 ms block: music + SFX + mix
music avg 3.00 max 4.80  audio CPU 40%          <- the synth alone; render time / window
missed 1  underrun 2  hw 3  clip 4              <- deadlines, dry writes, DMA replays, saturated samples
buffered min 56 max 64 ms  sched max 9.1        <- ring fill before each write; delivery interval
voices max 9  OPL ch avg 8.6 max 9
SFX 12 (12 w/music) q1 p1  runaway 0
audio stack min 3120 B  failures 0
-- Music + cue/100 ms: Ultima V Theme --
… (the same nine lines)
-- Render: 250 gameplay frames in 45.0s --
frame avg 42.1 p95 70.0 p99 80.0
frame max 120.4 ms  late (>55 ms) 3
compose avg 12.0 max 30.0  tiles max 20.0
tft avg 30.2 max 90.1 ms
cadence avg 118.0 max 400.0 ms
input 12 shown avg 90.0 max 130.0 ms
key handling max 20.0 ms  game busy 45%
-- System --
CPU0 78%  CPU1 55%  (FreeRTOS run time)
audio 51% main 70% input 1% sdlog 2%
other tasks 3% (of one core)
heap int 123456 (min 98765) B
heap PSRAM 4567890 (min 4500000) B
stack free B: main 9870 audio 3120 input 2100
OPL2 49716 Hz -> 16000 Hz out, ring 8x128
A3-04 guard 900 ns/read = 12.5 ms/8 ms blk
```

Every field the batch asked for is on it: render avg/max (and p99), scheduling gap (`sched max` = the longest delivery interval), missed deadlines, underruns (writer) and hardware underruns (the driver's `on_send_q_ovf`), minimum and maximum buffered ms, music voices max, the audio task's CPU (its own render time, and FreeRTOS's per-task figure), the audio stack high-water mark, heap and PSRAM (free and lowest-ever), the synth/output rates, and the new clip count. Formatting is portable and tested (`format_perf_report`, `a3_04b_perf` F1–F3: every field present, every line ≤ 51 characters with every counter at `UINT32_MAX`, "no audio"/"no frames"/"no run-time statistics" said rather than left blank).

### 19.4 Core / task contention map (Phase D)

What runs where, from the source, `sdkconfig` and the linked image:

| | Core 0 | Core 1 |
|---|---|---|
| Tasks | `main` (game loop: input handling, composition, rasterizer, TFT writes, logging) prio 1 · `openu5-input` prio 4 · `esp_timer` · IPC0 · IDLE0 | `openu5-audio` prio 3 (pump, OPL2 synth, SFX synth, mix, `i2s_channel_write`) · IPC1 · IDLE1 |
| Unpinned | `alpha20-sd-log` (idle priority; SD writes on the SPI bus the TFT shares), `Tmr Svc` — they run wherever a core is free | |
| Interrupts | I2S DMA EOF (125/s, a 256 B clear + queue send, IRAM), SPI (TFT and SD share `SPI2`), the keyboard task's I2C transactions, systick | systick, cross-core yield |
| Blocking | TFT: one `spi_device_transmit` per row, `vTaskDelay(1)` (10 ms) every 16 rows; waits for the shared SPI bus while the SD writer holds it | the audio task blocks in `i2s_channel_write` whenever the ring is full |

**What the audio task does NOT do to core 0:** it takes no lock the game thread takes (the perf window is a spinlock copy every 128 ms), allocates nothing, logs nothing, and never waits for the game thread; the game thread only posts zero-timeout queue messages. It cannot preempt the game loop — they are pinned to different cores. **The one thing both cores share is the memory system**: the 16 KB instruction cache, the 32 KB data cache and the MSPI bus behind them (flash and PSRAM). In A3-04A the synth — about half of core 1, continuously, whenever music plays (§19.7) — executed from flash through that shared ICache and read constant tables through the shared DCache, while the renderer on core 0 executes its own large code path from flash and streams the 62 KB viewport and the tile cache through PSRAM. Each core's cache misses evict the other's lines and queue on the same bus. **At Music Volume 0 % the synth does not run at all** (§19.6): the renderer had the caches and the bus to itself — the user's "much better". That is the mechanism this batch fixes (§19.8); it also explains the combat "static" as the other direction of the same coupling (§19.10).

Ruled out or bounded from the source: raw CPU on core 0 (the audio task never runs there); task priority (different cores; the audio task sleeps in the write — §19.9); the I2S interrupt (125/s, a few µs each on core 0, unchanged since A3-02 whose SFX did not slow rendering); audio-held locks (none shared). The SD-log writer is the one unpinned task that could be pushed around by the audio task's core-1 bursts while it holds the shared SPI bus; the report's per-task CPU (`sdlog %`) and `tft max` measure whether it matters.

### 19.5 Render instrumentation (Phase E)

`openu5::RenderPerfCounters` (`perf_report.{h,cpp}`, portable, host-tested — `a3_04b_perf` R1–R5), owned by the game thread; no logging in the hot path (the heartbeat and the report read a snapshot):

- **frame**: every drawn *gameplay* frame (world, combat, dungeon, scenes — not the Developer screen): total, **avg / max / p95 / p99** (2 ms histogram, up to 200 ms), and **late frames** — longer than 55 ms, one tick of the original's 18.2 Hz animation clock;
- **compose** (presentation + rasterizer + post-processing) and **tiles** (the rasterizer alone) avg/max; **tft** (`Board::show_alpha`: panels + SPI, bus waits included) avg/max;
- **cadence**: the interval between drawn frames, avg/max;
- **input**: each input's handling time (`handle()` wraps it), and **input → screen latency** — from the key's capture timestamp to the end of the first gameplay frame drawn after it (`shown avg/max`);
- **game busy**: frame + input-handling time over the window (a lower bound on the game task's CPU; the report's FreeRTOS `main %` is the true figure).

`a3_04b_perf_runtime` R1/R2 prove it through the real runtime: twelve steps are twelve inputs, each shown by a later frame; Developer screen redraws are not counted.

**Per-core / per-task CPU** — `tdeck::SystemPerf` (`system_perf.{h,cpp}`, device-only): the FreeRTOS run-time statistics, newly enabled in `sdkconfig.defaults` (`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`, 64-bit counters, esp_timer clock — one timer read per context switch). A window's CPU*n* = 1 − IDLE*n*'s share; the audio, main, input and SD-log tasks are found by name, so the audio task (created with the first sound) counts from zero. Heap and PSRAM free/lowest-ever and the three stacks' high-water marks come with it. `uxTaskGetSystemState` runs only for the report and the 5 s heartbeat.

### 19.6 Music Volume 0 % (Phases C, N)

**What A3-04A does at 0 %: option D — stop.** `AudioService::sync_music()` wants `MusicSong::None` when the volume is 0, so the backend receives `stop_music`, the pump stops the player, and — with no cue pending — drains one ring of silence and **turns the channel off**. The synth runs for **no block**: zero OPL work, zero resampling, and the audio task sleeps on its queue. It was never option A (synthesize, then multiply by zero). This is exactly why the user saw "0 % = much faster": at 0 % the only core-1 load that competes for the shared memory system is gone.

The context is kept (`current_music_context()`); raising the volume above 0 **restarts the context's song from its first bar**. Decision (deliberate, kept): *restart*, not *resume*. A pause at the exact position would keep a stopped player's voices, chip and sequencer state around (and ring buffers full of stale samples), and would need a replay-to-position to be exact after a context change; the patch's own driver restarts songs on every selector change, so a restart is what an unmuted context sounds like there too. A context that changes while muted is simply the song that starts on unmute. Nothing here changes: A3-04B only pins the behaviour.

Tests (`a3_04b_perf` Z0–Z6, through `AudioService` and the production pump): 80 % → the synth runs every block; 0 % → `stop_music`, no synth block, the pump drains ≤ 9 blocks and sleeps, context kept; back to 80 % → the first 40 blocks equal a fresh start bit for bit; contexts changed while muted touch nothing and the unmute plays the *current* context (Halls of Doom); a load while muted flushes SFX and starts nothing; a cue while muted is heard and the synth still runs for no block.

### 19.7 The OPL hot path (Phases G, H, I) — measured, then made cheaper, bit for bit

**Profile (static, from the linked images; host counts on the real corpus):**

- Per chip sample the synth visits every channel with an operator not Off. On the corpus that is **8.14 channels / 15.14 operators on average** (60 s of every song; Theme 8.61). Each sounding channel steps two envelopes, two phase accumulators, two waveform+exp lookups, feedback and the mix — at 49,716 Hz.
- **A3-04A's per-sounding-channel path: ~254 Xtensa instructions**, including four out-of-line calls (`advance_envelope` ×2, `Operator::sample` ×2) and two **64-bit** phase-increment computations (`int64` shift + multiply, ~20 instructions each on the ESP32-S3), plus an out-of-line `advance_clocks` per sample. At ~1.2 cycles per instruction that is **~305 cycles per channel per chip sample ≈ 54 % of core 1 for the Theme — before any flash-cache miss**, all of it fetched through the shared ICache.
- **Silent work** (Phase H): operators whose static attenuation already makes the output exactly 0 (`expo()` of anything ≥ 3072 is 0 — the exp ROM's largest entry + 2048 < 4096) are only **6.8 %** of the operators computed; channels with both operators silent 4.2 %; carrier silent 20.3 % (its modulator still runs: the feedback history needs it). A silent fast path alone is a small win — the cost is the per-operator work itself.
- **Resampler** (Phase I): per output sample A3-04A called `std::floor` and `std::lround` (libm, **in flash**) on software doubles, plus four ROM soft-double operations; per block a `std::ceil`. A few percent of core 1 — kept at 49,716 → 16,000 Hz: **neither rate was lowered**.
- **Worst-case track**: the Theme (most sounding channels, densest events), the benchmark's song; lightest: Halls of Doom (4.38 channels). Time per output block and per sample are what the report's `music avg/max` measures on the device.

**The changes (`music_synth.cpp`) — every one bit-exact:**

| Change | Why it is exact |
|---|---|
| Phase step in 32-bit, not `int64` | fnum ≤ 1023 + 2·3 after vibrato, block ≤ 7, multiplier ≤ 30: the product stays below 2²² |
| Phase step **cached per operator**, re-derived when F-number/block/multiplier/vibrato change (`refresh_channel`, `refresh_vibrato` — the latter on the vibrato clock, before that sample's step) | the same function of the same inputs, computed at the moment an input changes instead of every sample |
| Envelope early-outs **inline**: `env_rate` / `env_mask` cached per operator (state, rate registers, key code); `envelope_step()` is A3-04A's body past its early-outs, and re-derives the cache when the state changes | A3-04A returned at the same two tests; the rate of the next sample is the new state's, as before |
| Silent floor: `(eg_level << 3) + (tl << 5) + ksl ≥ 3072` → 0 without the waveform/exp lookups (the phase still advances) | waveform attenuation and tremolo are never negative, and `expo(≥ 3072) = 0` (G7) |
| OPL2 **mono bus**: the right half is not generated; the mix-down takes the left half | OPL2 has every channel on both sides, and `(x + x) * 0.5f == x` exactly |
| `std::floor(read_pos)` → `size_t(read_pos)`; `std::ceil` → an exact integer ceil; `std::lround(double(v) * 32767.0)` → an integer round of v's significand | `read_pos` is never negative; `double(v) * 32767` is exact (24 + 15 bits), so lround is a round-half-up of the magnitude (G8: 16 M random floats + every half-way point; `--exhaustive`: every float in [−1, 1]) |
| `wave_atten`, `expo`, `Operator::sample`, `advance_clocks` forced inline (`OPENU5_SYNTH_INLINE`); `wave_atten` an if-chain, not a switch | code generation only |
| `kEgInc`, `kMult2`, `kVibratoSteps` copied into the RAM tables object | same values (§19.8) |

**Result: ~155 instructions per sounding channel per chip sample (−39 %), no call on the common path, all from IRAM** (`generate()` is one 483-instruction IRAM function). At the same 1.2 CPI: ~186 cycles ⇒ **~33 % of core 1 for the Theme** (static estimate; host relative timing of the whole corpus: −43 %, 1.551 s → 0.882 s for 320 s of audio — a direction, not ESP32 truth).

Per 8 ms output block for the Theme (8.6 sounding channels × ~398 chip samples, + ~0.3 ms resampler/SFX/mix), static: **A3-04A ≈ 4.7 ms** (≈ 37 µs per output sample) → **A3-04B ≈ 2.9 ms** (≈ 23 µs per output sample). Voice allocation and the sequencer run per MIDI event (≤ 69 per block on the Theme's densest bar), not per sample. The device's `music avg/max` replaces these estimates.

**Bit-exactness proof** (`a3_04b_perf` G1–G9; the fingerprints were computed from the A3-04A synth *before any change* — `tests/a3_04b_synth_goldens.h`, `tools/a3_04b_golden_printer.cpp`, `native/core/a3-04b-goldens-from-a3-04a.log`):

| | Fingerprint |
|---|---|
| G1 A3-04's L6 corpus (every song, 256-frame, unity) | `6a7ff3d5727df2c6` (= A3-04A's documented value) |
| G2 device geometry: every song looping 30 s, 128-frame blocks, 16 kHz, 80 % | `52875b74637a7da9` |
| G3 the corpus through OPL3 (stereo bus) | `4116e61779750372` |
| G4 44.1 kHz / 22.05 kHz / 96-frame blocks | `51b361b865686794` |
| G5 OPL2 chip, 6,000 rounds of random register writes (every cached input changed mid-note) | `e35cff74c7f0587b` |
| G6 OPL3 chip, the same, both arrays, all 8 waveforms | `df50a8ed1111170a` |

All unchanged. Mutations S1–S10 (a missed cache refresh, the floor off by one, the mono path on OPL3, the rounding half-down, a truncated phase step…) each turn them RED (§19.13).

### 19.8 The fix for the cross-core coupling: the per-sample audio path in IRAM

`native/targets/tdeck/main/audio_iram.lf` (registered with `LDFRAGMENTS`) places, symbol by symbol, **every function executed per chip sample or per output sample** — the chip (`generate`, the envelope step, the vibrato refresh), the resampler (`MusicSongPlayer::render`, `fill_chip`), the SFX voice (`SfxPlayer::render`, `SpeakerVoice::render`/`fine_level`/`begin_*`, the PIT/noise/rumble helpers), `apply_gain_q15`, the mix (`render_block`) and its per-block counters — in **IRAM (`noflash`)**. Their constant tables live in the RAM tables object; `music_synth.cpp`, `sfx_synth.cpp` and `audio_stream.cpp` are built with **`-fno-jump-tables`** (a switch table would sit in flash `.rodata`). Only the per-MIDI-event voice allocator (`dispatch`), the per-cue start (`start_next`) and the chip buffer's one-time sizing stay in flash.

So while music plays, **core 1 no longer fetches instructions or constants through the caches core 0 renders with.** The audio task's remaining memory traffic is internal SRAM (chip state, tables, the 1.6 KB chip buffer, the DMA ring) plus a few PSRAM reads of the song's event list per block.

**Proof on the linked image** — `native/targets/tdeck/a3_04b_iram_check.py` walks the call graph from `AudioRingPump::render_block` (direct `call8` targets *and* the `l32r`+`callx` long calls an IRAM function needs to reach flash) and checks every function's address and every literal it loads:

- **RED on the A3-04A ELF** (`a3-04b-iram-a3-04a-image.log`): 25 flash functions on the path — `generate`, `sample`, `advance_envelope`, `render`, `fill_chip`, the whole SFX voice — plus `floor`, `ceil`, `lround` (libm, flash).
- **GREEN on the A3-04B ELF** (`a3-04b-iram-a3-04b-image.log`): 24 functions reached, all IRAM or ROM except the three by-design flash ones; no flash `.rodata` read. (An intermediate build was RED on eight small leaves and on `kVibratoSteps`/`kMult2` in `.rodata` — `a3-04b-iram-intermediate-image.log`; that is how they were found.)
- A3-04A's own rule still holds: `a3_04a_hotpath_check.py` finds no guard, lock, allocation, log or delay from `generate()` (`a3-04b-hotpath-a3-04a-rule.log`).

Cost: IRAM `.iram0.text` +5,692 B (the audio path plus FreeRTOS's run-time-stats code); the internal heap starts 7,472 B later (IRAM + `.bss` +1,456 B). The report's `heap int` / `min` show the headroom on the device.

**Not changed, and why:** the cache sizes (16 KB ICache / 32 KB DCache — a global trade of internal RAM; if frames are still slow with music *and* muted alike, that is the renderer's own cost, a separate question the report now answers); the SPI clock; the renderer (the batch forbids reducing it).

### 19.9 Producer and ring scheduling (Phases J, K, L)

**Audit (Phase L): the producer renders only when a DMA slot is needed.** Every `step()` renders one block and hands it to `i2s_channel_write`, which returns only when the DMA has finished a descriptor; with the ring full, the task sleeps there. It never renders ahead of the ring and never spins (A3-04A's runaway guard yields if writes stop blocking). **The high-water mark is the full ring; the low-water mark is one free descriptor.** Its CPU is therefore exactly the synth's cost × real time — the model's M1 proves it (2,500 blocks rendered in 19.95 s of ring time: exactly what the DMA consumed) and M2 that with music muted core 1 is ~idle between cues. A sleep "when render-ahead is healthy" would only empty descriptors the DMA is about to play.

**Phase J — a larger music render-ahead (a PSRAM/SRAM PCM ring in front of the DMA): assessed, not implemented.** Core 1 has no other work to give time to — the audio task already sleeps whenever the ring is full — and the cross-core coupling scales with the synth's *total* memory traffic, not its burstiness; bursts would not reduce it (and IRAM placement removes it). It would add latency to every volume change, song switch and load/title flush (or a flush path to discard pre-rendered music), and a PSRAM ring would *add* traffic on the shared bus. The 64 ms ring already rides out a single ~59 ms stall (A3-04A R6).

**Phase K — priority / affinity: unchanged, deliberately.** The audio task is alone on core 1 at priority 3; lowering it gives time only to IDLE1 and the unpinned SD writer, and cannot help core 0. Moving the I2S interrupt to core 1 (bringing the channel up on the audio task) would move 125 short ISRs/s off core 0 — kept as a lever if the retest's `tft max`/`frame max` still differ between music on and muted with everything else equal.

### 19.10 Combat "static" (Phase M)

A **mix-saturation counter** now counts every sample the music + SFX sum clamps (`clip` in the report; `AudioPerfSnapshot::mix_clipped`, per block and per window). Host evidence on the real corpus (`a3_04b_perf` C1–C3):

- The loudest song at unity peaks at **25,240** (Lord Blackthorn; the combat song Engagement and Melee: 9,697), the loudest cue at **15,688** (the shop's; the combat hits far less). At the **default volumes (80 % / 80 %) the largest possible sum is 26,191 < 32,768: no song under any cue can saturate.** A fight (the combat song under a heavy hit every 104 ms) never saturates even at 100 % / 100 % (peak 12,328).
- The counter itself is proved against an independent sum: the loudest song under the loudest cue at 100 %/100 % clips 3 samples, and the pump counts exactly those 3.

So the combat static is **not clipping**. The remaining candidates are underruns (the synth slowed by cache contention exactly when combat rendering is busiest — the §19.4 coupling in the other direction; the report's `missed`/`underrun`/`hw` counters) and the combat cues' own character (a noise burst *is* noise). IRAM placement removes the contention; if the retest still hears static with `underrun 0 hw 0 clip 0`, it is the cue's timbre — A3-05's tonal pass, not a defect.

### 19.11 Host performance model (Phase O)

`a3_04b_perf` M1–M4 — the production pump against A3-04A's ESP-IDF descriptor-ring model (now shared as `tests/a3_04a_virtual_i2s_ring.h`), with a per-block cost from the static synth costs above (sounding channels × chip samples × cycles, + fixed per-block work; desktop time is never used):

- **M1** the producer renders exactly what the DMA consumes, never ahead; the audio task's CPU is the synth's cost and nothing more (**33 %** of core 1 for the Theme at the A3-04B cost; 54 % at A3-04A's).
- **M2** music muted: the synth runs for no block; core 1 ~idle between cues (31 ‰).
- **M3** contention headroom: cost inflation in random bursts on 30 % of blocks (a busy renderer's cache pressure), swept until the speaker runs dry — **A3-04A tolerates 2.95×, A3-04B 5.35×** (≥ 1.5× the ratio of A3-04A's, as the synth-cost ratio 305/186 predicts). This is the model's statement of the combat-static hypothesis: A3-04A had little margin when the renderer pressed on the shared caches.
- **M4** a cue submitted while music plays is heard within the ring + one block (64 ms ≤ 72 ms) at the new cost.

The model does not claim the size of the cross-core coupling — that constant is unmeasured, and IRAM placement removes it by construction (§19.8). The retest measures it: frame times with music on vs. muted.

### 19.12 Control matrix (Phase F) — how the retest measures it

Each case: Developer → Diagnostics → **Audio/render stats (live)** once (dismiss it: that starts a clean window), do the activity for ~20 s, then read **Audio/render stats (live)** again and photograph the report pages.

| Case | Setup | What separates the hypotheses |
|---|---|---|
| 1 | Music Volume 0 % (capability present), walk | the reference: synth off; `CPU1` ≈ SFX only |
| 2 | Music > 0, title music, idle | audio CPU alone |
| 3 | Music > 0, overworld idle | + animation frames |
| 4 | Music > 0, walking continuously | **frame avg/max, tft max, late, input max vs case 1** — equal ⇒ contention fixed |
| 5 | Music + footsteps | + SFX; `clip` |
| 6 | Music + combat SFX (a fight) | `missed`, `underrun`, `hw`, `clip` — the static |
| 7 | No music at all (remove `openu5-audio.bin`, or stock files) | same as case 1 for the synth; SFX-only baseline |

A3-04A cannot be measured this way (its report never appeared); the user's own "0 % much better" is the A3-04A datum.

### 19.13 RED / GREEN and mutations (Phases Q, T)

**RED first** — none of these is "looks laggy":

1. **Diagnostic invisible**: mutation **D1** re-creates A3-04A's reporting inside the current code (lines appended to the gameplay transcript; the Developer screen shows none): the runtime test goes RED (D2 "the report replaces the rows", D4, D7, D9 …) — the reported defect, reproduced through raw keys and the real Developer screen.
2. **Per-sample audio path on the shared flash cache**: `a3_04b_iram_check.py` **RED on the A3-04A image**, GREEN on A3-04B's (§19.8).
3. **Synth cost**: A3-04A's per-channel path had four out-of-line calls and two 64-bit multiplies per chip sample (listing, §19.7); the source scan I4 (no phase-step computation, no 64-bit value, no library call in `generate()`) is RED on A3-04A's source.
4. **Music Volume 0 % still synthesizing**: *not* RED on A3-04A — it already stopped (§19.6); mutation Z1 (volume 0 keeps the song at zero gain) proves the tests would see it.
5. **Mixer clipping**: A3-04A had no counter; C1 now proves one against an oracle, and C3 proves the defaults cannot clip.

**Mutations** — `native/core/tools/a3_04b_mutation_check.py` → `native/core/a3-04b-mutation.log`. A mutation that does not build is *invalid*, not killed: every kill below is a failing check.

| # | Mutation | Killed by |
|---|---|---|
| S1 | the envelope cache is not refreshed after an attack/decay rate write (0x60) | `perf` G5, G6 (2 RED) |
| S2 | the envelope cache is not refreshed when the envelope changes state | `perf` G1, G2, G3, G4, G5 … (8 RED) |
| S3 | a vibrato step does not refresh the vibrato operators' phase step | `perf` G1, G2, G3, G4, G5 … (6 RED) |
| S4 | a vibrato-depth write (0xBD) does not refresh the phase steps | `perf` G5, G6 (2 RED) |
| S5 | the silent floor one below the exp ROM's real limit (3071) | `perf` G7 (1 RED) |
| S6 | total level / key scaling not refreshed on a 0x40 write | `perf` G5, G6 (2 RED) |
| S7 | the mono shortcut taken for the OPL3 chip too (its stereo bus mixed as left only) | `perf` G3 (1 RED) |
| S8 | the integer rounding rounds half down | `perf` G8 (1 RED) |
| S9 | the exact ceil returns the floor | `perf` G1, G2, G3, G4, G9 … (8 RED); `stream` R5, R7 … (7 RED) |
| S10 | the phase step truncated to 16 bits | `perf` G1, G2, G3, G4, G5 … (6 RED) |
| Z1 | muted music is still fully rendered (volume 0 keeps the song, at zero gain) | `perf` Z1, Z2, Z3, Z4, Z5 … (7 RED) |
| Z2 | a restart keeps the old song's buffered chip samples | `perf` Z2 (1 RED); `stream` R10 (1 RED) |
| Z3 | SFX starve while the music is muted (pending cues no longer wake the pump) | `perf` Z5 (1 RED); `stream` R3 (1 RED) (re-run, after Z5/M1 were strengthened) |
| K1 | the producer renders ahead of the ring (two renders per write) | `perf` M1 (1 RED); `stream` R17, R10 (2 RED) (re-run, after Z5/M1 were strengthened) |
| K2 | the producer never yields (the runaway guard never fires) | `stream` R9 (1 RED) |
| K3 | the ring is primed with one block instead of all of it (no render-ahead high water) | `stream` R1, R4 (2 RED) |
| C1 | the clipping counter disabled | `perf` C1 (1 RED) |
| C2 | the clipping counter counts only the positive limit | `perf` C1 (1 RED) |
| R1 | render stats never update (no frame is counted) | `runtime` R1 (1 RED) |
| R2 | a frame exactly one animation tick long counts as late | `perf` R1 (1 RED) |
| R3 | an input is never marked shown | `runtime` R1 (1 RED) |
| R4 | Developer screen redraws counted as gameplay frames | `runtime` R2, D9 … (3 RED) |
| R5 | the frame percentile reports the bucket's lower edge | `perf` R2 (1 RED) |
| D1 | A3-04A's reporting: the lines go to the gameplay transcript, the Developer screen shows none | `runtime` D2, D3, D4 … (11 RED) |
| D2 | the diagnostic results discarded (the report is built and never shown) | `runtime` D2, D3, D4 … (8 RED) |
| D3 | the report does not stay: any key dismisses it (no scrolling) | `runtime` D4 … (2 RED) |
| D4 | a report finished with the menu closed is dropped | `runtime` D7 … (2 RED) |
| D5 | the benchmark shows no progress on the Developer screen | `runtime` D1 (1 RED) |
| D6 | the report loses its render section | `perf` F1, F3 (2 RED); `runtime` D4, D9 (2 RED) |
| D7 | the live read does not start a new window | `runtime` D9 (1 RED) |
| I1 | generate() left out of the IRAM fragment | `perf` I1 (1 RED) |
| I2 | jump tables allowed again in the IRAM synth file | `perf` I2 (1 RED) |
| I3 | FreeRTOS run-time statistics dropped from the firmware configuration | `perf` I3 (1 RED) |

**33 mutations, 33 killed by a failing check, 0 survived, 0 invalid** (`a3-04b-mutation.log`, one clean pass on the final production code; restored build and all four tests GREEN afterwards). History, kept: a first pass (`a3-04b-mutation-pass1.log`) had three INVALID entries — S2's replacement left a parameter unused under `-Werror` (the mutation was fixed), S3/S4 hit a transient `ld returned 1` relinking an `.exe` — re-run clean in `a3-04b-mutation-pass1-rerun.log`. Reading which check killed what then showed two of this batch's own checks weaker than their labels: Z5 played its cue before the pump had gone to sleep (Z3 was caught only by A3-04A's R3) and M1 counted loop steps, not renders (K1 was caught only by A3-04A's R10/R17). Both were strengthened (Z5 drains to sleep first; M1 counts `render_block` calls) and Z3/K1 re-run (`a3-04b-mutation-rerun.log`): each is now killed by its own check.

### 19.14 Timing, gameplay and audio regression (Phases R, S)

The game thread's only changes are the counters (a few timer reads per frame and per input), the report view (Developer-menu only), and the renamed rows; nothing reads game state or a game clock differently, and nothing in the audio path changed its output (§19.7). **Full suite: **136/136 passed, serial, 126.08 s** (`native/core/a3-04b-final-ctest.log`; the first full run, before the Z5/M1 strengthening, was also 136/136 in 125.16 s, `a3-04b-ctest-pass1.log`)** — A3-04A's 134 plus `a3_04b_perf` (38 checks) and `a3_04b_perf_runtime` (21). All existing timing proofs pass unchanged: Camp, Blackthorn, Refuge, quake, shrine and combat-victory timelines under the none/synth/muted/broken/stalled/racing backends (`a3_01_audio_runtime` T19–T21, `a3_02_sfx_runtime`, `a3_03_sfx_runtime`), `batch51_*` pacing, `batch53a_ending_terminal`, persistence (`batch24`–`batch28`); and `a3_04b_perf_runtime` T1 walks with the counters, a live read and the report in between and ends in exactly the same game state.

Audio regression, all green: music contexts and switches, loops, Camp, Ending, volume, stock no-music, unknown patch (`a3_04_music_runtime`, A3-01 P/S-series); SFX overlay and independent volumes (A3-04A R12, R17); mute/unmute and load while muted (Z-series); load/Return-to-Title flush (A3-04A R15, A3-02 M4/M5); combat transitions (`a3_03_sfx_runtime`); the whole A3-04A stream suite (58/58) against the new synth.

### 19.15 Firmware (Phase U)

Fresh ESP-IDF 6.1 build, `native/targets/tdeck/build-a3-04b` (`a3-04b-firmware-configure.log`, `-build.log`; incremental rebuilds `-build-pass2.log`, `-build-pass3.log` after the IRAM check found the leaves). **Built clean on the first attempt; zero project warnings.**

- **Size: `0xeb210` = 963,088 B, +8,240 B** vs A3-04A's 954,848 B; **`0x14df0` = 85,488 B (8 %) free** in the 1 MiB app partition.
- **Static RAM:** `.iram0.text` 70,323 → 76,015 B (**+5,692 B**: the audio per-sample path + FreeRTOS run-time statistics); `.dram0.bss` 49,824 → 51,280 B (+1,456 B: render counters, the benchmark's saved window, system-perf state); `.dram0.data` +128 B. The internal heap starts 7,472 B later.
- **PSRAM:** + the report's rows (48 × 52 = 2,496 B) and the task table (24 × `TaskStatus_t`); the song library unchanged (~172 KB).
- **Buffers:** DMA ring unchanged — **8 × 128 frames = 2,048 B, 64 ms**; **high water = the full ring** (the write blocks), **low water = one free descriptor** (the write returns and the next block renders).
- **Tasks:** audio `openu5-audio` prio 3 core 1, 6,144 B stack (unchanged); game `main` prio 1 core 0; input prio 4 core 0.
- **Configuration:** `CONFIG_FREERTOS_USE_TRACE_FACILITY`, `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`, `…_RUN_TIME_COUNTER_TYPE_U64`, `…_RUN_TIME_STATS_USING_ESP_TIMER` (in the tracked `sdkconfig.defaults`; the ignored local `sdkconfig` had them explicitly off and was switched on).
- **Image checks:** `a3_04b_iram_check.py` GREEN; `a3_04a_hotpath_check.py` GREEN.
- Not flashed. The post-commit image (a fresh directory, `--no-ccache`) is the one the annotated tag names, with its SHA-256 and embedded `Git`.

### 19.16 Hardware retest (Phase V)

Copy the Launcher image named in tag `alpha3-a3-04b-render-contention` as usual; keep the existing `openu5-audio.bin` (no regeneration). The identity screen reads `FW 3.0.0-alpha3-dev-a3-04b-debug`.

1. **Music on, overworld idle ~20 s.** Then Developer → Diagnostics → *Audio/render stats (live)* → dismiss (Enter) — a clean window starts.
2. **Walk continuously ~20 s.** Read *Audio/render stats (live)*; photograph its pages (Down scrolls).
3. **Fight 2–3 enemies** a few rounds. Read and photograph again. Is there static?
4. **Compare map smoothness** (steps 2–3) with A3-04A's memory of it.
5. **Set Music Volume 0 %, repeat the walk** (~20 s) and read/photograph again — the reference.
6. **Restore Music Volume.**
7. Developer → Diagnostics → **Audio/render performance**; wait ~47 s (or walk during it and press Alt+D when the transcript says the report is ready).
8. **Send a photo of every page of the report** (Down to scroll; Enter/Back closes it).

**Pass:** no obvious overworld slowdown or tearing relative to the muted walk (frame avg/max and `tft max` close between steps 2 and 5); music smooth; no recurring static; `underrun`/`hw` 0 or near 0; `late` near zero; input responsive (`input … max` similar with music on and off). **Do not** start loudness/tone tuning — that is A3-05.

### 19.17 Findings

- **F-1 (fixed):** the Developer audio diagnostic's results were printed only to the gameplay transcript, hidden by the Developer screen and wrapped out of view (§19.2).
- **F-2 (fixed, retest pending):** the music synth executed from flash through the instruction and data caches both cores share; the renderer on core 0 and the synth on core 1 slowed each other (§19.4, §19.8).
- **F-3 (found, not fixed — out of scope):** `MusicSongPlayer::render` under-fills its chip buffer at output ratios above ~3 (below ~16.6 kHz, e.g. 11,025 Hz): `needed = ceil(1 + ratio·(frames−1)) + 2` can fall below `ratio·frames + 1`, the unconsumed fraction accumulates in `read_pos_`, and after a few hundred calls the interpolation reads past the buffer. **The device never reaches it**: at 16 kHz and 128 frames `needed − ratio·frames = 0.27 ≥ 0` for every starting fraction, and the golden G2 runs that geometry. Present since A3-04; host-only rates. Queued for a later audio batch.

### 19.18 Documentation and status

This section; the header and §0 above; `LAUNCHER.md`'s A3-04B row. **A3-04 music hardware status: MUSIC SMOOTHNESS IMPROVED — RENDER PERFORMANCE RETEST PENDING** until the user validates the A3-04B image.

## 20. A3-04C — music-on render/TFT and main-loop contention: measure first

**Hardware observation that opened this batch** (the user, after A3-04A/A3-04B): music plays mostly smoothly, but with music enabled the overworld/map refresh is noticeably laggy, visual updates look tearing-like or uneven, responsiveness is worse, render/TFT timing spikes were visible in the performance diagnostics and the main/game side looked heavily loaded; **Music Volume 0 % is substantially better.** No device numbers or photographs came with the report, and the image it was seen on is not recorded (A3-04B `…-a3-04b-debug` and A3-HF1 `…-a3-hf1-debug` both carry A3-04B's IRAM placement). The checklist (§20.10) records both first.

**Status: OUTCOME C — INSTRUMENTED; THE CAUSE IS NARROWED, NOT PROVEN; HARDWARE EVIDENCE PENDING.** Nothing in this batch changes what the game does, what it draws or what it plays. It adds the measurements and one Developer probe the device needs to tell the remaining hypotheses apart, and it leaves the synth, the renderer, the TFT driver, task placement and buffering exactly as A3-04B left them. The source and the linked image do not show a mechanism big enough to explain "substantially better at 0 %" after A3-04B (§20.6), so a production "fix" now would be a guess, which the batch rules forbid (§20.11).

### 20.1 Baseline (Phase 1)

- HEAD `5469b577` on `main` (A3-HF1's post-commit logs), tree clean. Latest tags: `alpha3-hf1-arena-loot` (`d1465ed7`), `alpha3-a3-04b-render-contention` (`11015be6`), `alpha3-a3-04a-audio-realtime` (`e48abb8d`).
- Firmware at baseline: A3-HF1, `0xeb250` = 963,152 B, 85,424 B (8 %) free.
- Fresh host build `native/core/build-a3-04c-base`, **serial ctest 137/137 passed in 143.07 s**, the one known w64devkit `stl_uninitialized.h` warning (`native/core/a3-04c-baseline-{configure,build,ctest}.log`).
- Reviewed: A3-04 (§17), A3-04A (§18), A3-04B (§19) and their diagnostics.

**Execution-context map** (from the source, `sdkconfig` and the linked image):

| What | Where / how |
|---|---|
| Audio task | `openu5-audio`, prio 3, **core 1**, 6 KiB internal stack. Blocks in `i2s_channel_write` (DMA-paced, one 8 ms block per wake) or, when silent, in a 20 ms idle queue wait |
| Game / render | ESP-IDF `main`, prio 1, **core 0**. Loop: drain ≤ 64 inputs → `handle()` → `render()` (twice after input) → `vTaskDelay(pdMS_TO_TICKS(5))`, which is **`vTaskDelay(0)` at `CONFIG_FREERTOS_HZ=100`: a yield, not a sleep**. The task is always runnable except inside the TFT write's waits |
| TFT write | Inside `main`: `Board::show_alpha` → one **blocking `spi_device_transmit` per pixel row** (SPI2 at 40 MHz, DMA from the internal `transfer_row_`), **`vTaskDelay(1)` every 16 rows** (every 32 chunks in `fill_rect`). The whole 176-px viewport is 158 rows ⇒ 9 yields, each until the next 10 ms tick |
| Interrupts on core 0 | I2S TX GDMA EOF (125/s while the channel runs, allocated where `ensure_started()` ran), SPI2 transaction completion (IRAM), esp_timer, systick |
| Input | `openu5-input`, prio 4, core 0 |
| SD-log writer | `alpha20-sd-log`, idle priority, **unpinned**, writes the card on **SPI2, the TFT's bus**; 4 KiB stdio buffer, flush every 2 s or on "important" lines |
| Synchronisation | Game → audio: two FreeRTOS queues (zero-timeout sends, `xQueueOverwrite` for music), atomics (gains, epoch, the new probe). Audio → game: the published perf window under a portMUX spinlock, copied every 16 blocks. TFT: the SPI2 bus lock shared with the SD card. SD: the storage mutex |
| Shared hardware | The **16 KB ICache / 32 KB DCache and the MSPI bus (flash + octal PSRAM) serve both cores**; SPI2 (TFT + SD); the FreeRTOS kernel lock |
| Code placement | Audio per-sample path in IRAM (A3-04B, image-checked). **FreeRTOS kernel and the SPI master's non-ISR code run from flash** (`CONFIG_FREERTOS_IN_IRAM` off) — used by both cores |
| Logging | Every `ESP_LOG` goes to USB-Serial/JTAG and into a 48-line PSRAM queue for the SD writer. The audio task logs nothing per block; **the game thread logs `PRESENTATION_DISPATCH` on every gameplay frame** (plus the 5 s heartbeat) |

### 20.2 What "Music Volume 0 %" does (Test C) — read from the source, pinned by tests

**It stops the song** (A3-04B §19.6's "option D"): `AudioService::sync_music()` wants `MusicSong::None` when the music volume is 0 → the backend's `stop_music()` → the pump stops the player → eight silent blocks (64 ms) drain → **`i2s_channel_disable`** → the audio task sleeps in its 20 ms queue wait. So, against music on, 0 % removes **three things at once**:

1. the OPL2 synth's work on core 1 (≈ 33 % of core 1 for the Theme, A3-04B's static estimate);
2. the I2S DMA and its **125/s EOF interrupt on core 0**;
3. the audio task's 125/s DMA-paced wakes (replaced by 50/s idle-queue timeouts).

It is not "synthesize and multiply by zero", not "mute after the mix", and it does not leave the sequencer running. The context is kept; raising the volume restarts the context's song from its first bar. A 0 %-vs-on comparison therefore **cannot say which of the three matters** — the probe of §20.4 splits them. Pinned by `a3_04b_perf` Z1–Z6 (unchanged) and `a3_04c_contention` P8 (the stop path is unaffected by the probe).

### 20.3 Instrumentation added (Phase 2)

All of it is aggregated in windows; nothing is logged per frame or per block. The window is the Developer report's: it starts at boot and at every *Audio/render stats (live)* read, so the on-screen report and the serial line always describe the same span.

| Measurement | Where | How / cost |
|---|---|---|
| **logic** avg/max — `render()`'s game logic before composing (combat, scenes, timers, input hold, ambient) | `alpha_runtime.cpp` | one timer read per drawn frame |
| **loop** count/avg/max — one pass of `main.cpp`'s loop (input, handling, drawing) | `main.cpp` → `note_loop_pass` | one timer read per pass |
| **TFT split per frame**: `xfer` (inside `spi_device_transmit`: bus acquire, DMA, completion wait), `yield` (inside the draw loops' `vTaskDelay(1)`), `fill` = tft − xfer − yield (building rows: the PSRAM viewport read, glyph lookups, byte swap; panel text; window setup) | `Board::tft_transmit` / `tft_yield` (every TFT transaction and every draw-loop yield goes through them — `a3_04c_contention` S1/S2) | two `CCOUNT` reads per transaction and per yield (core-0 cycles; no call to a timer, no lock) |
| **viewport frames vs other frames** — tft avg/max for frames that rewrote the whole viewport, and for the rest (animated cells, panels, text) | `show_alpha` marks the whole-viewport write | a counter |
| transactions, rows, **slow transactions** (> 1 ms: waited for the SPI bus or was preempted) and how many of those had the audio task running, the slowest transaction | `tft_transmit` | — |
| **yields**: count, max, **late** (> 11 ms: core 0 was not handed back at the tick) | `tft_yield` | — |
| **Row-level contention test**: every row is classified by the audio task's *running* flag at the row's start and at both ends of its transaction (busy / idle / straddling); **fill and xfer per row are averaged separately for audio-busy and audio-idle rows** | `tft_transmit` + `TdeckAudioBackend::activity_flag()` | one aligned-word load per end |
| audio **task busy per block** avg/max (DMA freed a descriptor → next block handed over = delivery interval − the write's wait) | `AudioPerfCounters::on_write` (flash, per block) | arithmetic only |
| audio **sfx+mix** avg (render − music) and the i2s **write** avg/max | report | — |
| **SD-log bursts**: count, max, total mutex-held time of the writer's wakes that wrote or flushed | `sd_diagnostic_logger.cpp` | one timer read per such wake |
| the **scenario** label: music capability, Music/SFX volume, song, probe | `AlphaRuntime::perf_scenario()` | — |

Kept from A3-04B unchanged: frame avg/p95/p99/max and late (> 55 ms), compose, tiles, tft avg/max, cadence, input → screen latency, key handling; audio render avg/p99/max, music avg/max, missed deadlines, underruns, hardware underruns, buffered min/max (the slack), sched max, clip, voices/channels, stack; CPU0/CPU1 and per-task CPU, heap, stacks.

The row-level split is the batch's key measurement. The same code runs over the same frames in the same run, and the only difference between the two sets of rows is whether core 1's audio task was executing at that instant. A shared-cache, MSPI, internal-SRAM or kernel-lock coupling shows up as **busy > idle**. With no coupling, the two averages match within noise, whatever the absolute frame time.

### 20.4 The discriminating probe: Developer › Diagnostics › **Probe: synth bypass**

A new Diagnostics row, **inserted above *Audio/render performance*** so that the stats, benchmark and test-tone rows keep their places counted from the end (A3-01/A3-04B tests navigate by them). Enter toggles it; the row shows `Probe: synth bypass: off` / `ON`. It is **off at boot and never saved**; every report and `A3C_PERF` line made while it is on is labelled **`BYPASS`**.

With the probe on, `AudioRingPump::render_block` renders the playing song's block as silence without touching the player. The song stays "playing": the channel keeps running, the DMA and its 125/s interrupt keep going, the audio task keeps its DMA-paced cadence, and SFX still sound. The OPL2 synth does no work and the song does not advance. Switching it off resumes the song exactly where it stopped (proved bit-exact, P6). So:

| Run | synth work | I2S DMA + EOF ISR on core 0 | 125/s audio wakes |
|---|---|---|---|
| B — music on | yes | yes | yes |
| **B′ — music on + probe** | **no** | yes | yes |
| C — Music Volume 0 % | no | no | no |

**B ≈ B′ ≫ C** puts the lag on the channel/interrupt/wake cadence, not the synth. **B′ ≈ C ≪ B** puts it on the synth's own work, which is then a memory-system coupling, because it runs on the other core. **B ≈ B′ ≈ C** says music is not the cause.

The check is one boolean load in `render_block`, which stays in IRAM (`a3_04b_iram_check.py` GREEN on the A3-04C image; `audio_iram.lf` unchanged).

### 20.5 Output formats (Phase 2 "diagnostic output")

**On the Developer screen** — the *Audio/render stats (live)* and *Audio/render performance* reports gain a section (values illustrative):

```
-- Contention map (A3-04C) --
music 80% Ultima V Theme sfx 80%              <- the scenario (BYPASS when the probe is on; "music n/a" without the capability)
logic avg 0.4 max 2.1  loop max 130.1 ms
viewport 40 frm tft avg 80.0 max 90.1         <- frames that rewrote the whole viewport
other 210 frm tft avg 12.0 max 30.0
tft/frame fill 4.1 xfer 6.0 yield 20.1        <- where a frame's TFT time went
yield 900 max 10.9 ms late 0 (tick 10)
xfer max 1.20 ms slow 3 (2 w/audio)
rows 12345  audio running at 33%
row fill us: audio idle 12.1 busy 12.3        <- the cross-core test: equal = no coupling
row xfer us: audio idle 65.0 busy 66.0
audio task/blk avg 2.90 max 3.20 ms
sfx+mix avg 0.20 write avg 5.0 max 7.9
sd log 3 bursts max 12.3 total 20.0 ms
```

The report buffer went from 48 to 64 lines; the benchmark's report is now 50 lines and would have lost its last two (`a3_04c_contention` F4).

**On serial / the SD log** — one `A3C_PERF` line every 5 s (the heartbeat), the same window as the screen, fixed field order so two runs diff field by field (≤ 719 characters: one SD-log record with its prefix, L2):

```
A3C_PERF scen=[music 80% Ultima V Theme sfx 80%] win=20.1s | frame n=250 avg=42.1 p95=70.0 max=120.4 late=3 | cmp=12.0/30.0 tiles=20.0 tft=30.2/90.1 in=12:130.0 | logic=0.4/2.1 vp=40:80.0/90.1 oth=210:12.0/30.0 | split fill=4.1 xfer=6.0 yld=20.1 | yld n=900 max=10.9 late=0 | xfer max=1.20 slow=3/2 | rows=12345 busy=33% fill=12.1/12.3 xfer=65.0/66.0 | loop=5000:5.0/130.1 | audio blk=2500 rnd=2.90/4.10 mus=2.70/3.90 task=3.20 buf=56 und=0 hw=0 miss=0 clip=0 | sd=3:12.3/20.0 | cpu0=98 cpu1=36 main=95 aud=33 inp=1 sdl=1
```

Legend: `a/b` = avg/max ms; `n:a/b` = count:avg/max; `cmp` compose; `vp`/`oth` viewport/other frames' TFT; `split` per-frame TFT ms; `slow=total/with-audio`; `rows … fill=idle/busy xfer=idle/busy` µs per row; `rnd`/`mus` block render/music ms; `task` audio task busy max per block; `buf` minimum buffered ms; `und`/`hw`/`miss` underruns / hardware underruns / missed deadlines; `sd=bursts:max/total` ms; CPU in %. The A3-04B `AUDIO_PERF` / `RENDER_PERF` / `SYS_PERF` heartbeat lines are unchanged.

### 20.6 Phase 4 — the hypotheses against the source and the image

| # | Hypothesis | Evidence available now | Status | What the device run decides it with |
|---|---|---|---|---|
| 1 | Both heavy tasks on one core / priority starvation | Audio pinned to core 1, game to core 0; the audio task cannot preempt the game loop | **ruled out** (source) | CPU0/CPU1, `main`/`aud` % |
| 2 | Audio bursts too large | Per-block work is on core 1; A3-04B's model: the producer renders exactly what the DMA consumes | bounded | `task` max per block |
| 3 | Excessive context switching | 125/s audio wakes on core 1; each TFT row already costs core 0 two switches of its own | small | rows busy vs idle |
| 4 | Critical sections | The perf spinlock is taken every 128 ms (copy of one snapshot) by core 1, by core 0 only for a report / heartbeat | **ruled out** (source) | — |
| 5 | Heap / allocator | The audio task allocates nothing after start (A3-04A) | **ruled out** (source, tests) | heap lines |
| 6 | Logging contention | The audio task logs nothing per block. **The game logs one line every gameplay frame** (`PRESENTATION_DISPATCH`), which feeds the SD writer on the TFT's bus, whether music plays or not | music-independent baseline | `sd=` bursts, `slow` transactions |
| 7 | SPI / TFT DMA interaction | I2S and SPI2 are separate peripherals on separate GDMA channels; SPI2's only other user is the SD card | not audio | `slow`, `xfer` busy vs idle |
| 8 | Cache / IRAM / flash | Per-sample audio path in IRAM (image check GREEN). **Residual per-block flash code on core 1: at most 2,048 instructions ≈ 5.3 KB ≈ 167 of the ICache's 512 lines** (`a3_04c_audio_flash_footprint.py`, below), most of it the FreeRTOS kernel and IDF I2S code the renderer's own SPI calls use too | small, not zero | **row `fill` busy vs idle**; B vs B′ |
| 9 | PSRAM bandwidth | The audio task touches PSRAM only per MIDI event (8-byte events, ≤ 69 per block on the Theme's densest bar) | small | row `fill` busy vs idle |
| 10 | Float synth monopolising a CPU | Core 1 only | not core 0 | CPU1 |
| 11 | Audio cadence → periodic core-0 stalls | The I2S EOF ISR runs on **core 0**, 125/s, only while the channel runs | **open** | **B′ vs C** (the probe keeps it) |
| 12 | Notification / semaphore contention | No primitive is shared between the audio task and the game thread except the kernel lock | small | row `xfer` busy vs idle |
| 13 | DMA ISR behaviour | see 11 | open | B′ vs C |
| 14 | Cross-core synchronisation | FreeRTOS's kernel spinlock: both cores take it on every queue/semaphore op; the TFT takes it several times per row | small | row `xfer` busy vs idle |
| 15 | Music work while inaudible | At 0 % none (stop). A3-04B's silent-operator floor covers rests | **ruled out** (source) | — |
| 16 | Expensive sequencing, repeated | Per MIDI event (`dispatch`, flash), not per sample | small | `mus` max vs avg |

**Two findings that do not depend on music, both visible now:**

- **The TFT write sleeps by design.** A full viewport is 158 rows with a `vTaskDelay(1)` every 16. Each 16-row band takes about 2 ms of SPI work and then waits for the next 10 ms tick, so a step's viewport write is roughly 9 ticks ≈ 85–95 ms, delivered in 16-row bands. On the panel that is a top-to-bottom wipe, which can look like tearing whether or not music plays. The new `yield` share measures it. If it dominates `tft` equally in B and C, the "tearing" is this pacing and belongs to a renderer batch, not audio.
- **"The main side looks heavily loaded" is the loop's shape.** `vTaskDelay(pdMS_TO_TICKS(5))` is `vTaskDelay(0)` at 100 Hz, so `main` is runnable all the time except inside the TFT's waits, and CPU0 reads near 100 % in every scenario. The loop counters and `main %` let B and C be compared. A high `main %` alone says nothing about music.

**Residual flash footprint of the audio task** (`native/targets/tdeck/a3_04c_audio_flash_footprint.py <elf> <log> --depth 3`, `a3-04c-audio-flash-footprint.log`): the walk starts from `TdeckAudioBackend::run`, `AudioRingPump::step` and the sink's `write`, skips error and log paths, and skips A3-04B's IRAM per-sample path. It reaches 54 functions: 18 in IRAM, 36 in flash, 2,048 flash instructions in total. The largest are `AudioPerfCounters::snapshot` (310, every 128 ms, not every block), `step` (238), `run` (153), `xQueueGenericSend` (128), `i2s_channel_write` (127), `xQueueSemaphoreTake` (109). The normal per-block path is a fraction of that upper bound. Even the whole bound, evicting and refilling 167 lines 125 times a second, is about 21,000 extra misses a second, on the order of 1 % of core 0. That is why the device measurement, not this estimate, decides it.

### 20.7 Phase 3 — the controlled comparisons (what each run isolates)

| Test | Setup | Isolates |
|---|---|---|
| **A** music unavailable | no `openu5-audio.bin` on the card (Settings: `Music Volume: Unavailable`) | the renderer alone; the audio task starts only for SFX |
| **B** music on | Music 80 %, SFX 80 % | everything |
| **B′** music on + probe | as B, *Probe: synth bypass: ON* | B minus the synth's work |
| **C** Music Volume 0 % | Music 0 %, SFX 80 % | B minus synth, DMA/ISR and wakes (the channel runs only around cues) |
| **D** SFX only | as C, walking into a wall / fighting so cues play | SFX synth + the channel's on/off cycles |
| **E** heavy refresh | B, then C, then B′ on the same heavy path (§20.10 step E) | the renderer's worst case under each |

### 20.8 Tests, RED / GREEN and mutations

- **New targets:** `a3_04c_contention` (41 checks: P probe on the production pump, B task busy against the descriptor-ring model, C counters, F report section, L the one-line format, S device wiring scans) and `a3_04c_contention_runtime` (13 checks through the real `AlphaRuntime` with raw keys: every gameplay frame's TFT timing reaches the report and the line, Developer frames are not counted, a live read restarts the window; the probe row toggles the audio task's probe, shows its state, labels the report, says "no audio output" without a backend; the game is untouched with the probe on and every frame timed). The host Board stub plays the device Board's per-frame timing (`batch37_set_tft_feed`).
- **Changed expectations (deliberate, not weakened):** `ui_debug_menu_test` U2 — Diagnostics has 20 rows (was 19), the new row is an action row like its neighbours.
- **RED found by the new tests before they were green (first run):** the scenario label dropped `BYPASS` when music is unavailable (runtime P3) — a real bug in the new formatter, fixed; the first `A3C_PERF` format overflowed its buffer on an ordinary window (L1/L2) — compacted, and the buffer sized to one SD-log record; the runtime test itself miscounted the fixture's untimed first frame (R1/R2) — the expectation corrected.
- **Mutations:** `native/core/tools/a3_04c_mutation_check.py` → `native/core/a3-04c-mutation.log`, 28 mutants over the pump (K1–K5), the counters (C1–C5), the report and line (F1–F5), the runtime (R1–R5), the Developer row (U1–U2) and the device wiring the scans guard (D1–D6). **28 mutants, 28 killed by a failing check, 0 survived.** The first pass killed 26. F1 and F5 were INVALID: F1 left `contention_section` unused and F5 mismatched its format arguments, both under `-Werror`. Both were rewritten to compile and re-run (`a3-04c-mutation-rerun.log`): killed. Which check killed each one is in the log. For example, K2 ("the probe discards the synth output but the song advances") is caught only by P6, the bit-exact resume.
- **Full suite:** fresh build `native/core/build-a3-04c`, **serial ctest 139/139 passed in 138.85 s** (`native/core/a3-04c-ctest-pass1.log`): A3-HF1's 137 plus the two new targets. The only warning is the known w64devkit one. Every A3-04A/A3-04B audio test passes unchanged: the synth is not modified, so the goldens G1–G6 stand. So do the timing proofs (Camp, Blackthorn, Refuge, quake, shrine, combat-victory timelines; `batch51_*` pacing), persistence and gameplay parity.

### 20.9 Firmware

Pre-commit build `native/targets/tdeck/build-a3-04c` (`a3-04c-firmware-configure.log`, `a3-04c-firmware-build.log`): ESP-IDF 6.1, `idf.py --no-ccache reconfigure` then `ninja -j 4`, **first attempt clean, zero project warnings** under `-Werror`. The five `component_validation` notices are ESP-IDF's own.

- **Size: `0xecd90` = 970,128 B, +6,976 B** vs A3-HF1's 963,152 B; **`0x13270` = 78,448 B (7 %) free** in the 1 MiB app partition.
- Sections vs A3-HF1: `.iram0.text` 76,015 → 76,003 B (unchanged: nothing moved into or out of IRAM); `.dram0.bss` +288 B (the Board's per-frame timing, the contention counters, the SD-log atomics); `.dram0.data` +112 B; `.flash.text` +5,612 B; `.flash.rodata` +1,264 B. PSRAM: the report buffer 48 → 64 rows (+832 B).
- **Image checks on the A3-04C ELF:** `a3_04b_iram_check.py` **GREEN** (`a3-04c-iram-check.log`: the per-sample path, now with the probe's check in `render_block`, is IRAM/ROM only, no flash `.rodata`); `a3_04a_hotpath_check.py` **GREEN** (`a3-04c-hotpath-check.log`). The disassembly of `Board::tft_transmit` shows the two `rsr.ccount` reads around the unchanged `spi_device_transmit` call, and nothing else on the row path.
- Version string `3.0.0-alpha3-dev-a3-04c-debug` (`CMakeLists.txt` `PROJECT_VER`), so the identity screen and the Launcher file name cannot be mistaken for A3-HF1's.
- Not flashed. The post-commit image (a fresh directory, `--no-ccache`, which embeds the commit) is the one the annotated tag `alpha3-a3-04c-contention-map` names, with its path, size, SHA-256 and `Git`.

### 20.10 Hardware validation checklist (the user's; not done here)

**0. Identity (before any number is recorded).** Flash the A3-04C Launcher image named in tag `alpha3-a3-04c-contention-map` (`OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04c-Debug-Launcher.bin`; path, SHA-256 and `Git` in the tag message) through Launcher. Keep the same game pack and the same `openu5-audio.bin`; nothing is regenerated. The boot identity screen must read **`FW 3.0.0-alpha3-dev-a3-04c-debug`** and the tag's `Git` hash. If not, stop: the numbers would belong to another image. Please also note which image the original lag report was seen on, if known.

**Serial monitor (optional but preferred).** Connect USB-C to a PC and open the USB-Serial/JTAG port at 115200 (for example `idf.py -p COMx monitor` from an ESP-IDF shell, or any serial terminal). Without a PC, the same line is in `/sd/ultima5/logs/alpha20-frontend-debug.log`, and the Developer report shows everything on screen. Capture the **last `A3C_PERF` line before each window is read** (one every 5 s). Also capture the `PERF_REPORT` lines the read prints.

**How every run is measured (same for each).**
1. Set the run's settings (System Menu › Settings). For the probe: Alt+D › Diagnostics › *Probe: synth bypass* (four rows up from the top) → Enter until the row reads the wanted state; leave the menu with Back.
2. Start a clean window: Alt+D › Diagnostics › *Audio/render stats (live)* → Enter → **Enter again to dismiss**. That read starts the window, so discard that report.
3. Do the activity for **60 s**. Walk by holding the trackball in one direction, turning at obstacles. Do not open menus.
4. Read the window: Alt+D › Diagnostics › *Audio/render stats (live)*. **Photograph every page** (Down scrolls; the contention section is near the end) or copy the matching `PERF_REPORT` / last `A3C_PERF` line from serial.

**Runs** (same place, same direction, same speed; the overworld in open grassland/forest):

| Run | Music | SFX | Probe | Activity (60 s) |
|---|---|---|---|---|
| A | unavailable (remove `openu5-audio.bin`, reboot; put it back afterwards) | 80 % | off | walk |
| B | 80 % | 80 % | off | walk |
| B′ | 80 % | 80 % | **ON** | walk |
| C | **0 %** | 80 % | off | walk |
| D | 0 % | 80 % | off | walk into a wall / along a coast so the step/bump cues keep playing; then one fight |
| E | 80 %, then 0 %, then 80 % + probe ON | 80 % | as stated | the heaviest refresh: walk continuously along a **coastline with animated water in view** (viewport rewrite every step + animated cells); then enter and leave a town twice (full-screen redraws) |

Switch the probe **off** again at the end (it is never saved; a reboot also clears it).

**What to look at while walking.** Step-to-screen lag. Whether the map repaints in visible bands (top to bottom). Uneven step cadence. Music smoothness and any static. Keyboard/trackball responsiveness.

**What to send.** For each run: the photographed report pages (or the `A3C_PERF` line), plus one sentence on the visual/feel.

**Reading the result — PASS / FAIL.** No run "passes" the batch on its own. The batch passes when the runs are complete and consistent enough to decide §20.11. A run is **invalid** (repeat it) when its `scen=` label does not match the table, when fewer than 150 gameplay frames were drawn (`frame n`), or when it contains a menu visit. Audio health must hold in B, B′ and E: `und=0 hw=0 miss=0` (or 0–1 at a song switch). **FAIL (report at once)** if any run shows underruns or missed deadlines in steady play, audible static, a crash or reboot, or music that does not resume after the probe is switched off.

The decision table the numbers feed:

| Observation | Meaning |
|---|---|
| B's `frame avg/max`, `tft`, `vp` close to C's (within ~10 %) and to A's | the lag is **not music**; look at `split`: if `yld` is most of `tft`, it is the TFT's tick pacing (§20.6) → a renderer batch |
| B ≫ C and **B′ ≈ C** | the synth's work couples through the memory system; the row `fill`/`xfer` busy-vs-idle gap should show it |
| B ≫ C and **B′ ≈ B** | not the synth: the I2S DMA/ISR/wake cadence on core 0 → A3-04D (below) |
| `slow` transactions high and `sd=` max large, in every run | SD-log writes holding the TFT's SPI bus (music-independent) |
| `yld … late` > 0 mostly in B | core 0 not handed back at the tick while music runs |

### 20.11 Outcome, remaining hypotheses and the next batch

**Outcome C.** The measurements are in place. The remaining causes are narrowed to four, each with a device test that separates it: (a) the synth's residual memory-system coupling; (b) the I2S DMA/ISR/wake cadence on core 0; (c) music-independent renderer pacing (tick-paced TFT bands, per-frame logging, SD writes on the TFT's bus); (d) something not modelled, which would show as a row busy/idle gap without a B′/C difference. Ruled out from source: same-core competition, priority starvation, shared locks, allocation, audio-side logging, music work at 0 %.

**No production fix in this batch, and why.** Each candidate fix belongs to one hypothesis. Moving the I2S channel and its interrupt to core 1 (A3-04B §19.9's kept lever) fixes (b). Placing the FreeRTOS kernel in IRAM (`CONFIG_FREERTOS_IN_IRAM`, a global IRAM trade) touches (a). Changing the TFT's yield cadence or the per-frame log touches (c), which is the renderer and outside the audio batch's mandate. Making any of them now would change behaviour on a guess the device has not confirmed.

**A3-04D, scoped by the result** (one of):
- *B′ ≈ B ≫ C:* bring the I2S channel up from the audio task so its GDMA interrupt lands on core 1. Prove it by the same A3C runs, with the pump, buffering, synth and priorities untouched.
- *B′ ≈ C ≪ B with a row busy/idle gap:* locate the shared resource with the gap as the metric. Candidates: `CONFIG_FREERTOS_IN_IRAM`, the per-block flash functions into IRAM, the song events into internal RAM. One at a time, each measured.
- *B ≈ B′ ≈ C:* audio is cleared. A renderer batch (not audio) takes the TFT pacing (the 16-row `vTaskDelay(1)`) and the per-frame `PRESENTATION_DISPATCH` log, with its own evidence rules.

### 20.12 Files

- Core: `include/openu5/perf_report.h`, `src/perf_report.cpp` (`TftTiming`, `SdLogPerf`, `ContentionCounters`, `PerfScenario`, the report section, `format_contention_line`, 64-line report); `include/openu5/audio_stream.h`, `src/audio_stream.cpp` (the probe, task busy, `AudioPerfSource::set_music_bypass`); `include/openu5/ui_debug_menu.h`, `src/ui_debug_menu.cpp` (the probe row).
- Device: `tdeck_board.{h,cpp}` (timed transactions/yields, row classification), `tdeck_audio.{h,cpp}` (activity flag, probe), `sd_diagnostic_logger.{h,cpp}` (bursts), `alpha_runtime.{h,cpp}` (logic time, per-frame split, scenario, `A3C_PERF`, probe service), `main.cpp` (wiring, loop passes), `CMakeLists.txt` (`PROJECT_VER`).
- Tests / tools: `tests/a3_04c_contention_test.cpp`, `host_tests/a3_04c_contention_runtime_test.cpp`, `host_stubs/batch37_board_capture_stub.cpp` (TFT feed), `tests/ui_debug_menu_test.cpp` (20 rows), `core/CMakeLists.txt`, `tools/a3_04c_mutation_check.py`, `targets/tdeck/a3_04c_audio_flash_footprint.py`.

## 21. A3-04D — SD diagnostic logging and the TFT on the shared SPI bus

**The hardware result that opened this batch** (the user's A3-04C runs, same open-overworld walk; approximate values read off the Developer report):

| Run | audio CPU | CPU0 | CPU1 | main | frame max | TFT max | SD-log burst max |
|---|---|---|---|---|---|---|---|
| B — music 80 % | ~41 % | ~79 % | ~43 % | ~78 % | ~1175 ms | ~1137 ms | ~1159 ms |
| B′ — music 80 %, synth bypass | ~2 % | ~62 % | ~3 % | ~61 % | ~871 ms | ~833 ms | ~848 ms |
| C — music 0 % | ~1 % | ~59 % | ~2 % | ~58 % | ~1504 ms | ~1465 ms | ~1500 ms |

The 0.8–1.5 s frame/TFT stalls stay when the synth is bypassed and when music is off; C, with no music at all, has the longest. In every run the frame maximum sits within 4–23 ms of the SD-log burst maximum, and the TFT maximum 15–35 ms below it. By §20.10's decision table this is the "SD-log writes holding the TFT's SPI bus (music-independent)" row. The synth's own cost (B vs B′: CPU1 43 → 3 %) is real but does not explain second-long stalls.

**Status: MECHANISM PROVEN FROM THE SOURCE; PRODUCTION POLICY CHANGED (Option A: SD diagnostic logging OFF by default, one Developer keypress to turn on); HARDWARE CONFIRMATION PENDING.** A correlation alone was not accepted: §21.2 traces the path from a log line to a TFT row waiting on the bus, through the project's code and ESP-IDF's. The device ON/OFF comparison (§21.7) is the confirmation still to come. The batch is **not** hardware-validated until it passes.

### 21.1 Baseline (Phase 1)

- HEAD `3ce935b9` on `main` (A3-04C's post-commit logs), tree clean. Latest tags: `alpha3-a3-04c-contention-map` (`2a53128a`), `alpha3-hf1-arena-loot` (`d1465ed7`), `alpha3-a3-04b-render-contention`.
- Firmware at baseline: A3-04C, `0xecd90` = 970,128 B, 78,448 B (7 %) free.
- Fresh host build `native/core/build-a3-04d-base`: **serial ctest 139/139 passed in 135.17 s**, the one known w64devkit `stl_uninitialized.h` warning (`native/core/a3-04d-baseline-{configure,build,ctest}.log`).

### 21.2 The mechanism, traced (Phase 1)

| # | Question | Answer, with the evidence |
|---|---|---|
| 1 | Which bus? | **One: SPI2.** `pins::kSharedSpiHost = SPI2_HOST` (`tdeck_pins.h`) carries the ST7789 (CS GPIO 12, 40 MHz) and the SD card (CS GPIO 39, `kSdClockKhz = 800`, `tdeck_board.cpp`). Nothing else is on it. |
| 2 | How does the TFT use it? | From the game loop (`main`, prio 1, core 0): one blocking `spi_device_transmit` per pixel row in `Board::tft_transmit`. A 640-byte row takes 128 µs of clocking. |
| 3 | How does the SD card use it? | ESP-IDF `sdspi_host_start_command` (`esp_driver_sdspi/src/sdspi_host.c` 481–539) calls `spi_device_acquire_bus(portMAX_DELAY)`, then sends the command, **every data block and, after each block, `poll_busy` while the card programs** (`start_command_write_blocks`, lines 895–1022), and only then `spi_device_release_bus`. `spi_device_acquire_bus`: "Transactions to all other devices will be put off until `spi_device_release_bus` is called" (`spi_master.h`). The write timeout is `SDMMC_WRITE_CMD_TIMEOUT_MS` = **5000 ms** (`sdmmc_common.h`). **While the card is busy, no TFT row can move, and nothing in the port bounds how long the card may be busy short of 5 s.** |
| 4 | What does one log flush send? | newlib hands FATFS the 4 KiB stdio buffer; `f_write` (`fatfs/src/ff.c` 4160–4240) writes its whole sectors as **one multi-sector `disk_write`** (clipped at a cluster boundary), i.e. one CMD25 of 8 × 512 B. **At 800 kHz that is ≥ 41 ms of clocking in a single bus hold**, before the card's programming time after each block. At a cluster boundary FATFS also reads a FAT sector and later writes it to both FATs (`use_one_fat = false`), each ≥ 5 ms at 800 kHz. |
| 5 | Which task writes, and is it synchronous? | `alpha20-sd-log`: `tskIDLE_PRIORITY`, **unpinned**, 4 KiB stack. Its `fwrite`/`fflush` are synchronous down to the card. It wakes for every queued line and at least every 250 ms, takes the storage mutex **on every wake**, and flushes every 2 s, on every "important" line (`INPUT_EDGE`, `KEYBOARD_METRICS` in each 5 s heartbeat, any `" W ("`/`" E ("` line, …), and whenever the 4 KiB buffer fills. The game thread does not touch the card. It formats a 768-byte copy of each log line (`vsnprintf`) and queues it (784 B into PSRAM). |
| 6 | How much is written while walking? | The host runtime logs about 5 INFO lines (≈ 0.4–0.5 KB) per step (`INPUT_ROUTE`, the command, the render line, `PRESENTATION_DISPATCH`, …). The 5 s heartbeat adds `AUDIO_PERF`, `RENDER_PERF`, `SYS_PERF`, `A3C_PERF` (≤ 719 B) and `KEYBOARD_METRICS`. A steady walk therefore fills the 4 KiB buffer every few seconds, on top of the 2 s periodic flushes. |
| 7 | Does anything else use the card during a walk? | **No.** `main.cpp` closes both packs after the PSRAM load ("Resource packs closed after PSRAM/cache load"), the audio pack is read once at boot, and saves/settings touch the card only when asked. During the test walk, the diagnostic log is the TFT's only competitor for SPI2. |
| 8 | Is there an fsync, close/reopen or metadata write per flush? | No fsync: `CONFIG_FATFS_IMMEDIATE_FSYNC` is off, so `vfs_fat_write` does not call `f_sync`. The log is closed only at rotation (512 KiB) or on an error. Metadata costs come from FAT updates at cluster boundaries, and rotation (unlink + rename + create) happens once per 512 KiB. |
| 9 | Can the logger block the TFT? | **Yes, for the whole of every card command (item 3).** A3-04C's burst timer brackets exactly those commands: from the storage mutex taken to the mutex given back. |

So the maxima line up because one frame's TFT write sits behind one long burst: the frame starts a few tens of ms into the burst, and its remaining rows go out as soon as the bus is released. Frame − burst = +16 / +23 / +4 ms and TFT − burst = −22 / −15 / −35 ms in B / B′ / C. The durations themselves come from the card (programming, garbage collection), which the source cannot predict. The device run measures them directly (§21.4, `insd`).

**Two side findings from the same trace:**
- **The SD log was less durable than it looked.** FATFS writes a file's size into its directory entry only in `f_sync` / `f_close` (`ff.c` 4256–4320), and A3-04C's logger closed the file only at rotation. After a power-off, the directory still records the size from the last rotation or boot, so a session's lines may not be readable afterwards. That fits the user's experience that the log was never important day to day. A3-04D closes the log when logging is switched off.
- **Serial is safe to keep.** With no host reading, ESP-IDF's USB-Serial/JTAG console busy-waits at most once for 50 ms after the FIFO fills, then drops bytes until a host reads again (`usb_serial_jtag_vfs.c` 148–170). It cannot produce second-long stalls.

### 21.3 The change: a Developer switch, OFF by default (Phases 2 and 5, Option A)

**Policy: diagnostic SD logging is off at boot.** `openu5::kSdDiagLoggingDefault = false`. **Developer › Diagnostics › *Probe: SD diag logging*** turns it on for the session. It is never saved, and a reboot turns it off. Serial logging is unchanged in every state.

- **The core log (`openu5::SdDiagLog`, `sd_diag_log.{h,cpp}`).** A3-04C's writer logic moved unchanged into core, so host tests run the device's own logic against a fake card: the same `[%010llu] ` prefix on every physical line, rotation at 512 KiB, the 2 s and "important"-line flushes, and the drop notice. Added to it: the switch; `mirror()`, the log hook's body (**serial always**; the card copy is formatted and queued only while on); and `wake()`, which **touches nothing while off with the file closed**.
- **The device writer** (`sd_diagnostic_logger.cpp`) keeps the FreeRTOS queue, task and storage mutex, stdio on FATFS, and A3-04C's burst timing. While the log is idle it **sleeps in `ulTaskNotifyTake(portMAX_DELAY)`**: no 250 ms wake, no mutex, no flush, no card access. Switching on notifies it. The next wake opens the log for append (a full log becomes the archive first), and from then on the cadence is A3-04C's exactly. Switching off: the next wake writes what was captured while on and closes the log (so the directory entry gets its size), then the writer sleeps again. A card error closes the log and switches it off for good (`n/a`); serial continues.
- **Boot no longer opens or rotates the log.** `initialize_storage()` only records that the card is mounted. The `logs` directory is created when logging is first switched on.
- **The row** is inserted **above** *Probe: synth bypass*, five rows up from the top. Every older row keeps its place counted from the end (the synth bypass is still four up, the test tone still the last). It reads `Probe: SD diag logging: off` / `ON` / `n/a`. Enter toggles it; with no card or writer it says `SD diag logging: no SD card logger on this device` on the Developer screen. Switching logs `A3D_PROBE sd_log=0|1` to serial.
- **Labels:** the scenario gains ` sdlog ON` / ` sdlog OFF` / ` sdlog n/a`, so it is on every report and every `A3C_PERF` line. It is left out only when no logger reports a state (the host's A3-04C label is byte-identical). While logging is off, the report says `sd log OFF: n bursts …` and the line `sd=OFF:n:max/total`, so zero counters read as "off", not as stale.
- **New attribution (the within-run proof).** The Board reads the writer's burst word (`sdlog::burst_flag()`, 1 while the writer holds the storage mutex) at both ends of every TFT transaction, as it reads the audio task's flag. A slow transaction (> 1 ms) during a burst is counted, and the longest is kept. Report line `slow in sd-log burst N max M ms`; `A3C_PERF` field `insd=N:M` after `slow=`. That costs two word loads per transaction.

**Why Option A is the smallest safe fix.**
- *Option B (buffer or defer)* cannot remove the stall. Whatever the buffering, a 4 KiB write at 800 kHz holds the TFT's bus for ≥ 41 ms, and then for the card's busy time. Removing that means changing the SD clock, the card's bus or the display's ownership of it, all outside this batch.
- *Option C (delete the logger)* would remove a tool the checklists still name (`ALPHA2_HARDWARE_CHECKLIST.md` asks for `/ultima5/logs/` on a FAIL).
- Option A leaves the logger one keypress away, adds no subsystem, and changes nothing the game, the saves or the audio do.

**What does not change:** gameplay; saves (the storage transaction does not depend on the switch, and an idle writer never takes the mutex); settings; the packs (loaded at boot, never through the logger); the smoke-test log (`smoke-tests.log`, written by the smoke tests themselves, which create their own directory); serial logging; music selection/sequencing, SFX, the audio buffering and the synth; TFT rendering and its 16-row `vTaskDelay(1)` yields; the main loop's `vTaskDelay(pdMS_TO_TICKS(5))`; input. All A3-04C metrics are kept.

### 21.4 Output formats

The report's contention section (values illustrative, logging off):

```
-- Contention map (A3-04C) --
music 80% Ultima V Theme sfx 80% sdlog OFF     <- scenario + the log's state
...
xfer max 0.30 ms slow 0 (0 w/audio)
slow in sd-log burst 0 max 0.0 ms              <- A3-04D: slow TFT transactions during an SD-log burst
...
sd log OFF: 0 bursts max 0.0 total 0.0 ms      <- "sd log N bursts ..." while on
```

`A3C_PERF` (serial; also the SD log while it is on), A3-04C's line with three additions:

```
A3C_PERF scen=[music 80% Ultima V Theme sfx 80% sdlog OFF] win=60.1s | frame n=... | ... | xfer max=0.30 slow=0/0 insd=0:0.0 | rows=... | ... | sd=OFF:0:0.0/0.0 | cpu0=... sdl=0
```

`insd=n:max` = slow TFT transactions with an SD-log burst in progress : the longest (ms). `sd=OFF:` prefix = logging switched off. The worst realistic line is 714 characters (≤ 719, one SD-log record with its prefix; `a3_04d_sd_log` F7).

### 21.5 Tests, RED / GREEN and mutations

- **New targets:** `a3_04d_sd_log` (40 checks: D the policy, C the log hook (serial always), W the writer against a fake card that records every operation, K the attribution counters, F the labels / report / line, S the device wiring and the unrelated SD users) and `a3_04d_sd_log_runtime` (9 checks through the real `AlphaRuntime` with raw keys: the row five up, the older rows' places, on/off through the device hooks backed by the core log, `sdlog ON/OFF` on the report and the line, `n/a` with no logger with A3-04C's label unchanged, and the game untouched).
- **Key behavioural proofs:** W1: logging OFF through 60 s of gameplay-rate logging, **the card is never touched** (0 opens, writes, flushes, closes or bursts), the writer stays idle, and serial got all 624 lines. C1: serial receives every call formatted exactly, whatever the state. W6: switching off writes what was captured, closes once, then no wake at all. W2–W5, W9, W10: while on, the A3-04C behaviour (prefix, flush cadence, rotation, drop notice) is unchanged. S9: saves, settings, smoke tests and packs do not depend on the log.
- **Changed expectation (deliberate, not weakened):** `ui_debug_menu_test` U2: Diagnostics has 21 rows (was 20). The new row is an action row like its neighbours, and the test tone is still the last row.
- **RED-first:** the device-wiring scans run against the **unmodified A3-04C device sources** (`git show HEAD:…` into scratch, `a3-04d-red-device-scans-vs-a3-04c.log`): **S1–S8 RED** (the writer never sleeps, the hook queues unconditionally, boot opens the log, there is no switch, no attribution, no wiring). S9 is GREEN on both trees, as a preservation guard must be. Its failure mode is proven by mutants D9/D10. The core and runtime checks use the new API, so they are proven by mutation.
- **First-run REDs:** F8 expected the SD-log line to be the report's last. The guard section follows the contention section (A3-04C's F4 already says so), so the expectation was corrected to "the last line is the guard's". No production change came from it.
- **Mutations:** `native/core/tools/a3_04d_mutation_check.py` → `native/core/a3-04d-mutation.log`. **32 mutants, 32 killed by a failing check, 0 survived, 0 invalid**, all on the first pass. They cover the policy and the core log (M1–M11: the default on, capture or serial ignoring the switch, a wake that touches the card while off, no close on switch-off, captured lines dropped, a writer that never idles, no availability or failure guard, and A3-04C's flush cadence and prefix changed), the labels and counters (F1–F6), the row (U1–U2), the runtime (R1–R3), and the device wiring the scans guard (D1–D10). D9/D10 make a save and the smoke tests depend on the log, and are what proves S9 can fail. The log says which check killed each one. For example, M1 (the log on at boot, i.e. A3-04C's behaviour) turns 10 core checks and 7 runtime checks RED.
- **Full suite:** fresh build `native/core/build-a3-04d`, **serial ctest 141/141 passed in 137.35 s** (`native/core/a3-04d-ctest-pass1.log`): A3-04C's 139 plus the two new targets. The only warning is the known w64devkit one. A3-04C's own contention tests pass unchanged, including S8 (the burst timing) and the L/F format checks.

### 21.6 Firmware

Pre-commit build `native/targets/tdeck/build-a3-04d` (`a3-04d-firmware-configure.log`, `a3-04d-firmware-build.log`): ESP-IDF 6.1, `idf.py --no-ccache reconfigure` then `ninja -j 4`, **first attempt clean, zero project warnings** under `-Werror`. The five `component_validation` notices are ESP-IDF's own.

- **Size: `0xed5e0` = 972,256 B, +2,128 B** vs A3-04C's 970,128 B; **`0x12a20` = 76,320 B (7 %) free** in the 1 MiB app partition.
- Sections vs A3-04C: `.iram0.text` 76,003 → 76,003 B (**unchanged**); `.dram0.data` +32 B; `.dram0.bss` +32 B; `.flash.text` +1,736 B (the core log, the Developer row, the labels); `.flash.rodata` +356 B.
- **Stack (`-fstack-usage`):** the writer's deepest path (writer task → drop notice → line → `fwrite`) is **1,840 B before and after** (A3-04C: 832 + 912 + 64 + 32; A3-04D: 832 + 48 + 816 + 80 + 32 + 32) on its 4 KiB stack. The log hook's frame is +48 B per log call (A3-04C 864 B; A3-04D 64 + 848 B).
- **Image checks on the A3-04D ELF:** `a3_04b_iram_check.py` **GREEN** (`a3-04d-iram-check.log`) and `a3_04a_hotpath_check.py` **GREEN** (`a3-04d-hotpath-check.log`). The audio path is untouched.
- Version string `3.0.0-alpha3-dev-a3-04d-debug` (`CMakeLists.txt` `PROJECT_VER`), so neither the identity screen nor the Launcher file name can be mistaken for A3-04C's.
- Not flashed. The post-commit image (a fresh directory, `--no-ccache`, which embeds the commit) is the one the annotated tag `alpha3-a3-04d-sd-log-isolation` names, with its path, size, SHA-256 and `Git`.

### 21.7 Hardware validation checklist (the user's; not done here)

**0. Identity first.** Flash the A3-04D Launcher image named in tag `alpha3-a3-04d-sd-log-isolation` through Launcher: `native/targets/tdeck/build-a3-04d-post/launcher/OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04d-Debug-Launcher.bin` (its size, SHA-256 and `Git` are in the tag message). Same game pack, same `openu5-audio.bin`: nothing is regenerated. The boot identity screen must read **`FW 3.0.0-alpha3-dev-a3-04d-debug`** and the tag's `Git` hash. If it does not, stop: the numbers would belong to another image.

**Serial (preferred).** USB-C to the PC. From an ESP-IDF PowerShell (after `. C:\esp\v6.1\esp-idf\export.ps1`), run `python -m esp_idf_monitor -p COMx -b 115200 --no-reset`, with COMx being the T-Deck's USB-Serial/JTAG port. `--no-reset` keeps the connect from rebooting the device. This standalone monitor touches no build directory; an `idf.py … monitor` pointed at a build dir can reconfigure that dir from the current sources. Any serial terminal on COMx at 115200 also works. Save the last **`A3C_PERF`** line before each read, and the **`PERF_REPORT`** lines the read prints. Also save the `A3D_PROBE sd_log=…` line when you switch. Without a PC, photograph the report pages. Logging is off in Tests 2–4, so the SD card will not have these lines.

**Common procedure (every test):**
1. Boot, load the same save, stand in the **same open overworld grassland/forest spot as A3-04C**, facing the same way.
2. Set the test's settings (System Menu › Settings for volumes; Alt+D › Diagnostics for the probes). The *Probe: SD diag logging* row is **five up** from the top of Diagnostics; *Probe: synth bypass* is **four up**. Enter until the row reads the wanted state, then Back out.
3. Start a clean window: Alt+D › Diagnostics › *Audio/render stats (live)* (two up) → Enter → **Enter again to dismiss**. That read starts the window, so discard it.
4. **Walk for 60 s** by holding the trackball in one direction, turning at obstacles, at the same speed as A3-04C. Open no menus.
5. Read the window: Alt+D › Diagnostics › *Audio/render stats (live)* → Enter. **Photograph every page** (Down scrolls; the contention section is near the end), or save the `PERF_REPORT` lines and the last `A3C_PERF` line.

A run is **invalid** (repeat it) if its scenario label does not match the table, if `frame n` < **150**, or if it included a menu visit.

| Test | Music | SFX | Probe: SD diag logging | Probe: synth bypass | Scenario label must read |
|---|---|---|---|---|---|
| **1** | **0 %** | 80 % | **ON** | off | `music 0% (silent) sfx 80% sdlog ON` |
| **2** | **0 %** | 80 % | **off** (boot default) | off | `music 0% (silent) sfx 80% sdlog OFF` |
| **3** | **80 %** | 80 % | **off** | off | `music 80% <song> sfx 80% sdlog OFF` |
| **4** (strongly preferred) | **80 %** | 80 % | **off** | **ON** | `music 80% <song> sfx 80% BYPASS sdlog OFF` |

Run 2 first, straight after boot, to confirm the default. Then 1 (switch the log on), then off again for 3 and 4. Switch the synth bypass off at the end. A reboot also clears both probes, and the log is off after it.

**Pages / fields to record for each test:** the scenario line; `frame avg/p95/max`, `late`; `tft avg/max`; `viewport … tft avg/max` and `other …`; `cadence avg/max`; `input` (in) avg/max; `tft/frame fill / xfer / yield`; `yield … late`; `xfer max … slow … (… w/audio)`; **`slow in sd-log burst N max M`**; `sd log …` (bursts / max / total); `audio task/blk`, `und/hw/miss`; CPU0 / CPU1 / main / aud / sdl %. The `A3C_PERF` line has all of them.

**While walking, note:** step-to-screen lag, visible banding, uneven cadence, music smoothness or static, input responsiveness.

**PASS (the SD log is the culprit and the fix works):**
- **Test 1 reproduces the stalls:** a frame/TFT max of hundreds of ms or more, with `sd log` max close to it, and **`insd` max ≈ `xfer max` ≈ the SD burst max**. This is the direct proof that one TFT transaction waited out the burst.
- **Test 2 removes them:** `sd=OFF:0:0.0/0.0`, `insd=0:0.0`, `xfer max` ≲ 1–2 ms, and **frame max / TFT max drastically lower than Test 1 and than A3-04C's C (~1.5 s)**. Expected: roughly the viewport write's ~90 ms tick-paced cost plus logic/compose, i.e. well under ~250 ms.
- **Test 3:** no new audio underruns (`und=0 hw=0 miss=0`, 0–1 at a song switch), music correct and smooth, frame/TFT max in the same range as Test 2 (no second-long spikes).
- **Test 4:** as Test 3 with CPU1 near idle. The Test 3 − Test 4 difference is the synth's remaining cost, now measured without SD noise.
- **No regressions:** saves and loads work (save once, load once), the game starts with the same packs, SFX work.

**FAIL / do not declare victory:** if **Test 2's frame/TFT max is still in the 0.8–1.5 s range**, SD logging is not the (only) cause. Report the Test 2 pages; do not tag. Also FAIL on audio underruns in steady play, audible static, a crash or reboot, or any save/load problem.

### 21.8 Outcome and the next step

**Outcome (host):** the mechanism is proven from the source, and production now keeps diagnostic SD logging out of normal play by default. **Pending:** Tests 1–4.

The next step is chosen by the device result:
- **Test 2 passes (stalls gone):** SD logging was the catastrophic-stall source. The next performance step is **renderer cadence/yield cleanup** (the 16-row `vTaskDelay(1)`: ~9 ticks ≈ 90 ms per viewport write, drawn in bands, §20.6), measured against Test 2/3 as the new baseline. It comes before synth optimisation because it sets the frame time with or without music. The synth's residual cost (Test 3 vs Test 4) decides whether synth work follows.
- **Test 2 still shows ~1 s stalls:** the SD log is not the cause. Report, don't fix. The `insd`/`slow`/`yield late` fields then point at what held core 0 or the bus.

### 21.9 Files

- Core: `include/openu5/sd_diag_log.h`, `src/sd_diag_log.cpp` (new: the switch, the hook body, the writer's wakes); `sources.cmake`; `include/openu5/perf_report.h`, `src/perf_report.cpp` (`SdLogState` in the scenario, `SdLogPerf::off`, `TftTiming::slow_xfers_sd`/`xfer_max_sd_cycles`, the report line and `insd=`/`sd=OFF:`); `include/openu5/ui_debug_menu.h`, `src/ui_debug_menu.cpp` (the row).
- Device: `sd_diagnostic_logger.{h,cpp}` (the core log wired to FreeRTOS/stdio; sleeps while idle; `state`/`set_enabled`/`burst_flag`), `tdeck_board.{h,cpp}` (SD attribution), `alpha_runtime.{h,cpp}` (hooks, scenario, the probe service), `main.cpp` (wiring, boot message), `CMakeLists.txt` (`PROJECT_VER`).
- Tests / tools: `tests/a3_04d_sd_log_test.cpp`, `host_tests/a3_04d_sd_log_runtime_test.cpp`, `tests/ui_debug_menu_test.cpp` (21 rows), `core/CMakeLists.txt`, `tools/a3_04d_mutation_check.py`.

## 22. A3-04E — renderer cadence, TFT yields and main-loop scheduling

**The hardware result that opened this batch** (the user's A3-04D runs, SD logging off, same open-overworld walk; values read off the Developer report):

| Test | Music | frame avg / max | TFT avg / max | viewport TFT avg / max | xfer max | slow | CPU0 | CPU1 | main | audio |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 % | 79.2 / 443.0 | 41.1 / 404.4 | 122.0 / 404.4 | 0.60 | 0 | 74 % | 1 % | 73 % | — |
| 3 | 80 % | 101.3 / 443.6 | 63.4 / 405.4 | 122.6 / 405.4 | 0.84 | 0 | 69 % | 43 % | 68 % | 42 % (3.35 ms/blk, no misses) |
| 4 | 80 %, synth bypass | 81.7 / 444.7 | 43.7 / 408.1 | 121.6 / 408.1 | 0.55 | 0 | 73 % | 2 % | 72 % | 2 % |

The SD stalls are gone (A3-04D confirmed). The ~400–445 ms worst case and the ~122 ms viewport average are **the same with and without the synth**, and the user remembers banded redraws from before music existed. The SPI transfers themselves are fast (worst transaction 0.55–0.84 ms, none slow).

**Status: OUTCOME A (host) — both stalls removed in production, each behind its own legacy probe so the device can measure them one at a time; HARDWARE VALIDATION PENDING.** The ~400 ms is not a stall at all. It is a deterministic, tick-quantised repaint, and the host reproduces it with the real Board code (§22.2). The draw loops now yield where they used to sleep to the next tick. An idle pass of the main loop now blocks on the input queue for one tick instead of spinning. Nothing the game does, draws, plays or saves changes, and the task placement and the tick rate are untouched.

*Hardware (A3-04E.1 closeout, 2026-09-27, §23.10): validated on the A3-04E.1 image, which carries this pacing unchanged. Legacy pacing on the device: the menu-exit repaint took 441–442 ms and walking steps ~155 ms. The yield pacing: ~184 ms and ~88 ms. The TFT full-screen maximum fell from 408.7 ms to 148.8 ms, and the frame maximum over an 11-minute default-pacing run was 186.1 ms. A3-04E's own image is not hardware-valid (the watchdog trip, §23); A3-04E.1 is the validated image.*

### 22.1 Baseline (Phase 1)

- HEAD `63c9e13b` on `main` (A3-04D's post-commit logs), tree clean. Latest tag `alpha3-a3-04d-sd-log-isolation` (`5cd8fd0b`).
- Firmware at baseline: A3-04D, `0xed5e0` = 972,256 B, 76,320 B (7 %) free.
- Fresh host build `native/core/build-a3-04e-base`: **serial ctest 141/141 passed in 136.13 s**, the one known w64devkit warning (`native/core/a3-04e-baseline-{configure,build,ctest}.log`).

**Scheduling map** (source, `sdkconfig`, ESP-IDF 6.1):

| Context | Core | Prio | How it waits |
|---|---|---|---|
| `main` — game logic, composition **and every TFT transaction** | 0 | 1 | Up to A3-04D: never, except inside the TFT write. The loop ended with `vTaskDelay(pdMS_TO_TICKS(5))` = `vTaskDelay(0)` at `CONFIG_FREERTOS_HZ=100`, which only reschedules (FreeRTOS `tasks.c`: "A delay time of zero just forces a reschedule") |
| `openu5-input` | 0 | 4 | `xTaskNotifyWait` (GPIO interrupt, 1-tick fallback); posts `RawInputEvent`s to a 64-entry queue that `main` drains with zero timeout. It preempts `main` whenever it is ready |
| `openu5-audio` | 1 | 3 | `i2s_channel_write` (DMA-paced, 8 ms blocks) or a 20 ms idle queue wait (A3-04A..D, unchanged) |
| `alpha20-sd-log` | any | 0 | `ulTaskNotifyTake(portMAX_DELAY)` while logging is off (A3-04D) |
| IDLE0 / IDLE1 | 0 / 1 | 0 | the task watchdog checks both (5 s, warning only, no panic) |
| `esp_timer`, `ipc0/1` | 0 / both | 22 / 24 | system |
| `Tmr Svc` | any | 1 | FreeRTOS software timers: the project uses none |
| TFT write | inside `main` | — | one **interrupt transaction per pixel row** (`spi_device_transmit`, SPI2 at 40 MHz, DMA from internal RAM). It blocks: `spi_device_get_trans_result` waits in `xQueueReceive` on the result queue (`esp_driver_spi/src/gpspi/spi_master.c:1267`, "block until return"), so IDLE0 runs during every row's DMA |

**Where the renderer paused** (all in `tdeck_board.cpp`): `fill_rect` after every 32nd chunk; `draw_rgb565_strided`, `draw_rgb565_scaled` and `draw_text_box` after rows 16, 32, …; never in `draw_text_box_metrics` or the sky strip. Every pause was `vTaskDelay(1)` inside the timed `Board::tft_yield` (A3-04C), i.e. a sleep until the next 10 ms tick.

### 22.2 The pacing cost, reconciled (Phase 2)

The pauses are counted **in the real Board code**. `tdeck_board.cpp` now builds on the host over `host_tests/board_shims`, a fake ST7789 that decodes the byte stream into a 320×240 panel, and the real `AlphaRuntime` drives it (`a3_04e_pacing_runtime`). The pause breakdown below is what that test prints. The timing model uses ESP-IDF's documented ESP32-S3 interrupt-transaction cost (`docs/en/api-reference/peripherals/spi_master.rst`, "Transaction Duration": 26 µs via DMA, 24 µs via CPU, measured with `CONFIG_SPI_MASTER_ISR_IN_IRAM` as this firmware has it), the bits at 40 MHz, and `vTaskDelay(n)` = sleep to the n-th next 10 ms tick. It does **not** model building the rows (the device's `fill`), so a modelled time is a lower bound.

| Frame | Pauses (real Board, host) | Tick-sleep floor | Modelled legacy TFT | Device A3-04D | Modelled A3-04E |
|---|---|---|---|---|---|
| **Leaving the Developer screen** (full-screen repaint) | **37** = clear 320×240 **7** + viewport 176×158 **9** + right-panel reflow 138×240 **7** + the two 2×180 viewport-frame sides **5 + 5** + the party and world frame sides (1×52, 1×32, twice) **1+1+1+1**; 1,717 transactions | ≥ 36 ticks = 360 ms | **391.7 ms** | TFT max **404.4 / 405.4 / 408.1** | **113.8 ms** |
| **A walking step** (viewport rewrite + panels) | **9** (after rows 16 … 144 of 158); 306 transactions | ≥ 8 ticks = 80 ms | **100.3 ms** | viewport avg **~122** (this average also includes the one repaint, and row building) | **27.7 ms** |

Why the maximum is the same ~443 ms in every A3-04D test: **every measurement window starts inside the Developer menu** (the *Audio/render stats (live)* read resets it), so its first gameplay frame is always this full-screen repaint. The repaint's cost is set by the tick, not by what is playing. It is not an SPI stall: the transfers are ~1,717 × ~30 µs plus the bytes. The frame max (443 ms) is the TFT max (404 ms) plus that frame's composition. Two of the 37 pauses show a pre-existing inefficiency: `fill_rect` chunks a rectangle by its width, so a 2-pixel-wide, 180-row frame line is 180 transactions of 4 bytes each, which is 5 pauses by itself. With tick sleeps that cost 50 ms per line; with yields it costs ~5 ms (§22.11).

The banding: with a pause every 16 rows and each pause waiting ~8 ms for the tick, the 158-row viewport arrives in **10 bands over ~90 ms**, a visible top-to-bottom wipe, music or not. Without the sleeps the same rows arrive in ~20–30 ms. That is still a top-to-bottom raster (the panel has no tearing-effect sync here), but it is 3–4× faster and has no pauses in it.

### 22.3 Why the yield existed (Phase 3)

It came in with the Alpha 2.0 import (`c18f5b64`, 2026-09-18) in `fill_rect`, `draw_rgb565_strided`, `draw_rgb565_scaled` and `draw_text_box`, with no comment, test or document. The Board before it had no pauses. Every candidate reason was checked against the source:

| Candidate reason | Finding |
|---|---|
| Task-watchdog / idle starvation | The watchdog watches IDLE0/IDLE1. **A draw cannot starve IDLE0**: every row blocks in `spi_device_transmit`'s result-queue wait, and IDLE0 runs there. What *can* starve it is the loop spinning between frames (`vTaskDelay(0)`), which the pauses never touched. `DEBUG51.md` already flagged `pdMS_TO_TICKS(5)` = 0 as an idle-starvation hypothesis. The A3-04E idle wait addresses exactly that |
| Input servicing | The input task has priority 4 and preempts the priority-1 game thread whenever it is ready. A sleep in `main` gives it nothing. Draining happens in `main` itself, so sleeping mid-draw only delays it |
| Audio servicing | The audio task is on core 1; the game→audio queues are zero-timeout sends |
| Another task starving | Only priority ≤ 1 tasks on core 0 could benefit: IDLE0 (above), `Tmr Svc` (unused) and the unpinned idle-priority SD writer, which runs on core 1 anyway |
| TFT driver / DMA completion | Every transaction is synchronous; there is nothing to let complete |
| Long critical sections | None in the draw loops |
| Visible artifacts | The sleep *causes* the bands |

*Correction (A3-04E.1, 2026-09-27, §23): the first row of this table is wrong as a guarantee. Hardware tripped the task watchdog on IDLE0. A row's wait lets the idle task in, but nothing makes it finish the pass whose hook feeds the watchdog, so the sleeps were doing that job. §23 adds an explicit, observed idle-service guarantee.*

**Conclusion: an inherited conservative scheduling choice.** No requirement for it was found. It most likely dates from before the input task existed ("the native app polls and redraws synchronously in app_main", `DEBUG51.md`), but the history does not say so, and that is recorded as unclear. That is why the legacy pause stays one keypress away, rather than being deleted.

### 22.4 Candidate renderer pacings (Phase 4)

| Candidate | Effect (host model, real Board) | Verdict |
|---|---|---|
| **A — `taskYIELD()` at the same points** | 0 tick sleeps; the 37 + 9 pauses become reschedules (modelled at 1 µs; nothing else of priority 1 is ready on core 0); repaint 391.7 → 113.8 ms, step 100.3 → 27.7 ms | **selected** |
| B — pause less often (every 64 rows) | viewport 2 sleeps, but the repaint still sleeps ~17 times (fill chunks, frame lines): ~170 ms, still tick-quantised | rejected: halves the cost, keeps the mechanism |
| C — pause only when needed | nothing on core 0 needs the game thread to sleep (§22.3), so "when needed" is never; it collapses into A/D | rejected (no consumer) |
| D — no pause | the same timing as A | A keeps an explicit cooperative point for free, so a future priority-1 peer on core 0 still gets its turn every 16 rows |

The cadence itself is unchanged (`openu5::tft_row_yield_due` / `tft_chunk_yield_due` are Alpha 2.0's expressions, pinned by P2). Only what happens at each point changed, through `openu5::tft_pause_ticks(TftPacing)`.

**The Phase 6 matrix, as far as a host can measure it** (`a3_04e_pacing_runtime`: the A3-04C/D procedure on the modelled bus, 12 steps; transfers and pauses only):

| Configuration | Repaint TFT | Step TFT (avg / max) | Pause time / viewport frame | Frame max |
|---|---|---|---|---|
| Baseline (TFT tick, loop spin) = A3-04D | 391.7 ms | 100.3 / 107.6 ms | 88.3 ms | 391.7 ms |
| Renderer candidate (TFT yield, loop spin) | 113.8 ms | 27.7 / 31.4 ms | 0.0 ms | 113.8 ms |
| Main-loop candidate (TFT tick, loop idle-wait) | as baseline | as baseline | as baseline | as baseline |
| Combined (A3-04E) | 113.8 ms | 27.7 / 31.4 ms | 0.0 ms | 113.8 ms |

`main.cpp` does not run on the host, so the main-loop candidate's effect (CPU0 / `main` %, the loop's sleep) exists only on the device: runs A0/A2 of §22.10 measure it. The host proves its policy and gate (P4–P6, L1–L6, S4–S6).

### 22.5 The main loop (Phase 5)

`vTaskDelay(pdMS_TO_TICKS(5))` is 0 ticks at 100 Hz, so the game thread never slept between frames. The smallest correct fix that meets the requirements:

- **An idle wait on the input queue** (`InputHardware::wait_for_event` = `xQueuePeek(event_queue_, &event, ticks)`) for **`openu5::kLoopIdleWaitTicks` = 1 tick**. This is a tick count, not milliseconds, so no tick rate rounds it to zero (P5). A queued event ends the wait at once, so input pays **no** added latency. The event stays queued for `poll()`, which counts and logs it.
- **A gate, `AlphaRuntime::loop_may_sleep()`.** A one-tick sleep makes a service at most one tick late. That is harmless for everything driven by the absolute clock: the 55 ms animation and ambient ticks, the quake / world-fx / invert / map-reveal windows, the input-hold threshold, the frontend. But some services schedule each step relative to the moment the previous one was serviced. The scene pacers use `resume_at = now + dwell`, the combat enemy beat uses `now + 400 ms`, and the poison flash, Camp, the audio benchmark and the smoke tests do the same, so a late step would push all later ones back. While any of them is live, or a frame is still owed, the loop does not sleep and their timing is exactly A3-04D's (L6: never once during a paced Camp).
- Otherwise `vTaskDelay(0)`, as before. The not-ready error screen waits one tick.

Rejected: a higher `CONFIG_FREERTOS_HZ` (a global change to every tick-based wait, and to the legacy pause's length); a deadline-driven loop (every service would have to publish its next deadline). No busy-wait anywhere (S5).

### 22.6 The device A/B: two Developer probes

**Developer › Diagnostics › *Probe: legacy TFT pacing*** (seven up from the top) and ***Probe: legacy loop spin*** (six up), inserted above *Probe: SD diag logging* so every older row keeps its place counted from the end (SD log five up, synth bypass four up, benchmark three, stats two, tone one). Each row reads `off` (A3-04E, the boot default) or `ON` (the A3-04D behaviour). Neither is saved; a reboot is A3-04E. Switching logs `A3E_PROBE tft=… loop=…`. Both ON is exactly A3-04D's pacing, so the baseline, each candidate alone and the combination run on **one image**.

### 22.7 Instrumentation (kept, and added)

Everything from A3-04B/C/D is kept, and `A3C_PERF` is **byte-identical** in format (F7), so old and new runs diff field by field. Added:

- `TftTiming::full_screen`: the Board marks the two gameplay full-screen repaints (the first game frame; leaving the Developer screen).
- `ContentionCounters`: full-screen repaints apart from the other viewport frames; pauses per viewport frame and their time; the window's total pause time; the loop's idle waits (count, avg, max, total asleep, input wakes) and passes per second.
- A report section, last before the OPL2 line (values illustrative):

```
-- Pacing (A3-04E) --
tft yield  loop idle-wait                      <- "tft TICK  loop SPIN" = the legacy probes ON
full-screen 1 frm tft avg 113.8 max 113.8      <- the menu-exit repaint
viewport w/o full 40 frm avg 27.7 max 31.4     <- walking
pauses/vp frm 9.0 = 0.0 ms  all 0.1 ms         <- legacy: 9.0 = ~88 ms
loop 95/s  waits 4800 avg 9.8 max 10.2 ms
loop asleep 47.0 of 60.0 s  input wakes 12
```

- One `A3E_PACE` line per 5 s heartbeat, after `A3C_PERF` (worst case 534 characters, inside one SD-log record):

```
A3E_PACE pace=[tft yield  loop idle-wait] win=60.1s | frame n=... avg=... max=... | tft=avg/max in=n:avg/max | full=n:avg/max vp=n:avg/max | yld n=... /vp=9.0 vpms=... tot=... max=... late=... | loop n=... /s=... max=... wait=n:avg/max in=... asleep=... | und=0 hw=0 miss=0 | cpu0=.. cpu1=.. main=.. aud=..
```

### 22.8 Tests, RED / GREEN and mutations

- **New targets.** `a3_04e_pacing` (37 checks): P the policy, C the counters, F the report / line / fit, U the rows, S the device wiring and the preservation guards. `a3_04e_pacing_runtime` (26 checks): the **real `AlphaRuntime` driving the real `tdeck_board.cpp`** over the fake panel. It covers Y the pauses (37 / 9, the same points and transactions in both modes), M the modelled procedure (legacy ≥ 360 ms repaint, A3-04E < half), V the panel (byte-identical transaction streams under both pacings; after every one of 1,099 gameplay renders the panel's viewport **is** the composed viewport, row for row: no incomplete frame, no corrupted or missing row), L the loop gate (idle → 1 tick; a handled-but-undrawn input, the legacy spin, the benchmark, a paced Camp → none), P the probes and T the game untouched.
- **Host seams.** `host_tests/board_shims/` (fake `driver/*.h`, `esp_cpu.h`, `esp_vfs_fat.h`, `sdkconfig.h`, and `fake_tdeck_bus.{h,cpp}`: the ST7789 decode, the stream hash, the timing model). `esp_shims/freertos/task.h`: `vTaskDelay` / `taskYIELD` hooks, no-ops unless a test installs them. The fixture now copies the runes font (the real Board refuses a world frame without it; the capture stub never read it). There is a read-only `AlphaRuntime::composed_viewport()`.
- **Changed expectations (deliberate, not weakened).** `ui_debug_menu_test` U2: Diagnostics has 23 rows (was 21), both new rows are action rows, and the tone is still last. `a3_04c_contention` S2: the guard is unchanged in intent (every draw-loop pause is the timed `tft_yield`), but it no longer pins the literal `vTaskDelay(1)`, which the policy replaced. It now requires that `tft_yield` is the only `vTaskDelay`/`taskYIELD` of the draw code (the other two `vTaskDelay`s are the power-up and ST7789 init waits). A full run taken after the production change and before these two edits (`native/core/a3-04e-ctest-probe.log`) has exactly these two RED, 139/141 otherwise green.
- **RED-first.** The device scans run against the **unmodified A3-04D device sources** (`git archive HEAD` into scratch, the untracked `sdkconfig` copied; `a3-04e-red-device-scans-vs-a3-04d.log`): **S1–S4 and S6–S10 RED** (the tick sleep, the literal cadence, no repaint mark, the zero-tick loop delay, no input peek, no pacing hand-over, no gate, no probes, no `A3E_PACE`). S5 (no busy-wait), S11 (task placement and tick rate) and S12 (never saved) are GREEN on both trees, as preservation guards must be; mutants D8/D9/D10 prove they can fail. The core and runtime checks use the new API, so they are proven by mutation.
- **First-run REDs, all in the tests.** F5 left the scenario on the legacy policy after F2 (fixed in the test). F5b measured the true worst-case `A3E_PACE` line at 534 characters: the buffer went 400 → 600, still inside one 768-byte SD-log record with its prefix. V1 first compared counts that included the Developer frames drawn before the probe was switched, and then compared streams that legitimately differ by the probe row's own "ON"/"off" text; the comparison now starts after the probe is set. A raw-string escape was lost in one test edit (a literal CR in `'\r'`), which the compiler caught. No production change came from any of them.
- **Mutations:** `native/core/tools/a3_04e_mutation_check.py` → `native/core/a3-04e-mutation.log`, 36 mutants: the policy (K1–K7), the counters, report and line (C1–C4, F1–F4), the rows (U1–U3), the runtime (R1–R8) and the Board / device wiring (D1–D10). **36 mutants, 36 killed by a failing check, 0 survived.** The first pass killed 33. K5, K6 and C3 were INVALID: each left a parameter unused under `-Werror`. They were rewritten with a `(void)` and re-run (`a3-04e-mutation-rerun.log`): killed. The log says which check killed each one. For example, D1 (A3-04D's `tft_yield`, always `vTaskDelay(1)`) turns S1 and the runtime's Y3/M3/M4/V1 RED. D3 (a "faster" fill that skips half of every rectangle in the yield mode) and D4 (the yield mode abandons the viewport at its first pause) are killed only by the panel checks V1/V2/V3: that is the proof that a timing gain cannot hide an incomplete frame here. R2 (the gate forgets the enemy beat) is caught by the source scan S8 alone. D9/D10 prove the preservation guards S11/S12 can fail.
- **Full suite:** fresh build `native/core/build-a3-04e`, **serial ctest 143/143 passed in 136.25 s** (`native/core/a3-04e-ctest-pass1.log`): A3-04D's 141 plus the two new targets. The only warning is the known w64devkit one. Every A3-04A..D audio test passes unchanged (the synth, pump and task are untouched), and so do the timing proofs (Camp, Blackthorn, Refuge, quake, shrine, combat timelines; `batch51_*` pacing), persistence and gameplay parity.

### 22.9 Firmware

Pre-commit build `native/targets/tdeck/build-a3-04e` (`a3-04e-firmware-configure.log`, `a3-04e-firmware-build.log`): ESP-IDF 6.1, `idf.py --no-ccache reconfigure` then `ninja -j 4`, **first attempt clean, zero project warnings** under `-Werror`.

- **Size: `0xee5d0` = 976,336 B, +4,080 B** vs A3-04D's 972,256 B; **`0x11a30` = 72,240 B (7 %) free** in the 1 MiB app partition.
- Sections vs A3-04D (`esp_idf_size` on both images' `.map` files, `a3-04e-size-a3-04d-image.log` / `a3-04e-size-a3-04e-image.log`): **IRAM `.text` 60,647 + 15,356 = 76,003 B, unchanged**; `.data` unchanged; `.bss` +80 B (the runtime's and the Board's pacing, the new counters); flash `.text` +3,348 B; `.rodata` +736 B.
- **Image checks on the A3-04E ELF:** `a3_04b_iram_check.py` **GREEN** (`a3-04e-iram-check.log`) and `a3_04a_hotpath_check.py` **GREEN** (`a3-04e-hotpath-check.log`). The audio path is untouched.
- **The pause in the linked image** (`a3-04e-tft-yield-disasm.log`): `Board::tft_yield` reads the Board's pacing byte between its two unchanged `rsr.ccount` reads. `TickSleep` calls `vTaskDelay(1)`; `Yield` calls `vPortYield` (`taskYIELD`).
- **Stack (`-fstack-usage`):** `log_metrics` 848 B before and after (the `A3C_PERF` and `A3E_PACE` buffers share its frame); `contention_line` 576 → 640 B; `pacing_line` 624 B, a sibling of it. The main task has 24 KiB.
- A later `idf.py -B build-a3-04e size` rebuilt that directory (with ccache) before sizing it. The sizes above come from the `.map` files, and the checks ran on the first image. The tagged image is the post-commit one anyway.
- Version string `3.0.0-alpha3-dev-a3-04e-debug` (`CMakeLists.txt` `PROJECT_VER`).
- Not flashed. The post-commit image (a fresh directory, `--no-ccache`, which embeds the commit) is the one the annotated tag `alpha3-a3-04e-render-pacing` names, with its path, size, SHA-256 and `Git`.

### 22.10 Hardware validation checklist (the user's; not done here)

**0. Identity first.** Flash the A3-04E Launcher image `native/targets/tdeck/build-a3-04e-post/launcher/OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04e-Debug-Launcher.bin` through Launcher (its size, SHA-256 and `Git` are in tag `alpha3-a3-04e-render-pacing`). Same game pack, same `openu5-audio.bin`: nothing is regenerated. The boot identity screen must read **`FW 3.0.0-alpha3-dev-a3-04e-debug`** and the tag's `Git` hash. If not, stop.

**Serial (strongly preferred for this batch: it is the watchdog check).** From an ESP-IDF PowerShell (`. C:\esp\v6.1\esp-idf\export.ps1`): `python -m esp_idf_monitor -p COMx -b 115200 --no-reset` (COMx = the T-Deck's USB-Serial/JTAG port). Save, for every run, the last **`A3C_PERF`** and **`A3E_PACE`** lines before the read, the **`PERF_REPORT`** lines the read prints, and every **`A3E_PROBE`** line. Afterwards search the whole capture for **`task_wdt`**: it must not appear. Without a PC, photograph every report page; the *Pacing (A3-04E)* section is the last before the `OPL2` line.

**Fixed settings for every run:** *Probe: SD diag logging* **off** (the boot default; never switch it on in this batch); *Probe: synth bypass* **off**; SFX 80 %.

**Diagnostics rows, counted Up from the top row:** 7 *Probe: legacy TFT pacing*, 6 *Probe: legacy loop spin*, 5 *Probe: SD diag logging*, 4 *Probe: synth bypass*, 3 *Audio/render performance*, 2 *Audio/render stats (live)*, 1 *Audio test tone*.

**Common procedure (each run):**
1. Set the run's probes (Alt+D › Diagnostics, Up N times, Enter until the row reads the wanted state) and the music volume (System Menu › Settings).
2. Start a clean window: Alt+D › Diagnostics › *Audio/render stats (live)* (two up) → Enter → **Enter** (dismiss) → Back → Back. Leaving the menu draws the full-screen repaint, which is **in** the window: it is the report's `full-screen` line.
3. Do the run's activity, opening no menu.
4. Read the window: Alt+D › Diagnostics › *Audio/render stats (live)* → Enter. Photograph every page or save the lines.

A run is **invalid** (repeat it) if the report's pacing line or the scenario (`… sdlog OFF`) does not match the table, if `frame n` < 150 (R4: < 100), or if it included any other menu visit.

| Run | Music | *legacy TFT pacing* | *legacy loop spin* | Report pacing line | Activity | Time |
|---|---|---|---|---|---|---|
| **A0** baseline | 0 % | **ON** | **ON** | `tft TICK  loop SPIN` | open-overworld grassland/forest, the A3-04D spot and direction; hold the trackball, turn at obstacles | 60 s |
| **A1** renderer alone | 0 % | off | **ON** | `tft yield  loop SPIN` | same | 60 s |
| **A2** main loop alone | 0 % | **ON** | off | `tft TICK  loop idle-wait` | same | 60 s |
| **R1** = A3 combined | 0 % | off | off | `tft yield  loop idle-wait` | same | 60 s |
| **R2** | **80 %** | off | off | `tft yield  loop idle-wait` | same | 60 s |
| **R3** heavy redraw | 80 % | off | off | same | walk along a coastline with animated water in view for 60 s, then enter and leave a town **three times** | ~2 min |
| **R4** responsiveness | 80 % | off | off | same | 30 s of rapid direction changes (flick the trackball N/E/S/W as fast as possible, count the flicks roughly), then open and close the System Menu 3× and the Developer menu 3× (read this window **last**, via the Developer menu) | ~60 s |

Run them in that order: A0 → A1 → A2 → R1 (each candidate alone before the combination), then R2–R4. Optional, for the eye: repeat R3 with both probes ON and compare the town entry/exit and the coastline.

**While running, note:** banding/tearing of the viewport, step-to-screen lag, uneven cadence, the town entry/exit and menu-dismissal repaints, animated water/coast, any incomplete or corrupted rows, music smoothness/tempo, SFX timing, static, input responsiveness.

**Fields to record (each run):** the pacing line; `frame avg/p95/max`; `tft avg/max`; `input shown avg/max`; the Pacing section (`full-screen … max`, `viewport w/o full … avg/max`, `pauses/vp frm … = … ms`, `loop …/s  waits … avg … max`, `loop asleep … of … s`, `input wakes`); `yield … late`; `xfer max … slow`; `audio … und/hw/miss`, `audio task/blk`; CPU0 / CPU1 / main / aud. `A3E_PACE` and `A3C_PERF` hold all of them.

**PASS / FAIL:**
- **A0 reproduces A3-04D** (otherwise the comparison is void; report and stop): `full-screen` max ~390–420 ms, `viewport w/o full` avg ~95–125 ms, `pauses/vp frm ~9.0 = ~85–90 ms`, CPU0/main ~70–75 %.
- **A1 (renderer):** `pauses/vp frm ~9.0 = < 1 ms`; `full-screen` max **≤ 200 ms** (model 114 ms plus row building); `viewport w/o full` avg **≤ 60 ms** (model 28 ms plus row building); no bands; no incomplete/corrupted rows.
- **A2 (main loop):** `loop asleep` more than half of the window; `input wakes` > 0; **main % at least 10 points below A0's**, CPU0 likewise; `input shown` avg no more than ~10 ms above A0's; the TFT figures as in A0.
- **R1 (combined):** A1's and A2's criteria together; `frame max` **≤ 250 ms** (A3-04D: ~443); `input shown` max ≤ 250 ms.
- **R2:** as R1; `und=0 hw=0 miss=0` (0–1 at a song switch); no static; the song's tempo unchanged; CPU1 ~ A3-04D Test 3's (~43 %: the synth is unchanged).
- **R3:** no incomplete or corrupted rows; town entry/exit visibly quicker; `full-screen` max ≤ 250 ms; audio as R2.
- **R4:** every flick moves (no lost or doubled step), no stuck input, `input shown` max ≤ 250 ms, each menu dismissal repaint ≤ 250 ms.
- **FAIL — report, do not tag:** any `task_wdt` line on serial; a crash or reboot; underruns or missed deadlines in steady play; static; a song-speed change; an SFX timing change; an incomplete/corrupted frame; lost/stuck input; or A1/R1 `full-screen` max not at least 30 % below A0's.

Switch both probes off at the end (a reboot also does).

### 22.11 Outcome, remaining performance work

**Outcome A (host):** the source of the ~400 ms worst case is exact. It is the full-screen repaint on leaving the Developer screen, which crosses 37 forced sleeps until the next 10 ms tick (§22.2), and the ~122 ms viewport average is the same mechanism (9 sleeps per step). No requirement for the sleeps exists (§22.3). Production now yields at the same points, and an idle loop pass waits for input for one tick. **Pending:** the device runs of §22.10. **Hardware validation is not claimed.**

*A3-04E.1 closeout (2026-09-27): the pacing is hardware-validated on the A3-04E.1 image (§23.10). The numbers are in §22's status note.*

Remaining, in the order the numbers suggest (each its own batch, measured with this report):
1. **Composition time.** A3-04D's frame avg (79 ms) minus TFT avg (41 ms) leaves ~38 ms of composition and logic per frame, which becomes the largest term once the tick sleeps are gone. The report's `compose avg/max` and `tiles max` locate it.
2. **`fill_rect` chunking by width.** A 1–2 px vertical line is one 4-byte transaction per row (~27 µs each: 180 of them per viewport-frame side). Chunking the pixel stream by up to 320 pixels regardless of width (the window auto-wraps) would take the repaint's ~530 small transactions to a handful. This is a renderer change with its own pixel-identity proof, and V1/V2 are the harness.
3. **Transcript redraw per step.** Every step rewrites the whole scrolled transcript (up to 19 text rows). A scroll-aware redraw is a renderer change.
4. **The synth's steady cost** (~3.3 ms per 8 ms block, ~43 % of core 1). It is unchanged here, and it is the next audio batch as planned.
5. The fountain ambient-SFX parity defect (separate, audio).

### 22.12 Files

- Core: `include/openu5/render_pacing.h`, `src/render_pacing.cpp` (new: the policy), `sources.cmake`; `include/openu5/perf_report.h`, `src/perf_report.cpp` (`TftTiming::full_screen`, the pacing counters, the report section, `format_pacing_line`); `include/openu5/ui_debug_menu.h`, `src/ui_debug_menu.cpp` (the two rows).
- Device: `tdeck_board.{h,cpp}` (the pause through the policy, `set_tft_pacing`, the repaint mark), `main.cpp` (the idle wait), `tdeck_input.{h,cpp}` (`wait_for_event`), `alpha_runtime.{h,cpp}` (pacing state, `loop_may_sleep`, the probes, the scenario, `A3E_PACE`, `composed_viewport`), `CMakeLists.txt` (`PROJECT_VER`).
- Tests / tools: `tests/a3_04e_pacing_test.cpp`, `host_tests/a3_04e_pacing_runtime_test.cpp`, `host_tests/board_shims/*`, `host_tests/esp_shims/freertos/task.h`, `host_tests/alpha_runtime_host_fixture.cpp` (runes font), `tests/ui_debug_menu_test.cpp` (23 rows), `tests/a3_04c_contention_test.cpp` (S2), `core/CMakeLists.txt`, `tools/a3_04e_mutation_check.py`.

## 23. A3-04E.1 — the task-watchdog regression: an idle-service guarantee

**The hardware report that opened this batch** (the user, a casual pre-test run of the A3-04E image before the §22.10 matrix, default pacing, SD logging off, normal gameplay): after ~71 s the task watchdog fired on IDLE0 with `main` running on CPU 0. Serial showed `task_wdt: Task watchdog got triggered`, `- IDLE0 (CPU 0)`, `CPU 0: main`, `CPU 1: IDLE1`. Audio stayed healthy (`und=0 hw=0 miss=0`). `A3E_PACE` did not appear in the capture, although the filter included it. **A3-04E is not hardware-valid**, and the §22.10 matrix is withdrawn until the watchdog soak of §23.8 passes.

**Status: FIXED ON THE HOST, REPRODUCED IN THE MODEL; HARDWARE VALIDATION PENDING.** A3-04E's pacing is kept. The game thread now *guarantees* core 0's idle task a pass of its loop at least every 200 ms, and it does so by observing the pass rather than assuming one. The watchdog is neither disabled nor extended.

*Hardware closeout (2026-09-27, §23.10): **FIXED ON HARDWARE.** No `task_wdt` in either capture. Default pacing: idle gap max 102.9 ms and `forced=0` over 11 minutes. The legacy loop-spin phases: 262 guard sleeps, idle gap max 223.9 ms.*

### 23.1 The scheduler, from the source (Phase: reproduce / analyse)

- The watchdog (`CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`, `CHECK_IDLE_TASK_CPU0=y`, warning only) is fed on core 0 by `esp_vApplicationIdleHook()` (`esp_system/freertos_hooks.c:41`). IDLE0 calls it **once per pass of its loop** (`FreeRTOS-Kernel/tasks.c` 4297–4350). Each pass first runs `prvCheckTasksWaitingTermination()`. It then runs a `taskYIELD()`, because the idle-priority ready list always holds IDLE0 and IDLE1 (`configIDLE_SHOULD_YIELD`). Only after that come the hooks. The kernel runs from flash (`CONFIG_FREERTOS_IN_IRAM` off).
- `main` has priority 1 and IDLE0 priority 0, on the same core. **IDLE0 runs only while `main` is blocked.** A yield — `taskYIELD()`, or `vTaskDelay(0)`, which is the same reschedule — returns straight to `main`. The user's point is right: the A3-04E draw loops' `taskYIELD()` can never feed the watchdog.
- A3-04E's §22.3 argued that the draws did not need to: every row blocks in `spi_device_transmit`'s result-queue wait, and "IDLE0 runs there". That was an argument from the source, never measured. **The hardware disproves it as a guarantee.** Many transactions are only a few microseconds long: 1–4-byte commands sent by the CPU, the 4-byte fill chunks of the thin frame lines, 32-byte animated-cell rows. Each is shorter than a switch into IDLE0 plus the flash-resident path to its hook. A row's wait lets IDLE0 in, but nothing makes it reach the hook, and IDLE0 is preempted at the end of every row. A3-04D's own numbers point the same way: its ~26 % idle on CPU 0 is about what the tick sleeps alone account for.
- **Where the A3-04E game thread did not block at all:**
  - (a) Draws: only yields, plus the row waits above.
  - (b) The end of a pass while the trackball keeps an event queued: `wait_for_event` peeks and returns at once.
  - (c) The spin branch (`vTaskDelay(0)`) while the gate says no.
  - (d) **Console output.** Every log byte busy-waits on the calling task for USB-Serial/JTAG FIFO room (`esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c` 148–170). With a monitor attached, the per-frame log lines and the ~4 KB heartbeat burst are CPU time with no block in it.

  Continuous walking with serial attached is (a) + (b) + (d): nothing guarantees IDLE0 a pass. Up to A3-04D the 16-row `vTaskDelay(1)` did, nine times per step.
- **The model reproduces it** (`a3_04e_pacing_runtime` W1). The real `AlphaRuntime` drives the real Board, with only tick-long blocks letting the idle loop pass (the hardware-observed case) and the trackball's events always queued. **A3-04E as shipped leaves core 0's idle loop unrun for 6,144 ms**, past the watchdog's 5 s.
- **The main-loop change is not the cause, and it does not help enough.** The idle wait only ever *adds* blocks compared with A3-04D. It cannot guarantee one: an input event ends it at once, and the gate skips it during paced scenes. With the legacy loop spin probe on, the A3-04E draws would starve IDLE0 the same way. *Latent in A3-04D too:* standing still with no animated cell in view, A3-04D drew nothing and spun, so IDLE0 could starve there as well. The A3-04E idle wait and now the guard cover that case.

### 23.2 Candidates

| Candidate | Verdict |
|---|---|
| Disable or extend the watchdog | refused (the requirement; S17 pins the configuration) |
| Restore `vTaskDelay(1)` every 16 rows | restores the ~370 ms repaint: rejected |
| A bounded row budget (sleep every N rows) | still a blind sleep: ~2 per viewport; does not cover (b)–(d), which draw nothing |
| `vTaskDelayUntil` (a fixed-rate loop) | paces an event-driven loop to a period; adds input latency; does not cover the draw loops |
| **An idle-service guard that watches the idle loop and blocks only when it is overdue** | **selected**: zero cost where the idle loop already runs; bounded where it does not; covers draws, spins and console time alike |

### 23.3 The mechanism

- `tdeck::IdleService` (`main/idle_service.{h,cpp}`, host-compilable) and the policy `openu5::IdleServiceGuard` (`render_pacing.{h,cpp}`).
- **The observation.** `main.cpp` registers `IdleService::core0_hook` with `esp_register_freertos_idle_hook_for_cpu(.., 0)`. ESP-IDF calls **every** registered hook of the core in the same `esp_vApplicationIdleHook()` pass, the watchdog's included (the loop has no short-circuit). So the hook's counter moving means the watchdog was fed. The hook only counts, and returns `true` so the core still waits for an interrupt.
- **The guarantee.** `enforce()` runs at the game thread's cooperative points: the draw loops' pause (A3-04E's yield now asks it first) and **the end of every loop pass**, after the idle wait or the reschedule. While the counter moves, it returns at once. When it has not moved for **`kIdleServiceBudgetUs` = 200 ms** (25× under the 5 s watchdog), it sleeps `vTaskDelay(1)` until the counter moves, at most `kIdleServiceMaxSleeps` = 3 ticks, because the first sleep can end a microsecond later at the next tick.
- Everything else of A3-04E stays: the yields, the idle wait, the gate, both legacy probes. The guard is on in every probe state; it is a safety floor, not a pacing variant. Under the legacy tick-sleep pacing it never fires (W5), so A0 still reproduces A3-04D.

  *Hardware refinement (2026-09-27, §23.10.2): W5 is about the draws, and the device agrees: the tick sleeps feed IDLE0. With **legacy loop spin** ON, though, a pass that draws nothing never blocks. This is the latent A3-04D case of §23.1. There the guard does fire, 169 times in the first 55 s of the full-legacy phase. A0 on this image is therefore A3-04D plus a one-tick sleep roughly every 200 ms while nothing is drawn. Its render figures still reproduce A3-04D (TFT max 408.7 ms against 404–408 ms). That phase is a deliberate starvation test, not production behaviour.*

### 23.4 Expected added latency (model)

| Situation | Guard cost |
|---|---|
| The idle loop already runs: standing still (the idle wait), the legacy tick sleeps, or rows that do let it pass | **0** (W5: 0 enforcements in all three) |
| Worst case: continuous walking, the trackball's events always queued, rows never letting the idle loop pass | one tick sleep per 200 ms: **30 sleeps = 274 ms in a 6.4 s, 60-step walk (4.3 %)**; the longest idle gap **200.9 ms**; no enforcement gave up (W2) |
| The menu-exit repaint (113.8 ms modelled) | **0–1 tick** (< the budget); model: 0 (W4). Legacy: 37 |
| Any single frame | at most one tick (≤ 10 ms; ≤ 30 ms in the three-sleep limit, never reached in the model) |

The walk ends in the same game state with and without the guard's sleeps (W3).

### 23.5 Why `A3E_PACE` did not appear

- **The code emits it.** Every heartbeat logs it directly after `A3C_PERF` as the burst's last perf line (source; now also H1, which captures two real heartbeats on the host: `A3E_PACE hb=1` / `hb=2`, each right after its `A3C_PERF`). Logging is v1 with no length cap and no tag-level override. The SD-log hook forwards every call to serial whatever its state (A3-04D C1).
- **The console drops the tail of a burst.** The USB-Serial/JTAG path (no driver installed) busy-waits per byte for FIFO room only while less than 50 ms has passed since the last byte that went out. After that it **drops every byte** until the FIFO takes one again (`usb_serial_jtag_vfs.c` 148–170). The heartbeat is one ~4 KB burst (METRICS, TRACKBALL_*, KEYBOARD_METRICS, AUDIO_PERF, RENDER_PERF, SYS_PERF, A3C_PERF ~700 B, then A3E_PACE ~450 B). A 50 ms stall of the host-side reader mid-burst loses exactly its tail, which is `A3E_PACE`.
- **Or the filter:** `esp_idf_monitor --print_filter` matches tags, not message text. Both lines share one tag, though, so a tag filter would pass or drop both alike.
- **To tell them apart:** `A3E_PACE` now starts with **`hb=N`**, the heartbeat number, so a dropped line shows as a gap in N. If `A3C_PERF` lines are present and `hb=` numbers are missing, the console dropped them. The Developer report (on screen) never depends on serial.

### 23.6 Tests, RED / GREEN and mutations

- `a3_04e_pacing` **50 checks** (was 37): G1–G6 the guard's policy (the budget against the watchdog; never due while the idle loop runs; due at exactly 200 ms; one pass ends it; the counters; a new window keeps the service state). F1b/F1c the report line. F5b the worst-case line with `hb=` and the idle fields (625 characters; buffer 680; inside one 768-byte SD-log record). S13–S17 the wiring: `enforce()` blocks (never yields), bounded; the hook is registered on core 0 and handed to the Board and the report; every pass ends with the guard; the draw loops' yield asks it first; **the watchdog configuration is unchanged**.
- `a3_04e_pacing_runtime` **34 checks** (was 26): W1 the regression reproduced (A3-04E without the guard: 6,144 ms idle gap); W2 bounded with the guard (200.9 ms, 4.3 %, none in vain); W3 the game untouched; W4 the repaint's cost; W5 zero cost where the idle loop runs; W6 `A3E_PACE` carries `hb=` and the idle window; W7 a live read shows the gap and starts a new window; H1 the heartbeat emits `A3E_PACE` after `A3C_PERF`. The fake bus now models core 0's idle counter (`rows_feed_idle`, `idle_pass_us`, `idle_wait_one_tick`), and the test emulates `main.cpp`'s end of a pass (S15 pins the device's).
- **RED-first:** S13–S16 are RED against the **A3-04E device sources as tagged** (`git archive HEAD`; `native/core/a3-04e1-red-device-scans-vs-a3-04e.log`); S17 is GREEN on both. W1 is the regression itself, reproduced on the same harness that shows the fix.
- **Mutations:** `tools/a3_04e_mutation_check.py` now holds 50 mutants: A3-04E's 36 (D1 and R8 re-anchored) plus I1–I14 for the guard: a budget beyond the watchdog, a guard that never asks, an idle pass not recognised, an enforcement that yields instead of sleeping, no sleeps allowed, a hook that keeps the core from idling, the draw-loop yield or the loop pass skipping the guard, the hook on core 1, the report / `A3E_PACE` / runtime losing the idle window or the heartbeat number, a live read not restarting the window. `native/core/a3-04e1-mutation.log`: **49 killed, 1 survived**. I13 (every heartbeat the same number) survived because H1 compared string positions with `>` and a missing `hb=2` is `npos`, larger than anything. H1 now requires every line explicitly, and I13 is killed on the re-run (`a3-04e1-mutation-rerun.log`, with I11/R8). **50 of 50 killed.** I4 (an enforcement that yields) is the regression's own mechanism: it turns S13 and the runtime's W2/W7 RED.
- **Full suite:** `build-a3-04e`, **serial ctest 143/143 passed** in 129.49 s (`native/core/a3-04e1-ctest-pass1.log`) and again, after the H1 fix, in 127.41 s (`a3-04e1-ctest-pass2.log`). The two A3-04E targets grew in place; no new target. The only warning is the known w64devkit one.

### 23.7 Firmware

Pre-commit build `native/targets/tdeck/build-a3-04e1` (`a3-04e1-firmware-configure.log`, `a3-04e1-firmware-build.log`): ESP-IDF 6.1, `idf.py --no-ccache reconfigure` then `ninja -j 4`, first attempt clean, zero project warnings under `-Werror`.

- **Size: `0xee9f0` = 977,392 B, +1,056 B** vs A3-04E; **`0x11610` = 71,184 B (7 %) free**. Sections vs A3-04E (`esp_idf_size` on the `.map`, `a3-04e1-size-image.log`): **IRAM `.text` unchanged** (60,647 + 15,356); `.data` unchanged; `.bss` +80 B (the guard's state, the idle counter in internal DRAM); flash `.text` +876 B; `.rodata` +176 B.
- `a3_04b_iram_check.py` **GREEN**, `a3_04a_hotpath_check.py` **GREEN** (the audio path is untouched).
- The linked `Board::tft_yield` (`a3-04e1-tft-yield-disasm.log`) calls `vTaskDelay` (legacy), `tdeck::IdleService::enforce()` and `vPortYield` (`taskYIELD`). `IdleService::core0_hook/core0_count/attach/enforce` and the counter are in the image.
- Version `3.0.0-alpha3-dev-a3-04e1-debug`. Not flashed. The post-commit image is the one tag `alpha3-a3-04e1-idle-service` names.

### 23.8 Hardware validation (the user's; not done here)

**0. Identity.** Flash the A3-04E.1 Launcher image `native/targets/tdeck/build-a3-04e1-post/launcher/OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04e1-Debug-Launcher.bin` through Launcher. The identity screen must read **`FW 3.0.0-alpha3-dev-a3-04e1-debug`** and the tag's `Git`. Same packs, same `openu5-audio.bin`.

**Serial, unfiltered, to a file.** From an ESP-IDF PowerShell: `python -m esp_idf_monitor -p COMx -b 115200 --no-reset build-a3-04e1-post\openu5_tdeck.elf` (run from `native\targets\tdeck`; the ELF also decodes any watchdog backtrace), then **Ctrl+T, Ctrl+L** to start saving everything the monitor receives to a file. Do not filter while capturing; search the file afterwards. Keep the monitor window in the foreground and do not scroll or select text in it while testing, because a paused reader is exactly what makes the console drop lines (§23.5).

**Stage 1 — the watchdog soak (first; nothing else until it passes).** Default pacing (both legacy probes off), SD diag logging off, synth bypass off.

| Soak | Music | Activity | Time |
|---|---|---|---|
| **S1** | 0 % | continuous trackball walking on the overworld (hold it, turn at obstacles), including along a coastline | 3 min |
| **S2** | 0 % | stand still in open grassland/forest with no water in view | 1 min |
| **S3** | 0 % | open and close the Developer menu (Alt+D, Back) 5×, the System Menu 5× | ~1 min |
| **S4** | **80 %** | normal play: walk, enter and leave a town twice, fight if a fight comes | 5 min |

Before S1 start a window (Alt+D › Diagnostics › *Audio/render stats (live)* → Enter → Enter → Back → Back). After each soak read it (same row) and photograph the *Pacing (A3-04E)* section. Its last line is **`idle0 gap max … ms forced n/… ms miss …`**.

**PASS (all four):**
- the saved serial file contains **no `task_wdt`**;
- `idle0 gap max` **< 250 ms**, and `miss 0`;
- in S4, `und=0 hw=0 miss=0`;
- no crash or reboot;
- `A3E_PACE hb=` numbers **consecutive** in the file. If `A3C_PERF` lines are there but some `hb=` numbers are missing, send the file: that is the console dropping lines, not the firmware omitting them.

**Record for S1** `forced n/… ms`. It answers the open hardware question directly. Close to 0 means the rows' own waits do feed IDLE0 on this board, and the 71 s trip came from the other unblocked paths (the queued-input peek, the console busy-wait). About 5 per second means they do not, as the model assumed.

**FAIL:** any `task_wdt`; `idle0 gap max` ≥ 250 ms or `miss` > 0; audio underruns; a crash. Report and stop.

**Stage 2 — only after Stage 1 passes:** the §22.10 matrix (A0, A1, A2, R1–R4), unchanged, on this image. Add the `idle0 gap max`, `forced` and `miss` fields to every run's record, and treat any `task_wdt` as a FAIL.

*As run (2026-09-27): Stage 1 was one continuous default-pacing soak. Stage 2 was abbreviated to one probe sequence. Both results and the reasoning are in §23.10.*

### 23.9 Files

- Core: `include/openu5/render_pacing.h`, `src/render_pacing.cpp` (`IdleServiceGuard`, the budget); `include/openu5/perf_report.h`, `src/perf_report.cpp` (the `idle0` line, `A3E_PACE`'s `hb=` and `idle0` fields, buffer 680).
- Device: `idle_service.{h,cpp}` (new), `CMakeLists.txt` (source list), `tdeck_board.{h,cpp}` (the yield asks the guard; `set_idle_service`), `main.cpp` (the hook, the guard at the end of every pass), `alpha_runtime.{h,cpp}` (`attach_idle_service`, the report, the heartbeat number, the window reset); `CMakeLists.txt` (`PROJECT_VER` `3.0.0-alpha3-dev-a3-04e1-debug`).
- Tests / tools: `tests/a3_04e_pacing_test.cpp`, `host_tests/a3_04e_pacing_runtime_test.cpp`, `host_tests/board_shims/fake_tdeck_bus.{h,cpp}` (the idle model), `core/CMakeLists.txt`, `tools/a3_04e_mutation_check.py` (D1/R8 anchors, I1–I14).

### 23.10 Hardware result (closeout, 2026-09-27)

**Verdict: PASS. The watchdog regression is fixed on hardware, and A3-04E's renderer pacing is hardware-validated. A3-04E and A3-04E.1 are closed.**

**Evidence.** The user flashed the A3-04E.1 image and captured serial with PlatformIO's device monitor (`COM11`, 115200). The monitor saved two UTF-16 files. They are committed here as UTF-8 with CRs stripped and nothing else changed:

| File (committed) | Original (the user's) | SHA-256 of the original | Uptime covered |
|---|---|---|---|
| `a3-04e1-hw-soak.log` (10,487 lines) | `a3-04e1-soak.txt` | `6c3a417f…a535ef` | 109.4 → 660.0 s (9 min 11 s captured; one boot, attached mid-run) |
| `a3-04e1-hw-probes.log` (3,872 lines) | `a3-04e1-soak2.txt` | `ad29e31c…093c40` | 51.3 → 260.0 s (a second boot: title, Continue, the probe sequence) |

`native/core/tools/a3_04e1_hw_summary.py` reads a capture and prints four things. First, the watchdog, crash and reboot search. Second, every `A3E_PROBE` line. Third, the heartbeat sequence and one row per `A3E_PACE`. Fourth, the gameplay per-frame `render … us` lines, grouped by the probe state they were drawn in. The Developer screen draws with `crc=00000000` and is excluded. Its output for both files is `a3-04e1-hw-summary.log`.

**Identity.** Neither capture includes the boot banner, so the `Git` hash was not captured on serial. The line formats identify the image: `A3E_PACE hb=` and the `idle0 gap=… forced=… miss=…` field exist only in A3-04E.1 (§23.5, §23.6).

#### 23.10.1 Stage 1: the watchdog soak (`a3-04e1-hw-soak.log`, default pacing)

The pacing line reads `tft yield  loop idle-wait` throughout, and no probe was switched. SD logging was OFF and SFX was at 80 %. The measurement window was never restarted: `win` grows from 112.0 to 659.2 s. Every maximum below is therefore over the **whole run since boot**, which is a stricter reading than the per-soak windows §23.8 asked for.

| Segment (uptime) | Music | What happened |
|---|---|---|
| ≤ 319 s | **80 %**: *Britannic Land*, *Greyson's Tale*, *Villager Taran* (the song follows the location) | walking, location changes, Developer menu visits, six System Menu opens (~300–319 s, ending in Settings) |
| 319–475 s | **0 %** (silent) | walking |
| 475–660 s | 0 % | standing still: no gameplay frame drawn at all (the count stays at 3,195), which is the idle-wait case |

| Criterion (§23.8, §22.10) | Result |
|---|---|
| no `task_wdt`, crash or reboot | **none**: no watchdog, Guru, abort, backtrace, `rst:` or E-level line |
| `idle0 gap` max < 250 ms, `miss 0` | **102.9 ms** over the whole run (11.0 ms at `hb=13`, 54.7 ms by `hb=14`, 84.4 ms by `hb=29`, 102.9 ms by `hb=53`); `miss=0` |
| `forced` (the S1 question) | **`0:0/0.0` on every heartbeat.** In default pacing on this board, the row waits, the SD waits and the idle wait let core 0's idle loop run often enough, and the guard never had to act |
| audio | `und=0 hw=0 miss=0` on every heartbeat; every `AUDIO_PERF` line `missed=0 underruns=0 hw_underruns=0` |
| heartbeats | `hb=13`–`122`: **110 consecutive**, none dropped, and 110 `A3C_PERF` lines |
| frame max ≤ 250 ms (R1) | **186.1 ms** (avg 61.9 ms, p95 94.0 ms, 3,195 frames) |
| full-screen repaint ≤ 250 ms (R3) | 17 repaints, TFT **avg 147.0, max 148.8 ms**; whole frame 180.9–183.6 ms for the six System Menu dismissals |
| viewport without full-screen ≤ 60 ms avg (A1) | **51.9 ms avg**, 71.2 ms max, over 716 frames |
| pauses | 9.7 per viewport frame, **0.0 ms**; 50.3 ms in total over the run; the longest single yield 0.21 ms |
| main loop | asleep **385.1 of 659.2 s (58 %)**, 977 input wakes |
| CPU1 with music (R2) | 41 % (the synth; A3-04D Test 3: ~43 %) |
| per-frame gameplay renders | 753 walking/redraw frames: **median 87.4 ms**, p90 89.2 ms, max 183.7 ms |

**What this settles and what it does not.** `forced=0` answers §23.8's question: on this board the draws and the idle wait feed IDLE0, and the guard was not needed in default pacing. So this soak did **not** reproduce A3-04E's 71 s trip, and that trip's exact trigger on the device remains unobserved. The guard was proven by Stage 2 instead: in the loop-spin phases the game thread never blocks between frames, which is the same starvation class, and the guard held the gap bounded (§23.10.2). Hardware-fixed therefore means two things: the watchdog never fired under default pacing, and when starvation is forced on purpose the guard stops it.

#### 23.10.2 Stage 2, abbreviated: the probe sequence (`a3-04e1-hw-probes.log`)

The captured boot loaded the save in 1,057 ms. The user then walked a few steps and switched the probes in the Developer menu. `A3E_PROBE` lines:

| Uptime | Probes → pacing | Phase |
|---|---|---|
| boot | `tft=yield loop=idle-wait` | production: 5 steps, 69–87 ms |
| 77.848 s | `tft=TICK loop=idle-wait` | 3.9 s inside the Developer menu (no gameplay frame) |
| 81.698 s | `tft=TICK loop=SPIN` | **full legacy = A3-04D pacing (A0)** |
| 136.688 s | `tft=yield loop=SPIN` | **renderer alone (A1)** |
| 165.348 s | `tft=yield loop=idle-wait` | **production restored (R1; Music 80 % from 174 s)** |

**The counters are cumulative across probe changes.** After the legacy phase, every `max` in `A3E_PACE`/`A3C_PERF` (frame 444.8, TFT 408.7, viewport 125.8) is still the legacy value, so those maxima say nothing about the new pacing. The per-phase evidence is the per-frame `render` lines and each counter's change across a phase:

| Phase | Gameplay steps (render lines) | Full-screen / menu-exit repaint (whole frame) | Tick-sleep time | Guard |
|---|---|---|---|---|
| full legacy (`TICK`/`SPIN`) | 55 steps: **median 155.5 ms** (149.4–160.9) | **441.3, 441.6, 442.4 ms** (two Developer exits, one System Menu) | ~71 ms per viewport frame (`vpms` 70.9 at `hb=17`, cumulative); single sleep max **9.60 ms** (one 10 ms tick); 4.4 s in total | 169 sleeps by `hb=17` (55 s); gap max 200.1 → 219.2 ms |
| renderer alone (`yield`/`SPIN`) | 52 steps: **median 88.0 ms** (86.6–89.1) | 183.0 ms | +3.3 ms over the whole phase (4,398.1 → 4,401.4 ms) | still acting: 169 → 256 (the spin starves IDLE0 while nothing is drawn); gap max 223.9 ms |
| production (`yield`/`idle-wait`) | 65 steps: **median 87.9 ms** (48.8–89.8) | 184.3 ms (System Menu) | ~0 | **frozen at 262** from `hb=24` (169 s) to `hb=42` (260 s): no guard sleep in production, at Music 0 % and 80 % |

From the counters over the legacy phase: the full-screen TFT maximum was **408.7 ms** (A3-04D: 404.4–408.1). The viewport TFT average reached **113.8 ms** at `hb=15` (A3-04D: ~122; §22.10's A0 band is 95–125). The frame max was **444.8 ms** (A3-04D: 443–445). **A0 reproduces A3-04D**, so the comparison is valid. The renderer change alone takes a walking step from ~155 to ~88 ms (−43 %) and the menu-exit repaint from ~442 to ~184 ms (−58 %). Stage 1 shows the same figures over 11 minutes: step median 87.4 ms, repaint TFT max 148.8 ms, which is −64 % against 408.7 ms and clears §22.10's 30 % bar. The loop setting does not change the step time (88.0 against 87.9 ms), as §22.4 predicted.

Across all 262 guard sleeps (2,337.3 ms, 8.9 ms each, so one tick), the largest idle gap was 223.9 ms. That is 24 ms over the 200 ms budget: the guard only checks at the cooperative points, and composition (~38 ms per frame) has none. It stayed under §23.8's 250 ms line and far under the watchdog's 5 s. No `task_wdt`. `und=0 hw=0 miss=0` throughout.

**The legacy phases are not production.** With legacy loop spin ON, the game thread never blocks when nothing is drawn. Those phases drive the idle-service guard on purpose: the 262 `forced` sleeps and the 200–224 ms gaps are the starvation test working. They are not a production figure. Production's are Stage 1's `forced=0` and 102.9 ms.

#### 23.10.3 Visual and UI stress

The user reports no visual corruption across Developer menu entry and exit, System Menu rendering and navigation (the Settings change to Music 0 %/80 %), full-screen redraws, normal movement, animated-world rendering, and play after the probes were restored. The host already proves that the panel always equals the composed viewport under both pacings (§22.8 V1–V3). The device agrees by eye.

#### 23.10.4 Why the §22.10 matrix was abbreviated

The full A0/A1/A2/R1–R4 matrix was not run as separate windows. The two captures already cover what it was designed to decide:

| §22.10 run | Covered by |
|---|---|
| A0 (legacy baseline) | the `TICK`/`SPIN` phase: reproduces A3-04D (408.7 ms TFT, ~155 ms steps, ~442 ms repaint) |
| A1 (renderer alone) | the `yield`/`SPIN` phase: steps 88.0 ms, repaint 183.0 ms, no tick-sleep time |
| A2 (main loop alone) | not run as a window (the `TICK`/`idle-wait` phase was 3.9 s inside the Developer menu). The main-loop change is measured in Stage 1 instead: the loop asleep 58 % of 11 min, 977 input wakes, and step time unchanged by the loop setting (above) |
| R1 (combined, Music 0 %) | Stage 1, 319–660 s, and Stage 2's production phase |
| R2 (Music 80 %) | Stage 1 ≤ 319 s and Stage 2 from 174 s: `und=0 hw=0 miss=0`, CPU1 41 % |
| R3 (heavy redraw, towns) | Stage 1's location changes (the song follows them), 17 full-screen repaints ≤ 148.8 ms TFT, animated-world rendering |
| R4 (menus, responsiveness) | Stage 1's six System Menu opens and Developer visits, and Stage 2's menu navigation; the idle gap stayed ≤ 102.9 ms through every menu in default pacing |
| watchdog (§23.8) | Stage 1 end to end, and Stage 2's forced-starvation phases |

**Not assessed**, for the record:
- **The `input shown` maximum.** The window was never restarted, so it includes time before the Stage 1 capture began (2,059.7 ms; 3,067.0 ms right after Continue in the Stage 2 capture) and one System Menu visit (7,016.1 ms, recorded at ~320 s, while a keypress stayed pending until the next gameplay frame). None of these is a gameplay latency. No run measured R4's ≤ 250 ms criterion in a clean window.
- **R4's counted trackball flicks** (lost or doubled steps) were not recorded. The user reports no stuck or lost input.

#### 23.10.5 Other observations (not A3-04E.1 defects; recorded, not acted on)

- **The System Menu takes ~0.72–0.75 s to open.** Every open is preceded by an `AlphaSave: SD_HEAP … state=save-inspect` line. `TDeckInput: INPUT_SERVICE render_block_us=721502`–`747882` shows the input consumer waiting that long. This is SD save-slot inspection, not pacing. A Settings save costs ~270 ms the same way. The idle gap stayed ≤ 102.9 ms through all of them, because the SD waits block. A candidate for a storage batch.
- **The internal-heap low-water mark is 200–344 B** (`SYS_PERF heap_int_min`). Free internal heap is ~230 KB at the first heartbeats and ~73–77 KB from around the first System Menu open onward (the `SD_HEAP` lines show the same figure). The low-water fell to 200 B during the System Menu opens at ~304 s. No allocation failure and no E-level line appear in either capture. The mark was never recorded before, so it is not known whether it is new. It deserves its own look before Alpha 3 ships.
- **`INPUT_SERVICE` warnings fire on ordinary steps.** The threshold (75 ms, `tdeck_input.cpp`) is below a normal ~88 ms step, so most of the 296 such warnings across the two captures are routine.
- **`PRESENTATION_DISPATCH` floods serial.** It is logged on every gameplay render (`alpha_runtime.cpp`): 2,532 of 10,487 lines (24 %) and 1,188 of 3,872 (31 %). It makes device captures harder to read and adds console busy-wait time on core 0 (§23.1 (d)). The fix is to log it on state change only, as a future cleanup.
- 15 slow SPI transactions out of 822,703 rows (`xfer max=1.19 ms`, `insd=0`) over the 11 minutes. Not attributed. Harmless at this size.

#### 23.10.6 Tag and files

- **No new tag, and the existing tag was not moved.** The project tags the implementation commit of each batch. Tag `alpha3-a3-04e1-idle-service` (`cb423a69`) names the post-commit image the user flashed, whose embedded `Git` is that commit. Moving the tag would break that identity check. Earlier hardware results (A3-01, A3-02, A3-04D's confirmation in §22) were recorded in this document without a tag. This closeout is a documentation commit after the tag, and the tag's image is the validated one.
- Files: `ALPHA3_AUDIO.md` (the top status, the notes in §22, §22.11, §23, §23.3 and §23.8, and this §23.10–§23.11); `GAMEPLAY_INTEGRATION_AUDIT.md` (the Alpha 3 note); `LAUNCHER.md` (the A3-04E.1 row); `a3-04e1-hw-soak.log`, `a3-04e1-hw-probes.log`, `a3-04e1-hw-summary.log` (new); `native/core/tools/a3_04e1_hw_summary.py` (new). No source, test or firmware change: the suite and the image are A3-04E.1's.

### 23.11 Next batch: ambient-SFX parity (separate from A3-04E.1)

Two device-observed ambient defects, for an audio-parity batch of their own:

1. **The fountain does not sustain its PC-speaker "burble".** On the device it is heard only briefly around movement, never as a continuous sound while standing next to it. §16.4 pins one NB(10, 30, 25000) burble (1.7 ms) per 55 ms tick while eligible. The original ticks 0x4102 on every redraw of its key wait (`getkey_with_redraw` 0x266c). The batch must derive that redraw cadence, and so the sound's real density, from the binary. It must also check on the device that `service_ambient` (`alpha_runtime.cpp`, called from the gameplay render path) keeps ticking while the player stands still, and how the SFX policy treats back-to-back 1.7 ms cues.
2. **Grandfather clocks play a wrong beep after movement before their tick-tock.** The runtime re-arms the clock's strike (`[0x5884]` ← the 12-hour clock) whenever the game time's **minute** changes (`service_ambient`: "the device sees that as the game clock moving"). A step advances the minute, so every step re-arms a strike. §16.4's C1–C2 pin exactly that ("after a step moves the clock, strikes the hour"). The batch must adjudicate `advance_clock` 0x5164–0x5183 against the binary, and find which advance re-arms `[0x5884]`, before touching the pinned row. A deliberate host row is not proof by itself: A3-HF1 found one that was invented.

Also queued: `PRESENTATION_DISPATCH` on state change only (§23.10.5), then §22.11's remaining performance list (composition time, `fill_rect` chunking, the transcript redraw, the synth's steady cost).
