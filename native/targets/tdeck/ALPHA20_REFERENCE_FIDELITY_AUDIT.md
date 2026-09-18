# Alpha 2.0 host/reference fidelity audit

Audit date: 2026-09-17  
Scope: Alpha 2.0 alpha1 frontend, New Journey, first save, Continue/Load, attract isolation, 320x240 HUD, celestial/wind, transcript, settings, resources, state machine, and Alpha 1.4 host regressions.  
Authority: the checked-in OpenU5 reverse-engineering notes, extracted original assets, original-state fixtures, and faithful web-port tests. No physical T-Deck claim is made.

## Result

Eighty-six reference behaviors were audited. The matrix totals are:

| MATCH | PARTIAL | FIXED | DEFERRED | PORT EXTENSION | HARDWARE PENDING | Total |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 46 | 8 | 23 | 2 | 5 | 2 | 86 |

The 23 FIXED rows arise from 17 distinct root defects. All practical BUG and REFERENCE FIDELITY GAP roots found in the audited native scope were fixed. Eight presentation/interaction rows remain explicitly PARTIAL: the complete 49-second title composition, witness-exact startup timing, scripted Return-to-the-View demo, Summoning scene-zero cadence, six Summoning illustration compositions, credits curtain reveal, richer combat status presentation, and original pagination/continuation presentation.

The authoritative reference trail used for the decisions below is:

- `re/notes/intro.md`, `re/notes/intro-scene-tables.md`, `re/notes/intro-attract-loop.md`, and `re/notes/intro-splash-anim-audit.md` for INTRO.OVL.
- `re/notes/gypsy.md`, `re/verified/gypsy.md`, and `re/notes/rng-186-acta.md` for FONT.OVL creation and shared RNG.
- `game/assets/init.gam`, `game/assets/init.ool`, and `game/assets/initial-state.json` for the initial semantic/byte state.
- `re/notes/cielo-176-acta.md` and `game/src/skin/fiel/sky.ts` for sun/moon/wind.
- `game/assets/intro-pics.png`, `intro-pics.json`, `intro-scenes.json`, and `questions.json` for original resources.

## Audit matrix

