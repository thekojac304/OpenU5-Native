#pragma once

#include "openu5/audio_pack.h"

namespace tdeck {

// A3-01. The optional SD audio pack (format: openu5/audio_pack.h). It is read
// once at boot, after the game packs passed their identity gate, and it never
// takes part in that gate: a missing, stale or corrupt audio pack only means
// "no music", with the reason shown in Settings.
constexpr char kAudioPackPath[] = "/sd/ultima5/openu5-audio.bin";

/** Read and validate the audio pack at `path`. Never fails: the result says why. */
openu5::AudioPackInfo load_audio_pack_info(const char *path = kAudioPackPath);

} // namespace tdeck
