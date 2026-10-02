#pragma once
// Alpha 4 UI Batch 1 (ALPHA4_UI.md): the whole 320x240 screen after each scripted
// state of a4_ui1_chrome_runtime, FNV-1a over every pixel. Recorded from the
// reviewed A4-UI1 Board with `a4_ui1_chrome_runtime <pack> <IBM.CH> --record <this file>`.
#include <cstddef>
#include <cstdint>
namespace a4_ui1_goldens {
constexpr size_t kCount = 8;
constexpr const char *kName[8] = {"gameplay", "move-lord-british", "move-blackthorn", "dungeon-bands", "settings", "settings-large", "system-menu", "main-menu"};
constexpr uint64_t kScreen[8] = {
    // Alpha 4 A4-UI4 (ALPHA4_UI.md section 8.20): the four gameplay states re-recorded for the
    // console package's live prompt row -- its bullet and the wave cursor, the only 42 pixels that
    // changed (x 184..195, y 232..239; a4-ui4-console-golden-proof.log).
    0xc4cad6349313aad7ull, // gameplay
    0x1e01bd0524dc02faull, // move-lord-british
    0x565de9b6a2d7d312ull, // move-blackthorn
    0x21ea0f6bf5e14e1cull, // dungeon-bands
    // Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10): the two Settings states re-recorded for the
    // trackball row's new text ("Trackball speed: 10/10"); only that row's pixels changed
    // (a4-enh1-p2-golden-proof.log). Then again for the removed "Developer: Hidden" row, the
    // only pixels that changed (a4-enh1-p3-golden-proof.log).
    // Alpha 4 A4-POLISH3 (section 13): again for the appended "Keyboard Backlight: Off" row, the
    // only pixels that changed (a4-polish3-golden-proof.log).
    0x914e832c2718da41ull, // settings
    0x2e0ea57d2967bfd7ull, // settings-large
    // Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.3): re-recorded from the A4-UI2 Board for the new
    // System Menu labels ("Save Game", "Load Game"); the other seven states are unchanged.
    // Alpha 4 A4-ENH1 (section 10): re-recorded for the root's new "Cheats" row (Return to Title
    // one row lower); only those two rows changed (a4-enh1-p4-golden-proof.log). Then for the
    // "Difficulty" row before it (a4-enh1-p5-golden-proof.log).
    0x1e17f66e36dd2a2dull, // system-menu
    // Alpha 4 A4-SAVE3 (ALPHA4_UI.md section 5): re-recorded for the new "PC Save Transfer" row
    // (the eighth row of a non-developer menu); the other seven states are unchanged.
    // Alpha 4 A4-UI4 (ALPHA4_UI.md section 8): re-recorded for the footer that names every hotkey
    // ("Select: arrows / Enter / J C T U A R S P"); the other seven states are unchanged.
    0xe9df6a7eb8cbc53bull, // main-menu
};
} // namespace a4_ui1_goldens