| # | Subsystem | Reference behavior | Current native behavior | Status | Evidence/test | Fix applied | Physical validation required? |
| ---: | --- | --- | --- | --- | --- | --- | --- |
| 1 | Title | Origin/Lord British stages, four walking figures, dissolve, final logo. | Uses exact final logo/fire assets but omits the full staged composition. | PARTIAL | `intro-splash-anim-audit.md`; `frontend_test` | Scope retained; gap documented. | Yes, after completion. |
| 2 | Title fire | Four original `ultima:1..4` subtitle/fire frames animate. | Exact extracted four-frame cycle. | MATCH | resource pack; target renderer | None. | Visual confirmation only. |
| 3 | Startup timing | Witness sequence is approximately 49 seconds with stage-specific waits. | Adapted 1.4 s title, 4.2 s lead, 12 s preview. | PARTIAL | `intro-splash-anim-audit.md`; constants | Overclaim removed. | Yes. |
| 4 | Menu idle | 200 input-poll ticks dispatch `R` (about 12 s in witness). | 12,000 ms idle dispatch to attract. | FIXED | `frontend_test` idle transition | Corrected 11,000 to 12,000 ms. | Timing feel only. |
| 5 | Attract entry | Startup and idle/`R` enter the View. | All three routes enter `AttractDemo`. | MATCH | `frontend_test` state paths | None. | Visual only. |
| 6 | Return to View demo | Original scripted View/figure sequence. | Read-only live 11x11 native-map preview, not the exact script. | PARTIAL | `intro-attract-loop.md` | Classified, not silently equated. | Yes. |
| 7 | Attract return | Any key returns cleanly to menu. | Any semantic input returns to `MainMenu`. | MATCH | repeated-cycle test | None. | Key feel only. |
| 8 | Summoning content | Twenty-one scenes in authoritative STORY order. | All 21 extracted texts in exact resource order. | MATCH | pack record count; `frontend_test`; focused Vitest | None. | Visual wrapping only. |
| 9 | Summoning waits | Scenes wait for input; no five-second auto-page timer. | Key-driven pages; no timer. | FIXED | `frontend_test` holds scene for 100 s | Removed fabricated five-second advance. | No. |
| 10 | Summoning scene 0 | Scene zero completes without a key after its presentation work. | Treated like the other waiting pages. | PARTIAL | `intro-scene-tables.md` | Gap documented pending exact cadence. | Yes. |
| 11 | Summoning art | Six original illustration compositions accompany scenes. | Authoritative text only. | PARTIAL | intro atlas inventory | No placeholder claimed as original. | Yes. |
| 12 | Acknowledgements art | STARTSC.16 288x137 credits panel. | Exact extracted `startsc:1` RGB565 panel. | FIXED | pack CRC/size; target renderer | Replaced generic text-only/fake port credit. | Visual only. |
| 13 | Credits reveal | Center-out curtain effect. | Static exact panel, key to return. | PARTIAL | `intro.md` | Gap documented. | Yes. |
| 14 | Menu ordering | J, C, T, U, A, R in that order. | First six rows are exact; extensions follow. | MATCH | `frontend_test`; `kMenu` | None. | No. |
| 15 | Direct keys | `JCTUAR` dispatch original functions. | Case-insensitive direct dispatch. | MATCH | `frontend_test` | None. | Keyboard feel only. |
| 16 | Numeric navigation | 1/3 previous, 2/4 next, wrapping. | Exact navigation over visible rows. | FIXED | `frontend_test` wrap checks | Added missing numeric handling. | No. |
| 17 | Acceptance | Enter or Space accepts selected function. | Both accept. | FIXED | `frontend_test` Space/Journey | Added missing Space handling. | No. |
| 18 | Journey gate | Empty Avatar prints exact no-active-game guidance and returns. | Exact message through recoverable error screen. | FIXED | `frontend_test`; runtime string | Restored authoritative text. | No. |
| 19 | Create route | C directly starts FONT character creation. | `NewJourney` transition immediately enters creation. | MATCH | `frontend_test` | None. | No. |
| 20 | U4 Transfer | Import/convert U4 party data. | Visible in original position; clear deferred notice; no mutation or intent; remains at menu. | DEFERRED | `frontend_test` transfer isolation | Safe return path verified; transfer not implemented. | No. |
| 21 | Extended menu | Original has no Load/Settings/Developer rows. | Rows 7-9 are appended; Developer is conditional. | PORT EXTENSION | menu/state tests | Kept after original six. | Visual only. |
| 22 | Name length | Maximum eight bytes. | Eight printable ASCII characters plus terminator. | MATCH | `frontend_test`; code bound | None. | Keyboard entry only. |
| 23 | Name encoding | Original accepts printable single-byte input. | Accepts U+0020..U+007e and stores byte-compatible ASCII. | MATCH | input bounds | None. | Non-US keyboard out of scope. |
| 24 | Empty name | Empty Enter silently aborts to menu. | Empty Enter now returns to menu without save. | FIXED | `frontend_test` | Corrected prior dead wait. | No. |
| 25 | Sex representation | M=`0x0b`, F=`0x0c` in GAM. | Exact values and M/F gate. | MATCH | golden GAM test | None. | No. |
| 26 | Questions | All 28 pairwise virtue questions at authoritative indexes. | Extracted `questions.bin`, 28 records, formula-checked index. | MATCH | packer; gypsy tests | None. | No. |
| 27 | Bracket | 4+2+1 elimination, seven answers, used-set reset each round. | Exact recurrence and bracket. | MATCH | seed-0 golden all-A/all-B vectors | None. | No. |
| 28 | Edge cases | Eliminated virtues cannot return; pair order determines A/B; no stat tie shortcut. | Exact used/eliminated masks and sorted pair. | MATCH | gypsy battery | None. | No. |
| 29 | Stat derivation | Winner bonuses; STR floor 20; INT copied to MP. | Exact output; seed-0 goldens 20/18/22/22 and 20/17/18/18. | MATCH | native and Vitest goldens | None. | No. |
| 30 | Profession | Avatar class remains `A`; creation does not derive another class. | Class, level, XP, HP, equipment retained from INIT.GAM. | MATCH | full-state comparison | None. | No. |
| 31 | Identity field scope | Creation edits only name, gender, STR/DEX/INT/current MP. | Exact creation-only byte patch. | FIXED | byte-for-byte expected GAM | Preserved inactive object-window bytes. | No. |
| 32 | Initial world state | INIT semantics: party Avatar/Shamino/Iolo, karma 75, food 63, gold 150, keys 2, gems 0, torches 4, location 13 at 15,15, date 139/4/5 08:35, foot; original equipment/spells/reagents/moonstones/NPC and progression state. | Full imported state and arrays retained; only identity fields change. | MATCH | `initial-state.json`; full `GameState`/`TurnState` comparison | None. | No. |
| 33 | RNG continuity | INTRO clock hash seeds shared RNG; FONT consumes it; gameplay continues resulting stream. | Uptime clock components hash once on first menu; post-bracket seed assigned to gameplay RNG. | FIXED | injected timestamp and seed-after assertion | Removed fixed-zero/private-stream behavior. | RTC-equivalence observation only. |
| 34 | First GAM | Original modifies INIT.GAM in place. | 4,192-byte output differs only at exact identity/stat offsets. | FIXED | deterministic byte golden | Creation-only template-byte preservation. | No. |
| 35 | First OOL | New SAVED.OOL equals 512-byte INIT.OOL seed in interior. | Exact copy from packed INIT.OOL. | FIXED | byte equality test | Added INIT.OOL resource and save seed. | No. |
| 36 | JSON sidecar | Original has no JSON sidecar. | Versioned semantic owner for native-only/unmapped state. | PORT EXTENSION | encode/reboot round trip | Kept separate from original GAM/OOL. | No. |
| 37 | Stale state | New creation cannot inherit prior commands, enemies, terrain, actors, combat, dungeon, quest/modal or map overrides. | All volatile owners reset before identity/save. | FIXED | full-state golden; runtime reset audit | Added explicit owner resets. | No. |
| 38 | Save transaction | A failed write preserves prior generation. | GAM/OOL/JSON temp files, CRC verification, commit last. | MATCH | persistence tests; code audit | None. | SD fault injection later. |
| 39 | Journey selection | Journey Onward loads the active/latest valid game. | Selects highest semantically valid committed generation. | FIXED | 53-test suite; candidate audit | Added semantic validation and fallback. | SD fault injection later. |
| 40 | Explicit Load | No original equivalent. | User can choose either valid recovery generation. | PORT EXTENSION | frontend load test | Does not change Journey semantics. | No. |
| 41 | Missing/corrupt saves | Original refuses missing/empty save and returns. | Empty/corrupt labeled and recoverable; no partial state commit. | FIXED | frontend/persistence tests | Transactional scratch restore. | SD fault injection later. |
| 42 | Incompatible newest | A bad newest generation must not mask a usable older one. | Newest semantic failure is excluded and older generation retried. | FIXED | save candidate code; recovery fixture | Added semantic fallback loop. | SD fault injection later. |
| 43 | Attract GameState | Demo must not alter live state. | Presentation reads a snapshot only. | MATCH | eight-cycle state hash | None. | No. |
| 44 | Attract RNG/clock | Presentation must not draw RNG or advance game time. | Hashes of RNG, `GameState`, and `TurnState` unchanged. | MATCH | eight-cycle state hash | None. | No. |
| 45 | Attract persistence | Demo must not touch saves/settings/generations. | No persistence intent or storage call exists on attract path. | MATCH | intent checks/code audit | None. | No. |
| 46 | Attract repetition | Repeated enter/exit cannot accumulate modal/demo state. | Eight entry/return cycles end at identical hashes/menu. | MATCH | deterministic isolation test | None. | No. |
| 47 | HUD viewport | Original map/combat view remains primary. | 176x176 11x11 authoritative tile viewport. | MATCH | presentation regression | None. | Visual only. |
| 48 | HUD party | Persistent party names, HP and conditions. | All active party members shown in compact 3x2 roster. | FIXED | renderer audit/build | Replaced Avatar-only row. | Readability. |
| 49 | HUD transcript | Running command/dialogue/event log. | Eight wrapped transcript lines in right panel. | MATCH | `UiSession`; log tests | None. | Readability. |
| 50 | HUD prompt | Active prompt/input follows transcript and clears on transition. | Prompt then input at log bottom; modal completion/cancel clears both. | FIXED | `ui_session_test` | Removed stale prompt leakage. | Readability. |
| 51 | HUD location | Meaningful current location/status. | Real location display name and transport/mode; no raw generic `L#` label. | FIXED | renderer audit | Restored name lookup. | Visual only. |
| 52 | HUD time/wind | Persistent day/time and wind. | Day, HH:MM and authoritative wind label in lower-left. | FIXED | renderer audit | Reclaimed unused label area. | Visual only. |
| 53 | HUD sky | Surface sun/moons and wind shown where original shows them. | State-derived band with original gating and wind labels. | MATCH | `frontend_test`; focused sky tests | None. | Glyph appearance. |
| 54 | Combat status | Original uses view, roster/status, active combat messaging and prompt cues. | View/targets, roster conditions and combat transcript present; no dedicated richer arena-status strip. | PARTIAL | presentation/combat tests | Gap documented; no redesign. | Yes. |
| 55 | HUD native-only space | Native-only labels must not displace original information. | Removed `[ future context actions ]`; space now carries day/time/wind. | FIXED | renderer audit | Removed placeholder. | No. |
| 56 | Sun logic | Cell `17-hour`, visible only 0..11, glyph `0x2a`, surface-gated. | Exact formula/gate. | MATCH | representative hours | None. | Glyph appearance. |
| 57 | Moon positions | Felucca `8-hour`, Trammel `2-hour`, add 24 only below -12. | Exact position rules. | MATCH | hour boundary tests | None. | Glyph appearance. |
| 58 | Moon phases | Display uses turn-latched phases; transitions occur in gameplay time logic. | HUD reads latched phases and never recomputes/mutates them. | MATCH | moon latch tests | None. | No. |
| 59 | Wind state | Display is the same `TurnState.wind` used by gameplay. | Exact Calm/N/S/E/W label mapping. | MATCH | five-direction test | None. | No. |
| 60 | Presentation purity | Drawing sky/wind cannot mutate time, RNG, wind or transport. | Pure `hud_world_state` value computation. | MATCH | before/after state tests | None. | No. |
| 61 | Sailing effects | Gameplay wind drives ship/sail mechanics; HUD only observes. | Existing transport/turn parity uses same wind owner. | MATCH | travel/transport parity | None. | Physical input only. |
| 62 | Transcript ordering | Echo, response, prompt and typed value remain semantically ordered. | Single append-only `UiSession` stream plus current prompt/input tail. | MATCH | UI/presentation tests | None. | No. |
| 63 | Dialogue/shops/combat | Mode messages and prompts remain visible without duplication. | Shared transcript for all modes; 53-suite mode regressions pass. | MATCH | dialogue/shop/combat tests | None. | Visual only. |
| 64 | Look/inventory/errors | Original names/text/errors remain meaningful. | Shared display-name/resources and transcript paths retained. | MATCH | display/look/item/command tests | None. | Visual only. |
| 65 | Pagination | Original text windows have mode-specific continuation behavior. | Fixed-height wrapped rolling log; semantic text retained but exact `More`/page cadence not reproduced. | PARTIAL | log-scroll/prompt tests | Gap documented. | Yes. |
| 66 | Stale prompt | Leaving/canceling a mode must not leave an old prompt. | Finish and cancel clear prompt/input. | FIXED | new `ui_session_test` assertions | Added explicit clearing. | No. |
| 67 | Settings ownership | Settings are not game state. | Separate `/sd/ultima5/settings.json`; not encoded in GAM/OOL/sidecar. | MATCH | settings codec/code audit | None. | SD persistence later. |
| 68 | Settings determinism | Changes cannot alter saves or gameplay RNG. | Codec/store path has no game/RNG owner; attract/settings hash tests stable. | MATCH | settings and isolation tests | None. | No. |
| 69 | Settings reboot | Versioned preferences survive restart. | Temp+rename store and strict version decoder. | MATCH | codec round trip | None. | SD persistence later. |
| 70 | Brightness | Port preference should control real backlight without gameplay effect. | Persisted, not yet applied/calibrated. | HARDWARE PENDING | settings audit | No hardware assumption made. | Yes. |
| 71 | Movement/trackball | Port preferences apply only to semantic input adapter. | Applied live and after boot; no gameplay-state field. | MATCH | input/settings tests | None. | Physical feel. |
| 72 | UI size | Port-only preference. | Persisted/reserved; alternate metrics intentionally not applied. | PORT EXTENSION | settings codec | Classified, not claimed functional. | If implemented later. |
| 73 | Developer visibility | Port-only conditional tool entry. | Build capability AND persisted visibility gate. | PORT EXTENSION | debug/frontend tests | Kept outside original six. | No. |
| 74 | Sound/music/touch | Not in this audit's implementation scope. | Reserved fields only; no audio or final touch behavior. | DEFERRED | settings schema | No implementation added. | Yes, when implemented. |
| 75 | Title resources | Real extracted logo and four fire frames; correct order. | Packed exact RGB565 assets with record/stride validation. | MATCH | packer/resource loader | None. | Visual only. |
| 76 | Intro/questions | 21 scene texts and 28 questions at correct indexes, no generic IDs. | Exact extracted resources and record counts. | MATCH | packer; extractor/focused tests | None. | Wrapping only. |
| 77 | Credits/save seeds | Real credits art and INIT.OOL must be in runtime pack. | `credits.rgb565` and `init.ool` required and size-validated. | FIXED | 28-entry pack; SHA-256 | Added both resources. | Visual/SD copy only. |
| 78 | Resource mismatch | Missing/wrong pack fails safely. | Size, version, CRC, entry and per-entry checks retained. | MATCH | resource validation/smoke harness compile | None. | Device error screen later. |
| 79 | State cancel/confirm | Cancel/confirm routes return or commit without stale modal state. | Creation/intro/load/settings/credits/error paths are recoverable. | FIXED | frontend/UI tests | Empty-name and prompt cleanup corrected. | Key feel only. |
| 80 | Re-entry/error recovery | Repeated entry/exit and errors cannot produce unreachable/stuck states. | Deterministic transitions; error key returns to menu. | MATCH | state-machine battery | None. | No. |
| 81 | Input leakage | Frontend input cannot dispatch gameplay commands. | Runtime short-circuits to frontend handler while frontend active. | MATCH | runtime code audit | None. | No. |
| 82 | Alpha 1.4 Mic/WASD/movement | Short Mic cancel, hold Movement Mode, semantic directions retained. | Existing adapters unchanged; host regressions pass. | MATCH | input/movement regressions | None. | Physical Mic/trackball feel. |
| 83 | Keyboard transport | Alpha 1.4 transport stability remains. | Host transport/travel parity passes. | HARDWARE PENDING | transport suites | No hardware claim. | Yes. |
| 84 | Debug/teleport/shops/inventory/Look | Existing Alpha 1.4 features remain. | Debug, map picker, shops, names, Look/sign suites pass. | MATCH | 53/53 native suite | None. | Spot check later. |
| 85 | Combat/enemy/post-combat | Targeting, identity and routing remain. | Combat/advanced-combat/dungeon suites pass. | MATCH | native parity suite | None. | Spot check later. |
| 86 | Resources/smoke harness | Validation and diagnostic harness remain available. | Harness compiles into target; resource tests pass. | MATCH | target build; smoke cases | None. | Run device smoke harness later. |

