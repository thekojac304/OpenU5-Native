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
    0x1420ef958ce8cdf7ull, // gameplay
    0x54617d53cd9591baull, // move-lord-british
    0xd979dd1b72f944f2ull, // move-blackthorn
    0x5bcf6f5a31e7850cull, // dungeon-bands
    0x3d7a8aeca46494cbull, // settings
    0x498bcd9840b130ebull, // settings-large
    // Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.3): re-recorded from the A4-UI2 Board for the new
    // System Menu labels ("Save Game", "Load Game"); the other seven states are unchanged.
    0x9b4da31f3546ca4full, // system-menu
    0x089fde73b423dfffull, // main-menu
};
} // namespace a4_ui1_goldens
