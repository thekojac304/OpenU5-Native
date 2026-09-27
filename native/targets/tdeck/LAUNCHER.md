# Launcher packaging — Alpha 2 (RC1, released as final)

> **Batch 55: Alpha 2 final is the RC1 image below, byte for byte.** It passed the
> Phase 8 hardware smoke. No final-version image is built: a rebuild would be a new,
> untested image. The release tag is `alpha2-batch55-release`. The firmware's source
> commit, and its embedded `Git`, is `211c676a1dca` (`alpha2-batch54-rc1`).
> SHA-256 `ff3dfe193973547db2648c5f486aa381086505eb80b5f2dadec7432194fb5828`.

From an activated ESP-IDF v6.1 shell, on a clean, committed tree (the firmware
embeds `git rev-parse --short=12 HEAD` at CMake configure time):

```sh
idf.py --no-ccache -B build-batch54 build
python package_launcher.py --build-dir build-batch54
```

Validated output:

`build-batch54/launcher/OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin`

The packager checks the ESP32-S3 application header, the segment checksum and the
appended SHA-256, then makes a byte-identical copy. It does not merge a
bootloader or partition table, pad, flash, or otherwise modify the image. Since
Batch 54 the file name is derived from `PROJECT_VER` (`CMakeLists.txt`), so a
release candidate never shares a name with an ordinary batch image:

