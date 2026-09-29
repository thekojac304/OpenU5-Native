#pragma once
// Alpha 4 UI Batch 1 (ALPHA4_UI.md): the whole 320x240 screen after each scripted
// state of a4_ui1_chrome_runtime, FNV-1a over every pixel. Not recorded yet: the
// golden is recorded from the reviewed A4-UI1 Board in its own commit, with
// `a4_ui1_chrome_runtime <pack> <IBM.CH> --record <this file>`.
#include <cstddef>
#include <cstdint>
namespace a4_ui1_goldens {
constexpr size_t kCount = 0;
constexpr const char *kName[1] = {""};
constexpr uint64_t kScreen[1] = {0};
} // namespace a4_ui1_goldens
