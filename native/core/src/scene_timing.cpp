#include "openu5/scene_timing.h"

#include <cstring>

// Batch 51. The Camp apparition's waits, one per point of rest.cpp's event
// stream, each named after the OUTSUBS camp_results instruction that spends
// it. The derivation is in the header; this is only the lookup.
namespace openu5 {
namespace {
bool cue(const GameEvent &e, const char *id) {
    return e.kind == GameEventKind::Sfx && e.text && std::strcmp(e.text, id) == 0;
}
} // namespace

bool camp_apparition_event(const GameEvent &e) {
    switch (e.kind) {
    case GameEventKind::CampSceneBegin:
    case GameEventKind::CampActorWake:
    case GameEventKind::CampViewportXor:
    case GameEventKind::CampViewportRestore:
        return true;
    case GameEventKind::Sfx:
        return cue(e, kApparitionMaterializeCue) || cue(e, kApparitionArpeggioCue) ||
               cue(e, kApparitionChimeCue) || cue(e, kApparitionChordCue);
    default:
        return false;
    }
}

uint32_t camp_apparition_wait_ms(const GameEvent &e) {
    switch (e.kind) {
    case GameEventKind::CampSceneBegin:
        // 0x06c2 fizzle_in(5,5,0x174): the figure over the fire. No timer [C];
        // its first frame is held for the presentation floor [D].
        return kFizzleFloorMs;
    case GameEventKind::CampActorWake:
        // 0x087f run_n_frames(1): the member is drawn standing. [A]
        return run_n_frames_ms(kApparitionWakeFrames);
    case GameEventKind::CampViewportXor:
        // 0x08aa rect_XOR: the chord cue that follows carries the hold.
        return 0;
    case GameEventKind::CampViewportRestore:
        // 0x08c4-0x08d9: three run_n_frames(1); the first repaints over the XOR. [A]
        return run_n_frames_ms(kApparitionRestoreFrames);
    case GameEventKind::Sfx:
        if (cue(e, kApparitionMaterializeCue)) return tone_sweep_ms(kApparitionMaterializeSamples);
        if (cue(e, kApparitionArpeggioCue)) return tone_sweep_ms(kApparitionArpeggioSamples);
        if (cue(e, kApparitionChimeCue)) return tone_sweep_ms(kApparitionChimeSamples);
        if (cue(e, kApparitionChordCue)) return tone_sweep_ms(kApparitionChordSamples);
        return 0;
    default:
        return 0;
    }
}

} // namespace openu5