## Distinct discrepancies and fixes

The 17 fixed roots were classified as follows:

1. **REFERENCE FIDELITY GAP:** menu idle used 11 seconds rather than the 200-tick witness approximation; corrected to 12 seconds.
2. **REFERENCE FIDELITY GAP:** Summoning invented a five-second auto-page timer; removed.
3. **REFERENCE FIDELITY GAP:** original numeric 1/2/3/4 navigation was missing; restored.
4. **REFERENCE FIDELITY GAP:** Space did not accept a menu item; restored.
5. **REFERENCE FIDELITY GAP:** empty-name Enter stalled instead of silently aborting; corrected.
6. **REFERENCE FIDELITY GAP:** creation used fixed seed zero/private RNG; replaced with INTRO clock hash and shared post-bracket gameplay seed.
7. **BUG:** gameplay could retain a stale RNG seed after New Journey; explicitly replaced by the bracket's resulting seed.
8. **BUG:** modal finish/cancel could leave stale prompt/input text; cleared on both paths.
9. **BUG:** a CRC-valid but semantically bad newest save could block the fallback generation and partially mutate owners; candidate validation and restore are now transactional with fallback.
10. **REFERENCE FIDELITY GAP:** Journey's missing-save message was generic; restored exact guidance.
11. **REFERENCE FIDELITY GAP:** credits used placeholder/fabricated port text; replaced with exact STARTSC panel.
12. **REFERENCE FIDELITY GAP:** HUD showed only the Avatar and raw IDs; now shows the active roster and real location name.
13. **REFERENCE FIDELITY GAP:** unused native-only HUD text occupied original-information space; replaced with day/time/wind.
14. **BUG:** New Journey did not reset every volatile command/world/modal owner before saving; reset is explicit.
15. **REFERENCE FIDELITY GAP:** generic export rewrote inactive GAM object bytes on the first save; creation-only output now retains INIT.GAM there.
16. **REFERENCE FIDELITY GAP:** runtime lacked INIT.OOL and synthesized the first overlay; exact 512-byte seed is now packed and used.
17. **BUG/BUILD GATE:** the target component did not actually enforce warnings-as-errors; it now does, and five guarded narrowing conversions were made explicit.

