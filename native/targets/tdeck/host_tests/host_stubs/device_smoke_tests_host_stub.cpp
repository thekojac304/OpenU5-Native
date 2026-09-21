// Batch 11 host-test seam.
//
// Harmless-sink stand-in for DeviceSmokeTests's PUBLIC interface
// (device_smoke_tests.h, unmodified). The real implementation
// (device_smoke_tests.cpp) writes a diagnostic log to the SD card via
// POSIX mkdir()/fopen() paths that only resolve under ESP-IDF's mounted
// VFS (and, incidentally, don't even compile against a Windows libc's
// single-argument mkdir()). The developer-menu smoke-test harness this
// class drives is not part of the command-routing seam Batch 11 host-tests
// exercise (Move/Open/ViewGem/Klimb never call it), so "nothing runs, no
// scenarios reported" is a faithful, harmless stand-in.
//
// Never linked into the T-Deck firmware: only this CMake host test target
// compiles it. device_smoke_tests.cpp (the real device implementation) is
// untouched.
#include "device_smoke_tests.h"

namespace tdeck {

void DeviceSmokeTests::start(int) {}
bool DeviceSmokeTests::pump() { return false; }
SmokeView DeviceSmokeTests::view() const { return SmokeView{}; }
const char *DeviceSmokeTests::group_name(size_t) { return "host-stub"; }

} // namespace tdeck
