#pragma once

#include <cstddef>
#include <cstdint>

#include "openu5/audio.h"

// Alpha 3 A3-03 -- the SFX inventory: what every cue id and every PC-speaker
// call site of the 1988 binaries is, and the presentation-time derivations
// that turn an event the core already emits into the original's sound.
// Derivations and the census: native/targets/tdeck/ALPHA3_AUDIO.md section 16.
//
// Pure data and string matching: no GameState, no clock, no RNG.
namespace openu5 {

/** Why a cue does or does not sound on the device. */
enum class SfxStatus : uint8_t {
    Implemented,          // compile_sfx has the original's program and the device routes it
    DeferredMusic,        // belongs to the music track (A3-04), not the speaker
    DeferredPresentation, // the sound needs a presentation the device does not run yet
    EvidenceUnknown,      // the site is known but its owner / trigger is not established
    IntentionallySilent,  // a marker or a disputed attribution: no sound of its own
    NoNativeEvent         // the original's event is not produced by the native core
};
const char *sfx_status_name(SfxStatus);

/** Every SfxId's status and a one-line reason / route. */
SfxStatus sfx_status(SfxId);
const char *sfx_status_note(SfxId);

/** One call of a PC-speaker primitive in the shipped binaries. */
struct SfxSite {
    const char *module;    // "ULTIMA.EXE", "SJOG.OVL", ... (the A3-01 census spelling)
    uint16_t offset;       // the call's file offset (ULTIMA.EXE: past the MZ header)
    const char *primitive; // TONE_SWEEP / NOISE_BURST / GLIDE / BEEP / SET_TONE / SPK_STOP
    SfxId cue;             // SfxId::None for a primitive's own body
    SfxStatus status;
    bool census;           // listed by re/tools/a3_01_sound_census.py (false: found by hand)
    const char *what;
    SfxId also = SfxId::None; // a second cue the same call serves (e.g. 0x2fd0: death and chest trap)
};
/** The whole site table; `count` receives its length. */
const SfxSite *sfx_sites(size_t &count);

/**
 * The combat message derivations (the reference's sfxForCombatEvent, widened
 * to every message-adjacent speaker call A3-03 re-read). `text` is the combat
 * event's text; `ended` is true for the CombatEventKind::Ended line, which
 * never sounds (it repeats the latch's "VICTORY!" and can fire twice).
 */
SfxId sfx_for_combat_text(const char *text, bool ended);
/** World message derivations (GameEventKind::Message text, exact literals). */
SfxId sfx_for_world_text(const char *text);

} // namespace openu5