Intentionally unimplemented: Ultima IV transfer, audio, final touch controls, and unrelated gameplay. Port extensions remain explicitly labeled: Load Game, Settings, conditional Developer, JSON sidecar, and reserved UI size.

## Golden New Journey/save result

The deterministic native golden begins from the exact 4,192-byte INIT.GAM and 512-byte INIT.OOL. It proves:

- name is at most eight printable single-byte characters; M/F store `0x0b`/`0x0c`;
- the 28-question mapping, 4+2+1 bracket, and stat formulas match the reference;
- first GAM changes only name, gender, STR, DEX, INT and current MP;
- first OOL equals INIT.OOL byte for byte;
- the JSON sidecar reboots to a full `GameState`/`TurnState` semantic match;
- Avatar remains class A; the initial party is Avatar, Shamino and Iolo;
- food 63, gold 150, keys 2, gems 0, torches 4, karma 75, on foot;
- year 139, month 4, day 5, 08:35, location 13/floor 0 at 15,15;
- equipment, spells, scrolls, potions, reagents, moonstones, NPC bitmaps, shrine/dungeon/progression state and all non-identity fields remain INIT-derived;
- no prior commands, enemies, actor walks, terrain overrides, combat/dungeon state, quest modal, debug state or prior session RNG survives.

