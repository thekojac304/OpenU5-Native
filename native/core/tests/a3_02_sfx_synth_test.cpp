// Alpha 3 A3-02 -- the PC-speaker synthesizer in the portable core: the
// primitives measured from the PCM they render, the cue table against the
// binary's constants and the TypeScript reference, the harpsichord, and the
// playback policy. Pure: no runtime, no clock.
//
//   a3_02_sfx_synth <a3-02-sfx-reference.txt> <native/core dir>
#include "openu5/audio.h"
#include "openu5/scene_timing.h"
#include "openu5/sfx_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label);
}
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

/** A program rendered alone, unscaled (the voice's own output). */
std::vector<int32_t> voice_pcm(const SpeakerProgram &p, uint16_t seed = kNoiseSeed) {
    uint16_t noise = seed;
    SpeakerVoice v;
    v.start(p, &noise);
    std::vector<int32_t> out;
    int32_t buf[128];
    for (size_t got; (got = v.render(buf, 128)) > 0;) out.insert(out.end(), buf, buf + got);
    return out;
}
std::vector<int32_t> cue_pcm(SfxId id, int32_t param = 0) {
    SpeakerProgram p;
    compile_sfx(id, param, p);
    return voice_pcm(p);
}
/** Pitch from the upward zero crossings in [from, to): periods between the first and the last one. */
double pitch_hz(const std::vector<int32_t> &s, size_t from, size_t to) {
    size_t n = 0, first = 0, last = 0;
    for (size_t i = from + 1; i < to && i < s.size(); ++i)
        if (s[i - 1] <= 0 && s[i] > 0) {
            if (!n) first = i;
            last = i;
            ++n;
        }
    return n > 1 ? double(n - 1) * kSfxOutputRateHz / double(last - first) : 0.0;
}
/** The source with // comments removed, so a scan never matches prose. */
std::string strip_comments(const std::string &src) {
    std::string out;
    std::istringstream in(src);
    for (std::string line; std::getline(in, line);) {
        const auto c = line.find("//");
        out += (c == std::string::npos ? line : line.substr(0, c)) + "\n";
    }
    return out;
}
double positive_fraction(const std::vector<int32_t> &s, size_t from, size_t to) {
    size_t n = 0;
    for (size_t i = from; i < to; ++i) n += s[i] > 0;
    return double(n) / double(to - from);
}
int32_t peak(const std::vector<int32_t> &s) {
    int32_t m = 0;
    for (auto v : s) m = std::max(m, std::abs(v));
    return m;
}
std::vector<int16_t> player_pcm(SfxPlayer &p, size_t frames, uint16_t gain) {
    std::vector<int16_t> out(frames);
    p.render(out.data(), frames, gain);
    return out;
}
SfxRequest req(SfxId id, int32_t param = 0) {
    SfxRequest r;
    r.id = id;
    r.param = param;
    r.gain_q15 = kUnityGainQ15;
    return r;
}
bool near(double a, double b, double rel) { return std::fabs(a - b) <= std::fabs(b) * rel; }

// DATA.OVL tables, restated here from the bytes (fileoff = DS + 0x10) so a
// wrong table in production cannot also be the oracle.
constexpr uint16_t kNotes[10] = {0x1eab, 0x0c2c, 0x0da9, 0x0f56, 0x103f, 0x123c, 0x1478, 0x16fa, 0x1857, 0x1b53};
constexpr uint16_t kInc[9] = {8810, 7830, 7060, 6550, 5950, 5570, 5180, 4820, 4480};
constexpr uint16_t kUp[9] = {2700, 3000, 1000, 100, 5000, 4000, 2500, 1000, 1};
constexpr uint16_t kDown[9] = {32700, 31000, 37000, 45000, 31000, 34000, 36500, 39000, 42000};
constexpr uint16_t kStep[9] = {3, 2, 2, 2, 1, 1, 1, 1, 1};

