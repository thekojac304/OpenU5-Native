// Alpha 4 UI Batch 1 (ALPHA4_UI.md) -- the "More Ultima V" chrome.
//
//   P  the resource pack: IBM.CH travels as "ibm.ch" (1,024 B, 128 glyphs x 8),
//      byte for byte the game's file, loaded into the owners, required by name,
//      and the firmware's identity lock names this pack
//
//   a4_ui1_chrome_runtime <openu5-alpha1-resources.bin> <original IBM.CH>
#include "../main/alpha_resources.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
std::string n(uint64_t v) { return std::to_string(v); }

std::vector<uint8_t> slurp(const char *path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

struct Toc { std::string name; uint32_t offset = 0, length = 0, records = 0, stride = 0; };
std::vector<Toc> toc(const std::vector<uint8_t> &b) {
    std::vector<Toc> out;
    if (b.size() < 32) return out;
    const uint32_t count = le32(b.data() + 16);
    for (uint32_t i = 0; i < count && 32 + size_t(i + 1) * 64 <= b.size(); ++i) {
        const uint8_t *t = b.data() + 32 + size_t(i) * 64;
        Toc e;
        e.name.assign(reinterpret_cast<const char *>(t), strnlen(reinterpret_cast<const char *>(t), 32));
        e.offset = le32(t + 32); e.length = le32(t + 36); e.records = le32(t + 44); e.stride = le32(t + 48);
        out.push_back(e);
    }
    return out;
}
// batch53_release_blockers' seam: rename one TOC entry and re-seal the TOC CRC.
bool rename_entry(std::vector<uint8_t> &bytes, const char *from, const char *to) {
    const uint32_t count = le32(bytes.data() + 16);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t *t = bytes.data() + 32 + size_t(i) * 64;
        if (std::strncmp(reinterpret_cast<const char *>(t), from, 32) != 0) continue;
        std::memset(t, 0, 32);
        std::memcpy(t, to, std::strlen(to));
        uint32_t crc = 0xffffffffU;
        for (size_t k = 32; k < 32 + size_t(count) * 64; ++k) {
            crc ^= bytes[k];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
        crc ^= 0xffffffffU;
        for (int k = 0; k < 4; ++k) bytes[28 + k] = uint8_t(crc >> (8 * k));
        return true;
    }
    return false;
}

void test_pack(const char *pack_path, const char *ibm_path) {
    std::printf("P  the resource pack carries IBM.CH\n");
    const auto bytes = slurp(pack_path);
    const auto original = slurp(ibm_path);
    const auto entries = toc(bytes);
    const Toc *ibm = nullptr;
    for (const auto &e : entries)
        if (e.name == "ibm.ch") ibm = &e;
    const bool shaped = ibm && ibm->length == 1024 && ibm->records == 128 && ibm->stride == 8 &&
                        size_t(ibm->offset) + ibm->length <= bytes.size();
    check(shaped, "P1 the pack has an \"ibm.ch\" entry of 1,024 B (128 records x 8) among its " + n(entries.size()) +
                      " entries");
    check(shaped && original.size() == 1024 &&
              std::equal(original.begin(), original.end(), bytes.begin() + std::ptrdiff_t(ibm->offset)),
          "P2 its bytes are the game's IBM.CH, byte for byte (" + n(original.size()) + " B on disk)");

    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    const auto opened = source.open(pack_path, report);
    static tdeck::AlphaResourceOwners owners{};
    const auto loaded = opened == ESP_OK ? source.load(owners, report) : opened;
    check(loaded == ESP_OK && owners.ibm_font && original.size() == 1024 &&
              std::memcmp(owners.ibm_font, original.data(), 1024) == 0,
          "P3 load() puts the 1,024 font bytes in the owners (ibm_font), unchanged");
    check(report.firmware_match && report.file_size == tdeck::kExpectedAlphaResourceSize &&
              report.payload_crc32 == tdeck::kExpectedAlphaResourceCrc32,
          "P4 the firmware's identity lock names THIS pack (" + n(report.file_size) + " B)");
    owners.release();
    source.close();

    auto stale = bytes;
    int err = -1;
    if (rename_entry(stale, "ibm.ch", "a4-ui1-renamed.ch")) {
        const std::string path = std::string(pack_path) + ".a4-ui1-stale.tmp";
        {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char *>(stale.data()), std::streamsize(stale.size()));
        }
        tdeck::AlphaResourcePack probe;
        tdeck::AlphaResourceReport r{};
        err = probe.open(path.c_str(), r);
        probe.close();
        std::remove(path.c_str());
    }
    check(err == ESP_ERR_NOT_FOUND, "P5 a pack without \"ibm.ch\" is refused by NAME at open() (ESP_ERR_NOT_FOUND, got " +
                                        std::to_string(err) + ")");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui1_chrome_runtime <openu5-alpha1-resources.bin> <IBM.CH>\n");
        return 2;
    }
    test_pack(argv[1], argv[2]);
    std::printf("A4-UI1 chrome runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