## Validation record

- Native host configure/build with `-Wall -Wextra -Wconversion -Werror`: passed.
- Native CTest: **53/53 passed**, including frontend, UI, persistence, movement, input, presentation, shops, dialogue, combat, travel, quest, dungeon and debug parity/drift gates.
- Focused authoritative web battery: **211/211 passed** across gypsy, faithful intro/Summoning, title animation, key behavior, sky/moon latch, save-native, log and prompt tests.
- Extractor suite during the repository-wide run: **232 passed, 2 skipped**.
- Repository-wide game suite was also attempted: **7,499 passed, 98 failed, 106 skipped**. It is not a clean Windows/public-checkout gate: failures include absent private route/disassembly/capture/save corpus, Windows path/symlink assumptions, missing `python` command alias, and unrelated source-shape ledgers. The focused Alpha/reference battery above is green; this audit does not claim the unrelated full web suite is green.
- ESP-IDF 6.1 ESP32-S3 build with target component warnings-as-errors: passed.
- Partition check: app binary **709,136 bytes**, 1 MiB partition, **339,440 bytes (32%) free**.
- Size report: flash code 464,046 B; flash data 160,708 B; DIRAM 100,710/341,760 B (29.47%, 241,050 B remain); IRAM 16,384/16,384 B; total unpadded image 709,012 B.
- Resource pack: 28 entries, **1,605,980 bytes**, payload CRC32 `d0000db2`, SHA-256 `9c028f7dac268285e300f69ffb521df60174a0721f02fb92b3c9f3d6fcbb2ba5`.
- Launcher package: **709,136 bytes**, SHA-256 `cc67d395d145e3e5927c70a92d3a946cdd9a21c27f7a22b445e8b6824592e365`.
- `git diff --check`: passed (only existing Windows LF-to-CRLF advisory messages).

