# Internal rename plan (`openu5` → Ultima V Native)

**Status: plan only. Nothing here has been executed.**

The public project name is **Ultima V Native**. Some technical identifiers still use the historical `openu5` name. They were left alone when the public name changed because renaming them touches code, firmware or shipped compatibility, and Alpha 4's RC5 firmware is hardware-validated and must not change. This document lists what could be migrated later, ideally alongside the Windows-port work, when a compatibility-aware release is being built anyway.

The browser port in this repository is OpenU5 itself and keeps its name; it is credited as a reference implementation in the [README](../README.md#relationship-to-openu5). Nothing below applies to it.

| Current name | Desired future name | Referenced in | Compatibility risk | Firmware rebuild? | Migration notes |
|---|---|---|---|---|---|
| `openu5-assets.bin` (SD tiles pack) | `ultima-v-native-assets.bin` | `native/targets/tdeck/main/asset_pack.h`, `native/tools/u5pack/cli.ts`, `npm run pack:native`, `ALPHA*.md`, `native/ASSETS.md` | High: Alpha 4 firmware opens this exact path; users' SD cards already hold it | Yes | Firmware should try the new name, then fall back to the old one; the packer writes the new name. Keep the CRC/SHA identity so the pack contents do not change. |
| `openu5-alpha1-resources.bin` (resource pack) | `ultima-v-native-resources.bin` | firmware resource loader, `npm run pack:alpha1`, release notes | High: the firmware refuses any other pack by size/CRC, and the path is fixed | Yes | Same dual-name lookup. The `alpha1` in the name is itself stale; rename once. |
| `openu5-audio.bin` (optional music pack) | `ultima-v-native-audio.bin` | audio pack loader, `npm run pack:audio`, `ALPHA3_AUDIO.md` | Medium: optional file, but existing users have it | Yes | Dual-name lookup; absence must stay a normal case. |
| `openu5_tdeck` (ESP-IDF project name) | `ultima_v_native_tdeck` | `native/targets/tdeck/CMakeLists.txt`, `package_launcher.py` (checks `project_name`), build scripts, `.bin`/`.elf` names | Medium: the packager and guards match the name; build artifact names change | Yes (new image) | Update the packager check and guard scripts in the same change. Release file names (`OpenU5-TDeck-Alpha…-Launcher.bin`) should change only at a version boundary. |
| `OpenU5-TDeck-Alpha4…Launcher.bin` (release file name) | `UltimaV-Native-TDeck-…Launcher.bin` | `package_launcher.py`, release notes, GitHub Release | Low for users, but historical releases keep the old name | Yes (repackage) | Do not rename the Alpha 4 asset; use the new name from the next release. |
| `openu5_core` (CMake project) and the `openu5` C++ namespace, `include/openu5/` headers | `uvn` or `ultima_v_native` | all of `native/core`, `native/targets/tdeck/main`, every test | Medium: a mechanical but very large change; any miss breaks the build | Yes | Do it as one mechanical commit with no behavior change, proven by an identical host suite and an unchanged memory map. |
| `OPENU5_*` CMake options (e.g. `OPENU5_ENABLE_DEVELOPER_TOOLS`) | `UVN_*` | `native/core/CMakeLists.txt`, docs, build commands in notes | Low: build-time only | Rebuild only | Accept both spellings for one release. |
| App / project IDs, embedded identity strings | follow the new project name | `main/` version and identity code, boot identity screen | Medium: hardware checklists and release notes quote `FW …` / `Git …` | Yes | Change with a version bump so identity strings in records stay unambiguous. |
| SD directory `/ultima5/` and save files (`alpha1-g{0,1}.*`, `alpha1-s{2,3}-g*`) | keep | persistence layer, save docs | **Very high**: user saves | Yes | Not recommended to rename. If ever done, read old paths and migrate with an explicit, tested step. |
| Build directories (`native/core/build-*`, `build-a4-rc5/`) | n/a | notes, logs, release records | Low | No | They are historical evidence; leave them. New work can use any naming. |
| `openu5-dev` (`.claude/launch.json`), `openu5-lang` and other web storage keys | keep for the browser port | `game/`, `demo-byo/` | Medium: browser users' stored data | No | Belongs to the OpenU5 browser port, not the native project. |

## Ground rules

1. No rename ships without a host-suite run and, for firmware, a hardware pass.
2. Anything a user's SD card or save depends on gets a fallback to the old name.
3. Rename in separate, mechanical commits; never mix with behavior changes.
4. Historical records (logs, tags, past release notes) keep the names they were written with.

## Public version policy

From v0.4.0 on, public releases use `v0.x.y` versions. Public release titles and asset names carry no "Alpha N", "RC N" or "Debug"; internal candidates may still use RC identifiers. v0.4.0 itself reuses the hardware-validated RC5 bytes, so its embedded identity (`FW 4.0.0-alpha4-rc5-debug`, `Git 59c14b907399`) is deliberately unchanged. At the next firmware-building release, change the embedded version string to the clean public version instead of carrying the Alpha scheme forward. The old git tag `alpha4-release` is kept as the historical internal release tag; the public tag is `v0.4.0`.
