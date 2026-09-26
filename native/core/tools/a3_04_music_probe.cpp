// Ad-hoc dev probe (not a ctest target): decodes the real local audio pack
// (native/assets/openu5-audio.bin, git-ignored, built from the user's own
// files) end to end and prints measurements, the same way a3_02_cue_sites.py
// measured the PC-speaker corpus before the SFX synth was written.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"

using namespace openu5;

namespace {
std::vector<uint8_t> read_file(const char *path) {
    std::FILE *f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return buf;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a3_04_music_probe <openu5-audio.bin>\n");
        return 2;
    }
    auto bytes = read_file(argv[1]);
    if (bytes.empty()) {
        std::fprintf(stderr, "could not read %s\n", argv[1]);
        return 2;
    }
    AudioPackPayload payload{};
    AudioPackInfo info = inspect_audio_pack(bytes.data(), bytes.size(), &payload);
    std::printf("pack state=%s capability=%s\n", audio_pack_state_name(info.state),
                info.state == AudioPackState::Valid ? music_capability_name(info.record.capability) : "-");
    if (info.state != AudioPackState::Valid || info.record.capability != MusicCapability::SupportedMusicPatch) {
        std::printf("no playable music payload\n");
        return 0;
    }

    MilesOplBank bank;
    const bool bank_ok = bank.load(payload.bank, payload.bank_length);
    std::printf("bank loaded=%d timbres=%zu\n", bank_ok, bank.count());

    for (int s = 0; s < 16; ++s) {
        MusicTrack track;
        const bool parsed = parse_xmi_events(payload.song[s], payload.song_length[s], track);
        if (!parsed) {
            std::printf("song %2d: PARSE FAILED\n", s);
            continue;
        }
        const double seconds = double(track.end_tick) / double(kXmiTicksPerSecond);
        std::printf("song %2d (%s): events=%zu end_tick=%u (%.1fs)\n", s,
                    music_song_title(MusicSong(s)), track.events.size(), track.end_tick, seconds);

        MusicSongPlayer player;
        player.start(track, bank, OplChipKind::Opl2, /*loop=*/false);
        constexpr uint32_t kRate = 16000;
        constexpr size_t kChunk = 256;
        std::vector<int16_t> chunk(kChunk);
        double peak = 0;
        double sum_sq = 0;
        size_t total = 0;
        size_t rendered_frames = size_t(seconds * kRate) + kRate * 4; // song + 4s tail margin
        for (size_t f = 0; f < rendered_frames && !player.ended(); f += kChunk) {
            player.render(chunk.data(), kChunk, kRate, kUnityGainQ15);
            for (auto v : chunk) {
                const double x = double(v) / 32768.0;
                if (std::fabs(x) > peak) peak = std::fabs(x);
                sum_sq += x * x;
            }
            total += kChunk;
        }
        const double rms = total ? std::sqrt(sum_sq / double(total)) : 0.0;
        std::printf("           rendered_frames=%zu peak=%.4f rms=%.4f ended=%d\n", total, peak, rms,
                    player.ended());
    }
    return 0;
}