## Flash/RAM/PSRAM impact

- Alpha 1.4 alpha2 app baseline: 692,576 B. Audited Alpha 2.0 app: 709,136 B. Delta: **+16,560 B flash**.
- Alpha 2.0 resource pack: 1,605,980 B. Relative to the pre-frontend 1,526,428 B pack: **+79,552 B SD storage**.
- New PSRAM-backed frontend/save-seed payload: **378,319 B** (title 281,600; intro text 11,160; questions 6,135; credits art 78,912; INIT.OOL 512).
- Static size tool reports DIRAM as above. Runtime peak/free internal RAM and free PSRAM require a real device; no physical margin claim is made.

## Exact artifacts

- Launcher binary: `C:\Dev\OpenU5-Native\native\targets\tdeck\build-alpha20-final\launcher\OpenU5-TDeck-Alpha2.0.0-alpha1-Debug-Launcher.bin`
- Resource pack: `C:\Dev\OpenU5-Native\native\assets\openu5-alpha1-resources.bin`
- This audit: `C:\Dev\OpenU5-Native\native\targets\tdeck\ALPHA20_REFERENCE_FIDELITY_AUDIT.md`

## Physical Alpha 2.0 checklist

1. Copy the exact resource pack to `/ultima5/openu5-alpha1-resources.bin`; verify resource identity and boot the exact Launcher image.
2. Observe logo/fire/title cadence, idle timeout, Return-to-the-View return, exact credits panel, and all 21 Summoning texts; note the documented partial title/demo/art/curtain items.
3. Exercise 1/2/3/4, arrows/WASD, Enter, Space, `JCTUARLS`, short Mic cancel and long Mic Movement Mode without input leakage.
4. Create M and F Avatars using multiple answer paths; power-cycle and verify identity, stats, party, inventory, location, date/time and progression defaults.
5. Save twice, corrupt only the newest generation, and verify Journey fallback plus explicit Load labels; repeat with an incompatible JSON sidecar.
6. Loop idle attract and `R` entry/exit, then compare save generation numbers and live gameplay state.
7. Advance real gameplay through representative sun/moon hours and all wind directions; confirm drawing causes no extra turn/RNG/wind/transport change.
8. Check six-member HUD readability, active prompt cleanup across dialogue/shop/combat cancel/finish, and long-text continuation behavior.
9. Change Movement Mode/trackball/settings, reboot, and confirm the game save is unchanged; calibrate brightness separately.
10. Run the existing device smoke harness and Alpha 1.4 checks for keyboard transport, debug/teleport, shops, Look/signs, inventory names, combat targeting/enemy identity and post-combat routing; record internal RAM/PSRAM watermarks.
