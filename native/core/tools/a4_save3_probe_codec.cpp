// A4-SAVE3 investigation probe: what does Native's codec keep of a real DOS save?
#include "openu5/persistence.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using namespace openu5;
static std::vector<uint8_t> rd(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static void ranges(const char *label, const uint8_t *a, const uint8_t *b, size_t n) {
    size_t total = 0;
    std::printf("%s\n", label);
    for (size_t i = 0; i < n;) {
        if (a[i] == b[i]) { ++i; continue; }
        size_t j = i;
        while (j < n && a[j] != b[j]) ++j;
        // merge gaps of < 4 equal bytes
        size_t k = j;
        while (k < n) {
            size_t e = k;
            while (e < n && a[e] == b[e] && e - k < 4) ++e;
            if (e < n && e - k < 4 && a[e] != b[e]) { k = e; while (k < n && a[k] != b[k]) ++k; j = k; }
            else break;
        }
        size_t diff = 0;
        for (size_t x = i; x < j; ++x) diff += a[x] != b[x];
        total += diff;
        std::printf("  0x%03zx-0x%03zx (%zu B, %zu differ)", i, j - 1, j - i, diff);
        if (j - i <= 8) { std::printf("  want"); for (size_t x = i; x < j; ++x) std::printf(" %02x", a[x]); std::printf("  got"); for (size_t x = i; x < j; ++x) std::printf(" %02x", b[x]); }
        std::printf("\n");
        i = j;
    }
    std::printf("  total differing bytes: %zu of %zu\n", total, n);
}
int main(int argc, char **argv) {
    const std::string root = argc > 1 ? argv[1] : "C:/Dev/OpenU5-Native/original/u5/ultima5/";
    auto saved = rd((root + "SAVED.GAM").c_str()), init = rd((root + "INIT.GAM").c_str());
    auto sool = rd((root + "SAVED.OOL").c_str()), iool = rd((root + "INIT.OOL").c_str());
    std::printf("SAVED.GAM %zu  INIT.GAM %zu  SAVED.OOL %zu  INIT.OOL %zu\n", saved.size(), init.size(), sool.size(), iool.size());
    std::printf("SAVED: location=%u floor=%u x=%u y=%u party=%u gold=%u turns=%u\n", saved[0x2ed], saved[0x2ef], saved[0x2f0], saved[0x2f1], saved[0x2b5], saved[0x204] | saved[0x205] << 8, saved[0x2e5]);
    GameState g{};
    TurnState t{};
    save::Json doc;
    save::SidecarSource src;
    auto e = save::load_native_state(saved.data(), saved.size(), nullptr, g, t, doc, src, true);
    std::printf("load_native_state(no sidecar, gate) = %d source=%d\n", int(e), int(src));
    if (e != save::Error::None) return 1;
    save::Gam out{};
    save::Json side;
    e = save::export_native_state(g, t, doc, init.data(), init.size(), out, side, true);
    std::printf("export_native_state(base=INIT.GAM) = %d\n", int(e));
    ranges("SAVED.GAM vs export(base INIT.GAM):", saved.data(), out.data(), save::kGamSize);
    e = save::export_native_state(g, t, doc, saved.data(), saved.size(), out, side, true);
    ranges("SAVED.GAM vs export(base SAVED.GAM):", saved.data(), out.data(), save::kGamSize);
    auto ool = save::build_ool(doc, iool.data(), iool.size());
    ranges("SAVED.OOL vs build_ool(base INIT.OOL):", sool.data(), ool.data(), 512);
    ool = save::build_ool(doc, sool.data(), sool.size());
    ranges("SAVED.OOL vs build_ool(base SAVED.OOL):", sool.data(), ool.data(), 512);
    std::string text;
    save::encode_json(side, text);
    std::printf("sidecar generated (%zu B): %.600s\n", text.size(), text.c_str());
    std::printf("doc keys:");
    for (auto &k : doc.keys) std::printf(" %s", std::string(k.begin(), k.end()).c_str());
    std::printf("\n");
}