| `PROJECT_VER` | Launcher file |
|---|---|
| `2.0.0-alpha2-debug` (Batches up to 53B) | `OpenU5-TDeck-Alpha2.0.0-alpha2-Debug-Launcher.bin` |
| `2.0.0-alpha2-rc1-debug` (Batch 54, RC1) | `OpenU5-TDeck-Alpha2.0.0-alpha2-RC1-Debug-Launcher.bin` |
| `3.0.0-alpha3-dev-a3-01-debug` (Alpha 3 A3-01, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-01-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-01-audio-architecture`. The game packs are unchanged; `/ultima5/openu5-audio.bin` is optional (`ALPHA3_AUDIO.md`). |
| `3.0.0-alpha3-dev-a3-02-debug` (Alpha 3 A3-02, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-02-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-02-sfx-synth`. The game packs are unchanged; the sound effects need no audio pack (`ALPHA3_AUDIO.md` §15). |
| `3.0.0-alpha3-dev-a3-04-debug` (Alpha 3 A3-04, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04-music-playback`. The game packs are unchanged; music playback needs the existing patched `openu5-audio.bin` (`ALPHA3_AUDIO.md` §17), no regeneration. Hardware confirmation pending (A3-05). |
| `3.0.0-alpha3-dev-a3-04a-debug` (Alpha 3 A3-04A, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04a-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04a-audio-realtime`. The music-stutter fix (`ALPHA3_AUDIO.md` §18): same packs, same `openu5-audio.bin`, no regeneration. Adds Developer > Diagnostics > Audio performance / Audio stats (live). Smoothness retest pending (§18.18). |
| `3.0.0-alpha3-dev-a3-04b-debug` (Alpha 3 A3-04B, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04b-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04b-render-contention`. The overworld-lag-with-music fix and the visible perf report (`ALPHA3_AUDIO.md` §19): same packs, same `openu5-audio.bin`, no regeneration. Developer > Diagnostics > Audio/render performance / Audio/render stats (live) open a report on the Developer screen. Render-performance retest pending (§19.16). |
| `3.0.0-alpha3-dev-a3-04c-debug` (Alpha 3 A3-04C, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04c-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04c-contention-map`. The contention map (`ALPHA3_AUDIO.md` §20): same packs, same `openu5-audio.bin`, no regeneration. The live report gains a contention section, serial/SD gains one `A3C_PERF` line every 5 s, and Developer > Diagnostics gains *Probe: synth bypass*. Hardware runs A/B/B′/C/D/E pending (§20.10). |
| `3.0.0-alpha3-dev-a3-04d-debug` (Alpha 3 A3-04D, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04d-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04d-sd-log-isolation`. SD diagnostic logging (`ALPHA3_AUDIO.md` §21) is **off at boot**: Developer > Diagnostics > *Probe: SD diag logging* turns it on for the session, and serial logging is unchanged. Same packs, same `openu5-audio.bin`, no regeneration. Reports and `A3C_PERF` say `sdlog ON/OFF` and attribute slow TFT transactions to SD bursts (`insd=`). Hardware Tests 1–4 pending (§21.7). |
| `3.0.0-alpha3-dev-a3-04e-debug` (Alpha 3 A3-04E, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04e-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04e-render-pacing`. Renderer and main-loop pacing (`ALPHA3_AUDIO.md` §22): the TFT draw loops yield instead of sleeping to the next tick every 16 rows, and an idle main-loop pass waits for input for one tick instead of spinning. Developer > Diagnostics > *Probe: legacy TFT pacing* / *Probe: legacy loop spin* bring either old behaviour back for the session (both off at boot). The report gains a *Pacing (A3-04E)* section and serial an `A3E_PACE` line. Same packs, same `openu5-audio.bin`, no regeneration. **Withdrawn: tripped the task watchdog on IDLE0 (§23); use A3-04E.1.** |
| `3.0.0-alpha3-dev-a3-04e1-debug` (Alpha 3 A3-04E.1, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04e1-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04e1-idle-service`. A3-04E's pacing plus the idle-service guarantee (`ALPHA3_AUDIO.md` §23): the game thread blocks one tick whenever core 0's idle loop (the task watchdog's feed) has not run for 200 ms. The report's Pacing section ends with `idle0 gap max … forced … miss …`; `A3E_PACE` starts with `hb=N`. Same packs, same `openu5-audio.bin`. **Hardware-validated 2026-09-27** (§23.10): no watchdog trip, pacing confirmed; the validated build of A3-04E's pacing. |
| `3.0.0-alpha3-dev-a3-hf2-debug` (Alpha 3 A3-HF2, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-hf2-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-hf2-ambient-clock`. A3-04E.1 plus the ambient-parity fix (`ALPHA3_AUDIO.md` §24): a grandfather clock strikes only when the game hour changes, not after every step. The fountain was not changed. Same packs, same `openu5-audio.bin`. **Hardware-validated 2026-09-27 (H-198 PASS).** |
| `3.0.0-alpha3-dev-a3-hf2-1-debug` (Alpha 3 A3-HF2.1, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-hf2-1-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-hf2-1-cleanup`. A3-HF2 plus one diagnostic change (`ALPHA3_AUDIO.md` §25): serial `PRESENTATION_DISPATCH` is written when the UI mode or presentation source changes, not every frame. No gameplay or audio change. Same packs, same `openu5-audio.bin`. Optional serial check H-199. |
| `3.0.0-alpha3-dev-a3-04f-debug` (Alpha 3 A3-04F, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-04f-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-a3-04f-render-efficiency`. A3-HF2.1 with a cheaper renderer (`ALPHA3_AUDIO.md` §26): the viewport checksum is table-driven (bit-identical), whole pixel rows share an SPI transaction, adjacent animated cells are one window, and unchanged party / status / transcript rows are not redrawn. The panel is pixel-identical to A3-HF2.1's on the host. No gameplay or audio change. Same packs, same `openu5-audio.bin`. **Hardware-validated 2026-09-27 (H-200 PASS, §26.17).** |
| `3.0.0-alpha3-dev-a3-hf3-debug` (Alpha 3 A3-HF3, **development build, not a release**) | `OpenU5-TDeck-Alpha3.0.0-alpha3-dev-a3-hf3-Debug-Launcher.bin`; its path, size, SHA-256 and `Git` are in tag `alpha3-hf3-combat-hit-feedback`. A3-04F plus the combat hit cue (D-63, `ALPHA3_AUDIO.md` §27): every hit shows the original's star on the struck cell and, for a party member, its roster row in reverse video, for the 174 ms of the hit's burst (ULTIMA.EXE 0x3564). The poisoning and sleep strikes also get their hit burst. No combat rule or save change. Same packs, same `openu5-audio.bin`. Hardware check H-201. |

- Firmware version: `2.0.0-alpha2-rc1-debug` (identity screen: `FW 2.0.0-alpha2-rc1-debug`)
- Size: 878,752 bytes (`0xd68a0`). This leaves 169,824 B (16 %) free in the 1 MiB app partition.
- Minimum 64-KiB-aligned Launcher allocation: 917,504 bytes (896 KiB)
- ESP-IDF: 6.1, target ESP32-S3
- The image SHA-256 and its embedded `Git` hash are recorded in the annotated tag `alpha2-batch54-rc1`.
- Required SD resource pack: 2,041,466 B, CRC32 `26f75ae6` (unchanged since Batch 53)

Release notes and install instructions: [`../../../ALPHA2.md`](../../../ALPHA2.md).
The Alpha 2.0.0 packaging record (779,680 B, SHA-256 `895f7099…0cbe`) is in the
git history of this file and in [ALPHA20_FRONTEND_NEW_GAME.md](ALPHA20_FRONTEND_NEW_GAME.md).
