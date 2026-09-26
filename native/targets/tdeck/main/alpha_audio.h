#pragma once

#include <cstdint>
#include <memory>

#include "openu5/audio_pack.h"

namespace tdeck {

// A3-01. The optional SD audio pack (format: openu5/audio_pack.h). It is read
// once at boot, after the game packs passed their identity gate, and it never
// takes part in that gate: a missing, stale or corrupt audio pack only means
// "no music", with the reason shown in Settings.
constexpr char kAudioPackPath[] = "/sd/ultima5/openu5-audio.bin";

/**
 * Alpha 3 A3-04. Ownership target for `load_audio_pack_info`'s `retain`
 * parameter: the whole pack file, kept alive only when there is something in
 * it worth playing (state Valid, capability SupportedMusicPatch), plus the
 * pointers into it a music player needs (openu5::AudioPackPayload -- the
 * same ones inspect_audio_pack's validation walk already found). Left empty
 * (bytes null, payload all-null) for every other capability: there is
 * nothing to keep resident, by construction, not by omission.
 */
struct RetainedAudioPayload {
    std::unique_ptr<uint8_t[]> bytes;
    openu5::AudioPackPayload payload{};
};

/**
 * Read and validate the audio pack at `path`. Never fails: the result says
 * why. `retain`, if given, is filled with the pack's own bytes (moved in,
 * not copied) and payload pointers into them -- but ONLY when the pack is
 * playable; otherwise the whole 512 KiB-at-most buffer is freed here, same
 * as when `retain` is null (every existing caller: A3-01..A3-03 host tests
 * and the identity-gate scan in a3_01_audio_runtime_test.cpp's I4, which
 * greps main.cpp for this exact call).
 */
openu5::AudioPackInfo load_audio_pack_info(const char *path = kAudioPackPath, RetainedAudioPayload *retain = nullptr);

} // namespace tdeck