struct RefRow {
    std::string cue;
    int n = 0, seg = 0;
    std::string kind;
    double f0 = 0, f1 = 0, ms = 0;
    int steps = 0;
};
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    const std::string core_dir = argv[2];

    // ======================================================================
    // SYNTH -- the primitives, measured.
    // ======================================================================
    {
        // Y1 fixed tone: beep 0x22c0 is a PIT square at clock / floor(clock / v).
        SpeakerProgram p;
        p.segments[0] = speaker_beep(1000, 200);
        p.count = 1;
        const auto s = voice_pcm(p);
        const double hz = pitch_hz(s, 200, s.size() - 200);
        std::printf("  beep 1000 measured %.2f Hz\n", hz);
        check(pit_quantized_millihertz(1000) == 1000152u && near(hz, 1000.15, 0.002),
              "Y1 a fixed tone (beep 1000) renders at the PIT's 1193182/1193 = 1000.15 Hz");
        const auto blocked = cue_pcm(SfxId::MoveBlocked);
        check(near(pitch_hz(blocked, 200, blocked.size() - 200), 165.0, 0.02),
              "Y1 move-blocked beep(0xa5,0xc8) renders at 165 Hz");
        const auto note = cue_pcm(SfxId::InstrumentNote, 1);
        const double want = sweep_millihertz(0x0c2c) / 1000.0;
        check(near(want, 1227.0, 0.001) && near(pitch_hz(note, 300, note.size() - 300), want, 0.02),
              "Y1 a tone_sweep's pitch is inc/65536 x 25806 Hz (harpsichord digit 1 = 1227 Hz), measured");
    }
    {
        // Y2 sweep: start/step sweep the DUTY (bx), never the pitch.
        SpeakerProgram p;
        compile_sfx(SfxId::TimeSpell, 7, p);
        const auto &up = p.segments[1], &down = p.segments[2];
        check(p.count == 3 && up.kind == SpeakerPrimitive::Sweep && down.kind == SpeakerPrimitive::Sweep &&
                  up.value == 4820 && up.start == 1000 && up.delta == 1 && down.start == 39000 && down.delta == -1 &&
                  up.iterations == 38000 && down.iterations == 38000,
              "Y2 the ceremony (idx 7) is NB lead + an UP sweep bx 1000 +1 and a DOWN sweep bx 39000 -1, 38000 each");
        const auto s = voice_pcm(p);
        const size_t a = speaker_segment_frames(p.segments[0]), b = a + speaker_segment_frames(up),
                     c = b + speaker_segment_frames(down);
        const size_t tenth = (b - a) / 10;
        const double up_first = positive_fraction(s, a + 64, a + tenth), up_last = positive_fraction(s, b - tenth, b - 64);
        const double dn_first = positive_fraction(s, b + 64, b + tenth), dn_last = positive_fraction(s, c - tenth, c - 64);
        std::printf("  duty proxy: up %.2f -> %.2f, down %.2f -> %.2f\n", up_first, up_last, dn_first, dn_last);
        check(up_first > up_last + 0.2 && dn_last > dn_first + 0.2,
              "Y2 sweep direction: bx rising narrows the gate (up sweep), bx falling widens it (down sweep)");
        const double p_up = pitch_hz(s, a + tenth, b - tenth), p_dn = pitch_hz(s, b + tenth, c - tenth);
        check(near(p_up, sweep_millihertz(4820) / 1000.0, 0.03) && near(p_dn, p_up, 0.03),
              "Y2 sweep endpoints: both legs keep the one pitch of inc (1898 Hz) while the duty moves");
    }
    {
        // Y3 glide: the staircase and its EFFECTIVE end (the nominal end is never written).
        const auto g = speaker_glide(1000, 200, 5, 300);
        check(g.iterations == 60 && g.delta == -13 && uint16_t(g.value + g.delta * 59) == 233,
              "Y3 glide(1000->200,5,300): 60 steps of -13 Hz, the last step at 233 Hz, not 200");
        const auto s = cue_pcm(SfxId::CannonFire);
        const double first = pitch_hz(s, 64, s.size() / 4), last = pitch_hz(s, s.size() * 3 / 4, s.size() - 64);
        const auto w = cue_pcm(SfxId::WaterfallFall);
        const double wf = pitch_hz(w, 64, w.size() / 4), wl = pitch_hz(w, w.size() * 3 / 4, w.size() - 64);
        const auto t = speaker_glide(0x320, 0x7d0, 1, 0x32);
        std::printf("  glide pitch: cannon %.0f -> %.0f Hz, waterfall %.0f -> %.0f Hz\n", first, last, wf, wl);
        check(first > last * 2 && wf > wl * 1.5 && t.delta == 24 && t.iterations == 50,
              "Y3 glides fall (cannon, waterfall) as the staircase says; torch glide climbs +24 Hz x 50");
        check(speaker_glide(1000, 200, 5, 0).iterations == 0 && speaker_glide(1000, 200, 5, -8).iterations == 0,
              "Y3 a glide with total <= 0 runs no pass (the signed jl at 0x43ef): silent");
    }
    {
        // Y4 noise: the local PRNG and the closed draw interval.
        uint16_t s = 0x1234;
        const uint16_t a = uint16_t(s + 0x9248), r = uint16_t((a >> 3) | (a << 13));
        check(speaker_noise_next(s) == uint16_t((r ^ 0x9248) + 0x11), "Y4 the PRNG is 0x2255-0x2262 verbatim");
        bool bounded = true, lo = false, hi = false;
        for (int i = 0; i < 20000; ++i) {
            const uint16_t v = speaker_noise_draw(s, 2000);
            bounded = bounded && v >= 100 && v <= 2000;
        }
        bool every[6] = {};
        for (int i = 0; i < 2000; ++i) {
            const uint16_t v = speaker_noise_draw(s, 105);
            if (v >= 100 && v <= 105) every[v - 100] = true;
            lo = lo || v == 100;
            hi = hi || v == 105;
        }
        check(bounded && lo && hi && std::all_of(every, every + 6, [](bool b) { return b; }),
              "Y4 noise draws stay in [100, band] and reach BOTH ends (sub cx,bx; inc cx: closed interval)");
        const auto burst = cue_pcm(SfxId::CombatHit);
        check(peak(burst) <= kSpeakerAmplitude && peak(burst) > kSpeakerAmplitude / 8,
              "Y4 a rendered noise burst is bounded by the square's peak and not silent");
        // [0x545c] is one word for the whole session: it is never re-seeded,
        // so two identical bursts draw different pitches.
        SfxPlayer pl;
        pl.submit(req(SfxId::CombatHit));
        pl.submit(req(SfxId::DungeonTrap));
        pl.submit(req(SfxId::CombatHit));
        std::vector<int16_t> all(2790 + 2790 + 2790);
        pl.render(all.data(), all.size(), kUnityGainQ15);
        uint16_t walk = kNoiseSeed;
        for (int i = 0; i < 300 + 75 + 300; ++i) walk = speaker_noise_next(walk);
        const std::vector<int16_t> a1(all.begin(), all.begin() + 2790), a2(all.begin() + 2790 + 2790, all.end());
        check(a1 != a2 && pl.noise_state() == walk,
              "Y4 the PRNG word persists across bursts (never re-seeded): a repeated burst differs, 675 draws later");
        const auto src = strip_comments(slurp(core_dir + "/src/sfx_synth.cpp"));
        check(src.find("g_rng") == std::string::npos && src.find("OriginalRng") == std::string::npos &&
                  src.find("rand(") == std::string::npos,
              "Y4 the noise never touches the game RNG (its PRNG word is [0x545c], the player's own)");
    }
    {
        // Y5 duration: sample counts from the one calibration.
        SpeakerProgram m, a, blk;
        compile_sfx(SfxId::ApparitionMaterialize, 0, m);
        compile_sfx(SfxId::ApparitionArpeggio, 0, a);
        compile_sfx(SfxId::MoveBlocked, 0, blk);
        check(speaker_program_frames(m) == uint32_t(uint64_t(kApparitionMaterializeSamples) * 16000 / 25806) &&
                  speaker_program_frames(a) == 6 * uint32_t(uint64_t(0x1388) * 16000 / 25806) &&
                  speaker_program_frames(blk) == 200u * 24u * 16000u / 25806u,
              "Y5 frames = samples x 16000 / 25806: materialize 6200, arpeggio 6 x 3100, move-blocked 2976");
        check(std::abs(int(speaker_program_frames(m)) - int(tone_sweep_ms(kApparitionMaterializeSamples) * 16)) <= 16 &&
                  std::abs(int(speaker_program_frames(a)) - int(tone_sweep_ms(kApparitionArpeggioSamples) * 16)) <= 16,
              "Y5 a scene cue lasts its paced hold to within one millisecond (both from kToneSweepSamplesPerSecond)");
        SpeakerProgram cer;
        compile_sfx(SfxId::TimeSpell, 3, cer);
        const uint64_t lead_us = uint64_t(0x1f40 + 0x640 * 3) * 3 * 1000000 / (2 * 25806); // start_magic_ceremony
        check(cer.segments[0].iterations == 16 && speaker_segment_frames(cer.segments[0]) == uint32_t(lead_us * 16 / 1000),
              "Y5 the ceremony lead is ceil(dur/step) = 16 draws and lasts the inverted-viewport lead exactly");
        check(voice_pcm(m).size() == speaker_program_frames(m) && voice_pcm(blk).size() == speaker_program_frames(blk),
              "Y5 the voice renders exactly the program's frames and then stops");
    }
    {
        // Y6..Y8 amplitude and gain.
        SfxPlayer p;
        p.submit(req(SfxId::ApparitionChord));
        const auto full = player_pcm(p, 20000, kUnityGainQ15);
        int32_t m = 0;
        for (auto v : full) m = std::max(m, std::abs(int32_t(v)));
        check(m > 0 && m <= 2 * kSpeakerAmplitude, "Y6 the loudest primitive stays within 2x the -12 dBFS peak: headroom");
        int32_t peaks[5]{};
        const uint8_t vols[5] = {0, 10, 50, 80, 100};
        for (int i = 0; i < 5; ++i) {
            SfxPlayer q;
            q.submit(req(SfxId::ApparitionChord));
            for (auto v : player_pcm(q, 20000, volume_to_gain_q15(vols[i]))) peaks[i] = std::max(peaks[i], std::abs(int32_t(v)));
        }
        std::printf("  chord peak at 0/10/50/80/100 %%: %d %d %d %d %d\n", peaks[0], peaks[1], peaks[2], peaks[3], peaks[4]);
        bool zero = true;
        {
            SfxPlayer q;
            q.submit(req(SfxId::CombatHit));
            for (auto v : player_pcm(q, 4000, volume_to_gain_q15(0))) zero = zero && v == 0;
        }
        check(zero && peaks[0] == 0, "Y7 0 % is exact digital silence (every sample 0)");
        check(peaks[0] < peaks[1] && peaks[1] < peaks[2] && peaks[2] < peaks[3] && peaks[3] < peaks[4],
              "Y8/I gain is strictly monotonic 0 < 10 < 50 < 80 < 100 %");
        check(std::abs(peaks[2] - int32_t(int64_t(peaks[4]) * 8191 / 32767)) <= 2 &&
                  std::abs(peaks[4] - m) <= 1,
              "Y8 the gain is applied ONCE: 50 % peak = 100 % peak x 8191/32767, 100 % = the unscaled peak");
        SfxPlayer over;
        over.submit(req(SfxId::ApparitionChord));
        const auto clamped = player_pcm(over, 20000, 65535);
        check(clamped == full && volume_to_gain_q15(250) == kUnityGainQ15,
              "Y8 a gain above unity (65535, or a 250 % volume) clamps to unity: no overflow");
        // A live change: the next chunk is at the new gain, sample for sample.
        SfxPlayer x, y;
        x.submit(req(SfxId::ApparitionChord));
        y.submit(req(SfxId::ApparitionChord));
        player_pcm(x, 4096, volume_to_gain_q15(80));
        player_pcm(y, 4096, volume_to_gain_q15(20));
        check(player_pcm(x, 4096, volume_to_gain_q15(20)) == player_pcm(y, 4096, volume_to_gain_q15(20)),
              "Y8/I changing SFX Volume mid-tone: the following chunk is exactly the new gain, phase unbroken");
    }
    {
        // Y9 cancellation.
        SfxPlayer p;
        p.submit(req(SfxId::ApparitionChord));
        p.submit(req(SfxId::MoveBlocked));
        player_pcm(p, 1000, kUnityGainQ15);
        p.flush();
        const auto tail = player_pcm(p, 4000, kUnityGainQ15);
        size_t last = 0;
        for (size_t i = 0; i < tail.size(); ++i)
            if (tail[i]) last = i + 1;
        check(last <= SfxPlayer::kReleaseFrames && p.idle() && p.pending() == 0,
              "Y9 a flush fades the playing cue out within 2 ms, drops the queue, and leaves exact silence");
        p.submit(req(SfxId::MoveBlocked));
        check(p.playing() == SfxId::MoveBlocked, "Y9 the player takes new cues after a flush");
    }
    {
        // Y10 overflow: 20 requests in one frame.
        SfxPlayer p;
        SfxAdmit first = p.submit(req(SfxId::ApparitionChord));
        const SfxId ids[] = {SfxId::MoveBlocked, SfxId::CombatHit, SfxId::TorchBorrowed, SfxId::DungeonZap, SfxId::FieldAfflict};
        for (int i = 0; i < 19; ++i) p.submit(req(ids[i % 5], i));
        check(first == SfxAdmit::Started && p.playing() == SfxId::ApparitionChord && p.pending() == SfxPlayer::kPendingDepth &&
                  p.stats().overflowed == 11 && p.pending_at(0).param == 11 && p.pending_at(7).param == 18,
              "Y10 20 rapid distinct requests: the first plays, the NEWEST 8 wait, the 11 oldest pending are dropped");
        SfxPlayer q;
        for (int i = 0; i < 20; ++i) q.submit(req(SfxId::MoveBlocked));
        check(q.pending() == 1 && q.stats().coalesced == 18, "Y10 20 identical requests: one plays, one waits, 18 coalesce");
        SfxPlayer e;
        e.sync_epoch(1);
        check(e.submit(req(SfxId::MoveBlocked), 0) == SfxAdmit::Stale && e.idle() &&
                  e.submit(req(SfxId::MoveBlocked), 1) == SfxAdmit::Started,
              "Y10 transport: a request posted before the latest flush epoch is dropped as stale");
    }

    // ======================================================================
    // EVENT MAPPING
    // ======================================================================
    {
        SpeakerProgram p;
        bool shape = true;
        compile_sfx(SfxId::MoveBlocked, 0, p);
        shape = shape && p.count == 1 && p.segments[0].kind == SpeakerPrimitive::Tone && p.segments[0].value == 0xa5;
        compile_sfx(SfxId::MoveStep, 0, p);
        shape = shape && p.count == 3 && p.segments[0].kind == SpeakerPrimitive::Noise && p.segments[0].value == 1000 &&
                p.segments[1].kind == SpeakerPrimitive::Silence && p.segments[2].value == 1500;
        compile_sfx(SfxId::CombatHit, 0, p);
        shape = shape && p.segments[0].kind == SpeakerPrimitive::Noise && p.segments[0].value == 2000 &&
                p.segments[0].iterations == 300;
        compile_sfx(SfxId::CombatHitHeavy, 0, p);
        shape = shape && p.segments[0].value == 500 && p.segments[0].iterations == 75;
        compile_sfx(SfxId::TorchBorrowed, 0, p);
        shape = shape && p.segments[0].kind == SpeakerPrimitive::Glide;
        compile_sfx(SfxId::BlackthornMaterialize, 0, p);
        shape = shape && p.segments[0].kind == SpeakerPrimitive::Sweep && p.segments[0].value == 0xaf0 &&
                p.segments[0].iterations == kBlackthornMaterializeSamples;
        compile_sfx(SfxId::ApparitionChord, 0, p);
        shape = shape && p.segments[0].iterations == kApparitionChordSamples && p.segments[0].value == 0x157c;
        compile_sfx(SfxId::MirrorBreak, 0, p);
        shape = shape && p.count == 18 && p.segments[17].value == 19000;
        check(shape, "E11 each cue compiles to the original's primitive sequence (beep / NB+gap+NB / NB / glide / sweeps)");
        bool ceremony = true;
        for (int i = 0; i < 9; ++i) {
            compile_sfx(SfxId::TimeSpell, i, p);
            ceremony = ceremony && p.count == 3 && p.segments[0].value == 0x2bc &&
                       p.segments[0].iterations == uint32_t((0x1f40 + 0x640 * i) / 0x320) && p.segments[1].value == kInc[i] &&
                       p.segments[1].start == kUp[i] && p.segments[1].delta == kStep[i] && p.segments[2].start == kDown[i] &&
                       p.segments[2].delta == -int(kStep[i]) && p.segments[1].iterations == uint32_t(0x2710 + 0xfa0 * i);
        }
        check(ceremony, "E11 the ceremony CAST2 0x0000(i) uses the DATA.OVL tables 0x4af6/0x4b08/0x4b1a/0x4b2c for all 9 i");
        check(sfx_for_combat_attack(false, 1, false) == SfxId::CombatHit &&
                  sfx_for_combat_attack(false, 1, true) == SfxId::CombatHitHeavy &&
                  sfx_for_combat_attack(true, -1, false) == SfxId::None && // A3-05: a death adds no burst
                  sfx_for_combat_attack(false, 0, false) == SfxId::None && sfx_for_combat_attack(false, -1, true) == SfxId::None,
              "E11 combat: a hit on an enemy / on the party / a death / a miss -> hit / heavy / silence (A3-05) / silence");
    }
    {
        SpeakerProgram p;
        bool consistent = true;
        size_t supported = 0;
        for (size_t i = 0; i <= kSfxIdCount; ++i) {
            const bool c = compile_sfx(SfxId(i), 0, p);
            consistent = consistent && c == sfx_supported(SfxId(i));
            supported += c;
        }
        SfxPlayer pl;
        // A3-03 widened the table (23 -> 62) and wired moongate; bard-song is
        // still declined (sfx_inventory.h: DeferredPresentation). A4-END1 wired
        // the ending's two sweeps (endgame-orb, params 0 and 1): 63.
        std::printf("  supported cues: %zu\n", supported);
        check(consistent && supported == 63 && !compile_sfx(SfxId::None, 0, p) && !compile_sfx(SfxId::Count, 0, p) &&
                  pl.submit(req(SfxId::BardSong)) == SfxAdmit::Unsupported && pl.submit(req(SfxId(250))) == SfxAdmit::Unsupported &&
                  pl.idle(),
              "E12 unknown / undeclared ids (None, Count, 250, bard-song) compile to nothing and leave the player idle");
        check(compile_sfx(SfxId::SpellCast, 0, p) == false && compile_sfx(SfxId::InvalidMagic, 0, p) == false,
              "E12 the spell-cast hook is a marker (its sound is the ceremony event's); invalid-magic has no adjudicated sound");
    }
    {
        SfxPlayer p;
        p.submit(req(SfxId::CannonFire));
        const auto a = p.submit(req(SfxId::MoveBlocked)), b = p.submit(req(SfxId::MoveBlocked));
        const auto c = p.submit(req(SfxId::CombatHit)), d = p.submit(req(SfxId::MoveBlocked));
        check(a == SfxAdmit::Queued && b == SfxAdmit::Coalesced && c == SfxAdmit::Queued && d == SfxAdmit::Queued &&
                  p.pending() == 3,
              "E13 a repeat of the NEWEST pending cue coalesces; the same cue after a different one queues again");
    }
    {
        SfxPlayer p;
        p.submit(req(SfxId::CannonFire));
        p.submit(req(SfxId::MoveBlocked));
        p.submit(req(SfxId::CombatHit));
        const auto s = p.submit(req(SfxId::ApparitionMaterialize));
        check(s == SfxAdmit::Preempted && p.pending() == 1 && p.pending_at(0).id == SfxId::ApparitionMaterialize,
              "E14 a scene cue preempts the playing ordinary cue and discards the lower-class cues queued behind it");
        const auto tail = player_pcm(p, SfxPlayer::kReleaseFrames + 1, kUnityGainQ15);
        check(p.playing() == SfxId::ApparitionMaterialize, "E14 it starts the moment the 2 ms fade of the cut cue ends");
        const auto s2 = p.submit(req(SfxId::ApparitionArpeggio));
        player_pcm(p, SfxPlayer::kReleaseFrames + 1, kUnityGainQ15);
        check(s2 == SfxAdmit::Preempted && p.playing() == SfxId::ApparitionArpeggio,
              "E14 a newer scene cue replaces the playing scene cue (the pacer is the clock: latest wins)");
        const auto o = p.submit(req(SfxId::MoveBlocked));
        check(o == SfxAdmit::Queued && p.playing() == SfxId::ApparitionArpeggio,
              "E14 an ordinary cue never interrupts a scene cue: it waits");
        p.submit(req(SfxId::DiagnosticTone));
        player_pcm(p, SfxPlayer::kReleaseFrames + 1, kUnityGainQ15);
        const auto sc = p.submit(req(SfxId::ApparitionChord));
        check(p.playing() == SfxId::DiagnosticTone && sc == SfxAdmit::Queued && p.pending() == 1,
              "E14 the diagnostic tone preempts everything and flushes the queue; a scene cue waits behind it");
        (void)tail;
    }

    // ======================================================================
    // HARPSICHORD -- TOWN 0x0e34: tone_sweep(note[digit], 1, 4000, 20000, -4).
    // ======================================================================
    {
        SpeakerProgram p;
        bool map = true;
        for (int d = 0; d < 10; ++d) {
            compile_sfx(SfxId::InstrumentNote, d, p);
            map = map && p.count == 1 && p.segments[0].value == kNotes[d] && p.segments[0].iterations == 4000 &&
                  p.segments[0].start == 20000 && p.segments[0].delta == -4;
        }
        SpeakerProgram lo, hi;
        compile_sfx(SfxId::InstrumentNote, -3, lo);
        compile_sfx(SfxId::InstrumentNote, 42, hi);
        check(map && lo.segments[0].value == kNotes[0] && hi.segments[0].value == kNotes[9],
              "H15 digit d plays note[d] of DATA.OVL 0x2746 (4000 samples, bx 20000 -4); out-of-range digits clamp");
        double hz[10];
        for (int d = 0; d < 10; ++d) {
            const auto s = cue_pcm(SfxId::InstrumentNote, d);
            hz[d] = pitch_hz(s, 300, s.size() - 300);
        }
        bool order = hz[0] > hz[9], exact = true;
        for (int d = 1; d < 9; ++d) order = order && hz[d] < hz[d + 1];
        for (int d = 0; d < 10; ++d) exact = exact && near(hz[d], sweep_millihertz(kNotes[d]) / 1000.0, 0.03);
        std::printf("  harpsichord Hz 1..9,0: %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f\n", hz[1], hz[2], hz[3],
                    hz[4], hz[5], hz[6], hz[7], hz[8], hz[9], hz[0]);
        check(order, "H16 digits 1..9 rise in pitch and 0 is the highest key (the 10th, above the octave)");
        check(exact && near(hz[8] / hz[1], double(kNotes[8]) / kNotes[1], 0.03),
              "H16 every note sounds at its table pitch; the intervals are the table's ratios");
        compile_sfx(SfxId::InstrumentNote, 5, p);
        check(speaker_program_frames(p) == 2480, "H16 a note lasts 4000 samples = 155 ms (2480 frames)");
    }
    {
        SfxPlayer p;
        const int keys[] = {6, 7, 8, 9, 8, 7, 8, 7, 6};
        for (int d : keys) p.submit(req(SfxId::InstrumentNote, d));
        const bool fifo = p.playing() == SfxId::InstrumentNote && p.pending() == 8 && p.pending_at(0).param == 7 &&
                          p.pending_at(7).param == 6 && p.stats().coalesced == 0;
        p.submit(req(SfxId::InstrumentNote, 7));
        check(fifo && p.stats().coalesced == 0 && p.stats().overflowed == 1,
              "H18 rapid notes queue in key order and are never coalesced (a repeated key is a note); overflow drops the oldest");
        // A repeated key is a repeated note, even while it is still waiting.
        SfxPlayer rep;
        const SfxAdmit r1 = rep.submit(req(SfxId::InstrumentNote, 3)), r2 = rep.submit(req(SfxId::InstrumentNote, 3)),
                       r3 = rep.submit(req(SfxId::InstrumentNote, 3));
        check(r1 == SfxAdmit::Started && r2 == SfxAdmit::Queued && r3 == SfxAdmit::Queued && rep.pending() == 2,
              "H18 the same digit pressed three times fast is three notes (queued, never coalesced)");
        SfxPlayer q;
        for (int d : {1, 2, 3}) q.submit(req(SfxId::InstrumentNote, d));
        size_t sounding = 0;
        std::vector<int16_t> buf(256);
        for (int i = 0; i < 64 && !q.idle(); ++i) sounding += q.render(buf.data(), buf.size(), kUnityGainQ15);
        check(sounding == 3 * 2480 && q.idle(), "H18 three queued notes each play in full, back to back, then silence");
        SfxPlayer r;
        r.submit(req(SfxId::InstrumentNote, 9));
        r.submit(req(SfxId::InstrumentNote, 0));
        player_pcm(r, 500, kUnityGainQ15);
        r.flush();
        const auto after = player_pcm(r, 3000, kUnityGainQ15);
        size_t last = 0;
        for (size_t i = 0; i < after.size(); ++i)
            if (after[i]) last = i + 1;
        check(last <= SfxPlayer::kReleaseFrames && r.idle(), "H17 cancelling a note mid-way leaves no stuck tone");
    }

    // ======================================================================
    // The TypeScript reference, row by row (native/core/tools/generate-sfx-fixtures.ts).
    // ======================================================================
    {
        std::ifstream in(argv[1]);
        std::vector<RefRow> rows;
        for (RefRow r; in >> r.cue >> r.n >> r.seg >> r.kind >> r.f0 >> r.f1 >> r.ms >> r.steps;) rows.push_back(r);
        size_t compared = 0, bad = 0;
        std::map<std::string, int> segs;
        for (const auto &r : rows) segs[r.cue + "/" + std::to_string(r.n)]++;
        for (const auto &r : rows) {
            const SfxId id = sfx_from_cue(r.cue.c_str());
            SpeakerProgram p;
            if (!compile_sfx(id, r.n, p) || p.count != segs[r.cue + "/" + std::to_string(r.n)] || r.seg >= p.count) {
                ++bad;
                std::printf("  shape mismatch %s %d\n", r.cue.c_str(), r.n);
                continue;
            }
            const auto &s = p.segments[r.seg];
            const double ms = speaker_segment_frames(s) * 1000.0 / kSfxOutputRateHz;
            bool ok = std::fabs(ms - r.ms) <= 0.1; // 17 ppm calibration + one frame
            if (r.kind == "tone") {
                const double hz = s.kind == SpeakerPrimitive::Sweep ? sweep_millihertz(s.value) / 1000.0
                                                                     : pit_quantized_millihertz(s.value) / 1000.0;
                ok = ok && (s.kind == SpeakerPrimitive::Sweep || s.kind == SpeakerPrimitive::Tone) &&
                     near(hz, r.f0, s.kind == SpeakerPrimitive::Sweep ? 0.0005 : 0.003);
            } else if (r.kind == "glide") {
                const double end = pit_quantized_millihertz(uint16_t(s.value + s.delta * int(s.iterations - 1))) / 1000.0;
                ok = ok && s.kind == SpeakerPrimitive::Glide && int(s.iterations) == r.steps &&
                     near(pit_quantized_millihertz(s.value) / 1000.0, r.f0, 0.003) && near(end, r.f1, 0.003);
            } else if (r.kind == "noise") {
                ok = ok && s.kind == SpeakerPrimitive::Noise && int(s.iterations) == r.steps;
            } else {
                ok = ok && s.kind == SpeakerPrimitive::Silence;
            }
            ++compared;
            if (!ok) {
                ++bad;
                std::printf("  reference mismatch %s %d seg %d: ms %.4f vs %.4f\n", r.cue.c_str(), r.n, r.seg, ms, r.ms);
            }
        }
        std::printf("  reference rows compared: %zu\n", compared);
        check(rows.size() == 81 && compared == rows.size() && bad == 0,
              "R1 all 81 reference segments agree: kind, pitch (sweep exact, PIT tones within the divisor's 0.3 %), "
              "length within 0.1 ms, glide staircase and noise draw counts");
    }

    // ======================================================================
    // Non-blocking by construction.
    // ======================================================================
    {
        const std::string src = strip_comments(slurp(core_dir + "/src/sfx_synth.cpp"));
        bool clean = !src.empty();
        for (const char *t : {"vTaskDelay", "sleep", "xQueue", "Semaphore", "esp_", "new ", "malloc", "std::vector", "mutex"})
            clean = clean && src.find(t) == std::string::npos;
        check(clean, "N1 sfx_synth.cpp holds no RTOS call, sleep, lock or heap allocation");
    }

    std::printf("A3-02 sfx synth: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
