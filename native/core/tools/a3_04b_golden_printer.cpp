// Alpha 3 A3-04B ad-hoc tool (not a ctest target): prints the synth
// fingerprints of tests/a3_04b_synth_goldens.h from whatever openu5_core it
// is linked with. Linked against an A3-04A build it produced the pinned
// values (native/core/a3-04b-goldens-from-a3-04a.log):
//   g++ -std=c++17 -O2 -I include -I tests tools/a3_04b_golden_printer.cpp <build>/libopenu5_core.a
//       -o golden_printer && golden_printer ../assets/openu5-audio.bin
// (11,025 Hz is deliberately absent: A3-04's resampler under-fills at output
// ratios above ~3 -- ALPHA3_AUDIO.md section 19, finding F-3 -- and the
// device only ever asks for 16 kHz.)
#include <cstdio>
#include <vector>
#include "a3_04b_synth_goldens.h"

static std::vector<uint8_t> read_file(const char *path) {
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

int main(int, char **argv) {
    using namespace openu5;
    auto bytes = read_file(argv[1]);
    AudioPackPayload payload{};
    inspect_audio_pack(bytes.data(), bytes.size(), &payload);
    MilesOplBank bank;
    bank.load(payload.bank, payload.bank_length);
    std::printf("kCorpusL6        = 0x%016llxull\n", (unsigned long long)a3_04b::corpus_l6(payload, bank));
    std::printf("kCorpusDevice128 = 0x%016llxull\n",
                (unsigned long long)a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 128, 16000, 30.0));
    std::printf("kCorpusOpl3      = 0x%016llxull\n",
                (unsigned long long)a3_04b::corpus_stream(payload, bank, OplChipKind::Opl3, 128, 16000, 10.0));
    const uint64_t a = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 44100, 4.0);
    const uint64_t b = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 22050, 4.0);
    const uint64_t c = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 96, 16000, 4.0);
    std::printf("kCorpusRates     = 0x%016llxull\n", (unsigned long long)(a ^ (b * 3) ^ (c * 7)));
    std::printf("kChipStressOpl2  = 0x%016llxull\n", (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl2, 0x5eed04b, 6000));
    std::printf("kChipStressOpl3  = 0x%016llxull\n", (unsigned long long)a3_04b::chip_stress(OplChipKind::Opl3, 0xa3a3b0b, 6000));
    return 0;
}
